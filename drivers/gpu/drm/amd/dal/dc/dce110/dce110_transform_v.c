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

#include "dce110_transform.h"
#include "dce110_transform_v.h"

#include "dce/dce_11_0_d.h"
#include "dce/dce_11_0_sh_mask.h"

#define NOT_IMPLEMENTED()  DAL_LOGGER_NOT_IMPL(LOG_MINOR_COMPONENT_CONTROLLER,\
			"TRANSFORM SCALER:%s()\n", __func__)

#define SCLV_PHASES 64

struct sclv_ratios_inits {
	uint32_t h_int_scale_ratio_luma;
	uint32_t h_int_scale_ratio_chroma;
	uint32_t v_int_scale_ratio_luma;
	uint32_t v_int_scale_ratio_chroma;
	struct init_int_and_frac h_init_luma;
	struct init_int_and_frac h_init_chroma;
	struct init_int_and_frac v_init_luma;
	struct init_int_and_frac v_init_chroma;
};

static void calculate_viewport(
		const struct scaler_data *scl_data,
		struct rect *luma_viewport,
		struct rect *chroma_viewport)
{
	/*Do not set chroma vp for rgb444 pixel format*/
	luma_viewport->x = scl_data->viewport.x - scl_data->viewport.x % 2;
	luma_viewport->y = scl_data->viewport.y - scl_data->viewport.y % 2;
	luma_viewport->width =
		scl_data->viewport.width - scl_data->viewport.width % 2;
	luma_viewport->height =
		scl_data->viewport.height - scl_data->viewport.height % 2;
	chroma_viewport->x = luma_viewport->x;
	chroma_viewport->y = luma_viewport->y;
	chroma_viewport->height = luma_viewport->height;
	chroma_viewport->width = luma_viewport->width;

	if (scl_data->format == PIXEL_FORMAT_420BPP12) {
		luma_viewport->height += luma_viewport->height % 2;
		luma_viewport->width += luma_viewport->width % 2;
		/*for 420 video chroma is 1/4 the area of luma, scaled
		 *vertically and horizontally
		 */
		chroma_viewport->x = luma_viewport->x / 2;
		chroma_viewport->y = luma_viewport->y / 2;
		chroma_viewport->height = luma_viewport->height / 2;
		chroma_viewport->width = luma_viewport->width / 2;
	}
}

static void program_viewport(
	struct dce110_transform *xfm110,
	struct rect *luma_view_port,
	struct rect *chroma_view_port)
{
	struct dc_context *ctx = xfm110->base.ctx;
	uint32_t value = 0;
	uint32_t addr = 0;

	if (luma_view_port->width != 0 && luma_view_port->height != 0) {
		addr = mmSCLV_VIEWPORT_START;
		value = 0;
		set_reg_field_value(
			value,
			luma_view_port->x,
			SCLV_VIEWPORT_START,
			VIEWPORT_X_START);
		set_reg_field_value(
			value,
			luma_view_port->y,
			SCLV_VIEWPORT_START,
			VIEWPORT_Y_START);
		dm_write_reg(ctx, addr, value);

		addr = mmSCLV_VIEWPORT_SIZE;
		value = 0;
		set_reg_field_value(
			value,
			luma_view_port->height,
			SCLV_VIEWPORT_SIZE,
			VIEWPORT_HEIGHT);
		set_reg_field_value(
			value,
			luma_view_port->width,
			SCLV_VIEWPORT_SIZE,
			VIEWPORT_WIDTH);
		dm_write_reg(ctx, addr, value);
	}

	if (chroma_view_port->width != 0 && chroma_view_port->height != 0) {
		addr = mmSCLV_VIEWPORT_START_C;
		value = 0;
		set_reg_field_value(
			value,
			chroma_view_port->x,
			SCLV_VIEWPORT_START_C,
			VIEWPORT_X_START_C);
		set_reg_field_value(
			value,
			chroma_view_port->y,
			SCLV_VIEWPORT_START_C,
			VIEWPORT_Y_START_C);
		dm_write_reg(ctx, addr, value);

		addr = mmSCLV_VIEWPORT_SIZE_C;
		value = 0;
		set_reg_field_value(
			value,
			chroma_view_port->height,
			SCLV_VIEWPORT_SIZE_C,
			VIEWPORT_HEIGHT_C);
		set_reg_field_value(
			value,
			chroma_view_port->width,
			SCLV_VIEWPORT_SIZE_C,
			VIEWPORT_WIDTH_C);
		dm_write_reg(ctx, addr, value);
	}
}

