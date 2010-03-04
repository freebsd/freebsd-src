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
 * $FreeBSD$
 */
/*-
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/sf_buf.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/unistd.h>

#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

/*
 * On systems without a direct mapped region (e.g. PPC64),
 * we use the same code as the Book E implementation. Since
 * we need to have runtime detection of this, define some machinery
 * for sf_bufs in this case, and ignore it on systems with direct maps.
 */

#ifndef NSFBUFS
#define NSFBUFS         (512 + maxusers * 16)
#endif

static void sf_buf_init(void *arg);
SYSINIT(sock_sf, SI_SUB_MBUF, SI_ORDER_ANY, sf_buf_init, NULL);
 
LIST_HEAD(sf_head, sf_buf);
 
/* A hash table of active sendfile(2) buffers */
static struct sf_head *sf_buf_active;
static u_long sf_buf_hashmask;

#define SF_BUF_HASH(m)  (((m) - vm_page_array) & sf_buf_hashmask)

static TAILQ_HEAD(, sf_buf) sf_buf_freelist;
static u_int sf_buf_alloc_want;

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
cpu_fork(struct thread *td1, struct proc *p2, struct thread *td2, int flags)
{
	struct	proc *p1;
	struct	trapframe *tf;
	struct	callframe *cf;
	struct	pcb *pcb;

	KASSERT(td1 == curthread || td1 == &thread0,
	    ("cpu_fork: p1 not curproc and not proc0"));
	CTR3(KTR_PROC, "cpu_fork: called td1=%08x p2=%08x flags=%x", (u_int)td1, (u_int)p2, flags);

	if ((flags & RFPROC) == 0)
		return;

	p1 = td1->td_proc;

	pcb = (struct pcb *)((td2->td_kstack +
	    td2->td_kstack_pages * PAGE_SIZE - sizeof(struct pcb)) & ~0x2fU);
	td2->td_pcb = pcb;

	/* Copy the pcb */
	bcopy(td1->td_pcb, pcb, sizeof(struct pcb));

	/*
	 * Create a fresh stack for the new process.
	 * Copy the trap frame for the return to user mode as if from a
	 * syscall.  This copies most of the user mode register values.
	 */
	tf = (struct trapframe *)pcb - 1;
	bcopy(td1->td_frame, tf, sizeof(*tf));

	/* Set up trap frame. */
	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 0;
	tf->cr &= ~0x10000000;

	td2->td_frame = tf;

	cf = (struct callframe *)tf - 1;
	memset(cf, 0, sizeof(struct callframe));
	cf->cf_func = (register_t)fork_return;
	cf->cf_arg0 = (register_t)td2;
	cf->cf_arg1 = (register_t)tf;

	pcb->pcb_sp = (register_t)cf;
	pcb->pcb_lr = (register_t)fork_trampoline;
	pcb->pcb_cpu.aim.usr = kernel_pmap->pm_sr[USER_SR];

	/* Setup to release spin count in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	td2->td_md.md_saved_msr = PSL_KERNSET;

	/*
 	 * Now cpu_switch() can schedule the new process.
	 */
}

/*
 * Intercept the return address from a freshly forked process that has NOT
 * been scheduled yet.
 *
 * This is needed to make kernel threads stay in kernel mode.
 */
void
cpu_set_fork_handler(td, func, arg)
	struct thread *td;
	void (*func)(void *);
	void *arg;
{
	struct	callframe *cf;

	CTR4(KTR_PROC, "%s called with td=%08x func=%08x arg=%08x",
	    __func__, (u_int)td, (u_int)func, (u_int)arg);

	cf = (struct callframe *)td->td_pcb->pcb_sp;

	cf->cf_func = (register_t)func;
	cf->cf_arg0 = (register_t)arg;
}

void
cpu_exit(td)
	register struct thread *td;
{
}

/*
 * Reset back to firmware.
 */
void
cpu_reset()
{
	OF_reboot();
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

	/* Don't bother on systems with a direct map */

	if (hw_direct_map)
		return;

        nsfbufs = NSFBUFS;
        TUNABLE_INT_FETCH("kern.ipc.nsfbufs", &nsfbufs);

        sf_buf_active = hashinit(nsfbufs, M_TEMP, &sf_buf_hashmask);
        TAILQ_INIT(&sf_buf_freelist);
        sf_base = kmem_alloc_nofault(kernel_map, nsfbufs * PAGE_SIZE);
        sf_bufs = malloc(nsfbufs * sizeof(struct sf_buf), M_TEMP, M_NOWAIT | M_ZERO);

        for (i = 0; i < nsfbufs; i++) {
                sf_bufs[i].kva = sf_base + i * PAGE_SIZE;
                TAILQ_INSERT_TAIL(&sf_buf_freelist, &sf_bufs[i], free_entry);
        }
        sf_buf_alloc_want = 0;
        mtx_init(&sf_buf_lock, "sf_buf", NULL, MTX_DEF);
}

/*
 * Get an sf_buf from the freelist. Will block if none are available.
 */
