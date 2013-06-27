/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)select.h	8.2 (Berkeley) 1/4/94
 * $FreeBSD$
 */

#ifndef _SYS_SELINFO_H_
#define	_SYS_SELINFO_H_

#include <sys/event.h>		/* for struct klist */

#ifdef VPS
#include <sys/condvar.h>
#include <vm/uma.h>
#endif

struct selfd;
TAILQ_HEAD(selfdlist, selfd);

/*
 * Used to maintain information about processes that wish to be
 * notified when I/O becomes possible.
 */
struct selinfo {
	struct selfdlist	si_tdlist;	/* List of sleeping threads. */
	struct knlist		si_note;	/* kernel note list */
	struct mtx		*si_mtx;	/* Lock for tdlist. */
};

#define	SEL_WAITING(si)		(!TAILQ_EMPTY(&(si)->si_tdlist))

#ifdef _KERNEL
void	seldrain(struct selinfo *sip);
void	selrecord(struct thread *selector, struct selinfo *sip);
void	selwakeup(struct selinfo *sip);
void	selwakeuppri(struct selinfo *sip, int pri);
void	seltdfini(struct thread *td);

#ifdef VPS
/*
 * One seltd per-thread allocated on demand as needed.
 *
 *      t - protected by st_mtx
 *      k - Only accessed by curthread or read-only
 */
struct seltd {
        STAILQ_HEAD(, selfd)    st_selq;        /* (k) List of selfds. */
        struct selfd            *st_free1;      /* (k) free fd for read set. */
        struct selfd            *st_free2;      /* (k) free fd for write set. */
        struct mtx              st_mtx;         /* Protects struct seltd */
        struct cv               st_wait;        /* (t) Wait channel. */
        int                     st_flags;       /* (t) SELTD_ flags. */
};

#define SELTD_PENDING   0x0001                  /* We have pending events. */
#define SELTD_RESCAN    0x0002                  /* Doing a rescan. */

/*
 * One selfd allocated per-thread per-file-descriptor.
 *      f - protected by sf_mtx
 */
struct selfd {
        STAILQ_ENTRY(selfd)     sf_link;        /* (k) fds owned by this td. */
        TAILQ_ENTRY(selfd)      sf_threads;     /* (f) fds on this selinfo. */
        struct selinfo          *sf_si;         /* (f) selinfo when linked. */
        struct mtx              *sf_mtx;        /* Pointer to selinfo mtx. */
        struct seltd            *sf_td;         /* (k) owning seltd. */
        void                    *sf_cookie;     /* (k) fd or pollfd. */
};

extern uma_zone_t selfd_zone;
MALLOC_DECLARE(M_SELECT);

void	selfdalloc(struct thread *td, void *cookie);
void	selfdfree(struct seltd *stp, struct selfd *sfp);
int	seltdwait(struct thread *td, sbintime_t sbt, sbintime_t precision);
void	seltdinit(struct thread *td);
void	seltdclear(struct thread *td);
#endif /* VPS */

#endif /* _KERNEL */

#endif /* !_SYS_SELINFO_H_ */
