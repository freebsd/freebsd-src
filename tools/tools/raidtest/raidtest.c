/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <inttypes.h>
#include <signal.h>
#include <err.h>
#include <errno.h>
#include <sys/endian.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>


#define	DEFAULT_DATA_FILE	"raidtest.data"
#define	MAX_IO_LENGTH		131072
#define	IO_TYPE_READ	0
#define	IO_TYPE_WRITE	1

struct ioreq {
	off_t		iorq_offset;
	unsigned	iorq_length;
	unsigned	iorq_type;
};

struct iorec {
	uint64_t	iorc_offset;
	uint32_t	iorc_length;
	uint8_t		iorc_type;
};

static void
usage(void)
{

	fprintf(stderr, "usage: %s genfile [-frw] <-s mediasize> [-S sectorsize] <-n nrequests> [file]\n", getprogname());
	fprintf(stderr, "       %s test [-rw] <-d device> [-n processes] [file]\n", getprogname());
	exit(EXIT_FAILURE);
}

static unsigned
gen_size(unsigned secsize)
{
	unsigned maxsec;

	maxsec = MAX_IO_LENGTH / secsize;
	return (secsize * ((arc4random() % maxsec) + 1)); 
}

static int
read_ioreq(int fd, struct ioreq *iorq)
{
	struct iorec iorc;

	if (read(fd, &iorc, sizeof(iorc)) != sizeof(iorc))
		return (1);
	iorq->iorq_offset = le64dec(&iorc.iorc_offset);
	iorq->iorq_length = le32dec(&iorc.iorc_length);
	iorq->iorq_type = iorc.iorc_type;
	return (0);
}

static int
write_ioreq(int fd, struct ioreq *iorq)
{
	struct iorec iorc;

	le64enc(&iorc.iorc_offset, iorq->iorq_offset);
	le32enc(&iorc.iorc_length, iorq->iorq_length);
	iorc.iorc_type = iorq->iorq_type;
	return (write(fd, &iorc, sizeof(iorc)) != sizeof(iorc));
}

