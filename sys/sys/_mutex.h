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
 * $FreeBSD: src/sys/sys/_mutex.h,v 1.9 2002/12/29 11:14:41 phk Exp $
 */

#ifndef _SYS__MUTEX_H_
#define	_SYS__MUTEX_H_

/*
 * Sleep/spin mutex.
 */
struct mtx {
	struct lock_object	mtx_object;	/* Common lock properties. */
	volatile uintptr_t	mtx_lock;	/* Owner and flags. */
	volatile u_int		mtx_recurse;	/* Number of recursive holds. */
	TAILQ_HEAD(, thread)	mtx_blocked;	/* Threads blocked on us. */
	LIST_ENTRY(mtx)		mtx_contested;	/* Next contested mtx. */

#ifdef MUTEX_PROFILING
	/*
	 * This does not result in variant structure sizes because
	 * MUTEX_PROFILING is in opt_global.h
	 */
	u_int64_t		mtx_acqtime;
	const char		*mtx_filename;
	int			mtx_lineno;
#endif
};

#endif /* !_SYS__MUTEX_H_ */
