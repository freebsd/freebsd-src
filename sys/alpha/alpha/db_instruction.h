/* $NetBSD: db_instruction.h,v 1.4 1997/09/16 22:53:32 thorpej Exp $ */

/*-
 * Mach Operating System
 * Copyright (c) 1993,1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 * $FreeBSD$
 */

/*
 *	File: alpha_instruction.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	11/91
 *
 *	Alpha Instruction set definition
 *
 *	Reference: "Alpha System Reference Manual", V4.0, April 1991
 *
 */

#ifndef	_ALPHA_INSTRUCTION_H_
#define	_ALPHA_INSTRUCTION_H_ 1

#if	!defined(ASSEMBLER)

/*
 *	All instructions are in one of five formats:
 *		Memory, Branch, Operate, Floating-point Operate, PAL
 *
 *	One minor departure from DEC's conventions is we use names
 *	for registers that are more akin their software use, e.g.
 *	rather then bluntly call them Ra/Rb/Rc we make clear which
 *	one is a source (Rs) and which one is a destination (Rd).
 *	When a second source register is defined we call it Rt.
 */


typedef union {
	/*
	 *	All instructions are 32 bits wide
	 */
	unsigned int	bits;

	/*
	 *	Generic instruction pseudo format; look at
	 *	opcode to see how to interpret the rest.
	 */
	struct {
		unsigned	  bits : 26,
				opcode : 6;
	} generic_format;

	/*
	 *	Memory instructions contain a 16 bit
	 *	signed immediate value and two register
	 *	specifiers
	 */
	struct {
		signed short	displacement;
		unsigned	rs : 5,
				rd : 5,
				opcode : 6;
	} mem_format;

	/*
	 *	Branch instruction contain a 21 bit offset,
	 *	which is sign-extended, shifted and combined
	 *	with the PC to form a 64 bit destination address.
	 *
	 *	In computed jump instructions the opcode is further
	 *	specified in the offset field, the rest of it is
	 *	used as branch target hint.  The destination of the
	 *	jump is the source register.
	 */
	struct {
		signed int	displacement : 21;
		unsigned	rd : 5,
				opcode : 6;
	} branch_format;

	struct {
		signed int	hint : 14;
		unsigned	action : 2,
				rs : 5,
				rd : 5,
				opcode : 6;
	} jump_format;


	struct {
		signed int offset	: 16;
		unsigned rb		: 5;
		unsigned ra		: 5;
		unsigned opcode		: 6;
	} memory_format;

	/*
	 *	Operate instructions are of two types, with
	 *	a second source register or with a literal
	 *	specifier.  Bit 12 sez which is which.
	 */
	struct {
		unsigned	rc : 5,
				function : 7,
				is_lit : 1,
				sbz_or_litlo : 3,
				rb_or_lithi : 5,
				ra : 5,
				opcode : 6;
	} operate_generic_format;
			
	struct {
		unsigned	rd : 5,
				function : 7,
				sbz : 4,
				rt : 5,
				rs : 5,
				opcode : 6;
	} operate_reg_format;

	struct {
		unsigned	rd : 5,
				function : 7,
				one : 1,
				literal : 8,
				rs : 5,
				opcode : 6;
	} operate_lit_format;


	/*
	 *	Floating point operate instruction are quite
	 *	uniform in the encoding.  As for the semantics..
	 */
	struct {
		unsigned	fd : 5,
				function : 11,
				ft : 5,
				fs : 5,
				opcode : 6;
	} float_format;


	/*
	 *	PAL instructions just define the major opcode
	 */

	struct {
		unsigned	function : 26,
				opcode : 6;
	} pal_format;

} alpha_instruction;

#endif /* !defined(ASSEMBLER) */

/*
 *
 *	Encoding of regular instructions  (Appendix C op cit)
 *
 */

		/* OPCODE, bits 26..31 */

#define	op_pal		0x00		/* see PAL sub-table */
					/* 1..7 reserved */
#define	op_lda		0x08
#define	op_ldah		0x09
#define	op_ldbu		0x0a
#define	op_ldq_u	0x0b
#define	op_ldwu		0x0c
#define	op_stw		0x0d
#define	op_stb		0x0e
#define	op_stq_u	0x0f

#define	op_arit		0x10		/* see ARIT sub-table */
#define	op_logical	0x11		/* see LOGICAL sub-table */
#define	op_bit		0x12		/* see BIT sub-table */
#define	op_mul		0x13		/* see MUL sub-table */
					/* reserved */
#define	op_vax_float	0x15		/* see FLOAT sub-table */
#define	op_ieee_float	0x16		/* see FLOAT sub-table */
#define	op_any_float	0x17		/* see FLOAT sub-table */

