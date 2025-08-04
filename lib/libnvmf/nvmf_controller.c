/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/utsname.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "libnvmf.h"
#include "internal.h"
#include "nvmft_subr.h"

bool
nvmf_nqn_valid_strict(const char *nqn)
{
	size_t len;

	if (!nvmf_nqn_valid(nqn))
		return (false);

	/*
	 * Stricter checks from the spec.  Linux does not seem to
	 * require these.
	 */
	len = strlen(nqn);

	/*
	 * NVMF_NQN_MIN_LEN does not include '.' and require at least
	 * one character of a domain name.
	 */
	if (len < NVMF_NQN_MIN_LEN + 2)
		return (false);
	if (memcmp("nqn.", nqn, strlen("nqn.")) != 0)
		return (false);
	nqn += strlen("nqn.");

	/* Next 4 digits must be a year. */
	for (u_int i = 0; i < 4; i++) {
		if (!isdigit(nqn[i]))
			return (false);
	}
	nqn += 4;

	/* '-' between year and month. */
	if (nqn[0] != '-')
		return (false);
	nqn++;

	/* 2 digit month. */
	for (u_int i = 0; i < 2; i++) {
		if (!isdigit(nqn[i]))
			return (false);
	}
	nqn += 2;

	/* '.' between month and reverse domain name. */
	if (nqn[0] != '.')
		return (false);
	return (true);
}

void
nvmf_init_cqe(void *cqe, const struct nvmf_capsule *nc, uint16_t status)
{
	struct nvme_completion *cpl = cqe;
	const struct nvme_command *cmd = nvmf_capsule_sqe(nc);

	memset(cpl, 0, sizeof(*cpl));
	cpl->cid = cmd->cid;
	cpl->status = htole16(status);
}

static struct nvmf_capsule *
nvmf_simple_response(const struct nvmf_capsule *nc, uint8_t sc_type,
    uint8_t sc_status)
{
	struct nvme_completion cpl;
	uint16_t status;

	status = NVMEF(NVME_STATUS_SCT, sc_type) |
	    NVMEF(NVME_STATUS_SC, sc_status);
	nvmf_init_cqe(&cpl, nc, status);
	return (nvmf_allocate_response(nc->nc_qpair, &cpl));
}

int
nvmf_controller_receive_capsule(struct nvmf_qpair *qp,
    struct nvmf_capsule **ncp)
{
	struct nvmf_capsule *nc;
	int error;
	uint8_t sc_status;

	*ncp = NULL;
	error = nvmf_receive_capsule(qp, &nc);
	if (error != 0)
		return (error);

	sc_status = nvmf_validate_command_capsule(nc);
	if (sc_status != NVME_SC_SUCCESS) {
		nvmf_send_generic_error(nc, sc_status);
		nvmf_free_capsule(nc);
		return (EPROTO);
	}

	*ncp = nc;
	return (0);
}

int
nvmf_controller_transmit_response(struct nvmf_capsule *nc)
{
	struct nvmf_qpair *qp = nc->nc_qpair;

	/* Set SQHD. */
	if (qp->nq_flow_control) {
		qp->nq_sqhd = (qp->nq_sqhd + 1) % qp->nq_qsize;
		nc->nc_cqe.sqhd = htole16(qp->nq_sqhd);
	} else
		nc->nc_cqe.sqhd = 0;

	return (nvmf_transmit_capsule(nc));
}

int
nvmf_send_response(const struct nvmf_capsule *cc, const void *cqe)
{
	struct nvmf_capsule *rc;
	int error;

	rc = nvmf_allocate_response(cc->nc_qpair, cqe);
	if (rc == NULL)
		return (ENOMEM);
	error = nvmf_controller_transmit_response(rc);
	nvmf_free_capsule(rc);
	return (error);
}

int
nvmf_send_error(const struct nvmf_capsule *cc, uint8_t sc_type,
    uint8_t sc_status)
{
	struct nvmf_capsule *rc;
	int error;

	rc = nvmf_simple_response(cc, sc_type, sc_status);
	error = nvmf_controller_transmit_response(rc);
	nvmf_free_capsule(rc);
	return (error);
}

int
nvmf_send_generic_error(const struct nvmf_capsule *nc, uint8_t sc_status)
{
	return (nvmf_send_error(nc, NVME_SCT_GENERIC, sc_status));
}

int
nvmf_send_success(const struct nvmf_capsule *nc)
{
	return (nvmf_send_generic_error(nc, NVME_SC_SUCCESS));
}

