/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/nv.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid.h>

#include "libnvmf.h"
#include "internal.h"

static void
nvmf_init_sqe(void *sqe, uint8_t opcode)
{
	struct nvme_command *cmd = sqe;

	memset(cmd, 0, sizeof(*cmd));
	cmd->opc = opcode;
}

static void
nvmf_init_fabrics_sqe(void *sqe, uint8_t fctype)
{
	struct nvmf_capsule_cmd *cmd = sqe;

	nvmf_init_sqe(sqe, NVME_OPC_FABRICS_COMMANDS);
	cmd->fctype = fctype;
}

struct nvmf_qpair *
nvmf_connect(struct nvmf_association *na,
    const struct nvmf_qpair_params *params, uint16_t qid, u_int queue_size,
    const uint8_t hostid[16], uint16_t cntlid, const char *subnqn,
    const char *hostnqn, uint32_t kato)
{
	struct nvmf_fabric_connect_cmd cmd;
	struct nvmf_fabric_connect_data data;
	const struct nvmf_fabric_connect_rsp *rsp;
	struct nvmf_qpair *qp;
	struct nvmf_capsule *cc, *rc;
	int error;
	uint16_t sqhd, status;

	qp = NULL;
	cc = NULL;
	rc = NULL;
	na_clear_error(na);
	if (na->na_controller) {
		na_error(na, "Cannot connect on a controller");
		goto error;
	}

	if (params->admin != (qid == 0)) {
		na_error(na, "Admin queue must use Queue ID 0");
		goto error;
	}

	if (qid == 0) {
		if (queue_size < NVME_MIN_ADMIN_ENTRIES ||
		    queue_size > NVME_MAX_ADMIN_ENTRIES) {
			na_error(na, "Invalid queue size %u", queue_size);
			goto error;
		}
	} else {
		if (queue_size < NVME_MIN_IO_ENTRIES ||
		    queue_size > NVME_MAX_IO_ENTRIES) {
			na_error(na, "Invalid queue size %u", queue_size);
			goto error;
		}

		/* KATO is only for Admin queues. */
		if (kato != 0) {
			na_error(na, "Cannot set KATO on I/O queues");
			goto error;
		}
	}

	qp = nvmf_allocate_qpair(na, params);
	if (qp == NULL)
		goto error;

	nvmf_init_fabrics_sqe(&cmd, NVMF_FABRIC_COMMAND_CONNECT);
	cmd.recfmt = 0;
	cmd.qid = htole16(qid);

	/* N.B. sqsize is 0's based. */
	cmd.sqsize = htole16(queue_size - 1);
	if (!na->na_params.sq_flow_control)
		cmd.cattr |= NVMF_CONNECT_ATTR_DISABLE_SQ_FC;
	cmd.kato = htole32(kato);

	cc = nvmf_allocate_command(qp, &cmd);
	if (cc == NULL) {
		na_error(na, "Failed to allocate command capsule: %s",
		    strerror(errno));
		goto error;
	}

	memset(&data, 0, sizeof(data));
	memcpy(data.hostid, hostid, sizeof(data.hostid));
	data.cntlid = htole16(cntlid);
	strlcpy(data.subnqn, subnqn, sizeof(data.subnqn));
	strlcpy(data.hostnqn, hostnqn, sizeof(data.hostnqn));

	error = nvmf_capsule_append_data(cc, &data, sizeof(data), true);
	if (error != 0) {
		na_error(na, "Failed to append data to CONNECT capsule: %s",
		    strerror(error));
		goto error;
	}

	error = nvmf_transmit_capsule(cc);
	if (error != 0) {
		na_error(na, "Failed to transmit CONNECT capsule: %s",
		    strerror(errno));
		goto error;
	}

	error = nvmf_receive_capsule(qp, &rc);
	if (error != 0) {
		na_error(na, "Failed to receive CONNECT response: %s",
		    strerror(error));
		goto error;
	}

	rsp = (const struct nvmf_fabric_connect_rsp *)&rc->nc_cqe;
	status = le16toh(rc->nc_cqe.status);
	if (status != 0) {
		if (NVME_STATUS_GET_SC(status) == NVMF_FABRIC_SC_INVALID_PARAM)
			na_error(na,
			    "CONNECT invalid parameter IATTR: %#x IPO: %#x",
			    rsp->status_code_specific.invalid.iattr,
			    rsp->status_code_specific.invalid.ipo);
		else
			na_error(na, "CONNECT failed, status %#x", status);
		goto error;
	}

	if (rc->nc_cqe.cid != cmd.cid) {
		na_error(na, "Mismatched CID in CONNECT response");
		goto error;
	}

	if (!rc->nc_sqhd_valid) {
		na_error(na, "CONNECT response without valid SQHD");
		goto error;
	}

	sqhd = le16toh(rsp->sqhd);
	if (sqhd == 0xffff) {
		if (na->na_params.sq_flow_control) {
			na_error(na, "Controller disabled SQ flow control");
			goto error;
		}
		qp->nq_flow_control = false;
	} else {
		qp->nq_flow_control = true;
		qp->nq_sqhd = sqhd;
		qp->nq_sqtail = sqhd;
	}

	if (rsp->status_code_specific.success.authreq) {
		na_error(na, "CONNECT response requests authentication\n");
		goto error;
	}

	qp->nq_qsize = queue_size;
	qp->nq_cntlid = le16toh(rsp->status_code_specific.success.cntlid);
	qp->nq_kato = kato;
	/* XXX: Save qid in qp? */
	return (qp);

error:
	if (rc != NULL)
		nvmf_free_capsule(rc);
	if (cc != NULL)
		nvmf_free_capsule(cc);
	if (qp != NULL)
		nvmf_free_qpair(qp);
	return (NULL);
}

