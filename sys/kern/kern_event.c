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
#include <sys/filedesc.h>
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

#include <vm/uma.h>

MALLOC_DEFINE(M_KQUEUE, "kqueue", "memory for kqueue system");

static int	kqueue_scan(struct file *fp, int maxevents,
		    struct kevent *ulistp, const struct timespec *timeout,
		    struct thread *td);
static void 	kqueue_wakeup(struct kqueue *kq);

static fo_rdwr_t	kqueue_read;
static fo_rdwr_t	kqueue_write;
static fo_ioctl_t	kqueue_ioctl;
static fo_poll_t	kqueue_poll;
static fo_kqfilter_t	kqueue_kqfilter;
static fo_stat_t	kqueue_stat;
static fo_close_t	kqueue_close;

static struct fileops kqueueops = {
	kqueue_read,
	kqueue_write,
	kqueue_ioctl,
	kqueue_poll,
	kqueue_kqfilter,
	kqueue_stat,
	kqueue_close,
	0
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

static uma_zone_t	knote_zone;
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

static int
filt_nullattach(struct knote *kn)
{

	return (ENXIO);
};

struct filterops null_filtops =
	{ 0, filt_nullattach, NULL, NULL };

extern struct filterops sig_filtops;

/*
 * Table for for all system-defined filters.
 */
static struct filterops *sysfilt_ops[] = {
	&file_filtops,			/* EVFILT_READ */
	&file_filtops,			/* EVFILT_WRITE */
	&null_filtops,			/* EVFILT_AIO */
	&file_filtops,			/* EVFILT_VNODE */
	&proc_filtops,			/* EVFILT_PROC */
	&sig_filtops,			/* EVFILT_SIGNAL */
	&timer_filtops,			/* EVFILT_TIMER */
	&file_filtops,			/* EVFILT_NETDEV */
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
	struct kqueue *kq = kn->kn_fp->f_data;

	if (kn->kn_filter != EVFILT_READ)
		return (1);

	kn->kn_fop = &kqread_filtops;
	SLIST_INSERT_HEAD(&kq->kq_sel.si_note, kn, kn_selnext);
	return (0);
}

static void
filt_kqdetach(struct knote *kn)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	SLIST_REMOVE(&kq->kq_sel.si_note, kn, knote, kn_selnext);
}

