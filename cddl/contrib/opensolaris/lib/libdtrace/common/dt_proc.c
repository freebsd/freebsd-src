/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2010 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2012 by Delphix. All rights reserved.
 */

/*
 * DTrace Process Control
 *
 * This file provides a set of routines that permit libdtrace and its clients
 * to create and grab process handles using libproc, and to share these handles
 * between library mechanisms that need libproc access, such as ustack(), and
 * client mechanisms that need libproc access, such as dtrace(1M) -c and -p.
 * The library provides several mechanisms in the libproc control layer:
 *
 * Reference Counting: The library code and client code can independently grab
 * the same process handles without interfering with one another.  Only when
 * the reference count drops to zero and the handle is not being cached (see
 * below for more information on caching) will Prelease() be called on it.
 *
 * Handle Caching: If a handle is grabbed PGRAB_RDONLY (e.g. by ustack()) and
 * the reference count drops to zero, the handle is not immediately released.
 * Instead, libproc handles are maintained on dph_lrulist in order from most-
 * recently accessed to least-recently accessed.  Idle handles are maintained
 * until a pre-defined LRU cache limit is exceeded, permitting repeated calls
 * to ustack() to avoid the overhead of releasing and re-grabbing processes.
 *
 * Process Control: For processes that are grabbed for control (~PGRAB_RDONLY)
 * or created by dt_proc_create(), a control thread is created to provide
 * callbacks on process exit and symbol table caching on dlopen()s.
 *
 * MT-Safety: Libproc is not MT-Safe, so dt_proc_lock() and dt_proc_unlock()
 * are provided to synchronize access to the libproc handle between libdtrace
 * code and client code and the control thread's use of the ps_prochandle.
 *
 * NOTE: MT-Safety is NOT provided for libdtrace itself, or for use of the
 * dtrace_proc_grab/dtrace_proc_create mechanisms.  Like all exported libdtrace
 * calls, these are assumed to be MT-Unsafe.  MT-Safety is ONLY provided for
 * synchronization between libdtrace control threads and the client thread.
 *
 * The ps_prochandles themselves are maintained along with a dt_proc_t struct
 * in a hash table indexed by PID.  This provides basic locking and reference
 * counting.  The dt_proc_t is also maintained in LRU order on dph_lrulist.
 * The dph_lrucnt and dph_lrulim count the number of cacheable processes and
 * the current limit on the number of actively cached entries.
 *
 * The control thread for a process establishes breakpoints at the rtld_db
 * locations of interest, updates mappings and symbol tables at these points,
 * and handles exec and fork (by always following the parent).  The control
 * thread automatically exits when the process dies or control is lost.
 *
 * A simple notification mechanism is provided for libdtrace clients using
 * dtrace_handle_proc() for notification of PS_UNDEAD or PS_LOST events.  If
 * such an event occurs, the dt_proc_t itself is enqueued on a notification
 * list and the control thread broadcasts to dph_cv.  dtrace_sleep() will wake
 * up using this condition and will then call the client handler as necessary.
 */

#include <sys/syscall.h>
#include <sys/wait.h>
#include <strings.h>
#include <signal.h>
#include <assert.h>
#include <errno.h>

#include <dt_proc.h>
#include <dt_pid.h>
#include <dt_impl.h>

#include <libproc_compat.h>

#define	IS_SYS_EXEC(w)	(w == SYS_execve)
#define	IS_SYS_FORK(w)	(w == SYS_vfork || w == SYS_fork)

static dt_bkpt_t *
dt_proc_bpcreate(dt_proc_t *dpr, uintptr_t addr, dt_bkpt_f *func, void *data)
{
	struct ps_prochandle *P = dpr->dpr_proc;
	dt_bkpt_t *dbp;

	assert(DT_MUTEX_HELD(&dpr->dpr_lock));

	if ((dbp = dt_zalloc(dpr->dpr_hdl, sizeof (dt_bkpt_t))) != NULL) {
		dbp->dbp_func = func;
		dbp->dbp_data = data;
		dbp->dbp_addr = addr;

		if (Psetbkpt(P, dbp->dbp_addr, &dbp->dbp_instr) == 0)
			dbp->dbp_active = B_TRUE;

		dt_list_append(&dpr->dpr_bps, dbp);
	}

	return (dbp);
}

static void
dt_proc_bpdestroy(dt_proc_t *dpr, int delbkpts)
{
	int state = Pstate(dpr->dpr_proc);
	dt_bkpt_t *dbp, *nbp;

	assert(DT_MUTEX_HELD(&dpr->dpr_lock));

	for (dbp = dt_list_next(&dpr->dpr_bps); dbp != NULL; dbp = nbp) {
		if (delbkpts && dbp->dbp_active &&
		    state != PS_LOST && state != PS_UNDEAD) {
			(void) Pdelbkpt(dpr->dpr_proc,
			    dbp->dbp_addr, dbp->dbp_instr);
		}
		nbp = dt_list_next(dbp);
		dt_list_delete(&dpr->dpr_bps, dbp);
		dt_free(dpr->dpr_hdl, dbp);
	}
}

