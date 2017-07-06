/*-
 * Copyright (c) 2015 SRI International
 * Copyright (c) 2011-2015 Robert N. M. Watson
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

#ifndef _SYS_CHERI_SERIAL_H_
#define	_SYS_CHERI_SERIAL_H_

/*
 * An expanded and serializable representation of CHERI capabilities.
 * This representation is intended to be valid for all variants
 * supporting up to 64-bit address spaces.
 *
 * If cs_tag is 0 than (cs_storage * 8) bytes of cs_data are the memory
 * contents of the capabilty.  If cs_tag is 1 then cs_type, cs_sealed,
 * cs_perms, cs_length, and cs_offset are valid.  For cs_type and
 * cs_perms, only the least significant cs_typebits and cs_permbits are
 * valid respectivly.
 */

struct cheri_serial {
	union {
		char	cs_data[32];
		struct {
			u_int		cs_type : 24;
			u_int		_cs_spare1 : 8;
			u_int		cs_sealed : 1;
			u_int 		cs_perms : 31;
			uint64_t	cs_base;
			uint64_t	cs_length;
			uint64_t	cs_offset;
		};
	};
	u_int				cs_tag : 1;
	u_int				cs_storage : 3; /* N 64-bit words */
	u_int				cs_typebits : 6;
	u_int				cs_permbits : 6;
	/* XXX-BD: ISA revisions */
	u_int				_cs_spare2 : 16;
	u_int				_ca_spare3;
	uint64_t			_ca_spare4;
};

/*
 * CHERI ISA-defined constants for capabilities -- suitable for inclusion from
 * assembly source code.
 */
#define	CHERI_SERIAL_PERM_GLOBAL		(1 << 0)	/* 0x00000001 */
#define	CHERI_SERIAL_PERM_EXECUTE		(1 << 1)	/* 0x00000002 */
#define	CHERI_SERIAL_PERM_LOAD			(1 << 2)	/* 0x00000004 */
#define	CHERI_SERIAL_PERM_STORE			(1 << 3)	/* 0x00000008 */
#define	CHERI_SERIAL_PERM_LOAD_CAP		(1 << 4)	/* 0x00000010 */
#define	CHERI_SERIAL_PERM_STORE_CAP		(1 << 5)	/* 0x00000020 */
#define	CHERI_SERIAL_PERM_STORE_LOCAL_CAP	(1 << 6)	/* 0x00000040 */
#define	CHERI_SERIAL_PERM_SEAL			(1 << 7)	/* 0x00000080 */
#define	CHERI_SERIAL_PERM_SETTYPE		(1 << 8)	/* 0x00000100 */
#define	CHERI_SERIAL_PERM_RESERVED1		(1 << 9)	/* 0x00000200 */
#define	CHERI_SERIAL_PERM_ACCESS_EPCC		(1 << 10)	/* 0x00000400 */
#define	CHERI_SERIAL_PERM_ACCESS_KDC		(1 << 11)	/* 0x00000800 */
#define	CHERI_SERIAL_PERM_ACCESS_KCC		(1 << 12)	/* 0x00001000 */
#define	CHERI_SERIAL_PERM_ACCESS_KR1C		(1 << 13)	/* 0x00002000 */
#define	CHERI_SERIAL_PERM_ACCESS_KR2C		(1 << 14)	/* 0x00004000 */

/*
 * User-defined permission bits.  The kernel actually snags one for the
 * purposes of authorising system calls from $pcc.  This is a bit of an
 * oddity: normally, we check permissions on data capabilities, not code
 * capabilities, but aligns with 'privilege' checks: e.g., $epcc access.
 */
#define	CHERI_SERIAL_PERM_USER0			(1 << 15)	/* 0x00008000 */
#define	CHERI_SERIAL_PERM_SYSCALL		CHERI_SERIAL_PERM_USER0
#define	CHERI_SERIAL_PERM_USER1			(1 << 16)	/* 0x00010000 */
#define	CHERI_SERIAL_PERM_USER2			(1 << 17)	/* 0x00020000 */
#define	CHERI_SERIAL_PERM_USER3			(1 << 18)	/* 0x00040000 */
#define	CHERI_SERIAL_PERM_USER4			(1 << 19)	/* 0x00080000 */
#define	CHERI_SERIAL_PERM_USER5			(1 << 20)	/* 0x00100000 */
#define	CHERI_SERIAL_PERM_USER6			(1 << 21)	/* 0x00200000 */
#define	CHERI_SERIAL_PERM_USER7			(1 << 22)	/* 0x00400000 */
#define	CHERI_SERIAL_PERM_USER8			(1 << 23)	/* 0x00800000 */
#define	CHERI_SERIAL_PERM_USER9			(1 << 24)	/* 0x01000000 */
#define	CHERI_SERIAL_PERM_USER10		(1 << 25)	/* 0x02000000 */
#define	CHERI_SERIAL_PERM_USER11		(1 << 26)	/* 0x04000000 */
#define	CHERI_SERIAL_PERM_USER12		(1 << 27)	/* 0x08000000 */
#define	CHERI_SERIAL_PERM_USER13		(1 << 28)	/* 0x10000000 */
#define	CHERI_SERIAL_PERM_USER14		(1 << 29)	/* 0x20000000 */
#define	CHERI_SERIAL_PERM_USER15		(1 << 30)	/* 0x40000000 */
#define	CHERI_SERIAL_PERM_USER_PRIVS					\
	(CHERI_SERIAL_PERM_USER0 | CHERI_SERIAL_PERM_USER1 |		\
	CHERI_SERIAL_PERM_USER2 | CHERI_SERIAL_PERM_USER3 |		\
	CHERI_SERIAL_PERM_USER4 | CHERI_SERIAL_PERM_USER5 |		\
	CHERI_SERIAL_PERM_USER6 | CHERI_SERIAL_PERM_USER7 |		\
	CHERI_SERIAL_PERM_USER8 | CHERI_SERIAL_PERM_USER9 |		\
	CHERI_SERIAL_PERM_USER10 | CHERI_SERIAL_PERM_USER11 |		\
	CHERI_SERIAL_PERM_USER12 | CHERI_SERIAL_PERM_USER13 |		\
	CHERI_SERIAL_PERM_USER14 | CHERI_SERIAL_PERM_USER15)

#if defined(_KERNEL) && defined(CPU_CHERI)
struct chericap;

void	cheri_serialize(struct cheri_serial *csp, void * __capability cap);
#endif

#endif /* _SYS_CHERI_SERIAL_H_ */
