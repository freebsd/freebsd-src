/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/socket.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <libnvmf.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <netinet/in.h>

static struct controller_info {
	uint32_t ioccsz;
	uint32_t nn;
	uint16_t mqes;
	bool	disconnect_supported;
} info;

enum rw { READ, WRITE };

static bool data_digests, flow_control_disable, header_digests;

static void
usage(void)
{
	fprintf(stderr, "nvmfdd [-FGg] [-c cntlid] [-t transport] [-o offset] [-l length] [-n nsid]\n"
	    "\tread|write <address:port> <nqn>\n");
	exit(1);
}

static void
tcp_association_params(struct nvmf_association_params *params)
{
	params->tcp.pda = 0;
	params->tcp.header_digests = header_digests;
	params->tcp.data_digests = data_digests;
	params->tcp.maxr2t = 1;
}

static void
tcp_qpair_params(struct nvmf_qpair_params *params, bool admin,
    const char *address, const char *port)
{
	struct addrinfo hints, *ai, *list;
	int error, s;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo(address, port, &hints, &list);
	if (error != 0)
		errx(1, "%s", gai_strerror(error));

	for (ai = list; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s == -1)
			continue;

		if (connect(s, ai->ai_addr, ai->ai_addrlen) != 0) {
			close(s);
			continue;
		}

		params->admin = admin;
		params->tcp.fd = s;
		freeaddrinfo(list);
		return;
	}
	err(1, "Failed to connect to controller");
}

static struct nvmf_qpair *
connect_admin_queue(struct nvmf_association *na,
    const struct nvmf_qpair_params *params, const uint8_t hostid[16],
    uint16_t cntlid, const char *hostnqn, const char *subnqn)
{
	struct nvme_controller_data cdata;
	struct nvmf_qpair *qp;
	uint64_t cap, cc, csts;
	u_int mps, mpsmin, mpsmax;
	int error, timo;

	qp = nvmf_connect(na, params, 0, NVMF_MIN_ADMIN_MAX_SQ_SIZE, hostid,
	    cntlid, subnqn, hostnqn, 0);
	if (qp == NULL)
		return (NULL);

	error = nvmf_read_property(qp, NVMF_PROP_CAP, 8, &cap);
	if (error != 0)
		errc(1, error, "Failed to fetch CAP");

	/* Require the NVM command set. */
	if (NVME_CAP_HI_CSS_NVM(cap >> 32) == 0)
		errx(1, "Controller does not support the NVM command set");

	/* Prefer native host page size if it fits. */
	mpsmin = NVMEV(NVME_CAP_HI_REG_MPSMIN, cap >> 32);
	mpsmax = NVMEV(NVME_CAP_HI_REG_MPSMAX, cap >> 32);
	mps = ffs(getpagesize()) - 1;
	if (mps < mpsmin + 12)
		mps = mpsmin;
	else if (mps > mpsmax + 12)
		mps = mpsmax;
	else
		mps -= 12;

	/* Configure controller. */
	error = nvmf_read_property(qp, NVMF_PROP_CC, 4, &cc);
	if (error != 0)
		errc(1, error, "Failed to fetch CC");

	/* Clear known fields preserving any reserved fields. */
	cc &= ~(NVMEM(NVME_CC_REG_IOCQES) | NVMEM(NVME_CC_REG_IOSQES) |
	    NVMEM(NVME_CC_REG_SHN) | NVMEM(NVME_CC_REG_AMS) |
	    NVMEM(NVME_CC_REG_MPS) | NVMEM(NVME_CC_REG_CSS));

	cc |= NVMEF(NVME_CC_REG_IOCQES, 4);	/* CQE entry size == 16 */
	cc |= NVMEF(NVME_CC_REG_IOSQES, 6);	/* SQE entry size == 64 */
	cc |= NVMEF(NVME_CC_REG_AMS, 0);	/* AMS 0 (Round-robin) */
	cc |= NVMEF(NVME_CC_REG_MPS, mps);
	cc |= NVMEF(NVME_CC_REG_CSS, 0);	/* NVM command set */
	cc |= NVMEF(NVME_CC_REG_EN, 1);		/* EN = 1 */

	error = nvmf_write_property(qp, NVMF_PROP_CC, 4, cc);
	if (error != 0)
		errc(1, error, "Failed to set CC");

	/* Wait for CSTS.RDY in Controller Status */
	timo = NVME_CAP_LO_TO(cap);
	for (;;) {
		error = nvmf_read_property(qp, NVMF_PROP_CSTS, 4, &csts);
		if (error != 0)
			errc(1, error, "Failed to fetch CSTS");

		if (NVMEV(NVME_CSTS_REG_RDY, csts) != 0)
			break;

		if (timo == 0)
			errx(1, "Controller failed to become ready");
		timo--;
		usleep(500 * 1000);
	}

	/* Fetch controller data. */
	error = nvmf_host_identify_controller(qp, &cdata);
	if (error != 0)
		errc(1, error, "Failed to fetch controller data");

	nvmf_update_assocation(na, &cdata);

	info.mqes = NVME_CAP_LO_MQES(cap);
	info.nn = cdata.nn;
	info.ioccsz = cdata.ioccsz;
	info.disconnect_supported = (cdata.ofcs & 1) != 0;

	return (qp);
}

