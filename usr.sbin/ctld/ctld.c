/*-
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
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

bool proxy_mode = false;

static volatile bool sighup_received = false;
static volatile bool sigterm_received = false;
static volatile bool sigalrm_received = false;

static int nchildren = 0;

static void
usage(void)
{

	fprintf(stderr, "usage: ctld [-d][-f config-file]\n");
	exit(1);
}

char *
checked_strdup(const char *s)
{
	char *c;

	c = strdup(s);
	if (c == NULL)
		log_err(1, "strdup");
	return (c);
}

struct conf *
conf_new(void)
{
	struct conf *conf;

	conf = calloc(1, sizeof(*conf));
	if (conf == NULL)
		log_err(1, "calloc");
	TAILQ_INIT(&conf->conf_targets);
	TAILQ_INIT(&conf->conf_auth_groups);
	TAILQ_INIT(&conf->conf_portal_groups);

	conf->conf_debug = 0;
	conf->conf_timeout = 60;
	conf->conf_maxproc = 30;

	return (conf);
}

void
conf_delete(struct conf *conf)
{
	struct target *targ, *tmp;
	struct auth_group *ag, *cagtmp;
	struct portal_group *pg, *cpgtmp;

	assert(conf->conf_pidfh == NULL);

	TAILQ_FOREACH_SAFE(targ, &conf->conf_targets, t_next, tmp)
		target_delete(targ);
	TAILQ_FOREACH_SAFE(ag, &conf->conf_auth_groups, ag_next, cagtmp)
		auth_group_delete(ag);
	TAILQ_FOREACH_SAFE(pg, &conf->conf_portal_groups, pg_next, cpgtmp)
		portal_group_delete(pg);
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
			    "is too short; it should be at least 16 characters "
			    "long", auth->a_user,
			    auth->a_auth_group->ag_target->t_name);
	}

	if (auth->a_mutual_secret != NULL) {
		len = strlen(auth->a_secret);
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
				    "at least 16 characters long",
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
	an->an_initator_name = checked_strdup(name);
	TAILQ_INSERT_TAIL(&ag->ag_names, an, an_next);
	return (an);
}

static void
auth_name_delete(struct auth_name *an)
{
	TAILQ_REMOVE(&an->an_auth_group->ag_names, an, an_next);

	free(an->an_initator_name);
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
		if (strcmp(auth_name->an_initator_name, name) == 0)
			return (auth_name);
	}

	return (NULL);
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
	ap->ap_initator_portal = checked_strdup(portal);
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
	log_errx(1, "Incorrect initiator portal '%s'", portal);
	return (NULL);
}

static void
auth_portal_delete(struct auth_portal *ap)
{
	TAILQ_REMOVE(&ap->ap_auth_group->ag_portals, ap, ap_next);

	free(ap->ap_initator_portal);
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

	TAILQ_FOREACH(ag, &conf->conf_auth_groups, ag_next) {
		if (ag->ag_name != NULL && strcmp(ag->ag_name, name) == 0)
			return (ag);
	}

	return (NULL);
}

static int
auth_group_set_type(struct auth_group *ag, int type)
{

	if (ag->ag_type == AG_TYPE_UNKNOWN) {
		ag->ag_type = type;
		return (0);
	}

	if (ag->ag_type == type)
		return (0);

	return (1);
}

int
auth_group_set_type_str(struct auth_group *ag, const char *str)
{
	int error, type;

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
		return (1);
	}

	error = auth_group_set_type(ag, type);
	if (error != 0) {
		if (ag->ag_name != NULL)
			log_warnx("cannot set auth-type to \"%s\" for "
			    "auth-group \"%s\"; already has a different "
			    "type", str, ag->ag_name);
		else
			log_warnx("cannot set auth-type to \"%s\" for target "
			    "\"%s\"; already has a different type",
			    str, ag->ag_target->t_name);
		return (1);
	}

	return (error);
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
	TAILQ_INIT(&pg->pg_portals);
	pg->pg_conf = conf;
	conf->conf_last_portal_group_tag++;
	pg->pg_tag = conf->conf_last_portal_group_tag;
	TAILQ_INSERT_TAIL(&conf->conf_portal_groups, pg, pg_next);

	return (pg);
}

void
portal_group_delete(struct portal_group *pg)
{
	struct portal *portal, *tmp;

	TAILQ_REMOVE(&pg->pg_conf->conf_portal_groups, pg, pg_next);

	TAILQ_FOREACH_SAFE(portal, &pg->pg_portals, p_next, tmp)
		portal_delete(portal);
	free(pg->pg_name);
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

int
portal_group_add_listen(struct portal_group *pg, const char *value, bool iser)
{
	struct addrinfo hints;
	struct portal *portal;
	char *addr, *ch, *arg;
	const char *port;
	int error, colons = 0;

	portal = portal_new(pg);
	portal->p_listen = checked_strdup(value);
	portal->p_iser = iser;

	arg = portal->p_listen;
	if (arg[0] == '\0') {
		log_warnx("empty listen address");
		portal_delete(portal);
		return (1);
	}
	if (arg[0] == '[') {
		/*
		 * IPv6 address in square brackets, perhaps with port.
		 */
		arg++;
		addr = strsep(&arg, "]");
		if (arg == NULL) {
			log_warnx("invalid listen address %s",
			    portal->p_listen);
			portal_delete(portal);
			return (1);
		}
		if (arg[0] == '\0') {
			port = "3260";
		} else if (arg[0] == ':') {
			port = arg + 1;
		} else {
			log_warnx("invalid listen address %s",
			    portal->p_listen);
			portal_delete(portal);
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
			port = "3260";
		} else {
			addr = strsep(&arg, ":");
			if (arg == NULL)
				port = "3260";
			else
				port = arg;
		}
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	error = getaddrinfo(addr, port, &hints, &portal->p_ai);
	if (error != 0) {
		log_warnx("getaddrinfo for %s failed: %s",
		    portal->p_listen, gai_strerror(error));
		portal_delete(portal);
		return (1);
	}

	/*
	 * XXX: getaddrinfo(3) may return multiple addresses; we should turn
	 *	those into multiple portals.
	 */

	return (0);
}

