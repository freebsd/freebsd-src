/*-
 * Copyright (c) 2011 Joseph Koshy
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
 * $Id: getdata.m4 2090 2011-10-27 08:07:39Z jkoshy $
 */

#include <libelf.h>
#include <gelf.h>
#include <string.h>
#include <unistd.h>

#include "elfts.h"
#include "tet_api.h"

include(`elfts.m4')

IC_REQUIRES_VERSION_INIT();

/*
 * Find an ELF section with the given name.
 */
static Elf_Scn *
findscn(Elf *e, const char *name)
{
	size_t shstrndx;
	const char *scn_name;
	Elf_Scn *scn;
	GElf_Shdr shdr;

	/* Locate the string table. */
	if (elf_getshdrstrndx(e, &shstrndx) != 0)
		return (NULL);

	/* Find a section with a matching name. */
	scn = NULL;
	while ((scn = elf_nextscn(e, scn)) != NULL) {
		if (gelf_getshdr(scn, &shdr) == NULL)
			return (NULL);
		if ((scn_name = elf_strptr(e, shstrndx,
		    (size_t) shdr.sh_name)) == NULL)
			return (NULL);
		if (strcmp(scn_name, name) == 0)
			return (scn);
	}

	return (NULL);
}

define(`ZEROSECTION',".zerosection")
undefine(`FN')
define(`FN',`
void
tcZeroSection$1$2(void)
{
	Elf *e;
	int error, fd, result;
	Elf_Scn *scn;
	Elf_Data *ed;

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	TP_ANNOUNCE("a data descriptor for a zero sized section is correctly retrieved");

	_TS_OPEN_FILE(e, "zerosection.$1$2", ELF_C_READ, fd, goto done;);

	if ((scn = findscn(e, ZEROSECTION)) == NULL) {
		TP_UNRESOLVED("Cannot find section \""ZEROSECTION"\"");
		goto done;
	}

	ed = NULL;
	if ((ed = elf_getdata(scn, ed)) == NULL) {
		error = elf_errno();
		TP_FAIL("elf_getdata failed %d \"%s\"", error,
		    elf_errmsg(error));
		goto done;
	}

	if (ed->d_size != 0 || ed->d_buf != NULL) {
		TP_FAIL("Illegal values returned: size %d buf %p",
		    (int) ed->d_size, (void *) ed->d_buf);
		goto done;
	}

	result = TET_PASS;

done:
	if (e)
		elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}
')

FN(lsb,32)
FN(lsb,64)
FN(msb,32)
FN(msb,64)

/*
 * Verify that a non-zero section is correctly read.
 */

static const char stringsection[] = {
changequote({,})
       '\0',
       '.', 's', 'h', 's', 't', 'r', 't', 'a', 'b', '\0',
       '.', 'z', 'e', 'r', 'o', 's', 'e', 'c', 't', 'i', 'o', 'n', '\0'
changequote
       };

undefine(`_FN')
define(`_FN',`
void
tcNonZeroSection$1$2(void)
{
	Elf *e;
	int error, fd, result;
	const size_t strsectionsize = sizeof stringsection;
	size_t n, shstrndx;
	const char *buf;
	Elf_Scn *scn;
	Elf_Data *ed;

	e = NULL;
	fd = -1;
	result = TET_UNRESOLVED;

	TP_ANNOUNCE("a data descriptor for a non-zero sized section "
	    "is correctly retrieved");

	_TS_OPEN_FILE(e, "zerosection.$1$2", ELF_C_READ, fd, goto done;);

	if (elf_getshdrstrndx(e, &shstrndx) != 0 ||
	    (scn = elf_getscn(e, shstrndx)) == NULL) {
		TP_UNRESOLVED("Cannot find string table section");
		goto done;
	}

	ed = NULL;
	if ((ed = elf_getdata(scn, ed)) == NULL) {
		error = elf_errno();
		TP_FAIL("elf_getdata failed %d \"%s\"", error,
		    elf_errmsg(error));
		goto done;
	}

	if (ed->d_size != strsectionsize) {
		TP_FAIL("Illegal values returned: d_size %d != expected %d",
		    (int) ed->d_size, strsectionsize);
		goto done;
	}

	if (memcmp(stringsection, ed->d_buf, strsectionsize) != 0) {
		buf = (const char *) ed->d_buf;
		for (n = 0; n < strsectionsize; n++)
			if (buf[n] != stringsection[n])
				break;
		TP_FAIL("String mismatch: buf[%d] \"%c\" != \"%c\"",
		    n, buf[n], stringsection[n]);
		goto done;
	}

	result = TET_PASS;

done:
	if (e)
		elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}
')

_FN(lsb,32)
_FN(lsb,64)
_FN(msb,32)
_FN(msb,64)
