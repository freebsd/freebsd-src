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

#define EBPF_NAME_MAX 64
#define EBPF_TYPE_MAX 64
#define EBPF_PROG_MAX_ATTACHED_MAPS 64

#define EBPF_PSEUDO_MAP_DESC 1

#define EBPF_STACK_SIZE 512

#define EBPF_ACTION_RESTART	2

enum ebpf_map_update_flags {
	EBPF_ANY = 0,
	EBPF_NOEXIST,
	EBPF_EXIST,
	__EBPF_MAP_UPDATE_FLAGS_MAX
};

#ifdef _KERNEL
struct ebpf_obj;
struct ebpf_prog;
struct ebpf_map;
struct ebpf_env;

struct ebpf_prog_attr {
	uint32_t type;
	struct ebpf_inst *prog;
	uint32_t prog_len;
	void *data; /* private data */
};

struct ebpf_map_attr {
	uint32_t type;
	uint32_t key_size;
	uint32_t value_size;
	uint32_t max_entries;
	uint32_t flags;
};

struct ebpf_map_ops {
	int (*init)(struct ebpf_map *em, struct ebpf_map_attr *attr);
	void* (*lookup_elem)(struct ebpf_map *em, void *key);
	int (*update_elem)(struct ebpf_map *em, void *key, void *value, uint64_t flags);
	int (*delete_elem)(struct ebpf_map *em, void *key);
	int (*lookup_elem_from_user)(struct ebpf_map *em, void *key, void *value);
	int (*update_elem_from_user)(struct ebpf_map *em, void *key, void *value, uint64_t flags);
	int (*delete_elem_from_user)(struct ebpf_map *em, void *key);
	int (*get_next_key_from_user)(struct ebpf_map *em, void *key, void *next_key);
	void (*deinit)(struct ebpf_map *em);
};

struct ebpf_map_type {
	char name[EBPF_NAME_MAX];
	struct ebpf_map_ops ops;
};

typedef uint64_t (*ebpf_helper_fn)(uint64_t arg0, uint64_t arg1,
		uint64_t arg2, uint64_t arg3, uint64_t arg4);

struct ebpf_helper_type {
	char name[EBPF_NAME_MAX];
	ebpf_helper_fn fn;
	uint32_t id;
};

struct ebpf_prog_ops {
	bool (*is_map_usable)(struct ebpf_map_type *emt);
	bool (*is_helper_usable)(struct ebpf_helper_type *eht);
};

struct ebpf_prog_type {
	char name[EBPF_NAME_MAX];
	struct ebpf_prog_ops ops;
};

struct ebpf_preprocessor_ops {
	struct ebpf_map *(*resolve_map_desc)(int32_t upper, int32_t lower, void *data);
};

struct ebpf_preprocessor_type {
	char name[EBPF_NAME_MAX];
	struct ebpf_preprocessor_ops ops;
};

struct ebpf_config {
	const struct ebpf_prog_type *prog_types[EBPF_TYPE_MAX];
	const struct ebpf_map_type *map_types[EBPF_TYPE_MAX];
	const struct ebpf_helper_type *helper_types[EBPF_TYPE_MAX];
	const struct ebpf_preprocessor_type *preprocessor_type;
};

int ebpf_init(void);
int ebpf_deinit(void);

int ebpf_env_create(struct ebpf_env **eep, const struct ebpf_config *ec);
int ebpf_env_destroy(struct ebpf_env *ee);

void ebpf_obj_acquire(struct ebpf_obj *eo);
void ebpf_obj_release(struct ebpf_obj *eo);

int ebpf_prog_create(struct ebpf_env *ee, struct ebpf_prog **epp, struct ebpf_prog_attr *attr);
void ebpf_prog_destroy(struct ebpf_prog *ep);
uint64_t ebpf_prog_run(void *ctx, struct ebpf_prog *ep);

int ebpf_map_create(struct ebpf_env *ee, struct ebpf_map **emp, struct ebpf_map_attr *attr);
void *ebpf_map_lookup_elem(struct ebpf_map *em, void *key);
int ebpf_map_update_elem(struct ebpf_map *em, void *key, void *value, uint64_t flags);
int ebpf_map_delete_elem(struct ebpf_map *em, void *key);
int ebpf_map_lookup_elem_from_user(struct ebpf_map *em, void *key, void *value);
int ebpf_map_update_elem_from_user(struct ebpf_map *em, void *key, void *value, uint64_t flags);
int ebpf_map_delete_elem_from_user(struct ebpf_map *em, void *key);
int ebpf_map_get_next_key_from_user(struct ebpf_map *em, void *key, void *next_key);
void ebpf_map_destroy(struct ebpf_map *em);

extern const struct ebpf_map_type emt_array;
extern const struct ebpf_map_type emt_percpu_array;
extern const struct ebpf_map_type emt_hashtable;
extern const struct ebpf_map_type emt_percpu_hashtable;
extern const struct ebpf_helper_type eht_map_lookup_elem;
extern const struct ebpf_helper_type eht_map_update_elem;
extern const struct ebpf_helper_type eht_map_delete_elem;

#endif
