/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Ng Peng Nam Sean
 * Copyright (c) 2022 Alexander V. Chernikov <melifaro@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _NETLINK_NETLINK_VAR_H_
#define _NETLINK_NETLINK_VAR_H_

#include <sys/ck.h>
#include <sys/epoch.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <net/vnet.h>

#define	NLSNDQ  65536 /* Default socket sendspace */
#define	NLRCVQ	65536 /* Default socket recvspace */

struct ucred;

struct nl_io_queue {
	STAILQ_HEAD(, mbuf)	head;
	int			length;
	int			hiwat;
};

struct nlpcb {
        struct socket           *nl_socket;
	uint64_t	        nl_groups;
	uint32_t                nl_port;
	uint32_t	        nl_flags;
	uint32_t	        nl_process_id;
        int                     nl_proto;
        bool			nl_active;
	bool			nl_bound;
        bool			nl_task_pending;
	bool			nl_tx_blocked; /* No new requests accepted */
	bool			nl_linux; /* true if running under compat */
	struct nl_io_queue	rx_queue;
	struct nl_io_queue	tx_queue;
	struct taskqueue	*nl_taskqueue;
	struct task		nl_task;
	struct ucred		*nl_cred; /* Copy of nl_socket->so_cred */
	uint64_t		nl_dropped_bytes;
	uint64_t		nl_dropped_messages;
        CK_LIST_ENTRY(nlpcb)    nl_next;
        CK_LIST_ENTRY(nlpcb)    nl_port_next;
	volatile u_int		nl_refcount;
	struct mtx		nl_lock;
	struct epoch_context	nl_epoch_ctx;
};
#define sotonlpcb(so)       ((struct nlpcb *)(so)->so_pcb)

#define	NLP_LOCK_INIT(_nlp)	mtx_init(&((_nlp)->nl_lock), "nlp mtx", NULL, MTX_DEF)
#define	NLP_LOCK_DESTROY(_nlp)	mtx_destroy(&((_nlp)->nl_lock))
#define	NLP_LOCK(_nlp)		mtx_lock(&((_nlp)->nl_lock))
#define	NLP_UNLOCK(_nlp)	mtx_unlock(&((_nlp)->nl_lock))

#define	ALIGNED_NL_SZ(_data)	roundup2((((struct nlmsghdr *)(_data))->nlmsg_len), 16)

/* nl_flags */
#define NLF_CAP_ACK             0x01 /* Do not send message body with errmsg */
#define NLF_EXT_ACK             0x02 /* Allow including extended TLVs in ack */
#define	NLF_STRICT		0x04 /* Perform strict header checks */

SYSCTL_DECL(_net_netlink);

struct nl_io {
	struct callout				callout;
	struct mbuf				*head;
	struct mbuf 				*last;
	int64_t					length;
};

struct nl_control {
	CK_LIST_HEAD(nl_pid_head, nlpcb)	ctl_port_head;
	CK_LIST_HEAD(nlpcb_head, nlpcb)		ctl_pcb_head;
	CK_LIST_ENTRY(nl_control)		ctl_next;
	struct nl_io				ctl_io;
	struct rmlock				ctl_lock;
};
VNET_DECLARE(struct nl_control *, nl_ctl);
#define	V_nl_ctl	VNET(nl_ctl)


struct sockaddr_nl;
struct sockaddr;
struct nlmsghdr;

/* netlink_module.c */
struct nl_control *vnet_nl_ctl_init(void);

int nl_verify_proto(int proto);
const char *nl_get_proto_name(int proto);

extern int netlink_unloading;

struct nl_proto_handler {
	nl_handler_f	cb;
	const char	*proto_name;
};
extern struct nl_proto_handler *nl_handlers;

/* netlink_domain.c */
void nl_send_group(struct mbuf *m, int cnt, int proto, int group_id);

/* netlink_io.c */
#define	NL_IOF_UNTRANSLATED	0x01
#define	NL_IOF_IGNORE_LIMIT	0x02
bool nl_send_one(struct mbuf *m, struct nlpcb *nlp, int cnt, int io_flags);
void nlmsg_ack(struct nlpcb *nlp, int error, struct nlmsghdr *nlmsg,
    struct nl_pstate *npt);
void nl_on_transmit(struct nlpcb *nlp);
void nl_init_io(struct nlpcb *nlp);
void nl_free_io(struct nlpcb *nlp);

void nl_taskqueue_handler(void *_arg, int pending);
int nl_receive_async(struct mbuf *m, struct socket *so);
void nl_process_receive_locked(struct nlpcb *nlp);

#endif
