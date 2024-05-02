/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/socket.h>
#include <err.h>
#include <libnvmf.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "nvmecontrol.h"
#include "fabrics.h"

/*
 * See comment about other possible settings in connect.c.
 */

static struct options {
	const char	*dev;
	const char	*transport;
	const char	*address;
	const char	*hostnqn;
	uint32_t	kato;
	uint16_t	num_io_queues;
	uint16_t	queue_size;
	bool		data_digests;
	bool		flow_control;
	bool		header_digests;
} opt = {
	.dev = NULL,
	.transport = "tcp",
	.address = NULL,
	.hostnqn = NULL,
	.kato = NVMF_KATO_DEFAULT / 1000,
	.num_io_queues = 1,
	.queue_size = 0,
	.data_digests = false,
	.flow_control = false,
	.header_digests = false,
};

static void
tcp_association_params(struct nvmf_association_params *params)
{
	params->tcp.pda = 0;
	params->tcp.header_digests = opt.header_digests;
	params->tcp.data_digests = opt.data_digests;
	/* XXX */
	params->tcp.maxr2t = 1;
}

static int
reconnect_nvm_controller(int fd, enum nvmf_trtype trtype, int adrfam,
    const char *address, const char *port)
{
	struct nvme_controller_data cdata;
	struct nvmf_association_params aparams;
	struct nvmf_reconnect_params rparams;
	struct nvmf_qpair *admin, **io;
	int error;

	error = nvmf_reconnect_params(fd, &rparams);
	if (error != 0) {
		warnc(error, "Failed to fetch reconnect parameters");
		return (EX_IOERR);
	}

	memset(&aparams, 0, sizeof(aparams));
	aparams.sq_flow_control = opt.flow_control;
	switch (trtype) {
	case NVMF_TRTYPE_TCP:
		tcp_association_params(&aparams);
		break;
	default:
		warnx("Unsupported transport %s", nvmf_transport_type(trtype));
		return (EX_UNAVAILABLE);
	}

	io = calloc(opt.num_io_queues, sizeof(*io));
	error = connect_nvm_queues(&aparams, trtype, adrfam, address, port,
	    rparams.cntlid, rparams.subnqn, opt.hostnqn, opt.kato, &admin, io,
	    opt.num_io_queues, opt.queue_size, &cdata);
	if (error != 0)
		return (error);

	error = nvmf_reconnect_host(fd, admin, opt.num_io_queues, io, &cdata);
	if (error != 0) {
		warnc(error, "Failed to handoff queues to kernel");
		return (EX_IOERR);
	}
	free(io);
	return (0);
}

static void
reconnect_fn(const struct cmd *f, int argc, char *argv[])
{
	enum nvmf_trtype trtype;
	const char *address, *port;
	char *tofree;
	int error, fd;

	if (arg_parse(argc, argv, f))
		return;

	if (strcasecmp(opt.transport, "tcp") == 0) {
		trtype = NVMF_TRTYPE_TCP;
	} else
		errx(EX_USAGE, "Unsupported or invalid transport");

	nvmf_parse_address(opt.address, &address, &port, &tofree);

	open_dev(opt.dev, &fd, 1, 1);
	if (port == NULL)
		errx(EX_USAGE, "Explicit port required");

	error = reconnect_nvm_controller(fd, trtype, AF_UNSPEC, address, port);
	if (error != 0)
		exit(error);

	close(fd);
	free(tofree);
}

static const struct opts reconnect_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("transport", 't', arg_string, opt, transport,
	    "Transport type"),
	OPT("nr-io-queues", 'i', arg_uint16, opt, num_io_queues,
	    "Number of I/O queues"),
	OPT("queue-size", 'Q', arg_uint16, opt, queue_size,
	    "Number of entries in each I/O queue"),
	OPT("keep-alive-tmo", 'k', arg_uint32, opt, kato,
	    "Keep Alive timeout (in seconds)"),
	OPT("hostnqn", 'q', arg_string, opt, hostnqn,
	    "Host NQN"),
	OPT("flow_control", 'F', arg_none, opt, flow_control,
	    "Request SQ flow control"),
	OPT("hdr_digests", 'g', arg_none, opt, header_digests,
	    "Enable TCP PDU header digests"),
	OPT("data_digests", 'G', arg_none, opt, data_digests,
	    "Enable TCP PDU data digests"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args reconnect_args[] = {
	{ arg_string, &opt.dev, "controller-id" },
	{ arg_string, &opt.address, "address" },
	{ arg_none, NULL, NULL },
};

static struct cmd reconnect_cmd = {
	.name = "reconnect",
	.fn = reconnect_fn,
	.descr = "Reconnect to a fabrics controller",
	.ctx_size = sizeof(opt),
	.opts = reconnect_opts,
	.args = reconnect_args,
};

CMD_COMMAND(reconnect_cmd);
