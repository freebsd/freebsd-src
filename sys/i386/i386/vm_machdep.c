/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * Copyright (c) 1989, 1990 William Jolitz
 * Copyright (c) 1994 John Dyson
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and William Jolitz.
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
 *	from: @(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 *	$Id: vm_machdep.c,v 1.28 1994/06/06 12:08:16 davidg Exp $
 */

#include "npx.h"
#include "param.h"
#include "systm.h"
#include "proc.h"
#include "malloc.h"
#include "buf.h"
#include "user.h"

#include "../include/cpu.h"

#include "vm/vm.h"
#include "vm/vm_kern.h"

#define b_cylin b_resid

#define MAXCLSTATS 256
int clstats[MAXCLSTATS];
int rqstats[MAXCLSTATS];


#ifndef NOBOUNCE
vm_map_t	io_map;
volatile int	kvasfreecnt;


caddr_t		bouncememory;
int		bouncepages, bpwait;
vm_offset_t	*bouncepa;
int		bmwait, bmfreeing;

#define BITS_IN_UNSIGNED (8*sizeof(unsigned))
int		bounceallocarraysize;
unsigned	*bounceallocarray;
int		bouncefree;

#define SIXTEENMEG (4096*4096)
#define MAXBKVA 1024

/* special list that can be used at interrupt time for eventual kva free */
struct kvasfree {
	vm_offset_t addr;
	vm_offset_t size;
} kvaf[MAXBKVA];


vm_offset_t vm_bounce_kva();
/*
 * get bounce buffer pages (count physically contiguous)
 * (only 1 inplemented now)
 */
vm_offset_t
vm_bounce_page_find(count)
	int count;
{
	int bit;
	int s,i;

	if (count != 1)
		panic("vm_bounce_page_find -- no support for > 1 page yet!!!");

	s = splbio();
retry:
	for (i = 0; i < bounceallocarraysize; i++) {
		if (bounceallocarray[i] != 0xffffffff) {
			if (bit = ffs(~bounceallocarray[i])) {
				bounceallocarray[i] |= 1 << (bit - 1) ;
				bouncefree -= count;
				splx(s);
				return bouncepa[(i * BITS_IN_UNSIGNED + (bit - 1))];
			}
		}
	}
	bpwait = 1;
	tsleep((caddr_t) &bounceallocarray, PRIBIO, "bncwai", 0);
	goto retry;
}

void
vm_bounce_kva_free(addr, size, now)
	vm_offset_t addr;
	vm_offset_t size;
	int now;
{
	int s = splbio();
	kvaf[kvasfreecnt].addr = addr;
	kvaf[kvasfreecnt].size = size;
	++kvasfreecnt;
	if( now) {
		/*
		 * this will do wakeups
		 */
		vm_bounce_kva(0,0);
	} else {
		if (bmwait) {
		/*
		 * if anyone is waiting on the bounce-map, then wakeup
		 */
			wakeup((caddr_t) io_map);
			bmwait = 0;
		}
	}
	splx(s);
}

/*
 * free count bounce buffer pages
 */
void
vm_bounce_page_free(pa, count)
	vm_offset_t pa;
	int count;
{
	int allocindex;
	int index;
	int bit;

	if (count != 1)
		panic("vm_bounce_page_free -- no support for > 1 page yet!!!\n");

	for(index=0;index<bouncepages;index++) {
		if( pa == bouncepa[index])
			break;
	}

	if( index == bouncepages)
		panic("vm_bounce_page_free: invalid bounce buffer");

	allocindex = index / BITS_IN_UNSIGNED;
	bit = index % BITS_IN_UNSIGNED;

	bounceallocarray[allocindex] &= ~(1 << bit);

	bouncefree += count;
	if (bpwait) {
		bpwait = 0;
		wakeup((caddr_t) &bounceallocarray);
	}
}

/*
 * allocate count bounce buffer kva pages
 */
vm_offset_t
vm_bounce_kva(size, waitok)
	int size;
	int waitok;
{
	int i;
	int startfree;
	vm_offset_t kva = 0;
	int s = splbio();
more:
	if (!bmfreeing && kvasfreecnt) {
		bmfreeing = 1;
		for (i = 0; i < kvasfreecnt; i++) {
			pmap_remove(kernel_pmap,
				kvaf[i].addr, kvaf[i].addr + kvaf[i].size);
			kmem_free_wakeup(io_map, kvaf[i].addr,
				kvaf[i].size);
		}
		kvasfreecnt = 0;
		bmfreeing = 0;
		if( bmwait) {
			bmwait = 0;
			wakeup( (caddr_t) io_map);
		}
	}

	if( size == 0) {
		splx(s);
		return NULL;
	}

	if ((kva = kmem_alloc_pageable(io_map, size)) == 0) {
		if( !waitok) {
			splx(s);
			return NULL;
		}
		bmwait = 1;
		tsleep((caddr_t) io_map, PRIBIO, "bmwait", 0);
		goto more;
	}
	splx(s);
	return kva;
}

/*
 * same as vm_bounce_kva -- but really allocate (but takes pages as arg)
 */
vm_offset_t
vm_bounce_kva_alloc(count) 
int count;
{
	int i;
	vm_offset_t kva;
	vm_offset_t pa;
	if( bouncepages == 0) {
		kva = (vm_offset_t) malloc(count*NBPG, M_TEMP, M_WAITOK);
		return kva;
	}
	kva = vm_bounce_kva(count*NBPG, 1);
	for(i=0;i<count;i++) {
		pa = vm_bounce_page_find(1);
		pmap_kenter(kva + i * NBPG, pa);
	}
	pmap_update();
	return kva;
}

/*
 * same as vm_bounce_kva_free -- but really free
 */
void
vm_bounce_kva_alloc_free(kva, count)
	vm_offset_t kva;
	int count;
{
	int i;
	vm_offset_t pa;
	if( bouncepages == 0) {
		free((caddr_t) kva, M_TEMP);
		return;
	}
	for(i = 0; i < count; i++) {
		pa = pmap_kextract(kva + i * NBPG);
		vm_bounce_page_free(pa, 1);
	}
	vm_bounce_kva_free(kva, count*NBPG, 0);
}

/*
 * do the things necessary to the struct buf to implement
 * bounce buffers...  inserted before the disk sort
 */
void
vm_bounce_alloc(bp)
	struct buf *bp;
{
	int countvmpg;
	vm_offset_t vastart, vaend;
	vm_offset_t vapstart, vapend;
	vm_offset_t va, kva;
	vm_offset_t pa;
	int dobounceflag = 0;
	int bounceindex;
	int i;
	int s;

	if (bouncepages == 0)
		return;

	if (bp->b_flags & B_BOUNCE) {
		printf("vm_bounce_alloc: called recursively???\n");
		return;
	}

	if (bp->b_bufsize < bp->b_bcount) {
		printf("vm_bounce_alloc: b_bufsize(0x%x) < b_bcount(0x%x) !!!!\n",
			bp->b_bufsize, bp->b_bcount);
		panic("vm_bounce_alloc");
	}

/*
 *  This is not really necessary
 *	if( bp->b_bufsize != bp->b_bcount) {
 *		printf("size: %d, count: %d\n", bp->b_bufsize, bp->b_bcount);
 *	}
 */
		

	vastart = (vm_offset_t) bp->b_un.b_addr;
	vaend = (vm_offset_t) bp->b_un.b_addr + bp->b_bufsize;

	vapstart = i386_trunc_page(vastart);
	vapend = i386_round_page(vaend);
	countvmpg = (vapend - vapstart) / NBPG;

/*
 * if any page is above 16MB, then go into bounce-buffer mode
 */
	va = vapstart;
	for (i = 0; i < countvmpg; i++) {
		pa = pmap_kextract(va);
		if (pa >= SIXTEENMEG)
			++dobounceflag;
		va += NBPG;
	}
	if (dobounceflag == 0)
		return;

	if (bouncepages < dobounceflag) 
		panic("Not enough bounce buffers!!!");

/*
 * allocate a replacement kva for b_addr
 */
	kva = vm_bounce_kva(countvmpg*NBPG, 1);
	va = vapstart;
	for (i = 0; i < countvmpg; i++) {
		pa = pmap_kextract(va);
		if (pa >= SIXTEENMEG) {
			/*
			 * allocate a replacement page
			 */
			vm_offset_t bpa = vm_bounce_page_find(1);
			pmap_kenter(kva + (NBPG * i), bpa);
			/*
			 * if we are writing, the copy the data into the page
			 */
			if ((bp->b_flags & B_READ) == 0) {
				pmap_update();
				bcopy((caddr_t) va, (caddr_t) kva + (NBPG * i), NBPG);
			}
		} else {
			/*
			 * use original page
			 */
			pmap_kenter(kva + (NBPG * i), pa);
		}
		va += NBPG;
	}
	pmap_update();

/*
 * flag the buffer as being bounced
 */
	bp->b_flags |= B_BOUNCE;
/*
 * save the original buffer kva
 */
	bp->b_savekva = bp->b_un.b_addr;
/*
 * put our new kva into the buffer (offset by original offset)
 */
	bp->b_un.b_addr = (caddr_t) (((vm_offset_t) kva) |
				((vm_offset_t) bp->b_savekva & (NBPG - 1)));
	return;
}

/*
 * hook into biodone to free bounce buffer
 */
void
vm_bounce_free(bp)
	struct buf *bp;
{
	int i;
	vm_offset_t origkva, bouncekva, bouncekvaend;
	int countvmpg;
	int s;

/*
 * if this isn't a bounced buffer, then just return
 */
	if ((bp->b_flags & B_BOUNCE) == 0)
		return;

/*
 *  This check is not necessary
 *	if (bp->b_bufsize != bp->b_bcount) {
 *		printf("vm_bounce_free: b_bufsize=%d, b_bcount=%d\n",
 *			bp->b_bufsize, bp->b_bcount);
 *	}
 */

	origkva = (vm_offset_t) bp->b_savekva;
	bouncekva = (vm_offset_t) bp->b_un.b_addr;

/*
 * check every page in the kva space for b_addr
 */
	for (i = 0; i < bp->b_bufsize; ) {
		vm_offset_t mybouncepa;
		vm_offset_t copycount;

		copycount = i386_round_page(bouncekva + 1) - bouncekva;
		mybouncepa = pmap_kextract(i386_trunc_page(bouncekva));

/*
 * if this is a bounced pa, then process as one
 */
		if ( mybouncepa != pmap_kextract( i386_trunc_page( origkva))) {
			vm_offset_t tocopy = copycount;
			if (i + tocopy > bp->b_bufsize)
				tocopy = bp->b_bufsize - i;
/*
 * if this is a read, then copy from bounce buffer into original buffer
 */
			if (bp->b_flags & B_READ)
				bcopy((caddr_t) bouncekva, (caddr_t) origkva, tocopy);
/*
 * free the bounce allocation
 */
			vm_bounce_page_free(mybouncepa, 1);
		}

		origkva += copycount;
		bouncekva += copycount;
		i += copycount;
	}

/*
 * add the old kva into the "to free" list
 */
	
	bouncekva= i386_trunc_page((vm_offset_t) bp->b_un.b_addr);
	bouncekvaend= i386_round_page((vm_offset_t)bp->b_un.b_addr + bp->b_bufsize);

	vm_bounce_kva_free( bouncekva, (bouncekvaend - bouncekva), 0);
	bp->b_un.b_addr = bp->b_savekva;
	bp->b_savekva = 0;
	bp->b_flags &= ~B_BOUNCE;

	return;
}


/*
 * init the bounce buffer system
 */
void
vm_bounce_init()
{
	vm_offset_t minaddr, maxaddr;
	int i;

	io_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr, MAXBKVA * NBPG, FALSE);
	kvasfreecnt = 0;

	if (bouncepages == 0)
		return;
	
	bounceallocarraysize = (bouncepages + BITS_IN_UNSIGNED - 1) / BITS_IN_UNSIGNED;
	bounceallocarray = malloc(bounceallocarraysize * sizeof(unsigned), M_TEMP, M_NOWAIT);

	if (!bounceallocarray)
		panic("Cannot allocate bounce resource array\n");

	bzero(bounceallocarray, bounceallocarraysize * sizeof(unsigned));
	bouncepa = malloc(bouncepages * sizeof(vm_offset_t), M_TEMP, M_NOWAIT);
	if (!bouncepa)
		panic("Cannot allocate physical memory array\n");

	for(i=0;i<bouncepages;i++) {
		vm_offset_t pa;
		if( (pa = pmap_kextract((vm_offset_t) bouncememory + i * NBPG)) >= SIXTEENMEG)
			panic("bounce memory out of range");
		if( pa == 0)
			panic("bounce memory not resident");
		bouncepa[i] = pa;
	}
	bouncefree = bouncepages;

}
#endif /* NOBOUNCE */


