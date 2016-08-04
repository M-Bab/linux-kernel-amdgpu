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

/*
 * Pre-requisites: headers required by header of this unit
 */
#include "include/gpio_types.h"
#include "../hw_gpio_pin.h"
#include "../hw_gpio.h"
#include "../hw_hpd.h"

/*
 * Header of this unit
 */

#include "hw_hpd_dce80.h"

/*
 * Post-requisites: headers required by this unit
 */

#include "dce/dce_8_0_d.h"
#include "dce/dce_8_0_sh_mask.h"


/*
 * This unit
 */

#define FROM_HW_HPD(ptr) \
	container_of((ptr), struct hw_hpd_dce80, base)

#define FROM_HW_GPIO(ptr) \
	FROM_HW_HPD(container_of((ptr), struct hw_hpd, base))

#define FROM_HW_GPIO_PIN(ptr) \
	FROM_HW_GPIO(container_of((ptr), struct hw_gpio, base))

static void destruct(
	struct hw_hpd_dce80 *pin)
{
	dal_hw_hpd_destruct(&pin->base);
}

static void destroy(
	struct hw_gpio_pin **ptr)
{
	struct hw_hpd_dce80 *pin = FROM_HW_GPIO_PIN(*ptr);

	destruct(pin);

	dm_free((*ptr)->ctx, pin);

	*ptr = NULL;
}

static enum gpio_result get_value(
	const struct hw_gpio_pin *ptr,
	uint32_t *value)
{
	struct hw_hpd_dce80 *pin = FROM_HW_GPIO_PIN(ptr);

	/* in Interrupt mode we ask for SENSE bit */

	if (ptr->mode == GPIO_MODE_INTERRUPT) {
		uint32_t regval;
		uint32_t hpd_delayed = 0;
		uint32_t hpd_sense = 0;

		regval = dm_read_reg(
				ptr->ctx,
				pin->addr.DC_HPD_INT_STATUS);

		hpd_delayed = get_reg_field_value(
				regval,
				DC_HPD1_INT_STATUS,
				DC_HPD1_SENSE_DELAYED);

		hpd_sense = get_reg_field_value(
				regval,
				DC_HPD1_INT_STATUS,
				DC_HPD1_SENSE);

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
	struct hw_hpd_dce80 *pin = FROM_HW_GPIO_PIN(ptr);

	if (!config_data)
		return GPIO_RESULT_INVALID_DATA;

	{
		uint32_t value;

		value = dm_read_reg(
			ptr->ctx,
			pin->addr.DC_HPD_TOGGLE_FILT_CNTL);

		set_reg_field_value(
			value,
			config_data->config.hpd.delay_on_connect / 10,
			DC_HPD1_TOGGLE_FILT_CNTL,
			DC_HPD1_CONNECT_INT_DELAY);

		set_reg_field_value(
			value,
			config_data->config.hpd.delay_on_disconnect / 10,
			DC_HPD1_TOGGLE_FILT_CNTL,
			DC_HPD1_DISCONNECT_INT_DELAY);

		dm_write_reg(
			ptr->ctx,
			pin->addr.DC_HPD_TOGGLE_FILT_CNTL,
			value);

	}

	return GPIO_RESULT_OK;
}

struct hw_gpio_generic_dce80_init {
	struct hw_gpio_pin_reg hw_gpio_data_reg;
	struct hw_hpd_dce80_addr addr;
};

static const struct hw_gpio_generic_dce80_init
	hw_gpio_generic_dce80_init[GPIO_HPD_COUNT] = {
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
			mmDC_HPD1_INT_STATUS,
			mmDC_HPD1_TOGGLE_FILT_CNTL
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
			mmDC_HPD2_INT_STATUS,
			mmDC_HPD2_TOGGLE_FILT_CNTL
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
			mmDC_HPD3_INT_STATUS,
			mmDC_HPD3_TOGGLE_FILT_CNTL
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
			mmDC_HPD4_INT_STATUS,
			mmDC_HPD4_TOGGLE_FILT_CNTL
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
			mmDC_HPD5_INT_STATUS,
			mmDC_HPD5_TOGGLE_FILT_CNTL
		}
	},
	/* GPIO_HPD_1 */
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
			mmDC_HPD6_INT_STATUS,
			mmDC_HPD6_TOGGLE_FILT_CNTL
		}
	}
};

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
	struct hw_hpd_dce80 *pin,
	enum gpio_id id,
	uint32_t en,
	struct dc_context *ctx)
{
	const struct hw_gpio_generic_dce80_init *init;

	if (id != GPIO_ID_HPD) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if ((en < GPIO_HPD_MIN) || (en > GPIO_HPD_MAX)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (!dal_hw_hpd_construct(&pin->base, id, en, ctx)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	pin->base.base.base.funcs = &funcs;

	init = hw_gpio_generic_dce80_init + en;

	pin->base.base.pin_reg = init->hw_gpio_data_reg;

	pin->addr = init->addr;

	return true;
}

struct hw_gpio_pin *dal_hw_hpd_dce80_create(
	struct dc_context *ctx,
	enum gpio_id id,
	uint32_t en)
{
	struct hw_hpd_dce80 *pin = dm_alloc(ctx, sizeof(struct hw_hpd_dce80));

	if (!pin) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	if (construct(pin, id, en, ctx))
		return &pin->base.base.base;

	BREAK_TO_DEBUGGER();

	dm_free(ctx, pin);

	return NULL;
}
