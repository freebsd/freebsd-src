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
 * $Id: elfts-openfile.c 1192 2010-09-12 05:40:00Z jkoshy $
 */

#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <string.h>

#include "tet_api.h"

#include "elfts.h"

/*
 * A TET startup() function for test cases that need elf_version()
 * to be called before each invocable component.
 */

Elf *
elfts_open_file(const char *fn, Elf_Cmd cmd, int *fdp)
{
	Elf *e;
	int fd, mode;

	switch (cmd) {
	case ELF_C_WRITE:
		mode = O_WRONLY | O_CREAT;
		break;
	case ELF_C_READ:
		mode = O_RDONLY;
		break;
	case ELF_C_RDWR:
		mode = O_RDWR;
		break;
	default:
		tet_printf("internal error: unknown cmd=%d.", cmd);
		return (NULL);
	}

	if ((fd = open(fn, mode, 0644)) < 0) {
		tet_printf("setup: open \"%s\" failed: %s.", fn,
		    strerror(errno));
		return (NULL);
	}

	if (fdp)
		*fdp = fd;

	if ((e = elf_begin(fd, cmd, NULL)) == NULL) {
		tet_printf("setup: elf_begin(%s) failed: %s.", fn,
		    elf_errmsg(-1));
		tet_result(TET_UNRESOLVED);
	}

	return (e);
}