static void
shutdown_controller(struct nvmf_qpair *qp)
{
	uint64_t cc;
	int error;

	error = nvmf_read_property(qp, NVMF_PROP_CC, 4, &cc);
	if (error != 0)
		errc(1, error, "Failed to fetch CC");

	cc |= NVMEF(NVME_CC_REG_SHN, NVME_SHN_NORMAL);

	error = nvmf_write_property(qp, NVMF_PROP_CC, 4, cc);
	if (error != 0)
		errc(1, error, "Failed to set CC to trigger shutdown");

	nvmf_free_qpair(qp);
}

static void
disconnect_queue(struct nvmf_qpair *qp)
{
	nvmf_free_qpair(qp);
}

static int
validate_namespace(struct nvmf_qpair *qp, u_int nsid, u_int *block_size)
{
	struct nvme_namespace_data nsdata;
	int error;
	uint8_t lbads, lbaf;

	if (nsid > info.nn) {
		warnx("Invalid namespace ID %u", nsid);
		return (ERANGE);
	}

	error = nvmf_host_identify_namespace(qp, nsid, &nsdata);
	if (error != 0) {
		warnc(error, "Failed to identify namespace");
		return (error);
	}

	nvme_namespace_data_swapbytes(&nsdata);

	if (NVMEV(NVME_NS_DATA_DPS_PIT, nsdata.dps) != 0) {
		warnx("End-to-end data protection is not supported");
		return (EINVAL);
	}

	lbaf = NVMEV(NVME_NS_DATA_FLBAS_FORMAT, nsdata.flbas);
	if (lbaf > nsdata.nlbaf) {
		warnx("Invalid LBA format index");
		return (EINVAL);
	}

	if (NVMEV(NVME_NS_DATA_LBAF_MS, nsdata.lbaf[lbaf]) != 0) {
		warnx("Namespaces with metadata are not supported");
		return (EINVAL);
	}

	lbads = NVMEV(NVME_NS_DATA_LBAF_LBADS, nsdata.lbaf[lbaf]);
	if (lbads == 0) {
		warnx("Invalid LBA format index");
		return (EINVAL);
	}

	*block_size = 1 << lbads;
	fprintf(stderr, "Detected block size %u\n", *block_size);
	return (0);
}

static int
nvmf_io_command(struct nvmf_qpair *qp, u_int nsid, enum rw command,
    uint64_t slba, uint16_t nlb, void *buffer, size_t length)
{
	struct nvme_command cmd;
	const struct nvme_completion *cqe;
	struct nvmf_capsule *cc, *rc;
	int error;
	uint16_t status;

	memset(&cmd, 0, sizeof(cmd));
	cmd.opc = command == WRITE ? NVME_OPC_WRITE : NVME_OPC_READ;
	cmd.nsid = htole32(nsid);
	cmd.cdw10 = htole32(slba);
	cmd.cdw11 = htole32(slba >> 32);
	cmd.cdw12 = htole32(nlb - 1);
	/* Sequential Request in cdw13? */

	cc = nvmf_allocate_command(qp, &cmd);
	if (cc == NULL)
		return (errno);

	error = nvmf_capsule_append_data(cc, buffer, length,
	    command == WRITE);
	if (error != 0) {
		nvmf_free_capsule(cc);
		return (error);
	}

	error = nvmf_host_transmit_command(cc);
	if (error != 0) {
		nvmf_free_capsule(cc);
		return (error);
	}

	error = nvmf_host_wait_for_response(cc, &rc);
	nvmf_free_capsule(cc);
	if (error != 0)
		return (error);

	cqe = nvmf_capsule_cqe(rc);
	status = le16toh(cqe->status);
	if (status != 0) {
		printf("NVMF: %s failed, status %#x\n", command == WRITE ?
		    "WRITE" : "READ", status);
		nvmf_free_capsule(rc);
		return (EIO);
	}

	nvmf_free_capsule(rc);
	return (0);
}

static int
nvmf_io(struct nvmf_qpair *qp, u_int nsid, u_int block_size, enum rw command,
    off_t offset, off_t length)
{
	char *buf;
	ssize_t rv;
	u_int todo;
	int error;

	if (offset % block_size != 0) {
		warnx("Misaligned offset");
		return (EINVAL);
	}
	if (length % block_size != 0 && command == WRITE)
		warnx("Length is not multiple of block size, will zero pad");

	buf = malloc(block_size);
	error = 0;
	while (length != 0) {
		todo = length;
		if (todo > block_size)
			todo = block_size;

		if (command == WRITE) {
			rv = read(STDIN_FILENO, buf, todo);
			if (rv == -1) {
				error = errno;
				break;
			}
			if (rv != todo) {
				warn("Short read on input");
				error = EIO;
				break;
			}

			if (todo < block_size)
				memset(buf + todo, 0, block_size - todo);
		}

		error = nvmf_io_command(qp, nsid, command, offset / block_size,
		    1, buf, block_size);
		if (error != 0) {
			warnc(error, "Failed I/O request");
			break;
		}

		if (command == READ)
			(void)write(STDOUT_FILENO, buf, todo);

		offset += block_size;
		length -= todo;
	}

	free(buf);
	return (error);
}

