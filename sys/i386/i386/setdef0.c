/*-
 * Copyright (c) 1997 John D. Polstra
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
 * $Id$
 */

extern void init_sets(void);

#ifdef __ELF__

#include <sys/param.h>
#include <sys/kernel.h>

/*
 * BEGIN_SET creates the section and label for a set, and emits the
 * count word at the front of it.  The count word initially contains 0,
 * but is filled in with the correct value at runtime by init_sets().
 */
#define BEGIN_SET(set)					\
	__asm__(".section .set." #set ",\"aw\"");	\
	__asm__(".globl " #set);			\
	__asm__(".type " #set ",@object");		\
	__asm__(".p2align 2");				\
	__asm__(#set ":");				\
	__asm__(".long 0");				\
	__asm__(".previous")

/*
 * DEFINE_SET calls BEGIN_SET and also enters the set into the mother
 * of all sets, `set_of_sets'.
 */
#define DEFINE_SET(set)	 BEGIN_SET(set); DATA_SET(set_of_sets, set)

/*
 * Define a set that contains a list of all the other linker sets.
 */

BEGIN_SET(set_of_sets);

#include <i386/i386/setdefs.h>	/* Contains a `DEFINE_SET' for each set */

extern struct linker_set set_of_sets;

/*
 * Fill in the leading "ls_length" fields of all linker sets.  This is
 * needed for ELF.  For a.out, it is already done by the linker.
 */
void
init_sets(void)
{
	struct linker_set **lspp = (struct linker_set **) set_of_sets.ls_items;
	struct linker_set *lsp;

	for (; (lsp = *lspp) != NULL; lspp++) {
		int i;

		for (i = 0; lsp->ls_items[i] != NULL; i++)  /* Count items */
			;
		lsp->ls_length = i;
	}
}

#else	/* ! __ELF__ */

void
init_sets(void)
{
}

#endif	/* __ELF__ */
