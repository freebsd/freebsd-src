/*
 * This module derived from code donated to the FreeBSD Project by 
 * Matthew Dillon <dillon@backplane.com>
 *
 * Copyright (c) 1998 The FreeBSD Project
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
 * $FreeBSD: src/lib/libstand/zalloc_malloc.c,v 1.5 1999/08/28 00:05:35 peter Exp $
 */

/*
 * MALLOC.C - malloc equivalent, runs on top of zalloc and uses sbrk
 */

#include "zalloc_defs.h"

static MemPool	MallocPool;

#ifdef DMALLOCDEBUG
static int MallocMax;
static int MallocCount;

void mallocstats(void);
#endif

#ifdef malloc
#undef malloc
#undef free
#endif

#ifdef __alpha__
void
free_region(void *start, void *end)
{
    zextendPool(&MallocPool, start, (caddr_t)end - (caddr_t)start);
    zfree(&MallocPool, start, (caddr_t)end - (caddr_t)start);
}
#endif

void *
malloc(size_t bytes)
{
    Guard *res;

#ifdef USEENDGUARD
    bytes += MALLOCALIGN + 1;
#else
    bytes += MALLOCALIGN;
#endif

    while ((res = znalloc(&MallocPool, bytes)) == NULL) {
	int incr = (bytes + BLKEXTENDMASK) & ~BLKEXTENDMASK;
	char *base;

	if ((base = sbrk(incr)) == (char *)-1)
	    return(NULL);
	zextendPool(&MallocPool, base, incr);
	zfree(&MallocPool, base, incr);
    }
#ifdef DMALLOCDEBUG
    if (++MallocCount > MallocMax)
	MallocMax = MallocCount;
#endif
#ifdef USEGUARD
    res->ga_Magic = GAMAGIC;
#endif
    res->ga_Bytes = bytes;
#ifdef USEENDGUARD
    *((char *)res + bytes - 1) = -2;
#endif
    return((char *)res + MALLOCALIGN);
}

void
free(void *ptr)
{
    size_t bytes;

    if (ptr != NULL) {
	Guard *res = (void *)((char *)ptr - MALLOCALIGN);

#ifdef USEGUARD
	if (res->ga_Magic != GAMAGIC)
	    panic("free: guard1 fail @ %p", ptr);
	res->ga_Magic = -1;
#endif
#ifdef USEENDGUARD
	if (*((char *)res + res->ga_Bytes - 1) != -2)
	    panic("free: guard2 fail @ %p + %d", ptr, res->ga_Bytes - MALLOCALIGN);
	*((char *)res + res->ga_Bytes - 1) = -1;
#endif

	bytes = res->ga_Bytes;
	zfree(&MallocPool, res, bytes);
#ifdef DMALLOCDEBUG
	--MallocCount;
#endif
    }
}


void *
calloc(size_t n1, size_t n2)
{
    iaddr_t bytes = (iaddr_t)n1 * (iaddr_t)n2;
    void *res;

    if ((res = malloc(bytes)) != NULL) {
	bzero(res, bytes);
#ifdef DMALLOCDEBUG
	if (++MallocCount > MallocMax)
	    MallocMax = MallocCount;
#endif
    }
    return(res);
}

/*
 * realloc() - I could be fancier here and free the old buffer before
 * 	       allocating the new one (saving potential fragmentation
 *	       and potential buffer copies).  But I don't bother.
 */

void *
realloc(void *ptr, size_t size)
{
    void *res;
    size_t old;

    if ((res = malloc(size)) != NULL) {
	if (ptr) {
	    old = *(size_t *)((char *)ptr - MALLOCALIGN) - MALLOCALIGN;
	    if (old < size)
		bcopy(ptr, res, old);
	    else
		bcopy(ptr, res, size);
	    free(ptr);
	} else {
#ifdef DMALLOCDEBUG
	    if (++MallocCount > MallocMax)
		MallocMax = MallocCount;
#ifdef EXITSTATS
	    if (DidAtExit == 0) {
		DidAtExit = 1;
		atexit(mallocstats);
	    }
#endif
#endif
	}
    }
    return(res);
}

void *
reallocf(void *ptr, size_t size)
{
    void *res;

    if ((res = realloc(ptr, size)) == NULL)
	free(ptr);
    return(res);
}

#ifdef DMALLOCDEBUG

void
mallocstats(void)
{
    printf("Active Allocations: %d/%d\n", MallocCount, MallocMax);
#ifdef ZALLOCDEBUG
    zallocstats(&MallocPool);
#endif
}

#endif

