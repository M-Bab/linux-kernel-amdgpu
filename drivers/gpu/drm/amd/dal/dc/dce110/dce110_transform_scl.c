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

#include "dce110_transform.h"
#include "dce110_transform_bit_depth.h"

#define UP_SCALER_RATIO_MAX 16000
#define DOWN_SCALER_RATIO_MAX 250
#define SCALER_RATIO_DIVIDER 1000

#define SCL_REG(reg)\
	(reg + xfm110->offsets.scl_offset)

#define DCFE_REG(reg)\
	(reg + xfm110->offsets.dcfe_offset)

#define SCL_PHASES 16

static const uint16_t filter_2tap_16p[18] = {
	4096, 0,
	3840, 256,
	3584, 512,
	3328, 768,
	3072, 1024,
	2816, 1280,
	2560, 1536,
	2304, 1792,
	2048, 2048
};

static const uint16_t filter_3tap_16p_upscale[27] = {
	2048, 2048, 0,
	1708, 2424, 16348,
	1372, 2796, 16308,
	1056, 3148, 16272,
	768, 3464, 16244,
	512, 3728, 16236,
	296, 3928, 16252,
	124, 4052, 16296,
	0, 4096, 0
};

static const uint16_t filter_3tap_16p_117[27] = {
	2048, 2048, 0,
	1824, 2276, 16376,
	1600, 2496, 16380,
	1376, 2700, 16,
	1156, 2880, 52,
	948, 3032, 108,
	756, 3144, 192,
	580, 3212, 296,
	428, 3236, 428
};

static const uint16_t filter_3tap_16p_150[27] = {
	2048, 2048, 0,
	1872, 2184, 36,
	1692, 2308, 88,
	1516, 2420, 156,
	1340, 2516, 236,
	1168, 2592, 328,
	1004, 2648, 440,
	844, 2684, 560,
	696, 2696, 696
};

static const uint16_t filter_3tap_16p_183[27] = {
	2048, 2048, 0,
	1892, 2104, 92,
	1744, 2152, 196,
	1592, 2196, 300,
	1448, 2232, 412,
	1304, 2256, 528,
	1168, 2276, 648,
	1032, 2288, 772,
	900, 2292, 900
};

static const uint16_t filter_4tap_16p_upscale[36] = {
	0, 4096, 0, 0,
	16240, 4056, 180, 16380,
	16136, 3952, 404, 16364,
	16072, 3780, 664, 16344,
	16040, 3556, 952, 16312,
	16036, 3284, 1268, 16272,
	16052, 2980, 1604, 16224,
	16084, 2648, 1952, 16176,
	16128, 2304, 2304, 16128
};

static const uint16_t filter_4tap_16p_117[36] = {
	428, 3236, 428, 0,
	276, 3232, 604, 16364,
	148, 3184, 800, 16340,
	44, 3104, 1016, 16312,
	16344, 2984, 1244, 16284,
	16284, 2832, 1488, 16256,
	16244, 2648, 1732, 16236,
	16220, 2440, 1976, 16220,
	16212, 2216, 2216, 16212
};

static const uint16_t filter_4tap_16p_150[36] = {
	696, 2700, 696, 0,
	560, 2700, 848, 16364,
	436, 2676, 1008, 16348,
	328, 2628, 1180, 16336,
	232, 2556, 1356, 16328,
	152, 2460, 1536, 16328,
	84, 2344, 1716, 16332,
	28, 2208, 1888, 16348,
	16376, 2052, 2052, 16376
};

static const uint16_t filter_4tap_16p_183[36] = {
	940, 2208, 940, 0,
	832, 2200, 1052, 4,
	728, 2180, 1164, 16,
	628, 2148, 1280, 36,
	536, 2100, 1392, 60,
	448, 2044, 1504, 92,
	368, 1976, 1612, 132,
	296, 1900, 1716, 176,
	232, 1812, 1812, 232
};

static void disable_enhanced_sharpness(struct dce110_transform *xfm110)
{
	uint32_t  value;

	value = dm_read_reg(xfm110->base.ctx,
			SCL_REG(mmSCL_F_SHARP_CONTROL));

	set_reg_field_value(value, 0,
			SCL_F_SHARP_CONTROL, SCL_HF_SHARP_EN);

	set_reg_field_value(value, 0,
			SCL_F_SHARP_CONTROL, SCL_VF_SHARP_EN);

	set_reg_field_value(value, 0,
			SCL_F_SHARP_CONTROL, SCL_HF_SHARP_SCALE_FACTOR);

	set_reg_field_value(value, 0,
			SCL_F_SHARP_CONTROL, SCL_VF_SHARP_SCALE_FACTOR);

	dm_write_reg(xfm110->base.ctx,
			SCL_REG(mmSCL_F_SHARP_CONTROL), value);
}

