/*
 * Copyright 2012-15 Advanced Micro Devices, Inc.
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

#include "dm_services.h"


#include "dc_types.h"
#include "core_types.h"

#include "include/grph_object_id.h"
#include "include/fixed31_32.h"
#include "include/logger_interface.h"
#include "basics/conversion.h"

#include "dce110_transform.h"

#include "reg_helper.h"

#define REG(reg) \
	(xfm110->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	xfm110->xfm_shift->field_name, xfm110->xfm_mask->field_name

#define CTX \
	xfm110->base.ctx

#define IDENTITY_RATIO(ratio) (dal_fixed31_32_u2d19(ratio) == (1 << 19))
#define GAMUT_MATRIX_SIZE 12

enum dcp_out_trunc_round_mode {
	DCP_OUT_TRUNC_ROUND_MODE_TRUNCATE,
	DCP_OUT_TRUNC_ROUND_MODE_ROUND
};

enum dcp_out_trunc_round_depth {
	DCP_OUT_TRUNC_ROUND_DEPTH_14BIT,
	DCP_OUT_TRUNC_ROUND_DEPTH_13BIT,
	DCP_OUT_TRUNC_ROUND_DEPTH_12BIT,
	DCP_OUT_TRUNC_ROUND_DEPTH_11BIT,
	DCP_OUT_TRUNC_ROUND_DEPTH_10BIT,
	DCP_OUT_TRUNC_ROUND_DEPTH_9BIT,
	DCP_OUT_TRUNC_ROUND_DEPTH_8BIT
};

/*  defines the various methods of bit reduction available for use */
enum dcp_bit_depth_reduction_mode {
	DCP_BIT_DEPTH_REDUCTION_MODE_DITHER,
	DCP_BIT_DEPTH_REDUCTION_MODE_ROUND,
	DCP_BIT_DEPTH_REDUCTION_MODE_TRUNCATE,
	DCP_BIT_DEPTH_REDUCTION_MODE_DISABLED,
	DCP_BIT_DEPTH_REDUCTION_MODE_INVALID
};

enum dcp_spatial_dither_mode {
	DCP_SPATIAL_DITHER_MODE_AAAA,
	DCP_SPATIAL_DITHER_MODE_A_AA_A,
	DCP_SPATIAL_DITHER_MODE_AABBAABB,
	DCP_SPATIAL_DITHER_MODE_AABBCCAABBCC,
	DCP_SPATIAL_DITHER_MODE_INVALID
};

enum dcp_spatial_dither_depth {
	DCP_SPATIAL_DITHER_DEPTH_30BPP,
	DCP_SPATIAL_DITHER_DEPTH_24BPP
};

/*****************************************************************************
 * set_clamp
 *
 * @param depth : bit depth to set the clamp to (should match denorm)
 *
 * @brief
 *     Programs clamp according to panel bit depth.
 *
 *******************************************************************************/
static void set_clamp(
	struct dce110_transform *xfm110,
	enum dc_color_depth depth)
{
	int clamp_max = 0;

	/* At the clamp block the data will be MSB aligned, so we set the max
	 * clamp accordingly.
	 * For example, the max value for 6 bits MSB aligned (14 bit bus) would
	 * be "11 1111 0000 0000" in binary, so 0x3F00.
	 */
	switch (depth) {
	case COLOR_DEPTH_666:
		/* 6bit MSB aligned on 14 bit bus '11 1111 0000 0000' */
		clamp_max = 0x3F00;
		break;
	case COLOR_DEPTH_888:
		/* 8bit MSB aligned on 14 bit bus '11 1111 1100 0000' */
		clamp_max = 0x3FC0;
		break;
	case COLOR_DEPTH_101010:
		/* 10bit MSB aligned on 14 bit bus '11 1111 1111 1100' */
		clamp_max = 0x3FFC;
		break;
	case COLOR_DEPTH_121212:
		/* 12bit MSB aligned on 14 bit bus '11 1111 1111 1111' */
		clamp_max = 0x3FFF;
		break;
	default:
		clamp_max = 0x3FC0;
		BREAK_TO_DEBUGGER(); /* Invalid clamp bit depth */
	}
	REG_SET_2(OUT_CLAMP_CONTROL_B_CB, 0,
			OUT_CLAMP_MIN_B_CB, 0,
			OUT_CLAMP_MAX_B_CB, clamp_max);

