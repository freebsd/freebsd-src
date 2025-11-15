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

#ifndef __ISCSI_HH__
#define	__ISCSI_HH__

#define	CONN_SESSION_TYPE_NONE		0
#define	CONN_SESSION_TYPE_DISCOVERY	1
#define	CONN_SESSION_TYPE_NORMAL	2

struct iscsi_connection {
	iscsi_connection(struct portal *portal, freebsd::fd_up fd,
	    const char *host, const struct sockaddr *client_sa);
	~iscsi_connection();

	void handle();
private:
	void login();
	void login_chap(struct auth_group *ag);
	void login_negotiate_key(struct pdu *request, const char *name,
	    const char *value, bool skipped_security,
	    struct keys *response_keys);
	bool login_portal_redirect(struct pdu *request);
	bool login_target_redirect(struct pdu *request);
	void login_negotiate(struct pdu *request);
	void login_wait_transition();

	void discovery();
	bool discovery_target_filtered_out(const struct port *port) const;

	void kernel_handoff();

	struct connection	conn;
	struct portal		*conn_portal = nullptr;
	const struct port	*conn_port = nullptr;
	struct target		*conn_target = nullptr;
	freebsd::fd_up		conn_fd;
	int			conn_session_type = CONN_SESSION_TYPE_NONE;
	std::string		conn_initiator_name;
	std::string		conn_initiator_addr;
	std::string		conn_initiator_alias;
	uint8_t			conn_initiator_isid[6] = {};
	const struct sockaddr	*conn_initiator_sa = nullptr;
	int			conn_max_recv_data_segment_limit = 0;
	int			conn_max_send_data_segment_limit = 0;
	int			conn_max_burst_limit = 0;
	int			conn_first_burst_limit = 0;
	std::string		conn_user;
	struct chap		*conn_chap = nullptr;
};

#endif /* !__ISCSI_HH__ */
