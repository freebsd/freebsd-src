/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon <jlemon@FreeBSD.org>
 * All rights reserved.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/malloc.h> 
#include <sys/unistd.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/selinfo.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/poll.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/uio.h>

#include <vm/vm_zone.h>

MALLOC_DEFINE(M_KQUEUE, "kqueue", "memory for kqueue system");

static int	kqueue_scan(struct file *fp, int maxevents,
		    struct kevent *ulistp, const struct timespec *timeout,
		    struct thread *td);
static int 	kqueue_read(struct file *fp, struct uio *uio,
		    struct ucred *cred, int flags, struct thread *td);
static int	kqueue_write(struct file *fp, struct uio *uio,
		    struct ucred *cred, int flags, struct thread *td);
static int	kqueue_ioctl(struct file *fp, u_long com, caddr_t data,
		    struct thread *td);
static int 	kqueue_poll(struct file *fp, int events, struct ucred *cred,
		    struct thread *td);
static int 	kqueue_kqfilter(struct file *fp, struct knote *kn);
static int 	kqueue_stat(struct file *fp, struct stat *st, struct thread *td);
static int 	kqueue_close(struct file *fp, struct thread *td);
static void 	kqueue_wakeup(struct kqueue *kq);

static struct fileops kqueueops = {
	kqueue_read,
	kqueue_write,
	kqueue_ioctl,
	kqueue_poll,
	kqueue_kqfilter,
	kqueue_stat,
	kqueue_close
};

static void 	knote_attach(struct knote *kn, struct filedesc *fdp);
static void 	knote_drop(struct knote *kn, struct thread *td);
static void 	knote_enqueue(struct knote *kn);
static void 	knote_dequeue(struct knote *kn);
static void 	knote_init(void);
static struct 	knote *knote_alloc(void);
static void 	knote_free(struct knote *kn);

static void	filt_kqdetach(struct knote *kn);
static int	filt_kqueue(struct knote *kn, long hint);
static int	filt_procattach(struct knote *kn);
static void	filt_procdetach(struct knote *kn);
static int	filt_proc(struct knote *kn, long hint);
static int	filt_fileattach(struct knote *kn);
static void	filt_timerexpire(void *knx);
static int	filt_timerattach(struct knote *kn);
static void	filt_timerdetach(struct knote *kn);
static int	filt_timer(struct knote *kn, long hint);

static struct filterops file_filtops =
	{ 1, filt_fileattach, NULL, NULL };
static struct filterops kqread_filtops =
	{ 1, NULL, filt_kqdetach, filt_kqueue };
static struct filterops proc_filtops =
	{ 0, filt_procattach, filt_procdetach, filt_proc };
static struct filterops timer_filtops =
	{ 0, filt_timerattach, filt_timerdetach, filt_timer };

static vm_zone_t	knote_zone;
static int 		kq_ncallouts = 0;
static int 		kq_calloutmax = (4 * 1024);
SYSCTL_INT(_kern, OID_AUTO, kq_calloutmax, CTLFLAG_RW,
    &kq_calloutmax, 0, "Maximum number of callouts allocated for kqueue");

#define KNOTE_ACTIVATE(kn) do { 					\
	kn->kn_status |= KN_ACTIVE;					\
	if ((kn->kn_status & (KN_QUEUED | KN_DISABLED)) == 0)		\
		knote_enqueue(kn);					\
} while(0)

#define	KN_HASHSIZE		64		/* XXX should be tunable */
#define KN_HASH(val, mask)	(((val) ^ (val >> 8)) & (mask))

extern struct filterops aio_filtops;
extern struct filterops sig_filtops;

/*
 * Table for for all system-defined filters.
 */
static struct filterops *sysfilt_ops[] = {
	&file_filtops,			/* EVFILT_READ */
	&file_filtops,			/* EVFILT_WRITE */
	&aio_filtops,			/* EVFILT_AIO */
	&file_filtops,			/* EVFILT_VNODE */
	&proc_filtops,			/* EVFILT_PROC */
	&sig_filtops,			/* EVFILT_SIGNAL */
	&timer_filtops,			/* EVFILT_TIMER */
};

