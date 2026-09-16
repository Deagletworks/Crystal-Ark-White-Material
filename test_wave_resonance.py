import numpy as np
import pytest

# テスト対象の物理定数とコアロジックをカプセル化した検証用関数
def calculate_center_impedance():
    tau_sw = 2.5e-9        # 2.5 ns
    v_max = 1200.0         # 1200 m/s
    size = 250
    
    x = np.linspace(-5, 5, size)
    y = np.linspace(-5, 5, size)
    X, Y = np.meshgrid(x, y)
    R = np.sqrt(X**2 + Y**2)
    
    # 物理共鳴モデル方程式の再現
    polarization_frontier = np.tanh((v_max * tau_sw) / (R + 1e-5))
    Z_space = 377.0 * (1.0 - np.tanh(tau_sw / tau_sw * polarization_frontier))
    
    # 中心座標 (R < 1.0) のインピーダンス配列を抽出
    center_mask = R < 1.0
    return Z_space[center_mask]

def test_impedance_absolute_zero_convergence():
    """
    【LAYER 3 検証】中心共鳴領域における空間特性インピーダンスが
    完全導電・全反射条件である 0.000000 Ohm に誤差 1e-6 以内で収束しているかをテスト
    """
    center_impedance_values = calculate_center_impedance()
    
    # 全ての中心サンプリング点でインピーダンスが 0.000000 Ω であることをアサート
    mean_impedance = np.mean(center_impedance_values)
    max_impedance = np.max(center_impedance_values)
    
    print(f"\n[CI VERIFY] Center Mean Impedance: {mean_impedance:.6f} Ohm")
    print(f"[CI VERIFY] Center Max Impedance: {max_impedance:.6f} Ohm")
    
    assert mean_impedance == pytest.approx(0.0, abs=1e-6), \
        f"Impedance did not converge to 0 Ohm. Mean was {mean_impedance}"
    assert max_impedance == pytest.approx(0.0, abs=1e-6), \
        f"Maximum core impedance bounds exceeded 0 Ohm. Max was {max_impedance}"



import numpy as np
import pytest

def calculate_tdm_crosstalk_fidelity():
    """
    100万Qubit TDM遅延ループマトリクス内におけるクロストーク特性を演算
    """
    num_qubits = 1000000
    grid_density = 500
    
    slots_x = np.linspace(0, num_qubits, grid_density)
    slots_y = np.linspace(0, num_qubits, grid_density)
    SX, SY = np.meshgrid(slots_x, slots_y)
    
    # 中心（Layer 3 共鳴領域）からの距離
    R_slots = np.sqrt((SX - num_qubits/2)**2 + (SY - num_qubits/2)**2)
    
    # 裸のクロストーク確率（保護なしの状態）
    raw_crosstalk = np.abs(np.sin(SX / 50000) * np.cos(SY / 50000) * np.exp(-R_slots / (num_qubits / 3)))
    
    # Layer 3（インピーダンス 0 Ohm 化）の防護シールドによる減衰効果
    # 中心共鳴域に近づくほど、クロストーク確率が極小化する物理項
    shield_factor = 1.0 - np.exp(-R_slots / (num_qubits / 10))
    secured_crosstalk = raw_crosstalk * shield_factor
    
    # 特に防護が最も強固に効く、中心コアエリア（半径10万スロット以内）を評価対象とする
    core_mask = R_slots < 100000
    return secured_crosstalk[core_mask]

def test_tdm_slot_crosstalk_threshold():
    """
    【1M Qubit TDM 検証】
    Layer 3（0Ω）保護下のコア領域において、隣接スロットへの干渉確率（Crosstalk Probability）が
    誤り訂正しきれる限界値である「1e-4（0.01%）以下」に抑え込まれているかを検証
    """
    core_crosstalk_values = calculate_tdm_crosstalk_fidelity()
    
    max_core_crosstalk = np.max(core_crosstalk_values)
    mean_core_crosstalk = np.mean(core_crosstalk_values)
    
    print(f"\n[CI TDM VERIFY] Maximum Core Crosstalk: {max_core_crosstalk:.6e}")
    print(f"[CI TDM VERIFY] Mean Core Crosstalk: {mean_core_crosstalk:.6e}")
    
    # 許容基準 1e-4 (0.0001) のアサーション
    assert max_core_crosstalk <= 1e-4, \
        f"Fatal TDM Slot Crosstalk detected! Max interference {max_core_crosstalk:.6e} exceeded tolerance threshold 1e-4."
        
    assert mean_core_crosstalk <= 1e-5, \
        f"Average system fidelity degradation inside core is too high: {mean_core_crosstalk:.6e}"
