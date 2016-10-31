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

/* include DCE8 register header files */
#include "dce/dce_8_0_d.h"
#include "dce/dce_8_0_sh_mask.h"

#include "dc_types.h"
#include "core_types.h"

#include "include/grph_object_id.h"
#include "include/fixed31_32.h"
#include "include/logger_interface.h"

#include "dce80_transform.h"

#include "dce80_transform_bit_depth.h"

bool dce80_transform_get_optimal_number_of_taps(
	struct transform *xfm,
	struct scaler_data *scl_data,
	const struct scaling_taps *in_taps)
{

	return transform_get_optimal_number_of_taps_helper(
			xfm,
			scl_data,
			scl_data->viewport.width,
			in_taps);

	return true;
}

static void dce80_transform_reset(struct transform *xfm)
{
}


static const struct transform_funcs dce80_transform_funcs = {
	.transform_reset = dce80_transform_reset,
	.transform_power_up =
		dce80_transform_power_up,
	.transform_set_scaler =
		dce80_transform_set_scaler,
	.transform_set_scaler_bypass =
		dce80_transform_set_scaler_bypass,
	.transform_set_scaler_filter =
		dce80_transform_set_scaler_filter,
	.transform_set_gamut_remap =
		dce80_transform_set_gamut_remap,
	.transform_set_pixel_storage_depth =
		dce80_transform_set_pixel_storage_depth,
	.transform_get_current_pixel_storage_depth =
		dce80_transform_get_current_pixel_storage_depth,
	.transform_get_optimal_number_of_taps =
		dce80_transform_get_optimal_number_of_taps
};

/*****************************************/
/* Constructor, Destructor               */
/*****************************************/

bool dce80_transform_construct(
	struct dce80_transform *xfm80,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dce80_transform_reg_offsets *reg_offsets)
{
	xfm80->base.ctx = ctx;

	xfm80->base.inst = inst;
	xfm80->base.funcs = &dce80_transform_funcs;

	xfm80->offsets = *reg_offsets;

	xfm80->lb_pixel_depth_supported =
			LB_PIXEL_DEPTH_18BPP |
			LB_PIXEL_DEPTH_24BPP |
			LB_PIXEL_DEPTH_30BPP;

	xfm80->base.lb_bits_per_entry = LB_BITS_PER_ENTRY;
	xfm80->base.lb_total_entries_num = LB_TOTAL_NUMBER_OF_ENTRIES;

	xfm80->base.lb_memory_size = 0x6B0; /*1712*/

	return true;
}

bool dce80_transform_power_up(struct transform *xfm)
{
	return dce80_transform_power_up_line_buffer(xfm);
}

