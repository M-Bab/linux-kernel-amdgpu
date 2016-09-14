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

#ifndef __DAL_HW_CTX_AUDIO_H__
#define __DAL_HW_CTX_AUDIO_H__

#include "include/audio_interface.h"
#include "include/link_service_types.h"

struct hw_ctx_audio;

struct azalia_reg_offsets {
	uint32_t azf0endpointx_azalia_f0_codec_endpoint_index;
	uint32_t azf0endpointx_azalia_f0_codec_endpoint_data;
};

/***** hook functions *****/

struct hw_ctx_audio_funcs {

	/* functions for hw_ctx creation */
	void (*destroy)(
		struct hw_ctx_audio **ptr);

	/***** from dal2 hwcontextaudio.hpp *****/

	void (*setup_audio_wall_dto)(
		const struct hw_ctx_audio *hw_ctx,
		enum signal_type signal,
		const struct audio_crtc_info *crtc_info,
		const struct audio_pll_info *pll_info);

	/* MM register access  read_register  write_register */

	/* initialize HW state */
	void (*hw_initialize)(
		const struct hw_ctx_audio *hw_ctx);

	/* check_audio_bandwidth */

	/* ~~~~  protected: ~~~~*/

	/* calc_max_audio_packets_per_line */
	/* speakers_to_channels */
	/* is_audio_format_supported */
	/* get_audio_clock_info */

	/* search pixel clock value for Azalia HDMI Audio */
	bool (*get_azalia_clock_info_hdmi)(
		const struct hw_ctx_audio *hw_ctx,
		uint32_t crtc_pixel_clock_in_khz,
		uint32_t actual_pixel_clock_in_khz,
		struct azalia_clock_info *azalia_clock_info);

	/* search pixel clock value for Azalia DP Audio */
	bool (*get_azalia_clock_info_dp)(
		const struct hw_ctx_audio *hw_ctx,
		uint32_t requested_pixel_clock_in_khz,
		const struct audio_pll_info *pll_info,
		struct azalia_clock_info *azalia_clock_info);

	/* @@@@   private:  @@@@  */

	/* check_audio_bandwidth_hdmi  */
	/* check_audio_bandwidth_dpsst */
	/* check_audio_bandwidth_dpmst */

};

struct hw_ctx_audio {
	const struct hw_ctx_audio_funcs *funcs;
	struct dc_context *ctx;

	/*audio_clock_infoTable[12];
	 *audio_clock_infoTable_36bpc[12];
	 *audio_clock_infoTable_48bpc[12];
	 *used by hw_ctx_audio.c file only. Will declare as static array
	 *azaliaclockinfoTable[12]  -- not used
	 *BusNumberMask;   BusNumberShift; DeviceNumberMask;
	 *not used by dce6 and after
	 */
};

/* --- object construct, destruct --- */

/*
 *called by derived audio object for specific ASIC. In case no derived object,
 *these two functions do not need exposed.
 */
bool dal_audio_construct_hw_ctx_audio(
	struct hw_ctx_audio *hw_ctx);


#endif  /* __DAL_HW_CTX_AUDIO_H__ */