#define	op_special	0x18		/* see SPECIAL sub-table */
#define	op_pal19	0x19		/* reserved for pal code */
#define	op_j		0x1a		/* see JUMP sub-table */
#define	op_pal1b	0x1b		/* reserved for pal code */
#define	op_intmisc	0x1c		/* see INTMISC sub-table */
#define	op_pal1d	0x1d		/* reserved for pal code */
#define	op_pal1e	0x1e		/* reserved for pal code */
#define	op_pal1f	0x1f		/* reserved for pal code */

#define	op_ldf		0x20
#define	op_ldg		0x21
#define	op_lds		0x22
#define	op_ldt		0x23
#define	op_stf		0x24
#define	op_stg		0x25
#define	op_sts		0x26
#define	op_stt		0x27
#define	op_ldl		0x28
#define	op_ldq		0x29
#define	op_ldl_l	0x2a
#define	op_ldq_l	0x2b
#define	op_stl		0x2c
#define	op_stq		0x2d
#define	op_stl_c	0x2e
#define	op_stq_c	0x2f
#define	op_br		0x30
#define	op_fbeq		0x31
#define	op_fblt		0x32
#define	op_fble		0x33
#define	op_bsr		0x34
#define	op_fbne		0x35
#define	op_fbge		0x36
#define	op_fbgt		0x37
#define	op_blbc		0x38
#define	op_beq		0x39
#define	op_blt		0x3a
#define	op_ble		0x3b
#define	op_blbs		0x3c
#define	op_bne		0x3d
#define	op_bge		0x3e
#define op_bgt		0x3f


		/* PAL, "function" opcodes (bits 0..25) */
/*
 * What we will implement is TBD.  These are the unprivileged ones
 * that we probably have to support for compat reasons.
 */

/* See <machine/pal.h> */

		/* ARIT, "function" opcodes (bits 5..11)  */

#define	op_addl		0x00
#define	op_s4addl	0x02
#define	op_subl		0x09
#define	op_s4subl	0x0b
#define	op_cmpbge	0x0f
#define	op_s8addl	0x12
#define	op_s8subl	0x1b
#define	op_cmpult	0x1d
#define	op_addq		0x20
#define	op_s4addq	0x22
#define	op_subq		0x29
#define	op_s4subq	0x2b
#define	op_cmpeq	0x2d
#define	op_s8addq	0x32
#define	op_s8subq	0x3b
#define	op_cmpule	0x3d
#define	op_addl_v	0x40
#define	op_subl_v	0x49
#define	op_cmplt	0x4d
#define	op_addq_v	0x60
#define	op_subq_v	0x69
#define	op_cmple	0x6d


		/* LOGICAL, "function" opcodes (bits 5..11)  */

#define	op_and		0x00
#define	op_andnot	0x08	/* bic */
#define	op_cmovlbs	0x14
#define	op_cmovlbc	0x16
#define	op_or		0x20	/* bis */
#define	op_cmoveq	0x24
#define	op_cmovne	0x26
#define	op_ornot	0x28
#define	op_xor		0x40
#define	op_cmovlt	0x44
#define	op_cmovge	0x46
#define	op_xornot	0x48	/* eqv */
#define	op_amask	0x61
#define	op_cmovle	0x64
#define	op_cmovgt	0x66
#define	op_implver	0x6c

		/* BIT, "function" opcodes (bits 5..11)  */

#define	op_mskbl	0x02
#define	op_extbl	0x06
#define	op_insbl	0x0b
#define	op_mskwl	0x12
#define	op_extwl	0x16
#define	op_inswl	0x1b
#define	op_mskll	0x22
#define	op_extll	0x26
#define	op_insll	0x2b
#define	op_zap		0x30
#define	op_zapnot	0x31
#define	op_mskql	0x32
#define	op_srl		0x34
#define	op_extql	0x36
#define	op_sll		0x39
#define	op_insql	0x3b
#define	op_sra		0x3c
#define	op_mskwh	0x52
#define	op_inswh	0x57
#define	op_extwh	0x5a
#define	op_msklh	0x62
#define	op_inslh	0x67
#define	op_extlh	0x6a
#define	op_extqh	0x7a
#define	op_insqh	0x77
#define	op_mskqh	0x72

		/* MUL, "function" opcodes (bits 5..11)  */

#define	op_mull		0x00
#define	op_mulq_v	0x60
#define	op_mull_v	0x40
#define	op_umulh	0x30
#define	op_mulq		0x20


		/* SPECIAL, "displacement" opcodes (bits 0..15)  */

