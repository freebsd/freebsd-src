/*-
 * SPDX-License-Identifier: BSD-1-Clause
 *
 * Copyright 2012 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2018, 2023 The FreeBSD Foundation
 *
 * Parts of this software was developed by Konstantin Belousov
 * <kib@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#include <sys/param.h>
#include <sys/elf.h>
#include <sys/elf_common.h>
#include <errno.h>
#include <stdlib.h>
#include "libc_private.h"

extern void (*__preinit_array_start[])(int, char **, char **) __hidden;
extern void (*__preinit_array_end[])(int, char **, char **) __hidden;
extern void (*__init_array_start[])(int, char **, char **) __hidden;
extern void (*__init_array_end[])(int, char **, char **) __hidden;
extern void (*__fini_array_start[])(void) __hidden;
extern void (*__fini_array_end[])(void) __hidden;
extern void _fini(void) __hidden;
extern void _init(void) __hidden;

extern int _DYNAMIC;
#pragma weak _DYNAMIC

#if defined(CRT_IRELOC_RELA)
extern const Elf_Rela __rela_iplt_start[] __weak_symbol __hidden;
extern const Elf_Rela __rela_iplt_end[] __weak_symbol __hidden;

#include "reloc.c"

static void
process_irelocs(void)
{
	const Elf_Rela *r;

	for (r = &__rela_iplt_start[0]; r < &__rela_iplt_end[0]; r++)
		crt1_handle_rela(r);
}
#elif defined(CRT_IRELOC_REL)
extern const Elf_Rel __rel_iplt_start[] __weak_symbol __hidden;
extern const Elf_Rel __rel_iplt_end[] __weak_symbol __hidden;

#include "reloc.c"

static void
process_irelocs(void)
{
	const Elf_Rel *r;

	for (r = &__rel_iplt_start[0]; r < &__rel_iplt_end[0]; r++)
		crt1_handle_rel(r);
}
#elif defined(CRT_IRELOC_SUPPRESS)
#else
#error "Define platform reloc type"
#endif

#ifndef PIC
static void
finalizer(void)
{
	void (*fn)(void);
	size_t array_size, n;

	array_size = __fini_array_end - __fini_array_start;
	for (n = array_size; n > 0; n--) {
		fn = __fini_array_start[n - 1];
		if ((uintptr_t)fn != 0 && (uintptr_t)fn != 1)
			(fn)();
	}
	_fini();
}
#endif

static void
handle_static_init(int argc, char **argv, char **env)
{
#ifndef PIC
	void (*fn)(int, char **, char **);
	size_t array_size, n;

	if (&_DYNAMIC != NULL)
		return;

	atexit(finalizer);

	array_size = __preinit_array_end - __preinit_array_start;
	for (n = 0; n < array_size; n++) {
		fn = __preinit_array_start[n];
		if ((uintptr_t)fn != 0 && (uintptr_t)fn != 1)
			fn(argc, argv, env);
	}
	_init();
	array_size = __init_array_end - __init_array_start;
	for (n = 0; n < array_size; n++) {
		fn = __init_array_start[n];
		if ((uintptr_t)fn != 0 && (uintptr_t)fn != 1)
			fn(argc, argv, env);
	}
#endif
}

static void
handle_argv(int argc, char *argv[], char **env)
{
	const char *s;

	if (environ == NULL)
		environ = env;
	if (argc > 0 && argv[0] != NULL) {
		__progname = argv[0];
		for (s = __progname; *s != '\0'; s++) {
			if (*s == '/')
				__progname = s + 1;
		}
	}
}

static void
handle_irelocs(char *env[])
{
#ifndef CRT_IRELOC_SUPPRESS
	const Elf_Auxinfo *aux;

	/* Find the auxiliary vector on the stack. */
	while (*env++ != 0)	/* Skip over environment, and NULL terminator */
		;
	aux = (const Elf_Auxinfo *)env;

	ifunc_init(aux);
	process_irelocs();
#else
	(void)env;
#endif
}

void
__libc_start1(int argc, char *argv[], char *env[], void (*cleanup)(void),
    int (*mainX)(int, char *[], char *[]))
{
	handle_argv(argc, argv, env);

	if (&_DYNAMIC != NULL) {
		atexit(cleanup);
	} else {
		handle_irelocs(env);
		_init_tls();
	}

	handle_static_init(argc, argv, env);

	/*
	 * C17 4.3 paragraph 3:
	 * The value of errno in the initial thread is zero at program
	 * startup.
	 */
	errno = 0;
	exit(mainX(argc, argv, env));
}

/* XXXKIB _mcleanup and monstartup defs */
extern void _mcleanup(void);
extern void monstartup(void *, void *);

void
__libc_start1_gcrt(int argc, char *argv[], char *env[],
    void (*cleanup)(void), int (*mainX)(int, char *[], char *[]),
    int *eprolp, int *etextp)
{
	handle_argv(argc, argv, env);

	if (&_DYNAMIC != NULL) {
		atexit(cleanup);
	} else {
		handle_irelocs(env);
		_init_tls();
	}

	atexit(_mcleanup);
	monstartup(eprolp, etextp);

	handle_static_init(argc, argv, env);
	errno = 0;
	exit(mainX(argc, argv, env));
}
