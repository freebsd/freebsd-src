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
 * $FreeBSD$
 */

#ifndef CTLD_H
#define	CTLD_H

#include <sys/queue.h>
#include <stdbool.h>
#include <libutil.h>

#define	DEFAULT_CONFIG_PATH		"/etc/ctl.conf"
#define	DEFAULT_PIDFILE			"/var/run/ctld.pid"
#define	DEFAULT_BLOCKSIZE		512

#define	MAX_NAME_LEN			223
#define	MAX_DATA_SEGMENT_LENGTH		(128 * 1024)
#define	MAX_BURST_LENGTH		16776192

struct auth {
	TAILQ_ENTRY(auth)		a_next;
	struct auth_group		*a_auth_group;
	char				*a_user;
	char				*a_secret;
	char				*a_mutual_user;
	char				*a_mutual_secret;
};

#define	AG_TYPE_UNKNOWN			0
#define	AG_TYPE_NO_AUTHENTICATION	1
#define	AG_TYPE_CHAP			2
#define	AG_TYPE_CHAP_MUTUAL		3

struct auth_group {
	TAILQ_ENTRY(auth_group)		ag_next;
	struct conf			*ag_conf;
	char				*ag_name;
	struct target			*ag_target;
	int				ag_type;
	TAILQ_HEAD(, auth)		ag_auths;
};

struct portal {
	TAILQ_ENTRY(portal)		p_next;
	struct portal_group		*p_portal_group;
	bool				p_iser;
	char				*p_listen;
	struct addrinfo			*p_ai;

	TAILQ_HEAD(, target)		p_targets;
	int				p_socket;
};

struct portal_group {
	TAILQ_ENTRY(portal_group)	pg_next;
	struct conf			*pg_conf;
	char				*pg_name;
	struct auth_group		*pg_discovery_auth_group;
	bool				pg_unassigned;
	TAILQ_HEAD(, portal)		pg_portals;

	uint16_t			pg_tag;
};

struct lun_option {
	TAILQ_ENTRY(lun_option)		lo_next;
	struct lun			*lo_lun;
	char				*lo_name;
	char				*lo_value;
};

struct lun {
	TAILQ_ENTRY(lun)		l_next;
	TAILQ_HEAD(, lun_option)	l_options;
	struct target			*l_target;
	int				l_lun;
	char				*l_backend;
	int				l_blocksize;
	char				*l_device_id;
	char				*l_path;
	char				*l_serial;
	int64_t				l_size;

	int				l_ctl_lun;
};

struct target {
	TAILQ_ENTRY(target)		t_next;
	TAILQ_HEAD(, lun)		t_luns;
	struct conf			*t_conf;
	struct auth_group		*t_auth_group;
	struct portal_group		*t_portal_group;
	char				*t_iqn;
	char				*t_alias;
};

struct conf {
	char				*conf_pidfile_path;
	TAILQ_HEAD(, target)		conf_targets;
	TAILQ_HEAD(, auth_group)	conf_auth_groups;
	TAILQ_HEAD(, portal_group)	conf_portal_groups;
	int				conf_debug;
	int				conf_timeout;
	int				conf_maxproc;

	uint16_t			conf_last_portal_group_tag;
	struct pidfh			*conf_pidfh;
};

#define	CONN_SESSION_TYPE_NONE		0
#define	CONN_SESSION_TYPE_DISCOVERY	1
#define	CONN_SESSION_TYPE_NORMAL	2

#define	CONN_DIGEST_NONE		0
#define	CONN_DIGEST_CRC32C		1

struct connection {
	struct portal		*conn_portal;
	struct target		*conn_target;
	int			conn_socket;
	int			conn_session_type;
	char			*conn_initiator_name;
	char			*conn_initiator_addr;
	char			*conn_initiator_alias;
	uint32_t		conn_cmdsn;
	uint32_t		conn_statsn;
	size_t			conn_max_data_segment_length;
	size_t			conn_max_burst_length;
	int			conn_immediate_data;
	int			conn_header_digest;
	int			conn_data_digest;
};

struct pdu {
	struct connection	*pdu_connection;
	struct iscsi_bhs	*pdu_bhs;
	char			*pdu_data;
	size_t			pdu_data_len;
};

#define	KEYS_MAX	1024