/*
 * Function:
 * void setup_scaling_configuration
 *
 * Purpose: setup scaling mode : bypass, RGb, YCbCr and nummber of taps
 * Input:   data
 *
 * Output:
 *  void
 */
static bool setup_scaling_configuration(
	struct dce110_transform *xfm110,
	const struct scaler_data *data)
{
	bool is_scaling_needed = false;
	struct dc_context *ctx = xfm110->base.ctx;
	uint32_t value = 0;

	set_reg_field_value(value, data->taps.h_taps - 1,
			SCLV_TAP_CONTROL, SCL_H_NUM_OF_TAPS);
	set_reg_field_value(value, data->taps.v_taps - 1,
			SCLV_TAP_CONTROL, SCL_V_NUM_OF_TAPS);
	set_reg_field_value(value, data->taps.h_taps_c - 1,
			SCLV_TAP_CONTROL, SCL_H_NUM_OF_TAPS_C);
	set_reg_field_value(value, data->taps.v_taps_c - 1,
			SCLV_TAP_CONTROL, SCL_V_NUM_OF_TAPS_C);
	dm_write_reg(ctx, mmSCLV_TAP_CONTROL, value);

	value = 0;
	if (data->taps.h_taps + data->taps.v_taps > 2) {
		set_reg_field_value(value, 1, SCLV_MODE, SCL_MODE);
		set_reg_field_value(value, 1, SCLV_MODE, SCL_PSCL_EN);
		is_scaling_needed = true;
	} else {
		set_reg_field_value(value, 0, SCLV_MODE, SCL_MODE);
		set_reg_field_value(value, 0, SCLV_MODE, SCL_PSCL_EN);
	}

	if (data->taps.h_taps_c + data->taps.v_taps_c > 2) {
		set_reg_field_value(value, 1, SCLV_MODE, SCL_MODE_C);
		set_reg_field_value(value, 1, SCLV_MODE, SCL_PSCL_EN_C);
		is_scaling_needed = true;
	} else if (data->format != PIXEL_FORMAT_420BPP12) {
		set_reg_field_value(
			value,
			get_reg_field_value(value, SCLV_MODE, SCL_MODE),
			SCLV_MODE,
			SCL_MODE_C);
		set_reg_field_value(
			value,
			get_reg_field_value(value, SCLV_MODE, SCL_PSCL_EN),
			SCLV_MODE,
			SCL_PSCL_EN_C);
	} else {
		set_reg_field_value(value, 0, SCLV_MODE, SCL_MODE_C);
		set_reg_field_value(value, 0, SCLV_MODE, SCL_PSCL_EN_C);
	}
	dm_write_reg(ctx, mmSCLV_MODE, value);

	value = 0;
	/*
	 * 0 - Replaced out of bound pixels with black pixel
	 * (or any other required color)
	 * 1 - Replaced out of bound pixels with the edge pixel
	 */
	set_reg_field_value(value, 1, SCLV_CONTROL, SCL_BOUNDARY_MODE);
	dm_write_reg(ctx, mmSCLV_CONTROL, value);

	return is_scaling_needed;
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

	int overscan_right = data->h_active - data->recout.x - data->recout.width;
	int overscan_bottom = data->v_active - data->recout.y - data->recout.height;

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
			mmSCLV_EXT_OVERSCAN_LEFT_RIGHT,
			overscan_left_right);

	dm_write_reg(xfm110->base.ctx,
			mmSCLV_EXT_OVERSCAN_TOP_BOTTOM,
			overscan_top_bottom);
}

static void set_coeff_update_complete(
		struct dce110_transform *xfm110)
{
	uint32_t value;

	value = dm_read_reg(xfm110->base.ctx, mmSCLV_UPDATE);
	set_reg_field_value(value, 1, SCLV_UPDATE, SCL_COEF_UPDATE_COMPLETE);
	dm_write_reg(xfm110->base.ctx, mmSCLV_UPDATE, value);
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
	int phases_to_program = SCLV_PHASES / 2 + 1;

	uint32_t select = 0;
	uint32_t power_ctl, power_ctl_off;

	if (!coeffs)
		return;

