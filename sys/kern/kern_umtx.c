/*
 * Copyright (c) 2002, Jeffrey Roberson <jeff@freebsd.org>
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sx.h>
#include <sys/thr.h>
#include <sys/umtx.h>

struct umtx_q {
	LIST_ENTRY(umtx_q)	uq_next;	/* Linked list for the hash. */
	TAILQ_HEAD(, thread) uq_tdq;	/* List of threads blocked here. */
	struct umtx	*uq_umtx;	/* Pointer key component. */
	pid_t		uq_pid;		/* Pid key component. */
};

#define	UMTX_QUEUES	128
#define	UMTX_HASH(pid, umtx)						\
    (((uintptr_t)pid + ((uintptr_t)umtx & ~65535)) % UMTX_QUEUES)

LIST_HEAD(umtx_head, umtx_q);
static struct umtx_head queues[UMTX_QUEUES];
static MALLOC_DEFINE(M_UMTX, "umtx", "UMTX queue memory");

static struct mtx umtx_lock;
MTX_SYSINIT(umtx, &umtx_lock, "umtx", MTX_DEF);

#define	UMTX_LOCK()	mtx_lock(&umtx_lock);
#define	UMTX_UNLOCK()	mtx_unlock(&umtx_lock);


static struct umtx_q *umtx_lookup(struct thread *, struct umtx *umtx);
static struct umtx_q *umtx_insert(struct thread *, struct umtx *umtx);

static struct umtx_q *
umtx_lookup(struct thread *td, struct umtx *umtx)
{
	struct umtx_head *head;
	struct umtx_q *uq;
	pid_t pid;

	pid = td->td_proc->p_pid;

	head = &queues[UMTX_HASH(td->td_proc->p_pid, umtx)];

	LIST_FOREACH(uq, head, uq_next) {
		if (uq->uq_pid == pid && uq->uq_umtx == umtx)
			return (uq);
	}

	return (NULL);
}

/*
 * Insert a thread onto the umtx queue.
 */
static struct umtx_q *
umtx_insert(struct thread *td, struct umtx *umtx)
{
	struct umtx_head *head;
	struct umtx_q *uq;
	pid_t pid;

	pid = td->td_proc->p_pid;

	if ((uq = umtx_lookup(td, umtx)) == NULL) {
		struct umtx_q *ins;

		UMTX_UNLOCK();
		ins = malloc(sizeof(*uq), M_UMTX, M_ZERO | M_WAITOK);
		UMTX_LOCK();

		/*
		 * Some one else could have succeeded while we were blocked
		 * waiting on memory.
		 */
		if ((uq = umtx_lookup(td, umtx)) == NULL) {
			head = &queues[UMTX_HASH(pid, umtx)];
			uq = ins;
			uq->uq_pid = pid;
			uq->uq_umtx = umtx;
			LIST_INSERT_HEAD(head, uq, uq_next);
			TAILQ_INIT(&uq->uq_tdq);
		} else
			free(ins, M_UMTX);
	}

	/*
	 * Insert us onto the end of the TAILQ.
	 */
	TAILQ_INSERT_TAIL(&uq->uq_tdq, td, td_umtx);

	return (uq);
}

static void
umtx_remove(struct umtx_q *uq, struct thread *td)
{
	TAILQ_REMOVE(&uq->uq_tdq, td, td_umtx);

	if (TAILQ_EMPTY(&uq->uq_tdq)) {
		LIST_REMOVE(uq, uq_next);
		free(uq, M_UMTX);
	}
}

