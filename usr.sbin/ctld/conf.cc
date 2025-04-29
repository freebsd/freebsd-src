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
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <cam/scsi/scsi_all.h>

#include "conf.h"
#include "ctld.h"

static struct conf *conf = NULL;
static struct auth_group *auth_group = NULL;
static struct portal_group *portal_group = NULL;
static struct target *target = NULL;
static struct lun *lun = NULL;

void
conf_start(struct conf *new_conf)
{
	assert(conf == NULL);
	conf = new_conf;
}

void
conf_finish(void)
{
	auth_group = NULL;
	portal_group = NULL;
	target = NULL;
	lun = NULL;
	conf = NULL;
}

bool
isns_add_server(const char *addr)
{
	return (isns_new(conf, addr));
}

void
conf_set_debug(int debug)
{
	conf->conf_debug = debug;
}

void
conf_set_isns_period(int period)
{
	conf->conf_isns_period = period;
}

void
conf_set_isns_timeout(int timeout)
{
	conf->conf_isns_timeout = timeout;
}

void
conf_set_maxproc(int maxproc)
{
	conf->conf_maxproc = maxproc;
}

bool
conf_set_pidfile_path(const char *path)
{
	if (conf->conf_pidfile_path != NULL) {
		log_warnx("pidfile specified more than once");
		return (false);
	}
	conf->conf_pidfile_path = checked_strdup(path);
	return (true);
}

void
conf_set_timeout(int timeout)
{
	conf->conf_timeout = timeout;
}

static bool
_auth_group_set_type(struct auth_group *ag, const char *str)
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
		log_warnx("invalid auth-type \"%s\" for %s", str, ag->ag_label);
		return (false);
	}

	if (ag->ag_type != AG_TYPE_UNKNOWN && ag->ag_type != type) {
		log_warnx("cannot set auth-type to \"%s\" for %s; "
		    "already has a different type", str, ag->ag_label);
		return (false);
	}

	ag->ag_type = type;

	return (true);
}

bool
auth_group_add_chap(const char *user, const char *secret)
{
	return (auth_new_chap(auth_group, user, secret));
}

bool
auth_group_add_chap_mutual(const char *user, const char *secret,
    const char *user2, const char *secret2)
{
	return (auth_new_chap_mutual(auth_group, user, secret, user2, secret2));
}

bool
auth_group_add_initiator_name(const char *name)
{
	return (auth_name_new(auth_group, name));
}

bool
auth_group_add_initiator_portal(const char *portal)
{
	return (auth_portal_new(auth_group, portal));
}

bool
auth_group_set_type(const char *type)
{
	return (_auth_group_set_type(auth_group, type));
}

bool
auth_group_start(const char *name)
{
	/*
	 * Make it possible to redefine the default auth-group. but
	 * only once.
	 */
	if (strcmp(name, "default") == 0) {
		if (conf->conf_default_ag_defined) {
			log_warnx("duplicated auth-group \"default\"");
			return (false);
		}

		conf->conf_default_ag_defined = true;
		auth_group = auth_group_find(conf, "default");
		return (true);
	}

	auth_group = auth_group_new(conf, name);
	return (auth_group != NULL);
}

void
auth_group_finish(void)
{
	auth_group = NULL;
}

bool
portal_group_start(const char *name)
{
	/*
	 * Make it possible to redefine the default portal-group. but
	 * only once.
	 */
	if (strcmp(name, "default") == 0) {
		if (conf->conf_default_pg_defined) {
			log_warnx("duplicated portal-group \"default\"");
			return (false);
		}

		conf->conf_default_pg_defined = true;
		portal_group = portal_group_find(conf, "default");
		return (true);
	}

	portal_group = portal_group_new(conf, name);
	return (portal_group != NULL);
}

void
portal_group_finish(void)
{
	portal_group = NULL;
}