/*
 *	@Function:
 *		void setup_scaling_configuration
 *	@Purpose: setup scaling mode : bypass, RGb, YCbCr and number of taps
 *	@Input:   data
 *
 *	@Output: void
 */
static bool setup_scaling_configuration(
	struct dce110_transform *xfm110,
	const struct scaler_data *data)
{
	struct dc_context *ctx = xfm110->base.ctx;
	uint32_t addr;
	uint32_t value;

	addr = SCL_REG(mmSCL_BYPASS_CONTROL);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(
		value,
		0,
		SCL_BYPASS_CONTROL,
		SCL_BYPASS_MODE);
	dm_write_reg(ctx, addr, value);

	if (data->taps.h_taps + data->taps.v_taps <= 2) {
		dce110_transform_set_scaler_bypass(&xfm110->base, NULL);
		return false;
	}

	addr = SCL_REG(mmSCL_TAP_CONTROL);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(value, data->taps.h_taps - 1,
			SCL_TAP_CONTROL, SCL_H_NUM_OF_TAPS);
	set_reg_field_value(value, data->taps.v_taps - 1,
			SCL_TAP_CONTROL, SCL_V_NUM_OF_TAPS);
	dm_write_reg(ctx, addr, value);

	addr = SCL_REG(mmSCL_MODE);
	value = dm_read_reg(ctx, addr);
	if (data->format <= PIXEL_FORMAT_GRPH_END)
		set_reg_field_value(value, 1, SCL_MODE, SCL_MODE);
	else
		set_reg_field_value(value, 2, SCL_MODE, SCL_MODE);
	set_reg_field_value(value, 1, SCL_MODE, SCL_PSCL_EN);
	dm_write_reg(ctx, addr, value);

	addr = SCL_REG(mmSCL_CONTROL);
	value = dm_read_reg(ctx, addr);
	 /* 1 - Replaced out of bound pixels with edge */
	set_reg_field_value(value, 1, SCL_CONTROL, SCL_BOUNDARY_MODE);
	dm_write_reg(ctx, addr, value);

	return true;
}

/**
* Function:
* void program_overscan
*
* Purpose: Programs overscan border
* Input:   overscan
*
* Output:
   void
*/
static void program_overscan(
		struct dce110_transform *xfm110,
		const struct scaler_data *data)
{
	uint32_t overscan_left_right = 0;
	uint32_t overscan_top_bottom = 0;

	int overscan_right = data->h_active
			- data->recout.x - data->recout.width;
	int overscan_bottom = data->v_active
			- data->recout.y - data->recout.height;

	if (overscan_right < 0) {
		BREAK_TO_DEBUGGER();
		overscan_right = 0;
	}
	if (overscan_bottom < 0) {
		BREAK_TO_DEBUGGER();
		overscan_bottom = 0;
	}

	set_reg_field_value(overscan_left_right, data->recout.x,
			EXT_OVERSCAN_LEFT_RIGHT, EXT_OVERSCAN_LEFT);

	set_reg_field_value(overscan_left_right, overscan_right,
			EXT_OVERSCAN_LEFT_RIGHT, EXT_OVERSCAN_RIGHT);

	set_reg_field_value(overscan_top_bottom, data->recout.y,
			EXT_OVERSCAN_TOP_BOTTOM, EXT_OVERSCAN_TOP);

	set_reg_field_value(overscan_top_bottom, overscan_bottom,
			EXT_OVERSCAN_TOP_BOTTOM, EXT_OVERSCAN_BOTTOM);

	dm_write_reg(xfm110->base.ctx,
			SCL_REG(mmEXT_OVERSCAN_LEFT_RIGHT),
			overscan_left_right);

	dm_write_reg(xfm110->base.ctx,
			SCL_REG(mmEXT_OVERSCAN_TOP_BOTTOM),
			overscan_top_bottom);
}

