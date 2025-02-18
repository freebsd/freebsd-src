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

#ifndef CTLD_H
#define	CTLD_H

#include <sys/_nv.h>
#include <sys/queue.h>
#ifdef ICL_KERNEL_PROXY
#include <sys/types.h>
#endif
#include <sys/socket.h>
#include <stdbool.h>
#include <libiscsiutil.h>
#include <libutil.h>

#define	DEFAULT_CONFIG_PATH		"/etc/ctl.conf"
#define	DEFAULT_PIDFILE			"/var/run/ctld.pid"
#define	DEFAULT_BLOCKSIZE		512
#define	DEFAULT_CD_BLOCKSIZE		2048

#define	MAX_LUNS			1024
#define	SOCKBUF_SIZE			1048576

struct auth {
	TAILQ_ENTRY(auth)		a_next;
	struct auth_group		*a_auth_group;
	char				*a_user;
	char				*a_secret;
	char				*a_mutual_user;
	char				*a_mutual_secret;
};

struct auth_name {
	TAILQ_ENTRY(auth_name)		an_next;
	struct auth_group		*an_auth_group;
	char				*an_initiator_name;
};

struct auth_portal {
	TAILQ_ENTRY(auth_portal)	ap_next;
	struct auth_group		*ap_auth_group;
	char				*ap_initiator_portal;
	struct sockaddr_storage		ap_sa;
	int				ap_mask;
};

#define	AG_TYPE_UNKNOWN			0
#define	AG_TYPE_DENY			1
#define	AG_TYPE_NO_AUTHENTICATION	2
#define	AG_TYPE_CHAP			3
#define	AG_TYPE_CHAP_MUTUAL		4

struct auth_group {
	TAILQ_ENTRY(auth_group)		ag_next;
	struct conf			*ag_conf;
	char				*ag_name;
	struct target			*ag_target;
	int				ag_type;
	TAILQ_HEAD(, auth)		ag_auths;
	TAILQ_HEAD(, auth_name)		ag_names;
	TAILQ_HEAD(, auth_portal)	ag_portals;
};

struct portal {
	TAILQ_ENTRY(portal)		p_next;
	struct portal_group		*p_portal_group;
	bool				p_iser;
	char				*p_listen;
	struct addrinfo			*p_ai;
#ifdef ICL_KERNEL_PROXY
	int				p_id;
#endif

	TAILQ_HEAD(, target)		p_targets;
	int				p_socket;
};

#define	PG_FILTER_UNKNOWN		0
#define	PG_FILTER_NONE			1
#define	PG_FILTER_PORTAL		2
#define	PG_FILTER_PORTAL_NAME		3
#define	PG_FILTER_PORTAL_NAME_AUTH	4

struct portal_group {
	TAILQ_ENTRY(portal_group)	pg_next;
	struct conf			*pg_conf;
	nvlist_t			*pg_options;
	char				*pg_name;
	struct auth_group		*pg_discovery_auth_group;
	int				pg_discovery_filter;
	bool				pg_foreign;
	bool				pg_unassigned;
	TAILQ_HEAD(, portal)		pg_portals;
	TAILQ_HEAD(, port)		pg_ports;
	char				*pg_offload;
	char				*pg_redirection;
	int				pg_dscp;
	int				pg_pcp;

	uint16_t			pg_tag;
};

/* Ports created by the kernel.  Perhaps the "p" means "physical" ? */
struct pport {
	TAILQ_ENTRY(pport)		pp_next;
	TAILQ_HEAD(, port)		pp_ports;
	struct kports			*pp_kports;
	char				*pp_name;

	uint32_t			pp_ctl_port;
};

struct port {
	TAILQ_ENTRY(port)		p_next;
	TAILQ_ENTRY(port)		p_pgs;
	TAILQ_ENTRY(port)		p_pps;
	TAILQ_ENTRY(port)		p_ts;
	struct conf			*p_conf;
	char				*p_name;
	struct auth_group		*p_auth_group;
	struct portal_group		*p_portal_group;
	struct pport			*p_pport;
	struct target			*p_target;

