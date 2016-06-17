#ifndef __PPC64_ELF_H
#define __PPC64_ELF_H

/*
 * ELF register definitions..
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <asm/types.h>
#include <asm/ptrace.h>
#include <asm/cputable.h>

#define ELF_NGREG	48	/* includes nip, msr, lr, etc. */
#define ELF_NFPREG	33	/* includes fpscr */
#define ELF_NVRREG	33	/* includes vscr */

typedef unsigned long elf_greg_t64;
typedef elf_greg_t64 elf_gregset_t64[ELF_NGREG];

typedef unsigned int elf_greg_t32;
typedef elf_greg_t32 elf_gregset_t32[ELF_NGREG];

/*
 * These are used to set parameters in the core dumps.
 */
#ifndef ELF_ARCH
# define ELF_ARCH	EM_PPC64
# define ELF_CLASS	ELFCLASS64
# define ELF_DATA	ELFDATA2MSB
  typedef elf_greg_t64 elf_greg_t;
  typedef elf_gregset_t64 elf_gregset_t;
# define elf_addr_t unsigned long
# define elf_caddr_t char *
#else
  /* Assumption: ELF_ARCH == EM_PPC and ELF_CLASS == ELFCLASS32 */
  typedef elf_greg_t32 elf_greg_t;
  typedef elf_gregset_t32 elf_gregset_t;
# define elf_addr_t u32
# define elf_caddr_t u32
#endif

/* Floating point registers */
typedef double elf_fpreg_t;
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

/* Altivec registers */
typedef __vector128 elf_vrreg_t;
typedef elf_vrreg_t elf_vrregset_t[ELF_NVRREG];


#ifdef __KERNEL__

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ((x)->e_machine == ELF_ARCH)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	4096

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         (0x08000000)

/* Common routine for both 32-bit and 64-bit processes */
#define ELF_CORE_COPY_REGS(gregs, regs) elf_core_copy_regs(gregs, regs);
static inline void
elf_core_copy_regs(elf_gregset_t dstRegs, struct pt_regs* srcRegs)
{
	int i;

	int numGPRS = ((sizeof(struct pt_regs)/sizeof(elf_greg_t64)) < ELF_NGREG) ? (sizeof(struct pt_regs)/sizeof(elf_greg_t64)) : ELF_NGREG;

	for (i=0; i < numGPRS; i++)
		dstRegs[i] = (elf_greg_t)((elf_greg_t64 *)srcRegs)[i];
}

/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  This could be done in userspace,
   but it's not easy, and we've already done it here.  */

#define ELF_HWCAP	(cur_cpu_spec->cpu_user_features)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.

   For the moment, we have only optimizations for the Intel generations,
   but that could change... */

#define ELF_PLATFORM	(NULL)


#define ELF_PLAT_INIT(_r, interp_load_addr)	do { \
	memset(_r->gpr, 0, sizeof(_r->gpr)); \
	_r->ctr = _r->link = _r->xer = _r->ccr = 0; \
	_r->gpr[2] = interp_load_addr; \
} while (0)



#define SET_PERSONALITY(ex, ibcs2)				\
do {	if ((ex).e_ident[EI_CLASS] == ELFCLASS32)		\
		current->thread.flags |= PPC_FLAG_32BIT;	\
	else							\
		current->thread.flags &= ~PPC_FLAG_32BIT;	\
	if (ibcs2)						\
		set_personality(PER_SVR4);			\
	else if (current->personality != PER_LINUX32)		\
		set_personality(PER_LINUX);			\
} while (0)

/*
 * We need to put in some extra aux table entries to tell glibc what
 * the cache block size is, so it can use the dcbz instruction safely.
 */
#define AT_DCACHEBSIZE		19
#define AT_ICACHEBSIZE		20
#define AT_UCACHEBSIZE		21
/* A special ignored type value for PPC, for glibc compatibility.  */
#define AT_IGNOREPPC		22

extern int dcache_bsize;
extern int icache_bsize;
extern int ucache_bsize;

/*
 * The requirements here are:
 * - keep the final alignment of sp (sp & 0xf)
 * - make sure the 32-bit value at the first 16 byte aligned position of
 *   AUXV is greater than 16 for glibc compatibility.
 *   AT_IGNOREPPC is used for that.
 * - for compatibility with glibc ARCH_DLINFO must always be defined on PPC,
 *   even if DLINFO_ARCH_ITEMS goes to zero or is undefined.
 */
#define DLINFO_ARCH_ITEMS	3
#define ARCH_DLINFO							\
do {									\
	sp -= DLINFO_ARCH_ITEMS * 2;					\
	NEW_AUX_ENT(0, AT_DCACHEBSIZE, dcache_bsize);			\
	NEW_AUX_ENT(1, AT_ICACHEBSIZE, icache_bsize);			\
	NEW_AUX_ENT(2, AT_UCACHEBSIZE, ucache_bsize);			\
	/*								\
	 * Now handle glibc compatibility.				\
	 */								\
	sp -= 2*2;							\
	NEW_AUX_ENT(0, AT_IGNOREPPC, AT_IGNOREPPC);			\
	NEW_AUX_ENT(1, AT_IGNOREPPC, AT_IGNOREPPC);			\
 } while (0)

#endif /* __KERNEL__ */
#endif /* __PPC64_ELF_H */
