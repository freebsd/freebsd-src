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

#include "ebpf_dev_platform.h"
#include <dev/ebpf/ebpf_map.h>
#include <dev/ebpf/ebpf_prog.h>

#include <sys/ebpf.h>
#include <sys/ebpf_elf.h>
#include <sys/ebpf_param.h>
#include <sys/ebpf_probe.h>
#include <sys/ebpf_dev.h>
#include <dev/ebpf/ebpf_dev_probe.h>
#include <dev/ebpf/ebpf_map_freebsd.h>
#include <dev/ebpf/ebpf_internal.h>
#include <dev/ebpf/ebpf_probe_syscall.h>
#include <sys/ebpf_vm_isa.h>

#define EBPF_HELPER_TYPE_DEF(prefix, helper) \
	const struct ebpf_helper_type eht_fbsd_ ## prefix ## helper = { \
		.name = "ebpf_" #helper, \
		.fn = (ebpf_helper_fn)ebpf_ ## prefix ## helper, \
		.id = EBPF_FUNC_ ## helper, \
	};

EBPF_HELPER_TYPE_DEF(, map_lookup_elem);
EBPF_HELPER_TYPE_DEF(, map_update_elem);
EBPF_HELPER_TYPE_DEF(, map_delete_elem);
EBPF_HELPER_TYPE_DEF(, map_path_lookup);
// EBPF_HELPER_TYPE_DEF(map_enqueue);
// EBPF_HELPER_TYPE_DEF(map_dequeue);

EBPF_HELPER_TYPE_DEF(probe_, copyinstr);
EBPF_HELPER_TYPE_DEF(probe_, copyout);
EBPF_HELPER_TYPE_DEF(probe_, dup);
EBPF_HELPER_TYPE_DEF(probe_, openat);
EBPF_HELPER_TYPE_DEF(probe_, fstat);
EBPF_HELPER_TYPE_DEF(probe_, fstatat);
EBPF_HELPER_TYPE_DEF(probe_, faccessat);
EBPF_HELPER_TYPE_DEF(probe_, set_errno);
EBPF_HELPER_TYPE_DEF(probe_, set_syscall_retval);
EBPF_HELPER_TYPE_DEF(probe_, pdfork);
EBPF_HELPER_TYPE_DEF(probe_, pdwait4_nohang);
EBPF_HELPER_TYPE_DEF(probe_, pdwait4_defer);
EBPF_HELPER_TYPE_DEF(probe_, fexecve);
EBPF_HELPER_TYPE_DEF(probe_, memset);
EBPF_HELPER_TYPE_DEF(probe_, readlinkat);
EBPF_HELPER_TYPE_DEF(probe_, exec_get_interp);
EBPF_HELPER_TYPE_DEF(probe_, strncmp);
EBPF_HELPER_TYPE_DEF(probe_, canonical_path);
EBPF_HELPER_TYPE_DEF(probe_, renameat);
EBPF_HELPER_TYPE_DEF(probe_, mkdirat);
EBPF_HELPER_TYPE_DEF(probe_, fchdir);
EBPF_HELPER_TYPE_DEF(probe_, getpid);
EBPF_HELPER_TYPE_DEF(probe_, get_errno);
EBPF_HELPER_TYPE_DEF(probe_, copyin);
EBPF_HELPER_TYPE_DEF(probe_, ktrnamei);
EBPF_HELPER_TYPE_DEF(probe_, symlink_path);
EBPF_HELPER_TYPE_DEF(probe_, strlcpy);
EBPF_HELPER_TYPE_DEF(probe_, kqueue);
EBPF_HELPER_TYPE_DEF(probe_, kevent_install);
EBPF_HELPER_TYPE_DEF(probe_, kevent_poll);
EBPF_HELPER_TYPE_DEF(probe_, kevent_block);
EBPF_HELPER_TYPE_DEF(probe_, close);
EBPF_HELPER_TYPE_DEF(probe_, get_syscall_retval);
EBPF_HELPER_TYPE_DEF(probe_, symlinkat);
EBPF_HELPER_TYPE_DEF(probe_, resolve_one_symlink);
EBPF_HELPER_TYPE_DEF(probe_, utimensat);
EBPF_HELPER_TYPE_DEF(probe_, fcntl);
EBPF_HELPER_TYPE_DEF(probe_, unlinkat);
EBPF_HELPER_TYPE_DEF(probe_, fchown);
EBPF_HELPER_TYPE_DEF(probe_, fchownat);
EBPF_HELPER_TYPE_DEF(probe_, fchmod);
EBPF_HELPER_TYPE_DEF(probe_, fchmodat);
EBPF_HELPER_TYPE_DEF(probe_, futimens);
EBPF_HELPER_TYPE_DEF(probe_, linkat);

