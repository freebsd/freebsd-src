/*-
 * Copyright (c) 2004, David Xu <davidxu@freebsd.org>
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
#include <sys/eventhandler.h>
#include <sys/thr.h>
#include <sys/umtx.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#define UMTX_PRIVATE	0
#define UMTX_SHARED	1

#define UMTX_STATIC_SHARED

struct umtx_key {
	int	type;
	union {
		struct {
			vm_object_t	object;
			long		offset;
		} shared;
		struct {
			struct umtx	*umtx;
			long		pid;
		} private;
		struct {
			void		*ptr;
			long		word;
		} both;
	} info;
};

struct umtx_q {
	LIST_ENTRY(umtx_q)	uq_next;	/* Linked list for the hash. */
	struct umtx_key		uq_key;		/* Umtx key. */
	struct thread		*uq_thread;	/* The thread waits on. */
	LIST_ENTRY(umtx_q)	uq_rqnext;	/* Linked list for requeuing. */
	vm_offset_t		uq_addr;	/* Umtx's virtual address. */
};

LIST_HEAD(umtx_head, umtx_q);
struct umtxq_chain {
	struct mtx		uc_lock;	/* Lock for this chain. */
	struct umtx_head	uc_queue;	/* List of sleep queues. */
#define	UCF_BUSY		0x01
#define	UCF_WANT		0x02
	int			uc_flags;
};

#define	GOLDEN_RATIO_PRIME	2654404609U
#define	UMTX_CHAINS		128
#define	UMTX_SHIFTS		(__WORD_BIT - 7)

static struct umtxq_chain umtxq_chains[UMTX_CHAINS];
static MALLOC_DEFINE(M_UMTX, "umtx", "UMTX queue memory");

static void umtxq_init_chains(void *);
static int umtxq_hash(struct umtx_key *key);
static struct mtx *umtxq_mtx(int chain);
static void umtxq_lock(struct umtx_key *key);
static void umtxq_unlock(struct umtx_key *key);
static void umtxq_busy(struct umtx_key *key);
static void umtxq_unbusy(struct umtx_key *key);
static void umtxq_insert(struct umtx_q *uq);
static void umtxq_remove(struct umtx_q *uq);
static int umtxq_sleep(struct thread *td, struct umtx_key *key,
	int prio, const char *wmesg, int timo);
static int umtxq_count(struct umtx_key *key);
static int umtxq_signal(struct umtx_key *key, int nr_wakeup);
#ifdef UMTX_DYNAMIC_SHARED
static void fork_handler(void *arg, struct proc *p1, struct proc *p2,
	int flags);
#endif
static int umtx_key_match(const struct umtx_key *k1, const struct umtx_key *k2);
static int umtx_key_get(struct thread *td, struct umtx *umtx,
	struct umtx_key *key);
static void umtx_key_release(struct umtx_key *key);

SYSINIT(umtx, SI_SUB_EVENTHANDLER+1, SI_ORDER_MIDDLE, umtxq_init_chains, NULL);

struct umtx_q *
umtxq_alloc(void)
{
	return (malloc(sizeof(struct umtx_q), M_UMTX, M_WAITOK));
}

void
umtxq_free(struct umtx_q *uq)
{
	free(uq, M_UMTX);
}

static void
umtxq_init_chains(void *arg __unused)
{
	int i;

	for (i = 0; i < UMTX_CHAINS; ++i) {
		mtx_init(&umtxq_chains[i].uc_lock, "umtxq_lock", NULL,
			 MTX_DEF | MTX_DUPOK);
		LIST_INIT(&umtxq_chains[i].uc_queue);
		umtxq_chains[i].uc_flags = 0;
	}
#ifdef UMTX_DYNAMIC_SHARED
	EVENTHANDLER_REGISTER(process_fork, fork_handler, 0, 10000);
#endif
}

static inline int
umtxq_hash(struct umtx_key *key)
{
	unsigned n = (uintptr_t)key->info.both.ptr + key->info.both.word;
	return (((n * GOLDEN_RATIO_PRIME) >> UMTX_SHIFTS) % UMTX_CHAINS);
}

static inline int
umtx_key_match(const struct umtx_key *k1, const struct umtx_key *k2)
{
	return (k1->type == k2->type &&
		k1->info.both.ptr == k2->info.both.ptr &&
	        k1->info.both.word == k2->info.both.word);
}

static inline struct mtx *
umtxq_mtx(int chain)
{
	return (&umtxq_chains[chain].uc_lock);
}

