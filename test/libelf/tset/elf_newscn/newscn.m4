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
 * $Id: newscn.m4 1389 2011-01-26 02:31:24Z jkoshy $
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
 * A null argument is handled.
 */
void
tcArgsNull(void)
{
	int error, result;
	Elf_Scn *scn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_newscn(NULL) fails.");

	result = TET_PASS;
	if ((scn = elf_newscn(NULL)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

	tet_result(result);
}

/*
 * An ELF descriptor for a data file is rejected.
 */
static char *nonelf = "This is not an ELF file.";

void
tcArgsNonElf(void)
{
	Elf *e;
	Elf_Scn *scn;
	int error, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_newscn(non-elf) fails.");

	TS_OPEN_MEMORY(e, nonelf);

	result = TET_PASS;

	if ((scn = elf_newscn(e)) != NULL ||
	    (error = elf_errno()) != ELF_E_ARGUMENT)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

	(void) elf_end(e);

	tet_result(result);
}

/*
 * elf_newscn() on a valid elf file succeeds.
 */
undefine(`FN')
define(`FN',`
void
tcElfSuccess$1$2(void)
{
	Elf *e;
	Elf_Scn *scn;
	int fd, result;
	size_t oldn, newn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: elf_newscn(read-only-elf) succeeds.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "newscn.$2$1", ELF_C_READ, fd, goto done;);

	if (elf_getshnum(e, &oldn) == 0) {
		TP_UNRESOLVED("elf_getshnum(old) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_newscn(e)) == NULL) {
		TP_FAIL("elf_newscn() failed: error=\"%s\".",
		    elf_errmsg(-1));
		result = TET_FAIL;
		goto done;
	}

	if (elf_getshnum(e, &newn) == 0) {
		TP_UNRESOLVED("elf_getshnum(new) failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;
	if (newn != (oldn + 1))
		TP_FAIL("newn %d != oldn %d + 1.", newn, oldn);

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

/*
 * elf_newscn() sets the dirty bit on the new descriptor.
 */
undefine(`FN')
define(`FN',`
void
tcAllocateDirty$1$2(void)
{
	Elf *e;
	Elf_Scn *scn;
	int fd, result;
	unsigned int flags;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: newly returned section is \"dirty\".");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, "newscn.$2$1", ELF_C_READ, fd, goto done;);

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: error=\"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;
	if ((flags = elf_flagscn(scn, ELF_C_SET, 0U)) != ELF_F_DIRTY)
		TP_FAIL("flags=0x%x != 0x%x.", flags, ELF_F_DIRTY);

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

/*
 * elf_newscn() on a file with lacking an ehdr fails.
 */

void
tcAllocateNoEhdr(void)
{
	Elf *e;
	Elf_Scn *scn;
	int error, fd, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("elf_newscn() fails without an EHdr.");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	result = TET_PASS;
	if ((scn = elf_newscn(e)) != NULL ||
	    (error = elf_errno()) != ELF_E_CLASS)
		TP_FAIL("scn=%p error=%d \"%s\".", (void *) scn,
		    error, elf_errmsg(error));

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}

/*
 * elf_newscn() on a new file returns a section descriptor with index 1.
 */
undefine(`FN')
define(`FN',`
void
tcAllocateNew$1$2(void)
{
	Elf *e;
	Elf_Scn *scn;
	Elf$1_Ehdr *eh;
	int fd, result;
	size_t ndx;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("TOUPPER($2)$1: newly returned section is \"dirty\".");

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	_TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd, goto done;);

	if ((eh = elf$1_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf$1_newehdr() failed: error=\"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: error=\"%s\".",
		    elf_errmsg(-1));
		goto done;
	}

	result = TET_PASS;
	if ((ndx = elf_ndxscn(scn)) != 1U)
		TP_FAIL("ndx=%d.", ndx);

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);

	tet_result(result);
}')

FN(32,`lsb')
FN(32,`msb')
FN(64,`lsb')
FN(64,`msb')
