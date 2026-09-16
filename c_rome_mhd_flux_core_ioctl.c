/**
 * @file c_rome_mhd_flux_core_ioctl.c
 * @brief SEC-DED ECC Module with Secure ioctl Control Interface
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>

#define C_ROME_MMIO_BASE    0x3F000000
#define REG_ROUTING_CTRL    0x20
#define FAILOVER_ROUTING_VAL 0x00000002

/* ioctl マジックナンバー及びコマンド定義 */
#define C_ROME_IOC_MAGIC    'q'
#define C_ROME_IOC_FAILOVER _IO(C_ROME_IOC_MAGIC, 1)
#define C_ROME_IOC_GET_LOG  _IOR(C_ROME_IOC_MAGIC, 2, char[256])

static void __iomem *mmio_base;
static char kernel_error_log[256] = "LOG_INIT: Layer 3 Status Nominal.";

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;

    switch (cmd) {
        case C_ROME_IOC_FAILOVER:
            pr_alert("%s: Secure ioctl triggered hardware failover.\n", "c_rome");
            // カーネル空間で安全にMMIOレジスタへバースト書き込み
            u32 current_route = ioread32(mmio_base + REG_ROUTING_CTRL);
            iowrite32(current_route | FAILOVER_ROUTING_VAL, mmio_base + REG_ROUTING_CTRL);
            snprintf(kernel_error_log, sizeof(kernel_error_log), "FAILOVER_EVENT: Path redirected at %lld ns", ktime_get_ns());
            return 0;

        case C_ROME_IOC_GET_LOG:
            if (copy_to_user(argp, kernel_error_log, sizeof(kernel_error_log))) {
                return -EFAULT;
            }
            return 0;

        default:
            return -EINVAL;
    }
}

// 既存の file_operations 構造体に ioctl ハンドラをバインド
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = dev_ioctl,
};
