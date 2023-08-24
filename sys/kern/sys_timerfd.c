/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Dmitry Chagin <dchagin@FreeBSD.org>
 * Copyright (c) 2023 Jake Freeland <jfree@FreeBSD.org>
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
#include <sys/callout.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/selinfo.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/timerfd.h>
#include <sys/timespec.h>
#include <sys/uio.h>
#include <sys/user.h>

#include <security/audit/audit.h>

#ifdef COMPAT_FREEBSD32
#include <compat/freebsd32/freebsd32.h>
#include <compat/freebsd32/freebsd32_proto.h>
#endif

static MALLOC_DEFINE(M_TIMERFD, "timerfd", "timerfd structures");
static LIST_HEAD(, timerfd) timerfd_head;
static struct unrhdr64 tfdino_unr;

#define	TFD_NOJUMP	0	/* Realtime clock has not jumped. */
#define	TFD_READ	1	/* Jumped, tfd has been read since. */
#define	TFD_ZREAD	2	/* Jumped backwards, CANCEL_ON_SET=false. */
#define	TFD_CANCELED	4	/* Jumped, CANCEL_ON_SET=true. */
#define	TFD_JUMPED	(TFD_ZREAD | TFD_CANCELED)

struct timerfd {
	/* User specified. */
	struct itimerspec tfd_time;	/* tfd timer */
	clockid_t	tfd_clockid;	/* timing base */
	int		tfd_flags;	/* creation flags */
	int		tfd_timflags;	/* timer flags */

	/* Used internally. */
	timerfd_t	tfd_count;	/* expiration count since last read */
	bool		tfd_expired;	/* true upon initial expiration */
	struct mtx	tfd_lock;	/* mtx lock */
	struct callout	tfd_callout;	/* expiration notification */
	struct selinfo	tfd_sel;	/* I/O alerts */
	struct timespec	tfd_boottim;	/* cached boottime */
	int		tfd_jumped;	/* timer jump status */
	LIST_ENTRY(timerfd) entry;	/* entry in list */

	/* For stat(2). */
	ino_t		tfd_ino;	/* inode number */
	struct timespec	tfd_atim;	/* time of last read */
	struct timespec	tfd_mtim;	/* time of last settime */
	struct timespec tfd_birthtim;	/* creation time */
};

static void
timerfd_init(void *data)
{
	new_unrhdr64(&tfdino_unr, 1);
}

SYSINIT(timerfd, SI_SUB_VFS, SI_ORDER_ANY, timerfd_init, NULL);

static inline void
timerfd_getboottime(struct timespec *ts)
{
	struct timeval tv;
	getboottime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, ts);
}

/*
 * Call when a discontinuous jump has occured in CLOCK_REALTIME and
 * update timerfd's cached boottime. A jump can be triggered using
 * functions like clock_settime(2) or settimeofday(2).
 *
 * Timer is marked TFD_CANCELED if TFD_TIMER_CANCEL_ON_SET is set
 * and the realtime clock jumps.
 * Timer is marked TFD_ZREAD if TFD_TIMER_CANCEL_ON_SET is not set,
 * but the realtime clock jumps backwards.
 */
void
timerfd_jumped(void)
{
	struct timerfd *tfd;
	struct timespec boottime, diff;

	timerfd_getboottime(&boottime);
	LIST_FOREACH(tfd, &timerfd_head, entry) {
		mtx_lock(&tfd->tfd_lock);
		if (tfd->tfd_clockid != CLOCK_REALTIME ||
		    (tfd->tfd_timflags & TFD_TIMER_ABSTIME) == 0 ||
		    timespeccmp(&boottime, &tfd->tfd_boottim, ==)) {
			mtx_unlock(&tfd->tfd_lock);
			continue;
		}

		if (callout_active(&tfd->tfd_callout)) {
			if ((tfd->tfd_timflags & TFD_TIMER_CANCEL_ON_SET) != 0)
				tfd->tfd_jumped = TFD_CANCELED;
			else if (timespeccmp(&boottime, &tfd->tfd_boottim, <))
				tfd->tfd_jumped = TFD_ZREAD;

			/*
			 * Do not reschedule callout when
			 * inside interval time loop.
			 */
			if (!tfd->tfd_expired) {
				timespecsub(&boottime,
				    &tfd->tfd_boottim, &diff);
				timespecsub(&tfd->tfd_time.it_value,
				    &diff, &tfd->tfd_time.it_value);
				if (callout_stop(&tfd->tfd_callout) == 1) {
					callout_schedule_sbt(&tfd->tfd_callout,
					    tstosbt(tfd->tfd_time.it_value),
					    0, C_ABSOLUTE);
				}
			}
		}

		tfd->tfd_boottim = boottime;
		mtx_unlock(&tfd->tfd_lock);
	}
}

