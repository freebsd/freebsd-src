/*-
 * Copyright (c) 2005-2007 Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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
 *
 */

/*
 * Logging code for hwpmc(4)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/pmclog.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

/*
 * Sysctl tunables
 */

SYSCTL_DECL(_kern_hwpmc);

/*
 * kern.hwpmc.logbuffersize -- size of the per-cpu owner buffers.
 */

static int pmclog_buffer_size = PMC_LOG_BUFFER_SIZE;
TUNABLE_INT(PMC_SYSCTL_NAME_PREFIX "logbuffersize", &pmclog_buffer_size);
SYSCTL_INT(_kern_hwpmc, OID_AUTO, logbuffersize, CTLFLAG_TUN|CTLFLAG_RD,
    &pmclog_buffer_size, 0, "size of log buffers in kilobytes");

/*
 * kern.hwpmc.nbuffer -- number of global log buffers
 */

static int pmc_nlogbuffers = PMC_NLOGBUFFERS;
TUNABLE_INT(PMC_SYSCTL_NAME_PREFIX "nbuffers", &pmc_nlogbuffers);
SYSCTL_INT(_kern_hwpmc, OID_AUTO, nbuffers, CTLFLAG_TUN|CTLFLAG_RD,
    &pmc_nlogbuffers, 0, "number of global log buffers");

/*
 * Global log buffer list and associated spin lock.
 */

TAILQ_HEAD(, pmclog_buffer) pmc_bufferlist =
	TAILQ_HEAD_INITIALIZER(pmc_bufferlist);
static struct mtx pmc_bufferlist_mtx;	/* spin lock */
static struct mtx pmc_kthread_mtx;	/* sleep lock */

#define	PMCLOG_INIT_BUFFER_DESCRIPTOR(D) do {				\
		const int __roundup = roundup(sizeof(*D),		\
			sizeof(uint32_t));				\
		(D)->plb_fence = ((char *) (D)) +			\
			 1024*pmclog_buffer_size;			\
		(D)->plb_base  = (D)->plb_ptr = ((char *) (D)) +	\
			__roundup;					\
	} while (0)


/*
 * Log file record constructors.
 */
#define	_PMCLOG_TO_HEADER(T,L)						\
	((PMCLOG_HEADER_MAGIC << 24) |					\
	 (PMCLOG_TYPE_ ## T << 16)   |					\
	 ((L) & 0xFFFF))

/* reserve LEN bytes of space and initialize the entry header */
#define	_PMCLOG_RESERVE(PO,TYPE,LEN,ACTION) do {			\
		uint32_t *_le;						\
		int _len = roundup((LEN), sizeof(uint32_t));		\
		if ((_le = pmclog_reserve((PO), _len)) == NULL) {	\
			ACTION;						\
		}							\
		*_le = _PMCLOG_TO_HEADER(TYPE,_len);			\
		_le += 3	/* skip over timestamp */

#define	PMCLOG_RESERVE(P,T,L)		_PMCLOG_RESERVE(P,T,L,return)
#define	PMCLOG_RESERVE_WITH_ERROR(P,T,L) _PMCLOG_RESERVE(P,T,L,		\
	error=ENOMEM;goto error)

#define	PMCLOG_EMIT32(V)	do { *_le++ = (V); } while (0)
#define	PMCLOG_EMIT64(V)	do { 					\
		*_le++ = (uint32_t) ((V) & 0xFFFFFFFF);			\
		*_le++ = (uint32_t) (((V) >> 32) & 0xFFFFFFFF);		\
	} while (0)


/* Emit a string.  Caution: does NOT update _le, so needs to be last */
#define	PMCLOG_EMITSTRING(S,L)	do { bcopy((S), _le, (L)); } while (0)

#define	PMCLOG_DESPATCH(PO)						\
		pmclog_release((PO));					\
	} while (0)


/*
 * Assertions about the log file format.
 */

CTASSERT(sizeof(struct pmclog_callchain) == 6*4 +
    PMC_CALLCHAIN_DEPTH_MAX*sizeof(uintfptr_t));
CTASSERT(sizeof(struct pmclog_closelog) == 3*4);
CTASSERT(sizeof(struct pmclog_dropnotify) == 3*4);
CTASSERT(sizeof(struct pmclog_map_in) == PATH_MAX +
    4*4 + sizeof(uintfptr_t));
CTASSERT(offsetof(struct pmclog_map_in,pl_pathname) ==
    4*4 + sizeof(uintfptr_t));
