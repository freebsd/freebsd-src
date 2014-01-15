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
 * $Id: getshstrndx.m4 1404 2011-02-05 07:51:28Z jkoshy $
 */

#include <ar.h>
#include <libelf.h>
#include <string.h>
#include <unistd.h>

#include "elfts.h"
#include "tet_api.h"

IC_REQUIRES_VERSION_INIT();

include(`elfts.m4')

/*
 * A NULL `Elf' argument fails.
 */
void
tcArgsNull(void)
{
	int error, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getshstrndx(NULL,*) fails.");

	result = TET_PASS;
	if (elf_getshstrndx(NULL, &n) != 0 ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("n=%d error=%d \"%s\".", n, error,
		    elf_errmsg(error));

	tet_result(result);
}

/*
 * elf_getshstrndx() on a non-ELF file fails.
 */
static char *nonelf = "This is not an ELF file.";

void
tcArgsNonElf(void)
{
	Elf *e;
	size_t n;
	int error, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_getshstrndx(non-elf) fails.");

	TS_OPEN_MEMORY(e, nonelf);

	result = TET_PASS;
	if (elf_getshstrndx(e, &n) != 0 ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("n=%d error=%d \"%s\".", n, error,
		    elf_errmsg(error));

	(void) elf_end(e);

	tet_result(result);
}


/*
 * elf_getshstrndx() on a well-formed file succeeds.
 */
undefine(`FN')
define(`FN',`
void
tcNormal_$1$3`'TOUPPER($4)(void)
{
	Elf *e;
	int fd, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($4)$1$3: elf_getshstrndx(elf) succeeds.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "$2.$4$3", ELF_C_READ, fd, goto done;);

	n = ~ (size_t) 0;

	result = TET_PASS;
	if (elf_getshstrndx(e, &n) == 0 || n != $5)
		TP_FAIL("n=%d, expected $5: error=\"%s\".", n,
		  elf_errmsg(-1));

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);

	tet_result(result);
}')

FN(N,newscn,32,lsb,1)
FN(N,newscn,32,msb,1)
FN(N,newscn,64,lsb,1)
FN(N,newscn,64,msb,1)
FN(X,`xscn-2',32,lsb,65537)
FN(X,`xscn-2',32,msb,65537)
FN(X,`xscn-2',64,lsb,65537)
FN(X,`xscn-2',64,msb,65537)

/*
 * elf_getshstrndx() on a file with a malformed section #0 fails.
 */
undefine(`FN')
define(`FN',`
void
tcMalformed_Xscn$1$2(void)
{
	Elf *e;
	int error, fd, result;
	size_t n;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_getshstrndx(elf) returns ELF_E_SECTION.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "xscn-3.$2$1", ELF_C_READ, fd, goto done;);

	n = ~(size_t) 0;
	result = TET_PASS;
	if ((elf_getshstrndx(e, &n) != 0 ||
	    (error = elf_errno()) != ELF_E_SECTION))
		TP_FAIL("n=%d error=%d \"%s\".", n, error, elf_errmsg(error));

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