struct sf_buf *
sf_buf_alloc(struct vm_page *m, int flags)
{
        struct sf_head *hash_list;
        struct sf_buf *sf;
        int error;

	if (hw_direct_map) {
		/* Shortcut the direct mapped case */

		return ((struct sf_buf *)m);
	}

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
 * Detatch mapped page and release resources back to the system.
 *
 * Remove a reference from the given sf_buf, adding it to the free
 * list when its reference count reaches zero. A freed sf_buf still,
 * however, retains its virtual-to-physical mapping until it is
 * recycled or reactivated by sf_buf_alloc(9).
 */
void
sf_buf_free(struct sf_buf *sf)
{
	if (hw_direct_map)
		return;

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
#if 0 /* XXX: Don't have busdma stuff yet */
	if (busdma_swi_pending != 0)
		busdma_swi();
#endif
}

/*
 * Tell whether this address is in some physical memory region.
 * Currently used by the kernel coredump code in order to avoid
 * dumping the ``ISA memory hole'' which could cause indefinite hangs,
 * or other unpredictable behaviour.
 */


int
is_physical_memory(addr)
	vm_offset_t addr;
{
	/*
	 * stuff other tests for known memory-mapped devices (PCI?)
	 * here
	 */

	return 1;
}

/*
 * Threading functions
 */
void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_clean(struct thread *td)
{
}

void
cpu_thread_alloc(struct thread *td)
{
	struct pcb *pcb;

	pcb = (struct pcb *)((td->td_kstack + td->td_kstack_pages * PAGE_SIZE -
	    sizeof(struct pcb)) & ~0x2fU);
	td->td_pcb = pcb;
	td->td_frame = (struct trapframe *)pcb - 1;
}

void
cpu_thread_free(struct thread *td)
{
}

void
cpu_thread_swapin(struct thread *td)
{
}

void
cpu_thread_swapout(struct thread *td)
{
}

void
cpu_set_syscall_retval(struct thread *td, int error)
{
	struct proc *p;
	struct trapframe *tf;
	int fixup;

	if (error == EJUSTRETURN)
		return;

	p = td->td_proc;
	tf = td->td_frame;

	if (tf->fixreg[0] == SYS___syscall) {
		int code = tf->fixreg[FIRSTARG + 1];
		if (p->p_sysent->sv_mask)
			code &= p->p_sysent->sv_mask;
		fixup = (code != SYS_freebsd6_lseek && code != SYS_lseek) ?
		    1 : 0;
	} else
		fixup = 0;

	switch (error) {
	case 0:
		if (fixup) {
			/*
			 * 64-bit return, 32-bit syscall. Fixup byte order
			 */
			tf->fixreg[FIRSTARG] = 0;
			tf->fixreg[FIRSTARG + 1] = td->td_retval[0];
		} else {
			tf->fixreg[FIRSTARG] = td->td_retval[0];
			tf->fixreg[FIRSTARG + 1] = td->td_retval[1];
		}
		tf->cr &= ~0x10000000;		/* XXX: Magic number */
		break;
	case ERESTART:
		/*
		 * Set user's pc back to redo the system call.
		 */
		tf->srr0 -= 4;
		break;
	default:
		if (p->p_sysent->sv_errsize) {
			error = (error < p->p_sysent->sv_errsize) ?
			    p->p_sysent->sv_errtbl[error] : -1;
		}
		tf->fixreg[FIRSTARG] = error;
		tf->cr |= 0x10000000;		/* XXX: Magic number */
		break;
	}
}

void
cpu_set_upcall(struct thread *td, struct thread *td0)
{
	struct pcb *pcb2;
	struct trapframe *tf;
	struct callframe *cf;

	pcb2 = td->td_pcb;

	/* Copy the upcall pcb */
	bcopy(td0->td_pcb, pcb2, sizeof(*pcb2));

	/* Create a stack for the new thread */
	tf = td->td_frame;
	bcopy(td0->td_frame, tf, sizeof(struct trapframe));
	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 0;
	tf->cr &= ~0x10000000;

	/* Set registers for trampoline to user mode. */
	cf = (struct callframe *)tf - 1;
	memset(cf, 0, sizeof(struct callframe));
	cf->cf_func = (register_t)fork_return;
	cf->cf_arg0 = (register_t)td;
	cf->cf_arg1 = (register_t)tf;

	pcb2->pcb_sp = (register_t)cf;
	pcb2->pcb_lr = (register_t)fork_trampoline;
	pcb2->pcb_cpu.aim.usr = kernel_pmap->pm_sr[USER_SR];

	/* Setup to release spin count in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_msr = PSL_KERNSET;
}

void
cpu_set_upcall_kse(struct thread *td, void (*entry)(void *), void *arg,
	stack_t *stack)
{
	struct trapframe *tf;
	uint32_t sp;

	tf = td->td_frame;
	/* align stack and alloc space for frame ptr and saved LR */
	sp = ((uint32_t)stack->ss_sp + stack->ss_size - sizeof(uint64_t)) &
	    ~0x1f;
	bzero(tf, sizeof(struct trapframe));

	tf->fixreg[1] = (register_t)sp;
	tf->fixreg[3] = (register_t)arg;
	tf->srr0 = (register_t)entry;
	tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
	td->td_pcb->pcb_flags = 0;

	td->td_retval[0] = (register_t)entry;
	td->td_retval[1] = 0;
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{

	td->td_frame->fixreg[2] = (register_t)tls_base + 0x7008;
	return (0);
}
