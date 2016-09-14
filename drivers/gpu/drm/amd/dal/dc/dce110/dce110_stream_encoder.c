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

#include "dm_services.h"
#include "dc_bios_types.h"
#include "dce110_stream_encoder.h"

#include "dce/dce_11_0_d.h"
#include "dce/dce_11_0_sh_mask.h"
#include "dce/dce_11_0_enum.h"

#define LINK_REG(reg)\
	(enc110->regs->reg)

#define VBI_LINE_0 0
#define DP_BLANK_MAX_RETRY 20
#define HDMI_CLOCK_CHANNEL_RATE_MORE_340M 340000

#ifndef HDMI_CONTROL__HDMI_DATA_SCRAMBLE_EN_MASK
	#define HDMI_CONTROL__HDMI_DATA_SCRAMBLE_EN_MASK 0x2
	#define HDMI_CONTROL__HDMI_DATA_SCRAMBLE_EN__SHIFT 0x1
#endif

enum {
	DP_MST_UPDATE_MAX_RETRY = 50
};

static void dce110_update_generic_info_packet(
	struct dce110_stream_encoder *enc110,
	uint32_t packet_index,
	const struct encoder_info_packet *info_packet)
{
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t addr;
	uint32_t regval;
	/* choose which generic packet to use */
	{
		addr = LINK_REG(AFMT_VBI_PACKET_CONTROL);

		regval = dm_read_reg(ctx, addr);

		set_reg_field_value(
			regval,
			packet_index,
			AFMT_VBI_PACKET_CONTROL,
			AFMT_GENERIC_INDEX);

		dm_write_reg(ctx, addr, regval);
	}

	/* write generic packet header
	 * (4th byte is for GENERIC0 only) */
	{
		addr = LINK_REG(AFMT_GENERIC_HDR);

		regval = 0;

		set_reg_field_value(
			regval,
			info_packet->hb0,
			AFMT_GENERIC_HDR,
			AFMT_GENERIC_HB0);

		set_reg_field_value(
			regval,
			info_packet->hb1,
			AFMT_GENERIC_HDR,
			AFMT_GENERIC_HB1);

		set_reg_field_value(
			regval,
			info_packet->hb2,
			AFMT_GENERIC_HDR,
			AFMT_GENERIC_HB2);

		set_reg_field_value(
			regval,
			info_packet->hb3,
			AFMT_GENERIC_HDR,
			AFMT_GENERIC_HB3);

		dm_write_reg(ctx, addr, regval);
	}

	/* write generic packet contents
	 * (we never use last 4 bytes)
	 * there are 8 (0-7) mmDIG0_AFMT_GENERIC0_x registers */
	{
		const uint32_t *content =
			(const uint32_t *) &info_packet->sb[0];

		uint32_t counter = 0;

		addr = LINK_REG(AFMT_GENERIC_0);

		do {
			dm_write_reg(ctx, addr++, *content++);

			++counter;
		} while (counter < 7);
	}

	addr = LINK_REG(AFMT_GENERIC_7);

	dm_write_reg(
		ctx,
		addr,
		0);

	/* force double-buffered packet update */
	{
		addr = LINK_REG(AFMT_VBI_PACKET_CONTROL);

		regval = dm_read_reg(ctx, addr);

		set_reg_field_value(
			regval,
			(packet_index == 0),
			AFMT_VBI_PACKET_CONTROL,
			AFMT_GENERIC0_UPDATE);

		set_reg_field_value(
			regval,
			(packet_index == 2),
			AFMT_VBI_PACKET_CONTROL,
			AFMT_GENERIC2_UPDATE);

		dm_write_reg(ctx, addr, regval);
	}
}

static void dce110_update_hdmi_info_packet(
	struct dce110_stream_encoder *enc110,
	uint32_t packet_index,
	const struct encoder_info_packet *info_packet)
{
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t cont, send, line;
	uint32_t addr;
	uint32_t regval;

	if (info_packet->valid) {
		dce110_update_generic_info_packet(
			enc110,
			packet_index,
			info_packet);

		/* enable transmission of packet(s) -
		 * packet transmission begins on the next frame */
		cont = 1;
		/* send packet(s) every frame */
		send = 1;
		/* select line number to send packets on */
		line = 2;
	} else {
		cont = 0;
		send = 0;
		line = 0;
	}

	/* choose which generic packet control to use */

	switch (packet_index) {
	case 0:
	case 1:
		addr = LINK_REG(HDMI_GENERIC_PACKET_CONTROL0);
		break;
	case 2:
	case 3:
		addr = LINK_REG(HDMI_GENERIC_PACKET_CONTROL1);
		break;
	default:
		/* invalid HW packet index */
		dal_logger_write(
			ctx->logger,
			LOG_MAJOR_WARNING,
			LOG_MINOR_COMPONENT_ENCODER,
			"Invalid HW packet index: %s()\n",
			__func__);
		return;
	}

	regval = dm_read_reg(ctx, addr);

	switch (packet_index) {
	case 0:
	case 2:
		set_reg_field_value(
			regval,
			cont,
			HDMI_GENERIC_PACKET_CONTROL0,
			HDMI_GENERIC0_CONT);
		set_reg_field_value(
			regval,
			send,
			HDMI_GENERIC_PACKET_CONTROL0,
			HDMI_GENERIC0_SEND);
		set_reg_field_value(
			regval,
			line,
			HDMI_GENERIC_PACKET_CONTROL0,
			HDMI_GENERIC0_LINE);
		break;
	case 1:
	case 3:
		set_reg_field_value(
			regval,
			cont,
			HDMI_GENERIC_PACKET_CONTROL0,
			HDMI_GENERIC1_CONT);
		set_reg_field_value(
			regval,
			send,
			HDMI_GENERIC_PACKET_CONTROL0,
			HDMI_GENERIC1_SEND);
		set_reg_field_value(
			regval,
			line,
			HDMI_GENERIC_PACKET_CONTROL0,
			HDMI_GENERIC1_LINE);
		break;
	default:
		/* invalid HW packet index */
		dal_logger_write(
			ctx->logger,
			LOG_MAJOR_WARNING,
			LOG_MINOR_COMPONENT_ENCODER,
			"Invalid HW packet index: %s()\n",
			__func__);
		return;
	}

	dm_write_reg(ctx, addr, regval);
}

