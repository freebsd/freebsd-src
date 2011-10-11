/*-
 * Copyright (c) 2001  The FreeBSD Project
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/systm.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_util.h>

DUMMY(setfsuid16);
DUMMY(setfsgid16);
DUMMY(getresuid16);
DUMMY(getresgid16);

#define	CAST_NOCHG(x)	((x == 0xFFFF) ? -1 : x)

int
linux_chown16(struct thread *td, struct linux_chown16_args *args)
{
	char *path;
	int error;

	LCONVPATHEXIST(td, args->path, &path);

#ifdef DEBUG
	if (ldebug(chown16))
		printf(ARGS(chown16, "%s, %d, %d"), path, args->uid, args->gid);
#endif
	error = kern_chown(td, path, UIO_SYSSPACE, CAST_NOCHG(args->uid),
	    CAST_NOCHG(args->gid));
	LFREEPATH(path);
	return (error);
}

int
linux_lchown16(struct thread *td, struct linux_lchown16_args *args)
{
	char *path;
	int error;

	LCONVPATHEXIST(td, args->path, &path);

#ifdef DEBUG
	if (ldebug(lchown16))
		printf(ARGS(lchown16, "%s, %d, %d"), path, args->uid,
		    args->gid);
#endif
	error = kern_lchown(td, path, UIO_SYSSPACE, CAST_NOCHG(args->uid),
	    CAST_NOCHG(args->gid));
	LFREEPATH(path);
	return (error);
}

int
linux_setgroups16(struct thread *td, struct linux_setgroups16_args *args)
{
	struct ucred *newcred, *oldcred;
	l_gid16_t *linux_gidset;
	gid_t *bsd_gidset;
	int ngrp, error;
	struct proc *p;

#ifdef DEBUG
	if (ldebug(setgroups16))
		printf(ARGS(setgroups16, "%d, *"), args->gidsetsize);
#endif

	ngrp = args->gidsetsize;
	if (ngrp < 0 || ngrp >= ngroups_max + 1)
		return (EINVAL);
	linux_gidset = malloc(ngrp * sizeof(*linux_gidset), M_TEMP, M_WAITOK);
	error = copyin(args->gidset, linux_gidset, ngrp * sizeof(l_gid16_t));
	if (error)
		free(linux_gidset, M_TEMP);
		return (error);
	newcred = crget();
	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

	/*
	 * cr_groups[0] holds egid. Setting the whole set from
	 * the supplied set will cause egid to be changed too.
	 * Keep cr_groups[0] unchanged to prevent that.
	 */

	if ((error = priv_check_cred(oldcred, PRIV_CRED_SETGROUPS, 0)) != 0) {
		PROC_UNLOCK(p);
		crfree(newcred);
		goto out;
	}

	if (ngrp > 0) {
		newcred->cr_ngroups = ngrp + 1;

		bsd_gidset = newcred->cr_groups;
		ngrp--;
		while (ngrp >= 0) {
			bsd_gidset[ngrp + 1] = linux_gidset[ngrp];
			ngrp--;
		}
	}
	else
		newcred->cr_ngroups = 1;

	setsugid(td->td_proc);
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	crfree(oldcred);
	error = 0;
out:
	free(linux_gidset, M_TEMP);
	return (error);
}

int
linux_getgroups16(struct thread *td, struct linux_getgroups16_args *args)
{
	struct ucred *cred;
	l_gid16_t *linux_gidset;
	gid_t *bsd_gidset;
	int bsd_gidsetsz, ngrp, error;

#ifdef DEBUG
	if (ldebug(getgroups16))
		printf(ARGS(getgroups16, "%d, *"), args->gidsetsize);
#endif

	cred = td->td_ucred;
	bsd_gidset = cred->cr_groups;
	bsd_gidsetsz = cred->cr_ngroups - 1;

	/*
	 * cr_groups[0] holds egid. Returning the whole set
	 * here will cause a duplicate. Exclude cr_groups[0]
	 * to prevent that.
	 */

	if ((ngrp = args->gidsetsize) == 0) {
		td->td_retval[0] = bsd_gidsetsz;
		return (0);
	}

	if (ngrp < bsd_gidsetsz)
		return (EINVAL);

	ngrp = 0;
	linux_gidset = malloc(bsd_gidsetsz * sizeof(*linux_gidset),
	    M_TEMP, M_WAITOK);
	while (ngrp < bsd_gidsetsz) {
		linux_gidset[ngrp] = bsd_gidset[ngrp + 1];
		ngrp++;
	}

	error = copyout(linux_gidset, args->gidset, ngrp * sizeof(l_gid16_t));
	free(linux_gidset, M_TEMP);
	if (error)
		return (error);

	td->td_retval[0] = ngrp;
	return (0);
}

/*
 * The FreeBSD native getgid(2) and getuid(2) also modify td->td_retval[1]
 * when COMPAT_43 is defined. This clobbers registers that are assumed to
 * be preserved. The following lightweight syscalls fixes this. See also
 * linux_getpid(2), linux_getgid(2) and linux_getuid(2) in linux_misc.c
 *
 * linux_getgid16() - MP SAFE
 * linux_getuid16() - MP SAFE
 */

int
linux_getgid16(struct thread *td, struct linux_getgid16_args *args)
{

	td->td_retval[0] = td->td_ucred->cr_rgid;
	return (0);
}

int
linux_getuid16(struct thread *td, struct linux_getuid16_args *args)
{

	td->td_retval[0] = td->td_ucred->cr_ruid;
	return (0);
}

int
linux_getegid16(struct thread *td, struct linux_getegid16_args *args)
{
	struct getegid_args bsd;

	return (sys_getegid(td, &bsd));
}

int
linux_geteuid16(struct thread *td, struct linux_geteuid16_args *args)
{
	struct geteuid_args bsd;

	return (sys_geteuid(td, &bsd));
}

int
linux_setgid16(struct thread *td, struct linux_setgid16_args *args)
{
	struct setgid_args bsd;

	bsd.gid = args->gid;
	return (sys_setgid(td, &bsd));
}

int
linux_setuid16(struct thread *td, struct linux_setuid16_args *args)
{
	struct setuid_args bsd;

	bsd.uid = args->uid;
	return (sys_setuid(td, &bsd));
}

int
linux_setregid16(struct thread *td, struct linux_setregid16_args *args)
{
	struct setregid_args bsd;

	bsd.rgid = CAST_NOCHG(args->rgid);
	bsd.egid = CAST_NOCHG(args->egid);
	return (sys_setregid(td, &bsd));
}

int
linux_setreuid16(struct thread *td, struct linux_setreuid16_args *args)
{
	struct setreuid_args bsd;

	bsd.ruid = CAST_NOCHG(args->ruid);
	bsd.euid = CAST_NOCHG(args->euid);
	return (sys_setreuid(td, &bsd));
}

int
linux_setresgid16(struct thread *td, struct linux_setresgid16_args *args)
{
	struct setresgid_args bsd;

	bsd.rgid = CAST_NOCHG(args->rgid);
	bsd.egid = CAST_NOCHG(args->egid);
	bsd.sgid = CAST_NOCHG(args->sgid);
	return (sys_setresgid(td, &bsd));
}

int
linux_setresuid16(struct thread *td, struct linux_setresuid16_args *args)
{
	struct setresuid_args bsd;

	bsd.ruid = CAST_NOCHG(args->ruid);
	bsd.euid = CAST_NOCHG(args->euid);
	bsd.suid = CAST_NOCHG(args->suid);
	return (sys_setresuid(td, &bsd));
}
