/*
 * Copyright 2016 Advanced Micro Devices, Inc.
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

#include "mem_input.h"
#include "reg_helper.h"
#include "basics/conversion.h"

#define CTX \
	mi->ctx
#define REG(reg)\
	mi->regs->reg

#undef FN
#define FN(reg_name, field_name) \
	mi->shifts->field_name, mi->masks->field_name

struct pte_setting {
	unsigned int bpp;
	unsigned int page_width;
	unsigned int page_height;
	unsigned char min_pte_before_flip_horiz_scan;
	unsigned char min_pte_before_flip_vert_scan;
	unsigned char pte_req_per_chunk;
	unsigned char param_6;
	unsigned char param_7;
	unsigned char param_8;
};

enum mi_bits_per_pixel {
	mi_bpp_8 = 0,
	mi_bpp_16,
	mi_bpp_32,
	mi_bpp_64,
	mi_bpp_count,
};

enum mi_tiling_format {
	mi_tiling_linear = 0,
	mi_tiling_1D,
	mi_tiling_2D,
	mi_tiling_count,
};

static const struct pte_setting pte_settings[mi_tiling_count][mi_bpp_count] = {
	[mi_tiling_linear] = {
		{  8, 4096, 1, 8, 0, 1, 0, 0, 0},
		{ 16, 2048, 1, 8, 0, 1, 0, 0, 0},
		{ 32, 1024, 1, 8, 0, 1, 0, 0, 0},
		{ 64,  512, 1, 8, 0, 1, 0, 0, 0}, /* new for 64bpp from HW */
	},
	[mi_tiling_1D] = {
		{  8, 512, 8, 1, 0, 1, 0, 0, 0},  /* 0 for invalid */
		{ 16, 256, 8, 2, 0, 1, 0, 0, 0},
		{ 32, 128, 8, 4, 0, 1, 0, 0, 0},
		{ 64,  64, 8, 4, 0, 1, 0, 0, 0}, /* fake */
	},
	[mi_tiling_2D] = {
		{  8, 64, 64,  8,  8, 1, 4, 0, 0},
		{ 16, 64, 32,  8, 16, 1, 8, 0, 0},
		{ 32, 32, 32, 16, 16, 1, 8, 0, 0},
		{ 64,  8, 32, 16, 16, 1, 8, 0, 0}, /* fake */
	},
};

static enum mi_bits_per_pixel get_mi_bpp(
		enum surface_pixel_format format)
{
	if (format >= SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616)
		return mi_bpp_64;
	else if (format >= SURFACE_PIXEL_FORMAT_GRPH_ARGB8888)
		return mi_bpp_32;
	else if (format >= SURFACE_PIXEL_FORMAT_GRPH_ARGB1555)
		return mi_bpp_16;
	else
		return mi_bpp_8;
}

static enum mi_tiling_format get_mi_tiling(
		union dc_tiling_info *tiling_info)
{
	switch (tiling_info->gfx8.array_mode) {
	case DC_ARRAY_1D_TILED_THIN1:
	case DC_ARRAY_1D_TILED_THICK:
	case DC_ARRAY_PRT_TILED_THIN1:
		return mi_tiling_1D;
	case DC_ARRAY_2D_TILED_THIN1:
	case DC_ARRAY_2D_TILED_THICK:
	case DC_ARRAY_2D_TILED_X_THICK:
	case DC_ARRAY_PRT_2D_TILED_THIN1:
	case DC_ARRAY_PRT_2D_TILED_THICK:
		return mi_tiling_2D;
	case DC_ARRAY_LINEAR_GENERAL:
	case DC_ARRAY_LINEAR_ALLIGNED:
		return mi_tiling_linear;
	default:
		return mi_tiling_2D;
	}
}

static bool is_vert_scan(enum dc_rotation_angle rotation)
{
	switch (rotation) {
	case ROTATION_ANGLE_90:
	case ROTATION_ANGLE_270:
		return true;
	default:
		return false;
	}
}

void dce_mem_input_program_pte_vm(struct mem_input *mi,
		enum surface_pixel_format format,
		union dc_tiling_info *tiling_info,
		enum dc_rotation_angle rotation)
{
	enum mi_bits_per_pixel mi_bpp = get_mi_bpp(format);
	enum mi_tiling_format mi_tiling = get_mi_tiling(tiling_info);
	const struct pte_setting *pte = &pte_settings[mi_tiling][mi_bpp];

	unsigned int page_width = log_2(pte->page_width);
	unsigned int page_height = log_2(pte->page_height);
	unsigned int min_pte_before_flip = is_vert_scan(rotation) ?
			pte->min_pte_before_flip_vert_scan :
			pte->min_pte_before_flip_horiz_scan;