/* setup stream encoder in dp mode */
static void dce110_stream_encoder_dp_set_stream_attribute(
	struct stream_encoder *enc,
	struct dc_crtc_timing *crtc_timing)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	const uint32_t addr = LINK_REG(DP_PIXEL_FORMAT);
	uint32_t value = dm_read_reg(ctx, addr);

	/* set pixel encoding */
	switch (crtc_timing->pixel_encoding) {
	case PIXEL_ENCODING_YCBCR422:
		set_reg_field_value(
			value,
			DP_PIXEL_ENCODING_YCBCR422,
			DP_PIXEL_FORMAT,
			DP_PIXEL_ENCODING);
		break;
	case PIXEL_ENCODING_YCBCR444:
		set_reg_field_value(
			value,
			DP_PIXEL_ENCODING_YCBCR444,
			DP_PIXEL_FORMAT,
			DP_PIXEL_ENCODING);

		if (crtc_timing->flags.Y_ONLY)
			if (crtc_timing->display_color_depth != COLOR_DEPTH_666)
				/* HW testing only, no use case yet.
				 * Color depth of Y-only could be
				 * 8, 10, 12, 16 bits */
				set_reg_field_value(
					value,
					DP_PIXEL_ENCODING_Y_ONLY,
					DP_PIXEL_FORMAT,
					DP_PIXEL_ENCODING);
		/* Note: DP_MSA_MISC1 bit 7 is the indicator
		 * of Y-only mode.
		 * This bit is set in HW if register
		 * DP_PIXEL_ENCODING is programmed to 0x4 */
		break;
	default:
		set_reg_field_value(
			value,
			DP_PIXEL_ENCODING_RGB444,
			DP_PIXEL_FORMAT,
			DP_PIXEL_ENCODING);
		break;
	}

	/* set color depth */

	switch (crtc_timing->display_color_depth) {
	case COLOR_DEPTH_888:
		set_reg_field_value(
			value,
			DP_COMPONENT_DEPTH_8BPC,
			DP_PIXEL_FORMAT,
			DP_COMPONENT_DEPTH);
		break;
	case COLOR_DEPTH_101010:
		set_reg_field_value(
			value,
			DP_COMPONENT_DEPTH_10BPC,
			DP_PIXEL_FORMAT,
			DP_COMPONENT_DEPTH);
		break;
	case COLOR_DEPTH_121212:
		set_reg_field_value(
			value,
			DP_COMPONENT_DEPTH_12BPC,
			DP_PIXEL_FORMAT,
			DP_COMPONENT_DEPTH);
		break;
	default:
		set_reg_field_value(
			value,
			DP_COMPONENT_DEPTH_6BPC,
			DP_PIXEL_FORMAT,
			DP_COMPONENT_DEPTH);
		break;
	}

	/* set dynamic range and YCbCr range */
	set_reg_field_value(value, 0, DP_PIXEL_FORMAT, DP_DYN_RANGE);
	set_reg_field_value(value, 0, DP_PIXEL_FORMAT, DP_YCBCR_RANGE);

	dm_write_reg(ctx, addr, value);
}

/* setup stream encoder in hdmi mode */
static void dce110_stream_encoder_hdmi_set_stream_attribute(
	struct stream_encoder *enc,
	struct dc_crtc_timing *crtc_timing,
	int actual_pix_clk_khz,
	bool enable_audio)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t value;
	uint32_t addr;
	struct bp_encoder_control cntl = {0};

	cntl.action = ENCODER_CONTROL_SETUP;
	cntl.engine_id = enc110->base.id;
	cntl.signal = SIGNAL_TYPE_HDMI_TYPE_A;
	cntl.enable_dp_audio = enable_audio;
	cntl.pixel_clock = actual_pix_clk_khz;
	cntl.lanes_number = LANE_COUNT_FOUR;

	if (enc110->base.bp->funcs->encoder_control(
			enc110->base.bp, &cntl) != BP_RESULT_OK)
		return;

	addr = LINK_REG(DIG_FE_CNTL);
	value = dm_read_reg(ctx, addr);

	switch (crtc_timing->pixel_encoding) {
	case PIXEL_ENCODING_YCBCR422:
		set_reg_field_value(value, 1, DIG_FE_CNTL, TMDS_PIXEL_ENCODING);
		break;
	default:
		set_reg_field_value(value, 0, DIG_FE_CNTL, TMDS_PIXEL_ENCODING);
		break;
	}
	set_reg_field_value(value, 0, DIG_FE_CNTL, TMDS_COLOR_FORMAT);
	dm_write_reg(ctx, addr, value);

	/* setup HDMI engine */
	addr = LINK_REG(HDMI_CONTROL);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(value, 1, HDMI_CONTROL, HDMI_PACKET_GEN_VERSION);
	set_reg_field_value(value, 1, HDMI_CONTROL, HDMI_KEEPOUT_MODE);
	set_reg_field_value(value, 0, HDMI_CONTROL, HDMI_DEEP_COLOR_ENABLE);
	set_reg_field_value(value, 0, HDMI_CONTROL, HDMI_DATA_SCRAMBLE_EN);
	set_reg_field_value(value, 0, HDMI_CONTROL, HDMI_CLOCK_CHANNEL_RATE);

	switch (crtc_timing->display_color_depth) {
	case COLOR_DEPTH_888:
		set_reg_field_value(
			value,
			0,
			HDMI_CONTROL,
			HDMI_DEEP_COLOR_DEPTH);
		break;
	case COLOR_DEPTH_101010:
		set_reg_field_value(
			value,
			1,
			HDMI_CONTROL,
			HDMI_DEEP_COLOR_DEPTH);
		set_reg_field_value(
			value,
			1,
			HDMI_CONTROL,
			HDMI_DEEP_COLOR_ENABLE);
		break;
	case COLOR_DEPTH_121212:
		set_reg_field_value(
			value,
			2,
			HDMI_CONTROL,
			HDMI_DEEP_COLOR_DEPTH);
		set_reg_field_value(
			value,
			1,
			HDMI_CONTROL,
			HDMI_DEEP_COLOR_ENABLE);
		break;
	case COLOR_DEPTH_161616:
		set_reg_field_value(
			value,
			3,
			HDMI_CONTROL,
			HDMI_DEEP_COLOR_DEPTH);
		set_reg_field_value(
			value,
			1,
			HDMI_CONTROL,
			HDMI_DEEP_COLOR_ENABLE);
		break;
	default:
		break;
	}

	if (actual_pix_clk_khz >= HDMI_CLOCK_CHANNEL_RATE_MORE_340M) {
		/* enable HDMI data scrambler */
		set_reg_field_value(
			value,
			1,
			HDMI_CONTROL,
			HDMI_DATA_SCRAMBLE_EN);

		/* HDMI_CLOCK_CHANNEL_RATE_MORE_340M
		 * Clock channel frequency is 1/4 of character rate.*/
		set_reg_field_value(
			value,
			1,
			HDMI_CONTROL,
			HDMI_CLOCK_CHANNEL_RATE);
	} else if (crtc_timing->flags.LTE_340MCSC_SCRAMBLE) {

		/* TODO: New feature for DCE11, still need to implement */

		/* enable HDMI data scrambler */
		set_reg_field_value(
			value,
			1,
			HDMI_CONTROL,
			HDMI_DATA_SCRAMBLE_EN);

		/* HDMI_CLOCK_CHANNEL_FREQ_EQUAL_TO_CHAR_RATE
		 * Clock channel frequency is the same
		 * as character rate */
		set_reg_field_value(
			value,
			0,
			HDMI_CONTROL,
			HDMI_CLOCK_CHANNEL_RATE);
	}

	dm_write_reg(ctx, addr, value);

	addr = LINK_REG(HDMI_VBI_PACKET_CONTROL);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(value, 1, HDMI_VBI_PACKET_CONTROL, HDMI_GC_CONT);
	set_reg_field_value(value, 1, HDMI_VBI_PACKET_CONTROL, HDMI_GC_SEND);
	set_reg_field_value(value, 1, HDMI_VBI_PACKET_CONTROL, HDMI_NULL_SEND);

	dm_write_reg(ctx, addr, value);

	/* following belongs to audio */
	addr = LINK_REG(HDMI_INFOFRAME_CONTROL0);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(
		value,
		1,
		HDMI_INFOFRAME_CONTROL0,
		HDMI_AUDIO_INFO_SEND);
	dm_write_reg(ctx, addr, value);

	addr = LINK_REG(AFMT_INFOFRAME_CONTROL0);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(
		value,
		1,
		AFMT_INFOFRAME_CONTROL0,
		AFMT_AUDIO_INFO_UPDATE);
	dm_write_reg(ctx, addr, value);

	addr = LINK_REG(HDMI_INFOFRAME_CONTROL1);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(
		value,
		VBI_LINE_0 + 2,
		HDMI_INFOFRAME_CONTROL1,
		HDMI_AUDIO_INFO_LINE);
	dm_write_reg(ctx, addr, value);

	addr = LINK_REG(HDMI_GC);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(value, 0, HDMI_GC, HDMI_GC_AVMUTE);
	dm_write_reg(ctx, addr, value);
}

