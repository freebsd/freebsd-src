/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Sze-Tyan Wang.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)forward.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "extern.h"

static void rlines __P((FILE *, long, struct stat *));

/*
 * forward -- display the file, from an offset, forward.
 *
 * There are eight separate cases for this -- regular and non-regular
 * files, by bytes or lines and from the beginning or end of the file.
 *
 * FBYTES	byte offset from the beginning of the file
 *	REG	seek
 *	NOREG	read, counting bytes
 *
 * FLINES	line offset from the beginning of the file
 *	REG	read, counting lines
 *	NOREG	read, counting lines
 *
 * RBYTES	byte offset from the end of the file
 *	REG	seek
 *	NOREG	cyclically read characters into a wrap-around buffer
 *
 * RLINES
 *	REG	mmap the file and step back until reach the correct offset.
 *	NOREG	cyclically read lines into a wrap-around array of buffers
 */
void
forward(fp, style, off, sbp)
	FILE *fp;
	enum STYLE style;
	long off;
	struct stat *sbp;
{
	register int ch;
	struct timeval interval;

	switch(style) {
	case FBYTES:
		if (off == 0)
			break;
		if (S_ISREG(sbp->st_mode)) {
			if (sbp->st_size < off)
				off = sbp->st_size;
			if (fseek(fp, off, SEEK_SET) == -1) {
				ierr();
				return;
			}
		} else while (off--)
			if ((ch = getc(fp)) == EOF) {
				if (ferror(fp)) {
					ierr();
					return;
				}
				break;
			}
		break;
	case FLINES:
		if (off == 0)
			break;
		for (;;) {
			if ((ch = getc(fp)) == EOF) {
				if (ferror(fp)) {
					ierr();
					return;
				}
				break;
			}
			if (ch == '\n' && !--off)
				break;
		}
		break;
	case RBYTES:
		if (S_ISREG(sbp->st_mode)) {
			if (sbp->st_size >= off &&
			    fseek(fp, -off, SEEK_END) == -1) {
				ierr();
				return;
			}
		} else if (off == 0) {
			while (getc(fp) != EOF);
			if (ferror(fp)) {
				ierr();
				return;
			}
		} else
			if (bytes(fp, off))
				return;
		break;
	case RLINES:
		if (S_ISREG(sbp->st_mode))
			if (!off) {
				if (fseek(fp, 0L, SEEK_END) == -1) {
					ierr();
					return;
				}
			} else
				rlines(fp, off, sbp);
		else if (off == 0) {
			while (getc(fp) != EOF);
			if (ferror(fp)) {
				ierr();
				return;
			}
		} else
			if (lines(fp, off))
				return;
		break;
	}

	/*
	 * We pause for 1/4 second after displaying any data that has
	 * accumulated since we read the file.
	 */

	for (;;) {
		while ((ch = getc(fp)) != EOF)
			if (putchar(ch) == EOF)
				oerr();
		if (ferror(fp)) {
			ierr();
			return;
		}
		(void)fflush(stdout);
		if (!fflag)
			break;

		interval.tv_sec = 0;
		interval.tv_usec = 250000;
		if (select(0, NULL, NULL, NULL, &interval) == -1)
			if (errno != EINTR)
				err(1, "select");
		clearerr(fp);
	}
}

/*
 * rlines -- display the last offset lines of the file.
 */
static void
rlines(fp, off, sbp)
	FILE *fp;
	long off;
	struct stat *sbp;
{
	register off_t size;
	register char *p;
	char *start;

	if (!(size = sbp->st_size))
		return;

	if (size > SIZE_T_MAX) {
		errno = EFBIG;
		ierr();
		return;
	}

	if ((start = mmap(NULL, (size_t)size,
	    PROT_READ, MAP_SHARED, fileno(fp), (off_t)0)) == MAP_FAILED) {
		ierr();
		return;
	}

	/* Last char is special, ignore whether newline or not. */
	for (p = start + size - 1; --size;)
		if (*--p == '\n' && !--off) {
			++p;
			break;
		}

	/* Set the file pointer to reflect the length displayed. */
	size = sbp->st_size - size;
	WR(p, size);
	if (fseek(fp, (long)sbp->st_size, SEEK_SET) == -1) {
		ierr();
		return;
	}
	if (munmap(start, (size_t)sbp->st_size)) {
		ierr();
		return;
	}
}