static int
filt_fileattach(struct knote *kn)
{
	
	return (fo_kqfilter(kn->kn_fp, kn));
}

/*ARGSUSED*/
static int
kqueue_kqfilter(struct file *fp, struct knote *kn)
{
	struct kqueue *kq = (struct kqueue *)kn->kn_fp->f_data;

	if (kn->kn_filter != EVFILT_READ)
		return (1);

	kn->kn_fop = &kqread_filtops;
	SLIST_INSERT_HEAD(&kq->kq_sel.si_note, kn, kn_selnext);
	return (0);
}

static void
filt_kqdetach(struct knote *kn)
{
	struct kqueue *kq = (struct kqueue *)kn->kn_fp->f_data;

	SLIST_REMOVE(&kq->kq_sel.si_note, kn, knote, kn_selnext);
}

/*ARGSUSED*/
static int
filt_kqueue(struct knote *kn, long hint)
{
	struct kqueue *kq = (struct kqueue *)kn->kn_fp->f_data;

	kn->kn_data = kq->kq_count;
	return (kn->kn_data > 0);
}

static int
filt_procattach(struct knote *kn)
{
	struct proc *p;
	int error;

	p = pfind(kn->kn_id);
	if (p == NULL)
		return (ESRCH);
	if ((error = p_cansee(curproc, p))) {
		PROC_UNLOCK(p);
		return (error);
	}

	kn->kn_ptr.p_proc = p;
	kn->kn_flags |= EV_CLEAR;		/* automatically set */

	/*
	 * internal flag indicating registration done by kernel
	 */
	if (kn->kn_flags & EV_FLAG1) {
		kn->kn_data = kn->kn_sdata;		/* ppid */
		kn->kn_fflags = NOTE_CHILD;
		kn->kn_flags &= ~EV_FLAG1;
	}

	SLIST_INSERT_HEAD(&p->p_klist, kn, kn_selnext);
	PROC_UNLOCK(p);

	return (0);
}

/*
 * The knote may be attached to a different process, which may exit,
 * leaving nothing for the knote to be attached to.  So when the process
 * exits, the knote is marked as DETACHED and also flagged as ONESHOT so
 * it will be deleted when read out.  However, as part of the knote deletion,
 * this routine is called, so a check is needed to avoid actually performing
 * a detach, because the original process does not exist any more.
 */
static void
filt_procdetach(struct knote *kn)
{
	struct proc *p = kn->kn_ptr.p_proc;

	if (kn->kn_status & KN_DETACHED)
		return;

	PROC_LOCK(p);
	SLIST_REMOVE(&p->p_klist, kn, knote, kn_selnext);
	PROC_UNLOCK(p);
}

static int
filt_proc(struct knote *kn, long hint)
{
	u_int event;

	/*
	 * mask off extra data
	 */
	event = (u_int)hint & NOTE_PCTRLMASK;

	/*
	 * if the user is interested in this event, record it.
	 */
	if (kn->kn_sfflags & event)
		kn->kn_fflags |= event;

	/*
	 * process is gone, so flag the event as finished.
	 */
	if (event == NOTE_EXIT) {
		kn->kn_status |= KN_DETACHED;
		kn->kn_flags |= (EV_EOF | EV_ONESHOT); 
		return (1);
	}

	/*
	 * process forked, and user wants to track the new process,
	 * so attach a new knote to it, and immediately report an
	 * event with the parent's pid.
	 */
	if ((event == NOTE_FORK) && (kn->kn_sfflags & NOTE_TRACK)) {
		struct kevent kev;
		int error;

		/*
		 * register knote with new process.
		 */
		kev.ident = hint & NOTE_PDATAMASK;	/* pid */
		kev.filter = kn->kn_filter;
		kev.flags = kn->kn_flags | EV_ADD | EV_ENABLE | EV_FLAG1;
		kev.fflags = kn->kn_sfflags;
		kev.data = kn->kn_id;			/* parent */
		kev.udata = kn->kn_kevent.udata;	/* preserve udata */
		error = kqueue_register(kn->kn_kq, &kev, NULL);
		if (error)
			kn->kn_fflags |= NOTE_TRACKERR;
	}

	return (kn->kn_fflags != 0);
}

