/*
 * Copyright 2015 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: AMD
 *
 */

/**
 * Bandwidth and Watermark calculations interface.
 * (Refer to "DCE11_mode_support.xlsm" from Perforce.)
 */
#ifndef __BANDWIDTH_CALCS_H__
#define __BANDWIDTH_CALCS_H__

#include "bw_fixed.h"
/*******************************************************************************
 * There are three types of input into Calculations:
 * 1. per-DCE static values - these are "hardcoded" properties of the DCEIP
 * 2. board-level values - these are generally coming from VBIOS parser
 * 3. mode/configuration values - depending Mode, Scaling number of Displays etc.
 ******************************************************************************/

enum bw_stereo_mode {
	mono,
	side_by_side,
	top_bottom
};

enum bw_ul_mode {
	ul_none,
	ul_only,
	ul_blend
};

enum bw_tiling_mode {
	linear,
	tiled
};

enum bw_panning_and_bezel_adj {
	none,
	any_lines
};

enum bw_underlay_surface_type {
	yuv_420,
	yuv_422
};

struct bw_calcs_input_dceip {
	struct bw_fixed dmif_request_buffer_size;
	struct bw_fixed de_tiling_buffer;
	struct bw_fixed dcfclk_request_generation;
	struct bw_fixed lines_interleaved_into_lb;
	struct bw_fixed chunk_width;
	struct bw_fixed number_of_graphics_pipes;
	struct bw_fixed number_of_underlay_pipes;
	bool display_write_back_supported;
	bool argb_compression_support;
	struct bw_fixed underlay_vscaler_efficiency6_bit_per_component;
	struct bw_fixed underlay_vscaler_efficiency8_bit_per_component;
	struct bw_fixed underlay_vscaler_efficiency10_bit_per_component;
	struct bw_fixed underlay_vscaler_efficiency12_bit_per_component;
	struct bw_fixed graphics_vscaler_efficiency6_bit_per_component;
	struct bw_fixed graphics_vscaler_efficiency8_bit_per_component;
	struct bw_fixed graphics_vscaler_efficiency10_bit_per_component;
	struct bw_fixed graphics_vscaler_efficiency12_bit_per_component;
	struct bw_fixed alpha_vscaler_efficiency;
	struct bw_fixed max_dmif_buffer_allocated;
	struct bw_fixed graphics_dmif_size;
	struct bw_fixed underlay_luma_dmif_size;
	struct bw_fixed underlay_chroma_dmif_size;
	bool pre_downscaler_enabled;
	bool underlay_downscale_prefetch_enabled;
	struct bw_fixed lb_write_pixels_per_dispclk;
	struct bw_fixed lb_size_per_component444;
	bool graphics_lb_nodownscaling_multi_line_prefetching;
	struct bw_fixed stutter_and_dram_clock_state_change_gated_before_cursor;
	struct bw_fixed underlay420_luma_lb_size_per_component;
	struct bw_fixed underlay420_chroma_lb_size_per_component;
	struct bw_fixed underlay422_lb_size_per_component;
	struct bw_fixed cursor_chunk_width;
	struct bw_fixed cursor_dcp_buffer_lines;
	struct bw_fixed cursor_memory_interface_buffer_pixels;
	struct bw_fixed underlay_maximum_width_efficient_for_tiling;
	struct bw_fixed underlay_maximum_height_efficient_for_tiling;
	struct bw_fixed peak_pte_request_to_eviction_ratio_limiting_multiple_displays_or_single_rotated_display;
	struct bw_fixed peak_pte_request_to_eviction_ratio_limiting_single_display_no_rotation;
	struct bw_fixed minimum_outstanding_pte_request_limit;
	struct bw_fixed maximum_total_outstanding_pte_requests_allowed_by_saw;
	bool limit_excessive_outstanding_dmif_requests;
	struct bw_fixed linear_mode_line_request_alternation_slice;
	struct bw_fixed scatter_gather_lines_of_pte_prefetching_in_linear_mode;
	struct bw_fixed display_write_back420_luma_mcifwr_buffer_size;
	struct bw_fixed display_write_back420_chroma_mcifwr_buffer_size;
	struct bw_fixed request_efficiency;
	struct bw_fixed dispclk_per_request;
	struct bw_fixed dispclk_ramping_factor;
	struct bw_fixed display_pipe_throughput_factor;
	struct bw_fixed scatter_gather_pte_request_rows_in_tiling_mode;
	struct bw_fixed mcifwr_all_surfaces_burst_time; /* 0 todo: this is a bug*/
};

