/* tc-i960.h - Basic 80960 instruction formats.
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2,
   or (at your option) any later version.

   GAS is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
   the GNU General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with GAS; see the file COPYING.  If not, write
   to the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef TC_I960
#define TC_I960 1

#define NO_LISTING

/*
 * The 'COJ' instructions are actually COBR instructions with the 'b' in
 * the mnemonic replaced by a 'j';  they are ALWAYS "de-optimized" if necessary:
 * if the displacement will not fit in 13 bits, the assembler will replace them
 * with the corresponding compare and branch instructions.
 *
 * All of the 'MEMn' instructions are the same format; the 'n' in the name
 * indicates the default index scale factor (the size of the datum operated on).
 *
 * The FBRA formats are not actually an instruction format.  They are the
 * "convenience directives" for branching on floating-point comparisons,
 * each of which generates 2 instructions (a 'bno' and one other branch).
 *
 * The CALLJ format is not actually an instruction format.  It indicates that
 * the instruction generated (a CTRL-format 'call') should have its relocation
 * specially flagged for link-time replacement with a 'bal' or 'calls' if
 * appropriate.
 */

/* tailor gas */
#define SYMBOLS_NEED_BACKPOINTERS
#define LOCAL_LABELS_FB
#define WANT_BITFIELDS

/* tailor the coff format */
#define OBJ_COFF_SECTION_HEADER_HAS_ALIGNMENT
#define OBJ_COFF_MAX_AUXENTRIES	(2)

/* other */
#define CTRL	0
#define COBR	1
#define COJ	2
#define REG	3
#define MEM1	4
#define MEM2	5
#define MEM4	6
#define MEM8	7
#define MEM12	8
#define MEM16	9
#define FBRA	10
#define CALLJ	11

/* Masks for the mode bits in REG format instructions */
#define M1		0x0800
#define M2		0x1000
#define M3		0x2000

/* Generate the 12-bit opcode for a REG format instruction by placing the
 * high 8 bits in instruction bits 24-31, the low 4 bits in instruction bits
 * 7-10.
 */

#define REG_OPC(opc)	((opc & 0xff0) << 20) | ((opc & 0xf) << 7)

/* Generate a template for a REG format instruction:  place the opcode bits
 * in the appropriate fields and OR in mode bits for the operands that will not
 * be used.  I.e.,
 *		set m1=1, if src1 will not be used
 *		set m2=1, if src2 will not be used
 *		set m3=1, if dst  will not be used
 *
 * Setting the "unused" mode bits to 1 speeds up instruction execution(!).
 * The information is also useful to us because some 1-operand REG instructions
 * use the src1 field, others the dst field; and some 2-operand REG instructions
 * use src1/src2, others src1/dst.  The set mode bits enable us to distinguish.
 */
#define R_0(opc)	( REG_OPC(opc) | M1 | M2 | M3 )	/* No operands      */
#define R_1(opc)	( REG_OPC(opc) | M2 | M3 )	/* 1 operand: src1  */
#define R_1D(opc)	( REG_OPC(opc) | M1 | M2 )	/* 1 operand: dst   */
#define R_2(opc)	( REG_OPC(opc) | M3 )		/* 2 ops: src1/src2 */
#define R_2D(opc)	( REG_OPC(opc) | M2 )		/* 2 ops: src1/dst  */
#define R_3(opc)	( REG_OPC(opc) )		/* 3 operands       */

/* DESCRIPTOR BYTES FOR REGISTER OPERANDS
 *
 * Interpret names as follows:
 *	R:   global or local register only
 *	RS:  global, local, or (if target allows) special-function register only
 *	RL:  global or local register, or integer literal
 *	RSL: global, local, or (if target allows) special-function register;
 *		or integer literal
 *	F:   global, local, or floating-point register
 *	FL:  global, local, or floating-point register; or literal (including
 *		floating point)
 *
 * A number appended to a name indicates that registers must be aligned,
 * as follows:
 *	2: register number must be multiple of 2
 *	4: register number must be multiple of 4
 */

