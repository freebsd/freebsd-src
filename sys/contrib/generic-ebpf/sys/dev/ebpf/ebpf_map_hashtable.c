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
#include "ebpf_allocator.h"
#include "ebpf_util.h"

struct ebpf_map_hashtable;

/*
 * hashtable_map's element. Actual value is following to
 * variable length key.
 */
struct hash_elem {
	EBPF_EPOCH_LIST_ENTRY(hash_elem) elem;
	uint8_t key[0];
	/* uint8_t value[value_size]; Instance of value in normal map case */
	/* uint8_t **valuep; Pointer to percpu value in percpu map case */
};

struct hash_bucket {
	EBPF_EPOCH_LIST_HEAD(, hash_elem) head;
	ebpf_spinmtx lock;
};

struct ebpf_map_hashtable {
	uint32_t elem_size;
	uint32_t key_size;   /* round upped key size */
	uint32_t value_size; /* round uppped value size */
	uint32_t nbuckets;
	struct hash_bucket *buckets;
	struct hash_elem **pcpu_extra_elems;
	struct ebpf_allocator allocator;
};

#define HASH_ELEM_VALUE(_hash_mapp, _elemp) ((_elemp)->key + (_hash_mapp)->key_size)
#define HASH_ELEM_PERCPU_VALUE(_hash_mapp, _elemp, _cpuid)                     \
	(*((uint8_t **)HASH_ELEM_VALUE(_hash_mapp, _elemp)) +                  \
	 (_hash_mapp)->value_size * (_cpuid))
#define HASH_ELEM_CURCPU_VALUE(_hash_mapp, _elemp)                             \
	HASH_ELEM_PERCPU_VALUE(_hash_mapp, _elemp, ebpf_curcpu())
#define HASH_BUCKET_LOCK(_bucketp) ebpf_spinmtx_lock(&_bucketp->lock);
#define HASH_BUCKET_UNLOCK(_bucketp) ebpf_spinmtx_unlock(&_bucketp->lock);

static struct hash_bucket *
get_hash_bucket(struct ebpf_map_hashtable *hash_map, uint32_t hash)
{
	return &hash_map->buckets[hash & (hash_map->nbuckets - 1)];
}

static struct hash_elem *
get_hash_elem(struct hash_bucket *bucket, void *key, uint32_t key_size)
{
	struct hash_elem *elem;
	EBPF_EPOCH_LIST_FOREACH(elem, &bucket->head, elem)
	{
		if (memcmp(elem->key, key, key_size) == 0)
			return elem;
	}
	return NULL;
}

static struct hash_elem *
get_extra_elem(struct ebpf_map_hashtable *hash_map, struct hash_elem *elem)
{
	struct hash_elem *tmp;
	tmp = hash_map->pcpu_extra_elems[ebpf_curcpu()];
	hash_map->pcpu_extra_elems[ebpf_curcpu()] = elem;
	return tmp;
}

static int
check_update_flags(struct ebpf_map_hashtable *hash_map, struct hash_elem *elem,
		   uint64_t flags)
{
	if (elem) {
		if (flags & EBPF_NOEXIST)
			return EEXIST;
	} else {
		if (flags & EBPF_EXIST)
			return ENOENT;
	}

	return 0;
}

static int
percpu_elem_ctor(void *mem, void *arg)
{
	uint8_t **valuep;
	struct hash_elem *elem = mem;
	struct ebpf_map_hashtable *hash_map = arg;

	valuep = (uint8_t **)HASH_ELEM_VALUE(hash_map, elem);
	*valuep = ebpf_calloc(ebpf_ncpus(), hash_map->value_size);
	if (*valuep == NULL)
		return ENOMEM;

	return 0;
}

static void
percpu_elem_dtor(void *mem, void *arg)
{
	uint8_t **valuep;
	struct hash_elem *elem = mem;
	struct ebpf_map_hashtable *hash_map = arg;

	valuep = (uint8_t **)HASH_ELEM_VALUE(hash_map, elem);
	ebpf_free(*valuep);
}

static bool
is_percpu(struct ebpf_map *map)
{
	if (map->emt == &emt_percpu_hashtable)
		return true;

	return false;
}