static int
timerfd_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct timerfd *tfd = fp->f_data;
	timerfd_t count;
	int error = 0;

	if (uio->uio_resid < sizeof(timerfd_t))
		return (EINVAL);

	mtx_lock(&tfd->tfd_lock);
retry:
	getnanotime(&tfd->tfd_atim);
	if ((tfd->tfd_jumped & TFD_JUMPED) != 0) {
		if (tfd->tfd_jumped == TFD_CANCELED)
			error = ECANCELED;
		tfd->tfd_jumped = TFD_READ;
		tfd->tfd_count = 0;
		mtx_unlock(&tfd->tfd_lock);
		return (error);
	} else {
		tfd->tfd_jumped = TFD_NOJUMP;
	}
	if (tfd->tfd_count == 0) {
		if ((fp->f_flag & FNONBLOCK) != 0) {
			mtx_unlock(&tfd->tfd_lock);
			return (EAGAIN);
		}
		td->td_rtcgen = atomic_load_acq_int(&rtc_generation);
		error = mtx_sleep(&tfd->tfd_count, &tfd->tfd_lock,
		    PCATCH, "tfdrd", 0);
		if (error == 0) {
			goto retry;
		} else {
			mtx_unlock(&tfd->tfd_lock);
			return (error);
		}
	}

	count = tfd->tfd_count;
	tfd->tfd_count = 0;
	mtx_unlock(&tfd->tfd_lock);
	error = uiomove(&count, sizeof(timerfd_t), uio);

	return (error);
}

static int
timerfd_ioctl(struct file *fp, u_long cmd, void *data,
    struct ucred *active_cred, struct thread *td)
{
	switch (cmd) {
	case FIOASYNC:
		if (*(int *)data != 0)
			atomic_set_int(&fp->f_flag, FASYNC);
		else
			atomic_clear_int(&fp->f_flag, FASYNC);
		return (0);
	case FIONBIO:
		if (*(int *)data != 0)
			atomic_set_int(&fp->f_flag, FNONBLOCK);
		else
			atomic_clear_int(&fp->f_flag, FNONBLOCK);
		return (0);
	}
	return (ENOTTY);
}

static int
timerfd_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct timerfd *tfd = fp->f_data;
	int revents = 0;

	mtx_lock(&tfd->tfd_lock);
	if ((events & (POLLIN | POLLRDNORM)) != 0 &&
	    tfd->tfd_count > 0 && tfd->tfd_jumped != TFD_READ)
		revents |= events & (POLLIN | POLLRDNORM);
	if (revents == 0)
		selrecord(td, &tfd->tfd_sel);
	mtx_unlock(&tfd->tfd_lock);

	return (revents);
}

static void
filt_timerfddetach(struct knote *kn)
{
	struct timerfd *tfd = kn->kn_hook;

	mtx_lock(&tfd->tfd_lock);
	knlist_remove(&tfd->tfd_sel.si_note, kn, 1);
	mtx_unlock(&tfd->tfd_lock);
}

static int
filt_timerfdread(struct knote *kn, long hint)
{
	struct timerfd *tfd = kn->kn_hook;

	return (tfd->tfd_count > 0);
}

static struct filterops timerfd_rfiltops = {
	.f_isfd = 1,
	.f_detach = filt_timerfddetach,
	.f_event = filt_timerfdread,
};

