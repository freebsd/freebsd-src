/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
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
#include "opt_global.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/malloc.h>
#include <sys/pioctl.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/jail.h>

static MALLOC_DEFINE(M_CRED, "cred", "credentials");

#ifndef _SYS_SYSPROTO_H_
struct getpid_args {
	int	dummy;
};
#endif

/*
 * getpid - MP SAFE
 */

/* ARGSUSED */
int
getpid(p, uap)
	struct proc *p;
	struct getpid_args *uap;
{

	p->p_retval[0] = p->p_pid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	PROC_LOCK(p);
	p->p_retval[1] = p->p_pptr->p_pid;
	PROC_UNLOCK(p);
#endif
	return (0);
}

/*
 * getppid - MP SAFE
 */

#ifndef _SYS_SYSPROTO_H_
struct getppid_args {
        int     dummy;
};
#endif
/* ARGSUSED */
int
getppid(p, uap)
	struct proc *p;
	struct getppid_args *uap;
{

	PROC_LOCK(p);
	p->p_retval[0] = p->p_pptr->p_pid;
	PROC_UNLOCK(p);
	return (0);
}

/* 
 * Get process group ID; note that POSIX getpgrp takes no parameter 
 *
 * MP SAFE
 */
#ifndef _SYS_SYSPROTO_H_
struct getpgrp_args {
        int     dummy;
};
#endif

int
getpgrp(p, uap)
	struct proc *p;
	struct getpgrp_args *uap;
{

	p->p_retval[0] = p->p_pgrp->pg_id;
	return (0);
}

/* Get an arbitary pid's process group id */
#ifndef _SYS_SYSPROTO_H_
struct getpgid_args {
	pid_t	pid;
};
#endif

int
getpgid(p, uap)
	struct proc *p;
	struct getpgid_args *uap;
{
	struct proc *pt;
	int error;

	if (uap->pid == 0)
		p->p_retval[0] = p->p_pgrp->pg_id;
	else {
		if ((pt = pfind(uap->pid)) == NULL)
			return ESRCH;
		if ((error = p_cansee(p, pt))) {
			PROC_UNLOCK(pt);
			return (error);
		}
		p->p_retval[0] = pt->p_pgrp->pg_id;
		PROC_UNLOCK(pt);
	}
	return 0;
}

/*
 * Get an arbitary pid's session id.
 */
#ifndef _SYS_SYSPROTO_H_
struct getsid_args {
	pid_t	pid;
};
#endif

int
getsid(p, uap)
	struct proc *p;
	struct getsid_args *uap;
{
	struct proc *pt;
	int error;

	if (uap->pid == 0)
		p->p_retval[0] = p->p_session->s_sid;
	else {
		if ((pt = pfind(uap->pid)) == NULL)
			return ESRCH;
		if ((error = p_cansee(p, pt))) {
			PROC_UNLOCK(pt);
			return (error);
		}
		p->p_retval[0] = pt->p_session->s_sid;
		PROC_UNLOCK(pt);
	}
	return 0;
}


/*
 * getuid() - MP SAFE
 */
#ifndef _SYS_SYSPROTO_H_
struct getuid_args {
        int     dummy;
};
#endif

/* ARGSUSED */
int
getuid(p, uap)
	struct proc *p;
	struct getuid_args *uap;
{

	p->p_retval[0] = p->p_ucred->cr_ruid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	p->p_retval[1] = p->p_ucred->cr_uid;
#endif
	return (0);
}

/*
 * geteuid() - MP SAFE
 */
#ifndef _SYS_SYSPROTO_H_
struct geteuid_args {
        int     dummy;
};
#endif

/* ARGSUSED */
int
geteuid(p, uap)
	struct proc *p;
	struct geteuid_args *uap;
{

	p->p_retval[0] = p->p_ucred->cr_uid;
	return (0);
}

/*
 * getgid() - MP SAFE
 */
#ifndef _SYS_SYSPROTO_H_
struct getgid_args {
        int     dummy;
};
#endif

/* ARGSUSED */
int
getgid(p, uap)
	struct proc *p;
	struct getgid_args *uap;
{

	p->p_retval[0] = p->p_ucred->cr_rgid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	p->p_retval[1] = p->p_ucred->cr_groups[0];
#endif
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

/* ARGSUSED */
int
getegid(p, uap)
	struct proc *p;
	struct getegid_args *uap;
{