CTASSERT(sizeof(struct pmclog_map_out) == 4*4 + 2*sizeof(uintfptr_t));
CTASSERT(sizeof(struct pmclog_pcsample) == 6*4 + sizeof(uintfptr_t));
CTASSERT(sizeof(struct pmclog_pmcallocate) == 6*4);
CTASSERT(sizeof(struct pmclog_pmcattach) == 5*4 + PATH_MAX);
CTASSERT(offsetof(struct pmclog_pmcattach,pl_pathname) == 5*4);
CTASSERT(sizeof(struct pmclog_pmcdetach) == 5*4);
CTASSERT(sizeof(struct pmclog_proccsw) == 5*4 + 8);
CTASSERT(sizeof(struct pmclog_procexec) == 5*4 + PATH_MAX +
    sizeof(uintfptr_t));
CTASSERT(offsetof(struct pmclog_procexec,pl_pathname) == 5*4 +
    sizeof(uintfptr_t));
CTASSERT(sizeof(struct pmclog_procexit) == 5*4 + 8);
CTASSERT(sizeof(struct pmclog_procfork) == 5*4);
CTASSERT(sizeof(struct pmclog_sysexit) == 4*4);
CTASSERT(sizeof(struct pmclog_userdata) == 4*4);

/*
 * Log buffer structure
 */

struct pmclog_buffer {
	TAILQ_ENTRY(pmclog_buffer) plb_next;
	char 		*plb_base;
	char		*plb_ptr;
	char 		*plb_fence;
};

/*
 * Prototypes
 */

static int pmclog_get_buffer(struct pmc_owner *po);
static void pmclog_loop(void *arg);
static void pmclog_release(struct pmc_owner *po);
static uint32_t *pmclog_reserve(struct pmc_owner *po, int length);
static void pmclog_schedule_io(struct pmc_owner *po);
static void pmclog_stop_kthread(struct pmc_owner *po);

/*
 * Helper functions
 */

/*
 * Get a log buffer
 */

static int
pmclog_get_buffer(struct pmc_owner *po)
{
	struct pmclog_buffer *plb;

	mtx_assert(&po->po_mtx, MA_OWNED);

	KASSERT(po->po_curbuf == NULL,
	    ("[pmclog,%d] po=%p current buffer still valid", __LINE__, po));

	mtx_lock_spin(&pmc_bufferlist_mtx);
	if ((plb = TAILQ_FIRST(&pmc_bufferlist)) != NULL)
		TAILQ_REMOVE(&pmc_bufferlist, plb, plb_next);
	mtx_unlock_spin(&pmc_bufferlist_mtx);

	PMCDBG(LOG,GTB,1, "po=%p plb=%p", po, plb);

#ifdef	DEBUG
	if (plb)
		KASSERT(plb->plb_ptr == plb->plb_base &&
		    plb->plb_base < plb->plb_fence,
		    ("[pmclog,%d] po=%p buffer invariants: ptr=%p "
		    "base=%p fence=%p", __LINE__, po, plb->plb_ptr,
		    plb->plb_base, plb->plb_fence));
#endif

	po->po_curbuf = plb;

	/* update stats */
	atomic_add_int(&pmc_stats.pm_buffer_requests, 1);
	if (plb == NULL)
		atomic_add_int(&pmc_stats.pm_buffer_requests_failed, 1);

	return (plb ? 0 : ENOMEM);
}

/*
 * Log handler loop.
 *
 * This function is executed by each pmc owner's helper thread.
 */

