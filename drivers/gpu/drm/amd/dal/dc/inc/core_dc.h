/*
 * core_dc.h
 *
 *  Created on: Nov 13, 2015
 *      Author: yonsun
 */

#ifndef __CORE_DC_H__
#define __CORE_DC_H__

#include "core_types.h"
#include "hw_sequencer.h"

struct core_dc {
	struct dc_context *ctx;

	uint8_t link_count;
	struct core_link *links[MAX_PIPES * 2];

	/* TODO: determine max number of targets*/
	struct validate_context current_context;
	struct resource_pool res_pool;

	/*Power State*/
	enum dc_video_power_state previous_power_state;
	enum dc_video_power_state current_power_state;

	/* Inputs into BW and WM calculations. */
	struct bw_calcs_dceip bw_dceip;
	struct bw_calcs_vbios bw_vbios;

	/* HW functions */
	struct hw_sequencer_funcs hwss;
};

#endif /* __CORE_DC_H__ */
