/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 The FreeBSD Foundation
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/nv.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libnvmf.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cam/scsi/scsi_all.h>

#include <algorithm>
#include <libutil++.hh>

#include "conf.h"
#include "ctld.hh"
#include "isns.hh"

bool proxy_mode = false;

static volatile bool sighup_received = false;
static volatile bool sigterm_received = false;
static volatile bool sigalrm_received = false;

static int kqfd;
static int nchildren = 0;

uint32_t conf::global_genctr;

static void
usage(void)
{

	fprintf(stderr, "usage: ctld [-d][-u][-f config-file]\n");
	fprintf(stderr, "       ctld -t [-u][-f config-file]\n");
	exit(1);
}

conf::conf()
{
	conf_genctr = global_genctr++;
}

void
conf::set_debug(int debug)
{
	conf_debug = debug;
}

void
conf::set_isns_period(int period)
{
	conf_isns_period = period;
}

void
conf::set_isns_timeout(int timeout)
{
	conf_isns_timeout = timeout;
}

void
conf::set_maxproc(int maxproc)
{
	conf_maxproc = maxproc;
}

void
conf::set_timeout(int timeout)
{
	conf_timeout = timeout;
}

bool
conf::set_pidfile_path(std::string_view path)
{
	if (!conf_pidfile_path.empty()) {
		log_warnx("pidfile specified more than once");
		return (false);
	}
	conf_pidfile_path = path;
	return (true);
}

void
conf::open_pidfile()
{
	const char *path;
	pid_t otherpid;

	assert(!conf_pidfile_path.empty());
	path = conf_pidfile_path.c_str();
	log_debugx("opening pidfile %s", path);
	conf_pidfile = pidfile_open(path, 0600, &otherpid);
	if (!conf_pidfile) {
		if (errno == EEXIST)
			log_errx(1, "daemon already running, pid: %jd.",
			    (intmax_t)otherpid);
		log_err(1, "cannot open or create pidfile \"%s\"", path);
	}
}

void
conf::write_pidfile()
{
	conf_pidfile.write();
}

void
conf::close_pidfile()
{
	conf_pidfile.close();
}

#ifdef ICL_KERNEL_PROXY
int
conf::add_proxy_portal(portal *portal)
{
	conf_proxy_portals.push_back(portal);
	return (conf_proxy_portals.size() - 1);
}

portal *
conf::proxy_portal(int id)
{
	if (id >= conf_proxy_portals.size())
		return (nullptr);
	return (conf_proxy_portals[id]);
}
#endif

bool
auth_group::set_type(const char *str)
{
	auth_type type;

	if (strcmp(str, "none") == 0) {
		type = auth_type::NO_AUTHENTICATION;
	} else if (strcmp(str, "deny") == 0) {
		type = auth_type::DENY;
	} else if (strcmp(str, "chap") == 0) {
		type = auth_type::CHAP;
	} else if (strcmp(str, "chap-mutual") == 0) {
		type = auth_type::CHAP_MUTUAL;
	} else {
		log_warnx("invalid auth-type \"%s\" for %s", str, label());
		return (false);
	}

	if (ag_type != auth_type::UNKNOWN && ag_type != type) {
		log_warnx("cannot set auth-type to \"%s\" for %s; "
		    "already has a different type", str, label());
		return (false);
	}

	ag_type = type;

	return (true);
}

void
auth_group::set_type(auth_type type)
{
	assert(ag_type == auth_type::UNKNOWN);

	ag_type = type;
}

const struct auth *
auth_group::find_auth(std::string_view user) const
{
	auto it = ag_auths.find(std::string(user));
	if (it == ag_auths.end())
		return (nullptr);

	return (&it->second);
}

void
auth_group::check_secret_length(const char *user, const char *secret,
    const char *secret_type)
{
	size_t len;

	len = strlen(secret);
	assert(len != 0);
	if (len > 16) {
		log_warnx("%s for user \"%s\", %s, is too long; it should be "
		    "at most 16 characters long", secret_type, user, label());
	}
	if (len < 12) {
		log_warnx("%s for user \"%s\", %s, is too short; it should be "
		    "at least 12 characters long", secret_type, user, label());
	}
}

bool
auth_group::add_chap(const char *user, const char *secret)
{
	if (ag_type == auth_type::UNKNOWN)
		ag_type = auth_type::CHAP;
	if (ag_type != auth_type::CHAP) {
		log_warnx("cannot mix \"chap\" authentication with "
		    "other types for %s", label());
		return (false);
	}

	check_secret_length(user, secret, "secret");

	const auto &pair = ag_auths.try_emplace(user, secret);
	if (!pair.second) {
		log_warnx("duplicate credentials for user \"%s\" for %s",
		    user, label());
		return (false);
	}

	return (true);
}

bool
auth_group::add_chap_mutual(const char *user, const char *secret,
    const char *user2, const char *secret2)
{
	if (ag_type == auth_type::UNKNOWN)
		ag_type = auth_type::CHAP_MUTUAL;
	if (ag_type != auth_type::CHAP_MUTUAL) {
		log_warnx("cannot mix \"chap-mutual\" authentication "
		    "with other types for %s", label());
		return (false);
	}

	check_secret_length(user, secret, "secret");
	check_secret_length(user, secret2, "mutual secret");

	const auto &pair = ag_auths.try_emplace(user, secret, user2, secret2);
	if (!pair.second) {
		log_warnx("duplicate credentials for user \"%s\" for %s",
		    user, label());
		return (false);
	}

	return (true);
}

bool
auth_group::add_host_nqn(std::string_view nqn)
{
	/* Silently ignore duplicates. */
	ag_host_names.emplace(nqn);
	return (true);
}

bool
auth_group::host_permitted(std::string_view nqn) const
{
	if (ag_host_names.empty())
		return (true);

	return (ag_host_names.count(std::string(nqn)) != 0);
}

bool
auth_group::add_initiator_name(std::string_view name)
{
	/* Silently ignore duplicates. */
	ag_initiator_names.emplace(name);
	return (true);
}

bool
auth_group::initiator_permitted(std::string_view initiator_name) const
{
	if (ag_initiator_names.empty())
		return (true);

	return (ag_initiator_names.count(std::string(initiator_name)) != 0);
}

bool
auth_portal::parse(const char *portal)
{
	std::string net(portal);
	std::string mask;

	/* Split into 'net' (address) and 'mask'. */
	size_t pos = net.find('/');
	if (pos != net.npos) {
		mask = net.substr(pos + 1);
		if (mask.empty())
			return false;
		net.resize(pos);
	}
	if (net.empty())
		return false;

	/*
	 * If 'net' starts with a '[', ensure it ends with a ']' and
	 * force interpreting the address as IPv6.
	 */
	bool brackets = net[0] == '[';
	if (brackets) {
		net.erase(0, 1);

		size_t len = net.length();
		if (len < 2)
			return false;
		if (net[len - 1] != ']')
			return false;
		net.resize(len - 1);
	}

	/* Parse address from 'net' and set default mask. */
	if (brackets || net.find(':') != net.npos) {
		struct sockaddr_in6 *sin6 =
		    (struct sockaddr_in6 *)&ap_sa;

		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, net.c_str(), &sin6->sin6_addr) <= 0)
			return false;
		ap_mask = sizeof(sin6->sin6_addr) * 8;
	} else {
		struct sockaddr_in *sin =
		    (struct sockaddr_in *)&ap_sa;

		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		if (inet_pton(AF_INET, net.c_str(), &sin->sin_addr) <= 0)
			return false;
		ap_mask = sizeof(sin->sin_addr) * 8;
	}

	/* Parse explicit mask if present. */
	if (!mask.empty()) {
		char *tmp;
		long m = strtol(mask.c_str(), &tmp, 0);
		if (m < 0 || m > ap_mask || tmp[0] != 0)
			return false;
		ap_mask = m;
	}

	return true;
}

bool
auth_group::add_host_address(const char *address)
{
	auth_portal ap;
	if (!ap.parse(address)) {
		log_warnx("invalid controller address \"%s\" for %s", address,
		    label());
		return (false);
	}

	ag_host_addresses.emplace_back(ap);
	return (true);
}

bool
auth_group::add_initiator_portal(const char *portal)
{
	auth_portal ap;
	if (!ap.parse(portal)) {
		log_warnx("invalid initiator portal \"%s\" for %s", portal,
		    label());
		return (false);
	}

	ag_initiator_portals.emplace_back(ap);
	return (true);
}

