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
 *
 * $FreeBSD: src/lib/csu/i386/c++rt0.c,v 1.9 1999/08/27 23:57:55 peter Exp $
 */

/*
 * Run-time module for GNU C++ compiled shared libraries.
 *
 * The linker constructs the following arrays of pointers to global
 * constructors and destructors. The first element contains the
 * number of pointers in each.
 * The tables are also null-terminated.
 */
extern void (*__CTOR_LIST__[])(void);
extern void (*__DTOR_LIST__[])(void);

static void
__dtors(void)
{
	unsigned long i = (unsigned long) __DTOR_LIST__[0];
	void (**p)(void) = __DTOR_LIST__ + i;

	while (i--)
		(**p--)();
}

static void
__ctors(void)
{
	void (**p)(void) = __CTOR_LIST__ + 1;

	while (*p)
		(**p++)();
}

extern void __init() asm(".init");
extern void __fini() asm(".fini");

void
__init(void)
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

void
__fini(void)
{
	__dtors();
}

/*
 * Make sure there is at least one constructor and one destructor in the
 * shared library.  Otherwise, the linker does not realize that the
 * constructor and destructor lists are linker sets.  It treats them as
 * commons and resolves them to the lists from the main program.  That
 * causes multiple invocations of the main program's static constructors
 * and destructors, which is very bad.
 */

static void
do_nothing(void)
{
}

/* Linker magic to add an element to a constructor or destructor list. */
#define TEXT_SET(set, sym) \
	asm(".stabs \"_" #set "\", 23, 0, 0, _" #sym)

TEXT_SET(__CTOR_LIST__, do_nothing);
TEXT_SET(__DTOR_LIST__, do_nothing);
