/*-
 * Copyright 2001 David E. O'Brien.
 * All rights reserved.
 * Copyright 1996-1998 John D. Polstra.
 * All rights reserved.
 * Copyright (c) 1997 Jason R. Thorpe.
 * Copyright (c) 1995 Christopher G. Demetriou
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
 *          This product includes software developed for the
 *          FreeBSD Project.  See http://www.freebsd.org/ for
 *          information about FreeBSD.
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.netbsd.org/ for
 *          information about NetBSD.
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

/*
 * First 5 arguments are specified by the PowerPC SVR4 ABI.
 * The last argument, ps_strings, is a BSD extension.
 */
void _start __P((int, char **, char **, const struct Struct_Obj_Entry *,
		void (*) __P((void)), struct ps_strings *));

char **environ;
char *__progname = "";
struct ps_strings *__ps_strings;

/* The entry function. */
void
_start(argc, argv, envp, obj, cleanup, ps_strings)
	int argc;
	char **argv, **envp;
	const struct Struct_Obj_Entry *obj;	/* from shared loader */
	void (*cleanup) __P((void));		/* from shared loader */
	struct ps_strings *ps_strings;		/* BSD extension */
{
	char *namep;

	environ = envp;

	if (argc > 0 && argv[0] != NULL) {
		char *s;
		__progname = argv[0];
		for (s = __progname; *s != '\0'; s++)
			if (*s == '/')
				__progname = s + 1;
	}

	if (ps_strings != (struct ps_strings *)0)
		__ps_strings = ps_strings;

	if (&_DYNAMIC != NULL)
		atexit(cleanup);

#ifdef GCRT
	atexit(_mcleanup);
#endif
	atexit(_fini);
#ifdef GCRT
	monstartup(&eprol, &etext);
#endif
	_init();
	exit( main(argc, argv, envp) );
}

#ifdef GCRT
__asm__(".text");
__asm__("eprol:");
__asm__(".previous");
#endif

__asm__(".ident\t\"$FreeBSD$\"");
