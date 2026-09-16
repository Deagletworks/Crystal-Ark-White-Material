# test_governance.py
import pytest
import numpy as np

def test_algebra_governance_multi_library():
    """
    kingdon または clifford ライブラリを使用して、
    四元数/幾何代数の基本ルール(反交換性)と、等変性の厳密性をテストします。
    """
    # 1. ライブラリの自動検知
    try:
        from kingdon import Algebra
        HAS_KINGDON = True
    except ImportError:
        HAS_KINGDON = False

    try:
        import clifford as cf
        HAS_CLIFFORD = True
    except ImportError:
        HAS_CLIFFORD = False

    if not (HAS_KINGDON or HAS_CLIFFORD):
        pytest.skip("幾何代数ライブラリ (kingdon または clifford) がインストールされていません。")

    # --- Kingdon でのテスト実行 ---
    if HAS_KINGDON:
        print("\n[Testing with kingdon]")
        alg = Algebra(3)
        e1, e2 = alg.blades['e1'], alg.blades['e2']
        
        # 反交換性テスト: e1*e2 == -e2*e1
        prod1 = e1 * e2
        prod2 = e2 * e1
        assert prod1 == -prod2
        
        # 等変性テスト: f(g(v)) == g(f(v))
        import math
        theta = math.pi / 3  # 60度回転
        R = math.cos(theta / 2) - (e1 * e2) * math.sin(theta / 2)
        w = 1.5 + 0.5 * (e1 * e2)  # 重み多重ベクトル
        v = e1                     # 入力ベクトル
        
        # 左辺: f(g(v)) = w * (R * v * ~R)
        v_rotated = R * v * ~R
        left_side = w * v_rotated
        
        # 右辺: g(f(v)) = R * (w * v) * ~R
        f_v = w * v
        right_side = R * f_v * ~R
        
        # 浮動小数点誤差を考慮した比較
        assert abs(left_side - right_side) < 1e-5

    # --- Clifford でのテスト実行 ---
    if HAS_CLIFFORD:
        print("\n[Testing with clifford]")
        # 3次元空間の幾何代数を生成
        layout, blades = cf.Cl(3)
        e1 = blades['e1']
        e2 = blades['e2']
        
        # 反交換性テスト: e1*e2 == -e2*e1
        prod1 = e1 * e2
        prod2 = e2 * e1
        # cliffordライブラリは == で厳密比較できる
        assert prod1 == -prod2
        
        # 等変性テスト
        theta = np.pi / 3
        # cliffordでのローター生成と共役（~）の定義
        R = np.cos(theta / 2) - (e1 * e2) * np.sin(theta / 2)
        w = 1.5 + 0.5 * (e1 * e2)
        v = e1
        
        left_side = w * (R * v * ~R)
        right_side = R * (w * v) * ~R
        
        # cliffordのマルチベクトル同士の誤差チェック
        diff = (left_side - right_side).norm()
        assert diff < 1e-5
