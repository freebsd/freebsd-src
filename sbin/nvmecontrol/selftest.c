/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Chuck Tuffli
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

#include "nvmecontrol.h"

#define SELFTEST_CODE_NONE 0xffu
#define SELFTEST_CODE_MAX  0xfu

static struct options {
	const char	*dev;
	uint8_t		stc;	/* Self-test Code */
} opt = {
	.dev = NULL,
	.stc = SELFTEST_CODE_NONE,
};

static void
selftest_op(int fd, uint32_t nsid, uint8_t stc)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_DEVICE_SELF_TEST;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32(stc);

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "self-test request failed");

	if (NVME_STATUS_GET_SCT(pt.cpl.status) == NVME_SCT_COMMAND_SPECIFIC &&
	    NVME_STATUS_GET_SC(pt.cpl.status) == NVME_SC_SELF_TEST_IN_PROGRESS)
		errx(EX_UNAVAILABLE, "device self-test in progress");
	else if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "self-test request returned error");
}

static void
selftest(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_controller_data	cdata;
	int				fd;
	char				*path;
	uint32_t			nsid;

	if (arg_parse(argc, argv, f))
		return;

	open_dev(opt.dev, &fd, 1, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 1, 1);
	}
	free(path);

	if (opt.stc == SELFTEST_CODE_NONE)
		errx(EX_USAGE, "must specify a Self-test Code");
	else if (opt.stc > SELFTEST_CODE_MAX)
		errx(EX_DATAERR, "illegal Self-test Code 0x%x", opt.stc);

	if (read_controller_data(fd, &cdata))
		errx(EX_IOERR, "Identify request failed");

	if (((cdata.oacs >> NVME_CTRLR_DATA_OACS_SELFTEST_SHIFT) &
	     NVME_CTRLR_DATA_OACS_SELFTEST_MASK) == 0)
		errx(EX_UNAVAILABLE, "controller does not support self-test");

	selftest_op(fd, nsid, opt.stc);

	close(fd);
	exit(0);
}

static const struct opts selftest_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("test-code", 'c', arg_uint8, opt, stc,
	    "Self-test Code to execute"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static struct args selftest_args[] = {
	{ arg_string, &opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd selftest_cmd = {
	.name = "selftest",
	.fn = selftest,
	.descr = "Start device self-test",
	.ctx_size = sizeof(opt),
	.opts = selftest_opts,
	.args = selftest_args,
};

CMD_COMMAND(selftest_cmd);