static void
cldiskvamerge( kvanew, orig1, orig1cnt, orig2, orig2cnt)
	vm_offset_t kvanew;
	vm_offset_t orig1, orig1cnt;
	vm_offset_t orig2, orig2cnt;
{
	int i;
	vm_offset_t pa;
/*
 * enter the transfer physical addresses into the new kva
 */
	for(i=0;i<orig1cnt;i++) {
		vm_offset_t pa;
		pa = pmap_kextract((caddr_t) orig1 + i * PAGE_SIZE);
		pmap_kenter(kvanew + i * PAGE_SIZE, pa);
	}

	for(i=0;i<orig2cnt;i++) {
		vm_offset_t pa;
		pa = pmap_kextract((caddr_t) orig2 + i * PAGE_SIZE);
		pmap_kenter(kvanew + (i + orig1cnt) * PAGE_SIZE, pa);
	}
	pmap_update();
}

void
cldisksort(struct buf *dp, struct buf *bp, vm_offset_t maxio)
{
	register struct buf *ap, *newbp;
	int i, trycount=0;
	vm_offset_t orig1pages, orig2pages;
	vm_offset_t orig1begin, orig2begin;
	vm_offset_t kvanew, kvaorig;

	if( bp->b_bcount < MAXCLSTATS*PAGE_SIZE)
		++rqstats[bp->b_bcount/PAGE_SIZE];
	/*
	 * If nothing on the activity queue, then
	 * we become the only thing.
	 */
	ap = dp->b_actf;
	if(ap == NULL) {
		dp->b_actf = bp;
		dp->b_actl = bp;
		bp->av_forw = NULL;
		return;
	}


	if (bp->b_flags & B_READ) {
		while( ap->av_forw && (ap->av_forw->b_flags & B_READ))
			ap = ap->av_forw;
		goto insert;
	}

	/*
	 * If we lie after the first (currently active)
	 * request, then we must locate the second request list
	 * and add ourselves to it.
	 */

	if (bp->b_pblkno < ap->b_pblkno) {
		while (ap->av_forw) {
			/*
			 * Check for an ``inversion'' in the
			 * normally ascending block numbers,
			 * indicating the start of the second request list.
			 */
			if (ap->av_forw->b_pblkno < ap->b_pblkno) {
				/*
				 * Search the second request list
				 * for the first request at a larger
				 * block number.  We go before that;
				 * if there is no such request, we go at end.
				 */
				do {
					if (bp->b_pblkno < ap->av_forw->b_pblkno)
						goto insert;
					ap = ap->av_forw;
				} while (ap->av_forw);
				goto insert;		/* after last */
			}
			ap = ap->av_forw;
		}
		/*
		 * No inversions... we will go after the last, and
		 * be the first request in the second request list.
		 */
		goto insert;
	}
	/*
	 * Request is at/after the current request...
	 * sort in the first request list.
	 */
	while (ap->av_forw) {
		/*
		 * We want to go after the current request
		 * if there is an inversion after it (i.e. it is
		 * the end of the first request list), or if
		 * the next request is a larger block than our request.
		 */
		if (ap->av_forw->b_pblkno < ap->b_pblkno ||
		    bp->b_pblkno < ap->av_forw->b_pblkno )
			goto insert;
		ap = ap->av_forw;
	}

insert:

#ifndef NOBOUNCE
	/*
	 * read clustering with new read-ahead disk drives hurts mostly, so
	 * we don't bother...
	 */
	if( bp->b_flags & (B_READ|B_SYNC))
		goto nocluster;
	if( bp->b_bcount != bp->b_bufsize) {
		goto nocluster;
	}
	/*
	 * we currently only cluster I/O transfers that are at page-aligned
	 * kvas and transfers that are multiples of page lengths.
	 */
	if ((bp->b_flags & B_BAD) == 0 &&
		((bp->b_bcount & PAGE_MASK) == 0) &&
		(((vm_offset_t) bp->b_un.b_addr & PAGE_MASK) == 0)) {
		if( maxio > MAXCLSTATS*PAGE_SIZE)
			maxio = MAXCLSTATS*PAGE_SIZE;
		/*
		 * merge with previous?
		 * conditions:
		 * 	1) We reside physically immediately after the previous block.
		 *	2) The previous block is not first on the device queue because
		 *	   such a block might be active.
		 *  3) The mode of the two I/Os is identical.
		 *  4) The previous kva is page aligned and the previous transfer
		 *	   is a multiple of a page in length.
		 *	5) And the total I/O size would be below the maximum.
		 */
		if( (ap->b_pblkno + (ap->b_bcount / DEV_BSIZE) == bp->b_pblkno) &&
			(dp->b_actf != ap) &&
			((ap->b_flags & ~(B_CLUSTER|B_BOUNCE)) == (bp->b_flags & ~B_BOUNCE)) &&
			((ap->b_flags & B_BAD) == 0) &&
			((ap->b_bcount & PAGE_MASK) == 0) &&
			(((vm_offset_t) ap->b_un.b_addr & PAGE_MASK) == 0) &&
			(ap->b_bcount + bp->b_bcount < maxio)) {

			/*
			 * something is majorly broken in the upper level
			 * fs code...  blocks can overlap!!!  this detects
			 * the overlap and does the right thing.
			 */
			if( ap->av_forw &&
				bp->b_pblkno + ((bp->b_bcount / DEV_BSIZE) > ap->av_forw->b_pblkno))  {
				goto nocluster;
			}

			orig1begin = (vm_offset_t) ap->b_un.b_addr;
			orig1pages = ap->b_bcount / PAGE_SIZE;

			orig2begin = (vm_offset_t) bp->b_un.b_addr;
			orig2pages = bp->b_bcount / PAGE_SIZE;

			/*
			 * see if we can allocate a kva, if we cannot, the don't
			 * cluster.
			 */
			kvanew = vm_bounce_kva( PAGE_SIZE * (orig1pages + orig2pages), 0);
			if( !kvanew) {
				goto nocluster;
			}

			if( (ap->b_flags & B_CLUSTER) == 0) {

				/*
				 * get a physical buf pointer
				 */
				newbp = (struct buf *)trypbuf();
				if( !newbp) {
					vm_bounce_kva_free( kvanew, PAGE_SIZE * (orig1pages + orig2pages), 1);
					goto nocluster;
				}

				cldiskvamerge( kvanew, orig1begin, orig1pages, orig2begin, orig2pages);

				/*
				 * build the new bp to be handed off to the device
				 */

				--clstats[ap->b_bcount/PAGE_SIZE];
				*newbp = *ap;
				newbp->b_flags |= B_CLUSTER;
				newbp->b_un.b_addr = (caddr_t) kvanew;
				newbp->b_bcount += bp->b_bcount;
				newbp->b_bufsize = newbp->b_bcount;
				newbp->b_clusterf = ap;
				newbp->b_clusterl = bp;
				++clstats[newbp->b_bcount/PAGE_SIZE];

				/*
				 * enter the new bp onto the device queue
				 */
				if( ap->av_forw)
					ap->av_forw->av_back = newbp;
				else
					dp->b_actl = newbp;

				if( dp->b_actf != ap )
					ap->av_back->av_forw = newbp;
				else
					dp->b_actf = newbp;

				/*
				 * enter the previous bps onto the cluster queue
				 */
				ap->av_forw = bp;
				bp->av_back = ap;
	
				ap->av_back = NULL;
				bp->av_forw = NULL;

			} else {
				vm_offset_t addr;

				cldiskvamerge( kvanew, orig1begin, orig1pages, orig2begin, orig2pages);
				/*
				 * free the old kva
				 */
				vm_bounce_kva_free( orig1begin, ap->b_bufsize, 0);
				--clstats[ap->b_bcount/PAGE_SIZE];

				ap->b_un.b_addr = (caddr_t) kvanew;

				ap->b_clusterl->av_forw = bp;
				bp->av_forw = NULL;
				bp->av_back = ap->b_clusterl;
				ap->b_clusterl = bp;

				ap->b_bcount += bp->b_bcount;
				ap->b_bufsize = ap->b_bcount;
				++clstats[ap->b_bcount/PAGE_SIZE];
			}
			return;
		/*
		 * merge with next?
		 * conditions:
		 * 	1) We reside physically before the next block.
		 *  3) The mode of the two I/Os is identical.
		 *  4) The next kva is page aligned and the next transfer
		 *	   is a multiple of a page in length.
		 *	5) And the total I/O size would be below the maximum.
		 */
		} else if( ap->av_forw &&
			(bp->b_pblkno + (bp->b_bcount / DEV_BSIZE) == ap->av_forw->b_pblkno) &&
			((bp->b_flags & ~B_BOUNCE) == (ap->av_forw->b_flags & ~(B_CLUSTER|B_BOUNCE))) &&
			((ap->av_forw->b_flags & B_BAD) == 0) &&
			((ap->av_forw->b_bcount & PAGE_MASK) == 0) &&
			(((vm_offset_t) ap->av_forw->b_un.b_addr & PAGE_MASK) == 0) &&
			(ap->av_forw->b_bcount + bp->b_bcount < maxio)) {

			/*
			 * something is majorly broken in the upper level
			 * fs code...  blocks can overlap!!!  this detects
			 * the overlap and does the right thing.
			 */
			if( (ap->b_pblkno + (ap->b_bcount / DEV_BSIZE)) > bp->b_pblkno) {
				goto nocluster;
			}

			orig1begin = (vm_offset_t) bp->b_un.b_addr;
			orig1pages = bp->b_bcount / PAGE_SIZE;

			orig2begin = (vm_offset_t) ap->av_forw->b_un.b_addr;
			orig2pages = ap->av_forw->b_bcount / PAGE_SIZE;

			/*
			 * see if we can allocate a kva, if we cannot, the don't
			 * cluster.
			 */
			kvanew = vm_bounce_kva( PAGE_SIZE * (orig1pages + orig2pages), 0);
			if( !kvanew) {
				goto nocluster;
			}
			
			/*
			 * if next isn't a cluster we need to create one
			 */
			if( (ap->av_forw->b_flags & B_CLUSTER) == 0) {

				/*
				 * get a physical buf pointer
				 */
				newbp = (struct buf *)trypbuf();
				if( !newbp) {
					vm_bounce_kva_free( kvanew, PAGE_SIZE * (orig1pages + orig2pages), 1);
					goto nocluster;
				}

				cldiskvamerge( kvanew, orig1begin, orig1pages, orig2begin, orig2pages);
				ap = ap->av_forw;
				--clstats[ap->b_bcount/PAGE_SIZE];
				*newbp = *ap;
				newbp->b_flags |= B_CLUSTER;
				newbp->b_un.b_addr = (caddr_t) kvanew;
				newbp->b_blkno = bp->b_blkno;
				newbp->b_pblkno = bp->b_pblkno;
				newbp->b_bcount += bp->b_bcount;
				newbp->b_bufsize = newbp->b_bcount;
				newbp->b_clusterf = bp;
				newbp->b_clusterl = ap;
				++clstats[newbp->b_bcount/PAGE_SIZE];

				if( ap->av_forw)
					ap->av_forw->av_back = newbp;
				else
					dp->b_actl = newbp;

				if( dp->b_actf != ap )
					ap->av_back->av_forw = newbp;
				else
					dp->b_actf = newbp;

				bp->av_forw = ap;
				ap->av_back = bp;
	
				bp->av_back = NULL;
				ap->av_forw = NULL;
			} else {
				vm_offset_t addr;

				cldiskvamerge( kvanew, orig1begin, orig1pages, orig2begin, orig2pages);
				ap = ap->av_forw;
				vm_bounce_kva_free( orig2begin, ap->b_bufsize, 0);

				ap->b_un.b_addr = (caddr_t) kvanew;
				bp->av_forw = ap->b_clusterf;
				ap->b_clusterf->av_back = bp;
				ap->b_clusterf = bp;
				bp->av_back = NULL;
				--clstats[ap->b_bcount/PAGE_SIZE];
			
				ap->b_blkno = bp->b_blkno;
				ap->b_pblkno = bp->b_pblkno;
				ap->b_bcount += bp->b_bcount;
				ap->b_bufsize = ap->b_bcount;
				++clstats[ap->b_bcount/PAGE_SIZE];

			}
			return;
		}
	}
#endif
	/*
	 * don't merge
	 */
nocluster:
	++clstats[bp->b_bcount/PAGE_SIZE];
	bp->av_forw = ap->av_forw;
	if( bp->av_forw)
		bp->av_forw->av_back = bp;
	else
		dp->b_actl = bp;

	ap->av_forw = bp;
	bp->av_back = ap;
}

