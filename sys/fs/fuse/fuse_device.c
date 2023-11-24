/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2007-2009 Google Inc.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/param.h>
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
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/sysctl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>

#include "fuse.h"
#include "fuse_internal.h"
#include "fuse_ipc.h"

#include <compat/linux/linux_errno.h>
#include <compat/linux/linux_errno.inc>

SDT_PROVIDER_DECLARE(fusefs);
/* 
 * Fuse trace probe:
 * arg0: verbosity.  Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(fusefs, , device, trace, "int", "char*");

static struct cdev *fuse_dev;

static d_kqfilter_t fuse_device_filter;
static d_open_t fuse_device_open;
static d_poll_t fuse_device_poll;
static d_read_t fuse_device_read;
static d_write_t fuse_device_write;

static struct cdevsw fuse_device_cdevsw = {
	.d_kqfilter = fuse_device_filter,
	.d_open = fuse_device_open,
	.d_name = "fuse",
	.d_poll = fuse_device_poll,
	.d_read = fuse_device_read,
	.d_write = fuse_device_write,
	.d_version = D_VERSION,
};

static int fuse_device_filt_read(struct knote *kn, long hint);
static int fuse_device_filt_write(struct knote *kn, long hint);
static void fuse_device_filt_detach(struct knote *kn);

struct filterops fuse_device_rfiltops = {
	.f_isfd = 1,
	.f_detach = fuse_device_filt_detach,
	.f_event = fuse_device_filt_read,
};

struct filterops fuse_device_wfiltops = {
	.f_isfd = 1,
	.f_event = fuse_device_filt_write,
};

/****************************
 *
 * >>> Fuse device op defs
 *
 ****************************/

static void
fdata_dtor(void *arg)
{
	struct fuse_data *fdata;
	struct fuse_ticket *tick;

	fdata = arg;
	if (fdata == NULL)
		return;

	fdata_set_dead(fdata);

	FUSE_LOCK();
	fuse_lck_mtx_lock(fdata->aw_mtx);
	/* wakup poll()ers */
	selwakeuppri(&fdata->ks_rsel, PZERO + 1);
	/* Don't let syscall handlers wait in vain */
	while ((tick = fuse_aw_pop(fdata))) {
		fuse_lck_mtx_lock(tick->tk_aw_mtx);
		fticket_set_answered(tick);
		tick->tk_aw_errno = ENOTCONN;
		wakeup(tick);
		fuse_lck_mtx_unlock(tick->tk_aw_mtx);
		FUSE_ASSERT_AW_DONE(tick);
		fuse_ticket_drop(tick);
	}
	fuse_lck_mtx_unlock(fdata->aw_mtx);

	/* Cleanup unsent operations */
	fuse_lck_mtx_lock(fdata->ms_mtx);
	while ((tick = fuse_ms_pop(fdata))) {
		fuse_ticket_drop(tick);
	}
	fuse_lck_mtx_unlock(fdata->ms_mtx);
	FUSE_UNLOCK();

	fdata_trydestroy(fdata);
}

static int
fuse_device_filter(struct cdev *dev, struct knote *kn)
{
	struct fuse_data *data;
	int error;

	error = devfs_get_cdevpriv((void **)&data);

	if (error == 0 && kn->kn_filter == EVFILT_READ) {
		kn->kn_fop = &fuse_device_rfiltops;
		kn->kn_hook = data;
		knlist_add(&data->ks_rsel.si_note, kn, 0);
		error = 0;
	} else if (error == 0 && kn->kn_filter == EVFILT_WRITE) {
		kn->kn_fop = &fuse_device_wfiltops;
		error = 0;
	} else if (error == 0) {
		error = EINVAL;
		kn->kn_data = error;
	}

	return (error);
}

static void
fuse_device_filt_detach(struct knote *kn)
{
	struct fuse_data *data;

	data = (struct fuse_data*)kn->kn_hook;
	MPASS(data != NULL);
	knlist_remove(&data->ks_rsel.si_note, kn, 0);
	kn->kn_hook = NULL;
}