bool
auth_portal::matches(const struct sockaddr *sa) const
{
	const uint8_t *a, *b;
	int i;

	if (ap_sa.ss_family != sa->sa_family)
		return (false);

	if (sa->sa_family == AF_INET) {
		a = (const uint8_t *)
		    &((const struct sockaddr_in *)sa)->sin_addr;
		b = (const uint8_t *)
		    &((const struct sockaddr_in *)&ap_sa)->sin_addr;
	} else {
		a = (const uint8_t *)
		    &((const struct sockaddr_in6 *)sa)->sin6_addr;
		b = (const uint8_t *)
		    &((const struct sockaddr_in6 *)&ap_sa)->sin6_addr;
	}
	for (i = 0; i < ap_mask / 8; i++) {
		if (a[i] != b[i])
			return (false);
	}
	if ((ap_mask % 8) != 0) {
		uint8_t bmask = 0xff << (8 - (ap_mask % 8));
		if ((a[i] & bmask) != (b[i] & bmask))
			return (false);
	}
	return (true);
}

bool
auth_group::host_permitted(const struct sockaddr *sa) const
{
	if (ag_host_addresses.empty())
		return (true);

	for (const auth_portal &ap : ag_host_addresses)
		if (ap.matches(sa))
			return (true);
	return (false);
}

bool
auth_group::initiator_permitted(const struct sockaddr *sa) const
{
	if (ag_initiator_portals.empty())
		return (true);

	for (const auth_portal &ap : ag_initiator_portals)
		if (ap.matches(sa))
			return (true);
	return (false);
}

struct auth_group *
conf::add_auth_group(const char *name)
{
	const auto &pair = conf_auth_groups.try_emplace(name,
	    std::make_shared<auth_group>(freebsd::stringf("auth-group \"%s\"",
	    name)));
	if (!pair.second) {
		log_warnx("duplicated auth-group \"%s\"", name);
		return (NULL);
	}

	return (pair.first->second.get());
}

/*
 * Make it possible to redefine the default auth-group, but only once.
 */
struct auth_group *
conf::define_default_auth_group()
{
	if (conf_default_ag_defined) {
		log_warnx("duplicated auth-group \"default\"");
		return (nullptr);
	}

	conf_default_ag_defined = true;
	return (find_auth_group("default").get());
}

auth_group_sp
conf::find_auth_group(std::string_view name)
{
	auto it = conf_auth_groups.find(std::string(name));
	if (it == conf_auth_groups.end())
		return {};

	return (it->second);
}

portal_group::portal_group(struct conf *conf, std::string_view name) :
    pg_conf(conf), pg_options(nvlist_create(0)), pg_name(name)
{
}

struct portal_group *
conf::add_portal_group(const char *name)
{
	auto pair = conf_portal_groups.try_emplace(name,
	    iscsi_make_portal_group(this, name));
	if (!pair.second) {
		log_warnx("duplicated portal-group \"%s\"", name);
		return (nullptr);
	}

	return (pair.first->second.get());
}

/*
 * Make it possible to redefine the default portal-group, but only
 * once.
 */
struct portal_group *
conf::define_default_portal_group()
{
	if (conf_default_pg_defined) {
		log_warnx("duplicated portal-group \"default\"");
		return (nullptr);
	}

	conf_default_pg_defined = true;
	return (find_portal_group("default"));
}

struct portal_group *
conf::find_portal_group(std::string_view name)
{
	auto it = conf_portal_groups.find(std::string(name));
	if (it == conf_portal_groups.end())
		return (nullptr);

	return (it->second.get());
}

struct portal_group *
conf::add_transport_group(const char *name)
{
	auto pair = conf_transport_groups.try_emplace(name,
	    nvmf_make_transport_group(this, name));
	if (!pair.second) {
		log_warnx("duplicated transport-group \"%s\"", name);
		return (nullptr);
	}

	return (pair.first->second.get());
}

/*
 * Make it possible to redefine the default transport-group, but only
 * once.
 */
struct portal_group *
conf::define_default_transport_group()
{
	if (conf_default_tg_defined) {
		log_warnx("duplicated transport-group \"default\"");
		return (nullptr);
	}

	conf_default_tg_defined = true;
	return (find_transport_group("default"));
}

struct portal_group *
conf::find_transport_group(std::string_view name)
{
	auto it = conf_transport_groups.find(std::string(name));
	if (it == conf_transport_groups.end())
		return (nullptr);

	return (it->second.get());
}

bool
portal_group::is_dummy() const
{
	if (pg_foreign)
		return (true);
	if (pg_portals.empty())
		return (true);
	return (false);
}

