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

#include "dm_services.h"

#include "dc_bios_types.h"

#include "include/adapter_service_interface.h"
#include "include/i2caux_interface.h"
#include "include/asic_capability_types.h"
#include "include/gpio_service_interface.h"
#include "include/asic_capability_interface.h"
#include "include/logger_interface.h"

#include "adapter_service.h"

#include "atom.h"

#define ABSOLUTE_BACKLIGHT_MAX 255
#define DEFAULT_MIN_BACKLIGHT 12
#define DEFAULT_MAX_BACKLIGHT 255
#define BACKLIGHT_CURVE_COEFFB 100
#define BACKLIGHT_CURVE_COEFFA_FACTOR 10000
#define BACKLIGHT_CURVE_COEFFB_FACTOR 100

/*
 * Adapter service feature entry table.
 *
 * This is an array of features that is used to generate feature set. Each
 * entry consists three element:
 *
 * Feature name, default value, and if this feature is a boolean type. A
 * feature can only be a boolean or int type.
 *
 * Example 1: a boolean type feature
 * FEATURE_ENABLE_HW_EDID_POLLING, false, true
 *
 * First element is feature name: EATURE_ENABLE_HW_EDID_POLLING, it has a
 * default value 0, and it is a boolean feature.
 *
 * Example 2: an int type feature
 * FEATURE_DCP_PROGRAMMING_WA, 0x1FF7, false
 *
 * In this case, the default value is 0x1FF7 and not a boolean type, which
 * makes it an int type.
 */
/* Type of feature with its runtime parameter and default value */
struct feature_source_entry {
	enum adapter_feature_id feature_id;
	uint32_t default_value;
	bool is_boolean_type;
};

