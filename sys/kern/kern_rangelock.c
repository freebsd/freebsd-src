/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Konstantin Belousov <kib@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/kassert.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rangelock.h>
#include <sys/sleepqueue.h>
#include <sys/smr.h>

#include <vm/uma.h>

/*
 * Implementation of range locks based on the paper
 * https://doi.org/10.1145/3342195.3387533
 * arXiv:2006.12144v1 [cs.OS] 22 Jun 2020
 * Scalable Range Locks for Scalable Address Spaces and Beyond
 * by Alex Kogan, Dave Dice, and Shady Issa
 */

static struct rl_q_entry *rl_e_unmark(const struct rl_q_entry *e);

/*
 * rl_q_next links all granted ranges in the lock.  We cannot free an
 * rl_q_entry while in the smr section, and cannot reuse rl_q_next
 * linkage since other threads might follow it even after CAS removed
 * the range.  Use rl_q_free for local list of ranges to remove after
 * the smr section is dropped.
 */
struct rl_q_entry {
	struct rl_q_entry *rl_q_next;
	struct rl_q_entry *rl_q_free;
	off_t		rl_q_start, rl_q_end;
	int		rl_q_flags;
#ifdef INVARIANTS
	struct thread	*rl_q_owner;
#endif
};

static uma_zone_t rl_entry_zone;
static smr_t rl_smr;

