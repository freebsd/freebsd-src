/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _HVSOCK_H
#define _HVSOCK_H
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/queue.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/vmbus.h>

/*
 * HyperV Socket Protocols
 */
#define	HYPERV_SOCK_PROTO_TRANS		1	/* Transport protocol */

#define	HVADDR_PORT_ANY			-1U
#define	HVADDR_PORT_UNKNOWN		-1U

#define HVS_LIST_BOUND			0x01
#define HVS_LIST_CONNECTED		0x02
#define HVS_LIST_ALL			(HVS_LIST_BOUND | HVS_LIST_CONNECTED)

struct sockaddr_hvs {
	unsigned char	sa_len;
	sa_family_t	sa_family;
	unsigned int	hvs_port;
	unsigned char	hvs_zero[sizeof(struct sockaddr) -
				 sizeof(sa_family_t) -
				 sizeof(unsigned char) -
				 sizeof(unsigned int)];
};

struct vmpipe_proto_header {
	uint32_t			vmpipe_pkt_type;
	uint32_t			vmpipe_data_size;
} __packed;

struct hvs_pkt_header {
	struct vmbus_chanpkt_hdr	chan_pkt_hdr;
	struct vmpipe_proto_header	vmpipe_pkt_hdr;
} __packed;

struct hvs_pcb {
	struct socket			*so;		/* Pointer to socket */
	struct sockaddr_hvs		local_addr;
	struct sockaddr_hvs		remote_addr;

	struct hyperv_guid		vm_srv_id;
	struct hyperv_guid		host_srv_id;

	struct vmbus_channel		*chan;
	/* Current packet header on rx ring */
	struct hvs_pkt_header		hvs_pkt;
	/* Available data in receive br in current packet */
	uint32_t			recv_data_len;
	/* offset in the packet */
	uint32_t			recv_data_off;
	bool				rb_init;
	/* Link lists for global bound and connected sockets */
	LIST_ENTRY(hvs_pcb)		bound_next;
	LIST_ENTRY(hvs_pcb)		connected_next;
};

#define so2hvspcb(so) \
	((struct hvs_pcb *)((so)->so_pcb))
#define hsvpcb2so(hvspcb) \
	((struct socket *)((hvspcb)->so))

void	hvs_addr_init(struct sockaddr_hvs *, const struct hyperv_guid *);
void	hvs_trans_close(struct socket *);
void	hvs_trans_detach(struct socket *);
void	hvs_trans_abort(struct socket *);
int	hvs_trans_attach(struct socket *, int, struct thread *);
int	hvs_trans_bind(struct socket *, struct sockaddr *, struct thread *);
int	hvs_trans_listen(struct socket *, int, struct thread *);
int	hvs_trans_accept(struct socket *, struct sockaddr **);
int	hvs_trans_connect(struct socket *,
	    struct sockaddr *, struct thread *);
int	hvs_trans_peeraddr(struct socket *, struct sockaddr **);
int	hvs_trans_sockaddr(struct socket *, struct sockaddr **);
int	hvs_trans_soreceive(struct socket *, struct sockaddr **,
	    struct uio *, struct mbuf **, struct mbuf **, int *);
int	hvs_trans_sosend(struct socket *, struct sockaddr *, struct uio *,
	     struct mbuf *, struct mbuf *, int, struct thread *);
int	hvs_trans_disconnect(struct socket *);
int	hvs_trans_shutdown(struct socket *);

int	hvs_trans_lock(void);
void	hvs_trans_unlock(void);

void	hvs_remove_socket_from_list(struct socket *, unsigned char);
#endif /* _HVSOCK_H */
