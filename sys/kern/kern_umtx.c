/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015, 2016 The FreeBSD Foundation
 * Copyright (c) 2004, David Xu <davidxu@freebsd.org>
 * Copyright (c) 2002, Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
#include "opt_umtx_profiling.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/syscallsubr.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/eventhandler.h>
#include <sys/umtx.h>
#include <sys/umtxvar.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/uma.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#include <machine/atomic.h>
#include <machine/cpu.h>

#include <compat/freebsd32/freebsd32.h>
#ifdef COMPAT_FREEBSD32
#include <compat/freebsd32/freebsd32_proto.h>
#endif

#define _UMUTEX_TRY		1
#define _UMUTEX_WAIT		2

#ifdef UMTX_PROFILING
#define	UPROF_PERC_BIGGER(w, f, sw, sf)					\
	(((w) > (sw)) || ((w) == (sw) && (f) > (sf)))
#endif

#define	UMTXQ_LOCKED_ASSERT(uc)		mtx_assert(&(uc)->uc_lock, MA_OWNED)
#ifdef INVARIANTS
#define	UMTXQ_ASSERT_LOCKED_BUSY(key) do {				\
	struct umtxq_chain *uc;						\
									\
	uc = umtxq_getchain(key);					\
	mtx_assert(&uc->uc_lock, MA_OWNED);				\
	KASSERT(uc->uc_busy != 0, ("umtx chain is not busy"));		\
} while (0)
#else
#define	UMTXQ_ASSERT_LOCKED_BUSY(key) do {} while (0)
#endif

/*
 * Don't propagate time-sharing priority, there is a security reason,
 * a user can simply introduce PI-mutex, let thread A lock the mutex,
 * and let another thread B block on the mutex, because B is
 * sleeping, its priority will be boosted, this causes A's priority to
 * be boosted via priority propagating too and will never be lowered even
 * if it is using 100%CPU, this is unfair to other processes.
 */

#define UPRI(td)	(((td)->td_user_pri >= PRI_MIN_TIMESHARE &&\
			  (td)->td_user_pri <= PRI_MAX_TIMESHARE) ?\
			 PRI_MAX_TIMESHARE : (td)->td_user_pri)

#define	GOLDEN_RATIO_PRIME	2654404609U
#ifndef	UMTX_CHAINS
#define	UMTX_CHAINS		512
#endif
#define	UMTX_SHIFTS		(__WORD_BIT - 9)

#define	GET_SHARE(flags)	\
    (((flags) & USYNC_PROCESS_SHARED) == 0 ? THREAD_SHARE : PROCESS_SHARE)

#define BUSY_SPINS		200

struct umtx_copyops {
	int	(*copyin_timeout)(const void *uaddr, struct timespec *tsp);
	int	(*copyin_umtx_time)(const void *uaddr, size_t size,
	    struct _umtx_time *tp);
	int	(*copyin_robust_lists)(const void *uaddr, size_t size,
	    struct umtx_robust_lists_params *rbp);
	int	(*copyout_timeout)(void *uaddr, size_t size,
	    struct timespec *tsp);
	const size_t	timespec_sz;
	const size_t	umtx_time_sz;
	const bool	compat32;
};

_Static_assert(sizeof(struct umutex) == sizeof(struct umutex32), "umutex32");
_Static_assert(__offsetof(struct umutex, m_spare[0]) ==
    __offsetof(struct umutex32, m_spare[0]), "m_spare32");

int umtx_shm_vnobj_persistent = 0;
SYSCTL_INT(_kern_ipc, OID_AUTO, umtx_vnode_persistent, CTLFLAG_RWTUN,
    &umtx_shm_vnobj_persistent, 0,
    "False forces destruction of umtx attached to file, on last close");
static int umtx_max_rb = 1000;
SYSCTL_INT(_kern_ipc, OID_AUTO, umtx_max_robust, CTLFLAG_RWTUN,
    &umtx_max_rb, 0,
    "Maximum number of robust mutexes allowed for each thread");

static uma_zone_t		umtx_pi_zone;
static struct umtxq_chain	umtxq_chains[2][UMTX_CHAINS];
static MALLOC_DEFINE(M_UMTX, "umtx", "UMTX queue memory");
static int			umtx_pi_allocated;

static SYSCTL_NODE(_debug, OID_AUTO, umtx, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "umtx debug");
SYSCTL_INT(_debug_umtx, OID_AUTO, umtx_pi_allocated, CTLFLAG_RD,
    &umtx_pi_allocated, 0, "Allocated umtx_pi");
static int umtx_verbose_rb = 1;
SYSCTL_INT(_debug_umtx, OID_AUTO, robust_faults_verbose, CTLFLAG_RWTUN,
    &umtx_verbose_rb, 0,
    "");

#ifdef UMTX_PROFILING
static long max_length;
SYSCTL_LONG(_debug_umtx, OID_AUTO, max_length, CTLFLAG_RD, &max_length, 0, "max_length");
static SYSCTL_NODE(_debug_umtx, OID_AUTO, chains, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "umtx chain stats");
#endif

static inline void umtx_abs_timeout_init2(struct umtx_abs_timeout *timo,
    const struct _umtx_time *umtxtime);

static void umtx_shm_init(void);
static void umtxq_sysinit(void *);
static void umtxq_hash(struct umtx_key *key);
static int do_unlock_pp(struct thread *td, struct umutex *m, uint32_t flags,
    bool rb);
static void umtx_thread_cleanup(struct thread *td);
SYSINIT(umtx, SI_SUB_EVENTHANDLER+1, SI_ORDER_MIDDLE, umtxq_sysinit, NULL);

#define umtxq_signal(key, nwake)	umtxq_signal_queue((key), (nwake), UMTX_SHARED_QUEUE)

static struct mtx umtx_lock;

#ifdef UMTX_PROFILING
static void
umtx_init_profiling(void)
{
	struct sysctl_oid *chain_oid;
	char chain_name[10];
	int i;

	for (i = 0; i < UMTX_CHAINS; ++i) {
		snprintf(chain_name, sizeof(chain_name), "%d", i);
		chain_oid = SYSCTL_ADD_NODE(NULL,
		    SYSCTL_STATIC_CHILDREN(_debug_umtx_chains), OID_AUTO,
		    chain_name, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
		    "umtx hash stats");
		SYSCTL_ADD_INT(NULL, SYSCTL_CHILDREN(chain_oid), OID_AUTO,
		    "max_length0", CTLFLAG_RD, &umtxq_chains[0][i].max_length, 0, NULL);
		SYSCTL_ADD_INT(NULL, SYSCTL_CHILDREN(chain_oid), OID_AUTO,
		    "max_length1", CTLFLAG_RD, &umtxq_chains[1][i].max_length, 0, NULL);
	}
}

static int
sysctl_debug_umtx_chains_peaks(SYSCTL_HANDLER_ARGS)
{
	char buf[512];
	struct sbuf sb;
	struct umtxq_chain *uc;
	u_int fract, i, j, tot, whole;
	u_int sf0, sf1, sf2, sf3, sf4;
	u_int si0, si1, si2, si3, si4;
	u_int sw0, sw1, sw2, sw3, sw4;

	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	for (i = 0; i < 2; i++) {
		tot = 0;
		for (j = 0; j < UMTX_CHAINS; ++j) {
			uc = &umtxq_chains[i][j];
			mtx_lock(&uc->uc_lock);
			tot += uc->max_length;
			mtx_unlock(&uc->uc_lock);
		}
		if (tot == 0)
			sbuf_printf(&sb, "%u) Empty ", i);
		else {
			sf0 = sf1 = sf2 = sf3 = sf4 = 0;
			si0 = si1 = si2 = si3 = si4 = 0;
			sw0 = sw1 = sw2 = sw3 = sw4 = 0;
			for (j = 0; j < UMTX_CHAINS; j++) {
				uc = &umtxq_chains[i][j];
				mtx_lock(&uc->uc_lock);
				whole = uc->max_length * 100;
				mtx_unlock(&uc->uc_lock);
				fract = (whole % tot) * 100;
				if (UPROF_PERC_BIGGER(whole, fract, sw0, sf0)) {
					sf0 = fract;
					si0 = j;
					sw0 = whole;
				} else if (UPROF_PERC_BIGGER(whole, fract, sw1,
				    sf1)) {
					sf1 = fract;
					si1 = j;
					sw1 = whole;
				} else if (UPROF_PERC_BIGGER(whole, fract, sw2,
				    sf2)) {
					sf2 = fract;
					si2 = j;
					sw2 = whole;
				} else if (UPROF_PERC_BIGGER(whole, fract, sw3,
				    sf3)) {
					sf3 = fract;
					si3 = j;
					sw3 = whole;
				} else if (UPROF_PERC_BIGGER(whole, fract, sw4,
				    sf4)) {
					sf4 = fract;
					si4 = j;
					sw4 = whole;
				}
			}
			sbuf_printf(&sb, "queue %u:\n", i);
			sbuf_printf(&sb, "1st: %u.%u%% idx: %u\n", sw0 / tot,
			    sf0 / tot, si0);
			sbuf_printf(&sb, "2nd: %u.%u%% idx: %u\n", sw1 / tot,
			    sf1 / tot, si1);
			sbuf_printf(&sb, "3rd: %u.%u%% idx: %u\n", sw2 / tot,
			    sf2 / tot, si2);
			sbuf_printf(&sb, "4th: %u.%u%% idx: %u\n", sw3 / tot,
			    sf3 / tot, si3);
			sbuf_printf(&sb, "5th: %u.%u%% idx: %u\n", sw4 / tot,
			    sf4 / tot, si4);
		}
	}
	sbuf_trim(&sb);
	sbuf_finish(&sb);
	sysctl_handle_string(oidp, sbuf_data(&sb), sbuf_len(&sb), req);
	sbuf_delete(&sb);
	return (0);
}