static void program_two_taps_filter(
	struct dce110_transform *xfm110,
	bool enable,
	bool vertical)
{
	uint32_t addr;
	uint32_t value;
	/* 1: Hard coded 2 tap filter
	 * 0: Programmable 2 tap filter from coefficient RAM
	 */
	if (vertical) {
		addr = SCL_REG(mmSCL_VERT_FILTER_CONTROL);
		value = dm_read_reg(xfm110->base.ctx, addr);
		set_reg_field_value(
			value,
			enable ? 1 : 0,
				SCL_VERT_FILTER_CONTROL,
				SCL_V_2TAP_HARDCODE_COEF_EN);

	} else {
		addr = SCL_REG(mmSCL_HORZ_FILTER_CONTROL);
		value = dm_read_reg(xfm110->base.ctx, addr);
		set_reg_field_value(
			value,
			enable ? 1 : 0,
			SCL_HORZ_FILTER_CONTROL,
			SCL_H_2TAP_HARDCODE_COEF_EN);
	}

	dm_write_reg(xfm110->base.ctx, addr, value);
}

static void set_coeff_update_complete(struct dce110_transform *xfm110)
{
	uint32_t value;
	uint32_t addr = SCL_REG(mmSCL_UPDATE);

	value = dm_read_reg(xfm110->base.ctx, addr);
	set_reg_field_value(value, 1, SCL_UPDATE, SCL_COEF_UPDATE_COMPLETE);
	dm_write_reg(xfm110->base.ctx, addr, value);
}

static const uint16_t *get_filter_3tap_16p(struct fixed31_32 ratio)
{
	if (ratio.value < dal_fixed31_32_one.value)
		return filter_3tap_16p_upscale;
	else if (ratio.value < dal_fixed31_32_from_fraction(4, 3).value)
		return filter_3tap_16p_117;
	else if (ratio.value < dal_fixed31_32_from_fraction(5, 3).value)
		return filter_3tap_16p_150;
	else
		return filter_3tap_16p_183;
}

static const uint16_t *get_filter_4tap_16p(struct fixed31_32 ratio)
{
	if (ratio.value < dal_fixed31_32_one.value)
		return filter_4tap_16p_upscale;
	else if (ratio.value < dal_fixed31_32_from_fraction(4, 3).value)
		return filter_4tap_16p_117;
	else if (ratio.value < dal_fixed31_32_from_fraction(5, 3).value)
		return filter_4tap_16p_150;
	else
		return filter_4tap_16p_183;
}

static void program_multi_taps_filter(
	struct dce110_transform *xfm110,
	int taps,
	const uint16_t *coeffs,
	enum ram_filter_type filter_type)
{
	struct dc_context *ctx = xfm110->base.ctx;
	int i, phase, pair;
	int array_idx = 0;
	int taps_pairs = (taps + 1) / 2;
	int phases_to_program = SCL_PHASES / 2 + 1;

	uint32_t select = 0;
	uint32_t power_ctl, power_ctl_off;

	if (!coeffs)
		return;

	/*We need to disable power gating on coeff memory to do programming*/
	power_ctl = dm_read_reg(ctx, DCFE_REG(mmDCFE_MEM_PWR_CTRL));
	power_ctl_off = power_ctl;
	set_reg_field_value(power_ctl_off, 1, DCFE_MEM_PWR_CTRL, SCL_COEFF_MEM_PWR_DIS);
	dm_write_reg(ctx, DCFE_REG(mmDCFE_MEM_PWR_CTRL), power_ctl_off);

	/*Wait to disable gating:*/
	for (i = 0; i < 10; i++) {
		if (get_reg_field_value(
				dm_read_reg(ctx, DCFE_REG(mmDCFE_MEM_PWR_STATUS)),
				DCFE_MEM_PWR_STATUS,
				SCL_COEFF_MEM_PWR_STATE) == 0)
			break;

		udelay(1);
	}

	set_reg_field_value(select, filter_type, SCL_COEF_RAM_SELECT, SCL_C_RAM_FILTER_TYPE);

	for (phase = 0; phase < phases_to_program; phase++) {
		/*we always program N/2 + 1 phases, total phases N, but N/2-1 are just mirror
		phase 0 is unique and phase N/2 is unique if N is even*/
		set_reg_field_value(select, phase, SCL_COEF_RAM_SELECT, SCL_C_RAM_PHASE);
		for (pair = 0; pair < taps_pairs; pair++) {
			uint32_t data = 0;

			set_reg_field_value(select, pair,
					SCL_COEF_RAM_SELECT, SCL_C_RAM_TAP_PAIR_IDX);

			dm_write_reg(ctx, SCL_REG(mmSCL_COEF_RAM_SELECT), select);

			set_reg_field_value(
					data, 1,
					SCL_COEF_RAM_TAP_DATA,
					SCL_C_RAM_EVEN_TAP_COEF_EN);
			set_reg_field_value(
					data, coeffs[array_idx],
					SCL_COEF_RAM_TAP_DATA,
					SCL_C_RAM_EVEN_TAP_COEF);

			if (taps % 2 && pair == taps_pairs - 1) {
				set_reg_field_value(
						data, 0,
						SCL_COEF_RAM_TAP_DATA,
						SCL_C_RAM_ODD_TAP_COEF_EN);
				array_idx++;
			} else {
				set_reg_field_value(
						data, 1,
						SCL_COEF_RAM_TAP_DATA,
						SCL_C_RAM_ODD_TAP_COEF_EN);
				set_reg_field_value(
						data, coeffs[array_idx + 1],
						SCL_COEF_RAM_TAP_DATA,
						SCL_C_RAM_ODD_TAP_COEF);

				array_idx += 2;
			}

			dm_write_reg(ctx, SCL_REG(mmSCL_COEF_RAM_TAP_DATA), data);
		}
	}

