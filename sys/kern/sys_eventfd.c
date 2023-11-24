/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Dmitry Chagin <dchagin@FreeBSD.org>
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/selinfo.h>
#include <sys/eventfd.h>

#include <security/audit/audit.h>

_Static_assert(EFD_CLOEXEC == O_CLOEXEC, "Mismatched EFD_CLOEXEC");
_Static_assert(EFD_NONBLOCK == O_NONBLOCK, "Mismatched EFD_NONBLOCK");

MALLOC_DEFINE(M_EVENTFD, "eventfd", "eventfd structures");

static fo_rdwr_t	eventfd_read;
static fo_rdwr_t	eventfd_write;
static fo_ioctl_t	eventfd_ioctl;
static fo_poll_t	eventfd_poll;
static fo_kqfilter_t	eventfd_kqfilter;
static fo_stat_t	eventfd_stat;
static fo_close_t	eventfd_close;
static fo_fill_kinfo_t	eventfd_fill_kinfo;

static struct fileops eventfdops = {
	.fo_read = eventfd_read,
	.fo_write = eventfd_write,
	.fo_truncate = invfo_truncate,
	.fo_ioctl = eventfd_ioctl,
	.fo_poll = eventfd_poll,
	.fo_kqfilter = eventfd_kqfilter,
	.fo_stat = eventfd_stat,
	.fo_close = eventfd_close,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = eventfd_fill_kinfo,
	.fo_flags = DFLAG_PASSABLE
};

static void	filt_eventfddetach(struct knote *kn);
static int	filt_eventfdread(struct knote *kn, long hint);
static int	filt_eventfdwrite(struct knote *kn, long hint);

static struct filterops eventfd_rfiltops = {
	.f_isfd = 1,
	.f_detach = filt_eventfddetach,
	.f_event = filt_eventfdread
};

static struct filterops eventfd_wfiltops = {
	.f_isfd = 1,
	.f_detach = filt_eventfddetach,
	.f_event = filt_eventfdwrite
};

struct eventfd {
	eventfd_t	efd_count;
	uint32_t	efd_flags;
	struct selinfo	efd_sel;
	struct mtx	efd_lock;
};

int
eventfd_create_file(struct thread *td, struct file *fp, uint32_t initval,
    int flags)
{
	struct eventfd *efd;
	int fflags;

	AUDIT_ARG_FFLAGS(flags);
	AUDIT_ARG_VALUE(initval);

	efd = malloc(sizeof(*efd), M_EVENTFD, M_WAITOK | M_ZERO);
	efd->efd_flags = flags;
	efd->efd_count = initval;
	mtx_init(&efd->efd_lock, "eventfd", NULL, MTX_DEF);
	knlist_init_mtx(&efd->efd_sel.si_note, &efd->efd_lock);

	fflags = FREAD | FWRITE;
	if ((flags & EFD_NONBLOCK) != 0)
		fflags |= FNONBLOCK;
	finit(fp, fflags, DTYPE_EVENTFD, efd, &eventfdops);

	return (0);
}

static int
eventfd_close(struct file *fp, struct thread *td)
{
	struct eventfd *efd;

	efd = fp->f_data;
	seldrain(&efd->efd_sel);
	knlist_destroy(&efd->efd_sel.si_note);
	mtx_destroy(&efd->efd_lock);
	free(efd, M_EVENTFD);
	return (0);
}

static int
eventfd_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct eventfd *efd;
	eventfd_t count;
	int error;

	if (uio->uio_resid < sizeof(eventfd_t))
		return (EINVAL);

	error = 0;
	efd = fp->f_data;
	mtx_lock(&efd->efd_lock);
	while (error == 0 && efd->efd_count == 0) {
		if ((fp->f_flag & FNONBLOCK) != 0) {
			mtx_unlock(&efd->efd_lock);
			return (EAGAIN);
		}
		error = mtx_sleep(&efd->efd_count, &efd->efd_lock, PCATCH,
		    "efdrd", 0);
	}
	if (error == 0) {
		MPASS(efd->efd_count > 0);
		if ((efd->efd_flags & EFD_SEMAPHORE) != 0) {
			count = 1;
			--efd->efd_count;
		} else {
			count = efd->efd_count;
			efd->efd_count = 0;
		}
		KNOTE_LOCKED(&efd->efd_sel.si_note, 0);
		selwakeup(&efd->efd_sel);
		wakeup(&efd->efd_count);
		mtx_unlock(&efd->efd_lock);
		error = uiomove(&count, sizeof(eventfd_t), uio);
	} else
		mtx_unlock(&efd->efd_lock);

	return (error);
}

