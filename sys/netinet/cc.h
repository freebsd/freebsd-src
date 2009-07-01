/*-
 * Copyright (c) 2008-2009
 * 	Swinburne University of Technology, Melbourne, Australia
 * Copyright (c) 2009 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by Lawrence Stewart and James Healy,
 * made possible in part by a grant from the Cisco University Research Program
 * Fund at Community Foundation Silicon Valley.
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
 *
 * $FreeBSD$
 */

#ifndef _NETINET_CC_H_
#define _NETINET_CC_H_

/* Needed for TCP_CA_NAME_MAX define which lives in tcp.h for compat reasons. */
#include <netinet/tcp.h>

/*
 * Global CC vars.
 */
extern	STAILQ_HEAD(cc_head, cc_algo) cc_list;
extern	const int tcprexmtthresh;
extern	struct cc_algo newreno_cc_algo;

/*
 * Define the new net.inet.tcp.cc sysctl tree.
 */
SYSCTL_DECL(_net_inet_tcp_cc);

/*
 * CC housekeeping functions.
 */
void	cc_init(void);
int	cc_register_algo(struct cc_algo *add_cc);
int	cc_deregister_algo(struct cc_algo *remove_cc);

/*
 * Structure to hold data and function pointers that together represent
 * a congestion control algorithm.
 * Based on similar structure in the SCTP stack.
 */
struct cc_algo {
	char name[TCP_CA_NAME_MAX];

	/* Init global module state on kldload. */
	int (*mod_init) (void);

	/* Cleanup global module state on kldunload. */
	int (*mod_destroy) (void);

	/* Init CC state for a new control block. */
	int (*cb_init) (struct tcpcb *tp);

	/* Cleanup CC state for a terminating control block. */
	void (*cb_destroy) (struct tcpcb *tp);

	/* Init variables for a newly established connection. */
	void (*conn_init) (struct tcpcb *tp);

	/* Called on receipt of a regular, valid ack. */
	void (*ack_received) (struct tcpcb *tp, struct tcphdr *th);

	/* Called before entering FR. */
	void (*pre_fr) (struct tcpcb *tp, struct tcphdr *th);

	/* Called after exiting FR. */
	void (*post_fr) (struct tcpcb *tp, struct tcphdr *th);

	/* Called when data transfer resumes after an idle period. */
	void (*after_idle) (struct tcpcb *tp);

	/* Called each time the connection's retransmit timer fires. */
	void (*after_timeout) (struct tcpcb *tp);

	STAILQ_ENTRY(cc_algo) entries;
};

/* Macro to obtain the CC algo's struct ptr. */
#define CC_ALGO(tp)	((tp)->cc_algo)

/* Macro to obtain the CC algo's data ptr. */
#define CC_DATA(tp)	((tp)->cc_data)

/* Macro to obtain the system default CC algo's struct ptr. */
#define CC_DEFAULT()	STAILQ_FIRST(&cc_list)

extern struct rwlock cc_list_lock;
#define CC_LIST_LOCK_INIT() rw_init(&cc_list_lock, "cc_list")
#define CC_LIST_LOCK_DESTROY() rw_destroy(&cc_list_lock)
#define CC_LIST_RLOCK() rw_rlock(&cc_list_lock)
#define CC_LIST_RUNLOCK() rw_runlock(&cc_list_lock)
#define CC_LIST_WLOCK() rw_wlock(&cc_list_lock)
#define CC_LIST_WUNLOCK() rw_wunlock(&cc_list_lock)
#define CC_LIST_WLOCK_ASSERT() rw_assert(&cc_list_lock, RA_WLOCKED)

#endif /* _NETINET_CC_H_ */
