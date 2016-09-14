/*
 * dc_helper.c
 *
 *  Created on: Aug 30, 2016
 *      Author: agrodzov
 */
#include "dm_services.h"
#include <stdarg.h>

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


unsigned int generic_reg_wait(const struct dc_context *ctx,
	uint32_t addr, uint32_t mask, uint32_t shift, uint32_t condition_value,
	unsigned int delay_between_poll_us, unsigned int time_out_num_tries)
{
	uint32_t field_value;
	uint32_t reg_val;
	int i;

	reg_val = dm_read_reg(ctx, addr);

	for (i = 1; i <= time_out_num_tries; i++) {
		field_value = get_reg_field_value_ex(reg_val, mask, shift);

		if (field_value == condition_value)
			return i;

		if (0 < delay_between_poll_us && delay_between_poll_us < 1000)
			udelay(delay_between_poll_us);

		if (delay_between_poll_us > 1000)
			msleep(delay_between_poll_us/1000);

		reg_val = dm_read_reg(ctx, addr);
	}

	BREAK_TO_DEBUGGER();
	return 0;
}
