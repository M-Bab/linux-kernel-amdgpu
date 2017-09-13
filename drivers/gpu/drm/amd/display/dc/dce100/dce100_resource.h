/*
 * dce100_resource.h
 *
 *  Created on: 2016-01-20
 *      Author: qyang
 */

#ifndef DCE100_RESOURCE_H_
#define DCE100_RESOURCE_H_

struct core_dc;
struct resource_pool;
struct dc_validation_set;

struct resource_pool *dce100_create_resource_pool(
	uint8_t num_virtual_links,
	struct core_dc *dc);

enum dc_status dce100_validate_plane(const struct dc_plane_state *plane_state);

#endif /* DCE100_RESOURCE_H_ */
