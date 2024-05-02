/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <err.h>
#include <errno.h>
#include <libnvmf.h>
#include <stdlib.h>

#include "internal.h"

struct controller {
	struct nvmf_qpair *qp;

	uint64_t cap;
	uint32_t vs;
	uint32_t cc;
	uint32_t csts;

	bool shutdown;

	struct nvme_controller_data cdata;
};

static bool
update_cc(struct controller *c, uint32_t new_cc)
{
	uint32_t changes;

	if (c->shutdown)
		return (false);
	if (!nvmf_validate_cc(c->qp, c->cap, c->cc, new_cc))
		return (false);

	changes = c->cc ^ new_cc;
	c->cc = new_cc;

	/* Handle shutdown requests. */
	if (NVMEV(NVME_CC_REG_SHN, changes) != 0 &&
	    NVMEV(NVME_CC_REG_SHN, new_cc) != 0) {
		c->csts &= ~NVMEM(NVME_CSTS_REG_SHST);
		c->csts |= NVMEF(NVME_CSTS_REG_SHST, NVME_SHST_COMPLETE);
		c->shutdown = true;
	}

	if (NVMEV(NVME_CC_REG_EN, changes) != 0) {
		if (NVMEV(NVME_CC_REG_EN, new_cc) == 0) {
			/* Controller reset. */
			c->csts = 0;
			c->shutdown = true;
		} else
			c->csts |= NVMEF(NVME_CSTS_REG_RDY, 1);
	}
	return (true);
}

static void
handle_property_get(const struct controller *c, const struct nvmf_capsule *nc,
    const struct nvmf_fabric_prop_get_cmd *pget)
{
	struct nvmf_fabric_prop_get_rsp rsp;

	nvmf_init_cqe(&rsp, nc, 0);

	switch (le32toh(pget->ofst)) {
	case NVMF_PROP_CAP:
		if (pget->attrib.size != NVMF_PROP_SIZE_8)
			goto error;
		rsp.value.u64 = htole64(c->cap);
		break;
	case NVMF_PROP_VS:
		if (pget->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		rsp.value.u32.low = htole32(c->vs);
		break;
	case NVMF_PROP_CC:
		if (pget->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		rsp.value.u32.low = htole32(c->cc);
		break;
	case NVMF_PROP_CSTS:
		if (pget->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		rsp.value.u32.low = htole32(c->csts);
		break;
	default:
		goto error;
	}

	nvmf_send_response(nc, &rsp);
	return;
error:
	nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
}

static void
handle_property_set(struct controller *c, const struct nvmf_capsule *nc,
    const struct nvmf_fabric_prop_set_cmd *pset)
{
	switch (le32toh(pset->ofst)) {
	case NVMF_PROP_CC:
		if (pset->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		if (!update_cc(c, le32toh(pset->value.u32.low)))
			goto error;
		break;
	default:
		goto error;
	}

	nvmf_send_success(nc);
	return;
error:
	nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
}

static void
handle_fabrics_command(struct controller *c,
    const struct nvmf_capsule *nc, const struct nvmf_fabric_cmd *fc)
{
	switch (fc->fctype) {
	case NVMF_FABRIC_COMMAND_PROPERTY_GET:
		handle_property_get(c, nc,
		    (const struct nvmf_fabric_prop_get_cmd *)fc);
		break;
	case NVMF_FABRIC_COMMAND_PROPERTY_SET:
		handle_property_set(c, nc,
		    (const struct nvmf_fabric_prop_set_cmd *)fc);
		break;
	case NVMF_FABRIC_COMMAND_CONNECT:
		warnx("CONNECT command on connected queue");
		nvmf_send_generic_error(nc, NVME_SC_COMMAND_SEQUENCE_ERROR);
		break;
	case NVMF_FABRIC_COMMAND_DISCONNECT:
		warnx("DISCONNECT command on admin queue");
		nvmf_send_error(nc, NVME_SCT_COMMAND_SPECIFIC,
		    NVMF_FABRIC_SC_INVALID_QUEUE_TYPE);
		break;
	default:
		warnx("Unsupported fabrics command %#x", fc->fctype);
		nvmf_send_generic_error(nc, NVME_SC_INVALID_OPCODE);
		break;
	}
}

static void
handle_identify_command(const struct controller *c,
    const struct nvmf_capsule *nc, const struct nvme_command *cmd)
{
	uint8_t cns;

	cns = le32toh(cmd->cdw10) & 0xFF;
	switch (cns) {
	case 1:
		break;
	default:
		warnx("Unsupported CNS %#x for IDENTIFY", cns);
		goto error;
	}

	nvmf_send_controller_data(nc, &c->cdata, sizeof(c->cdata));
	return;
error:
	nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
}

void
controller_handle_admin_commands(struct controller *c, handle_command *cb,
    void *cb_arg)
{
	struct nvmf_qpair *qp = c->qp;
	const struct nvme_command *cmd;
	struct nvmf_capsule *nc;
	int error;

	for (;;) {
		error = nvmf_controller_receive_capsule(qp, &nc);
		if (error != 0) {
			if (error != ECONNRESET)
				warnc(error, "Failed to read command capsule");
			break;
		}

		cmd = nvmf_capsule_sqe(nc);

		/*
		 * Only permit Fabrics commands while a controller is
		 * disabled.
		 */
		if (NVMEV(NVME_CC_REG_EN, c->cc) == 0 &&
		    cmd->opc != NVME_OPC_FABRICS_COMMANDS) {
			warnx("Unsupported admin opcode %#x whiled disabled\n",
			    cmd->opc);
			nvmf_send_generic_error(nc,
			    NVME_SC_COMMAND_SEQUENCE_ERROR);
			nvmf_free_capsule(nc);
			continue;
		}

		if (cb(nc, cmd, cb_arg)) {
			nvmf_free_capsule(nc);
			continue;
		}

		switch (cmd->opc) {
		case NVME_OPC_FABRICS_COMMANDS:
			handle_fabrics_command(c, nc,
			    (const struct nvmf_fabric_cmd *)cmd);
			break;
		case NVME_OPC_IDENTIFY:
			handle_identify_command(c, nc, cmd);
			break;
		default:
			warnx("Unsupported admin opcode %#x", cmd->opc);
			nvmf_send_generic_error(nc, NVME_SC_INVALID_OPCODE);
			break;
		}
		nvmf_free_capsule(nc);
	}
}

struct controller *
init_controller(struct nvmf_qpair *qp,
    const struct nvme_controller_data *cdata)
{
	struct controller *c;

	c = calloc(1, sizeof(*c));
	c->qp = qp;
	c->cap = nvmf_controller_cap(c->qp);
	c->vs = cdata->ver;
	c->cdata = *cdata;

	return (c);
}

void
free_controller(struct controller *c)
{
	free(c);
}
