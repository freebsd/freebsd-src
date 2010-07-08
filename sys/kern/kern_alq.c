/*-
 * Copyright (c) 2002, Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2008-2009, Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2009-2010, The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by Lawrence Stewart under sponsorship from the FreeBSD Foundation.
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

#include <security/mac/mac_framework.h>

/* Async. Logging Queue */
struct alq {
	int	aq_entmax;		/* Max entries */
	int	aq_entlen;		/* Entry length */
	char	*aq_entbuf;		/* Buffer for stored entries */
	int	aq_flags;		/* Queue flags */
	struct mtx	aq_mtx;		/* Queue lock */
	struct vnode	*aq_vp;		/* Open vnode handle */
	struct ucred	*aq_cred;	/* Credentials of the opening thread */
	struct ale	*aq_first;	/* First ent */
	struct ale	*aq_entfree;	/* First free ent */
	struct ale	*aq_entvalid;	/* First ent valid for writing */
	LIST_ENTRY(alq)	aq_act;		/* List of active queues */
	LIST_ENTRY(alq)	aq_link;	/* List of all queues */
};

#define	AQ_WANTED	0x0001		/* Wakeup sleeper when io is done */
#define	AQ_ACTIVE	0x0002		/* on the active list */
#define	AQ_FLUSHING	0x0004		/* doing IO */
#define	AQ_SHUTDOWN	0x0008		/* Queue no longer valid */

#define	ALQ_LOCK(alq)	mtx_lock_spin(&(alq)->aq_mtx)
#define	ALQ_UNLOCK(alq)	mtx_unlock_spin(&(alq)->aq_mtx)

static MALLOC_DEFINE(M_ALD, "ALD", "ALD");

/*
 * The ald_mtx protects the ald_queues list and the ald_active list.
 */
static struct mtx ald_mtx;
static LIST_HEAD(, alq) ald_queues;
static LIST_HEAD(, alq) ald_active;
static int ald_shutingdown = 0;
struct thread *ald_thread;
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
static void alq_destroy(struct alq *);
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

	ald_thread = FIRST_THREAD_IN_PROC(ald_proc);

	EVENTHANDLER_REGISTER(shutdown_pre_sync, ald_shutdown, NULL,
	    SHUTDOWN_PRI_FIRST);

	ALD_LOCK();

	for (;;) {
		while ((alq = LIST_FIRST(&ald_active)) == NULL &&
		    !ald_shutingdown)
			mtx_sleep(&ald_active, &ald_mtx, PWAIT, "aldslp", 0);

		/* Don't shutdown until all active ALQs are flushed. */
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
			wakeup(alq);
		ALD_LOCK();
	}

	kproc_exit(0);
}

static void
ald_shutdown(void *arg, int howto)
{
	struct alq *alq;

	ALD_LOCK();

	/* Ensure no new queues can be created. */
	ald_shutingdown = 1;

	/* Shutdown all ALQs prior to terminating the ald_daemon. */
	while ((alq = LIST_FIRST(&ald_queues)) != NULL) {
		LIST_REMOVE(alq, aq_link);
		ALD_UNLOCK();
		alq_shutdown(alq);
		ALD_LOCK();
	}

	/* At this point, all ALQs are flushed and shutdown. */

	/*
	 * Wake ald_daemon so that it exits. It won't be able to do
	 * anything until we mtx_sleep because we hold the ald_mtx.
	 */
	wakeup(&ald_active);

	/* Wait for ald_daemon to exit. */
	mtx_sleep(ald_proc, &ald_mtx, PWAIT, "aldslp", 0);

	ALD_UNLOCK();
}

static void
alq_shutdown(struct alq *alq)
{
	ALQ_LOCK(alq);

	/* Stop any new writers. */
	alq->aq_flags |= AQ_SHUTDOWN;

	/* Drain IO */
	while (alq->aq_flags & AQ_ACTIVE) {
		alq->aq_flags |= AQ_WANTED;
		msleep_spin(alq, &alq->aq_mtx, "aldclose", 0);
	}
	ALQ_UNLOCK(alq);

	vn_close(alq->aq_vp, FWRITE, alq->aq_cred,
	    curthread);
	crfree(alq->aq_cred);
}

