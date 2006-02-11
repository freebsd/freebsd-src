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
#include <bsm/audit_kevents.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>

#include <security/audit/audit.h>
#include <security/audit/audit_private.h>

#include <vm/uma.h>

/*
 * The AUDIT_EXCESSIVELY_VERBOSE define enables a number of
 * gratuitously noisy printf's to the console.  Due to the
 * volume, it should be left off unless you want your system
 * to churn a lot whenever the audit record flow gets high.
 */
//#define	AUDIT_EXCESSIVELY_VERBOSE
#ifdef AUDIT_EXCESSIVELY_VERBOSE
#define	AUDIT_PRINTF(x)	printf x
#else
#define	AUDIT_PRINTF(X)
#endif

static uma_zone_t audit_record_zone;
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
int					audit_enabled;
int					audit_suspended;

/*
 * Flags controlling behavior in low storage situations.
 * Should we panic if a write fails?  Should we fail stop
 * if we're out of disk space?
 */
int					audit_panic_on_write_fail;
int					audit_fail_stop;

/*
 * Are we currently "failing stop" due to out of disk space?
 */
static int				 audit_in_failure;

/*
 * Global audit statistiscs. 
 */
struct audit_fstat 			audit_fstat;

/*
 * Preselection mask for non-attributable events.
 */
struct au_mask			 	audit_nae_mask;

/*
 * Mutex to protect global variables shared between various threads and
 * processes.
 */
static struct mtx			audit_mtx;

/*
 * Queue of audit records ready for delivery to disk.  We insert new
 * records at the tail, and remove records from the head.  Also,
 * a count of the number of records used for checking queue depth.
 * In addition, a counter of records that we have allocated but are
 * not yet in the queue, which is needed to estimate the total
 * size of the combined set of records outstanding in the system.
 */
static TAILQ_HEAD(, kaudit_record)	audit_q;
static int				audit_q_len;
static int				audit_pre_q_len;

/*
 * Audit queue control settings (minimum free, low/high water marks, etc.)
 */
struct au_qctrl				audit_qctrl;

/*
 * Condition variable to signal to the worker that it has work to do:
 * either new records are in the queue, or a log replacement is taking
 * place.
 */
static struct cv			audit_cv;

/*
 * Worker thread that will schedule disk I/O, etc.
 */  
static struct proc			*audit_thread;

/*
 * When an audit log is rotated, the actual rotation must be performed
 * by the audit worker thread, as it may have outstanding writes on the
 * current audit log.  audit_replacement_vp holds the vnode replacing
 * the current vnode.  We can't let more than one replacement occur
 * at a time, so if more than one thread requests a replacement, only
 * one can have the replacement "in progress" at any given moment.  If
 * a thread tries to replace the audit vnode and discovers a replacement
 * is already in progress (i.e., audit_replacement_flag != 0), then it
 * will sleep on audit_replacement_cv waiting its turn to perform a
 * replacement.  When a replacement is completed, this cv is signalled
 * by the worker thread so a waiting thread can start another replacement.
 * We also store a credential to perform audit log write operations with.
 *
 * The current credential and vnode are thread-local to audit_worker.
 */
static struct cv			audit_replacement_cv;

static int				audit_replacement_flag;
static struct vnode			*audit_replacement_vp;
static struct ucred			*audit_replacement_cred;

/*
 * Condition variable to signal to the worker that it has work to do:
 * either new records are in the queue, or a log replacement is taking
 * place.
 */
static struct cv			audit_commit_cv;

/* 
 * Condition variable for  auditing threads wait on when in fail-stop mode. 
 * Threads wait on this CV forever (and ever), never seeing the light of 
 * day again.
 */
static struct cv			audit_fail_cv;

/*
 * Flags related to Kernel->user-space communication.
 */