	bool				p_ioctl_port;
	int				p_ioctl_pp;
	int				p_ioctl_vp;
	uint32_t			p_ctl_port;
};

struct lun {
	TAILQ_ENTRY(lun)		l_next;
	struct conf			*l_conf;
	nvlist_t			*l_options;
	char				*l_name;
	char				*l_backend;
	uint8_t				l_device_type;
	int				l_blocksize;
	char				*l_device_id;
	char				*l_path;
	char				*l_scsiname;
	char				*l_serial;
	int64_t				l_size;

	int				l_ctl_lun;
};

struct target {
	TAILQ_ENTRY(target)		t_next;
	struct conf			*t_conf;
	struct lun			*t_luns[MAX_LUNS];
	struct auth_group		*t_auth_group;
	TAILQ_HEAD(, port)		t_ports;
	char				*t_name;
	char				*t_alias;
	char				*t_redirection;
	/* Name of this target's physical port, if any, i.e. "isp0" */
	char				*t_pport;
};

struct isns {
	TAILQ_ENTRY(isns)		i_next;
	struct conf			*i_conf;
	char				*i_addr;
	struct addrinfo			*i_ai;
};

struct conf {
	char				*conf_pidfile_path;
	TAILQ_HEAD(, lun)		conf_luns;
	TAILQ_HEAD(, target)		conf_targets;
	TAILQ_HEAD(, auth_group)	conf_auth_groups;
	TAILQ_HEAD(, port)		conf_ports;
	TAILQ_HEAD(, portal_group)	conf_portal_groups;
	TAILQ_HEAD(, isns)		conf_isns;
	int				conf_isns_period;
	int				conf_isns_timeout;
	int				conf_debug;
	int				conf_timeout;
	int				conf_maxproc;

#ifdef ICL_KERNEL_PROXY
	int				conf_portal_id;
#endif
	struct pidfh			*conf_pidfh;

	bool				conf_default_pg_defined;
	bool				conf_default_ag_defined;
	bool				conf_kernel_port_on;
};

/* Physical ports exposed by the kernel */
struct kports {
	TAILQ_HEAD(, pport)		pports;
};

#define	CONN_SESSION_TYPE_NONE		0
#define	CONN_SESSION_TYPE_DISCOVERY	1
#define	CONN_SESSION_TYPE_NORMAL	2

struct ctld_connection {
	struct connection	conn;
	struct portal		*conn_portal;
	struct port		*conn_port;
	struct target		*conn_target;
	int			conn_session_type;
	char			*conn_initiator_name;
	char			*conn_initiator_addr;
	char			*conn_initiator_alias;
	uint8_t			conn_initiator_isid[6];
	struct sockaddr_storage	conn_initiator_sa;
	int			conn_max_recv_data_segment_limit;
	int			conn_max_send_data_segment_limit;
	int			conn_max_burst_limit;
	int			conn_first_burst_limit;
	const char		*conn_user;
	struct chap		*conn_chap;
};

extern int ctl_fd;

bool			parse_conf(struct conf *newconf, const char *path);
bool			uclparse_conf(struct conf *conf, const char *path);

struct conf		*conf_new(void);
struct conf		*conf_new_from_kernel(struct kports *kports);
void			conf_delete(struct conf *conf);
bool			conf_verify(struct conf *conf);

struct auth_group	*auth_group_new(struct conf *conf, const char *name);
void			auth_group_delete(struct auth_group *ag);
struct auth_group	*auth_group_find(const struct conf *conf,
			    const char *name);
bool			auth_group_set_type(struct auth_group *ag,
			    const char *type);

const struct auth	*auth_new_chap(struct auth_group *ag,
			    const char *user, const char *secret);
const struct auth	*auth_new_chap_mutual(struct auth_group *ag,
			    const char *user, const char *secret,
			    const char *user2, const char *secret2);
const struct auth	*auth_find(const struct auth_group *ag,
			    const char *user);

const struct auth_name	*auth_name_new(struct auth_group *ag,
			    const char *initiator_name);
bool			auth_name_defined(const struct auth_group *ag);
const struct auth_name	*auth_name_find(const struct auth_group *ag,
			    const char *initiator_name);