/*
 * quick version of vm_fault
 */

void
vm_fault_quick( v, prot)
	vm_offset_t v;
	int prot;
{
	if( (cpu_class == CPUCLASS_386) &&
		(prot & VM_PROT_WRITE))
		vm_fault(&curproc->p_vmspace->vm_map, v,
			VM_PROT_READ|VM_PROT_WRITE, FALSE);
	else if( prot & VM_PROT_WRITE)
		*(volatile char *)v += 0;
	else
		*(volatile char *)v;
}


/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the kernel stack and pcb, making the child
 * ready to run, and marking it so that it can return differently
 * than the parent.  Returns 1 in the child process, 0 in the parent.
 * We currently double-map the user area so that the stack is at the same
 * address in each process; in the future we will probably relocate
 * the frame pointers on the stack after copying.
 */
int
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	register struct user *up = p2->p_addr;
	int foo, offset, addr, i;
	extern char kstack[];
	extern int mvesp();

	/*
	 * Copy pcb and stack from proc p1 to p2. 
	 * We do this as cheaply as possible, copying only the active
	 * part of the stack.  The stack and pcb need to agree;
	 * this is tricky, as the final pcb is constructed by savectx,
	 * but its frame isn't yet on the stack when the stack is copied.
	 * swtch compensates for this when the child eventually runs.
	 * This should be done differently, with a single call
	 * that copies and updates the pcb+stack,
	 * replacing the bcopy and savectx.
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	offset = mvesp() - (int)kstack;
	bcopy((caddr_t)kstack + offset, (caddr_t)p2->p_addr + offset,
	    (unsigned) ctob(UPAGES) - offset);
	p2->p_regs = p1->p_regs;

	/*
	 * Wire top of address space of child to it's kstack.
	 * First, fault in a page of pte's to map it.
	 */
