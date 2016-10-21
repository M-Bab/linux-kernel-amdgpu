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
#include "../hw_ddc.h"

#include "hw_ddc_dce80.h"

#include "dce/dce_8_0_d.h"
#include "dce/dce_8_0_sh_mask.h"

#include "reg_helper.h"

#include "../ddc_regs.h"

 /* set field name */
#define SF(reg_name, field_name, post_fix)\
	.field_name = reg_name ## __ ## field_name ## post_fix

#undef FN
#define FN(reg_name, field_name) \
	ddc->shifts->field_name, ddc->masks->field_name

#define CTX \
	ddc->base.base.ctx
#define REG(reg)\
	(ddc->regs->reg)

static const struct ddc_registers ddc_data_regs[] = {
	ddc_data_regs(1),
	ddc_data_regs(2),
	ddc_data_regs(3),
	ddc_data_regs(4),
	ddc_data_regs(5),
	ddc_data_regs(6),
	ddc_vga_data_regs,
	ddc_i2c_data_regs
};

static const struct ddc_registers ddc_clk_regs[] = {
	ddc_clk_regs(1),
	ddc_clk_regs(2),
	ddc_clk_regs(3),
	ddc_clk_regs(4),
	ddc_clk_regs(5),
	ddc_clk_regs(6),
	ddc_vga_clk_regs,
	ddc_i2c_clk_regs
};

static const struct ddc_sh_mask ddc_shift = {
		DDC_MASK_SH_LIST(__SHIFT)
};

static const struct ddc_sh_mask ddc_mask = {
		DDC_MASK_SH_LIST(_MASK)
};

static void destruct(
	struct hw_ddc *pin)
{
	dal_hw_gpio_destruct(&pin->base);
}

static void destroy(
	struct hw_gpio_pin **ptr)
{
	struct hw_ddc *pin = HW_DDC_FROM_BASE(*ptr);

	destruct(pin);

	dm_free(pin);

	*ptr = NULL;
}

static enum gpio_result set_config(
	struct hw_gpio_pin *ptr,
	const struct gpio_config_data *config_data)
{
	struct hw_ddc *ddc = HW_DDC_FROM_BASE(ptr);
	struct hw_gpio *hw_gpio = NULL;
	uint32_t addr;
	uint32_t regval;
	uint32_t ddc_data_pd_en = 0;
	uint32_t ddc_clk_pd_en = 0;
	uint32_t aux_pad_mode = 0;

	hw_gpio = &ddc->base;

	if (hw_gpio == NULL) {
		ASSERT_CRITICAL(false);
		return GPIO_RESULT_NULL_HANDLE;
	}

	/* switch dual mode GPIO to I2C/AUX mode */
	addr = ddc->base.regs->MASK_reg;

	regval = REG_READ(gpio.MASK_reg);

	ddc_data_pd_en = get_reg_field_value_ex(
			regval,
			FN(,DC_GPIO_DDC1DATA_PD_EN));

	ddc_clk_pd_en = get_reg_field_value_ex(
			regval,
			FN(,DC_GPIO_DDC1CLK_PD_EN));

	aux_pad_mode = get_reg_field_value_ex(
			regval,
			FN(,AUX_PAD1_MODE));

	switch (config_data->config.ddc.type) {
	case GPIO_DDC_CONFIG_TYPE_MODE_I2C:
		/* On plug-in, there is a transient level on the pad
		 * which must be discharged through the internal pull-down.
		 * Enable internal pull-down, 2.5msec discharge time
		 * is required for detection of AUX mode */
		if (hw_gpio->base.en != GPIO_DDC_LINE_VIP_PAD) {
			if (!ddc_data_pd_en || !ddc_clk_pd_en) {
				set_reg_field_value_ex(
					regval,
					1,
					FN(,DC_GPIO_DDC1DATA_PD_EN));

				set_reg_field_value_ex(
					regval,
					1,
					FN(,DC_GPIO_DDC1CLK_PD_EN));

				REG_WRITE(gpio.MASK_reg, regval);

				if (config_data->type ==
					GPIO_CONFIG_TYPE_I2C_AUX_DUAL_MODE)
					/* should not affect normal I2C R/W */
					/* [anaumov] in DAL2, there was
					 * dc_service_delay_in_microseconds(2500); */
					msleep(3);
			}
		} else {
			uint32_t reg2 = regval;
			uint32_t sda_pd_dis = 0;
			uint32_t scl_pd_dis = 0;

			sda_pd_dis = get_reg_field_value_ex(
					reg2,
					FN(,DC_GPIO_SDA_PD_DIS));

			scl_pd_dis = get_reg_field_value_ex(
					reg2,
					FN(,DC_GPIO_SCL_PD_DIS));

			if (sda_pd_dis)
				sda_pd_dis = 0;

			if (!scl_pd_dis)
				scl_pd_dis = 1;

			if (sda_pd_dis || !scl_pd_dis) {
				REG_WRITE(gpio.MASK_reg, reg2);

				if (config_data->type ==
					GPIO_CONFIG_TYPE_I2C_AUX_DUAL_MODE)
					/* should not affect normal I2C R/W */
					/* [anaumov] in DAL2, there was
					 * dc_service_delay_in_microseconds(2500); */
					msleep(3);
			}
		}

		if (aux_pad_mode) {
			/* let pins to get de-asserted
			 * before setting pad to I2C mode */
			if (config_data->config.ddc.data_en_bit_present ||
				config_data->config.ddc.clock_en_bit_present)
				/* [anaumov] in DAL2, there was
				 * dc_service_delay_in_microseconds(2000); */
				msleep(2);

			/* set the I2C pad mode */
			/* read the register again,
			 * some bits may have been changed */
			REG_UPDATE(gpio.MASK_reg, AUX_PAD1_MODE, 1);
		}

		return GPIO_RESULT_OK;
	case GPIO_DDC_CONFIG_TYPE_MODE_AUX:
		/* set the AUX pad mode */
		if (!aux_pad_mode) {
			set_reg_field_value_ex(
				regval,
				1,
				FN(,AUX_PAD1_MODE));

			REG_WRITE(gpio.MASK_reg, regval);
		}

		return GPIO_RESULT_OK;
	case GPIO_DDC_CONFIG_TYPE_POLL_FOR_CONNECT:
		if ((hw_gpio->base.en >= GPIO_DDC_LINE_DDC1) &&
			(hw_gpio->base.en <= GPIO_DDC_LINE_DDC_VGA)) {
			REG_UPDATE_3(ddc_setup,
				DC_I2C_DDC1_ENABLE, 1,
				DC_I2C_DDC1_EDID_DETECT_ENABLE, 1,
				DC_I2C_DDC1_EDID_DETECT_MODE, 0);
			return GPIO_RESULT_OK;
		}
	break;
	case GPIO_DDC_CONFIG_TYPE_POLL_FOR_DISCONNECT:
		if ((hw_gpio->base.en >= GPIO_DDC_LINE_DDC1) &&
			(hw_gpio->base.en <= GPIO_DDC_LINE_DDC_VGA)) {
			REG_UPDATE_3(ddc_setup,
				DC_I2C_DDC1_ENABLE, 1,
				DC_I2C_DDC1_EDID_DETECT_ENABLE, 1,
				DC_I2C_DDC1_EDID_DETECT_MODE, 1);
			return GPIO_RESULT_OK;
		}
	break;
	case GPIO_DDC_CONFIG_TYPE_DISABLE_POLLING:
		if ((hw_gpio->base.en >= GPIO_DDC_LINE_DDC1) &&
			(hw_gpio->base.en <= GPIO_DDC_LINE_DDC_VGA)) {
			REG_UPDATE_2(ddc_setup,
				DC_I2C_DDC1_ENABLE, 0,
				DC_I2C_DDC1_EDID_DETECT_ENABLE, 0);
			return GPIO_RESULT_OK;
		}
	break;
	}

	BREAK_TO_DEBUGGER();

	return GPIO_RESULT_NON_SPECIFIC_ERROR;
}