	/*We need to disable power gating on coeff memory to do programming*/
	power_ctl = dm_read_reg(ctx, mmDCFEV_MEM_PWR_CTRL);
	power_ctl_off = power_ctl;
	set_reg_field_value(power_ctl_off, 1, DCFEV_MEM_PWR_CTRL, SCLV_COEFF_MEM_PWR_DIS);
	dm_write_reg(ctx, mmDCFEV_MEM_PWR_CTRL, power_ctl_off);

	/*Wait to disable gating:*/
	for (i = 0; i < 10; i++) {
		if (get_reg_field_value(
				dm_read_reg(ctx, mmDCFEV_MEM_PWR_STATUS),
				DCFEV_MEM_PWR_STATUS,
				SCLV_COEFF_MEM_PWR_STATE) == 0)
			break;

		udelay(1);
	}

	set_reg_field_value(select, filter_type, SCLV_COEF_RAM_SELECT, SCL_C_RAM_FILTER_TYPE);

	for (phase = 0; phase < phases_to_program; phase++) {
		/*we always program N/2 + 1 phases, total phases N, but N/2-1 are just mirror
		phase 0 is unique and phase N/2 is unique if N is even*/
		set_reg_field_value(select, phase, SCLV_COEF_RAM_SELECT, SCL_C_RAM_PHASE);
		for (pair = 0; pair < taps_pairs; pair++) {
			uint32_t data = 0;

			set_reg_field_value(select, pair,
					SCLV_COEF_RAM_SELECT, SCL_C_RAM_TAP_PAIR_IDX);

			dm_write_reg(ctx, mmSCLV_COEF_RAM_SELECT, select);

			set_reg_field_value(
					data, 1,
					SCLV_COEF_RAM_TAP_DATA,
					SCL_C_RAM_EVEN_TAP_COEF_EN);
			set_reg_field_value(
					data, coeffs[array_idx],
					SCLV_COEF_RAM_TAP_DATA,
					SCL_C_RAM_EVEN_TAP_COEF);

			if (taps % 2 && pair == taps_pairs - 1) {
				set_reg_field_value(
						data, 0,
						SCLV_COEF_RAM_TAP_DATA,
						SCL_C_RAM_ODD_TAP_COEF_EN);
				array_idx++;
			} else {
				set_reg_field_value(
						data, 1,
						SCLV_COEF_RAM_TAP_DATA,
						SCL_C_RAM_ODD_TAP_COEF_EN);
				set_reg_field_value(
						data, coeffs[array_idx + 1],
						SCLV_COEF_RAM_TAP_DATA,
						SCL_C_RAM_ODD_TAP_COEF);

				array_idx += 2;
			}

			dm_write_reg(ctx, mmSCLV_COEF_RAM_TAP_DATA, data);
		}
	}

	/*We need to restore power gating on coeff memory to initial state*/
	dm_write_reg(ctx, mmDCFEV_MEM_PWR_CTRL, power_ctl);
}

static void calculate_inits(
	struct dce110_transform *xfm110,
	const struct scaler_data *data,
	struct sclv_ratios_inits *inits,
	struct rect *luma_viewport,
	struct rect *chroma_viewport)
{
	inits->h_int_scale_ratio_luma =
		dal_fixed31_32_u2d19(data->ratios.horz) << 5;
	inits->v_int_scale_ratio_luma =
		dal_fixed31_32_u2d19(data->ratios.vert) << 5;
	inits->h_int_scale_ratio_chroma =
		dal_fixed31_32_u2d19(data->ratios.horz_c) << 5;
	inits->v_int_scale_ratio_chroma =
		dal_fixed31_32_u2d19(data->ratios.vert_c) << 5;

	inits->h_init_luma.integer = 1;
	inits->v_init_luma.integer = 1;
	inits->h_init_chroma.integer = 1;
	inits->v_init_chroma.integer = 1;
}

static void program_scl_ratios_inits(
	struct dce110_transform *xfm110,
	struct sclv_ratios_inits *inits)
{
	struct dc_context *ctx = xfm110->base.ctx;
	uint32_t addr = mmSCLV_HORZ_FILTER_SCALE_RATIO;
	uint32_t value = 0;

	set_reg_field_value(
		value,
		inits->h_int_scale_ratio_luma,
		SCLV_HORZ_FILTER_SCALE_RATIO,
		SCL_H_SCALE_RATIO);
	dm_write_reg(ctx, addr, value);

