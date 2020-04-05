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

#include "ebpf_env.h"

int
ebpf_env_create(struct ebpf_env **eep, const struct ebpf_config *ec)
{
	struct ebpf_env *ee;

	if (eep == NULL || ec == NULL)
		return EINVAL;

	ee = ebpf_calloc(1, sizeof(*ee));
	if (ee == NULL)
		return ENOMEM;

	ebpf_refcount_init(&ee->ref, 0);
	ee->ec = ec;

	*eep = ee;

	return 0;
}

int
ebpf_env_destroy(struct ebpf_env *ee)
{
	if (ee->ref != 0)
		return EBUSY;

	ebpf_free(ee);

	return 0;
}

void
ebpf_env_acquire(struct ebpf_env *ee)
{
	ebpf_assert(ee != NULL);
	ebpf_refcount_acquire(&ee->ref);
}

void
ebpf_env_release(struct ebpf_env *ee)
{
	ebpf_assert(ee != NULL);
	ebpf_refcount_release(&ee->ref);
}
