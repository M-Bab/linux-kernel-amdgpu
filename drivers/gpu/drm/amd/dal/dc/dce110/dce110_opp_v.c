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

#include "dce110_opp.h"
#include "dce110_opp_v.h"

#include "gamma_types.h"

/*****************************************/
/* Constructor, Destructor               */
/*****************************************/

struct opp_funcs funcs = {
		.opp_power_on_regamma_lut = dce110_opp_power_on_regamma_lut_v,

		.opp_program_regamma_pwl = dce110_opp_program_regamma_pwl_v,

		.opp_set_csc_default = dce110_opp_v_set_csc_default,

		.opp_set_csc_adjustment = dce110_opp_v_set_csc_adjustment,

		.opp_program_bit_depth_reduction =
				dce110_opp_program_bit_depth_reduction,

		.opp_program_clamping_and_pixel_encoding =
				dce110_opp_program_clamping_and_pixel_encoding,

		.opp_set_dyn_expansion = dce110_opp_set_dyn_expansion,

		.opp_set_regamma_mode = dce110_opp_set_regamma_mode,

		.opp_destroy = dce110_opp_destroy,
};

bool dce110_opp_v_construct(struct dce110_opp *opp110,
	struct dc_context *ctx,
	uint32_t inst,
	const struct dce110_opp_reg_offsets *offsets)
{
	opp110->base.funcs = &funcs;

	opp110->base.ctx = ctx;

	opp110->base.inst = inst;

	opp110->offsets = *offsets;

	return true;
}




