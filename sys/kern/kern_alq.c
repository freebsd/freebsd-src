/*-
 * Copyright (c) 2002, Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2008-2009, Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/alq.h>
#include <sys/malloc.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/eventhandler.h>

#if (__FreeBSD_version >= 700000)
#include <security/mac/mac_framework.h>
#endif

/* Async. Logging Queue */
struct alq {
	int	aq_entmax;		/* Max entries */
	int	aq_entlen;		/* Entry length */
	int	aq_freebytes;		/* Bytes available in buffer */
	int	aq_buflen;		/* Total length of our buffer */
	char	*aq_entbuf;		/* Buffer for stored entries */
	int	aq_writehead;
	int	aq_writetail;
	int	aq_flags;		/* Queue flags */
	struct mtx	aq_mtx;		/* Queue lock */
	struct vnode	*aq_vp;		/* Open vnode handle */
	struct ucred	*aq_cred;	/* Credentials of the opening thread */
	LIST_ENTRY(alq)	aq_act;		/* List of active queues */
	LIST_ENTRY(alq)	aq_link;	/* List of all queues */
};

#define	AQ_WANTED	0x0001		/* Wakeup sleeper when io is done */
#define	AQ_ACTIVE	0x0002		/* on the active list */
#define	AQ_FLUSHING	0x0004		/* doing IO */
#define	AQ_SHUTDOWN	0x0008		/* Queue no longer valid */

#define	ALQ_LOCK(alq)	mtx_lock_spin(&(alq)->aq_mtx)
#define	ALQ_UNLOCK(alq)	mtx_unlock_spin(&(alq)->aq_mtx)

#define ALQ_HAS_PENDING_DATA(alq) ((alq)->aq_freebytes != (alq)->aq_buflen)

static MALLOC_DEFINE(M_ALD, "ALD", "ALD");

/*
 * The ald_mtx protects the ald_queues list and the ald_active list.
 */
static struct mtx ald_mtx;
static LIST_HEAD(, alq) ald_queues;
static LIST_HEAD(, alq) ald_active;
static int ald_shutingdown = 0;
static struct proc *ald_proc;

#define	ALD_LOCK()	mtx_lock(&ald_mtx)
#define	ALD_UNLOCK()	mtx_unlock(&ald_mtx)

/* Daemon functions */
static int ald_add(struct alq *);
static int ald_rem(struct alq *);
static void ald_startup(void *);
static void ald_daemon(void);
static void ald_shutdown(void *, int);
static void ald_activate(struct alq *);
static void ald_deactivate(struct alq *);

/* Internal queue functions */
static void alq_shutdown(struct alq *);
static int alq_doio(struct alq *);


/*
 * Add a new queue to the global list.  Fail if we're shutting down.
 */
static int
ald_add(struct alq *alq)
{
	int error;

	error = 0;

	ALD_LOCK();
	if (ald_shutingdown) {
		error = EBUSY;
		goto done;
	}
	LIST_INSERT_HEAD(&ald_queues, alq, aq_link);
done:
	ALD_UNLOCK();
	return (error);
}

/*
 * Remove a queue from the global list unless we're shutting down.  If so,
 * the ald will take care of cleaning up it's resources.
 */
static int
ald_rem(struct alq *alq)
{
	int error;

	error = 0;

	ALD_LOCK();
	if (ald_shutingdown) {
		error = EBUSY;
		goto done;
	}
	LIST_REMOVE(alq, aq_link);
done:
	ALD_UNLOCK();
	return (error);
}

/*
 * Put a queue on the active list.  This will schedule it for writing.
 */
static void
ald_activate(struct alq *alq)
{
	LIST_INSERT_HEAD(&ald_active, alq, aq_act);
	wakeup(&ald_active);
}

static void
ald_deactivate(struct alq *alq)
{
	LIST_REMOVE(alq, aq_act);
	alq->aq_flags &= ~AQ_ACTIVE;
}

