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

#define DC_TO_CORE(dc)\
	container_of(dc, struct core_dc, public)

struct core_dc {
	struct dc public;
	struct dc_context *ctx;

	uint8_t link_count;
	struct core_link *links[MAX_PIPES * 2];

	struct validate_context *current_context;
	struct resource_pool *res_pool;

	/* Display Engine Clock levels */
	struct dm_pp_clock_levels sclk_lvls;

	/* Inputs into BW and WM calculations. */
	struct bw_calcs_dceip bw_dceip;
	struct bw_calcs_vbios bw_vbios;
#ifdef CONFIG_DRM_AMD_DC_DCN1_0
	struct dcn_soc_bounding_box dcn_soc;
	struct dcn_ip_params dcn_ip;
	struct display_mode_lib dml;
#endif

	/* HW functions */
	struct hw_sequencer_funcs hwss;
	struct dce_hwseq *hwseq;

	/* temp store of dm_pp_display_configuration
	 * to compare to see if display config changed
	 */
	struct dm_pp_display_configuration prev_display_config;
};

#endif /* __CORE_DC_H__ */
