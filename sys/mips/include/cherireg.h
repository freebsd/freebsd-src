/*-
 * Copyright (c) 2011-2017 Robert N. M. Watson
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
#define	CHERICAP_SHIFT	4
#else
#define	CHERICAP_SIZE   32
#define	CHERICAP_SHIFT	5
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
#define	CHERI_PERM_SYSTEM_REGS			(1 << 10)	/* 0x00000400 */

/*
 * User-defined permission bits.
 *
 * 256-bit CHERI has a substantially larger number of software-defined
 * permissions.
 */
#define	CHERI256_PERM_SW0			(1 << 15)	/* 0x00008000 */
#define	CHERI256_PERM_SW1			(1 << 16)	/* 0x00010000 */
#define	CHERI256_PERM_SW2			(1 << 17)	/* 0x00020000 */
#define	CHERI256_PERM_SW3			(1 << 18)	/* 0x00040000 */
#define	CHERI256_PERM_SW4			(1 << 19)	/* 0x00080000 */
#define	CHERI256_PERM_SW5			(1 << 20)	/* 0x00100000 */
#define	CHERI256_PERM_SW6			(1 << 21)	/* 0x00200000 */
#define	CHERI256_PERM_SW7			(1 << 22)	/* 0x00400000 */
#define	CHERI256_PERM_SW8			(1 << 23)	/* 0x00800000 */
#define	CHERI256_PERM_SW9			(1 << 24)	/* 0x01000000 */
#define	CHERI256_PERM_SW10			(1 << 25)	/* 0x02000000 */
#define	CHERI256_PERM_SW11			(1 << 26)	/* 0x04000000 */
#define	CHERI256_PERM_SW12			(1 << 27)	/* 0x08000000 */
#define	CHERI256_PERM_SW13			(1 << 28)	/* 0x10000000 */
#define	CHERI256_PERM_SW14			(1 << 29)	/* 0x20000000 */
#define	CHERI256_PERM_SW15			(1 << 30)	/* 0x40000000 */

#define	CHERI128_PERM_SW0			(1 << 15)	/* 0x00008000 */
#define	CHERI128_PERM_SW1			(1 << 16)	/* 0x00010000 */
#define	CHERI128_PERM_SW2			(1 << 17)	/* 0x00020000 */
#define	CHERI128_PERM_SW3			(1 << 18)	/* 0x00040000 */

#if (CHERICAP_SIZE == 32)
#define	CHERI_PERM_SW0		CHERI256_PERM_SW0
#define	CHERI_PERM_SW1		CHERI256_PERM_SW1
#define	CHERI_PERM_SW2		CHERI256_PERM_SW2
#define	CHERI_PERM_SW3		CHERI256_PERM_SW3
#define	CHERI_PERM_SW4		CHERI256_PERM_SW4
#define	CHERI_PERM_SW5		CHERI256_PERM_SW5
#define	CHERI_PERM_SW6		CHERI256_PERM_SW6
#define	CHERI_PERM_SW7		CHERI256_PERM_SW7
#define	CHERI_PERM_SW8		CHERI256_PERM_SW8
#define	CHERI_PERM_SW9		CHERI256_PERM_SW9
#define	CHERI_PERM_SW10		CHERI256_PERM_SW10
#define	CHERI_PERM_SW11		CHERI256_PERM_SW11
#define	CHERI_PERM_SW12		CHERI256_PERM_SW12
#define	CHERI_PERM_SW13		CHERI256_PERM_SW13
#define	CHERI_PERM_SW14		CHERI256_PERM_SW14
#define	CHERI_PERM_SW15		CHERI256_PERM_SW15
#else /* (!(CHERICAP_SIZE == 32)) */
#define	CHERI_PERM_SW0		CHERI128_PERM_SW0
#define	CHERI_PERM_SW1		CHERI128_PERM_SW1
#define	CHERI_PERM_SW2		CHERI128_PERM_SW2
#define	CHERI_PERM_SW3		CHERI128_PERM_SW3
#endif /* (!(CHERICAP_SIZE == 32)) */

/*
 * The kernel snags one for the software-defined permissions for the purposes
 * of authorising system calls from $pcc.  This is a bit of an oddity:
 * normally, we check permissions on data capabilities, not code capabilities,
 * but aligns with 'privilege' checks: e.g., $epcc access.  We may wish to
 * switch to another model, such as having userspace register one or more
 * class capabilities as suitable for system-call use.
 */