	REG_SET_2(OUT_CLAMP_CONTROL_G_Y, 0,
			OUT_CLAMP_MIN_G_Y, 0,
			OUT_CLAMP_MAX_G_Y, clamp_max);

	REG_SET_2(OUT_CLAMP_CONTROL_R_CR, 0,
			OUT_CLAMP_MIN_R_CR, 0,
			OUT_CLAMP_MAX_R_CR, clamp_max);
}

/*******************************************************************************
 * set_round
 *
 * @brief
 *     Programs Round/Truncate
 *
 * @param [in] mode  :round or truncate
 * @param [in] depth :bit depth to round/truncate to
 OUT_ROUND_TRUNC_MODE 3:0 0xA Output data round or truncate mode
 POSSIBLE VALUES:
      00 - truncate to u0.12
      01 - truncate to u0.11
      02 - truncate to u0.10
      03 - truncate to u0.9
      04 - truncate to u0.8
      05 - reserved
      06 - truncate to u0.14
      07 - truncate to u0.13		set_reg_field_value(
			value,
			clamp_max,
			OUT_CLAMP_CONTROL_R_CR,
			OUT_CLAMP_MAX_R_CR);
      08 - round to u0.12
      09 - round to u0.11
      10 - round to u0.10
      11 - round to u0.9
      12 - round to u0.8
      13 - reserved
      14 - round to u0.14
      15 - round to u0.13

 ******************************************************************************/
static void set_round(
	struct dce110_transform *xfm110,
	enum dcp_out_trunc_round_mode mode,
	enum dcp_out_trunc_round_depth depth)
{
	int depth_bits = 0;
	int mode_bit = 0;

	/*  set up bit depth */
	switch (depth) {
	case DCP_OUT_TRUNC_ROUND_DEPTH_14BIT:
		depth_bits = 6;
		break;
	case DCP_OUT_TRUNC_ROUND_DEPTH_13BIT:
		depth_bits = 7;
		break;
	case DCP_OUT_TRUNC_ROUND_DEPTH_12BIT:
		depth_bits = 0;
		break;
	case DCP_OUT_TRUNC_ROUND_DEPTH_11BIT:
		depth_bits = 1;
		break;
	case DCP_OUT_TRUNC_ROUND_DEPTH_10BIT:
		depth_bits = 2;
		break;
	case DCP_OUT_TRUNC_ROUND_DEPTH_9BIT:
		depth_bits = 3;
		break;
	case DCP_OUT_TRUNC_ROUND_DEPTH_8BIT:
		depth_bits = 4;
		break;
	default:
		depth_bits = 4;
		BREAK_TO_DEBUGGER(); /* Invalid dcp_out_trunc_round_depth */
	}

	/*  set up round or truncate */
	switch (mode) {
	case DCP_OUT_TRUNC_ROUND_MODE_TRUNCATE:
		mode_bit = 0;
		break;
	case DCP_OUT_TRUNC_ROUND_MODE_ROUND:
		mode_bit = 1;
		break;
	default:
		BREAK_TO_DEBUGGER(); /* Invalid dcp_out_trunc_round_mode */
	}

	depth_bits |= mode_bit << 3;

	REG_SET(OUT_ROUND_CONTROL, 0, OUT_ROUND_TRUNC_MODE, depth_bits);
}

/*****************************************************************************
 * set_dither
 *
 * @brief
 *     Programs Dither
 *
 * @param [in] dither_enable        : enable dither
 * @param [in] dither_mode           : dither mode to set
 * @param [in] dither_depth          : bit depth to dither to
 * @param [in] frame_random_enable    : enable frame random
 * @param [in] rgb_random_enable      : enable rgb random
 * @param [in] highpass_random_enable : enable highpass random
 *
 ******************************************************************************/

