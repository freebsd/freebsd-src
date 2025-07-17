/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef LIB9P_LIB9P_IMPL_H
#define LIB9P_LIB9P_IMPL_H

#include <stdio.h>
#include <stdlib.h>

#ifndef _KERNEL
static inline void *
l9p_malloc(size_t size)
{
	void *r = malloc(size);

	if (r == NULL) {
		fprintf(stderr, "cannot allocate %zd bytes: out of memory\n",
		    size);
		abort();
	}

	return (r);
}

static inline void *
l9p_calloc(size_t n, size_t size)
{
	void *r = calloc(n, size);

	if (r == NULL) {
		fprintf(stderr, "cannot allocate %zd bytes: out of memory\n",
		    n * size);
		abort();
	}

	return (r);
}

static inline void *
l9p_realloc(void *ptr, size_t newsize)
{
	void *r = realloc(ptr, newsize);

	if (r == NULL) {
		fprintf(stderr, "cannot allocate %zd bytes: out of memory\n",
		    newsize);
		abort();
	}

	return (r);
}
#endif /* _KERNEL */

#endif /* LIB9P_LIB9P_IMPL_H */
