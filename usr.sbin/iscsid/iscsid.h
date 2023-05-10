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
 * $FreeBSD$
 */

#ifndef ISCSID_H
#define	ISCSID_H

#include <stdbool.h>
#include <stdint.h>

#include <iscsi_ioctl.h>
#include <libiscsiutil.h>

#define	DEFAULT_PIDFILE			"/var/run/iscsid.pid"

#define	CONN_MUTUAL_CHALLENGE_LEN	1024
#define	SOCKBUF_SIZE			1048576

struct iscsid_connection {
	struct connection	conn;
	int			conn_iscsi_fd;
	unsigned int		conn_session_id;
	struct iscsi_session_conf	conn_conf;
	struct iscsi_session_limits	conn_limits;
	char			conn_target_alias[ISCSI_ADDR_LEN];
	int			conn_protocol_level;
	bool			conn_initial_r2t;
	struct chap		*conn_mutual_chap;
};

void			login(struct iscsid_connection *ic);

void			discovery(struct iscsid_connection *ic);

void			fail(const struct connection *, const char *);

#endif /* !ISCSID_H */
