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

#include <sys/ebpf.h>
#include <sys/ebpf_defines.h>
#include <sys/ebpf_param.h>

#define SECTION(name) __attribute__((section(name)))

struct ebpf_map_def {
	uint32_t type;
	uint32_t key_size;
	uint32_t value_size;
	uint32_t max_entries;
	uint32_t flags;
	uint32_t inner_map_idx;
	uint32_t numa_node;
};

#define EBPF_DEFINE_MAP(_name, _type, _key_size, _value_size, _max_entries,    \
			_flags)                                                \
	SECTION("maps")                                                        \
	struct ebpf_map_def _name = {.type = _type,            \
				     .key_size = _key_size,                    \
				     .value_size = _value_size,                \
				     .max_entries = _max_entries,              \
				     .flags = _flags};

#define EBPF_FUNC(RETTYPE, PREFIX, NAME, ...)                                          \
	RETTYPE(*PREFIX##NAME)                                                         \
	(__VA_ARGS__) __attribute__((__unused__)) =                            \
	    (RETTYPE(*)(__VA_ARGS__))EBPF_FUNC_##NAME

// Definitions of common external functions
static EBPF_FUNC(int, ebpf_, map_update_elem, struct ebpf_map_def *, void *,
		 void *, uint64_t);
static EBPF_FUNC(void *, ebpf_, map_lookup_elem, struct ebpf_map_def *, void *);
static EBPF_FUNC(void *, ebpf_, map_path_lookup, struct ebpf_map_def *, void **);
static EBPF_FUNC(int, ebpf_, map_delete_elem, struct ebpf_map_def *, void *);
static EBPF_FUNC(int, ebpf_, map_enqueue, struct ebpf_map_def *, void *);
static EBPF_FUNC(int, ebpf_, map_dequeue, struct ebpf_map_def *, void *);

