/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/alpha/include/inst.h,v 1.2 1999/08/28 00:38:45 peter Exp $
 */

#ifndef _MACHINE_INST_H_
#define _MACHINE_INST_H_

union alpha_instruction {
    u_int32_t word;
    struct {
	u_int32_t argument	: 26;
	u_int32_t opcode	: 6;
#define op_call_pal		0x00
#define op_lda			0x08
#define	op_ldah			0x09
#define op_ldbu			0x0a
#define op_unop			0x0b
#define op_ldq_u		0x0b
#define op_ldwu			0x0c
#define op_stw			0x0d
#define op_stb			0x0e
#define op_stq_u		0x0f
#define op_inta			0x10
#define		inta_addl	0x00
#define		inta_s4addl	0x02
#define		inta_subl	0x09
#define		inta_s4subl	0x0b
#define		inta_cmpbge	0x0f
#define		inta_s8addl	0x12
#define		inta_s8subl	0x1b
#define		inta_cmpult	0x1d
#define		inta_addq	0x20
#define		inta_s4addq	0x22
#define		inta_subq	0x29
#define		inta_s4subq	0x2b
#define		inta_cmpeq	0x2d
#define		inta_s8addq	0x32
#define		inta_s8subq	0x3b
#define		inta_cmpule	0x3d
#define		inta_addlv	0x40
#define		inta_sublv	0x49
#define		inta_cmplt	0x4d
#define		inta_addqv	0x60
#define		inta_subqv	0x69
#define		inta_cmple	0x6d
#define op_intl			0x11
#define		intl_and	0x00
#define		intl_andnot	0x08
#define		intl_bic	0x08
#define		intl_cmovlbs	0x14
#define		intl_cmovlbc	0x16
#define		intl_or		0x20
#define		intl_bis	0x20
#define		intl_cmoveq	0x24
#define		intl_cmovne	0x26
#define		intl_ornot	0x28
#define		intl_xor	0x40
#define		intl_cmovlt	0x44
#define		intl_cmovge	0x46
#define		intl_eqv	0x48
#define		intl_amask	0x61
#define		intl_cmovle	0x64
#define		intl_cmovgt	0x66
#define		intl_implver	0x6c
#define op_ints			0x12
#define		ints_mskbl	0x02
#define		ints_extbl	0x06
#define		ints_insbl	0x0b
#define		ints_mskwl	0x12
#define		ints_extwl	0x16
#define		ints_inswl	0x1b
#define		ints_mskll	0x22
#define		ints_extll	0x26
#define		ints_insll	0x2b
#define		ints_zap	0x30
#define		ints_zapnot	0x31
#define		ints_mskql	0x32
#define		ints_srl	0x34
#define		ints_extql	0x36
#define		ints_sll	0x39
#define		ints_insql	0x3b
#define		ints_sra	0x3c
#define		ints_mskwh	0x52
#define		ints_inswh	0x57
#define		ints_extwh	0x5a
#define		ints_msklh	0x62
#define		ints_inslh	0x67
#define		ints_extlh	0x6a
#define		ints_mskqh	0x72
#define		ints_insqh	0x77
#define		ints_extqh	0x7a
#define op_intm			0x13
#define		intm_mull	0x00
#define		intm_mulq	0x20
#define		intm_umulh	0x30
#define		intm_mullv	0x40
#define		intm_mulqv	0x60
#define op_opc14		0x14
#define op_fltv			0x15
#define op_flti			0x16
#define		flti_addsc	0x000
#define		flti_subsc	0x001
#define		flti_mulsc	0x002
#define		flti_divsc	0x003
#define		flti_addtc	0x020
#define		flti_subtc	0x021
#define		flti_multc	0x022
#define		flti_divtc	0x023
#define		flti_cvttsc	0x02c
#define		flti_cvttqc	0x02f
#define		flti_cvtqsc	0x03c
#define		flti_cvtqtc	0x03e

#define		flti_addsm	0x040
#define		flti_subsm	0x041
#define		flti_mulsm	0x042
#define		flti_divsm	0x043
#define		flti_addtm	0x060
#define		flti_subtm	0x061
#define		flti_multm	0x062
#define		flti_divtm	0x063
#define		flti_cvttsm	0x06c
#define		flti_cvttqm	0x06f
#define		flti_cvtqsm	0x07c
#define		flti_cvtqtm	0x07e

#define		flti_adds	0x080
#define		flti_subs	0x081
#define		flti_muls	0x082
#define		flti_divs	0x083

#define		flti_addt	0x0a0
#define		flti_subt	0x0a1
#define		flti_mult	0x0a2
#define		flti_divt	0x0a3
#define		flti_cmptun	0x0a4
#define		flti_cmpteq	0x0a5
#define		flti_cmptlt	0x0a6
#define		flti_cmptle	0x0a7
#define		flti_cvtts	0x0ac
#define		flti_cvttq	0x0af
#define		flti_cvtqs	0x0bc
#define		flti_cvtqt	0x0be

#define		flti_addsd	0x0c0
#define		flti_subsd	0x0c1
#define		flti_mulsd	0x0c2
#define		flti_divsd	0x0c3
#define		flti_addtd	0x0e0
#define		flti_subtd	0x0e1
#define		flti_multd	0x0e2
#define		flti_divtd	0x0e3
#define		flti_cvttsd	0x0ec
#define		flti_cvttqd	0x0ef
#define		flti_cvtqsd	0x0fc
#define		flti_cvtqtd	0x0fe

#define		flti_addsuc	0x100
#define		flti_subsuc	0x101
#define		flti_mulsuc	0x102
#define		flti_divsuc	0x103
#define		flti_addtuc	0x120
#define		flti_subtuc	0x121
#define		flti_multuc	0x122
#define		flti_divtuc	0x123
#define		flti_cvttsuc	0x12c
#define		flti_cvttqvc	0x12f

#define		flti_addsum	0x140
#define		flti_subsum	0x141
#define		flti_mulsum	0x142
#define		flti_divsum	0x143
#define		flti_addtum	0x160
#define		flti_subtum	0x161
#define		flti_multum	0x162
#define		flti_divtum	0x163
#define		flti_cvttsum	0x16c
#define		flti_cvttqvm	0x16f

#define		flti_addsu	0x180
#define		flti_subsu	0x181
#define		flti_mulsu	0x182
#define		flti_divsu	0x183
#define		flti_addtu	0x1a0
#define		flti_subtu	0x1a1
#define		flti_multu	0x1a2
#define		flti_divtu	0x1a3
#define		flti_cvttsu	0x1ac
#define		flti_cvttqv	0x1af

#define		flti_addsud	0x1c0
#define		flti_subsud	0x1c1
#define		flti_mulsud	0x1c2
#define		flti_divsud	0x1c3
#define		flti_addtud	0x1e0
#define		flti_subtud	0x1e1
#define		flti_multud	0x1e2
#define		flti_divtud	0x1e3
#define		flti_cvttsud	0x1ec
#define		flti_cvttqvd	0x1ef

#define		flti_cvtst	0x2ac

#define		flti_addssuc	0x500
#define		flti_subssuc	0x501
#define		flti_mulssuc	0x502
#define		flti_divssuc	0x503
#define		flti_addtsuc	0x520
#define		flti_subtsuc	0x521
#define		flti_multsuc	0x522
#define		flti_divtsuc	0x523
#define		flti_cvttssuc	0x52c
#define		flti_cvttqsvc	0x52f

#define		flti_addssum	0x540
#define		flti_subssum	0x541
#define		flti_mulssum	0x542
#define		flti_divssum	0x543
#define		flti_addtsum	0x560
#define		flti_subtsum	0x561
#define		flti_multsum	0x562
#define		flti_divtsum	0x563
#define		flti_cvttssum	0x56c
#define		flti_cvttqsvm	0x56f

#define		flti_addssu	0x580
#define		flti_subssu	0x581
#define		flti_mulssu	0x582
#define		flti_divssu	0x583
#define		flti_addtsu	0x5a0
#define		flti_subtsu	0x5a1
#define		flti_multsu	0x5a2
#define		flti_divtsu	0x5a3
#define		flti_cmptunsu	0x5a4
#define		flti_cmpteqsu	0x5a5
#define		flti_cmptltsu	0x5a6
#define		flti_cmptlesu	0x5a7
#define		flti_cvttssu	0x5ac
#define		flti_cvttqsv	0x5af

#define		flti_addssud	0x5c0
#define		flti_subssud	0x5c1
#define		flti_mulssud	0x5c2
#define		flti_divssud	0x5c3
#define		flti_addtsud	0x5e0
#define		flti_subtsud	0x5e1
#define		flti_multsud	0x5e2
#define		flti_divtsud	0x5e3
#define		flti_cvttssud	0x5ec
#define		flti_cvttqsvd	0x5ef

#define		flti_cvtsts	0x6ac

#define		flti_addssuic	0x700
#define		flti_subssuic	0x701
#define		flti_mulssuic	0x702
#define		flti_divssuic	0x703
#define		flti_addtsuic	0x720
#define		flti_subtsuic	0x721
#define		flti_multsuic	0x722
#define		flti_divtsuic	0x723
#define		flti_cvttssuic	0x72c
#define		flti_cvttqsvic	0x72f
#define		flti_cvtqssuic	0x73c
#define		flti_cvtqtsuic	0x73e

#define		flti_addssuim	0x740
#define		flti_subssuim	0x741
#define		flti_mulssuim	0x742
#define		flti_divssuim	0x743
#define		flti_addtsuim	0x760
#define		flti_subtsuim	0x761
#define		flti_multsuim	0x762
#define		flti_divtsuim	0x763
#define		flti_cvttssuim	0x76c
#define		flti_cvttqsvim	0x76f
#define		flti_cvtqssuim	0x77c
#define		flti_cvtqtsuim	0x77e

#define		flti_addssui	0x780
#define		flti_subssui	0x781
#define		flti_mulssui	0x782
#define		flti_divssui	0x783
#define		flti_addtsui	0x7a0
#define		flti_subtsui	0x7a1
#define		flti_multsui	0x7a2
#define		flti_divtsui	0x7a3
#define		flti_cmptunsui	0x7a4
#define		flti_cmpteqsui	0x7a5
#define		flti_cmptltsui	0x7a6
#define		flti_cmptlesui	0x7a7
#define		flti_cvttssui	0x7ac
#define		flti_cvttqsvi	0x7af
#define		flti_cvtqssui	0x7bc
#define		flti_cvtqtsui	0x7bc

#define		flti_addssuid	0x7c0
#define		flti_subssuid	0x7c1
#define		flti_mulssuid	0x7c2
#define		flti_divssuid	0x7c3
#define		flti_addtsuid	0x7e0
#define		flti_subtsuid	0x7e1
#define		flti_multsuid	0x7e2
#define		flti_divtsuid	0x7e3
#define		flti_cvttssuid	0x7ec
#define		flti_cvttqsvid	0x7ef
#define		flti_cvtqssuid	0x7fc
#define		flti_cvtqtsuid	0x7fc

#define op_fltl			0x17
#define		fltl_cvtlq	0x010
#define		fltl_cpys	0x020
#define		fltl_cpysn	0x021
#define		fltl_cpyse	0x022
#define		fltl_mt_fpcr	0x024
#define		fltl_mf_fpcr	0x025
#define		fltl_fcmoveq	0x02a
#define		fltl_fcmovne	0x02b
#define		fltl_fcmovlt	0x02c
#define		fltl_fcmovge	0x02d
#define		fltl_fcmovle	0x02e
#define		fltl_fcmovgt	0x02f
#define		fltl_cvtql	0x030
#define		fltl_cvtqlv	0x130
#define		fltl_cvtqlsv	0x530

#define op_misc			0x18
#define		misc_trapb	0x0000
#define		misc_excb	0x0400
#define		misc_mb		0x4000
#define		misc_wmb	0x4400
#define		misc_fetch	0x8000
#define		misc_fetch_m	0xa000
#define		misc_rpcc	0xc000
#define		misc_rc		0xe000
#define		misc_ecb	0xe800
#define		misc_rs		0xf000
#define		misc_wh64	0xf800

#define op_pal19		0x19
#define op_jsr			0x1a
#define op_pal1b		0x1b
#define op_pal1c		0x1c
#define op_pal1d		0x1d
#define op_pal1e		0x1e
#define op_pal1f		0x1f
#define op_ldf			0x20
#define op_ldg			0x21
#define op_lds			0x22
#define op_ldt			0x23
#define op_stf			0x24
#define op_stg			0x25
#define op_sts			0x26
#define op_stt			0x27
#define op_ldl			0x28
#define op_ldq			0x29
#define op_ldl_l		0x2a
#define op_ldq_l		0x2b
#define op_stl			0x2c
#define op_stq			0x2d
#define op_stl_c		0x2e
#define op_stq_c		0x2f
#define op_br			0x30
#define op_fbeq			0x31
#define op_fblt			0x32
#define op_fble			0x33
#define op_bsr			0x34
#define op_fbne			0x35
#define op_fbge			0x36
#define op_fbgt			0x37
#define op_blbc			0x38
#define op_beq			0x39
#define op_blt			0x3a
#define op_ble			0x3b
#define op_blbs			0x3c
#define op_bne			0x3d
#define op_bge			0x3e
#define op_bgt			0x3f
    } common;
    struct {
	u_int32_t function	: 16;
	u_int32_t rb		: 5;
	u_int32_t ra		: 5;
	u_int32_t opcode	: 6;
    } memory_format;
    struct {
	u_int32_t hint		: 14;
	u_int32_t function	: 2;
#define jsr_jmp			0
#define jsr_jsr			1
#define jsr_ret			2
#define jsr_jsr_coroutine	3
	u_int32_t rb		: 5;
	u_int32_t ra		: 5;
	u_int32_t opcode	: 6;
    } j_format;
    struct {
	int32_t memory_displacement : 16;
	u_int32_t rb		: 5;
	u_int32_t ra		: 5;
	u_int32_t opcode	: 6;
    } m_format;
    struct {
	u_int32_t rc		: 5;
	u_int32_t function	: 7;
	u_int32_t form		: 1;
	u_int32_t sbz		: 3;
	u_int32_t rb		: 5;
	u_int32_t ra		: 5;
	u_int32_t opcode	: 6;
    } o_format;
    struct {
	u_int32_t rc		: 5;
	u_int32_t function	: 7;
	u_int32_t form		: 1;
	u_int32_t literal	: 8;
	u_int32_t ra		: 5;
	u_int32_t opcode	: 6;
    } l_format;
    struct {
	u_int32_t fc		: 5;
	u_int32_t function	: 11;
	u_int32_t fb		: 5;
	u_int32_t fa		: 5;
	u_int32_t opcode	: 6;
    } f_format;
    struct {
	u_int32_t function	: 26;
	u_int32_t opcode	: 6;
    } pal_format;
    struct {
	int32_t branch_displacement : 21;
	u_int32_t ra		: 5;
	u_int32_t opcode	: 6;
    } b_format;
};

#endif /* _MACHINE_INST_H_ */
