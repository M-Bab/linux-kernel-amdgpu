/*
 * Copyright 2012-15 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
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

#include "atom.h"

#include "dce/dce_8_0_d.h"
#include "bif/bif_4_1_d.h"

#include "include/grph_object_id.h"
#include "include/grph_object_defs.h"
#include "include/grph_object_ctrl_defs.h"
#include "include/bios_parser_types.h"
#include "include/adapter_service_types.h"

#include "../bios_parser_helper.h"

/**
 * is_accelerated_mode
 *
 * @brief
 *  set Accelerated Mode in VBIOS scratch register, VBIOS will clean it when
 *  VGA/non-Accelerated mode is set
 *
 * @param
 *  NONE
 */
static bool is_accelerated_mode(
	struct dc_context *ctx)
{
	uint32_t addr = mmBIOS_SCRATCH_6;
	uint32_t value = dm_read_reg(ctx, addr);

	return (value & ATOM_S6_ACC_MODE) ? true : false;
}

static const struct bios_parser_helper bios_parser_helper_funcs = {
	.is_accelerated_mode = is_accelerated_mode,
};

const struct bios_parser_helper *dal_bios_parser_helper_dce80_get_table()
{
	return &bios_parser_helper_funcs;
}
