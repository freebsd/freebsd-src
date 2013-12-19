/*-
 * Copyright (c) 2011-2013 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#ifndef _MIPS_INCLUDE_CHERIREG_H_
#define	_MIPS_INCLUDE_CHERIREG_H_

/*
 * CHERI ISA-defined constants for capabilities -- suitable for inclusion from
 * assembly source code.
 *
 * XXXRW: CHERI_UNSEALED is not currently considered part of the perms word,
 * but perhaps it should be.
 */
#define	CHERI_PERM_NON_EPHEMERAL		0x0001
#define	CHERI_PERM_EXECUTE			0x0002
#define	CHERI_PERM_LOAD				0x0004
#define	CHERI_PERM_STORE			0x0008
#define	CHERI_PERM_LOAD_CAP			0x0010
#define	CHERI_PERM_STORE_CAP			0x0020
#define	CHERI_PERM_STORE_EPHEM_CAP		0x0040
#define	CHERI_PERM_SEAL				0x0080
#define	CHERI_PERM_SETTYPE			0x0100
#define	CHERI_PERM_RESERVED1			0x0200
#define	CHERI_PERM_ACCESS_EPCC			0x0400
#define	CHERI_PERM_ACCESS_KDC			0x0800
#define	CHERI_PERM_ACCESS_KCC			0x1000
#define	CHERI_PERM_ACCESS_KR1C			0x2000
#define	CHERI_PERM_ACCESS_KR2C			0x4000

#define	CHERI_PERM_PRIV							\
	(CHERI_PERM_NON_EPHEMERAL | CHERI_PERM_EXECUTE |		\
	CHERI_PERM_LOAD | CHERI_PERM_STORE | CHERI_PERM_LOAD_CAP |	\
	CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_EPHEM_CAP |		\
	CHERI_PERM_SEAL | CHERI_PERM_SETTYPE | CHERI_PERM_RESERVED1 |	\
	CHERI_PERM_ACCESS_EPCC | CHERI_PERM_ACCESS_KDC |		\
	CHERI_PERM_ACCESS_KCC | CHERI_PERM_ACCESS_KR1C |		\
	CHERI_PERM_ACCESS_KR2C)

#define	CHERI_PERM_USER							\
	(CHERI_PERM_NON_EPHEMERAL | CHERI_PERM_EXECUTE |		\
	CHERI_PERM_LOAD | CHERI_PERM_STORE | CHERI_PERM_LOAD_CAP |	\
	CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_EPHEM_CAP |		\
	CHERI_PERM_SEAL | CHERI_PERM_SETTYPE)

/*
 * Definition for kernel "privileged" capability able to name the entire
 * address space.
 */
#define	CHERI_CAP_PRIV_PERMS		CHERI_PERM_PRIV
#define	CHERI_CAP_PRIV_OTYPE		0x0
#define	CHERI_CAP_PRIV_BASE		0x0
#define	CHERI_CAP_PRIV_LENGTH		0xffffffffffffffff

/*
 * Definition for userspace "unprivileged" capability able to name the user
 * portion of the address space.
 */
#define	CHERI_CAP_USER_PERMS		CHERI_PERM_USER
#define	CHERI_CAP_USER_OTYPE		0x0
#define	CHERI_CAP_USER_BASE		MIPS_XUSEG_START
#define	CHERI_CAP_USER_LENGTH		(MIPS_XUSEG_END - MIPS_XUSEG_START)

/*
 * Definition for capability unable to name any resources.  This is suitable
 * for filling capability registers that should hold no privilege.
 *
 * XXXRW: Probably no longer required in CHERI ISAv2 as we can clear
 * registers.
 */
#define	CHERI_CAP_NOPRIV_PERMS		0x0
#define	CHERI_CAP_NOPRIV_OTYPE		0x0
#define	CHERI_CAP_NOPRIV_BASE		0x0
#define	CHERI_CAP_NOPRIV_LENGTH		0x0

/*
 * A blend of hardware and software allocation of capability registers.
 * Ideally, this list wouldn't exist here, but be purely in the assembler.
 */