static inline void
umtxq_busy(struct umtx_key *key)
{
	int chain = umtxq_hash(key);

	mtx_assert(umtxq_mtx(chain), MA_OWNED);
	while (umtxq_chains[chain].uc_flags & UCF_BUSY) {
		umtxq_chains[chain].uc_flags |= UCF_WANT;
		msleep(&umtxq_chains[chain], umtxq_mtx(chain),
		    0, "umtxq_busy", 0);
	}
	umtxq_chains[chain].uc_flags |= UCF_BUSY;
}

static inline void
umtxq_unbusy(struct umtx_key *key)
{
	int chain = umtxq_hash(key);

	mtx_assert(umtxq_mtx(chain), MA_OWNED);
	KASSERT(umtxq_chains[chain].uc_flags & UCF_BUSY, ("not busy"));
	umtxq_chains[chain].uc_flags &= ~UCF_BUSY;
	if (umtxq_chains[chain].uc_flags & UCF_WANT) {
		umtxq_chains[chain].uc_flags &= ~UCF_WANT;
		wakeup(&umtxq_chains[chain]);
	}
}

static inline void
umtxq_lock(struct umtx_key *key)
{
	int chain = umtxq_hash(key);
	mtx_lock(umtxq_mtx(chain));
}

static inline void
umtxq_unlock(struct umtx_key *key)
{
	int chain = umtxq_hash(key);
	mtx_unlock(umtxq_mtx(chain));
}

/*
 * Insert a thread onto the umtx queue.
 */
static inline void
umtxq_insert(struct umtx_q *uq)
{
	struct umtx_head *head;
	int chain = umtxq_hash(&uq->uq_key);

	mtx_assert(umtxq_mtx(chain), MA_OWNED);
	head = &umtxq_chains[chain].uc_queue;
	LIST_INSERT_HEAD(head, uq, uq_next);
	mtx_lock_spin(&sched_lock);
	uq->uq_thread->td_flags |= TDF_UMTXQ;
	mtx_unlock_spin(&sched_lock);
}

/*
 * Remove thread from the umtx queue.
 */
static inline void
umtxq_remove(struct umtx_q *uq)
{
	mtx_assert(umtxq_mtx(umtxq_hash(&uq->uq_key)), MA_OWNED);
	if (uq->uq_thread->td_flags & TDF_UMTXQ) {
		LIST_REMOVE(uq, uq_next);
		/* turning off TDF_UMTXQ should be the last thing. */
		mtx_lock_spin(&sched_lock);
		uq->uq_thread->td_flags &= ~TDF_UMTXQ;
		mtx_unlock_spin(&sched_lock);
	}
}

static int
umtxq_count(struct umtx_key *key)
{
	struct umtx_q *uq;
	struct umtx_head *head;
	int chain, count = 0;

	chain = umtxq_hash(key);
	mtx_assert(umtxq_mtx(chain), MA_OWNED);
	head = &umtxq_chains[chain].uc_queue;
	LIST_FOREACH(uq, head, uq_next) {
		if (umtx_key_match(&uq->uq_key, key)) {
			if (++count > 1)
				break;
		}
	}
	return (count);
}

static int
umtxq_signal(struct umtx_key *key, int n_wake)
{
	struct umtx_q *uq, *next;
	struct umtx_head *head;
	struct thread *blocked = NULL;
	int chain, ret;

	ret = 0;
	chain = umtxq_hash(key);
	mtx_assert(umtxq_mtx(chain), MA_OWNED);
	head = &umtxq_chains[chain].uc_queue;
	for (uq = LIST_FIRST(head); uq; uq = next) {
		next = LIST_NEXT(uq, uq_next);
		if (umtx_key_match(&uq->uq_key, key)) {
			blocked = uq->uq_thread;
			umtxq_remove(uq);
			wakeup(blocked);
			if (++ret >= n_wake)
				break;
		}
	}
	return (ret);
}

static inline int
umtxq_sleep(struct thread *td, struct umtx_key *key, int priority,
	    const char *wmesg, int timo)
{
	int chain = umtxq_hash(key);
	int error = msleep(td, umtxq_mtx(chain), priority, wmesg, timo);
	if (error == EWOULDBLOCK)
		error = ETIMEDOUT;
	return (error);
}

