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

#ifndef ISCSID_H
#define	ISCSID_H

#include <stdbool.h>
#include <stdint.h>

#include <iscsi_ioctl.h>

#define	DEFAULT_PIDFILE			"/var/run/iscsid.pid"

#define	CONN_DIGEST_NONE		0
#define	CONN_DIGEST_CRC32C		1

#define CONN_MUTUAL_CHALLENGE_LEN	1024

struct connection {
	int			conn_iscsi_fd;
#ifndef ICL_KERNEL_PROXY
	int			conn_socket;
#endif
	unsigned int		conn_session_id;
	struct iscsi_session_conf	conn_conf;
	char			conn_target_alias[ISCSI_ADDR_LEN];
	uint8_t			conn_isid[6];
	uint32_t		conn_statsn;
	int			conn_header_digest;
	int			conn_data_digest;
	bool			conn_initial_r2t;
	bool			conn_immediate_data;
	size_t			conn_max_data_segment_length;
	size_t			conn_max_burst_length;
	size_t			conn_first_burst_length;
	char			conn_mutual_challenge[CONN_MUTUAL_CHALLENGE_LEN];
	unsigned char		conn_mutual_id;
};

struct pdu {
	struct connection	*pdu_connection;
	struct iscsi_bhs	*pdu_bhs;
	char			*pdu_data;
	size_t			pdu_data_len;
};

#define	KEYS_MAX		1024

struct keys {
	char			*keys_names[KEYS_MAX];
	char			*keys_values[KEYS_MAX];
	char			*keys_data;
	size_t			keys_data_len;
};

struct keys		*keys_new(void);
void			keys_delete(struct keys *key);
void			keys_load(struct keys *keys, const struct pdu *pdu);
void			keys_save(struct keys *keys, struct pdu *pdu);
const char		*keys_find(struct keys *keys, const char *name);
int			keys_find_int(struct keys *keys, const char *name);
void			keys_add(struct keys *keys,
			    const char *name, const char *value);
void			keys_add_int(struct keys *keys,
			    const char *name, int value);

struct pdu		*pdu_new(struct connection *ic);
struct pdu		*pdu_new_response(struct pdu *request);
void			pdu_receive(struct pdu *request);
void			pdu_send(struct pdu *response);
void			pdu_delete(struct pdu *ip);

void			login(struct connection *ic);

void			discovery(struct connection *ic);

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
bool			timed_out(void);
void			fail(const struct connection *, const char *);

#endif /* !ISCSID_H */