	/*We need to restore power gating on coeff memory to initial state*/
	dm_write_reg(ctx, DCFE_REG(mmDCFE_MEM_PWR_CTRL), power_ctl);
}

static void program_viewport(
	struct dce110_transform *xfm110,
	const struct rect *view_port)
{
	struct dc_context *ctx = xfm110->base.ctx;
	uint32_t value = 0;
	uint32_t addr = 0;

	addr = SCL_REG(mmVIEWPORT_START);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(
		value,
		view_port->x,
		VIEWPORT_START,
		VIEWPORT_X_START);
	set_reg_field_value(
		value,
		view_port->y,
		VIEWPORT_START,
		VIEWPORT_Y_START);
	dm_write_reg(ctx, addr, value);

	addr = SCL_REG(mmVIEWPORT_SIZE);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(
		value,
		view_port->height,
		VIEWPORT_SIZE,
		VIEWPORT_HEIGHT);
	set_reg_field_value(
		value,
		view_port->width,
		VIEWPORT_SIZE,
		VIEWPORT_WIDTH);
	dm_write_reg(ctx, addr, value);

	/* TODO: add stereo support */
}

static void calculate_inits(
	struct dce110_transform *xfm110,
	const struct scaler_data *data,
	struct scl_ratios_inits *inits)
{
	struct fixed31_32 h_init;
	struct fixed31_32 v_init;

	inits->h_int_scale_ratio =
		dal_fixed31_32_u2d19(data->ratios.horz) << 5;
	inits->v_int_scale_ratio =
		dal_fixed31_32_u2d19(data->ratios.vert) << 5;

	h_init =
		dal_fixed31_32_div_int(
			dal_fixed31_32_add(
				data->ratios.horz,
				dal_fixed31_32_from_int(data->taps.h_taps + 1)),
				2);
	inits->h_init.integer = dal_fixed31_32_floor(h_init);
	inits->h_init.fraction = dal_fixed31_32_u0d19(h_init) << 5;

	v_init =
		dal_fixed31_32_div_int(
			dal_fixed31_32_add(
				data->ratios.vert,
				dal_fixed31_32_from_int(data->taps.v_taps + 1)),
				2);
	inits->v_init.integer = dal_fixed31_32_floor(v_init);
	inits->v_init.fraction = dal_fixed31_32_u0d19(v_init) << 5;
}

static void program_scl_ratios_inits(
	struct dce110_transform *xfm110,
	struct scl_ratios_inits *inits)
{
	uint32_t addr = SCL_REG(mmSCL_HORZ_FILTER_SCALE_RATIO);
	uint32_t value = 0;

	set_reg_field_value(
		value,
		inits->h_int_scale_ratio,
		SCL_HORZ_FILTER_SCALE_RATIO,
		SCL_H_SCALE_RATIO);
	dm_write_reg(xfm110->base.ctx, addr, value);

	addr = SCL_REG(mmSCL_VERT_FILTER_SCALE_RATIO);
	value = 0;
	set_reg_field_value(
		value,
		inits->v_int_scale_ratio,
		SCL_VERT_FILTER_SCALE_RATIO,
		SCL_V_SCALE_RATIO);
	dm_write_reg(xfm110->base.ctx, addr, value);

	addr = SCL_REG(mmSCL_HORZ_FILTER_INIT);
	value = 0;
	set_reg_field_value(
		value,
		inits->h_init.integer,
		SCL_HORZ_FILTER_INIT,
		SCL_H_INIT_INT);
	set_reg_field_value(
		value,
		inits->h_init.fraction,
		SCL_HORZ_FILTER_INIT,
		SCL_H_INIT_FRAC);
	dm_write_reg(xfm110->base.ctx, addr, value);