static bool
valid_hex(const char ch)
{
	switch (ch) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case 'a':
	case 'A':
	case 'b':
	case 'B':
	case 'c':
	case 'C':
	case 'd':
	case 'D':
	case 'e':
	case 'E':
	case 'f':
	case 'F':
		return (true);
	default:
		return (false);
	}
}

bool
valid_iscsi_name(const char *name)
{
	int i;

	if (strlen(name) >= MAX_NAME_LEN) {
		log_warnx("overlong name for target \"%s\"; max length allowed "
		    "by iSCSI specification is %d characters",
		    name, MAX_NAME_LEN);
		return (false);
	}

	/*
	 * In the cases below, we don't return an error, just in case the admin
	 * was right, and we're wrong.
	 */
	if (strncasecmp(name, "iqn.", strlen("iqn.")) == 0) {
		for (i = strlen("iqn."); name[i] != '\0'; i++) {
			/*
			 * XXX: We should verify UTF-8 normalisation, as defined
			 * 	by 3.2.6.2: iSCSI Name Encoding.
			 */
			if (isalnum(name[i]))
				continue;
			if (name[i] == '-' || name[i] == '.' || name[i] == ':')
				continue;
			log_warnx("invalid character \"%c\" in target name "
			    "\"%s\"; allowed characters are letters, digits, "
			    "'-', '.', and ':'", name[i], name);
			break;
		}
		/*
		 * XXX: Check more stuff: valid date and a valid reversed domain.
		 */
	} else if (strncasecmp(name, "eui.", strlen("eui.")) == 0) {
		if (strlen(name) != strlen("eui.") + 16)
			log_warnx("invalid target name \"%s\"; the \"eui.\" "
			    "should be followed by exactly 16 hexadecimal "
			    "digits", name);
		for (i = strlen("eui."); name[i] != '\0'; i++) {
			if (!valid_hex(name[i])) {
				log_warnx("invalid character \"%c\" in target "
				    "name \"%s\"; allowed characters are 1-9 "
				    "and A-F", name[i], name);
				break;
			}
		}
	} else if (strncasecmp(name, "naa.", strlen("naa.")) == 0) {
		if (strlen(name) > strlen("naa.") + 32)
			log_warnx("invalid target name \"%s\"; the \"naa.\" "
			    "should be followed by at most 32 hexadecimal "
			    "digits", name);
		for (i = strlen("naa."); name[i] != '\0'; i++) {
			if (!valid_hex(name[i])) {
				log_warnx("invalid character \"%c\" in target "
				    "name \"%s\"; allowed characters are 1-9 "
				    "and A-F", name[i], name);
				break;
			}
		}
	} else {
		log_warnx("invalid target name \"%s\"; should start with "
		    "either \".iqn\", \"eui.\", or \"naa.\"",
		    name);
	}
	return (true);
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
	if (valid_iscsi_name(name) == false) {
		log_warnx("target name \"%s\" is invalid", name);
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

	TAILQ_INIT(&targ->t_luns);
	targ->t_conf = conf;
	TAILQ_INSERT_TAIL(&conf->conf_targets, targ, t_next);

	return (targ);
}

void
target_delete(struct target *targ)
{
	struct lun *lun, *tmp;

	TAILQ_REMOVE(&targ->t_conf->conf_targets, targ, t_next);

	TAILQ_FOREACH_SAFE(lun, &targ->t_luns, l_next, tmp)
		lun_delete(lun);
	free(targ->t_name);
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

struct lun *
lun_new(struct target *targ, int lun_id)
{
	struct lun *lun;

	lun = lun_find(targ, lun_id);
	if (lun != NULL) {
		log_warnx("duplicated lun %d for target \"%s\"",
		    lun_id, targ->t_name);
		return (NULL);
	}

	lun = calloc(1, sizeof(*lun));
	if (lun == NULL)
		log_err(1, "calloc");
	lun->l_lun = lun_id;
	TAILQ_INIT(&lun->l_options);
	lun->l_target = targ;
	TAILQ_INSERT_TAIL(&targ->t_luns, lun, l_next);

	return (lun);
}

void
lun_delete(struct lun *lun)
{
	struct lun_option *lo, *tmp;

	TAILQ_REMOVE(&lun->l_target->t_luns, lun, l_next);

	TAILQ_FOREACH_SAFE(lo, &lun->l_options, lo_next, tmp)
		lun_option_delete(lo);
	free(lun->l_backend);
	free(lun->l_device_id);
	free(lun->l_path);
	free(lun->l_serial);
	free(lun);
}

struct lun *
lun_find(const struct target *targ, int lun_id)
{
	struct lun *lun;

	TAILQ_FOREACH(lun, &targ->t_luns, l_next) {
		if (lun->l_lun == lun_id)
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
lun_set_serial(struct lun *lun, const char *value)
{
	free(lun->l_serial);
	lun->l_serial = checked_strdup(value);
}

void
lun_set_size(struct lun *lun, size_t value)
{

	lun->l_size = value;
}

void
lun_set_ctl_lun(struct lun *lun, uint32_t value)
{

	lun->l_ctl_lun = value;
}

struct lun_option *
lun_option_new(struct lun *lun, const char *name, const char *value)
{
	struct lun_option *lo;

	lo = lun_option_find(lun, name);
	if (lo != NULL) {
		log_warnx("duplicated lun option %s for lun %d, target \"%s\"",
		    name, lun->l_lun, lun->l_target->t_name);
		return (NULL);
	}

	lo = calloc(1, sizeof(*lo));
	if (lo == NULL)
		log_err(1, "calloc");
	lo->lo_name = checked_strdup(name);
	lo->lo_value = checked_strdup(value);
	lo->lo_lun = lun;
	TAILQ_INSERT_TAIL(&lun->l_options, lo, lo_next);

	return (lo);
}

void
lun_option_delete(struct lun_option *lo)
{

	TAILQ_REMOVE(&lo->lo_lun->l_options, lo, lo_next);

	free(lo->lo_name);
	free(lo->lo_value);
	free(lo);
}

struct lun_option *
lun_option_find(const struct lun *lun, const char *name)
{
	struct lun_option *lo;

	TAILQ_FOREACH(lo, &lun->l_options, lo_next) {
		if (strcmp(lo->lo_name, name) == 0)
			return (lo);
	}

	return (NULL);
}

void
lun_option_set(struct lun_option *lo, const char *value)
{

	free(lo->lo_value);
	lo->lo_value = checked_strdup(value);
}

static struct connection *
connection_new(struct portal *portal, int fd, const char *host,
    const struct sockaddr *client_sa)
{
	struct connection *conn;

	conn = calloc(1, sizeof(*conn));
	if (conn == NULL)
		log_err(1, "calloc");
	conn->conn_portal = portal;
	conn->conn_socket = fd;
	conn->conn_initiator_addr = checked_strdup(host);
	memcpy(&conn->conn_initiator_sa, client_sa, client_sa->sa_len);

	/*
	 * Default values, from RFC 3720, section 12.
	 */
	conn->conn_max_data_segment_length = 8192;
	conn->conn_max_burst_length = 262144;
	conn->conn_immediate_data = true;

	return (conn);
}

#if 0
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
	struct lun_option *lo;

	TAILQ_FOREACH(ag, &conf->conf_auth_groups, ag_next) {
		fprintf(stderr, "auth-group %s {\n", ag->ag_name);
		TAILQ_FOREACH(auth, &ag->ag_auths, a_next)
			fprintf(stderr, "\t chap-mutual %s %s %s %s\n",
			    auth->a_user, auth->a_secret,
			    auth->a_mutual_user, auth->a_mutual_secret);
		TAILQ_FOREACH(auth_name, &ag->ag_names, an_next)
			fprintf(stderr, "\t initiator-name %s\n",
			    auth_name->an_initator_name);
		TAILQ_FOREACH(auth_portal, &ag->ag_portals, an_next)
			fprintf(stderr, "\t initiator-portal %s\n",
			    auth_portal->an_initator_portal);
		fprintf(stderr, "}\n");
	}
	TAILQ_FOREACH(pg, &conf->conf_portal_groups, pg_next) {
		fprintf(stderr, "portal-group %s {\n", pg->pg_name);
		TAILQ_FOREACH(portal, &pg->pg_portals, p_next)
			fprintf(stderr, "\t listen %s\n", portal->p_listen);
		fprintf(stderr, "}\n");
	}
	TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
		fprintf(stderr, "target %s {\n", targ->t_name);
		if (targ->t_alias != NULL)
			fprintf(stderr, "\t alias %s\n", targ->t_alias);
		TAILQ_FOREACH(lun, &targ->t_luns, l_next) {
			fprintf(stderr, "\tlun %d {\n", lun->l_lun);
			fprintf(stderr, "\t\tpath %s\n", lun->l_path);
			TAILQ_FOREACH(lo, &lun->l_options, lo_next)
				fprintf(stderr, "\t\toption %s %s\n",
				    lo->lo_name, lo->lo_value);
			fprintf(stderr, "\t}\n");
		}
		fprintf(stderr, "}\n");
	}
}
#endif

static int
conf_verify_lun(struct lun *lun)
{
	const struct lun *lun2;
	const struct target *targ2;

	if (lun->l_backend == NULL)
		lun_set_backend(lun, "block");
	if (strcmp(lun->l_backend, "block") == 0) {
		if (lun->l_path == NULL) {
			log_warnx("missing path for lun %d, target \"%s\"",
			    lun->l_lun, lun->l_target->t_name);
			return (1);
		}
	} else if (strcmp(lun->l_backend, "ramdisk") == 0) {
		if (lun->l_size == 0) {
			log_warnx("missing size for ramdisk-backed lun %d, "
			    "target \"%s\"", lun->l_lun, lun->l_target->t_name);
			return (1);
		}
		if (lun->l_path != NULL) {
			log_warnx("path must not be specified "
			    "for ramdisk-backed lun %d, target \"%s\"",
			    lun->l_lun, lun->l_target->t_name);
			return (1);
		}
	}
	if (lun->l_lun < 0 || lun->l_lun > 255) {
		log_warnx("invalid lun number for lun %d, target \"%s\"; "
		    "must be between 0 and 255", lun->l_lun,
		    lun->l_target->t_name);
		return (1);
	}
	if (lun->l_blocksize == 0) {
		lun_set_blocksize(lun, DEFAULT_BLOCKSIZE);
	} else if (lun->l_blocksize < 0) {
		log_warnx("invalid blocksize for lun %d, target \"%s\"; "
		    "must be larger than 0", lun->l_lun, lun->l_target->t_name);
		return (1);
	}
	if (lun->l_size != 0 && lun->l_size % lun->l_blocksize != 0) {
		log_warnx("invalid size for lun %d, target \"%s\"; "
		    "must be multiple of blocksize", lun->l_lun,
		    lun->l_target->t_name);
		return (1);
	}
	TAILQ_FOREACH(targ2, &lun->l_target->t_conf->conf_targets, t_next) {
		TAILQ_FOREACH(lun2, &targ2->t_luns, l_next) {
			if (lun == lun2)
				continue;
			if (lun->l_path != NULL && lun2->l_path != NULL &&
			    strcmp(lun->l_path, lun2->l_path) == 0) {
				log_debugx("WARNING: path \"%s\" duplicated "
				    "between lun %d, target \"%s\", and "
				    "lun %d, target \"%s\"", lun->l_path,
				    lun->l_lun, lun->l_target->t_name,
				    lun2->l_lun, lun2->l_target->t_name);
			}
		}
	}

	return (0);
}

int
conf_verify(struct conf *conf)
{
	struct auth_group *ag;
	struct portal_group *pg;
	struct target *targ;
	struct lun *lun;
	bool found_lun;
	int error;

	if (conf->conf_pidfile_path == NULL)
		conf->conf_pidfile_path = checked_strdup(DEFAULT_PIDFILE);

	TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
		if (targ->t_auth_group == NULL) {
			targ->t_auth_group = auth_group_find(conf,
			    "default");
			assert(targ->t_auth_group != NULL);
		}
		if (targ->t_portal_group == NULL) {
			targ->t_portal_group = portal_group_find(conf,
			    "default");
			assert(targ->t_portal_group != NULL);
		}
		found_lun = false;
		TAILQ_FOREACH(lun, &targ->t_luns, l_next) {
			error = conf_verify_lun(lun);
			if (error != 0)
				return (error);
			found_lun = true;
		}
		if (!found_lun) {
			log_warnx("no LUNs defined for target \"%s\"",
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

		TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
			if (targ->t_portal_group == pg)
				break;
		}
		if (targ == NULL) {
			if (strcmp(pg->pg_name, "default") != 0)
				log_warnx("portal-group \"%s\" not assigned "
				    "to any target", pg->pg_name);
			pg->pg_unassigned = true;
		} else
			pg->pg_unassigned = false;
	}
	TAILQ_FOREACH(ag, &conf->conf_auth_groups, ag_next) {
		if (ag->ag_name == NULL)
			assert(ag->ag_target != NULL);
		else
			assert(ag->ag_target == NULL);

		TAILQ_FOREACH(targ, &conf->conf_targets, t_next) {
			if (targ->t_auth_group == ag)
				break;
		}
		if (targ == NULL && ag->ag_name != NULL &&
		    strcmp(ag->ag_name, "default") != 0 &&
		    strcmp(ag->ag_name, "no-authentication") != 0 &&
		    strcmp(ag->ag_name, "no-access") != 0) {
			log_warnx("auth-group \"%s\" not assigned "
			    "to any target", ag->ag_name);
		}
	}

	return (0);
}

static int
conf_apply(struct conf *oldconf, struct conf *newconf)
{
	struct target *oldtarg, *newtarg, *tmptarg;
	struct lun *oldlun, *newlun, *tmplun;
	struct portal_group *oldpg, *newpg;
	struct portal *oldp, *newp;
	pid_t otherpid;
	int changed, cumulated_error = 0, error;
	int one = 1;

	if (oldconf->conf_debug != newconf->conf_debug) {
		log_debugx("changing debug level to %d", newconf->conf_debug);
		log_init(newconf->conf_debug);
	}

	if (oldconf->conf_pidfh != NULL) {
		assert(oldconf->conf_pidfile_path != NULL);
		if (newconf->conf_pidfile_path != NULL &&
		    strcmp(oldconf->conf_pidfile_path,
		    newconf->conf_pidfile_path) == 0) {
			newconf->conf_pidfh = oldconf->conf_pidfh;
			oldconf->conf_pidfh = NULL;
		} else {
			log_debugx("removing pidfile %s",
			    oldconf->conf_pidfile_path);
			pidfile_remove(oldconf->conf_pidfh);
			oldconf->conf_pidfh = NULL;
		}
	}

	if (newconf->conf_pidfh == NULL && newconf->conf_pidfile_path != NULL) {
		log_debugx("opening pidfile %s", newconf->conf_pidfile_path);
		newconf->conf_pidfh =
		    pidfile_open(newconf->conf_pidfile_path, 0600, &otherpid);
		if (newconf->conf_pidfh == NULL) {
			if (errno == EEXIST)
				log_errx(1, "daemon already running, pid: %jd.",
				    (intmax_t)otherpid);
			log_err(1, "cannot open or create pidfile \"%s\"",
			    newconf->conf_pidfile_path);
		}
	}

	/*
	 * XXX: If target or lun removal fails, we should somehow "move"
	 * 	the old lun or target into newconf, so that subsequent
	 * 	conf_apply() would try to remove them again.  That would
	 * 	be somewhat hairy, though, and lun deletion failures don't
	 * 	really happen, so leave it as it is for now.
	 */
	TAILQ_FOREACH_SAFE(oldtarg, &oldconf->conf_targets, t_next, tmptarg) {
		/*
		 * First, remove any targets present in the old configuration
		 * and missing in the new one.
		 */
		newtarg = target_find(newconf, oldtarg->t_name);
		if (newtarg == NULL) {
			TAILQ_FOREACH_SAFE(oldlun, &oldtarg->t_luns, l_next,
			    tmplun) {
				log_debugx("target %s not found in new "
				    "configuration; removing its lun %d, "
				    "backed by CTL lun %d",
				    oldtarg->t_name, oldlun->l_lun,
				    oldlun->l_ctl_lun);
				error = kernel_lun_remove(oldlun);
				if (error != 0) {
					log_warnx("failed to remove lun %d, "
					    "target %s, CTL lun %d",
					    oldlun->l_lun, oldtarg->t_name,
					    oldlun->l_ctl_lun);
					cumulated_error++;
				}
				lun_delete(oldlun);
			}
			kernel_port_remove(oldtarg);
			target_delete(oldtarg);
			continue;
		}

		/*
		 * Second, remove any LUNs present in the old target
		 * and missing in the new one.
		 */
		TAILQ_FOREACH_SAFE(oldlun, &oldtarg->t_luns, l_next, tmplun) {
			newlun = lun_find(newtarg, oldlun->l_lun);
			if (newlun == NULL) {
				log_debugx("lun %d, target %s, CTL lun %d "
				    "not found in new configuration; "
				    "removing", oldlun->l_lun, oldtarg->t_name,
				    oldlun->l_ctl_lun);
				error = kernel_lun_remove(oldlun);
				if (error != 0) {
					log_warnx("failed to remove lun %d, "
					    "target %s, CTL lun %d",
					    oldlun->l_lun, oldtarg->t_name,
					    oldlun->l_ctl_lun);
					cumulated_error++;
				}
				lun_delete(oldlun);
				continue;
			}

			/*
			 * Also remove the LUNs changed by more than size.
			 */
			changed = 0;
			assert(oldlun->l_backend != NULL);
			assert(newlun->l_backend != NULL);
			if (strcmp(newlun->l_backend, oldlun->l_backend) != 0) {
				log_debugx("backend for lun %d, target %s, "
				    "CTL lun %d changed; removing",
				    oldlun->l_lun, oldtarg->t_name,
				    oldlun->l_ctl_lun);
				changed = 1;
			}
			if (oldlun->l_blocksize != newlun->l_blocksize) {
				log_debugx("blocksize for lun %d, target %s, "
				    "CTL lun %d changed; removing",
				    oldlun->l_lun, oldtarg->t_name,
				    oldlun->l_ctl_lun);
				changed = 1;
			}
			if (newlun->l_device_id != NULL &&
			    (oldlun->l_device_id == NULL ||
			     strcmp(oldlun->l_device_id, newlun->l_device_id) !=
			     0)) {
				log_debugx("device-id for lun %d, target %s, "
				    "CTL lun %d changed; removing",
				    oldlun->l_lun, oldtarg->t_name,
				    oldlun->l_ctl_lun);
				changed = 1;
			}
			if (newlun->l_path != NULL &&
			    (oldlun->l_path == NULL ||
			     strcmp(oldlun->l_path, newlun->l_path) != 0)) {
				log_debugx("path for lun %d, target %s, "
				    "CTL lun %d, changed; removing",
				    oldlun->l_lun, oldtarg->t_name,
				    oldlun->l_ctl_lun);
				changed = 1;
			}
			if (newlun->l_serial != NULL &&
			    (oldlun->l_serial == NULL ||
			     strcmp(oldlun->l_serial, newlun->l_serial) != 0)) {
				log_debugx("serial for lun %d, target %s, "
				    "CTL lun %d changed; removing",
				    oldlun->l_lun, oldtarg->t_name,
				    oldlun->l_ctl_lun);
				changed = 1;
			}
			if (changed) {
				error = kernel_lun_remove(oldlun);
				if (error != 0) {
					log_warnx("failed to remove lun %d, "
					    "target %s, CTL lun %d",
					    oldlun->l_lun, oldtarg->t_name,
					    oldlun->l_ctl_lun);
					cumulated_error++;
				}
				lun_delete(oldlun);
				continue;
			}

			lun_set_ctl_lun(newlun, oldlun->l_ctl_lun);
		}
	}

	/*
	 * Now add new targets or modify existing ones.
	 */
	TAILQ_FOREACH(newtarg, &newconf->conf_targets, t_next) {
		oldtarg = target_find(oldconf, newtarg->t_name);

		TAILQ_FOREACH_SAFE(newlun, &newtarg->t_luns, l_next, tmplun) {
			if (oldtarg != NULL) {
				oldlun = lun_find(oldtarg, newlun->l_lun);
				if (oldlun != NULL) {
					if (newlun->l_size != oldlun->l_size) {
						log_debugx("resizing lun %d, "
						    "target %s, CTL lun %d",
						    newlun->l_lun,
						    newtarg->t_name,
						    newlun->l_ctl_lun);
						error =
						    kernel_lun_resize(newlun);
						if (error != 0) {
							log_warnx("failed to "
							    "resize lun %d, "
							    "target %s, "
							    "CTL lun %d",
							    newlun->l_lun,
							    newtarg->t_name,
							    newlun->l_lun);
							cumulated_error++;
						}
					}
					continue;
				}
			}
			log_debugx("adding lun %d, target %s",
			    newlun->l_lun, newtarg->t_name);
			error = kernel_lun_add(newlun);
			if (error != 0) {
				log_warnx("failed to add lun %d, target %s",
				    newlun->l_lun, newtarg->t_name);
				lun_delete(newlun);
				cumulated_error++;
			}
		}
		if (oldtarg == NULL)
			kernel_port_add(newtarg);
	}

	/*
	 * Go through the new portals, opening the sockets as neccessary.
	 */
	TAILQ_FOREACH(newpg, &newconf->conf_portal_groups, pg_next) {
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
					if (strcmp(newp->p_listen,
					    oldp->p_listen) == 0 &&
					    oldp->p_socket > 0) {
						newp->p_socket =
						    oldp->p_socket;
						oldp->p_socket = 0;
						break;
					}
				}
			}
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

			log_debugx("listening on %s, portal-group \"%s\"",
			    newp->p_listen, newpg->pg_name);
			newp->p_socket = socket(newp->p_ai->ai_family,
			    newp->p_ai->ai_socktype,
			    newp->p_ai->ai_protocol);
			if (newp->p_socket < 0) {
				log_warn("socket(2) failed for %s",
				    newp->p_listen);
				cumulated_error++;
				continue;
			}
			error = setsockopt(newp->p_socket, SOL_SOCKET,
			    SO_REUSEADDR, &one, sizeof(one));
			if (error != 0) {
				log_warn("setsockopt(SO_REUSEADDR) failed "
				    "for %s", newp->p_listen);
				close(newp->p_socket);
				newp->p_socket = 0;
				cumulated_error++;
				continue;
			}
			error = bind(newp->p_socket, newp->p_ai->ai_addr,
			    newp->p_ai->ai_addrlen);
			if (error != 0) {
				log_warn("bind(2) failed for %s",
				    newp->p_listen);
				close(newp->p_socket);
				newp->p_socket = 0;
				cumulated_error++;
				continue;
			}
			error = listen(newp->p_socket, -1);
			if (error != 0) {
				log_warn("listen(2) failed for %s",
				    newp->p_listen);
				close(newp->p_socket);
				newp->p_socket = 0;
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

	return (cumulated_error);
}

bool
timed_out(void)
{

	return (sigalrm_received);
}

static void
sigalrm_handler(int dummy __unused)
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
set_timeout(const struct conf *conf)
{
	struct sigaction sa;
	struct itimerval itv;
	int error;

	if (conf->conf_timeout <= 0) {
		log_debugx("session timeout disabled");
		return;
	}

	bzero(&sa, sizeof(sa));
	sa.sa_handler = sigalrm_handler;
	sigfillset(&sa.sa_mask);
	error = sigaction(SIGALRM, &sa, NULL);
	if (error != 0)
		log_err(1, "sigaction");

	/*
	 * First SIGALRM will arive after conf_timeout seconds.
	 * If we do nothing, another one will arrive a second later.
	 */
	bzero(&itv, sizeof(itv));
	itv.it_interval.tv_sec = 1;
	itv.it_value.tv_sec = conf->conf_timeout;

	log_debugx("setting session timeout to %d seconds",
	    conf->conf_timeout);
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
	struct connection *conn;
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
	}
	pidfile_close(conf->conf_pidfh);

	error = getnameinfo(client_sa, client_sa->sa_len,
	    host, sizeof(host), NULL, 0, NI_NUMERICHOST);
	if (error != 0)
		log_errx(1, "getnameinfo: %s", gai_strerror(error));

	log_debugx("accepted connection from %s; portal group \"%s\"",
	    host, portal->p_portal_group->pg_name);
	log_set_peer_addr(host);
	setproctitle("%s", host);

	conn = connection_new(portal, fd, host, client_sa);
	set_timeout(conf);
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

static int
fd_add(int fd, fd_set *fdset, int nfds)
{

	/*
	 * Skip sockets which we failed to bind.
	 */
	if (fd <= 0)
		return (nfds);

	FD_SET(fd, fdset);
	if (fd > nfds)
		nfds = fd;
	return (nfds);
}

static void
main_loop(struct conf *conf, bool dont_fork)
{
	struct portal_group *pg;
	struct portal *portal;
	struct sockaddr_storage client_sa;
	socklen_t client_salen;
#ifdef ICL_KERNEL_PROXY
	int connection_id;
	int portal_id;
#endif
	fd_set fdset;
	int error, nfds, client_fd;

	pidfile_write(conf->conf_pidfh);

	for (;;) {
		if (sighup_received || sigterm_received)
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

			FD_ZERO(&fdset);
			nfds = 0;
			TAILQ_FOREACH(pg, &conf->conf_portal_groups, pg_next) {
				TAILQ_FOREACH(portal, &pg->pg_portals, p_next)
					nfds = fd_add(portal->p_socket, &fdset, nfds);
			}
			error = select(nfds + 1, &fdset, NULL, NULL, NULL);
			if (error <= 0) {
				if (errno == EINTR)
					return;
				log_err(1, "select");
			}
			TAILQ_FOREACH(pg, &conf->conf_portal_groups, pg_next) {
				TAILQ_FOREACH(portal, &pg->pg_portals, p_next) {
					if (!FD_ISSET(portal->p_socket, &fdset))
						continue;
					client_salen = sizeof(client_sa);
					client_fd = accept(portal->p_socket,
					    (struct sockaddr *)&client_sa,
					    &client_salen);
					if (client_fd < 0)
						log_err(1, "accept");
					assert(client_salen >= client_sa.ss_len);

					handle_connection(portal, client_fd,
					    (struct sockaddr *)&client_sa,
					    dont_fork);
					break;
				}
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

int
main(int argc, char **argv)
{
	struct conf *oldconf, *newconf, *tmpconf;
	const char *config_path = DEFAULT_CONFIG_PATH;
	int debug = 0, ch, error;
	bool dont_daemonize = false;

	while ((ch = getopt(argc, argv, "df:R")) != -1) {
		switch (ch) {
		case 'd':
			dont_daemonize = true;
			debug++;
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

	oldconf = conf_new_from_kernel();
	newconf = conf_new_from_file(config_path);
	if (newconf == NULL)
		log_errx(1, "configuration error; exiting");
	if (debug > 0) {
		oldconf->conf_debug = debug;
		newconf->conf_debug = debug;
	}

	error = conf_apply(oldconf, newconf);
	if (error != 0)
		log_errx(1, "failed to apply configuration; exiting");

	conf_delete(oldconf);
	oldconf = NULL;

	register_signals();

	if (dont_daemonize == false) {
		log_debugx("daemonizing");
		if (daemon(0, 0) == -1) {
			log_warn("cannot daemonize");
			pidfile_remove(newconf->conf_pidfh);
			exit(1);
		}
	}

	for (;;) {
		main_loop(newconf, dont_daemonize);
		if (sighup_received) {
			sighup_received = false;
			log_debugx("received SIGHUP, reloading configuration");
			tmpconf = conf_new_from_file(config_path);
			if (tmpconf == NULL) {
				log_warnx("configuration error, "
				    "continuing with old configuration");
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

			log_debugx("disabling CTL iSCSI port "
			    "and terminating all connections");

			oldconf = newconf;
			newconf = conf_new();
			if (debug > 0)
				newconf->conf_debug = debug;
			error = conf_apply(oldconf, newconf);
			if (error != 0)
				log_warnx("failed to apply configuration");

			log_warnx("exiting on signal");
			exit(0);
		} else {
			nchildren -= wait_for_children(false);
			assert(nchildren >= 0);
		}
	}
	/* NOTREACHED */
}
