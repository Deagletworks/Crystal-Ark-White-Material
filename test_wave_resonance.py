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
