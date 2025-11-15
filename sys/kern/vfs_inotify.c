/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Klara, Inc.
 */

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/caprights.h>
#include <sys/counter.h>
#include <sys/dirent.h>
#define	EXTERR_CATEGORY	EXTERR_CAT_INOTIFY
#include <sys/exterrvar.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/inotify.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/ktrace.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/resourcevar.h>
#include <sys/selinfo.h>
#include <sys/stat.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/syslimits.h>
#include <sys/sysproto.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <sys/user.h>
#include <sys/vnode.h>

uint32_t inotify_rename_cookie;

static SYSCTL_NODE(_vfs, OID_AUTO, inotify, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "inotify configuration");

static int inotify_max_queued_events = 16384;
SYSCTL_INT(_vfs_inotify, OID_AUTO, max_queued_events, CTLFLAG_RWTUN,
    &inotify_max_queued_events, 0,
    "Maximum number of events to queue on an inotify descriptor");

static int inotify_max_user_instances = 256;
SYSCTL_INT(_vfs_inotify, OID_AUTO, max_user_instances, CTLFLAG_RWTUN,
    &inotify_max_user_instances, 0,
    "Maximum number of inotify descriptors per user");

static int inotify_max_user_watches;
SYSCTL_INT(_vfs_inotify, OID_AUTO, max_user_watches, CTLFLAG_RWTUN,
    &inotify_max_user_watches, 0,
    "Maximum number of inotify watches per user");

static int inotify_max_watches;
SYSCTL_INT(_vfs_inotify, OID_AUTO, max_watches, CTLFLAG_RWTUN,
    &inotify_max_watches, 0,
    "Maximum number of inotify watches system-wide");

static int inotify_watches;
SYSCTL_INT(_vfs_inotify, OID_AUTO, watches, CTLFLAG_RD,
    &inotify_watches, 0,
    "Total number of inotify watches currently in use");

static int inotify_coalesce = 1;
SYSCTL_INT(_vfs_inotify, OID_AUTO, coalesce, CTLFLAG_RWTUN,
    &inotify_coalesce, 0,
    "Coalesce inotify events when possible");

static COUNTER_U64_DEFINE_EARLY(inotify_event_drops);
SYSCTL_COUNTER_U64(_vfs_inotify, OID_AUTO, event_drops, CTLFLAG_RD,
    &inotify_event_drops,
    "Number of inotify events dropped due to limits or allocation failures");

static fo_rdwr_t	inotify_read;
static fo_ioctl_t	inotify_ioctl;
static fo_poll_t	inotify_poll;
static fo_kqfilter_t	inotify_kqfilter;
static fo_stat_t	inotify_stat;
static fo_close_t	inotify_close;
static fo_fill_kinfo_t	inotify_fill_kinfo;

static const struct fileops inotifyfdops = {
	.fo_read = inotify_read,
	.fo_write = invfo_rdwr,
	.fo_truncate = invfo_truncate,
	.fo_ioctl = inotify_ioctl,
	.fo_poll = inotify_poll,
	.fo_kqfilter = inotify_kqfilter,
	.fo_stat = inotify_stat,
	.fo_close = inotify_close,
	.fo_chmod = invfo_chmod,
	.fo_chown = invfo_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = inotify_fill_kinfo,
	.fo_cmp = file_kcmp_generic,
	.fo_flags = DFLAG_PASSABLE,
};

static void	filt_inotifydetach(struct knote *kn);
static int	filt_inotifyevent(struct knote *kn, long hint);

static const struct filterops inotify_rfiltops = {
	.f_isfd = 1,
	.f_detach = filt_inotifydetach,
	.f_event = filt_inotifyevent,
};

static MALLOC_DEFINE(M_INOTIFY, "inotify", "inotify data structures");

struct inotify_record {
	STAILQ_ENTRY(inotify_record) link;
	struct inotify_event	ev;
};

static uint64_t inotify_ino = 1;

/*
 * On LP64 systems this occupies 64 bytes, so we don't get internal
 * fragmentation by allocating watches with malloc(9).  If the size changes,
 * consider using a UMA zone to improve memory efficiency.
 */
struct inotify_watch {
	struct inotify_softc *sc; /* back-pointer */
	int		wd;	/* unique ID */
	uint32_t	mask;	/* event mask */
	struct vnode	*vp;	/* vnode being watched, refed */
	RB_ENTRY(inotify_watch) ilink;		/* inotify linkage */
	TAILQ_ENTRY(inotify_watch) vlink;	/* vnode linkage */
};

