/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <libnvmf.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "fabrics.h"

/*
 * Subroutines shared by several Fabrics commands.
 */
static char nqn[NVMF_NQN_MAX_LEN];
static uint8_t hostid[16];
static bool hostid_initted = false;

static bool
init_hostid(void)
{
	int error;

	if (hostid_initted)
		return (true);

	error = nvmf_hostid_from_hostuuid(hostid);
	if (error != 0) {
		warnc(error, "Failed to generate hostid");
		return (false);
	}
	error = nvmf_nqn_from_hostuuid(nqn);
	if (error != 0) {
		warnc(error, "Failed to generate host NQN");
		return (false);
	}

	hostid_initted = true;
	return (true);
}

const char *
nvmf_default_hostnqn(void)
{
	if (!init_hostid())
		exit(EX_IOERR);
	return (nqn);
}

void
nvmf_parse_address(const char *in_address, const char **address,
    const char **port, char **tofree)
{
	char *cp;

	/*
	 * Accepts the following address formats:
	 *
	 * [IPv6 address]:port
	 * IPv4 address:port
	 * hostname:port
	 * [IPv6 address]
	 * IPv6 address
	 * IPv4 address
	 * hostname
	 */
	if (in_address[0] == '[') {
		/* IPv6 address in square brackets. */
		cp = strchr(in_address + 1, ']');
		if (cp == NULL || cp == in_address + 1)
			errx(EX_USAGE, "Invalid address %s", in_address);
		*tofree = strndup(in_address + 1, cp - (in_address + 1));
		*address = *tofree;

		/* Skip over ']' */
		cp++;
		switch (*cp) {
		case '\0':
			*port = NULL;
			return;
		case ':':
			if (cp[1] != '\0') {
				*port = cp + 1;
				return;
			}
			/* FALLTHROUGH */
		default:
			errx(EX_USAGE, "Invalid address %s", in_address);
		}
	}

	/* Look for the first colon. */
	cp = strchr(in_address, ':');
	if (cp == NULL) {
		*address = in_address;
		*port = NULL;
		*tofree = NULL;
		return;
	}

	/* If there is another colon, assume this is an IPv6 address. */
	if (strchr(cp + 1, ':') != NULL) {
		*address = in_address;
		*port = NULL;
		*tofree = NULL;
		return;
	}

	/* Both strings on either side of the colon must be non-empty. */
	if (cp == in_address || cp[1] == '\0')
		errx(EX_USAGE, "Invalid address %s", in_address);

	*tofree = strndup(in_address, cp - in_address);
	*address = *tofree;

	/* Skip over ':' */
	*port = cp + 1;
}

uint16_t
nvmf_parse_cntlid(const char *cntlid)
{
	u_long value;

	if (strcasecmp(cntlid, "dynamic") == 0)
		return (NVMF_CNTLID_DYNAMIC);
	else if (strcasecmp(cntlid, "static") == 0)
		return (NVMF_CNTLID_STATIC_ANY);
	else {
		value = strtoul(cntlid, NULL, 0);

		if (value > NVMF_CNTLID_STATIC_MAX)
			errx(EX_USAGE, "Invalid controller ID");

		return (value);
	}
}

static bool
tcp_qpair_params(struct nvmf_qpair_params *params, int adrfam,
    const char *address, const char *port, struct addrinfo **aip,
    struct addrinfo **listp)
{
	struct addrinfo hints, *ai, *list;
	int error, s;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = adrfam;
	hints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo(address, port, &hints, &list);
	if (error != 0) {
		warnx("%s", gai_strerror(error));
		return (false);
	}

	for (ai = list; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s == -1)
			continue;

		if (connect(s, ai->ai_addr, ai->ai_addrlen) != 0) {
			close(s);
			continue;
		}

		params->tcp.fd = s;
		if (listp != NULL) {
			*aip = ai;
			*listp = list;
		} else
			freeaddrinfo(list);
		return (true);
	}
	warn("Failed to connect to controller at %s:%s", address, port);
	freeaddrinfo(list);
	return (false);
}

static bool
tcp_qpair_params_ai(struct nvmf_qpair_params *params, struct addrinfo *ai)
{
	int s;

	s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (s == -1)
		return (false);

	if (connect(s, ai->ai_addr, ai->ai_addrlen) != 0) {
		close(s);
		return (false);
	}

	params->tcp.fd = s;
	return (true);
}

