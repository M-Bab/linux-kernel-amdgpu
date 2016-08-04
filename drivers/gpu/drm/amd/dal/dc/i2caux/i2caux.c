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
#include "include/i2caux_interface.h"

/*
 * Header of this unit
 */

#include "i2caux.h"

/*
 * Post-requisites: headers required by this unit
 */

#include "engine.h"
#include "i2c_engine.h"
#include "aux_engine.h"

/*
 * This unit
 */

#if defined(CONFIG_DRM_AMD_DAL_DCE11_0) || defined(CONFIG_DRM_AMD_DAL_DCE10_0)
#include "dce110/i2caux_dce110.h"
#endif

#include "diagnostics/i2caux_diag.h"

/*
 * @brief
 * Plain API, available publicly
 */

struct i2caux *dal_i2caux_create(
	struct adapter_service *as,
	struct dc_context *ctx)
{
	enum dce_version dce_version;
	enum dce_environment dce_environment;

	if (!as) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	dce_version = dal_adapter_service_get_dce_version(as);
	dce_environment = dal_adapter_service_get_dce_environment(as);

	if (IS_FPGA_MAXIMUS_DC(dce_environment)) {
		return dal_i2caux_diag_fpga_create(as, ctx);
	}

	switch (dce_version) {
#if defined(CONFIG_DRM_AMD_DAL_DCE11_0)
#if defined(CONFIG_DRM_AMD_DAL_DCE10_0)
	case DCE_VERSION_10_0:
#endif
	case DCE_VERSION_11_0:
		return dal_i2caux_dce110_create(as, ctx);
#endif
	default:
		BREAK_TO_DEBUGGER();
		return NULL;
	}
}

bool dal_i2caux_submit_i2c_command(
	struct i2caux *i2caux,
	struct ddc *ddc,
	struct i2c_command *cmd)
{
	struct i2c_engine *engine;
	uint8_t index_of_payload = 0;
	bool result;

	if (!ddc) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (!cmd) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	switch (cmd->engine) {
	case I2C_COMMAND_ENGINE_SW:
		/* try to acquire SW engine first,
		 * acquire HW engine if SW engine not available */
		engine = i2caux->funcs->acquire_i2c_sw_engine(i2caux, ddc);

		if (!engine)
			engine = i2caux->funcs->acquire_i2c_hw_engine(
				i2caux, ddc);
	break;
	case I2C_COMMAND_ENGINE_HW:
	case I2C_COMMAND_ENGINE_DEFAULT:
	default:
		/* try to acquire HW engine first,
		 * acquire SW engine if HW engine not available */
		engine = i2caux->funcs->acquire_i2c_hw_engine(i2caux, ddc);

		if (!engine)
			engine = i2caux->funcs->acquire_i2c_sw_engine(
				i2caux, ddc);
	}

	if (!engine)
		return false;

	engine->funcs->set_speed(engine, cmd->speed);

	result = true;

	while (index_of_payload < cmd->number_of_payloads) {
		bool mot = (index_of_payload != cmd->number_of_payloads - 1);

		struct i2c_payload *payload = cmd->payloads + index_of_payload;

		struct i2caux_transaction_request request = { 0 };

		request.operation = payload->write ?
			I2CAUX_TRANSACTION_WRITE :
			I2CAUX_TRANSACTION_READ;

		request.payload.address_space =
			I2CAUX_TRANSACTION_ADDRESS_SPACE_I2C;
		request.payload.address = (payload->address << 1) |
			!payload->write;
		request.payload.length = payload->length;
		request.payload.data = payload->data;

		if (!engine->base.funcs->submit_request(
			&engine->base, &request, mot)) {
			result = false;
			break;
		}

		++index_of_payload;
	}

	i2caux->funcs->release_engine(i2caux, &engine->base);

	return result;
}

bool dal_i2caux_submit_aux_command(
	struct i2caux *i2caux,
	struct ddc *ddc,
	struct aux_command *cmd)
{
	struct aux_engine *engine;
	uint8_t index_of_payload = 0;
	bool result;

	if (!ddc) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (!cmd) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	engine = i2caux->funcs->acquire_aux_engine(i2caux, ddc);

	if (!engine)
		return false;

	engine->delay = cmd->defer_delay;
	engine->max_defer_write_retry = cmd->max_defer_write_retry;

	result = true;

