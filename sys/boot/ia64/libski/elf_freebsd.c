/* $NetBSD: loadfile.c,v 1.10 1998/06/25 06:45:46 ross Exp $ */

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>

#include <sys/param.h>
#include <sys/linker.h>
#include <machine/elf.h>
#include <machine/bootinfo.h>
#include <machine/ia64_cpu.h>
#include <machine/vmparam.h>

#include "bootstrap.h"
#include "libski.h"

#define _KERNEL

static int	elf64_exec(struct preloaded_file *amp);

struct file_format ia64_elf = { elf64_loadfile, elf64_exec };

#define PTE_MA_WB	0
#define PTE_MA_UC	4
#define PTE_MA_UCE	5
#define PTE_MA_WC	6
#define PTE_MA_NATPAGE	7

#define PTE_PL_KERN	0
#define PTE_PL_USER	3

#define PTE_AR_R	0
#define PTE_AR_RX	1
#define PTE_AR_RW	2
#define PTE_AR_RWX	3
#define PTE_AR_R_RW	4
#define PTE_AR_RX_RWX	5
#define PTE_AR_RWX_RW	6
#define PTE_AR_X_RX	7

/*
 * A short-format VHPT entry. Also matches the TLB insertion format.
 */
struct ia64_pte {
	u_int64_t	pte_p	:1;	/* bits 0..0 */
	u_int64_t	pte_rv1	:1;	/* bits 1..1 */
	u_int64_t	pte_ma	:3;	/* bits 2..4 */
	u_int64_t	pte_a	:1;	/* bits 5..5 */
	u_int64_t	pte_d	:1;	/* bits 6..6 */
	u_int64_t	pte_pl	:2;	/* bits 7..8 */
	u_int64_t	pte_ar	:3;	/* bits 9..11 */
	u_int64_t	pte_ppn	:38;	/* bits 12..49 */
	u_int64_t	pte_rv2	:2;	/* bits 50..51 */
	u_int64_t	pte_ed	:1;	/* bits 52..52 */
	u_int64_t	pte_ig	:11;	/* bits 53..63 */
};

static struct bootinfo bootinfo;

void
enter_kernel(const char* filename, u_int64_t start, struct bootinfo *bi)
{
	printf("Entering %s at 0x%lx...\n", filename, start);

	while (*filename == '/')
		filename++;
	ssc(0, (u_int64_t) filename, 0, 0, SSC_LOAD_SYMBOLS);

	__asm __volatile("mov cr.ipsr=%0"
			 :: "r"(IA64_PSR_IC
				| IA64_PSR_DT
				| IA64_PSR_RT
				| IA64_PSR_IT
				| IA64_PSR_BN));
	__asm __volatile("mov cr.iip=%0" :: "r"(start));
	__asm __volatile("mov cr.ifs=r0;;");
	__asm __volatile("mov r8=%0" :: "r" (bi));
	__asm __volatile("rfi;;");
}

static int
elf64_exec(struct preloaded_file *fp)
{
	struct file_metadata	*md;
	Elf_Ehdr		*hdr;
	struct ia64_pte		pte;
	struct bootinfo		*bi;

	if ((md = file_findmetadata(fp, MODINFOMD_ELFHDR)) == NULL)
		return(EFTYPE);			/* XXX actually EFUCKUP */
	hdr = (Elf_Ehdr *)&(md->md_data);

	/*
	 * Ugly hack, similar to linux. Dump the bootinfo into a
	 * special page reserved in the link map.
	 */
	bi = &bootinfo;
	bzero(bi, sizeof(struct bootinfo));
	bi_load(bi, fp);

	/*
	 * Region 6 is direct mapped UC and region 7 is direct mapped
	 * WC. The details of this is controlled by the Alt {I,D}TLB
	 * handlers. Here we just make sure that they have the largest 
	 * possible page size to minimise TLB usage.
	 */
	ia64_set_rr(IA64_RR_BASE(6), (6 << 8) | (28 << 2));
	ia64_set_rr(IA64_RR_BASE(7), (7 << 8) | (28 << 2));

	bzero(&pte, sizeof(pte));
	pte.pte_p = 1;
	pte.pte_ma = PTE_MA_WB;
	pte.pte_a = 1;
	pte.pte_d = 1;
	pte.pte_pl = PTE_PL_KERN;
	pte.pte_ar = PTE_AR_RWX;
	pte.pte_ppn = 0;

	__asm __volatile("mov cr.ifa=%0" :: "r"(IA64_RR_BASE(7)));
	__asm __volatile("mov cr.itir=%0" :: "r"(28 << 2));
	__asm __volatile("srlz.i;;");
	__asm __volatile("itr.i itr[%0]=%1;;"
			 :: "r"(0), "r"(*(u_int64_t*)&pte));
	__asm __volatile("srlz.i;;");
	__asm __volatile("itr.d dtr[%0]=%1;;"
			 :: "r"(0), "r"(*(u_int64_t*)&pte));
	__asm __volatile("srlz.i;;");

	enter_kernel(fp->f_name, hdr->e_entry, bi);
}