static void
raidtest_genfile(int argc, char *argv[])
{
	uintmax_t i, nreqs, mediasize, nsectors, nbytes, nrreqs, nwreqs;
	unsigned secsize, maxsec;
	const char *file = NULL;
	struct ioreq iorq;
	int ch, fd, flags, rdonly, wronly;

	nreqs = 0;
	mediasize = 0;
	secsize = 512;
	rdonly = wronly = 0;
	flags = O_WRONLY | O_CREAT | O_EXCL | O_TRUNC;
	while ((ch = getopt(argc, argv, "fn:rs:S:w")) != -1) {
		switch (ch) {
		case 'f':
			flags &= ~O_EXCL;
			break;
		case 'n':
			errno = 0;
			nreqs = strtoumax(optarg, NULL, 0);
			if (errno != 0) {
				err(EXIT_FAILURE,
				    "Invalid value for '%c' argument.", ch);
			}
			break;
		case 'r':
			rdonly = 1;
			break;
		case 's':
			errno = 0;
			mediasize = strtoumax(optarg, NULL, 0);
			if (errno != 0) {
				err(EXIT_FAILURE,
				    "Invalid value for '%c' argument.", ch);
			}
			break;
		case 'S':
			errno = 0;
			secsize = strtoul(optarg, NULL, 0);
			if (errno != 0) {
				err(EXIT_FAILURE,
				    "Invalid value for '%c' argument.", ch);
			}
			break;
		case 'w':
			wronly = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (nreqs == 0)
		errx(EXIT_FAILURE, "Option '%c' not specified.", 'n');
	if (mediasize == 0)
		errx(EXIT_FAILURE, "Option '%c' not specified.", 's');
	if (rdonly && wronly) {
		errx(EXIT_FAILURE, "Both '%c' and '%c' options were specified.",
		    'r', 'w');
	}
	if (argc == 0)
		file = DEFAULT_DATA_FILE;
	else if (argc == 1)
		file = argv[0];
	else
		usage();
	fd = open(file, flags, 0644);
	if (fd < 0)
		err(EXIT_FAILURE, "Cannot create '%s' file", file);
	nsectors = mediasize / secsize;
	nbytes = nrreqs = nwreqs = 0;
	for (i = 0; i < nreqs; i++) {
		/* Generate I/O request length. */
		iorq.iorq_length = gen_size(secsize);
		/* Generate I/O request offset. */
		maxsec = nsectors - (iorq.iorq_length / secsize);
		iorq.iorq_offset = (arc4random() % maxsec) * secsize;
		/* Generate I/O request type. */
		if (rdonly)
			iorq.iorq_type = IO_TYPE_READ;
		else if (wronly)
			iorq.iorq_type = IO_TYPE_WRITE;
		else
			iorq.iorq_type = arc4random() % 2;
		nbytes += iorq.iorq_length;
		switch (iorq.iorq_type) {
		case IO_TYPE_READ:
			nrreqs++; 
			break;
		case IO_TYPE_WRITE:
			nwreqs++; 
			break;
		}
		if (write_ioreq(fd, &iorq) != 0) {
			unlink(file);
			err(EXIT_FAILURE, "Error while writing");
		}
	}
	printf("File %s generated.\n", file);
	printf("Number of READ requests: %ju.\n", nrreqs);
	printf("Number of WRITE requests: %ju.\n", nwreqs);
	printf("Number of bytes to transmit: %ju.\n", nbytes);
}

static void
test_start(int fd, struct ioreq *iorqs, uintmax_t nreqs)
{
	unsigned char data[MAX_IO_LENGTH];
	struct ioreq *iorq;
	uintmax_t n;

	for (n = 0; n < nreqs; n++) {
		iorq = &iorqs[n];
		switch (iorq->iorq_type) {
		case IO_TYPE_READ:
			if (pread(fd, data, iorq->iorq_length,
			    iorq->iorq_offset) != (ssize_t)iorq->iorq_length) {
				fprintf(stderr,
				    "%u: read(%jd, %u) failed: %s.\n", getpid(),
				    (intmax_t)iorq->iorq_offset,
				    iorq->iorq_length, strerror(errno));
			}
			break;
		case IO_TYPE_WRITE:
			if (pwrite(fd, data, iorq->iorq_length,
			    iorq->iorq_offset) != (ssize_t)iorq->iorq_length) {
				fprintf(stderr,
				    "%u: write(%jd, %u) failed: %s.\n",
				    getpid(), (intmax_t)iorq->iorq_offset,
				    iorq->iorq_length, strerror(errno));
			}
			break;
		default:
			fprintf(stderr, "%u: Invalid request type: %u.\n",
			    getpid(), iorq->iorq_type);
			break;
		}
	}
}

static void
show_stats(long secs, uintmax_t nbytes, uintmax_t nreqs)
{

	printf("Bytes per second: %ju\n", nbytes / secs);
	printf("Requests per second: %ju\n", nreqs / secs);
}

static void
raidtest_test(int argc, char *argv[])
{
	uintmax_t i, nbytes, nreqs, nrreqs, nwreqs, reqs_per_proc, nstart;
	const char *dev, *file = NULL;
	struct timeval tstart, tend;
	struct ioreq *iorqs;
	unsigned nprocs;
	struct stat sb;
	pid_t *procs;
	int ch, fdd, fdf, j, rdonly, wronly;

	dev = NULL;
	nprocs = 1;
	rdonly = wronly = 0;
	while ((ch = getopt(argc, argv, "d:n:rvw")) != -1) {
		switch (ch) {
		case 'd':
			dev = optarg;
			break;
		case 'n':
			errno = 0;
			nprocs = strtoul(optarg, NULL, 0);
			if (errno != 0) {
				err(EXIT_FAILURE,
				    "Invalid value for '%c' argument.", ch);
			}
			break;
		case 'r':
			rdonly = 1;
			break;
		case 'w':
			wronly = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (dev == NULL)
		errx(EXIT_FAILURE, "Option '%c' not specified.", 'd');
	if (nprocs < 1)
		errx(EXIT_FAILURE, "Invalid number of processes");
	if (rdonly && wronly) {
		errx(EXIT_FAILURE, "Both '%c' and '%c' options were specified.",
		    'r', 'w');
	}
	if (argc == 0)
		file = DEFAULT_DATA_FILE;
	else if (argc == 1)
		file = argv[0];
	else
		usage();
	fdf = open(file, O_RDONLY);
	if (fdf < 0)
		err(EXIT_FAILURE, "Cannot open '%s' file", file);
	if (fstat(fdf, &sb) < 0)
		err(EXIT_FAILURE, "Cannot stat '%s' file", file);
	if ((sb.st_size % sizeof(struct iorec)) != 0)
		err(EXIT_FAILURE, "Invalid size of '%s' file", file);
	fdd = open(dev, O_RDWR | O_DIRECT);
	if (fdd < 0)
		err(EXIT_FAILURE, "Cannot open '%s' device", file);
	procs = malloc(sizeof(pid_t) * nprocs);
	if (procs == NULL) {
		close(fdf);
		close(fdd);
		errx(EXIT_FAILURE, "Cannot allocate %u bytes of memory.",
		    sizeof(pid_t) * nprocs);
	}
	iorqs =
	    malloc((sb.st_size / sizeof(struct iorec)) * sizeof(struct ioreq));
	if (iorqs == NULL) {
		close(fdf);
		close(fdd);
		free(procs);
		errx(EXIT_FAILURE, "Cannot allocate %jd bytes of memory.",
		    (intmax_t)(sb.st_size / sizeof(struct iorec)) *
		    sizeof(struct ioreq));
	}
	nreqs = sb.st_size / sizeof(struct iorec);
	nbytes = nrreqs = nwreqs = 0;
	for (i = 0; i < nreqs; i++) {
		if (read_ioreq(fdf, &iorqs[i]))
			err(EXIT_FAILURE, "Error while reading");
		if (rdonly)
			iorqs[i].iorq_type = IO_TYPE_READ;
		else if (wronly)
			iorqs[i].iorq_type = IO_TYPE_WRITE;
		nbytes += iorqs[i].iorq_length;
		switch (iorqs[i].iorq_type) {
		case IO_TYPE_READ:
			nrreqs++;
			break;
		case IO_TYPE_WRITE:
			nwreqs++;
			break;
		default:
			fprintf(stderr, "Invalid request type: %u.\n",
			    iorqs[i].iorq_type);
			break;
		}
	}
	close(fdf);
	printf("Read %ju requests from %s.\n", nreqs, file);
	printf("Number of READ requests: %ju.\n", nrreqs);
	printf("Number of WRITE requests: %ju.\n", nwreqs);
	printf("Number of bytes to transmit: %ju.\n", nbytes);
	printf("Number of processes: %u.\n", nprocs);
	fflush(stdout);
	reqs_per_proc = nreqs / nprocs;
	nstart = 0;
	gettimeofday(&tstart, NULL);
	for (j = 0; j < (int)nprocs; j++) {
		procs[i] = fork();
		switch (procs[i]) {
		case 0:
			free(procs);
			test_start(fdd, &iorqs[nstart], reqs_per_proc);
			free(iorqs);
			close(fdd);
			exit(EXIT_SUCCESS);
		case -1:
			fprintf(stderr, "Cannot create process %u: %s\n",
			    (unsigned)i, strerror(errno));
			for (j--; j >= 0; j--)
				kill(procs[j], SIGKILL);
			free(procs);
			free(iorqs);
			close(fdd);
			exit(EXIT_FAILURE);
		}
		nstart += reqs_per_proc;
	}
	free(iorqs);
	free(procs);
	for (j = 0; j < (int)nprocs; j++) {
		int status;

		wait(&status);
	}
	gettimeofday(&tend, NULL);
	show_stats(tend.tv_sec - tstart.tv_sec, nbytes, nreqs);
}

int
main(int argc, char *argv[])
{

	if (argc < 2)
		usage();
	argc--;
	argv++;
	if (strcmp(argv[0], "genfile") == 0)
		raidtest_genfile(argc, argv);
	else if (strcmp(argv[0], "test") == 0)
		raidtest_test(argc, argv);
	else
		usage();
	exit(EXIT_SUCCESS);
}