#define	CHERI_PERM_SYSCALL			CHERI_PERM_SW0

/*
 * Use another software-defined permission to restrict the ability to change
 * the page mapping underlying a capability.  This can't be the same
 * permission bit as CHERI_PERM_SYSCALL because $pcc should not confer the
 * right rewrite or remap executable memory.
 */
#define	CHERI_PERM_CHERIABI_VMMAP		CHERI_PERM_SW1

/*
 * Macros defining initial permission sets for various scenarios; details
 * depend on the permissions available on 256-bit or 128-bit CHERI:
 *
 * CHERI_PERMS_SWALL: Mask of all available software-defined permissions
 * CHERI_PERMS_HWALL: Mask of all available hardware-defined permissions
 */
#if (CHERICAP_SIZE == 32)
#define	CHERI_PERMS_SWALL						\
	(CHERI_PERM_SW0 | CHERI_PERM_SW1 | CHERI_PERM_SW2 |		\
	CHERI_PERM_SW3 | CHERI_PERM_SW4 | CHERI_PERM_SW5 |		\
	CHERI_PERM_SW6 | CHERI_PERM_SW7 | CHERI_PERM_SW8 |		\
	CHERI_PERM_SW9 | CHERI_PERM_SW10 | CHERI_PERM_SW11 |		\
	CHERI_PERM_SW12 | CHERI_PERM_SW13 | CHERI_PERM_SW14 |		\
	CHERI_PERM_SW15)
#else /* (!(CHERICAP_SIZE == 32)) */
#define	CHERI_PERMS_SWALL						\
	(CHERI_PERM_SW0 | CHERI_PERM_SW1 | CHERI_PERM_SW2 |		\
	CHERI_PERM_SW3)
#endif /* (!(CHERICAP_SIZE == 32)) */

#define	CHERI_PERMS_HWALL						\
	(CHERI_PERM_GLOBAL | CHERI_PERM_EXECUTE |			\
	CHERI_PERM_LOAD | CHERI_PERM_STORE | CHERI_PERM_LOAD_CAP |	\
	CHERI_PERM_STORE_CAP | CHERI_PERM_STORE_LOCAL_CAP |		\
	CHERI_PERM_SEAL | CHERI_PERM_RESERVED0 | CHERI_PERM_RESERVED1 |	\
	CHERI_PERM_SYSTEM_REGS)

/*
 * Root "object-type" capability for the kernel.  This can be used neither as
 * a data nor code capability.
 */
#define	CHERI_PERM_KERN_TYPE	(CHERI_PERM_GLOBAL | CHERI_PERM_SEAL)

/*
 * Basic userspace permission mask; CHERI_PERM_EXECUTE will be added for
 * executable capabilities ($pcc); CHERI_PERM_STORE, CHERI_PERM_STORE_CAP,
 * and CHERI_PERM_STORE_LOCAL_CAP will be added for data permissions ($c0).
 *
 * No variation required between 256-bit and 128-bit CHERI.
 */
#define	CHERI_PERMS_USERSPACE						\
	(CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP |	\
	(CHERI_PERMS_SWALL & ~CHERI_PERM_CHERIABI_VMMAP))

#define	CHERI_PERMS_USERSPACE_CODE					\
	(CHERI_PERMS_USERSPACE | CHERI_PERM_EXECUTE)

#define	CHERI_PERMS_USERSPACE_SEALCAP					\
	(CHERI_PERM_GLOBAL | CHERI_PERM_SEAL)

/*
 * XXX-BD: _DATA should not include _VMMAP, but malloc needs rework to
 * fix.
 */
#define	CHERI_PERMS_USERSPACE_DATA					\
				(CHERI_PERMS_USERSPACE |		\
				CHERI_PERM_STORE |			\
				CHERI_PERM_STORE_CAP |			\
				CHERI_PERM_STORE_LOCAL_CAP |		\
				CHERI_PERM_CHERIABI_VMMAP)

/*
 * Corresponding permission masks for kernel code and data; these are
 * currently a bit broad, and should be narrowed over time as the kernel
 * becomes more capability-aware.
 */
#define	CHERI_PERMS_KERNEL						\
	(CHERI_PERM_GLOBAL | CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP)	\

#define	CHERI_PERMS_KERNEL_CODE						\
	(CHERI_PERMS_KERNEL | CHERI_PERM_EXECUTE)

