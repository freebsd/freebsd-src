/*-
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
 */

/*
 * System calls related to processes and protection
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/acct.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/sysproto.h>
#include <sys/jail.h>
#include <sys/pioctl.h>
#include <sys/resourcevar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
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
getpid(struct thread *td, struct getpid_args *uap)
{
	struct proc *p = td->td_proc;

	td->td_retval[0] = p->p_pid;
#if defined(COMPAT_43)
	PROC_LOCK(p);
	td->td_retval[1] = p->p_pptr->p_pid;
	PROC_UNLOCK(p);
#endif
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
getppid(struct thread *td, struct getppid_args *uap)
{
	struct proc *p = td->td_proc;

	PROC_LOCK(p);
	td->td_retval[0] = p->p_pptr->p_pid;
	PROC_UNLOCK(p);
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
getpgrp(struct thread *td, struct getpgrp_args *uap)
{
	struct proc *p = td->td_proc;

	PROC_LOCK(p);
	td->td_retval[0] = p->p_pgrp->pg_id;
	PROC_UNLOCK(p);
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
getpgid(struct thread *td, struct getpgid_args *uap)
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
getsid(struct thread *td, struct getsid_args *uap)
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
	td->td_retval[0] = p->p_session->s_sid;
	PROC_UNLOCK(p);
	return (0);
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
getuid(struct thread *td, struct getuid_args *uap)
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
geteuid(struct thread *td, struct geteuid_args *uap)
{

	td->td_retval[0] = td->td_ucred->cr_uid;
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
getgid(struct thread *td, struct getgid_args *uap)
{

	td->td_retval[0] = td->td_ucred->cr_rgid;
#if defined(COMPAT_43)
	td->td_retval[1] = td->td_ucred->cr_groups[0];
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getegid(struct thread *td, struct getegid_args *uap)
{

	td->td_retval[0] = td->td_ucred->cr_groups[0];
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
getgroups(struct thread *td, register struct getgroups_args *uap)
{
	struct ucred *cred;
	u_int ngrp;
	int error;

	cred = td->td_ucred;
	if ((ngrp = uap->gidsetsize) == 0) {
		td->td_retval[0] = cred->cr_ngroups;
		return (0);
	}
	if (ngrp < cred->cr_ngroups)
		return (EINVAL);
	ngrp = cred->cr_ngroups;
	error = copyout(cred->cr_groups, uap->gidset, ngrp * sizeof(gid_t));
	if (error == 0)
		td->td_retval[0] = ngrp;
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
setsid(register struct thread *td, struct setsid_args *uap)
{
	struct pgrp *pgrp;
	int error;
	struct proc *p = td->td_proc;
	struct pgrp *newpgrp;
	struct session *newsess;

	error = 0;
	pgrp = NULL;

	MALLOC(newpgrp, struct pgrp *, sizeof(struct pgrp), M_PGRP, M_WAITOK | M_ZERO);
	MALLOC(newsess, struct session *, sizeof(struct session), M_SESSION, M_WAITOK | M_ZERO);

	sx_xlock(&proctree_lock);

	if (p->p_pgid == p->p_pid || (pgrp = pgfind(p->p_pid)) != NULL) {
		if (pgrp != NULL)
			PGRP_UNLOCK(pgrp);
		error = EPERM;
	} else {
		(void)enterpgrp(p, p->p_pid, newpgrp, newsess);
		td->td_retval[0] = p->p_pid;
		newpgrp = NULL;
		newsess = NULL;
	}

	sx_xunlock(&proctree_lock);

	if (newpgrp != NULL)
		FREE(newpgrp, M_PGRP);
	if (newsess != NULL)
		FREE(newsess, M_SESSION);

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
setpgid(struct thread *td, register struct setpgid_args *uap)
{
	struct proc *curp = td->td_proc;
	register struct proc *targp;	/* target process */
	register struct pgrp *pgrp;	/* target pgrp */
	int error;
	struct pgrp *newpgrp;

	if (uap->pgid < 0)
		return (EINVAL);

	error = 0;

	MALLOC(newpgrp, struct pgrp *, sizeof(struct pgrp), M_PGRP, M_WAITOK | M_ZERO);

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
	sx_xunlock(&proctree_lock);
	KASSERT((error == 0) || (newpgrp != NULL),
	    ("setpgid failed and newpgrp is NULL"));
	if (newpgrp != NULL)
		FREE(newpgrp, M_PGRP);
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
setuid(struct thread *td, struct setuid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t uid;
	struct uidinfo *uip;
	int error;

	uid = uap->uid;
	newcred = crget();
	uip = uifind(uid);
	PROC_LOCK(p);
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
	    (error = suser_cred(oldcred, SUSER_ALLOWJAIL)) != 0) {
		PROC_UNLOCK(p);
		uifree(uip);
		crfree(newcred);
		return (error);
	}

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	crcopy(newcred, oldcred);
#ifdef _POSIX_SAVED_IDS
	/*
	 * Do we have "appropriate privileges" (are we root or uid == euid)
	 * If so, we are changing the real uid and/or saved uid.
	 */
	if (
#ifdef POSIX_APPENDIX_B_4_2_2	/* Use the clause from B.4.2.2 */
	    uid == oldcred->cr_uid ||
#endif
	    suser_cred(oldcred, SUSER_ALLOWJAIL) == 0) /* we are using privs */