static void set_dither(
	struct dce110_transform *xfm110,
	bool dither_enable,
	enum dcp_spatial_dither_mode dither_mode,
	enum dcp_spatial_dither_depth dither_depth,
	bool frame_random_enable,
	bool rgb_random_enable,
	bool highpass_random_enable)
{
	int dither_depth_bits = 0;
	int dither_mode_bits = 0;

	switch (dither_mode) {
	case DCP_SPATIAL_DITHER_MODE_AAAA:
		dither_mode_bits = 0;
		break;
	case DCP_SPATIAL_DITHER_MODE_A_AA_A:
		dither_mode_bits = 1;
		break;
	case DCP_SPATIAL_DITHER_MODE_AABBAABB:
		dither_mode_bits = 2;
		break;
	case DCP_SPATIAL_DITHER_MODE_AABBCCAABBCC:
		dither_mode_bits = 3;
		break;
	default:
		/* Invalid dcp_spatial_dither_mode */
		BREAK_TO_DEBUGGER();
	}

	switch (dither_depth) {
	case DCP_SPATIAL_DITHER_DEPTH_30BPP:
		dither_depth_bits = 0;
		break;
	case DCP_SPATIAL_DITHER_DEPTH_24BPP:
		dither_depth_bits = 1;
		break;
	default:
		/* Invalid dcp_spatial_dither_depth */
		BREAK_TO_DEBUGGER();
	}

	/*  write the register */
	REG_SET_6(DCP_SPATIAL_DITHER_CNTL, 0,
			DCP_SPATIAL_DITHER_EN, dither_enable,
			DCP_SPATIAL_DITHER_MODE, dither_mode_bits,
			DCP_SPATIAL_DITHER_DEPTH, dither_depth_bits,
			DCP_FRAME_RANDOM_ENABLE, frame_random_enable,
			DCP_RGB_RANDOM_ENABLE, rgb_random_enable,
			DCP_HIGHPASS_RANDOM_ENABLE, highpass_random_enable);
}

/*****************************************************************************
 * dce110_transform_bit_depth_reduction_program
 *
 * @brief
 *     Programs the DCP bit depth reduction registers (Clamp, Round/Truncate,
 *      Dither) for dce110
 *
 * @param depth : bit depth to set the clamp to (should match denorm)
 *
 ******************************************************************************/
static void program_bit_depth_reduction(
	struct dce110_transform *xfm110,
	enum dc_color_depth depth,
	const struct bit_depth_reduction_params *bit_depth_params)
{
	enum dcp_bit_depth_reduction_mode depth_reduction_mode;
	enum dcp_spatial_dither_mode spatial_dither_mode;
	bool frame_random_enable;
	bool rgb_random_enable;
	bool highpass_random_enable;

	ASSERT(depth < COLOR_DEPTH_121212); /* Invalid clamp bit depth */

	if (bit_depth_params->flags.SPATIAL_DITHER_ENABLED) {
		depth_reduction_mode = DCP_BIT_DEPTH_REDUCTION_MODE_DITHER;
		frame_random_enable = true;
		rgb_random_enable = true;
		highpass_random_enable = true;

	} else {
		depth_reduction_mode = DCP_BIT_DEPTH_REDUCTION_MODE_DISABLED;
		frame_random_enable = false;
		rgb_random_enable = false;
		highpass_random_enable = false;
	}

	spatial_dither_mode = DCP_SPATIAL_DITHER_MODE_A_AA_A;

	set_clamp(xfm110, depth);

	switch (depth_reduction_mode) {
	case DCP_BIT_DEPTH_REDUCTION_MODE_DITHER:
		/*  Spatial Dither: Set round/truncate to bypass (12bit),
		 *  enable Dither (30bpp) */
		set_round(xfm110,
			DCP_OUT_TRUNC_ROUND_MODE_TRUNCATE,
			DCP_OUT_TRUNC_ROUND_DEPTH_12BIT);

		set_dither(xfm110, true, spatial_dither_mode,
			DCP_SPATIAL_DITHER_DEPTH_30BPP, frame_random_enable,
			rgb_random_enable, highpass_random_enable);
		break;
	case DCP_BIT_DEPTH_REDUCTION_MODE_ROUND:
		/*  Round: Enable round (10bit), disable Dither */
		set_round(xfm110,
			DCP_OUT_TRUNC_ROUND_MODE_ROUND,
			DCP_OUT_TRUNC_ROUND_DEPTH_10BIT);

		set_dither(xfm110, false, spatial_dither_mode,
			DCP_SPATIAL_DITHER_DEPTH_30BPP, frame_random_enable,
			rgb_random_enable, highpass_random_enable);
		break;
	case DCP_BIT_DEPTH_REDUCTION_MODE_TRUNCATE: /*  Truncate */
		/*  Truncate: Enable truncate (10bit), disable Dither */
		set_round(xfm110,
			DCP_OUT_TRUNC_ROUND_MODE_TRUNCATE,
			DCP_OUT_TRUNC_ROUND_DEPTH_10BIT);

		set_dither(xfm110, false, spatial_dither_mode,
			DCP_SPATIAL_DITHER_DEPTH_30BPP, frame_random_enable,
			rgb_random_enable, highpass_random_enable);
		break;

	case DCP_BIT_DEPTH_REDUCTION_MODE_DISABLED: /*  Disabled */
		/*  Truncate: Set round/truncate to bypass (12bit),
		 * disable Dither */
		set_round(xfm110,
			DCP_OUT_TRUNC_ROUND_MODE_TRUNCATE,
			DCP_OUT_TRUNC_ROUND_DEPTH_12BIT);

		set_dither(xfm110, false, spatial_dither_mode,
			DCP_SPATIAL_DITHER_DEPTH_30BPP, frame_random_enable,
			rgb_random_enable, highpass_random_enable);
		break;
	default:
		/* Invalid DCP Depth reduction mode */
		BREAK_TO_DEBUGGER();
		break;
	}
}

