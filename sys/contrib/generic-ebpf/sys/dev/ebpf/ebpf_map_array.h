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

struct ebpf_map;
struct ebpf_map_attr;

int array_map_init(struct ebpf_map *em, struct ebpf_map_attr *attr);
void array_map_deinit(struct ebpf_map *em);

void *array_map_lookup_elem(struct ebpf_map *em, void *key);
int array_map_lookup_elem_from_user(struct ebpf_map *em, void *key, void *value);
int array_map_update_elem(struct ebpf_map *em, void *key, void *value,
		      uint64_t flags);
int array_map_delete_elem(struct ebpf_map *map, void *key);
int array_map_get_next_key(struct ebpf_map *map, void *key, void *next_key);
