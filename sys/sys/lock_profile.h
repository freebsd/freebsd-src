/*-
 * Copyright (c) 2006 Kip Macy kmacy@FreeBSD.org
 * Copyright (c) 2006 Kris Kennaway kris@FreeBSD.org
 * Copyright (c) 2006 Dag-Erling Smorgrav des@des.no
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHAL THE AUTHORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/lock_profile.h,v 1.15.6.1 2008/11/25 02:59:29 kensmith Exp $
 */


#ifndef _SYS_LOCK_PROFILE_H_
#define _SYS_LOCK_PROFILE_H_

#ifdef LOCK_PROFILING
#include <sys/stdint.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <machine/atomic.h>
#include <machine/cpufunc.h>

#ifndef LPROF_HASH_SIZE
#define LPROF_HASH_SIZE		4096
#define LPROF_HASH_MASK		(LPROF_HASH_SIZE - 1)
#endif

#ifndef USE_CPU_NANOSECONDS
u_int64_t nanoseconds(void);
#endif

struct lock_prof {
	const char	*name;
	const char      *type;
	const char	*file;
	u_int		 namehash;
	int		line;
	uintmax_t	cnt_max;
	uintmax_t	cnt_tot;
	uintmax_t       cnt_wait;
	uintmax_t	cnt_cur;
	uintmax_t	cnt_contest_holding;
	uintmax_t	cnt_contest_locking;
};

extern struct lock_prof lprof_buf[LPROF_HASH_SIZE];
#define LPROF_SBUF_SIZE		256 * 400

/* We keep a smaller pool of spin mutexes for protecting the lprof hash entries */
#define LPROF_LOCK_SIZE         16	
#define LPROF_LOCK_MASK         (LPROF_LOCK_SIZE - 1)
#define LPROF_LHASH(hash)       ((hash) & LPROF_LOCK_MASK)

#define LPROF_LOCK(hash)        mtx_lock_spin(&lprof_locks[LPROF_LHASH(hash)])
#define LPROF_UNLOCK(hash)      mtx_unlock_spin(&lprof_locks[LPROF_LHASH(hash)])

#ifdef _KERNEL
extern struct mtx lprof_locks[LPROF_LOCK_SIZE];
extern int lock_prof_enable;

void _lock_profile_obtain_lock_success(struct lock_object *lo, int contested, uint64_t waittime, const char *file, int line); 
void _lock_profile_update_wait(struct lock_object *lo, uint64_t waitstart);
void _lock_profile_release_lock(struct lock_object *lo);

static inline void lock_profile_object_init(struct lock_object *lo, struct lock_class *class, const char *name) {
	const char *p;
	u_int hash = 0;
	struct lock_profile_object *l = &lo->lo_profile_obj;

	l->lpo_acqtime = 0;
	l->lpo_waittime = 0;
	l->lpo_filename = NULL;
	l->lpo_lineno = 0;
	l->lpo_contest_holding = 0;
	l->lpo_contest_locking = 0;
	l->lpo_type = class->lc_name;

	/* Hash the mutex name to an int so we don't have to strcmp() it repeatedly */
	for (p = name; *p != '\0'; p++)
		hash = 31 * hash + *p;
	l->lpo_namehash = hash;
#if 0
	if (opts & MTX_PROFILE)
		l->lpo_stack = stack_create();
#endif
}


static inline void 
lock_profile_object_destroy(struct lock_object *lo) 
{
#if 0
	struct lock_profile_object *l = &lo->lo_profile_obj;
	if (lo->lo_flags & LO_PROFILE)
		stack_destroy(l->lpo_stack);
#endif
}

static inline void lock_profile_obtain_lock_failed(struct lock_object *lo, int *contested,
    uint64_t *waittime) 
{
	struct lock_profile_object *l = &lo->lo_profile_obj;

	if (!(lo->lo_flags & LO_NOPROFILE) && lock_prof_enable &&
	    *contested == 0) {
		*waittime = nanoseconds();
		atomic_add_int(&l->lpo_contest_holding, 1);
		*contested = 1;
	}
}

static inline void lock_profile_obtain_lock_success(struct lock_object *lo, int contested, uint64_t waittime, const char *file, int line) 
{
	
	/* don't reset the timer when/if recursing */
	if (!(lo->lo_flags & LO_NOPROFILE) && lock_prof_enable &&
	    lo->lo_profile_obj.lpo_acqtime == 0) {
#ifdef LOCK_PROFILING_FAST
               if (contested == 0)
                       return;
#endif
	       _lock_profile_obtain_lock_success(lo, contested, waittime, file, line);
	}
}
static inline void lock_profile_release_lock(struct lock_object *lo)
{
	struct lock_profile_object *l = &lo->lo_profile_obj;

	if (!(lo->lo_flags & LO_NOPROFILE) && l->lpo_acqtime) 
		_lock_profile_release_lock(lo);
}

#endif /* _KERNEL */

#else /* !LOCK_PROFILING */

#ifdef _KERNEL
static inline void lock_profile_update_wait(struct lock_object *lo, uint64_t waitstart) {;}
static inline void lock_profile_update_contest_locking(struct lock_object *lo, int contested) {;}
static inline void lock_profile_release_lock(struct lock_object *lo) {;}
static inline void lock_profile_obtain_lock_failed(struct lock_object *lo, int *contested, uint64_t *waittime) {;}
static inline void lock_profile_obtain_lock_success(struct lock_object *lo, int contested, uint64_t waittime,  
						    const char *file, int line) {;}
static inline void lock_profile_object_destroy(struct lock_object *lo) {;}
static inline void lock_profile_object_init(struct lock_object *lo, struct lock_class *class, const char *name) {;}

#endif /* _KERNEL */

#endif  /* !LOCK_PROFILING */

#endif /* _SYS_LOCK_PROFILE_H_ */