static void
dt_proc_bpmatch(dtrace_hdl_t *dtp, dt_proc_t *dpr)
{
	unsigned long pc;
	dt_bkpt_t *dbp;

	assert(DT_MUTEX_HELD(&dpr->dpr_lock));

	proc_regget(dpr->dpr_proc, REG_PC, &pc);
	proc_bkptregadj(&pc);

	for (dbp = dt_list_next(&dpr->dpr_bps);
	    dbp != NULL; dbp = dt_list_next(dbp)) {
		if (pc == dbp->dbp_addr)
			break;
	}

	if (dbp == NULL) {
		dt_dprintf("pid %d: spurious breakpoint wakeup for %lx\n",
		    (int)dpr->dpr_pid, pc);
		return;
	}

	dt_dprintf("pid %d: hit breakpoint at %lx (%lu)\n",
	    (int)dpr->dpr_pid, (ulong_t)dbp->dbp_addr, ++dbp->dbp_hits);

	dbp->dbp_func(dtp, dpr, dbp->dbp_data);
	(void) Pxecbkpt(dpr->dpr_proc, dbp->dbp_instr);
}

static void
dt_proc_bpenable(dt_proc_t *dpr)
{
	dt_bkpt_t *dbp;

	assert(DT_MUTEX_HELD(&dpr->dpr_lock));

	for (dbp = dt_list_next(&dpr->dpr_bps);
	    dbp != NULL; dbp = dt_list_next(dbp)) {
		if (!dbp->dbp_active && Psetbkpt(dpr->dpr_proc,
		    dbp->dbp_addr, &dbp->dbp_instr) == 0)
			dbp->dbp_active = B_TRUE;
	}

	dt_dprintf("breakpoints enabled\n");
}

static void
dt_proc_bpdisable(dt_proc_t *dpr)
{
	dt_bkpt_t *dbp;

	assert(DT_MUTEX_HELD(&dpr->dpr_lock));

	for (dbp = dt_list_next(&dpr->dpr_bps);
	    dbp != NULL; dbp = dt_list_next(dbp)) {
		if (dbp->dbp_active && Pdelbkpt(dpr->dpr_proc,
		    dbp->dbp_addr, dbp->dbp_instr) == 0)
			dbp->dbp_active = B_FALSE;
	}

	dt_dprintf("breakpoints disabled\n");
}

static void
dt_proc_notify(dtrace_hdl_t *dtp, dt_proc_hash_t *dph, dt_proc_t *dpr,
    const char *msg)
{
	dt_proc_notify_t *dprn = dt_alloc(dtp, sizeof (dt_proc_notify_t));

	if (dprn == NULL) {
		dt_dprintf("failed to allocate notification for %d %s\n",
		    (int)dpr->dpr_pid, msg);
	} else {
		dprn->dprn_dpr = dpr;
		if (msg == NULL)
			dprn->dprn_errmsg[0] = '\0';
		else
			(void) strlcpy(dprn->dprn_errmsg, msg,
			    sizeof (dprn->dprn_errmsg));

		(void) pthread_mutex_lock(&dph->dph_lock);

		dprn->dprn_next = dph->dph_notify;
		dph->dph_notify = dprn;

		(void) pthread_cond_broadcast(&dph->dph_cv);
		(void) pthread_mutex_unlock(&dph->dph_lock);
	}
}

/*
 * Check to see if the control thread was requested to stop when the victim
 * process reached a particular event (why) rather than continuing the victim.
 * If 'why' is set in the stop mask, we wait on dpr_cv for dt_proc_continue().
 * If 'why' is not set, this function returns immediately and does nothing.
 */
static void
dt_proc_stop(dt_proc_t *dpr, uint8_t why)
{
	assert(DT_MUTEX_HELD(&dpr->dpr_lock));
	assert(why != DT_PROC_STOP_IDLE);

	if (dpr->dpr_stop & why) {
		dpr->dpr_stop |= DT_PROC_STOP_IDLE;
		dpr->dpr_stop &= ~why;

		(void) pthread_cond_broadcast(&dpr->dpr_cv);

		/*
		 * We disable breakpoints while stopped to preserve the
		 * integrity of the program text for both our own disassembly
		 * and that of the kernel.
		 */
		dt_proc_bpdisable(dpr);

		while (dpr->dpr_stop & DT_PROC_STOP_IDLE)
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

		dt_proc_bpenable(dpr);
	}
}

