/*-
 * Copyright (c) 2011 Marcel Moolenaar
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

#include <stdint.h>

uint32_t __sync_add_and_fetch_4(uint32_t *, uint32_t);
uint32_t __sync_sub_and_fetch_4(uint32_t *, uint32_t);
uint32_t __sync_val_compare_and_swap_4(uint32_t *, uint32_t, uint32_t);

uint32_t
__sync_add_and_fetch_4(uint32_t *ptr, uint32_t val)
{
	*ptr += val;
	return *ptr;
}

uint32_t
__sync_sub_and_fetch_4(uint32_t *ptr, uint32_t val)
{
	*ptr -= val;
	return *ptr;
}

uint32_t
__sync_val_compare_and_swap_4(uint32_t *ptr, uint32_t oldval, uint32_t newval)
{
	uint32_t res = *ptr;
	if (res == oldval)
		*ptr = newval;
	return res;
}
