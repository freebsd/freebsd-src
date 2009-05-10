/*-
 * Copyright (c) 2007 Stephan Uphoff <ups@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef _SYS_RMLOCK_H_
#define _SYS_RMLOCK_H_

#include <sys/mutex.h>

#include <sys/_lock.h>
#include <sys/_rmlock.h>

#ifdef _KERNEL


void	rm_init(struct rmlock *rm, const char *name, int opts);
void	rm_destroy(struct rmlock *rm);
int	rm_wowned(struct rmlock *rm);
void	rm_sysinit(void *arg);


void	_rm_wlock_debug(struct rmlock *rm, const char *file, int line);
void	_rm_wunlock_debug(struct rmlock *rm, const char *file, int line);
void	_rm_rlock_debug(struct rmlock *rm, struct rm_priotracker *tracker,
    const char *file, int line);
void	_rm_runlock_debug(struct rmlock *rm,  struct rm_priotracker *tracker,
    const char *file, int line);


void	_rm_wlock(struct rmlock *rm);
void	_rm_wunlock(struct rmlock *rm);
void	_rm_rlock(struct rmlock *rm, struct rm_priotracker *tracker);
void	_rm_runlock(struct rmlock *rm,  struct rm_priotracker *tracker);

/*
 * Public interface for lock operations.
 *
 */

#ifndef LOCK_DEBUG
#error LOCK_DEBUG not defined, include <sys/lock.h> before <sys/rmlock.h>
#endif

#if LOCK_DEBUG > 0

#define	rm_wlock(rm)	_rm_wlock_debug((rm), LOCK_FILE, LOCK_LINE)
#define	rm_wunlock(rm)	_rm_wunlock_debug((rm), LOCK_FILE, LOCK_LINE)
#define	rm_rlock(rm,tracker)  \
    _rm_rlock_debug((rm),(tracker), LOCK_FILE, LOCK_LINE )
#define	rm_runlock(rm,tracker)	\
    _rm_runlock_debug((rm), (tracker), LOCK_FILE, LOCK_LINE )

#else

#define	rm_wlock(rm)		_rm_wlock((rm))
#define	rm_wunlock(rm)		_rm_wunlock((rm))
#define	rm_rlock(rm,tracker)   	_rm_rlock((rm),(tracker))
#define	rm_runlock(rm,tracker)	_rm_runlock((rm), (tracker))

#endif

#define	rm_initialized(rm)	lock_initalized(&(rm)->lock_object)

struct rm_args {
	struct rmlock	*ra_rm;
	const char 	*ra_desc;
	int		ra_opts;
};

#define	RM_SYSINIT(name, rm, desc, opts)       				\
	static struct rm_args name##_args = {				\
		(rm),							\
		(desc),							\
                (opts),							\
	};								\
	SYSINIT(name##_rm_sysinit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    rm_sysinit, &name##_args);					\
	SYSUNINIT(name##_rm_sysuninit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    rm_destroy, (rm))


#endif /* _KERNEL */
#endif /* !_SYS_RMLOCK_H_ */