static int
fuse_device_filt_read(struct knote *kn, long hint)
{
	struct fuse_data *data;
	int ready;

	data = (struct fuse_data*)kn->kn_hook;
	MPASS(data != NULL);

	mtx_assert(&data->ms_mtx, MA_OWNED);
	if (fdata_get_dead(data)) {
		kn->kn_flags |= EV_EOF;
		kn->kn_fflags = ENODEV;
		kn->kn_data = 1;
		ready = 1;
	} else if (STAILQ_FIRST(&data->ms_head)) {
		MPASS(data->ms_count >= 1);
		kn->kn_data = data->ms_count;
		ready = 1;
	} else {
		ready = 0;
	}

	return (ready);
}

static int
fuse_device_filt_write(struct knote *kn, long hint)
{

	kn->kn_data = 0;

	/* The device is always ready to write, so we return 1*/
	return (1);
}

/*
 * Resources are set up on a per-open basis
 */
static int
fuse_device_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct fuse_data *fdata;
	int error;

	SDT_PROBE2(fusefs, , device, trace, 1, "device open");

	fdata = fdata_alloc(dev, td->td_ucred);
	error = devfs_set_cdevpriv(fdata, fdata_dtor);
	if (error != 0)
		fdata_trydestroy(fdata);
	else
		SDT_PROBE2(fusefs, , device, trace, 1, "device open success");
	return (error);
}

int
fuse_device_poll(struct cdev *dev, int events, struct thread *td)
{
	struct fuse_data *data;
	int error, revents = 0;

	error = devfs_get_cdevpriv((void **)&data);
	if (error != 0)
		return (events &
		    (POLLHUP|POLLIN|POLLRDNORM|POLLOUT|POLLWRNORM));

	if (events & (POLLIN | POLLRDNORM)) {
		fuse_lck_mtx_lock(data->ms_mtx);
		if (fdata_get_dead(data) || STAILQ_FIRST(&data->ms_head))
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &data->ks_rsel);
		fuse_lck_mtx_unlock(data->ms_mtx);
	}
	if (events & (POLLOUT | POLLWRNORM)) {
		revents |= events & (POLLOUT | POLLWRNORM);
	}
	return (revents);
}

/*
 * fuse_device_read hangs on the queue of VFS messages.
 * When it's notified that there is a new one, it picks that and
 * passes up to the daemon
 */
int
fuse_device_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	int err;
	struct fuse_data *data;
	struct fuse_ticket *tick;
	void *buf;
	int buflen;

	SDT_PROBE2(fusefs, , device, trace, 1, "fuse device read");

	err = devfs_get_cdevpriv((void **)&data);
	if (err != 0)
		return (err);

	fuse_lck_mtx_lock(data->ms_mtx);
again:
	if (fdata_get_dead(data)) {
		SDT_PROBE2(fusefs, , device, trace, 2,
			"we know early on that reader should be kicked so we "
			"don't wait for news");
		fuse_lck_mtx_unlock(data->ms_mtx);
		return (ENODEV);
	}
	if (!(tick = fuse_ms_pop(data))) {
		/* check if we may block */
		if (ioflag & O_NONBLOCK) {
			/* get outa here soon */
			fuse_lck_mtx_unlock(data->ms_mtx);
			return (EAGAIN);
		} else {
			err = msleep(data, &data->ms_mtx, PCATCH, "fu_msg", 0);
			if (err != 0) {
				fuse_lck_mtx_unlock(data->ms_mtx);
				return (fdata_get_dead(data) ? ENODEV : err);
			}
			tick = fuse_ms_pop(data);
		}
	}
	if (!tick) {
		/*
		 * We can get here if fuse daemon suddenly terminates,
		 * eg, by being hit by a SIGKILL
		 * -- and some other cases, too, tho not totally clear, when
		 * (cv_signal/wakeup_one signals the whole process ?)
		 */
		SDT_PROBE2(fusefs, , device, trace, 1, "no message on thread");
		goto again;
	}
	fuse_lck_mtx_unlock(data->ms_mtx);

	if (fdata_get_dead(data)) {
		/*
		 * somebody somewhere -- eg., umount routine --
		 * wants this liaison finished off
		 */
		SDT_PROBE2(fusefs, , device, trace, 2,
			"reader is to be sacked");
		if (tick) {
			SDT_PROBE2(fusefs, , device, trace, 2, "weird -- "
				"\"kick\" is set tho there is message");
			FUSE_ASSERT_MS_DONE(tick);
			fuse_ticket_drop(tick);
		}
		return (ENODEV);	/* This should make the daemon get off
					 * of us */
	}
	SDT_PROBE2(fusefs, , device, trace, 1,
		"fuse device read message successfully");

	buf = tick->tk_ms_fiov.base;
	buflen = tick->tk_ms_fiov.len;

	/*
	 * Why not ban mercilessly stupid daemons who can't keep up
	 * with us? (There is no much use of a partial read here...)
	 */
	/*
	 * XXX note that in such cases Linux FUSE throws EIO at the
	 * syscall invoker and stands back to the message queue. The
	 * rationale should be made clear (and possibly adopt that
	 * behaviour). Keeping the current scheme at least makes
	 * fallacy as loud as possible...
	 */
	if (uio->uio_resid < buflen) {
		fdata_set_dead(data);
		SDT_PROBE2(fusefs, , device, trace, 2,
		    "daemon is stupid, kick it off...");
		err = ENODEV;
	} else {
		err = uiomove(buf, buflen, uio);
	}

	FUSE_ASSERT_MS_DONE(tick);
	fuse_ticket_drop(tick);

	return (err);
}