static int
timerfd_kqfilter(struct file *fp, struct knote *kn)
{
	struct timerfd *tfd = fp->f_data;

	if (kn->kn_filter != EVFILT_READ)
		return (EINVAL);

	kn->kn_fop = &timerfd_rfiltops;
	kn->kn_hook = tfd;
	knlist_add(&tfd->tfd_sel.si_note, kn, 0);

	return (0);
}

static int
timerfd_stat(struct file *fp, struct stat *sb, struct ucred *active_cred)
{
	struct timerfd *tfd = fp->f_data;

	bzero(sb, sizeof(*sb));
	sb->st_nlink = fp->f_count - 1;
	sb->st_uid = fp->f_cred->cr_uid;
	sb->st_gid = fp->f_cred->cr_gid;
	sb->st_blksize = PAGE_SIZE;

	mtx_lock(&tfd->tfd_lock);
	sb->st_ino = tfd->tfd_ino;
	sb->st_atim = tfd->tfd_atim;
	sb->st_mtim = tfd->tfd_mtim;
	sb->st_birthtim = tfd->tfd_birthtim;
	mtx_unlock(&tfd->tfd_lock);

	return (0);
}

static int
timerfd_close(struct file *fp, struct thread *td)
{
	struct timerfd *tfd = fp->f_data;

	callout_drain(&tfd->tfd_callout);
	seldrain(&tfd->tfd_sel);
	knlist_destroy(&tfd->tfd_sel.si_note);
	mtx_destroy(&tfd->tfd_lock);
	LIST_REMOVE(tfd, entry);
	free(tfd, M_TIMERFD);
	fp->f_ops = &badfileops;

	return (0);
}

static int
timerfd_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{

	struct timerfd *tfd = fp->f_data;

	kif->kf_type = KF_TYPE_TIMERFD;
	mtx_lock(&tfd->tfd_lock);
	kif->kf_un.kf_timerfd.kf_timerfd_clockid = tfd->tfd_clockid;
	kif->kf_un.kf_timerfd.kf_timerfd_flags = tfd->tfd_flags;
	kif->kf_un.kf_timerfd.kf_timerfd_addr = (uintptr_t)tfd;
	mtx_unlock(&tfd->tfd_lock);

	return (0);
}

static struct fileops timerfdops = {
	.fo_read = timerfd_read,
	.fo_write = invfo_rdwr,
	.fo_truncate = invfo_truncate,
	.fo_ioctl = timerfd_ioctl,
	.fo_poll = timerfd_poll,
	.fo_kqfilter = timerfd_kqfilter,
	.fo_stat = timerfd_stat,
	.fo_close = timerfd_close,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = timerfd_fill_kinfo,
	.fo_flags = DFLAG_PASSABLE,
};

static void
timerfd_curval(struct timerfd *tfd, struct itimerspec *old_value)
{
	struct timespec curr_value;

	*old_value = tfd->tfd_time;
	if (timespecisset(&tfd->tfd_time.it_value)) {
		nanouptime(&curr_value);
		timespecsub(&tfd->tfd_time.it_value, &curr_value,
		    &old_value->it_value);
	}
}

static void
timerfd_expire(void *arg)
{
	struct timerfd *tfd = (struct timerfd *)arg;
	struct timespec uptime;

	++tfd->tfd_count;
	tfd->tfd_expired = true;
	if (timespecisset(&tfd->tfd_time.it_interval)) {
		/* Count missed events. */
		nanouptime(&uptime);
		if (timespeccmp(&uptime, &tfd->tfd_time.it_value, >)) {
			timespecsub(&uptime, &tfd->tfd_time.it_value, &uptime);
			tfd->tfd_count += tstosbt(uptime) /
			    tstosbt(tfd->tfd_time.it_interval);
		}
		timespecadd(&tfd->tfd_time.it_value,
		    &tfd->tfd_time.it_interval, &tfd->tfd_time.it_value);
		callout_schedule_sbt(&tfd->tfd_callout,
		    tstosbt(tfd->tfd_time.it_value),
		    0, C_ABSOLUTE);
	} else {
		/* Single shot timer. */
		callout_deactivate(&tfd->tfd_callout);
		timespecclear(&tfd->tfd_time.it_value);
	}

	wakeup(&tfd->tfd_count);
	selwakeup(&tfd->tfd_sel);
	KNOTE_LOCKED(&tfd->tfd_sel.si_note, 0);
}

