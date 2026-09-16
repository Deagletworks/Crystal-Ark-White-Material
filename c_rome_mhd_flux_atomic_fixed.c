/**
 * @file c_rome_mhd_flux_atomic_fixed.c
 * @brief C_ROME-OS Core Module with Strictly Packed Alignment & Atomic Ring Pointers
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>

#define RING_BUFFER_PAGES 8
#define CLK_THROTTLE_25MHZ  0x00000003
#define CLK_NOMINAL_100MHZ  0x00000000

/* Rust側の ChtmlAlertPacket と完全に1対1で同調するアトミックパック構造体 */
struct c_rome_alert_packet {
    u32 error_flag;
    u32 _pad;                  /* 64bitアライメント用明示的パディング */
    u64 event_timestamp_ns;
    char error_log[128];
} __attribute__((packed));     /* GCCのアライメントパディングを強制排除 */

struct c_rome_class_ring {
    void *virt_addr[RING_BUFFER_PAGES];
    dma_addr_t phys_handle[RING_BUFFER_PAGES];
    atomic_t head;             /* アトミック型へ拡張 */
    atomic_t tail;             /* アトミック型へ拡張 */
};

struct mhd_reg_map {
    u32 flux_x; u32 flux_y; u32 flux_z; u32 sync_trigger;
    u32 dma_addr_l; u32 dma_addr_h; u32 dma_ctrl; u32 int_status;
    u32 padding; u32 throttling_ctrl;
};

static void __iomem *mmio_base;
static struct c_rome_class_ring ring;

static irqreturn_t c_rome_tdm_throttling_handler(int irq, void *dev_id)
{
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;
    u32 status = ioread32(&regs->int_status);
    int h_val, t_val, avail;

    if (!(status & 0x00000001)) return IRQ_NONE;

    /* 1. アトミックなポインタ前進操作 (SMP競合を完全に排除) */
    t_val = atomic_inc_return(&ring.tail) % RING_BUFFER_PAGES;
    smp_mb();
    h_val = atomic_inc_return(&ring.head) % RING_BUFFER_PAGES;

    /* 2. 残りページの安全な算出 */
    avail = (h_val >= t_val) ? (RING_BUFFER_PAGES - (h_val - t_val)) : (t_val - h_val);

    /* 3. フィードバック制御およびライトバリアの適用 */
    if (unlikely(avail <= 1)) {
        iowrite32(CLK_THROTTLE_25MHZ, &regs->throttling_ctrl);
    } else if (avail >= 4) {
        if (ioread32(&regs->throttling_ctrl) != CLK_NOMINAL_100MHZ) {
            iowrite32(CLK_NOMINAL_100MHZ, &regs->throttling_ctrl);
        }
    }

    smp_wmb();
    mmiowb(); /* ハードウェアバス上でのI/O順序を強制 */

    dma_addr_t next_phys = ring.phys_handle[h_val];
    iowrite32((u32)(next_phys & 0xFFFFFFFF), &regs->dma_addr_l);
    iowrite32((u32)((next_phys >> 32) & 0xFFFFFFFF), &regs->dma_addr_h);

    iowrite32(0x00000001, &regs->int_status);
    return IRQ_HANDLED;
}
