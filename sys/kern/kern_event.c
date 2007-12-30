/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon <jlemon@FreeBSD.org>
 * Copyright 2004 John-Mark Gurney <jmg@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ktrace.h"

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
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/kthread.h>
#include <sys/selinfo.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/poll.h>
#include <sys/protosw.h>
#include <sys/sigio.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/syscallsubr.h>
#include <sys/taskqueue.h>
#include <sys/uio.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <vm/uma.h>

static MALLOC_DEFINE(M_KQUEUE, "kqueue", "memory for kqueue system");

/*
 * This lock is used if multiple kq locks are required.  This possibly
 * should be made into a per proc lock.
 */
static struct mtx	kq_global;
MTX_SYSINIT(kq_global, &kq_global, "kqueue order", MTX_DEF);
#define KQ_GLOBAL_LOCK(lck, haslck)	do {	\
	if (!haslck)				\
		mtx_lock(lck);			\
	haslck = 1;				\
} while (0)
#define KQ_GLOBAL_UNLOCK(lck, haslck)	do {	\
	if (haslck)				\
		mtx_unlock(lck);			\
	haslck = 0;				\
} while (0)

TASKQUEUE_DEFINE_THREAD(kqueue);

static int	kevent_copyout(void *arg, struct kevent *kevp, int count);
static int	kevent_copyin(void *arg, struct kevent *kevp, int count);
static int	kqueue_register(struct kqueue *kq, struct kevent *kev,
		    struct thread *td, int waitok);
static int	kqueue_acquire(struct file *fp, struct kqueue **kqp);
static void	kqueue_release(struct kqueue *kq, int locked);
static int	kqueue_expand(struct kqueue *kq, struct filterops *fops,
		    uintptr_t ident, int waitok);
static void	kqueue_task(void *arg, int pending);
static int	kqueue_scan(struct kqueue *kq, int maxevents,
		    struct kevent_copyops *k_ops,
		    const struct timespec *timeout,
		    struct kevent *keva, struct thread *td);
static void 	kqueue_wakeup(struct kqueue *kq);
static struct filterops *kqueue_fo_find(int filt);
static void	kqueue_fo_release(int filt);

static fo_rdwr_t	kqueue_read;
static fo_rdwr_t	kqueue_write;
static fo_ioctl_t	kqueue_ioctl;
static fo_poll_t	kqueue_poll;
static fo_kqfilter_t	kqueue_kqfilter;
static fo_stat_t	kqueue_stat;
static fo_close_t	kqueue_close;

static struct fileops kqueueops = {
	.fo_read = kqueue_read,
	.fo_write = kqueue_write,
	.fo_ioctl = kqueue_ioctl,
	.fo_poll = kqueue_poll,
	.fo_kqfilter = kqueue_kqfilter,
	.fo_stat = kqueue_stat,
	.fo_close = kqueue_close,
};

static int 	knote_attach(struct knote *kn, struct kqueue *kq);
static void 	knote_drop(struct knote *kn, struct thread *td);
static void 	knote_enqueue(struct knote *kn);
static void 	knote_dequeue(struct knote *kn);
static void 	knote_init(void);
static struct 	knote *knote_alloc(int waitok);
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
/* XXX - move to kern_proc.c?  */
static struct filterops proc_filtops =
	{ 0, filt_procattach, filt_procdetach, filt_proc };
static struct filterops timer_filtops =
	{ 0, filt_timerattach, filt_timerdetach, filt_timer };

static uma_zone_t	knote_zone;
static int 		kq_ncallouts = 0;
static int 		kq_calloutmax = (4 * 1024);
SYSCTL_INT(_kern, OID_AUTO, kq_calloutmax, CTLFLAG_RW,
    &kq_calloutmax, 0, "Maximum number of callouts allocated for kqueue");

/* XXX - ensure not KN_INFLUX?? */
#define KNOTE_ACTIVATE(kn, islock) do { 				\
	if ((islock))							\
		mtx_assert(&(kn)->kn_kq->kq_lock, MA_OWNED);		\
	else								\
		KQ_LOCK((kn)->kn_kq);					\
	(kn)->kn_status |= KN_ACTIVE;					\
	if (((kn)->kn_status & (KN_QUEUED | KN_DISABLED)) == 0)		\
		knote_enqueue((kn));					\
	if (!(islock))							\
		KQ_UNLOCK((kn)->kn_kq);					\
} while(0)
#define KQ_LOCK(kq) do {						\
	mtx_lock(&(kq)->kq_lock);					\
} while (0)
#define KQ_FLUX_WAKEUP(kq) do {						\
	if (((kq)->kq_state & KQ_FLUXWAIT) == KQ_FLUXWAIT) {		\
		(kq)->kq_state &= ~KQ_FLUXWAIT;				\
		wakeup((kq));						\
	}								\
} while (0)
#define KQ_UNLOCK_FLUX(kq) do {						\
	KQ_FLUX_WAKEUP(kq);						\
	mtx_unlock(&(kq)->kq_lock);					\
} while (0)
#define KQ_UNLOCK(kq) do {						\
	mtx_unlock(&(kq)->kq_lock);					\
} while (0)
#define KQ_OWNED(kq) do {						\
	mtx_assert(&(kq)->kq_lock, MA_OWNED);				\
} while (0)
#define KQ_NOTOWNED(kq) do {						\
	mtx_assert(&(kq)->kq_lock, MA_NOTOWNED);			\
} while (0)
#define KN_LIST_LOCK(kn) do {						\
	if (kn->kn_knlist != NULL)					\
		kn->kn_knlist->kl_lock(kn->kn_knlist->kl_lockarg);	\
} while (0)
#define KN_LIST_UNLOCK(kn) do {						\
	if (kn->kn_knlist != NULL) 					\
		kn->kn_knlist->kl_unlock(kn->kn_knlist->kl_lockarg);	\
} while (0)
#define	KNL_ASSERT_LOCK(knl, islocked) do {				\
	if (islocked)							\
		KNL_ASSERT_LOCKED(knl);				\
	else								\
		KNL_ASSERT_UNLOCKED(knl);				\
} while (0)
#ifdef INVARIANTS
#define	KNL_ASSERT_LOCKED(knl) do {					\
	if (!knl->kl_locked((knl)->kl_lockarg))				\
			panic("knlist not locked, but should be");	\
} while (0)
#define	KNL_ASSERT_UNLOCKED(knl) do {				\
	if (knl->kl_locked((knl)->kl_lockarg))				\
		panic("knlist locked, but should not be");		\
} while (0)
#else /* !INVARIANTS */
#define	KNL_ASSERT_LOCKED(knl) do {} while(0)
#define	KNL_ASSERT_UNLOCKED(knl) do {} while (0)
#endif /* INVARIANTS */

#define	KN_HASHSIZE		64		/* XXX should be tunable */
#define KN_HASH(val, mask)	(((val) ^ (val >> 8)) & (mask))

static int
filt_nullattach(struct knote *kn)
{

	return (ENXIO);
};

struct filterops null_filtops =
	{ 0, filt_nullattach, NULL, NULL };

/* XXX - make SYSINIT to add these, and move into respective modules. */
extern struct filterops sig_filtops;
extern struct filterops fs_filtops;