freebsd::addrinfo_up
parse_addr_port(const char *address, const char *def_port)
{
	struct addrinfo hints, *ai;
	int error;

	std::string addr(address);
	std::string port(def_port);
	if (addr[0] == '[') {
		/*
		 * IPv6 address in square brackets, perhaps with port.
		 */
		addr.erase(0, 1);
		size_t pos = addr.find(']');
		if (pos == 0 || pos == addr.npos)
			return {};
		if (pos < addr.length() - 1) {
			port = addr.substr(pos + 1);
			if (port[0] != ':' || port.length() < 2)
				return {};
			port.erase(0, 1);
		}
		addr.resize(pos);
	} else {
		/*
		 * Either IPv6 address without brackets - and without
		 * a port - or IPv4 address.  Just count the colons.
		 */
		size_t pos = addr.find(':');
		if (pos != addr.npos && addr.find(':', pos + 1) == addr.npos) {
			/* Only a single colon at `pos`. */
			if (pos == addr.length() - 1)
				return {};
			port = addr.substr(pos + 1);
			addr.resize(pos);
		}
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(addr.c_str(), port.c_str(), &hints, &ai);
	if (error != 0)
		return {};
	return freebsd::addrinfo_up(ai);
}

void
portal_group::add_port(struct portal_group_port *port)
{
	pg_ports.emplace(port->target()->name(), port);
}

void
portal_group::remove_port(struct portal_group_port *port)
{
	auto it = pg_ports.find(port->target()->name());
	pg_ports.erase(it);
}

freebsd::nvlist_up
portal_group::options() const
{
	return (freebsd::nvlist_up(nvlist_clone(pg_options.get())));
}

bool
portal_group::add_option(const char *name, const char *value)
{
	return (option_new(pg_options.get(), name, value));
}

bool
portal_group::set_discovery_auth_group(const char *ag_name)
{
	if (pg_discovery_auth_group != nullptr) {
		log_warnx("discovery-auth-group for %s "
		    "\"%s\" specified more than once", keyword(), name());
		return (false);
	}
	pg_discovery_auth_group = pg_conf->find_auth_group(ag_name);
	if (pg_discovery_auth_group == nullptr) {
		log_warnx("unknown discovery-auth-group \"%s\" "
		    "for %s \"%s\"", ag_name, keyword(), name());
		return (false);
	}
	return (true);
}

bool
portal_group::set_dscp(u_int dscp)
{
	if (dscp >= 0x40) {
		log_warnx("invalid DSCP value %u for %s \"%s\"",
		    dscp, keyword(), name());
		return (false);
	}

	pg_dscp = dscp;
	return (true);
}

void
portal_group::set_foreign()
{
	pg_foreign = true;
}

bool
portal_group::set_offload(const char *offload)
{
	if (!pg_offload.empty()) {
		log_warnx("cannot set offload to \"%s\" for "
		    "%s \"%s\"; already defined",
		    offload, keyword(), name());
		return (false);
	}

	pg_offload = offload;
	return (true);
}

bool
portal_group::set_pcp(u_int pcp)
{
	if (pcp > 7) {
		log_warnx("invalid PCP value %u for %s \"%s\"",
		    pcp, keyword(), name());
		return (false);
	}

	pg_pcp = pcp;
	return (true);
}

bool
portal_group::set_redirection(const char *addr)
{
	if (!pg_redirection.empty()) {
		log_warnx("cannot set redirection to \"%s\" for "
		    "%s \"%s\"; already defined",
		    addr, keyword(), name());
		return (false);
	}

	pg_redirection = addr;
	return (true);
}

void
portal_group::set_tag(uint16_t tag)
{
	pg_tag = tag;
}

void
portal_group::verify(struct conf *conf)
{
	if (pg_discovery_auth_group == nullptr) {
		pg_discovery_auth_group = conf->find_auth_group("default");
		assert(pg_discovery_auth_group != nullptr);
	}

	if (pg_discovery_filter == discovery_filter::UNKNOWN)
		pg_discovery_filter = discovery_filter::NONE;

	if (!pg_redirection.empty()) {
		if (!pg_ports.empty()) {
			log_debugx("%s \"%s\" assigned to target, "
			    "but configured for redirection", keyword(),
			    name());
		}
		pg_assigned = true;
	} else if (!pg_ports.empty()) {
		pg_assigned = true;
	} else {
		if (pg_name != "default")
			log_warnx("%s \"%s\" not assigned "
			    "to any target", keyword(), name());
		pg_assigned = false;
	}
}

/*
 * Try to reuse a socket for 'newp' from an existing socket in one of
 * our portals.
 */
bool
portal_group::reuse_socket(struct portal &newp)
{
	for (portal_up &portal : pg_portals) {
		if (newp.reuse_socket(*portal))
			return (true);
	}
	return (false);
}

int
portal_group::open_sockets(struct conf &oldconf)
{
	int cumulated_error = 0;

	if (pg_foreign)
		return (0);

	if (!pg_assigned) {
		log_debugx("not listening on %s \"%s\", "
		    "not assigned to any target", keyword(), name());
		return (0);
	}

	for (portal_up &portal : pg_portals) {
		/*
		 * Try to find already open portal and reuse the
		 * listening socket.  We don't care about what portal
		 * or portal group that was, what matters is the
		 * listening address.
		 */
		if (oldconf.reuse_portal_group_socket(*portal))
			continue;

		if (!portal->init_socket()) {
			cumulated_error++;
			continue;
		}
	}
	return (cumulated_error);
}

void
portal_group::close_sockets()
{
	for (portal_up &portal : pg_portals) {
		if (portal->socket() < 0)
			continue;
		log_debugx("closing socket for %s, %s \"%s\"",
		    portal->listen(), keyword(), name());
		portal->close();
	}
}

bool
conf::add_isns(const char *addr)
{
	if (conf_isns.count(addr) > 0) {
		log_warnx("duplicate iSNS address %s", addr);
		return (false);
	}

	freebsd::addrinfo_up ai = parse_addr_port(addr, "3205");
	if (!ai) {
		log_warnx("invalid iSNS address %s", addr);
		return (false);
	}

	/*
	 * XXX: getaddrinfo(3) may return multiple addresses; we should turn
	 *	those into multiple servers.
	 */

	conf_isns.emplace(addr, isns(addr, std::move(ai)));
	return (true);
}


freebsd::fd_up
isns::connect()
{
	freebsd::fd_up s;

	s = socket(i_ai->ai_family, i_ai->ai_socktype, i_ai->ai_protocol);
	if (!s) {
		log_warn("socket(2) failed for %s", addr());
		return (s);
	}
	if (::connect(s, i_ai->ai_addr, i_ai->ai_addrlen)) {
		log_warn("connect(2) failed for %s", addr());
		s.reset();
	}
	return (s);
}

bool
isns::send_request(int s, struct isns_req req)
{
	if (!req.send(s)) {
		log_warn("send(2) failed for %s", addr());
		return (false);
	}
	if (!req.receive(s)) {
		log_warn("receive(2) failed for %s", addr());
		return (false);
	}
	uint32_t error = req.get_status();
	if (error != 0) {
		log_warnx("iSNS %s error %u for %s", req.descr(), error,
		    addr());
		return (false);
	}
	return (true);
}

struct isns_req
conf::isns_register_request(const char *hostname)
{
	const struct portal_group *pg;

	isns_req req(ISNS_FUNC_DEVATTRREG, ISNS_FLAG_CLIENT, "register");
	req.add_str(32, conf_first_target->name());
	req.add_delim();
	req.add_str(1, hostname);
	req.add_32(2, 2); /* 2 -- iSCSI */
	req.add_32(6, conf_isns_period);
	for (const auto &kv : conf_portal_groups) {
		pg = kv.second.get();

		if (!pg->assigned())
			continue;
		for (const portal_up &portal : pg->portals()) {
			req.add_addr(16, portal->ai());
			req.add_port(17, portal->ai());
		}
	}
	for (const auto &kv : conf_targets) {
		const struct target *target = kv.second.get();

		req.add_str(32, target->name());
		req.add_32(33, 1); /* 1 -- Target*/
		if (target->has_alias())
			req.add_str(34, target->alias());
		for (const port *port : target->ports()) {
			pg = port->portal_group();
			if (pg == nullptr)
				continue;
			req.add_32(51, pg->tag());
			for (const portal_up &portal : pg->portals()) {
				req.add_addr(49, portal->ai());
				req.add_port(50, portal->ai());
			}
		}
	}
	return (req);
}

struct isns_req
conf::isns_check_request(const char *hostname)
{
	isns_req req(ISNS_FUNC_DEVATTRQRY, ISNS_FLAG_CLIENT, "check");
	req.add_str(32, conf_first_target->name());
	req.add_str(1, hostname);
	req.add_delim();
	req.add(2, 0, NULL);
	return (req);
}

struct isns_req
conf::isns_deregister_request(const char *hostname)
{
	isns_req req(ISNS_FUNC_DEVDEREG, ISNS_FLAG_CLIENT, "deregister");
	req.add_str(32, conf_first_target->name());
	req.add_delim();
	req.add_str(1, hostname);
	return (req);
}

void
conf::isns_register_targets(struct isns *isns, struct conf *oldconf)
{
	int error;
	char hostname[256];

	if (conf_targets.empty() || conf_portal_groups.empty())
		return;
	start_timer(conf_isns_timeout);
	freebsd::fd_up s = isns->connect();
	if (!s) {
		stop_timer();
		return;
	}
	error = gethostname(hostname, sizeof(hostname));
	if (error != 0)
		log_err(1, "gethostname");

	if (oldconf == nullptr || oldconf->conf_first_target == nullptr)
		oldconf = this;
	isns->send_request(s, oldconf->isns_deregister_request(hostname));
	isns->send_request(s, isns_register_request(hostname));
	s.reset();
	stop_timer();
}

void
conf::isns_check(struct isns *isns)
{
	int error;
	char hostname[256];

	if (conf_targets.empty() || conf_portal_groups.empty())
		return;
	start_timer(conf_isns_timeout);
	freebsd::fd_up s = isns->connect();
	if (!s) {
		stop_timer();
		return;
	}
	error = gethostname(hostname, sizeof(hostname));
	if (error != 0)
		log_err(1, "gethostname");

	if (!isns->send_request(s, isns_check_request(hostname))) {
		isns->send_request(s, isns_deregister_request(hostname));
		isns->send_request(s, isns_register_request(hostname));
	}
	s.reset();
	stop_timer();
}

void
conf::isns_deregister_targets(struct isns *isns)
{
	int error;
	char hostname[256];

	if (conf_targets.empty() || conf_portal_groups.empty())
		return;
	start_timer(conf_isns_timeout);
	freebsd::fd_up s = isns->connect();
	if (!s)
		return;
	error = gethostname(hostname, sizeof(hostname));
	if (error != 0)
		log_err(1, "gethostname");

	isns->send_request(s, isns_deregister_request(hostname));
	s.reset();
	stop_timer();
}

void
conf::isns_schedule_update()
{
	if (!conf_isns.empty())
		start_timer((conf_isns_period + 2) / 3);
}

void
conf::isns_update()
{
	stop_timer();
	for (auto &kv : conf_isns)
		isns_check(&kv.second);

	isns_schedule_update();
}

bool
kports::add_port(std::string &name, uint32_t ctl_port)
{
	const auto &pair = pports.try_emplace(name, name, ctl_port);
	if (!pair.second) {
		log_warnx("duplicate kernel port \"%s\" (%u)", name.c_str(),
		    ctl_port);
		return (false);
	}

	return (true);
}

bool
kports::has_port(std::string_view name)
{
	return (pports.count(std::string(name)) > 0);
}

struct pport *
kports::find_port(std::string_view name)
{
	auto it = pports.find(std::string(name));
	if (it == pports.end())
		return (nullptr);
	return (&it->second);
}

port::port(struct target *target) :
	p_target(target)
{
	target->add_port(this);
}

void
port::clear_references()
{
	p_target->remove_port(this);
}

portal_group_port::portal_group_port(struct target *target,
    struct portal_group *pg, auth_group_sp ag) :
	port(target), p_auth_group(ag), p_portal_group(pg)
{
	p_portal_group->add_port(this);
}

portal_group_port::portal_group_port(struct target *target,
    struct portal_group *pg, uint32_t ctl_port) :
	port(target), p_portal_group(pg)
{
	p_ctl_port = ctl_port;
	p_portal_group->add_port(this);
}

bool
portal_group_port::is_dummy() const
{
	return (p_portal_group->is_dummy());
}

void
portal_group_port::clear_references()
{
	p_portal_group->remove_port(this);
	port::clear_references();
}

bool
conf::add_port(struct target *target, struct portal_group *pg, auth_group_sp ag)
{
	std::string name = freebsd::stringf("%s-%s", pg->name(),
	    target->name());
	const auto &pair = conf_ports.try_emplace(name, pg->create_port(target,
	    ag));
	if (!pair.second) {
		log_warnx("duplicate port \"%s\"", name.c_str());
		return (false);
	}

	return (true);
}

bool
conf::add_port(struct target *target, struct portal_group *pg,
    uint32_t ctl_port)
{
	std::string name = freebsd::stringf("%s-%s", pg->name(),
	    target->name());
	const auto &pair = conf_ports.try_emplace(name, pg->create_port(target,
	    ctl_port));
	if (!pair.second) {
		log_warnx("duplicate port \"%s\"", name.c_str());
		return (false);
	}

	return (true);
}

bool
conf::add_port(struct target *target, struct pport *pp)
{
	std::string name = freebsd::stringf("%s-%s", pp->name(),
	    target->name());
	const auto &pair = conf_ports.try_emplace(name,
	    std::make_unique<kernel_port>(target, pp));
	if (!pair.second) {
		log_warnx("duplicate port \"%s\"", name.c_str());
		return (false);
	}

	pp->link();
	return (true);
}

bool
conf::add_port(struct kports &kports, struct target *target, int pp, int vp)
{
	struct pport *pport;

	std::string pname = freebsd::stringf("ioctl/%d/%d", pp, vp);

	pport = kports.find_port(pname);
	if (pport != NULL)
		return (add_port(target, pport));

	std::string name = pname + "-" + target->name();
	const auto &pair = conf_ports.try_emplace(name,
	    std::make_unique<ioctl_port>(target, pp, vp));
	if (!pair.second) {
		log_warnx("duplicate port \"%s\"", name.c_str());
		return (false);
	}

	return (true);
}

const struct port *
portal_group::find_port(std::string_view target) const
{
	auto it = pg_ports.find(std::string(target));
	if (it == pg_ports.end())
		return (nullptr);
	return (it->second);
}

struct target *
conf::add_controller(const char *name)
{
	if (!nvmf_nqn_valid_strict(name)) {
		log_warnx("controller name \"%s\" is invalid for NVMe", name);
		return nullptr;
	}

	/*
	 * Normalize the name to lowercase to match iSCSI.
	 */
	std::string t_name(name);
	for (char &c : t_name)
		c = tolower(c);

	auto const &pair = conf_controllers.try_emplace(t_name,
	    nvmf_make_controller(this, t_name));
	if (!pair.second) {
		log_warnx("duplicated controller \"%s\"", name);
		return nullptr;
	}

	return pair.first->second.get();
}

struct target *
conf::find_controller(std::string_view name)
{
	auto it = conf_controllers.find(std::string(name));
	if (it == conf_controllers.end())
		return nullptr;
	return it->second.get();
}

target::target(struct conf *conf, const char *keyword, std::string_view name) :
	t_conf(conf), t_name(name)
{
	t_label = freebsd::stringf("%s \"%s\"", keyword, t_name.c_str());
}

struct target *
conf::add_target(const char *name)
{
	if (!valid_iscsi_name(name, log_warnx))
		return (nullptr);

	/*
	 * RFC 3722 requires us to normalize the name to lowercase.
	 */
	std::string t_name(name);
	for (char &c : t_name)
		c = tolower(c);

	auto const &pair = conf_targets.try_emplace(t_name,
	    iscsi_make_target(this, t_name));
	if (!pair.second) {
		log_warnx("duplicated target \"%s\"", name);
		return (NULL);
	}

	if (conf_first_target == nullptr)
		conf_first_target = pair.first->second.get();
	return (pair.first->second.get());
}

struct target *
conf::find_target(std::string_view name)
{
	auto it = conf_targets.find(std::string(name));
	if (it == conf_targets.end())
		return (nullptr);
	return (it->second.get());
}

bool
target::use_private_auth(const char *keyword)
{
	if (t_private_auth)
		return (true);

	if (t_auth_group != nullptr) {
		log_warnx("cannot use both auth-group and %s for %s",
		    keyword, label());
		return (false);
	}

	t_auth_group = std::make_shared<struct auth_group>(t_label);
	t_private_auth = true;
	return (true);
}

bool
target::add_chap(const char *user, const char *secret)
{
	if (!use_private_auth("chap"))
		return (false);
	return (t_auth_group->add_chap(user, secret));
}

bool
target::add_chap_mutual(const char *user, const char *secret,
    const char *user2, const char *secret2)
{
	if (!use_private_auth("chap-mutual"))
		return (false);
	return (t_auth_group->add_chap_mutual(user, secret, user2, secret2));
}

bool
target::add_lun(u_int id, const char *lun_label, const char *lun_name)
{
	struct lun *t_lun;

	if (id >= MAX_LUNS) {
		log_warnx("%s too big for %s", lun_label, label());
		return (false);
	}

	if (t_luns[id] != NULL) {
		log_warnx("duplicate %s for %s", lun_label, label());
		return (false);
	}

	t_lun = t_conf->find_lun(lun_name);
	if (t_lun == NULL) {
		log_warnx("unknown LUN named %s used for %s", lun_name,
		    label());
		return (false);
	}

	t_luns[id] = t_lun;
	return (true);
}

bool
target::set_alias(std::string_view alias)
{
	if (has_alias()) {
		log_warnx("alias for %s specified more than once", label());
		return (false);
	}
	t_alias = alias;
	return (true);
}

bool
target::set_auth_group(const char *ag_name)
{
	if (t_auth_group != nullptr) {
		if (t_private_auth)
			log_warnx("cannot use both auth-group and explicit "
			    "authorisations for %s", label());
		else
			log_warnx("auth-group for %s "
			    "specified more than once", label());
		return (false);
	}
	t_auth_group = t_conf->find_auth_group(ag_name);
	if (t_auth_group == nullptr) {
		log_warnx("unknown auth-group \"%s\" for %s",
		    ag_name, label());
		return (false);
	}
	return (true);
}

bool
target::set_auth_type(const char *type)
{
	if (!use_private_auth("auth-type"))
		return (false);
	return (t_auth_group->set_type(type));
}

bool
target::set_physical_port(std::string_view pport)
{
	if (!t_pport.empty()) {
		log_warnx("cannot set multiple physical ports for target "
		    "\"%s\"", name());
		return (false);
	}
	t_pport = pport;
	return (true);
}

bool
target::set_redirection(const char *addr)
{
	if (!t_redirection.empty()) {
		log_warnx("cannot set redirection to \"%s\" for "
		    "%s; already defined",
		    addr, label());
		return (false);
	}

	t_redirection = addr;
	return (true);
}

struct lun *
target::start_lun(u_int id, const char *lun_label, const char *lun_name)
{
	if (id >= MAX_LUNS) {
		log_warnx("%s too big for %s", lun_label, label());
		return (nullptr);
	}

	if (t_luns[id] != NULL) {
		log_warnx("duplicate %s for %s", lun_label, label());
		return (nullptr);
	}

	struct lun *new_lun = t_conf->add_lun(lun_name);
	if (new_lun == nullptr)
		return (nullptr);

	new_lun->set_scsiname(lun_name);

	t_luns[id] = new_lun;

	return (new_lun);
}

void
target::add_port(struct port *port)
{
	t_ports.push_back(port);
}

void
target::remove_port(struct port *port)
{
	t_ports.remove(port);
}

void
target::remove_lun(struct lun *lun)
{
	/* XXX: clang is not able to deduce the type without the cast. */
	std::replace(t_luns.begin(), t_luns.end(), lun,
	    static_cast<struct lun *>(nullptr));
}

void
target::verify()
{
	if (t_auth_group == nullptr) {
		t_auth_group = t_conf->find_auth_group("default");
		assert(t_auth_group != nullptr);
	}
	if (t_ports.empty()) {
		struct portal_group *pg = default_portal_group();
		assert(pg != NULL);
		t_conf->add_port(this, pg, nullptr);
	}

	bool found = std::any_of(t_luns.begin(), t_luns.end(),
	    [](struct lun *lun) { return (lun != nullptr); });
	if (!found && t_redirection.empty())
		log_warnx("no LUNs defined for %s", label());
	if (found && !t_redirection.empty())
		log_debugx("%s contains LUNs, but configured "
		    "for redirection", label());
}

lun::lun(struct conf *conf, std::string_view name)
    : l_conf(conf), l_options(nvlist_create(0)), l_name(name)
{
}

struct lun *
conf::add_lun(const char *name)
{
	const auto &pair = conf_luns.try_emplace(name,
	    std::make_unique<lun>(this, name));
	if (!pair.second) {
		log_warnx("duplicated lun \"%s\"", name);
		return (NULL);
	}
	return (pair.first->second.get());
}

void
conf::delete_target_luns(struct lun *lun)
{
	for (const auto &kv : conf_targets)
		kv.second->remove_lun(lun);
	for (const auto &kv : conf_controllers)
		kv.second->remove_lun(lun);
}

struct lun *
conf::find_lun(std::string_view name)
{
	auto it = conf_luns.find(std::string(name));
	if (it == conf_luns.end())
		return (nullptr);
	return (it->second.get());
}

static void
nvlist_replace_string(nvlist_t *nvl, const char *name, const char *value)
{
	if (nvlist_exists_string(nvl, name))
		nvlist_free_string(nvl, name);
	nvlist_add_string(nvl, name, value);
}

freebsd::nvlist_up
lun::options() const
{
	freebsd::nvlist_up nvl(nvlist_clone(l_options.get()));
	if (!l_path.empty())
		nvlist_replace_string(nvl.get(), "file", l_path.c_str());

	nvlist_replace_string(nvl.get(), "ctld_name", l_name.c_str());

	if (!nvlist_exists_string(nvl.get(), "scsiname") &&
	    !l_scsiname.empty())
		nvlist_add_string(nvl.get(), "scsiname", l_scsiname.c_str());
	return (nvl);
}

bool
lun::add_option(const char *name, const char *value)
{
	return (option_new(l_options.get(), name, value));
}

bool
lun::set_backend(std::string_view value)
{
	if (!l_backend.empty()) {
		log_warnx("backend for lun \"%s\" specified more than once",
		    name());
		return (false);
	}

	l_backend = value;
	return (true);
}

bool
lun::set_blocksize(size_t value)
{
	if (l_blocksize != 0) {
		log_warnx("blocksize for lun \"%s\" specified more than once",
		    name());
		return (false);
	}
	l_blocksize = value;
	return (true);
}

bool
lun::set_ctl_lun(uint32_t value)
{
	if (l_ctl_lun >= 0) {
		log_warnx("ctl_lun for lun \"%s\" specified more than once",
		    name());
		return (false);
	}

	l_ctl_lun = value;
	return (true);
}

bool
lun::set_device_type(uint8_t device_type)
{
	if (device_type > 15) {
		log_warnx("invalid device-type \"%u\" for lun \"%s\"",
		    device_type, name());
		return (false);
	}

	l_device_type = device_type;
	return (true);
}

bool
lun::set_device_type(const char *value)
{
	const char *errstr;
	int device_type;

	if (strcasecmp(value, "disk") == 0 ||
	    strcasecmp(value, "direct") == 0)
		device_type = T_DIRECT;
	else if (strcasecmp(value, "processor") == 0)
		device_type = T_PROCESSOR;
	else if (strcasecmp(value, "cd") == 0 ||
	    strcasecmp(value, "cdrom") == 0 ||
	    strcasecmp(value, "dvd") == 0 ||
	    strcasecmp(value, "dvdrom") == 0)
		device_type = T_CDROM;
	else {
		device_type = strtonum(value, 0, 15, &errstr);
		if (errstr != NULL) {
			log_warnx("invalid device-type \"%s\" for lun \"%s\"",
			    value, name());
			return (false);
		}
	}

	l_device_type = device_type;
	return (true);
}

bool
lun::set_device_id(std::string_view value)
{
	if (!l_device_id.empty()) {
		log_warnx("device_id for lun \"%s\" specified more than once",
		    name());
		return (false);
	}

	l_device_id = value;
	return (true);
}

bool
lun::set_path(std::string_view value)
{
	if (!l_path.empty()) {
		log_warnx("path for lun \"%s\" specified more than once",
		    name());
		return (false);
	}

	l_path = value;
	return (true);
}

void
lun::set_scsiname(std::string_view value)
{
	l_scsiname = value;
}

bool
lun::set_serial(std::string_view value)
{
	if (!l_serial.empty()) {
		log_warnx("serial for lun \"%s\" specified more than once",
		    name());
		return (false);
	}

	l_serial = value;
	return (true);
}

bool
lun::set_size(uint64_t value)
{
	if (l_size != 0) {
		log_warnx("size for lun \"%s\" specified more than once",
		    name());
		return (false);
	}

	l_size = value;
	return (true);
}


bool
lun::changed(const struct lun &newlun) const
{
	if (l_backend != newlun.l_backend) {
		log_debugx("backend for lun \"%s\", CTL lun %d changed; "
		    "removing", name(), l_ctl_lun);
		return (true);
	}
	if (l_blocksize != newlun.l_blocksize) {
		log_debugx("blocksize for lun \"%s\", CTL lun %d changed; "
		    "removing", name(), l_ctl_lun);
		return (true);
	}
	if (l_device_id != newlun.l_device_id) {
		log_debugx("device-id for lun \"%s\", CTL lun %d changed; "
		    "removing", name(), l_ctl_lun);
		return (true);
	}
	if (l_path != newlun.l_path) {
		log_debugx("path for lun \"%s\", CTL lun %d, changed; "
		    "removing", name(), l_ctl_lun);
		return (true);
	}
	if (l_serial != newlun.l_serial) {
		log_debugx("serial for lun \"%s\", CTL lun %d changed; "
		    "removing", name(), l_ctl_lun);
		return (true);
	}
	return (false);
}

bool
option_new(nvlist_t *nvl, const char *name, const char *value)
{
	int error;

	if (nvlist_exists_string(nvl, name)) {
		log_warnx("duplicated option \"%s\"", name);
		return (false);
	}

	nvlist_add_string(nvl, name, value);
	error = nvlist_error(nvl);
	if (error != 0) {
		log_warnc(error, "failed to add option \"%s\"", name);
		return (false);
	}
	return (true);
}

bool
lun::verify()
{
	if (l_backend.empty())
		l_backend = "block";
	if (l_backend == "block") {
		if (l_path.empty()) {
			log_warnx("missing path for lun \"%s\"",
			    name());
			return (false);
		}
	} else if (l_backend == "ramdisk") {
		if (l_size == 0) {
			log_warnx("missing size for ramdisk-backed lun \"%s\"",
			    name());
			return (false);
		}
		if (!l_path.empty()) {
			log_warnx("path must not be specified "
			    "for ramdisk-backed lun \"%s\"",
			    name());
			return (false);
		}
	}
	if (l_blocksize == 0) {
		if (l_device_type == T_CDROM)
			l_blocksize = DEFAULT_CD_BLOCKSIZE;
		else
			l_blocksize = DEFAULT_BLOCKSIZE;
	} else if (l_blocksize < 0) {
		log_warnx("invalid blocksize %d for lun \"%s\"; "
		    "must be larger than 0", l_blocksize, name());
		return (false);
	}
	if (l_size != 0 && (l_size % l_blocksize) != 0) {
		log_warnx("invalid size for lun \"%s\"; "
		    "must be multiple of blocksize", name());
		return (false);
	}
	return (true);
}

bool
conf::verify()
{
	if (conf_pidfile_path.empty())
		conf_pidfile_path = DEFAULT_PIDFILE;

	std::unordered_map<std::string, struct lun *> path_map;
	for (const auto &kv : conf_luns) {
		struct lun *lun = kv.second.get();
		if (!lun->verify())
			return (false);

		const std::string &path = lun->path();
		if (path.empty())
			continue;

		const auto &pair = path_map.try_emplace(path, lun);
		if (!pair.second) {
			struct lun *lun2 = pair.first->second;
			log_debugx("WARNING: path \"%s\" duplicated "
			    "between lun \"%s\", and "
			    "lun \"%s\"", path.c_str(),
			    lun->name(), lun2->name());
		}
	}

	for (auto &kv : conf_targets) {
		kv.second->verify();
	}
	for (auto &kv : conf_controllers) {
		kv.second->verify();
	}
	for (auto &kv : conf_portal_groups) {
		kv.second->verify(this);
	}
	for (auto &kv : conf_transport_groups) {
		kv.second->verify(this);
	}
	for (const auto &kv : conf_auth_groups) {
		const std::string &ag_name = kv.first;
		if (ag_name == "default" ||
		    ag_name == "no-authentication" ||
		    ag_name == "no-access")
			continue;

		if (kv.second.use_count() == 1) {
			log_warnx("auth-group \"%s\" not assigned "
			    "to any target", ag_name.c_str());
		}
	}

	return (true);
}

bool
portal::reuse_socket(struct portal &oldp)
{
	struct kevent kev;

	if (p_listen != oldp.p_listen)
		return (false);

	if (!oldp.p_socket)
		return (false);

	EV_SET(&kev, oldp.p_socket, EVFILT_READ, EV_ADD, 0, 0, this);
	if (kevent(kqfd, &kev, 1, NULL, 0, NULL) == -1)
		return (false);

	p_socket = std::move(oldp.p_socket);
	return (true);
}

bool
portal::init_socket()
{
	struct portal_group *pg = portal_group();
	struct kevent kev;
	freebsd::fd_up s;
	int error;
	int one = 1;

#ifdef ICL_KERNEL_PROXY
	if (proxy_mode) {
		int id = pg->conf()->add_proxy_portal(this);
		log_debugx("listening on %s, %s \"%s\", "
		    "portal id %d, using ICL proxy", listen(), pg->keyword(),
		    pg->name(), id);
		kernel_listen(ai(), protocol() == ISER, id);
		return (true);
	}
#endif
	assert(proxy_mode == false);
	assert(protocol() != portal_protocol::ISER);

	log_debugx("listening on %s, %s \"%s\"", listen(), pg->keyword(),
	    pg->name());
	s = ::socket(p_ai->ai_family, p_ai->ai_socktype, p_ai->ai_protocol);
	if (!s) {
		log_warn("socket(2) failed for %s", listen());
		return (false);
	}

	if (setsockopt(s, SOL_SOCKET, SO_NO_DDP, &one,
	    sizeof(one)) == -1)
		log_warn("setsockopt(SO_NO_DDP) failed for %s", listen());
	error = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one,
	    sizeof(one));
	if (error != 0) {
		log_warn("setsockopt(SO_REUSEADDR) failed for %s", listen());
		return (false);
	}

	if (pg->dscp() != -1) {
		/* Only allow the 6-bit DSCP field to be modified */
		int tos = pg->dscp() << 2;
		switch (p_ai->ai_family) {
		case AF_INET:
			if (setsockopt(s, IPPROTO_IP, IP_TOS,
			    &tos, sizeof(tos)) == -1)
				log_warn("setsockopt(IP_TOS) failed for %s",
				    listen());
			break;
		case AF_INET6:
			if (setsockopt(s, IPPROTO_IPV6, IPV6_TCLASS,
			    &tos, sizeof(tos)) == -1)
				log_warn("setsockopt(IPV6_TCLASS) failed for %s",
				    listen());
			break;
		}
	}
	if (pg->pcp() != -1) {
		int pcp = pg->pcp();
		switch (p_ai->ai_family) {
		case AF_INET:
			if (setsockopt(s, IPPROTO_IP, IP_VLAN_PCP,
			    &pcp, sizeof(pcp)) == -1)
				log_warn("setsockopt(IP_VLAN_PCP) failed for %s",
				    listen());
			break;
		case AF_INET6:
			if (setsockopt(s, IPPROTO_IPV6, IPV6_VLAN_PCP,
			    &pcp, sizeof(pcp)) == -1)
				log_warn("setsockopt(IPV6_VLAN_PCP) failed for %s",
				    listen());
			break;
		}
	}

	if (!init_socket_options(s))
		return (false);

	error = bind(s, p_ai->ai_addr, p_ai->ai_addrlen);
	if (error != 0) {
		log_warn("bind(2) failed for %s", listen());
		return (false);
	}
	error = ::listen(s, -1);
	if (error != 0) {
		log_warn("listen(2) failed for %s", listen());
		return (false);
	}
	EV_SET(&kev, s, EVFILT_READ, EV_ADD, 0, 0, this);
	error = kevent(kqfd, &kev, 1, NULL, 0, NULL);
	if (error == -1) {
		log_warn("kevent(2) failed to register for %s", listen());
		return (false);
	}
	p_socket = std::move(s);
	return (true);
}

