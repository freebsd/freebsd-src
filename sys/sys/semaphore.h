/*-
 * Copyright (c) 1996, 1997
 *	HD Associates, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by HD Associates, Inc
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL HD ASSOCIATES OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/semaphore.h,v 1.12.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

/* semaphore.h: POSIX 1003.1b semaphores */

#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include <machine/_limits.h>

/* Opaque type definition. */
struct sem;
typedef	struct sem *	sem_t;

#define	SEM_FAILED	((sem_t *)0)
#define	SEM_VALUE_MAX	__INT_MAX

#ifndef _KERNEL
#include <sys/cdefs.h>

struct timespec;

__BEGIN_DECLS
int	 sem_close(sem_t *);
int	 sem_destroy(sem_t *);
int	 sem_getvalue(sem_t * __restrict, int * __restrict);
int	 sem_init(sem_t *, int, unsigned int);
sem_t	*sem_open(const char *, int, ...);
int	 sem_post(sem_t *);
int	 sem_timedwait(sem_t * __restrict, const struct timespec * __restrict);
int	 sem_trywait(sem_t *);
int	 sem_unlink(const char *);
int	 sem_wait(sem_t *);
__END_DECLS

#endif

#endif /* !_SEMAPHORE_H_ */
