/*-
 * Copyright (c) 1999,2000 Jonathan Lemon <jlemon@FreeBSD.org>
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
#include <sys/proc.h>
#include <sys/malloc.h> 
#include <sys/unistd.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/poll.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/sysproto.h>
#include <sys/uio.h>

#include <vm/vm_zone.h>

static int 	filt_nullattach(struct knote *kn);
static int 	filt_rwtypattach(struct knote *kn);
static int	filt_kqattach(struct knote *kn);
static void	filt_kqdetach(struct knote *kn);
static int	filt_kqueue(struct knote *kn, long hint);
static int	filt_procattach(struct knote *kn);
static void	filt_procdetach(struct knote *kn);
static int	filt_proc(struct knote *kn, long hint);

static int	kqueue_scan(struct file *fp, int maxevents,
		    struct kevent *ulistp, const struct timespec *timeout,
		    struct proc *p);
static int 	kqueue_read(struct file *fp, struct uio *uio,
		    struct ucred *cred, int flags, struct proc *p);
static int	kqueue_write(struct file *fp, struct uio *uio,
		    struct ucred *cred, int flags, struct proc *p);
static int	kqueue_ioctl(struct file *fp, u_long com, caddr_t data,
		    struct proc *p);
static int 	kqueue_poll(struct file *fp, int events, struct ucred *cred,
		    struct proc *p);
static int 	kqueue_stat(struct file *fp, struct stat *st, struct proc *p);
static int 	kqueue_close(struct file *fp, struct proc *p);
static void 	kqueue_wakeup(struct kqueue *kq);

static void 	knote_attach(struct knote *kn, struct filedesc *fdp);
static void 	knote_drop(struct knote *kn, struct proc *p);
static void 	knote_enqueue(struct knote *kn);
static void 	knote_dequeue(struct knote *kn);
static void 	knote_init(void);
static struct 	knote *knote_alloc(void);
static void 	knote_free(struct knote *kn);

static vm_zone_t	knote_zone;

#define KNOTE_ACTIVATE(kn) do { 					\
	kn->kn_status |= KN_ACTIVE;					\
	if ((kn->kn_status & (KN_QUEUED | KN_DISABLED)) == 0)		\
		knote_enqueue(kn);					\
} while(0)

#define	KN_HASHSIZE		64		/* XXX should be tunable */
#define KN_HASH(val, mask)	(((val) ^ (val >> 8)) & (mask))

static struct fileops kqueueops = {
	kqueue_read,
	kqueue_write,
	kqueue_ioctl,
	kqueue_poll,
	kqueue_stat,
	kqueue_close
};

extern struct filterops so_rwfiltops[];
extern struct filterops fifo_rwfiltops[];
extern struct filterops pipe_rwfiltops[];
extern struct filterops vn_rwfiltops[];

static struct filterops kq_rwfiltops[] = {
    { 1, filt_kqattach, filt_kqdetach, filt_kqueue },
    { 1, filt_nullattach, NULL, NULL },
};

extern struct filterops aio_filtops;
extern struct filterops sig_filtops;
extern struct filterops vn_filtops;

static struct filterops rwtype_filtops =
	{ 1, filt_rwtypattach, NULL, NULL };
static struct filterops proc_filtops =
	{ 0, filt_procattach, filt_procdetach, filt_proc };

/*
 * XXX
 * These must match the order of defines in <sys/file.h>
 */
static struct filterops *rwtypfilt_sw[] = {
	NULL,				/* 0 */
	vn_rwfiltops,			/* DTYPE_VNODE */
	so_rwfiltops,			/* DTYPE_SOCKET */
	pipe_rwfiltops,			/* DTYPE_PIPE */
	fifo_rwfiltops,			/* DTYPE_FIFO */
	kq_rwfiltops,			/* DTYPE_KQUEUE */
};

/*
 * table for for all system-defined filters.
 */
static struct filterops *sysfilt_ops[] = {
	&rwtype_filtops,		/* EVFILT_READ */
	&rwtype_filtops,		/* EVFILT_WRITE */
	&aio_filtops,			/* EVFILT_AIO */
	&vn_filtops,			/* EVFILT_VNODE */
	&proc_filtops,			/* EVFILT_PROC */
	&sig_filtops,			/* EVFILT_SIGNAL */
};

static int
filt_nullattach(struct knote *kn)
{
	return (ENXIO);
}