bool			auth_name_check(const struct auth_group *ag,
			    const char *initiator_name);

const struct auth_portal	*auth_portal_new(struct auth_group *ag,
				    const char *initiator_portal);
bool			auth_portal_defined(const struct auth_group *ag);
const struct auth_portal	*auth_portal_find(const struct auth_group *ag,
				    const struct sockaddr_storage *sa);
bool				auth_portal_check(const struct auth_group *ag,
				    const struct sockaddr_storage *sa);

struct portal_group	*portal_group_new(struct conf *conf, const char *name);
void			portal_group_delete(struct portal_group *pg);
struct portal_group	*portal_group_find(const struct conf *conf,
			    const char *name);
bool			portal_group_add_listen(struct portal_group *pg,
			    const char *listen, bool iser);
bool			portal_group_set_filter(struct portal_group *pg,
			    const char *filter);
bool			portal_group_set_offload(struct portal_group *pg,
			    const char *offload);
bool			portal_group_set_redirection(struct portal_group *pg,
			    const char *addr);

bool			isns_new(struct conf *conf, const char *addr);
void			isns_delete(struct isns *is);
void			isns_register(struct isns *isns, struct isns *oldisns);
void			isns_check(struct isns *isns);
void			isns_deregister(struct isns *isns);

struct pport		*pport_new(struct kports *kports, const char *name,
			    uint32_t ctl_port);
struct pport		*pport_find(const struct kports *kports,
			    const char *name);
struct pport		*pport_copy(struct pport *pp, struct kports *kports);
void			pport_delete(struct pport *pport);

struct port		*port_new(struct conf *conf, struct target *target,
			    struct portal_group *pg);
struct port		*port_new_ioctl(struct conf *conf,
			    struct kports *kports, struct target *target,
			    int pp, int vp);
struct port		*port_new_pp(struct conf *conf, struct target *target,
			    struct pport *pp);
struct port		*port_find(const struct conf *conf, const char *name);
struct port		*port_find_in_pg(const struct portal_group *pg,
			    const char *target);
void			port_delete(struct port *port);
bool			port_is_dummy(struct port *port);

struct target		*target_new(struct conf *conf, const char *name);
void			target_delete(struct target *target);
struct target		*target_find(struct conf *conf,
			    const char *name);
bool			target_set_redirection(struct target *target,
			    const char *addr);

struct lun		*lun_new(struct conf *conf, const char *name);
void			lun_delete(struct lun *lun);
struct lun		*lun_find(const struct conf *conf, const char *name);
void			lun_set_backend(struct lun *lun, const char *value);
void			lun_set_device_type(struct lun *lun, uint8_t value);
void			lun_set_blocksize(struct lun *lun, size_t value);
void			lun_set_device_id(struct lun *lun, const char *value);
void			lun_set_path(struct lun *lun, const char *value);
void			lun_set_scsiname(struct lun *lun, const char *value);
void			lun_set_serial(struct lun *lun, const char *value);
void			lun_set_size(struct lun *lun, int64_t value);
void			lun_set_ctl_lun(struct lun *lun, uint32_t value);

bool			option_new(nvlist_t *nvl,
			    const char *name, const char *value);

void			kernel_init(void);
int			kernel_lun_add(struct lun *lun);
int			kernel_lun_modify(struct lun *lun);
int			kernel_lun_remove(struct lun *lun);
void			kernel_handoff(struct ctld_connection *conn);
int			kernel_port_add(struct port *port);
int			kernel_port_update(struct port *port, struct port *old);
int			kernel_port_remove(struct port *port);
void			kernel_capsicate(void);

#ifdef ICL_KERNEL_PROXY
void			kernel_listen(struct addrinfo *ai, bool iser,
			    int portal_id);
void			kernel_accept(int *connection_id, int *portal_id,
			    struct sockaddr *client_sa,
			    socklen_t *client_salen);
void			kernel_send(struct pdu *pdu);
void			kernel_receive(struct pdu *pdu);
#endif

void			login(struct ctld_connection *conn);

void			discovery(struct ctld_connection *conn);

void			set_timeout(int timeout, int fatal);

#endif /* !CTLD_H */
