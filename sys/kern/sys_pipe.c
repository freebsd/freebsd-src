/*-
 * Copyright (c) 1996 John S. Dyson
 * Copyright (c) 2012 Giovanni Trematerra
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John S. Dyson.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 */

/*
 * This file contains a high-performance replacement for the socket-based
 * pipes scheme originally used in FreeBSD/4.4Lite.  It does not support
 * all features of sockets, but does do everything that pipes normally
 * do.
 */

/*
 * This code has two modes of operation, a small write mode and a large
 * write mode.  The small write mode acts like conventional pipes with
 * a kernel buffer.  If the buffer is less than PIPE_MINDIRECT, then the
 * "normal" pipe buffering is done.  If the buffer is between PIPE_MINDIRECT
 * and PIPE_SIZE in size, the sending process pins the underlying pages in
 * memory, and the receiving process copies directly from these pinned pages
 * in the sending process.
 *
 * If the sending process receives a signal, it is possible that it will
 * go away, and certainly its address space can change, because control
 * is returned back to the user-mode side.  In that case, the pipe code
 * arranges to copy the buffer supplied by the user process, to a pageable
 * kernel buffer, and the receiving process will grab the data from the
 * pageable kernel buffer.  Since signals don't happen all that often,
 * the copy operation is normally eliminated.
 *
 * The constant PIPE_MINDIRECT is chosen to make sure that buffering will
 * happen for small transfers so that the system will not spend all of
 * its time context switching.
 *
 * In order to limit the resource use of pipes, two sysctls exist:
 *
 * kern.ipc.maxpipekva - This is a hard limit on the amount of pageable
 * address space available to us in pipe_map. This value is normally
 * autotuned, but may also be loader tuned.
 *
 * kern.ipc.pipekva - This read-only sysctl tracks the current amount of
 * memory in use by pipes.
 *
 * Based on how large pipekva is relative to maxpipekva, the following
 * will happen:
 *
 * 0% - 50%:
 *     New pipes are given 16K of memory backing, pipes may dynamically
 *     grow to as large as 64K where needed.
 * 50% - 75%:
 *     New pipes are given 4K (or PAGE_SIZE) of memory backing,
 *     existing pipes may NOT grow.
 * 75% - 100%:
 *     New pipes are given 4K (or PAGE_SIZE) of memory backing,
 *     existing pipes will be shrunk down to 4K whenever possible.
 *
 * Resizing may be disabled by setting kern.ipc.piperesizeallowed=0.  If
 * that is set,  the only resize that will occur is the 0 -> SMALL_PIPE_SIZE
 * resize which MUST occur for reverse-direction pipes when they are
 * first used.
 *
 * Additional information about the current state of pipes may be obtained
 * from kern.ipc.pipes, kern.ipc.pipefragretry, kern.ipc.pipeallocfail,
 * and kern.ipc.piperesizefail.
 *
 * Locking rules:  There are two locks present here:  A mutex, used via
 * PIPE_LOCK, and a flag, used via pipelock().  All locking is done via
 * the flag, as mutexes can not persist over uiomove.  The mutex
 * exists only to guard access to the flag, and is not in itself a
 * locking mechanism.  Also note that there is only a single mutex for
 * both directions of a pipe.
 *
 * As pipelock() may have to sleep before it can acquire the flag, it
 * is important to reread all data after a call to pipelock(); everything
 * in the structure may have changed.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/ttycom.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/priv.h>
#include <sys/selinfo.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/pipe.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/event.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/uma.h>

/*
 * Use this define if you want to disable *fancy* VM things.  Expect an
 * approx 30% decrease in transfer rate.  This could be useful for
 * NetBSD or OpenBSD.
 */
/* #define PIPE_NODIRECT */

#define PIPE_PEER(pipe)	\
	(((pipe)->pipe_type & PIPE_TYPE_NAMED) ? (pipe) : ((pipe)->pipe_peer))

/*
 * interfaces to the outside world
 */
static fo_rdwr_t	pipe_read;
static fo_rdwr_t	pipe_write;
static fo_truncate_t	pipe_truncate;
static fo_ioctl_t	pipe_ioctl;
static fo_poll_t	pipe_poll;
static fo_kqfilter_t	pipe_kqfilter;
static fo_stat_t	pipe_stat;
static fo_close_t	pipe_close;
static fo_chmod_t	pipe_chmod;
static fo_chown_t	pipe_chown;
static fo_fill_kinfo_t	pipe_fill_kinfo;

const struct fileops pipeops = {
	.fo_read = pipe_read,
	.fo_write = pipe_write,
	.fo_truncate = pipe_truncate,
	.fo_ioctl = pipe_ioctl,
	.fo_poll = pipe_poll,
	.fo_kqfilter = pipe_kqfilter,
	.fo_stat = pipe_stat,
	.fo_close = pipe_close,
	.fo_chmod = pipe_chmod,
	.fo_chown = pipe_chown,
	.fo_sendfile = invfo_sendfile,
	.fo_fill_kinfo = pipe_fill_kinfo,
	.fo_cmp = file_kcmp_generic,
	.fo_flags = DFLAG_PASSABLE
};

static void	filt_pipedetach(struct knote *kn);
static void	filt_pipedetach_notsup(struct knote *kn);
static int	filt_pipenotsup(struct knote *kn, long hint);
static int	filt_piperead(struct knote *kn, long hint);
static int	filt_pipewrite(struct knote *kn, long hint);
static int	filt_pipedump(struct proc *p, struct knote *kn,
    struct kinfo_knote *kin);

static const struct filterops pipe_nfiltops = {
	.f_isfd = 1,
	.f_detach = filt_pipedetach_notsup,
	.f_event = filt_pipenotsup
	/* no userdump */
};
static const struct filterops pipe_rfiltops = {
	.f_isfd = 1,
	.f_detach = filt_pipedetach,
	.f_event = filt_piperead,
	.f_userdump = filt_pipedump,
};
static const struct filterops pipe_wfiltops = {
	.f_isfd = 1,
	.f_detach = filt_pipedetach,
	.f_event = filt_pipewrite,
	.f_userdump = filt_pipedump,
};

/*
 * Default pipe buffer size(s), this can be kind-of large now because pipe
 * space is pageable.  The pipe code will try to maintain locality of
 * reference for performance reasons, so small amounts of outstanding I/O
 * will not wipe the cache.
 */
#define MINPIPESIZE (PIPE_SIZE/3)
#define MAXPIPESIZE (2*PIPE_SIZE/3)

static long amountpipekva;
static int pipefragretry;
static int pipeallocfail;
static int piperesizefail;
static int piperesizeallowed = 1;
static long pipe_mindirect = PIPE_MINDIRECT;
static int pipebuf_reserv = 2;

SYSCTL_LONG(_kern_ipc, OID_AUTO, maxpipekva, CTLFLAG_RDTUN | CTLFLAG_NOFETCH,
	   &maxpipekva, 0, "Pipe KVA limit");
SYSCTL_LONG(_kern_ipc, OID_AUTO, pipekva, CTLFLAG_RD,
	   &amountpipekva, 0, "Pipe KVA usage");
SYSCTL_INT(_kern_ipc, OID_AUTO, pipefragretry, CTLFLAG_RD,
	  &pipefragretry, 0, "Pipe allocation retries due to fragmentation");
SYSCTL_INT(_kern_ipc, OID_AUTO, pipeallocfail, CTLFLAG_RD,
	  &pipeallocfail, 0, "Pipe allocation failures");
SYSCTL_INT(_kern_ipc, OID_AUTO, piperesizefail, CTLFLAG_RD,
	  &piperesizefail, 0, "Pipe resize failures");
SYSCTL_INT(_kern_ipc, OID_AUTO, piperesizeallowed, CTLFLAG_RW,
	  &piperesizeallowed, 0, "Pipe resizing allowed");
SYSCTL_INT(_kern_ipc, OID_AUTO, pipebuf_reserv, CTLFLAG_RW,
    &pipebuf_reserv, 0,
    "Superuser-reserved percentage of the pipe buffers space");

