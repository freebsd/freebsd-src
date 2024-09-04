/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/sysctl.h>
#include <err.h>
#include <errno.h>
#include <libnvmf.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

struct io_controller {
	struct controller *c;

	u_int num_io_queues;
	u_int active_io_queues;
	struct nvmf_qpair **io_qpairs;
	int *io_sockets;

	struct nvme_firmware_page fp;
	struct nvme_health_information_page hip;
	uint16_t partial_dur;
	uint16_t partial_duw;

	uint16_t cntlid;
	char hostid[16];
	char hostnqn[NVME_NQN_FIELD_SIZE];
};

static struct nvmf_association *io_na;
static pthread_cond_t io_cond;
static pthread_mutex_t io_na_mutex;
static struct io_controller *io_controller;
static const char *nqn;
static char serial[NVME_SERIAL_NUMBER_LENGTH];

void
init_io(const char *subnqn)
{
	struct nvmf_association_params aparams;
	u_long hostid;
	size_t len;

	memset(&aparams, 0, sizeof(aparams));
	aparams.sq_flow_control = !flow_control_disable;
	aparams.dynamic_controller_model = true;
	aparams.max_admin_qsize = NVME_MAX_ADMIN_ENTRIES;
	aparams.max_io_qsize = NVMF_MAX_IO_ENTRIES;
	aparams.tcp.pda = 0;
	aparams.tcp.header_digests = header_digests;
	aparams.tcp.data_digests = data_digests;
	aparams.tcp.maxh2cdata = maxh2cdata;
	io_na = nvmf_allocate_association(NVMF_TRTYPE_TCP, true,
	    &aparams);
	if (io_na == NULL)
		err(1, "Failed to create I/O controller association");

	nqn = subnqn;

	/* Generate a serial number from the kern.hostid node. */
	len = sizeof(hostid);
	if (sysctlbyname("kern.hostid", &hostid, &len, NULL, 0) == -1)
		err(1, "sysctl: kern.hostid");

	nvmf_controller_serial(serial, sizeof(serial), hostid);

	pthread_cond_init(&io_cond, NULL);
	pthread_mutex_init(&io_na_mutex, NULL);

	if (kernel_io)
		init_ctl_port(subnqn, &aparams);
}

void
shutdown_io(void)
{
	if (kernel_io)
		shutdown_ctl_port(nqn);
}

static void
handle_get_log_page(struct io_controller *ioc, const struct nvmf_capsule *nc,
    const struct nvme_command *cmd)
{
	uint64_t offset;
	uint32_t numd;
	size_t len;
	uint8_t lid;

	lid = le32toh(cmd->cdw10) & 0xff;
	numd = le32toh(cmd->cdw10) >> 16 | le32toh(cmd->cdw11) << 16;
	offset = le32toh(cmd->cdw12) | (uint64_t)le32toh(cmd->cdw13) << 32;

	if (offset % 3 != 0)
		goto error;

	len = (numd + 1) * 4;

	switch (lid) {
	case NVME_LOG_ERROR:
	{
		void *buf;

		if (len % sizeof(struct nvme_error_information_entry) != 0)
			goto error;

		buf = calloc(1, len);
		nvmf_send_controller_data(nc, buf, len);
		free(buf);
		return;
	}
	case NVME_LOG_HEALTH_INFORMATION:
		if (len != sizeof(ioc->hip))
			goto error;

		nvmf_send_controller_data(nc, &ioc->hip, sizeof(ioc->hip));
		return;
	case NVME_LOG_FIRMWARE_SLOT:
		if (len != sizeof(ioc->fp))
			goto error;

		nvmf_send_controller_data(nc, &ioc->fp, sizeof(ioc->fp));
		return;
	default:
		warnx("Unsupported page %#x for GET_LOG_PAGE\n", lid);
		goto error;
	}

error:
	nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
}

