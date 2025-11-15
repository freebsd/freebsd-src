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

#include <libutil++.hh>

#include "conf.h"
#include "ctld.hh"

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
	return (conf->add_isns(addr));
}

void
conf_set_debug(int debug)
{
	conf->set_debug(debug);
}

void
conf_set_isns_period(int period)
{
	conf->set_isns_period(period);
}

void
conf_set_isns_timeout(int timeout)
{
	conf->set_isns_timeout(timeout);
}

void
conf_set_maxproc(int maxproc)
{
	conf->set_maxproc(maxproc);
}

bool
conf_set_pidfile_path(const char *path)
{
	return (conf->set_pidfile_path(path));
}

void
conf_set_timeout(int timeout)
{
	conf->set_timeout(timeout);
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
auth_group_add_host_address(const char *portal)
{
	return (auth_group->add_host_address(portal));
}

bool
auth_group_add_host_nqn(const char *name)
{
	return (auth_group->add_host_nqn(name));
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
	if (strcmp(name, "default") == 0)
		auth_group = conf->define_default_auth_group();
	else
		auth_group = conf->add_auth_group(name);
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
	if (strcmp(name, "default") == 0)
		portal_group = conf->define_default_portal_group();
	else
		portal_group = conf->add_portal_group(name);
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
	return (portal_group->add_portal(listen, iser ? portal_protocol::ISER :
	    portal_protocol::ISCSI));
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
transport_group_start(const char *name)
{
	if (strcmp(name, "default") == 0)
		portal_group = conf->define_default_transport_group();
	else
		portal_group = conf->add_transport_group(name);
	return (portal_group != NULL);
}

bool
transport_group_add_listen_discovery_tcp(const char *listen)
{
	return portal_group->add_portal(listen,
	    portal_protocol::NVME_DISCOVERY_TCP);
}

bool
transport_group_add_listen_tcp(const char *listen)
{
	return portal_group->add_portal(listen, portal_protocol::NVME_TCP);
}

bool
lun_start(const char *name)
{
	lun = conf->add_lun(name);
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
	return (lun->add_option(name, value));
}

bool
lun_set_backend(const char *value)
{
	return (lun->set_backend(value));
}

bool
lun_set_blocksize(size_t value)
{
	return (lun->set_blocksize(value));
}

bool
lun_set_device_type(const char *value)
{
	return (lun->set_device_type(value));
}

bool
lun_set_device_id(const char *value)
{
	return (lun->set_device_id(value));
}

bool
lun_set_path(const char *value)
{
	return (lun->set_path(value));
}

bool
lun_set_serial(const char *value)
{
	return (lun->set_serial(value));
}

bool
lun_set_size(uint64_t value)
{
	return (lun->set_size(value));
}

bool
lun_set_ctl_lun(uint32_t value)
{
	return (lun->set_ctl_lun(value));
}

bool
target_start(const char *name)
{
	target = conf->add_target(name);
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
	return (target->add_chap(user, secret));
}

bool
target_add_chap_mutual(const char *user, const char *secret,
    const char *user2, const char *secret2)
{
	return (target->add_chap_mutual(user, secret, user2, secret2));
}

bool
target_add_initiator_name(const char *name)
{
	return (target->add_initiator_name(name));
}

bool
target_add_initiator_portal(const char *addr)
{
	return (target->add_initiator_portal(addr));
}

bool
target_add_lun(u_int id, const char *name)
{
	return (target->add_lun(id, name));
}

bool
target_add_portal_group(const char *pg_name, const char *ag_name)
{
	return (target->add_portal_group(pg_name, ag_name));
}

bool
target_set_alias(const char *alias)
{
	return (target->set_alias(alias));
}

bool
target_set_auth_group(const char *name)
{
	return (target->set_auth_group(name));
}

bool
target_set_auth_type(const char *type)
{
	return (target->set_auth_type(type));
}

bool
target_set_physical_port(const char *pport)
{
	return (target->set_physical_port(pport));
}

bool
target_set_redirection(const char *addr)
{
	return (target->set_redirection(addr));
}

bool
target_start_lun(u_int id)
{
	lun = target->start_lun(id);
	return (lun != nullptr);
}

bool
controller_start(const char *name)
{
	target = conf->add_controller(name);
	return (target != nullptr);
}

bool
controller_add_host_address(const char *addr)
{
	return (target->add_host_address(addr));
}

bool
controller_add_host_nqn(const char *name)
{
	return (target->add_host_nqn(name));
}

bool
controller_add_namespace(u_int id, const char *name)
{
	return (target->add_namespace(id, name));
}

bool
controller_start_namespace(u_int id)
{
	lun = target->start_namespace(id);
	return (lun != nullptr);
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
	} catch (std::bad_alloc &) {
		log_warnx("failed to allocate memory parsing %s", path);
		return (false);
	} catch (...) {
		log_warnx("unknown exception parsing %s", path);
		return (false);
	}

	return (parsed);
}
