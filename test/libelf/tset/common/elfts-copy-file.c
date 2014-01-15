/*-
 * Copyright (c) 2010 Joseph Koshy
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
 * $Id: elfts-copy-file.c 2077 2011-10-27 03:59:40Z jkoshy $
 */

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "elfts.h"

/*
 * A helper function to copy a file to a temporary.  Returns the name
 * of the temporary file created.
 */

#define	ELFTS_BUFSIZE		4096
#define	ELFTS_NAME_TEMPLATE	"elftsXXXXXXXX"

char *
elfts_copy_file(const char *rfn, int *error)
{
	int rfd, wfd;
	ssize_t nr, nw, wrem;
	char buf[ELFTS_BUFSIZE], *bp, *wfn;

	*error = 0;
	rfd = wfd = -1;
	bp = wfn = NULL;

	if ((wfn = malloc(sizeof(ELFTS_NAME_TEMPLATE))) == NULL)
		return NULL;

	(void) strcpy(wfn, ELFTS_NAME_TEMPLATE);

	if ((wfd = mkstemp(wfn)) == -1)
		goto error;

	if ((rfd = open(rfn, O_RDONLY)) == -1)
		goto error;

	/*
	 * Copy the bits over.
	 *
	 * Explicitly check for the POSIX `EINTR` error return so that
	 * the code works correctly non-BSD systems.
	 */
	for (;;) {
		if ((nr = read(rfd, buf, sizeof(buf))) < 0) {
			if (errno == EINTR)
				continue;
			goto error;
		}

		if (nr == 0)
			break;	/* EOF */

		for (bp = buf, wrem = nr; wrem > 0; bp += nw, wrem -= nw) {
			if ((nw = write(wfd, bp, wrem)) < 0) {
				if (errno == EINTR)
					continue;
				goto error;
			}
		}
	}

	(void) close(rfd);
	(void) close(wfd);
	return (wfn);

 error:
	*error = errno;

	if (wfd)
		(void) close(wfd);
	if (rfd)
		(void) close(rfd);
	if (wfn) {
		(void) unlink(wfn);
		free(wfn);
	}
	return (NULL);
}