#define	CHERI_CR_C0	0	/*   MIPS fetch/load/store capability. */
#define	CHERI_CR_C1	1
#define	CHERI_CR_C2	2
#define	CHERI_CR_C3	3
#define	CHERI_CR_C4	4
#define	CHERI_CR_C5	5
#define	CHERI_CR_C6	6
#define	CHERI_CR_C7	7
#define	CHERI_CR_C8	8
#define	CHERI_CR_C9	9
#define	CHERI_CR_C10	10
#define	CHERI_CR_C11	11
#define	CHERI_CR_C12	12
#define	CHERI_CR_C13	13
#define	CHERI_CR_C14	14
#define	CHERI_CR_C15	15
#define	CHERI_CR_C16	16
#define	CHERI_CR_C17	17
#define	CHERI_CR_C18	18
#define	CHERI_CR_C19	19
#define	CHERI_CR_C20	20
#define	CHERI_CR_C21	21
#define	CHERI_CR_C22	22
#define	CHERI_CR_C23	23
#define	CHERI_CR_RCC	24
#define	CHERI_CR_C25	25
#define	CHERI_CR_IDC	26
#define	CHERI_CR_KR1C	27
#define	CHERI_CR_KR2C	28
#define	CHERI_CR_KCC	29
#define	CHERI_CR_KDC	30
#define	CHERI_CR_EPCC	31

#define	CHERI_CR_CTEMP	CHERI_CR_KR1C	/* C-language temporary. */
#define	CHERI_CR_SEC0	CHERI_CR_KR2C	/* Saved $c0 in exception handler. */

/*
 * Offsets of registers in struct cheri_frame -- must match the definition in
 * cheri.h.
 */
#define	CHERI_CR_C0_OFF		0
#define	CHERI_CR_C1_OFF		1
#define	CHERI_CR_C2_OFF		2
#define	CHERI_CR_C3_OFF		3
#define	CHERI_CR_C4_OFF		4
#define	CHERI_CR_C5_OFF		5
#define	CHERI_CR_C6_OFF		6
#define	CHERI_CR_C7_OFF		7
#define	CHERI_CR_C8_OFF		8
#define	CHERI_CR_C9_OFF		9
#define	CHERI_CR_C10_OFF	10
#define	CHERI_CR_C11_OFF	11
#define	CHERI_CR_C12_OFF	12
#define	CHERI_CR_C13_OFF	13
#define	CHERI_CR_C14_OFF	14
#define	CHERI_CR_C15_OFF	15
#define	CHERI_CR_C16_OFF	16
#define	CHERI_CR_C17_OFF	17
#define	CHERI_CR_C18_OFF	18
#define	CHERI_CR_C19_OFF	19
#define	CHERI_CR_C20_OFF	20
#define	CHERI_CR_C21_OFF	21
#define	CHERI_CR_C22_OFF	22
#define	CHERI_CR_C23_OFF	23
#define	CHERI_CR_RCC_OFF	24
#define	CHERI_CR_C25_OFF	25
#define	CHERI_CR_IDC_OFF	26
#define	CHERI_CR_PCC_OFF	27	/* NB: Not register $c27! */

/*
 * List of CHERI capability cause code constants, which are used to
 * disambiguate various CP2 exceptions.
 *
 * XXXRW: I wonder if we really need different permissions for each exception-
 * handling capability.
 *
 * XXXRW: Curiously non-contiguous.
 *
 * XXXRW: KDC is listed as 0x1a in the spec, which collides with EPCC.  Not
 * sure what is actually used.
 */
#define	CHERI_EXCCODE_NONE		0x00
#define	CHERI_EXCCODE_LENGTH		0x01
#define	CHERI_EXCCODE_TAG		0x02
#define	CHERI_EXCCODE_SEAL		0x03
#define	CHERI_EXCCODE_TYPE		0x04
#define	CHERI_EXCCODE_CALL		0x05
#define	CHERI_EXCCODE_RETURN		0x06
#define	CHERI_EXCCODE_UNDERFLOW		0x07
#define	CHERI_EXCCODE_NON_EPHEM		0x10
#define	CHERI_EXCCODE_PERM_EXECUTE	0x11
#define	CHERI_EXCCODE_PERM_LOAD		0x12
#define	CHERI_EXCCODE_PERM_STORE	0x13
#define	CHERI_EXCCODE_PERM_LOADCAP	0x14
#define	CHERI_EXCCODE_PERM_STORECAP	0x15
#define	CHERI_EXCCODE_STORE_EPHEM	0x16
#define	CHERI_EXCCODE_PERM_SEAL		0x17
#define	CHERI_EXCCODE_PERM_SETTYPE	0x18
#define	CHERI_EXCCODE_ACCESS_EPCC	0x1a
#define	CHERI_EXCCODE_ACCESS_KDC	0x1b	/* XXXRW */
#define	CHERI_EXCCODE_ACCESS_KCC	0x1c
#define	CHERI_EXCCODE_ACCESS_KR1C	0x1d
#define	CHERI_EXCCODE_ACCESS_KR2C	0x1e

/*
 * Location of the CHERI CCall/CReturn software-path exception vector.
 */
#define	CHERI_CCALL_EXC_VEC	((intptr_t)(int32_t)0x80000280)

#endif /* _MIPS_INCLUDE_CHERIREG_H_ */
