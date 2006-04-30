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
 * $FreeBSD$
 */

#ifndef _MACHINE_PC_BIOS_H_
#define _MACHINE_PC_BIOS_H_

extern u_int32_t	bios_sigsearch(u_int32_t start, u_char *sig, int siglen, 
					 int paralen, int sigofs);

#define BIOS_PADDRTOVADDR(x)	((x) + KERNBASE)
#define BIOS_VADDRTOPADDR(x)	((x) - KERNBASE)

/*
 * Int 15:E820 'SMAP' structure
 *
 * XXX add constants for type
 */
#define SMAP_SIG	0x534D4150			/* 'SMAP' */
struct bios_smap {
    u_int64_t	base;
    u_int64_t	length;
    u_int32_t	type;
} __packed;

const u_char *bios_string(u_int from, u_int to, const u_char *string, int len);


#endif /* _MACHINE_PC_BIOS_H_ */
