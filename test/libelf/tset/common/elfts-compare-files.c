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
 * $Id: elfts-compare-files.c 1193 2010-09-12 05:43:52Z jkoshy $
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <libelf.h>
#include <string.h>
#include <unistd.h>

#include "tet_api.h"

/*
 * A helper function to compare a generated file against
 * a reference.
 */

int
elfts_compare_files(const char *rfn, const char *fn)
{
	int fd, result, rfd;
	struct stat sb, rsb;
	char *m, *rm;
	size_t c, nc;

	fd = rfd = -1;
	m = rm = NULL;
	result = TET_UNRESOLVED;

	if ((fd = open(fn, O_RDONLY, 0)) < 0) {
		tet_printf("U: open \"%s\" failed: %s.", fn,
		    strerror(errno));
		goto done;
	}

	if ((rfd = open(rfn, O_RDONLY, 0)) < 0) {
		tet_printf("U: open \"%s\" failed: %s.", rfn,
		    strerror(errno));
		goto done;
	}

	if (fstat(fd, &sb) < 0) {
		tet_printf("U: fstat \"%s\" failed: %s.", fn,
		    strerror(errno));
		goto done;
	}

	if (fstat(rfd, &rsb) < 0) {
		tet_printf("U: fstat \"%s\" failed: %s.", rfn,
		    strerror(errno));
		goto done;
	}

	if (sb.st_size != rsb.st_size) {
		tet_printf("F: refsz(%d) != target(%d).", rsb.st_size, sb.st_size);
		result = TET_FAIL;
		goto done;
	}

	if ((m = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd,
		 (off_t) 0)) == MAP_FAILED) {
		tet_printf("U: mmap \"%s\" failed: %s.", fn,
		    strerror(errno));
		goto done;
	}

	if ((rm = mmap(NULL, rsb.st_size, PROT_READ, MAP_SHARED, rfd,
		 (off_t) 0)) == MAP_FAILED) {
		tet_printf("U: mmap \"%s\" failed: %s.", rfn,
		    strerror(errno));
		goto done;
	}

	result = TET_PASS;
	nc = sb.st_size;

	/* Compare bytes. */
	for (c = 0; c < nc && *m == *rm; c++, m++, rm++)
		;
	if (c != nc) {
		tet_printf("F: @ offset 0x%x ref[%d] != actual[%d].", c,
		     *rm, *m);
		result = TET_FAIL;
	}

 done:
	if (m)
		(void) munmap(m, sb.st_size);
	if (rm)
		(void) munmap(rm, rsb.st_size);
	if (fd != -1)
		(void) close(fd);
	if (rfd != -1)
		(void) close(rfd);
	return (result);

}
