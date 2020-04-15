
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ryan Stone
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

#ifndef _SYS_EBPF_PARAM_H
#define _SYS_EBPF_PARAM_H

enum ebpf_fbsd_prog_types {
	EBPF_PROG_TYPE_VFS,
	EBPF_PROG_TYPE_MAX
};

enum ebpf_basic_map_types {
	EBPF_MAP_TYPE_BAD = 0,
	EBPF_MAP_TYPE_ARRAY,
	EBPF_MAP_TYPE_PERCPU_ARRAY,
	EBPF_MAP_TYPE_HASHTABLE,
	EBPF_MAP_TYPE_PERCPU_HASHTABLE,
	EBPF_MAP_TYPE_PROGARRAY,
	EBPF_MAP_TYPE_ARRAYQUEUE,
	EBPF_MAP_TYPE_MAX
};

enum ebpf_common_functions {
	EBPF_FUNC_unspec = 0,
	EBPF_FUNC_ebpf_map_update_elem,
	EBPF_FUNC_ebpf_map_lookup_elem,
	EBPF_FUNC_ebpf_map_delete_elem,
	EBPF_FUNC_ebpf_map_lookup_path,
	EBPF_FUNC_copyinstr,		/* 5 */
	EBPF_FUNC_copyout,
	EBPF_FUNC_dup,
	EBPF_FUNC_openat,
	EBPF_FUNC_fstatat,
	EBPF_FUNC_fstat,		/* 10 */
	EBPF_FUNC_faccessat,
	EBPF_FUNC_set_errno,
	EBPF_FUNC_set_syscall_retval,
	EBPF_FUNC_pdfork,
	EBPF_FUNC_pdwait4_nohang,	/* 15 */
	EBPF_FUNC_pdwait4_defer,
	EBPF_FUNC_fexecve,
	EBPF_FUNC_memset,
	EBPF_FUNC_readlinkat,
	EBPF_FUNC_dummy_unimpl,		/* 20 */
	EBPF_FUNC_exec_get_interp,
	EBPF_FUNC_strncmp,
	EBPF_FUNC_canonical_path,
	EBPF_FUNC_renameat,
	EBPF_FUNC_mkdirat,		/* 25 */
	EBPF_FUNC_fchdir,
	EBPF_FUNC_getpid,
	EBPF_FUNC_get_errno,
	EBPF_FUNC_copyin,
	EBPF_FUNC_ktrnamei,		/* 30 */
	EBPF_FUNC_symlink_path,
	EBPF_FUNC_strlcpy,
	EBPF_FUNC_ebpf_map_enqueue,
	EBPF_FUNC_ebpf_map_dequeue,
	EBPF_FUNC_kqueue,		/* 35 */
	EBPF_FUNC_kevent_install,
	EBPF_FUNC_kevent_poll,
	EBPF_FUNC_kevent_block,
	EBPF_FUNC_close,
	EBPF_FUNC_get_syscall_retval,	/* 40 */
	EBPF_FUNC_symlinkat,
	EBPF_FUNC_resolve_one_symlink,
	EBPF_FUNC_utimensat,
	EBPF_FUNC_fcntl,
	EBPF_FUNC_unlinkat,		/* 45 */
	EBPF_FUNC_fchown,
	EBPF_FUNC_fchownat,
	EBPF_FUNC_fchmod,
	EBPF_FUNC_fchmodat,
	EBPF_FUNC_futimens,		/* 50 */
	EBPF_FUNC_linkat,
	__EBPF_COMMON_FUNCTIONS_MAX
};

struct ebpf_symlink_res_bufs
{
	char *pathBuf;
	char *scratch1;
	char *scratch2;
};

#endif
