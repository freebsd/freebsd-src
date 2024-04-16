/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Netflix, Inc.
 * Copyright (C) 2018-2019 Alexander Motin <mav@FreeBSD.org>
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

static cmd_fn_t ns;
static cmd_fn_t nsactive;
static cmd_fn_t nsallocated;
static cmd_fn_t nscontrollers;
static cmd_fn_t nscreate;
static cmd_fn_t nsdelete;
static cmd_fn_t nsattach;
static cmd_fn_t nsdetach;
static cmd_fn_t nsattached;
static cmd_fn_t nsidentify;

#define NONE 0xffffffffu
#define NONE64 0xffffffffffffffffull
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
#define OPT_END	{ NULL, 0, arg_none, NULL, NULL }

static struct cmd ns_cmd = {
	.name = "ns",
	.fn = ns,
	.descr = "Namespace management commands",
	.ctx_size = 0,
	.opts = NULL,
	.args = NULL,
};

CMD_COMMAND(ns_cmd);

static struct active_options {
	const char	*dev;
} active_opt = {
	.dev = NULL,
};

static const struct args active_args[] = {
	{ arg_string, &active_opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd active_cmd = {
	.name = "active",
	.fn = nsactive,
	.descr = "List active (attached) namespaces",
	.ctx_size = sizeof(active_opt),
	.opts = NULL,
	.args = active_args,
};

CMD_SUBCOMMAND(ns_cmd, active_cmd);

static struct cmd allocated_cmd = {
	.name = "allocated",
	.fn = nsallocated,
	.descr = "List allocated (created) namespaces",
	.ctx_size = sizeof(active_opt),
	.opts = NULL,
	.args = active_args,
};

CMD_SUBCOMMAND(ns_cmd, allocated_cmd);

static struct controllers_options {
	const char	*dev;
} controllers_opt = {
	.dev = NULL,
};

static const struct args controllers_args[] = {
	{ arg_string, &controllers_opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd controllers_cmd = {
	.name = "controllers",
	.fn = nscontrollers,
	.descr = "List all controllers in NVM subsystem",
	.ctx_size = sizeof(controllers_opt),
	.opts = NULL,
	.args = controllers_args,
};

CMD_SUBCOMMAND(ns_cmd, controllers_cmd);

static struct create_options {
	uint64_t nsze;
	uint64_t cap;
	uint32_t lbaf;
	uint32_t mset;
	uint32_t nmic;
	uint32_t pi;
	uint32_t pil;
	uint32_t flbas;
	uint32_t dps;
//	uint32_t block_size;
	const char *dev;
} create_opt = {
	.nsze = NONE64,
	.cap = NONE64,
	.lbaf = NONE,
	.mset = NONE,
	.nmic = NONE,
	.pi = NONE,
	.pil = NONE,
	.flbas = NONE,
	.dps = NONE,
	.dev = NULL,
//	.block_size = NONE,
};

static const struct opts create_opts[] = {
	OPT("nsze", 's', arg_uint64, create_opt, nsze,
	    "The namespace size"),
	OPT("ncap", 'c', arg_uint64, create_opt, cap,
	    "The capacity of the namespace (<= ns size)"),
	OPT("lbaf", 'f', arg_uint32, create_opt, lbaf,
	    "The FMT field of the FLBAS"),
	OPT("mset", 'm', arg_uint32, create_opt, mset,
	    "The MSET field of the FLBAS"),
	OPT("nmic", 'n', arg_uint32, create_opt, nmic,
	    "Namespace multipath and sharing capabilities"),
	OPT("pi", 'p', arg_uint32, create_opt, pi,
	    "PI field of FLBAS"),
	OPT("pil", 'l', arg_uint32, create_opt, pil,
	    "PIL field of FLBAS"),
	OPT("flbas", 'L', arg_uint32, create_opt, flbas,
	    "Namespace formatted logical block size setting"),
	OPT("dps", 'd', arg_uint32, create_opt, dps,
	    "Data protection settings"),
//	OPT("block-size", 'b', arg_uint32, create_opt, block_size,
//	    "Blocksize of the namespace"),
	OPT_END
};

static const struct args create_args[] = {
	{ arg_string, &create_opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd create_cmd = {
	.name = "create",
	.fn = nscreate,
	.descr = "Create a namespace",
	.ctx_size = sizeof(create_opt),
	.opts = create_opts,
	.args = create_args,
};

CMD_SUBCOMMAND(ns_cmd, create_cmd);

static struct delete_options {
	uint32_t	nsid;
	const char	*dev;
} delete_opt = {
	.nsid = NONE - 1,
	.dev = NULL,
};

static const struct opts delete_opts[] = {
	OPT("namespace-id", 'n', arg_uint32, delete_opt, nsid,
	    "The namespace ID to delete"),
	OPT_END
};

static const struct args delete_args[] = {
	{ arg_string, &delete_opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd delete_cmd = {
	.name = "delete",
	.fn = nsdelete,
	.descr = "Delete a namespace",
	.ctx_size = sizeof(delete_opt),
	.opts = delete_opts,
	.args = delete_args,
};

CMD_SUBCOMMAND(ns_cmd, delete_cmd);

static struct attach_options {
	uint32_t	nsid;
	uint32_t	ctrlrid;
	const char	*dev;
} attach_opt = {
	.nsid = NONE,
	.ctrlrid = NONE - 1,
	.dev = NULL,
};

static const struct opts attach_opts[] = {
	OPT("namespace-id", 'n', arg_uint32, attach_opt, nsid,
	    "The namespace ID to attach"),
	OPT("controller", 'c', arg_uint32, attach_opt, ctrlrid,
	    "The controller ID to attach"),
	OPT_END
};

static const struct args attach_args[] = {
	{ arg_string, &attach_opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd attach_cmd = {
	.name = "attach",
	.fn = nsattach,
	.descr = "Attach a controller to a namespace",
	.ctx_size = sizeof(attach_opt),
	.opts = attach_opts,
	.args = attach_args,
};

CMD_SUBCOMMAND(ns_cmd, attach_cmd);

static struct attached_options {
	uint32_t	nsid;
	const char	*dev;
} attached_opt = {
	.nsid = NONE,
	.dev = NULL,
};

static const struct opts attached_opts[] = {
	OPT("namespace-id", 'n', arg_uint32, attached_opt, nsid,
	    "The namespace ID to request attached controllers"),
	OPT_END
};

static const struct args attached_args[] = {
	{ arg_string, &attached_opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd attached_cmd = {
	.name = "attached",
	.fn = nsattached,
	.descr = "List controllers attached to a namespace",
	.ctx_size = sizeof(attached_opt),
	.opts = attached_opts,
	.args = attached_args,
};

CMD_SUBCOMMAND(ns_cmd, attached_cmd);

static struct detach_options {
	uint32_t	nsid;
	uint32_t	ctrlrid;
	const char	*dev;
} detach_opt = {
	.nsid = NONE,
	.ctrlrid = NONE - 1,
	.dev = NULL,
};

static const struct opts detach_opts[] = {
	OPT("namespace-id", 'n', arg_uint32, detach_opt, nsid,
	    "The namespace ID to detach"),
	OPT("controller", 'c', arg_uint32, detach_opt, ctrlrid,
	    "The controller ID to detach"),
	OPT_END
};

static const struct args detach_args[] = {
	{ arg_string, &detach_opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd detach_cmd = {
	.name = "detach",
	.fn = nsdetach,
	.descr = "Detach a controller from a namespace",
	.ctx_size = sizeof(detach_opt),
	.opts = detach_opts,
	.args = detach_args,
};

CMD_SUBCOMMAND(ns_cmd, detach_cmd);

static struct identify_options {
	bool		hex;
	bool		verbose;
	const char	*dev;
	uint32_t	nsid;
} identify_opt = {
	.hex = false,
	.verbose = false,
	.dev = NULL,
	.nsid = NONE,
};

static const struct opts identify_opts[] = {
	OPT("hex", 'x', arg_none, identify_opt, hex,
	    "Print identiy information in hex"),
	OPT("verbose", 'v', arg_none, identify_opt, verbose,
	    "More verbosity: print entire identify table"),
	OPT("nsid", 'n', arg_uint32, identify_opt, nsid,
	    "The namespace ID to print IDENTIFY for"),
	{ NULL, 0, arg_none, NULL, NULL }
};

static const struct args identify_args[] = {
	{ arg_string, &identify_opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd identify_cmd = {
	.name = "identify",
	.fn = nsidentify,
	.descr = "Print IDENTIFY for allocated namespace",
	.ctx_size = sizeof(identify_opt),
	.opts = identify_opts,
	.args = identify_args,
};

CMD_SUBCOMMAND(ns_cmd, identify_cmd);

/* handles NVME_OPC_NAMESPACE_MANAGEMENT and ATTACHMENT admin cmds */

struct ns_result_str {
	uint16_t res;
	const char * str;
};

static struct ns_result_str ns_result[] = {
	{ 0x2,  "Invalid Field"},
	{ 0xa,  "Invalid Format"},
	{ 0xb,  "Invalid Namespace or format"},
	{ 0x15, "Namespace insufficient capacity"},
	{ 0x16, "Namespace ID unavailable"},
	{ 0x18, "Namespace already attached"},
	{ 0x19, "Namespace is private"},
	{ 0x1a, "Namespace is not attached"},
	{ 0x1b, "Thin provisioning not supported"},
	{ 0x1c, "Controller list invalid"},
	{ 0x24, "ANA Group Identifier Invalid"},
	{ 0x25, "ANA Attach Failed"},
	{ 0xFFFF, "Unknown"}
};

static const char *
get_res_str(uint16_t res)
{
	struct ns_result_str *t = ns_result;

	while (t->res != 0xFFFF) {
		if (t->res == res)
			return (t->str);
		t++;
	}
	return t->str;
}

static void
nsactive(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	int	fd, i;
	char	*path;
	uint32_t nsid;
	uint32_t list[1024];

	if (arg_parse(argc, argv, f))
		return;
	open_dev(active_opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 0, 1);
	}
	free(path);
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");

	/* Check that controller can execute this command. */
	if (NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, cd.oacs) == 0)
		errx(EX_UNAVAILABLE, "controller does not support namespace management");

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_IDENTIFY;
	pt.cmd.nsid = htole32(0);
	pt.cmd.cdw10 = htole32(0x02);
	pt.buf = list;
	pt.len = sizeof(list);
	pt.is_read = 1;
	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "identify request failed");
	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "identify request returned error");

	printf("Active namespaces:\n");
	for (i = 0; list[i] != 0; i++)
		printf("%10d\n", le32toh(list[i]));

	exit(0);
}

static void
nsallocated(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	int	fd, i;
	char	*path;
	uint32_t nsid;
	uint32_t list[1024];

	if (arg_parse(argc, argv, f))
		return;
	open_dev(active_opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 0, 1);
	}
	free(path);
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");

	/* Check that controller can execute this command. */
	if (NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, cd.oacs) == 0)
		errx(EX_UNAVAILABLE, "controller does not support namespace management");

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_IDENTIFY;
	pt.cmd.nsid = htole32(0);
	pt.cmd.cdw10 = htole32(0x10);
	pt.buf = list;
	pt.len = sizeof(list);
	pt.is_read = 1;
	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "identify request failed");
	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "identify request returned error");

	printf("Allocated namespaces:\n");
	for (i = 0; list[i] != 0; i++)
		printf("%10d\n", le32toh(list[i]));

	exit(0);
}

static void
nscontrollers(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	int	fd, i, n;
	char	*path;
	uint32_t nsid;
	uint16_t clist[2048];

	if (arg_parse(argc, argv, f))
		return;
	open_dev(controllers_opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 0, 1);
	}
	free(path);
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");

	/* Check that controller can execute this command. */
	if (NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, cd.oacs) == 0)
		errx(EX_UNAVAILABLE, "controller does not support namespace management");

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_IDENTIFY;
	pt.cmd.cdw10 = htole32(0x13);
	pt.buf = clist;
	pt.len = sizeof(clist);
	pt.is_read = 1;
	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "identify request failed");
	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "identify request returned error");

	n = le16toh(clist[0]);
	printf("NVM subsystem includes %d controller(s):\n", n);
	for (i = 0; i < n; i++)
		printf("  0x%04x\n", le16toh(clist[i + 1]));

	exit(0);
}

/*
 * NS MGMT Command specific status values:
 * 0xa = Invalid Format
 * 0x15 = Namespace Insufficient capacity
 * 0x16 = Namespace ID  unavailable (number namespaces exceeded)
 * 0xb = Thin Provisioning Not supported
 */
static void
nscreate(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	struct nvme_namespace_data nsdata;
	int	fd, result;
	char	*path;
	uint32_t nsid;

	if (arg_parse(argc, argv, f))
		return;

	if (create_opt.cap == NONE64)
		create_opt.cap = create_opt.nsze;
	if (create_opt.nsze == NONE64) {
		fprintf(stderr,
		    "Size not specified\n");
		arg_help(argc, argv, f);
	}

	open_dev(create_opt.dev, &fd, 1, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 1, 1);
	}
	free(path);
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");

	/* Check that controller can execute this command. */
	if (NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, cd.oacs) == 0)
		errx(EX_UNAVAILABLE, "controller does not support namespace management");

	memset(&nsdata, 0, sizeof(nsdata));
	nsdata.nsze = create_opt.nsze;
	nsdata.ncap = create_opt.cap;
	if (create_opt.flbas != NONE) {
		nsdata.flbas = create_opt.flbas;
	} else {
		/* Default to the first format, whatever it is. */
		nsdata.flbas = 0;
		if (create_opt.lbaf != NONE) {
			nsdata.flbas |= NVMEF(NVME_NS_DATA_FLBAS_FORMAT,
			    create_opt.lbaf);
		}
		if (create_opt.mset != NONE) {
			nsdata.flbas |= NVMEF(NVME_NS_DATA_FLBAS_EXTENDED,
			    create_opt.mset);
		}
	}
	if (create_opt.dps != NONE) {
		nsdata.dps = create_opt.dps;
	} else {
		/* Default to protection disabled. */
		nsdata.dps = 0;
		if (create_opt.pi != NONE) {
			nsdata.dps |= NVMEF(NVME_NS_DATA_DPS_MD_START,
			    create_opt.pi);
		}
		if (create_opt.pil != NONE) {
			nsdata.dps |= NVMEF(NVME_NS_DATA_DPS_PIT,
			    create_opt.pil);
		}
	}
	if (create_opt.nmic != NONE) {
		nsdata.nmic = create_opt.nmic;
	} else {
		/* Allow namespaces sharing if Multi-Path I/O is supported. */
		nsdata.nmic = NVMEF(NVME_NS_DATA_NMIC_MAY_BE_SHARED, !!cd.mic);
	}
	nvme_namespace_data_swapbytes(&nsdata);

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_NAMESPACE_MANAGEMENT;
	pt.cmd.cdw10 = htole32(0); /* create */
	pt.buf = &nsdata;
	pt.len = sizeof(struct nvme_namespace_data);
	pt.is_read = 0; /* passthrough writes data to ctrlr */
	if ((result = ioctl(fd, NVME_PASSTHROUGH_CMD, &pt)) < 0)
		errx(EX_IOERR, "ioctl request to %s failed: %d", create_opt.dev, result);

	if (nvme_completion_is_error(&pt.cpl)) {
		errx(EX_IOERR, "namespace creation failed: %s",
		    get_res_str(NVMEV(NVME_STATUS_SC, pt.cpl.status)));
	}
	printf("namespace %d created\n", pt.cpl.cdw0);
	exit(0);
}

static void
nsdelete(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	int	fd, result;
	char	*path;
	uint32_t nsid;
	char buf[2];

	if (arg_parse(argc, argv, f))
		return;

	open_dev(delete_opt.dev, &fd, 1, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 1, 1);
	} else if (delete_opt.nsid == NONE - 1) {
		close(fd);
		fprintf(stderr, "No NSID specified\n");
		arg_help(argc, argv, f);
	}
	if (delete_opt.nsid != NONE - 1)
		nsid = delete_opt.nsid;
	free(path);
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");

	/* Check that controller can execute this command. */
	if (NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, cd.oacs) == 0)
		errx(EX_UNAVAILABLE, "controller does not support namespace management");

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_NAMESPACE_MANAGEMENT;
	pt.cmd.cdw10 = htole32(1); /* delete */
	pt.buf = buf;
	pt.len = sizeof(buf);
	pt.is_read = 1;
	pt.cmd.nsid = nsid;

	if ((result = ioctl(fd, NVME_PASSTHROUGH_CMD, &pt)) < 0)
		errx(EX_IOERR, "ioctl request to %s failed: %d", delete_opt.dev, result);

	if (nvme_completion_is_error(&pt.cpl)) {
		errx(EX_IOERR, "namespace deletion failed: %s",
		    get_res_str(NVMEV(NVME_STATUS_SC, pt.cpl.status)));
	}
	printf("namespace %d deleted\n", nsid);
	exit(0);
}

