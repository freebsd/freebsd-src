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

#ifndef CROSS_LIBKVM
#include <machine/atomic.h>
#include <machine/bootinfo.h>
#include <machine/elf.h>
#include <machine/pte.h>
#else
#include "../../sys/ia64/include/atomic.h"
#include "../../sys/ia64/include/bootinfo.h"
#include "../../sys/ia64/include/elf.h"
#include "../../sys/ia64/include/pte.h"
#endif

#include <kvm.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "kvm_private.h"

#define	REGION_BASE(n)		(((uint64_t)(n)) << 61)
#define	REGION_ADDR(x)		((x) & ((1LL<<61)-1LL))

#define	NKPTEPG(ps)		((ps) / sizeof(struct ia64_lpte))
#define	NKPTEDIR(ps)		((ps) >> 3)
#define	KPTE_PTE_INDEX(va,ps)	(((va)/(ps)) % NKPTEPG(ps))
#define	KPTE_DIR0_INDEX(va,ps)	((((va)/(ps)) / NKPTEPG(ps)) / NKPTEDIR(ps))
#define	KPTE_DIR1_INDEX(va,ps)	((((va)/(ps)) / NKPTEPG(ps)) % NKPTEDIR(ps))

#define	PBVM_BASE		0x9ffc000000000000UL
#define	PBVM_PGSZ		(64 * 1024)

typedef size_t (a2p_f)(kvm_t *, uint64_t, off_t *);

struct vmstate {
	void	*mmapbase;
	size_t	mmapsize;
	size_t	pagesize;
	u_long	kptdir;
	u_long	*pbvm_pgtbl;
	u_int	pbvm_pgtblsz;
	a2p_f	*kvatop;
};

/*
 * Map the ELF headers into the process' address space. We do this in two
 * steps: first the ELF header itself and using that information the whole
 * set of headers.
 */
static int
ia64_maphdrs(kvm_t *kd, size_t sz)
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
 * Physical core support.
 */

static size_t
phys_addr2off(kvm_t *kd, uint64_t pa, off_t *ofs, size_t pgsz)
{
	Elf64_Ehdr *e;
	Elf64_Phdr *p;
	int n;

	if (pa != REGION_ADDR(pa))
		goto fail;

	e = (Elf64_Ehdr *)(kd->vmst->mmapbase);
	n = e->e_phnum;
	p = (Elf64_Phdr *)(void *)((uintptr_t)(void *)e + e->e_phoff);
	while (n && (pa < p->p_paddr || pa >= p->p_paddr + p->p_memsz))
		p++, n--;
	if (n == 0)
		goto fail;

	*ofs = (pa - p->p_paddr) + p->p_offset;
	if (pgsz == 0)
		return (p->p_memsz - (pa - p->p_paddr));
	return (pgsz - ((size_t)pa & (pgsz - 1)));

 fail:
	_kvm_err(kd, kd->program, "invalid physical address %#jx",
	    (uintmax_t)pa);
	return (0);
}

static size_t
phys_kvatop(kvm_t *kd, uint64_t va, off_t *ofs)
{
	struct ia64_lpte pte;
	uint64_t pa, pgaddr, pt0addr, pt1addr;
	size_t pgno, pgsz, pt0no, pt1no;

	if (va >= REGION_BASE(6)) {
		/* Regions 6 and 7: direct mapped. */
		pa = REGION_ADDR(va);
		return (phys_addr2off(kd, pa, ofs, 0));
	} else if (va >= REGION_BASE(5)) {
		/* Region 5: Kernel Virtual Memory. */
		va = REGION_ADDR(va);
		pgsz = kd->vmst->pagesize;
		pt0no = KPTE_DIR0_INDEX(va, pgsz);
		pt1no = KPTE_DIR1_INDEX(va, pgsz);
		pgno = KPTE_PTE_INDEX(va, pgsz);
		if (pt0no >= NKPTEDIR(pgsz))
			goto fail;
		pt0addr = kd->vmst->kptdir + (pt0no << 3);
		if (kvm_read(kd, pt0addr, &pt1addr, 8) != 8)
			goto fail;
		if (pt1addr == 0)
			goto fail;
		pt1addr += pt1no << 3;
		if (kvm_read(kd, pt1addr, &pgaddr, 8) != 8)
			goto fail;
		if (pgaddr == 0)
			goto fail;
		pgaddr += pgno * sizeof(pte);
		if (kvm_read(kd, pgaddr, &pte, sizeof(pte)) != sizeof(pte))
			goto fail;
		if (!(pte.pte & PTE_PRESENT))
			goto fail;
		pa = (pte.pte & PTE_PPN_MASK) + (va & (pgsz - 1));
		return (phys_addr2off(kd, pa, ofs, pgsz));
	} else if (va >= PBVM_BASE) {
		/* Region 4: Pre-Boot Virtual Memory (PBVM). */
		va -= PBVM_BASE;
		pgsz = PBVM_PGSZ;
		pt0no = va / pgsz;
		if (pt0no >= (kd->vmst->pbvm_pgtblsz >> 3))
			goto fail;
		pt0addr = kd->vmst->pbvm_pgtbl[pt0no];
		if (!(pt0addr & PTE_PRESENT))
			goto fail;
		pa = (pt0addr & PTE_PPN_MASK) + va % pgsz;
		return (phys_addr2off(kd, pa, ofs, pgsz));
	}

 fail:
	_kvm_err(kd, kd->program, "invalid kernel virtual address %#jx",
	    (uintmax_t)va);
	*ofs = -1;
	return (0);
}

