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
 *	$Id: vm_machdep.c,v 1.90 1997/10/10 09:44:12 peter Exp $
 */

#include "npx.h"
#include "opt_bounce.h"
#include "opt_vm86.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#ifdef SMP
#include <machine/smp.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif

#ifdef BOUNCE_BUFFERS
static vm_offset_t
		vm_bounce_kva __P((int size, int waitok));
static void	vm_bounce_kva_free __P((vm_offset_t addr, vm_offset_t size,
					int now));
static vm_offset_t
		vm_bounce_page_find __P((int count));
static void	vm_bounce_page_free __P((vm_offset_t pa, int count));

static volatile int	kvasfreecnt;

caddr_t		bouncememory;
int		bouncepages;
static int	bpwait;
static vm_offset_t	*bouncepa;
static int		bmwait, bmfreeing;

#define BITS_IN_UNSIGNED (8*sizeof(unsigned))
static int		bounceallocarraysize;
static unsigned	*bounceallocarray;
static int		bouncefree;

#if defined(PC98) && defined (EPSON_BOUNCEDMA)
#define SIXTEENMEG (3840*4096)			/* 15MB boundary */
#else
#define SIXTEENMEG (4096*4096)
#endif
#define MAXBKVA 1024
int		maxbkva = MAXBKVA*PAGE_SIZE;

/* special list that can be used at interrupt time for eventual kva free */
static struct kvasfree {
	vm_offset_t addr;
	vm_offset_t size;
} kvaf[MAXBKVA];

/*
 * get bounce buffer pages (count physically contiguous)
 * (only 1 inplemented now)
 */
