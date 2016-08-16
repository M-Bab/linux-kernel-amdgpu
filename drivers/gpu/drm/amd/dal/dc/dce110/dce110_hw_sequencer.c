/*
 * Copyright 2015 Advanced Micro Devices, Inc.
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
#include "dc.h"
#include "dc_bios_types.h"
#include "core_types.h"
#include "core_status.h"
#include "resource.h"
#include "hw_sequencer.h"
#include "dm_helpers.h"
#include "dce110_hw_sequencer.h"
#include "dce110_timing_generator.h"

#include "gpu/dce110/dc_clock_gating_dce110.h"

#include "bios/dce110/bios_dce110.h"
#include "timing_generator.h"
#include "mem_input.h"
#include "opp.h"
#include "ipp.h"
#include "transform.h"
#include "stream_encoder.h"
#include "link_encoder.h"
#include "clock_source.h"
#include "gamma_calcs.h"

/* include DCE11 register header files */
#include "dce/dce_11_0_d.h"
#include "dce/dce_11_0_sh_mask.h"

struct dce110_hw_seq_reg_offsets {
	uint32_t dcfe;
	uint32_t blnd;
	uint32_t crtc;
};

enum blender_mode {
	BLENDER_MODE_CURRENT_PIPE = 0,/* Data from current pipe only */
	BLENDER_MODE_OTHER_PIPE, /* Data from other pipe only */
	BLENDER_MODE_BLENDING,/* Alpha blending - blend 'current' and 'other' */
	BLENDER_MODE_STEREO
};

static const struct dce110_hw_seq_reg_offsets reg_offsets[] = {
{
	.dcfe = (mmDCFE0_DCFE_CLOCK_CONTROL - mmDCFE_CLOCK_CONTROL),
	.blnd = (mmBLND0_BLND_CONTROL - mmBLND_CONTROL),
	.crtc = (mmCRTC0_CRTC_GSL_CONTROL - mmCRTC_GSL_CONTROL),
},
{
	.dcfe = (mmDCFE1_DCFE_CLOCK_CONTROL - mmDCFE_CLOCK_CONTROL),
	.blnd = (mmBLND1_BLND_CONTROL - mmBLND_CONTROL),
	.crtc = (mmCRTC1_CRTC_GSL_CONTROL - mmCRTC_GSL_CONTROL),
},
{
	.dcfe = (mmDCFE2_DCFE_CLOCK_CONTROL - mmDCFE_CLOCK_CONTROL),
	.blnd = (mmBLND2_BLND_CONTROL - mmBLND_CONTROL),
	.crtc = (mmCRTC2_CRTC_GSL_CONTROL - mmCRTC_GSL_CONTROL),
},
{
	.dcfe = (mmDCFEV_CLOCK_CONTROL - mmDCFE_CLOCK_CONTROL),
	.blnd = (mmBLNDV_CONTROL - mmBLND_CONTROL),
	.crtc = (mmCRTCV_GSL_CONTROL - mmCRTC_GSL_CONTROL),
}
};

#define HW_REG_DCFE(reg, id)\
	(reg + reg_offsets[id].dcfe)

#define HW_REG_BLND(reg, id)\
	(reg + reg_offsets[id].blnd)

#define HW_REG_CRTC(reg, id)\
	(reg + reg_offsets[id].crtc)

#define MAX_WATERMARK 0xFFFF
#define SAFE_NBP_MARK 0x7FFF

/*******************************************************************************
 * Private definitions
 ******************************************************************************/
/***************************PIPE_CONTROL***********************************/
static void dce110_enable_fe_clock(
	struct dc_context *ctx, uint8_t controller_id, bool enable)
{
	uint32_t value = 0;
	uint32_t addr;

	addr = HW_REG_DCFE(mmDCFE_CLOCK_CONTROL, controller_id);

	value = dm_read_reg(ctx, addr);

	set_reg_field_value(
		value,
		enable,
		DCFE_CLOCK_CONTROL,
		DCFE_CLOCK_ENABLE);

	dm_write_reg(ctx, addr, value);
}

static void dce110_init_pte(struct dc_context *ctx)
{
	uint32_t addr;
	uint32_t value = 0;
	uint32_t chunk_int = 0;
	uint32_t chunk_mul = 0;

	addr = mmUNP_DVMM_PTE_CONTROL;
	value = dm_read_reg(ctx, addr);

	set_reg_field_value(
		value,
		0,
		DVMM_PTE_CONTROL,
		DVMM_USE_SINGLE_PTE);

	set_reg_field_value(
		value,
		1,
		DVMM_PTE_CONTROL,
		DVMM_PTE_BUFFER_MODE0);

	set_reg_field_value(
		value,
		1,
		DVMM_PTE_CONTROL,
		DVMM_PTE_BUFFER_MODE1);

	dm_write_reg(ctx, addr, value);

	addr = mmDVMM_PTE_REQ;
	value = dm_read_reg(ctx, addr);

	chunk_int = get_reg_field_value(
		value,
		DVMM_PTE_REQ,
		HFLIP_PTEREQ_PER_CHUNK_INT);

	chunk_mul = get_reg_field_value(
		value,
		DVMM_PTE_REQ,
		HFLIP_PTEREQ_PER_CHUNK_MULTIPLIER);

	if (chunk_int != 0x4 || chunk_mul != 0x4) {

		set_reg_field_value(
			value,
			255,
			DVMM_PTE_REQ,
			MAX_PTEREQ_TO_ISSUE);

		set_reg_field_value(
			value,
			4,
			DVMM_PTE_REQ,
			HFLIP_PTEREQ_PER_CHUNK_INT);

		set_reg_field_value(
			value,
			4,
			DVMM_PTE_REQ,
			HFLIP_PTEREQ_PER_CHUNK_MULTIPLIER);

		dm_write_reg(ctx, addr, value);
	}
}

/* this is a workaround for hw bug - it is a trigger on r/w */
static void trigger_write_crtc_h_blank_start_end(
	struct dc_context *ctx,
	uint8_t controller_id)
{
	uint32_t value;
	uint32_t addr;

	addr =  HW_REG_CRTC(mmCRTC_H_BLANK_START_END, controller_id);
	value = dm_read_reg(ctx, addr);
	dm_write_reg(ctx, addr, value);
}

static bool dce110_pipe_control_lock(
	struct dc_context *ctx,
	uint8_t controller_idx,
	uint32_t control_mask,
	bool lock)
{
	uint32_t addr = HW_REG_BLND(mmBLND_V_UPDATE_LOCK, controller_idx);
	uint32_t value = dm_read_reg(ctx, addr);

	if (control_mask & PIPE_LOCK_CONTROL_GRAPHICS)
		set_reg_field_value(
			value,
			lock,
			BLND_V_UPDATE_LOCK,
			BLND_DCP_GRPH_V_UPDATE_LOCK);

	if (control_mask & PIPE_LOCK_CONTROL_SCL)
		set_reg_field_value(
			value,
			lock,
			BLND_V_UPDATE_LOCK,
			BLND_SCL_V_UPDATE_LOCK);

	if (control_mask & PIPE_LOCK_CONTROL_SURFACE)
		set_reg_field_value(
			value,
			lock,
			BLND_V_UPDATE_LOCK,
			BLND_DCP_GRPH_SURF_V_UPDATE_LOCK);

	if (control_mask & PIPE_LOCK_CONTROL_BLENDER) {
		set_reg_field_value(
			value,
			lock,
			BLND_V_UPDATE_LOCK,
			BLND_BLND_V_UPDATE_LOCK);
	}

	if (control_mask & PIPE_LOCK_CONTROL_MODE)
		set_reg_field_value(
			value,
			lock,
			BLND_V_UPDATE_LOCK,
			BLND_V_UPDATE_LOCK_MODE);

	dm_write_reg(ctx, addr, value);

	if (!lock && (control_mask & PIPE_LOCK_CONTROL_BLENDER))
		trigger_write_crtc_h_blank_start_end(ctx, controller_idx);

	return true;
}

static void dce110_set_blender_mode(
	struct dc_context *ctx,
	uint8_t controller_id,
	uint32_t mode)
{
	uint32_t value;
	uint32_t addr = HW_REG_BLND(mmBLND_CONTROL, controller_id);
	uint32_t alpha_mode = 2;
	uint32_t blnd_mode = 0;
	uint32_t feedthrough = 1;
	uint32_t multiplied_mode = 0;

	switch (mode) {
	case BLENDER_MODE_OTHER_PIPE:
		feedthrough = 0;
		alpha_mode = 0;
		blnd_mode = 1;
		break;
	case BLENDER_MODE_BLENDING:
		feedthrough = 0;
		alpha_mode = 0;
		blnd_mode = 2;
		multiplied_mode = 1;
		break;
	case BLENDER_MODE_CURRENT_PIPE:
	default:
		if (controller_id == DCE110_UNDERLAY_IDX || controller_id == 0)
			feedthrough = 0;
		break;
	}

	value = dm_read_reg(ctx, addr);

	set_reg_field_value(
		value,
		feedthrough,
		BLND_CONTROL,
		BLND_FEEDTHROUGH_EN);
	set_reg_field_value(
		value,
		alpha_mode,
		BLND_CONTROL,
		BLND_ALPHA_MODE);
	set_reg_field_value(
		value,
		blnd_mode,
		BLND_CONTROL,
		BLND_MODE);
	set_reg_field_value(
		value,
		multiplied_mode,
		BLND_CONTROL,
		BLND_MULTIPLIED_MODE);

	dm_write_reg(ctx, addr, value);
}

