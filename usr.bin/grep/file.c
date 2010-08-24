/*	$OpenBSD: file.c,v 1.11 2010/07/02 20:48:48 nicm Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2008-2010 Gabor Kovesdan <gabor@FreeBSD.org>
 * Copyright (C) 2010 Dimitry Andric <dimitry@andric.com>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <bzlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <zlib.h>

#include "grep.h"

#define	MAXBUFSIZ	(32 * 1024)
#define	LNBUFBUMP	80

static gzFile gzbufdesc;
static BZFILE* bzbufdesc;

static unsigned char buffer[MAXBUFSIZ];
static unsigned char *bufpos;
static size_t bufrem;

static unsigned char *lnbuf;
static size_t lnbuflen;

static inline int
grep_refill(struct file *f)
{
	ssize_t nr;
	int bzerr;

	bufpos = buffer;
	bufrem = 0;

	if (filebehave == FILE_GZIP)
		nr = gzread(gzbufdesc, buffer, MAXBUFSIZ);
	else if (filebehave == FILE_BZIP && bzbufdesc != NULL) {
		nr = BZ2_bzRead(&bzerr, bzbufdesc, buffer, MAXBUFSIZ);
		switch (bzerr) {
		case BZ_OK:
		case BZ_STREAM_END:
			/* No problem, nr will be okay */
			break;
		case BZ_DATA_ERROR_MAGIC:
			/*
			 * As opposed to gzread(), which simply returns the
			 * plain file data, if it is not in the correct
			 * compressed format, BZ2_bzRead() instead aborts.
			 *
			 * So, just restart at the beginning of the file again,
			 * and use plain reads from now on.
			 */
			BZ2_bzReadClose(&bzerr, bzbufdesc);
			bzbufdesc = NULL;
			if (lseek(f->fd, 0, SEEK_SET) == -1)
				return (-1);
			nr = read(f->fd, buffer, MAXBUFSIZ);
			break;
		default:
			/* Make sure we exit with an error */
			nr = -1;
		}
	} else
		nr = read(f->fd, buffer, MAXBUFSIZ);

	if (nr < 0)
		return (-1);

	bufrem = nr;
	return (0);
}

static inline int
grep_lnbufgrow(size_t newlen)
{

	if (lnbuflen < newlen) {
		lnbuf = grep_realloc(lnbuf, newlen);
		lnbuflen = newlen;
	}

	return (0);
}

char *
grep_fgetln(struct file *f, size_t *lenp)
{
	unsigned char *p;
	char *ret;
	size_t len;
	size_t off;
	ptrdiff_t diff;

	/* Fill the buffer, if necessary */
	if (bufrem == 0 && grep_refill(f) != 0)
		goto error;

	if (bufrem == 0) {
		/* Return zero length to indicate EOF */
		*lenp = 0;
		return (bufpos);
	}

	/* Look for a newline in the remaining part of the buffer */
	if ((p = memchr(bufpos, '\n', bufrem)) != NULL) {
		++p; /* advance over newline */
		ret = bufpos;
		len = p - bufpos;
		bufrem -= len;
		bufpos = p;
		*lenp = len;
		return (ret);
	}

	/* We have to copy the current buffered data to the line buffer */
	for (len = bufrem, off = 0; ; len += bufrem) {
		/* Make sure there is room for more data */
		if (grep_lnbufgrow(len + LNBUFBUMP))
			goto error;
		memcpy(lnbuf + off, bufpos, len - off);
		off = len;
		if (grep_refill(f) != 0)
			goto error;
		if (bufrem == 0)
			/* EOF: return partial line */
			break;
		if ((p = memchr(bufpos, '\n', bufrem)) == NULL)
			continue;
		/* got it: finish up the line (like code above) */
		++p;
		diff = p - bufpos;
		len += diff;
		if (grep_lnbufgrow(len))
		    goto error;
		memcpy(lnbuf + off, bufpos, diff);
		bufrem -= diff;
		bufpos = p;
		break;
	}
	*lenp = len;
	return (lnbuf);

error:
	*lenp = 0;
	return (NULL);
}

static inline struct file *
grep_file_init(struct file *f)
{

	if (filebehave == FILE_GZIP &&
	    (gzbufdesc = gzdopen(f->fd, "r")) == NULL)
		goto error;

	if (filebehave == FILE_BZIP &&
	    (bzbufdesc = BZ2_bzdopen(f->fd, "r")) == NULL)
		goto error;

	/* Fill read buffer, also catches errors early */
	if (grep_refill(f) != 0)
		goto error;

	/* Check for binary stuff, if necessary */
	if (binbehave != BINFILE_TEXT && memchr(bufpos, '\0', bufrem) != NULL)
		f->binary = true;

	return (f);
error:
	close(f->fd);
	free(f);
	return (NULL);
}

/*
 * Opens a file for processing.
 */
struct file *
grep_open(const char *path)
{
	struct file *f;

	f = grep_malloc(sizeof *f);
	memset(f, 0, sizeof *f);
	if (path == NULL) {
		/* Processing stdin implies --line-buffered. */
		lbflag = true;
		f->fd = STDIN_FILENO;
	} else if ((f->fd = open(path, O_RDONLY)) == -1) {
		free(f);
		return (NULL);
	}

	return (grep_file_init(f));
}

/*
 * Closes a file.
 */
void
grep_close(struct file *f)
{

	close(f->fd);

	/* Reset read buffer and line buffer */
	bufpos = buffer;
	bufrem = 0;

	free(lnbuf);
	lnbuf = NULL;
	lnbuflen = 0;
}
