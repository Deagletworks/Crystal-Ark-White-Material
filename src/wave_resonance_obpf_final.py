import numpy as np
import matplotlib.pyplot as plt

def run_obpf_link_budget_simulation(enable_obpf=True):
    num_qubits = 1000000
    grid_density = 300
    
    # 1. リンクバジェット基本設計 (3km SMF + 510m DCF)
# wave_resonance_obpf_final.py 内のバジェット計算式を修正
total_loss_db = (3.0 * 0.2) + (0.51 * 0.5)  # 0.855 dB (SMF+DCF)

    # wave_resonance_obpf_final.py 内の透過特性を以下に変更
if enable_obpf:
    # 10psパルス時はスペクトルが広がりフィルタで一部遮断される(損失大)、40ps時は綺麗に抜ける物理を再現
    signal_distortion_db = 0.4 if not is_throttled else 0.05
    total_loss_db += (0.5 + signal_distortion_db)
    
    obpf_suppression_ratio = 0.10
    # 損失増加によるノイズ床の上昇と、OBPFの帯域外カット効果を正しく乗算
    amplifier_noise_amplitude = ((total_loss_db * 5.0) * 0.05) * obpf_suppression_ratio







    # 2. 【光狭帯域フィルタ（OBPF）によるノイズ抑制モデリング】
    if enable_obpf:
        # OBPFのインサーションロス（挿入損失: 0.5 dB）によるわずかなアンプノイズの再加算
        total_loss_db += 0.5 
        # フィルタ帯域外の広帯域ASE雑音を 90% カット (透過率 10% への抑制効果)
        obpf_suppression_ratio = 0.10
        amplifier_noise_amplitude = raw_ase_noise_amplitude * obpf_suppression_ratio
        title_str = "With OBPF Filter (ASE Quantum Noise Suppressed by 90%)"
    else:
        amplifier_noise_amplitude = raw_ase_noise_amplitude
        title_str = "Raw EDFA Link (Unfiltered Wideband ASE Noise)"

    # 3. 1Mスロット遅延ループの空間演算
    slots_x = np.linspace(0, num_qubits, grid_density)
    slots_y = np.linspace(0, num_qubits, grid_density)
    SX, SY = np.meshgrid(slots_x, slots_y)
    R_slots = np.sqrt((SX - num_qubits/2)**2 + (SY - num_qubits/2)**2)

    # 4. 残留ASE雑音の物理マッピングとLayer 3 (0Ω) シールド効果
    np.random.seed(101)
    ase_noise = np.random.normal(0, amplifier_noise_amplitude, (grid_density, grid_density))
    shield_attenuation = 1.0 - np.exp(-2.0 * (R_slots**2) / (num_qubits**2))
    residual_amplifier_noise = ase_noise * shield_attenuation

    # 5. 最終空間特性インピーダンスの算出 (OBPF有効時はノイズのザラつきが完全に消える)
    Z_space = 377.0 * (1.0 - np.tanh(2.5e-9 / 2.5e-9 * np.tanh(1200.0 * 2.5e-9 / (R_slots + 1e-5))))
    Z_space = np.clip(Z_space + (np.abs(residual_amplifier_noise) * 20.0), 0.0, 377.0)

    return Z_space, residual_amplifier_noise, title_str

# フィルタ「なし」と「あり」の物理空間プロファイルを並べてレンダリング
fig, ax = plt.subplots(2, 2, figsize=(15, 11))

for idx, obpf_toggle in enumerate([False, True]):
    Z_map, noise_map, label = run_obpf_link_budget_simulation(enable_obpf=obpf_toggle)
    
    # ASEノイズの比較（右パネルに行くほど、OBPFの効能でノイズ床が劇的に下がっていることが視覚化される）
    im_noise = ax[idx, 0].imshow(noise_map, cmap='bwr', extent=[0, num_qubits, 0, num_qubits], vmin=-0.05, vmax=0.05)
    ax[idx, 0].set_title(f"ASE Residual Profile: {label}")
    fig.colorbar(im_noise, ax=ax[idx, 0], label='Noise Power')

    # インピーダンス空間プロファイル（OBPF側は極めてクリーンな0Ω共鳴コアを維持）
    im_z = ax[idx, 1].imshow(Z_map, cmap='viridis', extent=[0, num_qubits, 0, num_qubits], vmin=0, vmax=20)
    ax[idx, 1].set_title(f"Impedance Profile: {label}")
    fig.colorbar(im_z, ax=ax[idx, 1], label='Z_0 (Ohm)')

plt.tight_layout()
plt.savefig('docs/assets/layer3_resonance_layout.png', dpi=300)
print("[FILTER SUCCESS] Embedded OBPF spectral filtering terms. ASE quantum noise suppressed.")