static void
inotify_init(void *arg __unused)
{
	/* Don't let a user hold too many vnodes. */
	inotify_max_user_watches = desiredvnodes / 3;
	/* Don't let the system hold too many vnodes. */
	inotify_max_watches = desiredvnodes / 2;
}
SYSINIT(inotify, SI_SUB_VFS, SI_ORDER_ANY, inotify_init, NULL);

static int
inotify_watch_cmp(const struct inotify_watch *a,
    const struct inotify_watch *b)
{
	if (a->wd < b->wd)
		return (-1);
	else if (a->wd > b->wd)
		return (1);
	else
		return (0);
}
RB_HEAD(inotify_watch_tree, inotify_watch);
RB_GENERATE_STATIC(inotify_watch_tree, inotify_watch, ilink, inotify_watch_cmp);

struct inotify_softc {
	struct mtx	lock;			/* serialize all softc writes */
	STAILQ_HEAD(, inotify_record) pending;	/* events waiting to be read */
	struct inotify_record overflow;		/* preallocated record */
	int		nextwatch;		/* next watch ID to try */
	int		npending;		/* number of pending events */
	size_t		nbpending;		/* bytes available to read */
	uint64_t	ino;			/* unique identifier */
	struct inotify_watch_tree watches;	/* active watches */
	TAILQ_HEAD(, inotify_watch) deadwatches; /* watches pending vrele() */
	struct task	reaptask;		/* task to reap dead watches */
	struct selinfo	sel;			/* select/poll/kevent info */
	struct ucred	*cred;			/* credential ref */
};

static struct inotify_record *
inotify_dequeue(struct inotify_softc *sc)
{
	struct inotify_record *rec;

	mtx_assert(&sc->lock, MA_OWNED);
	KASSERT(!STAILQ_EMPTY(&sc->pending),
	    ("%s: queue for %p is empty", __func__, sc));

	rec = STAILQ_FIRST(&sc->pending);
	STAILQ_REMOVE_HEAD(&sc->pending, link);
	sc->npending--;
	sc->nbpending -= sizeof(rec->ev) + rec->ev.len;
	return (rec);
}

static void
inotify_enqueue(struct inotify_softc *sc, struct inotify_record *rec, bool head)
{
	mtx_assert(&sc->lock, MA_OWNED);

	if (head)
		STAILQ_INSERT_HEAD(&sc->pending, rec, link);
	else
		STAILQ_INSERT_TAIL(&sc->pending, rec, link);
	sc->npending++;
	sc->nbpending += sizeof(rec->ev) + rec->ev.len;
}

static int
inotify_read(struct file *fp, struct uio *uio, struct ucred *cred, int flags,
    struct thread *td)
{
	struct inotify_softc *sc;
	struct inotify_record *rec;
	int error;
	bool first;

	sc = fp->f_data;
	error = 0;

	mtx_lock(&sc->lock);
	while (STAILQ_EMPTY(&sc->pending)) {
		if ((flags & IO_NDELAY) != 0 || (fp->f_flag & FNONBLOCK) != 0) {
			mtx_unlock(&sc->lock);
			return (EWOULDBLOCK);
		}
		error = msleep(&sc->pending, &sc->lock, PCATCH, "inotify", 0);
		if (error != 0) {
			mtx_unlock(&sc->lock);
			return (error);
		}
	}
	for (first = true; !STAILQ_EMPTY(&sc->pending); first = false) {
		size_t len;

		rec = inotify_dequeue(sc);
		len = sizeof(rec->ev) + rec->ev.len;
		if (uio->uio_resid < (ssize_t)len) {
			inotify_enqueue(sc, rec, true);
			if (first) {
				error = EXTERROR(EINVAL,
				    "read buffer is too small");
			}
			break;
		}
		mtx_unlock(&sc->lock);
		error = uiomove(&rec->ev, len, uio);
#ifdef KTRACE
		if (error == 0 && KTRPOINT(td, KTR_STRUCT))
			ktrstruct("inotify", &rec->ev, len);
#endif
		mtx_lock(&sc->lock);
		if (error != 0) {
			inotify_enqueue(sc, rec, true);
			mtx_unlock(&sc->lock);
			return (error);
		}
		if (rec == &sc->overflow) {
			/*
			 * Signal to inotify_queue_record() that the overflow
			 * record can be reused.
			 */
			memset(rec, 0, sizeof(*rec));
		} else {
			free(rec, M_INOTIFY);
		}
	}
	mtx_unlock(&sc->lock);
	return (error);
}

