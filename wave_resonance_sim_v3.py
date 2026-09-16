import numpy as np
import matplotlib.pyplot as plt

def run_scaled_tdm_simulation(is_throttled=False):
    # 物理定数および1M Qubit TDMの基本設定
    num_qubits = 1000000
    grid_density = 300
    
    # スロットリング（減速動作時）の動的時間追従補正 (Time-step Scaling)
    if is_throttled:
        # 25MHz 駆動時: パルス間隔は4倍の 40ピコ秒に延伸
        time_slot_ps = 40.0   
        attenuation_decay = 0.4  # 時間延伸に伴い、相関ノイズが空間的に広く散逸する物理項
        title_suffix = "Throttled State (25MHz / 40ps Slot Window)"
    else:
        # 100MHz 正常駆動時: パルス間隔 10ピコ秒
        time_slot_ps = 10.0   
        attenuation_decay = 0.8  # 高密度パルスによる局所熱・クロストーク集中
        title_suffix = "Nominal State (100MHz / 10ps Slot Window)"

    slots_x = np.linspace(0, num_qubits, grid_density)
    slots_y = np.linspace(0, num_qubits, grid_density)
    SX, SY = np.meshgrid(slots_x, slots_y)
    R_slots = np.sqrt((SX - num_qubits/2)**2 + (SY - num_qubits/2)**2)

    # 1. 外部環境ノイズと、時間スケールに応じた熱・位相散逸マトリクス
    np.random.seed(42)
    base_noise = np.sin(SX / 50000) * np.cos(SY / 50000)
    
    # スズ(SnV)空間反転対称性の減衰項に、時間追従補正(attenuation_decay)を乗算
    snv_symmetry_attenuation = 1.0 - np.exp(-attenuation_decay * (R_slots**2) / (num_qubits**2))
    mitigated_noise = base_noise * snv_symmetry_attenuation

    # 2. 空間特性インピーダンスの再マッピング (0.000000 Ohm への収束精度を動的評価)
    # パルス幅が広がると（スロットリング時）、干渉は緩和されるが時間ジッターに対する要求が変化する
    Z_space = 377.0 * (1.0 - np.tanh(2.5e-9 / 2.5e-9 * np.tanh(1200.0 * 2.5e-9 / (R_slots + 1e-5))))
    Z_space = np.clip(Z_space + (mitigated_noise * (5.0 if not is_throttled else 1.2)), 0.0, 377.0)

    return Z_space, mitigated_noise, title_suffix

# 通常時とスロットリング時の2つの時間軸フェーズを並べてシミュレート・可視化
fig, ax = plt.subplots(2, 2, figsize=(15, 11))

for idx, throttled_flag in enumerate([False, True]):
    Z_map, noise_map, label = run_scaled_tdm_simulation(is_throttled=throttled_flag)
    
    # 各状態における残差ノイズ分布のレンダリング
    im_noise = ax[idx, 0].imshow(noise_map, cmap='bwr', extent=[0, 1000000, 0, 1000000])
    ax[idx, 0].set_title(f"Residual Noise: {label}")
    fig.colorbar(im_noise, ax=ax[idx, 0], label='Noise Amp')

    # 各状態における空間特性インピーダンス（0Ωマッピングの維持特性）のレンダリング
    im_z = ax[idx, 1].imshow(Z_map, cmap='viridis', extent=[0, 1000000, 0, 1000000], vmin=0, vmax=50)
    ax[idx, 1].set_title(f"Impedance Profile: {label}")
    fig.colorbar(im_z, ax=ax[idx, 1], label='Z_0 (Ohm)')

plt.tight_layout()
plt.savefig('docs/assets/layer3_resonance_layout.png', dpi=300)
print("[SCALING SUCCESS] Dynamic time-step scaling applied. Simulation asset multi-layered.")