static void
ald_startup(void *unused)
{
	mtx_init(&ald_mtx, "ALDmtx", NULL, MTX_DEF|MTX_QUIET);
	LIST_INIT(&ald_queues);
	LIST_INIT(&ald_active);
}

static void
ald_daemon(void)
{
	int needwakeup;
	struct alq *alq;

	EVENTHANDLER_REGISTER(shutdown_pre_sync, ald_shutdown, NULL,
	    SHUTDOWN_PRI_FIRST);

	ALD_LOCK();

	for (;;) {
		while ((alq = LIST_FIRST(&ald_active)) == NULL
		    && !ald_shutingdown)
#if (__FreeBSD_version >= 700000)
			mtx_sleep(&ald_active, &ald_mtx, PWAIT, "aldslp", 0);
#else
			msleep(&ald_active, &ald_mtx, PWAIT, "aldslp", 0);
#endif

		/* Don't shutdown until all active alq's are flushed */
		if (ald_shutingdown && alq == NULL) {
			ALD_UNLOCK();
			break;
		}

		ALQ_LOCK(alq);
		ald_deactivate(alq);
		ALD_UNLOCK();
		needwakeup = alq_doio(alq);
		ALQ_UNLOCK(alq);
		if (needwakeup)
			wakeup_one(alq);
		ALD_LOCK();
	}

#if (__FreeBSD_version < 800000)
#if (__FreeBSD_version < 700000)
	wakeup(ald_proc);
#endif
	kthread_exit(0);
#else
	kproc_exit(0);
#endif
}

static void
ald_shutdown(void *arg, int howto)
{
	struct alq *alq;

	ALD_LOCK();

	/* Ensure no new queues can be created */
	ald_shutingdown = 1;

	/* Shutdown all alqs prior to terminating the ald_daemon */
	while ((alq = LIST_FIRST(&ald_queues)) != NULL) {
		LIST_REMOVE(alq, aq_link);
		ALD_UNLOCK();
		alq_shutdown(alq);
		ALD_LOCK();
	}

	/* At this point, all alqs are flushed and shutdown */

	/*
	 * Wake ald_daemon so that it exits. It won't be able to do
	 * anything until we mtx_sleep because we hold the ald_mtx
	 */
	wakeup(&ald_active);

	/* Wait for ald_daemon to exit */
#if (__FreeBSD_version >= 700000)
	mtx_sleep(ald_proc, &ald_mtx, PWAIT, "aldslp", 0);
#else
	msleep(ald_proc, &ald_mtx, PWAIT, "aldslp", 0);
#endif
	ALD_UNLOCK();
}

static void
alq_shutdown(struct alq *alq)
{
	ALQ_LOCK(alq);

	/* Stop any new writers. */
	alq->aq_flags |= AQ_SHUTDOWN;

	/*
	 * If the alq isn't active but has unwritten data (possible if
	 * the ALQ_NOACTIVATE flag has been used), explicitly activate the
	 * alq here so that the pending data gets flushed by the ald_daemon.
	 */
	if (!(alq->aq_flags & AQ_ACTIVE) &&
	    ALQ_HAS_PENDING_DATA(alq)) {
		alq->aq_flags |= AQ_ACTIVE;
		ALQ_UNLOCK(alq);
		ALD_LOCK();
		ald_activate(alq);
		ALD_UNLOCK();
		ALQ_LOCK(alq);
	}

	/* Drain IO */
	while (alq->aq_flags & AQ_ACTIVE) {
		alq->aq_flags |= AQ_WANTED;
		msleep_spin(alq, &alq->aq_mtx, "aldclose", 0);
	}

	ALQ_UNLOCK(alq);

	vn_close(alq->aq_vp, FWRITE, alq->aq_cred, curthread);
	crfree(alq->aq_cred);
}

/*
 * Flush all pending data to disk.  This operation will block.
 */