static int			audit_file_rotate_wait;

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
	 *
	 * XXXAUDIT: td_ucred access is OK without proc lock, but some other
	 * fields here may require the proc lock.
	 */
	cru2x(td->td_ucred, &ar->k_ar.ar_subj_cred);
	ar->k_ar.ar_subj_ruid = td->td_ucred->cr_ruid;
	ar->k_ar.ar_subj_rgid = td->td_ucred->cr_rgid;
	ar->k_ar.ar_subj_egid = td->td_ucred->cr_groups[0];
	ar->k_ar.ar_subj_auid = td->td_proc->p_au->ai_auid;
	ar->k_ar.ar_subj_asid = td->td_proc->p_au->ai_asid;
	ar->k_ar.ar_subj_pid = td->td_proc->p_pid;
	ar->k_ar.ar_subj_amask = td->td_proc->p_au->ai_mask;
	ar->k_ar.ar_subj_term = td->td_proc->p_au->ai_termid;
	bcopy(td->td_proc->p_comm, ar->k_ar.ar_subj_comm, MAXCOMLEN);

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
}

/*
 * XXXAUDIT: Should adjust comments below to make it clear that we get to
 * this point only if we believe we have storage, so not having space here
 * is a violation of invariants derived from administrative procedures.
 * I.e., someone else has written to the audit partition, leaving less space
 * than we accounted for.
 */
static int
audit_record_write(struct vnode *vp, struct kaudit_record *ar, 
    struct ucred *cred, struct thread *td)
{
	int ret;
	long temp;
	struct au_record *bsm;
	struct vattr vattr;
	struct statfs *mnt_stat = &vp->v_mount->mnt_stat;
	int vfslocked;

	vfslocked = VFS_LOCK_GIANT(vp->v_mount);

	/*
	 * First, gather statistics on the audit log file and file system
	 * so that we know how we're doing on space.  In both cases,
	 * if we're unable to perform the operation, we drop the record
	 * and return.  However, this is arguably an assertion failure.
	 * XXX Need a FreeBSD equivalent.
	 */
	ret = VFS_STATFS(vp->v_mount, mnt_stat, td);
	if (ret)
		goto out;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	ret = VOP_GETATTR(vp, &vattr, cred, td);
	VOP_UNLOCK(vp, 0, td);
	if (ret)
		goto out;

	/* update the global stats struct */
	audit_fstat.af_currsz = vattr.va_size; 

	/*
	 * XXX Need to decide what to do if the trigger to the audit daemon
	 * fails.
	 */

	/* 
	 * If we fall below minimum free blocks (hard limit), tell the audit
	 * daemon to force a rotation off of the file system. We also stop
	 * writing, which means this audit record is probably lost.
	 * If we fall below the minimum percent free blocks (soft limit), 
	 * then kindly suggest to the audit daemon to do something.
	 */
	if (mnt_stat->f_bfree < AUDIT_HARD_LIMIT_FREE_BLOCKS) {
		send_trigger(AUDIT_TRIGGER_NO_SPACE);
		/* Hopefully userspace did something about all the previous
		 * triggers that were sent prior to this critical condition.
		 * If fail-stop is set, then we're done; goodnight Gracie.
		 */
		if (audit_fail_stop)
			panic("Audit log space exhausted and fail-stop set.");
		else {
			audit_suspended = 1;
			ret = ENOSPC;
			goto out;
		}
	} else
		/* 
		 * Send a message to the audit daemon that disk space 
		 * is getting low.
		 *
		 * XXXAUDIT: Check math and block size calculation here.
		 */
		if (audit_qctrl.aq_minfree != 0) {
			temp = mnt_stat->f_blocks / (100 / 
			    audit_qctrl.aq_minfree);
			if (mnt_stat->f_bfree < temp)
				send_trigger(AUDIT_TRIGGER_LOW_SPACE);
		}

	/* Check if the current log file is full; if so, call for
	 * a log rotate. This is not an exact comparison; we may
	 * write some records over the limit. If that's not
	 * acceptable, then add a fudge factor here.
	 */
	if ((audit_fstat.af_filesz != 0) &&
	    (audit_file_rotate_wait == 0) && 
	    (vattr.va_size >= audit_fstat.af_filesz)) {
		audit_file_rotate_wait = 1;
		send_trigger(AUDIT_TRIGGER_OPEN_NEW);
	}