	p->p_retval[0] = p->p_ucred->cr_groups[0];
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getgroups_args {
	u_int	gidsetsize;
	gid_t	*gidset;
};
#endif
int
getgroups(p, uap)
	struct proc *p;
	register struct	getgroups_args *uap;
{
	struct ucred *cred = p->p_ucred;
	u_int ngrp;
	int error;

	if ((ngrp = uap->gidsetsize) == 0) {
		p->p_retval[0] = cred->cr_ngroups;
		return (0);
	}
	if (ngrp < cred->cr_ngroups)
		return (EINVAL);
	ngrp = cred->cr_ngroups;
	if ((error = copyout((caddr_t)cred->cr_groups,
	    (caddr_t)uap->gidset, ngrp * sizeof(gid_t))))
		return (error);
	p->p_retval[0] = ngrp;
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setsid_args {
        int     dummy;
};
#endif

/* ARGSUSED */
int
setsid(p, uap)
	register struct proc *p;
	struct setsid_args *uap;
{

	if (p->p_pgid == p->p_pid || pgfind(p->p_pid)) {
		return (EPERM);
	} else {
		(void)enterpgrp(p, p->p_pid, 1);
		p->p_retval[0] = p->p_pid;
		return (0);
	}
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
	int	pid;	/* target process id */
	int	pgid;	/* target pgrp id */
};
#endif
/* ARGSUSED */
int
setpgid(curp, uap)
	struct proc *curp;
	register struct setpgid_args *uap;
{
	register struct proc *targp;		/* target process */
	register struct pgrp *pgrp;		/* target pgrp */
	int error;

	if (uap->pgid < 0)
		return (EINVAL);
	if (uap->pid != 0 && uap->pid != curp->p_pid) {
		if ((targp = pfind(uap->pid)) == NULL || !inferior(targp)) {
			if (targp)
				PROC_UNLOCK(targp);
			return (ESRCH);
		}
		if ((error = p_cansee(curproc, targp))) {
			PROC_UNLOCK(targp);
			return (error);
		}
		if (targp->p_pgrp == NULL ||
		    targp->p_session != curp->p_session) {
			PROC_UNLOCK(targp);
			return (EPERM);
		}
		if (targp->p_flag & P_EXEC) {
			PROC_UNLOCK(targp);
			return (EACCES);
		}
	} else {
		targp = curp;
		PROC_LOCK(curp);	/* XXX: not needed */
	}
	if (SESS_LEADER(targp)) {
		PROC_UNLOCK(targp);
		return (EPERM);
	}
	if (uap->pgid == 0)
		uap->pgid = targp->p_pid;
	else if (uap->pgid != targp->p_pid)
		if ((pgrp = pgfind(uap->pgid)) == 0 ||
	            pgrp->pg_session != curp->p_session) {
			PROC_UNLOCK(targp);
			return (EPERM);
		}
	/* XXX: We should probably hold the lock across enterpgrp. */
	PROC_UNLOCK(targp);
	return (enterpgrp(targp, uap->pgid, 0));
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
setuid(p, uap)
	struct proc *p;
	struct setuid_args *uap;
{
	struct ucred *newcred, *oldcred;
	uid_t uid;
	int error;

	uid = uap->uid;
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
	 * 2: We determine if the real and/or saved uid's are going to
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
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)))
		return (error);

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
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct seteuid_args {
	uid_t	euid;
};
#endif
/* ARGSUSED */
int
seteuid(p, uap)
	struct proc *p;
	struct seteuid_args *uap;
{
	struct ucred *newcred, *oldcred;
	uid_t euid;
	int error;

	euid = uap->euid;
	oldcred = p->p_ucred;
	if (euid != oldcred->cr_ruid &&		/* allow seteuid(getuid()) */
	    euid != oldcred->cr_svuid &&	/* allow seteuid(saved uid) */
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)))
		return (error);
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
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setgid_args {
	gid_t	gid;
};
#endif
/* ARGSUSED */
int
setgid(p, uap)
	struct proc *p;
	struct setgid_args *uap;
{
	struct ucred *newcred, *oldcred;
	gid_t gid;
	int error;

	gid = uap->gid;
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
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)))
		return (error);

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
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setegid_args {
	gid_t	egid;
};
#endif
/* ARGSUSED */
int
setegid(p, uap)
	struct proc *p;
	struct setegid_args *uap;
{
	struct ucred *newcred, *oldcred;
	gid_t egid;
	int error;

