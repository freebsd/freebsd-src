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

	conf = calloc(1, sizeof(*conf));
	if (conf == NULL)
		log_err(1, "calloc");
	TAILQ_INIT(&conf->conf_luns);
	TAILQ_INIT(&conf->conf_targets);
	TAILQ_INIT(&conf->conf_auth_groups);
	TAILQ_INIT(&conf->conf_ports);
	TAILQ_INIT(&conf->conf_portal_groups);
	TAILQ_INIT(&conf->conf_isns);

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
	struct auth_group *ag, *cagtmp;
	struct portal_group *pg, *cpgtmp;
	struct isns *is, *istmp;

	assert(conf->conf_pidfh == NULL);

	TAILQ_FOREACH_SAFE(lun, &conf->conf_luns, l_next, ltmp)
		lun_delete(lun);
	TAILQ_FOREACH_SAFE(targ, &conf->conf_targets, t_next, tmp)
		target_delete(targ);
	TAILQ_FOREACH_SAFE(ag, &conf->conf_auth_groups, ag_next, cagtmp)
		auth_group_delete(ag);
	TAILQ_FOREACH_SAFE(pg, &conf->conf_portal_groups, pg_next, cpgtmp)
		portal_group_delete(pg);
	TAILQ_FOREACH_SAFE(is, &conf->conf_isns, i_next, istmp)
		isns_delete(is);
	assert(TAILQ_EMPTY(&conf->conf_ports));
	free(conf->conf_pidfile_path);
	free(conf);
}

static struct auth *
auth_new(struct auth_group *ag)
{
	struct auth *auth;

	auth = calloc(1, sizeof(*auth));
	if (auth == NULL)
		log_err(1, "calloc");
	auth->a_auth_group = ag;
	TAILQ_INSERT_TAIL(&ag->ag_auths, auth, a_next);
	return (auth);
}

static void
auth_delete(struct auth *auth)
{
	TAILQ_REMOVE(&auth->a_auth_group->ag_auths, auth, a_next);

	free(auth->a_user);
	free(auth->a_secret);
	free(auth->a_mutual_user);
	free(auth->a_mutual_secret);
	free(auth);
}

const struct auth *
auth_find(const struct auth_group *ag, const char *user)
{
	const struct auth *auth;

	TAILQ_FOREACH(auth, &ag->ag_auths, a_next) {
		if (strcmp(auth->a_user, user) == 0)
			return (auth);
	}

	return (NULL);
}

static void
auth_check_secret_length(struct auth *auth)
{
	size_t len;

	len = strlen(auth->a_secret);
	if (len > 16) {
		if (auth->a_auth_group->ag_name != NULL)
			log_warnx("secret for user \"%s\", auth-group \"%s\", "
			    "is too long; it should be at most 16 characters "
			    "long", auth->a_user, auth->a_auth_group->ag_name);
		else
			log_warnx("secret for user \"%s\", target \"%s\", "
			    "is too long; it should be at most 16 characters "
			    "long", auth->a_user,
			    auth->a_auth_group->ag_target->t_name);
	}
	if (len < 12) {
		if (auth->a_auth_group->ag_name != NULL)
			log_warnx("secret for user \"%s\", auth-group \"%s\", "
			    "is too short; it should be at least 12 characters "
			    "long", auth->a_user,
			    auth->a_auth_group->ag_name);
		else
			log_warnx("secret for user \"%s\", target \"%s\", "
			    "is too short; it should be at least 12 characters "
			    "long", auth->a_user,
			    auth->a_auth_group->ag_target->t_name);
	}

	if (auth->a_mutual_secret != NULL) {
		len = strlen(auth->a_mutual_secret);
		if (len > 16) {
			if (auth->a_auth_group->ag_name != NULL)
				log_warnx("mutual secret for user \"%s\", "
				    "auth-group \"%s\", is too long; it should "
				    "be at most 16 characters long",
				    auth->a_user, auth->a_auth_group->ag_name);
			else
				log_warnx("mutual secret for user \"%s\", "
				    "target \"%s\", is too long; it should "
				    "be at most 16 characters long",
				    auth->a_user,
				    auth->a_auth_group->ag_target->t_name);
		}
		if (len < 12) {
			if (auth->a_auth_group->ag_name != NULL)
				log_warnx("mutual secret for user \"%s\", "
				    "auth-group \"%s\", is too short; it "
				    "should be at least 12 characters long",
				    auth->a_user, auth->a_auth_group->ag_name);
			else
				log_warnx("mutual secret for user \"%s\", "
				    "target \"%s\", is too short; it should be "
				    "at least 12 characters long",
				    auth->a_user,
				    auth->a_auth_group->ag_target->t_name);
		}
	}
}

const struct auth *
auth_new_chap(struct auth_group *ag, const char *user,
    const char *secret)
{
	struct auth *auth;

	if (ag->ag_type == AG_TYPE_UNKNOWN)
		ag->ag_type = AG_TYPE_CHAP;
	if (ag->ag_type != AG_TYPE_CHAP) {
		if (ag->ag_name != NULL)
			log_warnx("cannot mix \"chap\" authentication with "
			    "other types for auth-group \"%s\"", ag->ag_name);
		else
			log_warnx("cannot mix \"chap\" authentication with "
			    "other types for target \"%s\"",
			    ag->ag_target->t_name);
		return (NULL);
	}

	auth = auth_new(ag);
	auth->a_user = checked_strdup(user);
	auth->a_secret = checked_strdup(secret);

	auth_check_secret_length(auth);

	return (auth);
}

const struct auth *
auth_new_chap_mutual(struct auth_group *ag, const char *user,
    const char *secret, const char *user2, const char *secret2)
{
	struct auth *auth;

	if (ag->ag_type == AG_TYPE_UNKNOWN)
		ag->ag_type = AG_TYPE_CHAP_MUTUAL;
	if (ag->ag_type != AG_TYPE_CHAP_MUTUAL) {
		if (ag->ag_name != NULL)
			log_warnx("cannot mix \"chap-mutual\" authentication "
			    "with other types for auth-group \"%s\"",
			    ag->ag_name);
		else
			log_warnx("cannot mix \"chap-mutual\" authentication "
			    "with other types for target \"%s\"",
			    ag->ag_target->t_name);
		return (NULL);
	}

	auth = auth_new(ag);
	auth->a_user = checked_strdup(user);
	auth->a_secret = checked_strdup(secret);
	auth->a_mutual_user = checked_strdup(user2);
	auth->a_mutual_secret = checked_strdup(secret2);

	auth_check_secret_length(auth);

	return (auth);
}

const struct auth_name *
auth_name_new(struct auth_group *ag, const char *name)
{
	struct auth_name *an;

	an = calloc(1, sizeof(*an));
	if (an == NULL)
		log_err(1, "calloc");
	an->an_auth_group = ag;
	an->an_initiator_name = checked_strdup(name);
	TAILQ_INSERT_TAIL(&ag->ag_names, an, an_next);
	return (an);
}