int
main(int ac, char **av)
{
	const char *transport;
	char *address, *port;
	enum rw command;
	struct nvmf_association_params aparams;
	struct nvmf_qpair_params qparams;
	struct nvmf_association *na;
	struct nvmf_qpair *admin, *io;
	char hostnqn[NVMF_NQN_MAX_LEN];
	uint8_t hostid[16];
	enum nvmf_trtype trtype;
	off_t offset, length;
	int ch, error;
	u_int block_size, cntlid, nsid, queues;

	cntlid = NVMF_CNTLID_DYNAMIC;
	offset = 0;
	length = 512;
	nsid = 1;
	port = NULL;
	transport = "tcp";
	while ((ch = getopt(ac, av, "FGc:gl:n:o:p:t:")) != -1) {
		switch (ch) {
		case 'F':
			flow_control_disable = true;
			break;
		case 'G':
			data_digests = true;
			break;
		case 'c':
			if (strcasecmp(optarg, "dynamic") == 0)
				cntlid = NVMF_CNTLID_DYNAMIC;
			else if (strcasecmp(optarg, "static") == 0)
				cntlid = NVMF_CNTLID_STATIC_ANY;
			else
				cntlid = strtoul(optarg, NULL, 0);
			break;
		case 'g':
			header_digests = true;
			break;
		case 'l':
			length = strtoumax(optarg, NULL, 0);
			break;
		case 'n':
			nsid = strtoul(optarg, NULL, 0);
			break;
		case 'o':
			offset = strtoumax(optarg, NULL, 0);
			break;
		case 't':
			transport = optarg;
			break;
		default:
			usage();
		}
	}

	av += optind;
	ac -= optind;

	if (ac != 3)
		usage();

	if (nsid == 0 || nsid >= 0xffffffff)
		errx(1, "Invalid namespace ID %u", nsid);

	if (strcasecmp(av[0], "read") == 0)
		command = READ;
	else if (strcasecmp(av[0], "write") == 0)
		command = WRITE;
	else
		errx(1, "Invalid command %s", av[0]);

	address = av[1];
	port = strrchr(address, ':');
	if (port == NULL || port[1] == '\0')
		errx(1, "Invalid address %s", address);
	*port = '\0';
	port++;

	memset(&aparams, 0, sizeof(aparams));
	aparams.sq_flow_control = !flow_control_disable;
	if (strcasecmp(transport, "tcp") == 0) {
		trtype = NVMF_TRTYPE_TCP;
		tcp_association_params(&aparams);
	} else
		errx(1, "Invalid transport %s", transport);

	error = nvmf_hostid_from_hostuuid(hostid);
	if (error != 0)
		errc(1, error, "Failed to generate hostid");
	error = nvmf_nqn_from_hostuuid(hostnqn);
	if (error != 0)
		errc(1, error, "Failed to generate host NQN");

	na = nvmf_allocate_association(trtype, false, &aparams);
	if (na == NULL)
		err(1, "Failed to create association");

	memset(&qparams, 0, sizeof(qparams));
	tcp_qpair_params(&qparams, true, address, port);

	admin = connect_admin_queue(na, &qparams, hostid, cntlid, hostnqn,
	    av[2]);
	if (admin == NULL)
		errx(1, "Failed to create admin queue: %s",
		    nvmf_association_error(na));

	error = validate_namespace(admin, nsid, &block_size);
	if (error != 0) {
		shutdown_controller(admin);
		nvmf_free_association(na);
		return (1);
	}

	error = nvmf_host_request_queues(admin, 1, &queues);
	if (error != 0) {
		shutdown_controller(admin);
		nvmf_free_association(na);
		errc(1, error, "Failed to request I/O queues");
	}

	memset(&qparams, 0, sizeof(qparams));
	tcp_qpair_params(&qparams, false, address, port);

	io = nvmf_connect(na, &qparams, 1, info.mqes + 1, hostid,
	    nvmf_cntlid(admin), av[2], hostnqn, 0);
	if (io == NULL) {
		warn("Failed to create I/O queue: %s",
		    nvmf_association_error(na));
		shutdown_controller(admin);
		nvmf_free_association(na);
		return (1);
	}
	nvmf_free_association(na);

	error = nvmf_io(io, nsid, block_size, command, offset, length);

	disconnect_queue(io);
	shutdown_controller(admin);
	return (error == 0 ? 0 : 1);
}