uint16_t
nvmf_cntlid(struct nvmf_qpair *qp)
{
	return (qp->nq_cntlid);
}

int
nvmf_host_transmit_command(struct nvmf_capsule *nc)
{
	struct nvmf_qpair *qp = nc->nc_qpair;
	uint16_t new_sqtail;
	int error;

	/* Fail if the queue is full. */
	new_sqtail = (qp->nq_sqtail + 1) % qp->nq_qsize;
	if (new_sqtail == qp->nq_sqhd)
		return (EBUSY);

	nc->nc_sqe.cid = htole16(qp->nq_cid);

	/* 4.2 Skip CID of 0xFFFF. */
	qp->nq_cid++;
	if (qp->nq_cid == 0xFFFF)
		qp->nq_cid = 0;

	error = nvmf_transmit_capsule(nc);
	if (error != 0)
		return (error);

	qp->nq_sqtail = new_sqtail;
	return (0);
}

/* Receive a single capsule and update SQ FC accounting. */
static int
nvmf_host_receive_capsule(struct nvmf_qpair *qp, struct nvmf_capsule **ncp)
{
	struct nvmf_capsule *nc;
	int error;

	/* If the SQ is empty, there is no response to wait for. */
	if (qp->nq_sqhd == qp->nq_sqtail)
		return (EWOULDBLOCK);

	error = nvmf_receive_capsule(qp, &nc);
	if (error != 0)
		return (error);

	if (qp->nq_flow_control) {
		if (nc->nc_sqhd_valid)
			qp->nq_sqhd = le16toh(nc->nc_cqe.sqhd);
	} else {
		/*
		 * If SQ FC is disabled, just advance the head for
		 * each response capsule received so that we track the
		 * number of outstanding commands.
		 */
		qp->nq_sqhd = (qp->nq_sqhd + 1) % qp->nq_qsize;
	}
	*ncp = nc;
	return (0);
}

int
nvmf_host_receive_response(struct nvmf_qpair *qp, struct nvmf_capsule **ncp)
{
	struct nvmf_capsule *nc;

	/* Return the oldest previously received response. */
	if (!TAILQ_EMPTY(&qp->nq_rx_capsules)) {
		nc = TAILQ_FIRST(&qp->nq_rx_capsules);
		TAILQ_REMOVE(&qp->nq_rx_capsules, nc, nc_link);
		*ncp = nc;
		return (0);
	}

	return (nvmf_host_receive_capsule(qp, ncp));
}

int
nvmf_host_wait_for_response(struct nvmf_capsule *cc,
    struct nvmf_capsule **rcp)
{
	struct nvmf_qpair *qp = cc->nc_qpair;
	struct nvmf_capsule *rc;
	int error;