void
alq_destroy(struct alq *alq)
{
	/* Drain all pending IO. */
	alq_shutdown(alq);

	mtx_destroy(&alq->aq_mtx);
	free(alq->aq_first, M_ALD);
	free(alq->aq_entbuf, M_ALD);
	free(alq, M_ALD);
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
	struct ale *ale;
	struct ale *alstart;
	int totlen;
	int iov;
	int vfslocked;

	vp = alq->aq_vp;
	td = curthread;
	totlen = 0;
	iov = 0;

	alstart = ale = alq->aq_entvalid;
	alq->aq_entvalid = NULL;

	bzero(&aiov, sizeof(aiov));
	bzero(&auio, sizeof(auio));

	do {
		if (aiov[iov].iov_base == NULL)
			aiov[iov].iov_base = ale->ae_data;
		aiov[iov].iov_len += alq->aq_entlen;
		totlen += alq->aq_entlen;
		/* Check to see if we're wrapping the buffer */
		if (ale->ae_data + alq->aq_entlen != ale->ae_next->ae_data)
			iov++;
		ale->ae_flags &= ~AE_VALID;
		ale = ale->ae_next;
	} while (ale->ae_flags & AE_VALID);

	alq->aq_flags |= AQ_FLUSHING;
	ALQ_UNLOCK(alq);

	if (iov == 2 || aiov[iov].iov_base == NULL)
		iov--;

	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_iovcnt = iov + 1;
	auio.uio_resid = totlen;
	auio.uio_td = td;

	/*
	 * Do all of the junk required to write now.
	 */
	vfslocked = VFS_LOCK_GIANT(vp->v_mount);
	vn_start_write(vp, &mp, V_WAIT);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	/*
	 * XXX: VOP_WRITE error checks are ignored.
	 */
#ifdef MAC
	if (mac_vnode_check_write(alq->aq_cred, NOCRED, vp) == 0)
#endif
		VOP_WRITE(vp, &auio, IO_UNIT | IO_APPEND, alq->aq_cred);
	VOP_UNLOCK(vp, 0);
	vn_finished_write(mp);
	VFS_UNLOCK_GIANT(vfslocked);

	ALQ_LOCK(alq);
	alq->aq_flags &= ~AQ_FLUSHING;

	if (alq->aq_entfree == NULL)
		alq->aq_entfree = alstart;

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
	struct ale *ale;
	struct ale *alp;
	struct alq *alq;
	char *bufp;
	int flags;
	int error;
	int i, vfslocked;

	*alqp = NULL;
	td = curthread;

	NDINIT(&nd, LOOKUP, NOFOLLOW | MPSAFE, UIO_SYSSPACE, file, td);
	flags = FWRITE | O_NOFOLLOW | O_CREAT;

	error = vn_open_cred(&nd, &flags, cmode, 0, cred, NULL);
	if (error)
		return (error);

	vfslocked = NDHASGIANT(&nd);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	/* We just unlock so we hold a reference */
	VOP_UNLOCK(nd.ni_vp, 0);
	VFS_UNLOCK_GIANT(vfslocked);

	alq = malloc(sizeof(*alq), M_ALD, M_WAITOK|M_ZERO);
	alq->aq_entbuf = malloc(count * size, M_ALD, M_WAITOK|M_ZERO);
	alq->aq_first = malloc(sizeof(*ale) * count, M_ALD, M_WAITOK|M_ZERO);
	alq->aq_vp = nd.ni_vp;
	alq->aq_cred = crhold(cred);
	alq->aq_entmax = count;
	alq->aq_entlen = size;
	alq->aq_entfree = alq->aq_first;

	mtx_init(&alq->aq_mtx, "ALD Queue", NULL, MTX_SPIN|MTX_QUIET);

	bufp = alq->aq_entbuf;
	ale = alq->aq_first;
	alp = NULL;

	/* Match up entries with buffers */
	for (i = 0; i < count; i++) {
		if (alp)
			alp->ae_next = ale;
		ale->ae_data = bufp;
		alp = ale;
		ale++;
		bufp += size;
	}

	alp->ae_next = alq->aq_first;

	if ((error = ald_add(alq)) != 0) {
		alq_destroy(alq);
		return (error);
	}

	*alqp = alq;

	return (0);
}

/*
 * Copy a new entry into the queue.  If the operation would block either
 * wait or return an error depending on the value of waitok.
 */
int
alq_write(struct alq *alq, void *data, int waitok)
{
	struct ale *ale;

	if ((ale = alq_get(alq, waitok)) == NULL)
		return (EWOULDBLOCK);

	bcopy(data, ale->ae_data, alq->aq_entlen);
	alq_post(alq, ale);

	return (0);
}

struct ale *
alq_get(struct alq *alq, int waitok)
{
	struct ale *ale;
	struct ale *aln;

	ale = NULL;

	ALQ_LOCK(alq);

	/* Loop until we get an entry or we're shutting down */
	while ((alq->aq_flags & AQ_SHUTDOWN) == 0 && 
	    (ale = alq->aq_entfree) == NULL &&
	    (waitok & ALQ_WAITOK)) {
		alq->aq_flags |= AQ_WANTED;
		msleep_spin(alq, &alq->aq_mtx, "alqget", 0);
	}

	if (ale != NULL) {
		aln = ale->ae_next;
		if ((aln->ae_flags & AE_VALID) == 0)
			alq->aq_entfree = aln;
		else
			alq->aq_entfree = NULL;
	} else
		ALQ_UNLOCK(alq);


	return (ale);
}

void
alq_post(struct alq *alq, struct ale *ale)
{
	int activate;

	ale->ae_flags |= AE_VALID;

	if (alq->aq_entvalid == NULL)
		alq->aq_entvalid = ale;

	if ((alq->aq_flags & AQ_ACTIVE) == 0) {
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
}

void
alq_flush(struct alq *alq)
{
	int needwakeup = 0;

	ALD_LOCK();
	ALQ_LOCK(alq);
	if (alq->aq_flags & AQ_ACTIVE) {
		ald_deactivate(alq);
		ALD_UNLOCK();
		needwakeup = alq_doio(alq);
	} else
		ALD_UNLOCK();
	ALQ_UNLOCK(alq);

	if (needwakeup)
		wakeup(alq);
}

/*
 * Flush remaining data, close the file and free all resources.
 */
void
alq_close(struct alq *alq)
{
	/* Only flush and destroy alq if not already shutting down. */
	if (ald_rem(alq) == 0)
		alq_destroy(alq);
}

static int
alq_load_handler(module_t mod, int what, void *arg)
{
	int ret;
	
	ret = 0;

	switch (what) {
	case MOD_LOAD:
	case MOD_SHUTDOWN:
		break;

	case MOD_QUIESCE:
		ALD_LOCK();
		/* Only allow unload if there are no open queues. */
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

	case MOD_UNLOAD:
		/* If MOD_QUIESCE failed we must fail here too. */
		if (ald_shutingdown == 0)
			ret = EBUSY;
		break;

	default:
		ret = EINVAL;
		break;
	}

	return (ret);
}

static moduledata_t alq_mod =
{
	"alq",
	alq_load_handler,
	NULL
};

DECLARE_MODULE(alq, alq_mod, SI_SUB_SMP, SI_ORDER_ANY);
MODULE_VERSION(alq, 1);