/*ARGSUSED*/
static void
dt_proc_bpmain(dtrace_hdl_t *dtp, dt_proc_t *dpr, const char *fname)
{
	dt_dprintf("pid %d: breakpoint at %s()\n", (int)dpr->dpr_pid, fname);
	dt_proc_stop(dpr, DT_PROC_STOP_MAIN);
}

static void
dt_proc_rdevent(dtrace_hdl_t *dtp, dt_proc_t *dpr, const char *evname)
{
	rd_event_msg_t rdm;
	rd_err_e err;

	if ((err = rd_event_getmsg(dpr->dpr_rtld, &rdm)) != RD_OK) {
		dt_dprintf("pid %d: failed to get %s event message: %s\n",
		    (int)dpr->dpr_pid, evname, rd_errstr(err));
		return;
	}

	dt_dprintf("pid %d: rtld event %s type=%d state %d\n",
	    (int)dpr->dpr_pid, evname, rdm.type, rdm.u.state);

	switch (rdm.type) {
	case RD_DLACTIVITY:
		if (rdm.u.state != RD_CONSISTENT)
			break;

		Pupdate_syms(dpr->dpr_proc);
		if (dt_pid_create_probes_module(dtp, dpr) != 0)
			dt_proc_notify(dtp, dtp->dt_procs, dpr,
			    dpr->dpr_errmsg);

		break;
	case RD_PREINIT:
		Pupdate_syms(dpr->dpr_proc);
		dt_proc_stop(dpr, DT_PROC_STOP_PREINIT);
		break;
	case RD_POSTINIT:
		Pupdate_syms(dpr->dpr_proc);
		dt_proc_stop(dpr, DT_PROC_STOP_POSTINIT);
		break;
	}
}

static void
dt_proc_rdwatch(dt_proc_t *dpr, rd_event_e event, const char *evname)
{
	rd_notify_t rdn;
	rd_err_e err;

	if ((err = rd_event_addr(dpr->dpr_rtld, event, &rdn)) != RD_OK) {
		dt_dprintf("pid %d: failed to get event address for %s: %s\n",
		    (int)dpr->dpr_pid, evname, rd_errstr(err));
		return;
	}

	if (rdn.type != RD_NOTIFY_BPT) {
		dt_dprintf("pid %d: event %s has unexpected type %d\n",
		    (int)dpr->dpr_pid, evname, rdn.type);
		return;
	}

	(void) dt_proc_bpcreate(dpr, rdn.u.bptaddr,
	    /* XXX ugly */
	    (dt_bkpt_f *)dt_proc_rdevent, __DECONST(void *, evname));
}

/*
 * Common code for enabling events associated with the run-time linker after
 * attaching to a process or after a victim process completes an exec(2).
 */
static void
dt_proc_attach(dt_proc_t *dpr, int exec)
{
	rd_err_e err;
	GElf_Sym sym;

	assert(DT_MUTEX_HELD(&dpr->dpr_lock));

	if (exec) {

		dt_proc_bpdestroy(dpr, B_FALSE);
	}
	if ((dpr->dpr_rtld = Prd_agent(dpr->dpr_proc)) != NULL &&
	    (err = rd_event_enable(dpr->dpr_rtld, B_TRUE)) == RD_OK) {
		dt_proc_rdwatch(dpr, RD_POSTINIT, "RD_POSTINIT");
	} else {
		dt_dprintf("pid %d: failed to enable rtld events: %s\n",
		    (int)dpr->dpr_pid, dpr->dpr_rtld ? rd_errstr(err) :
		    "rtld_db agent initialization failed");
	}

	Pupdate_maps(dpr->dpr_proc);

	if (Pxlookup_by_name(dpr->dpr_proc, LM_ID_BASE,
	    "a.out", "main", &sym, NULL) == 0) {
		(void) dt_proc_bpcreate(dpr, (uintptr_t)sym.st_value,
		    (dt_bkpt_f *)dt_proc_bpmain, "a.out`main");
	} else {
		dt_dprintf("pid %d: failed to find a.out`main: %s\n",
		    (int)dpr->dpr_pid, strerror(errno));
	}
}

typedef struct dt_proc_control_data {
	dtrace_hdl_t *dpcd_hdl;			/* DTrace handle */
	dt_proc_t *dpcd_proc;			/* proccess to control */
} dt_proc_control_data_t;

