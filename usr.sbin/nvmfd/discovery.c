/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <err.h>
#include <libnvmf.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

struct io_controller_data {
	struct nvme_discovery_log_entry entry;
	bool wildcard;
};

struct discovery_controller {
	struct nvme_discovery_log *discovery_log;
	size_t discovery_log_len;
	int s;
};

struct discovery_thread_arg {
	struct controller *c;
	struct nvmf_qpair *qp;
	int s;
};

static struct io_controller_data *io_controllers;
static struct nvmf_association *discovery_na;
static u_int num_io_controllers;

static bool
init_discovery_log_entry(struct nvme_discovery_log_entry *entry, int s,
    const char *subnqn)
{
	struct sockaddr_storage ss;
	socklen_t len;
	bool wildcard;

	len = sizeof(ss);
	if (getsockname(s, (struct sockaddr *)&ss, &len) == -1)
		err(1, "getsockname");

	memset(entry, 0, sizeof(*entry));
	entry->trtype = NVMF_TRTYPE_TCP;
	switch (ss.ss_family) {
	case AF_INET:
	{
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)&ss;
		entry->adrfam = NVMF_ADRFAM_IPV4;
		snprintf(entry->trsvcid, sizeof(entry->trsvcid), "%u",
		    htons(sin->sin_port));
		if (inet_ntop(AF_INET, &sin->sin_addr, entry->traddr,
		    sizeof(entry->traddr)) == NULL)
			err(1, "inet_ntop");
		wildcard = (sin->sin_addr.s_addr == htonl(INADDR_ANY));
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&ss;
		entry->adrfam = NVMF_ADRFAM_IPV6;
		snprintf(entry->trsvcid, sizeof(entry->trsvcid), "%u",
		    htons(sin6->sin6_port));
		if (inet_ntop(AF_INET6, &sin6->sin6_addr, entry->traddr,
		    sizeof(entry->traddr)) == NULL)
			err(1, "inet_ntop");
		wildcard = (memcmp(&sin6->sin6_addr, &in6addr_any,
		    sizeof(in6addr_any)) == 0);
		break;
	}
	default:
		errx(1, "Unsupported address family %u", ss.ss_family);
	}
	entry->subtype = NVMF_SUBTYPE_NVME;
	if (flow_control_disable)
		entry->treq |= (1 << 2);
	entry->portid = htole16(1);
	entry->cntlid = htole16(NVMF_CNTLID_DYNAMIC);
	entry->aqsz = NVME_MAX_ADMIN_ENTRIES;
	strlcpy(entry->subnqn, subnqn, sizeof(entry->subnqn));
	return (wildcard);
}

void
init_discovery(void)
{
	struct nvmf_association_params aparams;

	memset(&aparams, 0, sizeof(aparams));
	aparams.sq_flow_control = false;
	aparams.dynamic_controller_model = true;
	aparams.max_admin_qsize = NVME_MAX_ADMIN_ENTRIES;
	aparams.tcp.pda = 0;
	aparams.tcp.header_digests = header_digests;
	aparams.tcp.data_digests = data_digests;
	aparams.tcp.maxr2t = 1;
	aparams.tcp.maxh2cdata = 256 * 1024;
	discovery_na = nvmf_allocate_association(NVMF_TRTYPE_TCP, true,
	    &aparams);
	if (discovery_na == NULL)
		err(1, "Failed to create discovery association");
}

void
discovery_add_io_controller(int s, const char *subnqn)
{
	struct io_controller_data *icd;

	io_controllers = reallocf(io_controllers, (num_io_controllers + 1) *
	    sizeof(*io_controllers));

	icd = &io_controllers[num_io_controllers];
	num_io_controllers++;

	icd->wildcard = init_discovery_log_entry(&icd->entry, s, subnqn);
}

