/*
 * dc_helper.c
 *
 *  Created on: Aug 30, 2016
 *      Author: agrodzov
 */
#include "dm_services.h"

void generic_reg_update_ex(const struct dc_context *ctx,
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