#define	CHERI_PERMS_KERNEL_DATA						\
	(CHERI_PERMS_KERNEL | CHERI_PERM_STORE | CHERI_PERM_STORE_CAP |	\
	CHERI_PERM_STORE_LOCAL_CAP)

/*
 * The CHERI object-type space is split between userspace and kernel,
 * permitting kernel object references to be delegated to userspace (if
 * desired).  Currently, we provide 23 bits of namespace to each, with the top
 * bit set for kernel object types, but it is easy to imagine other splits.
 * User and kernel software should be written so as to not place assumptions
 * about the specific values used here, as they may change.
 */
#define	CHERI_OTYPE_USER_MIN	(0)
#define	CHERI_OTYPE_USER_MAX	((1 << 23) - 1)
#define	CHERI_OTYPE_KERN_MIN	(1 << 24)
#define	CHERI_OTYPE_KERN_MAX	((1 << 24) - 1)

#define	CHERI_OTYPE_KERN_FLAG	(1 << 23)
#define	CHERI_OTYPE_ISKERN(x)	(((x) & CHERI_OTYPE_KERN_FLAG) != 0)
#define	CHERI_OTYPE_ISUSER(x)	(!(CHERI_OTYPE_ISKERN(x)))

/*
 * When performing a userspace-to-userspace CCall, capability flow-control
 * checks normally prevent local capabilities from being delegated.  This can
 * be disabled on call (but not return) by using an object type with the 22nd
 * bit set -- combined with a suitable selector on the CCall instruction to
 * ensure that this behaviour is intended.
 */
#define	CHERI_OTYPE_LOCALOK_SHIFT	(22)
#define	CHERI_OTYPE_LOCALOK_FLAG	(1 << CHERI_OTYPE_LOCALOK_SHIFT
#define	CHERI_OTYPE_IS_LOCALOK(x)	(((x) & CHERI_OTYPE_LOCALOK_FLAG) != 0)

/*
 * Definition for a highly privileged kernel capability able to name the
 * entire address space, and suitable to derive all other kernel-related
 * capabilities from, including sealing capabilities.
 */
#define	CHERI_CAP_KERN_PERMS						\
	(CHERI_PERMS_SWALL | CHERI_PERMS_HWALL)
#define	CHERI_CAP_KERN_BASE		0x0
#define	CHERI_CAP_KERN_LENGTH		0xffffffffffffffff
#define	CHERI_CAP_KERN_OFFSET		0x0

/*
 * Definition for userspace "unprivileged" capability able to name the user
 * portion of the address space.
 */
#define	CHERI_CAP_USER_CODE_PERMS	CHERI_PERMS_USERSPACE_CODE
#define	CHERI_CAP_USER_CODE_BASE	MIPS_XUSEG_START
#define	CHERI_CAP_USER_CODE_LENGTH	(MIPS_XUSEG_END - MIPS_XUSEG_START)
#define	CHERI_CAP_USER_CODE_OFFSET	0x0

#define	CHERI_CAP_USER_DATA_PERMS	CHERI_PERMS_USERSPACE_DATA
#define	CHERI_CAP_USER_DATA_BASE	MIPS_XUSEG_START
#define	CHERI_CAP_USER_DATA_LENGTH	(MIPS_XUSEG_END - MIPS_XUSEG_START)
#define	CHERI_CAP_USER_DATA_OFFSET	0x0

#define	CHERI_CAP_USER_MMAP_PERMS					\
	(CHERI_PERMS_USERSPACE_DATA | CHERI_PERMS_USERSPACE_CODE |	\
	CHERI_PERM_CHERIABI_VMMAP)
#define	CHERI_CAP_USER_MMAP_BASE	MIPS_XUSEG_START
#define	CHERI_CAP_USER_MMAP_LENGTH	(MIPS_XUSEG_END - MIPS_XUSEG_START)
#define	CHERI_CAP_USER_MMAP_OFFSET	0x0

/*
 * Root sealing capability for all userspace object capabilities.  This is
 * made available to userspace via a sysarch(2).
 */
#define	CHERI_SEALCAP_USERSPACE_PERMS	CHERI_PERMS_USERSPACE_SEALCAP
#define	CHERI_SEALCAP_USERSPACE_BASE	CHERI_OTYPE_USER_MIN
#define	CHERI_SEALCAP_USERSPACE_LENGTH	\
    (CHERI_OTYPE_USER_MAX - CHERI_OTYPE_USER_MIN + 1)
