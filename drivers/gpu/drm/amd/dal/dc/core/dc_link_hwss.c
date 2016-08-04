/* Copyright 2015 Advanced Micro Devices, Inc. */

#include "dm_services.h"
#include "dc.h"
#include "inc/core_dc.h"
#include "include/ddc_service_types.h"
#include "include/i2caux_interface.h"
#include "link_hwss.h"
#include "hw_sequencer.h"
#include "dc_link_ddc.h"
#include "dm_helpers.h"
#include "dce110/dce110_link_encoder.h"
#include "dce110/dce110_stream_encoder.h"


enum dc_status core_link_read_dpcd(
	struct core_link* link,
	uint32_t address,
	uint8_t *data,
	uint32_t size)
{
	if (!dm_helper_dp_read_dpcd(link->ctx,
			&link->public,
			address, data, size))
			return DC_ERROR_UNEXPECTED;

	return DC_OK;
}

enum dc_status core_link_write_dpcd(
	struct core_link* link,
	uint32_t address,
	const uint8_t *data,
	uint32_t size)
{
	if (!dm_helper_dp_write_dpcd(link->ctx,
			&link->public,
			address, data, size))
				return DC_ERROR_UNEXPECTED;

	return DC_OK;
}

void dp_receiver_power_ctrl(struct core_link *link, bool on)
{
	uint8_t state;

	state = on ? DP_POWER_STATE_D0 : DP_POWER_STATE_D3;

	core_link_write_dpcd(link, DPCD_ADDRESS_POWER_STATE, &state,
			sizeof(state));
}

void dp_enable_link_phy(
	struct core_link *link,
	enum signal_type signal,
	const struct link_settings *link_settings)
{
	struct link_encoder *link_enc = link->link_enc;

	if (dc_is_dp_sst_signal(signal)) {
		if (signal == SIGNAL_TYPE_EDP) {
			link_enc->funcs->power_control(link_enc, true);
			link_enc->funcs->backlight_control(link_enc, true);
		}

		link_enc->funcs->enable_dp_output(
						link_enc,
						link_settings,
						CLOCK_SOURCE_ID_EXTERNAL);
	} else {
		link_enc->funcs->enable_dp_mst_output(
						link_enc,
						link_settings,
						CLOCK_SOURCE_ID_EXTERNAL);
	}

	dp_receiver_power_ctrl(link, true);
}

void dp_disable_link_phy(struct core_link *link, enum signal_type signal)
{
	if (!link->wa_flags.dp_keep_receiver_powered)
		dp_receiver_power_ctrl(link, false);

	if (signal == SIGNAL_TYPE_EDP)
		link->link_enc->funcs->backlight_control(link->link_enc, false);

	link->link_enc->funcs->disable_output(link->link_enc, signal);

	/* Clear current link setting.*/
	dm_memset(&link->public.cur_link_settings, 0,
			sizeof(link->public.cur_link_settings));
}

void dp_disable_link_phy_mst(struct core_link *link, struct core_stream *stream)
{
	/* MST disable link only when no stream use the link */
	if (link->mst_stream_alloc_table.stream_count > 0)
		return;

	dp_disable_link_phy(link, stream->signal);
}

bool dp_set_hw_training_pattern(
	struct core_link *link,
	enum hw_dp_training_pattern pattern)
{
	enum dp_test_pattern test_pattern = DP_TEST_PATTERN_UNSUPPORTED;
	struct encoder_set_dp_phy_pattern_param pattern_param = {0};
	struct link_encoder *encoder = link->link_enc;

	switch (pattern) {
	case HW_DP_TRAINING_PATTERN_1:
		test_pattern = DP_TEST_PATTERN_TRAINING_PATTERN1;
		break;
	case HW_DP_TRAINING_PATTERN_2:
		test_pattern = DP_TEST_PATTERN_TRAINING_PATTERN2;
		break;
	case HW_DP_TRAINING_PATTERN_3:
		test_pattern = DP_TEST_PATTERN_TRAINING_PATTERN3;
		break;
	case HW_DP_TRAINING_PATTERN_4:
		test_pattern = DP_TEST_PATTERN_TRAINING_PATTERN4;
		break;
	default:
		break;
	}

	pattern_param.dp_phy_pattern = test_pattern;
	pattern_param.custom_pattern = NULL;
	pattern_param.custom_pattern_size = 0;
	pattern_param.dp_panel_mode = dp_get_panel_mode(link);

	encoder->funcs->dp_set_phy_pattern(encoder, &pattern_param);

	return true;
}


void dp_set_hw_lane_settings(
	struct core_link *link,
	const struct link_training_settings *link_settings)
{
	struct link_encoder *encoder = link->link_enc;

	/* call Encoder to set lane settings */
	encoder->funcs->dp_set_lane_settings(encoder, link_settings);
}

enum dp_panel_mode dp_get_panel_mode(struct core_link *link)
{
	/* We need to explicitly check that connector
	 * is not DP. Some Travis_VGA get reported
	 * by video bios as DP.
	 */
	if (link->public.connector_signal != SIGNAL_TYPE_DISPLAY_PORT) {

		switch (link->dpcd_caps.branch_dev_id) {
		case DP_BRANCH_DEVICE_ID_2:
			if (strncmp(
				link->dpcd_caps.branch_dev_name,
				DP_VGA_LVDS_CONVERTER_ID_2,
				sizeof(
				link->dpcd_caps.
				branch_dev_name)) == 0) {
				return DP_PANEL_MODE_SPECIAL;
			}
			break;
		case DP_BRANCH_DEVICE_ID_3:
			if (strncmp(link->dpcd_caps.branch_dev_name,
				DP_VGA_LVDS_CONVERTER_ID_3,
				sizeof(
				link->dpcd_caps.
				branch_dev_name)) == 0) {
				return DP_PANEL_MODE_SPECIAL;
			}
			break;
		default:
			break;
		}

		if (link->dpcd_caps.panel_mode_edp) {
			return DP_PANEL_MODE_EDP;
		}
	}

	return DP_PANEL_MODE_DEFAULT;
}

void dp_set_hw_test_pattern(
	struct core_link *link,
	enum dp_test_pattern test_pattern)
{
	struct encoder_set_dp_phy_pattern_param pattern_param = {0};
	struct link_encoder *encoder = link->link_enc;

	pattern_param.dp_phy_pattern = test_pattern;
	pattern_param.custom_pattern = NULL;
	pattern_param.custom_pattern_size = 0;
	pattern_param.dp_panel_mode = dp_get_panel_mode(link);

	encoder->funcs->dp_set_phy_pattern(encoder, &pattern_param);
}