/*
 * file-type specific attach routine for read/write filters
 */
static int
filt_rwtypattach(struct knote *kn)
{
	struct filterops *fops;

	fops = rwtypfilt_sw[kn->kn_fp->f_type];
	if (fops == NULL)
		return (EINVAL);
	kn->kn_fop = &fops[~kn->kn_filter];	/* convert to 0-base index */
	return (kn->kn_fop->f_attach(kn));
}

static int
filt_kqattach(struct knote *kn)
{
	struct kqueue *kq = (struct kqueue *)kn->kn_fp->f_data;

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

	p = pfind(kn->kn_id);
	if (p == NULL)
		return (ESRCH);
	if (p_can(curproc, p, P_CAN_SEE, NULL))
		return (EACCES);

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

	/* XXX lock the proc here while adding to the list? */
	SLIST_INSERT_HEAD(&p->p_klist, kn, kn_selnext);

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

	/* XXX locking?  this might modify another process. */
	SLIST_REMOVE(&p->p_klist, kn, knote, kn_selnext);
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

int
kqueue(struct proc *p, struct kqueue_args *uap)
{
	struct filedesc *fdp = p->p_fd;
	struct kqueue *kq;
	struct file *fp;
	int fd, error;

	error = falloc(p, &fp, &fd);
	if (error)
		return (error);
	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_KQUEUE;
	fp->f_ops = &kqueueops;
	kq = malloc(sizeof(struct kqueue), M_TEMP, M_WAITOK);
	bzero(kq, sizeof(*kq));
	TAILQ_INIT(&kq->kq_head);
	fp->f_data = (caddr_t)kq;
	p->p_retval[0] = fd;
	if (fdp->fd_knlistsize < 0)
		fdp->fd_knlistsize = 0;		/* this process has a kq */
	kq->kq_fdp = fdp;
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
int
kevent(struct proc *p, struct kevent_args *uap)
{
	struct filedesc* fdp = p->p_fd;
	struct kevent *kevp;
	struct kqueue *kq;
	struct file *fp;
	struct timespec ts;
	int i, n, nerrors, error;

        if (((u_int)uap->fd) >= fdp->fd_nfiles ||
            (fp = fdp->fd_ofiles[uap->fd]) == NULL ||
	    (fp->f_type != DTYPE_KQUEUE))
		return (EBADF);

	if (uap->timeout != NULL) {
		error = copyin(uap->timeout, &ts, sizeof(ts));
		if (error)
			return error;
		uap->timeout = &ts;
	}

	kq = (struct kqueue *)fp->f_data;
	nerrors = 0;

	while (uap->nchanges > 0) {
		n = uap->nchanges > KQ_NEVENTS ? KQ_NEVENTS : uap->nchanges;
		error = copyin(uap->changelist, kq->kq_kev,
		    n * sizeof(struct kevent));
		if (error)
			return (error);
		for (i = 0; i < n; i++) {
			kevp = &kq->kq_kev[i];
			kevp->flags &= ~EV_SYSFLAGS;
			error = kqueue_register(kq, kevp, p);
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
					return (error);
				}
			}
		}
		uap->nchanges -= n;
		uap->changelist += n;
	}
	if (nerrors) {
        	p->p_retval[0] = nerrors;
		return (0);
	}

	error = kqueue_scan(fp, uap->nevents, uap->eventlist, uap->timeout, p);
	return (error);
}

