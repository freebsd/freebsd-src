/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/capsicum.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/filio.h>
#include <sys/mtio.h>
#include <sys/time.h>

#include <assert.h>
#include <capsicum_helpers.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dd.h"
#include "extern.h"

static void dd_close(void);
static void dd_in(void);
static void getfdtype(IO *);
static void setup(void);

IO	in, out;		/* input/output state */
STAT	st;			/* statistics */
void	(*cfunc)(void);		/* conversion function */
uintmax_t cpy_cnt;		/* # of blocks to copy */
static off_t	pending = 0;	/* pending seek if sparse */
uint64_t	ddflags = 0;	/* conversion options */
size_t	cbsz;			/* conversion block size */
uintmax_t files_cnt = 1;	/* # of files to copy */
const	u_char *ctab;		/* conversion table */
char	fill_char;		/* Character to fill with if defined */
size_t	speed = 0;		/* maximum speed, in bytes per second */
volatile sig_atomic_t need_summary;
volatile sig_atomic_t need_progress;
volatile sig_atomic_t kill_signal;

int
main(int argc __unused, char *argv[])
{
	struct itimerval itv = { { 1, 0 }, { 1, 0 } }; /* SIGALARM every second, if needed */

	prepare_io();

	(void)setlocale(LC_CTYPE, "");
	jcl(argv);
	setup();

	caph_cache_catpages();
	if (caph_enter() < 0)
		err(1, "unable to enter capability mode");

	(void)signal(SIGINFO, siginfo_handler);
	if (ddflags & C_PROGRESS) {
		(void)signal(SIGALRM, sigalarm_handler);
		setitimer(ITIMER_REAL, &itv, NULL);
	}

	atexit(summary);

	while (files_cnt--)
		dd_in();

	dd_close();
	/*
	 * Some devices such as cfi(4) may perform significant amounts
	 * of work when a write descriptor is closed.  Close the out
	 * descriptor explicitly so that the summary handler (called
	 * from an atexit() hook) includes this work.
	 */
	if (close(out.fd) == -1 && errno != EINTR)
		err(1, "close");
	exit(0);
}

static int
parity(u_char c)
{
	int i;

	i = c ^ (c >> 1) ^ (c >> 2) ^ (c >> 3) ^ 
	    (c >> 4) ^ (c >> 5) ^ (c >> 6) ^ (c >> 7);
	return (i & 1);
}