static void dce110_crtc_switch_to_clk_src(
				struct clock_source *clk_src, uint8_t crtc_inst)
{
	uint32_t pixel_rate_cntl_value;
	uint32_t addr;

	/* These addresses are the same across DCE8 - DCE11.2 */
	addr = mmCRTC0_PIXEL_RATE_CNTL + crtc_inst *
			(mmCRTC1_PIXEL_RATE_CNTL - mmCRTC0_PIXEL_RATE_CNTL);

	pixel_rate_cntl_value = dm_read_reg(clk_src->ctx, addr);

	if (clk_src->id == CLOCK_SOURCE_ID_EXTERNAL)
		set_reg_field_value(pixel_rate_cntl_value, 1,
			CRTC0_PIXEL_RATE_CNTL, DP_DTO0_ENABLE);
	else {
		set_reg_field_value(pixel_rate_cntl_value,
				0,
				CRTC0_PIXEL_RATE_CNTL,
				DP_DTO0_ENABLE);

		set_reg_field_value(pixel_rate_cntl_value,
				clk_src->id - CLOCK_SOURCE_ID_PLL0,
				CRTC0_PIXEL_RATE_CNTL,
				CRTC0_PIXEL_RATE_SOURCE);
	}
	dm_write_reg(clk_src->ctx, addr, pixel_rate_cntl_value);
}
/**************************************************************************/

static void enable_display_pipe_clock_gating(
	struct dc_context *ctx,
	bool clock_gating)
{
	/*TODO*/
}

static bool dce110_enable_display_power_gating(
	struct dc_context *ctx,
	uint8_t controller_id,
	struct dc_bios *dcb,
	enum pipe_gating_control power_gating)
{
	enum bp_result bp_result = BP_RESULT_OK;
	enum bp_pipe_control_action cntl;

	if (IS_FPGA_MAXIMUS_DC(ctx->dce_environment))
		return true;

	if (power_gating == PIPE_GATING_CONTROL_INIT)
		cntl = ASIC_PIPE_INIT;
	else if (power_gating == PIPE_GATING_CONTROL_ENABLE)
		cntl = ASIC_PIPE_ENABLE;
	else
		cntl = ASIC_PIPE_DISABLE;

	if (controller_id == DCE110_UNDERLAY_IDX)
		controller_id = CONTROLLER_ID_UNDERLAY0 - 1;

	if (power_gating != PIPE_GATING_CONTROL_INIT || controller_id == 0){

		bp_result = dcb->funcs->enable_disp_power_gating(
						dcb, controller_id + 1, cntl);

		/* Revert MASTER_UPDATE_MODE to 0 because bios sets it 2
		 * by default when command table is called
		 *
		 * Bios parser accepts controller_id = 6 as indicative of
		 * underlay pipe in dce110. But we do not support more
		 * than 3.
		 */
		if (controller_id < CONTROLLER_ID_MAX - 1)
			dm_write_reg(ctx,
				HW_REG_CRTC(mmCRTC_MASTER_UPDATE_MODE, controller_id),
				0);
	}

	if (power_gating != PIPE_GATING_CONTROL_ENABLE)
		dce110_init_pte(ctx);

	if (bp_result == BP_RESULT_OK)
		return true;
	else
		return false;
}

static void build_prescale_params(struct ipp_prescale_params *prescale_params,
		const struct core_surface *surface)
{
	prescale_params->mode = IPP_PRESCALE_MODE_FIXED_UNSIGNED;

	switch (surface->public.format) {
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB8888:
	case SURFACE_PIXEL_FORMAT_GRPH_BGRA8888:
		prescale_params->scale = 0x2020;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB2101010:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010:
		prescale_params->scale = 0x2008;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR16161616F:
		prescale_params->scale = 0x2000;
		break;
	default:
		ASSERT(false);
	}
}

static bool set_gamma_ramp(
	struct input_pixel_processor *ipp,
	struct output_pixel_processor *opp,
	const struct core_gamma *ramp,
	const struct core_surface *surface)
{
	struct ipp_prescale_params *prescale_params;
	struct pwl_params *regamma_params;
	bool result = false;

	prescale_params = dm_alloc(sizeof(struct ipp_prescale_params));

	if (prescale_params == NULL)
		goto prescale_alloc_fail;

	regamma_params = dm_alloc(sizeof(struct pwl_params));
	if (regamma_params == NULL)
		goto regamma_alloc_fail;

	regamma_params->hw_points_num = GAMMA_HW_POINTS_NUM;

	opp->funcs->opp_power_on_regamma_lut(opp, true);

	if (ipp) {
		build_prescale_params(prescale_params, surface);
		ipp->funcs->ipp_program_prescale(ipp, prescale_params);
	}

	if (ramp && calculate_regamma_params(regamma_params, ramp, surface)) {

		opp->funcs->opp_program_regamma_pwl(opp, regamma_params);
		if (ipp)
			ipp->funcs->ipp_set_degamma(ipp, IPP_DEGAMMA_MODE_HW_sRGB);
		opp->funcs->opp_set_regamma_mode(opp, OPP_REGAMMA_USER);
	} else {
		if (ipp)
			ipp->funcs->ipp_set_degamma(ipp, IPP_DEGAMMA_MODE_BYPASS);
		opp->funcs->opp_set_regamma_mode(opp, OPP_REGAMMA_BYPASS);
	}

	opp->funcs->opp_power_on_regamma_lut(opp, false);

	result = true;

	dm_free(regamma_params);

regamma_alloc_fail:
	dm_free(prescale_params);
prescale_alloc_fail:
	return result;
}

static enum dc_status bios_parser_crtc_source_select(
		struct pipe_ctx *pipe_ctx)
{
	struct dc_bios *dcb;
	/* call VBIOS table to set CRTC source for the HW
	 * encoder block
	 * note: video bios clears all FMT setting here. */
	struct bp_crtc_source_select crtc_source_select = {0};
	const struct core_sink *sink = pipe_ctx->stream->sink;

	crtc_source_select.engine_id = pipe_ctx->stream_enc->id;
	crtc_source_select.controller_id = pipe_ctx->pipe_idx + 1;
	/*TODO: Need to un-hardcode color depth, dp_audio and account for
	 * the case where signal and sink signal is different (translator
	 * encoder)*/
	crtc_source_select.signal = pipe_ctx->stream->signal;
	crtc_source_select.enable_dp_audio = false;
	crtc_source_select.sink_signal = pipe_ctx->stream->signal;
	crtc_source_select.display_output_bit_depth = PANEL_8BIT_COLOR;

	dcb = dal_adapter_service_get_bios_parser(sink->link->adapter_srv);

	if (BP_RESULT_OK != dcb->funcs->crtc_source_select(
		dcb,
		&crtc_source_select)) {
		return DC_ERROR_UNEXPECTED;
	}

	return DC_OK;
}


static void update_bios_scratch_critical_state(struct adapter_service *as,
		bool state)
{
	struct dc_bios *dcb = dal_adapter_service_get_bios_parser(as);

	dcb->funcs->set_scratch_critical_state(dcb, state);
}

static void update_info_frame(struct pipe_ctx *pipe_ctx)
{
	if (dc_is_hdmi_signal(pipe_ctx->stream->signal))
		pipe_ctx->stream_enc->funcs->update_hdmi_info_packets(
			pipe_ctx->stream_enc,
			&pipe_ctx->encoder_info_frame);
	else if (dc_is_dp_signal(pipe_ctx->stream->signal))
		pipe_ctx->stream_enc->funcs->update_dp_info_packets(
			pipe_ctx->stream_enc,
			&pipe_ctx->encoder_info_frame);
}

static void enable_stream(struct pipe_ctx *pipe_ctx)
{
	enum dc_lane_count lane_count =
		pipe_ctx->stream->sink->link->public.cur_link_settings.lane_count;

	struct dc_crtc_timing *timing = &pipe_ctx->stream->public.timing;
	struct core_link *link = pipe_ctx->stream->sink->link;

	/* 1. update AVI info frame (HDMI, DP)
	 * we always need to update info frame
	*/
	uint32_t active_total_with_borders;
	uint32_t early_control = 0;
	struct timing_generator *tg = pipe_ctx->tg;

	update_info_frame(pipe_ctx);
	/* enable early control to avoid corruption on DP monitor*/
	active_total_with_borders =
			timing->h_addressable
				+ timing->h_border_left
				+ timing->h_border_right;

	if (lane_count != 0)
		early_control = active_total_with_borders % lane_count;

	if (early_control == 0)
		early_control = lane_count;

	tg->funcs->set_early_control(tg, early_control);

	/* enable audio only within mode set */
	if (pipe_ctx->audio != NULL) {
		dal_audio_enable_output(
			pipe_ctx->audio,
			pipe_ctx->stream_enc->id,
			pipe_ctx->stream->signal);
	}

	/* For MST, there are multiply stream go to only one link.
	 * connect DIG back_end to front_end while enable_stream and
	 * disconnect them during disable_stream
	 * BY this, it is logic clean to separate stream and link */
	 link->link_enc->funcs->connect_dig_be_to_fe(link->link_enc,
			pipe_ctx->stream_enc->id, true);

}

