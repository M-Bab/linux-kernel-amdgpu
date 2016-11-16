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

/* include DCE11 register header files */
#include "dce/dce_11_0_d.h"
#include "dce/dce_11_0_sh_mask.h"

#include "dc_types.h"
#include "core_types.h"

#include "include/grph_object_id.h"
#include "include/fixed31_32.h"
#include "include/logger_interface.h"
#include "basics/conversion.h"

#include "dce110_transform.h"


#define DCP_REG(reg)\
	(reg + xfm110->offsets.dcp_offset)

#define LB_REG(reg)\
	(reg + xfm110->offsets.lb_offset)

#define IDENTITY_RATIO(ratio) (dal_fixed31_32_u2d19(ratio) == (1 << 19))
#define GAMUT_MATRIX_SIZE 12

#define DISP_BRIGHTNESS_DEFAULT_HW 0
#define DISP_BRIGHTNESS_MIN_HW -25
#define DISP_BRIGHTNESS_MAX_HW 25
#define DISP_BRIGHTNESS_STEP_HW 1
#define DISP_BRIGHTNESS_HW_DIVIDER 100

#define DISP_HUE_DEFAULT_HW 0
#define DISP_HUE_MIN_HW -30
#define DISP_HUE_MAX_HW 30
#define DISP_HUE_STEP_HW 1
#define DISP_HUE_HW_DIVIDER 1

#define DISP_CONTRAST_DEFAULT_HW 100
#define DISP_CONTRAST_MIN_HW 50
#define DISP_CONTRAST_MAX_HW 150
#define DISP_CONTRAST_STEP_HW 1
#define DISP_CONTRAST_HW_DIVIDER 100

#define DISP_SATURATION_DEFAULT_HW 100
#define DISP_SATURATION_MIN_HW 0
#define DISP_SATURATION_MAX_HW 200
#define DISP_SATURATION_STEP_HW 1
#define DISP_SATURATION_HW_DIVIDER 100

#define DISP_KELVIN_DEGRES_DEFAULT 6500
#define DISP_KELVIN_DEGRES_MIN 4000
#define DISP_KELVIN_DEGRES_MAX 10000
#define DISP_KELVIN_DEGRES_STEP 100
#define DISP_KELVIN_HW_DIVIDER 10000

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

/**
 *******************************************************************************
 * set_clamp
 *
 * @param depth : bit depth to set the clamp to (should match denorm)
 *
 * @brief
 *     Programs clamp according to panel bit depth.
 *
 * @return
 *     true if succeeds
 *
 *******************************************************************************
 */
static bool set_clamp(
	struct dce110_transform *xfm110,
	enum dc_color_depth depth)
{
	uint32_t clamp_max = 0;

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
		ASSERT_CRITICAL(false); /* Invalid clamp bit depth */
		return false;
	}

	{
		uint32_t value = 0;
		/*  always set min to 0 */
			set_reg_field_value(
			value,
			0,
			OUT_CLAMP_CONTROL_B_CB,
			OUT_CLAMP_MIN_B_CB);

		set_reg_field_value(
			value,
			clamp_max,
			OUT_CLAMP_CONTROL_B_CB,
			OUT_CLAMP_MAX_B_CB);

		dm_write_reg(xfm110->base.ctx,
			DCP_REG(mmOUT_CLAMP_CONTROL_B_CB),
			value);
	}

	{
		uint32_t value = 0;
		/*  always set min to 0 */
		set_reg_field_value(
			value,
			0,
			OUT_CLAMP_CONTROL_G_Y,
			OUT_CLAMP_MIN_G_Y);

		set_reg_field_value(
			value,
			clamp_max,
			OUT_CLAMP_CONTROL_G_Y,
			OUT_CLAMP_MAX_G_Y);

		dm_write_reg(xfm110->base.ctx,
			DCP_REG(mmOUT_CLAMP_CONTROL_G_Y),
			value);
	}

	{
		uint32_t value = 0;
		/*  always set min to 0 */
		set_reg_field_value(
			value,
			0,
			OUT_CLAMP_CONTROL_R_CR,
			OUT_CLAMP_MIN_R_CR);

		set_reg_field_value(
			value,
			clamp_max,
			OUT_CLAMP_CONTROL_R_CR,
			OUT_CLAMP_MAX_R_CR);

		dm_write_reg(xfm110->base.ctx,
			DCP_REG(mmOUT_CLAMP_CONTROL_R_CR),
			value);
	}

	return true;
}

/**
 *******************************************************************************
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

 * @return
 *     true if succeeds.
 *******************************************************************************
 */