	/*
	 * If the estimated amount of audit data in the audit event queue
	 * (plus records allocated but not yet queued) has reached the
	 * amount of free space on the disk, then we need to go into an
	 * audit fail stop state, in which we do not permit the
	 * allocation/committing of any new audit records.  We continue to
	 * process packets but don't allow any activities that might
	 * generate new records.  In the future, we might want to detect
	 * when space is available again and allow operation to continue,
	 * but this behavior is sufficient to meet fail stop requirements
	 * in CAPP.
	 */
	if (audit_fail_stop &&
	    (unsigned long)
	    ((audit_q_len + audit_pre_q_len + 1) * MAX_AUDIT_RECORD_SIZE) /
	    mnt_stat->f_bsize >= (unsigned long)(mnt_stat->f_bfree)) {
		printf(
    "audit_worker: free space below size of audit queue, failing stop\n");
		audit_in_failure = 1;
	}

	/* 
	 * If there is a user audit record attached to the kernel record,
	 * then write the user record.
	 */
	/* XXX Need to decide a few things here: IF the user audit 
	 * record is written, but the write of the kernel record fails,
	 * what to do? Should the kernel record come before or after the
	 * user record? For now, we write the user record first, and
	 * we ignore errors.
	 */
	if (ar->k_ar_commit & AR_COMMIT_USER) {
		/*
		 * Try submitting the record to any active audit pipes.
		 */
		audit_pipe_submit((void *)ar->k_udata, ar->k_ulen);

		/*
		 * And to disk.
		 */
		ret = vn_rdwr(UIO_WRITE, vp, (void *)ar->k_udata, ar->k_ulen,
		          (off_t)0, UIO_SYSSPACE, IO_APPEND|IO_UNIT, cred, NULL,
			  NULL, td); 
		if (ret)
			goto out;
	}

	/* 
	 * Convert the internal kernel record to BSM format and write it
	 * out if everything's OK.
	 */
	if (!(ar->k_ar_commit & AR_COMMIT_KERNEL)) {
		ret = 0;
		goto out;
	}

	/*
	 * XXXAUDIT: Should we actually allow this conversion to fail?  With
	 * sleeping memory allocation and invariants checks, perhaps not.
	 */
	ret = kaudit_to_bsm(ar, &bsm);
	if (ret == BSM_NOAUDIT) {
		ret = 0;
		goto out;
	}

	/*
	 * XXX: We drop the record on BSM conversion failure, but really
	 * this is an assertion failure.
	 */
	if (ret == BSM_FAILURE) {
		AUDIT_PRINTF(("BSM conversion failure\n"));
		ret = EINVAL;
		goto out;
	}

	/*
	 * Try submitting the record to any active audit pipes.
	 */
	audit_pipe_submit((void *)bsm->data, bsm->len);
	
	/*
	 * XXX
	 * We should break the write functionality away from the BSM record
	 * generation and have the BSM generation done before this function
	 * is called. This function will then take the BSM record as a
	 * parameter.
	 */
	ret = (vn_rdwr(UIO_WRITE, vp, (void *)bsm->data, bsm->len,
	    (off_t)0, UIO_SYSSPACE, IO_APPEND|IO_UNIT, cred, NULL, NULL, td));

	kau_free(bsm);

out:
	/*
	 * When we're done processing the current record, we have to
	 * check to see if we're in a failure mode, and if so, whether
	 * this was the last record left to be drained.  If we're done
	 * draining, then we fsync the vnode and panic.
	 */
	if (audit_in_failure &&
	    audit_q_len == 0 && audit_pre_q_len == 0) {
		VOP_LOCK(vp, LK_DRAIN | LK_INTERLOCK, td);
		(void)VOP_FSYNC(vp, MNT_WAIT, td);
		VOP_UNLOCK(vp, 0, td);
		panic("Audit store overflow; record queue drained.");
	}

