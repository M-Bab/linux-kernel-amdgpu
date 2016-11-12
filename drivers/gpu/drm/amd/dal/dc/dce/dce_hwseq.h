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

#define HWSEQ_DCE8_REG_LIST_BASE() \
	.DCFE_CLOCK_CONTROL[0] = mmCRTC0_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[1] = mmCRTC1_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[2] = mmCRTC2_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[3] = mmCRTC3_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[4] = mmCRTC4_CRTC_DCFE_CLOCK_CONTROL, \
	.DCFE_CLOCK_CONTROL[5] = mmCRTC5_CRTC_DCFE_CLOCK_CONTROL, \

#define HWSEQ_COMMON_REG_LIST_BASE() \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 0), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 1), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 2), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 3), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 4), \
	SRII(DCFE_CLOCK_CONTROL, DCFE, 5), \

 /* set field name */
#define SF(reg_name, field_name, post_fix)\
	.field_name = reg_name ## __ ## field_name ## post_fix

#define HWSEQ_DCE8_MASK_SH_LIST_BASE(mask_sh)\
	.DCFE_CLOCK_ENABLE = CRTC_DCFE_CLOCK_CONTROL__CRTC_DCFE_CLOCK_ENABLE ## mask_sh, \

#define HWSEQ_COMMON_MASK_SH_LIST_BASE(mask_sh)\
	SF(DCFE_CLOCK_CONTROL, DCFE_CLOCK_ENABLE, mask_sh),\

struct dce_hwseq_registers {
	uint32_t DCFE_CLOCK_CONTROL[6];
};

struct dce_hwseq_shift {
	uint8_t DCFE_CLOCK_ENABLE;
};

struct dce_hwseq_mask {
	uint32_t DCFE_CLOCK_ENABLE;
};

struct dce_hwseq {
	struct dc_context *ctx;
	const struct dce_hwseq_registers *regs;
	const struct dce_hwseq_shift *shifts;
	const struct dce_hwseq_mask *masks;
};

void dce_enable_fe_clock(struct dce_hwseq *hwss,
		unsigned int inst, bool enable);

#endif   /*__DCE_HWSEQ_H__*/
