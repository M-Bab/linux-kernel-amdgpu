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

#ifndef __DAL_AUDIO_INTERFACE_H__
#define __DAL_AUDIO_INTERFACE_H__

#include "audio_types.h"
#include "adapter_service_interface.h"
#include "signal_types.h"
#include "link_service_types.h"

/* forward declaration */
struct audio;
struct dal_adapter_service;

/*****  audio initialization data  *****/
/*
 * by audio, it means audio endpoint id. ASIC may have many endpoints.
 * upper sw layer will create one audio object instance for each endpoints.
 * ASIC support internal audio only. So enum number is used to differ
 * each endpoint
 */
struct audio_init_data {
	struct graphics_object_id audio_stream_id;
	struct dc_context *ctx;

	unsigned int inst;
	const struct dce110_audio_registers *reg;
};

enum audio_result {
	AUDIO_RESULT_OK,
	AUDIO_RESULT_ERROR,
};

/****** audio object create, destroy ******/

void dal_audio_destroy(
	struct audio **audio);

/***** programming interface *****/

/* perform power up sequence (boot up, resume, recovery) */
enum audio_result dal_audio_power_up(
	struct audio *audio);

/***** information interface *****/

/* Update audio wall clock source */
void dal_audio_setup_audio_wall_dto(
	struct audio *audio,
	enum signal_type signal,
	const struct audio_crtc_info *crtc_info,
	const struct audio_pll_info *pll_info);

#endif
