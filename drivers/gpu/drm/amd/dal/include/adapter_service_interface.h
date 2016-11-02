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

#ifndef __DAL_ADAPTER_SERVICE_INTERFACE_H__
#define __DAL_ADAPTER_SERVICE_INTERFACE_H__

#include "grph_object_ctrl_defs.h"
#include "gpio_interface.h"
#include "bios_parser_interface.h"
#include "adapter_service_types.h"
#include "dal_types.h"
#include "asic_capability_types.h"

#define SIZEOF_BACKLIGHT_LUT 101

/* forward declaration */
struct i2caux;
struct asic_cap;

/*
 * enum adapter_feature_id
 *
 * Definition of all adapter features
 *
 * The enumeration defines the IDs of all the adapter features. The enum
 * organizes all the features into several feature sets. The range of feature
 * set N is from ((N-1)*32+1) to (N*32). Because there may be three value-type
 * feature, boolean-type, unsigned char-type and unsinged int-type, the number
 * of features should be 32, 4 and 1 in the feature set accordingly.
 *
 * In a boolean-type feature set N, the enumeration value of the feature should
 * be ((N-1)*32+1), ((N-1)*32+2), ..., (N*32).
 *
 * In an unsigned char-type feature set N, the enumeration value of the
 * feature should be ((N-1)*32+1), ((N-1)*32+8), ((N-1)*32+16) and (N*32).
 *
 * In an unsigned int-type feature set N, the enumeration value of the feature
 * should be ((N-1)*32+1)
 */
enum adapter_feature_id {
	FEATURE_UNKNOWN = 0,

	/* Boolean set, up to 32 entries */
	FEATURE_ENABLE_HW_EDID_POLLING = 1,
	FEATURE_SET_01_START = FEATURE_ENABLE_HW_EDID_POLLING,
	FEATURE_DP_SINK_DETECT_POLL_DATA_PIN,
	FEATURE_UNDERFLOW_INTERRUPT,
	FEATURE_ALLOW_WATERMARK_ADJUSTMENT,
	FEATURE_DCP_DITHER_FRAME_RANDOM_ENABLE,
	FEATURE_DCP_DITHER_RGB_RANDOM_ENABLE,
	FEATURE_DCP_DITHER_HIGH_PASS_RANDOM_ENABLE,
	FEATURE_DETECT_REQUIRE_HPD_HIGH,
	FEATURE_LINE_BUFFER_ENHANCED_PIXEL_DEPTH, /* 10th */
	FEATURE_MAXIMIZE_URGENCY_WATERMARKS,
	FEATURE_MAXIMIZE_STUTTER_MARKS,
	FEATURE_MAXIMIZE_NBP_MARKS,
	FEATURE_USE_MAX_DISPLAY_CLK,
	FEATURE_ALLOW_EDP_RESOURCE_SHARING,
	FEATURE_SUPPORT_DP_YUV,
	FEATURE_SUPPORT_DP_Y_ONLY,
	FEATURE_ENABLE_DFS_BYPASS,
	FEATURE_LB_HIGH_RESOLUTION,
	FEATURE_DP_DISPLAY_FORCE_SS_ENABLE,
	FEATURE_REPORT_CE_MODE_ONLY,
	FEATURE_ALLOW_OPTIMIZED_MODE_AS_DEFAULT,
	FEATURE_FORCE_TIMING_RESYNC,
	FEATURE_TMDS_DISABLE_DITHERING,
	FEATURE_HDMI_DISABLE_DITHERING,
	FEATURE_DP_DISABLE_DITHERING, /* 30th */
	FEATURE_EMBEDDED_DISABLE_DITHERING,
	FEATURE_DISABLE_AZ_CLOCK_GATING, /* 32nd. This set is full */
	FEATURE_SET_01_END = FEATURE_SET_01_START + 31,

