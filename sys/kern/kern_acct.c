/*-
 * Copyright (c) 1994 Christopher G. Demetriou
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)kern_acct.c	8.1 (Berkeley) 6/14/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/namei.h>
#include <sys/acct.h>
#include <sys/resourcevar.h>
#include <sys/tty.h>

/*
 * The routines implemented in this file are described in:
 *      Leffler, et al.: The Design and Implementation of the 4.3BSD
 *	    UNIX Operating System (Addison Welley, 1989)
 * on pages 62-63.
 *
 * Arguably, to simplify accounting operations, this mechanism should
 * be replaced by one in which an accounting log file (similar to /dev/klog)
 * is read by a user process, etc.  However, that has its own problems.
 */

/*
 * Internal accounting functions.
 * The former's operation is described in Leffler, et al., and the latter
 * was provided by UCB with the 4.4BSD-Lite release
 */
static comp_t	encode_comp_t(u_long, u_long);
static void	acctwatch(void *);

/*
 * Accounting callout used for periodic scheduling of acctwatch.
 */
static struct	callout acctwatch_callout;

/*
 * Accounting vnode pointer, saved vnode pointer, and flags for each.
 */
static struct	vnode *acctp;
static struct	ucred *acctcred;
static int	acctflags;
static struct	vnode *savacctp;
static struct	ucred *savacctcred;
static int	savacctflags;

static struct mtx	acct_mtx;
MTX_SYSINIT(acct, &acct_mtx, "accounting", MTX_DEF);

/*
 * Values associated with enabling and disabling accounting
 */
static int acctsuspend = 2;	/* stop accounting when < 2% free space left */
SYSCTL_INT(_kern, OID_AUTO, acct_suspend, CTLFLAG_RW,
	&acctsuspend, 0, "percentage of free disk space below which accounting stops");

static int acctresume = 4;	/* resume when free space risen to > 4% */
SYSCTL_INT(_kern, OID_AUTO, acct_resume, CTLFLAG_RW,
	&acctresume, 0, "percentage of free disk space above which accounting resumes");

static int acctchkfreq = 15;	/* frequency (in seconds) to check space */
SYSCTL_INT(_kern, OID_AUTO, acct_chkfreq, CTLFLAG_RW,
	&acctchkfreq, 0, "frequency for checking the free space");

/*
 * Accounting system call.  Written based on the specification and
 * previous implementation done by Mark Tinguely.
 *
 * MPSAFE
 */
int
acct(td, uap)
	struct thread *td;
	struct acct_args /* {
		char *path;
	} */ *uap;
{
	struct nameidata nd;
	int error, flags;

	/* Make sure that the caller is root. */
	error = suser(td);
	if (error)
		return (error);

	mtx_lock(&Giant);

	/*
	 * If accounting is to be started to a file, open that file for
	 * appending and make sure it's a 'normal'.
	 */
	if (uap->path != NULL) {
		NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_USERSPACE, uap->path, td);
		flags = FWRITE | O_APPEND;
		error = vn_open(&nd, &flags, 0, -1);
		if (error)
			goto done2;
		NDFREE(&nd, NDF_ONLY_PNBUF);
#ifdef MAC
		error = mac_check_system_acct(td->td_ucred, nd.ni_vp);
		if (error) {
			vn_close(nd.ni_vp, flags, td->td_ucred, td);
			goto done2;
		}
#endif
		VOP_UNLOCK(nd.ni_vp, 0, td);
		if (nd.ni_vp->v_type != VREG) {
			vn_close(nd.ni_vp, flags, td->td_ucred, td);
			error = EACCES;
			goto done2;
		}
#ifdef MAC
	} else {
		error = mac_check_system_acct(td->td_ucred, NULL);
		if (error)
			goto done2;
#endif
	}

	mtx_lock(&acct_mtx);

	/*
	 * If accounting was previously enabled, kill the old space-watcher,
	 * close the file, and (if no new file was specified, leave).
	 *
	 * XXX arr: should not hold lock over vnode operation.
	 */
	if (acctp != NULLVP || savacctp != NULLVP) {
		callout_stop(&acctwatch_callout);
		error = vn_close((acctp != NULLVP ? acctp : savacctp),
		    (acctp != NULLVP ? acctflags : savacctflags),
		    (acctcred != NOCRED ? acctcred : savacctcred), td);
		acctp = savacctp = NULLVP;
		crfree(acctcred != NOCRED ? acctcred : savacctcred);
		acctcred = savacctcred = NOCRED;
		log(LOG_NOTICE, "Accounting disabled\n");
	}
	if (uap->path == NULL) {
		mtx_unlock(&acct_mtx);
		goto done2;
	}

	/*
	 * Save the new accounting file vnode, and schedule the new
	 * free space watcher.
	 */
	acctp = nd.ni_vp;
	acctcred = crhold(td->td_ucred);
	acctflags = flags;
	callout_init(&acctwatch_callout, 0);
	mtx_unlock(&acct_mtx);
	log(LOG_NOTICE, "Accounting enabled\n");
	acctwatch(NULL);

