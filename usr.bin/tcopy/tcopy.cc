/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Poul-Henning Kamp, <phk@FreeBSD.org>
 * Copyright (c) 1985, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/endian.h>
#include <sys/mtio.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <libutil.h>

#define	MAXREC	(1024 * 1024)
#define	NOCOUNT	(-2)

enum operation {READ, VERIFY, COPY, COPYVERIFY};

// Stuff the tape_devs need to know about
static int filen;
static uint64_t	record;

//---------------------------------------------------------------------

class tape_dev {
	size_t	max_read_size;
public:
	int fd;
	char *name;
	enum direction {SRC, DST} direction;

	tape_dev(int file_handle, const char *spec, bool destination);

	virtual ssize_t read_blk(void *dst, size_t len);
	virtual ssize_t verify_blk(void *dst, size_t len, size_t expected);
	virtual void write_blk(const void *src, size_t len);
	virtual void file_mark(void);
	virtual void rewind(void);
};

tape_dev::tape_dev(int file_handle, const char *spec, bool destination)
{
	assert(file_handle >= 0);
	fd = file_handle;
	name = strdup(spec);
	assert(name != NULL);
	direction = destination ? DST : SRC;
	max_read_size = 0;
}

ssize_t
tape_dev::read_blk(void *dst, size_t len)
{
	ssize_t retval = -1;

	if (max_read_size == 0) {
		max_read_size = len;
		while (max_read_size > 0) {
			retval = read(fd, dst, max_read_size);
			if (retval >= 0 || (errno != EINVAL && errno != EFBIG))
				break;
			if (max_read_size < 512)
				errx(1, "Cannot find a sane max blocksize");

			// Reduce to next lower power of two
			int i = flsl((long)max_read_size - 1L);
			max_read_size = 1UL << (i - 1);
		}
	} else {
		retval = read(fd, dst, (size_t)max_read_size);
	}
	if (retval < 0) {
		err(1, "read error, %s, file %d, record %ju",
		    name, filen, (uintmax_t)record);
	}
	return (retval);
}

ssize_t
tape_dev::verify_blk(void *dst, size_t len, size_t expected)
{
	(void)expected;
	return read_blk(dst, len);
}

void
tape_dev::write_blk(const void *src, size_t len)
{
	assert(len > 0);
	ssize_t nwrite = write(fd, src, len);
	if (nwrite < 0 || (size_t) nwrite != len) {
		if (nwrite == -1) {
			warn("write error, file %d, record %ju",
			    filen, (intmax_t)record);
		} else {
			warnx("write error, file %d, record %ju",
			    filen, (intmax_t)record);
			warnx("write (%zd) != read (%zd)", nwrite, len);
		}
		errx(5, "copy aborted");
	}
	return;
}

void
tape_dev::file_mark(void)
{
	struct mtop op;

	op.mt_op = MTWEOF;
	op.mt_count = (daddr_t)1;
	if (ioctl(fd, MTIOCTOP, (char *)&op) < 0)
		err(6, "tape op (write file mark)");
}

void
tape_dev::rewind(void)
{
	struct mtop op;

	op.mt_op = MTREW;
	op.mt_count = (daddr_t)1;
	if (ioctl(fd, MTIOCTOP, (char *)&op) < 0)
		err(6, "tape op (rewind)");
}

//---------------------------------------------------------------------

class tap_file: public tape_dev {
public:
	tap_file(int file_handle, const char *spec, bool dst) :
		tape_dev(file_handle, spec, dst) {};
	ssize_t read_blk(void *dst, size_t len);
	void write_blk(const void *src, size_t len);
	void file_mark(void);
	virtual void rewind(void);
};

static
ssize_t full_read(int fd, void *dst, size_t len)
{
	// Input may be a socket which returns partial reads

	ssize_t retval = read(fd, dst, len);
	if (retval <= 0 || (size_t)retval == len)
		return (retval);

	char *ptr = (char *)dst + retval;
	size_t left = len - (size_t)retval;
	while (left > 0) {
		retval = read(fd, ptr, left);
		if (retval <= 0)
			return (retval);
		left -= (size_t)retval;
		ptr += retval;
	}
	return ((ssize_t)len);
}