static void
filt_timerexpire(void *knx)
{
	struct knote *kn = knx;
	struct callout *calloutp;
	struct timeval tv;
	int tticks;

	kn->kn_data++;
	KNOTE_ACTIVATE(kn);

	if ((kn->kn_flags & EV_ONESHOT) == 0) {
		tv.tv_sec = kn->kn_sdata / 1000;
		tv.tv_usec = (kn->kn_sdata % 1000) * 1000;
		tticks = tvtohz(&tv);
		calloutp = (struct callout *)kn->kn_hook;
		callout_reset(calloutp, tticks, filt_timerexpire, kn);
	}
}

/*
 * data contains amount of time to sleep, in milliseconds
 */ 
static int
filt_timerattach(struct knote *kn)
{
	struct callout *calloutp;
	struct timeval tv;
	int tticks;

	if (kq_ncallouts >= kq_calloutmax)
		return (ENOMEM);
	kq_ncallouts++;

	tv.tv_sec = kn->kn_sdata / 1000;
	tv.tv_usec = (kn->kn_sdata % 1000) * 1000;
	tticks = tvtohz(&tv);

	kn->kn_flags |= EV_CLEAR;		/* automatically set */
	MALLOC(calloutp, struct callout *, sizeof(*calloutp),
	    M_KQUEUE, M_WAITOK);
	callout_init(calloutp, 0);
	callout_reset(calloutp, tticks, filt_timerexpire, kn);
	kn->kn_hook = (caddr_t)calloutp;

	return (0);
}

static void
filt_timerdetach(struct knote *kn)
{
	struct callout *calloutp;

	calloutp = (struct callout *)kn->kn_hook;
	callout_stop(calloutp);
	FREE(calloutp, M_KQUEUE);
	kq_ncallouts--;
}

static int
filt_timer(struct knote *kn, long hint)
{

	return (kn->kn_data != 0);
}

/*
 * MPSAFE
 */
int
kqueue(struct thread *td, struct kqueue_args *uap)
{
	struct filedesc *fdp;
	struct kqueue *kq;
	struct file *fp;
	int fd, error;

	mtx_lock(&Giant);
	fdp = td->td_proc->p_fd;
	error = falloc(td, &fp, &fd);
	if (error)
		goto done2;
	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_KQUEUE;
	fp->f_ops = &kqueueops;
	kq = malloc(sizeof(struct kqueue), M_KQUEUE, M_WAITOK | M_ZERO);
	TAILQ_INIT(&kq->kq_head);
	fp->f_data = (caddr_t)kq;
	td->td_retval[0] = fd;
	if (fdp->fd_knlistsize < 0)
		fdp->fd_knlistsize = 0;		/* this process has a kq */
	kq->kq_fdp = fdp;
done2:
	mtx_unlock(&Giant);
	return (error);
}

#ifndef _SYS_SYSPROTO_H_
struct kevent_args {
	int	fd;
	const struct kevent *changelist;
	int	nchanges;
	struct	kevent *eventlist;
	int	nevents;
	const struct timespec *timeout;
};
#endif
/*
 * MPSAFE
 */
