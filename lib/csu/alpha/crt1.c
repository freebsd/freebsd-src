/*-
 * Copyright 1996-1998 John D. Polstra.
 * All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *    for the NetBSD Project.
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
 *      $Id: crt1.c,v 1.1.1.1 1998/03/07 20:27:10 jdp Exp $
 */

#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif

#include <stdlib.h>

#ifdef	HAVE_RTLD
#include <sys/exec.h>
#include <sys/syscall.h>
#include <rtld.h>

const Obj_Entry *__mainprog_obj;

extern int		__syscall (int, ...);
#define	_exit(v)	__syscall(SYS_exit, (v))
#define	write(fd, s, n)	__syscall(SYS_write, (fd), (s), (n))

#define _FATAL(str)				\
	do {					\
		write(2, str, sizeof(str));	\
		_exit(1);			\
	} while (0)

#pragma weak _DYNAMIC
extern int _DYNAMIC;
#else
/*
 * When doing a bootstrap build, the header files for runtime
 * loader support are not available, so this source file is
 * compiled to a static object.
 */
#define	Obj_Entry	void
struct	ps_strings;
#endif

extern void _init(void);
extern void _fini(void);
extern int main(int, char **, char **);

char **environ;
char *__progname = "";

/* The entry function. */
void
_start(char **ap,
	void (*cleanup)(void),			/* from shared loader */
	const Obj_Entry *obj,			/* from shared loader */
	struct ps_strings *ps_strings)
{
	int argc;
	char **argv;
	char **env;

	argc = * (long *) ap;
	argv = ap + 1;
	env  = ap + 2 + argc;
	environ = env;
	if(argc > 0)
		__progname = argv[0];

#ifdef	HAVE_RTLD
	if (&_DYNAMIC != NULL) {
		if ((obj == NULL) || (obj->magic != RTLD_MAGIC))
			_FATAL("Corrupt Obj_Entry pointer in GOT");
		if (obj->version != RTLD_VERSION)
			_FATAL("Dynamic linker version mismatch");

		__mainprog_obj = obj;
		atexit(cleanup);
	}
#endif

	atexit(_fini);
	_init();
	exit( main(argc, argv, env) );
}
