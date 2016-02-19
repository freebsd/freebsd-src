/*-
 * Copyright (c) 2011-2016 Robert N. M. Watson
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
 * The size of in-memory capabilities in bytes; minimum alignment is also
 * assumed to be this size.
 */
#if defined(_MIPS_SZCAP) && (_MIPS_SZCAP != 128) && (_MIPS_SZCAP != 256)
#error "_MIPS_SZCAP defined but neither 128 nor 256"
#endif

#if defined(CPU_CHERI128) || (defined(_MIPS_SZCAP) && (_MIPS_SZCAP == 128))
#define	CHERICAP_SIZE   16
#else
#define	CHERICAP_SIZE   32
#endif

/*
 * CHERI ISA-defined constants for capabilities -- suitable for inclusion from
 * assembly source code.
 *
 * XXXRW: CHERI_UNSEALED is not currently considered part of the perms word,
 * but perhaps it should be.
 */
#define	CHERI_PERM_GLOBAL			(1 << 0)	/* 0x00000001 */
#define	CHERI_PERM_EXECUTE			(1 << 1)	/* 0x00000002 */
#define	CHERI_PERM_LOAD				(1 << 2)	/* 0x00000004 */
#define	CHERI_PERM_STORE			(1 << 3)	/* 0x00000008 */
#define	CHERI_PERM_LOAD_CAP			(1 << 4)	/* 0x00000010 */
#define	CHERI_PERM_STORE_CAP			(1 << 5)	/* 0x00000020 */
#define	CHERI_PERM_STORE_LOCAL_CAP		(1 << 6)	/* 0x00000040 */
#define	CHERI_PERM_SEAL				(1 << 7)	/* 0x00000080 */
#define	CHERI_PERM_RESERVED0			(1 << 8)	/* 0x00000100 */
#define	CHERI_PERM_RESERVED1			(1 << 9)	/* 0x00000200 */

/*
 * 256-bit CHERI has multiple exception-handling permissions, whereas 128-bit
 * CHERI has a single exception-handling permission.
 */
#if (CHERICAP_SIZE == 32)
#define	CHERI_PERM_ACCESS_EPCC			(1 << 10)	/* 0x00000400 */
#define	CHERI_PERM_ACCESS_KDC			(1 << 11)	/* 0x00000800 */
#define	CHERI_PERM_ACCESS_KCC			(1 << 12)	/* 0x00001000 */
#define	CHERI_PERM_ACCESS_KR1C			(1 << 13)	/* 0x00002000 */
#define	CHERI_PERM_ACCESS_KR2C			(1 << 14)	/* 0x00004000 */
#else /* (!(CHERICAP_SIZE == 32)) */
#define	CHERI_PERM_ACCESS_SYSTEM_REGISTERS	(1 << 10)	/* 0x00000400 */
#endif /* (!(CHERICAP_SIZE == 32)) */

/*
 * User-defined permission bits.
 *
 * 256-bit CHERI has a substantially larger number of user-defined
 * permissions.
 */
#if (CHERICAP_SIZE == 32)
#define	CHERI_PERM_USER0			(1 << 15)	/* 0x00008000 */
#define	CHERI_PERM_USER1			(1 << 16)	/* 0x00010000 */
#define	CHERI_PERM_USER2			(1 << 17)	/* 0x00020000 */
#define	CHERI_PERM_USER3			(1 << 18)	/* 0x00040000 */
#define	CHERI_PERM_USER4			(1 << 19)	/* 0x00080000 */
#define	CHERI_PERM_USER5			(1 << 20)	/* 0x00100000 */
#define	CHERI_PERM_USER6			(1 << 21)	/* 0x00200000 */
#define	CHERI_PERM_USER7			(1 << 22)	/* 0x00400000 */
#define	CHERI_PERM_USER8			(1 << 23)	/* 0x00800000 */
#define	CHERI_PERM_USER9			(1 << 24)	/* 0x01000000 */
#define	CHERI_PERM_USER10			(1 << 25)	/* 0x02000000 */
#define	CHERI_PERM_USER11			(1 << 26)	/* 0x04000000 */
#define	CHERI_PERM_USER12			(1 << 27)	/* 0x08000000 */
#define	CHERI_PERM_USER13			(1 << 28)	/* 0x10000000 */
#define	CHERI_PERM_USER14			(1 << 29)	/* 0x20000000 */
#define	CHERI_PERM_USER15			(1 << 30)	/* 0x40000000 */
#else /* (!(CHERICAP_SIZE == 32)) */
#define	CHERI_PERM_USER0			(1 << 15)	/* 0x00008000 */
#define	CHERI_PERM_USER1			(1 << 16)	/* 0x00010000 */
#define	CHERI_PERM_USER2			(1 << 17)	/* 0x00020000 */
#define	CHERI_PERM_USER3			(1 << 18)	/* 0x00040000 */
#endif /* (!(CHERICAP_SIZE == 32)) */

