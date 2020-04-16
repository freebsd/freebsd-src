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

#ifndef _SYS_EBPF_ELF_H
#define _SYS_EBPF_ELF_H

struct ebpf_map_def {
	uint32_t type;
	uint32_t key_size;
	uint32_t value_size;
	uint32_t max_entries;
	uint32_t flags;
	uint32_t inner_map_idx;
	uint32_t numa_node;
};

enum ebpf_common_functions {
	EBPF_FUNC_unspec = 0,
	EBPF_FUNC_map_update_elem,
	EBPF_FUNC_map_lookup_elem,
	EBPF_FUNC_map_delete_elem,
	EBPF_FUNC_map_path_lookup,
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
	EBPF_FUNC_map_enqueue,
	EBPF_FUNC_map_dequeue,
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

#endif