static void
setup(void)
{
	u_int cnt;
	int iflags, oflags;
	cap_rights_t rights;
	unsigned long cmds[] = { FIODTYPE, MTIOCTOP };

	if (in.name == NULL) {
		in.name = "stdin";
		in.fd = STDIN_FILENO;
	} else {
		iflags = 0;
		if (ddflags & C_IDIRECT)
			iflags |= O_DIRECT;
		before_io();
		in.fd = open(in.name, O_RDONLY | iflags, 0);
		after_io();
		if (in.fd == -1)
			err(1, "%s", in.name);
	}

	getfdtype(&in);

	cap_rights_init(&rights, CAP_READ, CAP_SEEK);
	if (caph_rights_limit(in.fd, &rights) == -1)
		err(1, "unable to limit capability rights");

	if (files_cnt > 1 && !(in.flags & ISTAPE))
		errx(1, "files is not supported for non-tape devices");

	cap_rights_set(&rights, CAP_FTRUNCATE, CAP_IOCTL, CAP_WRITE);
	if (ddflags & (C_FDATASYNC | C_FSYNC))
		cap_rights_set(&rights, CAP_FSYNC);
	if (out.name == NULL) {
		/* No way to check for read access here. */
		out.fd = STDOUT_FILENO;
		out.name = "stdout";
		if (ddflags & C_OFSYNC) {
			oflags = fcntl(out.fd, F_GETFL);
			if (oflags == -1)
				err(1, "unable to get fd flags for stdout");
			oflags |= O_FSYNC;
			if (fcntl(out.fd, F_SETFL, oflags) == -1)
				err(1, "unable to set fd flags for stdout");
		}
	} else {
		oflags = O_CREAT;
		if (!(ddflags & (C_SEEK | C_NOTRUNC)))
			oflags |= O_TRUNC;
		if (ddflags & C_OFSYNC)
			oflags |= O_FSYNC;
		if (ddflags & C_ODIRECT)
			oflags |= O_DIRECT;
		before_io();
		out.fd = open(out.name, O_RDWR | oflags, DEFFILEMODE);
		after_io();
		/*
		 * May not have read access, so try again with write only.
		 * Without read we may have a problem if output also does
		 * not support seeks.
		 */
		if (out.fd == -1) {
			before_io();
			out.fd = open(out.name, O_WRONLY | oflags, DEFFILEMODE);
			after_io();
			out.flags |= NOREAD;
			cap_rights_clear(&rights, CAP_READ);
		}
		if (out.fd == -1)
			err(1, "%s", out.name);
	}

	getfdtype(&out);

	if (caph_rights_limit(out.fd, &rights) == -1)
		err(1, "unable to limit capability rights");
	if (caph_ioctls_limit(out.fd, cmds, nitems(cmds)) == -1)
		err(1, "unable to limit capability rights");

	if (in.fd != STDIN_FILENO && out.fd != STDIN_FILENO) {
		if (caph_limit_stdin() == -1)
			err(1, "unable to limit capability rights");
	}

	if (in.fd != STDOUT_FILENO && out.fd != STDOUT_FILENO) {
		if (caph_limit_stdout() == -1)
			err(1, "unable to limit capability rights");
	}

	if (in.fd != STDERR_FILENO && out.fd != STDERR_FILENO) {
		if (caph_limit_stderr() == -1)
			err(1, "unable to limit capability rights");
	}

	/*
	 * Allocate space for the input and output buffers.  If not doing
	 * record oriented I/O, only need a single buffer.
	 */
	if (!(ddflags & (C_BLOCK | C_UNBLOCK))) {
		if ((in.db = malloc((size_t)out.dbsz + in.dbsz - 1)) == NULL)
			err(1, "input buffer");
		out.db = in.db;
	} else if ((in.db = malloc(MAX((size_t)in.dbsz, cbsz) + cbsz)) == NULL ||
	    (out.db = malloc(out.dbsz + cbsz)) == NULL)
		err(1, "output buffer");

	/* dbp is the first free position in each buffer. */
	in.dbp = in.db;
	out.dbp = out.db;

	/* Position the input/output streams. */
	if (in.offset)
		pos_in();
	if (out.offset)
		pos_out();

	/*
	 * Truncate the output file.  If it fails on a type of output file
	 * that it should _not_ fail on, error out.
	 */
	if ((ddflags & (C_OF | C_SEEK | C_NOTRUNC)) == (C_OF | C_SEEK) &&
	    out.flags & ISTRUNC)
		if (ftruncate(out.fd, out.offset * out.dbsz) == -1)
			err(1, "truncating %s", out.name);

	if (ddflags & (C_LCASE  | C_UCASE | C_ASCII | C_EBCDIC | C_PARITY)) {
		if (ctab != NULL) {
			for (cnt = 0; cnt <= 0377; ++cnt)
				casetab[cnt] = ctab[cnt];
		} else {
			for (cnt = 0; cnt <= 0377; ++cnt)
				casetab[cnt] = cnt;
		}
		if ((ddflags & C_PARITY) && !(ddflags & C_ASCII)) {
			/*
			 * If the input is not EBCDIC, and we do parity
			 * processing, strip input parity.
			 */
			for (cnt = 200; cnt <= 0377; ++cnt)
				casetab[cnt] = casetab[cnt & 0x7f];
		}
		if (ddflags & C_LCASE) {
			for (cnt = 0; cnt <= 0377; ++cnt)
				casetab[cnt] = tolower(casetab[cnt]);
		} else if (ddflags & C_UCASE) {
			for (cnt = 0; cnt <= 0377; ++cnt)
				casetab[cnt] = toupper(casetab[cnt]);
		}
		if ((ddflags & C_PARITY)) {
			/*
			 * This should strictly speaking be a no-op, but I
			 * wonder what funny LANG settings could get us.
			 */
			for (cnt = 0; cnt <= 0377; ++cnt)
				casetab[cnt] = casetab[cnt] & 0x7f;
		}
		if ((ddflags & C_PARSET)) {
			for (cnt = 0; cnt <= 0377; ++cnt)
				casetab[cnt] = casetab[cnt] | 0x80;
		}
		if ((ddflags & C_PAREVEN)) {
			for (cnt = 0; cnt <= 0377; ++cnt)
				if (parity(casetab[cnt]))
					casetab[cnt] = casetab[cnt] | 0x80;
		}
		if ((ddflags & C_PARODD)) {
			for (cnt = 0; cnt <= 0377; ++cnt)
				if (!parity(casetab[cnt]))
					casetab[cnt] = casetab[cnt] | 0x80;
		}

		ctab = casetab;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &st.start))
		err(1, "clock_gettime");
}

