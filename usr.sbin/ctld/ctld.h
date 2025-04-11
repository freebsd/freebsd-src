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

#include <array>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <libutil++>

#define	DEFAULT_CONFIG_PATH		"/etc/ctl.conf"
#define	DEFAULT_PIDFILE			"/var/run/ctld.pid"
#define	DEFAULT_BLOCKSIZE		512
#define	DEFAULT_CD_BLOCKSIZE		2048

#define	MAX_LUNS			1024
#define	SOCKBUF_SIZE			1048576

struct isns_req;
struct port;

struct auth {
	auth(const char *secret) : a_secret(secret) {}
	auth(const char *secret, const char *mutual_user,
	    const char *mutual_secret) : a_secret(secret),
	    a_mutual_user(mutual_user), a_mutual_secret(mutual_secret) {}

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
	auth_portal(const struct sockaddr_storage *ss, int mask) : ap_sa(*ss),
	    ap_mask(mask) {}

	bool matches(const struct sockaddr_storage *ss) const;

private:
	struct sockaddr_storage		ap_sa;
	int				ap_mask;
};

#define	AG_TYPE_UNKNOWN			0
#define	AG_TYPE_DENY			1
#define	AG_TYPE_NO_AUTHENTICATION	2
#define	AG_TYPE_CHAP			3
#define	AG_TYPE_CHAP_MUTUAL		4

struct auth_group {
	auth_group(std::string label) : ag_label(label) {}

	int type() const { return ag_type; }
	bool set_type(const char *str);
	void set_type(int type);

	const char *label() const { return ag_label.c_str(); }

	bool add_chap(const char *user, const char *secret);
	bool add_chap_mutual(const char *user, const char *secret,
	    const char *user2, const char *secret2);
	const struct auth *find_auth(const char *user) const;

	bool add_initiator_name(const char *initiator_name);
	bool initiator_permitted(const char *initiator_name) const;

	bool add_initiator_portal(const char *initiator_portal);
	bool initiator_permitted(const struct sockaddr_storage *sa) const;

private:
	void check_secret_length(const char *user, const char *secret,
	    const char *secret_type);

	std::string			ag_label;
	int				ag_type = AG_TYPE_UNKNOWN;
	std::unordered_map<std::string, auth> ag_auths;
	std::unordered_set<std::string> ag_names;
	std::list<auth_portal>		ag_portals;
};

typedef std::shared_ptr<auth_group> auth_group_sp;

struct portal {
	portal(struct portal_group *pg, const char *listen, bool iser,
	    struct addrinfo *ai) : p_portal_group(pg), p_listen(listen),
				   p_ai(ai), p_iser(iser) {}

	bool reuse_socket(portal &oldp);
	bool init_socket();

	portal_group *portal_group() { return p_portal_group; }
	const char *listen() const { return p_listen.c_str(); }
	const addrinfo *ai() const { return p_ai.get(); }
	int socket() const { return p_socket; }
	void close() { p_socket.reset(); }

private:
	struct portal_group		*p_portal_group;
	std::string			p_listen;
	freebsd::addrinfo_up		p_ai;
	bool				p_iser;

	freebsd::fd_up			p_socket;
};

typedef std::unique_ptr<portal> portal_up;

#define	PG_FILTER_UNKNOWN		0
#define	PG_FILTER_NONE			1
#define	PG_FILTER_PORTAL		2
#define	PG_FILTER_PORTAL_NAME		3
#define	PG_FILTER_PORTAL_NAME_AUTH	4

struct portal_group {
	portal_group(struct conf *conf, const char *name);

	struct conf *conf() const { return pg_conf; }
	const char *name() const { return pg_name.c_str(); }
	bool assigned() const { return pg_assigned; }
	bool is_dummy() const;
	bool is_redirecting() const { return !pg_redirection.empty(); }
	struct auth_group *discovery_auth_group() const
	{ return pg_discovery_auth_group.get(); }
	int discovery_filter() const { return pg_discovery_filter; }
	int dscp() const { return pg_dscp; }
	const char *offload() const { return pg_offload.c_str(); }
	const char *redirection() const { return pg_redirection.c_str(); }
	int pcp() const { return pg_pcp; }
	uint16_t tag() const { return pg_tag; }

	freebsd::nvlist_up options() const;

	const std::list<portal_up> &portals() const { return pg_portals; }
	const std::unordered_map<std::string, port *> &ports() const
	{ return pg_ports; }

	bool add_portal(const char *value, bool iser);
	bool add_option(const char *name, const char *value);
	bool set_discovery_auth_group(const char *name);
	bool set_dscp(u_int dscp);
	bool set_filter(const char *str);
	void set_foreign();
	bool set_offload(const char *offload);
	bool set_pcp(u_int pcp);
	bool set_redirection(const char *addr);
	void set_tag(uint16_t tag);

	void add_port(struct portal_group_port *port);
	const struct port *find_port(const char *target) const;
	void remove_port(struct portal_group_port *port);
	void verify(struct conf *conf);