/*
 * Main loop for all victim process control threads.  We initialize all the
 * appropriate /proc control mechanisms, and then enter a loop waiting for
 * the process to stop on an event or die.  We process any events by calling
 * appropriate subroutines, and exit when the victim dies or we lose control.
 *
 * The control thread synchronizes the use of dpr_proc with other libdtrace
 * threads using dpr_lock.  We hold the lock for all of our operations except
 * waiting while the process is running: this is accomplished by writing a
 * PCWSTOP directive directly to the underlying /proc/<pid>/ctl file.  If the
 * libdtrace client wishes to exit or abort our wait, SIGCANCEL can be used.
 */
static void *
dt_proc_control(void *arg)
{
	dt_proc_control_data_t *datap = arg;
	dtrace_hdl_t *dtp = datap->dpcd_hdl;
	dt_proc_t *dpr = datap->dpcd_proc;
	dt_proc_hash_t *dph = dtp->dt_procs;
	struct ps_prochandle *P = dpr->dpr_proc;
	int pid = dpr->dpr_pid;
	int notify = B_FALSE;

	/*
	 * We disable the POSIX thread cancellation mechanism so that the
	 * client program using libdtrace can't accidentally cancel our thread.
	 * dt_proc_destroy() uses SIGCANCEL explicitly to simply poke us out
	 * of PCWSTOP with EINTR, at which point we will see dpr_quit and exit.
	 */
	(void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	/*
	 * Set up the corresponding process for tracing by libdtrace.  We want
	 * to be able to catch breakpoints and efficiently single-step over
	 * them, and we need to enable librtld_db to watch libdl activity.
	 */
	(void) pthread_mutex_lock(&dpr->dpr_lock);

	dt_proc_attach(dpr, B_FALSE);		/* enable rtld breakpoints */

	/*
	 * If DT_CLOSE_KILL is set, we created the process; otherwise we
	 * grabbed it.  Check for an appropriate stop request and wait for
	 * dt_proc_continue.
	 */
	if (dpr->dpr_close == DT_CLOSE_KILL)
		dt_proc_stop(dpr, DT_PROC_STOP_CREATE);
	else
		dt_proc_stop(dpr, DT_PROC_STOP_GRAB);

	if (Psetrun(P, 0, 0) == -1) {
		dt_dprintf("pid %d: failed to set running: %s\n",
		    (int)dpr->dpr_pid, strerror(errno));
	}

	(void) pthread_mutex_unlock(&dpr->dpr_lock);

	/*
	 * Wait for the process corresponding to this control thread to stop,
	 * process the event, and then set it running again.  We want to sleep
	 * with dpr_lock *unheld* so that other parts of libdtrace can use the
	 * ps_prochandle in the meantime (e.g. ustack()).  To do this, we write
	 * a PCWSTOP directive directly to the underlying /proc/<pid>/ctl file.
	 * Once the process stops, we wake up, grab dpr_lock, and then call
	 * Pwait() (which will return immediately) and do our processing.
	 */
	while (!dpr->dpr_quit) {
		const lwpstatus_t *psp;

		/* Wait for the process to report status. */
		proc_wstatus(P);
		if (errno == EINTR)
			continue; /* check dpr_quit and continue waiting */

		(void) pthread_mutex_lock(&dpr->dpr_lock);

		switch (Pstate(P)) {
		case PS_STOP:
			psp = proc_getlwpstatus(P);

			dt_dprintf("pid %d: proc stopped showing %d/%d\n",
			    pid, psp->pr_why, psp->pr_what);

			/*
			 * If the process stops showing one of the events that
			 * we are tracing, perform the appropriate response.
			 * Note that we ignore PR_SUSPENDED, PR_CHECKPOINT, and
			 * PR_JOBCONTROL by design: if one of these conditions
			 * occurs, we will fall through to Psetrun() but the
			 * process will remain stopped in the kernel by the
			 * corresponding mechanism (e.g. job control stop).
			 */
			if (psp->pr_why == PR_FAULTED && psp->pr_what == FLTBPT)
				dt_proc_bpmatch(dtp, dpr);
			else if (psp->pr_why == PR_SYSENTRY &&
			    IS_SYS_FORK(psp->pr_what))
				dt_proc_bpdisable(dpr);
			else if (psp->pr_why == PR_SYSEXIT &&
			    IS_SYS_FORK(psp->pr_what))
				dt_proc_bpenable(dpr);
			else if (psp->pr_why == PR_SYSEXIT &&
			    IS_SYS_EXEC(psp->pr_what))
				dt_proc_attach(dpr, B_TRUE);
			break;

		case PS_LOST:
			dt_dprintf("pid %d: proc lost: %s\n",
			    pid, strerror(errno));

			dpr->dpr_quit = B_TRUE;
			notify = B_TRUE;
			break;

		case PS_UNDEAD:
			dt_dprintf("pid %d: proc died\n", pid);
			dpr->dpr_quit = B_TRUE;
			notify = B_TRUE;
			break;
		}

		if (Pstate(P) != PS_UNDEAD) {
			if (dpr->dpr_quit && dpr->dpr_close == DT_CLOSE_KILL) {
				/*
				 * We're about to kill the child, so don't
				 * bother resuming it.  In some cases, such as
				 * an initialization error, we shouldn't have
				 * started it in the first place, so letting it
				 * run could be harmful.
				 */
			} else if (Psetrun(P, 0, 0) == -1) {
				dt_dprintf("pid %d: failed to set running: "
				    "%s\n", (int)dpr->dpr_pid, strerror(errno));
			}
		}

		(void) pthread_mutex_unlock(&dpr->dpr_lock);
	}

	/*
	 * If the control thread detected PS_UNDEAD or PS_LOST, then enqueue
	 * the dt_proc_t structure on the dt_proc_hash_t notification list.
	 */
	if (notify)
		dt_proc_notify(dtp, dph, dpr, NULL);

	/*
	 * Destroy and remove any remaining breakpoints, set dpr_done and clear
	 * dpr_tid to indicate the control thread has exited, and notify any
	 * waiting thread in dt_proc_destroy() that we have succesfully exited.
	 */
	(void) pthread_mutex_lock(&dpr->dpr_lock);

	dt_proc_bpdestroy(dpr, B_TRUE);
	dpr->dpr_done = B_TRUE;
	dpr->dpr_tid = 0;

	(void) pthread_cond_broadcast(&dpr->dpr_cv);
	(void) pthread_mutex_unlock(&dpr->dpr_lock);

	return (NULL);
}

/*PRINTFLIKE3*/
static struct ps_prochandle *
dt_proc_error(dtrace_hdl_t *dtp, dt_proc_t *dpr, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	dt_set_errmsg(dtp, NULL, NULL, NULL, 0, format, ap);
	va_end(ap);

	if (dpr->dpr_proc != NULL)
		Prelease(dpr->dpr_proc, 0);

	dt_free(dtp, dpr);
	(void) dt_set_errno(dtp, EDT_COMPILER);
	return (NULL);
}

dt_proc_t *
dt_proc_lookup(dtrace_hdl_t *dtp, struct ps_prochandle *P, int remove)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	pid_t pid = proc_getpid(P);
	dt_proc_t *dpr, **dpp = &dph->dph_hash[pid & (dph->dph_hashlen - 1)];

	for (dpr = *dpp; dpr != NULL; dpr = dpr->dpr_hash) {
		if (dpr->dpr_pid == pid)
			break;
		else
			dpp = &dpr->dpr_hash;
	}

	assert(dpr != NULL);
	assert(dpr->dpr_proc == P);

	if (remove)
		*dpp = dpr->dpr_hash; /* remove from pid hash chain */

	return (dpr);
}