static void
auth_name_delete(struct auth_name *an)
{
	TAILQ_REMOVE(&an->an_auth_group->ag_names, an, an_next);

	free(an->an_initiator_name);
	free(an);
}

bool
auth_name_defined(const struct auth_group *ag)
{
	if (TAILQ_EMPTY(&ag->ag_names))
		return (false);
	return (true);
}

const struct auth_name *
auth_name_find(const struct auth_group *ag, const char *name)
{
	const struct auth_name *auth_name;

	TAILQ_FOREACH(auth_name, &ag->ag_names, an_next) {
		if (strcmp(auth_name->an_initiator_name, name) == 0)
			return (auth_name);
	}

	return (NULL);
}

bool
auth_name_check(const struct auth_group *ag, const char *initiator_name)
{
	if (!auth_name_defined(ag))
		return (true);

	if (auth_name_find(ag, initiator_name) == NULL)
		return (false);

	return (true);
}

const struct auth_portal *
auth_portal_new(struct auth_group *ag, const char *portal)
{
	struct auth_portal *ap;
	char *net, *mask, *str, *tmp;
	int len, dm, m;

	ap = calloc(1, sizeof(*ap));
	if (ap == NULL)
		log_err(1, "calloc");
	ap->ap_auth_group = ag;
	ap->ap_initiator_portal = checked_strdup(portal);
	mask = str = checked_strdup(portal);
	net = strsep(&mask, "/");
	if (net[0] == '[')
		net++;
	len = strlen(net);
	if (len == 0)
		goto error;
	if (net[len - 1] == ']')
		net[len - 1] = 0;
	if (strchr(net, ':') != NULL) {
		struct sockaddr_in6 *sin6 =
		    (struct sockaddr_in6 *)&ap->ap_sa;

		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, net, &sin6->sin6_addr) <= 0)
			goto error;
		dm = 128;
	} else {
		struct sockaddr_in *sin =
		    (struct sockaddr_in *)&ap->ap_sa;

		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		if (inet_pton(AF_INET, net, &sin->sin_addr) <= 0)
			goto error;
		dm = 32;
	}
	if (mask != NULL) {
		m = strtol(mask, &tmp, 0);
		if (m < 0 || m > dm || tmp[0] != 0)
			goto error;
	} else
		m = dm;
	ap->ap_mask = m;
	free(str);
	TAILQ_INSERT_TAIL(&ag->ag_portals, ap, ap_next);
	return (ap);

error:
	free(str);
	free(ap);
	log_warnx("incorrect initiator portal \"%s\"", portal);
	return (NULL);
}

static void
auth_portal_delete(struct auth_portal *ap)
{
	TAILQ_REMOVE(&ap->ap_auth_group->ag_portals, ap, ap_next);

	free(ap->ap_initiator_portal);
	free(ap);
}

bool
auth_portal_defined(const struct auth_group *ag)
{
	if (TAILQ_EMPTY(&ag->ag_portals))
		return (false);
	return (true);
}

const struct auth_portal *
auth_portal_find(const struct auth_group *ag, const struct sockaddr_storage *ss)
{
	const struct auth_portal *ap;
	const uint8_t *a, *b;
	int i;
	uint8_t bmask;

	TAILQ_FOREACH(ap, &ag->ag_portals, ap_next) {
		if (ap->ap_sa.ss_family != ss->ss_family)
			continue;
		if (ss->ss_family == AF_INET) {
			a = (const uint8_t *)
			    &((const struct sockaddr_in *)ss)->sin_addr;
			b = (const uint8_t *)
			    &((const struct sockaddr_in *)&ap->ap_sa)->sin_addr;
		} else {
			a = (const uint8_t *)
			    &((const struct sockaddr_in6 *)ss)->sin6_addr;
			b = (const uint8_t *)
			    &((const struct sockaddr_in6 *)&ap->ap_sa)->sin6_addr;
		}
		for (i = 0; i < ap->ap_mask / 8; i++) {
			if (a[i] != b[i])
				goto next;
		}
		if (ap->ap_mask % 8) {
			bmask = 0xff << (8 - (ap->ap_mask % 8));
			if ((a[i] & bmask) != (b[i] & bmask))
				goto next;
		}
		return (ap);
next:
		;
	}

	return (NULL);
}

bool
auth_portal_check(const struct auth_group *ag, const struct sockaddr_storage *sa)
{

	if (!auth_portal_defined(ag))
		return (true);

	if (auth_portal_find(ag, sa) == NULL)
		return (false);

	return (true);
}

struct auth_group *
auth_group_new(struct conf *conf, const char *name)
{
	struct auth_group *ag;

	if (name != NULL) {
		ag = auth_group_find(conf, name);
		if (ag != NULL) {
			log_warnx("duplicated auth-group \"%s\"", name);
			return (NULL);
		}
	}

	ag = calloc(1, sizeof(*ag));
	if (ag == NULL)
		log_err(1, "calloc");
	if (name != NULL)
		ag->ag_name = checked_strdup(name);
	TAILQ_INIT(&ag->ag_auths);
	TAILQ_INIT(&ag->ag_names);
	TAILQ_INIT(&ag->ag_portals);
	ag->ag_conf = conf;
	TAILQ_INSERT_TAIL(&conf->conf_auth_groups, ag, ag_next);

	return (ag);
}

void
auth_group_delete(struct auth_group *ag)
{
	struct auth *auth, *auth_tmp;
	struct auth_name *auth_name, *auth_name_tmp;
	struct auth_portal *auth_portal, *auth_portal_tmp;

	TAILQ_REMOVE(&ag->ag_conf->conf_auth_groups, ag, ag_next);

	TAILQ_FOREACH_SAFE(auth, &ag->ag_auths, a_next, auth_tmp)
		auth_delete(auth);
	TAILQ_FOREACH_SAFE(auth_name, &ag->ag_names, an_next, auth_name_tmp)
		auth_name_delete(auth_name);
	TAILQ_FOREACH_SAFE(auth_portal, &ag->ag_portals, ap_next,
	    auth_portal_tmp)
		auth_portal_delete(auth_portal);
	free(ag->ag_name);
	free(ag);
}

struct auth_group *
auth_group_find(const struct conf *conf, const char *name)
{
	struct auth_group *ag;

	assert(name != NULL);
	TAILQ_FOREACH(ag, &conf->conf_auth_groups, ag_next) {
		if (ag->ag_name != NULL && strcmp(ag->ag_name, name) == 0)
			return (ag);
	}

	return (NULL);
}

