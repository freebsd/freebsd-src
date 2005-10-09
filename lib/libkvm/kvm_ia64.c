/* $FreeBSD$ */
/*	$NetBSD: kvm_alpha.c,v 1.7.2.1 1997/11/02 20:34:26 mellon Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
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

#include <sys/types.h>
#include <sys/elf64.h>
#include <sys/mman.h>

#include <machine/pte.h>

#include <kvm.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include "kvm_private.h"

#define	REGION_BASE(n)		(((uint64_t)(n)) << 61)
#define	REGION_ADDR(x)		((x) & ((1LL<<61)-1LL))

#define	NKPTEPG(ps)		((ps) / sizeof(struct ia64_lpte))
#define	KPTE_PTE_INDEX(va,ps)	(((va)/(ps)) % NKPTEPG(ps))
#define	KPTE_DIR_INDEX(va,ps)	(((va)/(ps)) / NKPTEPG(ps))

struct vmstate {
	void	*mmapbase;
	size_t	mmapsize;
	size_t	pagesize;
	u_long	kptdir;
};

/*
 * Map the ELF headers into the process' address space. We do this in two
 * steps: first the ELF header itself and using that information the whole
 * set of headers.
 */
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
	Elf64_Ehdr *e = kd->vmst->mmapbase;
	Elf64_Phdr *p = (Elf64_Phdr*)((char*)e + e->e_phoff);
	int n = e->e_phnum;

	if (pa != REGION_ADDR(pa)) {
		_kvm_err(kd, kd->program, "internal error");
		return (0);
	}

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
	struct vmstate *vm = kd->vmst;

	if (vm->mmapbase != NULL)
		munmap(vm->mmapbase, vm->mmapsize);
	free(vm);
	kd->vmst = NULL;
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct nlist nlist[2];
	uint64_t va;
	Elf64_Ehdr *ehdr;
	size_t hdrsz;

	kd->vmst = (struct vmstate *)_kvm_malloc(kd, sizeof(*kd->vmst));
	if (kd->vmst == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}

	kd->vmst->pagesize = getpagesize();

	if (_kvm_maphdrs(kd, sizeof(Elf64_Ehdr)) == -1)
		return (-1);

	ehdr = kd->vmst->mmapbase;
	hdrsz = ehdr->e_phoff + ehdr->e_phentsize * ehdr->e_phnum;
	if (_kvm_maphdrs(kd, hdrsz) == -1)
		return (-1);

	/*
	 * At this point we've got enough information to use kvm_read() for
	 * direct mapped (ie region 6 and region 7) address, such as symbol
	 * addresses/values.
	 */

	nlist[0].n_name = "ia64_kptdir";
	nlist[1].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}

	if (kvm_read(kd, (nlist[0].n_value), &va, sizeof(va)) != sizeof(va)) {
		_kvm_err(kd, kd->program, "cannot read kptdir");
		return (-1);
	}

	if (va < REGION_BASE(6)) {
		_kvm_err(kd, kd->program, "kptdir is itself virtual");
		return (-1);
	}

	kd->vmst->kptdir = va;
	return (0);
}

int
_kvm_kvatop(kvm_t *kd, u_long va, off_t *pa)
{
	struct ia64_lpte pte;
	uint64_t pgaddr, ptaddr;
	size_t pgno, pgsz, ptno;

	if (va >= REGION_BASE(6)) {
		/* Regions 6 and 7: direct mapped. */
		return (_kvm_pa2off(kd, REGION_ADDR(va), pa, 0));
	} else if (va >= REGION_BASE(5)) {
		/* Region 5: virtual. */
		va = REGION_ADDR(va);
		pgsz = kd->vmst->pagesize;
		ptno = KPTE_DIR_INDEX(va, pgsz);
		pgno = KPTE_PTE_INDEX(va, pgsz);
		if (ptno >= (pgsz >> 3))
			goto fail;
		ptaddr = kd->vmst->kptdir + (ptno << 3);
		if (kvm_read(kd, ptaddr, &pgaddr, 8) != 8)
			goto fail;
		if (pgaddr == 0)
			goto fail;
		pgaddr += (pgno * sizeof(pte));
		if (kvm_read(kd, pgaddr, &pte, sizeof(pte)) != sizeof(pte))
			goto fail;
		if (!(pte.pte & PTE_PRESENT))
			goto fail;
		va = (pte.pte & PTE_PPN_MASK) + (va & (pgsz - 1));
		return (_kvm_pa2off(kd, va, pa, pgsz));
	}

 fail:
	_kvm_err(kd, kd->program, "invalid kernel virtual address");
	*pa = ~0UL;
	return (0);
}
