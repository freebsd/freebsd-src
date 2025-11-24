/*-
 * Copyright (c) 1999-2002, 2006, 2009 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001-2005 Networks Associates Technology, Inc.
 * Copyright (c) 2005-2006 SPARTA, Inc.
 * Copyright (c) 2008 Apple Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * This software was enhanced by SPARTA ISSO under SPAWAR contract 
 * N66001-04-C-6019 ("SEFOS").
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
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

#include <sys/cdefs.h>
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/abi_compat.h>
#include <sys/capsicum.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/socket.h>
#include <sys/pipe.h>
#include <sys/socketvar.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>
#include <security/mac/mac_syscalls.h>

#ifdef MAC

FEATURE(security_mac, "Mandatory Access Control Framework support");

static int	kern___mac_get_path(struct thread *td, const char *path_p,
		    struct mac *mac_p, int follow);
static int	kern___mac_set_path(struct thread *td, const char *path_p,
		    struct mac *mac_p, int follow);

#ifdef COMPAT_FREEBSD32
struct mac32 {
	uint32_t	m_buflen;	/* size_t */
	uint32_t	m_string;	/* char * */
};
#endif

/*
 * Copyin a 'struct mac', including the string pointed to by 'm_string'.
 *
 * On success (0 returned), fills '*mac', whose associated storage must be freed
 * after use by calling free_copied_label() (which see).  On success, 'u_string'
 * if not NULL is filled with the userspace address for 'u_mac->m_string'.
 */
int
mac_label_copyin(const void *const u_mac, struct mac *const mac,
    char **const u_string)
{
	char *buffer;
	int error;

#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32)) {
		struct mac32 mac32;

		error = copyin(u_mac, &mac32, sizeof(mac32));
		if (error != 0)
			return (error);

		CP(mac32, *mac, m_buflen);
		PTRIN_CP(mac32, *mac, m_string);
	} else
#endif
	{
		error = copyin(u_mac, mac, sizeof(*mac));
		if (error != 0)
			return (error);
	}

	error = mac_check_structmac_consistent(mac);
	if (error != 0)
		return (error);

	/* 'm_buflen' not too big checked by function call above. */
	buffer = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac->m_string, buffer, mac->m_buflen, NULL);
	if (error != 0) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	MPASS(error == 0);
	if (u_string != NULL)
		*u_string = mac->m_string;
	mac->m_string = buffer;
	return (0);
}

void
free_copied_label(const struct mac *const mac)
{
	free(mac->m_string, M_MACTEMP);
}

int
sys___mac_get_pid(struct thread *td, struct __mac_get_pid_args *uap)
{
	char *buffer, *u_buffer;
	struct mac mac;
	struct proc *tproc;
	struct ucred *tcred;
	int error;

	error = mac_label_copyin(uap->mac_p, &mac, &u_buffer);
	if (error)
		return (error);

	tproc = pfind(uap->pid);
	if (tproc == NULL) {
		error = ESRCH;
		goto free_mac_and_exit;
	}

	tcred = NULL;				/* Satisfy gcc. */
	error = p_cansee(td, tproc);
	if (error == 0)
		tcred = crhold(tproc->p_ucred);
	PROC_UNLOCK(tproc);
	if (error)
		goto free_mac_and_exit;

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_cred_externalize_label(tcred->cr_label, mac.m_string,
	    buffer, mac.m_buflen);
	if (error == 0)
		error = copyout(buffer, u_buffer, strlen(buffer)+1);
	free(buffer, M_MACTEMP);
	crfree(tcred);

free_mac_and_exit:
	free_copied_label(&mac);
	return (error);
}

