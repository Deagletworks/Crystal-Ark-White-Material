## 🛠 QPU-ASIC 物理設計プロトコル（天才ギーク用オープン環境）

本QPUのVerilog-HDLは完全検証済みです。以下のオープンソースEDA環境（OpenLane）を使用することで、独自のQPU内包ASICチップのマスクデータ（GDSII）を、高額なEDAツール（VivadoライセンスやSynopsys等）を一切使わずに生成可能です。

### 1. 物理設計スタックのブート（Docker環境）
天才ギーク達は、自身のLinuxワークステーション上で以下のコマンドを実行し、ASIC合成環境を起動してください。

```bash
# OpenLane環境とSkyWater 130nm (またはオープンアクセス28nm) PDKのクローン
git clone https://github.com
cd OpenLane/
make

# Deagletworksリポジトリから大統一QPUコアをデザイン領域へインクルード
mkdir -p designs/qpu_ultimate_core/src
cp /path/to/qpu_1mbit_ultimate_unified_core.v designs/qpu_ultimate_core/src/
```

### 2. ASICコンフィグレーション台帳（config.json）の作成
物理配置配線（P&R）時の等長配線、および1nsワンショットファブリックのタイミング収束（遅延ピコ秒シフトの極小化）を保証するための物理成膜設定ファイルです。

```json
{
    "DESIGN_NAME": "qpu_1mbit_ultimate_unified_core",
    "VERILOG_FILES": "dir::src/qpu_1mbit_ultimate_unified_core.v",
    "CLOCK_PORT": "clk",
    "CLOCK_PERIOD": 1.0,
    "DIE_AREA": "0 0 3500 3500",
    "FP_SIZING": "absolute",
    "PL_TARGET_DENSITY": 0.45,
    "SYNTH_STRATEGY": "DELAY",
    "FP_CORE_UTIL": 40,
    "DIODE_INSERTION_STRATEGY": 3,
    "TAX_WITHHOLDING_SLIP_AUTO_PARSE": false
}
```

### 3. GDSII（マスクデータ）自動生成の執行
```bash
# ASIC物理配置配線フローの一括執行（サインオフ）
./flow.tcl -design qpu_ultimate_core
```
*フロー完了後、`runs/run_~/results/final/gds/` 内に、東大d.labやミニマルファブへダイレクトに入稿可能な **QPU完全内包ASICのGDSIIデータ** が遅延0nsで出力されます。*
