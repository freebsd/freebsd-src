/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2007-2009 Google Inc. and Amit Singh
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Google Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2005 Csaba Henk.
 * All rights reserved.
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * Portions of this software were developed by BFF Storage Systems, LLC under
 * sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/sdt.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <vm/uma.h>

#include "fuse.h"
#include "fuse_node.h"
#include "fuse_ipc.h"
#include "fuse_internal.h"

SDT_PROVIDER_DECLARE(fusefs);
/* 
 * Fuse trace probe:
 * arg0: verbosity.  Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(fusefs, , ipc, trace, "int", "char*");

static void fdisp_make_pid(struct fuse_dispatcher *fdip, enum fuse_opcode op,
    struct fuse_data *data, uint64_t nid, pid_t pid, struct ucred *cred);
static void fuse_interrupt_send(struct fuse_ticket *otick, int err);
static struct fuse_ticket *fticket_alloc(struct fuse_data *data);
static void fticket_refresh(struct fuse_ticket *ftick);
static inline void fticket_reset(struct fuse_ticket *ftick);
static void fticket_destroy(struct fuse_ticket *ftick);
static int fticket_wait_answer(struct fuse_ticket *ftick);
static inline int 
fticket_aw_pull_uio(struct fuse_ticket *ftick,
    struct uio *uio);

static int fuse_body_audit(struct fuse_ticket *ftick, size_t blen);

static fuse_handler_t fuse_standard_handler;

static counter_u64_t fuse_ticket_count;
SYSCTL_COUNTER_U64(_vfs_fusefs_stats, OID_AUTO, ticket_count, CTLFLAG_RD,
    &fuse_ticket_count, "Number of allocated tickets");

static long fuse_iov_permanent_bufsize = 1 << 19;

SYSCTL_LONG(_vfs_fusefs, OID_AUTO, iov_permanent_bufsize, CTLFLAG_RW,
    &fuse_iov_permanent_bufsize, 0,
    "limit for permanently stored buffer size for fuse_iovs");
static int fuse_iov_credit = 16;

SYSCTL_INT(_vfs_fusefs, OID_AUTO, iov_credit, CTLFLAG_RW,
    &fuse_iov_credit, 0,
    "how many times is an oversized fuse_iov tolerated");

MALLOC_DEFINE(M_FUSEMSG, "fuse_msgbuf", "fuse message buffer");
static uma_zone_t ticket_zone;

/* 
 * TODO: figure out how to timeout INTERRUPT requests, because the daemon may
 * leagally never respond
 */
static int
fuse_interrupt_callback(struct fuse_ticket *tick, struct uio *uio)
{
	struct fuse_ticket *otick, *x_tick;
	struct fuse_interrupt_in *fii;
	struct fuse_data *data = tick->tk_data;
	bool found = false;

	fii = (struct fuse_interrupt_in*)((char*)tick->tk_ms_fiov.base +
		sizeof(struct fuse_in_header));

	fuse_lck_mtx_lock(data->aw_mtx);
	TAILQ_FOREACH_SAFE(otick, &data->aw_head, tk_aw_link, x_tick) {
		if (otick->tk_unique == fii->unique) {
			found = true;
			break;
		}
	}
	fuse_lck_mtx_unlock(data->aw_mtx);

	if (!found) {
		/* Original is already complete.  Just return */
		return 0;
	}

	/* Clear the original ticket's interrupt association */
	otick->irq_unique = 0;

	if (tick->tk_aw_ohead.error == ENOSYS) {
		fsess_set_notimpl(data->mp, FUSE_INTERRUPT);
		return 0;
	} else if (tick->tk_aw_ohead.error == EAGAIN) {
		/* 
		 * There are two reasons we might get this:
		 * 1) the daemon received the INTERRUPT request before the
		 *    original, or
		 * 2) the daemon received the INTERRUPT request after it
		 *    completed the original request.
		 * In the first case we should re-send the INTERRUPT.  In the
		 * second, we should ignore it.
		 */
		/* Resend */
		fuse_interrupt_send(otick, EINTR);
		return 0;
	} else {
		/* Illegal FUSE_INTERRUPT response */
		return EINVAL;
	}
}