static void
pmclog_loop(void *arg)
{
	int error;
	struct pmc_owner *po;
	struct pmclog_buffer *lb;
	struct proc *p;
	struct ucred *ownercred;
	struct ucred *mycred;
	struct thread *td;
	struct uio auio;
	struct iovec aiov;
	size_t nbytes;

	po = (struct pmc_owner *) arg;
	p = po->po_owner;
	td = curthread;
	mycred = td->td_ucred;

	PROC_LOCK(p);
	ownercred = crhold(p->p_ucred);
	PROC_UNLOCK(p);

	PMCDBG(LOG,INI,1, "po=%p kt=%p", po, po->po_kthread);
	KASSERT(po->po_kthread == curthread->td_proc,
	    ("[pmclog,%d] proc mismatch po=%p po/kt=%p curproc=%p", __LINE__,
		po, po->po_kthread, curthread->td_proc));

	lb = NULL;


	/*
	 * Loop waiting for I/O requests to be added to the owner
	 * struct's queue.  The loop is exited when the log file
	 * is deconfigured.
	 */

	mtx_lock(&pmc_kthread_mtx);

	for (;;) {

		/* check if we've been asked to exit */
		if ((po->po_flags & PMC_PO_OWNS_LOGFILE) == 0)
			break;

		if (lb == NULL) { /* look for a fresh buffer to write */
			mtx_lock_spin(&po->po_mtx);
			if ((lb = TAILQ_FIRST(&po->po_logbuffers)) == NULL) {
				mtx_unlock_spin(&po->po_mtx);

				if (po->po_flags & PMC_PO_SHUTDOWN) {
					mtx_unlock(&pmc_kthread_mtx);
					/*
			 		 * Close the file to get PMCLOG_EOF
					 * error in pmclog(3).
					 */
					fo_close(po->po_file, curthread);
					mtx_lock(&pmc_kthread_mtx);
				}

				(void) msleep(po, &pmc_kthread_mtx, PWAIT,
				    "pmcloop", 0);
				continue;
			}

			TAILQ_REMOVE(&po->po_logbuffers, lb, plb_next);
			mtx_unlock_spin(&po->po_mtx);
		}

		mtx_unlock(&pmc_kthread_mtx);

		/* process the request */
		PMCDBG(LOG,WRI,2, "po=%p base=%p ptr=%p", po,
		    lb->plb_base, lb->plb_ptr);
		/* change our thread's credentials before issuing the I/O */

		aiov.iov_base = lb->plb_base;
		aiov.iov_len  = nbytes = lb->plb_ptr - lb->plb_base;

		auio.uio_iov    = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = -1;
		auio.uio_resid  = nbytes;
		auio.uio_rw     = UIO_WRITE;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_td     = td;

		/* switch thread credentials -- see kern_ktrace.c */
		td->td_ucred = ownercred;
		error = fo_write(po->po_file, &auio, ownercred, 0, td);
		td->td_ucred = mycred;

		if (error) {
			/* XXX some errors are recoverable */
			/* send a SIGIO to the owner and exit */
			PROC_LOCK(p);
			kern_psignal(p, SIGIO);
			PROC_UNLOCK(p);

			mtx_lock(&pmc_kthread_mtx);

			po->po_error = error; /* save for flush log */

			PMCDBG(LOG,WRI,2, "po=%p error=%d", po, error);

			break;
		}

		mtx_lock(&pmc_kthread_mtx);

		/* put the used buffer back into the global pool */
		PMCLOG_INIT_BUFFER_DESCRIPTOR(lb);

		mtx_lock_spin(&pmc_bufferlist_mtx);
		TAILQ_INSERT_HEAD(&pmc_bufferlist, lb, plb_next);
		mtx_unlock_spin(&pmc_bufferlist_mtx);

		lb = NULL;
	}

	po->po_kthread = NULL;

	mtx_unlock(&pmc_kthread_mtx);

	/* return the current I/O buffer to the global pool */
	if (lb) {
		PMCLOG_INIT_BUFFER_DESCRIPTOR(lb);

		mtx_lock_spin(&pmc_bufferlist_mtx);
		TAILQ_INSERT_HEAD(&pmc_bufferlist, lb, plb_next);
		mtx_unlock_spin(&pmc_bufferlist_mtx);
	}

	/*
	 * Exit this thread, signalling the waiter
	 */

	crfree(ownercred);

	kproc_exit(0);
}

/*
 * Release and log entry and schedule an I/O if needed.
 */

static void
pmclog_release(struct pmc_owner *po)
{
	KASSERT(po->po_curbuf->plb_ptr >= po->po_curbuf->plb_base,
	    ("[pmclog,%d] buffer invariants po=%p ptr=%p base=%p", __LINE__,
		po, po->po_curbuf->plb_ptr, po->po_curbuf->plb_base));
	KASSERT(po->po_curbuf->plb_ptr <= po->po_curbuf->plb_fence,
	    ("[pmclog,%d] buffer invariants po=%p ptr=%p fenc=%p", __LINE__,
		po, po->po_curbuf->plb_ptr, po->po_curbuf->plb_fence));

	/* schedule an I/O if we've filled a buffer */
	if (po->po_curbuf->plb_ptr >= po->po_curbuf->plb_fence)
		pmclog_schedule_io(po);

	mtx_unlock_spin(&po->po_mtx);

	PMCDBG(LOG,REL,1, "po=%p", po);
}


