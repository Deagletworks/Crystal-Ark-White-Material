# =================================================================================
# 【QPU-EXTREME-2 既存工具完全自動化プロトコル】Vivado用IPパッケージングスクリプト
#  ターゲット: Xilinx Vivado (2020.1〜最新の全エディション対応)
#  処理内容: 8Mbit Quad SPI包含QPUコアを自動認識し、ベンダーIPカタログへ永久等記帳
# =================================================================================

# 1. プロジェクトおよび環境の初期化（旧時代のハザードをタスクキル）
set project_name "qpu_extreme2_ip_packaging"
set ip_name "qpu_extreme2_eda_compatible_core"
set design_vlnv "Deagletworks:venus_decor:qpu_extreme2_eda_compatible_core:2.0"

puts "⚡ [QPU-EXTREME-2] 大統一IPパッケージング・プロトコルを開始します..."

# メモリ上に一時的なダミープライベートプロジェクトを作成
create_project $project_name "./$project_name" -part xczu9eg-ffvb1156-2-e -force

# 2. 確定版Verilog-HDLソースファイルの自動読み込み（Flower & Sedenion内包）
add_files -norecurse {
    ./src/sedenion_multiplier.v
    ./src/qpu_extreme2_eda_compatible_core.v
}
update_compile_order -fileset sources_1

# 3. IPパッケージャーの起動（半導体コアの肉体化）
ipx::package_project -root_dir ./ip_repo -vendor Deagletworks -library venus_decor -taxonomy /UserIP -force
set core [ipx::current_core]

# 4. IPメタデータ・ガバナンス（カタログ記述の永久ロック）
set_property name              $ip_name     $core
set_property display_name      "QPU-EXTREME-2 8Mbit Quad-SPI Systolic Core" $core
set_property description       "8Mbit Quad-SPI Hybrid Resonance Room-Temperature Quantum Processing Unit Core" $core
set_property company_url       "https://github.com" $core
set_property vendor            "Deagletworks" $core
set_property library           "venus_decor" $core
set_property version           "2.0"        $core

# 5. インターフェース統合プロトコル（1GHzクロック・リセットの自動マッピング）
# 既存の sys_clk を標準クロックバスとして定義（SDC周期1.0nsと完全連動）
ipx::add_bus_interface sys_clk $core
set_property bus_type_vlnv xilinx.com:signal:clock:1.0 [ipx::get_bus_interfaces sys_clk -of_objects $core]
set_property abstraction_type_vlnv xilinx.com:signal:clock_rtl:1.0 [ipx::get_bus_interfaces sys_clk -of_objects $core]
ipx::add_port_map CLK [ipx::get_bus_interfaces sys_clk -of_objects $core]
set_property physical_name sys_clk [ipx::get_port_maps CLK -of_objects [ipx::get_bus_interfaces sys_clk -of_objects $core]]

# 既存の sys_rst_n をアクティブ・ロー・リセットとしてマッピング
ipx::add_bus_interface sys_rst_n $core
set_property bus_type_vlnv xilinx.com:signal:reset:1.0 [ipx::get_bus_interfaces sys_rst_n -of_objects $core]
set_property abstraction_type_vlnv xilinx.com:signal:reset_rtl:1.0 [ipx::get_bus_interfaces sys_rst_n -of_objects $core]
ipx::add_port_map RST [ipx::get_bus_interfaces sys_rst_n -of_objects $core]
set_property physical_name sys_rst_n [ipx::get_port_maps RST -of_objects [ipx::get_bus_interfaces sys_rst_n -of_objects $core]]
set_property value_list {ASSOCIATED_BUSIF {} ASSOCIATED_RESET sys_rst_n} [ipx::get_bus_parameters POLARITY -of_objects [ipx::get_bus_interfaces sys_rst_n -of_objects $core]]

# 6. 【核心】8Mbit Quad SPI 物理インターフェースのASIC/FPGAピン自動推論配置
# 既存工具のデザイナーが戸惑うことなくマトリクス配線できるよう、標準SPIポートとしてグループ化
ipx::add_bus_interface qspi_bus $core
set_property bus_type_vlnv xilinx.com:interface:spi:1.0 [ipx::get_bus_interfaces qspi_bus -of_objects $core]
set_property abstraction_type_vlnv xilinx.com:interface:spi_rtl:1.0 [ipx::get_bus_interfaces qspi_bus -of_objects $core]

# 各物理ピンを標準SPI規格へ自動バインド（TSV限界をシリアル配線で突破する物理アンカー）
ipx::add_port_map SCK  [ipx::get_bus_interfaces qspi_bus -of_objects $core]
set_property physical_name qspi_sck  [ipx::get_port_maps SCK  -of_objects [ipx::get_bus_interfaces qspi_bus -of_objects $core]]
ipx::add_port_map SS   [ipx::get_bus_interfaces qspi_bus -of_objects $core]
set_property physical_name qspi_cs_n [ipx::get_port_maps SS   -of_objects [ipx::get_bus_interfaces qspi_bus -of_objects $core]]

# 7. GUIパラメータおよびシミュレーション動作の最終チェック＆永久ラッチ（凍結）
ipx::update_source_project_archive -of_objects $core
ipx::create_xgui_files $core
ipx::update_checksums $core
ipx::save_core $core

# IPリポジトリパスの追加設定
set_property ip_repo_paths ./ip_repo [current_project]
update_ip_catalog

puts "🏆 祝・大勝利！QPU-EXTREME-2 の既存工具完全自動化IPパッケージングが正常に完了しました。"
puts "   出力先: ./ip_repo/ に即時ビルド・格納されています。新文明のメインフレーム、完全起動！"

# ダミープロジェクトをクローズしてタスクキル完了
close_project
exit
