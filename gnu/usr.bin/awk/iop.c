/*
 * iop.c - do i/o related things.
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992, 1993 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Progamming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GAWK; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "awk.h"

#ifndef atarist
#define INVALID_HANDLE (-1)
#else
#include <stddef.h>
#include <fcntl.h>
#define INVALID_HANDLE  (__SMALLEST_VALID_HANDLE - 1)
#endif  /* atarist */


#ifdef TEST
int bufsize = 8192;

void
fatal(s)
char *s;
{
	printf("%s\n", s);
	exit(1);
}
#endif

int
optimal_bufsize(fd)
int fd;
{
	struct stat stb;

#ifdef VMS
	/*
	 * These values correspond with the RMS multi-block count used by
	 * vms_open() in vms/vms_misc.c.
	 */
	if (isatty(fd) > 0)
		return BUFSIZ;
	else if (fstat(fd, &stb) < 0)
		return 8*512;	/* conservative in case of DECnet access */
	else
		return 32*512;

#else
	/*
	 * System V doesn't have the file system block size in the
	 * stat structure. So we have to make some sort of reasonable
	 * guess. We use stdio's BUFSIZ, since that is what it was
	 * meant for in the first place.
	 */
#ifdef BLKSIZE_MISSING
#define	DEFBLKSIZE	BUFSIZ
#else
#define DEFBLKSIZE	(stb.st_blksize ? stb.st_blksize : BUFSIZ)
#endif

#ifdef TEST
	return bufsize;
#else
#ifndef atarist
	if (isatty(fd))
#else
	/*
	 * On ST redirected stdin does not have a name attached
	 * (this could be hard to do to) and fstat would fail
	 */
	if (0 == fd || isatty(fd))
#endif  /*atarist */
		return BUFSIZ;
#ifndef BLKSIZE_MISSING
	/* VMS POSIX 1.0: st_blksize is never assigned a value, so zero it */
	stb.st_blksize = 0;
#endif
	if (fstat(fd, &stb) == -1)
		fatal("can't stat fd %d (%s)", fd, strerror(errno));
	if (lseek(fd, (off_t)0, 0) == -1)
		return DEFBLKSIZE;
	return ((int) (stb.st_size < DEFBLKSIZE ? stb.st_size : DEFBLKSIZE));
#endif	/*! TEST */
#endif	/*! VMS */
}

IOBUF *
iop_alloc(fd)
int fd;
{
	IOBUF *iop;

	if (fd == INVALID_HANDLE)
		return NULL;
	emalloc(iop, IOBUF *, sizeof(IOBUF), "iop_alloc");
	iop->flag = 0;
	if (isatty(fd))
		iop->flag |= IOP_IS_TTY;
	iop->size = optimal_bufsize(fd);
	iop->secsiz = -2;
	errno = 0;
	iop->fd = fd;
	iop->off = iop->buf = NULL;
	iop->cnt = 0;
	return iop;
}

/*
 * Get the next record.  Uses a "split buffer" where the latter part is
 * the normal read buffer and the head part is an "overflow" area that is used
 * when a record spans the end of the normal buffer, in which case the first
 * part of the record is copied into the overflow area just before the
 * normal buffer.  Thus, the eventual full record can be returned as a
 * contiguous area of memory with a minimum of copying.  The overflow area
 * is expanded as needed, so that records are unlimited in length.
 * We also mark both the end of the buffer and the end of the read() with
 * a sentinel character (the current record separator) so that the inside
 * loop can run as a single test.
 */
