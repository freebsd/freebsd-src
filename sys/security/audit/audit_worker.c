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
#include <bsm/audit_internal.h>
#include <bsm/audit_kevents.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>

#include <security/audit/audit.h>
#include <security/audit/audit_private.h>

#include <vm/uma.h>

/*
 * Worker thread that will schedule disk I/O, etc.
 */
static struct proc		*audit_thread;

/*
 * When an audit log is rotated, the actual rotation must be performed by the
 * audit worker thread, as it may have outstanding writes on the current
 * audit log.  audit_replacement_vp holds the vnode replacing the current
 * vnode.  We can't let more than one replacement occur at a time, so if more
 * than one thread requests a replacement, only one can have the replacement
 * "in progress" at any given moment.  If a thread tries to replace the audit
 * vnode and discovers a replacement is already in progress (i.e.,
 * audit_replacement_flag != 0), then it will sleep on audit_replacement_cv
 * waiting its turn to perform a replacement.  When a replacement is
 * completed, this cv is signalled by the worker thread so a waiting thread
 * can start another replacement.  We also store a credential to perform
 * audit log write operations with.
 *
 * The current credential and vnode are thread-local to audit_worker.
 */
static struct cv		audit_replacement_cv;

static int			audit_replacement_flag;
static struct vnode		*audit_replacement_vp;
static struct ucred		*audit_replacement_cred;

/*
 * Flags related to Kernel->user-space communication.
 */
static int			audit_file_rotate_wait;

/*
 * Write an audit record to a file, performed as the last stage after both
 * preselection and BSM conversion.  Both space management and write failures
 * are handled in this function.
 *
 * No attempt is made to deal with possible failure to deliver a trigger to
 * the audit daemon, since the message is asynchronous anyway.
 */