static ssize_t
phys_read(kvm_t *kd, uint64_t pa, void *buf, size_t bufsz)
{
	off_t ofs;
	size_t sz;

	sz = phys_addr2off(kd, pa, &ofs, 0);
	if (sz < bufsz)
		return ((ssize_t)sz);

	if (lseek(kd->pmfd, ofs, 0) == -1)
		return (-1);
	return (read(kd->pmfd, buf, bufsz));
}

/*
 * Virtual core support (aka minidump).
 */

static size_t
virt_addr2off(kvm_t *kd, uint64_t va, off_t *ofs, size_t pgsz)
{
	Elf64_Ehdr *e;
	Elf64_Phdr *p;
	int n;

	if (va < REGION_BASE(4))
		goto fail;

	e = (Elf64_Ehdr *)(kd->vmst->mmapbase);
	n = e->e_phnum;
	p = (Elf64_Phdr *)(void *)((uintptr_t)(void *)e + e->e_phoff); 
	while (n && (va < p->p_vaddr || va >= p->p_vaddr + p->p_memsz))
		p++, n--;
	if (n == 0)
		goto fail;

	*ofs = (va - p->p_vaddr) + p->p_offset;
	if (pgsz == 0)
		return (p->p_memsz - (va - p->p_vaddr));
	return (pgsz - ((size_t)va & (pgsz - 1)));

 fail:
	_kvm_err(kd, kd->program, "invalid virtual address %#jx",
	    (uintmax_t)va);
	return (0);
}

static size_t
virt_kvatop(kvm_t *kd, uint64_t va, off_t *ofs)
{

	return (virt_addr2off(kd, va, ofs, kd->vmst->pagesize));
}

/*
 * KVM architecture support functions.
 */

void
_kvm_freevtop(kvm_t *kd)
{
	struct vmstate *vm = kd->vmst;

	if (vm->pbvm_pgtbl != NULL)
		free(vm->pbvm_pgtbl);
	if (vm->mmapbase != NULL)
		munmap(vm->mmapbase, vm->mmapsize);
	free(vm);
	kd->vmst = NULL;
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct bootinfo bi;
	struct nlist nl[2];
	uint64_t va;
	Elf64_Ehdr *ehdr;
	size_t hdrsz;
	ssize_t sz;

	kd->vmst = (struct vmstate *)_kvm_malloc(kd, sizeof(*kd->vmst));
	if (kd->vmst == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}

#ifndef CROSS_LIBKVM
	kd->vmst->pagesize = getpagesize();
#else
	kd->vmst->pagesize = 8192;
#endif

	if (ia64_maphdrs(kd, sizeof(Elf64_Ehdr)) == -1)
		return (-1);

	ehdr = kd->vmst->mmapbase;
	hdrsz = ehdr->e_phoff + ehdr->e_phentsize * ehdr->e_phnum;
	if (ia64_maphdrs(kd, hdrsz) == -1)
		return (-1);

	kd->vmst->kvatop = (ehdr->e_flags & EF_IA_64_ABSOLUTE) ?
	    phys_kvatop : virt_kvatop;

	/*
	 * Load the PBVM page table. We need this to resolve PBVM addresses.
	 * The PBVM page table is obtained from the bootinfo structure, of
	 * which the address is given to us in e_entry. If e_entry is 0, then
	 * this is assumed to be a pre-PBVM kernel.
	 * Note that the address of the bootinfo structure is either physical
	 * or virtual, depending on whether the core is physical or virtual.
	 */
	if (ehdr->e_entry != 0 && (ehdr->e_flags & EF_IA_64_ABSOLUTE) != 0) {
		sz = phys_read(kd, ehdr->e_entry, &bi, sizeof(bi));
		if (sz != sizeof(bi)) {
			_kvm_err(kd, kd->program,
			    "cannot read bootinfo at physical address %#jx",
			    (uintmax_t)ehdr->e_entry);
			return (-1);
		}
		if (bi.bi_magic != BOOTINFO_MAGIC) {
			_kvm_err(kd, kd->program, "invalid bootinfo");
			return (-1);
		}
		kd->vmst->pbvm_pgtbl = _kvm_malloc(kd, bi.bi_pbvm_pgtblsz);
		if (kd->vmst->pbvm_pgtbl == NULL) {
			_kvm_err(kd, kd->program, "cannot allocate page table");
			return (-1);
		}
		kd->vmst->pbvm_pgtblsz = bi.bi_pbvm_pgtblsz;
		sz = phys_read(kd, bi.bi_pbvm_pgtbl, kd->vmst->pbvm_pgtbl,
		    bi.bi_pbvm_pgtblsz);
		if (sz != bi.bi_pbvm_pgtblsz) {
			_kvm_err(kd, kd->program,
			    "cannot read page table at physical address %#jx",
			    (uintmax_t)bi.bi_pbvm_pgtbl);
			return (-1);
		}
	} else {
		kd->vmst->pbvm_pgtbl = NULL;
		kd->vmst->pbvm_pgtblsz = 0;
	}

	/*
	 * At this point we've got enough information to use kvm_read() for
	 * direct mapped (ie region 6 and region 7) address, such as symbol
	 * addresses/values.
	 */

	nl[0].n_name = "ia64_kptdir";
	nl[1].n_name = 0;

	if (kvm_nlist(kd, nl) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}

	if (kvm_read(kd, (nl[0].n_value), &va, sizeof(va)) != sizeof(va)) {
		_kvm_err(kd, kd->program, "cannot read kptdir");
		return (-1);
	}

	if (va == REGION_BASE(5)) {
		_kvm_err(kd, kd->program, "kptdir is itself virtual");
		return (-1);
	}

	kd->vmst->kptdir = va;
	return (0);
}

int
_kvm_kvatop(kvm_t *kd, u_long va, off_t *ofs)
{
	size_t sz;

	sz = kd->vmst->kvatop(kd, va, ofs);
	return (sz);
}