static void disable_stream(struct pipe_ctx *pipe_ctx)
{
	struct core_stream *stream = pipe_ctx->stream;
	struct core_link *link = stream->sink->link;

	if (dc_is_hdmi_signal(pipe_ctx->stream->signal))
		pipe_ctx->stream_enc->funcs->stop_hdmi_info_packets(
			pipe_ctx->stream_enc);

	if (dc_is_dp_signal(pipe_ctx->stream->signal))
		pipe_ctx->stream_enc->funcs->stop_dp_info_packets(
			pipe_ctx->stream_enc);

	if (pipe_ctx->audio) {
		/* mute audio */
		dal_audio_mute(pipe_ctx->audio, pipe_ctx->stream_enc->id,
				pipe_ctx->stream->signal);

		/* TODO: notify audio driver for if audio modes list changed
		 * add audio mode list change flag */
		/* dal_audio_disable_azalia_audio_jack_presence(stream->audio,
		 * stream->stream_engine_id);
		 */
	}

	/* blank at encoder level */
	if (dc_is_dp_signal(pipe_ctx->stream->signal))
		pipe_ctx->stream_enc->funcs->dp_blank(pipe_ctx->stream_enc);

	link->link_enc->funcs->connect_dig_be_to_fe(
			link->link_enc,
			pipe_ctx->stream_enc->id,
			false);

}

static void unblank_stream(struct pipe_ctx *pipe_ctx,
		struct dc_link_settings *link_settings)
{
	struct encoder_unblank_param params = { { 0 } };

	/* only 3 items below are used by unblank */
	params.crtc_timing.pixel_clock =
		pipe_ctx->stream->public.timing.pix_clk_khz;
	params.link_settings.link_rate = link_settings->link_rate;
	pipe_ctx->stream_enc->funcs->dp_unblank(pipe_ctx->stream_enc, &params);
}

static enum audio_dto_source translate_to_dto_source(enum controller_id crtc_id)
{
	switch (crtc_id) {
	case CONTROLLER_ID_D0:
		return DTO_SOURCE_ID0;
	case CONTROLLER_ID_D1:
		return DTO_SOURCE_ID1;
	case CONTROLLER_ID_D2:
		return DTO_SOURCE_ID2;
	case CONTROLLER_ID_D3:
		return DTO_SOURCE_ID3;
	case CONTROLLER_ID_D4:
		return DTO_SOURCE_ID4;
	case CONTROLLER_ID_D5:
		return DTO_SOURCE_ID5;
	default:
		return DTO_SOURCE_UNKNOWN;
	}
}

static void build_audio_output(
	const struct pipe_ctx *pipe_ctx,
	struct audio_output *audio_output)
{
	const struct core_stream *stream = pipe_ctx->stream;
	audio_output->engine_id = pipe_ctx->stream_enc->id;

	audio_output->signal = pipe_ctx->stream->signal;

	/* audio_crtc_info  */

	audio_output->crtc_info.h_total =
		stream->public.timing.h_total;

	/*
	 * Audio packets are sent during actual CRTC blank physical signal, we
	 * need to specify actual active signal portion
	 */
	audio_output->crtc_info.h_active =
			stream->public.timing.h_addressable
			+ stream->public.timing.h_border_left
			+ stream->public.timing.h_border_right;

	audio_output->crtc_info.v_active =
			stream->public.timing.v_addressable
			+ stream->public.timing.v_border_top
			+ stream->public.timing.v_border_bottom;

	audio_output->crtc_info.pixel_repetition = 1;

	audio_output->crtc_info.interlaced =
			stream->public.timing.flags.INTERLACE;

	audio_output->crtc_info.refresh_rate =
		(stream->public.timing.pix_clk_khz*1000)/
		(stream->public.timing.h_total*stream->public.timing.v_total);

	audio_output->crtc_info.color_depth =
		stream->public.timing.display_color_depth;

	audio_output->crtc_info.requested_pixel_clock =
			pipe_ctx->pix_clk_params.requested_pix_clk;

	/*
	 * TODO - Investigate why calculated pixel clk has to be
	 * requested pixel clk
	 */
	audio_output->crtc_info.calculated_pixel_clock =
			pipe_ctx->pix_clk_params.requested_pix_clk;

	if (pipe_ctx->stream->signal == SIGNAL_TYPE_DISPLAY_PORT ||
			pipe_ctx->stream->signal == SIGNAL_TYPE_DISPLAY_PORT_MST) {
		audio_output->pll_info.dp_dto_source_clock_in_khz =
			dal_display_clock_get_dp_ref_clk_frequency(
				pipe_ctx->dis_clk);
	}

	audio_output->pll_info.feed_back_divider =
			pipe_ctx->pll_settings.feedback_divider;

	audio_output->pll_info.dto_source =
		translate_to_dto_source(
			pipe_ctx->pipe_idx + 1);

	/* TODO hard code to enable for now. Need get from stream */
	audio_output->pll_info.ss_enabled = true;

	audio_output->pll_info.ss_percentage =
			pipe_ctx->pll_settings.ss_percentage;
}

static void get_surface_visual_confirm_color(const struct pipe_ctx *pipe_ctx,
		struct tg_color *color)
{
	uint32_t color_value = MAX_TG_COLOR_VALUE * (4 - pipe_ctx->pipe_idx) / 4;

	switch (pipe_ctx->scl_data.format) {
	case PIXEL_FORMAT_ARGB8888:
		/* set boarder color to red */
		color->color_r_cr = color_value;
		break;

	case PIXEL_FORMAT_ARGB2101010:
		/* set boarder color to blue */
		color->color_b_cb = color_value;
		break;
	case PIXEL_FORMAT_420BPP12:
		/* set boarder color to green */
		color->color_g_y = color_value;
		break;
	case PIXEL_FORMAT_FP16:
		/* set boarder color to white */
		color->color_r_cr = color_value;
		color->color_b_cb = color_value;
		color->color_g_y = color_value;
		break;
	default:
		break;
	}
}

static void program_scaler(const struct core_dc *dc,
		const struct pipe_ctx *pipe_ctx)
{
	struct tg_color color = {0};

	if (dc->public.debug.surface_visual_confirm)
		get_surface_visual_confirm_color(pipe_ctx, &color);
	else
		color_space_to_black_color(pipe_ctx->stream->public.output_color_space,
									&color);

	pipe_ctx->xfm->funcs->transform_set_pixel_storage_depth(
		pipe_ctx->xfm,
		pipe_ctx->scl_data.lb_bpp,
		&pipe_ctx->stream->bit_depth_params);

	pipe_ctx->tg->funcs->set_overscan_blank_color(
		pipe_ctx->tg,
		&color);

	pipe_ctx->xfm->funcs->transform_set_scaler(pipe_ctx->xfm,
		&pipe_ctx->scl_data);
}

static enum dc_status apply_single_controller_ctx_to_hw(
		struct pipe_ctx *pipe_ctx,
		struct validate_context *context,
		struct core_dc *dc)
{
	struct core_stream *stream = pipe_ctx->stream;
	struct pipe_ctx *pipe_ctx_old = &dc->current_context->res_ctx.
			pipe_ctx[pipe_ctx->pipe_idx];
	struct tg_color black_color = {0};

	if (!pipe_ctx_old->stream) {
		/*
		 * Must blank CRTC after disabling power gating and before any
		 * programming, otherwise CRTC will be hung in bad state
		 */
		pipe_ctx->tg->funcs->set_blank(pipe_ctx->tg, true);

		if (false == pipe_ctx->clock_source->funcs->program_pix_clk(
				pipe_ctx->clock_source,
				&pipe_ctx->pix_clk_params,
				&pipe_ctx->pll_settings)) {
			BREAK_TO_DEBUGGER();
			return DC_ERROR_UNEXPECTED;
		}

		pipe_ctx->tg->funcs->program_timing(
				pipe_ctx->tg,
				&stream->public.timing,
				true);
	}

	/*TODO: mst support - use total stream count*/
	pipe_ctx->mi->funcs->allocate_mem_input(
					pipe_ctx->mi,
					stream->public.timing.h_total,
					stream->public.timing.v_total,
					stream->public.timing.pix_clk_khz,
					context->target_count);

	if (!pipe_ctx_old->stream) {
		if (false == pipe_ctx->tg->funcs->enable_crtc(
				pipe_ctx->tg)) {
			BREAK_TO_DEBUGGER();
			return DC_ERROR_UNEXPECTED;
		}
	}

	/* TODO: move to stream encoder */
	if (pipe_ctx->stream->signal != SIGNAL_TYPE_VIRTUAL)
		if (DC_OK != bios_parser_crtc_source_select(pipe_ctx)) {
			BREAK_TO_DEBUGGER();
			return DC_ERROR_UNEXPECTED;
		}

	pipe_ctx->opp->funcs->opp_set_dyn_expansion(
			pipe_ctx->opp,
			COLOR_SPACE_YCBCR601,
			stream->public.timing.display_color_depth,
			pipe_ctx->stream->signal);

	pipe_ctx->opp->funcs->opp_program_fmt(
			pipe_ctx->opp,
			&stream->bit_depth_params,
			&stream->clamping);

	if (pipe_ctx->stream->signal != SIGNAL_TYPE_VIRTUAL)
		stream->sink->link->link_enc->funcs->setup(
			stream->sink->link->link_enc,
			pipe_ctx->stream->signal);

	if (dc_is_dp_signal(pipe_ctx->stream->signal))
		pipe_ctx->stream_enc->funcs->dp_set_stream_attribute(
			pipe_ctx->stream_enc,
			&stream->public.timing);