	egid = uap->egid;
	oldcred = p->p_ucred;
	if (egid != oldcred->cr_rgid &&		/* allow setegid(getgid()) */
	    egid != oldcred->cr_svgid &&	/* allow setegid(saved gid) */
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)))
		return (error);
	newcred = crdup(oldcred);
	if (oldcred->cr_groups[0] != egid) {
		change_egid(newcred, egid);
		setsugid(p);
	}
	p->p_ucred = newcred;
	crfree(oldcred);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setgroups_args {
	u_int	gidsetsize;
	gid_t	*gidset;
};
#endif
/* ARGSUSED */
int
setgroups(p, uap)
	struct proc *p;
	struct setgroups_args *uap;
{
	struct ucred *newcred, *oldcred;
	u_int ngrp;
	int error;

	ngrp = uap->gidsetsize;
	oldcred = p->p_ucred;
	if ((error = suser_xxx(oldcred, NULL, PRISON_ROOT)))
		return (error);
	if (ngrp > NGROUPS)
		return (EINVAL);
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
			return (error);
		}
		newcred->cr_ngroups = ngrp;
	}
	setsugid(p);
	p->p_ucred = newcred;
	crfree(oldcred);
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setreuid_args {
	uid_t	ruid;
	uid_t	euid;
};
#endif
/* ARGSUSED */
int
setreuid(p, uap)
	register struct proc *p;
	struct setreuid_args *uap;
{
	struct ucred *newcred, *oldcred;
	uid_t ruid, euid;
	int error;