const struct ebpf_config fbsd_ebpf_config = {
	.prog_types = {
		[EBPF_PROG_TYPE_VFS] = &ept_vfs,
		[EBPF_PROG_TYPE_XDP] = &ept_xdp,
	},
	.map_types = {
		[EBPF_MAP_TYPE_ARRAY]            = &emt_array,
		[EBPF_MAP_TYPE_PERCPU_ARRAY]     = &emt_percpu_array,
		[EBPF_MAP_TYPE_HASHTABLE]        = &emt_hashtable,
		[EBPF_MAP_TYPE_PERCPU_HASHTABLE] = &emt_percpu_hashtable,
		[EBPF_MAP_TYPE_PROGARRAY]        = &emt_progarray,
// 		[EBPF_MAP_TYPE_ARRAYQUEUE]       = &emt_arrayqueue,

	},
	.helper_types = {
		[EBPF_FUNC_map_update_elem] = &eht_fbsd_map_update_elem,
		[EBPF_FUNC_map_lookup_elem] = &eht_fbsd_map_lookup_elem,
		[EBPF_FUNC_map_delete_elem] = &eht_fbsd_map_delete_elem,
		[EBPF_FUNC_map_path_lookup] = &eht_fbsd_map_path_lookup,
// 		[EBPF_FUNC_ebpf_map_enqueue] = &eht_fbsd_map_enqueue,
// 		[EBPF_FUNC_ebpf_map_dequeue] = &eht_fbsd_map_dequeue,

		[EBPF_FUNC_copyinstr] = &eht_fbsd_probe_copyinstr,
		[EBPF_FUNC_copyout] = &eht_fbsd_probe_copyout,
		[EBPF_FUNC_dup] = &eht_fbsd_probe_dup,
		[EBPF_FUNC_openat] = &eht_fbsd_probe_openat,
		[EBPF_FUNC_fstat] = &eht_fbsd_probe_fstat,
		[EBPF_FUNC_fstatat] = &eht_fbsd_probe_fstatat,
		[EBPF_FUNC_faccessat] = &eht_fbsd_probe_faccessat,
		[EBPF_FUNC_set_errno] = &eht_fbsd_probe_set_errno,
		[EBPF_FUNC_set_syscall_retval] = &eht_fbsd_probe_set_syscall_retval,
		[EBPF_FUNC_pdfork] = &eht_fbsd_probe_pdfork,
		[EBPF_FUNC_pdwait4_nohang] = &eht_fbsd_probe_pdwait4_nohang,
		[EBPF_FUNC_pdwait4_defer] = &eht_fbsd_probe_pdwait4_defer,
		[EBPF_FUNC_fexecve] = &eht_fbsd_probe_fexecve,
		[EBPF_FUNC_memset] = &eht_fbsd_probe_memset,
		[EBPF_FUNC_readlinkat] = &eht_fbsd_probe_readlinkat,
		[EBPF_FUNC_exec_get_interp] = &eht_fbsd_probe_exec_get_interp,
		[EBPF_FUNC_strncmp] = &eht_fbsd_probe_strncmp,
		[EBPF_FUNC_canonical_path] = &eht_fbsd_probe_canonical_path,
		[EBPF_FUNC_renameat] = &eht_fbsd_probe_renameat,
		[EBPF_FUNC_mkdirat] = &eht_fbsd_probe_mkdirat,
		[EBPF_FUNC_fchdir] = &eht_fbsd_probe_fchdir,
		[EBPF_FUNC_getpid] = &eht_fbsd_probe_getpid,
		[EBPF_FUNC_get_errno] = &eht_fbsd_probe_get_errno,
		[EBPF_FUNC_copyin] = &eht_fbsd_probe_copyin,
		[EBPF_FUNC_ktrnamei] = &eht_fbsd_probe_ktrnamei,
		[EBPF_FUNC_symlink_path] = &eht_fbsd_probe_symlink_path,
		[EBPF_FUNC_strlcpy] = &eht_fbsd_probe_strlcpy,
		[EBPF_FUNC_kqueue] = &eht_fbsd_probe_kqueue,
		[EBPF_FUNC_kevent_install] = &eht_fbsd_probe_kevent_install,
		[EBPF_FUNC_kevent_poll] = &eht_fbsd_probe_kevent_poll,
		[EBPF_FUNC_kevent_block] = &eht_fbsd_probe_kevent_block,
		[EBPF_FUNC_close] = &eht_fbsd_probe_close,
		[EBPF_FUNC_get_syscall_retval] = &eht_fbsd_probe_get_syscall_retval,
		[EBPF_FUNC_symlinkat] = &eht_fbsd_probe_symlinkat,
		[EBPF_FUNC_resolve_one_symlink] = &eht_fbsd_probe_resolve_one_symlink,
		[EBPF_FUNC_utimensat] = &eht_fbsd_probe_utimensat,
		[EBPF_FUNC_fcntl] = &eht_fbsd_probe_fcntl,
		[EBPF_FUNC_unlinkat] = &eht_fbsd_probe_unlinkat,
		[EBPF_FUNC_fchown] = &eht_fbsd_probe_fchown,
		[EBPF_FUNC_fchownat] = &eht_fbsd_probe_fchownat,
		[EBPF_FUNC_fchmod] = &eht_fbsd_probe_fchmod,
		[EBPF_FUNC_fchmodat] = &eht_fbsd_probe_fchmodat,
		[EBPF_FUNC_futimens] = &eht_fbsd_probe_futimens,
		[EBPF_FUNC_linkat] = &eht_fbsd_probe_linkat,

	},
	.preprocessor_type = NULL,
};