static int
alq_doio(struct alq *alq)
{
	struct thread *td;
	struct mount *mp;
	struct vnode *vp;
	struct uio auio;
	struct iovec aiov[2];
	int totlen;
	int iov;
	int vfslocked;

	KASSERT((ALQ_HAS_PENDING_DATA(alq)),
		("%s: queue emtpy!", __func__)
	);

	vp = alq->aq_vp;
	td = curthread;
	totlen = 0;
	iov = 1;

	bzero(&aiov, sizeof(aiov));
	bzero(&auio, sizeof(auio));

	/* Start the write from the location of our buffer tail pointer. */
	aiov[0].iov_base = alq->aq_entbuf + alq->aq_writetail;

	if (alq->aq_writetail < alq->aq_writehead) {
		/* Buffer not wrapped */
		totlen = aiov[0].iov_len = alq->aq_writehead - alq->aq_writetail;
	} else if (alq->aq_writehead == 0) {
		/* Buffer not wrapped (special case to avoid an empty iov) */
		totlen = aiov[0].iov_len = alq->aq_buflen - alq->aq_writetail;;
	} else {
		/*
		 * Buffer wrapped, requires 2 aiov entries:
		 * - first is from writetail to end of buffer
		 * - second is from start of buffer to writehead
		 */
		aiov[0].iov_len = alq->aq_buflen - alq->aq_writetail;
		iov++;
		aiov[1].iov_base = alq->aq_entbuf;
		aiov[1].iov_len =  alq->aq_writehead;
		totlen = aiov[0].iov_len + aiov[1].iov_len;
	}

	alq->aq_flags |= AQ_FLUSHING;
	ALQ_UNLOCK(alq);

	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_iovcnt = iov;
	auio.uio_resid = totlen;
	auio.uio_td = td;

	/*
	 * Do all of the junk required to write now.
	 */
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vn_start_write(vp, &mp, V_WAIT);
#if (__FreeBSD_version < 800000)
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
#else
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
#endif

	/*
	 * XXX: VOP_WRITE error checks are ignored.
	 */
#ifdef MAC
	if (mac_vnode_check_write(alq->aq_cred, NOCRED, vp) == 0)
#endif
		VOP_WRITE(vp, &auio, IO_UNIT | IO_APPEND, alq->aq_cred);
#if (__FreeBSD_version < 800000)
	VOP_UNLOCK(vp, 0, td);
#else
	VOP_UNLOCK(vp, 0);
#endif
	vn_finished_write(mp);
	VFS_UNLOCK_GIANT(vfslocked);

	ALQ_LOCK(alq);
	alq->aq_flags &= ~AQ_FLUSHING;

	/* Adjust writetail as required, taking into account wrapping. */
	alq->aq_writetail = (alq->aq_writetail + totlen) % alq->aq_buflen;
	alq->aq_freebytes += totlen;

	/*
	 * If we just flushed the buffer completely,
	 * reset indexes to 0 to minimise buffer wraps.
	 * This is also required to ensure alq_getn() can't wedge itself.
	 */
	if (!ALQ_HAS_PENDING_DATA(alq))
		alq->aq_writehead = alq->aq_writetail = 0;

	KASSERT((alq->aq_writetail >= 0 && alq->aq_writetail < alq->aq_buflen),
		("%s: aq_writetail < 0 || aq_writetail >= aq_buflen", __func__)
	);

	if (alq->aq_flags & AQ_WANTED) {
		alq->aq_flags &= ~AQ_WANTED;
		return (1);
	}

	return(0);
}

static struct kproc_desc ald_kp = {
        "ALQ Daemon",
        ald_daemon,
        &ald_proc
};

SYSINIT(aldthread, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, kproc_start, &ald_kp);
SYSINIT(ald, SI_SUB_LOCK, SI_ORDER_ANY, ald_startup, NULL);


/* User visible queue functions */

/*
 * Create the queue data structure, allocate the buffer, and open the file.
 */