/*
 * Table for for all system-defined filters.
 */
static struct mtx	filterops_lock;
MTX_SYSINIT(kqueue_filterops, &filterops_lock, "protect sysfilt_ops",
	MTX_DEF);
static struct {
	struct filterops *for_fop;
	int for_refcnt;
} sysfilt_ops[EVFILT_SYSCOUNT] = {
	{ &file_filtops },			/* EVFILT_READ */
	{ &file_filtops },			/* EVFILT_WRITE */
	{ &null_filtops },			/* EVFILT_AIO */
	{ &file_filtops },			/* EVFILT_VNODE */
	{ &proc_filtops },			/* EVFILT_PROC */
	{ &sig_filtops },			/* EVFILT_SIGNAL */
	{ &timer_filtops },			/* EVFILT_TIMER */
	{ &file_filtops },			/* EVFILT_NETDEV */
	{ &fs_filtops },			/* EVFILT_FS */
	{ &null_filtops },			/* EVFILT_LIO */
};

/*
 * Simple redirection for all cdevsw style objects to call their fo_kqfilter
 * method.
 */
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
		return (EINVAL);

	kn->kn_status |= KN_KQUEUE;
	kn->kn_fop = &kqread_filtops;
	knlist_add(&kq->kq_sel.si_note, kn, 0);

	return (0);
}

static void
filt_kqdetach(struct knote *kn)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	knlist_remove(&kq->kq_sel.si_note, kn, 0);
}

/*ARGSUSED*/
static int
filt_kqueue(struct knote *kn, long hint)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	kn->kn_data = kq->kq_count;
	return (kn->kn_data > 0);
}

/* XXX - move to kern_proc.c?  */
static int
filt_procattach(struct knote *kn)
{
	struct proc *p;
	int immediate;
	int error;

	immediate = 0;
	p = pfind(kn->kn_id);
	if (p == NULL && (kn->kn_sfflags & NOTE_EXIT)) {
		p = zpfind(kn->kn_id);
		immediate = 1;
	} else if (p != NULL && (p->p_flag & P_WEXIT)) {
		immediate = 1;
	}

	if (p == NULL)
		return (ESRCH);
	if ((error = p_cansee(curthread, p)))
		return (error);

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

	if (immediate == 0)
		knlist_add(&p->p_klist, kn, 1);

	/*
	 * Immediately activate any exit notes if the target process is a
	 * zombie.  This is necessary to handle the case where the target
	 * process, e.g. a child, dies before the kevent is registered.
	 */
	if (immediate && filt_proc(kn, NOTE_EXIT))
		KNOTE_ACTIVATE(kn, 0);

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
/* XXX - move to kern_proc.c?  */
static void
filt_procdetach(struct knote *kn)
{
	struct proc *p;

	p = kn->kn_ptr.p_proc;
	knlist_remove(&p->p_klist, kn, 0);
	kn->kn_ptr.p_proc = NULL;
}

/* XXX - move to kern_proc.c?  */
static int
filt_proc(struct knote *kn, long hint)
{
	struct proc *p = kn->kn_ptr.p_proc;
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
		if (!(kn->kn_status & KN_DETACHED))
			knlist_remove_inevent(&p->p_klist, kn);
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		kn->kn_data = p->p_xstat;
		kn->kn_ptr.p_proc = NULL;
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
		error = kqueue_register(kn->kn_kq, &kev, NULL, 0);
		if (error)
			kn->kn_fflags |= NOTE_TRACKERR;
	}

	return (kn->kn_fflags != 0);
}

static int
timertoticks(intptr_t data)
{
	struct timeval tv;
	int tticks;

	tv.tv_sec = data / 1000;
	tv.tv_usec = (data % 1000) * 1000;
	tticks = tvtohz(&tv);

	return tticks;
}

/* XXX - move to kern_timeout.c? */
static void
filt_timerexpire(void *knx)
{
	struct knote *kn = knx;
	struct callout *calloutp;

	kn->kn_data++;
	KNOTE_ACTIVATE(kn, 0);	/* XXX - handle locking */

	if ((kn->kn_flags & EV_ONESHOT) != EV_ONESHOT) {
		calloutp = (struct callout *)kn->kn_hook;
		callout_reset(calloutp, timertoticks(kn->kn_sdata),
		    filt_timerexpire, kn);
	}
}

/*
 * data contains amount of time to sleep, in milliseconds
 */
/* XXX - move to kern_timeout.c? */
static int
filt_timerattach(struct knote *kn)
{
	struct callout *calloutp;

	atomic_add_int(&kq_ncallouts, 1);

	if (kq_ncallouts >= kq_calloutmax) {
		atomic_add_int(&kq_ncallouts, -1);
		return (ENOMEM);
	}

	kn->kn_flags |= EV_CLEAR;		/* automatically set */
	kn->kn_status &= ~KN_DETACHED;		/* knlist_add usually sets it */
	MALLOC(calloutp, struct callout *, sizeof(*calloutp),
	    M_KQUEUE, M_WAITOK);
	callout_init(calloutp, CALLOUT_MPSAFE);
	kn->kn_hook = calloutp;
	callout_reset(calloutp, timertoticks(kn->kn_sdata), filt_timerexpire,
	    kn);

	return (0);
}

/* XXX - move to kern_timeout.c? */
static void
filt_timerdetach(struct knote *kn)
{
	struct callout *calloutp;

	calloutp = (struct callout *)kn->kn_hook;
	callout_drain(calloutp);
	FREE(calloutp, M_KQUEUE);
	atomic_add_int(&kq_ncallouts, -1);
	kn->kn_status |= KN_DETACHED;	/* knlist_remove usually clears it */
}

/* XXX - move to kern_timeout.c? */
static int
filt_timer(struct knote *kn, long hint)
{

	return (kn->kn_data != 0);
}