ssize_t
tap_file::read_blk(void *dst, size_t len)
{
	char lbuf[4];

	ssize_t nread = full_read(fd, lbuf, sizeof lbuf);
	if (nread == 0)
		return (0);

	if ((size_t)nread != sizeof lbuf)
		err(EX_DATAERR, "Corrupt tap-file, read hdr1=%zd", nread);

	uint32_t u = le32dec(lbuf);
	if (u == 0 || (u >> 24) == 0xff)
		return(0);

	if (u > len)
		err(17, "tapfile blocksize too big, 0x%08x", u);

	size_t alen = (u + 1) & ~1;
	assert (alen <= len);

	ssize_t retval = full_read(fd, dst, alen);
	if (retval < 0 || (size_t)retval != alen)
		err(EX_DATAERR, "Corrupt tap-file, read data=%zd", retval);

	nread = full_read(fd, lbuf, sizeof lbuf);
	if ((size_t)nread != sizeof lbuf)
		err(EX_DATAERR, "Corrupt tap-file, read hdr2=%zd", nread);

	uint32_t v = le32dec(lbuf);
	if (u == v)
		return (u);
	err(EX_DATAERR,
	    "Corrupt tap-file, headers differ (0x%08x != 0x%08x)", u, v);
}

void
tap_file::write_blk(const void *src, size_t len)
{
	struct iovec iov[4];
	uint8_t zero = 0;
	int niov = 0;
	size_t expect = 0;
	char tbuf[4];

	assert((len & ~0xffffffffULL) == 0);
	le32enc(tbuf, (uint32_t)len);

	iov[niov].iov_base = tbuf;
	iov[niov].iov_len = sizeof tbuf;
	expect += iov[niov].iov_len;
	niov += 1;

	iov[niov].iov_base = (void*)(uintptr_t)src;
	iov[niov].iov_len = len;
	expect += iov[niov].iov_len;
	niov += 1;

	if (len & 1) {
		iov[niov].iov_base = &zero;
		iov[niov].iov_len = 1;
		expect += iov[niov].iov_len;
		niov += 1;
	}

	iov[niov].iov_base = tbuf;
	iov[niov].iov_len = sizeof tbuf;
	expect += iov[niov].iov_len;
	niov += 1;

	ssize_t nwrite = writev(fd, iov, niov);
	if (nwrite < 0 || (size_t)nwrite != expect)
		errx(17, "write error (%zd != %zd)", nwrite, expect);
}

void
tap_file::file_mark(void)
{
	char tbuf[4];
	le32enc(tbuf, 0);
	ssize_t nwrite = write(fd, tbuf, sizeof tbuf);
	if ((size_t)nwrite != sizeof tbuf)
		errx(17, "write error (%zd != %zd)", nwrite, sizeof tbuf);
}

void
tap_file::rewind(void)
{
	off_t where;
	if (direction == DST) {
		char tbuf[4];
		le32enc(tbuf, 0xffffffff);
		ssize_t nwrite = write(fd, tbuf, sizeof tbuf);
		if ((size_t)nwrite != sizeof tbuf)
			errx(17,
			    "write error (%zd != %zd)", nwrite, sizeof tbuf);
	}
	where = lseek(fd, 0L, SEEK_SET);
	if (where != 0 && errno == ESPIPE)
		err(EX_USAGE, "Cannot rewind sockets and pipes");
	if (where != 0)
		err(17, "lseek(0) failed");
}

//---------------------------------------------------------------------

class file_set: public tape_dev {
public:
	file_set(int file_handle, const char *spec, bool dst) :
		tape_dev(file_handle, spec, dst) {};
	ssize_t read_blk(void *dst, size_t len);
	ssize_t verify_blk(void *dst, size_t len, size_t expected);
	void write_blk(const void *src, size_t len);
	void file_mark(void);
	void rewind(void);
	void open_next(bool increment);
};

void
file_set::open_next(bool increment)
{
	if (fd >= 0) {
		assert(close(fd) >= 0);
		fd = -1;
	}
	if (increment) {
		char *p = strchr(name, '\0') - 3;
		if (++p[2] == '9') {
			p[2] = '0';
			if (++p[1] == '9') {
				p[1] = '0';
				if (++p[0] == '9') {
					errx(EX_USAGE,
					    "file-set sequence overflow");
				}
			}
		}
	}
	if (direction == DST) {
		fd = open(name, O_RDWR|O_CREAT, DEFFILEMODE);
		if (fd < 0)
			err(1, "Could not open %s", name);
	} else {
		fd = open(name, O_RDONLY, 0);
	}
}

