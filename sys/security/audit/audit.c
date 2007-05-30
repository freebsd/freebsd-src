/*
 * Copyright (c) 1999-2005 Apple Computer, Inc.
 * Copyright (c) 2006 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/ipc.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <bsm/audit.h>
#include <bsm/audit_internal.h>
#include <bsm/audit_kevents.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>

#include <security/audit/audit.h>
#include <security/audit/audit_private.h>

#include <vm/uma.h>

static uma_zone_t	audit_record_zone;
static MALLOC_DEFINE(M_AUDITPROC, "audit_proc", "Audit process storage");
MALLOC_DEFINE(M_AUDITDATA, "audit_data", "Audit data storage");
MALLOC_DEFINE(M_AUDITPATH, "audit_path", "Audit path storage");
MALLOC_DEFINE(M_AUDITTEXT, "audit_text", "Audit text storage");

/*
 * Audit control settings that are set/read by system calls and are
 * hence non-static.
 */
/*
 * Define the audit control flags.
 */
int			audit_enabled;
int			audit_suspended;

/*
 * Flags controlling behavior in low storage situations.  Should we panic if
 * a write fails?  Should we fail stop if we're out of disk space?
 */
int			audit_panic_on_write_fail;
int			audit_fail_stop;
int			audit_argv;
int			audit_arge;

/*
 * Are we currently "failing stop" due to out of disk space?
 */
int			audit_in_failure;

/*
 * Global audit statistiscs.
 */
struct audit_fstat	audit_fstat;

/*
 * Preselection mask for non-attributable events.
 */
struct au_mask		audit_nae_mask;

/*
 * Mutex to protect global variables shared between various threads and
 * processes.
 */
struct mtx		audit_mtx;

/*
 * Queue of audit records ready for delivery to disk.  We insert new
 * records at the tail, and remove records from the head.  Also,
 * a count of the number of records used for checking queue depth.
 * In addition, a counter of records that we have allocated but are
 * not yet in the queue, which is needed to estimate the total
 * size of the combined set of records outstanding in the system.
 */
struct kaudit_queue	audit_q;
int			audit_q_len;
int			audit_pre_q_len;

/*
 * Audit queue control settings (minimum free, low/high water marks, etc.)
 */
struct au_qctrl		audit_qctrl;

/*
 * Condition variable to signal to the worker that it has work to do:
 * either new records are in the queue, or a log replacement is taking
 * place.
 */
struct cv		audit_worker_cv;

/*
 * Condition variable to flag when crossing the low watermark, meaning that
 * threads blocked due to hitting the high watermark can wake up and continue
 * to commit records.
 */
struct cv		audit_watermark_cv;

/*
 * Condition variable for  auditing threads wait on when in fail-stop mode.
 * Threads wait on this CV forever (and ever), never seeing the light of
 * day again.
 */
static struct cv	audit_fail_cv;

/*
 * Construct an audit record for the passed thread.
 */
static int
audit_record_ctor(void *mem, int size, void *arg, int flags)
{
	struct kaudit_record *ar;
	struct thread *td;

	KASSERT(sizeof(*ar) == size, ("audit_record_ctor: wrong size"));

	td = arg;
	ar = mem;
	bzero(ar, sizeof(*ar));
	ar->k_ar.ar_magic = AUDIT_RECORD_MAGIC;
	nanotime(&ar->k_ar.ar_starttime);

	/*
	 * Export the subject credential.
	 */
	cru2x(td->td_ucred, &ar->k_ar.ar_subj_cred);
	ar->k_ar.ar_subj_ruid = td->td_ucred->cr_ruid;
	ar->k_ar.ar_subj_rgid = td->td_ucred->cr_rgid;
	ar->k_ar.ar_subj_egid = td->td_ucred->cr_groups[0];
	PROC_LOCK(td->td_proc);
	ar->k_ar.ar_subj_auid = td->td_proc->p_au->ai_auid;
	ar->k_ar.ar_subj_asid = td->td_proc->p_au->ai_asid;
	ar->k_ar.ar_subj_pid = td->td_proc->p_pid;
	ar->k_ar.ar_subj_amask = td->td_proc->p_au->ai_mask;
	ar->k_ar.ar_subj_term_addr = td->td_proc->p_au->ai_termid;
	PROC_UNLOCK(td->td_proc);

	return (0);
}

