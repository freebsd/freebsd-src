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
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/thr.h>
#include <sys/umtx.h>

struct umtx_q {
	LIST_ENTRY(umtx_q)	uq_next;	/* Linked list for the hash. */
	TAILQ_HEAD(, thread)	uq_tdq;		/* List of threads blocked here. */
	struct umtx		*uq_umtx;	/* Pointer key component. */
	pid_t			uq_pid;		/* Pid key component. */
	int			uq_count;	/* How many threads blocked. */
};

LIST_HEAD(umtx_head, umtx_q);
struct umtxq_chain {
	struct mtx		uc_lock;	/* lock for this chain. */
	struct umtx_head	uc_queues;	/* List of sleep queues. */
};

#define	GOLDEN_RATIO_PRIME	2654404609U
#define	UMTX_CHAINS		128
#define	UMTX_SHIFTS		(__WORD_BIT - 7)

static struct umtxq_chain umtxq_chains[UMTX_CHAINS];
static MALLOC_DEFINE(M_UMTX, "umtx", "UMTX queue memory");

#define	UMTX_CONTESTED	LONG_MIN

static void umtx_init_chains(void *);
static int umtxq_hash(struct thread *, struct umtx *);
static void umtxq_lock(struct thread *td, struct umtx *key);
static void umtxq_unlock(struct thread *td, struct umtx *key);
static struct umtx_q *umtxq_lookup(struct thread *, struct umtx *);
static struct umtx_q *umtxq_insert(struct thread *, struct umtx *);
static int umtxq_count(struct thread *td, struct umtx *umtx);
static int umtx_sleep(struct thread *td, struct umtx *umtx, int priority,
	   const char *wmesg, int timo);
static void umtx_signal(struct thread *td, struct umtx *umtx);

SYSINIT(umtx, SI_SUB_LOCK, SI_ORDER_MIDDLE, umtx_init_chains, NULL);

static void
umtx_init_chains(void *arg __unused)
{
	int i;

	for (i = 0; i < UMTX_CHAINS; ++i) {
		mtx_init(&umtxq_chains[i].uc_lock, "umtxq_lock", NULL,
			 MTX_DEF | MTX_DUPOK);
		LIST_INIT(&umtxq_chains[i].uc_queues);
	}
}

static inline int
umtxq_hash(struct thread *td, struct umtx *umtx)
{
	unsigned n = (uintptr_t)umtx + td->td_proc->p_pid;
	return (((n * GOLDEN_RATIO_PRIME) >> UMTX_SHIFTS) % UMTX_CHAINS);
}

static inline void
umtxq_lock(struct thread *td, struct umtx *key)
{
	int chain = umtxq_hash(td, key);
	mtx_lock(&umtxq_chains[chain].uc_lock);
}

static void
umtxq_unlock(struct thread *td, struct umtx *key)
{
	int chain = umtxq_hash(td, key);
	mtx_unlock(&umtxq_chains[chain].uc_lock);
}