static int
eventfd_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct eventfd *efd;
	eventfd_t count;
	int error;

	if (uio->uio_resid < sizeof(eventfd_t))
		return (EINVAL);

	error = uiomove(&count, sizeof(eventfd_t), uio);
	if (error != 0)
		return (error);
	if (count == UINT64_MAX)
		return (EINVAL);

	efd = fp->f_data;
	mtx_lock(&efd->efd_lock);
retry:
	if (UINT64_MAX - efd->efd_count <= count) {
		if ((fp->f_flag & FNONBLOCK) != 0) {
			mtx_unlock(&efd->efd_lock);
			/* Do not not return the number of bytes written */
			uio->uio_resid += sizeof(eventfd_t);
			return (EAGAIN);
		}
		error = mtx_sleep(&efd->efd_count, &efd->efd_lock,
		    PCATCH, "efdwr", 0);
		if (error == 0)
			goto retry;
	}
	if (error == 0) {
		MPASS(UINT64_MAX - efd->efd_count > count);
		efd->efd_count += count;
		KNOTE_LOCKED(&efd->efd_sel.si_note, 0);
		selwakeup(&efd->efd_sel);
		wakeup(&efd->efd_count);
	}
	mtx_unlock(&efd->efd_lock);

	return (error);
}

static int
eventfd_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct eventfd *efd;
	int revents;

	efd = fp->f_data;
	revents = 0;
	mtx_lock(&efd->efd_lock);
	if ((events & (POLLIN | POLLRDNORM)) != 0 && efd->efd_count > 0)
		revents |= events & (POLLIN | POLLRDNORM);
	if ((events & (POLLOUT | POLLWRNORM)) != 0 && UINT64_MAX - 1 >
	    efd->efd_count)
		revents |= events & (POLLOUT | POLLWRNORM);
	if (revents == 0)
		selrecord(td, &efd->efd_sel);
	mtx_unlock(&efd->efd_lock);

	return (revents);
}

static int
eventfd_kqfilter(struct file *fp, struct knote *kn)
{
	struct eventfd *efd = fp->f_data;

	mtx_lock(&efd->efd_lock);
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &eventfd_rfiltops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &eventfd_wfiltops;
		break;
	default:
		mtx_unlock(&efd->efd_lock);
		return (EINVAL);
	}

	kn->kn_hook = efd;
	knlist_add(&efd->efd_sel.si_note, kn, 1);
	mtx_unlock(&efd->efd_lock);

	return (0);
}

static void
filt_eventfddetach(struct knote *kn)
{
	struct eventfd *efd = kn->kn_hook;

	mtx_lock(&efd->efd_lock);
	knlist_remove(&efd->efd_sel.si_note, kn, 1);
	mtx_unlock(&efd->efd_lock);
}

static int
filt_eventfdread(struct knote *kn, long hint)
{
	struct eventfd *efd = kn->kn_hook;
	int ret;

	mtx_assert(&efd->efd_lock, MA_OWNED);
	kn->kn_data = (int64_t)efd->efd_count;
	ret = efd->efd_count > 0;

	return (ret);
}

static int
filt_eventfdwrite(struct knote *kn, long hint)
{
	struct eventfd *efd = kn->kn_hook;
	int ret;

	mtx_assert(&efd->efd_lock, MA_OWNED);
	kn->kn_data = (int64_t)(UINT64_MAX - 1 - efd->efd_count);
	ret = UINT64_MAX - 1 > efd->efd_count;

	return (ret);
}

static int
eventfd_ioctl(struct file *fp, u_long cmd, void *data,
    struct ucred *active_cred, struct thread *td)
{
	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		return (0);
	}

	return (ENOTTY);
}

static int
eventfd_stat(struct file *fp, struct stat *st, struct ucred *active_cred)
{
	bzero((void *)st, sizeof *st);
	st->st_mode = S_IFIFO;
	return (0);
}

static int
eventfd_fill_kinfo(struct file *fp, struct kinfo_file *kif, struct filedesc *fdp)
{
	struct eventfd *efd = fp->f_data;

	kif->kf_type = KF_TYPE_EVENTFD;
	mtx_lock(&efd->efd_lock);
	kif->kf_un.kf_eventfd.kf_eventfd_value = efd->efd_count;
	kif->kf_un.kf_eventfd.kf_eventfd_flags = efd->efd_flags;
	kif->kf_un.kf_eventfd.kf_eventfd_addr = (uintptr_t)efd;
	mtx_unlock(&efd->efd_lock);
	return (0);
}
