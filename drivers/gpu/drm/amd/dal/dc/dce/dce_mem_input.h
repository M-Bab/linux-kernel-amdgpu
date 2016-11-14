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
#ifndef __DCE_MEM_INPUT_H__
#define __DCE_MEM_INPUT_H__

#define MI_REG_LIST(id)\
	SRI(GRPH_ENABLE, DCP, id),\
	SRI(GRPH_CONTROL, DCP, id),\
	SRI(GRPH_X_START, DCP, id),\
	SRI(GRPH_Y_START, DCP, id),\
	SRI(GRPH_X_END, DCP, id),\
	SRI(GRPH_Y_END, DCP, id),\
	SRI(GRPH_PITCH, DCP, id),\
	SRI(HW_ROTATION, DCP, id),\
	SRI(GRPH_SWAP_CNTL, DCP, id),\
	SRI(PRESCALE_GRPH_CONTROL, DCP, id)

struct dce_mem_input_registers {
	uint32_t GRPH_ENABLE;
	uint32_t GRPH_CONTROL;
	uint32_t GRPH_X_START;
	uint32_t GRPH_Y_START;
	uint32_t GRPH_X_END;
	uint32_t GRPH_Y_END;
	uint32_t GRPH_PITCH;
	uint32_t HW_ROTATION;
	uint32_t GRPH_SWAP_CNTL;
	uint32_t PRESCALE_GRPH_CONTROL;
};

/* Set_Filed_for_Block */
#define SFB(blk_name, reg_name, field_name, post_fix)\
	.field_name = blk_name ## reg_name ## __ ## field_name ## post_fix

#define MI_GFX8_TILE_MASK_SH_LIST(mask_sh, blk)\
	SFB(blk, GRPH_CONTROL, GRPH_BANK_HEIGHT, mask_sh),\
	SFB(blk, GRPH_CONTROL, GRPH_MACRO_TILE_ASPECT, mask_sh),\
	SFB(blk, GRPH_CONTROL, GRPH_TILE_SPLIT, mask_sh),\
	SFB(blk, GRPH_CONTROL, GRPH_MICRO_TILE_MODE, mask_sh),\
	SFB(blk, GRPH_CONTROL, GRPH_PIPE_CONFIG, mask_sh),\
	SFB(blk, GRPH_CONTROL, GRPH_ARRAY_MODE, mask_sh),\
	SFB(blk, GRPH_CONTROL, GRPH_COLOR_EXPANSION_MODE, mask_sh)

#define MI_DCP_MASK_SH_LIST(mask_sh, blk)\
	SFB(blk, GRPH_ENABLE, GRPH_ENABLE, mask_sh),\
	SFB(blk, GRPH_CONTROL, GRPH_DEPTH, mask_sh),\
	SFB(blk, GRPH_CONTROL, GRPH_FORMAT, mask_sh),\
	SFB(blk, GRPH_CONTROL, GRPH_NUM_BANKS, mask_sh),\
	SFB(blk, GRPH_X_START, GRPH_X_START, mask_sh),\
	SFB(blk, GRPH_Y_START, GRPH_Y_START, mask_sh),\
	SFB(blk, GRPH_X_END, GRPH_X_END, mask_sh),\
	SFB(blk, GRPH_Y_END, GRPH_Y_END, mask_sh),\
	SFB(blk, GRPH_PITCH, GRPH_PITCH, mask_sh),\
	SFB(blk, HW_ROTATION, GRPH_ROTATION_ANGLE, mask_sh),\
	SFB(blk, GRPH_SWAP_CNTL, GRPH_RED_CROSSBAR, mask_sh),\
	SFB(blk, GRPH_SWAP_CNTL, GRPH_BLUE_CROSSBAR, mask_sh),\
	SFB(blk, PRESCALE_GRPH_CONTROL, GRPH_PRESCALE_SELECT, mask_sh),\
	SFB(blk, PRESCALE_GRPH_CONTROL, GRPH_PRESCALE_R_SIGN, mask_sh),\
	SFB(blk, PRESCALE_GRPH_CONTROL, GRPH_PRESCALE_G_SIGN, mask_sh),\
	SFB(blk, PRESCALE_GRPH_CONTROL, GRPH_PRESCALE_B_SIGN, mask_sh)

#define MI_DCE_MASK_SH_LIST(mask_sh)\
		MI_DCP_MASK_SH_LIST(mask_sh, ),\
		MI_GFX8_TILE_MASK_SH_LIST(mask_sh, )

#define MI_REG_FIED_LIST(type) \
	type GRPH_ENABLE; \
	type GRPH_X_START; \
	type GRPH_Y_START; \
	type GRPH_X_END; \
	type GRPH_Y_END; \
	type GRPH_PITCH; \
	type GRPH_ROTATION_ANGLE; \
	type GRPH_RED_CROSSBAR; \
	type GRPH_BLUE_CROSSBAR; \
	type GRPH_PRESCALE_SELECT; \
	type GRPH_PRESCALE_R_SIGN; \
	type GRPH_PRESCALE_G_SIGN; \
	type GRPH_PRESCALE_B_SIGN; \
	type GRPH_DEPTH; \
	type GRPH_FORMAT; \
	type GRPH_NUM_BANKS; \
	type GRPH_BANK_WIDTH;\
	type GRPH_BANK_HEIGHT;\
	type GRPH_MACRO_TILE_ASPECT;\
	type GRPH_TILE_SPLIT;\
	type GRPH_MICRO_TILE_MODE;\
	type GRPH_PIPE_CONFIG;\
	type GRPH_ARRAY_MODE;\
	type GRPH_COLOR_EXPANSION_MODE;\
	type GRPH_SW_MODE; \
	type GRPH_NUM_SHADER_ENGINES; \
	type GRPH_NUM_PIPES; \

struct dce_mem_input_shift {
	MI_REG_FIED_LIST(uint8_t)
};

struct dce_mem_input_mask {
	MI_REG_FIED_LIST(uint32_t)
};

struct mem_input;
bool dce_mem_input_program_surface_config(struct mem_input *mi,
	enum surface_pixel_format format,
	union dc_tiling_info *tiling_info,
	union plane_size *plane_size,
	enum dc_rotation_angle rotation,
	struct dc_plane_dcc_param *dcc,
	bool horizontal_mirror);

#endif /*__DCE_MEM_INPUT_H__*/