int
alq_open(struct alq **alqp, const char *file, struct ucred *cred, int cmode,
    int size, int count)
{
	struct thread *td;
	struct nameidata nd;
	struct alq *alq;
	int flags;
	int error;
	int vfslocked;

	KASSERT((size > 0), ("%s: size <= 0", __func__));
	KASSERT((count >= 0), ("%s: count < 0", __func__));

	*alqp = NULL;
	td = curthread;

	NDINIT(&nd, LOOKUP, NOFOLLOW | MPSAFE, UIO_SYSSPACE, file, td);
	flags = FWRITE | O_NOFOLLOW | O_CREAT;

#if (__FreeBSD_version < 700000)
	error = vn_open_cred(&nd, &flags, cmode, cred, 0);
#elif (__FreeBSD_version < 800098)
	error = vn_open_cred(&nd, &flags, cmode, cred, NULL);
#else
	error = vn_open_cred(&nd, &flags, cmode, 0, cred, NULL);
#endif
	if (error)
		return (error);

	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	/* We just unlock so we hold a reference. */
#if (__FreeBSD_version < 800000)
	VOP_UNLOCK(nd.ni_vp, 0, td);
#else
	VOP_UNLOCK(nd.ni_vp, 0);
#endif
	VFS_UNLOCK_GIANT(vfslocked);

	alq = malloc(sizeof(*alq), M_ALD, M_WAITOK|M_ZERO);
	alq->aq_vp = nd.ni_vp;
	alq->aq_cred = crhold(cred);

	mtx_init(&alq->aq_mtx, "ALD Queue", NULL, MTX_SPIN|MTX_QUIET);

	if (count > 0) {
		/* Fixed length messages. */
		alq->aq_buflen = size * count;
		alq->aq_entmax = count;
		alq->aq_entlen = size;
	} else {
		/* Variable length messages. */
		alq->aq_buflen = size;
		alq->aq_entmax = 0;
		alq->aq_entlen = 0;
	}

	alq->aq_freebytes = alq->aq_buflen;
	alq->aq_entbuf = malloc(alq->aq_buflen, M_ALD, M_WAITOK|M_ZERO);

	alq->aq_writehead = alq->aq_writetail = 0;

	if ((error = ald_add(alq)) != 0)
		return (error);
	*alqp = alq;

	return (0);
}

/*
 * Copy a new entry into the queue.  If the operation would block either
 * wait or return an error depending on the value of waitok.
 */
int
alq_write(struct alq *alq, void *data, int flags)
{
	/* Should only be called in fixed length message (legacy) mode. */
	KASSERT((alq->aq_entmax > 0 && alq->aq_entlen > 0),
		("%s: fixed length write on variable length queue", __func__)
	);
	return (alq_writen(alq, data, alq->aq_entlen, flags));
}