static void
tcp_discovery_association_params(struct nvmf_association_params *params)
{
	params->tcp.pda = 0;
	params->tcp.header_digests = false;
	params->tcp.data_digests = false;
	params->tcp.maxr2t = 1;
}

struct nvmf_qpair *
connect_discovery_adminq(enum nvmf_trtype trtype, const char *address,
    const char *port, const char *hostnqn)
{
	struct nvmf_association_params aparams;
	struct nvmf_qpair_params qparams;
	struct nvmf_association *na;
	struct nvmf_qpair *qp;
	uint64_t cap, cc, csts;
	int error, timo;

	memset(&aparams, 0, sizeof(aparams));
	aparams.sq_flow_control = false;
	switch (trtype) {
	case NVMF_TRTYPE_TCP:
		/* 7.4.9.3 Default port for discovery */
		if (port == NULL)
			port = "8009";
		tcp_discovery_association_params(&aparams);
		break;
	default:
		errx(EX_UNAVAILABLE, "Unsupported transport %s",
		    nvmf_transport_type(trtype));
	}

	if (!init_hostid())
		exit(EX_IOERR);
	if (hostnqn != NULL) {
		if (!nvmf_nqn_valid(hostnqn))
			errx(EX_USAGE, "Invalid HostNQN %s", hostnqn);
	} else
		hostnqn = nqn;

	na = nvmf_allocate_association(trtype, false, &aparams);
	if (na == NULL)
		err(EX_IOERR, "Failed to create discovery association");
	memset(&qparams, 0, sizeof(qparams));
	qparams.admin = true;
	if (!tcp_qpair_params(&qparams, AF_UNSPEC, address, port, NULL, NULL))
		exit(EX_NOHOST);
	qp = nvmf_connect(na, &qparams, 0, NVME_MIN_ADMIN_ENTRIES, hostid,
	    NVMF_CNTLID_DYNAMIC, NVMF_DISCOVERY_NQN, hostnqn, 0);
	if (qp == NULL)
		errx(EX_IOERR, "Failed to connect to discovery controller: %s",
		    nvmf_association_error(na));
	nvmf_free_association(na);

	/* Fetch Controller Capabilities Property */
	error = nvmf_read_property(qp, NVMF_PROP_CAP, 8, &cap);
	if (error != 0)
		errc(EX_IOERR, error, "Failed to fetch CAP");

	/* Set Controller Configuration Property (CC.EN=1) */
	error = nvmf_read_property(qp, NVMF_PROP_CC, 4, &cc);
	if (error != 0)
		errc(EX_IOERR, error, "Failed to fetch CC");

	/* Clear known fields preserving any reserved fields. */
	cc &= ~(NVMEM(NVME_CC_REG_SHN) | NVMEM(NVME_CC_REG_AMS) |
	    NVMEM(NVME_CC_REG_MPS) | NVMEM(NVME_CC_REG_CSS));

	/* Leave AMS, MPS, and CSS as 0. */

	cc |= NVMEF(NVME_CC_REG_EN, 1);

	error = nvmf_write_property(qp, NVMF_PROP_CC, 4, cc);
	if (error != 0)
		errc(EX_IOERR, error, "Failed to set CC");

	/* Wait for CSTS.RDY in Controller Status */
	timo = NVME_CAP_LO_TO(cap);
	for (;;) {
		error = nvmf_read_property(qp, NVMF_PROP_CSTS, 4, &csts);
		if (error != 0)
			errc(EX_IOERR, error, "Failed to fetch CSTS");

		if (NVMEV(NVME_CSTS_REG_RDY, csts) != 0)
			break;

		if (timo == 0)
			errx(EX_IOERR, "Controller failed to become ready");
		timo--;
		usleep(500 * 1000);
	}

	return (qp);
}

/*
 * XXX: Should this accept the admin queue size as a parameter rather
 * than always using NVMF_MIN_ADMIN_MAX_SQ_SIZE?
 */