bool
auth_group_set_type(struct auth_group *ag, const char *str)
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
		if (ag->ag_name != NULL)
			log_warnx("invalid auth-type \"%s\" for auth-group "
			    "\"%s\"", str, ag->ag_name);
		else
			log_warnx("invalid auth-type \"%s\" for target "
			    "\"%s\"", str, ag->ag_target->t_name);
		return (false);
	}

	if (ag->ag_type != AG_TYPE_UNKNOWN && ag->ag_type != type) {
		if (ag->ag_name != NULL) {
			log_warnx("cannot set auth-type to \"%s\" for "
			    "auth-group \"%s\"; already has a different "
			    "type", str, ag->ag_name);
		} else {
			log_warnx("cannot set auth-type to \"%s\" for target "
			    "\"%s\"; already has a different type",
			    str, ag->ag_target->t_name);
		}
		return (false);
	}

	ag->ag_type = type;

	return (true);
}

static struct portal *
portal_new(struct portal_group *pg)
{
	struct portal *portal;

	portal = calloc(1, sizeof(*portal));
	if (portal == NULL)
		log_err(1, "calloc");
	TAILQ_INIT(&portal->p_targets);
	portal->p_portal_group = pg;
	TAILQ_INSERT_TAIL(&pg->pg_portals, portal, p_next);
	return (portal);
}

static void
portal_delete(struct portal *portal)
{

	TAILQ_REMOVE(&portal->p_portal_group->pg_portals, portal, p_next);
	if (portal->p_ai != NULL)
		freeaddrinfo(portal->p_ai);
	free(portal->p_listen);
	free(portal);
}

struct portal_group *
portal_group_new(struct conf *conf, const char *name)
{
	struct portal_group *pg;

	pg = portal_group_find(conf, name);
	if (pg != NULL) {
		log_warnx("duplicated portal-group \"%s\"", name);
		return (NULL);
	}

	pg = calloc(1, sizeof(*pg));
	if (pg == NULL)
		log_err(1, "calloc");
	pg->pg_name = checked_strdup(name);
	pg->pg_options = nvlist_create(0);
	TAILQ_INIT(&pg->pg_portals);
	TAILQ_INIT(&pg->pg_ports);
	pg->pg_conf = conf;
	pg->pg_tag = 0;		/* Assigned later in conf_apply(). */
	pg->pg_dscp = -1;
	pg->pg_pcp = -1;
	TAILQ_INSERT_TAIL(&conf->conf_portal_groups, pg, pg_next);

	return (pg);
}

void
portal_group_delete(struct portal_group *pg)
{
	struct portal *portal, *tmp;
	struct port *port, *tport;

	TAILQ_FOREACH_SAFE(port, &pg->pg_ports, p_pgs, tport)
		port_delete(port);
	TAILQ_REMOVE(&pg->pg_conf->conf_portal_groups, pg, pg_next);

	TAILQ_FOREACH_SAFE(portal, &pg->pg_portals, p_next, tmp)
		portal_delete(portal);
	nvlist_destroy(pg->pg_options);
	free(pg->pg_name);
	free(pg->pg_offload);
	free(pg->pg_redirection);
	free(pg);
}

struct portal_group *
portal_group_find(const struct conf *conf, const char *name)
{
	struct portal_group *pg;

	TAILQ_FOREACH(pg, &conf->conf_portal_groups, pg_next) {
		if (strcmp(pg->pg_name, name) == 0)
			return (pg);
	}

	return (NULL);
}