	/* Boolean set, up to 32 entries */
	FEATURE_WIRELESS_ENABLE = FEATURE_SET_01_END + 1,
	FEATURE_SET_02_START = FEATURE_WIRELESS_ENABLE,
	FEATURE_WIRELESS_FULL_TIMING_ADJUSTMENT,
	FEATURE_WIRELESS_LIMIT_720P,
	FEATURE_WIRELESS_ENABLE_COMPRESSED_AUDIO,
	FEATURE_WIRELESS_INCLUDE_UNVERIFIED_TIMINGS,
	FEATURE_MODIFY_TIMINGS_FOR_WIRELESS,
	FEATURE_ALLOW_SELF_REFRESH,
	FEATURE_ALLOW_DYNAMIC_PIXEL_ENCODING_CHANGE,
	FEATURE_ALLOW_HSYNC_VSYNC_ADJUSTMENT,
	FEATURE_FORCE_PSR, /* 10th */
	FEATURE_PREFER_3D_TIMING,
	FEATURE_VARI_BRIGHT_ENABLE,
	FEATURE_PSR_ENABLE,
	FEATURE_DP_FRAME_PACK_STEREO3D,
	FEATURE_ALLOW_HDMI_WITHOUT_AUDIO,
	FEATURE_RESTORE_USAGE_I2C_SW_ENGING,
	FEATURE_ABM_2_0,
	FEATURE_SUPPORT_MIRABILIS,
	FEATURE_LOAD_DMCU_FIRMWARE, /* 20th */
	FEATURE_DONGLE_SINK_COUNT_CHECK,
	FEATURE_INSTANT_UP_SCALE_DOWN_SCALE,
	FEATURE_TILED_DISPLAY,
	FEATURE_CHANGE_I2C_SPEED_CONTROL,
	FEATURE_REPORT_SINGLE_SELECTED_TIMING,
	FEATURE_ALLOW_HDMI_HIGH_CLK_DP_DONGLE,
	FEATURE_SUPPORT_EXTERNAL_PANEL_DRR,
	FEATURE_SUPPORT_SMOOTH_BRIGHTNESS,
	FEATURE_ALLOW_DIRECT_MEMORY_ACCESS_TRIG, /* 30th */
	FEATURE_POWER_GATING_LB_PORTION,
	FEATURE_SET_02_END = FEATURE_SET_02_START + 31,

	/* UInt set, 1 entry: DCP Bit Depth Reduction Mode */
	FEATURE_DCP_BIT_DEPTH_REDUCTION_MODE = FEATURE_SET_02_END + 1,
	FEATURE_SET_03_START = FEATURE_DCP_BIT_DEPTH_REDUCTION_MODE,
	FEATURE_SET_03_END = FEATURE_SET_03_START + 31,

	/* UInt set, 1 entry: DCP Dither Mode */
	FEATURE_DCP_DITHER_MODE = FEATURE_SET_03_END + 1,
	FEATURE_SET_04_START = FEATURE_DCP_DITHER_MODE,
	FEATURE_SET_04_END = FEATURE_SET_04_START + 31,

	/* UInt set, 1 entry: DCP Programming WA(workaround) */
	FEATURE_DCP_PROGRAMMING_WA = FEATURE_SET_04_END + 1,
	FEATURE_SET_06_START = FEATURE_DCP_PROGRAMMING_WA,
	FEATURE_SET_06_END = FEATURE_SET_06_START + 31,

	/* UInt set, 1 entry: Maximum number of controllers */
	FEATURE_MAX_CONTROLLER_NUM = FEATURE_SET_06_END + 1,
	FEATURE_SET_09_START = FEATURE_MAX_CONTROLLER_NUM,
	FEATURE_SET_09_END = FEATURE_SET_09_START + 31,

	/* UInt set, 1 entry: Type of DRR support */
	FEATURE_DRR_SUPPORT = FEATURE_SET_09_END + 1,
	FEATURE_SET_10_START = FEATURE_DRR_SUPPORT,
	FEATURE_SET_10_END = FEATURE_SET_10_START + 31,

	/* UInt set, 1 entry: Stutter mode support */
	FEATURE_STUTTER_MODE = FEATURE_SET_10_END + 1,
	FEATURE_SET_11_START = FEATURE_STUTTER_MODE,
	FEATURE_SET_11_END = FEATURE_SET_11_START + 31,

	/* UInt set, 1 entry: Measure PSR setup time */
	FEATURE_PSR_SETUP_TIME_TEST = FEATURE_SET_11_END + 1,
	FEATURE_SET_12_START = FEATURE_PSR_SETUP_TIME_TEST,
	FEATURE_SET_12_END = FEATURE_SET_12_START + 31,

	/* Boolean set, up to 32 entries */
	FEATURE_POWER_GATING_PIPE_IN_TILE = FEATURE_SET_12_END + 1,
	FEATURE_SET_13_START = FEATURE_POWER_GATING_PIPE_IN_TILE,
	FEATURE_USE_PPLIB,
	FEATURE_DPMS_AUDIO_ENDPOINT_CONTROL,
	FEATURE_PIXEL_PERFECT_OUTPUT,
	FEATURE_8BPP_SUPPORTED,
	FEATURE_SET_13_END = FEATURE_SET_13_START + 31,