int
sys___mac_get_proc(struct thread *td, struct __mac_get_proc_args *uap)
{
	char *buffer, *u_buffer;
	struct mac mac;
	int error;

	error = mac_label_copyin(uap->mac_p, &mac, &u_buffer);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_cred_externalize_label(td->td_ucred->cr_label,
	    mac.m_string, buffer, mac.m_buflen);
	if (error == 0)
		error = copyout(buffer, u_buffer, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free_copied_label(&mac);
	return (error);
}

/*
 * Performs preparation (including allocations) for mac_set_proc().
 *
 * No lock should be held while calling this function.  On success,
 * mac_set_proc_finish() must be called to free the data associated to
 * 'mac_set_proc_data', even if mac_set_proc_core() fails.  'mac_set_proc_data'
 * is not set in case of error, and is set to a non-NULL value on success.
 */
int
mac_set_proc_prepare(struct thread *const td, const struct mac *const mac,
    void **const mac_set_proc_data)
{
	struct label *intlabel;
	int error;

	PROC_LOCK_ASSERT(td->td_proc, MA_NOTOWNED);

	if (!(mac_labeled & MPC_OBJECT_CRED))
		return (EINVAL);

	intlabel = mac_cred_label_alloc();
	error = mac_cred_internalize_label(intlabel, mac->m_string);
	if (error) {
		mac_cred_label_free(intlabel);
		return (error);
	}

	*mac_set_proc_data = intlabel;
	return (0);
}

/*
 * Actually sets the MAC label on 'newcred'.
 *
 * The current process' lock *must* be held.  This function only sets the label
 * on 'newcred', but does not put 'newcred' in place on the current process'
 * (consequently, it also does not call setsugid()).  'mac_set_proc_data' must
 * be the pointer returned by mac_set_proc_prepare().  If called, this function
 * must be so between a successful call to mac_set_proc_prepare() and
 * mac_set_proc_finish(), but calling it is not mandatory (e.g., if some other
 * error occured under the process lock that obsoletes setting the MAC label).
 */
int
mac_set_proc_core(struct thread *const td, struct ucred *const newcred,
    void *const mac_set_proc_data)
{
	struct label *const intlabel = mac_set_proc_data;
	struct proc *const p = td->td_proc;
	int error;

	MPASS(td == curthread);
	PROC_LOCK_ASSERT(p, MA_OWNED);

	error = mac_cred_check_relabel(p->p_ucred, intlabel);
	if (error)
		return (error);

	mac_cred_relabel(newcred, intlabel);
	return (0);
}

/*
 * Performs mac_set_proc() last operations, without the process lock.
 *
 * 'proc_label_set' indicates whether the label was actually set by a call to
 * mac_set_proc_core() that succeeded.  'mac_set_proc_data' must be the pointer
 * returned by mac_set_proc_prepare(), and its associated data will be freed.
 */
void
mac_set_proc_finish(struct thread *const td, bool proc_label_set,
    void *const mac_set_proc_data)
{
	struct label *const intlabel = mac_set_proc_data;

	PROC_LOCK_ASSERT(td->td_proc, MA_NOTOWNED);

	if (proc_label_set)
		mac_proc_vm_revoke(td);
	mac_cred_label_free(intlabel);
}

int
sys___mac_set_proc(struct thread *td, struct __mac_set_proc_args *uap)
{
	struct ucred *newcred, *oldcred;
	void *intlabel;
	struct proc *const p = td->td_proc;
	struct mac mac;
	int error;

	error = mac_label_copyin(uap->mac_p, &mac, NULL);
	if (error)
		return (error);

	error = mac_set_proc_prepare(td, &mac, &intlabel);
	if (error)
		goto free_label;

	newcred = crget();

	PROC_LOCK(p);
	oldcred = p->p_ucred;
	crcopy(newcred, oldcred);

	error = mac_set_proc_core(td, newcred, intlabel);
	if (error) {
		PROC_UNLOCK(p);
		crfree(newcred);
		goto finish;
	}

	setsugid(p);
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);

	crfree(oldcred);
finish:
	mac_set_proc_finish(td, error == 0, intlabel);
free_label:
	free_copied_label(&mac);
	return (error);
}

int
sys___mac_get_fd(struct thread *td, struct __mac_get_fd_args *uap)
{
	char *u_buffer, *buffer;
	struct label *intlabel;
	struct file *fp;
	struct mac mac;
	struct vnode *vp;
	struct pipe *pipe;
	struct socket *so;
	cap_rights_t rights;
	int error;

	error = mac_label_copyin(uap->mac_p, &mac, &u_buffer);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = fget(td, uap->fd, cap_rights_init_one(&rights, CAP_MAC_GET),
	    &fp);
	if (error)
		goto out;

	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		if (!(mac_labeled & MPC_OBJECT_VNODE)) {
			error = EINVAL;
			goto out_fdrop;
		}
		vp = fp->f_vnode;
		intlabel = mac_vnode_label_alloc();
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		mac_vnode_copy_label(vp->v_label, intlabel);
		VOP_UNLOCK(vp);
		error = mac_vnode_externalize_label(intlabel, mac.m_string,
		    buffer, mac.m_buflen);
		mac_vnode_label_free(intlabel);
		break;

	case DTYPE_PIPE:
		if (!(mac_labeled & MPC_OBJECT_PIPE)) {
			error = EINVAL;
			goto out_fdrop;
		}
		pipe = fp->f_data;
		intlabel = mac_pipe_label_alloc();
		PIPE_LOCK(pipe);
		mac_pipe_copy_label(pipe->pipe_pair->pp_label, intlabel);
		PIPE_UNLOCK(pipe);
		error = mac_pipe_externalize_label(intlabel, mac.m_string,
		    buffer, mac.m_buflen);
		mac_pipe_label_free(intlabel);
		break;

	case DTYPE_SOCKET:
		if (!(mac_labeled & MPC_OBJECT_SOCKET)) {
			error = EINVAL;
			goto out_fdrop;
		}
		so = fp->f_data;
		intlabel = mac_socket_label_alloc(M_WAITOK);
		SOCK_LOCK(so);
		mac_socket_copy_label(so->so_label, intlabel);
		SOCK_UNLOCK(so);
		error = mac_socket_externalize_label(intlabel, mac.m_string,
		    buffer, mac.m_buflen);
		mac_socket_label_free(intlabel);
		break;

	default:
		error = EINVAL;
	}
	if (error == 0)
		error = copyout(buffer, u_buffer, strlen(buffer)+1);
