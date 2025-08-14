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
 */

#ifndef __CONF_H__
#define	__CONF_H__

/*
 * This file defines the interface between parse.y and the rest of
 * ctld.
 */

__BEGIN_DECLS

bool	auth_group_start(const char *name);
void	auth_group_finish(void);
bool	auth_group_add_chap(const char *user, const char *secret);
bool	auth_group_add_chap_mutual(const char *user, const char *secret,
	    const char *user2, const char *secret2);
bool	auth_group_add_host_address(const char *portal);
bool	auth_group_add_host_nqn(const char *name);
bool	auth_group_add_initiator_name(const char *name);
bool	auth_group_add_initiator_portal(const char *portal);
bool	auth_group_set_type(const char *type);

void	conf_set_debug(int debug);
void	conf_set_isns_period(int period);
void	conf_set_isns_timeout(int timeout);
void	conf_set_maxproc(int maxproc);
bool	conf_set_pidfile_path(const char *path);
void	conf_set_timeout(int timeout);

bool	isns_add_server(const char *addr);

bool	portal_group_start(const char *name);
void	portal_group_finish(void);
bool	portal_group_add_listen(const char *listen, bool iser);
bool	portal_group_add_option(const char *name, const char *value);
bool	portal_group_set_discovery_auth_group(const char *name);
bool	portal_group_set_dscp(u_int dscp);
bool	portal_group_set_filter(const char *filter);
void	portal_group_set_foreign(void);
bool	portal_group_set_offload(const char *offload);
bool	portal_group_set_pcp(u_int pcp);
bool	portal_group_set_redirection(const char *addr);
void	portal_group_set_tag(uint16_t tag);

bool	transport_group_start(const char *name);
bool	transport_group_add_listen_discovery_tcp(const char *listen);
bool	transport_group_add_listen_tcp(const char *listen);

bool	target_start(const char *name);
void	target_finish(void);
bool	target_add_chap(const char *user, const char *secret);
bool	target_add_chap_mutual(const char *user, const char *secret,
	    const char *user2, const char *secret2);
bool	target_add_initiator_name(const char *name);
bool	target_add_initiator_portal(const char *addr);
bool	target_add_lun(u_int id, const char *name);
bool	target_add_portal_group(const char *pg_name, const char *ag_name);
bool	target_set_alias(const char *alias);
bool	target_set_auth_group(const char *name);
bool	target_set_auth_type(const char *type);
bool	target_set_physical_port(const char *pport);
bool	target_set_redirection(const char *addr);
bool	target_start_lun(u_int id);

bool	controller_start(const char *name);
bool	controller_add_host_address(const char *addr);
bool	controller_add_host_nqn(const char *name);
bool	controller_add_namespace(u_int id, const char *name);
bool	controller_start_namespace(u_int id);

bool	lun_start(const char *name);
void	lun_finish(void);
bool	lun_add_option(const char *name, const char *value);
bool	lun_set_backend(const char *value);
bool	lun_set_blocksize(size_t value);
bool	lun_set_ctl_lun(uint32_t value);
bool	lun_set_device_id(const char *value);
bool	lun_set_device_type(const char *value);
bool	lun_set_path(const char *value);
bool	lun_set_serial(const char *value);
bool	lun_set_size(uint64_t value);

bool	yyparse_conf(FILE *fp);

__END_DECLS

#endif /* !__CONF_H__ */
