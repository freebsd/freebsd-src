/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021,2023 The FreeBSD Foundation
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
 */

#ifndef	_LINUXKPI_LINUX_UUID_H
#define	_LINUXKPI_LINUX_UUID_H

#include <linux/random.h>

#define	UUID_STRING_LEN	36

#define	GUID_INIT(x0_3, x4_5, x6_7, x8, x9, x10, x11, x12, x13, x14, x15) \
	((guid_t) { .x = { 						\
		[0]  =  (x0_3)        & 0xff,				\
		[1]  = ((x0_3) >> 8)  & 0xff,				\
		[2]  = ((x0_3) >> 16) & 0xff,				\
		[3]  = ((x0_3) >> 24) & 0xff,				\
		[4]  =  (x4_5)        & 0xff,				\
		[5]  = ((x4_5) >> 8)  & 0xff,				\
		[6]  =  (x6_7)        & 0xff,				\
		[7]  = ((x6_7) >> 8)  & 0xff,				\
		[8]  =  (x8),						\
		[9]  =  (x9),						\
		[10] =  (x10),						\
		[11] =  (x11),						\
		[12] =  (x12),						\
		[13] =  (x13),						\
		[14] =  (x14),						\
		[15] =  (x15)						\
}})

typedef struct {
	char	x[16];
} guid_t;

static inline void
guid_gen(guid_t *g)
{

	get_random_bytes(g, 16);
	g->x[7] = (g->x[7] & 0x0f) | 0x40;
	g->x[8] = (g->x[8] & 0x3f) | 0x80;
}

static inline void
guid_copy(guid_t *dst, const guid_t *src)
{
	memcpy(dst, src, sizeof(*dst));
}

#endif	/* _LINUXKPI_LINUX_UUID_H */