static int
umtx_key_get(struct thread *td, struct umtx *umtx, struct umtx_key *key)
{
#if defined(UMTX_DYNAMIC_SHARED) || defined(UMTX_STATIC_SHARED)
	vm_map_t map;
	vm_map_entry_t entry;
	vm_pindex_t pindex;
	vm_prot_t prot;
	boolean_t wired;

	map = &td->td_proc->p_vmspace->vm_map;
	if (vm_map_lookup(&map, (vm_offset_t)umtx, VM_PROT_WRITE,
	    &entry, &key->info.shared.object, &pindex, &prot,
	    &wired) != KERN_SUCCESS) {
		return EFAULT;
	}
#endif

#if defined(UMTX_DYNAMIC_SHARED)
	key->type = UMTX_SHARED;
	key->info.shared.offset = entry->offset + entry->start - 
		(vm_offset_t)umtx;
	/*
	 * Add object reference, if we don't do this, a buggy application
	 * deallocates the object, the object will be reused by other
	 * applications, then unlock will wake wrong thread.
	 */
	vm_object_reference(key->info.shared.object);
	vm_map_lookup_done(map, entry);
#elif defined(UMTX_STATIC_SHARED)
	if (VM_INHERIT_SHARE == entry->inheritance) {
		key->type = UMTX_SHARED;
		key->info.shared.offset = entry->offset + entry->start -
			(vm_offset_t)umtx;
		vm_object_reference(key->info.shared.object);
	} else {
		key->type = UMTX_PRIVATE;
		key->info.private.umtx = umtx;
		key->info.private.pid  = td->td_proc->p_pid;
	}
	vm_map_lookup_done(map, entry);
#else
	key->type = UMTX_PRIVATE;
	key->info.private.umtx = umtx;
	key->info.private.pid  = td->td_proc->p_pid;
#endif
	return (0);
}

static inline void
umtx_key_release(struct umtx_key *key)
{
	if (key->type == UMTX_SHARED)
		vm_object_deallocate(key->info.shared.object);
}

static inline int
umtxq_queue_me(struct thread *td, struct umtx *umtx, struct umtx_q *uq)
{
	int error;

	if ((error = umtx_key_get(td, umtx, &uq->uq_key)) != 0)
		return (error);

	uq->uq_addr = (vm_offset_t)umtx;
	uq->uq_thread = td;
	umtxq_lock(&uq->uq_key);
	/* hmm, for condition variable, we don't need busy flag. */
	umtxq_busy(&uq->uq_key);
	umtxq_insert(uq);
	umtxq_unbusy(&uq->uq_key);
	umtxq_unlock(&uq->uq_key);
	return (0);
}

#if defined(UMTX_DYNAMIC_SHARED)
static void
fork_handler(void *arg, struct proc *p1, struct proc *p2, int flags)
{
	vm_map_t map;
	vm_map_entry_t entry;
	vm_object_t object;
	vm_pindex_t pindex;
	vm_prot_t prot;
	boolean_t wired;
	struct umtx_key key;
	LIST_HEAD(, umtx_q) workq;
	struct umtx_q *uq;
	struct thread *td;
	int onq;

	LIST_INIT(&workq);

	/* Collect threads waiting on umtxq */
	PROC_LOCK(p1);
	FOREACH_THREAD_IN_PROC(p1, td) {
		if (td->td_flags & TDF_UMTXQ) {
			uq = td->td_umtxq;
			if (uq)
				LIST_INSERT_HEAD(&workq, uq, uq_rqnext);
		}
	}
	PROC_UNLOCK(p1);

	LIST_FOREACH(uq, &workq, uq_rqnext) {
		map = &p1->p_vmspace->vm_map;
		if (vm_map_lookup(&map, uq->uq_addr, VM_PROT_WRITE,
		    &entry, &object, &pindex, &prot, &wired) != KERN_SUCCESS) {
			continue;
		}
		key.type = UMTX_SHARED;
		key.info.shared.object = object;
		key.info.shared.offset = entry->offset + entry->start -
			uq->uq_addr;
		if (umtx_key_match(&key, &uq->uq_key)) {
			vm_map_lookup_done(map, entry);
			continue;
		}
		
		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		if (uq->uq_thread->td_flags & TDF_UMTXQ) {
			umtxq_remove(uq);
			onq = 1;
		} else
			onq = 0;
		umtxq_unbusy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);
		if (onq) {
			vm_object_deallocate(uq->uq_key.info.shared.object);
			uq->uq_key = key;
			umtxq_lock(&uq->uq_key);
			umtxq_busy(&uq->uq_key);
			umtxq_insert(uq);
			umtxq_unbusy(&uq->uq_key);
			umtxq_unlock(&uq->uq_key);
			vm_object_reference(uq->uq_key.info.shared.object);
		}
		vm_map_lookup_done(map, entry);
	}
}
#endif

