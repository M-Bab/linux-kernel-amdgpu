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
#include "include/gpio_interface.h"
#include "include/irq_interface.h"
#include "include/gpio_service_interface.h"
#include "hw_gpio_pin.h"
#include "hw_translate.h"
#include "hw_factory.h"
#include "gpio_service.h"
#include "gpio.h"

/*
 * Header of this unit
 */

#include "irq.h"

/*
 * Post-requisites: headers required by this unit
 */

/*
 * This unit
 */

enum gpio_result dal_irq_open(
	struct gpio *irq)
{
	return dal_gpio_open(irq, GPIO_MODE_INTERRUPT);
}

enum gpio_result dal_irq_get_value(
	const struct gpio *irq,
	uint32_t *value)
{
	return dal_gpio_get_value(irq, value);
}

enum dc_irq_source dal_irq_get_source(
	const struct gpio *irq)
{
	enum gpio_id id = dal_gpio_get_id(irq);

	switch (id) {
	case GPIO_ID_HPD:
		return (enum dc_irq_source)(DC_IRQ_SOURCE_HPD1 +
			dal_gpio_get_enum(irq));
	case GPIO_ID_GPIO_PAD:
		return (enum dc_irq_source)(DC_IRQ_SOURCE_GPIOPAD0 +
			dal_gpio_get_enum(irq));
	default:
		return DC_IRQ_SOURCE_INVALID;
	}
}

enum dc_irq_source dal_irq_get_rx_source(
	const struct gpio *irq)
{
	enum gpio_id id = dal_gpio_get_id(irq);

	switch (id) {
	case GPIO_ID_HPD:
		return (enum dc_irq_source)(DC_IRQ_SOURCE_HPD1RX +
			dal_gpio_get_enum(irq));
	default:
		return DC_IRQ_SOURCE_INVALID;
	}
}

enum gpio_result dal_irq_setup_hpd_filter(
	struct gpio *irq,
	struct gpio_hpd_config *config)
{
	struct gpio_config_data config_data;

	if (!config)
		return GPIO_RESULT_INVALID_DATA;

	config_data.type = GPIO_CONFIG_TYPE_HPD;
	config_data.config.hpd = *config;

	return dal_gpio_set_config(irq, &config_data);
}

void dal_irq_close(
	struct gpio *irq)
{
	dal_gpio_close(irq);
}

/*
 * @brief
 * Creation and destruction
 */

struct gpio *dal_gpio_create_irq(
	struct gpio_service *service,
	enum gpio_id id,
	uint32_t en)
{
	struct gpio *irq;

	switch (id) {
	case GPIO_ID_HPD:
	case GPIO_ID_GPIO_PAD:
	break;
	default:
		ASSERT_CRITICAL(false);
		return NULL;
	}

	irq = dal_gpio_service_create_gpio_ex(
		service, id, en, GPIO_PIN_OUTPUT_STATE_DEFAULT);

	if (irq)
		return irq;

	ASSERT_CRITICAL(false);
	return NULL;
}

static void destruct(struct gpio *irq)
{
	dal_gpio_close(irq);
	dal_gpio_service_destroy_gpio(&irq);

}

void dal_gpio_destroy_irq(
	struct gpio **irq)
{
	if (!irq || !*irq) {
		ASSERT_CRITICAL(false);
		return;
	}

	destruct(*irq);
	dm_free(*irq);

	*irq = NULL;
}
