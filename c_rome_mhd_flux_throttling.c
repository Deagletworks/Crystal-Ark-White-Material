/**
 * @file c_rome_mhd_flux_throttling.c
 * @brief DMA Ring Buffer Monitor with Hardware Clock Throttling Feedback Loop
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>

#define C_ROME_MMIO_BASE    0x3F000000
#define MMIO_REGION_SIZE    0x00000100
#define IRQ_LINE_TDM        42
#define PAGE_SIZE_TDM       4096
#define RING_BUFFER_PAGES   8

/* 追加のクロック・スロットリング制御レジスタ定義 */
#define REG_THROTTLING_CTRL 0x24  /* 0x24: クロック・周波数制御レジスタ */
#define CLK_NOMINAL_100MHZ  0x00000000 /* 通常時：100MHzフル駆動 */
#define CLK_THROTTLE_25MHZ  0x00000003 /* スロットリング時：周波数を1/4に動的抑制 */

struct mhd_reg_map {
    u32 flux_x;          /* 0x00 */
    u32 flux_y;          /* 0x04 */
    u32 flux_z;          /* 0x08 */
    u32 sync_trigger;    /* 0x0C */
    u32 dma_addr_l;      /* 0x10 */
    u32 dma_addr_h;      /* 0x14 */
    u32 dma_ctrl;        /* 0x18 */
    u32 int_status;      /* 0x1C */
    u32 padding[1];      /* 0x20 - REG_ROUTING_CTRL用 */
    u32 throttling_ctrl; /* 0x24 - 動的スロットリング */
};

struct c_rome_class_ring {
    void *virt_addr[RING_BUFFER_PAGES];
    dma_addr_t phys_handle[RING_BUFFER_PAGES];
    u32 head;
    u32 tail;
};

static void __iomem *mmio_base;
static struct c_rome_class_ring ring;

/**
 * @brief 残り空きバッファページ数を算出する関数
 */
static inline u32 get_available_ring_pages(void)
{
    /* リングバッファの特性上、現在のheadとtailの間隔から残量を計算 */
    if (ring.head >= ring.tail) {
        return RING_BUFFER_PAGES - (ring.head - ring.tail);
    } else {
        return ring.tail - ring.head;
    }
}

/**
// c_rome_mhd_flux_throttling.c 内の割り込み処理を以下に修正
static irqreturn_t c_rome_tdm_throttling_handler(int irq, void *dev_id)
{
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;
    u32 status = ioread32(&regs->int_status);

    if (!(status & 0x00000001)) return IRQ_NONE;

    /* CPU側の処理完了に伴うテールポインタ更新 */
    ring.tail = (ring.tail + 1) % RING_BUFFER_PAGES;
    
    smp_mb(); /* コンパイラ・CPUの最適化による並べ替えを完全に抑止 (メモリバリア) */

    /* ハードウェア側の次ページインクリメント */
    ring.head = (ring.head + 1) % RING_BUFFER_PAGES;

    u32 available_pages = get_available_ring_pages();

    if (unlikely(available_pages <= 1)) {
        /* スロットリング発火 */
        iowrite32(CLK_THROTTLE_25MHZ, &regs->throttling_ctrl);
    } else if (available_pages >= 4) {
        if (ioread32(&regs->throttling_ctrl) != CLK_NOMINAL_100MHZ) {
            iowrite32(CLK_NOMINAL_100MHZ, &regs->throttling_ctrl);
        }
    }

    smp_wmb(); /* ハードウェアへのレジスタ書き込み前に全てのメモリ操作を確定 */

    dma_addr_t next_phys = ring.phys_handle[ring.head];
    iowrite32((u32)(next_phys & 0xFFFFFFFF), &regs->dma_addr_l);
    iowrite32((u32)((next_phys >> 32) & 0xFFFFFFFF), &regs->dma_addr_h);

    iowrite32(0x00000001, &regs->int_status);
    return IRQ_HANDLED;
}

