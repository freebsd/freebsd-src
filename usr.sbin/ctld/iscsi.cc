/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003, 2004 Silicon Graphics International Corp.
 * Copyright (c) 1997-2007 Kenneth D. Merry
 * Copyright (c) 2012 The FreeBSD Foundation
 * Copyright (c) 2017 Jakub Wojciech Klama <jceel@FreeBSD.org>
 * All rights reserved.
 * Copyright (c) 2025 Chelsio Communications, Inc.
 *
 * Portions of this software were developed by Edward Tomasz Napierala
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 */

#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/time.h>
#include <assert.h>
#include <libiscsiutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_ioctl.h>

#include "ctld.hh"
#include "iscsi.hh"

#define	SOCKBUF_SIZE			1048576

struct iscsi_portal final : public portal {
	iscsi_portal(struct portal_group *pg, const char *listen,
	    portal_protocol protocol, freebsd::addrinfo_up ai) :
		portal(pg, listen, protocol, std::move(ai)) {}

	bool init_socket_options(int s) override;
	void handle_connection(freebsd::fd_up fd, const char *host,
	    const struct sockaddr *client_sa) override;
};

struct iscsi_portal_group final : public portal_group {
	iscsi_portal_group(struct conf *conf, std::string_view name) :
		portal_group(conf, name) {}

	const char *keyword() const override
	{ return "portal-group"; }

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
	static uint16_t last_portal_group_tag;
};

struct iscsi_port final : public portal_group_port {
	iscsi_port(struct target *target, struct portal_group *pg,
	    auth_group_sp ag) :
		portal_group_port(target, pg, ag) {}
	iscsi_port(struct target *target, struct portal_group *pg,
	    uint32_t ctl_port) :
		portal_group_port(target, pg, ctl_port) {}

	bool kernel_create_port() override;
	bool kernel_remove_port() override;

private:
	static bool module_loaded;
	static void load_kernel_module();
};

struct iscsi_target final : public target {
	iscsi_target(struct conf *conf, std::string_view name) :
		target(conf, "target", name) {}

	bool add_initiator_name(std::string_view name) override;
	bool add_initiator_portal(const char *addr) override;
	bool add_lun(u_int id, const char *lun_name) override;
	bool add_portal_group(const char *pg_name, const char *ag_name)
	    override;
	struct lun *start_lun(u_int id) override;

protected:
	struct portal_group *default_portal_group() override;
};

#ifdef ICL_KERNEL_PROXY
static void	pdu_receive_proxy(struct pdu *pdu);
static void	pdu_send_proxy(struct pdu *pdu);
#endif /* ICL_KERNEL_PROXY */
static void	pdu_fail(const struct connection *conn, const char *reason);

uint16_t iscsi_portal_group::last_portal_group_tag = 0xff;
bool	iscsi_port::module_loaded = false;

static struct connection_ops conn_ops = {
	.timed_out = timed_out,
#ifdef ICL_KERNEL_PROXY
	.pdu_receive_proxy = pdu_receive_proxy,
	.pdu_send_proxy = pdu_send_proxy,
#else
	.pdu_receive_proxy = nullptr,
	.pdu_send_proxy = nullptr,
#endif
	.fail = pdu_fail,
};

portal_group_up
iscsi_make_portal_group(struct conf *conf, std::string_view name)
{
	return std::make_unique<iscsi_portal_group>(conf, name);
}

target_up
iscsi_make_target(struct conf *conf, std::string_view name)
{
	return std::make_unique<iscsi_target>(conf, name);
}

void
iscsi_portal_group::allocate_tag()
{
	set_tag(++last_portal_group_tag);
}

bool
iscsi_portal_group::add_portal(const char *value, portal_protocol protocol)
{
	switch (protocol) {
	case portal_protocol::ISCSI:
	case portal_protocol::ISER:
		break;
	default:
		log_warnx("unsupported portal protocol for %s", value);
		return (false);
	}

	freebsd::addrinfo_up ai = parse_addr_port(value, "3260");
	if (!ai) {
		log_warnx("invalid listen address %s", value);
		return (false);
	}

	/*
	 * XXX: getaddrinfo(3) may return multiple addresses; we should turn
	 *	those into multiple portals.
	 */

	pg_portals.emplace_back(std::make_unique<iscsi_portal>(this, value,
	    protocol, std::move(ai)));
	return (true);
}

void
iscsi_portal_group::add_default_portals()
{
	add_portal("0.0.0.0", portal_protocol::ISCSI);
	add_portal("[::]", portal_protocol::ISCSI);
}

