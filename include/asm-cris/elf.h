#ifndef __ASMCRIS_ELF_H
#define __ASMCRIS_ELF_H

/*
 * ELF register definitions..
 */

#include <asm/ptrace.h>

typedef unsigned long elf_greg_t;

/* Note that NGREG is defined to ELF_NGREG in include/linux/elfcore.h, and is
   thus exposed to user-space. */
#define ELF_NGREG (sizeof (struct user_regs_struct) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

/* A placeholder; CRIS does not have any fp regs.  */
typedef unsigned long elf_fpregset_t;

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ( (x)->e_machine == EM_CRIS )

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB;
#define ELF_ARCH	EM_CRIS

	/* SVR4/i386 ABI (pages 3-31, 3-32) says that when the program
	   starts (a register; assume first param register for CRIS)
	   contains a pointer to a function which might be
	   registered using `atexit'.  This provides a mean for the
	   dynamic linker to call DT_FINI functions for shared libraries
	   that have been loaded before the code runs.

	   A value of 0 tells we have no such handler.  */
	
	/* Explicitly set registers to 0 to increase determinism.  */
#define ELF_PLAT_INIT(_r, load_addr)	do { \
	(_r)->r13 = 0; (_r)->r12 = 0; (_r)->r11 = 0; (_r)->r10 = 0; \
	(_r)->r9 = 0;  (_r)->r8 = 0;  (_r)->r7 = 0;  (_r)->r6 = 0;  \
	(_r)->r5 = 0;  (_r)->r4 = 0;  (_r)->r3 = 0;  (_r)->r2 = 0;  \
	(_r)->r1 = 0;  (_r)->r0 = 0;  (_r)->mof = 0; (_r)->srp = 0; \
} while (0)

#define USE_ELF_CORE_DUMP

/* The additional layer below is because the stack pointer is missing in 
   the pt_regs struct, but needed in a core dump. pr_reg is a elf_gregset_t,
   and should be filled in according to the layout of the user_regs_struct
   struct; regs is a pt_regs struct. We dump all registers, though several are
   obviously unnecessary. That way there's less need for intelligence at 
   the receiving end (i.e. gdb). */
#define ELF_CORE_COPY_REGS(pr_reg, regs)                   \
	pr_reg[0] = regs->r0;                              \
	pr_reg[1] = regs->r1;                              \
	pr_reg[2] = regs->r2;                              \
	pr_reg[3] = regs->r3;                              \
	pr_reg[4] = regs->r4;                              \
	pr_reg[5] = regs->r5;                              \
	pr_reg[6] = regs->r6;                              \
	pr_reg[7] = regs->r7;                              \
	pr_reg[8] = regs->r8;                              \
	pr_reg[9] = regs->r9;                              \
	pr_reg[10] = regs->r10;                            \
	pr_reg[11] = regs->r11;                            \
	pr_reg[12] = regs->r12;                            \
	pr_reg[13] = regs->r13;                            \
	pr_reg[14] = rdusp();               /* sp */       \
	pr_reg[15] = regs->irp;             /* pc */       \
	pr_reg[16] = 0;                     /* p0 */       \
	pr_reg[17] = rdvr();                /* vr */       \
	pr_reg[18] = 0;                     /* p2 */       \
	pr_reg[19] = 0;                     /* p3 */       \
	pr_reg[20] = 0;                     /* p4 */       \
	pr_reg[21] = (regs->dccr & 0xffff); /* ccr */      \
	pr_reg[22] = 0;                     /* p6 */       \
	pr_reg[23] = regs->mof;             /* mof */      \
	pr_reg[24] = 0;                     /* p8 */       \
	pr_reg[25] = 0;                     /* ibr */      \
	pr_reg[26] = 0;                     /* irp */      \
	pr_reg[27] = regs->srp;             /* srp */      \
	pr_reg[28] = 0;                     /* bar */      \
	pr_reg[29] = regs->dccr;            /* dccr */     \
	pr_reg[30] = 0;                     /* brp */      \
	pr_reg[31] = rdusp();               /* usp */      \
	pr_reg[32] = 0;                     /* csrinstr */ \
	pr_reg[33] = 0;                     /* csraddr */  \
	pr_reg[34] = 0;                     /* csrdata */


#define ELF_EXEC_PAGESIZE	8192

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         (2 * TASK_SIZE / 3)

/* This yields a mask that user programs can use to figure out what
   instruction set this CPU supports.  This could be done in user space,
   but it's not easy, and we've already done it here.  */

#define ELF_HWCAP       (0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.
*/

#define ELF_PLATFORM  (NULL)

#ifdef __KERNEL__
#define SET_PERSONALITY(ex, ibcs2) set_personality((ibcs2)?PER_SVR4:PER_LINUX)
#endif

#endif