struct keys {
	char		*keys_names[KEYS_MAX];
	char		*keys_values[KEYS_MAX];
	char		*keys_data;
	size_t		keys_data_len;
};

struct conf		*conf_new(void);
struct conf		*conf_new_from_file(const char *path);
struct conf		*conf_new_from_kernel(void);
void			conf_delete(struct conf *conf);
int			conf_verify(struct conf *conf);

struct auth_group	*auth_group_new(struct conf *conf, const char *name);
void			auth_group_delete(struct auth_group *ag);
struct auth_group	*auth_group_find(struct conf *conf, const char *name);

const struct auth	*auth_new_chap(struct auth_group *ag,
			    const char *user, const char *secret);
const struct auth	*auth_new_chap_mutual(struct auth_group *ag,
			    const char *user, const char *secret,
			    const char *user2, const char *secret2);
const struct auth	*auth_find(struct auth_group *ag,
			    const char *user);

struct portal_group	*portal_group_new(struct conf *conf, const char *name);
void			portal_group_delete(struct portal_group *pg);
struct portal_group	*portal_group_find(struct conf *conf, const char *name);
int			portal_group_add_listen(struct portal_group *pg,
			    const char *listen, bool iser);

struct target		*target_new(struct conf *conf, const char *iqn);
void			target_delete(struct target *target);
struct target		*target_find(struct conf *conf,
			    const char *iqn);

struct lun		*lun_new(struct target *target, int lun_id);
void			lun_delete(struct lun *lun);
struct lun		*lun_find(struct target *target, int lun_id);
void			lun_set_backend(struct lun *lun, const char *value);
void			lun_set_blocksize(struct lun *lun, size_t value);
void			lun_set_device_id(struct lun *lun, const char *value);
void			lun_set_path(struct lun *lun, const char *value);
void			lun_set_serial(struct lun *lun, const char *value);
void			lun_set_size(struct lun *lun, size_t value);
void			lun_set_ctl_lun(struct lun *lun, uint32_t value);

struct lun_option	*lun_option_new(struct lun *lun,
			    const char *name, const char *value);
void			lun_option_delete(struct lun_option *clo);
struct lun_option	*lun_option_find(struct lun *lun, const char *name);
void			lun_option_set(struct lun_option *clo,
			    const char *value);

void			kernel_init(void);
int			kernel_lun_add(struct lun *lun);
int			kernel_lun_resize(struct lun *lun);
int			kernel_lun_remove(struct lun *lun);
void			kernel_handoff(struct connection *conn);
int			kernel_port_on(void);
int			kernel_port_off(void);
void			kernel_capsicate(void);

/*
 * ICL_KERNEL_PROXY
 */
void			kernel_listen(struct addrinfo *ai, bool iser);
int			kernel_accept(void);
void			kernel_send(struct pdu *pdu);
void			kernel_receive(struct pdu *pdu);

struct keys		*keys_new(void);
void			keys_delete(struct keys *keys);
void			keys_load(struct keys *keys, const struct pdu *pdu);
void			keys_save(struct keys *keys, struct pdu *pdu);
const char		*keys_find(struct keys *keys, const char *name);
int			keys_find_int(struct keys *keys, const char *name);
void			keys_add(struct keys *keys,
			    const char *name, const char *value);
void			keys_add_int(struct keys *keys,
			    const char *name, int value);

struct pdu		*pdu_new(struct connection *conn);
struct pdu		*pdu_new_response(struct pdu *request);
void			pdu_delete(struct pdu *pdu);
void			pdu_receive(struct pdu *request);
void			pdu_send(struct pdu *response);

void			login(struct connection *conn);

void			discovery(struct connection *conn);

void			log_init(int level);
void			log_set_peer_name(const char *name);
void			log_set_peer_addr(const char *addr);
void			log_err(int, const char *, ...)
			    __dead2 __printf0like(2, 3);
void			log_errx(int, const char *, ...)
			    __dead2 __printf0like(2, 3);
void			log_warn(const char *, ...) __printf0like(1, 2);
void			log_warnx(const char *, ...) __printflike(1, 2);
void			log_debugx(const char *, ...) __printf0like(1, 2);

char			*checked_strdup(const char *);
bool			valid_iscsi_name(const char *name);
bool			timed_out(void);

#endif /* !CTLD_H */