bool
conf::reuse_portal_group_socket(struct portal &newp)
{
	for (auto &kv : conf_portal_groups) {
		struct portal_group &pg = *kv.second;

		if (pg.reuse_socket(newp))
			return (true);
	}
	for (auto &kv : conf_transport_groups) {
		struct portal_group &pg = *kv.second;

		if (pg.reuse_socket(newp))
			return (true);
	}
	return (false);
}

int
conf::apply(struct conf *oldconf)
{
	int cumulated_error = 0;

	if (oldconf->conf_debug != conf_debug) {
		log_debugx("changing debug level to %d", conf_debug);
		log_init(conf_debug);
	}

	/*
	 * On startup, oldconf created via conf_new_from_kernel will
	 * not contain a valid pidfile_path, and the current
	 * conf_pidfile will already own the pidfile.  On shutdown,
	 * the temporary newconf will not contain a valid
	 * pidfile_path, and the pidfile will be cleaned up when the
	 * oldconf is deleted.
	 */
	if (!oldconf->conf_pidfile_path.empty() &&
	    !conf_pidfile_path.empty()) {
		if (oldconf->conf_pidfile_path != conf_pidfile_path) {
			/* pidfile has changed.  rename it */
			log_debugx("moving pidfile to %s",
			    conf_pidfile_path.c_str());
			if (rename(oldconf->conf_pidfile_path.c_str(),
				conf_pidfile_path.c_str()) != 0) {
				log_err(1, "renaming pidfile %s -> %s",
				    oldconf->conf_pidfile_path.c_str(),
				    conf_pidfile_path.c_str());
			}
		}
		conf_pidfile = std::move(oldconf->conf_pidfile);
	}

	/*
	 * Go through the new portal groups, assigning tags or preserving old.
	 */
	for (auto &kv : conf_portal_groups) {
		struct portal_group &newpg = *kv.second;

		if (newpg.tag() != 0)
			continue;
		auto it = oldconf->conf_portal_groups.find(kv.first);
		if (it != oldconf->conf_portal_groups.end())
			newpg.set_tag(it->second->tag());
		else
			newpg.allocate_tag();
	}
	for (auto &kv : conf_transport_groups) {
		struct portal_group &newpg = *kv.second;

		if (newpg.tag() != 0)
			continue;
		auto it = oldconf->conf_transport_groups.find(kv.first);
		if (it != oldconf->conf_transport_groups.end())
			newpg.set_tag(it->second->tag());
		else
			newpg.allocate_tag();
	}

	/* Deregister on removed iSNS servers. */
	for (auto &kv : oldconf->conf_isns) {
		if (conf_isns.count(kv.first) == 0)
			oldconf->isns_deregister_targets(&kv.second);
	}

	/*
	 * XXX: If target or lun removal fails, we should somehow "move"
	 *      the old lun or target into this, so that subsequent
	 *      conf::apply() would try to remove them again.  That would
	 *      be somewhat hairy, though, and lun deletion failures don't
	 *      really happen, so leave it as it is for now.
	 */
	/*
	 * First, remove any ports present in the old configuration
	 * and missing in the new one.
	 */
	for (const auto &kv : oldconf->conf_ports) {
		const std::string &name = kv.first;
		port *oldport = kv.second.get();

		if (oldport->is_dummy())
			continue;
		const auto it = conf_ports.find(name);
		if (it != conf_ports.end() && !it->second->is_dummy())
			continue;
		log_debugx("removing port \"%s\"", name.c_str());
		if (!oldport->kernel_remove()) {
			log_warnx("failed to remove port %s", name.c_str());
			/*
			 * XXX: Uncomment after fixing the root cause.
			 *
			 * cumulated_error++;
			 */
		}
	}

	/*
	 * Second, remove any LUNs present in the old configuration
	 * and missing in the new one.
	 */
	for (auto it = oldconf->conf_luns.begin();
	     it != oldconf->conf_luns.end(); ) {
		struct lun *oldlun = it->second.get();

		auto newit = conf_luns.find(it->first);
		if (newit == conf_luns.end()) {
			log_debugx("lun \"%s\", CTL lun %d "
			    "not found in new configuration; "
			    "removing", oldlun->name(), oldlun->ctl_lun());
			if (!oldlun->kernel_remove()) {
				log_warnx("failed to remove lun \"%s\", "
				    "CTL lun %d",
				    oldlun->name(), oldlun->ctl_lun());
				cumulated_error++;
			}
			it++;
			continue;
		}

		/*
		 * Also remove the LUNs changed by more than size.
		 */
		struct lun *newlun = newit->second.get();
		if (oldlun->changed(*newlun)) {
			if (!oldlun->kernel_remove()) {
				log_warnx("failed to remove lun \"%s\", "
				    "CTL lun %d",
				    oldlun->name(), oldlun->ctl_lun());
				cumulated_error++;
			}

			/*
			 * Delete the lun from the old configuration
			 * so it is added as a new LUN below.
			 */
			it = oldconf->conf_luns.erase(it);
			continue;
		}

		newlun->set_ctl_lun(oldlun->ctl_lun());
		it++;
	}

	for (auto it = conf_luns.begin(); it != conf_luns.end(); ) {
		struct lun *newlun = it->second.get();

		auto oldit = oldconf->conf_luns.find(it->first);
		if (oldit != oldconf->conf_luns.end()) {
			log_debugx("modifying lun \"%s\", CTL lun %d",
			    newlun->name(), newlun->ctl_lun());
			if (!newlun->kernel_modify()) {
				log_warnx("failed to "
				    "modify lun \"%s\", CTL lun %d",
				    newlun->name(), newlun->ctl_lun());
				cumulated_error++;
			}
			it++;
			continue;
		}

		log_debugx("adding lun \"%s\"", newlun->name());
		if (!newlun->kernel_add()) {
			log_warnx("failed to add lun \"%s\"", newlun->name());
			delete_target_luns(newlun);
			it = conf_luns.erase(it);
			cumulated_error++;
		} else
			it++;
	}

	/*
	 * Now add new ports or modify existing ones.
	 */
	for (auto it = conf_ports.begin(); it != conf_ports.end(); ) {
		const std::string &name = it->first;
		port *newport = it->second.get();

		if (newport->is_dummy()) {
			it++;
			continue;
		}
		const auto oldit = oldconf->conf_ports.find(name);
		if (oldit == oldconf->conf_ports.end() ||
		    oldit->second->is_dummy()) {
			log_debugx("adding port \"%s\"", name.c_str());
			if (!newport->kernel_add()) {
				log_warnx("failed to add port %s",
				    name.c_str());

				/*
				 * XXX: Uncomment after fixing the
				 * root cause.
				 *
				 * cumulated_error++;
				 */

				/*
				 * conf "owns" the port, but other
				 * objects contain pointers to this
				 * port that must be removed before
				 * deleting the port.
				 */
				newport->clear_references();
				it = conf_ports.erase(it);
			} else
				it++;
		} else {
			log_debugx("updating port \"%s\"", name.c_str());
			if (!newport->kernel_update(oldit->second.get()))
				log_warnx("failed to update port %s",
				    name.c_str());
			it++;
		}
	}

	/*
	 * Go through the new portals, opening the sockets as necessary.
	 */
	for (auto &kv : conf_portal_groups) {
		cumulated_error += kv.second->open_sockets(*oldconf);
	}
	for (auto &kv : conf_transport_groups) {
		cumulated_error += kv.second->open_sockets(*oldconf);
	}

	/*
	 * Go through the no longer used sockets, closing them.
	 */
	for (auto &kv : oldconf->conf_portal_groups) {
		kv.second->close_sockets();
	}
	for (auto &kv : oldconf->conf_transport_groups) {
		kv.second->close_sockets();
	}

	/* (Re-)Register on remaining/new iSNS servers. */
	for (auto &kv : conf_isns) {
		auto it = oldconf->conf_isns.find(kv.first);
		if (it == oldconf->conf_isns.end())
			isns_register_targets(&kv.second, nullptr);
		else
			isns_register_targets(&kv.second, oldconf);
	}

	isns_schedule_update();

	return (cumulated_error);
}