#endif
	{
		/*
		 * Set the real uid and transfer proc count to new user.
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
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	uifree(uip);
	crfree(oldcred);
	return (0);
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
seteuid(struct thread *td, struct seteuid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t euid;
	struct uidinfo *euip;
	int error;

	euid = uap->euid;
	newcred = crget();
	euip = uifind(euid);
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	if (euid != oldcred->cr_ruid &&		/* allow seteuid(getuid()) */
	    euid != oldcred->cr_svuid &&	/* allow seteuid(saved uid) */
	    (error = suser_cred(oldcred, SUSER_ALLOWJAIL)) != 0) {
		PROC_UNLOCK(p);
		uifree(euip);
		crfree(newcred);
		return (error);
	}
	/*
	 * Everything's okay, do it.  Copy credentials so other references do
	 * not see our changes.
	 */
	crcopy(newcred, oldcred);
	if (oldcred->cr_uid != euid) {
		change_euid(newcred, euip);
		setsugid(p);
	}
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	uifree(euip);
	crfree(oldcred);
	return (0);
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
setgid(struct thread *td, struct setgid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t gid;
	int error;

	gid = uap->gid;
	newcred = crget();
	PROC_LOCK(p);
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
	    (error = suser_cred(oldcred, SUSER_ALLOWJAIL)) != 0) {
		PROC_UNLOCK(p);
		crfree(newcred);
		return (error);
	}

	crcopy(newcred, oldcred);