static inline int
fuse_ohead_audit(struct fuse_out_header *ohead, struct uio *uio)
{
	if (uio->uio_resid + sizeof(struct fuse_out_header) != ohead->len) {
		SDT_PROBE2(fusefs, , device, trace, 1,
			"Format error: body size "
			"differs from size claimed by header");
		return (EINVAL);
	}
	if (uio->uio_resid && ohead->unique != 0 && ohead->error) {
		SDT_PROBE2(fusefs, , device, trace, 1, 
			"Format error: non zero error but message had a body");
		return (EINVAL);
	}

	return (0);
}

SDT_PROBE_DEFINE1(fusefs, , device, fuse_device_write_notify,
	"struct fuse_out_header*");
SDT_PROBE_DEFINE1(fusefs, , device, fuse_device_write_missing_ticket,
	"uint64_t");
SDT_PROBE_DEFINE1(fusefs, , device, fuse_device_write_found,
	"struct fuse_ticket*");
/*
 * fuse_device_write first reads the header sent by the daemon.
 * If that's OK, looks up ticket/callback node by the unique id seen in header.
 * If the callback node contains a handler function, the uio is passed over
 * that.
 */
static int
fuse_device_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct fuse_out_header ohead;
	int err = 0;
	struct fuse_data *data;
	struct mount *mp;
	struct fuse_ticket *tick, *itick, *x_tick;
	int found = 0;

	err = devfs_get_cdevpriv((void **)&data);
	if (err != 0)
		return (err);
	mp = data->mp;

	if (uio->uio_resid < sizeof(struct fuse_out_header)) {
		SDT_PROBE2(fusefs, , device, trace, 1,
			"fuse_device_write got less than a header!");
		fdata_set_dead(data);
		return (EINVAL);
	}
	if ((err = uiomove(&ohead, sizeof(struct fuse_out_header), uio)) != 0)
		return (err);

	if (data->linux_errnos != 0 && ohead.error != 0) {
		err = -ohead.error;
		if (err < 0 || err >= nitems(linux_to_bsd_errtbl))
			return (EINVAL);

		/* '-', because it will get flipped again below */
		ohead.error = -linux_to_bsd_errtbl[err];
	}

	/*
	 * We check header information (which is redundant) and compare it
	 * with what we see. If we see some inconsistency we discard the
	 * whole answer and proceed on as if it had never existed. In
	 * particular, no pretender will be woken up, regardless the
	 * "unique" value in the header.
	 */
	if ((err = fuse_ohead_audit(&ohead, uio))) {
		fdata_set_dead(data);
		return (err);
	}
	/* Pass stuff over to callback if there is one installed */

	/* Looking for ticket with the unique id of header */
	fuse_lck_mtx_lock(data->aw_mtx);
	TAILQ_FOREACH_SAFE(tick, &data->aw_head, tk_aw_link,
	    x_tick) {
		if (tick->tk_unique == ohead.unique) {
			SDT_PROBE1(fusefs, , device, fuse_device_write_found,
				tick);
			found = 1;
			fuse_aw_remove(tick);
			break;
		}
	}
	if (found && tick->irq_unique > 0) {
		/* 
		 * Discard the FUSE_INTERRUPT ticket that tried to interrupt
		 * this operation
		 */
		TAILQ_FOREACH_SAFE(itick, &data->aw_head, tk_aw_link,
		    x_tick) {
			if (itick->tk_unique == tick->irq_unique) {
				fuse_aw_remove(itick);
				fuse_ticket_drop(itick);
				break;
			}
		}
		tick->irq_unique = 0;
	}
	fuse_lck_mtx_unlock(data->aw_mtx);

	if (found) {
		if (tick->tk_aw_handler) {
			/*
			 * We found a callback with proper handler. In this
			 * case the out header will be 0wnd by the callback,
			 * so the fun of freeing that is left for her.
			 * (Then, by all chance, she'll just get that's done
			 * via ticket_drop(), so no manual mucking
			 * around...)
			 */
			SDT_PROBE2(fusefs, , device, trace, 1,
				"pass ticket to a callback");
			/* Sanitize the linuxism of negative errnos */
			ohead.error *= -1;
			if (ohead.error < 0 || ohead.error > ELAST) {
				/* Illegal error code */
				ohead.error = EIO;
				memcpy(&tick->tk_aw_ohead, &ohead,
					sizeof(ohead));
				tick->tk_aw_handler(tick, uio);
				err = EINVAL;
			} else {
				memcpy(&tick->tk_aw_ohead, &ohead,
					sizeof(ohead));
				err = tick->tk_aw_handler(tick, uio);
			}
		} else {
			/* pretender doesn't wanna do anything with answer */
			SDT_PROBE2(fusefs, , device, trace, 1,
				"stuff devalidated, so we drop it");
		}

		/*
		 * As aw_mtx was not held during the callback execution the
		 * ticket may have been inserted again.  However, this is safe
		 * because fuse_ticket_drop() will deal with refcount anyway.
		 */
		fuse_ticket_drop(tick);
	} else if (ohead.unique == 0){
		/* unique == 0 means asynchronous notification */
		SDT_PROBE1(fusefs, , device, fuse_device_write_notify, &ohead);
		switch (ohead.error) {
		case FUSE_NOTIFY_INVAL_ENTRY:
			err = fuse_internal_invalidate_entry(mp, uio);
			break;
		case FUSE_NOTIFY_INVAL_INODE:
			err = fuse_internal_invalidate_inode(mp, uio);
			break;
		case FUSE_NOTIFY_RETRIEVE:
		case FUSE_NOTIFY_STORE:
			/*
			 * Unimplemented.  I don't know of any file systems
			 * that use them, and the protocol isn't sound anyway,
			 * since the notification messages don't include the
			 * inode's generation number.  Without that, it's
			 * possible to manipulate the cache of the wrong vnode.
			 * Finally, it's not defined what this message should
			 * do for a file with dirty cache.
			 */
		case FUSE_NOTIFY_POLL:
			/* Unimplemented.  See comments in fuse_vnops */
		default:
			/* Not implemented */
			err = ENOSYS;
		}
	} else {
		/* no callback at all! */
		SDT_PROBE1(fusefs, , device, fuse_device_write_missing_ticket, 
			ohead.unique);
		if (ohead.error == -EAGAIN) {
			/* 
			 * This was probably a response to a FUSE_INTERRUPT
			 * operation whose original operation is already
			 * complete.  We can't store FUSE_INTERRUPT tickets
			 * indefinitely because their responses are optional.
			 * So we delete them when the original operation
			 * completes.  And sadly the fuse_header_out doesn't
			 * identify the opcode, so we have to guess.
			 */
			err = 0;
		} else {
			err = EINVAL;
		}
	}

	return (err);
}

int
fuse_device_init(void)
{

	fuse_dev = make_dev(&fuse_device_cdevsw, 0, UID_ROOT, GID_OPERATOR,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, "fuse");
	if (fuse_dev == NULL)
		return (ENOMEM);
	return (0);
}

void
fuse_device_destroy(void)
{

	MPASS(fuse_dev != NULL);
	destroy_dev(fuse_dev);
}
