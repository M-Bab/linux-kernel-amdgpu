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

#ifndef __DAL_HW_CTX_ADAPTER_SERVICE_H__
#define __DAL_HW_CTX_ADAPTER_SERVICE_H__

enum audio_straps {
	AUDIO_STRAPS_NOT_ALLOWED = 0,
	AUDIO_STRAPS_DP_AUDIO_ALLOWED,
	AUDIO_STRAPS_DP_HDMI_AUDIO_ON_DONGLE,
	AUDIO_STRAPS_DP_HDMI_AUDIO
};

struct hw_ctx_adapter_service;

struct hw_ctx_adapter_service_funcs {
	void (*destroy)(
		struct hw_ctx_adapter_service *hw_ctx);
	/* Initializes relevant HW registers
	 * and caches relevant data from HW registers */
	bool (*power_up)(
		struct hw_ctx_adapter_service *hw_ctx);
	/* Enumerate audio objects */
	struct graphics_object_id (*enum_audio_object)(
		const struct hw_ctx_adapter_service *hw_ctx,
		uint32_t index);
};

struct hw_ctx_adapter_service {
	struct dc_context *ctx;
	const struct hw_ctx_adapter_service_funcs *funcs;
	enum audio_straps cached_audio_straps;
};

bool dal_adapter_service_construct_hw_ctx(
	struct hw_ctx_adapter_service *hw_ctx,
	struct dc_context *ctx);

union audio_support dal_adapter_service_hw_ctx_get_audio_support(
	const struct hw_ctx_adapter_service *hw_ctx);

void dal_adapter_service_destruct_hw_ctx(
	struct hw_ctx_adapter_service *hw_ctx);

void dal_adapter_service_destroy_hw_ctx(
	struct hw_ctx_adapter_service **ptr);

#endif /* __DAL_HW_CTX_ADAPTER_SERVICE_H__ */
