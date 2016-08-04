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

#ifndef __DAL_I2CAUX_INTERFACE_H__
#define __DAL_I2CAUX_INTERFACE_H__

#include "ddc_interface.h"
#include "adapter_service_interface.h"

struct i2c_payload {
	bool write;
	uint8_t address;
	uint8_t length;
	uint8_t *data;
};

enum i2c_command_engine {
	I2C_COMMAND_ENGINE_DEFAULT,
	I2C_COMMAND_ENGINE_SW,
	I2C_COMMAND_ENGINE_HW
};

struct i2c_command {
	struct i2c_payload *payloads;
	uint8_t number_of_payloads;

	enum i2c_command_engine engine;

	/* expressed in KHz
	 * zero means "use default value" */
	uint32_t speed;
};

#define DEFAULT_AUX_MAX_DATA_SIZE 16
#define AUX_MAX_DEFER_WRITE_RETRY 20

struct aux_payload {
	/* set following flag to read/write I2C data,
	 * reset it to read/write DPCD data */
	bool i2c_over_aux;
	/* set following flag to write data,
	 * reset it to read data */
	bool write;
	uint32_t address;
	uint8_t length;
	uint8_t *data;
};

struct aux_command {
	struct aux_payload *payloads;
	uint8_t number_of_payloads;

	/* expressed in milliseconds
	 * zero means "use default value" */
	uint32_t defer_delay;

	/* zero means "use default value" */
	uint32_t max_defer_write_retry;
};

union aux_config {
	struct {
		uint32_t ALLOW_AUX_WHEN_HPD_LOW:1;
	} bits;
	uint32_t raw;
};

struct i2caux;

struct i2caux *dal_i2caux_create(
	struct adapter_service *as,
	struct dc_context *ctx);

bool dal_i2caux_submit_i2c_command(
	struct i2caux *i2caux,
	struct ddc *ddc,
	struct i2c_command *cmd);

bool dal_i2caux_submit_aux_command(
	struct i2caux *i2caux,
	struct ddc *ddc,
	struct aux_command *cmd);

void dal_i2caux_keep_engine_power_up(
	struct i2caux *i2caux,
	struct ddc *ddc,
	bool keep_power_up);

bool dal_i2caux_start_gtc_sync(
	struct i2caux *i2caux,
	struct ddc *ddc);

bool dal_i2caux_stop_gtc_sync(
	struct i2caux *i2caux,
	struct ddc *ddc);

void dal_i2caux_configure_aux(
	struct i2caux *i2caux,
	struct ddc *ddc,
	union aux_config cfg);

void dal_i2caux_destroy(
	struct i2caux **ptr);

#endif
