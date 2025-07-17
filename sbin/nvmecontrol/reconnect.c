/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/dnv.h>
#include <sys/nv.h>
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
	.dev = NULL,
	.transport = "tcp",
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
tcp_association_params(struct nvmf_association_params *params,
    bool header_digests, bool data_digests)
{
	params->tcp.pda = 0;
	params->tcp.header_digests = header_digests;
	params->tcp.data_digests = data_digests;
	/* XXX */
	params->tcp.maxr2t = 1;
}

static int
reconnect_nvm_controller(int fd, const struct nvmf_association_params *aparams,
    enum nvmf_trtype trtype, int adrfam, const char *address, const char *port,
    uint16_t cntlid, const char *subnqn, const char *hostnqn, uint32_t kato,
    uint32_t reconnect_delay, uint32_t controller_loss_timeout,
    u_int num_io_queues, u_int queue_size,
    const struct nvme_discovery_log_entry *dle)
{
	struct nvme_controller_data cdata;
	struct nvme_discovery_log_entry dle_thunk;
	struct nvmf_qpair *admin, **io;
	int error;

	io = calloc(num_io_queues, sizeof(*io));
	error = connect_nvm_queues(aparams, trtype, adrfam, address, port,
	    cntlid, subnqn, hostnqn, kato, &admin, io, num_io_queues,
	    queue_size, &cdata);
	if (error != 0) {
		free(io);
		return (error);
	}

	if (dle == NULL) {
		error = nvmf_init_dle_from_admin_qp(admin, &cdata, &dle_thunk);
		if (error != 0) {
			warnc(error, "Failed to generate handoff parameters");
			disconnect_nvm_queues(admin, io, num_io_queues);
			free(io);
			return (EX_IOERR);
		}
		dle = &dle_thunk;
	}

	error = nvmf_reconnect_host(fd, dle, hostnqn, admin, num_io_queues, io,
	    &cdata, reconnect_delay, controller_loss_timeout);
	if (error != 0) {
		warnc(error, "Failed to handoff queues to kernel");
		free(io);
		return (EX_IOERR);
	}
	free(io);
	return (0);
}

static int
reconnect_by_address(int fd, const nvlist_t *rparams, const char *addr)
{
	const struct nvme_discovery_log_entry *dle;
	struct nvmf_association_params aparams;
	enum nvmf_trtype trtype;
	const char *address, *hostnqn, *port;
	char *subnqn, *tofree;
	int error;

	memset(&aparams, 0, sizeof(aparams));
	aparams.sq_flow_control = opt.flow_control;
	if (strcasecmp(opt.transport, "tcp") == 0) {
		trtype = NVMF_TRTYPE_TCP;
		tcp_association_params(&aparams, opt.header_digests,
		    opt.data_digests);
	} else {
		warnx("Unsupported or invalid transport");
		return (EX_USAGE);
	}

	nvmf_parse_address(addr, &address, &port, &tofree);
	if (port == NULL) {
		free(tofree);
		warnx("Explicit port required");
		return (EX_USAGE);
	}

	dle = nvlist_get_binary(rparams, "dle", NULL);

	hostnqn = opt.hostnqn;
	if (hostnqn == NULL)
		hostnqn = nvmf_default_hostnqn();

	/* Ensure subnqn is a terminated C string. */
	subnqn = strndup(dle->subnqn, sizeof(dle->subnqn));

	error = reconnect_nvm_controller(fd, &aparams, trtype, AF_UNSPEC,
	    address, port, le16toh(dle->cntlid), subnqn, hostnqn,
	    opt.kato * 1000, opt.reconnect_delay, opt.controller_loss_timeout,
	    opt.num_io_queues, opt.queue_size, NULL);
	free(subnqn);
	free(tofree);
	return (error);
}