static const struct feature_source_entry feature_entry_table[] = {
	/* Feature name | default value | is boolean type */
	{FEATURE_ENABLE_HW_EDID_POLLING, false, true},
	{FEATURE_DP_SINK_DETECT_POLL_DATA_PIN, false, true},
	{FEATURE_UNDERFLOW_INTERRUPT, false, true},
	{FEATURE_ALLOW_WATERMARK_ADJUSTMENT, false, true},
	{FEATURE_DCP_DITHER_FRAME_RANDOM_ENABLE, false, true},
	{FEATURE_DCP_DITHER_RGB_RANDOM_ENABLE, false, true},
	{FEATURE_DCP_DITHER_HIGH_PASS_RANDOM_ENABLE, false, true},
	{FEATURE_LINE_BUFFER_ENHANCED_PIXEL_DEPTH, false, true},
	{FEATURE_MAXIMIZE_URGENCY_WATERMARKS, false, true},
	{FEATURE_MAXIMIZE_STUTTER_MARKS, false, true},
	{FEATURE_MAXIMIZE_NBP_MARKS, false, true},
	{FEATURE_USE_MAX_DISPLAY_CLK, false, true},
	{FEATURE_ALLOW_EDP_RESOURCE_SHARING, false, true},
	{FEATURE_SUPPORT_DP_YUV, false, true},
	{FEATURE_SUPPORT_DP_Y_ONLY, false, true},
	{FEATURE_MODIFY_TIMINGS_FOR_WIRELESS, false, true},
	{FEATURE_DCP_BIT_DEPTH_REDUCTION_MODE, 0, false},
	{FEATURE_DCP_DITHER_MODE, 0, false},
	{FEATURE_DCP_PROGRAMMING_WA, 0, false},
	{FEATURE_NO_HPD_LOW_POLLING_VCC_OFF, false, true},
	{FEATURE_ENABLE_DFS_BYPASS, false, true},
	{FEATURE_WIRELESS_FULL_TIMING_ADJUSTMENT, false, true},
	{FEATURE_WIRELESS_LIMIT_720P, false, true},
	{FEATURE_MODIFY_TIMINGS_FOR_WIRELESS, false, true},
	{FEATURE_DETECT_REQUIRE_HPD_HIGH, false, true},
	{FEATURE_NO_HPD_LOW_POLLING_VCC_OFF, false, true},
	{FEATURE_LB_HIGH_RESOLUTION, false, true},
	{FEATURE_MAX_CONTROLLER_NUM, 0, false},
	{FEATURE_DRR_SUPPORT, AS_DRR_SUPPORT_ENABLED, false},
	{FEATURE_STUTTER_MODE, 15, false},
	{FEATURE_DP_DISPLAY_FORCE_SS_ENABLE, false, true},
	{FEATURE_REPORT_CE_MODE_ONLY, false, true},
	{FEATURE_ALLOW_OPTIMIZED_MODE_AS_DEFAULT, false, true},
	{FEATURE_FORCE_TIMING_RESYNC, false, true},
	{FEATURE_TMDS_DISABLE_DITHERING, false, true},
	{FEATURE_HDMI_DISABLE_DITHERING, false, true},
	{FEATURE_DP_DISABLE_DITHERING, false, true},
	{FEATURE_EMBEDDED_DISABLE_DITHERING, true, true},
	{FEATURE_ALLOW_SELF_REFRESH, false, true},
	{FEATURE_ALLOW_DYNAMIC_PIXEL_ENCODING_CHANGE, false, true},
	{FEATURE_ALLOW_HSYNC_VSYNC_ADJUSTMENT, false, true},
	{FEATURE_FORCE_PSR, false, true},
	{FEATURE_PSR_SETUP_TIME_TEST, 0, false},
	{FEATURE_POWER_GATING_PIPE_IN_TILE, true, true},
	{FEATURE_POWER_GATING_LB_PORTION, true, true},
	{FEATURE_PREFER_3D_TIMING, false, true},
	{FEATURE_VARI_BRIGHT_ENABLE, true, true},
	{FEATURE_PSR_ENABLE, false, true},
	{FEATURE_WIRELESS_ENABLE_COMPRESSED_AUDIO, false, true},
	{FEATURE_WIRELESS_INCLUDE_UNVERIFIED_TIMINGS, true, true},
	{FEATURE_DP_FRAME_PACK_STEREO3D, false, true},
	{FEATURE_DISPLAY_PREFERRED_VIEW, 0, false},
	{FEATURE_ALLOW_HDMI_WITHOUT_AUDIO, false, true},
	{FEATURE_ABM_2_0, false, true},
	{FEATURE_SUPPORT_MIRABILIS, false, true},
	{FEATURE_OPTIMIZATION, 0xFFFF, false},
	{FEATURE_PERF_MEASURE, 0, false},
	{FEATURE_MIN_BACKLIGHT_LEVEL, 0, false},
	{FEATURE_MAX_BACKLIGHT_LEVEL, 255, false},
	{FEATURE_LOAD_DMCU_FIRMWARE, true, true},
	{FEATURE_DISABLE_AZ_CLOCK_GATING, false, true},
	{FEATURE_DONGLE_SINK_COUNT_CHECK, true, true},
	{FEATURE_INSTANT_UP_SCALE_DOWN_SCALE, false, true},
	{FEATURE_TILED_DISPLAY, false, true},
	{FEATURE_PREFERRED_ABM_CONFIG_SET, 0, false},
	{FEATURE_CHANGE_SW_I2C_SPEED, 50, false},
	{FEATURE_CHANGE_HW_I2C_SPEED, 50, false},
	{FEATURE_CHANGE_I2C_SPEED_CONTROL, false, true},
	{FEATURE_DEFAULT_PSR_LEVEL, 0, false},
	{FEATURE_MAX_CLOCK_SOURCE_NUM, 0, false},
	{FEATURE_REPORT_SINGLE_SELECTED_TIMING, false, true},
	{FEATURE_ALLOW_HDMI_HIGH_CLK_DP_DONGLE, true, true},
	{FEATURE_SUPPORT_EXTERNAL_PANEL_DRR, false, true},
	{FEATURE_LVDS_SAFE_PIXEL_CLOCK_RANGE, 0, false},
	{FEATURE_ABM_CONFIG, 0, false},
	{FEATURE_WIRELESS_ENABLE, false, true},
	{FEATURE_ALLOW_DIRECT_MEMORY_ACCESS_TRIG, false, true},
	{FEATURE_FORCE_STATIC_SCREEN_EVENT_TRIGGERS, 0, false},
	{FEATURE_USE_PPLIB, true, true},
	{FEATURE_DISABLE_LPT_SUPPORT, false, true},
	{FEATURE_DUMMY_FBC_BACKEND, false, true},
	{FEATURE_DPMS_AUDIO_ENDPOINT_CONTROL, true, true},
	{FEATURE_DISABLE_FBC_COMP_CLK_GATE, false, true},
	{FEATURE_PIXEL_PERFECT_OUTPUT, false, true},
	{FEATURE_8BPP_SUPPORTED, false, true},
};

