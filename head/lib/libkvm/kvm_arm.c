/*-
 * Copyright (c) 2005 Olivier Houchard
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
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ARM machine dependent routines for kvm.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/elf32.h>
#include <sys/mman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/pmap.h>

#include <db.h>
#include <limits.h>
#include <kvm.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kvm_private.h"

/* minidump must be the first item! */
struct vmstate {
	int minidump;		/* 1 = minidump mode */
	pd_entry_t *l1pt;
	void *mmapbase;
	size_t mmapsize;
};

static int
_kvm_maphdrs(kvm_t *kd, size_t sz)
{
	struct vmstate *vm = kd->vmst;

	/* munmap() previous mmap(). */
	if (vm->mmapbase != NULL) {
		munmap(vm->mmapbase, vm->mmapsize);
		vm->mmapbase = NULL;
	}

	vm->mmapsize = sz;
	vm->mmapbase = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, kd->pmfd, 0);
	if (vm->mmapbase == MAP_FAILED) {
		_kvm_err(kd, kd->program, "cannot mmap corefile");
		return (-1);
	}

	return (0);
}

/*
 * Translate a physical memory address to a file-offset in the crash-dump.
 */
static size_t
_kvm_pa2off(kvm_t *kd, uint64_t pa, off_t *ofs, size_t pgsz)
{
	Elf32_Ehdr *e = kd->vmst->mmapbase;
	Elf32_Phdr *p = (Elf32_Phdr*)((char*)e + e->e_phoff);
	int n = e->e_phnum;

	while (n && (pa < p->p_paddr || pa >= p->p_paddr + p->p_memsz))
		p++, n--;
	if (n == 0)
		return (0);

	*ofs = (pa - p->p_paddr) + p->p_offset;
	if (pgsz == 0)
		return (p->p_memsz - (pa - p->p_paddr));
	return (pgsz - ((size_t)pa & (pgsz - 1)));
}

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0) {
		if (kd->vmst->minidump)
			return (_kvm_minidump_freevtop(kd));
		if (kd->vmst->mmapbase != NULL)
			munmap(kd->vmst->mmapbase, kd->vmst->mmapsize);
		free(kd->vmst);
		kd->vmst = NULL;
	}
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct vmstate *vm;
	struct nlist nl[2];
	u_long kernbase, physaddr, pa;
	pd_entry_t *l1pt;
	Elf32_Ehdr *ehdr;
	size_t hdrsz;
	char minihdr[8];

	if (!kd->rawdump) {
		if (pread(kd->pmfd, &minihdr, 8, 0) == 8) {
			if (memcmp(&minihdr, "minidump", 8) == 0)
				return (_kvm_minidump_initvtop(kd));
		} else {
			_kvm_err(kd, kd->program, "cannot read header");
			return (-1);
		}
	}

	vm = _kvm_malloc(kd, sizeof(*vm));
	if (vm == 0) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}
	kd->vmst = vm;
	vm->l1pt = NULL;
	if (_kvm_maphdrs(kd, sizeof(Elf32_Ehdr)) == -1)
		return (-1);
	ehdr = kd->vmst->mmapbase;
	hdrsz = ehdr->e_phoff + ehdr->e_phentsize * ehdr->e_phnum;
	if (_kvm_maphdrs(kd, hdrsz) == -1)
		return (-1);
	nl[0].n_name = "kernbase";
	nl[1].n_name = NULL;
	if (kvm_nlist(kd, nl) != 0)
		kernbase = KERNBASE;
	else
		kernbase = nl[0].n_value;

	nl[0].n_name = "physaddr";
	if (kvm_nlist(kd, nl) != 0) {
		_kvm_err(kd, kd->program, "couldn't get phys addr");
		return (-1);
	}
	physaddr = nl[0].n_value;
	nl[0].n_name = "kernel_l1pa";
	if (kvm_nlist(kd, nl) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}
	if (kvm_read(kd, (nl[0].n_value - kernbase + physaddr), &pa,
	    sizeof(pa)) != sizeof(pa)) {
		_kvm_err(kd, kd->program, "cannot read kernel_l1pa");
		return (-1);
	}
	l1pt = _kvm_malloc(kd, L1_TABLE_SIZE);
	if (kvm_read(kd, pa, l1pt, L1_TABLE_SIZE) != L1_TABLE_SIZE) {
		_kvm_err(kd, kd->program, "cannot read l1pt");
		free(l1pt);
		return (-1);
	}
	vm->l1pt = l1pt;
	return 0;
}

