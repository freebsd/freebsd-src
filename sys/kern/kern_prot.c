/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 * Copyright (c) 2000-2001 Robert N. M. Watson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)kern_prot.c	8.6 (Berkeley) 1/21/94
 * $FreeBSD$
 */

/*
 * System calls related to processes and protection
 */

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sx.h>
#include <sys/sysproto.h>
#include <sys/jail.h>
#include <sys/malloc.h>
#include <sys/pioctl.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>

static MALLOC_DEFINE(M_CRED, "cred", "credentials");

SYSCTL_DECL(_security);
SYSCTL_NODE(_security, OID_AUTO, bsd, CTLFLAG_RW, 0,
    "BSD security policy");

#ifndef _SYS_SYSPROTO_H_
struct getpid_args {
	int	dummy;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getpid(td, uap)
	struct thread *td;
	struct getpid_args *uap;
{
	struct proc *p = td->td_proc;
	int s;

	s = mtx_lock_giant(kern_giant_proc);
	td->td_retval[0] = p->p_pid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	PROC_LOCK(p);
	td->td_retval[1] = p->p_pptr->p_pid;
	PROC_UNLOCK(p);
#endif
	mtx_unlock_giant(s);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getppid_args {
        int     dummy;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getppid(td, uap)
	struct thread *td;
	struct getppid_args *uap;
{
	struct proc *p = td->td_proc;
	int s;

	s = mtx_lock_giant(kern_giant_proc);
	PROC_LOCK(p);
	td->td_retval[0] = p->p_pptr->p_pid;
	PROC_UNLOCK(p);
	mtx_unlock_giant(s);
	return (0);
}

/*
 * Get process group ID; note that POSIX getpgrp takes no parameter.
 */
#ifndef _SYS_SYSPROTO_H_
struct getpgrp_args {
        int     dummy;
};
#endif
/*
 * MPSAFE
 */
int
getpgrp(td, uap)
	struct thread *td;
	struct getpgrp_args *uap;
{
	struct proc *p = td->td_proc;

	mtx_lock(&Giant);
	td->td_retval[0] = p->p_pgrp->pg_id;
	mtx_unlock(&Giant);
	return (0);
}

/* Get an arbitary pid's process group id */
#ifndef _SYS_SYSPROTO_H_
struct getpgid_args {
	pid_t	pid;
};
#endif
/*
 * MPSAFE
 */
int
getpgid(td, uap)
	struct thread *td;
	struct getpgid_args *uap;
{
	struct proc *p = td->td_proc;
	struct proc *pt;
	int error, s;

	s = mtx_lock_giant(kern_giant_proc);
	error = 0;
	if (uap->pid == 0)
		td->td_retval[0] = p->p_pgrp->pg_id;
	else if ((pt = pfind(uap->pid)) == NULL)
		error = ESRCH;
	else {
		error = p_cansee(p, pt);
		if (error == 0)
			td->td_retval[0] = pt->p_pgrp->pg_id;
		PROC_UNLOCK(pt);
	}
	mtx_unlock_giant(s);
	return (error);
}

/*
 * Get an arbitary pid's session id.
 */
#ifndef _SYS_SYSPROTO_H_
struct getsid_args {
	pid_t	pid;
};
#endif
/*
 * MPSAFE
 */
int
getsid(td, uap)
	struct thread *td;
	struct getsid_args *uap;
{
	struct proc *p = td->td_proc;
	struct proc *pt;
	int error;

	mtx_lock(&Giant);
	error = 0;
	if (uap->pid == 0)
		td->td_retval[0] = p->p_session->s_sid;
	else if ((pt = pfind(uap->pid)) == NULL)
		error = ESRCH;
	else {
		error = p_cansee(p, pt);
		if (error == 0)
			td->td_retval[0] = pt->p_session->s_sid;
		PROC_UNLOCK(pt);
	}
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct getuid_args {
        int     dummy;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getuid(td, uap)
	struct thread *td;
	struct getuid_args *uap;
{
	struct proc *p = td->td_proc;

	mtx_lock(&Giant);
	td->td_retval[0] = p->p_ucred->cr_ruid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	td->td_retval[1] = p->p_ucred->cr_uid;
#endif
	mtx_unlock(&Giant);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct geteuid_args {
        int     dummy;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
geteuid(td, uap)
	struct thread *td;
	struct geteuid_args *uap;
{
	mtx_lock(&Giant);
	td->td_retval[0] = td->td_proc->p_ucred->cr_uid;
	mtx_unlock(&Giant);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getgid_args {
        int     dummy;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getgid(td, uap)
	struct thread *td;
	struct getgid_args *uap;
{
	struct proc *p = td->td_proc;

	mtx_lock(&Giant);
	td->td_retval[0] = p->p_ucred->cr_rgid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	td->td_retval[1] = p->p_ucred->cr_groups[0];
#endif
	mtx_unlock(&Giant);
	return (0);
}

/*
 * Get effective group ID.  The "egid" is groups[0], and could be obtained
 * via getgroups.  This syscall exists because it is somewhat painful to do
 * correctly in a library function.
 */
#ifndef _SYS_SYSPROTO_H_
struct getegid_args {
        int     dummy;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getegid(td, uap)
	struct thread *td;
	struct getegid_args *uap;
{
	struct proc *p = td->td_proc;

	mtx_lock(&Giant);
	td->td_retval[0] = p->p_ucred->cr_groups[0];
	mtx_unlock(&Giant);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getgroups_args {
	u_int	gidsetsize;
	gid_t	*gidset;
};
#endif
/*
 * MPSAFE
 */
int
getgroups(td, uap)
	struct thread *td;
	register struct getgroups_args *uap;
{
	struct ucred *cred;
	struct proc *p = td->td_proc;
	u_int ngrp;
	int error;

	mtx_lock(&Giant);
	error = 0;
	cred = p->p_ucred;
	if ((ngrp = uap->gidsetsize) == 0) {
		td->td_retval[0] = cred->cr_ngroups;
		goto done2;
	}
	if (ngrp < cred->cr_ngroups) {
		error = EINVAL;
		goto done2;
	}
	ngrp = cred->cr_ngroups;
	if ((error = copyout((caddr_t)cred->cr_groups,
	    (caddr_t)uap->gidset, ngrp * sizeof(gid_t))))
		goto done2;
	td->td_retval[0] = ngrp;
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setsid_args {
        int     dummy;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setsid(td, uap)
	register struct thread *td;
	struct setsid_args *uap;
{
	int error;
	struct proc *p = td->td_proc;

	mtx_lock(&Giant);
	if (p->p_pgid == p->p_pid || pgfind(p->p_pid))
		error = EPERM;
	else {
		(void)enterpgrp(p, p->p_pid, 1);
		td->td_retval[0] = p->p_pid;
		error = 0;
	}
	mtx_unlock(&Giant);
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setpgid(td, uap)
	struct thread *td;
	register struct setpgid_args *uap;
{
	struct proc *curp = td->td_proc;
	register struct proc *targp;	/* target process */
	register struct pgrp *pgrp;	/* target pgrp */
	int error;

	if (uap->pgid < 0)
		return (EINVAL);
	mtx_lock(&Giant);
	sx_slock(&proctree_lock);
	if (uap->pid != 0 && uap->pid != curp->p_pid) {
		if ((targp = pfind(uap->pid)) == NULL || !inferior(targp)) {
			if (targp)
				PROC_UNLOCK(targp);
			error = ESRCH;
			goto done2;
		}
		if ((error = p_cansee(curproc, targp))) {
			PROC_UNLOCK(targp);
			goto done2;
		}
		if (targp->p_pgrp == NULL ||
		    targp->p_session != curp->p_session) {
			PROC_UNLOCK(targp);
			error = EPERM;
			goto done2;
		}
		if (targp->p_flag & P_EXEC) {
			PROC_UNLOCK(targp);
			error = EACCES;
			goto done2;
		}
	} else {
		targp = curp;
		PROC_LOCK(curp);	/* XXX: not needed */
	}
	if (SESS_LEADER(targp)) {
		PROC_UNLOCK(targp);
		error = EPERM;
		goto done2;
	}
	if (uap->pgid == 0)
		uap->pgid = targp->p_pid;
	else if (uap->pgid != targp->p_pid) {
		if ((pgrp = pgfind(uap->pgid)) == 0 ||
		    pgrp->pg_session != curp->p_session) {
			PROC_UNLOCK(targp);
			error = EPERM;
			goto done2;
		}
	}
	/* XXX: We should probably hold the lock across enterpgrp. */
	PROC_UNLOCK(targp);
	error = enterpgrp(targp, uap->pgid, 0);
done2:
	sx_sunlock(&proctree_lock);
	mtx_unlock(&Giant);
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setuid(td, uap)
	struct thread *td;
	struct setuid_args *uap;
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t uid;
	int error;

	uid = uap->uid;
	mtx_lock(&Giant);
	error = 0;
	oldcred = p->p_ucred;

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
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		goto done2;

	newcred = crdup(oldcred);
#ifdef _POSIX_SAVED_IDS
	/*
	 * Do we have "appropriate privileges" (are we root or uid == euid)
	 * If so, we are changing the real uid and/or saved uid.
	 */
	if (
#ifdef POSIX_APPENDIX_B_4_2_2	/* Use the clause from B.4.2.2 */
	    uid == oldcred->cr_uid ||
#endif
	    suser_xxx(oldcred, NULL, PRISON_ROOT) == 0) /* we are using privs */
#endif
	{
		/*
		 * Set the real uid and transfer proc count to new user.
		 */
		if (uid != oldcred->cr_ruid) {
			change_ruid(newcred, uid);
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
	 * Copy credentials so other references do not see our changes.
	 */
	if (uid != oldcred->cr_uid) {
		change_euid(newcred, uid);
		setsugid(p);
	}
	p->p_ucred = newcred;
	crfree(oldcred);
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct seteuid_args {
	uid_t	euid;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
seteuid(td, uap)
	struct thread *td;
	struct seteuid_args *uap;
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t euid;
	int error;

	euid = uap->euid;
	mtx_lock(&Giant);
	error = 0;
	oldcred = p->p_ucred;
	if (euid != oldcred->cr_ruid &&		/* allow seteuid(getuid()) */
	    euid != oldcred->cr_svuid &&	/* allow seteuid(saved uid) */
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		goto done2;
	/*
	 * Everything's okay, do it.  Copy credentials so other references do
	 * not see our changes.
	 */
	newcred = crdup(oldcred);
	if (oldcred->cr_uid != euid) {
		change_euid(newcred, euid);
		setsugid(p);
	}
	p->p_ucred = newcred;
	crfree(oldcred);
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setgid_args {
	gid_t	gid;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setgid(td, uap)
	struct thread *td;
	struct setgid_args *uap;
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t gid;
	int error;

	gid = uap->gid;
	mtx_lock(&Giant);
	error = 0;
	oldcred = p->p_ucred;

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
	    gid != oldcred->cr_groups[0] && /* allow setgid(getegid()) */
#endif
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		goto done2;

	newcred = crdup(oldcred);
#ifdef _POSIX_SAVED_IDS
	/*
	 * Do we have "appropriate privileges" (are we root or gid == egid)
	 * If so, we are changing the real uid and saved gid.
	 */
	if (
#ifdef POSIX_APPENDIX_B_4_2_2	/* use the clause from B.4.2.2 */
	    gid == oldcred->cr_groups[0] ||
#endif
	    suser_xxx(oldcred, NULL, PRISON_ROOT) == 0) /* we are using privs */
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
	if (oldcred->cr_groups[0] != gid) {
		change_egid(newcred, gid);
		setsugid(p);
	}
	p->p_ucred = newcred;
	crfree(oldcred);
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setegid_args {
	gid_t	egid;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setegid(td, uap)
	struct thread *td;
	struct setegid_args *uap;
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t egid;
	int error;

	egid = uap->egid;
	mtx_lock(&Giant);
	error = 0;
	oldcred = p->p_ucred;
	if (egid != oldcred->cr_rgid &&		/* allow setegid(getgid()) */
	    egid != oldcred->cr_svgid &&	/* allow setegid(saved gid) */
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		goto done2;
	newcred = crdup(oldcred);
	if (oldcred->cr_groups[0] != egid) {
		change_egid(newcred, egid);
		setsugid(p);
	}
	p->p_ucred = newcred;
	crfree(oldcred);
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setgroups_args {
	u_int	gidsetsize;
	gid_t	*gidset;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setgroups(td, uap)
	struct thread *td;
	struct setgroups_args *uap;
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	u_int ngrp;
	int error;

	ngrp = uap->gidsetsize;
	mtx_lock(&Giant);
	oldcred = p->p_ucred;
	if ((error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		goto done2;
	if (ngrp > NGROUPS) {
		error = EINVAL;
		goto done2;
	}
	/*
	 * XXX A little bit lazy here.  We could test if anything has
	 * changed before crcopy() and setting P_SUGID.
	 */
	newcred = crdup(oldcred);
	if (ngrp < 1) {
		/*
		 * setgroups(0, NULL) is a legitimate way of clearing the
		 * groups vector on non-BSD systems (which generally do not
		 * have the egid in the groups[0]).  We risk security holes
		 * when running non-BSD software if we do not do the same.
		 */
		newcred->cr_ngroups = 1;
	} else {
		if ((error = copyin((caddr_t)uap->gidset,
		    (caddr_t)newcred->cr_groups, ngrp * sizeof(gid_t)))) {
			crfree(newcred);
			goto done2;
		}
		newcred->cr_ngroups = ngrp;
	}
	setsugid(p);
	p->p_ucred = newcred;
	crfree(oldcred);
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setreuid_args {
	uid_t	ruid;
	uid_t	euid;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setreuid(td, uap)
	register struct thread *td;
	struct setreuid_args *uap;
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t euid, ruid;
	int error;

	euid = uap->euid;
	ruid = uap->ruid;
	mtx_lock(&Giant);
	error = 0;
	oldcred = p->p_ucred;
	if (((ruid != (uid_t)-1 && ruid != oldcred->cr_ruid &&
	      ruid != oldcred->cr_svuid) ||
	     (euid != (uid_t)-1 && euid != oldcred->cr_uid &&
	      euid != oldcred->cr_ruid && euid != oldcred->cr_svuid)) &&
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		goto done2;
	newcred = crdup(oldcred);
	if (euid != (uid_t)-1 && oldcred->cr_uid != euid) {
		change_euid(newcred, euid);
		setsugid(p);
	}
	if (ruid != (uid_t)-1 && oldcred->cr_ruid != ruid) {
		change_ruid(newcred, ruid);
		setsugid(p);
	}
	if ((ruid != (uid_t)-1 || newcred->cr_uid != newcred->cr_ruid) &&
	    newcred->cr_svuid != newcred->cr_uid) {
		change_svuid(newcred, newcred->cr_uid);
		setsugid(p);
	}
	p->p_ucred = newcred;
	crfree(oldcred);
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct setregid_args {
	gid_t	rgid;
	gid_t	egid;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setregid(td, uap)
	register struct thread *td;
	struct setregid_args *uap;
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t egid, rgid;
	int error;

	egid = uap->egid;
	rgid = uap->rgid;
	mtx_lock(&Giant);
	error = 0;
	oldcred = p->p_ucred;
	if (((rgid != (gid_t)-1 && rgid != oldcred->cr_rgid &&
	    rgid != oldcred->cr_svgid) ||
	     (egid != (gid_t)-1 && egid != oldcred->cr_groups[0] &&
	     egid != oldcred->cr_rgid && egid != oldcred->cr_svgid)) &&
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		goto done2;
	newcred = crdup(oldcred);
	if (egid != (gid_t)-1 && oldcred->cr_groups[0] != egid) {
		change_egid(newcred, egid);
		setsugid(p);
	}
	if (rgid != (gid_t)-1 && oldcred->cr_rgid != rgid) {
		change_rgid(newcred, rgid);
		setsugid(p);
	}
	if ((rgid != (gid_t)-1 || newcred->cr_groups[0] != newcred->cr_rgid) &&
	    newcred->cr_svgid != newcred->cr_groups[0]) {
		change_svgid(newcred, newcred->cr_groups[0]);
		setsugid(p);
	}
	p->p_ucred = newcred;
	crfree(oldcred);
done2:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * setresuid(ruid, euid, suid) is like setreuid except control over the
 * saved uid is explicit.
 */

#ifndef _SYS_SYSPROTO_H_
struct setresuid_args {
	uid_t	ruid;
	uid_t	euid;
	uid_t	suid;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setresuid(td, uap)
	register struct thread *td;
	struct setresuid_args *uap;
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t euid, ruid, suid;
	int error;

	euid = uap->euid;
	ruid = uap->ruid;
	suid = uap->suid;
	mtx_lock(&Giant);
	oldcred = p->p_ucred;
	if (((ruid != (uid_t)-1 && ruid != oldcred->cr_ruid &&
	     ruid != oldcred->cr_svuid &&
	      ruid != oldcred->cr_uid) ||
	     (euid != (uid_t)-1 && euid != oldcred->cr_ruid &&
	    euid != oldcred->cr_svuid &&
	      euid != oldcred->cr_uid) ||
	     (suid != (uid_t)-1 && suid != oldcred->cr_ruid &&
	    suid != oldcred->cr_svuid &&
	      suid != oldcred->cr_uid)) &&
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		goto done2;
	newcred = crdup(oldcred);
	if (euid != (uid_t)-1 && oldcred->cr_uid != euid) {
		change_euid(newcred, euid);
		setsugid(p);
	}
	if (ruid != (uid_t)-1 && oldcred->cr_ruid != ruid) {
		change_ruid(newcred, ruid);
		setsugid(p);
	}
	if (suid != (uid_t)-1 && oldcred->cr_svuid != suid) {
		change_svuid(newcred, suid);
		setsugid(p);
	}
	p->p_ucred = newcred;
	crfree(oldcred);
	error = 0;
done2:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * setresgid(rgid, egid, sgid) is like setregid except control over the
 * saved gid is explicit.
 */

#ifndef _SYS_SYSPROTO_H_
struct setresgid_args {
	gid_t	rgid;
	gid_t	egid;
	gid_t	sgid;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setresgid(td, uap)
	register struct thread *td;
	struct setresgid_args *uap;
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t egid, rgid, sgid;
	int error;

	egid = uap->egid;
	rgid = uap->rgid;
	sgid = uap->sgid;
	mtx_lock(&Giant);
	oldcred = p->p_ucred;
	if (((rgid != (gid_t)-1 && rgid != oldcred->cr_rgid &&
	      rgid != oldcred->cr_svgid &&
	      rgid != oldcred->cr_groups[0]) ||
	     (egid != (gid_t)-1 && egid != oldcred->cr_rgid &&
	      egid != oldcred->cr_svgid &&
	      egid != oldcred->cr_groups[0]) ||
	     (sgid != (gid_t)-1 && sgid != oldcred->cr_rgid &&
	      sgid != oldcred->cr_svgid &&
	      sgid != oldcred->cr_groups[0])) &&
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		goto done2;
	newcred = crdup(oldcred);
	if (egid != (gid_t)-1 && oldcred->cr_groups[0] != egid) {
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
	p->p_ucred = newcred;
	crfree(oldcred);
	error = 0;
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct getresuid_args {
	uid_t	*ruid;
	uid_t	*euid;
	uid_t	*suid;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getresuid(td, uap)
	register struct thread *td;
	struct getresuid_args *uap;
{
	struct ucred *cred;
	struct proc *p = td->td_proc;
	int error1 = 0, error2 = 0, error3 = 0;

	mtx_lock(&Giant);
	cred = p->p_ucred;
	if (uap->ruid)
		error1 = copyout((caddr_t)&cred->cr_ruid,
		    (caddr_t)uap->ruid, sizeof(cred->cr_ruid));
	if (uap->euid)
		error2 = copyout((caddr_t)&cred->cr_uid,
		    (caddr_t)uap->euid, sizeof(cred->cr_uid));
	if (uap->suid)
		error3 = copyout((caddr_t)&cred->cr_svuid,
		    (caddr_t)uap->suid, sizeof(cred->cr_svuid));
	mtx_unlock(&Giant);
	return (error1 ? error1 : error2 ? error2 : error3);
}

#ifndef _SYS_SYSPROTO_H_
struct getresgid_args {
	gid_t	*rgid;
	gid_t	*egid;
	gid_t	*sgid;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getresgid(td, uap)
	register struct thread *td;
	struct getresgid_args *uap;
{
	struct ucred *cred;
	struct proc *p = td->td_proc;
	int error1 = 0, error2 = 0, error3 = 0;

	mtx_lock(&Giant);
	cred = p->p_ucred;
	if (uap->rgid)
		error1 = copyout((caddr_t)&cred->cr_rgid,
		    (caddr_t)uap->rgid, sizeof(cred->cr_rgid));
	if (uap->egid)
		error2 = copyout((caddr_t)&cred->cr_groups[0],
		    (caddr_t)uap->egid, sizeof(cred->cr_groups[0]));
	if (uap->sgid)
		error3 = copyout((caddr_t)&cred->cr_svgid,
		    (caddr_t)uap->sgid, sizeof(cred->cr_svgid));
	mtx_unlock(&Giant);
	return (error1 ? error1 : error2 ? error2 : error3);
}

#ifndef _SYS_SYSPROTO_H_
struct issetugid_args {
	int dummy;
};
#endif
/*
 * NOT MPSAFE?
 */
/* ARGSUSED */
int
issetugid(td, uap)
	register struct thread *td;
	struct issetugid_args *uap;
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

/*
 * MPSAFE
 */
int
__setugid(td, uap)
	struct thread *td;
	struct __setugid_args *uap;
{
#ifdef REGRESSION
	int error;

	mtx_lock(&Giant);
	error = 0;
	switch (uap->flag) {
	case 0:
		td->td_proc->p_flag &= ~P_SUGID;
		break;
	case 1:
		td->td_proc->p_flag |= P_SUGID;
		break;
	default:
		error = EINVAL;
		break;
	}
	mtx_unlock(&Giant);
	return (error);
#else /* !REGRESSION */

	return (ENOSYS);
#endif /* REGRESSION */
}

/*
 * Check if gid is a member of the group set.
 */
int
groupmember(gid, cred)
	gid_t gid;
	struct ucred *cred;
{
	register gid_t *gp;
	gid_t *egp;

	egp = &(cred->cr_groups[cred->cr_ngroups]);
	for (gp = cred->cr_groups; gp < egp; gp++)
		if (*gp == gid)
			return (1);
	return (0);
}

/*
 * `suser_enabled' (which can be set by the security.suser_enabled
 * sysctl) determines whether the system 'super-user' policy is in effect.
 * If it is nonzero, an effective uid of 0 connotes special privilege,
 * overriding many mandatory and discretionary protections.  If it is zero,
 * uid 0 is offered no special privilege in the kernel security policy.
 * Setting it to zero may seriously impact the functionality of many
 * existing userland programs, and should not be done without careful
 * consideration of the consequences.
 */
int	suser_enabled = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, suser_enabled, CTLFLAG_RW,
    &suser_enabled, 0, "processes with uid 0 have privilege");
TUNABLE_INT("security.bsd.suser_enabled", &suser_enabled);

/*
 * Test whether the specified credentials imply "super-user" privilege.
 * Return 0 or EPERM.
 */
int
suser(p)
	struct proc *p;
{

	return (suser_xxx(0, p, 0));
}

/*
 * version for when the thread pointer is available and not the proc.
 * (saves having to include proc.h into every file that needs to do the change.)
 */
int
suser_td(td)
	struct thread *td;
{
	return (suser_xxx(0, td->td_proc, 0));
}

/*
 * wrapper to use if you have the thread on hand but not the proc.
 */
int
suser_xxx_td(cred, td, flag)
	struct ucred *cred;
	struct thread *td;
	int flag;
{
	return(suser_xxx(cred, td->td_proc, flag));
}

int
suser_xxx(cred, proc, flag)
	struct ucred *cred;
	struct proc *proc;
	int flag;
{
	if (!suser_enabled)
		return (EPERM);
	if (!cred && !proc) {
		printf("suser_xxx(): THINK!\n");
		return (EPERM);
	}
	if (cred == NULL)
		cred = proc->p_ucred;
	if (cred->cr_uid != 0)
		return (EPERM);
	if (jailed(cred) && !(flag & PRISON_ROOT))
		return (EPERM);
	return (0);
}

/*
 * Test the active securelevel against a given level.  securelevel_gt()
 * implements (securelevel > level).  securelevel_ge() implements
 * (securelevel >= level).  Note that the logic is inverted -- these
 * functions return EPERM on "success" and 0 on "failure".
 *
 * cr is permitted to be NULL for the time being, as there were some
 * existing securelevel checks that occurred without a process/credential
 * context.  In the future this will be disallowed, so a kernel message
 * is displayed.
 */
int
securelevel_gt(struct ucred *cr, int level)
{
	int active_securelevel;

	active_securelevel = securelevel;
	if (cr == NULL)
		printf("securelevel_gt: cr is NULL\n");
	if (cr->cr_prison != NULL) {
		mtx_lock(&cr->cr_prison->pr_mtx);
		active_securelevel = imax(cr->cr_prison->pr_securelevel,
		    active_securelevel);
		mtx_unlock(&cr->cr_prison->pr_mtx);
	}
	return (active_securelevel > level ? EPERM : 0);
}

int
securelevel_ge(struct ucred *cr, int level)
{
	int active_securelevel;

	active_securelevel = securelevel;
	if (cr == NULL)
		printf("securelevel_gt: cr is NULL\n");
	if (cr->cr_prison != NULL) {
		mtx_lock(&cr->cr_prison->pr_mtx);
		active_securelevel = imax(cr->cr_prison->pr_securelevel,
		    active_securelevel);
		mtx_unlock(&cr->cr_prison->pr_mtx);
	}
	return (active_securelevel >= level ? EPERM : 0);
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
	if (!see_other_uids && u1->cr_ruid != u2->cr_ruid) {
		if (suser_xxx(u1, NULL, PRISON_ROOT) != 0)
			return (ESRCH);
	}
	return (0);
}

/*-
 * Determine if p1 "can see" the subject specified by p2.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: Sufficient locks to protect p1->p_ucred and p2->p_ucred must
 *        be held.  Normally, p1 will be curproc, and a lock must be held
 *        for p2.
 * References: p1 and p2 must be valid for the lifetime of the call
 */
int
p_cansee(struct proc *p1, struct proc *p2)
{

	/* Wrap cr_cansee() for all functionality. */
	return (cr_cansee(p1->p_ucred, p2->p_ucred));
}

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

	/*
	 * Jail semantics limit the scope of signalling to proc in the
	 * same jail as cred, if cred is in jail.
	 */
	error = prison_check(cred, proc->p_ucred);
	if (error)
		return (error);

	/*
	 * UNIX signal semantics depend on the status of the P_SUGID
	 * bit on the target process.  If the bit is set, then additional
	 * restrictions are placed on the set of available signals.
	 */
	if (proc->p_flag & P_SUGID) {
		switch (signum) {
		case 0:
		case SIGKILL:
		case SIGINT:
		case SIGTERM:
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
			error = suser_xxx(cred, NULL, PRISON_ROOT);
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
		/* Not permitted without privilege. */
		error = suser_xxx(cred, NULL, PRISON_ROOT);
		if (error)
			return (error);
	}

	return (0);
}


/*-
 * Determine whether p1 may deliver the specified signal to p2.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: Sufficient locks to protect various components of p1 and p2
 *        must be held.  Normally, p1 will be curproc, and a lock must
 *        be held for p2.
 * References: p1 and p2 must be valid for the lifetime of the call
 */
int
p_cansignal(struct proc *p1, struct proc *p2, int signum)
{

	if (p1 == p2)
		return (0);

	/*
	 * UNIX signalling semantics require that processes in the same
	 * session always be able to deliver SIGCONT to one another,
	 * overriding the remaining protections.
	 */
	if (signum == SIGCONT && p1->p_session == p2->p_session)
		return (0);

	return (cr_cansignal(p1->p_ucred, p2, signum));
}

/*-
 * Determine whether p1 may reschedule p2.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: Sufficient locks to protect various components of p1 and p2
 *        must be held.  Normally, p1 will be curproc, and a lock must
 *        be held for p2.
 * References: p1 and p2 must be valid for the lifetime of the call
 */
int
p_cansched(struct proc *p1, struct proc *p2)
{
	int error;

	if (p1 == p2)
		return (0);
	if ((error = prison_check(p1->p_ucred, p2->p_ucred)))
		return (error);
	if (p1->p_ucred->cr_ruid == p2->p_ucred->cr_ruid)
		return (0);
	if (p1->p_ucred->cr_uid == p2->p_ucred->cr_ruid)
		return (0);
	if (suser_xxx(0, p1, PRISON_ROOT) == 0)
		return (0);

#ifdef CAPABILITIES
	if (!cap_check(NULL, p1, CAP_SYS_NICE, PRISON_ROOT))
		return (0);
#endif

	return (EPERM);
}

/*
 * The 'unprivileged_proc_debug' flag may be used to disable a variety of
 * unprivileged inter-process debugging services, including some procfs
 * functionality, ptrace(), and ktrace().  In the past, inter-process
 * debugging has been involved in a variety of security problems, and sites
 * not requiring the service might choose to disable it when hardening
 * systems.
 *
 * XXX: Should modifying and reading this variable require locking?
 * XXX: data declarations should be together near the beginning of the file.
 */
static int	unprivileged_proc_debug = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, unprivileged_proc_debug, CTLFLAG_RW,
    &unprivileged_proc_debug, 0,
    "Unprivileged processes may use process debugging facilities");

/*-
 * Determine whether p1 may debug p2.
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: Sufficient locks to protect various components of p1 and p2
 *        must be held.  Normally, p1 will be curproc, and a lock must
 *        be held for p2.
 * References: p1 and p2 must be valid for the lifetime of the call
 */
int
p_candebug(struct proc *p1, struct proc *p2)
{
	int credentialchanged, error, grpsubset, i, uidsubset;

	if (!unprivileged_proc_debug) {
		error = suser_xxx(NULL, p1, PRISON_ROOT);
		if (error)
			return (error);
	}
	if (p1 == p2)
		return (0);
	if ((error = prison_check(p1->p_ucred, p2->p_ucred)))
		return (error);

	/*
	 * Is p2's group set a subset of p1's effective group set?  This
	 * includes p2's egid, group access list, rgid, and svgid.
	 */
	grpsubset = 1;
	for (i = 0; i < p2->p_ucred->cr_ngroups; i++) {
		if (!groupmember(p2->p_ucred->cr_groups[i], p1->p_ucred)) {
			grpsubset = 0;
			break;
		}
	}
	grpsubset = grpsubset &&
	    groupmember(p2->p_ucred->cr_rgid, p1->p_ucred) &&
	    groupmember(p2->p_ucred->cr_svgid, p1->p_ucred);

	/*
	 * Are the uids present in p2's credential equal to p1's
	 * effective uid?  This includes p2's euid, svuid, and ruid.
	 */
	uidsubset = (p1->p_ucred->cr_uid == p2->p_ucred->cr_uid &&
	    p1->p_ucred->cr_uid == p2->p_ucred->cr_svuid &&
	    p1->p_ucred->cr_uid == p2->p_ucred->cr_ruid);

	/*
	 * Has the credential of the process changed since the last exec()?
	 */
	credentialchanged = (p2->p_flag & P_SUGID);

	/*
	 * If p2's gids aren't a subset, or the uids aren't a subset,
	 * or the credential has changed, require appropriate privilege
	 * for p1 to debug p2.  For POSIX.1e capabilities, this will
	 * require CAP_SYS_PTRACE.
	 */
	if (!grpsubset || !uidsubset || credentialchanged) {
		error = suser_xxx(NULL, p1, PRISON_ROOT);
		if (error)
			return (error);
	}

	/* Can't trace init when securelevel > 0. */
	if (p2 == initproc) {
		error = securelevel_gt(p1->p_ucred, 0);
		if (error)
			return (error);
	}

	/*
	 * Can't trace a process that's currently exec'ing.
	 * XXX: Note, this is not a security policy decision, it's a
	 * basic correctness/functionality decision.  Therefore, this check
	 * should be moved to the caller's of p_candebug().
	 */
	if ((p2->p_flag & P_INEXEC) != 0)
		return (EAGAIN);

	return (0);
}

/*
 * Allocate a zeroed cred structure.
 */
struct ucred *
crget()
{
	register struct ucred *cr;

	MALLOC(cr, struct ucred *, sizeof(*cr), M_CRED, M_WAITOK | M_ZERO);
	cr->cr_ref = 1;
	mtx_init(&cr->cr_mtx, "ucred", MTX_DEF);
	return (cr);
}

/*
 * Claim another reference to a ucred structure.
 */
struct ucred *
crhold(cr)
	struct ucred *cr;
{

	mtx_lock(&cr->cr_mtx);
	cr->cr_ref++;
	mtx_unlock(&cr->cr_mtx);
	return (cr);
}

/*
 * Free a cred structure.
 * Throws away space when ref count gets to 0.
 */
void
crfree(cr)
	struct ucred *cr;
{

	mtx_lock(&cr->cr_mtx);
	KASSERT(cr->cr_ref > 0, ("bad ucred refcount: %d", cr->cr_ref));
	if (--cr->cr_ref == 0) {
		mtx_destroy(&cr->cr_mtx);
		/*
		 * Some callers of crget(), such as nfs_statfs(),
		 * allocate a temporary credential, but don't
		 * allocate a uidinfo structure.
		 */
		if (cr->cr_uidinfo != NULL)
			uifree(cr->cr_uidinfo);
		if (cr->cr_ruidinfo != NULL)
			uifree(cr->cr_ruidinfo);
		/*
		 * Free a prison, if any.
		 */
		if (jailed(cr))
			prison_free(cr->cr_prison);
		FREE((caddr_t)cr, M_CRED);
	} else
		mtx_unlock(&cr->cr_mtx);
}

/*
 * Check to see if this ucred is shared.
 */
int
crshared(cr)
	struct ucred *cr;
{
	int shared;

	mtx_lock(&cr->cr_mtx);
	shared = (cr->cr_ref > 1);
	mtx_unlock(&cr->cr_mtx);
	return (shared);
}

/*
 * Copy a ucred's contents from a template.  Does not block.
 */
void
crcopy(dest, src)
	struct ucred *dest, *src;
{

	KASSERT(crshared(dest) == 0, ("crcopy of shared ucred"));
	bcopy(&src->cr_startcopy, &dest->cr_startcopy,
	    (unsigned)((caddr_t)&src->cr_endcopy -
		(caddr_t)&src->cr_startcopy));
	uihold(dest->cr_uidinfo);
	uihold(dest->cr_ruidinfo);
	if (jailed(dest))
		prison_hold(dest->cr_prison);
}

/*
 * Dup cred struct to a new held one.
 */
struct ucred *
crdup(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	newcr = crget();
	crcopy(newcr, cr);
	return (newcr);
}

/*
 * small routine to swap a thread's current ucred for the correct one
 * taken from the process.
 */
void
cred_update_thread(struct thread *td)
{
	struct proc *p;

	p = td->td_proc;
	if (td->td_ucred != NULL) {
		mtx_lock(&Giant);
		crfree(td->td_ucred);
		mtx_unlock(&Giant);
		td->td_ucred = NULL;
	}
	PROC_LOCK(p);
	td->td_ucred = crhold(p->p_ucred);
	PROC_UNLOCK(p);
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getlogin(td, uap)
	struct thread *td;
	struct getlogin_args *uap;
{
	int error;
	struct proc *p = td->td_proc;

	mtx_lock(&Giant);
	if (uap->namelen > MAXLOGNAME)
		uap->namelen = MAXLOGNAME;
	error = copyout((caddr_t) p->p_pgrp->pg_session->s_login,
	    (caddr_t) uap->namebuf, uap->namelen);
	mtx_unlock(&Giant);
	return(error);
}

/*
 * Set login name.
 */
#ifndef _SYS_SYSPROTO_H_
struct setlogin_args {
	char	*namebuf;
};
#endif
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setlogin(td, uap)
	struct thread *td;
	struct setlogin_args *uap;
{
	struct proc *p = td->td_proc;
	int error;
	char logintmp[MAXLOGNAME];

	mtx_lock(&Giant);
	if ((error = suser_xxx(0, p, PRISON_ROOT)) != 0)
		goto done2;
	error = copyinstr((caddr_t) uap->namebuf, (caddr_t) logintmp,
	    sizeof(logintmp), (size_t *)0);
	if (error == ENAMETOOLONG)
		error = EINVAL;
	else if (!error)
		(void)memcpy(p->p_pgrp->pg_session->s_login, logintmp,
		    sizeof(logintmp));
done2:
	mtx_unlock(&Giant);
	return (error);
}

void
setsugid(p)
	struct proc *p;
{
	p->p_flag |= P_SUGID;
	if (!(p->p_pfsflags & PF_ISUGID))
		p->p_stops = 0;
}

/*-
 * Change a process's effective uid.
 * Side effects: newcred->cr_uid and newcred->cr_uidinfo will be modified.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_euid(newcred, euid)
	struct ucred *newcred;
	uid_t euid;
{

	newcred->cr_uid = euid;
	uifree(newcred->cr_uidinfo);
	newcred->cr_uidinfo = uifind(euid);
}

/*-
 * Change a process's effective gid.
 * Side effects: newcred->cr_gid will be modified.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_egid(newcred, egid)
	struct ucred *newcred;
	gid_t egid;
{

	newcred->cr_groups[0] = egid;
}

/*-
 * Change a process's real uid.
 * Side effects: newcred->cr_ruid will be updated, newcred->cr_ruidinfo
 *               will be updated, and the old and new cr_ruidinfo proc
 *               counts will be updated.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_ruid(newcred, ruid)
	struct ucred *newcred;
	uid_t ruid;
{

	(void)chgproccnt(newcred->cr_ruidinfo, -1, 0);
	newcred->cr_ruid = ruid;
	uifree(newcred->cr_ruidinfo);
	newcred->cr_ruidinfo = uifind(ruid);
	(void)chgproccnt(newcred->cr_ruidinfo, 1, 0);
}

/*-
 * Change a process's real gid.
 * Side effects: newcred->cr_rgid will be updated.
 * References: newcred must be an exclusive credential reference for the
 *             duration of the call.
 */
void
change_rgid(newcred, rgid)
	struct ucred *newcred;
	gid_t rgid;
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
change_svuid(newcred, svuid)
	struct ucred *newcred;
	uid_t svuid;
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
change_svgid(newcred, svgid)
	struct ucred *newcred;
	gid_t svgid;
{

	newcred->cr_svgid = svgid;
}