out_fdrop:
	fdrop(fp, td);
out:
	free(buffer, M_MACTEMP);
	free_copied_label(&mac);
	return (error);
}

int
sys___mac_get_file(struct thread *td, struct __mac_get_file_args *uap)
{

	return (kern___mac_get_path(td, uap->path_p, uap->mac_p, FOLLOW));
}

int
sys___mac_get_link(struct thread *td, struct __mac_get_link_args *uap)
{

	return (kern___mac_get_path(td, uap->path_p, uap->mac_p, NOFOLLOW));
}

static int
kern___mac_get_path(struct thread *td, const char *path_p, struct mac *mac_p,
   int follow)
{
	char *u_buffer, *buffer;
	struct nameidata nd;
	struct label *intlabel;
	struct mac mac;
	int error;

	if (!(mac_labeled & MPC_OBJECT_VNODE))
		return (EINVAL);

	error = mac_label_copyin(mac_p, &mac, &u_buffer);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	NDINIT(&nd, LOOKUP, LOCKLEAF | follow, UIO_USERSPACE, path_p);
	error = namei(&nd);
	if (error)
		goto out;

	intlabel = mac_vnode_label_alloc();
	mac_vnode_copy_label(nd.ni_vp->v_label, intlabel);
	error = mac_vnode_externalize_label(intlabel, mac.m_string, buffer,
	    mac.m_buflen);
	vput(nd.ni_vp);
	NDFREE_PNBUF(&nd);
	mac_vnode_label_free(intlabel);

	if (error == 0)
		error = copyout(buffer, u_buffer, strlen(buffer)+1);

out:
	free(buffer, M_MACTEMP);
	free_copied_label(&mac);

	return (error);
}

int
sys___mac_set_fd(struct thread *td, struct __mac_set_fd_args *uap)
{
	struct label *intlabel;
	struct pipe *pipe;
	struct socket *so;
	struct file *fp;
	struct mount *mp;
	struct vnode *vp;
	struct mac mac;
	cap_rights_t rights;
	int error;

	error = mac_label_copyin(uap->mac_p, &mac, NULL);
	if (error)
		return (error);

	error = fget(td, uap->fd, cap_rights_init_one(&rights, CAP_MAC_SET),
	    &fp);
	if (error)
		goto out;

	switch (fp->f_type) {
	case DTYPE_FIFO:
	case DTYPE_VNODE:
		if (!(mac_labeled & MPC_OBJECT_VNODE)) {
			error = EINVAL;
			goto out_fdrop;
		}
		intlabel = mac_vnode_label_alloc();
		error = mac_vnode_internalize_label(intlabel, mac.m_string);
		if (error) {
			mac_vnode_label_free(intlabel);
			break;
		}
		vp = fp->f_vnode;
		error = vn_start_write(vp, &mp, V_WAIT | V_PCATCH);
		if (error != 0) {
			mac_vnode_label_free(intlabel);
			break;
		}
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		error = vn_setlabel(vp, intlabel, td->td_ucred);
		VOP_UNLOCK(vp);
		vn_finished_write(mp);
		mac_vnode_label_free(intlabel);
		break;

	case DTYPE_PIPE:
		if (!(mac_labeled & MPC_OBJECT_PIPE)) {
			error = EINVAL;
			goto out_fdrop;
		}
		intlabel = mac_pipe_label_alloc();
		error = mac_pipe_internalize_label(intlabel, mac.m_string);
		if (error == 0) {
			pipe = fp->f_data;
			PIPE_LOCK(pipe);
			error = mac_pipe_label_set(td->td_ucred,
			    pipe->pipe_pair, intlabel);
			PIPE_UNLOCK(pipe);
		}
		mac_pipe_label_free(intlabel);
		break;

	case DTYPE_SOCKET:
		if (!(mac_labeled & MPC_OBJECT_SOCKET)) {
			error = EINVAL;
			goto out_fdrop;
		}
		intlabel = mac_socket_label_alloc(M_WAITOK);
		error = mac_socket_internalize_label(intlabel, mac.m_string);
		if (error == 0) {
			so = fp->f_data;
			error = mac_socket_label_set(td->td_ucred, so,
			    intlabel);
		}
		mac_socket_label_free(intlabel);
		break;

	default:
		error = EINVAL;
	}
out_fdrop:
	fdrop(fp, td);
out:
	free_copied_label(&mac);
	return (error);
}

