/*
 * Copyright 2013-15 Advanced Micro Devices, Inc.
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

/*
 * Pre-requisites: headers required by header of this unit
 */

#include "dal_services.h"
#include "include/gpio_types.h"
#include "../hw_factory.h"

/*
 * Header of this unit
 */

#include "../hw_gpio_pin.h"
#include "../hw_gpio.h"
#include "../hw_ddc.h"
#include "../hw_hpd.h"

#include "hw_factory_dce110.h"
#include "hw_hpd_dce110.h"
#include "hw_ddc_dce110.h"

/* fucntion table */
static const struct hw_factory_funcs funcs = {
	.create_dvo = NULL,
	.create_ddc_data = dal_hw_ddc_dce110_create,
	.create_ddc_clock = dal_hw_ddc_dce110_create,
	.create_generic = NULL,
	.create_hpd = dal_hw_hpd_dce110_create,
	.create_gpio_pad = NULL,
	.create_sync = NULL,
	.create_gsl = NULL,
};

/*
 * dal_hw_factory_dce110_init
 *
 * @brief
 * Initialize HW factory function pointers and pin info
 *
 * @param
 * struct hw_factory *factory - [out] struct of function pointers
 */
void dal_hw_factory_dce110_init(struct hw_factory *factory)
{
	/*TODO check ASIC CAPs*/
	factory->number_of_pins[GPIO_ID_DVO1] = 24;
	factory->number_of_pins[GPIO_ID_DVO12] = 2;
	factory->number_of_pins[GPIO_ID_DVO24] = 1;
	factory->number_of_pins[GPIO_ID_DDC_DATA] = 8;
	factory->number_of_pins[GPIO_ID_DDC_CLOCK] = 8;
	factory->number_of_pins[GPIO_ID_GENERIC] = 7;
	factory->number_of_pins[GPIO_ID_HPD] = 6;
	factory->number_of_pins[GPIO_ID_GPIO_PAD] = 31;
	factory->number_of_pins[GPIO_ID_VIP_PAD] = 0;
	factory->number_of_pins[GPIO_ID_SYNC] = 2;
	factory->number_of_pins[GPIO_ID_GSL] = 4;

	factory->funcs = &funcs;
}
