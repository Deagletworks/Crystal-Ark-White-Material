# 🛡️ DEAGLETWORKS MULTI-CORNER PROCESS-VOLTAGE-TEMPERATURE (PVT) STIMULUS
# ターゲット: TSV立体積層・常温FeRAM格子安定化用タイミング制約 ( worst, typ, best )

# 🌟 BEST条件 (Fast Corner): 常温(25℃)・電圧高(1.05V)・常温FeRAM高速反転状態
set_operating_conditions -STEREOSCOPIC_BEST -library crystal_power_best \
    -max_proc 0.85 -max_volt 1.05 -max_temp 25.0
set_clock_uncertainty 0.010 [get_clocks sys_clk]

# 🌟 TYP条件 (Typical Corner): 常温(50℃)・標準電圧(0.95V)・自律熱管理マルチフェロイック動的平衡
set_operating_conditions -STEREOSCOPIC_TYP -library crystal_power_typ \
    -max_proc 1.00 -max_volt 0.95 -max_temp 50.0
set_clock_uncertainty 0.030 [get_clocks sys_clk]

# 🌟 WORST条件 (Slow Corner): 高温(125℃)・低電圧(0.85V)・TSVひずみ極大時
set_operating_conditions -STEREOSCOPIC_WORST -library crystal_power_worst \
    -max_proc 1.25 -max_volt 0.85 -max_temp 125.0
set_clock_uncertainty 0.050 [get_clocks sys_clk]

# 物理配置（P&R）への強制命令：遅延ハザードの自動差分解消
set_max_fanout 2 [current_design]
set_max_transition 0.040 [current_design]
