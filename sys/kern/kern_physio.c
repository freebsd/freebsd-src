/*
 * Copyright (c) 1989, 1990, 1991, 1992 William F. Jolitz, TeleMuse
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
 *	$Id: kern_physio.c,v 1.7 1994/04/25 23:48:29 davidg Exp $
 */

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "conf.h"
#include "proc.h"
#include "malloc.h"
#include "vnode.h"
#include "vm/vm.h"
#include "vm/vm_page.h"
#include "specdev.h"

#define HOLD_WORKS_FOR_SHARING

/*
 * Driver interface to do "raw" I/O in the address space of a
 * user process directly for read and write operations..
 */

int
rawread(dev, uio)
	dev_t dev; struct uio *uio;
{
	return (uioapply(physio, (caddr_t) cdevsw[major(dev)].d_strategy,
				 (caddr_t) (u_long) dev, uio));
}

int
rawwrite(dev, uio)
	dev_t dev; struct uio *uio;
{
	return (uioapply(physio, (caddr_t) cdevsw[major(dev)].d_strategy,
				(caddr_t) (u_long) dev, uio));
}

static void
physwakeup(bp)
	struct buf *bp;
{
	wakeup((caddr_t) bp);
	bp->b_flags &= ~B_CALL;
}

int physio(strat, dev, bp, off, rw, base, len, p)
	d_strategy_t strat; 
	dev_t dev;
	struct buf *bp;
	int rw, off;
	caddr_t base;
	int *len;
	struct proc *p;
{
	int amttodo = *len;
	int error, amtdone;
	vm_prot_t ftype;
	vm_offset_t v, lastv, pa;
	caddr_t adr;
	int oldflags;
	int s;
	
	int bp_alloc = (bp == 0);

/*
 * keep the process from being swapped
 */
	oldflags = p->p_flag;
	p->p_flag |= SPHYSIO;

	rw = rw == UIO_READ ? B_READ : 0;

	/* create and build a buffer header for a transfer */

	if (bp_alloc) {
		bp = (struct buf *)getpbuf();
		bzero((char *)bp, sizeof(*bp));			/* 09 Sep 92*/
	} else {
		s = splbio();
		while (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			tsleep((caddr_t)bp, PRIBIO, "physbw", 0);
		}
		bp->b_flags |= B_BUSY;
		splx(s);
	}

	bp->b_proc = p;
	bp->b_dev = dev;
	bp->b_error = 0;
	bp->b_blkno = off/DEV_BSIZE;
	amtdone = 0;

	/* iteratively do I/O on as large a chunk as possible */
	do {
		bp->b_flags = B_BUSY | B_PHYS | B_CALL | rw;
		bp->b_iodone = physwakeup;
		bp->b_un.b_addr = base;
		/*
		 * Notice that b_bufsize is more owned by the buffer
		 * allocating entity, while b_bcount might be modified
		 * by the called I/O routines.  So after I/O is complete
		 * the only thing guaranteed to be unchanged is
		 * b_bufsize.
		 */
		bp->b_bcount = min (256*1024, amttodo);
		bp->b_bufsize = bp->b_bcount;

		/* first, check if accessible */
		if (rw == B_READ && !useracc(base, bp->b_bufsize, B_WRITE)) {
			error = EFAULT;
			goto errrtn;
		}
		if (rw == B_WRITE && !useracc(base, bp->b_bufsize, B_READ)) {
			error = EFAULT;
			goto errrtn;
		}

		/* update referenced and dirty bits, handle copy objects */
		if (rw == B_READ)
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;

		lastv = 0;
		for (adr = (caddr_t)trunc_page(base); adr < base + bp->b_bufsize;
			adr += NBPG) {
	
/*
 * make sure that the pde is valid and held
 */
			v = trunc_page(((vm_offset_t)vtopte(adr)));
			if (v != lastv) {
				vm_fault_quick(v, VM_PROT_READ);
				pa = pmap_extract(&p->p_vmspace->vm_pmap, v);
				vm_page_hold(PHYS_TO_VM_PAGE(pa));
				lastv = v;
			}

/*
 * do the vm_fault if needed, do the copy-on-write thing when
 * reading stuff off device into memory.
 */
			vm_fault_quick(adr, ftype);
			pa = pmap_extract(&p->p_vmspace->vm_pmap, (vm_offset_t) adr);
/*
 * hold the data page
 */
			vm_page_hold(PHYS_TO_VM_PAGE(pa));
		}

		vmapbuf(bp);

		/* perform transfer */
		(*strat)(bp);

		/* pageout daemon doesn't wait for pushed pages */
		s = splbio();
		while ((bp->b_flags & B_DONE) == 0)
			tsleep((caddr_t)bp, PRIBIO, "physstr", 0);
		splx(s);

		vunmapbuf(bp);

/*
 * unhold the pde, and data pages
 */
		lastv = 0;
		for (adr = (caddr_t)trunc_page(base); adr < base + bp->b_bufsize;
			adr += NBPG) {
			v = trunc_page(((vm_offset_t)vtopte(adr)));
			if (v != lastv) {
				pa = pmap_extract(&p->p_vmspace->vm_pmap, v);
				vm_page_unhold(PHYS_TO_VM_PAGE(pa));
				lastv = v;
			}
			pa = pmap_extract(&p->p_vmspace->vm_pmap, (vm_offset_t) adr);
			vm_page_unhold(PHYS_TO_VM_PAGE(pa));
		}
			

		/*
		 * in this case, we need to use b_bcount instead of
		 * b_bufsize.
		 */
		amtdone = bp->b_bcount - bp->b_resid;
		amttodo -= amtdone;
		base += amtdone;
		bp->b_blkno += amtdone/DEV_BSIZE;
	} while (amttodo && (bp->b_flags & B_ERROR) == 0 && amtdone > 0);

	error = bp->b_error;
errrtn:
	if (bp_alloc) {
		relpbuf(bp);
	} else {
		bp->b_flags &= ~B_BUSY;
		wakeup((caddr_t)bp);
	}
	*len = amttodo;

/*
 * allow the process to be swapped
 */
	p->p_flag &= ~SPHYSIO;
	p->p_flag |= (oldflags & SPHYSIO);

	return (error);
}