ssize_t
file_set::read_blk(void *dst, size_t len)
{
	(void)dst;
	(void)len;
	errx(EX_SOFTWARE, "That was not supposed to happen");
}

ssize_t
file_set::verify_blk(void *dst, size_t len, size_t expected)
{
	(void)len;
	if (fd < 0)
		open_next(true);
	if (fd < 0)
		return (0);
	ssize_t retval = read(fd, dst, expected);
	if (retval == 0) {
		assert(close(fd) >= 0);
		fd = -1;
	}
	return (retval);
}

void
file_set::write_blk(const void *src, size_t len)
{
	if (fd < 0)
		open_next(true);
	ssize_t nwrite = write(fd, src, len);
	if (nwrite < 0 || (size_t)nwrite != len)
		errx(17, "write error (%zd != %zd)", nwrite, len);
}

void
file_set::file_mark(void)
{
	if (fd < 0)
		return;

	off_t where = lseek(fd, 0UL, SEEK_CUR);

	int i = ftruncate(fd, where);
	if (i < 0)
		errx(17, "truncate error, %s to %jd", name, (intmax_t)where);
	assert(close(fd) >= 0);
	fd = -1;
}

void
file_set::rewind(void)
{
	char *p = strchr(name, '\0') - 3;
	p[0] = '0';
	p[1] = '0';
	p[2] = '0';
	open_next(false);
}

//---------------------------------------------------------------------

class flat_file: public tape_dev {
public:
	flat_file(int file_handle, const char *spec, bool dst) :
		tape_dev(file_handle, spec, dst) {};
	ssize_t read_blk(void *dst, size_t len);
	ssize_t verify_blk(void *dst, size_t len, size_t expected);
	void write_blk(const void *src, size_t len);
	void file_mark(void);
	virtual void rewind(void);
};

ssize_t
flat_file::read_blk(void *dst, size_t len)
{
	(void)dst;
	(void)len;
	errx(EX_SOFTWARE, "That was not supposed to happen");
}

ssize_t
flat_file::verify_blk(void *dst, size_t len, size_t expected)
{
	(void)len;
	return (read(fd, dst, expected));
}

void
flat_file::write_blk(const void *src, size_t len)
{
	ssize_t nwrite = write(fd, src, len);
	if (nwrite < 0 || (size_t)nwrite != len)
		errx(17, "write error (%zd != %zd)", nwrite, len);
}

void
flat_file::file_mark(void)
{
	return;
}

void
flat_file::rewind(void)
{
	errx(EX_SOFTWARE, "That was not supposed to happen");
}

//---------------------------------------------------------------------

enum e_how {H_INPUT, H_OUTPUT, H_VERIFY};

static tape_dev *
open_arg(const char *arg, enum e_how how, int rawfile)
{
	int fd;

	if (!strcmp(arg, "-") && how == H_OUTPUT)
		fd = STDOUT_FILENO;
	else if (!strcmp(arg, "-"))
		fd = STDIN_FILENO;
	else if (how == H_OUTPUT)
		fd = open(arg, O_RDWR|O_CREAT, DEFFILEMODE);
	else
		fd = open(arg, O_RDONLY);

	if (fd < 0)
		err(EX_NOINPUT, "Cannot open %s:", arg);

	struct mtop mt;
	mt.mt_op = MTNOP;
	mt.mt_count = 1;
	int i = ioctl(fd, MTIOCTOP, &mt);

	if (i >= 0)
		return (new tape_dev(fd, arg, how == H_OUTPUT));

	size_t alen = strlen(arg);
	if (alen >= 5 && !strcmp(arg + (alen - 4), ".000")) {
		if (how == H_INPUT)
			errx(EX_USAGE,
			    "File-sets files cannot be used as source");
		return (new file_set(fd, arg, how == H_OUTPUT));
	}

	if (how != H_INPUT && rawfile)
		return (new flat_file(fd, arg, how == H_OUTPUT));

	return (new tap_file(fd, arg, how == H_OUTPUT));
}

//---------------------------------------------------------------------

static tape_dev *input;
static tape_dev *output;