static void
dt_proc_destroy(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_notify_t *npr, **npp;
	int rflag;

	assert(dpr != NULL);

	switch (dpr->dpr_close) {
	case DT_CLOSE_KILL:
		dt_dprintf("killing pid %d\n", (int)dpr->dpr_pid);
		rflag = PRELEASE_KILL;
		break;
	case DT_CLOSE_RUN:
		dt_dprintf("releasing pid %d\n", (int)dpr->dpr_pid);
		rflag = 0;
		break;
	}

	if (dpr->dpr_tid) {
		/*
		 * Set the dpr_quit flag to tell the daemon thread to exit.  We
		 * send it a SIGCANCEL to poke it out of PCWSTOP or any other
		 * long-term /proc system call.  Our daemon threads have POSIX
		 * cancellation disabled, so EINTR will be the only effect.  We
		 * then wait for dpr_done to indicate the thread has exited.
		 *
		 * We can't use pthread_kill() to send SIGCANCEL because the
		 * interface forbids it and we can't use pthread_cancel()
		 * because with cancellation disabled it won't actually
		 * send SIGCANCEL to the target thread, so we use _lwp_kill()
		 * to do the job.  This is all built on evil knowledge of
		 * the details of the cancellation mechanism in libc.
		 */
		(void) pthread_mutex_lock(&dpr->dpr_lock);
		dpr->dpr_quit = B_TRUE;
		pthread_kill(dpr->dpr_tid, SIGTHR);

		/*
		 * If the process is currently idling in dt_proc_stop(), re-
		 * enable breakpoints and poke it into running again.
		 */
		if (dpr->dpr_stop & DT_PROC_STOP_IDLE) {
			dt_proc_bpenable(dpr);
			dpr->dpr_stop &= ~DT_PROC_STOP_IDLE;
			(void) pthread_cond_broadcast(&dpr->dpr_cv);
		}

		while (!dpr->dpr_done)
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

		(void) pthread_mutex_unlock(&dpr->dpr_lock);
	}

	/*
	 * Before we free the process structure, remove this dt_proc_t from the
	 * lookup hash, and then walk the dt_proc_hash_t's notification list
	 * and remove this dt_proc_t if it is enqueued.
	 */
	(void) pthread_mutex_lock(&dph->dph_lock);
	(void) dt_proc_lookup(dtp, P, B_TRUE);
	npp = &dph->dph_notify;

	while ((npr = *npp) != NULL) {
		if (npr->dprn_dpr == dpr) {
			*npp = npr->dprn_next;
			dt_free(dtp, npr);
		} else {
			npp = &npr->dprn_next;
		}
	}

	(void) pthread_mutex_unlock(&dph->dph_lock);

	/*
	 * Remove the dt_proc_list from the LRU list, release the underlying
	 * libproc handle, and free our dt_proc_t data structure.
	 */
	if (dpr->dpr_cacheable) {
		assert(dph->dph_lrucnt != 0);
		dph->dph_lrucnt--;
	}

	dt_list_delete(&dph->dph_lrulist, dpr);
	Prelease(dpr->dpr_proc, rflag);
	dt_free(dtp, dpr);
}