void
nvmf_connect_invalid_parameters(const struct nvmf_capsule *cc, bool data,
    uint16_t offset)
{
	struct nvmf_fabric_connect_rsp rsp;
	struct nvmf_capsule *rc;

	nvmf_init_cqe(&rsp, cc,
	    NVMEF(NVME_STATUS_SCT, NVME_SCT_COMMAND_SPECIFIC) |
	    NVMEF(NVME_STATUS_SC, NVMF_FABRIC_SC_INVALID_PARAM));
	rsp.status_code_specific.invalid.ipo = htole16(offset);
	rsp.status_code_specific.invalid.iattr = data ? 1 : 0;
	rc = nvmf_allocate_response(cc->nc_qpair, &rsp);
	nvmf_transmit_capsule(rc);
	nvmf_free_capsule(rc);
}

struct nvmf_qpair *
nvmf_accept(struct nvmf_association *na, const struct nvmf_qpair_params *params,
    struct nvmf_capsule **ccp, struct nvmf_fabric_connect_data *data)
{
	static const char hostid_zero[sizeof(data->hostid)];
	const struct nvmf_fabric_connect_cmd *cmd;
	struct nvmf_qpair *qp;
	struct nvmf_capsule *cc, *rc;
	u_int qsize;
	int error;
	uint16_t cntlid;
	uint8_t sc_status;

	qp = NULL;
	cc = NULL;
	rc = NULL;
	*ccp = NULL;
	na_clear_error(na);
	if (!na->na_controller) {
		na_error(na, "Cannot accept on a host");
		goto error;
	}

	qp = nvmf_allocate_qpair(na, params);
	if (qp == NULL)
		goto error;

	/* Read the CONNECT capsule. */
	error = nvmf_receive_capsule(qp, &cc);
	if (error != 0) {
		na_error(na, "Failed to receive CONNECT: %s", strerror(error));
		goto error;
	}

	sc_status = nvmf_validate_command_capsule(cc);
	if (sc_status != 0) {
		na_error(na, "CONNECT command failed to validate: %u",
		    sc_status);
		rc = nvmf_simple_response(cc, NVME_SCT_GENERIC, sc_status);
		goto error;
	}

	cmd = nvmf_capsule_sqe(cc);
	if (cmd->opcode != NVME_OPC_FABRICS_COMMANDS ||
	    cmd->fctype != NVMF_FABRIC_COMMAND_CONNECT) {
		na_error(na, "Invalid opcode in CONNECT (%u,%u)", cmd->opcode,
		    cmd->fctype);
		rc = nvmf_simple_response(cc, NVME_SCT_GENERIC,
		    NVME_SC_INVALID_OPCODE);
		goto error;
	}

	if (cmd->recfmt != htole16(0)) {
		na_error(na, "Unsupported CONNECT record format %u",
		    le16toh(cmd->recfmt));
		rc = nvmf_simple_response(cc, NVME_SCT_COMMAND_SPECIFIC,
		    NVMF_FABRIC_SC_INCOMPATIBLE_FORMAT);
		goto error;
	}

	qsize = le16toh(cmd->sqsize) + 1;
	if (cmd->qid == 0) {
		/* Admin queue limits. */
		if (qsize < NVME_MIN_ADMIN_ENTRIES ||
		    qsize > NVME_MAX_ADMIN_ENTRIES ||
		    qsize > na->na_params.max_admin_qsize) {
			na_error(na, "Invalid queue size %u", qsize);
			nvmf_connect_invalid_parameters(cc, false,
			    offsetof(struct nvmf_fabric_connect_cmd, sqsize));
			goto error;
		}
		qp->nq_admin = true;
	} else {
		/* I/O queues not allowed for discovery. */
		if (na->na_params.max_io_qsize == 0) {
			na_error(na, "I/O queue on discovery controller");
			nvmf_connect_invalid_parameters(cc, false,
			    offsetof(struct nvmf_fabric_connect_cmd, qid));
			goto error;
		}

		/* I/O queue limits. */
		if (qsize < NVME_MIN_IO_ENTRIES ||
		    qsize > NVME_MAX_IO_ENTRIES ||
		    qsize > na->na_params.max_io_qsize) {
			na_error(na, "Invalid queue size %u", qsize);
			nvmf_connect_invalid_parameters(cc, false,
			    offsetof(struct nvmf_fabric_connect_cmd, sqsize));
			goto error;
		}

		/* KATO is reserved for I/O queues. */
		if (cmd->kato != 0) {
			na_error(na,
			    "KeepAlive timeout specified for I/O queue");
			nvmf_connect_invalid_parameters(cc, false,
			    offsetof(struct nvmf_fabric_connect_cmd, kato));
			goto error;
		}
		qp->nq_admin = false;
	}
	qp->nq_qsize = qsize;

