/*-
 * Copyright (c) 2008 Joseph Koshy
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
 * $Id: flagarhdr.m4 2077 2011-10-27 03:59:40Z jkoshy $
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
 * Tests for elf_flagarhdr().
 */

IC_REQUIRES_VERSION_INIT();

/*
 * The following defines to match that in ./Makefile.
 */
define(`TP_ARFILE',`"a.ar"')


/*
 * Boiler plate to get a valid ELF pointer.
 */
define(`_TP_DECLARATIONS',`
	int fd;
	Elf *ar, *member;
	Elf_Arhdr *arh;')
define(`_TP_PROLOGUE',`
	result = TET_UNRESOLVED;
	fd = -1;
	ar = member = NULL;

	TS_OPEN_FILE(ar, TP_ARFILE, ELF_C_READ, fd);

	if ((member = elf_begin(fd, ELF_C_READ, ar)) == NULL) {
	   	TP_UNRESOLVED("retrieval of archive member failed: \"%s\".",
		    elf_errmsg(-1));					   
		goto done;
	}

	if ((arh = elf_getarhdr(member)) == NULL) {
		TP_UNRESOLVED("elf_getarhdr() on member failed: \"%s\".",
		    elf_errmsg(-1));
		goto done;
	}
')
define(`_TP_EPILOGUE',`
 done:
	if (member)
		(void) elf_end(member);
	if (ar)
		(void) elf_end(ar);
	if (fd != -1)
		(void) close(fd);
')

TP_FLAG_NULL(`elf_flagarhdr');
TP_FLAG_ILLEGAL_CMD(`elf_flagarhdr',`arh')
TP_FLAG_CLR(`elf_flagarhdr',`arh')
TP_FLAG_SET(`elf_flagarhdr',`arh')
TP_FLAG_ILLEGAL_FLAG(`elf_flagarhdr',`arh',`ELF_F_DIRTY')
