/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1989, 1990, 1991, 1993
 *	The Regents of the University of California.
 * (c) UNIX System Laboratories, Inc.
 * Copyright (c) 2000-2001 Robert N. M. Watson.
 * All rights reserved.
 *
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * System calls related to processes and protection
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/abi_compat.h>
#include <sys/acct.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/loginclass.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/ptrace.h>
#include <sys/refcount.h>
#include <sys/sx.h>
#include <sys/priv.h>
#include <sys/proc.h>
#ifdef COMPAT_43
#include <sys/sysent.h>
#endif
#include <sys/sysproto.h>
#include <sys/jail.h>
#include <sys/racct.h>
#include <sys/rctl.h>
#include <sys/resourcevar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>

#ifdef MAC
#include <security/mac/mac_syscalls.h>
#endif

#include <vm/uma.h>

#ifdef REGRESSION
FEATURE(regression,
    "Kernel support for interfaces necessary for regression testing (SECURITY RISK!)");
#endif

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

static MALLOC_DEFINE(M_CRED, "cred", "credentials");

SYSCTL_NODE(_security, OID_AUTO, bsd, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "BSD security policy");

static void crfree_final(struct ucred *cr);

static inline void
groups_check_positive_len(int ngrp)
{
	MPASS2(ngrp >= 0, "negative number of groups");
}
static inline void
groups_check_max_len(int ngrp)
{
	MPASS2(ngrp <= ngroups_max, "too many supplementary groups");
}

static void groups_normalize(int *ngrp, gid_t *groups);
static void crsetgroups_internal(struct ucred *cr, int ngrp,
    const gid_t *groups);

static int cr_canseeotheruids(struct ucred *u1, struct ucred *u2);
static int cr_canseeothergids(struct ucred *u1, struct ucred *u2);
static int cr_canseejailproc(struct ucred *u1, struct ucred *u2);

