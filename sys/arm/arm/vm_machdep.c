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
 * Redistribution and use in source and binary :forms, with or without
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/sf_buf.h>
#include <sys/unistd.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_pageout.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include <machine/md_var.h>

#ifndef NSFBUFS
#define NSFBUFS		(512 + maxusers * 16)
#endif

#ifndef ARM_USE_SMALL_ALLOC
static void     sf_buf_init(void *arg);
SYSINIT(sock_sf, SI_SUB_MBUF, SI_ORDER_ANY, sf_buf_init, NULL)

LIST_HEAD(sf_head, sf_buf);
	

/*
 * A hash table of active sendfile(2) buffers
 */
static struct sf_head *sf_buf_active;
static u_long sf_buf_hashmask;

#define SF_BUF_HASH(m)  (((m) - vm_page_array) & sf_buf_hashmask)

static TAILQ_HEAD(, sf_buf) sf_buf_freelist;
static u_int    sf_buf_alloc_want;

/*
 * A lock used to synchronize access to the hash table and free list
 */
static struct mtx sf_buf_lock;
#endif

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(register struct thread *td1, register struct proc *p2,
    struct thread *td2, int flags)
{
	struct pcb *pcb1, *pcb2;
	struct trapframe *tf;
	struct switchframe *sf;
	struct mdproc *mdp2;

	if ((flags & RFPROC) == 0)
		return;
	pcb1 = td1->td_pcb;
	pcb2 = (struct pcb *)(td2->td_kstack + td2->td_kstack_pages * PAGE_SIZE) - 1;
#ifdef __XSCALE__
	pmap_use_minicache(td2->td_kstack, td2->td_kstack_pages * PAGE_SIZE);
	if (td2->td_altkstack)
		pmap_use_minicache(td2->td_altkstack, td2->td_altkstack_pages *
		    PAGE_SIZE);
#endif
	td2->td_pcb = pcb2;
	bcopy(td1->td_pcb, pcb2, sizeof(*pcb2));
	mdp2 = &p2->p_md;
	bcopy(&td1->td_proc->p_md, mdp2, sizeof(*mdp2));
	pcb2->un_32.pcb32_und_sp = td2->td_kstack + USPACE_UNDEF_STACK_TOP;
	pcb2->un_32.pcb32_sp = td2->td_kstack +
	    USPACE_SVC_STACK_TOP - sizeof(*pcb2);
	pmap_activate(td2);
	td2->td_frame = tf =
	    (struct trapframe *)pcb2->un_32.pcb32_sp - 1;
	*tf = *td1->td_frame;
	sf = (struct switchframe *)tf - 1;
	sf->sf_r4 = (u_int)fork_return;
	sf->sf_r5 = (u_int)td2;
	sf->sf_pc = (u_int)fork_trampoline;
	tf->tf_spsr &= ~PSR_C_bit;
	tf->tf_r0 = 0;
	tf->tf_r1 = 0;
	pcb2->un_32.pcb32_sp = (u_int)sf;