#if 0
        addr = trunc_page((u_int)vtopte(kstack));
	vm_map_pageable(&p2->p_vmspace->vm_map, addr, addr+NBPG, FALSE);
	for (i=0; i < UPAGES; i++)
		pmap_enter(&p2->p_vmspace->vm_pmap, kstack+i*NBPG,
			   pmap_extract(kernel_pmap, ((int)p2->p_addr)+i*NBPG),
			   /*
			    * The user area has to be mapped writable because
			    * it contains the kernel stack (when CR0_WP is on
			    * on a 486 there is no user-read/kernel-write
			    * mode).  It is protected from user mode access
			    * by the segment limits.
			    */
			   VM_PROT_READ|VM_PROT_WRITE, TRUE);
#endif
	pmap_activate(&p2->p_vmspace->vm_pmap, &up->u_pcb);

	/*
	 * 
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
	if (savectx(up, 1)) {
		/*
		 * Return 1 in child.
		 */
		return (1);
	}
	return (0);
}

#ifdef notyet
/*
 * cpu_exit is called as the last action during exit.
 *
 * We change to an inactive address space and a "safe" stack,
 * passing thru an argument to the new stack. Now, safely isolated
 * from the resources we're shedding, we release the address space
 * and any remaining machine-dependent resources, including the
 * memory for the user structure and kernel stack.
 *
 * Next, we assign a dummy context to be written over by swtch,
 * calling it to send this process off to oblivion.
 * [The nullpcb allows us to minimize cost in swtch() by not having
 * a special case].
 */
