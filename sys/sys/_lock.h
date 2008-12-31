/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/_lock.h,v 1.14.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _SYS__LOCK_H_
#define	_SYS__LOCK_H_

struct lock_profile_object {
        /*
         * This does not result in variant structure sizes because
         * MUTEX_PROFILING is in opt_global.h
         */
	u_int64_t               lpo_acqtime;
	u_int64_t               lpo_waittime;
	const char              *lpo_filename;
	u_int                   lpo_namehash;
	int                     lpo_lineno;
	const char              *lpo_type;
        /*
         * Fields relating to measuring contention on mutexes.
         * holding must be accessed atomically since it's
         * modified by threads that don't yet hold the mutex.
         * locking is only modified and referenced while
         * the mutex is held.
         */
        u_int                   lpo_contest_holding;
        u_int                   lpo_contest_locking;
};

struct lock_object {
	const	char *lo_name;		/* Individual lock name. */
	const	char *lo_type;		/* General lock type. */
	u_int	lo_flags;
#ifdef LOCK_PROFILING
        struct  lock_profile_object lo_profile_obj;
#endif
	union {				/* Data for witness. */
		STAILQ_ENTRY(lock_object) lod_list;
		struct	witness *lod_witness;
	} lo_witness_data;
};

#endif /* !_SYS__LOCK_H_ */
