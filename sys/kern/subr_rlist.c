/*
 * Copyright (c) 1992 William F. Jolitz, TeleMuse
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This software is a component of "386BSD" developed by 
	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: subr_rlist.c,v 1.3 1993/11/25 01:33:18 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "malloc.h"
#include "rlist.h"

/*
 * Resource lists.
 */

/*
 * Add space to a resource list. Used to either
 * initialize a list or return free space to it.
 */
void
rlist_free (rlp, start, end)
	register struct rlist **rlp;
	unsigned start, end;
{
	struct rlist *head;

	head = *rlp;

loop:
	/* if nothing here, insert (tail of list) */
	if (*rlp == 0) {
		*rlp = (struct rlist *)malloc(sizeof(**rlp), M_TEMP, M_NOWAIT);
		(*rlp)->rl_start = start;
		(*rlp)->rl_end = end;
		(*rlp)->rl_next = 0;
		return;
	}

	/* if new region overlaps something currently present, panic */
	if (start >= (*rlp)->rl_start && start <= (*rlp)->rl_end)  {
		printf("Frag %d:%d, ent %d:%d ", start, end,
			(*rlp)->rl_start, (*rlp)->rl_end);
		panic("overlapping front rlist_free: freed twice?");
	}
	if (end >= (*rlp)->rl_start && end <= (*rlp)->rl_end) {
		printf("Frag %d:%d, ent %d:%d ", start, end,
			(*rlp)->rl_start, (*rlp)->rl_end);
		panic("overlapping tail rlist_free: freed twice?");
	}

	/* are we adjacent to this element? (in front) */
	if (end+1 == (*rlp)->rl_start) {
		/* coalesce */
		(*rlp)->rl_start = start;
		goto scan;
	}

	/* are we before this element? */
	if (end < (*rlp)->rl_start) {
		register struct rlist *nlp;

		nlp = (struct rlist *)malloc(sizeof(*nlp), M_TEMP, M_NOWAIT);
		nlp->rl_start = start;
		nlp->rl_end = end;
		nlp->rl_next = *rlp;
		*rlp = nlp;
		return;
	}

	/* are we adjacent to this element? (at tail) */
	if ((*rlp)->rl_end + 1 == start) {
		/* coalesce */
		(*rlp)->rl_end = end;
		goto scan;
	}

	/* are we after this element */
	if (start  > (*rlp)->rl_end) {
		rlp = &((*rlp)->rl_next);
		goto loop;
	} else
		panic("rlist_free: can't happen");

scan:
	/* can we coalesce list now that we've filled a void? */
	{
		register struct rlist *lp, *lpn;

		for (lp = head; lp->rl_next ;) { 
			lpn = lp->rl_next;

			/* coalesce ? */
			if (lp->rl_end + 1 == lpn->rl_start) {
				lp->rl_end = lpn->rl_end;
				lp->rl_next = lpn->rl_next;
				free(lpn, M_TEMP);
			} else
				lp = lp->rl_next;
		}
	}
}

/*
 * Obtain a region of desired size from a resource list.
 * If nothing available of that size, return 0. Otherwise,
 * return a value of 1 and set resource start location with
 * "*loc". (Note: loc can be zero if we don't wish the value)
 */
int rlist_alloc (rlp, size, loc)
struct rlist **rlp; unsigned size, *loc; {
	register struct rlist *lp;


	/* walk list, allocating first thing that's big enough (first fit) */
	for (; *rlp; rlp = &((*rlp)->rl_next))
		if(size <= (*rlp)->rl_end - (*rlp)->rl_start + 1) {

			/* hand it to the caller */
			if (loc) *loc = (*rlp)->rl_start;
			(*rlp)->rl_start += size;

			/* did we eat this element entirely? */
			if ((*rlp)->rl_start > (*rlp)->rl_end) {
				lp = (*rlp)->rl_next;
				free (*rlp, M_TEMP);
				*rlp = lp;
			}

			return (1);
		}

	/* nothing in list that's big enough */
	return (0);
}

/*
 * Finished with this resource list, reclaim all space and
 * mark it as being empty.
 */
void
rlist_destroy (rlp)
	struct rlist **rlp;
{
	struct rlist *lp, *nlp;

	lp = *rlp;
	*rlp = 0;
	for (; lp; lp = nlp) {
		nlp = lp->rl_next;
		free (lp, M_TEMP);
	}
}