static void pipeinit(void *dummy __unused);
static void pipeclose(struct pipe *cpipe);
static void pipe_free_kmem(struct pipe *cpipe);
static int pipe_create(struct pipe *pipe, bool backing);
static int pipe_paircreate(struct thread *td, struct pipepair **p_pp);
static __inline int pipelock(struct pipe *cpipe, bool catch);
static __inline void pipeunlock(struct pipe *cpipe);
static void pipe_timestamp(struct timespec *tsp);
#ifndef PIPE_NODIRECT
static int pipe_build_write_buffer(struct pipe *wpipe, struct uio *uio);
static void pipe_destroy_write_buffer(struct pipe *wpipe);
static int pipe_direct_write(struct pipe *wpipe, struct uio *uio);
static void pipe_clone_write_buffer(struct pipe *wpipe);
#endif
static int pipespace(struct pipe *cpipe, int size);
static int pipespace_new(struct pipe *cpipe, int size);

static int	pipe_zone_ctor(void *mem, int size, void *arg, int flags);
static int	pipe_zone_init(void *mem, int size, int flags);
static void	pipe_zone_fini(void *mem, int size);

static uma_zone_t pipe_zone;
static struct unrhdr64 pipeino_unr;
static dev_t pipedev_ino;

SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_ANY, pipeinit, NULL);

static void
pipeinit(void *dummy __unused)
{

	pipe_zone = uma_zcreate("pipe", sizeof(struct pipepair),
	    pipe_zone_ctor, NULL, pipe_zone_init, pipe_zone_fini,
	    UMA_ALIGN_PTR, 0);
	KASSERT(pipe_zone != NULL, ("pipe_zone not initialized"));
	new_unrhdr64(&pipeino_unr, 1);
	pipedev_ino = devfs_alloc_cdp_inode();
	KASSERT(pipedev_ino > 0, ("pipe dev inode not initialized"));
}