static int
inotify_ioctl(struct file *fp, u_long com, void *data, struct ucred *cred,
    struct thread *td)
{
	struct inotify_softc *sc;

	sc = fp->f_data;

	switch (com) {
	case FIONREAD:
		*(int *)data = (int)sc->nbpending;
		return (0);
	case FIONBIO:
	case FIOASYNC:
		return (0);
	default:
		return (ENOTTY);
	}

	return (0);
}

static int
inotify_poll(struct file *fp, int events, struct ucred *cred, struct thread *td)
{
	struct inotify_softc *sc;
	int revents;

	sc = fp->f_data;
	revents = 0;

	mtx_lock(&sc->lock);
	if ((events & (POLLIN | POLLRDNORM)) != 0 && sc->npending > 0)
		revents |= events & (POLLIN | POLLRDNORM);
	else
		selrecord(td, &sc->sel);
	mtx_unlock(&sc->lock);
	return (revents);
}

static void
filt_inotifydetach(struct knote *kn)
{
	struct inotify_softc *sc;

	sc = kn->kn_hook;
	knlist_remove(&sc->sel.si_note, kn, 0);
}

static int
filt_inotifyevent(struct knote *kn, long hint)
{
	struct inotify_softc *sc;

	sc = kn->kn_hook;
	mtx_assert(&sc->lock, MA_OWNED);
	kn->kn_data = sc->nbpending;
	return (kn->kn_data > 0);
}

static int
inotify_kqfilter(struct file *fp, struct knote *kn)
{
	struct inotify_softc *sc;

	if (kn->kn_filter != EVFILT_READ)
		return (EINVAL);
	sc = fp->f_data;
	kn->kn_fop = &inotify_rfiltops;
	kn->kn_hook = sc;
	knlist_add(&sc->sel.si_note, kn, 0);
	return (0);
}

static int
inotify_stat(struct file *fp, struct stat *sb, struct ucred *cred)
{
	struct inotify_softc *sc;

	sc = fp->f_data;

	memset(sb, 0, sizeof(*sb));
	sb->st_mode = S_IFREG | S_IRUSR;
	sb->st_blksize = sizeof(struct inotify_event) + _IN_NAMESIZE(NAME_MAX);
	mtx_lock(&sc->lock);
	sb->st_size = sc->nbpending;
	sb->st_blocks = sc->npending;
	sb->st_uid = sc->cred->cr_ruid;
	sb->st_gid = sc->cred->cr_rgid;
	sb->st_ino = sc->ino;
	mtx_unlock(&sc->lock);
	return (0);
}

static void
inotify_unlink_watch_locked(struct inotify_softc *sc, struct inotify_watch *watch)
{
	struct vnode *vp;

	vp = watch->vp;
	mtx_assert(&vp->v_pollinfo->vpi_lock, MA_OWNED);

	atomic_subtract_int(&inotify_watches, 1);
	(void)chginotifywatchcnt(sc->cred->cr_ruidinfo, -1, 0);

	TAILQ_REMOVE(&vp->v_pollinfo->vpi_inotify, watch, vlink);
	if (TAILQ_EMPTY(&vp->v_pollinfo->vpi_inotify))
		vn_irflag_unset(vp, VIRF_INOTIFY);
}

static void
inotify_free_watch(struct inotify_watch *watch)
{
	/*
	 * Formally, we don't need to lock the vnode here.  However, if we
	 * don't, and vrele() releases the last reference, it's possible the
	 * vnode will be recycled while a different thread holds the vnode lock.
	 * Work around this bug by acquiring the lock here.
	 */
	(void)vn_lock(watch->vp, LK_EXCLUSIVE | LK_RETRY);
	vput(watch->vp);
	free(watch, M_INOTIFY);
}

/*
 * Assumes that the watch has already been removed from its softc.
 */
static void
inotify_remove_watch(struct inotify_watch *watch)
{
	struct inotify_softc *sc;
	struct vnode *vp;

	sc = watch->sc;

	vp = watch->vp;
	mtx_lock(&vp->v_pollinfo->vpi_lock);
	inotify_unlink_watch_locked(sc, watch);
	mtx_unlock(&vp->v_pollinfo->vpi_lock);
	inotify_free_watch(watch);
}