static void
getfdtype(IO *io)
{
	struct stat sb;
	int type;

	if (fstat(io->fd, &sb) == -1)
		err(1, "%s", io->name);
	if (S_ISREG(sb.st_mode))
		io->flags |= ISTRUNC;
	if (S_ISCHR(sb.st_mode) || S_ISBLK(sb.st_mode)) { 
		if (ioctl(io->fd, FIODTYPE, &type) == -1) {
			err(1, "%s", io->name);
		} else {
			if (type & D_TAPE)
				io->flags |= ISTAPE;
			else if (type & (D_DISK | D_MEM))
				io->flags |= ISSEEK;
			if (S_ISCHR(sb.st_mode) && (type & D_TAPE) == 0)
				io->flags |= ISCHR;
		}
		return;
	}
	errno = 0;
	if (lseek(io->fd, (off_t)0, SEEK_CUR) == -1 && errno == ESPIPE)
		io->flags |= ISPIPE;
	else
		io->flags |= ISSEEK;
}

/*
 * Limit the speed by adding a delay before every block read.
 * The delay (t_usleep) is equal to the time computed from block
 * size and the specified speed limit (t_target) minus the time
 * spent on actual read and write operations (t_io).
 */
static void
speed_limit(void)
{
	static double t_prev, t_usleep;
	double t_now, t_io, t_target;

	t_now = secs_elapsed();
	t_io = t_now - t_prev - t_usleep;
	t_target = (double)in.dbsz / (double)speed;
	t_usleep = t_target - t_io;
	if (t_usleep > 0)
		usleep(t_usleep * 1000000);
	else
		t_usleep = 0;
	t_prev = t_now;
}

static void
swapbytes(void *v, size_t len)
{
	unsigned char *p = v;
	unsigned char t;

	while (len > 1) {
		t = p[0];
		p[0] = p[1];
		p[1] = t;
		p += 2;
		len -= 2;
	}
}

static void
dd_in(void)
{
	ssize_t n;

	for (;;) {
		switch (cpy_cnt) {
		case -1:			/* count=0 was specified */
			return;
		case 0:
			break;
		default:
			if (st.in_full + st.in_part >= (uintmax_t)cpy_cnt)
				return;
			break;
		}

		if (speed > 0)
			speed_limit();

		/*
		 * Zero the buffer first if sync; if doing block operations,
		 * use spaces.
		 */
		if (ddflags & C_SYNC) {
			if (ddflags & C_FILL)
				memset(in.dbp, fill_char, in.dbsz);
			else if (ddflags & (C_BLOCK | C_UNBLOCK))
				memset(in.dbp, ' ', in.dbsz);
			else
				memset(in.dbp, 0, in.dbsz);
		}

		in.dbrcnt = 0;
fill:
		before_io();
		n = read(in.fd, in.dbp + in.dbrcnt, in.dbsz - in.dbrcnt);
		after_io();

		/* EOF */
		if (n == 0 && in.dbrcnt == 0)
			return;

		/* Read error */
		if (n == -1) {
			/*
			 * If noerror not specified, die.  POSIX requires that
			 * the warning message be followed by an I/O display.
			 */
			if (!(ddflags & C_NOERROR))
				err(1, "%s", in.name);
			warn("%s", in.name);
			summary();

			/*
			 * If it's a seekable file descriptor, seek past the
			 * error.  If your OS doesn't do the right thing for
			 * raw disks this section should be modified to re-read
			 * in sector size chunks.
			 */
			if (in.flags & ISSEEK &&
			    lseek(in.fd, (off_t)in.dbsz, SEEK_CUR))
				warn("%s", in.name);

			/* If sync not specified, omit block and continue. */
			if (!(ddflags & C_SYNC))
				continue;
		}

		/* If conv=sync, use the entire block. */
		if (ddflags & C_SYNC)
			n = in.dbsz;

		/* Count the bytes read for this block. */
		in.dbrcnt += n;

		/* Count the number of full and partial blocks. */
		if (in.dbrcnt == in.dbsz)
			++st.in_full;
		else if (ddflags & C_IFULLBLOCK && n != 0)
			goto fill; /* these don't count */
		else
			++st.in_part;

		/* Count the total bytes read for this file. */
		in.dbcnt += in.dbrcnt;

		/*
		 * POSIX states that if bs is set and no other conversions
		 * than noerror, notrunc or sync are specified, the block
		 * is output without buffering as it is read.
		 */
		if ((ddflags & ~(C_NOERROR | C_NOTRUNC | C_SYNC)) == C_BS) {
			out.dbcnt = in.dbcnt;
			dd_out(1);
			in.dbcnt = 0;
			continue;
		}

		if (ddflags & C_SWAB) {
			if ((n = in.dbrcnt) & 1) {
				++st.swab;
				--n;
			}
			swapbytes(in.dbp, (size_t)n);
		}

		/* Advance to the next block. */
		in.dbp += in.dbrcnt;
		(*cfunc)();
		if (need_summary)
			summary();
		if (need_progress)
			progress();
	}
}