bool
portal_group_add_listen(const char *listen, bool iser)
{
	return (portal_group_add_portal(portal_group, listen, iser));
}

bool
portal_group_add_option(const char *name, const char *value)
{
	return (option_new(portal_group->pg_options, name, value));
}

bool
portal_group_set_discovery_auth_group(const char *name)
{
	if (portal_group->pg_discovery_auth_group != NULL) {
		log_warnx("discovery-auth-group for portal-group "
		    "\"%s\" specified more than once",
		    portal_group->pg_name);
		return (false);
	}
	portal_group->pg_discovery_auth_group = auth_group_find(conf, name);
	if (portal_group->pg_discovery_auth_group == NULL) {
		log_warnx("unknown discovery-auth-group \"%s\" "
		    "for portal-group \"%s\"", name, portal_group->pg_name);
		return (false);
	}
	return (true);
}

bool
portal_group_set_dscp(u_int dscp)
{
	if (dscp >= 0x40) {
		log_warnx("invalid DSCP value %u for portal-group \"%s\"",
		    dscp, portal_group->pg_name);
		return (false);
	}

	portal_group->pg_dscp = dscp;
	return (true);
}

bool
portal_group_set_filter(const char *str)
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
		    str, portal_group->pg_name);
		return (false);
	}

	if (portal_group->pg_discovery_filter != PG_FILTER_UNKNOWN &&
	    portal_group->pg_discovery_filter != filter) {
		log_warnx("cannot set discovery-filter to \"%s\" for "
		    "portal-group \"%s\"; already has a different "
		    "value", str, portal_group->pg_name);
		return (false);
	}

	portal_group->pg_discovery_filter = filter;

	return (true);
}

void
portal_group_set_foreign(void)
{
	portal_group->pg_foreign = true;
}

bool
portal_group_set_offload(const char *offload)
{

	if (portal_group->pg_offload != NULL) {
		log_warnx("cannot set offload to \"%s\" for "
		    "portal-group \"%s\"; already defined",
		    offload, portal_group->pg_name);
		return (false);
	}

	portal_group->pg_offload = checked_strdup(offload);

	return (true);
}

bool
portal_group_set_pcp(u_int pcp)
{
	if (pcp > 7) {
		log_warnx("invalid PCP value %u for portal-group \"%s\"",
		    pcp, portal_group->pg_name);
		return (false);
	}

	portal_group->pg_pcp = pcp;
	return (true);
}

bool
portal_group_set_redirection(const char *addr)
{

	if (portal_group->pg_redirection != NULL) {
		log_warnx("cannot set redirection to \"%s\" for "
		    "portal-group \"%s\"; already defined",
		    addr, portal_group->pg_name);
		return (false);
	}

	portal_group->pg_redirection = checked_strdup(addr);

	return (true);
}

void
portal_group_set_tag(uint16_t tag)
{
	portal_group->pg_tag = tag;
}

bool
lun_start(const char *name)
{
	lun = lun_new(conf, name);
	return (lun != NULL);
}

void
lun_finish(void)
{
	lun = NULL;
}

bool
lun_add_option(const char *name, const char *value)
{
	return (option_new(lun->l_options, name, value));
}

bool
lun_set_backend(const char *value)
{
	if (lun->l_backend != NULL) {
		log_warnx("backend for lun \"%s\" specified more than once",
		    lun->l_name);
		return (false);
	}

	lun->l_backend = checked_strdup(value);
	return (true);
}

bool
lun_set_blocksize(size_t value)
{
	if (lun->l_blocksize != 0) {
		log_warnx("blocksize for lun \"%s\" specified more than once",
		    lun->l_name);
		return (false);
	}
	lun->l_blocksize = value;
	return (true);
}

bool
lun_set_device_type(const char *value)
{
	uint64_t device_type;

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
	else if (expand_number(value, &device_type) != 0 || device_type > 15) {
		log_warnx("invalid device-type \"%s\" for lun \"%s\"", value,
		    lun->l_name);
		return (false);
	}

	lun->l_device_type = device_type;
	return (true);
}