struct proc *swtch_to_inactive();
volatile void
cpu_exit(p)
	register struct proc *p;
{
	static struct pcb nullpcb;	/* pcb to overwrite on last swtch */

#if NNPX > 0
	npxexit(p);
#endif	/* NNPX */

	/* move to inactive space and stack, passing arg accross */
	p = swtch_to_inactive(p);

	/* drop per-process resources */
	vmspace_free(p->p_vmspace);
	kmem_free(kernel_map, (vm_offset_t)p->p_addr, ctob(UPAGES));

	p->p_addr = (struct user *) &nullpcb;
	splclock();
	swtch();
	/* NOTREACHED */
}
#else
void
cpu_exit(p)
	register struct proc *p;
{
	
#if NNPX > 0
	npxexit(p);
#endif	/* NNPX */
	splclock();
	curproc = 0;
	swtch();
	/* 
	 * This is to shutup the compiler, and if swtch() failed I suppose
	 * this would be a good thing.  This keeps gcc happy because panic
	 * is a volatile void function as well.
	 */
	panic("cpu_exit");
}

void
cpu_wait(p) struct proc *p; {
/*	extern vm_map_t upages_map; */
	extern char kstack[];

	/* drop per-process resources */
 	pmap_remove(vm_map_pmap(kernel_map), (vm_offset_t) p->p_addr,
		((vm_offset_t) p->p_addr) + ctob(UPAGES));
	kmem_free(kernel_map, (vm_offset_t)p->p_addr, ctob(UPAGES));
	vmspace_free(p->p_vmspace);
}
#endif