static int
connect_nvm_adminq(struct nvmf_association *na,
    const struct nvmf_qpair_params *params, struct nvmf_qpair **qpp,
    uint16_t cntlid, const char *subnqn, const char *hostnqn, uint32_t kato,
    uint16_t *mqes)
{
	struct nvmf_qpair *qp;
	uint64_t cap, cc, csts;
	u_int mps, mpsmin, mpsmax;
	int error, timo;

	qp = nvmf_connect(na, params, 0, NVMF_MIN_ADMIN_MAX_SQ_SIZE, hostid,
	    cntlid, subnqn, hostnqn, kato);
	if (qp == NULL) {
		warnx("Failed to connect to NVM controller %s: %s", subnqn,
		    nvmf_association_error(na));
		return (EX_IOERR);
	}

	/* Fetch Controller Capabilities Property */
	error = nvmf_read_property(qp, NVMF_PROP_CAP, 8, &cap);
	if (error != 0) {
		warnc(error, "Failed to fetch CAP");
		nvmf_free_qpair(qp);
		return (EX_IOERR);
	}

	/* Require the NVM command set. */
	if (NVME_CAP_HI_CSS_NVM(cap >> 32) == 0) {
		warnx("Controller %s does not support the NVM command set",
		    subnqn);
		nvmf_free_qpair(qp);
		return (EX_UNAVAILABLE);
	}

	*mqes = NVME_CAP_LO_MQES(cap);

	/* Prefer native host page size if it fits. */
	mpsmin = NVMEV(NVME_CAP_HI_REG_MPSMIN, cap >> 32);
	mpsmax = NVMEV(NVME_CAP_HI_REG_MPSMAX, cap >> 32);
	mps = ffs(getpagesize()) - 1;
	if (mps < mpsmin + NVME_MPS_SHIFT)
		mps = mpsmin;
	else if (mps > mpsmax + NVME_MPS_SHIFT)
		mps = mpsmax;
	else
		mps -= NVME_MPS_SHIFT;

	/* Configure controller. */
	error = nvmf_read_property(qp, NVMF_PROP_CC, 4, &cc);
	if (error != 0) {
		warnc(error, "Failed to fetch CC");
		nvmf_free_qpair(qp);
		return (EX_IOERR);
	}

	/* Clear known fields preserving any reserved fields. */
	cc &= ~(NVMEM(NVME_CC_REG_IOCQES) | NVMEM(NVME_CC_REG_IOSQES) |
	    NVMEM(NVME_CC_REG_SHN) | NVMEM(NVME_CC_REG_AMS) |
	    NVMEM(NVME_CC_REG_MPS) | NVMEM(NVME_CC_REG_CSS));

	cc |= NVMEF(NVME_CC_REG_IOCQES, 4);	/* CQE entry size == 16 */
	cc |= NVMEF(NVME_CC_REG_IOSQES, 6);	/* SEQ entry size == 64 */
	cc |= NVMEF(NVME_CC_REG_AMS, 0);	/* AMS 0 (Round-robin) */
	cc |= NVMEF(NVME_CC_REG_MPS, mps);
	cc |= NVMEF(NVME_CC_REG_CSS, 0);	/* NVM command set */
	cc |= NVMEF(NVME_CC_REG_EN, 1);		/* EN = 1 */

	error = nvmf_write_property(qp, NVMF_PROP_CC, 4, cc);
	if (error != 0) {
		warnc(error, "Failed to set CC");
		nvmf_free_qpair(qp);
		return (EX_IOERR);
	}

	/* Wait for CSTS.RDY in Controller Status */
	timo = NVME_CAP_LO_TO(cap);
	for (;;) {
		error = nvmf_read_property(qp, NVMF_PROP_CSTS, 4, &csts);
		if (error != 0) {
			warnc(error, "Failed to fetch CSTS");
			nvmf_free_qpair(qp);
			return (EX_IOERR);
		}

		if (NVMEV(NVME_CSTS_REG_RDY, csts) != 0)
			break;

		if (timo == 0) {
			warnx("Controller failed to become ready");
			nvmf_free_qpair(qp);
			return (EX_IOERR);
		}
		timo--;
		usleep(500 * 1000);
	}

	*qpp = qp;
	return (0);
}

static void
shutdown_controller(struct nvmf_qpair *qp)
{
	uint64_t cc;
	int error;

	error = nvmf_read_property(qp, NVMF_PROP_CC, 4, &cc);
	if (error != 0) {
		warnc(error, "Failed to fetch CC");
		goto out;
	}

	cc |= NVMEF(NVME_CC_REG_SHN, NVME_SHN_NORMAL);

	error = nvmf_write_property(qp, NVMF_PROP_CC, 4, cc);
	if (error != 0) {
		warnc(error, "Failed to set CC to trigger shutdown");
		goto out;
	}

out:
	nvmf_free_qpair(qp);
}