	REG_UPDATE(GRPH_PIPE_OUTSTANDING_REQUEST_LIMIT,
			GRPH_PIPE_OUTSTANDING_REQUEST_LIMIT, 0xff);

	REG_UPDATE_3(DVMM_PTE_CONTROL,
			DVMM_PAGE_WIDTH, page_width,
			DVMM_PAGE_HEIGHT, page_height,
			DVMM_MIN_PTE_BEFORE_FLIP, min_pte_before_flip);

	REG_UPDATE_2(DVMM_PTE_ARB_CONTROL,
			DVMM_PTE_REQ_PER_CHUNK, pte->pte_req_per_chunk,
			DVMM_MAX_PTE_REQ_OUTSTANDING, 0xff);
}

static void program_urgency_watermark(struct mem_input *mi,
	uint32_t wm_select,
	uint32_t urgency_low_wm,
	uint32_t urgency_high_wm)
{
	REG_UPDATE(DPG_WATERMARK_MASK_CONTROL,
		URGENCY_WATERMARK_MASK, wm_select);

	REG_SET_2(DPG_PIPE_URGENCY_CONTROL, 0,
		URGENCY_LOW_WATERMARK, urgency_low_wm,
		URGENCY_HIGH_WATERMARK, urgency_high_wm);
}

static void program_nbp_watermark(struct mem_input *mi,
	uint32_t wm_select,
	uint32_t nbp_wm)
{
	if (REG(DPG_PIPE_NB_PSTATE_CHANGE_CONTROL)) {
		REG_UPDATE(DPG_WATERMARK_MASK_CONTROL,
				NB_PSTATE_CHANGE_WATERMARK_MASK, wm_select);

		REG_UPDATE_3(DPG_PIPE_NB_PSTATE_CHANGE_CONTROL,
				NB_PSTATE_CHANGE_ENABLE, 1,
				NB_PSTATE_CHANGE_URGENT_DURING_REQUEST, 1,
				NB_PSTATE_CHANGE_NOT_SELF_REFRESH_DURING_REQUEST, 1);

		REG_UPDATE(DPG_PIPE_NB_PSTATE_CHANGE_CONTROL,
				NB_PSTATE_CHANGE_WATERMARK, nbp_wm);
	}
}

static void program_stutter_watermark(struct mem_input *mi,
	uint32_t wm_select,
	uint32_t stutter_mark)
{
	REG_UPDATE(DPG_WATERMARK_MASK_CONTROL,
		STUTTER_EXIT_SELF_REFRESH_WATERMARK_MASK, wm_select);

		REG_UPDATE(DPG_PIPE_STUTTER_CONTROL,
				STUTTER_EXIT_SELF_REFRESH_WATERMARK, stutter_mark);
}

void dce_mem_input_program_display_marks(struct mem_input *mi,
	struct bw_watermarks nbp,
	struct bw_watermarks stutter,
	struct bw_watermarks urgent,
	uint32_t total_dest_line_time_ns)
{
	uint32_t stutter_en = mi->ctx->dc->debug.disable_stutter ? 0 : 1;

	program_urgency_watermark(mi, 0, /* set a */
			urgent.a_mark, total_dest_line_time_ns);
	program_urgency_watermark(mi, 1, /* set b */
			urgent.b_mark, total_dest_line_time_ns);
	program_urgency_watermark(mi, 2, /* set c */
			urgent.c_mark, total_dest_line_time_ns);
	program_urgency_watermark(mi, 3, /* set d */
			urgent.d_mark, total_dest_line_time_ns);

	REG_UPDATE_2(DPG_PIPE_STUTTER_CONTROL,
		STUTTER_ENABLE, stutter_en,
		STUTTER_IGNORE_FBC, 1);
	program_nbp_watermark(mi, 0, nbp.a_mark); /* set a */
	program_nbp_watermark(mi, 1, nbp.b_mark); /* set b */
	program_nbp_watermark(mi, 2, nbp.c_mark); /* set c */
	program_nbp_watermark(mi, 3, nbp.d_mark); /* set d */

	program_stutter_watermark(mi, 0, stutter.a_mark); /* set a */
	program_stutter_watermark(mi, 1, stutter.b_mark); /* set b */
	program_stutter_watermark(mi, 2, stutter.c_mark); /* set c */
	program_stutter_watermark(mi, 3, stutter.d_mark); /* set d */
}