static void
inotify_reap(void *arg, int pending)
{
	struct inotify_softc *sc;
	struct inotify_watch *watch;

	sc = arg;
	mtx_lock(&sc->lock);
	while ((watch = TAILQ_FIRST(&sc->deadwatches)) != NULL) {
		TAILQ_REMOVE(&sc->deadwatches, watch, vlink);
		mtx_unlock(&sc->lock);
		inotify_free_watch(watch);
		mtx_lock(&sc->lock);
	}
	mtx_unlock(&sc->lock);
}

static int
inotify_close(struct file *fp, struct thread *td)
{
	struct inotify_softc *sc;
	struct inotify_record *rec;
	struct inotify_watch *watch;

	sc = fp->f_data;

	/* Detach watches from their vnodes. */
	mtx_lock(&sc->lock);
	(void)chginotifycnt(sc->cred->cr_ruidinfo, -1, 0);
	while ((watch = RB_MIN(inotify_watch_tree, &sc->watches)) != NULL) {
		RB_REMOVE(inotify_watch_tree, &sc->watches, watch);
		mtx_unlock(&sc->lock);
		inotify_remove_watch(watch);
		mtx_lock(&sc->lock);
	}

	/* Make sure that any asynchronous vrele() calls are done. */
	mtx_unlock(&sc->lock);
	taskqueue_drain(taskqueue_thread, &sc->reaptask);
	mtx_lock(&sc->lock);
	KASSERT(RB_EMPTY(&sc->watches),
	    ("%s: watches not empty in %p", __func__, sc));
	KASSERT(TAILQ_EMPTY(&sc->deadwatches),
	    ("%s: deadwatches not empty in %p", __func__, sc));

	/* Drop pending events. */
	while (!STAILQ_EMPTY(&sc->pending)) {
		rec = inotify_dequeue(sc);
		if (rec != &sc->overflow)
			free(rec, M_INOTIFY);
	}
	mtx_unlock(&sc->lock);
	seldrain(&sc->sel);
	knlist_destroy(&sc->sel.si_note);
	mtx_destroy(&sc->lock);
	crfree(sc->cred);
	free(sc, M_INOTIFY);
	return (0);
}

static int
inotify_fill_kinfo(struct file *fp, struct kinfo_file *kif,
    struct filedesc *fdp)
{
	struct inotify_softc *sc;

	sc = fp->f_data;

	kif->kf_type = KF_TYPE_INOTIFY;
	kif->kf_un.kf_inotify.kf_inotify_npending = sc->npending;
	kif->kf_un.kf_inotify.kf_inotify_nbpending = sc->nbpending;
	return (0);
}

int
inotify_create_file(struct thread *td, struct file *fp, int flags, int *fflagsp)
{
	struct inotify_softc *sc;
	int fflags;

	if ((flags & ~(IN_NONBLOCK | IN_CLOEXEC)) != 0)
		return (EINVAL);

	if (!chginotifycnt(td->td_ucred->cr_ruidinfo, 1,
	    inotify_max_user_instances))
		return (EMFILE);

	sc = malloc(sizeof(*sc), M_INOTIFY, M_WAITOK | M_ZERO);
	sc->nextwatch = 1; /* Required for compatibility. */
	STAILQ_INIT(&sc->pending);
	RB_INIT(&sc->watches);
	TAILQ_INIT(&sc->deadwatches);
	TASK_INIT(&sc->reaptask, 0, inotify_reap, sc);
	mtx_init(&sc->lock, "inotify", NULL, MTX_DEF);
	knlist_init_mtx(&sc->sel.si_note, &sc->lock);
	sc->cred = crhold(td->td_ucred);
	sc->ino = atomic_fetchadd_64(&inotify_ino, 1);

	fflags = FREAD;
	if ((flags & IN_NONBLOCK) != 0)
		fflags |= FNONBLOCK;
	if ((flags & IN_CLOEXEC) != 0)
		*fflagsp |= O_CLOEXEC;
	finit(fp, fflags, DTYPE_INOTIFY, sc, &inotifyfdops);

	return (0);
}