static struct ebpf_obj *
fd2eo(int fd, ebpf_thread *td)
{
	int error;
	ebpf_file *f;
	struct ebpf_obj *eo;

	if (fd < 0 || td == NULL)
		return NULL;

	error = ebpf_fget(td, fd, &f);
	if (error != 0)
		return NULL;

	if (!is_ebpf_objfile(f)) {
		ebpf_fdrop(f, td);
		return NULL;
	}

	eo = ebpf_file_get_data(f);
	ebpf_obj_acquire(eo);

	ebpf_fdrop(f, td);

	return eo;
}

static struct ebpf_prog *
fd2ep(int fd, ebpf_thread *td)
{
	struct ebpf_obj *eo = fd2eo(fd, td);
	return EO2EP(eo);
}

static struct ebpf_map *
fd2em(int fd, ebpf_thread *td)
{
	struct ebpf_obj *eo = fd2eo(fd, td);
	return EO2EM(eo);
}

/*
 * XXX: This function should not be in here. We should move
 * this function to ebpf module side and make generic
 * preprocessor.
 */
static int
ebpf_prog_preprocess(struct ebpf_prog *ep, ebpf_thread *td)
{
	int error;
	struct ebpf_inst *prog = ep->prog, *cur;
	uint16_t num_insts = ep->prog_len / sizeof(struct ebpf_inst);
	struct ebpf_map *em;

	for (uint32_t i = 0; i < num_insts; i++) {
		cur = prog + i;

		if (cur->opcode != EBPF_OP_LDDW)
			continue;

		if (i == num_insts - 1 || cur[1].opcode != 0 ||
		    cur[1].dst != 0 || cur[1].src != 0 || cur[1].offset != 0)
			return EINVAL;

		if (cur->src == 0)
			continue;

		/*
		 * Currently, only assume pseudo map descriptor
		 */
		if (cur->src != EBPF_PSEUDO_MAP_DESC)
			return EINVAL;

		em = fd2em(cur->imm, td);
		if (em == NULL)
			return EINVAL;

		cur[0].imm = (uint32_t)em;
		cur[1].imm = ((uint64_t)em) >> 32;

		/* Allow duplicate */
		error = ebpf_prog_attach_map(ep, em);

		ebpf_obj_release((struct ebpf_obj *)em);

		if (error != 0 && error != EEXIST)
			return error;

		i++;
	}

	return 0;
}