int
kevent(struct thread *td, struct kevent_args *uap)
{
	struct kevent *kevp;
	struct kqueue *kq;
	struct file *fp;
	struct timespec ts;
	int i, n, nerrors, error;

	mtx_lock(&Giant);
	if ((error = fget(td, uap->fd, &fp)) != 0)
		goto done;
	if (fp->f_type != DTYPE_KQUEUE) {
		error = EBADF;
		goto done;
	}
	if (uap->timeout != NULL) {
		error = copyin(uap->timeout, &ts, sizeof(ts));
		if (error)
			goto done;
		uap->timeout = &ts;
	}

	kq = (struct kqueue *)fp->f_data;
	nerrors = 0;

	while (uap->nchanges > 0) {
		n = uap->nchanges > KQ_NEVENTS ? KQ_NEVENTS : uap->nchanges;
		error = copyin(uap->changelist, kq->kq_kev,
		    n * sizeof(struct kevent));
		if (error)
			goto done;
		for (i = 0; i < n; i++) {
			kevp = &kq->kq_kev[i];
			kevp->flags &= ~EV_SYSFLAGS;
			error = kqueue_register(kq, kevp, td);
			if (error) {
				if (uap->nevents != 0) {
					kevp->flags = EV_ERROR;
					kevp->data = error;
					(void) copyout((caddr_t)kevp,
					    (caddr_t)uap->eventlist,
					    sizeof(*kevp));
					uap->eventlist++;
					uap->nevents--;
					nerrors++;
				} else {
					goto done;
				}
			}
		}
		uap->nchanges -= n;
		uap->changelist += n;
	}
	if (nerrors) {
        	td->td_retval[0] = nerrors;
		error = 0;
		goto done;
	}

	error = kqueue_scan(fp, uap->nevents, uap->eventlist, uap->timeout, td);
done:
	if (fp != NULL)
		fdrop(fp, td);
	mtx_unlock(&Giant);
	return (error);
}

int
kqueue_register(struct kqueue *kq, struct kevent *kev, struct thread *td)
{
	struct filedesc *fdp = kq->kq_fdp;
	struct filterops *fops;
	struct file *fp = NULL;
	struct knote *kn = NULL;
	int s, error = 0;

	if (kev->filter < 0) {
		if (kev->filter + EVFILT_SYSCOUNT < 0)
			return (EINVAL);
		fops = sysfilt_ops[~kev->filter];	/* to 0-base index */
	} else {
		/*
		 * XXX
		 * filter attach routine is responsible for insuring that
		 * the identifier can be attached to it.
		 */
		printf("unknown filter: %d\n", kev->filter);
		return (EINVAL);
	}

	if (fops->f_isfd) {
		/* validate descriptor */
		if ((u_int)kev->ident >= fdp->fd_nfiles ||
		    (fp = fdp->fd_ofiles[kev->ident]) == NULL)
			return (EBADF);
		fhold(fp);

		if (kev->ident < fdp->fd_knlistsize) {
			SLIST_FOREACH(kn, &fdp->fd_knlist[kev->ident], kn_link)
				if (kq == kn->kn_kq &&
				    kev->filter == kn->kn_filter)
					break;
		}
	} else {
		if (fdp->fd_knhashmask != 0) {
			struct klist *list;
			
			list = &fdp->fd_knhash[
			    KN_HASH((u_long)kev->ident, fdp->fd_knhashmask)];
			SLIST_FOREACH(kn, list, kn_link)
				if (kev->ident == kn->kn_id &&
				    kq == kn->kn_kq &&
				    kev->filter == kn->kn_filter)
					break;
		}
	}

	if (kn == NULL && ((kev->flags & EV_ADD) == 0)) {
		error = ENOENT;
		goto done;
	}

	/*
	 * kn now contains the matching knote, or NULL if no match
	 */
	if (kev->flags & EV_ADD) {

		if (kn == NULL) {
			kn = knote_alloc();
			if (kn == NULL) {
				error = ENOMEM;
				goto done;
			}
			kn->kn_fp = fp;
			kn->kn_kq = kq;
			kn->kn_fop = fops;

			/*
			 * apply reference count to knote structure, and
			 * do not release it at the end of this routine.
			 */
			fp = NULL;

			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kev->fflags = 0;
			kev->data = 0;
			kn->kn_kevent = *kev;

			knote_attach(kn, fdp);
			if ((error = fops->f_attach(kn)) != 0) {
				knote_drop(kn, td);
				goto done;
			}
		} else {
			/*
			 * The user may change some filter values after the
			 * initial EV_ADD, but doing so will not reset any 
			 * filter which have already been triggered.
			 */
			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kn->kn_kevent.udata = kev->udata;
		}

		s = splhigh();
		if (kn->kn_fop->f_event(kn, 0))
			KNOTE_ACTIVATE(kn);
		splx(s);

	} else if (kev->flags & EV_DELETE) {
		kn->kn_fop->f_detach(kn);
		knote_drop(kn, td);
		goto done;
	}

	if ((kev->flags & EV_DISABLE) &&
	    ((kn->kn_status & KN_DISABLED) == 0)) {
		s = splhigh();
		kn->kn_status |= KN_DISABLED;
		splx(s);
	}

	if ((kev->flags & EV_ENABLE) && (kn->kn_status & KN_DISABLED)) {
		s = splhigh();
		kn->kn_status &= ~KN_DISABLED;
		if ((kn->kn_status & KN_ACTIVE) &&
		    ((kn->kn_status & KN_QUEUED) == 0))
			knote_enqueue(kn);
		splx(s);
	}

done:
	if (fp != NULL)
		fdrop(fp, td);
	return (error);
}