enum {
	LEGACY_MAX_NUM_OF_CONTROLLERS = 2,
	DEFAULT_NUM_COFUNC_NON_DP_DISPLAYS = 2
};

/*
 * get_feature_entries_num
 *
 * Get number of feature entries
 */
static inline uint32_t get_feature_entries_num(void)
{
	return ARRAY_SIZE(feature_entry_table);
}

static void get_platform_info_methods(
		struct adapter_service *as)
{
	struct platform_info_params params;
	uint32_t mask = 0;

	params.data = &mask;
	params.method = PM_GET_AVAILABLE_METHODS;

	if (dm_get_platform_info(as->ctx, &params))
		as->platform_methods_mask = mask;

}

static void initialize_backlight_caps(
		struct adapter_service *as)
{
	struct firmware_info fw_info;
	struct embedded_panel_info panel_info;
	struct platform_info_ext_brightness_caps caps;
	struct platform_info_params params;
	bool custom_curve_present = false;
	bool custom_min_max_present = false;
	struct dc_bios *dcb = as->ctx->dc_bios;

	if (!(PM_GET_EXTENDED_BRIGHNESS_CAPS & as->platform_methods_mask)) {
			dm_logger_write(as->ctx->logger, LOG_BACKLIGHT,
					"This method is not supported\n");
			return;
	}

	if (dcb->funcs->get_firmware_info(dcb, &fw_info) != BP_RESULT_OK ||
		dcb->funcs->get_embedded_panel_info(dcb, &panel_info) != BP_RESULT_OK)
		return;

	params.data = &caps;
	params.method = PM_GET_EXTENDED_BRIGHNESS_CAPS;

	if (dm_get_platform_info(as->ctx, &params)) {
		as->ac_level_percentage = caps.basic_caps.ac_level_percentage;
		as->dc_level_percentage = caps.basic_caps.dc_level_percentage;
		custom_curve_present = (caps.data_points_num > 0);
		custom_min_max_present = true;
	} else
		return;
	/* Choose minimum backlight level base on priority:
	 * extended caps,VBIOS,default */
	if (custom_min_max_present)
		as->backlight_8bit_lut[0] = caps.min_input_signal;

	else if (fw_info.min_allowed_bl_level > 0)
		as->backlight_8bit_lut[0] = fw_info.min_allowed_bl_level;

	else
		as->backlight_8bit_lut[0] = DEFAULT_MIN_BACKLIGHT;

	/* Choose maximum backlight level base on priority:
	 * extended caps,default */
	if (custom_min_max_present)
		as->backlight_8bit_lut[100] = caps.max_input_signal;

	else
		as->backlight_8bit_lut[100] = DEFAULT_MAX_BACKLIGHT;

	if (as->backlight_8bit_lut[100] > ABSOLUTE_BACKLIGHT_MAX)
		as->backlight_8bit_lut[100] = ABSOLUTE_BACKLIGHT_MAX;