	/* Check if a response was already received. */
	TAILQ_FOREACH(rc, &qp->nq_rx_capsules, nc_link) {
		if (rc->nc_cqe.cid == cc->nc_sqe.cid) {
			TAILQ_REMOVE(&qp->nq_rx_capsules, rc, nc_link);
			*rcp = rc;
			return (0);
		}
	}

	/* Wait for a response. */
	for (;;) {
		error = nvmf_host_receive_capsule(qp, &rc);
		if (error != 0)
			return (error);

		if (rc->nc_cqe.cid != cc->nc_sqe.cid) {
			TAILQ_INSERT_TAIL(&qp->nq_rx_capsules, rc, nc_link);
			continue;
		}

		*rcp = rc;
		return (0);
	}
}

struct nvmf_capsule *
nvmf_keepalive(struct nvmf_qpair *qp)
{
	struct nvme_command cmd;

	if (!qp->nq_admin) {
		errno = EINVAL;
		return (NULL);
	}

	nvmf_init_sqe(&cmd, NVME_OPC_KEEP_ALIVE);

	return (nvmf_allocate_command(qp, &cmd));
}

static struct nvmf_capsule *
nvmf_get_property(struct nvmf_qpair *qp, uint32_t offset, uint8_t size)
{
	struct nvmf_fabric_prop_get_cmd cmd;

	nvmf_init_fabrics_sqe(&cmd, NVMF_FABRIC_COMMAND_PROPERTY_GET);
	switch (size) {
	case 4:
		cmd.attrib.size = NVMF_PROP_SIZE_4;
		break;
	case 8:
		cmd.attrib.size = NVMF_PROP_SIZE_8;
		break;
	default:
		errno = EINVAL;
		return (NULL);
	}
	cmd.ofst = htole32(offset);

	return (nvmf_allocate_command(qp, &cmd));
}

int
nvmf_read_property(struct nvmf_qpair *qp, uint32_t offset, uint8_t size,
    uint64_t *value)
{
	struct nvmf_capsule *cc, *rc;
	const struct nvmf_fabric_prop_get_rsp *rsp;
	uint16_t status;
	int error;

	if (!qp->nq_admin)
		return (EINVAL);

	cc = nvmf_get_property(qp, offset, size);
	if (cc == NULL)
		return (errno);

	error = nvmf_host_transmit_command(cc);
	if (error != 0) {
		nvmf_free_capsule(cc);
		return (error);
	}

	error = nvmf_host_wait_for_response(cc, &rc);
	nvmf_free_capsule(cc);
	if (error != 0)
		return (error);

	rsp = (const struct nvmf_fabric_prop_get_rsp *)&rc->nc_cqe;
	status = le16toh(rc->nc_cqe.status);
	if (status != 0) {
		printf("NVMF: PROPERTY_GET failed, status %#x\n", status);
		nvmf_free_capsule(rc);
		return (EIO);
	}

	if (size == 8)
		*value = le64toh(rsp->value.u64);
	else
		*value = le32toh(rsp->value.u32.low);
	nvmf_free_capsule(rc);
	return (0);
}

static struct nvmf_capsule *
nvmf_set_property(struct nvmf_qpair *qp, uint32_t offset, uint8_t size,
    uint64_t value)
{
	struct nvmf_fabric_prop_set_cmd cmd;

	nvmf_init_fabrics_sqe(&cmd, NVMF_FABRIC_COMMAND_PROPERTY_SET);
	switch (size) {
	case 4:
		cmd.attrib.size = NVMF_PROP_SIZE_4;
		cmd.value.u32.low = htole32(value);
		break;
	case 8:
		cmd.attrib.size = NVMF_PROP_SIZE_8;
		cmd.value.u64 = htole64(value);
		break;
	default:
		errno = EINVAL;
		return (NULL);
	}
	cmd.ofst = htole32(offset);

	return (nvmf_allocate_command(qp, &cmd));
}

int
nvmf_write_property(struct nvmf_qpair *qp, uint32_t offset, uint8_t size,
    uint64_t value)
{
	struct nvmf_capsule *cc, *rc;
	uint16_t status;
	int error;

	if (!qp->nq_admin)
		return (EINVAL);

	cc = nvmf_set_property(qp, offset, size, value);
	if (cc == NULL)
		return (errno);

	error = nvmf_host_transmit_command(cc);
	if (error != 0) {
		nvmf_free_capsule(cc);
		return (error);
	}

	error = nvmf_host_wait_for_response(cc, &rc);
	nvmf_free_capsule(cc);
	if (error != 0)
		return (error);

	status = le16toh(rc->nc_cqe.status);
	if (status != 0) {
		printf("NVMF: PROPERTY_SET failed, status %#x\n", status);
		nvmf_free_capsule(rc);
		return (EIO);
	}

	nvmf_free_capsule(rc);
	return (0);
}

