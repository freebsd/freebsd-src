/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#define NONE 0xffffffffu
#define SES_NONE 0
#define SES_USER 1
#define SES_CRYPTO 2

/* Tables for command line parsing */

static cmd_fn_t format;

static struct options {
	uint32_t	lbaf;
	uint32_t	ms;
	uint32_t	pi;
	uint32_t	pil;
	uint32_t	ses;
	bool		Eflag;
	bool		Cflag;
	const char	*dev;
} opt = {
	.lbaf = NONE,
	.ms = NONE,
	.pi = NONE,
	.pil = NONE,
	.ses = SES_NONE,
	.Eflag = false,
	.Cflag = false,
	.dev = NULL,
};

static const struct opts format_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("crypto", 'C', arg_none, opt, Cflag,
	    "Crptographic erase"),
	OPT("erase", 'E', arg_none, opt, Eflag,
	    "User data erase"),
	OPT("lbaf", 'f', arg_uint32, opt, lbaf,
	    "LBA Format to apply to the media"),
	OPT("ms", 'm', arg_uint32, opt, ms,
	    "Metadata settings"),
	OPT("pi", 'p', arg_uint32, opt, pi,
	    "Protective information"),
	OPT("pil", 'l', arg_uint32, opt, pil,
	    "Protective information location"),
	OPT("ses", 's', arg_uint32, opt, ses,
	    "Secure erase settings"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args format_args[] = {
	{ arg_string, &opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd format_cmd = {
	.name = "format",
	.fn = format,
	.descr = "Format/erase one or all the namespaces",
	.ctx_size = sizeof(opt),
	.opts = format_opts,
	.args = format_args,
};

CMD_COMMAND(format_cmd);

/* End of tables for command line parsing */

static void
format(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_controller_data	cd;
	struct nvme_namespace_data	nsd;
	struct nvme_pt_command		pt;
	char				*path;
	const char			*target;
	uint32_t			nsid;
	int				lbaf, ms, pi, pil, ses, fd;

	if (arg_parse(argc, argv, f))
		return;

	if ((int)opt.Eflag + opt.Cflag + (opt.ses != SES_NONE) > 1) {
		fprintf(stderr,
		    "Only one of -E, -C or -s may be specified\n");
		arg_help(argc, argv, f);
	}

	target = opt.dev;
	lbaf = opt.lbaf;
	ms = opt.ms;
	pi = opt.pi;
	pil = opt.pil;
	if (opt.Eflag)
		ses = SES_USER;
	else if (opt.Cflag)
		ses = SES_CRYPTO;
	else
		ses = opt.ses;

	open_dev(target, &fd, 1, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid == 0) {
		nsid = NVME_GLOBAL_NAMESPACE_TAG;
	} else {
		/*
		 * We send FORMAT commands to the controller, not the namespace,
		 * since it is an admin cmd.  The namespace ID will be specified
		 * in the command itself.  So parse the namespace's device node
		 * string to get the controller substring and namespace ID.
		 */
		close(fd);
		open_dev(path, &fd, 1, 1);
	}
	free(path);

	/* Check that controller can execute this command. */
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");
	if (((cd.oacs >> NVME_CTRLR_DATA_OACS_FORMAT_SHIFT) &
	    NVME_CTRLR_DATA_OACS_FORMAT_MASK) == 0)
		errx(EX_UNAVAILABLE, "controller does not support format");
	if (((cd.fna >> NVME_CTRLR_DATA_FNA_CRYPTO_ERASE_SHIFT) &
	    NVME_CTRLR_DATA_FNA_CRYPTO_ERASE_MASK) == 0 && ses == SES_CRYPTO)
		errx(EX_UNAVAILABLE, "controller does not support cryptographic erase");

	if (nsid != NVME_GLOBAL_NAMESPACE_TAG) {
		if (((cd.fna >> NVME_CTRLR_DATA_FNA_FORMAT_ALL_SHIFT) &
		    NVME_CTRLR_DATA_FNA_FORMAT_ALL_MASK) && ses == SES_NONE)
			errx(EX_UNAVAILABLE, "controller does not support per-NS format");
		if (((cd.fna >> NVME_CTRLR_DATA_FNA_ERASE_ALL_SHIFT) &
		    NVME_CTRLR_DATA_FNA_ERASE_ALL_MASK) && ses != SES_NONE)
			errx(EX_UNAVAILABLE, "controller does not support per-NS erase");

		/* Try to keep previous namespace parameters. */
		if (read_namespace_data(fd, nsid, &nsd))
			errx(EX_IOERR, "Identify request failed");
		if (lbaf < 0)
			lbaf = (nsd.flbas >> NVME_NS_DATA_FLBAS_FORMAT_SHIFT)
			    & NVME_NS_DATA_FLBAS_FORMAT_MASK;
		if (lbaf > nsd.nlbaf)
			errx(EX_USAGE, "LBA format is out of range");
		if (ms < 0)
			ms = (nsd.flbas >> NVME_NS_DATA_FLBAS_EXTENDED_SHIFT)
			    & NVME_NS_DATA_FLBAS_EXTENDED_MASK;
		if (pi < 0)
			pi = (nsd.dps >> NVME_NS_DATA_DPS_MD_START_SHIFT)
			    & NVME_NS_DATA_DPS_MD_START_MASK;
		if (pil < 0)
			pil = (nsd.dps >> NVME_NS_DATA_DPS_PIT_SHIFT)
			    & NVME_NS_DATA_DPS_PIT_MASK;
	} else {

		/* We have no previous parameters, so default to zeroes. */
		if (lbaf < 0)
			lbaf = 0;
		if (ms < 0)
			ms = 0;
		if (pi < 0)
			pi = 0;
		if (pil < 0)
			pil = 0;
	}

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_FORMAT_NVM;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32((ses << 9) + (pil << 8) + (pi << 5) +
	    (ms << 4) + lbaf);

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "format request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "format request returned error");
	close(fd);
	exit(0);
}
