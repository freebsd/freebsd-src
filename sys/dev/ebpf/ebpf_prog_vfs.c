/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ryan Stone
 * All rights reserved.
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
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/event.h>
#include <sys/smp.h>
#include <sys/lock.h>
#include <sys/resource.h>
#include <sys/sx.h>

#include <sys/ebpf.h>
#include <sys/ebpf_param.h>
#include <dev/ebpf/ebpf_dev_freebsd.h>
#include <dev/ebpf/ebpf_internal.h>
#include <dev/ebpf/ebpf_dev_probe.h>

static struct sx cpu_sx[MAXCPU];

static void
vfs_init(void)
{
	int i;

	for (i = 0; i < mp_ncpus; ++i) {
		sx_init(&cpu_sx[i], "ebpf_vfs_cpu_sx");
	}
}

static void
vfs_deinit(void)
{
	int i;

	for (i = 0; i < mp_ncpus; ++i) {
		sx_destroy(&cpu_sx[i]);
	}
}

static int
vfs_reserve_cpu(struct ebpf_vm_state *vm_state)
{
	int c, error;

	c = curcpu;
	error = sx_slock_sig(&cpu_sx[c]);
	if (error != 0) {
		return (error);
	}

	vm_state->cpu = c;
	return (0);
}

static void
vfs_release_cpu(struct ebpf_vm_state *vm_state)
{
	sx_sunlock(&cpu_sx[vm_state->cpu]);
}

static bool
vfs_is_map_usable(struct ebpf_map_type *emt)
{
	return (1);
}

static bool
vfs_is_helper_usable(struct ebpf_helper_type *eht)
{
	switch (eht->id) {
		case EBPF_FUNC_map_update_elem:
		case EBPF_FUNC_map_lookup_elem:
		case EBPF_FUNC_map_delete_elem:
		case EBPF_FUNC_map_path_lookup:
		case EBPF_FUNC_map_enqueue:
		case EBPF_FUNC_map_dequeue:

		case EBPF_FUNC_copyinstr:
		case EBPF_FUNC_copyout:
		case EBPF_FUNC_dup:
		case EBPF_FUNC_openat:
		case EBPF_FUNC_fstat:
		case EBPF_FUNC_fstatat:
		case EBPF_FUNC_faccessat:
		case EBPF_FUNC_set_errno:
		case EBPF_FUNC_set_syscall_retval:
		case EBPF_FUNC_pdfork:
		case EBPF_FUNC_pdwait4_nohang:
		case EBPF_FUNC_pdwait4_defer:
		case EBPF_FUNC_fexecve:
		case EBPF_FUNC_memset:
		case EBPF_FUNC_readlinkat:
		case EBPF_FUNC_exec_get_interp:
		case EBPF_FUNC_strncmp:
		case EBPF_FUNC_canonical_path:
		case EBPF_FUNC_renameat:
		case EBPF_FUNC_mkdirat:
		case EBPF_FUNC_fchdir:
		case EBPF_FUNC_getpid:
		case EBPF_FUNC_get_errno:
		case EBPF_FUNC_copyin:
		case EBPF_FUNC_ktrnamei:
		case EBPF_FUNC_symlink_path:
		case EBPF_FUNC_strlcpy:
		case EBPF_FUNC_kqueue:
		case EBPF_FUNC_kevent_install:
		case EBPF_FUNC_kevent_poll:
		case EBPF_FUNC_kevent_block:
		case EBPF_FUNC_close:
		case EBPF_FUNC_get_syscall_retval:
		case EBPF_FUNC_symlinkat:
		case EBPF_FUNC_resolve_one_symlink:
		case EBPF_FUNC_utimensat:
		case EBPF_FUNC_fcntl:
		case EBPF_FUNC_unlinkat:
		case EBPF_FUNC_fchown:
		case EBPF_FUNC_fchownat:
		case EBPF_FUNC_fchmod:
		case EBPF_FUNC_fchmodat:
		case EBPF_FUNC_futimens:
		case EBPF_FUNC_linkat:
			return (true);
	}

	return (false);
}

const struct ebpf_probe_ops vfs_probe_ops = {
	.init = vfs_init,
	.fini = vfs_deinit,
	.reserve_cpu = vfs_reserve_cpu,
	.release_cpu = vfs_release_cpu,
};

const struct ebpf_prog_type ept_vfs = {
	.name = "vfs",
	.ops = {
		.is_map_usable = vfs_is_map_usable,
		.is_helper_usable = vfs_is_helper_usable,
	},
};