static void
audit_record_write(struct vnode *vp, struct ucred *cred, struct thread *td,
    void *data, size_t len)
{
	static struct timeval last_lowspace_trigger;
	static struct timeval last_fail;
	static int cur_lowspace_trigger;
	struct statfs *mnt_stat;
	int error, vfslocked;
	static int cur_fail;
	struct vattr vattr;
	long temp;

	if (vp == NULL)
		return;

 	mnt_stat = &vp->v_mount->mnt_stat;
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);

	/*
	 * First, gather statistics on the audit log file and file system so
	 * that we know how we're doing on space.  Consider failure of these
	 * operations to indicate a future inability to write to the file.
	 */
	error = VFS_STATFS(vp->v_mount, mnt_stat, td);
	if (error)
		goto fail;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	error = VOP_GETATTR(vp, &vattr, cred, td);
	VOP_UNLOCK(vp, 0, td);
	if (error)
		goto fail;
	audit_fstat.af_currsz = vattr.va_size;

	/*
	 * We handle four different space-related limits:
	 *
	 * - A fixed (hard) limit on the minimum free blocks we require on
	 *   the file system, and results in record loss, a trigger, and
	 *   possible fail stop due to violating invariants.
	 *
	 * - An administrative (soft) limit, which when fallen below, results
	 *   in the kernel notifying the audit daemon of low space.
	 *
	 * - An audit trail size limit, which when gone above, results in the
	 *   kernel notifying the audit daemon that rotation is desired.
	 *
	 * - The total depth of the kernel audit record exceeding free space,
	 *   which can lead to possible fail stop (with drain), in order to
	 *   prevent violating invariants.  Failure here doesn't halt
	 *   immediately, but prevents new records from being generated.
	 *
	 * Possibly, the last of these should be handled differently, always
	 * allowing a full queue to be lost, rather than trying to prevent
	 * loss.
	 *
	 * First, handle the hard limit, which generates a trigger and may
	 * fail stop.  This is handled in the same manner as ENOSPC from
	 * VOP_WRITE, and results in record loss.
	 */
	if (mnt_stat->f_bfree < AUDIT_HARD_LIMIT_FREE_BLOCKS) {
		error = ENOSPC;
		goto fail_enospc;
	}

	/*
	 * Second, handle falling below the soft limit, if defined; we send
	 * the daemon a trigger and continue processing the record.  Triggers
	 * are limited to 1/sec.
	 */
	if (audit_qctrl.aq_minfree != 0) {
		/*
		 * XXXAUDIT: Check math and block size calculations here.
		 */
		temp = mnt_stat->f_blocks / (100 / audit_qctrl.aq_minfree);
		if (mnt_stat->f_bfree < temp) {
			if (ppsratecheck(&last_lowspace_trigger,
			    &cur_lowspace_trigger, 1)) {
				(void)send_trigger(AUDIT_TRIGGER_LOW_SPACE);
				printf("Warning: audit space low\n");
			}
		}
	}

	/*
	 * If the current file is getting full, generate a rotation trigger
	 * to the daemon.  This is only approximate, which is fine as more
	 * records may be generated before the daemon rotates the file.
	 */
	if ((audit_fstat.af_filesz != 0) && (audit_file_rotate_wait == 0) &&
	    (vattr.va_size >= audit_fstat.af_filesz)) {
		audit_file_rotate_wait = 1;
		(void)send_trigger(AUDIT_TRIGGER_ROTATE_KERNEL);
	}

	/*
	 * If the estimated amount of audit data in the audit event queue
	 * (plus records allocated but not yet queued) has reached the amount
	 * of free space on the disk, then we need to go into an audit fail
	 * stop state, in which we do not permit the allocation/committing of
	 * any new audit records.  We continue to process records but don't
	 * allow any activities that might generate new records.  In the
	 * future, we might want to detect when space is available again and
	 * allow operation to continue, but this behavior is sufficient to
	 * meet fail stop requirements in CAPP.
	 */
	if (audit_fail_stop) {
		if ((unsigned long)((audit_q_len + audit_pre_q_len + 1) *
		    MAX_AUDIT_RECORD_SIZE) / mnt_stat->f_bsize >=
		    (unsigned long)(mnt_stat->f_bfree)) {
			if (ppsratecheck(&last_fail, &cur_fail, 1))
				printf("audit_record_write: free space "
				    "below size of audit queue, failing "
				    "stop\n");
			audit_in_failure = 1;
		} else if (audit_in_failure) {
			/*
			 * Note: if we want to handle recovery, this is the
			 * spot to do it: unset audit_in_failure, and issue a
			 * wakeup on the cv.
			 */
		}
	}

	error = vn_rdwr(UIO_WRITE, vp, data, len, (off_t)0, UIO_SYSSPACE,
	    IO_APPEND|IO_UNIT, cred, NULL, NULL, td);
	if (error == ENOSPC)
		goto fail_enospc;
	else if (error)
		goto fail;

	/*
	 * Catch completion of a queue drain here; if we're draining and the
	 * queue is now empty, fail stop.  That audit_fail_stop is implicitly
	 * true, since audit_in_failure can only be set of audit_fail_stop is
	 * set.
	 *
	 * Note: if we handle recovery from audit_in_failure, then we need to
	 * make panic here conditional.
	 */
	if (audit_in_failure) {
		if (audit_q_len == 0 && audit_pre_q_len == 0) {
			VOP_LOCK(vp, LK_DRAIN | LK_INTERLOCK, td);
			(void)VOP_FSYNC(vp, MNT_WAIT, td);
			VOP_UNLOCK(vp, 0, td);
			panic("Audit store overflow; record queue drained.");
		}
	}

	VFS_UNLOCK_GIANT(vfslocked);
	return;

fail_enospc:
	/*
	 * ENOSPC is considered a special case with respect to failures, as
	 * this can reflect either our preemptive detection of insufficient
	 * space, or ENOSPC returned by the vnode write call.
	 */
	if (audit_fail_stop) {
		VOP_LOCK(vp, LK_DRAIN | LK_INTERLOCK, td);
		(void)VOP_FSYNC(vp, MNT_WAIT, td);
		VOP_UNLOCK(vp, 0, td);
		panic("Audit log space exhausted and fail-stop set.");
	}
	(void)send_trigger(AUDIT_TRIGGER_NO_SPACE);
	audit_suspended = 1;

	/* FALLTHROUGH */