/* Returns a value from <sysexits.h> */
int
connect_nvm_queues(const struct nvmf_association_params *aparams,
    enum nvmf_trtype trtype, int adrfam, const char *address,
    const char *port, uint16_t cntlid, const char *subnqn, const char *hostnqn,
    uint32_t kato, struct nvmf_qpair **admin, struct nvmf_qpair **io,
    u_int num_io_queues, u_int queue_size, struct nvme_controller_data *cdata)
{
	struct nvmf_qpair_params qparams;
	struct nvmf_association *na;
	struct addrinfo *ai, *list;
	u_int queues;
	int error;
	uint16_t mqes;

	switch (trtype) {
	case NVMF_TRTYPE_TCP:
		break;
	default:
		warnx("Unsupported transport %s", nvmf_transport_type(trtype));
		return (EX_UNAVAILABLE);
	}

	if (!init_hostid())
		return (EX_IOERR);
	if (hostnqn == NULL || !nvmf_nqn_valid(hostnqn)) {
		warnx("Invalid HostNQN %s", hostnqn);
		return (EX_USAGE);
	}

	/* Association. */
	na = nvmf_allocate_association(trtype, false, aparams);
	if (na == NULL) {
		warn("Failed to create association for %s", subnqn);
		return (EX_IOERR);
	}

	/* Admin queue. */
	memset(&qparams, 0, sizeof(qparams));
	qparams.admin = true;
	if (!tcp_qpair_params(&qparams, adrfam, address, port, &ai, &list)) {
		nvmf_free_association(na);
		return (EX_NOHOST);
	}
	error = connect_nvm_adminq(na, &qparams, admin, cntlid, subnqn, hostnqn,
	    kato, &mqes);
	if (error != 0) {
		nvmf_free_association(na);
		freeaddrinfo(list);
		return (error);
	}

	/* Validate I/O queue size. */
	memset(io, 0, sizeof(*io) * num_io_queues);
	if (queue_size == 0)
		queue_size = (u_int)mqes + 1;
	else if (queue_size > (u_int)mqes + 1) {
		warnx("I/O queue size exceeds controller maximum (%u)",
		    mqes + 1);
		error = EX_USAGE;
		goto out;
	}

	/* Fetch controller data. */
	error = nvmf_host_identify_controller(*admin, cdata);
	if (error != 0) {
		warnc(error, "Failed to fetch controller data for %s", subnqn);
		error = EX_IOERR;
		goto out;
	}

	nvmf_update_assocation(na, cdata);

	error = nvmf_host_request_queues(*admin, num_io_queues, &queues);
	if (error != 0) {
		warnc(error, "Failed to request I/O queues");
		error = EX_IOERR;
		goto out;
	}
	if (queues < num_io_queues) {
		warnx("Controller enabled fewer I/O queues (%u) than requested (%u)",
		    queues, num_io_queues);
		error = EX_PROTOCOL;
		goto out;
	}

	/* I/O queues. */
	for (u_int i = 0; i < num_io_queues; i++) {
		memset(&qparams, 0, sizeof(qparams));
		qparams.admin = false;
		if (!tcp_qpair_params_ai(&qparams, ai)) {
			warn("Failed to connect to controller at %s:%s",
			    address, port);
			error = EX_NOHOST;
			goto out;
		}
		io[i] = nvmf_connect(na, &qparams, i + 1, queue_size, hostid,
		    nvmf_cntlid(*admin), subnqn, hostnqn, 0);
		if (io[i] == NULL) {
			warnx("Failed to create I/O queue: %s",
			    nvmf_association_error(na));
			error = EX_IOERR;
			goto out;
		}
	}
	nvmf_free_association(na);
	freeaddrinfo(list);
	return (0);

out:
	disconnect_nvm_queues(*admin, io, num_io_queues);
	nvmf_free_association(na);
	freeaddrinfo(list);
	return (error);
}

void
disconnect_nvm_queues(struct nvmf_qpair *admin, struct nvmf_qpair **io,
    u_int num_io_queues)
{
	for (u_int i = 0; i < num_io_queues; i++) {
		if (io[i] == NULL)
			break;
		nvmf_free_qpair(io[i]);
	}
	shutdown_controller(admin);
}