int
_umtx_lock(struct thread *td, struct _umtx_lock_args *uap)
    /* struct umtx *umtx */
{
	struct umtx_q *uq;
	struct umtx *umtx;
	intptr_t owner;
	intptr_t old;
	int error;

	uq = NULL;

	/*
	 * Care must be exercised when dealing with this structure.  It
	 * can fault on any access.
	 */
	umtx = uap->umtx;	

	for (;;) {
		/*
		 * Try the uncontested case.  This should be done in userland.
		 */
		owner = casuptr((intptr_t *)&umtx->u_owner,
		    UMTX_UNOWNED, (intptr_t)td);

		/* The address was invalid. */
		if (owner == -1)
			return (EFAULT);

		/* The acquire succeeded. */
		if (owner == UMTX_UNOWNED)
			return (0);

		/* If no one owns it but it is contested try to acquire it. */
		if (owner == UMTX_CONTESTED) {
			owner = casuptr((intptr_t *)&umtx->u_owner,
			    UMTX_CONTESTED, ((intptr_t)td | UMTX_CONTESTED));

			/* The address was invalid. */
			if (owner == -1)
				return (EFAULT);

			if (owner == UMTX_CONTESTED)
				goto out;

			/* If this failed the lock has changed, restart. */
			continue;
		}


		UMTX_LOCK();
		uq = umtx_insert(td, umtx);
		UMTX_UNLOCK();

		/*
		 * Set the contested bit so that a release in user space
		 * knows to use the system call for unlock.  If this fails
		 * either some one else has acquired the lock or it has been
		 * released.
		 */
		old = casuptr((intptr_t *)&umtx->u_owner, owner,
		    owner | UMTX_CONTESTED);

		/* The address was invalid. */
		if (old == -1) {
			UMTX_LOCK();
			umtx_remove(uq, td);
			UMTX_UNLOCK();
			return (EFAULT);
		}

		/*
		 * We set the contested bit, sleep. Otherwise the lock changed
		 * and we need to retry or we lost a race to the thread
		 * unlocking the umtx.
		 */
		UMTX_LOCK();
		mtx_lock_spin(&sched_lock);
		if (old == owner && (td->td_flags & TDF_UMTXWAKEUP) == 0) {
			mtx_unlock_spin(&sched_lock);
			error = msleep(td, &umtx_lock,
			    td->td_priority | PCATCH, "umtx", 0);
			mtx_lock_spin(&sched_lock);
		} else
			error = 0;
		td->td_flags &= ~TDF_UMTXWAKEUP;
		mtx_unlock_spin(&sched_lock);

		umtx_remove(uq, td);
		UMTX_UNLOCK();

		/*
		 * If we caught a signal we might have to retry or exit 
		 * immediately.
		 */
		if (error)
			return (error);
	}
out:
	/*
	 * We reach here only if we just acquired a contested umtx.
	 *
	 * If there are no other threads on this umtx's queue
	 * clear the contested bit. However, we cannot hold
	 * a lock across casuptr().  So after we unset it we
	 * have to recheck, and set it again if another thread has
	 * put itself on the queue in the mean time.
	 */
	error = 0;
	UMTX_LOCK();
	uq = umtx_lookup(td, umtx);
	UMTX_UNLOCK();
	if (uq == NULL)
		old = casuptr((intptr_t *)&umtx->u_owner,
		    ((intptr_t)td | UMTX_CONTESTED), (intptr_t)td);
	if (uq == NULL && old == ((intptr_t)td | UMTX_CONTESTED)) {
		UMTX_LOCK();
		uq = umtx_lookup(td, umtx);
		UMTX_UNLOCK();
		if (uq != NULL) {
			old = casuptr((intptr_t *)&umtx->u_owner,
			    (intptr_t)td, ((intptr_t)td | UMTX_CONTESTED));
			if (old == -1)
				error = EFAULT;
			else if (old != (intptr_t)td)
				error = EINVAL;
		}
	}
	return (error);
}

int
_umtx_unlock(struct thread *td, struct _umtx_unlock_args *uap)
    /* struct umtx *umtx */
{
	struct thread *blocked;
	struct umtx *umtx;
	struct umtx_q *uq;
	intptr_t owner;
	intptr_t old;

	umtx = uap->umtx;

	/*
	 * Make sure we own this mtx.
	 *
	 * XXX Need a {fu,su}ptr this is not correct on arch where
	 * sizeof(intptr_t) != sizeof(long).
	 */
	if ((owner = fuword(&umtx->u_owner)) == -1)
		return (EFAULT);

	if ((struct thread *)(owner & ~UMTX_CONTESTED) != td)
		return (EPERM);

	/* We should only ever be in here for contested locks */
	KASSERT((owner & UMTX_CONTESTED) != 0, ("contested umtx is not."));

	old = casuptr((intptr_t *)&umtx->u_owner, owner, UMTX_CONTESTED);

	if (old == -1)
		return (EFAULT);

	/*
	 * This will only happen if someone modifies the lock without going
	 * through this api.
	 */
	if (old != owner)
		return (EINVAL);

	/*
	 * We have to wake up one of the blocked threads.
	 */
	UMTX_LOCK();
	uq = umtx_lookup(td, umtx);
	if (uq != NULL) {
		blocked = TAILQ_FIRST(&uq->uq_tdq);
		KASSERT(blocked != NULL, ("umtx_q with no waiting threads."));
		mtx_lock_spin(&sched_lock);
		blocked->td_flags |= TDF_UMTXWAKEUP;
		mtx_unlock_spin(&sched_lock);
		wakeup(blocked);
	}

	UMTX_UNLOCK();

	return (0);
}
