/*	$FreeBSD$	*/
/*	$NecBSD: physio_proc.h,v 3.4 1999/07/23 20:47:03 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _I386_PHYSIO_PROC_H_
#define _I386_PHYSIO_PROC_H_
#include <sys/buf.h>
#include <sys/queue.h>

struct physio_proc;
TAILQ_HEAD(physio_proc_head, physio_proc);
struct physio_proc_head physio_proc_freet, physio_proc_busyt;

struct physio_proc {
	TAILQ_ENTRY(physio_proc) pp_chain;
	struct proc *pp_proc;
};

static __inline struct physio_proc *physio_proc_enter __P((struct buf *));
static __inline void physio_proc_leave __P((struct physio_proc *));

static __inline struct physio_proc *
physio_proc_enter(bp)
	struct buf *bp;
{
	struct physio_proc *pp;
	int s;

	if (bp == NULL || (bp->b_flags & B_PHYS) == 0)
		return NULL;	
	if ((pp = physio_proc_freet.tqh_first) == NULL)
		return NULL;

	s = splstatclock();
	TAILQ_REMOVE(&physio_proc_freet, pp, pp_chain);
#if !defined(__FreeBSD__) || __FreeBSD_version < 400001
	pp->pp_proc = bp->b_proc;
#endif
	TAILQ_INSERT_TAIL(&physio_proc_busyt, pp, pp_chain);
	splx(s);
	return pp;
}

static __inline void
physio_proc_leave(pp)
	struct physio_proc *pp;
{
	int s;

	if (pp == NULL)
		return;

	s = splstatclock();
	TAILQ_REMOVE(&physio_proc_busyt, pp, pp_chain);
	TAILQ_INSERT_TAIL(&physio_proc_freet, pp, pp_chain);
	pp->pp_proc = NULL;
	splx(s);
}

void physio_proc_init __P((void));
#endif /* _I386_PHYSIO_PROC_H_ */
