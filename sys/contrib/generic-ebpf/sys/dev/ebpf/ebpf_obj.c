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

#include "ebpf_obj.h"

void
ebpf_obj_init(struct ebpf_env *ee, struct ebpf_obj *eo)
{
	ebpf_assert(ee != NULL && eo != NULL);
	ebpf_env_acquire(ee);
	eo->eo_ee = ee;
	ebpf_refcount_init(&eo->eo_ref, 1);
}

void
ebpf_obj_acquire(struct ebpf_obj *eo)
{
	ebpf_assert(eo != NULL);
	ebpf_refcount_acquire(&eo->eo_ref);
}

void
ebpf_obj_release(struct ebpf_obj *eo)
{
	ebpf_assert(eo != NULL);
	if (ebpf_refcount_release(&eo->eo_ref) != 0) {
		eo->eo_dtor(eo);
		ebpf_env_release(eo->eo_ee);
		ebpf_free(eo);
	}
}
