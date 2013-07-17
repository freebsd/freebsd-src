/*-
 * Copyright (c) 2008-2011 Robert N. M. Watson
 * Copyright (c) 2010-2011 Jonathan Anderson
 * Copyright (c) 2012 FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Portions of this software were developed by Pawel Jakub Dawidek under
 * sponsorship from the FreeBSD Foundation.
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

/*
 * FreeBSD kernel capability facility.
 *
 * Two kernel features are implemented here: capability mode, a sandboxed mode
 * of execution for processes, and capabilities, a refinement on file
 * descriptors that allows fine-grained control over operations on the file
 * descriptor.  Collectively, these allow processes to run in the style of a
 * historic "capability system" in which they can use only resources
 * explicitly delegated to them.  This model is enforced by restricting access
 * to global namespaces in capability mode.
 *
 * Capabilities wrap other file descriptor types, binding them to a constant
 * rights mask set when the capability is created.  New capabilities may be
 * derived from existing capabilities, but only if they have the same or a
 * strict subset of the rights on the original capability.
 *
 * System calls permitted in capability mode are defined in capabilities.conf;
 * calls must be carefully audited for safety to ensure that they don't allow
 * escape from a sandbox.  Some calls permit only a subset of operations in
 * capability mode -- for example, shm_open(2) is limited to creating
 * anonymous, rather than named, POSIX shared memory objects.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_capsicum.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/uio.h>
#include <sys/ktrace.h>

#include <security/audit/audit.h>

#include <vm/uma.h>
#include <vm/vm.h>

#ifdef CAPABILITY_MODE

FEATURE(security_capability_mode, "Capsicum Capability Mode");

/*
 * System call to enter capability mode for the process.
 */
int
sys_cap_enter(struct thread *td, struct cap_enter_args *uap)
{
	struct ucred *newcred, *oldcred;
	struct proc *p;

	if (IN_CAPABILITY_MODE(td))
		return (0);

	newcred = crget();
	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	crcopy(newcred, oldcred);
	newcred->cr_flags |= CRED_FLAG_CAPMODE;
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);
}

/*
 * System call to query whether the process is in capability mode.
 */
int
sys_cap_getmode(struct thread *td, struct cap_getmode_args *uap)
{
	u_int i;

	i = IN_CAPABILITY_MODE(td) ? 1 : 0;
	return (copyout(&i, uap->modep, sizeof(i)));
}

#else /* !CAPABILITY_MODE */

int
sys_cap_enter(struct thread *td, struct cap_enter_args *uap)
{

	return (ENOSYS);
}

int
sys_cap_getmode(struct thread *td, struct cap_getmode_args *uap)
{

	return (ENOSYS);
}

#endif /* CAPABILITY_MODE */

#ifdef CAPABILITIES

FEATURE(security_capabilities, "Capsicum Capabilities");

MALLOC_DECLARE(M_FILECAPS);

static inline int
_cap_check(cap_rights_t have, cap_rights_t need, enum ktr_cap_fail_type type)
{


	if ((need & ~have) != 0) {
#ifdef KTRACE
		if (KTRPOINT(curthread, KTR_CAPFAIL))
			ktrcapfail(type, need, have);
#endif
		return (ENOTCAPABLE);
	}
	return (0);
}

/*
 * Test whether a capability grants the requested rights.
 */
int
cap_check(cap_rights_t have, cap_rights_t need)
{

	return (_cap_check(have, need, CAPFAIL_NOTCAPABLE));
}

/*
 * Convert capability rights into VM access flags.
 */
u_char
cap_rights_to_vmprot(cap_rights_t have)
{
	u_char maxprot;

	maxprot = VM_PROT_NONE;
	if (have & CAP_MMAP_R)
		maxprot |= VM_PROT_READ;
	if (have & CAP_MMAP_W)
		maxprot |= VM_PROT_WRITE;
	if (have & CAP_MMAP_X)
		maxprot |= VM_PROT_EXECUTE;

	return (maxprot);
}

/*
 * Extract rights from a capability for monitoring purposes -- not for use in
 * any other way, as we want to keep all capability permission evaluation in
 * this one file.
 */