	/* Fetch CONNECT data. */
	if (nvmf_capsule_data_len(cc) != sizeof(*data)) {
		na_error(na, "Invalid data payload length for CONNECT: %zu",
		    nvmf_capsule_data_len(cc));
		nvmf_connect_invalid_parameters(cc, false,
		    offsetof(struct nvmf_fabric_connect_cmd, sgl1));
		goto error;
	}

	error = nvmf_receive_controller_data(cc, 0, data, sizeof(*data));
	if (error != 0) {
		na_error(na, "Failed to read data for CONNECT: %s",
		    strerror(error));
		rc = nvmf_simple_response(cc, NVME_SCT_GENERIC,
		    NVME_SC_DATA_TRANSFER_ERROR);
		goto error;
	}

	/* The hostid must be non-zero. */
	if (memcmp(data->hostid, hostid_zero, sizeof(hostid_zero)) == 0) {
		na_error(na, "HostID in CONNECT data is zero");
		nvmf_connect_invalid_parameters(cc, true,
		    offsetof(struct nvmf_fabric_connect_data, hostid));
		goto error;
	}

	cntlid = le16toh(data->cntlid);
	if (cmd->qid == 0) {
		if (na->na_params.dynamic_controller_model) {
			if (cntlid != NVMF_CNTLID_DYNAMIC) {
				na_error(na, "Invalid controller ID %#x",
				    cntlid);
				nvmf_connect_invalid_parameters(cc, true,
				    offsetof(struct nvmf_fabric_connect_data,
					cntlid));
				goto error;
			}
		} else {
			if (cntlid > NVMF_CNTLID_STATIC_MAX &&
			    cntlid != NVMF_CNTLID_STATIC_ANY) {
				na_error(na, "Invalid controller ID %#x",
				    cntlid);
				nvmf_connect_invalid_parameters(cc, true,
				    offsetof(struct nvmf_fabric_connect_data,
					cntlid));
				goto error;
			}
		}
	} else {
		/* Wildcard Controller IDs are only valid on an Admin queue. */
		if (cntlid > NVMF_CNTLID_STATIC_MAX) {
			na_error(na, "Invalid controller ID %#x", cntlid);
			nvmf_connect_invalid_parameters(cc, true,
			    offsetof(struct nvmf_fabric_connect_data, cntlid));
			goto error;
		}
	}

	/* Simple validation of each NQN. */
	if (!nvmf_nqn_valid(data->subnqn)) {
		na_error(na, "Invalid SubNQN %.*s", (int)sizeof(data->subnqn),
		    data->subnqn);
		nvmf_connect_invalid_parameters(cc, true,
		    offsetof(struct nvmf_fabric_connect_data, subnqn));
		goto error;
	}
	if (!nvmf_nqn_valid(data->hostnqn)) {
		na_error(na, "Invalid HostNQN %.*s", (int)sizeof(data->hostnqn),
		    data->hostnqn);
		nvmf_connect_invalid_parameters(cc, true,
		    offsetof(struct nvmf_fabric_connect_data, hostnqn));
		goto error;
	}

	if (na->na_params.sq_flow_control ||
	    (cmd->cattr & NVMF_CONNECT_ATTR_DISABLE_SQ_FC) == 0)
		qp->nq_flow_control = true;
	else
		qp->nq_flow_control = false;
	qp->nq_sqhd = 0;
	qp->nq_kato = le32toh(cmd->kato);
	*ccp = cc;
	return (qp);
error:
	if (rc != NULL) {
		nvmf_transmit_capsule(rc);
		nvmf_free_capsule(rc);
	}
	if (cc != NULL)
		nvmf_free_capsule(cc);
	if (qp != NULL)
		nvmf_free_qpair(qp);
	return (NULL);
}

int
nvmf_finish_accept(const struct nvmf_capsule *cc, uint16_t cntlid)
{
	struct nvmf_fabric_connect_rsp rsp;
	struct nvmf_qpair *qp = cc->nc_qpair;
	struct nvmf_capsule *rc;
	int error;

	nvmf_init_cqe(&rsp, cc, 0);
	if (qp->nq_flow_control)
		rsp.sqhd = htole16(qp->nq_sqhd);
	else
		rsp.sqhd = htole16(0xffff);
	rsp.status_code_specific.success.cntlid = htole16(cntlid);
	rc = nvmf_allocate_response(qp, &rsp);
	if (rc == NULL)
		return (ENOMEM);
	error = nvmf_transmit_capsule(rc);
	nvmf_free_capsule(rc);
	if (error == 0)
		qp->nq_cntlid = cntlid;
	return (error);
}

uint64_t
nvmf_controller_cap(struct nvmf_qpair *qp)
{
	const struct nvmf_association *na = qp->nq_association;

	return (_nvmf_controller_cap(na->na_params.max_io_qsize,
	    NVMF_CC_EN_TIMEOUT));
}

