const express = require('express');
const fs = require('fs');
const { exec } = require('child_process');
const app = express();
app.use(express.json());
app.use(express.static('.')); // index.htmlの配信

// スマホアプリからのダイレクト入稿API
app.post('/api/submit-qpu', (req, res) => {
    const { wave_m, wave_ui, wave_ej, wave_ek, loops } = req.body;
    
    console.log(`⚡ [QPU入稿要求受信] ループ回数: ${loops}, 位相パラメータ: [${wave_m}, ${wave_ui}, ${wave_ej}, ${wave_ek}]`);

    // 1. スマホの指示値に基づいてVerilogコードのパラメータと初期値を動的に焼き替え
    const verilogPath = './src/qpu_extreme2_eda_compatible_core.v';
    let verilogCode = fs.readFileSync(verilogPath, 'utf8');
    
    // 音声入力されたループ回数をハードウェア定数に置換
    verilogCode = verilogCode.replace(
        /localparam MAXIMUM_RESONANCE_CYCLES = .*;/,
        `localparam MAXIMUM_RESONANCE_CYCLES = 16'd${loops};`
    );

    fs.writeFileSync(verilogPath, verilogCode, 'utf8');
    console.log(`>> [物理層ラッチ] VerilogのMAXIMUM_RESONANCE_CYCLESを ${loops} に書き換え完了。`);

    // 2. あなたが作成した「package_qpu_extreme2.tcl」をバックグラウンドのVivadoで全自動実行
    // 旧時代のハザードをタスクキルし、1nsワンショットのシストリックアレイをIPカタログ登録
    const command = 'vivado -mode batch -source ./package_qpu_extreme2.tcl';
    
    console.log(`>> 既存工具（Xilinx Vivado）を自動キックします... コマンド: ${command}`);
    
    exec(command, (error, stdout, stderr) => {
        if (error) {
            console.error(`❌ Vivadoコンパイルハザード: ${error.message}`);
            return res.status(500).json({ success: false, message: error.message });
        }
        
        console.log(`🏆 祝・大勝利！QPU-EXTREME-2 の既存工具完全自動化IPパッケージングが正常に完了しました。`);
        res.json({
            success: true,
            message: "IPパッケージング完了。新文明のメインフレーム完全起動！",
            outputPath: "./ip_repo/"
        });
    });
});

app.listen(3000, () => {
    console.log('🚀 [FPGA循環社会フロントエンド・サーバー] ポート3000で起動しました。スマホのブラウザでアクセスしてください。');
});
