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
#include "dc_services.h"
#include "dc.h"
#include "core_types.h"
#include "resource.h"

/*******************************************************************************
 * Private definitions
 ******************************************************************************/

struct stream {
	struct core_stream protected;
	int ref_count;
};

#define DC_STREAM_TO_STREAM(dc_stream) container_of(dc_stream, struct stream, protected.public)

/*******************************************************************************
 * Private functions
 ******************************************************************************/

static void build_bit_depth_reduction_params(
		struct bit_depth_reduction_params *fmt_bit_depth)
{
	/*TODO: Need to un-hardcode, refer to function with same name
	 * in dal2 hw_sequencer*/

	fmt_bit_depth->flags.TRUNCATE_ENABLED = 0;
	fmt_bit_depth->flags.SPATIAL_DITHER_ENABLED = 0;
	fmt_bit_depth->flags.FRAME_MODULATION_ENABLED = 0;
	fmt_bit_depth->flags.SPATIAL_DITHER_DEPTH = 1;

	fmt_bit_depth->flags.SPATIAL_DITHER_ENABLED = 1;
	/* frame random is on by default */
	fmt_bit_depth->flags.FRAME_RANDOM = 1;
	/* apply RGB dithering */
	fmt_bit_depth->flags.RGB_RANDOM = true;

	return;

}

static void setup_pixel_encoding(
	struct clamping_and_pixel_encoding_params *clamping)
{
	/*TODO: Need to un-hardcode, refer to function with same name
		 * in dal2 hw_sequencer*/

	clamping->pixel_encoding = PIXEL_ENCODING_RGB;

	return;
}

static bool construct(struct core_stream *stream,
	const struct dc_sink *dc_sink_data)
{
	uint32_t i = 0;

	stream->sink = DC_SINK_TO_CORE(dc_sink_data);
	stream->ctx = stream->sink->ctx;
	stream->public.sink = dc_sink_data;

	dc_sink_retain(dc_sink_data);

	build_bit_depth_reduction_params(&stream->fmt_bit_depth);
	setup_pixel_encoding(&stream->clamping);

	/* Copy audio modes */
	/* TODO - Remove this translation */
	for (i = 0; i < (dc_sink_data->edid_caps.audio_mode_count); i++)
	{
		stream->public.audio_info.modes[i].channel_count = dc_sink_data->edid_caps.audio_modes[i].channel_count;
		stream->public.audio_info.modes[i].format_code = dc_sink_data->edid_caps.audio_modes[i].format_code;
		stream->public.audio_info.modes[i].sample_rates.all = dc_sink_data->edid_caps.audio_modes[i].sample_rate;
		stream->public.audio_info.modes[i].sample_size = dc_sink_data->edid_caps.audio_modes[i].sample_size;
	}
	stream->public.audio_info.mode_count = dc_sink_data->edid_caps.audio_mode_count;
	stream->public.audio_info.audio_latency = dc_sink_data->edid_caps.audio_latency;
	stream->public.audio_info.video_latency = dc_sink_data->edid_caps.video_latency;
	dc_service_memmove(
		stream->public.audio_info.display_name,
		dc_sink_data->edid_caps.display_name,
		AUDIO_INFO_DISPLAY_NAME_SIZE_IN_CHARS);
	stream->public.audio_info.manufacture_id = dc_sink_data->edid_caps.manufacturer_id;
	stream->public.audio_info.product_id = dc_sink_data->edid_caps.product_id;
	stream->public.audio_info.flags.all = dc_sink_data->edid_caps.speaker_flags;

	/* TODO - Unhardcode port_id */
	stream->public.audio_info.port_id[0] = 0x5558859e;
	stream->public.audio_info.port_id[1] = 0xd989449;

	/* EDID CAP translation for HDMI 2.0 */
	stream->public.timing.flags.LTE_340MCSC_SCRAMBLE = dc_sink_data->edid_caps.lte_340mcsc_scramble;
	return true;
}

static void destruct(struct core_stream *stream)
{
	dc_sink_release(&stream->sink->public);
}

void dc_stream_retain(struct dc_stream *dc_stream)
{
	struct stream *stream = DC_STREAM_TO_STREAM(dc_stream);
	stream->ref_count++;
}

void dc_stream_release(struct dc_stream *public)
{
	struct stream *stream = DC_STREAM_TO_STREAM(public);
	struct core_stream *protected = DC_STREAM_TO_CORE(public);
	struct dc_context *ctx = protected->ctx;
	stream->ref_count--;

	if (stream->ref_count == 0) {
		destruct(protected);
		dc_service_free(ctx, stream);
	}
}

struct dc_stream *dc_create_stream_for_sink(const struct dc_sink *dc_sink)
{
	struct core_sink *sink = DC_SINK_TO_CORE(dc_sink);
	struct stream *stream;

	if (sink == NULL)
		goto alloc_fail;

	stream = dc_service_alloc(sink->ctx, sizeof(struct stream));

	if (NULL == stream)
		goto alloc_fail;

	if (false == construct(&stream->protected, dc_sink))
			goto construct_fail;

	dc_stream_retain(&stream->protected.public);

	return &stream->protected.public;

construct_fail:
	dc_service_free(sink->ctx, stream);

alloc_fail:
	return NULL;
}