bool
timed_out(void)
{

	return (sigalrm_received);
}

static void
sigalrm_handler_fatal(int dummy __unused)
{
	/*
	 * It would be easiest to just log an error and exit.  We can't
	 * do this, though, because log_errx() is not signal safe, since
	 * it calls syslog(3).  Instead, set a flag checked by pdu_send()
	 * and pdu_receive(), to call log_errx() there.  Should they fail
	 * to notice, we'll exit here one second later.
	 */
	if (sigalrm_received) {
		/*
		 * Oh well.  Just give up and quit.
		 */
		_exit(2);
	}

	sigalrm_received = true;
}

static void
sigalrm_handler(int dummy __unused)
{

	sigalrm_received = true;
}

void
stop_timer()
{
	struct itimerval itv;
	int error;

	log_debugx("session timeout disabled");
	bzero(&itv, sizeof(itv));
	error = setitimer(ITIMER_REAL, &itv, NULL);
	if (error != 0)
		log_err(1, "setitimer");
	sigalrm_received = false;
}

void
start_timer(int timeout, bool fatal)
{
	struct sigaction sa;
	struct itimerval itv;
	int error;

	if (timeout <= 0) {
		stop_timer();
		return;
	}

	sigalrm_received = false;
	bzero(&sa, sizeof(sa));
	if (fatal)
		sa.sa_handler = sigalrm_handler_fatal;
	else
		sa.sa_handler = sigalrm_handler;
	sigfillset(&sa.sa_mask);
	error = sigaction(SIGALRM, &sa, NULL);
	if (error != 0)
		log_err(1, "sigaction");

	/*
	 * First SIGALRM will arive after timeout seconds.
	 * If we do nothing, another one will arrive a second later.
	 */
	log_debugx("setting session timeout to %d seconds", timeout);
	bzero(&itv, sizeof(itv));
	itv.it_interval.tv_sec = 1;
	itv.it_value.tv_sec = timeout;
	error = setitimer(ITIMER_REAL, &itv, NULL);
	if (error != 0)
		log_err(1, "setitimer");
}

