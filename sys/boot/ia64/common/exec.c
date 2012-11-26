/*-
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>

#include <sys/param.h>
#include <sys/linker.h>
#include <machine/elf.h>
#include <machine/ia64_cpu.h>
#include <machine/pte.h>

#include <efi.h>
#include <efilib.h>

#include "libia64.h"

static u_int itr_idx = 0;
static u_int dtr_idx = 0;

static vm_offset_t ia64_text_start;
static size_t ia64_text_size;

static vm_offset_t ia64_data_start;
static size_t ia64_data_size;

static int elf64_exec(struct preloaded_file *amp);
static int elf64_obj_exec(struct preloaded_file *amp);

static struct file_format ia64_elf = {
	elf64_loadfile,
	elf64_exec
};
static struct file_format ia64_elf_obj = {
	elf64_obj_loadfile,
	elf64_obj_exec
};

struct file_format *file_formats[] = {
	&ia64_elf,
	&ia64_elf_obj,
	NULL
};

static u_int
sz2shft(vm_offset_t ofs, vm_size_t sz)
{
	vm_size_t s;
	u_int shft;

	shft = 12;	/* Start with 4K */
	s = 1 << shft;
	while (s <= sz) {
		shft++;
		s <<= 1;
	}
	do {
		shft--;
		s >>= 1;
	} while (ofs & (s - 1));

	return (shft);
}

/*
 * Entered with psr.ic and psr.i both zero.
 */
static void
enter_kernel(uint64_t start, struct bootinfo *bi)
{

	__asm __volatile("srlz.i;;");
	__asm __volatile("mov cr.ipsr=%0"
			 :: "r"(IA64_PSR_IC
				| IA64_PSR_DT
				| IA64_PSR_RT
				| IA64_PSR_IT
				| IA64_PSR_BN));
	__asm __volatile("mov cr.iip=%0" :: "r"(start));
	__asm __volatile("mov cr.ifs=r0;;");
	__asm __volatile("mov ar.rsc=0;; flushrs;;");
	__asm __volatile("mov r8=%0" :: "r" (bi));
	__asm __volatile("rfi;;");

	/* NOTREACHED */
}

static u_int
mmu_wire(vm_offset_t va, vm_paddr_t pa, u_int pgshft, u_int acc)
{
	pt_entry_t pte;

	/* Round up to the smallest possible page size. */
	if (pgshft < 12)
		pgshft = 12;
	/* Truncate to the largest possible page size (256MB). */
	if (pgshft > 28)
		pgshft = 28;
	/* Round down to a valid (mappable) page size. */
	if (pgshft > 14 && (pgshft & 1) != 0)
		pgshft--;

	pte = PTE_PRESENT | PTE_MA_WB | PTE_ACCESSED | PTE_DIRTY |
	    PTE_PL_KERN | (acc & PTE_AR_MASK) | (pa & PTE_PPN_MASK);

	__asm __volatile("mov cr.ifa=%0" :: "r"(va));
	__asm __volatile("mov cr.itir=%0" :: "r"(pgshft << 2));
	__asm __volatile("srlz.d;;");

	__asm __volatile("ptr.d %0,%1" :: "r"(va), "r"(pgshft << 2));
	__asm __volatile("srlz.d;;");
	__asm __volatile("itr.d dtr[%0]=%1" :: "r"(dtr_idx), "r"(pte));
	__asm __volatile("srlz.d;;");
	dtr_idx++;

	if (acc == PTE_AR_RWX || acc == PTE_AR_RX) {
		__asm __volatile("ptr.i %0,%1;;" :: "r"(va), "r"(pgshft << 2));
		__asm __volatile("srlz.i;;");
		__asm __volatile("itr.i itr[%0]=%1;;" :: "r"(itr_idx), "r"(pte));
		__asm __volatile("srlz.i;;");
		itr_idx++;
	}

	return (pgshft);
}

