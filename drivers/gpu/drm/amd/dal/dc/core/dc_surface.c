/*
 * Copyright 2015 Advanced Micro Devices, Inc.
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

/* DC interface (public) */
#include "dm_services.h"
#include "dc.h"

/* DC core (private) */
#include "core_dc.h"
#include "inc/transform.h"

/*******************************************************************************
 * Private structures
 ******************************************************************************/
struct surface {
	struct core_surface protected;
	enum dc_irq_source irq_source;
	int ref_count;
};

struct gamma {
	struct core_gamma protected;
	int ref_count;
};

#define DC_SURFACE_TO_SURFACE(dc_surface) container_of(dc_surface, struct surface, protected.public)
#define CORE_SURFACE_TO_SURFACE(core_surface) container_of(core_surface, struct surface, protected)

#define DC_GAMMA_TO_GAMMA(dc_gamma) \
	container_of(dc_gamma, struct gamma, protected.public)
#define CORE_GAMMA_TO_GAMMA(core_gamma) \
	container_of(core_gamma, struct gamma, protected)


/*******************************************************************************
 * Private functions
 ******************************************************************************/
static bool construct(struct dc_context *ctx, struct surface *surface)
{
	surface->protected.ctx = ctx;
	return true;
}

static void destruct(struct surface *surface)
{
	if (surface->protected.public.gamma_correction)
		dc_gamma_release(surface->protected.public.gamma_correction);
}

/*******************************************************************************
 * Public functions
 ******************************************************************************/
void enable_surface_flip_reporting(struct dc_surface *dc_surface,
		uint32_t controller_id)
{
	struct surface *surface = DC_SURFACE_TO_SURFACE(dc_surface);
	surface->irq_source = controller_id + DC_IRQ_SOURCE_PFLIP1 - 1;
	/*register_flip_interrupt(surface);*/
}

struct dc_surface *dc_create_surface(const struct core_dc *dc)
{
	struct surface *surface = dm_alloc(dc->ctx, sizeof(*surface));

	if (NULL == surface)
		goto alloc_fail;

	if (false == construct(dc->ctx, surface))
		goto construct_fail;

	dc_surface_retain(&surface->protected.public);

	return &surface->protected.public;

construct_fail:
	dm_free(dc->ctx, surface);

alloc_fail:
	return NULL;
}

void dc_surface_retain(const struct dc_surface *dc_surface)
{
	struct surface *surface = DC_SURFACE_TO_SURFACE(dc_surface);

	++surface->ref_count;
}

void dc_surface_release(const struct dc_surface *dc_surface)
{
	struct surface *surface = DC_SURFACE_TO_SURFACE(dc_surface);
	--surface->ref_count;

	if (surface->ref_count == 0) {
		destruct(surface);
		dm_free(surface->protected.ctx, surface);
	}
}

static bool construct_gamma(struct dc_context *ctx, struct gamma *gamma)
{
	return true;
}

static void destruct_gamma(struct gamma *gamma)
{

}

void dc_gamma_retain(const struct dc_gamma *dc_gamma)
{
	struct gamma *gamma = DC_GAMMA_TO_GAMMA(dc_gamma);

	++gamma->ref_count;
}

void dc_gamma_release(const struct dc_gamma *dc_gamma)
{
	struct gamma *gamma = DC_GAMMA_TO_GAMMA(dc_gamma);
	--gamma->ref_count;

	if (gamma->ref_count == 0) {
		destruct_gamma(gamma);
		dm_free(gamma->protected.ctx, gamma);
	}
}


struct dc_gamma *dc_create_gamma(const struct core_dc *dc)
{
	struct gamma *gamma = dm_alloc(dc->ctx, sizeof(*gamma));

	if (gamma == NULL)
		goto alloc_fail;

	if (false == construct_gamma(dc->ctx, gamma))
		goto construct_fail;

	dc_gamma_retain(&gamma->protected.public);

	return &gamma->protected.public;

construct_fail:
	dm_free(dc->ctx, gamma);

alloc_fail:
	return NULL;
}