/* setup stream encoder in dvi mode */
static void dce110_stream_encoder_dvi_set_stream_attribute(
	struct stream_encoder *enc,
	struct dc_crtc_timing *crtc_timing,
	bool is_dual_link)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t addr = LINK_REG(DIG_FE_CNTL);
	uint32_t value = dm_read_reg(ctx, addr);
	struct bp_encoder_control cntl = {0};

	cntl.action = ENCODER_CONTROL_SETUP;
	cntl.engine_id = enc110->base.id;
	cntl.signal = is_dual_link ?
			SIGNAL_TYPE_DVI_DUAL_LINK : SIGNAL_TYPE_DVI_SINGLE_LINK;
	cntl.enable_dp_audio = false;
	cntl.pixel_clock = crtc_timing->pix_clk_khz;
	cntl.lanes_number = (is_dual_link) ? LANE_COUNT_EIGHT : LANE_COUNT_FOUR;

	if (enc110->base.bp->funcs->encoder_control(
			enc110->base.bp, &cntl) != BP_RESULT_OK)
		return;

	switch (crtc_timing->pixel_encoding) {
	case PIXEL_ENCODING_YCBCR422:
		set_reg_field_value(value, 1, DIG_FE_CNTL, TMDS_PIXEL_ENCODING);
		break;
	default:
		set_reg_field_value(value, 0, DIG_FE_CNTL, TMDS_PIXEL_ENCODING);
		break;
	}

	switch (crtc_timing->display_color_depth) {
	case COLOR_DEPTH_101010:
		if (crtc_timing->pixel_encoding == PIXEL_ENCODING_RGB)
			set_reg_field_value(
				value,
				2,
				DIG_FE_CNTL,
				TMDS_COLOR_FORMAT);
		else
			set_reg_field_value(
				value,
				0,
				DIG_FE_CNTL,
				TMDS_COLOR_FORMAT);
		break;
	default:
		set_reg_field_value(value, 0, DIG_FE_CNTL, TMDS_COLOR_FORMAT);
		break;
	}
	dm_write_reg(ctx, addr, value);
}

static void dce110_stream_encoder_set_mst_bandwidth(
	struct stream_encoder *enc,
	struct fixed31_32 avg_time_slots_per_mtp)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t addr;
	uint32_t field;
	uint32_t value;
	uint32_t retries = 0;
	uint32_t x = dal_fixed31_32_floor(
		avg_time_slots_per_mtp);
	uint32_t y = dal_fixed31_32_ceil(
		dal_fixed31_32_shl(
			dal_fixed31_32_sub_int(
				avg_time_slots_per_mtp,
				x),
			26));

	{
		addr = LINK_REG(DP_MSE_RATE_CNTL);
		value = dm_read_reg(ctx, addr);

		set_reg_field_value(
			value,
			x,
			DP_MSE_RATE_CNTL,
			DP_MSE_RATE_X);

		set_reg_field_value(
			value,
			y,
			DP_MSE_RATE_CNTL,
			DP_MSE_RATE_Y);

		dm_write_reg(ctx, addr, value);
	}

	/* wait for update to be completed on the link */
	/* i.e. DP_MSE_RATE_UPDATE_PENDING field (read only) */
	/* is reset to 0 (not pending) */
	{
		addr = LINK_REG(DP_MSE_RATE_UPDATE);

		do {
			value = dm_read_reg(ctx, addr);

			field = get_reg_field_value(
					value,
					DP_MSE_RATE_UPDATE,
					DP_MSE_RATE_UPDATE_PENDING);

			if (!(field &
			DP_MSE_RATE_UPDATE__DP_MSE_RATE_UPDATE_PENDING_MASK))
				break;

			udelay(10);

			++retries;
		} while (retries < DP_MST_UPDATE_MAX_RETRY);
	}
}

static void dce110_stream_encoder_update_hdmi_info_packets(
	struct stream_encoder *enc,
	const struct encoder_info_frame *info_frame)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t regval;
	uint32_t addr;
	uint32_t control0val;
	uint32_t control1val;

	if (info_frame->avi.valid) {
		const uint32_t *content =
			(const uint32_t *) &info_frame->avi.sb[0];

		addr = LINK_REG(AFMT_AVI_INFO0);
		regval = content[0];
		dm_write_reg(
			ctx,
			addr,
			regval);
		regval = content[1];

		addr = LINK_REG(AFMT_AVI_INFO1);
		dm_write_reg(
			ctx,
			addr,
			regval);
		regval = content[2];

		addr = LINK_REG(AFMT_AVI_INFO2);
		dm_write_reg(
			ctx,
			addr,
			regval);
		regval = content[3];

		/* move version to AVI_INFO3 */
		addr = LINK_REG(AFMT_AVI_INFO3);
		set_reg_field_value(
			regval,
			info_frame->avi.hb1,
			AFMT_AVI_INFO3,
			AFMT_AVI_INFO_VERSION);

		dm_write_reg(
			ctx,
			addr,
			regval);

		addr = LINK_REG(HDMI_INFOFRAME_CONTROL0);

		control0val = dm_read_reg(ctx, addr);

		set_reg_field_value(
			control0val,
			1,
			HDMI_INFOFRAME_CONTROL0,
			HDMI_AVI_INFO_SEND);

		set_reg_field_value(
			control0val,
			1,
			HDMI_INFOFRAME_CONTROL0,
			HDMI_AVI_INFO_CONT);

		dm_write_reg(ctx, addr, control0val);

		addr = LINK_REG(HDMI_INFOFRAME_CONTROL1);

		control1val = dm_read_reg(ctx, addr);

		set_reg_field_value(
			control1val,
			VBI_LINE_0 + 2,
			HDMI_INFOFRAME_CONTROL1,
			HDMI_AVI_INFO_LINE);

		dm_write_reg(ctx, addr, control1val);
	} else {
		addr = LINK_REG(HDMI_INFOFRAME_CONTROL0);

		regval = dm_read_reg(ctx, addr);

		set_reg_field_value(
			regval,
			0,
			HDMI_INFOFRAME_CONTROL0,
			HDMI_AVI_INFO_SEND);

		set_reg_field_value(
			regval,
			0,
			HDMI_INFOFRAME_CONTROL0,
			HDMI_AVI_INFO_CONT);

		dm_write_reg(ctx, addr, regval);
	}

	dce110_update_hdmi_info_packet(enc110, 0, &info_frame->vendor);
	dce110_update_hdmi_info_packet(enc110, 1, &info_frame->gamut);
	dce110_update_hdmi_info_packet(enc110, 2, &info_frame->spd);
}

