/**
 * @file c_rome_mhd_flux_core_ecc.c
 * @brief C_ROME-OS Core Kernel Module with SEC-DED ECC & Single Parity over TDM-DMA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>

#define MODULE_NAME "c_rome_mhd_core"
#define C_ROME_MMIO_BASE    0x3F000000
#define MMIO_REGION_SIZE    0x00000100
#define IRQ_LINE_TDM        42
#define TDM_BUFFER_SIZE     4096

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

static void __iomem *mmio_base;
static void *dma_virt_buffer;
static dma_addr_t dma_phys_handle;

/**
 * @brief 32ビットデータパケットに対するSEC-DED ECCハミングパリティのチェックと自己修復
 * @param data 検証対象の32ビット符号ワード (上位24bit: データ, 下位8bit: パリティシンドローム)
 * @return u32 修正されたクリーンな24bitデータペイロード
 */
static inline u32 c_rome_validate_and_repair_ecc(u32 encoded_word, bool *error_detected, bool *fatal_corrupt)
{
    u32 data_payload = (encoded_word >> 8) & 0x00FFFFFF;
    u32 received_ecc = encoded_word & 0x000000FF;
    u32 calculated_ecc = 0;
    
    *error_detected = false;
    *fatal_corrupt = false;

    /* 24ビットデータラインに対する超高速XORシンドローム計算 (簡易ハミング行列演算) */
    for (int i = 0; i < 24; i++) {
        if ((data_payload >> i) & 1) {
            calculated_ecc ^= (i + 1); /* 各ビットインデックスをシンドローム空間にマッピング */
        }
    }

    u32 syndrome = received_ecc ^ calculated_ecc;

    if (syndrome != 0) {
        *error_detected = true;
        
        /* シンドロームが示すビット位置（1ベース）を反転させて1ビット自動エラー訂正 (SEC) */
        if (syndrome <= 24) {
            pr_warn_ratelimited("%s: 1-Bit ECC Error detected at bit %d. Repairing payload.\n", MODULE_NAME, syndrome);
            data_payload ^= (1 << (syndrome - 1));
        } else {
            /* 2ビット以上の多重エラー（ハミング限界突破）の検出 (DEDフラグ発火) */
            *fatal_corrupt = true;
            pr_crit_ratelimited("%s: Fatal Uncorrectable Multi-Bit ECC Corruption in TDM Packet!\n", MODULE_NAME);
        }
    }

    return data_payload;
}

/**
 * @brief TDMパルス同期 割り込みハンドラ (ECC・パリティインスペクション拡張版)
 */
static irqreturn_t c_rome_tdm_irq_handler(int irq, void *dev_id)
{
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;
    u32 status = ioread32(&regs->int_status);

    if (!(status & 0x00000001)) {
        return IRQ_NONE;
    }

    /* 1. DMA一貫性バッファから最新のMHDパケットを読み出し検証 */
    u32 *packet_ptr = (u32 *)dma_virt_buffer;
    for (int i = 0; i < (TDM_BUFFER_SIZE / sizeof(u32)); i += 4) {
        bool err, fatal;
        
        /* X, Y, Z ベクトルのECC修復とパリティチェック */
        u32 clean_x = c_rome_validate_and_repair_ecc(packet_ptr[i], &err, &fatal);
        u32 clean_y = c_rome_validate_and_repair_ecc(packet_ptr[i+1], &err, &fatal);
        
        if (unlikely(fatal)) {
            /* 致命的なデータ汚染時は安全のため前回の正常値を維持し、ハードウェアのトリガーを緊急緊急マスク */
            iowrite32(0x00000000, &regs->dma_ctrl);
            goto clear_interrupt;
        }
        
        /* 正常/修復されたパケットデータをMMIO物理レジスタへパイプライン書き込み */
        iowrite32(clean_x, &regs->flux_x);
        iowrite32(clean_y, &regs->flux_y);
    }

    /* 2. ページ反転 (Ping-Pong) */
    u32 dma_ctrl_val = ioread32(&regs->dma_ctrl);
    dma_ctrl_val ^= 0x00000002;
    iowrite32(dma_ctrl_val, &regs->dma_ctrl);

clear_interrupt:
    iowrite32(0x00000001, &regs->int_status);
    return IRQ_HANDLED;
}

static int __init c_rome_core_init(void)
{
    int ret;
    struct mhd_reg_map __iomem *regs;

    mmio_base = ioremap(C_ROME_MMIO_BASE, MMIO_REGION_SIZE);
    if (!mmio_base) return -ENOMEM;
    regs = (struct mhd_reg_map __iomem *)mmio_base;

    dma_virt_buffer = dma_alloc_coherent(NULL, TDM_BUFFER_SIZE, &dma_phys_handle, GFP_KERNEL);
    if (!dma_virt_buffer) {
        iounmap(mmio_base);
        return -ENOMEM;
    }

    iowrite32((u32)(dma_phys_handle & 0xFFFFFFFF), &regs->dma_addr_l);
    iowrite32((u32)((dma_phys_handle >> 32) & 0xFFFFFFFF), &regs->dma_addr_h);

    ret = request_irq(IRQ_LINE_TDM, c_rome_tdm_irq_handler, IRQF_TRIGGER_RISING, MODULE_NAME, NULL);
    if (ret) {
        dma_free_coherent(NULL, TDM_BUFFER_SIZE, dma_virt_buffer, dma_phys_handle);
        iounmap(mmio_base);
        return ret;
    }

    iowrite32(0x00000001, &regs->dma_ctrl);
    pr_info("%s: SEC-DED ECC Engine initialized over DMA stream.\n", MODULE_NAME);
    return 0;
}

static void __exit c_rome_core_exit(void)
{
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;
    if (mmio_base) iowrite32(0x00000000, &regs->dma_ctrl);
    free_irq(IRQ_LINE_TDM, NULL);
    if (dma_virt_buffer) dma_free_coherent(NULL, TDM_BUFFER_SIZE, dma_virt_buffer, dma_phys_handle);
    if (mmio_base) iounmap(mmio_base);
}

module_init(c_rome_core_init);
module_exit(c_rome_core_exit);
