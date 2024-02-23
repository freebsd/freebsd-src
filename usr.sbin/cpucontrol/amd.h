/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006, 2008 Stanislav Sedov <stas@FreeBSD.org>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AMD_H
#define	AMD_H

/*
 * Prototypes.
 */
ucode_probe_t	amd_probe;
ucode_update_t	amd_update;
ucode_probe_t	amd10h_probe;
ucode_update_t	amd10h_update;

typedef struct amd_fw_header {
	uint32_t	date;		/* Update creation date. */
	uint32_t	xz0[2];
	uint32_t	checksum;	/* ucode checksum. */
	uint32_t	xz1[2];
	uint32_t	signature;	/* Low byte of cpuid(0). */
	uint32_t	magic;		/* 0x0Xaaaaaa */
	uint32_t	xz2[8];
} amd_fw_header_t;

#define	AMD_MAGIC	0xaaaaaa

#endif /* !AMD_H */