static int
ebpf_ioc_load_prog(struct ebpf_env *ee, union ebpf_req *req, ebpf_thread *td)
{
	int error, fd;
	ebpf_file *f;
	struct ebpf_prog *ep;
	struct ebpf_inst *insts;

	if (req == NULL || req->prog_fdp == NULL ||
			req->prog_type >= EBPF_PROG_TYPE_MAX ||
	    req->prog == NULL || req->prog_len == 0 ||
	    td == NULL)
		return EINVAL;

	insts = ebpf_malloc(req->prog_len);
	if (insts == NULL)
		return ENOMEM;

	error = ebpf_copyin(req->prog, insts, req->prog_len);
	if (error != 0)
		goto err0;

	struct ebpf_prog_attr attr = {
		.type = req->prog_type,
		.prog = insts,
		.prog_len = req->prog_len
	};

	error = ebpf_prog_create(ee, &ep, &attr);
	if (error != 0)
		goto err0;

	ebpf_free(insts);

	error = ebpf_prog_preprocess(ep, td);
	if (error != 0)
		goto err1;

	error = ebpf_fopen(td, &f, &fd, (struct ebpf_obj *)ep);
	if (error != 0)
		goto err1;

	error = ebpf_copyout(&fd, req->prog_fdp, sizeof(int));
	if (error != 0)
		goto err2;

	return 0;

err2:
	ebpf_fdrop(f, td);
err1:
	ebpf_prog_destroy(ep);
	return error;
err0:
	ebpf_free(insts);
	return error;
}

static int
ebpf_ioc_map_create(struct ebpf_env *ee, union ebpf_req *req, ebpf_thread *td)
{
	int error, fd;
	ebpf_file *f;
	struct ebpf_map *em;

	if (req == NULL || req->map_fdp == NULL || td == NULL)
		return EINVAL;

	struct ebpf_map_attr attr = {
		.type = req->map_type,
		.key_size = req->key_size,
		.value_size = req->value_size,
		.max_entries = req->max_entries,
		.flags = req->map_flags
	};

	error = ebpf_map_create(ee, &em, &attr);
	if (error != 0)
		return error;

	error = ebpf_fopen(td, &f, &fd, (struct ebpf_obj *)em);
	if (error != 0)
		goto err0;

	error = ebpf_copyout(&fd, req->map_fdp, sizeof(int));
	if (error != 0)
		goto err1;

	return 0;

err1:
	ebpf_fdrop(f, td);
err0:
	ebpf_map_destroy(em);
	return error;
}

static int
ebpf_ioc_map_lookup_elem(union ebpf_req *req, ebpf_thread *td)
{
	int error = 0;
	void *k, *v;
	uint32_t nvalues;
	struct ebpf_map *em;

	if (req == NULL || td == NULL || (void *)req->key == NULL ||
			(void *)req->value == NULL)
		return EINVAL;

	em = fd2em(req->map_fd, td);
	if (em == NULL)
		return EINVAL;

	k = ebpf_malloc(em->key_size);
	if (k == NULL) {
		error = ENOMEM;
		goto err0;
	}

	nvalues = em->percpu ? ebpf_ncpus() : 1;
	v = ebpf_calloc(nvalues, em->value_size);
	if (v == NULL) {
		error = ENOMEM;
		goto err1;
	}

	error = ebpf_copyin((void *)req->key, k, em->key_size);
	if (error != 0)
		goto err2;

	error = ebpf_map_lookup_elem_from_user(em, k, v);
	if (error != 0)
		goto err2;

	error = ebpf_copyout(v, (void *)req->value,
			em->value_size * nvalues);
	if (error != 0)
		goto err2;

err2:
	ebpf_free(v);
err1:
	ebpf_free(k);
err0:
	ebpf_obj_release((struct ebpf_obj *)em);
	return error;
}