static int
sysctl_debug_umtx_chains_clear(SYSCTL_HANDLER_ARGS)
{
	struct umtxq_chain *uc;
	u_int i, j;
	int clear, error;

	clear = 0;
	error = sysctl_handle_int(oidp, &clear, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (clear != 0) {
		for (i = 0; i < 2; ++i) {
			for (j = 0; j < UMTX_CHAINS; ++j) {
				uc = &umtxq_chains[i][j];
				mtx_lock(&uc->uc_lock);
				uc->length = 0;
				uc->max_length = 0;
				mtx_unlock(&uc->uc_lock);
			}
		}
	}
	return (0);
}

SYSCTL_PROC(_debug_umtx_chains, OID_AUTO, clear,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, 0, 0,
    sysctl_debug_umtx_chains_clear, "I",
    "Clear umtx chains statistics");
SYSCTL_PROC(_debug_umtx_chains, OID_AUTO, peaks,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, 0, 0,
    sysctl_debug_umtx_chains_peaks, "A",
    "Highest peaks in chains max length");
#endif

static void
umtxq_sysinit(void *arg __unused)
{
	int i, j;

	umtx_pi_zone = uma_zcreate("umtx pi", sizeof(struct umtx_pi),
		NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	for (i = 0; i < 2; ++i) {
		for (j = 0; j < UMTX_CHAINS; ++j) {
			mtx_init(&umtxq_chains[i][j].uc_lock, "umtxql", NULL,
				 MTX_DEF | MTX_DUPOK);
			LIST_INIT(&umtxq_chains[i][j].uc_queue[0]);
			LIST_INIT(&umtxq_chains[i][j].uc_queue[1]);
			LIST_INIT(&umtxq_chains[i][j].uc_spare_queue);
			TAILQ_INIT(&umtxq_chains[i][j].uc_pi_list);
			umtxq_chains[i][j].uc_busy = 0;
			umtxq_chains[i][j].uc_waiters = 0;
#ifdef UMTX_PROFILING
			umtxq_chains[i][j].length = 0;
			umtxq_chains[i][j].max_length = 0;
#endif
		}
	}
#ifdef UMTX_PROFILING
	umtx_init_profiling();
#endif
	mtx_init(&umtx_lock, "umtx lock", NULL, MTX_DEF);
	umtx_shm_init();
}

struct umtx_q *
umtxq_alloc(void)
{
	struct umtx_q *uq;

	uq = malloc(sizeof(struct umtx_q), M_UMTX, M_WAITOK | M_ZERO);
	uq->uq_spare_queue = malloc(sizeof(struct umtxq_queue), M_UMTX,
	    M_WAITOK | M_ZERO);
	TAILQ_INIT(&uq->uq_spare_queue->head);
	TAILQ_INIT(&uq->uq_pi_contested);
	uq->uq_inherited_pri = PRI_MAX;
	return (uq);
}

void
umtxq_free(struct umtx_q *uq)
{

	MPASS(uq->uq_spare_queue != NULL);
	free(uq->uq_spare_queue, M_UMTX);
	free(uq, M_UMTX);
}

static inline void
umtxq_hash(struct umtx_key *key)
{
	unsigned n;

	n = (uintptr_t)key->info.both.a + key->info.both.b;
	key->hash = ((n * GOLDEN_RATIO_PRIME) >> UMTX_SHIFTS) % UMTX_CHAINS;
}

struct umtxq_chain *
umtxq_getchain(struct umtx_key *key)
{

	if (key->type <= TYPE_SEM)
		return (&umtxq_chains[1][key->hash]);
	return (&umtxq_chains[0][key->hash]);
}

/*
 * Set chain to busy state when following operation
 * may be blocked (kernel mutex can not be used).
 */
void
umtxq_busy(struct umtx_key *key)
{
	struct umtxq_chain *uc;

	uc = umtxq_getchain(key);
	mtx_assert(&uc->uc_lock, MA_OWNED);
	if (uc->uc_busy) {
#ifdef SMP
		if (smp_cpus > 1) {
			int count = BUSY_SPINS;
			if (count > 0) {
				umtxq_unlock(key);
				while (uc->uc_busy && --count > 0)
					cpu_spinwait();
				umtxq_lock(key);
			}
		}
#endif
		while (uc->uc_busy) {
			uc->uc_waiters++;
			msleep(uc, &uc->uc_lock, 0, "umtxqb", 0);
			uc->uc_waiters--;
		}
	}
	uc->uc_busy = 1;
}

/*
 * Unbusy a chain.
 */
void
umtxq_unbusy(struct umtx_key *key)
{
	struct umtxq_chain *uc;

	uc = umtxq_getchain(key);
	mtx_assert(&uc->uc_lock, MA_OWNED);
	KASSERT(uc->uc_busy != 0, ("not busy"));
	uc->uc_busy = 0;
	if (uc->uc_waiters)
		wakeup_one(uc);
}

void
umtxq_unbusy_unlocked(struct umtx_key *key)
{

	umtxq_lock(key);
	umtxq_unbusy(key);
	umtxq_unlock(key);
}

static struct umtxq_queue *
umtxq_queue_lookup(struct umtx_key *key, int q)
{
	struct umtxq_queue *uh;
	struct umtxq_chain *uc;

	uc = umtxq_getchain(key);
	UMTXQ_LOCKED_ASSERT(uc);
	LIST_FOREACH(uh, &uc->uc_queue[q], link) {
		if (umtx_key_match(&uh->key, key))
			return (uh);
	}

	return (NULL);
}

void
umtxq_insert_queue(struct umtx_q *uq, int q)
{
	struct umtxq_queue *uh;
	struct umtxq_chain *uc;

	uc = umtxq_getchain(&uq->uq_key);
	UMTXQ_LOCKED_ASSERT(uc);
	KASSERT((uq->uq_flags & UQF_UMTXQ) == 0, ("umtx_q is already on queue"));
	uh = umtxq_queue_lookup(&uq->uq_key, q);
	if (uh != NULL) {
		LIST_INSERT_HEAD(&uc->uc_spare_queue, uq->uq_spare_queue, link);
	} else {
		uh = uq->uq_spare_queue;
		uh->key = uq->uq_key;
		LIST_INSERT_HEAD(&uc->uc_queue[q], uh, link);
#ifdef UMTX_PROFILING
		uc->length++;
		if (uc->length > uc->max_length) {
			uc->max_length = uc->length;
			if (uc->max_length > max_length)
				max_length = uc->max_length;
		}
#endif
	}
	uq->uq_spare_queue = NULL;

	TAILQ_INSERT_TAIL(&uh->head, uq, uq_link);
	uh->length++;
	uq->uq_flags |= UQF_UMTXQ;
	uq->uq_cur_queue = uh;
	return;
}

void
umtxq_remove_queue(struct umtx_q *uq, int q)
{
	struct umtxq_chain *uc;
	struct umtxq_queue *uh;

	uc = umtxq_getchain(&uq->uq_key);
	UMTXQ_LOCKED_ASSERT(uc);
	if (uq->uq_flags & UQF_UMTXQ) {
		uh = uq->uq_cur_queue;
		TAILQ_REMOVE(&uh->head, uq, uq_link);
		uh->length--;
		uq->uq_flags &= ~UQF_UMTXQ;
		if (TAILQ_EMPTY(&uh->head)) {
			KASSERT(uh->length == 0,
			    ("inconsistent umtxq_queue length"));
#ifdef UMTX_PROFILING
			uc->length--;
#endif
			LIST_REMOVE(uh, link);
		} else {
			uh = LIST_FIRST(&uc->uc_spare_queue);
			KASSERT(uh != NULL, ("uc_spare_queue is empty"));
			LIST_REMOVE(uh, link);
		}
		uq->uq_spare_queue = uh;
		uq->uq_cur_queue = NULL;
	}
}

/*
 * Check if there are multiple waiters
 */
int
umtxq_count(struct umtx_key *key)
{
	struct umtxq_queue *uh;

	UMTXQ_LOCKED_ASSERT(umtxq_getchain(key));
	uh = umtxq_queue_lookup(key, UMTX_SHARED_QUEUE);
	if (uh != NULL)
		return (uh->length);
	return (0);
}

/*
 * Check if there are multiple PI waiters and returns first
 * waiter.
 */
static int
umtxq_count_pi(struct umtx_key *key, struct umtx_q **first)
{
	struct umtxq_queue *uh;

	*first = NULL;
	UMTXQ_LOCKED_ASSERT(umtxq_getchain(key));
	uh = umtxq_queue_lookup(key, UMTX_SHARED_QUEUE);
	if (uh != NULL) {
		*first = TAILQ_FIRST(&uh->head);
		return (uh->length);
	}
	return (0);
}

/*
 * Wake up threads waiting on an userland object by a bit mask.
 */
int
umtxq_signal_mask(struct umtx_key *key, int n_wake, u_int bitset)
{
	struct umtxq_queue *uh;
	struct umtx_q *uq, *uq_temp;
	int ret;

	ret = 0;
	UMTXQ_LOCKED_ASSERT(umtxq_getchain(key));
	uh = umtxq_queue_lookup(key, UMTX_SHARED_QUEUE);
	if (uh == NULL)
		return (0);
	TAILQ_FOREACH_SAFE(uq, &uh->head, uq_link, uq_temp) {
		if ((uq->uq_bitset & bitset) == 0)
			continue;
		umtxq_remove_queue(uq, UMTX_SHARED_QUEUE);
		wakeup_one(uq);
		if (++ret >= n_wake)
			break;
	}
	return (ret);
}

/*
 * Wake up threads waiting on an userland object.
 */

static int
umtxq_signal_queue(struct umtx_key *key, int n_wake, int q)
{
	struct umtxq_queue *uh;
	struct umtx_q *uq;
	int ret;

	ret = 0;
	UMTXQ_LOCKED_ASSERT(umtxq_getchain(key));
	uh = umtxq_queue_lookup(key, q);
	if (uh != NULL) {
		while ((uq = TAILQ_FIRST(&uh->head)) != NULL) {
			umtxq_remove_queue(uq, q);
			wakeup(uq);
			if (++ret >= n_wake)
				return (ret);
		}
	}
	return (ret);
}

/*
 * Wake up specified thread.
 */
static inline void
umtxq_signal_thread(struct umtx_q *uq)
{

	UMTXQ_LOCKED_ASSERT(umtxq_getchain(&uq->uq_key));
	umtxq_remove(uq);
	wakeup(uq);
}

/*
 * Wake up a maximum of n_wake threads that are waiting on an userland
 * object identified by key. The remaining threads are removed from queue
 * identified by key and added to the queue identified by key2 (requeued).
 * The n_requeue specifies an upper limit on the number of threads that
 * are requeued to the second queue.
 */
int
umtxq_requeue(struct umtx_key *key, int n_wake, struct umtx_key *key2,
    int n_requeue)
{
	struct umtxq_queue *uh;
	struct umtx_q *uq, *uq_temp;
	int ret;

	ret = 0;
	UMTXQ_LOCKED_ASSERT(umtxq_getchain(key));
	UMTXQ_LOCKED_ASSERT(umtxq_getchain(key2));
	uh = umtxq_queue_lookup(key, UMTX_SHARED_QUEUE);
	if (uh == NULL)
		return (0);
	TAILQ_FOREACH_SAFE(uq, &uh->head, uq_link, uq_temp) {
		if (++ret <= n_wake) {
			umtxq_remove(uq);
			wakeup_one(uq);
		} else {
			umtxq_remove(uq);
			uq->uq_key = *key2;
			umtxq_insert(uq);
			if (ret - n_wake == n_requeue)
				break;
		}
	}
	return (ret);
}

static inline int
tstohz(const struct timespec *tsp)
{
	struct timeval tv;

	TIMESPEC_TO_TIMEVAL(&tv, tsp);
	return tvtohz(&tv);
}

void
umtx_abs_timeout_init(struct umtx_abs_timeout *timo, int clockid,
    int absolute, const struct timespec *timeout)
{

	timo->clockid = clockid;
	if (!absolute) {
		timo->is_abs_real = false;
		kern_clock_gettime(curthread, timo->clockid, &timo->cur);
		timespecadd(&timo->cur, timeout, &timo->end);
	} else {
		timo->end = *timeout;
		timo->is_abs_real = clockid == CLOCK_REALTIME ||
		    clockid == CLOCK_REALTIME_FAST ||
		    clockid == CLOCK_REALTIME_PRECISE ||
		    clockid == CLOCK_SECOND;
	}
}

static void
umtx_abs_timeout_init2(struct umtx_abs_timeout *timo,
    const struct _umtx_time *umtxtime)
{

	umtx_abs_timeout_init(timo, umtxtime->_clockid,
	    (umtxtime->_flags & UMTX_ABSTIME) != 0, &umtxtime->_timeout);
}

static void
umtx_abs_timeout_enforce_min(sbintime_t *sbt)
{
	sbintime_t when, mint;

	mint = curproc->p_umtx_min_timeout;
	if (__predict_false(mint != 0)) {
		when = sbinuptime() + mint;
		if (*sbt < when)
			*sbt = when;
	}
}

static int
umtx_abs_timeout_getsbt(struct umtx_abs_timeout *timo, sbintime_t *sbt,
    int *flags)
{
	struct bintime bt, bbt;
	struct timespec tts;
	sbintime_t rem;

	switch (timo->clockid) {

	/* Clocks that can be converted into absolute time. */
	case CLOCK_REALTIME:
	case CLOCK_REALTIME_PRECISE:
	case CLOCK_REALTIME_FAST:
	case CLOCK_MONOTONIC:
	case CLOCK_MONOTONIC_PRECISE:
	case CLOCK_MONOTONIC_FAST:
	case CLOCK_UPTIME:
	case CLOCK_UPTIME_PRECISE:
	case CLOCK_UPTIME_FAST:
	case CLOCK_SECOND:
		timespec2bintime(&timo->end, &bt);
		switch (timo->clockid) {
		case CLOCK_REALTIME:
		case CLOCK_REALTIME_PRECISE:
		case CLOCK_REALTIME_FAST:
		case CLOCK_SECOND:
			getboottimebin(&bbt);
			bintime_sub(&bt, &bbt);
			break;
		}
		if (bt.sec < 0)
			return (ETIMEDOUT);
		if (bt.sec >= (SBT_MAX >> 32)) {
			*sbt = 0;
			*flags = 0;
			return (0);
		}
		*sbt = bttosbt(bt);
		umtx_abs_timeout_enforce_min(sbt);

		/*
		 * Check if the absolute time should be aligned to
		 * avoid firing multiple timer events in non-periodic
		 * timer mode.
		 */
		switch (timo->clockid) {
		case CLOCK_REALTIME_FAST:
		case CLOCK_MONOTONIC_FAST:
		case CLOCK_UPTIME_FAST:
			rem = *sbt % tc_tick_sbt;
			if (__predict_true(rem != 0))
				*sbt += tc_tick_sbt - rem;
			break;
		case CLOCK_SECOND:
			rem = *sbt % SBT_1S;
			if (__predict_true(rem != 0))
				*sbt += SBT_1S - rem;
			break;
		}
		*flags = C_ABSOLUTE;
		return (0);

	/* Clocks that has to be periodically polled. */
	case CLOCK_VIRTUAL:
	case CLOCK_PROF:
	case CLOCK_THREAD_CPUTIME_ID:
	case CLOCK_PROCESS_CPUTIME_ID:
	default:
		kern_clock_gettime(curthread, timo->clockid, &timo->cur);
		if (timespeccmp(&timo->end, &timo->cur, <=))
			return (ETIMEDOUT);
		timespecsub(&timo->end, &timo->cur, &tts);
		*sbt = tick_sbt * tstohz(&tts);
		*flags = C_HARDCLOCK;
		return (0);
	}
}

static uint32_t
umtx_unlock_val(uint32_t flags, bool rb)
{

	if (rb)
		return (UMUTEX_RB_OWNERDEAD);
	else if ((flags & UMUTEX_NONCONSISTENT) != 0)
		return (UMUTEX_RB_NOTRECOV);
	else
		return (UMUTEX_UNOWNED);

}

/*
 * Put thread into sleep state, before sleeping, check if
 * thread was removed from umtx queue.
 */
int
umtxq_sleep(struct umtx_q *uq, const char *wmesg,
    struct umtx_abs_timeout *timo)
{
	struct umtxq_chain *uc;
	sbintime_t sbt = 0;
	int error, flags = 0;

	uc = umtxq_getchain(&uq->uq_key);
	UMTXQ_LOCKED_ASSERT(uc);
	for (;;) {
		if (!(uq->uq_flags & UQF_UMTXQ)) {
			error = 0;
			break;
		}
		if (timo != NULL) {
			if (timo->is_abs_real)
				curthread->td_rtcgen =
				    atomic_load_acq_int(&rtc_generation);
			error = umtx_abs_timeout_getsbt(timo, &sbt, &flags);
			if (error != 0)
				break;
		}
		error = msleep_sbt(uq, &uc->uc_lock, PCATCH | PDROP, wmesg,
		    sbt, 0, flags);
		uc = umtxq_getchain(&uq->uq_key);
		mtx_lock(&uc->uc_lock);
		if (error == EINTR || error == ERESTART)
			break;
		if (error == EWOULDBLOCK && (flags & C_ABSOLUTE) != 0) {
			error = ETIMEDOUT;
			break;
		}
	}

	curthread->td_rtcgen = 0;
	return (error);
}

/*
 * Convert userspace address into unique logical address.
 */
int
umtx_key_get(const void *addr, int type, int share, struct umtx_key *key)
{
	struct thread *td = curthread;
	vm_map_t map;
	vm_map_entry_t entry;
	vm_pindex_t pindex;
	vm_prot_t prot;
	boolean_t wired;

	key->type = type;
	if (share == THREAD_SHARE) {
		key->shared = 0;
		key->info.private.vs = td->td_proc->p_vmspace;
		key->info.private.addr = (uintptr_t)addr;
	} else {
		MPASS(share == PROCESS_SHARE || share == AUTO_SHARE);
		map = &td->td_proc->p_vmspace->vm_map;
		if (vm_map_lookup(&map, (vm_offset_t)addr, VM_PROT_WRITE,
		    &entry, &key->info.shared.object, &pindex, &prot,
		    &wired) != KERN_SUCCESS) {
			return (EFAULT);
		}

		if ((share == PROCESS_SHARE) ||
		    (share == AUTO_SHARE &&
		     VM_INHERIT_SHARE == entry->inheritance)) {
			key->shared = 1;
			key->info.shared.offset = (vm_offset_t)addr -
			    entry->start + entry->offset;
			vm_object_reference(key->info.shared.object);
		} else {
			key->shared = 0;
			key->info.private.vs = td->td_proc->p_vmspace;
			key->info.private.addr = (uintptr_t)addr;
		}
		vm_map_lookup_done(map, entry);
	}

	umtxq_hash(key);
	return (0);
}

/*
 * Release key.
 */
void
umtx_key_release(struct umtx_key *key)
{
	if (key->shared)
		vm_object_deallocate(key->info.shared.object);
}

#ifdef COMPAT_FREEBSD10
/*
 * Lock a umtx object.
 */
static int
do_lock_umtx(struct thread *td, struct umtx *umtx, u_long id,
    const struct timespec *timeout)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	u_long owner;
	u_long old;
	int error = 0;

	uq = td->td_umtxq;
	if (timeout != NULL)
		umtx_abs_timeout_init(&timo, CLOCK_REALTIME, 0, timeout);

	/*
	 * Care must be exercised when dealing with umtx structure. It
	 * can fault on any access.
	 */
	for (;;) {
		/*
		 * Try the uncontested case.  This should be done in userland.
		 */
		owner = casuword(&umtx->u_owner, UMTX_UNOWNED, id);

		/* The acquire succeeded. */
		if (owner == UMTX_UNOWNED)
			return (0);

		/* The address was invalid. */
		if (owner == -1)
			return (EFAULT);

		/* If no one owns it but it is contested try to acquire it. */
		if (owner == UMTX_CONTESTED) {
			owner = casuword(&umtx->u_owner,
			    UMTX_CONTESTED, id | UMTX_CONTESTED);

			if (owner == UMTX_CONTESTED)
				return (0);

			/* The address was invalid. */
			if (owner == -1)
				return (EFAULT);

			error = thread_check_susp(td, false);
			if (error != 0)
				break;

			/* If this failed the lock has changed, restart. */
			continue;
		}

		/*
		 * If we caught a signal, we have retried and now
		 * exit immediately.
		 */
		if (error != 0)
			break;

		if ((error = umtx_key_get(umtx, TYPE_SIMPLE_LOCK,
			AUTO_SHARE, &uq->uq_key)) != 0)
			return (error);

		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_insert(uq);
		umtxq_unbusy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);

		/*
		 * Set the contested bit so that a release in user space
		 * knows to use the system call for unlock.  If this fails
		 * either some one else has acquired the lock or it has been
		 * released.
		 */
		old = casuword(&umtx->u_owner, owner, owner | UMTX_CONTESTED);

		/* The address was invalid. */
		if (old == -1) {
			umtxq_lock(&uq->uq_key);
			umtxq_remove(uq);
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
		if (old == owner)
			error = umtxq_sleep(uq, "umtx", timeout == NULL ? NULL :
			    &timo);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
		umtx_key_release(&uq->uq_key);

		if (error == 0)
			error = thread_check_susp(td, false);
	}

	if (timeout == NULL) {
		/* Mutex locking is restarted if it is interrupted. */
		if (error == EINTR)
			error = ERESTART;
	} else {
		/* Timed-locking is not restarted. */
		if (error == ERESTART)
			error = EINTR;
	}
	return (error);
}

/*
 * Unlock a umtx object.
 */
static int
do_unlock_umtx(struct thread *td, struct umtx *umtx, u_long id)
{
	struct umtx_key key;
	u_long owner;
	u_long old;
	int error;
	int count;

	/*
	 * Make sure we own this mtx.
	 */
	owner = fuword(__DEVOLATILE(u_long *, &umtx->u_owner));
	if (owner == -1)
		return (EFAULT);

	if ((owner & ~UMTX_CONTESTED) != id)
		return (EPERM);

	/* This should be done in userland */
	if ((owner & UMTX_CONTESTED) == 0) {
		old = casuword(&umtx->u_owner, owner, UMTX_UNOWNED);
		if (old == -1)
			return (EFAULT);
		if (old == owner)
			return (0);
		owner = old;
	}

	/* We should only ever be in here for contested locks */
	if ((error = umtx_key_get(umtx, TYPE_SIMPLE_LOCK, AUTO_SHARE,
	    &key)) != 0)
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
	old = casuword(&umtx->u_owner, owner,
	    count <= 1 ? UMTX_UNOWNED : UMTX_CONTESTED);
	umtxq_lock(&key);
	umtxq_signal(&key,1);
	umtxq_unbusy(&key);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	if (old == -1)
		return (EFAULT);
	if (old != owner)
		return (EINVAL);
	return (0);
}

#ifdef COMPAT_FREEBSD32

/*
 * Lock a umtx object.
 */
static int
do_lock_umtx32(struct thread *td, uint32_t *m, uint32_t id,
	const struct timespec *timeout)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	uint32_t owner;
	uint32_t old;
	int error = 0;

	uq = td->td_umtxq;

	if (timeout != NULL)
		umtx_abs_timeout_init(&timo, CLOCK_REALTIME, 0, timeout);

	/*
	 * Care must be exercised when dealing with umtx structure. It
	 * can fault on any access.
	 */
	for (;;) {
		/*
		 * Try the uncontested case.  This should be done in userland.
		 */
		owner = casuword32(m, UMUTEX_UNOWNED, id);

		/* The acquire succeeded. */
		if (owner == UMUTEX_UNOWNED)
			return (0);

		/* The address was invalid. */
		if (owner == -1)
			return (EFAULT);

		/* If no one owns it but it is contested try to acquire it. */
		if (owner == UMUTEX_CONTESTED) {
			owner = casuword32(m,
			    UMUTEX_CONTESTED, id | UMUTEX_CONTESTED);
			if (owner == UMUTEX_CONTESTED)
				return (0);

			/* The address was invalid. */
			if (owner == -1)
				return (EFAULT);

			error = thread_check_susp(td, false);
			if (error != 0)
				break;

			/* If this failed the lock has changed, restart. */
			continue;
		}

		/*
		 * If we caught a signal, we have retried and now
		 * exit immediately.
		 */
		if (error != 0)
			return (error);

		if ((error = umtx_key_get(m, TYPE_SIMPLE_LOCK,
			AUTO_SHARE, &uq->uq_key)) != 0)
			return (error);

		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_insert(uq);
		umtxq_unbusy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);

		/*
		 * Set the contested bit so that a release in user space
		 * knows to use the system call for unlock.  If this fails
		 * either some one else has acquired the lock or it has been
		 * released.
		 */
		old = casuword32(m, owner, owner | UMUTEX_CONTESTED);

		/* The address was invalid. */
		if (old == -1) {
			umtxq_lock(&uq->uq_key);
			umtxq_remove(uq);
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
		if (old == owner)
			error = umtxq_sleep(uq, "umtx", timeout == NULL ?
			    NULL : &timo);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
		umtx_key_release(&uq->uq_key);

		if (error == 0)
			error = thread_check_susp(td, false);
	}

	if (timeout == NULL) {
		/* Mutex locking is restarted if it is interrupted. */
		if (error == EINTR)
			error = ERESTART;
	} else {
		/* Timed-locking is not restarted. */
		if (error == ERESTART)
			error = EINTR;
	}
	return (error);
}

/*
 * Unlock a umtx object.
 */
static int
do_unlock_umtx32(struct thread *td, uint32_t *m, uint32_t id)
{
	struct umtx_key key;
	uint32_t owner;
	uint32_t old;
	int error;
	int count;

	/*
	 * Make sure we own this mtx.
	 */
	owner = fuword32(m);
	if (owner == -1)
		return (EFAULT);

	if ((owner & ~UMUTEX_CONTESTED) != id)
		return (EPERM);

	/* This should be done in userland */
	if ((owner & UMUTEX_CONTESTED) == 0) {
		old = casuword32(m, owner, UMUTEX_UNOWNED);
		if (old == -1)
			return (EFAULT);
		if (old == owner)
			return (0);
		owner = old;
	}

	/* We should only ever be in here for contested locks */
	if ((error = umtx_key_get(m, TYPE_SIMPLE_LOCK, AUTO_SHARE,
		&key)) != 0)
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
	old = casuword32(m, owner,
		count <= 1 ? UMUTEX_UNOWNED : UMUTEX_CONTESTED);
	umtxq_lock(&key);
	umtxq_signal(&key,1);
	umtxq_unbusy(&key);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	if (old == -1)
		return (EFAULT);
	if (old != owner)
		return (EINVAL);
	return (0);
}
#endif	/* COMPAT_FREEBSD32 */
#endif	/* COMPAT_FREEBSD10 */

/*
 * Fetch and compare value, sleep on the address if value is not changed.
 */
static int
do_wait(struct thread *td, void *addr, u_long id,
    struct _umtx_time *timeout, int compat32, int is_private)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	u_long tmp;
	uint32_t tmp32;
	int error = 0;

	uq = td->td_umtxq;
	if ((error = umtx_key_get(addr, TYPE_SIMPLE_WAIT,
	    is_private ? THREAD_SHARE : AUTO_SHARE, &uq->uq_key)) != 0)
		return (error);

	if (timeout != NULL)
		umtx_abs_timeout_init2(&timo, timeout);

	umtxq_lock(&uq->uq_key);
	umtxq_insert(uq);
	umtxq_unlock(&uq->uq_key);
	if (compat32 == 0) {
		error = fueword(addr, &tmp);
		if (error != 0)
			error = EFAULT;
	} else {
		error = fueword32(addr, &tmp32);
		if (error == 0)
			tmp = tmp32;
		else
			error = EFAULT;
	}
	umtxq_lock(&uq->uq_key);
	if (error == 0) {
		if (tmp == id)
			error = umtxq_sleep(uq, "uwait", timeout == NULL ?
			    NULL : &timo);
		if ((uq->uq_flags & UQF_UMTXQ) == 0)
			error = 0;
		else
			umtxq_remove(uq);
	} else if ((uq->uq_flags & UQF_UMTXQ) != 0) {
		umtxq_remove(uq);
	}
	umtxq_unlock(&uq->uq_key);
	umtx_key_release(&uq->uq_key);
	if (error == ERESTART)
		error = EINTR;
	return (error);
}

/*
 * Wake up threads sleeping on the specified address.
 */
int
kern_umtx_wake(struct thread *td, void *uaddr, int n_wake, int is_private)
{
	struct umtx_key key;
	int ret;

	if ((ret = umtx_key_get(uaddr, TYPE_SIMPLE_WAIT,
	    is_private ? THREAD_SHARE : AUTO_SHARE, &key)) != 0)
		return (ret);
	umtxq_lock(&key);
	umtxq_signal(&key, n_wake);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	return (0);
}