static int
sysctl_handle_pipe_mindirect(SYSCTL_HANDLER_ARGS)
{
	int error = 0;
	long tmp_pipe_mindirect = pipe_mindirect;

	error = sysctl_handle_long(oidp, &tmp_pipe_mindirect, arg2, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	/*
	 * Don't allow pipe_mindirect to be set so low that we violate
	 * atomicity requirements.
	 */
	if (tmp_pipe_mindirect <= PIPE_BUF)
		return (EINVAL);
	pipe_mindirect = tmp_pipe_mindirect;
	return (0);
}
SYSCTL_OID(_kern_ipc, OID_AUTO, pipe_mindirect, CTLTYPE_LONG | CTLFLAG_RW,
    &pipe_mindirect, 0, sysctl_handle_pipe_mindirect, "L",
    "Minimum write size triggering VM optimization");

static int
pipe_zone_ctor(void *mem, int size, void *arg, int flags)
{
	struct pipepair *pp;
	struct pipe *rpipe, *wpipe;

	KASSERT(size == sizeof(*pp), ("pipe_zone_ctor: wrong size"));

	pp = (struct pipepair *)mem;

	/*
	 * We zero both pipe endpoints to make sure all the kmem pointers
	 * are NULL, flag fields are zero'd, etc.  We timestamp both
	 * endpoints with the same time.
	 */
	rpipe = &pp->pp_rpipe;
	bzero(rpipe, sizeof(*rpipe));
	pipe_timestamp(&rpipe->pipe_ctime);
	rpipe->pipe_atime = rpipe->pipe_mtime = rpipe->pipe_ctime;

	wpipe = &pp->pp_wpipe;
	bzero(wpipe, sizeof(*wpipe));
	wpipe->pipe_ctime = rpipe->pipe_ctime;
	wpipe->pipe_atime = wpipe->pipe_mtime = rpipe->pipe_ctime;

	rpipe->pipe_peer = wpipe;
	rpipe->pipe_pair = pp;
	wpipe->pipe_peer = rpipe;
	wpipe->pipe_pair = pp;

	/*
	 * Mark both endpoints as present; they will later get free'd
	 * one at a time.  When both are free'd, then the whole pair
	 * is released.
	 */
	rpipe->pipe_present = PIPE_ACTIVE;
	wpipe->pipe_present = PIPE_ACTIVE;

	/*
	 * Eventually, the MAC Framework may initialize the label
	 * in ctor or init, but for now we do it elswhere to avoid
	 * blocking in ctor or init.
	 */
	pp->pp_label = NULL;

	return (0);
}

static int
pipe_zone_init(void *mem, int size, int flags)
{
	struct pipepair *pp;

	KASSERT(size == sizeof(*pp), ("pipe_zone_init: wrong size"));

	pp = (struct pipepair *)mem;

	mtx_init(&pp->pp_mtx, "pipe mutex", NULL, MTX_DEF | MTX_NEW);
	return (0);
}

static void
pipe_zone_fini(void *mem, int size)
{
	struct pipepair *pp;

	KASSERT(size == sizeof(*pp), ("pipe_zone_fini: wrong size"));

	pp = (struct pipepair *)mem;

	mtx_destroy(&pp->pp_mtx);
}

static int
pipe_paircreate(struct thread *td, struct pipepair **p_pp)
{
	struct pipepair *pp;
	struct pipe *rpipe, *wpipe;
	int error;

	*p_pp = pp = uma_zalloc(pipe_zone, M_WAITOK);
#ifdef MAC
	/*
	 * The MAC label is shared between the connected endpoints.  As a
	 * result mac_pipe_init() and mac_pipe_create() are called once
	 * for the pair, and not on the endpoints.
	 */
	mac_pipe_init(pp);
	mac_pipe_create(td->td_ucred, pp);
#endif
	rpipe = &pp->pp_rpipe;
	wpipe = &pp->pp_wpipe;
	pp->pp_owner = crhold(td->td_ucred);

	knlist_init_mtx(&rpipe->pipe_sel.si_note, PIPE_MTX(rpipe));
	knlist_init_mtx(&wpipe->pipe_sel.si_note, PIPE_MTX(wpipe));

	/*
	 * Only the forward direction pipe is backed by big buffer by
	 * default.
	 */
	error = pipe_create(rpipe, true);
	if (error != 0)
		goto fail;
	error = pipe_create(wpipe, false);
	if (error != 0) {
		/*
		 * This cleanup leaves the pipe inode number for rpipe
		 * still allocated, but never used.  We do not free
		 * inode numbers for opened pipes, which is required
		 * for correctness because numbers must be unique.
		 * But also it avoids any memory use by the unr
		 * allocator, so stashing away the transient inode
		 * number is reasonable.
		 */
		pipe_free_kmem(rpipe);
		goto fail;
	}

	rpipe->pipe_state |= PIPE_DIRECTOK;
	wpipe->pipe_state |= PIPE_DIRECTOK;
	return (0);

fail:
	knlist_destroy(&rpipe->pipe_sel.si_note);
	knlist_destroy(&wpipe->pipe_sel.si_note);
	crfree(pp->pp_owner);
#ifdef MAC
	mac_pipe_destroy(pp);
#endif
	uma_zfree(pipe_zone, pp);
	return (error);
}

int
pipe_named_ctor(struct pipe **ppipe, struct thread *td)
{
	struct pipepair *pp;
	int error;

	error = pipe_paircreate(td, &pp);
	if (error != 0)
		return (error);
	pp->pp_rpipe.pipe_type |= PIPE_TYPE_NAMED;
	*ppipe = &pp->pp_rpipe;
	return (0);
}

void
pipe_dtor(struct pipe *dpipe)
{
	struct pipe *peer;

	peer = (dpipe->pipe_type & PIPE_TYPE_NAMED) != 0 ? dpipe->pipe_peer : NULL;
	funsetown(&dpipe->pipe_sigio);
	pipeclose(dpipe);
	if (peer != NULL) {
		funsetown(&peer->pipe_sigio);
		pipeclose(peer);
	}
}

/*
 * Get a timestamp.
 *
 * This used to be vfs_timestamp but the higher precision is unnecessary and
 * can very negatively affect performance in virtualized environments (e.g., on
 * vms running on amd64 when using the rdtscp instruction).
 */
static void
pipe_timestamp(struct timespec *tsp)
{

	getnanotime(tsp);
}

/*
 * The pipe system call for the DTYPE_PIPE type of pipes.  If we fail, let
 * the zone pick up the pieces via pipeclose().
 */
int
kern_pipe(struct thread *td, int fildes[2], int flags, struct filecaps *fcaps1,
    struct filecaps *fcaps2)
{
	struct file *rf, *wf;
	struct pipe *rpipe, *wpipe;
	struct pipepair *pp;
	int fd, fflags, error;

	error = pipe_paircreate(td, &pp);
	if (error != 0)
		return (error);
	rpipe = &pp->pp_rpipe;
	wpipe = &pp->pp_wpipe;
	error = falloc_caps(td, &rf, &fd, flags, fcaps1);
	if (error) {
		pipeclose(rpipe);
		pipeclose(wpipe);
		return (error);
	}
	/* An extra reference on `rf' has been held for us by falloc_caps(). */
	fildes[0] = fd;

	fflags = FREAD | FWRITE;
	if ((flags & O_NONBLOCK) != 0)
		fflags |= FNONBLOCK;

	/*
	 * Warning: once we've gotten past allocation of the fd for the
	 * read-side, we can only drop the read side via fdrop() in order
	 * to avoid races against processes which manage to dup() the read
	 * side while we are blocked trying to allocate the write side.
	 */
	finit(rf, fflags, DTYPE_PIPE, rpipe, &pipeops);
	error = falloc_caps(td, &wf, &fd, flags, fcaps2);
	if (error) {
		fdclose(td, rf, fildes[0]);
		fdrop(rf, td);
		/* rpipe has been closed by fdrop(). */
		pipeclose(wpipe);
		return (error);
	}
	/* An extra reference on `wf' has been held for us by falloc_caps(). */
	finit(wf, fflags, DTYPE_PIPE, wpipe, &pipeops);
	fdrop(wf, td);
	fildes[1] = fd;
	fdrop(rf, td);

	return (0);
}

#ifdef COMPAT_FREEBSD10
/* ARGSUSED */
int
freebsd10_pipe(struct thread *td, struct freebsd10_pipe_args *uap __unused)
{
	int error;
	int fildes[2];

	error = kern_pipe(td, fildes, 0, NULL, NULL);
	if (error)
		return (error);

	td->td_retval[0] = fildes[0];
	td->td_retval[1] = fildes[1];

	return (0);
}
#endif

int
sys_pipe2(struct thread *td, struct pipe2_args *uap)
{
	int error, fildes[2];

	if (uap->flags & ~(O_CLOEXEC | O_NONBLOCK))
		return (EINVAL);
	error = kern_pipe(td, fildes, uap->flags, NULL, NULL);
	if (error)
		return (error);
	error = copyout(fildes, uap->fildes, 2 * sizeof(int));
	if (error) {
		(void)kern_close(td, fildes[0]);
		(void)kern_close(td, fildes[1]);
	}
	return (error);
}

/*
 * Allocate kva for pipe circular buffer, the space is pageable
 * This routine will 'realloc' the size of a pipe safely, if it fails
 * it will retain the old buffer.
 * If it fails it will return ENOMEM.
 */
static int
pipespace_new(struct pipe *cpipe, int size)
{
	caddr_t buffer;
	int error, cnt, firstseg;
	static int curfail = 0;
	static struct timeval lastfail;

	KASSERT(!mtx_owned(PIPE_MTX(cpipe)), ("pipespace: pipe mutex locked"));
	KASSERT(!(cpipe->pipe_state & PIPE_DIRECTW),
		("pipespace: resize of direct writes not allowed"));
retry:
	cnt = cpipe->pipe_buffer.cnt;
	if (cnt > size)
		size = cnt;

	size = round_page(size);
	buffer = (caddr_t) vm_map_min(pipe_map);

	if (!chgpipecnt(cpipe->pipe_pair->pp_owner->cr_ruidinfo,
	    size, lim_cur(curthread, RLIMIT_PIPEBUF))) {
		if (cpipe->pipe_buffer.buffer == NULL &&
		    size > SMALL_PIPE_SIZE) {
			size = SMALL_PIPE_SIZE;
			goto retry;
		}
		return (ENOMEM);
	}

	vm_map_lock(pipe_map);
	if (priv_check(curthread, PRIV_PIPEBUF) != 0 && maxpipekva / 100 *
	    (100 - pipebuf_reserv) < amountpipekva + size) {
		vm_map_unlock(pipe_map);
		chgpipecnt(cpipe->pipe_pair->pp_owner->cr_ruidinfo, -size, 0);
		if (cpipe->pipe_buffer.buffer == NULL &&
		    size > SMALL_PIPE_SIZE) {
			size = SMALL_PIPE_SIZE;
			pipefragretry++;
			goto retry;
		}
		return (ENOMEM);
	}
	error = vm_map_find_locked(pipe_map, NULL, 0, (vm_offset_t *)&buffer,
	    size, 0, VMFS_ANY_SPACE, VM_PROT_RW, VM_PROT_RW, 0);
	vm_map_unlock(pipe_map);
	if (error != KERN_SUCCESS) {
		chgpipecnt(cpipe->pipe_pair->pp_owner->cr_ruidinfo, -size, 0);
		if (cpipe->pipe_buffer.buffer == NULL &&
		    size > SMALL_PIPE_SIZE) {
			size = SMALL_PIPE_SIZE;
			pipefragretry++;
			goto retry;
		}
		if (cpipe->pipe_buffer.buffer == NULL) {
			pipeallocfail++;
			if (ppsratecheck(&lastfail, &curfail, 1))
				printf("kern.ipc.maxpipekva exceeded; see tuning(7)\n");
		} else {
			piperesizefail++;
		}
		return (ENOMEM);
	}

	/* copy data, then free old resources if we're resizing */
	if (cnt > 0) {
		if (cpipe->pipe_buffer.in <= cpipe->pipe_buffer.out) {
			firstseg = cpipe->pipe_buffer.size - cpipe->pipe_buffer.out;
			bcopy(&cpipe->pipe_buffer.buffer[cpipe->pipe_buffer.out],
				buffer, firstseg);
			if ((cnt - firstseg) > 0)
				bcopy(cpipe->pipe_buffer.buffer, &buffer[firstseg],
					cpipe->pipe_buffer.in);
		} else {
			bcopy(&cpipe->pipe_buffer.buffer[cpipe->pipe_buffer.out],
				buffer, cnt);
		}
	}
	pipe_free_kmem(cpipe);
	cpipe->pipe_buffer.buffer = buffer;
	cpipe->pipe_buffer.size = size;
	cpipe->pipe_buffer.in = cnt;
	cpipe->pipe_buffer.out = 0;
	cpipe->pipe_buffer.cnt = cnt;
	atomic_add_long(&amountpipekva, cpipe->pipe_buffer.size);
	return (0);
}

/*
 * Wrapper for pipespace_new() that performs locking assertions.
 */
static int
pipespace(struct pipe *cpipe, int size)
{

	KASSERT(cpipe->pipe_state & PIPE_LOCKFL,
	    ("Unlocked pipe passed to pipespace"));
	return (pipespace_new(cpipe, size));
}

/*
 * lock a pipe for I/O, blocking other access
 */
static __inline int
pipelock(struct pipe *cpipe, bool catch)
{
	int error, prio;

	PIPE_LOCK_ASSERT(cpipe, MA_OWNED);

	prio = PRIBIO;
	if (catch)
		prio |= PCATCH;
	while (cpipe->pipe_state & PIPE_LOCKFL) {
		KASSERT(cpipe->pipe_waiters >= 0,
		    ("%s: bad waiter count %d", __func__,
		    cpipe->pipe_waiters));
		cpipe->pipe_waiters++;
		error = msleep(&cpipe->pipe_waiters, PIPE_MTX(cpipe), prio,
		    "pipelk", 0);
		cpipe->pipe_waiters--;
		if (error != 0)
			return (error);
	}
	cpipe->pipe_state |= PIPE_LOCKFL;
	return (0);
}

/*
 * unlock a pipe I/O lock
 */
static __inline void
pipeunlock(struct pipe *cpipe)
{

	PIPE_LOCK_ASSERT(cpipe, MA_OWNED);
	KASSERT(cpipe->pipe_state & PIPE_LOCKFL,
		("Unlocked pipe passed to pipeunlock"));
	KASSERT(cpipe->pipe_waiters >= 0,
	    ("%s: bad waiter count %d", __func__,
	    cpipe->pipe_waiters));
	cpipe->pipe_state &= ~PIPE_LOCKFL;
	if (cpipe->pipe_waiters > 0)
		wakeup_one(&cpipe->pipe_waiters);
}

void
pipeselwakeup(struct pipe *cpipe)
{

	PIPE_LOCK_ASSERT(cpipe, MA_OWNED);
	if (cpipe->pipe_state & PIPE_SEL) {
		selwakeuppri(&cpipe->pipe_sel, PSOCK);
		if (!SEL_WAITING(&cpipe->pipe_sel))
			cpipe->pipe_state &= ~PIPE_SEL;
	}
	if ((cpipe->pipe_state & PIPE_ASYNC) && cpipe->pipe_sigio)
		pgsigio(&cpipe->pipe_sigio, SIGIO, 0);
	KNOTE_LOCKED(&cpipe->pipe_sel.si_note, 0);
}

/*
 * Initialize and allocate VM and memory for pipe.  The structure
 * will start out zero'd from the ctor, so we just manage the kmem.
 */
static int
pipe_create(struct pipe *pipe, bool large_backing)
{
	int error;

	error = pipespace_new(pipe, !large_backing || amountpipekva >
	    maxpipekva / 2 ? SMALL_PIPE_SIZE : PIPE_SIZE);
	if (error == 0)
		pipe->pipe_ino = alloc_unr64(&pipeino_unr);
	return (error);
}

/* ARGSUSED */
static int
pipe_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct pipe *rpipe;
	int error;
	int nread = 0;
	int size;

	rpipe = fp->f_data;

	/*
	 * Try to avoid locking the pipe if we have nothing to do.
	 *
	 * There are programs which share one pipe amongst multiple processes
	 * and perform non-blocking reads in parallel, even if the pipe is
	 * empty.  This in particular is the case with BSD make, which when
	 * spawned with a high -j number can find itself with over half of the
	 * calls failing to find anything.
	 */
	if ((fp->f_flag & FNONBLOCK) != 0 && !mac_pipe_check_read_enabled()) {
		if (__predict_false(uio->uio_resid == 0))
			return (0);
		if ((atomic_load_short(&rpipe->pipe_state) & PIPE_EOF) == 0 &&
		    atomic_load_int(&rpipe->pipe_buffer.cnt) == 0 &&
		    atomic_load_int(&rpipe->pipe_pages.cnt) == 0)
			return (EAGAIN);
	}

	PIPE_LOCK(rpipe);
	++rpipe->pipe_busy;
	error = pipelock(rpipe, true);
	if (error)
		goto unlocked_error;

#ifdef MAC
	error = mac_pipe_check_read(active_cred, rpipe->pipe_pair);
	if (error)
		goto locked_error;
#endif
	if (amountpipekva > (3 * maxpipekva) / 4) {
		if ((rpipe->pipe_state & PIPE_DIRECTW) == 0 &&
		    rpipe->pipe_buffer.size > SMALL_PIPE_SIZE &&
		    rpipe->pipe_buffer.cnt <= SMALL_PIPE_SIZE &&
		    piperesizeallowed == 1) {
			PIPE_UNLOCK(rpipe);
			pipespace(rpipe, SMALL_PIPE_SIZE);
			PIPE_LOCK(rpipe);
		}
	}

	while (uio->uio_resid) {
		/*
		 * normal pipe buffer receive
		 */
		if (rpipe->pipe_buffer.cnt > 0) {
			size = rpipe->pipe_buffer.size - rpipe->pipe_buffer.out;
			if (size > rpipe->pipe_buffer.cnt)
				size = rpipe->pipe_buffer.cnt;
			if (size > uio->uio_resid)
				size = uio->uio_resid;

			PIPE_UNLOCK(rpipe);
			error = uiomove(
			    &rpipe->pipe_buffer.buffer[rpipe->pipe_buffer.out],
			    size, uio);
			PIPE_LOCK(rpipe);
			if (error)
				break;

			rpipe->pipe_buffer.out += size;
			if (rpipe->pipe_buffer.out >= rpipe->pipe_buffer.size)
				rpipe->pipe_buffer.out = 0;

			rpipe->pipe_buffer.cnt -= size;

			/*
			 * If there is no more to read in the pipe, reset
			 * its pointers to the beginning.  This improves
			 * cache hit stats.
			 */
			if (rpipe->pipe_buffer.cnt == 0) {
				rpipe->pipe_buffer.in = 0;
				rpipe->pipe_buffer.out = 0;
			}
			nread += size;
#ifndef PIPE_NODIRECT
		/*
		 * Direct copy, bypassing a kernel buffer.
		 */
		} else if ((size = rpipe->pipe_pages.cnt) != 0) {
			if (size > uio->uio_resid)
				size = (u_int) uio->uio_resid;
			PIPE_UNLOCK(rpipe);
			error = uiomove_fromphys(rpipe->pipe_pages.ms,
			    rpipe->pipe_pages.pos, size, uio);
			PIPE_LOCK(rpipe);
			if (error)
				break;
			nread += size;
			rpipe->pipe_pages.pos += size;
			rpipe->pipe_pages.cnt -= size;
			if (rpipe->pipe_pages.cnt == 0) {
				rpipe->pipe_state &= ~PIPE_WANTW;
				wakeup(rpipe);
			}
#endif
		} else {
			/*
			 * detect EOF condition
			 * read returns 0 on EOF, no need to set error
			 */
			if (rpipe->pipe_state & PIPE_EOF)
				break;

			/*
			 * If the "write-side" has been blocked, wake it up now.
			 */
			if (rpipe->pipe_state & PIPE_WANTW) {
				rpipe->pipe_state &= ~PIPE_WANTW;
				wakeup(rpipe);
			}

			/*
			 * Break if some data was read.
			 */
			if (nread > 0)
				break;

			/*
			 * Unlock the pipe buffer for our remaining processing.
			 * We will either break out with an error or we will
			 * sleep and relock to loop.
			 */
			pipeunlock(rpipe);

			/*
			 * Handle non-blocking mode operation or
			 * wait for more data.
			 */
			if (fp->f_flag & FNONBLOCK) {
				error = EAGAIN;
			} else {
				rpipe->pipe_state |= PIPE_WANTR;
				if ((error = msleep(rpipe, PIPE_MTX(rpipe),
				    PRIBIO | PCATCH,
				    "piperd", 0)) == 0)
					error = pipelock(rpipe, true);
			}
			if (error)
				goto unlocked_error;
		}
	}
#ifdef MAC
locked_error:
#endif
	pipeunlock(rpipe);

	/* XXX: should probably do this before getting any locks. */
	if (error == 0)
		pipe_timestamp(&rpipe->pipe_atime);
unlocked_error:
	--rpipe->pipe_busy;

	/*
	 * PIPE_WANT processing only makes sense if pipe_busy is 0.
	 */
	if ((rpipe->pipe_busy == 0) && (rpipe->pipe_state & PIPE_WANT)) {
		rpipe->pipe_state &= ~(PIPE_WANT|PIPE_WANTW);
		wakeup(rpipe);
	} else if (rpipe->pipe_buffer.cnt < MINPIPESIZE) {
		/*
		 * Handle write blocking hysteresis.
		 */
		if (rpipe->pipe_state & PIPE_WANTW) {
			rpipe->pipe_state &= ~PIPE_WANTW;
			wakeup(rpipe);
		}
	}

	/*
	 * Only wake up writers if there was actually something read.
	 * Otherwise, when calling read(2) at EOF, a spurious wakeup occurs.
	 */
	if (nread > 0 &&
	    rpipe->pipe_buffer.size - rpipe->pipe_buffer.cnt >= PIPE_BUF)
		pipeselwakeup(rpipe);

	PIPE_UNLOCK(rpipe);
	if (nread > 0)
		td->td_ru.ru_msgrcv++;
	return (error);
}