fail:
	/*
	 * We have failed to write to the file, so the current record is
	 * lost, which may require an immediate system halt.
	 */
	if (audit_panic_on_write_fail) {
		VOP_LOCK(vp, LK_DRAIN | LK_INTERLOCK, td);
		(void)VOP_FSYNC(vp, MNT_WAIT, td);
		VOP_UNLOCK(vp, 0, td);
		panic("audit_worker: write error %d\n", error);
	} else if (ppsratecheck(&last_fail, &cur_fail, 1))
		printf("audit_worker: write error %d\n", error);
	VFS_UNLOCK_GIANT(vfslocked);
}

/*
 * If an appropriate signal has been received rotate the audit log based on
 * the global replacement variables.  Signal consumers as needed that the
 * rotation has taken place.
 *
 * The global variables and CVs used to signal the audit_worker to perform a
 * rotation are essentially a message queue of depth 1.  It would be much
 * nicer to actually use a message queue.
 */
static void
audit_worker_rotate(struct ucred **audit_credp, struct vnode **audit_vpp,
    struct thread *audit_td)
{
	int do_replacement_signal, vfslocked;
	struct ucred *old_cred;
	struct vnode *old_vp;

	mtx_assert(&audit_mtx, MA_OWNED);

	do_replacement_signal = 0;
	while (audit_replacement_flag != 0) {
		old_cred = *audit_credp;
		old_vp = *audit_vpp;
		*audit_credp = audit_replacement_cred;
		*audit_vpp = audit_replacement_vp;
		audit_replacement_cred = NULL;
		audit_replacement_vp = NULL;
		audit_replacement_flag = 0;

		audit_enabled = (*audit_vpp != NULL);

		if (old_vp != NULL) {
			mtx_unlock(&audit_mtx);
			vfslocked = VFS_LOCK_GIANT(old_vp->v_mount);
			vn_close(old_vp, AUDIT_CLOSE_FLAGS, old_cred,
			    audit_td);
			VFS_UNLOCK_GIANT(vfslocked);
			crfree(old_cred);
			mtx_lock(&audit_mtx);
			old_cred = NULL;
			old_vp = NULL;
		}
		do_replacement_signal = 1;
	}

	/*
	 * Signal that replacement have occurred to wake up and start any
	 * other replacements started in parallel.  We can continue about our
	 * business in the mean time.  We broadcast so that both new
	 * replacements can be inserted, but also so that the source(s) of
	 * replacement can return successfully.
	 */
	if (do_replacement_signal)
		cv_broadcast(&audit_replacement_cv);
}

/*
 * Given a kernel audit record, process as required.  Kernel audit records
 * are converted to one, or possibly two, BSM records, depending on whether
 * there is a user audit record present also.  Kernel records need be
 * converted to BSM before they can be written out.  Both types will be
 * written to disk, and audit pipes.
 */
static void
audit_worker_process_record(struct vnode *audit_vp, struct ucred *audit_cred,
    struct thread *audit_td, struct kaudit_record *ar)
{
	struct au_record *bsm;
	au_class_t class;
	au_event_t event;
	au_id_t auid;
	int error, sorf;

	/*
	 * First, handle the user record, if any: commit to the system trail
	 * and audit pipes as selected.
	 */
	if ((ar->k_ar_commit & AR_COMMIT_USER) &&
	    (ar->k_ar_commit & AR_PRESELECT_USER_TRAIL))
		audit_record_write(audit_vp, audit_cred, audit_td,
		    ar->k_udata, ar->k_ulen);

	if ((ar->k_ar_commit & AR_COMMIT_USER) &&
	    (ar->k_ar_commit & AR_PRESELECT_USER_PIPE))
		audit_pipe_submit_user(ar->k_udata, ar->k_ulen);

	if (!(ar->k_ar_commit & AR_COMMIT_KERNEL) ||
	    ((ar->k_ar_commit & AR_PRESELECT_PIPE) == 0 &&
	    (ar->k_ar_commit & AR_PRESELECT_TRAIL) == 0))
		return;

	auid = ar->k_ar.ar_subj_auid;
	event = ar->k_ar.ar_event;
	class = au_event_class(event);
	if (ar->k_ar.ar_errno == 0)
		sorf = AU_PRS_SUCCESS;
	else
		sorf = AU_PRS_FAILURE;

