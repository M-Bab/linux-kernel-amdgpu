/*
 * Copyright 2015 Advanced Micro Devices, Inc.
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

/**
 * This file defines external dependencies of Display Core.
 */

#ifndef __DM_REG_UPDATE_H__
#define __DM_REG_UPDATE_H__

#include "dm_services.h"
#include <stdarg.h>

static void generic_reg_update_ex(const struct dc_context *ctx,
		uint32_t addr, uint32_t reg_val, int n, ...)
{
	int shift, mask, field_value;
	int i = 0;

	va_list ap;
	va_start(ap, n);

	 while (i < n) {
		shift = va_arg(ap, int);
		mask = va_arg(ap, int);
		field_value = va_arg(ap, int);

		reg_val = set_reg_field_value_ex(reg_val, field_value, mask, shift);
		i++;
	  }

	 dm_write_reg(ctx, addr, reg_val);
	 va_end(ap);


}

#define generic_reg_update(ctx, inst_offset, reg_name, n, ...)\
		generic_reg_update_ex(ctx, \
		mm##reg_name + inst_offset, dm_read_reg(ctx, mm##reg_name + inst_offset), n, \
		__VA_ARGS__)

#define generic_reg_set(ctx, inst_offset, reg_name, n, ...)\
		generic_reg_update_ex(ctx, \
		mm##reg_name + inst_offset, 0, n, \
		__VA_ARGS__)


#endif /* __DM_REG_UPDATE_H__ */
