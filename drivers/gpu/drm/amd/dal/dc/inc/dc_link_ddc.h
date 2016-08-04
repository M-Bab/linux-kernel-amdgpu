/*
 * Copyright 2012-15 Advanced Micro Devices, Inc.
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

#ifndef __DAL_DDC_SERVICE_H__
#define __DAL_DDC_SERVICE_H__

#include "include/ddc_service_types.h"
#include "include/i2caux_interface.h"

#define EDID_SEGMENT_SIZE 256

struct ddc_service;
struct adapter_service;
struct graphics_object_id;
enum ddc_result;
struct av_sync_data;
struct dp_receiver_id_info;

struct i2c_payloads;
struct aux_payloads;

struct i2c_payloads *dal_ddc_i2c_payloads_create(struct dc_context *ctx, uint32_t count);
struct i2c_payload *dal_ddc_i2c_payloads_get(struct i2c_payloads *p);
uint32_t  dal_ddc_i2c_payloads_get_count(struct i2c_payloads *p);
void dal_ddc_i2c_payloads_destroy(struct i2c_payloads **p);

struct aux_payloads *dal_ddc_aux_payloads_create(struct dc_context *ctx, uint32_t count);
struct aux_payload *dal_ddc_aux_payloads_get(struct aux_payloads *p);
uint32_t dal_ddc_aux_payloads_get_count(struct aux_payloads *p);
void dal_ddc_aux_payloads_destroy(struct aux_payloads **p);

void dal_ddc_i2c_payloads_add(
		struct i2c_payloads *payloads,
		uint32_t address,
		uint32_t len,
		uint8_t *data,
		bool write);

void dal_ddc_aux_payloads_add(
		struct aux_payloads *payloads,
		uint32_t address,
		uint32_t len,
		uint8_t *data,
		bool write);

struct ddc_service_init_data {
	struct adapter_service *as;
	struct graphics_object_id id;
	struct dc_context *ctx;
};

struct ddc_service *dal_ddc_service_create(
		struct ddc_service_init_data *ddc_init_data);

void dal_ddc_service_destroy(struct ddc_service **ddc);

enum ddc_service_type dal_ddc_service_get_type(struct ddc_service *ddc);

void dal_ddc_service_set_transaction_type(
		struct ddc_service *ddc,
		enum ddc_transaction_type type);

bool dal_ddc_service_is_in_aux_transaction_mode(struct ddc_service *ddc);

uint32_t dal_ddc_service_edid_query(struct ddc_service *ddc);

uint32_t dal_ddc_service_get_edid_buf_len(struct ddc_service *ddc);

void dal_ddc_service_get_edid_buf(struct ddc_service *ddc, uint8_t *edid_buf);

void dal_ddc_service_i2c_query_dp_dual_mode_adaptor(
		struct ddc_service *ddc,
		struct display_sink_capability *sink_cap);

bool dal_ddc_service_query_ddc_data(
		struct ddc_service *ddc,
		uint32_t address,
		uint8_t *write_buf,
		uint32_t write_size,
		uint8_t *read_buf,
		uint32_t read_size);

bool dal_ddc_service_get_dp_receiver_id_info(
		struct ddc_service *ddc,
		struct dp_receiver_id_info *info);

enum ddc_result dal_ddc_service_read_dpcd_data(
		struct ddc_service *ddc,
		uint32_t address,
		uint8_t *data,
		uint32_t len);

enum ddc_result dal_ddc_service_write_dpcd_data(
		struct ddc_service *ddc,
		uint32_t address,
		const uint8_t *data,
		uint32_t len);

void dal_ddc_service_write_scdc_data(
		struct ddc_service *ddc_service,
		uint32_t pix_clk,
		bool lte_340_scramble);

void dal_ddc_service_read_scdc_data(
		struct ddc_service *ddc_service);

void ddc_service_set_dongle_type(struct ddc_service *ddc,
		enum display_dongle_type dongle_type);

void dal_ddc_service_set_ddc_pin(
		struct ddc_service *ddc_service,
		struct ddc *ddc);

struct ddc *dal_ddc_service_get_ddc_pin(struct ddc_service *ddc_service);
void dal_ddc_service_reset_dp_receiver_id_info(struct ddc_service *ddc_service);

enum ddc_result dal_ddc_service_read_dpcd_data(
		struct ddc_service *ddc,
		uint32_t address,
		uint8_t *data,
		uint32_t len);
enum ddc_result dal_ddc_service_write_dpcd_data(
		struct ddc_service *ddc,
		uint32_t address,
		const uint8_t *data,
		uint32_t len);

#endif /* __DAL_DDC_SERVICE_H__ */