static struct inotify_record *
inotify_alloc_record(uint32_t wd, const char *name, size_t namelen, int event,
    uint32_t cookie, int waitok)
{
	struct inotify_event *evp;
	struct inotify_record *rec;

	rec = malloc(sizeof(*rec) + _IN_NAMESIZE(namelen), M_INOTIFY,
	    waitok | M_ZERO);
	if (rec == NULL)
		return (NULL);
	evp = &rec->ev;
	evp->wd = wd;
	evp->mask = event;
	evp->cookie = cookie;
	evp->len = _IN_NAMESIZE(namelen);
	if (name != NULL)
		memcpy(evp->name, name, namelen);
	return (rec);
}

static bool
inotify_can_coalesce(struct inotify_softc *sc, struct inotify_event *evp)
{
	struct inotify_record *prev;

	mtx_assert(&sc->lock, MA_OWNED);

	prev = STAILQ_LAST(&sc->pending, inotify_record, link);
	return (prev != NULL && prev->ev.mask == evp->mask &&
	    prev->ev.wd == evp->wd && prev->ev.cookie == evp->cookie &&
	    prev->ev.len == evp->len &&
	    memcmp(prev->ev.name, evp->name, evp->len) == 0);
}

static void
inotify_overflow_event(struct inotify_event *evp)
{
	evp->mask = IN_Q_OVERFLOW;
	evp->wd = -1;
	evp->cookie = 0;
	evp->len = 0;
}

/*
 * Put an event record on the queue for an inotify desscriptor.  Return false if
 * the record was not enqueued for some reason, true otherwise.
 */
static bool
inotify_queue_record(struct inotify_softc *sc, struct inotify_record *rec)
{
	struct inotify_event *evp;

	mtx_assert(&sc->lock, MA_OWNED);

	evp = &rec->ev;
	if (__predict_false(rec == &sc->overflow)) {
		/*
		 * Is the overflow record already in the queue?  If so, there's
		 * not much else we can do: we're here because a kernel memory
		 * shortage prevented new record allocations.
		 */
		counter_u64_add(inotify_event_drops, 1);
		if (evp->mask == IN_Q_OVERFLOW)
			return (false);
		inotify_overflow_event(evp);
	} else {
		/* Try to coalesce duplicate events. */
		if (inotify_coalesce && inotify_can_coalesce(sc, evp))
			return (false);

		/*
		 * Would this one overflow the queue?  If so, convert it to an
		 * overflow event and try again to coalesce.
		 */
		if (sc->npending >= inotify_max_queued_events) {
			counter_u64_add(inotify_event_drops, 1);
			inotify_overflow_event(evp);
			if (inotify_can_coalesce(sc, evp))
				return (false);
		}
	}
	inotify_enqueue(sc, rec, false);
	selwakeup(&sc->sel);
	KNOTE_LOCKED(&sc->sel.si_note, 0);
	wakeup(&sc->pending);
	return (true);
}

static void
inotify_log_one(struct inotify_watch *watch, const char *name, size_t namelen,
    int event, uint32_t cookie)
{
	struct inotify_watch key;
	struct inotify_softc *sc;
	struct inotify_record *rec;
	bool allocfail;

	mtx_assert(&watch->vp->v_pollinfo->vpi_lock, MA_OWNED);

	sc = watch->sc;
	rec = inotify_alloc_record(watch->wd, name, namelen, event, cookie,
	    M_NOWAIT);
	if (rec == NULL) {
		rec = &sc->overflow;
		allocfail = true;
	} else {
		allocfail = false;
	}

	mtx_lock(&sc->lock);
	if (!inotify_queue_record(sc, rec) && rec != &sc->overflow)
		free(rec, M_INOTIFY);
	if ((watch->mask & IN_ONESHOT) != 0 ||
	    (event & (IN_DELETE_SELF | IN_UNMOUNT)) != 0) {
		if (!allocfail) {
			rec = inotify_alloc_record(watch->wd, NULL, 0,
			    IN_IGNORED, 0, M_NOWAIT);
			if (rec == NULL)
				rec = &sc->overflow;
			if (!inotify_queue_record(sc, rec) &&
			    rec != &sc->overflow)
				free(rec, M_INOTIFY);
		}

		/*
		 * Remove the watch, taking care to handle races with
		 * inotify_close().  The thread that removes the watch is
		 * responsible for freeing it.
		 */
		key.wd = watch->wd;
		if (RB_FIND(inotify_watch_tree, &sc->watches, &key) != NULL) {
			RB_REMOVE(inotify_watch_tree, &sc->watches, watch);
			inotify_unlink_watch_locked(sc, watch);

			/*
			 * Defer the vrele() to a sleepable thread context.
			 */
			TAILQ_INSERT_TAIL(&sc->deadwatches, watch, vlink);
			taskqueue_enqueue(taskqueue_thread, &sc->reaptask);
		}
	}
	mtx_unlock(&sc->lock);
}