/*
 * Lock PTHREAD_PRIO_NONE protocol POSIX mutex.
 */
static int
do_lock_normal(struct thread *td, struct umutex *m, uint32_t flags,
    struct _umtx_time *timeout, int mode)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	uint32_t owner, old, id;
	int error, rv;

	id = td->td_tid;
	uq = td->td_umtxq;
	error = 0;
	if (timeout != NULL)
		umtx_abs_timeout_init2(&timo, timeout);

	/*
	 * Care must be exercised when dealing with umtx structure. It
	 * can fault on any access.
	 */
	for (;;) {
		rv = fueword32(&m->m_owner, &owner);
		if (rv == -1)
			return (EFAULT);
		if (mode == _UMUTEX_WAIT) {
			if (owner == UMUTEX_UNOWNED ||
			    owner == UMUTEX_CONTESTED ||
			    owner == UMUTEX_RB_OWNERDEAD ||
			    owner == UMUTEX_RB_NOTRECOV)
				return (0);
		} else {
			/*
			 * Robust mutex terminated.  Kernel duty is to
			 * return EOWNERDEAD to the userspace.  The
			 * umutex.m_flags UMUTEX_NONCONSISTENT is set
			 * by the common userspace code.
			 */
			if (owner == UMUTEX_RB_OWNERDEAD) {
				rv = casueword32(&m->m_owner,
				    UMUTEX_RB_OWNERDEAD, &owner,
				    id | UMUTEX_CONTESTED);
				if (rv == -1)
					return (EFAULT);
				if (rv == 0) {
					MPASS(owner == UMUTEX_RB_OWNERDEAD);
					return (EOWNERDEAD); /* success */
				}
				MPASS(rv == 1);
				rv = thread_check_susp(td, false);
				if (rv != 0)
					return (rv);
				continue;
			}
			if (owner == UMUTEX_RB_NOTRECOV)
				return (ENOTRECOVERABLE);

			/*
			 * Try the uncontested case.  This should be
			 * done in userland.
			 */
			rv = casueword32(&m->m_owner, UMUTEX_UNOWNED,
			    &owner, id);
			/* The address was invalid. */
			if (rv == -1)
				return (EFAULT);

			/* The acquire succeeded. */
			if (rv == 0) {
				MPASS(owner == UMUTEX_UNOWNED);
				return (0);
			}

			/*
			 * If no one owns it but it is contested try
			 * to acquire it.
			 */
			MPASS(rv == 1);
			if (owner == UMUTEX_CONTESTED) {
				rv = casueword32(&m->m_owner,
				    UMUTEX_CONTESTED, &owner,
				    id | UMUTEX_CONTESTED);
				/* The address was invalid. */
				if (rv == -1)
					return (EFAULT);
				if (rv == 0) {
					MPASS(owner == UMUTEX_CONTESTED);
					return (0);
				}
				if (rv == 1) {
					rv = thread_check_susp(td, false);
					if (rv != 0)
						return (rv);
				}

				/*
				 * If this failed the lock has
				 * changed, restart.
				 */
				continue;
			}

			/* rv == 1 but not contested, likely store failure */
			rv = thread_check_susp(td, false);
			if (rv != 0)
				return (rv);
		}

		if (mode == _UMUTEX_TRY)
			return (EBUSY);

		/*
		 * If we caught a signal, we have retried and now
		 * exit immediately.
		 */
		if (error != 0)
			return (error);

		if ((error = umtx_key_get(m, TYPE_NORMAL_UMUTEX,
		    GET_SHARE(flags), &uq->uq_key)) != 0)
			return (error);

		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_insert(uq);
		umtxq_unlock(&uq->uq_key);

		/*
		 * Set the contested bit so that a release in user space
		 * knows to use the system call for unlock.  If this fails
		 * either some one else has acquired the lock or it has been
		 * released.
		 */
		rv = casueword32(&m->m_owner, owner, &old,
		    owner | UMUTEX_CONTESTED);

		/* The address was invalid or casueword failed to store. */
		if (rv == -1 || rv == 1) {
			umtxq_lock(&uq->uq_key);
			umtxq_remove(uq);
			umtxq_unbusy(&uq->uq_key);
			umtxq_unlock(&uq->uq_key);
			umtx_key_release(&uq->uq_key);
			if (rv == -1)
				return (EFAULT);
			if (rv == 1) {
				rv = thread_check_susp(td, false);
				if (rv != 0)
					return (rv);
			}
			continue;
		}

		/*
		 * We set the contested bit, sleep. Otherwise the lock changed
		 * and we need to retry or we lost a race to the thread
		 * unlocking the umtx.
		 */
		umtxq_lock(&uq->uq_key);
		umtxq_unbusy(&uq->uq_key);
		MPASS(old == owner);
		error = umtxq_sleep(uq, "umtxn", timeout == NULL ?
		    NULL : &timo);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
		umtx_key_release(&uq->uq_key);

		if (error == 0)
			error = thread_check_susp(td, false);
	}

	return (0);
}

/*
 * Unlock PTHREAD_PRIO_NONE protocol POSIX mutex.
 */
static int
do_unlock_normal(struct thread *td, struct umutex *m, uint32_t flags, bool rb)
{
	struct umtx_key key;
	uint32_t owner, old, id, newlock;
	int error, count;

	id = td->td_tid;

again:
	/*
	 * Make sure we own this mtx.
	 */
	error = fueword32(&m->m_owner, &owner);
	if (error == -1)
		return (EFAULT);

	if ((owner & ~UMUTEX_CONTESTED) != id)
		return (EPERM);

	newlock = umtx_unlock_val(flags, rb);
	if ((owner & UMUTEX_CONTESTED) == 0) {
		error = casueword32(&m->m_owner, owner, &old, newlock);
		if (error == -1)
			return (EFAULT);
		if (error == 1) {
			error = thread_check_susp(td, false);
			if (error != 0)
				return (error);
			goto again;
		}
		MPASS(old == owner);
		return (0);
	}

	/* We should only ever be in here for contested locks */
	if ((error = umtx_key_get(m, TYPE_NORMAL_UMUTEX, GET_SHARE(flags),
	    &key)) != 0)
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
	if (count > 1)
		newlock |= UMUTEX_CONTESTED;
	error = casueword32(&m->m_owner, owner, &old, newlock);
	umtxq_lock(&key);
	umtxq_signal(&key, 1);
	umtxq_unbusy(&key);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	if (error == -1)
		return (EFAULT);
	if (error == 1) {
		if (old != owner)
			return (EINVAL);
		error = thread_check_susp(td, false);
		if (error != 0)
			return (error);
		goto again;
	}
	return (0);
}

/*
 * Check if the mutex is available and wake up a waiter,
 * only for simple mutex.
 */
static int
do_wake_umutex(struct thread *td, struct umutex *m)
{
	struct umtx_key key;
	uint32_t owner;
	uint32_t flags;
	int error;
	int count;

again:
	error = fueword32(&m->m_owner, &owner);
	if (error == -1)
		return (EFAULT);

	if ((owner & ~UMUTEX_CONTESTED) != 0 && owner != UMUTEX_RB_OWNERDEAD &&
	    owner != UMUTEX_RB_NOTRECOV)
		return (0);

	error = fueword32(&m->m_flags, &flags);
	if (error == -1)
		return (EFAULT);

	/* We should only ever be in here for contested locks */
	if ((error = umtx_key_get(m, TYPE_NORMAL_UMUTEX, GET_SHARE(flags),
	    &key)) != 0)
		return (error);

	umtxq_lock(&key);
	umtxq_busy(&key);
	count = umtxq_count(&key);
	umtxq_unlock(&key);

	if (count <= 1 && owner != UMUTEX_RB_OWNERDEAD &&
	    owner != UMUTEX_RB_NOTRECOV) {
		error = casueword32(&m->m_owner, UMUTEX_CONTESTED, &owner,
		    UMUTEX_UNOWNED);
		if (error == -1) {
			error = EFAULT;
		} else if (error == 1) {
			umtxq_lock(&key);
			umtxq_unbusy(&key);
			umtxq_unlock(&key);
			umtx_key_release(&key);
			error = thread_check_susp(td, false);
			if (error != 0)
				return (error);
			goto again;
		}
	}

	umtxq_lock(&key);
	if (error == 0 && count != 0) {
		MPASS((owner & ~UMUTEX_CONTESTED) == 0 ||
		    owner == UMUTEX_RB_OWNERDEAD ||
		    owner == UMUTEX_RB_NOTRECOV);
		umtxq_signal(&key, 1);
	}
	umtxq_unbusy(&key);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	return (error);
}

/*
 * Check if the mutex has waiters and tries to fix contention bit.
 */
static int
do_wake2_umutex(struct thread *td, struct umutex *m, uint32_t flags)
{
	struct umtx_key key;
	uint32_t owner, old;
	int type;
	int error;
	int count;

	switch (flags & (UMUTEX_PRIO_INHERIT | UMUTEX_PRIO_PROTECT |
	    UMUTEX_ROBUST)) {
	case 0:
	case UMUTEX_ROBUST:
		type = TYPE_NORMAL_UMUTEX;
		break;
	case UMUTEX_PRIO_INHERIT:
		type = TYPE_PI_UMUTEX;
		break;
	case (UMUTEX_PRIO_INHERIT | UMUTEX_ROBUST):
		type = TYPE_PI_ROBUST_UMUTEX;
		break;
	case UMUTEX_PRIO_PROTECT:
		type = TYPE_PP_UMUTEX;
		break;
	case (UMUTEX_PRIO_PROTECT | UMUTEX_ROBUST):
		type = TYPE_PP_ROBUST_UMUTEX;
		break;
	default:
		return (EINVAL);
	}
	if ((error = umtx_key_get(m, type, GET_SHARE(flags), &key)) != 0)
		return (error);

	owner = 0;
	umtxq_lock(&key);
	umtxq_busy(&key);
	count = umtxq_count(&key);
	umtxq_unlock(&key);

	error = fueword32(&m->m_owner, &owner);
	if (error == -1)
		error = EFAULT;

	/*
	 * Only repair contention bit if there is a waiter, this means
	 * the mutex is still being referenced by userland code,
	 * otherwise don't update any memory.
	 */
	while (error == 0 && (owner & UMUTEX_CONTESTED) == 0 &&
	    (count > 1 || (count == 1 && (owner & ~UMUTEX_CONTESTED) != 0))) {
		error = casueword32(&m->m_owner, owner, &old,
		    owner | UMUTEX_CONTESTED);
		if (error == -1) {
			error = EFAULT;
			break;
		}
		if (error == 0) {
			MPASS(old == owner);
			break;
		}
		owner = old;
		error = thread_check_susp(td, false);
	}

	umtxq_lock(&key);
	if (error == EFAULT) {
		umtxq_signal(&key, INT_MAX);
	} else if (count != 0 && ((owner & ~UMUTEX_CONTESTED) == 0 ||
	    owner == UMUTEX_RB_OWNERDEAD || owner == UMUTEX_RB_NOTRECOV))
		umtxq_signal(&key, 1);
	umtxq_unbusy(&key);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	return (error);
}

struct umtx_pi *
umtx_pi_alloc(int flags)
{
	struct umtx_pi *pi;

	pi = uma_zalloc(umtx_pi_zone, M_ZERO | flags);
	TAILQ_INIT(&pi->pi_blocked);
	atomic_add_int(&umtx_pi_allocated, 1);
	return (pi);
}

void
umtx_pi_free(struct umtx_pi *pi)
{
	uma_zfree(umtx_pi_zone, pi);
	atomic_add_int(&umtx_pi_allocated, -1);
}

/*
 * Adjust the thread's position on a pi_state after its priority has been
 * changed.
 */
static int
umtx_pi_adjust_thread(struct umtx_pi *pi, struct thread *td)
{
	struct umtx_q *uq, *uq1, *uq2;
	struct thread *td1;

	mtx_assert(&umtx_lock, MA_OWNED);
	if (pi == NULL)
		return (0);

	uq = td->td_umtxq;

	/*
	 * Check if the thread needs to be moved on the blocked chain.
	 * It needs to be moved if either its priority is lower than
	 * the previous thread or higher than the next thread.
	 */
	uq1 = TAILQ_PREV(uq, umtxq_head, uq_lockq);
	uq2 = TAILQ_NEXT(uq, uq_lockq);
	if ((uq1 != NULL && UPRI(td) < UPRI(uq1->uq_thread)) ||
	    (uq2 != NULL && UPRI(td) > UPRI(uq2->uq_thread))) {
		/*
		 * Remove thread from blocked chain and determine where
		 * it should be moved to.
		 */
		TAILQ_REMOVE(&pi->pi_blocked, uq, uq_lockq);
		TAILQ_FOREACH(uq1, &pi->pi_blocked, uq_lockq) {
			td1 = uq1->uq_thread;
			MPASS(td1->td_proc->p_magic == P_MAGIC);
			if (UPRI(td1) > UPRI(td))
				break;
		}

		if (uq1 == NULL)
			TAILQ_INSERT_TAIL(&pi->pi_blocked, uq, uq_lockq);
		else
			TAILQ_INSERT_BEFORE(uq1, uq, uq_lockq);
	}
	return (1);
}

static struct umtx_pi *
umtx_pi_next(struct umtx_pi *pi)
{
	struct umtx_q *uq_owner;

	if (pi->pi_owner == NULL)
		return (NULL);
	uq_owner = pi->pi_owner->td_umtxq;
	if (uq_owner == NULL)
		return (NULL);
	return (uq_owner->uq_pi_blocked);
}

/*
 * Floyd's Cycle-Finding Algorithm.
 */
static bool
umtx_pi_check_loop(struct umtx_pi *pi)
{
	struct umtx_pi *pi1;	/* fast iterator */

	mtx_assert(&umtx_lock, MA_OWNED);
	if (pi == NULL)
		return (false);
	pi1 = pi;
	for (;;) {
		pi = umtx_pi_next(pi);
		if (pi == NULL)
			break;
		pi1 = umtx_pi_next(pi1);
		if (pi1 == NULL)
			break;
		pi1 = umtx_pi_next(pi1);
		if (pi1 == NULL)
			break;
		if (pi == pi1)
			return (true);
	}
	return (false);
}

/*
 * Propagate priority when a thread is blocked on POSIX
 * PI mutex.
 */
static void
umtx_propagate_priority(struct thread *td)
{
	struct umtx_q *uq;
	struct umtx_pi *pi;
	int pri;

	mtx_assert(&umtx_lock, MA_OWNED);
	pri = UPRI(td);
	uq = td->td_umtxq;
	pi = uq->uq_pi_blocked;
	if (pi == NULL)
		return;
	if (umtx_pi_check_loop(pi))
		return;

	for (;;) {
		td = pi->pi_owner;
		if (td == NULL || td == curthread)
			return;

		MPASS(td->td_proc != NULL);
		MPASS(td->td_proc->p_magic == P_MAGIC);

		thread_lock(td);
		if (td->td_lend_user_pri > pri)
			sched_lend_user_prio(td, pri);
		else {
			thread_unlock(td);
			break;
		}
		thread_unlock(td);

		/*
		 * Pick up the lock that td is blocked on.
		 */
		uq = td->td_umtxq;
		pi = uq->uq_pi_blocked;
		if (pi == NULL)
			break;
		/* Resort td on the list if needed. */
		umtx_pi_adjust_thread(pi, td);
	}
}

/*
 * Unpropagate priority for a PI mutex when a thread blocked on
 * it is interrupted by signal or resumed by others.
 */
static void
umtx_repropagate_priority(struct umtx_pi *pi)
{
	struct umtx_q *uq, *uq_owner;
	struct umtx_pi *pi2;
	int pri;

	mtx_assert(&umtx_lock, MA_OWNED);

	if (umtx_pi_check_loop(pi))
		return;
	while (pi != NULL && pi->pi_owner != NULL) {
		pri = PRI_MAX;
		uq_owner = pi->pi_owner->td_umtxq;

		TAILQ_FOREACH(pi2, &uq_owner->uq_pi_contested, pi_link) {
			uq = TAILQ_FIRST(&pi2->pi_blocked);
			if (uq != NULL) {
				if (pri > UPRI(uq->uq_thread))
					pri = UPRI(uq->uq_thread);
			}
		}

		if (pri > uq_owner->uq_inherited_pri)
			pri = uq_owner->uq_inherited_pri;
		thread_lock(pi->pi_owner);
		sched_lend_user_prio(pi->pi_owner, pri);
		thread_unlock(pi->pi_owner);
		if ((pi = uq_owner->uq_pi_blocked) != NULL)
			umtx_pi_adjust_thread(pi, uq_owner->uq_thread);
	}
}

/*
 * Insert a PI mutex into owned list.
 */
static void
umtx_pi_setowner(struct umtx_pi *pi, struct thread *owner)
{
	struct umtx_q *uq_owner;

	uq_owner = owner->td_umtxq;
	mtx_assert(&umtx_lock, MA_OWNED);
	MPASS(pi->pi_owner == NULL);
	pi->pi_owner = owner;
	TAILQ_INSERT_TAIL(&uq_owner->uq_pi_contested, pi, pi_link);
}

/*
 * Disown a PI mutex, and remove it from the owned list.
 */
static void
umtx_pi_disown(struct umtx_pi *pi)
{

	mtx_assert(&umtx_lock, MA_OWNED);
	TAILQ_REMOVE(&pi->pi_owner->td_umtxq->uq_pi_contested, pi, pi_link);
	pi->pi_owner = NULL;
}

/*
 * Claim ownership of a PI mutex.
 */
int
umtx_pi_claim(struct umtx_pi *pi, struct thread *owner)
{
	struct umtx_q *uq;
	int pri;

	mtx_lock(&umtx_lock);
	if (pi->pi_owner == owner) {
		mtx_unlock(&umtx_lock);
		return (0);
	}

	if (pi->pi_owner != NULL) {
		/*
		 * userland may have already messed the mutex, sigh.
		 */
		mtx_unlock(&umtx_lock);
		return (EPERM);
	}
	umtx_pi_setowner(pi, owner);
	uq = TAILQ_FIRST(&pi->pi_blocked);
	if (uq != NULL) {
		pri = UPRI(uq->uq_thread);
		thread_lock(owner);
		if (pri < UPRI(owner))
			sched_lend_user_prio(owner, pri);
		thread_unlock(owner);
	}
	mtx_unlock(&umtx_lock);
	return (0);
}

/*
 * Adjust a thread's order position in its blocked PI mutex,
 * this may result new priority propagating process.
 */
void
umtx_pi_adjust(struct thread *td, u_char oldpri)
{
	struct umtx_q *uq;
	struct umtx_pi *pi;

	uq = td->td_umtxq;
	mtx_lock(&umtx_lock);
	/*
	 * Pick up the lock that td is blocked on.
	 */
	pi = uq->uq_pi_blocked;
	if (pi != NULL) {
		umtx_pi_adjust_thread(pi, td);
		umtx_repropagate_priority(pi);
	}
	mtx_unlock(&umtx_lock);
}

/*
 * Sleep on a PI mutex.
 */