	bool reuse_socket(struct portal &newp);
	int open_sockets(struct conf &oldconf);
	void close_sockets();

private:
	struct conf			*pg_conf;
	freebsd::nvlist_up		pg_options;
	std::string			pg_name;
	auth_group_sp			pg_discovery_auth_group;
	int				pg_discovery_filter = PG_FILTER_UNKNOWN;
	bool				pg_foreign = false;
	bool				pg_assigned = false;
	std::list<portal_up>	        pg_portals;
	std::unordered_map<std::string, port *> pg_ports;
	std::string			pg_offload;
	std::string			pg_redirection;
	int				pg_dscp = -1;
	int				pg_pcp = -1;

	uint16_t			pg_tag = 0;
};

typedef std::unique_ptr<portal_group> portal_group_up;

struct port {
	port(struct target *target);
	virtual ~port() = default;

	struct target *target() const { return p_target; }
	virtual struct auth_group *auth_group() const { return nullptr; }
	virtual struct portal_group *portal_group() const { return nullptr; }

	virtual bool is_dummy() const { return true; }

	virtual void clear_references();

	bool kernel_add();
	bool kernel_update(const port *oport);
	bool kernel_remove();

	virtual bool kernel_create_port() = 0;
	virtual bool kernel_remove_port() = 0;

protected:
	struct target			*p_target;

	uint32_t			p_ctl_port = 0;
};

struct portal_group_port final : public port {
	portal_group_port(struct target *target, struct portal_group *pg,
	    auth_group_sp ag, uint32_t ctl_port);
	~portal_group_port() override = default;

	struct auth_group *auth_group() const override
	{ return p_auth_group.get(); }
	struct portal_group *portal_group() const override
	{ return p_portal_group; }

	bool is_dummy() const override;

	void clear_references() override;

	bool kernel_create_port() override;
	bool kernel_remove_port() override;

private:
	auth_group_sp			p_auth_group;
	struct portal_group		*p_portal_group;
};

struct ioctl_port final : public port {
	ioctl_port(struct target *target, int pp, int vp)
	    : port(target), p_ioctl_pp(pp), p_ioctl_vp(vp) {}
	~ioctl_port() override = default;

	bool kernel_create_port() override;
	bool kernel_remove_port() override;

private:
	int				p_ioctl_pp;
	int				p_ioctl_vp;
};

struct kernel_port final : public port {
	kernel_port(struct target *target, struct pport *pp)
	    : port(target), p_pport(pp) {}
	~kernel_port() override = default;

	bool kernel_create_port() override;
	bool kernel_remove_port() override;

private:
	struct pport			*p_pport;
};

struct lun {
	lun(struct conf *conf, const char *name);

	const char *name() const { return l_name.c_str(); }
	const std::string &path() const { return l_path; }
	int ctl_lun() const { return l_ctl_lun; }

	freebsd::nvlist_up options() const;

	bool add_option(const char *name, const char *value);
	bool set_backend(const char *value);
	bool set_blocksize(size_t value);
	bool set_ctl_lun(uint32_t value);
	bool set_device_type(uint8_t device_type);
	bool set_device_type(const char *value);
	bool set_device_id(const char *value);
	bool set_path(const char *value);
	void set_scsiname(const char *value);
	bool set_serial(const char *value);
	bool set_size(uint64_t value);

	bool changed(const struct lun &old) const;
	bool verify();

	bool kernel_add();
	bool kernel_modify() const;
	bool kernel_remove() const;

private:
	struct conf			*l_conf;
	freebsd::nvlist_up		l_options;
	std::string			l_name;
	std::string			l_backend;
	uint8_t				l_device_type = 0;
	int				l_blocksize = 0;
	std::string			l_device_id;
	std::string			l_path;
	std::string			l_scsiname;
	std::string			l_serial;
	uint64_t			l_size = 0;

	int				l_ctl_lun = -1;
};

struct target {
	target(struct conf *conf, std::string &name)
	    : t_conf(conf), t_name(name) {}

	bool has_alias() const { return !t_alias.empty(); }
	bool has_pport() const { return !t_pport.empty(); }
	bool has_redirection() const { return !t_redirection.empty(); }
	const char *alias() const { return t_alias.c_str(); }
	const char *name() const { return t_name.c_str(); }
	const char *pport() const { return t_pport.c_str(); }
	bool private_auth() const { return t_private_auth; }
	const char *redirection() const { return t_redirection.c_str(); }

	struct auth_group *auth_group() const { return t_auth_group.get(); }
	const std::list<port *> &ports() const { return t_ports; }
	const struct lun *lun(int idx) const { return t_luns[idx]; }

	bool add_chap(const char *user, const char *secret);
	bool add_chap_mutual(const char *user, const char *secret,
	    const char *user2, const char *secret2);
	bool add_initiator_name(const char *name);
	bool add_initiator_portal(const char *addr);
	bool add_lun(u_int id, const char *lun_name);
	bool add_portal_group(const char *pg_name, const char *ag_name);
	bool set_alias(const char *alias);
	bool set_auth_group(const char *ag_name);
	bool set_auth_type(const char *type);
	bool set_physical_port(const char *pport);
	bool set_redirection(const char *addr);
	struct lun *start_lun(u_int id);