/*
 * Attempt to reserve 'length' bytes of space in an owner's log
 * buffer.  The function returns a pointer to 'length' bytes of space
 * if there was enough space or returns NULL if no space was
 * available.  Non-null returns do so with the po mutex locked.  The
 * caller must invoke pmclog_release() on the pmc owner structure
 * when done.
 */

static uint32_t *
pmclog_reserve(struct pmc_owner *po, int length)
{
	uintptr_t newptr, oldptr;
	uint32_t *lh;
	struct timespec ts;

	PMCDBG(LOG,ALL,1, "po=%p len=%d", po, length);

	KASSERT(length % sizeof(uint32_t) == 0,
	    ("[pmclog,%d] length not a multiple of word size", __LINE__));

	mtx_lock_spin(&po->po_mtx);

	/* No more data when shutdown in progress. */
	if (po->po_flags & PMC_PO_SHUTDOWN) {
		mtx_unlock_spin(&po->po_mtx);
		return (NULL);
	}

	if (po->po_curbuf == NULL)
		if (pmclog_get_buffer(po) != 0) {
			mtx_unlock_spin(&po->po_mtx);
			return (NULL);
		}

	KASSERT(po->po_curbuf != NULL,
	    ("[pmclog,%d] po=%p no current buffer", __LINE__, po));

	KASSERT(po->po_curbuf->plb_ptr >= po->po_curbuf->plb_base &&
	    po->po_curbuf->plb_ptr <= po->po_curbuf->plb_fence,
	    ("[pmclog,%d] po=%p buffer invariants: ptr=%p base=%p fence=%p",
		__LINE__, po, po->po_curbuf->plb_ptr, po->po_curbuf->plb_base,
		po->po_curbuf->plb_fence));

	oldptr = (uintptr_t) po->po_curbuf->plb_ptr;
	newptr = oldptr + length;

	KASSERT(oldptr != (uintptr_t) NULL,
	    ("[pmclog,%d] po=%p Null log buffer pointer", __LINE__, po));

	/*
	 * If we have space in the current buffer, return a pointer to
	 * available space with the PO structure locked.
	 */
	if (newptr <= (uintptr_t) po->po_curbuf->plb_fence) {
		po->po_curbuf->plb_ptr = (char *) newptr;
		goto done;
	}

	/*
	 * Otherwise, schedule the current buffer for output and get a
	 * fresh buffer.
	 */
	pmclog_schedule_io(po);

	if (pmclog_get_buffer(po) != 0) {
		mtx_unlock_spin(&po->po_mtx);
		return (NULL);
	}

	KASSERT(po->po_curbuf != NULL,
	    ("[pmclog,%d] po=%p no current buffer", __LINE__, po));

	KASSERT(po->po_curbuf->plb_ptr != NULL,
	    ("[pmclog,%d] null return from pmc_get_log_buffer", __LINE__));

	KASSERT(po->po_curbuf->plb_ptr == po->po_curbuf->plb_base &&
	    po->po_curbuf->plb_ptr <= po->po_curbuf->plb_fence,
	    ("[pmclog,%d] po=%p buffer invariants: ptr=%p base=%p fence=%p",
		__LINE__, po, po->po_curbuf->plb_ptr, po->po_curbuf->plb_base,
		po->po_curbuf->plb_fence));

	oldptr = (uintptr_t) po->po_curbuf->plb_ptr;

 done:
	lh = (uint32_t *) oldptr;
	lh++;				/* skip header */
	getnanotime(&ts);		/* fill in the timestamp */
	*lh++ = ts.tv_sec & 0xFFFFFFFF;
	*lh++ = ts.tv_nsec & 0xFFFFFFF;
	return ((uint32_t *) oldptr);
}

/*
 * Schedule an I/O.
 *
 * Transfer the current buffer to the helper kthread.
 */

static void
pmclog_schedule_io(struct pmc_owner *po)
{
	KASSERT(po->po_curbuf != NULL,
	    ("[pmclog,%d] schedule_io with null buffer po=%p", __LINE__, po));

	KASSERT(po->po_curbuf->plb_ptr >= po->po_curbuf->plb_base,
	    ("[pmclog,%d] buffer invariants po=%p ptr=%p base=%p", __LINE__,
		po, po->po_curbuf->plb_ptr, po->po_curbuf->plb_base));
	KASSERT(po->po_curbuf->plb_ptr <= po->po_curbuf->plb_fence,
	    ("[pmclog,%d] buffer invariants po=%p ptr=%p fenc=%p", __LINE__,
		po, po->po_curbuf->plb_ptr, po->po_curbuf->plb_fence));

	PMCDBG(LOG,SIO, 1, "po=%p", po);

	mtx_assert(&po->po_mtx, MA_OWNED);

	/*
	 * Add the current buffer to the tail of the buffer list and
	 * wakeup the helper.
	 */
	TAILQ_INSERT_TAIL(&po->po_logbuffers, po->po_curbuf, plb_next);
	po->po_curbuf = NULL;
	wakeup_one(po);
}