int
umtxq_sleep_pi(struct umtx_q *uq, struct umtx_pi *pi, uint32_t owner,
    const char *wmesg, struct umtx_abs_timeout *timo, bool shared)
{
	struct thread *td, *td1;
	struct umtx_q *uq1;
	int error, pri;
#ifdef INVARIANTS
	struct umtxq_chain *uc;

	uc = umtxq_getchain(&pi->pi_key);
#endif
	error = 0;
	td = uq->uq_thread;
	KASSERT(td == curthread, ("inconsistent uq_thread"));
	UMTXQ_LOCKED_ASSERT(umtxq_getchain(&uq->uq_key));
	KASSERT(uc->uc_busy != 0, ("umtx chain is not busy"));
	umtxq_insert(uq);
	mtx_lock(&umtx_lock);
	if (pi->pi_owner == NULL) {
		mtx_unlock(&umtx_lock);
		td1 = tdfind(owner, shared ? -1 : td->td_proc->p_pid);
		mtx_lock(&umtx_lock);
		if (td1 != NULL) {
			if (pi->pi_owner == NULL)
				umtx_pi_setowner(pi, td1);
			PROC_UNLOCK(td1->td_proc);
		}
	}

	TAILQ_FOREACH(uq1, &pi->pi_blocked, uq_lockq) {
		pri = UPRI(uq1->uq_thread);
		if (pri > UPRI(td))
			break;
	}

	if (uq1 != NULL)
		TAILQ_INSERT_BEFORE(uq1, uq, uq_lockq);
	else
		TAILQ_INSERT_TAIL(&pi->pi_blocked, uq, uq_lockq);

	uq->uq_pi_blocked = pi;
	thread_lock(td);
	td->td_flags |= TDF_UPIBLOCKED;
	thread_unlock(td);
	umtx_propagate_priority(td);
	mtx_unlock(&umtx_lock);
	umtxq_unbusy(&uq->uq_key);

	error = umtxq_sleep(uq, wmesg, timo);
	umtxq_remove(uq);

	mtx_lock(&umtx_lock);
	uq->uq_pi_blocked = NULL;
	thread_lock(td);
	td->td_flags &= ~TDF_UPIBLOCKED;
	thread_unlock(td);
	TAILQ_REMOVE(&pi->pi_blocked, uq, uq_lockq);
	umtx_repropagate_priority(pi);
	mtx_unlock(&umtx_lock);
	umtxq_unlock(&uq->uq_key);

	return (error);
}

/*
 * Add reference count for a PI mutex.
 */
void
umtx_pi_ref(struct umtx_pi *pi)
{

	UMTXQ_LOCKED_ASSERT(umtxq_getchain(&pi->pi_key));
	pi->pi_refcount++;
}

/*
 * Decrease reference count for a PI mutex, if the counter
 * is decreased to zero, its memory space is freed.
 */
void
umtx_pi_unref(struct umtx_pi *pi)
{
	struct umtxq_chain *uc;

	uc = umtxq_getchain(&pi->pi_key);
	UMTXQ_LOCKED_ASSERT(uc);
	KASSERT(pi->pi_refcount > 0, ("invalid reference count"));
	if (--pi->pi_refcount == 0) {
		mtx_lock(&umtx_lock);
		if (pi->pi_owner != NULL)
			umtx_pi_disown(pi);
		KASSERT(TAILQ_EMPTY(&pi->pi_blocked),
			("blocked queue not empty"));
		mtx_unlock(&umtx_lock);
		TAILQ_REMOVE(&uc->uc_pi_list, pi, pi_hashlink);
		umtx_pi_free(pi);
	}
}

/*
 * Find a PI mutex in hash table.
 */
struct umtx_pi *
umtx_pi_lookup(struct umtx_key *key)
{
	struct umtxq_chain *uc;
	struct umtx_pi *pi;

	uc = umtxq_getchain(key);
	UMTXQ_LOCKED_ASSERT(uc);

	TAILQ_FOREACH(pi, &uc->uc_pi_list, pi_hashlink) {
		if (umtx_key_match(&pi->pi_key, key)) {
			return (pi);
		}
	}
	return (NULL);
}

/*
 * Insert a PI mutex into hash table.
 */
void
umtx_pi_insert(struct umtx_pi *pi)
{
	struct umtxq_chain *uc;

	uc = umtxq_getchain(&pi->pi_key);
	UMTXQ_LOCKED_ASSERT(uc);
	TAILQ_INSERT_TAIL(&uc->uc_pi_list, pi, pi_hashlink);
}

/*
 * Drop a PI mutex and wakeup a top waiter.
 */
int
umtx_pi_drop(struct thread *td, struct umtx_key *key, bool rb, int *count)
{
	struct umtx_q *uq_first, *uq_first2, *uq_me;
	struct umtx_pi *pi, *pi2;
	int pri;

	UMTXQ_ASSERT_LOCKED_BUSY(key);
	*count = umtxq_count_pi(key, &uq_first);
	if (uq_first != NULL) {
		mtx_lock(&umtx_lock);
		pi = uq_first->uq_pi_blocked;
		KASSERT(pi != NULL, ("pi == NULL?"));
		if (pi->pi_owner != td && !(rb && pi->pi_owner == NULL)) {
			mtx_unlock(&umtx_lock);
			/* userland messed the mutex */
			return (EPERM);
		}
		uq_me = td->td_umtxq;
		if (pi->pi_owner == td)
			umtx_pi_disown(pi);
		/* get highest priority thread which is still sleeping. */
		uq_first = TAILQ_FIRST(&pi->pi_blocked);
		while (uq_first != NULL &&
		    (uq_first->uq_flags & UQF_UMTXQ) == 0) {
			uq_first = TAILQ_NEXT(uq_first, uq_lockq);
		}
		pri = PRI_MAX;
		TAILQ_FOREACH(pi2, &uq_me->uq_pi_contested, pi_link) {
			uq_first2 = TAILQ_FIRST(&pi2->pi_blocked);
			if (uq_first2 != NULL) {
				if (pri > UPRI(uq_first2->uq_thread))
					pri = UPRI(uq_first2->uq_thread);
			}
		}
		thread_lock(td);
		sched_lend_user_prio(td, pri);
		thread_unlock(td);
		mtx_unlock(&umtx_lock);
		if (uq_first)
			umtxq_signal_thread(uq_first);
	} else {
		pi = umtx_pi_lookup(key);
		/*
		 * A umtx_pi can exist if a signal or timeout removed the
		 * last waiter from the umtxq, but there is still
		 * a thread in do_lock_pi() holding the umtx_pi.
		 */
		if (pi != NULL) {
			/*
			 * The umtx_pi can be unowned, such as when a thread
			 * has just entered do_lock_pi(), allocated the
			 * umtx_pi, and unlocked the umtxq.
			 * If the current thread owns it, it must disown it.
			 */
			mtx_lock(&umtx_lock);
			if (pi->pi_owner == td)
				umtx_pi_disown(pi);
			mtx_unlock(&umtx_lock);
		}
	}
	return (0);
}

/*
 * Lock a PI mutex.
 */
static int
do_lock_pi(struct thread *td, struct umutex *m, uint32_t flags,
    struct _umtx_time *timeout, int try)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	struct umtx_pi *pi, *new_pi;
	uint32_t id, old_owner, owner, old;
	int error, rv;

	id = td->td_tid;
	uq = td->td_umtxq;

	if ((error = umtx_key_get(m, (flags & UMUTEX_ROBUST) != 0 ?
	    TYPE_PI_ROBUST_UMUTEX : TYPE_PI_UMUTEX, GET_SHARE(flags),
	    &uq->uq_key)) != 0)
		return (error);

	if (timeout != NULL)
		umtx_abs_timeout_init2(&timo, timeout);

	umtxq_lock(&uq->uq_key);
	pi = umtx_pi_lookup(&uq->uq_key);
	if (pi == NULL) {
		new_pi = umtx_pi_alloc(M_NOWAIT);
		if (new_pi == NULL) {
			umtxq_unlock(&uq->uq_key);
			new_pi = umtx_pi_alloc(M_WAITOK);
			umtxq_lock(&uq->uq_key);
			pi = umtx_pi_lookup(&uq->uq_key);
			if (pi != NULL) {
				umtx_pi_free(new_pi);
				new_pi = NULL;
			}
		}
		if (new_pi != NULL) {
			new_pi->pi_key = uq->uq_key;
			umtx_pi_insert(new_pi);
			pi = new_pi;
		}
	}
	umtx_pi_ref(pi);
	umtxq_unlock(&uq->uq_key);

	/*
	 * Care must be exercised when dealing with umtx structure.  It
	 * can fault on any access.
	 */
	for (;;) {
		/*
		 * Try the uncontested case.  This should be done in userland.
		 */
		rv = casueword32(&m->m_owner, UMUTEX_UNOWNED, &owner, id);
		/* The address was invalid. */
		if (rv == -1) {
			error = EFAULT;
			break;
		}
		/* The acquire succeeded. */
		if (rv == 0) {
			MPASS(owner == UMUTEX_UNOWNED);
			error = 0;
			break;
		}

		if (owner == UMUTEX_RB_NOTRECOV) {
			error = ENOTRECOVERABLE;
			break;
		}

		/*
		 * Nobody owns it, but the acquire failed. This can happen
		 * with ll/sc atomics.
		 */
		if (owner == UMUTEX_UNOWNED) {
			error = thread_check_susp(td, true);
			if (error != 0)
				break;
			continue;
		}

		/*
		 * Avoid overwriting a possible error from sleep due
		 * to the pending signal with suspension check result.
		 */
		if (error == 0) {
			error = thread_check_susp(td, true);
			if (error != 0)
				break;
		}

		/* If no one owns it but it is contested try to acquire it. */
		if (owner == UMUTEX_CONTESTED || owner == UMUTEX_RB_OWNERDEAD) {
			old_owner = owner;
			rv = casueword32(&m->m_owner, owner, &owner,
			    id | UMUTEX_CONTESTED);
			/* The address was invalid. */
			if (rv == -1) {
				error = EFAULT;
				break;
			}
			if (rv == 1) {
				if (error == 0) {
					error = thread_check_susp(td, true);
					if (error != 0)
						break;
				}

				/*
				 * If this failed the lock could
				 * changed, restart.
				 */
				continue;
			}

			MPASS(rv == 0);
			MPASS(owner == old_owner);
			umtxq_lock(&uq->uq_key);
			umtxq_busy(&uq->uq_key);
			error = umtx_pi_claim(pi, td);
			umtxq_unbusy(&uq->uq_key);
			umtxq_unlock(&uq->uq_key);
			if (error != 0) {
				/*
				 * Since we're going to return an
				 * error, restore the m_owner to its
				 * previous, unowned state to avoid
				 * compounding the problem.
				 */
				(void)casuword32(&m->m_owner,
				    id | UMUTEX_CONTESTED, old_owner);
			}
			if (error == 0 && old_owner == UMUTEX_RB_OWNERDEAD)
				error = EOWNERDEAD;
			break;
		}

		if ((owner & ~UMUTEX_CONTESTED) == id) {
			error = EDEADLK;
			break;
		}

		if (try != 0) {
			error = EBUSY;
			break;
		}

		/*
		 * If we caught a signal, we have retried and now
		 * exit immediately.
		 */
		if (error != 0)
			break;

		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);

		/*
		 * Set the contested bit so that a release in user space
		 * knows to use the system call for unlock.  If this fails
		 * either some one else has acquired the lock or it has been
		 * released.
		 */
		rv = casueword32(&m->m_owner, owner, &old, owner |
		    UMUTEX_CONTESTED);

		/* The address was invalid. */
		if (rv == -1) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = EFAULT;
			break;
		}
		if (rv == 1) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = thread_check_susp(td, true);
			if (error != 0)
				break;

			/*
			 * The lock changed and we need to retry or we
			 * lost a race to the thread unlocking the
			 * umtx.  Note that the UMUTEX_RB_OWNERDEAD
			 * value for owner is impossible there.
			 */
			continue;
		}

		umtxq_lock(&uq->uq_key);

		/* We set the contested bit, sleep. */
		MPASS(old == owner);
		error = umtxq_sleep_pi(uq, pi, owner & ~UMUTEX_CONTESTED,
		    "umtxpi", timeout == NULL ? NULL : &timo,
		    (flags & USYNC_PROCESS_SHARED) != 0);
		if (error != 0)
			continue;

		error = thread_check_susp(td, false);
		if (error != 0)
			break;
	}

	umtxq_lock(&uq->uq_key);
	umtx_pi_unref(pi);
	umtxq_unlock(&uq->uq_key);

	umtx_key_release(&uq->uq_key);
	return (error);
}

/*
 * Unlock a PI mutex.
 */
static int
do_unlock_pi(struct thread *td, struct umutex *m, uint32_t flags, bool rb)
{
	struct umtx_key key;
	uint32_t id, new_owner, old, owner;
	int count, error;

	id = td->td_tid;

usrloop:
	/*
	 * Make sure we own this mtx.
	 */
	error = fueword32(&m->m_owner, &owner);
	if (error == -1)
		return (EFAULT);

	if ((owner & ~UMUTEX_CONTESTED) != id)
		return (EPERM);

	new_owner = umtx_unlock_val(flags, rb);

	/* This should be done in userland */
	if ((owner & UMUTEX_CONTESTED) == 0) {
		error = casueword32(&m->m_owner, owner, &old, new_owner);
		if (error == -1)
			return (EFAULT);
		if (error == 1) {
			error = thread_check_susp(td, true);
			if (error != 0)
				return (error);
			goto usrloop;
		}
		if (old == owner)
			return (0);
		owner = old;
	}

	/* We should only ever be in here for contested locks */
	if ((error = umtx_key_get(m, (flags & UMUTEX_ROBUST) != 0 ?
	    TYPE_PI_ROBUST_UMUTEX : TYPE_PI_UMUTEX, GET_SHARE(flags),
	    &key)) != 0)
		return (error);

	umtxq_lock(&key);
	umtxq_busy(&key);
	error = umtx_pi_drop(td, &key, rb, &count);
	if (error != 0) {
		umtxq_unbusy(&key);
		umtxq_unlock(&key);
		umtx_key_release(&key);
		/* userland messed the mutex */
		return (error);
	}
	umtxq_unlock(&key);

	/*
	 * When unlocking the umtx, it must be marked as unowned if
	 * there is zero or one thread only waiting for it.
	 * Otherwise, it must be marked as contested.
	 */

	if (count > 1)
		new_owner |= UMUTEX_CONTESTED;
again:
	error = casueword32(&m->m_owner, owner, &old, new_owner);
	if (error == 1) {
		error = thread_check_susp(td, false);
		if (error == 0)
			goto again;
	}
	umtxq_unbusy_unlocked(&key);
	umtx_key_release(&key);
	if (error == -1)
		return (EFAULT);
	if (error == 0 && old != owner)
		return (EINVAL);
	return (error);
}

/*
 * Lock a PP mutex.
 */
static int
do_lock_pp(struct thread *td, struct umutex *m, uint32_t flags,
    struct _umtx_time *timeout, int try)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq, *uq2;
	struct umtx_pi *pi;
	uint32_t ceiling;
	uint32_t owner, id;
	int error, pri, old_inherited_pri, new_pri, rv;
	bool su;

	id = td->td_tid;
	uq = td->td_umtxq;
	if ((error = umtx_key_get(m, (flags & UMUTEX_ROBUST) != 0 ?
	    TYPE_PP_ROBUST_UMUTEX : TYPE_PP_UMUTEX, GET_SHARE(flags),
	    &uq->uq_key)) != 0)
		return (error);

	if (timeout != NULL)
		umtx_abs_timeout_init2(&timo, timeout);

	su = (priv_check(td, PRIV_SCHED_RTPRIO) == 0);
	for (;;) {
		old_inherited_pri = uq->uq_inherited_pri;
		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);

		rv = fueword32(&m->m_ceilings[0], &ceiling);
		if (rv == -1) {
			error = EFAULT;
			goto out;
		}
		ceiling = RTP_PRIO_MAX - ceiling;
		if (ceiling > RTP_PRIO_MAX) {
			error = EINVAL;
			goto out;
		}
		new_pri = PRI_MIN_REALTIME + ceiling;

		if (td->td_base_user_pri < new_pri) {
			error = EINVAL;
			goto out;
		}
		if (su) {
			mtx_lock(&umtx_lock);
			if (new_pri < uq->uq_inherited_pri) {
				uq->uq_inherited_pri = new_pri;
				thread_lock(td);
				if (new_pri < UPRI(td))
					sched_lend_user_prio(td, new_pri);
				thread_unlock(td);
			}
			mtx_unlock(&umtx_lock);
		}

		rv = casueword32(&m->m_owner, UMUTEX_CONTESTED, &owner,
		    id | UMUTEX_CONTESTED);
		/* The address was invalid. */
		if (rv == -1) {
			error = EFAULT;
			break;
		}
		if (rv == 0) {
			MPASS(owner == UMUTEX_CONTESTED);
			error = 0;
			break;
		}
		/* rv == 1 */
		if (owner == UMUTEX_RB_OWNERDEAD) {
			rv = casueword32(&m->m_owner, UMUTEX_RB_OWNERDEAD,
			    &owner, id | UMUTEX_CONTESTED);
			if (rv == -1) {
				error = EFAULT;
				break;
			}
			if (rv == 0) {
				MPASS(owner == UMUTEX_RB_OWNERDEAD);
				error = EOWNERDEAD; /* success */
				break;
			}

			/*
			 *  rv == 1, only check for suspension if we
			 *  did not already catched a signal.  If we
			 *  get an error from the check, the same
			 *  condition is checked by the umtxq_sleep()
			 *  call below, so we should obliterate the
			 *  error to not skip the last loop iteration.
			 */
			if (error == 0) {
				error = thread_check_susp(td, false);
				if (error == 0) {
					if (try != 0)
						error = EBUSY;
					else
						continue;
				}
				error = 0;
			}
		} else if (owner == UMUTEX_RB_NOTRECOV) {
			error = ENOTRECOVERABLE;
		}

		if (try != 0)
			error = EBUSY;

		/*
		 * If we caught a signal, we have retried and now
		 * exit immediately.
		 */
		if (error != 0)
			break;

		umtxq_lock(&uq->uq_key);
		umtxq_insert(uq);
		umtxq_unbusy(&uq->uq_key);
		error = umtxq_sleep(uq, "umtxpp", timeout == NULL ?
		    NULL : &timo);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);

		mtx_lock(&umtx_lock);
		uq->uq_inherited_pri = old_inherited_pri;
		pri = PRI_MAX;
		TAILQ_FOREACH(pi, &uq->uq_pi_contested, pi_link) {
			uq2 = TAILQ_FIRST(&pi->pi_blocked);
			if (uq2 != NULL) {
				if (pri > UPRI(uq2->uq_thread))
					pri = UPRI(uq2->uq_thread);
			}
		}
		if (pri > uq->uq_inherited_pri)
			pri = uq->uq_inherited_pri;
		thread_lock(td);
		sched_lend_user_prio(td, pri);
		thread_unlock(td);
		mtx_unlock(&umtx_lock);
	}

	if (error != 0 && error != EOWNERDEAD) {
		mtx_lock(&umtx_lock);
		uq->uq_inherited_pri = old_inherited_pri;
		pri = PRI_MAX;
		TAILQ_FOREACH(pi, &uq->uq_pi_contested, pi_link) {
			uq2 = TAILQ_FIRST(&pi->pi_blocked);
			if (uq2 != NULL) {
				if (pri > UPRI(uq2->uq_thread))
					pri = UPRI(uq2->uq_thread);
			}
		}
		if (pri > uq->uq_inherited_pri)
			pri = uq->uq_inherited_pri;
		thread_lock(td);
		sched_lend_user_prio(td, pri);
		thread_unlock(td);
		mtx_unlock(&umtx_lock);
	}

