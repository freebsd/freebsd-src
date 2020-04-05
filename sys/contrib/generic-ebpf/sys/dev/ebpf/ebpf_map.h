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

#include "ebpf_obj.h"

struct ebpf_map {
	struct ebpf_obj eo;
	const struct ebpf_map_type *emt;
	uint32_t key_size;
	uint32_t value_size;
	uint32_t map_flags;
	uint32_t max_entries;
	bool percpu;
	void *data;
};

#define EO2EM(eo) \
	(eo != NULL && eo->eo_type == EBPF_OBJ_TYPE_MAP ? \
   (struct ebpf_map *)eo : NULL)