#ifndef PIPE_NODIRECT
/*
 * Map the sending processes' buffer into kernel space and wire it.
 * This is similar to a physical write operation.
 */
static int
pipe_build_write_buffer(struct pipe *wpipe, struct uio *uio)
{
	u_int size;
	int i;

	PIPE_LOCK_ASSERT(wpipe, MA_OWNED);
	KASSERT((wpipe->pipe_state & PIPE_DIRECTW) == 0,
	    ("%s: PIPE_DIRECTW set on %p", __func__, wpipe));
	KASSERT(wpipe->pipe_pages.cnt == 0,
	    ("%s: pipe map for %p contains residual data", __func__, wpipe));

	if (uio->uio_iov->iov_len > wpipe->pipe_buffer.size)
                size = wpipe->pipe_buffer.size;
	else
                size = uio->uio_iov->iov_len;

	wpipe->pipe_state |= PIPE_DIRECTW;
	PIPE_UNLOCK(wpipe);
	i = vm_fault_quick_hold_pages(&curproc->p_vmspace->vm_map,
	    (vm_offset_t)uio->uio_iov->iov_base, size, VM_PROT_READ,
	    wpipe->pipe_pages.ms, PIPENPAGES);
	PIPE_LOCK(wpipe);
	if (i < 0) {
		wpipe->pipe_state &= ~PIPE_DIRECTW;
		return (EFAULT);
	}

	wpipe->pipe_pages.npages = i;
	wpipe->pipe_pages.pos =
	    ((vm_offset_t) uio->uio_iov->iov_base) & PAGE_MASK;
	wpipe->pipe_pages.cnt = size;

	uio->uio_iov->iov_len -= size;
	uio->uio_iov->iov_base = (char *)uio->uio_iov->iov_base + size;
	if (uio->uio_iov->iov_len == 0) {
		uio->uio_iov++;
		uio->uio_iovcnt--;
	}
	uio->uio_resid -= size;
	uio->uio_offset += size;
	return (0);
}

