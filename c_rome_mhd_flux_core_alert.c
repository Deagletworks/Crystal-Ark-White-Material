/**
 * @file c_rome_mhd_flux_core_alert.c
 * @brief C_ROME-OS Core Module with SEC-DED ECC & Real-Time Character Device Alert Engine
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/wait.h>

#define MODULE_NAME       "c_rome_mhd_core"
#define DEVICE_NAME       "c_rome_layer3_alert"
#define C_ROME_MMIO_BASE  0x3F000000
#define MMIO_REGION_SIZE  0x00000100
#define IRQ_LINE_TDM      42
#define TDM_BUFFER_SIZE   4096

/* キャラクターデバイス制御用グローバル定義 */
static int major_number;
static struct cdev alert_cdev;
static struct class *alert_class = NULL;
static DECLARE_WAIT_QUEUE_HEAD(alert_wait_queue);
static atomic_t fatal_error_flag = ATOMIC_INIT(0);

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

/* ユーザー空間プロセスがアラートファイルをリードした際の挙動 (ブロッキングI/Oによる待機) */
static ssize_t dev_read(struct file *file, char __user *buffer, size_t length, loff_t *offset)
{
    int ret;
    char alert_msg[] = "CRITICAL_ERROR: DED_FATAL_CORRUPTION_DETECTED_IN_LAYER3_TDM_DMA\n";
    size_t msg_len = sizeof(alert_msg);

    /* DEDエラーが発生するまでユーザー空間プロセスを超低消費電力待機（スリープ）させる */
    ret = wait_event_interruptible(alert_wait_queue, atomic_read(&fatal_error_flag) != 0);
    if (ret < 0) return ret;

    if (length < msg_len) return -EINVAL;

    /* アラートメッセージをユーザー空間へコピー */
    if (copy_to_user(buffer, alert_msg, msg_len)) {
        return -EFAULT;
    }

    /* フラグのリセット */
    atomic_set(&fatal_error_flag, 0);
    return msg_len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
};

/**
 * @brief SEC-DED ECCハミングパリティのチェックと自己修復
 */
static inline u32 c_rome_validate_and_repair_ecc(u32 encoded_word, bool *fatal_corrupt)
{
    u32 data_payload = (encoded_word >> 8) & 0x00FFFFFF;
    u32 received_ecc = encoded_word & 0x000000FF;
    u32 calculated_ecc = 0;
    
    for (int i = 0; i < 24; i++) {
        if ((data_payload >> i) & 1) calculated_ecc ^= (i + 1);
    }

    u32 syndrome = received_ecc ^ calculated_ecc;

    if (unlikely(syndrome != 0)) {
        if (syndrome <= 24) {
            data_payload ^= (1 << (syndrome - 1)); // 1ビット自動訂正
        } else {
            *fatal_corrupt = true; // 2ビット致命的エラー (DED)
        }
    }
    return data_payload;
}

/**
 * @brief TDMパルス同期 割り込みハンドラ
 */
static irqreturn_t c_rome_tdm_irq_handler(int irq, void *dev_id)
{
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;
    u32 status = ioread32(&regs->int_status);

    if (!(status & 0x00000001)) return IRQ_NONE;

    u32 *packet_ptr = (u32 *)dma_virt_buffer;
    for (int i = 0; i < (TDM_BUFFER_SIZE / sizeof(u32)); i += 4) {
        bool fatal = false;
        u32 clean_x = c_rome_validate_and_repair_ecc(packet_ptr[i], &fatal);
        u32 clean_y = c_rome_validate_and_repair_ecc(packet_ptr[i+1], &fatal);
        
        if (unlikely(fatal)) {
            /* 1. ハードウェアのDMA転送を緊急マスク停止 */
            iowrite32(0x00000000, &regs->dma_ctrl);
            
            /* 2. リアルタイムアラートフラグのセットと、待機中のユーザー空間アプリの即時叩き起こし */
            atomic_set(&fatal_error_flag, 1);
            wake_up_interruptible(&alert_wait_queue);
            
            goto clear_interrupt;
        }
        
        iowrite32(clean_x, &regs->flux_x);
        iowrite32(clean_y, &regs->flux_y);
    }

    /* ページ反転 (Ping-Pong) */
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
    dev_t dev;

    /* 1. キャラクターデバイスの動的登録 */
    ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (ret < 0) return ret;
    major_number = MAJOR(dev);

    cdev_init(&alert_cdev, &fops);
    ret = cdev_add(&alert_cdev, dev, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev, 1);
        return ret;
    }

    /* /sys/class/c_rome_layer3_alert の自動生成、/dev/c_rome_layer3_alert ノードの自動作成 */
    alert_class = class_create(DEVICE_NAME);
    if (IS_ERR(alert_class)) {
        cdev_del(&alert_cdev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(alert_class);
    }
    device_create(alert_class, NULL, dev, NULL, DEVICE_NAME);

    /* 2. MMIO & DMA & IRQ のセットアップ */
    mmio_base = ioremap(C_ROME_MMIO_BASE, MMIO_REGION_SIZE);
    if (!mmio_base) goto class_err;
    regs = (struct mhd_reg_map __iomem *)mmio_base;

    dma_virt_buffer = dma_alloc_coherent(NULL, TDM_BUFFER_SIZE, &dma_phys_handle, GFP_KERNEL);
    if (!dma_virt_buffer) goto io_err;

    iowrite32((u32)(dma_phys_handle & 0xFFFFFFFF), &regs->dma_addr_l);
    iowrite32((u32)((dma_phys_handle >> 32) & 0xFFFFFFFF), &regs->dma_addr_h);

    ret = request_irq(IRQ_LINE_TDM, c_rome_tdm_irq_handler, IRQF_TRIGGER_RISING, MODULE_NAME, NULL);
    if (ret) goto dma_err;

    iowrite32(0x00000001, &regs->dma_ctrl);
    pr_info("%s: SEC-DED ECC & Character Device Alert Core deployed.\n", MODULE_NAME);
    return 0;

dma_err:
    dma_free_coherent(NULL, TDM_BUFFER_SIZE, dma_virt_buffer, dma_phys_handle);
io_err:
    iounmap(mmio_base);
class_err:
    device_destroy(alert_class, MKDEV(major_number, 0));
    class_destroy(alert_class);
    cdev_del(&alert_cdev);
    unregister_chrdev_region(MKDEV(major_number, 0), 1);
    return -ENOMEM;
}

static void __exit c_rome_core_exit(void)
{
    struct mhd_reg_map __iomem *regs = (struct mhd_reg_map __iomem *)mmio_base;
    if (mmio_base) iowrite32(0x00000000, &regs->dma_ctrl);
    free_irq(IRQ_LINE_TDM, NULL);
    if (dma_virt_buffer) dma_free_coherent(NULL, TDM_BUFFER_SIZE, dma_virt_buffer, dma_phys_handle);
    if (mmio_base) iounmap(mmio_base);

    device_destroy(alert_class, MKDEV(major_number, 0));
    class_destroy(alert_class);
    cdev_del(&alert_cdev);
    unregister_chrdev_region(MKDEV(major_number, 0), 1);
    pr_info("%s: Alert Core disassembled.\n", MODULE_NAME);
}

module_init(c_rome_core_init);
module_exit(c_rome_core_exit);
