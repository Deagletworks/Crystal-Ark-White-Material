// src/main.rs (Atomic ioctl 統合版)
use std::fs::OpenOptions;
use std::io;
use std::os::unix::io::AsRawFd;
use std::sync::{Arc, Mutex};
use std::thread;

const ALERT_DEVICE_PATH: &str = "/dev/c_rome_layer3_alert";
const C_ROME_IOC_MAGIC: u8 = b'q';

/// カーネル空間の struct c_rome_alert_packet と完全に一致するアライメント構造体
// src/main.rs (アライメント修正版)
#[repr(C, packed)] // メモリを隙間なく詰め、カーネル側のCアライメントに完全同調させる
#[derive(Debug, Clone)]
pub struct ChtmlAlertPacket {
    pub error_flag: u32,
    pub _pad: u32, // 64bitアライメント用の4バイト明示的パディング
    pub event_timestamp_ns: u64,
    pub error_log: [u8; 128],
}


// nixマクロを用いてアトミック読み出し ioctl コマンドを生成
nix::ioctl_read!(read_atomic_alert, C_ROME_IOC_MAGIC, 3, ChtmlAlertPacket);

fn main() -> io::Result<()> {
    println!("[LAUNCH] C_ROME-OS Layer 3 Secure Atomic Monitor launched.");

    let file = OpenOptions::new()
        .read(true)
        .open(ALERT_DEVICE_PATH)?;
    let fd = file.as_raw_fd();

    // デバイスファイルをioctl専用として運用する常時監視ループ
    loop {
        // ゼロ初期化されたパケット領域を確保
        let mut packet = ChtmlAlertPacket {
            error_flag: 0,
            event_timestamp_ns: 0,
            error_log: [0u8; 128],
        };

        println!("[MONITOR] Entering ultra-low-power sleep via kernel-side wait_queue...");
        
        unsafe {
            // この ioctl 呼び出し自体が、カーネル内でエラーが発生するまで安全にブロッキング（スリープ）します。
            // 発生した瞬間、ログとフラグとタイムスタンプをアトミックに持ち帰ります。
            match read_atomic_alert(fd, &mut packet) {
                Ok(_) if packet.error_flag == 1 => {
                    let log_string = String::from_utf8_lossy(&packet.error_log);
                    eprintln!("\n🚨 [ATOMIC ALERT DETECTED]");
                    eprintln!(" ├─ Timestamp: {} ns", packet.event_timestamp_ns);
                    eprintln!(" └─ Kernel Log: {}", log_string.trim_matches('\0').trim());

                    // ここで安全に代替量子制御トランスデューサへのルーティング処理へ移行
                    // execute_hardware_failover_secure(&file);
                }
                Ok(_) => {}
                Err(e) => {
                    eprintln!("[IOCTL ERROR] Channel synchronization failure: {:?}", e);
                    thread::sleep(std::time::Duration::from_millis(100));
                }
            }
        }
    }
}

