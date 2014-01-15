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
 * $Id: fsize.m4 1722 2011-08-13 05:34:38Z jkoshy $
 */

include(`elfts.m4')

#include <sys/param.h>

#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <unistd.h>

#include "elfts.h"
#include "tet_api.h"

IC_REQUIRES_VERSION_INIT();

/*
 * Test the `elf[32,64]_fsize' and `gelf_fsize' entry points.
 */

void
tcArgumentGelfNull(void)
{
	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("gelf_fsize(NULL,...) fails with error  ELF_E_ARGUMENT.");

	if (gelf_fsize(NULL, ELF_T_ADDR, 1, EV_CURRENT) != 0 ||
	    elf_errno() != ELF_E_ARGUMENT)
		tet_result(TET_FAIL);
	else
		tet_result(TET_PASS);
}

void
tcArgumentBadVersion(void)
{
	Elf *e;
	int bad_version, fd, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("using an unsupported version number fails with "
	    "ELF_E_VERSION.");

	TS_OPEN_FILE(e,"fsize.msb32",ELF_C_READ,fd);

	bad_version = EV_CURRENT+1;

	result = TET_PASS;

	if (elf32_fsize(ELF_T_ADDR, 1, bad_version) != 0 ||
	    elf_errno() != ELF_E_VERSION)
		result = TET_FAIL;
	else if (elf64_fsize(ELF_T_ADDR, 1, bad_version) != 0 ||
	    elf_errno() != ELF_E_VERSION)
		result = TET_FAIL;
	else if (gelf_fsize(e, ELF_T_ADDR, 1, bad_version) != 0 ||
	    elf_errno() != ELF_E_VERSION)
		result = TET_FAIL;

	tet_result(result);

	(void) elf_end(e);
	(void) close(fd);
}

void
tcArgumentBadTypeTooSmall(void)
{
	Elf *e;
	int fd, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("a type parameter less than 0 fails with "
	    "ELF_E_ARGUMENT.");

	TS_OPEN_FILE(e,"fsize.msb32",ELF_C_READ,fd);

	result = TET_PASS;
	if (elf32_fsize(-1, 1, EV_CURRENT) != 0 ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;
	else if (elf64_fsize(-1, 1, EV_CURRENT) != 0 ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;
	else if (gelf_fsize(e, -1, 1, EV_CURRENT) != 0 ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;

	tet_result(result);
	(void) elf_end(e);
	(void) close(fd);
}

void
tcArgumentBadTypeTooLarge(void)
{
	Elf *e;
	int fd, result;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("a type parameter >= ELF_T_NUM is fails with "
	    "ELF_E_ARGUMENT.");

	TS_OPEN_FILE(e,"fsize.msb32",ELF_C_READ,fd);

	result = TET_PASS;
	if (elf32_fsize(ELF_T_NUM, 1, EV_CURRENT) != 0 ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;
	else if (elf64_fsize(ELF_T_NUM, 1, EV_CURRENT) != 0 ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;
	else if (gelf_fsize(e, ELF_T_NUM, 1, EV_CURRENT) != 0 ||
	    elf_errno() != ELF_E_ARGUMENT)
		result = TET_FAIL;

	tet_result(result);
	(void) elf_end(e);
	(void) close(fd);
}

static size_t sizes32[ELF_T_NUM] = { 
#define	DEFINE_SIZE(N,SZ)	[ELF_T_##N] = (SZ)
	DEFINE_SIZE(ADDR,	4),
	DEFINE_SIZE(BYTE,	1),
	DEFINE_SIZE(CAP,	8),
	DEFINE_SIZE(DYN,	4+4),
	DEFINE_SIZE(EHDR,	16+2+2+4+4+4+4+4+2+2+2+2+2+2),
	DEFINE_SIZE(HALF,	2),
	DEFINE_SIZE(LWORD,	8),
	DEFINE_SIZE(MOVE,	20),
	DEFINE_SIZE(MOVEP,	0),
	DEFINE_SIZE(NOTE,	1),
	DEFINE_SIZE(OFF,	4),
	DEFINE_SIZE(PHDR,	4+4+4+4+4+4+4+4),
	DEFINE_SIZE(REL,	4+4),
	DEFINE_SIZE(RELA,	4+4+4),
	DEFINE_SIZE(SHDR,	4+4+4+4+4+4+4+4+4+4),
	DEFINE_SIZE(SWORD,	4),
	DEFINE_SIZE(SXWORD,	0),
	DEFINE_SIZE(SYM,	4+4+4+1+1+2),
	DEFINE_SIZE(SYMINFO,	4),
	DEFINE_SIZE(VDEF,	1),
	DEFINE_SIZE(VNEED,	1),
	DEFINE_SIZE(WORD,	4),
	DEFINE_SIZE(XWORD,	0),
	DEFINE_SIZE(GNUHASH,	1)
#undef	DEFINE_SIZE
};

void
tcSizesSize32(void)
{
	Elf *e;
	int fd, i;
	size_t size;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("check 32 bit sizes of ELF types");

	TS_OPEN_FILE(e,"fsize.msb32",ELF_C_READ,fd);

	for (i = ELF_T_ADDR; i < ELF_T_NUM; i++) {
		if ((size = elf32_fsize(i, 1, EV_CURRENT)) != sizes32[i]) {
			tet_printf("fail: elf32_fsize(%d): %d != %d",
			    i, size, sizes32[i]);
			tet_result(TET_FAIL);
			return;
		}
		if ((size = gelf_fsize(e, i, 1, EV_CURRENT)) != sizes32[i]) {
			tet_printf("fail: gelf_fsize(%d): %d != %d",
			    i, size, sizes32[i]);
			tet_result(TET_FAIL);
			return;
		}
	}
	tet_result(TET_PASS);
	(void) elf_end(e);
	(void) close(fd);
}

static size_t sizes64[ELF_T_NUM] = {
#define	DEFINE_SIZE(N,SZ)	[ELF_T_##N] = (SZ)
	DEFINE_SIZE(ADDR,	8),
	DEFINE_SIZE(BYTE,	1),
	DEFINE_SIZE(CAP,	16),
	DEFINE_SIZE(DYN,	8+8),
	DEFINE_SIZE(EHDR,	16+2+2+4+8+8+8+4+2+2+2+2+2+2),
	DEFINE_SIZE(HALF,	2),
	DEFINE_SIZE(LWORD,	8),
	DEFINE_SIZE(MOVE,	28),
	DEFINE_SIZE(MOVEP,	0),
	DEFINE_SIZE(NOTE,	1),
	DEFINE_SIZE(OFF,	8),
	DEFINE_SIZE(PHDR,	4+4+8+8+8+8+8+8),
	DEFINE_SIZE(REL,	8+8),
	DEFINE_SIZE(RELA,	8+8+8),
	DEFINE_SIZE(SHDR,	4+4+8+8+8+8+4+4+8+8),
	DEFINE_SIZE(SWORD,	4),
	DEFINE_SIZE(SXWORD,	8),
	DEFINE_SIZE(SYM,	4+1+1+2+8+8),
	DEFINE_SIZE(SYMINFO,	4),
	DEFINE_SIZE(VDEF,	1),
	DEFINE_SIZE(VNEED,	1),
	DEFINE_SIZE(WORD,	4),
	DEFINE_SIZE(XWORD,	8),
	DEFINE_SIZE(GNUHASH,	1)
#undef	DEFINE_SIZE
};

void
tcSizesSize64(void)
{
	Elf *e;
	int fd, i;
	size_t size;

	TP_CHECK_INITIALIZATION();

	TP_ANNOUNCE("check 64 bit sizes of ELF types");

	TS_OPEN_FILE(e,"fsize.msb64",ELF_C_READ,fd);

	for (i = ELF_T_ADDR; i < ELF_T_NUM; i++) {
		if ((size = elf64_fsize(i, 1, EV_CURRENT)) != sizes64[i]) {
			tet_printf("fail: elf64_fsize(%d): %d != %d",
			    i, size, sizes64[i]);
			tet_result(TET_FAIL);
			return;
		}
		if ((size = gelf_fsize(e, i, 1, EV_CURRENT)) != sizes64[i]) {
			tet_printf("fail: gelf_fsize(%d): %d != %d",
			    i, size, sizes64[i]);
			tet_result(TET_FAIL);
			return;
		}
	}
	tet_result(TET_PASS);
	(void) elf_end(e);
	(void) close(fd);

}