static void program_tiling(struct mem_input *mi,
	const union dc_tiling_info *info)
{
	if (mi->masks->GRPH_ARRAY_MODE) { /* GFX8 */
		REG_UPDATE_9(GRPH_CONTROL,
				GRPH_NUM_BANKS, info->gfx8.num_banks,
				GRPH_BANK_WIDTH, info->gfx8.bank_width,
				GRPH_BANK_HEIGHT, info->gfx8.bank_height,
				GRPH_MACRO_TILE_ASPECT, info->gfx8.tile_aspect,
				GRPH_TILE_SPLIT, info->gfx8.tile_split,
				GRPH_MICRO_TILE_MODE, info->gfx8.tile_mode,
				GRPH_PIPE_CONFIG, info->gfx8.pipe_config,
				GRPH_ARRAY_MODE, info->gfx8.array_mode,
				GRPH_COLOR_EXPANSION_MODE, 1);
		/* 01 - DCP_GRPH_COLOR_EXPANSION_MODE_ZEXP: zero expansion for YCbCr */
		/*
				GRPH_Z, 0);
				*/
	}
}


static void program_size_and_rotation(
	struct mem_input *mi,
	enum dc_rotation_angle rotation,
	const union plane_size *plane_size)
{
	const struct rect *in_rect = &plane_size->grph.surface_size;
	struct rect hw_rect = plane_size->grph.surface_size;
	const uint32_t rotation_angles[ROTATION_ANGLE_COUNT] = {
			[ROTATION_ANGLE_0] = 0,
			[ROTATION_ANGLE_90] = 1,
			[ROTATION_ANGLE_180] = 2,
			[ROTATION_ANGLE_270] = 3,
	};

	if (rotation == ROTATION_ANGLE_90 || rotation == ROTATION_ANGLE_270) {
		hw_rect.x = in_rect->y;
		hw_rect.y = in_rect->x;

		hw_rect.height = in_rect->width;
		hw_rect.width = in_rect->height;
	}

	REG_SET(GRPH_X_START, 0,
			GRPH_X_START, hw_rect.x);

	REG_SET(GRPH_Y_START, 0,
			GRPH_Y_START, hw_rect.y);

	REG_SET(GRPH_X_END, 0,
			GRPH_X_END, hw_rect.width);

	REG_SET(GRPH_Y_END, 0,
			GRPH_Y_END, hw_rect.height);

	REG_SET(GRPH_PITCH, 0,
			GRPH_PITCH, plane_size->grph.surface_pitch);

	REG_SET(HW_ROTATION, 0,
			GRPH_ROTATION_ANGLE, rotation_angles[rotation]);
}

static void program_grph_pixel_format(
	struct mem_input *mi,
	enum surface_pixel_format format)
{
	uint32_t red_xbar = 0, blue_xbar = 0; /* no swap */
	uint32_t grph_depth, grph_format;
	uint32_t sign = 0, floating = 0;

	if (format == SURFACE_PIXEL_FORMAT_GRPH_ABGR8888 ||
			/*todo: doesn't look like we handle BGRA here,
			 *  should problem swap endian*/
		format == SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010 ||
		format == SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010_XR_BIAS ||
		format == SURFACE_PIXEL_FORMAT_GRPH_ABGR16161616F) {
		/* ABGR formats */
		red_xbar = 2;
		blue_xbar = 2;
	}

	REG_SET_2(GRPH_SWAP_CNTL, 0,
			GRPH_RED_CROSSBAR, red_xbar,
			GRPH_BLUE_CROSSBAR, blue_xbar);

	switch (format) {
	case SURFACE_PIXEL_FORMAT_GRPH_PALETA_256_COLORS:
		grph_depth = 0;
		grph_format = 0;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB1555:
		grph_depth = 1;
		grph_format = 0;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_RGB565:
		grph_depth = 1;
		grph_format = 1;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB8888:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR8888:
		grph_depth = 2;
		grph_format = 0;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB2101010:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010_XR_BIAS:
		grph_depth = 2;
		grph_format = 1;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR16161616F:
		sign = 1;
		floating = 1;
		/* no break */
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616F: /* shouldn't this get float too? */
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616:
		grph_depth = 3;
		grph_format = 0;
		break;
	default:
		DC_ERR("unsupported grph pixel format");
		break;
	}

	REG_UPDATE_2(GRPH_CONTROL,
			GRPH_DEPTH, grph_depth,
			GRPH_FORMAT, grph_format);

	REG_UPDATE_4(PRESCALE_GRPH_CONTROL,
			GRPH_PRESCALE_SELECT, floating,
			GRPH_PRESCALE_R_SIGN, sign,
			GRPH_PRESCALE_G_SIGN, sign,
			GRPH_PRESCALE_B_SIGN, sign);
}