	if (as->backlight_8bit_lut[0] > as->backlight_8bit_lut[100])
		as->backlight_8bit_lut[0] = as->backlight_8bit_lut[100];

	if (custom_curve_present) {
		uint16_t index = 1;
		uint16_t i;
		uint16_t num_of_data_points = (caps.data_points_num <= 99 ?
				caps.data_points_num : 99);
		/* Filling translation table from data points -
		 * between every two provided data points we
		 * lineary interpolate missing values
		 */
		for (i = 0 ; i < num_of_data_points; i++) {
			uint16_t luminance = caps.data_points[i].luminance;
			uint16_t signal_level =
					caps.data_points[i].signal_level;

			if (signal_level < as->backlight_8bit_lut[0])
				signal_level = as->backlight_8bit_lut[0];

			if (signal_level > as->backlight_8bit_lut[100])
				signal_level = as->backlight_8bit_lut[100];

			/* Lineary interpolate missing values */
			if (index < luminance) {
				uint16_t base_value =
						as->backlight_8bit_lut[index-1];
				uint16_t delta_signal =
						signal_level - base_value;
				uint16_t delta_luma = luminance - index + 1;
				uint16_t step = delta_signal;

				for (; index < luminance ; index++) {
					as->backlight_8bit_lut[index] =
							base_value +
							(step / delta_luma);
					step += delta_signal;
				}
			}
			/* Now [index == luminance], so we can add
			 * data point to the translation table */
			as->backlight_8bit_lut[index++] = signal_level;
		}
		/* Complete the final segment of interpolation -
		 * between last datapoint and maximum value */
		if (index < 100) {
			uint16_t base_value = as->backlight_8bit_lut[index-1];
			uint16_t delta_signal =
					as->backlight_8bit_lut[100]-base_value;
			uint16_t delta_luma = 100 - index + 1;
			uint16_t step = delta_signal;

			for (; index < 100 ; index++) {
				as->backlight_8bit_lut[index] = base_value +
						(step / delta_luma);
				step += delta_signal;
			}
		}
	}
	/* build backlight translation table based on default curve */
	else {
		/* Default backlight curve can be defined by
		 * polinomial F(x) = A(x*x) + Bx + C.
		 * Backlight curve should always  satisfy
		 * F(0) = min, F(100) = max, so polinomial coefficients are:
		 * A is 0.0255 - B/100 - min/10000 -
		 * (255-max)/10000 = (max - min)/10000 - B/100
		 * B is adjustable factor to modify the curve.
		 * Bigger B results in less concave curve.
		 * B range is [0..(max-min)/100]
		 * C is backlight minimum
		 */
		uint16_t delta = as->backlight_8bit_lut[100] -
				as->backlight_8bit_lut[0];
		uint16_t coeffc = as->backlight_8bit_lut[0];
		uint16_t coeffb = (BACKLIGHT_CURVE_COEFFB < delta ?
				BACKLIGHT_CURVE_COEFFB : delta);
		uint16_t coeffa = delta - coeffb;
		uint16_t i;
		uint32_t temp;

		for (i = 1; i < 100 ; i++) {
			temp = (coeffa * i * i) / BACKLIGHT_CURVE_COEFFA_FACTOR;
			as->backlight_8bit_lut[i] = temp + (coeffb * i) /
					BACKLIGHT_CURVE_COEFFB_FACTOR + coeffc;
		}
	}
	as->backlight_caps_initialized = true;
}
/*
 * get_feature_value_from_data_sources
 *
 * For a given feature, determine its value from ASIC cap and wireless
 * data source.
 * idx : index of feature_entry_table for the feature id.
 */