/*
 * Clean up any remaining I/O and flush output.  If necessary, the output file
 * is truncated.
 */
static void
dd_close(void)
{
	if (cfunc == def)
		def_close();
	else if (cfunc == block)
		block_close();
	else if (cfunc == unblock)
		unblock_close();
	if (ddflags & C_OSYNC && out.dbcnt && out.dbcnt < out.dbsz) {
		if (ddflags & C_FILL)
			memset(out.dbp, fill_char, out.dbsz - out.dbcnt);
		else if (ddflags & (C_BLOCK | C_UNBLOCK))
			memset(out.dbp, ' ', out.dbsz - out.dbcnt);
		else
			memset(out.dbp, 0, out.dbsz - out.dbcnt);
		out.dbcnt = out.dbsz;
	}
	if (out.dbcnt || pending)
		dd_out(1);

	/*
	 * If the file ends with a hole, ftruncate it to extend its size
	 * up to the end of the hole (without having to write any data).
	 */
	if (out.seek_offset > 0 && (out.flags & ISTRUNC)) {
		if (ftruncate(out.fd, out.seek_offset) == -1)
			err(1, "truncating %s", out.name);
	}

	if (ddflags & C_FSYNC) {
		if (fsync(out.fd) == -1)
			err(1, "fsyncing %s", out.name);
	} else if (ddflags & C_FDATASYNC) {
		if (fdatasync(out.fd) == -1)
			err(1, "fdatasyncing %s", out.name);
	}
}

void
dd_out(int force)
{
	u_char *outp;
	size_t cnt, n;
	ssize_t nw;
	static int warned;
	int sparse;

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

	/*
	 * If force, first try to write all pending data, else try to write
	 * just one block. Subsequently always write data one full block at
	 * a time at most.
	 */
	for (n = force ? out.dbcnt : out.dbsz;; n = out.dbsz) {
		cnt = n;
		do {
			sparse = 0;
			if (ddflags & C_SPARSE) {
				/* Is buffer sparse? */
				sparse = BISZERO(outp, cnt);
			}
			if (sparse && !force) {
				pending += cnt;
				nw = cnt;
			} else {
				if (pending != 0) {
					/*
					 * Seek past hole.  Note that we need to record the
					 * reached offset, because we might have no more data
					 * to write, in which case we'll need to call
					 * ftruncate to extend the file size.
					 */
					out.seek_offset = lseek(out.fd, pending, SEEK_CUR);
					if (out.seek_offset == -1)
						err(2, "%s: seek error creating sparse file",
						    out.name);
					pending = 0;
				}
				if (cnt) {
					before_io();
					nw = write(out.fd, outp, cnt);
					after_io();
					out.seek_offset = 0;
				} else {
					return;
				}
			}

			if (nw <= 0) {
				if (nw == 0)
					errx(1, "%s: end of device", out.name);
				if (errno != EINTR)
					err(1, "%s", out.name);
				nw = 0;
			}

			outp += nw;
			st.bytes += nw;

			if ((size_t)nw == n && n == (size_t)out.dbsz)
				++st.out_full;
			else
				++st.out_part;

			if ((size_t) nw != cnt) {
				if (out.flags & ISTAPE)
					errx(1, "%s: short write on tape device",
				    	out.name);
				if (out.flags & ISCHR && !warned) {
					warned = 1;
					warnx("%s: short write on character device",
				    	out.name);
				}
			}

			cnt -= nw;
		} while (cnt != 0);

		if ((out.dbcnt -= n) < out.dbsz)
			break;
	}

	/* Reassemble the output block. */
	if (out.dbcnt)
		(void)memmove(out.db, out.dbp - out.dbcnt, out.dbcnt);
	out.dbp = out.db + out.dbcnt;
}