static int
parse_addr_port(char *arg, const char *def_port, struct addrinfo **ai)
{
	struct addrinfo hints;
	char *str, *addr, *ch;
	const char *port;
	int error, colons = 0;

	str = arg = strdup(arg);
	if (arg[0] == '[') {
		/*
		 * IPv6 address in square brackets, perhaps with port.
		 */
		arg++;
		addr = strsep(&arg, "]");
		if (arg == NULL) {
			free(str);
			return (1);
		}
		if (arg[0] == '\0') {
			port = def_port;
		} else if (arg[0] == ':') {
			port = arg + 1;
		} else {
			free(str);
			return (1);
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
	free(str);
	return ((error != 0) ? 1 : 0);
}

bool
portal_group_add_listen(struct portal_group *pg, const char *value, bool iser)
{
	struct portal *portal;

	portal = portal_new(pg);
	portal->p_listen = checked_strdup(value);
	portal->p_iser = iser;

	if (parse_addr_port(portal->p_listen, "3260", &portal->p_ai)) {
		log_warnx("invalid listen address %s", portal->p_listen);
		portal_delete(portal);
		return (false);
	}

	/*
	 * XXX: getaddrinfo(3) may return multiple addresses; we should turn
	 *	those into multiple portals.
	 */

	return (true);
}

bool
isns_new(struct conf *conf, const char *addr)
{
	struct isns *isns;

	isns = calloc(1, sizeof(*isns));
	if (isns == NULL)
		log_err(1, "calloc");
	isns->i_conf = conf;
	TAILQ_INSERT_TAIL(&conf->conf_isns, isns, i_next);
	isns->i_addr = checked_strdup(addr);

	if (parse_addr_port(isns->i_addr, "3205", &isns->i_ai)) {
		log_warnx("invalid iSNS address %s", isns->i_addr);
		isns_delete(isns);
		return (false);
	}

	/*
	 * XXX: getaddrinfo(3) may return multiple addresses; we should turn
	 *	those into multiple servers.
	 */

	return (true);
}

void
isns_delete(struct isns *isns)
{

	TAILQ_REMOVE(&isns->i_conf->conf_isns, isns, i_next);
	free(isns->i_addr);
	if (isns->i_ai != NULL)
		freeaddrinfo(isns->i_ai);
	free(isns);
}

static int
isns_do_connect(struct isns *isns)
{
	int s;

	s = socket(isns->i_ai->ai_family, isns->i_ai->ai_socktype,
	    isns->i_ai->ai_protocol);
	if (s < 0) {
		log_warn("socket(2) failed for %s", isns->i_addr);
		return (-1);
	}
	if (connect(s, isns->i_ai->ai_addr, isns->i_ai->ai_addrlen)) {
		log_warn("connect(2) failed for %s", isns->i_addr);
		close(s);
		return (-1);
	}
	return(s);
}

static int
isns_do_register(struct isns *isns, int s, const char *hostname)
{
	struct conf *conf = isns->i_conf;
	struct target *target;
	struct portal *portal;
	struct portal_group *pg;
	struct port *port;
	struct isns_req *req;
	int res = 0;
	uint32_t error;

	req = isns_req_create(ISNS_FUNC_DEVATTRREG, ISNS_FLAG_CLIENT);
	isns_req_add_str(req, 32, TAILQ_FIRST(&conf->conf_targets)->t_name);
	isns_req_add_delim(req);
	isns_req_add_str(req, 1, hostname);
	isns_req_add_32(req, 2, 2); /* 2 -- iSCSI */
	isns_req_add_32(req, 6, conf->conf_isns_period);
	TAILQ_FOREACH(pg, &conf->conf_portal_groups, pg_next) {
		if (pg->pg_unassigned)
			continue;
		TAILQ_FOREACH(portal, &pg->pg_portals, p_next) {
			isns_req_add_addr(req, 16, portal->p_ai);
			isns_req_add_port(req, 17, portal->p_ai);
		}
	}
	TAILQ_FOREACH(target, &conf->conf_targets, t_next) {
		isns_req_add_str(req, 32, target->t_name);
		isns_req_add_32(req, 33, 1); /* 1 -- Target*/
		if (target->t_alias != NULL)
			isns_req_add_str(req, 34, target->t_alias);
		TAILQ_FOREACH(port, &target->t_ports, p_ts) {
			if ((pg = port->p_portal_group) == NULL)
				continue;
			isns_req_add_32(req, 51, pg->pg_tag);
			TAILQ_FOREACH(portal, &pg->pg_portals, p_next) {
				isns_req_add_addr(req, 49, portal->p_ai);
				isns_req_add_port(req, 50, portal->p_ai);
			}
		}
	}
	res = isns_req_send(s, req);
	if (res < 0) {
		log_warn("send(2) failed for %s", isns->i_addr);
		goto quit;
	}
	res = isns_req_receive(s, req);
	if (res < 0) {
		log_warn("receive(2) failed for %s", isns->i_addr);
		goto quit;
	}
	error = isns_req_get_status(req);
	if (error != 0) {
		log_warnx("iSNS register error %d for %s", error, isns->i_addr);
		res = -1;
	}
quit:
	isns_req_free(req);
	return (res);
}

static int
isns_do_check(struct isns *isns, int s, const char *hostname)
{
	struct conf *conf = isns->i_conf;
	struct isns_req *req;
	int res = 0;
	uint32_t error;

	req = isns_req_create(ISNS_FUNC_DEVATTRQRY, ISNS_FLAG_CLIENT);
	isns_req_add_str(req, 32, TAILQ_FIRST(&conf->conf_targets)->t_name);
	isns_req_add_str(req, 1, hostname);
	isns_req_add_delim(req);
	isns_req_add(req, 2, 0, NULL);
	res = isns_req_send(s, req);
	if (res < 0) {
		log_warn("send(2) failed for %s", isns->i_addr);
		goto quit;
	}
	res = isns_req_receive(s, req);
	if (res < 0) {
		log_warn("receive(2) failed for %s", isns->i_addr);
		goto quit;
	}
	error = isns_req_get_status(req);
	if (error != 0) {
		log_warnx("iSNS check error %d for %s", error, isns->i_addr);
		res = -1;
	}
quit:
	isns_req_free(req);
	return (res);
}

static int
isns_do_deregister(struct isns *isns, int s, const char *hostname)
{
	struct conf *conf = isns->i_conf;
	struct isns_req *req;
	int res = 0;
	uint32_t error;

	req = isns_req_create(ISNS_FUNC_DEVDEREG, ISNS_FLAG_CLIENT);
	isns_req_add_str(req, 32, TAILQ_FIRST(&conf->conf_targets)->t_name);
	isns_req_add_delim(req);
	isns_req_add_str(req, 1, hostname);
	res = isns_req_send(s, req);
	if (res < 0) {
		log_warn("send(2) failed for %s", isns->i_addr);
		goto quit;
	}
	res = isns_req_receive(s, req);
	if (res < 0) {
		log_warn("receive(2) failed for %s", isns->i_addr);
		goto quit;
	}
	error = isns_req_get_status(req);
	if (error != 0) {
		log_warnx("iSNS deregister error %d for %s", error, isns->i_addr);
		res = -1;
	}
quit:
	isns_req_free(req);
	return (res);
}

void
isns_register(struct isns *isns, struct isns *oldisns)
{
	struct conf *conf = isns->i_conf;
	int error, s;
	char hostname[256];

	if (TAILQ_EMPTY(&conf->conf_targets) ||
	    TAILQ_EMPTY(&conf->conf_portal_groups))
		return;
	set_timeout(conf->conf_isns_timeout, false);
	s = isns_do_connect(isns);
	if (s < 0) {
		set_timeout(0, false);
		return;
	}
	error = gethostname(hostname, sizeof(hostname));
	if (error != 0)
		log_err(1, "gethostname");

	if (oldisns == NULL || TAILQ_EMPTY(&oldisns->i_conf->conf_targets))
		oldisns = isns;
	isns_do_deregister(oldisns, s, hostname);
	isns_do_register(isns, s, hostname);
	close(s);
	set_timeout(0, false);
}

void
isns_check(struct isns *isns)
{
	struct conf *conf = isns->i_conf;
	int error, s, res;
	char hostname[256];

	if (TAILQ_EMPTY(&conf->conf_targets) ||
	    TAILQ_EMPTY(&conf->conf_portal_groups))
		return;
	set_timeout(conf->conf_isns_timeout, false);
	s = isns_do_connect(isns);
	if (s < 0) {
		set_timeout(0, false);
		return;
	}
	error = gethostname(hostname, sizeof(hostname));
	if (error != 0)
		log_err(1, "gethostname");

	res = isns_do_check(isns, s, hostname);
	if (res < 0) {
		isns_do_deregister(isns, s, hostname);
		isns_do_register(isns, s, hostname);
	}
	close(s);
	set_timeout(0, false);
}

void
isns_deregister(struct isns *isns)
{
	struct conf *conf = isns->i_conf;
	int error, s;
	char hostname[256];

	if (TAILQ_EMPTY(&conf->conf_targets) ||
	    TAILQ_EMPTY(&conf->conf_portal_groups))
		return;
	set_timeout(conf->conf_isns_timeout, false);
	s = isns_do_connect(isns);
	if (s < 0)
		return;
	error = gethostname(hostname, sizeof(hostname));
	if (error != 0)
		log_err(1, "gethostname");

	isns_do_deregister(isns, s, hostname);
	close(s);
	set_timeout(0, false);
}

bool
portal_group_set_filter(struct portal_group *pg, const char *str)
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
		    str, pg->pg_name);
		return (false);
	}

	if (pg->pg_discovery_filter != PG_FILTER_UNKNOWN &&
	    pg->pg_discovery_filter != filter) {
		log_warnx("cannot set discovery-filter to \"%s\" for "
		    "portal-group \"%s\"; already has a different "
		    "value", str, pg->pg_name);
		return (false);
	}

	pg->pg_discovery_filter = filter;

	return (true);
}

bool
portal_group_set_offload(struct portal_group *pg, const char *offload)
{

	if (pg->pg_offload != NULL) {
		log_warnx("cannot set offload to \"%s\" for "
		    "portal-group \"%s\"; already defined",
		    offload, pg->pg_name);
		return (false);
	}

	pg->pg_offload = checked_strdup(offload);

	return (true);
}

bool
portal_group_set_redirection(struct portal_group *pg, const char *addr)
{

	if (pg->pg_redirection != NULL) {
		log_warnx("cannot set redirection to \"%s\" for "
		    "portal-group \"%s\"; already defined",
		    addr, pg->pg_name);
		return (false);
	}

	pg->pg_redirection = checked_strdup(addr);

	return (true);
}

