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

#include "hw_hpd_dce110.h"

#include "dce/dce_11_0_d.h"
#include "dce/dce_11_0_sh_mask.h"

/* set field name */
#define SF(reg_name, field_name, post_fix)\
	.field_name = reg_name ## __ ## field_name ## post_fix

#include "reg_helper.h"
#include "../hpd_regs.h"

#undef FN
#define FN(reg_name, field_name) \
	hpd->shifts->field_name, hpd->masks->field_name

#define CTX \
	hpd->base.base.ctx
#define REG(reg)\
	(hpd->regs->reg)


#define hpd_regs(id) \
{\
	HPD_REG_LIST(id)\
}

static const struct hpd_registers hpd_regs[] = {
	hpd_regs(0),
	hpd_regs(1),
	hpd_regs(2),
	hpd_regs(3),
	hpd_regs(4),
	hpd_regs(5)
};

static const struct hpd_sh_mask hpd_shift = {
		HPD_MASK_SH_LIST(__SHIFT)
};

static const struct hpd_sh_mask hpd_mask = {
		HPD_MASK_SH_LIST(_MASK)
};

static void destruct(
	struct hw_hpd_dce110 *pin)
{
	dal_hw_hpd_destruct(&pin->base);
}

static void destroy(
	struct hw_gpio_pin **ptr)
{
	struct hw_hpd_dce110 *pin = HPD_DCE110_FROM_BASE(*ptr);

	destruct(pin);

	dm_free(pin);

	*ptr = NULL;
}

struct hw_gpio_generic_dce110_init {
	struct hw_gpio_pin_reg hw_gpio_data_reg;
	struct hw_hpd_dce110_addr addr;
};

static const struct hw_gpio_generic_dce110_init
	hw_gpio_generic_dce110_init[GPIO_HPD_COUNT] = {
	/* GPIO_HPD_1 */
	{
		{
			{
				mmDC_GPIO_HPD_MASK,
				DC_GPIO_HPD_MASK__DC_GPIO_HPD1_MASK_MASK
			},
			{
				mmDC_GPIO_HPD_A,
				DC_GPIO_HPD_A__DC_GPIO_HPD1_A_MASK
			},
			{
				mmDC_GPIO_HPD_EN,
				DC_GPIO_HPD_EN__DC_GPIO_HPD1_EN_MASK
			},
			{
				mmDC_GPIO_HPD_Y,
				DC_GPIO_HPD_Y__DC_GPIO_HPD1_Y_MASK
			}
		},
		{
			mmHPD0_DC_HPD_INT_STATUS,
			mmHPD0_DC_HPD_TOGGLE_FILT_CNTL
		}
	},
	/* GPIO_HPD_2 */
	{
		{
			{
				mmDC_GPIO_HPD_MASK,
				DC_GPIO_HPD_MASK__DC_GPIO_HPD2_MASK_MASK
			},
			{
				mmDC_GPIO_HPD_A,
				DC_GPIO_HPD_A__DC_GPIO_HPD2_A_MASK
			},
			{
				mmDC_GPIO_HPD_EN,
				DC_GPIO_HPD_EN__DC_GPIO_HPD2_EN_MASK
			},
			{
				mmDC_GPIO_HPD_Y,
				DC_GPIO_HPD_Y__DC_GPIO_HPD2_Y_MASK
			}
		},
		{
			mmHPD1_DC_HPD_INT_STATUS,
			mmHPD1_DC_HPD_TOGGLE_FILT_CNTL
		}
	},
	/* GPIO_HPD_3 */
	{
		{
			{
				mmDC_GPIO_HPD_MASK,
				DC_GPIO_HPD_MASK__DC_GPIO_HPD3_MASK_MASK
			},
			{
				mmDC_GPIO_HPD_A,
				DC_GPIO_HPD_A__DC_GPIO_HPD3_A_MASK
			},
			{
				mmDC_GPIO_HPD_EN,
				DC_GPIO_HPD_EN__DC_GPIO_HPD3_EN_MASK
			},
			{
				mmDC_GPIO_HPD_Y,
				DC_GPIO_HPD_Y__DC_GPIO_HPD3_Y_MASK
			}
		},
		{
			mmHPD2_DC_HPD_INT_STATUS,
			mmHPD2_DC_HPD_TOGGLE_FILT_CNTL
		}
	},
	/* GPIO_HPD_4 */
	{
		{
			{
				mmDC_GPIO_HPD_MASK,
				DC_GPIO_HPD_MASK__DC_GPIO_HPD4_MASK_MASK
			},
			{
				mmDC_GPIO_HPD_A,
				DC_GPIO_HPD_A__DC_GPIO_HPD4_A_MASK
			},
			{
				mmDC_GPIO_HPD_EN,
				DC_GPIO_HPD_EN__DC_GPIO_HPD4_EN_MASK
			},
			{
				mmDC_GPIO_HPD_Y,
				DC_GPIO_HPD_Y__DC_GPIO_HPD4_Y_MASK
			}
		},
		{
			mmHPD3_DC_HPD_INT_STATUS,
			mmHPD3_DC_HPD_TOGGLE_FILT_CNTL
		}
	},
	/* GPIO_HPD_5 */
	{
		{
			{
				mmDC_GPIO_HPD_MASK,
				DC_GPIO_HPD_MASK__DC_GPIO_HPD5_MASK_MASK
			},
			{
				mmDC_GPIO_HPD_A,
				DC_GPIO_HPD_A__DC_GPIO_HPD5_A_MASK
			},
			{
				mmDC_GPIO_HPD_EN,
				DC_GPIO_HPD_EN__DC_GPIO_HPD5_EN_MASK
			},
			{
				mmDC_GPIO_HPD_Y,
				DC_GPIO_HPD_Y__DC_GPIO_HPD5_Y_MASK
			}
		},
		{
			mmHPD4_DC_HPD_INT_STATUS,
			mmHPD4_DC_HPD_TOGGLE_FILT_CNTL
		}
	},
	/* GPIO_HPD_6 */
	{
		{
			{
				mmDC_GPIO_HPD_MASK,
				DC_GPIO_HPD_MASK__DC_GPIO_HPD6_MASK_MASK
			},
			{
				mmDC_GPIO_HPD_A,
				DC_GPIO_HPD_A__DC_GPIO_HPD6_A_MASK
			},
			{
				mmDC_GPIO_HPD_EN,
				DC_GPIO_HPD_EN__DC_GPIO_HPD6_EN_MASK
			},
			{
				mmDC_GPIO_HPD_Y,
				DC_GPIO_HPD_Y__DC_GPIO_HPD6_Y_MASK
			}
		},
		{
			mmHPD5_DC_HPD_INT_STATUS,
			mmHPD5_DC_HPD_TOGGLE_FILT_CNTL
		}
	}
};

