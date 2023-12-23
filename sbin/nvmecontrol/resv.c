/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2019 Alexander Motin <mav@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioccom.h>

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

static cmd_fn_t resv;
static cmd_fn_t resvacquire;
static cmd_fn_t resvregister;
static cmd_fn_t resvrelease;
static cmd_fn_t resvreport;

#define NONE 0xffffffffu
#define NONE64 0xffffffffffffffffull
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
#define OPT_END	{ NULL, 0, arg_none, NULL, NULL }

static struct cmd resv_cmd = {
	.name = "resv",
	.fn = resv,
	.descr = "Reservation commands",
	.ctx_size = 0,
	.opts = NULL,
	.args = NULL,
};

CMD_COMMAND(resv_cmd);

static struct acquire_options {
	uint64_t	crkey;
	uint64_t	prkey;
	uint8_t		rtype;
	uint8_t		racqa;
	const char	*dev;
} acquire_opt = {
	.crkey = 0,
	.prkey = 0,
	.rtype = 0,
	.racqa = 0,
	.dev = NULL,
};

static const struct opts acquire_opts[] = {
	OPT("crkey", 'c', arg_uint64, acquire_opt, crkey,
	    "Current Reservation Key"),
	OPT("prkey", 'p', arg_uint64, acquire_opt, prkey,
	    "Preempt Reservation Key"),
	OPT("rtype", 't', arg_uint8, acquire_opt, rtype,
	    "Reservation Type"),
	OPT("racqa", 'a', arg_uint8, acquire_opt, racqa,
	    "Acquire Action (0=acq, 1=pre, 2=pre+ab)"),
	{ NULL, 0, arg_none, NULL, NULL }
};

