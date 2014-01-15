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
 * $Id: data.m4 223 2008-08-10 15:40:06Z jkoshy $
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
 * Tests for elf_flagdata().
 */

IC_REQUIRES_VERSION_INIT();

/*
 * A Null argument is allowed.
 */

TP_FLAG_NULL(`elf_flagdata')

/* Boilerplate for getting hold of a valid Elf_Data structure. */

define(`_TP_DECLARATIONS',`
	int fd;
	Elf *e;
	Elf32_Ehdr *eh;
	Elf_Scn *scn;
	Elf_Data *d;')
define(`_TP_PROLOGUE',`
	result = TET_UNRESOLVED;
	fd = -1;
	e = NULL;

	TS_OPEN_FILE(e, TS_NEWFILE, ELF_C_WRITE, fd);

	if ((eh = elf32_newehdr(e)) == NULL) {
		TP_UNRESOLVED("elf_newehdr() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((scn = elf_newscn(e)) == NULL) {
		TP_UNRESOLVED("elf_newscn() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}

	if ((d = elf_newdata(scn)) == NULL) {
		TP_UNRESOLVED("elf_newdata() failed: \"%s\".", elf_errmsg(-1));
		goto done;
	}')
define(`_TP_EPILOGUE',`
 done:
	if (e)
		(void) elf_end(e);
	if (fd != -1)
		(void) close(fd);
	(void) unlink(TS_NEWFILE);')

/*
 * An illegal cmd value is rejected.
 */

TP_FLAG_ILLEGAL_CMD(`elf_flagdata',`d')

/*
 * Legal cmd values are allowed.
 */
TP_FLAG_CLR(`elf_flagdata',`d')
TP_FLAG_SET(`elf_flagdata',`d')

/*
 * Illegal flag values are rejected.
 */
TP_FLAG_ILLEGAL_FLAG(`elf_flagdata',`d',`ELF_F_DIRTY')
