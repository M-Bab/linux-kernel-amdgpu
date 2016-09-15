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
#ifndef __DAL_AUDIO_DCE_110_H__
#define __DAL_AUDIO_DCE_110_H__

#include "audio.h"

#define AUD_COMMON_REG_LIST_BASE(id)\
	SRI(AZALIA_F0_CODEC_ENDPOINT_INDEX, AZF0ENDPOINT, id),\
	SRI(AZALIA_F0_CODEC_ENDPOINT_DATA, AZF0ENDPOINT, id),\
	SR(AZALIA_F0_CODEC_FUNCTION_PARAMETER_STREAM_FORMATS),\
	SR(AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES),\
	SR(AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES),\
	SR(DCCG_AUDIO_DTO_SOURCE),\
	SR(DCCG_AUDIO_DTO0_MODULE),\
	SR(DCCG_AUDIO_DTO0_PHASE),\
	SR(DCCG_AUDIO_DTO1_MODULE),\
	SR(DCCG_AUDIO_DTO1_PHASE)

#define AUD_COMMON_REG_LIST(id)\
	AUD_COMMON_REG_LIST_BASE(id)


struct dce110_audio_registers {
	uint32_t AZALIA_F0_CODEC_ENDPOINT_INDEX;
	uint32_t AZALIA_F0_CODEC_ENDPOINT_DATA;

	uint32_t AZALIA_F0_CODEC_FUNCTION_PARAMETER_STREAM_FORMATS;
	uint32_t AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES;
	uint32_t AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES;

	uint32_t DCCG_AUDIO_DTO_SOURCE;
	uint32_t DCCG_AUDIO_DTO0_MODULE;
	uint32_t DCCG_AUDIO_DTO0_PHASE;
	uint32_t DCCG_AUDIO_DTO1_MODULE;
	uint32_t DCCG_AUDIO_DTO1_PHASE;
};

struct audio_dce110 {
	struct audio base;
	const struct dce110_audio_registers *regs;
	/* dce-specific members are following */
	/* none */
};

struct audio *dce110_audio_create(
		struct dc_context *ctx,
		unsigned int inst,
		const struct dce110_audio_registers *reg);

void dce110_aud_destroy(struct audio **audio);

void dce110_aud_hw_init(struct audio *audio);

void dce110_aud_az_enable(struct audio *audio);
void dce110_aud_az_disable(struct audio *audio);

void dce110_aud_az_configure(struct audio *audio,
	enum signal_type signal,
	const struct audio_crtc_info *crtc_info,
	const struct audio_info *audio_info);

void dce110_aud_wall_dto_setup(struct audio *audio,
	enum signal_type signal,
	const struct audio_crtc_info *crtc_info,
	const struct audio_pll_info *pll_info);

#endif   /*__DAL_AUDIO_DCE_110_H__*/
