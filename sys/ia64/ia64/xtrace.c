/*-
 * Copyright (c) 2014 Marcel Moolenaar
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_ddb.h"
#include "opt_xtrace.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <machine/md_var.h>
#include <machine/pte.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#define	XTRACE_LOG2SZ	14	/* 16KB trace buffers */

struct ia64_xtrace_record {
	uint64_t	ivt;
	uint64_t	itc;
	uint64_t	iip;
	uint64_t	ifa;
	uint64_t	isr;
	uint64_t	ipsr;
	uint64_t	itir;
	uint64_t	iipa;

	uint64_t	ifs;
	uint64_t	iim;
	uint64_t	iha;
	uint64_t	unat;
	uint64_t	rsc;
	uint64_t	bsp;
	uint64_t	tp;
	uint64_t	sp;
};

extern uint32_t ia64_xtrace_enabled;
extern uint64_t ia64_xtrace_mask;

static uint64_t ia64_xtrace_base;

static void
ia64_xtrace_init_common(vm_paddr_t pa)
{
	uint64_t psr;
	pt_entry_t pte;

	pte = PTE_PRESENT | PTE_MA_WB | PTE_ACCESSED | PTE_DIRTY |
	    PTE_PL_KERN | PTE_AR_RW;
	pte |= pa & PTE_PPN_MASK;

	__asm __volatile("ptr.d %0,%1" :: "r"(ia64_xtrace_base),
	    "r"(XTRACE_LOG2SZ << 2));

	__asm __volatile("mov   %0=psr" : "=r"(psr));
	__asm __volatile("rsm   psr.ic|psr.i");
	ia64_srlz_i();

	ia64_set_ifa(ia64_xtrace_base);
	ia64_set_itir(XTRACE_LOG2SZ << 2);
	ia64_srlz_d();
	__asm __volatile("itr.d dtr[%0]=%1" :: "r"(6), "r"(pte));

	__asm __volatile("mov   psr.l=%0" :: "r" (psr));
	ia64_srlz_i();

	pcpup->pc_md.xtrace_tail = ia64_xtrace_base;
	ia64_set_k3(ia64_xtrace_base);
}

void *
ia64_xtrace_alloc(void)
{
	uintptr_t buf;
	size_t sz;

	sz = 1UL << XTRACE_LOG2SZ;
	buf = kmem_alloc_contig(kernel_arena, sz, M_WAITOK | M_ZERO,
	    0UL, ~0UL, sz, 0, VM_MEMATTR_DEFAULT);
	return ((void *)buf);
}

void
ia64_xtrace_init_ap(void *buf)
{
	vm_paddr_t pa;

	if (buf == NULL) {
		ia64_set_k3(0);
		return;
	}
	pcpup->pc_md.xtrace_buffer = buf;
	pa = ia64_tpa((uintptr_t)buf);
	ia64_xtrace_init_common(pa);
}

void
ia64_xtrace_init_bsp(void)
{
	void *buf;
	vm_paddr_t pa;
	size_t sz;

	sz = 1UL << XTRACE_LOG2SZ;
	ia64_xtrace_base = VM_MIN_KERNEL_ADDRESS + (sz << 1);
	ia64_xtrace_mask = ~sz;

	buf = ia64_physmem_alloc(sz, sz);
	if (buf == NULL) {
		ia64_set_k3(0);
		return;
	}
	pcpup->pc_md.xtrace_buffer = buf;
	pa = IA64_RR_MASK((uintptr_t)buf);
	ia64_xtrace_init_common(pa);
}

static void
ia64_xtrace_init(void *dummy __unused)
{

	TUNABLE_INT_FETCH("machdep.xtrace.enabled", &ia64_xtrace_enabled);
}
SYSINIT(xtrace, SI_SUB_CPU, SI_ORDER_ANY, ia64_xtrace_init, NULL);

void
ia64_xtrace_save(void)
{
	struct ia64_xtrace_record *rec;
	uint64_t head, tail;

	critical_enter();
	head = ia64_get_k3();
	tail = PCPU_GET(md.xtrace_tail);
	if (head == 0 || tail == 0) {
		critical_exit();
		return;
	}
	while (head != tail) {
		rec = (void *)(uintptr_t)tail;
		CTR6(KTR_TRAP, "XTRACE: itc=%lu, ticks=%d: "
		    "IVT=%#lx, IIP=%#lx, IFA=%#lx, ISR=%#lx",
		    rec->itc, ticks,
		    rec->ivt, rec->iip, rec->ifa, rec->isr);
		tail += sizeof(*rec);
		tail &= ia64_xtrace_mask;
	}
	PCPU_SET(md.xtrace_tail, tail);
	critical_exit();
}

void
ia64_xtrace_stop(void)
{
	ia64_xtrace_enabled = 0;
}

#if 0
#ifdef DDB

#include <ddb/ddb.h>

DB_SHOW_COMMAND(xtrace, db_xtrace)
{
        struct ia64_xtrace_record *p, *r;

        p = (ia64_xtptr == 0) ? ia64_xtptr1 : ia64_xtptr;
        if (p == 0) {
                db_printf("Exception trace buffer not allocated\n");
                return;
        }

        r = (p->ivt == 0) ? ia64_xtbase : p;
        if (r->ivt == 0) {
                db_printf("No exception trace records written\n");
                return;
        }

        db_printf("IVT\t\t ITC\t\t  IIP\t\t   IFA\n");
        do {
                db_printf("%016lx %016lx %016lx %016lx\n",
                    r->ivt, r->itc, r->iip, r->ifa);
                r++;
                if (r == ia64_xtlim)
                        r = ia64_xtbase;
        } while (r != p);
}

#endif /* DDB */
#endif
