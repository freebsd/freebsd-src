/*-
 * Copyright 1996, 1997, 1998, 2000 John D. Polstra.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>

typedef void (*fptr)(void);

static fptr ctor_list[1] __attribute__((section(".ctors"))) = { (fptr) -1 };
static fptr dtor_list[1] __attribute__((section(".dtors"))) = { (fptr) -1 };

static void
do_ctors(void)
{
    fptr *fpp;

    for(fpp = ctor_list + 1;  *fpp != 0;  ++fpp)
	;
    while(--fpp > ctor_list)
	(**fpp)();
}

static void
do_dtors(void)
{
    fptr *fpp;

    for(fpp = dtor_list + 1;  *fpp != 0;  ++fpp)
	(**fpp)();
}

/*
 * With very large programs on some architectures (e.g., the Alpha),
 * it is possible to get relocation overflows on the limited
 * displacements of call/bsr instructions.  It is particularly likely
 * for the calls from _init() and _fini(), because they are in
 * separate sections.  Avoid the problem by forcing indirect calls.
 */
static void (*p_do_ctors)(void) = do_ctors;
static void (*p_do_dtors)(void) = do_dtors;

extern void _init(void) __attribute__((section(".init")));

void
_init(void)
{
	(*p_do_ctors)();
}

extern void _fini(void) __attribute__((section(".fini")));

void
_fini(void)
{
	(*p_do_dtors)();
}

#include "crtbrand.c"
