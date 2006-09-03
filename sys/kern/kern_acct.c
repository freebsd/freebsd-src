/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Copyright (c) 1994 Christopher G. Demetriou
 * Copyright (c) 2005 Robert N. M. Watson
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
#include <sys/acct.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/tty.h>
#include <sys/vnode.h>

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
static void	acctwatch(void);
static void	acct_thread(void *);
static int	acct_disable(struct thread *);

/*
 * Accounting vnode pointer, saved vnode pointer, and flags for each.
 * acct_sx protects against changes to the active vnode and credentials
 * while accounting records are being committed to disk.
 */
static int		 acct_suspended;
static struct vnode	*acct_vp;
static struct ucred	*acct_cred;
static int		 acct_flags;
static struct sx	 acct_sx;

SX_SYSINIT(acct, &acct_sx, "acct_sx");

/*
 * State of the accounting kthread.
 */
static int		 acct_state;

#define	ACCT_RUNNING	1	/* Accounting kthread is running. */
#define	ACCT_EXITREQ	2	/* Accounting kthread should exit. */

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

static int
sysctl_acct_chkfreq(SYSCTL_HANDLER_ARGS)
{
	int error, value;

	/* Write out the old value. */
	error = SYSCTL_OUT(req, &acctchkfreq, sizeof(int));
	if (error || req->newptr == NULL)
		return (error);

	/* Read in and verify the new value. */
	error = SYSCTL_IN(req, &value, sizeof(int));
	if (error)
		return (error);
	if (value <= 0)
		return (EINVAL);
	acctchkfreq = value;
	return (0);
}
SYSCTL_PROC(_kern, OID_AUTO, acct_chkfreq, CTLTYPE_INT|CTLFLAG_RW,
    &acctchkfreq, 0, sysctl_acct_chkfreq, "I",
    "frequency for checking the free space");

SYSCTL_INT(_kern, OID_AUTO, acct_suspended, CTLFLAG_RD, &acct_suspended, 0,
	"Accounting suspended or not");

/*
 * Accounting system call.  Written based on the specification and
 * previous implementation done by Mark Tinguely.
 *
 * MPSAFE
 */
int
acct(struct thread *td, struct acct_args *uap)
{
	struct nameidata nd;
	int error, flags;

	/* Make sure that the caller is root. */
	error = suser(td);
	if (error)
		return (error);

	/*
	 * If accounting is to be started to a file, open that file for
	 * appending and make sure it's a 'normal'.  While we could
	 * conditionally acquire Giant here, we're actually interacting with
	 * vnodes from possibly two file systems, making the logic a bit
	 * complicated.  For now, use Giant unconditionally.
	 */
	mtx_lock(&Giant);
	if (uap->path != NULL) {
		NDINIT(&nd, LOOKUP, NOFOLLOW | AUDITVNODE1, UIO_USERSPACE,
		    uap->path, td);
		flags = FWRITE | O_APPEND;
		error = vn_open(&nd, &flags, 0, -1);
		if (error)
			goto done;
		NDFREE(&nd, NDF_ONLY_PNBUF);
#ifdef MAC
		error = mac_check_system_acct(td->td_ucred, nd.ni_vp);
		if (error) {
			VOP_UNLOCK(nd.ni_vp, 0, td);
			vn_close(nd.ni_vp, flags, td->td_ucred, td);
			goto done;
		}
#endif
		VOP_UNLOCK(nd.ni_vp, 0, td);
		if (nd.ni_vp->v_type != VREG) {
			vn_close(nd.ni_vp, flags, td->td_ucred, td);
			error = EACCES;
			goto done;
		}
#ifdef MAC
	} else {
		error = mac_check_system_acct(td->td_ucred, NULL);
		if (error)
			goto done;
#endif
	}

	/*
	 * Disallow concurrent access to the accounting vnode while we swap
	 * it out, in order to prevent access after close.
	 */
	sx_xlock(&acct_sx);

	/*
	 * If accounting was previously enabled, kill the old space-watcher,
	 * close the file, and (if no new file was specified, leave).  Reset
	 * the suspended state regardless of whether accounting remains
	 * enabled.
	 */
	acct_suspended = 0;
	if (acct_vp != NULL)
		error = acct_disable(td);
	if (uap->path == NULL) {
		if (acct_state & ACCT_RUNNING) {
			acct_state |= ACCT_EXITREQ;
			wakeup(&acct_state);
		}
		sx_xunlock(&acct_sx);
		goto done;
	}

	/*
	 * Save the new accounting file vnode, and schedule the new
	 * free space watcher.
	 */
	acct_vp = nd.ni_vp;
	acct_cred = crhold(td->td_ucred);
	acct_flags = flags;
	if (acct_state & ACCT_RUNNING)
		acct_state &= ~ACCT_EXITREQ;
	else {
		/*
		 * Try to start up an accounting kthread.  We may start more
		 * than one, but if so the extras will commit suicide as
		 * soon as they start up.
		 */
		error = kthread_create(acct_thread, NULL, NULL, 0, 0,
		    "accounting");
		if (error) {
			(void) vn_close(acct_vp, acct_flags, acct_cred, td);
			crfree(acct_cred);
			acct_vp = NULL;
			acct_cred = NULL;
			acct_flags = 0;
			sx_xunlock(&acct_sx);
			log(LOG_NOTICE, "Unable to start accounting thread\n");
			goto done;
		}
	}
	sx_xunlock(&acct_sx);
	log(LOG_NOTICE, "Accounting enabled\n");
done:
	mtx_unlock(&Giant);
	return (error);
}