static size_t maxblk = MAXREC;
static uint64_t	lastrec, fsize, tsize;
static FILE *msg;
static ssize_t lastnread;
static struct timespec t_start, t_end;

static void
report_total(FILE *file)
{
	double dur = (t_end.tv_nsec - t_start.tv_nsec) * 1e-9;
	dur += t_end.tv_sec - t_start.tv_sec;
	uintmax_t tot = tsize + fsize;
	fprintf(file, "total length: %ju bytes", tot);
	fprintf(file, " time: %.0f s", dur);
	tot /= 1024;
	fprintf(file, " rate: %.1f kB/s", (double)tot/dur);
	fprintf(file, "\n");
}

static void
sigintr(int signo __unused)
{
	(void)signo;
	(void)clock_gettime(CLOCK_MONOTONIC, &t_end);
	if (record) {
		if (record - lastrec > 1)
			fprintf(msg, "records %ju to %ju\n",
			    (intmax_t)lastrec, (intmax_t)record);
		else
			fprintf(msg, "record %ju\n", (intmax_t)lastrec);
	}
	fprintf(msg, "interrupt at file %d: record %ju\n",
	    filen, (uintmax_t)record);
	report_total(msg);
	exit(1);
}

#ifdef SIGINFO
static volatile sig_atomic_t want_info;

static void
siginfo(int signo)
{
	(void)signo;
	want_info = 1;
}

static void
check_want_info(void)
{
	if (want_info) {
		(void)clock_gettime(CLOCK_MONOTONIC, &t_end);
		fprintf(stderr, "tcopy: file %d record %ju ",
		    filen, (uintmax_t)record);
		report_total(stderr);
		want_info = 0;
	}
}

#else /* !SIGINFO */

static void
check_want_info(void)
{
}

#endif

static char *
getspace(size_t blk)
{
	void *bp;

	assert(blk > 0);
	if ((bp = malloc(blk)) == NULL)
		errx(11, "no memory");
	return ((char *)bp);
}

static void
usage(void)
{
	fprintf(stderr, "usage: tcopy [-cvx] [-s maxblk] [src [dest]]\n");
	exit(1);
}

static void
progress(ssize_t nread)
{
	if (nread != lastnread) {
		if (lastnread != 0 && lastnread != NOCOUNT) {
			if (lastrec == 0 && nread == 0)
				fprintf(msg, "%ju records\n",
				    (uintmax_t)record);
			else if (record - lastrec > 1)
				fprintf(msg, "records %ju to %ju\n",
				    (uintmax_t)lastrec,
				    (uintmax_t)record);
			else
				fprintf(msg, "record %ju\n",
				    (uintmax_t)lastrec);
		}
		if (nread != 0)
			fprintf(msg,
			    "file %d: block size %zd: ", filen, nread);
		(void) fflush(msg);
		lastrec = record;
	}
	if (nread > 0) {
		fsize += (size_t)nread;
		record++;
	} else {
		if (lastnread <= 0 && lastnread != NOCOUNT) {
			fprintf(msg, "eot\n");
			return;
		}
		fprintf(msg,
		    "file %d: eof after %ju records: %ju bytes\n",
		    filen, (uintmax_t)record, (uintmax_t)fsize);
		filen++;
		tsize += fsize;
		fsize = record = lastrec = 0;
		lastnread = 0;
	}
	lastnread = nread;
}

static void
read_or_copy(void)
{
	int needeof;
	ssize_t nread, prev_read;
	char *buff = getspace(maxblk);

	(void)clock_gettime(CLOCK_MONOTONIC, &t_start);
	needeof = 0;
	for (prev_read = NOCOUNT;;) {
		check_want_info();
		nread = input->read_blk(buff, maxblk);
		progress(nread);
		if (nread > 0) {
			if (output != NULL) {
				if (needeof) {
					output->file_mark();
					needeof = 0;
				}
				output->write_blk(buff, (size_t)nread);
			}
		} else {
			if (prev_read <= 0 && prev_read != NOCOUNT) {
				break;
			}
			needeof = 1;
		}
		prev_read = nread;
	}
	(void)clock_gettime(CLOCK_MONOTONIC, &t_end);
	report_total(msg);
	free(buff);
}

