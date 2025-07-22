/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
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

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/compressor.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/rmlock.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/ucoredump.h>
#include <sys/wait.h>

static int coredump(struct thread *td, const char **);

int compress_user_cores = 0;

static SLIST_HEAD(, coredumper)	coredumpers =
    SLIST_HEAD_INITIALIZER(coredumpers);
static struct rmlock	coredump_rmlock;
RM_SYSINIT(coredump_lock, &coredump_rmlock, "coredump_lock");

static int kern_logsigexit = 1;
SYSCTL_INT(_kern, KERN_LOGSIGEXIT, logsigexit, CTLFLAG_RW,
    &kern_logsigexit, 0,
    "Log processes quitting on abnormal signals to syslog(3)");

static int sugid_coredump;
SYSCTL_INT(_kern, OID_AUTO, sugid_coredump, CTLFLAG_RWTUN,
    &sugid_coredump, 0, "Allow setuid and setgid processes to dump core");

static int do_coredump = 1;
SYSCTL_INT(_kern, OID_AUTO, coredump, CTLFLAG_RW,
	&do_coredump, 0, "Enable/Disable coredumps");

static int
sysctl_compress_user_cores(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = compress_user_cores;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (val != 0 && !compressor_avail(val))
		return (EINVAL);
	compress_user_cores = val;
	return (error);
}
SYSCTL_PROC(_kern, OID_AUTO, compress_user_cores,
    CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_NEEDGIANT, 0, sizeof(int),
    sysctl_compress_user_cores, "I",
    "Enable compression of user corefiles ("
    __XSTRING(COMPRESS_GZIP) " = gzip, "
    __XSTRING(COMPRESS_ZSTD) " = zstd)");

int compress_user_cores_level = 6;
SYSCTL_INT(_kern, OID_AUTO, compress_user_cores_level, CTLFLAG_RWTUN,
    &compress_user_cores_level, 0,
    "Corefile compression level");

void
coredumper_register(struct coredumper *cd)
{

	blockcount_init(&cd->cd_refcount);
	rm_wlock(&coredump_rmlock);
	SLIST_INSERT_HEAD(&coredumpers, cd, cd_entry);
	rm_wunlock(&coredump_rmlock);
}

void
coredumper_unregister(struct coredumper *cd)
{

	rm_wlock(&coredump_rmlock);
	SLIST_REMOVE(&coredumpers, cd, coredumper, cd_entry);
	rm_wunlock(&coredump_rmlock);

	/*
	 * Wait for any in-process coredumps to finish before returning.
	 */
	blockcount_wait(&cd->cd_refcount, NULL, "dumpwait", 0);
}

/*
 * Force the current process to exit with the specified signal, dumping core
 * if appropriate.  We bypass the normal tests for masked and caught signals,
 * allowing unrecoverable failures to terminate the process without changing
 * signal state.  Mark the accounting record with the signal termination.
 * If dumping core, save the signal number for the debugger.  Calls exit and
 * does not return.
 */
void
sigexit(struct thread *td, int sig)
{
	struct proc *p = td->td_proc;
	int rv;
	bool logexit;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	proc_set_p2_wexit(p);

	p->p_acflag |= AXSIG;
	if ((p->p_flag2 & P2_LOGSIGEXIT_CTL) == 0)
		logexit = kern_logsigexit != 0;
	else
		logexit = (p->p_flag2 & P2_LOGSIGEXIT_ENABLE) != 0;

	/*
	 * We must be single-threading to generate a core dump.  This
	 * ensures that the registers in the core file are up-to-date.
	 * Also, the ELF dump handler assumes that the thread list doesn't
	 * change out from under it.
	 *
	 * XXX If another thread attempts to single-thread before us
	 *     (e.g. via fork()), we won't get a dump at all.
	 */
	if (sig_do_core(sig) && thread_single(p, SINGLE_NO_EXIT) == 0) {
		const char *err = NULL;

		p->p_sig = sig;
		/*
		 * Log signals which would cause core dumps
		 * (Log as LOG_INFO to appease those who don't want
		 * these messages.)
		 * XXX : Todo, as well as euid, write out ruid too
		 * Note that coredump() drops proc lock.
		 */
		rv = coredump(td, &err);
		if (rv == 0) {
			MPASS(err == NULL);
			sig |= WCOREFLAG;
		} else if (err == NULL) {
			switch (rv) {
			case EFAULT:
				err = "bad address";
				break;
			case EINVAL:
				err = "invalild argument";
				break;
			case EFBIG:
				err = "too large";
				break;
			default:
				err = "other error";
				break;
			}
		}
		if (logexit)
			log(LOG_INFO,
			    "pid %d (%s), jid %d, uid %d: exited on "
			    "signal %d (%s%s)\n", p->p_pid, p->p_comm,
			    p->p_ucred->cr_prison->pr_id,
			    td->td_ucred->cr_uid, sig &~ WCOREFLAG,
			    err != NULL ? "no core dump - " : "core dumped",
			    err != NULL ? err : "");
	} else
		PROC_UNLOCK(p);
	exit1(td, 0, sig);
	/* NOTREACHED */
}


