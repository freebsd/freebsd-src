/*-
 * Copyright (c) 2006,2011 Joseph Koshy
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
 * $Id: ndxscn.m4 1415 2011-02-05 12:45:23Z jkoshy $
 */

#include <ar.h>
#include <libelf.h>
#include <string.h>
#include <unistd.h>

#include "elfts.h"
#include "tet_api.h"

include(`elfts.m4')

IC_REQUIRES_VERSION_INIT();

/*
 * A NULL argument is handled.
 */
void
tcArgsNull(void)
{
	int error, result;
	size_t shn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_ndxscn(NULL) fails.");

	result = TET_PASS;
	if ((shn = elf_ndxscn(NULL)) != SHN_UNDEF ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("shn=%d error=%d \"%s\".", shn,
		    error, elf_errmsg(error));

	tet_result(result);
}

/*
 * elf_ndxscn() on a valid section succeeds.
 */
undefine(`FN')
define(`FN',`
void
tcScnSuccess$1$2(void)
{
	Elf *e;
	Elf_Scn *scn;
	int fd, result;
	size_t nscn, n, r;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_ndxscn(elf) succeeds.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "newscn.$2$1", ELF_C_READ, fd, goto done;);

	if (elf_getshnum(e, &nscn) == 0) {
		TP_UNRESOLVED("elf_getshnum(old) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;
	for (n = SHN_UNDEF; n < nscn; n++) {
		if ((scn = elf_getscn(e, n)) == NULL) {
			TP_UNRESOLVED("elf_getscn(%d) failed: \"%s\".", n,
			    elf_errmsg(-1));
			break;
		}

		if ((r = elf_ndxscn(scn)) != n) {
			TP_FAIL("r=%d != %n, error=\"%s\".", r, n,
			    elf_errmsg(-1));
			break;
		}
	}

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')