int
kqueue_register(struct kqueue *kq, struct kevent *kev, struct proc *p)
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

	if (kn == NULL && ((kev->flags & EV_ADD) == 0))
		return (ENOENT);

	/*
	 * kn now contains the matching knote, or NULL if no match
	 */
	if (kev->flags & EV_ADD) {

		if (kn == NULL) {
			kn = knote_alloc();
			if (kn == NULL)
				return (ENOMEM);
			if (fp != NULL)
				fhold(fp);
			kn->kn_fp = fp;
			kn->kn_kq = kq;
			kn->kn_fop = fops;

			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kev->fflags = 0;
			kev->data = 0;
			kn->kn_kevent = *kev;

			knote_attach(kn, fdp);
			if ((error = fops->f_attach(kn)) != 0) {
				knote_drop(kn, p);
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
		knote_drop(kn, p);
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
	return (error);
}

static int
kqueue_scan(struct file *fp, int maxevents, struct kevent *ulistp,
	const struct timespec *tsp, struct proc *p)
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
			knote_drop(kn, p);
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
        p->p_retval[0] = maxevents - count;
	return (error);
}

/*
 * XXX
 * This could be expanded to call kqueue_scan, if desired.
 */
/*ARGSUSED*/
static int
kqueue_read(struct file *fp, struct uio *uio, struct ucred *cred,
	int flags, struct proc *p)
{
	return (ENXIO);
}

/*ARGSUSED*/
static int
kqueue_write(struct file *fp, struct uio *uio, struct ucred *cred,
	 int flags, struct proc *p)
{
	return (ENXIO);
}

/*ARGSUSED*/
static int
kqueue_ioctl(struct file *fp, u_long com, caddr_t data, struct proc *p)
{
	return (ENOTTY);
}

/*ARGSUSED*/
static int
kqueue_poll(struct file *fp, int events, struct ucred *cred, struct proc *p)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;
	int revents = 0;
	int s = splnet();

        if (events & (POLLIN | POLLRDNORM)) {
                if (kq->kq_count) {
                        revents |= events & (POLLIN | POLLRDNORM);
		} else {
                        selrecord(p, &kq->kq_sel);
			kq->kq_state |= KQ_SEL;
		}
	}
	splx(s);
	return (revents);
}

/*ARGSUSED*/
static int
kqueue_stat(struct file *fp, struct stat *st, struct proc *p)
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
kqueue_close(struct file *fp, struct proc *p)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;
	struct filedesc *fdp = p->p_fd;
	struct knote **knp, *kn, *kn0;
	int i;

	for (i = 0; i < fdp->fd_knlistsize; i++) {
		knp = &SLIST_FIRST(&fdp->fd_knlist[i]);
		kn = *knp;
		while (kn != NULL) {
			kn0 = SLIST_NEXT(kn, kn_link);
			if (kq == kn->kn_kq) {
				kn->kn_fop->f_detach(kn);
				fdrop(kn->kn_fp, p);
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
	free(kq, M_TEMP);
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
knote_remove(struct proc *p, struct klist *list)
{
	struct knote *kn;

	while ((kn = SLIST_FIRST(list)) != NULL) {
		kn->kn_fop->f_detach(kn);
		knote_drop(kn, p);
	}
}

/*
 * remove all knotes referencing a specified fd
 */
void
knote_fdclose(struct proc *p, int fd)
{
	struct filedesc *fdp = p->p_fd;
	struct klist *list = &fdp->fd_knlist[fd];

	knote_remove(p, list);
}

static void
knote_attach(struct knote *kn, struct filedesc *fdp)
{
	struct klist *list;
	int size;

	if (! kn->kn_fop->f_isfd) {
		if (fdp->fd_knhashmask == 0)
			fdp->fd_knhash = hashinit(KN_HASHSIZE, M_TEMP,
			    &fdp->fd_knhashmask);
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];
		goto done;
	}

	if (fdp->fd_knlistsize <= kn->kn_id) {
		size = fdp->fd_knlistsize;
		while (size <= kn->kn_id)
			size += KQEXTENT;
		MALLOC(list, struct klist *,
		    size * sizeof(struct klist *), M_TEMP, M_WAITOK);
		bcopy((caddr_t)fdp->fd_knlist, (caddr_t)list,
		    fdp->fd_knlistsize * sizeof(struct klist *));
		bzero((caddr_t)list +
		    fdp->fd_knlistsize * sizeof(struct klist *),
		    (size - fdp->fd_knlistsize) * sizeof(struct klist *));
		if (fdp->fd_knlist != NULL)
			FREE(fdp->fd_knlist, M_TEMP);
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
knote_drop(struct knote *kn, struct proc *p)
{
        struct filedesc *fdp = p->p_fd;
	struct klist *list;

	if (kn->kn_fop->f_isfd)
		list = &fdp->fd_knlist[kn->kn_id];
	else
		list = &fdp->fd_knhash[KN_HASH(kn->kn_id, fdp->fd_knhashmask)];

	SLIST_REMOVE(list, kn, knote, kn_link);
	if (kn->kn_status & KN_QUEUED)
		knote_dequeue(kn);
	if (kn->kn_fop->f_isfd)
		fdrop(kn->kn_fp, p);
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
