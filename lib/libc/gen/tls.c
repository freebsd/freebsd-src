/*-
 * Copyright (c) 2004 Doug Rabson
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
 *	$FreeBSD$
 */

/*
 * Define stubs for TLS internals so that programs and libraries can
 * link. These functions will be replaced by functional versions at
 * runtime from ld-elf.so.1.
 */

#include <stddef.h>

#ifdef __i386__

extern void *___tls_get_addr(void *ti) __attribute__((__regparm__(1)));

#pragma weak ___tls_get_addr
__attribute__((__regparm__(1)))
void *
___tls_get_addr(void *ti)
{
	return (0);
}

#endif

#pragma weak __tls_get_addr
void *
__tls_get_addr()
{
	return (0);
}

#pragma weak _rtld_allocate_tls
void *
_rtld_allocate_tls(void *oldtls, size_t tcbsize, size_t tcbalign)
{
	return (0);
}

#pragma weak _rtld_free_tls
void
_rtld_free_tls(void *tcb, size_t tcbsize, size_t tcbalign)
{
}