static int dce110_transform_get_max_num_of_supported_lines(
	struct transform *xfm,
	enum lb_pixel_depth depth,
	int pixel_width)
{
	int pixels_per_entries = 0;
	int max_pixels_supports = 0;

	ASSERT(pixel_width);

	/* Find number of pixels that can fit into a single LB entry and
	 * take floor of the value since we cannot store a single pixel
	 * across multiple entries. */
	switch (depth) {
	case LB_PIXEL_DEPTH_18BPP:
		pixels_per_entries = xfm->lb_bits_per_entry / 18;
		break;

	case LB_PIXEL_DEPTH_24BPP:
		pixels_per_entries = xfm->lb_bits_per_entry / 24;
		break;

	case LB_PIXEL_DEPTH_30BPP:
		pixels_per_entries = xfm->lb_bits_per_entry / 30;
		break;

	case LB_PIXEL_DEPTH_36BPP:
		pixels_per_entries = xfm->lb_bits_per_entry / 36;
		break;

	default:
		dm_logger_write(xfm->ctx->logger, LOG_WARNING,
			"%s: Invalid LB pixel depth",
			__func__);
		BREAK_TO_DEBUGGER();
		break;
	}

	ASSERT(pixels_per_entries);

	max_pixels_supports =
			pixels_per_entries *
			xfm->lb_total_entries_num;

	return (max_pixels_supports / pixel_width);
}

static void set_denormalization(
	struct dce110_transform *xfm110,
	enum dc_color_depth depth)
{
	int denorm_mode = 0;

	switch (depth) {
	case COLOR_DEPTH_666:
		/* 63/64 for 6 bit output color depth */
		denorm_mode = 1;
		break;
	case COLOR_DEPTH_888:
		/* Unity for 8 bit output color depth
		 * because prescale is disabled by default */
		denorm_mode = 0;
		break;
	case COLOR_DEPTH_101010:
		/* 1023/1024 for 10 bit output color depth */
		denorm_mode = 3;
		break;
	case COLOR_DEPTH_121212:
		/* 4095/4096 for 12 bit output color depth */
		denorm_mode = 5;
		break;
	case COLOR_DEPTH_141414:
	case COLOR_DEPTH_161616:
	default:
		/* not valid used case! */
		break;
	}

	REG_SET(DENORM_CONTROL, 0, DENORM_MODE, denorm_mode);
}

static void dce110_transform_set_pixel_storage_depth(
	struct transform *xfm,
	enum lb_pixel_depth depth,
	const struct bit_depth_reduction_params *bit_depth_params)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);
	int pixel_depth, expan_mode;
	enum dc_color_depth color_depth;

	switch (depth) {
	case LB_PIXEL_DEPTH_18BPP:
		color_depth = COLOR_DEPTH_666;
		pixel_depth = 2;
		expan_mode  = 1;
		break;
	case LB_PIXEL_DEPTH_24BPP:
		color_depth = COLOR_DEPTH_888;
		pixel_depth = 1;
		expan_mode  = 1;
		break;
	case LB_PIXEL_DEPTH_30BPP:
		color_depth = COLOR_DEPTH_101010;
		pixel_depth = 0;
		expan_mode  = 1;
		break;
	case LB_PIXEL_DEPTH_36BPP:
		color_depth = COLOR_DEPTH_121212;
		pixel_depth = 3;
		expan_mode  = 0;
		break;
	default:
		color_depth = COLOR_DEPTH_101010;
		pixel_depth = 0;
		expan_mode  = 1;
		BREAK_TO_DEBUGGER();
		break;
	}

	set_denormalization(xfm110, color_depth);
	program_bit_depth_reduction(xfm110, color_depth, bit_depth_params);

	REG_UPDATE_2(LB_DATA_FORMAT,
			PIXEL_DEPTH, pixel_depth,
			PIXEL_EXPAN_MODE, expan_mode);

	if (!(xfm110->lb_pixel_depth_supported & depth)) {
		/*we should use unsupported capabilities
		 *  unless it is required by w/a*/
		dm_logger_write(xfm->ctx->logger, LOG_WARNING,
			"%s: Capability not supported",
			__func__);
	}
}

