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
#ifndef __DCE_HWSEQ_H__
#define __DCE_HWSEQ_H__

#include "hw_sequencer.h"

#define HWSEQ_DCEF_REG_LIST_DCE8() \
	.DCFE_CLOCK_CONTROL[0] = mmCRTC0_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[1] = mmCRTC1_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[2] = mmCRTC2_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[3] = mmCRTC3_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[4] = mmCRTC4_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[5] = mmCRTC5_CRTC_DCFE_CLOCK_CONTROL

#define HWSEQ_DCEF_REG_LIST() \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 0), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 1), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 2), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 3), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 4), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 5)

#define HWSEQ_BLND_REG_LIST() \
	SRII(BLND_V_UPDATE_LOCK, BLND, 0), \
	SRII(BLND_V_UPDATE_LOCK, BLND, 1), \
	SRII(BLND_V_UPDATE_LOCK, BLND, 2), \
	SRII(BLND_V_UPDATE_LOCK, BLND, 3), \
	SRII(BLND_V_UPDATE_LOCK, BLND, 4), \
	SRII(BLND_V_UPDATE_LOCK, BLND, 5), \
	SRII(BLND_CONTROL, BLND, 0), \
	SRII(BLND_CONTROL, BLND, 1), \
	SRII(BLND_CONTROL, BLND, 2), \
	SRII(BLND_CONTROL, BLND, 3), \
	SRII(BLND_CONTROL, BLND, 4), \
	SRII(BLND_CONTROL, BLND, 5)

#define HWSEQ_DCE8_REG_LIST_BASE() \
	HWSEQ_DCEF_REG_LIST_DCE8(), \
	HWSEQ_BLND_REG_LIST(), \

#define HWSEQ_COMMON_REG_LIST_BASE() \
	HWSEQ_DCEF_REG_LIST(), \
	HWSEQ_BLND_REG_LIST()

struct dce_hwseq_registers {
	uint32_t DCFE_CLOCK_CONTROL[6];
	uint32_t BLND_V_UPDATE_LOCK[6];
	uint32_t BLND_CONTROL[6];
	uint32_t BLNDV_CONTROL;
	uint32_t CRTC_H_BLANK_START_END[6];
};
 /* set field name */
#define HWS_SF(blk_name, reg_name, field_name, post_fix)\
	.field_name = blk_name ## reg_name ## __ ## field_name ## post_fix

#define HWSEQ_DCEF_MASK_SH_LIST(mask_sh, blk)\
	HWS_SF(blk, CLOCK_CONTROL, DCFE_CLOCK_ENABLE, mask_sh)

#define HWSEQ_BLND_MASK_SH_LIST(mask_sh, blk)\
	HWS_SF(blk, V_UPDATE_LOCK, BLND_DCP_GRPH_V_UPDATE_LOCK, mask_sh),\
	HWS_SF(blk, V_UPDATE_LOCK, BLND_SCL_V_UPDATE_LOCK, mask_sh),\
	HWS_SF(blk, V_UPDATE_LOCK, BLND_DCP_GRPH_SURF_V_UPDATE_LOCK, mask_sh),\
	HWS_SF(blk, V_UPDATE_LOCK, BLND_BLND_V_UPDATE_LOCK, mask_sh),\
	HWS_SF(blk, V_UPDATE_LOCK, BLND_V_UPDATE_LOCK_MODE, mask_sh),\
	HWS_SF(blk, CONTROL, BLND_FEEDTHROUGH_EN, mask_sh),\
	HWS_SF(blk, CONTROL, BLND_ALPHA_MODE, mask_sh),\
	HWS_SF(blk, CONTROL, BLND_MODE, mask_sh),\
	HWS_SF(blk, CONTROL, BLND_MULTIPLIED_MODE, mask_sh)

#define HWSEQ_DCE8_MASK_SH_LIST_BASE(mask_sh)\
	.DCFE_CLOCK_ENABLE = CRTC_DCFE_CLOCK_CONTROL__CRTC_DCFE_CLOCK_ENABLE ## mask_sh, \
	HWS_SF(BLND_, V_UPDATE_LOCK, BLND_DCP_GRPH_V_UPDATE_LOCK, mask_sh),\
	HWS_SF(BLND_, V_UPDATE_LOCK, BLND_SCL_V_UPDATE_LOCK, mask_sh),\
	HWS_SF(BLND_, V_UPDATE_LOCK, BLND_DCP_GRPH_SURF_V_UPDATE_LOCK, mask_sh),\
	HWS_SF(BLND_, CONTROL, BLND_MODE, mask_sh)

#define HWSEQ_COMMON_MASK_SH_LIST_BASE(mask_sh)\
	HWSEQ_DCEF_MASK_SH_LIST(mask_sh, DCFE_),\
	HWSEQ_BLND_MASK_SH_LIST(mask_sh, BLND_)

#define HWSEQ_REG_FIED_LIST(type) \
	type DCFE_CLOCK_ENABLE; \
	type BLND_DCP_GRPH_V_UPDATE_LOCK; \
	type BLND_SCL_V_UPDATE_LOCK; \
	type BLND_DCP_GRPH_SURF_V_UPDATE_LOCK; \
	type BLND_BLND_V_UPDATE_LOCK; \
	type BLND_V_UPDATE_LOCK_MODE; \
	type BLND_FEEDTHROUGH_EN; \
	type BLND_ALPHA_MODE; \
	type BLND_MODE; \
	type BLND_MULTIPLIED_MODE; \

struct dce_hwseq_shift {
	HWSEQ_REG_FIED_LIST(uint8_t)
};

struct dce_hwseq_mask {
	HWSEQ_REG_FIED_LIST(uint32_t)
};

struct dce_hwseq_wa {
	bool blnd_crtc_trigger;
};

struct dce_hwseq {
	struct dc_context *ctx;
	const struct dce_hwseq_registers *regs;
	const struct dce_hwseq_shift *shifts;
	const struct dce_hwseq_mask *masks;
	struct dce_hwseq_wa wa;
};

void dce_enable_fe_clock(struct dce_hwseq *hwss,
		unsigned int inst, bool enable);

void dce_pipe_control_lock(struct dce_hwseq *hws,
		unsigned int blnd_inst,
		enum pipe_lock_control control_mask,
		bool lock);

enum blnd_mode {
	BLND_MODE_CURRENT_PIPE = 0,/* Data from current pipe only */
	BLND_MODE_OTHER_PIPE, /* Data from other pipe only */
	BLND_MODE_BLENDING,/* Alpha blending - blend 'current' and 'other' */
	BLND_MODE_STEREO
};
void dce_set_blender_mode(struct dce_hwseq *hws,
	unsigned int blnd_inst, enum blnd_mode mode);
#endif   /*__DCE_HWSEQ_H__*/
