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
#include <machine/alpha_cpu.h>

#include "libarc.h"

/*
 * Convert from a 64bit superpage address to a 32bit arc superpage address.
 */
static void *
convert_superpage(vm_offset_t p)
{
    if (p < ALPHA_K0SEG_BASE || p >= ALPHA_K0SEG_END) {
	printf("stupid address %p\n", (void *)p);
	panic("broken");
    }
    return (void *) (0xffffffff80000000 + (p - ALPHA_K0SEG_BASE));
}

int
arc_copyin(void *src, vm_offset_t dest, size_t len)
{
    bcopy(src, convert_superpage(dest), len);
    return(len);
}

int
arc_copyout(vm_offset_t src, void *dest, size_t len)
{
    bcopy(convert_superpage(src), dest, len);
    return(len);
}

int
arc_readin(int fd, vm_offset_t dest, size_t len)
{
    return(read(fd, convert_superpage(dest), len));
}

    
