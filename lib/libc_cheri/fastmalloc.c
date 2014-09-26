/*-
 * Copyright (c) 2014 SRI International
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

#include <machine/cheri.h>
#include <machine/cheric.h>

#ifdef MALLOC_DEBUG
#include <stdint.h>
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>

register_t  _sb_heapbase;
size_t	 _sb_heaplen;
void	*_sb_heapcap;

#define	MALLOC_ALIGN	sizeof(__capability void *)

void *
malloc(size_t size)
{
	size_t rsize;
	char *ptr;

	if (_sb_heapcap == NULL) {
		_sb_heapcap = cheri_setlen(
		    cheri_incbase(__builtin_cheri_get_global_data_cap(),
		    _sb_heapbase), _sb_heaplen);
#ifdef MALLOC_DEBUG
		printf("%s: _sb_heapcap base 0x%jx offset 0x%jx length 0x%zx\n",
		    __func__, cheri_getbase(_sb_heapcap),
		    cheri_getoffset(_sb_heapcap), cheri_getlen(_sb_heapcap));
#endif
	}

#if defined(MALLOC_DEBUG) && MALLOC_DEBUG > 1
	printf("about to malloc %zu at 0x%jx\n", size,
	    cheri_getbase(_sb_heapcap));
#endif

	rsize = roundup2(size + MALLOC_ALIGN, MALLOC_ALIGN);
#if defined(MALLOC_DEBUG) && MALLOC_DEBUG > 1
	printf("rsize = %zu\n", rsize);
#endif
	if (cheri_getlen(_sb_heapcap) < rsize)
		return (NULL);
	ptr = cheri_setlen(_sb_heapcap, rsize);
	_sb_heapcap = cheri_incbase(_sb_heapcap, rsize);

	/* XXX: replace with capability to allocation */
	*(size_t*)ptr = rsize;
	ptr += 32;

#if defined(MALLOC_DEBUG) && MALLOC_DEBUG > 1
	printf("ptr base 0x%jx offset 0x%jx length 0x%zx\n",
	    cheri_getbase(ptr), cheri_getoffset(ptr), cheri_getlen(ptr));
#endif

	return(ptr);
}

void
free(void *ptr __unused)
{

}

void *
calloc(size_t number, size_t size)
{
	char *ptr;

	/* XXX: check for overflow */
	ptr = malloc(number * size);
	if (ptr != NULL)
		memset(ptr, 0, size);
	return (ptr);
}

void *
realloc(void *ptr, size_t size)
{
	size_t osize;
	char *nptr;

	osize = *(size_t*)ptr - 32;
	if (osize >= size)
		return (ptr);
	nptr = malloc(size);
	memcpy(nptr, ptr, osize);
	return nptr;
}

void *   
reallocf(void *ptr, size_t size)
{
	void *new;

	if ((new = realloc(ptr, size)) == NULL)
		free(ptr);

	return (new);
}