#define	op_draint	0x0000
#define	op_mb		0x4000
#define	op_fetch	0x8000
#define	op_fetch_m	0xa000
#define	op_rpcc		0xc000
#define op_rc		0xe000
#define	op_rs		0xf000

		/* JUMP, "action" opcodes (bits 14..15) */

#define	op_jmp		0x0
#define	op_jsr		0x1
#define	op_ret		0x2
#define	op_jcr		0x3

		/* INTMISC, "function" opcodes (operate format) */

#define	op_sextb	0x00
#define	op_sextw	0x01
#define	op_ctpop	0x30
#define	op_perr		0x31
#define	op_ctlz		0x32
#define	op_cttz		0x33
#define	op_unpkbw	0x34
#define	op_unpkbl	0x35
#define	op_pkwb		0x36
#define	op_pklb		0x37
#define	op_minsb8	0x38
#define	op_minsw4	0x39
#define	op_minub8	0x3a
#define	op_minuw4	0x3b
#define	op_maxub8	0x3c
#define	op_maxuw4	0x3d
#define	op_maxsb8	0x3e
#define	op_maxsw4	0x3f
#define	op_ftoit	0x70
#define	op_ftois	0x78

/*
 *
 *	Encoding of floating point instructions (pagg. C-5..6 op cit)
 *
 *	Load and store operations use opcodes op_ldf..op_stt
 */

		/* any FLOAT, "function" opcodes (bits 5..11)  */

#define	op_cvtlq	0x010
#define	op_cpys		0x020
#define	op_cpysn	0x021
#define	op_cpyse	0x022
#define	op_mt_fpcr	0x024
#define	op_mf_fpcr	0x025
#define	op_fcmoveq	0x02a
#define	op_fcmovne	0x02b
#define	op_fcmovlt	0x02c
#define	op_fcmovge	0x02d
#define	op_fcmovle	0x02e
#define	op_fcmovgt	0x02f
#define	op_cvtql	0x030
#define	op_cvtql_v	0x130
#define	op_cvtql_sv	0x330


		/* ieee FLOAT, "function" opcodes (bits 5..11)  */

