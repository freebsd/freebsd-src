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
 * $Id: kern_prot.c,v 1.34 1997/08/19 06:00:27 peter Exp $
 */

/*
 * System calls related to processes and protection
 */

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/unistd.h>

static MALLOC_DEFINE(M_CRED, "cred", "credentials");

#ifndef _SYS_SYSPROTO_H_
struct getpid_args {
	int	dummy;
};
#endif

/* ARGSUSED */
int
getpid(p, uap, retval)
	struct proc *p;
	struct getpid_args *uap;
	int *retval;
{

	*retval = p->p_pid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	retval[1] = p->p_pptr->p_pid;
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
getppid(p, uap, retval)
	struct proc *p;
	struct getppid_args *uap;
	int *retval;
{

	*retval = p->p_pptr->p_pid;
	return (0);
}

/* Get process group ID; note that POSIX getpgrp takes no parameter */
#ifndef _SYS_SYSPROTO_H_
struct getpgrp_args {
        int     dummy;
};
#endif

int
getpgrp(p, uap, retval)
	struct proc *p;
	struct getpgrp_args *uap;
	int *retval;
{

	*retval = p->p_pgrp->pg_id;
	return (0);
}

/* Get an arbitary pid's process group id */
#ifndef _SYS_SYSPROTO_H_
struct getpgid_args {
	pid_t	pid;
};
#endif

int
getpgid(p, uap, retval)
	struct proc *p;
	struct getpgid_args *uap;
	int *retval;
{
	if (uap->pid == 0)
		goto found;

	if ((p == pfind(uap->pid)) == 0)
		return ESRCH;
found:
	*retval = p->p_pgrp->pg_id;
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
getsid(p, uap, retval)
	struct proc *p;
	struct getsid_args *uap;
	int *retval;
{
	if (uap->pid == 0)
		goto found;

	if ((p == pfind(uap->pid)) == 0)
		return ESRCH;
found:
	*retval = p->p_pgrp->pg_session->s_leader->p_pid;
	return 0;
}


#ifndef _SYS_SYSPROTO_H_
struct getuid_args {
        int     dummy;
};
#endif

/* ARGSUSED */
int
getuid(p, uap, retval)
	struct proc *p;
	struct getuid_args *uap;
	int *retval;
{

	*retval = p->p_cred->p_ruid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	retval[1] = p->p_ucred->cr_uid;
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
geteuid(p, uap, retval)
	struct proc *p;
	struct geteuid_args *uap;
	int *retval;
{

	*retval = p->p_ucred->cr_uid;
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getgid_args {
        int     dummy;
};
#endif

/* ARGSUSED */
int
getgid(p, uap, retval)
	struct proc *p;
	struct getgid_args *uap;
	int *retval;
{

	*retval = p->p_cred->p_rgid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	retval[1] = p->p_ucred->cr_groups[0];
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
getegid(p, uap, retval)
	struct proc *p;
	struct getegid_args *uap;
	int *retval;
{

	*retval = p->p_ucred->cr_groups[0];
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct getgroups_args {
	u_int	gidsetsize;
	gid_t	*gidset;
};
#endif
int
getgroups(p, uap, retval)
	struct proc *p;
	register struct	getgroups_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register u_int ngrp;
	int error;

	if ((ngrp = uap->gidsetsize) == 0) {
		*retval = pc->pc_ucred->cr_ngroups;
		return (0);
	}
	if (ngrp < pc->pc_ucred->cr_ngroups)
		return (EINVAL);
	ngrp = pc->pc_ucred->cr_ngroups;
	if ((error = copyout((caddr_t)pc->pc_ucred->cr_groups,
	    (caddr_t)uap->gidset, ngrp * sizeof(gid_t))))
		return (error);
	*retval = ngrp;
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setsid_args {
        int     dummy;
};
#endif

/* ARGSUSED */
int
setsid(p, uap, retval)
	register struct proc *p;
	struct setsid_args *uap;
	int *retval;
{

	if (p->p_pgid == p->p_pid || pgfind(p->p_pid)) {
		return (EPERM);
	} else {
		(void)enterpgrp(p, p->p_pid, 1);
		*retval = p->p_pid;
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
setpgid(curp, uap, retval)
	struct proc *curp;
	register struct setpgid_args *uap;
	int *retval;
{
	register struct proc *targp;		/* target process */
	register struct pgrp *pgrp;		/* target pgrp */

	if (uap->pgid < 0)
		return (EINVAL);
	if (uap->pid != 0 && uap->pid != curp->p_pid) {
		if ((targp = pfind(uap->pid)) == 0 || !inferior(targp))
			return (ESRCH);
		if (targp->p_pgrp == NULL ||  targp->p_session != curp->p_session)
			return (EPERM);
		if (targp->p_flag & P_EXEC)
			return (EACCES);
	} else
		targp = curp;
	if (SESS_LEADER(targp))
		return (EPERM);
	if (uap->pgid == 0)
		uap->pgid = targp->p_pid;
	else if (uap->pgid != targp->p_pid)
		if ((pgrp = pgfind(uap->pgid)) == 0 ||
	            pgrp->pg_session != curp->p_session)
			return (EPERM);
	return (enterpgrp(targp, uap->pgid, 0));
}

/*
 * Use the clause in B.4.2.2 that allows setuid/setgid to be 4.2/4.3BSD
 * compatable.  It says that setting the uid/gid to euid/egid is a special
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
setuid(p, uap, retval)
	struct proc *p;
	struct setuid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register uid_t uid;
	int error;

	/*
	 * See if we have "permission" by POSIX 1003.1 rules.
	 *
	 * Note that setuid(geteuid()) is a special case of 
	 * "appropriate privileges" in appendix B.4.2.2.  We need
	 * to use this clause to be compatable with traditional BSD
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
	uid = uap->uid;
	if (uid != pc->p_ruid &&		/* allow setuid(getuid()) */
#ifdef _POSIX_SAVED_IDS
	    uid != pc->p_svuid &&		/* allow setuid(saved gid) */
#endif
#ifdef POSIX_APPENDIX_B_4_2_2	/* Use BSD-compat clause from B.4.2.2 */
	    uid != pc->pc_ucred->cr_uid &&	/* allow setuid(geteuid()) */
#endif
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

#ifdef _POSIX_SAVED_IDS
	/*
	 * Do we have "appropriate privileges" (are we root or uid == euid)
	 * If so, we are changing the real uid and/or saved uid.
	 */
	if (
#ifdef POSIX_APPENDIX_B_4_2_2	/* Use the clause from B.4.2.2 */
	    uid == pc->pc_ucred->cr_uid ||
#endif
	    suser(pc->pc_ucred, &p->p_acflag) == 0) /* we are using privs */
#endif
	{
		/*
		 * Transfer proc count to new user.
		 */
		if (uid != pc->p_ruid) {
			(void)chgproccnt(pc->p_ruid, -1);
			(void)chgproccnt(uid, 1);
		}
		/*
		 * Set real uid
		 */
		if (uid != pc->p_ruid) {
			p->p_flag |= P_SUGID;
			pc->p_ruid = uid;
		}
		/*
		 * Set saved uid
		 *
		 * XXX always set saved uid even if not _POSIX_SAVED_IDS, as
		 * the security of seteuid() depends on it.  B.4.2.2 says it
		 * is important that we should do this.
		 */
		if (pc->p_svuid != uid) {
			p->p_flag |= P_SUGID;
			pc->p_svuid = uid;
		}
	}

	/*
	 * In all permitted cases, we are changing the euid.
	 * Copy credentials so other references do not see our changes.
	 */
	if (pc->pc_ucred->cr_uid != uid) {
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_uid = uid;
		p->p_flag |= P_SUGID;
	}
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct seteuid_args {
	uid_t	euid;
};
#endif
/* ARGSUSED */
int
seteuid(p, uap, retval)
	struct proc *p;
	struct seteuid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register uid_t euid;
	int error;

	euid = uap->euid;
	if (euid != pc->p_ruid &&		/* allow seteuid(getuid()) */
	    euid != pc->p_svuid &&		/* allow seteuid(saved uid) */
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	/*
	 * Everything's okay, do it.  Copy credentials so other references do
	 * not see our changes.
	 */
	if (pc->pc_ucred->cr_uid != euid) {
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_uid = euid;
		p->p_flag |= P_SUGID;
	}
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setgid_args {
	gid_t	gid;
};
#endif
/* ARGSUSED */
int
setgid(p, uap, retval)
	struct proc *p;
	struct setgid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register gid_t gid;
	int error;

	/*
	 * See if we have "permission" by POSIX 1003.1 rules.
	 *
	 * Note that setgid(getegid()) is a special case of
	 * "appropriate privileges" in appendix B.4.2.2.  We need
	 * to use this clause to be compatable with traditional BSD
	 * semantics.  Basically, it means that "setgid(xx)" sets all
	 * three id's (assuming you have privs).
	 *
	 * For notes on the logic here, see setuid() above.
	 */
	gid = uap->gid;
	if (gid != pc->p_rgid &&		/* allow setgid(getgid()) */
#ifdef _POSIX_SAVED_IDS
	    gid != pc->p_svgid &&		/* allow setgid(saved gid) */
#endif
#ifdef POSIX_APPENDIX_B_4_2_2	/* Use BSD-compat clause from B.4.2.2 */
	    gid != pc->pc_ucred->cr_groups[0] && /* allow setgid(getegid()) */
#endif
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

#ifdef _POSIX_SAVED_IDS
	/*
	 * Do we have "appropriate privileges" (are we root or gid == egid)
	 * If so, we are changing the real uid and saved gid.
	 */
	if (
#ifdef POSIX_APPENDIX_B_4_2_2	/* use the clause from B.4.2.2 */
	    gid == pc->pc_ucred->cr_groups[0] ||
#endif
	    suser(pc->pc_ucred, &p->p_acflag) == 0) /* we are using privs */
#endif
	{
		/*
		 * Set real gid
		 */
		if (pc->p_rgid != gid) {
			p->p_flag |= P_SUGID;
			pc->p_rgid = gid;
		}
		/*
		 * Set saved gid
		 *
		 * XXX always set saved gid even if not _POSIX_SAVED_IDS, as
		 * the security of setegid() depends on it.  B.4.2.2 says it
		 * is important that we should do this.
		 */
		if (pc->p_svgid != gid) {
			p->p_flag |= P_SUGID;
			pc->p_svgid = gid;
		}
	}
	/*
	 * In all cases permitted cases, we are changing the egid.
	 * Copy credentials so other references do not see our changes.
	 */
	if (pc->pc_ucred->cr_groups[0] != gid) {
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_groups[0] = gid;
		p->p_flag |= P_SUGID;
	}
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct setegid_args {
	gid_t	egid;
};
#endif
/* ARGSUSED */
int
setegid(p, uap, retval)
	struct proc *p;
	struct setegid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register gid_t egid;
	int error;

	egid = uap->egid;
	if (egid != pc->p_rgid &&		/* allow setegid(getgid()) */
	    egid != pc->p_svgid &&		/* allow setegid(saved gid) */
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	if (pc->pc_ucred->cr_groups[0] != egid) {
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_groups[0] = egid;
		p->p_flag |= P_SUGID;
	}
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
setgroups(p, uap, retval)
	struct proc *p;
	struct setgroups_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register u_int ngrp;
	int error;

	if ((error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);
	ngrp = uap->gidsetsize;
	if (ngrp > NGROUPS)
		return (EINVAL);
	/*
	 * XXX A little bit lazy here.  We could test if anything has
	 * changed before crcopy() and setting P_SUGID.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	if (ngrp < 1) {
		/*
		 * setgroups(0, NULL) is a legitimate way of clearing the
		 * groups vector on non-BSD systems (which generally do not
		 * have the egid in the groups[0]).  We risk security holes
		 * when running non-BSD software if we do not do the same.
		 */
		pc->pc_ucred->cr_ngroups = 1;
	} else {
		if ((error = copyin((caddr_t)uap->gidset,
		    (caddr_t)pc->pc_ucred->cr_groups, ngrp * sizeof(gid_t))))
			return (error);
		pc->pc_ucred->cr_ngroups = ngrp;
	}
	p->p_flag |= P_SUGID;
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
setreuid(p, uap, retval)
	register struct proc *p;
	struct setreuid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register uid_t ruid, euid;
	int error;

	ruid = uap->ruid;
	euid = uap->euid;
	if ((ruid != (uid_t)-1 && ruid != pc->p_ruid && ruid != pc->p_svuid ||
	     euid != (uid_t)-1 && euid != pc->pc_ucred->cr_uid &&
	     euid != pc->p_ruid && euid != pc->p_svuid) &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (euid != (uid_t)-1 && pc->pc_ucred->cr_uid != euid) {
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_uid = euid;
		p->p_flag |= P_SUGID;
	}
	if (ruid != (uid_t)-1 && pc->p_ruid != ruid) {
		(void)chgproccnt(pc->p_ruid, -1);
		(void)chgproccnt(ruid, 1);
		pc->p_ruid = ruid;
		p->p_flag |= P_SUGID;
	}
	if ((ruid != (uid_t)-1 || pc->pc_ucred->cr_uid != pc->p_ruid) &&
	    pc->p_svuid != pc->pc_ucred->cr_uid) {
		pc->p_svuid = pc->pc_ucred->cr_uid;
		p->p_flag |= P_SUGID;
	}
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
setregid(p, uap, retval)
	register struct proc *p;
	struct setregid_args *uap;
	int *retval;
{
	register struct pcred *pc = p->p_cred;
	register gid_t rgid, egid;
	int error;

	rgid = uap->rgid;
	egid = uap->egid;
	if ((rgid != (gid_t)-1 && rgid != pc->p_rgid && rgid != pc->p_svgid ||
	     egid != (gid_t)-1 && egid != pc->pc_ucred->cr_groups[0] &&
	     egid != pc->p_rgid && egid != pc->p_svgid) &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (egid != (gid_t)-1 && pc->pc_ucred->cr_groups[0] != egid) {
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_groups[0] = egid;
		p->p_flag |= P_SUGID;
	}
	if (rgid != (gid_t)-1 && pc->p_rgid != rgid) {
		pc->p_rgid = rgid;
		p->p_flag |= P_SUGID;
	}
	if ((rgid != (gid_t)-1 || pc->pc_ucred->cr_groups[0] != pc->p_rgid) &&
	    pc->p_svgid != pc->pc_ucred->cr_groups[0]) {
		pc->p_svgid = pc->pc_ucred->cr_groups[0];
		p->p_flag |= P_SUGID;
	}
	return (0);
}

#ifndef _SYS_SYSPROTO_H_
struct issetugid_args {
	int dummy;
};
#endif
/* ARGSUSED */
int
issetugid(p, uap, retval)
	register struct proc *p;
	struct issetugid_args *uap;
	int *retval;
{
	/*
	 * Note: OpenBSD sets a P_SUGIDEXEC flag set at execve() time,
	 * we use P_SUGID because we consider changing the owners as
	 * "tainting" as well.
	 * This is significant for procs that start as root and "become"
	 * a user without an exec - programs cannot know *everything*
	 * that libc *might* have put in their data segment.
	 */
	if (p->p_flag & P_SUGID)
		return (1);
	return (0);
}

/*
 * Check if gid is a member of the group set.
 */
int
groupmember(gid, cred)
	gid_t gid;
	register struct ucred *cred;
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
 * Test whether the specified credentials imply "super-user"
 * privilege; if so, and we have accounting info, set the flag
 * indicating use of super-powers.
 * Returns 0 or error.
 */
int
suser(cred, acflag)
	struct ucred *cred;
	u_short *acflag;
{
	if (cred->cr_uid == 0) {
		if (acflag)
			*acflag |= ASU;
		return (0);
	}
	return (EPERM);
}

/*
 * Allocate a zeroed cred structure.
 */
struct ucred *
crget()
{
	register struct ucred *cr;

	MALLOC(cr, struct ucred *, sizeof(*cr), M_CRED, M_WAITOK);
	bzero((caddr_t)cr, sizeof(*cr));
	cr->cr_ref = 1;
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
	int s;

	s = splimp();				/* ??? */
	if (--cr->cr_ref == 0)
		FREE((caddr_t)cr, M_CRED);
	(void) splx(s);
}

/*
 * Copy cred structure to a new one and free the old one.
 */
struct ucred *
crcopy(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	if (cr->cr_ref == 1)
		return (cr);
	newcr = crget();
	*newcr = *cr;
	crfree(cr);
	newcr->cr_ref = 1;
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

	newcr = crget();
	*newcr = *cr;
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
getlogin(p, uap, retval)
	struct proc *p;
	struct getlogin_args *uap;
	int *retval;
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
setlogin(p, uap, retval)
	struct proc *p;
	struct setlogin_args *uap;
	int *retval;
{
	int error;
	char logintmp[MAXLOGNAME];

	if ((error = suser(p->p_ucred, &p->p_acflag)))
		return (error);
	error = copyinstr((caddr_t) uap->namebuf, (caddr_t) logintmp,
	    sizeof(logintmp), (u_int *)0);
	if (error == ENAMETOOLONG)
		error = EINVAL;
	else if (!error)
		(void) memcpy(p->p_pgrp->pg_session->s_login, logintmp,
		    sizeof(logintmp));
	return (error);
}