static int
hashtable_map_init(struct ebpf_map *map, struct ebpf_map_attr *attr)
{
	int error;

	map->percpu = is_percpu(map);

	/* Check overflow */
	if (ebpf_roundup(attr->key_size, 8) + ebpf_roundup(attr->value_size, 8) +
		sizeof(struct hash_elem) >
	    UINT32_MAX) {
		return E2BIG;
	}

	struct ebpf_map_hashtable *hash_map = ebpf_calloc(1, sizeof(*hash_map));
	if (hash_map == NULL)
		return ENOMEM;

	/*
	 * Roundup key size and value size for efficiency.
	 * This affects sizeof element. Never allow users
	 * to see "padded" memory region.
	 *
	 * Here we cache the "internal" key_size and value_size.
	 * For getting the "real" key_size and value_size, please
	 * use values stored in struct ebpf_map.
	 */
	hash_map->key_size = ebpf_roundup(attr->key_size, 8);
	hash_map->value_size = ebpf_roundup(attr->value_size, 8);

	if (map->percpu)
		hash_map->elem_size = hash_map->key_size + sizeof(uint8_t *) +
				      sizeof(struct hash_elem);
	else
		hash_map->elem_size = hash_map->key_size +
				      hash_map->value_size +
				      sizeof(struct hash_elem);

	/*
	 * Roundup number of buckets to power of two.
	 * This improbes performance, because we don't have to
	 * use slow moduro opearation.
	 */
	hash_map->nbuckets = ebpf_roundup_pow_of_two(attr->max_entries);
	hash_map->buckets =
	    ebpf_calloc(hash_map->nbuckets, sizeof(struct hash_bucket));
	if (hash_map->buckets == NULL) {
		error = ENOMEM;
		goto err0;
	}

	for (uint32_t i = 0; i < hash_map->nbuckets; i++) {
		EBPF_EPOCH_LIST_INIT(&hash_map->buckets[i].head);
		ebpf_spinmtx_init(&hash_map->buckets[i].lock,
			      "ebpf_hashtable_map bucket lock");
	}

	if (map->percpu) {
		error = ebpf_allocator_init(&hash_map->allocator,
					    hash_map->elem_size, attr->max_entries,
					    percpu_elem_ctor, hash_map);
		if (error != 0)
			goto err1;
	} else {
		error = ebpf_allocator_init(
		    &hash_map->allocator, hash_map->elem_size,
		    attr->max_entries + ebpf_ncpus(), NULL, NULL);
		if (error != 0)
			goto err1;

		hash_map->pcpu_extra_elems =
		    ebpf_calloc(ebpf_ncpus(), sizeof(struct hash_elem *));
		if (hash_map->pcpu_extra_elems == NULL) {
			error = ENOMEM;
			goto err2;
		}

		/*
		 * Reserve percpu extra map element in here.
		 * These elemens are useful to update existing
		 * map element. Since updating is running at
		 * critical section, we don't require any lock
		 * to take this element.
		 */
		for (uint16_t i = 0; i < ebpf_ncpus(); i++) {
			hash_map->pcpu_extra_elems[i] =
			    ebpf_allocator_alloc(&hash_map->allocator);
			ebpf_assert(hash_map->pcpu_extra_elems[i]);
		}
	}

	map->data = hash_map;

	return 0;

err2:
	ebpf_allocator_deinit(&hash_map->allocator,
			      map->percpu ? percpu_elem_dtor : NULL,
			      map->percpu ? hash_map : NULL);
err1:
	ebpf_free(hash_map->buckets);
err0:
	ebpf_free(hash_map);
	return error;
}

static void
hashtable_map_deinit(struct ebpf_map *map)
{
	struct ebpf_map_hashtable *hash_map = map->data;

	/*
	 * Wait for current readers
	 */
	ebpf_epoch_wait();

	if (!map->percpu)
		for (uint16_t i = 0; i < ebpf_ncpus(); i++)
			ebpf_allocator_free(&hash_map->allocator,
					hash_map->pcpu_extra_elems[i]);

	struct hash_elem *elem;
	for (uint32_t i = 0; i < hash_map->nbuckets; i++) {
		while (!EBPF_EPOCH_LIST_EMPTY(&hash_map->buckets[i].head)) {
			elem =
			    EBPF_EPOCH_LIST_FIRST(&hash_map->buckets[i].head,
              struct hash_elem, elem);
			if (elem != NULL) {
				EBPF_EPOCH_LIST_REMOVE(elem, elem);
				ebpf_allocator_free(&hash_map->allocator, elem);
			}
		}
	}

	ebpf_allocator_deinit(&hash_map->allocator,
			      map->percpu ? percpu_elem_dtor : NULL,
			      map->percpu ? hash_map : NULL);

	for (uint32_t i = 0; i < hash_map->nbuckets; i++)
		ebpf_spinmtx_destroy(&hash_map->buckets[i].lock);

	if (!map->percpu)
		ebpf_free(hash_map->pcpu_extra_elems);

	ebpf_free(hash_map->buckets);
	ebpf_free(hash_map);
}

static void *
hashtable_map_lookup_elem(struct ebpf_map *map, void *key)
{
	uint32_t hash = ebpf_jenkins_hash(key, map->key_size, 0);
	struct ebpf_map_hashtable *hash_map;
	struct hash_bucket *bucket;
	struct hash_elem *elem;

	hash_map = map->data;
	bucket = get_hash_bucket(hash_map, hash);
	elem = get_hash_elem(bucket, key, map->key_size);
	if (elem == NULL)
		return NULL;

	return map->percpu ? HASH_ELEM_CURCPU_VALUE(hash_map, elem)
			   : HASH_ELEM_VALUE(hash_map, elem);
}