/*
 * Set a red zone in the kernel stack after the u. area.
 */
void
setredzone(pte, vaddr)
	u_short *pte;
	caddr_t vaddr;
{
/* eventually do this by setting up an expand-down stack segment
   for ss0: selector, allowing stack access down to top of u.
   this means though that protection violations need to be handled
   thru a double fault exception that must do an integral task
   switch to a known good context, within which a dump can be
   taken. a sensible scheme might be to save the initial context
   used by sched (that has physical memory mapped 1:1 at bottom)
   and take the dump while still in mapped mode */
}

/*
 * Convert kernel VA to physical address
 */
u_long
kvtop(void *addr)
{
	vm_offset_t va;

	va = pmap_kextract((vm_offset_t)addr);
	if (va == 0)
		panic("kvtop: zero page frame");
	return((int)va);
}

extern vm_map_t phys_map;

/*
 * Map an IO request into kernel virtual address space.
 *
 * All requests are (re)mapped into kernel VA space.
 * Notice that we use b_bufsize for the size of the buffer
 * to be mapped.  b_bcount might be modified by the driver.
 */
void
vmapbuf(bp)
	register struct buf *bp;
{
	register int npf;
	register caddr_t addr;
	register long flags = bp->b_flags;
	struct proc *p;
	int off;
	vm_offset_t kva;
	register vm_offset_t pa;

	if ((flags & B_PHYS) == 0)
		panic("vmapbuf");
	addr = bp->b_saveaddr = bp->b_un.b_addr;
	off = (int)addr & PGOFSET;
	p = bp->b_proc;
	npf = btoc(round_page(bp->b_bufsize + off));
	kva = kmem_alloc_wait(phys_map, ctob(npf));
	bp->b_un.b_addr = (caddr_t) (kva + off);
	while (npf--) {
		pa = pmap_extract(&p->p_vmspace->vm_pmap, (vm_offset_t)addr);
		if (pa == 0)
			panic("vmapbuf: null page frame");
		pmap_kenter(kva, trunc_page(pa));
		addr += PAGE_SIZE;
		kva += PAGE_SIZE;
	}
	pmap_update();
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
void
vunmapbuf(bp)
	register struct buf *bp;
{
	register int npf;
	register caddr_t addr = bp->b_un.b_addr;
	vm_offset_t kva;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");
	npf = btoc(round_page(bp->b_bufsize + ((int)addr & PGOFSET)));
	kva = (vm_offset_t)((int)addr & ~PGOFSET);
	kmem_free_wakeup(phys_map, kva, ctob(npf));
	bp->b_un.b_addr = bp->b_saveaddr;
	bp->b_saveaddr = NULL;
}

/*
 * Force reset the processor by invalidating the entire address space!
 */
void
cpu_reset() {

	/* force a shutdown by unmapping entire address space ! */
	bzero((caddr_t) PTD, NBPG);

	/* "good night, sweet prince .... <THUNK!>" */
	tlbflush(); 
	/* NOTREACHED */
	while(1);
}

/*
 * Grow the user stack to allow for 'sp'. This version grows the stack in
 *	chunks of SGROWSIZ.
 */
int
grow(p, sp)
	struct proc *p;
	int sp;
{
	unsigned int nss;
	caddr_t v;
	struct vmspace *vm = p->p_vmspace;

	if ((caddr_t)sp <= vm->vm_maxsaddr || (unsigned)sp >= (unsigned)USRSTACK)
	    return (1);

	nss = roundup(USRSTACK - (unsigned)sp, PAGE_SIZE);

	if (nss > p->p_rlimit[RLIMIT_STACK].rlim_cur)
		return (0);

	if (vm->vm_ssize && roundup(vm->vm_ssize << PAGE_SHIFT,
	    SGROWSIZ) < nss) {
		int grow_amount;
		/*
		 * If necessary, grow the VM that the stack occupies
		 * to allow for the rlimit. This allows us to not have
		 * to allocate all of the VM up-front in execve (which
		 * is expensive).
		 * Grow the VM by the amount requested rounded up to
		 * the nearest SGROWSIZ to provide for some hysteresis.
		 */
		grow_amount = roundup((nss - (vm->vm_ssize << PAGE_SHIFT)), SGROWSIZ);
		v = (char *)USRSTACK - roundup(vm->vm_ssize << PAGE_SHIFT,
		    SGROWSIZ) - grow_amount;
		/*
		 * If there isn't enough room to extend by SGROWSIZ, then
		 * just extend to the maximum size
		 */
		if (v < vm->vm_maxsaddr) {
			v = vm->vm_maxsaddr;
			grow_amount = MAXSSIZ - (vm->vm_ssize << PAGE_SHIFT);
		}
		if (vm_allocate(&vm->vm_map, (vm_offset_t *)&v,
		    grow_amount, FALSE) != KERN_SUCCESS) {
			return (0);
		}
		vm->vm_ssize += grow_amount >> PAGE_SHIFT;
	}

	return (1);
}