static int
_do_lock(struct thread *td, struct umtx *umtx, long id, int timo)
{
	struct umtx_q *uq;
	intptr_t owner;
	intptr_t old;
	int error = 0;

	uq = td->td_umtxq;
	/*
	 * Care must be exercised when dealing with umtx structure.  It
	 * can fault on any access.
	 */

	for (;;) {
		/*
		 * Try the uncontested case.  This should be done in userland.
		 */
		owner = casuptr((intptr_t *)&umtx->u_owner,
		    UMTX_UNOWNED, id);

		/* The acquire succeeded. */
		if (owner == UMTX_UNOWNED)
			return (0);

		/* The address was invalid. */
		if (owner == -1)
			return (EFAULT);

		/* If no one owns it but it is contested try to acquire it. */
		if (owner == UMTX_CONTESTED) {
			owner = casuptr((intptr_t *)&umtx->u_owner,
			    UMTX_CONTESTED, id | UMTX_CONTESTED);

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
		if (error || (error = umtxq_queue_me(td, umtx, uq)) != 0)
			return (error);

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
			umtxq_lock(&uq->uq_key);
			umtxq_busy(&uq->uq_key);
			umtxq_remove(uq);
			umtxq_unbusy(&uq->uq_key);
			umtxq_unlock(&uq->uq_key);
			umtx_key_release(&uq->uq_key);
			return (EFAULT);
		}

		/*
		 * We set the contested bit, sleep. Otherwise the lock changed
		 * and we need to retry or we lost a race to the thread
		 * unlocking the umtx.
		 */
		umtxq_lock(&uq->uq_key);
		if (old == owner && (td->td_flags & TDF_UMTXQ)) {
			error = umtxq_sleep(td, &uq->uq_key, PCATCH,
				       "umtx", timo);
		}
		umtxq_busy(&uq->uq_key);
		umtxq_remove(uq);
		umtxq_unbusy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);
		umtx_key_release(&uq->uq_key);
	}

	return (0);
}

static int
do_lock(struct thread *td, struct umtx *umtx, long id,
	struct timespec *timeout)
{
	struct timespec ts, ts2, ts3;
	struct timeval tv;
	int error;

	if (timeout == NULL) {
		error = _do_lock(td, umtx, id, 0);
	} else {
		getnanouptime(&ts);
		timespecadd(&ts, timeout);
		TIMESPEC_TO_TIMEVAL(&tv, timeout);
		for (;;) {
			error = _do_lock(td, umtx, id, tvtohz(&tv));
			if (error != ETIMEDOUT)
				break;
			getnanouptime(&ts2);
			if (timespeccmp(&ts2, &ts, >=)) {
				error = ETIMEDOUT;
				break;
			}
			ts3 = ts;
			timespecsub(&ts3, &ts2);
			TIMESPEC_TO_TIMEVAL(&tv, &ts3);
		}
	}
	/*
	 * This lets userland back off critical region if needed.
	 */
	if (error == ERESTART)
		error = EINTR;
	return (error);
}

static int
do_unlock(struct thread *td, struct umtx *umtx, long id)
{
	struct umtx_key key;
	intptr_t owner;
	intptr_t old;
	int error;
	int count;

	/*
	 * Make sure we own this mtx.
	 *
	 * XXX Need a {fu,su}ptr this is not correct on arch where
	 * sizeof(intptr_t) != sizeof(long).
	 */
	if ((owner = fuword(&umtx->u_owner)) == -1)
		return (EFAULT);

	if ((owner & ~UMTX_CONTESTED) != id)
		return (EPERM);

	/* We should only ever be in here for contested locks */
	if ((owner & UMTX_CONTESTED) == 0)
		return (EINVAL);

	if ((error = umtx_key_get(td, umtx, &key)) != 0)
		return (error);

	umtxq_lock(&key);
	umtxq_busy(&key);
	count = umtxq_count(&key);
	umtxq_unlock(&key);

	/*
	 * When unlocking the umtx, it must be marked as unowned if
	 * there is zero or one thread only waiting for it.
	 * Otherwise, it must be marked as contested.
	 */
	old = casuptr((intptr_t *)&umtx->u_owner, owner,
			count <= 1 ? UMTX_UNOWNED : UMTX_CONTESTED);
	umtxq_lock(&key);
	umtxq_signal(&key, 0);
	umtxq_unbusy(&key);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	if (old == -1)
		return (EFAULT);
	if (old != owner)
		return (EINVAL);
	return (0);
}

