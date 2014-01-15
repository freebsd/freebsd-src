/*-
 * Copyright (c) 2006 Joseph Koshy
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
 * $Id: getclass.m4 1358 2011-01-08 05:40:41Z jkoshy $
 */

#include <fcntl.h>
#include <gelf.h>
#include <unistd.h>

#include "elfts.h"
#include "tet_api.h"

/*
 * Test the `gelf_getclass' entry point.
 */

IC_REQUIRES_VERSION_INIT();

void
tp_null(void)
{
	tet_infoline("assertion: gelf_getclass(NULL) should return "
	    "ELFCLASSNONE.");

	tet_result (gelf_getclass(NULL) != ELFCLASSNONE ? TET_FAIL : TET_PASS);
}

void
tp_class32(void)
{
	Elf *e;
	int fd;

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: gelf_getclass() return ELFCLASS32 for a "
	    "32 bit ELF file.");

	TS_OPEN_FILE(e,"getclass.msb32",ELF_C_READ,fd);

	tet_result (gelf_getclass(e) != ELFCLASS32 ? TET_FAIL : TET_PASS);

	(void) elf_end(e);
	(void) close(fd);
}

void
tp_class64(void)
{
	Elf *e;
	int fd;

	TP_CHECK_INITIALIZATION();

	tet_infoline("assertion: gelf_getclass() return ELFCLASS64 for a "
	    "64 bit ELF file.");

	TS_OPEN_FILE(e,"getclass.msb64",ELF_C_READ,fd);

	tet_result (gelf_getclass(e) != ELFCLASS64 ? TET_FAIL : TET_PASS);

	(void) elf_end(e);
	(void) close(fd);
}