/* Interrupt the operation otick.  Return err as its error code */
void
fuse_interrupt_send(struct fuse_ticket *otick, int err)
{
	struct fuse_dispatcher fdi;
	struct fuse_interrupt_in *fii;
	struct fuse_in_header *ftick_hdr;
	struct fuse_data *data = otick->tk_data;
	struct fuse_ticket *tick, *xtick;
	struct ucred reused_creds;
	gid_t reused_groups[1];

	if (otick->irq_unique == 0) {
		/* 
		 * If the daemon hasn't yet received otick, then we can answer
		 * it ourselves and return.
		 */
		fuse_lck_mtx_lock(data->ms_mtx);
		STAILQ_FOREACH_SAFE(tick, &otick->tk_data->ms_head, tk_ms_link,
			xtick) {
			if (tick == otick) {
				STAILQ_REMOVE(&otick->tk_data->ms_head, tick,
					fuse_ticket, tk_ms_link);
				otick->tk_data->ms_count--;
				otick->tk_ms_link.stqe_next = NULL;
				fuse_lck_mtx_unlock(data->ms_mtx);

				fuse_lck_mtx_lock(otick->tk_aw_mtx);
				if (!fticket_answered(otick)) {
					fticket_set_answered(otick);
					otick->tk_aw_errno = err;
					wakeup(otick);
				}
				fuse_lck_mtx_unlock(otick->tk_aw_mtx);

				fuse_ticket_drop(tick);
				return;
			}
		}
		fuse_lck_mtx_unlock(data->ms_mtx);

		/*
		 * If the fuse daemon doesn't support interrupts, then there's
		 * nothing more that we can do
		 */
		if (fsess_not_impl(data->mp, FUSE_INTERRUPT))
			return;

		/* 
		 * If the fuse daemon has already received otick, then we must
		 * send FUSE_INTERRUPT.
		 */
		ftick_hdr = fticket_in_header(otick);
		reused_creds.cr_uid = ftick_hdr->uid;
		reused_groups[0] = ftick_hdr->gid;
		reused_creds.cr_groups = reused_groups;
		fdisp_init(&fdi, sizeof(*fii));
		fdisp_make_pid(&fdi, FUSE_INTERRUPT, data, ftick_hdr->nodeid,
			ftick_hdr->pid, &reused_creds);

		fii = fdi.indata;
		fii->unique = otick->tk_unique;
		fuse_insert_callback(fdi.tick, fuse_interrupt_callback);

		otick->irq_unique = fdi.tick->tk_unique;
		/* Interrupt ops should be delivered ASAP */
		fuse_insert_message(fdi.tick, true);
		fdisp_destroy(&fdi);
	} else {
		/* This ticket has already been interrupted */
	}
}

void
fiov_init(struct fuse_iov *fiov, size_t size)
{
	uint32_t msize = FU_AT_LEAST(size);

	fiov->len = 0;

	fiov->base = malloc(msize, M_FUSEMSG, M_WAITOK | M_ZERO);

	fiov->allocated_size = msize;
	fiov->credit = fuse_iov_credit;
}

void
fiov_teardown(struct fuse_iov *fiov)
{
	MPASS(fiov->base != NULL);
	free(fiov->base, M_FUSEMSG);
}

void
fiov_adjust(struct fuse_iov *fiov, size_t size)
{
	if (fiov->allocated_size < size ||
	    (fuse_iov_permanent_bufsize >= 0 &&
	    fiov->allocated_size - size > fuse_iov_permanent_bufsize &&
	    --fiov->credit < 0)) {
		fiov->base = realloc(fiov->base, FU_AT_LEAST(size), M_FUSEMSG,
		    M_WAITOK | M_ZERO);
		if (!fiov->base) {
			panic("FUSE: realloc failed");
		}
		fiov->allocated_size = FU_AT_LEAST(size);
		fiov->credit = fuse_iov_credit;
		/* Clear data buffer after reallocation */
		bzero(fiov->base, size);
	} else if (size > fiov->len) {
		/* Clear newly extended portion of data buffer */
		bzero((char*)fiov->base + fiov->len, size - fiov->len);
	}
	fiov->len = size;
}

