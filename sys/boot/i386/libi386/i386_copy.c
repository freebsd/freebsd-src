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

#include "libi386.h"
#include "btxv86.h"

#define READIN_BUF	(16 * 1024)

ssize_t
i386_copyin(const void *src, vm_offset_t dest, const size_t len)
{
    if (dest + len >= memtop) {
	errno = EFBIG;
	return(-1);
    }

    bcopy(src, PTOV(dest), len);
    return(len);
}

ssize_t
i386_copyout(const vm_offset_t src, void *dest, const size_t len)
{
    if (src + len >= memtop) {
	errno = EFBIG;
	return(-1);
    }
    
    bcopy(PTOV(src), dest, len);
    return(len);
}


ssize_t
i386_readin(const int fd, vm_offset_t dest, const size_t len)
{
    void	*buf;
    size_t	resid, chunk, get;
    ssize_t	got;

    if (dest + len >= memtop)
	return(0);

    chunk = min(READIN_BUF, len);
    buf = malloc(chunk);
    if (buf == NULL)
	return(0);

    for (resid = len; resid > 0; resid -= got, dest += got) {
	get = min(chunk, resid);
	got = read(fd, buf, get);
	if (got <= 0)
	    break;
	bcopy(buf, PTOV(dest), (size_t)got);
    }
    free(buf);
    return(len - resid);
}