struct pport *
pport_new(struct kports *kports, const char *name, uint32_t ctl_port)
{
	struct pport *pp;

	pp = calloc(1, sizeof(*pp));
	if (pp == NULL)
		log_err(1, "calloc");
	pp->pp_kports = kports;
	pp->pp_name = checked_strdup(name);
	pp->pp_ctl_port = ctl_port;
	TAILQ_INIT(&pp->pp_ports);
	TAILQ_INSERT_TAIL(&kports->pports, pp, pp_next);
	return (pp);
}

struct pport *
pport_find(const struct kports *kports, const char *name)
{
	struct pport *pp;

	TAILQ_FOREACH(pp, &kports->pports, pp_next) {
		if (strcasecmp(pp->pp_name, name) == 0)
			return (pp);
	}
	return (NULL);
}

struct pport *
pport_copy(struct pport *pp, struct kports *kports)
{
	struct pport *ppnew;

	ppnew = pport_new(kports, pp->pp_name, pp->pp_ctl_port);
	return (ppnew);
}

void
pport_delete(struct pport *pp)
{
	struct port *port, *tport;

	TAILQ_FOREACH_SAFE(port, &pp->pp_ports, p_ts, tport)
		port_delete(port);
	TAILQ_REMOVE(&pp->pp_kports->pports, pp, pp_next);
	free(pp->pp_name);
	free(pp);
}

struct port *
port_new(struct conf *conf, struct target *target, struct portal_group *pg)
{
	struct port *port;
	char *name;
	int ret;

	ret = asprintf(&name, "%s-%s", pg->pg_name, target->t_name);
	if (ret <= 0)
		log_err(1, "asprintf");
	if (port_find(conf, name) != NULL) {
		log_warnx("duplicate port \"%s\"", name);
		free(name);
		return (NULL);
	}
	port = calloc(1, sizeof(*port));
	if (port == NULL)
		log_err(1, "calloc");
	port->p_conf = conf;
	port->p_name = name;
	TAILQ_INSERT_TAIL(&conf->conf_ports, port, p_next);
	TAILQ_INSERT_TAIL(&target->t_ports, port, p_ts);
	port->p_target = target;
	TAILQ_INSERT_TAIL(&pg->pg_ports, port, p_pgs);
	port->p_portal_group = pg;
	return (port);
}

struct port *
port_new_ioctl(struct conf *conf, struct kports *kports, struct target *target,
    int pp, int vp)
{
	struct pport *pport;
	struct port *port;
	char *pname;
	char *name;
	int ret;

	ret = asprintf(&pname, "ioctl/%d/%d", pp, vp);
	if (ret <= 0) {
		log_err(1, "asprintf");
		return (NULL);
	}

	pport = pport_find(kports, pname);
	if (pport != NULL) {
		free(pname);
		return (port_new_pp(conf, target, pport));
	}

	ret = asprintf(&name, "%s-%s", pname, target->t_name);
	free(pname);

	if (ret <= 0)
		log_err(1, "asprintf");
	if (port_find(conf, name) != NULL) {
		log_warnx("duplicate port \"%s\"", name);
		free(name);
		return (NULL);
	}
	port = calloc(1, sizeof(*port));
	if (port == NULL)
		log_err(1, "calloc");
	port->p_conf = conf;
	port->p_name = name;
	port->p_ioctl_port = true;
	port->p_ioctl_pp = pp;
	port->p_ioctl_vp = vp;
	TAILQ_INSERT_TAIL(&conf->conf_ports, port, p_next);
	TAILQ_INSERT_TAIL(&target->t_ports, port, p_ts);
	port->p_target = target;
	return (port);
}

struct port *
port_new_pp(struct conf *conf, struct target *target, struct pport *pp)
{
	struct port *port;
	char *name;
	int ret;

	ret = asprintf(&name, "%s-%s", pp->pp_name, target->t_name);
	if (ret <= 0)
		log_err(1, "asprintf");
	if (port_find(conf, name) != NULL) {
		log_warnx("duplicate port \"%s\"", name);
		free(name);
		return (NULL);
	}
	port = calloc(1, sizeof(*port));
	if (port == NULL)
		log_err(1, "calloc");
	port->p_conf = conf;
	port->p_name = name;
	TAILQ_INSERT_TAIL(&conf->conf_ports, port, p_next);
	TAILQ_INSERT_TAIL(&target->t_ports, port, p_ts);
	port->p_target = target;
	TAILQ_INSERT_TAIL(&pp->pp_ports, port, p_pps);
	port->p_pport = pp;
	return (port);
}

struct port *
port_find(const struct conf *conf, const char *name)
{
	struct port *port;

	TAILQ_FOREACH(port, &conf->conf_ports, p_next) {
		if (strcasecmp(port->p_name, name) == 0)
			return (port);
	}

	return (NULL);
}

struct port *
port_find_in_pg(const struct portal_group *pg, const char *target)
{
	struct port *port;

	TAILQ_FOREACH(port, &pg->pg_ports, p_pgs) {
		if (strcasecmp(port->p_target->t_name, target) == 0)
			return (port);
	}

	return (NULL);
}

void
port_delete(struct port *port)
{

	if (port->p_portal_group)
		TAILQ_REMOVE(&port->p_portal_group->pg_ports, port, p_pgs);
	if (port->p_pport)
		TAILQ_REMOVE(&port->p_pport->pp_ports, port, p_pps);
	if (port->p_target)
		TAILQ_REMOVE(&port->p_target->t_ports, port, p_ts);
	TAILQ_REMOVE(&port->p_conf->conf_ports, port, p_next);
	free(port->p_name);
	free(port);
}

bool
port_is_dummy(struct port *port)
{

	if (port->p_portal_group) {
		if (port->p_portal_group->pg_foreign)
			return (true);
		if (TAILQ_EMPTY(&port->p_portal_group->pg_portals))
			return (true);
	}
	return (false);
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
	targ = calloc(1, sizeof(*targ));
	if (targ == NULL)
		log_err(1, "calloc");
	targ->t_name = checked_strdup(name);

	/*
	 * RFC 3722 requires us to normalize the name to lowercase.
	 */
	len = strlen(name);
	for (i = 0; i < len; i++)
		targ->t_name[i] = tolower(targ->t_name[i]);

	targ->t_conf = conf;
	TAILQ_INIT(&targ->t_ports);
	TAILQ_INSERT_TAIL(&conf->conf_targets, targ, t_next);

	return (targ);
}

