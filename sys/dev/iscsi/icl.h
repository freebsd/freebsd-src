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

#ifndef ICL_H
#define	ICL_H

/*
 * iSCSI Common Layer.  It's used by both the initiator and target to send
 * and receive iSCSI PDUs.
 */

struct icl_conn;

struct icl_pdu {
	TAILQ_ENTRY(icl_pdu)	ip_next;
	struct icl_conn		*ip_conn;
	struct iscsi_bhs	*ip_bhs;
	struct mbuf		*ip_bhs_mbuf;
	size_t			ip_ahs_len;
	struct mbuf		*ip_ahs_mbuf;
	size_t			ip_data_len;
	struct mbuf		*ip_data_mbuf;

	/*
	 * User (initiator or provider) private fields.
	 */
	uint32_t		ip_prv0;
	uint32_t		ip_prv1;
	uint32_t		ip_prv2;
};

struct icl_pdu		*icl_pdu_new_bhs(struct icl_conn *ic, int flags);
size_t			icl_pdu_data_segment_length(const struct icl_pdu *ip);
int			icl_pdu_append_data(struct icl_pdu *ip, const void *addr, size_t len, int flags);
void			icl_pdu_get_data(struct icl_pdu *ip, size_t off, void *addr, size_t len);
void			icl_pdu_queue(struct icl_pdu *ip);
void			icl_pdu_free(struct icl_pdu *ip);

#define ICL_CONN_STATE_INVALID		0
#define ICL_CONN_STATE_BHS		1
#define ICL_CONN_STATE_AHS		2
#define ICL_CONN_STATE_HEADER_DIGEST	3
#define ICL_CONN_STATE_DATA		4
#define ICL_CONN_STATE_DATA_DIGEST	5

#define	ICL_MAX_DATA_SEGMENT_LENGTH	(128 * 1024)

struct icl_conn {
	struct mtx		ic_lock;
	struct socket		*ic_socket;
	volatile u_int		ic_outstanding_pdus;
	TAILQ_HEAD(, icl_pdu)	ic_to_send;
	size_t			ic_receive_len;
	int			ic_receive_state;
	struct icl_pdu		*ic_receive_pdu;
	struct cv		ic_send_cv;
	struct cv		ic_receive_cv;
	bool			ic_header_crc32c;
	bool			ic_data_crc32c;
	bool			ic_send_running;
	bool			ic_receive_running;
	size_t			ic_max_data_segment_length;
	bool			ic_disconnecting;
	bool			ic_iser;

	void			(*ic_receive)(struct icl_pdu *);
	void			(*ic_error)(struct icl_conn *);

	/*
	 * User (initiator or provider) private fields.
	 */
	void			*ic_prv0;
};

struct icl_conn		*icl_conn_new(void);
void			icl_conn_free(struct icl_conn *ic);
int			icl_conn_handoff(struct icl_conn *ic, int fd);
void			icl_conn_shutdown(struct icl_conn *ic);
void			icl_conn_close(struct icl_conn *ic);
bool			icl_conn_connected(struct icl_conn *ic);

#ifdef ICL_KERNEL_PROXY

struct sockaddr;
struct icl_listen;

struct icl_listen_sock {
	TAILQ_ENTRY(icl_listen_sock)	ils_next;
	struct icl_listen	*ils_listen;
	struct socket		*ils_socket;
	bool			ils_running;
	bool			ils_disconnecting;
};

struct icl_listen	{
	TAILQ_HEAD(, icl_listen_sock)	il_sockets;
	struct sx			il_lock;
	void				(*il_accept)(struct socket *);
};

/*
 * Initiator part.
 */
int			icl_conn_connect(struct icl_conn *ic, bool rdma,
			    int domain, int socktype, int protocol,
			    struct sockaddr *from_sa, struct sockaddr *to_sa);
/*
 * Target part.
 */
struct icl_listen	*icl_listen_new(void (*accept_cb)(struct socket *));
void			icl_listen_free(struct icl_listen *il);
int			icl_listen_add(struct icl_listen *il, bool rdma, int domain,
    int socktype, int protocol, struct sockaddr *sa);
int			icl_listen_remove(struct icl_listen *il, struct sockaddr *sa);

/*
 * This one is not a public API; only to be used by icl_proxy.c.
 */
int			icl_conn_handoff_sock(struct icl_conn *ic, struct socket *so);

#endif /* ICL_KERNEL_PROXY */

#endif /* !ICL_H */