#define	op_adds_c	0x000
#define	op_subs_c	0x001
#define	op_muls_c	0x002
#define	op_divs_c	0x003
#define	op_addt_c	0x020
#define	op_subt_c	0x021
#define	op_mult_c	0x022
#define	op_divt_c	0x023
#define	op_cvtts_c	0x02c
#define	op_cvttq_c	0x02f
#define	op_cvtqs_c	0x03c
#define	op_cvtqt_c	0x03e
#define	op_adds_m	0x040
#define	op_subs_m	0x041
#define	op_muls_m	0x042
#define	op_divs_m	0x043
#define	op_addt_m	0x060
#define	op_subt_m	0x061
#define	op_mult_m	0x062
#define	op_divt_m	0x063
#define	op_cvtts_m	0x06c
#define	op_cvtqs_m	0x07c
#define	op_cvtqt_m	0x07e
#define	op_adds		0x080
#define	op_subs		0x081
#define	op_muls		0x082
#define	op_divs		0x083
#define	op_addt		0x0a0
#define	op_subt		0x0a1
#define	op_mult		0x0a2
#define	op_divt		0x0a3
#define	op_cmptun	0x0a4
#define	op_cmpteq	0x0a5
#define	op_cmptlt	0x0a6
#define	op_cmptle	0x0a7
#define	op_cvtts	0x0ac
#define	op_cvttq	0x0af
#define	op_cvtqs	0x0bc
#define	op_cvtqt	0x0be
#define	op_adds_d	0x0c0
#define	op_subs_d	0x0c1
#define	op_muls_d	0x0c2
#define	op_divs_d	0x0c3
#define	op_addt_d	0x0e0
#define	op_subt_d	0x0e1
#define	op_mult_d	0x0e2
#define	op_divt_d	0x0e3
#define	op_cvtts_d	0x0ec
#define	op_cvtqs_d	0x0fc
#define	op_cvtqt_d	0x0fe
#define	op_adds_uc	0x100
#define	op_subs_uc	0x101
#define	op_muls_uc	0x102
#define	op_divs_uc	0x103
#define	op_addt_uc	0x120
#define	op_subt_uc	0x121
#define	op_mult_uc	0x122
#define	op_divt_uc	0x123
#define	op_cvtts_uc	0x12c
#define	op_cvttq_vc	0x12f
#define	op_adds_um	0x140
#define	op_subs_um	0x141
#define	op_muls_um	0x142
#define	op_divs_um	0x143
#define	op_addt_um	0x160
#define	op_subt_um	0x161
#define	op_mult_um	0x162
#define	op_divt_um	0x163
#define	op_cvtts_um	0x16c
#define	op_adds_u	0x180
#define	op_subs_u	0x181
#define	op_muls_u	0x182
#define	op_divs_u	0x183
#define	op_addt_u	0x1a0
#define	op_subt_u	0x1a1
#define	op_mult_u	0x1a2
#define	op_divt_u	0x1a3
#define	op_cvtts_u	0x1ac
#define	op_cvttq_v	0x1af
#define	op_adds_ud	0x1c0
#define	op_subs_ud	0x1c1
#define	op_muls_ud	0x1c2
#define	op_divs_ud	0x1c3
#define	op_addt_ud	0x1e0
#define	op_subt_ud	0x1e1
#define	op_mult_ud	0x1e2
#define	op_divt_ud	0x1e3
#define	op_cvtts_ud	0x1ec
#define	op_adds_suc	0x500
#define	op_subs_suc	0x501
#define	op_muls_suc	0x502
#define	op_divs_suc	0x503
#define	op_addt_suc	0x520
#define	op_subt_suc	0x521
#define	op_mult_suc	0x522
#define	op_divt_suc	0x523
#define	op_cvtts_suc	0x52c
#define	op_cvttq_svc	0x52f
#define	op_adds_sum	0x540
#define	op_subs_sum	0x541
#define	op_muls_sum	0x542
#define	op_divs_sum	0x543
#define	op_addt_sum	0x560
#define	op_subt_sum	0x561
#define	op_mult_sum	0x562
#define	op_divt_sum	0x563
#define	op_cvtts_sum	0x56c
#define	op_adds_su	0x580
#define	op_subs_su	0x581
#define	op_muls_su	0x582
#define	op_divs_su	0x583
#define	op_addt_su	0x5a0
#define	op_subt_su	0x5a1
#define	op_mult_su	0x5a2
#define	op_divt_su	0x5a3
#define	op_cmptun_su	0x5a4
#define	op_cmpteq_su	0x5a5
#define	op_cmptlt_su	0x5a6
#define	op_cmptle_su	0x5a7
#define	op_cvtts_su	0x5ac
#define	op_cvttq_sv	0x5af
#define	op_adds_sud	0x5c0
#define	op_subs_sud	0x5c1
#define	op_muls_sud	0x5c2
#define	op_divs_sud	0x5c3
#define	op_addt_sud	0x5e0
#define	op_subt_sud	0x5e1
#define	op_mult_sud	0x5e2
#define	op_divt_sud	0x5e3
#define	op_cvtts_sud	0x5ec
#define	op_adds_suic	0x700
#define	op_subs_suic	0x701
#define	op_muls_suic	0x702
#define	op_divs_suic	0x703
#define	op_addt_suic	0x720
#define	op_subt_suic	0x721
#define	op_mult_suic	0x722
#define	op_divt_suic	0x723
#define	op_cvtts_suic	0x72c
#define	op_cvttq_svic	0x72f
#define	op_cvtqs_suic	0x73c
#define	op_cvtqt_suic	0x73e
#define	op_adds_suim	0x740
#define	op_subs_suim	0x741
#define	op_muls_suim	0x742
#define	op_divs_suim	0x743
#define	op_addt_suim	0x760
#define	op_subt_suim	0x761
#define	op_mult_suim	0x762
#define	op_divt_suim	0x763
#define	op_cvtts_suim	0x76c
#define	op_cvtqs_suim	0x77c
#define	op_cvtqt_suim	0x77e
#define	op_adds_sui	0x780
#define	op_subs_sui	0x781
#define	op_muls_sui	0x782
#define	op_divs_sui	0x783
#define	op_addt_sui	0x7a0
#define	op_subt_sui	0x7a1
#define	op_mult_sui	0x7a2
#define	op_divt_sui	0x7a3
#define	op_cvtts_sui	0x7ac
#define	op_cvttq_svi	0x7af
#define	op_cvtqs_sui	0x7bc
#define	op_cvtqt_sui	0x7be
#define	op_adds_suid	0x7c0
#define	op_subs_suid	0x7c1
#define	op_muls_suid	0x7c2
#define	op_divs_suid	0x7c3
#define	op_addt_suid	0x7e0
#define	op_subt_suid	0x7e1
#define	op_mult_suid	0x7e2
#define	op_divt_suid	0x7e3
#define	op_cvtts_suid	0x7ec
#define	op_cvtqs_suid	0x7fc
#define	op_cvtqt_suid	0x7fe


		/* vax FLOAT, "function" opcodes (bits 5..11)  */