static void
rangelock_sys_init(void)
{
	rl_entry_zone = uma_zcreate("rl_entry", sizeof(struct rl_q_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGNOF(struct rl_q_entry),
	    UMA_ZONE_SMR);
	rl_smr = uma_zone_get_smr(rl_entry_zone);
}
SYSINIT(rl, SI_SUB_LOCK, SI_ORDER_ANY, rangelock_sys_init, NULL);

static struct rl_q_entry *
rlqentry_alloc(vm_ooffset_t start, vm_ooffset_t end, int flags)
{
	struct rl_q_entry *e;

	e = uma_zalloc_smr(rl_entry_zone, M_WAITOK);
	e->rl_q_next = NULL;
	e->rl_q_free = NULL;
	e->rl_q_start = start;
	e->rl_q_end = end;
	e->rl_q_flags = flags;
#ifdef INVARIANTS
	e->rl_q_owner = curthread;
#endif
	return (e);
}

void
rangelock_init(struct rangelock *lock)
{
	lock->sleepers = false;
	atomic_store_ptr(&lock->head, NULL);
}

void
rangelock_destroy(struct rangelock *lock)
{
	struct rl_q_entry *e, *ep;

	MPASS(!lock->sleepers);
	for (e = (struct rl_q_entry *)atomic_load_ptr(&lock->head);
	    e != NULL; e = rl_e_unmark(ep)) {
		ep = atomic_load_ptr(&e->rl_q_next);
		uma_zfree_smr(rl_entry_zone, e);
	}
}

static bool
rl_e_is_marked(const struct rl_q_entry *e)
{
	return (((uintptr_t)e & 1) != 0);
}

static struct rl_q_entry *
rl_e_unmark(const struct rl_q_entry *e)
{
	MPASS(rl_e_is_marked(e));
	return ((struct rl_q_entry *)((uintptr_t)e & ~1));
}

static struct rl_q_entry *
rl_q_load(struct rl_q_entry **p)
{
	return ((struct rl_q_entry *)atomic_load_acq_ptr((uintptr_t *)p));
}

static bool
rl_e_is_rlock(const struct rl_q_entry *e)
{
	return ((e->rl_q_flags & RL_LOCK_TYPE_MASK) == RL_LOCK_READ);
}

void
rangelock_unlock(struct rangelock *lock, void *cookie)
{
	struct rl_q_entry *e;

	e = cookie;
	MPASS(lock != NULL && e != NULL);
	MPASS(!rl_e_is_marked(rl_q_load(&e->rl_q_next)));
	MPASS(e->rl_q_owner == curthread);

	sleepq_lock(&lock->sleepers);
#ifdef INVARIANTS
	int r = atomic_testandset_long((uintptr_t *)&e->rl_q_next, 0);
	MPASS(r == 0);
#else
	atomic_set_ptr((uintptr_t *)&e->rl_q_next, 1);
#endif
	lock->sleepers = false;
	sleepq_broadcast(&lock->sleepers, SLEEPQ_SLEEP, 0, 0);
	sleepq_release(&lock->sleepers);
}

/*
 * result: -1 if e1 before e2
 *          1 if e1 after e2
 *          0 if e1 and e2 overlap
 */
static int
rl_e_compare(const struct rl_q_entry *e1, const struct rl_q_entry *e2)
{
	if (e1 == NULL)
		return (1);
	if (e1->rl_q_start >= e2->rl_q_end)
		return (1);
	if (e2->rl_q_start >= e1->rl_q_end)
		return (-1);
	return (0);
}

static void
rl_insert_sleep(struct rangelock *lock)
{
	smr_exit(rl_smr);
	DROP_GIANT();
	lock->sleepers = true;
	sleepq_add(&lock->sleepers, NULL, "rangelk", 0, 0);
	sleepq_wait(&lock->sleepers, PRI_USER);
	PICKUP_GIANT();
	smr_enter(rl_smr);
}

static bool
rl_q_cas(struct rl_q_entry **prev, struct rl_q_entry *old,
    struct rl_q_entry *new)
{
	return (atomic_cmpset_rel_ptr((uintptr_t *)prev, (uintptr_t)old,
	    (uintptr_t)new) != 0);
}

static bool
rl_insert(struct rangelock *lock, struct rl_q_entry *e, bool trylock,
    struct rl_q_entry **free)
{
	struct rl_q_entry *cur, *next, **prev;
	int r;

again:
	prev = &lock->head;
	if (rl_q_load(prev) == NULL && rl_q_cas(prev, NULL, e))
		return (true);

	for (cur = rl_q_load(prev);;) {
		if (rl_e_is_marked(cur))
			goto again;

		if (cur != NULL) {
			next = rl_q_load(&cur->rl_q_next);
			if (rl_e_is_marked(next)) {
				next = rl_e_unmark(next);
				if (rl_q_cas(prev, cur, next)) {
#ifdef INVARIANTS
					cur->rl_q_owner = NULL;
#endif
					cur->rl_q_free = *free;
					*free = cur;
				}
				cur = next;
				continue;
			}
		}

		r = rl_e_compare(cur, e);
		if (r == -1) {
			prev = &cur->rl_q_next;
			cur = rl_q_load(prev);
		} else if (r == 0) {
			sleepq_lock(&lock->sleepers);
			if (__predict_false(rl_e_is_marked(rl_q_load(
			    &cur->rl_q_next)))) {
				sleepq_release(&lock->sleepers);
				continue;
			}
			if (trylock) {
				sleepq_release(&lock->sleepers);
				return (false);
			}
			rl_insert_sleep(lock);
			/* e is still valid */
			goto again;
		} else /* r == 1 */ {
			e->rl_q_next = cur;
			if (rl_q_cas(prev, cur, e)) {
				atomic_thread_fence_acq();
				return (true);
			}
			/* Reset rl_q_next in case we hit fast path. */
			e->rl_q_next = NULL;
			cur = rl_q_load(prev);
		}
	}
}

static struct rl_q_entry *
rangelock_lock_int(struct rangelock *lock, struct rl_q_entry *e,
    bool trylock)
{
	struct rl_q_entry *free, *x, *xp;
	bool res;

	free = NULL;
	smr_enter(rl_smr);
	res = rl_insert(lock, e, trylock, &free);
	smr_exit(rl_smr);
	MPASS(trylock || res);
	if (!res) {
		e->rl_q_free = free;
		free = e;
		e = NULL;
	}
	for (x = free; x != NULL; x = xp) {
		MPASS(!rl_e_is_marked(x));
		xp = x->rl_q_free;
		MPASS(!rl_e_is_marked(xp));
		uma_zfree_smr(rl_entry_zone, x);
	}
	return (e);
}

void *
rangelock_rlock(struct rangelock *lock, vm_ooffset_t start, vm_ooffset_t end)
{
	struct rl_q_entry *e;

	e = rlqentry_alloc(start, end, RL_LOCK_READ);
	return (rangelock_lock_int(lock, e, false));
}

void *
rangelock_tryrlock(struct rangelock *lock, vm_ooffset_t start, vm_ooffset_t end)
{
	struct rl_q_entry *e;

	e = rlqentry_alloc(start, end, RL_LOCK_READ);
	return (rangelock_lock_int(lock, e, true));
}

void *
rangelock_wlock(struct rangelock *lock, vm_ooffset_t start, vm_ooffset_t end)
{
	struct rl_q_entry *e;

	e = rlqentry_alloc(start, end, RL_LOCK_WRITE);
	return (rangelock_lock_int(lock, e, true));
}

void *
rangelock_trywlock(struct rangelock *lock, vm_ooffset_t start, vm_ooffset_t end)
{
	struct rl_q_entry *e;

	e = rlqentry_alloc(start, end, RL_LOCK_WRITE);
	return (rangelock_lock_int(lock, e, true));
}

#ifdef INVARIANT_SUPPORT
void
_rangelock_cookie_assert(void *cookie, int what, const char *file, int line)
{
}
#endif	/* INVARIANT_SUPPORT */

#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(rangelock, db_show_rangelock)
{
	struct rangelock *lock;
	struct rl_q_entry *e, *x;

	if (!have_addr) {
		db_printf("show rangelock addr\n");
		return;
	}

	lock = (struct rangelock *)addr;
	db_printf("rangelock %p sleepers %d\n", lock, lock->sleepers);
	for (e = lock->head;;) {
		x = rl_e_is_marked(e) ? rl_e_unmark(e) : e;
		if (x == NULL)
			break;
		db_printf("  entry %p marked %d %d start %#jx end %#jx "
		    "flags %x next %p",
		    e, rl_e_is_marked(e), rl_e_is_marked(x->rl_q_next),
		    x->rl_q_start, x->rl_q_end, x->rl_q_flags, x->rl_q_next);
#ifdef INVARIANTS
		db_printf(" owner %p (%d)", x->rl_q_owner,
		    x->rl_q_owner != NULL ? x->rl_q_owner->td_tid : -1);
#endif
		db_printf("\n");
		e = x->rl_q_next;
	}
}

#endif	/* DDB */
