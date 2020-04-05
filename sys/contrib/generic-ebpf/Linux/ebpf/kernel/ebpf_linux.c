/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019, Yutaro Hayakawa
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*-
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (c) 2019 Yutaro Hayakawa
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <dev/ebpf/ebpf_platform.h>
#include <dev/ebpf/ebpf_allocator.h>
#include <dev/ebpf/ebpf_obj.h>
#include <dev/ebpf/ebpf_prog.h>
#include <sys/ebpf.h>

void *
ebpf_malloc(size_t size)
{
	return kmalloc(size, GFP_NOWAIT);
}

void *
ebpf_calloc(size_t number, size_t size)
{
	void *ret = kmalloc(number * size, GFP_NOWAIT);
	if (ret == NULL) {
		return NULL;
	}

	memset(ret, 0, number * size);

	return ret;
}

void *
ebpf_exalloc(size_t size)
{
	return __vmalloc(size, GFP_NOWAIT, PAGE_KERNEL_EXEC);
}

void
ebpf_exfree(void *mem, size_t size)
{
	vfree(mem);
}

void
ebpf_free(void *mem)
{
	kfree(mem);
}

int
ebpf_error(const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = vprintk(fmt, ap);
	va_end(ap);

	return ret;
}

uint16_t
ebpf_ncpus(void)
{
	return nr_cpu_ids;
}

uint16_t
ebpf_curcpu(void)
{
  return smp_processor_id();
}

long
ebpf_getpagesize(void)
{
	return PAGE_SIZE;
}

void
ebpf_epoch_enter(void)
{
  rcu_read_lock();
}

void
ebpf_epoch_exit(void)
{
  rcu_read_unlock();
}

void
ebpf_epoch_call(ebpf_epoch_context *ctx,
    void (*callback)(ebpf_epoch_context *))
{
  call_rcu(ctx, callback);
}

void
ebpf_epoch_wait(void)
{
  synchronize_rcu();
}

void
ebpf_mtx_init(ebpf_mtx *mutex, const char *name)
{
	mutex_init(mutex);
}

void
ebpf_mtx_lock(ebpf_mtx *mutex)
{
	mutex_lock(mutex);
}

void
ebpf_mtx_unlock(ebpf_mtx *mutex)
{
	mutex_unlock(mutex);
}

void
ebpf_mtx_destroy(ebpf_mtx *mutex)
{
	mutex_destroy(mutex);
}

void
ebpf_spinmtx_init(ebpf_spinmtx *mutex, const char *name)
{
  raw_spin_lock_init(mutex);
}

void
ebpf_spinmtx_lock(ebpf_spinmtx *mutex)
{
  raw_spin_lock(mutex);
}

void
ebpf_spinmtx_unlock(ebpf_spinmtx *mutex)
{
  raw_spin_unlock(mutex);
}

void
ebpf_spinmtx_destroy(ebpf_spinmtx *mutex)
{ 
  return;
}

void
ebpf_refcount_init(uint32_t *count, uint32_t value)
{
  *count = value;
}

void
ebpf_refcount_acquire(uint32_t *count)
{
  __sync_fetch_and_add(count, 1);
}

int
ebpf_refcount_release(uint32_t *count)
{
	uint32_t old;

	old = __sync_fetch_and_add(count, -1);
	ebpf_assert(old > 0);

	if (old > 1) {
		return 0;
	}

	return 1;
}

uint32_t
ebpf_jenkins_hash(const void *buf, size_t len, uint32_t hash)
{
  return jhash(buf, len, hash);
}

int
ebpf_init(void)
{
	return 0;
}

static int
ebpf_mod_init(void)
{
	return ebpf_init();
}

int
ebpf_deinit(void)
{
	return 0;
}

static void
ebpf_mod_deinit(void)
{
	int error;
	error = ebpf_deinit();
	ebpf_assert(error == 0);
}

/* sys/sys/ebpf.h */
EXPORT_SYMBOL(ebpf_env_create);
EXPORT_SYMBOL(ebpf_env_destroy);
EXPORT_SYMBOL(ebpf_prog_create);
EXPORT_SYMBOL(ebpf_prog_destroy);
EXPORT_SYMBOL(ebpf_prog_run);
EXPORT_SYMBOL(ebpf_map_create);
EXPORT_SYMBOL(ebpf_map_lookup_elem);
EXPORT_SYMBOL(ebpf_map_update_elem);
EXPORT_SYMBOL(ebpf_map_delete_elem);
EXPORT_SYMBOL(ebpf_map_lookup_elem_from_user);
EXPORT_SYMBOL(ebpf_map_update_elem_from_user);
EXPORT_SYMBOL(ebpf_map_delete_elem_from_user);
EXPORT_SYMBOL(ebpf_map_get_next_key_from_user);
EXPORT_SYMBOL(ebpf_map_destroy);

/* dev/ebpf/ebpf_allocator.h */
EXPORT_SYMBOL(ebpf_allocator_init);
EXPORT_SYMBOL(ebpf_allocator_deinit);
EXPORT_SYMBOL(ebpf_allocator_alloc);
EXPORT_SYMBOL(ebpf_allocator_free);

/* dev/ebpf/ebpf_env.h */
EXPORT_SYMBOL(ebpf_env_acquire);
EXPORT_SYMBOL(ebpf_env_release);

/* dev/ebpf/ebpf_obj.h */
EXPORT_SYMBOL(ebpf_obj_init);
EXPORT_SYMBOL(ebpf_obj_acquire);
EXPORT_SYMBOL(ebpf_obj_release);

/* dev/ebpf/ebpf_platform.h */
EXPORT_SYMBOL(ebpf_malloc);
EXPORT_SYMBOL(ebpf_calloc);
EXPORT_SYMBOL(ebpf_free);
EXPORT_SYMBOL(ebpf_exalloc);
EXPORT_SYMBOL(ebpf_exfree);
EXPORT_SYMBOL(ebpf_error);
EXPORT_SYMBOL(ebpf_ncpus);
EXPORT_SYMBOL(ebpf_curcpu);
EXPORT_SYMBOL(ebpf_getpagesize);
EXPORT_SYMBOL(ebpf_epoch_enter);
EXPORT_SYMBOL(ebpf_epoch_exit);
EXPORT_SYMBOL(ebpf_epoch_call);
EXPORT_SYMBOL(ebpf_epoch_wait);
EXPORT_SYMBOL(ebpf_mtx_init);
EXPORT_SYMBOL(ebpf_mtx_lock);
EXPORT_SYMBOL(ebpf_mtx_unlock);
EXPORT_SYMBOL(ebpf_mtx_destroy);
EXPORT_SYMBOL(ebpf_spinmtx_init);
EXPORT_SYMBOL(ebpf_spinmtx_lock);
EXPORT_SYMBOL(ebpf_spinmtx_unlock);
EXPORT_SYMBOL(ebpf_spinmtx_destroy);
EXPORT_SYMBOL(ebpf_jenkins_hash);
EXPORT_SYMBOL(ebpf_refcount_init);
EXPORT_SYMBOL(ebpf_refcount_acquire);
EXPORT_SYMBOL(ebpf_refcount_release);

/* dev/ebpf/ebpf_prog.h */
EXPORT_SYMBOL(ebpf_prog_attach_map);

module_init(ebpf_mod_init);
module_exit(ebpf_mod_deinit);
MODULE_AUTHOR("Yutaro Hayakawa");
MODULE_DESCRIPTION("Generic eBPF Module");
MODULE_LICENSE("Dual BSD/GPL");
