/* $Id: elf.h,v 1.30.2.1 2002/02/04 22:37:47 davem Exp $ */
#ifndef __ASM_SPARC64_ELF_H
#define __ASM_SPARC64_ELF_H

/*
 * ELF register definitions..
 */

#include <asm/ptrace.h>
#ifdef __KERNEL__
#include <asm/processor.h>
#endif

/*
 * These are used to set parameters in the core dumps.
 */
#ifndef ELF_ARCH
#define ELF_ARCH		EM_SPARCV9
#define ELF_CLASS		ELFCLASS64
#define ELF_DATA		ELFDATA2MSB

typedef unsigned long elf_greg_t;

#define ELF_NGREG 36
typedef elf_greg_t elf_gregset_t[ELF_NGREG];
/* Format of 64-bit elf_gregset_t is:
 * 	G0 --> G7
 * 	O0 --> O7
 * 	L0 --> L7
 * 	I0 --> I7
 *	TSTATE
 *	TPC
 *	TNPC
 *	Y
 */
#include <asm/psrcompat.h>
#define ELF_CORE_COPY_REGS(__elf_regs, __pt_regs)	\
do {	unsigned long *dest = &(__elf_regs[0]);		\
	struct pt_regs *src = (__pt_regs);		\
	unsigned long *sp;				\
	int i;						\
	for(i = 0; i < 16; i++)				\
		dest[i] = src->u_regs[i];		\
	/* Don't try this at home kids... */		\
	set_fs(USER_DS);				\
	sp = (unsigned long *)				\
	 ((src->u_regs[14] + STACK_BIAS)		\
	  & 0xfffffffffffffff8UL);			\
	for(i = 0; i < 16; i++)				\
		__get_user(dest[i+16], &sp[i]);		\
	set_fs(KERNEL_DS);				\
	dest[32] = src->tstate;				\
	dest[33] = src->tpc;				\
	dest[34] = src->tnpc;				\
	dest[35] = src->y;				\
} while (0);

typedef struct {
	unsigned long	pr_regs[32];
	unsigned long	pr_fsr;
	unsigned long	pr_gsr;
	unsigned long	pr_fprs;
} elf_fpregset_t;
#endif

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#ifndef elf_check_arch
#define elf_check_arch(x) ((x)->e_machine == ELF_ARCH)	/* Might be EM_SPARCV9 or EM_SPARC */
#endif

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	PAGE_SIZE

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#ifndef ELF_ET_DYN_BASE
#define ELF_ET_DYN_BASE         0x0000010000000000UL
#endif


/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  */

/* On Ultra, we support all of the v8 capabilities. */
#define ELF_HWCAP	((HWCAP_SPARC_FLUSH | HWCAP_SPARC_STBAR | \
			  HWCAP_SPARC_SWAP | HWCAP_SPARC_MULDIV | \
			  HWCAP_SPARC_V9) | \
			 ((tlb_type == cheetah || tlb_type == cheetah_plus) ? \
			  HWCAP_SPARC_ULTRA3 : 0))

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.  */

#define ELF_PLATFORM	(NULL)

#ifdef __KERNEL__
#define SET_PERSONALITY(ex, ibcs2)			\
do {	unsigned char flags = current->thread.flags;	\
	if ((ex).e_ident[EI_CLASS] == ELFCLASS32)	\
		flags |= SPARC_FLAG_32BIT;		\
	else						\
		flags &= ~SPARC_FLAG_32BIT;		\
	if (flags != current->thread.flags) {		\
		/* flush_thread will update pgd cache */\
		current->thread.flags |= SPARC_FLAG_ABI_PENDING; \
	} else {					\
		current->thread.flags &= ~SPARC_FLAG_ABI_PENDING; \
	}						\
	if (ibcs2)					\
		set_personality(PER_SVR4);		\
	else if (current->personality != PER_LINUX32)	\
		set_personality(PER_LINUX);		\
} while (0)
#endif

#endif /* !(__ASM_SPARC64_ELF_H) */
