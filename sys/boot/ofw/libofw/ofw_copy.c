/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
/*
 * MD primitives supporting placement of module data 
 *
 * XXX should check load address/size against memory top.
 */
#include <stand.h>

#include "libofw.h"

#define	READIN_BUF	(4 * 1024)
#define	PAGE_SIZE	0x1000
#define	PAGE_MASK	0x0fff

#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))

ssize_t
ofw_copyin(const void *src, vm_offset_t dest, const size_t len)
{
	void	*destp, *addr;
	size_t	dlen;

	destp = (void *)(dest & ~PAGE_MASK);
	dlen = roundup(len, PAGE_SIZE);

	if (OF_call_method("claim", memory, 3, 1, destp, dlen, 0, &addr)
	    == -1) {
		printf("ofw_copyin: physical claim failed\n");
		return (0);
	}

	if (OF_call_method("claim", mmu, 3, 1, destp, dlen, 0, &addr) == -1) {
		printf("ofw_copyin: virtual claim failed\n");
		return (0);
	}

	if (OF_call_method("map", mmu, 4, 0, destp, destp, dlen, 0) == -1) {
		printf("ofw_copyin: map failed\n");
		return (0);
	}

	bcopy(src, (void *)dest, len);
	return(len);
}

ssize_t
ofw_copyout(const vm_offset_t src, void *dest, const size_t len)
{
	bcopy((void *)src, dest, len);
	return(len);
}

ssize_t
ofw_readin(const int fd, vm_offset_t dest, const size_t len)
{
	void		*buf;
	size_t		resid, chunk, get;
	ssize_t		got;
	vm_offset_t	p;

	p = dest;

	chunk = min(READIN_BUF, len);
	buf = malloc(chunk);
	if (buf == NULL) {
		printf("ofw_readin: buf malloc failed\n");
		return(0);
	}

	for (resid = len; resid > 0; resid -= got, p += got) {
		get = min(chunk, resid);
		got = read(fd, buf, get);

		if (got <= 0) {
			printf("ofw_readin: read failed\n");
			break;
		}

		ofw_copyin(buf, p, got);
	}

	free(buf);
	return(len - resid);
}
