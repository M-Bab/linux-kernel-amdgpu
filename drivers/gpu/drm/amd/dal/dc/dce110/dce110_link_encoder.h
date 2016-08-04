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

#ifndef __DC_LINK_ENCODER__DCE110_H__
#define __DC_LINK_ENCODER__DCE110_H__

#include "inc/link_encoder.h"

#define TO_DCE110_LINK_ENC(link_encoder)\
	container_of(link_encoder, struct dce110_link_encoder, base)

struct dce110_link_enc_offsets {
	uint32_t dig_offset;
	uint32_t dp_offset;
};

struct dce110_link_encoder {
	struct link_encoder base;
	struct dce110_link_enc_offsets offsets;
};

struct link_encoder *dce110_link_encoder_create(
	const struct encoder_init_data *init);

void dce110_link_encoder_destroy(struct link_encoder **enc);

bool dce110_link_encoder_validate_output_with_stream(
	struct link_encoder *enc,
	const struct core_stream *stream);

/****************** HW programming ************************/

/* initialize HW */  /* why do we initialze aux in here? */
bool dce110_link_encoder_power_up(struct link_encoder *enc);

/* program DIG_MODE in DIG_BE */
/* TODO can this be combined with enable_output? */
void dce110_link_encoder_setup(
	struct link_encoder *enc,
	enum signal_type signal);

/* enables TMDS PHY output */
/* TODO: still need depth or just pass in adjusted pixel clock? */
bool dce110_link_encoder_enable_tmds_output(
	struct link_encoder *enc,
	enum clock_source_id clock_source,
	enum dc_color_depth color_depth,
	uint32_t pixel_clock);

/* enables TMDS PHY output */
/* TODO: still need this or just pass in adjusted pixel clock? */
bool dce110_link_encoder_enable_dual_link_tmds_output(
	struct link_encoder *enc,
	enum clock_source_id clock_source,
	enum dc_color_depth color_depth,
	uint32_t pixel_clock);

/* enables DP PHY output */
bool dce110_link_encoder_enable_dp_output(
	struct link_encoder *enc,
	const struct link_settings *link_settings,
	enum clock_source_id clock_source);

/* enables DP PHY output in MST mode */
bool dce110_link_encoder_enable_dp_mst_output(
	struct link_encoder *enc,
	const struct link_settings *link_settings,
	enum clock_source_id clock_source);

/* disable PHY output */
bool dce110_link_encoder_disable_output(
	struct link_encoder *enc,
	enum signal_type signal);

/* set DP lane settings */
bool dce110_link_encoder_dp_set_lane_settings(
	struct link_encoder *enc,
	const struct link_training_settings *link_settings);

void dce110_link_encoder_set_dp_phy_pattern(
	struct link_encoder *enc,
	const struct encoder_set_dp_phy_pattern_param *param);

/* programs DP MST VC payload allocation */
void dce110_link_encoder_update_mst_stream_allocation_table(
	struct link_encoder *enc,
	const struct dp_mst_stream_allocation_table *table);

void dce110_link_encoder_set_lcd_backlight_level(
	struct link_encoder *enc,
	uint32_t level);

bool dce110_link_encoder_enable_output(
	struct link_encoder *enc,
	const struct link_settings *link_settings,
	enum engine_id engine,
	enum clock_source_id clock_source,
	enum signal_type signal,
	enum dc_color_depth color_depth,
	uint32_t pixel_clock);

void dce110_link_encoder_connect_dig_be_to_fe(
	struct link_encoder *enc,
	enum engine_id engine,
	bool connect);


#endif /* __DC_LINK_ENCODER__DCE110_H__ */