cap_rights_t
cap_rights(struct filedesc *fdp, int fd)
{

	return (fdp->fd_ofiles[fd].fde_rights);
}

/*
 * System call to limit rights of the given capability.
 */
int
sys_cap_rights_limit(struct thread *td, struct cap_rights_limit_args *uap)
{
	struct filedesc *fdp;
	cap_rights_t rights;
	int error, fd;

	fd = uap->fd;
	rights = uap->rights;

	AUDIT_ARG_FD(fd);
	AUDIT_ARG_RIGHTS(rights);

	if ((rights & ~CAP_ALL) != 0)
		return (EINVAL);

	fdp = td->td_proc->p_fd;
	FILEDESC_XLOCK(fdp);
	if (fget_locked(fdp, fd) == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}
	error = _cap_check(cap_rights(fdp, fd), rights, CAPFAIL_INCREASE);
	if (error == 0) {
		fdp->fd_ofiles[fd].fde_rights = rights;
		if ((rights & CAP_IOCTL) == 0) {
			free(fdp->fd_ofiles[fd].fde_ioctls, M_FILECAPS);
			fdp->fd_ofiles[fd].fde_ioctls = NULL;
			fdp->fd_ofiles[fd].fde_nioctls = 0;
		}
		if ((rights & CAP_FCNTL) == 0)
			fdp->fd_ofiles[fd].fde_fcntls = 0;
	}
	FILEDESC_XUNLOCK(fdp);
	return (error);
}

/*
 * System call to query the rights mask associated with a capability.
 */
int
sys_cap_rights_get(struct thread *td, struct cap_rights_get_args *uap)
{
	struct filedesc *fdp;
	cap_rights_t rights;
	int fd;

	fd = uap->fd;

	AUDIT_ARG_FD(fd);

	fdp = td->td_proc->p_fd;
	FILEDESC_SLOCK(fdp);
	if (fget_locked(fdp, fd) == NULL) {
		FILEDESC_SUNLOCK(fdp);
		return (EBADF);
	}
	rights = cap_rights(fdp, fd);
	FILEDESC_SUNLOCK(fdp);
	return (copyout(&rights, uap->rightsp, sizeof(*uap->rightsp)));
}

/*
 * Test whether a capability grants the given ioctl command.
 * If descriptor doesn't have CAP_IOCTL, then ioctls list is empty and
 * ENOTCAPABLE will be returned.
 */
int
cap_ioctl_check(struct filedesc *fdp, int fd, u_long cmd)
{
	u_long *cmds;
	ssize_t ncmds;
	long i;

	FILEDESC_LOCK_ASSERT(fdp);
	KASSERT(fd >= 0 && fd < fdp->fd_nfiles,
	    ("%s: invalid fd=%d", __func__, fd));

	ncmds = fdp->fd_ofiles[fd].fde_nioctls;
	if (ncmds == -1)
		return (0);

	cmds = fdp->fd_ofiles[fd].fde_ioctls;
	for (i = 0; i < ncmds; i++) {
		if (cmds[i] == cmd)
			return (0);
	}

	return (ENOTCAPABLE);
}

/*
 * Check if the current ioctls list can be replaced by the new one.
 */
static int
cap_ioctl_limit_check(struct filedesc *fdp, int fd, const u_long *cmds,
    size_t ncmds)
{
	u_long *ocmds;
	ssize_t oncmds;
	u_long i;
	long j;

	oncmds = fdp->fd_ofiles[fd].fde_nioctls;
	if (oncmds == -1)
		return (0);
	if (oncmds < (ssize_t)ncmds)
		return (ENOTCAPABLE);

	ocmds = fdp->fd_ofiles[fd].fde_ioctls;
	for (i = 0; i < ncmds; i++) {
		for (j = 0; j < oncmds; j++) {
			if (cmds[i] == ocmds[j])
				break;
		}
		if (j == oncmds)
			return (ENOTCAPABLE);
	}

	return (0);
}