out:
	umtxq_unbusy_unlocked(&uq->uq_key);
	umtx_key_release(&uq->uq_key);
	return (error);
}

/*
 * Unlock a PP mutex.
 */
static int
do_unlock_pp(struct thread *td, struct umutex *m, uint32_t flags, bool rb)
{
	struct umtx_key key;
	struct umtx_q *uq, *uq2;
	struct umtx_pi *pi;
	uint32_t id, owner, rceiling;
	int error, pri, new_inherited_pri;
	bool su;

	id = td->td_tid;
	uq = td->td_umtxq;
	su = (priv_check(td, PRIV_SCHED_RTPRIO) == 0);

	/*
	 * Make sure we own this mtx.
	 */
	error = fueword32(&m->m_owner, &owner);
	if (error == -1)
		return (EFAULT);

	if ((owner & ~UMUTEX_CONTESTED) != id)
		return (EPERM);

	error = copyin(&m->m_ceilings[1], &rceiling, sizeof(uint32_t));
	if (error != 0)
		return (error);

	if (rceiling == -1)
		new_inherited_pri = PRI_MAX;
	else {
		rceiling = RTP_PRIO_MAX - rceiling;
		if (rceiling > RTP_PRIO_MAX)
			return (EINVAL);
		new_inherited_pri = PRI_MIN_REALTIME + rceiling;
	}

	if ((error = umtx_key_get(m, (flags & UMUTEX_ROBUST) != 0 ?
	    TYPE_PP_ROBUST_UMUTEX : TYPE_PP_UMUTEX, GET_SHARE(flags),
	    &key)) != 0)
		return (error);
	umtxq_lock(&key);
	umtxq_busy(&key);
	umtxq_unlock(&key);
	/*
	 * For priority protected mutex, always set unlocked state
	 * to UMUTEX_CONTESTED, so that userland always enters kernel
	 * to lock the mutex, it is necessary because thread priority
	 * has to be adjusted for such mutex.
	 */
	error = suword32(&m->m_owner, umtx_unlock_val(flags, rb) |
	    UMUTEX_CONTESTED);

	umtxq_lock(&key);
	if (error == 0)
		umtxq_signal(&key, 1);
	umtxq_unbusy(&key);
	umtxq_unlock(&key);

	if (error == -1)
		error = EFAULT;
	else {
		mtx_lock(&umtx_lock);
		if (su || new_inherited_pri == PRI_MAX)
			uq->uq_inherited_pri = new_inherited_pri;
		pri = PRI_MAX;
		TAILQ_FOREACH(pi, &uq->uq_pi_contested, pi_link) {
			uq2 = TAILQ_FIRST(&pi->pi_blocked);
			if (uq2 != NULL) {
				if (pri > UPRI(uq2->uq_thread))
					pri = UPRI(uq2->uq_thread);
			}
		}
		if (pri > uq->uq_inherited_pri)
			pri = uq->uq_inherited_pri;
		thread_lock(td);
		sched_lend_user_prio(td, pri);
		thread_unlock(td);
		mtx_unlock(&umtx_lock);
	}
	umtx_key_release(&key);
	return (error);
}

static int
do_set_ceiling(struct thread *td, struct umutex *m, uint32_t ceiling,
    uint32_t *old_ceiling)
{
	struct umtx_q *uq;
	uint32_t flags, id, owner, save_ceiling;
	int error, rv, rv1;

	error = fueword32(&m->m_flags, &flags);
	if (error == -1)
		return (EFAULT);
	if ((flags & UMUTEX_PRIO_PROTECT) == 0)
		return (EINVAL);
	if (ceiling > RTP_PRIO_MAX)
		return (EINVAL);
	id = td->td_tid;
	uq = td->td_umtxq;
	if ((error = umtx_key_get(m, (flags & UMUTEX_ROBUST) != 0 ?
	    TYPE_PP_ROBUST_UMUTEX : TYPE_PP_UMUTEX, GET_SHARE(flags),
	    &uq->uq_key)) != 0)
		return (error);
	for (;;) {
		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);

		rv = fueword32(&m->m_ceilings[0], &save_ceiling);
		if (rv == -1) {
			error = EFAULT;
			break;
		}

		rv = casueword32(&m->m_owner, UMUTEX_CONTESTED, &owner,
		    id | UMUTEX_CONTESTED);
		if (rv == -1) {
			error = EFAULT;
			break;
		}

		if (rv == 0) {
			MPASS(owner == UMUTEX_CONTESTED);
			rv = suword32(&m->m_ceilings[0], ceiling);
			rv1 = suword32(&m->m_owner, UMUTEX_CONTESTED);
			error = (rv == 0 && rv1 == 0) ? 0: EFAULT;
			break;
		}

		if ((owner & ~UMUTEX_CONTESTED) == id) {
			rv = suword32(&m->m_ceilings[0], ceiling);
			error = rv == 0 ? 0 : EFAULT;
			break;
		}

		if (owner == UMUTEX_RB_OWNERDEAD) {
			error = EOWNERDEAD;
			break;
		} else if (owner == UMUTEX_RB_NOTRECOV) {
			error = ENOTRECOVERABLE;
			break;
		}

		/*
		 * If we caught a signal, we have retried and now
		 * exit immediately.
		 */
		if (error != 0)
			break;

		/*
		 * We set the contested bit, sleep. Otherwise the lock changed
		 * and we need to retry or we lost a race to the thread
		 * unlocking the umtx.
		 */
		umtxq_lock(&uq->uq_key);
		umtxq_insert(uq);
		umtxq_unbusy(&uq->uq_key);
		error = umtxq_sleep(uq, "umtxpp", NULL);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
	}
	umtxq_lock(&uq->uq_key);
	if (error == 0)
		umtxq_signal(&uq->uq_key, INT_MAX);
	umtxq_unbusy(&uq->uq_key);
	umtxq_unlock(&uq->uq_key);
	umtx_key_release(&uq->uq_key);
	if (error == 0 && old_ceiling != NULL) {
		rv = suword32(old_ceiling, save_ceiling);
		error = rv == 0 ? 0 : EFAULT;
	}
	return (error);
}

/*
 * Lock a userland POSIX mutex.
 */
static int
do_lock_umutex(struct thread *td, struct umutex *m,
    struct _umtx_time *timeout, int mode)
{
	uint32_t flags;
	int error;

	error = fueword32(&m->m_flags, &flags);
	if (error == -1)
		return (EFAULT);

	switch (flags & (UMUTEX_PRIO_INHERIT | UMUTEX_PRIO_PROTECT)) {
	case 0:
		error = do_lock_normal(td, m, flags, timeout, mode);
		break;
	case UMUTEX_PRIO_INHERIT:
		error = do_lock_pi(td, m, flags, timeout, mode);
		break;
	case UMUTEX_PRIO_PROTECT:
		error = do_lock_pp(td, m, flags, timeout, mode);
		break;
	default:
		return (EINVAL);
	}
	if (timeout == NULL) {
		if (error == EINTR && mode != _UMUTEX_WAIT)
			error = ERESTART;
	} else {
		/* Timed-locking is not restarted. */
		if (error == ERESTART)
			error = EINTR;
	}
	return (error);
}

/*
 * Unlock a userland POSIX mutex.
 */
static int
do_unlock_umutex(struct thread *td, struct umutex *m, bool rb)
{
	uint32_t flags;
	int error;

	error = fueword32(&m->m_flags, &flags);
	if (error == -1)
		return (EFAULT);

	switch (flags & (UMUTEX_PRIO_INHERIT | UMUTEX_PRIO_PROTECT)) {
	case 0:
		return (do_unlock_normal(td, m, flags, rb));
	case UMUTEX_PRIO_INHERIT:
		return (do_unlock_pi(td, m, flags, rb));
	case UMUTEX_PRIO_PROTECT:
		return (do_unlock_pp(td, m, flags, rb));
	}

	return (EINVAL);
}

static int
do_cv_wait(struct thread *td, struct ucond *cv, struct umutex *m,
    struct timespec *timeout, u_long wflags)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	uint32_t flags, clockid, hasw;
	int error;

	uq = td->td_umtxq;
	error = fueword32(&cv->c_flags, &flags);
	if (error == -1)
		return (EFAULT);
	error = umtx_key_get(cv, TYPE_CV, GET_SHARE(flags), &uq->uq_key);
	if (error != 0)
		return (error);

	if ((wflags & CVWAIT_CLOCKID) != 0) {
		error = fueword32(&cv->c_clockid, &clockid);
		if (error == -1) {
			umtx_key_release(&uq->uq_key);
			return (EFAULT);
		}
		if (clockid < CLOCK_REALTIME ||
		    clockid >= CLOCK_THREAD_CPUTIME_ID) {
			/* hmm, only HW clock id will work. */
			umtx_key_release(&uq->uq_key);
			return (EINVAL);
		}
	} else {
		clockid = CLOCK_REALTIME;
	}

	umtxq_lock(&uq->uq_key);
	umtxq_busy(&uq->uq_key);
	umtxq_insert(uq);
	umtxq_unlock(&uq->uq_key);

	/*
	 * Set c_has_waiters to 1 before releasing user mutex, also
	 * don't modify cache line when unnecessary.
	 */
	error = fueword32(&cv->c_has_waiters, &hasw);
	if (error == 0 && hasw == 0)
		error = suword32(&cv->c_has_waiters, 1);
	if (error != 0) {
		umtxq_lock(&uq->uq_key);
		umtxq_remove(uq);
		umtxq_unbusy(&uq->uq_key);
		error = EFAULT;
		goto out;
	}

	umtxq_unbusy_unlocked(&uq->uq_key);

	error = do_unlock_umutex(td, m, false);

	if (timeout != NULL)
		umtx_abs_timeout_init(&timo, clockid,
		    (wflags & CVWAIT_ABSTIME) != 0, timeout);

	umtxq_lock(&uq->uq_key);
	if (error == 0) {
		error = umtxq_sleep(uq, "ucond", timeout == NULL ?
		    NULL : &timo);
	}

	if ((uq->uq_flags & UQF_UMTXQ) == 0)
		error = 0;
	else {
		/*
		 * This must be timeout,interrupted by signal or
		 * surprious wakeup, clear c_has_waiter flag when
		 * necessary.
		 */
		umtxq_busy(&uq->uq_key);
		if ((uq->uq_flags & UQF_UMTXQ) != 0) {
			int oldlen = uq->uq_cur_queue->length;
			umtxq_remove(uq);
			if (oldlen == 1) {
				umtxq_unlock(&uq->uq_key);
				if (suword32(&cv->c_has_waiters, 0) != 0 &&
				    error == 0)
					error = EFAULT;
				umtxq_lock(&uq->uq_key);
			}
		}
		umtxq_unbusy(&uq->uq_key);
		if (error == ERESTART)
			error = EINTR;
	}
out:
	umtxq_unlock(&uq->uq_key);
	umtx_key_release(&uq->uq_key);
	return (error);
}

/*
 * Signal a userland condition variable.
 */
static int
do_cv_signal(struct thread *td, struct ucond *cv)
{
	struct umtx_key key;
	int error, cnt, nwake;
	uint32_t flags;

	error = fueword32(&cv->c_flags, &flags);
	if (error == -1)
		return (EFAULT);
	if ((error = umtx_key_get(cv, TYPE_CV, GET_SHARE(flags), &key)) != 0)
		return (error);
	umtxq_lock(&key);
	umtxq_busy(&key);
	cnt = umtxq_count(&key);
	nwake = umtxq_signal(&key, 1);
	if (cnt <= nwake) {
		umtxq_unlock(&key);
		error = suword32(&cv->c_has_waiters, 0);
		if (error == -1)
			error = EFAULT;
		umtxq_lock(&key);
	}
	umtxq_unbusy(&key);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	return (error);
}

static int
do_cv_broadcast(struct thread *td, struct ucond *cv)
{
	struct umtx_key key;
	int error;
	uint32_t flags;

	error = fueword32(&cv->c_flags, &flags);
	if (error == -1)
		return (EFAULT);
	if ((error = umtx_key_get(cv, TYPE_CV, GET_SHARE(flags), &key)) != 0)
		return (error);

	umtxq_lock(&key);
	umtxq_busy(&key);
	umtxq_signal(&key, INT_MAX);
	umtxq_unlock(&key);

	error = suword32(&cv->c_has_waiters, 0);
	if (error == -1)
		error = EFAULT;

	umtxq_unbusy_unlocked(&key);

	umtx_key_release(&key);
	return (error);
}

static int
do_rw_rdlock(struct thread *td, struct urwlock *rwlock, long fflag,
    struct _umtx_time *timeout)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	uint32_t flags, wrflags;
	int32_t state, oldstate;
	int32_t blocked_readers;
	int error, error1, rv;

	uq = td->td_umtxq;
	error = fueword32(&rwlock->rw_flags, &flags);
	if (error == -1)
		return (EFAULT);
	error = umtx_key_get(rwlock, TYPE_RWLOCK, GET_SHARE(flags), &uq->uq_key);
	if (error != 0)
		return (error);

	if (timeout != NULL)
		umtx_abs_timeout_init2(&timo, timeout);

	wrflags = URWLOCK_WRITE_OWNER;
	if (!(fflag & URWLOCK_PREFER_READER) && !(flags & URWLOCK_PREFER_READER))
		wrflags |= URWLOCK_WRITE_WAITERS;

	for (;;) {
		rv = fueword32(&rwlock->rw_state, &state);
		if (rv == -1) {
			umtx_key_release(&uq->uq_key);
			return (EFAULT);
		}

		/* try to lock it */
		while (!(state & wrflags)) {
			if (__predict_false(URWLOCK_READER_COUNT(state) ==
			    URWLOCK_MAX_READERS)) {
				umtx_key_release(&uq->uq_key);
				return (EAGAIN);
			}
			rv = casueword32(&rwlock->rw_state, state,
			    &oldstate, state + 1);
			if (rv == -1) {
				umtx_key_release(&uq->uq_key);
				return (EFAULT);
			}
			if (rv == 0) {
				MPASS(oldstate == state);
				umtx_key_release(&uq->uq_key);
				return (0);
			}
			error = thread_check_susp(td, true);
			if (error != 0)
				break;
			state = oldstate;
		}

		if (error)
			break;

		/* grab monitor lock */
		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);

		/*
		 * re-read the state, in case it changed between the try-lock above
		 * and the check below
		 */
		rv = fueword32(&rwlock->rw_state, &state);
		if (rv == -1)
			error = EFAULT;

		/* set read contention bit */
		while (error == 0 && (state & wrflags) &&
		    !(state & URWLOCK_READ_WAITERS)) {
			rv = casueword32(&rwlock->rw_state, state,
			    &oldstate, state | URWLOCK_READ_WAITERS);
			if (rv == -1) {
				error = EFAULT;
				break;
			}
			if (rv == 0) {
				MPASS(oldstate == state);
				goto sleep;
			}
			state = oldstate;
			error = thread_check_susp(td, false);
			if (error != 0)
				break;
		}
		if (error != 0) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			break;
		}

		/* state is changed while setting flags, restart */
		if (!(state & wrflags)) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = thread_check_susp(td, true);
			if (error != 0)
				break;
			continue;
		}

sleep:
		/*
		 * Contention bit is set, before sleeping, increase
		 * read waiter count.
		 */
		rv = fueword32(&rwlock->rw_blocked_readers,
		    &blocked_readers);
		if (rv == 0)
			rv = suword32(&rwlock->rw_blocked_readers,
			    blocked_readers + 1);
		if (rv == -1) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = EFAULT;
			break;
		}

		while (state & wrflags) {
			umtxq_lock(&uq->uq_key);
			umtxq_insert(uq);
			umtxq_unbusy(&uq->uq_key);

			error = umtxq_sleep(uq, "urdlck", timeout == NULL ?
			    NULL : &timo);

			umtxq_busy(&uq->uq_key);
			umtxq_remove(uq);
			umtxq_unlock(&uq->uq_key);
			if (error)
				break;
			rv = fueword32(&rwlock->rw_state, &state);
			if (rv == -1) {
				error = EFAULT;
				break;
			}
		}

		/* decrease read waiter count, and may clear read contention bit */
		rv = fueword32(&rwlock->rw_blocked_readers,
		    &blocked_readers);
		if (rv == 0)
			rv = suword32(&rwlock->rw_blocked_readers,
			    blocked_readers - 1);
		if (rv == -1) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = EFAULT;
			break;
		}
		if (blocked_readers == 1) {
			rv = fueword32(&rwlock->rw_state, &state);
			if (rv == -1) {
				umtxq_unbusy_unlocked(&uq->uq_key);
				error = EFAULT;
				break;
			}
			for (;;) {
				rv = casueword32(&rwlock->rw_state, state,
				    &oldstate, state & ~URWLOCK_READ_WAITERS);
				if (rv == -1) {
					error = EFAULT;
					break;
				}
				if (rv == 0) {
					MPASS(oldstate == state);
					break;
				}
				state = oldstate;
				error1 = thread_check_susp(td, false);
				if (error1 != 0) {
					if (error == 0)
						error = error1;
					break;
				}
			}
		}

		umtxq_unbusy_unlocked(&uq->uq_key);
		if (error != 0)
			break;
	}
	umtx_key_release(&uq->uq_key);
	if (error == ERESTART)
		error = EINTR;
	return (error);
}

static int
do_rw_wrlock(struct thread *td, struct urwlock *rwlock, struct _umtx_time *timeout)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	uint32_t flags;
	int32_t state, oldstate;
	int32_t blocked_writers;
	int32_t blocked_readers;
	int error, error1, rv;

	uq = td->td_umtxq;
	error = fueword32(&rwlock->rw_flags, &flags);
	if (error == -1)
		return (EFAULT);
	error = umtx_key_get(rwlock, TYPE_RWLOCK, GET_SHARE(flags), &uq->uq_key);
	if (error != 0)
		return (error);

	if (timeout != NULL)
		umtx_abs_timeout_init2(&timo, timeout);

	blocked_readers = 0;
	for (;;) {
		rv = fueword32(&rwlock->rw_state, &state);
		if (rv == -1) {
			umtx_key_release(&uq->uq_key);
			return (EFAULT);
		}
		while ((state & URWLOCK_WRITE_OWNER) == 0 &&
		    URWLOCK_READER_COUNT(state) == 0) {
			rv = casueword32(&rwlock->rw_state, state,
			    &oldstate, state | URWLOCK_WRITE_OWNER);
			if (rv == -1) {
				umtx_key_release(&uq->uq_key);
				return (EFAULT);
			}
			if (rv == 0) {
				MPASS(oldstate == state);
				umtx_key_release(&uq->uq_key);
				return (0);
			}
			state = oldstate;
			error = thread_check_susp(td, true);
			if (error != 0)
				break;
		}

		if (error) {
			if ((state & (URWLOCK_WRITE_OWNER |
			    URWLOCK_WRITE_WAITERS)) == 0 &&
			    blocked_readers != 0) {
				umtxq_lock(&uq->uq_key);
				umtxq_busy(&uq->uq_key);
				umtxq_signal_queue(&uq->uq_key, INT_MAX,
				    UMTX_SHARED_QUEUE);
				umtxq_unbusy(&uq->uq_key);
				umtxq_unlock(&uq->uq_key);
			}

			break;
		}

		/* grab monitor lock */
		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);

		/*
		 * Re-read the state, in case it changed between the
		 * try-lock above and the check below.
		 */
		rv = fueword32(&rwlock->rw_state, &state);
		if (rv == -1)
			error = EFAULT;

		while (error == 0 && ((state & URWLOCK_WRITE_OWNER) ||
		    URWLOCK_READER_COUNT(state) != 0) &&
		    (state & URWLOCK_WRITE_WAITERS) == 0) {
			rv = casueword32(&rwlock->rw_state, state,
			    &oldstate, state | URWLOCK_WRITE_WAITERS);
			if (rv == -1) {
				error = EFAULT;
				break;
			}
			if (rv == 0) {
				MPASS(oldstate == state);
				goto sleep;
			}
			state = oldstate;
			error = thread_check_susp(td, false);
			if (error != 0)
				break;
		}
		if (error != 0) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			break;
		}

		if ((state & URWLOCK_WRITE_OWNER) == 0 &&
		    URWLOCK_READER_COUNT(state) == 0) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = thread_check_susp(td, false);
			if (error != 0)
				break;
			continue;
		}