static void dce110_stream_encoder_stop_hdmi_info_packets(
	struct stream_encoder *enc)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t addr = 0;
	uint32_t value = 0;

	/* stop generic packets 0 & 1 on HDMI */
	addr = LINK_REG(HDMI_GENERIC_PACKET_CONTROL0);

	value = dm_read_reg(ctx, addr);

	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL0,
		HDMI_GENERIC1_CONT);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL0,
		HDMI_GENERIC1_LINE);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL0,
		HDMI_GENERIC1_SEND);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL0,
		HDMI_GENERIC0_CONT);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL0,
		HDMI_GENERIC0_LINE);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL0,
		HDMI_GENERIC0_SEND);

	dm_write_reg(ctx, addr, value);

	/* stop generic packets 2 & 3 on HDMI */
	addr = LINK_REG(HDMI_GENERIC_PACKET_CONTROL1);

	value = dm_read_reg(ctx, addr);

	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL1,
		HDMI_GENERIC2_CONT);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL1,
		HDMI_GENERIC2_LINE);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL1,
		HDMI_GENERIC2_SEND);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL1,
		HDMI_GENERIC3_CONT);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL1,
		HDMI_GENERIC3_LINE);
	set_reg_field_value(
		value,
		0,
		HDMI_GENERIC_PACKET_CONTROL1,
		HDMI_GENERIC3_SEND);

	dm_write_reg(ctx, addr, value);

	/* stop AVI packet on HDMI */
	addr = LINK_REG(HDMI_INFOFRAME_CONTROL0);

	value = dm_read_reg(ctx, addr);

	set_reg_field_value(
		value,
		0,
		HDMI_INFOFRAME_CONTROL0,
		HDMI_AVI_INFO_SEND);
	set_reg_field_value(
		value,
		0,
		HDMI_INFOFRAME_CONTROL0,
		HDMI_AVI_INFO_CONT);

	dm_write_reg(ctx, addr, value);
}

static void dce110_stream_encoder_update_dp_info_packets(
	struct stream_encoder *enc,
	const struct encoder_info_frame *info_frame)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t addr = LINK_REG(DP_SEC_CNTL);
	uint32_t value;

	if (info_frame->vsc.valid)
		dce110_update_generic_info_packet(
			enc110,
			0,
			&info_frame->vsc);

	/* enable/disable transmission of packet(s).
	*  If enabled, packet transmission begins on the next frame
	*/

	value = dm_read_reg(ctx, addr);

	set_reg_field_value(
		value,
		info_frame->vsc.valid,
		DP_SEC_CNTL,
		DP_SEC_GSP0_ENABLE);
	/* This bit is the master enable bit.
	* When enabling secondary stream engine,
	* this master bit must also be set.
	* This register shared with audio info frame.
	* Therefore we need to enable master bit
	* if at least on of the fields is not 0
	*/
	if (value)
		set_reg_field_value(
			value,
			1,
			DP_SEC_CNTL,
			DP_SEC_STREAM_ENABLE);

	dm_write_reg(ctx, addr, value);
}

static void dce110_stream_encoder_stop_dp_info_packets(
	struct stream_encoder *enc)
{
	/* stop generic packets on DP */
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t addr = LINK_REG(DP_SEC_CNTL);
	uint32_t value = dm_read_reg(ctx, addr);

	set_reg_field_value(value, 0, DP_SEC_CNTL, DP_SEC_GSP0_ENABLE);
	set_reg_field_value(value, 0, DP_SEC_CNTL, DP_SEC_GSP1_ENABLE);
	set_reg_field_value(value, 0, DP_SEC_CNTL, DP_SEC_GSP2_ENABLE);
	set_reg_field_value(value, 0, DP_SEC_CNTL, DP_SEC_GSP3_ENABLE);
	set_reg_field_value(value, 0, DP_SEC_CNTL, DP_SEC_AVI_ENABLE);
	set_reg_field_value(value, 0, DP_SEC_CNTL, DP_SEC_MPG_ENABLE);
	set_reg_field_value(value, 0, DP_SEC_CNTL, DP_SEC_STREAM_ENABLE);

	/* this register shared with audio info frame.
	 * therefore we need to keep master enabled
	 * if at least one of the fields is not 0 */

	if (value)
		set_reg_field_value(
			value,
			1,
			DP_SEC_CNTL,
			DP_SEC_STREAM_ENABLE);

	dm_write_reg(ctx, addr, value);
}

static void dce110_stream_encoder_dp_blank(
	struct stream_encoder *enc)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t addr = LINK_REG(DP_VID_STREAM_CNTL);
	uint32_t value = dm_read_reg(ctx, addr);
	uint32_t retries = 0;
	uint32_t max_retries = DP_BLANK_MAX_RETRY * 10;

	/* Note: For CZ, we are changing driver default to disable
	 * stream deferred to next VBLANK. If results are positive, we
	 * will make the same change to all DCE versions. There are a
	 * handful of panels that cannot handle disable stream at
	 * HBLANK and will result in a white line flash across the
	 * screen on stream disable. */

	/* Specify the video stream disable point
	 * (2 = start of the next vertical blank) */
	set_reg_field_value(
		value,
		2,
		DP_VID_STREAM_CNTL,
		DP_VID_STREAM_DIS_DEFER);
	/* Larger delay to wait until VBLANK - use max retry of
	* 10us*3000=30ms. This covers 16.6ms of typical 60 Hz mode +
	* a little more because we may not trust delay accuracy.
	*/
	max_retries = DP_BLANK_MAX_RETRY * 150;

	/* disable DP stream */
	set_reg_field_value(value, 0, DP_VID_STREAM_CNTL, DP_VID_STREAM_ENABLE);
	dm_write_reg(ctx, addr, value);

	/* the encoder stops sending the video stream
	* at the start of the vertical blanking.
	* Poll for DP_VID_STREAM_STATUS == 0
	*/

	do {
		value = dm_read_reg(ctx, addr);

		if (!get_reg_field_value(
			value,
			DP_VID_STREAM_CNTL,
			DP_VID_STREAM_STATUS))
			break;

		udelay(10);

		++retries;
	} while (retries < max_retries);

	ASSERT(retries <= max_retries);

	/* Tell the DP encoder to ignore timing from CRTC, must be done after
	* the polling. If we set DP_STEER_FIFO_RESET before DP stream blank is
	* complete, stream status will be stuck in video stream enabled state,
	* i.e. DP_VID_STREAM_STATUS stuck at 1.
	*/
	addr = LINK_REG(DP_STEER_FIFO);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(value, true, DP_STEER_FIFO, DP_STEER_FIFO_RESET);
	dm_write_reg(ctx, addr, value);
}