static bool set_round(
	struct dce110_transform *xfm110,
	enum dcp_out_trunc_round_mode mode,
	enum dcp_out_trunc_round_depth depth)
{
	uint32_t depth_bits = 0;
	uint32_t mode_bit = 0;
	/*  zero out all bits */
	uint32_t value = 0;

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
		/* Invalid dcp_out_trunc_round_depth */
		ASSERT_CRITICAL(false);
		return false;
	}

	set_reg_field_value(
		value,
		depth_bits,
		OUT_ROUND_CONTROL,
		OUT_ROUND_TRUNC_MODE);

	/*  set up round or truncate */
	switch (mode) {
	case DCP_OUT_TRUNC_ROUND_MODE_TRUNCATE:
		mode_bit = 0;
		break;
	case DCP_OUT_TRUNC_ROUND_MODE_ROUND:
		mode_bit = 1;
		break;
	default:
		/* Invalid dcp_out_trunc_round_mode */
		ASSERT_CRITICAL(false);
		return false;
	}

	depth_bits |= mode_bit << 3;

	set_reg_field_value(
		value,
		depth_bits,
		OUT_ROUND_CONTROL,
		OUT_ROUND_TRUNC_MODE);

	/*  write the register */
	dm_write_reg(xfm110->base.ctx,
				DCP_REG(mmOUT_ROUND_CONTROL),
				value);

	return true;
}

/**
 *******************************************************************************
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
 * @return
 *     true if succeeds.
 *******************************************************************************
 */

static bool set_dither(
	struct dce110_transform *xfm110,
	bool dither_enable,
	enum dcp_spatial_dither_mode dither_mode,
	enum dcp_spatial_dither_depth dither_depth,
	bool frame_random_enable,
	bool rgb_random_enable,
	bool highpass_random_enable)
{
	uint32_t dither_depth_bits = 0;
	uint32_t dither_mode_bits = 0;
	/*  zero out all bits */
	uint32_t value = 0;

	/* set up the fields */
	if (dither_enable)
		set_reg_field_value(
			value,
			1,
			DCP_SPATIAL_DITHER_CNTL,
			DCP_SPATIAL_DITHER_EN);

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
		ASSERT_CRITICAL(false);
		return false;

	}
	set_reg_field_value(
		value,
		dither_mode_bits,
		DCP_SPATIAL_DITHER_CNTL,
		DCP_SPATIAL_DITHER_MODE);

	switch (dither_depth) {
	case DCP_SPATIAL_DITHER_DEPTH_30BPP:
		dither_depth_bits = 0;
		break;
	case DCP_SPATIAL_DITHER_DEPTH_24BPP:
		dither_depth_bits = 1;
		break;
	default:
		/* Invalid dcp_spatial_dither_depth */
		ASSERT_CRITICAL(false);
		return false;
	}

	set_reg_field_value(
		value,
		dither_depth_bits,
		DCP_SPATIAL_DITHER_CNTL,
		DCP_SPATIAL_DITHER_DEPTH);

	if (frame_random_enable)
		set_reg_field_value(
			value,
			1,
			DCP_SPATIAL_DITHER_CNTL,
			DCP_FRAME_RANDOM_ENABLE);

	if (rgb_random_enable)
		set_reg_field_value(
			value,
			1,
			DCP_SPATIAL_DITHER_CNTL,
			DCP_RGB_RANDOM_ENABLE);

	if (highpass_random_enable)
		set_reg_field_value(
			value,
			1,
			DCP_SPATIAL_DITHER_CNTL,
			DCP_HIGHPASS_RANDOM_ENABLE);

	/*  write the register */
	dm_write_reg(xfm110->base.ctx,
				DCP_REG(mmDCP_SPATIAL_DITHER_CNTL),
				value);

	return true;
}

/**
 *******************************************************************************
 * dce110_transform_bit_depth_reduction_program
 *
 * @brief
 *     Programs the DCP bit depth reduction registers (Clamp, Round/Truncate,
 *      Dither) for dce110
 *
 * @param depth : bit depth to set the clamp to (should match denorm)
 *
 * @return
 *     true if succeeds.
 *******************************************************************************
 */
static bool program_bit_depth_reduction(
	struct dce110_transform *xfm110,
	enum dc_color_depth depth,
	const struct bit_depth_reduction_params *bit_depth_params)
{
	enum dcp_bit_depth_reduction_mode depth_reduction_mode;
	enum dcp_spatial_dither_mode spatial_dither_mode;
	bool frame_random_enable;
	bool rgb_random_enable;
	bool highpass_random_enable;

	if (depth > COLOR_DEPTH_121212) {
		ASSERT_CRITICAL(false); /* Invalid clamp bit depth */
		return false;
	}

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

	if (!set_clamp(xfm110, depth)) {
		/* Failure in set_clamp() */
		ASSERT_CRITICAL(false);
		return false;
	}

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
		ASSERT_CRITICAL(false);
		break;
	}

	return true;
}

