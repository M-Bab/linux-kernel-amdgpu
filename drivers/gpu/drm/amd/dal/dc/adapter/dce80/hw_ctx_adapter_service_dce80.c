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
#include "include/adapter_service_types.h"
#include "include/grph_object_id.h"
#include "../hw_ctx_adapter_service.h"

#include "hw_ctx_adapter_service_dce80.h"

#include "dce/dce_8_0_d.h"
#include "dce/dce_8_0_sh_mask.h"

#ifndef mmCC_DC_HDMI_STRAPS
#define mmCC_DC_HDMI_STRAPS 0x1918
#define CC_DC_HDMI_STRAPS__HDMI_DISABLE_MASK 0x40
#define CC_DC_HDMI_STRAPS__HDMI_DISABLE__SHIFT 0x6
#define CC_DC_HDMI_STRAPS__AUDIO_STREAM_NUMBER_MASK 0x700
#define CC_DC_HDMI_STRAPS__AUDIO_STREAM_NUMBER__SHIFT 0x8
#endif

enum {
	MAX_NUMBER_OF_AUDIO_PINS = 7
};

static const uint32_t audio_index_reg_offset[] = {
	mmAZF0ENDPOINT0_AZALIA_F0_CODEC_ENDPOINT_INDEX,
	mmAZF0ENDPOINT1_AZALIA_F0_CODEC_ENDPOINT_INDEX,
	mmAZF0ENDPOINT2_AZALIA_F0_CODEC_ENDPOINT_INDEX,
	mmAZF0ENDPOINT3_AZALIA_F0_CODEC_ENDPOINT_INDEX,
	mmAZF0ENDPOINT4_AZALIA_F0_CODEC_ENDPOINT_INDEX,
	mmAZF0ENDPOINT5_AZALIA_F0_CODEC_ENDPOINT_INDEX,
	/* TR, BN has 7 audio endpoints but 6 DIGs */
	mmAZF0ENDPOINT6_AZALIA_F0_CODEC_ENDPOINT_INDEX
};

static const uint32_t audio_data_reg_offset[] = {
	mmAZF0ENDPOINT0_AZALIA_F0_CODEC_ENDPOINT_DATA,
	mmAZF0ENDPOINT1_AZALIA_F0_CODEC_ENDPOINT_DATA,
	mmAZF0ENDPOINT2_AZALIA_F0_CODEC_ENDPOINT_DATA,
	mmAZF0ENDPOINT3_AZALIA_F0_CODEC_ENDPOINT_DATA,
	mmAZF0ENDPOINT4_AZALIA_F0_CODEC_ENDPOINT_DATA,
	mmAZF0ENDPOINT5_AZALIA_F0_CODEC_ENDPOINT_DATA,
	mmAZF0ENDPOINT6_AZALIA_F0_CODEC_ENDPOINT_DATA
};

static const struct graphics_object_id invalid_go = {
	0, ENUM_ID_UNKNOWN, OBJECT_TYPE_UNKNOWN
};

#define FROM_HW_CTX(ptr) \
	container_of((ptr), struct hw_ctx_adapter_service_dce80, base)

static void destruct(
	struct hw_ctx_adapter_service_dce80 *hw_ctx)
{
	/* There is nothing to destruct at the moment */

	dal_adapter_service_destruct_hw_ctx(&hw_ctx->base);
}

static void destroy(
	struct hw_ctx_adapter_service *ptr)
{
	struct hw_ctx_adapter_service_dce80 *hw_ctx =
		FROM_HW_CTX(ptr);

	destruct(hw_ctx);

	dm_free(hw_ctx);
}

static uint32_t get_number_of_connected_audio_endpoints_multistream(
		struct hw_ctx_adapter_service *hw_ctx)
{
	struct dc_context *ctx = hw_ctx->ctx;
	uint32_t num_connected_audio_endpoints = 0;
	uint32_t i;
	uint32_t default_config =
	ixAZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_CONFIGURATION_DEFAULT;