int
alq_writen(struct alq *alq, void *data, int len, int flags)
{
	int activate = 0;
	int copy = len;

	KASSERT((len > 0 && len <= alq->aq_buflen),
		("%s: len <= 0 || len > aq_buflen", __func__)
	);

	ALQ_LOCK(alq);

	/*
	 * If the message is larger than our underlying buffer or
	 * there is not enough free space in our underlying buffer
	 * to accept the message and the user can't wait, return.
	 */
	if ((len > alq->aq_buflen) ||
	    ((flags & ALQ_NOWAIT) && (alq->aq_freebytes < len))) {
		ALQ_UNLOCK(alq);
		return (EWOULDBLOCK);
	}

	/*
	 * ALQ_WAITOK or alq->aq_freebytes > len, either spin until
	 * we have enough free bytes (former) or skip (latter).
	 */
	while (alq->aq_freebytes < len && (alq->aq_flags & AQ_SHUTDOWN) == 0) {
		alq->aq_flags |= AQ_WANTED;
		msleep_spin(alq, &alq->aq_mtx, "alqwriten", 0);
	}

	/*
	 * We need to serialise wakups to ensure records remain in order...
	 * Therefore, wakeup the next thread in the queue waiting for
	 * alq resources to be available.
	 * (technically this is only required if we actually entered the above
	 * while loop)
	 */
	wakeup_one(alq);

	/* Bail if we're shutting down. */
	if (alq->aq_flags & AQ_SHUTDOWN) {
		ALQ_UNLOCK(alq);
		return (EWOULDBLOCK);
	}

	/*
	 * If we need to wrap the buffer to accommodate the write,
	 * we'll need 2 calls to bcopy.
	 */
	if ((alq->aq_buflen - alq->aq_writehead) < len)
		copy = alq->aq_buflen - alq->aq_writehead;

	/* Copy message (or part thereof if wrap required) to the buffer. */
	bcopy(data, alq->aq_entbuf + alq->aq_writehead, copy);
	alq->aq_writehead += copy;

	if (alq->aq_writehead >= alq->aq_buflen) {
		KASSERT((alq->aq_writehead == alq->aq_buflen),
		    ("alq->aq_writehead (%d) > alq->aq_buflen (%d)",
		    alq->aq_writehead,
		    alq->aq_buflen)
		);
		alq->aq_writehead = 0;
	}

	if (copy != len) {
		/*
		 * Wrap the buffer by copying the remainder of our message
		 * to the start of the buffer and resetting aq_writehead.
		 */
		bcopy(((uint8_t *)data)+copy, alq->aq_entbuf, len - copy);
		alq->aq_writehead = len - copy;
	}

	KASSERT((alq->aq_writehead >= 0 && alq->aq_writehead < alq->aq_buflen),
		("%s: aq_writehead < 0 || aq_writehead >= aq_buflen", __func__)
	);

	alq->aq_freebytes -= len;

	if (((alq->aq_flags & AQ_ACTIVE) == 0) &&
		((flags & ALQ_NOACTIVATE) == 0)) {
		alq->aq_flags |= AQ_ACTIVE;
		activate = 1;
	}

	ALQ_UNLOCK(alq);

	if (activate) {
		ALD_LOCK();
		ald_activate(alq);
		ALD_UNLOCK();
	}

	return (0);
}

struct ale *
alq_get(struct alq *alq, int flags)
{
	/* Should only be called in fixed length message (legacy) mode. */
	KASSERT((alq->aq_entmax > 0 && alq->aq_entlen > 0),
		("%s: fixed length get on variable length queue", __func__)
	);
	return (alq_getn(alq, alq->aq_entlen, flags));
}

struct ale *
alq_getn(struct alq *alq, int len, int flags)
{
	struct ale *ale;
	int contigbytes;

	KASSERT((len > 0 && len <= alq->aq_buflen),
		("%s: len <= 0 || len > alq->aq_buflen", __func__)
	);

	ale = malloc(	sizeof(struct ale),
			M_ALD,
			(flags & ALQ_NOWAIT) ? M_NOWAIT : M_WAITOK
	);

	if (ale == NULL)
		return (NULL);

	ALQ_LOCK(alq);

	/*
	 * Determine the number of free contiguous bytes.
	 * We ensure elsewhere that if aq_writehead == aq_writetail because
	 * the buffer is empty, they will both be set to 0 and therefore
	 * aq_freebytes == aq_buflen and is fully contiguous.
	 * If they are equal and the buffer is not empty, aq_freebytes will
	 * be 0 indicating the buffer is full.
	 */
	if (alq->aq_writehead <= alq->aq_writetail)
		contigbytes = alq->aq_freebytes;
	else
		contigbytes = alq->aq_buflen - alq->aq_writehead;

	/*
	 * If the message is larger than our underlying buffer or
	 * there is not enough free contiguous space in our underlying buffer
	 * to accept the message and the user can't wait, return.
	 */
	if ((len > alq->aq_buflen) ||
		((flags & ALQ_NOWAIT) && (contigbytes < len))) {
		ALQ_UNLOCK(alq);
		free(ale, M_ALD);
		return (NULL);
	}

	/*
	 * ALQ_WAITOK or contigbytes >= len,
	 * either spin until we have enough free contiguous bytes (former)
	 * or skip (latter).
	 */
	while (contigbytes < len && (alq->aq_flags & AQ_SHUTDOWN) == 0) {
		alq->aq_flags |= AQ_WANTED;
		msleep_spin(alq, &alq->aq_mtx, "alqgetn", 0);
		if (alq->aq_writehead <= alq->aq_writetail)
			contigbytes = alq->aq_freebytes;
		else
			contigbytes = alq->aq_buflen - alq->aq_writehead;
	}

	/*
	 * We need to serialise wakups to ensure records remain in order.
	 * Therefore, wakeup the next thread in the queue waiting for
	 * alq resources to be available.
	 * (technically this is only required if we actually entered the above
	 * while loop)
	 */
	wakeup_one(alq);

	/* Bail if we're shutting down */
	if (alq->aq_flags & AQ_SHUTDOWN) {
		ALQ_UNLOCK(alq);
		free(ale, M_ALD);
		return (NULL);
	}

	/*
	 * If we are here, we have a contiguous number of bytes >= len
	 * available in our buffer starting at aq_writehead.
	 */
	ale->ae_data = alq->aq_entbuf + alq->aq_writehead;
	ale->ae_datalen = len;
	alq->aq_writehead += len;
	alq->aq_freebytes -= len;

	/* Wrap aq_writehead if we've filled to the end of the buffer. */
	if (alq->aq_writehead == alq->aq_buflen)
		alq->aq_writehead = 0;

	KASSERT((alq->aq_writehead >= 0 && alq->aq_writehead < alq->aq_buflen),
		("%s: aq_writehead < 0 || aq_writehead >= aq_buflen", __func__)
	);

	return (ale);
}

