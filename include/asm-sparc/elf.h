/* $Id: elf.h,v 1.22 2000/07/12 01:27:08 davem Exp $ */
#ifndef __ASMSPARC_ELF_H
#define __ASMSPARC_ELF_H

/*
 * ELF register definitions..
 */

#include <linux/config.h>
#include <asm/ptrace.h>
#include <asm/mbus.h>

/* For the most part we present code dumps in the format
 * Solaris does.
 */
typedef unsigned long elf_greg_t;
#define ELF_NGREG 38
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

/* Format is:
 * 	G0 --> G7
 *	O0 --> O7
 *	L0 --> L7
 *	I0 --> I7
 *	PSR, PC, nPC, Y, WIM, TBR
 */
#define ELF_CORE_COPY_REGS(__elf_regs, __pt_regs)	\
do {	unsigned long *dest = &(__elf_regs[0]);		\
	struct pt_regs *src = (__pt_regs);		\
	unsigned long *sp;				\
	memcpy(&dest[0], &src->u_regs[0],		\
	       sizeof(unsigned long) * 16);		\
	/* Don't try this at home kids... */		\
	set_fs(USER_DS);				\
	sp = (unsigned long *) src->u_regs[14];		\
	copy_from_user(&dest[16], sp,			\
		       sizeof(unsigned long) * 16);	\
	set_fs(KERNEL_DS);				\
	dest[32] = src->psr;				\
	dest[33] = src->pc;				\
	dest[34] = src->npc;				\
	dest[35] = src->y;				\
	dest[36] = dest[37] = 0; /* XXX */		\
} while(0);

typedef struct {
	union {
		unsigned long	pr_regs[32];
		double		pr_dregs[16];
	} pr_fr;
	unsigned long __unused;
	unsigned long	pr_fsr;
	unsigned char	pr_qcnt;
	unsigned char	pr_q_entrysize;
	unsigned char	pr_en;
	unsigned int	pr_q[64];
} elf_fpregset_t;

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ((x)->e_machine == EM_SPARC)

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_ARCH	EM_SPARC
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2MSB

#define USE_ELF_CORE_DUMP
#ifndef CONFIG_SUN4
#define ELF_EXEC_PAGESIZE	4096
#else
#define ELF_EXEC_PAGESIZE	8192
#endif


/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         (0x08000000)

/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  This can NOT be done in userspace
   on Sparc.  */

/* Sun4c has none of the capabilities, most sun4m's have them all.
 * XXX This is gross, set some global variable at boot time. -DaveM
 */
#define ELF_HWCAP	((ARCH_SUN4C_SUN4) ? 0 : \
			 (HWCAP_SPARC_FLUSH | HWCAP_SPARC_STBAR | \
			  HWCAP_SPARC_SWAP | \
			  ((srmmu_modtype != Cypress && \
			    srmmu_modtype != Cypress_vE && \
			    srmmu_modtype != Cypress_vD) ? \
			   HWCAP_SPARC_MULDIV : 0)))

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo. */

#define ELF_PLATFORM	(NULL)

#ifdef __KERNEL__
#define SET_PERSONALITY(ex, ibcs2) set_personality((ibcs2)?PER_SVR4:PER_LINUX)
#endif

#endif /* !(__ASMSPARC_ELF_H) */
