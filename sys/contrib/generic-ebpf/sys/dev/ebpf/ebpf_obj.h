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

#include "ebpf_env.h"

#define EBPF_OBJ_MAX_DEPS 128

struct ebpf_obj;

typedef void (*ebpf_obj_dtor)(struct ebpf_obj*);

enum ebpf_obj_type {
	EBPF_OBJ_TYPE_PROG,
	EBPF_OBJ_TYPE_MAP,
	EBPF_OBJ_TYPE_MAX
};

struct ebpf_obj {
	struct ebpf_env *eo_ee;
	uint32_t eo_ref;
	uint32_t eo_type;
	void (*eo_dtor)(struct ebpf_obj*);
};

void ebpf_obj_init(struct ebpf_env *ee, struct ebpf_obj *eo);
