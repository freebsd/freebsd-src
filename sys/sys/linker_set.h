/*-
 * Copyright (c) 1999 John D. Polstra
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/sys/linker_set.h,v 1.4.2.1 2000/08/02 21:52:20 peter Exp $
 */

#ifndef _SYS_LINKER_SET_H_
#define _SYS_LINKER_SET_H_

/*
 * The following macros are used to declare global sets of objects, which
 * are collected by the linker into a `struct linker_set' as defined below.
 * For ELF, this is done by constructing a separate segment for each set.
 * For a.out, it is done automatically by the linker.
 *
 * In the MAKE_SET macros below, the lines:
 *
 *   static void const * const __set_##set##_sym_##sym = &sym;
 *
 * are present only to prevent the compiler from producing bogus
 * warnings about unused symbols.
 */
#ifdef __ELF__

/*
 * Alpha GAS needs an align before the section change.  It seems to assume
 * that after the .previous, it is aligned, so the following .align 3 is
 * ignored.  Since the previous instructions often contain strings, this is
 * a problem.
 */

#ifdef __alpha__
#define MAKE_SET(set, sym)						\
	static void const * const __set_##set##_sym_##sym = &sym;	\
	__asm(".align 3");						\
	__asm(".section .set." #set ",\"aw\"");				\
	__asm(".quad " #sym);						\
	__asm(".previous")
#else
#define MAKE_SET(set, sym)						\
	static void const * const __set_##set##_sym_##sym = &sym;	\
	__asm(".section .set." #set ",\"aw\"");				\
	__asm(".long " #sym);						\
	__asm(".previous")
#endif
#define TEXT_SET(set, sym) MAKE_SET(set, sym)
#define DATA_SET(set, sym) MAKE_SET(set, sym)

#else

/*
 * NB: the constants defined below must match those defined in
 * ld/ld.h.  Since their calculation requires arithmetic, we
 * can't name them symbolically (e.g., 23 is N_SETT | N_EXT).
 */
#define MAKE_SET(set, sym, type) \
	static void const * const __set_##set##_sym_##sym = &sym; \
	__asm(".stabs \"_" #set "\", " #type ", 0, 0, _" #sym)
#define TEXT_SET(set, sym) MAKE_SET(set, sym, 23)
#define DATA_SET(set, sym) MAKE_SET(set, sym, 25)

#endif

struct linker_set {
	int	ls_length;
	void	*ls_items[1];		/* really ls_length of them,
						 * trailing NULL */
};

#endif /* _SYS_LINKER_SET_H_ */
