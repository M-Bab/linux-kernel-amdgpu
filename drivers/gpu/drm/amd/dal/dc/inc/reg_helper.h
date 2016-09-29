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
 */

#ifndef DRIVERS_GPU_DRM_AMD_DAL_DEV_DC_INC_REG_HELPER_H_
#define DRIVERS_GPU_DRM_AMD_DAL_DEV_DC_INC_REG_HELPER_H_

/* macro for register read/write
 * user of macro need to define
 *
 * CTX ==> macro to ptr to dc_context
 *    eg. aud110->base.ctx
 *
 * REG ==> macro to location of register offset
 *    eg. aud110->regs->reg
 */
#define REG_READ(reg_name) \
		dm_read_reg(CTX, REG(reg_name))

#define REG_WRITE(reg_name, value) \
		dm_write_reg(CTX, REG(reg_name), value)


/* macro to set register fields. */
#define REG_SET_N(reg_name, n, initial_val, ...)	\
		generic_reg_update_ex(CTX, \
				REG(reg_name), \
				initial_val, \
				n, __VA_ARGS__)

#define REG_SET(reg_name, initial_val, field, val)	\
		REG_SET_N(reg_name, 1, initial_val, \
				FD(reg_name##__##field), val)

#define REG_SET_2(reg, init_value, f1, v1, f2, v2)	\
		REG_SET_N(reg, 2, init_value, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2)

#define REG_SET_3(reg, init_value, f1, v1, f2, v2, f3, v3)	\
		REG_SET_N(reg, 3, init_value, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2,\
				FD(reg##__##f3), v3)

#define REG_SET_4(reg, init_value, f1, v1, f2, v2, f3, v3, f4, v4)	\
		REG_SET_N(reg, 4, init_value, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2,\
				FD(reg##__##f3), v3,\
				FD(reg##__##f4), v4)

#define REG_SET_6(reg, init_value, f1, v1, f2, v2, f3, v3, f4, v4,	\
		f5, v5, f6, v6)	\
		REG_SET_N(reg, 6, init_value, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2,\
				FD(reg##__##f3), v3,\
				FD(reg##__##f4), v4,\
				FD(reg##__##f5), v5,\
				FD(reg##__##f6), v6)

#define REG_SET_7(reg, init_value, f1, v1, f2, v2, f3, v3, f4, v4,	\
		f5, v5, f6, v6, f7, v7)	\
		REG_SET_N(reg, 7, init_value, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2,\
				FD(reg##__##f3), v3,\
				FD(reg##__##f4), v4,\
				FD(reg##__##f5), v5,\
				FD(reg##__##f6), v6,\
				FD(reg##__##f7), v7)


/* macro to get register fields
 * read given register and fill in field value in output parameter */
#define REG_GET(reg_name, field, val)	\
		generic_reg_get(CTX, REG(reg_name), \
				FD(reg_name##__##field), val)

#define REG_GET_2(reg_name, f1, v1, f2, v2)	\
		generic_reg_get2(CTX, REG(reg_name), \
				FD(reg_name##__##f1), v1, \
				FD(reg_name##__##f2), v2)

/* macro to poll and wait for a register field to read back given value */

#define REG_WAIT(reg_name, field, val, delay, max_try)	\
		generic_reg_wait(CTX, \
				REG(reg_name), FD(reg_name##__##field), val,\
				delay, max_try)

/* macro to update (read, modify, write) register fields
 */
#define REG_UPDATE_N(reg_name, n, ...)	\
		generic_reg_update_ex(CTX, \
				REG(reg_name), \
				REG_READ(reg_name), \
				n, __VA_ARGS__)

#define REG_UPDATE(reg_name, field, val)	\
		REG_UPDATE_N(reg_name, 1, \
				FD(reg_name##__##field), val)

#define REG_UPDATE_2(reg, f1, v1, f2, v2)	\
		REG_UPDATE_N(reg, 2,\
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2)

#define REG_UPDATE_3(reg, f1, v1, f2, v2, f3, v3)	\
		REG_UPDATE_N(reg, 3, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2, \
				FD(reg##__##f3), v3)

#define REG_UPDATE_4(reg, f1, v1, f2, v2, f3, v3, f4, v4)	\
		REG_UPDATE_N(reg, 4, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2, \
				FD(reg##__##f3), v3, \
				FD(reg##__##f4), v4)

#define REG_UPDATE_5(reg, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5)	\
		REG_UPDATE_N(reg, 5, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2, \
				FD(reg##__##f3), v3, \
				FD(reg##__##f4), v4, \
				FD(reg##__##f5), v5)

#define REG_UPDATE_6(reg, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6)	\
		REG_UPDATE_N(reg, 6, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2, \
				FD(reg##__##f3), v3, \
				FD(reg##__##f4), v4, \
				FD(reg##__##f5), v5, \
				FD(reg##__##f6), v6)

#define REG_UPDATE_7(reg, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7)	\
		REG_UPDATE_N(reg, 7, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2, \
				FD(reg##__##f3), v3, \
				FD(reg##__##f4), v4, \
				FD(reg##__##f5), v5, \
				FD(reg##__##f6), v6, \
				FD(reg##__##f7), v7)

#define REG_UPDATE_8(reg, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8)	\
		REG_UPDATE_N(reg, 8, \
				FD(reg##__##f1), v1,\
				FD(reg##__##f2), v2, \
				FD(reg##__##f3), v3, \
				FD(reg##__##f4), v4, \
				FD(reg##__##f5), v5, \
				FD(reg##__##f6), v6, \
				FD(reg##__##f7), v7, \
				FD(reg##__##f8), v8)

#define REG_UPDATE_9(reg, f1, v1, f2, v2, f3, v3, f4, v4, f5, v5, f6, v6, f7, v7, f8, v8, f9, v9)	\
		REG_UPDATE_N(reg, 9, \
				FD(reg##__##f1), val1,\
				FD(reg##__##f2), val2, \
				FD(reg##__##f3), val3, \
				FD(reg##__##f4), val4, \
				FD(reg##__##f5), val5, \
				FD(reg##__##f6), val6, \
				FD(reg##__##f7), val7, \
				FD(reg##__##f8), val8, \
				FD(reg##__##f9), val9)

/* macro to update a register field to specified values in given sequences.
 * useful when toggling bits
 */
#define REG_UPDATE_SEQ(reg, field, value1, value2) \
{	uint32_t val = REG_UPDATE(reg, field, value1); \
	REG_SET(reg, val, field, value2); }

/* macro to update fields in register 1 field at a time in given order */
#define REG_UPDATE_1BY1_2(reg, f1, v1, f2, v2) \
{	uint32_t val = REG_UPDATE(reg, f1, v1); \
	REG_SET(reg, val, f2, v2); }

#define REG_UPDATE_1BY1_3(reg, f1, v1, f2, v2, f3, v3) \
{	uint32_t val = REG_UPDATE(reg, f1, v1); \
	val = REG_SET(reg, val, f2, v2); \
	REG_SET(reg, val, f3, v3); }

uint32_t generic_reg_get(const struct dc_context *ctx, uint32_t addr,
		uint8_t shift, uint32_t mask, uint32_t *field_value);

uint32_t generic_reg_get2(const struct dc_context *ctx, uint32_t addr,
		uint8_t shift1, uint32_t mask1, uint32_t *field_value1,
		uint8_t shift2, uint32_t mask2, uint32_t *field_value2);

#endif /* DRIVERS_GPU_DRM_AMD_DAL_DEV_DC_INC_REG_HELPER_H_ */