static int
dt_proc_create_thread(dtrace_hdl_t *dtp, dt_proc_t *dpr, uint_t stop)
{
	dt_proc_control_data_t data;
	sigset_t nset, oset;
	pthread_attr_t a;
	int err;

	(void) pthread_mutex_lock(&dpr->dpr_lock);
	dpr->dpr_stop |= stop; /* set bit for initial rendezvous */

	(void) pthread_attr_init(&a);
	(void) pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);

	(void) sigfillset(&nset);
	(void) sigdelset(&nset, SIGABRT);	/* unblocked for assert() */
	(void) sigdelset(&nset, SIGUSR1);	/* see dt_proc_destroy() */

	data.dpcd_hdl = dtp;
	data.dpcd_proc = dpr;

	(void) pthread_sigmask(SIG_SETMASK, &nset, &oset);
	err = pthread_create(&dpr->dpr_tid, &a, dt_proc_control, &data);
	(void) pthread_sigmask(SIG_SETMASK, &oset, NULL);

	/*
	 * If the control thread was created, then wait on dpr_cv for either
	 * dpr_done to be set (the victim died or the control thread failed)
	 * or DT_PROC_STOP_IDLE to be set, indicating that the victim is now
	 * stopped by /proc and the control thread is at the rendezvous event.
	 * On success, we return with the process and control thread stopped:
	 * the caller can then apply dt_proc_continue() to resume both.
	 */
	if (err == 0) {
		while (!dpr->dpr_done && !(dpr->dpr_stop & DT_PROC_STOP_IDLE))
			(void) pthread_cond_wait(&dpr->dpr_cv, &dpr->dpr_lock);

		/*
		 * If dpr_done is set, the control thread aborted before it
		 * reached the rendezvous event.  This is either due to PS_LOST
		 * or PS_UNDEAD (i.e. the process died).  We try to provide a
		 * small amount of useful information to help figure it out.
		 */
		if (dpr->dpr_done) {
			int stat = proc_getwstat(dpr->dpr_proc);
			int pid = proc_getpid(dpr->dpr_proc);
			if (proc_state(dpr->dpr_proc) == PS_LOST) {
				(void) dt_proc_error(dpr->dpr_hdl, dpr,
				    "failed to control pid %d: process exec'd "
				    "set-id or unobservable program\n", pid);
			} else if (WIFSIGNALED(stat)) {
				(void) dt_proc_error(dpr->dpr_hdl, dpr,
				    "failed to control pid %d: process died "
				    "from signal %d\n", pid, WTERMSIG(stat));
			} else {
				(void) dt_proc_error(dpr->dpr_hdl, dpr,
				    "failed to control pid %d: process exited "
				    "with status %d\n", pid, WEXITSTATUS(stat));
			}

			err = ESRCH; /* cause grab() or create() to fail */
		}
	} else {
		(void) dt_proc_error(dpr->dpr_hdl, dpr,
		    "failed to create control thread for process-id %d: %s\n",
		    (int)dpr->dpr_pid, strerror(err));
	}

	if (err == 0)
		(void) pthread_mutex_unlock(&dpr->dpr_lock);
	(void) pthread_attr_destroy(&a);

	return (err);
}

