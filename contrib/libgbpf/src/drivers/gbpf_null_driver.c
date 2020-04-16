/*-
 * SPDX-License-Identifier: Apache License 2.0
 *
 * Copyright 2017-2018 Yutaro Hayakawa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdlib.h>

#include <gbpf/drivers/gbpf_null_driver.h>

static int
gbpf_null_load_prog(GBPFDriver *self, uint16_t prog_type, void *prog,
		    uint32_t prog_len)
{
	return 0;
}

static int
gbpf_null_map_create(GBPFDriver *self, uint16_t type, uint32_t key_size,
		     uint32_t value_size, uint32_t max_entries,
		     uint32_t map_flags)
{
	return 0;
}

static int
gbpf_null_map_update_elem(GBPFDriver *self, int map_desc, void *key,
			  void *value, uint64_t flags)
{
	return 0;
}

static int
gbpf_null_map_lookup_elem(GBPFDriver *self, int map_desc, void *key,
			  void *value)
{
	return 0;
}

static int
gbpf_null_map_delete_elem(GBPFDriver *self, int map_desc, void *key)
{
	return 0;
}

static int
gbpf_null_map_get_next_key(GBPFDriver *self, int map_desc, void *key,
			   void *next_key)
{
	return 0;
}

static int32_t
gbpf_null_get_map_type_by_name(GBPFDriver *self, const char *name)
{
	return -1;
}

static int32_t
gbpf_null_get_prog_type_by_name(GBPFDriver *self, const char *name)
{
	return -1;
}

static void
gbpf_null_close_prog_desc(GBPFDriver *self, int prog_desc)
{
	return;
}

static void
gbpf_null_close_map_desc(GBPFDriver *self, int map_desc)
{
	return;
}

GBPFNullDriver *
gbpf_null_driver_create(void)
{
	struct gbpf_null_driver *driver =
	    malloc(sizeof(struct gbpf_null_driver));
	if (!driver) {
		return NULL;
	}

	driver->base.load_prog = gbpf_null_load_prog;
	driver->base.map_create = gbpf_null_map_create;
	driver->base.map_update_elem = gbpf_null_map_update_elem;
	driver->base.map_lookup_elem = gbpf_null_map_lookup_elem;
	driver->base.map_delete_elem = gbpf_null_map_delete_elem;
	driver->base.map_get_next_key = gbpf_null_map_get_next_key;
	driver->base.get_map_type_by_name = gbpf_null_get_map_type_by_name;
	driver->base.get_prog_type_by_name = gbpf_null_get_prog_type_by_name;
	driver->base.close_prog_desc = gbpf_null_close_prog_desc;
	driver->base.close_map_desc = gbpf_null_close_map_desc;

	return driver;
}

void
gbpf_null_driver_destroy(GBPFNullDriver *driver)
{
	free(driver);
}
