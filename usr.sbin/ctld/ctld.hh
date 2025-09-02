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

#include <array>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <libutil++.hh>

#define	DEFAULT_CONFIG_PATH		"/etc/ctl.conf"
#define	DEFAULT_PIDFILE			"/var/run/ctld.pid"
#define	DEFAULT_BLOCKSIZE		512
#define	DEFAULT_CD_BLOCKSIZE		2048

#define	MAX_LUNS			1024

struct isns_req;
struct port;

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

	bool add_host_nqn(std::string_view nqn);
	bool host_permitted(std::string_view nqn) const;

	bool add_host_address(const char *address);
	bool host_permitted(const struct sockaddr *sa) const;

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
	std::unordered_set<std::string> ag_host_names;
	std::list<auth_portal>		ag_host_addresses;
	std::unordered_set<std::string> ag_initiator_names;
	std::list<auth_portal>		ag_initiator_portals;
};

using auth_group_sp = std::shared_ptr<auth_group>;

enum class portal_protocol {
	ISCSI,
	ISER,
	NVME_TCP,
	NVME_DISCOVERY_TCP,
};

struct portal {
	portal(struct portal_group *pg, std::string_view listen,
	    portal_protocol protocol, freebsd::addrinfo_up ai) :
		p_portal_group(pg), p_listen(listen), p_ai(std::move(ai)),
		p_protocol(protocol) {}
	virtual ~portal() = default;

	bool reuse_socket(portal &oldp);
	bool init_socket();
	virtual bool init_socket_options(int s __unused) { return true; }
	virtual void handle_connection(freebsd::fd_up fd, const char *host,
	    const struct sockaddr *client_sa) = 0;

	struct portal_group *portal_group() const { return p_portal_group; }
	const char *listen() const { return p_listen.c_str(); }
	const addrinfo *ai() const { return p_ai.get(); }
	portal_protocol protocol() const { return p_protocol; }
	int socket() const { return p_socket; }
	void close() { p_socket.reset(); }

private:
	struct portal_group		*p_portal_group;
	std::string			p_listen;
	freebsd::addrinfo_up		p_ai;
	portal_protocol			p_protocol;

	freebsd::fd_up			p_socket;
};

using portal_up = std::unique_ptr<portal>;
using port_up = std::unique_ptr<port>;

enum class discovery_filter {
	UNKNOWN,
	NONE,
	PORTAL,
	PORTAL_NAME,
	PORTAL_NAME_AUTH
};

struct portal_group {
	portal_group(struct conf *conf, std::string_view name);
	virtual ~portal_group() = default;

	struct conf *conf() const { return pg_conf; }
	virtual const char *keyword() const = 0;
	const char *name() const { return pg_name.c_str(); }
	bool assigned() const { return pg_assigned; }
	bool is_dummy() const;
	bool is_redirecting() const { return !pg_redirection.empty(); }
	struct auth_group *discovery_auth_group() const
	{ return pg_discovery_auth_group.get(); }
	enum discovery_filter discovery_filter() const
	{ return pg_discovery_filter; }
	int dscp() const { return pg_dscp; }
	const char *offload() const { return pg_offload.c_str(); }
	const char *redirection() const { return pg_redirection.c_str(); }
	int pcp() const { return pg_pcp; }
	uint16_t tag() const { return pg_tag; }

	freebsd::nvlist_up options() const;

	const std::list<portal_up> &portals() const { return pg_portals; }
	const std::unordered_map<std::string, port *> &ports() const
	{ return pg_ports; }

	virtual void allocate_tag() = 0;
	virtual bool add_portal(const char *value,
	    portal_protocol protocol) = 0;
	virtual void add_default_portals() = 0;
	bool add_option(const char *name, const char *value);
	bool set_discovery_auth_group(const char *name);
	bool set_dscp(u_int dscp);
	virtual bool set_filter(const char *str) = 0;
	void set_foreign();
	bool set_offload(const char *offload);
	bool set_pcp(u_int pcp);
	bool set_redirection(const char *addr);
	void set_tag(uint16_t tag);

