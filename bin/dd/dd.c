/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego and Lance
 * Visser of Convex Computer Corporation.
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
 *
 *	$Id: dd.c,v 1.5 1995/10/23 21:31:48 ache Exp $
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)dd.c	8.5 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dd.h"
#include "extern.h"

static void dd_close __P((void));
static void dd_in __P((void));
static void getfdtype __P((IO *));
static void setup __P((void));

IO	in, out;		/* input/output state */
STAT	st;			/* statistics */
void	(*cfunc) __P((void));	/* conversion function */
u_long	cpy_cnt;		/* # of blocks to copy */
u_int	ddflags;		/* conversion options */
u_int	cbsz;			/* conversion block size */
u_int	files_cnt = 1;		/* # of files to copy */
u_char	*ctab;			/* conversion table */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	(void)setlocale(LC_CTYPE, "");
	jcl(argv);
	setup();

	(void)signal(SIGINFO, summaryx);
	(void)signal(SIGINT, terminate);

	atexit(summary);

	while (files_cnt--)
		dd_in();

	dd_close();
	exit(0);
}

static void
setup()
{
	u_int cnt;
	struct timeval tv;

	if (in.name == NULL) {
		in.name = "stdin";
		in.fd = STDIN_FILENO;
	} else {
		in.fd = open(in.name, O_RDONLY, 0);
		if (in.fd < 0)
			err(1, "%s", in.name);
	}

	getfdtype(&in);

	if (files_cnt > 1 && !(in.flags & ISTAPE))
		errx(1, "files is not supported for non-tape devices");

	if (out.name == NULL) {
		/* No way to check for read access here. */
		out.fd = STDOUT_FILENO;
		out.name = "stdout";
	} else {
#define	OFLAGS \
    (O_CREAT | (ddflags & (C_SEEK | C_NOTRUNC) ? 0 : O_TRUNC))
		out.fd = open(out.name, O_RDWR | OFLAGS, DEFFILEMODE);
		/*
		 * May not have read access, so try again with write only.
		 * Without read we may have a problem if output also does
		 * not support seeks.
		 */
		if (out.fd < 0) {
			out.fd = open(out.name, O_WRONLY | OFLAGS, DEFFILEMODE);
			out.flags |= NOREAD;
		}
		if (out.fd < 0)
			err(1, "%s", out.name);
	}

	getfdtype(&out);

	/*
	 * Allocate space for the input and output buffers.  If not doing
	 * record oriented I/O, only need a single buffer.
	 */
	if (!(ddflags & (C_BLOCK|C_UNBLOCK))) {
		if ((in.db = malloc(out.dbsz + in.dbsz - 1)) == NULL)
			err(1, NULL);
		out.db = in.db;
	} else if ((in.db =
	    malloc((u_int)(MAX(in.dbsz, cbsz) + cbsz))) == NULL ||
	    (out.db = malloc((u_int)(out.dbsz + cbsz))) == NULL)
		err(1, NULL);
	in.dbp = in.db;
	out.dbp = out.db;

	/* Position the input/output streams. */
	if (in.offset)
		pos_in();
	if (out.offset)
		pos_out();

	/*
	 * Truncate the output file; ignore errors because it fails on some
	 * kinds of output files, tapes, for example.
	 */
	if (ddflags & (C_OF | C_SEEK | C_NOTRUNC) == (C_OF | C_SEEK))
		(void)ftruncate(out.fd, (off_t)out.offset * out.dbsz);

	/*
	 * If converting case at the same time as another conversion, build a
	 * table that does both at once.  If just converting case, use the
	 * built-in tables.
	 */
	if (ddflags & (C_LCASE|C_UCASE))
		if (ddflags & C_ASCII)
			if (ddflags & C_LCASE) {
				for (cnt = 0; cnt <= 0377; ++cnt)
					if (isupper(ctab[cnt]))
						ctab[cnt] = tolower(ctab[cnt]);
			} else {
				for (cnt = 0; cnt <= 0377; ++cnt)
					if (islower(ctab[cnt]))
						ctab[cnt] = toupper(ctab[cnt]);
			}
		else if (ddflags & C_EBCDIC)
			if (ddflags & C_LCASE) {
				for (cnt = 0; cnt <= 0377; ++cnt)
					if (isupper(cnt))
						ctab[cnt] = ctab[tolower(cnt)];
			} else {
				for (cnt = 0; cnt <= 0377; ++cnt)
					if (islower(cnt))
						ctab[cnt] = ctab[toupper(cnt)];
			}
		else {
			ctab = ddflags & C_LCASE ? u2l : l2u;
			if (ddflags & C_LCASE) {
				for (cnt = 0; cnt <= 0377; ++cnt)
					if (isupper(cnt))
						ctab[cnt] = tolower(cnt);
					else
						ctab[cnt] = cnt;
			} else {
				for (cnt = 0; cnt <= 0377; ++cnt)
					if (islower(cnt))
						ctab[cnt] = toupper(cnt);
					else
						ctab[cnt] = cnt;
			}
		}
	(void)gettimeofday(&tv, (struct timezone *)NULL);
	st.start = tv.tv_sec + tv.tv_usec * 1e-6; 
}

static void
getfdtype(io)
	IO *io;
{
	struct mtget mt;
	struct stat sb;

	if (fstat(io->fd, &sb))
		err(1, "%s", io->name);
	if (S_ISCHR(sb.st_mode))
		io->flags |= ioctl(io->fd, MTIOCGET, &mt) ? ISCHR : ISTAPE;
	else if (lseek(io->fd, (off_t)0, SEEK_CUR) == -1 && errno == ESPIPE)
		io->flags |= ISPIPE;		/* XXX fixed in 4.4BSD */
}