bool
nvmf_validate_cc(struct nvmf_qpair *qp, uint64_t cap, uint32_t old_cc,
    uint32_t new_cc)
{
	const struct nvmf_association *na = qp->nq_association;

	return (_nvmf_validate_cc(na->na_params.max_io_qsize, cap, old_cc,
	    new_cc));
}

void
nvmf_init_discovery_controller_data(struct nvmf_qpair *qp,
    struct nvme_controller_data *cdata)
{
	const struct nvmf_association *na = qp->nq_association;
	struct utsname utsname;
	char *cp;

	memset(cdata, 0, sizeof(*cdata));

	/*
	 * 5.2 Figure 37 states model name and serial are reserved,
	 * but Linux includes them.  Don't bother with serial, but
	 * do set model name.
	 */
	uname(&utsname);
	nvmf_strpad(cdata->mn, utsname.sysname, sizeof(cdata->mn));
	nvmf_strpad(cdata->fr, utsname.release, sizeof(cdata->fr));
	cp = memchr(cdata->fr, '-', sizeof(cdata->fr));
	if (cp != NULL)
		memset(cp, ' ', sizeof(cdata->fr) - (cp - (char *)cdata->fr));

	cdata->ctrlr_id = htole16(qp->nq_cntlid);
	cdata->ver = htole32(NVME_REV(1, 4));
	cdata->cntrltype = 2;

	cdata->lpa = NVMEF(NVME_CTRLR_DATA_LPA_EXT_DATA, 1);
	cdata->elpe = 0;

	cdata->maxcmd = htole16(na->na_params.max_admin_qsize);

	/* Transport-specific? */
	cdata->sgls = htole32(
	    NVMEF(NVME_CTRLR_DATA_SGLS_TRANSPORT_DATA_BLOCK, 1) |
	    NVMEF(NVME_CTRLR_DATA_SGLS_ADDRESS_AS_OFFSET, 1) |
	    NVMEF(NVME_CTRLR_DATA_SGLS_NVM_COMMAND_SET, 1));

	strlcpy(cdata->subnqn, NVMF_DISCOVERY_NQN, sizeof(cdata->subnqn));
}

void
nvmf_init_io_controller_data(struct nvmf_qpair *qp, const char *serial,
    const char *subnqn, int nn, uint32_t ioccsz,
    struct nvme_controller_data *cdata)
{
	const struct nvmf_association *na = qp->nq_association;
	struct utsname utsname;

	uname(&utsname);

	memset(cdata, 0, sizeof(*cdata));
	_nvmf_init_io_controller_data(qp->nq_cntlid, na->na_params.max_io_qsize,
	    serial, utsname.sysname, utsname.release, subnqn, nn, ioccsz,
	    sizeof(struct nvme_completion), cdata);
}

uint8_t
nvmf_get_log_page_id(const struct nvme_command *cmd)
{
	assert(cmd->opc == NVME_OPC_GET_LOG_PAGE);
	return (le32toh(cmd->cdw10) & 0xff);
}

uint64_t
nvmf_get_log_page_length(const struct nvme_command *cmd)
{
	uint32_t numd;

	assert(cmd->opc == NVME_OPC_GET_LOG_PAGE);
	numd = le32toh(cmd->cdw10) >> 16 | (le32toh(cmd->cdw11) & 0xffff) << 16;
	return ((numd + 1) * 4);
}

uint64_t
nvmf_get_log_page_offset(const struct nvme_command *cmd)
{
	assert(cmd->opc == NVME_OPC_GET_LOG_PAGE);
	return (le32toh(cmd->cdw12) | (uint64_t)le32toh(cmd->cdw13) << 32);
}

int
nvmf_handoff_controller_qpair(struct nvmf_qpair *qp,
    const struct nvmf_fabric_connect_cmd *cmd,
    const struct nvmf_fabric_connect_data *data, struct nvmf_ioc_nv *nv)
{
	nvlist_t *nvl, *nvl_qp;
	int error;

	error = nvmf_kernel_handoff_params(qp, &nvl_qp);
	if (error)
		return (error);

	nvl = nvlist_create(0);
	nvlist_add_number(nvl, "trtype", qp->nq_association->na_trtype);
	nvlist_move_nvlist(nvl, "params", nvl_qp);
	nvlist_add_binary(nvl, "cmd", cmd, sizeof(*cmd));
	nvlist_add_binary(nvl, "data", data, sizeof(*data));

	error = nvmf_pack_ioc_nvlist(nv, nvl);
	nvlist_destroy(nvl);
	return (error);
}