struct bw_calcs_input_vbios {
	struct bw_fixed dram_channel_width_in_bits;
	struct bw_fixed number_of_dram_channels;
	struct bw_fixed number_of_dram_banks;
	struct bw_fixed high_yclk_mhz;
	struct bw_fixed high_dram_bandwidth_per_channel;
	struct bw_fixed low_yclk_mhz;
	struct bw_fixed low_dram_bandwidth_per_channel;
	struct bw_fixed low_sclk_mhz;
	struct bw_fixed mid_sclk_mhz;
	struct bw_fixed high_sclk_mhz;
	struct bw_fixed low_voltage_max_dispclk_mhz;
	struct bw_fixed mid_voltage_max_dispclk_mhz;
	struct bw_fixed high_voltage_max_dispclk_mhz;
	struct bw_fixed data_return_bus_width;
	struct bw_fixed trc;
	struct bw_fixed dmifmc_urgent_latency;
	struct bw_fixed stutter_self_refresh_exit_latency;
	struct bw_fixed nbp_state_change_latency;
	struct bw_fixed mcifwrmc_urgent_latency;
	bool scatter_gather_enable;
	struct bw_fixed down_spread_percentage;
	struct bw_fixed cursor_width;
	struct bw_fixed average_compression_rate;
	struct bw_fixed number_of_request_slots_gmc_reserves_for_dmif_per_channel;
	struct bw_fixed blackout_duration;
	struct bw_fixed maximum_blackout_recovery_time;
};

struct bw_calcs_input_mode_data_internal {
	/* data for all displays */
	uint32_t number_of_displays;
	struct bw_fixed graphics_rotation_angle;
	struct bw_fixed underlay_rotation_angle;
	bool display_synchronization_enabled;
	enum bw_underlay_surface_type underlay_surface_type;
	enum bw_panning_and_bezel_adj panning_and_bezel_adjustment;
	enum bw_tiling_mode graphics_tiling_mode;
	bool graphics_interlace_mode;
	struct bw_fixed graphics_bytes_per_pixel;
	struct bw_fixed graphics_htaps;
	struct bw_fixed graphics_vtaps;
	struct bw_fixed graphics_lb_bpc;
	struct bw_fixed underlay_lb_bpc;
	enum bw_tiling_mode underlay_tiling_mode;
	struct bw_fixed underlay_htaps;
	struct bw_fixed underlay_vtaps;
	struct bw_fixed underlay_src_width;
	struct bw_fixed underlay_src_height;
	struct bw_fixed underlay_pitch_in_pixels;
	enum bw_stereo_mode underlay_stereo_mode;
	bool d0_fbc_enable;
	bool d0_lpt_enable;
	struct bw_fixed d0_htotal;
	struct bw_fixed d0_pixel_rate;
	struct bw_fixed d0_graphics_src_width;
	struct bw_fixed d0_graphics_src_height;
	struct bw_fixed d0_graphics_scale_ratio;
	enum bw_stereo_mode d0_graphics_stereo_mode;
	enum bw_ul_mode d0_underlay_mode;
	struct bw_fixed d0_underlay_scale_ratio;
	struct bw_fixed d1_htotal;
	struct bw_fixed d1_pixel_rate;
	struct bw_fixed d1_graphics_src_width;
	struct bw_fixed d1_graphics_src_height;
	struct bw_fixed d1_graphics_scale_ratio;
	enum bw_stereo_mode d1_graphics_stereo_mode;
	bool d1_display_write_back_dwb_enable;
	enum bw_ul_mode d1_underlay_mode;
	struct bw_fixed d1_underlay_scale_ratio;
	struct bw_fixed d2_htotal;
	struct bw_fixed d2_pixel_rate;
	struct bw_fixed d2_graphics_src_width;
	struct bw_fixed d2_graphics_src_height;
	struct bw_fixed d2_graphics_scale_ratio;
	enum bw_stereo_mode d2_graphics_stereo_mode;
};