struct ps_prochandle *
dt_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv,
    proc_child_func *pcf, void *child_arg)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr;
	int err;

	if ((dpr = dt_zalloc(dtp, sizeof (dt_proc_t))) == NULL)
		return (NULL); /* errno is set for us */

	(void) pthread_mutex_init(&dpr->dpr_lock, NULL);
	(void) pthread_cond_init(&dpr->dpr_cv, NULL);

	if ((err = proc_create(file, argv, dtp->dt_proc_env, pcf, child_arg,
	    &dpr->dpr_proc)) != 0) {
		return (dt_proc_error(dtp, dpr,
		    "failed to execute %s: %s\n", file, Pcreate_error(err)));
	}

	dpr->dpr_hdl = dtp;
	dpr->dpr_pid = proc_getpid(dpr->dpr_proc);
	dpr->dpr_close = DT_CLOSE_KILL;

	if (dt_proc_create_thread(dtp, dpr, dtp->dt_prcmode) != 0)
		return (NULL); /* dt_proc_error() has been called for us */

	dpr->dpr_hash = dph->dph_hash[dpr->dpr_pid & (dph->dph_hashlen - 1)];
	dph->dph_hash[dpr->dpr_pid & (dph->dph_hashlen - 1)] = dpr;
	dt_list_prepend(&dph->dph_lrulist, dpr);

	dt_dprintf("created pid %d\n", (int)dpr->dpr_pid);
	dpr->dpr_refs++;

	return (dpr->dpr_proc);
}

struct ps_prochandle *
dt_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags, int nomonitor)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	uint_t h = pid & (dph->dph_hashlen - 1);
	dt_proc_t *dpr, *opr;
	int err;

	/*
	 * Search the hash table for the pid.  If it is already grabbed or
	 * created, move the handle to the front of the lrulist, increment
	 * the reference count, and return the existing ps_prochandle.
	 */
	for (dpr = dph->dph_hash[h]; dpr != NULL; dpr = dpr->dpr_hash) {
		if (dpr->dpr_pid == pid && !dpr->dpr_stale) {
			/*
			 * If the cached handle was opened read-only and
			 * this request is for a writeable handle, mark
			 * the cached handle as stale and open a new handle.
			 * Since it's stale, unmark it as cacheable.
			 */
			if (dpr->dpr_rdonly && !(flags & PGRAB_RDONLY)) {
				dt_dprintf("upgrading pid %d\n", (int)pid);
				dpr->dpr_stale = B_TRUE;
				dpr->dpr_cacheable = B_FALSE;
				dph->dph_lrucnt--;
				break;
			}

			dt_dprintf("grabbed pid %d (cached)\n", (int)pid);
			dt_list_delete(&dph->dph_lrulist, dpr);
			dt_list_prepend(&dph->dph_lrulist, dpr);
			dpr->dpr_refs++;
			return (dpr->dpr_proc);
		}
	}

	if ((dpr = dt_zalloc(dtp, sizeof (dt_proc_t))) == NULL)
		return (NULL); /* errno is set for us */

	(void) pthread_mutex_init(&dpr->dpr_lock, NULL);
	(void) pthread_cond_init(&dpr->dpr_cv, NULL);

	if ((err = proc_attach(pid, flags, &dpr->dpr_proc)) != 0) {
		return (dt_proc_error(dtp, dpr,
		    "failed to grab pid %d: %s\n", (int)pid, Pgrab_error(err)));
	}

	dpr->dpr_hdl = dtp;
	dpr->dpr_pid = pid;
	dpr->dpr_close = DT_CLOSE_RUN;

	/*
	 * If we are attempting to grab the process without a monitor
	 * thread, then mark the process cacheable only if it's being
	 * grabbed read-only.  If we're currently caching more process
	 * handles than dph_lrulim permits, attempt to find the
	 * least-recently-used handle that is currently unreferenced and
	 * release it from the cache.  Otherwise we are grabbing the process
	 * for control: create a control thread for this process and store
	 * its ID in dpr->dpr_tid.
	 */
	if (nomonitor || (flags & PGRAB_RDONLY)) {
		if (dph->dph_lrucnt >= dph->dph_lrulim) {
			for (opr = dt_list_prev(&dph->dph_lrulist);
			    opr != NULL; opr = dt_list_prev(opr)) {
				if (opr->dpr_cacheable && opr->dpr_refs == 0) {
					dt_proc_destroy(dtp, opr->dpr_proc);
					break;
				}
			}
		}

		if (flags & PGRAB_RDONLY) {
			dpr->dpr_cacheable = B_TRUE;
			dpr->dpr_rdonly = B_TRUE;
			dph->dph_lrucnt++;
		}

	} else if (dt_proc_create_thread(dtp, dpr, DT_PROC_STOP_GRAB) != 0)
		return (NULL); /* dt_proc_error() has been called for us */

	dpr->dpr_hash = dph->dph_hash[h];
	dph->dph_hash[h] = dpr;
	dt_list_prepend(&dph->dph_lrulist, dpr);

	dt_dprintf("grabbed pid %d\n", (int)pid);
	dpr->dpr_refs++;

	return (dpr->dpr_proc);
}