static bool get_feature_value_from_data_sources(
		const struct adapter_service *as,
		const uint32_t idx,
		uint32_t *data)
{
	if (idx >= get_feature_entries_num()) {
		ASSERT_CRITICAL(false);
		return false;
	}

	switch (feature_entry_table[idx].feature_id) {
	case FEATURE_WIRELESS_LIMIT_720P:
		*data = as->asic_cap->caps.WIRELESS_LIMIT_TO_720P;
		break;

	case FEATURE_WIRELESS_FULL_TIMING_ADJUSTMENT:
		*data = as->asic_cap->caps.WIRELESS_FULL_TIMING_ADJUSTMENT;
		break;

	case FEATURE_MODIFY_TIMINGS_FOR_WIRELESS:
		*data = as->asic_cap->caps.WIRELESS_TIMING_ADJUSTMENT;
		break;

	case FEATURE_DETECT_REQUIRE_HPD_HIGH:
		*data = as->asic_cap->caps.HPD_CHECK_FOR_EDID;
		break;

	case FEATURE_NO_HPD_LOW_POLLING_VCC_OFF:
		*data = as->asic_cap->caps.NO_VCC_OFF_HPD_POLLING;
		break;

	case FEATURE_STUTTER_MODE:
		*data = as->asic_cap->data[ASIC_DATA_STUTTERMODE];
		break;

	case FEATURE_8BPP_SUPPORTED:
		*data = as->asic_cap->caps.SUPPORT_8BPP;
		break;

	default:
		return false;
	}

	return true;
}

/* get_bool_value
 *
 * Get the boolean value of a given feature
 */
static bool get_bool_value(
	const uint32_t set,
	const uint32_t idx)
{
	if (idx >= 32) {
		ASSERT_CRITICAL(false);
		return false;
	}

	return ((set & (1 << idx)) != 0);
}

/*
 * lookup_feature_entry
 *
 * Find the entry index of a given feature in feature table
 */
static uint32_t lookup_feature_entry(struct adapter_service *as,
				     enum adapter_feature_id feature_id)
{
	uint32_t entries_num = get_feature_entries_num();
	uint32_t i = 0;

	while (i != entries_num) {
		if (feature_entry_table[i].feature_id == feature_id)
			break;

		++i;
	}

	return i;
}

/*
 * set_bool_value
 *
 * Set the boolean value of a given feature
 */
static void set_bool_value(
	uint32_t *set,
	const uint32_t idx,
	bool value)
{
	if (idx >= 32) {
		ASSERT_CRITICAL(false);
		return;
	}

	if (value)
		*set |= (1 << idx);
	else
		*set &= ~(1 << idx);
}

/*
 * generate_feature_set
 *
 * Generate the internal feature set from multiple data sources
 */
static bool generate_feature_set(
		struct adapter_service *as)
{
	uint32_t i = 0;
	uint32_t value = 0;
	uint32_t set_idx = 0;
	uint32_t internal_idx = 0;
	uint32_t entry_num = 0;
	const struct feature_source_entry *entry = NULL;

	memset(as->adapter_feature_set, 0, sizeof(as->adapter_feature_set));
	entry_num = get_feature_entries_num();

	while (i != entry_num) {
		entry = &feature_entry_table[i];

		if (entry->feature_id <= FEATURE_UNKNOWN ||
				entry->feature_id >= FEATURE_MAXIMUM) {
			ASSERT_CRITICAL(false);
			return false;
		}

		set_idx = (uint32_t)((entry->feature_id - 1) / 32);
		internal_idx = (uint32_t)((entry->feature_id - 1) % 32);

		if (!get_feature_value_from_data_sources(
				as, i, &value)) {
			/*
			 * Can't find feature values from
			 * above data sources
			 * Assign default value
			 */
			value = as->default_values[entry->feature_id];
		}

		if (entry->is_boolean_type)
			set_bool_value(&as->adapter_feature_set[set_idx],
					internal_idx,
					value != 0);
		else
			as->adapter_feature_set[set_idx] = value;

		i++;
	}

	return true;
}

/*
 * adapter_service_destruct
 *
 * Release memory of objects in adapter service
 */
