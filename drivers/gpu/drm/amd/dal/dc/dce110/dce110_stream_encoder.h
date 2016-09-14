/*
 * Copyright 2012-15 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
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

#ifndef __DC_STREAM_ENCODER_DCE110_H__
#define __DC_STREAM_ENCODER_DCE110_H__

#include "stream_encoder.h"

#define DCE110STRENC_FROM_STRENC(stream_encoder)\
	container_of(stream_encoder, struct dce110_stream_encoder, base)

#define SE_REG(reg_name, block_prefix, id)\
	.reg_name = block_prefix ## id ## _ ## reg_name\

#define SE_COMMON_REG_LIST(id)\
	.AFMT_AVI_INFO0 = mmDIG ## id ## _AFMT_AVI_INFO0,\
	.AFMT_AVI_INFO1 = mmDIG ## id ## _AFMT_AVI_INFO1,\
	.AFMT_AVI_INFO2 = mmDIG ## id ## _AFMT_AVI_INFO2,\
	.AFMT_AVI_INFO3 = mmDIG ## id ## _AFMT_AVI_INFO3,\
	.AFMT_GENERIC_0 = mmDIG ## id ## _AFMT_GENERIC_0,\
	.AFMT_GENERIC_7 = mmDIG ## id ## _AFMT_GENERIC_7,\
	.AFMT_GENERIC_HDR = mmDIG ## id ## _AFMT_GENERIC_HDR,\
	.AFMT_INFOFRAME_CONTROL0 = mmDIG ## id ## _AFMT_INFOFRAME_CONTROL0,\
	.AFMT_VBI_PACKET_CONTROL = mmDIG ## id ## _AFMT_VBI_PACKET_CONTROL,\
	.AFMT_AUDIO_PACKET_CONTROL = mmDIG ## id ## _AFMT_AUDIO_PACKET_CONTROL,\
	.AFMT_AUDIO_PACKET_CONTROL2 = mmDIG ## id ## _AFMT_AUDIO_PACKET_CONTROL2,\
	.AFMT_AUDIO_SRC_CONTROL = mmDIG ## id ## _AFMT_AUDIO_SRC_CONTROL,\
	.AFMT_60958_0 = mmDIG ## id ## _AFMT_60958_0,\
	.AFMT_60958_1 = mmDIG ## id ## _AFMT_60958_1,\
	.AFMT_60958_2 = mmDIG ## id ## _AFMT_60958_2,\
	.DIG_FE_CNTL = mmDIG ## id ## _DIG_FE_CNTL,\
	.DP_MSE_RATE_CNTL = mmDP ## id ## _DP_MSE_RATE_CNTL,\
	.DP_MSE_RATE_UPDATE = mmDP ## id ## _DP_MSE_RATE_UPDATE,\
	.DP_PIXEL_FORMAT = mmDP ## id ## _DP_PIXEL_FORMAT,\
	.DP_SEC_CNTL = mmDP ## id ## _DP_SEC_CNTL,\
	.DP_STEER_FIFO = mmDP ## id ## _DP_STEER_FIFO,\
	.DP_VID_M = mmDP ## id ## _DP_VID_M,\
	.DP_VID_N = mmDP ## id ## _DP_VID_N,\
	.DP_VID_STREAM_CNTL = mmDP ## id ## _DP_VID_STREAM_CNTL,\
	.DP_VID_TIMING = mmDP ## id ## _DP_VID_TIMING,\
	.DP_SEC_AUD_N = mmDP ## id ## _DP_SEC_AUD_N,\
	.DP_SEC_TIMESTAMP = mmDP ## id ## _DP_SEC_TIMESTAMP,\
	.HDMI_CONTROL = mmDIG ## id ## _HDMI_CONTROL,\
	.HDMI_GC = mmDIG ## id ## _HDMI_GC,\
	.HDMI_GENERIC_PACKET_CONTROL0 = mmDIG ## id ## _HDMI_GENERIC_PACKET_CONTROL0,\
	.HDMI_GENERIC_PACKET_CONTROL1 = mmDIG ## id ## _HDMI_GENERIC_PACKET_CONTROL1,\
	.HDMI_INFOFRAME_CONTROL0 = mmDIG ## id ## _HDMI_INFOFRAME_CONTROL0,\
	.HDMI_INFOFRAME_CONTROL1 = mmDIG ## id ## _HDMI_INFOFRAME_CONTROL1,\
	.HDMI_VBI_PACKET_CONTROL = mmDIG ## id ## _HDMI_VBI_PACKET_CONTROL,\
	SE_REG(HDMI_AUDIO_PACKET_CONTROL, mmDIG, id),\
	SE_REG(HDMI_ACR_PACKET_CONTROL, mmDIG, id),\
	SE_REG(HDMI_ACR_32_0, mmDIG, id),\
	SE_REG(HDMI_ACR_32_1, mmDIG, id),\
	SE_REG(HDMI_ACR_44_0, mmDIG, id),\
	SE_REG(HDMI_ACR_44_1, mmDIG, id),\
	SE_REG(HDMI_ACR_48_0, mmDIG, id),\
	SE_REG(HDMI_ACR_48_1, mmDIG, id),\
	.TMDS_CNTL = mmDIG ## id ## _TMDS_CNTL,

struct dce110_stream_enc_registers {
	uint32_t AFMT_CNTL;
	uint32_t AFMT_AVI_INFO0;
	uint32_t AFMT_AVI_INFO1;
	uint32_t AFMT_AVI_INFO2;
	uint32_t AFMT_AVI_INFO3;
	uint32_t AFMT_GENERIC_0;
	uint32_t AFMT_GENERIC_7;
	uint32_t AFMT_GENERIC_HDR;
	uint32_t AFMT_INFOFRAME_CONTROL0;
	uint32_t AFMT_VBI_PACKET_CONTROL;
	uint32_t AFMT_AUDIO_PACKET_CONTROL;
	uint32_t AFMT_AUDIO_PACKET_CONTROL2;
	uint32_t AFMT_AUDIO_SRC_CONTROL;
	uint32_t AFMT_60958_0;
	uint32_t AFMT_60958_1;
	uint32_t AFMT_60958_2;
	uint32_t DIG_FE_CNTL;
	uint32_t DP_MSE_RATE_CNTL;
	uint32_t DP_MSE_RATE_UPDATE;
	uint32_t DP_PIXEL_FORMAT;
	uint32_t DP_SEC_CNTL;
	uint32_t DP_STEER_FIFO;
	uint32_t DP_VID_M;
	uint32_t DP_VID_N;
	uint32_t DP_VID_STREAM_CNTL;
	uint32_t DP_VID_TIMING;
	uint32_t DP_SEC_AUD_N;
	uint32_t DP_SEC_TIMESTAMP;
	uint32_t HDMI_CONTROL;
	uint32_t HDMI_GC;
	uint32_t HDMI_GENERIC_PACKET_CONTROL0;
	uint32_t HDMI_GENERIC_PACKET_CONTROL1;
	uint32_t HDMI_INFOFRAME_CONTROL0;
	uint32_t HDMI_INFOFRAME_CONTROL1;
	uint32_t HDMI_VBI_PACKET_CONTROL;
	uint32_t HDMI_AUDIO_PACKET_CONTROL;
	uint32_t HDMI_ACR_PACKET_CONTROL;
	uint32_t HDMI_ACR_32_0;
	uint32_t HDMI_ACR_32_1;
	uint32_t HDMI_ACR_44_0;
	uint32_t HDMI_ACR_44_1;
	uint32_t HDMI_ACR_48_0;
	uint32_t HDMI_ACR_48_1;
	uint32_t TMDS_CNTL;
};

struct dce110_stream_encoder {
	struct stream_encoder base;
	const struct dce110_stream_enc_registers *regs;
};

bool dce110_stream_encoder_construct(
	struct dce110_stream_encoder *enc110,
	struct dc_context *ctx,
	struct dc_bios *bp,
	enum engine_id eng_id,
	const struct dce110_stream_enc_registers *regs);


void dce110_se_audio_mute_control(
	struct stream_encoder *enc, bool mute);

void dce110_se_dp_audio_enable(
		struct stream_encoder *enc);

void dce110_se_dp_audio_disable(
		struct stream_encoder *enc);

#endif /* __DC_STREAM_ENCODER_DCE110_H__ */
