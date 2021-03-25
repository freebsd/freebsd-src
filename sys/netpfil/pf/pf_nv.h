/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
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
 */
#ifndef _PF_NV_H_
#define _PF_NV_H_

#include <sys/types.h>
#include <sys/nv.h>

int	pf_nvbinary(const nvlist_t *, const char *, void *, size_t);
int	pf_nvint(const nvlist_t *, const char *, int *);
int	pf_nvuint8(const nvlist_t *, const char *, uint8_t *);
int	pf_nvuint8_array(const nvlist_t *, const char *, uint8_t *,
	    size_t, size_t *);
void	pf_uint8_array_nv(nvlist_t *, const char *, const uint8_t *,
	    size_t);
int	pf_nvuint16(const nvlist_t *, const char *, uint16_t *);
int	pf_nvuint16_array(const nvlist_t *, const char *, uint16_t *,
	    size_t, size_t *);
void	pf_uint16_array_nv(nvlist_t *, const char *, const uint16_t *,
	    size_t);
int	pf_nvuint32(const nvlist_t *, const char *, uint32_t *);
int	pf_nvuint32_array(const nvlist_t *, const char *, uint32_t *,
	    size_t, size_t *);
void	pf_uint32_array_nv(nvlist_t *, const char *, const uint32_t *,
	    size_t);

int	pf_nvstring(const nvlist_t *, const char *, char *, size_t);

#define	PFNV_CHK(x)	do {	\
	error = (x);		\
	if (error != 0)		\
		goto errout;	\
	} while (0)

#endif