#ifndef _SYS_SYSPROTO_H_
struct getpid_args {
	int	dummy;
};
#endif
/* ARGSUSED */
int
sys_getpid(struct thread *td, struct getpid_args *uap)
{
	struct proc *p = td->td_proc;

	td->td_retval[0] = p->p_pid;
#if defined(COMPAT_43)
	if (SV_PROC_FLAG(p, SV_AOUT))
		td->td_retval[1] = kern_getppid(td);
#endif
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getppid_args {
        int     dummy;
};
#endif
/* ARGSUSED */
int
sys_getppid(struct thread *td, struct getppid_args *uap)
{

	td->td_retval[0] = kern_getppid(td);
	return (0);
}

int
kern_getppid(struct thread *td)
{
	struct proc *p = td->td_proc;

	return (p->p_oppid);
}

/*
 * Get process group ID; note that POSIX getpgrp takes no parameter.
 */
#ifndef _SYS_SYSPROTO_H_
struct getpgrp_args {
        int     dummy;
};
#endif
int
sys_getpgrp(struct thread *td, struct getpgrp_args *uap)
{
	struct proc *p = td->td_proc;

	PROC_LOCK(p);
	td->td_retval[0] = p->p_pgrp->pg_id;
	PROC_UNLOCK(p);
	return (0);
}

/* Get an arbitrary pid's process group id */
#ifndef _SYS_SYSPROTO_H_
struct getpgid_args {
	pid_t	pid;
};
#endif
int
sys_getpgid(struct thread *td, struct getpgid_args *uap)
{
	struct proc *p;
	int error;

	if (uap->pid == 0) {
		p = td->td_proc;
		PROC_LOCK(p);
	} else {
		p = pfind(uap->pid);
		if (p == NULL)
			return (ESRCH);
		error = p_cansee(td, p);
		if (error) {
			PROC_UNLOCK(p);
			return (error);
		}
	}
	td->td_retval[0] = p->p_pgrp->pg_id;
	PROC_UNLOCK(p);
	return (0);
}

/*
 * Get an arbitrary pid's session id.
 */
#ifndef _SYS_SYSPROTO_H_
struct getsid_args {
	pid_t	pid;
};
#endif
int
sys_getsid(struct thread *td, struct getsid_args *uap)
{

	return (kern_getsid(td, uap->pid));
}

int
kern_getsid(struct thread *td, pid_t pid)
{
	struct proc *p;
	int error;

	if (pid == 0) {
		p = td->td_proc;
		PROC_LOCK(p);
	} else {
		p = pfind(pid);
		if (p == NULL)
			return (ESRCH);
		error = p_cansee(td, p);
		if (error) {
			PROC_UNLOCK(p);
			return (error);
		}
	}
	td->td_retval[0] = p->p_session->s_sid;
	PROC_UNLOCK(p);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getuid_args {
        int     dummy;
};
#endif
/* ARGSUSED */
int
sys_getuid(struct thread *td, struct getuid_args *uap)
{

	td->td_retval[0] = td->td_ucred->cr_ruid;
#if defined(COMPAT_43)
	td->td_retval[1] = td->td_ucred->cr_uid;
#endif
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct geteuid_args {
        int     dummy;
};
#endif
/* ARGSUSED */
int
sys_geteuid(struct thread *td, struct geteuid_args *uap)
{

	td->td_retval[0] = td->td_ucred->cr_uid;
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getgid_args {
        int     dummy;
};
#endif
/* ARGSUSED */
int
sys_getgid(struct thread *td, struct getgid_args *uap)
{

	td->td_retval[0] = td->td_ucred->cr_rgid;
#if defined(COMPAT_43)
	td->td_retval[1] = td->td_ucred->cr_gid;
#endif
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getegid_args {
        int     dummy;
};
#endif
/* ARGSUSED */
int
sys_getegid(struct thread *td, struct getegid_args *uap)
{

	td->td_retval[0] = td->td_ucred->cr_gid;
	return (0);
}

#ifdef COMPAT_FREEBSD14
int
freebsd14_getgroups(struct thread *td, struct freebsd14_getgroups_args *uap)
{
	struct ucred *cred;
	int ngrp, error;

	cred = td->td_ucred;

	/*
	 * For FreeBSD < 15.0, we account for the egid being placed at the
	 * beginning of the group list prior to all supplementary groups.
	 */
	ngrp = cred->cr_ngroups + 1;
	if (uap->gidsetsize == 0) {
		error = 0;
		goto out;
	} else if (uap->gidsetsize < ngrp) {
		return (EINVAL);
	}

	error = copyout(&cred->cr_gid, uap->gidset, sizeof(gid_t));
	if (error == 0)
		error = copyout(cred->cr_groups, uap->gidset + 1,
		    (ngrp - 1) * sizeof(gid_t));

out:
	td->td_retval[0] = ngrp;
	return (error);

}
#endif	/* COMPAT_FREEBSD14 */

#ifndef _SYS_SYSPROTO_H_
struct getgroups_args {
	int	gidsetsize;
	gid_t	*gidset;
};
#endif
int
sys_getgroups(struct thread *td, struct getgroups_args *uap)
{
	struct ucred *cred;
	int ngrp, error;

	cred = td->td_ucred;

	ngrp = cred->cr_ngroups;
	if (uap->gidsetsize == 0) {
		error = 0;
		goto out;
	}
	if (uap->gidsetsize < ngrp)
		return (EINVAL);

	error = copyout(cred->cr_groups, uap->gidset, ngrp * sizeof(gid_t));
out:
	td->td_retval[0] = ngrp;
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setsid_args {
        int     dummy;
};
#endif
/* ARGSUSED */
int
sys_setsid(struct thread *td, struct setsid_args *uap)
{
	struct pgrp *pgrp;
	int error;
	struct proc *p = td->td_proc;
	struct pgrp *newpgrp;
	struct session *newsess;

	pgrp = NULL;

	newpgrp = uma_zalloc(pgrp_zone, M_WAITOK);
	newsess = malloc(sizeof(struct session), M_SESSION, M_WAITOK | M_ZERO);

again:
	error = 0;
	sx_xlock(&proctree_lock);

	if (p->p_pgid == p->p_pid || (pgrp = pgfind(p->p_pid)) != NULL) {
		if (pgrp != NULL)
			PGRP_UNLOCK(pgrp);
		error = EPERM;
	} else {
		error = enterpgrp(p, p->p_pid, newpgrp, newsess);
		if (error == ERESTART)
			goto again;
		MPASS(error == 0);
		td->td_retval[0] = p->p_pid;
		newpgrp = NULL;
		newsess = NULL;
	}

	sx_xunlock(&proctree_lock);

	uma_zfree(pgrp_zone, newpgrp);
	free(newsess, M_SESSION);

	return (error);
}

/*
 * set process group (setpgid/old setpgrp)
 *
 * caller does setpgid(targpid, targpgid)
 *
 * pid must be caller or child of caller (ESRCH)
 * if a child
 *	pid must be in same session (EPERM)
 *	pid can't have done an exec (EACCES)
 * if pgid != pid
 * 	there must exist some pid in same session having pgid (EPERM)
 * pid must not be session leader (EPERM)
 */
#ifndef _SYS_SYSPROTO_H_
struct setpgid_args {
	int	pid;		/* target process id */
	int	pgid;		/* target pgrp id */
};
#endif
/* ARGSUSED */
int
sys_setpgid(struct thread *td, struct setpgid_args *uap)
{
	struct proc *curp = td->td_proc;
	struct proc *targp;	/* target process */
	struct pgrp *pgrp;	/* target pgrp */
	int error;
	struct pgrp *newpgrp;

	if (uap->pgid < 0)
		return (EINVAL);

	newpgrp = uma_zalloc(pgrp_zone, M_WAITOK);

again:
	error = 0;

	sx_xlock(&proctree_lock);
	if (uap->pid != 0 && uap->pid != curp->p_pid) {
		if ((targp = pfind(uap->pid)) == NULL) {
			error = ESRCH;
			goto done;
		}
		if (!inferior(targp)) {
			PROC_UNLOCK(targp);
			error = ESRCH;
			goto done;
		}
		if ((error = p_cansee(td, targp))) {
			PROC_UNLOCK(targp);
			goto done;
		}
		if (targp->p_pgrp == NULL ||
		    targp->p_session != curp->p_session) {
			PROC_UNLOCK(targp);
			error = EPERM;
			goto done;
		}
		if (targp->p_flag & P_EXEC) {
			PROC_UNLOCK(targp);
			error = EACCES;
			goto done;
		}
		PROC_UNLOCK(targp);
	} else
		targp = curp;
	if (SESS_LEADER(targp)) {
		error = EPERM;
		goto done;
	}
	if (uap->pgid == 0)
		uap->pgid = targp->p_pid;
	if ((pgrp = pgfind(uap->pgid)) == NULL) {
		if (uap->pgid == targp->p_pid) {
			error = enterpgrp(targp, uap->pgid, newpgrp,
			    NULL);
			if (error == 0)
				newpgrp = NULL;
		} else
			error = EPERM;
	} else {
		if (pgrp == targp->p_pgrp) {
			PGRP_UNLOCK(pgrp);
			goto done;
		}
		if (pgrp->pg_id != targp->p_pid &&
		    pgrp->pg_session != curp->p_session) {
			PGRP_UNLOCK(pgrp);
			error = EPERM;
			goto done;
		}
		PGRP_UNLOCK(pgrp);
		error = enterthispgrp(targp, pgrp);
	}
done:
	KASSERT(error == 0 || newpgrp != NULL,
	    ("setpgid failed and newpgrp is NULL"));
	if (error == ERESTART)
		goto again;
	sx_xunlock(&proctree_lock);
	uma_zfree(pgrp_zone, newpgrp);
	return (error);
}

static int
gidp_cmp(const void *p1, const void *p2)
{
	const gid_t g1 = *(const gid_t *)p1;
	const gid_t g2 = *(const gid_t *)p2;

	return ((g1 > g2) - (g1 < g2));
}

/*
 * Final storage for supplementary groups will be returned via 'groups'.
 * '*groups' must be NULL on input, and if not equal to 'smallgroups'
 * on output, must be freed (M_TEMP) *even if* an error is returned.
 */
static int
kern_setcred_copyin_supp_groups(struct setcred *const wcred,
    const u_int flags, gid_t *const smallgroups, gid_t **const groups)
{
	MPASS(*groups == NULL);

	if (flags & SETCREDF_SUPP_GROUPS) {
		int error;

		/*
		 * Check for the limit for number of groups right now in order
		 * to limit the amount of bytes to copy.
		 */
		if (wcred->sc_supp_groups_nb > ngroups_max)
			return (EINVAL);

		/*
		 * Since we are going to be copying the supplementary groups
		 * from userland, make room also for the effective GID right
		 * now, to avoid having to allocate and copy again the
		 * supplementary groups.
		 */
		*groups = wcred->sc_supp_groups_nb <= CRED_SMALLGROUPS_NB ?
		    smallgroups : malloc(wcred->sc_supp_groups_nb *
		    sizeof(*groups), M_TEMP, M_WAITOK);

		error = copyin(wcred->sc_supp_groups, *groups,
		    wcred->sc_supp_groups_nb * sizeof(*groups));
		if (error != 0)
			return (error);
		wcred->sc_supp_groups = *groups;
	} else {
		wcred->sc_supp_groups_nb = 0;
		wcred->sc_supp_groups = NULL;
	}

	return (0);
}

int
user_setcred(struct thread *td, const u_int flags,
    const void *const uwcred, const size_t size, bool is_32bit)
{
	struct setcred wcred;
#ifdef MAC
	struct mac mac;
	/* Pointer to 'struct mac' or 'struct mac32'. */
	void *umac;
#endif
	gid_t smallgroups[CRED_SMALLGROUPS_NB];
	gid_t *groups = NULL;
	int error;

	/*
	 * As the only point of this wrapper function is to copyin() from
	 * userland, we only interpret the data pieces we need to perform this
	 * operation and defer further sanity checks to kern_setcred(), except
	 * that we redundantly check here that no unknown flags have been
	 * passed.
	 */
	if ((flags & ~SETCREDF_MASK) != 0)
		return (EINVAL);

#ifdef COMPAT_FREEBSD32
	if (is_32bit) {
		struct setcred32 wcred32;

		if (size != sizeof(wcred32))
			return (EINVAL);
		error = copyin(uwcred, &wcred32, sizeof(wcred32));
		if (error != 0)
			return (error);
		/* These fields have exactly the same sizes and positions. */
		memcpy(&wcred, &wcred32, __rangeof(struct setcred32,
		    setcred32_copy_start, setcred32_copy_end));
		/* Remaining fields are pointers and need PTRIN*(). */
		PTRIN_CP(wcred32, wcred, sc_supp_groups);
		PTRIN_CP(wcred32, wcred, sc_label);
	} else
#endif /* COMPAT_FREEBSD32 */
	{
		if (size != sizeof(wcred))
			return (EINVAL);
		error = copyin(uwcred, &wcred, sizeof(wcred));
		if (error != 0)
			return (error);
	}
#ifdef MAC
	umac = wcred.sc_label;
#endif
	/* Also done on !MAC as a defensive measure. */
	wcred.sc_label = NULL;

	/*
	 * Copy supplementary groups as needed.  There is no specific
	 * alternative for 32-bit compatibility as 'gid_t' has the same size
	 * everywhere.
	 */
	error = kern_setcred_copyin_supp_groups(&wcred, flags, smallgroups,
	    &groups);
	if (error != 0)
		goto free_groups;

#ifdef MAC
	if ((flags & SETCREDF_MAC_LABEL) != 0) {
#ifdef COMPAT_FREEBSD32
		if (is_32bit)
			error = mac_label_copyin32(umac, &mac, NULL);
		else
#endif
			error = mac_label_copyin(umac, &mac, NULL);
		if (error != 0)
			goto free_groups;
		wcred.sc_label = &mac;
	}
#endif

	error = kern_setcred(td, flags, &wcred, groups);

#ifdef MAC
	if (wcred.sc_label != NULL)
		free_copied_label(wcred.sc_label);
#endif

free_groups:
	if (groups != smallgroups)
		free(groups, M_TEMP);

	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setcred_args {
	u_int			 flags;	/* Flags. */
	const struct setcred	*wcred;
	size_t			 size;	/* Passed 'setcred' structure length. */
};
#endif
/* ARGSUSED */
int
sys_setcred(struct thread *td, struct setcred_args *uap)
{
	return (user_setcred(td, uap->flags, uap->wcred, uap->size, false));
}

/*
 * CAUTION: This function normalizes groups in 'wcred'.
 *
 * If 'preallocated_groups' is non-NULL, it must be an already allocated array
 * of size 'wcred->sc_supp_groups_nb' containing the supplementary groups, and
 * 'wcred->sc_supp_groups' then must point to it.
 */
int
kern_setcred(struct thread *const td, const u_int flags,
    struct setcred *const wcred, gid_t *preallocated_groups)
{
	struct proc *const p = td->td_proc;
	struct ucred *new_cred, *old_cred, *to_free_cred;
	struct uidinfo *uip = NULL, *ruip = NULL;
#ifdef MAC
	void *mac_set_proc_data = NULL;
	bool proc_label_set = false;
#endif
	gid_t *groups = NULL;
	gid_t smallgroups[CRED_SMALLGROUPS_NB];
	int error;
	bool cred_set = false;

	/* Bail out on unrecognized flags. */
	if (flags & ~SETCREDF_MASK)
		return (EINVAL);

	/*
	 * Part 1: We allocate and perform preparatory operations with no locks.
	 */

	if (flags & SETCREDF_SUPP_GROUPS) {
		if (wcred->sc_supp_groups_nb > ngroups_max)
			return (EINVAL);
		if (preallocated_groups != NULL) {
			groups = preallocated_groups;
			MPASS(preallocated_groups == wcred->sc_supp_groups);
		} else {
			if (wcred->sc_supp_groups_nb <= CRED_SMALLGROUPS_NB)
				groups = smallgroups;
			else
				groups = malloc(wcred->sc_supp_groups_nb *
				    sizeof(*groups), M_TEMP, M_WAITOK);
			memcpy(groups, wcred->sc_supp_groups,
			    wcred->sc_supp_groups_nb * sizeof(*groups));
		}
	}

	if (flags & SETCREDF_MAC_LABEL) {
#ifdef MAC
		error = mac_set_proc_prepare(td, wcred->sc_label,
		    &mac_set_proc_data);
		if (error != 0)
			goto free_groups;
#else
		error = ENOTSUP;
		goto free_groups;
#endif
	}

	if (flags & SETCREDF_UID) {
		AUDIT_ARG_EUID(wcred->sc_uid);
		uip = uifind(wcred->sc_uid);
	}
	if (flags & SETCREDF_RUID) {
		AUDIT_ARG_RUID(wcred->sc_ruid);
		ruip = uifind(wcred->sc_ruid);
	}
	if (flags & SETCREDF_SVUID)
		AUDIT_ARG_SUID(wcred->sc_svuid);

	if (flags & SETCREDF_GID)
		AUDIT_ARG_EGID(wcred->sc_gid);
	if (flags & SETCREDF_RGID)
		AUDIT_ARG_RGID(wcred->sc_rgid);
	if (flags & SETCREDF_SVGID)
		AUDIT_ARG_SGID(wcred->sc_svgid);
	if (flags & SETCREDF_SUPP_GROUPS) {
		/*
		 * Output the raw supplementary groups array for better
		 * traceability.
		 */
		AUDIT_ARG_GROUPSET(groups, wcred->sc_supp_groups_nb);
		groups_normalize(&wcred->sc_supp_groups_nb, groups);
	}

	/*
	 * We first completely build the new credentials and only then pass them
	 * to MAC along with the old ones so that modules can check whether the
	 * requested transition is allowed.
	 */
	new_cred = crget();
	to_free_cred = new_cred;
	if (flags & SETCREDF_SUPP_GROUPS)
		crextend(new_cred, wcred->sc_supp_groups_nb);

#ifdef MAC
	mac_cred_setcred_enter();
#endif

	/*
	 * Part 2: We grab the process lock as to have a stable view of its
	 * current credentials, and prepare a copy of them with the requested
	 * changes applied under that lock.
	 */

	PROC_LOCK(p);
	old_cred = crcopysafe(p, new_cred);

	/*
	 * Change user IDs.
	 */
	if (flags & SETCREDF_UID)
		change_euid(new_cred, uip);
	if (flags & SETCREDF_RUID)
		change_ruid(new_cred, ruip);
	if (flags & SETCREDF_SVUID)
		change_svuid(new_cred, wcred->sc_svuid);

	/*
	 * Change groups.
	 */
	if (flags & SETCREDF_SUPP_GROUPS)
		crsetgroups_internal(new_cred, wcred->sc_supp_groups_nb,
		    groups);
	if (flags & SETCREDF_GID)
		change_egid(new_cred, wcred->sc_gid);
	if (flags & SETCREDF_RGID)
		change_rgid(new_cred, wcred->sc_rgid);
	if (flags & SETCREDF_SVGID)
		change_svgid(new_cred, wcred->sc_svgid);

#ifdef MAC
	/*
	 * Change the MAC label.
	 */
	if (flags & SETCREDF_MAC_LABEL) {
		error = mac_set_proc_core(td, new_cred, mac_set_proc_data);
		if (error != 0)
			goto unlock_finish;
		proc_label_set = true;
	}

	/*
	 * MAC security modules checks.
	 */
	error = mac_cred_check_setcred(flags, old_cred, new_cred);
	if (error != 0)
		goto unlock_finish;
#endif
	/*
	 * Privilege check.
	 */
	error = priv_check_cred(old_cred, PRIV_CRED_SETCRED);
	if (error != 0)
		goto unlock_finish;

	/*
	 * Set the new credentials, noting that they have changed.
	 */
	cred_set = proc_set_cred_enforce_proc_lim(p, new_cred);
	if (cred_set) {
		setsugid(p);
		to_free_cred = old_cred;
#ifdef RACCT
		racct_proc_ucred_changed(p, old_cred, new_cred);
#endif
#ifdef RCTL
		crhold(new_cred);
#endif
		MPASS(error == 0);
	} else
		error = EAGAIN;

unlock_finish:
	PROC_UNLOCK(p);

	/*
	 * Part 3: After releasing the process lock, we perform cleanups and
	 * finishing operations.
	 */

#ifdef RCTL
	if (cred_set) {
		rctl_proc_ucred_changed(p, new_cred);
		/* Paired with the crhold() just above. */
		crfree(new_cred);
	}
#endif

#ifdef MAC
	if (mac_set_proc_data != NULL)
		mac_set_proc_finish(td, proc_label_set, mac_set_proc_data);
	mac_cred_setcred_exit();
#endif
	crfree(to_free_cred);
	if (uip != NULL)
		uifree(uip);
	if (ruip != NULL)
		uifree(ruip);
free_groups:
	if (groups != preallocated_groups && groups != smallgroups)
		free(groups, M_TEMP); /* Deals with 'groups' being NULL. */
	return (error);
}

/*
 * Use the clause in B.4.2.2 that allows setuid/setgid to be 4.2/4.3BSD
 * compatible.  It says that setting the uid/gid to euid/egid is a special
 * case of "appropriate privilege".  Once the rules are expanded out, this
 * basically means that setuid(nnn) sets all three id's, in all permitted
 * cases unless _POSIX_SAVED_IDS is enabled.  In that case, setuid(getuid())
 * does not set the saved id - this is dangerous for traditional BSD
 * programs.  For this reason, we *really* do not want to set
 * _POSIX_SAVED_IDS and do not want to clear POSIX_APPENDIX_B_4_2_2.
 */
#define POSIX_APPENDIX_B_4_2_2

#ifndef _SYS_SYSPROTO_H_
struct setuid_args {
	uid_t	uid;
};
#endif
/* ARGSUSED */
int
sys_setuid(struct thread *td, struct setuid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t uid;
	struct uidinfo *uip;
	int error;

	uid = uap->uid;
	AUDIT_ARG_UID(uid);
	newcred = crget();
	uip = uifind(uid);
	PROC_LOCK(p);
	/*
	 * Copy credentials so other references do not see our changes.
	 */
	oldcred = crcopysafe(p, newcred);

#ifdef MAC
	error = mac_cred_check_setuid(oldcred, uid);
	if (error)
		goto fail;
#endif

	/*
	 * See if we have "permission" by POSIX 1003.1 rules.
	 *
	 * Note that setuid(geteuid()) is a special case of
	 * "appropriate privileges" in appendix B.4.2.2.  We need
	 * to use this clause to be compatible with traditional BSD
	 * semantics.  Basically, it means that "setuid(xx)" sets all
	 * three id's (assuming you have privs).
	 *
	 * Notes on the logic.  We do things in three steps.
	 * 1: We determine if the euid is going to change, and do EPERM
	 *    right away.  We unconditionally change the euid later if this
	 *    test is satisfied, simplifying that part of the logic.
	 * 2: We determine if the real and/or saved uids are going to
	 *    change.  Determined by compile options.
	 * 3: Change euid last. (after tests in #2 for "appropriate privs")
	 */
	if (uid != oldcred->cr_ruid &&		/* allow setuid(getuid()) */
#ifdef _POSIX_SAVED_IDS
	    uid != oldcred->cr_svuid &&		/* allow setuid(saved gid) */
#endif
#ifdef POSIX_APPENDIX_B_4_2_2	/* Use BSD-compat clause from B.4.2.2 */
	    uid != oldcred->cr_uid &&		/* allow setuid(geteuid()) */
#endif
	    (error = priv_check_cred(oldcred, PRIV_CRED_SETUID)) != 0)
		goto fail;

#ifdef _POSIX_SAVED_IDS
	/*
	 * Do we have "appropriate privileges" (are we root or uid == euid)
	 * If so, we are changing the real uid and/or saved uid.
	 */
	if (
#ifdef POSIX_APPENDIX_B_4_2_2	/* Use the clause from B.4.2.2 */
	    uid == oldcred->cr_uid ||
#endif
	    /* We are using privs. */
	    priv_check_cred(oldcred, PRIV_CRED_SETUID) == 0)
#endif
	{
		/*
		 * Set the real uid.
		 */
		if (uid != oldcred->cr_ruid) {
			change_ruid(newcred, uip);
			setsugid(p);
		}
		/*
		 * Set saved uid
		 *
		 * XXX always set saved uid even if not _POSIX_SAVED_IDS, as
		 * the security of seteuid() depends on it.  B.4.2.2 says it
		 * is important that we should do this.
		 */
		if (uid != oldcred->cr_svuid) {
			change_svuid(newcred, uid);
			setsugid(p);
		}
	}

	/*
	 * In all permitted cases, we are changing the euid.
	 */
	if (uid != oldcred->cr_uid) {
		change_euid(newcred, uip);
		setsugid(p);
	}
	/*
	 * This also transfers the proc count to the new user.
	 */
	proc_set_cred(p, newcred);
#ifdef RACCT
	racct_proc_ucred_changed(p, oldcred, newcred);
#endif
#ifdef RCTL
	crhold(newcred);
#endif
	PROC_UNLOCK(p);
#ifdef RCTL
	rctl_proc_ucred_changed(p, newcred);
	crfree(newcred);
#endif
	uifree(uip);
	crfree(oldcred);
	return (0);

fail:
	PROC_UNLOCK(p);
	uifree(uip);
	crfree(newcred);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct seteuid_args {
	uid_t	euid;
};
#endif
/* ARGSUSED */
int
sys_seteuid(struct thread *td, struct seteuid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t euid;
	struct uidinfo *euip;
	int error;

	euid = uap->euid;
	AUDIT_ARG_EUID(euid);
	newcred = crget();
	euip = uifind(euid);
	PROC_LOCK(p);
	/*
	 * Copy credentials so other references do not see our changes.
	 */
	oldcred = crcopysafe(p, newcred);

#ifdef MAC
	error = mac_cred_check_seteuid(oldcred, euid);
	if (error)
		goto fail;
#endif

	if (euid != oldcred->cr_ruid &&		/* allow seteuid(getuid()) */
	    euid != oldcred->cr_svuid &&	/* allow seteuid(saved uid) */
	    (error = priv_check_cred(oldcred, PRIV_CRED_SETEUID)) != 0)
		goto fail;

	/*
	 * Everything's okay, do it.
	 */
	if (oldcred->cr_uid != euid) {
		change_euid(newcred, euip);
		setsugid(p);
	}
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);
	uifree(euip);
	crfree(oldcred);
	return (0);

fail:
	PROC_UNLOCK(p);
	uifree(euip);
	crfree(newcred);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setgid_args {
	gid_t	gid;
};
#endif
/* ARGSUSED */
int
sys_setgid(struct thread *td, struct setgid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t gid;
	int error;

	gid = uap->gid;
	AUDIT_ARG_GID(gid);
	newcred = crget();
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

#ifdef MAC
	error = mac_cred_check_setgid(oldcred, gid);
	if (error)
		goto fail;
#endif

	/*
	 * See if we have "permission" by POSIX 1003.1 rules.
	 *
	 * Note that setgid(getegid()) is a special case of
	 * "appropriate privileges" in appendix B.4.2.2.  We need
	 * to use this clause to be compatible with traditional BSD
	 * semantics.  Basically, it means that "setgid(xx)" sets all
	 * three id's (assuming you have privs).
	 *
	 * For notes on the logic here, see setuid() above.
	 */
	if (gid != oldcred->cr_rgid &&		/* allow setgid(getgid()) */
#ifdef _POSIX_SAVED_IDS
	    gid != oldcred->cr_svgid &&		/* allow setgid(saved gid) */
#endif
#ifdef POSIX_APPENDIX_B_4_2_2	/* Use BSD-compat clause from B.4.2.2 */
	    gid != oldcred->cr_gid && /* allow setgid(getegid()) */
#endif
	    (error = priv_check_cred(oldcred, PRIV_CRED_SETGID)) != 0)
		goto fail;

#ifdef _POSIX_SAVED_IDS
	/*
	 * Do we have "appropriate privileges" (are we root or gid == egid)
	 * If so, we are changing the real uid and saved gid.
	 */
	if (
#ifdef POSIX_APPENDIX_B_4_2_2	/* use the clause from B.4.2.2 */
	    gid == oldcred->cr_gid ||
#endif
	    /* We are using privs. */
	    priv_check_cred(oldcred, PRIV_CRED_SETGID) == 0)
#endif
	{
		/*
		 * Set real gid
		 */
		if (oldcred->cr_rgid != gid) {
			change_rgid(newcred, gid);
			setsugid(p);
		}
		/*
		 * Set saved gid
		 *
		 * XXX always set saved gid even if not _POSIX_SAVED_IDS, as
		 * the security of setegid() depends on it.  B.4.2.2 says it
		 * is important that we should do this.
		 */
		if (oldcred->cr_svgid != gid) {
			change_svgid(newcred, gid);
			setsugid(p);
		}
	}
	/*
	 * In all cases permitted cases, we are changing the egid.
	 * Copy credentials so other references do not see our changes.
	 */
	if (oldcred->cr_gid != gid) {
		change_egid(newcred, gid);
		setsugid(p);
	}
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);

fail:
	PROC_UNLOCK(p);
	crfree(newcred);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setegid_args {
	gid_t	egid;
};
#endif
/* ARGSUSED */
int
sys_setegid(struct thread *td, struct setegid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t egid;
	int error;

	egid = uap->egid;
	AUDIT_ARG_EGID(egid);
	newcred = crget();
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

#ifdef MAC
	error = mac_cred_check_setegid(oldcred, egid);
	if (error)
		goto fail;
#endif

	if (egid != oldcred->cr_rgid &&		/* allow setegid(getgid()) */
	    egid != oldcred->cr_svgid &&	/* allow setegid(saved gid) */
	    (error = priv_check_cred(oldcred, PRIV_CRED_SETEGID)) != 0)
		goto fail;

	if (oldcred->cr_gid != egid) {
		change_egid(newcred, egid);
		setsugid(p);
	}
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);

fail:
	PROC_UNLOCK(p);
	crfree(newcred);
	return (error);
}

#ifdef COMPAT_FREEBSD14
int
freebsd14_setgroups(struct thread *td, struct freebsd14_setgroups_args *uap)
{
	gid_t smallgroups[CRED_SMALLGROUPS_NB];
	gid_t *groups;
	int gidsetsize, error;

	/*
	 * Before FreeBSD 15.0, we allow one more group to be supplied to
	 * account for the egid appearing before the supplementary groups.  This
	 * may technically allow one more supplementary group for systems that
	 * did use the default NGROUPS_MAX if we round it back up to 1024.
	 */
	gidsetsize = uap->gidsetsize;
	if (gidsetsize > ngroups_max + 1 || gidsetsize < 0)
		return (EINVAL);

	if (gidsetsize > CRED_SMALLGROUPS_NB)
		groups = malloc(gidsetsize * sizeof(gid_t), M_TEMP, M_WAITOK);
	else
		groups = smallgroups;

	error = copyin(uap->gidset, groups, gidsetsize * sizeof(gid_t));
	if (error == 0) {
		int ngroups = gidsetsize > 0 ? gidsetsize - 1 /* egid */ : 0;

		error = kern_setgroups(td, &ngroups, groups + 1);
		if (error == 0 && gidsetsize > 0)
			td->td_proc->p_ucred->cr_gid = groups[0];
	}

	if (groups != smallgroups)
		free(groups, M_TEMP);
	return (error);
}
#endif	/* COMPAT_FREEBSD14 */

#ifndef _SYS_SYSPROTO_H_
struct setgroups_args {
	int	gidsetsize;
	gid_t	*gidset;
};
#endif
/* ARGSUSED */
int
sys_setgroups(struct thread *td, struct setgroups_args *uap)
{
	gid_t smallgroups[CRED_SMALLGROUPS_NB];
	gid_t *groups;
	int gidsetsize, error;

	/*
	 * Sanity check size now to avoid passing too big a value to copyin(),
	 * even if kern_setgroups() will do it again.
	 *
	 * Ideally, the 'gidsetsize' argument should have been a 'u_int' (and it
	 * was, in this implementation, for a long time), but POSIX standardized
	 * getgroups() to take an 'int' and it would be quite entrapping to have
	 * setgroups() differ.
	 */
	gidsetsize = uap->gidsetsize;
	if (gidsetsize > ngroups_max || gidsetsize < 0)
		return (EINVAL);

	if (gidsetsize > CRED_SMALLGROUPS_NB)
		groups = malloc(gidsetsize * sizeof(gid_t), M_TEMP, M_WAITOK);
	else
		groups = smallgroups;

	error = copyin(uap->gidset, groups, gidsetsize * sizeof(gid_t));
	if (error == 0)
		error = kern_setgroups(td, &gidsetsize, groups);

	if (groups != smallgroups)
		free(groups, M_TEMP);
	return (error);
}

/*
 * CAUTION: This function normalizes 'groups', possibly also changing the value
 * of '*ngrpp' as a consequence.
 */
int
kern_setgroups(struct thread *td, int *ngrpp, gid_t *groups)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	int ngrp, error;

	ngrp = *ngrpp;
	/* Sanity check size. */
	if (ngrp < 0 || ngrp > ngroups_max)
		return (EINVAL);

	AUDIT_ARG_GROUPSET(groups, ngrp);

	groups_normalize(&ngrp, groups);
	*ngrpp = ngrp;

	newcred = crget();
	crextend(newcred, ngrp);
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

#ifdef MAC
	/*
	 * We pass NULL here explicitly if we don't have any supplementary
	 * groups mostly for the sake of normalization, but also to avoid/detect
	 * a situation where a MAC module has some assumption about the layout
	 * of `groups` matching historical behavior.
	 */
	error = mac_cred_check_setgroups(oldcred, ngrp,
	    ngrp == 0 ? NULL : groups);
	if (error)
		goto fail;
#endif

	error = priv_check_cred(oldcred, PRIV_CRED_SETGROUPS);
	if (error)
		goto fail;

	crsetgroups_internal(newcred, ngrp, groups);
	setsugid(p);
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);

fail:
	PROC_UNLOCK(p);
	crfree(newcred);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setreuid_args {
	uid_t	ruid;
	uid_t	euid;
};
#endif
/* ARGSUSED */
int
sys_setreuid(struct thread *td, struct setreuid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t euid, ruid;
	struct uidinfo *euip, *ruip;
	int error;

	euid = uap->euid;
	ruid = uap->ruid;
	AUDIT_ARG_EUID(euid);
	AUDIT_ARG_RUID(ruid);
	newcred = crget();
	euip = uifind(euid);
	ruip = uifind(ruid);
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

#ifdef MAC
	error = mac_cred_check_setreuid(oldcred, ruid, euid);
	if (error)
		goto fail;
#endif

	if (((ruid != (uid_t)-1 && ruid != oldcred->cr_ruid &&
	      ruid != oldcred->cr_svuid) ||
	     (euid != (uid_t)-1 && euid != oldcred->cr_uid &&
	      euid != oldcred->cr_ruid && euid != oldcred->cr_svuid)) &&
	    (error = priv_check_cred(oldcred, PRIV_CRED_SETREUID)) != 0)
		goto fail;

	if (euid != (uid_t)-1 && oldcred->cr_uid != euid) {
		change_euid(newcred, euip);
		setsugid(p);
	}
	if (ruid != (uid_t)-1 && oldcred->cr_ruid != ruid) {
		change_ruid(newcred, ruip);
		setsugid(p);
	}
	if ((ruid != (uid_t)-1 || newcred->cr_uid != newcred->cr_ruid) &&
	    newcred->cr_svuid != newcred->cr_uid) {
		change_svuid(newcred, newcred->cr_uid);
		setsugid(p);
	}
	proc_set_cred(p, newcred);
#ifdef RACCT
	racct_proc_ucred_changed(p, oldcred, newcred);
#endif
#ifdef RCTL
	crhold(newcred);
#endif
	PROC_UNLOCK(p);
#ifdef RCTL
	rctl_proc_ucred_changed(p, newcred);
	crfree(newcred);
#endif
	uifree(ruip);
	uifree(euip);
	crfree(oldcred);
	return (0);

fail:
	PROC_UNLOCK(p);
	uifree(ruip);
	uifree(euip);
	crfree(newcred);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setregid_args {
	gid_t	rgid;
	gid_t	egid;
};
#endif
/* ARGSUSED */
int
sys_setregid(struct thread *td, struct setregid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t egid, rgid;
	int error;

	egid = uap->egid;
	rgid = uap->rgid;
	AUDIT_ARG_EGID(egid);
	AUDIT_ARG_RGID(rgid);
	newcred = crget();
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

#ifdef MAC
	error = mac_cred_check_setregid(oldcred, rgid, egid);
	if (error)
		goto fail;
#endif

	if (((rgid != (gid_t)-1 && rgid != oldcred->cr_rgid &&
	    rgid != oldcred->cr_svgid) ||
	     (egid != (gid_t)-1 && egid != oldcred->cr_gid &&
	     egid != oldcred->cr_rgid && egid != oldcred->cr_svgid)) &&
	    (error = priv_check_cred(oldcred, PRIV_CRED_SETREGID)) != 0)
		goto fail;

	if (egid != (gid_t)-1 && oldcred->cr_gid != egid) {
		change_egid(newcred, egid);
		setsugid(p);
	}
	if (rgid != (gid_t)-1 && oldcred->cr_rgid != rgid) {
		change_rgid(newcred, rgid);
		setsugid(p);
	}
	if ((rgid != (gid_t)-1 || newcred->cr_gid != newcred->cr_rgid) &&
	    newcred->cr_svgid != newcred->cr_gid) {
		change_svgid(newcred, newcred->cr_gid);
		setsugid(p);
	}
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);

fail:
	PROC_UNLOCK(p);
	crfree(newcred);
	return (error);
}

/*
 * setresuid(ruid, euid, suid) is like setreuid except control over the saved
 * uid is explicit.
 */
#ifndef _SYS_SYSPROTO_H_
struct setresuid_args {
	uid_t	ruid;
	uid_t	euid;
	uid_t	suid;
};
#endif
/* ARGSUSED */
int
sys_setresuid(struct thread *td, struct setresuid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t euid, ruid, suid;
	struct uidinfo *euip, *ruip;
	int error;

	euid = uap->euid;
	ruid = uap->ruid;
	suid = uap->suid;
	AUDIT_ARG_EUID(euid);
	AUDIT_ARG_RUID(ruid);
	AUDIT_ARG_SUID(suid);
	newcred = crget();
	euip = uifind(euid);
	ruip = uifind(ruid);
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

#ifdef MAC
	error = mac_cred_check_setresuid(oldcred, ruid, euid, suid);
	if (error)
		goto fail;
#endif

	if (((ruid != (uid_t)-1 && ruid != oldcred->cr_ruid &&
	     ruid != oldcred->cr_svuid &&
	      ruid != oldcred->cr_uid) ||
	     (euid != (uid_t)-1 && euid != oldcred->cr_ruid &&
	    euid != oldcred->cr_svuid &&
	      euid != oldcred->cr_uid) ||
	     (suid != (uid_t)-1 && suid != oldcred->cr_ruid &&
	    suid != oldcred->cr_svuid &&
	      suid != oldcred->cr_uid)) &&
	    (error = priv_check_cred(oldcred, PRIV_CRED_SETRESUID)) != 0)
		goto fail;

	if (euid != (uid_t)-1 && oldcred->cr_uid != euid) {
		change_euid(newcred, euip);
		setsugid(p);
	}
	if (ruid != (uid_t)-1 && oldcred->cr_ruid != ruid) {
		change_ruid(newcred, ruip);
		setsugid(p);
	}
	if (suid != (uid_t)-1 && oldcred->cr_svuid != suid) {
		change_svuid(newcred, suid);
		setsugid(p);
	}
	proc_set_cred(p, newcred);
#ifdef RACCT
	racct_proc_ucred_changed(p, oldcred, newcred);
#endif
#ifdef RCTL
	crhold(newcred);
#endif
	PROC_UNLOCK(p);
#ifdef RCTL
	rctl_proc_ucred_changed(p, newcred);
	crfree(newcred);
#endif
	uifree(ruip);
	uifree(euip);
	crfree(oldcred);
	return (0);

fail:
	PROC_UNLOCK(p);
	uifree(ruip);
	uifree(euip);
	crfree(newcred);
	return (error);

}

/*
 * setresgid(rgid, egid, sgid) is like setregid except control over the saved
 * gid is explicit.
 */
#ifndef _SYS_SYSPROTO_H_
struct setresgid_args {
	gid_t	rgid;
	gid_t	egid;
	gid_t	sgid;
};
#endif
/* ARGSUSED */
int
sys_setresgid(struct thread *td, struct setresgid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t egid, rgid, sgid;
	int error;

	egid = uap->egid;
	rgid = uap->rgid;
	sgid = uap->sgid;
	AUDIT_ARG_EGID(egid);
	AUDIT_ARG_RGID(rgid);
	AUDIT_ARG_SGID(sgid);
	newcred = crget();
	PROC_LOCK(p);
	oldcred = crcopysafe(p, newcred);

#ifdef MAC
	error = mac_cred_check_setresgid(oldcred, rgid, egid, sgid);
	if (error)
		goto fail;
#endif

	if (((rgid != (gid_t)-1 && rgid != oldcred->cr_rgid &&
	      rgid != oldcred->cr_svgid &&
	      rgid != oldcred->cr_gid) ||
	     (egid != (gid_t)-1 && egid != oldcred->cr_rgid &&
	      egid != oldcred->cr_svgid &&
	      egid != oldcred->cr_gid) ||
	     (sgid != (gid_t)-1 && sgid != oldcred->cr_rgid &&
	      sgid != oldcred->cr_svgid &&
	      sgid != oldcred->cr_gid)) &&
	    (error = priv_check_cred(oldcred, PRIV_CRED_SETRESGID)) != 0)
		goto fail;

	if (egid != (gid_t)-1 && oldcred->cr_gid != egid) {
		change_egid(newcred, egid);
		setsugid(p);
	}
	if (rgid != (gid_t)-1 && oldcred->cr_rgid != rgid) {
		change_rgid(newcred, rgid);
		setsugid(p);
	}
	if (sgid != (gid_t)-1 && oldcred->cr_svgid != sgid) {
		change_svgid(newcred, sgid);
		setsugid(p);
	}
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);

fail:
	PROC_UNLOCK(p);
	crfree(newcred);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct getresuid_args {
	uid_t	*ruid;
	uid_t	*euid;
	uid_t	*suid;
};
#endif
/* ARGSUSED */
int
sys_getresuid(struct thread *td, struct getresuid_args *uap)
{
	struct ucred *cred;
	int error1 = 0, error2 = 0, error3 = 0;

	cred = td->td_ucred;
	if (uap->ruid)
		error1 = copyout(&cred->cr_ruid,
		    uap->ruid, sizeof(cred->cr_ruid));
	if (uap->euid)
		error2 = copyout(&cred->cr_uid,
		    uap->euid, sizeof(cred->cr_uid));
	if (uap->suid)
		error3 = copyout(&cred->cr_svuid,
		    uap->suid, sizeof(cred->cr_svuid));
	return (error1 ? error1 : error2 ? error2 : error3);
}

#ifndef _SYS_SYSPROTO_H_
struct getresgid_args {
	gid_t	*rgid;
	gid_t	*egid;
	gid_t	*sgid;
};
#endif
/* ARGSUSED */
int
sys_getresgid(struct thread *td, struct getresgid_args *uap)
{
	struct ucred *cred;
	int error1 = 0, error2 = 0, error3 = 0;

	cred = td->td_ucred;
	if (uap->rgid)
		error1 = copyout(&cred->cr_rgid,
		    uap->rgid, sizeof(cred->cr_rgid));
	if (uap->egid)
		error2 = copyout(&cred->cr_gid,
		    uap->egid, sizeof(cred->cr_gid));
	if (uap->sgid)
		error3 = copyout(&cred->cr_svgid,
		    uap->sgid, sizeof(cred->cr_svgid));
	return (error1 ? error1 : error2 ? error2 : error3);
}

#ifndef _SYS_SYSPROTO_H_
struct issetugid_args {
	int dummy;
};
#endif
/* ARGSUSED */
int
sys_issetugid(struct thread *td, struct issetugid_args *uap)
{
	struct proc *p = td->td_proc;

	/*
	 * Note: OpenBSD sets a P_SUGIDEXEC flag set at execve() time,
	 * we use P_SUGID because we consider changing the owners as
	 * "tainting" as well.
	 * This is significant for procs that start as root and "become"
	 * a user without an exec - programs cannot know *everything*
	 * that libc *might* have put in their data segment.
	 */
	td->td_retval[0] = (p->p_flag & P_SUGID) ? 1 : 0;
	return (0);
}

int
sys___setugid(struct thread *td, struct __setugid_args *uap)
{
#ifdef REGRESSION
	struct proc *p;

	p = td->td_proc;
	switch (uap->flag) {
	case 0:
		PROC_LOCK(p);
		p->p_flag &= ~P_SUGID;
		PROC_UNLOCK(p);
		return (0);
	case 1:
		PROC_LOCK(p);
		p->p_flag |= P_SUGID;
		PROC_UNLOCK(p);
		return (0);
	default:
		return (EINVAL);
	}
#else /* !REGRESSION */

	return (ENOSYS);
#endif /* REGRESSION */
}

#ifdef INVARIANTS
static void
groups_check_normalized(int ngrp, const gid_t *groups)
{
	gid_t prev_g;

	groups_check_positive_len(ngrp);
	groups_check_max_len(ngrp);

	if (ngrp <= 1)
		return;

	prev_g = groups[0];
	for (int i = 1; i < ngrp; ++i) {
		const gid_t g = groups[i];

		if (prev_g >= g)
			panic("%s: groups[%d] (%u) >= groups[%d] (%u)",
			    __func__, i - 1, prev_g, i, g);
		prev_g = g;
	}
}
#else
#define groups_check_normalized(...)
#endif

/*
 * Returns whether gid designates a supplementary group in cred.
 */
bool
group_is_supplementary(const gid_t gid, const struct ucred *const cred)
{

	groups_check_normalized(cred->cr_ngroups, cred->cr_groups);

	/*
	 * Perform a binary search of the supplementary groups.  This is
	 * possible because we sort the groups in crsetgroups().
	 */
	return (bsearch(&gid, cred->cr_groups, cred->cr_ngroups,
	    sizeof(gid), gidp_cmp) != NULL);
}

/*
 * Check if gid is a member of the (effective) group set (i.e., effective and
 * supplementary groups).
 */
bool
groupmember(gid_t gid, const struct ucred *cred)
{

	groups_check_positive_len(cred->cr_ngroups);

	if (gid == cred->cr_gid)
		return (true);

	return (group_is_supplementary(gid, cred));
}

/*
 * Check if gid is a member of the real group set (i.e., real and supplementary
 * groups).
 */
bool
realgroupmember(gid_t gid, const struct ucred *cred)
{
	groups_check_positive_len(cred->cr_ngroups);

	if (gid == cred->cr_rgid)
		return (true);

	return (group_is_supplementary(gid, cred));
}

/*
 * Test the active securelevel against a given level.  securelevel_gt()
 * implements (securelevel > level).  securelevel_ge() implements
 * (securelevel >= level).  Note that the logic is inverted -- these
 * functions return EPERM on "success" and 0 on "failure".
 *
 * Due to care taken when setting the securelevel, we know that no jail will
 * be less secure that its parent (or the physical system), so it is sufficient
 * to test the current jail only.
 *
 * XXXRW: Possibly since this has to do with privilege, it should move to
 * kern_priv.c.
 */
int
securelevel_gt(struct ucred *cr, int level)
{

	return (cr->cr_prison->pr_securelevel > level ? EPERM : 0);
}

int
securelevel_ge(struct ucred *cr, int level)
{

	return (cr->cr_prison->pr_securelevel >= level ? EPERM : 0);
}

/*
 * 'see_other_uids' determines whether or not visibility of processes
 * and sockets with credentials holding different real uids is possible
 * using a variety of system MIBs.
 * XXX: data declarations should be together near the beginning of the file.
 */
static int	see_other_uids = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, see_other_uids, CTLFLAG_RW,
    &see_other_uids, 0,
    "Unprivileged processes may see subjects/objects with different real uid");

/*-
 * Determine if u1 "can see" the subject specified by u2, according to the
 * 'see_other_uids' policy.
 * Returns: 0 for permitted, ESRCH otherwise
 * Locks: none
 * References: *u1 and *u2 must not change during the call
 *             u1 may equal u2, in which case only one reference is required
 */
static int
cr_canseeotheruids(struct ucred *u1, struct ucred *u2)
{

	if (!see_other_uids && u1->cr_ruid != u2->cr_ruid) {
		if (priv_check_cred(u1, PRIV_SEEOTHERUIDS) != 0)
			return (ESRCH);
	}
	return (0);
}

/*
 * 'see_other_gids' determines whether or not visibility of processes
 * and sockets with credentials holding different real gids is possible
 * using a variety of system MIBs.
 * XXX: data declarations should be together near the beginning of the file.
 */
static int	see_other_gids = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, see_other_gids, CTLFLAG_RW,
    &see_other_gids, 0,
    "Unprivileged processes may see subjects/objects with different real gid");

/*
 * Determine if u1 can "see" the subject specified by u2, according to the
 * 'see_other_gids' policy.
 * Returns: 0 for permitted, ESRCH otherwise
 * Locks: none
 * References: *u1 and *u2 must not change during the call
 *             u1 may equal u2, in which case only one reference is required
 */
static int
cr_canseeothergids(struct ucred *u1, struct ucred *u2)
{
	if (see_other_gids)
		return (0);

	/* Restriction in force. */

	if (realgroupmember(u1->cr_rgid, u2))
		return (0);

	for (int i = 0; i < u1->cr_ngroups; i++)
		if (realgroupmember(u1->cr_groups[i], u2))
			return (0);

	if (priv_check_cred(u1, PRIV_SEEOTHERGIDS) == 0)
		return (0);

	return (ESRCH);
}

/*
 * 'see_jail_proc' determines whether or not visibility of processes and
 * sockets with credentials holding different jail ids is possible using a
 * variety of system MIBs.
 *
 * XXX: data declarations should be together near the beginning of the file.
 */

static int	see_jail_proc = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, see_jail_proc, CTLFLAG_RW,
    &see_jail_proc, 0,
    "Unprivileged processes may see subjects/objects with different jail ids");

/*-
 * Determine if u1 "can see" the subject specified by u2, according to the
 * 'see_jail_proc' policy.
 * Returns: 0 for permitted, ESRCH otherwise
 * Locks: none
 * References: *u1 and *u2 must not change during the call
 *             u1 may equal u2, in which case only one reference is required
 */
static int
cr_canseejailproc(struct ucred *u1, struct ucred *u2)
{
	if (see_jail_proc || /* Policy deactivated. */
	    u1->cr_prison == u2->cr_prison || /* Same jail. */
	    priv_check_cred(u1, PRIV_SEEJAILPROC) == 0) /* Privileged. */
		return (0);

	return (ESRCH);
}

/*
 * Determine if u1 can tamper with the subject specified by u2, if they are in
 * different jails and 'unprivileged_parent_tampering' jail policy allows it.
 *
 * May be called if u1 and u2 are in the same jail, but it is expected that the
 * caller has already done a prison_check() prior to calling it.
 *
 * Returns: 0 for permitted, EPERM otherwise
 */
static int
cr_can_tamper_with_subjail(struct ucred *u1, struct ucred *u2, int priv)
{

	MPASS(prison_check(u1, u2) == 0);
	if (u1->cr_prison == u2->cr_prison)
		return (0);

	if (priv_check_cred(u1, priv) == 0)
		return (0);

	/*
	 * Jails do not maintain a distinct UID space, so process visibility is
	 * all that would control an unprivileged process' ability to tamper
	 * with a process in a subjail by default if we did not have the
	 * allow.unprivileged_parent_tampering knob to restrict it by default.
	 */
	if (prison_allow(u2, PR_ALLOW_UNPRIV_PARENT_TAMPER))
		return (0);

	return (EPERM);
}

/*
 * Helper for cr_cansee*() functions to abide by system-wide security.bsd.see_*
 * policies.  Determines if u1 "can see" u2 according to these policies.
 * Returns: 0 for permitted, ESRCH otherwise
 */
int
cr_bsd_visible(struct ucred *u1, struct ucred *u2)
{
	int error;

	error = cr_canseeotheruids(u1, u2);
	if (error != 0)
		return (error);
	error = cr_canseeothergids(u1, u2);
	if (error != 0)
		return (error);
	error = cr_canseejailproc(u1, u2);
	if (error != 0)
		return (error);
	return (0);
}

/*-
 * Determine if u1 "can see" the subject specified by u2.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: none
 * References: *u1 and *u2 must not change during the call
 *             u1 may equal u2, in which case only one reference is required
 */
int
cr_cansee(struct ucred *u1, struct ucred *u2)
{
	int error;

	if ((error = prison_check(u1, u2)))
		return (error);
#ifdef MAC
	if ((error = mac_cred_check_visible(u1, u2)))
		return (error);
#endif
	if ((error = cr_bsd_visible(u1, u2)))
		return (error);
	return (0);
}

/*-
 * Determine if td "can see" the subject specified by p.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: Sufficient locks to protect p->p_ucred must be held.  td really
 *        should be curthread.
 * References: td and p must be valid for the lifetime of the call
 */
int
p_cansee(struct thread *td, struct proc *p)
{
	/* Wrap cr_cansee() for all functionality. */
	KASSERT(td == curthread, ("%s: td not curthread", __func__));
	PROC_LOCK_ASSERT(p, MA_OWNED);

	if (td->td_proc == p)
		return (0);
	return (cr_cansee(td->td_ucred, p->p_ucred));
}

/*
 * 'conservative_signals' prevents the delivery of a broad class of
 * signals by unprivileged processes to processes that have changed their
 * credentials since the last invocation of execve().  This can prevent
 * the leakage of cached information or retained privileges as a result
 * of a common class of signal-related vulnerabilities.  However, this
 * may interfere with some applications that expect to be able to
 * deliver these signals to peer processes after having given up
 * privilege.
 */
static int	conservative_signals = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, conservative_signals, CTLFLAG_RW,
    &conservative_signals, 0, "Unprivileged processes prevented from "
    "sending certain signals to processes whose credentials have changed");
/*-
 * Determine whether cred may deliver the specified signal to proc.
 * Returns: 0 for permitted, an errno value otherwise.
 * Locks: A lock must be held for proc.
 * References: cred and proc must be valid for the lifetime of the call.
 */
int
cr_cansignal(struct ucred *cred, struct proc *proc, int signum)
{
	int error;

	PROC_LOCK_ASSERT(proc, MA_OWNED);
	/*
	 * Jail semantics limit the scope of signalling to proc in the
	 * same jail as cred, if cred is in jail.
	 */
	error = prison_check(cred, proc->p_ucred);
	if (error)
		return (error);
#ifdef MAC
	if ((error = mac_proc_check_signal(cred, proc, signum)))
		return (error);
#endif
	if ((error = cr_bsd_visible(cred, proc->p_ucred)))
		return (error);

	/*
	 * UNIX signal semantics depend on the status of the P_SUGID
	 * bit on the target process.  If the bit is set, then additional
	 * restrictions are placed on the set of available signals.
	 */
	if (conservative_signals && (proc->p_flag & P_SUGID)) {
		switch (signum) {
		case 0:
		case SIGKILL:
		case SIGINT:
		case SIGTERM:
		case SIGALRM:
		case SIGSTOP:
		case SIGTTIN:
		case SIGTTOU:
		case SIGTSTP:
		case SIGHUP:
		case SIGUSR1:
		case SIGUSR2:
			/*
			 * Generally, permit job and terminal control
			 * signals.
			 */
			break;
		default:
			/* Not permitted without privilege. */
			error = priv_check_cred(cred, PRIV_SIGNAL_SUGID);
			if (error)
				return (error);
		}
	}

	/*
	 * Generally, the target credential's ruid or svuid must match the
	 * subject credential's ruid or euid.
	 */
	if (cred->cr_ruid != proc->p_ucred->cr_ruid &&
	    cred->cr_ruid != proc->p_ucred->cr_svuid &&
	    cred->cr_uid != proc->p_ucred->cr_ruid &&
	    cred->cr_uid != proc->p_ucred->cr_svuid) {
		error = priv_check_cred(cred, PRIV_SIGNAL_DIFFCRED);
		if (error)
			return (error);
	}

	/*
	 * At this point, the target may be in a different jail than the
	 * subject -- the subject must be in a parent jail to the target,
	 * whether it is prison0 or a subordinate of prison0 that has
	 * children.  Additional privileges are required to allow this, as
	 * whether the creds are truly equivalent or not must be determined on
	 * a case-by-case basis.
	 */
	error = cr_can_tamper_with_subjail(cred, proc->p_ucred,
	    PRIV_SIGNAL_DIFFJAIL);
	if (error)
		return (error);

	return (0);
}

/*-
 * Determine whether td may deliver the specified signal to p.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: Sufficient locks to protect various components of td and p
 *        must be held.  td must be curthread, and a lock must be
 *        held for p.
 * References: td and p must be valid for the lifetime of the call
 */
int
p_cansignal(struct thread *td, struct proc *p, int signum)
{

	KASSERT(td == curthread, ("%s: td not curthread", __func__));
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (td->td_proc == p)
		return (0);

	/*
	 * UNIX signalling semantics require that processes in the same
	 * session always be able to deliver SIGCONT to one another,
	 * overriding the remaining protections.
	 */
	/* XXX: This will require an additional lock of some sort. */
	if (signum == SIGCONT && td->td_proc->p_session == p->p_session)
		return (0);
	/*
	 * Some compat layers use SIGTHR and higher signals for
	 * communication between different kernel threads of the same
	 * process, so that they expect that it's always possible to
	 * deliver them, even for suid applications where cr_cansignal() can
	 * deny such ability for security consideration.  It should be
	 * pretty safe to do since the only way to create two processes
	 * with the same p_leader is via rfork(2).
	 */
	if (td->td_proc->p_leader != NULL && signum >= SIGTHR &&
	    signum < SIGTHR + 4 && td->td_proc->p_leader == p->p_leader)
		return (0);

	return (cr_cansignal(td->td_ucred, p, signum));
}

/*-
 * Determine whether td may reschedule p.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: Sufficient locks to protect various components of td and p
 *        must be held.  td must be curthread, and a lock must
 *        be held for p.
 * References: td and p must be valid for the lifetime of the call
 */
int
p_cansched(struct thread *td, struct proc *p)
{
	int error;

	KASSERT(td == curthread, ("%s: td not curthread", __func__));
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (td->td_proc == p)
		return (0);
	if ((error = prison_check(td->td_ucred, p->p_ucred)))
		return (error);
#ifdef MAC
	if ((error = mac_proc_check_sched(td->td_ucred, p)))
		return (error);
#endif
	if ((error = cr_bsd_visible(td->td_ucred, p->p_ucred)))
		return (error);

	if (td->td_ucred->cr_ruid != p->p_ucred->cr_ruid &&
	    td->td_ucred->cr_uid != p->p_ucred->cr_ruid) {
		error = priv_check(td, PRIV_SCHED_DIFFCRED);
		if (error)
			return (error);
	}

	error = cr_can_tamper_with_subjail(td->td_ucred, p->p_ucred,
	    PRIV_SCHED_DIFFJAIL);
	if (error)
		return (error);

	return (0);
}

/*
 * Handle getting or setting the prison's unprivileged_proc_debug
 * value.
 */
static int
sysctl_unprivileged_proc_debug(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = prison_allow(req->td->td_ucred, PR_ALLOW_UNPRIV_DEBUG);
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 0 && val != 1)
		return (EINVAL);
	prison_set_allow(req->td->td_ucred, PR_ALLOW_UNPRIV_DEBUG, val);
	return (0);
}

/*
 * The 'unprivileged_proc_debug' flag may be used to disable a variety of
 * unprivileged inter-process debugging services, including some procfs
 * functionality, ptrace(), and ktrace().  In the past, inter-process
 * debugging has been involved in a variety of security problems, and sites
 * not requiring the service might choose to disable it when hardening
 * systems.
 */
SYSCTL_PROC(_security_bsd, OID_AUTO, unprivileged_proc_debug,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_SECURE |
    CTLFLAG_MPSAFE, 0, 0, sysctl_unprivileged_proc_debug, "I",
    "Unprivileged processes may use process debugging facilities");

/*
 * Return true if the object owner/group ids are subset of the active
 * credentials.
 */
bool
cr_xids_subset(struct ucred *active_cred, struct ucred *obj_cred)
{
	int i;
	bool grpsubset, uidsubset;

	/*
	 * Is p's group set a subset of td's effective group set?  This
	 * includes p's egid, group access list, rgid, and svgid.
	 */
	grpsubset = true;
	for (i = 0; i < obj_cred->cr_ngroups; i++) {
		if (!groupmember(obj_cred->cr_groups[i], active_cred)) {
			grpsubset = false;
			break;
		}
	}
	grpsubset = grpsubset &&
	    groupmember(obj_cred->cr_gid, active_cred) &&
	    groupmember(obj_cred->cr_rgid, active_cred) &&
	    groupmember(obj_cred->cr_svgid, active_cred);

	/*
	 * Are the uids present in obj_cred's credential equal to
	 * active_cred's effective uid?  This includes obj_cred's
	 * euid, svuid, and ruid.
	 */
	uidsubset = (active_cred->cr_uid == obj_cred->cr_uid &&
	    active_cred->cr_uid == obj_cred->cr_svuid &&
	    active_cred->cr_uid == obj_cred->cr_ruid);

	return (uidsubset && grpsubset);
}

/*-
 * Determine whether td may debug p.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: Sufficient locks to protect various components of td and p
 *        must be held.  td must be curthread, and a lock must
 *        be held for p.
 * References: td and p must be valid for the lifetime of the call
 */
int
p_candebug(struct thread *td, struct proc *p)
{
	int error;

	KASSERT(td == curthread, ("%s: td not curthread", __func__));
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (td->td_proc == p)
		return (0);
	if ((error = priv_check(td, PRIV_DEBUG_UNPRIV)))
		return (error);
	if ((error = prison_check(td->td_ucred, p->p_ucred)))
		return (error);
#ifdef MAC
	if ((error = mac_proc_check_debug(td->td_ucred, p)))
		return (error);
#endif
	if ((error = cr_bsd_visible(td->td_ucred, p->p_ucred)))
		return (error);

	/*
	 * If p's gids aren't a subset, or the uids aren't a subset,
	 * or the credential has changed, require appropriate privilege
	 * for td to debug p.
	 */
	if (!cr_xids_subset(td->td_ucred, p->p_ucred)) {
		error = priv_check(td, PRIV_DEBUG_DIFFCRED);
		if (error)
			return (error);
	}

	/*
	 * Has the credential of the process changed since the last exec()?
	 */
	if ((p->p_flag & P_SUGID) != 0) {
		error = priv_check(td, PRIV_DEBUG_SUGID);
		if (error)
			return (error);
	}

	error = cr_can_tamper_with_subjail(td->td_ucred, p->p_ucred,
	    PRIV_DEBUG_DIFFJAIL);
	if (error)
		return (error);

	/* Can't trace init when securelevel > 0. */
	if (p == initproc) {
		error = securelevel_gt(td->td_ucred, 0);
		if (error)
			return (error);
	}

	/*
	 * Can't trace a process that's currently exec'ing.
	 *
	 * XXX: Note, this is not a security policy decision, it's a
	 * basic correctness/functionality decision.  Therefore, this check
	 * should be moved to the caller's of p_candebug().
	 */
	if ((p->p_flag & P_INEXEC) != 0)
		return (EBUSY);

	/* Denied explicitly */
	if ((p->p_flag2 & P2_NOTRACE) != 0) {
		error = priv_check(td, PRIV_DEBUG_DENIED);
		if (error != 0)
			return (error);
	}

	return (0);
}

/*-
 * Determine whether the subject represented by cred can "see" a socket.
 * Returns: 0 for permitted, ENOENT otherwise.
 */
int
cr_canseesocket(struct ucred *cred, struct socket *so)
{
	int error;

	error = prison_check(cred, so->so_cred);
	if (error)
		return (ENOENT);
#ifdef MAC
	error = mac_socket_check_visible(cred, so);
	if (error)
		return (error);
#endif
	if (cr_bsd_visible(cred, so->so_cred))
		return (ENOENT);

	return (0);
}

/*-
 * Determine whether td can wait for the exit of p.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: Sufficient locks to protect various components of td and p
 *        must be held.  td must be curthread, and a lock must
 *        be held for p.
 * References: td and p must be valid for the lifetime of the call

 */
int
p_canwait(struct thread *td, struct proc *p)
{
	int error;

	KASSERT(td == curthread, ("%s: td not curthread", __func__));
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if ((error = prison_check(td->td_ucred, p->p_ucred)))
		return (error);
#ifdef MAC
	if ((error = mac_proc_check_wait(td->td_ucred, p)))
		return (error);
#endif
#if 0
	/* XXXMAC: This could have odd effects on some shells. */
	if ((error = cr_bsd_visible(td->td_ucred, p->p_ucred)))
		return (error);
#endif

	return (0);
}

/*
 * Credential management.
 *
 * struct ucred objects are rarely allocated but gain and lose references all
 * the time (e.g., on struct file alloc/dealloc) turning refcount updates into
 * a significant source of cache-line ping ponging. Common cases are worked
 * around by modifying thread-local counter instead if the cred to operate on
 * matches td_realucred.
 *
 * The counter is split into 2 parts:
 * - cr_users -- total count of all struct proc and struct thread objects
 *   which have given cred in p_ucred and td_ucred respectively
 * - cr_ref -- the actual ref count, only valid if cr_users == 0
 *
 * If users == 0 then cr_ref behaves similarly to refcount(9), in particular if
 * the count reaches 0 the object is freeable.
 * If users > 0 and curthread->td_realucred == cred, then updates are performed
 * against td_ucredref.
 * In other cases updates are performed against cr_ref.
 *
 * Changing td_realucred into something else decrements cr_users and transfers
 * accumulated updates.
 */
struct ucred *
crcowget(struct ucred *cr)
{

	mtx_lock(&cr->cr_mtx);
	KASSERT(cr->cr_users > 0, ("%s: users %d not > 0 on cred %p",
	    __func__, cr->cr_users, cr));
	cr->cr_users++;
	cr->cr_ref++;
	mtx_unlock(&cr->cr_mtx);
	return (cr);
}

static struct ucred *
crunuse(struct thread *td)
{
	struct ucred *cr, *crold;

	MPASS(td->td_realucred == td->td_ucred);
	cr = td->td_realucred;
	mtx_lock(&cr->cr_mtx);
	cr->cr_ref += td->td_ucredref;
	td->td_ucredref = 0;
	KASSERT(cr->cr_users > 0, ("%s: users %d not > 0 on cred %p",
	    __func__, cr->cr_users, cr));
	cr->cr_users--;
	if (cr->cr_users == 0) {
		KASSERT(cr->cr_ref > 0, ("%s: ref %ld not > 0 on cred %p",
		    __func__, cr->cr_ref, cr));
		crold = cr;
	} else {
		cr->cr_ref--;
		crold = NULL;
	}
	mtx_unlock(&cr->cr_mtx);
	td->td_realucred = NULL;
	return (crold);
}

static void
crunusebatch(struct ucred *cr, u_int users, long ref)
{

	KASSERT(users > 0, ("%s: passed users %d not > 0 ; cred %p",
	    __func__, users, cr));
	mtx_lock(&cr->cr_mtx);
	KASSERT(cr->cr_users >= users, ("%s: users %d not > %d on cred %p",
	    __func__, cr->cr_users, users, cr));
	cr->cr_users -= users;
	cr->cr_ref += ref;
	cr->cr_ref -= users;
	if (cr->cr_users > 0) {
		mtx_unlock(&cr->cr_mtx);
		return;
	}
	KASSERT(cr->cr_ref >= 0, ("%s: ref %ld not >= 0 on cred %p",
	    __func__, cr->cr_ref, cr));
	if (cr->cr_ref > 0) {
		mtx_unlock(&cr->cr_mtx);
		return;
	}
	crfree_final(cr);
}

void
crcowfree(struct thread *td)
{
	struct ucred *cr;

	cr = crunuse(td);
	if (cr != NULL)
		crfree(cr);
}

struct ucred *
crcowsync(void)
{
	struct thread *td;
	struct proc *p;
	struct ucred *crnew, *crold;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	MPASS(td->td_realucred == td->td_ucred);
	if (td->td_realucred == p->p_ucred)
		return (NULL);

	crnew = crcowget(p->p_ucred);
	crold = crunuse(td);
	td->td_realucred = crnew;
	td->td_ucred = td->td_realucred;
	return (crold);
}

/*
 * Batching.
 */
void
credbatch_add(struct credbatch *crb, struct thread *td)
{
	struct ucred *cr;

	MPASS(td->td_realucred != NULL);
	MPASS(td->td_realucred == td->td_ucred);
	MPASS(TD_GET_STATE(td) == TDS_INACTIVE);
	cr = td->td_realucred;
	KASSERT(cr->cr_users > 0, ("%s: users %d not > 0 on cred %p",
	    __func__, cr->cr_users, cr));
	if (crb->cred != cr) {
		if (crb->users > 0) {
			MPASS(crb->cred != NULL);
			crunusebatch(crb->cred, crb->users, crb->ref);
			crb->users = 0;
			crb->ref = 0;
		}
	}
	crb->cred = cr;
	crb->users++;
	crb->ref += td->td_ucredref;
	td->td_ucredref = 0;
	td->td_realucred = NULL;
}

void
credbatch_final(struct credbatch *crb)
{

	MPASS(crb->cred != NULL);
	MPASS(crb->users > 0);
	crunusebatch(crb->cred, crb->users, crb->ref);
}

/*
 * Allocate a zeroed cred structure.
 */
struct ucred *
crget(void)
{
	struct ucred *cr;

	cr = malloc(sizeof(*cr), M_CRED, M_WAITOK | M_ZERO);
	mtx_init(&cr->cr_mtx, "cred", NULL, MTX_DEF);
	cr->cr_ref = 1;
#ifdef AUDIT
	audit_cred_init(cr);
#endif
#ifdef MAC
	mac_cred_init(cr);
#endif
	cr->cr_groups = cr->cr_smallgroups;
	cr->cr_agroups = nitems(cr->cr_smallgroups);
	return (cr);
}

/*
 * Claim another reference to a ucred structure.
 */
struct ucred *
crhold(struct ucred *cr)
{
	struct thread *td;

	td = curthread;
	if (__predict_true(td->td_realucred == cr)) {
		KASSERT(cr->cr_users > 0, ("%s: users %d not > 0 on cred %p",
		    __func__, cr->cr_users, cr));
		td->td_ucredref++;
		return (cr);
	}
	mtx_lock(&cr->cr_mtx);
	cr->cr_ref++;
	mtx_unlock(&cr->cr_mtx);
	return (cr);
}

/*
 * Free a cred structure.  Throws away space when ref count gets to 0.
 */
void
crfree(struct ucred *cr)
{
	struct thread *td;

	td = curthread;
	if (__predict_true(td->td_realucred == cr)) {
		KASSERT(cr->cr_users > 0, ("%s: users %d not > 0 on cred %p",
		    __func__, cr->cr_users, cr));
		td->td_ucredref--;
		return;
	}
	mtx_lock(&cr->cr_mtx);
	KASSERT(cr->cr_users >= 0, ("%s: users %d not >= 0 on cred %p",
	    __func__, cr->cr_users, cr));
	cr->cr_ref--;
	if (cr->cr_users > 0) {
		mtx_unlock(&cr->cr_mtx);
		return;
	}
	KASSERT(cr->cr_ref >= 0, ("%s: ref %ld not >= 0 on cred %p",
	    __func__, cr->cr_ref, cr));
	if (cr->cr_ref > 0) {
		mtx_unlock(&cr->cr_mtx);
		return;
	}
	crfree_final(cr);
}

static void
crfree_final(struct ucred *cr)
{

	KASSERT(cr->cr_users == 0, ("%s: users %d not == 0 on cred %p",
	    __func__, cr->cr_users, cr));
	KASSERT(cr->cr_ref == 0, ("%s: ref %ld not == 0 on cred %p",
	    __func__, cr->cr_ref, cr));

	/*
	 * Some callers of crget(), such as nfs_statfs(), allocate a temporary
	 * credential, but don't allocate a uidinfo structure.
	 */
	if (cr->cr_uidinfo != NULL)
		uifree(cr->cr_uidinfo);
	if (cr->cr_ruidinfo != NULL)
		uifree(cr->cr_ruidinfo);
	if (cr->cr_prison != NULL)
		prison_free(cr->cr_prison);
	if (cr->cr_loginclass != NULL)
		loginclass_free(cr->cr_loginclass);
#ifdef AUDIT
	audit_cred_destroy(cr);
#endif
#ifdef MAC
	mac_cred_destroy(cr);
#endif
	mtx_destroy(&cr->cr_mtx);
	if (cr->cr_groups != cr->cr_smallgroups)
		free(cr->cr_groups, M_CRED);
	free(cr, M_CRED);
}

/*
 * Copy a ucred's contents from a template.  Does not block.
 */
void
crcopy(struct ucred *dest, struct ucred *src)
{

	bcopy(&src->cr_startcopy, &dest->cr_startcopy,
	    (unsigned)((caddr_t)&src->cr_endcopy -
		(caddr_t)&src->cr_startcopy));
	dest->cr_flags = src->cr_flags;
	crsetgroups(dest, src->cr_ngroups, src->cr_groups);
	uihold(dest->cr_uidinfo);
	uihold(dest->cr_ruidinfo);
	prison_hold(dest->cr_prison);
	loginclass_hold(dest->cr_loginclass);
#ifdef AUDIT
	audit_cred_copy(src, dest);
#endif
#ifdef MAC
	mac_cred_copy(src, dest);
#endif
}

/*
 * Dup cred struct to a new held one.
 */
struct ucred *
crdup(struct ucred *cr)
{
	struct ucred *newcr;

	newcr = crget();
	crcopy(newcr, cr);
	return (newcr);
}

/*
 * Fill in a struct xucred based on a struct ucred.
 */
void
cru2x(struct ucred *cr, struct xucred *xcr)
{
	int ngroups;

	bzero(xcr, sizeof(*xcr));
	xcr->cr_version = XUCRED_VERSION;
	xcr->cr_uid = cr->cr_uid;
	xcr->cr_gid = cr->cr_gid;

	/*
	 * We use a union to alias cr_gid to cr_groups[0] in the xucred, so
	 * this is kind of ugly; cr_ngroups still includes the egid for our
	 * purposes to avoid bumping the xucred version.
	 */
	ngroups = MIN(cr->cr_ngroups + 1, nitems(xcr->cr_groups));
	xcr->cr_ngroups = ngroups;
	bcopy(cr->cr_groups, xcr->cr_sgroups,
	    (ngroups - 1) * sizeof(*cr->cr_groups));
}

void
cru2xt(struct thread *td, struct xucred *xcr)
{

	cru2x(td->td_ucred, xcr);
	xcr->cr_pid = td->td_proc->p_pid;
}

/*
 * Change process credentials.
 *
 * Callers are responsible for providing the reference for passed credentials
 * and for freeing old ones.  Calls chgproccnt() to correctly account the
 * current process to the proper real UID, if the latter has changed.  Returns
 * whether the operation was successful.  Failure can happen only on
 * 'enforce_proc_lim' being true and if no new process can be accounted to the
 * new real UID because of the current limit (see the inner comment for more
 * details) and the caller does not have privilege (PRIV_PROC_LIMIT) to override
 * that.
 */
static bool
_proc_set_cred(struct proc *p, struct ucred *newcred, bool enforce_proc_lim)
{
	struct ucred *const oldcred = p->p_ucred;

	MPASS(oldcred != NULL);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT(newcred->cr_users == 0, ("%s: users %d not 0 on cred %p",
	    __func__, newcred->cr_users, newcred));
	KASSERT(newcred->cr_ref == 1, ("%s: ref %ld not 1 on cred %p",
	    __func__, newcred->cr_ref, newcred));

	if (newcred->cr_ruidinfo != oldcred->cr_ruidinfo) {
		/*
		 * XXXOC: This check is flawed but nonetheless the best we can
		 * currently do as we don't really track limits per UID contrary
		 * to what we pretend in setrlimit(2).  Until this is reworked,
		 * we just check here that the number of processes for our new
		 * real UID doesn't exceed this process' process number limit
		 * (which is meant to be associated with the current real UID).
		 */
		const int proccnt_changed = chgproccnt(newcred->cr_ruidinfo, 1,
		    enforce_proc_lim ? lim_cur_proc(p, RLIMIT_NPROC) : 0);

		if (!proccnt_changed) {
			if (priv_check_cred(oldcred, PRIV_PROC_LIMIT) != 0)
				return (false);
			(void)chgproccnt(newcred->cr_ruidinfo, 1, 0);
		}
	}

	mtx_lock(&oldcred->cr_mtx);
	KASSERT(oldcred->cr_users > 0, ("%s: users %d not > 0 on cred %p",
	    __func__, oldcred->cr_users, oldcred));
	oldcred->cr_users--;
	mtx_unlock(&oldcred->cr_mtx);
	p->p_ucred = newcred;
	newcred->cr_users = 1;
	PROC_UPDATE_COW(p);
	if (newcred->cr_ruidinfo != oldcred->cr_ruidinfo)
		(void)chgproccnt(oldcred->cr_ruidinfo, -1, 0);
	return (true);
}

void
proc_set_cred(struct proc *p, struct ucred *newcred)
{
	bool success __diagused = _proc_set_cred(p, newcred, false);

	MPASS(success);
}

bool
proc_set_cred_enforce_proc_lim(struct proc *p, struct ucred *newcred)
{
	return (_proc_set_cred(p, newcred, true));
}

void
proc_unset_cred(struct proc *p, bool decrement_proc_count)
{
	struct ucred *cr;

	MPASS(p->p_state == PRS_ZOMBIE || p->p_state == PRS_NEW);
	cr = p->p_ucred;
	p->p_ucred = NULL;
	KASSERT(cr->cr_users > 0, ("%s: users %d not > 0 on cred %p",
	    __func__, cr->cr_users, cr));
	mtx_lock(&cr->cr_mtx);
	cr->cr_users--;
	if (cr->cr_users == 0)
		KASSERT(cr->cr_ref > 0, ("%s: ref %ld not > 0 on cred %p",
		    __func__, cr->cr_ref, cr));
	mtx_unlock(&cr->cr_mtx);
	if (decrement_proc_count)
		(void)chgproccnt(cr->cr_ruidinfo, -1, 0);
	crfree(cr);
}

struct ucred *
crcopysafe(struct proc *p, struct ucred *cr)
{
	struct ucred *oldcred;
	int groups;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	oldcred = p->p_ucred;
	while (cr->cr_agroups < oldcred->cr_ngroups) {
		groups = oldcred->cr_ngroups;
		PROC_UNLOCK(p);
		crextend(cr, groups);
		PROC_LOCK(p);
		oldcred = p->p_ucred;
	}
	crcopy(cr, oldcred);

	return (oldcred);
}

/*
 * Extend the passed-in credentials to hold n groups.
 *
 * Must not be called after groups have been set.
 */
void
crextend(struct ucred *cr, int n)
{
	size_t nbytes;

	MPASS2(cr->cr_ref == 1, "'cr_ref' must be 1 (referenced, unshared)");
	MPASS2((cr->cr_flags & CRED_FLAG_GROUPSET) == 0,
	    "groups on 'cr' already set!");
	groups_check_positive_len(n);
	groups_check_max_len(n);

	if (n <= cr->cr_agroups)
		return;

	nbytes = n * sizeof(gid_t);
	if (nbytes < n)
		panic("Too many groups (memory size overflow)! "
		    "Computation of 'kern.ngroups' should have prevented this, "
		    "please fix it. In the meantime, reduce 'kern.ngroups'.");

	/*
	 * We allocate a power of 2 larger than 'nbytes', except when that
	 * exceeds PAGE_SIZE, in which case we allocate the right multiple of
	 * pages.  We assume PAGE_SIZE is a power of 2 (the call to roundup2()
	 * below) but do not need to for sizeof(gid_t).
	 */
	if (nbytes < PAGE_SIZE) {
		if (!powerof2(nbytes))
			/* fls*() return a bit index starting at 1. */
			nbytes = 1 << flsl(nbytes);
	} else
		nbytes = roundup2(nbytes, PAGE_SIZE);

	/* Free the old array. */
	if (cr->cr_groups != cr->cr_smallgroups)
		free(cr->cr_groups, M_CRED);

	cr->cr_groups = malloc(nbytes, M_CRED, M_WAITOK | M_ZERO);
	cr->cr_agroups = nbytes / sizeof(gid_t);
}

/*
 * Normalizes a set of groups to be applied to a 'struct ucred'.
 *
 * Normalization ensures that the supplementary groups are sorted in ascending
 * order and do not contain duplicates.  This allows group_is_supplementary() to
 * do a binary search.
 */
static void
groups_normalize(int *ngrp, gid_t *groups)
{
	gid_t prev_g;
	int ins_idx;

	groups_check_positive_len(*ngrp);
	groups_check_max_len(*ngrp);

	if (*ngrp <= 1)
		return;

	qsort(groups, *ngrp, sizeof(*groups), gidp_cmp);

	/* Remove duplicates. */
	prev_g = groups[0];
	ins_idx = 1;
	for (int i = ins_idx; i < *ngrp; ++i) {
		const gid_t g = groups[i];

		if (g != prev_g) {
			if (i != ins_idx)
				groups[ins_idx] = g;
			++ins_idx;
			prev_g = g;
		}
	}
	*ngrp = ins_idx;

	groups_check_normalized(*ngrp, groups);
}

/*
 * Internal function copying groups into a credential.
 *
 * 'ngrp' must be strictly positive.  Either the passed 'groups' array must have
 * been normalized in advance (see groups_normalize()), else it must be so
 * before the structure is to be used again.
 *
 * This function is suitable to be used under any lock (it doesn't take any lock
 * itself nor sleep, and in particular doesn't allocate memory).  crextend()
 * must have been called beforehand to ensure sufficient space is available.
 * See also crsetgroups(), which handles that.
 */
static void
crsetgroups_internal(struct ucred *cr, int ngrp, const gid_t *groups)
{

	MPASS2(cr->cr_ref == 1, "'cr_ref' must be 1 (referenced, unshared)");
	MPASS2(cr->cr_agroups >= ngrp, "'cr_agroups' too small");
	groups_check_positive_len(ngrp);

	bcopy(groups, cr->cr_groups, ngrp * sizeof(gid_t));
	cr->cr_ngroups = ngrp;
	cr->cr_flags |= CRED_FLAG_GROUPSET;
}

/*
 * Copy groups in to a credential after expanding it if required.
 *
 * May sleep in order to allocate memory (except if, e.g., crextend() was called
 * before with 'ngrp' or greater).  Truncates the list to 'ngroups_max' if
 * it is too large.  Array 'groups' doesn't need to be sorted.  'ngrp' must be
 * positive.
 */
void
crsetgroups(struct ucred *cr, int ngrp, const gid_t *groups)
{

	if (ngrp > ngroups_max)
		ngrp = ngroups_max;
	cr->cr_ngroups = 0;
	if (ngrp == 0) {
		cr->cr_flags |= CRED_FLAG_GROUPSET;
		return;
	}

	/*
	 * crextend() asserts that groups are not set, as it may allocate a new
	 * backing storage without copying the content of the old one.  Since we
	 * are going to install a completely new set anyway, signal that we
	 * consider the old ones thrown away.
	 */
	cr->cr_flags &= ~CRED_FLAG_GROUPSET;

	crextend(cr, ngrp);
	crsetgroups_internal(cr, ngrp, groups);
	groups_normalize(&cr->cr_ngroups, cr->cr_groups);
}

/*
 * Same as crsetgroups() but sets the effective GID as well.
 *
 * This function ensures that an effective GID is always present in credentials.
 * An empty array will only set the effective GID to 'default_egid', while
 * a non-empty array will peel off groups[0] to set as the effective GID and use
 * the remainder, if any, as supplementary groups.
 */
void
crsetgroups_and_egid(struct ucred *cr, int ngrp, const gid_t *groups,
    const gid_t default_egid)
{
	if (ngrp == 0) {
		cr->cr_gid = default_egid;
		cr->cr_ngroups = 0;
		cr->cr_flags |= CRED_FLAG_GROUPSET;
		return;
	}

	crsetgroups(cr, ngrp - 1, groups + 1);
	cr->cr_gid = groups[0];
}

/*
 * Get login name, if available.
 */
#ifndef _SYS_SYSPROTO_H_
struct getlogin_args {
	char	*namebuf;
	u_int	namelen;
};
#endif
/* ARGSUSED */
int
sys_getlogin(struct thread *td, struct getlogin_args *uap)
{
	char login[MAXLOGNAME];
	struct proc *p = td->td_proc;
	size_t len;

	if (uap->namelen > MAXLOGNAME)
		uap->namelen = MAXLOGNAME;
	PROC_LOCK(p);
	SESS_LOCK(p->p_session);
	len = strlcpy(login, p->p_session->s_login, uap->namelen) + 1;
	SESS_UNLOCK(p->p_session);
	PROC_UNLOCK(p);
	if (len > uap->namelen)
		return (ERANGE);
	return (copyout(login, uap->namebuf, len));
}

/*
 * Set login name.
 */
#ifndef _SYS_SYSPROTO_H_
struct setlogin_args {
	char	*namebuf;
};
#endif
/* ARGSUSED */
int
sys_setlogin(struct thread *td, struct setlogin_args *uap)
{
	struct proc *p = td->td_proc;
	int error;
	char logintmp[MAXLOGNAME];

	CTASSERT(sizeof(p->p_session->s_login) >= sizeof(logintmp));

	error = priv_check(td, PRIV_PROC_SETLOGIN);
	if (error)
		return (error);
	error = copyinstr(uap->namebuf, logintmp, sizeof(logintmp), NULL);
	if (error != 0) {
		if (error == ENAMETOOLONG)
			error = EINVAL;
		return (error);
	}
	AUDIT_ARG_LOGIN(logintmp);
	PROC_LOCK(p);
	SESS_LOCK(p->p_session);
	strcpy(p->p_session->s_login, logintmp);
	SESS_UNLOCK(p->p_session);
	PROC_UNLOCK(p);
	return (0);
}

void
setsugid(struct proc *p)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
	p->p_flag |= P_SUGID;
}