	VFS_UNLOCK_GIANT(vfslocked);

	return (ret);
}

/*
 * The audit_worker thread is responsible for watching the event queue,
 * dequeueing records, converting them to BSM format, and committing them to
 * disk.  In order to minimize lock thrashing, records are dequeued in sets
 * to a thread-local work queue.  In addition, the audit_work performs the
 * actual exchange of audit log vnode pointer, as audit_vp is a thread-local
 * variable.
 */
static void
audit_worker(void *arg)
{
	int do_replacement_signal, error;
	TAILQ_HEAD(, kaudit_record) ar_worklist;
	struct kaudit_record *ar;
	struct vnode *audit_vp, *old_vp;
	int vfslocked;

	struct ucred *audit_cred, *old_cred;
	struct thread *audit_td;

	AUDIT_PRINTF(("audit_worker starting\n"));

	/*
	 * These are thread-local variables requiring no synchronization.
	 */
	TAILQ_INIT(&ar_worklist);
	audit_cred = NULL;
	audit_td = curthread;
	audit_vp = NULL;

	mtx_lock(&audit_mtx);
	while (1) {
		/*
		 * First priority: replace the audit log target if requested.
		 * Accessing the vnode here requires dropping the audit_mtx;
		 * in case another replacement was scheduled while the mutex
		 * was released, we loop.
		 *
		 * XXX It could well be we should drain existing records
		 * first to ensure that the timestamps and ordering
		 * are right.
		 */
		do_replacement_signal = 0;
		while (audit_replacement_flag != 0) {
			old_cred = audit_cred;
			old_vp = audit_vp;
			audit_cred = audit_replacement_cred;
			audit_vp = audit_replacement_vp;
			audit_replacement_cred = NULL;
			audit_replacement_vp = NULL;
			audit_replacement_flag = 0;

			audit_enabled = (audit_vp != NULL);

			/*
			 * XXX: What to do about write failures here?
			 */
			if (old_vp != NULL) {
				AUDIT_PRINTF(("Closing old audit file\n"));
				mtx_unlock(&audit_mtx);
				vfslocked = VFS_LOCK_GIANT(old_vp->v_mount);
				vn_close(old_vp, AUDIT_CLOSE_FLAGS, old_cred,
				    audit_td);
				VFS_UNLOCK_GIANT(vfslocked);
				crfree(old_cred);
				mtx_lock(&audit_mtx);
				old_cred = NULL;
				old_vp = NULL;
				AUDIT_PRINTF(("Audit file closed\n"));
			}
			if (audit_vp != NULL) {
				AUDIT_PRINTF(("Opening new audit file\n"));
			}
			do_replacement_signal = 1;
		}
		/*
		 * Signal that replacement have occurred to wake up and
		 * start any other replacements started in parallel.  We can
		 * continue about our business in the mean time.  We
		 * broadcast so that both new replacements can be inserted,
		 * but also so that the source(s) of replacement can return
		 * successfully.
		 */
		if (do_replacement_signal)
			cv_broadcast(&audit_replacement_cv);

		/*
		 * Next, check to see if we have any records to drain into
		 * the vnode.  If not, go back to waiting for an event.
		 */
		if (TAILQ_EMPTY(&audit_q)) {
			AUDIT_PRINTF(("audit_worker waiting\n"));
			cv_wait(&audit_cv, &audit_mtx);
			AUDIT_PRINTF(("audit_worker woken up\n"));
	AUDIT_PRINTF(("audit_worker: new vp = %p; value of flag %d\n",
	    audit_replacement_vp, audit_replacement_flag));
			continue;
		}

		/*
		 * If we have records, but there's no active vnode to write
		 * to, drain the record queue.  Generally, we prevent the
		 * unnecessary allocation of records elsewhere, but we need
		 * to allow for races between conditional allocation and
		 * queueing.  Go back to waiting when we're done.
		 */
		if (audit_vp == NULL) {
			while ((ar = TAILQ_FIRST(&audit_q))) {
				TAILQ_REMOVE(&audit_q, ar, k_q);
				uma_zfree(audit_record_zone, ar);
				audit_q_len--;
				/*
				 * XXXRW: Why broadcast if we hold the
				 * mutex and know that audit_vp is NULL?
				 */
				if (audit_q_len <= audit_qctrl.aq_lowater)
					cv_broadcast(&audit_commit_cv);
			}
			continue;
		}

		/*
		 * We have both records to write and an active vnode to write
		 * to.  Dequeue a record, and start the write.  Eventually,
		 * it might make sense to dequeue several records and perform
		 * our own clustering, if the lower layers aren't doing it
		 * automatically enough.
		 */
		while ((ar = TAILQ_FIRST(&audit_q))) {
			TAILQ_REMOVE(&audit_q, ar, k_q);
			audit_q_len--;
			if (audit_q_len <= audit_qctrl.aq_lowater)
				cv_broadcast(&audit_commit_cv);
			TAILQ_INSERT_TAIL(&ar_worklist, ar, k_q);
		}

		mtx_unlock(&audit_mtx);
		while ((ar = TAILQ_FIRST(&ar_worklist))) {
			TAILQ_REMOVE(&ar_worklist, ar, k_q);
			if (audit_vp != NULL) {
				error = audit_record_write(audit_vp, ar, 
				    audit_cred, audit_td);
				if (error && audit_panic_on_write_fail)
					panic("audit_worker: write error %d\n",
					    error);
				else if (error)
					printf("audit_worker: write error %d\n",
					    error);
			}
			uma_zfree(audit_record_zone, ar);
		}
		mtx_lock(&audit_mtx);
	}
}