	if (dc_is_hdmi_signal(pipe_ctx->stream->signal))
		pipe_ctx->stream_enc->funcs->hdmi_set_stream_attribute(
			pipe_ctx->stream_enc,
			&stream->public.timing,
			stream->phy_pix_clk,
			pipe_ctx->audio != NULL);

	if (dc_is_dvi_signal(pipe_ctx->stream->signal))
		pipe_ctx->stream_enc->funcs->dvi_set_stream_attribute(
			pipe_ctx->stream_enc,
			&stream->public.timing,
			(pipe_ctx->stream->signal == SIGNAL_TYPE_DVI_DUAL_LINK) ?
			true : false);

	/* program blank color */
	color_space_to_black_color(stream->public.output_color_space, &black_color);
	pipe_ctx->tg->funcs->set_blank_color(
			pipe_ctx->tg,
			&black_color);

	if (!pipe_ctx_old->stream) {
		core_link_enable_stream(pipe_ctx);

		if (dc_is_dp_signal(pipe_ctx->stream->signal))
			unblank_stream(pipe_ctx,
				&stream->sink->link->public.cur_link_settings);
	}

	if (!pipe_ctx_old || memcmp(&pipe_ctx_old->scl_data,
				&pipe_ctx->scl_data,
				sizeof(struct scaler_data)) != 0)
		program_scaler(dc, pipe_ctx);

	return DC_OK;
}

/******************************************************************************/

static void power_down_encoders(struct core_dc *dc)
{
	int i;

	for (i = 0; i < dc->link_count; i++) {
		dc->links[i]->link_enc->funcs->disable_output(
				dc->links[i]->link_enc, SIGNAL_TYPE_NONE);
	}
}

static void power_down_controllers(struct core_dc *dc)
{
	int i;

	for (i = 0; i < dc->res_pool->pipe_count; i++) {
		dc->res_pool->timing_generators[i]->funcs->disable_crtc(
				dc->res_pool->timing_generators[i]);
	}
}

static void power_down_clock_sources(struct core_dc *dc)
{
	int i;

	if (dc->res_pool->dp_clock_source->funcs->cs_power_down(
		dc->res_pool->dp_clock_source) == false)
		dm_error("Failed to power down pll! (dp clk src)\n");

	for (i = 0; i < dc->res_pool->clk_src_count; i++) {
		if (dc->res_pool->clock_sources[i]->funcs->cs_power_down(
				dc->res_pool->clock_sources[i]) == false)
			dm_error("Failed to power down pll! (clk src index=%d)\n", i);
	}
}

static void power_down_all_hw_blocks(struct core_dc *dc)
{
	power_down_encoders(dc);

	power_down_controllers(dc);

	power_down_clock_sources(dc);
}

static void disable_vga_and_power_gate_all_controllers(
		struct core_dc *dc)
{
	int i;
	struct timing_generator *tg;
	struct dc_bios *dcb;
	struct dc_context *ctx;

	dcb = dal_adapter_service_get_bios_parser(
			dc->res_pool->adapter_srv);

	for (i = 0; i < dc->res_pool->pipe_count; i++) {
		tg = dc->res_pool->timing_generators[i];
		ctx = dc->ctx;

		tg->funcs->disable_vga(tg);

		/* Enable CLOCK gating for each pipe BEFORE controller
		 * powergating. */
		enable_display_pipe_clock_gating(ctx,
				true);
		dc->hwss.enable_display_power_gating(ctx, i, dcb,
				PIPE_GATING_CONTROL_ENABLE);
		dc->res_pool->transforms[i]->funcs->transform_reset(
				dc->res_pool->transforms[i]);
	}
}

/**
 * When ASIC goes from VBIOS/VGA mode to driver/accelerated mode we need:
 *  1. Power down all DC HW blocks
 *  2. Disable VGA engine on all controllers
 *  3. Enable power gating for controller
 *  4. Set acc_mode_change bit (VBIOS will clear this bit when going to FSDOS)
 */
static void enable_accelerated_mode(struct core_dc *dc)
{
	struct dc_bios *dcb;

	dcb = dal_adapter_service_get_bios_parser(dc->res_pool->adapter_srv);

	power_down_all_hw_blocks(dc);

	disable_vga_and_power_gate_all_controllers(dc);
	dce110_set_scratch_acc_mode_change(dc->ctx);
}

#if 0
static enum clocks_state get_required_clocks_state(
	struct display_clock *display_clock,
	struct state_dependent_clocks *req_state_dep_clks)
{
	enum clocks_state clocks_required_state;
	enum clocks_state dp_link_required_state;
	enum clocks_state overall_required_state;

	clocks_required_state = dal_display_clock_get_required_clocks_state(
			display_clock, req_state_dep_clks);

	dp_link_required_state = CLOCKS_STATE_ULTRA_LOW;

	/* overall required state is the max of required state for clocks
	 * (pixel, display clock) and the required state for DP link. */
	overall_required_state =
		clocks_required_state > dp_link_required_state ?
			clocks_required_state : dp_link_required_state;

	/* return the min required state */
	return overall_required_state;
}

static bool dc_pre_clock_change(
		struct dc_context *ctx,
		struct minimum_clocks_calculation_result *min_clk_in,
		enum clocks_state required_clocks_state,
		struct power_to_dal_info *output)
{
	struct dal_to_power_info input = {0};

	input.min_deep_sleep_sclk = min_clk_in->min_deep_sleep_sclk;
	input.min_mclk = min_clk_in->min_mclk_khz;
	input.min_sclk = min_clk_in->min_sclk_khz;

	switch (required_clocks_state) {
	case CLOCKS_STATE_ULTRA_LOW:
		input.required_clock = PP_CLOCKS_STATE_ULTRA_LOW;
		break;
	case CLOCKS_STATE_LOW:
		input.required_clock = PP_CLOCKS_STATE_LOW;
		break;
	case CLOCKS_STATE_NOMINAL:
		input.required_clock = PP_CLOCKS_STATE_NOMINAL;
		break;
	case CLOCKS_STATE_PERFORMANCE:
		input.required_clock = PP_CLOCKS_STATE_PERFORMANCE;
		break;
	default:
		input.required_clock = PP_CLOCKS_STATE_NOMINAL;
		break;
	}

	if (!dc_service_pp_pre_dce_clock_change(ctx, &input, output)) {
		dm_error("DC: dc_service_pp_pre_dce_clock_change failed!\n");
		return false;
	}

	return true;
}

static bool dc_set_clocks_and_clock_state (
		struct validate_context *context)
{
	struct power_to_dal_info output = {0};

	struct display_clock *disp_clk = context->res_ctx.pool->display_clock;
	struct dc_context *ctx = context->targets[0]->ctx;

	if (!dc_pre_clock_change(
			ctx,
			&context->res_ctx.min_clocks,
			get_required_clocks_state(
					context->res_ctx.pool->display_clock,
					&context->res_ctx.state_clocks),
			&output)) {
		/* "output" was not updated by PPLib.
		 * DAL will use default values for set mode.
		 *
		 * Do NOT fail this call. */
		return true;
	}

	/* PPLib accepted the "clock state" that we need, that means we
	 * can store it as minimum state because PPLib guarantees not go below
	 * that state.
	 *
	 * Update the clock state here (prior to setting Pixel clock,
	 * or Display clock)
	 **/
	if (!dal_display_clock_set_min_clocks_state(
			disp_clk, context->res_ctx.required_clocks_state)) {
		BREAK_TO_DEBUGGER();
		dm_error("DC: failed to set minimum clock state!\n");
	}

	/*bm_clk_info.max_mclk_khz = output.max_mclk;
	bm_clk_info.min_mclk_khz = output.min_mclk;
	bm_clk_info.max_sclk_khz = output.max_sclk;
	bm_clk_info.min_sclk_khz = output.min_sclk;*/

	/* Now let Bandwidth Manager know about values we got from PPLib. */
	/*dal_bandwidth_manager_set_dynamic_clock_info(bw_mgr, &bm_clk_info);*/

	return true;
}
#endif

/**
 * Call display_engine_clock_dce80 to perform the Dclk programming.
 */
static void set_display_clock(struct validate_context *context)
{
	/* Program the display engine clock.
	 * Check DFS bypass mode support or not. DFSbypass feature is only when
	 * BIOS GPU info table reports support. */

	if (/*dal_adapter_service_is_dfs_bypass_enabled()*/ false) {
		/*TODO: set_display_clock_dfs_bypass(
				hws,
				path_set,
				context->res_ctx.pool->display_clock,
				context->res_ctx.min_clocks.min_dclk_khz);*/
	} else {
		/*
		 * TODO: need to either port work around from DAL2 function
		 * getActualRequiredDisplayClock or program displayclock without
		 * calling vbios. Currently temporily work
		 * around by increasing the displclk by 15 percent
		 */
		dal_display_clock_set_clock(
				context->res_ctx.pool->display_clock,
				context->bw_results.dispclk_khz * 115 / 100);
	}


	/* TODO: When changing display engine clock, DMCU WaitLoop must be
	 * reconfigured in order to maintain the same delays within DMCU
	 * programming sequences. */

	/* TODO: Start GTC counter */
}

static uint32_t compute_pstate_blackout_duration(
	struct bw_fixed blackout_duration,
	const struct core_stream *stream)
{
	uint32_t total_dest_line_time_ns;
	uint32_t pstate_blackout_duration_ns;

	pstate_blackout_duration_ns = 1000 * blackout_duration.value >> 24;

	total_dest_line_time_ns = 1000000UL *
		stream->public.timing.h_total /
		stream->public.timing.pix_clk_khz +
		pstate_blackout_duration_ns;

	return total_dest_line_time_ns;
}

