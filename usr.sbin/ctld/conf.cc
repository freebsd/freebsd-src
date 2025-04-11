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

#include <libutil++>

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

bool
auth_group_add_chap(const char *user, const char *secret)
{
	return (auth_group->add_chap(user, secret));
}

bool
auth_group_add_chap_mutual(const char *user, const char *secret,
    const char *user2, const char *secret2)
{
	return (auth_group->add_chap_mutual(user, secret, user2, secret2));
}

bool
auth_group_add_initiator_name(const char *name)
{
	return (auth_group->add_initiator_name(name));
}

bool
auth_group_add_initiator_portal(const char *portal)
{
	return (auth_group->add_initiator_portal(portal));
}

bool
auth_group_set_type(const char *type)
{
	return (auth_group->set_type(type));
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
		auth_group = auth_group_find(conf, "default").get();
		return (true);
	}

	auth_group = auth_group_new(conf, name);
	return (auth_group != nullptr);
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
	return (portal_group->add_portal(listen, iser));
}

bool
portal_group_add_option(const char *name, const char *value)
{
	return (portal_group->add_option(name, value));
}

bool
portal_group_set_discovery_auth_group(const char *name)
{
	return (portal_group->set_discovery_auth_group(name));
}

bool
portal_group_set_dscp(u_int dscp)
{
	return (portal_group->set_dscp(dscp));
}

bool
portal_group_set_filter(const char *str)
{
	return (portal_group->set_filter(str));
}

void
portal_group_set_foreign(void)
{
	portal_group->set_foreign();
}

bool
portal_group_set_offload(const char *offload)
{
	return (portal_group->set_offload(offload));
}

bool
portal_group_set_pcp(u_int pcp)
{
	return (portal_group->set_pcp(pcp));
}

bool
portal_group_set_redirection(const char *addr)
{
	return (portal_group->set_redirection(addr));
}

void
portal_group_set_tag(uint16_t tag)
{
	portal_group->set_tag(tag);
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

static bool
target_use_private_auth(const char *keyword)
{
	if (target->t_auth_group != nullptr) {
		if (!target->t_private_auth) {
			log_warnx("cannot use both auth-group and "
			    "%s for target \"%s\"", keyword, target->t_name);
			return (false);
		}
	} else {
		target->t_auth_group = auth_group_new(target);
		if (target->t_auth_group == nullptr)
			return (false);
		target->t_private_auth = true;
	}
	return (true);
}

bool
target_add_chap(const char *user, const char *secret)
{
	if (!target_use_private_auth("chap"))
		return (false);
	return (target->t_auth_group->add_chap(user, secret));
}

bool
target_add_chap_mutual(const char *user, const char *secret,
    const char *user2, const char *secret2)
{
	if (!target_use_private_auth("chap-mutual"))
		return (false);
	return (target->t_auth_group->add_chap_mutual(user, secret, user2,
	    secret2));
}

bool
target_add_initiator_name(const char *name)
{
	if (!target_use_private_auth("initiator-name"))
		return (false);
	return (target->t_auth_group->add_initiator_name(name));
}

bool
target_add_initiator_portal(const char *addr)
{
	if (!target_use_private_auth("initiator-portal"))
		return (false);
	return (target->t_auth_group->add_initiator_portal(addr));
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
	auth_group_sp ag;

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
	}

	if (!port_new(conf, target, pg, std::move(ag))) {
		log_warnx("can't link portal-group \"%s\" to target \"%s\"",
		    pg_name, target->t_name);
		return (false);
	}
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
	if (target->t_auth_group != nullptr) {
		if (target->t_private_auth)
			log_warnx("cannot use both auth-group and explicit "
			    "authorisations for target \"%s\"", target->t_name);
		else
			log_warnx("auth-group for target \"%s\" "
			    "specified more than once", target->t_name);
		return (false);
	}
	target->t_auth_group = auth_group_find(conf, name);
	if (target->t_auth_group == nullptr) {
		log_warnx("unknown auth-group \"%s\" for target \"%s\"", name,
		    target->t_name);
		return (false);
	}
	return (true);
}

bool
target_set_auth_type(const char *type)
{
	if (!target_use_private_auth("auth-type"))
		return (false);
	return (target->t_auth_group->set_type(type));
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

bool
parse_conf(const char *path)
{
	freebsd::FILE_up fp(fopen(path, "r"));
	if (fp == nullptr) {
		log_warn("unable to open configuration file %s", path);
		return (false);
	}

	bool parsed;
	try {
		parsed = yyparse_conf(fp.get());
	} catch (std::bad_alloc) {
		log_warnx("failed to allocate memory parsing %s", path);
		return (false);
	} catch (...) {
		log_warnx("unknown exception parsing %s", path);
		return (false);
	}

	return (parsed);
}