static int
ebpf_ioc_map_update_elem(union ebpf_req *req, ebpf_thread *td)
{
	int error = 0;
	void *k, *v;
	struct ebpf_map *em;

	if (req == NULL || td == NULL || (void *)req->key == NULL ||
			(void *)req->value == NULL)
		return EINVAL;

	em = fd2em(req->map_fd, td);
	if (em == NULL)
		return EINVAL;

	k = ebpf_malloc(em->key_size);
	if (k == NULL) {
		error = ENOMEM;
		goto err0;
	}

	v = ebpf_malloc(em->value_size);
	if (v == NULL) {
		error = ENOMEM;
		goto err1;
	}

	error = ebpf_copyin((void *)req->key, k, em->key_size);
	if (error != 0)
		goto err2;

	error = ebpf_copyin((void *)req->value, v, em->value_size);
	if (error != 0)
		goto err2;

	error = ebpf_map_update_elem_from_user(em, k, v, req->flags);
	if (error != 0)
		goto err2;

err2:
	ebpf_free(v);
err1:
	ebpf_free(k);
err0:
	ebpf_obj_release((struct ebpf_obj *)em);
	return error;
}

static int
ebpf_ioc_map_delete_elem(union ebpf_req *req, ebpf_thread *td)
{
	int error;
	void *k;
	struct ebpf_map *em;

	if (req == NULL || td == NULL || (void *)req->key == NULL)
		return EINVAL;

	em = fd2em(req->map_fd, td);
	if (em == NULL)
		return EINVAL;

	k = ebpf_malloc(em->key_size);
	if (k == NULL) {
		error = ENOMEM;
		goto err0;
	}

	error = ebpf_copyin((void *)req->key, k, em->key_size);
	if (error != 0)
		goto err1;

	error = ebpf_map_delete_elem_from_user(em, k);
	if (error != 0)
		goto err1;

err1:
	ebpf_free(k);
err0:
	ebpf_obj_release((struct ebpf_obj *)em);
	return error;
}

static int
ebpf_ioc_map_get_next_key(union ebpf_req *req, ebpf_thread *td)
{
	int error = 0;
	void *k = NULL, *nk;
	struct ebpf_map *em;

	/*
	 * key == NULL is valid, because it means "give me a first key"
	 */
	if (req == NULL || td == NULL ||
			(void *)req->next_key == NULL)
		return EINVAL;

	em = fd2em(req->map_fd, td);
	if (em == NULL)
		return EINVAL;

	if (req->key != NULL) {
		k = ebpf_malloc(em->key_size);
		if (k == NULL) {
			error = ENOMEM;
			goto err0;
		}

		error = ebpf_copyin((void *)req->key, k, em->key_size);
		if (error != 0)
			goto err1;
	}

	nk = ebpf_malloc(em->key_size);
	if (nk == NULL) {
		error = ENOMEM;
		goto err1;
	}

	error = ebpf_map_get_next_key_from_user(em, k, nk);
	if (error != 0)
		goto err2;

	error = ebpf_copyout(nk, (void *)req->next_key, em->key_size);
	if (error != 0)
		goto err2;

err2:
	ebpf_free(nk);
err1:
	if (k)
		ebpf_free(k);
err0:
	ebpf_obj_release((struct ebpf_obj *)em);
	return error;
}

static const struct ebpf_map_type *
ebpf_env_get_map_type(struct ebpf_env *ee, uint32_t id)
{

	if (id > nitems(ee->ec->map_types)) {
		return (NULL);
	}

	return (ee->ec->map_types[id]);
}

static const struct ebpf_prog_type *
ebpf_env_get_prog_type(struct ebpf_env *ee, uint32_t id)
{

	if (id > nitems(ee->ec->prog_types)) {
		return (NULL);
	}

	return (ee->ec->prog_types[id]);
}

static int
ebpf_ioc_get_map_type_info(struct ebpf_env *ee, union ebpf_req *req)
{
	struct ebpf_map_type_info info;

	const struct ebpf_map_type *type = ebpf_env_get_map_type(ee, req->mt_id);
	if (type == NULL)
		return (EINVAL);

	bzero(&info, sizeof(info));
	strlcpy(info.name, type->name, EBPF_NAME_MAX);

	return (ebpf_copyout(&info, req->mt_info, sizeof(info)));
}

static int
ebpf_ioc_get_prog_type_info(struct ebpf_env *ee, union ebpf_req *req)
{
	struct ebpf_prog_type_info info;

	const struct ebpf_prog_type *type = ebpf_env_get_prog_type(ee, req->pt_id);
	if (type == NULL)
		return (ENOENT);

	bzero(&info, sizeof(info));
	strlcpy(info.name, type->name, EBPF_NAME_MAX);

	return (ebpf_copyout(&info, req->pt_info, sizeof(info)));
}