/* Resize the fiov if needed, and clear it's buffer */
void
fiov_refresh(struct fuse_iov *fiov)
{
	fiov_adjust(fiov, 0);
}

static int
fticket_ctor(void *mem, int size, void *arg, int flags)
{
	struct fuse_ticket *ftick = mem;
	struct fuse_data *data = arg;

	FUSE_ASSERT_MS_DONE(ftick);
	FUSE_ASSERT_AW_DONE(ftick);

	ftick->tk_data = data;
	ftick->irq_unique = 0;
	refcount_init(&ftick->tk_refcount, 1);
	counter_u64_add(fuse_ticket_count, 1);

	fticket_refresh(ftick);

	return 0;
}

static void
fticket_dtor(void *mem, int size, void *arg)
{
#ifdef INVARIANTS
	struct fuse_ticket *ftick = mem;
#endif

	FUSE_ASSERT_MS_DONE(ftick);
	FUSE_ASSERT_AW_DONE(ftick);

	counter_u64_add(fuse_ticket_count, -1);
}

static int
fticket_init(void *mem, int size, int flags)
{
	struct fuse_ticket *ftick = mem;

	bzero(ftick, sizeof(struct fuse_ticket));

	fiov_init(&ftick->tk_ms_fiov, sizeof(struct fuse_in_header));

	mtx_init(&ftick->tk_aw_mtx, "fuse answer delivery mutex", NULL, MTX_DEF);
	fiov_init(&ftick->tk_aw_fiov, 0);

	return 0;
}

static void
fticket_fini(void *mem, int size)
{
	struct fuse_ticket *ftick = mem;

	fiov_teardown(&ftick->tk_ms_fiov);
	fiov_teardown(&ftick->tk_aw_fiov);
	mtx_destroy(&ftick->tk_aw_mtx);
}

static inline struct fuse_ticket *
fticket_alloc(struct fuse_data *data)
{
	return uma_zalloc_arg(ticket_zone, data, M_WAITOK);
}

static inline void
fticket_destroy(struct fuse_ticket *ftick)
{
	return uma_zfree(ticket_zone, ftick);
}

/* Prepare the ticket to be reused and clear its data buffers */
static inline void
fticket_refresh(struct fuse_ticket *ftick)
{
	fticket_reset(ftick);

	fiov_refresh(&ftick->tk_ms_fiov);
	fiov_refresh(&ftick->tk_aw_fiov);
}

/* Prepare the ticket to be reused, but don't clear its data buffers */
static inline void
fticket_reset(struct fuse_ticket *ftick)
{
	struct fuse_data *data = ftick->tk_data;

	FUSE_ASSERT_MS_DONE(ftick);
	FUSE_ASSERT_AW_DONE(ftick);

	bzero(&ftick->tk_aw_ohead, sizeof(struct fuse_out_header));

	ftick->tk_aw_errno = 0;
	ftick->tk_flag = 0;

	/* May be truncated to 32 bits on LP32 arches */
	ftick->tk_unique = atomic_fetchadd_long(&data->ticketer, 1);
	if (ftick->tk_unique == 0)
		ftick->tk_unique = atomic_fetchadd_long(&data->ticketer, 1);
}