static void
dd_in()
{
	int flags, n;

	for (flags = ddflags;;) {
		if (cpy_cnt && (st.in_full + st.in_part) >= cpy_cnt)
			return;

		/*
		 * Zero the buffer first if trying to recover from errors so
		 * lose the minimum amount of data.  If doing block operations
		 * use spaces.
		 */
		if ((flags & (C_NOERROR|C_SYNC)) == (C_NOERROR|C_SYNC))
			if (flags & (C_BLOCK|C_UNBLOCK))
				memset(in.dbp, ' ', in.dbsz);
			else
				memset(in.dbp, 0, in.dbsz);

		n = read(in.fd, in.dbp, in.dbsz);
		if (n == 0) {
			in.dbrcnt = 0;
			return;
		}

		/* Read error. */
		if (n < 0) {
			/*
			 * If noerror not specified, die.  POSIX requires that
			 * the warning message be followed by an I/O display.
			 */
			if (!(flags & C_NOERROR))
				err(1, "%s", in.name);
			warn("%s", in.name);
			summary();

			/*
			 * If it's not a tape drive or a pipe, seek past the
			 * error.  If your OS doesn't do the right thing for
			 * raw disks this section should be modified to re-read
			 * in sector size chunks.
			 */
			if (!(in.flags & (ISPIPE|ISTAPE)) &&
			    lseek(in.fd, (off_t)in.dbsz, SEEK_CUR))
				warn("%s", in.name);

			/* If sync not specified, omit block and continue. */
			if (!(ddflags & C_SYNC))
				continue;

			/* Read errors count as full blocks. */
			in.dbcnt += in.dbrcnt = in.dbsz;
			++st.in_full;

		/* Handle full input blocks. */
		} else if (n == in.dbsz) {
			in.dbcnt += in.dbrcnt = n;
			++st.in_full;

		/* Handle partial input blocks. */
		} else {
			/* If sync, use the entire block. */
			if (ddflags & C_SYNC)
				in.dbcnt += in.dbrcnt = in.dbsz;
			else
				in.dbcnt += in.dbrcnt = n;
			++st.in_part;
		}

		/*
		 * POSIX states that if bs is set and no other conversions
		 * than noerror, notrunc or sync are specified, the block
		 * is output without buffering as it is read.
		 */
		if (ddflags & C_BS) {
			out.dbcnt = in.dbcnt;
			dd_out(1);
			in.dbcnt = 0;
			continue;
		}

		if (ddflags & C_SWAB) {
			if ((n = in.dbcnt) & 1) {
				++st.swab;
				--n;
			}
			swab(in.dbp, in.dbp, n);
		}

		in.dbp += in.dbrcnt;
		(*cfunc)();
	}
}

/*
 * Cleanup any remaining I/O and flush output.  If necesssary, output file
 * is truncated.
 */
static void
dd_close()
{
	if (cfunc == def)
		def_close();
	else if (cfunc == block)
		block_close();
	else if (cfunc == unblock)
		unblock_close();
	if (ddflags & C_OSYNC && out.dbcnt < out.dbsz) {
		memset(out.dbp, 0, out.dbsz - out.dbcnt);
		out.dbcnt = out.dbsz;
	}
	if (out.dbcnt)
		dd_out(1);
}

void
dd_out(force)
	int force;
{
	static int warned;
	int cnt, n, nw;
	u_char *outp;

	/*
	 * Write one or more blocks out.  The common case is writing a full
	 * output block in a single write; increment the full block stats.
	 * Otherwise, we're into partial block writes.  If a partial write,
	 * and it's a character device, just warn.  If a tape device, quit.
	 *
	 * The partial writes represent two cases.  1: Where the input block
	 * was less than expected so the output block was less than expected.
	 * 2: Where the input block was the right size but we were forced to
	 * write the block in multiple chunks.  The original versions of dd(1)
	 * never wrote a block in more than a single write, so the latter case
	 * never happened.
	 *
	 * One special case is if we're forced to do the write -- in that case
	 * we play games with the buffer size, and it's usually a partial write.
	 */
	outp = out.db;
	for (n = force ? out.dbcnt : out.dbsz;; n = out.dbsz) {
		for (cnt = n;; cnt -= nw) {
			nw = write(out.fd, outp, cnt);
			if (nw <= 0) {
				if (nw == 0)
					errx(1, "%s: end of device", out.name);
				if (errno != EINTR)
					err(1, "%s", out.name);
				nw = 0;
			}
			outp += nw;
			st.bytes += nw;
			if (nw == n) {
				if (n != out.dbsz)
					++st.out_part;
				else
					++st.out_full;
				break;
			}
			++st.out_part;
			if (nw == cnt)
				break;
			if (out.flags & ISCHR && !warned) {
				warned = 1;
				warnx("%s: short write on character device",
				    out.name);
			}
			if (out.flags & ISTAPE)
				errx(1, "%s: short write on tape device", out.name);
		}
		if ((out.dbcnt -= n) < out.dbsz)
			break;
	}

	/* Reassemble the output block. */
	if (out.dbcnt)
		memmove(out.db, out.dbp - out.dbcnt, out.dbcnt);
	out.dbp = out.db + out.dbcnt;
}