done2:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * Write out process accounting information, on process exit.
 * Data to be written out is specified in Leffler, et al.
 * and are enumerated below.  (They're also noted in the system
 * "acct.h" header file.)
 */
int
acct_process(td)
	struct thread *td;
{
	struct acct acct;
	struct timeval ut, st, tmp;
	struct plimit *newlim, *oldlim;
	struct proc *p;
	struct rusage *r;
	struct ucred *uc;
	struct vnode *vp;
	int t, ret;

	/*
	 * Lockless check of accounting condition before doing the hard
	 * work.
	 */
	if (acctp == NULLVP)
		return (0);

	mtx_lock(&acct_mtx);

	/*
	 * If accounting isn't enabled, don't bother.  Have to check again
	 * once we own the lock in case we raced with disabling of accounting
	 * by another thread.
	 */
	vp = acctp;
	if (vp == NULLVP) {
		mtx_unlock(&acct_mtx);
		return (0);
	}

	p = td->td_proc;

	/*
	 * Get process accounting information.
	 */

	PROC_LOCK(p);
	/* (1) The name of the command that ran */
	bcopy(p->p_comm, acct.ac_comm, sizeof acct.ac_comm);

	/* (2) The amount of user and system time that was used */
	mtx_lock_spin(&sched_lock);
	calcru(p, &ut, &st, NULL);
	mtx_unlock_spin(&sched_lock);
	acct.ac_utime = encode_comp_t(ut.tv_sec, ut.tv_usec);
	acct.ac_stime = encode_comp_t(st.tv_sec, st.tv_usec);

	/* (3) The elapsed time the command ran (and its starting time) */
	tmp = boottime;
	timevaladd(&tmp, &p->p_stats->p_start);
	acct.ac_btime = tmp.tv_sec;
	microuptime(&tmp);
	timevalsub(&tmp, &p->p_stats->p_start);
	acct.ac_etime = encode_comp_t(tmp.tv_sec, tmp.tv_usec);

	/* (4) The average amount of memory used */
	r = &p->p_stats->p_ru;
	tmp = ut;
	timevaladd(&tmp, &st);
	t = tmp.tv_sec * hz + tmp.tv_usec / tick;
	if (t)
		acct.ac_mem = (r->ru_ixrss + r->ru_idrss + r->ru_isrss) / t;
	else
		acct.ac_mem = 0;

	/* (5) The number of disk I/O operations done */
	acct.ac_io = encode_comp_t(r->ru_inblock + r->ru_oublock, 0);

	/* (6) The UID and GID of the process */
	acct.ac_uid = p->p_ucred->cr_ruid;
	acct.ac_gid = p->p_ucred->cr_rgid;

	/* (7) The terminal from which the process was started */
	SESS_LOCK(p->p_session);
	if ((p->p_flag & P_CONTROLT) && p->p_pgrp->pg_session->s_ttyp)
		acct.ac_tty = dev2udev(p->p_pgrp->pg_session->s_ttyp->t_dev);
	else
		acct.ac_tty = NODEV;
	SESS_UNLOCK(p->p_session);

	/* (8) The boolean flags that tell how the process terminated, etc. */
	acct.ac_flag = p->p_acflag;
	PROC_UNLOCK(p);

	/*
	 * Finish doing things that require acct_mtx, and release acct_mtx.
	 */
	uc = crhold(acctcred);
	vref(vp);
	mtx_unlock(&acct_mtx);