	while (index_of_payload < cmd->number_of_payloads) {
		bool mot = (index_of_payload != cmd->number_of_payloads - 1);

		struct aux_payload *payload = cmd->payloads + index_of_payload;

		struct i2caux_transaction_request request = { 0 };

		request.operation = payload->write ?
			I2CAUX_TRANSACTION_WRITE :
			I2CAUX_TRANSACTION_READ;

		if (payload->i2c_over_aux) {
			request.payload.address_space =
				I2CAUX_TRANSACTION_ADDRESS_SPACE_I2C;

			request.payload.address = (payload->address << 1) |
				!payload->write;
		} else {
			request.payload.address_space =
				I2CAUX_TRANSACTION_ADDRESS_SPACE_DPCD;

			request.payload.address = payload->address;
		}

		request.payload.length = payload->length;
		request.payload.data = payload->data;

		if (!engine->base.funcs->submit_request(
			&engine->base, &request, mot)) {
			result = false;
			break;
		}

		++index_of_payload;
	}

	i2caux->funcs->release_engine(i2caux, &engine->base);

	return result;
}

static bool get_hw_supported_ddc_line(
	struct ddc *ddc,
	enum gpio_ddc_line *line)
{
	enum gpio_ddc_line line_found;

	if (!ddc) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	if (!dal_ddc_is_hw_supported(ddc))
		return false;

	line_found = dal_ddc_get_line(ddc);

	if (line_found >= GPIO_DDC_LINE_COUNT)
		return false;

	*line = line_found;

	return true;
}

void dal_i2caux_keep_engine_power_up(
	struct i2caux *i2caux,
	struct ddc *ddc,
	bool keep_power_up)
{
	enum gpio_ddc_line line;
	struct i2c_engine *engine;

	if (!get_hw_supported_ddc_line(ddc, &line))
		return;

	engine = i2caux->i2c_hw_engines[line];

	engine->base.funcs->keep_power_up_count(&engine->base, keep_power_up);
}

bool dal_i2caux_start_gtc_sync(
	struct i2caux *i2caux,
	struct ddc *ddc)
{
	enum gpio_ddc_line line;

	struct aux_engine *engine;

	bool result;

	if (!get_hw_supported_ddc_line(ddc, &line))
		return false;

	engine = i2caux->aux_engines[line];

	if (!engine)
		return false;

	if (!engine->base.funcs->acquire(&engine->base, ddc))
		return false;

	result = engine->funcs->start_gtc_sync(engine);

	i2caux->funcs->release_engine(i2caux, &engine->base);

	return result;
}

bool dal_i2caux_stop_gtc_sync(
	struct i2caux *i2caux,
	struct ddc *ddc)
{
	enum gpio_ddc_line line;

	struct aux_engine *engine;

	if (!get_hw_supported_ddc_line(ddc, &line))
		return false;

	engine = i2caux->aux_engines[line];

	if (!engine)
		return false;

	if (!engine->base.funcs->acquire(&engine->base, ddc))
		return false;

	engine->funcs->stop_gtc_sync(engine);

	i2caux->funcs->release_engine(i2caux, &engine->base);

	return true;
}

void dal_i2caux_configure_aux(
	struct i2caux *i2caux,
	struct ddc *ddc,
	union aux_config cfg)
{
	struct aux_engine *engine =
		i2caux->funcs->acquire_aux_engine(i2caux, ddc);

	if (!engine)
		return;

	engine->funcs->configure(engine, cfg);

	i2caux->funcs->release_engine(i2caux, &engine->base);
}

void dal_i2caux_destroy(
	struct i2caux **i2caux)
{
	if (!i2caux || !*i2caux) {
		BREAK_TO_DEBUGGER();
		return;
	}

	(*i2caux)->funcs->destroy(i2caux);

	*i2caux = NULL;
}

/*
 * @brief
 * An utility function used by 'struct i2caux' and its descendants
 */

uint32_t dal_i2caux_get_reference_clock(
	struct adapter_service *as)
{
	struct firmware_info info = { { 0 } };

	if (!dal_adapter_service_get_firmware_info(as, &info))
		return 0;

	return info.pll_info.crystal_frequency;
}

/*
 * @brief
 * i2caux
 */

enum {
	/* following are expressed in KHz */
	DEFAULT_I2C_SW_SPEED = 50,
	DEFAULT_I2C_HW_SPEED = 50,

	/* This is the timeout as defined in DP 1.2a,
	 * 2.3.4 "Detailed uPacket TX AUX CH State Description". */
	AUX_TIMEOUT_PERIOD = 400,