int
nvmf_hostid_from_hostuuid(uint8_t hostid[16])
{
	char hostuuid_str[64];
	uuid_t hostuuid;
	size_t len;
	uint32_t status;

	len = sizeof(hostuuid_str);
	if (sysctlbyname("kern.hostuuid", hostuuid_str, &len, NULL, 0) != 0)
		return (errno);

	uuid_from_string(hostuuid_str, &hostuuid, &status);
	switch (status) {
	case uuid_s_ok:
		break;
	case uuid_s_no_memory:
		return (ENOMEM);
	default:
		return (EINVAL);
	}

	uuid_enc_le(hostid, &hostuuid);
	return (0);
}

int
nvmf_nqn_from_hostuuid(char nqn[NVMF_NQN_MAX_LEN])
{
	char hostuuid_str[64];
	size_t len;

	len = sizeof(hostuuid_str);
	if (sysctlbyname("kern.hostuuid", hostuuid_str, &len, NULL, 0) != 0)
		return (errno);

	strlcpy(nqn, NVMF_NQN_UUID_PRE, NVMF_NQN_MAX_LEN);
	strlcat(nqn, hostuuid_str, NVMF_NQN_MAX_LEN);
	return (0);
}

int
nvmf_host_identify_controller(struct nvmf_qpair *qp,
    struct nvme_controller_data *cdata)
{
	struct nvme_command cmd;
	struct nvmf_capsule *cc, *rc;
	int error;
	uint16_t status;

	if (!qp->nq_admin)
		return (EINVAL);

	nvmf_init_sqe(&cmd, NVME_OPC_IDENTIFY);

	/* 5.15.1 Use CNS of 0x01 for controller data. */
	cmd.cdw10 = htole32(1);

	cc = nvmf_allocate_command(qp, &cmd);
	if (cc == NULL)
		return (errno);

	error = nvmf_capsule_append_data(cc, cdata, sizeof(*cdata), false);
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

	status = le16toh(rc->nc_cqe.status);
	if (status != 0) {
		printf("NVMF: IDENTIFY failed, status %#x\n", status);
		nvmf_free_capsule(rc);
		return (EIO);
	}

	nvmf_free_capsule(rc);
	return (0);
}

int
nvmf_host_identify_namespace(struct nvmf_qpair *qp, uint32_t nsid,
    struct nvme_namespace_data *nsdata)
{
	struct nvme_command cmd;
	struct nvmf_capsule *cc, *rc;
	int error;
	uint16_t status;

	if (!qp->nq_admin)
		return (EINVAL);

	nvmf_init_sqe(&cmd, NVME_OPC_IDENTIFY);

	/* 5.15.1 Use CNS of 0x00 for namespace data. */
	cmd.cdw10 = htole32(0);
	cmd.nsid = htole32(nsid);

	cc = nvmf_allocate_command(qp, &cmd);
	if (cc == NULL)
		return (errno);

	error = nvmf_capsule_append_data(cc, nsdata, sizeof(*nsdata), false);
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

	status = le16toh(rc->nc_cqe.status);
	if (status != 0) {
		printf("NVMF: IDENTIFY failed, status %#x\n", status);
		nvmf_free_capsule(rc);
		return (EIO);
	}

	nvmf_free_capsule(rc);
	return (0);
}