	ruid = uap->ruid;
	euid = uap->euid;
	oldcred = p->p_ucred;
	if (((ruid != (uid_t)-1 && ruid != oldcred->cr_ruid &&
	      ruid != oldcred->cr_svuid) ||
	     (euid != (uid_t)-1 && euid != oldcred->cr_uid &&
	      euid != oldcred->cr_ruid && euid != oldcred->cr_svuid)) &&
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		return (error);
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
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setregid_args {
	gid_t	rgid;
	gid_t	egid;
};
#endif
/* ARGSUSED */
int
setregid(p, uap)
	register struct proc *p;
	struct setregid_args *uap;
{
	struct ucred *newcred, *oldcred;
	gid_t rgid, egid;
	int error;

	rgid = uap->rgid;
	egid = uap->egid;
	oldcred = p->p_ucred;
	if (((rgid != (gid_t)-1 && rgid != oldcred->cr_rgid &&
	    rgid != oldcred->cr_svgid) ||
	     (egid != (gid_t)-1 && egid != oldcred->cr_groups[0] &&
	     egid != oldcred->cr_rgid && egid != oldcred->cr_svgid)) &&
	    (error = suser_xxx(oldcred, NULL, PRISON_ROOT)) != 0)
		return (error);

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
	return (0);
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
/* ARGSUSED */
int
setresuid(p, uap)
	register struct proc *p;
	struct setresuid_args *uap;
{
	struct ucred *newcred, *oldcred;
	uid_t ruid, euid, suid;
	int error;

	ruid = uap->ruid;
	euid = uap->euid;
	suid = uap->suid;
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
		return (error);

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
	return (0);
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
/* ARGSUSED */
int
setresgid(p, uap)
	register struct proc *p;
	struct setresgid_args *uap;
{
	struct ucred *newcred, *oldcred;
	gid_t rgid, egid, sgid;
	int error;

	rgid = uap->rgid;
	egid = uap->egid;
	sgid = uap->sgid;
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
		return (error);

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
	return (0);
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
getresuid(p, uap)
	register struct proc *p;
	struct getresuid_args *uap;
{
	struct ucred *cred = p->p_ucred;
	int error1 = 0, error2 = 0, error3 = 0;

	if (uap->ruid)
		error1 = copyout((caddr_t)&cred->cr_ruid,
		    (caddr_t)uap->ruid, sizeof(cred->cr_ruid));
	if (uap->euid)
		error2 = copyout((caddr_t)&cred->cr_uid,
		    (caddr_t)uap->euid, sizeof(cred->cr_uid));
	if (uap->suid)
		error3 = copyout((caddr_t)&cred->cr_svuid,
		    (caddr_t)uap->suid, sizeof(cred->cr_svuid));
	return error1 ? error1 : (error2 ? error2 : error3);
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
getresgid(p, uap)
	register struct proc *p;
	struct getresgid_args *uap;
{
	struct ucred *cred = p->p_ucred;
	int error1 = 0, error2 = 0, error3 = 0;

	if (uap->rgid)
		error1 = copyout((caddr_t)&cred->cr_rgid,
		    (caddr_t)uap->rgid, sizeof(cred->cr_rgid));
	if (uap->egid)
		error2 = copyout((caddr_t)&cred->cr_groups[0],
		    (caddr_t)uap->egid, sizeof(cred->cr_groups[0]));
	if (uap->sgid)
		error3 = copyout((caddr_t)&cred->cr_svgid,
		    (caddr_t)uap->sgid, sizeof(cred->cr_svgid));
	return error1 ? error1 : (error2 ? error2 : error3);
}


#ifndef _SYS_SYSPROTO_H_
struct issetugid_args {
	int dummy;
};
#endif
/* ARGSUSED */
int
issetugid(p, uap)
	register struct proc *p;
	struct issetugid_args *uap;
{
	/*
	 * Note: OpenBSD sets a P_SUGIDEXEC flag set at execve() time,
	 * we use P_SUGID because we consider changing the owners as
	 * "tainting" as well.
	 * This is significant for procs that start as root and "become"
	 * a user without an exec - programs cannot know *everything*
	 * that libc *might* have put in their data segment.
	 */
	p->p_retval[0] = (p->p_flag & P_SUGID) ? 1 : 0;
	return (0);
}

int
__setugid(p, uap)
	struct proc *p;
	struct __setugid_args *uap;
{

#ifdef REGRESSION
	switch (uap->flag) {
	case 0:
		p->p_flag &= ~P_SUGID;
		return (0);
	case 1:
		p->p_flag |= P_SUGID;
		return (0);
	default:
		return (EINVAL);
	}
#else /* !REGRESSION */
	return (ENOSYS);
#endif /* !REGRESSION */
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

static int suser_permitted = 1;

SYSCTL_INT(_kern, OID_AUTO, suser_permitted, CTLFLAG_RW, &suser_permitted, 0,
    "processes with uid 0 have privilege");

/*
 * Test whether the specified credentials imply "super-user"
 * privilege; if so, and we have accounting info, set the flag
 * indicating use of super-powers.
 * Returns 0 or error.
 */
int
suser(p)
	struct proc *p;
{
	return suser_xxx(0, p, 0);
}

int
suser_xxx(cred, proc, flag)
	struct ucred *cred;
	struct proc *proc;
	int flag;
{
	if (!suser_permitted)
		return (EPERM);
	if (!cred && !proc) {
		printf("suser_xxx(): THINK!\n");
		return (EPERM);
	}
	if (!cred) 
		cred = proc->p_ucred;
	if (cred->cr_uid != 0) 
		return (EPERM);
	if (jailed(cred) && !(flag & PRISON_ROOT))
		return (EPERM);
	return (0);
}

/*
 * u_cansee(u1, u2): determine if u1 "can see" the subject specified by u2
 * Arguments: imutable credentials u1, u2
 * Returns: 0 for permitted, an errno value otherwise
 * Locks: none
 * References: u1 and u2 must be valid for the lifetime of the call
 *             u1 may equal u2, in which case only one reference is required
 */
int
u_cansee(struct ucred *u1, struct ucred *u2)
{
	int error;

	if ((error = prison_check(u1, u2)))
		return (error);
	if (!ps_showallprocs && u1->cr_ruid != u2->cr_ruid) {
		if (suser_xxx(u1, NULL, PRISON_ROOT) != 0)
			return (ESRCH);
	}
	return (0);
}

int
p_cansee(struct proc *p1, struct proc *p2)
{

	/* Wrap u_cansee() for all functionality. */
	return (u_cansee(p1->p_ucred, p2->p_ucred));
}

/*
 * Can process p1 send the signal signum to process p2?
 */
int
p_cansignal(struct proc *p1, struct proc *p2, int signum)
{
	int	error;
	
	if (p1 == p2)
		return (0);

	/*
	 * Jail semantics limit the scope of signalling to p2 in the same
	 * jail as p1, if p1 is in jail.
	 */
	if ((error = prison_check(p1->p_ucred, p2->p_ucred)))
		return (error);

	/*
	 * UNIX signalling semantics require that processes in the same
	 * session always be able to deliver SIGCONT to one another,
	 * overriding the remaining protections.
	 */
	if (signum == SIGCONT && p1->p_session == p2->p_session)
		return (0);

	/*
	 * UNIX uid semantics depend on the status of the P_SUGID
	 * bit on the target process.  If the bit is set, then more
	 * restricted signal sets are permitted.
	 */
	if (p2->p_flag & P_SUGID) {
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
			break;
		default:
			/* Not permitted, try privilege. */
			error = suser_xxx(NULL, p1, PRISON_ROOT);
			if (error)
				return (error);
		}
	}

	/*
	 * Generally, the object credential's ruid or svuid must match the
	 * subject credential's ruid or euid.
	 */
	if (p1->p_ucred->cr_ruid != p2->p_ucred->cr_ruid &&
	    p1->p_ucred->cr_ruid != p2->p_ucred->cr_svuid &&
	    p1->p_ucred->cr_uid != p2->p_ucred->cr_ruid &&
	    p1->p_ucred->cr_uid != p2->p_ucred->cr_svuid) {
		/* Not permitted, try privilege. */
		error = suser_xxx(NULL, p1, PRISON_ROOT);
		if (error)
			return (error);
	}

        return (0);
}

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

	if (!suser_xxx(0, p1, PRISON_ROOT))
		return (0);

#ifdef CAPABILITIES
	if (!cap_check_xxx(0, p1, CAP_SYS_NICE, PRISON_ROOT))
		return (0);
#endif

	return (EPERM);
}

int
p_candebug(struct proc *p1, struct proc *p2)
{
	int error;

	if (p1 == p2)
		return (0);

	if ((error = prison_check(p1->p_ucred, p2->p_ucred)))
		return (error);

	/* not owned by you, has done setuid (unless you're root) */
	/* add a CAP_SYS_PTRACE here? */
	if (p1->p_ucred->cr_uid != p2->p_ucred->cr_uid ||
	    p1->p_ucred->cr_uid != p2->p_ucred->cr_svuid ||
	    p1->p_ucred->cr_uid != p2->p_ucred->cr_ruid ||
	    p2->p_flag & P_SUGID)
		if ((error = suser_xxx(0, p1, PRISON_ROOT)))
			return (error);

	/* can't trace init when securelevel > 0 */
	if (securelevel > 0 && p2->p_pid == 1)
		return (EPERM);

	return (0);
}

/*
 * Allocate a zeroed cred structure.
 */
struct ucred *
crget()
{
	register struct ucred *cr;

	MALLOC(cr, struct ucred *, sizeof(*cr), M_CRED, M_WAITOK|M_ZERO);
	cr->cr_ref = 1;
	mtx_init(&cr->cr_mtx, "ucred", MTX_DEF);
	return (cr);
}

/*
 * Claim another reference to a ucred structure
 */
void
crhold(cr)
	struct ucred *cr;
{

	mtx_lock(&cr->cr_mtx);
	cr->cr_ref++;
	mtx_unlock(&(cr)->cr_mtx);
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
	} else {
		mtx_unlock(&cr->cr_mtx);
	}
}

/*
 * Copy cred structure to a new one and free the old one.
 */
struct ucred *
crcopy(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	mtx_lock(&cr->cr_mtx);
	if (cr->cr_ref == 1) {
		mtx_unlock(&cr->cr_mtx);
		return (cr);
	}
	mtx_unlock(&cr->cr_mtx);
	newcr = crdup(cr);
	crfree(cr);
	return (newcr);
}

/*
 * Dup cred struct to a new held one.
 */
struct ucred *
crdup(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	MALLOC(newcr, struct ucred *, sizeof(*cr), M_CRED, M_WAITOK);
	*newcr = *cr;
	mtx_init(&newcr->cr_mtx, "ucred", MTX_DEF);
	uihold(newcr->cr_uidinfo);
	uihold(newcr->cr_ruidinfo);
	if (jailed(newcr))
		prison_hold(newcr->cr_prison);
	newcr->cr_ref = 1;
	return (newcr);
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
getlogin(p, uap)
	struct proc *p;
	struct getlogin_args *uap;
{

	if (uap->namelen > MAXLOGNAME)
		uap->namelen = MAXLOGNAME;
	return (copyout((caddr_t) p->p_pgrp->pg_session->s_login,
	    (caddr_t) uap->namebuf, uap->namelen));
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
setlogin(p, uap)
	struct proc *p;
	struct setlogin_args *uap;
{
	int error;
	char logintmp[MAXLOGNAME];

	if ((error = suser_xxx(0, p, PRISON_ROOT)))
		return (error);
	error = copyinstr((caddr_t) uap->namebuf, (caddr_t) logintmp,
	    sizeof(logintmp), (size_t *)0);
	if (error == ENAMETOOLONG)
		error = EINVAL;
	else if (!error)
		(void) memcpy(p->p_pgrp->pg_session->s_login, logintmp,
		    sizeof(logintmp));
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

/*
 * change_euid(): Change a process's effective uid.
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

/*
 * change_egid(): Change a process's effective gid.
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

/*
 * change_ruid(): Change a process's real uid.
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

/*
 * change_rgid(): Change a process's real gid.
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

/*
 * change_svuid(): Change a process's saved uid.
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

/*
 * change_svgid(): Change a process's saved gid.
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