int
kern_timerfd_create(struct thread *td, int clockid, int flags)
{
	struct file *fp;
	struct timerfd *tfd;
	int error, fd, fflags = 0;

	AUDIT_ARG_VALUE(clockid);
	AUDIT_ARG_FFLAGS(flags);

	if (clockid != CLOCK_REALTIME && clockid != CLOCK_MONOTONIC)
		return (EINVAL);
	if ((flags & ~(TFD_CLOEXEC | TFD_NONBLOCK)) != 0)
		return (EINVAL);
	if ((flags & TFD_CLOEXEC) != 0)
		fflags |= O_CLOEXEC;

	tfd = malloc(sizeof(*tfd), M_TIMERFD, M_WAITOK | M_ZERO);
	if (tfd == NULL)
		return (ENOMEM);
	tfd->tfd_clockid = (clockid_t)clockid;
	tfd->tfd_flags = flags;
	tfd->tfd_ino = alloc_unr64(&tfdino_unr);
	mtx_init(&tfd->tfd_lock, "timerfd", NULL, MTX_DEF);
	callout_init_mtx(&tfd->tfd_callout, &tfd->tfd_lock, 0);
	knlist_init_mtx(&tfd->tfd_sel.si_note, &tfd->tfd_lock);
	timerfd_getboottime(&tfd->tfd_boottim);
	getnanotime(&tfd->tfd_birthtim);
	LIST_INSERT_HEAD(&timerfd_head, tfd, entry);

	error = falloc(td, &fp, &fd, fflags);
	if (error != 0)
		return (error);
	fflags = FREAD;
	if ((flags & TFD_NONBLOCK) != 0)
		fflags |= FNONBLOCK;

	finit(fp, fflags, DTYPE_TIMERFD, tfd, &timerfdops);
	fdrop(fp, td);

	td->td_retval[0] = fd;
	return (0);
}

int
kern_timerfd_gettime(struct thread *td, int fd, struct itimerspec *curr_value)
{
	struct file *fp;
	struct timerfd *tfd;
	int error;

	error = fget(td, fd, &cap_write_rights, &fp);
	if (error != 0)
		return (error);
	tfd = fp->f_data;
	if (tfd == NULL || fp->f_type != DTYPE_TIMERFD) {
		fdrop(fp, td);
		return (EINVAL);
	}

	mtx_lock(&tfd->tfd_lock);
	timerfd_curval(tfd, curr_value);
	mtx_unlock(&tfd->tfd_lock);

	fdrop(fp, td);
	return (0);
}

int
kern_timerfd_settime(struct thread *td, int fd, int flags,
    const struct itimerspec *new_value, struct itimerspec *old_value)
{
	struct file *fp;
	struct timerfd *tfd;
	struct timespec ts;
	int error = 0;

	if ((flags & ~(TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET)) != 0)
		return (EINVAL);
	if (!timespecvalid_interval(&new_value->it_value) ||
	    !timespecvalid_interval(&new_value->it_interval))
		return (EINVAL);

	error = fget(td, fd, &cap_write_rights, &fp);
	if (error != 0)
		return (error);
	tfd = fp->f_data;
	if (tfd == NULL || fp->f_type != DTYPE_TIMERFD) {
		fdrop(fp, td);
		return (EINVAL);
	}

	mtx_lock(&tfd->tfd_lock);
	getnanotime(&tfd->tfd_mtim);
	tfd->tfd_timflags = flags;

	/* Store old itimerspec, if applicable. */
	if (old_value != NULL)
		timerfd_curval(tfd, old_value);

	/* Set new expiration. */
	tfd->tfd_time = *new_value;
	if (timespecisset(&tfd->tfd_time.it_value)) {
		if ((flags & TFD_TIMER_ABSTIME) == 0) {
			nanouptime(&ts);
			timespecadd(&tfd->tfd_time.it_value, &ts,
			    &tfd->tfd_time.it_value);
		} else if (tfd->tfd_clockid == CLOCK_REALTIME) {
			/* ECANCELED if unread jump is pending. */
			if (tfd->tfd_jumped == TFD_CANCELED)
				error = ECANCELED;
			/* Convert from CLOCK_REALTIME to CLOCK_BOOTTIME. */
			timespecsub(&tfd->tfd_time.it_value, &tfd->tfd_boottim,
			    &tfd->tfd_time.it_value);
		}
		callout_reset_sbt(&tfd->tfd_callout,
		    tstosbt(tfd->tfd_time.it_value),
		    0, timerfd_expire, tfd, C_ABSOLUTE);
	} else {
		callout_stop(&tfd->tfd_callout);
	}
	tfd->tfd_count = 0;
	tfd->tfd_expired = false;
	tfd->tfd_jumped = TFD_NOJUMP;
	mtx_unlock(&tfd->tfd_lock);

	fdrop(fp, td);
	return (error);
}

