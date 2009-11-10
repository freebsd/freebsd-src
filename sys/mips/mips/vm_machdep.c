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
 *	from: src/sys/i386/i386/vm_machdep.c,v 1.132.2.2 2000/08/26 04:19:26 yokota
 *	JNPR: vm_machdep.c,v 1.8.2.2 2007/08/16 15:59:17 girish
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/pltfm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>
#include <sys/mbuf.h>
#include <sys/sf_buf.h>

#ifndef NSFBUFS
#define	NSFBUFS		(512 + maxusers * 16)
#endif

static void	sf_buf_init(void *arg);
SYSINIT(sock_sf, SI_SUB_MBUF, SI_ORDER_ANY, sf_buf_init, NULL);

LIST_HEAD(sf_head, sf_buf);


/*
 * A hash table of active sendfile(2) buffers
 */
static struct sf_head *sf_buf_active;
static u_long sf_buf_hashmask;

#define	SF_BUF_HASH(m)	(((m) - vm_page_array) & sf_buf_hashmask)

static TAILQ_HEAD(, sf_buf) sf_buf_freelist;
static u_int	sf_buf_alloc_want;

/*
 * A lock used to synchronize access to the hash table and free list
 */
static struct mtx sf_buf_lock;

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(register struct thread *td1,register struct proc *p2,
    struct thread *td2,int flags)
{
	register struct proc *p1;
	struct pcb *pcb2;

	p1 = td1->td_proc;
	if ((flags & RFPROC) == 0)
		return;
	/* It is assumed that the vm_thread_alloc called
	 * cpu_thread_alloc() before cpu_fork is called.
	 */

	/* Point the pcb to the top of the stack */
	pcb2 = td2->td_pcb;

	/* Copy p1's pcb, note that in this case
	 * our pcb also includes the td_frame being copied
	 * too. The older mips2 code did an additional copy
	 * of the td_frame, for us thats not needed any
	 * longer (this copy does them both 
	 */
	bcopy(td1->td_pcb, pcb2, sizeof(*pcb2));

	/* Point mdproc and then copy over td1's contents
	 * md_proc is empty for MIPS
	 */
	td2->td_md.md_flags = td1->td_md.md_flags & MDTD_FPUSED;

	/*
	 * Set up return-value registers as fork() libc stub expects.
	 */
	td2->td_frame->v0 = 0;
	td2->td_frame->v1 = 1;
	td2->td_frame->a3 = 0;

	if (td1 == PCPU_GET(fpcurthread))
		MipsSaveCurFPState(td1);

	pcb2->pcb_context.val[PCB_REG_RA] = (register_t)fork_trampoline;
	/* Make sp 64-bit aligned */
	pcb2->pcb_context.val[PCB_REG_SP] = (register_t)(((vm_offset_t)td2->td_pcb &
	    ~(sizeof(__int64_t) - 1)) - STAND_FRAME_SIZE);
	pcb2->pcb_context.val[PCB_REG_S0] = (register_t)fork_return;
	pcb2->pcb_context.val[PCB_REG_S1] = (register_t)td2;
	pcb2->pcb_context.val[PCB_REG_S2] = (register_t)td2->td_frame;
	pcb2->pcb_context.val[PCB_REG_SR] = SR_INT_MASK;
	/*
	 * FREEBSD_DEVELOPERS_FIXME:
	 * Setup any other CPU-Specific registers (Not MIPS Standard)
	 * and/or bits in other standard MIPS registers (if CPU-Specific)
	 *  that are needed.
	 */

	td2->td_md.md_saved_intr = MIPS_SR_INT_IE;
	td2->td_md.md_spinlock_count = 1;
#ifdef TARGET_OCTEON
	pcb2->pcb_context.val[PCB_REG_SR] |= MIPS_SR_COP_2_BIT | MIPS32_SR_PX | MIPS_SR_UX | MIPS_SR_KX | MIPS_SR_SX;
#endif

}

/*
 * Intercept the return address from a freshly forked process that has NOT
 * been scheduled yet.
 *
 * This is needed to make kernel threads stay in kernel mode.
 */
void
cpu_set_fork_handler(struct thread *td, void (*func) __P((void *)), void *arg)
{
	/*
	 * Note that the trap frame follows the args, so the function
	 * is really called like this:	func(arg, frame);
	 */
	td->td_pcb->pcb_context.val[PCB_REG_S0] = (register_t) func;
	td->td_pcb->pcb_context.val[PCB_REG_S1] = (register_t) arg;
}