static int dce110_transform_get_max_num_of_supported_lines(
	struct transform *xfm,
	enum lb_pixel_depth depth,
	int pixel_width)
{
	int pixels_per_entries = 0;
	int max_pixels_supports = 0;

	ASSERT_CRITICAL(pixel_width);

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
		ASSERT_CRITICAL(false);
		break;
	}

	ASSERT_CRITICAL(pixels_per_entries);

	max_pixels_supports =
			pixels_per_entries *
			xfm->lb_total_entries_num;

	return (max_pixels_supports / pixel_width);
}

static void set_denormalization(
	struct dce110_transform *xfm110,
	enum dc_color_depth depth)
{
	uint32_t value = dm_read_reg(xfm110->base.ctx,
			DCP_REG(mmDENORM_CONTROL));

	switch (depth) {
	case COLOR_DEPTH_666:
		/* 63/64 for 6 bit output color depth */
		set_reg_field_value(
			value,
			1,
			DENORM_CONTROL,
			DENORM_MODE);
		break;
	case COLOR_DEPTH_888:
		/* Unity for 8 bit output color depth
		 * because prescale is disabled by default */
		set_reg_field_value(
			value,
			0,
			DENORM_CONTROL,
			DENORM_MODE);
		break;
	case COLOR_DEPTH_101010:
		/* 1023/1024 for 10 bit output color depth */
		set_reg_field_value(
			value,
			3,
			DENORM_CONTROL,
			DENORM_MODE);
		break;
	case COLOR_DEPTH_121212:
		/* 4095/4096 for 12 bit output color depth */
		set_reg_field_value(
			value,
			5,
			DENORM_CONTROL,
			DENORM_MODE);
		break;
	case COLOR_DEPTH_141414:
	case COLOR_DEPTH_161616:
	default:
		/* not valid used case! */
		break;
	}

	dm_write_reg(xfm110->base.ctx,
			DCP_REG(mmDENORM_CONTROL),
			value);

}

bool dce110_transform_set_pixel_storage_depth(
	struct transform *xfm,
	enum lb_pixel_depth depth,
	const struct bit_depth_reduction_params *bit_depth_params)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);
	bool ret = true;
	uint32_t value;
	enum dc_color_depth color_depth;

	value = dm_read_reg(xfm->ctx, LB_REG(mmLB_DATA_FORMAT));
	switch (depth) {
	case LB_PIXEL_DEPTH_18BPP:
		color_depth = COLOR_DEPTH_666;
		set_reg_field_value(value, 2, LB_DATA_FORMAT, PIXEL_DEPTH);
		set_reg_field_value(value, 1, LB_DATA_FORMAT, PIXEL_EXPAN_MODE);
		break;
	case LB_PIXEL_DEPTH_24BPP:
		color_depth = COLOR_DEPTH_888;
		set_reg_field_value(value, 1, LB_DATA_FORMAT, PIXEL_DEPTH);
		set_reg_field_value(value, 1, LB_DATA_FORMAT, PIXEL_EXPAN_MODE);
		break;
	case LB_PIXEL_DEPTH_30BPP:
		color_depth = COLOR_DEPTH_101010;
		set_reg_field_value(value, 0, LB_DATA_FORMAT, PIXEL_DEPTH);
		set_reg_field_value(value, 1, LB_DATA_FORMAT, PIXEL_EXPAN_MODE);
		break;
	case LB_PIXEL_DEPTH_36BPP:
		color_depth = COLOR_DEPTH_121212;
		set_reg_field_value(value, 3, LB_DATA_FORMAT, PIXEL_DEPTH);
		set_reg_field_value(value, 0, LB_DATA_FORMAT, PIXEL_EXPAN_MODE);
		break;
	default:
		ret = false;
		break;
	}

	if (ret == true) {
		set_denormalization(xfm110, color_depth);
		ret = program_bit_depth_reduction(xfm110, color_depth,
				bit_depth_params);

		set_reg_field_value(value, 0, LB_DATA_FORMAT, ALPHA_EN);
		dm_write_reg(xfm->ctx, LB_REG(mmLB_DATA_FORMAT), value);
		if (!(xfm110->lb_pixel_depth_supported & depth)) {
			/*we should use unsupported capabilities
			 *  unless it is required by w/a*/
			dm_logger_write(xfm->ctx->logger, LOG_WARNING,
				"%s: Capability not supported",
				__func__);
		}
	}

	return ret;
}