static struct umtx_q *
umtxq_lookup(struct thread *td, struct umtx *umtx)
{
	struct umtx_head *head;
	struct umtx_q *uq;
	pid_t pid;
	int chain;

	chain = umtxq_hash(td, umtx);
	mtx_assert(&umtxq_chains[chain].uc_lock, MA_OWNED);
	pid = td->td_proc->p_pid;
	head = &umtxq_chains[chain].uc_queues;
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
umtxq_insert(struct thread *td, struct umtx *umtx)
{
	struct umtx_head *head;
	struct umtx_q *uq, *ins = NULL;
	pid_t pid;
	int chain;

	chain = umtxq_hash(td, umtx);
	pid = td->td_proc->p_pid;
	if ((uq = umtxq_lookup(td, umtx)) == NULL) {
		umtxq_unlock(td, umtx);
		ins = malloc(sizeof(*uq), M_UMTX, M_ZERO | M_WAITOK);
		umtxq_lock(td, umtx);

		/*
		 * Some one else could have succeeded while we were blocked
		 * waiting on memory.
		 */
		if ((uq = umtxq_lookup(td, umtx)) == NULL) {
			head = &umtxq_chains[chain].uc_queues;
			uq = ins;
			uq->uq_pid = pid;
			uq->uq_umtx = umtx;
			uq->uq_count = 0;
			LIST_INSERT_HEAD(head, uq, uq_next);
			TAILQ_INIT(&uq->uq_tdq);
			ins = NULL;
		}
	}
	TAILQ_INSERT_TAIL(&uq->uq_tdq, td, td_umtx);
	uq->uq_count++;
	if (ins) {
		umtxq_unlock(td, umtx);
		free(ins, M_UMTX);
		umtxq_lock(td, umtx);
	}
	return (uq);
}

/*
 * Remove thread from umtx queue, umtx chain lock is also
 * released.
 */
static void
umtx_remove(struct umtx_q *uq, struct thread *td, struct umtx *umtx)
{
	int chain;

	chain = umtxq_hash(td, umtx);
	mtx_assert(&umtxq_chains[chain].uc_lock, MA_OWNED);
	TAILQ_REMOVE(&uq->uq_tdq, td, td_umtx);
	uq->uq_count--;
	if (TAILQ_EMPTY(&uq->uq_tdq)) {
		LIST_REMOVE(uq, uq_next);
		umtxq_unlock(td, umtx);
		free(uq, M_UMTX);
	} else
		umtxq_unlock(td, umtx);
}

static inline int
umtxq_count(struct thread *td, struct umtx *umtx)
{
	struct umtx_q *uq;
	int count = 0;

	umtxq_lock(td, umtx);
	if ((uq = umtxq_lookup(td, umtx)) != NULL)
		count = uq->uq_count;
	umtxq_unlock(td, umtx);
	return (count);
}

static inline int
umtx_sleep(struct thread *td, struct umtx *umtx, int priority,
	   const char *wmesg, int timo)
{
	int chain;

	chain = umtxq_hash(td, umtx);
	mtx_assert(&umtxq_chains[chain].uc_lock, MA_OWNED);
	return (msleep(td, &umtxq_chains[chain].uc_lock, priority,
		       wmesg, timo));	
}

static void
umtx_signal(struct thread *td, struct umtx *umtx)
{
	struct umtx_q *uq;
	struct thread *blocked = NULL;

	umtxq_lock(td, umtx);
	if ((uq = umtxq_lookup(td, umtx)) != NULL) {
		if ((blocked = TAILQ_FIRST(&uq->uq_tdq)) != NULL) {
			mtx_lock_spin(&sched_lock);
			blocked->td_flags |= TDF_UMTXWAKEUP;
			mtx_unlock_spin(&sched_lock);
		}
	}
	umtxq_unlock(td, umtx);
	if (blocked != NULL)
		wakeup(blocked);
}

int
_umtx_lock(struct thread *td, struct _umtx_lock_args *uap)
    /* struct umtx *umtx */
{
	struct umtx_q *uq;
	struct umtx *umtx;
	intptr_t owner;
	intptr_t old;
	int error = 0;

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
		    UMTX_UNOWNED, td->td_tid);

		/* The acquire succeeded. */
		if (owner == UMTX_UNOWNED)
			return (0);

		/* The address was invalid. */
		if (owner == -1)
			return (EFAULT);

		/* If no one owns it but it is contested try to acquire it. */
		if (owner == UMTX_CONTESTED) {
			owner = casuptr((intptr_t *)&umtx->u_owner,
			    UMTX_CONTESTED, td->td_tid | UMTX_CONTESTED);

			if (owner == UMTX_CONTESTED)
				return (0);

			/* The address was invalid. */
			if (owner == -1)
				return (EFAULT);

			/* If this failed the lock has changed, restart. */
			continue;
		}

		/*
		 * If we caught a signal, we have retried and now
		 * exit immediately.
		 */
		if (error)
			return (error);

		umtxq_lock(td, umtx);
		uq = umtxq_insert(td, umtx);
		umtxq_unlock(td, umtx);

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
			umtxq_lock(td, umtx);
			umtx_remove(uq, td, umtx);
			/* unlocked by umtx_remove */
			return (EFAULT);
		}

		/*
		 * We set the contested bit, sleep. Otherwise the lock changed
		 * and we need to retry or we lost a race to the thread
		 * unlocking the umtx.
		 */
		umtxq_lock(td, umtx);
		if (old == owner && (td->td_flags & TDF_UMTXWAKEUP) == 0)
			error = umtx_sleep(td, umtx, td->td_priority | PCATCH,
				    "umtx", 0);
		else
			error = 0;
		umtx_remove(uq, td, umtx);
		/* unlocked by umtx_remove */

		if (td->td_flags & TDF_UMTXWAKEUP) {
			/*
			 * If we were resumed by umtxq_unlock, we should retry
			 * to avoid a race.
			 */
			mtx_lock_spin(&sched_lock);
			td->td_flags &= ~TDF_UMTXWAKEUP;
			mtx_unlock_spin(&sched_lock);
			continue;
		}

		/*
		 * If we caught a signal, exit immediately.
		 */
		if (error)
			return (error);
	}

	return (0);
}

int
_umtx_unlock(struct thread *td, struct _umtx_unlock_args *uap)
    /* struct umtx *umtx */
{
	struct umtx *umtx;
	intptr_t owner;
	intptr_t old;
	int count;

	umtx = uap->umtx;

	/*
	 * Make sure we own this mtx.
	 *
	 * XXX Need a {fu,su}ptr this is not correct on arch where
	 * sizeof(intptr_t) != sizeof(long).
	 */
	if ((owner = fuword(&umtx->u_owner)) == -1)
		return (EFAULT);

	if ((owner & ~UMTX_CONTESTED) != td->td_tid)
		return (EPERM);

	/* We should only ever be in here for contested locks */
	if ((owner & UMTX_CONTESTED) == 0)
		return (EINVAL);

	/*
	 * When unlocking the umtx, it must be marked as unowned if
	 * there is zero or one thread only waiting for it.
	 * Otherwise, it must be marked as contested.
	 */
	old = casuptr((intptr_t *)&umtx->u_owner, owner, UMTX_UNOWNED);
	if (old == -1)
		return (EFAULT);
	if (old != owner)
		return (EINVAL);

	/*
	 * At the point, a new thread can lock the umtx before we
	 * reach here, so contested bit will not be set, if there
	 * are two or more threads on wait queue, we should set
	 * contensted bit for them.
	 */
	count = umtxq_count(td, umtx);
	if (count <= 0)
		return (0);

	/*
	 * If there is second thread waiting on umtx, set contested bit,
	 * if they are resumed before we reach here, it is harmless,
	 * just a bit unefficient.
	 */
	if (count > 1) {
		owner = UMTX_UNOWNED;
		for (;;) {
			old = casuptr((intptr_t *)&umtx->u_owner, owner,
				    owner | UMTX_CONTESTED);
			if (old == owner)
				break;
			if (old == -1)
				return (EFAULT);
			owner = old;
		}
		/*
		 * Another thread locked the umtx before us, so don't bother
		 * to wake more threads, that thread will do it when it unlocks
		 * the umtx.
		 */
		if ((owner & ~UMTX_CONTESTED) != 0)
			return (0);
	}

	/* Wake blocked thread. */
	umtx_signal(td, umtx);

	return (0);
}