static int
fticket_wait_answer(struct fuse_ticket *ftick)
{
	struct thread *td = curthread;
	sigset_t blockedset, oldset;
	int err = 0, stops_deferred;
	struct fuse_data *data = ftick->tk_data;
	bool interrupted = false;

	if (fsess_maybe_impl(ftick->tk_data->mp, FUSE_INTERRUPT) &&
	    data->dataflags & FSESS_INTR) {
		SIGEMPTYSET(blockedset);
	} else {
		/* Block all signals except (implicitly) SIGKILL */
		SIGFILLSET(blockedset);
	}
	stops_deferred = sigdeferstop(SIGDEFERSTOP_SILENT);
	kern_sigprocmask(td, SIG_BLOCK, NULL, &oldset, 0);

	fuse_lck_mtx_lock(ftick->tk_aw_mtx);

retry:
	if (fticket_answered(ftick)) {
		goto out;
	}

	if (fdata_get_dead(data)) {
		err = ENOTCONN;
		fticket_set_answered(ftick);
		goto out;
	}
	kern_sigprocmask(td, SIG_BLOCK, &blockedset, NULL, 0);
	err = msleep(ftick, &ftick->tk_aw_mtx, PCATCH, "fu_ans",
	    data->daemon_timeout * hz);
	kern_sigprocmask(td, SIG_SETMASK, &oldset, NULL, 0);
	if (err == EWOULDBLOCK) {
		SDT_PROBE2(fusefs, , ipc, trace, 3,
			"fticket_wait_answer: EWOULDBLOCK");
#ifdef XXXIP				/* die conditionally */
		if (!fdata_get_dead(data)) {
			fdata_set_dead(data);
		}
#endif
		err = ETIMEDOUT;
		fticket_set_answered(ftick);
	} else if ((err == EINTR || err == ERESTART)) {
		/*
		 * Whether we get EINTR or ERESTART depends on whether
		 * SA_RESTART was set by sigaction(2).
		 *
		 * Try to interrupt the operation and wait for an EINTR response
		 * to the original operation.  If the file system does not
		 * support FUSE_INTERRUPT, then we'll just wait for it to
		 * complete like normal.  If it does support FUSE_INTERRUPT,
		 * then it will either respond EINTR to the original operation,
		 * or EAGAIN to the interrupt.
		 */
		sigset_t tmpset;

		SDT_PROBE2(fusefs, , ipc, trace, 4,
			"fticket_wait_answer: interrupt");
		fuse_lck_mtx_unlock(ftick->tk_aw_mtx);
		fuse_interrupt_send(ftick, err);

		PROC_LOCK(td->td_proc);
		mtx_lock(&td->td_proc->p_sigacts->ps_mtx);
		tmpset = td->td_proc->p_siglist;
		SIGSETOR(tmpset, td->td_siglist);
		mtx_unlock(&td->td_proc->p_sigacts->ps_mtx);
		PROC_UNLOCK(td->td_proc);

		fuse_lck_mtx_lock(ftick->tk_aw_mtx);
		if (!interrupted && !SIGISMEMBER(tmpset, SIGKILL)) { 
			/* 
			 * Block all signals while we wait for an interrupt
			 * response.  The protocol doesn't discriminate between
			 * different signals.
			 */
			SIGFILLSET(blockedset);
			interrupted = true;
			goto retry;
		} else {
			/*
			 * Return immediately for fatal signals, or if this is
			 * the second interruption.  We should only be
			 * interrupted twice if the thread is stopped, for
			 * example during sigexit.
			 */
		}
	} else if (err) {
		SDT_PROBE2(fusefs, , ipc, trace, 6,
			"fticket_wait_answer: other error");
	} else {
		SDT_PROBE2(fusefs, , ipc, trace, 7, "fticket_wait_answer: OK");
	}
out:
	if (!(err || fticket_answered(ftick))) {
		SDT_PROBE2(fusefs, , ipc, trace, 1,
			"FUSE: requester was woken up but still no answer");
		err = ENXIO;
	}
	fuse_lck_mtx_unlock(ftick->tk_aw_mtx);
	sigallowstop(stops_deferred);

	return err;
}

static	inline
int
fticket_aw_pull_uio(struct fuse_ticket *ftick, struct uio *uio)
{
	int err = 0;
	size_t len = uio_resid(uio);

	if (len) {
		fiov_adjust(fticket_resp(ftick), len);
		err = uiomove(fticket_resp(ftick)->base, len, uio);
	}
	return err;
}

int
fticket_pull(struct fuse_ticket *ftick, struct uio *uio)
{
	int err = 0;

	if (ftick->tk_aw_ohead.error) {
		return 0;
	}
	err = fuse_body_audit(ftick, uio_resid(uio));
	if (!err) {
		err = fticket_aw_pull_uio(ftick, uio);
	}
	return err;
}

