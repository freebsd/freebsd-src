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

#include <sys/ebpf_vm_isa.h>

#include "gbpf_driver.h"

struct ebpf_map_def;
struct gbpf_elf_walker;
typedef struct gbpf_elf_walker GBPFElfWalker;

struct gbpf_elf_walker {
	GBPFDriver *driver;
	void (*on_prog)(GBPFElfWalker *walker, const char *name,
			struct ebpf_inst *prog, uint32_t prog_len);
	void (*on_map)(GBPFElfWalker *walker, const char *name, int desc,
		       struct ebpf_map_def *map);
	void *data;
};

int gbpf_walk_elf(GBPFElfWalker *walker, GBPFDriver *driver, const char *fname);