static int
hashtable_map_lookup_elem_from_user(struct ebpf_map *map, void *key,
				    void *value)
{
	uint32_t hash = ebpf_jenkins_hash(key, map->key_size, 0);
	struct ebpf_map_hashtable *hash_map;
	struct hash_bucket *bucket;
	struct hash_elem *elem;

	hash_map = map->data;
	bucket = get_hash_bucket(hash_map, hash);
	elem = get_hash_elem(bucket, key, map->key_size);
	if (elem == NULL)
		return ENOENT;

	memcpy(value, HASH_ELEM_VALUE(hash_map, elem), map->value_size);

	return 0;
}

static int
hashtable_map_lookup_elem_percpu_from_user(struct ebpf_map *map, void *key,
					   void *value)
{
	uint32_t hash = ebpf_jenkins_hash(key, map->key_size, 0);
	struct ebpf_map_hashtable *hash_map;
	struct hash_bucket *bucket;
	struct hash_elem *elem;

	hash_map = map->data;
	bucket = get_hash_bucket(hash_map, hash);
	elem = get_hash_elem(bucket, key, map->key_size);
	if (elem == NULL)
		return ENOENT;

	for (uint16_t i = 0; i < ebpf_ncpus(); i++)
		memcpy((uint8_t *)value + map->value_size * i,
		       HASH_ELEM_PERCPU_VALUE(hash_map, elem, i),
		       map->value_size);

	return 0;
}

static int
hashtable_map_update_elem(struct ebpf_map *map, void *key, void *value,
			  uint64_t flags)
{
	int error = 0;
	uint32_t hash = ebpf_jenkins_hash(key, map->key_size, 0);
	struct hash_bucket *bucket;
	struct hash_elem *old_elem, *new_elem;
	struct ebpf_map_hashtable *hash_map = map->data;

	bucket = get_hash_bucket(hash_map, hash);

	HASH_BUCKET_LOCK(bucket);

	old_elem = get_hash_elem(bucket, key, map->key_size);
	error = check_update_flags(hash_map, old_elem, flags);
	if (error != 0)
		goto err0;

	if (old_elem != NULL) {
		/*
		 * In case of updating existing element, we can
		 * use percpu extra elements and swap it with old
		 * element. This avoids take lock of memory allocator.
		 */
		new_elem = get_extra_elem(hash_map, old_elem);
	} else {
		new_elem = ebpf_allocator_alloc(&hash_map->allocator);
		if (!new_elem) {
			error = EBUSY;
			goto err0;
		}
	}

	memcpy(new_elem->key, key, map->key_size);
	memcpy(HASH_ELEM_VALUE(hash_map, new_elem), value, map->value_size);

	EBPF_EPOCH_LIST_INSERT_HEAD(&bucket->head, new_elem, elem);
	if (old_elem != NULL)
		EBPF_EPOCH_LIST_REMOVE(old_elem, elem);

err0:
	HASH_BUCKET_UNLOCK(bucket);
	return error;
}

static int
hashtable_map_update_elem_percpu(struct ebpf_map *map, void *key, void *value,
				 uint64_t flags)
{
	int error = 0;
	uint32_t hash = ebpf_jenkins_hash(key, map->key_size, 0);
	struct hash_bucket *bucket;
	struct hash_elem *old_elem, *new_elem;
	struct ebpf_map_hashtable *hash_map = map->data;

	bucket = get_hash_bucket(hash_map, hash);

	HASH_BUCKET_LOCK(bucket);

	old_elem = get_hash_elem(bucket, key, map->key_size);
	error = check_update_flags(hash_map, old_elem, flags);
	if (error != 0)
		goto err0;

	if (old_elem != NULL) {
		memcpy(HASH_ELEM_CURCPU_VALUE(hash_map, old_elem), value,
		       map->value_size);
	} else {
		new_elem = ebpf_allocator_alloc(&hash_map->allocator);
		if (new_elem == NULL) {
			error = EBUSY;
			goto err0;
		}

		memcpy(new_elem->key, key, map->key_size);
		memcpy(HASH_ELEM_CURCPU_VALUE(hash_map, new_elem), value,
		       map->value_size);
		EBPF_EPOCH_LIST_INSERT_HEAD(&bucket->head, new_elem, elem);
	}

err0:
	HASH_BUCKET_UNLOCK(bucket);
	return error;
}