	addr = SCL_REG(mmSCL_VERT_FILTER_INIT);
	value = 0;
	set_reg_field_value(
		value,
		inits->v_init.integer,
		SCL_VERT_FILTER_INIT,
		SCL_V_INIT_INT);
	set_reg_field_value(
		value,
		inits->v_init.fraction,
		SCL_VERT_FILTER_INIT,
		SCL_V_INIT_FRAC);
	dm_write_reg(xfm110->base.ctx, addr, value);

	addr = SCL_REG(mmSCL_AUTOMATIC_MODE_CONTROL);
	value = 0;
	set_reg_field_value(
		value,
		0,
		SCL_AUTOMATIC_MODE_CONTROL,
		SCL_V_CALC_AUTO_RATIO_EN);
	set_reg_field_value(
		value,
		0,
		SCL_AUTOMATIC_MODE_CONTROL,
		SCL_H_CALC_AUTO_RATIO_EN);
	dm_write_reg(xfm110->base.ctx, addr, value);
}

static const uint16_t *get_filter_coeffs_16p(int taps, struct fixed31_32 ratio)
{
	if (taps == 4)
		return get_filter_4tap_16p(ratio);
	else if (taps == 3)
		return get_filter_3tap_16p(ratio);
	else if (taps == 2)
		return filter_2tap_16p;
	else if (taps == 1)
		return NULL;
	else {
		/* should never happen, bug */
		BREAK_TO_DEBUGGER();
		return NULL;
	}
}

bool dce110_transform_set_scaler(
	struct transform *xfm,
	const struct scaler_data *data)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);
	bool is_scaling_required;
	bool filter_updated = false;
	const uint16_t *coeffs_v, *coeffs_h;

	disable_enhanced_sharpness(xfm110);

	/* 1. Program overscan */
	program_overscan(xfm110, data);

	/* 2. Program taps and configuration */
	is_scaling_required = setup_scaling_configuration(xfm110, data);

	if (is_scaling_required) {
		/* 3. Calculate and program ratio, filter initialization */
		struct scl_ratios_inits inits = { 0 };

		calculate_inits(xfm110, data, &inits);

		program_scl_ratios_inits(xfm110, &inits);

		coeffs_v = get_filter_coeffs_16p(data->taps.v_taps, data->ratios.vert);
		coeffs_h = get_filter_coeffs_16p(data->taps.h_taps, data->ratios.horz);

		if (coeffs_v != xfm110->filter_v || coeffs_h != xfm110->filter_h) {
			/* 4. Program vertical filters */
			if (xfm110->filter_v == NULL)
				program_two_taps_filter(xfm110, 0, true);
			program_multi_taps_filter(
					xfm110,
					data->taps.v_taps,
					coeffs_v,
					FILTER_TYPE_RGB_Y_VERTICAL);
			program_multi_taps_filter(
					xfm110,
					data->taps.v_taps,
					coeffs_v,
					FILTER_TYPE_ALPHA_VERTICAL);

			/* 5. Program horizontal filters */
			if (xfm110->filter_h == NULL)
				program_two_taps_filter(xfm110, 0, false);
			program_multi_taps_filter(
					xfm110,
					data->taps.h_taps,
					coeffs_h,
					FILTER_TYPE_RGB_Y_HORIZONTAL);
			program_multi_taps_filter(
					xfm110,
					data->taps.h_taps,
					coeffs_h,
					FILTER_TYPE_ALPHA_HORIZONTAL);

			xfm110->filter_v = coeffs_v;
			xfm110->filter_h = coeffs_h;
			filter_updated = true;
		}
	}

	/* 6. Program the viewport */
	program_viewport(xfm110, &data->viewport);

	/* 7. Set bit to flip to new coefficient memory */
	if (filter_updated)
		set_coeff_update_complete(xfm110);

	return true;
}

void dce110_transform_set_scaler_bypass(
		struct transform *xfm,
		const struct scaler_data *scl_data)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);
	uint32_t scl_mode;

	disable_enhanced_sharpness(xfm110);

	scl_mode = dm_read_reg(xfm->ctx, SCL_REG(mmSCL_MODE));
	set_reg_field_value(scl_mode, 0, SCL_MODE, SCL_MODE);
	set_reg_field_value(scl_mode, 0, SCL_MODE, SCL_PSCL_EN);
	dm_write_reg(xfm->ctx, SCL_REG(mmSCL_MODE), scl_mode);
}

void dce110_transform_set_scaler_filter(
	struct transform *xfm,
	struct scaler_filter *filter)
{
	xfm->filter = filter;
}

#define IDENTITY_RATIO(ratio) (dal_fixed31_32_u2d19(ratio) == (1 << 19))

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
		scl_data->lb_bpp,
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