static bool
handle_io_identify_command(const struct nvmf_capsule *nc,
    const struct nvme_command *cmd)
{
	struct nvme_namespace_data nsdata;
	struct nvme_ns_list nslist;
	uint32_t nsid;
	uint8_t cns;

	cns = le32toh(cmd->cdw10) & 0xFF;
	switch (cns) {
	case 0:	/* Namespace data. */
		if (!device_namespace_data(le32toh(cmd->nsid), &nsdata)) {
			nvmf_send_generic_error(nc,
			    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
			return (true);
		}

		nvmf_send_controller_data(nc, &nsdata, sizeof(nsdata));
		return (true);
	case 2:	/* Active namespace list. */
		nsid = le32toh(cmd->nsid);
		if (nsid >= 0xfffffffe) {
			nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
			return (true);
		}

		device_active_nslist(nsid, &nslist);
		nvmf_send_controller_data(nc, &nslist, sizeof(nslist));
		return (true);
	case 3:	/* Namespace Identification Descriptor list. */
		if (!device_identification_descriptor(le32toh(cmd->nsid),
		    &nsdata)) {
			nvmf_send_generic_error(nc,
			    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
			return (true);
		}

		nvmf_send_controller_data(nc, &nsdata, sizeof(nsdata));
		return (true);
	default:
		return (false);
	}
}

static void
handle_set_features(struct io_controller *ioc, const struct nvmf_capsule *nc,
    const struct nvme_command *cmd)
{
	struct nvme_completion cqe;
	uint8_t fid;

	fid = NVMEV(NVME_FEAT_SET_FID, le32toh(cmd->cdw10));
	switch (fid) {
	case NVME_FEAT_NUMBER_OF_QUEUES:
	{
		uint32_t num_queues;

		if (ioc->num_io_queues != 0) {
			nvmf_send_generic_error(nc,
			    NVME_SC_COMMAND_SEQUENCE_ERROR);
			return;
		}

		num_queues = le32toh(cmd->cdw11) & 0xffff;

		/* 5.12.1.7: 65535 is invalid. */
		if (num_queues == 65535)
			goto error;

		/* Fabrics requires the same number of SQs and CQs. */
		if (le32toh(cmd->cdw11) >> 16 != num_queues)
			goto error;

		/* Convert to 1's based */
		num_queues++;

		/* Lock to synchronize with handle_io_qpair. */
		pthread_mutex_lock(&io_na_mutex);
		ioc->num_io_queues = num_queues;
		ioc->io_qpairs = calloc(num_queues, sizeof(*ioc->io_qpairs));
		ioc->io_sockets = calloc(num_queues, sizeof(*ioc->io_sockets));
		pthread_mutex_unlock(&io_na_mutex);

		nvmf_init_cqe(&cqe, nc, 0);
		cqe.cdw0 = cmd->cdw11;
		nvmf_send_response(nc, &cqe);
		return;
	}
	case NVME_FEAT_ASYNC_EVENT_CONFIGURATION:
	{
		uint32_t aer_mask;

		aer_mask = le32toh(cmd->cdw11);

		/* Check for any reserved or unimplemented feature bits. */
		if ((aer_mask & 0xffffc000) != 0)
			goto error;

		/* No AERs are generated by this daemon. */
		nvmf_send_success(nc);
		return;
	}
	default:
		warnx("Unsupported feature ID %u for SET_FEATURES", fid);
		goto error;
	}

error:
	nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
}

static bool
admin_command(const struct nvmf_capsule *nc, const struct nvme_command *cmd,
    void *arg)
{
	struct io_controller *ioc = arg;

	switch (cmd->opc) {
	case NVME_OPC_GET_LOG_PAGE:
		handle_get_log_page(ioc, nc, cmd);
		return (true);
	case NVME_OPC_IDENTIFY:
		return (handle_io_identify_command(nc, cmd));
	case NVME_OPC_SET_FEATURES:
		handle_set_features(ioc, nc, cmd);
		return (true);
	case NVME_OPC_ASYNC_EVENT_REQUEST:
		/* Ignore and never complete. */
		return (true);
	case NVME_OPC_KEEP_ALIVE:
		nvmf_send_success(nc);
		return (true);
	default:
		return (false);
	}
}

static void
handle_admin_qpair(struct io_controller *ioc)
{
	pthread_setname_np(pthread_self(), "admin queue");

	controller_handle_admin_commands(ioc->c, admin_command, ioc);

	pthread_mutex_lock(&io_na_mutex);
	for (u_int i = 0; i < ioc->num_io_queues; i++) {
		if (ioc->io_qpairs[i] == NULL || ioc->io_sockets[i] == -1)
			continue;
		close(ioc->io_sockets[i]);
		ioc->io_sockets[i] = -1;
	}

	/* Wait for I/O threads to notice. */
	while (ioc->active_io_queues > 0)
		pthread_cond_wait(&io_cond, &io_na_mutex);

	io_controller = NULL;
	pthread_mutex_unlock(&io_na_mutex);

	free_controller(ioc->c);

	free(ioc);
}

static bool
handle_io_fabrics_command(const struct nvmf_capsule *nc,
    const struct nvmf_fabric_cmd *fc)
{
	switch (fc->fctype) {
	case NVMF_FABRIC_COMMAND_CONNECT:
		warnx("CONNECT command on connected queue");
		nvmf_send_generic_error(nc, NVME_SC_COMMAND_SEQUENCE_ERROR);
		break;
	case NVMF_FABRIC_COMMAND_DISCONNECT:
	{
		const struct nvmf_fabric_disconnect_cmd *dis =
		    (const struct nvmf_fabric_disconnect_cmd *)fc;
		if (dis->recfmt != htole16(0)) {
			nvmf_send_error(nc, NVME_SCT_COMMAND_SPECIFIC,
			    NVMF_FABRIC_SC_INCOMPATIBLE_FORMAT);
			break;
		}
		nvmf_send_success(nc);
		return (true);
	}
	default:
		warnx("Unsupported fabrics command %#x", fc->fctype);
		nvmf_send_generic_error(nc, NVME_SC_INVALID_OPCODE);
		break;
	}

	return (false);
}

static void
hip_add(uint64_t pair[2], uint64_t addend)
{
	uint64_t old, new;

	old = le64toh(pair[0]);
	new = old + addend;
	pair[0] = htole64(new);
	if (new < old)
		pair[1] += htole64(1);
}

static uint64_t
cmd_lba(const struct nvme_command *cmd)
{
	return ((uint64_t)le32toh(cmd->cdw11) << 32 | le32toh(cmd->cdw10));
}

static u_int
cmd_nlb(const struct nvme_command *cmd)
{
	return ((le32toh(cmd->cdw12) & 0xffff) + 1);
}

static void
handle_read(struct io_controller *ioc, const struct nvmf_capsule *nc,
    const struct nvme_command *cmd)
{
	size_t len;

	len = nvmf_capsule_data_len(nc);
	device_read(le32toh(cmd->nsid), cmd_lba(cmd), cmd_nlb(cmd), nc);
	hip_add(ioc->hip.host_read_commands, 1);

	len /= 512;
	len += ioc->partial_dur;
	if (len > 1000)
		hip_add(ioc->hip.data_units_read, len / 1000);
	ioc->partial_dur = len % 1000;
}

static void
handle_write(struct io_controller *ioc, const struct nvmf_capsule *nc,
    const struct nvme_command *cmd)
{
	size_t len;

	len = nvmf_capsule_data_len(nc);
	device_write(le32toh(cmd->nsid), cmd_lba(cmd), cmd_nlb(cmd), nc);
	hip_add(ioc->hip.host_write_commands, 1);

	len /= 512;
	len += ioc->partial_duw;
	if (len > 1000)
		hip_add(ioc->hip.data_units_written, len / 1000);
	ioc->partial_duw = len % 1000;
}

static void
handle_flush(const struct nvmf_capsule *nc, const struct nvme_command *cmd)
{
	device_flush(le32toh(cmd->nsid), nc);
}

static bool
handle_io_commands(struct io_controller *ioc, struct nvmf_qpair *qp)
{
	const struct nvme_command *cmd;
	struct nvmf_capsule *nc;
	int error;
	bool disconnect;

	disconnect = false;

	while (!disconnect) {
		error = nvmf_controller_receive_capsule(qp, &nc);
		if (error != 0) {
			if (error != ECONNRESET)
				warnc(error, "Failed to read command capsule");
			break;
		}

		cmd = nvmf_capsule_sqe(nc);

		switch (cmd->opc) {
		case NVME_OPC_FLUSH:
			if (cmd->nsid == htole32(0xffffffff)) {
				nvmf_send_generic_error(nc,
				    NVME_SC_INVALID_NAMESPACE_OR_FORMAT);
				break;
			}
			handle_flush(nc, cmd);
			break;
		case NVME_OPC_WRITE:
			handle_write(ioc, nc, cmd);
			break;
		case NVME_OPC_READ:
			handle_read(ioc, nc, cmd);
			break;
		case NVME_OPC_FABRICS_COMMANDS:
			disconnect = handle_io_fabrics_command(nc,
			    (const struct nvmf_fabric_cmd *)cmd);
			break;
		default:
			warnx("Unsupported NVM opcode %#x", cmd->opc);
			nvmf_send_generic_error(nc, NVME_SC_INVALID_OPCODE);
			break;
		}
		nvmf_free_capsule(nc);
	}

	return (disconnect);
}

static void
handle_io_qpair(struct io_controller *ioc, struct nvmf_qpair *qp, int qid)
{
	char name[64];
	bool disconnect;

	snprintf(name, sizeof(name), "I/O queue %d", qid);
	pthread_setname_np(pthread_self(), name);

	disconnect = handle_io_commands(ioc, qp);

	pthread_mutex_lock(&io_na_mutex);
	if (disconnect)
		ioc->io_qpairs[qid - 1] = NULL;
	if (ioc->io_sockets[qid - 1] != -1) {
		close(ioc->io_sockets[qid - 1]);
		ioc->io_sockets[qid - 1] = -1;
	}
	ioc->active_io_queues--;
	if (ioc->active_io_queues == 0)
		pthread_cond_broadcast(&io_cond);
	pthread_mutex_unlock(&io_na_mutex);
}

static void
connect_admin_qpair(int s, struct nvmf_qpair *qp, struct nvmf_capsule *nc,
    const struct nvmf_fabric_connect_data *data)
{
	struct nvme_controller_data cdata;
	struct io_controller *ioc;
	int error;

	/* Can only have one active I/O controller at a time. */
	pthread_mutex_lock(&io_na_mutex);
	if (io_controller != NULL) {
		pthread_mutex_unlock(&io_na_mutex);
		nvmf_send_error(nc, NVME_SCT_COMMAND_SPECIFIC,
		    NVMF_FABRIC_SC_CONTROLLER_BUSY);
		goto error;
	}

	error = nvmf_finish_accept(nc, 2);
	if (error != 0) {
		pthread_mutex_unlock(&io_na_mutex);
		warnc(error, "Failed to send CONNECT response");
		goto error;
	}

	ioc = calloc(1, sizeof(*ioc));
	ioc->cntlid = 2;
	memcpy(ioc->hostid, data->hostid, sizeof(ioc->hostid));
	memcpy(ioc->hostnqn, data->hostnqn, sizeof(ioc->hostnqn));

	nvmf_init_io_controller_data(qp, serial, nqn, device_count(),
	    NVMF_IOCCSZ, &cdata);

	ioc->fp.afi = NVMEF(NVME_FIRMWARE_PAGE_AFI_SLOT, 1);
	memcpy(ioc->fp.revision[0], cdata.fr, sizeof(cdata.fr));

	ioc->hip.power_cycles[0] = 1;

	ioc->c = init_controller(qp, &cdata);

	io_controller = ioc;
	pthread_mutex_unlock(&io_na_mutex);

	nvmf_free_capsule(nc);

	handle_admin_qpair(ioc);
	close(s);
	return;

error:
	nvmf_free_capsule(nc);
	close(s);
}

static void
connect_io_qpair(int s, struct nvmf_qpair *qp, struct nvmf_capsule *nc,
    const struct nvmf_fabric_connect_data *data, uint16_t qid)
{
	struct io_controller *ioc;
	int error;

	pthread_mutex_lock(&io_na_mutex);
	if (io_controller == NULL) {
		pthread_mutex_unlock(&io_na_mutex);
		warnx("Attempt to create I/O qpair without admin qpair");
		nvmf_send_generic_error(nc, NVME_SC_COMMAND_SEQUENCE_ERROR);
		goto error;
	}

	if (memcmp(io_controller->hostid, data->hostid,
	    sizeof(data->hostid)) != 0) {
		pthread_mutex_unlock(&io_na_mutex);
		warnx("hostid mismatch for I/O qpair CONNECT");
		nvmf_connect_invalid_parameters(nc, true,
		    offsetof(struct nvmf_fabric_connect_data, hostid));
		goto error;
	}
	if (le16toh(data->cntlid) != io_controller->cntlid) {
		pthread_mutex_unlock(&io_na_mutex);
		warnx("cntlid mismatch for I/O qpair CONNECT");
		nvmf_connect_invalid_parameters(nc, true,
		    offsetof(struct nvmf_fabric_connect_data, cntlid));
		goto error;
	}
	if (memcmp(io_controller->hostnqn, data->hostnqn,
	    sizeof(data->hostid)) != 0) {
		pthread_mutex_unlock(&io_na_mutex);
		warnx("host NQN mismatch for I/O qpair CONNECT");
		nvmf_connect_invalid_parameters(nc, true,
		    offsetof(struct nvmf_fabric_connect_data, hostnqn));
		goto error;
	}

	if (io_controller->num_io_queues == 0) {
		pthread_mutex_unlock(&io_na_mutex);
		warnx("Attempt to create I/O qpair without enabled queues");
		nvmf_send_generic_error(nc, NVME_SC_COMMAND_SEQUENCE_ERROR);
		goto error;
	}
	if (qid > io_controller->num_io_queues) {
		pthread_mutex_unlock(&io_na_mutex);
		warnx("Attempt to create invalid I/O qpair %u", qid);
		nvmf_connect_invalid_parameters(nc, false,
		    offsetof(struct nvmf_fabric_connect_cmd, qid));
		goto error;
	}
	if (io_controller->io_qpairs[qid - 1] != NULL) {
		pthread_mutex_unlock(&io_na_mutex);
		warnx("Attempt to re-create I/O qpair %u", qid);
		nvmf_send_generic_error(nc, NVME_SC_COMMAND_SEQUENCE_ERROR);
		goto error;
	}

	error = nvmf_finish_accept(nc, io_controller->cntlid);
	if (error != 0) {
		pthread_mutex_unlock(&io_na_mutex);
		warnc(error, "Failed to send CONNECT response");
		goto error;
	}

	ioc = io_controller;
	ioc->active_io_queues++;
	ioc->io_qpairs[qid - 1] = qp;
	ioc->io_sockets[qid - 1] = s;
	pthread_mutex_unlock(&io_na_mutex);

	nvmf_free_capsule(nc);

	handle_io_qpair(ioc, qp, qid);
	return;

error:
	nvmf_free_capsule(nc);
	close(s);
}

static void *
io_socket_thread(void *arg)
{
	struct nvmf_fabric_connect_data data;
	struct nvmf_qpair_params qparams;
	const struct nvmf_fabric_connect_cmd *cmd;
	struct nvmf_capsule *nc;
	struct nvmf_qpair *qp;
	int s;

	pthread_detach(pthread_self());

	s = (intptr_t)arg;
	memset(&qparams, 0, sizeof(qparams));
	qparams.tcp.fd = s;

	nc = NULL;
	qp = nvmf_accept(io_na, &qparams, &nc, &data);
	if (qp == NULL) {
		warnx("Failed to create I/O qpair: %s",
		    nvmf_association_error(io_na));
		goto error;
	}

	if (kernel_io) {
		ctl_handoff_qpair(qp, nvmf_capsule_sqe(nc), &data);
		goto error;
	}

	if (strcmp(data.subnqn, nqn) != 0) {
		warn("I/O qpair with invalid SubNQN: %.*s",
		    (int)sizeof(data.subnqn), data.subnqn);
		nvmf_connect_invalid_parameters(nc, true,
		    offsetof(struct nvmf_fabric_connect_data, subnqn));
		goto error;
	}

	/* Is this an admin or I/O queue pair? */
	cmd = nvmf_capsule_sqe(nc);
	if (cmd->qid == 0)
		connect_admin_qpair(s, qp, nc, &data);
	else
		connect_io_qpair(s, qp, nc, &data, le16toh(cmd->qid));
	nvmf_free_qpair(qp);
	return (NULL);

error:
	if (nc != NULL)
		nvmf_free_capsule(nc);
	if (qp != NULL)
		nvmf_free_qpair(qp);
	close(s);
	return (NULL);
}

void
handle_io_socket(int s)
{
	pthread_t thr;
	int error;

	error = pthread_create(&thr, NULL, io_socket_thread,
	    (void *)(uintptr_t)s);
	if (error != 0) {
		warnc(error, "Failed to create I/O qpair thread");
		close(s);
	}
}