/*
 * The kernel snags one for the user-defined permissions for the purposes of
 * authorising system calls from $pcc.  This is a bit of an oddity: normally,
 * we check permissions on data capabilities, not code capabilities, but
 * aligns with 'privilege' checks: e.g., $epcc access.  We may wish to switch
 * to another model, such as having userspace register one or more class
 * capabilities as suitable for privilege.
 */
#define	CHERI_PERM_SYSCALL			CHERI_PERM_USER0

/*
 * Macros defining initial permission sets for various scenarios; details
 * depend on the permissions available on 256-bit or 128-bit CHERI:
 *
 * CHERI_PERM_USER_PRIVS: Mask of all available user-defined permissions
 * CHERI_PERM_PRIV: Mask of all available hardware-defined permissions
 */
#if (CHERICAP_SIZE == 32)
#define	CHERI_PERM_USER_PRIVS						\
	(CHERI_PERM_USER0 | CHERI_PERM_USER1 | CHERI_PERM_USER2 |	\
	CHERI_PERM_USER3 | CHERI_PERM_USER4 | CHERI_PERM_USER5 |	\
	CHERI_PERM_USER6 | CHERI_PERM_USER7 | CHERI_PERM_USER8 |	\
	CHERI_PERM_USER9 | CHERI_PERM_USER10 | CHERI_PERM_USER11 |	\
	CHERI_PERM_USER12 | CHERI_PERM_USER13 | CHERI_PERM_USER14 |	\
	CHERI_PERM_USER15)

#define	CHERI_PERM_PRIV							\
	(CHERI_PERM_GLOBAL | CHERI_PERM_EXECUTE |			\
	CHERI_PERM_LOAD | CHERI_PERM_STORE | CHERI_PERM_LOAD_CAP |	\
	CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_LOCAL_CAP |		\
	CHERI_PERM_SEAL | CHERI_PERM_RESERVED0 | CHERI_PERM_RESERVED1 |	\
	CHERI_PERM_ACCESS_EPCC | CHERI_PERM_ACCESS_KDC |		\
	CHERI_PERM_ACCESS_KCC | CHERI_PERM_ACCESS_KR1C |		\
	CHERI_PERM_ACCESS_KR2C | CHERI_PERM_USER_PRIVS)
#else /* (!(CHERICAP_SIZE == 32)) */
#define	CHERI_PERM_USER_PRIVS						\
	(CHERI_PERM_USER0 | CHERI_PERM_USER1 | CHERI_PERM_USER2 |	\
	CHERI_PERM_USER3)

#define	CHERI_PERM_PRIV							\
	(CHERI_PERM_GLOBAL | CHERI_PERM_EXECUTE |			\
	CHERI_PERM_LOAD | CHERI_PERM_STORE | CHERI_PERM_LOAD_CAP |	\
	CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_LOCAL_CAP |		\
	CHERI_PERM_SEAL | CHERI_PERM_RESERVED0 | CHERI_PERM_RESERVED1)
#endif /* (!(CHERICAP_SIZE == 32)) */

/*
 * Basic userspace permission mask; CHERI_PERM_EXECUTE will be added for
 * executable capabilities ($pcc); CHERI_PERM_STORE, CHERI_PERM_STORE_CAP,
 * and CHERI_PERM_STORE_LOCAL_CAP will be added for data permissions ($c0).
 *
 * No variation required between 256-bit and 128-bit CHERI.
 */
#define	CHERI_PERM_USER							\
	(CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP |	\
	CHERI_PERM_USER_PRIVS)

