`timescale 1ns / 1ps

module saga20_expanded_transformer_tb;

    parameter DATA_WIDTH = 32;
    parameter CHANNELS   = 16;
    parameter GRID_SIZE  = 16;

    reg                               sys_clk;
    reg                               sys_rst_n;
    reg                               hazard_override;
    reg [15:0]                        ferroelectric_bias_in;
    reg [CHANNELS*DATA_WIDTH-1:0]     query_stream_in;
    reg [CHANNELS*DATA_WIDTH-1:0]     key_stream_in;
    reg [CHANNELS*DATA_WIDTH-1:0]     value_stream_in;
    
    wire [CHANNELS*DATA_WIDTH-1:0]    attention_matrix_out;
    wire [15:0]                       optical_phase_lock_metric;
    wire [7:0]                        phase_alignment_status [CHANNELS-1:0];
    wire [63:0]                       ai_throughput_counter;
    wire                              system_locked;
    wire                              ransomware_detected_flag;

    saga20_expanded_transformer_core #(
        .DATA_WIDTH(DATA_WIDTH),
        .CHANNELS(CHANNELS),
        .GRID_SIZE(GRID_SIZE)
    ) uut (
        .sys_clk(sys_clk),
        .sys_rst_n(sys_rst_n),
        .hazard_override(hazard_override),
        .ferroelectric_bias_in(ferroelectric_bias_in),
        .query_stream_in(query_stream_in),
        .key_stream_in(key_stream_in),
        .value_stream_in(value_stream_in),
        .attention_matrix_out(attention_matrix_out),
        .optical_phase_lock_metric(optical_phase_lock_metric),
        .phase_alignment_status(phase_alignment_status),
        .ai_throughput_counter(ai_throughput_counter),
        .system_locked(system_locked),
        .ransomware_detected_flag(ransomware_detected_flag)
    );

    always #0.5 sys_clk = ~sys_clk;

    initial begin
        sys_clk = 0;
        sys_rst_n = 0;
        hazard_override = 0;
        ferroelectric_bias_in = 16'h01F4;
        query_stream_in = {CHANNELS{32'hA5A55A5A}};
        key_stream_in   = {CHANNELS{32'h5A5A99FF}};
        value_stream_in = {CHANNELS{32'h12345678}};

        #10;
        sys_rst_n = 1;

        #40;
        ferroelectric_bias_in = 16'h03E8; 

        #40;
        query_stream_in[31:0] = 32'hFFFFFFFF;
        key_stream_in[31:0]   = 32'h00000000;

        #80;
        query_stream_in[31:0] = 32'hA5A55A5A;
        key_stream_in[31:0]   = 32'h5A5A99FF;

        #100;
        $finish;
    end

    initial begin
        $dumpfile("saga20_ransomware_defense_waveform.vcd");
        $dumpvars(0, saga20_expanded_transformer_tb);
    end

endmodule