	/*
	 * Eliminate any file size rlimit.
	 */
	newlim = lim_alloc();
	PROC_LOCK(p);
	oldlim = p->p_limit;
	lim_copy(newlim, oldlim);
	newlim->pl_rlimit[RLIMIT_FSIZE].rlim_cur = RLIM_INFINITY;
	p->p_limit = newlim;
	PROC_UNLOCK(p);
	lim_free(oldlim);

	/*
	 * Write the accounting information to the file.
	 */
	VOP_LEASE(vp, td, uc, LEASE_WRITE);
	ret = vn_rdwr(UIO_WRITE, vp, (caddr_t)&acct, sizeof (acct),
	    (off_t)0, UIO_SYSSPACE, IO_APPEND|IO_UNIT, uc, NOCRED,
	    (int *)0, td);
	vrele(vp);
	crfree(uc);
	return (ret);
}

/*
 * Encode_comp_t converts from ticks in seconds and microseconds
 * to ticks in 1/AHZ seconds.  The encoding is described in
 * Leffler, et al., on page 63.
 */

#define	MANTSIZE	13			/* 13 bit mantissa. */
#define	EXPSIZE		3			/* Base 8 (3 bit) exponent. */
#define	MAXFRACT	((1 << MANTSIZE) - 1)	/* Maximum fractional value. */

static comp_t
encode_comp_t(s, us)
	u_long s, us;
{
	int exp, rnd;

	exp = 0;
	rnd = 0;
	s *= AHZ;
	s += us / (1000000 / AHZ);	/* Maximize precision. */

	while (s > MAXFRACT) {
	rnd = s & (1 << (EXPSIZE - 1));	/* Round up? */
		s >>= EXPSIZE;		/* Base 8 exponent == 3 bit shift. */
		exp++;
	}

	/* If we need to round up, do it (and handle overflow correctly). */
	if (rnd && (++s > MAXFRACT)) {
		s >>= EXPSIZE;
		exp++;
	}

	/* Clean it up and polish it off. */
	exp <<= MANTSIZE;		/* Shift the exponent into place */
	exp += s;			/* and add on the mantissa. */
	return (exp);
}

/*
 * Periodically check the filesystem to see if accounting
 * should be turned on or off.  Beware the case where the vnode
 * has been vgone()'d out from underneath us, e.g. when the file
 * system containing the accounting file has been forcibly unmounted.
 */
/* ARGSUSED */
static void
acctwatch(a)
	void *a;
{
	struct statfs sb;

	mtx_lock(&acct_mtx);

	/*
	 * XXX arr: need to fix the issue of holding acct_mtx over
	 * the below vnode operations.
	 */
	if (savacctp != NULLVP) {
		if (savacctp->v_type == VBAD) {
			(void) vn_close(savacctp, savacctflags, savacctcred,
			    NULL);
			savacctp = NULLVP;
			savacctcred = NOCRED;
			mtx_unlock(&acct_mtx);
			return;
		}
		(void)VFS_STATFS(savacctp->v_mount, &sb, curthread);
		if (sb.f_bavail > acctresume * sb.f_blocks / 100) {
			acctp = savacctp;
			acctcred = savacctcred;
			acctflags = savacctflags;
			savacctp = NULLVP;
			savacctcred = NOCRED;
			log(LOG_NOTICE, "Accounting resumed\n");
		}
	} else {
		if (acctp == NULLVP) {
			mtx_unlock(&acct_mtx);
			return;
		}
		if (acctp->v_type == VBAD) {
			(void) vn_close(acctp, acctflags, acctcred, NULL);
			acctp = NULLVP;
			crfree(acctcred);
			acctcred = NOCRED;
			mtx_unlock(&acct_mtx);
			return;
		}
		(void)VFS_STATFS(acctp->v_mount, &sb, curthread);
		if (sb.f_bavail <= acctsuspend * sb.f_blocks / 100) {
			savacctp = acctp;
			savacctflags = acctflags;
			savacctcred = acctcred;
			acctp = NULLVP;
			acctcred = NOCRED;
			log(LOG_NOTICE, "Accounting suspended\n");
		}
	}
	callout_reset(&acctwatch_callout, acctchkfreq * hz, acctwatch, NULL);
	mtx_unlock(&acct_mtx);
}
