/*
 * Support for 32-bit Linux for S390 ELF binaries.
 *
 * Copyright (C) 2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 * Author(s): Gerhard Tonn (ton@de.ibm.com)
 *
 * Heavily inspired by the 32-bit Sparc compat code which is
 * Copyright (C) 1995, 1996, 1997, 1998 David S. Miller (davem@redhat.com)
 * Copyright (C) 1995, 1996, 1997, 1998 Jakub Jelinek   (jj@ultra.linux.cz)
 */


#define __ASMS390_ELF_H

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2MSB
#define ELF_ARCH	EM_S390

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) \
	(((x)->e_machine == EM_S390 || (x)->e_machine == EM_S390_OLD) \
         && (x)->e_ident[EI_CLASS] == ELF_CLASS)

/* ELF register definitions */
#define NUM_GPRS      16
#define NUM_FPRS      16
#define NUM_ACRS      16    

#define TASK31_SIZE		(0x80000000UL)

/* For SVR4/S390 the function pointer to be registered with `atexit` is
   passed in R14. */
#define ELF_PLAT_INIT(_r, load_addr) \
	do { \
	_r->gprs[14] = 0; \
	current->thread.flags |= S390_FLAG_31BIT; \
	} while(0)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE       4096

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         ((TASK31_SIZE & 0x80000000) \
                                ? TASK31_SIZE / 3 * 2 \
                                : 2 * TASK31_SIZE / 3)     

/* Wow, the "main" arch needs arch dependent functions too.. :) */

/* regs is struct pt_regs, pr_reg is elf_gregset_t (which is
   now struct_user_regs, they are different) */

#define ELF_CORE_COPY_REGS(pr_reg, regs)        \
	{ \
	int i; \
	memcpy(&pr_reg.psw.mask, &regs->psw.mask, 4); \
	memcpy(&pr_reg.psw.addr, ((char*)&regs->psw.addr)+4, 4); \
	for(i=0; i<NUM_GPRS; i++) \
		pr_reg.gprs[i] = regs->gprs[i]; \
	for(i=0; i<NUM_ACRS; i++) \
		pr_reg.acrs[i] = regs->acrs[i]; \
	pr_reg.orig_gpr2 = regs->orig_gpr2; \
	}



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
	if (ibcs2)                                      \
		set_personality(PER_SVR4);              \
	else if (current->personality != PER_LINUX32)   \
		set_personality(PER_LINUX);             \
} while (0)
#endif 

#include "linux32.h"

typedef _s390_fp_regs32 elf_fpregset_t;

typedef struct
{
	
	_psw_t32	psw;
	__u32		gprs[__NUM_GPRS]; 
	__u32		acrs[__NUM_ACRS]; 
	__u32		orig_gpr2;
} s390_regs32;
typedef s390_regs32 elf_gregset_t;

#include <asm/processor.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/elfcore.h>

int setup_arg_pages32(struct linux_binprm *bprm);

struct timeval32
{
    int tv_sec, tv_usec;
};

#define elf_prstatus elf_prstatus32
struct elf_prstatus32
{
	struct elf_siginfo pr_info;	/* Info associated with signal */
	short	pr_cursig;		/* Current signal */
	u32	pr_sigpend;	/* Set of pending signals */
	u32	pr_sighold;	/* Set of held signals */
	pid_t	pr_pid;
	pid_t	pr_ppid;
	pid_t	pr_pgrp;
	pid_t	pr_sid;
	struct timeval32 pr_utime;	/* User time */
	struct timeval32 pr_stime;	/* System time */
	struct timeval32 pr_cutime;	/* Cumulative user time */
	struct timeval32 pr_cstime;	/* Cumulative system time */
	elf_gregset_t pr_reg;	/* GP registers */
	int pr_fpvalid;		/* True if math co-processor being used.  */
};

#define elf_prpsinfo elf_prpsinfo32
struct elf_prpsinfo32
{
	char	pr_state;	/* numeric process state */
	char	pr_sname;	/* char for pr_state */
	char	pr_zomb;	/* zombie */
	char	pr_nice;	/* nice val */
	u32	pr_flag;	/* flags */
	u16	pr_uid;
	u16	pr_gid;
	pid_t	pr_pid, pr_ppid, pr_pgrp, pr_sid;
	/* Lots missing */
	char	pr_fname[16];	/* filename of executable */
	char	pr_psargs[ELF_PRARGSZ];	/* initial part of arg list */
};

#include <linux/highuid.h>

#undef NEW_TO_OLD_UID
#undef NEW_TO_OLD_GID
#define NEW_TO_OLD_UID(uid) ((uid) > 65535) ? (u16)overflowuid : (u16)(uid)
#define NEW_TO_OLD_GID(gid) ((gid) > 65535) ? (u16)overflowgid : (u16)(gid) 

#define elf_addr_t	u32
#define elf_caddr_t	u32
/*
#define init_elf_binfmt init_elf32_binfmt
*/
#undef CONFIG_BINFMT_ELF
#ifdef CONFIG_BINFMT_ELF32
#define CONFIG_BINFMT_ELF CONFIG_BINFMT_ELF32
#endif
#undef CONFIG_BINFMT_ELF_MODULE
#ifdef CONFIG_BINFMT_ELF32_MODULE
#define CONFIG_BINFMT_ELF_MODULE CONFIG_BINFMT_ELF32_MODULE
#endif

#undef start_thread
#define start_thread                    start_thread31 
#define setup_arg_pages(bprm)           setup_arg_pages32(bprm)
#define elf_map				elf_map32

MODULE_DESCRIPTION("Binary format loader for compatibility with 32bit Linux for S390 binaries,"
                   " Copyright 2000 IBM Corporation"); 
MODULE_AUTHOR("Gerhard Tonn <ton@de.ibm.com>");

#undef MODULE_DESCRIPTION
#undef MODULE_AUTHOR

#include "../../../fs/binfmt_elf.c"

static unsigned long
elf_map32 (struct file *filep, unsigned long addr, struct elf_phdr *eppnt, int prot, int type)
{
	unsigned long map_addr;

	if(!addr)
		addr = 0x40000000;

	down_write(&current->mm->mmap_sem);
	map_addr = do_mmap(filep, ELF_PAGESTART(addr),
			   eppnt->p_filesz + ELF_PAGEOFFSET(eppnt->p_vaddr), prot, type,
			   eppnt->p_offset - ELF_PAGEOFFSET(eppnt->p_vaddr));
	up_write(&current->mm->mmap_sem);
	return(map_addr);
}

