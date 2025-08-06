/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/time.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libiscsiutil.h>
#include <libnvmf.h>
#include <libutil.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_ioctl.h>

#include <memory>

#include "ctld.hh"
#include "nvmf.hh"

#define	DEFAULT_MAXH2CDATA	(256 * 1024)

struct nvmf_io_portal final : public nvmf_portal {
	nvmf_io_portal(struct portal_group *pg, const char *listen,
	    portal_protocol protocol, freebsd::addrinfo_up ai,
	    const struct nvmf_association_params &aparams,
	    nvmf_association_up na) :
		nvmf_portal(pg, listen, protocol, std::move(ai), aparams,
		    std::move(na)) {}

	void handle_connection(freebsd::fd_up fd, const char *host,
	    const struct sockaddr *client_sa) override;
};

struct nvmf_transport_group final : public portal_group {
	nvmf_transport_group(struct conf *conf, std::string_view name) :
		portal_group(conf, name) {}

	const char *keyword() const override
	{ return "transport-group"; }

	void allocate_tag() override;
	bool add_portal(const char *value, portal_protocol protocol)
	    override;
	void add_default_portals() override;
	bool set_filter(const char *str) override;

	virtual port_up create_port(struct target *target, auth_group_sp ag)
	    override;
	virtual port_up create_port(struct target *target, uint32_t ctl_port)
	    override;

private:
	struct nvmf_association_params init_aparams(portal_protocol protocol);

	static uint16_t last_port_id;
};

struct nvmf_port final : public portal_group_port {
	nvmf_port(struct target *target, struct portal_group *pg,
	    auth_group_sp ag) :
		portal_group_port(target, pg, ag) {}
	nvmf_port(struct target *target, struct portal_group *pg,
	    uint32_t ctl_port) :
		portal_group_port(target, pg, ctl_port) {}

	bool kernel_create_port() override;
	bool kernel_remove_port() override;

private:
	static bool modules_loaded;
	static void load_kernel_modules();
};

struct nvmf_controller final : public target {
	nvmf_controller(struct conf *conf, std::string_view name) :
		target(conf, "controller", name) {}

	bool add_host_nqn(std::string_view name) override;
	bool add_host_address(const char *addr) override;
	bool add_namespace(u_int id, const char *lun_name) override;
	bool add_portal_group(const char *pg_name, const char *ag_name)
	    override;
	struct lun *start_namespace(u_int id) override;

protected:
	struct portal_group *default_portal_group() override;
};

uint16_t nvmf_transport_group::last_port_id = 0;
bool nvmf_port::modules_loaded = false;

static bool need_tcp_transport = false;

static bool
parse_bool(const nvlist_t *nvl, const char *key, bool def)
{
	const char *value;

	if (!nvlist_exists_string(nvl, key))
		return def;

	value = nvlist_get_string(nvl, key);
	if (strcasecmp(value, "true") == 0 ||
	    strcasecmp(value, "1") == 0)
		return true;
	if (strcasecmp(value, "false") == 0 ||
	    strcasecmp(value, "0") == 0)
		return false;

	log_warnx("Invalid value \"%s\" for boolean option %s", value, key);
	return def;
}

static uint64_t
parse_number(const nvlist_t *nvl, const char *key, uint64_t def, uint64_t minv,
    uint64_t maxv)
{
	const char *value;
	int64_t val;

	if (!nvlist_exists_string(nvl, key))
		return def;

	value = nvlist_get_string(nvl, key);
	if (expand_number(value, &val) == 0 && val >= 0 &&
	    (uint64_t)val >= minv && (uint64_t)val <= maxv)
		return (uint64_t)val;

	log_warnx("Invalid value \"%s\" for numeric option %s", value, key);
	return def;
}

struct nvmf_association_params
nvmf_transport_group::init_aparams(portal_protocol protocol)
{
	struct nvmf_association_params params;
	memset(&params, 0, sizeof(params));

	/* Options shared between discovery and I/O associations. */
	const nvlist_t *nvl = pg_options.get();
	params.tcp.header_digests = parse_bool(nvl, "HDGST", false);
	params.tcp.data_digests = parse_bool(nvl, "DDGST", false);
	uint64_t value = parse_number(nvl, "MAXH2CDATA", DEFAULT_MAXH2CDATA,
	    4096, UINT32_MAX);
	if (value % 4 != 0) {
		log_warnx("Invalid value \"%ju\" for option MAXH2CDATA",
		    (uintmax_t)value);
		value = DEFAULT_MAXH2CDATA;
	}
	params.tcp.maxh2cdata = value;

	switch (protocol) {
	case portal_protocol::NVME_TCP:
		params.sq_flow_control = parse_bool(nvl, "SQFC", false);
		params.dynamic_controller_model = true;
		params.max_admin_qsize = parse_number(nvl, "max_admin_qsize",
		    NVME_MAX_ADMIN_ENTRIES, NVME_MIN_ADMIN_ENTRIES,
		    NVME_MAX_ADMIN_ENTRIES);
		params.max_io_qsize = parse_number(nvl, "max_io_qsize",
		    NVME_MAX_IO_ENTRIES, NVME_MIN_IO_ENTRIES,
		    NVME_MAX_IO_ENTRIES);
		params.tcp.pda = 0;
		break;
	case portal_protocol::NVME_DISCOVERY_TCP:
		params.sq_flow_control = false;
		params.dynamic_controller_model = true;
		params.max_admin_qsize = NVME_MAX_ADMIN_ENTRIES;
		params.tcp.pda = 0;
		break;
	default:
		__assert_unreachable();
	}

	return params;
}