bool
iscsi_portal_group::set_filter(const char *str)
{
	enum discovery_filter filter;

	if (strcmp(str, "none") == 0) {
		filter = discovery_filter::NONE;
	} else if (strcmp(str, "portal") == 0) {
		filter = discovery_filter::PORTAL;
	} else if (strcmp(str, "portal-name") == 0) {
		filter = discovery_filter::PORTAL_NAME;
	} else if (strcmp(str, "portal-name-auth") == 0) {
		filter = discovery_filter::PORTAL_NAME_AUTH;
	} else {
		log_warnx("invalid discovery-filter \"%s\" for portal-group "
		    "\"%s\"; valid values are \"none\", \"portal\", "
		    "\"portal-name\", and \"portal-name-auth\"",
		    str, name());
		return (false);
	}

	if (pg_discovery_filter != discovery_filter::UNKNOWN &&
	    pg_discovery_filter != filter) {
		log_warnx("cannot set discovery-filter to \"%s\" for "
		    "portal-group \"%s\"; already has a different "
		    "value", str, name());
		return (false);
	}

	pg_discovery_filter = filter;
	return (true);
}

port_up
iscsi_portal_group::create_port(struct target *target, auth_group_sp ag)
{
	return std::make_unique<iscsi_port>(target, this, ag);
}

port_up
iscsi_portal_group::create_port(struct target *target, uint32_t ctl_port)
{
	return std::make_unique<iscsi_port>(target, this, ctl_port);
}

void
iscsi_port::load_kernel_module()
{
	int saved_errno;

	if (module_loaded)
		return;

	saved_errno = errno;
	if (modfind("cfiscsi") == -1 && kldload("cfiscsi") == -1)
		log_warn("couldn't load cfiscsi");
	errno = saved_errno;
	module_loaded = true;
}

bool
iscsi_port::kernel_create_port()
{
	struct portal_group *pg = p_portal_group;
	struct target *targ = p_target;

	load_kernel_module();

	freebsd::nvlist_up nvl = pg->options();
	nvlist_add_string(nvl.get(), "cfiscsi_target", targ->name());
	nvlist_add_string(nvl.get(), "ctld_portal_group_name", pg->name());
	nvlist_add_stringf(nvl.get(), "cfiscsi_portal_group_tag", "%u",
	    pg->tag());

	if (targ->has_alias()) {
		nvlist_add_string(nvl.get(), "cfiscsi_target_alias",
		    targ->alias());
	}

	return (ctl_create_port("iscsi", nvl.get(), &p_ctl_port));
}

bool
iscsi_port::kernel_remove_port()
{
	freebsd::nvlist_up nvl(nvlist_create(0));
	nvlist_add_string(nvl.get(), "cfiscsi_target", p_target->name());
	nvlist_add_stringf(nvl.get(), "cfiscsi_portal_group_tag", "%u",
	    p_portal_group->tag());

	return (ctl_remove_port("iscsi", nvl.get()));
}

bool
iscsi_portal::init_socket_options(int s)
{
	int sockbuf;

	sockbuf = SOCKBUF_SIZE;
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sockbuf,
	    sizeof(sockbuf)) == -1) {
		log_warn("setsockopt(SO_RCVBUF) failed for %s", listen());
		return (false);
	}
	sockbuf = SOCKBUF_SIZE;
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sockbuf,
	    sizeof(sockbuf)) == -1) {
		log_warn("setsockopt(SO_SNDBUF) failed for %s", listen());
		return (false);
	}
	return (true);
}

bool
iscsi_target::add_initiator_name(std::string_view name)
{
	if (!use_private_auth("initiator-name"))
		return (false);
	return (t_auth_group->add_initiator_name(name));
}

bool
iscsi_target::add_initiator_portal(const char *addr)
{
	if (!use_private_auth("initiator-portal"))
		return (false);
	return (t_auth_group->add_initiator_portal(addr));
}

bool
iscsi_target::add_lun(u_int id, const char *lun_name)
{
	std::string lun_label = "LUN " + std::to_string(id);
	return target::add_lun(id, lun_label.c_str(), lun_name);
}

bool
iscsi_target::add_portal_group(const char *pg_name, const char *ag_name)
{
	struct portal_group *pg;
	auth_group_sp ag;

	pg = t_conf->find_portal_group(pg_name);
	if (pg == NULL) {
		log_warnx("unknown portal-group \"%s\" for %s", pg_name,
		    label());
		return (false);
	}

	if (ag_name != NULL) {
		ag = t_conf->find_auth_group(ag_name);
		if (ag == NULL) {
			log_warnx("unknown auth-group \"%s\" for %s", ag_name,
			    label());
			return (false);
		}
	}

	if (!t_conf->add_port(this, pg, std::move(ag))) {
		log_warnx("can't link portal-group \"%s\" to %s", pg_name,
		    label());
		return (false);
	}
	return (true);
}

struct lun *
iscsi_target::start_lun(u_int id)
{
	std::string lun_label = "LUN " + std::to_string(id);
	std::string lun_name = freebsd::stringf("%s,lun,%u", name(), id);
	return target::start_lun(id, lun_label.c_str(), lun_name.c_str());
}