/* get the index of the pipe_ctx if there were no gaps in the pipe_ctx array*/
int get_bw_result_idx(
		struct resource_context *res_ctx,
		int pipe_idx)
{
	int i, collapsed_idx;

	if (res_ctx->pipe_ctx[pipe_idx].top_pipe)
		return 3;

	collapsed_idx = 0;
	for (i = 0; i < pipe_idx; i++) {
		if (res_ctx->pipe_ctx[i].stream)
			collapsed_idx++;
	}

	return collapsed_idx;
}

static bool is_watermark_set_a_greater(
		const struct bw_watermarks *set_a,
		const struct bw_watermarks *set_b)
{
	if (set_a->a_mark > set_b->a_mark
			|| set_a->b_mark > set_b->b_mark
			|| set_a->c_mark > set_b->c_mark
			|| set_a->d_mark > set_b->d_mark)
		return true;
	return false;
}

static bool did_watermarks_increase(
		struct pipe_ctx *pipe_ctx,
		struct validate_context *context,
		struct validate_context *old_context)
{
	int collapsed_pipe_idx = get_bw_result_idx(&context->res_ctx,
			pipe_ctx->pipe_idx);
	int old_collapsed_pipe_idx = get_bw_result_idx(&old_context->res_ctx,
			pipe_ctx->pipe_idx);
	struct pipe_ctx *old_pipe_ctx =  &old_context->res_ctx.pipe_ctx[pipe_ctx->pipe_idx];

	if (!old_pipe_ctx->stream)
		return true;

	if (is_watermark_set_a_greater(
			&context->bw_results.nbp_state_change_wm_ns[collapsed_pipe_idx],
			&old_context->bw_results.nbp_state_change_wm_ns[old_collapsed_pipe_idx]))
		return true;
	if (is_watermark_set_a_greater(
			&context->bw_results.stutter_exit_wm_ns[collapsed_pipe_idx],
			&old_context->bw_results.stutter_exit_wm_ns[old_collapsed_pipe_idx]))
		return true;
	if (is_watermark_set_a_greater(
			&context->bw_results.urgent_wm_ns[collapsed_pipe_idx],
			&old_context->bw_results.urgent_wm_ns[old_collapsed_pipe_idx]))
		return true;

	return false;
}

static void program_wm_for_pipe(struct core_dc *dc,
		struct pipe_ctx *pipe_ctx,
		struct validate_context *context)
{
	int total_dest_line_time_ns = compute_pstate_blackout_duration(
			dc->bw_vbios.blackout_duration,
			pipe_ctx->stream);
	int bw_result_idx = get_bw_result_idx(&context->res_ctx,
				pipe_ctx->pipe_idx);

	pipe_ctx->mi->funcs->mem_input_program_display_marks(
		pipe_ctx->mi,
		context->bw_results.nbp_state_change_wm_ns[bw_result_idx],
		context->bw_results.stutter_exit_wm_ns[bw_result_idx],
		context->bw_results.urgent_wm_ns[bw_result_idx],
		total_dest_line_time_ns);

	if (pipe_ctx->top_pipe)
		pipe_ctx->mi->funcs->mem_input_program_chroma_display_marks(
				pipe_ctx->mi,
				context->bw_results.nbp_state_change_wm_ns[bw_result_idx + 1],
				context->bw_results.stutter_exit_wm_ns[bw_result_idx + 1],
				context->bw_results.urgent_wm_ns[bw_result_idx + 1],
				total_dest_line_time_ns);
}

static void set_displaymarks(
	const struct core_dc *dc,
	struct validate_context *context)
{
	uint8_t i, num_pipes;

	for (i = 0, num_pipes = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[i];
		uint32_t total_dest_line_time_ns;

		if (pipe_ctx->stream == NULL
			|| pipe_ctx->pipe_idx == DCE110_UNDERLAY_IDX)
			continue;

		total_dest_line_time_ns = compute_pstate_blackout_duration(
			dc->bw_vbios.blackout_duration, pipe_ctx->stream);
		pipe_ctx->mi->funcs->mem_input_program_display_marks(
			pipe_ctx->mi,
			context->bw_results.nbp_state_change_wm_ns[num_pipes],
			context->bw_results.stutter_exit_wm_ns[num_pipes],
			context->bw_results.urgent_wm_ns[num_pipes],
			total_dest_line_time_ns);
		num_pipes++;
	}
}

static void set_safe_displaymarks(struct resource_context *res_ctx)
{
	uint8_t i;
	struct bw_watermarks max_marks = {
		MAX_WATERMARK, MAX_WATERMARK, MAX_WATERMARK, MAX_WATERMARK };
	struct bw_watermarks nbp_marks = {
		SAFE_NBP_MARK, SAFE_NBP_MARK, SAFE_NBP_MARK, SAFE_NBP_MARK };

	for (i = 0; i < MAX_PIPES; i++) {
		if (res_ctx->pipe_ctx[i].stream == NULL)
			continue;

		res_ctx->pipe_ctx[i].mi->funcs->mem_input_program_display_marks(
				res_ctx->pipe_ctx[i].mi,
				nbp_marks,
				max_marks,
				max_marks,
				MAX_WATERMARK);
	}
}

static void switch_dp_clock_sources(
	const struct core_dc *dc,
	struct resource_context *res_ctx)
{
	uint8_t i;
	for (i = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe_ctx = &res_ctx->pipe_ctx[i];

		if (pipe_ctx->stream == NULL || pipe_ctx->top_pipe)
			continue;

		if (dc_is_dp_signal(pipe_ctx->stream->signal)) {
			struct clock_source *clk_src =
				resource_find_used_clk_src_for_sharing(
						res_ctx, pipe_ctx);

			if (clk_src &&
				clk_src != pipe_ctx->clock_source) {
				resource_unreference_clock_source(
					res_ctx, pipe_ctx->clock_source);
				pipe_ctx->clock_source = clk_src;
				resource_reference_clock_source(res_ctx, clk_src);
				dc->hwss.crtc_switch_to_clk_src(clk_src, i);
			}
		}
	}
}

/*******************************************************************************
 * Public functions
 ******************************************************************************/

static void reset_single_pipe_hw_ctx(
		const struct core_dc *dc,
		struct pipe_ctx *pipe_ctx,
		struct validate_context *context)
{
	struct dc_bios *dcb;

	dcb = dal_adapter_service_get_bios_parser(
			context->res_ctx.pool->adapter_srv);
	if (pipe_ctx->audio) {
		dal_audio_disable_output(pipe_ctx->audio,
				pipe_ctx->stream_enc->id,
				pipe_ctx->stream->signal);
		pipe_ctx->audio = NULL;
	}

	core_link_disable_stream(pipe_ctx);
	if (!pipe_ctx->tg->funcs->set_blank(pipe_ctx->tg, true)) {
		dm_error("DC: failed to blank crtc!\n");
		BREAK_TO_DEBUGGER();
	}
	pipe_ctx->tg->funcs->disable_crtc(pipe_ctx->tg);
	pipe_ctx->mi->funcs->free_mem_input(
				pipe_ctx->mi, context->target_count);
	pipe_ctx->xfm->funcs->transform_set_scaler_bypass(pipe_ctx->xfm, NULL);
	pipe_ctx->xfm->funcs->transform_reset(pipe_ctx->xfm);
	resource_unreference_clock_source(&context->res_ctx, pipe_ctx->clock_source);
	dc->hwss.enable_display_power_gating(
		pipe_ctx->stream->ctx, pipe_ctx->pipe_idx, dcb,
			PIPE_GATING_CONTROL_ENABLE);

	pipe_ctx->stream = NULL;
}

static void set_drr(struct pipe_ctx **pipe_ctx,
		int num_pipes, int vmin, int vmax)
{
	int i = 0;
	struct drr_params params = {0};

	params.vertical_total_max = vmax;
	params.vertical_total_min = vmin;

	/* TODO: If multiple pipes are to be supported, you need some GSL stuff */

	for (i = 0; i < num_pipes; i++) {
		pipe_ctx[i]->tg->funcs->set_drr(pipe_ctx[i]->tg, &params);
	}
}

/*TODO: const validate_context*/
static enum dc_status apply_ctx_to_hw(
		struct core_dc *dc,
		struct validate_context *context)
{
	enum dc_status status;
	int i;
	bool programmed_audio_dto = false;

	/* Reset old context */
	/* look up the targets that have been removed since last commit */
	for (i = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe_ctx_old =
			&dc->current_context->res_ctx.pipe_ctx[i];
		struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[i];

		/* Note: We need to disable output if clock sources change,
		 * since bios does optimization and doesn't apply if changing
		 * PHY when not already disabled.
		 */

		/* Skip underlay pipe since it will be handled in commit surface*/
		if (!pipe_ctx_old->stream || pipe_ctx_old->top_pipe)
			continue;

		if (!pipe_ctx->stream ||
				pipe_need_reprogram(pipe_ctx_old, pipe_ctx))
			reset_single_pipe_hw_ctx(
				dc, pipe_ctx_old, dc->current_context);
	}

	/* Skip applying if no targets */
	if (context->target_count <= 0)
		return DC_OK;