void
dt_proc_release(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	dt_proc_hash_t *dph = dtp->dt_procs;

	assert(dpr != NULL);
	assert(dpr->dpr_refs != 0);

	if (--dpr->dpr_refs == 0 &&
	    (!dpr->dpr_cacheable || dph->dph_lrucnt > dph->dph_lrulim))
		dt_proc_destroy(dtp, P);
}

void
dt_proc_continue(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);

	(void) pthread_mutex_lock(&dpr->dpr_lock);

	if (dpr->dpr_stop & DT_PROC_STOP_IDLE) {
		dpr->dpr_stop &= ~DT_PROC_STOP_IDLE;
		(void) pthread_cond_broadcast(&dpr->dpr_cv);
	}

	(void) pthread_mutex_unlock(&dpr->dpr_lock);
}

void
dt_proc_lock(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	int err = pthread_mutex_lock(&dpr->dpr_lock);
	assert(err == 0); /* check for recursion */
}

void
dt_proc_unlock(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_t *dpr = dt_proc_lookup(dtp, P, B_FALSE);
	int err = pthread_mutex_unlock(&dpr->dpr_lock);
	assert(err == 0); /* check for unheld lock */
}

void
dt_proc_init(dtrace_hdl_t *dtp)
{
	extern char **environ;
	static char *envdef[] = {
		"LD_NOLAZYLOAD=1",	/* linker lazy loading hides funcs */
		NULL
	};
	char **p;
	int i;

	if ((dtp->dt_procs = dt_zalloc(dtp, sizeof (dt_proc_hash_t) +
	    sizeof (dt_proc_t *) * _dtrace_pidbuckets - 1)) == NULL)
		return;

	(void) pthread_mutex_init(&dtp->dt_procs->dph_lock, NULL);
	(void) pthread_cond_init(&dtp->dt_procs->dph_cv, NULL);

	dtp->dt_procs->dph_hashlen = _dtrace_pidbuckets;
	dtp->dt_procs->dph_lrulim = _dtrace_pidlrulim;

	/*
	 * Count how big our environment needs to be.
	 */
	for (i = 1, p = environ; *p != NULL; i++, p++)
		continue;
	for (p = envdef; *p != NULL; i++, p++)
		continue;

	if ((dtp->dt_proc_env = dt_zalloc(dtp, sizeof (char *) * i)) == NULL)
		return;

	for (i = 0, p = environ; *p != NULL; i++, p++) {
		if ((dtp->dt_proc_env[i] = strdup(*p)) == NULL)
			goto err;
	}
	for (p = envdef; *p != NULL; i++, p++) {
		if ((dtp->dt_proc_env[i] = strdup(*p)) == NULL)
			goto err;
	}

	return;

err:
	while (--i != 0) {
		dt_free(dtp, dtp->dt_proc_env[i]);
	}
	dt_free(dtp, dtp->dt_proc_env);
	dtp->dt_proc_env = NULL;
}

void
dt_proc_fini(dtrace_hdl_t *dtp)
{
	dt_proc_hash_t *dph = dtp->dt_procs;
	dt_proc_t *dpr;
	char **p;

	while ((dpr = dt_list_next(&dph->dph_lrulist)) != NULL)
		dt_proc_destroy(dtp, dpr->dpr_proc);

	dtp->dt_procs = NULL;
	dt_free(dtp, dph);

	for (p = dtp->dt_proc_env; *p != NULL; p++)
		dt_free(dtp, *p);

	dt_free(dtp, dtp->dt_proc_env);
	dtp->dt_proc_env = NULL;
}

struct ps_prochandle *
dtrace_proc_create(dtrace_hdl_t *dtp, const char *file, char *const *argv,
    proc_child_func *pcf, void *child_arg)
{
	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	struct ps_prochandle *P = dt_proc_create(dtp, file, argv, pcf, child_arg);

	if (P != NULL && idp != NULL && idp->di_id == 0) {
		idp->di_id = proc_getpid(P); /* $target = created pid */
	}

	return (P);
}

struct ps_prochandle *
dtrace_proc_grab(dtrace_hdl_t *dtp, pid_t pid, int flags)
{
	dt_ident_t *idp = dt_idhash_lookup(dtp->dt_macros, "target");
	struct ps_prochandle *P = dt_proc_grab(dtp, pid, flags, 0);

	if (P != NULL && idp != NULL && idp->di_id == 0)
		idp->di_id = pid; /* $target = grabbed pid */

	return (P);
}

void
dtrace_proc_release(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_release(dtp, P);
}

void
dtrace_proc_continue(dtrace_hdl_t *dtp, struct ps_prochandle *P)
{
	dt_proc_continue(dtp, P);
}
