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

#include "dm_services.h"

#include "include/logger_interface.h"

#include "audio_dce80.h"
#include "../dce110/audio_dce110.h"

/***** static functions  *****/

static void destruct(struct audio_dce110 *audio)
{
	/*release memory allocated for hw_ctx -- allocated is initiated
	 *by audio_dce80 power_up
	 *audio->base->hw_ctx = NULL is done within hw-ctx->destroy
	 */
	if (audio->base.hw_ctx)
		audio->base.hw_ctx->funcs->destroy(&(audio->base.hw_ctx));

	/* reset base_audio_block */
	dal_audio_destruct_base(&audio->base);
}

static void destroy(struct audio **ptr)
{
	struct audio_dce110 *audio = NULL;

	audio = container_of(*ptr, struct audio_dce110, base);

	destruct(audio);

	/* release memory allocated for audio_dce110*/
	dm_free(audio);
	*ptr = NULL;
}

/**
* initialize
*
* @brief
*  Perform SW initialization - create audio hw context. Then do HW
*  initialization. this function is called at dal_audio_power_up.
*
* @param
*  NONE
*/
static enum audio_result initialize(
	struct audio *audio)
{
	uint8_t audio_endpoint_enum_id = 0;

	audio_endpoint_enum_id = audio->id.enum_id;

	/* HW CTX already create*/
	if (audio->hw_ctx != NULL)
		return AUDIO_RESULT_OK;

	audio->hw_ctx = dal_audio_create_hw_ctx_audio_dce80(
			audio->ctx,
			audio_endpoint_enum_id);

	if (audio->hw_ctx == NULL)
		return AUDIO_RESULT_ERROR;

	/* override HW default settings */
	audio->hw_ctx->funcs->hw_initialize(audio->hw_ctx);

	return AUDIO_RESULT_OK;
}

/**
* SetupAudioDTO
*
* @brief
*  Update audio source clock from hardware context.
*
* @param
*  determines if we have a HDMI link active
*  known pixel rate for HDMI
*  known DCPLL frequency
*/
static void setup_audio_wall_dto(
	struct audio *audio,
	enum signal_type signal,
	const struct audio_crtc_info *crtc_info,
	const struct audio_pll_info *pll_info)
{
	audio->hw_ctx->funcs->setup_audio_wall_dto(
		audio->hw_ctx, signal, crtc_info, pll_info);
}

static const struct audio_funcs funcs = {
	.destroy = destroy,
	.initialize = initialize,
	.setup_audio_wall_dto = setup_audio_wall_dto,
	.az_enable = dce110_aud_az_enable,
	.az_disable = dce110_aud_az_disable,
	.az_configure = dce110_aud_az_configure,
};

static bool construct(
	struct audio_dce110 *audio,
	const struct audio_init_data *init_data)
{
	struct audio *base = &audio->base;

	/* base audio construct*/
	if (!dal_audio_construct_base(base, init_data))
		return false;

	/*vtable methods*/
	base->funcs = &funcs;
	return true;
}

/* --- audio scope functions  --- */

struct audio *dal_audio_create_dce80(
	const struct audio_init_data *init_data)
{
	/*allocate memory for audio_dce110 */
	struct audio_dce110 *audio = dm_alloc(sizeof(struct audio_dce110));

	if (audio == NULL)
		return NULL;

	audio->regs = init_data->reg;

	/*pointer to base_audio_block of audio_dce110 ==> audio base object */
	if (construct(audio, init_data))
		return &audio->base;

	 /*release memory allocated if fail */
	dm_free(audio);
	return NULL;
}

/* Do not need expose construct_dce80 and destruct_dce80 becuase there is
 *derived object after dce80
 */

