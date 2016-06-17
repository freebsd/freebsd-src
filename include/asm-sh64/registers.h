#ifndef __ASM_SH64_REGISTERS_H
#define __ASM_SH64_REGISTERS_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * include/asm-sh64/registers.h
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 */

#ifdef __ASSEMBLY__
/* =====================================================================
** 
** Section 1: acts on assembly sources pre-processed by GPP ( <source.S>).
**	      Assigns symbolic names to control & target registers.
*/

/*
 * Concerning registers name, the assembly sources follows the
 * CDC naming convention.
 * This section converts CDC registers names into RedHat style.
 */

/*
 * Control Registers.
 */
#define SR	cr0
#define SSR	cr1
#define PSSR	cr2
			/* cr3 UNDEFINED */
#define INTEVT	cr4
#define EXPEVT	cr5
#define PEXPEVT	cr6
#define TRA	cr7
#define SPC	cr8
#define PSPC	cr9
#define RESVEC	cr10
#define VBR	cr11
			/* cr12 UNDEFINED */
#define TEA	cr13
			/* cr14-cr15 UNDEFINED */
#define DCR	cr16
#define KCR0	cr17
#define KCR1	cr18
			/* cr19-cr31 UNDEFINED */
			/* cr32-cr61 RESERVED */
#define CTC	cr62
#define USR	cr63

/*
 * ABI dependent registers (general purpose set)
 * Note: this set of registers name could be shared by the
 *       CDC and RedHat toolchain
 */
#define RET	r2
#define ARG1	r2
#define ARG2	r3
#define ARG3	r4
#define ARG4	r5
#define ARG5	r6
#define ARG6	r7
#define FBP	r14
#define SP	r15
#define GVDT	r26
#define GCDT	r27
#define LINK	r18
#define OS	r40
#define ZERO	r63

/*
 * Target registers need name convertion...
 * (defines only those used by assembly code)
 */
#define t0	tr0
#define t1	tr1
#define t2	tr2
#define t3	tr3
#define t4	tr4
#define t5	tr5
#define t6	tr6
#define t7	tr7

/*
** RedHat style compatibility macros:
**
**   _loada: loads the address the address  of the symbol (first
**           argument) into the general purpose register (second
**           argument)
*/
        .macro  _loada   symbol, gp_reg
         movi   \symbol, \gp_reg
        .endm

        .macro  _ptar    symbol, tr_reg
         pt     \symbol, \tr_reg
        .endm

        .macro  _ptaru   symbol, tr_reg
         pt/u   \symbol, \tr_reg
        .endm

        .macro  _pta    disp_b,  tr_reg
         pta    $+\disp_b, \tr_reg
        .endm

/*
 * Status register defines: used only by assembly sources (and
 * 			    syntax independednt)
 */
#define SR_RESET_VAL	0x0000000050008000
#define SR_HARMLESS	0x00000000500080f0	/* Write ignores for most */
#define SR_ENABLE_FPU	0xffffffffffff7fff	/* AND with this */

#ifdef ST_DEBUG
#define SR_ENABLE_MMU	0x0000000084000000	/* OR with this */
#else
#define SR_ENABLE_MMU	0x0000000080000000	/* OR with this */
#endif

#define SR_UNBLOCK_EXC	0xffffffffefffffff	/* AND with this */
#define SR_BLOCK_EXC	0x0000000010000000	/* OR with this */

#else	/* Not __ASSEMBLY__ syntax */

/* =====================================================================
** 
** Section 2: this is required to manage __asm__ statement expanded
**            by "C" compiler
*/
	/*
	** RedHat style symbolic address resolution inside "asm" blocks
	*/
        asm ("\t.macro  _loada   symbol, gp_reg\n"
             "\tmovi    \\symbol, \\gp_reg\n"
             "\t.endm\n"
             "\n"
             "\t.macro  _ptar    symbol, tr_reg\n"
             "\tpt      \\symbol, \\tr_reg\n"
             "\t.endm\n"
             "\n"
             "\t.macro  _pta    disp_b, tr_reg\n"
             "\tpta     $+\\disp_b, \\tr_reg\n"
             "\t.endm\n"
	     "\n"
             "\t.macro  _fgetscr  f_reg63\n"
	     "\tfgetscr \\f_reg63\n"
             "\t.endm\n"
	     "\n"
             "\t.macro  _fputscr  f_reg63\n"
	     "\tfputscr \\f_reg63\n"
             "\t.endm\n"
	     "\n"
	    );

	/*
	** RedHat style target register name inside "asm" blocks
	*/
#	define __t0	__str(tr0)
#	define __t1	__str(tr1)
#	define __t2	__str(tr2)
#	define __t3	__str(tr3)
#	define __t4	__str(tr4)
#	define __t5	__str(tr5)
#	define __t6	__str(tr6)
#	define __t7	__str(tr7)

	/*
	** RedHat style control register name inside "asm" blocks
	*/
#	define __c0	__str(cr0)		/* SR       */
#	define __c1	__str(cr1)		/* SSR      */
#	define __c2	__str(cr2)		/* PSSR     */
#	define __c4	__str(cr4)		/* INTEVT   */
#	define __c5	__str(cr5)		/* EXPEVT   */
#	define __c6	__str(cr6)		/* PEXPEVT  */
#	define __c8	__str(cr8)		/* SPC      */
#	define __c9	__str(cr9)		/* PSPC     */
#	define __c13	__str(cr13)		/* TEA      */
#	define __c17	__str(cr17)		/* KCR0     */
#	define __c18	__str(cr18)		/* KCR1     */

	/*
	** RedHat style float and double register name inside "asm" blocks
	** defines below are used only in .../arch/sh5/kernel/fpu.c
	*/
#	define __f(x)	__str(fr##x)
#	define __p(x)	__str(fp##x)
#	define __d(x)	__str(dr##x)

	/*
	** Stringify reg. name (common for CDC & RedHat)
	*/
#	define __str(x)  #x

#endif /* __ASSEMBLY__ */
#endif /* __ASM_SH64_REGISTERS_H */
