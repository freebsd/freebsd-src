/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <err.h>
#include <libnvmf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "comnd.h"
#include "fabrics.h"
#include "nvmecontrol_ext.h"

static struct options {
	const char	*transport;
	const char	*address;
	const char	*hostnqn;
	bool		verbose;
} opt = {
	.transport = "tcp",
	.address = NULL,
	.hostnqn = NULL,
	.verbose = false,
};

static void
identify_controller(struct nvmf_qpair *qp)
{
	struct nvme_controller_data cdata;
	int error;

	error = nvmf_host_identify_controller(qp, &cdata);
	if (error != 0)
		errc(EX_IOERR, error, "Failed to fetch controller data");
	nvme_print_controller(&cdata);
}

static const char *
nvmf_address_family(uint8_t adrfam)
{
	static char buf[8];

	switch (adrfam) {
	case NVMF_ADRFAM_IPV4:
		return ("AF_INET");
	case NVMF_ADRFAM_IPV6:
		return ("AF_INET6");
	case NVMF_ADRFAM_IB:
		return ("InfiniBand");
	case NVMF_ADRFAM_FC:
		return ("Fibre Channel");
	case NVMF_ADRFAM_INTRA_HOST:
		return ("Intra-host");
	default:
		snprintf(buf, sizeof(buf), "0x%02x\n", adrfam);
		return (buf);
	}
}

static const char *
nvmf_subsystem_type(uint8_t subtype)
{
	static char buf[8];

	switch (subtype) {
	case NVMF_SUBTYPE_DISCOVERY:
		return ("Discovery");
	case NVMF_SUBTYPE_NVME:
		return ("NVMe");
	default:
		snprintf(buf, sizeof(buf), "0x%02x\n", subtype);
		return (buf);
	}
}

static const char *
nvmf_secure_channel(uint8_t treq)
{
	switch (treq & 0x03) {
	case NVMF_TREQ_SECURE_CHANNEL_NOT_SPECIFIED:
		return ("Not specified");
	case NVMF_TREQ_SECURE_CHANNEL_REQUIRED:
		return ("Required");
	case NVMF_TREQ_SECURE_CHANNEL_NOT_REQUIRED:
		return ("Not required");
	default:
		return ("0x03");
	}
}

static const char *
nvmf_controller_id(uint16_t cntlid)
{
	static char buf[8];

	switch (cntlid) {
	case NVMF_CNTLID_DYNAMIC:
		return ("Dynamic");
	case NVMF_CNTLID_STATIC_ANY:
		return ("Static");
	default:
		snprintf(buf, sizeof(buf), "%u", cntlid);
		return (buf);
	}
}

static const char *
nvmf_rdma_service_type(uint8_t qptype)
{
	static char buf[8];

	switch (qptype) {
	case NVMF_RDMA_QPTYPE_RELIABLE_CONNECTED:
		return ("Reliable connected");
	case NVMF_RDMA_QPTYPE_RELIABLE_DATAGRAM:
		return ("Reliable datagram");
	default:
		snprintf(buf, sizeof(buf), "0x%02x\n", qptype);
		return (buf);
	}
}

static const char *
nvmf_rdma_provider_type(uint8_t prtype)
{
	static char buf[8];

	switch (prtype) {
	case NVMF_RDMA_PRTYPE_NONE:
		return ("None");
	case NVMF_RDMA_PRTYPE_IB:
		return ("InfiniBand");
	case NVMF_RDMA_PRTYPE_ROCE:
		return ("RoCE (v1)");
	case NVMF_RDMA_PRTYPE_ROCE2:
		return ("RoCE (v2)");
	case NVMF_RDMA_PRTYPE_IWARP:
		return ("iWARP");
	default:
		snprintf(buf, sizeof(buf), "0x%02x\n", prtype);
		return (buf);
	}
}

static const char *
nvmf_rdma_cms(uint8_t cms)
{
	static char buf[8];

	switch (cms) {
	case NVMF_RDMA_CMS_RDMA_CM:
		return ("RDMA_IP_CM");
	default:
		snprintf(buf, sizeof(buf), "0x%02x\n", cms);
		return (buf);
	}
}

