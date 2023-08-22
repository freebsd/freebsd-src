/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2019 Alexander Motin <mav@FreeBSD.org>
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

/* Tables for command line parsing */

static cmd_fn_t sanitize;

static struct options {
	bool		ause;
	bool		ndas;
	bool		oipbp;
	bool		reportonly;
	uint8_t		owpass;
	uint32_t	ovrpat;
	const char	*sanact;
	const char	*dev;
} opt = {
	.ause = false,
	.ndas = false,
	.oipbp = false,
	.reportonly = false,
	.owpass = 1,
	.ovrpat = 0,
	.sanact = NULL,
	.dev = NULL,
};

static const struct opts sanitize_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("ause", 'U', arg_none, opt, ause,
	    "Allow Unrestricted Sanitize Exit"),
	OPT("ndas", 'd', arg_none, opt, ndas,
	    "No Deallocate After Sanitize"),
	OPT("oipbp", 'I', arg_none, opt, oipbp,
	    "Overwrite Invert Pattern Between Passes"),
	OPT("reportonly", 'r', arg_none, opt, reportonly,
	    "Report previous sanitize status"),
	OPT("owpass", 'c', arg_uint8, opt, owpass,
	    "Overwrite Pass Count"),
	OPT("ovrpat", 'p', arg_uint32, opt, ovrpat,
	    "Overwrite Pattern"),
	OPT("sanact", 'a', arg_string, opt, sanact,
	    "Sanitize Action (block, overwrite, crypto)"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args sanitize_args[] = {
	{ arg_string, &opt.dev, "controller-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd sanitize_cmd = {
	.name = "sanitize",
	.fn = sanitize,
	.descr = "Sanitize NVM subsystem",
	.ctx_size = sizeof(opt),
	.opts = sanitize_opts,
	.args = sanitize_args,
};

CMD_COMMAND(sanitize_cmd);

/* End of tables for command line parsing */

static void
sanitize(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_controller_data	cd;
	struct nvme_pt_command		pt;
	struct nvme_sanitize_status_page ss;
	char				*path;
	uint32_t			nsid;
	int				sanact = 0, fd, delay = 1;

	if (arg_parse(argc, argv, f))
		return;

	if (opt.sanact == NULL) {
		if (!opt.reportonly) {
			fprintf(stderr, "Sanitize Action is not specified\n");
			arg_help(argc, argv, f);
		}
	} else {
		if (strcmp(opt.sanact, "exitfailure") == 0)
			sanact = 1;
		else if (strcmp(opt.sanact, "block") == 0)
			sanact = 2;
		else if (strcmp(opt.sanact, "overwrite") == 0)
			sanact = 3;
		else if (strcmp(opt.sanact, "crypto") == 0)
			sanact = 4;
		else {
			fprintf(stderr, "Incorrect Sanitize Action value\n");
			arg_help(argc, argv, f);
		}
	}
	if (opt.owpass == 0 || opt.owpass > 16) {
		fprintf(stderr, "Incorrect Overwrite Pass Count value\n");
		arg_help(argc, argv, f);
	}

	open_dev(opt.dev, &fd, 1, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 1, 1);
	}
	free(path);

	if (opt.reportonly)
		goto wait;

	/* Check that controller can execute this command. */
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");
	if (((cd.sanicap >> NVME_CTRLR_DATA_SANICAP_BES_SHIFT) &
	     NVME_CTRLR_DATA_SANICAP_BES_MASK) == 0 && sanact == 2)
		errx(EX_UNAVAILABLE, "controller does not support Block Erase");
	if (((cd.sanicap >> NVME_CTRLR_DATA_SANICAP_OWS_SHIFT) &
	     NVME_CTRLR_DATA_SANICAP_OWS_MASK) == 0 && sanact == 3)
		errx(EX_UNAVAILABLE, "controller does not support Overwrite");
	if (((cd.sanicap >> NVME_CTRLR_DATA_SANICAP_CES_SHIFT) &
	     NVME_CTRLR_DATA_SANICAP_CES_MASK) == 0 && sanact == 4)
		errx(EX_UNAVAILABLE, "controller does not support Crypto Erase");

	/*
	 * If controller supports only one namespace, we may sanitize it.
	 * If there can be more, make user explicit in his commands.
	 */
	if (nsid != 0 && cd.nn > 1)
		errx(EX_UNAVAILABLE, "can't sanitize one of namespaces, specify controller");

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_SANITIZE;
	pt.cmd.cdw10 = htole32((opt.ndas << 9) | (opt.oipbp << 8) |
	    ((opt.owpass & 0xf) << 4) | (opt.ause << 3) | sanact);
	pt.cmd.cdw11 = htole32(opt.ovrpat);

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "sanitize request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "sanitize request returned error");

wait:
	read_logpage(fd, NVME_LOG_SANITIZE_STATUS,
	    NVME_GLOBAL_NAMESPACE_TAG, 0, 0, 0, &ss, sizeof(ss));
	switch ((ss.sstat >> NVME_SS_PAGE_SSTAT_STATUS_SHIFT) &
	    NVME_SS_PAGE_SSTAT_STATUS_MASK) {
	case NVME_SS_PAGE_SSTAT_STATUS_NEVER:
		printf("Never sanitized");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_COMPLETED:
		printf("Sanitize completed");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_INPROG:
		printf("Sanitize in progress: %u%% (%u/65535)\r",
		    (ss.sprog * 100 + 32768) / 65536, ss.sprog);
		fflush(stdout);
		if (delay < 16)
			delay++;
		sleep(delay);
		goto wait;
	case NVME_SS_PAGE_SSTAT_STATUS_FAILED:
		printf("Sanitize failed");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_COMPLETEDWD:
		printf("Sanitize completed with deallocation");
		break;
	default:
		printf("Sanitize status unknown");
		break;
	}
	if (delay > 1)
		printf("                       ");
	printf("\n");

	close(fd);
	exit(0);
}