static void
audit_record_dtor(void *mem, int size, void *arg)
{
	struct kaudit_record *ar;

	KASSERT(sizeof(*ar) == size, ("audit_record_dtor: wrong size"));

	ar = mem;
	if (ar->k_ar.ar_arg_upath1 != NULL)
		free(ar->k_ar.ar_arg_upath1, M_AUDITPATH);
	if (ar->k_ar.ar_arg_upath2 != NULL)
		free(ar->k_ar.ar_arg_upath2, M_AUDITPATH);
	if (ar->k_ar.ar_arg_text != NULL)
		free(ar->k_ar.ar_arg_text, M_AUDITTEXT);
	if (ar->k_udata != NULL)
		free(ar->k_udata, M_AUDITDATA);
	if (ar->k_ar.ar_arg_argv != NULL)
		free(ar->k_ar.ar_arg_argv, M_AUDITTEXT);
	if (ar->k_ar.ar_arg_envv != NULL)
		free(ar->k_ar.ar_arg_envv, M_AUDITTEXT);
}

/*
 * Initialize the Audit subsystem: configuration state, work queue,
 * synchronization primitives, worker thread, and trigger device node.  Also
 * call into the BSM assembly code to initialize it.
 */
static void
audit_init(void)
{

	printf("Security auditing service present\n");
	audit_enabled = 0;
	audit_suspended = 0;
	audit_panic_on_write_fail = 0;
	audit_fail_stop = 0;
	audit_in_failure = 0;
	audit_argv = 0;
	audit_arge = 0;

	audit_fstat.af_filesz = 0;	/* '0' means unset, unbounded */
	audit_fstat.af_currsz = 0;
	audit_nae_mask.am_success = AU_NULL;
	audit_nae_mask.am_failure = AU_NULL;

	TAILQ_INIT(&audit_q);
	audit_q_len = 0;
	audit_pre_q_len = 0;
	audit_qctrl.aq_hiwater = AQ_HIWATER;
	audit_qctrl.aq_lowater = AQ_LOWATER;
	audit_qctrl.aq_bufsz = AQ_BUFSZ;
	audit_qctrl.aq_minfree = AU_FS_MINFREE;

	mtx_init(&audit_mtx, "audit_mtx", NULL, MTX_DEF);
	cv_init(&audit_worker_cv, "audit_worker_cv");
	cv_init(&audit_watermark_cv, "audit_watermark_cv");
	cv_init(&audit_fail_cv, "audit_fail_cv");

	audit_record_zone = uma_zcreate("audit_record",
	    sizeof(struct kaudit_record), audit_record_ctor,
	    audit_record_dtor, NULL, NULL, UMA_ALIGN_PTR, 0);

	/* Initialize the BSM audit subsystem. */
	kau_init();

	audit_trigger_init();

	/* Register shutdown handler. */
	EVENTHANDLER_REGISTER(shutdown_pre_sync, audit_shutdown, NULL,
	    SHUTDOWN_PRI_FIRST);

	/* Start audit worker thread. */
	audit_worker_init();
}

SYSINIT(audit_init, SI_SUB_AUDIT, SI_ORDER_FIRST, audit_init, NULL)

/*
 * Drain the audit queue and close the log at shutdown.  Note that this can
 * be called both from the system shutdown path and also from audit
 * configuration syscalls, so 'arg' and 'howto' are ignored.
 */
void
audit_shutdown(void *arg, int howto)
{

	audit_rotate_vnode(NULL, NULL);
}

/*
 * Return the current thread's audit record, if any.
 */
struct kaudit_record *
currecord(void)
{

	return (curthread->td_ar);
}

/*
 * XXXAUDIT: There are a number of races present in the code below due to
 * release and re-grab of the mutex.  The code should be revised to become
 * slightly less racy.
 *
 * XXXAUDIT: Shouldn't there be logic here to sleep waiting on available
 * pre_q space, suspending the system call until there is room?
 */
struct kaudit_record *
audit_new(int event, struct thread *td)
{
	struct kaudit_record *ar;
	int no_record;

	mtx_lock(&audit_mtx);
	no_record = (audit_suspended || !audit_enabled);
	mtx_unlock(&audit_mtx);
	if (no_record)
		return (NULL);

	/*
	 * Note: the number of outstanding uncommitted audit records is
	 * limited to the number of concurrent threads servicing system calls
	 * in the kernel.
	 */
	ar = uma_zalloc_arg(audit_record_zone, td, M_WAITOK);
	ar->k_ar.ar_event = event;

	mtx_lock(&audit_mtx);
	audit_pre_q_len++;
	mtx_unlock(&audit_mtx);

	return (ar);
}

void
audit_free(struct kaudit_record *ar)
{

	uma_zfree(audit_record_zone, ar);
}

