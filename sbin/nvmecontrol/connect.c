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

#include "comnd.h"
#include "fabrics.h"

/*
 * Settings that are currently hardcoded but could be exposed to the
 * user via additional command line options:
 *
 * - ADMIN queue entries
 * - MaxR2T
 */

static struct options {
	const char	*transport;
	const char	*address;
	const char	*cntlid;
	const char	*subnqn;
	const char	*hostnqn;
	uint32_t	kato;
	uint32_t	reconnect_delay;
	uint32_t	controller_loss_timeout;
	uint16_t	num_io_queues;
	uint16_t	queue_size;
	bool		data_digests;
	bool		flow_control;
	bool		header_digests;
} opt = {
	.transport = "tcp",
	.address = NULL,
	.cntlid = "dynamic",
	.subnqn = NULL,
	.hostnqn = NULL,
	.kato = NVMF_KATO_DEFAULT / 1000,
	.reconnect_delay = NVMF_DEFAULT_RECONNECT_DELAY,
	.controller_loss_timeout = NVMF_DEFAULT_CONTROLLER_LOSS,
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
connect_nvm_controller(enum nvmf_trtype trtype, int adrfam, const char *address,
    const char *port, uint16_t cntlid, const char *subnqn,
    const struct nvme_discovery_log_entry *dle)
{
	struct nvme_controller_data cdata;
	struct nvme_discovery_log_entry dle_thunk;
	struct nvmf_association_params aparams;
	struct nvmf_qpair *admin, **io;
	const char *hostnqn;
	int error;

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

	hostnqn = opt.hostnqn;
	if (hostnqn == NULL)
		hostnqn = nvmf_default_hostnqn();
	io = calloc(opt.num_io_queues, sizeof(*io));
	error = connect_nvm_queues(&aparams, trtype, adrfam, address, port,
	    cntlid, subnqn, hostnqn, opt.kato * 1000, &admin, io,
	    opt.num_io_queues, opt.queue_size, &cdata);
	if (error != 0) {
		free(io);
		return (error);
	}

	if (dle == NULL) {
		error = nvmf_init_dle_from_admin_qp(admin, &cdata, &dle_thunk);
		if (error != 0) {
			warnc(error, "Failed to generate handoff parameters");
			disconnect_nvm_queues(admin, io, opt.num_io_queues);
			free(io);
			return (EX_IOERR);
		}
		dle = &dle_thunk;
	}

	error = nvmf_handoff_host(dle, hostnqn, admin, opt.num_io_queues, io,
	    &cdata, opt.reconnect_delay, opt.controller_loss_timeout);
	if (error != 0) {
		warnc(error, "Failed to handoff queues to kernel");
		free(io);
		return (EX_IOERR);
	}
	free(io);
	return (0);
}

static void
connect_discovery_entry(struct nvme_discovery_log_entry *entry)
{
	int adrfam;

	switch (entry->trtype) {
	case NVMF_TRTYPE_TCP:
		switch (entry->adrfam) {
		case NVMF_ADRFAM_IPV4:
			adrfam = AF_INET;
			break;
		case NVMF_ADRFAM_IPV6:
			adrfam = AF_INET6;
			break;
		default:
			warnx("Skipping unsupported address family for %s",
			    entry->subnqn);
			return;
		}
		switch (entry->tsas.tcp.sectype) {
		case NVME_TCP_SECURITY_NONE:
			break;
		default:
			warnx("Skipping unsupported TCP security type for %s",
			    entry->subnqn);
			return;
		}
		break;
	default:
		warnx("Skipping unsupported transport %s for %s",
		    nvmf_transport_type(entry->trtype), entry->subnqn);
		return;
	}

	/*
	 * XXX: Track portids and avoid duplicate connections for a
	 * given (subnqn,portid)?
	 */

	/* XXX: Should this make use of entry->aqsz in some way? */
	connect_nvm_controller(entry->trtype, adrfam, entry->traddr,
	    entry->trsvcid, entry->cntlid, entry->subnqn, entry);
}

static void
connect_discovery_log_page(struct nvmf_qpair *qp)
{
	struct nvme_discovery_log *log;
	int error;

	error = nvmf_host_fetch_discovery_log_page(qp, &log);
	if (error != 0)
		errc(EX_IOERR, error, "Failed to fetch discovery log page");

	for (u_int i = 0; i < log->numrec; i++)
		connect_discovery_entry(&log->entries[i]);
	free(log);
}

static void
discover_controllers(enum nvmf_trtype trtype, const char *address,
    const char *port)
{
	struct nvmf_qpair *qp;

	qp = connect_discovery_adminq(trtype, address, port, opt.hostnqn);

	connect_discovery_log_page(qp);

	nvmf_free_qpair(qp);
}

static void
connect_fn(const struct cmd *f, int argc, char *argv[])
{
	enum nvmf_trtype trtype;
	const char *address, *port;
	char *tofree;
	u_long cntlid;
	int error;

	if (arg_parse(argc, argv, f))
		return;

	if (opt.num_io_queues <= 0)
		errx(EX_USAGE, "Invalid number of I/O queues");

	if (strcasecmp(opt.transport, "tcp") == 0) {
		trtype = NVMF_TRTYPE_TCP;
	} else
		errx(EX_USAGE, "Unsupported or invalid transport");

	nvmf_parse_address(opt.address, &address, &port, &tofree);
	if (port == NULL)
		errx(EX_USAGE, "Explicit port required");

	cntlid = nvmf_parse_cntlid(opt.cntlid);

	error = connect_nvm_controller(trtype, AF_UNSPEC, address, port, cntlid,
	    opt.subnqn, NULL);
	if (error != 0)
		exit(error);

	free(tofree);
}

static void
connect_all_fn(const struct cmd *f, int argc, char *argv[])
{
	enum nvmf_trtype trtype;
	const char *address, *port;
	char *tofree;

	if (arg_parse(argc, argv, f))
		return;

	if (opt.num_io_queues <= 0)
		errx(EX_USAGE, "Invalid number of I/O queues");

	if (strcasecmp(opt.transport, "tcp") == 0) {
		trtype = NVMF_TRTYPE_TCP;
	} else
		errx(EX_USAGE, "Unsupported or invalid transport");

	nvmf_parse_address(opt.address, &address, &port, &tofree);
	discover_controllers(trtype, address, port);

	free(tofree);
}

static const struct opts connect_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("transport", 't', arg_string, opt, transport,
	    "Transport type"),
	OPT("cntlid", 'c', arg_string, opt, cntlid,
	    "Controller ID"),
	OPT("nr-io-queues", 'i', arg_uint16, opt, num_io_queues,
	    "Number of I/O queues"),
	OPT("queue-size", 'Q', arg_uint16, opt, queue_size,
	    "Number of entries in each I/O queue"),
	OPT("keep-alive-tmo", 'k', arg_uint32, opt, kato,
	    "Keep Alive timeout (in seconds)"),
	OPT("reconnect-delay", 'r', arg_uint32, opt, reconnect_delay,
	    "Delay between reconnect attempts after connection loss "
	    "(in seconds)"),
	OPT("ctrl-loss-tmo", 'l', arg_uint32, opt, controller_loss_timeout,
	    "Controller loss timeout after connection loss (in seconds)"),
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

static const struct args connect_args[] = {
	{ arg_string, &opt.address, "address" },
	{ arg_string, &opt.subnqn, "SubNQN" },
	{ arg_none, NULL, NULL },
};

static const struct args connect_all_args[] = {
	{ arg_string, &opt.address, "address" },
	{ arg_none, NULL, NULL },
};

static struct cmd connect_cmd = {
	.name = "connect",
	.fn = connect_fn,
	.descr = "Connect to a fabrics controller",
	.ctx_size = sizeof(opt),
	.opts = connect_opts,
	.args = connect_args,
};

static struct cmd connect_all_cmd = {
	.name = "connect-all",
	.fn = connect_all_fn,
	.descr = "Discover and connect to fabrics controllers",
	.ctx_size = sizeof(opt),
	.opts = connect_opts,
	.args = connect_all_args,
};

CMD_COMMAND(connect_cmd);
CMD_COMMAND(connect_all_cmd);
