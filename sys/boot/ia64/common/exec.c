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

#include <ia64/include/bootinfo.h>
#include <ia64/include/vmparam.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"

#define _KERNEL

static int	elf64_exec(struct preloaded_file *amp);

struct file_format ia64_elf = { elf64_loadfile, elf64_exec };

/*
 * Entered with psr.ic and psr.i both zero.
 */
void
enter_kernel(uint64_t start, uint64_t bi)
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

static int
elf64_exec(struct preloaded_file *fp)
{
	struct file_metadata	*md;
	Elf_Ehdr		*hdr;
	pt_entry_t		pte;
	uint64_t		bi_addr;

	md = file_findmetadata(fp, MODINFOMD_ELFHDR);
	if (md == NULL)
		return (EINVAL);
	hdr = (Elf_Ehdr *)&(md->md_data);

	bi_load(fp, &bi_addr);

	printf("Entering %s at 0x%lx...\n", fp->f_name, hdr->e_entry);

	ldr_enter(fp->f_name);

	__asm __volatile("rsm psr.ic|psr.i;;");
	__asm __volatile("srlz.i;;");

	/*
	 * Region 6 is direct mapped UC and region 7 is direct mapped
	 * WC. The details of this is controlled by the Alt {I,D}TLB
	 * handlers. Here we just make sure that they have the largest 
	 * possible page size to minimise TLB usage.
	 */
	ia64_set_rr(IA64_RR_BASE(6), (6 << 8) | (28 << 2));
	ia64_set_rr(IA64_RR_BASE(7), (7 << 8) | (28 << 2));

	pte = PTE_PRESENT | PTE_MA_WB | PTE_ACCESSED | PTE_DIRTY |
	    PTE_PL_KERN | PTE_AR_RWX | PTE_ED;

	__asm __volatile("mov cr.ifa=%0" :: "r"(IA64_RR_BASE(7)));
	__asm __volatile("mov cr.itir=%0" :: "r"(28 << 2));
	__asm __volatile("ptr.i %0,%1" :: "r"(IA64_RR_BASE(7)), "r"(28<<2));
	__asm __volatile("ptr.d %0,%1" :: "r"(IA64_RR_BASE(7)), "r"(28<<2));
	__asm __volatile("srlz.i;;");
	__asm __volatile("itr.i itr[%0]=%1;;" :: "r"(0), "r"(pte));
	__asm __volatile("srlz.i;;");
	__asm __volatile("itr.d dtr[%0]=%1;;" :: "r"(0), "r"(pte));
	__asm __volatile("srlz.i;;");

	enter_kernel(hdr->e_entry, bi_addr);

	/* NOTREACHED */
	return (0);
}