	/* Apply new context */
	update_bios_scratch_critical_state(context->res_ctx.pool->adapter_srv, true);

	for (i = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe_ctx_old =
					&dc->current_context->res_ctx.pipe_ctx[i];
		struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[i];
		struct dc_bios *dcb;

		if (pipe_ctx->stream == NULL || pipe_ctx->top_pipe)
			continue;

		if (pipe_ctx->stream == pipe_ctx_old->stream) {
			if (pipe_ctx_old->clock_source != pipe_ctx->clock_source)
				dc->hwss.crtc_switch_to_clk_src(
						pipe_ctx->clock_source, i);
			continue;
		}

		dcb = dal_adapter_service_get_bios_parser(
				context->res_ctx.pool->adapter_srv);

		dc->hwss.enable_display_power_gating(
				dc->ctx, i, dcb,
				PIPE_GATING_CONTROL_DISABLE);
	}

	set_safe_displaymarks(&context->res_ctx);
	/*TODO: when pplib works*/
	/*dc_set_clocks_and_clock_state(context);*/

	if (context->bw_results.dispclk_khz
		> dc->current_context->bw_results.dispclk_khz)
		set_display_clock(context);

	for (i = 0; i < MAX_PIPES; i++) {
		struct pipe_ctx *pipe_ctx_old =
					&dc->current_context->res_ctx.pipe_ctx[i];
		struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[i];

		if (pipe_ctx->stream == NULL)
			continue;

		if (pipe_ctx->stream == pipe_ctx_old->stream)
			continue;

		if (pipe_ctx->top_pipe)
			continue;

		status = apply_single_controller_ctx_to_hw(
				pipe_ctx,
				context,
				dc);

		if (DC_OK != status)
			return status;
	}

	/* Setup audio rate clock source */
	/* Issue:
	 * Audio lag happened on DP monitor when unplug a HDMI monitor
	 *
	 * Cause:
	 * In case of DP and HDMI connected or HDMI only, DCCG_AUDIO_DTO_SEL
	 * is set to either dto0 or dto1, audio should work fine.
	 * In case of DP connected only, DCCG_AUDIO_DTO_SEL should be dto1,
	 * set to dto0 will cause audio lag.
	 *
	 * Solution:
	 * Not optimized audio wall dto setup. When mode set, iterate pipe_ctx,
	 * find first available pipe with audio, setup audio wall DTO per topology
	 * instead of per pipe.
	 */
	for (i = 0; i < MAX_PIPES; i++) {
		if (context->res_ctx.pipe_ctx[i].audio != NULL) {
			struct audio_output audio_output;
			struct pipe_ctx *pipe_ctx = &context->res_ctx.pipe_ctx[i];

			build_audio_output(pipe_ctx, &audio_output);
			if (AUDIO_RESULT_OK != dal_audio_setup(
					pipe_ctx->audio,
					&audio_output,
					&pipe_ctx->stream->public.audio_info)) {
				BREAK_TO_DEBUGGER();
				return DC_ERROR_UNEXPECTED;
			}
			if (!programmed_audio_dto) {
				dal_audio_setup_audio_wall_dto(
					pipe_ctx->audio,
					pipe_ctx->stream->signal,
					&audio_output.crtc_info,
					&audio_output.pll_info);
				programmed_audio_dto = true;
			}
		}
	}

	dc->hwss.set_displaymarks(dc, context);

	update_bios_scratch_critical_state(context->res_ctx.pool->adapter_srv, false);

	switch_dp_clock_sources(dc, &context->res_ctx);

	return DC_OK;
}

/*******************************************************************************
 * Front End programming
 ******************************************************************************/
static void set_default_colors(struct pipe_ctx *pipe_ctx)
{
	struct default_adjustment default_adjust = { 0 };

	default_adjust.force_hw_default = false;
	if (pipe_ctx->surface == NULL)
		default_adjust.in_color_space = COLOR_SPACE_SRGB;
	else
		default_adjust.in_color_space =
				pipe_ctx->surface->public.color_space;
	if (pipe_ctx->stream == NULL)
		default_adjust.out_color_space = COLOR_SPACE_SRGB;
	else
		default_adjust.out_color_space =
				pipe_ctx->stream->public.output_color_space;
	default_adjust.csc_adjust_type = GRAPHICS_CSC_ADJUST_TYPE_SW;
	default_adjust.surface_pixel_format = pipe_ctx->scl_data.format;

	/* display color depth */
	default_adjust.color_depth =
		pipe_ctx->stream->public.timing.display_color_depth;

	/* Lb color depth */
	default_adjust.lb_color_depth = LB_PIXEL_DEPTH_30BPP;
	/*dal_hw_sequencer_translate_to_lb_color_depth(
			build_params->
			line_buffer_params[path_id][plane_id].depth);*/

	pipe_ctx->opp->funcs->opp_set_csc_default(
					pipe_ctx->opp, &default_adjust);
}

static bool blender_configuration_changed(struct pipe_ctx *pipe_ctx, struct pipe_ctx *old_pipe_ctx)
{
	if (pipe_ctx->bottom_pipe && !old_pipe_ctx->bottom_pipe)
		return true;

	if (!pipe_ctx->bottom_pipe && old_pipe_ctx->bottom_pipe)
		return true;

	return false;
}

static void program_blender_if_needed(const struct core_dc *dc,
		struct pipe_ctx *pipe_ctx)
{
	struct pipe_ctx *old_pipe_ctx = &dc->current_context->res_ctx.pipe_ctx[pipe_ctx->pipe_idx];
	enum blender_mode blender_mode = BLENDER_MODE_CURRENT_PIPE;

	if (!old_pipe_ctx->stream || blender_configuration_changed(pipe_ctx, old_pipe_ctx)) {
		if (pipe_ctx->bottom_pipe)
			blender_mode = BLENDER_MODE_BLENDING;
		dc->hwss.set_blender_mode(
			dc->ctx, pipe_ctx->pipe_idx, blender_mode);
	}
}

/**
 * Program the Front End of the Pipe.
 * The Back End was already programmed by Set Mode.
 */
static void set_plane_config(
	const struct core_dc *dc,
	struct pipe_ctx *pipe_ctx,
	struct resource_context *res_ctx)
{
	struct mem_input *mi = pipe_ctx->mi;
	struct pipe_ctx *old_pipe = NULL;
	struct dc_context *ctx = pipe_ctx->stream->ctx;
	struct core_surface *surface = pipe_ctx->surface;
	struct xfm_grph_csc_adjustment adjust;
	struct out_csc_color_matrix tbl_entry;
	unsigned int i;

	if (dc->current_context)
		old_pipe =
			&dc->current_context->
			res_ctx.pipe_ctx[pipe_ctx->pipe_idx];

	memset(&adjust, 0, sizeof(adjust));
	memset(&tbl_entry, 0, sizeof(tbl_entry));
	adjust.gamut_adjust_type = GRAPHICS_GAMUT_ADJUST_TYPE_BYPASS;

	dc->hwss.enable_fe_clock(ctx, pipe_ctx->pipe_idx, true);

	if (pipe_ctx->stream->public.csc_color_matrix.enable_adjustment
			== true) {
		tbl_entry.color_space =
			pipe_ctx->stream->public.output_color_space;

		for (i = 0; i < 12; i++)
			tbl_entry.regval[i] =
			pipe_ctx->stream->public.csc_color_matrix.matrix[i];

		pipe_ctx->opp->funcs->opp_set_csc_adjustment
				(pipe_ctx->opp, &tbl_entry);
	} else {
		set_default_colors(pipe_ctx);
	}

	if (pipe_ctx->stream->public.gamut_remap_matrix.enable_remap == true) {
		adjust.gamut_adjust_type = GRAPHICS_GAMUT_ADJUST_TYPE_SW;
		adjust.temperature_matrix[0] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[0];
		adjust.temperature_matrix[1] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[1];
		adjust.temperature_matrix[2] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[2];
		adjust.temperature_matrix[3] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[4];
		adjust.temperature_matrix[4] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[5];
		adjust.temperature_matrix[5] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[6];
		adjust.temperature_matrix[6] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[8];
		adjust.temperature_matrix[7] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[9];
		adjust.temperature_matrix[8] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[10];
	}

	pipe_ctx->xfm->funcs->transform_set_gamut_remap(pipe_ctx->xfm, &adjust);


	program_scaler(dc, pipe_ctx);

	program_blender_if_needed(dc, pipe_ctx);

	if (pipe_ctx->bottom_pipe)
		pipe_ctx->xfm->funcs->transform_set_alpha(pipe_ctx->xfm, true);

	mi->funcs->mem_input_program_surface_config(
			mi,
			surface->public.format,
			&surface->public.tiling_info,
			&surface->public.plane_size,
			surface->public.rotation);

	if (dc->public.config.gpu_vm_support)
		mi->funcs->mem_input_program_pte_vm(
				pipe_ctx->mi,
				surface->public.format,
				&surface->public.tiling_info,
				surface->public.rotation);
}

static void update_plane_addr(const struct core_dc *dc,
		struct pipe_ctx *pipe_ctx)
{
	struct core_surface *surface = pipe_ctx->surface;

	if (surface == NULL)
		return;

	pipe_ctx->mi->funcs->mem_input_program_surface_flip_and_addr(
			pipe_ctx->mi,
			&surface->public.address,
			surface->public.flip_immediate);

	surface->status.requested_address = surface->public.address;

	if (surface->public.visible)
		pipe_ctx->tg->funcs->set_blank(pipe_ctx->tg, false);