static int
wait_for_children(bool block)
{
	pid_t pid;
	int status;
	int num = 0;

	for (;;) {
		/*
		 * If "block" is true, wait for at least one process.
		 */
		if (block && num == 0)
			pid = wait4(-1, &status, 0, NULL);
		else
			pid = wait4(-1, &status, WNOHANG, NULL);
		if (pid <= 0)
			break;
		if (WIFSIGNALED(status)) {
			log_warnx("child process %d terminated with signal %d",
			    pid, WTERMSIG(status));
		} else if (WEXITSTATUS(status) != 0) {
			log_warnx("child process %d terminated with exit status %d",
			    pid, WEXITSTATUS(status));
		} else {
			log_debugx("child process %d terminated gracefully", pid);
		}
		num++;
	}

	return (num);
}

static void
handle_connection(struct portal *portal, freebsd::fd_up fd,
    const struct sockaddr *client_sa, bool dont_fork)
{
	struct portal_group *pg;
	int error;
	pid_t pid;
	char host[NI_MAXHOST + 1];
	struct conf *conf;

	pg = portal->portal_group();
	conf = pg->conf();

	if (dont_fork) {
		log_debugx("incoming connection; not forking due to -d flag");
	} else {
		nchildren -= wait_for_children(false);
		assert(nchildren >= 0);

		while (conf->maxproc() > 0 && nchildren >= conf->maxproc()) {
			log_debugx("maxproc limit of %d child processes hit; "
			    "waiting for child process to exit",
			    conf->maxproc());
			nchildren -= wait_for_children(true);
			assert(nchildren >= 0);
		}
		log_debugx("incoming connection; forking child process #%d",
		    nchildren);
		nchildren++;
		pid = fork();
		if (pid < 0)
			log_err(1, "fork");
		if (pid > 0)
			return;
		conf->close_pidfile();
	}

	error = getnameinfo(client_sa, client_sa->sa_len,
	    host, sizeof(host), NULL, 0, NI_NUMERICHOST);
	if (error != 0)
		log_errx(1, "getnameinfo: %s", gai_strerror(error));

	log_debugx("accepted connection from %s; portal group \"%s\"",
	    host, pg->name());
	log_set_peer_addr(host);
	setproctitle("%s", host);

	portal->handle_connection(std::move(fd), host, client_sa);
	log_debugx("nothing more to do; exiting");
	exit(0);
}