bool
lun_set_device_id(const char *value)
{
	if (lun->l_device_id != NULL) {
		log_warnx("device_id for lun \"%s\" specified more than once",
		    lun->l_name);
		return (false);
	}

	lun->l_device_id = checked_strdup(value);
	return (true);
}

bool
lun_set_path(const char *value)
{
	if (lun->l_path != NULL) {
		log_warnx("path for lun \"%s\" specified more than once",
		    lun->l_name);
		return (false);
	}

	lun->l_path = checked_strdup(value);
	return (true);
}

bool
lun_set_serial(const char *value)
{
	if (lun->l_serial != NULL) {
		log_warnx("serial for lun \"%s\" specified more than once",
		    lun->l_name);
		return (false);
	}

	lun->l_serial = checked_strdup(value);
	return (true);
}

bool
lun_set_size(uint64_t value)
{
	if (lun->l_size != 0) {
		log_warnx("size for lun \"%s\" specified more than once",
		    lun->l_name);
		return (false);
	}

	lun->l_size = value;
	return (true);
}

bool
lun_set_ctl_lun(uint32_t value)
{

	if (lun->l_ctl_lun >= 0) {
		log_warnx("ctl_lun for lun \"%s\" specified more than once",
		    lun->l_name);
		return (false);
	}
	lun->l_ctl_lun = value;
	return (true);
}

bool
target_start(const char *name)
{
	target = target_new(conf, name);
	return (target != NULL);
}

void
target_finish(void)
{
	target = NULL;
}

bool
target_add_chap(const char *user, const char *secret)
{
	if (target->t_auth_group != NULL) {
		if (target->t_auth_group->ag_name != NULL) {
			log_warnx("cannot use both auth-group and "
			    "chap for target \"%s\"", target->t_name);
			return (false);
		}
	} else {
		target->t_auth_group = auth_group_new(conf, target);
		if (target->t_auth_group == NULL)
			return (false);
	}
	return (auth_new_chap(target->t_auth_group, user, secret));
}

bool
target_add_chap_mutual(const char *user, const char *secret,
    const char *user2, const char *secret2)
{
	if (target->t_auth_group != NULL) {
		if (target->t_auth_group->ag_name != NULL) {
			log_warnx("cannot use both auth-group and "
			    "chap-mutual for target \"%s\"", target->t_name);
			return (false);
		}
	} else {
		target->t_auth_group = auth_group_new(conf, target);
		if (target->t_auth_group == NULL)
			return (false);
	}
	return (auth_new_chap_mutual(target->t_auth_group, user, secret, user2,
	    secret2));
}

bool
target_add_initiator_name(const char *name)
{
	if (target->t_auth_group != NULL) {
		if (target->t_auth_group->ag_name != NULL) {
			log_warnx("cannot use both auth-group and "
			    "initiator-name for target \"%s\"", target->t_name);
			return (false);
		}
	} else {
		target->t_auth_group = auth_group_new(conf, target);
		if (target->t_auth_group == NULL)
			return (false);
	}
	return (auth_name_new(target->t_auth_group, name));
}

bool
target_add_initiator_portal(const char *addr)
{
	if (target->t_auth_group != NULL) {
		if (target->t_auth_group->ag_name != NULL) {
			log_warnx("cannot use both auth-group and "
			    "initiator-portal for target \"%s\"",
			    target->t_name);
			return (false);
		}
	} else {
		target->t_auth_group = auth_group_new(conf, target);
		if (target->t_auth_group == NULL)
			return (false);
	}
	return (auth_portal_new(target->t_auth_group, addr));
}