/* from arm/pmap.c */
#define	L1_IDX(va)		(((vm_offset_t)(va)) >> L1_S_SHIFT)
/* from arm/pmap.h */
#define	L1_TYPE_INV	0x00		/* Invalid (fault) */
#define	L1_TYPE_C	0x01		/* Coarse L2 */
#define	L1_TYPE_S	0x02		/* Section */
#define	L1_TYPE_F	0x03		/* Fine L2 */
#define	L1_TYPE_MASK	0x03		/* mask of type bits */

#define	l1pte_section_p(pde)	(((pde) & L1_TYPE_MASK) == L1_TYPE_S)
#define	l1pte_valid(pde)	((pde) != 0)
#define	l2pte_valid(pte)	((pte) != 0)
#define l2pte_index(v)		(((v) & L2_ADDR_BITS) >> L2_S_SHIFT)


int
_kvm_kvatop(kvm_t *kd, u_long va, off_t *pa)
{
	struct vmstate *vm = kd->vmst;
	pd_entry_t pd;
	pt_entry_t pte;
	u_long pte_pa;

	if (kd->vmst->minidump)
		return (_kvm_minidump_kvatop(kd, va, pa));

	if (vm->l1pt == NULL)
		return (_kvm_pa2off(kd, va, pa, PAGE_SIZE));
	pd = vm->l1pt[L1_IDX(va)];
	if (!l1pte_valid(pd))
		goto invalid;
	if (l1pte_section_p(pd)) {
		/* 1MB section mapping. */
		*pa = ((u_long)pd & L1_S_ADDR_MASK) + (va & L1_S_OFFSET);
		return  (_kvm_pa2off(kd, *pa, pa, L1_S_SIZE));
	}
	pte_pa = (pd & L1_ADDR_MASK) + l2pte_index(va) * sizeof(pte);
	_kvm_pa2off(kd, pte_pa, (off_t *)&pte_pa, L1_S_SIZE);
	if (lseek(kd->pmfd, pte_pa, 0) == -1) {
		_kvm_syserr(kd, kd->program, "_kvm_kvatop: lseek");
		goto invalid;
	}
	if (read(kd->pmfd, &pte, sizeof(pte)) != sizeof (pte)) {
		_kvm_syserr(kd, kd->program, "_kvm_kvatop: read");
		goto invalid;
	}
	if (!l2pte_valid(pte)) {
		goto invalid;
	}
	if ((pte & L2_TYPE_MASK) == L2_TYPE_L) {
		*pa = (pte & L2_L_FRAME) | (va & L2_L_OFFSET);
		return (_kvm_pa2off(kd, *pa, pa, L2_L_SIZE));
	}
	*pa = (pte & L2_S_FRAME) | (va & L2_S_OFFSET);
	return (_kvm_pa2off(kd, *pa, pa, PAGE_SIZE));
invalid:
	_kvm_err(kd, 0, "Invalid address (%lx)", va);
	return 0;
}

/*
 * Machine-dependent initialization for ALL open kvm descriptors,
 * not just those for a kernel crash dump.  Some architectures
 * have to deal with these NOT being constants!  (i.e. m68k)
 */
#ifdef FBSD_NOT_YET
int
_kvm_mdopen(kvm_t *kd)
{

	kd->usrstack = USRSTACK;
	kd->min_uva = VM_MIN_ADDRESS;
	kd->max_uva = VM_MAXUSER_ADDRESS;

	return (0);
}
#endif