/*
 * Dump a process' core.  The main routine does some
 * policy checking, and creates the name of the coredump;
 * then it passes on a vnode and a size limit to the process-specific
 * coredump routine if there is one; if there _is not_ one, it returns
 * ENOSYS; otherwise it returns the error from the process-specific routine.
 */
static int
coredump(struct thread *td, const char **errmsg)
{
	struct coredumper *iter, *chosen;
	struct proc *p = td->td_proc;
	struct rm_priotracker tracker;
	off_t limit;
	int error, priority;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	MPASS((p->p_flag & P_HADTHREADS) == 0 || p->p_singlethread == td);

	if (!do_coredump || (!sugid_coredump && (p->p_flag & P_SUGID) != 0) ||
	    (p->p_flag2 & P2_NOTRACE) != 0) {
		PROC_UNLOCK(p);

		if (!do_coredump)
			*errmsg = "denied by kern.coredump";
		else if ((p->p_flag2 & P2_NOTRACE) != 0)
			*errmsg = "process has trace disabled";
		else
			*errmsg = "sugid process denied by kern.sugid_coredump";
		return (EFAULT);
	}

	/*
	 * Note that the bulk of limit checking is done after
	 * the corefile is created.  The exception is if the limit
	 * for corefiles is 0, in which case we don't bother
	 * creating the corefile at all.  This layout means that
	 * a corefile is truncated instead of not being created,
	 * if it is larger than the limit.
	 */
	limit = (off_t)lim_cur(td, RLIMIT_CORE);
	if (limit == 0 || racct_get_available(p, RACCT_CORE) == 0) {
		PROC_UNLOCK(p);
		*errmsg = "coredumpsize limit is 0";
		return (EFBIG);
	}

	rm_rlock(&coredump_rmlock, &tracker);
	priority = -1;
	chosen = NULL;
	SLIST_FOREACH(iter, &coredumpers, cd_entry) {
		if (iter->cd_probe == NULL) {
			/*
			 * If we haven't found anything of a higher priority
			 * yet, we'll call this a GENERIC.  Ideally, we want
			 * coredumper modules to include a probe function.
			 */
			if (priority < 0) {
				priority = COREDUMPER_GENERIC;
				chosen = iter;
			}

			continue;
		}

		error = (*iter->cd_probe)(td);
		if (error < 0)
			continue;

		/*
		 * Higher priority than previous options.
		 */
		if (error > priority) {
			priority = error;
			chosen = iter;
		}
	}

	/*
	 * Acquire our refcount before we drop the lock so that
	 * coredumper_unregister() can safely assume that the refcount will only
	 * go down once it's dropped the rmlock.
	 */
	blockcount_acquire(&chosen->cd_refcount, 1);
	rm_runlock(&coredump_rmlock, &tracker);

	/* Currently, we always have the vnode dumper built in. */
	MPASS(chosen != NULL);
	error = ((*chosen->cd_handle)(td, limit));
	PROC_LOCK_ASSERT(p, MA_NOTOWNED);

	blockcount_release(&chosen->cd_refcount, 1);

	return (error);
}
