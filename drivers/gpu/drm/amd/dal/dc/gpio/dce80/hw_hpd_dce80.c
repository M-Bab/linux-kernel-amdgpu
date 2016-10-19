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
#include "../hw_gpio.h"
#include "../hw_hpd.h"

#include "hw_hpd_dce80.h"

#include "dce/dce_8_0_d.h"
#include "dce/dce_8_0_sh_mask.h"

#include "reg_helper.h"
#include "../hpd_regs.h"

#undef FN
#define FN(reg_name, field_name) \
	hpd->shifts->field_name, hpd->masks->field_name

#define CTX \
	hpd->base.base.ctx
#define REG(reg)\
	(hpd->regs->reg)

#define HPD_REG_LIST_DCE8(id) \
	HPD_GPIO_REG_LIST(id), \
	.int_status = mmDC_HPD ## id ## _INT_STATUS,\
	.toggle_filt_cntl = mmDC_HPD ## id ## _TOGGLE_FILT_CNTL

#define HPD_MASK_SH_LIST_DCE8(mask_sh) \
		.DC_HPD_SENSE_DELAYED = DC_HPD1_INT_STATUS__DC_HPD1_SENSE_DELAYED ## mask_sh,\
		.DC_HPD_SENSE = DC_HPD1_INT_STATUS__DC_HPD1_SENSE ## mask_sh,\
		.DC_HPD_CONNECT_INT_DELAY = DC_HPD1_TOGGLE_FILT_CNTL__DC_HPD1_CONNECT_INT_DELAY ## mask_sh,\
		.DC_HPD_DISCONNECT_INT_DELAY = DC_HPD1_TOGGLE_FILT_CNTL__DC_HPD1_DISCONNECT_INT_DELAY ## mask_sh

#define hpd_regs(id) \
{\
	HPD_REG_LIST_DCE8(id)\
}

static const struct hpd_registers hpd_regs[] = {
	hpd_regs(1),
	hpd_regs(2),
	hpd_regs(3),
	hpd_regs(4),
	hpd_regs(5),
	hpd_regs(6)
};

static const struct hpd_sh_mask hpd_shift = {
		HPD_MASK_SH_LIST_DCE8(__SHIFT)
};

static const struct hpd_sh_mask hpd_mask = {
		HPD_MASK_SH_LIST_DCE8(_MASK)
};

static void destruct(
	struct hw_hpd *hpd)
{
	dal_hw_hpd_destruct(hpd);
}

static void destroy(
	struct hw_gpio_pin **ptr)
{
	struct hw_hpd *hpd = HW_HPD_FROM_BASE(*ptr);

	destruct(hpd);
	dm_free(hpd);
	*ptr = NULL;
}

static enum gpio_result get_value(
	const struct hw_gpio_pin *ptr,
	uint32_t *value)
{
	struct hw_hpd *hpd = HW_HPD_FROM_BASE(ptr);
	uint32_t hpd_delayed = 0;

	/* in Interrupt mode we ask for SENSE bit */

	if (ptr->mode == GPIO_MODE_INTERRUPT) {

		REG_GET(int_status,
			DC_HPD_SENSE_DELAYED, &hpd_delayed);

		*value = hpd_delayed;
		return GPIO_RESULT_OK;
	}

	/* in any other modes, operate as normal GPIO */

	return dal_hw_gpio_get_value(ptr, value);
}

static enum gpio_result set_config(
	struct hw_gpio_pin *ptr,
	const struct gpio_config_data *config_data)
{
	struct hw_hpd *hpd = HW_HPD_FROM_BASE(ptr);

	if (!config_data)
		return GPIO_RESULT_INVALID_DATA;

	REG_UPDATE_2(toggle_filt_cntl,
		DC_HPD_CONNECT_INT_DELAY, config_data->config.hpd.delay_on_connect / 10,
		DC_HPD_DISCONNECT_INT_DELAY, config_data->config.hpd.delay_on_disconnect / 10);

	return GPIO_RESULT_OK;
}

static const struct hw_gpio_pin_funcs funcs = {
	.destroy = destroy,
	.open = dal_hw_gpio_open,
	.get_value = get_value,
	.set_value = dal_hw_gpio_set_value,
	.set_config = set_config,
	.change_mode = dal_hw_gpio_change_mode,
	.close = dal_hw_gpio_close,
};

static bool construct(
	struct hw_hpd *hpd,
	enum gpio_id id,
	uint32_t en,
	struct dc_context *ctx)
{
	if (id != GPIO_ID_HPD) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if ((en < GPIO_HPD_MIN) || (en > GPIO_HPD_MAX)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (!dal_hw_hpd_construct(hpd, id, en, ctx)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	hpd->base.base.funcs = &funcs;

	hpd->regs = &hpd_regs[en];
	hpd->shifts = &hpd_shift;
	hpd->masks = &hpd_mask;

	hpd->base.regs = &hpd_regs[en].gpio;

	return true;
}

struct hw_gpio_pin *dal_hw_hpd_dce80_create(
	struct dc_context *ctx,
	enum gpio_id id,
	uint32_t en)
{
	struct hw_hpd *hpd = dm_alloc(sizeof(struct hw_hpd));

	if (!hpd) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	if (construct(hpd, id, en, ctx))
		return &hpd->base.base;

	BREAK_TO_DEBUGGER();

	dm_free(hpd);

	return NULL;
}
