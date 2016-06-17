/*
 *  include/asm-s390/elf.h
 *
 *  S390 version
 *
 *  Derived from "include/asm-i386/elf.h"
 */

#ifndef __ASMS390_ELF_H
#define __ASMS390_ELF_H

/*
 * ELF register definitions..
 */

#include <asm/ptrace.h>
#include <asm/user.h>


typedef s390_fp_regs elf_fpregset_t;
typedef s390_regs elf_gregset_t;

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS64
#define ELF_DATA	ELFDATA2MSB
#define ELF_ARCH	EM_S390

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) \
        (((x)->e_machine == EM_S390 || (x)->e_machine == EM_S390_OLD) \
         && (x)->e_ident[EI_CLASS] == ELF_CLASS)

/* For SVR4/S390 the function pointer to be registered with `atexit` is
   passed in R14. */
#define ELF_PLAT_INIT(_r, load_addr) \
	do { \
	_r->gprs[14] = 0; \
	current->thread.flags = 0; \
	} while(0)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	4096

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         (TASK_SIZE / 3 * 2)

/* Wow, the "main" arch needs arch dependent functions too.. :) */

/* regs is struct pt_regs, pr_reg is elf_gregset_t (which is
   now struct_user_regs, they are different) */

#define ELF_CORE_COPY_REGS(pr_reg, regs)	\
	memcpy(&pr_reg,regs,sizeof(elf_gregset_t)); \



/* This yields a mask that user programs can use to figure out what
   instruction set this CPU supports. */

#define ELF_HWCAP (0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.

   For the moment, we have only optimizations for the Intel generations,
   but that could change... */

#define ELF_PLATFORM (NULL)

#ifdef __KERNEL__
#define SET_PERSONALITY(ex, ibcs2)			\
do {							\
	if (ibcs2)					\
		set_personality(PER_SVR4);		\
	else if (current->personality != PER_LINUX32)	\
		set_personality(PER_LINUX);		\
} while (0)
#endif

#endif