static int
hashtable_map_update_elem_percpu_from_user(struct ebpf_map *map, void *key,
					   void *value, uint64_t flags)
{
	int error = 0;
	uint32_t hash = ebpf_jenkins_hash(key, map->key_size, 0);
	struct hash_bucket *bucket;
	struct hash_elem *old_elem, *new_elem;
	struct ebpf_map_hashtable *hash_map = map->data;

	bucket = get_hash_bucket(hash_map, hash);

	HASH_BUCKET_LOCK(bucket);

	old_elem = get_hash_elem(bucket, key, map->key_size);
	error = check_update_flags(hash_map, old_elem, flags);
	if (error != 0)
		goto err0;

	if (old_elem != NULL) {
		for (uint16_t i = 0; i < ebpf_ncpus(); i++)
			memcpy(HASH_ELEM_PERCPU_VALUE(hash_map, old_elem, i),
			       value, map->value_size);
	} else {
		new_elem = ebpf_allocator_alloc(&hash_map->allocator);
		if (new_elem == NULL) {
			error = EBUSY;
			goto err0;
        }

		for (uint16_t i = 0; i < ebpf_ncpus(); i++)
			memcpy(HASH_ELEM_PERCPU_VALUE(hash_map, new_elem, i),
			       value, map->value_size);

		memcpy(new_elem->key, key, map->key_size);
		EBPF_EPOCH_LIST_INSERT_HEAD(&bucket->head, new_elem, elem);
	}

err0:
	HASH_BUCKET_UNLOCK(bucket);
	return error;
}

static int
hashtable_map_delete_elem(struct ebpf_map *map, void *key)
{
	uint32_t hash = ebpf_jenkins_hash(key, map->key_size, 0);
	struct ebpf_map_hashtable *hash_map = map->data;
	struct hash_bucket *bucket;
	struct hash_elem *elem;

	bucket = get_hash_bucket(hash_map, hash);

	HASH_BUCKET_LOCK(bucket);

	elem = get_hash_elem(bucket, key, map->key_size);
	if (elem != NULL)
		EBPF_EPOCH_LIST_REMOVE(elem, elem);

	HASH_BUCKET_UNLOCK(bucket);

	/*
	 * Just return element to memory allocator without any
	 * synchronization. This is safe, because ebpf_allocator
	 * never calls free().
	 */
	if (elem != NULL)
		ebpf_allocator_free(&hash_map->allocator, elem);

	return 0;
}

static int
hashtable_map_get_next_key(struct ebpf_map *map, void *key, void *next_key)
{
	struct ebpf_map_hashtable *hash_map = map->data;
	struct hash_bucket *bucket;
	struct hash_elem *elem, *next_elem;
	uint32_t hash = 0;
	uint32_t i = 0;

	if (key == NULL)
		goto get_first_key;

	hash = ebpf_jenkins_hash(key, map->key_size, 0);
	bucket = get_hash_bucket(hash_map, hash);
	elem = get_hash_elem(bucket, key, map->key_size);
	if (elem == NULL)
		goto get_first_key;

	next_elem = EBPF_EPOCH_LIST_NEXT(elem, elem);
	if (next_elem != NULL) {
		memcpy(next_key, next_elem->key, map->key_size);
		return 0;
	}

	i = (hash & (hash_map->nbuckets - 1)) + 1;

get_first_key:
	for (; i < hash_map->nbuckets; i++) {
		bucket = hash_map->buckets + i;
		EBPF_EPOCH_LIST_FOREACH(elem, &bucket->head, elem)
		{
			memcpy(next_key, elem->key, map->key_size);
			return 0;
		}
	}

	return ENOENT;
}

const struct ebpf_map_type emt_hashtable = {
	.name = "hashtable",
	.ops = {
		.init = hashtable_map_init,
		.update_elem = hashtable_map_update_elem,
		.lookup_elem = hashtable_map_lookup_elem,
		.delete_elem = hashtable_map_delete_elem,
		.update_elem_from_user = hashtable_map_update_elem,
		.lookup_elem_from_user = hashtable_map_lookup_elem_from_user,
		.delete_elem_from_user = hashtable_map_delete_elem,
		.get_next_key_from_user = hashtable_map_get_next_key,
		.deinit = hashtable_map_deinit
	}
};

const struct ebpf_map_type emt_percpu_hashtable = {
	.name = "percpu_hashtable",
	.ops = {
		.init = hashtable_map_init,
		.update_elem = hashtable_map_update_elem_percpu,
		.lookup_elem = hashtable_map_lookup_elem,
		.delete_elem = hashtable_map_delete_elem,
		.update_elem_from_user = hashtable_map_update_elem_percpu_from_user,
		.lookup_elem_from_user = hashtable_map_lookup_elem_percpu_from_user,
		.delete_elem_from_user = hashtable_map_delete_elem,
		.get_next_key_from_user = hashtable_map_get_next_key,
		.deinit = hashtable_map_deinit
	}
};