void
target_delete(struct target *targ)
{
	struct port *port, *tport;

	TAILQ_FOREACH_SAFE(port, &targ->t_ports, p_ts, tport)
		port_delete(port);
	TAILQ_REMOVE(&targ->t_conf->conf_targets, targ, t_next);

	free(targ->t_pport);
	free(targ->t_name);
	free(targ->t_redirection);
	free(targ);
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

bool
target_set_redirection(struct target *target, const char *addr)
{

	if (target->t_redirection != NULL) {
		log_warnx("cannot set redirection to \"%s\" for "
		    "target \"%s\"; already defined",
		    addr, target->t_name);
		return (false);
	}

	target->t_redirection = checked_strdup(addr);

	return (true);
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

	lun = calloc(1, sizeof(*lun));
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
lun_set_backend(struct lun *lun, const char *value)
{
	free(lun->l_backend);
	lun->l_backend = checked_strdup(value);
}

void
lun_set_blocksize(struct lun *lun, size_t value)
{

	lun->l_blocksize = value;
}

void
lun_set_device_type(struct lun *lun, uint8_t value)
{

	lun->l_device_type = value;
}

void
lun_set_device_id(struct lun *lun, const char *value)
{
	free(lun->l_device_id);
	lun->l_device_id = checked_strdup(value);
}

void
lun_set_path(struct lun *lun, const char *value)
{
	free(lun->l_path);
	lun->l_path = checked_strdup(value);
}

void
lun_set_scsiname(struct lun *lun, const char *value)
{
	free(lun->l_scsiname);
	lun->l_scsiname = checked_strdup(value);
}

void
lun_set_serial(struct lun *lun, const char *value)
{
	free(lun->l_serial);
	lun->l_serial = checked_strdup(value);
}

void
lun_set_size(struct lun *lun, int64_t value)
{

	lun->l_size = value;
}

void
lun_set_ctl_lun(struct lun *lun, uint32_t value)
{

	lun->l_ctl_lun = value;
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

	conn = calloc(1, sizeof(*conn));
	if (conn == NULL)
		log_err(1, "calloc");
	connection_init(&conn->conn, &conn_ops, proxy_mode);
	conn->conn.conn_socket = fd;
	conn->conn_portal = portal;
	conn->conn_initiator_addr = checked_strdup(host);
	memcpy(&conn->conn_initiator_sa, client_sa, client_sa->sa_len);

	return (conn);
}

#if 0
static void
options_print(const char *prefix, nvlist_t *nvl)
{
	const char *name;
	void *cookie;

	cookie = NULL;
	while ((name = nvlist_next(nvl, NULL, &cookie)) != NULL) {
		fprintf(stderr, "%soption %s %s\n", prefix, name,
		    nvlist_get_string(nvl, name));
	}
}

static void
conf_print(struct conf *conf)
{
	struct auth_group *ag;
	struct auth *auth;
	struct auth_name *auth_name;
	struct auth_portal *auth_portal;
	struct portal_group *pg;
	struct portal *portal;
	struct target *targ;
	struct lun *lun;

	TAILQ_FOREACH(ag, &conf->conf_auth_groups, ag_next) {
		fprintf(stderr, "auth-group %s {\n", ag->ag_name);
		TAILQ_FOREACH(auth, &ag->ag_auths, a_next)
			fprintf(stderr, "\t chap-mutual %s %s %s %s\n",
			    auth->a_user, auth->a_secret,
			    auth->a_mutual_user, auth->a_mutual_secret);
		TAILQ_FOREACH(auth_name, &ag->ag_names, an_next)
			fprintf(stderr, "\t initiator-name %s\n",
			    auth_name->an_initiator_name);
		TAILQ_FOREACH(auth_portal, &ag->ag_portals, ap_next)
			fprintf(stderr, "\t initiator-portal %s\n",
			    auth_portal->ap_initiator_portal);
		fprintf(stderr, "}\n");
	}
	TAILQ_FOREACH(pg, &conf->conf_portal_groups, pg_next) {
		fprintf(stderr, "portal-group %s {\n", pg->pg_name);
		TAILQ_FOREACH(portal, &pg->pg_portals, p_next)
			fprintf(stderr, "\t listen %s\n", portal->p_listen);
		options_print("\t", pg->pg_options);
		fprintf(stderr, "}\n");
	}
	TAILQ_FOREACH(lun, &conf->conf_luns, l_next) {
		fprintf(stderr, "\tlun %s {\n", lun->l_name);
		fprintf(stderr, "\t\tpath %s\n", lun->l_path);
		options_print("\t\t", lun->l_options);
		fprintf(stderr, "\t}\n");
	}
	TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
		fprintf(stderr, "target %s {\n", targ->t_name);
		if (targ->t_alias != NULL)
			fprintf(stderr, "\t alias %s\n", targ->t_alias);
		fprintf(stderr, "}\n");
	}
}
#endif

static bool
conf_verify_lun(struct lun *lun)
{
	const struct lun *lun2;

	if (lun->l_backend == NULL)
		lun_set_backend(lun, "block");
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
		if (lun->l_device_type == 5)
			lun_set_blocksize(lun, DEFAULT_CD_BLOCKSIZE);
		else
			lun_set_blocksize(lun, DEFAULT_BLOCKSIZE);
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
	struct auth_group *ag;
	struct portal_group *pg;
	struct port *port;
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
		if (TAILQ_EMPTY(&targ->t_ports)) {
			pg = portal_group_find(conf, "default");
			assert(pg != NULL);
			port_new(conf, targ, pg);
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
	TAILQ_FOREACH(pg, &conf->conf_portal_groups, pg_next) {
		assert(pg->pg_name != NULL);
		if (pg->pg_discovery_auth_group == NULL) {
			pg->pg_discovery_auth_group =
			    auth_group_find(conf, "default");
			assert(pg->pg_discovery_auth_group != NULL);
		}

		if (pg->pg_discovery_filter == PG_FILTER_UNKNOWN)
			pg->pg_discovery_filter = PG_FILTER_NONE;

		if (pg->pg_redirection != NULL) {
			if (!TAILQ_EMPTY(&pg->pg_ports)) {
				log_debugx("portal-group \"%s\" assigned "
				    "to target, but configured "
				    "for redirection",
				    pg->pg_name);
			}
			pg->pg_unassigned = false;
		} else if (!TAILQ_EMPTY(&pg->pg_ports)) {
			pg->pg_unassigned = false;
		} else {
			if (strcmp(pg->pg_name, "default") != 0)
				log_warnx("portal-group \"%s\" not assigned "
				    "to any target", pg->pg_name);
			pg->pg_unassigned = true;
		}
	}
	TAILQ_FOREACH(ag, &conf->conf_auth_groups, ag_next) {
		if (ag->ag_name == NULL)
			assert(ag->ag_target != NULL);
		else
			assert(ag->ag_target == NULL);

		found = false;
		TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
			if (targ->t_auth_group == ag) {
				found = true;
				break;
			}
		}
		TAILQ_FOREACH(port, &conf->conf_ports, p_next) {
			if (port->p_auth_group == ag) {
				found = true;
				break;
			}
		}
		TAILQ_FOREACH(pg, &conf->conf_portal_groups, pg_next) {
			if (pg->pg_discovery_auth_group == ag) {
				found = true;
				break;
			}
		}
		if (!found && ag->ag_name != NULL &&
		    strcmp(ag->ag_name, "default") != 0 &&
		    strcmp(ag->ag_name, "no-authentication") != 0 &&
		    strcmp(ag->ag_name, "no-access") != 0) {
			log_warnx("auth-group \"%s\" not assigned "
			    "to any target", ag->ag_name);
		}
	}

	return (true);
}

static bool
portal_reuse_socket(struct portal *oldp, struct portal *newp)
{
	struct kevent kev;

	if (strcmp(newp->p_listen, oldp->p_listen) != 0)
		return (false);

	if (oldp->p_socket <= 0)
		return (false);

	EV_SET(&kev, oldp->p_socket, EVFILT_READ, EV_ADD, 0, 0, newp);
	if (kevent(kqfd, &kev, 1, NULL, 0, NULL) == -1)
		return (false);

	newp->p_socket = oldp->p_socket;
	oldp->p_socket = 0;
	return (true);
}

static bool
portal_init_socket(struct portal *p)
{
	struct portal_group *pg = p->p_portal_group;
	struct kevent kev;
	int error, sockbuf;
	int one = 1;

	log_debugx("listening on %s, portal-group \"%s\"",
	    p->p_listen, pg->pg_name);
	p->p_socket = socket(p->p_ai->ai_family, p->p_ai->ai_socktype,
	    p->p_ai->ai_protocol);
	if (p->p_socket < 0) {
		log_warn("socket(2) failed for %s",
		    p->p_listen);
		return (false);
	}

	sockbuf = SOCKBUF_SIZE;
	if (setsockopt(p->p_socket, SOL_SOCKET, SO_RCVBUF, &sockbuf,
	    sizeof(sockbuf)) == -1)
		log_warn("setsockopt(SO_RCVBUF) failed for %s",
		    p->p_listen);
	sockbuf = SOCKBUF_SIZE;
	if (setsockopt(p->p_socket, SOL_SOCKET, SO_SNDBUF, &sockbuf,
	    sizeof(sockbuf)) == -1)
		log_warn("setsockopt(SO_SNDBUF) failed for %s", p->p_listen);
	if (setsockopt(p->p_socket, SOL_SOCKET, SO_NO_DDP, &one,
	    sizeof(one)) == -1)
		log_warn("setsockopt(SO_NO_DDP) failed for %s", p->p_listen);
	error = setsockopt(p->p_socket, SOL_SOCKET, SO_REUSEADDR, &one,
	    sizeof(one));
	if (error != 0) {
		log_warn("setsockopt(SO_REUSEADDR) failed for %s", p->p_listen);
		close(p->p_socket);
		p->p_socket = 0;
		return (false);
	}

	if (pg->pg_dscp != -1) {
		/* Only allow the 6-bit DSCP field to be modified */
		int tos = pg->pg_dscp << 2;
		switch (p->p_ai->ai_family) {
		case AF_INET:
			if (setsockopt(p->p_socket, IPPROTO_IP, IP_TOS,
			    &tos, sizeof(tos)) == -1)
				log_warn("setsockopt(IP_TOS) failed for %s",
				    p->p_listen);
			break;
		case AF_INET6:
			if (setsockopt(p->p_socket, IPPROTO_IPV6, IPV6_TCLASS,
			    &tos, sizeof(tos)) == -1)
				log_warn("setsockopt(IPV6_TCLASS) failed for %s",
				    p->p_listen);
			break;
		}
	}
	if (pg->pg_pcp != -1) {
		int pcp = pg->pg_pcp;
		switch (p->p_ai->ai_family) {
		case AF_INET:
			if (setsockopt(p->p_socket, IPPROTO_IP, IP_VLAN_PCP,
			    &pcp, sizeof(pcp)) == -1)
				log_warn("setsockopt(IP_VLAN_PCP) failed for %s",
				    p->p_listen);
			break;
		case AF_INET6:
			if (setsockopt(p->p_socket, IPPROTO_IPV6, IPV6_VLAN_PCP,
			    &pcp, sizeof(pcp)) == -1)
				log_warn("setsockopt(IPV6_VLAN_PCP) failed for %s",
				    p->p_listen);
			break;
		}
	}

	error = bind(p->p_socket, p->p_ai->ai_addr,
	    p->p_ai->ai_addrlen);
	if (error != 0) {
		log_warn("bind(2) failed for %s", p->p_listen);
		close(p->p_socket);
		p->p_socket = 0;
		return (false);
	}
	error = listen(p->p_socket, -1);
	if (error != 0) {
		log_warn("listen(2) failed for %s", p->p_listen);
		close(p->p_socket);
		p->p_socket = 0;
		return (false);
	}
	EV_SET(&kev, p->p_socket, EVFILT_READ, EV_ADD, 0, 0, p);
	error = kevent(kqfd, &kev, 1, NULL, 0, NULL);
	if (error == -1) {
		log_warn("kevent(2) failed to register for %s", p->p_listen);
		close(p->p_socket);
		p->p_socket = 0;
		return (false);
	}
	return (true);
}

static int
conf_apply(struct conf *oldconf, struct conf *newconf)
{
	struct lun *oldlun, *newlun, *tmplun;
	struct portal_group *oldpg, *newpg;
	struct portal *oldp, *newp;
	struct port *oldport, *newport, *tmpport;
	struct isns *oldns, *newns;
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
	TAILQ_FOREACH(newpg, &newconf->conf_portal_groups, pg_next) {
		if (newpg->pg_tag != 0)
			continue;
		oldpg = portal_group_find(oldconf, newpg->pg_name);
		if (oldpg != NULL)
			newpg->pg_tag = oldpg->pg_tag;
		else
			newpg->pg_tag = ++last_portal_group_tag;
	}

	/* Deregister on removed iSNS servers. */
	TAILQ_FOREACH(oldns, &oldconf->conf_isns, i_next) {
		TAILQ_FOREACH(newns, &newconf->conf_isns, i_next) {
			if (strcmp(oldns->i_addr, newns->i_addr) == 0)
				break;
		}
		if (newns == NULL)
			isns_deregister(oldns);
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
	TAILQ_FOREACH_SAFE(oldport, &oldconf->conf_ports, p_next, tmpport) {
		if (port_is_dummy(oldport))
			continue;
		newport = port_find(newconf, oldport->p_name);
		if (newport != NULL && !port_is_dummy(newport))
			continue;
		log_debugx("removing port \"%s\"", oldport->p_name);
		error = kernel_port_remove(oldport);
		if (error != 0) {
			log_warnx("failed to remove port %s",
			    oldport->p_name);
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

		lun_set_ctl_lun(newlun, oldlun->l_ctl_lun);
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
	TAILQ_FOREACH_SAFE(newport, &newconf->conf_ports, p_next, tmpport) {
		if (port_is_dummy(newport))
			continue;
		oldport = port_find(oldconf, newport->p_name);

		if (oldport == NULL || port_is_dummy(oldport)) {
			log_debugx("adding port \"%s\"", newport->p_name);
			error = kernel_port_add(newport);
		} else {
			log_debugx("updating port \"%s\"", newport->p_name);
			newport->p_ctl_port = oldport->p_ctl_port;
			error = kernel_port_update(newport, oldport);
		}
		if (error != 0) {
			log_warnx("failed to %s port %s",
			    (oldport == NULL) ? "add" : "update",
			    newport->p_name);
			if (oldport == NULL || port_is_dummy(oldport))
				port_delete(newport);
			/*
			 * XXX: Uncomment after fixing the root cause.
			 *
			 * cumulated_error++;
			 */
		}
	}

	/*
	 * Go through the new portals, opening the sockets as necessary.
	 */
	TAILQ_FOREACH(newpg, &newconf->conf_portal_groups, pg_next) {
		if (newpg->pg_foreign)
			continue;
		if (newpg->pg_unassigned) {
			log_debugx("not listening on portal-group \"%s\", "
			    "not assigned to any target",
			    newpg->pg_name);
			continue;
		}
		TAILQ_FOREACH(newp, &newpg->pg_portals, p_next) {
			/*
			 * Try to find already open portal and reuse
			 * the listening socket.  We don't care about
			 * what portal or portal group that was, what
			 * matters is the listening address.
			 */
			TAILQ_FOREACH(oldpg, &oldconf->conf_portal_groups,
			    pg_next) {
				TAILQ_FOREACH(oldp, &oldpg->pg_portals,
				    p_next) {
					if (portal_reuse_socket(oldp, newp))
						goto reused;
				}
			}
		reused:
			if (newp->p_socket > 0) {
				/*
				 * We're done with this portal.
				 */
				continue;
			}

#ifdef ICL_KERNEL_PROXY
			if (proxy_mode) {
				newpg->pg_conf->conf_portal_id++;
				newp->p_id = newpg->pg_conf->conf_portal_id;
				log_debugx("listening on %s, portal-group "
				    "\"%s\", portal id %d, using ICL proxy",
				    newp->p_listen, newpg->pg_name, newp->p_id);
				kernel_listen(newp->p_ai, newp->p_iser,
				    newp->p_id);
				continue;
			}
#endif
			assert(proxy_mode == false);
			assert(newp->p_iser == false);

			if (!portal_init_socket(newp)) {
				cumulated_error++;
				continue;
			}
		}
	}

	/*
	 * Go through the no longer used sockets, closing them.
	 */
	TAILQ_FOREACH(oldpg, &oldconf->conf_portal_groups, pg_next) {
		TAILQ_FOREACH(oldp, &oldpg->pg_portals, p_next) {
			if (oldp->p_socket <= 0)
				continue;
			log_debugx("closing socket for %s, portal-group \"%s\"",
			    oldp->p_listen, oldpg->pg_name);
			close(oldp->p_socket);
			oldp->p_socket = 0;
		}
	}

	/* (Re-)Register on remaining/new iSNS servers. */
	TAILQ_FOREACH(newns, &newconf->conf_isns, i_next) {
		TAILQ_FOREACH(oldns, &oldconf->conf_isns, i_next) {
			if (strcmp(oldns->i_addr, newns->i_addr) == 0)
				break;
		}
		isns_register(newns, oldns);
	}

	/* Schedule iSNS update */
	if (!TAILQ_EMPTY(&newconf->conf_isns))
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
	int error;
	pid_t pid;
	char host[NI_MAXHOST + 1];
	struct conf *conf;

	conf = portal->p_portal_group->pg_conf;

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
	    host, portal->p_portal_group->pg_name);
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
			TAILQ_FOREACH(pg, &conf->conf_portal_groups, pg_next) {
				TAILQ_FOREACH(portal, &pg->pg_portals, p_next) {
					if (portal->p_id == portal_id) {
						goto found;
					}
				}
			}

			log_errx(1, "kernel returned invalid portal_id %d",
			    portal_id);

found:
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
				portal = kev.udata;
				assert(portal->p_socket == (int)kev.ident);

				client_salen = sizeof(client_sa);
				client_fd = accept(portal->p_socket,
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
	ag->ag_type = AG_TYPE_NO_AUTHENTICATION;

	ag = auth_group_new(conf, "no-access");
	assert(ag != NULL);
	ag->ag_type = AG_TYPE_DENY;

	pg = portal_group_new(conf, "default");
	assert(pg != NULL);

	if (ucl)
		valid = uclparse_conf(conf, path);
	else
		valid = parse_conf(conf, path);

	if (!valid) {
		conf_delete(conf);
		return (NULL);
	}

	check_perms(path);

	if (conf->conf_default_ag_defined == false) {
		log_debugx("auth-group \"default\" not defined; "
		    "going with defaults");
		ag = auth_group_find(conf, "default");
		assert(ag != NULL);
		ag->ag_type = AG_TYPE_DENY;
	}

	if (conf->conf_default_pg_defined == false) {
		log_debugx("portal-group \"default\" not defined; "
		    "going with defaults");
		pg = portal_group_find(conf, "default");
		assert(pg != NULL);
		portal_group_add_listen(pg, "0.0.0.0", false);
		portal_group_add_listen(pg, "[::]", false);
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
new_pports_from_conf(struct conf *conf, struct kports *kports)
{
	struct target *targ;
	struct pport *pp;
	struct port *tp;
	int ret, i_pp, i_vp;

	TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
		if (!targ->t_pport)
			continue;

		ret = sscanf(targ->t_pport, "ioctl/%d/%d", &i_pp, &i_vp);
		if (ret > 0) {
			tp = port_new_ioctl(conf, kports, targ, i_pp, i_vp);
			if (tp == NULL) {
				log_warnx("can't create new ioctl port "
				    "for target \"%s\"", targ->t_name);
				return (false);
			}

			continue;
		}

		pp = pport_find(kports, targ->t_pport);
		if (pp == NULL) {
			log_warnx("unknown port \"%s\" for target \"%s\"",
			    targ->t_pport, targ->t_name);
			return (false);
		}
		if (!TAILQ_EMPTY(&pp->pp_ports)) {
			log_warnx("can't link port \"%s\" to target \"%s\", "
			    "port already linked to some target",
			    targ->t_pport, targ->t_name);
			return (false);
		}
		tp = port_new_pp(conf, targ, pp);
		if (tp == NULL) {
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
	struct isns *newns;
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

	TAILQ_INIT(&kports.pports);
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

	oldconf = conf_new_from_kernel(&kports);

	if (debug > 0) {
		oldconf->conf_debug = debug;
		newconf->conf_debug = debug;
	}

	if (!new_pports_from_conf(newconf, &kports))
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
	if (!TAILQ_EMPTY(&newconf->conf_isns))
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
			} else if (new_pports_from_conf(tmpconf, &kports)) {
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
				TAILQ_FOREACH(newns, &newconf->conf_isns, i_next)
					isns_check(newns);
				/* Schedule iSNS update */
				if (!TAILQ_EMPTY(&newconf->conf_isns)) {
					set_timeout((newconf->conf_isns_period
					    + 2) / 3,
					    false);
				}
			}
		}
	}
	/* NOTREACHED */
}
