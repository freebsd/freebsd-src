/*-
 * Copyright (c) 2006 John Baldwin <jhb@FreeBSD.org>
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * This module holds the global variables and functions used to maintain
 * lock_object structures.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_mprof.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/lock_profile.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <machine/cpufunc.h>

CTASSERT(LOCK_CLASS_MAX == 15);

struct lock_class *lock_classes[LOCK_CLASS_MAX + 1] = {
	&lock_class_mtx_spin,
	&lock_class_mtx_sleep,
	&lock_class_sx,
	&lock_class_rm,
	&lock_class_rw,
	&lock_class_lockmgr,
};

void
lock_init(struct lock_object *lock, struct lock_class *class, const char *name,
    const char *type, int flags)
{
	int i;

	/* Check for double-init and zero object. */
	KASSERT(!lock_initalized(lock), ("lock \"%s\" %p already initialized",
	    name, lock));

	/* Look up lock class to find its index. */
	for (i = 0; i < LOCK_CLASS_MAX; i++)
		if (lock_classes[i] == class) {
			lock->lo_flags = i << LO_CLASSSHIFT;
			break;
		}
	KASSERT(i < LOCK_CLASS_MAX, ("unknown lock class %p", class));

	/* Initialize the lock object. */
	lock->lo_name = name;
	lock->lo_type = type != NULL ? type : name;
	lock->lo_flags |= flags | LO_INITIALIZED;
	LOCK_LOG_INIT(lock, 0);
	WITNESS_INIT(lock);
}

void
lock_destroy(struct lock_object *lock)
{

	KASSERT(lock_initalized(lock), ("lock %p is not initialized", lock));
	WITNESS_DESTROY(lock);
	LOCK_LOG_DESTROY(lock, 0);
	lock->lo_flags &= ~LO_INITIALIZED;
}

#ifdef DDB
DB_SHOW_COMMAND(lock, db_show_lock)
{
	struct lock_object *lock;
	struct lock_class *class;

	if (!have_addr)
		return;
	lock = (struct lock_object *)addr;
	if (LO_CLASSINDEX(lock) > LOCK_CLASS_MAX) {
		db_printf("Unknown lock class: %d\n", LO_CLASSINDEX(lock));
		return;
	}
	class = LOCK_CLASS(lock);
	db_printf(" class: %s\n", class->lc_name);
	db_printf(" name: %s\n", lock->lo_name);
	if (lock->lo_type && lock->lo_type != lock->lo_name)
		db_printf(" type: %s\n", lock->lo_type);
	class->lc_ddb_show(lock);
}
#endif

#ifdef LOCK_PROFILING

/*
 * One object per-thread for each lock the thread owns.  Tracks individual
 * lock instances.
 */
struct lock_profile_object {
	LIST_ENTRY(lock_profile_object) lpo_link;
	struct lock_object *lpo_obj;
	const char	*lpo_file;
	int		lpo_line;
	uint16_t	lpo_ref;
	uint16_t	lpo_cnt;
	u_int64_t	lpo_acqtime;
	u_int64_t	lpo_waittime;
	u_int		lpo_contest_locking;
};

/*
 * One lock_prof for each (file, line, lock object) triple.
 */
struct lock_prof {
	SLIST_ENTRY(lock_prof) link;
	struct lock_class *class;
	const char	*file;
	const char	*name;
	int		line;
	int		ticks;
	uintmax_t	cnt_max;
	uintmax_t	cnt_tot;
	uintmax_t	cnt_wait;
	uintmax_t	cnt_cur;
	uintmax_t	cnt_contest_locking;
};

SLIST_HEAD(lphead, lock_prof);

#define	LPROF_HASH_SIZE		4096
#define	LPROF_HASH_MASK		(LPROF_HASH_SIZE - 1)
#define	LPROF_CACHE_SIZE	4096

/*
 * Array of objects and profs for each type of object for each cpu.  Spinlocks
 * are handled seperately because a thread may be preempted and acquire a
 * spinlock while in the lock profiling code of a non-spinlock.  In this way
 * we only need a critical section to protect the per-cpu lists.
 */
struct lock_prof_type {
	struct lphead		lpt_lpalloc;
	struct lpohead		lpt_lpoalloc;
	struct lphead		lpt_hash[LPROF_HASH_SIZE];
	struct lock_prof	lpt_prof[LPROF_CACHE_SIZE];
	struct lock_profile_object lpt_objs[LPROF_CACHE_SIZE];
};

struct lock_prof_cpu {
	struct lock_prof_type	lpc_types[2]; /* One for spin one for other. */
};

