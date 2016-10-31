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

const uint16_t filter_2tap_64p[66] = {
	4096, 0,
	4032, 64,
	3968, 128,
	3904, 192,
	3840, 256,
	3776, 320,
	3712, 384,
	3648, 448,
	3584, 512,
	3520, 576,
	3456, 640,
	3392, 704,
	3328, 768,
	3264, 832,
	3200, 896,
	3136, 960,
	3072, 1024,
	3008, 1088,
	2944, 1152,
	2880, 1216,
	2816, 1280,
	2752, 1344,
	2688, 1408,
	2624, 1472,
	2560, 1536,
	2496, 1600,
	2432, 1664,
	2368, 1728,
	2304, 1792,
	2240, 1856,
	2176, 1920,
	2112, 1984,
	2048, 2048 };

const uint16_t filter_4tap_64p_upscale[132] = {
	0, 4096, 0, 0,
	16344, 4092, 40, 0,
	16308, 4084, 84, 16380,
	16272, 4072, 132, 16380,
	16240, 4056, 180, 16380,
	16212, 4036, 232, 16376,
	16184, 4012, 288, 16372,
	16160, 3984, 344, 16368,
	16136, 3952, 404, 16364,
	16116, 3916, 464, 16360,
	16100, 3872, 528, 16356,
	16084, 3828, 596, 16348,
	16072, 3780, 664, 16344,
	16060, 3728, 732, 16336,
	16052, 3676, 804, 16328,
	16044, 3616, 876, 16320,
	16040, 3556, 952, 16312,
	16036, 3492, 1028, 16300,
	16032, 3424, 1108, 16292,
	16032, 3356, 1188, 16280,
	16036, 3284, 1268, 16272,
	16036, 3212, 1352, 16260,
	16040, 3136, 1436, 16248,
	16044, 3056, 1520, 16236,
	16052, 2980, 1604, 16224,
	16060, 2896, 1688, 16212,
	16064, 2816, 1776, 16200,
	16076, 2732, 1864, 16188,
	16084, 2648, 1952, 16176,
	16092, 2564, 2040, 16164,
	16104, 2476, 2128, 16152,
	16116, 2388, 2216, 16140,
	16128, 2304, 2304, 16128 };

const uint16_t filter_4tap_64p_117[132] = {
	420, 3248, 420, 0,
	380, 3248, 464, 16380,
	344, 3248, 508, 16372,
	308, 3248, 552, 16368,
	272, 3240, 596, 16364,
	236, 3236, 644, 16356,
	204, 3224, 692, 16352,
	172, 3212, 744, 16344,
	144, 3196, 796, 16340,
	116, 3180, 848, 16332,
	88, 3160, 900, 16324,
	60, 3136, 956, 16320,
	36, 3112, 1012, 16312,
	16, 3084, 1068, 16304,
	16380, 3056, 1124, 16296,
	16360, 3024, 1184, 16292,
	16340, 2992, 1244, 16284,
	16324, 2956, 1304, 16276,
	16308, 2920, 1364, 16268,
	16292, 2880, 1424, 16264,
	16280, 2836, 1484, 16256,
	16268, 2792, 1548, 16252,
	16256, 2748, 1608, 16244,
	16248, 2700, 1668, 16240,
	16240, 2652, 1732, 16232,
	16232, 2604, 1792, 16228,
	16228, 2552, 1856, 16224,
	16220, 2500, 1916, 16220,
	16216, 2444, 1980, 16216,
	16216, 2388, 2040, 16216,
	16212, 2332, 2100, 16212,
	16212, 2276, 2160, 16212,
	16212, 2220, 2220, 16212 };