	addr = mmSCLV_VERT_FILTER_SCALE_RATIO;
	value = 0;
	set_reg_field_value(
		value,
		inits->v_int_scale_ratio_luma,
		SCLV_VERT_FILTER_SCALE_RATIO,
		SCL_V_SCALE_RATIO);
	dm_write_reg(ctx, addr, value);

	addr = mmSCLV_HORZ_FILTER_SCALE_RATIO_C;
	value = 0;
	set_reg_field_value(
		value,
		inits->h_int_scale_ratio_chroma,
		SCLV_HORZ_FILTER_SCALE_RATIO_C,
		SCL_H_SCALE_RATIO_C);
	dm_write_reg(ctx, addr, value);

	addr = mmSCLV_VERT_FILTER_SCALE_RATIO_C;
	value = 0;
	set_reg_field_value(
		value,
		inits->v_int_scale_ratio_chroma,
		SCLV_VERT_FILTER_SCALE_RATIO_C,
		SCL_V_SCALE_RATIO_C);
	dm_write_reg(ctx, addr, value);

	addr = mmSCLV_HORZ_FILTER_INIT;
	value = 0;
	set_reg_field_value(
		value,
		inits->h_init_luma.fraction,
		SCLV_HORZ_FILTER_INIT,
		SCL_H_INIT_FRAC);
	set_reg_field_value(
		value,
		inits->h_init_luma.integer,
		SCLV_HORZ_FILTER_INIT,
		SCL_H_INIT_INT);
	dm_write_reg(ctx, addr, value);

	addr = mmSCLV_VERT_FILTER_INIT;
	value = 0;
	set_reg_field_value(
		value,
		inits->v_init_luma.fraction,
		SCLV_VERT_FILTER_INIT,
		SCL_V_INIT_FRAC);
	set_reg_field_value(
		value,
		inits->v_init_luma.integer,
		SCLV_VERT_FILTER_INIT,
		SCL_V_INIT_INT);
	dm_write_reg(ctx, addr, value);

	addr = mmSCLV_HORZ_FILTER_INIT_C;
	value = 0;
	set_reg_field_value(
		value,
		inits->h_init_chroma.fraction,
		SCLV_HORZ_FILTER_INIT_C,
		SCL_H_INIT_FRAC_C);
	set_reg_field_value(
		value,
		inits->h_init_chroma.integer,
		SCLV_HORZ_FILTER_INIT_C,
		SCL_H_INIT_INT_C);
	dm_write_reg(ctx, addr, value);

	addr = mmSCLV_VERT_FILTER_INIT_C;
	value = 0;
	set_reg_field_value(
		value,
		inits->v_init_chroma.fraction,
		SCLV_VERT_FILTER_INIT_C,
		SCL_V_INIT_FRAC_C);
	set_reg_field_value(
		value,
		inits->v_init_chroma.integer,
		SCLV_VERT_FILTER_INIT_C,
		SCL_V_INIT_INT_C);
	dm_write_reg(ctx, addr, value);
}

static const uint16_t *get_filter_coeffs_64p(int taps, struct fixed31_32 ratio)
{
	if (taps == 4)
		return get_filter_4tap_64p(ratio);
	else if (taps == 2)
		return filter_2tap_64p;
	else if (taps == 1)
		return NULL;
	else {
		/* should never happen, bug */
		BREAK_TO_DEBUGGER();
		return NULL;
	}
}

