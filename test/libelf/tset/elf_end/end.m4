/*-
 * Copyright (c) 2006,2010 Joseph Koshy
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
 * $Id: end.m4 1349 2011-01-01 15:18:43Z jkoshy $
 */

include(`elfts.m4')

#include <libelf.h>

#include "elfts.h"
#include "tet_api.h"

IC_REQUIRES_VERSION_INIT();

/*
 * Test the `elf_end' entry point.
 */

void
tcEndNullOk(void)
{
	TP_ANNOUNCE("a NULL argument is allowed.");

	tet_result(elf_end(NULL) == 0 ? TET_PASS : TET_FAIL);
}

char data[] = "0xDEADC0DE";

void
tcBeginEndPair(void)
{
	Elf *e;

	TP_ANNOUNCE("a single begin/end pair works");

	TP_CHECK_INITIALIZATION();

	TS_OPEN_MEMORY(e,data);

	tet_result (elf_end(e) == 0 ? TET_PASS : TET_FAIL);
}

void
tcNestedCount(void)
{
	int r1, r2;
	Elf *e, *e1;

	TP_ANNOUNCE("begin/end pairs nest correctly");

	TP_CHECK_INITIALIZATION();

	TS_OPEN_MEMORY(e,data);

	if ((e1 = elf_begin(-1, ELF_C_READ, e)) != e) {
		tet_result(TET_UNRESOLVED);
		return;
	}

	if ((r1 = elf_end(e1)) != 1 ||
	    (r2 = elf_end(e)) != 0) {
		tet_printf("fail: r1=%d r2=%d.", r1, r2);
		tet_result(TET_FAIL);
		return;
	}

	tet_result(TET_PASS);
}

/*
 * TODO
 * - closing a member of an archive should decrement the parent's activation
 *   count.
 * - opening a member of an archive should increment the parent's activation
 *   count.
 * - What do we do about out of order elf_end()'s on archives and members.
 */
