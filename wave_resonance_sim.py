import numpy as np
import matplotlib.pyplot as plt

# 空間グリッドと周波数パラメータの設定
size = 200
x = np.linspace(-10, 10, size)
y = np.linspace(-10, 10, size)
X, Y = np.meshgrid(x, y)

# マルチフェロイック共鳴条件: Z_0 -> 0.000000 Ohm
# 誘電率の極大化と透磁率の極小化によるインピーダンス・ゼロの波動プロファイルを擬似生成
R = np.sqrt(X**2 + Y**2)
resonance_wave = np.sin(2 * np.pi * R / 3.0) * np.exp(-0.1 * R)
impedance_zero_shield = np.where(R < 4, 0.0, np.abs(resonance_wave) * 377.0)

# プロット作成
fig, ax = plt.subplots(1, 2, figsize=(12, 5))

# 波動共鳴パターンの描画
im0 = ax[0].imshow(resonance_wave, cmap='coolwarm', extent=[-10,10,-10,10])
ax[0].set_title("Layer 3: Wave Resonance Phase")
fig.colorbar(im0, ax=ax[0], label='Phase Amplitude')

# インピーダンス空間分布 (中心部 0.000000 Ω)
im1 = ax[1].imshow(impedance_zero_shield, cmap='inferno', extent=[-10,10,-10,10])
ax[1].set_title("Spatial Impedance Z_0 -> 0.000000 Ohm")
fig.colorbar(im1, ax=ax[1], label='Impedance (Ohm)')

plt.tight_layout()
# 自動埋め込み対象の画像として保存
plt.savefig('docs/assets/layer3_resonance_layout.png', dpi=300)
print("[SUCCESS] Layout image generated at docs/assets/layer3_resonance_layout.png")