/*
 * Unwire the process buffer.
 */
static void
pipe_destroy_write_buffer(struct pipe *wpipe)
{

	PIPE_LOCK_ASSERT(wpipe, MA_OWNED);
	KASSERT((wpipe->pipe_state & PIPE_DIRECTW) != 0,
	    ("%s: PIPE_DIRECTW not set on %p", __func__, wpipe));
	KASSERT(wpipe->pipe_pages.cnt == 0,
	    ("%s: pipe map for %p contains residual data", __func__, wpipe));

	wpipe->pipe_state &= ~PIPE_DIRECTW;
	vm_page_unhold_pages(wpipe->pipe_pages.ms, wpipe->pipe_pages.npages);
	wpipe->pipe_pages.npages = 0;
}

/*
 * In the case of a signal, the writing process might go away.  This
 * code copies the data into the circular buffer so that the source
 * pages can be freed without loss of data.
 */
static void
pipe_clone_write_buffer(struct pipe *wpipe)
{
	struct uio uio;
	struct iovec iov;
	int size;
	int pos;

	PIPE_LOCK_ASSERT(wpipe, MA_OWNED);
	KASSERT((wpipe->pipe_state & PIPE_DIRECTW) != 0,
	    ("%s: PIPE_DIRECTW not set on %p", __func__, wpipe));

	size = wpipe->pipe_pages.cnt;
	pos = wpipe->pipe_pages.pos;
	wpipe->pipe_pages.cnt = 0;

	wpipe->pipe_buffer.in = size;
	wpipe->pipe_buffer.out = 0;
	wpipe->pipe_buffer.cnt = size;

	PIPE_UNLOCK(wpipe);
	iov.iov_base = wpipe->pipe_buffer.buffer;
	iov.iov_len = size;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = size;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = curthread;
	uiomove_fromphys(wpipe->pipe_pages.ms, pos, size, &uio);
	PIPE_LOCK(wpipe);
	pipe_destroy_write_buffer(wpipe);
}

/*
 * This implements the pipe buffer write mechanism.  Note that only
 * a direct write OR a normal pipe write can be pending at any given time.
 * If there are any characters in the pipe buffer, the direct write will
 * be deferred until the receiving process grabs all of the bytes from
 * the pipe buffer.  Then the direct mapping write is set-up.
 */
static int
pipe_direct_write(struct pipe *wpipe, struct uio *uio)
{
	int error;

retry:
	PIPE_LOCK_ASSERT(wpipe, MA_OWNED);
	if ((wpipe->pipe_state & PIPE_EOF) != 0) {
		error = EPIPE;
		goto error1;
	}
	if (wpipe->pipe_state & PIPE_DIRECTW) {
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
		pipeselwakeup(wpipe);
		wpipe->pipe_state |= PIPE_WANTW;
		pipeunlock(wpipe);
		error = msleep(wpipe, PIPE_MTX(wpipe),
		    PRIBIO | PCATCH, "pipdww", 0);
		pipelock(wpipe, false);
		if (error != 0)
			goto error1;
		goto retry;
	}
	if (wpipe->pipe_buffer.cnt > 0) {
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
		pipeselwakeup(wpipe);
		wpipe->pipe_state |= PIPE_WANTW;
		pipeunlock(wpipe);
		error = msleep(wpipe, PIPE_MTX(wpipe),
		    PRIBIO | PCATCH, "pipdwc", 0);
		pipelock(wpipe, false);
		if (error != 0)
			goto error1;
		goto retry;
	}

	error = pipe_build_write_buffer(wpipe, uio);
	if (error) {
		goto error1;
	}

	while (wpipe->pipe_pages.cnt != 0 &&
	    (wpipe->pipe_state & PIPE_EOF) == 0) {
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
		pipeselwakeup(wpipe);
		wpipe->pipe_state |= PIPE_WANTW;
		pipeunlock(wpipe);
		error = msleep(wpipe, PIPE_MTX(wpipe), PRIBIO | PCATCH,
		    "pipdwt", 0);
		pipelock(wpipe, false);
		if (error != 0)
			break;
	}

	if ((wpipe->pipe_state & PIPE_EOF) != 0) {
		wpipe->pipe_pages.cnt = 0;
		pipe_destroy_write_buffer(wpipe);
		pipeselwakeup(wpipe);
		error = EPIPE;
	} else if (error == EINTR || error == ERESTART) {
		pipe_clone_write_buffer(wpipe);
	} else {
		pipe_destroy_write_buffer(wpipe);
	}
	KASSERT((wpipe->pipe_state & PIPE_DIRECTW) == 0,
	    ("pipe %p leaked PIPE_DIRECTW", wpipe));
	return (error);

error1:
	wakeup(wpipe);
	return (error);
}
#endif