struct fuse_data *
fdata_alloc(struct cdev *fdev, struct ucred *cred)
{
	struct fuse_data *data;

	data = malloc(sizeof(struct fuse_data), M_FUSEMSG, M_WAITOK | M_ZERO);

	data->fdev = fdev;
	mtx_init(&data->ms_mtx, "fuse message list mutex", NULL, MTX_DEF);
	STAILQ_INIT(&data->ms_head);
	data->ms_count = 0;
	knlist_init_mtx(&data->ks_rsel.si_note, &data->ms_mtx);
	mtx_init(&data->aw_mtx, "fuse answer list mutex", NULL, MTX_DEF);
	TAILQ_INIT(&data->aw_head);
	data->daemoncred = crhold(cred);
	data->daemon_timeout = FUSE_DEFAULT_DAEMON_TIMEOUT;
	sx_init(&data->rename_lock, "fuse rename lock");
	data->ref = 1;

	return data;
}

void
fdata_trydestroy(struct fuse_data *data)
{
	data->ref--;
	MPASS(data->ref >= 0);
	if (data->ref != 0)
		return;

	/* Driving off stage all that stuff thrown at device... */
	sx_destroy(&data->rename_lock);
	crfree(data->daemoncred);
	mtx_destroy(&data->aw_mtx);
	knlist_delete(&data->ks_rsel.si_note, curthread, 0);
	knlist_destroy(&data->ks_rsel.si_note);
	mtx_destroy(&data->ms_mtx);

	free(data, M_FUSEMSG);
}

void
fdata_set_dead(struct fuse_data *data)
{
	FUSE_LOCK();
	if (fdata_get_dead(data)) {
		FUSE_UNLOCK();
		return;
	}
	fuse_lck_mtx_lock(data->ms_mtx);
	data->dataflags |= FSESS_DEAD;
	wakeup_one(data);
	selwakeuppri(&data->ks_rsel, PZERO + 1);
	wakeup(&data->ticketer);
	fuse_lck_mtx_unlock(data->ms_mtx);
	FUSE_UNLOCK();
}

struct fuse_ticket *
fuse_ticket_fetch(struct fuse_data *data)
{
	int err = 0;
	struct fuse_ticket *ftick;

	ftick = fticket_alloc(data);

	if (!(data->dataflags & FSESS_INITED)) {
		/* Sleep until get answer for INIT messsage */
		FUSE_LOCK();
		if (!(data->dataflags & FSESS_INITED) && data->ticketer > 2) {
			err = msleep(&data->ticketer, &fuse_mtx, PCATCH | PDROP,
			    "fu_ini", 0);
			if (err)
				fdata_set_dead(data);
		} else
			FUSE_UNLOCK();
	}
	return ftick;
}

int
fuse_ticket_drop(struct fuse_ticket *ftick)
{
	int die;

	die = refcount_release(&ftick->tk_refcount);
	if (die)
		fticket_destroy(ftick);

	return die;
}

void
fuse_insert_callback(struct fuse_ticket *ftick, fuse_handler_t * handler)
{
	if (fdata_get_dead(ftick->tk_data)) {
		return;
	}
	ftick->tk_aw_handler = handler;

	fuse_lck_mtx_lock(ftick->tk_data->aw_mtx);
	fuse_aw_push(ftick);
	fuse_lck_mtx_unlock(ftick->tk_data->aw_mtx);
}

/*
 * Insert a new upgoing ticket into the message queue
 *
 * If urgent is true, insert at the front of the queue.  Otherwise, insert in
 * FIFO order.
 */
void
fuse_insert_message(struct fuse_ticket *ftick, bool urgent)
{
	if (ftick->tk_flag & FT_DIRTY) {
		panic("FUSE: ticket reused without being refreshed");
	}
	ftick->tk_flag |= FT_DIRTY;

	if (fdata_get_dead(ftick->tk_data)) {
		return;
	}
	fuse_lck_mtx_lock(ftick->tk_data->ms_mtx);
	if (urgent)
		fuse_ms_push_head(ftick);
	else
		fuse_ms_push(ftick);
	wakeup_one(ftick->tk_data);
	selwakeuppri(&ftick->tk_data->ks_rsel, PZERO + 1);
	KNOTE_LOCKED(&ftick->tk_data->ks_rsel.si_note, 0);
	fuse_lck_mtx_unlock(ftick->tk_data->ms_mtx);
}