portal_group_up
nvmf_make_transport_group(struct conf *conf, std::string_view name)
{
	return std::make_unique<nvmf_transport_group>(conf, name);
}

target_up
nvmf_make_controller(struct conf *conf, std::string_view name)
{
	return std::make_unique<nvmf_controller>(conf, name);
}

void
nvmf_transport_group::allocate_tag()
{
	set_tag(++last_port_id);
}

bool
nvmf_transport_group::add_portal(const char *value, portal_protocol protocol)
{
	freebsd::addrinfo_up ai;
	enum nvmf_trtype trtype;

	switch (protocol) {
	case portal_protocol::NVME_TCP:
		trtype = NVMF_TRTYPE_TCP;
		ai = parse_addr_port(value, "4420");
		break;
	case portal_protocol::NVME_DISCOVERY_TCP:
		trtype = NVMF_TRTYPE_TCP;
		ai = parse_addr_port(value, "8009");
		break;
	default:
		log_warnx("unsupported transport protocol for %s", value);
		return false;
	}

	if (!ai) {
		log_warnx("invalid listen address %s", value);
		return false;
	}

	struct nvmf_association_params aparams = init_aparams(protocol);
	nvmf_association_up association(nvmf_allocate_association(trtype, true,
	    &aparams));
	if (!association) {
		log_warn("Failed to create NVMe controller association");
		return false;
	}

	/*
	 * XXX: getaddrinfo(3) may return multiple addresses; we should turn
	 *	those into multiple portals.
	 */

	portal_up portal;
	if (protocol == portal_protocol::NVME_DISCOVERY_TCP) {
		portal = std::make_unique<nvmf_discovery_portal>(this, value,
		    protocol, std::move(ai), aparams, std::move(association));
	} else {
		portal = std::make_unique<nvmf_io_portal>(this, value,
		    protocol, std::move(ai), aparams, std::move(association));
		need_tcp_transport = true;
	}

	pg_portals.emplace_back(std::move(portal));
	return true;
}

void
nvmf_transport_group::add_default_portals()
{
	add_portal("0.0.0.0", portal_protocol::NVME_DISCOVERY_TCP);
	add_portal("[::]", portal_protocol::NVME_DISCOVERY_TCP);
	add_portal("0.0.0.0", portal_protocol::NVME_TCP);
	add_portal("[::]", portal_protocol::NVME_TCP);
}

bool
nvmf_transport_group::set_filter(const char *str)
{
	enum discovery_filter filter;

	if (strcmp(str, "none") == 0) {
		filter = discovery_filter::NONE;
	} else if (strcmp(str, "address") == 0) {
		filter = discovery_filter::PORTAL;
	} else if (strcmp(str, "address-name") == 0) {
		filter = discovery_filter::PORTAL_NAME;
	} else {
		log_warnx("invalid discovery-filter \"%s\" for transport-group "
		    "\"%s\"; valid values are \"none\", \"address\", "
		    "and \"address-name\"",
		    str, name());
		return false;
	}

	if (pg_discovery_filter != discovery_filter::UNKNOWN &&
	    pg_discovery_filter != filter) {
		log_warnx("cannot set discovery-filter to \"%s\" for "
		    "transport-group \"%s\"; already has a different "
		    "value", str, name());
		return false;
	}

	pg_discovery_filter = filter;
	return true;
}

port_up
nvmf_transport_group::create_port(struct target *target, auth_group_sp ag)
{
	return std::make_unique<nvmf_port>(target, this, ag);
}

port_up
nvmf_transport_group::create_port(struct target *target, uint32_t ctl_port)
{
	return std::make_unique<nvmf_port>(target, this, ctl_port);
}

void
nvmf_port::load_kernel_modules()
{
	int saved_errno;

	if (modules_loaded)
		return;

	saved_errno = errno;
	if (modfind("nvmft") == -1 && kldload("nvmft") == -1)
		log_warn("couldn't load nvmft");

	if (need_tcp_transport) {
		if (modfind("nvmf/tcp") == -1 && kldload("nvmf_tcp") == -1)
			log_warn("couldn't load nvmf_tcp");
	}

	errno = saved_errno;
	modules_loaded = true;
}

