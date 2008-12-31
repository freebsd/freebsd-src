/*-
 * Copyright (c) 1997 Michael Smith
 * Copyright (c) 1998 Jonathan Lemon
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
 * $FreeBSD: src/sys/amd64/include/pc/bios.h,v 1.19.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_PC_BIOS_H_
#define _MACHINE_PC_BIOS_H_

extern u_int32_t	bios_sigsearch(u_int32_t start, u_char *sig, int siglen, 
					 int paralen, int sigofs);

#define BIOS_PADDRTOVADDR(x)	((x) + KERNBASE)
#define BIOS_VADDRTOPADDR(x)	((x) - KERNBASE)

/*
 * Int 15:E820 'SMAP' structure
 */

#define SMAP_SIG	0x534D4150			/* 'SMAP' */

#define	SMAP_TYPE_MEMORY	1
#define	SMAP_TYPE_RESERVED	2
#define	SMAP_TYPE_ACPI_RECLAIM	3
#define	SMAP_TYPE_ACPI_NVS	4
#define	SMAP_TYPE_ACPI_ERROR	5

struct bios_smap {
    u_int64_t	base;
    u_int64_t	length;
    u_int32_t	type;
} __packed;

struct bios_oem_signature {
	char * anchor;		/* search anchor string in BIOS memory */
	size_t offset;		/* offset from anchor (may be negative) */
	size_t totlen;		/* total length of BIOS string to copy */
} __packed;
struct bios_oem_range {
	u_int from;		/* shouldn't be below 0xe0000 */
	u_int to;		/* shouldn't be above 0xfffff */
} __packed;
struct bios_oem {
	struct bios_oem_range range;
	struct bios_oem_signature signature[];
} __packed;

extern int
bios_oem_strings(struct bios_oem *oem, u_char *buffer, size_t maxlen);


#endif /* _MACHINE_PC_BIOS_H_ */