static int
nvmf_get_discovery_log_page(struct nvmf_qpair *qp, uint64_t offset, void *buf,
    size_t len)
{
	struct nvme_command cmd;
	struct nvmf_capsule *cc, *rc;
	size_t numd;
	int error;
	uint16_t status;

	if (len % 4 != 0 || len == 0 || offset % 4 != 0)
		return (EINVAL);

	numd = (len / 4) - 1;
	nvmf_init_sqe(&cmd, NVME_OPC_GET_LOG_PAGE);
	cmd.cdw10 = htole32(numd << 16 | NVME_LOG_DISCOVERY);
	cmd.cdw11 = htole32(numd >> 16);
	cmd.cdw12 = htole32(offset);
	cmd.cdw13 = htole32(offset >> 32);

	cc = nvmf_allocate_command(qp, &cmd);
	if (cc == NULL)
		return (errno);

	error = nvmf_capsule_append_data(cc, buf, len, false);
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

	status = le16toh(rc->nc_cqe.status);
	if (NVMEV(NVME_STATUS_SC, status) ==
	    NVMF_FABRIC_SC_LOG_RESTART_DISCOVERY) {
		nvmf_free_capsule(rc);
		return (EAGAIN);
	}
	if (status != 0) {
		printf("NVMF: GET_LOG_PAGE failed, status %#x\n", status);
		nvmf_free_capsule(rc);
		return (EIO);
	}

	nvmf_free_capsule(rc);
	return (0);
}

int
nvmf_host_fetch_discovery_log_page(struct nvmf_qpair *qp,
    struct nvme_discovery_log **logp)
{
	struct nvme_discovery_log hdr, *log;
	size_t payload_len;
	int error;

	if (!qp->nq_admin)
		return (EINVAL);

	log = NULL;
	for (;;) {
		error = nvmf_get_discovery_log_page(qp, 0, &hdr, sizeof(hdr));
		if (error != 0) {
			free(log);
			return (error);
		}
		nvme_discovery_log_swapbytes(&hdr);

		if (hdr.recfmt != 0) {
			printf("NVMF: Unsupported discovery log format: %d\n",
			    hdr.recfmt);
			free(log);
			return (EINVAL);
		}

		if (hdr.numrec > 1024) {
			printf("NVMF: Too many discovery log entries: %ju\n",
			    (uintmax_t)hdr.numrec);
			free(log);
			return (EFBIG);
		}

		payload_len = sizeof(log->entries[0]) * hdr.numrec;
		log = reallocf(log, sizeof(*log) + payload_len);
		if (log == NULL)
			return (ENOMEM);
		*log = hdr;
		if (hdr.numrec == 0)
			break;

		error = nvmf_get_discovery_log_page(qp, sizeof(hdr),
		    log->entries, payload_len);
		if (error == EAGAIN)
			continue;
		if (error != 0) {
			free(log);
			return (error);
		}

		/* Re-read the header and check the generation count. */
		error = nvmf_get_discovery_log_page(qp, 0, &hdr, sizeof(hdr));
		if (error != 0) {
			free(log);
			return (error);
		}
		nvme_discovery_log_swapbytes(&hdr);

		if (log->genctr != hdr.genctr)
			continue;

		for (u_int i = 0; i < log->numrec; i++)
			nvme_discovery_log_entry_swapbytes(&log->entries[i]);
		break;
	}
	*logp = log;
	return (0);
}

int
nvmf_init_dle_from_admin_qp(struct nvmf_qpair *qp,
    const struct nvme_controller_data *cdata,
    struct nvme_discovery_log_entry *dle)
{
	int error;
	uint16_t cntlid;

	memset(dle, 0, sizeof(*dle));
	error = nvmf_populate_dle(qp, dle);
	if (error != 0)
		return (error);
	if ((cdata->fcatt & 1) == 0)
		cntlid = NVMF_CNTLID_DYNAMIC;
	else
		cntlid = cdata->ctrlr_id;
	dle->cntlid = htole16(cntlid);
	memcpy(dle->subnqn, cdata->subnqn, sizeof(dle->subnqn));
	return (0);
}

int
nvmf_host_request_queues(struct nvmf_qpair *qp, u_int requested, u_int *actual)
{
	struct nvme_command cmd;
	struct nvmf_capsule *cc, *rc;
	int error;
	uint16_t status;

	if (!qp->nq_admin || requested < 1 || requested > 65535)
		return (EINVAL);

	/* The number of queues is 0's based. */
	requested--;

	nvmf_init_sqe(&cmd, NVME_OPC_SET_FEATURES);
	cmd.cdw10 = htole32(NVME_FEAT_NUMBER_OF_QUEUES);

	/* Same number of completion and submission queues. */
	cmd.cdw11 = htole32((requested << 16) | requested);

	cc = nvmf_allocate_command(qp, &cmd);
	if (cc == NULL)
		return (errno);

	error = nvmf_host_transmit_command(cc);
	if (error != 0) {
		nvmf_free_capsule(cc);
		return (error);
	}

	error = nvmf_host_wait_for_response(cc, &rc);
	nvmf_free_capsule(cc);
	if (error != 0)
		return (error);

	status = le16toh(rc->nc_cqe.status);
	if (status != 0) {
		printf("NVMF: SET_FEATURES failed, status %#x\n", status);
		nvmf_free_capsule(rc);
		return (EIO);
	}

	*actual = (le32toh(rc->nc_cqe.cdw0) & 0xffff) + 1;
	nvmf_free_capsule(rc);
	return (0);
}

