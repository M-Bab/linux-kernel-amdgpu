/*
 * Copyright 2016 Advanced Micro Devices, Inc.
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

#include "mem_input.h"
#include "reg_helper.h"

#define CTX \
	mi->ctx
#define REG(reg)\
	mi->regs->reg

#undef FN
#define FN(reg_name, field_name) \
	mi->shifts->field_name, mi->masks->field_name



static void program_tiling(struct mem_input *mi,
	const union dc_tiling_info *info)
{
	if (mi->masks->GRPH_ARRAY_MODE) { /* GFX8 */
		REG_UPDATE_9(GRPH_CONTROL,
				GRPH_NUM_BANKS, info->gfx8.num_banks,
				GRPH_BANK_WIDTH, info->gfx8.bank_width,
				GRPH_BANK_HEIGHT, info->gfx8.bank_height,
				GRPH_MACRO_TILE_ASPECT, info->gfx8.tile_aspect,
				GRPH_TILE_SPLIT, info->gfx8.tile_split,
				GRPH_MICRO_TILE_MODE, info->gfx8.tile_mode,
				GRPH_PIPE_CONFIG, info->gfx8.pipe_config,
				GRPH_ARRAY_MODE, info->gfx8.array_mode,
				GRPH_COLOR_EXPANSION_MODE, 1);
		/* 01 - DCP_GRPH_COLOR_EXPANSION_MODE_ZEXP: zero expansion for YCbCr */
		/*
				GRPH_Z, 0);
				*/
	}
}


static void program_size_and_rotation(
	struct mem_input *mi,
	enum dc_rotation_angle rotation,
	const union plane_size *plane_size)
{
	const struct rect *in_rect = &plane_size->grph.surface_size;
	struct rect hw_rect = plane_size->grph.surface_size;
	const uint32_t rotation_angles[ROTATION_ANGLE_COUNT] = {
			[ROTATION_ANGLE_0] = 0,
			[ROTATION_ANGLE_90] = 1,
			[ROTATION_ANGLE_180] = 2,
			[ROTATION_ANGLE_270] = 3,
	};

	if (rotation == ROTATION_ANGLE_90 || rotation == ROTATION_ANGLE_270) {
		hw_rect.x = in_rect->y;
		hw_rect.y = in_rect->x;

		hw_rect.height = in_rect->width;
		hw_rect.width = in_rect->height;
	}

	REG_SET(GRPH_X_START, 0,
			GRPH_X_START, hw_rect.x);

	REG_SET(GRPH_Y_START, 0,
			GRPH_Y_START, hw_rect.y);

	REG_SET(GRPH_X_END, 0,
			GRPH_X_END, hw_rect.width);

	REG_SET(GRPH_Y_END, 0,
			GRPH_Y_END, hw_rect.height);

	REG_SET(GRPH_PITCH, 0,
			GRPH_PITCH, plane_size->grph.surface_pitch);

	REG_SET(HW_ROTATION, 0,
			GRPH_ROTATION_ANGLE, rotation_angles[rotation]);
}

static void program_grph_pixel_format(
	struct mem_input *mi,
	enum surface_pixel_format format)
{
	uint32_t red_xbar = 0, blue_xbar = 0; /* no swap */
	uint32_t grph_depth, grph_format;
	uint32_t sign = 0, floating = 0;

	if (format == SURFACE_PIXEL_FORMAT_GRPH_BGRA8888 ||
			/*todo: doesn't look like we handle BGRA here,
			 *  should problem swap endian*/
		format == SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010 ||
		format == SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010_XR_BIAS ||
		format == SURFACE_PIXEL_FORMAT_GRPH_ABGR16161616F) {
		/* ABGR formats */
		red_xbar = 2;
		blue_xbar = 2;
	}

	REG_SET_2(GRPH_SWAP_CNTL, 0,
			GRPH_RED_CROSSBAR, red_xbar,
			GRPH_BLUE_CROSSBAR, blue_xbar);

	switch (format) {
	case SURFACE_PIXEL_FORMAT_GRPH_PALETA_256_COLORS:
		grph_depth = 0;
		grph_format = 0;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB1555:
		grph_depth = 1;
		grph_format = 0;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_RGB565:
		grph_depth = 1;
		grph_format = 1;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB8888:
	case SURFACE_PIXEL_FORMAT_GRPH_BGRA8888:
		grph_depth = 2;
		grph_format = 0;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB2101010:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010:
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR2101010_XR_BIAS:
		grph_depth = 2;
		grph_format = 1;
		break;
	case SURFACE_PIXEL_FORMAT_GRPH_ABGR16161616F:
		sign = 1;
		floating = 1;
		/* no break */
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616F: /* shouldn't this get float too? */
	case SURFACE_PIXEL_FORMAT_GRPH_ARGB16161616:
		grph_depth = 3;
		grph_format = 0;
		break;
	default:
		DC_ERR("unsupported grph pixel format");
		break;
	}

	REG_UPDATE_2(GRPH_CONTROL,
			GRPH_DEPTH, grph_depth,
			GRPH_FORMAT, grph_format);

	REG_UPDATE_4(PRESCALE_GRPH_CONTROL,
			GRPH_PRESCALE_SELECT, floating,
			GRPH_PRESCALE_R_SIGN, sign,
			GRPH_PRESCALE_G_SIGN, sign,
			GRPH_PRESCALE_B_SIGN, sign);
}

bool dce_mem_input_program_surface_config(struct mem_input *mi,
	enum surface_pixel_format format,
	union dc_tiling_info *tiling_info,
	union plane_size *plane_size,
	enum dc_rotation_angle rotation,
	struct dc_plane_dcc_param *dcc,
	bool horizontal_mirror)
{
	REG_UPDATE(GRPH_ENABLE, GRPH_ENABLE, 1);

	program_tiling(mi, tiling_info);
	program_size_and_rotation(mi, rotation, plane_size);

	if (format >= SURFACE_PIXEL_FORMAT_GRPH_BEGIN &&
		format < SURFACE_PIXEL_FORMAT_VIDEO_BEGIN)
		program_grph_pixel_format(mi, format);

	return true;
}