int
sys_cap_ioctls_limit(struct thread *td, struct cap_ioctls_limit_args *uap)
{
	struct filedesc *fdp;
	u_long *cmds, *ocmds;
	size_t ncmds;
	int error, fd;

	fd = uap->fd;
	ncmds = uap->ncmds;

	AUDIT_ARG_FD(fd);

	if (ncmds > 256)	/* XXX: Is 256 sane? */
		return (EINVAL);

	if (ncmds == 0) {
		cmds = NULL;
	} else {
		cmds = malloc(sizeof(cmds[0]) * ncmds, M_FILECAPS, M_WAITOK);
		error = copyin(uap->cmds, cmds, sizeof(cmds[0]) * ncmds);
		if (error != 0) {
			free(cmds, M_FILECAPS);
			return (error);
		}
	}

	fdp = td->td_proc->p_fd;
	FILEDESC_XLOCK(fdp);

	if (fget_locked(fdp, fd) == NULL) {
		error = EBADF;
		goto out;
	}

	error = cap_ioctl_limit_check(fdp, fd, cmds, ncmds);
	if (error != 0)
		goto out;

	ocmds = fdp->fd_ofiles[fd].fde_ioctls;
	fdp->fd_ofiles[fd].fde_ioctls = cmds;
	fdp->fd_ofiles[fd].fde_nioctls = ncmds;

	cmds = ocmds;
	error = 0;
out:
	FILEDESC_XUNLOCK(fdp);
	free(cmds, M_FILECAPS);
	return (error);
}

int
sys_cap_ioctls_get(struct thread *td, struct cap_ioctls_get_args *uap)
{
	struct filedesc *fdp;
	struct filedescent *fdep;
	u_long *cmds;
	size_t maxcmds;
	int error, fd;

	fd = uap->fd;
	cmds = uap->cmds;
	maxcmds = uap->maxcmds;

	AUDIT_ARG_FD(fd);

	fdp = td->td_proc->p_fd;
	FILEDESC_SLOCK(fdp);

	if (fget_locked(fdp, fd) == NULL) {
		error = EBADF;
		goto out;
	}

	/*
	 * If all ioctls are allowed (fde_nioctls == -1 && fde_ioctls == NULL)
	 * the only sane thing we can do is to not populate the given array and
	 * return CAP_IOCTLS_ALL.
	 */

	fdep = &fdp->fd_ofiles[fd];
	if (cmds != NULL && fdep->fde_ioctls != NULL) {
		error = copyout(fdep->fde_ioctls, cmds,
		    sizeof(cmds[0]) * MIN(fdep->fde_nioctls, maxcmds));
		if (error != 0)
			goto out;
	}
	if (fdep->fde_nioctls == -1)
		td->td_retval[0] = CAP_IOCTLS_ALL;
	else
		td->td_retval[0] = fdep->fde_nioctls;

	error = 0;
out:
	FILEDESC_SUNLOCK(fdp);
	return (error);
}

/*
 * Test whether a capability grants the given fcntl command.
 */
int
cap_fcntl_check(struct filedesc *fdp, int fd, int cmd)
{
	uint32_t fcntlcap;

	KASSERT(fd >= 0 && fd < fdp->fd_nfiles,
	    ("%s: invalid fd=%d", __func__, fd));

	fcntlcap = (1 << cmd);
	KASSERT((CAP_FCNTL_ALL & fcntlcap) != 0,
	    ("Unsupported fcntl=%d.", cmd));

	if ((fdp->fd_ofiles[fd].fde_fcntls & fcntlcap) != 0)
		return (0);

	return (ENOTCAPABLE);
}

int
sys_cap_fcntls_limit(struct thread *td, struct cap_fcntls_limit_args *uap)
{
	struct filedesc *fdp;
	uint32_t fcntlrights;
	int fd;

	fd = uap->fd;
	fcntlrights = uap->fcntlrights;

	AUDIT_ARG_FD(fd);
	AUDIT_ARG_FCNTL_RIGHTS(fcntlrights);

	if ((fcntlrights & ~CAP_FCNTL_ALL) != 0)
		return (EINVAL);

	fdp = td->td_proc->p_fd;
	FILEDESC_XLOCK(fdp);

	if (fget_locked(fdp, fd) == NULL) {
		FILEDESC_XUNLOCK(fdp);
		return (EBADF);
	}

	if ((fcntlrights & ~fdp->fd_ofiles[fd].fde_fcntls) != 0) {
		FILEDESC_XUNLOCK(fdp);
		return (ENOTCAPABLE);
	}

	fdp->fd_ofiles[fd].fde_fcntls = fcntlrights;
	FILEDESC_XUNLOCK(fdp);

	return (0);
}

