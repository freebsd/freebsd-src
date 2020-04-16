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

#include <gbpf_driver.h>
#include <errno.h>

int
gbpf_load_prog(GBPFDriver *driver, uint16_t prog_type, void *prog,
	       uint32_t prog_len)
{
	return driver->load_prog(driver, prog_type, prog, prog_len);
}

int
gbpf_map_create(GBPFDriver *driver, uint16_t type, uint32_t key_size,
		uint32_t value_size, uint32_t max_entries, uint32_t map_flags)
{
	return driver->map_create(driver, type, key_size, value_size,
				  max_entries, map_flags);
}

int
gbpf_map_update_elem(GBPFDriver *driver, int map_desc, void *key, void *value,
		     uint64_t flags)
{
	return driver->map_update_elem(driver, map_desc, key, value, flags);
}

int
gbpf_map_lookup_elem(GBPFDriver *driver, int map_desc, void *key, void *value)
{
	return driver->map_lookup_elem(driver, map_desc, key, value);
}

int
gbpf_map_delete_elem(GBPFDriver *driver, int map_desc, void *key)
{
	return driver->map_delete_elem(driver, map_desc, key);
}

int
gbpf_map_get_next_key(GBPFDriver *driver, int map_desc, void *key,
		      void *next_key)
{
	return driver->map_get_next_key(driver, map_desc, key, next_key);
}

int32_t
gbpf_get_map_type_by_name(GBPFDriver *driver, const char *name)
{
	return driver->get_map_type_by_name(driver, name);
}

int32_t
gbpf_get_prog_type_by_name(GBPFDriver *driver, const char *name)
{
	return driver->get_prog_type_by_name(driver, name);
}

int
gbpf_attach_probe(GBPFDriver *driver, int prog_desc, const char * probe, int jit)
{
	if (!driver->attach_probe)
		return (ENXIO);

	return driver->attach_probe(driver, prog_desc, probe, jit);
}

void
gbpf_close_prog_desc(GBPFDriver *driver, int prog_desc)
{
	return driver->close_prog_desc(driver, prog_desc);
}

void
gbpf_close_map_desc(GBPFDriver *driver, int map_desc)
{
	return driver->close_map_desc(driver, map_desc);
}