/*
 * Attach and Detach use Dword 10, and a controller list (section 4.9)
 * This struct is 4096 bytes in size.
 * 0h = attach
 * 1h = detach
 *
 * Result values for both attach/detach:
 *
 * Completion 18h = Already attached
 *            19h = NS is private and already attached to a controller
 *            1Ah = Not attached, request could not be completed
 *            1Ch = Controller list invalid.
 *
 * 0x2 Invalid Field can occur if ctrlrid d.n.e in system.
 */
static void
nsattach(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	int	fd, result;
	char	*path;
	uint32_t nsid;
	uint16_t clist[2048];

	if (arg_parse(argc, argv, f))
		return;
	open_dev(attach_opt.dev, &fd, 1, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 1, 1);
	} else if (attach_opt.nsid == NONE) {
		close(fd);
		fprintf(stderr, "No NSID specified\n");
		arg_help(argc, argv, f);
	}
	if (attach_opt.nsid != NONE)
		nsid = attach_opt.nsid;
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");

	/* Check that controller can execute this command. */
	if (NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, cd.oacs) == 0)
		errx(EX_UNAVAILABLE, "controller does not support namespace management");

	if (attach_opt.ctrlrid == NONE) {
		/* Get full list of controllers to attach to. */
		memset(&pt, 0, sizeof(pt));
		pt.cmd.opc = NVME_OPC_IDENTIFY;
		pt.cmd.cdw10 = htole32(0x13);
		pt.buf = clist;
		pt.len = sizeof(clist);
		pt.is_read = 1;
		if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
			err(EX_IOERR, "identify request failed");
		if (nvme_completion_is_error(&pt.cpl))
			errx(EX_IOERR, "identify request returned error");
	} else {
		/* By default attach to this controller. */
		if (attach_opt.ctrlrid == NONE - 1)
			attach_opt.ctrlrid = cd.ctrlr_id;
		memset(&clist, 0, sizeof(clist));
		clist[0] = htole16(1);
		clist[1] = htole16(attach_opt.ctrlrid);
	}

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_NAMESPACE_ATTACHMENT;
	pt.cmd.cdw10 = htole32(0); /* attach */
	pt.cmd.nsid = nsid;
	pt.buf = &clist;
	pt.len = sizeof(clist);

	if ((result = ioctl(fd, NVME_PASSTHROUGH_CMD, &pt)) < 0)
		errx(EX_IOERR, "ioctl request to %s failed: %d", attach_opt.dev, result);

	if (nvme_completion_is_error(&pt.cpl)) {
		errx(EX_IOERR, "namespace attach failed: %s",
		    get_res_str(NVMEV(NVME_STATUS_SC, pt.cpl.status)));
	}
	printf("namespace %d attached\n", nsid);
	exit(0);
}