static const struct args acquire_args[] = {
	{ arg_string, &acquire_opt.dev, "namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd acquire_cmd = {
	.name = "acquire",
	.fn = resvacquire,
	.descr = "Acquire/preempt reservation",
	.ctx_size = sizeof(acquire_opt),
	.opts = acquire_opts,
	.args = acquire_args,
};

CMD_SUBCOMMAND(resv_cmd, acquire_cmd);

static struct register_options {
	uint64_t	crkey;
	uint64_t	nrkey;
	uint8_t		rrega;
	bool		iekey;
	uint8_t		cptpl;
	const char	*dev;
} register_opt = {
	.crkey = 0,
	.nrkey = 0,
	.rrega = 0,
	.iekey = false,
	.cptpl = 0,
	.dev = NULL,
};

static const struct opts register_opts[] = {
	OPT("crkey", 'c', arg_uint64, register_opt, crkey,
	    "Current Reservation Key"),
	OPT("nrkey", 'k', arg_uint64, register_opt, nrkey,
	    "New Reservation Key"),
	OPT("rrega", 'r', arg_uint8, register_opt, rrega,
	    "Register Action (0=reg, 1=unreg, 2=replace)"),
	OPT("iekey", 'i', arg_none, register_opt, iekey,
	    "Ignore Existing Key"),
	OPT("cptpl", 'p', arg_uint8, register_opt, cptpl,
	    "Change Persist Through Power Loss State"),
	{ NULL, 0, arg_none, NULL, NULL }
};

static const struct args register_args[] = {
	{ arg_string, &register_opt.dev, "namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd register_cmd = {
	.name = "register",
	.fn = resvregister,
	.descr = "Register/unregister reservation",
	.ctx_size = sizeof(register_opt),
	.opts = register_opts,
	.args = register_args,
};

CMD_SUBCOMMAND(resv_cmd, register_cmd);

static struct release_options {
	uint64_t	crkey;
	uint8_t		rtype;
	uint8_t		rrela;
	const char	*dev;
} release_opt = {
	.crkey = 0,
	.rtype = 0,
	.rrela = 0,
	.dev = NULL,
};

static const struct opts release_opts[] = {
	OPT("crkey", 'c', arg_uint64, release_opt, crkey,
	    "Current Reservation Key"),
	OPT("rtype", 't', arg_uint8, release_opt, rtype,
	    "Reservation Type"),
	OPT("rrela", 'a', arg_uint8, release_opt, rrela,
	    "Release Action (0=release, 1=clear)"),
	{ NULL, 0, arg_none, NULL, NULL }
};

static const struct args release_args[] = {
	{ arg_string, &release_opt.dev, "namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd release_cmd = {
	.name = "release",
	.fn = resvrelease,
	.descr = "Release/clear reservation",
	.ctx_size = sizeof(release_opt),
	.opts = release_opts,
	.args = release_args,
};

CMD_SUBCOMMAND(resv_cmd, release_cmd);

static struct report_options {
	bool		hex;
	bool		verbose;
	bool		eds;
	const char	*dev;
} report_opt = {
	.hex = false,
	.verbose = false,
	.eds = false,
	.dev = NULL,
};

static const struct opts report_opts[] = {
	OPT("hex", 'x', arg_none, report_opt, hex,
	    "Print reservation status in hex"),
	OPT("verbose", 'v', arg_none, report_opt, verbose,
	    "More verbosity"),
	OPT("eds", 'e', arg_none, report_opt, eds,
	    "Extended Data Structure"),
	{ NULL, 0, arg_none, NULL, NULL }
};

static const struct args report_args[] = {
	{ arg_string, &report_opt.dev, "namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd report_cmd = {
	.name = "report",
	.fn = resvreport,
	.descr = "Print reservation status",
	.ctx_size = sizeof(report_opt),
	.opts = report_opts,
	.args = report_args,
};

CMD_SUBCOMMAND(resv_cmd, report_cmd);

/* handles NVME_OPC_RESERVATION_* NVM commands */

static void
resvacquire(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	uint64_t	data[2];
	int		fd;
	uint32_t	nsid;

	if (arg_parse(argc, argv, f))
		return;
	open_dev(acquire_opt.dev, &fd, 0, 1);
	get_nsid(fd, NULL, &nsid);
	if (nsid == 0) {
		fprintf(stderr, "This command require namespace-id\n");
		arg_help(argc, argv, f);
	}

	data[0] = htole64(acquire_opt.crkey);
	data[1] = htole64(acquire_opt.prkey);

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_RESERVATION_ACQUIRE;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32((acquire_opt.racqa & 7) |
	    (acquire_opt.rtype << 8));
	pt.buf = &data;
	pt.len = sizeof(data);
	pt.is_read = 0;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "acquire request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "acquire request returned error");

	close(fd);
	exit(0);
}

static void
resvregister(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	uint64_t	data[2];
	int		fd;
	uint32_t	nsid;

	if (arg_parse(argc, argv, f))
		return;
	open_dev(register_opt.dev, &fd, 0, 1);
	get_nsid(fd, NULL, &nsid);
	if (nsid == 0) {
		fprintf(stderr, "This command require namespace-id\n");
		arg_help(argc, argv, f);
	}

	data[0] = htole64(register_opt.crkey);
	data[1] = htole64(register_opt.nrkey);

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_RESERVATION_REGISTER;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32((register_opt.rrega & 7) |
	    (register_opt.iekey << 3) | (register_opt.cptpl << 30));
	pt.buf = &data;
	pt.len = sizeof(data);
	pt.is_read = 0;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "register request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "register request returned error");

	close(fd);
	exit(0);
}

static void
resvrelease(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	uint64_t	data[1];
	int		fd;
	uint32_t	nsid;

	if (arg_parse(argc, argv, f))
		return;
	open_dev(release_opt.dev, &fd, 0, 1);
	get_nsid(fd, NULL, &nsid);
	if (nsid == 0) {
		fprintf(stderr, "This command require namespace-id\n");
		arg_help(argc, argv, f);
	}

	data[0] = htole64(release_opt.crkey);

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_RESERVATION_RELEASE;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32((release_opt.rrela & 7) |
	    (release_opt.rtype << 8));
	pt.buf = &data;
	pt.len = sizeof(data);
	pt.is_read = 0;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "release request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "release request returned error");

	close(fd);
	exit(0);
}

static void
resvreport(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_resv_status	*s;
	struct nvme_resv_status_ext *e;
	uint8_t		data[4096] __aligned(4);
	int		fd;
	u_int		i, n;
	uint32_t	nsid;

	if (arg_parse(argc, argv, f))
		return;
	open_dev(report_opt.dev, &fd, 0, 1);
	get_nsid(fd, NULL, &nsid);
	if (nsid == 0) {
		fprintf(stderr, "This command require namespace-id\n");
		arg_help(argc, argv, f);
	}

	bzero(data, sizeof(data));
	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_RESERVATION_REPORT;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32(sizeof(data) / 4 - 1);
	pt.cmd.cdw11 = htole32(report_opt.eds);	/* EDS */
	pt.buf = &data;
	pt.len = sizeof(data);
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "report request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "report request returned error");

	close(fd);

	if (report_opt.eds)
		nvme_resv_status_ext_swapbytes((void *)data, sizeof(data));
	else
		nvme_resv_status_swapbytes((void *)data, sizeof(data));

	if (report_opt.hex) {
		i = sizeof(data);
		if (!report_opt.verbose) {
			for (; i > 64; i--) {
				if (data[i - 1] != 0)
					break;
			}
		}
		print_hex(&data, i);
		exit(0);
	}

	s = (struct nvme_resv_status *)data;
	n = (s->regctl[1] << 8) | s->regctl[0];
	printf("Generation:                       %u\n", s->gen);
	printf("Reservation Type:                 %u\n", s->rtype);
	printf("Number of Registered Controllers: %u\n", n);
	printf("Persist Through Power Loss State: %u\n", s->ptpls);
	if (report_opt.eds) {
		e = (struct nvme_resv_status_ext *)data;
		n = MIN(n, (sizeof(data) - sizeof(e)) / sizeof(e->ctrlr[0]));
		for (i = 0; i < n; i++) {
			printf("Controller ID:                    0x%04x\n",
			    e->ctrlr[i].ctrlr_id);
			printf("  Reservation Status:             %u\n",
			    e->ctrlr[i].rcsts);
			printf("  Reservation Key:                0x%08jx\n",
			    e->ctrlr[i].rkey);
			printf("  Host Identifier:                0x%08jx%08jx\n",
			    e->ctrlr[i].hostid[0], e->ctrlr[i].hostid[1]);
		}
	} else {
		n = MIN(n, (sizeof(data) - sizeof(s)) / sizeof(s->ctrlr[0]));
		for (i = 0; i < n; i++) {
			printf("Controller ID:                    0x%04x\n",
			    s->ctrlr[i].ctrlr_id);
			printf("  Reservation Status:             %u\n",
			    s->ctrlr[i].rcsts);
			printf("  Host Identifier:                0x%08jx\n",
			    s->ctrlr[i].hostid);
			printf("  Reservation Key:                0x%08jx\n",
			    s->ctrlr[i].rkey);
		}
	}
	exit(0);
}

static void
resv(const struct cmd *nf __unused, int argc, char *argv[])
{

	cmd_dispatch(argc, argv, &resv_cmd);
}