/*
 * Stop the helper kthread.
 */

static void
pmclog_stop_kthread(struct pmc_owner *po)
{
	/*
	 * Close the file to force the thread out of fo_write,
	 * unset flag, wakeup the helper thread,
	 * wait for it to exit
	 */

	if (po->po_file != NULL)
		fo_close(po->po_file, curthread);

	mtx_lock(&pmc_kthread_mtx);
	po->po_flags &= ~PMC_PO_OWNS_LOGFILE;
	wakeup_one(po);
	if (po->po_kthread)
		msleep(po->po_kthread, &pmc_kthread_mtx, PPAUSE, "pmckstp", 0);
	mtx_unlock(&pmc_kthread_mtx);
}

/*
 * Public functions
 */

/*
 * Configure a log file for pmc owner 'po'.
 *
 * Parameter 'logfd' is a file handle referencing an open file in the
 * owner process.  This file needs to have been opened for writing.
 */

int
pmclog_configure_log(struct pmc_mdep *md, struct pmc_owner *po, int logfd)
{
	int error;
	struct proc *p;

	/*
	 * As long as it is possible to get a LOR between pmc_sx lock and
	 * proctree/allproc sx locks used for adding a new process, assure
	 * the former is not held here.
	 */
	sx_assert(&pmc_sx, SA_UNLOCKED);
	PMCDBG(LOG,CFG,1, "config po=%p logfd=%d", po, logfd);

	p = po->po_owner;

	/* return EBUSY if a log file was already present */
	if (po->po_flags & PMC_PO_OWNS_LOGFILE)
		return (EBUSY);

	KASSERT(po->po_kthread == NULL,
	    ("[pmclog,%d] po=%p kthread (%p) already present", __LINE__, po,
		po->po_kthread));
	KASSERT(po->po_file == NULL,
	    ("[pmclog,%d] po=%p file (%p) already present", __LINE__, po,
		po->po_file));

	/* get a reference to the file state */
	error = fget_write(curthread, logfd, CAP_WRITE, &po->po_file);
	if (error)
		goto error;

	/* mark process as owning a log file */
	po->po_flags |= PMC_PO_OWNS_LOGFILE;
	error = kproc_create(pmclog_loop, po, &po->po_kthread,
	    RFHIGHPID, 0, "hwpmc: proc(%d)", p->p_pid);
	if (error)
		goto error;

	/* mark process as using HWPMCs */
	PROC_LOCK(p);
	p->p_flag |= P_HWPMC;
	PROC_UNLOCK(p);

	/* create a log initialization entry */
	PMCLOG_RESERVE_WITH_ERROR(po, INITIALIZE,
	    sizeof(struct pmclog_initialize));
	PMCLOG_EMIT32(PMC_VERSION);
	PMCLOG_EMIT32(md->pmd_cputype);
	PMCLOG_DESPATCH(po);

	return (0);

 error:
	/* shutdown the thread */
	if (po->po_kthread)
		pmclog_stop_kthread(po);

	KASSERT(po->po_kthread == NULL, ("[pmclog,%d] po=%p kthread not "
	    "stopped", __LINE__, po));

	if (po->po_file)
		(void) fdrop(po->po_file, curthread);
	po->po_file  = NULL;	/* clear file and error state */
	po->po_error = 0;

	return (error);
}


/*
 * De-configure a log file.  This will throw away any buffers queued
 * for this owner process.
 */