	virtual port_up create_port(struct target *target, auth_group_sp ag) =
	    0;
	virtual port_up create_port(struct target *target, uint32_t ctl_port) =
	    0;

	void add_port(struct portal_group_port *port);
	const struct port *find_port(std::string_view target) const;
	void remove_port(struct portal_group_port *port);
	void verify(struct conf *conf);

	bool reuse_socket(struct portal &newp);
	int open_sockets(struct conf &oldconf);
	void close_sockets();

protected:
	struct conf			*pg_conf;
	freebsd::nvlist_up		pg_options;
	const char			*pg_keyword;
	std::string			pg_name;
	auth_group_sp			pg_discovery_auth_group;
	enum discovery_filter		pg_discovery_filter =
	    discovery_filter::UNKNOWN;
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

using portal_group_up = std::unique_ptr<portal_group>;

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

struct portal_group_port : public port {
	portal_group_port(struct target *target, struct portal_group *pg,
	    auth_group_sp ag);
	portal_group_port(struct target *target, struct portal_group *pg,
	    uint32_t ctl_port);
	~portal_group_port() override = default;

	struct auth_group *auth_group() const override
	{ return p_auth_group.get(); }
	struct portal_group *portal_group() const override
	{ return p_portal_group; }

	bool is_dummy() const override;

	void clear_references() override;

protected:
	auth_group_sp			p_auth_group;
	struct portal_group		*p_portal_group;
};

struct ioctl_port final : public port {
	ioctl_port(struct target *target, int pp, int vp) :
		port(target), p_ioctl_pp(pp), p_ioctl_vp(vp) {}
	~ioctl_port() override = default;

	bool kernel_create_port() override;
	bool kernel_remove_port() override;

private:
	int				p_ioctl_pp;
	int				p_ioctl_vp;
};

struct kernel_port final : public port {
	kernel_port(struct target *target, struct pport *pp) :
		port(target), p_pport(pp) {}
	~kernel_port() override = default;

	bool kernel_create_port() override;
	bool kernel_remove_port() override;

private:
	struct pport			*p_pport;
};

struct lun {
	lun(struct conf *conf, std::string_view name);

	const char *name() const { return l_name.c_str(); }
	const std::string &path() const { return l_path; }
	int ctl_lun() const { return l_ctl_lun; }

	freebsd::nvlist_up options() const;

	bool add_option(const char *name, const char *value);
	bool set_backend(std::string_view value);
	bool set_blocksize(size_t value);
	bool set_ctl_lun(uint32_t value);
	bool set_device_type(uint8_t device_type);
	bool set_device_type(const char *value);
	bool set_device_id(std::string_view value);
	bool set_path(std::string_view value);
	void set_scsiname(std::string_view value);
	bool set_serial(std::string_view value);
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
	target(struct conf *conf, const char *keyword, std::string_view name);
	virtual ~target() = default;

	bool has_alias() const { return !t_alias.empty(); }
	bool has_pport() const { return !t_pport.empty(); }
	bool has_redirection() const { return !t_redirection.empty(); }
	const char *alias() const { return t_alias.c_str(); }
	const char *name() const { return t_name.c_str(); }
	const char *label() const { return t_label.c_str(); }
	const char *pport() const { return t_pport.c_str(); }
	bool private_auth() const { return t_private_auth; }
	const char *redirection() const { return t_redirection.c_str(); }

	struct auth_group *auth_group() const { return t_auth_group.get(); }
	const std::list<port *> &ports() const { return t_ports; }
	const struct lun *lun(int idx) const { return t_luns[idx]; }