static int
kqueue_scan(struct file *fp, int maxevents, struct kevent *ulistp,
	const struct timespec *tsp, struct thread *td)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;
	struct kevent *kevp;
	struct timeval atv, rtv, ttv;
	struct knote *kn, marker;
	int s, count, timeout, nkev = 0, error = 0;

	count = maxevents;
	if (count == 0)
		goto done;

	if (tsp != NULL) {
		TIMESPEC_TO_TIMEVAL(&atv, tsp);
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done;
		}
		if (tsp->tv_sec == 0 && tsp->tv_nsec == 0)
			timeout = -1;
		else 
			timeout = atv.tv_sec > 24 * 60 * 60 ?
			    24 * 60 * 60 * hz : tvtohz(&atv);
		getmicrouptime(&rtv);
		timevaladd(&atv, &rtv);
	} else {
		atv.tv_sec = 0;
		atv.tv_usec = 0;
		timeout = 0;
	}
	goto start;

retry:
	if (atv.tv_sec || atv.tv_usec) {
		getmicrouptime(&rtv);
		if (timevalcmp(&rtv, &atv, >=))
			goto done;
		ttv = atv;
		timevalsub(&ttv, &rtv);
		timeout = ttv.tv_sec > 24 * 60 * 60 ?
			24 * 60 * 60 * hz : tvtohz(&ttv);
	}

start:
	kevp = kq->kq_kev;
	s = splhigh();
	if (kq->kq_count == 0) {
		if (timeout < 0) { 
			error = EWOULDBLOCK;
		} else {
			kq->kq_state |= KQ_SLEEP;
			error = tsleep(kq, PSOCK | PCATCH, "kqread", timeout);
		}
		splx(s);
		if (error == 0)
			goto retry;
		/* don't restart after signals... */
		if (error == ERESTART)
			error = EINTR;
		else if (error == EWOULDBLOCK)
			error = 0;
		goto done;
	}

	TAILQ_INSERT_TAIL(&kq->kq_head, &marker, kn_tqe); 
	while (count) {
		kn = TAILQ_FIRST(&kq->kq_head);
		TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe); 
		if (kn == &marker) {
			splx(s);
			if (count == maxevents)
				goto retry;
			goto done;
		}
		if (kn->kn_status & KN_DISABLED) {
			kn->kn_status &= ~KN_QUEUED;
			kq->kq_count--;
			continue;
		}
		if ((kn->kn_flags & EV_ONESHOT) == 0 &&
		    kn->kn_fop->f_event(kn, 0) == 0) {
			kn->kn_status &= ~(KN_QUEUED | KN_ACTIVE);
			kq->kq_count--;
			continue;
		}
		*kevp = kn->kn_kevent;
		kevp++;
		nkev++;
		if (kn->kn_flags & EV_ONESHOT) {
			kn->kn_status &= ~KN_QUEUED;
			kq->kq_count--;
			splx(s);
			kn->kn_fop->f_detach(kn);
			knote_drop(kn, td);
			s = splhigh();
		} else if (kn->kn_flags & EV_CLEAR) {
			kn->kn_data = 0;
			kn->kn_fflags = 0;
			kn->kn_status &= ~(KN_QUEUED | KN_ACTIVE);
			kq->kq_count--;
		} else {
			TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe); 
		}
		count--;
		if (nkev == KQ_NEVENTS) {
			splx(s);
			error = copyout((caddr_t)&kq->kq_kev, (caddr_t)ulistp,
			    sizeof(struct kevent) * nkev);
			ulistp += nkev;
			nkev = 0;
			kevp = kq->kq_kev;
			s = splhigh();
			if (error)
				break;
		}
	}
	TAILQ_REMOVE(&kq->kq_head, &marker, kn_tqe); 
	splx(s);
