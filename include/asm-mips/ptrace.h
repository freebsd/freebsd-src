/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000 by Ralf Baechle
 *
 * Machine dependent structs and defines to help the user use
 * the ptrace system call.
 */
#ifndef _ASM_PTRACE_H
#define _ASM_PTRACE_H

#include <asm/isadep.h>

/* 0 - 31 are integer registers, 32 - 63 are fp registers.  */
#define FPR_BASE	32
#define PC		64
#define CAUSE		65
#define BADVADDR	66
#define MMHI		67
#define MMLO		68
#define FPC_CSR		69
#define FPC_EIR		70

#ifndef __ASSEMBLY__
/*
 * This struct defines the way the registers are stored on the stack during a
 * system call/exception. As usual the registers k0/k1 aren't being saved.
 */
struct pt_regs {
	/* Pad bytes for argument save space on the stack. */
	unsigned long pad0[6];

	/* Saved main processor registers. */
	unsigned long regs[32];

	/* Saved special registers. */
	unsigned long cp0_status;
	unsigned long lo;
	unsigned long hi;
	unsigned long cp0_badvaddr;
	unsigned long cp0_cause;
	unsigned long cp0_epc;
};

#define __str2(x) #x
#define __str(x) __str2(x)

#define save_static_function(symbol)                                    \
__asm__ (                                                               \
        ".globl\t" #symbol "\n\t"                                       \
        ".align\t2\n\t"                                                 \
        ".type\t" #symbol ", @function\n\t"                             \
        ".ent\t" #symbol ", 0\n"                                        \
        #symbol":\n\t"                                                  \
        ".frame\t$29, 0, $31\n\t"                                       \
        "sw\t$16,"__str(PT_R16)"($29)\t\t\t# save_static_function\n\t"  \
        "sw\t$17,"__str(PT_R17)"($29)\n\t"                              \
        "sw\t$18,"__str(PT_R18)"($29)\n\t"                              \
        "sw\t$19,"__str(PT_R19)"($29)\n\t"                              \
        "sw\t$20,"__str(PT_R20)"($29)\n\t"                              \
        "sw\t$21,"__str(PT_R21)"($29)\n\t"                              \
        "sw\t$22,"__str(PT_R22)"($29)\n\t"                              \
        "sw\t$23,"__str(PT_R23)"($29)\n\t"                              \
        "sw\t$30,"__str(PT_R30)"($29)\n\t"                              \
        ".end\t" #symbol "\n\t"                                         \
        ".size\t" #symbol",. - " #symbol)

/* Used in declaration of save_static functions.  */
#define static_unused static __attribute__((unused))

#endif /* !__ASSEMBLY__ */

/* Arbitrarily choose the same ptrace numbers as used by the Sparc code. */
/* #define PTRACE_GETREGS		12 */
/* #define PTRACE_SETREGS		13 */
/* #define PTRACE_GETFPREGS		14 */
/* #define PTRACE_SETFPREGS		15 */
/* #define PTRACE_GETFPXREGS		18 */
/* #define PTRACE_SETFPXREGS		19 */

#define PTRACE_SETOPTIONS	21

/* options set using PTRACE_SETOPTIONS */
#define PTRACE_O_TRACESYSGOOD	0x00000001

#ifdef __ASSEMBLY__
#include <asm/offset.h>
#endif

#ifdef __KERNEL__

#ifndef __ASSEMBLY__
/*
 * Does the process account for user or for system time?
 */
#define user_mode(regs) (((regs)->cp0_status & KU_MASK) == KU_USER)

#define instruction_pointer(regs) ((regs)->cp0_epc)

extern void show_regs(struct pt_regs *);
#endif /* !__ASSEMBLY__ */

#endif

#endif /* _ASM_PTRACE_H */
