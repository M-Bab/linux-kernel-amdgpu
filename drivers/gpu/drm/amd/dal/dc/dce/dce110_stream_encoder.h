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

#define SE_COMMON_REG_LIST_BASE(id) \
	SRI(AFMT_AVI_INFO0, DIG, id), \
	SRI(AFMT_AVI_INFO1, DIG, id), \
	SRI(AFMT_AVI_INFO2, DIG, id), \
	SRI(AFMT_AVI_INFO3, DIG, id), \
	SRI(AFMT_GENERIC_0, DIG, id), \
	SRI(AFMT_GENERIC_1, DIG, id), \
	SRI(AFMT_GENERIC_2, DIG, id), \
	SRI(AFMT_GENERIC_3, DIG, id), \
	SRI(AFMT_GENERIC_4, DIG, id), \
	SRI(AFMT_GENERIC_5, DIG, id), \
	SRI(AFMT_GENERIC_6, DIG, id), \
	SRI(AFMT_GENERIC_7, DIG, id), \
	SRI(AFMT_GENERIC_HDR, DIG, id), \
	SRI(AFMT_INFOFRAME_CONTROL0, DIG, id), \
	SRI(AFMT_VBI_PACKET_CONTROL, DIG, id), \
	SRI(AFMT_AUDIO_PACKET_CONTROL, DIG, id), \
	SRI(AFMT_AUDIO_PACKET_CONTROL2, DIG, id), \
	SRI(AFMT_AUDIO_SRC_CONTROL, DIG, id), \
	SRI(AFMT_60958_0, DIG, id), \
	SRI(AFMT_60958_1, DIG, id), \
	SRI(AFMT_60958_2, DIG, id), \
	SRI(DIG_FE_CNTL, DIG, id), \
	SRI(HDMI_CONTROL, DIG, id), \
	SRI(HDMI_GC, DIG, id), \
	SRI(HDMI_GENERIC_PACKET_CONTROL0, DIG, id), \
	SRI(HDMI_GENERIC_PACKET_CONTROL1, DIG, id), \
	SRI(HDMI_INFOFRAME_CONTROL0, DIG, id), \
	SRI(HDMI_INFOFRAME_CONTROL1, DIG, id), \
	SRI(HDMI_VBI_PACKET_CONTROL, DIG, id), \
	SRI(HDMI_AUDIO_PACKET_CONTROL, DIG, id),\
	SRI(HDMI_ACR_PACKET_CONTROL, DIG, id),\
	SRI(HDMI_ACR_32_0, DIG, id),\
	SRI(HDMI_ACR_32_1, DIG, id),\
	SRI(HDMI_ACR_44_0, DIG, id),\
	SRI(HDMI_ACR_44_1, DIG, id),\
	SRI(HDMI_ACR_48_0, DIG, id),\
	SRI(HDMI_ACR_48_1, DIG, id),\
	SRI(TMDS_CNTL, DIG, id), \
	SRI(DP_MSE_RATE_CNTL, DP, id), \
	SRI(DP_MSE_RATE_UPDATE, DP, id), \
	SRI(DP_PIXEL_FORMAT, DP, id), \
	SRI(DP_SEC_CNTL, DP, id), \
	SRI(DP_STEER_FIFO, DP, id), \
	SRI(DP_VID_M, DP, id), \
	SRI(DP_VID_N, DP, id), \
	SRI(DP_VID_STREAM_CNTL, DP, id), \
	SRI(DP_VID_TIMING, DP, id), \
	SRI(DP_SEC_AUD_N, DP, id), \
	SRI(DP_SEC_TIMESTAMP, DP, id)

#define SE_COMMON_REG_LIST(id)\
	SE_COMMON_REG_LIST_BASE(id), \
	SRI(AFMT_CNTL, DIG, id)

struct dce110_stream_enc_registers {
	uint32_t AFMT_CNTL;
	uint32_t AFMT_AVI_INFO0;
	uint32_t AFMT_AVI_INFO1;
	uint32_t AFMT_AVI_INFO2;
	uint32_t AFMT_AVI_INFO3;
	uint32_t AFMT_GENERIC_0;
	uint32_t AFMT_GENERIC_1;
	uint32_t AFMT_GENERIC_2;
	uint32_t AFMT_GENERIC_3;
	uint32_t AFMT_GENERIC_4;
	uint32_t AFMT_GENERIC_5;
	uint32_t AFMT_GENERIC_6;
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

void dce110_se_dp_audio_setup(
	struct stream_encoder *enc,
	unsigned int az_inst,
	struct audio_info *info);

void dce110_se_dp_audio_enable(
		struct stream_encoder *enc);

void dce110_se_dp_audio_disable(
		struct stream_encoder *enc);

void dce110_se_hdmi_audio_setup(
	struct stream_encoder *enc,
	unsigned int az_inst,
	struct audio_info *info,
	struct audio_crtc_info *audio_crtc_info);

void dce110_se_hdmi_audio_disable(
	struct stream_encoder *enc);

#endif /* __DC_STREAM_ENCODER_DCE110_H__ */
