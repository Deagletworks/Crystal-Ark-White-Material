/**
 * @file c_rome_mhd_flux_core.c
 * @brief C_ROME-OS Core Kernel Module with Ultra-Low-Latency DMA & IRQ Sync
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#define MODULE_NAME "c_rome_mhd_core"
#define C_ROME_MMIO_BASE    0x3F000000
#define MMIO_REGION_SIZE    0x00000100
#define IRQ_LINE_TDM        42
#define TDM_BUFFER_SIZE     4096  /* 1本の光の筋に乗せる時分割ビット列用バッファ */

/* レジスタマップ・オフセット構造体定義 */
struct mhd_reg_map {
    u32 flux_x;       /* 0x00 */
    u32 flux_y;       /* 0x04 */
    u32 flux_z;       /* 0x08 */
    u32 sync_trigger; /* 0x0C */
    u32 dma_addr_l;   /* 0x10 */
    u32 dma_addr_h;   /* 0x14 */
    u32 dma_ctrl;     /* 0x18 */
    u32 int_status;   /* 0x1C */
};

MODULE_LICENSE("GPL");
MODULE_AUTHOR("C_ROME-OS Core Lab");
MODULE_DESCRIPTION("Layer 3 Coherent Interface / TDM-DMA & Hardware Interrupt Engine");

static void __iomem *mmio_base;
static void *dma_virt_buffer;
static dma_addr_t dma_phys_handle;

/**
 * @brief 超高速TDMパルス同期 割り込みハンドラ (ISR)
 */
static irqreturn_t c_rome_tdm_irq_handler(int irq, void *dev_id)
{
    u32 status;
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;

    /* 割り込み因子の確認と即時クリア */
    status = ioread32(&regs->int_status);
    if (!(status & 0x00000001)) {
        return IRQ_NONE; /* このデバイスの割り込みではない */
    }

    /* 割り込みクリアの書き込み (W1C: Write 1 to Clear) */
    iowrite32(0x00000001, &regs->int_status);

    /* 次の時間領域多重(TDM)スロットパルスのためのダブルバッファフリップ（Ping-Pong） */
    u32 dma_ctrl_val = ioread32(&regs->dma_ctrl);
    dma_ctrl_val ^= 0x00000002; /* Bit[1] を反転させて裏ページをアクティブ化 */
    iowrite32(dma_ctrl_val, &regs->dma_ctrl);

    return IRQ_HANDLED;
}

/* 初期化ルーチン */
static int __init c_rome_core_init(void)
{
    int ret;
    struct mhd_reg_map __iomem *regs;

    pr_info("%s: Spawning Layer 3 Engine...\n", MODULE_NAME);

    /* 1. MMIOの登録 */
    mmio_base = ioremap(C_ROME_MMIO_BASE, MMIO_REGION_SIZE);
    if (!mmio_base) {
        return -ENOMEM;
    }
    regs = (struct mhd_reg_map __iomem *)mmio_base;

    /* 2. TDM同期用 一貫性（コヒーレント）DMAバッファのアロケーション */
    dma_virt_buffer = dma_alloc_coherent(NULL, TDM_BUFFER_SIZE, &dma_phys_handle, GFP_KERNEL);
    if (!dma_virt_buffer) {
        iounmap(mmio_base);
        return -ENOMEM;
    }

    /* ハードウェアへDMAバッファの物理アドレス（上下64bit）を設定 */
    iowrite32((u32)(dma_phys_handle & 0xFFFFFFFF), &regs->dma_addr_l);
    iowrite32((u32)((dma_phys_handle >> 32) & 0xFFFFFFFF), &regs->dma_addr_h);

    /* 3. 超高速タイミング割り込みの登録 (非共有・最優先ライン) */
    ret = request_irq(IRQ_LINE_TDM, c_rome_tdm_irq_handler, IRQF_TRIGGER_RISING, MODULE_NAME, NULL);
    if (ret) {
        pr_err("%s: IRQ registration failed on line %d\n", MODULE_NAME, IRQ_LINE_TDM);
        dma_free_coherent(NULL, TDM_BUFFER_SIZE, dma_virt_buffer, dma_phys_handle);
        iounmap(mmio_base);
        return ret;
    }

    /* 4. DMAおよびTDM制御エンジンの有効化 */
    iowrite32(0x00000001, &regs->dma_ctrl); // Bit[0] = 1 (Enable)

    pr_info("%s: Full Coherent Control Stack Active. Ready for TDM-MHD Resonance.\n", MODULE_NAME);
    return 0;
}

/* クリーンアップ */
static void __exit c_rome_core_exit(void)
{
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;
    
    /* ハードウェア停止 */
    if (mmio_base) {
        iowrite32(0x00000000, &regs->dma_ctrl);
    }

    free_irq(IRQ_LINE_TDM, NULL);
    if (dma_virt_buffer) {
        dma_free_coherent(NULL, TDM_BUFFER_SIZE, dma_virt_buffer, dma_phys_handle);
    }
    if (mmio_base) {
        iounmap(mmio_base);
    }
    pr_info("%s: Engine shutdown complete.\n", MODULE_NAME);
}

module_init(c_rome_core_init);
module_exit(c_rome_core_exit);