static int
pipe_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{
	struct pipe *wpipe, *rpipe;
	ssize_t orig_resid;
	int desiredsize, error;

	rpipe = fp->f_data;
	wpipe = PIPE_PEER(rpipe);
	PIPE_LOCK(rpipe);
	error = pipelock(wpipe, true);
	if (error) {
		PIPE_UNLOCK(rpipe);
		return (error);
	}
	/*
	 * detect loss of pipe read side, issue SIGPIPE if lost.
	 */
	if (wpipe->pipe_present != PIPE_ACTIVE ||
	    (wpipe->pipe_state & PIPE_EOF)) {
		pipeunlock(wpipe);
		PIPE_UNLOCK(rpipe);
		return (EPIPE);
	}
#ifdef MAC
	error = mac_pipe_check_write(active_cred, wpipe->pipe_pair);
	if (error) {
		pipeunlock(wpipe);
		PIPE_UNLOCK(rpipe);
		return (error);
	}
#endif
	++wpipe->pipe_busy;

	/* Choose a larger size if it's advantageous */
	desiredsize = max(SMALL_PIPE_SIZE, wpipe->pipe_buffer.size);
	while (desiredsize < wpipe->pipe_buffer.cnt + uio->uio_resid) {
		if (piperesizeallowed != 1)
			break;
		if (amountpipekva > maxpipekva / 2)
			break;
		if (desiredsize == BIG_PIPE_SIZE)
			break;
		desiredsize = desiredsize * 2;
	}

	/* Choose a smaller size if we're in a OOM situation */
	if (amountpipekva > (3 * maxpipekva) / 4 &&
	    wpipe->pipe_buffer.size > SMALL_PIPE_SIZE &&
	    wpipe->pipe_buffer.cnt <= SMALL_PIPE_SIZE &&
	    piperesizeallowed == 1)
		desiredsize = SMALL_PIPE_SIZE;

	/* Resize if the above determined that a new size was necessary */
	if (desiredsize != wpipe->pipe_buffer.size &&
	    (wpipe->pipe_state & PIPE_DIRECTW) == 0) {
		PIPE_UNLOCK(wpipe);
		pipespace(wpipe, desiredsize);
		PIPE_LOCK(wpipe);
	}
	MPASS(wpipe->pipe_buffer.size != 0);

	orig_resid = uio->uio_resid;

	while (uio->uio_resid) {
		int space;

		if (wpipe->pipe_state & PIPE_EOF) {
			error = EPIPE;
			break;
		}
#ifndef PIPE_NODIRECT
		/*
		 * If the transfer is large, we can gain performance if
		 * we do process-to-process copies directly.
		 * If the write is non-blocking, we don't use the
		 * direct write mechanism.
		 *
		 * The direct write mechanism will detect the reader going
		 * away on us.
		 */
		if (uio->uio_segflg == UIO_USERSPACE &&
		    uio->uio_iov->iov_len >= pipe_mindirect &&
		    wpipe->pipe_buffer.size >= pipe_mindirect &&
		    (fp->f_flag & FNONBLOCK) == 0) {
			error = pipe_direct_write(wpipe, uio);
			if (error != 0)
				break;
			continue;
		}
#endif

		/*
		 * Pipe buffered writes cannot be coincidental with
		 * direct writes.  We wait until the currently executing
		 * direct write is completed before we start filling the
		 * pipe buffer.  We break out if a signal occurs or the
		 * reader goes away.
		 */
		if (wpipe->pipe_pages.cnt != 0) {
			if (wpipe->pipe_state & PIPE_WANTR) {
				wpipe->pipe_state &= ~PIPE_WANTR;
				wakeup(wpipe);
			}
			pipeselwakeup(wpipe);
			wpipe->pipe_state |= PIPE_WANTW;
			pipeunlock(wpipe);
			error = msleep(wpipe, PIPE_MTX(rpipe), PRIBIO | PCATCH,
			    "pipbww", 0);
			pipelock(wpipe, false);
			if (error != 0)
				break;
			continue;
		}

		space = wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt;

		/* Writes of size <= PIPE_BUF must be atomic. */
		if ((space < uio->uio_resid) && (orig_resid <= PIPE_BUF))
			space = 0;

		if (space > 0) {
			int size;	/* Transfer size */
			int segsize;	/* first segment to transfer */

			/*
			 * Transfer size is minimum of uio transfer
			 * and free space in pipe buffer.
			 */
			if (space > uio->uio_resid)
				size = uio->uio_resid;
			else
				size = space;
			/*
			 * First segment to transfer is minimum of
			 * transfer size and contiguous space in
			 * pipe buffer.  If first segment to transfer
			 * is less than the transfer size, we've got
			 * a wraparound in the buffer.
			 */
			segsize = wpipe->pipe_buffer.size -
				wpipe->pipe_buffer.in;
			if (segsize > size)
				segsize = size;

			/* Transfer first segment */

			PIPE_UNLOCK(rpipe);
			error = uiomove(&wpipe->pipe_buffer.buffer[wpipe->pipe_buffer.in],
					segsize, uio);
			PIPE_LOCK(rpipe);

			if (error == 0 && segsize < size) {
				KASSERT(wpipe->pipe_buffer.in + segsize ==
					wpipe->pipe_buffer.size,
					("Pipe buffer wraparound disappeared"));
				/*
				 * Transfer remaining part now, to
				 * support atomic writes.  Wraparound
				 * happened.
				 */

				PIPE_UNLOCK(rpipe);
				error = uiomove(
				    &wpipe->pipe_buffer.buffer[0],
				    size - segsize, uio);
				PIPE_LOCK(rpipe);
			}
			if (error == 0) {
				wpipe->pipe_buffer.in += size;
				if (wpipe->pipe_buffer.in >=
				    wpipe->pipe_buffer.size) {
					KASSERT(wpipe->pipe_buffer.in ==
						size - segsize +
						wpipe->pipe_buffer.size,
						("Expected wraparound bad"));
					wpipe->pipe_buffer.in = size - segsize;
				}

				wpipe->pipe_buffer.cnt += size;
				KASSERT(wpipe->pipe_buffer.cnt <=
					wpipe->pipe_buffer.size,
					("Pipe buffer overflow"));
			}
			if (error != 0)
				break;
			continue;
		} else {
			/*
			 * If the "read-side" has been blocked, wake it up now.
			 */
			if (wpipe->pipe_state & PIPE_WANTR) {
				wpipe->pipe_state &= ~PIPE_WANTR;
				wakeup(wpipe);
			}

			/*
			 * don't block on non-blocking I/O
			 */
			if (fp->f_flag & FNONBLOCK) {
				error = EAGAIN;
				break;
			}

			/*
			 * We have no more space and have something to offer,
			 * wake up select/poll.
			 */
			pipeselwakeup(wpipe);

			wpipe->pipe_state |= PIPE_WANTW;
			pipeunlock(wpipe);
			error = msleep(wpipe, PIPE_MTX(rpipe),
			    PRIBIO | PCATCH, "pipewr", 0);
			pipelock(wpipe, false);
			if (error != 0)
				break;
			continue;
		}
	}

	--wpipe->pipe_busy;

	if ((wpipe->pipe_busy == 0) && (wpipe->pipe_state & PIPE_WANT)) {
		wpipe->pipe_state &= ~(PIPE_WANT | PIPE_WANTR);
		wakeup(wpipe);
	} else if (wpipe->pipe_buffer.cnt > 0) {
		/*
		 * If we have put any characters in the buffer, we wake up
		 * the reader.
		 */
		if (wpipe->pipe_state & PIPE_WANTR) {
			wpipe->pipe_state &= ~PIPE_WANTR;
			wakeup(wpipe);
		}
	}

	/*
	 * Don't return EPIPE if any byte was written.
	 * EINTR and other interrupts are handled by generic I/O layer.
	 * Do not pretend that I/O succeeded for obvious user error
	 * like EFAULT.
	 */
	if (uio->uio_resid != orig_resid && error == EPIPE)
		error = 0;

	if (error == 0)
		pipe_timestamp(&wpipe->pipe_mtime);

	/*
	 * We have something to offer,
	 * wake up select/poll.
	 */
	if (wpipe->pipe_buffer.cnt)
		pipeselwakeup(wpipe);

	pipeunlock(wpipe);
	PIPE_UNLOCK(rpipe);
	if (uio->uio_resid != orig_resid)
		td->td_ru.ru_msgsnd++;
	return (error);
}

/* ARGSUSED */
static int
pipe_truncate(struct file *fp, off_t length, struct ucred *active_cred,
    struct thread *td)
{
	struct pipe *cpipe;
	int error;

	cpipe = fp->f_data;
	if (cpipe->pipe_type & PIPE_TYPE_NAMED)
		error = vnops.fo_truncate(fp, length, active_cred, td);
	else
		error = invfo_truncate(fp, length, active_cred, td);
	return (error);
}

/*
 * we implement a very minimal set of ioctls for compatibility with sockets.
 */
static int
pipe_ioctl(struct file *fp, u_long cmd, void *data, struct ucred *active_cred,
    struct thread *td)
{
	struct pipe *mpipe = fp->f_data;
	int error;

	PIPE_LOCK(mpipe);

#ifdef MAC
	error = mac_pipe_check_ioctl(active_cred, mpipe->pipe_pair, cmd, data);
	if (error) {
		PIPE_UNLOCK(mpipe);
		return (error);
	}
#endif

