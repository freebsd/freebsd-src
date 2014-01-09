/*-
 * Copyright (c) 2012 Robert N. M. Watson
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

#include <machine/cheri.h>
#include <machine/sysarch.h>

#include <cheri/cheri_memcpy.h>

#include <magic.h>
#include <stdlib.h>

#include "minifile.h"

int	invoke(register_t a0, register_t a1, register_t a2, register_t a3);

/*
 * Sandboxed magic_buffer invocation
 * 
 * a0 holds length of the output capabilty, a1 holds the length of the
 * magic data, and a2 holds the length of the input file buffer.  a3
 * indicates if timing data should be collected.
 */
int
invoke(register_t a0, register_t a1, register_t a2, register_t a3)
{
	int ret = 0;
	size_t outsize, magicsize, filesize;
	char *filebuf;
	const char *type, *errstr;
	void *magicbuf;
	magic_t magic;
	int dotimings;
	uint32_t timings[4];

	outsize = a0;
	magicsize = a1;
	filesize = a2;
	dotimings = a3;

	if (dotimings)
		timings[0] = sysarch(MIPS_GET_COUNT, NULL);

	if ((magicbuf = malloc(magicsize)) == NULL)
		return (-1);
	memcpy_fromcap(magicbuf, MINIFILE_MAGIC_CAP, 0, magicsize);
	magic = magic_open(MAGIC_MIME_TYPE);
        if (magic == NULL)
		return (-1);
        if (magic_load_buffers(magic, &magicbuf, &magicsize, 1) == -1) {
                magic_close(magic);
                return (-1);
        }

	if ((filebuf = malloc(filesize)) == NULL)
		return (-1);
	memcpy_fromcap(filebuf, MINIFILE_FILE_CAP, 0, filesize);

	if (dotimings)
		timings[1] = sysarch(MIPS_GET_COUNT, NULL);

        type = magic_buffer(magic, filebuf, filesize);
	if (type == NULL) {
		ret = -1;
		errstr = magic_error(magic);
		type = (errstr == NULL ? "badmagic" : errstr);
	}

	if (dotimings)
		timings[2] = sysarch(MIPS_GET_COUNT, NULL);

	memcpy_tocap(MINIFILE_OUT_CAP, type, 0, MIN(strlen(type) + 1, outsize));

	if (dotimings) {
		timings[3] = sysarch(MIPS_GET_COUNT, NULL);

		memcpy_tocap(MINIFILE_TIMING_CAP, timings, 0,
		    (4 * sizeof(uint32_t)));
	}

	return (ret);
}