bool
nvmf_port::kernel_create_port()
{
	struct portal_group *pg = p_portal_group;
	struct target *targ = p_target;

	load_kernel_modules();

	freebsd::nvlist_up nvl = pg->options();
	nvlist_add_string(nvl.get(), "subnqn", targ->name());
	nvlist_add_string(nvl.get(), "ctld_transport_group_name",
	    pg->name());
	nvlist_add_stringf(nvl.get(), "portid", "%u", pg->tag());
	if (!nvlist_exists_string(nvl.get(), "max_io_qsize"))
		nvlist_add_stringf(nvl.get(), "max_io_qsize", "%u",
		    NVME_MAX_IO_ENTRIES);

	return ctl_create_port("nvmf", nvl.get(), &p_ctl_port);
}

bool
nvmf_port::kernel_remove_port()
{
	freebsd::nvlist_up nvl(nvlist_create(0));
	nvlist_add_string(nvl.get(), "subnqn", p_target->name());

	return ctl_remove_port("nvmf", nvl.get());
}

bool
nvmf_controller::add_host_nqn(std::string_view name)
{
	if (!use_private_auth("host-nqn"))
		return false;
	return t_auth_group->add_host_nqn(name);
}

bool
nvmf_controller::add_host_address(const char *addr)
{
	if (!use_private_auth("host-address"))
		return false;
	return t_auth_group->add_host_address(addr);
}

bool
nvmf_controller::add_namespace(u_int id, const char *lun_name)
{
	if (id == 0) {
		log_warnx("namespace ID cannot be 0 for %s", label());
		return false;
	}

	std::string lun_label = "namespace ID " + std::to_string(id - 1);
	return target::add_lun(id, lun_label.c_str(), lun_name);
}

bool
nvmf_controller::add_portal_group(const char *pg_name, const char *ag_name)
{
	struct portal_group *pg;
	auth_group_sp ag;

	pg = t_conf->find_transport_group(pg_name);
	if (pg == NULL) {
		log_warnx("unknown transport-group \"%s\" for %s", pg_name,
		    label());
		return false;
	}

	if (ag_name != NULL) {
		ag = t_conf->find_auth_group(ag_name);
		if (ag == NULL) {
			log_warnx("unknown auth-group \"%s\" for %s", ag_name,
			    label());
			return false;
		}
	}

	if (!t_conf->add_port(this, pg, std::move(ag))) {
		log_warnx("can't link transport-group \"%s\" to %s", pg_name,
		    label());
		return false;
	}
	return true;
}

struct lun *
nvmf_controller::start_namespace(u_int id)
{
	if (id == 0) {
		log_warnx("namespace ID cannot be 0 for %s", label());
		return nullptr;
	}

	std::string lun_label = "namespace ID " + std::to_string(id - 1);
	std::string lun_name = freebsd::stringf("%s,nsid,%u", name(), id);
	return target::start_lun(id, lun_label.c_str(), lun_name.c_str());
}

struct portal_group *
nvmf_controller::default_portal_group()
{
	return t_conf->find_transport_group("default");
}

void
nvmf_io_portal::handle_connection(freebsd::fd_up fd, const char *host __unused,
    const struct sockaddr *client_sa __unused)
{
	struct nvmf_qpair_params qparams;
	memset(&qparams, 0, sizeof(qparams));
	qparams.tcp.fd = fd;

	struct nvmf_capsule *nc = NULL;
	struct nvmf_fabric_connect_data data;
	nvmf_qpair_up qp(nvmf_accept(association(), &qparams, &nc, &data));
	if (!qp) {
		log_warnx("Failed to create NVMe I/O qpair: %s",
		    nvmf_association_error(association()));
		return;
	}
	nvmf_capsule_up nc_guard(nc);
	const struct nvmf_fabric_connect_cmd *cmd =
	    (const struct nvmf_fabric_connect_cmd *)nvmf_capsule_sqe(nc);

	struct ctl_nvmf req;
	memset(&req, 0, sizeof(req));
	req.type = CTL_NVMF_HANDOFF;
	int error = nvmf_handoff_controller_qpair(qp.get(), cmd, &data,
	    &req.data.handoff);
	if (error != 0) {
		log_warnc(error,
		    "Failed to prepare NVMe I/O qpair for handoff");
		return;
	}

	if (ioctl(ctl_fd, CTL_NVMF, &req) != 0)
		log_warn("ioctl(CTL_NVMF/CTL_NVMF_HANDOFF)");
	if (req.status == CTL_NVMF_ERROR)
		log_warnx("Failed to handoff NVMF connection: %s",
		    req.error_str);
	else if (req.status != CTL_NVMF_OK)
		log_warnx("Failed to handoff NVMF connection with status %d",
		    req.status);
}