static void
nsdetach(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	int	fd, result;
	char	*path;
	uint32_t nsid;
	uint16_t clist[2048];

	if (arg_parse(argc, argv, f))
		return;
	open_dev(detach_opt.dev, &fd, 1, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 1, 1);
	} else if (detach_opt.nsid == NONE) {
		close(fd);
		fprintf(stderr, "No NSID specified\n");
		arg_help(argc, argv, f);
	}
	if (detach_opt.nsid != NONE)
		nsid = detach_opt.nsid;
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");

	/* Check that controller can execute this command. */
	if (NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, cd.oacs) == 0)
		errx(EX_UNAVAILABLE, "controller does not support namespace management");

	if (detach_opt.ctrlrid == NONE) {
		/* Get list of controllers this namespace attached to. */
		memset(&pt, 0, sizeof(pt));
		pt.cmd.opc = NVME_OPC_IDENTIFY;
		pt.cmd.nsid = htole32(nsid);
		pt.cmd.cdw10 = htole32(0x12);
		pt.buf = clist;
		pt.len = sizeof(clist);
		pt.is_read = 1;
		if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
			err(EX_IOERR, "identify request failed");
		if (nvme_completion_is_error(&pt.cpl))
			errx(EX_IOERR, "identify request returned error");
		if (clist[0] == 0) {
			detach_opt.ctrlrid = cd.ctrlr_id;
			memset(&clist, 0, sizeof(clist));
			clist[0] = htole16(1);
			clist[1] = htole16(detach_opt.ctrlrid);
		}
	} else {
		/* By default detach from this controller. */
		if (detach_opt.ctrlrid == NONE - 1)
			detach_opt.ctrlrid = cd.ctrlr_id;
		memset(&clist, 0, sizeof(clist));
		clist[0] = htole16(1);
		clist[1] = htole16(detach_opt.ctrlrid);
	}

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_NAMESPACE_ATTACHMENT;
	pt.cmd.cdw10 = htole32(1); /* detach */
	pt.cmd.nsid = nsid;
	pt.buf = &clist;
	pt.len = sizeof(clist);

	if ((result = ioctl(fd, NVME_PASSTHROUGH_CMD, &pt)) < 0)
		errx(EX_IOERR, "ioctl request to %s failed: %d", detach_opt.dev, result);

	if (nvme_completion_is_error(&pt.cpl)) {
		errx(EX_IOERR, "namespace detach failed: %s",
		    get_res_str(NVMEV(NVME_STATUS_SC, pt.cpl.status)));
	}
	printf("namespace %d detached\n", nsid);
	exit(0);
}

