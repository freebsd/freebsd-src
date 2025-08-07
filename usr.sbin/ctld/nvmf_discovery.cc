/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2025 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <libiscsiutil.h>
#include <libnvmf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include "ctld.hh"
#include "nvmf.hh"

struct discovery_log {
	discovery_log(const struct portal_group *pg);

	const char *data() const { return buf.data(); }
	size_t length() const { return buf.size(); }

	void append(const struct nvme_discovery_log_entry *entry);

private:
	struct nvme_discovery_log *header()
	{ return reinterpret_cast<struct nvme_discovery_log *>(buf.data()); }

	std::vector<char> buf;
};

struct discovery_controller {
	discovery_controller(freebsd::fd_up s, struct nvmf_qpair *qp,
	    const discovery_log &discovery_log);

	void handle_admin_commands();
private:
	bool update_cc(uint32_t new_cc);
	void handle_property_get(const struct nvmf_capsule *nc,
	    const struct nvmf_fabric_prop_get_cmd *pget);
	void handle_property_set(const struct nvmf_capsule *nc,
	    const struct nvmf_fabric_prop_set_cmd *pset);
	void handle_fabrics_command(const struct nvmf_capsule *nc,
	    const struct nvmf_fabric_cmd *cmd);
	void handle_identify_command(const struct nvmf_capsule *nc,
	    const struct nvme_command *cmd);
	void handle_get_log_page_command(const struct nvmf_capsule *nc,
	    const struct nvme_command *cmd);

	struct nvmf_qpair *qp;

	uint64_t cap = 0;
	uint32_t vs = 0;
	uint32_t cc = 0;
	uint32_t csts = 0;

	bool shutdown = false;

	struct nvme_controller_data cdata;

	const struct discovery_log &discovery_log;
	freebsd::fd_up s;
};

discovery_log::discovery_log(const struct portal_group *pg) :
	buf(sizeof(nvme_discovery_log))
{
	struct nvme_discovery_log *log = header();

	log->genctr = htole32(pg->conf()->genctr());
	log->recfmt = 0;
}

void
discovery_log::append(const struct nvme_discovery_log_entry *entry)
{
	const char *cp = reinterpret_cast<const char *>(entry);
	buf.insert(buf.end(), cp, cp + sizeof(*entry));

	struct nvme_discovery_log *log = header();
	log->numrec = htole32(le32toh(log->numrec) + 1);
}

static bool
discovery_controller_filtered(const struct portal_group *pg,
    const struct sockaddr *client_sa, std::string_view hostnqn,
    const struct port *port)
{
	const struct target *targ = port->target();
	const struct auth_group *ag = port->auth_group();
	if (ag == nullptr)
		ag = targ->auth_group();

	assert(pg->discovery_filter() != discovery_filter::UNKNOWN);

	if (pg->discovery_filter() >= discovery_filter::PORTAL &&
	    !ag->host_permitted(client_sa)) {
		log_debugx("host address does not match addresses "
		    "allowed for controller \"%s\"; skipping", targ->name());
		return true;
	}

	if (pg->discovery_filter() >= discovery_filter::PORTAL_NAME &&
	    !ag->host_permitted(hostnqn) != 0) {
		log_debugx("HostNQN does not match NQNs "
		    "allowed for controller \"%s\"; skipping", targ->name());
		return true;
	}

	/* XXX: auth not yet implemented for NVMe */

	return false;
}

static bool
portal_uses_wildcard_address(const struct portal *p)
{
	const struct addrinfo *ai = p->ai();

	switch (ai->ai_family) {
	case AF_INET:
	{
		const struct sockaddr_in *sin;

		sin = (const struct sockaddr_in *)ai->ai_addr;
		return sin->sin_addr.s_addr == htonl(INADDR_ANY);
	}
	case AF_INET6:
	{
		const struct sockaddr_in6 *sin6;

		sin6 = (const struct sockaddr_in6 *)ai->ai_addr;
		return memcmp(&sin6->sin6_addr, &in6addr_any,
		    sizeof(in6addr_any)) == 0;
	}
	default:
		__assert_unreachable();
	}
}

