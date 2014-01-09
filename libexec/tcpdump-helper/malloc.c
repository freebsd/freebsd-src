/*-
 * Copyright (c) 2012 Robert N. M. Watson
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

#include <sys/param.h>
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#define CTASSERT(x)     _Static_assert(x, "compile-time assertion failed");

static const uint64_t red0 = 0x0123456789ABCDEF;
static const uint64_t red1 = 0xFEDCBA9876543210;

struct minfo {
	uint64_t	red0;
	size_t		size;
	size_t		rsize;
	uint64_t	red1;
};
CTASSERT(sizeof(struct minfo) == 32);

void	*_sb_heapbase;
size_t	 _sb_heaplen;

void *
malloc(size_t size)
{
	size_t rsize;
	struct minfo *minfo;
	char *ptr;

	rsize = roundup2(size, sizeof(minfo));
	if (_sb_heaplen < rsize + sizeof(minfo))
		return (NULL);

	minfo = _sb_heapbase;
	*minfo = (struct minfo) {
		.red0 = red0,
		.size = size,
		.rsize = rsize,
		.red1 = red1
	};
	_sb_heaplen -= sizeof(minfo);
	ptr = (char *)(minfo + 1);
	_sb_heaplen -= rsize;
	_sb_heapbase = ptr + rsize;

	return (ptr);
}

void *
calloc(size_t number, size_t size)
{
	void *ptr;

	ptr = malloc(number * size);
	if (ptr != NULL)
		memset(ptr, 0, number * size);

	return (ptr);
}

void
free(void *ptr)
{
#ifdef CHECK_RZ
	struct minfo *minfo;

	minfo = ptr;
	minfo--;
	if (minfo->red0 != red0 || minfo->red1 != red1)
		abort();
#endif
}

void *
realloc(void *ptr, size_t size)
{
	char *nptr;
	struct minfo *minfo;

	if (ptr == NULL)
		return (malloc(size));

	minfo = ptr;
	minfo--;
#ifdef CHECK_RZ
	if (minfo->red0 != red0 || minfo->red1 != red1)
		abort();
#endif
	
	if (size <= minfo->rsize) {
		minfo->size = size;
		return (ptr);
	}
	
	nptr = malloc(size);
	if (nptr != NULL) {
		memcpy(nptr, ptr, minfo->size);
		free(ptr);
	}

	return (nptr);
}

void *
reallocf(void *ptr, size_t size)
{
	char *nptr;

	nptr = realloc(ptr, size);

	if (nptr == NULL)
		free(ptr);

	return (nptr);
}