sleep:
		rv = fueword32(&rwlock->rw_blocked_writers,
		    &blocked_writers);
		if (rv == 0)
			rv = suword32(&rwlock->rw_blocked_writers,
			    blocked_writers + 1);
		if (rv == -1) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = EFAULT;
			break;
		}

		while ((state & URWLOCK_WRITE_OWNER) ||
		    URWLOCK_READER_COUNT(state) != 0) {
			umtxq_lock(&uq->uq_key);
			umtxq_insert_queue(uq, UMTX_EXCLUSIVE_QUEUE);
			umtxq_unbusy(&uq->uq_key);

			error = umtxq_sleep(uq, "uwrlck", timeout == NULL ?
			    NULL : &timo);

			umtxq_busy(&uq->uq_key);
			umtxq_remove_queue(uq, UMTX_EXCLUSIVE_QUEUE);
			umtxq_unlock(&uq->uq_key);
			if (error)
				break;
			rv = fueword32(&rwlock->rw_state, &state);
			if (rv == -1) {
				error = EFAULT;
				break;
			}
		}

		rv = fueword32(&rwlock->rw_blocked_writers,
		    &blocked_writers);
		if (rv == 0)
			rv = suword32(&rwlock->rw_blocked_writers,
			    blocked_writers - 1);
		if (rv == -1) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = EFAULT;
			break;
		}
		if (blocked_writers == 1) {
			rv = fueword32(&rwlock->rw_state, &state);
			if (rv == -1) {
				umtxq_unbusy_unlocked(&uq->uq_key);
				error = EFAULT;
				break;
			}
			for (;;) {
				rv = casueword32(&rwlock->rw_state, state,
				    &oldstate, state & ~URWLOCK_WRITE_WAITERS);
				if (rv == -1) {
					error = EFAULT;
					break;
				}
				if (rv == 0) {
					MPASS(oldstate == state);
					break;
				}
				state = oldstate;
				error1 = thread_check_susp(td, false);
				/*
				 * We are leaving the URWLOCK_WRITE_WAITERS
				 * behind, but this should not harm the
				 * correctness.
				 */
				if (error1 != 0) {
					if (error == 0)
						error = error1;
					break;
				}
			}
			rv = fueword32(&rwlock->rw_blocked_readers,
			    &blocked_readers);
			if (rv == -1) {
				umtxq_unbusy_unlocked(&uq->uq_key);
				error = EFAULT;
				break;
			}
		} else
			blocked_readers = 0;

		umtxq_unbusy_unlocked(&uq->uq_key);
	}

	umtx_key_release(&uq->uq_key);
	if (error == ERESTART)
		error = EINTR;
	return (error);
}

static int
do_rw_unlock(struct thread *td, struct urwlock *rwlock)
{
	struct umtx_q *uq;
	uint32_t flags;
	int32_t state, oldstate;
	int error, rv, q, count;

	uq = td->td_umtxq;
	error = fueword32(&rwlock->rw_flags, &flags);
	if (error == -1)
		return (EFAULT);
	error = umtx_key_get(rwlock, TYPE_RWLOCK, GET_SHARE(flags), &uq->uq_key);
	if (error != 0)
		return (error);

	error = fueword32(&rwlock->rw_state, &state);
	if (error == -1) {
		error = EFAULT;
		goto out;
	}
	if (state & URWLOCK_WRITE_OWNER) {
		for (;;) {
			rv = casueword32(&rwlock->rw_state, state,
			    &oldstate, state & ~URWLOCK_WRITE_OWNER);
			if (rv == -1) {
				error = EFAULT;
				goto out;
			}
			if (rv == 1) {
				state = oldstate;
				if (!(oldstate & URWLOCK_WRITE_OWNER)) {
					error = EPERM;
					goto out;
				}
				error = thread_check_susp(td, true);
				if (error != 0)
					goto out;
			} else
				break;
		}
	} else if (URWLOCK_READER_COUNT(state) != 0) {
		for (;;) {
			rv = casueword32(&rwlock->rw_state, state,
			    &oldstate, state - 1);
			if (rv == -1) {
				error = EFAULT;
				goto out;
			}
			if (rv == 1) {
				state = oldstate;
				if (URWLOCK_READER_COUNT(oldstate) == 0) {
					error = EPERM;
					goto out;
				}
				error = thread_check_susp(td, true);
				if (error != 0)
					goto out;
			} else
				break;
		}
	} else {
		error = EPERM;
		goto out;
	}

	count = 0;

	if (!(flags & URWLOCK_PREFER_READER)) {
		if (state & URWLOCK_WRITE_WAITERS) {
			count = 1;
			q = UMTX_EXCLUSIVE_QUEUE;
		} else if (state & URWLOCK_READ_WAITERS) {
			count = INT_MAX;
			q = UMTX_SHARED_QUEUE;
		}
	} else {
		if (state & URWLOCK_READ_WAITERS) {
			count = INT_MAX;
			q = UMTX_SHARED_QUEUE;
		} else if (state & URWLOCK_WRITE_WAITERS) {
			count = 1;
			q = UMTX_EXCLUSIVE_QUEUE;
		}
	}

	if (count) {
		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_signal_queue(&uq->uq_key, count, q);
		umtxq_unbusy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);
	}
out:
	umtx_key_release(&uq->uq_key);
	return (error);
}

#if defined(COMPAT_FREEBSD9) || defined(COMPAT_FREEBSD10)
static int
do_sem_wait(struct thread *td, struct _usem *sem, struct _umtx_time *timeout)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	uint32_t flags, count, count1;
	int error, rv, rv1;

	uq = td->td_umtxq;
	error = fueword32(&sem->_flags, &flags);
	if (error == -1)
		return (EFAULT);
	error = umtx_key_get(sem, TYPE_SEM, GET_SHARE(flags), &uq->uq_key);
	if (error != 0)
		return (error);

	if (timeout != NULL)
		umtx_abs_timeout_init2(&timo, timeout);

again:
	umtxq_lock(&uq->uq_key);
	umtxq_busy(&uq->uq_key);
	umtxq_insert(uq);
	umtxq_unlock(&uq->uq_key);
	rv = casueword32(&sem->_has_waiters, 0, &count1, 1);
	if (rv != -1)
		rv1 = fueword32(&sem->_count, &count);
	if (rv == -1 || rv1 == -1 || count != 0 || (rv == 1 && count1 == 0)) {
		if (rv == 0)
			rv = suword32(&sem->_has_waiters, 0);
		umtxq_lock(&uq->uq_key);
		umtxq_unbusy(&uq->uq_key);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
		if (rv == -1 || rv1 == -1) {
			error = EFAULT;
			goto out;
		}
		if (count != 0) {
			error = 0;
			goto out;
		}
		MPASS(rv == 1 && count1 == 0);
		rv = thread_check_susp(td, true);
		if (rv == 0)
			goto again;
		error = rv;
		goto out;
	}
	umtxq_lock(&uq->uq_key);
	umtxq_unbusy(&uq->uq_key);

	error = umtxq_sleep(uq, "usem", timeout == NULL ? NULL : &timo);

	if ((uq->uq_flags & UQF_UMTXQ) == 0)
		error = 0;
	else {
		umtxq_remove(uq);
		/* A relative timeout cannot be restarted. */
		if (error == ERESTART && timeout != NULL &&
		    (timeout->_flags & UMTX_ABSTIME) == 0)
			error = EINTR;
	}
	umtxq_unlock(&uq->uq_key);
out:
	umtx_key_release(&uq->uq_key);
	return (error);
}

/*
 * Signal a userland semaphore.
 */
static int
do_sem_wake(struct thread *td, struct _usem *sem)
{
	struct umtx_key key;
	int error, cnt;
	uint32_t flags;

	error = fueword32(&sem->_flags, &flags);
	if (error == -1)
		return (EFAULT);
	if ((error = umtx_key_get(sem, TYPE_SEM, GET_SHARE(flags), &key)) != 0)
		return (error);
	umtxq_lock(&key);
	umtxq_busy(&key);
	cnt = umtxq_count(&key);
	if (cnt > 0) {
		/*
		 * Check if count is greater than 0, this means the memory is
		 * still being referenced by user code, so we can safely
		 * update _has_waiters flag.
		 */
		if (cnt == 1) {
			umtxq_unlock(&key);
			error = suword32(&sem->_has_waiters, 0);
			umtxq_lock(&key);
			if (error == -1)
				error = EFAULT;
		}
		umtxq_signal(&key, 1);
	}
	umtxq_unbusy(&key);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	return (error);
}
#endif

static int
do_sem2_wait(struct thread *td, struct _usem2 *sem, struct _umtx_time *timeout)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	uint32_t count, flags;
	int error, rv;

	uq = td->td_umtxq;
	flags = fuword32(&sem->_flags);
	if (timeout != NULL)
		umtx_abs_timeout_init2(&timo, timeout);

again:
	error = umtx_key_get(sem, TYPE_SEM, GET_SHARE(flags), &uq->uq_key);
	if (error != 0)
		return (error);
	umtxq_lock(&uq->uq_key);
	umtxq_busy(&uq->uq_key);
	umtxq_insert(uq);
	umtxq_unlock(&uq->uq_key);
	rv = fueword32(&sem->_count, &count);
	if (rv == -1) {
		umtxq_lock(&uq->uq_key);
		umtxq_unbusy(&uq->uq_key);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
		umtx_key_release(&uq->uq_key);
		return (EFAULT);
	}
	for (;;) {
		if (USEM_COUNT(count) != 0) {
			umtxq_lock(&uq->uq_key);
			umtxq_unbusy(&uq->uq_key);
			umtxq_remove(uq);
			umtxq_unlock(&uq->uq_key);
			umtx_key_release(&uq->uq_key);
			return (0);
		}
		if (count == USEM_HAS_WAITERS)
			break;
		rv = casueword32(&sem->_count, 0, &count, USEM_HAS_WAITERS);
		if (rv == 0)
			break;
		umtxq_lock(&uq->uq_key);
		umtxq_unbusy(&uq->uq_key);
		umtxq_remove(uq);
		umtxq_unlock(&uq->uq_key);
		umtx_key_release(&uq->uq_key);
		if (rv == -1)
			return (EFAULT);
		rv = thread_check_susp(td, true);
		if (rv != 0)
			return (rv);
		goto again;
	}
	umtxq_lock(&uq->uq_key);
	umtxq_unbusy(&uq->uq_key);

	error = umtxq_sleep(uq, "usem", timeout == NULL ? NULL : &timo);

	if ((uq->uq_flags & UQF_UMTXQ) == 0)
		error = 0;
	else {
		umtxq_remove(uq);
		if (timeout != NULL && (timeout->_flags & UMTX_ABSTIME) == 0) {
			/* A relative timeout cannot be restarted. */
			if (error == ERESTART)
				error = EINTR;
			if (error == EINTR) {
				kern_clock_gettime(curthread, timo.clockid,
				    &timo.cur);
				timespecsub(&timo.end, &timo.cur,
				    &timeout->_timeout);
			}
		}
	}
	umtxq_unlock(&uq->uq_key);
	umtx_key_release(&uq->uq_key);
	return (error);
}

/*
 * Signal a userland semaphore.
 */
static int
do_sem2_wake(struct thread *td, struct _usem2 *sem)
{
	struct umtx_key key;
	int error, cnt, rv;
	uint32_t count, flags;

	rv = fueword32(&sem->_flags, &flags);
	if (rv == -1)
		return (EFAULT);
	if ((error = umtx_key_get(sem, TYPE_SEM, GET_SHARE(flags), &key)) != 0)
		return (error);
	umtxq_lock(&key);
	umtxq_busy(&key);
	cnt = umtxq_count(&key);
	if (cnt > 0) {
		/*
		 * If this was the last sleeping thread, clear the waiters
		 * flag in _count.
		 */
		if (cnt == 1) {
			umtxq_unlock(&key);
			rv = fueword32(&sem->_count, &count);
			while (rv != -1 && count & USEM_HAS_WAITERS) {
				rv = casueword32(&sem->_count, count, &count,
				    count & ~USEM_HAS_WAITERS);
				if (rv == 1) {
					rv = thread_check_susp(td, true);
					if (rv != 0)
						break;
				}
			}
			if (rv == -1)
				error = EFAULT;
			else if (rv > 0) {
				error = rv;
			}
			umtxq_lock(&key);
		}

		umtxq_signal(&key, 1);
	}
	umtxq_unbusy(&key);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	return (error);
}

#ifdef COMPAT_FREEBSD10
int
freebsd10__umtx_lock(struct thread *td, struct freebsd10__umtx_lock_args *uap)
{
	return (do_lock_umtx(td, uap->umtx, td->td_tid, 0));
}

int
freebsd10__umtx_unlock(struct thread *td,
    struct freebsd10__umtx_unlock_args *uap)
{
	return (do_unlock_umtx(td, uap->umtx, td->td_tid));
}
#endif

inline int
umtx_copyin_timeout(const void *uaddr, struct timespec *tsp)
{
	int error;

	error = copyin(uaddr, tsp, sizeof(*tsp));
	if (error == 0) {
		if (!timespecvalid_interval(tsp))
			error = EINVAL;
	}
	return (error);
}

static inline int
umtx_copyin_umtx_time(const void *uaddr, size_t size, struct _umtx_time *tp)
{
	int error;

	if (size <= sizeof(tp->_timeout)) {
		tp->_clockid = CLOCK_REALTIME;
		tp->_flags = 0;
		error = copyin(uaddr, &tp->_timeout, sizeof(tp->_timeout));
	} else
		error = copyin(uaddr, tp, sizeof(*tp));
	if (error != 0)
		return (error);
	if (!timespecvalid_interval(&tp->_timeout))
		return (EINVAL);
	return (0);
}

static int
umtx_copyin_robust_lists(const void *uaddr, size_t size,
    struct umtx_robust_lists_params *rb)
{

	if (size > sizeof(*rb))
		return (EINVAL);
	return (copyin(uaddr, rb, size));
}

static int
umtx_copyout_timeout(void *uaddr, size_t sz, struct timespec *tsp)
{

	/*
	 * Should be guaranteed by the caller, sz == uaddr1 - sizeof(_umtx_time)
	 * and we're only called if sz >= sizeof(timespec) as supplied in the
	 * copyops.
	 */
	KASSERT(sz >= sizeof(*tsp),
	    ("umtx_copyops specifies incorrect sizes"));

	return (copyout(tsp, uaddr, sizeof(*tsp)));
}

#ifdef COMPAT_FREEBSD10
static int
__umtx_op_lock_umtx(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct timespec *ts, timeout;
	int error;

	/* Allow a null timespec (wait forever). */
	if (uap->uaddr2 == NULL)
		ts = NULL;
	else {
		error = ops->copyin_timeout(uap->uaddr2, &timeout);
		if (error != 0)
			return (error);
		ts = &timeout;
	}
#ifdef COMPAT_FREEBSD32
	if (ops->compat32)
		return (do_lock_umtx32(td, uap->obj, uap->val, ts));
#endif
	return (do_lock_umtx(td, uap->obj, uap->val, ts));
}

static int
__umtx_op_unlock_umtx(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
#ifdef COMPAT_FREEBSD32
	if (ops->compat32)
		return (do_unlock_umtx32(td, uap->obj, uap->val));
#endif
	return (do_unlock_umtx(td, uap->obj, uap->val));
}
#endif	/* COMPAT_FREEBSD10 */

#if !defined(COMPAT_FREEBSD10)
static int
__umtx_op_unimpl(struct thread *td __unused, struct _umtx_op_args *uap __unused,
    const struct umtx_copyops *ops __unused)
{
	return (EOPNOTSUPP);
}
#endif	/* COMPAT_FREEBSD10 */

static int
__umtx_op_wait(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct _umtx_time timeout, *tm_p;
	int error;

	if (uap->uaddr2 == NULL)
		tm_p = NULL;
	else {
		error = ops->copyin_umtx_time(
		    uap->uaddr2, (size_t)uap->uaddr1, &timeout);
		if (error != 0)
			return (error);
		tm_p = &timeout;
	}
	return (do_wait(td, uap->obj, uap->val, tm_p, ops->compat32, 0));
}

static int
__umtx_op_wait_uint(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct _umtx_time timeout, *tm_p;
	int error;

	if (uap->uaddr2 == NULL)
		tm_p = NULL;
	else {
		error = ops->copyin_umtx_time(
		    uap->uaddr2, (size_t)uap->uaddr1, &timeout);
		if (error != 0)
			return (error);
		tm_p = &timeout;
	}
	return (do_wait(td, uap->obj, uap->val, tm_p, 1, 0));
}

static int
__umtx_op_wait_uint_private(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct _umtx_time *tm_p, timeout;
	int error;

	if (uap->uaddr2 == NULL)
		tm_p = NULL;
	else {
		error = ops->copyin_umtx_time(
		    uap->uaddr2, (size_t)uap->uaddr1, &timeout);
		if (error != 0)
			return (error);
		tm_p = &timeout;
	}
	return (do_wait(td, uap->obj, uap->val, tm_p, 1, 1));
}

static int
__umtx_op_wake(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (kern_umtx_wake(td, uap->obj, uap->val, 0));
}

#define BATCH_SIZE	128
static int
__umtx_op_nwake_private_native(struct thread *td, struct _umtx_op_args *uap)
{
	char *uaddrs[BATCH_SIZE], **upp;
	int count, error, i, pos, tocopy;

	upp = (char **)uap->obj;
	error = 0;
	for (count = uap->val, pos = 0; count > 0; count -= tocopy,
	    pos += tocopy) {
		tocopy = MIN(count, BATCH_SIZE);
		error = copyin(upp + pos, uaddrs, tocopy * sizeof(char *));
		if (error != 0)
			break;
		for (i = 0; i < tocopy; ++i) {
			kern_umtx_wake(td, uaddrs[i], INT_MAX, 1);
		}
		maybe_yield();
	}
	return (error);
}