static void dce110_transform_v_set_scaler(
	struct transform *xfm,
	const struct scaler_data *data)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);
	bool is_scaling_required = false;
	bool filter_updated = false;
	const uint16_t *coeffs_v, *coeffs_h, *coeffs_h_c, *coeffs_v_c;
	struct rect luma_viewport = {0};
	struct rect chroma_viewport = {0};

	/* 1. Calculate viewport, viewport programming should happen after init
	 * calculations as they may require an adjustment in the viewport.
	 */

	calculate_viewport(data, &luma_viewport, &chroma_viewport);

	/* 2. Program overscan */
	program_overscan(xfm110, data);

	/* 3. Program taps and configuration */
	is_scaling_required = setup_scaling_configuration(xfm110, data);

	if (is_scaling_required) {
		/* 4. Calculate and program ratio, filter initialization */

		struct sclv_ratios_inits inits = { 0 };

		calculate_inits(
			xfm110,
			data,
			&inits,
			&luma_viewport,
			&chroma_viewport);

		program_scl_ratios_inits(xfm110, &inits);

		coeffs_v = get_filter_coeffs_64p(data->taps.v_taps, data->ratios.vert);
		coeffs_h = get_filter_coeffs_64p(data->taps.h_taps, data->ratios.horz);
		coeffs_v_c = get_filter_coeffs_64p(data->taps.v_taps_c, data->ratios.vert_c);
		coeffs_h_c = get_filter_coeffs_64p(data->taps.h_taps_c, data->ratios.horz_c);

		if (coeffs_v != xfm110->filter_v
				|| coeffs_v_c != xfm110->filter_v_c
				|| coeffs_h != xfm110->filter_h
				|| coeffs_h_c != xfm110->filter_h_c) {
		/* 5. Program vertical filters */
			program_multi_taps_filter(
					xfm110,
					data->taps.v_taps,
					coeffs_v,
					FILTER_TYPE_RGB_Y_VERTICAL);
			program_multi_taps_filter(
					xfm110,
					data->taps.v_taps_c,
					coeffs_v_c,
					FILTER_TYPE_CBCR_VERTICAL);

		/* 6. Program horizontal filters */
			program_multi_taps_filter(
					xfm110,
					data->taps.h_taps,
					coeffs_h,
					FILTER_TYPE_RGB_Y_HORIZONTAL);
			program_multi_taps_filter(
					xfm110,
					data->taps.h_taps_c,
					coeffs_h_c,
					FILTER_TYPE_CBCR_HORIZONTAL);

			xfm110->filter_v = coeffs_v;
			xfm110->filter_v_c = coeffs_v_c;
			xfm110->filter_h = coeffs_h;
			xfm110->filter_h_c = coeffs_h_c;
			filter_updated = true;
		}
	}

	/* 7. Program the viewport */
	program_viewport(xfm110, &luma_viewport, &chroma_viewport);

	/* 8. Set bit to flip to new coefficient memory */
	if (filter_updated)
		set_coeff_update_complete(xfm110);
}

static bool dce110_transform_v_power_up_line_buffer(struct transform *xfm)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);
	uint32_t value;

	value = dm_read_reg(xfm110->base.ctx, mmLBV_MEMORY_CTRL);

	/*Use all three pieces of memory always*/
	set_reg_field_value(value, 0, LBV_MEMORY_CTRL, LB_MEMORY_CONFIG);
	/*hard coded number DCE11 1712(0x6B0) Partitions: 720/960/1712*/
	set_reg_field_value(value, xfm110->base.lb_memory_size, LBV_MEMORY_CTRL,
			LB_MEMORY_SIZE);

	dm_write_reg(xfm110->base.ctx, mmLBV_MEMORY_CTRL, value);

	return true;
}

static void dce110_transform_v_reset(struct transform *xfm)
{
	struct dce110_transform *xfm110 = TO_DCE110_TRANSFORM(xfm);

	xfm110->filter_h = NULL;
	xfm110->filter_v = NULL;
	xfm110->filter_h_c = NULL;
	xfm110->filter_v_c = NULL;
}

static const struct transform_funcs dce110_transform_v_funcs = {
	.transform_reset =
			dce110_transform_v_reset,
	.transform_power_up =
		dce110_transform_v_power_up_line_buffer,
	.transform_set_scaler =
		dce110_transform_v_set_scaler,
	.transform_set_gamut_remap =
		dce110_transform_set_gamut_remap,
	.transform_set_pixel_storage_depth =
		dce110_transform_v_set_pixel_storage_depth,
	.transform_get_optimal_number_of_taps =
			dce110_transform_get_optimal_number_of_taps
};
/*****************************************/
/* Constructor, Destructor               */
/*****************************************/

bool dce110_transform_v_construct(
	struct dce110_transform *xfm110,
	struct dc_context *ctx)
{
	xfm110->base.ctx = ctx;

	xfm110->base.funcs = &dce110_transform_v_funcs;

	xfm110->lb_pixel_depth_supported =
			LB_PIXEL_DEPTH_18BPP |
			LB_PIXEL_DEPTH_24BPP |
			LB_PIXEL_DEPTH_30BPP;

	xfm110->base.lb_bits_per_entry = LB_BITS_PER_ENTRY;
	xfm110->base.lb_total_entries_num = LB_TOTAL_NUMBER_OF_ENTRIES;

	return true;
}