static void adapter_service_destruct(
	struct adapter_service *as)
{
	dal_asic_capability_destroy(&as->asic_cap);
}

/*
 * adapter_service_construct
 *
 * Construct the derived type of adapter service
 */
static bool adapter_service_construct(
	struct adapter_service *as,
	struct as_init_data *init_data)
{
	struct dc_bios *dcb;
	uint32_t i;

	if (!init_data)
		return false;

	/* Create ASIC capability */
	as->ctx = init_data->ctx;
	as->asic_cap = dal_asic_capability_create(
			&init_data->hw_init_data, as->ctx);

	if (!as->asic_cap) {
		ASSERT_CRITICAL(false);
		return false;
	}

	for (i = 0; i < ARRAY_SIZE(feature_entry_table); i++) {
		enum adapter_feature_id id =
			feature_entry_table[i].feature_id;

		as->default_values[id] = feature_entry_table[i].default_value;
	}

	if (as->ctx->dce_version == DCE_VERSION_11_0) {
		uint32_t i;

		for (i = 0; i < ARRAY_SIZE(feature_entry_table); i++) {
			enum adapter_feature_id id =
				feature_entry_table[i].feature_id;

			if (id == FEATURE_MAXIMIZE_URGENCY_WATERMARKS ||
				id == FEATURE_MAXIMIZE_STUTTER_MARKS ||
				id == FEATURE_MAXIMIZE_NBP_MARKS)
				as->default_values[id] = true;
		}
	}

	/* Generate feature set table */
	if (!generate_feature_set(as)) {
		ASSERT_CRITICAL(false);
		goto failed_to_generate_features;
	}

	as->dce_environment = init_data->dce_environment;

	dcb = as->ctx->dc_bios;

	dcb->funcs->post_init(dcb, as);

	/* Generate backlight translation table and initializes
			  other brightness properties */
	as->backlight_caps_initialized = false;

	get_platform_info_methods(as);

	initialize_backlight_caps(as);

	return true;

failed_to_generate_features:
	dal_asic_capability_destroy(&as->asic_cap);

	return false;
}

/*
 * Global function definition
 */

/*
 * dal_adapter_service_create
 *
 * Create adapter service
 */
struct adapter_service *dal_adapter_service_create(
	struct as_init_data *init_data)
{
	struct adapter_service *as;

	as = dm_alloc(sizeof(struct adapter_service));

	if (!as) {
		ASSERT_CRITICAL(false);
		return NULL;
	}

	if (adapter_service_construct(as, init_data))
		return as;

	ASSERT_CRITICAL(false);

	dm_free(as);

	return NULL;
}

/*
 * dal_adapter_service_destroy
 *
 * Destroy adapter service and objects it contains
 */
void dal_adapter_service_destroy(
	struct adapter_service **as)
{
	if (!as) {
		ASSERT_CRITICAL(false);
		return;
	}

	if (!*as) {
		ASSERT_CRITICAL(false);
		return;
	}

	adapter_service_destruct(*as);

	dm_free(*as);

	*as = NULL;
}

/*
 * dal_adapter_service_is_feature_supported
 *
 * Return if a given feature is supported by the ASIC. The feature has to be
 * a boolean type.
 */
bool dal_adapter_service_is_feature_supported(struct adapter_service *as,
					      enum adapter_feature_id feature_id)
{
	bool data = 0;

	dal_adapter_service_get_feature_value(as, feature_id, &data, sizeof(bool));

	return data;
}

/*
 * dal_adapter_service_is_dfs_bypass_enabled
 *
 * Check if DFS bypass is enabled
 */
bool dal_adapter_service_is_dfs_bypass_enabled(
	struct adapter_service *as)
{
	struct dc_bios *bp = as->ctx->dc_bios;

	if (bp->integrated_info == NULL)
		return false;
	if ((bp->integrated_info->gpu_cap_info & DFS_BYPASS_ENABLE) &&
	    dal_adapter_service_is_feature_supported(as,
			FEATURE_ENABLE_DFS_BYPASS))
		return true;
	else
		return false;
}