static void program_gamut_remap(
	struct dce110_transform *xfm110,
	const uint16_t *reg_val)
{
	if (reg_val) {
		REG_SET_2(GAMUT_REMAP_C11_C12, 0,
				GAMUT_REMAP_C11, reg_val[0],
				GAMUT_REMAP_C12, reg_val[1]);
		REG_SET_2(GAMUT_REMAP_C13_C14, 0,
				GAMUT_REMAP_C13, reg_val[2],
				GAMUT_REMAP_C14, reg_val[3]);
		REG_SET_2(GAMUT_REMAP_C21_C22, 0,
				GAMUT_REMAP_C21, reg_val[4],
				GAMUT_REMAP_C22, reg_val[5]);
		REG_SET_2(GAMUT_REMAP_C23_C24, 0,
				GAMUT_REMAP_C23, reg_val[6],
				GAMUT_REMAP_C24, reg_val[7]);
		REG_SET_2(GAMUT_REMAP_C31_C32, 0,
				GAMUT_REMAP_C31, reg_val[8],
				GAMUT_REMAP_C32, reg_val[9]);
		REG_SET_2(GAMUT_REMAP_C33_C34, 0,
				GAMUT_REMAP_C33, reg_val[10],
				GAMUT_REMAP_C34, reg_val[11]);

		REG_SET(GAMUT_REMAP_CONTROL, 0, GRPH_GAMUT_REMAP_MODE, 1);
	} else
		REG_SET(GAMUT_REMAP_CONTROL, 0, GRPH_GAMUT_REMAP_MODE, 0);

}

/**
 *****************************************************************************
 *  Function: dal_transform_wide_gamut_set_gamut_remap
 *
 *  @param [in] const struct xfm_grph_csc_adjustment *adjust
 *
 *  @return
 *     void
 *
 *  @note calculate and apply color temperature adjustment to in Rgb color space
 *
 *  @see
 *
 *****************************************************************************
 */
void dce110_transform_set_gamut_remap(
	struct transform *xfm,
	const struct xfm_grph_csc_adjustment *adjust)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);

	if (adjust->gamut_adjust_type != GRAPHICS_GAMUT_ADJUST_TYPE_SW)
		/* Bypass if type is bypass or hw */
		program_gamut_remap(xfm110, NULL);
	else {
		struct fixed31_32 arr_matrix[GAMUT_MATRIX_SIZE];
		uint16_t arr_reg_val[GAMUT_MATRIX_SIZE];

		arr_matrix[0] = adjust->temperature_matrix[0];
		arr_matrix[1] = adjust->temperature_matrix[1];
		arr_matrix[2] = adjust->temperature_matrix[2];
		arr_matrix[3] = dal_fixed31_32_zero;

		arr_matrix[4] = adjust->temperature_matrix[3];
		arr_matrix[5] = adjust->temperature_matrix[4];
		arr_matrix[6] = adjust->temperature_matrix[5];
		arr_matrix[7] = dal_fixed31_32_zero;

		arr_matrix[8] = adjust->temperature_matrix[6];
		arr_matrix[9] = adjust->temperature_matrix[7];
		arr_matrix[10] = adjust->temperature_matrix[8];
		arr_matrix[11] = dal_fixed31_32_zero;

		convert_float_matrix(
			arr_reg_val, arr_matrix, GAMUT_MATRIX_SIZE);

		program_gamut_remap(xfm110, arr_reg_val);
	}
}

static uint32_t decide_taps(struct fixed31_32 ratio, uint32_t in_taps, bool chroma)
{
	uint32_t taps;

	if (IDENTITY_RATIO(ratio)) {
		return 1;
	} else if (in_taps != 0) {
		taps = in_taps;
	} else {
		taps = 4;
	}

	if (chroma) {
		taps /= 2;
		if (taps < 2)
			taps = 2;
	}

	return taps;
}