/*
 * Initialize the Audit subsystem: configuration state, work queue,
 * synchronization primitives, worker thread, and trigger device node.  Also
 * call into the BSM assembly code to initialize it.
 */
static void
audit_init(void)
{
	int error;

	printf("Security auditing service present\n");
	audit_enabled = 0;
	audit_suspended = 0;
	audit_panic_on_write_fail = 0;
	audit_fail_stop = 0;
	audit_in_failure = 0;

	audit_replacement_vp = NULL;
	audit_replacement_cred = NULL;
	audit_replacement_flag = 0;

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
	cv_init(&audit_cv, "audit_cv");
	cv_init(&audit_replacement_cv, "audit_replacement_cv");
	cv_init(&audit_commit_cv, "audit_commit_cv");
	cv_init(&audit_fail_cv, "audit_fail_cv");

	audit_record_zone = uma_zcreate("audit_record_zone",
	    sizeof(struct kaudit_record), audit_record_ctor,
	    audit_record_dtor, NULL, NULL, UMA_ALIGN_PTR, 0);

	/* Initialize the BSM audit subsystem. */
	kau_init();

	audit_file_rotate_wait = 0;
	audit_trigger_init();

	/* Register shutdown handler. */
	EVENTHANDLER_REGISTER(shutdown_pre_sync, audit_shutdown, NULL,
	    SHUTDOWN_PRI_FIRST);

	error = kthread_create(audit_worker, NULL, &audit_thread, RFHIGHPID,
	    0, "audit_worker");
	if (error != 0)
		panic("audit_init: kthread_create returned %d", error);
}

SYSINIT(audit_init, SI_SUB_AUDIT, SI_ORDER_FIRST, audit_init, NULL)