	if (surface->public.flip_immediate)
		while (pipe_ctx->mi->funcs->mem_input_is_flip_pending(
				pipe_ctx->mi));
}

static void update_pending_status(struct pipe_ctx *pipe_ctx)
{
	struct core_surface *surface = pipe_ctx->surface;

	if (surface == NULL)
		return;

	surface->status.is_flip_pending =
			pipe_ctx->mi->funcs->mem_input_is_flip_pending(
					pipe_ctx->mi);

	surface->status.current_address = pipe_ctx->mi->current_address;
}

static void power_down(struct core_dc *dc)
{
	power_down_all_hw_blocks(dc);
	disable_vga_and_power_gate_all_controllers(dc);
}

static bool wait_for_reset_trigger_to_occur(
	struct dc_context *dc_ctx,
	struct timing_generator *tg)
{
	bool rc = false;

	/* To avoid endless loop we wait at most
	 * frames_to_wait_on_triggered_reset frames for the reset to occur. */
	const uint32_t frames_to_wait_on_triggered_reset = 10;
	uint32_t i;

	for (i = 0; i < frames_to_wait_on_triggered_reset; i++) {

		if (!tg->funcs->is_counter_moving(tg)) {
			DC_ERROR("TG counter is not moving!\n");
			break;
		}

		if (tg->funcs->did_triggered_reset_occur(tg)) {
			rc = true;
			/* usually occurs at i=1 */
			DC_SYNC_INFO("GSL: reset occurred at wait count: %d\n",
					i);
			break;
		}

		/* Wait for one frame. */
		tg->funcs->wait_for_state(tg, CRTC_STATE_VACTIVE);
		tg->funcs->wait_for_state(tg, CRTC_STATE_VBLANK);
	}

	if (false == rc)
		DC_ERROR("GSL: Timeout on reset trigger!\n");

	return rc;
}

/* Enable timing synchronization for a group of Timing Generators. */
static void dce110_enable_timing_synchronization(
		struct core_dc *dc,
		int group_index,
		int group_size,
		struct pipe_ctx *grouped_pipes[])
{
	struct dc_context *dc_ctx = dc->ctx;
	struct dcp_gsl_params gsl_params = { 0 };
	int i;

	DC_SYNC_INFO("GSL: Setting-up...\n");

	/* Designate a single TG in the group as a master.
	 * Since HW doesn't care which one, we always assign
	 * the 1st one in the group. */
	gsl_params.gsl_group = 0;
	gsl_params.gsl_master = grouped_pipes[0]->tg->inst;

	for (i = 0; i < group_size; i++)
		grouped_pipes[i]->tg->funcs->setup_global_swap_lock(
					grouped_pipes[i]->tg, &gsl_params);

	/* Reset slave controllers on master VSync */
	DC_SYNC_INFO("GSL: enabling trigger-reset\n");

	for (i = 1 /* skip the master */; i < group_size; i++)
		grouped_pipes[i]->tg->funcs->enable_reset_trigger(
					grouped_pipes[i]->tg, gsl_params.gsl_group);



	for (i = 1 /* skip the master */; i < group_size; i++) {
		DC_SYNC_INFO("GSL: waiting for reset to occur.\n");
		wait_for_reset_trigger_to_occur(dc_ctx, grouped_pipes[i]->tg);
		/* Regardless of success of the wait above, remove the reset or
		 * the driver will start timing out on Display requests. */
		DC_SYNC_INFO("GSL: disabling trigger-reset.\n");
		grouped_pipes[i]->tg->funcs->disable_reset_trigger(grouped_pipes[i]->tg);
	}


	/* GSL Vblank synchronization is a one time sync mechanism, assumption
	 * is that the sync'ed displays will not drift out of sync over time*/
	DC_SYNC_INFO("GSL: Restoring register states.\n");
	for (i = 0; i < group_size; i++)
		grouped_pipes[i]->tg->funcs->tear_down_global_swap_lock(grouped_pipes[i]->tg);

	DC_SYNC_INFO("GSL: Set-up complete.\n");
}

static void init_hw(struct core_dc *dc)
{
	int i;
	struct dc_bios *bp;
	struct transform *xfm;

	bp = dc->ctx->dc_bios;
	for (i = 0; i < dc->res_pool->pipe_count; i++) {
		xfm = dc->res_pool->transforms[i];
		xfm->funcs->transform_reset(xfm);

		dc->hwss.enable_display_power_gating(
				dc->ctx, i, bp,
				PIPE_GATING_CONTROL_INIT);
		dc->hwss.enable_display_power_gating(
				dc->ctx, i, bp,
				PIPE_GATING_CONTROL_DISABLE);
		xfm->funcs->transform_power_up(xfm);
		dc->hwss.enable_display_pipe_clock_gating(
			dc->ctx,
			true);
	}

	dc->hwss.clock_gating_power_up(dc->ctx, false);
	bp->funcs->power_up(bp);
	/***************************************/

	for (i = 0; i < dc->link_count; i++) {
		/****************************************/
		/* Power up AND update implementation according to the
		 * required signal (which may be different from the
		 * default signal on connector). */
		struct core_link *link = dc->links[i];
		link->link_enc->funcs->hw_init(link->link_enc);
	}

	for (i = 0; i < dc->res_pool->pipe_count; i++) {
		struct timing_generator *tg = dc->res_pool->timing_generators[i];

		tg->funcs->disable_vga(tg);

		/* Blank controller using driver code instead of
		 * command table. */
		tg->funcs->set_blank(tg, true);
	}

	for (i = 0; i < dc->res_pool->audio_count; i++) {
		struct audio *audio = dc->res_pool->audios[i];

		if (dal_audio_power_up(audio) != AUDIO_RESULT_OK)
			dm_error("Failed audio power up!\n");
	}
}

/* TODO: move this to apply_ctx_tohw some how?*/
static void dce110_power_on_pipe_if_needed(
		struct core_dc *dc,
		struct pipe_ctx *pipe_ctx,
		struct validate_context *context)
{
	struct pipe_ctx *old_pipe_ctx = &dc->current_context->res_ctx.pipe_ctx[pipe_ctx->pipe_idx];
	struct dc_bios *dcb = dal_adapter_service_get_bios_parser(
					context->res_ctx.pool->adapter_srv);
	struct tg_color black_color = {0};

	if (!old_pipe_ctx->stream && pipe_ctx->stream) {
		dc->hwss.enable_display_power_gating(
				dc->ctx,
				pipe_ctx->pipe_idx,
				dcb, PIPE_GATING_CONTROL_DISABLE);

		/*
		 * This is for powering on underlay, so crtc does not
		 * need to be enabled
		 */

		pipe_ctx->tg->funcs->program_timing(pipe_ctx->tg,
				&pipe_ctx->stream->public.timing,
				false);

		pipe_ctx->tg->funcs->enable_advanced_request(
				pipe_ctx->tg,
				true,
				&pipe_ctx->stream->public.timing);

		pipe_ctx->mi->funcs->allocate_mem_input(pipe_ctx->mi,
				pipe_ctx->stream->public.timing.h_total,
				pipe_ctx->stream->public.timing.v_total,
				pipe_ctx->stream->public.timing.pix_clk_khz,
				context->target_count);

		/* TODO unhardcode*/
		color_space_to_black_color(COLOR_SPACE_YCBCR601, &black_color);
		pipe_ctx->tg->funcs->set_blank_color(
				pipe_ctx->tg,
				&black_color);
	}
}

static void dce110_increase_watermarks_for_pipe(
		struct core_dc *dc,
		struct pipe_ctx *pipe_ctx,
		struct validate_context *context)
{
	if (did_watermarks_increase(pipe_ctx, context, dc->current_context))
		program_wm_for_pipe(dc, pipe_ctx, context);
}

static void dce110_set_bandwidth(struct core_dc *dc)
{
	int i;

	for (i = 0; i < dc->current_context->res_ctx.pool->pipe_count; i++) {
		struct pipe_ctx *pipe_ctx = &dc->current_context->res_ctx.pipe_ctx[i];

		if (!pipe_ctx->stream)
			continue;

		program_wm_for_pipe(dc, pipe_ctx, dc->current_context);
	}

	dc->hwss.set_display_clock(dc->current_context);
}

static void dce110_program_blending(struct core_dc *dc,
		struct pipe_ctx *pipe_ctx,
		struct validate_context *context)
{
	program_blender_if_needed(dc, pipe_ctx);
}

static void dce110_program_front_end_for_pipe(struct core_dc *dc,
		struct pipe_ctx *pipe_ctx,
		struct validate_context *context)
{
	struct mem_input *mi = pipe_ctx->mi;
	struct pipe_ctx *old_pipe = NULL;
	struct dc_context *ctx = pipe_ctx->stream->ctx;
	struct core_surface *surface = pipe_ctx->surface;
	struct xfm_grph_csc_adjustment adjust;

	int lock_mask =
		PIPE_LOCK_CONTROL_GRAPHICS |
		PIPE_LOCK_CONTROL_SCL |
		PIPE_LOCK_CONTROL_BLENDER |
		PIPE_LOCK_CONTROL_MODE;

	if (!pipe_ctx->surface->public.flip_immediate)
		lock_mask |= PIPE_LOCK_CONTROL_SURFACE;

