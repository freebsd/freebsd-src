/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD */

#ifndef UTILS_H
#define UTILS_H

#include <machine/stdarg.h>	/* variable args */

/* TODO optimize of mips, even i & (i-1) is better */

static int __inline__ 
get_set_bit_count64(uint64_t value)
{
	int i, result = 0;

	for (i = 0; i < sizeof(value) * 8; i++)
		if (value & (1ULL << i))
			result++;

	return result;
}

static int __inline__ 
find_first_set_bit64(uint64_t value)
{
	int i;

	for (i = 0; i < sizeof(value) * 8; i++)
		if (value & (1ULL << i))
			return i;

	return -1;
}

static int __inline__ 
find_next_set_bit64(uint64_t value, int pos)
{
	int i;

	for (i = pos + 1; i < sizeof(value) * 8; i++)
		if (value & (1ULL << i))
			return i;

	return -1;
}

/** ---  **/

static int __inline__ 
get_set_bit_count(uint32_t value)
{
	int i, result = 0;

	for (i = 0; i < sizeof(value) * 8; i++)
		if (value & (1U << i))
			result++;

	return result;
}

static int __inline__ 
find_first_set_bit(uint32_t value)
{
	int i;

	for (i = 0; i < sizeof(value) * 8; i++)
		if (value & (1U << i))
			return i;

	return -1;
}

static int __inline__ 
find_next_set_bit(uint32_t value, int pos)
{
	int i;

	for (i = pos + 1; i < sizeof(value) * 8; i++)
		if (value & (1U << i))
			return i;

	return -1;
}

#ifdef DEBUG
void abort();

#define DPUTC(c)         (putchar(c) && fflush(stdout))
#define DPRINT(fmt, ...) printf(fmt "\n", __VA_ARGS__)
#define ASSERT(x)   ((x) || ({ printf("%s failed at (%s:%d)", #x, __FILE__, __LINE__) ; abort(); 0; }) )
#else
#define DPUTC(c)
#define DPRINT(fmt, ...)
#define ASSERT(x)
#endif

void xlr_send_sample(uint32_t tag, uint32_t value, uint32_t ts, uint32_t td);

#endif