int
kqueue(struct thread *td, struct kqueue_args *uap)
{
	struct filedesc *fdp;
	struct kqueue *kq;
	struct file *fp;
	int fd, error;

	fdp = td->td_proc->p_fd;
	error = falloc(td, &fp, &fd);
	if (error)
		goto done2;

	/* An extra reference on `nfp' has been held for us by falloc(). */
	kq = malloc(sizeof *kq, M_KQUEUE, M_WAITOK | M_ZERO);
	mtx_init(&kq->kq_lock, "kqueue", NULL, MTX_DEF|MTX_DUPOK);
	TAILQ_INIT(&kq->kq_head);
	kq->kq_fdp = fdp;
	knlist_init(&kq->kq_sel.si_note, &kq->kq_lock, NULL, NULL, NULL);
	TASK_INIT(&kq->kq_task, 0, kqueue_task, kq);

	FILEDESC_XLOCK(fdp);
	SLIST_INSERT_HEAD(&fdp->fd_kqlist, kq, kq_list);
	FILEDESC_XUNLOCK(fdp);

	finit(fp, FREAD | FWRITE, DTYPE_KQUEUE, kq, &kqueueops);
	fdrop(fp, td);

	td->td_retval[0] = fd;
done2:
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
kevent(struct thread *td, struct kevent_args *uap)
{
	struct timespec ts, *tsp;
	struct kevent_copyops k_ops = { uap,
					kevent_copyout,
					kevent_copyin};
	int error;
#ifdef KTRACE
	struct uio ktruio;
	struct iovec ktriov;
	struct uio *ktruioin = NULL;
	struct uio *ktruioout = NULL;
#endif

	if (uap->timeout != NULL) {
		error = copyin(uap->timeout, &ts, sizeof(ts));
		if (error)
			return (error);
		tsp = &ts;
	} else
		tsp = NULL;

#ifdef KTRACE
	if (KTRPOINT(td, KTR_GENIO)) {
		ktriov.iov_base = uap->changelist;
		ktriov.iov_len = uap->nchanges * sizeof(struct kevent);
		ktruio = (struct uio){ .uio_iov = &ktriov, .uio_iovcnt = 1,
		    .uio_segflg = UIO_USERSPACE, .uio_rw = UIO_READ,
		    .uio_td = td };
		ktruioin = cloneuio(&ktruio);
		ktriov.iov_base = uap->eventlist;
		ktriov.iov_len = uap->nevents * sizeof(struct kevent);
		ktruioout = cloneuio(&ktruio);
	}
#endif

	error = kern_kevent(td, uap->fd, uap->nchanges, uap->nevents,
	    &k_ops, tsp);

#ifdef KTRACE
	if (ktruioin != NULL) {
		ktruioin->uio_resid = uap->nchanges * sizeof(struct kevent);
		ktrgenio(uap->fd, UIO_WRITE, ktruioin, 0);
		ktruioout->uio_resid = td->td_retval[0] * sizeof(struct kevent);
		ktrgenio(uap->fd, UIO_READ, ktruioout, error);
	}
#endif

	return (error);
}

/*
 * Copy 'count' items into the destination list pointed to by uap->eventlist.
 */
static int
kevent_copyout(void *arg, struct kevent *kevp, int count)
{
	struct kevent_args *uap;
	int error;

	KASSERT(count <= KQ_NEVENTS, ("count (%d) > KQ_NEVENTS", count));
	uap = (struct kevent_args *)arg;

	error = copyout(kevp, uap->eventlist, count * sizeof *kevp);
	if (error == 0)
		uap->eventlist += count;
	return (error);
}

/*
 * Copy 'count' items from the list pointed to by uap->changelist.
 */
static int
kevent_copyin(void *arg, struct kevent *kevp, int count)
{
	struct kevent_args *uap;
	int error;

	KASSERT(count <= KQ_NEVENTS, ("count (%d) > KQ_NEVENTS", count));
	uap = (struct kevent_args *)arg;

	error = copyin(uap->changelist, kevp, count * sizeof *kevp);
	if (error == 0)
		uap->changelist += count;
	return (error);
}

int
kern_kevent(struct thread *td, int fd, int nchanges, int nevents,
    struct kevent_copyops *k_ops, const struct timespec *timeout)
{
	struct kevent keva[KQ_NEVENTS];
	struct kevent *kevp, *changes;
	struct kqueue *kq;
	struct file *fp;
	int i, n, nerrors, error;

	if ((error = fget(td, fd, &fp)) != 0)
		return (error);
	if ((error = kqueue_acquire(fp, &kq)) != 0)
		goto done_norel;

	nerrors = 0;

	while (nchanges > 0) {
		n = nchanges > KQ_NEVENTS ? KQ_NEVENTS : nchanges;
		error = k_ops->k_copyin(k_ops->arg, keva, n);
		if (error)
			goto done;
		changes = keva;
		for (i = 0; i < n; i++) {
			kevp = &changes[i];
			if (!kevp->filter)
				continue;
			kevp->flags &= ~EV_SYSFLAGS;
			error = kqueue_register(kq, kevp, td, 1);
			if (error) {
				if (nevents != 0) {
					kevp->flags = EV_ERROR;
					kevp->data = error;
					(void) k_ops->k_copyout(k_ops->arg,
					    kevp, 1);
					nevents--;
					nerrors++;
				} else {
					goto done;
				}
			}
		}
		nchanges -= n;
	}
	if (nerrors) {
		td->td_retval[0] = nerrors;
		error = 0;
		goto done;
	}

	error = kqueue_scan(kq, nevents, k_ops, timeout, keva, td);
done:
	kqueue_release(kq, 0);
done_norel:
	fdrop(fp, td);
	return (error);
}

int
kqueue_add_filteropts(int filt, struct filterops *filtops)
{
	int error;

	if (filt > 0 || filt + EVFILT_SYSCOUNT < 0) {
		printf(
"trying to add a filterop that is out of range: %d is beyond %d\n",
		    ~filt, EVFILT_SYSCOUNT);
		return EINVAL;
	}
	mtx_lock(&filterops_lock);
	if (sysfilt_ops[~filt].for_fop != &null_filtops &&
	    sysfilt_ops[~filt].for_fop != NULL)
		error = EEXIST;
	else {
		sysfilt_ops[~filt].for_fop = filtops;
		sysfilt_ops[~filt].for_refcnt = 0;
	}
	mtx_unlock(&filterops_lock);

	return (0);
}

int
kqueue_del_filteropts(int filt)
{
	int error;

	error = 0;
	if (filt > 0 || filt + EVFILT_SYSCOUNT < 0)
		return EINVAL;

	mtx_lock(&filterops_lock);
	if (sysfilt_ops[~filt].for_fop == &null_filtops ||
	    sysfilt_ops[~filt].for_fop == NULL)
		error = EINVAL;
	else if (sysfilt_ops[~filt].for_refcnt != 0)
		error = EBUSY;
	else {
		sysfilt_ops[~filt].for_fop = &null_filtops;
		sysfilt_ops[~filt].for_refcnt = 0;
	}
	mtx_unlock(&filterops_lock);

	return error;
}

static struct filterops *
kqueue_fo_find(int filt)
{

	if (filt > 0 || filt + EVFILT_SYSCOUNT < 0)
		return NULL;

	mtx_lock(&filterops_lock);
	sysfilt_ops[~filt].for_refcnt++;
	if (sysfilt_ops[~filt].for_fop == NULL)
		sysfilt_ops[~filt].for_fop = &null_filtops;
	mtx_unlock(&filterops_lock);

	return sysfilt_ops[~filt].for_fop;
}

static void
kqueue_fo_release(int filt)
{

	if (filt > 0 || filt + EVFILT_SYSCOUNT < 0)
		return;

	mtx_lock(&filterops_lock);
	KASSERT(sysfilt_ops[~filt].for_refcnt > 0,
	    ("filter object refcount not valid on release"));
	sysfilt_ops[~filt].for_refcnt--;
	mtx_unlock(&filterops_lock);
}

/*
 * A ref to kq (obtained via kqueue_acquire) must be held.  waitok will
 * influence if memory allocation should wait.  Make sure it is 0 if you
 * hold any mutexes.
 */
static int
kqueue_register(struct kqueue *kq, struct kevent *kev, struct thread *td, int waitok)
{
	struct filterops *fops;
	struct file *fp;
	struct knote *kn, *tkn;
	int error, filt, event;
	int haskqglobal;

	fp = NULL;
	kn = NULL;
	error = 0;
	haskqglobal = 0;

	filt = kev->filter;
	fops = kqueue_fo_find(filt);
	if (fops == NULL)
		return EINVAL;

	tkn = knote_alloc(waitok);		/* prevent waiting with locks */

findkn:
	if (fops->f_isfd) {
		KASSERT(td != NULL, ("td is NULL"));
		error = fget(td, kev->ident, &fp);
		if (error)
			goto done;

		if ((kev->flags & EV_ADD) == EV_ADD && kqueue_expand(kq, fops,
		    kev->ident, 0) != 0) {
			/* try again */
			fdrop(fp, td);
			fp = NULL;
			error = kqueue_expand(kq, fops, kev->ident, waitok);
			if (error)
				goto done;
			goto findkn;
		}

		if (fp->f_type == DTYPE_KQUEUE) {
			/*
			 * if we add some inteligence about what we are doing,
			 * we should be able to support events on ourselves.
			 * We need to know when we are doing this to prevent
			 * getting both the knlist lock and the kq lock since
			 * they are the same thing.
			 */
			if (fp->f_data == kq) {
				error = EINVAL;
				goto done;
			}

			KQ_GLOBAL_LOCK(&kq_global, haskqglobal);
		}

		KQ_LOCK(kq);
		if (kev->ident < kq->kq_knlistsize) {
			SLIST_FOREACH(kn, &kq->kq_knlist[kev->ident], kn_link)
				if (kev->filter == kn->kn_filter)
					break;
		}
	} else {
		if ((kev->flags & EV_ADD) == EV_ADD)
			kqueue_expand(kq, fops, kev->ident, waitok);

		KQ_LOCK(kq);
		if (kq->kq_knhashmask != 0) {
			struct klist *list;

			list = &kq->kq_knhash[
			    KN_HASH((u_long)kev->ident, kq->kq_knhashmask)];
			SLIST_FOREACH(kn, list, kn_link)
				if (kev->ident == kn->kn_id &&
				    kev->filter == kn->kn_filter)
					break;
		}
	}

	/* knote is in the process of changing, wait for it to stablize. */
	if (kn != NULL && (kn->kn_status & KN_INFLUX) == KN_INFLUX) {
		if (fp != NULL) {
			fdrop(fp, td);
			fp = NULL;
		}
		KQ_GLOBAL_UNLOCK(&kq_global, haskqglobal);
		kq->kq_state |= KQ_FLUXWAIT;
		msleep(kq, &kq->kq_lock, PSOCK | PDROP, "kqflxwt", 0);
		goto findkn;
	}

	if (kn == NULL && ((kev->flags & EV_ADD) == 0)) {
		KQ_UNLOCK(kq);
		error = ENOENT;
		goto done;
	}

	/*
	 * kn now contains the matching knote, or NULL if no match
	 */
	if (kev->flags & EV_ADD) {
		if (kn == NULL) {
			kn = tkn;
			tkn = NULL;
			if (kn == NULL) {
				KQ_UNLOCK(kq);
				error = ENOMEM;
				goto done;
			}
			kn->kn_fp = fp;
			kn->kn_kq = kq;
			kn->kn_fop = fops;
			/*
			 * apply reference counts to knote structure, and
			 * do not release it at the end of this routine.
			 */
			fops = NULL;
			fp = NULL;

			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kev->fflags = 0;
			kev->data = 0;
			kn->kn_kevent = *kev;
			kn->kn_kevent.flags &= ~(EV_ADD | EV_DELETE |
			    EV_ENABLE | EV_DISABLE);
			kn->kn_status = KN_INFLUX|KN_DETACHED;

			error = knote_attach(kn, kq);
			KQ_UNLOCK(kq);
			if (error != 0) {
				tkn = kn;
				goto done;
			}

			if ((error = kn->kn_fop->f_attach(kn)) != 0) {
				knote_drop(kn, td);
				goto done;
			}
			KN_LIST_LOCK(kn);
		} else {
			/*
			 * The user may change some filter values after the
			 * initial EV_ADD, but doing so will not reset any
			 * filter which has already been triggered.
			 */
			kn->kn_status |= KN_INFLUX;
			KQ_UNLOCK(kq);
			KN_LIST_LOCK(kn);
			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kn->kn_kevent.udata = kev->udata;
		}

		/*
		 * We can get here with kn->kn_knlist == NULL.
		 * This can happen when the initial attach event decides that
		 * the event is "completed" already.  i.e. filt_procattach
		 * is called on a zombie process.  It will call filt_proc
		 * which will remove it from the list, and NULL kn_knlist.
		 */
		event = kn->kn_fop->f_event(kn, 0);
		KQ_LOCK(kq);
		if (event)
			KNOTE_ACTIVATE(kn, 1);
		kn->kn_status &= ~KN_INFLUX;
		KN_LIST_UNLOCK(kn);
	} else if (kev->flags & EV_DELETE) {
		kn->kn_status |= KN_INFLUX;
		KQ_UNLOCK(kq);
		if (!(kn->kn_status & KN_DETACHED))
			kn->kn_fop->f_detach(kn);
		knote_drop(kn, td);
		goto done;
	}

	if ((kev->flags & EV_DISABLE) &&
	    ((kn->kn_status & KN_DISABLED) == 0)) {
		kn->kn_status |= KN_DISABLED;
	}

	if ((kev->flags & EV_ENABLE) && (kn->kn_status & KN_DISABLED)) {
		kn->kn_status &= ~KN_DISABLED;
		if ((kn->kn_status & KN_ACTIVE) &&
		    ((kn->kn_status & KN_QUEUED) == 0))
			knote_enqueue(kn);
	}
	KQ_UNLOCK_FLUX(kq);

done:
	KQ_GLOBAL_UNLOCK(&kq_global, haskqglobal);
	if (fp != NULL)
		fdrop(fp, td);
	if (tkn != NULL)
		knote_free(tkn);
	if (fops != NULL)
		kqueue_fo_release(filt);
	return (error);
}

static int
kqueue_acquire(struct file *fp, struct kqueue **kqp)
{
	int error;
	struct kqueue *kq;

	error = 0;

	kq = fp->f_data;
	if (fp->f_type != DTYPE_KQUEUE || kq == NULL)
		return (EBADF);
	*kqp = kq;
	KQ_LOCK(kq);
	if ((kq->kq_state & KQ_CLOSING) == KQ_CLOSING) {
		KQ_UNLOCK(kq);
		return (EBADF);
	}
	kq->kq_refcnt++;
	KQ_UNLOCK(kq);

	return error;
}

static void
kqueue_release(struct kqueue *kq, int locked)
{
	if (locked)
		KQ_OWNED(kq);
	else
		KQ_LOCK(kq);
	kq->kq_refcnt--;
	if (kq->kq_refcnt == 1)
		wakeup(&kq->kq_refcnt);
	if (!locked)
		KQ_UNLOCK(kq);
}

static void
kqueue_schedtask(struct kqueue *kq)
{

	KQ_OWNED(kq);
	KASSERT(((kq->kq_state & KQ_TASKDRAIN) != KQ_TASKDRAIN),
	    ("scheduling kqueue task while draining"));

	if ((kq->kq_state & KQ_TASKSCHED) != KQ_TASKSCHED) {
		taskqueue_enqueue(taskqueue_kqueue, &kq->kq_task);
		kq->kq_state |= KQ_TASKSCHED;
	}
}

/*
 * Expand the kq to make sure we have storage for fops/ident pair.
 *
 * Return 0 on success (or no work necessary), return errno on failure.
 *
 * Not calling hashinit w/ waitok (proper malloc flag) should be safe.
 * If kqueue_register is called from a non-fd context, there usually/should
 * be no locks held.
 */
static int
kqueue_expand(struct kqueue *kq, struct filterops *fops, uintptr_t ident,
	int waitok)
{
	struct klist *list, *tmp_knhash;
	u_long tmp_knhashmask;
	int size;
	int fd;
	int mflag = waitok ? M_WAITOK : M_NOWAIT;

	KQ_NOTOWNED(kq);

	if (fops->f_isfd) {
		fd = ident;
		if (kq->kq_knlistsize <= fd) {
			size = kq->kq_knlistsize;
			while (size <= fd)
				size += KQEXTENT;
			MALLOC(list, struct klist *,
			    size * sizeof list, M_KQUEUE, mflag);
			if (list == NULL)
				return ENOMEM;
			KQ_LOCK(kq);
			if (kq->kq_knlistsize > fd) {
				FREE(list, M_KQUEUE);
				list = NULL;
			} else {
				if (kq->kq_knlist != NULL) {
					bcopy(kq->kq_knlist, list,
					    kq->kq_knlistsize * sizeof list);
					FREE(kq->kq_knlist, M_KQUEUE);
					kq->kq_knlist = NULL;
				}
				bzero((caddr_t)list +
				    kq->kq_knlistsize * sizeof list,
				    (size - kq->kq_knlistsize) * sizeof list);
				kq->kq_knlistsize = size;
				kq->kq_knlist = list;
			}
			KQ_UNLOCK(kq);
		}
	} else {
		if (kq->kq_knhashmask == 0) {
			tmp_knhash = hashinit(KN_HASHSIZE, M_KQUEUE,
			    &tmp_knhashmask);
			if (tmp_knhash == NULL)
				return ENOMEM;
			KQ_LOCK(kq);
			if (kq->kq_knhashmask == 0) {
				kq->kq_knhash = tmp_knhash;
				kq->kq_knhashmask = tmp_knhashmask;
			} else {
				free(tmp_knhash, M_KQUEUE);
			}
			KQ_UNLOCK(kq);
		}
	}

	KQ_NOTOWNED(kq);
	return 0;
}

static void
kqueue_task(void *arg, int pending)
{
	struct kqueue *kq;
	int haskqglobal;

	haskqglobal = 0;
	kq = arg;

	KQ_GLOBAL_LOCK(&kq_global, haskqglobal);
	KQ_LOCK(kq);

	KNOTE_LOCKED(&kq->kq_sel.si_note, 0);

	kq->kq_state &= ~KQ_TASKSCHED;
	if ((kq->kq_state & KQ_TASKDRAIN) == KQ_TASKDRAIN) {
		wakeup(&kq->kq_state);
	}
	KQ_UNLOCK(kq);
	KQ_GLOBAL_UNLOCK(&kq_global, haskqglobal);
}

/*
 * Scan, update kn_data (if not ONESHOT), and copyout triggered events.
 * We treat KN_MARKER knotes as if they are INFLUX.
 */
static int
kqueue_scan(struct kqueue *kq, int maxevents, struct kevent_copyops *k_ops,
    const struct timespec *tsp, struct kevent *keva, struct thread *td)
{
	struct kevent *kevp;
	struct timeval atv, rtv, ttv;
	struct knote *kn, *marker;
	int count, timeout, nkev, error;
	int haskqglobal;

	count = maxevents;
	nkev = 0;
	error = 0;
	haskqglobal = 0;

	if (maxevents == 0)
		goto done_nl;

	if (tsp != NULL) {
		TIMESPEC_TO_TIMEVAL(&atv, tsp);
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done_nl;
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
	marker = knote_alloc(1);
	if (marker == NULL) {
		error = ENOMEM;
		goto done_nl;
	}
	marker->kn_status = KN_MARKER;
	KQ_LOCK(kq);
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
	kevp = keva;
	if (kq->kq_count == 0) {
		if (timeout < 0) {
			error = EWOULDBLOCK;
		} else {
			kq->kq_state |= KQ_SLEEP;
			error = msleep(kq, &kq->kq_lock, PSOCK | PCATCH,
			    "kqread", timeout);
		}
		if (error == 0)
			goto retry;
		/* don't restart after signals... */
		if (error == ERESTART)
			error = EINTR;
		else if (error == EWOULDBLOCK)
			error = 0;
		goto done;
	}

	TAILQ_INSERT_TAIL(&kq->kq_head, marker, kn_tqe);
	while (count) {
		KQ_OWNED(kq);
		kn = TAILQ_FIRST(&kq->kq_head);

		if ((kn->kn_status == KN_MARKER && kn != marker) ||
		    (kn->kn_status & KN_INFLUX) == KN_INFLUX) {
			kq->kq_state |= KQ_FLUXWAIT;
			error = msleep(kq, &kq->kq_lock, PSOCK,
			    "kqflxwt", 0);
			continue;
		}

		TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
		if ((kn->kn_status & KN_DISABLED) == KN_DISABLED) {
			kn->kn_status &= ~KN_QUEUED;
			kq->kq_count--;
			continue;
		}
		if (kn == marker) {
			KQ_FLUX_WAKEUP(kq);
			if (count == maxevents)
				goto retry;
			goto done;
		}
		KASSERT((kn->kn_status & KN_INFLUX) == 0,
		    ("KN_INFLUX set when not suppose to be"));

		if ((kn->kn_flags & EV_ONESHOT) == EV_ONESHOT) {
			kn->kn_status &= ~KN_QUEUED;
			kn->kn_status |= KN_INFLUX;
			kq->kq_count--;
			KQ_UNLOCK(kq);
			/*
			 * We don't need to lock the list since we've marked
			 * it _INFLUX.
			 */
			*kevp = kn->kn_kevent;
			if (!(kn->kn_status & KN_DETACHED))
				kn->kn_fop->f_detach(kn);
			knote_drop(kn, td);
			KQ_LOCK(kq);
			kn = NULL;
		} else {
			kn->kn_status |= KN_INFLUX;
			KQ_UNLOCK(kq);
			if ((kn->kn_status & KN_KQUEUE) == KN_KQUEUE)
				KQ_GLOBAL_LOCK(&kq_global, haskqglobal);
			KN_LIST_LOCK(kn);
			if (kn->kn_fop->f_event(kn, 0) == 0) {
				KQ_LOCK(kq);
				KQ_GLOBAL_UNLOCK(&kq_global, haskqglobal);
				kn->kn_status &=
				    ~(KN_QUEUED | KN_ACTIVE | KN_INFLUX);
				kq->kq_count--;
				KN_LIST_UNLOCK(kn);
				continue;
			}
			*kevp = kn->kn_kevent;
			KQ_LOCK(kq);
			KQ_GLOBAL_UNLOCK(&kq_global, haskqglobal);
			if (kn->kn_flags & EV_CLEAR) {
				kn->kn_data = 0;
				kn->kn_fflags = 0;
				kn->kn_status &= ~(KN_QUEUED | KN_ACTIVE);
				kq->kq_count--;
			} else
				TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
			
			kn->kn_status &= ~(KN_INFLUX);
			KN_LIST_UNLOCK(kn);
		}

		/* we are returning a copy to the user */
		kevp++;
		nkev++;
		count--;

		if (nkev == KQ_NEVENTS) {
			KQ_UNLOCK_FLUX(kq);
			error = k_ops->k_copyout(k_ops->arg, keva, nkev);
			nkev = 0;
			kevp = keva;
			KQ_LOCK(kq);
			if (error)
				break;
		}
	}
	TAILQ_REMOVE(&kq->kq_head, marker, kn_tqe);
done:
	KQ_OWNED(kq);
	KQ_UNLOCK_FLUX(kq);
	knote_free(marker);
done_nl:
	KQ_NOTOWNED(kq);
	if (nkev != 0)
		error = k_ops->k_copyout(k_ops->arg, keva, nkev);
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
kqueue_ioctl(struct file *fp, u_long cmd, void *data,
	struct ucred *active_cred, struct thread *td)
{
	/*
	 * Enabling sigio causes two major problems:
	 * 1) infinite recursion:
	 * Synopsys: kevent is being used to track signals and have FIOASYNC
	 * set.  On receipt of a signal this will cause a kqueue to recurse
	 * into itself over and over.  Sending the sigio causes the kqueue
	 * to become ready, which in turn posts sigio again, forever.
	 * Solution: this can be solved by setting a flag in the kqueue that
	 * we have a SIGIO in progress.
	 * 2) locking problems:
	 * Synopsys: Kqueue is a leaf subsystem, but adding signalling puts
	 * us above the proc and pgrp locks.
	 * Solution: Post a signal using an async mechanism, being sure to
	 * record a generation count in the delivery so that we do not deliver
	 * a signal to the wrong process.
	 *
	 * Note, these two mechanisms are somewhat mutually exclusive!
	 */
#if 0
	struct kqueue *kq;

	kq = fp->f_data;
	switch (cmd) {
	case FIOASYNC:
		if (*(int *)data) {
			kq->kq_state |= KQ_ASYNC;
		} else {
			kq->kq_state &= ~KQ_ASYNC;
		}
		return (0);

	case FIOSETOWN:
		return (fsetown(*(int *)data, &kq->kq_sigio));

	case FIOGETOWN:
		*(int *)data = fgetown(&kq->kq_sigio);
		return (0);
	}
#endif

	return (ENOTTY);
}

/*ARGSUSED*/
static int
kqueue_poll(struct file *fp, int events, struct ucred *active_cred,
	struct thread *td)
{
	struct kqueue *kq;
	int revents = 0;
	int error;

	if ((error = kqueue_acquire(fp, &kq)))
		return POLLERR;

	KQ_LOCK(kq);
	if (events & (POLLIN | POLLRDNORM)) {
		if (kq->kq_count) {
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			selrecord(td, &kq->kq_sel);
			if (SEL_WAITING(&kq->kq_sel))
				kq->kq_state |= KQ_SEL;
		}
	}
	kqueue_release(kq, 1);
	KQ_UNLOCK(kq);
	return (revents);
}

/*ARGSUSED*/
static int
kqueue_stat(struct file *fp, struct stat *st, struct ucred *active_cred,
	struct thread *td)
{

	bzero((void *)st, sizeof *st);
	/*
	 * We no longer return kq_count because the unlocked value is useless.
	 * If you spent all this time getting the count, why not spend your
	 * syscall better by calling kevent?
	 *
	 * XXX - This is needed for libc_r.
	 */
	st->st_mode = S_IFIFO;
	return (0);
}

/*ARGSUSED*/
static int
kqueue_close(struct file *fp, struct thread *td)
{
	struct kqueue *kq = fp->f_data;
	struct filedesc *fdp;
	struct knote *kn;
	int i;
	int error;

	if ((error = kqueue_acquire(fp, &kq)))
		return error;

	KQ_LOCK(kq);

	KASSERT((kq->kq_state & KQ_CLOSING) != KQ_CLOSING,
	    ("kqueue already closing"));
	kq->kq_state |= KQ_CLOSING;
	if (kq->kq_refcnt > 1)
		msleep(&kq->kq_refcnt, &kq->kq_lock, PSOCK, "kqclose", 0);

	KASSERT(kq->kq_refcnt == 1, ("other refs are out there!"));
	fdp = kq->kq_fdp;

	KASSERT(knlist_empty(&kq->kq_sel.si_note),
	    ("kqueue's knlist not empty"));

	for (i = 0; i < kq->kq_knlistsize; i++) {
		while ((kn = SLIST_FIRST(&kq->kq_knlist[i])) != NULL) {
			KASSERT((kn->kn_status & KN_INFLUX) == 0,
			    ("KN_INFLUX set when not suppose to be"));
			kn->kn_status |= KN_INFLUX;
			KQ_UNLOCK(kq);
			if (!(kn->kn_status & KN_DETACHED))
				kn->kn_fop->f_detach(kn);
			knote_drop(kn, td);
			KQ_LOCK(kq);
		}
	}
	if (kq->kq_knhashmask != 0) {
		for (i = 0; i <= kq->kq_knhashmask; i++) {
			while ((kn = SLIST_FIRST(&kq->kq_knhash[i])) != NULL) {
				KASSERT((kn->kn_status & KN_INFLUX) == 0,
				    ("KN_INFLUX set when not suppose to be"));
				kn->kn_status |= KN_INFLUX;
				KQ_UNLOCK(kq);
				if (!(kn->kn_status & KN_DETACHED))
					kn->kn_fop->f_detach(kn);
				knote_drop(kn, td);
				KQ_LOCK(kq);
			}
		}
	}

	if ((kq->kq_state & KQ_TASKSCHED) == KQ_TASKSCHED) {
		kq->kq_state |= KQ_TASKDRAIN;
		msleep(&kq->kq_state, &kq->kq_lock, PSOCK, "kqtqdr", 0);
	}

	if ((kq->kq_state & KQ_SEL) == KQ_SEL) {
		selwakeuppri(&kq->kq_sel, PSOCK);
		if (!SEL_WAITING(&kq->kq_sel))
			kq->kq_state &= ~KQ_SEL;
	}

	KQ_UNLOCK(kq);

	FILEDESC_XLOCK(fdp);
	SLIST_REMOVE(&fdp->fd_kqlist, kq, kqueue, kq_list);
	FILEDESC_XUNLOCK(fdp);

	knlist_destroy(&kq->kq_sel.si_note);
	mtx_destroy(&kq->kq_lock);
	kq->kq_fdp = NULL;

	if (kq->kq_knhash != NULL)
		free(kq->kq_knhash, M_KQUEUE);
	if (kq->kq_knlist != NULL)
		free(kq->kq_knlist, M_KQUEUE);

	funsetown(&kq->kq_sigio);
	free(kq, M_KQUEUE);
	fp->f_data = NULL;

	return (0);
}

static void
kqueue_wakeup(struct kqueue *kq)
{
	KQ_OWNED(kq);

	if ((kq->kq_state & KQ_SLEEP) == KQ_SLEEP) {
		kq->kq_state &= ~KQ_SLEEP;
		wakeup(kq);
	}
	if ((kq->kq_state & KQ_SEL) == KQ_SEL) {
		selwakeuppri(&kq->kq_sel, PSOCK);
		if (!SEL_WAITING(&kq->kq_sel))
			kq->kq_state &= ~KQ_SEL;
	}
	if (!knlist_empty(&kq->kq_sel.si_note))
		kqueue_schedtask(kq);
	if ((kq->kq_state & KQ_ASYNC) == KQ_ASYNC) {
		pgsigio(&kq->kq_sigio, SIGIO, 0);
	}
}

/*
 * Walk down a list of knotes, activating them if their event has triggered.
 *
 * There is a possibility to optimize in the case of one kq watching another.
 * Instead of scheduling a task to wake it up, you could pass enough state
 * down the chain to make up the parent kqueue.  Make this code functional
 * first.
 */
void
knote(struct knlist *list, long hint, int islocked)
{
	struct kqueue *kq;
	struct knote *kn;

	if (list == NULL)
		return;

	KNL_ASSERT_LOCK(list, islocked);

	if (!islocked) 
		list->kl_lock(list->kl_lockarg); 

	/*
	 * If we unlock the list lock (and set KN_INFLUX), we can eliminate
	 * the kqueue scheduling, but this will introduce four
	 * lock/unlock's for each knote to test.  If we do, continue to use
	 * SLIST_FOREACH, SLIST_FOREACH_SAFE is not safe in our case, it is
	 * only safe if you want to remove the current item, which we are
	 * not doing.
	 */
	SLIST_FOREACH(kn, &list->kl_list, kn_selnext) {
		kq = kn->kn_kq;
		if ((kn->kn_status & KN_INFLUX) != KN_INFLUX) {
			KQ_LOCK(kq);
			if ((kn->kn_status & KN_INFLUX) != KN_INFLUX) {
				kn->kn_status |= KN_HASKQLOCK;
				if (kn->kn_fop->f_event(kn, hint))
					KNOTE_ACTIVATE(kn, 1);
				kn->kn_status &= ~KN_HASKQLOCK;
			}
			KQ_UNLOCK(kq);
		}
		kq = NULL;
	}
	if (!islocked)
		list->kl_unlock(list->kl_lockarg); 
}

/*
 * add a knote to a knlist
 */
void
knlist_add(struct knlist *knl, struct knote *kn, int islocked)
{
	KNL_ASSERT_LOCK(knl, islocked);
	KQ_NOTOWNED(kn->kn_kq);
	KASSERT((kn->kn_status & (KN_INFLUX|KN_DETACHED)) ==
	    (KN_INFLUX|KN_DETACHED), ("knote not KN_INFLUX and KN_DETACHED"));
	if (!islocked)
		knl->kl_lock(knl->kl_lockarg);
	SLIST_INSERT_HEAD(&knl->kl_list, kn, kn_selnext);
	if (!islocked)
		knl->kl_unlock(knl->kl_lockarg);
	KQ_LOCK(kn->kn_kq);
	kn->kn_knlist = knl;
	kn->kn_status &= ~KN_DETACHED;
	KQ_UNLOCK(kn->kn_kq);
}

static void
knlist_remove_kq(struct knlist *knl, struct knote *kn, int knlislocked, int kqislocked)
{
	KASSERT(!(!!kqislocked && !knlislocked), ("kq locked w/o knl locked"));
	KNL_ASSERT_LOCK(knl, knlislocked);
	mtx_assert(&kn->kn_kq->kq_lock, kqislocked ? MA_OWNED : MA_NOTOWNED);
	if (!kqislocked)
		KASSERT((kn->kn_status & (KN_INFLUX|KN_DETACHED)) == KN_INFLUX,
    ("knlist_remove called w/o knote being KN_INFLUX or already removed"));
	if (!knlislocked)
		knl->kl_lock(knl->kl_lockarg);
	SLIST_REMOVE(&knl->kl_list, kn, knote, kn_selnext);
	kn->kn_knlist = NULL;
	if (!knlislocked)
		knl->kl_unlock(knl->kl_lockarg);
	if (!kqislocked)
		KQ_LOCK(kn->kn_kq);
	kn->kn_status |= KN_DETACHED;
	if (!kqislocked)
		KQ_UNLOCK(kn->kn_kq);
}

/*
 * remove all knotes from a specified klist
 */
void
knlist_remove(struct knlist *knl, struct knote *kn, int islocked)
{

	knlist_remove_kq(knl, kn, islocked, 0);
}

/*
 * remove knote from a specified klist while in f_event handler.
 */
void
knlist_remove_inevent(struct knlist *knl, struct knote *kn)
{

	knlist_remove_kq(knl, kn, 1,
	    (kn->kn_status & KN_HASKQLOCK) == KN_HASKQLOCK);
}

int
knlist_empty(struct knlist *knl)
{
	KNL_ASSERT_LOCKED(knl);
	return SLIST_EMPTY(&knl->kl_list);
}

static struct mtx	knlist_lock;
MTX_SYSINIT(knlist_lock, &knlist_lock, "knlist lock for lockless objects",
	MTX_DEF);
static void knlist_mtx_lock(void *arg);
static void knlist_mtx_unlock(void *arg);
static int knlist_mtx_locked(void *arg);

static void
knlist_mtx_lock(void *arg)
{
	mtx_lock((struct mtx *)arg);
}

static void
knlist_mtx_unlock(void *arg)
{
	mtx_unlock((struct mtx *)arg);
}

static int
knlist_mtx_locked(void *arg)
{
	return (mtx_owned((struct mtx *)arg));
}

void
knlist_init(struct knlist *knl, void *lock, void (*kl_lock)(void *),
    void (*kl_unlock)(void *), int (*kl_locked)(void *))
{

	if (lock == NULL)
		knl->kl_lockarg = &knlist_lock;
	else
		knl->kl_lockarg = lock;

	if (kl_lock == NULL)
		knl->kl_lock = knlist_mtx_lock;
	else
		knl->kl_lock = kl_lock;
	if (kl_unlock == NULL)
		knl->kl_unlock = knlist_mtx_unlock;
	else
		knl->kl_unlock = kl_unlock;
	if (kl_locked == NULL)
		knl->kl_locked = knlist_mtx_locked;
	else
		knl->kl_locked = kl_locked;

	SLIST_INIT(&knl->kl_list);
}

void
knlist_destroy(struct knlist *knl)
{

#ifdef INVARIANTS
	/*
	 * if we run across this error, we need to find the offending
	 * driver and have it call knlist_clear.
	 */
	if (!SLIST_EMPTY(&knl->kl_list))
		printf("WARNING: destroying knlist w/ knotes on it!\n");
#endif

	knl->kl_lockarg = knl->kl_lock = knl->kl_unlock = NULL;
	SLIST_INIT(&knl->kl_list);
}

/*
 * Even if we are locked, we may need to drop the lock to allow any influx
 * knotes time to "settle".
 */
void
knlist_cleardel(struct knlist *knl, struct thread *td, int islocked, int killkn)
{
	struct knote *kn, *kn2;
	struct kqueue *kq;

	if (islocked)
		KNL_ASSERT_LOCKED(knl);
	else {
		KNL_ASSERT_UNLOCKED(knl);
again:		/* need to reacquire lock since we have dropped it */
		knl->kl_lock(knl->kl_lockarg);
	}

	SLIST_FOREACH_SAFE(kn, &knl->kl_list, kn_selnext, kn2) {
		kq = kn->kn_kq;
		KQ_LOCK(kq);
		if ((kn->kn_status & KN_INFLUX)) {
			KQ_UNLOCK(kq);
			continue;
		}
		knlist_remove_kq(knl, kn, 1, 1);
		if (killkn) {
			kn->kn_status |= KN_INFLUX | KN_DETACHED;
			KQ_UNLOCK(kq);
			knote_drop(kn, td);
		} else {
			/* Make sure cleared knotes disappear soon */
			kn->kn_flags |= (EV_EOF | EV_ONESHOT);
			KQ_UNLOCK(kq);
		}
		kq = NULL;
	}

	if (!SLIST_EMPTY(&knl->kl_list)) {
		/* there are still KN_INFLUX remaining */
		kn = SLIST_FIRST(&knl->kl_list);
		kq = kn->kn_kq;
		KQ_LOCK(kq);
		KASSERT(kn->kn_status & KN_INFLUX,
		    ("knote removed w/o list lock"));
		knl->kl_unlock(knl->kl_lockarg);
		kq->kq_state |= KQ_FLUXWAIT;
		msleep(kq, &kq->kq_lock, PSOCK | PDROP, "kqkclr", 0);
		kq = NULL;
		goto again;
	}

	if (islocked)
		KNL_ASSERT_LOCKED(knl);
	else {
		knl->kl_unlock(knl->kl_lockarg);
		KNL_ASSERT_UNLOCKED(knl);
	}
}

/*
 * Remove all knotes referencing a specified fd must be called with FILEDESC
 * lock.  This prevents a race where a new fd comes along and occupies the
 * entry and we attach a knote to the fd.
 */
void
knote_fdclose(struct thread *td, int fd)
{
	struct filedesc *fdp = td->td_proc->p_fd;
	struct kqueue *kq;
	struct knote *kn;
	int influx;

	FILEDESC_XLOCK_ASSERT(fdp);

	/*
	 * We shouldn't have to worry about new kevents appearing on fd
	 * since filedesc is locked.
	 */
	SLIST_FOREACH(kq, &fdp->fd_kqlist, kq_list) {
		KQ_LOCK(kq);

again:
		influx = 0;
		while (kq->kq_knlistsize > fd &&
		    (kn = SLIST_FIRST(&kq->kq_knlist[fd])) != NULL) {
			if (kn->kn_status & KN_INFLUX) {
				/* someone else might be waiting on our knote */
				if (influx)
					wakeup(kq);
				kq->kq_state |= KQ_FLUXWAIT;
				msleep(kq, &kq->kq_lock, PSOCK, "kqflxwt", 0);
				goto again;
			}
			kn->kn_status |= KN_INFLUX;
			KQ_UNLOCK(kq);
			if (!(kn->kn_status & KN_DETACHED))
				kn->kn_fop->f_detach(kn);
			knote_drop(kn, td);
			influx = 1;
			KQ_LOCK(kq);
		}
		KQ_UNLOCK_FLUX(kq);
	}
}

static int
knote_attach(struct knote *kn, struct kqueue *kq)
{
	struct klist *list;

	KASSERT(kn->kn_status & KN_INFLUX, ("knote not marked INFLUX"));
	KQ_OWNED(kq);

	if (kn->kn_fop->f_isfd) {
		if (kn->kn_id >= kq->kq_knlistsize)
			return ENOMEM;
		list = &kq->kq_knlist[kn->kn_id];
	} else {
		if (kq->kq_knhash == NULL)
			return ENOMEM;
		list = &kq->kq_knhash[KN_HASH(kn->kn_id, kq->kq_knhashmask)];
	}

	SLIST_INSERT_HEAD(list, kn, kn_link);

	return 0;
}

/*
 * knote must already have been detached using the f_detach method.
 * no lock need to be held, it is assumed that the KN_INFLUX flag is set
 * to prevent other removal.
 */
static void
knote_drop(struct knote *kn, struct thread *td)
{
	struct kqueue *kq;
	struct klist *list;

	kq = kn->kn_kq;

	KQ_NOTOWNED(kq);
	KASSERT((kn->kn_status & KN_INFLUX) == KN_INFLUX,
	    ("knote_drop called without KN_INFLUX set in kn_status"));

	KQ_LOCK(kq);
	if (kn->kn_fop->f_isfd)
		list = &kq->kq_knlist[kn->kn_id];
	else
		list = &kq->kq_knhash[KN_HASH(kn->kn_id, kq->kq_knhashmask)];

	if (!SLIST_EMPTY(list))
		SLIST_REMOVE(list, kn, knote, kn_link);
	if (kn->kn_status & KN_QUEUED)
		knote_dequeue(kn);
	KQ_UNLOCK_FLUX(kq);

	if (kn->kn_fop->f_isfd) {
		fdrop(kn->kn_fp, td);
		kn->kn_fp = NULL;
	}
	kqueue_fo_release(kn->kn_kevent.filter);
	kn->kn_fop = NULL;
	knote_free(kn);
}

static void
knote_enqueue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;

	KQ_OWNED(kn->kn_kq);
	KASSERT((kn->kn_status & KN_QUEUED) == 0, ("knote already queued"));

	TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
	kn->kn_status |= KN_QUEUED;
	kq->kq_count++;
	kqueue_wakeup(kq);
}

static void
knote_dequeue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;

	KQ_OWNED(kn->kn_kq);
	KASSERT(kn->kn_status & KN_QUEUED, ("knote not queued"));

	TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
	kn->kn_status &= ~KN_QUEUED;
	kq->kq_count--;
}

static void
knote_init(void)
{

	knote_zone = uma_zcreate("KNOTE", sizeof(struct knote), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);
}
SYSINIT(knote, SI_SUB_PSEUDO, SI_ORDER_ANY, knote_init, NULL)

static struct knote *
knote_alloc(int waitok)
{
	return ((struct knote *)uma_zalloc(knote_zone,
	    (waitok ? M_WAITOK : M_NOWAIT)|M_ZERO));
}

static void
knote_free(struct knote *kn)
{
	if (kn != NULL)
		uma_zfree(knote_zone, kn);
}

/*
 * Register the kev w/ the kq specified by fd.
 */
int 
kqfd_register(int fd, struct kevent *kev, struct thread *td, int waitok)
{
	struct kqueue *kq;
	struct file *fp;
	int error;

	if ((error = fget(td, fd, &fp)) != 0)
		return (error);
	if ((error = kqueue_acquire(fp, &kq)) != 0)
		goto noacquire;

	error = kqueue_register(kq, kev, td, waitok);

	kqueue_release(kq, 0);

noacquire:
	fdrop(fp, td);

	return error;
}