	/* UInt set, 1 entry: Display preferred view
	 * 0: no preferred view
	 * 1: native and preferred timing of embedded display will have high
	 *    priority, so other displays will support it always
	 */
	FEATURE_DISPLAY_PREFERRED_VIEW = FEATURE_SET_13_END + 1,
	FEATURE_SET_15_START = FEATURE_DISPLAY_PREFERRED_VIEW,
	FEATURE_SET_15_END = FEATURE_SET_15_START + 31,

	/* UInt set, 1 entry: DAL optimization */
	FEATURE_OPTIMIZATION = FEATURE_SET_15_END + 1,
	FEATURE_SET_16_START = FEATURE_OPTIMIZATION,
	FEATURE_SET_16_END = FEATURE_SET_16_START + 31,

	/* UInt set, 1 entry: Performance measurement */
	FEATURE_PERF_MEASURE = FEATURE_SET_16_END + 1,
	FEATURE_SET_17_START = FEATURE_PERF_MEASURE,
	FEATURE_SET_17_END = FEATURE_SET_17_START + 31,

	/* UInt set, 1 entry: Minimum backlight value [0-255] */
	FEATURE_MIN_BACKLIGHT_LEVEL = FEATURE_SET_17_END + 1,
	FEATURE_SET_18_START = FEATURE_MIN_BACKLIGHT_LEVEL,
	FEATURE_SET_18_END = FEATURE_SET_18_START + 31,

	/* UInt set, 1 entry: Maximum backlight value [0-255] */
	FEATURE_MAX_BACKLIGHT_LEVEL = FEATURE_SET_18_END + 1,
	FEATURE_SET_19_START = FEATURE_MAX_BACKLIGHT_LEVEL,
	FEATURE_SET_19_END = FEATURE_SET_19_START + 31,

	/* UInt set, 1 entry: AMB setting
	 *
	 * Each byte will control the ABM configuration to use for a specific
	 * ABM level.
	 *
	 * HW team provided 12 different ABM min/max reduction pairs to choose
	 * between for each ABM level.
	 *
	 * ABM level Byte Setting
	 *       1    0   Default = 0 (setting 3), can be override to 1-12
	 *       2    1   Default = 0 (setting 7), can be override to 1-12
	 *       3    2   Default = 0 (setting 8), can be override to 1-12
	 *       4    3   Default = 0 (setting 10), can be override to 1-12
	 *
	 * For example,
	 * FEATURE_PREFERRED_ABM_CONFIG_SET = 0x0C060500, this represents:
	 * ABM level 1 use default setting (setting 3)
	 * ABM level 2 uses setting 5
	 * ABM level 3 uses setting 6
	 * ABM level 4 uses setting 12
	 * Internal use only!
	 */
	FEATURE_PREFERRED_ABM_CONFIG_SET = FEATURE_SET_19_END + 1,
	FEATURE_SET_20_START = FEATURE_PREFERRED_ABM_CONFIG_SET,
	FEATURE_SET_20_END = FEATURE_SET_20_START + 31,

	/* UInt set, 1 entry: Change SW I2C speed */
	FEATURE_CHANGE_SW_I2C_SPEED = FEATURE_SET_20_END + 1,
	FEATURE_SET_21_START = FEATURE_CHANGE_SW_I2C_SPEED,
	FEATURE_SET_21_END = FEATURE_SET_21_START + 31,

	/* UInt set, 1 entry: Change HW I2C speed */
	FEATURE_CHANGE_HW_I2C_SPEED = FEATURE_SET_21_END + 1,
	FEATURE_SET_22_START = FEATURE_CHANGE_HW_I2C_SPEED,
	FEATURE_SET_22_END = FEATURE_SET_22_START + 31,

	/* UInt set, 1 entry:
	 * When PSR issue occurs, it is sometimes hard to debug since the
	 * failure occurs immediately at boot. Use this setting to skip or
	 * postpone PSR functionality and re-enable through DSAT. */
	FEATURE_DEFAULT_PSR_LEVEL = FEATURE_SET_22_END + 1,
	FEATURE_SET_23_START = FEATURE_DEFAULT_PSR_LEVEL,
	FEATURE_SET_23_END = FEATURE_SET_23_START + 31,

	/* UInt set, 1 entry: Allowed pixel clock range for LVDS */
	FEATURE_LVDS_SAFE_PIXEL_CLOCK_RANGE = FEATURE_SET_23_END + 1,
	FEATURE_SET_24_START = FEATURE_LVDS_SAFE_PIXEL_CLOCK_RANGE,
	FEATURE_SET_24_END = FEATURE_SET_24_START + 31,