/*
 * Disable currently in-progress accounting by closing the vnode, dropping
 * our reference to the credential, and clearing the vnode's flags.
 */
static int
acct_disable(struct thread *td)
{
	int error;

	sx_assert(&acct_sx, SX_XLOCKED);
	error = vn_close(acct_vp, acct_flags, acct_cred, td);
	crfree(acct_cred);
	acct_vp = NULL;
	acct_cred = NULL;
	acct_flags = 0;
	log(LOG_NOTICE, "Accounting disabled\n");
	return (error);
}

/*
 * Write out process accounting information, on process exit.
 * Data to be written out is specified in Leffler, et al.
 * and are enumerated below.  (They're also noted in the system
 * "acct.h" header file.)
 */
int
acct_process(struct thread *td)
{
	struct acct acct;
	struct timeval ut, st, tmp;
	struct plimit *newlim, *oldlim;
	struct proc *p;
	struct rusage *r;
	int t, ret, vfslocked;

	/*
	 * Lockless check of accounting condition before doing the hard
	 * work.
	 */
	if (acct_vp == NULL || acct_suspended)
		return (0);

	sx_slock(&acct_sx);

	/*
	 * If accounting isn't enabled, don't bother.  Have to check again
	 * once we own the lock in case we raced with disabling of accounting
	 * by another thread.
	 */
	if (acct_vp == NULL || acct_suspended) {
		sx_sunlock(&acct_sx);
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
	calcru(p, &ut, &st);
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
	vfslocked = VFS_LOCK_GIANT(acct_vp->v_mount);
	VOP_LEASE(acct_vp, td, acct_cred, LEASE_WRITE);
	ret = vn_rdwr(UIO_WRITE, acct_vp, (caddr_t)&acct, sizeof (acct),
	    (off_t)0, UIO_SYSSPACE, IO_APPEND|IO_UNIT, acct_cred, NOCRED,
	    (int *)0, td);
	VFS_UNLOCK_GIANT(vfslocked);
	sx_sunlock(&acct_sx);
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
encode_comp_t(u_long s, u_long us)
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
acctwatch(void)
{
	struct statfs sb;
	int vfslocked;

	sx_assert(&acct_sx, SX_XLOCKED);

	/*
	 * If accounting was disabled before our kthread was scheduled,
	 * then acct_vp might be NULL.  If so, just ask our kthread to
	 * exit and return.
	 */
	if (acct_vp == NULL) {
		acct_state |= ACCT_EXITREQ;
		return;
	}

	/*
	 * If our vnode is no longer valid, tear it down and signal the
	 * accounting thread to die.
	 */
	vfslocked = VFS_LOCK_GIANT(acct_vp->v_mount);
	if (acct_vp->v_type == VBAD) {
		(void) acct_disable(NULL);
		VFS_UNLOCK_GIANT(vfslocked);
		acct_state |= ACCT_EXITREQ;
		return;
	}

	/*
	 * Stopping here is better than continuing, maybe it will be VBAD
	 * next time around.
	 */
	if (VFS_STATFS(acct_vp->v_mount, &sb, curthread) < 0) {
		VFS_UNLOCK_GIANT(vfslocked);
		return;
	}
	VFS_UNLOCK_GIANT(vfslocked);
	if (acct_suspended) {
		if (sb.f_bavail > (int64_t)(acctresume * sb.f_blocks /
		    100)) {
			acct_suspended = 0;
			log(LOG_NOTICE, "Accounting resumed\n");
		}
	} else {
		if (sb.f_bavail <= (int64_t)(acctsuspend * sb.f_blocks /
		    100)) {
			acct_suspended = 1;
			log(LOG_NOTICE, "Accounting suspended\n");
		}
	}
}

/*
 * The main loop for the dedicated kernel thread that periodically calls
 * acctwatch().
 */
static void
acct_thread(void *dummy)
{
	u_char pri;

	/* This is a low-priority kernel thread. */
	pri = PRI_MAX_KERN;
	mtx_lock_spin(&sched_lock);
	sched_prio(curthread, pri);
	mtx_unlock_spin(&sched_lock);

	/* If another accounting kthread is already running, just die. */
	sx_xlock(&acct_sx);
	if (acct_state & ACCT_RUNNING) {
		sx_xunlock(&acct_sx);
		kthread_exit(0);
	}
	acct_state |= ACCT_RUNNING;

	/* Loop until we are asked to exit. */
	while (!(acct_state & ACCT_EXITREQ)) {

		/* Perform our periodic checks. */
		acctwatch();

		/*
		 * We check this flag again before sleeping since the
		 * acctwatch() might have shut down accounting and asked us
		 * to exit.
		 */
		if (!(acct_state & ACCT_EXITREQ)) {
			sx_xunlock(&acct_sx);
			tsleep(&acct_state, pri, "-", acctchkfreq * hz);
			sx_xlock(&acct_sx);
		}
	}

	/*
	 * Acknowledge the exit request and shutdown.  We clear both the
	 * exit request and running flags.
	 */
	acct_state = 0;
	sx_xunlock(&acct_sx);
	kthread_exit(0);
}