/* output video stream to link encoder */
static void dce110_stream_encoder_dp_unblank(
	struct stream_encoder *enc,
	const struct encoder_unblank_param *param)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);
	struct dc_context *ctx = enc110->base.ctx;
	uint32_t addr;
	uint32_t value;

	if (param->link_settings.link_rate != LINK_RATE_UNKNOWN) {
		uint32_t n_vid = 0x8000;
		uint32_t m_vid;

		/* M / N = Fstream / Flink
		* m_vid / n_vid = pixel rate / link rate
		*/

		uint64_t m_vid_l = n_vid;

		m_vid_l *= param->crtc_timing.pixel_clock;
		m_vid_l = div_u64(m_vid_l,
			param->link_settings.link_rate
				* LINK_RATE_REF_FREQ_IN_KHZ);

		m_vid = (uint32_t) m_vid_l;

		/* enable auto measurement */
		addr = LINK_REG(DP_VID_TIMING);
		value = dm_read_reg(ctx, addr);
		set_reg_field_value(value, 0, DP_VID_TIMING, DP_VID_M_N_GEN_EN);
		dm_write_reg(ctx, addr, value);

		/* auto measurement need 1 full 0x8000 symbol cycle to kick in,
		* therefore program initial value for Mvid and Nvid
		*/
		addr = LINK_REG(DP_VID_N);
		value = dm_read_reg(ctx, addr);
		set_reg_field_value(value, n_vid, DP_VID_N, DP_VID_N);
		dm_write_reg(ctx, addr, value);

		addr = LINK_REG(DP_VID_M);
		value = dm_read_reg(ctx, addr);
		set_reg_field_value(value, m_vid, DP_VID_M, DP_VID_M);
		dm_write_reg(ctx, addr, value);

		addr = LINK_REG(DP_VID_TIMING);
		value = dm_read_reg(ctx, addr);
		set_reg_field_value(value, 1, DP_VID_TIMING, DP_VID_M_N_GEN_EN);
		dm_write_reg(ctx, addr, value);
	}

	/* set DIG_START to 0x1 to resync FIFO */
	addr = LINK_REG(DIG_FE_CNTL);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(value, 1, DIG_FE_CNTL, DIG_START);
	dm_write_reg(ctx, addr, value);

	/* switch DP encoder to CRTC data */
	addr = LINK_REG(DP_STEER_FIFO);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(value, 0, DP_STEER_FIFO, DP_STEER_FIFO_RESET);
	dm_write_reg(ctx, addr, value);

	/* wait 100us for DIG/DP logic to prime
	* (i.e. a few video lines)
	*/
	udelay(100);

	/* the hardware would start sending video at the start of the next DP
	* frame (i.e. rising edge of the vblank).
	* NOTE: We used to program DP_VID_STREAM_DIS_DEFER = 2 here, but this
	* register has no effect on enable transition! HW always guarantees
	* VID_STREAM enable at start of next frame, and this is not
	* programmable
	*/
	addr = LINK_REG(DP_VID_STREAM_CNTL);
	value = dm_read_reg(ctx, addr);
	set_reg_field_value(
		value,
		true,
		DP_VID_STREAM_CNTL,
		DP_VID_STREAM_ENABLE);
	dm_write_reg(ctx, addr, value);
}

#define LINK_REG_READ(reg_name) \
		dm_read_reg(enc110->base.ctx, LINK_REG(reg_name))

#define LINK_REG_WRITE(reg_name, value) \
		dm_write_reg(enc110->base.ctx, LINK_REG(reg_name), value)

#define LINK_REG_SET_N(reg_name, n, ...)	\
		generic_reg_update_ex(enc110->base.ctx, \
				LINK_REG(reg_name), \
				0, \
				n, __VA_ARGS__)

