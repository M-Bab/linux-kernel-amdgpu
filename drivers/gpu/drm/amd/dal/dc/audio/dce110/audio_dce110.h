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

#include "audio/audio.h"
#include "audio/hw_ctx_audio.h"
#include "audio/dce110/hw_ctx_audio_dce110.h"

#define AUD_REG(reg_name, block_prefix, id)\
	.reg_name = block_prefix ## id ## _ ## reg_name

#define MM_REG(reg_name)\
	.reg_name = mm ## reg_name

#define AUD_COMMON_REG_LIST_BASE(id)\
	SE_REG(AZALIA_F0_CODEC_ENDPOINT_INDEX, mmAZF0ENDPOINT, id),\
	SE_REG(AZALIA_F0_CODEC_ENDPOINT_DATA, mmAZF0ENDPOINT, id),\
	MM_REG(AZALIA_F0_CODEC_FUNCTION_PARAMETER_STREAM_FORMATS),\
	MM_REG(AZALIA_F0_CODEC_FUNCTION_PARAMETER_SUPPORTED_SIZE_RATES),\
	MM_REG(AZALIA_F0_CODEC_FUNCTION_PARAMETER_POWER_STATES),\
	MM_REG(DCCG_AUDIO_DTO_SOURCE),\
	MM_REG(DCCG_AUDIO_DTO0_MODULE),\
	MM_REG(DCCG_AUDIO_DTO0_PHASE),\
	MM_REG(DCCG_AUDIO_DTO1_MODULE),\
	MM_REG(DCCG_AUDIO_DTO1_PHASE)

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

struct audio *dal_audio_create_dce110(const struct audio_init_data *init_data);

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