static int
fuse_body_audit(struct fuse_ticket *ftick, size_t blen)
{
	int err = 0;
	enum fuse_opcode opcode;

	opcode = fticket_opcode(ftick);

	switch (opcode) {
	case FUSE_BMAP:
		err = (blen == sizeof(struct fuse_bmap_out)) ? 0 : EINVAL;
		break;

	case FUSE_LINK:
	case FUSE_LOOKUP:
	case FUSE_MKDIR:
	case FUSE_MKNOD:
	case FUSE_SYMLINK:
		if (fuse_libabi_geq(ftick->tk_data, 7, 9)) {
			err = (blen == sizeof(struct fuse_entry_out)) ?
				0 : EINVAL;
		} else {
			err = (blen == FUSE_COMPAT_ENTRY_OUT_SIZE) ? 0 : EINVAL;
		}
		break;

	case FUSE_FORGET:
		panic("FUSE: a handler has been intalled for FUSE_FORGET");
		break;

	case FUSE_GETATTR:
	case FUSE_SETATTR:
		if (fuse_libabi_geq(ftick->tk_data, 7, 9)) {
			err = (blen == sizeof(struct fuse_attr_out)) ? 
			  0 : EINVAL;
		} else {
			err = (blen == FUSE_COMPAT_ATTR_OUT_SIZE) ? 0 : EINVAL;
		}
		break;

	case FUSE_READLINK:
		err = (PAGE_SIZE >= blen) ? 0 : EINVAL;
		break;

	case FUSE_UNLINK:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_RMDIR:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_RENAME:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_OPEN:
		err = (blen == sizeof(struct fuse_open_out)) ? 0 : EINVAL;
		break;

	case FUSE_READ:
		err = (((struct fuse_read_in *)(
		    (char *)ftick->tk_ms_fiov.base +
		    sizeof(struct fuse_in_header)
		    ))->size >= blen) ? 0 : EINVAL;
		break;

	case FUSE_WRITE:
		err = (blen == sizeof(struct fuse_write_out)) ? 0 : EINVAL;
		break;

	case FUSE_STATFS:
		if (fuse_libabi_geq(ftick->tk_data, 7, 4)) {
			err = (blen == sizeof(struct fuse_statfs_out)) ? 
			  0 : EINVAL;
		} else {
			err = (blen == FUSE_COMPAT_STATFS_SIZE) ? 0 : EINVAL;
		}
		break;

	case FUSE_RELEASE:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_FSYNC:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_SETXATTR:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_GETXATTR:
	case FUSE_LISTXATTR:
		/*
		 * These can have varying response lengths, and 0 length
		 * isn't necessarily invalid.
		 */
		err = 0;
		break;

	case FUSE_REMOVEXATTR:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_FLUSH:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_INIT:
		if (blen == sizeof(struct fuse_init_out) ||
		    blen == FUSE_COMPAT_INIT_OUT_SIZE ||
		    blen == FUSE_COMPAT_22_INIT_OUT_SIZE) {
			err = 0;
		} else {
			err = EINVAL;
		}
		break;

	case FUSE_OPENDIR:
		err = (blen == sizeof(struct fuse_open_out)) ? 0 : EINVAL;
		break;

	case FUSE_READDIR:
		err = (((struct fuse_read_in *)(
		    (char *)ftick->tk_ms_fiov.base +
		    sizeof(struct fuse_in_header)
		    ))->size >= blen) ? 0 : EINVAL;
		break;

	case FUSE_RELEASEDIR:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_FSYNCDIR:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_GETLK:
		err = (blen == sizeof(struct fuse_lk_out)) ? 0 : EINVAL;
		break;

	case FUSE_SETLK:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_SETLKW:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_ACCESS:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_CREATE:
		if (fuse_libabi_geq(ftick->tk_data, 7, 9)) {
			err = (blen == sizeof(struct fuse_entry_out) +
			    sizeof(struct fuse_open_out)) ? 0 : EINVAL;
		} else {
			err = (blen == FUSE_COMPAT_ENTRY_OUT_SIZE +
			    sizeof(struct fuse_open_out)) ? 0 : EINVAL;
		}
		break;

	case FUSE_DESTROY:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_FALLOCATE:
		err = (blen == 0) ? 0 : EINVAL;
		break;

	case FUSE_LSEEK:
		err = (blen == sizeof(struct fuse_lseek_out)) ? 0 : EINVAL;
		break;

	case FUSE_COPY_FILE_RANGE:
		err = (blen == sizeof(struct fuse_write_out)) ? 0 : EINVAL;
		break;

	default:
		panic("FUSE: opcodes out of sync (%d)\n", opcode);
	}

	return err;
}