struct bw_calcs_input_single_display {
	uint32_t graphics_rotation_angle;
	uint32_t underlay_rotation_angle;
	enum bw_underlay_surface_type underlay_surface_type;
	enum bw_panning_and_bezel_adj panning_and_bezel_adjustment;
	uint32_t graphics_bytes_per_pixel;
	bool graphics_interlace_mode;
	enum bw_tiling_mode graphics_tiling_mode;
	uint32_t graphics_h_taps;
	uint32_t graphics_v_taps;
	uint32_t graphics_lb_bpc;
	uint32_t underlay_lb_bpc;
	enum bw_tiling_mode underlay_tiling_mode;
	uint32_t underlay_h_taps;
	uint32_t underlay_v_taps;
	uint32_t underlay_src_width;
	uint32_t underlay_src_height;
	uint32_t underlay_pitch_in_pixels;
	enum bw_stereo_mode underlay_stereo_mode;
	bool fbc_enable;
	bool lpt_enable;
	uint32_t h_total;
	struct bw_fixed pixel_rate;
	uint32_t graphics_src_width;
	uint32_t graphics_src_height;
	struct bw_fixed graphics_scale_ratio;
	enum bw_stereo_mode graphics_stereo_mode;
	enum bw_ul_mode underlay_mode;
};

#define BW_CALCS_MAX_NUM_DISPLAYS 3

struct bw_calcs_input_mode_data {
	/* data for all displays */
	uint8_t number_of_displays;
	bool display_synchronization_enabled;

	struct bw_calcs_input_single_display
				displays_data[BW_CALCS_MAX_NUM_DISPLAYS];
};

/*******************************************************************************
 * Output data structure(s).
 ******************************************************************************/