/*
 * dal_adapter_service_get_asic_vram_bit_width
 *
 * Get the video RAM bit width set on the ASIC
 */
uint32_t dal_adapter_service_get_asic_vram_bit_width(
	struct adapter_service *as)
{
	return as->asic_cap->data[ASIC_DATA_VRAM_BITWIDTH];
}

struct dal_asic_runtime_flags dal_adapter_service_get_asic_runtime_flags(
		struct adapter_service *as)
{
	return as->asic_cap->runtime_flags;
}

/*
 * dal_adapter_service_get_feature_value
 *
 * Get the cached value of a given feature. This value can be a boolean, int,
 * or characters.
 */
bool dal_adapter_service_get_feature_value(struct adapter_service *as,
					   const enum adapter_feature_id feature_id,
					   void *data,
					   uint32_t size)
{
	uint32_t entry_idx = 0;
	uint32_t set_idx = 0;
	uint32_t set_internal_idx = 0;

	if (feature_id >= FEATURE_MAXIMUM || feature_id <= FEATURE_UNKNOWN) {
		ASSERT_CRITICAL(false);
		return false;
	}

	if (data == NULL) {
		ASSERT_CRITICAL(false);
		return false;
	}

	entry_idx = lookup_feature_entry(as, feature_id);
	set_idx = (uint32_t)((feature_id - 1)/32);
	set_internal_idx = (uint32_t)((feature_id - 1) % 32);

	if (entry_idx >= get_feature_entries_num()) {
		/* Cannot find this entry */
		ASSERT_CRITICAL(false);
		return false;
	}

	if (feature_entry_table[entry_idx].is_boolean_type) {
		if (size != sizeof(bool)) {
			ASSERT_CRITICAL(false);
			return false;
		}

		*(bool *)data = get_bool_value(as->adapter_feature_set[set_idx],
				set_internal_idx);
	} else {
		if (size != sizeof(uint32_t)) {
			ASSERT_CRITICAL(false);
			return false;
		}

		*(uint32_t *)data = as->adapter_feature_set[set_idx];
	}

	return true;
}

/*
 * dal_adapter_service_should_optimize
 *
 * @brief Reports whether driver settings allow requested optimization
 *
 * @param
 * as: adapter service handler
 * feature: for which optimization is validated
 *
 * @return
 * true if requested feature can be optimized
 */
bool dal_adapter_service_should_optimize(
		struct adapter_service *as, enum optimization_feature feature)
{
	uint32_t supported_optimization = 0;
	struct dal_asic_runtime_flags flags;
	struct dc_bios *bp = as->ctx->dc_bios;

	if (!dal_adapter_service_get_feature_value(as, FEATURE_OPTIMIZATION,
			&supported_optimization, sizeof(uint32_t)))
		return false;

	/* Retrieve ASIC runtime flags */
	flags = dal_adapter_service_get_asic_runtime_flags(as);

	/* Check runtime flags against different optimization features */
	switch (feature) {
	case OF_SKIP_HW_PROGRAMMING_ON_ENABLED_EMBEDDED_DISPLAY:
		if (!flags.flags.bits.OPTIMIZED_DISPLAY_PROGRAMMING_ON_BOOT)
			return false;
		break;

	case OF_SKIP_RESET_OF_ALL_HW_ON_S3RESUME:
		if (bp->integrated_info == NULL ||
				!flags.flags.bits.SKIP_POWER_DOWN_ON_RESUME)
			return false;
		break;
	case OF_SKIP_POWER_DOWN_INACTIVE_ENCODER:
		if (!dal_adapter_service_get_asic_runtime_flags(as).flags.bits.
			SKIP_POWER_DOWN_ON_RESUME)
			return false;
		break;
	default:
		break;
	}

	return (supported_optimization & feature) != 0;
}

