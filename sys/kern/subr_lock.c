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
__FBSDID("$FreeBSD: src/sys/kern/subr_lock.c,v 1.17.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_ddb.h"
#include "opt_mprof.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/lock_profile.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

CTASSERT(LOCK_CLASS_MAX == 15);

struct lock_class *lock_classes[LOCK_CLASS_MAX + 1] = {
	&lock_class_mtx_spin,
	&lock_class_mtx_sleep,
	&lock_class_sx,
	&lock_class_rw,
	&lock_class_lockmgr,
};

#ifdef LOCK_PROFILING
#include <machine/cpufunc.h>

SYSCTL_NODE(_debug, OID_AUTO, lock, CTLFLAG_RD, NULL, "lock debugging");
SYSCTL_NODE(_debug_lock, OID_AUTO, prof, CTLFLAG_RD, NULL, "lock profiling");
int lock_prof_enable = 0;
SYSCTL_INT(_debug_lock_prof, OID_AUTO, enable, CTLFLAG_RW,
    &lock_prof_enable, 0, "Enable lock profiling");

/*
 * lprof_buf is a static pool of profiling records to avoid possible
 * reentrance of the memory allocation functions.
 *
 * Note: NUM_LPROF_BUFFERS must be smaller than LPROF_HASH_SIZE.
 */
struct lock_prof lprof_buf[LPROF_HASH_SIZE];
static int allocated_lprof_buf;
struct mtx lprof_locks[LPROF_LOCK_SIZE];


/* SWAG: sbuf size = avg stat. line size * number of locks */
#define LPROF_SBUF_SIZE		256 * 400

static int lock_prof_acquisitions;
SYSCTL_INT(_debug_lock_prof, OID_AUTO, acquisitions, CTLFLAG_RD,
    &lock_prof_acquisitions, 0, "Number of lock acquistions recorded");
static int lock_prof_records;
SYSCTL_INT(_debug_lock_prof, OID_AUTO, records, CTLFLAG_RD,
    &lock_prof_records, 0, "Number of profiling records");
static int lock_prof_maxrecords = LPROF_HASH_SIZE;
SYSCTL_INT(_debug_lock_prof, OID_AUTO, maxrecords, CTLFLAG_RD,
    &lock_prof_maxrecords, 0, "Maximum number of profiling records");
static int lock_prof_rejected;
SYSCTL_INT(_debug_lock_prof, OID_AUTO, rejected, CTLFLAG_RD,
    &lock_prof_rejected, 0, "Number of rejected profiling records");
static int lock_prof_hashsize = LPROF_HASH_SIZE;
SYSCTL_INT(_debug_lock_prof, OID_AUTO, hashsize, CTLFLAG_RD,
    &lock_prof_hashsize, 0, "Hash size");
static int lock_prof_collisions = 0;
SYSCTL_INT(_debug_lock_prof, OID_AUTO, collisions, CTLFLAG_RD,
    &lock_prof_collisions, 0, "Number of hash collisions");

#ifndef USE_CPU_NANOSECONDS
u_int64_t
nanoseconds(void)
{
	struct timespec tv;

	nanotime(&tv);
	return (tv.tv_sec * (u_int64_t)1000000000 + tv.tv_nsec);
}
#endif

static int
dump_lock_prof_stats(SYSCTL_HANDLER_ARGS)
{
        struct sbuf *sb;
        int error, i;
        static int multiplier = 1;
        const char *p;

        if (allocated_lprof_buf == 0)
                return (SYSCTL_OUT(req, "No locking recorded",
                    sizeof("No locking recorded")));

retry_sbufops:
        sb = sbuf_new(NULL, NULL, LPROF_SBUF_SIZE * multiplier, SBUF_FIXEDLEN);
        sbuf_printf(sb, "\n%6s %12s %12s %11s %5s %5s %12s %12s %s\n",
            "max", "total", "wait_total", "count", "avg", "wait_avg", "cnt_hold", "cnt_lock", "name");
        for (i = 0; i < LPROF_HASH_SIZE; ++i) {
                if (lprof_buf[i].name == NULL)
                        continue;
                for (p = lprof_buf[i].file;
                        p != NULL && strncmp(p, "../", 3) == 0; p += 3)
                                /* nothing */ ;
                sbuf_printf(sb, "%6ju %12ju %12ju %11ju %5ju %5ju %12ju %12ju %s:%d (%s:%s)\n",
                    lprof_buf[i].cnt_max / 1000,
                    lprof_buf[i].cnt_tot / 1000,
                    lprof_buf[i].cnt_wait / 1000,
                    lprof_buf[i].cnt_cur,
                    lprof_buf[i].cnt_cur == 0 ? (uintmax_t)0 :
                        lprof_buf[i].cnt_tot / (lprof_buf[i].cnt_cur * 1000),
                    lprof_buf[i].cnt_cur == 0 ? (uintmax_t)0 :
                        lprof_buf[i].cnt_wait / (lprof_buf[i].cnt_cur * 1000),
                    lprof_buf[i].cnt_contest_holding,
                    lprof_buf[i].cnt_contest_locking,
                    p, lprof_buf[i].line, 
			    lprof_buf[i].type,
			    lprof_buf[i].name);
                if (sbuf_overflowed(sb)) {
                        sbuf_delete(sb);
                        multiplier++;
                        goto retry_sbufops;
                }
        }

        sbuf_finish(sb);
        error = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb) + 1);
        sbuf_delete(sb);
        return (error);
}
static int
reset_lock_prof_stats(SYSCTL_HANDLER_ARGS)
{
        int error, v;

        if (allocated_lprof_buf == 0)
                return (0);

        v = 0;
        error = sysctl_handle_int(oidp, &v, 0, req);
        if (error)
                return (error);
        if (req->newptr == NULL)
                return (error);
        if (v == 0)
                return (0);

        bzero(lprof_buf, LPROF_HASH_SIZE*sizeof(*lprof_buf));
        allocated_lprof_buf = 0;
        return (0);
}