	if (blender_configuration_changed(pipe_ctx, &dc->current_context->res_ctx.pipe_ctx[pipe_ctx->pipe_idx]))
		dc->hwss.pipe_control_lock(
				dc->ctx,
				pipe_ctx->pipe_idx,
				lock_mask,
				true);

	if (dc->current_context)
		old_pipe = &dc->current_context->res_ctx.pipe_ctx[pipe_ctx->pipe_idx];

	memset(&adjust, 0, sizeof(adjust));
	adjust.gamut_adjust_type = GRAPHICS_GAMUT_ADJUST_TYPE_BYPASS;

	dc->hwss.enable_fe_clock(ctx, pipe_ctx->pipe_idx, true);

	set_default_colors(pipe_ctx);

	if (pipe_ctx->stream->public.gamut_remap_matrix.enable_remap == true) {
		adjust.gamut_adjust_type = GRAPHICS_GAMUT_ADJUST_TYPE_SW;
		adjust.temperature_matrix[0] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[0];
		adjust.temperature_matrix[1] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[1];
		adjust.temperature_matrix[2] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[2];
		adjust.temperature_matrix[3] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[4];
		adjust.temperature_matrix[4] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[5];
		adjust.temperature_matrix[5] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[6];
		adjust.temperature_matrix[6] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[8];
		adjust.temperature_matrix[7] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[9];
		adjust.temperature_matrix[8] =
				pipe_ctx->stream->
				public.gamut_remap_matrix.matrix[10];
	}

	pipe_ctx->xfm->funcs->transform_set_gamut_remap(pipe_ctx->xfm, &adjust);

	if (old_pipe && memcmp(&old_pipe->scl_data,
				&pipe_ctx->scl_data,
				sizeof(struct scaler_data)) != 0)
		program_scaler(dc, pipe_ctx);

	if (pipe_ctx->bottom_pipe)
		pipe_ctx->xfm->funcs->transform_set_alpha(pipe_ctx->xfm, true);

	mi->funcs->mem_input_program_surface_config(
			mi,
			surface->public.format,
			&surface->public.tiling_info,
			&surface->public.plane_size,
			surface->public.rotation);

	if (dc->public.config.gpu_vm_support)
		mi->funcs->mem_input_program_pte_vm(
				pipe_ctx->mi,
				surface->public.format,
				&surface->public.tiling_info,
				surface->public.rotation);

	dc->hwss.update_plane_addr(dc, pipe_ctx);

	dal_logger_write(dc->ctx->logger,
			LOG_MAJOR_INTERFACE_TRACE,
			LOG_MINOR_COMPONENT_SURFACE,
			"Pipe:%d 0x%x: addr hi:0x%x, "
			"addr low:0x%x, "
			"src: %d, %d, %d,"
			" %d; dst: %d, %d, %d, %d;"
			"clip: %d, %d, %d, %d\n",
			pipe_ctx->pipe_idx,
			pipe_ctx->surface,
			pipe_ctx->surface->public.address.grph.addr.high_part,
			pipe_ctx->surface->public.address.grph.addr.low_part,
			pipe_ctx->surface->public.src_rect.x,
			pipe_ctx->surface->public.src_rect.y,
			pipe_ctx->surface->public.src_rect.width,
			pipe_ctx->surface->public.src_rect.height,
			pipe_ctx->surface->public.dst_rect.x,
			pipe_ctx->surface->public.dst_rect.y,
			pipe_ctx->surface->public.dst_rect.width,
			pipe_ctx->surface->public.dst_rect.height,
			pipe_ctx->surface->public.clip_rect.x,
			pipe_ctx->surface->public.clip_rect.y,
			pipe_ctx->surface->public.clip_rect.width,
			pipe_ctx->surface->public.clip_rect.height);

	dal_logger_write(dc->ctx->logger,
			LOG_MAJOR_INTERFACE_TRACE,
			LOG_MINOR_COMPONENT_SURFACE,
			"Pipe %d: width, height, x, y\n"
			"viewport:%d, %d, %d, %d\n"
			"recout:  %d, %d, %d, %d\n",
			pipe_ctx->pipe_idx,
			pipe_ctx->scl_data.viewport.width,
			pipe_ctx->scl_data.viewport.height,
			pipe_ctx->scl_data.viewport.x,
			pipe_ctx->scl_data.viewport.y,
			pipe_ctx->scl_data.recout.width,
			pipe_ctx->scl_data.recout.height,
			pipe_ctx->scl_data.recout.x,
			pipe_ctx->scl_data.recout.y);
}



static void dce110_prepare_pipe_for_surface_commit(
		struct core_dc *dc,
		struct pipe_ctx *pipe_ctx,
		struct validate_context *context) {
	struct core_gamma *gamma = NULL;

	dc->hwss.increase_watermarks_for_pipe(dc, pipe_ctx, context);

	if (pipe_ctx->surface->public.gamma_correction)
		gamma = DC_GAMMA_TO_CORE(
			pipe_ctx->surface->public.gamma_correction);

	dc->hwss.set_gamma_correction(
			pipe_ctx->ipp,
			pipe_ctx->opp,
			gamma, pipe_ctx->surface);
}

static enum dc_status apply_ctx_to_surface_locked(
		struct core_dc *dc,
		struct validate_context *context)
{
	int i;

	for (i = 0; i < context->res_ctx.pool->pipe_count; i++) {
		struct pipe_ctx *head_pipe = &context->res_ctx.pipe_ctx[i];

		if (!head_pipe->surface || head_pipe->top_pipe != NULL)
			continue;

		hw_sequencer_program_pipe_tree(dc, context, head_pipe,
				dce110_power_on_pipe_if_needed);

		hw_sequencer_program_pipe_tree(dc, context, head_pipe,
				dce110_prepare_pipe_for_surface_commit);

	}


	return DC_OK;
}

static enum dc_status apply_ctx_to_surface_unlock(
		struct core_dc *dc,
		struct validate_context *context)
{
	int i;

	for (i = 0; i < context->res_ctx.pool->pipe_count; i++) {
		struct pipe_ctx *head_pipe = &context->res_ctx.pipe_ctx[i];

		if (!head_pipe->surface || head_pipe->top_pipe != NULL)
			continue;

		hw_sequencer_program_pipe_tree(dc, context, head_pipe,
				dce110_program_front_end_for_pipe);

		hw_sequencer_program_pipe_tree(dc, context, head_pipe,
				dce110_program_blending);
	}

	/* Go in reverse order so that all pipes are unlocked simultaneously
	 * when pipe 0 is unlocked
	 * Need PIPE_LOCK_CONTROL_MODE to be 1 for this
	 */
	for (i = context->res_ctx.pool->pipe_count - 1; i >= 0; i--) {
		struct pipe_ctx *pipe_ctx =
					&context->res_ctx.pipe_ctx[i];

		dc->hwss.pipe_control_lock(
				dc->ctx,
				pipe_ctx->pipe_idx,
				PIPE_LOCK_CONTROL_GRAPHICS |
				PIPE_LOCK_CONTROL_SCL |
				PIPE_LOCK_CONTROL_BLENDER |
				PIPE_LOCK_CONTROL_SURFACE,
				false);
	}

	return DC_OK;
}

static void update_plane_surface(
	struct core_dc *dc,
	struct validate_context *context,
	struct dc_surface *new_surfaces[],
	uint8_t new_surface_count)
{
	int i, j;

	for (i = 0; i < new_surface_count; i++)
		for (j = 0; j < context->res_ctx.pool->pipe_count; j++) {
			struct pipe_ctx *pipe_ctx =
						&context->res_ctx.pipe_ctx[j];

			if (pipe_ctx->surface !=
					DC_SURFACE_TO_CORE(new_surfaces[i]))
				continue;

			dc->hwss.set_plane_config(
				dc, pipe_ctx, &context->res_ctx);
		}
}

static const struct hw_sequencer_funcs dce110_funcs = {
	.init_hw = init_hw,
	.apply_ctx_to_hw = apply_ctx_to_hw,
	.apply_ctx_to_surface_locked = apply_ctx_to_surface_locked,
	.apply_ctx_to_surface_unlock = apply_ctx_to_surface_unlock,
	.set_plane_config = set_plane_config,
	.update_plane_addr = update_plane_addr,
	.update_pending_status = update_pending_status,
	.update_plane_surface = update_plane_surface,
	.set_gamma_correction = set_gamma_ramp,
	.power_down = power_down,
	.enable_accelerated_mode = enable_accelerated_mode,
	.enable_timing_synchronization = dce110_enable_timing_synchronization,
	.update_info_frame = update_info_frame,
	.enable_stream = enable_stream,
	.disable_stream = disable_stream,
	.enable_display_pipe_clock_gating = enable_display_pipe_clock_gating,
	.crtc_switch_to_clk_src = dce110_crtc_switch_to_clk_src,
	.enable_display_power_gating = dce110_enable_display_power_gating,
	.enable_fe_clock = dce110_enable_fe_clock,
	.pipe_control_lock = dce110_pipe_control_lock,
	.set_blender_mode = dce110_set_blender_mode,
	.clock_gating_power_up = dal_dc_clock_gating_dce110_power_up,/*todo*/
	.set_display_clock = set_display_clock,
	.set_displaymarks = set_displaymarks,
	.increase_watermarks_for_pipe = dce110_increase_watermarks_for_pipe,
	.set_bandwidth = dce110_set_bandwidth,
	.set_drr = set_drr
};

bool dce110_hw_sequencer_construct(struct core_dc *dc)
{
	dc->hwss = dce110_funcs;

	return true;
}