static bool
is_queue_pair_idle(struct nvmf_qpair *qp)
{
	if (qp->nq_sqhd != qp->nq_sqtail)
		return (false);
	if (!TAILQ_EMPTY(&qp->nq_rx_capsules))
		return (false);
	return (true);
}

static int
prepare_queues_for_handoff(struct nvmf_ioc_nv *nv,
    const struct nvme_discovery_log_entry *dle, const char *hostnqn,
    struct nvmf_qpair *admin_qp, u_int num_queues,
    struct nvmf_qpair **io_queues, const struct nvme_controller_data *cdata,
    uint32_t reconnect_delay, uint32_t controller_loss_timeout)
{
	const struct nvmf_association *na = admin_qp->nq_association;
	nvlist_t *nvl, *nvl_qp, *nvl_rparams;
	u_int i;
	int error;

	if (num_queues == 0)
		return (EINVAL);

	/* Ensure trtype matches. */
	if (dle->trtype != na->na_trtype)
		return (EINVAL);

	/* All queue pairs must be idle. */
	if (!is_queue_pair_idle(admin_qp))
		return (EBUSY);
	for (i = 0; i < num_queues; i++) {
		if (!is_queue_pair_idle(io_queues[i]))
			return (EBUSY);
	}

	/* Fill out reconnect parameters. */
	nvl_rparams = nvlist_create(0);
	nvlist_add_binary(nvl_rparams, "dle", dle, sizeof(*dle));
	nvlist_add_string(nvl_rparams, "hostnqn", hostnqn);
	nvlist_add_number(nvl_rparams, "num_io_queues", num_queues);
	nvlist_add_number(nvl_rparams, "kato", admin_qp->nq_kato);
	nvlist_add_number(nvl_rparams, "reconnect_delay", reconnect_delay);
	nvlist_add_number(nvl_rparams, "controller_loss_timeout",
	    controller_loss_timeout);
	nvlist_add_number(nvl_rparams, "io_qsize", io_queues[0]->nq_qsize);
	nvlist_add_bool(nvl_rparams, "sq_flow_control",
	    na->na_params.sq_flow_control);
	switch (na->na_trtype) {
	case NVMF_TRTYPE_TCP:
		nvlist_add_bool(nvl_rparams, "header_digests",
		    na->na_params.tcp.header_digests);
		nvlist_add_bool(nvl_rparams, "data_digests",
		    na->na_params.tcp.data_digests);
		break;
	default:
		__unreachable();
	}
	error = nvlist_error(nvl_rparams);
	if (error != 0) {
		nvlist_destroy(nvl_rparams);
		return (error);
	}

	nvl = nvlist_create(0);
	nvlist_add_number(nvl, "trtype", na->na_trtype);
	nvlist_add_number(nvl, "kato", admin_qp->nq_kato);
	nvlist_add_number(nvl, "reconnect_delay", reconnect_delay);
	nvlist_add_number(nvl, "controller_loss_timeout",
	    controller_loss_timeout);
	nvlist_move_nvlist(nvl, "rparams", nvl_rparams);

	/* First, the admin queue. */
	error = nvmf_kernel_handoff_params(admin_qp, &nvl_qp);
	if (error) {
		nvlist_destroy(nvl);
		return (error);
	}
	nvlist_move_nvlist(nvl, "admin", nvl_qp);

	/* Next, the I/O queues. */
	for (i = 0; i < num_queues; i++) {
		error = nvmf_kernel_handoff_params(io_queues[i], &nvl_qp);
		if (error) {
			nvlist_destroy(nvl);
			return (error);
		}
		nvlist_append_nvlist_array(nvl, "io", nvl_qp);
	}

	nvlist_add_binary(nvl, "cdata", cdata, sizeof(*cdata));

	error = nvmf_pack_ioc_nvlist(nv, nvl);
	nvlist_destroy(nvl);
	return (error);
}