static int
has_null_term(const char * str, int max)
{
	int i;

	for (i = 0; i < max; ++i) {
		if (str[i] == '\0')
			return (1);
	}

	return (0);
}

static int
ebpf_attach(union ebpf_req *req, ebpf_thread *td)
{
	struct ebpf_req_attach *attach;
	ebpf_file *f;
	struct ebpf_prog *prog;
	int error;

	attach = &req->attach;

	error = ebpf_fd_to_program(td, attach->prog_fd, &f, &prog);
	if (error != 0) {
		return (EINVAL);
	}

	error = ebpf_probe_attach(attach->probe_id, prog, f, attach->jit);
	if (error != 0) {
		goto err0;
	}

	return (0);

err0:
	ebpf_fdrop(f, td);
	return (error);
}

static int
ebpf_fill_probeinfo(struct ebpf_probe *probe, void *arg)
{
	struct ebpf_probe_info *info;

	info = arg;
	bzero(info, sizeof(*info));

	info->id = probe->id;
	memcpy(&info->name, &probe->name, sizeof(info->name));
	info->num_attached = probe->active;

	return (0);
}

static int
ebpf_probe_by_name(union ebpf_req *req, ebpf_thread *td)
{

	if (!has_null_term(req->probe_by_name.name.tracer,
	    sizeof(req->probe_by_name.name.tracer))) {
		    return (EINVAL);
	}

	if (!has_null_term(req->probe_by_name.name.provider,
	    sizeof(req->probe_by_name.name.provider))) {
		    return (EINVAL);
	}

	if (!has_null_term(req->probe_by_name.name.module,
	    sizeof(req->probe_by_name.name.module))) {
		    return (EINVAL);
	}

	if (!has_null_term(req->probe_by_name.name.function,
	    sizeof(req->probe_by_name.name.function))) {
		    return (EINVAL);
	}

	if (!has_null_term(req->probe_by_name.name.name,
	    sizeof(req->probe_by_name.name.name))) {
		    return (EINVAL);
	}

	return (ebpf_get_probe_by_name(&req->probe_by_name.name,
	    ebpf_fill_probeinfo, &req->probe_by_name.info));
}

static int
ebpf_probe_iter(union ebpf_req *req, ebpf_thread *td)
{
	int error;

	error = ebpf_next_probe(req->probe_iter.prev_id,
	    ebpf_fill_probeinfo, &req->probe_iter.info);

	/* Indicates the end of the probe list. */
	if (error == ECHILD) {
		req->probe_iter.info.id = EBPF_PROBE_FIRST;
		return (0);
	}

	return (error);
}

int
ebpf_ioctl(struct ebpf_env *ee, uint32_t cmd, void *data, ebpf_thread *td)
{
	int error;
	union ebpf_req *req = (union ebpf_req *)data;

	if (data == NULL || td == NULL) {
		return EINVAL;
	}

	switch (cmd) {
	case EBPFIOC_LOAD_PROG:
		error = ebpf_ioc_load_prog(ee, req, td);
		break;
	case EBPFIOC_MAP_CREATE:
		error = ebpf_ioc_map_create(ee, req, td);
		break;
	case EBPFIOC_MAP_LOOKUP_ELEM:
		error = ebpf_ioc_map_lookup_elem(req, td);
		break;
	case EBPFIOC_MAP_UPDATE_ELEM:
		error = ebpf_ioc_map_update_elem(req, td);
		break;
	case EBPFIOC_MAP_DELETE_ELEM:
		error = ebpf_ioc_map_delete_elem(req, td);
		break;
	case EBPFIOC_MAP_GET_NEXT_KEY:
		error = ebpf_ioc_map_get_next_key(req, td);
		break;
	case EBPFIOC_GET_MAP_TYPE_INFO:
		error = ebpf_ioc_get_map_type_info(ee, req);
		break;
	case EBPFIOC_GET_PROG_TYPE_INFO:
		error = ebpf_ioc_get_prog_type_info(ee, req);
		break;
	case EBPFIOC_ATTACH_PROBE:
		error = ebpf_attach(req, td);
		break;
	case EBPFIOC_PROBE_BY_NAME:
		error = ebpf_probe_by_name(req, td);
		break;
	case EBPFIOC_PROBE_ITER:
		error = ebpf_probe_iter(req, td);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}
