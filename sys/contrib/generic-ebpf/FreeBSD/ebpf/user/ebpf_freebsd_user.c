/*-
 * SPDX-License-Identifier: Apache License 2.0
 *
 * Copyright 2018 Yutaro Hayakawa
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
#include <dev/ebpf/ebpf_jhash.h>
#include <dev/ebpf/ebpf_epoch.h>
#include <sys/ebpf.h>

#define __inline __attribute__((always_inline))

__inline void *
ebpf_malloc(size_t size)
{
	return malloc(size);
}

__inline void *
ebpf_calloc(size_t number, size_t size)
{
	return calloc(number, size);
}

__inline void *
ebpf_exalloc(size_t size)
{
	void *ret = NULL;

	ret = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ret == MAP_FAILED) {
		fprintf(stderr, "mmap in ebpf_exalloc failed\n");
		return NULL;
	}

	return ret;
}

__inline void
ebpf_exfree(void *mem, size_t size)
{
	munmap(mem, size);
}

__inline void
ebpf_free(void *mem)
{
	free(mem);
}

int
ebpf_error(const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vfprintf(stderr, fmt, ap);
	va_end(ap);

	return ret;
}

__inline uint16_t
ebpf_ncpus(void)
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}

__inline uint16_t
ebpf_curcpu(void)
{
	int error;
	cpuset_t cpus;

	error = pthread_getaffinity_np(pthread_self(), sizeof(cpus), &cpus);
	ebpf_assert(!error);

	/*
	 * Return first CPU founded from affinity set.
	 * If the program pinned the thread to single
	 * CPU, this function returns pinned CPU.
	 *
	 * Note that the epoch never works correctly
	 * unless the running thread is pinned to
	 * single CPU.
	 */
	for (uint16_t i = 0; i < CPU_MAXSIZE; i++) {
		if (CPU_ISSET(i, &cpus)) {
			return i;
		}
	}

	/*
	 * Should not reach to here
	 */
	ebpf_assert(false);
	return 0;
}

__inline long
ebpf_getpagesize(void)
{
	return sysconf(_SC_PAGE_SIZE);
}

__inline void
ebpf_refcount_init(uint32_t *count, uint32_t value)
{
	*count = value;
}

__inline void
ebpf_refcount_acquire(uint32_t *count)
{
	ebpf_assert(*count < UINT32_MAX);
	ck_pr_inc_32(count);
}

__inline int
ebpf_refcount_release(uint32_t *count)
{
	uint32_t old;

	old = ck_pr_faa_32(count, -1);
	ebpf_assert(old > 0);

	if (old > 1) {
		return 0;
	}

	return 1;
}

__inline void
ebpf_mtx_init(ebpf_mtx *mutex, const char *name)
{
	int error = pthread_mutex_init(mutex, NULL);
	assert(!error);
}

__inline void
ebpf_mtx_lock(ebpf_mtx *mutex)
{
	int error = pthread_mutex_lock(mutex);
	assert(!error);
}

__inline void
ebpf_mtx_unlock(ebpf_mtx *mutex)
{
	int error = pthread_mutex_unlock(mutex);
	assert(!error);
}

__inline void
ebpf_mtx_destroy(ebpf_mtx *mutex)
{
	int error = pthread_mutex_destroy(mutex);
	assert(!error);
}

__inline void
ebpf_spinmtx_init(ebpf_spinmtx *mutex, const char *name)
{
	int error = pthread_spin_init(mutex, 0);
	assert(!error);
}

__inline void
ebpf_spinmtx_lock(ebpf_spinmtx *mutex)
{
	int error = pthread_spin_lock(mutex);
	assert(!error);
}

__inline void
ebpf_spinmtx_unlock(ebpf_spinmtx *mutex)
{
	int error = pthread_spin_unlock(mutex);
	assert(!error);
}

__inline void
ebpf_spinmtx_destroy(ebpf_spinmtx *mutex)
{
	int error = pthread_spin_destroy(mutex);
	assert(!error);
}

__inline uint32_t
ebpf_jenkins_hash(const void *buf, size_t len, uint32_t hash)
{
  return jenkins_hash(buf, len, hash);
}

int
ebpf_init(void)
{
	int error;

	error = ebpf_epoch_init();
	if (error != 0) {
		return error;
	}

	return 0;
}

int
ebpf_deinit(void)
{
	int error;

	error = ebpf_epoch_deinit();
	if (error != 0) {
		return error;
	}

	return 0;
}