void
cpu_exit(struct thread *td)
{
}

void
cpu_thread_exit(struct thread *td)
{

	if (PCPU_GET(fpcurthread) == td)
		PCPU_GET(fpcurthread) = (struct thread *)0;
}

void
cpu_thread_free(struct thread *td)
{
}

void
cpu_thread_clean(struct thread *td)
{
}

void
cpu_thread_swapin(struct thread *td)
{
	pt_entry_t *pte;
	int i;

	/*
	 * The kstack may be at a different physical address now.
	 * Cache the PTEs for the Kernel stack in the machine dependent
	 * part of the thread struct so cpu_switch() can quickly map in
	 * the pcb struct and kernel stack.
	 */
	if (!(pte = pmap_segmap(kernel_pmap, td->td_md.md_realstack)))
		panic("cpu_thread_swapin: invalid segmap");
	pte += ((vm_offset_t)td->td_md.md_realstack >> PGSHIFT) & (NPTEPG - 1);

	for (i = 0; i < KSTACK_PAGES - 1; i++) {
		td->td_md.md_upte[i] = *pte & ~(PTE_RO|PTE_WIRED);
		pte++;
	}
}

void
cpu_thread_swapout(struct thread *td)
{
}

void
cpu_thread_alloc(struct thread *td)
{
	pt_entry_t *pte;
	int i;

	if(td->td_kstack & (1 << PAGE_SHIFT))
		td->td_md.md_realstack = td->td_kstack + PAGE_SIZE;
	else
		td->td_md.md_realstack = td->td_kstack;

	td->td_pcb = (struct pcb *)(td->td_md.md_realstack +
	    (td->td_kstack_pages - 1) * PAGE_SIZE) - 1;
	td->td_frame = &td->td_pcb->pcb_regs;

	if (!(pte = pmap_segmap(kernel_pmap, td->td_md.md_realstack)))
		panic("cpu_thread_alloc: invalid segmap");
	pte += ((vm_offset_t)td->td_md.md_realstack >> PGSHIFT) & (NPTEPG - 1);

	for (i = 0; i < KSTACK_PAGES - 1; i++) {
		td->td_md.md_upte[i] = *pte & ~(PTE_RO|PTE_WIRED);
		pte++;
	}
}

void
cpu_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *locr0 = td->td_frame;
	unsigned int code;
	int quad_syscall;

	code = locr0->v0;
	quad_syscall = 0;
	if (code == SYS_syscall)
		code = locr0->a0;
	else if (code == SYS___syscall) {
		code = _QUAD_LOWWORD ? locr0->a1 : locr0->a0;
		quad_syscall = 1;
	}

	switch (error) {
	case 0:
		if (quad_syscall && code != SYS_lseek) {
			/*
			 * System call invoked through the
			 * SYS___syscall interface but the
			 * return value is really just 32
			 * bits.
			 */
			locr0->v0 = td->td_retval[0];
			if (_QUAD_LOWWORD)
				locr0->v1 = td->td_retval[0];
			locr0->a3 = 0;
		} else {
			locr0->v0 = td->td_retval[0];
			locr0->v1 = td->td_retval[1];
			locr0->a3 = 0;
		}
		break;

	case ERESTART:
		locr0->pc = td->td_pcb->pcb_tpc;
		break;

	case EJUSTRETURN:
		break;	/* nothing to do */

	default:
		if (quad_syscall && code != SYS_lseek) {
			locr0->v0 = error;
			if (_QUAD_LOWWORD)
				locr0->v1 = error;
			locr0->a3 = 1;
		} else {
			locr0->v0 = error;
			locr0->a3 = 1;
		}
	}
}

/*
 * Initialize machine state (pcb and trap frame) for a new thread about to
 * upcall. Put enough state in the new thread's PCB to get it to go back
 * userret(), where we can intercept it again to set the return (upcall)
 * Address and stack, along with those from upcals that are from other sources
 * such as those generated in thread_userret() itself.
 */
