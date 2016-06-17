#ifndef __ASM_CRIS_USER_H
#define __ASM_CRIS_USER_H

#include <linux/types.h>
#include <asm/ptrace.h>
#include <asm/page.h>

/*
 * Core file format: The core file is written in such a way that gdb
 * can understand it and provide useful information to the user (under
 * linux we use the `trad-core' bfd).  The file contents are as follows:
 *
 *  upage: 1 page consisting of a user struct that tells gdb
 *	what is present in the file.  Directly after this is a
 *	copy of the task_struct, which is currently not used by gdb,
 *	but it may come in handy at some point.  All of the registers
 *	are stored as part of the upage.  The upage should always be
 *	only one page long.
 *  data: The data segment follows next.  We use current->end_text to
 *	current->brk to pick up all of the user variables, plus any memory
 *	that may have been sbrk'ed.  No attempt is made to determine if a
 *	page is demand-zero or if a page is totally unused, we just cover
 *	the entire range.  All of the addresses are rounded in such a way
 *	that an integral number of pages is written.
 *  stack: We need the stack information in order to get a meaningful
 *	backtrace.  We need to write the data from usp to
 *	current->start_stack, so we round each of these in order to be able
 *	to write an integer number of pages.
 */

/* User mode registers, used for core dumps. In order to keep ELF_NGREG
   sensible we let all registers be 32 bits. The csr registers are included
   for future use. */
struct user_regs_struct {
        unsigned long r0;       /* General registers. */
        unsigned long r1;
        unsigned long r2;
        unsigned long r3;
        unsigned long r4;
        unsigned long r5;
        unsigned long r6;
        unsigned long r7;
        unsigned long r8;
        unsigned long r9;
        unsigned long r10;
        unsigned long r11;
        unsigned long r12;
        unsigned long r13;
        unsigned long sp;       /* Stack pointer. */
        unsigned long pc;       /* Program counter. */
        unsigned long p0;       /* Constant zero (only 8 bits). */
        unsigned long vr;       /* Version register (only 8 bits). */
        unsigned long p2;       /* Reserved. */
        unsigned long p3;       /* Reserved. */
        unsigned long p4;       /* Constant zero (only 16 bits). */
        unsigned long ccr;      /* Condition code register (only 16 bits). */
        unsigned long p6;       /* Reserved. */
        unsigned long mof;      /* Multiply overflow register. */
        unsigned long p8;       /* Constant zero. */
        unsigned long ibr;      /* Not accessible. */
        unsigned long irp;      /* Not accessible. */
        unsigned long srp;      /* Subroutine return pointer. */
        unsigned long bar;      /* Not accessible. */
        unsigned long dccr;     /* Dword condition code register. */
        unsigned long brp;      /* Not accessible. */
        unsigned long usp;      /* User-mode stack pointer. Same as sp when 
                                   in user mode. */
        unsigned long csrinstr; /* Internal status registers. */
        unsigned long csraddr;
        unsigned long csrdata;
};

        
struct user {
	struct user_regs_struct	regs;		/* entire machine state */
	size_t		u_tsize;		/* text size (pages) */
	size_t		u_dsize;		/* data size (pages) */
	size_t		u_ssize;		/* stack size (pages) */
	unsigned long	start_code;		/* text starting address */
	unsigned long	start_data;		/* data starting address */
	unsigned long	start_stack;		/* stack starting address */
	long int	signal;			/* signal causing core dump */
	struct regs *	u_ar0;			/* help gdb find registers */
	unsigned long	magic;			/* identifies a core file */
	char		u_comm[32];		/* user command name */
};

#define NBPG			PAGE_SIZE
#define UPAGES			1
#define HOST_TEXT_START_ADDR	(u.start_code)
#define HOST_DATA_START_ADDR	(u.start_data)
#define HOST_STACK_END_ADDR	(u.start_stack + u.u_ssize * NBPG)

#endif /* __ASM_CRIS_USER_H */
