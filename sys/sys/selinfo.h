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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

/*
 * Used to maintain information about processes that wish to be
 * notified when I/O becomes possible.
 */
struct selinfo {
	TAILQ_ENTRY(selinfo)	si_thrlist;	/* list hung off of thread */
	struct	thread *si_thread;	/* thread waiting */
	struct	klist si_note;	/* kernel note list */
	short	si_flags;	/* see below */
};
#define	SI_COLL	0x0001		/* collision occurred */

#define	SEL_WAITING(si)	\
	((si)->si_thread != NULL || ((si)->si_flags & SI_COLL) != 0)

#ifdef _KERNEL
void	clear_selinfo_list(struct thread *td);
void	selrecord(struct thread *selector, struct selinfo *sip);
void	selwakeup(struct selinfo *sip);
#endif

#endif /* !_SYS_SELINFO_H_ */
