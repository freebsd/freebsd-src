/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2012-2013 Intel Corporation
 * All rights reserved.
 * Copyright (C) 2018-2019 Alexander Motin <mav@FreeBSD.org>
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

#include "nvmecontrol.h"
#include "nvmecontrol_ext.h"

#define NONE 0xfffffffeu

static struct options {
	bool		hex;
	bool		verbose;
	const char	*dev;
	uint32_t	nsid;
} opt = {
	.hex = false,
	.verbose = false,
	.dev = NULL,
	.nsid = NONE,
};

static void
identify_ctrlr(int fd)
{
	struct nvme_controller_data	cdata;
	int				hexlength;

	if (read_controller_data(fd, &cdata))
		errx(EX_IOERR, "Identify request failed");
	close(fd);

	if (opt.hex) {
		if (opt.verbose)
			hexlength = sizeof(struct nvme_controller_data);
		else
			hexlength = offsetof(struct nvme_controller_data,
			    reserved8);
		print_hex(&cdata, hexlength);
		exit(0);
	}

	nvme_print_controller(&cdata);
	exit(0);
}

static void
identify_ns(int fd, uint32_t nsid)
{
	struct nvme_namespace_data	nsdata;
	int				hexlength;

	if (read_namespace_data(fd, nsid, &nsdata))
		errx(EX_IOERR, "Identify request failed");
	close(fd);

	if (opt.hex) {
		if (opt.verbose)
			hexlength = sizeof(struct nvme_namespace_data);
		else
			hexlength = offsetof(struct nvme_namespace_data,
			    reserved6);
		print_hex(&nsdata, hexlength);
		exit(0);
	}

	nvme_print_namespace(&nsdata);
	exit(0);
}

static void
identify(const struct cmd *f, int argc, char *argv[])
{
	char		*path;
	int		fd;
	uint32_t	nsid;

	if (arg_parse(argc, argv, f))
		return;

	open_dev(opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		/*
		 * We got namespace device, but we need to send IDENTIFY
		 * commands to the controller, not the namespace, since it
		 * is an admin cmd.  The namespace ID will be specified in
		 * the IDENTIFY command itself.
		 */
		close(fd);
		open_dev(path, &fd, 0, 1);
	}
	free(path);
	if (opt.nsid != NONE)
		nsid = opt.nsid;

	if (nsid == 0)
		identify_ctrlr(fd);
	else
		identify_ns(fd, nsid);
}

static const struct opts identify_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("hex", 'x', arg_none, opt, hex,
	    "Print identity information in hex"),
	OPT("verbose", 'v', arg_none, opt, verbose,
	    "More verbosity: print entire identify table"),
	OPT("nsid", 'n', arg_uint32, opt, nsid,
	    "Namespace ID to use if not in device name"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args identify_args[] = {
	{ arg_string, &opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd identify_cmd = {
	.name = "identify",
	.fn = identify,
	.descr = "Print summary of the IDENTIFY information",
	.ctx_size = sizeof(opt),
	.opts = identify_opts,
	.args = identify_args,
};

CMD_COMMAND(identify_cmd);