int
nvmf_handoff_host(const struct nvme_discovery_log_entry *dle,
    const char *hostnqn, struct nvmf_qpair *admin_qp, u_int num_queues,
    struct nvmf_qpair **io_queues, const struct nvme_controller_data *cdata,
    uint32_t reconnect_delay, uint32_t controller_loss_timeout)
{
	struct nvmf_ioc_nv nv;
	u_int i;
	int error, fd;

	fd = open("/dev/nvmf", O_RDWR);
	if (fd == -1) {
		error = errno;
		goto out;
	}

	error = prepare_queues_for_handoff(&nv, dle, hostnqn, admin_qp,
	    num_queues, io_queues, cdata, reconnect_delay,
	    controller_loss_timeout);
	if (error != 0)
		goto out;

	if (ioctl(fd, NVMF_HANDOFF_HOST, &nv) == -1)
		error = errno;
	free(nv.data);

out:
	if (fd >= 0)
		close(fd);
	for (i = 0; i < num_queues; i++)
		(void)nvmf_free_qpair(io_queues[i]);
	(void)nvmf_free_qpair(admin_qp);
	return (error);
}

int
nvmf_disconnect_host(const char *host)
{
	int error, fd;

	error = 0;
	fd = open("/dev/nvmf", O_RDWR);
	if (fd == -1) {
		error = errno;
		goto out;
	}

	if (ioctl(fd, NVMF_DISCONNECT_HOST, &host) == -1)
		error = errno;

out:
	if (fd >= 0)
		close(fd);
	return (error);
}

int
nvmf_disconnect_all(void)
{
	int error, fd;

	error = 0;
	fd = open("/dev/nvmf", O_RDWR);
	if (fd == -1) {
		error = errno;
		goto out;
	}

	if (ioctl(fd, NVMF_DISCONNECT_ALL) == -1)
		error = errno;

out:
	if (fd >= 0)
		close(fd);
	return (error);
}

static int
nvmf_read_ioc_nv(int fd, u_long com, nvlist_t **nvlp)
{
	struct nvmf_ioc_nv nv;
	nvlist_t *nvl;
	int error;

	memset(&nv, 0, sizeof(nv));
	if (ioctl(fd, com, &nv) == -1)
		return (errno);

	nv.data = malloc(nv.len);
	nv.size = nv.len;
	if (ioctl(fd, com, &nv) == -1) {
		error = errno;
		free(nv.data);
		return (error);
	}

	nvl = nvlist_unpack(nv.data, nv.len, 0);
	free(nv.data);
	if (nvl == NULL)
		return (EINVAL);

	*nvlp = nvl;
	return (0);
}

int
nvmf_reconnect_params(int fd, nvlist_t **nvlp)
{
	return (nvmf_read_ioc_nv(fd, NVMF_RECONNECT_PARAMS, nvlp));
}

int
nvmf_reconnect_host(int fd, const struct nvme_discovery_log_entry *dle,
    const char *hostnqn, struct nvmf_qpair *admin_qp, u_int num_queues,
    struct nvmf_qpair **io_queues, const struct nvme_controller_data *cdata,
    uint32_t reconnect_delay, uint32_t controller_loss_timeout)
{
	struct nvmf_ioc_nv nv;
	u_int i;
	int error;

	error = prepare_queues_for_handoff(&nv, dle, hostnqn, admin_qp,
	    num_queues, io_queues, cdata, reconnect_delay,
	    controller_loss_timeout);
	if (error != 0)
		goto out;

	if (ioctl(fd, NVMF_RECONNECT_HOST, &nv) == -1)
		error = errno;
	free(nv.data);

out:
	for (i = 0; i < num_queues; i++)
		(void)nvmf_free_qpair(io_queues[i]);
	(void)nvmf_free_qpair(admin_qp);
	return (error);
}

int
nvmf_connection_status(int fd, nvlist_t **nvlp)
{
	return (nvmf_read_ioc_nv(fd, NVMF_CONNECTION_STATUS, nvlp));
}