	error = kaudit_to_bsm(ar, &bsm);
	switch (error) {
	case BSM_NOAUDIT:
		return;

	case BSM_FAILURE:
		printf("audit_worker_process_record: BSM_FAILURE\n");
		return;

	case BSM_SUCCESS:
		break;

	default:
		panic("kaudit_to_bsm returned %d", error);
	}

	if (ar->k_ar_commit & AR_PRESELECT_TRAIL)
		audit_record_write(audit_vp, audit_cred, audit_td, bsm->data,
		    bsm->len);

	if (ar->k_ar_commit & AR_PRESELECT_PIPE)
		audit_pipe_submit(auid, event, class, sorf,
		    ar->k_ar_commit & AR_PRESELECT_TRAIL, bsm->data,
		    bsm->len);

	kau_free(bsm);
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
	struct kaudit_queue ar_worklist;
	struct kaudit_record *ar;
	struct ucred *audit_cred;
	struct thread *audit_td;
	struct vnode *audit_vp;
	int lowater_signal;

	/*
	 * These are thread-local variables requiring no synchronization.
	 */
	TAILQ_INIT(&ar_worklist);
	audit_cred = NULL;
	audit_td = curthread;
	audit_vp = NULL;

	mtx_lock(&audit_mtx);
	while (1) {
		mtx_assert(&audit_mtx, MA_OWNED);

		/*
		 * Wait for record or rotation events.
		 */
		while (!audit_replacement_flag && TAILQ_EMPTY(&audit_q))
			cv_wait(&audit_worker_cv, &audit_mtx);

		/*
		 * First priority: replace the audit log target if requested.
		 */
		audit_worker_rotate(&audit_cred, &audit_vp, audit_td);

		/*
		 * If there are records in the global audit record queue,
		 * transfer them to a thread-local queue and process them
		 * one by one.  If we cross the low watermark threshold,
		 * signal any waiting processes that they may wake up and
		 * continue generating records.
		 */
		lowater_signal = 0;
		while ((ar = TAILQ_FIRST(&audit_q))) {
			TAILQ_REMOVE(&audit_q, ar, k_q);
			audit_q_len--;
			if (audit_q_len == audit_qctrl.aq_lowater)
				lowater_signal++;
			TAILQ_INSERT_TAIL(&ar_worklist, ar, k_q);
		}
		if (lowater_signal)
			cv_broadcast(&audit_watermark_cv);

		mtx_unlock(&audit_mtx);
		while ((ar = TAILQ_FIRST(&ar_worklist))) {
			TAILQ_REMOVE(&ar_worklist, ar, k_q);
			audit_worker_process_record(audit_vp, audit_cred,
			    audit_td, ar);
			audit_free(ar);
		}
		mtx_lock(&audit_mtx);
	}
}

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
 * message queue of depth 1.  We are essentially acquiring ownership of the
 * communications queue, inserting our message, and waiting for an
 * acknowledgement.
 */
void
audit_rotate_vnode(struct ucred *cred, struct vnode *vp)
{

	/*
	 * If other parallel log replacements have been requested, we wait
	 * until they've finished before continuing.
	 */
	mtx_lock(&audit_mtx);
	while (audit_replacement_flag != 0)
		cv_wait(&audit_replacement_cv, &audit_mtx);
	audit_replacement_cred = cred;
	audit_replacement_flag = 1;
	audit_replacement_vp = vp;

	/*
	 * Wake up the audit worker to perform the exchange once we release
	 * the mutex.
	 */
	cv_signal(&audit_worker_cv);

	/*
	 * Wait for the audit_worker to broadcast that a replacement has
	 * taken place; we know that once this has happened, our vnode has
	 * been replaced in, so we can return successfully.
	 */
	cv_wait(&audit_replacement_cv, &audit_mtx);
	audit_file_rotate_wait = 0; /* We can now request another rotation */
	mtx_unlock(&audit_mtx);
}

void
audit_worker_init(void)
{
	int error;

	cv_init(&audit_replacement_cv, "audit_replacement_cv");
	error = kthread_create(audit_worker, NULL, &audit_thread, RFHIGHPID,
	    0, "audit");
	if (error)
		panic("audit_worker_init: kthread_create returned %d", error);
}