static inline void
fuse_setup_ihead(struct fuse_in_header *ihead, struct fuse_ticket *ftick,
    uint64_t nid, enum fuse_opcode op, size_t blen, pid_t pid,
    struct ucred *cred)
{
	ihead->len = sizeof(*ihead) + blen;
	ihead->unique = ftick->tk_unique;
	ihead->nodeid = nid;
	ihead->opcode = op;

	ihead->pid = pid;
	ihead->uid = cred->cr_uid;
	ihead->gid = cred->cr_groups[0];
}

/*
 * fuse_standard_handler just pulls indata and wakes up pretender.
 * Doesn't try to interpret data, that's left for the pretender.
 * Though might do a basic size verification before the pull-in takes place
 */

static int
fuse_standard_handler(struct fuse_ticket *ftick, struct uio *uio)
{
	int err = 0;

	err = fticket_pull(ftick, uio);

	fuse_lck_mtx_lock(ftick->tk_aw_mtx);

	if (!fticket_answered(ftick)) {
		fticket_set_answered(ftick);
		ftick->tk_aw_errno = err;
		wakeup(ftick);
	}
	fuse_lck_mtx_unlock(ftick->tk_aw_mtx);

	return err;
}

/*
 * Reinitialize a dispatcher from a pid and node id, without resizing or
 * clearing its data buffers
 */
static void
fdisp_refresh_pid(struct fuse_dispatcher *fdip, enum fuse_opcode op,
    struct mount *mp, uint64_t nid, pid_t pid, struct ucred *cred)
{
	MPASS(fdip->tick);
	MPASS2(sizeof(fdip->finh) + fdip->iosize <= fdip->tick->tk_ms_fiov.len,
		"Must use fdisp_make_pid to increase the size of the fiov");
	fticket_reset(fdip->tick);

	FUSE_DIMALLOC(&fdip->tick->tk_ms_fiov, fdip->finh,
	    fdip->indata, fdip->iosize);

	fuse_setup_ihead(fdip->finh, fdip->tick, nid, op, fdip->iosize, pid,
		cred);
}

/* Initialize a dispatcher from a pid and node id */
static void
fdisp_make_pid(struct fuse_dispatcher *fdip, enum fuse_opcode op,
    struct fuse_data *data, uint64_t nid, pid_t pid, struct ucred *cred)
{
	if (fdip->tick) {
		fticket_refresh(fdip->tick);
	} else {
		fdip->tick = fuse_ticket_fetch(data);
	}

	/* FUSE_DIMALLOC will bzero the fiovs when it enlarges them */
	FUSE_DIMALLOC(&fdip->tick->tk_ms_fiov, fdip->finh,
	    fdip->indata, fdip->iosize);

	fuse_setup_ihead(fdip->finh, fdip->tick, nid, op, fdip->iosize, pid, cred);
}

void
fdisp_make(struct fuse_dispatcher *fdip, enum fuse_opcode op, struct mount *mp,
    uint64_t nid, struct thread *td, struct ucred *cred)
{
	struct fuse_data *data = fuse_get_mpdata(mp);
	RECTIFY_TDCR(td, cred);

	return fdisp_make_pid(fdip, op, data, nid, td->td_proc->p_pid, cred);
}

void
fdisp_make_vp(struct fuse_dispatcher *fdip, enum fuse_opcode op,
    struct vnode *vp, struct thread *td, struct ucred *cred)
{
	struct mount *mp = vnode_mount(vp);
	struct fuse_data *data = fuse_get_mpdata(mp);

	RECTIFY_TDCR(td, cred);
	return fdisp_make_pid(fdip, op, data, VTOI(vp),
	    td->td_proc->p_pid, cred);
}