#define LINK_REG_SET(reg_name, field, val)	\
		LINK_REG_SET_N(reg_name, 1, FD(reg_name##__##field), val)

#define LINK_REG_UPDATE_N(reg_name, n, ...)	\
		generic_reg_update_ex(enc110->base.ctx, \
				LINK_REG(reg_name), \
				LINK_REG_READ(reg_name), \
				n, __VA_ARGS__)

#define LINK_REG_UPDATE(reg_name, field, val)	\
		LINK_REG_UPDATE_N(reg_name, 1, FD(reg_name##__##field), val)

#define LINK_REG_WAIT(reg_name, field, val, delay, max_try)	\
		generic_reg_wait(enc110->base.ctx, \
				LINK_REG(reg_name), FD(reg_name##__##field), val,\
				delay, max_try)

#define DP_SEC_AUD_N__DP_SEC_AUD_N__DEFAULT 0x8000
#define DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE__AUTO_CALC 1

#include "include/audio_types.h"

/**
* speakersToChannels
*
* @brief
*  translate speakers to channels
*
*  FL  - Front Left
*  FR  - Front Right
*  RL  - Rear Left
*  RR  - Rear Right
*  RC  - Rear Center
*  FC  - Front Center
*  FLC - Front Left Center
*  FRC - Front Right Center
*  RLC - Rear Left Center
*  RRC - Rear Right Center
*  LFE - Low Freq Effect
*
*               FC
*          FLC      FRC
*    FL                    FR
*
*                    LFE
*              ()
*
*
*    RL                    RR
*          RLC      RRC
*               RC
*
*             ch  8   7   6   5   4   3   2   1
* 0b00000011      -   -   -   -   -   -   FR  FL
* 0b00000111      -   -   -   -   -   LFE FR  FL
* 0b00001011      -   -   -   -   FC  -   FR  FL
* 0b00001111      -   -   -   -   FC  LFE FR  FL
* 0b00010011      -   -   -   RC  -   -   FR  FL
* 0b00010111      -   -   -   RC  -   LFE FR  FL
* 0b00011011      -   -   -   RC  FC  -   FR  FL
* 0b00011111      -   -   -   RC  FC  LFE FR  FL
* 0b00110011      -   -   RR  RL  -   -   FR  FL
* 0b00110111      -   -   RR  RL  -   LFE FR  FL
* 0b00111011      -   -   RR  RL  FC  -   FR  FL
* 0b00111111      -   -   RR  RL  FC  LFE FR  FL
* 0b01110011      -   RC  RR  RL  -   -   FR  FL
* 0b01110111      -   RC  RR  RL  -   LFE FR  FL
* 0b01111011      -   RC  RR  RL  FC  -   FR  FL
* 0b01111111      -   RC  RR  RL  FC  LFE FR  FL
* 0b11110011      RRC RLC RR  RL  -   -   FR  FL
* 0b11110111      RRC RLC RR  RL  -   LFE FR  FL
* 0b11111011      RRC RLC RR  RL  FC  -   FR  FL
* 0b11111111      RRC RLC RR  RL  FC  LFE FR  FL
* 0b11000011      FRC FLC -   -   -   -   FR  FL
* 0b11000111      FRC FLC -   -   -   LFE FR  FL
* 0b11001011      FRC FLC -   -   FC  -   FR  FL
* 0b11001111      FRC FLC -   -   FC  LFE FR  FL
* 0b11010011      FRC FLC -   RC  -   -   FR  FL
* 0b11010111      FRC FLC -   RC  -   LFE FR  FL
* 0b11011011      FRC FLC -   RC  FC  -   FR  FL
* 0b11011111      FRC FLC -   RC  FC  LFE FR  FL
* 0b11110011      FRC FLC RR  RL  -   -   FR  FL
* 0b11110111      FRC FLC RR  RL  -   LFE FR  FL
* 0b11111011      FRC FLC RR  RL  FC  -   FR  FL
* 0b11111111      FRC FLC RR  RL  FC  LFE FR  FL
*
* @param
*  speakers - speaker information as it comes from CEA audio block
*/
/* translate speakers to channels */

union audio_cea_channels {
	uint8_t all;
	struct audio_cea_channels_bits {
		uint32_t FL:1;
		uint32_t FR:1;
		uint32_t LFE:1;
		uint32_t FC:1;
		uint32_t RL_RC:1;
		uint32_t RR:1;
		uint32_t RC_RLC_FLC:1;
		uint32_t RRC_FRC:1;
	} channels;
};

struct audio_clock_info {
	/* pixel clock frequency*/
	uint32_t pixel_clock_in_10khz;
	/* N - 32KHz audio */
	uint32_t n_32khz;
	/* CTS - 32KHz audio*/
	uint32_t cts_32khz;
	uint32_t n_44khz;
	uint32_t cts_44khz;
	uint32_t n_48khz;
	uint32_t cts_48khz;
};

/* 25.2MHz/1.001*/
/* 25.2MHz/1.001*/
/* 25.2MHz*/
/* 27MHz */
/* 27MHz*1.001*/
/* 27MHz*1.001*/
/* 54MHz*/
/* 54MHz*1.001*/
/* 74.25MHz/1.001*/
/* 74.25MHz*/
/* 148.5MHz/1.001*/
/* 148.5MHz*/

static const struct audio_clock_info audio_clock_info_table[12] = {
	{2517, 4576, 28125, 7007, 31250, 6864, 28125},
	{2518, 4576, 28125, 7007, 31250, 6864, 28125},
	{2520, 4096, 25200, 6272, 28000, 6144, 25200},
	{2700, 4096, 27000, 6272, 30000, 6144, 27000},
	{2702, 4096, 27027, 6272, 30030, 6144, 27027},
	{2703, 4096, 27027, 6272, 30030, 6144, 27027},
	{5400, 4096, 54000, 6272, 60000, 6144, 54000},
	{5405, 4096, 54054, 6272, 60060, 6144, 54054},
	{7417, 11648, 210937, 17836, 234375, 11648, 140625},
	{7425, 4096, 74250, 6272, 82500, 6144, 74250},
	{14835, 11648, 421875, 8918, 234375, 5824, 140625},
	{14850, 4096, 148500, 6272, 165000, 6144, 148500}
};

static const struct audio_clock_info audio_clock_info_table_36bpc[12] = {
	{2517, 9152, 84375, 7007, 48875, 9152, 56250},
	{2518, 9152, 84375, 7007, 48875, 9152, 56250},
	{2520, 4096, 37800, 6272, 42000, 6144, 37800},
	{2700, 4096, 40500, 6272, 45000, 6144, 40500},
	{2702, 8192, 81081, 6272, 45045, 8192, 54054},
	{2703, 8192, 81081, 6272, 45045, 8192, 54054},
	{5400, 4096, 81000, 6272, 90000, 6144, 81000},
	{5405, 4096, 81081, 6272, 90090, 6144, 81081},
	{7417, 11648, 316406, 17836, 351562, 11648, 210937},
	{7425, 4096, 111375, 6272, 123750, 6144, 111375},
	{14835, 11648, 632812, 17836, 703125, 11648, 421875},
	{14850, 4096, 222750, 6272, 247500, 6144, 222750}
};

static const struct audio_clock_info audio_clock_info_table_48bpc[12] = {
	{2517, 4576, 56250, 7007, 62500, 6864, 56250},
	{2518, 4576, 56250, 7007, 62500, 6864, 56250},
	{2520, 4096, 50400, 6272, 56000, 6144, 50400},
	{2700, 4096, 54000, 6272, 60000, 6144, 54000},
	{2702, 4096, 54054, 6267, 60060, 8192, 54054},
	{2703, 4096, 54054, 6272, 60060, 8192, 54054},
	{5400, 4096, 108000, 6272, 120000, 6144, 108000},
	{5405, 4096, 108108, 6272, 120120, 6144, 108108},
	{7417, 11648, 421875, 17836, 468750, 11648, 281250},
	{7425, 4096, 148500, 6272, 165000, 6144, 148500},
	{14835, 11648, 843750, 8918, 468750, 11648, 281250},
	{14850, 4096, 297000, 6272, 330000, 6144, 297000}
};

union audio_cea_channels speakers_to_channels(
	struct audio_speaker_flags speaker_flags)
{
	union audio_cea_channels cea_channels = {0};

	/* these are one to one */
	cea_channels.channels.FL = speaker_flags.FL_FR;
	cea_channels.channels.FR = speaker_flags.FL_FR;
	cea_channels.channels.LFE = speaker_flags.LFE;
	cea_channels.channels.FC = speaker_flags.FC;

	/* if Rear Left and Right exist move RC speaker to channel 7
	 * otherwise to channel 5
	 */
	if (speaker_flags.RL_RR) {
		cea_channels.channels.RL_RC = speaker_flags.RL_RR;
		cea_channels.channels.RR = speaker_flags.RL_RR;
		cea_channels.channels.RC_RLC_FLC = speaker_flags.RC;
	} else {
		cea_channels.channels.RL_RC = speaker_flags.RC;
	}

	/* FRONT Left Right Center and REAR Left Right Center are exclusive */
	if (speaker_flags.FLC_FRC) {
		cea_channels.channels.RC_RLC_FLC = speaker_flags.FLC_FRC;
		cea_channels.channels.RRC_FRC = speaker_flags.FLC_FRC;
	} else {
		cea_channels.channels.RC_RLC_FLC = speaker_flags.RLC_RRC;
		cea_channels.channels.RRC_FRC = speaker_flags.RLC_RRC;
	}

	return cea_channels;
}

uint32_t calc_max_audio_packets_per_line(
	const struct audio_crtc_info *crtc_info)
{
	uint32_t max_packets_per_line;

	max_packets_per_line =
		crtc_info->h_total - crtc_info->h_active;

	if (crtc_info->pixel_repetition)
		max_packets_per_line *= crtc_info->pixel_repetition;

	/* for other hdmi features */
	max_packets_per_line -= 58;
	/* for Control Period */
	max_packets_per_line -= 16;
	/* Number of Audio Packets per Line */
	max_packets_per_line /= 32;

	return max_packets_per_line;
}

bool get_audio_clock_info(
	enum dc_color_depth color_depth,
	uint32_t crtc_pixel_clock_in_khz,
	uint32_t actual_pixel_clock_in_khz,
	struct audio_clock_info *audio_clock_info)
{
	const struct audio_clock_info *clock_info;
	uint32_t index;
	uint32_t crtc_pixel_clock_in_10khz = crtc_pixel_clock_in_khz / 10;
	uint32_t audio_array_size;

	if (audio_clock_info == NULL)
		return false; /* should not happen */

	switch (color_depth) {
	case COLOR_DEPTH_161616:
		clock_info = audio_clock_info_table_48bpc;
		audio_array_size = ARRAY_SIZE(
				audio_clock_info_table_48bpc);
		break;
	case COLOR_DEPTH_121212:
		clock_info = audio_clock_info_table_36bpc;
		audio_array_size = ARRAY_SIZE(
				audio_clock_info_table_36bpc);
		break;
	default:
		clock_info = audio_clock_info_table;
		audio_array_size = ARRAY_SIZE(
				audio_clock_info_table);
		break;
	}

	if (clock_info != NULL) {
		/* search for exact pixel clock in table */
		for (index = 0; index < audio_array_size; index++) {
			if (clock_info[index].pixel_clock_in_10khz >
				crtc_pixel_clock_in_10khz)
				break;  /* not match */
			else if (clock_info[index].pixel_clock_in_10khz ==
					crtc_pixel_clock_in_10khz) {
				/* match found */
				if (audio_clock_info != NULL) {
					*audio_clock_info = clock_info[index];
					return true;
				}
			}
		}
	}

	/* not found */
	if (actual_pixel_clock_in_khz == 0)
		actual_pixel_clock_in_khz = crtc_pixel_clock_in_khz;

	/* See HDMI spec  the table entry under
	 *  pixel clock of "Other". */
	audio_clock_info->pixel_clock_in_10khz =
			actual_pixel_clock_in_khz / 10;
	audio_clock_info->cts_32khz = actual_pixel_clock_in_khz;
	audio_clock_info->cts_44khz = actual_pixel_clock_in_khz;
	audio_clock_info->cts_48khz = actual_pixel_clock_in_khz;

	audio_clock_info->n_32khz = 4096;
	audio_clock_info->n_44khz = 6272;
	audio_clock_info->n_48khz = 6144;

	return true;
}

static void dce110_se_audio_setup(
	struct stream_encoder *enc,
	unsigned int az_inst,
	struct audio_info *audio_info)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);

	uint32_t speakers = 0;
	uint32_t channels = 0;

	ASSERT(audio_info);
	if (audio_info == NULL)
		/* This should not happen.it does so we don't get BSOD*/
		return;

	speakers = audio_info->flags.info.ALLSPEAKERS;
	channels = speakers_to_channels(audio_info->flags.speaker_flags).all;

	/* setup the audio stream source select (audio -> dig mapping) */
	LINK_REG_SET(AFMT_AUDIO_SRC_CONTROL, AFMT_AUDIO_SRC_SELECT, az_inst);

	/* Channel allocation */
	LINK_REG_UPDATE(AFMT_AUDIO_PACKET_CONTROL2, AFMT_AUDIO_CHANNEL_ENABLE, channels);
}

static void dce110_se_setup_hdmi_audio(
	struct stream_encoder *enc,
	const struct audio_crtc_info *crtc_info)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);

	struct audio_clock_info audio_clock_info = {0};
	uint32_t max_packets_per_line;
	uint32_t addr = 0;
	uint32_t value = 0;

	/* For now still do calculation, although this field is ignored when
	above HDMI_PACKET_GEN_VERSION set to 1 */
	max_packets_per_line = calc_max_audio_packets_per_line(crtc_info);

	/* HDMI_AUDIO_PACKET_CONTROL */
	LINK_REG_UPDATE_N(HDMI_AUDIO_PACKET_CONTROL, 2,
			FD(HDMI_AUDIO_PACKET_CONTROL__HDMI_AUDIO_PACKETS_PER_LINE), max_packets_per_line,
			FD(HDMI_AUDIO_PACKET_CONTROL__HDMI_AUDIO_DELAY_EN), 1);

	/* AFMT_AUDIO_PACKET_CONTROL */
	LINK_REG_UPDATE(AFMT_AUDIO_PACKET_CONTROL, AFMT_60958_CS_UPDATE, 1);

	/* AFMT_AUDIO_PACKET_CONTROL2 */
	LINK_REG_UPDATE_N(AFMT_AUDIO_PACKET_CONTROL2, 2,
			FD(AFMT_AUDIO_PACKET_CONTROL2__AFMT_AUDIO_LAYOUT_OVRD), 0,
			FD(AFMT_AUDIO_PACKET_CONTROL2__AFMT_60958_OSF_OVRD), 0);

	/* HDMI_ACR_PACKET_CONTROL */
	LINK_REG_UPDATE_N(HDMI_ACR_PACKET_CONTROL, 3,
			FD(HDMI_ACR_PACKET_CONTROL__HDMI_ACR_AUTO_SEND), 1,
			FD(HDMI_ACR_PACKET_CONTROL__HDMI_ACR_SOURCE), 0,
			FD(HDMI_ACR_PACKET_CONTROL__HDMI_ACR_AUDIO_PRIORITY), 0);

	/* Program audio clock sample/regeneration parameters */
	if (get_audio_clock_info(
		crtc_info->color_depth,
		crtc_info->requested_pixel_clock,
		crtc_info->calculated_pixel_clock,
		&audio_clock_info)) {

		/* HDMI_ACR_32_0__HDMI_ACR_CTS_32_MASK */
		LINK_REG_UPDATE(HDMI_ACR_32_0, HDMI_ACR_CTS_32, audio_clock_info.cts_32khz);

		/* HDMI_ACR_32_1__HDMI_ACR_N_32_MASK */
		LINK_REG_UPDATE(HDMI_ACR_32_1, HDMI_ACR_N_32, audio_clock_info.n_32khz);

		/* HDMI_ACR_44_0__HDMI_ACR_CTS_44_MASK */
		LINK_REG_UPDATE(HDMI_ACR_44_0, HDMI_ACR_CTS_44, audio_clock_info.cts_44khz);

		/* HDMI_ACR_44_1__HDMI_ACR_N_44_MASK */
		LINK_REG_UPDATE(HDMI_ACR_44_1, HDMI_ACR_N_44, audio_clock_info.n_44khz);

		/* HDMI_ACR_48_0__HDMI_ACR_CTS_48_MASK */
		LINK_REG_UPDATE(HDMI_ACR_48_0, HDMI_ACR_CTS_48, audio_clock_info.cts_48khz);

		/* HDMI_ACR_48_1__HDMI_ACR_N_48_MASK */
		LINK_REG_UPDATE(HDMI_ACR_48_1, HDMI_ACR_N_48, audio_clock_info.n_48khz);

		/* Video driver cannot know in advance which sample rate will
		be used by HD Audio driver
		HDMI_ACR_PACKET_CONTROL__HDMI_ACR_N_MULTIPLE field is
		programmed below in interruppt callback */
	} /* if */

	/* AFMT_60958_0__AFMT_60958_CS_CHANNEL_NUMBER_L_MASK &
	AFMT_60958_0__AFMT_60958_CS_CLOCK_ACCURACY_MASK */
	LINK_REG_UPDATE_N(AFMT_60958_0, 2,
			FD(AFMT_60958_0__AFMT_60958_CS_CHANNEL_NUMBER_L), 1,
			FD(AFMT_60958_0__AFMT_60958_CS_CLOCK_ACCURACY), 0);

	/* AFMT_60958_1 AFMT_60958_CS_CHALNNEL_NUMBER_R */
	LINK_REG_UPDATE(AFMT_60958_1, AFMT_60958_CS_CHANNEL_NUMBER_R, 2);

	/*AFMT_60958_2 now keep this settings until
	 *  Programming guide comes out*/
	LINK_REG_UPDATE_N(AFMT_60958_2, 6,
			FD(AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_2), 3,
			FD(AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_3), 4,
			FD(AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_4), 5,
			FD(AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_5), 6,
			FD(AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_6), 7,
			FD(AFMT_60958_2__AFMT_60958_CS_CHANNEL_NUMBER_7), 8);
}

static void dce110_se_setup_dp_audio(
	struct stream_encoder *enc)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);

	/* --- DP Audio packet configurations --- */
	uint32_t addr = 0;
	uint32_t value = 0;

	/* ATP Configuration */
	LINK_REG_SET(DP_SEC_AUD_N, DP_SEC_AUD_N, DP_SEC_AUD_N__DP_SEC_AUD_N__DEFAULT);

	/* Async/auto-calc timestamp mode */
	LINK_REG_SET(DP_SEC_TIMESTAMP, DP_SEC_TIMESTAMP_MODE,
			DP_SEC_TIMESTAMP__DP_SEC_TIMESTAMP_MODE__AUTO_CALC);

	/* --- The following are the registers
	 *  copied from the SetupHDMI --- */

	/* AFMT_AUDIO_PACKET_CONTROL */
	LINK_REG_UPDATE(AFMT_AUDIO_PACKET_CONTROL, AFMT_60958_CS_UPDATE, 1);

	/* AFMT_AUDIO_PACKET_CONTROL2 */
	/* Program the ATP and AIP next */
	LINK_REG_UPDATE_N(AFMT_AUDIO_PACKET_CONTROL2, 2,
			FD(AFMT_AUDIO_PACKET_CONTROL2__AFMT_AUDIO_LAYOUT_OVRD), 0,
			FD(AFMT_AUDIO_PACKET_CONTROL2__AFMT_60958_OSF_OVRD), 0);

	/* AFMT_INFOFRAME_CONTROL0 */
	LINK_REG_UPDATE(AFMT_INFOFRAME_CONTROL0, AFMT_AUDIO_INFO_UPDATE, 1);

	/* AFMT_60958_0__AFMT_60958_CS_CLOCK_ACCURACY_MASK */
	LINK_REG_UPDATE(AFMT_60958_0, AFMT_60958_CS_CLOCK_ACCURACY, 0);
}

