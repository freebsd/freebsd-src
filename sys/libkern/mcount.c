/*-
 * Copyright (c) 1983, 1992, 1993
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
 */

#if !defined(lint) && !defined(KERNEL) && defined(LIBC_SCCS)
#if 0
static char sccsid[] = "@(#)mcount.c	8.1 (Berkeley) 6/4/93";
#endif
static const char rcsid[] =
	"$Id$";
#endif

#include <sys/param.h>
#include <sys/gmon.h>
#ifdef KERNEL
#include <sys/systm.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
void	bintr __P((void));
void	btrap __P((void));
void	eintr __P((void));
void	user __P((void));
#endif

/*
 * mcount is called on entry to each function compiled with the profiling
 * switch set.  _mcount(), which is declared in a machine-dependent way
 * with _MCOUNT_DECL, does the actual work and is either inlined into a
 * C routine or called by an assembly stub.  In any case, this magic is
 * taken care of by the MCOUNT definition in <machine/profile.h>.
 *
 * _mcount updates data structures that represent traversals of the
 * program's call graph edges.  frompc and selfpc are the return
 * address and function address that represents the given call graph edge.
 *
 * Note: the original BSD code used the same variable (frompcindex) for
 * both frompcindex and frompc.  Any reasonable, modern compiler will
 * perform this optimization.
 */
_MCOUNT_DECL(frompc, selfpc)	/* _mcount; may be static, inline, etc */
	register fptrint_t frompc, selfpc;
{
#ifdef GUPROF
	u_int delta;
#endif
	register fptrdiff_t frompci;
	register u_short *frompcindex;
	register struct tostruct *top, *prevtop;
	register struct gmonparam *p;
	register long toindex;
#ifdef KERNEL
	register int s;		/* XXX */
	u_long save_eflags;	/* XXX */
#endif

	p = &_gmonparam;
#ifndef GUPROF			/* XXX */
	/*
	 * check that we are profiling
	 * and that we aren't recursively invoked.
	 */
	if (p->state != GMON_PROF_ON)
		return;
#endif
#ifdef KERNEL
	MCOUNT_ENTER;
#else
	p->state = GMON_PROF_BUSY;
#endif
	frompci = frompc - p->lowpc;

#ifdef KERNEL
	/*
	 * When we are called from an exception handler, frompci may be
	 * for a user address.  Convert such frompci's to the index of
	 * user() to merge all user counts.
	 */
	if (frompci >= p->textsize) {
		if (frompci + p->lowpc
		    >= (fptrint_t)(VM_MAXUSER_ADDRESS + UPAGES * NBPG))
			goto done;
		frompci = (fptrint_t)user - p->lowpc;
		if (frompci >= p->textsize)
		    goto done;
	}
#endif /* KERNEL */

#ifdef GUPROF
	if (p->state != GMON_PROF_HIRES)
		goto skip_guprof_stuff;
	/*
	 * Look at the clock and add the count of clock cycles since the
	 * clock was last looked at to a counter for frompc.  This
	 * solidifies the count for the function containing frompc and
	 * effectively starts another clock for the current function.
	 * The count for the new clock will be solidified when another
	 * function call is made or the function returns.
	 *
	 * We use the usual sampling counters since they can be located
	 * efficiently.  4-byte counters are usually necessary.
	 *
	 * There are many complications for subtracting the profiling
	 * overheads from the counts for normal functions and adding
	 * them to the counts for mcount(), mexitcount() and cputime().
	 * We attempt to handle fractional cycles, but the overheads
	 * are usually underestimated because they are calibrated for
	 * a simpler than usual setup.
	 */
	delta = cputime() - p->mcount_overhead;
	p->cputime_overhead_resid += p->cputime_overhead_frac;
	p->mcount_overhead_resid += p->mcount_overhead_frac;
	if ((int)delta < 0)
		*p->mcount_count += delta + p->mcount_overhead
				    - p->cputime_overhead;
	else if (delta != 0) {
		if (p->cputime_overhead_resid >= CALIB_SCALE) {
			p->cputime_overhead_resid -= CALIB_SCALE;
			++*p->cputime_count;
			--delta;
		}
		if (delta != 0) {
			if (p->mcount_overhead_resid >= CALIB_SCALE) {
				p->mcount_overhead_resid -= CALIB_SCALE;
				++*p->mcount_count;
				--delta;
			}
			KCOUNT(p, frompci) += delta;
		}
		*p->mcount_count += p->mcount_overhead_sub;
	}
	*p->cputime_count += p->cputime_overhead;
skip_guprof_stuff:
#endif /* GUPROF */

#ifdef KERNEL
	/*
	 * When we are called from an exception handler, frompc is faked
	 * to be for where the exception occurred.  We've just solidified
	 * the count for there.  Now convert frompci to the index of btrap()
	 * for trap handlers and bintr() for interrupt handlers to make
	 * exceptions appear in the call graph as calls from btrap() and
	 * bintr() instead of calls from all over.
	 */
	if ((fptrint_t)selfpc >= (fptrint_t)btrap
	    && (fptrint_t)selfpc < (fptrint_t)eintr) {
		if ((fptrint_t)selfpc >= (fptrint_t)bintr)
			frompci = (fptrint_t)bintr - p->lowpc;
		else
			frompci = (fptrint_t)btrap - p->lowpc;
	}
#endif /* KERNEL */

	/*
	 * check that frompc is a reasonable pc value.
	 * for example:	signal catchers get called from the stack,
	 *		not from text space.  too bad.
	 */
	if (frompci >= p->textsize)
		goto done;

	frompcindex = &p->froms[frompci / (p->hashfraction * sizeof(*p->froms))];
	toindex = *frompcindex;
	if (toindex == 0) {
		/*
		 *	first time traversing this arc
		 */
		toindex = ++p->tos[0].link;
		if (toindex >= p->tolimit)
			/* halt further profiling */
			goto overflow;

		*frompcindex = toindex;
		top = &p->tos[toindex];
		top->selfpc = selfpc;
		top->count = 1;
		top->link = 0;
		goto done;
	}
	top = &p->tos[toindex];
	if (top->selfpc == selfpc) {
		/*
		 * arc at front of chain; usual case.
		 */
		top->count++;
		goto done;
	}
	/*
	 * have to go looking down chain for it.
	 * top points to what we are looking at,
	 * prevtop points to previous top.
	 * we know it is not at the head of the chain.
	 */
	for (; /* goto done */; ) {
		if (top->link == 0) {
			/*
			 * top is end of the chain and none of the chain
			 * had top->selfpc == selfpc.
			 * so we allocate a new tostruct
			 * and link it to the head of the chain.
			 */
			toindex = ++p->tos[0].link;
			if (toindex >= p->tolimit)
				goto overflow;

			top = &p->tos[toindex];
			top->selfpc = selfpc;
			top->count = 1;
			top->link = *frompcindex;
			*frompcindex = toindex;
			goto done;
		}
		/*
		 * otherwise, check the next arc on the chain.
		 */
		prevtop = top;
		top = &p->tos[top->link];
		if (top->selfpc == selfpc) {
			/*
			 * there it is.
			 * increment its count
			 * move it to the head of the chain.
			 */
			top->count++;
			toindex = prevtop->link;
			prevtop->link = top->link;
			top->link = *frompcindex;
			*frompcindex = toindex;
			goto done;
		}

	}
done:
#ifdef KERNEL
	MCOUNT_EXIT;
#else
	p->state = GMON_PROF_ON;
#endif
	return;
overflow:
	p->state = GMON_PROF_ERROR;
#ifdef KERNEL
	MCOUNT_EXIT;
#endif
	return;
}