/* Refresh a fuse_dispatcher so it can be reused, but don't zero its data */
void
fdisp_refresh_vp(struct fuse_dispatcher *fdip, enum fuse_opcode op,
    struct vnode *vp, struct thread *td, struct ucred *cred)
{
	RECTIFY_TDCR(td, cred);
	return fdisp_refresh_pid(fdip, op, vnode_mount(vp), VTOI(vp),
	    td->td_proc->p_pid, cred);
}

SDT_PROBE_DEFINE2(fusefs, , ipc, fdisp_wait_answ_error, "char*", "int");

int
fdisp_wait_answ(struct fuse_dispatcher *fdip)
{
	int err = 0;

	fdip->answ_stat = 0;
	fuse_insert_callback(fdip->tick, fuse_standard_handler);
	fuse_insert_message(fdip->tick, false);

	if ((err = fticket_wait_answer(fdip->tick))) {
		fuse_lck_mtx_lock(fdip->tick->tk_aw_mtx);

		if (fticket_answered(fdip->tick)) {
			/*
	                 * Just between noticing the interrupt and getting here,
	                 * the standard handler has completed his job.
	                 * So we drop the ticket and exit as usual.
	                 */
			SDT_PROBE2(fusefs, , ipc, fdisp_wait_answ_error,
				"IPC: interrupted, already answered", err);
			fuse_lck_mtx_unlock(fdip->tick->tk_aw_mtx);
			goto out;
		} else {
			/*
	                 * So we were faster than the standard handler.
	                 * Then by setting the answered flag we get *him*
	                 * to drop the ticket.
	                 */
			SDT_PROBE2(fusefs, , ipc, fdisp_wait_answ_error,
				"IPC: interrupted, setting to answered", err);
			fticket_set_answered(fdip->tick);
			fuse_lck_mtx_unlock(fdip->tick->tk_aw_mtx);
			return err;
		}
	}

	if (fdip->tick->tk_aw_errno == ENOTCONN) {
		/* The daemon died while we were waiting for a response */
		err = ENOTCONN;
		goto out;
	} else if (fdip->tick->tk_aw_errno) {
		/* 
		 * There was some sort of communication error with the daemon
		 * that the client wouldn't understand.
		 */
		SDT_PROBE2(fusefs, , ipc, fdisp_wait_answ_error,
			"IPC: explicit EIO-ing", fdip->tick->tk_aw_errno);
		err = EIO;
		goto out;
	}
	if ((err = fdip->tick->tk_aw_ohead.error)) {
		SDT_PROBE2(fusefs, , ipc, fdisp_wait_answ_error,
			"IPC: setting status", fdip->tick->tk_aw_ohead.error);
		/*
	         * This means a "proper" fuse syscall error.
	         * We record this value so the caller will
	         * be able to know it's not a boring messaging
	         * failure, if she wishes so (and if not, she can
	         * just simply propagate the return value of this routine).
	         * [XXX Maybe a bitflag would do the job too,
	         * if other flags needed, this will be converted thusly.]
	         */
		fdip->answ_stat = err;
		goto out;
	}
	fdip->answ = fticket_resp(fdip->tick)->base;
	fdip->iosize = fticket_resp(fdip->tick)->len;

	return 0;

out:
	return err;
}

void
fuse_ipc_init(void)
{
	ticket_zone = uma_zcreate("fuse_ticket", sizeof(struct fuse_ticket),
	    fticket_ctor, fticket_dtor, fticket_init, fticket_fini,
	    UMA_ALIGN_PTR, 0);
	fuse_ticket_count = counter_u64_alloc(M_WAITOK);
}

void
fuse_ipc_destroy(void)
{
	counter_u64_free(fuse_ticket_count);
	uma_zdestroy(ticket_zone);
}

SDT_PROBE_DEFINE3(fusefs,, ipc, warn, "struct fuse_data*", "unsigned", "char*");
void
fuse_warn(struct fuse_data *data, unsigned flag, const char *msg)
{
	SDT_PROBE3(fusefs, , ipc, warn, data, flag, msg);
	if (!(data->dataflags & flag)) {
		printf("WARNING: FUSE protocol violation for server mounted at "
		    "%s: %s  "
		    "This warning will not be repeated.\n",
		    data->mp->mnt_stat.f_mntonname, msg);
		data->dataflags |= flag;
	}
}
