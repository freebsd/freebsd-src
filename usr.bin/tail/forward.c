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
#if 0
static char sccsid[] = "@(#)forward.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/event.h>

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

/* defines for inner loop actions */
#define USE_SLEEP	0
#define USE_KQUEUE	1
#define ADD_EVENTS	2

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
	int ch, kq = -1;
	int action = USE_SLEEP;
	struct kevent ev[2];
	struct stat sb2;

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

	if (fflag) {
		kq = kqueue();
		if (kq < 0)
			err(1, "kqueue");
		action = ADD_EVENTS;
	}

	for (;;) {
		while ((ch = getc(fp)) != EOF)
			if (putchar(ch) == EOF)
				oerr();
		if (ferror(fp)) {
			ierr();
			return;
		}
		(void)fflush(stdout);
		if (! fflag)
			break;
		clearerr(fp);

		switch (action) {
		case ADD_EVENTS: {
			int n = 0;
			struct timespec ts = { 0, 0 };

			if (Fflag && fileno(fp) != STDIN_FILENO) {
				EV_SET(&ev[n], fileno(fp), EVFILT_VNODE,
				    EV_ADD | EV_ENABLE | EV_CLEAR,
				    NOTE_DELETE | NOTE_RENAME, 0, 0);
				n++;
			}
			EV_SET(&ev[n], fileno(fp), EVFILT_READ,
			    EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, 0);
			n++;

			if (kevent(kq, ev, n, NULL, 0, &ts) < 0) {
				close(kq);
				kq = -1;
				action = USE_SLEEP;
			} else {
				action = USE_KQUEUE;
			}
			break;
		}

		case USE_KQUEUE:
			if (kevent(kq, NULL, 0, ev, 1, NULL) < 0)
				err(1, "kevent");

			if (ev->filter == EVFILT_VNODE) {
				/* file was rotated, wait until it reappears */
				action = USE_SLEEP;
			} else if (ev->data < 0) {
				/* file shrank, reposition to end */
				if (fseek(fp, 0L, SEEK_END) == -1) {
					ierr();
					return;
				}
			}
			break;

		case USE_SLEEP:
                	(void) usleep(250000);
	                clearerr(fp);

			if (Fflag && fileno(fp) != STDIN_FILENO &&
			    stat(fname, &sb2) != -1) {
				if (sb2.st_ino != sbp->st_ino ||
				    sb2.st_dev != sbp->st_dev ||
				    sb2.st_rdev != sbp->st_rdev ||
				    sb2.st_nlink == 0) {
					fp = freopen(fname, "r", fp);
					if (fp == NULL) {
						ierr();
						break;
					}
					*sbp = sb2;
					if (kq != -1)
						action = ADD_EVENTS;
				} else if (kq != -1) {
					action = USE_KQUEUE;
				}
			}
			break;
		}
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
	struct mapinfo map;
	off_t curoff, size;
	int i;

	if (!(size = sbp->st_size))
		return;
	map.start = NULL;
	map.fd = fileno(fp);
	map.mapoff = map.maxoff = size;

	/*
	 * Last char is special, ignore whether newline or not. Note that
	 * size == 0 is dealt with above, and size == 1 sets curoff to -1.
	 */
	curoff = size - 2;
	while (curoff >= 0) {
		if (curoff < map.mapoff && maparound(&map, curoff) != 0) {
			ierr();
			return;
		}
		for (i = curoff - map.mapoff; i >= 0; i--)
			if (map.start[i] == '\n' && --off == 0)
				break;
		/* `i' is either the map offset of a '\n', or -1. */
		curoff = map.mapoff + i;
		if (i >= 0)
			break;
	}
	curoff++;
	if (mapprint(&map, curoff, size - curoff) != 0) {
		ierr();
		exit(1);
	}

	/* Set the file pointer to reflect the length displayed. */
	if (fseeko(fp, sbp->st_size, SEEK_SET) == -1) {
		ierr();
		return;
	}
	if (map.start != NULL && munmap(map.start, map.maplen)) {
		ierr();
		return;
	}
}
