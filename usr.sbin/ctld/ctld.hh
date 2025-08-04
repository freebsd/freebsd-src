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

#ifndef __CTLD_HH__
#define	__CTLD_HH__

#include <sys/_nv.h>
#include <sys/queue.h>
#ifdef ICL_KERNEL_PROXY
#include <sys/types.h>
#endif
#include <sys/socket.h>
#include <stdbool.h>
#include <libiscsiutil.h>
#include <libutil.h>

#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#define	DEFAULT_CONFIG_PATH		"/etc/ctl.conf"
#define	DEFAULT_PIDFILE			"/var/run/ctld.pid"
#define	DEFAULT_BLOCKSIZE		512
#define	DEFAULT_CD_BLOCKSIZE		2048

#define	MAX_LUNS			1024
#define	SOCKBUF_SIZE			1048576

struct auth {
	auth(std::string_view secret) : a_secret(secret) {}
	auth(std::string_view secret, std::string_view mutual_user,
	    std::string_view mutual_secret) :
		a_secret(secret), a_mutual_user(mutual_user),
		a_mutual_secret(mutual_secret) {}

	bool mutual() const { return !a_mutual_user.empty(); }

	const char *secret() const { return a_secret.c_str(); }
	const char *mutual_user() const { return a_mutual_user.c_str(); }
	const char *mutual_secret() const { return a_mutual_secret.c_str(); }

private:
	std::string			a_secret;
	std::string			a_mutual_user;
	std::string			a_mutual_secret;
};

struct auth_portal {
	bool matches(const struct sockaddr *sa) const;
	bool parse(const char *portal);

private:
	struct sockaddr_storage		ap_sa;
	int				ap_mask = 0;
};

enum class auth_type {
	UNKNOWN,
	DENY,
	NO_AUTHENTICATION,
	CHAP,
	CHAP_MUTUAL
};

struct auth_group {
	auth_group(std::string label) : ag_label(label) {}

	auth_type type() const { return ag_type; }
	bool set_type(const char *str);
	void set_type(auth_type type);

	const char *label() const { return ag_label.c_str(); }

	bool add_chap(const char *user, const char *secret);
	bool add_chap_mutual(const char *user, const char *secret,
	    const char *user2, const char *secret2);
	const struct auth *find_auth(std::string_view user) const;

	bool add_initiator_name(std::string_view initiator_name);
	bool initiator_permitted(std::string_view initiator_name) const;

	bool add_initiator_portal(const char *initiator_portal);
	bool initiator_permitted(const struct sockaddr *sa) const;

private:
	void check_secret_length(const char *user, const char *secret,
	    const char *secret_type);

	std::string			ag_label;
	auth_type			ag_type = auth_type::UNKNOWN;
	std::unordered_map<std::string, auth> ag_auths;
	std::unordered_set<std::string> ag_names;
	std::list<auth_portal>		ag_portals;
};

using auth_group_sp = std::shared_ptr<auth_group>;

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
	auth_group_sp			pg_discovery_auth_group;
	int				pg_discovery_filter = PG_FILTER_UNKNOWN;
	bool				pg_foreign = false;
	bool				pg_unassigned = false;
	TAILQ_HEAD(, portal)		pg_portals;
	TAILQ_HEAD(, port)		pg_ports;
	char				*pg_offload = nullptr;
	char				*pg_redirection = nullptr;
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
	auth_group_sp			p_auth_group;
	struct portal_group		*p_portal_group = nullptr;
	struct pport			*p_pport = nullptr;
	struct target			*p_target;

	bool				p_ioctl_port = false;
	int				p_ioctl_pp = 0;
	int				p_ioctl_vp = 0;
	uint32_t			p_ctl_port = 0;
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
	uint64_t			l_size;

	int				l_ctl_lun;
};

struct target {
	TAILQ_ENTRY(target)		t_next;
	struct conf			*t_conf;
	struct lun			*t_luns[MAX_LUNS] = {};
	auth_group_sp			t_auth_group;
	TAILQ_HEAD(, port)		t_ports;
	char				*t_name;
	char				*t_alias;
	char				*t_redirection;
	/* Name of this target's physical port, if any, i.e. "isp0" */
	char				*t_pport;
	bool				t_private_auth;
};

struct isns {
	TAILQ_ENTRY(isns)		i_next;
	struct conf			*i_conf;
	char				*i_addr;
	struct addrinfo			*i_ai;
};

struct conf {
	char				*conf_pidfile_path = nullptr;
	TAILQ_HEAD(, lun)		conf_luns;
	TAILQ_HEAD(, target)		conf_targets;
	std::unordered_map<std::string, auth_group_sp> conf_auth_groups;
	TAILQ_HEAD(, port)		conf_ports;
	TAILQ_HEAD(, portal_group)	conf_portal_groups;
	TAILQ_HEAD(, isns)		conf_isns;
	int				conf_isns_period;
	int				conf_isns_timeout;
	int				conf_debug;
	int				conf_timeout;
	int				conf_maxproc;

#ifdef ICL_KERNEL_PROXY
	int				conf_portal_id = 0;
#endif
	struct pidfh			*conf_pidfh = nullptr;

	bool				conf_default_pg_defined = false;
	bool				conf_default_ag_defined = false;
	bool				conf_kernel_port_on = false;
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
	const struct sockaddr	*conn_initiator_sa;
	int			conn_max_recv_data_segment_limit;
	int			conn_max_send_data_segment_limit;
	int			conn_max_burst_limit;
	int			conn_first_burst_limit;
	const char		*conn_user;
	struct chap		*conn_chap;
};

extern int ctl_fd;

bool			parse_conf(const char *path);
bool			uclparse_conf(const char *path);

struct conf		*conf_new(void);
struct conf		*conf_new_from_kernel(struct kports *kports);
void			conf_delete(struct conf *conf);
void			conf_finish(void);
void			conf_start(struct conf *new_conf);
bool			conf_verify(struct conf *conf);

struct auth_group	*auth_group_new(struct conf *conf, const char *name);
auth_group_sp		auth_group_new(struct target *target);
auth_group_sp		auth_group_find(const struct conf *conf,
			    const char *name);

struct portal_group	*portal_group_new(struct conf *conf, const char *name);
void			portal_group_delete(struct portal_group *pg);
struct portal_group	*portal_group_find(const struct conf *conf,
			    const char *name);
bool			portal_group_add_portal(struct portal_group *pg,
			    const char *value, bool iser);

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

struct lun		*lun_new(struct conf *conf, const char *name);
void			lun_delete(struct lun *lun);
struct lun		*lun_find(const struct conf *conf, const char *name);
void			lun_set_scsiname(struct lun *lun, const char *value);

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

#endif /* !__CTLD_HH__ */