void
alq_post(struct alq *alq, struct ale *ale, int flags)
{
	int activate;

	if (((alq->aq_flags & AQ_ACTIVE) == 0) &&
		((flags & ALQ_NOACTIVATE) == 0)) {
		alq->aq_flags |= AQ_ACTIVE;
		activate = 1;
	} else
		activate = 0;

	ALQ_UNLOCK(alq);

	if (activate) {
		ALD_LOCK();
		ald_activate(alq);
		ALD_UNLOCK();
	}

	free(ale, M_ALD);
}

void
alq_flush(struct alq *alq)
{
	int needwakeup = 0;

	ALD_LOCK();
	ALQ_LOCK(alq);

	if (alq->aq_flags & AQ_ACTIVE)
		ald_deactivate(alq);

	ALD_UNLOCK();

	/*
	 * Pull the lever iff there is data to flush and we're
	 * not already in the middle of a flush operation.
	 */
	if (ALQ_HAS_PENDING_DATA(alq) && (alq->aq_flags & AQ_FLUSHING) == 0)
		needwakeup = alq_doio(alq);

	ALQ_UNLOCK(alq);

	if (needwakeup)
		wakeup_one(alq);
}

/*
 * Flush remaining data, close the file and free all resources.
 */
void
alq_close(struct alq *alq)
{
	/*
	 * If we're already shuting down someone else will flush and close
	 * the vnode.
	 */
	if (ald_rem(alq) != 0)
		return;

	/*
	 * Drain all pending IO.
	 */
	alq_shutdown(alq);

	mtx_destroy(&alq->aq_mtx);
	free(alq->aq_entbuf, M_ALD);
	free(alq, M_ALD);
}

static int alq_load_handler(module_t mod, int what, void *arg)
{
	int ret = 0;

	switch(what) {
		case MOD_LOAD:
		case MOD_UNLOAD:
		case MOD_SHUTDOWN:
			break;
		
		case MOD_QUIESCE:
			ALD_LOCK();
			/* only allow unload if there are no open queues */
			if (LIST_FIRST(&ald_queues) == NULL) {
				ald_shutingdown = 1;
				ALD_UNLOCK();
				ald_shutdown(NULL, 0);
				mtx_destroy(&ald_mtx);
			} else {
				ALD_UNLOCK();
				ret = EBUSY;
			}
			break;
		
		default:
			ret = EINVAL;
			break;
	}

	return (ret);
}

/* basic module data */
static moduledata_t alq_mod =
{
	"alq",
	alq_load_handler, /* execution entry point for the module */
	NULL
};

DECLARE_MODULE(alq, alq_mod, SI_SUB_SMP, SI_ORDER_ANY);
MODULE_VERSION(alq, 1);
