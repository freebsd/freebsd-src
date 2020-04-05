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

#include "ebpf_map.h"

static void
ebpf_map_dtor(struct ebpf_obj *eo)
{
	struct ebpf_map *em = (struct ebpf_map *)eo;
	em->emt->ops.deinit(em);
}

int
ebpf_map_create(struct ebpf_env *ee, struct ebpf_map **emp,
		struct ebpf_map_attr *attr)
{
	int error;
	struct ebpf_map *em;
	const struct ebpf_map_type *emt;

	if (ee == NULL || emp == NULL || attr == NULL ||
		attr->type >= EBPF_TYPE_MAX ||
		attr->key_size == 0 || attr->value_size == 0 ||
		attr->max_entries == 0)
		return EINVAL;

	emt = ee->ec->map_types[attr->type];
	if (emt == NULL)
		return EINVAL;

	em = ebpf_malloc(sizeof(*em));
	if (em == NULL)
		return ENOMEM;

	ebpf_obj_init(ee, &em->eo);
	em->eo.eo_type	= EBPF_OBJ_TYPE_MAP;
	em->eo.eo_dtor	= ebpf_map_dtor;
	em->emt 	= emt;
	em->key_size	= attr->key_size;
	em->value_size	= attr->value_size;
	em->max_entries	= attr->max_entries;
	em->map_flags	= attr->flags;

	error = emt->ops.init(em, attr);
	if (error != 0) {
		/* 
		 * In here, eo has a reference to ee. We need to
		 * manually release it instead of calling
		 * ebpf_obj_release() since the initialization of
		 * the map is not complete.
		 */
		ebpf_env_release(ee);
		ebpf_free(em);
		return error;
	}

	*emp = em;

	return 0;
}

void *
ebpf_map_lookup_elem(struct ebpf_map *em, void *key)
{
	if (em == NULL || key == NULL)
		return NULL;

	return em->emt->ops.lookup_elem(em, key);
}

int
ebpf_map_lookup_elem_from_user(struct ebpf_map *em, void *key, void *value)
{
	int error;

	if (em == NULL || key == NULL || value == NULL)
		return EINVAL;

	ebpf_epoch_enter();
	error = em->emt->ops.lookup_elem_from_user(em, key, value);
	ebpf_epoch_exit();

	return error;
}

int
ebpf_map_update_elem(struct ebpf_map *em, void *key, void *value,
		     uint64_t flags)
{
	if (em == NULL || key == NULL ||
			value == NULL || flags > EBPF_EXIST)
		return EINVAL;

	return em->emt->ops.update_elem(em, key, value, flags);
}

int
ebpf_map_update_elem_from_user(struct ebpf_map *em, void *key, void *value,
			       uint64_t flags)
{
	int error;

	ebpf_epoch_enter();
	error = em->emt->ops.update_elem_from_user(em, key, value, flags);
	ebpf_epoch_exit();

	return error;
}

int
ebpf_map_delete_elem(struct ebpf_map *em, void *key)
{
	if (em == NULL || key == NULL)
		return EINVAL;

	return em->emt->ops.delete_elem(em, key);
}

int
ebpf_map_delete_elem_from_user(struct ebpf_map *em, void *key)
{
	int error;
	if (em == NULL || key == NULL)
		return EINVAL;

	ebpf_epoch_enter();
	error = em->emt->ops.delete_elem_from_user(em, key);
	ebpf_epoch_exit();

	return error;
}

int
ebpf_map_get_next_key_from_user(struct ebpf_map *em, void *key, void *next_key)
{
	int error;

	/*
	 * key == NULL is valid, because it means "Give me a
	 * first key"
	 */
	if (em == NULL || next_key == NULL)
		return EINVAL;

	ebpf_epoch_enter();
	error = em->emt->ops.get_next_key_from_user(em, key, next_key);
	ebpf_epoch_exit();

	return error;
}

void
ebpf_map_destroy(struct ebpf_map *em)
{
	if (em == NULL)
		return;

	ebpf_obj_release((struct ebpf_obj *)em);
}

const struct ebpf_helper_type eht_map_lookup_elem = {
	.name = "map_lookup_elem",
	.fn = (ebpf_helper_fn)ebpf_map_lookup_elem
};

const struct ebpf_helper_type eht_map_update_elem = {
	.name = "map_update_elem",
	.fn = (ebpf_helper_fn)ebpf_map_update_elem
};

const struct ebpf_helper_type eht_map_delete_elem = {
	.name = "map_delete_elem",
	.fn = (ebpf_helper_fn)ebpf_map_delete_elem
};