static void program_gamut_remap(
	struct dce110_transform *xfm110,
	const uint16_t *reg_val)
{
	struct dc_context *ctx = xfm110->base.ctx;
	uint32_t value = 0;
	uint32_t addr = DCP_REG(mmGAMUT_REMAP_CONTROL);

	/* the register controls ovl also */
	value = dm_read_reg(ctx, addr);

	if (reg_val) {
		{
			uint32_t reg_data = 0;
			uint32_t addr = DCP_REG(mmGAMUT_REMAP_C11_C12);

			/* fixed S2.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[0],
				GAMUT_REMAP_C11_C12,
				GAMUT_REMAP_C11);
			/* fixed S2.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[1],
				GAMUT_REMAP_C11_C12,
				GAMUT_REMAP_C12);

			dm_write_reg(ctx, addr, reg_data);
		}
		{
			uint32_t reg_data = 0;
			uint32_t addr = DCP_REG(mmGAMUT_REMAP_C13_C14);

			/* fixed S2.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[2],
				GAMUT_REMAP_C13_C14,
				GAMUT_REMAP_C13);

			/* fixed S0.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[3],
				GAMUT_REMAP_C13_C14,
				GAMUT_REMAP_C14);

			dm_write_reg(ctx, addr, reg_data);
		}
		{
			uint32_t reg_data = 0;
			uint32_t addr = DCP_REG(mmGAMUT_REMAP_C21_C22);

			/* fixed S2.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[4],
				GAMUT_REMAP_C21_C22,
				GAMUT_REMAP_C21);

			/* fixed S0.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[5],
				GAMUT_REMAP_C21_C22,
				GAMUT_REMAP_C22);

			dm_write_reg(ctx, addr, reg_data);
		}
		{
			uint32_t reg_data = 0;
			uint32_t addr = DCP_REG(mmGAMUT_REMAP_C23_C24);

			/* fixed S2.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[6],
				GAMUT_REMAP_C23_C24,
				GAMUT_REMAP_C23);

			/* fixed S0.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[7],
				GAMUT_REMAP_C23_C24,
				GAMUT_REMAP_C24);

			dm_write_reg(ctx, addr, reg_data);
		}
		{
			uint32_t reg_data = 0;
			uint32_t addr = DCP_REG(mmGAMUT_REMAP_C31_C32);

			/* fixed S2.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[8],
				GAMUT_REMAP_C31_C32,
				GAMUT_REMAP_C31);

			/* fixed S0.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[9],
				GAMUT_REMAP_C31_C32,
				GAMUT_REMAP_C32);

			dm_write_reg(ctx, addr, reg_data);
		}
		{
			uint32_t reg_data = 0;
			uint32_t addr = DCP_REG(mmGAMUT_REMAP_C33_C34);

			/* fixed S2.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[10],
				GAMUT_REMAP_C33_C34,
				GAMUT_REMAP_C33);

			/* fixed S0.13 format */
			set_reg_field_value(
				reg_data,
				reg_val[11],
				GAMUT_REMAP_C33_C34,
				GAMUT_REMAP_C34);

			dm_write_reg(ctx, addr, reg_data);
		}

		set_reg_field_value(
			value,
			1,
			GAMUT_REMAP_CONTROL,
			GRPH_GAMUT_REMAP_MODE);

	} else
		set_reg_field_value(
			value,
			0,
			GAMUT_REMAP_CONTROL,
			GRPH_GAMUT_REMAP_MODE);

	addr = DCP_REG(mmGAMUT_REMAP_CONTROL);
	dm_write_reg(ctx, addr, value);

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
	const struct dce110_transform_reg_offsets *reg_offsets)
{
	xfm110->base.ctx = ctx;

	xfm110->base.inst = inst;
	xfm110->base.funcs = &dce110_transform_funcs;

	xfm110->offsets = *reg_offsets;

	xfm110->lb_pixel_depth_supported =
			LB_PIXEL_DEPTH_18BPP |
			LB_PIXEL_DEPTH_24BPP |
			LB_PIXEL_DEPTH_30BPP;

	xfm110->base.lb_bits_per_entry = LB_BITS_PER_ENTRY;
	xfm110->base.lb_total_entries_num = LB_TOTAL_NUMBER_OF_ENTRIES;

	xfm110->base.lb_memory_size = 0x6B0; /*1712*/

	return true;
}