#define	CHERI_PERM_USER_CODE	(CHERI_PERM_USER | CHERI_PERM_EXECUTE)
#define	CHERI_PERM_USER_DATA	(CHERI_PERM_USER | CHERI_PERM_STORE |	\
				CHERI_PERM_STORE_CAP |			\
				CHERI_PERM_STORE_LOCAL_CAP)

/*
 * Root "object-type" capability -- queried via sysarch(2) when libcheri needs
 * to allocate types.  This can be used neither as a data nor code capability.
 *
 * No variation required between 256-bit and 128-bit CHERI.
 */
#define	CHERI_PERM_USER_TYPE	(CHERI_PERM_GLOBAL | CHERI_PERM_SEAL)

/*
 * Definition for kernel "privileged" capability able to name the entire
 * address space.
 *
 * No variation required between 256-bit and 128-bit CHERI.
 */
#define	CHERI_CAP_PRIV_PERMS		CHERI_PERM_PRIV
#define	CHERI_CAP_PRIV_OTYPE		0x0
#define	CHERI_CAP_PRIV_BASE		0x0
#define	CHERI_CAP_PRIV_LENGTH		0xffffffffffffffff
#define	CHERI_CAP_PRIV_OFFSET		0x0

/*
 * Definition for userspace "unprivileged" capability able to name the user
 * portion of the address space.
 *
 * No variation required between 256-bit and 128-bit CHERI.
 */
#define	CHERI_CAP_USER_CODE_PERMS	CHERI_PERM_USER_CODE
#define	CHERI_CAP_USER_CODE_OTYPE	0x0
#define	CHERI_CAP_USER_CODE_BASE	MIPS_XUSEG_START
#define	CHERI_CAP_USER_CODE_LENGTH	(MIPS_XUSEG_END - MIPS_XUSEG_START)
#define	CHERI_CAP_USER_CODE_OFFSET	0x0

#define	CHERI_CAP_USER_DATA_PERMS	CHERI_PERM_USER_DATA
#define	CHERI_CAP_USER_DATA_OTYPE	0x0
#define	CHERI_CAP_USER_DATA_BASE	MIPS_XUSEG_START
#define	CHERI_CAP_USER_DATA_LENGTH	(MIPS_XUSEG_END - MIPS_XUSEG_START)
#define	CHERI_CAP_USER_DATA_OFFSET	0x0

#define	CHERI_CAP_USER_TYPE_PERMS	CHERI_PERM_USER_TYPE
#define	CHERI_CAP_USER_TYPE_OTYPE	0x0
#define	CHERI_CAP_USER_TYPE_BASE	MIPS_XUSEG_START
#define	CHERI_CAP_USER_TYPE_LENGTH	(MIPS_XUSEG_END - MIPS_XUSEG_START)
#define	CHERI_CAP_USER_TYPE_OFFSET	0x0

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

#define	CHERI_CR_CTEMP0	CHERI_CR_C11	/* C capability manipulation. */
#define	CHERI_CR_CTEMP1	CHERI_CR_C12	/* C capability manipulation. */
#define	CHERI_CR_SEC0	CHERI_CR_KR2C	/* Saved $c0 in exception handler. */

/*
 * Offsets of registers in struct cheri_frame when treated as an array of
 * capabilities -- must match the definition in cheri.h.
 */
#define	CHERIFRAME_OFF_C0	0
#define	CHERIFRAME_OFF_C1	1
#define	CHERIFRAME_OFF_C2	2
#define	CHERIFRAME_OFF_C3	3
#define	CHERIFRAME_OFF_C4	4
#define	CHERIFRAME_OFF_C5	5
#define	CHERIFRAME_OFF_C6	6
#define	CHERIFRAME_OFF_C7	7
#define	CHERIFRAME_OFF_C8	8
#define	CHERIFRAME_OFF_C9	9
#define	CHERIFRAME_OFF_C10	10
#define	CHERIFRAME_OFF_C11	11
#define	CHERIFRAME_OFF_C12	12
#define	CHERIFRAME_OFF_C13	13
#define	CHERIFRAME_OFF_C14	14
#define	CHERIFRAME_OFF_C15	15
#define	CHERIFRAME_OFF_C16	16
#define	CHERIFRAME_OFF_C17	17
#define	CHERIFRAME_OFF_C18	18
#define	CHERIFRAME_OFF_C19	19
#define	CHERIFRAME_OFF_C20	20
#define	CHERIFRAME_OFF_C21	21
#define	CHERIFRAME_OFF_C22	22
#define	CHERIFRAME_OFF_C23	23
#define	CHERIFRAME_OFF_RCC	24
#define	CHERIFRAME_OFF_C25	25
#define	CHERIFRAME_OFF_IDC	26
#define	CHERIFRAME_OFF_PCC	27	/* NB: Not register $c27! */