static enum gpio_result get_value(
	const struct hw_gpio_pin *ptr,
	uint32_t *value)
{
	struct hw_hpd_dce110 *pin = HPD_DCE110_FROM_BASE(ptr);
	struct hw_hpd *hpd = &pin->base;
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
	struct hw_hpd_dce110 *pin = HPD_DCE110_FROM_BASE(ptr);
	struct hw_hpd *hpd = &pin->base;

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
	struct hw_hpd_dce110 *pin,
	enum gpio_id id,
	uint32_t en,
	struct dc_context *ctx)
{
	const struct hw_gpio_generic_dce110_init *init;
	struct hw_hpd *hpd = &pin->base;

	if (id != GPIO_ID_HPD) {
		ASSERT_CRITICAL(false);
		return false;
	}

	if ((en < GPIO_HPD_MIN) || (en > GPIO_HPD_MAX)) {
		ASSERT_CRITICAL(false);
		return false;
	}

	if (!dal_hw_hpd_construct(&pin->base, id, en, ctx)) {
		ASSERT_CRITICAL(false);
		return false;
	}

	pin->base.base.base.funcs = &funcs;

	init = hw_gpio_generic_dce110_init + en;

	pin->base.base.pin_reg = init->hw_gpio_data_reg;

	pin->addr = init->addr;

	hpd->regs = &hpd_regs[en];
	hpd->shifts = &hpd_shift;
	hpd->masks = &hpd_mask;

	hpd->base.regs = &hpd_regs[en].gpio;

	return true;
}

struct hw_gpio_pin *dal_hw_hpd_dce110_create(
	struct dc_context *ctx,
	enum gpio_id id,
	uint32_t en)
{
	struct hw_hpd_dce110 *pin = dm_alloc(sizeof(struct hw_hpd_dce110));

	if (!pin) {
		ASSERT_CRITICAL(false);
		return NULL;
	}

	if (construct(pin, id, en, ctx))
		return &pin->base.base.base;

	ASSERT_CRITICAL(false);

	dm_free(pin);

	return NULL;
}
