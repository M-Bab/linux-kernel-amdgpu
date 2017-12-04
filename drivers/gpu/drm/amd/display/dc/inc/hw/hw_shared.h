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

#ifndef __DAL_HW_SHARED_H__
#define __DAL_HW_SHARED_H__

#include "os_types.h"
#include "fixed31_32.h"
#include "dc_hw_types.h"

/******************************************************************************
 * Data types shared between different Virtual HW blocks
 ******************************************************************************/

#define MAX_PIPES 6

struct gamma_curve {
	uint32_t offset;
	uint32_t segments_num;
};

struct curve_points {
	struct fixed31_32 x;
	struct fixed31_32 y;
	struct fixed31_32 offset;
	struct fixed31_32 slope;

	uint32_t custom_float_x;
	uint32_t custom_float_y;
	uint32_t custom_float_offset;
	uint32_t custom_float_slope;
};

struct pwl_result_data {
	struct fixed31_32 red;
	struct fixed31_32 green;
	struct fixed31_32 blue;

	struct fixed31_32 delta_red;
	struct fixed31_32 delta_green;
	struct fixed31_32 delta_blue;

	uint32_t red_reg;
	uint32_t green_reg;
	uint32_t blue_reg;

	uint32_t delta_red_reg;
	uint32_t delta_green_reg;
	uint32_t delta_blue_reg;
};

struct pwl_params {
	struct gamma_curve arr_curve_points[34];
	struct curve_points arr_points[2];
	struct pwl_result_data rgb_resulted[256 + 3];
	uint32_t hw_points_num;
};

/* move to dpp
 * while we are moving functionality out of opp to dpp to align
 * HW programming to HW IP, we define these struct in hw_shared
 * so we can still compile while refactoring
 */

enum lb_pixel_depth {
	/* do not change the values because it is used as bit vector */
	LB_PIXEL_DEPTH_18BPP = 1,
	LB_PIXEL_DEPTH_24BPP = 2,
	LB_PIXEL_DEPTH_30BPP = 4,
	LB_PIXEL_DEPTH_36BPP = 8
};

enum graphics_csc_adjust_type {
	GRAPHICS_CSC_ADJUST_TYPE_BYPASS = 0,
	GRAPHICS_CSC_ADJUST_TYPE_HW, /* without adjustments */
	GRAPHICS_CSC_ADJUST_TYPE_SW  /*use adjustments */
};

enum ipp_degamma_mode {
	IPP_DEGAMMA_MODE_BYPASS,
	IPP_DEGAMMA_MODE_HW_sRGB,
	IPP_DEGAMMA_MODE_HW_xvYCC,
	IPP_DEGAMMA_MODE_USER_PWL
};

enum ipp_output_format {
	IPP_OUTPUT_FORMAT_12_BIT_FIX,
	IPP_OUTPUT_FORMAT_16_BIT_BYPASS,
	IPP_OUTPUT_FORMAT_FLOAT
};

enum expansion_mode {
	EXPANSION_MODE_DYNAMIC,
	EXPANSION_MODE_ZERO
};

struct default_adjustment {
	enum lb_pixel_depth lb_color_depth;
	enum dc_color_space out_color_space;
	enum dc_color_space in_color_space;
	enum dc_color_depth color_depth;
	enum pixel_format surface_pixel_format;
	enum graphics_csc_adjust_type csc_adjust_type;
	bool force_hw_default;
};

struct out_csc_color_matrix {
	enum dc_color_space color_space;
	uint16_t regval[12];
};

struct output_csc_matrix {
	enum dc_color_space color_space;
	uint16_t regval[12];
};

static const struct output_csc_matrix output_csc_matrix[] = {
	{ COLOR_SPACE_SRGB,
		{ 0x2000, 0, 0, 0, 0, 0x2000, 0, 0, 0, 0, 0x2000, 0} },
	{ COLOR_SPACE_SRGB_LIMITED,
		{ 0x1B67, 0, 0, 0x201, 0, 0x1B67, 0, 0x201, 0, 0, 0x1B67, 0x201} },
	{ COLOR_SPACE_YCBCR601,
		{ 0xE04, 0xF444, 0xFDB9, 0x1004, 0x831, 0x1016, 0x320, 0x201, 0xFB45,
				0xF6B7, 0xE04, 0x1004} },
	{ COLOR_SPACE_YCBCR709,
		{ 0xE04, 0xF345, 0xFEB7, 0x1004, 0x5D3, 0x1399, 0x1FA,
				0x201, 0xFCCA, 0xF533, 0xE04, 0x1004} },

	/* TODO: correct values below */
	{ COLOR_SPACE_YCBCR601_LIMITED,
		{ 0xE00, 0xF447, 0xFDB9, 0x1000, 0x991,
				0x12C9, 0x3A6, 0x200, 0xFB47, 0xF6B9, 0xE00, 0x1000} },
	{ COLOR_SPACE_YCBCR709_LIMITED,
		{ 0xE00, 0xF349, 0xFEB7, 0x1000, 0x6CE, 0x16E3,
				0x24F, 0x200, 0xFCCB, 0xF535, 0xE00, 0x1000} },
};

enum opp_regamma {
	OPP_REGAMMA_BYPASS = 0,
	OPP_REGAMMA_SRGB,
	OPP_REGAMMA_3_6,
	OPP_REGAMMA_USER
};

struct csc_transform {
	uint16_t matrix[12];
	bool enable_adjustment;
};

struct dc_bias_and_scale {
	uint16_t scale_red;
	uint16_t bias_red;
	uint16_t scale_green;
	uint16_t bias_green;
	uint16_t scale_blue;
	uint16_t bias_blue;
};

#endif /* __DAL_HW_SHARED_H__ */