int
sys_timerfd_create(struct thread *td, struct timerfd_create_args *uap)
{
	return (kern_timerfd_create(td, uap->clockid, uap->flags));
}

int
sys_timerfd_gettime(struct thread *td, struct timerfd_gettime_args *uap)
{
	struct itimerspec curr_value;
	int error;

	error = kern_timerfd_gettime(td, uap->fd, &curr_value);
	if (error == 0)
		error = copyout(&curr_value, uap->curr_value,
		    sizeof(curr_value));

	return (error);
}

int
sys_timerfd_settime(struct thread *td, struct timerfd_settime_args *uap)
{
	struct itimerspec new_value, old_value;
	int error;

	error = copyin(uap->new_value, &new_value, sizeof(new_value));
	if (error != 0)
		return (error);
	if (uap->old_value == NULL) {
		error = kern_timerfd_settime(td, uap->fd, uap->flags,
		    &new_value, NULL);
	} else {
		error = kern_timerfd_settime(td, uap->fd, uap->flags,
		    &new_value, &old_value);
		if (error == 0)
			error = copyout(&old_value, uap->old_value,
			    sizeof(old_value));
	}
	return (error);
}

#ifdef COMPAT_FREEBSD32
int
freebsd32_timerfd_gettime(struct thread *td,
    struct freebsd32_timerfd_gettime_args *uap)
{
	struct itimerspec curr_value;
	struct itimerspec32 curr_value32;
	int error;

	error = kern_timerfd_gettime(td, uap->fd, &curr_value);
	if (error == 0) {
		CP(curr_value, curr_value32, it_value.tv_sec);
		CP(curr_value, curr_value32, it_value.tv_nsec);
		CP(curr_value, curr_value32, it_interval.tv_sec);
		CP(curr_value, curr_value32, it_interval.tv_nsec);
		error = copyout(&curr_value32, uap->curr_value,
		    sizeof(curr_value32));
	}

	return (error);
}

int
freebsd32_timerfd_settime(struct thread *td,
    struct freebsd32_timerfd_settime_args *uap)
{
	struct itimerspec new_value, old_value;
	struct itimerspec32 new_value32, old_value32;
	int error;

	error = copyin(uap->new_value, &new_value32, sizeof(new_value32));
	if (error != 0)
		return (error);
	CP(new_value32, new_value, it_value.tv_sec);
	CP(new_value32, new_value, it_value.tv_nsec);
	CP(new_value32, new_value, it_interval.tv_sec);
	CP(new_value32, new_value, it_interval.tv_nsec);
	if (uap->old_value == NULL) {
		error = kern_timerfd_settime(td, uap->fd, uap->flags,
		    &new_value, NULL);
	} else {
		error = kern_timerfd_settime(td, uap->fd, uap->flags,
		    &new_value, &old_value);
		if (error == 0) {
			CP(old_value, old_value32, it_value.tv_sec);
			CP(old_value, old_value32, it_value.tv_nsec);
			CP(old_value, old_value32, it_interval.tv_sec);
			CP(old_value, old_value32, it_interval.tv_nsec);
			error = copyout(&old_value32, uap->old_value,
			    sizeof(old_value32));
		}
	}
	return (error);
}
#endif
