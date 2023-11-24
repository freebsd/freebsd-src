/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_dtrace.h>
#include <compat/linux/linux_util.h>

/* DTrace init */
LIN_SDT_PROVIDER_DECLARE(LINUX_DTRACE);

/**
 * DTrace probes in this module.
 */
LIN_SDT_PROBE_DEFINE1(uid16, linux_chown16, conv_path, "char *");
LIN_SDT_PROBE_DEFINE1(uid16, linux_lchown16, conv_path, "char *");
LIN_SDT_PROBE_DEFINE1(uid16, linux_setgroups16, copyin_error, "int");
LIN_SDT_PROBE_DEFINE1(uid16, linux_setgroups16, priv_check_cred_error, "int");
LIN_SDT_PROBE_DEFINE1(uid16, linux_getgroups16, copyout_error, "int");

DUMMY(setfsuid16);
DUMMY(setfsgid16);
DUMMY(getresuid16);
DUMMY(getresgid16);

#define	CAST_NOCHG(x)	((x == 0xFFFF) ? -1 : x)

int
linux_chown16(struct thread *td, struct linux_chown16_args *args)
{

	return (kern_fchownat(td, AT_FDCWD, args->path, UIO_USERSPACE,
	    CAST_NOCHG(args->uid), CAST_NOCHG(args->gid), 0));
}

int
linux_lchown16(struct thread *td, struct linux_lchown16_args *args)
{

	return (kern_fchownat(td, AT_FDCWD, args->path, UIO_USERSPACE,
	    CAST_NOCHG(args->uid), CAST_NOCHG(args->gid), AT_SYMLINK_NOFOLLOW));
}

int
linux_setgroups16(struct thread *td, struct linux_setgroups16_args *args)
{
	struct ucred *newcred, *oldcred;
	l_gid16_t *linux_gidset;
	gid_t *bsd_gidset;
	int ngrp, error;
	struct proc *p;

	ngrp = args->gidsetsize;
	if (ngrp < 0 || ngrp >= ngroups_max + 1)
		return (EINVAL);
	linux_gidset = malloc(ngrp * sizeof(*linux_gidset), M_LINUX, M_WAITOK);
	error = copyin(args->gidset, linux_gidset, ngrp * sizeof(l_gid16_t));
	if (error) {
		LIN_SDT_PROBE1(uid16, linux_setgroups16, copyin_error, error);
		free(linux_gidset, M_LINUX);
		return (error);
	}
	newcred = crget();
	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

	/*
	 * cr_groups[0] holds egid. Setting the whole set from
	 * the supplied set will cause egid to be changed too.
	 * Keep cr_groups[0] unchanged to prevent that.
	 */

	if ((error = priv_check_cred(oldcred, PRIV_CRED_SETGROUPS)) != 0) {
		PROC_UNLOCK(p);
		crfree(newcred);

		LIN_SDT_PROBE1(uid16, linux_setgroups16, priv_check_cred_error,
		    error);
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
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);
	crfree(oldcred);
	error = 0;
out:
	free(linux_gidset, M_LINUX);

	return (error);
}

int
linux_getgroups16(struct thread *td, struct linux_getgroups16_args *args)
{
	struct ucred *cred;
	l_gid16_t *linux_gidset;
	gid_t *bsd_gidset;
	int bsd_gidsetsz, ngrp, error;

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
	    M_LINUX, M_WAITOK);
	while (ngrp < bsd_gidsetsz) {
		linux_gidset[ngrp] = bsd_gidset[ngrp + 1];
		ngrp++;
	}

	error = copyout(linux_gidset, args->gidset, ngrp * sizeof(l_gid16_t));
	free(linux_gidset, M_LINUX);
	if (error) {
		LIN_SDT_PROBE1(uid16, linux_getgroups16, copyout_error, error);
		return (error);
	}

	td->td_retval[0] = ngrp;

	return (0);
}

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
	int error;

	error = sys_getegid(td, &bsd);

	return (error);
}

int
linux_geteuid16(struct thread *td, struct linux_geteuid16_args *args)
{
	struct geteuid_args bsd;
	int error;

	error = sys_geteuid(td, &bsd);

	return (error);
}

int
linux_setgid16(struct thread *td, struct linux_setgid16_args *args)
{
	struct setgid_args bsd;
	int error;

	bsd.gid = args->gid;
	error = sys_setgid(td, &bsd);

	return (error);
}

int
linux_setuid16(struct thread *td, struct linux_setuid16_args *args)
{
	struct setuid_args bsd;
	int error;

	bsd.uid = args->uid;
	error = sys_setuid(td, &bsd);

	return (error);
}

int
linux_setregid16(struct thread *td, struct linux_setregid16_args *args)
{
	struct setregid_args bsd;
	int error;

	bsd.rgid = CAST_NOCHG(args->rgid);
	bsd.egid = CAST_NOCHG(args->egid);
	error = sys_setregid(td, &bsd);

	return (error);
}

int
linux_setreuid16(struct thread *td, struct linux_setreuid16_args *args)
{
	struct setreuid_args bsd;
	int error;

	bsd.ruid = CAST_NOCHG(args->ruid);
	bsd.euid = CAST_NOCHG(args->euid);
	error = sys_setreuid(td, &bsd);

	return (error);
}

int
linux_setresgid16(struct thread *td, struct linux_setresgid16_args *args)
{
	struct setresgid_args bsd;
	int error;

	bsd.rgid = CAST_NOCHG(args->rgid);
	bsd.egid = CAST_NOCHG(args->egid);
	bsd.sgid = CAST_NOCHG(args->sgid);
	error = sys_setresgid(td, &bsd);

	return (error);
}

int
linux_setresuid16(struct thread *td, struct linux_setresuid16_args *args)
{
	struct setresuid_args bsd;
	int error;

	bsd.ruid = CAST_NOCHG(args->ruid);
	bsd.euid = CAST_NOCHG(args->euid);
	bsd.suid = CAST_NOCHG(args->suid);
	error = sys_setresuid(td, &bsd);

	return (error);
}