static int
__umtx_op_nwake_private_compat32(struct thread *td, struct _umtx_op_args *uap)
{
	uint32_t uaddrs[BATCH_SIZE], *upp;
	int count, error, i, pos, tocopy;

	upp = (uint32_t *)uap->obj;
	error = 0;
	for (count = uap->val, pos = 0; count > 0; count -= tocopy,
	    pos += tocopy) {
		tocopy = MIN(count, BATCH_SIZE);
		error = copyin(upp + pos, uaddrs, tocopy * sizeof(uint32_t));
		if (error != 0)
			break;
		for (i = 0; i < tocopy; ++i) {
			kern_umtx_wake(td, (void *)(uintptr_t)uaddrs[i],
			    INT_MAX, 1);
		}
		maybe_yield();
	}
	return (error);
}

static int
__umtx_op_nwake_private(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{

	if (ops->compat32)
		return (__umtx_op_nwake_private_compat32(td, uap));
	return (__umtx_op_nwake_private_native(td, uap));
}

static int
__umtx_op_wake_private(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (kern_umtx_wake(td, uap->obj, uap->val, 1));
}

static int
__umtx_op_lock_umutex(struct thread *td, struct _umtx_op_args *uap,
   const struct umtx_copyops *ops)
{
	struct _umtx_time *tm_p, timeout;
	int error;

	/* Allow a null timespec (wait forever). */
	if (uap->uaddr2 == NULL)
		tm_p = NULL;
	else {
		error = ops->copyin_umtx_time(
		    uap->uaddr2, (size_t)uap->uaddr1, &timeout);
		if (error != 0)
			return (error);
		tm_p = &timeout;
	}
	return (do_lock_umutex(td, uap->obj, tm_p, 0));
}

static int
__umtx_op_trylock_umutex(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_lock_umutex(td, uap->obj, NULL, _UMUTEX_TRY));
}

static int
__umtx_op_wait_umutex(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct _umtx_time *tm_p, timeout;
	int error;

	/* Allow a null timespec (wait forever). */
	if (uap->uaddr2 == NULL)
		tm_p = NULL;
	else {
		error = ops->copyin_umtx_time(
		    uap->uaddr2, (size_t)uap->uaddr1, &timeout);
		if (error != 0)
			return (error);
		tm_p = &timeout;
	}
	return (do_lock_umutex(td, uap->obj, tm_p, _UMUTEX_WAIT));
}

static int
__umtx_op_wake_umutex(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_wake_umutex(td, uap->obj));
}

static int
__umtx_op_unlock_umutex(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_unlock_umutex(td, uap->obj, false));
}

static int
__umtx_op_set_ceiling(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_set_ceiling(td, uap->obj, uap->val, uap->uaddr1));
}

static int
__umtx_op_cv_wait(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct timespec *ts, timeout;
	int error;

	/* Allow a null timespec (wait forever). */
	if (uap->uaddr2 == NULL)
		ts = NULL;
	else {
		error = ops->copyin_timeout(uap->uaddr2, &timeout);
		if (error != 0)
			return (error);
		ts = &timeout;
	}
	return (do_cv_wait(td, uap->obj, uap->uaddr1, ts, uap->val));
}

static int
__umtx_op_cv_signal(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_cv_signal(td, uap->obj));
}

static int
__umtx_op_cv_broadcast(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_cv_broadcast(td, uap->obj));
}

static int
__umtx_op_rw_rdlock(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct _umtx_time timeout;
	int error;

	/* Allow a null timespec (wait forever). */
	if (uap->uaddr2 == NULL) {
		error = do_rw_rdlock(td, uap->obj, uap->val, 0);
	} else {
		error = ops->copyin_umtx_time(uap->uaddr2,
		   (size_t)uap->uaddr1, &timeout);
		if (error != 0)
			return (error);
		error = do_rw_rdlock(td, uap->obj, uap->val, &timeout);
	}
	return (error);
}

static int
__umtx_op_rw_wrlock(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct _umtx_time timeout;
	int error;

	/* Allow a null timespec (wait forever). */
	if (uap->uaddr2 == NULL) {
		error = do_rw_wrlock(td, uap->obj, 0);
	} else {
		error = ops->copyin_umtx_time(uap->uaddr2,
		   (size_t)uap->uaddr1, &timeout);
		if (error != 0)
			return (error);

		error = do_rw_wrlock(td, uap->obj, &timeout);
	}
	return (error);
}

static int
__umtx_op_rw_unlock(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_rw_unlock(td, uap->obj));
}

#if defined(COMPAT_FREEBSD9) || defined(COMPAT_FREEBSD10)
static int
__umtx_op_sem_wait(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct _umtx_time *tm_p, timeout;
	int error;

	/* Allow a null timespec (wait forever). */
	if (uap->uaddr2 == NULL)
		tm_p = NULL;
	else {
		error = ops->copyin_umtx_time(
		    uap->uaddr2, (size_t)uap->uaddr1, &timeout);
		if (error != 0)
			return (error);
		tm_p = &timeout;
	}
	return (do_sem_wait(td, uap->obj, tm_p));
}

static int
__umtx_op_sem_wake(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_sem_wake(td, uap->obj));
}
#endif

static int
__umtx_op_wake2_umutex(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_wake2_umutex(td, uap->obj, uap->val));
}

static int
__umtx_op_sem2_wait(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct _umtx_time *tm_p, timeout;
	size_t uasize;
	int error;

	/* Allow a null timespec (wait forever). */
	if (uap->uaddr2 == NULL) {
		uasize = 0;
		tm_p = NULL;
	} else {
		uasize = (size_t)uap->uaddr1;
		error = ops->copyin_umtx_time(uap->uaddr2, uasize, &timeout);
		if (error != 0)
			return (error);
		tm_p = &timeout;
	}
	error = do_sem2_wait(td, uap->obj, tm_p);
	if (error == EINTR && uap->uaddr2 != NULL &&
	    (timeout._flags & UMTX_ABSTIME) == 0 &&
	    uasize >= ops->umtx_time_sz + ops->timespec_sz) {
		error = ops->copyout_timeout(
		    (void *)((uintptr_t)uap->uaddr2 + ops->umtx_time_sz),
		    uasize - ops->umtx_time_sz, &timeout._timeout);
		if (error == 0) {
			error = EINTR;
		}
	}

	return (error);
}

static int
__umtx_op_sem2_wake(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (do_sem2_wake(td, uap->obj));
}

#define	USHM_OBJ_UMTX(o)						\
    ((struct umtx_shm_obj_list *)(&(o)->umtx_data))

#define	USHMF_REG_LINKED	0x0001
#define	USHMF_OBJ_LINKED	0x0002
struct umtx_shm_reg {
	TAILQ_ENTRY(umtx_shm_reg) ushm_reg_link;
	LIST_ENTRY(umtx_shm_reg) ushm_obj_link;
	struct umtx_key		ushm_key;
	struct ucred		*ushm_cred;
	struct shmfd		*ushm_obj;
	u_int			ushm_refcnt;
	u_int			ushm_flags;
};

LIST_HEAD(umtx_shm_obj_list, umtx_shm_reg);
TAILQ_HEAD(umtx_shm_reg_head, umtx_shm_reg);

static uma_zone_t umtx_shm_reg_zone;
static struct umtx_shm_reg_head umtx_shm_registry[UMTX_CHAINS];
static struct mtx umtx_shm_lock;
static struct umtx_shm_reg_head umtx_shm_reg_delfree =
    TAILQ_HEAD_INITIALIZER(umtx_shm_reg_delfree);

static void umtx_shm_free_reg(struct umtx_shm_reg *reg);

static void
umtx_shm_reg_delfree_tq(void *context __unused, int pending __unused)
{
	struct umtx_shm_reg_head d;
	struct umtx_shm_reg *reg, *reg1;

	TAILQ_INIT(&d);
	mtx_lock(&umtx_shm_lock);
	TAILQ_CONCAT(&d, &umtx_shm_reg_delfree, ushm_reg_link);
	mtx_unlock(&umtx_shm_lock);
	TAILQ_FOREACH_SAFE(reg, &d, ushm_reg_link, reg1) {
		TAILQ_REMOVE(&d, reg, ushm_reg_link);
		umtx_shm_free_reg(reg);
	}
}

static struct task umtx_shm_reg_delfree_task =
    TASK_INITIALIZER(0, umtx_shm_reg_delfree_tq, NULL);

static struct umtx_shm_reg *
umtx_shm_find_reg_locked(const struct umtx_key *key)
{
	struct umtx_shm_reg *reg;
	struct umtx_shm_reg_head *reg_head;

	KASSERT(key->shared, ("umtx_p_find_rg: private key"));
	mtx_assert(&umtx_shm_lock, MA_OWNED);
	reg_head = &umtx_shm_registry[key->hash];
	TAILQ_FOREACH(reg, reg_head, ushm_reg_link) {
		KASSERT(reg->ushm_key.shared,
		    ("non-shared key on reg %p %d", reg, reg->ushm_key.shared));
		if (reg->ushm_key.info.shared.object ==
		    key->info.shared.object &&
		    reg->ushm_key.info.shared.offset ==
		    key->info.shared.offset) {
			KASSERT(reg->ushm_key.type == TYPE_SHM, ("TYPE_USHM"));
			KASSERT(reg->ushm_refcnt > 0,
			    ("reg %p refcnt 0 onlist", reg));
			KASSERT((reg->ushm_flags & USHMF_REG_LINKED) != 0,
			    ("reg %p not linked", reg));
			reg->ushm_refcnt++;
			return (reg);
		}
	}
	return (NULL);
}

static struct umtx_shm_reg *
umtx_shm_find_reg(const struct umtx_key *key)
{
	struct umtx_shm_reg *reg;

	mtx_lock(&umtx_shm_lock);
	reg = umtx_shm_find_reg_locked(key);
	mtx_unlock(&umtx_shm_lock);
	return (reg);
}

static void
umtx_shm_free_reg(struct umtx_shm_reg *reg)
{

	chgumtxcnt(reg->ushm_cred->cr_ruidinfo, -1, 0);
	crfree(reg->ushm_cred);
	shm_drop(reg->ushm_obj);
	uma_zfree(umtx_shm_reg_zone, reg);
}

static bool
umtx_shm_unref_reg_locked(struct umtx_shm_reg *reg, bool force)
{
	bool res;

	mtx_assert(&umtx_shm_lock, MA_OWNED);
	KASSERT(reg->ushm_refcnt > 0, ("ushm_reg %p refcnt 0", reg));
	reg->ushm_refcnt--;
	res = reg->ushm_refcnt == 0;
	if (res || force) {
		if ((reg->ushm_flags & USHMF_REG_LINKED) != 0) {
			TAILQ_REMOVE(&umtx_shm_registry[reg->ushm_key.hash],
			    reg, ushm_reg_link);
			reg->ushm_flags &= ~USHMF_REG_LINKED;
		}
		if ((reg->ushm_flags & USHMF_OBJ_LINKED) != 0) {
			LIST_REMOVE(reg, ushm_obj_link);
			reg->ushm_flags &= ~USHMF_OBJ_LINKED;
		}
	}
	return (res);
}

static void
umtx_shm_unref_reg(struct umtx_shm_reg *reg, bool force)
{
	vm_object_t object;
	bool dofree;

	if (force) {
		object = reg->ushm_obj->shm_object;
		VM_OBJECT_WLOCK(object);
		vm_object_set_flag(object, OBJ_UMTXDEAD);
		VM_OBJECT_WUNLOCK(object);
	}
	mtx_lock(&umtx_shm_lock);
	dofree = umtx_shm_unref_reg_locked(reg, force);
	mtx_unlock(&umtx_shm_lock);
	if (dofree)
		umtx_shm_free_reg(reg);
}

void
umtx_shm_object_init(vm_object_t object)
{

	LIST_INIT(USHM_OBJ_UMTX(object));
}

void
umtx_shm_object_terminated(vm_object_t object)
{
	struct umtx_shm_reg *reg, *reg1;
	bool dofree;

	if (LIST_EMPTY(USHM_OBJ_UMTX(object)))
		return;

	dofree = false;
	mtx_lock(&umtx_shm_lock);
	LIST_FOREACH_SAFE(reg, USHM_OBJ_UMTX(object), ushm_obj_link, reg1) {
		if (umtx_shm_unref_reg_locked(reg, true)) {
			TAILQ_INSERT_TAIL(&umtx_shm_reg_delfree, reg,
			    ushm_reg_link);
			dofree = true;
		}
	}
	mtx_unlock(&umtx_shm_lock);
	if (dofree)
		taskqueue_enqueue(taskqueue_thread, &umtx_shm_reg_delfree_task);
}

static int
umtx_shm_create_reg(struct thread *td, const struct umtx_key *key,
    struct umtx_shm_reg **res)
{
	struct umtx_shm_reg *reg, *reg1;
	struct ucred *cred;
	int error;

	reg = umtx_shm_find_reg(key);
	if (reg != NULL) {
		*res = reg;
		return (0);
	}
	cred = td->td_ucred;
	if (!chgumtxcnt(cred->cr_ruidinfo, 1, lim_cur(td, RLIMIT_UMTXP)))
		return (ENOMEM);
	reg = uma_zalloc(umtx_shm_reg_zone, M_WAITOK | M_ZERO);
	reg->ushm_refcnt = 1;
	bcopy(key, &reg->ushm_key, sizeof(*key));
	reg->ushm_obj = shm_alloc(td->td_ucred, O_RDWR, false);
	reg->ushm_cred = crhold(cred);
	error = shm_dotruncate(reg->ushm_obj, PAGE_SIZE);
	if (error != 0) {
		umtx_shm_free_reg(reg);
		return (error);
	}
	mtx_lock(&umtx_shm_lock);
	reg1 = umtx_shm_find_reg_locked(key);
	if (reg1 != NULL) {
		mtx_unlock(&umtx_shm_lock);
		umtx_shm_free_reg(reg);
		*res = reg1;
		return (0);
	}
	reg->ushm_refcnt++;
	TAILQ_INSERT_TAIL(&umtx_shm_registry[key->hash], reg, ushm_reg_link);
	LIST_INSERT_HEAD(USHM_OBJ_UMTX(key->info.shared.object), reg,
	    ushm_obj_link);
	reg->ushm_flags = USHMF_REG_LINKED | USHMF_OBJ_LINKED;
	mtx_unlock(&umtx_shm_lock);
	*res = reg;
	return (0);
}

static int
umtx_shm_alive(struct thread *td, void *addr)
{
	vm_map_t map;
	vm_map_entry_t entry;
	vm_object_t object;
	vm_pindex_t pindex;
	vm_prot_t prot;
	int res, ret;
	boolean_t wired;

	map = &td->td_proc->p_vmspace->vm_map;
	res = vm_map_lookup(&map, (uintptr_t)addr, VM_PROT_READ, &entry,
	    &object, &pindex, &prot, &wired);
	if (res != KERN_SUCCESS)
		return (EFAULT);
	if (object == NULL)
		ret = EINVAL;
	else
		ret = (object->flags & OBJ_UMTXDEAD) != 0 ? ENOTTY : 0;
	vm_map_lookup_done(map, entry);
	return (ret);
}