void
audit_commit(struct kaudit_record *ar, int error, int retval)
{
	au_event_t event;
	au_class_t class;
	au_id_t auid;
	int sorf;
	struct au_mask *aumask;

	if (ar == NULL)
		return;

	/*
	 * Decide whether to commit the audit record by checking the
	 * error value from the system call and using the appropriate
	 * audit mask.
	 *
	 * XXXAUDIT: Synchronize access to audit_nae_mask?
	 */
	if (ar->k_ar.ar_subj_auid == AU_DEFAUDITID)
		aumask = &audit_nae_mask;
	else
		aumask = &ar->k_ar.ar_subj_amask;

	if (error)
		sorf = AU_PRS_FAILURE;
	else
		sorf = AU_PRS_SUCCESS;

	switch(ar->k_ar.ar_event) {

	case AUE_OPEN_RWTC:
		/* The open syscall always writes a AUE_OPEN_RWTC event; change
		 * it to the proper type of event based on the flags and the
		 * error value.
		 */
		ar->k_ar.ar_event = flags_and_error_to_openevent(
		    ar->k_ar.ar_arg_fflags, error);
		break;

	case AUE_SYSCTL:
		ar->k_ar.ar_event = ctlname_to_sysctlevent(
		    ar->k_ar.ar_arg_ctlname, ar->k_ar.ar_valid_arg);
		break;

	case AUE_AUDITON:
		/* Convert the auditon() command to an event */
		ar->k_ar.ar_event = auditon_command_event(ar->k_ar.ar_arg_cmd);
		break;
	}

	auid = ar->k_ar.ar_subj_auid;
	event = ar->k_ar.ar_event;
	class = au_event_class(event);

	ar->k_ar_commit |= AR_COMMIT_KERNEL;
	if (au_preselect(event, class, aumask, sorf) != 0)
		ar->k_ar_commit |= AR_PRESELECT_TRAIL;
	if (audit_pipe_preselect(auid, event, class, sorf,
	    ar->k_ar_commit & AR_PRESELECT_TRAIL) != 0)
		ar->k_ar_commit |= AR_PRESELECT_PIPE;
	if ((ar->k_ar_commit & (AR_PRESELECT_TRAIL | AR_PRESELECT_PIPE |
	    AR_PRESELECT_USER_TRAIL | AR_PRESELECT_USER_PIPE)) == 0) {
		mtx_lock(&audit_mtx);
		audit_pre_q_len--;
		mtx_unlock(&audit_mtx);
		audit_free(ar);
		return;
	}

	ar->k_ar.ar_errno = error;
	ar->k_ar.ar_retval = retval;

	/*
	 * We might want to do some system-wide post-filtering
	 * here at some point.
	 */

	/*
	 * Timestamp system call end.
	 */
	nanotime(&ar->k_ar.ar_endtime);

	mtx_lock(&audit_mtx);

	/*
	 * Note: it could be that some records initiated while audit was
	 * enabled should still be committed?
	 */
	if (audit_suspended || !audit_enabled) {
		audit_pre_q_len--;
		mtx_unlock(&audit_mtx);
		audit_free(ar);
		return;
	}

	/*
	 * Constrain the number of committed audit records based on
	 * the configurable parameter.
	 */
	while (audit_q_len >= audit_qctrl.aq_hiwater) {
		AUDIT_PRINTF(("audit_commit: sleeping to wait for "
		   "audit queue to drain below high water mark\n"));
		cv_wait(&audit_watermark_cv, &audit_mtx);
		AUDIT_PRINTF(("audit_commit: woke up waiting for "
		   "audit queue draining\n"));
	}

	TAILQ_INSERT_TAIL(&audit_q, ar, k_q);
	audit_q_len++;
	audit_pre_q_len--;
	cv_signal(&audit_worker_cv);
	mtx_unlock(&audit_mtx);
}

/*
 * audit_syscall_enter() is called on entry to each system call.  It is
 * responsible for deciding whether or not to audit the call (preselection),
 * and if so, allocating a per-thread audit record.  audit_new() will fill in
 * basic thread/credential properties.
 */
