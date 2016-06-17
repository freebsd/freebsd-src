#ifndef __PPC_ELF_H
#define __PPC_ELF_H

/*
 * ELF register definitions..
 */
#include <asm/types.h>
#include <asm/ptrace.h>
#include <asm/cputable.h>

#define ELF_NGREG	48	/* includes nip, msr, lr, etc. */
#define ELF_NFPREG	33	/* includes fpscr */
#define ELF_NVRREG	33	/* includes vscr */

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_ARCH	EM_PPC
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2MSB

/* General registers */
typedef unsigned long elf_greg_t;
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

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

#define elf_check_arch(x) ((x)->e_machine == EM_PPC)

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         (0x08000000)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	4096

#define ELF_CORE_COPY_REGS(gregs, regs) \
	memcpy(gregs, regs, \
	       sizeof(struct pt_regs) < sizeof(elf_gregset_t)? \
	       sizeof(struct pt_regs): sizeof(elf_gregset_t));


/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  This could be done in userspace,
   but it's not easy, and we've already done it here.  */

#define ELF_HWCAP	(cur_cpu_spec[0]->cpu_user_features)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.

   For the moment, we have only optimizations for the Intel generations,
   but that could change... */

#define ELF_PLATFORM	(NULL)

#define SET_PERSONALITY(ex, ibcs2) set_personality((ibcs2)?PER_SVR4:PER_LINUX)

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
#endif
