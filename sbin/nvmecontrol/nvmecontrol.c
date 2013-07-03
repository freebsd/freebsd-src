/*-
 * Copyright (C) 2012-2013 Intel Corporation
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
#include <sys/ioccom.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "nvmecontrol.h"

static void perftest_usage(void);

static void
usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, DEVLIST_USAGE);
	fprintf(stderr, IDENTIFY_USAGE);
	fprintf(stderr, RESET_USAGE);
	fprintf(stderr, PERFTEST_USAGE);
	exit(EX_USAGE);
}

void
read_controller_data(int fd, struct nvme_controller_data *cdata)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_IDENTIFY;
	pt.cmd.cdw10 = 1;
	pt.buf = cdata;
	pt.len = sizeof(*cdata);
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0) {
		printf("Identify request failed. errno=%d (%s)\n",
		    errno, strerror(errno));
		exit(EX_IOERR);
	}

	if (nvme_completion_is_error(&pt.cpl)) {
		printf("Passthrough command returned error.\n");
		exit(EX_IOERR);
	}
}

void
read_namespace_data(int fd, int nsid, struct nvme_namespace_data *nsdata)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_IDENTIFY;
	pt.cmd.nsid = nsid;
	pt.buf = nsdata;
	pt.len = sizeof(*nsdata);
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0) {
		printf("Identify request failed. errno=%d (%s)\n",
		    errno, strerror(errno));
		exit(EX_IOERR);
	}

	if (nvme_completion_is_error(&pt.cpl)) {
		printf("Passthrough command returned error.\n");
		exit(EX_IOERR);
	}
}

int
open_dev(const char *str, int *fd, int show_error, int exit_on_error)
{
	struct stat	devstat;
	char		full_path[64];

	snprintf(full_path, sizeof(full_path), "/dev/%s", str);
	if (stat(full_path, &devstat) != 0) {
		if (show_error)
			fprintf(stderr, "error\n");
		if (exit_on_error)
			exit(EX_NOINPUT);
		else
			return (EX_NOINPUT);
	}

	*fd = open(full_path, O_RDWR);
	if (*fd < 0) {
		if (show_error)
			printf("Could not open %s. errno=%d (%s)\n", full_path,
			    errno, strerror(errno));
		if (exit_on_error)
			exit(EX_NOPERM);
		else
			return (EX_NOPERM);
	}

	return (EX_OK);
}

static void
print_perftest(struct nvme_io_test *io_test, bool perthread)
{
	uint32_t i, io_completed = 0, iops, mbps;

	for (i = 0; i < io_test->num_threads; i++)
		io_completed += io_test->io_completed[i];

	iops = io_completed/io_test->time;
	mbps = iops * io_test->size / (1024*1024);

	printf("Threads: %2d Size: %6d %5s Time: %3d IO/s: %7d MB/s: %4d\n",
	    io_test->num_threads, io_test->size,
	    io_test->opc == NVME_OPC_READ ? "READ" : "WRITE",
	    io_test->time, iops, mbps);

	if (perthread)
		for (i = 0; i < io_test->num_threads; i++)
			printf("\t%3d: %8d IO/s\n", i,
			    io_test->io_completed[i]/io_test->time);

	exit(1);
}

static void
perftest_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, PERFTEST_USAGE);
	exit(EX_USAGE);
}

static void
perftest(int argc, char *argv[])
{
	struct nvme_io_test		io_test;
	int				fd;
	char				ch;
	char				*p;
	u_long				ioctl_cmd = NVME_IO_TEST;
	bool				nflag, oflag, sflag, tflag;
	int				perthread = 0;

	nflag = oflag = sflag = tflag = false;

	memset(&io_test, 0, sizeof(io_test));

	while ((ch = getopt(argc, argv, "f:i:n:o:ps:t:")) != -1) {
		switch (ch) {
		case 'f':
			if (!strcmp(optarg, "refthread"))
				io_test.flags |= NVME_TEST_FLAG_REFTHREAD;
			break;
		case 'i':
			if (!strcmp(optarg, "bio") ||
			    !strcmp(optarg, "wait"))
				ioctl_cmd = NVME_BIO_TEST;
			else if (!strcmp(optarg, "io") ||
				 !strcmp(optarg, "intr"))
				ioctl_cmd = NVME_IO_TEST;
			break;
		case 'n':
			nflag = true;
			io_test.num_threads = strtoul(optarg, &p, 0);
			if (p != NULL && *p != '\0') {
				fprintf(stderr,
				    "\"%s\" not valid number of threads.\n",
				    optarg);
				perftest_usage();
			} else if (io_test.num_threads == 0 ||
				   io_test.num_threads > 128) {
				fprintf(stderr,
				    "\"%s\" not valid number of threads.\n",
				    optarg);
				perftest_usage();
			}
			break;
		case 'o':
			oflag = true;
			if (!strcmp(optarg, "read") || !strcmp(optarg, "READ"))
				io_test.opc = NVME_OPC_READ;
			else if (!strcmp(optarg, "write") ||
				 !strcmp(optarg, "WRITE"))
				io_test.opc = NVME_OPC_WRITE;
			else {
				fprintf(stderr, "\"%s\" not valid opcode.\n",
				    optarg);
				perftest_usage();
			}
			break;
		case 'p':
			perthread = 1;
			break;
		case 's':
			sflag = true;
			io_test.size = strtoul(optarg, &p, 0);
			if (p == NULL || *p == '\0' || toupper(*p) == 'B') {
				// do nothing
			} else if (toupper(*p) == 'K') {
				io_test.size *= 1024;
			} else if (toupper(*p) == 'M') {
				io_test.size *= 1024 * 1024;
			} else {
				fprintf(stderr, "\"%s\" not valid size.\n",
				    optarg);
				perftest_usage();
			}
			break;
		case 't':
			tflag = true;
			io_test.time = strtoul(optarg, &p, 0);
			if (p != NULL && *p != '\0') {
				fprintf(stderr,
				    "\"%s\" not valid time duration.\n",
				    optarg);
				perftest_usage();
			}
			break;
		}
	}

	if (!nflag || !oflag || !sflag || !tflag || optind >= argc)
		perftest_usage();

	open_dev(argv[optind], &fd, 1, 1);
	if (ioctl(fd, ioctl_cmd, &io_test) < 0) {
		fprintf(stderr, "NVME_IO_TEST failed. errno=%d (%s)\n", errno,
		    strerror(errno));
		close(fd);
		exit(EX_IOERR);
	}

	close(fd);
	print_perftest(&io_test, perthread);
	exit(EX_OK);
}

static void
reset_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, RESET_USAGE);
	exit(EX_USAGE);
}

static void
reset_ctrlr(int argc, char *argv[])
{
	int	ch, fd;

	while ((ch = getopt(argc, argv, "")) != -1) {
		switch ((char)ch) {
		default:
			reset_usage();
		}
	}

	open_dev(argv[optind], &fd, 1, 1);
	if (ioctl(fd, NVME_RESET_CONTROLLER) < 0) {
		printf("Reset request to %s failed. errno=%d (%s)\n",
		    argv[optind], errno, strerror(errno));
		exit(EX_IOERR);
	}

	exit(EX_OK);
}

int
main(int argc, char *argv[])
{

	if (argc < 2)
		usage();

	if (strcmp(argv[1], "devlist") == 0)
		devlist(argc-1, &argv[1]);
	else if (strcmp(argv[1], "identify") == 0)
		identify(argc-1, &argv[1]);
	else if (strcmp(argv[1], "perftest") == 0)
		perftest(argc-1, &argv[1]);
	else if (strcmp(argv[1], "reset") == 0)
		reset_ctrlr(argc-1, &argv[1]);

	usage();

	return (0);
}

