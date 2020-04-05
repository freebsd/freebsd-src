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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <elf.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <ck_queue.h>
#include <ck_epoch.h>
#include <ck_pr.h>

/* Alternative of FreeBSD's one */
#define CPU_MAXSIZE 256

typedef void *ebpf_epoch_context;
typedef pthread_mutex_t ebpf_mtx;
typedef pthread_spinlock_t ebpf_spinmtx;

#define ebpf_assert(expr) assert(expr)

#define EBPF_EPOCH_LIST_ENTRY(_type) CK_LIST_ENTRY(_type)
#define EBPF_EPOCH_LIST_EMPTY(_type) CK_LIST_EMPTY(_type)
#define EBPF_EPOCH_LIST_FIRST(_headp, _type, _name) CK_LIST_FIRST(_headp)
#define EBPF_EPOCH_LIST_HEAD(_name, _type) CK_LIST_HEAD(_name, _type)
#define EBPF_EPOCH_LIST_INIT(_headp) CK_LIST_INIT(_headp)
#define EBPF_EPOCH_LIST_FOREACH(_var, _head, _name)                            \
	CK_LIST_FOREACH(_var, _head, _name)
#define EBPF_EPOCH_LIST_INSERT_HEAD(_head, _elem, _name)                       \
	CK_LIST_INSERT_HEAD(_head, _elem, _name)
#define EBPF_EPOCH_LIST_REMOVE(_elem, _name) CK_LIST_REMOVE(_elem, _name)
#define EBPF_EPOCH_LIST_NEXT(_elem, _name) CK_LIST_NEXT(_elem, _name)
