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
 * $Id: ehdr.m4 3174 2015-03-27 17:13:41Z emaste $
 */

include(`elfts.m4')

#include <gelf.h>
#include <libelf.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "tet_api.h"

#include "elfts.h"

#include "gelf_ehdr_template.h"


void
tcNullGelfGetNullElf(void)
{
	int result;
	GElf_Ehdr dst;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_getehdr(NULL,*) fails with ELF_E_ARGUMENT");

	result = TET_PASS;
	if (gelf_getehdr(NULL,&dst) != NULL || elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;
	tet_result(result);
}

void
tcNullGelfGetNullDst(void)
{
	Elf *e;
	int fd;
	char *fn = "ehdr.msb32";

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_getehdr(*,NULL) fails with ELF_E_ARGUMENT");

	TS_OPEN_FILE(e,fn,ELF_C_READ,fd);

	if (gelf_getehdr(e, NULL) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);

	(void) elf_end(e);
	(void) close(fd);
}

void
tcNonElfFails(void)
{
	Elf *e;
	GElf_Ehdr d;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_getehdr(E) for non-ELF (E) fails with "
	    "ELF_E_ARGUMENT");

	TS_OPEN_MEMORY(e,data);

	if (gelf_getehdr(e, &d) != NULL ||
	    elf_errno() != ELF_E_ARGUMENT)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);

	(void) elf_end(e);
}

void
tcBadElfVersion(void)
{
	int err;
	Elf *e;
	void *eh;
	GElf_Ehdr d;
	char badelf[sizeof(badelftemplate)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_getehdr() on an ELF object with an unsupported "
	    "version fails with ELF_E_VERSION");

	(void) memcpy(badelf, badelftemplate, sizeof(badelf));

	badelf[EI_VERSION] = EV_NONE;
	badelf[EI_CLASS] = ELFCLASS32;
	badelf[EI_DATA]  = ELFDATA2MSB;

	TS_OPEN_MEMORY(e,badelf);

	if ((eh = gelf_getehdr(e, &d)) != NULL ||
	    (err = elf_errno()) != ELF_E_VERSION) {
		tet_printf("fail: error=%d eh=%p.", err, (void *) eh);
		tet_result(TET_FAIL);
	} else
		tet_result(TET_PASS);

	(void) elf_end(e);
}

void
tcMalformedElf(void)
{
	int err;
	Elf *e;
	void *eh;
	GElf_Ehdr d;
	char badelf[sizeof(badelftemplate)];

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_getehdr() on a malformed ELF object fails with "
	    "ELF_E_HEADER");

	(void) memcpy(badelf, badelftemplate, sizeof(badelf));
	badelf[EI_VERSION] = EV_CURRENT;
	badelf[EI_CLASS]   = ELFCLASS32;
	badelf[EI_DATA]	   = ELFDATA2MSB;

	TS_OPEN_MEMORY(e, badelf);

	if ((eh = gelf_getehdr(e, &d)) != NULL ||
	    (err = elf_errno()) != ELF_E_HEADER) {
		tet_printf("fail: error=%d eh=%p.", err, (void *) eh);
		tet_result(TET_FAIL);
	} else
		tet_result(TET_PASS);

	(void) elf_end(e);
}

static char *filenames[] = {
	"ehdr.lsb32",
	"ehdr.msb32",
	"ehdr.lsb64",
	"ehdr.msb64",
	NULL
};

void
tcGoodElfValid(void)
{
	int fd, result;
	GElf_Ehdr d1, *eh;
	Elf *e;
	char *fn;
	int i, data, class;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("a successful gelf_getehdr() call returns "
	    "a pointer to the passed in structure, filled with the correct "
	    "contents");

	result = TET_PASS;
	e = NULL;
	fd = -1;

	for (i = 0; i < 4; i++) {
		fn = filenames[i];

		TS_OPEN_FILE(e, fn, ELF_C_READ, fd);

		if ((eh = gelf_getehdr(e, &d1)) == NULL) {
			tet_printf("fail: gelf_getehdr(%s): %s.", *fn,
			    elf_errmsg(-1));
			result = TET_FAIL;
			goto done;
		}

		if (eh != &d1) {
			tet_printf("fail: gelf_getehdr() return != argument.");
			result = TET_FAIL;
			goto done;
		}

		data = (i & 1) ? ELFDATA2MSB : ELFDATA2LSB;
		class = (i <= 1) ? ELFCLASS32 : ELFCLASS64;

		CHECK_EHDR(eh, data, class);

		if (result != TET_PASS)
			break;

		(void) elf_end(e);	e = NULL;
		(void) close(fd);	fd = -1;
	}

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	tet_result(result);
}

void
tcDupCalls(void)
{
	int fd, result;
	Elf *e;
	GElf_Ehdr d1, d2;
	GElf_Ehdr *eh1, *eh2;
	char **fn;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("successful calls to gelf_getehdr() for the same object "
	    "return identical contents");

	result = TET_PASS;
	e = NULL;
	fd = -1;

	for (fn = filenames; *fn; fn++) {
		TS_OPEN_FILE(e,*fn,ELF_C_READ,fd);

		if ((eh1 = gelf_getehdr(e, &d1)) == NULL ||
		    (eh2 = gelf_getehdr(e, &d2)) == NULL) {
			tet_printf("unresolved: gelf_getehdr(%s) failed.",
			    *fn);
			result = TET_UNRESOLVED;
			goto done;
		}

		COMPARE_EHDR(*fn, d1, d2);

		if (result != TET_PASS)
			goto done;

		(void) elf_end(e);	e = NULL;
		(void) close(fd);	fd = -1;
	}

 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);

	tet_result(result);

}