int
pmclog_deconfigure_log(struct pmc_owner *po)
{
	int error;
	struct pmclog_buffer *lb;

	PMCDBG(LOG,CFG,1, "de-config po=%p", po);

	if ((po->po_flags & PMC_PO_OWNS_LOGFILE) == 0)
		return (EINVAL);

	KASSERT(po->po_sscount == 0,
	    ("[pmclog,%d] po=%p still owning SS PMCs", __LINE__, po));
	KASSERT(po->po_file != NULL,
	    ("[pmclog,%d] po=%p no log file", __LINE__, po));

	/* stop the kthread, this will reset the 'OWNS_LOGFILE' flag */
	if (po->po_kthread)
		pmclog_stop_kthread(po);

	KASSERT(po->po_kthread == NULL,
	    ("[pmclog,%d] po=%p kthread not stopped", __LINE__, po));

	/* return all queued log buffers to the global pool */
	while ((lb = TAILQ_FIRST(&po->po_logbuffers)) != NULL) {
		TAILQ_REMOVE(&po->po_logbuffers, lb, plb_next);
		PMCLOG_INIT_BUFFER_DESCRIPTOR(lb);
		mtx_lock_spin(&pmc_bufferlist_mtx);
		TAILQ_INSERT_HEAD(&pmc_bufferlist, lb, plb_next);
		mtx_unlock_spin(&pmc_bufferlist_mtx);
	}

	/* return the 'current' buffer to the global pool */
	if ((lb = po->po_curbuf) != NULL) {
		PMCLOG_INIT_BUFFER_DESCRIPTOR(lb);
		mtx_lock_spin(&pmc_bufferlist_mtx);
		TAILQ_INSERT_HEAD(&pmc_bufferlist, lb, plb_next);
		mtx_unlock_spin(&pmc_bufferlist_mtx);
	}

	/* drop a reference to the fd */
	error = fdrop(po->po_file, curthread);
	po->po_file  = NULL;
	po->po_error = 0;

	return (error);
}

/*
 * Flush a process' log buffer.
 */

int
pmclog_flush(struct pmc_owner *po)
{
	int error;
	struct pmclog_buffer *lb;

	PMCDBG(LOG,FLS,1, "po=%p", po);

	/*
	 * If there is a pending error recorded by the logger thread,
	 * return that.
	 */
	if (po->po_error)
		return (po->po_error);

	error = 0;

	/*
	 * Check that we do have an active log file.
	 */
	mtx_lock(&pmc_kthread_mtx);
	if ((po->po_flags & PMC_PO_OWNS_LOGFILE) == 0) {
		error = EINVAL;
		goto error;
	}

	/*
	 * Schedule the current buffer if any and not empty.
	 */
	mtx_lock_spin(&po->po_mtx);
	lb = po->po_curbuf;
	if (lb && lb->plb_ptr != lb->plb_base) {
		pmclog_schedule_io(po);
	} else
		error = ENOBUFS;
	mtx_unlock_spin(&po->po_mtx);

 error:
	mtx_unlock(&pmc_kthread_mtx);

	return (error);
}

int
pmclog_close(struct pmc_owner *po)
{

	PMCDBG(LOG,CLO,1, "po=%p", po);

	mtx_lock(&pmc_kthread_mtx);

	/*
	 * Schedule the current buffer.
	 */
	mtx_lock_spin(&po->po_mtx);
	if (po->po_curbuf)
		pmclog_schedule_io(po);
	else
		wakeup_one(po);
	mtx_unlock_spin(&po->po_mtx);

	/*
	 * Initiate shutdown: no new data queued,
	 * thread will close file on last block.
	 */
	po->po_flags |= PMC_PO_SHUTDOWN;

	mtx_unlock(&pmc_kthread_mtx);

	return (0);
}

void
pmclog_process_callchain(struct pmc *pm, struct pmc_sample *ps)
{
	int n, recordlen;
	uint32_t flags;
	struct pmc_owner *po;

	PMCDBG(LOG,SAM,1,"pm=%p pid=%d n=%d", pm, ps->ps_pid,
	    ps->ps_nsamples);

	recordlen = offsetof(struct pmclog_callchain, pl_pc) +
	    ps->ps_nsamples * sizeof(uintfptr_t);
	po = pm->pm_owner;
	flags = PMC_CALLCHAIN_TO_CPUFLAGS(ps->ps_cpu,ps->ps_flags);
	PMCLOG_RESERVE(po, CALLCHAIN, recordlen);
	PMCLOG_EMIT32(ps->ps_pid);
	PMCLOG_EMIT32(pm->pm_id);
	PMCLOG_EMIT32(flags);
	for (n = 0; n < ps->ps_nsamples; n++)
		PMCLOG_EMITADDR(ps->ps_pc[n]);
	PMCLOG_DESPATCH(po);
}

void
pmclog_process_closelog(struct pmc_owner *po)
{
	PMCLOG_RESERVE(po,CLOSELOG,sizeof(struct pmclog_closelog));
	PMCLOG_DESPATCH(po);
}