bool
target_add_lun(u_int id, const char *name)
{
	struct lun *t_lun;

	if (id >= MAX_LUNS) {
		log_warnx("LUN %u too big for target \"%s\"", id,
		    target->t_name);
		return (false);
	}

	if (target->t_luns[id] != NULL) {
		log_warnx("duplicate LUN %u for target \"%s\"", id,
		    target->t_name);
		return (false);
	}

	t_lun = lun_find(conf, name);
	if (t_lun == NULL) {
		log_warnx("unknown LUN named %s used for target \"%s\"",
		    name, target->t_name);
		return (false);
	}

	target->t_luns[id] = t_lun;
	return (true);
}

bool
target_add_portal_group(const char *pg_name, const char *ag_name)
{
	struct portal_group *pg;
	struct auth_group *ag;
	struct port *p;

	pg = portal_group_find(conf, pg_name);
	if (pg == NULL) {
		log_warnx("unknown portal-group \"%s\" for target \"%s\"",
		    pg_name, target->t_name);
		return (false);
	}

	if (ag_name != NULL) {
		ag = auth_group_find(conf, ag_name);
		if (ag == NULL) {
			log_warnx("unknown auth-group \"%s\" for target \"%s\"",
			    ag_name, target->t_name);
			return (false);
		}
	} else
		ag = NULL;

	p = port_new(conf, target, pg);
	if (p == NULL) {
		log_warnx("can't link portal-group \"%s\" to target \"%s\"",
		    pg_name, target->t_name);
		return (false);
	}
	p->p_auth_group = ag;
	return (true);
}

bool
target_set_alias(const char *alias)
{
	if (target->t_alias != NULL) {
		log_warnx("alias for target \"%s\" specified more than once",
		    target->t_name);
		return (false);
	}
	target->t_alias = checked_strdup(alias);
	return (true);
}

bool
target_set_auth_group(const char *name)
{
	if (target->t_auth_group != NULL) {
		if (target->t_auth_group->ag_name != NULL)
			log_warnx("auth-group for target \"%s\" "
			    "specified more than once", target->t_name);
		else
			log_warnx("cannot use both auth-group and explicit "
			    "authorisations for target \"%s\"", target->t_name);
		return (false);
	}
	target->t_auth_group = auth_group_find(conf, name);
	if (target->t_auth_group == NULL) {
		log_warnx("unknown auth-group \"%s\" for target \"%s\"", name,
		    target->t_name);
		return (false);
	}
	return (true);
}

bool
target_set_auth_type(const char *type)
{
	if (target->t_auth_group != NULL) {
		if (target->t_auth_group->ag_name != NULL) {
			log_warnx("cannot use both auth-group and "
			    "auth-type for target \"%s\"", target->t_name);
			return (false);
		}
	} else {
		target->t_auth_group = auth_group_new(conf, target);
		if (target->t_auth_group == NULL)
			return (false);
	}
	return (_auth_group_set_type(target->t_auth_group, type));
}

bool
target_set_physical_port(const char *pport)
{
	if (target->t_pport != NULL) {
		log_warnx("cannot set multiple physical ports for target "
		    "\"%s\"", target->t_name);
		return (false);
	}
	target->t_pport = checked_strdup(pport);
	return (true);
}

bool
target_set_redirection(const char *addr)
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

bool
target_start_lun(u_int id)
{
	struct lun *new_lun;
	char *name;

	if (id >= MAX_LUNS) {
		log_warnx("LUN %u too big for target \"%s\"", id,
		    target->t_name);
		return (false);
	}

	if (target->t_luns[id] != NULL) {
		log_warnx("duplicate LUN %u for target \"%s\"", id,
		    target->t_name);
		return (false);
	}

	if (asprintf(&name, "%s,lun,%u", target->t_name, id) <= 0)
		log_err(1, "asprintf");

	new_lun = lun_new(conf, name);
	if (new_lun == NULL)
		return (false);

	lun_set_scsiname(new_lun, name);
	free(name);

	target->t_luns[id] = new_lun;

	lun = new_lun;
	return (true);
}