struct portal_group *
iscsi_target::default_portal_group()
{
	return t_conf->find_portal_group("default");
}

#ifdef ICL_KERNEL_PROXY

static void
pdu_receive_proxy(struct pdu *pdu)
{
	struct connection *conn;
	size_t len;

	assert(proxy_mode);
	conn = pdu->pdu_connection;

	kernel_receive(pdu);

	len = pdu_ahs_length(pdu);
	if (len > 0)
		log_errx(1, "protocol error: non-empty AHS");

	len = pdu_data_segment_length(pdu);
	assert(len <= (size_t)conn->conn_max_recv_data_segment_length);
	pdu->pdu_data_len = len;
}

static void
pdu_send_proxy(struct pdu *pdu)
{

	assert(proxy_mode);

	pdu_set_data_segment_length(pdu, pdu->pdu_data_len);
	kernel_send(pdu);
}

#endif /* ICL_KERNEL_PROXY */

static void
pdu_fail(const struct connection *conn __unused, const char *reason __unused)
{
}

iscsi_connection::iscsi_connection(struct portal *portal, freebsd::fd_up fd,
    const char *host, const struct sockaddr *client_sa) :
	conn_portal(portal), conn_fd(std::move(fd)), conn_initiator_addr(host),
	conn_initiator_sa(client_sa)
{
	connection_init(&conn, &conn_ops, proxy_mode);
	conn.conn_socket = conn_fd;
}

iscsi_connection::~iscsi_connection()
{
	chap_delete(conn_chap);
}

void
iscsi_connection::kernel_handoff()
{
	struct portal_group *pg = conn_portal->portal_group();
	struct ctl_iscsi req;

	bzero(&req, sizeof(req));

	req.type = CTL_ISCSI_HANDOFF;
	strlcpy(req.data.handoff.initiator_name, conn_initiator_name.c_str(),
	    sizeof(req.data.handoff.initiator_name));
	strlcpy(req.data.handoff.initiator_addr, conn_initiator_addr.c_str(),
	    sizeof(req.data.handoff.initiator_addr));
	if (!conn_initiator_alias.empty()) {
		strlcpy(req.data.handoff.initiator_alias,
		    conn_initiator_alias.c_str(),
		    sizeof(req.data.handoff.initiator_alias));
	}
	memcpy(req.data.handoff.initiator_isid, conn_initiator_isid,
	    sizeof(req.data.handoff.initiator_isid));
	strlcpy(req.data.handoff.target_name, conn_target->name(),
	    sizeof(req.data.handoff.target_name));
	strlcpy(req.data.handoff.offload, pg->offload(),
	    sizeof(req.data.handoff.offload));
#ifdef ICL_KERNEL_PROXY
	if (proxy_mode)
		req.data.handoff.connection_id = conn.conn_socket;
	else
		req.data.handoff.socket = conn.conn_socket;
#else
	req.data.handoff.socket = conn.conn_socket;
#endif
	req.data.handoff.portal_group_tag = pg->tag();
	if (conn.conn_header_digest == CONN_DIGEST_CRC32C)
		req.data.handoff.header_digest = CTL_ISCSI_DIGEST_CRC32C;
	if (conn.conn_data_digest == CONN_DIGEST_CRC32C)
		req.data.handoff.data_digest = CTL_ISCSI_DIGEST_CRC32C;
	req.data.handoff.cmdsn = conn.conn_cmdsn;
	req.data.handoff.statsn = conn.conn_statsn;
	req.data.handoff.max_recv_data_segment_length =
	    conn.conn_max_recv_data_segment_length;
	req.data.handoff.max_send_data_segment_length =
	    conn.conn_max_send_data_segment_length;
	req.data.handoff.max_burst_length = conn.conn_max_burst_length;
	req.data.handoff.first_burst_length = conn.conn_first_burst_length;
	req.data.handoff.immediate_data = conn.conn_immediate_data;

	if (ioctl(ctl_fd, CTL_ISCSI, &req) == -1) {
		log_err(1, "error issuing CTL_ISCSI ioctl; "
		    "dropping connection");
	}

	if (req.status != CTL_ISCSI_OK) {
		log_errx(1, "error returned from CTL iSCSI handoff request: "
		    "%s; dropping connection", req.error_str);
	}
}

void
iscsi_connection::handle()
{
	login();
	if (conn_session_type == CONN_SESSION_TYPE_NORMAL) {
		kernel_handoff();
		log_debugx("connection handed off to the kernel");
	} else {
		assert(conn_session_type == CONN_SESSION_TYPE_DISCOVERY);
		discovery();
	}
}

void
iscsi_portal::handle_connection(freebsd::fd_up fd, const char *host,
    const struct sockaddr *client_sa)
{
	struct conf *conf = portal_group()->conf();

	iscsi_connection conn(this, std::move(fd), host, client_sa);
	start_timer(conf->timeout(), true);
	kernel_capsicate();
	conn.handle();
}