	error = 0;
	switch (cmd) {
	case FIONBIO:
		break;

	case FIOASYNC:
		if (*(int *)data) {
			mpipe->pipe_state |= PIPE_ASYNC;
		} else {
			mpipe->pipe_state &= ~PIPE_ASYNC;
		}
		break;

	case FIONREAD:
		if (!(fp->f_flag & FREAD)) {
			*(int *)data = 0;
			PIPE_UNLOCK(mpipe);
			return (0);
		}
		if (mpipe->pipe_pages.cnt != 0)
			*(int *)data = mpipe->pipe_pages.cnt;
		else
			*(int *)data = mpipe->pipe_buffer.cnt;
		break;

	case FIOSETOWN:
		PIPE_UNLOCK(mpipe);
		error = fsetown(*(int *)data, &mpipe->pipe_sigio);
		goto out_unlocked;

	case FIOGETOWN:
		*(int *)data = fgetown(&mpipe->pipe_sigio);
		break;

	/* This is deprecated, FIOSETOWN should be used instead. */
	case TIOCSPGRP:
		PIPE_UNLOCK(mpipe);
		error = fsetown(-(*(int *)data), &mpipe->pipe_sigio);
		goto out_unlocked;

	/* This is deprecated, FIOGETOWN should be used instead. */
	case TIOCGPGRP:
		*(int *)data = -fgetown(&mpipe->pipe_sigio);
		break;

	default:
		error = ENOTTY;
		break;
	}
	PIPE_UNLOCK(mpipe);
out_unlocked:
	return (error);
}

static int
pipe_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{
	struct pipe *rpipe;
	struct pipe *wpipe;
	int levents, revents;
#ifdef MAC
	int error;
#endif

	revents = 0;
	rpipe = fp->f_data;
	wpipe = PIPE_PEER(rpipe);
	PIPE_LOCK(rpipe);
#ifdef MAC
	error = mac_pipe_check_poll(active_cred, rpipe->pipe_pair);
	if (error)
		goto locked_error;
#endif
	if (fp->f_flag & FREAD && events & (POLLIN | POLLRDNORM))
		if (rpipe->pipe_pages.cnt > 0 || rpipe->pipe_buffer.cnt > 0)
			revents |= events & (POLLIN | POLLRDNORM);

	if (fp->f_flag & FWRITE && events & (POLLOUT | POLLWRNORM))
		if (wpipe->pipe_present != PIPE_ACTIVE ||
		    (wpipe->pipe_state & PIPE_EOF) ||
		    ((wpipe->pipe_state & PIPE_DIRECTW) == 0 &&
		     ((wpipe->pipe_buffer.size - wpipe->pipe_buffer.cnt) >= PIPE_BUF ||
			 wpipe->pipe_buffer.size == 0)))
			revents |= events & (POLLOUT | POLLWRNORM);

	levents = events &
	    (POLLIN | POLLINIGNEOF | POLLPRI | POLLRDNORM | POLLRDBAND);
	if (rpipe->pipe_type & PIPE_TYPE_NAMED && fp->f_flag & FREAD && levents &&
	    fp->f_pipegen == rpipe->pipe_wgen)
		events |= POLLINIGNEOF;

	if ((events & POLLINIGNEOF) == 0) {
		if (rpipe->pipe_state & PIPE_EOF) {
			if (fp->f_flag & FREAD)
				revents |= (events & (POLLIN | POLLRDNORM));
			if (wpipe->pipe_present != PIPE_ACTIVE ||
			    (wpipe->pipe_state & PIPE_EOF))
				revents |= POLLHUP;
		}
	}

	if (revents == 0) {
		/*
		 * Add ourselves regardless of eventmask as we have to return
		 * POLLHUP even if it was not asked for.
		 */
		if ((fp->f_flag & FREAD) != 0) {
			selrecord(td, &rpipe->pipe_sel);
			if (SEL_WAITING(&rpipe->pipe_sel))
				rpipe->pipe_state |= PIPE_SEL;
		}

		if ((fp->f_flag & FWRITE) != 0 &&
		    wpipe->pipe_present == PIPE_ACTIVE) {
			selrecord(td, &wpipe->pipe_sel);
			if (SEL_WAITING(&wpipe->pipe_sel))
				wpipe->pipe_state |= PIPE_SEL;
		}
	}
#ifdef MAC
locked_error:
#endif
	PIPE_UNLOCK(rpipe);

	return (revents);
}

/*
 * We shouldn't need locks here as we're doing a read and this should
 * be a natural race.
 */
static int
pipe_stat(struct file *fp, struct stat *ub, struct ucred *active_cred)
{
	struct pipe *pipe;
#ifdef MAC
	int error;
#endif

	pipe = fp->f_data;
#ifdef MAC
	if (mac_pipe_check_stat_enabled()) {
		PIPE_LOCK(pipe);
		error = mac_pipe_check_stat(active_cred, pipe->pipe_pair);
		PIPE_UNLOCK(pipe);
		if (error) {
			return (error);
		}
	}
#endif

	/* For named pipes ask the underlying filesystem. */
	if (pipe->pipe_type & PIPE_TYPE_NAMED) {
		return (vnops.fo_stat(fp, ub, active_cred));
	}

	bzero(ub, sizeof(*ub));
	ub->st_mode = S_IFIFO;
	ub->st_blksize = PAGE_SIZE;
	if (pipe->pipe_pages.cnt != 0)
		ub->st_size = pipe->pipe_pages.cnt;
	else
		ub->st_size = pipe->pipe_buffer.cnt;
	ub->st_blocks = howmany(ub->st_size, ub->st_blksize);
	ub->st_atim = pipe->pipe_atime;
	ub->st_mtim = pipe->pipe_mtime;
	ub->st_ctim = pipe->pipe_ctime;
	ub->st_uid = fp->f_cred->cr_uid;
	ub->st_gid = fp->f_cred->cr_gid;
	ub->st_dev = pipedev_ino;
	ub->st_ino = pipe->pipe_ino;
	/*
	 * Left as 0: st_nlink, st_rdev, st_flags, st_gen.
	 */
	return (0);
}

/* ARGSUSED */
static int
pipe_close(struct file *fp, struct thread *td)
{

	if (fp->f_vnode != NULL) 
		return vnops.fo_close(fp, td);
	fp->f_ops = &badfileops;
	pipe_dtor(fp->f_data);
	fp->f_data = NULL;
	return (0);
}

static int
pipe_chmod(struct file *fp, mode_t mode, struct ucred *active_cred, struct thread *td)
{
	struct pipe *cpipe;
	int error;

	cpipe = fp->f_data;
	if (cpipe->pipe_type & PIPE_TYPE_NAMED)
		error = vn_chmod(fp, mode, active_cred, td);
	else
		error = invfo_chmod(fp, mode, active_cred, td);
	return (error);
}

static int
pipe_chown(struct file *fp, uid_t uid, gid_t gid, struct ucred *active_cred,
    struct thread *td)
{
	struct pipe *cpipe;
	int error;

	cpipe = fp->f_data;
	if (cpipe->pipe_type & PIPE_TYPE_NAMED)
		error = vn_chown(fp, uid, gid, active_cred, td);
	else
		error = invfo_chown(fp, uid, gid, active_cred, td);
	return (error);
}

static int
pipe_fill_kinfo(struct file *fp, struct kinfo_file *kif, struct filedesc *fdp)
{
	struct pipe *pi;

	if (fp->f_type == DTYPE_FIFO)
		return (vn_fill_kinfo(fp, kif, fdp));
	kif->kf_type = KF_TYPE_PIPE;
	pi = fp->f_data;
	kif->kf_un.kf_pipe.kf_pipe_addr = (uintptr_t)pi;
	kif->kf_un.kf_pipe.kf_pipe_peer = (uintptr_t)pi->pipe_peer;
	kif->kf_un.kf_pipe.kf_pipe_buffer_cnt = pi->pipe_buffer.cnt;
	kif->kf_un.kf_pipe.kf_pipe_buffer_in = pi->pipe_buffer.in;
	kif->kf_un.kf_pipe.kf_pipe_buffer_out = pi->pipe_buffer.out;
	kif->kf_un.kf_pipe.kf_pipe_buffer_size = pi->pipe_buffer.size;
	return (0);
}