static void dce110_se_enable_audio_clock(
	struct stream_encoder *enc,
	bool enable)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);

	if (LINK_REG(AFMT_CNTL) == 0)
		return;   /* DCE8/10 does not have this register */

	LINK_REG_UPDATE(AFMT_CNTL, AFMT_AUDIO_CLOCK_EN, !!enable);

	/* wait for AFMT clock to turn on,
	 * expectation: this should complete in 1-2 reads
	 */
	LINK_REG_WAIT(AFMT_CNTL, AFMT_AUDIO_CLOCK_ON, !!enable,
			1, 10);
}

static void dce110_se_enable_dp_audio(
	struct stream_encoder *enc)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);

	/* Enable Audio packets */
	LINK_REG_UPDATE(DP_SEC_CNTL, DP_SEC_ASP_ENABLE, 1);

	/* Program the ATP and AIP next */
	LINK_REG_UPDATE_N(DP_SEC_CNTL, 2,
			FD(DP_SEC_CNTL__DP_SEC_ATP_ENABLE), 1,
			FD(DP_SEC_CNTL__DP_SEC_AIP_ENABLE), 1);

	/* Program STREAM_ENABLE after all the other enables. */
	LINK_REG_UPDATE(DP_SEC_CNTL, DP_SEC_STREAM_ENABLE, 1);
}

