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

#include <drm/drm_crtc.h>

#include "amdgpu.h"
#include "amdgpu_dm.h"
#include "dc.h"

enum amdgpu_dm_pipe_crc_source {
	AMDGPU_DM_PIPE_CRC_SOURCE_NONE = 0,
	AMDGPU_DM_PIPE_CRC_SOURCE_AUTO,
	AMDGPU_DM_PIPE_CRC_SOURCE_MAX,
	AMDGPU_DM_PIPE_CRC_SOURCE_INVALID = -1,
};

static enum amdgpu_dm_pipe_crc_source dm_parse_crc_source(const char *source)
{
	if (!source || !strcmp(source, "none"))
		return AMDGPU_DM_PIPE_CRC_SOURCE_NONE;
	if (!strcmp(source, "auto"))
		return AMDGPU_DM_PIPE_CRC_SOURCE_AUTO;

	return AMDGPU_DM_PIPE_CRC_SOURCE_INVALID;
}

int amdgpu_dm_crtc_set_crc_source(struct drm_crtc *crtc, const char *src_name,
			   size_t *values_cnt)
{
	struct dm_crtc_state *crtc_state = to_dm_crtc_state(crtc->state);
	struct dc_stream_state *stream_state = crtc_state->stream;
	bool ret;

	enum amdgpu_dm_pipe_crc_source source = dm_parse_crc_source(src_name);

	if (source < 0) {
		DRM_DEBUG_DRIVER("Unknown CRC source %s for CRTC%d\n",
				 src_name, crtc->index);
		return -EINVAL;
	}

	if (source == AMDGPU_DM_PIPE_CRC_SOURCE_AUTO) {
		ret = dc_stream_configure_crc(stream_state->ctx->dc,
					      stream_state,
					      true, true);
	} else {
		ret = dc_stream_configure_crc(stream_state->ctx->dc,
					      stream_state,
					      false, false);
	}

	if (ret) {
		*values_cnt = 3;
		/* Reset crc_skipped flag on dm state */
		crtc_state->crc_first_skipped = false;
		return 0;
	}
	return -EINVAL;
}

/**
 * amdgpu_dm_crtc_handle_crc_irq: Report to DRM the CRC on given CRTC.
 * @crtc: DRM CRTC object.
 *
 * This function should be called at the end of a vblank, when the fb has been
 * fully processed through the pipe.
 */
void amdgpu_dm_crtc_handle_crc_irq(struct drm_crtc *crtc)
{
	struct dm_crtc_state *crtc_state = to_dm_crtc_state(crtc->state);
	struct dc_stream_state *stream_state = crtc_state->stream;
	uint32_t crcs[3];

	/*
	 * Since flipping and crc enablement happen asynchronously, we - more
	 * often than not - will be returning an 'uncooked' crc on first frame.
	 * Probably because hw isn't ready yet. Simply skip the first crc
	 * value.
	 */
	if (!crtc_state->crc_first_skipped) {
		crtc_state->crc_first_skipped = true;
		return;
	}

	if (!dc_stream_get_crc(stream_state->ctx->dc, stream_state,
			       &crcs[0], &crcs[1], &crcs[2]))
		return;

	drm_crtc_add_crc_entry(crtc, true,
			       drm_crtc_accurate_vblank_count(crtc), crcs);
}