static void
main_loop(bool dont_fork)
{
	struct kevent kev;
	struct portal *portal;
	struct sockaddr_storage client_sa;
	socklen_t client_salen;
#ifdef ICL_KERNEL_PROXY
	int connection_id;
	int portal_id;
#endif
	int error, client_fd;

	for (;;) {
		if (sighup_received || sigterm_received || timed_out())
			return;

#ifdef ICL_KERNEL_PROXY
		if (proxy_mode) {
			client_salen = sizeof(client_sa);
			kernel_accept(&connection_id, &portal_id,
			    (struct sockaddr *)&client_sa, &client_salen);
			assert(client_salen >= client_sa.ss_len);

			log_debugx("incoming connection, id %d, portal id %d",
			    connection_id, portal_id);
			portal = conf->proxy_portal(portal_id);
			if (portal == nullptr)
				log_errx(1,
				    "kernel returned invalid portal_id %d",
				    portal_id);

			handle_connection(portal, connection_id,
			    (struct sockaddr *)&client_sa, dont_fork);
		} else {
#endif
			assert(proxy_mode == false);

			error = kevent(kqfd, NULL, 0, &kev, 1, NULL);
			if (error == -1) {
				if (errno == EINTR)
					continue;
				log_err(1, "kevent");
			}

			switch (kev.filter) {
			case EVFILT_READ:
				portal = reinterpret_cast<struct portal *>(kev.udata);
				assert(portal->socket() == (int)kev.ident);

				client_salen = sizeof(client_sa);
				client_fd = accept(portal->socket(),
				    (struct sockaddr *)&client_sa,
				    &client_salen);
				if (client_fd < 0) {
					if (errno == ECONNABORTED)
						continue;
					log_err(1, "accept");
				}
				assert(client_salen >= client_sa.ss_len);

				handle_connection(portal, client_fd,
				    (struct sockaddr *)&client_sa, dont_fork);
				break;
			default:
				__assert_unreachable();
			}
#ifdef ICL_KERNEL_PROXY
		}
#endif
	}
}

