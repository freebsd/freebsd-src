/*	$OpenBSD: file.c,v 1.11 2010/07/02 20:48:48 nicm Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2008-2009 Gabor Kovesdan <gabor@FreeBSD.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <zlib.h>

#include "grep.h"

static char	 fname[MAXPATHLEN];	/* file name */

#define		 MAXBUFSIZ	(16 * 1024)
#define		 PREREAD_M	0.2

/* Some global variables for the buffering and reading. */
static char	*lnbuf;
static size_t	 lnbuflen;
static unsigned char *binbuf;
static int	 binbufsiz;
unsigned char	*binbufptr;
static int	 bzerr;

#define iswbinary(ch)	(!iswspace((ch)) && iswcntrl((ch)) && \
			    (ch != L'\b') && (ch != L'\0'))

/*
 * Returns a single character according to the file type.
 * Returns -1 on failure.
 */
int
grep_fgetc(struct file *f)
{
	unsigned char c;

	switch (filebehave) {
	case FILE_STDIO:
		return (fgetc(f->f));
	case FILE_GZIP:
		return (gzgetc(f->gzf));
	case FILE_BZIP:
		BZ2_bzRead(&bzerr, f->bzf, &c, 1);
		if (bzerr == BZ_STREAM_END)
			return (-1);
		else if (bzerr != BZ_SEQUENCE_ERROR && bzerr != BZ_OK)
			errx(2, "%s", getstr(2));
		return (c);
	}
	return (-1);
}

/*
 * Returns true if the file position is a EOF, returns false
 * otherwise.
 */
int
grep_feof(struct file *f)
{

	switch (filebehave) {
	case FILE_STDIO:
		return (feof(f->f));
	case FILE_GZIP:
		return (gzeof(f->gzf));
	case FILE_BZIP:
		return (bzerr == BZ_STREAM_END);
	}
	return (1);
}

/*
 * At the first call, fills in an internal buffer and checks if the given
 * file is a binary file and sets the binary flag accordingly.  Then returns
 * a single line and sets len to the length of the returned line.
 * At any other call returns a single line either from the internal buffer
 * or from the file if the buffer is exhausted and sets len to the length
 * of the line.
 */
char *
grep_fgetln(struct file *f, size_t *len)
{
	struct stat st;
	size_t bufsiz, i = 0;
	int ch = 0;

	/* Fill in the buffer if it is empty. */
	if (binbufptr == NULL) {

		/* Only pre-read to the buffer if we need the binary check. */
		if (binbehave != BINFILE_TEXT) {
			if (f->stdin)
				st.st_size = MAXBUFSIZ;
			else if (stat(fname, &st) != 0)
				err(2, NULL);

			bufsiz = (MAXBUFSIZ > (st.st_size * PREREAD_M)) ?
			    (st.st_size / 2) : MAXBUFSIZ;

			binbuf = grep_malloc(sizeof(char) * bufsiz);

			while (i < bufsiz) {
				ch = grep_fgetc(f);
				if (ch == EOF)
					break;
				binbuf[i++] = ch;
			}

			f->binary = memchr(binbuf, (filebehave != FILE_GZIP) ?
			    '\0' : '\200', i - 1) != NULL;
		}
		binbufsiz = i;
		binbufptr = binbuf;
	}

	/* Read a line whether from the buffer or from the file itself. */
	for (i = 0; !(grep_feof(f) &&
	    (binbufptr == &binbuf[binbufsiz])); i++) {
		if (binbufptr == &binbuf[binbufsiz]) {
			ch = grep_fgetc(f);
		} else {
			ch = binbufptr[0];
			binbufptr++;
		}
		if (i >= lnbuflen) {
			lnbuflen *= 2;
			lnbuf = grep_realloc(lnbuf, ++lnbuflen);
		}
		if ((ch == '\n') || (ch == EOF)) {
			lnbuf[i] = '\0';
			break;
		} else
			lnbuf[i] = ch;
	}
	if (grep_feof(f) && (i == 0) && (ch != '\n'))
		return (NULL);
	*len = i;
	return (lnbuf);
}

/*
 * Opens the standard input for processing.
 */
struct file *
grep_stdin_open(void)
{
	struct file *f;

	snprintf(fname, sizeof fname, "%s", getstr(1));

	f = grep_malloc(sizeof *f);

	if ((f->f = fdopen(STDIN_FILENO, "r")) != NULL) {
		f->stdin = true;
		return (f);
	}

	free(f);
	return (NULL);
}

/*
 * Opens a normal, a gzipped or a bzip2 compressed file for processing.
 */
struct file *
grep_open(const char *path)
{
	struct file *f;

	snprintf(fname, sizeof fname, "%s", path);

	f = grep_malloc(sizeof *f);

	f->stdin = false;
	switch (filebehave) {
	case FILE_STDIO:
		if ((f->f = fopen(path, "r")) != NULL)
			return (f);
		break;
	case FILE_GZIP:
		if ((f->gzf = gzopen(fname, "r")) != NULL)
			return (f);
		break;
	case FILE_BZIP:
		if ((f->bzf = BZ2_bzopen(fname, "r")) != NULL)
			return (f);
		break;
	}

	free(f);
	return (NULL);
}

/*
 * Closes a normal, a gzipped or a bzip2 compressed file.
 */
void
grep_close(struct file *f)
{

	switch (filebehave) {
	case FILE_STDIO:
		fclose(f->f);
		break;
	case FILE_GZIP:
		gzclose(f->gzf);
		break;
	case FILE_BZIP:
		BZ2_bzclose(f->bzf);
		break;
	}

	/* Reset read buffer for the file we are closing */
	binbufptr = NULL;
	free(binbuf);

}
