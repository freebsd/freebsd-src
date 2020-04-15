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

#include <dev/ebpf/ebpf_platform.h>
#include <sys/ebpf.h>

MALLOC_DECLARE(M_EBPFBUF);
MALLOC_DEFINE(M_EBPFBUF, "ebpf-buffers", "Buffers for ebpf and its subsystems");

/*
 * Platform dependent function implementations
 */
__inline void *
ebpf_malloc(size_t size)
{
	return malloc(size, M_EBPFBUF, M_NOWAIT);
}

__inline void *
ebpf_calloc(size_t number, size_t size)
{
	return malloc(number * size, M_EBPFBUF, M_NOWAIT | M_ZERO);
}

__inline void *
ebpf_exalloc(size_t size)
{
	return malloc(size, M_EBPFBUF, M_NOWAIT | M_EXEC);
}

__inline void
ebpf_exfree(void *mem, size_t size)
{
	free(mem, M_EBPFBUF);
}

__inline void
ebpf_free(void *mem)
{
	free(mem, M_EBPFBUF);
}

int
ebpf_error(const char *fmt, ...)
{
	int ret;
	__va_list ap;

	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);

	return ret;
}

__inline uint16_t
ebpf_ncpus(void)
{
	return mp_maxid + 1;
}

#if 0
__inline uint16_t
ebpf_curcpu(void)
{
	return curcpu;
}
#endif

__inline long
ebpf_getpagesize(void)
{
	return PAGE_SIZE;
}

static epoch_t ebpf_epoch;

__inline void
ebpf_epoch_enter(void)
{
	epoch_enter(ebpf_epoch);
}

__inline void
ebpf_epoch_exit(void)
{
	epoch_exit(ebpf_epoch);
}

__inline void
ebpf_epoch_call(ebpf_epoch_context *ctx,
		void (*callback)(ebpf_epoch_context *))
{
	epoch_call(ebpf_epoch, callback, ctx);
}

__inline void
ebpf_epoch_wait(void)
{
	epoch_wait(ebpf_epoch);
}

__inline void
ebpf_mtx_init(ebpf_mtx *mutex, const char *name)
{
	mtx_init(mutex, name, NULL, MTX_DEF);
}

__inline void
ebpf_mtx_lock(ebpf_mtx *mutex)
{
	mtx_lock(mutex);
}

__inline void
ebpf_mtx_unlock(ebpf_mtx *mutex)
{
	mtx_unlock(mutex);
}

__inline void
ebpf_mtx_destroy(ebpf_mtx *mutex)
{
	mtx_destroy(mutex);
}

__inline void
ebpf_spinmtx_init(ebpf_spinmtx *mutex, const char *name)
{
	mtx_init(mutex, name, NULL, MTX_SPIN);
}

__inline void
ebpf_spinmtx_lock(ebpf_spinmtx *mutex)
{
	mtx_lock_spin(mutex);
}

__inline void
ebpf_spinmtx_unlock(ebpf_spinmtx *mutex)
{
	mtx_unlock_spin(mutex);
}

__inline void
ebpf_spinmtx_destroy(ebpf_spinmtx *mutex)
{
	mtx_destroy(mutex);
}

__inline void
ebpf_refcount_init(uint32_t *count, uint32_t value)
{
	refcount_init(count, value);
}

__inline void
ebpf_refcount_acquire(uint32_t *count)
{
	refcount_acquire(count);
}

__inline int
ebpf_refcount_release(uint32_t *count)
{
	return refcount_release(count);
}

__inline uint32_t
ebpf_jenkins_hash(const void *buf, size_t len, uint32_t hash)
{
	return jenkins_hash(buf, len, hash);
}

int
ebpf_init(void)
{
	ebpf_epoch = epoch_alloc("ebpf_epoch", 0);
	return 0;
}

int
ebpf_deinit(void)
{
	epoch_free(ebpf_epoch);
	return 0;
}
