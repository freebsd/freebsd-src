/*-
 * SPDX-License-Identifier: Apache License 2.0
 *
 * Copyright 2019 Yutaro Hayakawa
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

#include "ebpf_platform.h"
#include "ebpf_queue.h"

struct ebpf_allocator_entry {
	SLIST_ENTRY(ebpf_allocator_entry) entry;
};

struct ebpf_allocator {
	SLIST_HEAD(, ebpf_allocator_entry) free_block;
	SLIST_HEAD(, ebpf_allocator_entry) used_segment;
	ebpf_spinmtx lock;
	uint32_t nblocks;
	uint32_t block_size;
	uint32_t count;
};

int ebpf_allocator_init(struct ebpf_allocator *alloc, uint32_t block_size,
			uint32_t nblocks, int (*ctor)(void *, void *),
			void *arg);
void ebpf_allocator_deinit(struct ebpf_allocator *alloc,
			   void (*dtor)(void *, void *), void *arg);
void *ebpf_allocator_alloc(struct ebpf_allocator *alloc);
void ebpf_allocator_free(struct ebpf_allocator *alloc, void *ptr);
