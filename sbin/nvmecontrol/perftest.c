/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "nvmecontrol.h"

/* Tables for command line parsing */

static cmd_fn_t perftest;

#define NONE 0xffffffffu
static struct options {
	bool		perthread;
	uint32_t	threads;
	uint32_t	size;
	uint32_t	time;
	const char	*op;
	const char	*intr;
	const char	*flags;
	const char	*dev;
} opt = {
	.perthread = false,
	.threads = 0,
	.size = 0,
	.time = 0,
	.op = NULL,
	.intr = NULL,
	.flags = NULL,
	.dev = NULL,
};


static const struct opts perftest_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("perthread", 'p', arg_none, opt, perthread,
	    "Report per-thread results"),
	OPT("threads", 'n', arg_uint32, opt, threads,
	    "Number of threads to run"),
	OPT("size", 's', arg_uint32, opt, size,
	    "Size of the test"),
	OPT("time", 't', arg_uint32, opt, time,
	    "How long to run the test in seconds"),
	OPT("operation", 'o', arg_string, opt, op,
	    "Operation type: 'read' or 'write'"),
	OPT("interrupt", 'i', arg_string, opt, intr,
	    "Interrupt mode: 'intr' or 'wait'"),
	OPT("flags", 'f', arg_string, opt, flags,
	    "Turn on testing flags: refthread"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args perftest_args[] = {
	{ arg_string, &opt.dev, "namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd perftest_cmd = {
	.name = "perftest",
	.fn = perftest,
	.descr = "Perform low-level performance testing",
	.ctx_size = sizeof(opt),
	.opts = perftest_opts,
	.args = perftest_args,
};

CMD_COMMAND(perftest_cmd);

/* End of tables for command line parsing */

static void
print_perftest(struct nvme_io_test *io_test, bool perthread)
{
	uint64_t	io_completed = 0, iops, mbps;
	uint32_t	i;

	for (i = 0; i < io_test->num_threads; i++)
		io_completed += io_test->io_completed[i];

	iops = io_completed/io_test->time;
	mbps = iops * io_test->size / (1024*1024);

	printf("Threads: %2d Size: %6d %5s Time: %3d IO/s: %7ju MB/s: %4ju\n",
	    io_test->num_threads, io_test->size,
	    io_test->opc == NVME_OPC_READ ? "READ" : "WRITE",
	    io_test->time, (uintmax_t)iops, (uintmax_t)mbps);

	if (perthread)
		for (i = 0; i < io_test->num_threads; i++)
			printf("\t%3d: %8ju IO/s\n", i,
			    (uintmax_t)io_test->io_completed[i]/io_test->time);
}

static void
perftest(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_io_test		io_test;
	int				fd;
	u_long				ioctl_cmd = NVME_IO_TEST;

	memset(&io_test, 0, sizeof(io_test));
	if (arg_parse(argc, argv, f))
		return;
	
	if (opt.op == NULL)
		arg_help(argc, argv, f);
	if (opt.flags != NULL && strcmp(opt.flags, "refthread") == 0)
		io_test.flags |= NVME_TEST_FLAG_REFTHREAD;
	if (opt.intr != NULL) {
		if (strcmp(opt.intr, "bio") == 0 ||
		    strcmp(opt.intr, "wait") == 0)
			ioctl_cmd = NVME_BIO_TEST;
		else if (strcmp(opt.intr, "io") == 0 ||
		    strcmp(opt.intr, "intr") == 0)
			ioctl_cmd = NVME_IO_TEST;
		else {
			fprintf(stderr, "Unknown interrupt test type %s\n", opt.intr);
			arg_help(argc, argv, f);
		}
	}
	if (opt.threads <= 0 || opt.threads > 128) {
		fprintf(stderr, "Bad number of threads %d\n", opt.threads);
		arg_help(argc, argv, f);
	}
	io_test.num_threads = opt.threads;
	if (strcasecmp(opt.op, "read") == 0)
		io_test.opc = NVME_OPC_READ;
	else if (strcasecmp(opt.op, "write") == 0)
		io_test.opc = NVME_OPC_WRITE;
	else {
		fprintf(stderr, "\"%s\" not valid opcode.\n", opt.op);
		arg_help(argc, argv, f);
	}
	if (opt.time == 0) {
		fprintf(stderr, "No time speciifed\n");
		arg_help(argc, argv, f);
	}
	io_test.time = opt.time;
	io_test.size = opt.size;
	open_dev(opt.dev, &fd, 1, 1);
	if (ioctl(fd, ioctl_cmd, &io_test) < 0)
		err(EX_IOERR, "ioctl NVME_IO_TEST failed");

	close(fd);
	print_perftest(&io_test, opt.perthread);
	exit(0);
}