struct lock_prof_cpu *lp_cpu[MAXCPU];

int lock_prof_enable = 0;

/* SWAG: sbuf size = avg stat. line size * number of locks */
#define LPROF_SBUF_SIZE		256 * 400

static int lock_prof_rejected;
static int lock_prof_skipspin;
static int lock_prof_skipcount;

#ifndef USE_CPU_NANOSECONDS
u_int64_t
nanoseconds(void)
{
	struct bintime bt;
	u_int64_t ns;

	binuptime(&bt);
	/* From bintime2timespec */
	ns = bt.sec * (u_int64_t)1000000000;
	ns += ((uint64_t)1000000000 * (uint32_t)(bt.frac >> 32)) >> 32;
	return (ns);
}
#endif

static void
lock_prof_init_type(struct lock_prof_type *type)
{
	int i;

	SLIST_INIT(&type->lpt_lpalloc);
	LIST_INIT(&type->lpt_lpoalloc);
	for (i = 0; i < LPROF_CACHE_SIZE; i++) {
		SLIST_INSERT_HEAD(&type->lpt_lpalloc, &type->lpt_prof[i],
		    link);
		LIST_INSERT_HEAD(&type->lpt_lpoalloc, &type->lpt_objs[i],
		    lpo_link);
	}
}

static void
lock_prof_init(void *arg)
{
	int cpu;

	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		lp_cpu[cpu] = malloc(sizeof(*lp_cpu[cpu]), M_DEVBUF,
		    M_WAITOK | M_ZERO);
		lock_prof_init_type(&lp_cpu[cpu]->lpc_types[0]);
		lock_prof_init_type(&lp_cpu[cpu]->lpc_types[1]);
	}
}
SYSINIT(lockprof, SI_SUB_SMP, SI_ORDER_ANY, lock_prof_init, NULL);

static void
lock_prof_reset(void)
{
	struct lock_prof_cpu *lpc;
	int enabled, i, cpu;

	enabled = lock_prof_enable;
	lock_prof_enable = 0;
	pause("lpreset", hz / 10);
	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		lpc = lp_cpu[cpu];
		for (i = 0; i < LPROF_CACHE_SIZE; i++) {
			LIST_REMOVE(&lpc->lpc_types[0].lpt_objs[i], lpo_link);
			LIST_REMOVE(&lpc->lpc_types[1].lpt_objs[i], lpo_link);
		}
		bzero(lpc, sizeof(*lpc));
		lock_prof_init_type(&lpc->lpc_types[0]);
		lock_prof_init_type(&lpc->lpc_types[1]);
	}
	lock_prof_enable = enabled;
}

static void
lock_prof_output(struct lock_prof *lp, struct sbuf *sb)
{
	const char *p;

	for (p = lp->file; p != NULL && strncmp(p, "../", 3) == 0; p += 3);
	sbuf_printf(sb,
	    "%6ju %12ju %12ju %11ju %5ju %5ju %12ju %12ju %s:%d (%s:%s)\n",
	    lp->cnt_max / 1000, lp->cnt_tot / 1000,
	    lp->cnt_wait / 1000, lp->cnt_cur,
	    lp->cnt_cur == 0 ? (uintmax_t)0 :
	    lp->cnt_tot / (lp->cnt_cur * 1000),
	    lp->cnt_cur == 0 ? (uintmax_t)0 :
	    lp->cnt_wait / (lp->cnt_cur * 1000),
	    (uintmax_t)0, lp->cnt_contest_locking,
	    p, lp->line, lp->class->lc_name, lp->name);
}

static void
lock_prof_sum(struct lock_prof *match, struct lock_prof *dst, int hash,
    int spin, int t)
{
	struct lock_prof_type *type;
	struct lock_prof *l;
	int cpu;

	dst->file = match->file;
	dst->line = match->line;
	dst->class = match->class;
	dst->name = match->name;

	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		if (lp_cpu[cpu] == NULL)
			continue;
		type = &lp_cpu[cpu]->lpc_types[spin];
		SLIST_FOREACH(l, &type->lpt_hash[hash], link) {
			if (l->ticks == t)
				continue;
			if (l->file != match->file || l->line != match->line ||
			    l->name != match->name)
				continue;
			l->ticks = t;
			if (l->cnt_max > dst->cnt_max)
				dst->cnt_max = l->cnt_max;
			dst->cnt_tot += l->cnt_tot;
			dst->cnt_wait += l->cnt_wait;
			dst->cnt_cur += l->cnt_cur;
			dst->cnt_contest_locking += l->cnt_contest_locking;
		}
	}
	
}

