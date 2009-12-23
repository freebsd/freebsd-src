/*-
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
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif

#include <stdlib.h>
#include "libc_private.h"
#include "crtbrand.c"

struct Struct_Obj_Entry;
struct ps_strings;

#ifndef NOSHARED
extern int _DYNAMIC;
#pragma weak _DYNAMIC
#endif

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
const char *__progname = "";
struct ps_strings *__ps_strings;

void __gccmain(void) {}
void __main(void) {}

/*
 * Historically, mips has used __start for the beginning address of programs.
 * However, the Cavium toolchain (and maybe others) use _start.  Define both
 * here.  The assembler code here tries to juggle the arguments such that they
 * are right for all ABIs and then calls __start_mips which is what used to
 * be just plain __start, and still is on other BSD ports.
 */

/* The entry function. */
__asm("	.text			\n"
"	.align	8		\n"
"	.globl	_start		\n"
"	_start:			\n"
"	.globl	__start		\n"
"	__start:		\n"
#if defined(__mips_n32) || defined(__mips_n64)
"	.cpsetup $25, $24, __start\n"
#else
"	.set noreorder		\n"
"	.cpload $25		\n"
"	.set reorder		\n"
#endif
"	/* Get cleanup routine and main object set by rtld */\n"
"	/* Note that a2 is already set to ps_string by _rtld_start */\n"
"	/* move	a3, a0        */\n"
"	/* move	t0, a1        */\n"
"	/* Get argc, argv from stack */	\n"
"	/* lw	a0, 0(sp)     */\n"
"	/* move	a1, sp        */\n"
"	/* addu	a1, 4         */\n"
"				\n"
"	/* Stack should 8bytes aligned */\n"
"	/* required by ABI to pass     */\n"
"	/* 64-bits arguments           */\n"
"	/* and	sp, ~8        */\n"
"	/* subu	sp, sp, 20    */\n"
"	/* sw	t0, 16(sp)    */\n"
"				\n"
"	move	$7, $4		/* atexit */\n"
"	move	$8, $5		/* main_obj entry */\n"
#if defined(__mips_n64)
"	ld	$4, 0($29)	\n"
"	move	$5, $29		\n"
"	addu	$5, 8		\n"
#else
"	lw	$4, 0($29)	\n"
"	move	$5, $29		\n"
"	addu	$5, 4		\n"
#endif
"				\n"
"	and	$29, 0xfffffff8	\n"
"	subu	$29, $29, 24	/* args slot + cleanup + 4 bytes padding */ \n"
"	sw	$8, 16($29)	\n"
"\n"
"	la	 $25, __start_mips  \n"
"	nop	 \n"
"	j	 $25\n");
/* ARGSUSED */

void
__start_mips(int argc, char **argv, struct ps_strings *ps_strings,
    void (*cleanup)(void), struct Struct_Obj_Entry *obj __unused)
{
	char **env;
	const char *s;

	env  = argv + argc + 1;
	environ = env;

	if(argc > 0 && argv[0] != NULL) {
		__progname = argv[0];
		for (s = __progname; *s != '\0'; s++)
			if (*s == '/')
				__progname = s + 1;
	}

	__ps_strings = ps_strings;

	if (&_DYNAMIC != NULL)
		atexit(cleanup);
	else
		_init_tls();

#ifdef GCRT
	atexit(_mcleanup);
#endif
	atexit(_fini);
#ifdef GCRT
	monstartup(&eprol, &etext);
#endif
#ifndef NOGPREL
	_init();
#endif
	exit( main(argc, argv, env) );
}

#ifdef GCRT
__asm__(".text");
__asm__("eprol:");
__asm__(".previous");
#endif