	bool add_chap(const char *user, const char *secret);
	bool add_chap_mutual(const char *user, const char *secret,
	    const char *user2, const char *secret2);
	virtual bool add_host_address(const char *) { return false; }
	virtual bool add_host_nqn(std::string_view) { return false; }
	virtual bool add_initiator_name(std::string_view) { return false; }
	virtual bool add_initiator_portal(const char *) { return false; }
	virtual bool add_lun(u_int, const char *) { return false; }
	virtual bool add_namespace(u_int, const char *) { return false; }
	virtual bool add_portal_group(const char *pg_name,
	    const char *ag_name) = 0;
	bool set_alias(std::string_view alias);
	bool set_auth_group(const char *ag_name);
	bool set_auth_type(const char *type);
	bool set_physical_port(std::string_view pport);
	bool set_redirection(const char *addr);
	virtual struct lun *start_lun(u_int) { return nullptr; }
	virtual struct lun *start_namespace(u_int) { return nullptr; }

	void add_port(struct port *port);
	void remove_lun(struct lun *lun);
	void remove_port(struct port *port);
	void verify();

protected:
	bool use_private_auth(const char *keyword);
	bool add_lun(u_int id, const char *lun_label, const char *lun_name);
	struct lun *start_lun(u_int id, const char *lun_label,
	    const char *lun_name);
	virtual struct portal_group *default_portal_group() = 0;

	struct conf			*t_conf;
	std::array<struct lun *, MAX_LUNS> t_luns = {};
	auth_group_sp			t_auth_group;
	std::list<port *>		t_ports;
	std::string			t_name;
	std::string			t_label;
	std::string			t_alias;
	std::string			t_redirection;
	/* Name of this target's physical port, if any, i.e. "isp0" */
	std::string			t_pport;
	bool				t_private_auth = false;
};

using target_up = std::unique_ptr<target>;

struct isns {
	isns(std::string_view addr, freebsd::addrinfo_up ai) :
		i_addr(addr), i_ai(std::move(ai)) {}

	const char *addr() const { return i_addr.c_str(); }

	freebsd::fd_up connect();
	bool send_request(int s, struct isns_req req);

private:
	std::string			i_addr;
	freebsd::addrinfo_up		i_ai;
};

struct conf {
	conf();

	int maxproc() const { return conf_maxproc; }
	int timeout() const { return conf_timeout; }
	uint32_t genctr() const { return conf_genctr; }

	bool default_auth_group_defined() const
	{ return conf_default_ag_defined; }
	bool default_portal_group_defined() const
	{ return conf_default_pg_defined; }
	bool default_transport_group_defined() const
	{ return conf_default_tg_defined; }

	struct auth_group *add_auth_group(const char *ag_name);
	struct auth_group *define_default_auth_group();
	auth_group_sp find_auth_group(std::string_view ag_name);

	struct portal_group *add_portal_group(const char *name);
	struct portal_group *define_default_portal_group();
	struct portal_group *find_portal_group(std::string_view name);

	struct portal_group *add_transport_group(const char *name);
	struct portal_group *define_default_transport_group();
	struct portal_group *find_transport_group(std::string_view name);

	bool add_port(struct target *target, struct portal_group *pg,
	    auth_group_sp ag);
	bool add_port(struct target *target, struct portal_group *pg,
	    uint32_t ctl_port);
	bool add_port(struct target *target, struct pport *pp);
	bool add_port(struct kports &kports, struct target *target, int pp,
	    int vp);
	bool add_pports(struct kports &kports);

	struct target *add_controller(const char *name);
	struct target *find_controller(std::string_view name);

	struct target *add_target(const char *name);
	struct target *find_target(std::string_view name);

	struct lun *add_lun(const char *name);
	struct lun *find_lun(std::string_view name);

	void set_debug(int debug);
	void set_isns_period(int period);
	void set_isns_timeout(int timeout);
	void set_maxproc(int maxproc);
	bool set_pidfile_path(std::string_view path);
	void set_timeout(int timeout);

	void open_pidfile();
	void write_pidfile();
	void close_pidfile();