#define SFR	0x10		/* Mask for the "sfr-OK" bit */
#define LIT	0x08		/* Mask for the "literal-OK" bit */
#define FP	0x04		/* Mask for "floating-point-OK" bit */

/* This macro ors the bits together.  Note that 'align' is a mask
 * for the low 0, 1, or 2 bits of the register number, as appropriate.
 */
#define OP(align,lit,fp,sfr)	( align | lit | fp | sfr )

#define R	OP( 0, 0,   0,  0   )
#define RS	OP( 0, 0,   0,  SFR )
#define RL	OP( 0, LIT, 0,  0   )
#define RSL	OP( 0, LIT, 0,  SFR )
#define F	OP( 0, 0,   FP, 0   )
#define FL	OP( 0, LIT, FP, 0   )
#define R2	OP( 1, 0,   0,  0   )
#define RL2	OP( 1, LIT, 0,  0   )
#define F2	OP( 1, 0,   FP, 0   )
#define FL2	OP( 1, LIT, FP, 0   )
#define R4	OP( 3, 0,   0,  0   )
#define RL4	OP( 3, LIT, 0,  0   )
#define F4	OP( 3, 0,   FP, 0   )
#define FL4	OP( 3, LIT, FP, 0   )

#define M	0x7f	/* Memory operand (MEMA & MEMB format instructions) */

/* Macros to extract info from the register operand descriptor byte 'od'.
 */
#define SFR_OK(od)	(od & SFR)	/* TRUE if sfr operand allowed */
#define LIT_OK(od)	(od & LIT)	/* TRUE if literal operand allowed */
#define FP_OK(od)	(od & FP)	/* TRUE if floating-point op allowed */
#define REG_ALIGN(od,n)	((od & 0x3 & n) == 0)
/* TRUE if reg #n is properly aligned */
#define MEMOP(od)	(od == M)	/* TRUE if operand is a memory operand*/

/* Classes of 960 intructions:
 *	- each instruction falls into one class.
 *	- each target architecture supports one or more classes.
 *
 * EACH CONSTANT MUST CONTAIN 1 AND ONLY 1 SET BIT!:  see targ_has_iclass().
 */
#define I_BASE	0x01	/* 80960 base instruction set	*/
#define I_CX	0x02	/* 80960Cx instruction		*/
#define I_DEC	0x04	/* Decimal instruction		*/
#define I_FP	0x08	/* Floating point instruction	*/
#define I_KX	0x10	/* 80960Kx instruction		*/
#define I_MIL	0x20	/* Military instruction		*/

/* MEANING OF 'n_other' in the symbol record.
 *
 * If non-zero, the 'n_other' fields indicates either a leaf procedure or
 * a system procedure, as follows:
 *
 *	1 <= n_other <= 32 :
 *		The symbol is the entry point to a system procedure.
 *		'n_value' is the address of the entry, as for any other
 *		procedure.  The system procedure number (which can be used in
 *		a 'calls' instruction) is (n_other-1).  These entries come from
 *		'.sysproc' directives.
 *
 *	n_other == N_CALLNAME
 *		the symbol is the 'call' entry point to a leaf procedure.
 *		The *next* symbol in the symbol table must be the corresponding
 *		'bal' entry point to the procedure (see following).  These
 *		entries come from '.leafproc' directives in which two different
 *		symbols are specified (the first one is represented here).
 *
 *
 *	n_other == N_BALNAME
 *		the symbol is the 'bal' entry point to a leaf procedure.
 *		These entries result from '.leafproc' directives in which only
 *		one symbol is specified, or in which the same symbol is
 *		specified twice.
 *
 * Note that an N_CALLNAME entry *must* have a corresponding N_BALNAME entry,
 * but not every N_BALNAME entry must have an N_CALLNAME entry.
 */
#define	N_CALLNAME	(-1)
#define	N_BALNAME	(-2)


/* i960 uses a custom relocation record. */

