/**
 * @file c_rome_mhd_flux.c
 * @brief C_ROME-OS Kernel Module for MHD Hydrogen Plasma Flux Vector Writing
 * @note Target Hardware: Layer 3 Multiferroic Controller / Real-Time DAC MMIO
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>

#define MODULE_NAME "c_rome_mhd_flux"
#define C_ROME_MMIO_BASE  0x3F000000  /* マルチフェロイック制御レジスタの物理ベースアドレス */
#define MMIO_REGION_SIZE  0x1000

/* MHD磁束ベクトル書き込み用オフセットアドレス定義 */
#define REG_MHD_FLUX_X    0x00
#define REG_MHD_FLUX_Y    0x04
#define REG_MHD_FLUX_Z    0x08
#define REG_SYNC_TRIGGER  0x0C

MODULE_LICENSE("GPL");
MODULE_AUTHOR("C_ROME-OS Core Team");
MODULE_DESCRIPTION("MHD Plasma Flux Vector Control for Layer 3 Resonance Resonance");

static void __iomem *mhd_control_base;

/**
 * @brief MHD水素プラズマの磁束ベクトル情報をハードウェアへ物理書き込み
 * @param fx X方向磁束密度 (Tesla換算スケーリング整数値)
 * @param fy Y方向磁束密度
 * @param fz Z方向磁束密度
 */
void c_rome_write_mhd_vector(s32 fx, s32 fy, fz, u32 sync_ns)
{
    if (unlikely(!mhd_control_base)) {
        pr_err("%s: MMIO base memory is not mapped\n", MODULE_NAME);
        return;
    }

    /* レジスタへの数ナノ秒パルスに同期した超低遅延バースト書き込み */
    iowrite32((u32)fx, mhd_control_base + REG_MHD_FLUX_X);
    iowrite32((u32)fy, mhd_control_base + REG_MHD_FLUX_Y);
    iowrite32((u32)fz, mhd_control_base + REG_MHD_FLUX_Z);

    /* 時間軸同期フラグ（数ns精度タイミング制御トリガー）の発火 */
    iowrite32(sync_ns, mhd_control_base + REG_SYNC_TRIGGER);
}
EXPORT_SYMBOL(c_rome_write_mhd_vector);

/* モジュールロード時の初期化 */
static int __init c_rome_mhd_init(void)
{
    pr_info("%s: Initializing C_ROME-OS MHD Plasma Vector Module...\n", MODULE_NAME);

    /* 物理アドレス空間の仮想カーネル空間へのマッピング */
    mhd_control_base = ioremap(C_ROME_MMIO_BASE, MMIO_REGION_SIZE);
    if (!mhd_control_base) {
        pr_err("%s: ioremap failed for base 0x%08X\n", MODULE_NAME, C_ROME_MMIO_BASE);
        return -ENOMEM;
    }

    pr_info("%s: MMIO successfully mapped to %p. Layer 3 0-Ohm system ready.\n", MODULE_NAME, mhd_control_base);
    return 0;
}

/* モジュールアンロード時のクリーンアップ */
static void __exit c_rome_mhd_exit(void)
{
    pr_info("%s: Unloading C_ROME-OS MHD Plasma Module.\n", MODULE_NAME);
    if (mhd_control_base) {
        iounmap(mhd_control_base);
    }
}

module_init(c_rome_mhd_init);
module_exit(c_rome_mhd_exit);