static void
pipe_free_kmem(struct pipe *cpipe)
{

	KASSERT(!mtx_owned(PIPE_MTX(cpipe)),
	    ("pipe_free_kmem: pipe mutex locked"));

	if (cpipe->pipe_buffer.buffer != NULL) {
		atomic_subtract_long(&amountpipekva, cpipe->pipe_buffer.size);
		chgpipecnt(cpipe->pipe_pair->pp_owner->cr_ruidinfo,
		    -cpipe->pipe_buffer.size, 0);
		vm_map_remove(pipe_map,
		    (vm_offset_t)cpipe->pipe_buffer.buffer,
		    (vm_offset_t)cpipe->pipe_buffer.buffer + cpipe->pipe_buffer.size);
		cpipe->pipe_buffer.buffer = NULL;
	}
#ifndef PIPE_NODIRECT
	{
		cpipe->pipe_pages.cnt = 0;
		cpipe->pipe_pages.pos = 0;
		cpipe->pipe_pages.npages = 0;
	}
#endif
}

/*
 * shutdown the pipe
 */
static void
pipeclose(struct pipe *cpipe)
{
#ifdef MAC
	struct pipepair *pp;
#endif
	struct pipe *ppipe;

	KASSERT(cpipe != NULL, ("pipeclose: cpipe == NULL"));

	PIPE_LOCK(cpipe);
	pipelock(cpipe, false);
#ifdef MAC
	pp = cpipe->pipe_pair;
#endif

	/*
	 * If the other side is blocked, wake it up saying that
	 * we want to close it down.
	 */
	cpipe->pipe_state |= PIPE_EOF;
	while (cpipe->pipe_busy) {
		wakeup(cpipe);
		cpipe->pipe_state |= PIPE_WANT;
		pipeunlock(cpipe);
		msleep(cpipe, PIPE_MTX(cpipe), PRIBIO, "pipecl", 0);
		pipelock(cpipe, false);
	}

	pipeselwakeup(cpipe);

	/*
	 * Disconnect from peer, if any.
	 */
	ppipe = cpipe->pipe_peer;
	if (ppipe->pipe_present == PIPE_ACTIVE) {
		ppipe->pipe_state |= PIPE_EOF;
		wakeup(ppipe);
		pipeselwakeup(ppipe);
	}

	/*
	 * Mark this endpoint as free.  Release kmem resources.  We
	 * don't mark this endpoint as unused until we've finished
	 * doing that, or the pipe might disappear out from under
	 * us.
	 */
	PIPE_UNLOCK(cpipe);
	pipe_free_kmem(cpipe);
	PIPE_LOCK(cpipe);
	cpipe->pipe_present = PIPE_CLOSING;
	pipeunlock(cpipe);

	/*
	 * knlist_clear() may sleep dropping the PIPE_MTX. Set the
	 * PIPE_FINALIZED, that allows other end to free the
	 * pipe_pair, only after the knotes are completely dismantled.
	 */
	knlist_clear(&cpipe->pipe_sel.si_note, 1);
	cpipe->pipe_present = PIPE_FINALIZED;
	seldrain(&cpipe->pipe_sel);
	knlist_destroy(&cpipe->pipe_sel.si_note);

	/*
	 * If both endpoints are now closed, release the memory for the
	 * pipe pair.  If not, unlock.
	 */
	if (ppipe->pipe_present == PIPE_FINALIZED) {
		PIPE_UNLOCK(cpipe);
		crfree(cpipe->pipe_pair->pp_owner);
#ifdef MAC
		mac_pipe_destroy(pp);
#endif
		uma_zfree(pipe_zone, cpipe->pipe_pair);
	} else
		PIPE_UNLOCK(cpipe);
}

/*ARGSUSED*/
static int
pipe_kqfilter(struct file *fp, struct knote *kn)
{
	struct pipe *cpipe;

	/*
	 * If a filter is requested that is not supported by this file
	 * descriptor, don't return an error, but also don't ever generate an
	 * event.
	 */
	if ((kn->kn_filter == EVFILT_READ) && !(fp->f_flag & FREAD)) {
		kn->kn_fop = &pipe_nfiltops;
		return (0);
	}
	if ((kn->kn_filter == EVFILT_WRITE) && !(fp->f_flag & FWRITE)) {
		kn->kn_fop = &pipe_nfiltops;
		return (0);
	}
	cpipe = fp->f_data;
	PIPE_LOCK(cpipe);
	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &pipe_rfiltops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &pipe_wfiltops;
		if (cpipe->pipe_peer->pipe_present != PIPE_ACTIVE) {
			/* other end of pipe has been closed */
			PIPE_UNLOCK(cpipe);
			return (EPIPE);
		}
		cpipe = PIPE_PEER(cpipe);
		break;
	default:
		if ((cpipe->pipe_type & PIPE_TYPE_NAMED) != 0) {
			PIPE_UNLOCK(cpipe);
			return (vnops.fo_kqfilter(fp, kn));
		}
		PIPE_UNLOCK(cpipe);
		return (EINVAL);
	}

	kn->kn_hook = cpipe; 
	knlist_add(&cpipe->pipe_sel.si_note, kn, 1);
	PIPE_UNLOCK(cpipe);
	return (0);
}

static void
filt_pipedetach(struct knote *kn)
{
	struct pipe *cpipe = kn->kn_hook;

	PIPE_LOCK(cpipe);
	knlist_remove(&cpipe->pipe_sel.si_note, kn, 1);
	PIPE_UNLOCK(cpipe);
}

/*ARGSUSED*/
static int
filt_piperead(struct knote *kn, long hint)
{
	struct file *fp = kn->kn_fp;
	struct pipe *rpipe = kn->kn_hook;

	PIPE_LOCK_ASSERT(rpipe, MA_OWNED);
	kn->kn_data = rpipe->pipe_buffer.cnt;
	if (kn->kn_data == 0)
		kn->kn_data = rpipe->pipe_pages.cnt;

	if ((rpipe->pipe_state & PIPE_EOF) != 0 &&
	    ((rpipe->pipe_type & PIPE_TYPE_NAMED) == 0 ||
	    fp->f_pipegen != rpipe->pipe_wgen)) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	kn->kn_flags &= ~EV_EOF;
	return (kn->kn_data > 0);
}

/*ARGSUSED*/
static int
filt_pipewrite(struct knote *kn, long hint)
{
	struct pipe *wpipe = kn->kn_hook;

	/*
	 * If this end of the pipe is closed, the knote was removed from the
	 * knlist and the list lock (i.e., the pipe lock) is therefore not held.
	 */
	if (wpipe->pipe_present == PIPE_ACTIVE ||
	    (wpipe->pipe_type & PIPE_TYPE_NAMED) != 0) {
		PIPE_LOCK_ASSERT(wpipe, MA_OWNED);

		if (wpipe->pipe_state & PIPE_DIRECTW) {
			kn->kn_data = 0;
		} else if (wpipe->pipe_buffer.size > 0) {
			kn->kn_data = wpipe->pipe_buffer.size -
			    wpipe->pipe_buffer.cnt;
		} else {
			kn->kn_data = PIPE_BUF;
		}
	}

	if (wpipe->pipe_present != PIPE_ACTIVE ||
	    (wpipe->pipe_state & PIPE_EOF)) {
		kn->kn_flags |= EV_EOF;
		return (1);
	}
	kn->kn_flags &= ~EV_EOF;
	return (kn->kn_data >= PIPE_BUF);
}

static void
filt_pipedetach_notsup(struct knote *kn)
{

}

static int
filt_pipenotsup(struct knote *kn, long hint)
{

	return (0);
}

static int
filt_pipedump(struct proc *p, struct knote *kn,
    struct kinfo_knote *kin)
{
	struct pipe *pipe = kn->kn_hook;

	kin->knt_extdata = KNOTE_EXTDATA_PIPE;
	kin->knt_pipe.knt_pipe_ino = pipe->pipe_ino;
	return (0);
}
