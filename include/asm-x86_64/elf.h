#ifndef __ASM_X86_64_ELF_H
#define __ASM_X86_64_ELF_H

/*
 * ELF register definitions..
 */

#include <asm/ptrace.h>
#include <asm/user.h>

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof (struct user_regs_struct) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct user_i387_struct elf_fpregset_t;
typedef struct user_fxsr_struct elf_fpxregset_t;

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) \
	((x)->e_machine == EM_X86_64)

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS64
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_X86_64

/* SVR4/i386 ABI (pages 3-31, 3-32) says that when the program starts %edx
   contains a pointer to a function which might be registered using `atexit'.
   This provides a mean for the dynamic linker to call DT_FINI functions for
   shared libraries that have been loaded before the code runs.

   A value of 0 tells we have no such handler. 

   We might as well make sure everything else is cleared too (except for %esp),
   just to make things more deterministic.
 */
#define ELF_PLAT_INIT(_r, load_addr)	do { \
	struct task_struct *cur = current; \
	(_r)->rbx = 0; (_r)->rcx = 0; (_r)->rdx = 0; \
	(_r)->rsi = 0; (_r)->rdi = 0; (_r)->rbp = 0; \
	(_r)->rax = 0;				\
	(_r)->r8 = 0;				\
	(_r)->r9 = 0;				\
	(_r)->r10 = 0;				\
	(_r)->r11 = 0;				\
	(_r)->r12 = 0;				\
	(_r)->r13 = 0;				\
	(_r)->r14 = 0;				\
	(_r)->r15 = 0;				\
        cur->thread.fs = 0; cur->thread.gs = 0; \
	cur->thread.fsindex = 0; cur->thread.gsindex = 0; \
        cur->thread.ds = 0; cur->thread.es = 0;  \
	cur->thread.flags &= ~THREAD_IA32; \
} while (0)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	4096

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         (2 * TASK_SIZE / 3)

/* regs is struct pt_regs, pr_reg is elf_gregset_t (which is
   now struct_user_regs, they are different). Assumes current is the process
   getting dumped. */

#define ELF_CORE_COPY_REGS(pr_reg, regs)  do { \
	unsigned v;						\
	(pr_reg)[0] = (regs)->r15;				\
	(pr_reg)[1] = (regs)->r14;				\
	(pr_reg)[2] = (regs)->r13;				\
	(pr_reg)[3] = (regs)->r12;				\
	(pr_reg)[4] = (regs)->rbp;				\
	(pr_reg)[5] = (regs)->rbx;				\
	(pr_reg)[6] = (regs)->r11;				\
	(pr_reg)[7] = (regs)->r10;				\
	(pr_reg)[8] = (regs)->r9;				\
	(pr_reg)[9] = (regs)->r8;				\
	(pr_reg)[10] = (regs)->rax;				\
	(pr_reg)[11] = (regs)->rcx;				\
	(pr_reg)[12] = (regs)->rdx;				\
	(pr_reg)[13] = (regs)->rsi;				\
	(pr_reg)[14] = (regs)->rdi;				\
	(pr_reg)[15] = (regs)->orig_rax;			\
	(pr_reg)[16] = (regs)->rip;				\
	(pr_reg)[17] = (regs)->cs;				\
	(pr_reg)[18] = (regs)->eflags;				\
	(pr_reg)[19] = (regs)->rsp;				\
	(pr_reg)[20] = (regs)->ss;				\
	(pr_reg)[21] = current->thread.fs;			\
	(pr_reg)[22] = current->thread.gs;			\
	asm("movl %%ds,%0" : "=r" (v)); (pr_reg)[23] = v;	\
	asm("movl %%es,%0" : "=r" (v)); (pr_reg)[24] = v;	\
	asm("movl %%fs,%0" : "=r" (v)); (pr_reg)[25] = v;	\
	asm("movl %%gs,%0" : "=r" (v)); (pr_reg)[26] = v;	\
} while(0);

/* This yields a mask that user programs can use to figure out what
   instruction set this CPU supports.  This could be done in user space,
   but it's not easy, and we've already done it here.  */

#define ELF_HWCAP	(boot_cpu_data.x86_capability[0])

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.

   For the moment, we have only optimizations for the Intel generations,
   but that could change... */

/* I'm not sure if we can use '-' here */
#define ELF_PLATFORM  ("x86_64")

#ifdef __KERNEL__
extern void set_personality_64bit(void);
#define SET_PERSONALITY(ex, ibcs2) set_personality_64bit()
	
#endif

#endif
