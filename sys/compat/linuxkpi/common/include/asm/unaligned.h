/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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

#ifndef	_LINUXKPI_ASM_UNALIGNED_H
#define	_LINUXKPI_ASM_UNALIGNED_H

#include <linux/types.h>
#include <asm/byteorder.h>

static __inline uint16_t
get_unaligned_le16(const void *p)
{

	return (le16_to_cpup((const __le16 *)p));
}

static __inline uint32_t
get_unaligned_le32(const void *p)
{

	return (le32_to_cpup((const __le32 *)p));
}

static __inline void
put_unaligned_le32(__le32 v, void *p)
{
	__le32 x;

	x = cpu_to_le32(v);
	memcpy(p, &x, sizeof(x));
}

static __inline void
put_unaligned_le64(__le64 v, void *p)
{
	__le64 x;

	x = cpu_to_le64(v);
	memcpy(p, &x, sizeof(x));
}

static __inline uint16_t
get_unaligned_be16(const void *p)
{

	return (be16_to_cpup((const __be16 *)p));
}

static __inline uint32_t
get_unaligned_be32(const void *p)
{

	return (be32_to_cpup((const __be32 *)p));
}

#endif	/* _LINUXKPI_ASM_UNALIGNED_H */