void
cpu_set_upcall(struct thread *td, struct thread *td0)
{
	struct pcb *pcb2;

	/* Point the pcb to the top of the stack. */
	pcb2 = td->td_pcb;

	/*
	 * Copy the upcall pcb.  This loads kernel regs.
	 * Those not loaded individually below get their default
	 * values here.
	 *
	 * XXXKSE It might be a good idea to simply skip this as
	 * the values of the other registers may be unimportant.
	 * This would remove any requirement for knowing the KSE
	 * at this time (see the matching comment below for
	 * more analysis) (need a good safe default).
	 * In MIPS, the trapframe is the first element of the PCB
	 * and gets copied when we copy the PCB. No seperate copy
	 * is needed.
	 */
	bcopy(td0->td_pcb, pcb2, sizeof(*pcb2));

	/*
	 * Set registers for trampoline to user mode.
	 */

	pcb2->pcb_context.val[PCB_REG_RA] = (register_t)fork_trampoline;
	/* Make sp 64-bit aligned */
	pcb2->pcb_context.val[PCB_REG_SP] = (register_t)(((vm_offset_t)td->td_pcb &
	    ~(sizeof(__int64_t) - 1)) - STAND_FRAME_SIZE);
	pcb2->pcb_context.val[PCB_REG_S0] = (register_t)fork_return;
	pcb2->pcb_context.val[PCB_REG_S1] = (register_t)td;
	pcb2->pcb_context.val[PCB_REG_S2] = (register_t)td->td_frame;


	/* Dont set IE bit in SR. sched lock release will take care of it */
/* idle_mask is jmips pcb2->pcb_context.val[11] = (ALL_INT_MASK & idle_mask); */
	pcb2->pcb_context.val[PCB_REG_SR] = SR_INT_MASK;
#ifdef TARGET_OCTEON
	pcb2->pcb_context.val[PCB_REG_SR] |= MIPS_SR_COP_2_BIT | MIPS_SR_COP_0_BIT |
	  MIPS32_SR_PX | MIPS_SR_UX | MIPS_SR_KX | MIPS_SR_SX;
#endif

	/*
	 * FREEBSD_DEVELOPERS_FIXME:
	 * Setup any other CPU-Specific registers (Not MIPS Standard)
	 * that are needed.
	 */

	/* SMP Setup to release sched_lock in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_intr = MIPS_SR_INT_IE;
#if 0
	    /* Maybe we need to fix this? */
	td->td_md.md_saved_sr = ( (MIPS_SR_COP_2_BIT | MIPS_SR_COP_0_BIT) |
	                          (MIPS32_SR_PX | MIPS_SR_UX | MIPS_SR_KX | MIPS_SR_SX) |
	                          (MIPS_SR_INT_IE | MIPS_HARD_INT_MASK));
#endif
}

/*
 * Set that machine state for performing an upcall that has to
 * be done in thread_userret() so that those upcalls generated
 * in thread_userret() itself can be done as well.
 */
void
cpu_set_upcall_kse(struct thread *td, void (*entry)(void *), void *arg,
    stack_t *stack)
{
	struct trapframe *tf;
	u_int32_t sp;

	/*
	* At the point where a function is called, sp must be 8
	* byte aligned[for compatibility with 64-bit CPUs]
	* in ``See MIPS Run'' by D. Sweetman, p. 269
	* align stack */
	sp = ((uint32_t)(stack->ss_sp + stack->ss_size) & ~0x7) -
	    STAND_FRAME_SIZE;

	/*
	 * Set the trap frame to point at the beginning of the uts
	 * function.
	 */
	tf = td->td_frame;
	bzero(tf, sizeof(struct trapframe));
	tf->sp = (register_t)sp;
	tf->pc = (register_t)entry;
	tf->a0 = (register_t)arg;

	tf->sr = SR_KSU_USER | SR_EXL;
#ifdef TARGET_OCTEON
	tf->sr |=  MIPS_SR_INT_IE | MIPS_SR_COP_0_BIT | MIPS_SR_UX |
	  MIPS_SR_KX;
#endif
/*	tf->sr |= (ALL_INT_MASK & idle_mask) | SR_INT_ENAB; */
	/**XXX the above may now be wrong -- mips2 implements this as panic */
	/*
	 * FREEBSD_DEVELOPERS_FIXME:
	 * Setup any other CPU-Specific registers (Not MIPS Standard)
	 * that are needed.
	 */
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

/*
 * Implement the pre-zeroed page mechanism.
 * This routine is called from the idle loop.
 */

#define	ZIDLE_LO(v)	((v) * 2 / 3)
#define	ZIDLE_HI(v)	((v) * 4 / 5)

/*
 * Tell whether this address is in some physical memory region.
 * Currently used by the kernel coredump code in order to avoid
 * dumping non-memory physical address space.
 */
int
is_physical_memory(vm_offset_t addr)
{
	if (addr >= SDRAM_ADDR_START && addr <= SDRAM_ADDR_END)
		return 1;
	else
		return 0;
}

int
is_cacheable_mem(vm_offset_t pa)
{
	if ((pa >= SDRAM_ADDR_START && pa <= SDRAM_ADDR_END) ||
#ifdef FLASH_ADDR_START
	    (pa >= FLASH_ADDR_START && pa <= FLASH_ADDR_END))
#else
	    0)
#endif
		return 1;
	else
		return 0;
}

