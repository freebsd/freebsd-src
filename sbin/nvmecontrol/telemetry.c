/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2024 Netflix, Inc
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

#include <sys/param.h>
#include <sys/ioccom.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/endian.h>

#include "nvmecontrol.h"

/* Tables for command line parsing */

static cmd_fn_t telemetry_log;

#define NONE 0xffffffffu
static struct options {
	const char *outfn;
	const char *dev;
	uint8_t da;
} opt = {
	.outfn = NULL,
	.dev = NULL,
	.da = 3,
};

static const struct opts telemetry_log_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("output-file", 'O', arg_string, opt, outfn,
	    "output file for telemetry data"),
	OPT("data-area", 'd', arg_uint8, opt, da,
	    "output file for telemetry data"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args telemetry_log_args[] = {
	{ arg_string, &opt.dev, "<controller id|namespace id>" },
	{ arg_none, NULL, NULL },
};

static struct cmd telemetry_log_cmd = {
	.name = "telemetry-log",
	.fn = telemetry_log,
	.descr = "Retrieves telemetry log pages from drive",
	.ctx_size = sizeof(opt),
	.opts = telemetry_log_opts,
	.args = telemetry_log_args,
};

CMD_COMMAND(telemetry_log_cmd);

/* End of tables for command line parsing */

/*
 * Note: Even though this is a logpage, it's variable size and tricky
 * to get with some weird options, so it's its own command.
 */

static void
telemetry_log(const struct cmd *f, int argc, char *argv[])
{
	int				fd, fdout;
	char				*path;
	uint32_t			nsid;
	ssize_t				size;
	uint64_t			off;
	ssize_t				chunk;
	struct nvme_controller_data	cdata;
	bool				can_telemetry;
	struct nvme_telemetry_log_page  tlp, buf;

	if (arg_parse(argc, argv, f))
		return;
	if (opt.da < 1 || opt.da > 3)
		errx(EX_USAGE, "Data area %d is not in the range 1-3\n", opt.da);
	if (opt.outfn == NULL)
		errx(EX_USAGE, "No output file specified");

	open_dev(opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid == 0) {
		nsid = NVME_GLOBAL_NAMESPACE_TAG;
	} else {
		close(fd);
		open_dev(path, &fd, 0, 1);
	}
	free(path);

	if (read_controller_data(fd, &cdata))
		errx(EX_IOERR, "Identify request failed");

	can_telemetry = NVMEV(NVME_CTRLR_DATA_LPA_TELEMETRY, cdata.lpa);
	if (!can_telemetry)
		errx(EX_UNAVAILABLE, "Drive does not support telemetry");
	if (nsid != NVME_GLOBAL_NAMESPACE_TAG)
		errx(EX_UNAVAILABLE, "Cannot operate on namespace");

	fdout = open(opt.outfn, O_WRONLY | O_CREAT, 0664);
	if (fdout == -1)
		err(EX_IOERR, "Can't create %s", opt.outfn);

	/* Read the log page */
	size = sizeof(tlp);
	off = 0;
	read_logpage(fd, NVME_LOG_TELEMETRY_HOST_INITIATED, nsid, 0, 0, 0,
	    off, 0, 0, 0, &tlp, size);
	switch(opt.da) {
	case 1:
		size = letoh(tlp.da1_last);
		break;
	case 2:
		size = letoh(tlp.da2_last);
		break;
	case 3:
		size = letoh(tlp.da3_last);
		break;
	default:
		errx(EX_USAGE, "Impossible data area %d", opt.da);
	}
	size = (size + 1) * 512; /* The count of additional pages */
	chunk = 4096;

	printf("Extracting %llu bytes\n", (unsigned long long)size);
	do {
		if (chunk > size)
			chunk = size;
		read_logpage(fd, NVME_LOG_TELEMETRY_HOST_INITIATED, nsid, 0, 0, 0,
		    off, 0, 0, 0, &buf, chunk);
		if (off == 0) {
			/*
			 * Sanity check to make sure that the generation number
			 * didn't change between the two reads.
			 */
			if (tlp.hi_gen != buf.hi_gen)
				warnx(
				    "Generation number changed from %d to %d",
				    tlp.hi_gen, buf.hi_gen);
		}
		if (write(fdout, &buf, chunk) != chunk)
			err(EX_IOERR, "Error writing %s", opt.outfn);
		off += chunk;
		size -= chunk;
	} while (size > 0);

	close(fdout);
	close(fd);
	exit(0);
}