static void
umtx_shm_init(void)
{
	int i;

	umtx_shm_reg_zone = uma_zcreate("umtx_shm", sizeof(struct umtx_shm_reg),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	mtx_init(&umtx_shm_lock, "umtxshm", NULL, MTX_DEF);
	for (i = 0; i < nitems(umtx_shm_registry); i++)
		TAILQ_INIT(&umtx_shm_registry[i]);
}

static int
umtx_shm(struct thread *td, void *addr, u_int flags)
{
	struct umtx_key key;
	struct umtx_shm_reg *reg;
	struct file *fp;
	int error, fd;

	if (__bitcount(flags & (UMTX_SHM_CREAT | UMTX_SHM_LOOKUP |
	    UMTX_SHM_DESTROY| UMTX_SHM_ALIVE)) != 1)
		return (EINVAL);
	if ((flags & UMTX_SHM_ALIVE) != 0)
		return (umtx_shm_alive(td, addr));
	error = umtx_key_get(addr, TYPE_SHM, PROCESS_SHARE, &key);
	if (error != 0)
		return (error);
	KASSERT(key.shared == 1, ("non-shared key"));
	if ((flags & UMTX_SHM_CREAT) != 0) {
		error = umtx_shm_create_reg(td, &key, &reg);
	} else {
		reg = umtx_shm_find_reg(&key);
		if (reg == NULL)
			error = ESRCH;
	}
	umtx_key_release(&key);
	if (error != 0)
		return (error);
	KASSERT(reg != NULL, ("no reg"));
	if ((flags & UMTX_SHM_DESTROY) != 0) {
		umtx_shm_unref_reg(reg, true);
	} else {
#if 0
#ifdef MAC
		error = mac_posixshm_check_open(td->td_ucred,
		    reg->ushm_obj, FFLAGS(O_RDWR));
		if (error == 0)
#endif
			error = shm_access(reg->ushm_obj, td->td_ucred,
			    FFLAGS(O_RDWR));
		if (error == 0)
#endif
			error = falloc_caps(td, &fp, &fd, O_CLOEXEC, NULL);
		if (error == 0) {
			shm_hold(reg->ushm_obj);
			finit(fp, FFLAGS(O_RDWR), DTYPE_SHM, reg->ushm_obj,
			    &shm_ops);
			td->td_retval[0] = fd;
			fdrop(fp, td);
		}
	}
	umtx_shm_unref_reg(reg, false);
	return (error);
}

static int
__umtx_op_shm(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops __unused)
{

	return (umtx_shm(td, uap->uaddr1, uap->val));
}

static int
__umtx_op_robust_lists(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	struct umtx_robust_lists_params rb;
	int error;

	if (ops->compat32) {
		if ((td->td_pflags2 & TDP2_COMPAT32RB) == 0 &&
		    (td->td_rb_list != 0 || td->td_rbp_list != 0 ||
		    td->td_rb_inact != 0))
			return (EBUSY);
	} else if ((td->td_pflags2 & TDP2_COMPAT32RB) != 0) {
		return (EBUSY);
	}

	bzero(&rb, sizeof(rb));
	error = ops->copyin_robust_lists(uap->uaddr1, uap->val, &rb);
	if (error != 0)
		return (error);

	if (ops->compat32)
		td->td_pflags2 |= TDP2_COMPAT32RB;

	td->td_rb_list = rb.robust_list_offset;
	td->td_rbp_list = rb.robust_priv_list_offset;
	td->td_rb_inact = rb.robust_inact_offset;
	return (0);
}

static int
__umtx_op_get_min_timeout(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	long val;
	int error, val1;

	val = sbttons(td->td_proc->p_umtx_min_timeout);
	if (ops->compat32) {
		val1 = (int)val;
		error = copyout(&val1, uap->uaddr1, sizeof(val1));
	} else {
		error = copyout(&val, uap->uaddr1, sizeof(val));
	}
	return (error);
}

static int
__umtx_op_set_min_timeout(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *ops)
{
	if (uap->val < 0)
		return (EINVAL);
	td->td_proc->p_umtx_min_timeout = nstosbt(uap->val);
	return (0);
}

#if defined(__i386__) || defined(__amd64__)
/*
 * Provide the standard 32-bit definitions for x86, since native/compat32 use a
 * 32-bit time_t there.  Other architectures just need the i386 definitions
 * along with their standard compat32.
 */
struct timespecx32 {
	int64_t			tv_sec;
	int32_t			tv_nsec;
};

struct umtx_timex32 {
	struct	timespecx32	_timeout;
	uint32_t		_flags;
	uint32_t		_clockid;
};

#ifndef __i386__
#define	timespeci386	timespec32
#define	umtx_timei386	umtx_time32
#endif
#else /* !__i386__ && !__amd64__ */
/* 32-bit architectures can emulate i386, so define these almost everywhere. */
struct timespeci386 {
	int32_t			tv_sec;
	int32_t			tv_nsec;
};

struct umtx_timei386 {
	struct	timespeci386	_timeout;
	uint32_t		_flags;
	uint32_t		_clockid;
};

#if defined(__LP64__)
#define	timespecx32	timespec32
#define	umtx_timex32	umtx_time32
#endif
#endif

static int
umtx_copyin_robust_lists32(const void *uaddr, size_t size,
    struct umtx_robust_lists_params *rbp)
{
	struct umtx_robust_lists_params_compat32 rb32;
	int error;

	if (size > sizeof(rb32))
		return (EINVAL);
	bzero(&rb32, sizeof(rb32));
	error = copyin(uaddr, &rb32, size);
	if (error != 0)
		return (error);
	CP(rb32, *rbp, robust_list_offset);
	CP(rb32, *rbp, robust_priv_list_offset);
	CP(rb32, *rbp, robust_inact_offset);
	return (0);
}

#ifndef __i386__
static inline int
umtx_copyin_timeouti386(const void *uaddr, struct timespec *tsp)
{
	struct timespeci386 ts32;
	int error;

	error = copyin(uaddr, &ts32, sizeof(ts32));
	if (error == 0) {
		if (!timespecvalid_interval(&ts32))
			error = EINVAL;
		else {
			CP(ts32, *tsp, tv_sec);
			CP(ts32, *tsp, tv_nsec);
		}
	}
	return (error);
}

static inline int
umtx_copyin_umtx_timei386(const void *uaddr, size_t size, struct _umtx_time *tp)
{
	struct umtx_timei386 t32;
	int error;

	t32._clockid = CLOCK_REALTIME;
	t32._flags   = 0;
	if (size <= sizeof(t32._timeout))
		error = copyin(uaddr, &t32._timeout, sizeof(t32._timeout));
	else
		error = copyin(uaddr, &t32, sizeof(t32));
	if (error != 0)
		return (error);
	if (!timespecvalid_interval(&t32._timeout))
		return (EINVAL);
	TS_CP(t32, *tp, _timeout);
	CP(t32, *tp, _flags);
	CP(t32, *tp, _clockid);
	return (0);
}

static int
umtx_copyout_timeouti386(void *uaddr, size_t sz, struct timespec *tsp)
{
	struct timespeci386 remain32 = {
		.tv_sec = tsp->tv_sec,
		.tv_nsec = tsp->tv_nsec,
	};

	/*
	 * Should be guaranteed by the caller, sz == uaddr1 - sizeof(_umtx_time)
	 * and we're only called if sz >= sizeof(timespec) as supplied in the
	 * copyops.
	 */
	KASSERT(sz >= sizeof(remain32),
	    ("umtx_copyops specifies incorrect sizes"));

	return (copyout(&remain32, uaddr, sizeof(remain32)));
}
#endif /* !__i386__ */

#if defined(__i386__) || defined(__LP64__)
static inline int
umtx_copyin_timeoutx32(const void *uaddr, struct timespec *tsp)
{
	struct timespecx32 ts32;
	int error;

	error = copyin(uaddr, &ts32, sizeof(ts32));
	if (error == 0) {
		if (!timespecvalid_interval(&ts32))
			error = EINVAL;
		else {
			CP(ts32, *tsp, tv_sec);
			CP(ts32, *tsp, tv_nsec);
		}
	}
	return (error);
}

static inline int
umtx_copyin_umtx_timex32(const void *uaddr, size_t size, struct _umtx_time *tp)
{
	struct umtx_timex32 t32;
	int error;

	t32._clockid = CLOCK_REALTIME;
	t32._flags   = 0;
	if (size <= sizeof(t32._timeout))
		error = copyin(uaddr, &t32._timeout, sizeof(t32._timeout));
	else
		error = copyin(uaddr, &t32, sizeof(t32));
	if (error != 0)
		return (error);
	if (!timespecvalid_interval(&t32._timeout))
		return (EINVAL);
	TS_CP(t32, *tp, _timeout);
	CP(t32, *tp, _flags);
	CP(t32, *tp, _clockid);
	return (0);
}

static int
umtx_copyout_timeoutx32(void *uaddr, size_t sz, struct timespec *tsp)
{
	struct timespecx32 remain32 = {
		.tv_sec = tsp->tv_sec,
		.tv_nsec = tsp->tv_nsec,
	};

	/*
	 * Should be guaranteed by the caller, sz == uaddr1 - sizeof(_umtx_time)
	 * and we're only called if sz >= sizeof(timespec) as supplied in the
	 * copyops.
	 */
	KASSERT(sz >= sizeof(remain32),
	    ("umtx_copyops specifies incorrect sizes"));

	return (copyout(&remain32, uaddr, sizeof(remain32)));
}
#endif /* __i386__ || __LP64__ */

typedef int (*_umtx_op_func)(struct thread *td, struct _umtx_op_args *uap,
    const struct umtx_copyops *umtx_ops);

static const _umtx_op_func op_table[] = {
#ifdef COMPAT_FREEBSD10
	[UMTX_OP_LOCK]		= __umtx_op_lock_umtx,
	[UMTX_OP_UNLOCK]	= __umtx_op_unlock_umtx,
#else
	[UMTX_OP_LOCK]		= __umtx_op_unimpl,
	[UMTX_OP_UNLOCK]	= __umtx_op_unimpl,
#endif
	[UMTX_OP_WAIT]		= __umtx_op_wait,
	[UMTX_OP_WAKE]		= __umtx_op_wake,
	[UMTX_OP_MUTEX_TRYLOCK]	= __umtx_op_trylock_umutex,
	[UMTX_OP_MUTEX_LOCK]	= __umtx_op_lock_umutex,
	[UMTX_OP_MUTEX_UNLOCK]	= __umtx_op_unlock_umutex,
	[UMTX_OP_SET_CEILING]	= __umtx_op_set_ceiling,
	[UMTX_OP_CV_WAIT]	= __umtx_op_cv_wait,
	[UMTX_OP_CV_SIGNAL]	= __umtx_op_cv_signal,
	[UMTX_OP_CV_BROADCAST]	= __umtx_op_cv_broadcast,
	[UMTX_OP_WAIT_UINT]	= __umtx_op_wait_uint,
	[UMTX_OP_RW_RDLOCK]	= __umtx_op_rw_rdlock,
	[UMTX_OP_RW_WRLOCK]	= __umtx_op_rw_wrlock,
	[UMTX_OP_RW_UNLOCK]	= __umtx_op_rw_unlock,
	[UMTX_OP_WAIT_UINT_PRIVATE] = __umtx_op_wait_uint_private,
	[UMTX_OP_WAKE_PRIVATE]	= __umtx_op_wake_private,
	[UMTX_OP_MUTEX_WAIT]	= __umtx_op_wait_umutex,
	[UMTX_OP_MUTEX_WAKE]	= __umtx_op_wake_umutex,
#if defined(COMPAT_FREEBSD9) || defined(COMPAT_FREEBSD10)
	[UMTX_OP_SEM_WAIT]	= __umtx_op_sem_wait,
	[UMTX_OP_SEM_WAKE]	= __umtx_op_sem_wake,
#else
	[UMTX_OP_SEM_WAIT]	= __umtx_op_unimpl,
	[UMTX_OP_SEM_WAKE]	= __umtx_op_unimpl,
#endif
	[UMTX_OP_NWAKE_PRIVATE]	= __umtx_op_nwake_private,
	[UMTX_OP_MUTEX_WAKE2]	= __umtx_op_wake2_umutex,
	[UMTX_OP_SEM2_WAIT]	= __umtx_op_sem2_wait,
	[UMTX_OP_SEM2_WAKE]	= __umtx_op_sem2_wake,
	[UMTX_OP_SHM]		= __umtx_op_shm,
	[UMTX_OP_ROBUST_LISTS]	= __umtx_op_robust_lists,
	[UMTX_OP_GET_MIN_TIMEOUT] = __umtx_op_get_min_timeout,
	[UMTX_OP_SET_MIN_TIMEOUT] = __umtx_op_set_min_timeout,
};

static const struct umtx_copyops umtx_native_ops = {
	.copyin_timeout = umtx_copyin_timeout,
	.copyin_umtx_time = umtx_copyin_umtx_time,
	.copyin_robust_lists = umtx_copyin_robust_lists,
	.copyout_timeout = umtx_copyout_timeout,
	.timespec_sz = sizeof(struct timespec),
	.umtx_time_sz = sizeof(struct _umtx_time),
};

#ifndef __i386__
static const struct umtx_copyops umtx_native_opsi386 = {
	.copyin_timeout = umtx_copyin_timeouti386,
	.copyin_umtx_time = umtx_copyin_umtx_timei386,
	.copyin_robust_lists = umtx_copyin_robust_lists32,
	.copyout_timeout = umtx_copyout_timeouti386,
	.timespec_sz = sizeof(struct timespeci386),
	.umtx_time_sz = sizeof(struct umtx_timei386),
	.compat32 = true,
};
#endif

#if defined(__i386__) || defined(__LP64__)
/* i386 can emulate other 32-bit archs, too! */
static const struct umtx_copyops umtx_native_opsx32 = {
	.copyin_timeout = umtx_copyin_timeoutx32,
	.copyin_umtx_time = umtx_copyin_umtx_timex32,
	.copyin_robust_lists = umtx_copyin_robust_lists32,
	.copyout_timeout = umtx_copyout_timeoutx32,
	.timespec_sz = sizeof(struct timespecx32),
	.umtx_time_sz = sizeof(struct umtx_timex32),
	.compat32 = true,
};

#ifdef COMPAT_FREEBSD32
#ifdef __amd64__
#define	umtx_native_ops32	umtx_native_opsi386
#else
#define	umtx_native_ops32	umtx_native_opsx32
#endif
#endif /* COMPAT_FREEBSD32 */
#endif /* __i386__ || __LP64__ */

#define	UMTX_OP__FLAGS	(UMTX_OP__32BIT | UMTX_OP__I386)

static int
kern__umtx_op(struct thread *td, void *obj, int op, unsigned long val,
    void *uaddr1, void *uaddr2, const struct umtx_copyops *ops)
{
	struct _umtx_op_args uap = {
		.obj = obj,
		.op = op & ~UMTX_OP__FLAGS,
		.val = val,
		.uaddr1 = uaddr1,
		.uaddr2 = uaddr2
	};

	if ((uap.op >= nitems(op_table)))
		return (EINVAL);
	return ((*op_table[uap.op])(td, &uap, ops));
}

int
sys__umtx_op(struct thread *td, struct _umtx_op_args *uap)
{
	static const struct umtx_copyops *umtx_ops;

	umtx_ops = &umtx_native_ops;
#ifdef __LP64__
	if ((uap->op & (UMTX_OP__32BIT | UMTX_OP__I386)) != 0) {
		if ((uap->op & UMTX_OP__I386) != 0)
			umtx_ops = &umtx_native_opsi386;
		else
			umtx_ops = &umtx_native_opsx32;
	}
#elif !defined(__i386__)
	/* We consider UMTX_OP__32BIT a nop on !i386 ILP32. */
	if ((uap->op & UMTX_OP__I386) != 0)
		umtx_ops = &umtx_native_opsi386;
#else
	/* Likewise, UMTX_OP__I386 is a nop on i386. */
	if ((uap->op & UMTX_OP__32BIT) != 0)
		umtx_ops = &umtx_native_opsx32;
#endif
	return (kern__umtx_op(td, uap->obj, uap->op, uap->val, uap->uaddr1,
	    uap->uaddr2, umtx_ops));
}

#ifdef COMPAT_FREEBSD32
#ifdef COMPAT_FREEBSD10
int
freebsd10_freebsd32__umtx_lock(struct thread *td,
    struct freebsd10_freebsd32__umtx_lock_args *uap)
{
	return (do_lock_umtx32(td, (uint32_t *)uap->umtx, td->td_tid, NULL));
}

int
freebsd10_freebsd32__umtx_unlock(struct thread *td,
    struct freebsd10_freebsd32__umtx_unlock_args *uap)
{
	return (do_unlock_umtx32(td, (uint32_t *)uap->umtx, td->td_tid));
}
#endif /* COMPAT_FREEBSD10 */

int
freebsd32__umtx_op(struct thread *td, struct freebsd32__umtx_op_args *uap)
{

	return (kern__umtx_op(td, uap->obj, uap->op, uap->val, uap->uaddr1,
	    uap->uaddr2, &umtx_native_ops32));
}
#endif /* COMPAT_FREEBSD32 */

void
umtx_thread_init(struct thread *td)
{

	td->td_umtxq = umtxq_alloc();
	td->td_umtxq->uq_thread = td;
}

void
umtx_thread_fini(struct thread *td)
{

	umtxq_free(td->td_umtxq);
}

/*
 * It will be called when new thread is created, e.g fork().
 */
void
umtx_thread_alloc(struct thread *td)
{
	struct umtx_q *uq;

	uq = td->td_umtxq;
	uq->uq_inherited_pri = PRI_MAX;

	KASSERT(uq->uq_flags == 0, ("uq_flags != 0"));
	KASSERT(uq->uq_thread == td, ("uq_thread != td"));
	KASSERT(uq->uq_pi_blocked == NULL, ("uq_pi_blocked != NULL"));
	KASSERT(TAILQ_EMPTY(&uq->uq_pi_contested), ("uq_pi_contested is not empty"));
}

/*
 * exec() hook.
 *
 * Clear robust lists for all process' threads, not delaying the
 * cleanup to thread exit, since the relevant address space is
 * destroyed right now.
 */
void
umtx_exec(struct proc *p)
{
	struct thread *td;

	KASSERT(p == curproc, ("need curproc"));
	KASSERT((p->p_flag & P_HADTHREADS) == 0 ||
	    (p->p_flag & P_STOPPED_SINGLE) != 0,
	    ("curproc must be single-threaded"));
	/*
	 * There is no need to lock the list as only this thread can be
	 * running.
	 */
	FOREACH_THREAD_IN_PROC(p, td) {
		KASSERT(td == curthread ||
		    ((td->td_flags & TDF_BOUNDARY) != 0 && TD_IS_SUSPENDED(td)),
		    ("running thread %p %p", p, td));
		umtx_thread_cleanup(td);
		td->td_rb_list = td->td_rbp_list = td->td_rb_inact = 0;
	}

	p->p_umtx_min_timeout = 0;
}

/*
 * thread exit hook.
 */
void
umtx_thread_exit(struct thread *td)
{

	umtx_thread_cleanup(td);
}

static int
umtx_read_uptr(struct thread *td, uintptr_t ptr, uintptr_t *res, bool compat32)
{
	u_long res1;
	uint32_t res32;
	int error;

	if (compat32) {
		error = fueword32((void *)ptr, &res32);
		if (error == 0)
			res1 = res32;
	} else {
		error = fueword((void *)ptr, &res1);
	}
	if (error == 0)
		*res = res1;
	else
		error = EFAULT;
	return (error);
}

static void
umtx_read_rb_list(struct thread *td, struct umutex *m, uintptr_t *rb_list,
    bool compat32)
{
	struct umutex32 m32;

	if (compat32) {
		memcpy(&m32, m, sizeof(m32));
		*rb_list = m32.m_rb_lnk;
	} else {
		*rb_list = m->m_rb_lnk;
	}
}

static int
umtx_handle_rb(struct thread *td, uintptr_t rbp, uintptr_t *rb_list, bool inact,
    bool compat32)
{
	struct umutex m;
	int error;

	KASSERT(td->td_proc == curproc, ("need current vmspace"));
	error = copyin((void *)rbp, &m, sizeof(m));
	if (error != 0)
		return (error);
	if (rb_list != NULL)
		umtx_read_rb_list(td, &m, rb_list, compat32);
	if ((m.m_flags & UMUTEX_ROBUST) == 0)
		return (EINVAL);
	if ((m.m_owner & ~UMUTEX_CONTESTED) != td->td_tid)
		/* inact is cleared after unlock, allow the inconsistency */
		return (inact ? 0 : EINVAL);
	return (do_unlock_umutex(td, (struct umutex *)rbp, true));
}

static void
umtx_cleanup_rb_list(struct thread *td, uintptr_t rb_list, uintptr_t *rb_inact,
    const char *name, bool compat32)
{
	int error, i;
	uintptr_t rbp;
	bool inact;

	if (rb_list == 0)
		return;
	error = umtx_read_uptr(td, rb_list, &rbp, compat32);
	for (i = 0; error == 0 && rbp != 0 && i < umtx_max_rb; i++) {
		if (rbp == *rb_inact) {
			inact = true;
			*rb_inact = 0;
		} else
			inact = false;
		error = umtx_handle_rb(td, rbp, &rbp, inact, compat32);
	}
	if (i == umtx_max_rb && umtx_verbose_rb) {
		uprintf("comm %s pid %d: reached umtx %smax rb %d\n",
		    td->td_proc->p_comm, td->td_proc->p_pid, name, umtx_max_rb);
	}
	if (error != 0 && umtx_verbose_rb) {
		uprintf("comm %s pid %d: handling %srb error %d\n",
		    td->td_proc->p_comm, td->td_proc->p_pid, name, error);
	}
}

/*
 * Clean up umtx data.
 */
static void
umtx_thread_cleanup(struct thread *td)
{
	struct umtx_q *uq;
	struct umtx_pi *pi;
	uintptr_t rb_inact;
	bool compat32;

	/*
	 * Disown pi mutexes.
	 */
	uq = td->td_umtxq;
	if (uq != NULL) {
		if (uq->uq_inherited_pri != PRI_MAX ||
		    !TAILQ_EMPTY(&uq->uq_pi_contested)) {
			mtx_lock(&umtx_lock);
			uq->uq_inherited_pri = PRI_MAX;
			while ((pi = TAILQ_FIRST(&uq->uq_pi_contested)) != NULL) {
				pi->pi_owner = NULL;
				TAILQ_REMOVE(&uq->uq_pi_contested, pi, pi_link);
			}
			mtx_unlock(&umtx_lock);
		}
		sched_lend_user_prio_cond(td, PRI_MAX);
	}

	compat32 = (td->td_pflags2 & TDP2_COMPAT32RB) != 0;
	td->td_pflags2 &= ~TDP2_COMPAT32RB;

	if (td->td_rb_inact == 0 && td->td_rb_list == 0 && td->td_rbp_list == 0)
		return;

	/*
	 * Handle terminated robust mutexes.  Must be done after
	 * robust pi disown, otherwise unlock could see unowned
	 * entries.
	 */
	rb_inact = td->td_rb_inact;
	if (rb_inact != 0)
		(void)umtx_read_uptr(td, rb_inact, &rb_inact, compat32);
	umtx_cleanup_rb_list(td, td->td_rb_list, &rb_inact, "", compat32);
	umtx_cleanup_rb_list(td, td->td_rbp_list, &rb_inact, "priv ", compat32);
	if (rb_inact != 0)
		(void)umtx_handle_rb(td, rb_inact, NULL, true, compat32);
}
