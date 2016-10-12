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

#include "include/bios_parser_types.h"
#include "include/adapter_service_types.h"
#include "include/logger_interface.h"

#include "../bios_parser_helper.h"

#include "dce/dce_11_0_d.h"
#include "bif/bif_5_1_d.h"

/**
 * set_scratch_acc_mode_change
 *
 * @brief
 *  set Accelerated Mode in VBIOS scratch register, VBIOS will clean it when
 *  VGA/non-Accelerated mode is set
 *
 * @param
 *  struct dc_context *ctx - [in] DAL context
 */
void dce110_set_scratch_acc_mode_change(struct dc_context *ctx)
{
	uint32_t addr = mmBIOS_SCRATCH_6;
	uint32_t value = 0;

	value = dm_read_reg(ctx, addr);

	value |= ATOM_S6_ACC_MODE;

	dm_write_reg(ctx, addr, value);
}

/*
 * is_accelerated_mode
 *
 * @brief
 *  set Accelerated Mode in VBIOS scratch register, VBIOS will clean it when
 *  VGA/non-Accelerated mode is set
 *
 * @param
 * struct dc_context *ctx
 *
 * @return
 * true if in acceleration mode, false otherwise.
 */
static bool is_accelerated_mode(
	struct dc_context *ctx)
{
	uint32_t addr = mmBIOS_SCRATCH_6;
	uint32_t value = dm_read_reg(ctx, addr);

	return (value & ATOM_S6_ACC_MODE) ? true : false;
}

#define BIOS_SCRATCH0_DAC_B_SHIFT 8

void dce110_set_scratch_critical_state(struct dc_context *ctx,
				       bool state)
{
	uint32_t addr = mmBIOS_SCRATCH_6;
	uint32_t value = dm_read_reg(ctx, addr);

	if (state)
		value |= ATOM_S6_CRITICAL_STATE;
	else
		value &= ~ATOM_S6_CRITICAL_STATE;

	dm_write_reg(ctx, addr, value);
}

/* function table */
static const struct bios_parser_helper bios_parser_helper_funcs = {
	.is_accelerated_mode = is_accelerated_mode,
};

/*
 * dal_bios_parser_dce110_init_bios_helper
 *
 * @brief
 * Initialize BIOS helper functions
 *
 * @param
 * const struct command_table_helper **h - [out] struct of functions
 *
 */

const struct bios_parser_helper *dal_bios_parser_helper_dce110_get_table()
{
	return &bios_parser_helper_funcs;
}