done:
	if (nkev != 0)
		error = copyout((caddr_t)&kq->kq_kev, (caddr_t)ulistp,
		    sizeof(struct kevent) * nkev);
        td->td_retval[0] = maxevents - count;
	return (error);
}

/*
 * XXX
 * This could be expanded to call kqueue_scan, if desired.
 */
/*ARGSUSED*/
static int
kqueue_read(struct file *fp, struct uio *uio, struct ucred *cred,
	int flags, struct thread *td)
{
	return (ENXIO);
}

/*ARGSUSED*/
static int
kqueue_write(struct file *fp, struct uio *uio, struct ucred *cred,
	 int flags, struct thread *td)
{
	return (ENXIO);
}

/*ARGSUSED*/
static int
kqueue_ioctl(struct file *fp, u_long com, caddr_t data, struct thread *td)
{
	return (ENOTTY);
}

/*ARGSUSED*/
static int
kqueue_poll(struct file *fp, int events, struct ucred *cred, struct thread *td)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;
	int revents = 0;
	int s = splnet();

        if (events & (POLLIN | POLLRDNORM)) {
                if (kq->kq_count) {
                        revents |= events & (POLLIN | POLLRDNORM);
		} else {
                        selrecord(td, &kq->kq_sel);
			kq->kq_state |= KQ_SEL;
		}
	}
	splx(s);
	return (revents);
}

/*ARGSUSED*/
static int
kqueue_stat(struct file *fp, struct stat *st, struct thread *td)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;

	bzero((void *)st, sizeof(*st));
	st->st_size = kq->kq_count;
	st->st_blksize = sizeof(struct kevent);
	st->st_mode = S_IFIFO;
	return (0);
}

/*ARGSUSED*/
static int
kqueue_close(struct file *fp, struct thread *td)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;
	struct filedesc *fdp = td->td_proc->p_fd;
	struct knote **knp, *kn, *kn0;
	int i;

	for (i = 0; i < fdp->fd_knlistsize; i++) {
		knp = &SLIST_FIRST(&fdp->fd_knlist[i]);
		kn = *knp;
		while (kn != NULL) {
			kn0 = SLIST_NEXT(kn, kn_link);
			if (kq == kn->kn_kq) {
				kn->kn_fop->f_detach(kn);
				fdrop(kn->kn_fp, td);
				knote_free(kn);
				*knp = kn0;
			} else {
				knp = &SLIST_NEXT(kn, kn_link);
			}
			kn = kn0;
		}
	}
	if (fdp->fd_knhashmask != 0) {
		for (i = 0; i < fdp->fd_knhashmask + 1; i++) {
			knp = &SLIST_FIRST(&fdp->fd_knhash[i]);
			kn = *knp;
			while (kn != NULL) {
				kn0 = SLIST_NEXT(kn, kn_link);
				if (kq == kn->kn_kq) {
					kn->kn_fop->f_detach(kn);
		/* XXX non-fd release of kn->kn_ptr */
					knote_free(kn);
					*knp = kn0;
				} else {
					knp = &SLIST_NEXT(kn, kn_link);
				}
				kn = kn0;
			}
		}
	}
	free(kq, M_KQUEUE);
	fp->f_data = NULL;

	return (0);
}

static void
kqueue_wakeup(struct kqueue *kq)
{

	if (kq->kq_state & KQ_SLEEP) {
		kq->kq_state &= ~KQ_SLEEP;
		wakeup(kq);
	}
	if (kq->kq_state & KQ_SEL) {
		kq->kq_state &= ~KQ_SEL;
		selwakeup(&kq->kq_sel);
	}
	KNOTE(&kq->kq_sel.si_note, 0);
}