int
sys_cap_fcntls_get(struct thread *td, struct cap_fcntls_get_args *uap)
{
	struct filedesc *fdp;
	uint32_t rights;
	int fd;

	fd = uap->fd;

	AUDIT_ARG_FD(fd);

	fdp = td->td_proc->p_fd;
	FILEDESC_SLOCK(fdp);
	if (fget_locked(fdp, fd) == NULL) {
		FILEDESC_SUNLOCK(fdp);
		return (EBADF);
	}
	rights = fdp->fd_ofiles[fd].fde_fcntls;
	FILEDESC_SUNLOCK(fdp);

	return (copyout(&rights, uap->fcntlrightsp, sizeof(rights)));
}

/*
 * For backward compatibility.
 */
int
sys_cap_new(struct thread *td, struct cap_new_args *uap)
{
	struct filedesc *fdp;
	cap_rights_t rights;
	register_t newfd;
	int error, fd;

	fd = uap->fd;
	rights = uap->rights;

	AUDIT_ARG_FD(fd);
	AUDIT_ARG_RIGHTS(rights);

	if ((rights & ~CAP_ALL) != 0)
		return (EINVAL);

	fdp = td->td_proc->p_fd;
	FILEDESC_SLOCK(fdp);
	if (fget_locked(fdp, fd) == NULL) {
		FILEDESC_SUNLOCK(fdp);
		return (EBADF);
	}
	error = _cap_check(cap_rights(fdp, fd), rights, CAPFAIL_INCREASE);
	FILEDESC_SUNLOCK(fdp);
	if (error != 0)
		return (error);

	error = do_dup(td, 0, fd, 0, &newfd);
	if (error != 0)
		return (error);

	FILEDESC_XLOCK(fdp);
	/*
	 * We don't really care about the race between checking capability
	 * rights for the source descriptor and now. If capability rights
	 * were ok at that earlier point, the process had this descriptor
	 * with those rights, so we don't increase them in security sense,
	 * the process might have done the cap_new(2) a bit earlier to get
	 * the same effect.
	 */
	fdp->fd_ofiles[newfd].fde_rights = rights;
	if ((rights & CAP_IOCTL) == 0) {
		free(fdp->fd_ofiles[newfd].fde_ioctls, M_FILECAPS);
		fdp->fd_ofiles[newfd].fde_ioctls = NULL;
		fdp->fd_ofiles[newfd].fde_nioctls = 0;
	}
	if ((rights & CAP_FCNTL) == 0)
		fdp->fd_ofiles[newfd].fde_fcntls = 0;
	FILEDESC_XUNLOCK(fdp);

	td->td_retval[0] = newfd;

	return (0);
}

#else /* !CAPABILITIES */

/*
 * Stub Capability functions for when options CAPABILITIES isn't compiled
 * into the kernel.
 */

int
sys_cap_rights_limit(struct thread *td, struct cap_rights_limit_args *uap)
{

	return (ENOSYS);
}

int
sys_cap_rights_get(struct thread *td, struct cap_rights_get_args *uap)
{

	return (ENOSYS);
}

int
sys_cap_ioctls_limit(struct thread *td, struct cap_ioctls_limit_args *uap)
{

	return (ENOSYS);
}

int
sys_cap_ioctls_get(struct thread *td, struct cap_ioctls_get_args *uap)
{

	return (ENOSYS);
}

int
sys_cap_fcntls_limit(struct thread *td, struct cap_fcntls_limit_args *uap)
{

	return (ENOSYS);
}

int
sys_cap_fcntls_get(struct thread *td, struct cap_fcntls_get_args *uap)
{

	return (ENOSYS);
}

int
sys_cap_new(struct thread *td, struct cap_new_args *uap)
{

	return (ENOSYS);
}

#endif /* CAPABILITIES */