/* let obj-aout.h know */
#define CUSTOM_RELOC_FORMAT 1
/* let a.out.gnu.h know */
#define N_RELOCATION_INFO_DECLARED 1
struct relocation_info {
	int	 r_address;	/* File address of item to be relocated	*/
	unsigned
    r_index:24,/* Index of symbol on which relocation is based*/
    r_pcrel:1,	/* 1 => relocate PC-relative; else absolute
		 *	On i960, pc-relative implies 24-bit
		 *	address, absolute implies 32-bit.
		 */
    r_length:2,	/* Number of bytes to relocate:
		 *	0 => 1 byte
		 *	1 => 2 bytes
		 *	2 => 4 bytes -- only value used for i960
		 */
    r_extern:1,
    r_bsr:1,	/* Something for the GNU NS32K assembler */
    r_disp:1,	/* Something for the GNU NS32K assembler */
    r_callj:1,	/* 1 if relocation target is an i960 'callj' */
    nuthin:1;	/* Unused				*/
};

/* hacks for tracking callj's */
#if defined(OBJ_AOUT) | defined(OBJ_BOUT)

#define TC_S_IS_SYSPROC(s)	((1 <= S_GET_OTHER(s)) && (S_GET_OTHER(s) <= 32))
#define TC_S_IS_BALNAME(s)	(S_GET_OTHER(s) == N_BALNAME)
#define TC_S_IS_CALLNAME(s)	(S_GET_OTHER(s) == N_CALLNAME)
#define TC_S_IS_BADPROC(s)	((S_GET_OTHER(s) != 0) && !TC_S_IS_CALLNAME(s) && !TC_S_IS_BALNAME(s) && !TC_S_IS_SYSPROC(s))

#define TC_S_SET_SYSPROC(s, p)	(S_SET_OTHER((s), (p)+1))
#define TC_S_GET_SYSPROC(s)	(S_GET_OTHER(s)-1)

#define TC_S_FORCE_TO_BALNAME(s)	(S_SET_OTHER((s), N_BALNAME))
#define TC_S_FORCE_TO_CALLNAME(s)	(S_SET_OTHER((s), N_CALLNAME))
#define TC_S_FORCE_TO_SYSPROC(s)	{;}

#elif defined(OBJ_COFF)

#define TC_S_IS_SYSPROC(s)	(S_GET_STORAGE_CLASS(s) == C_SCALL)
#define TC_S_IS_BALNAME(s)	(SF_GET_BALNAME(s))
#define TC_S_IS_CALLNAME(s)	(SF_GET_CALLNAME(s))
#define TC_S_IS_BADPROC(s)	(TC_S_IS_SYSPROC(s) && TC_S_GET_SYSPROC(s) < 0 && 31 < TC_S_GET_SYSPROC(s))

#define TC_S_SET_SYSPROC(s, p)	((s)->sy_symbol.ost_auxent[1].x_sc.x_stindx = (p))
#define TC_S_GET_SYSPROC(s)	((s)->sy_symbol.ost_auxent[1].x_sc.x_stindx)

#define TC_S_FORCE_TO_BALNAME(s)	(SF_SET_BALNAME(s))
#define TC_S_FORCE_TO_CALLNAME(s)	(SF_SET_CALLNAME(s))
#define TC_S_FORCE_TO_SYSPROC(s)	(S_SET_STORAGE_CLASS((s), C_SCALL))

#else /* switch on OBJ */
you lose
#endif /* witch on OBJ */

#if __STDC__ == 1

    void brtab_emit(void);
void reloc_callj(); /* this is really reloc_callj(fixS *fixP) but I don't want to change header inclusion order. */
void tc_set_bal_of_call(); /* this is really tc_set_bal_of_call(symbolS *callP, symbolS *balP) */

#else /* not __STDC__ */

void brtab_emit();
void reloc_callj();
void tc_set_bal_of_call();

#endif /* not __STDC__ */

char *_tc_get_bal_of_call(); /* this is really symbolS *tc_get_bal_of_call(symbolS *callP). */
#define tc_get_bal_of_call(c)	((symbolS *) _tc_get_bal_of_call(c))
#endif

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 131
 * End:
 */

/* end of tc-i960.h */