void
inotify_log(struct vnode *vp, const char *name, size_t namelen, int event,
    uint32_t cookie)
{
	struct inotify_watch *watch, *tmp;

	KASSERT((event & ~(IN_ALL_EVENTS | IN_ISDIR | IN_UNMOUNT)) == 0,
	    ("inotify_log: invalid event %#x", event));

	mtx_lock(&vp->v_pollinfo->vpi_lock);
	TAILQ_FOREACH_SAFE(watch, &vp->v_pollinfo->vpi_inotify, vlink, tmp) {
		KASSERT(watch->vp == vp,
		    ("inotify_log: watch %p vp != vp", watch));
		if ((watch->mask & event) != 0 || event == IN_UNMOUNT)
			inotify_log_one(watch, name, namelen, event, cookie);
	}
	mtx_unlock(&vp->v_pollinfo->vpi_lock);
}

/*
 * An inotify event occurred on a watched vnode.
 */
void
vn_inotify(struct vnode *vp, struct vnode *dvp, struct componentname *cnp,
    int event, uint32_t cookie)
{
	int isdir;

	VNPASS(vp->v_holdcnt > 0, vp);

	isdir = vp->v_type == VDIR ? IN_ISDIR : 0;

	if (dvp != NULL) {
		VNPASS(dvp->v_holdcnt > 0, dvp);

		/*
		 * Should we log an event for the vnode itself?
		 */
		if ((vn_irflag_read(vp) & VIRF_INOTIFY) != 0) {
			int selfevent;

			switch (event) {
			case _IN_MOVE_DELETE:
			case IN_DELETE:
				/*
				 * IN_DELETE_SELF is only generated when the
				 * last hard link of a file is removed.
				 */
				selfevent = IN_DELETE_SELF;
				if (vp->v_type != VDIR) {
					struct vattr va;
					int error;

					error = VOP_GETATTR(vp, &va,
					    cnp->cn_cred);
					if (error == 0 && va.va_nlink != 0)
						selfevent = 0;
				}
				break;
			case IN_MOVED_FROM:
				cookie = 0;
				selfevent = IN_MOVE_SELF;
				break;
			case _IN_ATTRIB_LINKCOUNT:
				selfevent = IN_ATTRIB;
				break;
			default:
				selfevent = event;
				break;
			}

			if ((selfevent & ~_IN_DIR_EVENTS) != 0) {
				inotify_log(vp, NULL, 0, selfevent | isdir,
				    cookie);
			}
		}

		/*
		 * Something is watching the directory through which this vnode
		 * was referenced, so we may need to log the event.
		 */
		if ((event & IN_ALL_EVENTS) != 0 &&
		    (vn_irflag_read(dvp) & VIRF_INOTIFY) != 0) {
			inotify_log(dvp, cnp->cn_nameptr,
			    cnp->cn_namelen, event | isdir, cookie);
		}
	} else {
		/*
		 * We don't know which watched directory might contain the
		 * vnode, so we have to fall back to searching the name cache.
		 */
		cache_vop_inotify(vp, event, cookie);
	}
}

