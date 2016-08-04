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

#ifndef __AUDIO_TYPES_H__
#define __AUDIO_TYPES_H__

#include "grph_object_defs.h"
#include "signal_types.h"

#define AUDIO_INFO_DISPLAY_NAME_SIZE_IN_CHARS 20
#define MAX_HW_AUDIO_INFO_DISPLAY_NAME_SIZE_IN_CHARS 18
#define MULTI_CHANNEL_SPLIT_NO_ASSO_INFO 0xFFFFFFFF

struct audio_pll_hw_settings {
	uint32_t feed_back_divider;
	uint32_t step_size_integer;
	uint32_t step_size_fraction;
	uint32_t step_range;
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

struct azalia_clock_info {
	uint32_t pixel_clock_in_10khz;
	uint32_t audio_dto_phase;
	uint32_t audio_dto_module;
	uint32_t audio_dto_wall_clock_ratio;
};

enum audio_dto_source {
	DTO_SOURCE_UNKNOWN = 0,
	DTO_SOURCE_ID0,
	DTO_SOURCE_ID1,
	DTO_SOURCE_ID2,
	DTO_SOURCE_ID3,
	DTO_SOURCE_ID4,
	DTO_SOURCE_ID5
};

union audio_sample_rates {
	struct sample_rates {
		uint8_t RATE_32:1;
		uint8_t RATE_44_1:1;
		uint8_t RATE_48:1;
		uint8_t RATE_88_2:1;
		uint8_t RATE_96:1;
		uint8_t RATE_176_4:1;
		uint8_t RATE_192:1;
	} rate;

	uint8_t all;
};

enum audio_format_code {
	AUDIO_FORMAT_CODE_FIRST = 1,
	AUDIO_FORMAT_CODE_LINEARPCM = AUDIO_FORMAT_CODE_FIRST,

	AUDIO_FORMAT_CODE_AC3,
	/*Layers 1 & 2 */
	AUDIO_FORMAT_CODE_MPEG1,
	/*MPEG1 Layer 3 */
	AUDIO_FORMAT_CODE_MP3,
	/*multichannel */
	AUDIO_FORMAT_CODE_MPEG2,
	AUDIO_FORMAT_CODE_AAC,
	AUDIO_FORMAT_CODE_DTS,
	AUDIO_FORMAT_CODE_ATRAC,
	AUDIO_FORMAT_CODE_1BITAUDIO,
	AUDIO_FORMAT_CODE_DOLBYDIGITALPLUS,
	AUDIO_FORMAT_CODE_DTS_HD,
	AUDIO_FORMAT_CODE_MAT_MLP,
	AUDIO_FORMAT_CODE_DST,
	AUDIO_FORMAT_CODE_WMAPRO,
	AUDIO_FORMAT_CODE_LAST,
	AUDIO_FORMAT_CODE_COUNT =
		AUDIO_FORMAT_CODE_LAST - AUDIO_FORMAT_CODE_FIRST
};

struct audio_mode {
	 /* ucData[0] [6:3] */
	enum audio_format_code format_code;
	/* ucData[0] [2:0] */
	uint8_t channel_count;
	/* ucData[1] */
	union audio_sample_rates sample_rates;
	union {
		/* for LPCM */
		uint8_t sample_size;
		/* for Audio Formats 2-8 (Max bit rate divided by 8 kHz) */
		uint8_t max_bit_rate;
		/* for Audio Formats 9-15 */
		uint8_t vendor_specific;
	};
};

struct audio_speaker_flags {
    uint32_t FL_FR:1;
    uint32_t LFE:1;
    uint32_t FC:1;
    uint32_t RL_RR:1;
    uint32_t RC:1;
    uint32_t FLC_FRC:1;
    uint32_t RLC_RRC:1;
    uint32_t SUPPORT_AI:1;
};

struct audio_speaker_info {
    uint32_t ALLSPEAKERS:7;
    uint32_t SUPPORT_AI:1;
};

struct audio_info_flags {

	union {

		struct audio_speaker_flags speaker_flags;
		struct audio_speaker_info   info;

		uint8_t all;
	};
};

/*struct audio_info_flags {
	struct audio_speaker_flags {
		uint32_t FL_FR:1;
		uint32_t LFE:1;
		uint32_t FC:1;
		uint32_t RL_RR:1;
		uint32_t RC:1;
		uint32_t FLC_FRC:1;
		uint32_t RLC_RRC:1;
		uint32_t SUPPORT_AI:1;
	};

	struct audio_speaker_info {
		uint32_t ALLSPEAKERS:7;
		uint32_t SUPPORT_AI:1;
	};

	union {
		struct audio_speaker_flags speaker_flags;
		struct audio_speaker_info info;
	};
};
*/

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

struct audio_info {
	struct audio_info_flags flags;
	uint32_t video_latency;
	uint32_t audio_latency;
	uint32_t display_index;
	uint8_t display_name[AUDIO_INFO_DISPLAY_NAME_SIZE_IN_CHARS];
	uint32_t manufacture_id;
	uint32_t product_id;
	/* PortID used for ContainerID when defined */
	uint32_t port_id[2];
	uint32_t mode_count;
	/* this field must be last in this struct */
	struct audio_mode modes[DC_MAX_AUDIO_DESC_COUNT];
};

struct audio_crtc_info {
	uint32_t h_total;
	uint32_t h_active;
	uint32_t v_active;
	uint32_t pixel_repetition;
	uint32_t requested_pixel_clock; /* in KHz */
	uint32_t calculated_pixel_clock; /* in KHz */
	uint32_t refresh_rate;
	enum dc_color_depth color_depth;
	bool interlaced;
};

/* PLL information required for AZALIA DTO calculation */

struct audio_pll_info {
	uint32_t dp_dto_source_clock_in_khz;
	uint32_t feed_back_divider;
	enum audio_dto_source dto_source;
	bool ss_enabled;
	uint32_t ss_percentage;
	uint32_t ss_percentage_divider;
};

struct audio_channel_associate_info {
	union {
		struct {
			uint32_t ALL_CHANNEL_FL:4;
			uint32_t ALL_CHANNEL_FR:4;
			uint32_t ALL_CHANNEL_FC:4;
			uint32_t ALL_CHANNEL_Sub:4;
			uint32_t ALL_CHANNEL_SL:4;
			uint32_t ALL_CHANNEL_SR:4;
			uint32_t ALL_CHANNEL_BL:4;
			uint32_t ALL_CHANNEL_BR:4;
		} bits;
		uint32_t u32all;
	};
};

struct audio_output {
	/* Front DIG id. */
	enum engine_id engine_id;
	/* encoder output signal */
	enum signal_type signal;
	/* video timing */
	struct audio_crtc_info crtc_info;
	/* PLL for audio */
	struct audio_pll_info pll_info;
};

struct audio_feature_support {
	/* supported engines*/
	uint32_t ENGINE_DIGA:1;
	uint32_t ENGINE_DIGB:1;
	uint32_t ENGINE_DIGC:1;
	uint32_t ENGINE_DIGD:1;
	uint32_t ENGINE_DIGE:1;
	uint32_t ENGINE_DIGF:1;
	uint32_t ENGINE_DIGG:1;
	uint32_t MULTISTREAM_AUDIO:1;
};

enum audio_payload {
	CHANNEL_SPLIT_MAPPINGCHANG = 0x9,
};

#endif /* __AUDIO_TYPES_H__ */