static void
mmu_setup_legacy(uint64_t entry)
{

	/*
	 * Region 6 is direct mapped UC and region 7 is direct mapped
	 * WC. The details of this is controlled by the Alt {I,D}TLB
	 * handlers. Here we just make sure that they have the largest
	 * possible page size to minimise TLB usage.
	 */
	ia64_set_rr(IA64_RR_BASE(6), (6 << 8) | (28 << 2));
	ia64_set_rr(IA64_RR_BASE(7), (7 << 8) | (28 << 2));
	__asm __volatile("srlz.i;;");

	mmu_wire(entry, IA64_RR_MASK(entry), 28, PTE_AR_RWX);
}

static void
mmu_setup_paged(struct bootinfo *bi)
{
	void *pa;
	size_t sz;
	u_int shft;

	ia64_set_rr(IA64_RR_BASE(IA64_PBVM_RR),
	    (IA64_PBVM_RR << 8) | (IA64_PBVM_PAGE_SHIFT << 2));
	__asm __volatile("srlz.i;;");

	/* Wire the PBVM page table. */
	mmu_wire(IA64_PBVM_PGTBL, (uintptr_t)ia64_pgtbl,
	    sz2shft(IA64_PBVM_PGTBL, ia64_pgtblsz), PTE_AR_RW);

	/* Wire as much of the text segment as we can. */
	sz = ia64_text_size;	/* XXX */
	pa = ia64_va2pa(ia64_text_start, &ia64_text_size);
	ia64_text_size = sz;	/* XXX */
	shft = sz2shft(ia64_text_start, ia64_text_size);
	shft = mmu_wire(ia64_text_start, (uintptr_t)pa, shft, PTE_AR_RWX);
	ia64_copyin(&shft, (uintptr_t)&bi->bi_text_mapped, 4);

	/* Wire as much of the data segment as well. */
	sz = ia64_data_size;	/* XXX */
	pa = ia64_va2pa(ia64_data_start, &ia64_data_size);
	ia64_data_size = sz;	/* XXX */
	shft = sz2shft(ia64_data_start, ia64_data_size);
	shft = mmu_wire(ia64_data_start, (uintptr_t)pa, shft, PTE_AR_RW);
	ia64_copyin(&shft, (uintptr_t)&bi->bi_data_mapped, 4);

	/* Update the bootinfo with the number of TRs used. */
	ia64_copyin(&itr_idx, (uintptr_t)&bi->bi_itr_used, 4);
	ia64_copyin(&dtr_idx, (uintptr_t)&bi->bi_dtr_used, 4);
}

static int
elf64_exec(struct preloaded_file *fp)
{
	struct bootinfo *bi;
	struct file_metadata *md;
	Elf_Ehdr *hdr;
	int error;

	md = file_findmetadata(fp, MODINFOMD_ELFHDR);
	if (md == NULL)
		return (EINVAL);

	error = ia64_bootinfo(fp, &bi);
	if (error)
		return (error);

	hdr = (Elf_Ehdr *)&(md->md_data);
	printf("Entering %s at 0x%lx...\n", fp->f_name, hdr->e_entry);

	error = ia64_platform_enter(fp->f_name);
	if (error)
		return (error);

	__asm __volatile("rsm psr.ic|psr.i;;");
	__asm __volatile("srlz.i;;");

	if (IS_LEGACY_KERNEL())
		mmu_setup_legacy(hdr->e_entry);
	else
		mmu_setup_paged(bi);

	enter_kernel(hdr->e_entry, bi);
	/* NOTREACHED */
	return (EDOOFUS);
}

static int
elf64_obj_exec(struct preloaded_file *fp)
{

	printf("%s called for preloaded file %p (=%s):\n", __func__, fp,
	    fp->f_name);
	return (ENOSYS);
}

void
ia64_loadseg(Elf_Ehdr *eh, Elf_Phdr *ph, uint64_t delta)
{

	if (eh->e_type != ET_EXEC)
		return;

	if (ph->p_flags & PF_X) {
		ia64_text_start = ph->p_vaddr + delta;
		ia64_text_size = ph->p_memsz;

		ia64_sync_icache(ia64_text_start, ia64_text_size);
	} else {
		ia64_data_start = ph->p_vaddr + delta;
		ia64_data_size = ph->p_memsz;
	}
}