/*
 * audit_rotate_vnode() is called by a user or kernel thread to configure or
 * de-configure auditing on a vnode.  The arguments are the replacement
 * credential and vnode to substitute for the current credential and vnode,
 * if any.  If either is set to NULL, both should be NULL, and this is used
 * to indicate that audit is being disabled.  The real work is done in the
 * audit_worker thread, but audit_rotate_vnode() waits synchronously for that
 * to complete.
 *
 * The vnode should be referenced and opened by the caller.  The credential
 * should be referenced.  audit_rotate_vnode() will own both references as of
 * this call, so the caller should not release either.
 *
 * XXXAUDIT: Review synchronize communication logic.  Really, this is a
 * message queue of depth 1.
 *
 * XXXAUDIT: Enhance the comments below to indicate that we are basically
 * acquiring ownership of the communications queue, inserting our message,
 * and waiting for an acknowledgement.
 */
void
audit_rotate_vnode(struct ucred *cred, struct vnode *vp)
{

	/*
	 * If other parallel log replacements have been requested, we wait
	 * until they've finished before continuing.
	 */
	mtx_lock(&audit_mtx);
	while (audit_replacement_flag != 0) {
		AUDIT_PRINTF(("audit_rotate_vnode: sleeping to wait for "
		    "flag\n"));
		cv_wait(&audit_replacement_cv, &audit_mtx);
		AUDIT_PRINTF(("audit_rotate_vnode: woken up (flag %d)\n",
		    audit_replacement_flag));
	}
	audit_replacement_cred = cred;
	audit_replacement_flag = 1;
	audit_replacement_vp = vp;

	/*
	 * Wake up the audit worker to perform the exchange once we
	 * release the mutex.
	 */
	cv_signal(&audit_cv);

	/*
	 * Wait for the audit_worker to broadcast that a replacement has
	 * taken place; we know that once this has happened, our vnode
	 * has been replaced in, so we can return successfully.
	 */
	AUDIT_PRINTF(("audit_rotate_vnode: waiting for news of "
	    "replacement\n"));
	cv_wait(&audit_replacement_cv, &audit_mtx);
	AUDIT_PRINTF(("audit_rotate_vnode: change acknowledged by "
	    "audit_worker (flag " "now %d)\n", audit_replacement_flag));
	mtx_unlock(&audit_mtx);

	audit_file_rotate_wait = 0; /* We can now request another rotation */
}

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
__inline__ struct kaudit_record *
currecord(void)
{

	return (curthread->td_ar);
}

/*
 * MPSAFE
 *
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
	 * XXX: The number of outstanding uncommitted audit records is
	 * limited to the number of concurrent threads servicing system
	 * calls in the kernel.
	 */
	ar = uma_zalloc_arg(audit_record_zone, td, M_WAITOK);
	ar->k_ar.ar_event = event;

	mtx_lock(&audit_mtx);
	audit_pre_q_len++;
	mtx_unlock(&audit_mtx);

	return (ar);
}

/*
 * MPSAFE
 */
