/* LINTLIBRARY */
/*-
 * Copyright 2001 David E. O'Brien.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef lint
#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif
#endif /* lint */

#include <stdlib.h>

#include "libc_private.h"
#include "crtbrand.c"

struct Struct_Obj_Entry;
struct ps_strings;

extern int _DYNAMIC;
#pragma weak _DYNAMIC

extern void _fini(void);
extern void _init(void);
extern int main(int, char **, char **);
extern void __sparc_utrap_setup(void);

#ifdef GCRT
extern void _mcleanup(void);
extern void monstartup(void *, void *);
extern int eprol;
extern int etext;
#endif

char **environ;
const char *__progname = "";

void _start(char **, void (*)(void), struct Struct_Obj_Entry *,
    struct ps_strings *);

/* The entry function. */
/*
 * %o0 holds ps_strings pointer.
 *
 * Note: kernel may (is not set in stone yet) pass ELF aux vector in %o1,
 * but for now we do not use it here.
 *
 * The SPARC compliance definitions specifies that the kernel pass the
 * address of a function to be executed on exit in %g1. We do not make
 * use of it as it is quite broken, because gcc can use this register
 * as a temporary, so it is not safe from C code. Its even more broken
 * for dynamic executables since rtld runs first.
 */
/* ARGSUSED */
void
_start(char **ap, void (*cleanup)(void), struct Struct_Obj_Entry *obj __unused,
    struct ps_strings *ps_strings __unused)
{
	int argc;
	char **argv;
	char **env;
	const char *s;

	argc = *(long *)(void *)ap;
	argv = ap + 1;
	env  = ap + 2 + argc;
	environ = env;
	if (argc > 0 && argv[0] != NULL) {
		__progname = argv[0];
		for (s = __progname; *s != '\0'; s++)
			if (*s == '/')
				__progname = s + 1;
	}

	if (&_DYNAMIC != NULL)
		atexit(cleanup);
	else {
		__sparc_utrap_setup();
		_init_tls();
	}
#ifdef GCRT
	atexit(_mcleanup);
#endif
	atexit(_fini);
#ifdef GCRT
	monstartup(&eprol, &etext);
#endif
	_init();
	exit( main(argc, argv, env) );
}

#ifdef GCRT
__asm__(".text");
__asm__("eprol:");
__asm__(".previous");
#endif
