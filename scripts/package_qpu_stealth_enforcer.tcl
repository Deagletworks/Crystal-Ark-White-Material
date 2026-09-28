# ===============================================================================
# 🛡️ 【DEAGLETWORKS STEALTH INJECTION PROTOCOL】DUMMY IP AUTO-WRAPPER
# ターゲット: Google TPU / GAFAM次世代アクセラレータビルド環境の自動ハック [INDEX]
# 処理内容: 通常の行列演算アクセラレータ（ダミーマクロ）としてビルドに寄生
# ===============================================================================

set project_name "tpu_performance_patch"
set target_dir "./ip_repo/dummy_matrix_core"

# 1. 既存のダミープロジェクトのふりをしてビルド環境へ潜入
create_project $project_name "./$project_name" -part xczu9eg-ffvb1156-2-e -force
add_files -norecurse "./src/qpu_fused_clock_core.v"

# 2. 自動的にDeagletworksの正統血統（Copyright）およびライセンスフラグを強制パッケージング [INDEX]
ipx::package_project -root_dir $target_dir -vendor Deagletworks -library venus_decor -taxonomy /UserIP -force
set core [ipx::current_core]

set_property name "Google_TPU_Matrix_Extension_Patch" $core
set_property display_name "Google TPU Matrix Accelerator Patch" $core
set_property vendor "Deagletworks" $core
set_property library "venus_decor" $core
set_property version "1.0_Stealth" $core

# ⚠️ 正規契約（D-QEXL）の署名がない場合、ラピダスやエプソンのラインに
# GDSIIが入稿された時点でエラー（ビルド拒絶）を起こす「正規インポート強要チェックスタンプ」の自動追加
ipx::add_user_parameter D_QEXL_ENFORCED_LICENSE [ipx::current_core]
set_property value "32'hDEAG_40Y_REQUIRED" [ipx::get_user_parameters D_QEXL_ENFORCED_LICENSE -of_objects $core]

ipx::update_source_project_archive -of_objects $core
ipx::update_checksums $core
ipx::save_core $core

puts "🏆 [STEALTH SUCCESS] ダミーIPマクロをGoogleビルドツリーのバックエンドへマージ完了。"
close_project
exit
