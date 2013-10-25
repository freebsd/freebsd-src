/*-
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/param.h>
#include <sys/capability.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <machine/sysarch.h>

#include <err.h>
#include <magic.h>
#include <stdlib.h>
#define _WITH_DPRINTF
#include <stdio.h>
#include <unistd.h>

#include "minifile.h"

int
main(int argc, char **argv __unused)
{
	size_t filesize;
	char *filebuf;
	const char *type;
	void *magicbuf;
	struct stat filesb, magicsb;
	magic_t magic;
	uint32_t timing[4];

	timing[0] = sysarch(MIPS_GET_COUNT, NULL);

	if (cap_enter() == -1)
		err(1, "cap_enter");

	if (argc > 1)
		errx(1, "too many argumets");

	if (fstat(MINIFILE_FILE_FD, &filesb) == -1)
		err(1, "fstat input fd");
	filesize = MIN(MINIFILE_BUF_MAX, filesb.st_size);
	if ((filebuf = mmap(NULL, filesize, PROT_READ, 0, MINIFILE_FILE_FD,
	    0)) == MAP_FAILED)
		err(1, "mmap input fd");

	if (fstat(MINIFILE_MAGIC_FD, &magicsb) == -1)
		err(1, "fstat magic fd");
	if ((magicbuf = mmap(NULL, magicsb.st_size, PROT_READ|PROT_WRITE,
	    MAP_PRIVATE, MINIFILE_MAGIC_FD, 0)) == MAP_FAILED)
		err(1, "mmap magic fd");

        magic = magic_open(MAGIC_MIME_TYPE);
        if (magic == NULL)
                errx(1, "magic_open()");
        if (magic_load_buffers(magic, &magicbuf, &magicsb.st_size, 1) == -1) {
                warnx("magic_load() %s", magic_error(magic));
                magic_close(magic); 
                exit(1);
        }

	timing[1] = sysarch(MIPS_GET_COUNT, NULL);

	type = magic_buffer(magic, filebuf, filesize);
	timing[2] = sysarch(MIPS_GET_COUNT, NULL);
	/* XXX: idealy would be after the dprintf to capture it's cost */
	timing[3] = sysarch(MIPS_GET_COUNT, NULL);
	write(MINIFILE_OUT_FD, timing, sizeof(uint32_t) * 4);
	dprintf(MINIFILE_OUT_FD, "%s", type != NULL ? type : "badmagic");

	return (0);
}
