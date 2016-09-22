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

#ifndef __DAL_I2C_HW_ENGINE_DCE110_H__
#define __DAL_I2C_HW_ENGINE_DCE110_H__

#define I2C_HW_ENGINE_COMMON_REG_LIST(id)\
	SRI(SETUP, DC_I2C_DDC, id),\
	SRI(SPEED, DC_I2C_DDC, id),\
	SR(DC_I2C_ARBITRATION),\
	SR(DC_I2C_CONTROL),\
	SR(DC_I2C_SW_STATUS),\
	SR(DC_I2C_TRANSACTION0),\
	SR(DC_I2C_TRANSACTION1),\
	SR(DC_I2C_TRANSACTION2),\
	SR(DC_I2C_TRANSACTION3),\
	SR(DC_I2C_DATA),\
	SR(MICROSECOND_TIME_BASE_DIV)

struct dce110_i2c_hw_engine_registers {
	uint32_t SETUP;
	uint32_t SPEED;
	uint32_t DC_I2C_ARBITRATION;
	uint32_t DC_I2C_CONTROL;
	uint32_t DC_I2C_SW_STATUS;
	uint32_t DC_I2C_TRANSACTION0;
	uint32_t DC_I2C_TRANSACTION1;
	uint32_t DC_I2C_TRANSACTION2;
	uint32_t DC_I2C_TRANSACTION3;
	uint32_t DC_I2C_DATA;
	uint32_t MICROSECOND_TIME_BASE_DIV;
};


struct i2c_hw_engine_dce110 {
	struct i2c_hw_engine base;
	const struct dce110_i2c_hw_engine_registers *regs;
	struct {
		uint32_t DC_I2C_DDCX_SETUP;
		uint32_t DC_I2C_DDCX_SPEED;
	} addr;
	uint32_t engine_id;
	/* expressed in kilohertz */
	uint32_t reference_frequency;
	/* number of bytes currently used in HW buffer */
	uint32_t buffer_used_bytes;
	/* number of bytes used for write transaction in HW buffer
	 * - this will be used as the index to read from*/
	uint32_t buffer_used_write;
	/* number of pending transactions (before GO) */
	uint32_t transaction_count;
	uint32_t engine_keep_power_up_count;
};

struct i2c_hw_engine_dce110_create_arg {
	uint32_t engine_id;
	uint32_t reference_frequency;
	uint32_t default_speed;
	struct dc_context *ctx;
	const struct dce110_i2c_hw_engine_registers *regs;
};

struct i2c_engine *dal_i2c_hw_engine_dce110_create(
	const struct i2c_hw_engine_dce110_create_arg *arg);

bool i2c_hw_engine_dce110_construct(
	struct i2c_hw_engine_dce110 *engine_dce110,
	const struct i2c_hw_engine_dce110_create_arg *arg);

#endif
