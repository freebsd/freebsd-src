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

#ifdef __FreeBSD__
#ifdef _KERNEL
#include "ebpf_freebsd.h"
#else
#include <ebpf_freebsd_user.h>
#endif
#elif defined(linux)
#ifdef _KERNEL
#include <ebpf_linux.h>
#else
#include <ebpf_linux_user.h>
#endif
#elif defined(__APPLE__)
#ifdef _KERNEL
#error Kernel space code is not supported
#else
#include <ebpf_darwin_user.h>
#endif
#else
#error Unsupported platform
#endif

/*
 * Prototypes of platform dependent functions
 */
extern int ebpf_init(void);
extern int ebpf_deinit(void);
extern void *ebpf_malloc(size_t size);
extern void *ebpf_calloc(size_t number, size_t size);
extern void ebpf_free(void *mem);
extern void *ebpf_exalloc(size_t size);
extern void ebpf_exfree(void *mem, size_t size);
extern int ebpf_error(const char *fmt, ...);
extern uint16_t ebpf_ncpus(void);
extern uint16_t ebpf_curcpu(void);
extern long ebpf_getpagesize(void);
extern void ebpf_epoch_enter(void);
extern void ebpf_epoch_exit(void);
extern void ebpf_epoch_call(ebpf_epoch_context *ctx,
			    void (*callback)(ebpf_epoch_context *));
extern void ebpf_epoch_wait(void);
extern void ebpf_mtx_init(ebpf_mtx *mutex, const char *name);
extern void ebpf_mtx_lock(ebpf_mtx *mutex);
extern void ebpf_mtx_unlock(ebpf_mtx *mutex);
extern void ebpf_mtx_destroy(ebpf_mtx *mutex);
extern void ebpf_spinmtx_init(ebpf_spinmtx *mutex, const char *name);
extern void ebpf_spinmtx_lock(ebpf_spinmtx *mutex);
extern void ebpf_spinmtx_unlock(ebpf_spinmtx *mutex);
extern void ebpf_spinmtx_destroy(ebpf_spinmtx *mutex);
extern uint32_t ebpf_jenkins_hash(const void *buf, size_t len, uint32_t hash);
extern void ebpf_refcount_init(uint32_t *count, uint32_t value);
extern void ebpf_refcount_acquire(uint32_t *count);
extern int ebpf_refcount_release(uint32_t *count);
extern bool ebpf_prog_exit(void);