SYSCTL_PROC(_debug_lock_prof, OID_AUTO, stats, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, dump_lock_prof_stats, "A", "Lock profiling statistics");

SYSCTL_PROC(_debug_lock_prof, OID_AUTO, reset, CTLTYPE_INT | CTLFLAG_RW,
    NULL, 0, reset_lock_prof_stats, "I", "Reset lock profiling statistics");
#endif

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
	lock_profile_object_init(lock, class, name);
}

void
lock_destroy(struct lock_object *lock)
{

	KASSERT(lock_initalized(lock), ("lock %p is not initialized", lock));
	lock_profile_object_destroy(lock);
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
void _lock_profile_obtain_lock_success(struct lock_object *lo, int contested, uint64_t waittime, const char *file, int line)
{
        struct lock_profile_object *l = &lo->lo_profile_obj;

	lo->lo_profile_obj.lpo_contest_holding = 0;
	
	if (contested)
		lo->lo_profile_obj.lpo_contest_locking++;		
	
	l->lpo_filename = file;
	l->lpo_lineno = line;
	l->lpo_acqtime = nanoseconds(); 
	if (waittime && (l->lpo_acqtime > waittime))
		l->lpo_waittime = l->lpo_acqtime - waittime;
	else
		l->lpo_waittime = 0;
}

void _lock_profile_release_lock(struct lock_object *lo)
{
        struct lock_profile_object *l = &lo->lo_profile_obj;

        if (l->lpo_acqtime) {
                const char *unknown = "(unknown)";
                u_int64_t acqtime, now, waittime;
                struct lock_prof *mpp;
                u_int hash;
                const char *p = l->lpo_filename;
                int collision = 0;

                now = nanoseconds();
                acqtime = l->lpo_acqtime;
                waittime = l->lpo_waittime;
                if (now <= acqtime)
                        return;
                if (p == NULL || *p == '\0')
                        p = unknown;
                hash = (l->lpo_namehash * 31 * 31 + (uintptr_t)p * 31 + l->lpo_lineno) & LPROF_HASH_MASK;
                mpp = &lprof_buf[hash];
                while (mpp->name != NULL) {
                        if (mpp->line == l->lpo_lineno &&
                          mpp->file == p &&
                          mpp->namehash == l->lpo_namehash)
                                break;
                        /* If the lprof_hash entry is allocated to someone 
			 * else, try the next one 
			 */
                        collision = 1;
                        hash = (hash + 1) & LPROF_HASH_MASK;
                        mpp = &lprof_buf[hash];
                }
                if (mpp->name == NULL) {
                        int buf;

                        buf = atomic_fetchadd_int(&allocated_lprof_buf, 1);
                        /* Just exit if we cannot get a trace buffer */
                        if (buf >= LPROF_HASH_SIZE) {
                                ++lock_prof_rejected;
                                return;
                        }
			mpp->file = p;
			mpp->line = l->lpo_lineno;
			mpp->namehash = l->lpo_namehash;
			mpp->type = l->lpo_type;
			mpp->name = lo->lo_name;

			if (collision)
				++lock_prof_collisions;
			
                        /* 
			 * We might have raced someone else but who cares, 
			 * they'll try again next time 
			 */
                        ++lock_prof_records;
                }
                LPROF_LOCK(hash);
                /*
                 * Record if the lock has been held longer now than ever
                 * before.
                 */
                if (now - acqtime > mpp->cnt_max)
                        mpp->cnt_max = now - acqtime;
                mpp->cnt_tot += now - acqtime;
                mpp->cnt_wait += waittime;
                mpp->cnt_cur++;
                /*
                 * There's a small race, really we should cmpxchg
                 * 0 with the current value, but that would bill
                 * the contention to the wrong lock instance if
                 * it followed this also.
                 */
                mpp->cnt_contest_holding += l->lpo_contest_holding;
                mpp->cnt_contest_locking += l->lpo_contest_locking;
                LPROF_UNLOCK(hash);

        }
        l->lpo_acqtime = 0;
        l->lpo_waittime = 0;
        l->lpo_contest_locking = 0;
        l->lpo_contest_holding = 0;
}
#endif