#define maximum_number_of_surfaces 12
/*Units : MHz, us */
struct bw_results_internal {
	bool cpup_state_change_enable;
	bool cpuc_state_change_enable;
	bool nbp_state_change_enable;
	bool stutter_mode_enable;
	struct bw_fixed number_of_underlay_surfaces;
	struct bw_fixed src_width_after_surface_type;
	struct bw_fixed src_height_after_surface_type;
	struct bw_fixed hsr_after_surface_type;
	struct bw_fixed vsr_after_surface_type;
	struct bw_fixed src_width_after_rotation;
	struct bw_fixed src_height_after_rotation;
	struct bw_fixed hsr_after_rotation;
	struct bw_fixed vsr_after_rotation;
	struct bw_fixed source_height_pixels;
	struct bw_fixed hsr_after_stereo;
	struct bw_fixed vsr_after_stereo;
	struct bw_fixed source_width_in_lb;
	struct bw_fixed lb_line_pitch;
	struct bw_fixed underlay_maximum_source_efficient_for_tiling;
	struct bw_fixed num_lines_at_frame_start;
	struct bw_fixed min_dmif_size_in_time;
	struct bw_fixed min_mcifwr_size_in_time;
	struct bw_fixed total_requests_for_dmif_size;
	struct bw_fixed peak_pte_request_to_eviction_ratio_limiting;
	struct bw_fixed useful_pte_per_pte_request;
	struct bw_fixed scatter_gather_pte_request_rows;
	struct bw_fixed scatter_gather_row_height;
	struct bw_fixed scatter_gather_pte_requests_in_vblank;
	struct bw_fixed inefficient_linear_pitch_in_bytes;
	struct bw_fixed inefficient_underlay_pitch_in_pixels;
	struct bw_fixed minimum_underlay_pitch_padding_recommended_for_efficiency;
	struct bw_fixed cursor_total_data;
	struct bw_fixed cursor_total_request_groups;
	struct bw_fixed scatter_gather_total_pte_requests;
	struct bw_fixed scatter_gather_total_pte_request_groups;
	struct bw_fixed tile_width_in_pixels;
	struct bw_fixed dmif_total_number_of_data_request_page_close_open;
	struct bw_fixed mcifwr_total_number_of_data_request_page_close_open;
	struct bw_fixed bytes_per_page_close_open;
	struct bw_fixed mcifwr_total_page_close_open_time;
	struct bw_fixed total_requests_for_adjusted_dmif_size;
	struct bw_fixed total_dmifmc_urgent_trips;
	struct bw_fixed total_dmifmc_urgent_latency;
	struct bw_fixed total_display_reads_required_data;
	struct bw_fixed total_display_reads_required_dram_access_data;
	struct bw_fixed total_display_writes_required_data;
	struct bw_fixed total_display_writes_required_dram_access_data;
	struct bw_fixed display_reads_required_data;
	struct bw_fixed display_reads_required_dram_access_data;
	struct bw_fixed dmif_total_page_close_open_time;
	struct bw_fixed min_cursor_memory_interface_buffer_size_in_time;
	struct bw_fixed min_read_buffer_size_in_time;
	struct bw_fixed display_reads_time_for_data_transfer;
	struct bw_fixed display_writes_time_for_data_transfer;
	struct bw_fixed dmif_required_dram_bandwidth;
	struct bw_fixed mcifwr_required_dram_bandwidth;
	struct bw_fixed required_dmifmc_urgent_latency_for_page_close_open;
	struct bw_fixed required_mcifmcwr_urgent_latency;
	struct bw_fixed required_dram_bandwidth_gbyte_per_second;
	struct bw_fixed dram_bandwidth;
	struct bw_fixed dmif_required_sclk;
	struct bw_fixed mcifwr_required_sclk;
	struct bw_fixed required_sclk;
	struct bw_fixed downspread_factor;
	struct bw_fixed v_scaler_efficiency;
	struct bw_fixed scaler_limits_factor;
	struct bw_fixed display_pipe_pixel_throughput;
	struct bw_fixed total_dispclk_required_with_ramping;
	struct bw_fixed total_dispclk_required_without_ramping;
	struct bw_fixed total_read_request_bandwidth;
	struct bw_fixed total_write_request_bandwidth;
	struct bw_fixed dispclk_required_for_total_read_request_bandwidth;
	struct bw_fixed total_dispclk_required_with_ramping_with_request_bandwidth;
	struct bw_fixed total_dispclk_required_without_ramping_with_request_bandwidth;
	struct bw_fixed dispclk;
	struct bw_fixed blackout_recovery_time;
	struct bw_fixed min_pixels_per_data_fifo_entry;
	struct bw_fixed sclk_deep_sleep;
	struct bw_fixed chunk_request_time;
	struct bw_fixed cursor_request_time;
	struct bw_fixed line_source_pixels_transfer_time;
	struct bw_fixed dmifdram_access_efficiency;
	struct bw_fixed mcifwrdram_access_efficiency;
	struct bw_fixed total_average_bandwidth_no_compression;
	struct bw_fixed total_average_bandwidth;
	struct bw_fixed total_stutter_cycle_duration;
	struct bw_fixed stutter_burst_time;
	struct bw_fixed time_in_self_refresh;
	struct bw_fixed stutter_efficiency;
	struct bw_fixed worst_number_of_trips_to_memory;
	struct bw_fixed immediate_flip_time;
	struct bw_fixed latency_for_non_dmif_clients;
	struct bw_fixed latency_for_non_mcifwr_clients;
	struct bw_fixed dmifmc_urgent_latency_supported_in_high_sclk_and_yclk;
	struct bw_fixed nbp_state_dram_speed_change_margin;
	struct bw_fixed display_reads_time_for_data_transfer_and_urgent_latency;
	bool use_alpha[maximum_number_of_surfaces];
	bool orthogonal_rotation[maximum_number_of_surfaces];
	bool enable[maximum_number_of_surfaces];
	bool access_one_channel_only[maximum_number_of_surfaces];
	bool scatter_gather_enable_for_pipe[maximum_number_of_surfaces];
	bool interlace_mode[maximum_number_of_surfaces];
	struct bw_fixed bytes_per_pixel[maximum_number_of_surfaces];
	struct bw_fixed h_total[maximum_number_of_surfaces];
	struct bw_fixed pixel_rate[maximum_number_of_surfaces];
	struct bw_fixed src_width[maximum_number_of_surfaces];
	struct bw_fixed pitch_in_pixels[maximum_number_of_surfaces];
	struct bw_fixed pitch_in_pixels_after_surface_type[maximum_number_of_surfaces];
	struct bw_fixed src_height[maximum_number_of_surfaces];
	struct bw_fixed scale_ratio[maximum_number_of_surfaces];
	struct bw_fixed h_taps[maximum_number_of_surfaces];
	struct bw_fixed v_taps[maximum_number_of_surfaces];
	struct bw_fixed rotation_angle[maximum_number_of_surfaces];
	struct bw_fixed lb_bpc[maximum_number_of_surfaces];
	struct bw_fixed compression_rate[maximum_number_of_surfaces];
	struct bw_fixed hsr[maximum_number_of_surfaces];
	struct bw_fixed vsr[maximum_number_of_surfaces];
	struct bw_fixed source_width_rounded_up_to_chunks[maximum_number_of_surfaces];
	struct bw_fixed source_width_pixels[maximum_number_of_surfaces];
	struct bw_fixed source_height_rounded_up_to_chunks[maximum_number_of_surfaces];
	struct bw_fixed display_bandwidth[maximum_number_of_surfaces];
	struct bw_fixed request_bandwidth[maximum_number_of_surfaces];
	struct bw_fixed bytes_per_request[maximum_number_of_surfaces];
	struct bw_fixed useful_bytes_per_request[maximum_number_of_surfaces];
	struct bw_fixed lines_interleaved_in_mem_access[maximum_number_of_surfaces];
	struct bw_fixed latency_hiding_lines[maximum_number_of_surfaces];
	struct bw_fixed lb_partitions[maximum_number_of_surfaces];
	struct bw_fixed lb_partitions_max[maximum_number_of_surfaces];
	struct bw_fixed dispclk_required_with_ramping[maximum_number_of_surfaces];
	struct bw_fixed dispclk_required_without_ramping[maximum_number_of_surfaces];
	struct bw_fixed data_buffer_size[maximum_number_of_surfaces];
	struct bw_fixed outstanding_chunk_request_limit[maximum_number_of_surfaces];
	struct bw_fixed urgent_watermark[maximum_number_of_surfaces];
	struct bw_fixed stutter_exit_watermark[maximum_number_of_surfaces];
	struct bw_fixed nbp_state_change_watermark[maximum_number_of_surfaces];
	struct bw_fixed v_filter_init[maximum_number_of_surfaces];
	struct bw_fixed stutter_cycle_duration[maximum_number_of_surfaces];
	struct bw_fixed average_bandwidth[maximum_number_of_surfaces];
	struct bw_fixed average_bandwidth_no_compression[maximum_number_of_surfaces];
	struct bw_fixed scatter_gather_pte_request_limit[maximum_number_of_surfaces];
	struct bw_fixed lb_size_per_component[maximum_number_of_surfaces];
	struct bw_fixed memory_chunk_size_in_bytes[maximum_number_of_surfaces];
	struct bw_fixed pipe_chunk_size_in_bytes[maximum_number_of_surfaces];
	struct bw_fixed number_of_trips_to_memory_for_getting_apte_row[maximum_number_of_surfaces];
	struct bw_fixed adjusted_data_buffer_size[maximum_number_of_surfaces];
	struct bw_fixed adjusted_data_buffer_size_in_memory[maximum_number_of_surfaces];
	struct bw_fixed pixels_per_data_fifo_entry[maximum_number_of_surfaces];
	struct bw_fixed scatter_gather_pte_requests_in_row[maximum_number_of_surfaces];
	struct bw_fixed pte_request_per_chunk[maximum_number_of_surfaces];
	struct bw_fixed scatter_gather_page_width[maximum_number_of_surfaces];
	struct bw_fixed scatter_gather_page_height[maximum_number_of_surfaces];
	struct bw_fixed lb_lines_in_per_line_out_in_beginning_of_frame[maximum_number_of_surfaces];
	struct bw_fixed lb_lines_in_per_line_out_in_middle_of_frame[maximum_number_of_surfaces];
	struct bw_fixed cursor_width_pixels[maximum_number_of_surfaces];
	struct bw_fixed line_buffer_prefetch[maximum_number_of_surfaces];
	struct bw_fixed minimum_latency_hiding[maximum_number_of_surfaces];
	struct bw_fixed maximum_latency_hiding[maximum_number_of_surfaces];
	struct bw_fixed minimum_latency_hiding_with_cursor[maximum_number_of_surfaces];
	struct bw_fixed maximum_latency_hiding_with_cursor[maximum_number_of_surfaces];
	struct bw_fixed src_pixels_for_first_output_pixel[maximum_number_of_surfaces];
	struct bw_fixed src_pixels_for_last_output_pixel[maximum_number_of_surfaces];
	struct bw_fixed src_data_for_first_output_pixel[maximum_number_of_surfaces];
	struct bw_fixed src_data_for_last_output_pixel[maximum_number_of_surfaces];
	struct bw_fixed active_time[maximum_number_of_surfaces];
	struct bw_fixed horizontal_blank_and_chunk_granularity_factor[maximum_number_of_surfaces];
	struct bw_fixed cursor_latency_hiding[maximum_number_of_surfaces];
	struct bw_fixed dmif_burst_time[3][3];
	struct bw_fixed mcifwr_burst_time[3][3];
	struct bw_fixed line_source_transfer_time[maximum_number_of_surfaces][3][3];
	struct bw_fixed dram_speed_change_margin[3][3];
	struct bw_fixed dispclk_required_for_dram_speed_change[3][3];
	struct bw_fixed blackout_duration_margin[3][3];
	struct bw_fixed dispclk_required_for_blackout_duration[3][3];
	struct bw_fixed dispclk_required_for_blackout_recovery[3][3];
	struct bw_fixed dmif_required_sclk_for_urgent_latency[6];
};