void
pmclog_process_dropnotify(struct pmc_owner *po)
{
	PMCLOG_RESERVE(po,DROPNOTIFY,sizeof(struct pmclog_dropnotify));
	PMCLOG_DESPATCH(po);
}

void
pmclog_process_map_in(struct pmc_owner *po, pid_t pid, uintfptr_t start,
    const char *path)
{
	int pathlen, recordlen;

	KASSERT(path != NULL, ("[pmclog,%d] map-in, null path", __LINE__));

	pathlen = strlen(path) + 1;	/* #bytes for path name */
	recordlen = offsetof(struct pmclog_map_in, pl_pathname) +
	    pathlen;

	PMCLOG_RESERVE(po, MAP_IN, recordlen);
	PMCLOG_EMIT32(pid);
	PMCLOG_EMITADDR(start);
	PMCLOG_EMITSTRING(path,pathlen);
	PMCLOG_DESPATCH(po);
}

void
pmclog_process_map_out(struct pmc_owner *po, pid_t pid, uintfptr_t start,
    uintfptr_t end)
{
	KASSERT(start <= end, ("[pmclog,%d] start > end", __LINE__));

	PMCLOG_RESERVE(po, MAP_OUT, sizeof(struct pmclog_map_out));
	PMCLOG_EMIT32(pid);
	PMCLOG_EMITADDR(start);
	PMCLOG_EMITADDR(end);
	PMCLOG_DESPATCH(po);
}

void
pmclog_process_pmcallocate(struct pmc *pm)
{
	struct pmc_owner *po;

	po = pm->pm_owner;

	PMCDBG(LOG,ALL,1, "pm=%p", pm);

	PMCLOG_RESERVE(po, PMCALLOCATE, sizeof(struct pmclog_pmcallocate));
	PMCLOG_EMIT32(pm->pm_id);
	PMCLOG_EMIT32(pm->pm_event);
	PMCLOG_EMIT32(pm->pm_flags);
	PMCLOG_DESPATCH(po);
}

void
pmclog_process_pmcattach(struct pmc *pm, pid_t pid, char *path)
{
	int pathlen, recordlen;
	struct pmc_owner *po;

	PMCDBG(LOG,ATT,1,"pm=%p pid=%d", pm, pid);

	po = pm->pm_owner;

	pathlen = strlen(path) + 1;	/* #bytes for the string */
	recordlen = offsetof(struct pmclog_pmcattach, pl_pathname) + pathlen;

	PMCLOG_RESERVE(po, PMCATTACH, recordlen);
	PMCLOG_EMIT32(pm->pm_id);
	PMCLOG_EMIT32(pid);
	PMCLOG_EMITSTRING(path, pathlen);
	PMCLOG_DESPATCH(po);
}

void
pmclog_process_pmcdetach(struct pmc *pm, pid_t pid)
{
	struct pmc_owner *po;

	PMCDBG(LOG,ATT,1,"!pm=%p pid=%d", pm, pid);

	po = pm->pm_owner;

	PMCLOG_RESERVE(po, PMCDETACH, sizeof(struct pmclog_pmcdetach));
	PMCLOG_EMIT32(pm->pm_id);
	PMCLOG_EMIT32(pid);
	PMCLOG_DESPATCH(po);
}

/*
 * Log a context switch event to the log file.
 */

void
pmclog_process_proccsw(struct pmc *pm, struct pmc_process *pp, pmc_value_t v)
{
	struct pmc_owner *po;

	KASSERT(pm->pm_flags & PMC_F_LOG_PROCCSW,
	    ("[pmclog,%d] log-process-csw called gratuitously", __LINE__));

	PMCDBG(LOG,SWO,1,"pm=%p pid=%d v=%jx", pm, pp->pp_proc->p_pid,
	    v);

	po = pm->pm_owner;

	PMCLOG_RESERVE(po, PROCCSW, sizeof(struct pmclog_proccsw));
	PMCLOG_EMIT32(pm->pm_id);
	PMCLOG_EMIT64(v);
	PMCLOG_EMIT32(pp->pp_proc->p_pid);
	PMCLOG_DESPATCH(po);
}

void
pmclog_process_procexec(struct pmc_owner *po, pmc_id_t pmid, pid_t pid,
    uintfptr_t startaddr, char *path)
{
	int pathlen, recordlen;

	PMCDBG(LOG,EXC,1,"po=%p pid=%d path=\"%s\"", po, pid, path);

	pathlen   = strlen(path) + 1;	/* #bytes for the path */
	recordlen = offsetof(struct pmclog_procexec, pl_pathname) + pathlen;

	PMCLOG_RESERVE(po, PROCEXEC, recordlen);
	PMCLOG_EMIT32(pid);
	PMCLOG_EMITADDR(startaddr);
	PMCLOG_EMIT32(pmid);
	PMCLOG_EMITSTRING(path,pathlen);
	PMCLOG_DESPATCH(po);
}

