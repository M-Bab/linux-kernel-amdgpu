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
 */

#ifndef DRIVERS_GPU_DRM_AMD_DAL_DEV_DC_INC_RESOURCE_H_
#define DRIVERS_GPU_DRM_AMD_DAL_DEV_DC_INC_RESOURCE_H_

#include "core_types.h"
#include "core_status.h"
#include "core_dc.h"
#include "dal_asic_id.h"

/* TODO unhardcode, 4 for CZ*/
#define MEMORY_TYPE_MULTIPLIER 4

enum dce_version resource_parse_asic_id(
		struct hw_asic_id asic_id);

struct resource_pool *dc_create_resource_pool(struct adapter_service *adapter_serv,
				struct core_dc *dc,
				int num_virtual_links,
				enum dce_version dc_version,
				struct hw_asic_id asic_id);

void dc_destroy_resource_pool(struct resource_pool **pool,
				enum dce_version dc_version);

enum dc_status resource_map_pool_resources(
		const struct core_dc *dc,
		struct validate_context *context);

bool resource_build_scaling_params(
		const struct dc_surface *surface,
		struct pipe_ctx *pipe_ctx);

enum dc_status resource_build_scaling_params_for_context(
		const struct core_dc *dc,
		struct validate_context *context);

void resource_build_info_frame(struct pipe_ctx *pipe_ctx);

void resource_unreference_clock_source(
		struct resource_context *res_ctx,
		struct clock_source *clock_source);

void resource_reference_clock_source(
		struct resource_context *res_ctx,
		struct clock_source *clock_source);

bool resource_are_streams_timing_synchronizable(
		const struct core_stream *stream1,
		const struct core_stream *stream2);

struct clock_source *resource_find_used_clk_src_for_sharing(
		struct resource_context *res_ctx,
		struct pipe_ctx *pipe_ctx);

struct clock_source *dc_resource_find_first_free_pll(
		struct resource_context *res_ctx);

struct pipe_ctx *resource_get_head_pipe_for_stream(
		struct resource_context *res_ctx,
		const struct core_stream *stream);

bool resource_attach_surfaces_to_context(
		const struct dc_surface *const *surfaces,
		int surface_count,
		const struct dc_target *dc_target,
		struct validate_context *context);

struct pipe_ctx *find_idle_secondary_pipe(struct resource_context *res_ctx);

bool resource_is_stream_unchanged(
	const struct validate_context *old_context, struct core_stream *stream);

bool is_target_unchanged(
	const struct core_target *old_target, const struct core_target *target);
bool resource_validate_attach_surfaces(
		const struct dc_validation_set set[],
		int set_count,
		const struct validate_context *old_context,
		struct validate_context *context);

void validate_guaranteed_copy_target(
		struct validate_context *context,
		int max_targets);

void resource_validate_ctx_update_pointer_after_copy(
		const struct validate_context *src_ctx,
		struct validate_context *dst_ctx);

void resource_validate_ctx_copy_construct(
		const struct validate_context *src_ctx,
		struct validate_context *dst_ctx);

void resource_validate_ctx_destruct(struct validate_context *context);

enum dc_status resource_map_clock_resources(
		const struct core_dc *dc,
		struct validate_context *context);

bool pipe_need_reprogram(
		struct pipe_ctx *pipe_ctx_old,
		struct pipe_ctx *pipe_ctx);


#endif /* DRIVERS_GPU_DRM_AMD_DAL_DEV_DC_INC_RESOURCE_H_ */