static bool
init_discovery_log_entry(struct nvme_discovery_log_entry *entry,
    const struct target *target, const struct portal *portal,
    const char *wildcard_host)
{
	/*
	 * The TCP port for I/O controllers might not be fixed, so
	 * fetch the sockaddr of the socket to determine which port
	 * the kernel chose.
	 */
	struct sockaddr_storage ss;
	socklen_t len = sizeof(ss);
	if (getsockname(portal->socket(), (struct sockaddr *)&ss, &len) == -1) {
		log_warn("Failed getsockname building discovery log entry");
		return false;
	}

	const struct nvmf_association_params *aparams =
	    static_cast<const nvmf_portal *>(portal)->aparams();

	memset(entry, 0, sizeof(*entry));
	entry->trtype = NVMF_TRTYPE_TCP;
	int error = getnameinfo((struct sockaddr *)&ss, len,
	    (char *)entry->traddr, sizeof(entry->traddr),
	    (char *)entry->trsvcid, sizeof(entry->trsvcid),
	    NI_NUMERICHOST | NI_NUMERICSERV);
	if (error != 0) {
		log_warnx("Failed getnameinfo building discovery log entry: %s",
		    gai_strerror(error));
		return false;
	}

	if (portal_uses_wildcard_address(portal))
		strncpy((char *)entry->traddr, wildcard_host,
		    sizeof(entry->traddr));
	switch (portal->ai()->ai_family) {
	case AF_INET:
		entry->adrfam = NVMF_ADRFAM_IPV4;
		break;
	case AF_INET6:
		entry->adrfam = NVMF_ADRFAM_IPV6;
		break;
	default:
		__assert_unreachable();
	}
	entry->subtype = NVMF_SUBTYPE_NVME;
	if (!aparams->sq_flow_control)
		entry->treq |= (1 << 2);
	entry->portid = htole16(portal->portal_group()->tag());
	entry->cntlid = htole16(NVMF_CNTLID_DYNAMIC);
	entry->aqsz = aparams->max_admin_qsize;
	strncpy((char *)entry->subnqn, target->name(), sizeof(entry->subnqn));
	return true;
}

static discovery_log
build_discovery_log_page(const struct portal_group *pg, int fd,
    const struct sockaddr *client_sa,
    const struct nvmf_fabric_connect_data &data)
{
	discovery_log discovery_log(pg);

	struct sockaddr_storage ss;
	socklen_t len = sizeof(ss);
	if (getsockname(fd, (struct sockaddr *)&ss, &len) == -1) {
		log_warn("build_discovery_log_page: getsockname");
		return discovery_log;
	}

	char wildcard_host[NI_MAXHOST];
	int error = getnameinfo((struct sockaddr *)&ss, len, wildcard_host,
	    sizeof(wildcard_host), NULL, 0, NI_NUMERICHOST);
	if (error != 0) {
		log_warnx("build_discovery_log_page: getnameinfo: %s",
		    gai_strerror(error));
		return discovery_log;
	}

	const char *nqn = (const char *)data.hostnqn;
	std::string hostnqn(nqn, strnlen(nqn, sizeof(data.hostnqn)));
	for (const auto &kv : pg->ports()) {
		const struct port *port = kv.second;
		if (discovery_controller_filtered(pg, client_sa, hostnqn, port))
			continue;

		for (const portal_up &portal : pg->portals()) {
			if (portal->protocol() != portal_protocol::NVME_TCP)
				continue;

			if (portal_uses_wildcard_address(portal.get()) &&
			    portal->ai()->ai_family != client_sa->sa_family)
				continue;

			struct nvme_discovery_log_entry entry;
			if (init_discovery_log_entry(&entry, port->target(),
			    portal.get(), wildcard_host))
				discovery_log.append(&entry);
		}
	}

	return discovery_log;
}