	bool add_isns(const char *addr);
	void isns_register_targets(struct isns *isns, struct conf *oldconf);
	void isns_deregister_targets(struct isns *isns);
	void isns_schedule_update();
	void isns_update();

	int apply(struct conf *oldconf);
	void delete_target_luns(struct lun *lun);
	bool reuse_portal_group_socket(struct portal &newp);
	bool verify();

private:
	struct isns_req isns_register_request(const char *hostname);
	struct isns_req isns_check_request(const char *hostname);
	struct isns_req isns_deregister_request(const char *hostname);
	void isns_check(struct isns *isns);

	std::string			conf_pidfile_path;
	std::unordered_map<std::string, std::unique_ptr<lun>> conf_luns;
	std::unordered_map<std::string, target_up> conf_targets;
	std::unordered_map<std::string, target_up> conf_controllers;
	std::unordered_map<std::string, auth_group_sp> conf_auth_groups;
	std::unordered_map<std::string, std::unique_ptr<port>> conf_ports;
	std::unordered_map<std::string, portal_group_up> conf_portal_groups;
	std::unordered_map<std::string, portal_group_up> conf_transport_groups;
	std::unordered_map<std::string, isns> conf_isns;
	struct target			*conf_first_target = nullptr;
	int				conf_isns_period = 900;
	int				conf_isns_timeout = 5;
	int				conf_debug = 0;
	int				conf_timeout = 60;
	int				conf_maxproc = 30;
	uint32_t			conf_genctr = 0;

	freebsd::pidfile		conf_pidfile;

	bool				conf_default_pg_defined = false;
	bool				conf_default_tg_defined = false;
	bool				conf_default_ag_defined = false;

	static uint32_t			global_genctr;

#ifdef ICL_KERNEL_PROXY
public:
	int add_proxy_portal(portal *);
	portal *proxy_portal(int);
private:
	std::vector<portal *>		conf_proxy_portals;
#endif
};

using conf_up = std::unique_ptr<conf>;

/* Physical ports exposed by the kernel */
struct pport {
	pport(std::string_view name, uint32_t ctl_port) : pp_name(name),
	    pp_ctl_port(ctl_port) {}

	const char *name() const { return pp_name.c_str(); }
	uint32_t ctl_port() const { return pp_ctl_port; }

	bool linked() const { return pp_linked; }
	void link() { pp_linked = true; }

private:
	std::string			pp_name;
	uint32_t			pp_ctl_port;
	bool				pp_linked = false;
};

struct kports {
	bool add_port(std::string &name, uint32_t ctl_port);
	bool has_port(std::string_view name);
	struct pport *find_port(std::string_view name);

private:
	std::unordered_map<std::string, struct pport> pports;
};

extern bool proxy_mode;
extern int ctl_fd;

bool			parse_conf(const char *path);
bool			uclparse_conf(const char *path);

conf_up			conf_new_from_kernel(struct kports &kports);
void			conf_finish(void);
void			conf_start(struct conf *new_conf);

bool			option_new(nvlist_t *nvl,
			    const char *name, const char *value);

freebsd::addrinfo_up	parse_addr_port(const char *address,
			    const char *def_port);

void			kernel_init(void);
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

bool			ctl_create_port(const char *driver,
			    const nvlist_t *nvl, uint32_t *ctl_port);
bool			ctl_remove_port(const char *driver, nvlist_t *nvl);

portal_group_up		iscsi_make_portal_group(struct conf *conf,
			    std::string_view name);
target_up		iscsi_make_target(struct conf *conf,
			    std::string_view name);

portal_group_up		nvmf_make_transport_group(struct conf *conf,
			    std::string_view name);
target_up		nvmf_make_controller(struct conf *conf,
			    std::string_view name);

void			start_timer(int timeout, bool fatal = false);
void			stop_timer();
bool			timed_out();

#endif /* !__CTLD_HH__ */
