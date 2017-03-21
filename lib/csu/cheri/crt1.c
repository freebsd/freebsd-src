/*-
 * Copyright 2015 SRI International
 * Copyright 2013 Philip Withnall
 * Copyright 1996-1998 John D. Polstra.
 * All rights reserved.
 * Copyright (c) 1995 Christopher G. Demetriou
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * $FreeBSD: head/lib/csu/mips/crt1_c.c 245133 2013-01-07 17:58:27Z kib $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/lib/csu/mips/crt1_c.c 245133 2013-01-07 17:58:27Z kib $");

#ifndef __GNUC__
#error "GCC is needed to compile this file"
#endif

#include <sys/types.h>
#include <sys/cheriabi.h>

#include <machine/elf.h>

#include <stdlib.h>
#include "libc_private.h"
#include "crtbrand.c"
#include "ignore_init.c"

struct Struct_Obj_Entry;
struct ps_strings;

void __start(struct cheriabi_execdata *, void (*)(void),
    struct Struct_Obj_Entry *);
extern void crt_call_constructors(void);

#ifdef GCRT
/* Profiling support. */
extern void _mcleanup(void);
extern void monstartup(void *, void *);
extern int eprol;
extern int etext;
#endif

struct ps_strings *__ps_strings;
Elf64_Auxinfo *__auxargs;

struct capreloc
{
	uint64_t capability_location;
	uint64_t object;
	uint64_t offset;
	uint64_t size;
	uint64_t permissions;
};
static const uint64_t function_reloc_flag = 1ULL<<63;
static const uint64_t function_pointer_permissions =
	~0 &
	~__CHERI_CAP_PERMISSION_PERMIT_STORE_CAPABILITY__ &
	~__CHERI_CAP_PERMISSION_PERMIT_STORE__;
static const uint64_t global_pointer_permissions =
	~0 & ~__CHERI_CAP_PERMISSION_PERMIT_EXECUTE__;

__attribute__((weak))
extern struct capreloc __start___cap_relocs;
__attribute__((weak))
extern struct capreloc __stop___cap_relocs;

static void
crt_init_globals()
{
	void *gdc = __builtin_cheri_global_data_get();
	void *pcc = __builtin_cheri_program_counter_get();

	gdc = __builtin_cheri_perms_and(gdc, global_pointer_permissions);
	pcc = __builtin_cheri_perms_and(pcc, function_pointer_permissions);
	for (struct capreloc *reloc = &__start___cap_relocs ;
	     reloc < &__stop___cap_relocs ; reloc++)
	{
		_Bool isFunction = (reloc->permissions & function_reloc_flag) ==
			function_reloc_flag;
		void **dest = __builtin_cheri_offset_set(gdc, reloc->capability_location);
		void *base = isFunction ? pcc : gdc;
		void *src = __builtin_cheri_offset_set(base, reloc->object);
		if (!isFunction && (reloc->size != 0))
		{
			src = __builtin_cheri_bounds_set(src, reloc->size);
		}
		src = __builtin_cheri_offset_increment(src, reloc->offset);
		*dest = src;
	}
}

/* The entry function, C part. This performs the bulk of program initialisation
 * before handing off to main(). It is called by __start, which is defined in
 * crt1_s.s, and necessarily written in raw assembly so that it can re-align
 * the stack before setting up the first stack frame and calling _start1().
 *
 * It would be nice to be able to hide the _start1 symbol, but that's not
 * possible, since it must be present in the GOT in order to be resolvable by
 * the position independent code in __start.
 * See: http://stackoverflow.com/questions/8095531/mips-elf-and-partial-linking
 */
void
__start(struct cheriabi_execdata *ce,
	void (*cleanup)(void),			/* from shared loader */
	struct Struct_Obj_Entry *obj __unused)	/* from shared loader */
{
	int argc;
	char **argv;
	char **env;

	crt_init_globals();

	argc = ce->ce_argc;
	argv = ce->ce_argv;
	env = ce->ce_envp;
	__auxargs = (Elf64_Auxinfo *)ce->ce_auxargs;
	__ps_strings = ce->ce_ps_strings;
	handle_argv(argc, argv, env);

	if (&_DYNAMIC != NULL)
		atexit(cleanup);
	else
		_init_tls();

	crt_call_constructors();

#ifdef GCRT
	/* Set up profiling support for the program, if we're being compiled
	 * with profiling support enabled (-DGCRT).
	 * See: http://sourceware.org/binutils/docs/gprof/Implementation.html
	 */
	atexit(_mcleanup);
	monstartup(&eprol, &etext);

	/* Create an 'eprol' (end of prologue?) label which delimits the start
	 * of the .text section covered by profiling. This must be before
	 * main(). */
__asm__("eprol:");
#endif

	handle_static_init(argc, argv, env);

	exit(main(argc, argv, env));
}