const uint16_t filter_4tap_64p_150[132] = {
	696, 2700, 696, 0,
	660, 2704, 732, 16380,
	628, 2704, 768, 16376,
	596, 2704, 804, 16372,
	564, 2700, 844, 16364,
	532, 2696, 884, 16360,
	500, 2692, 924, 16356,
	472, 2684, 964, 16352,
	440, 2676, 1004, 16352,
	412, 2668, 1044, 16348,
	384, 2656, 1088, 16344,
	360, 2644, 1128, 16340,
	332, 2632, 1172, 16336,
	308, 2616, 1216, 16336,
	284, 2600, 1260, 16332,
	260, 2580, 1304, 16332,
	236, 2560, 1348, 16328,
	216, 2540, 1392, 16328,
	196, 2516, 1436, 16328,
	176, 2492, 1480, 16324,
	156, 2468, 1524, 16324,
	136, 2440, 1568, 16328,
	120, 2412, 1612, 16328,
	104, 2384, 1656, 16328,
	88, 2352, 1700, 16332,
	72, 2324, 1744, 16332,
	60, 2288, 1788, 16336,
	48, 2256, 1828, 16340,
	36, 2220, 1872, 16344,
	24, 2184, 1912, 16352,
	12, 2148, 1952, 16356,
	4, 2112, 1996, 16364,
	16380, 2072, 2036, 16372 };

const uint16_t filter_4tap_64p_183[132] = {
	944, 2204, 944, 0,
	916, 2204, 972, 0,
	888, 2200, 996, 0,
	860, 2200, 1024, 4,
	832, 2196, 1052, 4,
	808, 2192, 1080, 8,
	780, 2188, 1108, 12,
	756, 2180, 1140, 12,
	728, 2176, 1168, 16,
	704, 2168, 1196, 20,
	680, 2160, 1224, 24,
	656, 2152, 1252, 28,
	632, 2144, 1280, 36,
	608, 2132, 1308, 40,
	584, 2120, 1336, 48,
	560, 2112, 1364, 52,
	536, 2096, 1392, 60,
	516, 2084, 1420, 68,
	492, 2072, 1448, 76,
	472, 2056, 1476, 84,
	452, 2040, 1504, 92,
	428, 2024, 1532, 100,
	408, 2008, 1560, 112,
	392, 1992, 1584, 120,
	372, 1972, 1612, 132,
	352, 1956, 1636, 144,
	336, 1936, 1664, 156,
	316, 1916, 1688, 168,
	300, 1896, 1712, 180,
	284, 1876, 1736, 192,
	268, 1852, 1760, 208,
	252, 1832, 1784, 220,
	236, 1808, 1808, 236 };

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

const uint16_t *get_filter_4tap_64p(struct fixed31_32 ratio)
{
	if (ratio.value < dal_fixed31_32_one.value)
		return filter_4tap_64p_upscale;
	else if (ratio.value < dal_fixed31_32_from_fraction(4, 3).value)
		return filter_4tap_64p_117;
	else if (ratio.value < dal_fixed31_32_from_fraction(5, 3).value)
		return filter_4tap_64p_150;
	else
		return filter_4tap_64p_183;
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

static void dce110_transform_v_set_scalerv_bypass(
		struct transform *xfm,
		const struct scaler_data *scl_data)
{
	uint32_t addr = mmSCLV_MODE;
	uint32_t value = dm_read_reg(xfm->ctx, addr);

	set_reg_field_value(value, 0, SCLV_MODE, SCL_MODE);
	set_reg_field_value(value, 0, SCLV_MODE, SCL_MODE_C);
	set_reg_field_value(value, 0, SCLV_MODE, SCL_PSCL_EN);
	set_reg_field_value(value, 0, SCLV_MODE, SCL_PSCL_EN_C);
	dm_write_reg(xfm->ctx, addr, value);
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

static bool dce110_transform_v_set_scaler(
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

	return true;
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
	.transform_set_scaler_bypass =
		dce110_transform_v_set_scalerv_bypass,
	.transform_set_scaler_filter =
		dce110_transform_set_scaler_filter,
	.transform_set_gamut_remap =
		dce110_transform_set_gamut_remap,
	.transform_set_pixel_storage_depth =
		dce110_transform_v_set_pixel_storage_depth,
	.transform_get_current_pixel_storage_depth =
		dce110_transform_v_get_current_pixel_storage_depth,
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