static void
nsattached(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	int	fd, i, n;
	char	*path;
	uint32_t nsid;
	uint16_t clist[2048];

	if (arg_parse(argc, argv, f))
		return;
	open_dev(attached_opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 1, 1);
	} else if (attached_opt.nsid == NONE) {
		close(fd);
		fprintf(stderr, "No NSID specified\n");
		arg_help(argc, argv, f);
	}
	if (attached_opt.nsid != NONE)
		nsid = attached_opt.nsid;
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");

	/* Check that controller can execute this command. */
	if (NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, cd.oacs) == 0)
		errx(EX_UNAVAILABLE, "controller does not support namespace management");

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_IDENTIFY;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32(0x12);
	pt.buf = clist;
	pt.len = sizeof(clist);
	pt.is_read = 1;
	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "identify request failed");
	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "identify request returned error");

	n = le16toh(clist[0]);
	printf("Attached %d controller(s):\n", n);
	for (i = 0; i < n; i++)
		printf("  0x%04x\n", le16toh(clist[i + 1]));

	exit(0);
}

static void
nsidentify(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	struct nvme_namespace_data nsdata;
	uint8_t	*data;
	int	fd;
	char	*path;
	uint32_t nsid;
	u_int	i;

	if (arg_parse(argc, argv, f))
		return;
	open_dev(identify_opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid != 0) {
		close(fd);
		open_dev(path, &fd, 1, 1);
	} else if (identify_opt.nsid == NONE) {
		close(fd);
		fprintf(stderr, "No NSID specified\n");
		arg_help(argc, argv, f);
	}
	if (identify_opt.nsid != NONE)
		nsid = identify_opt.nsid;
	if (read_controller_data(fd, &cd))
		errx(EX_IOERR, "Identify request failed");

	/* Check that controller can execute this command. */
	if (NVMEV(NVME_CTRLR_DATA_OACS_NSMGMT, cd.oacs) == 0)
		errx(EX_UNAVAILABLE, "controller does not support namespace management");

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_IDENTIFY;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32(0x11);
	pt.buf = &nsdata;
	pt.len = sizeof(nsdata);
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "identify request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "identify request returned error");

	close(fd);

	data = (uint8_t *)&nsdata;
	for (i = 0; i < sizeof(nsdata); i++) {
		if (data[i] != 0)
			break;
	}
	if (i == sizeof(nsdata))
		errx(EX_UNAVAILABLE, "namespace %d is not allocated", nsid);

	/* Convert data to host endian */
	nvme_namespace_data_swapbytes(&nsdata);

	if (identify_opt.hex) {
		i = sizeof(struct nvme_namespace_data);
		if (!identify_opt.verbose) {
			for (; i > 384; i--) {
				if (data[i - 1] != 0)
					break;
			}
		}
		print_hex(&nsdata, i);
		exit(0);
	}

	print_namespace(&nsdata);
	exit(0);
}

static void
ns(const struct cmd *nf __unused, int argc, char *argv[])
{

	cmd_dispatch(argc, argv, &ns_cmd);
}