void
audit_commit(struct kaudit_record *ar, int error, int retval)
{
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

	if (au_preselect(ar->k_ar.ar_event, aumask, sorf) != 0)
		ar->k_ar_commit |= AR_COMMIT_KERNEL;

	/*
	 * XXXRW: Why is this necessary?  Should we ever accept a record that
	 * we're not willing to commit?
	 */
	if ((ar->k_ar_commit & (AR_COMMIT_USER | AR_COMMIT_KERNEL)) == 0) {
		mtx_lock(&audit_mtx);
		audit_pre_q_len--;
		mtx_unlock(&audit_mtx);
		uma_zfree(audit_record_zone, ar);
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
		uma_zfree(audit_record_zone, ar);
		return;
	}
	
	/*
	 * Constrain the number of committed audit records based on
	 * the configurable parameter.
	 */
	while (audit_q_len >= audit_qctrl.aq_hiwater) {
		AUDIT_PRINTF(("audit_commit: sleeping to wait for "
		   "audit queue to drain below high water mark\n"));
		cv_wait(&audit_commit_cv, &audit_mtx);
		AUDIT_PRINTF(("audit_commit: woke up waiting for "
		   "audit queue draining\n"));
	}

	TAILQ_INSERT_TAIL(&audit_q, ar, k_q);
	audit_q_len++;
	audit_pre_q_len--;
	cv_signal(&audit_cv);
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
	int audit_event;
	struct au_mask *aumask;

	KASSERT(td->td_ar == NULL, ("audit_syscall_enter: td->td_ar != NULL"));

	/*
	 * In FreeBSD, each ABI has its own system call table, and hence
	 * mapping of system call codes to audit events.  Convert the code to
	 * an audit event identifier using the process system call table
	 * reference.  In Darwin, there's only one, so we use the global
	 * symbol for the system call table.
	 *
	 * XXXAUDIT: Should we audit that a bad system call was made, and if
	 * so, how?
	 */
	if (code >= td->td_proc->p_sysent->sv_size)
		return;

	audit_event = td->td_proc->p_sysent->sv_table[code].sy_auevent;
	if (audit_event == AUE_NULL)
		return;

	/*
	 * Check which audit mask to use; either the kernel non-attributable
	 * event mask or the process audit mask.
	 */
	if (td->td_proc->p_au->ai_auid == AU_DEFAUDITID)
		aumask = &audit_nae_mask;
	else
		aumask = &td->td_proc->p_au->ai_mask;
	
	/*
	 * Allocate an audit record, if preselection allows it, and store 
	 * in the thread for later use.
	 */
	if (au_preselect(audit_event, aumask,
			AU_PRS_FAILURE | AU_PRS_SUCCESS)) {
		/*
		 * If we're out of space and need to suspend unprivileged
		 * processes, do that here rather than trying to allocate
		 * another audit record.
		 *
		 * XXXRW: We might wish to be able to continue here in the
		 * future, if the system recovers.  That should be possible
		 * by means of checking the condition in a loop around
		 * cv_wait().  It might be desirable to reevaluate whether an
		 * audit record is still required for this event by
		 * re-calling au_preselect().
		 */
		if (audit_in_failure && suser(td) != 0) {
			cv_wait(&audit_fail_cv, &audit_mtx);
			panic("audit_failing_stop: thread continued");
		}
		td->td_ar = audit_new(audit_event, td);
	} else
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
	/* XXXAUDIT: Zero?  Slab allocate? */
	//printf("audit_proc_alloc: pid %d p_au %p\n", p->p_pid, p->p_au);
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
 * Initialize the audit information for the a process, presumably the first 
 * process in the system.
 * XXX It is not clear what the initial values should be for audit ID, 
 * session ID, etc. 
 */
void
audit_proc_kproc0(struct proc *p)
{

	KASSERT(p->p_au != NULL, ("audit_proc_kproc0: p->p_au == NULL (%d)",
	    p->p_pid));
	//printf("audit_proc_kproc0: pid %d p_au %p\n", p->p_pid, p->p_au);
	bzero(p->p_au, sizeof(*(p)->p_au));
}

void
audit_proc_init(struct proc *p)
{

	KASSERT(p->p_au != NULL, ("audit_proc_init: p->p_au == NULL (%d)",
	    p->p_pid));
	//printf("audit_proc_init: pid %d p_au %p\n", p->p_pid, p->p_au);
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
	//printf("audit_proc_fork: parent pid %d p_au %p\n", parent->p_pid,
	//    parent->p_au);
	//printf("audit_proc_fork: child pid %d p_au %p\n", child->p_pid,
	//    child->p_au);
	bcopy(parent->p_au, child->p_au, sizeof(*child->p_au));
	/*
	 * XXXAUDIT: Zero pointers to external memory, or assert they are
	 * zero?
	 */
}

/*
 * Free the auditing structure for the process. 
 */
void
audit_proc_free(struct proc *p)
{

	KASSERT(p->p_au != NULL, ("p->p_au == NULL (%d)", p->p_pid));
	//printf("audit_proc_free: pid %d p_au %p\n", p->p_pid, p->p_au);
	/*
	 * XXXAUDIT: Assert that external memory pointers are NULL?
	 */
	free(p->p_au, M_AUDITPROC);
	p->p_au = NULL;
}