/*ARGSUSED*/
static int
filt_kqueue(struct knote *kn, long hint)
{
	struct kqueue *kq = kn->kn_fp->f_data;

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
	if ((error = p_cansee(curthread, p))) {
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
	    M_KQUEUE, 0);
	callout_init(calloutp, 0);
	callout_reset(calloutp, tticks, filt_timerexpire, kn);
	kn->kn_hook = calloutp;

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
	kq = malloc(sizeof(struct kqueue), M_KQUEUE, M_ZERO);
	TAILQ_INIT(&kq->kq_head);
	FILE_LOCK(fp);
	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_KQUEUE;
	fp->f_ops = &kqueueops;
	TAILQ_INIT(&kq->kq_head);
	fp->f_data = kq;
	FILE_UNLOCK(fp);
	FILEDESC_LOCK(fdp);
	td->td_retval[0] = fd;
	if (fdp->fd_knlistsize < 0)
		fdp->fd_knlistsize = 0;		/* this process has a kq */
	FILEDESC_UNLOCK(fdp);
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

	if ((error = fget(td, uap->fd, &fp)) != 0)
		return (error);
	if (fp->f_type != DTYPE_KQUEUE) {
		fdrop(fp, td);
		return (EBADF);
	}
	if (uap->timeout != NULL) {
		error = copyin(uap->timeout, &ts, sizeof(ts));
		if (error)
			goto done_nogiant;
		uap->timeout = &ts;
	}
	mtx_lock(&Giant);

	kq = fp->f_data;
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
					(void) copyout(kevp,
					    uap->eventlist,
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
	mtx_unlock(&Giant);
done_nogiant:
	if (fp != NULL)
		fdrop(fp, td);
	return (error);
}

int
kqueue_add_filteropts(int filt, struct filterops *filtops)
{

	if (filt > 0)
		panic("filt(%d) > 0", filt);
	if (filt + EVFILT_SYSCOUNT < 0)
		panic("filt(%d) + EVFILT_SYSCOUNT(%d) == %d < 0",
		    filt, EVFILT_SYSCOUNT, filt + EVFILT_SYSCOUNT);
	if (sysfilt_ops[~filt] != &null_filtops)
		panic("sysfilt_ops[~filt(%d)] != &null_filtops", filt);
	sysfilt_ops[~filt] = filtops;
	return (0);
}

int
kqueue_del_filteropts(int filt)
{

	if (filt > 0)
		panic("filt(%d) > 0", filt);
	if (filt + EVFILT_SYSCOUNT < 0)
		panic("filt(%d) + EVFILT_SYSCOUNT(%d) == %d < 0",
		    filt, EVFILT_SYSCOUNT, filt + EVFILT_SYSCOUNT);
	if (sysfilt_ops[~filt] == &null_filtops)
		panic("sysfilt_ops[~filt(%d)] != &null_filtops", filt);
	sysfilt_ops[~filt] = &null_filtops;
	return (0);
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

	FILEDESC_LOCK(fdp);
	if (fops->f_isfd) {
		/* validate descriptor */
		if ((u_int)kev->ident >= fdp->fd_nfiles ||
		    (fp = fdp->fd_ofiles[kev->ident]) == NULL) {
			FILEDESC_UNLOCK(fdp);
			return (EBADF);
		}
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
	FILEDESC_UNLOCK(fdp);

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
			 * filter which has already been triggered.
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
	struct kqueue *kq;
	struct kevent *kevp;
	struct timeval atv, rtv, ttv;
	struct knote *kn, marker;
	int s, count, timeout, nkev = 0, error = 0;

	FILE_LOCK_ASSERT(fp, MA_NOTOWNED);

	kq = fp->f_data;
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
			error = copyout(&kq->kq_kev, ulistp,
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
		error = copyout(&kq->kq_kev, ulistp,
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
kqueue_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
	int flags, struct thread *td)
{
	return (ENXIO);
}

/*ARGSUSED*/
static int
kqueue_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
	 int flags, struct thread *td)
{
	return (ENXIO);
}

/*ARGSUSED*/
static int
kqueue_ioctl(struct file *fp, u_long com, void *data,
	struct ucred *active_cred, struct thread *td)
{
	return (ENOTTY);
}

/*ARGSUSED*/
static int
kqueue_poll(struct file *fp, int events, struct ucred *active_cred,
	struct thread *td)
{
	struct kqueue *kq;
	int revents = 0;
	int s = splnet();

	kq = fp->f_data;
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
kqueue_stat(struct file *fp, struct stat *st, struct ucred *active_cred,
	struct thread *td)
{
	struct kqueue *kq;

	kq = fp->f_data;
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
	struct kqueue *kq = fp->f_data;
	struct filedesc *fdp = kq->kq_fdp;
	struct knote **knp, *kn, *kn0;
	int i;

	FILEDESC_LOCK(fdp);
	for (i = 0; i < fdp->fd_knlistsize; i++) {
		knp = &SLIST_FIRST(&fdp->fd_knlist[i]);
		kn = *knp;
		while (kn != NULL) {
			kn0 = SLIST_NEXT(kn, kn_link);
			if (kq == kn->kn_kq) {
				kn->kn_fop->f_detach(kn);
				*knp = kn0;
				FILE_LOCK(kn->kn_fp);
				FILEDESC_UNLOCK(fdp);
				fdrop_locked(kn->kn_fp, td);
				knote_free(kn);
				FILEDESC_LOCK(fdp);
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
					*knp = kn0;
		/* XXX non-fd release of kn->kn_ptr */
					FILEDESC_UNLOCK(fdp);
					knote_free(kn);
					FILEDESC_LOCK(fdp);
				} else {
					knp = &SLIST_NEXT(kn, kn_link);
				}
				kn = kn0;
			}
		}
	}
	FILEDESC_UNLOCK(fdp);
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
	struct klist *list;

	FILEDESC_LOCK(fdp);
	list = &fdp->fd_knlist[fd];
	FILEDESC_UNLOCK(fdp);
	knote_remove(td, list);
}

static void
knote_attach(struct knote *kn, struct filedesc *fdp)
{
	struct klist *list, *tmp_knhash;
	u_long tmp_knhashmask;
	int size;

	FILEDESC_LOCK(fdp);

	if (! kn->kn_fop->f_isfd) {
		if (fdp->fd_knhashmask == 0) {
			FILEDESC_UNLOCK(fdp);
			tmp_knhash = hashinit(KN_HASHSIZE, M_KQUEUE,
			    &tmp_knhashmask);
			FILEDESC_LOCK(fdp);
			if (fdp->fd_knhashmask == 0) {
				fdp->fd_knhash = tmp_knhash;
				fdp->fd_knhashmask = tmp_knhashmask;
			} else {
				free(tmp_knhash, M_KQUEUE);
			}
		}
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];
		goto done;
	}

	if (fdp->fd_knlistsize <= kn->kn_id) {
		size = fdp->fd_knlistsize;
		while (size <= kn->kn_id)
			size += KQEXTENT;
		FILEDESC_UNLOCK(fdp);
		MALLOC(list, struct klist *,
		    size * sizeof(struct klist *), M_KQUEUE, 0);
		FILEDESC_LOCK(fdp);
		if (fdp->fd_knlistsize > kn->kn_id) {
			FREE(list, M_KQUEUE);
			goto bigenough;
		}
		if (fdp->fd_knlist != NULL) {
			bcopy(fdp->fd_knlist, list,
			    fdp->fd_knlistsize * sizeof(struct klist *));
			FREE(fdp->fd_knlist, M_KQUEUE);
		}
		bzero((caddr_t)list +
		    fdp->fd_knlistsize * sizeof(struct klist *),
		    (size - fdp->fd_knlistsize) * sizeof(struct klist *));
		fdp->fd_knlistsize = size;
		fdp->fd_knlist = list;
	}
bigenough:
	list = &fdp->fd_knlist[kn->kn_id];
done:
	FILEDESC_UNLOCK(fdp);
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

	FILEDESC_LOCK(fdp);
	if (kn->kn_fop->f_isfd)
		list = &fdp->fd_knlist[kn->kn_id];
	else
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];
	if (kn->kn_fop->f_isfd)
		FILE_LOCK(kn->kn_fp);
	FILEDESC_UNLOCK(fdp);

	SLIST_REMOVE(list, kn, knote, kn_link);
	if (kn->kn_status & KN_QUEUED)
		knote_dequeue(kn);
	if (kn->kn_fop->f_isfd)
		fdrop_locked(kn->kn_fp, td);
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
	knote_zone = uma_zcreate("KNOTE", sizeof(struct knote), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);

}
SYSINIT(knote, SI_SUB_PSEUDO, SI_ORDER_ANY, knote_init, NULL)

static struct knote *
knote_alloc(void)
{
	return ((struct knote *)uma_zalloc(knote_zone, 0));
}

static void
knote_free(struct knote *kn)
{
	uma_zfree(knote_zone, kn);
}
