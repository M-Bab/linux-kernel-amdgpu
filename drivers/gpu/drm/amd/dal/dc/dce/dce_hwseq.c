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

#include "dce_hwseq.h"
#include "reg_helper.h"
#include "hw_sequencer.h"

#define CTX \
	hws->ctx
#define REG(reg)\
	hws->regs->reg

#undef FN
#define FN(reg_name, field_name) \
	hws->shifts->field_name, hws->masks->field_name

void dce_enable_fe_clock(struct dce_hwseq *hws,
		unsigned int fe_inst, bool enable)
{
	REG_UPDATE(DCFE_CLOCK_CONTROL[fe_inst],
			DCFE_CLOCK_ENABLE, enable);
}

void dce_pipe_control_lock(struct dce_hwseq *hws,
		unsigned int blnd_inst,
		enum pipe_lock_control control_mask,
		bool lock)
{
	uint32_t lock_val = lock ? 1 : 0;
	uint32_t dcp_grph, scl, dcp_grph_surf, blnd, update_lock_mode;

	uint32_t val = REG_GET_5(BLND_V_UPDATE_LOCK[blnd_inst],
			BLND_DCP_GRPH_V_UPDATE_LOCK, &dcp_grph,
			BLND_SCL_V_UPDATE_LOCK, &scl,
			BLND_DCP_GRPH_SURF_V_UPDATE_LOCK, &dcp_grph_surf,
			BLND_BLND_V_UPDATE_LOCK, &blnd,
			BLND_V_UPDATE_LOCK_MODE, &update_lock_mode);

	if (control_mask & PIPE_LOCK_CONTROL_GRAPHICS)
		dcp_grph = lock_val;

	if (control_mask & PIPE_LOCK_CONTROL_SCL)
		scl = lock_val;

	if (control_mask & PIPE_LOCK_CONTROL_SURFACE)
		dcp_grph_surf = lock_val;

	if (control_mask & PIPE_LOCK_CONTROL_BLENDER)
		blnd = lock_val;

	if (control_mask & PIPE_LOCK_CONTROL_MODE)
		update_lock_mode = lock_val;

	REG_SET_5(BLND_V_UPDATE_LOCK[blnd_inst], val,
			BLND_DCP_GRPH_V_UPDATE_LOCK, &dcp_grph,
			BLND_SCL_V_UPDATE_LOCK, &scl,
			BLND_DCP_GRPH_SURF_V_UPDATE_LOCK, &dcp_grph_surf,
			BLND_BLND_V_UPDATE_LOCK, &blnd,
			BLND_V_UPDATE_LOCK_MODE, &update_lock_mode);

	if (hws->wa.blnd_crtc_trigger)
		if (!lock && (control_mask & PIPE_LOCK_CONTROL_BLENDER)) {
			uint32_t value = REG_READ(CRTC_H_BLANK_START_END[blnd_inst]);
			REG_WRITE(CRTC_H_BLANK_START_END[blnd_inst], value);
		}
}

void dce_set_blender_mode(struct dce_hwseq *hws,
	unsigned int blnd_inst,
	enum blnd_mode mode)
{
	uint32_t feedthrough = 1;
	uint32_t blnd_mode = 0;
	uint32_t multiplied_mode = 0;
	uint32_t alpha_mode = 2;

	switch (mode) {
	case BLND_MODE_OTHER_PIPE:
		feedthrough = 0;
		blnd_mode = 1;
		alpha_mode = 0;
		break;
	case BLND_MODE_BLENDING:
		feedthrough = 0;
		blnd_mode = 2;
		alpha_mode = 0;
		multiplied_mode = 1;
		break;
	case BLND_MODE_CURRENT_PIPE:
	default:
		if (REG(BLND_CONTROL[blnd_inst]) == REG(BLNDV_CONTROL) ||
				blnd_inst == 0)
			feedthrough = 0;
		break;
	}

	REG_UPDATE_4(BLND_CONTROL[blnd_inst],
		BLND_FEEDTHROUGH_EN, feedthrough,
		BLND_ALPHA_MODE, alpha_mode,
		BLND_MODE, blnd_mode,
		BLND_MULTIPLIED_MODE, multiplied_mode);
}