static void dce110_se_disable_dp_audio(
	struct stream_encoder *enc)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);

	uint32_t value = LINK_REG_READ(DP_SEC_CNTL);

	/* Disable Audio packets */
	set_reg_field_value(value, 0,
		DP_SEC_CNTL, DP_SEC_ASP_ENABLE);

	set_reg_field_value(value, 0,
		DP_SEC_CNTL, DP_SEC_ATP_ENABLE);

	set_reg_field_value(value, 0,
		DP_SEC_CNTL, DP_SEC_AIP_ENABLE);

	set_reg_field_value(value, 0,
		DP_SEC_CNTL, DP_SEC_ACM_ENABLE);

	set_reg_field_value(value, 0,
		DP_SEC_CNTL, DP_SEC_STREAM_ENABLE);

	/* This register shared with encoder info frame. Therefore we need to
	keep master enabled if at least on of the fields is not 0 */
	if (value != 0)
		set_reg_field_value(value, 1,
			DP_SEC_CNTL, DP_SEC_STREAM_ENABLE);

	LINK_REG_WRITE(DP_SEC_CNTL, value);
}

void dce110_se_audio_mute_control(
	struct stream_encoder *enc,
	bool mute)
{
	struct dce110_stream_encoder *enc110 = DCE110STRENC_FROM_STRENC(enc);

	LINK_REG_UPDATE(AFMT_AUDIO_PACKET_CONTROL, AFMT_AUDIO_SAMPLE_SEND, !mute);
}

void dce110_se_dp_audio_setup(
	struct stream_encoder *enc,
	unsigned int az_inst,
	struct audio_info *info)
{
	dce110_se_audio_setup(enc, az_inst, info);
}

void dce110_se_dp_audio_enable(
	struct stream_encoder *enc)
{
	dce110_se_enable_audio_clock(enc, true);
	dce110_se_setup_dp_audio(enc);
	dce110_se_enable_dp_audio(enc);
}

void dce110_se_dp_audio_disable(
	struct stream_encoder *enc)
{
	dce110_se_disable_dp_audio(enc);
	dce110_se_enable_audio_clock(enc, false);
}

void dce110_se_hdmi_audio_setup(
	struct stream_encoder *enc,
	unsigned int az_inst,
	struct audio_info *info,
	struct audio_crtc_info *audio_crtc_info)
{
	dce110_se_enable_audio_clock(enc, true);
	dce110_se_setup_hdmi_audio(enc, audio_crtc_info);
	dce110_se_audio_setup(enc, az_inst, info);
}

void dce110_se_hdmi_audio_disable(
	struct stream_encoder *enc)
{
	dce110_se_enable_audio_clock(enc, false);
}

static const struct stream_encoder_funcs dce110_str_enc_funcs = {
	.dp_set_stream_attribute =
		dce110_stream_encoder_dp_set_stream_attribute,
	.hdmi_set_stream_attribute =
		dce110_stream_encoder_hdmi_set_stream_attribute,
	.dvi_set_stream_attribute =
		dce110_stream_encoder_dvi_set_stream_attribute,
	.set_mst_bandwidth =
		dce110_stream_encoder_set_mst_bandwidth,
	.update_hdmi_info_packets =
		dce110_stream_encoder_update_hdmi_info_packets,
	.stop_hdmi_info_packets =
		dce110_stream_encoder_stop_hdmi_info_packets,
	.update_dp_info_packets =
		dce110_stream_encoder_update_dp_info_packets,
	.stop_dp_info_packets =
		dce110_stream_encoder_stop_dp_info_packets,
	.dp_blank =
		dce110_stream_encoder_dp_blank,
	.dp_unblank =
		dce110_stream_encoder_dp_unblank,

	.audio_mute_control = dce110_se_audio_mute_control,

	.dp_audio_setup = dce110_se_dp_audio_setup,
	.dp_audio_enable = dce110_se_dp_audio_enable,
	.dp_audio_disable = dce110_se_dp_audio_disable,

	.hdmi_audio_setup = dce110_se_hdmi_audio_setup,
	.hdmi_audio_disable = dce110_se_hdmi_audio_disable,
};

bool dce110_stream_encoder_construct(
	struct dce110_stream_encoder *enc110,
	struct dc_context *ctx,
	struct dc_bios *bp,
	enum engine_id eng_id,
	const struct dce110_stream_enc_registers *regs)
{
	if (!enc110)
		return false;
	if (!bp)
		return false;

	enc110->base.funcs = &dce110_str_enc_funcs;
	enc110->base.ctx = ctx;
	enc110->base.id = eng_id;
	enc110->base.bp = bp;
	enc110->regs = regs;

	return true;
}
