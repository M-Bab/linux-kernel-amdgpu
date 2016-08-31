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

#ifndef MODULES_INC_MOD_BACKLIGHT_H_
#define MODULES_INC_MOD_BACKLIGHT_H_

#include "dm_services.h"

struct mod_backlight {
	int dummy;
};

struct mod_backlight *mod_backlight_create(struct dc *dc);

void mod_backlight_destroy(struct mod_backlight *mod_backlight);

bool mod_backlight_add_sink(struct mod_backlight *mod_backlight,
		const struct dc_sink *sink);

bool mod_backlight_remove_sink(struct mod_backlight *mod_backlight,
		const struct dc_sink *sink);

bool mod_backlight_set_backlight(struct mod_backlight *mod_backlight,
		const struct dc_stream **streams, int num_streams,
		unsigned int backlight_8bit);

bool mod_backlight_get_backlight(struct mod_backlight *mod_backlight,
		const struct dc_sink *sink,
		unsigned int *backlight_8bit);

void mod_backlight_initialize_backlight_caps
		(struct mod_backlight *mod_backlight);

unsigned int mod_backlight_backlight_level_percentage_to_signal
		(struct mod_backlight *mod_backlight, unsigned int percentage);

unsigned int mod_backlight_backlight_level_signal_to_percentage
	(struct mod_backlight *mod_backlight, unsigned int signalLevel8bit);

bool mod_backlight_get_panel_backlight_boundaries
				(struct mod_backlight *mod_backlight,
				unsigned int *min_backlight,
				unsigned int *max_backlight,
				unsigned int *output_ac_level_percentage,
				unsigned int *output_dc_level_percentage);

bool mod_backlight_set_smooth_brightness(struct mod_backlight *mod_backlight,
		const struct dc_sink *sink, bool enable_brightness);

bool mod_backlight_notify_mode_change(struct mod_backlight *mod_backlight,
		const struct dc_stream *stream);
#endif /* MODULES_INC_MOD_BACKLIGHT_H_ */
