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
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cam/scsi/scsi_all.h>

#include <libutil++>

#include "conf.h"
#include "ctld.h"
#include "isns.h"

static bool	timed_out(void);
#ifdef ICL_KERNEL_PROXY
static void	pdu_receive_proxy(struct pdu *pdu);
static void	pdu_send_proxy(struct pdu *pdu);
#endif /* ICL_KERNEL_PROXY */
static void	pdu_fail(const struct connection *conn, const char *reason);

bool proxy_mode = false;

static volatile bool sighup_received = false;
static volatile bool sigterm_received = false;
static volatile bool sigalrm_received = false;

static int kqfd;
static int nchildren = 0;
static uint16_t last_portal_group_tag = 0xff;

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

static void
usage(void)
{

	fprintf(stderr, "usage: ctld [-d][-u][-f config-file]\n");
	fprintf(stderr, "       ctld -t [-u][-f config-file]\n");
	exit(1);
}

struct conf *
conf_new(void)
{
	struct conf *conf;

	conf = new struct conf();
	TAILQ_INIT(&conf->conf_luns);
	TAILQ_INIT(&conf->conf_targets);

	conf->conf_isns_period = 900;
	conf->conf_isns_timeout = 5;
	conf->conf_debug = 0;
	conf->conf_timeout = 60;
	conf->conf_maxproc = 30;

	return (conf);
}

void
conf_delete(struct conf *conf)
{
	struct lun *lun, *ltmp;
	struct target *targ, *tmp;

	assert(conf->conf_pidfh == NULL);

	TAILQ_FOREACH_SAFE(lun, &conf->conf_luns, l_next, ltmp)
		lun_delete(lun);
	TAILQ_FOREACH_SAFE(targ, &conf->conf_targets, t_next, tmp)
		target_delete(targ);
	free(conf->conf_pidfile_path);
	delete conf;
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
	int type;

	if (strcmp(str, "none") == 0) {
		type = AG_TYPE_NO_AUTHENTICATION;
	} else if (strcmp(str, "deny") == 0) {
		type = AG_TYPE_DENY;
	} else if (strcmp(str, "chap") == 0) {
		type = AG_TYPE_CHAP;
	} else if (strcmp(str, "chap-mutual") == 0) {
		type = AG_TYPE_CHAP_MUTUAL;
	} else {
		log_warnx("invalid auth-type \"%s\" for %s", str, label());
		return (false);
	}

	if (ag_type != AG_TYPE_UNKNOWN && ag_type != type) {
		log_warnx("cannot set auth-type to \"%s\" for %s; "
		    "already has a different type", str, label());
		return (false);
	}

	ag_type = type;

	return (true);
}

void
auth_group::set_type(int type)
{
	assert(ag_type == AG_TYPE_UNKNOWN);

	ag_type = type;
}