	/* Setup to release spin count in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	td2->td_md.md_saved_cspr = 0;
	td2->td_md.md_tp = *(uint32_t **)ARM_TP_ADDRESS;
}
				
void
cpu_thread_swapin(struct thread *td)
{
}       

void    
cpu_thread_swapout(struct thread *td)
{       
}

/*
 * Detatch mapped page and release resources back to the system.
 */
void
sf_buf_free(struct sf_buf *sf)
{
#ifndef ARM_USE_SMALL_ALLOC
	 mtx_lock(&sf_buf_lock);
	 sf->ref_count--;
	 if (sf->ref_count == 0) {
		 TAILQ_INSERT_TAIL(&sf_buf_freelist, sf, free_entry);
		 nsfbufsused--;
		 if (sf_buf_alloc_want > 0)
			 wakeup_one(&sf_buf_freelist);
	 }
	 mtx_unlock(&sf_buf_lock);				 
#endif
}

#ifndef ARM_USE_SMALL_ALLOC
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
#endif

/*
 * Get an sf_buf from the freelist. Will block if none are available.
 */
struct sf_buf *
sf_buf_alloc(struct vm_page *m, int flags)
{
#ifdef ARM_USE_SMALL_ALLOC
	return ((struct sf_buf *)m);
#else
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
	pmap_kenter(sf->kva, VM_PAGE_TO_PHYS(sf->m));
done:
	mtx_unlock(&sf_buf_lock);
	return (sf);
#endif
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
	struct trapframe *tf;
	struct switchframe *sf;

	bcopy(td0->td_frame, td->td_frame, sizeof(struct trapframe));
	bcopy(td0->td_pcb, td->td_pcb, sizeof(struct pcb));
	tf = td->td_frame;
	sf = (struct switchframe *)tf - 1;
	sf->sf_r4 = (u_int)fork_return;
	sf->sf_r5 = (u_int)td;
	sf->sf_pc = (u_int)fork_trampoline;
	tf->tf_spsr &= ~PSR_C_bit;
	tf->tf_r0 = 0;
	td->td_pcb->un_32.pcb32_sp = (u_int)sf;
	td->td_pcb->un_32.pcb32_und_sp = td->td_kstack + USPACE_UNDEF_STACK_TOP;

	/* Setup to release spin count in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_cspr = 0;
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
	struct trapframe *tf = td->td_frame;

	tf->tf_usr_sp = ((int)stack->ss_sp + stack->ss_size
	    - sizeof(struct trapframe)) & ~7;
	tf->tf_pc = (int)entry;
	tf->tf_r0 = (int)arg;
	tf->tf_spsr = PSR_USR32_MODE;
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{

	if (td != curthread)
		td->td_md.md_tp = tls_base;
	else {
		critical_enter();
		*(void **)ARM_TP_ADDRESS = tls_base;
		critical_exit();
	}
	return (0);
}

void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_setup(struct thread *td)
{
	td->td_pcb = (struct pcb *)(td->td_kstack + td->td_kstack_pages * 
	    PAGE_SIZE) - 1;
	td->td_frame = (struct trapframe *)
	    ((u_int)td->td_kstack + USPACE_SVC_STACK_TOP - sizeof(struct pcb)) - 1;
#ifdef __XSCALE__
	pmap_use_minicache(td->td_kstack, td->td_kstack_pages * PAGE_SIZE);
#endif  
		
}
void
cpu_thread_clean(struct thread *td)
{
}

/*
 * Intercept the return address from a freshly forked process that has NOT
 * been scheduled yet.
 *
 * This is needed to make kernel threads stay in kernel mode.
 */
void
cpu_set_fork_handler(struct thread *td, void (*func)(void *), void *arg)
{
	struct switchframe *sf;
	struct trapframe *tf;
	
	tf = td->td_frame;
	sf = (struct switchframe *)tf - 1;
	sf->sf_r4 = (u_int)func;
	sf->sf_r5 = (u_int)arg;
	td->td_pcb->un_32.pcb32_sp = (u_int)sf;
}

/*
 * Software interrupt handler for queued VM system processing.
 */   
void  
swi_vm(void *dummy)
{
	
	if (busdma_swi_pending)
		busdma_swi();
}

void
cpu_exit(struct thread *td)
{
}

#define BITS_PER_INT	(8 * sizeof(int))
vm_offset_t arm_nocache_startaddr;
static int arm_nocache_allocated[ARM_NOCACHE_KVA_SIZE / (PAGE_SIZE * 
    BITS_PER_INT)];

/*
 * Functions to map and unmap memory non-cached into KVA the kernel won't try 
 * to allocate. The goal is to provide uncached memory to busdma, to honor
 * BUS_DMA_COHERENT. 
 * We can allocate at most ARM_NOCACHE_KVA_SIZE bytes. 
 * The allocator is rather dummy, each page is represented by a bit in
 * a bitfield, 0 meaning the page is not allocated, 1 meaning it is.
 * As soon as it finds enough contiguous pages to satisfy the request,
 * it returns the address.
 */
void *
arm_remap_nocache(void *addr, vm_size_t size)
{
	int i, j;
	
	size = round_page(size);
	for (i = 0; i < MIN(ARM_NOCACHE_KVA_SIZE / (PAGE_SIZE * BITS_PER_INT),
	    ARM_TP_ADDRESS); i++) {
		if (!(arm_nocache_allocated[i / BITS_PER_INT] & (1 << (i % 
		    BITS_PER_INT)))) {
			for (j = i; j < i + (size / (PAGE_SIZE)); j++)
				if (arm_nocache_allocated[j / BITS_PER_INT] &
				    (1 << (j % BITS_PER_INT)))
					break;
			if (j == i + (size / (PAGE_SIZE)))
				break;
		}
	}
	if (i < MIN(ARM_NOCACHE_KVA_SIZE / (PAGE_SIZE * BITS_PER_INT), 
	    ARM_TP_ADDRESS)) {
		vm_offset_t tomap = arm_nocache_startaddr + i * PAGE_SIZE;
		void *ret = (void *)tomap;
		vm_paddr_t physaddr = vtophys((vm_offset_t)addr);
		
		for (; tomap < (vm_offset_t)ret + size; tomap += PAGE_SIZE,
		    physaddr += PAGE_SIZE, i++) {
			pmap_kenter_nocache(tomap, physaddr);
			arm_nocache_allocated[i / BITS_PER_INT] |= 1 << (i % 
			    BITS_PER_INT);
		}
		return (ret);
	}
	return (NULL);
}

void
arm_unmap_nocache(void *addr, vm_size_t size)
{
	vm_offset_t raddr = (vm_offset_t)addr;
	int i;

	size = round_page(size);
	i = (raddr - arm_nocache_startaddr) / (PAGE_SIZE);
	for (; size > 0; size -= PAGE_SIZE, i++)
		arm_nocache_allocated[i / BITS_PER_INT] &= ~(1 << (i % 
		    BITS_PER_INT));
}

#ifdef ARM_USE_SMALL_ALLOC

static TAILQ_HEAD(,arm_small_page) pages_normal = 
	TAILQ_HEAD_INITIALIZER(pages_normal);
static TAILQ_HEAD(,arm_small_page) pages_wt = 
	TAILQ_HEAD_INITIALIZER(pages_wt);
static TAILQ_HEAD(,arm_small_page) free_pgdesc =
	TAILQ_HEAD_INITIALIZER(free_pgdesc);

extern uma_zone_t l2zone;

struct mtx smallalloc_mtx;

MALLOC_DEFINE(M_VMSMALLALLOC, "vm_small_alloc", "VM Small alloc data");

vm_offset_t alloc_firstaddr;

vm_offset_t
arm_ptovirt(vm_paddr_t pa)
{
	int i;
	vm_offset_t addr = alloc_firstaddr;

	KASSERT(alloc_firstaddr != 0, ("arm_ptovirt called to early ?"));
	for (i = 0; dump_avail[i + 1]; i += 2) {
		if (pa >= dump_avail[i] && pa < dump_avail[i + 1])
			break;
		addr += (dump_avail[i + 1] & L1_S_FRAME) + L1_S_SIZE -
		    (dump_avail[i] & L1_S_FRAME);
	}
	KASSERT(dump_avail[i + 1] != 0, ("Trying to access invalid physical address"));
	return (addr + (pa - (dump_avail[i] & L1_S_FRAME)));
}

void
arm_init_smallalloc(void)
{
	vm_offset_t to_map = 0, mapaddr;
	int i;
	
	/* 
	 * We need to use dump_avail and not phys_avail, since we want to
	 * map the whole memory and not just the memory available to the VM
	 * to be able to do a pa => va association for any address.
	 */
	   
	for (i = 0; dump_avail[i + 1]; i+= 2) {
		to_map += (dump_avail[i + 1] & L1_S_FRAME) + L1_S_SIZE -
		    (dump_avail[i] & L1_S_FRAME);
	}
	alloc_firstaddr = mapaddr = KERNBASE - to_map;
	for (i = 0; dump_avail[i + 1]; i+= 2) {
		vm_offset_t size = (dump_avail[i + 1] & L1_S_FRAME) +
		    L1_S_SIZE - (dump_avail[i] & L1_S_FRAME);
		vm_offset_t did = 0;
		while (size > 0 ) {
			pmap_kenter_section(mapaddr, 
			    (dump_avail[i] & L1_S_FRAME) + did, SECTION_CACHE);
			mapaddr += L1_S_SIZE;
			did += L1_S_SIZE;
			size -= L1_S_SIZE;
		}
	}
}

void
arm_add_smallalloc_pages(void *list, void *mem, int bytes, int pagetable)
{
	struct arm_small_page *pg;
	
	bytes &= ~PAGE_MASK;
	while (bytes > 0) {
		pg = (struct arm_small_page *)list;
		pg->addr = mem;
		if (pagetable)
			TAILQ_INSERT_HEAD(&pages_wt, pg, pg_list);
		else
			TAILQ_INSERT_HEAD(&pages_normal, pg, pg_list);
		list = (char *)list + sizeof(*pg);
		mem = (char *)mem + PAGE_SIZE;
		bytes -= PAGE_SIZE;
	}
}

void *
uma_small_alloc(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	void *ret;
	struct arm_small_page *sp;
	TAILQ_HEAD(,arm_small_page) *head;
	static vm_pindex_t color;
	vm_page_t m;

	*flags = UMA_SLAB_PRIV;
	/*
	 * For CPUs where we setup page tables as write back, there's no
	 * need to maintain two separate pools.
	 */
	if (zone == l2zone && pte_l1_s_cache_mode != pte_l1_s_cache_mode_pt)
		head = (void *)&pages_wt;
	else
		head = (void *)&pages_normal;

	mtx_lock(&smallalloc_mtx);
	sp = TAILQ_FIRST(head);

	if (!sp) {
		int pflags;

		mtx_unlock(&smallalloc_mtx);
		if (zone == l2zone &&
		    pte_l1_s_cache_mode != pte_l1_s_cache_mode_pt) {
			*flags = UMA_SLAB_KMEM;
			ret = ((void *)kmem_malloc(kmem_map, bytes, M_NOWAIT));
			return (ret);
		}
		if ((wait & (M_NOWAIT|M_USE_RESERVE)) == M_NOWAIT)
			pflags = VM_ALLOC_INTERRUPT;
		else
			pflags = VM_ALLOC_SYSTEM;
		if (wait & M_ZERO)
			pflags |= VM_ALLOC_ZERO;
		for (;;) {
			m = vm_page_alloc(NULL, color++, 
			    pflags | VM_ALLOC_NOOBJ);
			if (m == NULL) {
				if (wait & M_NOWAIT)
					return (NULL);
				VM_WAIT;
			} else
				break;
		}
		ret = (void *)arm_ptovirt(VM_PAGE_TO_PHYS(m));
		if ((wait & M_ZERO) && (m->flags & PG_ZERO) == 0)
			bzero(ret, PAGE_SIZE);
		return (ret);
	}    
	TAILQ_REMOVE(head, sp, pg_list);
	TAILQ_INSERT_HEAD(&free_pgdesc, sp, pg_list);
	ret = sp->addr;
	mtx_unlock(&smallalloc_mtx);
	if ((wait & M_ZERO))
		bzero(ret, bytes);
	return (ret);
}

void
uma_small_free(void *mem, int size, u_int8_t flags)
{
	pd_entry_t *pd;
	pt_entry_t *pt;

	if (flags & UMA_SLAB_KMEM)
		kmem_free(kmem_map, (vm_offset_t)mem, size);
	else {
		struct arm_small_page *sp;

		if ((vm_offset_t)mem >= KERNBASE) {
			mtx_lock(&smallalloc_mtx);
			sp = TAILQ_FIRST(&free_pgdesc);
			KASSERT(sp != NULL, ("No more free page descriptor ?"));
			TAILQ_REMOVE(&free_pgdesc, sp, pg_list);
			sp->addr = mem;
			pmap_get_pde_pte(kernel_pmap, (vm_offset_t)mem, &pd,
			    &pt);
			if ((*pd & pte_l1_s_cache_mask) == 
			    pte_l1_s_cache_mode_pt &&
			    pte_l1_s_cache_mode_pt != pte_l1_s_cache_mode)
				TAILQ_INSERT_HEAD(&pages_wt, sp, pg_list);
			else
				TAILQ_INSERT_HEAD(&pages_normal, sp, pg_list);
			mtx_unlock(&smallalloc_mtx);
		} else {
			vm_page_t m;
			vm_paddr_t pa = vtophys((vm_offset_t)mem);

			m = PHYS_TO_VM_PAGE(pa);
			vm_page_lock_queues();
			vm_page_free(m);
			vm_page_unlock_queues();
		}
	}
}

#endif