int
get_a_record(out, iop, grRS, errcode)
char **out;
IOBUF *iop;
register int grRS;
int *errcode;
{
	register char *bp = iop->off;
	char *bufend;
	char *start = iop->off;			/* beginning of record */
	char rs;
	int saw_newline = 0, eat_whitespace = 0;	/* used iff grRS==0 */

	if (iop->cnt == EOF) {	/* previous read hit EOF */
		*out = NULL;
		return EOF;
	}

	if (grRS == 0) {	/* special case:  grRS == "" */
		rs = '\n';
	} else
		rs = (char) grRS;

	/* set up sentinel */
	if (iop->buf) {
		bufend = iop->buf + iop->size + iop->secsiz;
		*bufend = rs;
	} else
		bufend = NULL;

	for (;;) {	/* break on end of record, read error or EOF */

		/* Following code is entered on the first call of this routine
		 * for a new iop, or when we scan to the end of the buffer.
		 * In the latter case, we copy the current partial record to
		 * the space preceding the normal read buffer.  If necessary,
		 * we expand this space.  This is done so that we can return
		 * the record as a contiguous area of memory.
		 */
		if ((iop->flag & IOP_IS_INTERNAL) == 0 && bp >= bufend) {
			char *oldbuf = NULL;
			char *oldsplit = iop->buf + iop->secsiz;
			long len;	/* record length so far */

			len = bp - start;
			if (len > iop->secsiz) {
				/* expand secondary buffer */
				if (iop->secsiz == -2)
					iop->secsiz = 256;
				while (len > iop->secsiz)
					iop->secsiz *= 2;
				oldbuf = iop->buf;
				emalloc(iop->buf, char *,
				    iop->size+iop->secsiz+2, "get_a_record");
				bufend = iop->buf + iop->size + iop->secsiz;
				*bufend = rs;
			}
			if (len > 0) {
				char *newsplit = iop->buf + iop->secsiz;

				if (start < oldsplit) {
					memcpy(newsplit - len, start,
							oldsplit - start);
					memcpy(newsplit - (bp - oldsplit),
							oldsplit, bp - oldsplit);
				} else
					memcpy(newsplit - len, start, len);
			}
			bp = iop->end = iop->off = iop->buf + iop->secsiz;
			start = bp - len;
			if (oldbuf) {
				free(oldbuf);
				oldbuf = NULL;
			}
		}
		/* Following code is entered whenever we have no more data to
		 * scan.  In most cases this will read into the beginning of
		 * the main buffer, but in some cases (terminal, pipe etc.)
		 * we may be doing smallish reads into more advanced positions.
		 */
		if (bp >= iop->end) {
			if ((iop->flag & IOP_IS_INTERNAL) != 0) {
				iop->cnt = EOF;
				break;
			}
			iop->cnt = read(iop->fd, iop->end, bufend - iop->end);
			if (iop->cnt == -1) {
				if (! do_unix && errcode != NULL) {
					*errcode = errno;
					iop->cnt = EOF;
					break;
				} else
					fatal("error reading input: %s",
						strerror(errno));
			} else if (iop->cnt == 0) {
				iop->cnt = EOF;
				break;
			}
			iop->end += iop->cnt;
			*iop->end = rs;
		}
		if (grRS == 0) {
			extern int default_FS;

			if (default_FS && (bp == start || eat_whitespace)) {
				while (bp < iop->end
				  	&& (*bp == ' ' || *bp == '\t' || *bp == '\n'))
					bp++;
				if (bp == iop->end) {
					eat_whitespace = 1;
					continue;
				} else
					eat_whitespace = 0;
			}
			if (saw_newline && *bp == rs) {
				bp++;
				break;
			}
			saw_newline = 0;
		}

		while (*bp++ != rs)
			;

		if (bp <= iop->end) {
			if (grRS == 0)
				saw_newline = 1;
			else
				break;
		} else
			bp--;

		if ((iop->flag & IOP_IS_INTERNAL) != 0)
			iop->cnt = bp - start;
	}
	if (iop->cnt == EOF
	    && (((iop->flag & IOP_IS_INTERNAL) != 0) || start == bp)) {
		*out = NULL;
		return EOF;
	}

	iop->off = bp;
	bp--;
	if (*bp != rs)
		bp++;
	*bp = '\0';
	if (grRS == 0) {
		/* there could be more newlines left, clean 'em out now */
		while (*(iop->off) == rs && iop->off <= iop->end)
			(iop->off)++;

		if (*--bp == rs)
			*bp = '\0';
		else
			bp++;
	}

	*out = start;
	return bp - start;
}

#ifdef TEST
main(argc, argv)
int argc;
char *argv[];
{
	IOBUF *iop;
	char *out;
	int cnt;
	char rs[2];

	rs[0] = 0;
	if (argc > 1)
		bufsize = atoi(argv[1]);
	if (argc > 2)
		rs[0] = *argv[2];
	iop = iop_alloc(0);
	while ((cnt = get_a_record(&out, iop, rs[0], NULL)) > 0) {
		fwrite(out, 1, cnt, stdout);
		fwrite(rs, 1, 1, stdout);
	}
}
#endif