/*
 * Actual definition of mcount function.  Defined in <machine/profile.h>,
 * which is included by <sys/gmon.h>.
 */
MCOUNT

#ifdef GUPROF
void
mexitcount(selfpc)
	fptrint_t selfpc;
{
	struct gmonparam *p;
	fptrint_t selfpcdiff;

	p = &_gmonparam;
	selfpcdiff = selfpc - (fptrint_t)p->lowpc;
	if (selfpcdiff < p->textsize) {
		u_int delta;

		/*
		 * Solidify the count for the current function.
		 */
		delta = cputime() - p->mexitcount_overhead;
		p->cputime_overhead_resid += p->cputime_overhead_frac;
		p->mexitcount_overhead_resid += p->mexitcount_overhead_frac;
		if ((int)delta < 0)
			*p->mexitcount_count += delta + p->mexitcount_overhead
						- p->cputime_overhead;
		else if (delta != 0) {
			if (p->cputime_overhead_resid >= CALIB_SCALE) {
				p->cputime_overhead_resid -= CALIB_SCALE;
				++*p->cputime_count;
				--delta;
			}
			if (delta != 0) {
				if (p->mexitcount_overhead_resid
				    >= CALIB_SCALE) {
					p->mexitcount_overhead_resid
					    -= CALIB_SCALE;
					++*p->mexitcount_count;
					--delta;
				}
				KCOUNT(p, selfpcdiff) += delta;
			}
			*p->mexitcount_count += p->mexitcount_overhead_sub;
		}
		*p->cputime_count += p->cputime_overhead;
	}
}
#endif /* GUPROF */