/*
 * Log a process exit event (and accumulated pmc value) to the log file.
 */

void
pmclog_process_procexit(struct pmc *pm, struct pmc_process *pp)
{
	int ri;
	struct pmc_owner *po;

	ri = PMC_TO_ROWINDEX(pm);
	PMCDBG(LOG,EXT,1,"pm=%p pid=%d v=%jx", pm, pp->pp_proc->p_pid,
	    pp->pp_pmcs[ri].pp_pmcval);

	po = pm->pm_owner;

	PMCLOG_RESERVE(po, PROCEXIT, sizeof(struct pmclog_procexit));
	PMCLOG_EMIT32(pm->pm_id);
	PMCLOG_EMIT64(pp->pp_pmcs[ri].pp_pmcval);
	PMCLOG_EMIT32(pp->pp_proc->p_pid);
	PMCLOG_DESPATCH(po);
}

/*
 * Log a fork event.
 */

void
pmclog_process_procfork(struct pmc_owner *po, pid_t oldpid, pid_t newpid)
{
	PMCLOG_RESERVE(po, PROCFORK, sizeof(struct pmclog_procfork));
	PMCLOG_EMIT32(oldpid);
	PMCLOG_EMIT32(newpid);
	PMCLOG_DESPATCH(po);
}

/*
 * Log a process exit event of the form suitable for system-wide PMCs.
 */

void
pmclog_process_sysexit(struct pmc_owner *po, pid_t pid)
{
	PMCLOG_RESERVE(po, SYSEXIT, sizeof(struct pmclog_sysexit));
	PMCLOG_EMIT32(pid);
	PMCLOG_DESPATCH(po);
}

/*
 * Write a user log entry.
 */

int
pmclog_process_userlog(struct pmc_owner *po, struct pmc_op_writelog *wl)
{
	int error;

	PMCDBG(LOG,WRI,1, "writelog po=%p ud=0x%x", po, wl->pm_userdata);

	error = 0;

	PMCLOG_RESERVE_WITH_ERROR(po, USERDATA,
	    sizeof(struct pmclog_userdata));
	PMCLOG_EMIT32(wl->pm_userdata);
	PMCLOG_DESPATCH(po);

 error:
	return (error);
}

/*
 * Initialization.
 *
 * Create a pool of log buffers and initialize mutexes.
 */

void
pmclog_initialize()
{
	int n;
	struct pmclog_buffer *plb;

	if (pmclog_buffer_size <= 0) {
		(void) printf("hwpmc: tunable logbuffersize=%d must be "
		    "greater than zero.\n", pmclog_buffer_size);
		pmclog_buffer_size = PMC_LOG_BUFFER_SIZE;
	}

	if (pmc_nlogbuffers <= 0) {
		(void) printf("hwpmc: tunable nlogbuffers=%d must be greater "
		    "than zero.\n", pmc_nlogbuffers);
		pmc_nlogbuffers = PMC_NLOGBUFFERS;
	}

	/* create global pool of log buffers */
	for (n = 0; n < pmc_nlogbuffers; n++) {
		plb = malloc(1024 * pmclog_buffer_size, M_PMC,
		    M_WAITOK|M_ZERO);
		PMCLOG_INIT_BUFFER_DESCRIPTOR(plb);
		TAILQ_INSERT_HEAD(&pmc_bufferlist, plb, plb_next);
	}
	mtx_init(&pmc_bufferlist_mtx, "pmc-buffer-list", "pmc-leaf",
	    MTX_SPIN);
	mtx_init(&pmc_kthread_mtx, "pmc-kthread", "pmc-sleep", MTX_DEF);
}

/*
 * Shutdown logging.
 *
 * Destroy mutexes and release memory back the to free pool.
 */

void
pmclog_shutdown()
{
	struct pmclog_buffer *plb;

	mtx_destroy(&pmc_kthread_mtx);
	mtx_destroy(&pmc_bufferlist_mtx);

	while ((plb = TAILQ_FIRST(&pmc_bufferlist)) != NULL) {
		TAILQ_REMOVE(&pmc_bufferlist, plb, plb_next);
		free(plb, M_PMC);
	}
}
