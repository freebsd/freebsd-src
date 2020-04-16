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
#ifdef linux

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/resource.h>

#include <gbpf/drivers/gbpf_linux_driver.h>

static int
gbpf_linux_load_prog(GBPFDriver *self, uint16_t prog_type, void *prog,
		     uint32_t prog_len)
{
	GBPFLinuxDriver *driver = (GBPFLinuxDriver *)self;
	union bpf_attr attr;

	memset(&attr, 0, sizeof(union bpf_attr));

	attr.prog_type = prog_type;
	attr.insn_cnt = prog_len;
	attr.insns = (uint64_t)prog;
	attr.license = driver->license;
	attr.log_level = driver->log_level;
	attr.log_size = driver->log_size;
	attr.log_buf = driver->log_buf;
	attr.kern_version = driver->kern_version;

	return syscall(321, &attr, sizeof(union bpf_attr));
}

static int
gbpf_linux_map_create(GBPFDriver *self, uint16_t type, uint32_t key_size,
		      uint32_t value_size, uint32_t max_entries,
		      uint32_t map_flags)
{
	union bpf_attr attr;

	memset(&attr, 0, sizeof(union bpf_attr));

	attr.map_type = type;
	attr.key_size = key_size;
	attr.value_size = value_size;
	attr.max_entries = max_entries;

	struct rlimit rl = {};
	if (getrlimit(RLIMIT_MEMLOCK, &rl) == 0) {
		rl.rlim_max = RLIM_INFINITY;
		rl.rlim_cur = rl.rlim_max;
		if (setrlimit(RLIMIT_MEMLOCK, &rl) == 0) {
			return syscall(321, BPF_MAP_CREATE, &attr,
				       sizeof(union bpf_attr));
		}
	}

	return -1;
}

static int
gbpf_linux_map_update_elem(GBPFDriver *self, int map_desc, void *key,
			   void *value, uint64_t flags)
{
	union bpf_attr attr;

	memset(&attr, 0, sizeof(union bpf_attr));

	attr.map_fd = map_desc;
	attr.key = (uint64_t)key;
	attr.value = (uint64_t)value;
	attr.flags = flags;

	return syscall(321, BPF_MAP_UPDATE_ELEM, &attr, sizeof(union bpf_attr));
}

static int
gbpf_linux_map_lookup_elem(GBPFDriver *self, int map_desc, void *key,
			   void *value)
{
	union bpf_attr attr;

	memset(&attr, 0, sizeof(union bpf_attr));

	attr.map_fd = map_desc;
	attr.key = (uint64_t)key;
	attr.value = (uint64_t)value;

	return syscall(321, BPF_MAP_LOOKUP_ELEM, &attr, sizeof(union bpf_attr));
}

static int
gbpf_linux_map_delete_elem(GBPFDriver *self, int map_desc, void *key)
{
	union bpf_attr attr;

	memset(&attr, 0, sizeof(union bpf_attr));

	attr.map_fd = map_desc;
	attr.key = (uint64_t)key;

	return syscall(321, BPF_MAP_DELETE_ELEM, &attr, sizeof(union bpf_attr));
}

static int
gbpf_linux_map_get_next_key(GBPFDriver *self, int map_desc, void *key,
			    void *next_key)
{
	union bpf_attr attr;

	memset(&attr, 0, sizeof(union bpf_attr));

	attr.map_fd = map_desc;
	attr.key = (uint64_t)key;
	attr.next_key = (uint64_t)next_key;

	return syscall(321, BPF_MAP_GET_NEXT_KEY, &attr,
		       sizeof(union bpf_attr));
}

static void
gbpf_linux_close_prog_desc(GBPFDriver *self, int prog_desc)
{
	close(prog_desc);
}

static void
gbpf_linux_close_map_desc(GBPFDriver *self, int map_desc)
{
	close(map_desc);
}

GBPFLinuxDriver *
gbpf_linux_driver_create(void)
{
	GBPFLinuxDriver *driver = calloc(1, sizeof(GBPFLinuxDriver));
	if (!driver) {
		return NULL;
	}

	driver->base.load_prog = gbpf_linux_load_prog;
	driver->base.map_create = gbpf_linux_map_create;
	driver->base.map_update_elem = gbpf_linux_map_update_elem;
	driver->base.map_lookup_elem = gbpf_linux_map_lookup_elem;
	driver->base.map_delete_elem = gbpf_linux_map_delete_elem;
	driver->base.map_get_next_key = gbpf_linux_map_get_next_key;
	driver->base.close_prog_desc = gbpf_linux_close_prog_desc;
	driver->base.close_map_desc = gbpf_linux_close_map_desc;

	return driver;
}

void
gbpf_linux_driver_destroy(GBPFLinuxDriver *driver)
{
	free(driver);
}

#endif