const struct auth *
auth_group::find_auth(const char *user) const
{
	auto it = ag_auths.find(user);
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
	if (ag_type == AG_TYPE_UNKNOWN)
		ag_type = AG_TYPE_CHAP;
	if (ag_type != AG_TYPE_CHAP) {
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
	if (ag_type == AG_TYPE_UNKNOWN)
		ag_type = AG_TYPE_CHAP_MUTUAL;
	if (ag_type != AG_TYPE_CHAP_MUTUAL) {
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
auth_group::add_initiator_name(const char *name)
{
	/* Silently ignore duplicates. */
	ag_names.emplace(name);
	return (true);
}

bool
auth_group::initiator_permitted(const char *initiator_name) const
{
	if (ag_names.empty())
		return (true);

	return (ag_names.count(initiator_name) != 0);
}

bool
auth_group::add_initiator_portal(const char *portal)
{
	struct sockaddr_storage ss;
	char *net, *mask, *tmp;
	int len, dm, m;

	freebsd::malloc_up<char> str(strdup(portal));
	if (str == nullptr)
		throw std::bad_alloc();

	mask = str.get();
	net = strsep(&mask, "/");
	if (net[0] == '[') {
		net++;
		len = strlen(net);
		if (len < 2)
			goto error;
		if (net[len - 1] != ']')
			goto error;
		net[len - 1] = 0;
	} else if (net[0] == '\0')
		goto error;
	if (str.get()[0] == '[' || strchr(net, ':') != NULL) {
		struct sockaddr_in6 *sin6 =
		    (struct sockaddr_in6 *)&ss;

		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, net, &sin6->sin6_addr) <= 0)
			goto error;
		dm = 128;
	} else {
		struct sockaddr_in *sin =
		    (struct sockaddr_in *)&ss;

		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		if (inet_pton(AF_INET, net, &sin->sin_addr) <= 0)
			goto error;
		dm = 32;
	}
	if (mask != NULL) {
		if (mask[0] == '\0')
			goto error;
		m = strtol(mask, &tmp, 0);
		if (m < 0 || m > dm || tmp[0] != 0)
			goto error;
	} else
		m = dm;

	ag_portals.emplace_back(&ss, m);
	return (true);

error:
	log_warnx("invalid initiator portal \"%s\" for %s", portal, label());
	return (false);
}

bool
auth_portal::matches(const struct sockaddr_storage *ss) const
{
	const uint8_t *a, *b;
	int i;

	if (ap_sa.ss_family != ss->ss_family)
		return (false);

	if (ss->ss_family == AF_INET) {
		a = (const uint8_t *)
		    &((const struct sockaddr_in *)ss)->sin_addr;
		b = (const uint8_t *)
		    &((const struct sockaddr_in *)&ap_sa)->sin_addr;
	} else {
		a = (const uint8_t *)
		    &((const struct sockaddr_in6 *)ss)->sin6_addr;
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
auth_group::initiator_permitted(const struct sockaddr_storage *sa) const
{
	if (ag_portals.empty())
		return (true);

	for (const auth_portal &ap : ag_portals)
		if (ap.matches(sa))
			return (true);
	return (false);
}

struct auth_group *
auth_group_new(struct conf *conf, const char *name)
{
	const auto &pair = conf->conf_auth_groups.try_emplace(name,
	    std::make_shared<auth_group>(freebsd::stringf("auth-group \"%s\"",
	    name)));
	if (!pair.second) {
		log_warnx("duplicated auth-group \"%s\"", name);
		return (NULL);
	}

	return (pair.first->second.get());
}

auth_group_sp
auth_group_new(struct target *target)
{
	return (std::make_shared<auth_group>(freebsd::stringf("target \"%s\"",
	    target->t_name)));
}

auth_group_sp
auth_group_find(const struct conf *conf, const char *name)
{
	auto it = conf->conf_auth_groups.find(name);
	if (it == conf->conf_auth_groups.end())
		return {};

	return (it->second);
}

portal_group::portal_group(struct conf *conf, const char *name)
	: pg_conf(conf), pg_options(nvlist_create(0)), pg_name(name)
{
}

struct portal_group *
portal_group_new(struct conf *conf, const char *name)
{
	auto pair = conf->conf_portal_groups.try_emplace(name,
	    std::make_unique<portal_group>(conf, name));
	if (!pair.second) {
		log_warnx("duplicated portal-group \"%s\"", name);
		return (nullptr);
	}

	return (pair.first->second.get());
}

struct portal_group *
portal_group_find(struct conf *conf, const char *name)
{
	auto it = conf->conf_portal_groups.find(name);
	if (it == conf->conf_portal_groups.end())
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

static bool
parse_addr_port(const char *address, const char *def_port, struct addrinfo **ai)
{
	struct addrinfo hints;
	char *addr, *arg, *ch;
	const char *port;
	int error, colons = 0;

	freebsd::malloc_up<char> str(strdup(address));
	arg = str.get();
	if (arg[0] == '[') {
		/*
		 * IPv6 address in square brackets, perhaps with port.
		 */
		arg++;
		addr = strsep(&arg, "]");
		if (arg == NULL) {
			return (false);
		}
		if (arg[0] == '\0') {
			port = def_port;
		} else if (arg[0] == ':') {
			port = arg + 1;
		} else {
			return (false);
		}
	} else {
		/*
		 * Either IPv6 address without brackets - and without
		 * a port - or IPv4 address.  Just count the colons.
		 */
		for (ch = arg; *ch != '\0'; ch++) {
			if (*ch == ':')
				colons++;
		}
		if (colons > 1) {
			addr = arg;
			port = def_port;
		} else {
			addr = strsep(&arg, ":");
			if (arg == NULL)
				port = def_port;
			else
				port = arg;
		}
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(addr, port, &hints, ai);
	return (error == 0);
}

void
portal_group::add_port(struct portal_group_port *port)
{
	pg_ports.emplace(port->target()->t_name, port);
}

void
portal_group::remove_port(struct portal_group_port *port)
{
	auto it = pg_ports.find(port->target()->t_name);
	pg_ports.erase(it);
}

freebsd::nvlist_up
portal_group::options() const
{
	return (freebsd::nvlist_up(nvlist_clone(pg_options.get())));
}

bool
portal_group::add_portal(const char *value, bool iser)
{
	struct addrinfo *ai;

	if (!parse_addr_port(value, "3260", &ai)) {
		log_warnx("invalid listen address %s", value);
		return (false);
	}

	/*
	 * XXX: getaddrinfo(3) may return multiple addresses; we should turn
	 *	those into multiple portals.
	 */

	pg_portals.emplace_back(std::make_unique<portal>(this, value, iser, ai));
	return (true);
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
		log_warnx("discovery-auth-group for portal-group "
		    "\"%s\" specified more than once", name());
		return (false);
	}
	pg_discovery_auth_group = auth_group_find(pg_conf, ag_name);
	if (pg_discovery_auth_group == nullptr) {
		log_warnx("unknown discovery-auth-group \"%s\" "
		    "for portal-group \"%s\"", ag_name, name());
		return (false);
	}
	return (true);
}

bool
portal_group::set_dscp(u_int dscp)
{
	if (dscp >= 0x40) {
		log_warnx("invalid DSCP value %u for portal-group \"%s\"",
		    dscp, name());
		return (false);
	}

	pg_dscp = dscp;
	return (true);
}

bool
portal_group::set_filter(const char *str)
{
	int filter;

	if (strcmp(str, "none") == 0) {
		filter = PG_FILTER_NONE;
	} else if (strcmp(str, "portal") == 0) {
		filter = PG_FILTER_PORTAL;
	} else if (strcmp(str, "portal-name") == 0) {
		filter = PG_FILTER_PORTAL_NAME;
	} else if (strcmp(str, "portal-name-auth") == 0) {
		filter = PG_FILTER_PORTAL_NAME_AUTH;
	} else {
		log_warnx("invalid discovery-filter \"%s\" for portal-group "
		    "\"%s\"; valid values are \"none\", \"portal\", "
		    "\"portal-name\", and \"portal-name-auth\"",
		    str, name());
		return (false);
	}

	if (pg_discovery_filter != PG_FILTER_UNKNOWN &&
	    pg_discovery_filter != filter) {
		log_warnx("cannot set discovery-filter to \"%s\" for "
		    "portal-group \"%s\"; already has a different "
		    "value", str, name());
		return (false);
	}

	pg_discovery_filter = filter;
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
		    "portal-group \"%s\"; already defined",
		    offload, name());
		return (false);
	}

	pg_offload = offload;
	return (true);
}

bool
portal_group::set_pcp(u_int pcp)
{
	if (pcp > 7) {
		log_warnx("invalid PCP value %u for portal-group \"%s\"",
		    pcp, name());
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
		    "portal-group \"%s\"; already defined",
		    addr, name());
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
		pg_discovery_auth_group =  auth_group_find(conf, "default");
		assert(pg_discovery_auth_group != nullptr);
	}

	if (pg_discovery_filter == PG_FILTER_UNKNOWN)
		pg_discovery_filter = PG_FILTER_NONE;

	if (!pg_redirection.empty()) {
		if (!pg_ports.empty()) {
			log_debugx("portal-group \"%s\" assigned to target, "
			    "but configured for redirection", name());
		}
		pg_assigned = true;
	} else if (!pg_ports.empty()) {
		pg_assigned = true;
	} else {
		if (pg_name == "default")
			log_warnx("portal-group \"%s\" not assigned "
			    "to any target", name());
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
		log_debugx("not listening on portal-group \"%s\", "
		    "not assigned to any target", name());
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
		log_debugx("closing socket for %s, portal-group \"%s\"",
		    portal->listen(), name());
		portal->close();
	}
}

bool
isns_new(struct conf *conf, const char *addr)
{
	struct addrinfo *ai;

	if (conf->conf_isns.count(addr) > 0) {
		log_warnx("duplicate iSNS address %s", addr);
		return (false);
	}

	if (!parse_addr_port(addr, "3205", &ai)) {
		log_warnx("invalid iSNS address %s", addr);
		return (false);
	}

	/*
	 * XXX: getaddrinfo(3) may return multiple addresses; we should turn
	 *	those into multiple servers.
	 */

	conf->conf_isns.emplace(addr, isns(addr, ai));
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

static struct isns_req
isns_register_request(struct conf *conf, const char *hostname)
{
	struct target *target;
	const struct portal_group *pg;

	isns_req req(ISNS_FUNC_DEVATTRREG, ISNS_FLAG_CLIENT, "register");
	req.add_str(32, TAILQ_FIRST(&conf->conf_targets)->t_name);
	req.add_delim();
	req.add_str(1, hostname);
	req.add_32(2, 2); /* 2 -- iSCSI */
	req.add_32(6, conf->conf_isns_period);
	for (const auto &kv : conf->conf_portal_groups) {
		pg = kv.second.get();

		if (!pg->assigned())
			continue;
		for (const portal_up &portal : pg->portals()) {
			req.add_addr(16, portal->ai());
			req.add_port(17, portal->ai());
		}
	}
	TAILQ_FOREACH(target, &conf->conf_targets, t_next) {
		req.add_str(32, target->t_name);
		req.add_32(33, 1); /* 1 -- Target*/
		if (target->t_alias != NULL)
			req.add_str(34, target->t_alias);
		for (const port *port : target->t_ports) {
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

static struct isns_req
isns_check_request(struct conf *conf, const char *hostname)
{
	isns_req req(ISNS_FUNC_DEVATTRQRY, ISNS_FLAG_CLIENT, "check");
	req.add_str(32, TAILQ_FIRST(&conf->conf_targets)->t_name);
	req.add_str(1, hostname);
	req.add_delim();
	req.add(2, 0, NULL);
	return (req);
}

static struct isns_req
isns_deregister_request(struct conf *conf, const char *hostname)
{
	isns_req req(ISNS_FUNC_DEVDEREG, ISNS_FLAG_CLIENT, "deregister");
	req.add_str(32, TAILQ_FIRST(&conf->conf_targets)->t_name);
	req.add_delim();
	req.add_str(1, hostname);
	return (req);
}

void
isns_register_targets(struct conf *conf, struct isns *isns,
    struct conf *oldconf)
{
	int error;
	char hostname[256];

	if (TAILQ_EMPTY(&conf->conf_targets) ||
	    conf->conf_portal_groups.empty())
		return;
	set_timeout(conf->conf_isns_timeout, false);
	freebsd::fd_up s = isns->connect();
	if (!s) {
		set_timeout(0, false);
		return;
	}
	error = gethostname(hostname, sizeof(hostname));
	if (error != 0)
		log_err(1, "gethostname");

	if (oldconf == NULL || TAILQ_EMPTY(&oldconf->conf_targets))
		oldconf = conf;
	isns->send_request(s, isns_deregister_request(oldconf, hostname));
	isns->send_request(s, isns_register_request(conf, hostname));
	s.reset();
	set_timeout(0, false);
}

void
isns_check(struct conf *conf, struct isns *isns)
{
	int error;
	char hostname[256];

	if (TAILQ_EMPTY(&conf->conf_targets) ||
	    conf->conf_portal_groups.empty())
		return;
	set_timeout(conf->conf_isns_timeout, false);
	freebsd::fd_up s = isns->connect();
	if (!s) {
		set_timeout(0, false);
		return;
	}
	error = gethostname(hostname, sizeof(hostname));
	if (error != 0)
		log_err(1, "gethostname");

	if (!isns->send_request(s, isns_check_request(conf, hostname))) {
		isns->send_request(s, isns_deregister_request(conf, hostname));
		isns->send_request(s, isns_register_request(conf, hostname));
	}
	s.reset();
	set_timeout(0, false);
}

void
isns_deregister_targets(struct conf *conf, struct isns *isns)
{
	int error;
	char hostname[256];

	if (TAILQ_EMPTY(&conf->conf_targets) ||
	    conf->conf_portal_groups.empty())
		return;
	set_timeout(conf->conf_isns_timeout, false);
	freebsd::fd_up s = isns->connect();
	if (!s)
		return;
	error = gethostname(hostname, sizeof(hostname));
	if (error != 0)
		log_err(1, "gethostname");

	isns->send_request(s, isns_deregister_request(conf, hostname));
	s.reset();
	set_timeout(0, false);
}

bool
kports::add_port(const char *name, uint32_t ctl_port)
{
	const auto &pair = pports.try_emplace(name, name, ctl_port);
	if (!pair.second) {
		log_warnx("duplicate kernel port \"%s\" (%u)", name, ctl_port);
		return (false);
	}

	return (true);
}

bool
kports::has_port(const char *name)
{
	return (pports.count(name) > 0);
}

struct pport *
kports::find_port(const char *name)
{
	auto it = pports.find(name);
	if (it == pports.end())
		return (nullptr);
	return (&it->second);
}

port::port(struct target *target)
    : p_target(target)
{
	target->t_ports.push_back(this);
}

void
port::clear_references()
{
	p_target->t_ports.remove(this);
}

portal_group_port::portal_group_port(struct target *target,
    struct portal_group *pg, auth_group_sp ag, uint32_t ctl_port) :
	port(target), p_auth_group(ag), p_portal_group(pg)
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
port_new(struct conf *conf, struct target *target, struct portal_group *pg,
    auth_group_sp ag)
{
	std::string name = freebsd::stringf("%s-%s", pg->name(),
	    target->t_name);
	const auto &pair = conf->conf_ports.try_emplace(name,
	    std::make_unique<portal_group_port>(target, pg, ag, 0));
	if (!pair.second) {
		log_warnx("duplicate port \"%s\"", name.c_str());
		return (false);
	}

	return (true);
}

bool
port_new(struct conf *conf, struct target *target, struct portal_group *pg,
    uint32_t ctl_port)
{
	std::string name = freebsd::stringf("%s-%s", pg->name(),
	    target->t_name);
	const auto &pair = conf->conf_ports.try_emplace(name,
	    std::make_unique<portal_group_port>(target, pg, nullptr, ctl_port));
	if (!pair.second) {
		log_warnx("duplicate port \"%s\"", name.c_str());
		return (false);
	}

	return (true);
}

static bool
port_new_pp(struct conf *conf, struct target *target, struct pport *pp)
{
	std::string name = freebsd::stringf("%s-%s", pp->name(),
	    target->t_name);
	const auto &pair = conf->conf_ports.try_emplace(name,
	    std::make_unique<kernel_port>(target, pp));
	if (!pair.second) {
		log_warnx("duplicate port \"%s\"", name.c_str());
		return (false);
	}

	pp->link();
	return (true);
}

static bool
port_new_ioctl(struct conf *conf, struct kports &kports, struct target *target,
    int pp, int vp)
{
	struct pport *pport;

	std::string pname = freebsd::stringf("ioctl/%d/%d", pp, vp);

	pport = kports.find_port(pname.c_str());
	if (pport != NULL)
		return (port_new_pp(conf, target, pport));

	std::string name = pname + "-" + target->t_name;
	const auto &pair = conf->conf_ports.try_emplace(name,
	    std::make_unique<ioctl_port>(target, pp, vp));
	if (!pair.second) {
		log_warnx("duplicate port \"%s\"", name.c_str());
		return (false);
	}

	return (true);
}

const struct port *
portal_group::find_port(const char *target) const
{
	auto it = pg_ports.find(target);
	if (it == pg_ports.end())
		return (nullptr);
	return (it->second);
}

struct target *
target_new(struct conf *conf, const char *name)
{
	struct target *targ;
	int i, len;

	targ = target_find(conf, name);
	if (targ != NULL) {
		log_warnx("duplicated target \"%s\"", name);
		return (NULL);
	}
	if (valid_iscsi_name(name, log_warnx) == false) {
		return (NULL);
	}
	targ = new target();
	targ->t_name = checked_strdup(name);

	/*
	 * RFC 3722 requires us to normalize the name to lowercase.
	 */
	len = strlen(name);
	for (i = 0; i < len; i++)
		targ->t_name[i] = tolower(targ->t_name[i]);

	targ->t_conf = conf;
	TAILQ_INSERT_TAIL(&conf->conf_targets, targ, t_next);

	return (targ);
}

void
target_delete(struct target *targ)
{
	TAILQ_REMOVE(&targ->t_conf->conf_targets, targ, t_next);

	free(targ->t_pport);
	free(targ->t_name);
	free(targ->t_redirection);
	delete targ;
}

struct target *
target_find(struct conf *conf, const char *name)
{
	struct target *targ;

	TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
		if (strcasecmp(targ->t_name, name) == 0)
			return (targ);
	}

	return (NULL);
}

struct lun *
lun_new(struct conf *conf, const char *name)
{
	struct lun *lun;

	lun = lun_find(conf, name);
	if (lun != NULL) {
		log_warnx("duplicated lun \"%s\"", name);
		return (NULL);
	}

	lun = reinterpret_cast<struct lun *>(calloc(1, sizeof(*lun)));
	if (lun == NULL)
		log_err(1, "calloc");
	lun->l_conf = conf;
	lun->l_name = checked_strdup(name);
	lun->l_options = nvlist_create(0);
	TAILQ_INSERT_TAIL(&conf->conf_luns, lun, l_next);
	lun->l_ctl_lun = -1;

	return (lun);
}

void
lun_delete(struct lun *lun)
{
	struct target *targ;
	int i;

	TAILQ_FOREACH(targ, &lun->l_conf->conf_targets, t_next) {
		for (i = 0; i < MAX_LUNS; i++) {
			if (targ->t_luns[i] == lun)
				targ->t_luns[i] = NULL;
		}
	}
	TAILQ_REMOVE(&lun->l_conf->conf_luns, lun, l_next);

	nvlist_destroy(lun->l_options);
	free(lun->l_name);
	free(lun->l_backend);
	free(lun->l_device_id);
	free(lun->l_path);
	free(lun->l_scsiname);
	free(lun->l_serial);
	free(lun);
}

struct lun *
lun_find(const struct conf *conf, const char *name)
{
	struct lun *lun;

	TAILQ_FOREACH(lun, &conf->conf_luns, l_next) {
		if (strcmp(lun->l_name, name) == 0)
			return (lun);
	}

	return (NULL);
}

void
lun_set_scsiname(struct lun *lun, const char *value)
{
	free(lun->l_scsiname);
	lun->l_scsiname = checked_strdup(value);
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

static struct ctld_connection *
connection_new(struct portal *portal, int fd, const char *host,
    const struct sockaddr *client_sa)
{
	struct ctld_connection *conn;

	conn = reinterpret_cast<struct ctld_connection *>(calloc(1, sizeof(*conn)));
	if (conn == NULL)
		log_err(1, "calloc");
	connection_init(&conn->conn, &conn_ops, proxy_mode);
	conn->conn.conn_socket = fd;
	conn->conn_portal = portal;
	conn->conn_initiator_addr = checked_strdup(host);
	memcpy(&conn->conn_initiator_sa, client_sa, client_sa->sa_len);

	return (conn);
}

static bool
conf_verify_lun(struct lun *lun)
{
	const struct lun *lun2;

	if (lun->l_backend == NULL)
		lun->l_backend = checked_strdup("block");
	if (strcmp(lun->l_backend, "block") == 0) {
		if (lun->l_path == NULL) {
			log_warnx("missing path for lun \"%s\"",
			    lun->l_name);
			return (false);
		}
	} else if (strcmp(lun->l_backend, "ramdisk") == 0) {
		if (lun->l_size == 0) {
			log_warnx("missing size for ramdisk-backed lun \"%s\"",
			    lun->l_name);
			return (false);
		}
		if (lun->l_path != NULL) {
			log_warnx("path must not be specified "
			    "for ramdisk-backed lun \"%s\"",
			    lun->l_name);
			return (false);
		}
	}
	if (lun->l_blocksize == 0) {
		if (lun->l_device_type == T_CDROM)
			lun->l_blocksize = DEFAULT_CD_BLOCKSIZE;
		else
			lun->l_blocksize = DEFAULT_BLOCKSIZE;
	} else if (lun->l_blocksize < 0) {
		log_warnx("invalid blocksize for lun \"%s\"; "
		    "must be larger than 0", lun->l_name);
		return (false);
	}
	if (lun->l_size != 0 && lun->l_size % lun->l_blocksize != 0) {
		log_warnx("invalid size for lun \"%s\"; "
		    "must be multiple of blocksize", lun->l_name);
		return (false);
	}
	TAILQ_FOREACH(lun2, &lun->l_conf->conf_luns, l_next) {
		if (lun == lun2)
			continue;
		if (lun->l_path != NULL && lun2->l_path != NULL &&
		    strcmp(lun->l_path, lun2->l_path) == 0) {
			log_debugx("WARNING: path \"%s\" duplicated "
			    "between lun \"%s\", and "
			    "lun \"%s\"", lun->l_path,
			    lun->l_name, lun2->l_name);
		}
	}

	return (true);
}

bool
conf_verify(struct conf *conf)
{
	struct portal_group *pg;
	struct target *targ;
	struct lun *lun;
	bool found;
	int i;

	if (conf->conf_pidfile_path == NULL)
		conf->conf_pidfile_path = checked_strdup(DEFAULT_PIDFILE);

	TAILQ_FOREACH(lun, &conf->conf_luns, l_next) {
		if (!conf_verify_lun(lun))
			return (false);
	}
	TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
		if (targ->t_auth_group == NULL) {
			targ->t_auth_group = auth_group_find(conf,
			    "default");
			assert(targ->t_auth_group != NULL);
		}
		if (targ->t_ports.empty()) {
			pg = portal_group_find(conf, "default");
			assert(pg != NULL);
			port_new(conf, targ, pg, nullptr);
		}
		found = false;
		for (i = 0; i < MAX_LUNS; i++) {
			if (targ->t_luns[i] != NULL)
				found = true;
		}
		if (!found && targ->t_redirection == NULL) {
			log_warnx("no LUNs defined for target \"%s\"",
			    targ->t_name);
		}
		if (found && targ->t_redirection != NULL) {
			log_debugx("target \"%s\" contains luns, "
			    " but configured for redirection",
			    targ->t_name);
		}
	}
	for (auto &kv : conf->conf_portal_groups) {
		kv.second->verify(conf);
	}
	for (const auto &kv : conf->conf_auth_groups) {
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
	int error, sockbuf;
	int one = 1;

#ifdef ICL_KERNEL_PROXY
	if (proxy_mode) {
		int id = pg->conf()->add_proxy_portal(this);
		log_debugx("listening on %s, portal-group \"%s\", "
		    "portal id %d, using ICL proxy", listen(), pg->pg_name,
		    id);
		kernel_listen(ai(), p_iser, id);
		return (true);
	}
#endif
	assert(proxy_mode == false);
	assert(p_iser == false);

	log_debugx("listening on %s, portal-group \"%s\"", listen(),
	    pg->name());
	s = ::socket(p_ai->ai_family, p_ai->ai_socktype, p_ai->ai_protocol);
	if (!s) {
		log_warn("socket(2) failed for %s", listen());
		return (false);
	}

	sockbuf = SOCKBUF_SIZE;
	if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sockbuf,
	    sizeof(sockbuf)) == -1)
		log_warn("setsockopt(SO_RCVBUF) failed for %s", listen());
	sockbuf = SOCKBUF_SIZE;
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sockbuf,
	    sizeof(sockbuf)) == -1)
		log_warn("setsockopt(SO_SNDBUF) failed for %s", listen());
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
	return (false);
}

static int
conf_apply(struct conf *oldconf, struct conf *newconf)
{
	struct lun *oldlun, *newlun, *tmplun;
	int changed, cumulated_error = 0, error;

	if (oldconf->conf_debug != newconf->conf_debug) {
		log_debugx("changing debug level to %d", newconf->conf_debug);
		log_init(newconf->conf_debug);
	}

	if (oldconf->conf_pidfile_path != NULL &&
	    newconf->conf_pidfile_path != NULL)
	{
		if (strcmp(oldconf->conf_pidfile_path,
		           newconf->conf_pidfile_path) != 0)
		{
			/* pidfile has changed.  rename it */
			log_debugx("moving pidfile to %s",
				newconf->conf_pidfile_path);
			if (rename(oldconf->conf_pidfile_path,
				   newconf->conf_pidfile_path))
			{
				log_err(1, "renaming pidfile %s -> %s",
					oldconf->conf_pidfile_path,
					newconf->conf_pidfile_path);
			}
		}
		newconf->conf_pidfh = oldconf->conf_pidfh;
		oldconf->conf_pidfh = NULL;
	}

	/*
	 * Go through the new portal groups, assigning tags or preserving old.
	 */
	for (auto &kv : newconf->conf_portal_groups) {
		struct portal_group &newpg = *kv.second;

		if (newpg.tag() != 0)
			continue;
		auto it = oldconf->conf_portal_groups.find(kv.first);
		if (it != oldconf->conf_portal_groups.end())
			newpg.set_tag(it->second->tag());
		else
			newpg.set_tag(++last_portal_group_tag);
	}

	/* Deregister on removed iSNS servers. */
	for (auto &kv : oldconf->conf_isns) {
		if (newconf->conf_isns.count(kv.first) == 0)
			isns_deregister_targets(oldconf, &kv.second);
	}

	/*
	 * XXX: If target or lun removal fails, we should somehow "move"
	 *      the old lun or target into newconf, so that subsequent
	 *      conf_apply() would try to remove them again.  That would
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
		const auto it = newconf->conf_ports.find(name);
		if (it == newconf->conf_ports.end())
			continue;
		if (it->second->is_dummy())
			continue;
		log_debugx("removing port \"%s\"", name.c_str());
		error = oldport->kernel_remove();
		if (error != 0) {
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
	TAILQ_FOREACH_SAFE(oldlun, &oldconf->conf_luns, l_next, tmplun) {
		newlun = lun_find(newconf, oldlun->l_name);
		if (newlun == NULL) {
			log_debugx("lun \"%s\", CTL lun %d "
			    "not found in new configuration; "
			    "removing", oldlun->l_name, oldlun->l_ctl_lun);
			error = kernel_lun_remove(oldlun);
			if (error != 0) {
				log_warnx("failed to remove lun \"%s\", "
				    "CTL lun %d",
				    oldlun->l_name, oldlun->l_ctl_lun);
				cumulated_error++;
			}
			continue;
		}

		/*
		 * Also remove the LUNs changed by more than size.
		 */
		changed = 0;
		assert(oldlun->l_backend != NULL);
		assert(newlun->l_backend != NULL);
		if (strcmp(newlun->l_backend, oldlun->l_backend) != 0) {
			log_debugx("backend for lun \"%s\", "
			    "CTL lun %d changed; removing",
			    oldlun->l_name, oldlun->l_ctl_lun);
			changed = 1;
		}
		if (oldlun->l_blocksize != newlun->l_blocksize) {
			log_debugx("blocksize for lun \"%s\", "
			    "CTL lun %d changed; removing",
			    oldlun->l_name, oldlun->l_ctl_lun);
			changed = 1;
		}
		if (newlun->l_device_id != NULL &&
		    (oldlun->l_device_id == NULL ||
		     strcmp(oldlun->l_device_id, newlun->l_device_id) !=
		     0)) {
			log_debugx("device-id for lun \"%s\", "
			    "CTL lun %d changed; removing",
			    oldlun->l_name, oldlun->l_ctl_lun);
			changed = 1;
		}
		if (newlun->l_path != NULL &&
		    (oldlun->l_path == NULL ||
		     strcmp(oldlun->l_path, newlun->l_path) != 0)) {
			log_debugx("path for lun \"%s\", "
			    "CTL lun %d, changed; removing",
			    oldlun->l_name, oldlun->l_ctl_lun);
			changed = 1;
		}
		if (newlun->l_serial != NULL &&
		    (oldlun->l_serial == NULL ||
		     strcmp(oldlun->l_serial, newlun->l_serial) != 0)) {
			log_debugx("serial for lun \"%s\", "
			    "CTL lun %d changed; removing",
			    oldlun->l_name, oldlun->l_ctl_lun);
			changed = 1;
		}
		if (changed) {
			error = kernel_lun_remove(oldlun);
			if (error != 0) {
				log_warnx("failed to remove lun \"%s\", "
				    "CTL lun %d",
				    oldlun->l_name, oldlun->l_ctl_lun);
				cumulated_error++;
			}
			lun_delete(oldlun);
			continue;
		}

		newlun->l_ctl_lun = oldlun->l_ctl_lun;
	}

	TAILQ_FOREACH_SAFE(newlun, &newconf->conf_luns, l_next, tmplun) {
		oldlun = lun_find(oldconf, newlun->l_name);
		if (oldlun != NULL) {
			log_debugx("modifying lun \"%s\", CTL lun %d",
			    newlun->l_name, newlun->l_ctl_lun);
			error = kernel_lun_modify(newlun);
			if (error != 0) {
				log_warnx("failed to "
				    "modify lun \"%s\", CTL lun %d",
				    newlun->l_name, newlun->l_ctl_lun);
				cumulated_error++;
			}
			continue;
		}
		log_debugx("adding lun \"%s\"", newlun->l_name);
		error = kernel_lun_add(newlun);
		if (error != 0) {
			log_warnx("failed to add lun \"%s\"", newlun->l_name);
			lun_delete(newlun);
			cumulated_error++;
		}
	}

	/*
	 * Now add new ports or modify existing ones.
	 */
	for (auto it = newconf->conf_ports.begin();
	     it != newconf->conf_ports.end(); ) {
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
				it = newconf->conf_ports.erase(it);
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
	for (auto &kv : newconf->conf_portal_groups) {
		cumulated_error += kv.second->open_sockets(*oldconf);
	}

	/*
	 * Go through the no longer used sockets, closing them.
	 */
	for (auto &kv : oldconf->conf_portal_groups) {
		kv.second->close_sockets();
	}

	/* (Re-)Register on remaining/new iSNS servers. */
	for (auto &kv : newconf->conf_isns) {
		auto it = oldconf->conf_isns.find(kv.first);
		if (it == oldconf->conf_isns.end())
			isns_register_targets(newconf, &kv.second, nullptr);
		else
			isns_register_targets(newconf, &kv.second, oldconf);
	}

	/* Schedule iSNS update */
	if (!newconf->conf_isns.empty())
		set_timeout((newconf->conf_isns_period + 2) / 3, false);

	return (cumulated_error);
}

static bool
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
set_timeout(int timeout, int fatal)
{
	struct sigaction sa;
	struct itimerval itv;
	int error;

	if (timeout <= 0) {
		log_debugx("session timeout disabled");
		bzero(&itv, sizeof(itv));
		error = setitimer(ITIMER_REAL, &itv, NULL);
		if (error != 0)
			log_err(1, "setitimer");
		sigalrm_received = false;
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
	 * First SIGALRM will arive after conf_timeout seconds.
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
handle_connection(struct portal *portal, int fd,
    const struct sockaddr *client_sa, bool dont_fork)
{
	struct ctld_connection *conn;
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

		while (conf->conf_maxproc > 0 && nchildren >= conf->conf_maxproc) {
			log_debugx("maxproc limit of %d child processes hit; "
			    "waiting for child process to exit", conf->conf_maxproc);
			nchildren -= wait_for_children(true);
			assert(nchildren >= 0);
		}
		log_debugx("incoming connection; forking child process #%d",
		    nchildren);
		nchildren++;
		pid = fork();
		if (pid < 0)
			log_err(1, "fork");
		if (pid > 0) {
			close(fd);
			return;
		}
		pidfile_close(conf->conf_pidfh);
	}

	error = getnameinfo(client_sa, client_sa->sa_len,
	    host, sizeof(host), NULL, 0, NI_NUMERICHOST);
	if (error != 0)
		log_errx(1, "getnameinfo: %s", gai_strerror(error));

	log_debugx("accepted connection from %s; portal group \"%s\"",
	    host, pg->name());
	log_set_peer_addr(host);
	setproctitle("%s", host);

	conn = connection_new(portal, fd, host, client_sa);
	set_timeout(conf->conf_timeout, true);
	kernel_capsicate();
	login(conn);
	if (conn->conn_session_type == CONN_SESSION_TYPE_NORMAL) {
		kernel_handoff(conn);
		log_debugx("connection handed off to the kernel");
	} else {
		assert(conn->conn_session_type == CONN_SESSION_TYPE_DISCOVERY);
		discovery(conn);
	}
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

static struct conf *
conf_new_from_file(const char *path, bool ucl)
{
	struct conf *conf;
	struct auth_group *ag;
	struct portal_group *pg;
	bool valid;

	log_debugx("obtaining configuration from %s", path);

	conf = conf_new();

	ag = auth_group_new(conf, "default");
	assert(ag != NULL);

	ag = auth_group_new(conf, "no-authentication");
	assert(ag != NULL);
	ag->set_type(AG_TYPE_NO_AUTHENTICATION);

	ag = auth_group_new(conf, "no-access");
	assert(ag != NULL);
	ag->set_type(AG_TYPE_DENY);

	pg = portal_group_new(conf, "default");
	assert(pg != NULL);

	conf_start(conf);
	if (ucl)
		valid = uclparse_conf(path);
	else
		valid = parse_conf(path);
	conf_finish();

	if (!valid) {
		conf_delete(conf);
		return (NULL);
	}

	check_perms(path);

	if (conf->conf_default_ag_defined == false) {
		log_debugx("auth-group \"default\" not defined; "
		    "going with defaults");
		ag = auth_group_find(conf, "default").get();
		assert(ag != NULL);
		ag->set_type(AG_TYPE_DENY);
	}

	if (conf->conf_default_pg_defined == false) {
		log_debugx("portal-group \"default\" not defined; "
		    "going with defaults");
		pg = portal_group_find(conf, "default");
		assert(pg != NULL);
		pg->add_portal("0.0.0.0", false);
		pg->add_portal("[::]", false);
	}

	conf->conf_kernel_port_on = true;

	if (!conf_verify(conf)) {
		conf_delete(conf);
		return (NULL);
	}

	return (conf);
}

/*
 * If the config file specifies physical ports for any target, associate them
 * with the config file.  If necessary, create them.
 */
static bool
new_pports_from_conf(struct conf *conf, struct kports &kports)
{
	struct target *targ;
	struct pport *pp;
	int ret, i_pp, i_vp;

	TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
		if (!targ->t_pport)
			continue;

		ret = sscanf(targ->t_pport, "ioctl/%d/%d", &i_pp, &i_vp);
		if (ret > 0) {
			if (!port_new_ioctl(conf, kports, targ, i_pp, i_vp)) {
				log_warnx("can't create new ioctl port "
				    "for target \"%s\"", targ->t_name);
				return (false);
			}

			continue;
		}

		pp = kports.find_port(targ->t_pport);
		if (pp == NULL) {
			log_warnx("unknown port \"%s\" for target \"%s\"",
			    targ->t_pport, targ->t_name);
			return (false);
		}
		if (pp->linked()) {
			log_warnx("can't link port \"%s\" to target \"%s\", "
			    "port already linked to some target",
			    targ->t_pport, targ->t_name);
			return (false);
		}
		if (!port_new_pp(conf, targ, pp)) {
			log_warnx("can't link port \"%s\" to target \"%s\"",
			    targ->t_pport, targ->t_name);
			return (false);
		}
	}
	return (true);
}

int
main(int argc, char **argv)
{
	struct kports kports;
	struct conf *oldconf, *newconf, *tmpconf;
	const char *config_path = DEFAULT_CONFIG_PATH;
	int debug = 0, ch, error;
	pid_t otherpid;
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

	newconf = conf_new_from_file(config_path, use_ucl);

	if (newconf == NULL)
		log_errx(1, "configuration error; exiting");

	if (test_config)
		return (0);

	assert(newconf->conf_pidfile_path != NULL);
	log_debugx("opening pidfile %s", newconf->conf_pidfile_path);
	newconf->conf_pidfh = pidfile_open(newconf->conf_pidfile_path, 0600,
		&otherpid);
	if (newconf->conf_pidfh == NULL) {
		if (errno == EEXIST)
			log_errx(1, "daemon already running, pid: %jd.",
			    (intmax_t)otherpid);
		log_err(1, "cannot open or create pidfile \"%s\"",
		    newconf->conf_pidfile_path);
	}

	register_signals();

	oldconf = conf_new_from_kernel(kports);

	if (debug > 0) {
		oldconf->conf_debug = debug;
		newconf->conf_debug = debug;
	}

	if (!new_pports_from_conf(newconf, kports))
		log_errx(1, "Error associating physical ports; exiting");

	if (daemonize) {
		log_debugx("daemonizing");
		if (daemon(0, 0) == -1) {
			log_warn("cannot daemonize");
			pidfile_remove(newconf->conf_pidfh);
			exit(1);
		}
	}

	kqfd = kqueue();
	if (kqfd == -1) {
		log_warn("Cannot create kqueue");
		pidfile_remove(newconf->conf_pidfh);
		exit(1);
	}

	error = conf_apply(oldconf, newconf);
	if (error != 0)
		log_errx(1, "failed to apply configuration; exiting");

	conf_delete(oldconf);
	oldconf = NULL;

	pidfile_write(newconf->conf_pidfh);

	/* Schedule iSNS update */
	if (!newconf->conf_isns.empty())
		set_timeout((newconf->conf_isns_period + 2) / 3, false);

	for (;;) {
		main_loop(!daemonize);
		if (sighup_received) {
			sighup_received = false;
			log_debugx("received SIGHUP, reloading configuration");
			tmpconf = conf_new_from_file(config_path, use_ucl);

			if (tmpconf == NULL) {
				log_warnx("configuration error, "
				    "continuing with old configuration");
			} else if (!new_pports_from_conf(tmpconf, kports)) {
				log_warnx("Error associating physical ports, "
				    "continuing with old configuration");
				conf_delete(tmpconf);
			} else {
				if (debug > 0)
					tmpconf->conf_debug = debug;
				oldconf = newconf;
				newconf = tmpconf;

				error = conf_apply(oldconf, newconf);
				if (error != 0)
					log_warnx("failed to reload "
					    "configuration");
				conf_delete(oldconf);
				oldconf = NULL;
			}
		} else if (sigterm_received) {
			log_debugx("exiting on signal; "
			    "reloading empty configuration");

			log_debugx("removing CTL iSCSI ports "
			    "and terminating all connections");

			oldconf = newconf;
			newconf = conf_new();
			if (debug > 0)
				newconf->conf_debug = debug;
			error = conf_apply(oldconf, newconf);
			if (error != 0)
				log_warnx("failed to apply configuration");
			if (oldconf->conf_pidfh) {
				pidfile_remove(oldconf->conf_pidfh);
				oldconf->conf_pidfh = NULL;
			}
			conf_delete(newconf);
			conf_delete(oldconf);
			oldconf = NULL;

			log_warnx("exiting on signal");
			exit(0);
		} else {
			nchildren -= wait_for_children(false);
			assert(nchildren >= 0);
			if (timed_out()) {
				set_timeout(0, false);
				for (auto &kv : newconf->conf_isns)
					isns_check(newconf, &kv.second);

				/* Schedule iSNS update */
				if (!newconf->conf_isns.empty()) {
					set_timeout((newconf->conf_isns_period
					    + 2) / 3,
					    false);
				}
			}
		}
	}
	/* NOTREACHED */
}