/*-
 * Change a process's effective uid.
 * Side effects: newcred->cr_uid and newcred->cr_uidinfo will be modified.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_euid(struct ucred *newcred, struct uidinfo *euip)
{

	newcred->cr_uid = euip->ui_uid;
	uihold(euip);
	uifree(newcred->cr_uidinfo);
	newcred->cr_uidinfo = euip;
}

/*-
 * Change a process's effective gid.
 * Side effects: newcred->cr_gid will be modified.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_egid(struct ucred *newcred, gid_t egid)
{

	newcred->cr_gid = egid;
}

/*-
 * Change a process's real uid.
 * Side effects: newcred->cr_ruid will be updated, newcred->cr_ruidinfo
 *               will be updated.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_ruid(struct ucred *newcred, struct uidinfo *ruip)
{

	newcred->cr_ruid = ruip->ui_uid;
	uihold(ruip);
	uifree(newcred->cr_ruidinfo);
	newcred->cr_ruidinfo = ruip;
}

/*-
 * Change a process's real gid.
 * Side effects: newcred->cr_rgid will be updated.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_rgid(struct ucred *newcred, gid_t rgid)
{

	newcred->cr_rgid = rgid;
}

/*-
 * Change a process's saved uid.
 * Side effects: newcred->cr_svuid will be updated.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_svuid(struct ucred *newcred, uid_t svuid)
{

	newcred->cr_svuid = svuid;
}

/*-
 * Change a process's saved gid.
 * Side effects: newcred->cr_svgid will be updated.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_svgid(struct ucred *newcred, gid_t svgid)
{

	newcred->cr_svgid = svgid;
}

bool allow_ptrace = true;
SYSCTL_BOOL(_security_bsd, OID_AUTO, allow_ptrace, CTLFLAG_RWTUN,
    &allow_ptrace, 0,
    "Deny ptrace(2) use by returning ENOSYS");
