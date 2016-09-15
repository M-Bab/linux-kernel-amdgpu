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

#include "audio.h"
#include "hw_ctx_audio.h"

#if defined(CONFIG_DRM_AMD_DAL_DCE8_0)
#include "dce80/audio_dce80.h"
#include "dce80/hw_ctx_audio_dce80.h"
#endif

#if defined(CONFIG_DRM_AMD_DAL_DCE11_0)
#include "dce110/audio_dce110.h"
#include "dce110/hw_ctx_audio_dce110.h"
#endif


/***** static function : only used within audio.c *****/

/* stub for hook functions */
static void destroy(
	struct audio **audio)
{
	/*DCE specific, must be implemented in derived*/
	BREAK_TO_DEBUGGER();
}

static enum audio_result initialize(
	struct audio *audio)
{
	/*DCE specific, must be implemented in derived. Implemeentaion of
	 *initialize will create audio hw context. create_hw_ctx
	 */
	BREAK_TO_DEBUGGER();
	return AUDIO_RESULT_OK;
}

static const struct audio_funcs audio_funcs = {
	.destroy = destroy,
	.initialize = initialize,
};

/***** SCOPE : declare in audio.h. use within dal-audio. *****/

bool dal_audio_construct_base(
	struct audio *audio,
	const struct audio_init_data *init_data)
{
	/* base hook functions */
	audio->funcs = &audio_funcs;

	audio->ctx = init_data->ctx;

	/* save audio endpoint number to identify object creating */
	audio->id = init_data->audio_stream_id;
	audio->inst = init_data->inst;

	return true;
}

/* except hw_ctx, no other hw need reset. so do nothing */
void dal_audio_destruct_base(
	struct audio *audio)
{
}

/* audio object creator triage.
 * memory for "struct audio   dal_audio_create_dce8x" allocate
 * will happens within dal_audio_dce8x. memory allocate is done
 * with dal_audio_create_dce8x. memory release is initiated by
 * dal_audio_destroy. It will call hook function which will finially
 * used destroy() of dal_audio_dce8x. therefore, no memroy allocate
 *and release happen physcially at audio base object.
 */
void dal_audio_destroy(
	struct audio **audio)
{
	if (!audio || !*audio) {
		BREAK_TO_DEBUGGER();
		return;
	}

	(*audio)->funcs->destroy(audio);

	*audio = NULL;
}

/* DP Audio register write access. This function call hw_ctx directly
 * not overwitten at audio level.
 */

/* perform power up sequence (boot up, resume, recovery) */
enum audio_result dal_audio_power_up(
	struct audio *audio)
{
	return audio->funcs->initialize(audio);
}

