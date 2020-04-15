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

#include "ebpf_prog.h"
#include "ebpf_map.h"

static void
ebpf_prog_dtor(struct ebpf_obj *eo)
{
	struct ebpf_prog *ep = (struct ebpf_prog *)eo;

	for (uint32_t i = 0; i < ep->ndep_maps; i++)
		ebpf_obj_release((struct ebpf_obj *)ep->dep_maps[i]);

	ebpf_free(ep->prog);
}

int
ebpf_prog_create(struct ebpf_env *ee, struct ebpf_prog **epp,
		 struct ebpf_prog_attr *attr)
{
	struct ebpf_prog *ep;
	const struct ebpf_prog_type *ept;

	if (ee == NULL || epp == NULL || attr == NULL ||
			attr->type >= EBPF_TYPE_MAX ||
			attr->prog == NULL || attr->prog_len == 0)
		return EINVAL;

	ept = ee->ec->prog_types[attr->type];
	if (ept == NULL)
		return EINVAL;

	ep = ebpf_malloc(sizeof(*ep));
	if (ep == NULL)
		return ENOMEM;

	ep->prog = ebpf_malloc(attr->prog_len);
	if (ep->prog == NULL) {
		ebpf_free(ep);
		return ENOMEM;
	}

	ebpf_obj_init(ee, &ep->eo);
	ep->eo.eo_type	= EBPF_OBJ_TYPE_PROG;
	ep->eo.eo_dtor 	= ebpf_prog_dtor;
	ep->ept 	= ept;
	ep->ndep_maps 	= 0;
	ep->prog_len 	= attr->prog_len;
	ep->type	= attr->type;

	memcpy(ep->prog, attr->prog, attr->prog_len);
	memset(ep->dep_maps, 0,
			sizeof(ep->dep_maps[0]) * EBPF_PROG_MAX_ATTACHED_MAPS);

	*epp = ep;

	return 0;
}

void
ebpf_prog_destroy(struct ebpf_prog *ep)
{
	if (ep == NULL)
		return;

	ebpf_obj_release(&ep->eo);
}

int
ebpf_prog_attach_map(struct ebpf_prog *ep, struct ebpf_map *em)
{
	if (ep == NULL || em == NULL)
		return EINVAL;

	/* Cannot attach the map from different ebpf_env */
	if (ep->eo.eo_ee != em->eo.eo_ee)
		return EINVAL;

	if (ep->ndep_maps >= EBPF_PROG_MAX_ATTACHED_MAPS)
		return EBUSY;

	for (uint32_t i = 0; i < EBPF_PROG_MAX_ATTACHED_MAPS; i++) {
		if (ep->dep_maps[i] != NULL) {
			if (ep->dep_maps[i] == em)
				return EEXIST;
		} else {
			ebpf_obj_acquire((struct ebpf_obj *)em);
			ep->dep_maps[ep->ndep_maps++] = em;
			break;
		}
	}

	return 0;
}