int
vn_inotify_add_watch(struct vnode *vp, struct inotify_softc *sc, uint32_t mask,
    uint32_t *wdp, struct thread *td)
{
	struct inotify_watch *watch, *watch1;
	uint32_t wd;

	/*
	 * If this is a directory, make sure all of its entries are present in
	 * the name cache so that we're able to look them up if an event occurs.
	 * The persistent reference on the directory prevents the outgoing name
	 * cache entries from being reclaimed.
	 */
	if (vp->v_type == VDIR) {
		struct dirent *dp;
		char *buf;
		off_t off;
		size_t buflen, len;
		int eof, error;

		buflen = 128 * sizeof(struct dirent);
		buf = malloc(buflen, M_TEMP, M_WAITOK);

		error = 0;
		len = off = eof = 0;
		for (;;) {
			struct nameidata nd;

			error = vn_dir_next_dirent(vp, td, buf, buflen, &dp,
			    &len, &off, &eof);
			if (error != 0)
				break;
			if (len == 0)
				/* Finished reading. */
				break;
			if (strcmp(dp->d_name, ".") == 0 ||
			    strcmp(dp->d_name, "..") == 0)
				continue;

			/*
			 * namei() consumes a reference on the starting
			 * directory if it's specified as a vnode.
			 */
			vrefact(vp);
			VOP_UNLOCK(vp);
			NDINIT_ATVP(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE,
			    dp->d_name, vp);
			error = namei(&nd);
			vn_lock(vp, LK_SHARED | LK_RETRY);
			if (error != 0)
				break;
			NDFREE_PNBUF(&nd);
			vn_irflag_set_cond(nd.ni_vp, VIRF_INOTIFY_PARENT);
			vrele(nd.ni_vp);
		}
		free(buf, M_TEMP);
		if (error != 0)
			return (error);
	}

	/*
	 * The vnode referenced in kern_inotify_add_watch() might be different
	 * than this one if nullfs is in the picture.
	 */
	vrefact(vp);
	watch = malloc(sizeof(*watch), M_INOTIFY, M_WAITOK | M_ZERO);
	watch->sc = sc;
	watch->vp = vp;
	watch->mask = mask;

	/*
	 * Are we updating an existing watch?  Search the vnode's list rather
	 * than that of the softc, as the former is likely to be shorter.
	 */
	v_addpollinfo(vp);
	mtx_lock(&vp->v_pollinfo->vpi_lock);
	TAILQ_FOREACH(watch1, &vp->v_pollinfo->vpi_inotify, vlink) {
		if (watch1->sc == sc)
			break;
	}
	mtx_lock(&sc->lock);
	if (watch1 != NULL) {
		mtx_unlock(&vp->v_pollinfo->vpi_lock);

		/*
		 * We found an existing watch, update it based on our flags.
		 */
		if ((mask & IN_MASK_CREATE) != 0) {
			mtx_unlock(&sc->lock);
			vrele(vp);
			free(watch, M_INOTIFY);
			return (EEXIST);
		}
		if ((mask & IN_MASK_ADD) != 0)
			watch1->mask |= mask;
		else
			watch1->mask = mask;
		*wdp = watch1->wd;
		mtx_unlock(&sc->lock);
		vrele(vp);
		free(watch, M_INOTIFY);
		return (EJUSTRETURN);
	}

	/*
	 * We're creating a new watch.  Add it to the softc and vnode watch
	 * lists.
	 */
	do {
		struct inotify_watch key;

		/*
		 * Search for the next available watch descriptor.  This is
		 * implemented so as to avoid reusing watch descriptors for as
		 * long as possible.
		 */
		key.wd = wd = sc->nextwatch++;
		watch1 = RB_FIND(inotify_watch_tree, &sc->watches, &key);
	} while (watch1 != NULL || wd == 0);
	watch->wd = wd;
	RB_INSERT(inotify_watch_tree, &sc->watches, watch);
	TAILQ_INSERT_TAIL(&vp->v_pollinfo->vpi_inotify, watch, vlink);
	mtx_unlock(&sc->lock);
	mtx_unlock(&vp->v_pollinfo->vpi_lock);
	vn_irflag_set_cond(vp, VIRF_INOTIFY);

	*wdp = wd;

	return (0);
}

void
vn_inotify_revoke(struct vnode *vp)
{
	if (vp->v_pollinfo == NULL) {
		/* This is a nullfs vnode which shadows a watched vnode. */
		return;
	}
	inotify_log(vp, NULL, 0, IN_UNMOUNT, 0);
}

static int
fget_inotify(struct thread *td, int fd, const cap_rights_t *needrightsp,
    struct file **fpp)
{
	struct file *fp;
	int error;

	error = fget(td, fd, needrightsp, &fp);
	if (error != 0)
		return (error);
	if (fp->f_type != DTYPE_INOTIFY) {
		fdrop(fp, td);
		return (EINVAL);
	}
	*fpp = fp;
	return (0);
}

int
kern_inotify_add_watch(int fd, int dfd, const char *path, uint32_t mask,
    struct thread *td)
{
	struct nameidata nd;
	struct file *fp;
	struct inotify_softc *sc;
	struct vnode *vp;
	uint32_t wd;
	int count, error;

