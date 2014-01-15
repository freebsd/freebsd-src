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
 * $Id: elf.m4 1599 2011-07-04 03:18:52Z jkoshy $
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "elfts.h"

#include "tet_api.h"

include(`elfts.m4')
include(`elf_flag.m4')

/*
 * Tests for elf_flagelf().
 */

IC_REQUIRES_VERSION_INIT();

TP_FLAG_NULL(`elf_flagelf')

/*
 * Boiler plate to get a valid ELF pointer.
 */
define(`_TP_DECLARATIONS',`
	int fd;
	Elf *e;')
define(`_TP_PROLOGUE',`
	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd);')
define(`_TP_EPILOGUE',`
 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);')

TP_FLAG_NON_ELF(`elf_flagelf')

TP_FLAG_ILLEGAL_CMD(`elf_flagelf',`e')

TP_FLAG_CLR(`elf_flagelf',`e')
TP_FLAG_SET(`elf_flagelf',`e')

TP_FLAG_ILLEGAL_FLAG(`elf_flagelf',`e',
	`ELF_F_DIRTY|ELF_F_LAYOUT|ELF_F_ARCHIVE|ELF_F_ARCHIVE_SYSV')


define(`TS_ARFILE',`"a.ar"')

dnl Helper function.

define(`_FN',`
void
$1(void)
{
	int error, fd, result, ret;
	Elf *e;

	TP_ANNOUNCE($2);

	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	TS_OPEN_FILE(e, $3, $4, fd);

	result = TET_PASS;
	if ((ret = elf_flagelf(e, ELF_C_SET, $5)) != 0 ||
	    (error = elf_errno()) != ELF_E_ARGUMENT) {
		TP_FAIL("ret=%d,error=%d \"%s\".", ret, error,
		    elf_errmsg(error));
		    goto done;
	}

	_TP_EPILOGUE()

	tet_result(result);
}')

/*
 * Attempting to set ELF_F_ARCHIVE on an object opened with ELF_C_READ
 * should fail.
 */
_FN(`tcArgsArchiveFlagOnReadFD',
	`"Setting ELF_F_ARCHIVE on an object opened with "
	 "ELF_C_READ should fail."',
	 TS_ARFILE, ELF_C_READ,
	 ELF_F_ARCHIVE)

/*
 * Attempting to set ELF_F_ARCHIVE_SYSV without ELF_F_ARCHIVE should fail.
 */
_FN(`tcArgsArchiveFlagSysV',
	`"Setting ELF_F_ARCHIVE_SYSV without ELF_F_ARCHIVE should fail."',
	TS_NEWFILE, ELF_C_WRITE,
	ELF_F_ARCHIVE_SYSV)