/*
 * Allocate a pool of sf_bufs (sendfile(2) or "super-fast" if you prefer. :-))
 */
static void
sf_buf_init(void *arg)
{
	struct sf_buf *sf_bufs;
	vm_offset_t sf_base;
	int i;

	nsfbufs = NSFBUFS;
	TUNABLE_INT_FETCH("kern.ipc.nsfbufs", &nsfbufs);

	sf_buf_active = hashinit(nsfbufs, M_TEMP, &sf_buf_hashmask);
	TAILQ_INIT(&sf_buf_freelist);
	sf_base = kmem_alloc_nofault(kernel_map, nsfbufs * PAGE_SIZE);
	sf_bufs = malloc(nsfbufs * sizeof(struct sf_buf), M_TEMP,
	    M_NOWAIT | M_ZERO);
	for (i = 0; i < nsfbufs; i++) {
		sf_bufs[i].kva = sf_base + i * PAGE_SIZE;
		TAILQ_INSERT_TAIL(&sf_buf_freelist, &sf_bufs[i], free_entry);
	}
	sf_buf_alloc_want = 0;
	mtx_init(&sf_buf_lock, "sf_buf", NULL, MTX_DEF);
}

/*
 * Allocate an sf_buf for the given vm_page.  On this machine, however, there
 * is no sf_buf object.	 Instead, an opaque pointer to the given vm_page is
 * returned.
 */
struct sf_buf *
sf_buf_alloc(struct vm_page *m, int flags)
{
	struct sf_head *hash_list;
	struct sf_buf *sf;
	int error;

	hash_list = &sf_buf_active[SF_BUF_HASH(m)];
	mtx_lock(&sf_buf_lock);
	LIST_FOREACH(sf, hash_list, list_entry) {
		if (sf->m == m) {
			sf->ref_count++;
			if (sf->ref_count == 1) {
				TAILQ_REMOVE(&sf_buf_freelist, sf, free_entry);
				nsfbufsused++;
				nsfbufspeak = imax(nsfbufspeak, nsfbufsused);
			}
			goto done;
		}
	}
	while ((sf = TAILQ_FIRST(&sf_buf_freelist)) == NULL) {
		if (flags & SFB_NOWAIT)
			goto done;
		sf_buf_alloc_want++;
		mbstat.sf_allocwait++;
		error = msleep(&sf_buf_freelist, &sf_buf_lock,
		    (flags & SFB_CATCH) ? PCATCH | PVM : PVM, "sfbufa", 0);
		sf_buf_alloc_want--;

		/*
		 * If we got a signal, don't risk going back to sleep.
		 */
		if (error)
			goto done;
	}
	TAILQ_REMOVE(&sf_buf_freelist, sf, free_entry);
	if (sf->m != NULL)
		LIST_REMOVE(sf, list_entry);
	LIST_INSERT_HEAD(hash_list, sf, list_entry);
	sf->ref_count = 1;
	sf->m = m;
	nsfbufsused++;
	nsfbufspeak = imax(nsfbufspeak, nsfbufsused);
	pmap_qenter(sf->kva, &sf->m, 1);
done:
	mtx_unlock(&sf_buf_lock);
	return (sf);
}

/*
 * Free the sf_buf.  In fact, do nothing because there are no resources
 * associated with the sf_buf.
 */
void
sf_buf_free(struct sf_buf *sf)
{
	mtx_lock(&sf_buf_lock);
	sf->ref_count--;
	if (sf->ref_count == 0) {
		TAILQ_INSERT_TAIL(&sf_buf_freelist, sf, free_entry);
		nsfbufsused--;
		if (sf_buf_alloc_want > 0)
			wakeup_one(&sf_buf_freelist);
	}
	mtx_unlock(&sf_buf_lock);
}

/*
 * Software interrupt handler for queued VM system processing.
 */
void
swi_vm(void *dummy)
{
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{

	/* TBD */
	return (0);
}

void
cpu_throw(struct thread *old, struct thread *new)
{

	func_2args_asmmacro(&mips_cpu_throw, old, new);
	panic("mips_cpu_throw() returned");
}
