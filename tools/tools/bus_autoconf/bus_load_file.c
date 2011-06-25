/* $FreeBSD$ */

/*-
 * Copyright (c) 2011 Hans Petter Selasky. All rights reserved.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <err.h>
#include <sysexits.h>
#include <unistd.h>

#include "bus_load_file.h"

void
load_file(const char *fname, uint8_t **pptr, uint32_t *plen)
{
	uint8_t *ptr;
	uint32_t len;
	off_t off;
	int f;

	f = open(fname, O_RDONLY);
	if (f < 0)
		err(EX_NOINPUT, "Cannot open file '%s'", fname);

	off = lseek(f, 0, SEEK_END);
	if (off <= 0)
		err(EX_NOINPUT, "Cannot seek to end of file");

	if (lseek(f, 0, SEEK_SET) < 0)
		err(EX_NOINPUT, "Cannot seek to beginning of file");

	len = off;
	if (len != off)
		err(EX_NOINPUT, "File '%s' is too big", fname);

	ptr = malloc(len);
	if (ptr == NULL)
		errx(EX_SOFTWARE, "Out of memory");

	if (read(f, ptr, len) != len)
		err(EX_NOINPUT, "Cannot read all data");

	close(f);

	*pptr = ptr;
	*plen = len;
}
