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
#include "hw_hpd.h"

#include "reg_helper.h"
#include "gpio_regs.h"

#undef FN
#define FN(reg_name, field_name) \
	gpio->regs->field_name ## _shift, gpio->regs->field_name ## _mask

#define CTX \
	gpio->base.ctx
#define REG(reg)\
	(gpio->regs->reg)

static enum gpio_result config_mode(
	struct hw_gpio *gpio,
	enum gpio_mode mode)
{
	if (mode == GPIO_MODE_INTERRUPT) {
		/* Interrupt mode supported only by HPD (IrqGpio) pins. */
		gpio->base.mode = mode;
		REG_UPDATE(MASK_reg, MASK, 0);
		return GPIO_RESULT_OK;
	} else
		/* For any mode other than Interrupt,
		 * act as normal GPIO. */
		return dal_hw_gpio_config_mode(gpio, mode);
}

const struct hw_gpio_funcs hw_hpd_func = {
	.config_mode = config_mode,
};

bool dal_hw_hpd_construct(
	struct hw_hpd *pin,
	enum gpio_id id,
	uint32_t en,
	struct dc_context *ctx)
{
	if (!dal_hw_gpio_construct(&pin->base, id, en, ctx))
		return false;
	pin->base.funcs = &hw_hpd_func;
	return true;
}

void dal_hw_hpd_destruct(
	struct hw_hpd *pin)
{
	dal_hw_gpio_destruct(&pin->base);
}