#ifdef _POSIX_SAVED_IDS
	/*
	 * Do we have "appropriate privileges" (are we root or gid == egid)
	 * If so, we are changing the real uid and saved gid.
	 */
	if (
#ifdef POSIX_APPENDIX_B_4_2_2	/* use the clause from B.4.2.2 */
	    gid == oldcred->cr_groups[0] ||
#endif
	    suser_cred(oldcred, SUSER_ALLOWJAIL) == 0) /* we are using privs */
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
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);
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
setegid(struct thread *td, struct setegid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t egid;
	int error;

	egid = uap->egid;
	newcred = crget();
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	if (egid != oldcred->cr_rgid &&		/* allow setegid(getgid()) */
	    egid != oldcred->cr_svgid &&	/* allow setegid(saved gid) */
	    (error = suser_cred(oldcred, SUSER_ALLOWJAIL)) != 0) {
		PROC_UNLOCK(p);
		crfree(newcred);
		return (error);
	}
	crcopy(newcred, oldcred);
	if (oldcred->cr_groups[0] != egid) {
		change_egid(newcred, egid);
		setsugid(p);
	}
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	crfree(oldcred);
	return (0);
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
setgroups(struct thread *td, struct setgroups_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *tempcred, *oldcred;
	u_int ngrp;
	int error;

	ngrp = uap->gidsetsize;
	if (ngrp > NGROUPS)
		return (EINVAL);
	tempcred = crget();
	error = copyin(uap->gidset, tempcred->cr_groups, ngrp * sizeof(gid_t));
	if (error != 0) {
		crfree(tempcred);
		return (error);
	}
	newcred = crget();
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	error = suser_cred(oldcred, SUSER_ALLOWJAIL);
	if (error) {
		PROC_UNLOCK(p);
		crfree(newcred);
		crfree(tempcred);
		return (error);
	}
		
	/*
	 * XXX A little bit lazy here.  We could test if anything has
	 * changed before crcopy() and setting P_SUGID.
	 */
	crcopy(newcred, oldcred);
	if (ngrp < 1) {
		/*
		 * setgroups(0, NULL) is a legitimate way of clearing the
		 * groups vector on non-BSD systems (which generally do not
		 * have the egid in the groups[0]).  We risk security holes
		 * when running non-BSD software if we do not do the same.
		 */
		newcred->cr_ngroups = 1;
	} else {
		bcopy(tempcred->cr_groups, newcred->cr_groups,
		    ngrp * sizeof(gid_t));
		newcred->cr_ngroups = ngrp;
	}
	setsugid(p);
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	crfree(tempcred);
	crfree(oldcred);
	return (0);
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
setreuid(register struct thread *td, struct setreuid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t euid, ruid;
	struct uidinfo *euip, *ruip;
	int error;

	euid = uap->euid;
	ruid = uap->ruid;
	newcred = crget();
	euip = uifind(euid);
	ruip = uifind(ruid);
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	if (((ruid != (uid_t)-1 && ruid != oldcred->cr_ruid &&
	      ruid != oldcred->cr_svuid) ||
	     (euid != (uid_t)-1 && euid != oldcred->cr_uid &&
	      euid != oldcred->cr_ruid && euid != oldcred->cr_svuid)) &&
	    (error = suser_cred(oldcred, SUSER_ALLOWJAIL)) != 0) {
		PROC_UNLOCK(p);
		uifree(ruip);
		uifree(euip);
		crfree(newcred);
		return (error);
	}
	crcopy(newcred, oldcred);
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
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	uifree(ruip);
	uifree(euip);
	crfree(oldcred);
	return (0);
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
setregid(register struct thread *td, struct setregid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t egid, rgid;
	int error;

	egid = uap->egid;
	rgid = uap->rgid;
	newcred = crget();
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	if (((rgid != (gid_t)-1 && rgid != oldcred->cr_rgid &&
	    rgid != oldcred->cr_svgid) ||
	     (egid != (gid_t)-1 && egid != oldcred->cr_groups[0] &&
	     egid != oldcred->cr_rgid && egid != oldcred->cr_svgid)) &&
	    (error = suser_cred(oldcred, SUSER_ALLOWJAIL)) != 0) {
		PROC_UNLOCK(p);
		crfree(newcred);
		return (error);
	}

	crcopy(newcred, oldcred);
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
	PROC_UNLOCK(p);
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setresuid(register struct thread *td, struct setresuid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	uid_t euid, ruid, suid;
	struct uidinfo *euip, *ruip;
	int error;

	euid = uap->euid;
	ruid = uap->ruid;
	suid = uap->suid;
	newcred = crget();
	euip = uifind(euid);
	ruip = uifind(ruid);
	PROC_LOCK(p);
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
	    (error = suser_cred(oldcred, SUSER_ALLOWJAIL)) != 0) {
		PROC_UNLOCK(p);
		uifree(ruip);
		uifree(euip);
		crfree(newcred);
		return (error);
	}

	crcopy(newcred, oldcred);
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
	p->p_ucred = newcred;
	PROC_UNLOCK(p);
	uifree(ruip);
	uifree(euip);
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
setresgid(register struct thread *td, struct setresgid_args *uap)
{
	struct proc *p = td->td_proc;
	struct ucred *newcred, *oldcred;
	gid_t egid, rgid, sgid;
	int error;

	egid = uap->egid;
	rgid = uap->rgid;
	sgid = uap->sgid;
	newcred = crget();
	PROC_LOCK(p);
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
	    (error = suser_cred(oldcred, SUSER_ALLOWJAIL)) != 0) {
		PROC_UNLOCK(p);
		crfree(newcred);
		return (error);
	}

	crcopy(newcred, oldcred);
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
	PROC_UNLOCK(p);
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getresuid(register struct thread *td, struct getresuid_args *uap)
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
getresgid(register struct thread *td, struct getresgid_args *uap)
{
	struct ucred *cred;
	int error1 = 0, error2 = 0, error3 = 0;

	cred = td->td_ucred;
	if (uap->rgid)
		error1 = copyout(&cred->cr_rgid,
		    uap->rgid, sizeof(cred->cr_rgid));
	if (uap->egid)
		error2 = copyout(&cred->cr_groups[0],
		    uap->egid, sizeof(cred->cr_groups[0]));
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
/*
 * MPSAFE
 */
/* ARGSUSED */
int
issetugid(register struct thread *td, struct issetugid_args *uap)
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
	PROC_LOCK(p);
	td->td_retval[0] = (p->p_flag & P_SUGID) ? 1 : 0;
	PROC_UNLOCK(p);
	return (0);
}

/*
 * MPSAFE
 */
int
__setugid(struct thread *td, struct __setugid_args *uap)
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

/*
 * Check if gid is a member of the group set.
 *
 * MPSAFE (cred must be held)
 */
int
groupmember(gid_t gid, struct ucred *cred)
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
suser_cred(struct ucred *cred, int flag)
{

	if (!suser_enabled)
		return (EPERM);
	if (((flag & SUSER_RUID) ? cred->cr_ruid : cred->cr_uid) != 0)
		return (EPERM);
	if (jailed(cred) && !(flag & SUSER_ALLOWJAIL))
		return (EPERM);
	return (0);
}

/*
 * Shortcut to hide contents of struct td and struct proc from the
 * caller, promoting binary compatibility.
 */
int
suser(struct thread *td)
{

#ifdef INVARIANTS
	if (td != curthread) {
		printf("suser: thread %p (%d %s) != curthread %p (%d %s)\n",
		    td, td->td_proc->p_pid, td->td_proc->p_comm,
		    curthread, curthread->td_proc->p_pid,
		    curthread->td_proc->p_comm);
#ifdef KDB
		kdb_backtrace();
#endif
	}
#endif
	return (suser_cred(td->td_ucred, 0));
}

/*
 * Test the active securelevel against a given level.  securelevel_gt()
 * implements (securelevel > level).  securelevel_ge() implements
 * (securelevel >= level).  Note that the logic is inverted -- these
 * functions return EPERM on "success" and 0 on "failure".
 *
 * MPSAFE
 */
int
securelevel_gt(struct ucred *cr, int level)
{
	int active_securelevel;

	active_securelevel = securelevel;
	KASSERT(cr != NULL, ("securelevel_gt: null cr"));
	if (cr->cr_prison != NULL)
		active_securelevel = imax(cr->cr_prison->pr_securelevel,
		    active_securelevel);
	return (active_securelevel > level ? EPERM : 0);
}

int
securelevel_ge(struct ucred *cr, int level)
{
	int active_securelevel;

	active_securelevel = securelevel;
	KASSERT(cr != NULL, ("securelevel_ge: null cr"));
	if (cr->cr_prison != NULL)
		active_securelevel = imax(cr->cr_prison->pr_securelevel,
		    active_securelevel);
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
 * Determine if u1 "can see" the subject specified by u2, according to the
 * 'see_other_uids' policy.
 * Returns: 0 for permitted, ESRCH otherwise
 * Locks: none
 * References: *u1 and *u2 must not change during the call
 *             u1 may equal u2, in which case only one reference is required
 */
static int
cr_seeotheruids(struct ucred *u1, struct ucred *u2)
{

	if (!see_other_uids && u1->cr_ruid != u2->cr_ruid) {
		if (suser_cred(u1, SUSER_ALLOWJAIL) != 0)
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
cr_seeothergids(struct ucred *u1, struct ucred *u2)
{
	int i, match;
	
	if (!see_other_gids) {
		match = 0;
		for (i = 0; i < u1->cr_ngroups; i++) {
			if (groupmember(u1->cr_groups[i], u2))
				match = 1;
			if (match)
				break;
		}
		if (!match) {
			if (suser_cred(u1, SUSER_ALLOWJAIL) != 0)
				return (ESRCH);
		}
	}
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
	if ((error = mac_check_cred_visible(u1, u2)))
		return (error);
#endif
	if ((error = cr_seeotheruids(u1, u2)))
		return (error);
	if ((error = cr_seeothergids(u1, u2)))
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
	if ((error = mac_check_proc_signal(cred, proc, signum)))
		return (error);
#endif
	if ((error = cr_seeotheruids(cred, proc->p_ucred)))
		return (error);
	if ((error = cr_seeothergids(cred, proc->p_ucred)))
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
		case SIGTHR:
			/*
			 * Generally, permit job and terminal control
			 * signals.
			 */
			break;
		default:
			/* Not permitted without privilege. */
			error = suser_cred(cred, SUSER_ALLOWJAIL);
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
		error = suser_cred(cred, SUSER_ALLOWJAIL);
		if (error)
			return (error);
	}

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
	if ((error = mac_check_proc_sched(td->td_ucred, p)))
		return (error);
#endif
	if ((error = cr_seeotheruids(td->td_ucred, p->p_ucred)))
		return (error);
	if ((error = cr_seeothergids(td->td_ucred, p->p_ucred)))
		return (error);
	if (td->td_ucred->cr_ruid == p->p_ucred->cr_ruid)
		return (0);
	if (td->td_ucred->cr_uid == p->p_ucred->cr_ruid)
		return (0);
	if (suser_cred(td->td_ucred, SUSER_ALLOWJAIL) == 0)
		return (0);

#ifdef CAPABILITIES
	if (!cap_check(NULL, td, CAP_SYS_NICE, SUSER_ALLOWJAIL))
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
	int credentialchanged, error, grpsubset, i, uidsubset;

	KASSERT(td == curthread, ("%s: td not curthread", __func__));
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (!unprivileged_proc_debug) {
		error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
		if (error)
			return (error);
	}
	if (td->td_proc == p)
		return (0);
	if ((error = prison_check(td->td_ucred, p->p_ucred)))
		return (error);
#ifdef MAC
	if ((error = mac_check_proc_debug(td->td_ucred, p)))
		return (error);
#endif
	if ((error = cr_seeotheruids(td->td_ucred, p->p_ucred)))
		return (error);
	if ((error = cr_seeothergids(td->td_ucred, p->p_ucred)))
		return (error);

	/*
	 * Is p's group set a subset of td's effective group set?  This
	 * includes p's egid, group access list, rgid, and svgid.
	 */
	grpsubset = 1;
	for (i = 0; i < p->p_ucred->cr_ngroups; i++) {
		if (!groupmember(p->p_ucred->cr_groups[i], td->td_ucred)) {
			grpsubset = 0;
			break;
		}
	}
	grpsubset = grpsubset &&
	    groupmember(p->p_ucred->cr_rgid, td->td_ucred) &&
	    groupmember(p->p_ucred->cr_svgid, td->td_ucred);

	/*
	 * Are the uids present in p's credential equal to td's
	 * effective uid?  This includes p's euid, svuid, and ruid.
	 */
	uidsubset = (td->td_ucred->cr_uid == p->p_ucred->cr_uid &&
	    td->td_ucred->cr_uid == p->p_ucred->cr_svuid &&
	    td->td_ucred->cr_uid == p->p_ucred->cr_ruid);

	/*
	 * Has the credential of the process changed since the last exec()?
	 */
	credentialchanged = (p->p_flag & P_SUGID);

	/*
	 * If p's gids aren't a subset, or the uids aren't a subset,
	 * or the credential has changed, require appropriate privilege
	 * for td to debug p.  For POSIX.1e capabilities, this will
	 * require CAP_SYS_PTRACE.
	 */
	if (!grpsubset || !uidsubset || credentialchanged) {
		error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
		if (error)
			return (error);
	}

	/* Can't trace init when securelevel > 0. */
	if (p == initproc) {
		error = securelevel_gt(td->td_ucred, 0);
		if (error)
			return (error);
	}

	/*
	 * Can't trace a process that's currently exec'ing.
	 * XXX: Note, this is not a security policy decision, it's a
	 * basic correctness/functionality decision.  Therefore, this check
	 * should be moved to the caller's of p_candebug().
	 */
	if ((p->p_flag & P_INEXEC) != 0)
		return (EAGAIN);

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
	SOCK_LOCK(so);
	error = mac_check_socket_visible(cred, so);
	SOCK_UNLOCK(so);
	if (error)
		return (error);
#endif
	if (cr_seeotheruids(cred, so->so_cred))
		return (ENOENT);
	if (cr_seeothergids(cred, so->so_cred))
		return (ENOENT);

	return (0);
}

/*
 * Allocate a zeroed cred structure.
 * MPSAFE
 */
struct ucred *
crget(void)
{
	register struct ucred *cr;

	MALLOC(cr, struct ucred *, sizeof(*cr), M_CRED, M_WAITOK | M_ZERO);
	cr->cr_ref = 1;
	cr->cr_mtxp = mtx_pool_find(mtxpool_sleep, cr);
#ifdef MAC
	mac_init_cred(cr);
#endif
	return (cr);
}

/*
 * Claim another reference to a ucred structure.
 * MPSAFE
 */
struct ucred *
crhold(struct ucred *cr)
{

	mtx_lock(cr->cr_mtxp);
	cr->cr_ref++;
	mtx_unlock(cr->cr_mtxp);
	return (cr);
}

/*
 * Free a cred structure.
 * Throws away space when ref count gets to 0.
 * MPSAFE
 */
void
crfree(struct ucred *cr)
{
	struct mtx *mtxp = cr->cr_mtxp;

	mtx_lock(mtxp);
	KASSERT(cr->cr_ref > 0, ("bad ucred refcount: %d", cr->cr_ref));
	if (--cr->cr_ref == 0) {
		mtx_unlock(mtxp);
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
#ifdef MAC
		mac_destroy_cred(cr);
#endif
		FREE(cr, M_CRED);
	} else {
		mtx_unlock(mtxp);
	}
}

/*
 * Check to see if this ucred is shared.
 * MPSAFE
 */
int
crshared(struct ucred *cr)
{
	int shared;

	mtx_lock(cr->cr_mtxp);
	shared = (cr->cr_ref > 1);
	mtx_unlock(cr->cr_mtxp);
	return (shared);
}

/*
 * Copy a ucred's contents from a template.  Does not block.
 * MPSAFE
 */
void
crcopy(struct ucred *dest, struct ucred *src)
{

	KASSERT(crshared(dest) == 0, ("crcopy of shared ucred"));
	bcopy(&src->cr_startcopy, &dest->cr_startcopy,
	    (unsigned)((caddr_t)&src->cr_endcopy -
		(caddr_t)&src->cr_startcopy));
	uihold(dest->cr_uidinfo);
	uihold(dest->cr_ruidinfo);
	if (jailed(dest))
		prison_hold(dest->cr_prison);
#ifdef MAC
	mac_copy_cred(src, dest);
#endif
}

/*
 * Dup cred struct to a new held one.
 * MPSAFE
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
 * MPSAFE
 */
void
cru2x(struct ucred *cr, struct xucred *xcr)
{

	bzero(xcr, sizeof(*xcr));
	xcr->cr_version = XUCRED_VERSION;
	xcr->cr_uid = cr->cr_uid;
	xcr->cr_ngroups = cr->cr_ngroups;
	bcopy(cr->cr_groups, xcr->cr_groups, sizeof(cr->cr_groups));
}

/*
 * small routine to swap a thread's current ucred for the correct one
 * taken from the process.
 * MPSAFE
 */
void
cred_update_thread(struct thread *td)
{
	struct proc *p;
	struct ucred *cred;

	p = td->td_proc;
	cred = td->td_ucred;
	PROC_LOCK(p);
	td->td_ucred = crhold(p->p_ucred);
	PROC_UNLOCK(p);
	if (cred != NULL)
		crfree(cred);
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
getlogin(struct thread *td, struct getlogin_args *uap)
{
	int error;
	char login[MAXLOGNAME];
	struct proc *p = td->td_proc;

	if (uap->namelen > MAXLOGNAME)
		uap->namelen = MAXLOGNAME;
	PROC_LOCK(p);
	SESS_LOCK(p->p_session);
	bcopy(p->p_session->s_login, login, uap->namelen);
	SESS_UNLOCK(p->p_session);
	PROC_UNLOCK(p);
	error = copyout(login, uap->namebuf, uap->namelen);
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
setlogin(struct thread *td, struct setlogin_args *uap)
{
	struct proc *p = td->td_proc;
	int error;
	char logintmp[MAXLOGNAME];

	error = suser_cred(td->td_ucred, SUSER_ALLOWJAIL);
	if (error)
		return (error);
	error = copyinstr(uap->namebuf, logintmp, sizeof(logintmp), NULL);
	if (error == ENAMETOOLONG)
		error = EINVAL;
	else if (!error) {
		PROC_LOCK(p);
		SESS_LOCK(p->p_session);
		(void) memcpy(p->p_session->s_login, logintmp,
		    sizeof(logintmp));
		SESS_UNLOCK(p->p_session);
		PROC_UNLOCK(p);
	}
	return (error);
}

void
setsugid(struct proc *p)
{

	PROC_LOCK_ASSERT(p, MA_OWNED);
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
change_ruid(struct ucred *newcred, struct uidinfo *ruip)
{

	(void)chgproccnt(newcred->cr_ruidinfo, -1, 0);
	newcred->cr_ruid = ruip->ui_uid;
	uihold(ruip);
	uifree(newcred->cr_ruidinfo);
	newcred->cr_ruidinfo = ruip;
	(void)chgproccnt(newcred->cr_ruidinfo, 1, 0);
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