void
audit_syscall_enter(unsigned short code, struct thread *td)
{
	struct au_mask *aumask;
	au_class_t class;
	au_event_t event;
	au_id_t auid;

	KASSERT(td->td_ar == NULL, ("audit_syscall_enter: td->td_ar != NULL"));

	/*
	 * In FreeBSD, each ABI has its own system call table, and hence
	 * mapping of system call codes to audit events.  Convert the code to
	 * an audit event identifier using the process system call table
	 * reference.  In Darwin, there's only one, so we use the global
	 * symbol for the system call table.  No audit record is generated
	 * for bad system calls, as no operation has been performed.
	 */
	if (code >= td->td_proc->p_sysent->sv_size)
		return;

	event = td->td_proc->p_sysent->sv_table[code].sy_auevent;
	if (event == AUE_NULL)
		return;

	/*
	 * Check which audit mask to use; either the kernel non-attributable
	 * event mask or the process audit mask.
	 */
	auid = td->td_proc->p_au->ai_auid;
	if (auid == AU_DEFAUDITID)
		aumask = &audit_nae_mask;
	else
		aumask = &td->td_proc->p_au->ai_mask;

	/*
	 * Allocate an audit record, if preselection allows it, and store
	 * in the thread for later use.
	 */
	class = au_event_class(event);
	if (au_preselect(event, class, aumask, AU_PRS_BOTH)) {
		/*
		 * If we're out of space and need to suspend unprivileged
		 * processes, do that here rather than trying to allocate
		 * another audit record.
		 *
		 * Note: we might wish to be able to continue here in the
		 * future, if the system recovers.  That should be possible
		 * by means of checking the condition in a loop around
		 * cv_wait().  It might be desirable to reevaluate whether an
		 * audit record is still required for this event by
		 * re-calling au_preselect().
		 */
		if (audit_in_failure &&
		    priv_check(td, PRIV_AUDIT_FAILSTOP) != 0) {
			cv_wait(&audit_fail_cv, &audit_mtx);
			panic("audit_failing_stop: thread continued");
		}
		td->td_ar = audit_new(event, td);
	} else if (audit_pipe_preselect(auid, event, class, AU_PRS_BOTH, 0))
		td->td_ar = audit_new(event, td);
	else
		td->td_ar = NULL;
}

/*
 * audit_syscall_exit() is called from the return of every system call, or in
 * the event of exit1(), during the execution of exit1().  It is responsible
 * for committing the audit record, if any, along with return condition.
 */
void
audit_syscall_exit(int error, struct thread *td)
{
	int retval;

	/*
	 * Commit the audit record as desired; once we pass the record
	 * into audit_commit(), the memory is owned by the audit
	 * subsystem.
	 * The return value from the system call is stored on the user
	 * thread. If there was an error, the return value is set to -1,
	 * imitating the behavior of the cerror routine.
	 */
	if (error)
		retval = -1;
	else
		retval = td->td_retval[0];

	audit_commit(td->td_ar, error, retval);
	if (td->td_ar != NULL)
		AUDIT_PRINTF(("audit record committed by pid %d\n",
			td->td_proc->p_pid));
	td->td_ar = NULL;

}

/*
 * Allocate storage for a new process (init, or otherwise).
 */
void
audit_proc_alloc(struct proc *p)
{

	KASSERT(p->p_au == NULL, ("audit_proc_alloc: p->p_au != NULL (%d)",
	    p->p_pid));
	p->p_au = malloc(sizeof(*(p->p_au)), M_AUDITPROC, M_WAITOK);
}

/*
 * Allocate storage for a new thread.
 */
void
audit_thread_alloc(struct thread *td)
{

	td->td_ar = NULL;
}

/*
 * Thread destruction.
 */
void
audit_thread_free(struct thread *td)
{

	KASSERT(td->td_ar == NULL, ("audit_thread_free: td_ar != NULL"));
}

/*
 * Initialize audit information for the first kernel process (proc 0) and for
 * the first user process (init).
 *
 * XXX It is not clear what the initial values should be for audit ID,
 * session ID, etc.
 */
void
audit_proc_kproc0(struct proc *p)
{

	KASSERT(p->p_au != NULL, ("audit_proc_kproc0: p->p_au == NULL (%d)",
	    p->p_pid));
	bzero(p->p_au, sizeof(*(p)->p_au));
}

void
audit_proc_init(struct proc *p)
{

	KASSERT(p->p_au != NULL, ("audit_proc_init: p->p_au == NULL (%d)",
	    p->p_pid));
	bzero(p->p_au, sizeof(*(p)->p_au));
	p->p_au->ai_auid = AU_DEFAUDITID;
}

/*
 * Copy the audit info from the parent process to the child process when
 * a fork takes place.
 */
void
audit_proc_fork(struct proc *parent, struct proc *child)
{

	PROC_LOCK_ASSERT(parent, MA_OWNED);
	PROC_LOCK_ASSERT(child, MA_OWNED);
	KASSERT(parent->p_au != NULL,
	    ("audit_proc_fork: parent->p_au == NULL (%d)", parent->p_pid));
	KASSERT(child->p_au != NULL,
	    ("audit_proc_fork: child->p_au == NULL (%d)", child->p_pid));
	bcopy(parent->p_au, child->p_au, sizeof(*child->p_au));
}

/*
 * Free the auditing structure for the process.
 */
void
audit_proc_free(struct proc *p)
{

	KASSERT(p->p_au != NULL, ("p->p_au == NULL (%d)", p->p_pid));
	free(p->p_au, M_AUDITPROC);
	p->p_au = NULL;
}
