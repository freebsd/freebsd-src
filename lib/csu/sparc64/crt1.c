/*
 * Copyright 2001 David E. O'Brien
 * All rights reserved.
 * Copyright (c) 1995, 1998 Berkeley Software Design, Inc.
 * All rights reserved.
 * Copyright 1996-1998 John D. Polstra.
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
 * 3. The name of the authors may not be used to endorse or promote products
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

#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif

#include <stdlib.h>
#include "crtbrand.c"

struct Struct_Obj_Entry;
struct ps_strings;

#pragma weak _DYNAMIC
extern int _DYNAMIC;

extern void _init(void);
extern void _fini(void);
extern int main(int, char **, char **);

#ifdef GCRT
extern void _mcleanup(void);
extern void monstartup(void *, void *);
extern int eprol;
extern int etext;
#endif

char **environ;
char *__progname = "";

/* The entry function. */
/*
 *
 * %o0 holds ps_strings pointer.  For Solaris compat and/or shared
 * libraries, if %g1 is not 0, it is a routine to pass to atexit().
 * (By passing the pointer in the usual argument register, we avoid
 * having to do any inline assembly, except to recover %g1.)
 *
 * Note: kernel may (is not set in stone yet) pass ELF aux vector in %o1,
 * but for now we do not use it here.
 */
void
_start(char **ap,
	void (*cleanup)(void),			/* from shared loader */
	struct Struct_Obj_Entry *obj,		/* from shared loader */
	struct ps_strings *ps_strings)
{
	int argc;
	char **argv;
	char **env;
#if 0
	void (*term)(void);	

	/* Grab %g1 before it gets used for anything by the compiler. */
	/* Sparc ELF psABI specifies a termination routine (if any) will be in
	   %g1 */
	__asm__ volatile("mov %%g1,%0" : "=r"(term));
#endif

	argc = * (long *) ap;
	argv = ap + 1;
	env  = ap + 2 + argc;
	environ = env;
	if(argc > 0 && argv[0] != NULL) {
		char *s;
		__progname = argv[0];
		for (s = __progname; *s != '\0'; s++)
			if (*s == '/')
				__progname = s + 1;
	}

#if 0
	/*
	 * If the kernel or a shared library wants us to call
	 * a termination function, arrange to do so.
	 */
	if (term)
		atexit(term);
#endif

	if (&_DYNAMIC != NULL)
		atexit(cleanup);

#ifdef GCRT
	atexit(_mcleanup);
#endif
#if 0
	atexit(_fini);
#endif
#ifdef GCRT
	monstartup(&eprol, &etext);
#endif
#if 0
	_init();
#endif
	exit( main(argc, argv, env) );
}

#ifdef GCRT
__asm__(".text");
__asm__("eprol:");
__asm__(".previous");
#endif

/*
 * NOTE: Leave the RCS ID _after_ __start(), in case it gets placed in .text.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
