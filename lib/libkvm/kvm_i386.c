/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_hp300.c	8.1 (Berkeley) 6/4/93";
#else
static const char rcsid[] =
 "$FreeBSD: src/lib/libkvm/kvm_i386.c,v 1.11 1999/12/27 07:14:57 peter Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * i386 machine dependent routines for kvm.  Hopefully, the forthcoming
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <limits.h>

#include "kvm_private.h"

#ifndef btop
#define	btop(x)		(i386_btop(x))
#define	ptob(x)		(i386_ptob(x))
#endif

struct vmstate {
	pd_entry_t	*PTD;
};

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0) {
		if (kd->vmst->PTD) {
			free(kd->vmst->PTD);
		}
		free(kd->vmst);
	}
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct vmstate *vm;
	struct nlist nlist[2];
	u_long pa;
	pd_entry_t	*PTD;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == 0) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}
	kd->vmst = vm;
	vm->PTD = 0;

	nlist[0].n_name = "_IdlePTD";
	nlist[1].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}
	if (kvm_read(kd, (nlist[0].n_value - KERNBASE), &pa, sizeof(pa)) != sizeof(pa)) {
		_kvm_err(kd, kd->program, "cannot read IdlePTD");
		return (-1);
	}
	PTD = _kvm_malloc(kd, PAGE_SIZE);
	if (kvm_read(kd, pa, PTD, PAGE_SIZE) != PAGE_SIZE) {
		_kvm_err(kd, kd->program, "cannot read PTD");
		return (-1);
	}
	vm->PTD = PTD;
	return (0);
}

static int
_kvm_vatop(kvm_t *kd, u_long va, u_long *pa)
{
	struct vmstate *vm;
	u_long offset;
	u_long pte_pa;
	pd_entry_t pde;
	pt_entry_t pte;
	u_long pdeindex;
	u_long pteindex;
	int i;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return((off_t)0);
	}

	vm = kd->vmst;
	offset = va & (PAGE_SIZE - 1);

	/*
	 * If we are initializing (kernel page table descriptor pointer
	 * not yet set) then return pa == va to avoid infinite recursion.
	 */
	if (vm->PTD == 0) {
		*pa = va;
		return (PAGE_SIZE - offset);
	}

	pdeindex = va >> PDRSHIFT;
	pde = vm->PTD[pdeindex];
	if (((u_long)pde & PG_V) == 0)
		goto invalid;

	if ((u_long)pde & PG_PS) {
	      /*
	       * No second-level page table; ptd describes one 4MB page.
	       * (We assume that the kernel wouldn't set PG_PS without enabling
	       * it cr0, and that the kernel doesn't support 36-bit physical
	       * addresses).
	       */
#define	PAGE4M_MASK	(NBPDR - 1)
#define	PG_FRAME4M	(~PAGE4M_MASK)
		*pa = ((u_long)pde & PG_FRAME4M) + (va & PAGE4M_MASK);
		return (NBPDR - (va & PAGE4M_MASK));
	}

	pteindex = (va >> PAGE_SHIFT) & (NPTEPG-1);
	pte_pa = ((u_long)pde & PG_FRAME) + (pteindex * sizeof(pt_entry_t));

	/* XXX This has to be a physical address read, kvm_read is virtual */
	if (lseek(kd->pmfd, pte_pa, 0) == -1) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: lseek");
		goto invalid;
	}
	if (read(kd->pmfd, &pte, sizeof pte) != sizeof pte) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: read");
		goto invalid;
	}
	if (((u_long)pte & PG_V) == 0)
		goto invalid;

	*pa = ((u_long)pte & PG_FRAME) + offset;
	return (PAGE_SIZE - offset);

invalid:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}

int
_kvm_kvatop(kvm_t *kd, u_long va, u_long *pa)
{
	return (_kvm_vatop(kd, va, pa));
}
