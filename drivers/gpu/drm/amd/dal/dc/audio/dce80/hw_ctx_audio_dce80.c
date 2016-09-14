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
#include "dce/dce_8_0_d.h"
#include "dce/dce_8_0_sh_mask.h"
#include "hw_ctx_audio_dce80.h"

#define FROM_BASE(ptr) \
	container_of((ptr), struct hw_ctx_audio_dce80, base)

#define DP_SEC_AUD_N__DP_SEC_AUD_N__DEFAULT 0x8000
#define DP_AUDIO_DTO_MODULE_WITHOUT_SS 360
#define DP_AUDIO_DTO_PHASE_WITHOUT_SS 24

#define DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE__AUDIO_FRONT_END 0
#define DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE__AUTO_CALC 1
#define DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE__REGISTER_PROGRAMMABLE 2

#define FIRST_AUDIO_STREAM_ID 1

/* --- static functions --- */

/* static void dal_audio_destruct_hw_ctx_audio_dce80(
	struct hw_ctx_audio_dce80 *ctx);*/

static void destroy(
	struct hw_ctx_audio **ptr)
{
	struct hw_ctx_audio_dce80 *hw_ctx_dce80;

	hw_ctx_dce80 = container_of(
		*ptr, struct hw_ctx_audio_dce80, base);

	/* release memory allocated for struct hw_ctx_audio_dce80 */
	dm_free(hw_ctx_dce80);

	*ptr = NULL;
}

/* ---  hook functions --- */

static bool get_azalia_clock_info_hdmi(
	const struct hw_ctx_audio *hw_ctx,
	uint32_t crtc_pixel_clock_in_khz,
	uint32_t actual_pixel_clock_in_khz,
	struct azalia_clock_info *azalia_clock_info);

static bool get_azalia_clock_info_dp(
	const struct hw_ctx_audio *hw_ctx,
	uint32_t requested_pixel_clock_in_khz,
	const struct audio_pll_info *pll_info,
	struct azalia_clock_info *azalia_clock_info);

static void setup_audio_wall_dto(
	const struct hw_ctx_audio *hw_ctx,
	enum signal_type signal,
	const struct audio_crtc_info *crtc_info,
	const struct audio_pll_info *pll_info)
{
	struct azalia_clock_info clock_info = { 0 };

	uint32_t value = dm_read_reg(hw_ctx->ctx, mmDCCG_AUDIO_DTO_SOURCE);

	/* TODO: GraphicsObject\inc\GraphicsObjectDefs.hpp(131):
	 *inline bool isHdmiSignal(SignalType signal)
	 *if (Signals::isHdmiSignal(signal))
	 */
	if (dc_is_hdmi_signal(signal)) {
		/*DTO0 Programming goal:
		-generate 24MHz, 128*Fs from 24MHz
		-use DTO0 when an active HDMI port is connected
		(optionally a DP is connected) */

		/* calculate DTO settings */
		get_azalia_clock_info_hdmi(
			hw_ctx,
			crtc_info->requested_pixel_clock,
			crtc_info->calculated_pixel_clock,
			&clock_info);

		/* On TN/SI, Program DTO source select and DTO select before
		programming DTO modulo and DTO phase. These bits must be
		programmed first, otherwise there will be no HDMI audio at boot
		up. This is a HW sequence change (different from old ASICs).
		Caution when changing this programming sequence.

		HDMI enabled, using DTO0
		program master CRTC for DTO0 */
		{
			set_reg_field_value(value,
				pll_info->dto_source - DTO_SOURCE_ID0,
				DCCG_AUDIO_DTO_SOURCE,
				DCCG_AUDIO_DTO0_SOURCE_SEL);

			set_reg_field_value(value,
				0,
				DCCG_AUDIO_DTO_SOURCE,
				DCCG_AUDIO_DTO_SEL);

			dm_write_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO_SOURCE, value);
		}

		/* module */
		{
			value = dm_read_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO0_MODULE);
			set_reg_field_value(value,
				clock_info.audio_dto_module,
				DCCG_AUDIO_DTO0_MODULE,
				DCCG_AUDIO_DTO0_MODULE);
			dm_write_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO0_MODULE, value);
		}

		/* phase */
		{
			value = 0;

			value = dm_read_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO0_PHASE);
			set_reg_field_value(value,
				clock_info.audio_dto_phase,
				DCCG_AUDIO_DTO0_PHASE,
				DCCG_AUDIO_DTO0_PHASE);

			dm_write_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO0_PHASE, value);
		}

	} else {
		/*DTO1 Programming goal:
		-generate 24MHz, 512*Fs, 128*Fs from 24MHz
		-default is to used DTO1, and switch to DTO0 when an audio
		master HDMI port is connected
		-use as default for DP

		calculate DTO settings */
		get_azalia_clock_info_dp(
			hw_ctx,
			crtc_info->requested_pixel_clock,
			pll_info,
			&clock_info);

		/* Program DTO select before programming DTO modulo and DTO
		phase. default to use DTO1 */

		{
			set_reg_field_value(value, 1,
				DCCG_AUDIO_DTO_SOURCE,
				DCCG_AUDIO_DTO_SEL);
			/*dal_write_reg(mmDCCG_AUDIO_DTO_SOURCE, value)*/

			/* Select 512fs for DP TODO: web register definition
			does not match register header file */
			set_reg_field_value(value, 1,
				DCCG_AUDIO_DTO_SOURCE,
				DCCG_AUDIO_DTO2_USE_512FBR_DTO);

			dm_write_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO_SOURCE, value);
		}

		/* module */
		{
			value = 0;

			value = dm_read_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO1_MODULE);

			set_reg_field_value(value,
				clock_info.audio_dto_module,
				DCCG_AUDIO_DTO1_MODULE,
				DCCG_AUDIO_DTO1_MODULE);

			dm_write_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO1_MODULE, value);
		}

		/* phase */
		{
			value = 0;

			value = dm_read_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO1_PHASE);

			set_reg_field_value(value,
				clock_info.audio_dto_phase,
				DCCG_AUDIO_DTO1_PHASE,
				DCCG_AUDIO_DTO1_PHASE);

			dm_write_reg(hw_ctx->ctx,
					mmDCCG_AUDIO_DTO1_PHASE, value);
		}

		/* DAL2 code separate DCCG_AUDIO_DTO_SEL and
		DCCG_AUDIO_DTO2_USE_512FBR_DTO programming into two different
		location. merge together should not hurt */
		/*value.bits.DCCG_AUDIO_DTO2_USE_512FBR_DTO = 1;
		dal_write_reg(mmDCCG_AUDIO_DTO_SOURCE, value);*/
	}
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