static void
lock_prof_type_stats(struct lock_prof_type *type, struct sbuf *sb, int spin,
    int t)
{
	struct lock_prof *l;
	int i;

	for (i = 0; i < LPROF_HASH_SIZE; ++i) {
		SLIST_FOREACH(l, &type->lpt_hash[i], link) {
			struct lock_prof lp = {};

			if (l->ticks == t)
				continue;
			lock_prof_sum(l, &lp, i, spin, t);
			lock_prof_output(&lp, sb);
			if (sbuf_overflowed(sb))
				return;
		}
	}
}

static int
dump_lock_prof_stats(SYSCTL_HANDLER_ARGS)
{
	static int multiplier = 1;
	struct sbuf *sb;
	int error, cpu, t;
	int enabled;

retry_sbufops:
	sb = sbuf_new(NULL, NULL, LPROF_SBUF_SIZE * multiplier, SBUF_FIXEDLEN);
	sbuf_printf(sb, "\n%6s %12s %12s %11s %5s %5s %12s %12s %s\n",
	    "max", "total", "wait_total", "count", "avg", "wait_avg", "cnt_hold", "cnt_lock", "name");
	enabled = lock_prof_enable;
	lock_prof_enable = 0;
	pause("lpreset", hz / 10);
	t = ticks;
	for (cpu = 0; cpu <= mp_maxid; cpu++) {
		if (lp_cpu[cpu] == NULL)
			continue;
		lock_prof_type_stats(&lp_cpu[cpu]->lpc_types[0], sb, 0, t);
		lock_prof_type_stats(&lp_cpu[cpu]->lpc_types[1], sb, 1, t);
		if (sbuf_overflowed(sb)) {
			sbuf_delete(sb);
			multiplier++;
			goto retry_sbufops;
		}
	}
	lock_prof_enable = enabled;

	sbuf_finish(sb);
	error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
	sbuf_delete(sb);
	return (error);
}

static int
enable_lock_prof(SYSCTL_HANDLER_ARGS)
{
	int error, v;

	v = lock_prof_enable;
	error = sysctl_handle_int(oidp, &v, v, req);
	if (error)
		return (error);
	if (req->newptr == NULL)
		return (error);
	if (v == lock_prof_enable)
		return (0);
	if (v == 1)
		lock_prof_reset();
	lock_prof_enable = !!v;

	return (0);
}

static int
reset_lock_prof_stats(SYSCTL_HANDLER_ARGS)
{
	int error, v;

	v = 0;
	error = sysctl_handle_int(oidp, &v, 0, req);
	if (error)
		return (error);
	if (req->newptr == NULL)
		return (error);
	if (v == 0)
		return (0);
	lock_prof_reset();

	return (0);
}

static struct lock_prof *
lock_profile_lookup(struct lock_object *lo, int spin, const char *file,
    int line)
{
	const char *unknown = "(unknown)";
	struct lock_prof_type *type;
	struct lock_prof *lp;
	struct lphead *head;
	const char *p;
	u_int hash;

	p = file;
	if (p == NULL || *p == '\0')
		p = unknown;
	hash = (uintptr_t)lo->lo_name * 31 + (uintptr_t)p * 31 + line;
	hash &= LPROF_HASH_MASK;
	type = &lp_cpu[PCPU_GET(cpuid)]->lpc_types[spin];
	head = &type->lpt_hash[hash];
	SLIST_FOREACH(lp, head, link) {
		if (lp->line == line && lp->file == p &&
		    lp->name == lo->lo_name)
			return (lp);

	}
	lp = SLIST_FIRST(&type->lpt_lpalloc);
	if (lp == NULL) {
		lock_prof_rejected++;
		return (lp);
	}
	SLIST_REMOVE_HEAD(&type->lpt_lpalloc, link);
	lp->file = p;
	lp->line = line;
	lp->class = LOCK_CLASS(lo);
	lp->name = lo->lo_name;
	SLIST_INSERT_HEAD(&type->lpt_hash[hash], lp, link);
	return (lp);
}

static struct lock_profile_object *
lock_profile_object_lookup(struct lock_object *lo, int spin, const char *file,
    int line)
{
	struct lock_profile_object *l;
	struct lock_prof_type *type;
	struct lpohead *head;

	head = &curthread->td_lprof[spin];
	LIST_FOREACH(l, head, lpo_link)
		if (l->lpo_obj == lo && l->lpo_file == file &&
		    l->lpo_line == line)
			return (l);
	critical_enter();
	type = &lp_cpu[PCPU_GET(cpuid)]->lpc_types[spin];
	l = LIST_FIRST(&type->lpt_lpoalloc);
	if (l == NULL) {
		lock_prof_rejected++;
		critical_exit();
		return (NULL);
	}
	LIST_REMOVE(l, lpo_link);
	critical_exit();
	l->lpo_obj = lo;
	l->lpo_file = file;
	l->lpo_line = line;
	l->lpo_cnt = 0;
	LIST_INSERT_HEAD(head, l, lpo_link);

	return (l);
}