static void
sighup_handler(int dummy __unused)
{

	sighup_received = true;
}

static void
sigterm_handler(int dummy __unused)
{

	sigterm_received = true;
}

static void
sigchld_handler(int dummy __unused)
{

	/*
	 * The only purpose of this handler is to make SIGCHLD
	 * interrupt the ISCSIDWAIT ioctl(2), so we can call
	 * wait_for_children().
	 */
}

static void
register_signals(void)
{
	struct sigaction sa;
	int error;

	bzero(&sa, sizeof(sa));
	sa.sa_handler = sighup_handler;
	sigfillset(&sa.sa_mask);
	error = sigaction(SIGHUP, &sa, NULL);
	if (error != 0)
		log_err(1, "sigaction");

	sa.sa_handler = sigterm_handler;
	error = sigaction(SIGTERM, &sa, NULL);
	if (error != 0)
		log_err(1, "sigaction");

	sa.sa_handler = sigterm_handler;
	error = sigaction(SIGINT, &sa, NULL);
	if (error != 0)
		log_err(1, "sigaction");

	sa.sa_handler = sigchld_handler;
	error = sigaction(SIGCHLD, &sa, NULL);
	if (error != 0)
		log_err(1, "sigaction");
}

static void
check_perms(const char *path)
{
	struct stat sb;
	int error;

	error = stat(path, &sb);
	if (error != 0) {
		log_warn("stat");
		return;
	}
	if (sb.st_mode & S_IWOTH) {
		log_warnx("%s is world-writable", path);
	} else if (sb.st_mode & S_IROTH) {
		log_warnx("%s is world-readable", path);
	} else if (sb.st_mode & S_IXOTH) {
		/*
		 * Ok, this one doesn't matter, but still do it,
		 * just for consistency.
		 */
		log_warnx("%s is world-executable", path);
	}

	/*
	 * XXX: Should we also check for owner != 0?
	 */
}

static conf_up
conf_new_from_file(const char *path, bool ucl)
{
	struct auth_group *ag;
	struct portal_group *pg;
	bool valid;

	log_debugx("obtaining configuration from %s", path);

	conf_up conf = std::make_unique<struct conf>();

	ag = conf->add_auth_group("default");
	assert(ag != NULL);

	ag = conf->add_auth_group("no-authentication");
	assert(ag != NULL);
	ag->set_type(auth_type::NO_AUTHENTICATION);

	ag = conf->add_auth_group("no-access");
	assert(ag != NULL);
	ag->set_type(auth_type::DENY);

	pg = conf->add_portal_group("default");
	assert(pg != NULL);

	pg = conf->add_transport_group("default");
	assert(pg != NULL);

	conf_start(conf.get());
	if (ucl)
		valid = uclparse_conf(path);
	else
		valid = parse_conf(path);
	conf_finish();

	if (!valid) {
		conf.reset();
		return {};
	}

	check_perms(path);

	if (!conf->default_auth_group_defined()) {
		log_debugx("auth-group \"default\" not defined; "
		    "going with defaults");
		ag = conf->find_auth_group("default").get();
		assert(ag != NULL);
		ag->set_type(auth_type::DENY);
	}

	if (!conf->default_portal_group_defined()) {
		log_debugx("portal-group \"default\" not defined; "
		    "going with defaults");
		pg = conf->find_portal_group("default");
		assert(pg != NULL);
		pg->add_default_portals();
	}

	if (!conf->default_portal_group_defined()) {
		log_debugx("transport-group \"default\" not defined; "
		    "going with defaults");
		pg = conf->find_transport_group("default");
		assert(pg != NULL);
		pg->add_default_portals();
	}

	if (!conf->verify()) {
		conf.reset();
		return {};
	}

	return (conf);
}

/*
 * If the config file specifies physical ports for any target, associate them
 * with the config file.  If necessary, create them.
 */
bool
conf::add_pports(struct kports &kports)
{
	struct pport *pp;
	int ret, i_pp, i_vp;

	for (auto &kv : conf_targets) {
		struct target *targ = kv.second.get();

		if (!targ->has_pport())
			continue;

		ret = sscanf(targ->pport(), "ioctl/%d/%d", &i_pp, &i_vp);
		if (ret > 0) {
			if (!add_port(kports, targ, i_pp, i_vp)) {
				log_warnx("can't create new ioctl port "
				    "for %s", targ->label());
				return (false);
			}

			continue;
		}

		pp = kports.find_port(targ->pport());
		if (pp == NULL) {
			log_warnx("unknown port \"%s\" for %s",
			    targ->pport(), targ->label());
			return (false);
		}
		if (pp->linked()) {
			log_warnx("can't link port \"%s\" to %s, "
			    "port already linked to some target",
			    targ->pport(), targ->label());
			return (false);
		}
		if (!add_port(targ, pp)) {
			log_warnx("can't link port \"%s\" to %s",
			    targ->pport(), targ->label());
			return (false);
		}
	}
	return (true);
}

int
main(int argc, char **argv)
{
	struct kports kports;
	const char *config_path = DEFAULT_CONFIG_PATH;
	int debug = 0, ch, error;
	bool daemonize = true;
	bool test_config = false;
	bool use_ucl = false;

	while ((ch = getopt(argc, argv, "dtuf:R")) != -1) {
		switch (ch) {
		case 'd':
			daemonize = false;
			debug++;
			break;
		case 't':
			test_config = true;
			break;
		case 'u':
			use_ucl = true;
			break;
		case 'f':
			config_path = optarg;
			break;
		case 'R':
#ifndef ICL_KERNEL_PROXY
			log_errx(1, "ctld(8) compiled without ICL_KERNEL_PROXY "
			    "does not support iSER protocol");
#endif
			proxy_mode = true;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	if (argc != 0)
		usage();

	log_init(debug);
	kernel_init();

	conf_up newconf = conf_new_from_file(config_path, use_ucl);

	if (newconf == NULL)
		log_errx(1, "configuration error; exiting");

	if (test_config)
		return (0);

	newconf->open_pidfile();

	register_signals();

	conf_up oldconf = conf_new_from_kernel(kports);

	if (debug > 0) {
		oldconf->set_debug(debug);
		newconf->set_debug(debug);
	}

	if (!newconf->add_pports(kports))
		log_errx(1, "Error associating physical ports; exiting");

	if (daemonize) {
		log_debugx("daemonizing");
		if (daemon(0, 0) == -1) {
			log_warn("cannot daemonize");
			return (1);
		}
	}

	kqfd = kqueue();
	if (kqfd == -1) {
		log_warn("Cannot create kqueue");
		return (1);
	}

	error = newconf->apply(oldconf.get());
	if (error != 0)
		log_errx(1, "failed to apply configuration; exiting");

	oldconf.reset();

	newconf->write_pidfile();

	newconf->isns_schedule_update();

	for (;;) {
		main_loop(!daemonize);
		if (sighup_received) {
			sighup_received = false;
			log_debugx("received SIGHUP, reloading configuration");
			conf_up tmpconf = conf_new_from_file(config_path,
			    use_ucl);

			if (tmpconf == NULL) {
				log_warnx("configuration error, "
				    "continuing with old configuration");
			} else if (!tmpconf->add_pports(kports)) {
				log_warnx("Error associating physical ports, "
				    "continuing with old configuration");
			} else {
				if (debug > 0)
					tmpconf->set_debug(debug);
				oldconf = std::move(newconf);
				newconf = std::move(tmpconf);

				error = newconf->apply(oldconf.get());
				if (error != 0)
					log_warnx("failed to reload "
					    "configuration");
				oldconf.reset();
			}
		} else if (sigterm_received) {
			log_debugx("exiting on signal; "
			    "reloading empty configuration");

			log_debugx("removing CTL iSCSI ports "
			    "and terminating all connections");

			oldconf = std::move(newconf);
			newconf = std::make_unique<conf>();
			if (debug > 0)
				newconf->set_debug(debug);
			error = newconf->apply(oldconf.get());
			if (error != 0)
				log_warnx("failed to apply configuration");
			oldconf.reset();

			log_warnx("exiting on signal");
			return (0);
		} else {
			nchildren -= wait_for_children(false);
			assert(nchildren >= 0);
			if (timed_out()) {
				newconf->isns_update();
			}
		}
	}
	/* NOTREACHED */
}
