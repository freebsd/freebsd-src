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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <sys/ebpf_dev.h>

#include <gbpf_driver.h>
#include <ebpf_dev_driver.h>

static int
ebpf_dev_load_prog(GBPFDriver *self, uint16_t prog_type, void *prog,
		   uint32_t prog_len)
{
	EBPFDevDriver *driver = (EBPFDevDriver *)self;
	int fd, error;

	union ebpf_req req;
	req.prog_fdp = &fd;
	req.prog_type = prog_type;
	req.prog_len = prog_len;
	req.prog = prog;

	error = ioctl(driver->ebpf_fd, EBPFIOC_LOAD_PROG, &req);
	if (error) {
		return error;
	}

	return fd;
}

static int
ebpf_dev_map_create(GBPFDriver *self, uint16_t type, uint32_t key_size,
		    uint32_t value_size, uint32_t max_entries,
		    uint32_t map_flags)
{
	EBPFDevDriver *driver = (EBPFDevDriver *)self;
	int fd, error;

	union ebpf_req req;
	req.map_fdp = &fd;
	req.map_type = type;
	req.key_size = key_size;
	req.value_size = value_size;
	req.max_entries = max_entries;
	req.map_flags = map_flags;

	error = ioctl(driver->ebpf_fd, EBPFIOC_MAP_CREATE, &req);
	if (error) {
		return error;
	}

	return fd;
}

static int
ebpf_dev_map_update_elem(GBPFDriver *self, int map_desc, void *key, void *value,
			 uint64_t flags)
{
	EBPFDevDriver *driver = (EBPFDevDriver *)self;
	union ebpf_req req;
	req.map_fd = map_desc;
	req.key = key;
	req.value = value;
	req.flags = flags;

	return ioctl(driver->ebpf_fd, EBPFIOC_MAP_UPDATE_ELEM, &req);
}

static int
ebpf_dev_map_lookup_elem(GBPFDriver *self, int map_desc, void *key, void *value)
{
	EBPFDevDriver *driver = (EBPFDevDriver *)self;

	union ebpf_req req;
	req.map_fd = map_desc;
	req.key = key;
	req.value = value;

	return ioctl(driver->ebpf_fd, EBPFIOC_MAP_LOOKUP_ELEM, &req);
}

static int
ebpf_dev_map_delete_elem(GBPFDriver *self, int map_desc, void *key)
{
	EBPFDevDriver *driver = (EBPFDevDriver *)self;
	union ebpf_req req;
	req.map_fd = map_desc;
	req.key = key;

	return ioctl(driver->ebpf_fd, EBPFIOC_MAP_DELETE_ELEM, &req);
}

static int
ebpf_dev_map_get_next_key(GBPFDriver *self, int map_desc, void *key,
			  void *next_key)
{
	EBPFDevDriver *driver = (EBPFDevDriver *)self;
	union ebpf_req req;
	req.map_fd = map_desc;
	req.key = key;
	req.next_key = next_key;

	return ioctl(driver->ebpf_fd, EBPFIOC_MAP_GET_NEXT_KEY, &req);
}

static int32_t
ebpf_dev_get_map_type_by_name(GBPFDriver *self, const char *name)
{
	int error;
	EBPFDevDriver *driver = (EBPFDevDriver *)self;
	struct ebpf_map_type_info info;
	union ebpf_req req;

	for (uint16_t i = 0; i < EBPF_TYPE_MAX; i++) {
		req.mt_id = i;
		req.mt_info = &info;
		error = ioctl(driver->ebpf_fd, EBPFIOC_GET_MAP_TYPE_INFO, &req);
		if (error) {
			return -1;
		}

		if (strcmp(name, info.name) == 0) {
			return i;
		}
	}

	return -1;
}

