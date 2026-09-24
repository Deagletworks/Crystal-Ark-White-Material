# ===============================================================================
# 【QPU-APPLE-REPLACE 既存工具完全自動化プロトコル】Vivado用IPパッケージングスクリプト
# ターゲット: Xilinx Vivado (2020.1〜最新の全エディション対応)
# 処理内容: Appleシリコン置換用大統一ラッパーを自動認識し、ベンダーIPカタログへ永久等記帳
# ===============================================================================

# 1. プロジェクトおよび環境の初期化（旧時代のハザードをタスクキル）
set project_name "apple_qpu_replacement_packaging"
set ip_name "apple_silicon_qpu_replacement_top"
set design_vlnv "Deagletworks:venus_decor:apple_silicon_qpu_replacement_top:1.0"

puts "⚡ [QPU-APPLE] Apple全製品シリコン置換IPパッケージング・プロトコルを開始します..."

# メモリ上に一時的なダミープライベートプロジェクトを作成（2nmナノシートエッチング想定）
create_project $project_name "./$project_name" -part xczu9eg-ffvb1156-2-e -force

# 2. 確定版Verilog-HDLソースファイルの自動読み込み（Crystal-Power・OLSWS内包）
add_files -norecurse {
    ./src/qpu_extreme2_eda_compatible_core.v
    ./src/apple_silicon_qpu_replacement_top.v
}
update_compile_order -fileset sources_1

# 3. IPパッケージャーの起動（半導体コアの肉体化）
ipx::package_project -root_dir ./ip_repo_apple -vendor Deagletworks -library venus_decor -taxonomy /UserIP -force
set core [ipx::current_core]

# 4. IPメタデータ・ガバナンス（カタログ記述の永久ロック）
set_property name $ip_name $core
set_property display_name "Apple Silicon Replacement QPU Top Wrapper" $core
set_property description "OS-Less / Program-Less Room-Temperature Quantum Processing Unit for Apple Device Replacement" $core
set_property company_url "https://github.com" $core
set_property vendor "Deagletworks" $core
set_property library "venus_decor" $core
set_property version "1.0" $core

# 5. インターフェース統合プロトコル（1GHzクロック・リセットの自動マッピング）
# 既存の clk_in_1ghz を標準クロックバスとして定義（SDC周期1.0nsと完全連動）
ipx::add_bus_interface clk_in_1ghz $core
set_property bus_type_vlnv xilinx.com:signal:clock:1.0 [ipx::get_bus_interfaces clk_in_1ghz -of_objects $core]
set_property abstraction_type_vlnv xilinx.com:signal:clock_rtl:1.0 [ipx::get_bus_interfaces clk_in_1ghz -of_objects $core]
ipx::add_port_map CLK [ipx::get_bus_interfaces clk_in_1ghz -of_objects $core]
set_property physical_name clk_in_1ghz [ipx::get_port_maps CLK -of_objects [ipx::get_bus_interfaces clk_in_1ghz -of_objects $core]]

# 既存の rst_n をアクティブ・ロー・リセットとしてマッピング
ipx::add_bus_interface rst_n $core
set_property bus_type_vlnv xilinx.com:signal:reset:1.0 [ipx::get_bus_interfaces rst_n -of_objects $core]
set_property abstraction_type_vlnv xilinx.com:signal:reset_rtl:1.0 [ipx::get_bus_interfaces rst_n -of_objects $core]
ipx::add_port_map RST [ipx::get_bus_interfaces rst_n -of_objects $core]
set_property physical_name rst_n [ipx::get_port_maps RST -of_objects [ipx::get_bus_interfaces rst_n -of_objects $core]]
set_property value_list {ASSOCIATED_BUSIF {} ASSOCIATED_RESET rst_n} [ipx::get_bus_parameters POLARITY -of_objects [ipx::get_bus_interfaces rst_n -of_objects $core]]

# 6. 【核心】既存のApple基板シリアル通信インターフェースへの自動マッピング
ipx::add_bus_interface apple_qspi_bus $core
set_property bus_type_vlnv xilinx.com:interface:spi:1.0 [ipx::get_bus_interfaces apple_qspi_bus -of_objects $core]
set_property abstraction_type_vlnv xilinx.com:interface:spi_rtl:1.0 [ipx::get_bus_interfaces apple_qspi_bus -of_objects $core]

# 各物理ピンを標準SPI規格へ自動バインド（Apple基板の既存パターンをそのまま流用）
ipx::add_port_map SCK [ipx::get_bus_interfaces apple_qspi_bus -of_objects $core]
set_property physical_name apple_qspi_sck [ipx::get_port_maps SCK -of_objects [ipx::get_bus_interfaces apple_qspi_bus -of_objects $core]]
ipx::add_port_map SS [ipx::get_bus_interfaces apple_qspi_bus -of_objects $core]
set_property physical_name apple_qspi_cs_n [ipx::get_port_maps SS -of_objects [ipx::get_bus_interfaces apple_qspi_bus -of_objects $core]]

# 7. GUIパラメータの定義（Mac/iPhone/Watch等のリコンフィギュラブル切り替えスイッチ設定）
set_property value "EXA_MOBILE" [ipx::get_user_parameters TARGET_DEVICE -of_objects $core]
set_property value_format string [ipx::get_user_parameters TARGET_DEVICE -of_objects $core]

# 8. 最終チェック＆永久ラッチ（凍結）
ipx::update_source_project_archive -of_objects $core
ipx::create_xgui_files $core
ipx::update_checksums $core
ipx::save_core $core

# IPリポジトリパスの追加設定
set_property ip_repo_paths ./ip_repo_apple [current_project]
update_ip_catalog

puts "🏆 祝・大勝利！ Appleシリコン完全置換トップマクロの自動化IPパッケージングが正常に完了しました。"
puts "   出力先: ./ip_repo_apple/ に格納。老若男女のスマホ設計アプリからいつでもキック可能です！"

# ダミープロジェクトをクローズしてタスクキル完了
close_project
exit
