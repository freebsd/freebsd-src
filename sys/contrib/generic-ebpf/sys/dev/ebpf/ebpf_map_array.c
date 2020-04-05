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

struct ebpf_map_array {
	void *array;
};

#define ARRAY_MAP(_map) ((struct ebpf_map_array *)(_map->data))

static void
array_map_deinit(struct ebpf_map *em)
{
	struct ebpf_map_array *ma = em->data;

	ebpf_epoch_wait();

	ebpf_free(ma->array);
	ebpf_free(ma);
}

static void
array_map_deinit_percpu(struct ebpf_map *em)
{
	struct ebpf_map_array *ma = em->data;

	ebpf_epoch_wait();

	for (uint16_t i = 0; i < ebpf_ncpus(); i++)
		ebpf_free(ma[i].array);

	ebpf_free(ma);
}

static int
array_map_init_common(struct ebpf_map_array *ma, struct ebpf_map_attr *attr)
{
	ma->array = ebpf_calloc(attr->max_entries, attr->value_size);
	if (ma->array == NULL)
		return ENOMEM;

	return 0;
}

static int
array_map_init(struct ebpf_map *em, struct ebpf_map_attr *attr)
{
	int error;

	struct ebpf_map_array *ma =
	    ebpf_calloc(1, sizeof(*ma));
	if (ma == NULL)
		return ENOMEM;

	error = array_map_init_common(ma, attr);
	if (error != 0) {
		ebpf_free(ma);
		return error;
	}

	em->data = ma;
	em->percpu = false;

	return 0;
}

static int
array_map_init_percpu(struct ebpf_map *em, struct ebpf_map_attr *attr)
{
	int error;
	uint16_t ncpus = ebpf_ncpus();

	struct ebpf_map_array *ma =
	    ebpf_calloc(ncpus, sizeof(*ma));
	if (ma == NULL)
		return ENOMEM;

	uint16_t i;
	for (i = 0; i < ncpus; i++) {
		error = array_map_init_common(ma + i, attr);
		if (error != 0)
			goto err0;
	}

	em->data = ma;
	em->percpu = true;

	return 0;

err0:
	for (uint16_t j = i; j > 0; j--)
		ebpf_free(ma[i].array);

	ebpf_free(ma);

	return error;
}

static void *
array_map_lookup_elem(struct ebpf_map *em, void *key)
{
	uint32_t k = *(uint32_t *)key;

	if (k >= em->max_entries)
		return NULL;

	return (uint8_t *)(ARRAY_MAP(em)->array) + (em->value_size * k);
}

static int
array_map_lookup_elem_from_user(struct ebpf_map *em, void *key, void *value)
{
	uint32_t k = *(uint32_t *)key;

	if (k >= em->max_entries)
		return EINVAL;

	uint8_t *elem =
	    (uint8_t *)(ARRAY_MAP(em)->array) + (em->value_size * k);
	memcpy((uint8_t *)value, elem, em->value_size);

	return 0;
}

static void *
array_map_lookup_elem_percpu(struct ebpf_map *em, void *key)
{
	uint32_t k = *(uint32_t *)key;

	if (k >= em->max_entries)
		return NULL;

	return (uint8_t *)((ARRAY_MAP(em) + ebpf_curcpu())->array) +
	       (em->value_size * k);
}

static int
array_map_lookup_elem_percpu_from_user(struct ebpf_map *em, void *key,
				       void *value)
{
	uint32_t k = *(uint32_t *)key;

	if (k >= em->max_entries)
		return EINVAL;

	uint8_t *elem;
	for (uint32_t i = 0; i < ebpf_ncpus(); i++) {
		elem = (uint8_t *)((ARRAY_MAP(em) + i)->array) +
		       (em->value_size * k);
		memcpy((uint8_t *)value + em->value_size * i, elem,
		       em->value_size);
	}

	return 0;
}

static int
array_map_update_elem_common(struct ebpf_map *em,
			     struct ebpf_map_array *ma, uint32_t key,
			     void *value, uint64_t flags)
{
	uint8_t *elem = (uint8_t *)ma->array + (em->value_size * key);

	memcpy(elem, value, em->value_size);

	return 0;
}

static inline int
array_map_update_check_attr(struct ebpf_map *em, void *key, void *value,
			    uint64_t flags)
{
	if (flags & EBPF_NOEXIST)
		return EEXIST;

	if (*(uint32_t *)key >= em->max_entries)
		return EINVAL;

	return 0;
}

static int
array_map_update_elem(struct ebpf_map *em, void *key, void *value,
		      uint64_t flags)
{
	int error;
	struct ebpf_map_array *ma = em->data;

	error = array_map_update_check_attr(em, key, value, flags);
	if (error != 0)
		return error;

	return array_map_update_elem_common(em, ma, *(uint32_t *)key,
					    value, flags);
}

static int
array_map_update_elem_percpu(struct ebpf_map *em, void *key, void *value,
			     uint64_t flags)
{
	int error;
	struct ebpf_map_array *ma = em->data;

	error = array_map_update_check_attr(em, key, value, flags);
	if (error != 0)
		return error;

	return array_map_update_elem_common(em, ma + ebpf_curcpu(),
					    *(uint32_t *)key, value, flags);
}

static int
array_map_update_elem_percpu_from_user(struct ebpf_map *map, void *key,
				       void *value, uint64_t flags)
{
	int error;
	struct ebpf_map_array *ma = map->data;

	error = array_map_update_check_attr(map, key, value, flags);
	if (error != 0)
		return error;

	for (uint16_t i = 0; i < ebpf_ncpus(); i++)
		array_map_update_elem_common(map, ma + i,
					     *(uint32_t *)key, value, flags);

	return 0;
}

static int
array_map_delete_elem(struct ebpf_map *map, void *key)
{
	return EINVAL;
}

static int
array_map_get_next_key(struct ebpf_map *map, void *key, void *next_key)
{
	uint32_t k = key ? *(uint32_t *)key : UINT32_MAX;
	uint32_t *nk = (uint32_t *)next_key;

	if (k >= map->max_entries) {
		*nk = 0;
		return 0;
	}

	if (k == map->max_entries - 1)
		return ENOENT;

	*nk = k + 1;
	return 0;
}

const struct ebpf_map_type emt_array = {
	.name = "array",
	.ops = {
		.init = array_map_init,
		.update_elem = array_map_update_elem,
		.lookup_elem = array_map_lookup_elem,
		.delete_elem = array_map_delete_elem,
		.update_elem_from_user = array_map_update_elem,
		.lookup_elem_from_user = array_map_lookup_elem_from_user,
		.delete_elem_from_user = array_map_delete_elem,
		.get_next_key_from_user = array_map_get_next_key,
		.deinit = array_map_deinit
	}
};

const struct ebpf_map_type emt_percpu_array = {
	.name = "percpu_array",
	.ops = {
		.init = array_map_init_percpu,
		.update_elem = array_map_update_elem_percpu,
		.lookup_elem = array_map_lookup_elem_percpu,
		.delete_elem = array_map_delete_elem, // delete is anyway invalid
		.update_elem_from_user = array_map_update_elem_percpu_from_user,
		.lookup_elem_from_user = array_map_lookup_elem_percpu_from_user,
		.delete_elem_from_user = array_map_delete_elem, // delete is anyway invalid
		.get_next_key_from_user = array_map_get_next_key,
		.deinit = array_map_deinit_percpu
	}
};