static void
verify(void)
{
	char *buf1 = getspace(maxblk);
	char *buf2 = getspace(maxblk);
	int eot = 0;
	ssize_t nread1, nread2;
	filen = 0;
	tsize = 0;

	assert(output != NULL);
	(void)clock_gettime(CLOCK_MONOTONIC, &t_start);

	while (1) {
		check_want_info();
		nread1 = input->read_blk(buf1, (size_t)maxblk);
		nread2 = output->verify_blk(buf2, maxblk, (size_t)nread1);
		progress(nread1);
		if (nread1 != nread2) {
			fprintf(msg,
			    "tcopy: tapes have different block sizes; "
			    "%zd != %zd.\n", nread1, nread2);
			exit(1);
		}
		if (nread1 > 0 && memcmp(buf1, buf2, (size_t)nread1)) {
			fprintf(msg, "tcopy: tapes have different data.\n");
			exit(1);
		} else if (nread1 > 0) {
			eot = 0;
		} else if (eot++) {
			break;
		}
	}
	(void)clock_gettime(CLOCK_MONOTONIC, &t_end);
	report_total(msg);
	fprintf(msg, "tcopy: tapes are identical.\n");
	fprintf(msg, "rewinding\n");
	input->rewind();
	output->rewind();

	free(buf1);
	free(buf2);
}

int
main(int argc, char *argv[])
{
	enum operation op = READ;
	int ch;
	unsigned long maxphys = 0;
	size_t l_maxphys = sizeof maxphys;
	int64_t tmp;
	int rawfile = 0;

	setbuf(stderr, NULL);

	if (!sysctlbyname("kern.maxphys", &maxphys, &l_maxphys, NULL, 0UL))
		maxblk = maxphys;

	msg = stdout;
	while ((ch = getopt(argc, argv, "cl:rs:vx")) != -1)
		switch((char)ch) {
		case 'c':
			op = COPYVERIFY;
			break;
		case 'l':
			msg = fopen(optarg, "w");
			if (msg == NULL)
				errx(EX_CANTCREAT, "Cannot open %s", optarg);
			setbuf(msg, NULL);
			break;
		case 'r':
			rawfile = 1;
			break;
		case 's':
			if (expand_number(optarg, &tmp)) {
				warnx("illegal block size");
				usage();
			}
			if (tmp <= 0) {
				warnx("illegal block size");
				usage();
			}
			maxblk = tmp;
			break;
		case 'v':
			op = VERIFY;
			break;
		case 'x':
			if (msg == stdout)
				msg = stderr;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch(argc) {
	case 0:
		if (op != READ)
			usage();
		break;
	case 1:
		if (op != READ)
			usage();
		break;
	case 2:
		if (op == READ)
			op = COPY;
		if (!strcmp(argv[1], "-")) {
			if (op == COPYVERIFY)
				errx(EX_USAGE,
				    "Cannot copy+verify with '-' destination");
			if (msg == stdout)
				msg = stderr;
		}
		if (op == VERIFY)
			output = open_arg(argv[1], H_VERIFY, 0);
		else
			output = open_arg(argv[1], H_OUTPUT, rawfile);
		break;
	default:
		usage();
	}

	if (argc == 0) {
		input = open_arg(_PATH_DEFTAPE, H_INPUT, 0);
	} else {
		input = open_arg(argv[0], H_INPUT, 0);
	}

	if ((signal(SIGINT, SIG_IGN)) != SIG_IGN)
		(void) signal(SIGINT, sigintr);

#ifdef SIGINFO
	(void)signal(SIGINFO, siginfo);
#endif

	if (op != VERIFY) {
		if (op == COPYVERIFY) {
			assert(output != NULL);
			fprintf(msg, "rewinding\n");
			input->rewind();
			output->rewind();
		}

		read_or_copy();

		if (op == COPY || op == COPYVERIFY) {
			assert(output != NULL);
			output->file_mark();
			output->file_mark();
		}
	}

	if (op == VERIFY || op == COPYVERIFY) {

		if (op == COPYVERIFY) {
			assert(output != NULL);
			fprintf(msg, "rewinding\n");
			input->rewind();
			output->rewind();
			input->direction = tape_dev::SRC;
			output->direction = tape_dev::SRC;
		}

		verify();
	}

	if (msg != stderr && msg != stdout)
		report_total(stderr);

	return(0);
}