bool
discovery_controller::update_cc(uint32_t new_cc)
{
	uint32_t changes;

	if (shutdown)
		return false;
	if (!nvmf_validate_cc(qp, cap, cc, new_cc))
		return false;

	changes = cc ^ new_cc;
	cc = new_cc;

	/* Handle shutdown requests. */
	if (NVMEV(NVME_CC_REG_SHN, changes) != 0 &&
	    NVMEV(NVME_CC_REG_SHN, new_cc) != 0) {
		csts &= ~NVMEM(NVME_CSTS_REG_SHST);
		csts |= NVMEF(NVME_CSTS_REG_SHST, NVME_SHST_COMPLETE);
		shutdown = true;
	}

	if (NVMEV(NVME_CC_REG_EN, changes) != 0) {
		if (NVMEV(NVME_CC_REG_EN, new_cc) == 0) {
			/* Controller reset. */
			csts = 0;
			shutdown = true;
		} else
			csts |= NVMEF(NVME_CSTS_REG_RDY, 1);
	}
	return true;
}

void
discovery_controller::handle_property_get(const struct nvmf_capsule *nc,
    const struct nvmf_fabric_prop_get_cmd *pget)
{
	struct nvmf_fabric_prop_get_rsp rsp;

	nvmf_init_cqe(&rsp, nc, 0);

	switch (le32toh(pget->ofst)) {
	case NVMF_PROP_CAP:
		if (pget->attrib.size != NVMF_PROP_SIZE_8)
			goto error;
		rsp.value.u64 = htole64(cap);
		break;
	case NVMF_PROP_VS:
		if (pget->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		rsp.value.u32.low = htole32(vs);
		break;
	case NVMF_PROP_CC:
		if (pget->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		rsp.value.u32.low = htole32(cc);
		break;
	case NVMF_PROP_CSTS:
		if (pget->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		rsp.value.u32.low = htole32(csts);
		break;
	default:
		goto error;
	}

	nvmf_send_response(nc, &rsp);
	return;
error:
	nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
}

void
discovery_controller::handle_property_set(const struct nvmf_capsule *nc,
    const struct nvmf_fabric_prop_set_cmd *pset)
{
	switch (le32toh(pset->ofst)) {
	case NVMF_PROP_CC:
		if (pset->attrib.size != NVMF_PROP_SIZE_4)
			goto error;
		if (!update_cc(le32toh(pset->value.u32.low)))
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

void
discovery_controller::handle_fabrics_command(const struct nvmf_capsule *nc,
    const struct nvmf_fabric_cmd *fc)
{
	switch (fc->fctype) {
	case NVMF_FABRIC_COMMAND_PROPERTY_GET:
		handle_property_get(nc,
		    (const struct nvmf_fabric_prop_get_cmd *)fc);
		break;
	case NVMF_FABRIC_COMMAND_PROPERTY_SET:
		handle_property_set(nc,
		    (const struct nvmf_fabric_prop_set_cmd *)fc);
		break;
	case NVMF_FABRIC_COMMAND_CONNECT:
		log_warnx("CONNECT command on connected queue");
		nvmf_send_generic_error(nc, NVME_SC_COMMAND_SEQUENCE_ERROR);
		break;
	case NVMF_FABRIC_COMMAND_DISCONNECT:
		log_warnx("DISCONNECT command on admin queue");
		nvmf_send_error(nc, NVME_SCT_COMMAND_SPECIFIC,
		    NVMF_FABRIC_SC_INVALID_QUEUE_TYPE);
		break;
	default:
		log_warnx("Unsupported fabrics command %#x", fc->fctype);
		nvmf_send_generic_error(nc, NVME_SC_INVALID_OPCODE);
		break;
	}
}

void
discovery_controller::handle_identify_command(const struct nvmf_capsule *nc,
    const struct nvme_command *cmd)
{
	uint8_t cns;

	cns = le32toh(cmd->cdw10) & 0xFF;
	switch (cns) {
	case 1:
		break;
	default:
		log_warnx("Unsupported CNS %#x for IDENTIFY", cns);
		goto error;
	}

	nvmf_send_controller_data(nc, &cdata, sizeof(cdata));
	return;
error:
	nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
}

void
discovery_controller::handle_get_log_page_command(const struct nvmf_capsule *nc,
    const struct nvme_command *cmd)
{
	uint64_t offset;
	uint32_t length;

	switch (nvmf_get_log_page_id(cmd)) {
	case NVME_LOG_DISCOVERY:
		break;
	default:
		log_warnx("Unsupported log page %u for discovery controller",
		    nvmf_get_log_page_id(cmd));
		goto error;
	}

	offset = nvmf_get_log_page_offset(cmd);
	if (offset >= discovery_log.length())
		goto error;

	length = nvmf_get_log_page_length(cmd);
	if (length > discovery_log.length() - offset)
		length = discovery_log.length() - offset;

	nvmf_send_controller_data(nc, discovery_log.data() + offset, length);
	return;
error:
	nvmf_send_generic_error(nc, NVME_SC_INVALID_FIELD);
}

void
discovery_controller::handle_admin_commands()
{
	for (;;) {
		struct nvmf_capsule *nc;
		int error = nvmf_controller_receive_capsule(qp, &nc);
		if (error != 0) {
			if (error != ECONNRESET)
				log_warnc(error,
				    "Failed to read command capsule");
			break;
		}
		nvmf_capsule_up nc_guard(nc);

		const struct nvme_command *cmd =
		    (const struct nvme_command *)nvmf_capsule_sqe(nc);

		/*
		 * Only permit Fabrics commands while a controller is
		 * disabled.
		 */
		if (NVMEV(NVME_CC_REG_EN, cc) == 0 &&
		    cmd->opc != NVME_OPC_FABRICS_COMMANDS) {
			log_warnx("Unsupported admin opcode %#x while disabled\n",
			    cmd->opc);
			nvmf_send_generic_error(nc,
			    NVME_SC_COMMAND_SEQUENCE_ERROR);
			continue;
		}

		switch (cmd->opc) {
		case NVME_OPC_FABRICS_COMMANDS:
			handle_fabrics_command(nc,
			    (const struct nvmf_fabric_cmd *)cmd);
			break;
		case NVME_OPC_IDENTIFY:
			handle_identify_command(nc, cmd);
			break;
		case NVME_OPC_GET_LOG_PAGE:
			handle_get_log_page_command(nc, cmd);
			break;
		default:
			log_warnx("Unsupported admin opcode %#x", cmd->opc);
			nvmf_send_generic_error(nc, NVME_SC_INVALID_OPCODE);
			break;
		}
	}
}

discovery_controller::discovery_controller(freebsd::fd_up fd,
    struct nvmf_qpair *qp, const struct discovery_log &discovery_log) :
	qp(qp), discovery_log(discovery_log), s(std::move(fd))
{
	nvmf_init_discovery_controller_data(qp, &cdata);
	cap = nvmf_controller_cap(qp);
	vs = cdata.ver;
}

void
nvmf_discovery_portal::handle_connection(freebsd::fd_up fd,
    const char *host __unused, const struct sockaddr *client_sa)
{
	struct nvmf_qpair_params qparams;
	memset(&qparams, 0, sizeof(qparams));
	qparams.tcp.fd = fd;

	struct nvmf_capsule *nc = NULL;
	struct nvmf_fabric_connect_data data;
	nvmf_qpair_up qp(nvmf_accept(association(), &qparams, &nc, &data));
	if (!qp) {
		log_warnx("Failed to create NVMe discovery qpair: %s",
		    nvmf_association_error(association()));
		return;
	}
	nvmf_capsule_up nc_guard(nc);

	if (strncmp((char *)data.subnqn, NVMF_DISCOVERY_NQN,
	    sizeof(data.subnqn)) != 0) {
		log_warnx("Discovery NVMe qpair with invalid SubNQN: %.*s",
		    (int)sizeof(data.subnqn), data.subnqn);
		nvmf_connect_invalid_parameters(nc, true,
		    offsetof(struct nvmf_fabric_connect_data, subnqn));
		return;
	}

	/* Just use a controller ID of 1 for all discovery controllers. */
	int error = nvmf_finish_accept(nc, 1);
	if (error != 0) {
		log_warnc(error, "Failed to send NVMe CONNECT reponse");
		return;
	}
	nc_guard.reset();

	discovery_log discovery_log = build_discovery_log_page(portal_group(),
	    fd, client_sa, data);

	discovery_controller controller(std::move(fd), qp.get(), discovery_log);
	controller.handle_admin_commands();
}