	void add_port(struct port *port);
	void remove_lun(struct lun *lun);
	void remove_port(struct port *port);
	void verify();

private:
	bool use_private_auth(const char *keyword);

	struct conf			*t_conf;
	std::array<struct lun *, MAX_LUNS> t_luns;
	auth_group_sp			t_auth_group;
	std::list<port *>		t_ports;
	std::string			t_name;
	std::string			t_alias;
	std::string			t_redirection;
	/* Name of this target's physical port, if any, i.e. "isp0" */
	std::string			t_pport;
	bool				t_private_auth;
};

struct isns {
	isns(const char *addr, struct addrinfo *ai) : i_addr(addr), i_ai(ai) {}

	const char *addr() const { return i_addr.c_str(); }

	freebsd::fd_up connect();
	bool send_request(int s, struct isns_req req);

private:
	std::string			i_addr;
	freebsd::addrinfo_up		i_ai;
};

struct conf {
	bool reuse_portal_group_socket(struct portal &newp);

	char				*conf_pidfile_path = nullptr;
	std::unordered_map<std::string, std::unique_ptr<lun>> conf_luns;
	std::unordered_map<std::string, std::unique_ptr<target>> conf_targets;
	std::unordered_map<std::string, auth_group_sp> conf_auth_groups;
	std::unordered_map<std::string, std::unique_ptr<port>> conf_ports;
	std::unordered_map<std::string, portal_group_up> conf_portal_groups;
	std::unordered_map<std::string, isns> conf_isns;
	struct target			*conf_first_target = nullptr;
	int				conf_isns_period;
	int				conf_isns_timeout;
	int				conf_debug;
	int				conf_timeout;
	int				conf_maxproc;

	struct pidfh			*conf_pidfh = nullptr;

	bool				conf_default_pg_defined = false;
	bool				conf_default_ag_defined = false;
	bool				conf_kernel_port_on = false;

#ifdef ICL_KERNEL_PROXY
	int add_proxy_portal(portal *);
	portal *proxy_portal(int);
private:
	std::vector<portal *>		conf_proxy_portals;
#endif
};

/* Physical ports exposed by the kernel */
struct pport {
	pport(const char *name, uint32_t ctl_port) : pp_name(name),
						     pp_ctl_port(ctl_port) {}

	const char *name() const { return pp_name.c_str(); }
	uint32_t ctl_port() const { return pp_ctl_port; }

	bool linked() const { return pp_linked; }
	void link() { pp_linked = true; }

private:
	std::string			pp_name;
	uint32_t			pp_ctl_port;
	bool				pp_linked;
};

struct kports {
	bool add_port(const char *name, uint32_t ctl_port);
	bool has_port(const char *name);
	struct pport *find_port(const char *name);

private:
	std::unordered_map<std::string, struct pport> pports;
};

#define	CONN_SESSION_TYPE_NONE		0
#define	CONN_SESSION_TYPE_DISCOVERY	1
#define	CONN_SESSION_TYPE_NORMAL	2

struct ctld_connection {
	struct connection	conn;
	struct portal		*conn_portal;
	const struct port	*conn_port;
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

bool			parse_conf(const char *path);
bool			uclparse_conf(const char *path);

struct conf		*conf_new(void);
struct conf		*conf_new_from_kernel(struct kports &kports);
void			conf_delete(struct conf *conf);
void			conf_finish(void);
void			conf_start(struct conf *new_conf);
bool			conf_verify(struct conf *conf);

struct auth_group	*auth_group_new(struct conf *conf, const char *name);
auth_group_sp		auth_group_find(const struct conf *conf,
			    const char *name);

struct portal_group	*portal_group_new(struct conf *conf, const char *name);
struct portal_group	*portal_group_find(struct conf *conf, const char *name);

bool			isns_new(struct conf *conf, const char *addr);
void			isns_check(struct conf *conf, struct isns *isns);
void			isns_deregister_targets(struct conf *conf,
			    struct isns *isns);
void			isns_register_targets(struct conf *conf,
			    struct isns *isns, struct conf *oldconf);

bool			port_new(struct conf *conf, struct target *target,
			    struct portal_group *pg, auth_group_sp ag);
bool			port_new(struct conf *conf, struct target *target,
			    struct portal_group *pg, uint32_t ctl_port);

struct target		*target_new(struct conf *conf, const char *name);
struct target		*target_find(struct conf *conf,
			    const char *name);

struct lun		*lun_new(struct conf *conf, const char *name);
struct lun		*lun_find(const struct conf *conf, const char *name);

bool			option_new(nvlist_t *nvl,
			    const char *name, const char *value);

void			kernel_init(void);
void			kernel_handoff(struct ctld_connection *conn);
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
