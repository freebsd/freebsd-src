/*-
 * Copyright (c) Peter Wemm <peter@netplex.com.au>
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
 *
 * $Id: globaldata.h,v 1.4 1998/05/17 23:08:02 tegge Exp $
 */

/*
 * This structure maps out the global data that needs to be kept on a
 * per-cpu basis.  genassym uses this to generate offsets for the assembler
 * code, which also provides external symbols so that C can get at them as
 * though they were really globals.
 *
 * The SMP parts are setup in pmap.c and locore.s for the BSP, and
 * mp_machdep.c sets up the data for the AP's to "see" when they awake.
 * The reason for doing it via a struct is so that an array of pointers
 * to each CPU's data can be set up for things like "check curproc on all
 * other processors"
 */
struct globaldata {
	struct proc	*curproc;
	struct proc	*npxproc;
	struct pcb	*curpcb;
	struct i386tss	common_tss;
	struct timeval	switchtime;
#ifdef VM86
	struct segment_descriptor common_tssd;
	u_int		private_tss;
	u_int		my_tr;		
#endif
#ifdef SMP
	u_int		cpuid;
	u_int		cpu_lockid;
	u_int		other_cpus;
	pd_entry_t	*my_idlePTD;
	u_int		ss_eflags;
	pt_entry_t	*prv_CMAP1;
	pt_entry_t	*prv_CMAP2;
	pt_entry_t	*prv_CMAP3;
	pt_entry_t	*prv_PMAP1;
	int		inside_intr;
#endif
};

#ifdef SMP
/*
 * This is the upper (0xff800000) address space layout that is per-cpu.
 * It is setup in locore.s and pmap.c for the BSP and in mp_machdep.c for
 * each AP.  genassym helps export this to the assembler code.
 */
struct privatespace {
	/* page 0 - data page */
	struct globaldata globaldata;
	char		__filler0[PAGE_SIZE - sizeof(struct globaldata)];

	/* page 1 - page table page */
	pt_entry_t	prvpt[NPTEPG];

	/* page 2 - local apic mapping */
	lapic_t		lapic;
	char		__filler1[PAGE_SIZE - sizeof(lapic_t)];

	/* page 3..2+UPAGES - idle stack (UPAGES pages) */
	char		idlestack[UPAGES * PAGE_SIZE];

	/* page 3+UPAGES..6+UPAGES - CPAGE1,CPAGE2,CPAGE3,PPAGE1 */
	char		CPAGE1[PAGE_SIZE];
	char		CPAGE2[PAGE_SIZE];
	char		CPAGE3[PAGE_SIZE];
	char		PPAGE1[PAGE_SIZE];

	/* page 7+UPAGES..15 - spare, unmapped */
	char		__filler2[(9-UPAGES) * PAGE_SIZE];

	/* page 16-31 - space for IO apics */
	char		ioapics[16 * PAGE_SIZE];

	/* page 32-47 - maybe other cpu's globaldata pages? */
};
#endif