void dce_mem_input_program_surface_config(struct mem_input *mi,
	enum surface_pixel_format format,
	union dc_tiling_info *tiling_info,
	union plane_size *plane_size,
	enum dc_rotation_angle rotation,
	struct dc_plane_dcc_param *dcc,
	bool horizontal_mirror,
	bool visible)
{
	REG_UPDATE(GRPH_ENABLE, GRPH_ENABLE, 1);

	program_tiling(mi, tiling_info);
	program_size_and_rotation(mi, rotation, plane_size);

	if (format >= SURFACE_PIXEL_FORMAT_GRPH_BEGIN &&
		format < SURFACE_PIXEL_FORMAT_VIDEO_BEGIN)
		program_grph_pixel_format(mi, format);
}

static uint32_t get_dmif_switch_time_us(
	uint32_t h_total,
	uint32_t v_total,
	uint32_t pix_clk_khz)
{
	uint32_t frame_time;
	uint32_t pixels_per_second;
	uint32_t pixels_per_frame;
	uint32_t refresh_rate;
	const uint32_t us_in_sec = 1000000;
	const uint32_t min_single_frame_time_us = 30000;
	/*return double of frame time*/
	const uint32_t single_frame_time_multiplier = 2;

	if (!h_total || v_total || !pix_clk_khz)
		return single_frame_time_multiplier * min_single_frame_time_us;

	/*TODO: should we use pixel format normalized pixel clock here?*/
	pixels_per_second = pix_clk_khz * 1000;
	pixels_per_frame = h_total * v_total;

	if (!pixels_per_second || !pixels_per_frame) {
		/* avoid division by zero */
		ASSERT(pixels_per_frame);
		ASSERT(pixels_per_second);
		return single_frame_time_multiplier * min_single_frame_time_us;
	}

	refresh_rate = pixels_per_second / pixels_per_frame;

	if (!refresh_rate) {
		/* avoid division by zero*/
		ASSERT(refresh_rate);
		return single_frame_time_multiplier * min_single_frame_time_us;
	}

	frame_time = us_in_sec / refresh_rate;

	if (frame_time < min_single_frame_time_us)
		frame_time = min_single_frame_time_us;

	frame_time *= single_frame_time_multiplier;

	return frame_time;
}

void dce_mem_input_allocate_dmif(struct mem_input *mi,
	uint32_t h_total,
	uint32_t v_total,
	uint32_t pix_clk_khz,
	uint32_t total_stream_num)
{
	const uint32_t retry_delay = 10;
	uint32_t retry_count = get_dmif_switch_time_us(
			h_total,
			v_total,
			pix_clk_khz) / retry_delay;

	uint32_t pix_dur;
	uint32_t buffers_allocated;
	uint32_t dmif_buffer_control;

	dmif_buffer_control = REG_GET(DMIF_BUFFER_CONTROL,
			DMIF_BUFFERS_ALLOCATED, &buffers_allocated);

	if (buffers_allocated == 2)
		return;

	REG_SET(DMIF_BUFFER_CONTROL, dmif_buffer_control,
			DMIF_BUFFERS_ALLOCATED, 2);

	REG_WAIT(DMIF_BUFFER_CONTROL,
			DMIF_BUFFERS_ALLOCATION_COMPLETED, 1,
			retry_delay, retry_count);

	if (pix_clk_khz != 0) {
		pix_dur = 1000000000ULL / pix_clk_khz;

		REG_UPDATE(DPG_PIPE_ARBITRATION_CONTROL1,
			PIXEL_DURATION, pix_dur);
	}

	if (mi->wa.single_head_rdreq_dmif_limit) {
		uint32_t eanble =  (total_stream_num > 1) ? 0 :
				mi->wa.single_head_rdreq_dmif_limit;

		REG_UPDATE(MC_HUB_RDREQ_DMIF_LIMIT,
				ENABLE, eanble);
	}
}

void dce_mem_input_free_dmif(struct mem_input *mi,
		uint32_t total_stream_num)
{
	uint32_t buffers_allocated;
	uint32_t dmif_buffer_control;

	dmif_buffer_control = REG_GET(DMIF_BUFFER_CONTROL,
			DMIF_BUFFERS_ALLOCATED, &buffers_allocated);

	if (buffers_allocated == 0)
		return;

	REG_SET(DMIF_BUFFER_CONTROL, dmif_buffer_control,
			DMIF_BUFFERS_ALLOCATED, 0);

	REG_WAIT(DMIF_BUFFER_CONTROL,
			DMIF_BUFFERS_ALLOCATION_COMPLETED, 1,
			10, 0xBB8);

	if (mi->wa.single_head_rdreq_dmif_limit) {
		uint32_t eanble =  (total_stream_num > 1) ? 0 :
				mi->wa.single_head_rdreq_dmif_limit;

		REG_UPDATE(MC_HUB_RDREQ_DMIF_LIMIT,
				ENABLE, eanble);
	}
}
