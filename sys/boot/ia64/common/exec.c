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

#include <ia64/include/vmparam.h>

#include <efi.h>
#include <efilib.h>

#include "libia64.h"

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

static void
mmu_wire(vm_offset_t va, vm_paddr_t pa, vm_size_t sz, u_int acc)
{
	static u_int iidx = 0, didx = 0;
	pt_entry_t pte;
	u_int shft;

	/* Round up to the smallest possible page size. */
	if (sz < 4096)
		sz = 4096;
	/* Determine the exponent (base 2). */
	shft = 0;
	while (sz > 1) {
		shft++;
		sz >>= 1;
	}
	/* Truncate to the largest possible page size (256MB). */
	if (shft > 28)
		shft = 28;
	/* Round down to a valid (mappable) page size. */
	if (shft > 14 && (shft & 1) != 0)
		shft--;

	pte = PTE_PRESENT | PTE_MA_WB | PTE_ACCESSED | PTE_DIRTY |
	    PTE_PL_KERN | (acc & PTE_AR_MASK) | (pa & PTE_PPN_MASK);

	__asm __volatile("mov cr.ifa=%0" :: "r"(va));
	__asm __volatile("mov cr.itir=%0" :: "r"(shft << 2));
	__asm __volatile("srlz.d;;");

	__asm __volatile("ptr.d %0,%1" :: "r"(va), "r"(shft << 2));
	__asm __volatile("srlz.d;;");
	__asm __volatile("itr.d dtr[%0]=%1" :: "r"(didx), "r"(pte));
	__asm __volatile("srlz.d;;");
	didx++;

	if (acc == PTE_AR_RWX) {
		__asm __volatile("ptr.i %0,%1;;" :: "r"(va), "r"(shft << 2));
		__asm __volatile("srlz.i;;");
		__asm __volatile("itr.i itr[%0]=%1;;" :: "r"(iidx), "r"(pte));
		__asm __volatile("srlz.i;;");
		iidx++;
	}
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

	mmu_wire(entry, IA64_RR_MASK(entry), 1UL << 28, PTE_AR_RWX);
}

static void
mmu_setup_paged(vm_offset_t pbvm_top)
{
	vm_size_t sz;

	ia64_set_rr(IA64_RR_BASE(IA64_PBVM_RR),
	    (IA64_PBVM_RR << 8) | (IA64_PBVM_PAGE_SHIFT << 2));
	__asm __volatile("srlz.i;;");

	/* Wire the PBVM page table. */
	mmu_wire(IA64_PBVM_PGTBL, (uintptr_t)ia64_pgtbl, ia64_pgtblsz,
	    PTE_AR_RW);

	/* Wire as much of the PBVM we can. This must be a power of 2. */
	sz = pbvm_top - IA64_PBVM_BASE;
	sz = (sz + IA64_PBVM_PAGE_MASK) & ~IA64_PBVM_PAGE_MASK;
	while (sz & (sz - 1))
		sz -= IA64_PBVM_PAGE_SIZE;
	mmu_wire(IA64_PBVM_BASE, ia64_pgtbl[0], sz, PTE_AR_RWX);
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
		mmu_setup_paged((uintptr_t)(bi + 1));

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