	/* UInt set, 1 entry: Max number of clock sources */
	FEATURE_MAX_CLOCK_SOURCE_NUM = FEATURE_SET_24_END + 1,
	FEATURE_SET_25_START = FEATURE_MAX_CLOCK_SOURCE_NUM,
	FEATURE_SET_25_END = FEATURE_SET_25_START + 31,

	/* UInt set, 1 entry: Select the ABM configuration to use.
	 *
	 * This feature set is used to allow packaging option to be defined
	 * to allow OEM to select between the default ABM configuration or
	 * alternative predefined configurations that may be more aggressive.
	 *
	 * Note that this regkey is meant for external use to select the
	 * configuration OEM wants. Whereas the other PREFERRED_ABM_CONFIG_SET
	 * key is only used for internal use and allows full reconfiguration.
	 */
	FEATURE_ABM_CONFIG = FEATURE_SET_25_END + 1,
	FEATURE_SET_26_START = FEATURE_ABM_CONFIG,
	FEATURE_SET_26_END = FEATURE_SET_26_START + 31,

	/* UInt set, 1 entry: Select the default speed in which smooth
	 * brightness feature should converge towards target backlight level.
	 *
	 * For example, a setting of 500 means it takes 500ms to transition
	 * from current backlight level to the new requested backlight level.
	 */
	FEATURE_SMOOTH_BRTN_ADJ_TIME_IN_MS = FEATURE_SET_26_END + 1,
	FEATURE_SET_27_START = FEATURE_SMOOTH_BRTN_ADJ_TIME_IN_MS,
	FEATURE_SET_27_END = FEATURE_SET_27_START + 31,

	/* Set 28: UInt set, 1 entry: Allow runtime parameter to force specific
	 * Static Screen Event triggers for test purposes. */
	FEATURE_FORCE_STATIC_SCREEN_EVENT_TRIGGERS = FEATURE_SET_27_END + 1,
	FEATURE_SET_28_START = FEATURE_FORCE_STATIC_SCREEN_EVENT_TRIGGERS,
	FEATURE_SET_28_END = FEATURE_SET_28_START + 31,

	FEATURE_MAXIMUM
};

/* Adapter service */
struct adapter_service {
	struct dc_context *ctx;
	struct asic_capability *asic_cap;
	enum dce_environment dce_environment;
	uint32_t platform_methods_mask;
	uint32_t ac_level_percentage;
	uint32_t dc_level_percentage;
	uint32_t backlight_caps_initialized;
	uint32_t backlight_8bit_lut[SIZEOF_BACKLIGHT_LUT];
	uint32_t adapter_feature_set[FEATURE_MAXIMUM/32];
	uint32_t default_values[FEATURE_MAXIMUM];
};

/* Adapter Service type of DRR support*/
enum as_drr_support {
	AS_DRR_SUPPORT_DISABLED = 0x0,
	AS_DRR_SUPPORT_ENABLED = 0x1,
	AS_DRR_SUPPORT_MIN_FORCED_FPS = 0xA
};

/* Adapter service initialize data structure*/
struct as_init_data {
	struct hw_asic_id hw_init_data;
	struct dc_context *ctx;
	const struct dal_override_parameters *display_param;
	struct dc_bios *vbios_override;
	enum dce_environment dce_environment;
};

/* Create adapter service */
struct adapter_service *dal_adapter_service_create(
	struct as_init_data *init_data);

/* Destroy adapter service and objects it contains */
void dal_adapter_service_destroy(
	struct adapter_service **as);

/* Check if DFS bypass is enabled */
bool dal_adapter_service_is_dfs_bypass_enabled(struct adapter_service *as);

/* Return if a given feature is supported by the ASIC */
bool dal_adapter_service_is_feature_supported(struct adapter_service *as,
	enum adapter_feature_id feature_id);

/* Get the cached value of a given feature */
bool dal_adapter_service_get_feature_value(struct adapter_service *as,
	const enum adapter_feature_id feature_id,
	void *data,
	uint32_t size);

struct dal_asic_runtime_flags dal_adapter_service_get_asic_runtime_flags(
	struct adapter_service *as);

/* Reports whether driver settings allow requested optimization */
bool dal_adapter_service_should_optimize(
		struct adapter_service *as, enum optimization_feature feature);

#endif /* __DAL_ADAPTER_SERVICE_INTERFACE_H__ */