static int
reconnect_by_params(int fd, const nvlist_t *rparams)
{
	struct nvmf_association_params aparams;
	const struct nvme_discovery_log_entry *dle;
	char *address, *port, *subnqn;
	int adrfam, error;

	dle = nvlist_get_binary(rparams, "dle", NULL);

	memset(&aparams, 0, sizeof(aparams));
	aparams.sq_flow_control = nvlist_get_bool(rparams, "sq_flow_control");
	switch (dle->trtype) {
	case NVMF_TRTYPE_TCP:
		switch (dle->adrfam) {
		case NVMF_ADRFAM_IPV4:
			adrfam = AF_INET;
			break;
		case NVMF_ADRFAM_IPV6:
			adrfam = AF_INET6;
			break;
		default:
			warnx("Unsupported address family");
			return (EX_UNAVAILABLE);
		}
		switch (dle->tsas.tcp.sectype) {
		case NVME_TCP_SECURITY_NONE:
			break;
		default:
			warnx("Unsupported TCP security type");
			return (EX_UNAVAILABLE);
		}
		break;

		tcp_association_params(&aparams,
		    nvlist_get_bool(rparams, "header_digests"),
		    nvlist_get_bool(rparams, "data_digests"));
		break;
	default:
		warnx("Unsupported transport %s",
		    nvmf_transport_type(dle->trtype));
		return (EX_UNAVAILABLE);
	}

	/* Ensure address, port, and subnqn is a terminated C string. */
	address = strndup(dle->traddr, sizeof(dle->traddr));
	port = strndup(dle->trsvcid, sizeof(dle->trsvcid));
	subnqn = strndup(dle->subnqn, sizeof(dle->subnqn));

	error = reconnect_nvm_controller(fd, &aparams, dle->trtype, adrfam,
	    address, port, le16toh(dle->cntlid), dle->subnqn,
	    nvlist_get_string(rparams, "hostnqn"),
	    dnvlist_get_number(rparams, "kato", 0),
	    dnvlist_get_number(rparams, "reconnect_delay", 0),
	    dnvlist_get_number(rparams, "controller_loss_timeout", 0),
	    nvlist_get_number(rparams, "num_io_queues"),
	    nvlist_get_number(rparams, "io_qsize"), dle);
	free(subnqn);
	free(port);
	free(address);
	return (error);
}

static int
fetch_and_validate_rparams(int fd, nvlist_t **rparamsp)
{
	const struct nvme_discovery_log_entry *dle;
	nvlist_t *rparams;
	size_t len;
	int error;

	error = nvmf_reconnect_params(fd, &rparams);
	if (error != 0) {
		warnc(error, "Failed to fetch reconnect parameters");
		return (EX_IOERR);
	}

	if (!nvlist_exists_binary(rparams, "dle") ||
	    !nvlist_exists_string(rparams, "hostnqn") ||
	    !nvlist_exists_number(rparams, "num_io_queues") ||
	    !nvlist_exists_number(rparams, "io_qsize") ||
	    !nvlist_exists_bool(rparams, "sq_flow_control")) {
		nvlist_destroy(rparams);
		warnx("Missing required reconnect parameters");
		return (EX_IOERR);
	}

	dle = nvlist_get_binary(rparams, "dle", &len);
	if (len != sizeof(*dle)) {
		nvlist_destroy(rparams);
		warnx("Discovery Log entry reconnect parameter is wrong size");
		return (EX_IOERR);
	}

	switch (dle->trtype) {
	case NVMF_TRTYPE_TCP:
		if (!nvlist_exists_bool(rparams, "header_digests") ||
		    !nvlist_exists_bool(rparams, "data_digests")) {
			nvlist_destroy(rparams);
			warnx("Missing required reconnect parameters");
			return (EX_IOERR);
		}
		break;
	default:
		nvlist_destroy(rparams);
		warnx("Unsupported transport %s",
		    nvmf_transport_type(dle->trtype));
		return (EX_UNAVAILABLE);
	}

	*rparamsp = rparams;
	return (0);
}

static void
reconnect_fn(const struct cmd *f, int argc, char *argv[])
{
	nvlist_t *rparams;
	int error, fd;

	if (arg_parse(argc, argv, f))
		return;

	open_dev(opt.dev, &fd, 1, 1);
	error = fetch_and_validate_rparams(fd, &rparams);
	if (error != 0)
		exit(error);

	/* Check for optional address. */
	if (optind < argc)
		error = reconnect_by_address(fd, rparams, argv[optind]);
	else
		error = reconnect_by_params(fd, rparams);
	if (error != 0)
		exit(error);

	nvlist_destroy(rparams);
	close(fd);
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

static const struct args reconnect_args[] = {
	{ arg_string, &opt.dev, "controller-id" },
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