/* search pixel clock value for Azalia HDMI Audio */
static bool get_azalia_clock_info_hdmi(
	const struct hw_ctx_audio *hw_ctx,
	uint32_t crtc_pixel_clock_in_khz,
	uint32_t actual_pixel_clock_in_khz,
	struct azalia_clock_info *azalia_clock_info)
{
	if (azalia_clock_info == NULL)
		return false;

	/* audio_dto_phase= 24 * 10,000;
	 *   24MHz in [100Hz] units */
	azalia_clock_info->audio_dto_phase =
			24 * 10000;

	/* audio_dto_module = PCLKFrequency * 10,000;
	 *  [khz] -> [100Hz] */
	azalia_clock_info->audio_dto_module =
			actual_pixel_clock_in_khz * 10;

	return true;
}

/* search pixel clock value for Azalia DP Audio */
static bool get_azalia_clock_info_dp(
	const struct hw_ctx_audio *hw_ctx,
	uint32_t requested_pixel_clock_in_khz,
	const struct audio_pll_info *pll_info,
	struct azalia_clock_info *azalia_clock_info)
{
	if (pll_info == NULL || azalia_clock_info == NULL)
		return false;

	/* Reported dpDtoSourceClockInkhz value for
	 * DCE8 already adjusted for SS, do not need any
	 * adjustment here anymore
	 */

	/*audio_dto_phase = 24 * 10,000;
	 * 24MHz in [100Hz] units */
	azalia_clock_info->audio_dto_phase = 24 * 10000;

	/*audio_dto_module = dpDtoSourceClockInkhz * 10,000;
	 *  [khz] ->[100Hz] */
	azalia_clock_info->audio_dto_module =
		pll_info->dp_dto_source_clock_in_khz * 10;

	return true;
}

static const struct hw_ctx_audio_funcs funcs = {
	.destroy = destroy,
	.setup_audio_wall_dto =
		setup_audio_wall_dto,
	.hw_initialize =
		hw_initialize,
	.get_azalia_clock_info_hdmi =
		get_azalia_clock_info_hdmi,
	.get_azalia_clock_info_dp =
		get_azalia_clock_info_dp,
};

bool dal_audio_construct_hw_ctx_audio_dce80(
	struct hw_ctx_audio_dce80 *hw_ctx,
	uint8_t azalia_stream_id,
	struct dc_context *ctx)
{
	struct hw_ctx_audio *base = &hw_ctx->base;

	base->funcs = &funcs;

	/* save audio endpoint or dig front for current dce80 audio object */
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
	case 5: {
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_index =
			mmAZF0ENDPOINT4_AZALIA_F0_CODEC_ENDPOINT_INDEX;
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_data =
			mmAZF0ENDPOINT4_AZALIA_F0_CODEC_ENDPOINT_DATA;
		}
		break;
	case 6: {
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_index =
			mmAZF0ENDPOINT5_AZALIA_F0_CODEC_ENDPOINT_INDEX;
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_data =
			mmAZF0ENDPOINT5_AZALIA_F0_CODEC_ENDPOINT_DATA;
		}
		break;
	case 7: {
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_index =
			mmAZF0ENDPOINT6_AZALIA_F0_CODEC_ENDPOINT_INDEX;
			hw_ctx->az_mm_reg_offsets.
			azf0endpointx_azalia_f0_codec_endpoint_data =
			mmAZF0ENDPOINT6_AZALIA_F0_CODEC_ENDPOINT_DATA;
		}
		break;
	default:
		/*DALASSERT_MSG(false,("Invalid Azalia stream ID!"));*/
		BREAK_TO_DEBUGGER();
		break;
	}

	return true;
}

struct hw_ctx_audio *dal_audio_create_hw_ctx_audio_dce80(
	struct dc_context *ctx,
	uint32_t azalia_stream_id)
{
	/* allocate memory for struc hw_ctx_audio_dce80 */
	struct hw_ctx_audio_dce80 *hw_ctx_dce80 =
			dm_alloc(sizeof(struct hw_ctx_audio_dce80));

	if (!hw_ctx_dce80) {
		BREAK_TO_DEBUGGER();
		return NULL;
	}

	/*return pointer to hw_ctx_audio back to caller  -- audio object */
	if (dal_audio_construct_hw_ctx_audio_dce80(
			hw_ctx_dce80, azalia_stream_id, ctx))
		return &hw_ctx_dce80->base;

	BREAK_TO_DEBUGGER();

	dm_free(hw_ctx_dce80);

	return NULL;
}

