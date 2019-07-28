/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017 Netflix, Inc.
 * Copyright (C) 2018 Alexander Motin <mav@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ioccom.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvmecontrol.h"

/* Tables for command line parsing */

static cmd_fn_t ns;
static cmd_fn_t nscreate;
static cmd_fn_t nsdelete;
static cmd_fn_t nsattach;
static cmd_fn_t nsdetach;

#define NONE 0xffffffffu
#define NONE64 0xffffffffffffffffull
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
#define OPT_END	{ NULL, 0, arg_none, NULL, NULL }

static struct cmd ns_cmd = {
	.name = "ns", .fn = ns, .descr = "Namespace commands", .ctx_size = 0, .opts = NULL, .args = NULL,
};

CMD_COMMAND(ns_cmd);

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
	OPT("flbas", 'l', arg_uint32, create_opt, flbas,
	    "Namespace formatted logical block size setting"),
	OPT("dps", 'd', arg_uint32, create_opt, dps,
	    "Data protection settings"),
//	OPT("block-size", 'b', arg_uint32, create_opt, block_size,
//	    "Blocksize of the namespace"),
	OPT_END
};

static const struct args create_args[] = {
	{ arg_string, &create_opt.dev, "controller-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd create_cmd = {
	.name = "create",
	.fn = nscreate,
	.descr = "Create a new namespace",
	.ctx_size = sizeof(create_opt),
	.opts = create_opts,
	.args = create_args,
};

CMD_SUBCOMMAND(ns_cmd, create_cmd);

static struct delete_options {
	uint32_t	nsid;
	const char	*dev;
} delete_opt = {
	.nsid = NONE,
	.dev = NULL,
};

static const struct opts delete_opts[] = {
	OPT("namespace-id", 'n', arg_uint32, delete_opt, nsid,
	    "The namespace ID to delete"),
	OPT_END
};

static const struct args delete_args[] = {
	{ arg_string, &delete_opt.dev, "controller-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd delete_cmd = {
	.name = "delete",
	.fn = nsdelete,
	.descr = "Delete a new namespace",
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
	OPT("controller", 'c', arg_uint32, attach_opt, nsid,
	    "The controller ID to attach"),
	OPT_END
};

static const struct args attach_args[] = {
	{ arg_string, &attach_opt.dev, "controller-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd attach_cmd = {
	.name = "attach",
	.fn = nsattach,
	.descr = "Attach a new namespace",
	.ctx_size = sizeof(attach_opt),
	.opts = attach_opts,
	.args = attach_args,
};

CMD_SUBCOMMAND(ns_cmd, attach_cmd);

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
	OPT("controller", 'c', arg_uint32, detach_opt, nsid,
	    "The controller ID to detach"),
	OPT_END
};

static const struct args detach_args[] = {
	{ arg_string, &detach_opt.dev, "controller-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd detach_cmd = {
	.name = "detach",
	.fn = nsdetach,
	.descr = "Detach a new namespace",
	.ctx_size = sizeof(detach_opt),
	.opts = detach_opts,
	.args = detach_args,
};

CMD_SUBCOMMAND(ns_cmd, detach_cmd);

#define NS_USAGE							\
	"ns (create|delete|attach|detach)\n"

/* handles NVME_OPC_NAMESPACE_MANAGEMENT and ATTACHMENT admin cmds */

struct ns_result_str {
	uint16_t res;
	const char * str;
};

static struct ns_result_str ns_result[] = {
	{ 0x2,  "Invalid Field"},
	{ 0xa,  "Invalid Format"},
	{ 0xb,  "Invalid Namespace or format"},
	{ 0x15, "Namespace insufficent capacity"},
	{ 0x16, "Namespace ID unavaliable"},
	{ 0x18, "Namespace already attached"},
	{ 0x19, "Namespace is private"},
	{ 0x1a, "Namespace is not attached"},
	{ 0x1b, "Thin provisioning not supported"},
	{ 0x1c, "Controller list invalid"},
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

/*
 * NS MGMT Command specific status values:
 * 0xa = Invalid Format
 * 0x15 = Namespace Insuffience capacity
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
	read_controller_data(fd, &cd);

	/* Check that controller can execute this command. */
	if (((cd.oacs >> NVME_CTRLR_DATA_OACS_NSMGMT_SHIFT) &
	    NVME_CTRLR_DATA_OACS_NSMGMT_MASK) == 0)
		errx(1, "controller does not support namespace management");

	/* Allow namespaces sharing if Multi-Path I/O is supported. */
	if (create_opt.nmic == NONE) {
		create_opt.nmic = cd.mic ? (NVME_NS_DATA_NMIC_MAY_BE_SHARED_MASK <<
		     NVME_NS_DATA_NMIC_MAY_BE_SHARED_SHIFT) : 0;
	}

	memset(&nsdata, 0, sizeof(nsdata));
	nsdata.nsze = create_opt.nsze;
	nsdata.ncap = create_opt.cap;
	if (create_opt.flbas == NONE)
		nsdata.flbas = ((create_opt.lbaf & NVME_NS_DATA_FLBAS_FORMAT_MASK)
		    << NVME_NS_DATA_FLBAS_FORMAT_SHIFT) |
		    ((create_opt.mset & NVME_NS_DATA_FLBAS_EXTENDED_MASK)
			<< NVME_NS_DATA_FLBAS_EXTENDED_SHIFT);
	else
		nsdata.flbas = create_opt.flbas;
	if (create_opt.dps == NONE)
		nsdata.dps = ((create_opt.pi & NVME_NS_DATA_DPS_MD_START_MASK)
		    << NVME_NS_DATA_DPS_MD_START_SHIFT) |
		    ((create_opt.pil & NVME_NS_DATA_DPS_PIT_MASK)
			<< NVME_NS_DATA_DPS_PIT_SHIFT);
	else
		nsdata.dps = create_opt.dps;
	nsdata.nmic = create_opt.nmic;
	nvme_namespace_data_swapbytes(&nsdata);

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_NAMESPACE_MANAGEMENT;

	pt.cmd.cdw10 = 0; /* create */
	pt.buf = &nsdata;
	pt.len = sizeof(struct nvme_namespace_data);
	pt.is_read = 0; /* passthrough writes data to ctrlr */
	if ((result = ioctl(fd, NVME_PASSTHROUGH_CMD, &pt)) < 0)
		errx(1, "ioctl request to %s failed: %d", argv[optind], result);

	if (nvme_completion_is_error(&pt.cpl)) {
		errx(1, "namespace creation failed: %s",
		    get_res_str((pt.cpl.status >> NVME_STATUS_SC_SHIFT) &
		    NVME_STATUS_SC_MASK));
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
	char buf[2];

	if (arg_parse(argc, argv, f))
		return;
	if (delete_opt.nsid == NONE) {
		fprintf(stderr,
		    "No NSID specified");
		arg_help(argc, argv, f);
	}

	open_dev(delete_opt.dev, &fd, 1, 1);
	read_controller_data(fd, &cd);

	/* Check that controller can execute this command. */
	if (((cd.oacs >> NVME_CTRLR_DATA_OACS_NSMGMT_SHIFT) &
	    NVME_CTRLR_DATA_OACS_NSMGMT_MASK) == 0)
		errx(1, "controller does not support namespace management");

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_NAMESPACE_MANAGEMENT;
	pt.cmd.cdw10 = 1; /* delete */
	pt.buf = buf;
	pt.len = sizeof(buf);
	pt.is_read = 1;
	pt.cmd.nsid = delete_opt.nsid;

	if ((result = ioctl(fd, NVME_PASSTHROUGH_CMD, &pt)) < 0)
		errx(1, "ioctl request to %s failed: %d", delete_opt.dev, result);

	if (nvme_completion_is_error(&pt.cpl)) {
		errx(1, "namespace deletion failed: %s",
		    get_res_str((pt.cpl.status >> NVME_STATUS_SC_SHIFT) &
		    NVME_STATUS_SC_MASK));
	}
	printf("namespace %d deleted\n", delete_opt.nsid);
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
	uint16_t clist[2048];

	if (arg_parse(argc, argv, f))
		return;
	if (attach_opt.nsid == NONE) {
		fprintf(stderr, "No valid NSID specified\n");
		arg_help(argc, argv, f);
	}
	open_dev(attach_opt.dev, &fd, 1, 1);
	read_controller_data(fd, &cd);

	/* Check that controller can execute this command. */
	if (((cd.oacs >> NVME_CTRLR_DATA_OACS_NSMGMT_SHIFT) &
	    NVME_CTRLR_DATA_OACS_NSMGMT_MASK) == 0)
		errx(1, "controller does not support namespace management");

	if (attach_opt.ctrlrid == NONE) {
		/* Get full list of controllers to attach to. */
		memset(&pt, 0, sizeof(pt));
		pt.cmd.opc = NVME_OPC_IDENTIFY;
		pt.cmd.cdw10 = htole32(0x13);
		pt.buf = clist;
		pt.len = sizeof(clist);
		pt.is_read = 1;
		if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
			err(1, "identify request failed");
		if (nvme_completion_is_error(&pt.cpl))
			errx(1, "identify request returned error");
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
	pt.cmd.cdw10 = 0; /* attach */
	pt.cmd.nsid = attach_opt.nsid;
	pt.buf = &clist;
	pt.len = sizeof(clist);

	if ((result = ioctl(fd, NVME_PASSTHROUGH_CMD, &pt)) < 0)
		errx(1, "ioctl request to %s failed: %d", attach_opt.dev, result);

	if (nvme_completion_is_error(&pt.cpl)) {
		errx(1, "namespace attach failed: %s",
		    get_res_str((pt.cpl.status >> NVME_STATUS_SC_SHIFT) &
		    NVME_STATUS_SC_MASK));
	}
	printf("namespace %d attached\n", attach_opt.nsid);
	exit(0);
}

static void
nsdetach(const struct cmd *f, int argc, char *argv[])
{
	struct nvme_pt_command	pt;
	struct nvme_controller_data cd;
	int	fd, result;
	uint16_t clist[2048];

	if (arg_parse(argc, argv, f))
		return;
	if (attach_opt.nsid == NONE) {
		fprintf(stderr, "No valid NSID specified\n");
		arg_help(argc, argv, f);
	}
	open_dev(attach_opt.dev, &fd, 1, 1);
	read_controller_data(fd, &cd);

	/* Check that controller can execute this command. */
	if (((cd.oacs >> NVME_CTRLR_DATA_OACS_NSMGMT_SHIFT) &
	    NVME_CTRLR_DATA_OACS_NSMGMT_MASK) == 0)
		errx(1, "controller does not support namespace management");

	if (detach_opt.ctrlrid == NONE) {
		/* Get list of controllers this namespace attached to. */
		memset(&pt, 0, sizeof(pt));
		pt.cmd.opc = NVME_OPC_IDENTIFY;
		pt.cmd.nsid = htole32(detach_opt.nsid);
		pt.cmd.cdw10 = htole32(0x12);
		pt.buf = clist;
		pt.len = sizeof(clist);
		pt.is_read = 1;
		if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
			err(1, "identify request failed");
		if (nvme_completion_is_error(&pt.cpl))
			errx(1, "identify request returned error");
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
	pt.cmd.cdw10 = 1; /* detach */
	pt.cmd.nsid = detach_opt.nsid;
	pt.buf = &clist;
	pt.len = sizeof(clist);

	if ((result = ioctl(fd, NVME_PASSTHROUGH_CMD, &pt)) < 0)
		errx(1, "ioctl request to %s failed: %d", argv[optind], result);

	if (nvme_completion_is_error(&pt.cpl)) {
		errx(1, "namespace detach failed: %s",
		    get_res_str((pt.cpl.status >> NVME_STATUS_SC_SHIFT) &
		    NVME_STATUS_SC_MASK));
	}
	printf("namespace %d detached\n", detach_opt.nsid);
	exit(0);
}

static void
ns(const struct cmd *nf __unused, int argc, char *argv[])
{

	cmd_dispatch(argc, argv, &ns_cmd);
}