static int32_t
ebpf_dev_get_prog_type_by_name(GBPFDriver *self, const char *name)
{
	int error;
	EBPFDevDriver *driver = (EBPFDevDriver *)self;
	struct ebpf_prog_type_info info;
	union ebpf_req req;

	for (uint16_t i = 0; i < EBPF_TYPE_MAX; i++) {
		req.pt_id = i;
		req.pt_info = &info;
		error = ioctl(driver->ebpf_fd, EBPFIOC_GET_MAP_TYPE_INFO, &req);
		if (error) {
			return -1;
		}

		if (strcmp(name, info.name) == 0) {
			return (int32_t)i;
		}
	}

	return -1;
}

static int
ebpf_dev_attach_probe(GBPFDriver *self, int prog_desc, const char *tracer,
    const char *provider, const char *module, const char *function,
    const char *name, int jit)
{
	union ebpf_req req;
	EBPFDevDriver *driver = (EBPFDevDriver *)self;
	int error;
	ebpf_probe_id_t id;

	memset(&req, 0, sizeof(req));
	strlcpy(req.probe_by_name.name.tracer, tracer, sizeof(req.probe_by_name.name.tracer));
	strlcpy(req.probe_by_name.name.provider, provider, sizeof(req.probe_by_name.name.provider));
	strlcpy(req.probe_by_name.name.module, module, sizeof(req.probe_by_name.name.module));
	strlcpy(req.probe_by_name.name.function, function, sizeof(req.probe_by_name.name.function));
	strlcpy(req.probe_by_name.name.name, name, sizeof(req.probe_by_name.name.name));

	error = ioctl(driver->ebpf_fd, EBPFIOC_PROBE_BY_NAME, &req);
	if (error != 0) {
		return (error);
	}

	id = req.probe_by_name.info.id;

	req.attach.prog_fd = prog_desc;
	req.attach.probe_id = id;
	req.attach.jit = jit;

	return ioctl(driver->ebpf_fd, EBPFIOC_ATTACH_PROBE, &req);
}

static void
ebpf_dev_close_prog_desc(GBPFDriver *self, int prog_desc)
{
	close(prog_desc);
}

static void
ebpf_dev_close_map_desc(GBPFDriver *self, int map_desc)
{
	close(map_desc);
}

static int
ebpf_dev_get_probe_info(GBPFDriver *self, ebpf_probe_id_t id, struct ebpf_probe_info *info)
{
	union ebpf_req req;
	EBPFDevDriver *driver = (EBPFDevDriver *)self;
	int error;

	req.probe_iter.prev_id = id;
	error = ioctl(driver->ebpf_fd, EBPFIOC_PROBE_ITER, &req);
	if (error == 0) {
		memcpy(info, &req.probe_iter.info, sizeof(*info));
	}

	return (error);
}

EBPFDevDriver *
ebpf_dev_driver_create(void)
{
	EBPFDevDriver *driver = malloc(sizeof(EBPFDevDriver));
	if (!driver) {
		return NULL;
	}

	driver->ebpf_fd = open("/dev/ebpf", O_RDWR);
	if (driver->ebpf_fd < 0) {
		free(driver);
		return NULL;
	}

	driver->base.load_prog = ebpf_dev_load_prog;
	driver->base.map_create = ebpf_dev_map_create;
	driver->base.map_update_elem = ebpf_dev_map_update_elem;
	driver->base.map_lookup_elem = ebpf_dev_map_lookup_elem;
	driver->base.map_delete_elem = ebpf_dev_map_delete_elem;
	driver->base.map_get_next_key = ebpf_dev_map_get_next_key;
	driver->base.get_map_type_by_name = ebpf_dev_get_map_type_by_name;
	driver->base.get_prog_type_by_name = ebpf_dev_get_prog_type_by_name;
	driver->base.close_prog_desc = ebpf_dev_close_prog_desc;
	driver->base.close_map_desc = ebpf_dev_close_map_desc;
	driver->base.attach_probe = ebpf_dev_attach_probe;
	driver->base.get_probe_info = ebpf_dev_get_probe_info;

	return driver;
}

void
ebpf_dev_driver_destroy(EBPFDevDriver *driver)
{
	close(driver->ebpf_fd);
	free(driver);
}