/*
 * Offset of the capability cause register in struct cheri_kframe -- must
 * match the definition in cheri.h.  Note that although this constant is sized
 * based on capabilities, in fact the cause register is 64-bit.  We may want
 * to revisit this if we add more 64-bit values.
 */
#define	CHERIFRAME_OFF_CAPCAUSE	28

/*
 * Offsets of registers in struct cheri_kframe -- must match the definition in
 * cheri.h.
 */
#define	CHERIKFRAME_OFF_C17	0
#define	CHERIKFRAME_OFF_C18	1
#define	CHERIKFRAME_OFF_C19	2
#define	CHERIKFRAME_OFF_C20	3
#define	CHERIKFRAME_OFF_C21	4
#define	CHERIKFRAME_OFF_C22	5
#define	CHERIKFRAME_OFF_C23	6
#define	CHERIKFRAME_OFF_RCC	7

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
#define	CHERI_EXCCODE_USER_PERM		0x08
#define	CHERI_EXCCODE_TLBSTORE		0x09
#define	CHERI_EXCCODE_IMPRECISE		0x0a
#define	_CHERI_EXCCODE_RESERVED0b	0x0b
#define	_CHERI_EXCCODE_RESERVED0c	0x0c
#define	_CHERI_EXCCODE_RESERVED0d	0x0d
#define	_CHERI_EXCCODE_RESERVED0e	0x0e
#define	_CHERI_EXCCODE_RESERVED0f	0x0f
#define	CHERI_EXCCODE_GLOBAL		0x10
#define	CHERI_EXCCODE_PERM_EXECUTE	0x11
#define	CHERI_EXCCODE_PERM_LOAD		0x12
#define	CHERI_EXCCODE_PERM_STORE	0x13
#define	CHERI_EXCCODE_PERM_LOADCAP	0x14
#define	CHERI_EXCCODE_PERM_STORECAP	0x15
#define	CHERI_EXCCODE_STORE_LOCALCAP	0x16
#define	CHERI_EXCCODE_PERM_SEAL		0x17
#define	_CHERI_EXCCODE_RESERVED18	0x18
#define	_CHERI_EXCCODE_RESERVED19	0x19
#define	CHERI_EXCCODE_ACCESS_EPCC	0x1a
#define	CHERI_EXCCODE_ACCESS_KDC	0x1b	/* XXXRW */
#define	CHERI_EXCCODE_ACCESS_KCC	0x1c
#define	CHERI_EXCCODE_ACCESS_KR1C	0x1d
#define	CHERI_EXCCODE_ACCESS_KR2C	0x1e
#define	_CHERI_EXCCODE_RESERVED1f	0x1f

/*
 * User-defined CHERI exception codes are numbered 128...255.
 */
#define	CHERI_EXCCODE_SW_BASE		0x80
#define	CHERI_EXCCODE_SW_LOCALARG	0x80	/* Non-global CCall argument. */
#define	CHERI_EXCCODE_SW_LOCALRET	0x81	/* Non-global CReturn value. */
#define	CHERI_EXCCODE_SW_CCALLREGS	0x82	/* Incorrect CCall registers. */

/*
 * How to turn the cause register into an exception code and register number.
 */
#define	CHERI_CAPCAUSE_EXCCODE_MASK	0xff00
#define	CHERI_CAPCAUSE_EXCCODE_SHIFT	8
#define	CHERI_CAPCAUSE_REGNUM_MASK	0xff

/*
 * Location of the CHERI CCall/CReturn software-path exception vector.
 */
#define	CHERI_CCALL_EXC_VEC	((intptr_t)(int32_t)0x80000280)

#endif /* _MIPS_INCLUDE_CHERIREG_H_ */