#define	CHERI_SEALCAP_USERSPACE_OFFSET	0x0

/*
 * A blend of hardware and software allocation of capability registers.
 * Ideally, this list wouldn't exist here, but be purely in the assembler.
 */
#define	CHERI_CR_C0	0	/*   MIPS fetch/load/store capability. */
#define	CHERI_CR_DDC	CHERI_CR_C0
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
#define	CHERI_CR_STC	CHERI_CR_C11
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
#define	CHERI_CR_C24	24
#define	CHERI_CR_C25	25
#define	CHERI_CR_C26	26
#define	CHERI_CR_IDC	CHERI_CR_C26
#define	CHERI_CR_C27	27
#define	CHERI_CR_KR1C	CHERI_CR_C27
#define	CHERI_CR_C28	28
#define	CHERI_CR_KR2C	CHERI_CR_C28
#define	CHERI_CR_C29	29
#define	CHERI_CR_KCC	CHERI_CR_C29
#define	CHERI_CR_C30	30
#define	CHERI_CR_KDC	CHERI_CR_C30
#define	CHERI_CR_C31	31
#define	CHERI_CR_EPCC	CHERI_CR_C31

#define	CHERI_CR_CTEMP0	CHERI_CR_C13	/* C capability manipulation. */
#define	CHERI_CR_CTEMP1	CHERI_CR_C14	/* C capability manipulation. */
#define	CHERI_CR_SEC0	CHERI_CR_KR2C	/* Saved $c0 in exception handler. */

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
#define	CHERIKFRAME_OFF_C24	7

/*
 * List of CHERI capability cause code constants, which are used to
 * characterise various CP2 exceptions.
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
#define	CHERI_EXCCODE_PERM_USER		CHERI_EXCCODE_USER_PERM
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
#define	CHERI_EXCCODE_SYSTEM_REGS	0x18
#define	_CHERI_EXCCODE_RESERVED19	0x19
#define	_CHERI_EXCCODE_RESERVED1a	0x1a
#define	_CHERI_EXCCODE_RESERVED1b	0x1b
#define	_CHERI_EXCCODE_RESERVED1c	0x1c
#define	_CHERI_EXCCODE_RESERVED1d	0x1d
#define	_CHERI_EXCCODE_RESERVED1e	0x1e
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

#if CHERICAP_SIZE == 32
#define	CHERI_ALIGN_SHIFT(l)	0ULL
#define	CHERI_SEAL_ALIGN_SHIFT(l)	0ULL
#else /* (!(CHERICAP_SIZE == 32)) */

#define	CHERI_BASELEN_BITS	20
#define	CHERI_SEAL_BASELEN_BITS	5
#define	CHERI_SLOP_BITS		2
#define	CHERI_ADDR_BITS		64
#define	CHERI_SEAL_MIN_ALIGN	12

/*
 * Use __builtin_clzll() to implement flsll() on clang where emission of
 * DCLZ instructions is correctly conditionalized.
 */
#ifdef __clang__
#define	_flsll(x)	(64 - __builtin_clzll(x))
#else
#define	_flsll(x)	flsll(x)
#endif
#define	CHERI_ALIGN_SHIFT(l)						\
    ((_flsll(l) <= (CHERI_BASELEN_BITS - CHERI_SLOP_BITS)) ? 0ULL :	\
    (_flsll(l) - (CHERI_BASELEN_BITS - CHERI_SLOP_BITS)))
#define	_CHERI_SEAL_ALIGN_SHIFT(l)					\
    ((_flsll(l) <= (CHERI_SEAL_BASELEN_BITS)) ? 0ULL :	\
    (_flsll(l) - (CHERI_SEAL_BASELEN_BITS)))
#define CHERI_SEAL_ALIGN_SHIFT(l)					\
    (_CHERI_SEAL_ALIGN_SHIFT(l) < CHERI_SEAL_MIN_ALIGN ?		\
     CHERI_SEAL_MIN_ALIGN : _CHERI_SEAL_ALIGN_SHIFT(l))
#endif /* (!(CHERICAP_SIZE == 32)) */

#define	CHERI_ALIGN_MASK(l)		~(~0ULL << CHERI_ALIGN_SHIFT(l))
#define	CHERI_SEAL_ALIGN_MASK(l)	~(~0ULL << CHERI_SEAL_ALIGN_SHIFT(l))

#endif /* _MIPS_INCLUDE_CHERIREG_H_ */