/*
 * walk down a list of knotes, activating them if their event has triggered.
 */
void
knote(struct klist *list, long hint)
{
	struct knote *kn;

	SLIST_FOREACH(kn, list, kn_selnext)
		if (kn->kn_fop->f_event(kn, hint))
			KNOTE_ACTIVATE(kn);
}

/*
 * remove all knotes from a specified klist
 */
void
knote_remove(struct thread *td, struct klist *list)
{
	struct knote *kn;

	while ((kn = SLIST_FIRST(list)) != NULL) {
		kn->kn_fop->f_detach(kn);
		knote_drop(kn, td);
	}
}

/*
 * remove all knotes referencing a specified fd
 */
void
knote_fdclose(struct thread *td, int fd)
{
	struct filedesc *fdp = td->td_proc->p_fd;
	struct klist *list = &fdp->fd_knlist[fd];

	knote_remove(td, list);
}

static void
knote_attach(struct knote *kn, struct filedesc *fdp)
{
	struct klist *list;
	int size;

	if (! kn->kn_fop->f_isfd) {
		if (fdp->fd_knhashmask == 0)
			fdp->fd_knhash = hashinit(KN_HASHSIZE, M_KQUEUE,
			    &fdp->fd_knhashmask);
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];
		goto done;
	}

	if (fdp->fd_knlistsize <= kn->kn_id) {
		size = fdp->fd_knlistsize;
		while (size <= kn->kn_id)
			size += KQEXTENT;
		MALLOC(list, struct klist *,
		    size * sizeof(struct klist *), M_KQUEUE, M_WAITOK);
		bcopy((caddr_t)fdp->fd_knlist, (caddr_t)list,
		    fdp->fd_knlistsize * sizeof(struct klist *));
		bzero((caddr_t)list +
		    fdp->fd_knlistsize * sizeof(struct klist *),
		    (size - fdp->fd_knlistsize) * sizeof(struct klist *));
		if (fdp->fd_knlist != NULL)
			FREE(fdp->fd_knlist, M_KQUEUE);
		fdp->fd_knlistsize = size;
		fdp->fd_knlist = list;
	}
	list = &fdp->fd_knlist[kn->kn_id];
done:
	SLIST_INSERT_HEAD(list, kn, kn_link);
	kn->kn_status = 0;
}

/*
 * should be called at spl == 0, since we don't want to hold spl
 * while calling fdrop and free.
 */
static void
knote_drop(struct knote *kn, struct thread *td)
{
        struct filedesc *fdp = td->td_proc->p_fd;
	struct klist *list;

	if (kn->kn_fop->f_isfd)
		list = &fdp->fd_knlist[kn->kn_id];
	else
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];

	SLIST_REMOVE(list, kn, knote, kn_link);
	if (kn->kn_status & KN_QUEUED)
		knote_dequeue(kn);
	if (kn->kn_fop->f_isfd)
		fdrop(kn->kn_fp, td);
	knote_free(kn);
}


static void
knote_enqueue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;
	int s = splhigh();

	KASSERT((kn->kn_status & KN_QUEUED) == 0, ("knote already queued"));

	TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe); 
	kn->kn_status |= KN_QUEUED;
	kq->kq_count++;
	splx(s);
	kqueue_wakeup(kq);
}

static void
knote_dequeue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;
	int s = splhigh();

	KASSERT(kn->kn_status & KN_QUEUED, ("knote not queued"));

	TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe); 
	kn->kn_status &= ~KN_QUEUED;
	kq->kq_count--;
	splx(s);
}

static void
knote_init(void)
{
	knote_zone = zinit("KNOTE", sizeof(struct knote), 0, 0, 1);
}
SYSINIT(knote, SI_SUB_PSEUDO, SI_ORDER_ANY, knote_init, NULL)

static struct knote *
knote_alloc(void)
{
	return ((struct knote *)zalloc(knote_zone));
}

static void
knote_free(struct knote *kn)
{
	zfree(knote_zone, kn);
}
