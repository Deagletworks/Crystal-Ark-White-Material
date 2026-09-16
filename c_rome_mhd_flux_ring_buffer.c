/**
 * @file c_rome_mhd_flux_ring_buffer.c
 * @brief C_ROME-OS Core Module with 8-Page DMA Ring Buffer for 1M Qubit TDM
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#define MODULE_NAME       "c_rome_mhd_core"
#define C_ROME_MMIO_BASE  0x3F000000
#define MMIO_REGION_SIZE  0x00000100
#define IRQ_LINE_TDM      42
#define PAGE_SIZE_TDM     4096
#define RING_BUFFER_PAGES 8  /* 8ページのマルチバッファ構成 */

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

/* リングバッファ管理構造体 */
struct c_rome_ring_buffer {
    void *virt_addr[RING_BUFFER_PAGES];
    dma_addr_t phys_handle[RING_BUFFER_PAGES];
    u32 head; /* ハードウェアが現在書き込んでいるページ */
    u32 tail; /* カーネル/CPUが現在処理しているページ */
};

static void __iomem *mmio_base;
static struct c_rome_ring_buffer ring;

/**
 * @brief 8ページ巡回型 TDMパルス同期 割り込みハンドラ
 */
static irqreturn_t c_rome_tdm_ring_irq_handler(int irq, void *dev_id)
{
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;
    u32 status = ioread32(&regs->int_status);

    if (!(status & 0x00000001)) return IRQ_NONE;

    /* 1. 現在ハードウェアが処理を完了したバッファ(tailページ)のデータを回収 */
    u32 *current_page = (u32 *)ring.virt_addr[ring.tail];
    
    // ここで前述のSEC-DED ECC/パリティ検証アルゴリズムをパイプライン実行
    // (デモ簡略化のため、ここではインデックスのみ更新)
    
    /* 2. テールポインタをインクリメントし、リングを1歩進める (8で巡回) */
    ring.tail = (ring.tail + 1) % RING_BUFFER_PAGES;

    /* 3. 次の空きバッファページ(head)のアドレスをハードウェアへ先行設定 */
    ring.head = (ring.head + 1) % RING_BUFFER_PAGES;
    
    /* オーバーラン確認（処理がハードウェアの速度に追いついていない場合） */
    if (unlikely(ring.head == ring.tail)) {
        pr_crit_ratelimited("%s: DMA Ring Buffer Overrun! 1M Qubit TDM stream corrupted.\n", MODULE_NAME);
        iowrite32(0x00000000, &regs->dma_ctrl); // 緊急停止
        goto clear_int;
    }

    /* ハードウェアへ次のターゲット物理アドレスをコミット */
    dma_addr_t next_phys = ring.phys_handle[ring.head];
    iowrite32((u32)(next_phys & 0xFFFFFFFF), &regs->dma_addr_l);
    iowrite32((u32)((next_phys >> 32) & 0xFFFFFFFF), &regs->dma_addr_h);

clear_int:
    iowrite32(0x00000001, &regs->int_status); // W1C
    return IRQ_HANDLED;
}

static int __init c_rome_ring_init(void)
{
    int ret, i;
    struct mhd_reg_map __iomem *regs;

    mmio_base = ioremap(C_ROME_MMIO_BASE, MMIO_REGION_SIZE);
    if (!mmio_base) return -ENOMEM;
    regs = (struct mhd_reg_map __iomem *)mmio_base;

    /* 8ページ分の一貫性DMAバッファを一括アロケーション */
    for (i = 0; i < RING_BUFFER_PAGES; i++) {
        ring.virt_addr[i] = dma_alloc_coherent(NULL, PAGE_SIZE_TDM, &ring.phys_handle[i], GFP_KERNEL);
        if (!ring.virt_addr[i]) {
            ret = -ENOMEM;
            goto free_dma;
        }
    }

    ring.head = 0;
    ring.tail = 0;

    /* 初期のバッファアドレスをセット */
    iowrite32((u32)(ring.phys_handle[0] & 0xFFFFFFFF), &regs->dma_addr_l);
    iowrite32((u32)((ring.phys_handle[0] >> 32) & 0xFFFFFFFF), &regs->dma_addr_h);

    ret = request_irq(IRQ_LINE_TDM, c_rome_tdm_ring_irq_handler, IRQF_TRIGGER_RISING, MODULE_NAME, NULL);
    if (ret) goto free_dma;

    iowrite32(0x00000001, &regs->dma_ctrl);
    pr_info("%s: 8-Page DMA Ring Buffer successfully deployed.\n", MODULE_NAME);
    return 0;

free_dma:
    for (i = 0; i < RING_BUFFER_PAGES; i++) {
        if (ring.virt_addr[i]) {
            dma_free_coherent(NULL, PAGE_SIZE_TDM, ring.virt_addr[i], ring.phys_handle[i]);
        }
    }
    iounmap(mmio_base);
    return ret;
}

static void __exit c_rome_ring_exit(void)
{
    int i;
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;
    if (mmio_base) iowrite32(0x00000000, &regs->dma_ctrl);
    free_irq(IRQ_LINE_TDM, NULL);
    for (i = 0; i < RING_BUFFER_PAGES; i++) {
        dma_free_coherent(NULL, PAGE_SIZE_TDM, ring.virt_addr[i], ring.phys_handle[i]);
    }
    if (mmio_base) iounmap(mmio_base);
}

module_init(c_rome_ring_init);
module_exit(c_rome_ring_exit);
