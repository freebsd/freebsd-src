/* $Id$ */
/*	From: NetBSD: crtbegin.c,v 1.2 1997/03/21 05:47:28 cgd Exp */

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#ifndef ECOFF_COMPAT

/*
 * XXX EVENTUALLY SHOULD BE MERGED BACK WITH c++rt0.c
 */

/*
 * Run-time module which handles constructors and destructors,
 * and NetBSD identification.
 *
 * The linker constructs the following arrays of pointers to global
 * constructors and destructors. The first element contains the
 * number of pointers in each or -1 to indicate that the run-time
 * code should figure out how many there are.  The tables are also
 * null-terminated.
 */

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>
#include <stdlib.h>

/* #include "sysident.h" */

static void (*__CTOR_LIST__[1]) __P((void))
    __attribute__((section(".ctors"))) = { (void *)-1 };	/* XXX */
static void (*__DTOR_LIST__[1]) __P((void))
    __attribute__((section(".dtors"))) = { (void *)-1 };	/* XXX */

static void	__dtors __P((void));
static void	__ctors __P((void));

static void
__dtors()
{
	unsigned long i = (unsigned long) __DTOR_LIST__[0];
	void (**p)(void);

	if (i == -1)  {
		for (i = 1; __DTOR_LIST__[i] != NULL; i++)
			;
		i--;
	}
	p = __DTOR_LIST__ + i;
	while (i--)
		(**p--)();
}

static void
__ctors()
{
	void (**p)(void) = __CTOR_LIST__ + 1;

	while (*p)
		(**p++)();
}

extern void __init(void) __attribute__((section(".init")));

void
__init()
{
	static int initialized = 0;

	/*
	 * Call global constructors.
	 * Arrange to call global destructors at exit.
	 */
	if (!initialized) {
		initialized = 1;
		__ctors();
	}

}

extern void __fini(void) __attribute__((section(".fini")));

void
__fini()
{
	/*
	 * Call global destructors.
	 */
	__dtors();
}

#endif /* !ECOFF_COMPAT */