#define	op_addf_c	0x000
#define	op_subf_c	0x001
#define	op_mulf_c	0x002
#define	op_divf_c	0x003
#define	op_cvtdg_c	0x01e
#define	op_addg_c	0x020
#define	op_subg_c	0x021
#define	op_mulg_c	0x022
#define	op_divg_c	0x023
#define	op_cvtgf_c	0x02c
#define	op_cvtgd_c	0x02d
#define	op_cvtgqg_c	0x02f
#define	op_cvtqf_c	0x03c
#define	op_cvtqg_c	0x03e
#define	op_addf		0x080
#define	op_subf		0x081
#define	op_mulf		0x082
#define	op_divf		0x083
#define	op_cvtdg	0x09e
#define	op_addg		0x0a0
#define	op_subg		0x0a1
#define	op_mulg		0x0a2
#define	op_divg		0x0a3
#define	op_cmpgeq	0x0a5
#define	op_cmpglt	0x0a6
#define	op_cmpgle	0x0a7
#define	op_cvtgf	0x0ac
#define	op_cvtgd	0x0ad
#define	op_cvtgq	0x0af
#define	op_cvtqf	0x0bc
#define	op_cvtqg	0x0be
#define	op_addf_uc	0x100
#define	op_subf_uc	0x101
#define	op_mulf_uc	0x102
#define	op_divf_uc	0x103
#define	op_cvtdg_uc	0x11e
#define	op_addg_uc	0x120
#define	op_subg_uc	0x121
#define	op_mulg_uc	0x122
#define	op_divg_uc	0x123
#define	op_cvtgf_uc	0x12c
#define	op_cvtgd_uc	0x12d
#define	op_cvtgqg_vc	0x12f
#define	op_addf_u	0x180
#define	op_subf_u	0x181
#define	op_mulf_u	0x182
#define	op_divf_u	0x183
#define	op_cvtdg_u	0x19e
#define	op_addg_u	0x1a0
#define	op_subg_u	0x1a1
#define	op_mulg_u	0x1a2
#define	op_divg_u	0x1a3
#define	op_cvtgf_u	0x1ac
#define	op_cvtgd_u	0x1ad
#define	op_cvtgqg_v	0x1af
#define	op_addf_sc	0x400
#define	op_subf_sc	0x401
#define	op_mulf_sc	0x402
#define	op_divf_sc	0x403
#define	op_cvtdg_sc	0x41e
#define	op_addg_sc	0x420
#define	op_subg_sc	0x421
#define	op_mulg_sc	0x422
#define	op_divg_sc	0x423
#define	op_cvtgf_sc	0x42c
#define	op_cvtgd_sc	0x42d
#define	op_cvtgqg_sc	0x42f
#define	op_cvtqf_sc	0x43c
#define	op_cvtqg_sc	0x43e
#define	op_addf_s	0x480
#define	op_subf_s	0x481
#define	op_mulf_s	0x482
#define	op_divf_s	0x483
#define	op_cvtdg_s	0x49e
#define	op_addg_s	0x4a0
#define	op_subg_s	0x4a1
#define	op_mulg_s	0x4a2
#define	op_divg_s	0x4a3
#define	op_cmpgeq_s	0x4a5
#define	op_cmpglt_s	0x4a6
#define	op_cmpgle_s	0x4a7
#define	op_cvtgf_s	0x4ac
#define	op_cvtgd_s	0x4ad
#define	op_cvtgqg_s	0x4af
#define	op_cvtqf_s	0x4bc
#define	op_cvtqg_s	0x4be
#define	op_addf_suc	0x500
#define	op_subf_suc	0x501
#define	op_mulf_suc	0x502
#define	op_divf_suc	0x503
#define	op_cvtdg_suc	0x51e
#define	op_addg_suc	0x520
#define	op_subg_suc	0x521
#define	op_mulg_suc	0x522
#define	op_divg_suc	0x523
#define	op_cvtgf_suc	0x52c
#define	op_cvtgd_suc	0x52d
#define	op_cvtgqg_svc	0x52f
#define	op_addf_su	0x580
#define	op_subf_su	0x581
#define	op_mulf_su	0x582
#define	op_divf_su	0x583
#define	op_cvtdg_su	0x59e
#define	op_addg_su	0x5a0
#define	op_subg_su	0x5a1
#define	op_mulg_su	0x5a2
#define	op_divg_su	0x5a3
#define	op_cvtgf_su	0x5ac
#define	op_cvtgd_su	0x5ad
#define	op_cvtgqg_sv	0x5af


#endif	/* _ALPHA_INSTRUCTION_H_ */