static vm_offset_t
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
			bit = ffs(~bounceallocarray[i]);
			if (bit) {
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

static void
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
static void
vm_bounce_page_free(pa, count)
	vm_offset_t pa;
	int count;
{
	int allocindex;
	int index;
	int bit;

	if (count != 1)
		panic("vm_bounce_page_free -- no support for > 1 page yet!!!");

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
static vm_offset_t
vm_bounce_kva(size, waitok)
	int size;
	int waitok;
{
	int i;
	vm_offset_t kva = 0;
	vm_offset_t off;
	int s = splbio();
more:
	if (!bmfreeing && kvasfreecnt) {
		bmfreeing = 1;
		for (i = 0; i < kvasfreecnt; i++) {
			for(off=0;off<kvaf[i].size;off+=PAGE_SIZE) {
				pmap_kremove( kvaf[i].addr + off);
			}
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
		return 0;
	}

	if ((kva = kmem_alloc_pageable(io_map, size)) == 0) {
		if( !waitok) {
			splx(s);
			return 0;
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
		kva = (vm_offset_t) malloc(count*PAGE_SIZE, M_TEMP, M_WAITOK);
		return kva;
	}
	kva = vm_bounce_kva(count*PAGE_SIZE, 1);
	for(i=0;i<count;i++) {
		pa = vm_bounce_page_find(1);
		pmap_kenter(kva + i * PAGE_SIZE, pa);
	}
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
		pa = pmap_kextract(kva + i * PAGE_SIZE);
		vm_bounce_page_free(pa, 1);
	}
	vm_bounce_kva_free(kva, count*PAGE_SIZE, 0);
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
	int i;

	if (bouncepages == 0)
		return;

	if (bp->b_flags & B_BOUNCE) {
		printf("vm_bounce_alloc: called recursively???\n");
		return;
	}

	if (bp->b_bufsize < bp->b_bcount) {
		printf(
		    "vm_bounce_alloc: b_bufsize(0x%lx) < b_bcount(0x%lx) !!\n",
			bp->b_bufsize, bp->b_bcount);
		panic("vm_bounce_alloc");
	}

/*
 *  This is not really necessary
 *	if( bp->b_bufsize != bp->b_bcount) {
 *		printf("size: %d, count: %d\n", bp->b_bufsize, bp->b_bcount);
 *	}
 */


	vastart = (vm_offset_t) bp->b_data;
	vaend = (vm_offset_t) bp->b_data + bp->b_bufsize;

	vapstart = trunc_page(vastart);
	vapend = round_page(vaend);
	countvmpg = (vapend - vapstart) / PAGE_SIZE;

/*
 * if any page is above 16MB, then go into bounce-buffer mode
 */
	va = vapstart;
	for (i = 0; i < countvmpg; i++) {
		pa = pmap_kextract(va);
		if (pa >= SIXTEENMEG)
			++dobounceflag;
		if( pa == 0)
			panic("vm_bounce_alloc: Unmapped page");
		va += PAGE_SIZE;
	}
	if (dobounceflag == 0)
		return;

	if (bouncepages < dobounceflag)
		panic("Not enough bounce buffers!!!");

/*
 * allocate a replacement kva for b_addr
 */
	kva = vm_bounce_kva(countvmpg*PAGE_SIZE, 1);
#if 0
	printf("%s: vapstart: %x, vapend: %x, countvmpg: %d, kva: %x ",
		(bp->b_flags & B_READ) ? "read":"write",
			vapstart, vapend, countvmpg, kva);
#endif
	va = vapstart;
	for (i = 0; i < countvmpg; i++) {
		pa = pmap_kextract(va);
		if (pa >= SIXTEENMEG) {
			/*
			 * allocate a replacement page
			 */
			vm_offset_t bpa = vm_bounce_page_find(1);
			pmap_kenter(kva + (PAGE_SIZE * i), bpa);
#if 0
			printf("r(%d): (%x,%x,%x) ", i, va, pa, bpa);
#endif
			/*
			 * if we are writing, the copy the data into the page
			 */
			if ((bp->b_flags & B_READ) == 0) {
				bcopy((caddr_t) va, (caddr_t) kva + (PAGE_SIZE * i), PAGE_SIZE);
			}
		} else {
			/*
			 * use original page
			 */
			pmap_kenter(kva + (PAGE_SIZE * i), pa);
		}
		va += PAGE_SIZE;
	}

/*
 * flag the buffer as being bounced
 */
	bp->b_flags |= B_BOUNCE;
/*
 * save the original buffer kva
 */
	bp->b_savekva = bp->b_data;
/*
 * put our new kva into the buffer (offset by original offset)
 */
	bp->b_data = (caddr_t) (((vm_offset_t) kva) |
				((vm_offset_t) bp->b_savekva & PAGE_MASK));
#if 0
	printf("b_savekva: %x, newva: %x\n", bp->b_savekva, bp->b_data);
#endif
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
	bouncekva = (vm_offset_t) bp->b_data;
/*
	printf("free: %d ", bp->b_bufsize);
*/

/*
 * check every page in the kva space for b_addr
 */
	for (i = 0; i < bp->b_bufsize; ) {
		vm_offset_t mybouncepa;
		vm_offset_t copycount;

		copycount = round_page(bouncekva + 1) - bouncekva;
		mybouncepa = pmap_kextract(trunc_page(bouncekva));

/*
 * if this is a bounced pa, then process as one
 */
		if ( mybouncepa != pmap_kextract( trunc_page( origkva))) {
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

/*
			printf("(kva: %x, pa: %x)", bouncekva, mybouncepa);
*/
			vm_bounce_page_free(mybouncepa, 1);
		}

		origkva += copycount;
		bouncekva += copycount;
		i += copycount;
	}

/*
	printf("\n");
*/
/*
 * add the old kva into the "to free" list
 */

	bouncekva= trunc_page((vm_offset_t) bp->b_data);
	bouncekvaend= round_page((vm_offset_t)bp->b_data + bp->b_bufsize);

/*
	printf("freeva: %d\n", (bouncekvaend - bouncekva) / PAGE_SIZE);
*/
	vm_bounce_kva_free( bouncekva, (bouncekvaend - bouncekva), 0);
	bp->b_data = bp->b_savekva;
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
	int i;

	kvasfreecnt = 0;

	if (bouncepages == 0)
		return;

	bounceallocarraysize = (bouncepages + BITS_IN_UNSIGNED - 1) / BITS_IN_UNSIGNED;
	bounceallocarray = malloc(bounceallocarraysize * sizeof(unsigned), M_TEMP, M_NOWAIT);

	if (!bounceallocarray)
		panic("Cannot allocate bounce resource array");

	bouncepa = malloc(bouncepages * sizeof(vm_offset_t), M_TEMP, M_NOWAIT);
	if (!bouncepa)
		panic("Cannot allocate physical memory array");

	for(i=0;i<bounceallocarraysize;i++) {
		bounceallocarray[i] = 0xffffffff;
	}

	for(i=0;i<bouncepages;i++) {
		vm_offset_t pa;
		if( (pa = pmap_kextract((vm_offset_t) bouncememory + i * PAGE_SIZE)) >= SIXTEENMEG)
			panic("bounce memory out of range");
		if( pa == 0)
			panic("bounce memory not resident");
		bouncepa[i] = pa;
		bounceallocarray[i/(8*sizeof(int))] &= ~(1<<(i%(8*sizeof(int))));
	}
	bouncefree = bouncepages;

}
#endif /* BOUNCE_BUFFERS */

/*
 * quick version of vm_fault
 */
void
vm_fault_quick(v, prot)
	caddr_t v;
	int prot;
{
	if (prot & VM_PROT_WRITE)
		subyte(v, fubyte(v));
	else
		fubyte(v);
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	struct pcb *pcb2 = &p2->p_addr->u_pcb;

	/* Ensure that p1's pcb is up to date. */
	if (npxproc == p1)
		npxsave(&p1->p_addr->u_pcb.pcb_savefpu);

	/* Copy p1's pcb. */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;

	/*
	 * Create a new fresh stack for the new process.
	 * Copy the trap frame for the return to user mode as if from a
	 * syscall.  This copies the user mode register values.
	 */
	p2->p_md.md_regs = (struct trapframe *)
			   ((int)p2->p_addr + UPAGES * PAGE_SIZE) - 1;
	*p2->p_md.md_regs = *p1->p_md.md_regs;

	/*
	 * Set registers for trampoline to user mode.  Leave space for the
	 * return address on stack.  These are the kernel mode register values.
	 */
	pcb2->pcb_cr3 = vtophys(p2->p_vmspace->vm_pmap.pm_pdir);
	pcb2->pcb_edi = p2->p_md.md_regs->tf_edi;
	pcb2->pcb_esi = (int)fork_return;
	pcb2->pcb_ebp = p2->p_md.md_regs->tf_ebp;
	pcb2->pcb_esp = (int)p2->p_md.md_regs - sizeof(void *);
	pcb2->pcb_ebx = (int)p2;
	pcb2->pcb_eip = (int)fork_trampoline;
	/*
	 * pcb2->pcb_ldt:	duplicated below, if necessary.
	 * pcb2->pcb_ldt_len:	cloned above.
	 * pcb2->pcb_savefpu:	cloned above.
	 * pcb2->pcb_flags:	cloned above (always 0 here?).
	 * pcb2->pcb_onfault:	cloned above (always NULL here?).
	 */

#ifdef VM86
	/*
	 * XXX don't copy the i/o pages.  this should probably be fixed.
	 */
	pcb2->pcb_ext = 0;
#endif

#ifdef USER_LDT
        /* Copy the LDT, if necessary. */
        if (pcb2->pcb_ldt != 0) {
                union descriptor *new_ldt;
                size_t len = pcb2->pcb_ldt_len * sizeof(union descriptor);

                new_ldt = (union descriptor *)kmem_alloc(kernel_map, len);
                bcopy(pcb2->pcb_ldt, new_ldt, len);
                pcb2->pcb_ldt = (caddr_t)new_ldt;
        }
#endif

	/*
	 * Now, cpu_switch() can schedule the new process.
	 * pcb_esp is loaded pointing to the cpu_switch() stack frame
	 * containing the return address when exiting cpu_switch.
	 * This will normally be to proc_trampoline(), which will have
	 * %ebx loaded with the new proc's pointer.  proc_trampoline()
	 * will set up a stack to call fork_return(p, frame); to complete
	 * the return to user-mode.
	 */
}

/*
 * Intercept the return address from a freshly forked process that has NOT
 * been scheduled yet.
 *
 * This is needed to make kernel threads stay in kernel mode.
 */
void
cpu_set_fork_handler(p, func, arg)
	struct proc *p;
	void (*func) __P((void *));
	void *arg;
{
	/*
	 * Note that the trap frame follows the args, so the function
	 * is really called like this:  func(arg, frame);
	 */
	p->p_addr->u_pcb.pcb_esi = (int) func;	/* function */
	p->p_addr->u_pcb.pcb_ebx = (int) arg;	/* first arg */
}

void
cpu_exit(p)
	register struct proc *p;
{
#if defined(USER_LDT) || defined(VM86)
	struct pcb *pcb = &p->p_addr->u_pcb; 
#endif

#if NNPX > 0
	npxexit(p);
#endif	/* NNPX */
#ifdef VM86
	if (pcb->pcb_ext != 0) {
	        /* 
		 * XXX do we need to move the TSS off the allocated pages 
		 * before freeing them?  (not done here)
		 */
		kmem_free(kernel_map, (vm_offset_t)pcb->pcb_ext,
		    ctob(IOPAGES + 1));
		pcb->pcb_ext = 0;
	}
#endif
#ifdef USER_LDT
	if (pcb->pcb_ldt != 0) {
		if (pcb == curpcb)
			lldt(GSEL(GUSERLDT_SEL, SEL_KPL));
		kmem_free(kernel_map, (vm_offset_t)pcb->pcb_ldt,
			pcb->pcb_ldt_len * sizeof(union descriptor));
		pcb->pcb_ldt_len = (int)pcb->pcb_ldt = 0;
	}
#endif
	cnt.v_swtch++;
	cpu_switch(p);
	panic("cpu_exit");
}

void
cpu_wait(p)
	struct proc *p;
{
	/* drop per-process resources */
	pmap_dispose_proc(p);
	vmspace_free(p->p_vmspace);
}

/*
 * Dump the machine specific header information at the start of a core dump.
 */
int
cpu_coredump(p, vp, cred)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
{

	return (vn_rdwr(UIO_WRITE, vp, (caddr_t) p->p_addr, ctob(UPAGES),
	    (off_t)0, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *)NULL,
	    p));
}

#ifdef notyet
static void
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
#endif

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
	register caddr_t addr, v, kva;
	vm_offset_t pa;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	for (v = bp->b_saveaddr, addr = (caddr_t)trunc_page(bp->b_data);
	    addr < bp->b_data + bp->b_bufsize;
	    addr += PAGE_SIZE, v += PAGE_SIZE) {
		/*
		 * Do the vm_fault if needed; do the copy-on-write thing
		 * when reading stuff off device into memory.
		 */
		vm_fault_quick(addr,
			(bp->b_flags&B_READ)?(VM_PROT_READ|VM_PROT_WRITE):VM_PROT_READ);
		pa = trunc_page(pmap_kextract((vm_offset_t) addr));
		if (pa == 0)
			panic("vmapbuf: page not present");
		vm_page_hold(PHYS_TO_VM_PAGE(pa));
		pmap_kenter((vm_offset_t) v, pa);
	}

	kva = bp->b_saveaddr;
	bp->b_saveaddr = bp->b_data;
	bp->b_data = kva + (((vm_offset_t) bp->b_data) & PAGE_MASK);
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
void
vunmapbuf(bp)
	register struct buf *bp;
{
	register caddr_t addr;
	vm_offset_t pa;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	for (addr = (caddr_t)trunc_page(bp->b_data);
	    addr < bp->b_data + bp->b_bufsize;
	    addr += PAGE_SIZE) {
		pa = trunc_page(pmap_kextract((vm_offset_t) addr));
		pmap_kremove((vm_offset_t) addr);
		vm_page_unhold(PHYS_TO_VM_PAGE(pa));
	}

	bp->b_data = bp->b_saveaddr;
}

/*
 * Force reset the processor by invalidating the entire address space!
 */
void
cpu_reset() {
#ifdef PC98
	/*
	 * Attempt to do a CPU reset via CPU reset port.
	 */
	asm("cli");
	outb(0x37, 0x0f);		/* SHUT0 = 0. */
	outb(0x37, 0x0b);		/* SHUT1 = 0. */
	outb(0xf0, 0x00);		/* Reset. */
#else
	/*
	 * Attempt to do a CPU reset via the keyboard controller,
	 * do not turn of the GateA20, as any machine that fails
	 * to do the reset here would then end up in no man's land.
	 */

#if !defined(BROKEN_KEYBOARD_RESET)
	outb(IO_KBD + 4, 0xFE);
	DELAY(500000);	/* wait 0.5 sec to see if that did it */
	printf("Keyboard reset did not work, attempting CPU shutdown\n");
	DELAY(1000000);	/* wait 1 sec for printf to complete */
#endif
#endif /* PC98 */
	/* force a shutdown by unmapping entire address space ! */
	bzero((caddr_t) PTD, PAGE_SIZE);

	/* "good night, sweet prince .... <THUNK!>" */
	invltlb();
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
	u_int sp;
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
		if ((grow_amount == 0) || (vm_map_find(&vm->vm_map, NULL, 0, (vm_offset_t *)&v,
		    grow_amount, FALSE, VM_PROT_ALL, VM_PROT_ALL, 0) != KERN_SUCCESS)) {
			return (0);
		}
		vm->vm_ssize += grow_amount >> PAGE_SHIFT;
	}

	return (1);
}

/*
 * Implement the pre-zeroed page mechanism.
 * This routine is called from the idle loop.
 */
int
vm_page_zero_idle()
{
	static int free_rover;
	vm_page_t m;
	int s;

#ifdef WRONG
	if (cnt.v_free_count <= cnt.v_interrupt_free_min) 
		return (0);
#endif
	/*
	 * XXX
	 * We stop zeroing pages when there are sufficent prezeroed pages.
	 * This threshold isn't really needed, except we want to
	 * bypass unneeded calls to vm_page_list_find, and the
	 * associated cache flush and latency.  The pre-zero will
	 * still be called when there are significantly more
	 * non-prezeroed pages than zeroed pages.  The threshold
	 * of half the number of reserved pages is arbitrary, but
	 * approximately the right amount.  Eventually, we should
	 * perhaps interrupt the zero operation when a process
	 * is found to be ready to run.
	 */
	if (cnt.v_free_count - vm_page_zero_count <= cnt.v_free_reserved / 2)
		return (0);
#ifdef SMP
	get_mplock();
#endif
	s = splvm();
	enable_intr();
	m = vm_page_list_find(PQ_FREE, free_rover);
	if (m != NULL) {
		--(*vm_page_queues[m->queue].lcnt);
		TAILQ_REMOVE(vm_page_queues[m->queue].pl, m, pageq);
		splx(s);
#ifdef SMP
		rel_mplock();
#endif
		pmap_zero_page(VM_PAGE_TO_PHYS(m));
#ifdef SMP
		get_mplock();
#endif
		(void)splvm();
		m->queue = PQ_ZERO + m->pc;
		++(*vm_page_queues[m->queue].lcnt);
		TAILQ_INSERT_HEAD(vm_page_queues[m->queue].pl, m, pageq);
		free_rover = (free_rover + PQ_PRIME3) & PQ_L2_MASK;
		++vm_page_zero_count;
	}
	splx(s);
	disable_intr();
#ifdef SMP
	rel_mplock();
#endif
	return (1);
}

/*
 * Tell whether this address is in some physical memory region.
 * Currently used by the kernel coredump code in order to avoid
 * dumping the ``ISA memory hole'' which could cause indefinite hangs,
 * or other unpredictable behaviour.
 */

#include "isa.h"

int
is_physical_memory(addr)
	vm_offset_t addr;
{

#if NISA > 0
	/* The ISA ``memory hole''. */
	if (addr >= 0xa0000 && addr < 0x100000)
		return 0;
#endif

	/*
	 * stuff other tests for known memory-mapped devices (PCI?)
	 * here
	 */

	return 1;
}
