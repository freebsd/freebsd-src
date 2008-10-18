/*-
 * Copyright (c) 2008 Swinburne University of Technology, Melbourne, Australia
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

#include <sys/queue.h>
#include <netinet/tcp_var.h>

/*
 * Global CC vars
 */
extern	STAILQ_HEAD(cc_head, cc_algo) cc_list;
extern	char cc_algorithm[];
extern	const int tcprexmtthresh;
extern	struct cc_algo newreno_cc_algo;

/*
 * Define the new net.inet.tcp.cc sysctl tree
 */
SYSCTL_DECL(_net_inet_tcp_cc);

/*
 * CC housekeeping functions
 */
void	cc_init(void);
int	cc_register_algorithm(struct cc_algo *add_cc);
int	cc_deregister_algorithm(struct cc_algo *remove_cc);

/*
 * NewReno CC functions
 */
int	newreno_init(struct tcpcb *tp);
void	newreno_cwnd_init(struct tcpcb *tp);
void	newreno_ack_received(struct tcpcb *tp, struct tcphdr *th);
void	newreno_pre_fr(struct tcpcb *tp, struct tcphdr *th);
void	newreno_post_fr(struct tcpcb *tp, struct tcphdr *th);
void	newreno_after_idle(struct tcpcb *tp);
void	newreno_after_timeout(struct tcpcb *tp);
void	newreno_ssthresh_update(struct tcpcb *tp);

/*
 * Structure to hold function pointers to the functions responsible
 * for congestion control. Based on similar structure in the SCTP stack
 */
struct cc_algo {
	char name[TCP_CA_NAME_MAX];

	/* init the congestion algorithm for the specified control block */
	int (*init) (struct tcpcb *tp);

	/* deinit the congestion algorithm for the specified control block */
	void (*deinit) (struct tcpcb *tp);

	/* initilise cwnd at the start of a connection */
	void (*cwnd_init) (struct tcpcb *tp);

	/* called on the receipt of a valid ack */
	void (*ack_received) (struct tcpcb *tp, struct tcphdr *th);

	/* called before entering FR */
	void (*pre_fr) (struct tcpcb *tp, struct tcphdr *th);

	/*  after exiting FR */
	void (*post_fr) (struct tcpcb *tp, struct tcphdr *th);

	/* perform tasks when data transfer resumes after an idle period */
	void (*after_idle) (struct tcpcb *tp);

	/* perform tasks when the connection's retransmit timer expires */
	void (*after_timeout) (struct tcpcb *tp);

	STAILQ_ENTRY(cc_algo) entries;
};

#define CC_ALGO(tp) ((tp)->cc_algo)
#define CC_DATA(tp) ((tp)->cc_data)

extern struct rwlock cc_list_lock;
#define CC_LIST_LOCK_INIT() rw_init(&cc_list_lock, "cc_list")
#define CC_LIST_LOCK_DESTROY() rw_destroy(&cc_list_lock)
#define CC_LIST_RLOCK() rw_rlock(&cc_list_lock)
#define CC_LIST_RUNLOCK() rw_runlock(&cc_list_lock)
#define CC_LIST_WLOCK() rw_wlock(&cc_list_lock)
#define CC_LIST_WUNLOCK() rw_wunlock(&cc_list_lock)
#define CC_LIST_TRY_WLOCK() rw_try_upgrade(&cc_list_lock)
#define CC_LIST_W2RLOCK() rw_downgrade(&cc_list_lock)

#endif /* _NETINET_CC_H_ */
