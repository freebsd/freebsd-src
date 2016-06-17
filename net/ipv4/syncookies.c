/*
 *  Syncookies implementation for the Linux kernel
 *
 *  Copyright (C) 1997 Andi Kleen
 *  Based on ideas by D.J.Bernstein and Eric Schenk. 
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 * 
 *  $Id: syncookies.c,v 1.17 2001/10/26 14:55:41 davem Exp $
 *
 *  Missing: IPv6 support. 
 */

#include <linux/tcp.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <net/tcp.h>

extern int sysctl_tcp_syncookies;

/* 
 * This table has to be sorted and terminated with (__u16)-1.
 * XXX generate a better table.
 * Unresolved Issues: HIPPI with a 64k MSS is not well supported.
 */
static __u16 const msstab[] = {
	64 - 1,
	256 - 1,	
	512 - 1,
	536 - 1,
	1024 - 1,	
	1440 - 1,
	1460 - 1,
	4312 - 1,
	(__u16)-1
};
/* The number doesn't include the -1 terminator */
#define NUM_MSS (sizeof(msstab)/sizeof(msstab[0]) - 1)

/*
 * Generate a syncookie.  mssp points to the mss, which is returned
 * rounded down to the value encoded in the cookie.
 */
__u32 cookie_v4_init_sequence(struct sock *sk, struct sk_buff *skb, __u16 *mssp)
{
	int mssind;
	const __u16 mss = *mssp;

	
	sk->tp_pinfo.af_tcp.last_synq_overflow = jiffies;

	/* XXX sort msstab[] by probability?  Binary search? */
	for (mssind = 0; mss > msstab[mssind + 1]; mssind++)
		;
	*mssp = msstab[mssind] + 1;

	NET_INC_STATS_BH(SyncookiesSent);

	return secure_tcp_syn_cookie(skb->nh.iph->saddr, skb->nh.iph->daddr,
				     skb->h.th->source, skb->h.th->dest,
				     ntohl(skb->h.th->seq),
				     jiffies / (HZ * 60), mssind);
}

/* 
 * This (misnamed) value is the age of syncookie which is permitted.
 * Its ideal value should be dependent on TCP_TIMEOUT_INIT and
 * sysctl_tcp_retries1. It's a rather complicated formula (exponential
 * backoff) to compute at runtime so it's currently hardcoded here.
 */
#define COUNTER_TRIES 4
/*  
 * Check if a ack sequence number is a valid syncookie. 
 * Return the decoded mss if it is, or 0 if not.
 */
static inline int cookie_check(struct sk_buff *skb, __u32 cookie)
{
	__u32 seq; 
	__u32 mssind;

	seq = ntohl(skb->h.th->seq)-1; 
	mssind = check_tcp_syn_cookie(cookie,
				      skb->nh.iph->saddr, skb->nh.iph->daddr,
				      skb->h.th->source, skb->h.th->dest,
				      seq, jiffies / (HZ * 60), COUNTER_TRIES);

	return mssind < NUM_MSS ? msstab[mssind] + 1 : 0;
}

extern struct or_calltable or_ipv4;

static inline struct sock *get_cookie_sock(struct sock *sk, struct sk_buff *skb,
					   struct open_request *req,
					   struct dst_entry *dst)
{
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);
	struct sock *child;

	child = tp->af_specific->syn_recv_sock(sk, skb, req, dst);
	if (child)
		tcp_acceptq_queue(sk, req, child);
	else
		tcp_openreq_free(req);

	return child;
}

struct sock *cookie_v4_check(struct sock *sk, struct sk_buff *skb,
			     struct ip_options *opt)
{
	__u32 cookie = ntohl(skb->h.th->ack_seq) - 1; 
	struct sock *ret = sk;
	struct open_request *req; 
	int mss; 
	struct rtable *rt; 
	__u8 rcv_wscale;

	if (!sysctl_tcp_syncookies || !skb->h.th->ack)
		goto out;

  	if (time_after(jiffies, sk->tp_pinfo.af_tcp.last_synq_overflow + TCP_TIMEOUT_INIT) ||
	    (mss = cookie_check(skb, cookie)) == 0) {
	 	NET_INC_STATS_BH(SyncookiesFailed);
		goto out;
	}

	NET_INC_STATS_BH(SyncookiesRecv);

	req = tcp_openreq_alloc();
	ret = NULL;
	if (!req)
		goto out;

	req->rcv_isn		= htonl(skb->h.th->seq) - 1;
	req->snt_isn		= cookie; 
	req->mss		= mss;
 	req->rmt_port		= skb->h.th->source;
	req->af.v4_req.loc_addr = skb->nh.iph->daddr;
	req->af.v4_req.rmt_addr = skb->nh.iph->saddr;
	req->class		= &or_ipv4; /* for savety */
	req->af.v4_req.opt	= NULL;

	/* We throwed the options of the initial SYN away, so we hope
	 * the ACK carries the same options again (see RFC1122 4.2.3.8)
	 */
	if (opt && opt->optlen) {
		int opt_size = sizeof(struct ip_options) + opt->optlen;

		req->af.v4_req.opt = kmalloc(opt_size, GFP_ATOMIC);
		if (req->af.v4_req.opt) {
			if (ip_options_echo(req->af.v4_req.opt, skb)) {
				kfree(req->af.v4_req.opt);
				req->af.v4_req.opt = NULL;
			}
		}
	}

	req->snd_wscale = req->rcv_wscale = req->tstamp_ok = 0;
	req->wscale_ok	= req->sack_ok = 0; 
	req->expires	= 0UL; 
	req->retrans	= 0; 
	
	/*
	 * We need to lookup the route here to get at the correct
	 * window size. We should better make sure that the window size
	 * hasn't changed since we received the original syn, but I see
	 * no easy way to do this. 
	 */
	if (ip_route_output(&rt,
			    opt && 
			    opt->srr ? opt->faddr : req->af.v4_req.rmt_addr,
			    req->af.v4_req.loc_addr,
			    RT_CONN_FLAGS(sk),
			    0)) { 
		tcp_openreq_free(req);
		goto out; 
	}

	/* Try to redo what tcp_v4_send_synack did. */
	req->window_clamp = rt->u.dst.window;  
	tcp_select_initial_window(tcp_full_space(sk), req->mss,
				  &req->rcv_wnd, &req->window_clamp, 
				  0, &rcv_wscale);
	/* BTW win scale with syncookies is 0 by definition */
	req->rcv_wscale	  = rcv_wscale; 

	ret = get_cookie_sock(sk, skb, req, &rt->u.dst);
out:	return ret;
}
