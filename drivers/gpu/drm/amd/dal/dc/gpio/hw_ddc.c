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

#include "include/gpio_types.h"
#include "hw_gpio.h"
#include "hw_ddc.h"

#include "reg_helper.h"
#include "gpio_regs.h"

#undef FN
#define FN(reg_name, field_name) \
	gpio->regs->field_name ## _shift, gpio->regs->field_name ## _mask

#define CTX \
	gpio->base.ctx
#define REG(reg)\
	(gpio->regs->reg)

#define FROM_HW_GPIO(ptr) \
	container_of((ptr), struct hw_ddc, base)

#define FROM_HW_GPIO_PIN(ptr) \
	FROM_HW_GPIO(container_of((ptr), struct hw_gpio, base))

bool dal_hw_ddc_open(
	struct hw_gpio_pin *ptr,
	enum gpio_mode mode,
	void *options)
{
	struct hw_ddc *pin = FROM_HW_GPIO_PIN(ptr);
	struct hw_gpio *gpio = &pin->base;
	uint32_t en;

	if (!options) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	/* get the EN bit before overwriting it */
	REG_GET(EN_reg, EN, &en);

	((struct gpio_ddc_open_options *)options)->en_bit_present = (en != 0);

	return dal_hw_gpio_open(ptr, mode, options);
}