struct bw_watermarks {
	uint32_t a_mark;
	uint32_t b_mark;
};

struct bw_calcs_output {
	bool cpuc_state_change_enable;
	bool cpup_state_change_enable;
	bool stutter_mode_enable;
	bool nbp_state_change_enable;
	struct bw_watermarks urgent_wm_ns[4];
	struct bw_watermarks stutter_exit_wm_ns[4];
	struct bw_watermarks nbp_state_change_wm_ns[4];
	uint32_t required_sclk;
	uint32_t required_sclk_deep_sleep;
	uint32_t required_yclk;
	uint32_t dispclk_khz;
	uint32_t required_blackout_duration_us;
};


/**
 * Initialize structures with data which will NOT change at runtime.
 */
void bw_calcs_init(
	struct bw_calcs_input_dceip *bw_dceip,
	struct bw_calcs_input_vbios *bw_vbios);

/**
 * Return:
 *	true -	Display(s) configuration supported.
 *		In this case 'calcs_output' contains data for HW programming
 *	false - Display(s) configuration not supported (not enough bandwidth).
 */
bool bw_calcs(
	struct dc_context *ctx,
	const struct bw_calcs_input_dceip *dceip,
	const struct bw_calcs_input_vbios *vbios,
	const struct bw_calcs_input_mode_data *mode_data,
	struct bw_calcs_output *calcs_output);


#endif /* __BANDWIDTH_CALCS_H__ */