static void
build_discovery_log_page(struct discovery_controller *dc)
{
	struct sockaddr_storage ss;
	socklen_t len;
	char traddr[256];
	u_int i, nentries;
	uint8_t adrfam;

	if (dc->discovery_log != NULL)
		return;

	len = sizeof(ss);
	if (getsockname(dc->s, (struct sockaddr *)&ss, &len) == -1) {
		warn("build_discovery_log_page: getsockname");
		return;
	}

	memset(traddr, 0, sizeof(traddr));
	switch (ss.ss_family) {
	case AF_INET:
	{
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)&ss;
		adrfam = NVMF_ADRFAM_IPV4;
		if (inet_ntop(AF_INET, &sin->sin_addr, traddr,
		    sizeof(traddr)) == NULL) {
			warn("build_discovery_log_page: inet_ntop");
			return;
		}
		break;
	}
	case AF_INET6:
	{
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)&ss;
		adrfam = NVMF_ADRFAM_IPV6;
		if (inet_ntop(AF_INET6, &sin6->sin6_addr, traddr,
		    sizeof(traddr)) == NULL) {
			warn("build_discovery_log_page: inet_ntop");
			return;
		}
		break;
	}
	default:
		assert(false);
	}

	nentries = 0;
	for (i = 0; i < num_io_controllers; i++) {
		if (io_controllers[i].wildcard &&
		    io_controllers[i].entry.adrfam != adrfam)
			continue;
		nentries++;
	}

	dc->discovery_log_len = sizeof(*dc->discovery_log) +
	    nentries * sizeof(struct nvme_discovery_log_entry);
	dc->discovery_log = calloc(dc->discovery_log_len, 1);
	dc->discovery_log->numrec = nentries;
	dc->discovery_log->recfmt = 0;
	nentries = 0;
	for (i = 0; i < num_io_controllers; i++) {
		if (io_controllers[i].wildcard &&
		    io_controllers[i].entry.adrfam != adrfam)
			continue;

		dc->discovery_log->entries[nentries] = io_controllers[i].entry;
		if (io_controllers[i].wildcard)
			memcpy(dc->discovery_log->entries[nentries].traddr,
			    traddr, sizeof(traddr));
	}
}

static void
handle_get_log_page_command(const struct nvmf_capsule *nc,
    const struct nvme_command *cmd, struct discovery_controller *dc)
{
	uint64_t offset;
	uint32_t length;

	switch (nvmf_get_log_page_id(cmd)) {
	case NVME_LOG_DISCOVERY:
		break;
	default:
		warnx("Unsupported log page %u for discovery controller",
		    nvmf_get_log_page_id(cmd));
		goto error;
	}

	build_discovery_log_page(dc);

	offset = nvmf_get_log_page_offset(cmd);
	if (offset >= dc->discovery_log_len)
		goto error;

	length = nvmf_get_log_page_length(cmd);
	if (length > dc->discovery_log_len - offset)
		length = dc->discovery_log_len - offset;

	nvmf_send_controller_data(nc, (char *)dc->discovery_log + offset,
	    length);
	return;
error:
	nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
}

static bool
discovery_command(const struct nvmf_capsule *nc, const struct nvme_command *cmd,
    void *arg)
{
	struct discovery_controller *dc = arg;

	switch (cmd->opc) {
	case NVME_OPC_GET_LOG_PAGE:
		handle_get_log_page_command(nc, cmd, dc);
		return (true);
	default:
		return (false);
	}
}

static void *
discovery_thread(void *arg)
{
	struct discovery_thread_arg *dta = arg;
	struct discovery_controller dc;

	pthread_detach(pthread_self());

	memset(&dc, 0, sizeof(dc));
	dc.s = dta->s;

	controller_handle_admin_commands(dta->c, discovery_command, &dc);

	free(dc.discovery_log);
	free_controller(dta->c);

	nvmf_free_qpair(dta->qp);

	close(dta->s);
	free(dta);
	return (NULL);
}

void
handle_discovery_socket(int s)
{
	struct nvmf_fabric_connect_data data;
	struct nvme_controller_data cdata;
	struct nvmf_qpair_params qparams;
	struct discovery_thread_arg *dta;
	struct nvmf_capsule *nc;
	struct nvmf_qpair *qp;
	pthread_t thr;
	int error;

	memset(&qparams, 0, sizeof(qparams));
	qparams.tcp.fd = s;

	nc = NULL;
	qp = nvmf_accept(discovery_na, &qparams, &nc, &data);
	if (qp == NULL) {
		warnx("Failed to create discovery qpair: %s",
		    nvmf_association_error(discovery_na));
		goto error;
	}

	if (strcmp(data.subnqn, NVMF_DISCOVERY_NQN) != 0) {
		warn("Discovery qpair with invalid SubNQN: %.*s",
		    (int)sizeof(data.subnqn), data.subnqn);
		nvmf_connect_invalid_parameters(nc, true,
		    offsetof(struct nvmf_fabric_connect_data, subnqn));
		goto error;
	}

	/* Just use a controller ID of 1 for all discovery controllers. */
	error = nvmf_finish_accept(nc, 1);
	if (error != 0) {
		warnc(error, "Failed to send CONNECT reponse");
		goto error;
	}

	nvmf_init_discovery_controller_data(qp, &cdata);

	dta = malloc(sizeof(*dta));
	dta->qp = qp;
	dta->s = s;
	dta->c = init_controller(qp, &cdata);

	error = pthread_create(&thr, NULL, discovery_thread, dta);
	if (error != 0) {
		warnc(error, "Failed to create discovery thread");
		free_controller(dta->c);
		free(dta);
		goto error;
	}

	nvmf_free_capsule(nc);
	return;

error:
	if (nc != NULL)
		nvmf_free_capsule(nc);
	if (qp != NULL)
		nvmf_free_qpair(qp);
	close(s);
}