	/* Ideally, the SW timeout should be just above 550usec
	 * which is programmed in HW.
	 * But the SW timeout of 600usec is not reliable,
	 * because on some systems, delay_in_microseconds()
	 * returns faster than it should.
	 * EPR #379763: by trial-and-error on different systems,
	 * 700usec is the minimum reliable SW timeout for polling
	 * the AUX_SW_STATUS.AUX_SW_DONE bit.
	 * This timeout expires *only* when there is
	 * AUX Error or AUX Timeout conditions - not during normal operation.
	 * During normal operation, AUX_SW_STATUS.AUX_SW_DONE bit is set
	 * at most within ~240usec. That means,
	 * increasing this timeout will not affect normal operation,
	 * and we'll timeout after
	 * SW_AUX_TIMEOUT_PERIOD_MULTIPLIER * AUX_TIMEOUT_PERIOD = 1600usec.
	 * This timeout is especially important for
	 * resume from S3 and CTS. */
	SW_AUX_TIMEOUT_PERIOD_MULTIPLIER = 4
};

struct i2c_engine *dal_i2caux_acquire_i2c_sw_engine(
	struct i2caux *i2caux,
	struct ddc *ddc)
{
	enum gpio_ddc_line line;
	struct i2c_engine *engine = NULL;

	if (get_hw_supported_ddc_line(ddc, &line))
		engine = i2caux->i2c_sw_engines[line];

	if (!engine)
		engine = i2caux->i2c_generic_sw_engine;

	if (!engine)
		return NULL;

	if (!engine->base.funcs->acquire(&engine->base, ddc))
		return NULL;

	return engine;
}

struct aux_engine *dal_i2caux_acquire_aux_engine(
	struct i2caux *i2caux,
	struct ddc *ddc)
{
	enum gpio_ddc_line line;
	struct aux_engine *engine;

	if (!get_hw_supported_ddc_line(ddc, &line))
		return NULL;

	engine = i2caux->aux_engines[line];

	if (!engine)
		return NULL;

	if (!engine->base.funcs->acquire(&engine->base, ddc))
		return NULL;

	return engine;
}

void dal_i2caux_release_engine(
	struct i2caux *i2caux,
	struct engine *engine)
{
	engine->funcs->release_engine(engine);

	dal_ddc_close(engine->ddc);

	engine->ddc = NULL;
}

bool dal_i2caux_construct(
	struct i2caux *i2caux,
	struct adapter_service *as,
	struct dc_context *ctx)
{
	uint32_t i = 0;

	i2caux->ctx = ctx;
	do {
		i2caux->i2c_sw_engines[i] = NULL;
		i2caux->i2c_hw_engines[i] = NULL;
		i2caux->aux_engines[i] = NULL;

		++i;
	} while (i < GPIO_DDC_LINE_COUNT);

	i2caux->i2c_generic_sw_engine = NULL;
	i2caux->i2c_generic_hw_engine = NULL;

	i2caux->aux_timeout_period =
		SW_AUX_TIMEOUT_PERIOD_MULTIPLIER * AUX_TIMEOUT_PERIOD;

	i2caux->default_i2c_sw_speed = DEFAULT_I2C_SW_SPEED;
	i2caux->default_i2c_hw_speed = DEFAULT_I2C_HW_SPEED;

	return true;
}

void dal_i2caux_destruct(
	struct i2caux *i2caux)
{
	uint32_t i = 0;

	if (i2caux->i2c_generic_hw_engine)
		i2caux->i2c_generic_hw_engine->funcs->destroy(
			&i2caux->i2c_generic_hw_engine);

	if (i2caux->i2c_generic_sw_engine)
		i2caux->i2c_generic_sw_engine->funcs->destroy(
			&i2caux->i2c_generic_sw_engine);

	do {
		if (i2caux->aux_engines[i])
			i2caux->aux_engines[i]->funcs->destroy(
				&i2caux->aux_engines[i]);

		if (i2caux->i2c_hw_engines[i])
			i2caux->i2c_hw_engines[i]->funcs->destroy(
				&i2caux->i2c_hw_engines[i]);

		if (i2caux->i2c_sw_engines[i])
			i2caux->i2c_sw_engines[i]->funcs->destroy(
				&i2caux->i2c_sw_engines[i]);

		++i;
	} while (i < GPIO_DDC_LINE_COUNT);
}