static int
do_wait(struct thread *td, struct umtx *umtx, long id, struct timespec *timeout)
{
	struct umtx_q *uq;
	struct timespec ts, ts2, ts3;
	struct timeval tv;
	long tmp;
	int error = 0;

	uq = td->td_umtxq;
	if ((error = umtxq_queue_me(td, umtx, uq)) != 0)
		return (error);
	tmp = fuword(&umtx->u_owner);
	if (tmp != id) {
		umtxq_lock(&uq->uq_key);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
	} else if (timeout == NULL) {
		umtxq_lock(&uq->uq_key);
		if (td->td_flags & TDF_UMTXQ)
			error = umtxq_sleep(td, &uq->uq_key,
			    PCATCH, "ucond", 0);
		if (!(td->td_flags & TDF_UMTXQ))
			error = 0;
		else
			umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
	} else {
		getnanouptime(&ts);
		timespecadd(&ts, timeout);
		TIMESPEC_TO_TIMEVAL(&tv, timeout);
		for (;;) {
			umtxq_lock(&uq->uq_key);
			if (td->td_flags & TDF_UMTXQ) {
				error = umtxq_sleep(td, &uq->uq_key, PCATCH,
					    "ucond", tvtohz(&tv));
			}
			if (!(td->td_flags & TDF_UMTXQ)) {
				umtxq_unlock(&uq->uq_key);
				goto out;
			}
			umtxq_unlock(&uq->uq_key);
			if (error != ETIMEDOUT)
				break;
			getnanouptime(&ts2);
			if (timespeccmp(&ts2, &ts, >=)) {
				error = ETIMEDOUT;
				break;
			}
			ts3 = ts;
			timespecsub(&ts3, &ts2);
			TIMESPEC_TO_TIMEVAL(&tv, &ts3);
		}
		umtxq_lock(&uq->uq_key);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
	}
out:
	umtx_key_release(&uq->uq_key);
	if (error == ERESTART)
		error = EINTR;
	return (error);
}

int
kern_umtx_wake(struct thread *td, void *uaddr, int n_wake)
{
	struct umtx_key key;
	int ret;
	
	if ((ret = umtx_key_get(td, uaddr, &key)) != 0)
		return (ret);
	umtxq_lock(&key);
	ret = umtxq_signal(&key, n_wake);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	return (0);
}

int
_umtx_lock(struct thread *td, struct _umtx_lock_args *uap)
    /* struct umtx *umtx */
{
	return _do_lock(td, uap->umtx, td->td_tid, 0);
}

int
_umtx_unlock(struct thread *td, struct _umtx_unlock_args *uap)
    /* struct umtx *umtx */
{
	return do_unlock(td, uap->umtx, td->td_tid);
}

int
_umtx_op(struct thread *td, struct _umtx_op_args *uap)
{
	struct timespec timeout;
	struct timespec *ts;
	int error;

	switch(uap->op) {
	case UMTX_OP_LOCK:
		/* Allow a null timespec (wait forever). */
		if (uap->uaddr2 == NULL)
			ts = NULL;
		else {
			error = copyin(uap->uaddr2, &timeout, sizeof(timeout));
			if (error != 0)
				break;
			if (timeout.tv_nsec >= 1000000000 ||
			    timeout.tv_nsec < 0) {
				error = EINVAL;
				break;
			}
			ts = &timeout;
		}
		error = do_lock(td, uap->umtx, uap->id, ts);
		break;
	case UMTX_OP_UNLOCK:
		error = do_unlock(td, uap->umtx, uap->id);
		break;
	case UMTX_OP_WAIT:
		/* Allow a null timespec (wait forever). */
		if (uap->uaddr2 == NULL)
			ts = NULL;
		else {
			error = copyin(uap->uaddr2, &timeout, sizeof(timeout));
			if (error != 0)
				break;
			if (timeout.tv_nsec >= 1000000000 ||
			    timeout.tv_nsec < 0) {
				error = EINVAL;
				break;
			}
			ts = &timeout;
		}
		error = do_wait(td, uap->umtx, uap->id, ts);
		break;
	case UMTX_OP_WAKE:
		error = kern_umtx_wake(td, uap->umtx, uap->id);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}
