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
#include "reg_helper.h"
#include "dce110_transform.h"

#define SCL_PHASES 16

#define REG(reg) \
	(xfm110->regs->reg)

#undef FN
#define FN(reg_name, field_name) \
	xfm110->xfm_shift->field_name, xfm110->xfm_mask->field_name

#define CTX \
	xfm110->base.ctx

static void dce110_transform_set_scaler_bypass(
		struct transform *xfm,
		const struct scaler_data *scl_data)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);

	REG_UPDATE_2(SCL_MODE, SCL_MODE, 0, SCL_PSCL_EN, 0);
}

static bool setup_scaling_configuration(
	struct dce110_transform *xfm110,
	const struct scaler_data *data)
{
	struct dc_context *ctx = xfm110->base.ctx;

	if (data->taps.h_taps + data->taps.v_taps <= 2) {
		dce110_transform_set_scaler_bypass(&xfm110->base, NULL);
		return false;
	}

	REG_SET_2(SCL_TAP_CONTROL, 0,
			SCL_H_NUM_OF_TAPS, data->taps.h_taps - 1,
			SCL_V_NUM_OF_TAPS, data->taps.v_taps - 1);

	if (data->format <= PIXEL_FORMAT_GRPH_END)
		REG_UPDATE_2(SCL_MODE, SCL_MODE, 1, SCL_PSCL_EN, 1);
	else
		REG_UPDATE_2(SCL_MODE, SCL_MODE, 2, SCL_PSCL_EN, 1);

	/* 1 - Replace out of bound pixels with edge */
	REG_SET(SCL_CONTROL, 0, SCL_BOUNDARY_MODE, 1);

	return true;
}

static void program_overscan(
		struct dce110_transform *xfm110,
		const struct scaler_data *data)
{
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

	REG_SET_2(EXT_OVERSCAN_LEFT_RIGHT, 0,
			EXT_OVERSCAN_LEFT, data->recout.x,
			EXT_OVERSCAN_RIGHT, overscan_right);
	REG_SET_2(EXT_OVERSCAN_TOP_BOTTOM, 0,
			EXT_OVERSCAN_TOP, data->recout.y,
			EXT_OVERSCAN_BOTTOM, overscan_bottom);
}

static void program_multi_taps_filter(
	struct dce110_transform *xfm110,
	int taps,
	const uint16_t *coeffs,
	enum ram_filter_type filter_type)
{
	int phase, pair;
	int array_idx = 0;
	int taps_pairs = (taps + 1) / 2;
	int phases_to_program = SCL_PHASES / 2 + 1;

	uint32_t power_ctl = 0;

	if (!coeffs)
		return;

	/*We need to disable power gating on coeff memory to do programming*/
	if (REG(DCFE_MEM_PWR_CTRL)) {
		power_ctl = REG_READ(DCFE_MEM_PWR_CTRL);
		REG_SET(DCFE_MEM_PWR_CTRL, power_ctl, SCL_COEFF_MEM_PWR_DIS, 1);

		REG_WAIT(DCFE_MEM_PWR_STATUS, SCL_COEFF_MEM_PWR_STATE, 0, 1, 10);
	}
	for (phase = 0; phase < phases_to_program; phase++) {
		/*we always program N/2 + 1 phases, total phases N, but N/2-1 are just mirror
		phase 0 is unique and phase N/2 is unique if N is even*/
		for (pair = 0; pair < taps_pairs; pair++) {
			uint16_t odd_coeff = 0;

			REG_SET_3(SCL_COEF_RAM_SELECT, 0,
					SCL_C_RAM_FILTER_TYPE, filter_type,
					SCL_C_RAM_PHASE, phase,
					SCL_C_RAM_TAP_PAIR_IDX, pair);

			if (taps % 2 && pair == taps_pairs - 1)
				array_idx++;
			else {
				odd_coeff = coeffs[array_idx + 1];
				array_idx += 2;
			}

			REG_SET_4(SCL_COEF_RAM_TAP_DATA, 0,
					SCL_C_RAM_EVEN_TAP_COEF_EN, 1,
					SCL_C_RAM_EVEN_TAP_COEF, coeffs[array_idx],
					SCL_C_RAM_ODD_TAP_COEF_EN, 1,
					SCL_C_RAM_ODD_TAP_COEF, odd_coeff);
		}
	}

	/*We need to restore power gating on coeff memory to initial state*/
	if (REG(DCFE_MEM_PWR_CTRL))
		REG_WRITE(DCFE_MEM_PWR_CTRL, power_ctl);
}

static void program_viewport(
	struct dce110_transform *xfm110,
	const struct rect *view_port)
{
	REG_SET_2(VIEWPORT_START, 0,
			VIEWPORT_X_START, view_port->x,
			VIEWPORT_Y_START, view_port->y);

	REG_SET_2(VIEWPORT_SIZE, 0,
			VIEWPORT_HEIGHT, view_port->height,
			VIEWPORT_WIDTH, view_port->width);

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

	REG_SET(SCL_HORZ_FILTER_SCALE_RATIO, 0,
			SCL_H_SCALE_RATIO, inits->h_int_scale_ratio);

	REG_SET(SCL_VERT_FILTER_SCALE_RATIO, 0,
			SCL_V_SCALE_RATIO, inits->v_int_scale_ratio);

	REG_SET_2(SCL_HORZ_FILTER_INIT, 0,
			SCL_H_INIT_INT, inits->h_init.integer,
			SCL_H_INIT_FRAC, inits->h_init.fraction);

	REG_SET_2(SCL_VERT_FILTER_INIT, 0,
			SCL_V_INIT_INT, inits->v_init.integer,
			SCL_V_INIT_FRAC, inits->v_init.fraction);

	REG_WRITE(SCL_AUTOMATIC_MODE_CONTROL, 0);
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

void dce110_transform_set_scaler(
	struct transform *xfm,
	const struct scaler_data *data)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);
	bool is_scaling_required;
	bool filter_updated = false;
	const uint16_t *coeffs_v, *coeffs_h;

	/*Use all three pieces of memory always*/
	REG_SET_2(LB_MEMORY_CTRL, 0,
			LB_MEMORY_CONFIG, 0,
			LB_MEMORY_SIZE, xfm110->base.lb_memory_size);

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
				REG_SET(SCL_VERT_FILTER_CONTROL, 0,
						SCL_V_2TAP_HARDCODE_COEF_EN, 0);
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
				REG_SET(SCL_HORZ_FILTER_CONTROL, 0,
						SCL_H_2TAP_HARDCODE_COEF_EN, 0);
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
		REG_UPDATE(SCL_UPDATE, SCL_COEF_UPDATE_COMPLETE, 1);

	REG_UPDATE(LB_DATA_FORMAT, ALPHA_EN, data->lb_params.alpha_en);
}
