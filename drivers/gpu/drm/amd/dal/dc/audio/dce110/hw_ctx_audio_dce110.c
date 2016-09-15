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

#include "include/logger_interface.h"
#include "../hw_ctx_audio.h"
#include "hw_ctx_audio_dce110.h"

#include "dce/dce_11_0_d.h"
#include "dce/dce_11_0_sh_mask.h"

#define FROM_BASE(ptr) \
	container_of((ptr), struct hw_ctx_audio_dce110, base)

#define DP_SEC_AUD_N__DP_SEC_AUD_N__DEFAULT 0x8000
#define DP_AUDIO_DTO_MODULE_WITHOUT_SS 360
#define DP_AUDIO_DTO_PHASE_WITHOUT_SS 24

#define DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE__AUDIO_FRONT_END 0
#define DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE__AUTO_CALC 1
#define DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE__REGISTER_PROGRAMMABLE 2

#define FIRST_AUDIO_STREAM_ID 1

#define NOT_IMPLEMENTED() DAL_LOGGER_NOT_IMPL(LOG_MINOR_COMPONENT_AUDIO, \
			"Audio:%s()\n", __func__)


static void destroy(
	struct hw_ctx_audio **ptr)
{
	struct hw_ctx_audio_dce110 *hw_ctx_dce110;

	hw_ctx_dce110 = container_of(
		*ptr, struct hw_ctx_audio_dce110, base);

	/* release memory allocated for struct hw_ctx_audio_dce110 */
	dm_free(hw_ctx_dce110);

	*ptr = NULL;
}

/* initialize HW state */
static void hw_initialize(
	const struct hw_ctx_audio *hw_ctx)
{
	uint32_t stream_id = FROM_BASE(hw_ctx)->azalia_stream_id;
	uint32_t addr;

	/* we only need to program the following registers once, so we only do
	it for the first audio stream.*/
	if (stream_id != FIRST_AUDIO_STREAM_ID)
		return;

	/* Suport R5 - 32khz
	 * Suport R6 - 44.1khz
	 * Suport R7 - 48khz
	 */
	addr = mmAZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES;
	{
		uint32_t value;

		value = dm_read_reg(hw_ctx->ctx, addr);

		set_reg_field_value(value, 0x70,
		AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES,
		AUDIO_RATE_CAPABILITIES);

		dm_write_reg(hw_ctx->ctx, addr, value);
	}

	/*Keep alive bit to verify HW block in BU. */
	addr = mmAZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES;
	{
		uint32_t value;

		value = dm_read_reg(hw_ctx->ctx, addr);

		set_reg_field_value(value, 1,
		AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES,
		CLKSTOP);

		set_reg_field_value(value, 1,
		AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES,
		EPSS);
		dm_write_reg(hw_ctx->ctx, addr, value);
	}
}

static const struct hw_ctx_audio_funcs funcs = {
	.destroy = destroy,
	.hw_initialize =
		hw_initialize,
};

static bool construct(
	struct hw_ctx_audio_dce110 *hw_ctx,
	uint8_t azalia_stream_id,
	struct dc_context *ctx)
{
	struct hw_ctx_audio *base = &hw_ctx->base;

	base->funcs = &funcs;

	/* save audio endpoint or dig front for current dce110 audio object */
	hw_ctx->azalia_stream_id = azalia_stream_id;
	hw_ctx->base.ctx = ctx;

	/* azalia audio endpoints register offsets. azalia is associated with
	DIG front. save AUDIO register offset */
	switch (azalia_stream_id) {
	case 1: {
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_index =
			mmAZF0ENDPOINT0_AZALIA_F0_CODEC_ENDPOINT_INDEX;
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_data =
			mmAZF0ENDPOINT0_AZALIA_F0_CODEC_ENDPOINT_DATA;
		}
		break;
	case 2: {
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_index =
			mmAZF0ENDPOINT1_AZALIA_F0_CODEC_ENDPOINT_INDEX;
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_data =
			mmAZF0ENDPOINT1_AZALIA_F0_CODEC_ENDPOINT_DATA;
		}
		break;
	case 3: {
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_index =
			mmAZF0ENDPOINT2_AZALIA_F0_CODEC_ENDPOINT_INDEX;
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_data =
			mmAZF0ENDPOINT2_AZALIA_F0_CODEC_ENDPOINT_DATA;
		}
		break;
	case 4: {
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_index =
			mmAZF0ENDPOINT3_AZALIA_F0_CODEC_ENDPOINT_INDEX;
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_data =
			mmAZF0ENDPOINT3_AZALIA_F0_CODEC_ENDPOINT_DATA;
		}
		break;
	default:
		dal_logger_write(
			hw_ctx->base.ctx->logger,
			LOG_MAJOR_WARNING,
			LOG_MINOR_COMPONENT_AUDIO,
			"Invalid Azalia stream ID!");
		break;
	}

	return true;
}

/* audio_dce110 is derived from audio directly, not via dce80  */
struct hw_ctx_audio *dal_hw_ctx_audio_dce110_create(
	struct dc_context *ctx,
	uint32_t azalia_stream_id)
{
	/* allocate memory for struc hw_ctx_audio_dce110 */
	struct hw_ctx_audio_dce110 *hw_ctx_dce110 =
			dm_alloc(sizeof(struct hw_ctx_audio_dce110));

	if (!hw_ctx_dce110) {
		ASSERT_CRITICAL(hw_ctx_dce110);
		return NULL;
	}

	/*return pointer to hw_ctx_audio back to caller  -- audio object */
	if (construct(
			hw_ctx_dce110, azalia_stream_id, ctx))
		return &hw_ctx_dce110->base;

	dal_logger_write(
		ctx->logger,
		LOG_MAJOR_ERROR,
		LOG_MINOR_COMPONENT_AUDIO,
		"Failed to create hw_ctx_audio for DCE11\n");

	dm_free(hw_ctx_dce110);

	return NULL;
}
