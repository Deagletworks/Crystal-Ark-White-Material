/**
 * @file c_rome_mhd_flux_core_atomic_ioctl.c
 * @brief C_ROME-OS Core Module with Atomic ioctl Alert & Telemetry Pipeline
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/wait.h>

#define MODULE_NAME       "c_rome_mhd_core"
#define DEVICE_NAME       "c_rome_layer3_alert"

/* ioctl マジックナンバーの再定義（アトミックマッピング構造体転送用） */
#define C_ROME_IOC_MAGIC    'q'
#define C_ROME_IOC_GET_ALERT _IOR(C_ROME_IOC_MAGIC, 3, struct c_rome_alert_packet)

/* ユーザー空間へアトミック転送するための構造体定義 */
struct c_rome_alert_packet {
    u32 error_flag;          /* 1: DED致命的エラー発生 */
    u64 event_timestamp_ns;  /* エラー検知時のカーネルタイムスタンプ */
    char error_log[128];     /* エラーログ文字列 */
};

static int major_number;
static struct cdev alert_cdev;
static struct class *alert_class = NULL;

static DECLARE_WAIT_QUEUE_HEAD(alert_wait_queue);
static atomic_t fatal_error_event = ATOMIC_INIT(0);
static struct c_rome_alert_packet global_alert_pool;
static DEFINE_SPINLOCK(log_spinlock); /* ログ構造体保護用のスピンロック */

/**
 * @brief アトミック ioctl コマンドハンドラ
 */
static long dev_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret;
    struct c_rome_alert_packet local_packet;
    unsigned long flags;

    switch (cmd) {
        case C_ROME_IOC_GET_ALERT:
            /* 1. DEDエラーイベントが発生するまで超低消費電力スリープで待機 */
            ret = wait_event_interruptible(alert_wait_queue, atomic_read(&fatal_error_event) != 0);
            if (ret < 0) return ret; /* 割り込みによる中断 */

            /* 2. スピンロックを確保し、カーネル内プールからデータを安全にローカルへ退避 */
            spin_lock_irqsave(&log_spinlock, flags);
            memcpy(&local_packet, &global_alert_pool, sizeof(struct c_rome_alert_packet));
            atomic_set(&fatal_error_event, 0); /* イベントフラグをクリア */
            spin_unlock_irqrestore(&log_spinlock, flags);

            /* 3. ユーザー空間へ構造体丸ごとアトミックに一括コピー (デッドロックフリー) */
            if (copy_to_user((void __user *)arg, &local_packet, sizeof(struct c_rome_alert_packet))) {
                return -EFAULT;
            }
            return 0;

        default:
            return -EINVAL;
    }
}

/* read/writeを完全に排除し、インターフェースを ioctl のみに制限して堅牢化 */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = dev_unlocked_ioctl,
};

/**
 * @brief 既存の割り込みハンドラ (ISR) 内から呼び出されるDEDログ書き込み処理
 * @note 割り込みコンテキストから呼ばれるため、スピンロックで保護
 */
void c_rome_trigger_hardware_alert(const char *msg)
{
    unsigned long flags;
    
    spin_lock_irqsave(&log_spinlock, flags);
    global_alert_pool.error_flag = 1;
    global_alert_pool.event_timestamp_ns = ktime_get_ns();
    snprintf(global_alert_pool.error_log, sizeof(global_alert_pool.error_log), "%s", msg);
    spin_unlock_irqrestore(&log_spinlock, flags);

    /* 待機中のRust監視プロセスを即座に叩き起こす */
    atomic_set(&fatal_error_event, 1);
    wake_up_interruptible(&alert_wait_queue);
}