	/* find the total number of streams available via the
	 * AZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_CONFIGURATION_DEFAULT
	 * registers (one for each pin) starting from pin 1
	 * up to the max number of audio pins.
	 * We stop on the first pin where
	 * PORT_CONNECTIVITY == 1 (as instructed by HW team).
	 */
	for (i = 0; i < MAX_NUMBER_OF_AUDIO_PINS; i++) {
		uint32_t value = 0;

		set_reg_field_value(value,
			default_config,
			AZALIA_F0_CODEC_ENDPOINT_INDEX,
			AZALIA_ENDPOINT_REG_INDEX);

		dm_write_reg(ctx, audio_index_reg_offset[i], value);

		value = 0;
		value = dm_read_reg(ctx, audio_data_reg_offset[i]);

		/* 1 means not supported*/
		if (get_reg_field_value(value,
		AZALIA_F0_CODEC_PIN_CONTROL_RESPONSE_CONFIGURATION_DEFAULT,
		PORT_CONNECTIVITY) == 1)
			break;

		num_connected_audio_endpoints++;
	}

	return num_connected_audio_endpoints;
}

static uint32_t get_number_of_connected_audio_endpoints(
	struct hw_ctx_adapter_service *hw_ctx)
{
	uint32_t addr = mmCC_DC_HDMI_STRAPS;
	uint32_t value = 0;
	uint32_t field = 0;

	if (hw_ctx->cached_audio_straps == AUDIO_STRAPS_NOT_ALLOWED)
		/* audio straps indicate no audio supported */
		return 0;

	value = dm_read_reg(hw_ctx->ctx, addr);

	field = get_reg_field_value(
			value, CC_DC_HDMI_STRAPS, AUDIO_STREAM_NUMBER);
	if (field == 1)
		/* multi streams not supported */
		return 1;
	else if (field == 0)
		/* multi streams supported */
		return get_number_of_connected_audio_endpoints_multistream(
				hw_ctx);

	/* unexpected value */
	ASSERT_CRITICAL(false);
	return field;
}

static bool power_up(
	struct hw_ctx_adapter_service *hw_ctx)
{
	uint32_t value = 0;
	uint32_t field = 0;

	value = dm_read_reg(hw_ctx->ctx, mmCC_DC_HDMI_STRAPS);
	field = get_reg_field_value(
			value, CC_DC_HDMI_STRAPS, HDMI_DISABLE);

	if (field == 0) {
		hw_ctx->cached_audio_straps = AUDIO_STRAPS_DP_HDMI_AUDIO;
	} else {
		value = dm_read_reg(
				hw_ctx->ctx, mmDC_PINSTRAPS);
		field = get_reg_field_value(
					value,
					DC_PINSTRAPS,
					DC_PINSTRAPS_AUDIO);
	}

	/* get the number of connected audio endpoints */
	FROM_HW_CTX(hw_ctx)->number_of_connected_audio_endpoints =
		get_number_of_connected_audio_endpoints(hw_ctx);

	return true;
}

static struct graphics_object_id enum_audio_object(
	const struct hw_ctx_adapter_service *hw_ctx,
	uint32_t index)
{
	uint32_t number_of_connected_audio_endpoints =
		FROM_HW_CTX(hw_ctx)->number_of_connected_audio_endpoints;

	if (index >= number_of_connected_audio_endpoints)
		return invalid_go;
	else
		return dal_graphics_object_id_init(
			AUDIO_ID_INTERNAL_AZALIA,
			(enum object_enum_id)(index + 1),
			OBJECT_TYPE_AUDIO);
}

static const struct hw_ctx_adapter_service_funcs funcs = {
	destroy,
	power_up,
	enum_audio_object,
};

static bool construct(
	struct hw_ctx_adapter_service_dce80 *hw_ctx,
	struct dc_context *ctx)
{
	if (!dal_adapter_service_construct_hw_ctx(&hw_ctx->base, ctx)) {
		BREAK_TO_DEBUGGER();
		return false;
	}

	hw_ctx->base.funcs = &funcs;

	hw_ctx->number_of_connected_audio_endpoints = 0;

	return true;
}

struct hw_ctx_adapter_service *
	dal_adapter_service_create_hw_ctx_dce80(struct dc_context *ctx)
{
	struct hw_ctx_adapter_service_dce80 *hw_ctx =
			dm_alloc(sizeof(struct hw_ctx_adapter_service_dce80));

	if (!hw_ctx) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	if (construct(hw_ctx, ctx))
		return &hw_ctx->base;

	BREAK_TO_DEBUGGER();

	dm_free(hw_ctx);

	return NULL;
}