int
sys___mac_set_file(struct thread *td, struct __mac_set_file_args *uap)
{

	return (kern___mac_set_path(td, uap->path_p, uap->mac_p, FOLLOW));
}

int
sys___mac_set_link(struct thread *td, struct __mac_set_link_args *uap)
{

	return (kern___mac_set_path(td, uap->path_p, uap->mac_p, NOFOLLOW));
}

static int
kern___mac_set_path(struct thread *td, const char *path_p, struct mac *mac_p,
    int follow)
{
	struct label *intlabel;
	struct nameidata nd;
	struct mount *mp;
	struct mac mac;
	int error;

	if (!(mac_labeled & MPC_OBJECT_VNODE))
		return (EINVAL);

	error = mac_label_copyin(mac_p, &mac, NULL);
	if (error)
		return (error);

	intlabel = mac_vnode_label_alloc();
	error = mac_vnode_internalize_label(intlabel, mac.m_string);
	free_copied_label(&mac);
	if (error)
		goto out;

	NDINIT(&nd, LOOKUP, LOCKLEAF | follow, UIO_USERSPACE, path_p);
	error = namei(&nd);
	if (error == 0) {
		error = vn_start_write(nd.ni_vp, &mp, V_WAIT | V_PCATCH);
		if (error == 0) {
			error = vn_setlabel(nd.ni_vp, intlabel,
			    td->td_ucred);
			vn_finished_write(mp);
		}
		vput(nd.ni_vp);
		NDFREE_PNBUF(&nd);
	}
out:
	mac_vnode_label_free(intlabel);
	return (error);
}

int
sys_mac_syscall(struct thread *td, struct mac_syscall_args *uap)
{
	struct mac_policy_conf *mpc;
	char target[MAC_MAX_POLICY_NAME];
	int error;

	error = copyinstr(uap->policy, target, sizeof(target), NULL);
	if (error)
		return (error);

	error = ENOSYS;
	LIST_FOREACH(mpc, &mac_static_policy_list, mpc_list) {
		if (strcmp(mpc->mpc_name, target) == 0 &&
		    mpc->mpc_ops->mpo_syscall != NULL) {
			error = mpc->mpc_ops->mpo_syscall(td,
			    uap->call, uap->arg);
			goto out;
		}
	}

	if (!LIST_EMPTY(&mac_policy_list)) {
		mac_policy_slock_sleep();
		LIST_FOREACH(mpc, &mac_policy_list, mpc_list) {
			if (strcmp(mpc->mpc_name, target) == 0 &&
			    mpc->mpc_ops->mpo_syscall != NULL) {
				error = mpc->mpc_ops->mpo_syscall(td,
				    uap->call, uap->arg);
				break;
			}
		}
		mac_policy_sunlock_sleep();
	}
out:
	return (error);
}

#else /* !MAC */

int
sys___mac_get_pid(struct thread *td, struct __mac_get_pid_args *uap)
{

	return (ENOSYS);
}

int
sys___mac_get_proc(struct thread *td, struct __mac_get_proc_args *uap)
{

	return (ENOSYS);
}

int
sys___mac_set_proc(struct thread *td, struct __mac_set_proc_args *uap)
{

	return (ENOSYS);
}

int
sys___mac_get_fd(struct thread *td, struct __mac_get_fd_args *uap)
{

	return (ENOSYS);
}

int
sys___mac_get_file(struct thread *td, struct __mac_get_file_args *uap)
{

	return (ENOSYS);
}

int
sys___mac_get_link(struct thread *td, struct __mac_get_link_args *uap)
{

	return (ENOSYS);
}

int
sys___mac_set_fd(struct thread *td, struct __mac_set_fd_args *uap)
{

	return (ENOSYS);
}

int
sys___mac_set_file(struct thread *td, struct __mac_set_file_args *uap)
{

	return (ENOSYS);
}

int
sys___mac_set_link(struct thread *td, struct __mac_set_link_args *uap)
{

	return (ENOSYS);
}

int
sys_mac_syscall(struct thread *td, struct mac_syscall_args *uap)
{

	return (ENOSYS);
}

#endif /* !MAC */