bool transform_get_optimal_number_of_taps_helper(
	struct transform *xfm,
	struct scaler_data *scl_data,
	uint32_t pixel_width,
	const struct scaling_taps *in_taps) {

	int max_num_of_lines;

	max_num_of_lines = dce110_transform_get_max_num_of_supported_lines(
		xfm,
		scl_data->lb_params.depth,
		pixel_width);

	/* Fail if in_taps are impossible */
	if (in_taps->v_taps >= max_num_of_lines)
		return false;

	/*
	 * Set taps according to this policy (in this order)
	 * - Use 1 for no scaling
	 * - Use input taps
	 * - Use 4 and reduce as required by line buffer size
	 * - Decide chroma taps if chroma is scaled
	 *
	 * Ignore input chroma taps. Decide based on non-chroma
	 */
	scl_data->taps.h_taps = decide_taps(scl_data->ratios.horz, in_taps->h_taps, false);
	scl_data->taps.v_taps = decide_taps(scl_data->ratios.vert, in_taps->v_taps, false);
	scl_data->taps.h_taps_c = decide_taps(scl_data->ratios.horz_c, in_taps->h_taps, true);
	scl_data->taps.v_taps_c = decide_taps(scl_data->ratios.vert_c, in_taps->v_taps, true);

	if (!IDENTITY_RATIO(scl_data->ratios.vert)) {
		/* reduce v_taps if needed but ensure we have at least two */
		if (in_taps->v_taps == 0
				&& max_num_of_lines <= scl_data->taps.v_taps
				&& scl_data->taps.v_taps > 1) {
			scl_data->taps.v_taps = max_num_of_lines - 1;
		}

		if (scl_data->taps.v_taps <= 1)
			return false;
	}

	if (!IDENTITY_RATIO(scl_data->ratios.vert_c)) {
		/* reduce chroma v_taps if needed but ensure we have at least two */
		if (max_num_of_lines <= scl_data->taps.v_taps_c && scl_data->taps.v_taps_c > 1) {
			scl_data->taps.v_taps_c = max_num_of_lines - 1;
		}

		if (scl_data->taps.v_taps_c <= 1)
			return false;
	}

	/* we've got valid taps */
	return true;

}

bool dce110_transform_get_optimal_number_of_taps(
	struct transform *xfm,
	struct scaler_data *scl_data,
	const struct scaling_taps *in_taps)
{
	uint32_t pixel_width;


	if (scl_data->viewport.width > scl_data->recout.width)
		pixel_width = scl_data->recout.width;
	else
		pixel_width = scl_data->viewport.width;

	return transform_get_optimal_number_of_taps_helper(
			xfm,
			scl_data,
			pixel_width,
			in_taps);

}

static void dce110_transform_reset(struct transform *xfm)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);

	xfm110->filter_h = NULL;
	xfm110->filter_v = NULL;
}


static const struct transform_funcs dce110_transform_funcs = {
	.transform_reset = dce110_transform_reset,
	.transform_set_scaler =
		dce110_transform_set_scaler,
	.transform_set_gamut_remap =
		dce110_transform_set_gamut_remap,
	.transform_set_pixel_storage_depth =
		dce110_transform_set_pixel_storage_depth,
	.transform_get_optimal_number_of_taps =
		dce110_transform_get_optimal_number_of_taps
};

/*****************************************/
/* Constructor, Destructor               */
/*****************************************/

bool dce110_transform_construct(
	struct dce110_transform *xfm110,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dce110_transform_registers *regs,
	const struct dce110_transform_shift *xfm_shift,
	const struct dce110_transform_mask *xfm_mask)
{
	xfm110->base.ctx = ctx;

	xfm110->base.inst = inst;
	xfm110->base.funcs = &dce110_transform_funcs;

	xfm110->regs = regs;
	xfm110->xfm_shift = xfm_shift;
	xfm110->xfm_mask = xfm_mask;

	xfm110->lb_pixel_depth_supported =
			LB_PIXEL_DEPTH_18BPP |
			LB_PIXEL_DEPTH_24BPP |
			LB_PIXEL_DEPTH_30BPP;

	xfm110->base.lb_bits_per_entry = LB_BITS_PER_ENTRY;
	xfm110->base.lb_total_entries_num = LB_TOTAL_NUMBER_OF_ENTRIES;

	xfm110->base.lb_memory_size = 0x6B0; /*1712*/

	return true;
}
