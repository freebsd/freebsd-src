/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024, Michal Meloun <mmel@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <stdlib.h>
#include <string.h>

#include "thr_private.h"

int __aeabi_idiv(int , int );
unsigned __aeabi_uidiv(unsigned, unsigned );

struct {int q; int r;} __aeabi_idivmod(int, int );
struct {unsigned q; unsigned r;} __aeabi_uidivmod(unsigned, unsigned);

struct {int64_t q; int64_t r;} __aeabi_ldivmod(int64_t, int64_t);
struct {uint64_t q; uint64_t r;} __aeabi_uldivmod(uint64_t, uint64_t);

void __aeabi_memset(void *dest, size_t n, int c);
void __aeabi_memclr(void *dest, size_t n);
void __aeabi_memmove(void *dest, void *src, size_t n);
void __aeabi_memcpy(void *dest, void *src, size_t n);
void __aeabi_memcmp(void *dest, void *src, size_t n);

void
_thr_resolve_machdep(void)
{
	char tmp[2];

	__aeabi_idiv(1, 1);
	__aeabi_uidiv(1, 1);

	__aeabi_idivmod(1, 1);
	__aeabi_uidivmod(1, 1);

	__aeabi_ldivmod(1, 1);
	__aeabi_uldivmod(1, 1);

	__aeabi_memset(tmp, 1, 0);
	__aeabi_memclr(tmp, 1);
	__aeabi_memmove(tmp, tmp + 1, 1);
	__aeabi_memcpy(tmp, tmp + 1, 1);
	__aeabi_memcmp(tmp, tmp + 1, 1);
}
