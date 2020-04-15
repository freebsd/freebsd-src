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

#pragma once

#ifdef __FreeBSD__
#include "ebpf_dev_freebsd.h"
#elif defined(linux)
#include <ebpf_dev_linux.h>
#else
#error Unsupported platform
#endif

struct ebpf_env;
struct ebpf_obj;

/*
 * Prototypes of platform dependent functions.
 */
bool is_ebpf_objfile(ebpf_file *fp);
int ebpf_fopen(ebpf_thread *td, ebpf_file **fp, int *fd,
	       struct ebpf_obj *data);
struct ebpf_obj *ebpf_file_get_data(ebpf_file *f);
int ebpf_fget(ebpf_thread *td, int fd, ebpf_file **f);
int ebpf_fdrop(ebpf_file *f, ebpf_thread *td);
int ebpf_copyin(const void *uaddr, void *kaddr, size_t len);
int ebpf_copyout(const void *kaddr, void *uaddr, size_t len);
ebpf_thread *ebpf_curthread(void);
int ebpf_ioctl(struct ebpf_env *, uint32_t cmd, void *data, ebpf_thread *td);

int ebpf_dev_init(void);
int ebpf_dev_fini(void);

extern const struct ebpf_config fbsd_ebpf_config;