static const char *
nvmf_tcp_security_type(uint8_t sectype)
{
	static char buf[8];

	switch (sectype) {
	case NVME_TCP_SECURITY_NONE:
		return ("None");
	case NVME_TCP_SECURITY_TLS_1_2:
		return ("TLS 1.2");
	case NVME_TCP_SECURITY_TLS_1_3:
		return ("TLS 1.3");
	default:
		snprintf(buf, sizeof(buf), "0x%02x\n", sectype);
		return (buf);
	}
}

static void
print_discovery_entry(u_int i, struct nvme_discovery_log_entry *entry)
{
	printf("Entry %02d\n", i + 1);
	printf("========\n");
	printf(" Transport type:       %s\n",
	    nvmf_transport_type(entry->trtype));
	printf(" Address family:       %s\n",
	    nvmf_address_family(entry->adrfam));
	printf(" Subsystem type:       %s\n",
	    nvmf_subsystem_type(entry->subtype));
	printf(" SQ flow control:      %s\n",
	    (entry->treq & (1 << 2)) == 0 ? "required" : "optional");
	printf(" Secure Channel:       %s\n", nvmf_secure_channel(entry->treq));
	printf(" Port ID:              %u\n", entry->portid);
	printf(" Controller ID:        %s\n",
	    nvmf_controller_id(entry->cntlid));
	printf(" Max Admin SQ Size:    %u\n", entry->aqsz);
	printf(" Sub NQN:              %s\n", entry->subnqn);
	printf(" Transport address:    %s\n", entry->traddr);
	printf(" Service identifier:   %s\n", entry->trsvcid);
	switch (entry->trtype) {
	case NVMF_TRTYPE_RDMA:
		printf(" RDMA Service Type:    %s\n",
		    nvmf_rdma_service_type(entry->tsas.rdma.rdma_qptype));
		printf(" RDMA Provider Type:   %s\n",
		    nvmf_rdma_provider_type(entry->tsas.rdma.rdma_prtype));
		printf(" RDMA CMS:             %s\n",
		    nvmf_rdma_cms(entry->tsas.rdma.rdma_cms));
		printf(" Partition key:        %u\n",
		    entry->tsas.rdma.rdma_pkey);
		break;
	case NVMF_TRTYPE_TCP:
		printf(" Security Type:        %s\n",
		    nvmf_tcp_security_type(entry->tsas.tcp.sectype));
		break;
	}
}

static void
dump_discovery_log_page(struct nvmf_qpair *qp)
{
	struct nvme_discovery_log *log;
	int error;

	error = nvmf_host_fetch_discovery_log_page(qp, &log);
	if (error != 0)
		errc(EX_IOERR, error, "Failed to fetch discovery log page");

	printf("Discovery\n");
	printf("=========\n");
	if (log->numrec == 0) {
		printf("No entries found\n");
	} else {
		for (u_int i = 0; i < log->numrec; i++)
			print_discovery_entry(i, &log->entries[i]);
	}
	free(log);
}

static void
discover(const struct cmd *f, int argc, char *argv[])
{
	enum nvmf_trtype trtype;
	struct nvmf_qpair *qp;
	const char *address, *port;
	char *tofree;

	if (arg_parse(argc, argv, f))
		return;

	if (strcasecmp(opt.transport, "tcp") == 0) {
		trtype = NVMF_TRTYPE_TCP;
	} else
		errx(EX_USAGE, "Unsupported or invalid transport");

	nvmf_parse_address(opt.address, &address, &port, &tofree);
	qp = connect_discovery_adminq(trtype, address, port, opt.hostnqn);
	free(tofree);

	/* Use Identify to fetch controller data */
	if (opt.verbose) {
		identify_controller(qp);
		printf("\n");
	}

	/* Fetch Log pages */
	dump_discovery_log_page(qp);

	nvmf_free_qpair(qp);
}

static const struct opts discover_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("transport", 't', arg_string, opt, transport,
	    "Transport type"),
	OPT("hostnqn", 'q', arg_string, opt, hostnqn,
	    "Host NQN"),
	OPT("verbose", 'v', arg_none, opt, verbose,
	    "Display the discovery controller's controller data"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args discover_args[] = {
	{ arg_string, &opt.address, "address" },
	{ arg_none, NULL, NULL },
};

static struct cmd discover_cmd = {
	.name = "discover",
	.fn = discover,
	.descr = "List discovery log pages from a fabrics controller",
	.ctx_size = sizeof(opt),
	.opts = discover_opts,
	.args = discover_args,
};

CMD_COMMAND(discover_cmd);
