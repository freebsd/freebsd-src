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
 */
/*
 * Changes Copyright (C) 1995, David Greenman & John Dyson; This software may
 * be used, modified, copied, distributed, and sold, in both source and
 * binary form provided that the above copyright and these terms are
 * retained. Under no circumstances is the author responsible for the proper
 * functioning of this software, nor does the author assume any responsibility
 * for damages incurred with its use.
 *
 *	$Id: subr_rlist.c,v 1.25 1998/02/06 12:13:26 eivind Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/rlist.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

/*
 * Resource lists.
 */

#define RLIST_MIN 128
static int rlist_count=0;
static struct rlist *rlfree;

static struct rlist	*rlist_malloc __P((void));
static __inline void	rlist_mfree __P((struct rlist *rl));

static struct rlist *
rlist_malloc()
{
	struct rlist *rl;
	int i;
	while( rlist_count < RLIST_MIN) {
		int s = splhigh();
		rl = (struct rlist *)kmem_alloc(kernel_map, PAGE_SIZE);
		splx(s);
		if( !rl)
			break;

		for(i=0;i<(PAGE_SIZE/(sizeof *rl));i++) {
			rl->rl_next = rlfree;
			rlfree = rl;
			rlist_count++;
			rl++;
		}
	}

	if( (rl = rlfree) == 0 )
		panic("Cannot get an rlist entry");

	--rlist_count;
	rlfree = rl->rl_next;
	return rl;
}

static __inline void
rlist_mfree(rl)
	struct rlist *rl;
{
	rl->rl_next = rlfree;
	rlfree = rl;
	++rlist_count;
}

void
rlist_free(rlh, start, end)
	struct rlisthdr *rlh;
	u_int start, end;
{
	struct rlist **rlp = &rlh->rlh_list;
	struct rlist *prev_rlp = NULL, *cur_rlp = *rlp, *next_rlp = NULL;
	int s;

	s = splhigh();
	while (rlh->rlh_lock & RLH_LOCKED) {
		rlh->rlh_lock |= RLH_DESIRED;
		tsleep(rlh, PSWP, "rlistf", 0);
	}
	rlh->rlh_lock |= RLH_LOCKED;
	splx(s);

	/*
	 * Traverse the list looking for an entry after the one we want
	 * to insert.
	 */
	while (cur_rlp != NULL) {
		if (start < cur_rlp->rl_start)
			break;
#ifdef DIAGNOSTIC
		if (prev_rlp) {
			if (prev_rlp->rl_end + 1 == cur_rlp->rl_start)
				panic("rlist_free: missed coalesce opportunity");
			if (prev_rlp->rl_end ==  cur_rlp->rl_start)
				panic("rlist_free: entries overlap");
			if (prev_rlp->rl_end > cur_rlp->rl_start)
				panic("entries out of order");
		}
#endif
		prev_rlp = cur_rlp;
		cur_rlp = cur_rlp->rl_next;
	}

	if (cur_rlp != NULL) {

		if (end >= cur_rlp->rl_start)
			panic("rlist_free: free end overlaps already freed area");

		if (prev_rlp) {
			if (start <= prev_rlp->rl_end)
				panic("rlist_free: free start overlaps already freed area");
			/*
			 * Attempt to append
			 */
			if (prev_rlp->rl_end + 1 == start) {
				prev_rlp->rl_end = end;
				/*
				 * Attempt to prepend and coalesce
				 */
				if (end + 1 == cur_rlp->rl_start) {
					prev_rlp->rl_end = cur_rlp->rl_end;
					prev_rlp->rl_next = cur_rlp->rl_next;
					rlist_mfree(cur_rlp);
				}
				goto done;
			}
		}
		/*
		 * Attempt to prepend
		 */
		if (end + 1 == cur_rlp->rl_start) {
			cur_rlp->rl_start = start;
			goto done;
		}
	}
	/*
	 * Reached the end of the list without finding a larger entry.
	 * Append to last entry if there is one and it's adjacent.
	 */
	if (prev_rlp) {
		if (start <= prev_rlp->rl_end)
			panic("rlist_free: free start overlaps already freed area at list tail");
		/*
		 * Attempt to append
		 */
		if (prev_rlp->rl_end + 1 == start) {
			prev_rlp->rl_end = end;
			goto done;
		}
	}

	/*
	 * Could neither append nor prepend; allocate a new entry.
	 */
	next_rlp = cur_rlp;
	cur_rlp = rlist_malloc();
	cur_rlp->rl_start = start;
	cur_rlp->rl_end = end;
	cur_rlp->rl_next = next_rlp;
	if (prev_rlp) {
		prev_rlp->rl_next = cur_rlp;
	} else {
		/*
		 * No previous - this entry is the new list head.
		 */
		*rlp = cur_rlp;
	}

done:
	rlh->rlh_lock &= ~RLH_LOCKED;
	if (rlh->rlh_lock & RLH_DESIRED) {
		wakeup(rlh);
		rlh->rlh_lock &= ~RLH_DESIRED;
	}
	return;
}

/*
 * Obtain a region of desired size from a resource list.
 * If nothing available of that size, return 0. Otherwise,
 * return a value of 1 and set resource start location with
 * "*loc". (Note: loc can be zero if we don't wish the value)
 */
int
rlist_alloc (rlh, size, loc)
	struct rlisthdr *rlh;
	unsigned size, *loc;
{
	struct rlist **rlp = &rlh->rlh_list;
	register struct rlist *lp;
	int s;
	register struct rlist *olp = 0;

	s = splhigh();
	while (rlh->rlh_lock & RLH_LOCKED) {
		rlh->rlh_lock |= RLH_DESIRED;
		tsleep(rlh, PSWP, "rlistf", 0);
	}
	rlh->rlh_lock |= RLH_LOCKED;
	splx(s);

	/* walk list, allocating first thing that's big enough (first fit) */
	for (; *rlp; rlp = &((*rlp)->rl_next))
		if(size <= (*rlp)->rl_end - (*rlp)->rl_start + 1) {

			/* hand it to the caller */
			if (loc) *loc = (*rlp)->rl_start;
			(*rlp)->rl_start += size;

			/* did we eat this element entirely? */
			if ((*rlp)->rl_start > (*rlp)->rl_end) {
				lp = (*rlp)->rl_next;
				rlist_mfree(*rlp);
				/*
				 * if the deleted element was in fromt
				 * of the list, adjust *rlp, else don't.
				 */
				if (olp) {
					olp->rl_next = lp;
				} else {
					*rlp = lp;
				}
			}

			rlh->rlh_lock &= ~RLH_LOCKED;
			if (rlh->rlh_lock & RLH_DESIRED) {
				wakeup(rlh);
				rlh->rlh_lock &= ~RLH_DESIRED;
			}
			return (1);
		} else {
			olp = *rlp;
		}

	rlh->rlh_lock &= ~RLH_LOCKED;
	if (rlh->rlh_lock & RLH_DESIRED) {
		wakeup(rlh);
		rlh->rlh_lock &= ~RLH_DESIRED;
	}
	/* nothing in list that's big enough */
	return (0);
}

/*
 * Finished with this resource list, reclaim all space and
 * mark it as being empty.
 */
void
rlist_destroy (rlh)
	struct rlisthdr *rlh;
{
	struct rlist **rlp = &rlh->rlh_list;
	struct rlist *lp, *nlp;

	lp = *rlp;
	*rlp = 0;
	for (; lp; lp = nlp) {
		nlp = lp->rl_next;
		rlist_mfree(lp);
	}
}