	fp = NULL;
	vp = NULL;

	if ((mask & IN_ALL_EVENTS) == 0)
		return (EXTERROR(EINVAL, "no events specified"));
	if ((mask & (IN_MASK_ADD | IN_MASK_CREATE)) ==
	    (IN_MASK_ADD | IN_MASK_CREATE))
		return (EXTERROR(EINVAL,
		    "IN_MASK_ADD and IN_MASK_CREATE are mutually exclusive"));
	if ((mask & ~(IN_ALL_EVENTS | _IN_ALL_FLAGS | IN_UNMOUNT)) != 0)
		return (EXTERROR(EINVAL, "unrecognized flag"));

	error = fget_inotify(td, fd, &cap_inotify_add_rights, &fp);
	if (error != 0)
		return (error);
	sc = fp->f_data;

	NDINIT_AT(&nd, LOOKUP,
	    ((mask & IN_DONT_FOLLOW) ? NOFOLLOW : FOLLOW) | LOCKLEAF |
	    LOCKSHARED | AUDITVNODE1, UIO_USERSPACE, path, dfd);
	error = namei(&nd);
	if (error != 0)
		goto out;
	NDFREE_PNBUF(&nd);
	vp = nd.ni_vp;

	error = VOP_ACCESS(vp, VREAD, td->td_ucred, td);
	if (error != 0)
		goto out;

	if ((mask & IN_ONLYDIR) != 0 && vp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}

	count = atomic_fetchadd_int(&inotify_watches, 1);
	if (count > inotify_max_watches) {
		atomic_subtract_int(&inotify_watches, 1);
		error = ENOSPC;
		goto out;
	}
	if (!chginotifywatchcnt(sc->cred->cr_ruidinfo, 1,
	    inotify_max_user_watches)) {
		atomic_subtract_int(&inotify_watches, 1);
		error = ENOSPC;
		goto out;
	}
	error = VOP_INOTIFY_ADD_WATCH(vp, sc, mask, &wd, td);
	if (error != 0) {
		atomic_subtract_int(&inotify_watches, 1);
		(void)chginotifywatchcnt(sc->cred->cr_ruidinfo, -1, 0);
		if (error == EJUSTRETURN) {
			/* We updated an existing watch, everything is ok. */
			error = 0;
		} else {
			goto out;
		}
	}
	td->td_retval[0] = wd;

out:
	if (vp != NULL)
		vput(vp);
	fdrop(fp, td);
	return (error);
}

int
sys_inotify_add_watch_at(struct thread *td,
    struct inotify_add_watch_at_args *uap)
{
	return (kern_inotify_add_watch(uap->fd, uap->dfd, uap->path,
	    uap->mask, td));
}

int
kern_inotify_rm_watch(int fd, uint32_t wd, struct thread *td)
{
	struct file *fp;
	struct inotify_softc *sc;
	struct inotify_record *rec;
	struct inotify_watch key, *watch;
	int error;

	error = fget_inotify(td, fd, &cap_inotify_rm_rights, &fp);
	if (error != 0)
		return (error);
	sc = fp->f_data;

	rec = inotify_alloc_record(wd, NULL, 0, IN_IGNORED, 0, M_WAITOK);

	/*
	 * For compatibility with Linux, we do not remove pending events
	 * associated with the watch.  Watch descriptors are implemented so as
	 * to avoid being reused for as long as possible, so one hopes that any
	 * pending events from the removed watch descriptor will be removed
	 * before the watch descriptor is recycled.
	 */
	key.wd = wd;
	mtx_lock(&sc->lock);
	watch = RB_FIND(inotify_watch_tree, &sc->watches, &key);
	if (watch == NULL) {
		free(rec, M_INOTIFY);
		error = EINVAL;
	} else {
		RB_REMOVE(inotify_watch_tree, &sc->watches, watch);
		if (!inotify_queue_record(sc, rec)) {
			free(rec, M_INOTIFY);
			error = 0;
		}
	}
	mtx_unlock(&sc->lock);
	if (watch != NULL)
		inotify_remove_watch(watch);
	fdrop(fp, td);
	return (error);
}

int
sys_inotify_rm_watch(struct thread *td, struct inotify_rm_watch_args *uap)
{
	return (kern_inotify_rm_watch(uap->fd, uap->wd, td));
}