static const struct hw_gpio_pin_funcs funcs = {
	.destroy = destroy,
	.open = dal_hw_gpio_open,
	.get_value = dal_hw_gpio_get_value,
	.set_value = dal_hw_gpio_set_value,
	.set_config = set_config,
	.change_mode = dal_hw_gpio_change_mode,
	.close = dal_hw_gpio_close,
};

void define_ddc_dce80_registers(
		struct hw_gpio_pin *pin,
		uint32_t en)
{
	struct hw_ddc *ddc = HW_DDC_FROM_BASE(pin);

	switch (pin->id) {
	case GPIO_ID_DDC_DATA:
		ddc->regs = &ddc_data_regs[en];
		ddc->base.regs = &ddc_data_regs[en].gpio;
		break;
	case GPIO_ID_DDC_CLOCK:
		ddc->regs = &ddc_clk_regs[en];
		ddc->base.regs = &ddc_clk_regs[en].gpio;
		break;
	default:
		ASSERT_CRITICAL(false);
		return;
	}

	ddc->shifts = &ddc_shift;
	ddc->masks = &ddc_mask;

}

static bool construct(
	struct hw_ddc *ddc,
	enum gpio_id id,
	uint32_t en,
	struct dc_context *ctx)
{
	if ((en < GPIO_DDC_LINE_MIN) || (en > GPIO_DDC_LINE_MAX)) {
		ASSERT_CRITICAL(false);
		return false;
	}

	if (!dal_hw_gpio_construct(&ddc->base, id, en, ctx)) {
		ASSERT_CRITICAL(false);
		return false;
	}

	ddc->base.base.funcs = &funcs;

	return true;
}

struct hw_gpio_pin *dal_hw_ddc_dce80_create(
	struct dc_context *ctx,
	enum gpio_id id,
	uint32_t en)
{
	struct hw_ddc *pin = dm_alloc(sizeof(struct hw_ddc));

	if (!pin) {
		ASSERT_CRITICAL(false);
		return NULL;
	}

	if (construct(pin, id, en, ctx))
		return &pin->base.base;

	ASSERT_CRITICAL(false);

	dm_free(pin);

	return NULL;
}