void
lock_profile_obtain_lock_success(struct lock_object *lo, int contested,
    uint64_t waittime, const char *file, int line)
{
	static int lock_prof_count;
	struct lock_profile_object *l;
	int spin;

	/* don't reset the timer when/if recursing */
	if (!lock_prof_enable || (lo->lo_flags & LO_NOPROFILE))
		return;
	if (lock_prof_skipcount &&
	    (++lock_prof_count % lock_prof_skipcount) != 0)
		return;
	spin = LOCK_CLASS(lo) == &lock_class_mtx_spin;
	if (spin && lock_prof_skipspin == 1)
		return;
	l = lock_profile_object_lookup(lo, spin, file, line);
	if (l == NULL)
		return;
	l->lpo_cnt++;
	if (++l->lpo_ref > 1)
		return;
	l->lpo_contest_locking = contested;
	l->lpo_acqtime = nanoseconds(); 
	if (waittime && (l->lpo_acqtime > waittime))
		l->lpo_waittime = l->lpo_acqtime - waittime;
	else
		l->lpo_waittime = 0;
}

void
lock_profile_release_lock(struct lock_object *lo)
{
	struct lock_profile_object *l;
	struct lock_prof_type *type;
	struct lock_prof *lp;
	u_int64_t holdtime;
	struct lpohead *head;
	int spin;

	if (!lock_prof_enable || (lo->lo_flags & LO_NOPROFILE))
		return;
	spin = LOCK_CLASS(lo) == &lock_class_mtx_spin;
	head = &curthread->td_lprof[spin];
	critical_enter();
	LIST_FOREACH(l, head, lpo_link)
		if (l->lpo_obj == lo)
			break;
	if (l == NULL)
		goto out;
	if (--l->lpo_ref > 0)
		goto out;
	lp = lock_profile_lookup(lo, spin, l->lpo_file, l->lpo_line);
	if (lp == NULL)
		goto release;
	holdtime = nanoseconds() - l->lpo_acqtime;
	if (holdtime < 0)
		goto release;
	/*
	 * Record if the lock has been held longer now than ever
	 * before.
	 */
	if (holdtime > lp->cnt_max)
		lp->cnt_max = holdtime;
	lp->cnt_tot += holdtime;
	lp->cnt_wait += l->lpo_waittime;
	lp->cnt_contest_locking += l->lpo_contest_locking;
	lp->cnt_cur += l->lpo_cnt;
release:
	LIST_REMOVE(l, lpo_link);
	type = &lp_cpu[PCPU_GET(cpuid)]->lpc_types[spin];
	LIST_INSERT_HEAD(&type->lpt_lpoalloc, l, lpo_link);
out:
	critical_exit();
}

SYSCTL_NODE(_debug, OID_AUTO, lock, CTLFLAG_RD, NULL, "lock debugging");
SYSCTL_NODE(_debug_lock, OID_AUTO, prof, CTLFLAG_RD, NULL, "lock profiling");
SYSCTL_INT(_debug_lock_prof, OID_AUTO, skipspin, CTLFLAG_RW,
    &lock_prof_skipspin, 0, "Skip profiling on spinlocks.");
SYSCTL_INT(_debug_lock_prof, OID_AUTO, skipcount, CTLFLAG_RW,
    &lock_prof_skipcount, 0, "Sample approximately every N lock acquisitions.");
SYSCTL_INT(_debug_lock_prof, OID_AUTO, rejected, CTLFLAG_RD,
    &lock_prof_rejected, 0, "Number of rejected profiling records");
SYSCTL_PROC(_debug_lock_prof, OID_AUTO, stats, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, dump_lock_prof_stats, "A", "Lock profiling statistics");
SYSCTL_PROC(_debug_lock_prof, OID_AUTO, reset, CTLTYPE_INT | CTLFLAG_RW,
    NULL, 0, reset_lock_prof_stats, "I", "Reset lock profiling statistics");
SYSCTL_PROC(_debug_lock_prof, OID_AUTO, enable, CTLTYPE_INT | CTLFLAG_RW,
    NULL, 0, enable_lock_prof, "I", "Enable lock profiling");

#endif
