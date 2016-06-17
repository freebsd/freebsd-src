/*
 *	TCP over IPv6
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	$Id: tcp_ipv6.c,v 1.142.2.1 2001/12/21 05:06:08 davem Exp $
 *
 *	Based on: 
 *	linux/net/ipv4/tcp.c
 *	linux/net/ipv4/tcp_input.c
 *	linux/net/ipv4/tcp_output.c
 *
 *	Fixes:
 *	Hideaki YOSHIFUJI	:	sin6_scope_id support
 *	YOSHIFUJI Hideaki @USAGI and:	Support IPV6_V6ONLY socket option, which
 *	Alexey Kuznetsov		allow both IPv4 and IPv6 sockets to bind
 *					a single port at the same time.
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/sched.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/jhash.h>
#include <linux/ipsec.h>

#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/random.h>

#include <net/tcp.h>
#include <net/ndisc.h>
#include <net/ipv6.h>
#include <net/transp_v6.h>
#include <net/addrconf.h>
#include <net/ip6_route.h>
#include <net/inet_ecn.h>

#include <asm/uaccess.h>

static void	tcp_v6_send_reset(struct sk_buff *skb);
static void	tcp_v6_or_send_ack(struct sk_buff *skb, struct open_request *req);
static void	tcp_v6_send_check(struct sock *sk, struct tcphdr *th, int len, 
				  struct sk_buff *skb);

static int	tcp_v6_do_rcv(struct sock *sk, struct sk_buff *skb);
static int	tcp_v6_xmit(struct sk_buff *skb, int ipfragok);

static struct tcp_func ipv6_mapped;
static struct tcp_func ipv6_specific;

/* I have no idea if this is a good hash for v6 or not. -DaveM */
static __inline__ int tcp_v6_hashfn(struct in6_addr *laddr, u16 lport,
				    struct in6_addr *faddr, u16 fport)
{
	int hashent = (lport ^ fport);

	hashent ^= (laddr->s6_addr32[3] ^ faddr->s6_addr32[3]);
	hashent ^= hashent>>16;
	hashent ^= hashent>>8;
	return (hashent & (tcp_ehash_size - 1));
}

static __inline__ int tcp_v6_sk_hashfn(struct sock *sk)
{
	struct in6_addr *laddr = &sk->net_pinfo.af_inet6.rcv_saddr;
	struct in6_addr *faddr = &sk->net_pinfo.af_inet6.daddr;
	__u16 lport = sk->num;
	__u16 fport = sk->dport;
	return tcp_v6_hashfn(laddr, lport, faddr, fport);
}

/* Grrr, addr_type already calculated by caller, but I don't want
 * to add some silly "cookie" argument to this method just for that.
 * But it doesn't matter, the recalculation is in the rarest path
 * this function ever takes.
 */
static int tcp_v6_get_port(struct sock *sk, unsigned short snum)
{
	struct tcp_bind_hashbucket *head;
	struct tcp_bind_bucket *tb;
	int ret;

	local_bh_disable();
	if (snum == 0) {
		int low = sysctl_local_port_range[0];
		int high = sysctl_local_port_range[1];
		int remaining = (high - low) + 1;
		int rover;

		spin_lock(&tcp_portalloc_lock);
		rover = tcp_port_rover;
		do {	rover++;
			if ((rover < low) || (rover > high))
				rover = low;
			head = &tcp_bhash[tcp_bhashfn(rover)];
			spin_lock(&head->lock);
			for (tb = head->chain; tb; tb = tb->next)
				if (tb->port == rover)
					goto next;
			break;
		next:
			spin_unlock(&head->lock);
		} while (--remaining > 0);
		tcp_port_rover = rover;
		spin_unlock(&tcp_portalloc_lock);

		/* Exhausted local port range during search? */
		ret = 1;
		if (remaining <= 0)
			goto fail;

		/* OK, here is the one we will use. */
		snum = rover;
		tb = NULL;
	} else {
		head = &tcp_bhash[tcp_bhashfn(snum)];
		spin_lock(&head->lock);
		for (tb = head->chain; tb != NULL; tb = tb->next)
			if (tb->port == snum)
				break;
	}
	if (tb != NULL && tb->owners != NULL) {
		if (tb->fastreuse > 0 && sk->reuse != 0 && sk->state != TCP_LISTEN) {
			goto success;
		} else {
			struct sock *sk2 = tb->owners;
			int sk_reuse = sk->reuse;
			int addr_type = ipv6_addr_type(&sk->net_pinfo.af_inet6.rcv_saddr);

			/* We must walk the whole port owner list in this case. -DaveM */
			for( ; sk2 != NULL; sk2 = sk2->bind_next) {
				if (sk != sk2 &&
				    (!sk->bound_dev_if ||
				     !sk2->bound_dev_if ||
				     sk->bound_dev_if == sk2->bound_dev_if)) {
					if (!sk_reuse	||
					    !sk2->reuse	||
					    sk2->state == TCP_LISTEN) {
						/* NOTE: IPv6 tw bucket have different format */
						if ((!sk2->rcv_saddr && !ipv6_only_sock(sk)) ||
						    (sk2->family == AF_INET6 &&
						     ipv6_addr_any(&sk2->net_pinfo.af_inet6.rcv_saddr) &&
						     !(ipv6_only_sock(sk2) && addr_type == IPV6_ADDR_MAPPED)) ||
						    (addr_type == IPV6_ADDR_ANY &&
						     (!ipv6_only_sock(sk) ||
						      !(sk2->family == AF_INET6 ? ipv6_addr_type(&sk2->net_pinfo.af_inet6.rcv_saddr) == IPV6_ADDR_MAPPED : 1))) ||
						    (sk2->family == AF_INET6 &&
						     !ipv6_addr_cmp(&sk->net_pinfo.af_inet6.rcv_saddr,
								    sk2->state != TCP_TIME_WAIT ?
								    &sk2->net_pinfo.af_inet6.rcv_saddr :
								    &((struct tcp_tw_bucket*)sk)->v6_rcv_saddr)) ||
						    (addr_type == IPV6_ADDR_MAPPED && 
						     !ipv6_only_sock(sk2) &&
						     (!sk2->rcv_saddr ||
						      !sk->rcv_saddr ||
						      sk->rcv_saddr == sk2->rcv_saddr)))
							break;
					}
				}
			}
			/* If we found a conflict, fail. */
			ret = 1;
			if (sk2 != NULL)
				goto fail_unlock;
		}
	}
	ret = 1;
	if (tb == NULL &&
	    (tb = tcp_bucket_create(head, snum)) == NULL)
			goto fail_unlock;
	if (tb->owners == NULL) {
		if (sk->reuse && sk->state != TCP_LISTEN)
			tb->fastreuse = 1;
		else
			tb->fastreuse = 0;
	} else if (tb->fastreuse &&
		   ((sk->reuse == 0) || (sk->state == TCP_LISTEN)))
		tb->fastreuse = 0;

success:
	sk->num = snum;
	if (sk->prev == NULL) {
		if ((sk->bind_next = tb->owners) != NULL)
			tb->owners->bind_pprev = &sk->bind_next;
		tb->owners = sk;
		sk->bind_pprev = &tb->owners;
		sk->prev = (struct sock *) tb;
	} else {
		BUG_TRAP(sk->prev == (struct sock *) tb);
	}
	ret = 0;

fail_unlock:
	spin_unlock(&head->lock);
fail:
	local_bh_enable();
	return ret;
}

static __inline__ void __tcp_v6_hash(struct sock *sk)
{
	struct sock **skp;
	rwlock_t *lock;

	BUG_TRAP(sk->pprev==NULL);

	if(sk->state == TCP_LISTEN) {
		skp = &tcp_listening_hash[tcp_sk_listen_hashfn(sk)];
		lock = &tcp_lhash_lock;
		tcp_listen_wlock();
	} else {
		skp = &tcp_ehash[(sk->hashent = tcp_v6_sk_hashfn(sk))].chain;
		lock = &tcp_ehash[sk->hashent].lock;
		write_lock(lock);
	}

	if((sk->next = *skp) != NULL)
		(*skp)->pprev = &sk->next;
	*skp = sk;
	sk->pprev = skp;
	sock_prot_inc_use(sk->prot);
	write_unlock(lock);
}


static void tcp_v6_hash(struct sock *sk)
{
	if(sk->state != TCP_CLOSE) {
		if (sk->tp_pinfo.af_tcp.af_specific == &ipv6_mapped) {
			tcp_prot.hash(sk);
			return;
		}
		local_bh_disable();
		__tcp_v6_hash(sk);
		local_bh_enable();
	}
}

static struct sock *tcp_v6_lookup_listener(struct in6_addr *daddr, unsigned short hnum, int dif)
{
	struct sock *sk;
	struct sock *result = NULL;
	int score, hiscore;

	hiscore=0;
	read_lock(&tcp_lhash_lock);
	sk = tcp_listening_hash[tcp_lhashfn(hnum)];
	for(; sk; sk = sk->next) {
		if((sk->num == hnum) && (sk->family == PF_INET6)) {
			struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
			
			score = 1;
			if(!ipv6_addr_any(&np->rcv_saddr)) {
				if(ipv6_addr_cmp(&np->rcv_saddr, daddr))
					continue;
				score++;
			}
			if (sk->bound_dev_if) {
				if (sk->bound_dev_if != dif)
					continue;
				score++;
			}
			if (score == 3) {
				result = sk;
				break;
			}
			if (score > hiscore) {
				hiscore = score;
				result = sk;
			}
		}
	}
	if (result)
		sock_hold(result);
	read_unlock(&tcp_lhash_lock);
	return result;
}

/* Sockets in TCP_CLOSE state are _always_ taken out of the hash, so
 * we need not check it for TCP lookups anymore, thanks Alexey. -DaveM
 *
 * The sockhash lock must be held as a reader here.
 */

static inline struct sock *__tcp_v6_lookup_established(struct in6_addr *saddr, u16 sport,
						       struct in6_addr *daddr, u16 hnum,
						       int dif)
{
	struct tcp_ehash_bucket *head;
	struct sock *sk;
	__u32 ports = TCP_COMBINED_PORTS(sport, hnum);
	int hash;

	/* Optimize here for direct hit, only listening connections can
	 * have wildcards anyways.
	 */
	hash = tcp_v6_hashfn(daddr, hnum, saddr, sport);
	head = &tcp_ehash[hash];
	read_lock(&head->lock);
	for(sk = head->chain; sk; sk = sk->next) {
		/* For IPV6 do the cheaper port and family tests first. */
		if(TCP_IPV6_MATCH(sk, saddr, daddr, ports, dif))
			goto hit; /* You sunk my battleship! */
	}
	/* Must check for a TIME_WAIT'er before going to listener hash. */
	for(sk = (head + tcp_ehash_size)->chain; sk; sk = sk->next) {
		if(*((__u32 *)&(sk->dport))	== ports	&&
		   sk->family			== PF_INET6) {
			struct tcp_tw_bucket *tw = (struct tcp_tw_bucket *)sk;
			if(!ipv6_addr_cmp(&tw->v6_daddr, saddr)	&&
			   !ipv6_addr_cmp(&tw->v6_rcv_saddr, daddr) &&
			   (!sk->bound_dev_if || sk->bound_dev_if == dif))
				goto hit;
		}
	}
	read_unlock(&head->lock);
	return NULL;

hit:
	sock_hold(sk);
	read_unlock(&head->lock);
	return sk;
}


static inline struct sock *__tcp_v6_lookup(struct in6_addr *saddr, u16 sport,
					   struct in6_addr *daddr, u16 hnum,
					   int dif)
{
	struct sock *sk;

	sk = __tcp_v6_lookup_established(saddr, sport, daddr, hnum, dif);

	if (sk)
		return sk;

	return tcp_v6_lookup_listener(daddr, hnum, dif);
}

inline struct sock *tcp_v6_lookup(struct in6_addr *saddr, u16 sport,
				  struct in6_addr *daddr, u16 dport,
				  int dif)
{
	struct sock *sk;

	local_bh_disable();
	sk = __tcp_v6_lookup(saddr, sport, daddr, ntohs(dport), dif);
	local_bh_enable();

	return sk;
}


/*
 * Open request hash tables.
 */

static u32 tcp_v6_synq_hash(struct in6_addr *raddr, u16 rport, u32 rnd)
{
	u32 a, b, c;

	a = raddr->s6_addr32[0];
	b = raddr->s6_addr32[1];
	c = raddr->s6_addr32[2];

	a += JHASH_GOLDEN_RATIO;
	b += JHASH_GOLDEN_RATIO;
	c += rnd;
	__jhash_mix(a, b, c);

	a += raddr->s6_addr32[3];
	b += (u32) rport;
	__jhash_mix(a, b, c);

	return c & (TCP_SYNQ_HSIZE - 1);
}

static struct open_request *tcp_v6_search_req(struct tcp_opt *tp,
					      struct open_request ***prevp,
					      __u16 rport,
					      struct in6_addr *raddr,
					      struct in6_addr *laddr,
					      int iif)
{
	struct tcp_listen_opt *lopt = tp->listen_opt;
	struct open_request *req, **prev;  

	for (prev = &lopt->syn_table[tcp_v6_synq_hash(raddr, rport, lopt->hash_rnd)];
	     (req = *prev) != NULL;
	     prev = &req->dl_next) {
		if (req->rmt_port == rport &&
		    req->class->family == AF_INET6 &&
		    !ipv6_addr_cmp(&req->af.v6_req.rmt_addr, raddr) &&
		    !ipv6_addr_cmp(&req->af.v6_req.loc_addr, laddr) &&
		    (!req->af.v6_req.iif || req->af.v6_req.iif == iif)) {
			BUG_TRAP(req->sk == NULL);
			*prevp = prev;
			return req;
		}
	}

	return NULL;
}

static __inline__ u16 tcp_v6_check(struct tcphdr *th, int len,
				   struct in6_addr *saddr, 
				   struct in6_addr *daddr, 
				   unsigned long base)
{
	return csum_ipv6_magic(saddr, daddr, len, IPPROTO_TCP, base);
}

static __u32 tcp_v6_init_sequence(struct sock *sk, struct sk_buff *skb)
{
	if (skb->protocol == htons(ETH_P_IPV6)) {
		return secure_tcpv6_sequence_number(skb->nh.ipv6h->daddr.s6_addr32,
						    skb->nh.ipv6h->saddr.s6_addr32,
						    skb->h.th->dest,
						    skb->h.th->source);
	} else {
		return secure_tcp_sequence_number(skb->nh.iph->daddr,
						  skb->nh.iph->saddr,
						  skb->h.th->dest,
						  skb->h.th->source);
	}
}

static int tcp_v6_check_established(struct sock *sk)
{
	struct in6_addr *daddr = &sk->net_pinfo.af_inet6.rcv_saddr;
	struct in6_addr *saddr = &sk->net_pinfo.af_inet6.daddr;
	int dif = sk->bound_dev_if;
	u32 ports = TCP_COMBINED_PORTS(sk->dport, sk->num);
	int hash = tcp_v6_hashfn(daddr, sk->num, saddr, sk->dport);
	struct tcp_ehash_bucket *head = &tcp_ehash[hash];
	struct sock *sk2, **skp;
	struct tcp_tw_bucket *tw;

	write_lock_bh(&head->lock);

	for(skp = &(head + tcp_ehash_size)->chain; (sk2=*skp)!=NULL; skp = &sk2->next) {
		tw = (struct tcp_tw_bucket*)sk2;

		if(*((__u32 *)&(sk2->dport))	== ports	&&
		   sk2->family			== PF_INET6	&&
		   !ipv6_addr_cmp(&tw->v6_daddr, saddr)		&&
		   !ipv6_addr_cmp(&tw->v6_rcv_saddr, daddr)	&&
		   sk2->bound_dev_if == sk->bound_dev_if) {
			struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);

			if (tw->ts_recent_stamp) {
				/* See comment in tcp_ipv4.c */
				if ((tp->write_seq = tw->snd_nxt+65535+2) == 0)
					tp->write_seq = 1;
				tp->ts_recent = tw->ts_recent;
				tp->ts_recent_stamp = tw->ts_recent_stamp;
				sock_hold(sk2);
				skp = &head->chain;
				goto unique;
			} else
				goto not_unique;
		}
	}
	tw = NULL;

	for(skp = &head->chain; (sk2=*skp)!=NULL; skp = &sk2->next) {
		if(TCP_IPV6_MATCH(sk, saddr, daddr, ports, dif))
			goto not_unique;
	}

unique:
	BUG_TRAP(sk->pprev==NULL);
	if ((sk->next = *skp) != NULL)
		(*skp)->pprev = &sk->next;

	*skp = sk;
	sk->pprev = skp;
	sk->hashent = hash;
	sock_prot_inc_use(sk->prot);
	write_unlock_bh(&head->lock);

	if (tw) {
		/* Silly. Should hash-dance instead... */
		local_bh_disable();
		tcp_tw_deschedule(tw);
		tcp_timewait_kill(tw);
		NET_INC_STATS_BH(TimeWaitRecycled);
		local_bh_enable();

		tcp_tw_put(tw);
	}
	return 0;

not_unique:
	write_unlock_bh(&head->lock);
	return -EADDRNOTAVAIL;
}

static int tcp_v6_hash_connect(struct sock *sk)
{
	struct tcp_bind_hashbucket *head;
	struct tcp_bind_bucket *tb;

	/* XXX */ 
	if (sk->num == 0) { 
		int err = tcp_v6_get_port(sk, sk->num);
		if (err)
			return err;
		sk->sport = htons(sk->num); 	
	}

	head = &tcp_bhash[tcp_bhashfn(sk->num)];
	tb = head->chain;

	spin_lock_bh(&head->lock);

	if (tb->owners == sk && sk->bind_next == NULL) {
		__tcp_v6_hash(sk);
		spin_unlock_bh(&head->lock);
		return 0;
	} else {
		spin_unlock_bh(&head->lock);
		return tcp_v6_check_established(sk);
	}
}

static __inline__ int tcp_v6_iif(struct sk_buff *skb)
{
	struct inet6_skb_parm *opt = (struct inet6_skb_parm *) skb->cb;
	return opt->iif;
}

static int tcp_v6_connect(struct sock *sk, struct sockaddr *uaddr, 
			  int addr_len)
{
	struct sockaddr_in6 *usin = (struct sockaddr_in6 *) uaddr;
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
	struct in6_addr *saddr = NULL;
	struct in6_addr saddr_buf;
	struct flowi fl;
	struct dst_entry *dst;
	int addr_type;
	int err;

	if (addr_len < SIN6_LEN_RFC2133) 
		return -EINVAL;

	if (usin->sin6_family != AF_INET6) 
		return(-EAFNOSUPPORT);

	fl.fl6_flowlabel = 0;
	if (np->sndflow) {
		fl.fl6_flowlabel = usin->sin6_flowinfo&IPV6_FLOWINFO_MASK;
		IP6_ECN_flow_init(fl.fl6_flowlabel);
		if (fl.fl6_flowlabel&IPV6_FLOWLABEL_MASK) {
			struct ip6_flowlabel *flowlabel;
			flowlabel = fl6_sock_lookup(sk, fl.fl6_flowlabel);
			if (flowlabel == NULL)
				return -EINVAL;
			ipv6_addr_copy(&usin->sin6_addr, &flowlabel->dst);
			fl6_sock_release(flowlabel);
		}
	}

	/*
  	 *	connect() to INADDR_ANY means loopback (BSD'ism).
  	 */
  	
  	if(ipv6_addr_any(&usin->sin6_addr))
		usin->sin6_addr.s6_addr[15] = 0x1; 

	addr_type = ipv6_addr_type(&usin->sin6_addr);

	if(addr_type & IPV6_ADDR_MULTICAST)
		return -ENETUNREACH;

	if (addr_type&IPV6_ADDR_LINKLOCAL) {
		if (addr_len >= sizeof(struct sockaddr_in6) &&
		    usin->sin6_scope_id) {
			/* If interface is set while binding, indices
			 * must coincide.
			 */
			if (sk->bound_dev_if &&
			    sk->bound_dev_if != usin->sin6_scope_id)
				return -EINVAL;

			sk->bound_dev_if = usin->sin6_scope_id;
		}

		/* Connect to link-local address requires an interface */
		if (sk->bound_dev_if == 0)
			return -EINVAL;
	}

	if (tp->ts_recent_stamp && ipv6_addr_cmp(&np->daddr, &usin->sin6_addr)) {
		tp->ts_recent = 0;
		tp->ts_recent_stamp = 0;
		tp->write_seq = 0;
	}

	ipv6_addr_copy(&np->daddr, &usin->sin6_addr);
	np->flow_label = fl.fl6_flowlabel;

	/*
	 *	TCP over IPv4
	 */

	if (addr_type == IPV6_ADDR_MAPPED) {
		u32 exthdrlen = tp->ext_header_len;
		struct sockaddr_in sin;

		SOCK_DEBUG(sk, "connect: ipv4 mapped\n");

		if (__ipv6_only_sock(sk))
			return -ENETUNREACH;

		sin.sin_family = AF_INET;
		sin.sin_port = usin->sin6_port;
		sin.sin_addr.s_addr = usin->sin6_addr.s6_addr32[3];

		sk->tp_pinfo.af_tcp.af_specific = &ipv6_mapped;
		sk->backlog_rcv = tcp_v4_do_rcv;

		err = tcp_v4_connect(sk, (struct sockaddr *)&sin, sizeof(sin));

		if (err) {
			tp->ext_header_len = exthdrlen;
			sk->tp_pinfo.af_tcp.af_specific = &ipv6_specific;
			sk->backlog_rcv = tcp_v6_do_rcv;
			goto failure;
		} else {
			ipv6_addr_set(&np->saddr, 0, 0, htonl(0x0000FFFF),
				      sk->saddr);
			ipv6_addr_set(&np->rcv_saddr, 0, 0, htonl(0x0000FFFF),
				      sk->rcv_saddr);
		}

		return err;
	}

	if (!ipv6_addr_any(&np->rcv_saddr))
		saddr = &np->rcv_saddr;

	fl.proto = IPPROTO_TCP;
	fl.fl6_dst = &np->daddr;
	fl.fl6_src = saddr;
	fl.oif = sk->bound_dev_if;
	fl.uli_u.ports.dport = usin->sin6_port;
	fl.uli_u.ports.sport = sk->sport;

	if (np->opt && np->opt->srcrt) {
		struct rt0_hdr *rt0 = (struct rt0_hdr *) np->opt->srcrt;
		fl.nl_u.ip6_u.daddr = rt0->addr;
	}

	dst = ip6_route_output(sk, &fl);

	if ((err = dst->error) != 0) {
		dst_release(dst);
		goto failure;
	}

	ip6_dst_store(sk, dst, NULL);
	sk->route_caps = dst->dev->features&~NETIF_F_IP_CSUM;

	if (saddr == NULL) {
		err = ipv6_get_saddr(dst, &np->daddr, &saddr_buf);
		if (err)
			goto failure;

		saddr = &saddr_buf;
	}

	/* set the source address */
	ipv6_addr_copy(&np->rcv_saddr, saddr);
	ipv6_addr_copy(&np->saddr, saddr);
	sk->rcv_saddr= LOOPBACK4_IPV6;

	tp->ext_header_len = 0;
	if (np->opt)
		tp->ext_header_len = np->opt->opt_flen+np->opt->opt_nflen;
	tp->mss_clamp = IPV6_MIN_MTU - sizeof(struct tcphdr) - sizeof(struct ipv6hdr);

	sk->dport = usin->sin6_port;

	tcp_set_state(sk, TCP_SYN_SENT); 
	err = tcp_v6_hash_connect(sk); 
	if (err)
		goto late_failure;

	if (!tp->write_seq)
		tp->write_seq = secure_tcpv6_sequence_number(np->saddr.s6_addr32,
							     np->daddr.s6_addr32,
							     sk->sport, sk->dport);
	err = tcp_connect(sk);
	if (err)
		goto late_failure;

	return 0;

late_failure:
	tcp_set_state(sk, TCP_CLOSE); 
failure:
	__sk_dst_reset(sk);
	sk->dport = 0;
	sk->route_caps = 0;
	return err;
}

void tcp_v6_err(struct sk_buff *skb, struct inet6_skb_parm *opt,
		int type, int code, int offset, __u32 info)
{
	struct ipv6hdr *hdr = (struct ipv6hdr*)skb->data;
	struct tcphdr *th = (struct tcphdr *)(skb->data+offset);
	struct ipv6_pinfo *np;
	struct sock *sk;
	int err;
	struct tcp_opt *tp; 
	__u32 seq;

	sk = tcp_v6_lookup(&hdr->daddr, th->dest, &hdr->saddr, th->source, skb->dev->ifindex);

	if (sk == NULL) {
		ICMP6_INC_STATS_BH(Icmp6InErrors);
		return;
	}

	if (sk->state == TCP_TIME_WAIT) {
		tcp_tw_put((struct tcp_tw_bucket*)sk);
		return;
	}

	bh_lock_sock(sk);
	if (sk->lock.users)
		NET_INC_STATS_BH(LockDroppedIcmps);

	if (sk->state == TCP_CLOSE)
		goto out;

	tp = &sk->tp_pinfo.af_tcp;
	seq = ntohl(th->seq); 
	if (sk->state != TCP_LISTEN && !between(seq, tp->snd_una, tp->snd_nxt)) {
		NET_INC_STATS_BH(OutOfWindowIcmps);
		goto out;
	}

	np = &sk->net_pinfo.af_inet6;

	if (type == ICMPV6_PKT_TOOBIG) {
		struct dst_entry *dst = NULL;

		if (sk->lock.users)
			goto out;
		if ((1<<sk->state)&(TCPF_LISTEN|TCPF_CLOSE))
			goto out;

		/* icmp should have updated the destination cache entry */
		dst = __sk_dst_check(sk, np->dst_cookie);

		if (dst == NULL) {
			struct flowi fl;

			/* BUGGG_FUTURE: Again, it is not clear how
			   to handle rthdr case. Ignore this complexity
			   for now.
			 */
			fl.proto = IPPROTO_TCP;
			fl.nl_u.ip6_u.daddr = &np->daddr;
			fl.nl_u.ip6_u.saddr = &np->saddr;
			fl.oif = sk->bound_dev_if;
			fl.uli_u.ports.dport = sk->dport;
			fl.uli_u.ports.sport = sk->sport;

			dst = ip6_route_output(sk, &fl);
		} else
			dst_hold(dst);

		if (dst->error) {
			sk->err_soft = -dst->error;
		} else if (tp->pmtu_cookie > dst->pmtu) {
			tcp_sync_mss(sk, dst->pmtu);
			tcp_simple_retransmit(sk);
		} /* else let the usual retransmit timer handle it */
		dst_release(dst);
		goto out;
	}

	icmpv6_err_convert(type, code, &err);

	/* Might be for an open_request */
	switch (sk->state) {
		struct open_request *req, **prev;
	case TCP_LISTEN:
		if (sk->lock.users)
			goto out;

		req = tcp_v6_search_req(tp, &prev, th->dest, &hdr->daddr,
					&hdr->saddr, tcp_v6_iif(skb));
		if (!req)
			goto out;

		/* ICMPs are not backlogged, hence we cannot get
		 * an established socket here.
		 */
		BUG_TRAP(req->sk == NULL);

		if (seq != req->snt_isn) {
			NET_INC_STATS_BH(OutOfWindowIcmps);
			goto out;
		}

		tcp_synq_drop(sk, req, prev);
		goto out;

	case TCP_SYN_SENT:
	case TCP_SYN_RECV:  /* Cannot happen.
			       It can, it SYNs are crossed. --ANK */ 
		if (sk->lock.users == 0) {
			TCP_INC_STATS_BH(TcpAttemptFails);
			sk->err = err;
			sk->error_report(sk);		/* Wake people up to see the error (see connect in sock.c) */

			tcp_done(sk);
		} else {
			sk->err_soft = err;
		}
		goto out;
	}

	if (sk->lock.users == 0 && np->recverr) {
		sk->err = err;
		sk->error_report(sk);
	} else {
		sk->err_soft = err;
	}

out:
	bh_unlock_sock(sk);
	sock_put(sk);
}


static int tcp_v6_send_synack(struct sock *sk, struct open_request *req,
			      struct dst_entry *dst)
{
	struct sk_buff * skb;
	struct ipv6_txoptions *opt = NULL;
	struct flowi fl;
	int err = -1;

	fl.proto = IPPROTO_TCP;
	fl.nl_u.ip6_u.daddr = &req->af.v6_req.rmt_addr;
	fl.nl_u.ip6_u.saddr = &req->af.v6_req.loc_addr;
	fl.fl6_flowlabel = 0;
	fl.oif = req->af.v6_req.iif;
	fl.uli_u.ports.dport = req->rmt_port;
	fl.uli_u.ports.sport = sk->sport;

	if (dst == NULL) {
		opt = sk->net_pinfo.af_inet6.opt;
		if (opt == NULL &&
		    sk->net_pinfo.af_inet6.rxopt.bits.srcrt == 2 &&
		    req->af.v6_req.pktopts) {
			struct sk_buff *pktopts = req->af.v6_req.pktopts;
			struct inet6_skb_parm *rxopt = (struct inet6_skb_parm *)pktopts->cb;
			if (rxopt->srcrt)
				opt = ipv6_invert_rthdr(sk, (struct ipv6_rt_hdr*)(pktopts->nh.raw + rxopt->srcrt));
		}

		if (opt && opt->srcrt) {
			struct rt0_hdr *rt0 = (struct rt0_hdr *) opt->srcrt;
			fl.nl_u.ip6_u.daddr = rt0->addr;
		}

		dst = ip6_route_output(sk, &fl);
		if (dst->error)
			goto done;
	}

	skb = tcp_make_synack(sk, dst, req);
	if (skb) {
		struct tcphdr *th = skb->h.th;

		th->check = tcp_v6_check(th, skb->len,
					 &req->af.v6_req.loc_addr, &req->af.v6_req.rmt_addr,
					 csum_partial((char *)th, skb->len, skb->csum));

		fl.nl_u.ip6_u.daddr = &req->af.v6_req.rmt_addr;
		err = ip6_xmit(sk, skb, &fl, opt);
		if (err == NET_XMIT_CN)
			err = 0;
	}

done:
	dst_release(dst);
        if (opt && opt != sk->net_pinfo.af_inet6.opt)
		sock_kfree_s(sk, opt, opt->tot_len);
	return err;
}

static void tcp_v6_or_free(struct open_request *req)
{
	if (req->af.v6_req.pktopts)
		kfree_skb(req->af.v6_req.pktopts);
}

static struct or_calltable or_ipv6 = {
	AF_INET6,
	tcp_v6_send_synack,
	tcp_v6_or_send_ack,
	tcp_v6_or_free,
	tcp_v6_send_reset
};

static int ipv6_opt_accepted(struct sock *sk, struct sk_buff *skb)
{
	struct inet6_skb_parm *opt = (struct inet6_skb_parm *)skb->cb;

	if (sk->net_pinfo.af_inet6.rxopt.all) {
		if ((opt->hop && sk->net_pinfo.af_inet6.rxopt.bits.hopopts) ||
		    ((IPV6_FLOWINFO_MASK&*(u32*)skb->nh.raw) &&
		     sk->net_pinfo.af_inet6.rxopt.bits.rxflow) ||
		    (opt->srcrt && sk->net_pinfo.af_inet6.rxopt.bits.srcrt) ||
		    ((opt->dst1 || opt->dst0) && sk->net_pinfo.af_inet6.rxopt.bits.dstopts))
			return 1;
	}
	return 0;
}


static void tcp_v6_send_check(struct sock *sk, struct tcphdr *th, int len, 
			      struct sk_buff *skb)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;

	if (skb->ip_summed == CHECKSUM_HW) {
		th->check = ~csum_ipv6_magic(&np->saddr, &np->daddr, len, IPPROTO_TCP,  0);
		skb->csum = offsetof(struct tcphdr, check);
	} else {
		th->check = csum_ipv6_magic(&np->saddr, &np->daddr, len, IPPROTO_TCP, 
					    csum_partial((char *)th, th->doff<<2, 
							 skb->csum));
	}
}


static void tcp_v6_send_reset(struct sk_buff *skb)
{
	struct tcphdr *th = skb->h.th, *t1; 
	struct sk_buff *buff;
	struct flowi fl;

	if (th->rst)
		return;

	if (ipv6_addr_is_multicast(&skb->nh.ipv6h->daddr))
		return; 

	/*
	 * We need to grab some memory, and put together an RST,
	 * and then put it into the queue to be sent.
	 */

	buff = alloc_skb(MAX_HEADER + sizeof(struct ipv6hdr), GFP_ATOMIC);
	if (buff == NULL) 
	  	return;

	skb_reserve(buff, MAX_HEADER + sizeof(struct ipv6hdr));

	t1 = (struct tcphdr *) skb_push(buff,sizeof(struct tcphdr));

	/* Swap the send and the receive. */
	memset(t1, 0, sizeof(*t1));
	t1->dest = th->source;
	t1->source = th->dest;
	t1->doff = sizeof(*t1)/4;
	t1->rst = 1;
  
	if(th->ack) {
	  	t1->seq = th->ack_seq;
	} else {
		t1->ack = 1;
		t1->ack_seq = htonl(ntohl(th->seq) + th->syn + th->fin
				    + skb->len - (th->doff<<2));
	}

	buff->csum = csum_partial((char *)t1, sizeof(*t1), 0);

	fl.nl_u.ip6_u.daddr = &skb->nh.ipv6h->saddr;
	fl.nl_u.ip6_u.saddr = &skb->nh.ipv6h->daddr;
	fl.fl6_flowlabel = 0;

	t1->check = csum_ipv6_magic(fl.nl_u.ip6_u.saddr,
				    fl.nl_u.ip6_u.daddr, 
				    sizeof(*t1), IPPROTO_TCP,
				    buff->csum);

	fl.proto = IPPROTO_TCP;
	fl.oif = tcp_v6_iif(skb);
	fl.uli_u.ports.dport = t1->dest;
	fl.uli_u.ports.sport = t1->source;

	/* sk = NULL, but it is safe for now. RST socket required. */
	buff->dst = ip6_route_output(NULL, &fl);

	if (buff->dst->error == 0) {
		ip6_xmit(NULL, buff, &fl, NULL);
		TCP_INC_STATS_BH(TcpOutSegs);
		TCP_INC_STATS_BH(TcpOutRsts);
		return;
	}

	kfree_skb(buff);
}

static void tcp_v6_send_ack(struct sk_buff *skb, u32 seq, u32 ack, u32 win, u32 ts)
{
	struct tcphdr *th = skb->h.th, *t1;
	struct sk_buff *buff;
	struct flowi fl;
	int tot_len = sizeof(struct tcphdr);

	buff = alloc_skb(MAX_HEADER + sizeof(struct ipv6hdr), GFP_ATOMIC);
	if (buff == NULL)
		return;

	skb_reserve(buff, MAX_HEADER + sizeof(struct ipv6hdr));

	if (ts)
		tot_len += 3*4;

	t1 = (struct tcphdr *) skb_push(buff,tot_len);

	/* Swap the send and the receive. */
	memset(t1, 0, sizeof(*t1));
	t1->dest = th->source;
	t1->source = th->dest;
	t1->doff = tot_len/4;
	t1->seq = htonl(seq);
	t1->ack_seq = htonl(ack);
	t1->ack = 1;
	t1->window = htons(win);
	
	if (ts) {
		u32 *ptr = (u32*)(t1 + 1);
		*ptr++ = htonl((TCPOPT_NOP << 24) |
			       (TCPOPT_NOP << 16) |
			       (TCPOPT_TIMESTAMP << 8) |
			       TCPOLEN_TIMESTAMP);
		*ptr++ = htonl(tcp_time_stamp);
		*ptr = htonl(ts);
	}

	buff->csum = csum_partial((char *)t1, tot_len, 0);

	fl.nl_u.ip6_u.daddr = &skb->nh.ipv6h->saddr;
	fl.nl_u.ip6_u.saddr = &skb->nh.ipv6h->daddr;
	fl.fl6_flowlabel = 0;

	t1->check = csum_ipv6_magic(fl.nl_u.ip6_u.saddr,
				    fl.nl_u.ip6_u.daddr, 
				    tot_len, IPPROTO_TCP,
				    buff->csum);

	fl.proto = IPPROTO_TCP;
	fl.oif = tcp_v6_iif(skb);
	fl.uli_u.ports.dport = t1->dest;
	fl.uli_u.ports.sport = t1->source;

	buff->dst = ip6_route_output(NULL, &fl);

	if (buff->dst->error == 0) {
		ip6_xmit(NULL, buff, &fl, NULL);
		TCP_INC_STATS_BH(TcpOutSegs);
		return;
	}

	kfree_skb(buff);
}

static void tcp_v6_timewait_ack(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_tw_bucket *tw = (struct tcp_tw_bucket *)sk;

	tcp_v6_send_ack(skb, tw->snd_nxt, tw->rcv_nxt,
			tw->rcv_wnd>>tw->rcv_wscale, tw->ts_recent);

	tcp_tw_put(tw);
}

static void tcp_v6_or_send_ack(struct sk_buff *skb, struct open_request *req)
{
	tcp_v6_send_ack(skb, req->snt_isn+1, req->rcv_isn+1, req->rcv_wnd, req->ts_recent);
}


static struct sock *tcp_v6_hnd_req(struct sock *sk,struct sk_buff *skb)
{
	struct open_request *req, **prev;
	struct tcphdr *th = skb->h.th;
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);
	struct sock *nsk;

	/* Find possible connection requests. */
	req = tcp_v6_search_req(tp, &prev, th->source, &skb->nh.ipv6h->saddr,
				&skb->nh.ipv6h->daddr, tcp_v6_iif(skb));
	if (req)
		return tcp_check_req(sk, skb, req, prev);

	nsk = __tcp_v6_lookup_established(&skb->nh.ipv6h->saddr,
					  th->source,
					  &skb->nh.ipv6h->daddr,
					  ntohs(th->dest),
					  tcp_v6_iif(skb));

	if (nsk) {
		if (nsk->state != TCP_TIME_WAIT) {
			bh_lock_sock(nsk);
			return nsk;
		}
		tcp_tw_put((struct tcp_tw_bucket*)nsk);
		return NULL;
	}

#if 0 /*def CONFIG_SYN_COOKIES*/
	if (!th->rst && !th->syn && th->ack)
		sk = cookie_v6_check(sk, skb, &(IPCB(skb)->opt));
#endif
	return sk;
}

static void tcp_v6_synq_add(struct sock *sk, struct open_request *req)
{
	struct tcp_opt *tp = &sk->tp_pinfo.af_tcp;
	struct tcp_listen_opt *lopt = tp->listen_opt;
	u32 h = tcp_v6_synq_hash(&req->af.v6_req.rmt_addr, req->rmt_port, lopt->hash_rnd);

	req->sk = NULL;
	req->expires = jiffies + TCP_TIMEOUT_INIT;
	req->retrans = 0;
	req->dl_next = lopt->syn_table[h];

	write_lock(&tp->syn_wait_lock);
	lopt->syn_table[h] = req;
	write_unlock(&tp->syn_wait_lock);

	tcp_synq_added(sk);
}


/* FIXME: this is substantially similar to the ipv4 code.
 * Can some kind of merge be done? -- erics
 */
static int tcp_v6_conn_request(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_opt tp;
	struct open_request *req = NULL;
	__u32 isn = TCP_SKB_CB(skb)->when;

	if (skb->protocol == htons(ETH_P_IP))
		return tcp_v4_conn_request(sk, skb);

	/* FIXME: do the same check for anycast */
	if (ipv6_addr_is_multicast(&skb->nh.ipv6h->daddr))
		goto drop; 

	/*
	 *	There are no SYN attacks on IPv6, yet...	
	 */
	if (tcp_synq_is_full(sk) && !isn) {
		if (net_ratelimit())
			printk(KERN_INFO "TCPv6: dropping request, synflood is possible\n");
		goto drop;		
	}

	if (tcp_acceptq_is_full(sk) && tcp_synq_young(sk) > 1)
		goto drop;

	req = tcp_openreq_alloc();
	if (req == NULL)
		goto drop;

	tcp_clear_options(&tp);
	tp.mss_clamp = IPV6_MIN_MTU - sizeof(struct tcphdr) - sizeof(struct ipv6hdr);
	tp.user_mss = sk->tp_pinfo.af_tcp.user_mss;

	tcp_parse_options(skb, &tp, 0);

	tp.tstamp_ok = tp.saw_tstamp;
	tcp_openreq_init(req, &tp, skb);

	req->class = &or_ipv6;
	ipv6_addr_copy(&req->af.v6_req.rmt_addr, &skb->nh.ipv6h->saddr);
	ipv6_addr_copy(&req->af.v6_req.loc_addr, &skb->nh.ipv6h->daddr);
	TCP_ECN_create_request(req, skb->h.th);
	req->af.v6_req.pktopts = NULL;
	if (ipv6_opt_accepted(sk, skb) ||
	    sk->net_pinfo.af_inet6.rxopt.bits.rxinfo ||
	    sk->net_pinfo.af_inet6.rxopt.bits.rxhlim) {
		atomic_inc(&skb->users);
		req->af.v6_req.pktopts = skb;
	}
	req->af.v6_req.iif = sk->bound_dev_if;

	/* So that link locals have meaning */
	if (!sk->bound_dev_if && ipv6_addr_type(&req->af.v6_req.rmt_addr)&IPV6_ADDR_LINKLOCAL)
		req->af.v6_req.iif = tcp_v6_iif(skb);

	if (isn == 0) 
		isn = tcp_v6_init_sequence(sk,skb);

	req->snt_isn = isn;

	if (tcp_v6_send_synack(sk, req, NULL))
		goto drop;

	tcp_v6_synq_add(sk, req);

	return 0;

drop:
	if (req)
		tcp_openreq_free(req);

	TCP_INC_STATS_BH(TcpAttemptFails);
	return 0; /* don't send reset */
}

static struct sock * tcp_v6_syn_recv_sock(struct sock *sk, struct sk_buff *skb,
					  struct open_request *req,
					  struct dst_entry *dst)
{
	struct ipv6_pinfo *np;
	struct flowi fl;
	struct tcp_opt *newtp;
	struct sock *newsk;
	struct ipv6_txoptions *opt;

	if (skb->protocol == htons(ETH_P_IP)) {
		/*
		 *	v6 mapped
		 */

		newsk = tcp_v4_syn_recv_sock(sk, skb, req, dst);

		if (newsk == NULL) 
			return NULL;

		np = &newsk->net_pinfo.af_inet6;

		ipv6_addr_set(&np->daddr, 0, 0, htonl(0x0000FFFF),
			      newsk->daddr);

		ipv6_addr_set(&np->saddr, 0, 0, htonl(0x0000FFFF),
			      newsk->saddr);

		ipv6_addr_copy(&np->rcv_saddr, &np->saddr);

		newsk->tp_pinfo.af_tcp.af_specific = &ipv6_mapped;
		newsk->backlog_rcv = tcp_v4_do_rcv;
		newsk->net_pinfo.af_inet6.pktoptions = NULL;
		newsk->net_pinfo.af_inet6.opt = NULL;
		newsk->net_pinfo.af_inet6.mcast_oif = tcp_v6_iif(skb);
		newsk->net_pinfo.af_inet6.mcast_hops = skb->nh.ipv6h->hop_limit;

		/* Charge newly allocated IPv6 socket. Though it is mapped,
		 * it is IPv6 yet.
		 */
#ifdef INET_REFCNT_DEBUG
		atomic_inc(&inet6_sock_nr);
#endif
		MOD_INC_USE_COUNT;

		/* It is tricky place. Until this moment IPv4 tcp
		   worked with IPv6 af_tcp.af_specific.
		   Sync it now.
		 */
		tcp_sync_mss(newsk, newsk->tp_pinfo.af_tcp.pmtu_cookie);

		return newsk;
	}

	opt = sk->net_pinfo.af_inet6.opt;

	if (tcp_acceptq_is_full(sk))
		goto out_overflow;

	if (sk->net_pinfo.af_inet6.rxopt.bits.srcrt == 2 &&
	    opt == NULL && req->af.v6_req.pktopts) {
		struct inet6_skb_parm *rxopt = (struct inet6_skb_parm *)req->af.v6_req.pktopts->cb;
		if (rxopt->srcrt)
			opt = ipv6_invert_rthdr(sk, (struct ipv6_rt_hdr*)(req->af.v6_req.pktopts->nh.raw+rxopt->srcrt));
	}

	if (dst == NULL) {
		fl.proto = IPPROTO_TCP;
		fl.nl_u.ip6_u.daddr = &req->af.v6_req.rmt_addr;
		if (opt && opt->srcrt) {
			struct rt0_hdr *rt0 = (struct rt0_hdr *) opt->srcrt;
			fl.nl_u.ip6_u.daddr = rt0->addr;
		}
		fl.nl_u.ip6_u.saddr = &req->af.v6_req.loc_addr;
		fl.fl6_flowlabel = 0;
		fl.oif = sk->bound_dev_if;
		fl.uli_u.ports.dport = req->rmt_port;
		fl.uli_u.ports.sport = sk->sport;

		dst = ip6_route_output(sk, &fl);
	}

	if (dst->error)
		goto out;

	newsk = tcp_create_openreq_child(sk, req, skb);
	if (newsk == NULL)
		goto out;

	/* Charge newly allocated IPv6 socket */
#ifdef INET_REFCNT_DEBUG
	atomic_inc(&inet6_sock_nr);
#endif
	MOD_INC_USE_COUNT;

	ip6_dst_store(newsk, dst, NULL);
	sk->route_caps = dst->dev->features&~NETIF_F_IP_CSUM;

	newtp = &(newsk->tp_pinfo.af_tcp);

	np = &newsk->net_pinfo.af_inet6;
	ipv6_addr_copy(&np->daddr, &req->af.v6_req.rmt_addr);
	ipv6_addr_copy(&np->saddr, &req->af.v6_req.loc_addr);
	ipv6_addr_copy(&np->rcv_saddr, &req->af.v6_req.loc_addr);
	newsk->bound_dev_if = req->af.v6_req.iif;

	/* Now IPv6 options... 

	   First: no IPv4 options.
	 */
	newsk->protinfo.af_inet.opt = NULL;

	/* Clone RX bits */
	np->rxopt.all = sk->net_pinfo.af_inet6.rxopt.all;

	/* Clone pktoptions received with SYN */
	np->pktoptions = NULL;
	if (req->af.v6_req.pktopts) {
		np->pktoptions = skb_clone(req->af.v6_req.pktopts, GFP_ATOMIC);
		kfree_skb(req->af.v6_req.pktopts);
		req->af.v6_req.pktopts = NULL;
		if (np->pktoptions)
			skb_set_owner_r(np->pktoptions, newsk);
	}
	np->opt = NULL;
	np->mcast_oif = tcp_v6_iif(skb);
	np->mcast_hops = skb->nh.ipv6h->hop_limit;

	/* Clone native IPv6 options from listening socket (if any)

	   Yes, keeping reference count would be much more clever,
	   but we make one more one thing there: reattach optmem
	   to newsk.
	 */
	if (opt) {
		np->opt = ipv6_dup_options(newsk, opt);
		if (opt != sk->net_pinfo.af_inet6.opt)
			sock_kfree_s(sk, opt, opt->tot_len);
	}

	newtp->ext_header_len = 0;
	if (np->opt)
		newtp->ext_header_len = np->opt->opt_nflen + np->opt->opt_flen;

	tcp_sync_mss(newsk, dst->pmtu);
	newtp->advmss = dst->advmss;
	tcp_initialize_rcv_mss(newsk);

	newsk->daddr	= LOOPBACK4_IPV6;
	newsk->saddr	= LOOPBACK4_IPV6;
	newsk->rcv_saddr= LOOPBACK4_IPV6;

	__tcp_v6_hash(newsk);
	tcp_inherit_port(sk, newsk);

	return newsk;

out_overflow:
	NET_INC_STATS_BH(ListenOverflows);
out:
	NET_INC_STATS_BH(ListenDrops);
	if (opt && opt != sk->net_pinfo.af_inet6.opt)
		sock_kfree_s(sk, opt, opt->tot_len);
	dst_release(dst);
	return NULL;
}

static int tcp_v6_checksum_init(struct sk_buff *skb)
{
	if (skb->ip_summed == CHECKSUM_HW) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		if (!tcp_v6_check(skb->h.th,skb->len,&skb->nh.ipv6h->saddr,
				  &skb->nh.ipv6h->daddr,skb->csum))
			return 0;
		NETDEBUG(if (net_ratelimit()) printk(KERN_DEBUG "hw tcp v6 csum failed\n"));
	}
	if (skb->len <= 76) {
		if (tcp_v6_check(skb->h.th,skb->len,&skb->nh.ipv6h->saddr,
				 &skb->nh.ipv6h->daddr,skb_checksum(skb, 0, skb->len, 0)))
			return -1;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else {
		skb->csum = ~tcp_v6_check(skb->h.th,skb->len,&skb->nh.ipv6h->saddr,
					  &skb->nh.ipv6h->daddr,0);
	}
	return 0;
}

/* The socket must have it's spinlock held when we get
 * here.
 *
 * We have a potential double-lock case here, so even when
 * doing backlog processing we use the BH locking scheme.
 * This is because we cannot sleep with the original spinlock
 * held.
 */
static int tcp_v6_do_rcv(struct sock *sk, struct sk_buff *skb)
{
	struct sk_buff *opt_skb = NULL;

	/* Imagine: socket is IPv6. IPv4 packet arrives,
	   goes to IPv4 receive handler and backlogged.
	   From backlog it always goes here. Kerboom...
	   Fortunately, tcp_rcv_established and rcv_established
	   handle them correctly, but it is not case with
	   tcp_v6_hnd_req and tcp_v6_send_reset().   --ANK
	 */

	if (skb->protocol == htons(ETH_P_IP))
		return tcp_v4_do_rcv(sk, skb);

	/*
	 *	socket locking is here for SMP purposes as backlog rcv
	 *	is currently called with bh processing disabled.
	 */

  	IP6_INC_STATS_BH(Ip6InDelivers);

	/* Do Stevens' IPV6_PKTOPTIONS.

	   Yes, guys, it is the only place in our code, where we
	   may make it not affecting IPv4.
	   The rest of code is protocol independent,
	   and I do not like idea to uglify IPv4.

	   Actually, all the idea behind IPV6_PKTOPTIONS
	   looks not very well thought. For now we latch
	   options, received in the last packet, enqueued
	   by tcp. Feel free to propose better solution.
	                                       --ANK (980728)
	 */
	if (sk->net_pinfo.af_inet6.rxopt.all)
		opt_skb = skb_clone(skb, GFP_ATOMIC);

	if (sk->state == TCP_ESTABLISHED) { /* Fast path */
		TCP_CHECK_TIMER(sk);
		if (tcp_rcv_established(sk, skb, skb->h.th, skb->len))
			goto reset;
		TCP_CHECK_TIMER(sk);
		if (opt_skb)
			goto ipv6_pktoptions;
		return 0;
	}

	if (skb->len < (skb->h.th->doff<<2) || tcp_checksum_complete(skb))
		goto csum_err;

	if (sk->state == TCP_LISTEN) { 
		struct sock *nsk = tcp_v6_hnd_req(sk, skb);
		if (!nsk)
			goto discard;

		/*
		 * Queue it on the new socket if the new socket is active,
		 * otherwise we just shortcircuit this and continue with
		 * the new socket..
		 */
 		if(nsk != sk) {
			if (tcp_child_process(sk, nsk, skb))
				goto reset;
			if (opt_skb)
				__kfree_skb(opt_skb);
			return 0;
		}
	}

	TCP_CHECK_TIMER(sk);
	if (tcp_rcv_state_process(sk, skb, skb->h.th, skb->len))
		goto reset;
	TCP_CHECK_TIMER(sk);
	if (opt_skb)
		goto ipv6_pktoptions;
	return 0;

reset:
	tcp_v6_send_reset(skb);
discard:
	if (opt_skb)
		__kfree_skb(opt_skb);
	kfree_skb(skb);
	return 0;
csum_err:
	TCP_INC_STATS_BH(TcpInErrs);
	goto discard;


ipv6_pktoptions:
	/* Do you ask, what is it?

	   1. skb was enqueued by tcp.
	   2. skb is added to tail of read queue, rather than out of order.
	   3. socket is not in passive state.
	   4. Finally, it really contains options, which user wants to receive.
	 */
	if (TCP_SKB_CB(opt_skb)->end_seq == sk->tp_pinfo.af_tcp.rcv_nxt &&
	    !((1<<sk->state)&(TCPF_CLOSE|TCPF_LISTEN))) {
		if (sk->net_pinfo.af_inet6.rxopt.bits.rxinfo)
			sk->net_pinfo.af_inet6.mcast_oif = tcp_v6_iif(opt_skb);
		if (sk->net_pinfo.af_inet6.rxopt.bits.rxhlim)
			sk->net_pinfo.af_inet6.mcast_hops = opt_skb->nh.ipv6h->hop_limit;
		if (ipv6_opt_accepted(sk, opt_skb)) {
			skb_set_owner_r(opt_skb, sk);
			opt_skb = xchg(&sk->net_pinfo.af_inet6.pktoptions, opt_skb);
		} else {
			__kfree_skb(opt_skb);
			opt_skb = xchg(&sk->net_pinfo.af_inet6.pktoptions, NULL);
		}
	}

	if (opt_skb)
		kfree_skb(opt_skb);
	return 0;
}

int tcp_v6_rcv(struct sk_buff *skb)
{
	struct tcphdr *th;	
	struct sock *sk;
	int ret;

	if (skb->pkt_type != PACKET_HOST)
		goto discard_it;

	/*
	 *	Count it even if it's bad.
	 */
	TCP_INC_STATS_BH(TcpInSegs);

	if (!pskb_may_pull(skb, sizeof(struct tcphdr)))
		goto discard_it;

	th = skb->h.th;

	if (th->doff < sizeof(struct tcphdr)/4)
		goto bad_packet;
	if (!pskb_may_pull(skb, th->doff*4))
		goto discard_it;

	if ((skb->ip_summed != CHECKSUM_UNNECESSARY &&
	     tcp_v6_checksum_init(skb) < 0))
		goto bad_packet;

	th = skb->h.th;
	TCP_SKB_CB(skb)->seq = ntohl(th->seq);
	TCP_SKB_CB(skb)->end_seq = (TCP_SKB_CB(skb)->seq + th->syn + th->fin +
				    skb->len - th->doff*4);
	TCP_SKB_CB(skb)->ack_seq = ntohl(th->ack_seq);
	TCP_SKB_CB(skb)->when = 0;
	TCP_SKB_CB(skb)->flags = ip6_get_dsfield(skb->nh.ipv6h);
	TCP_SKB_CB(skb)->sacked = 0;

	sk = __tcp_v6_lookup(&skb->nh.ipv6h->saddr, th->source,
			     &skb->nh.ipv6h->daddr, ntohs(th->dest), tcp_v6_iif(skb));

	if (!sk)
		goto no_tcp_socket;

process:
	if(!ipsec_sk_policy(sk,skb))
		goto discard_and_relse;
	if(sk->state == TCP_TIME_WAIT)
		goto do_time_wait;

	if (sk_filter(sk, skb, 0))
		goto discard_and_relse;
		
	skb->dev = NULL;

	bh_lock_sock(sk);
	ret = 0;
	if (!sk->lock.users) {
		if (!tcp_prequeue(sk, skb))
			ret = tcp_v6_do_rcv(sk, skb);
	} else
		sk_add_backlog(sk, skb);
	bh_unlock_sock(sk);

	sock_put(sk);
	return ret;

no_tcp_socket:
	if (skb->len < (th->doff<<2) || tcp_checksum_complete(skb)) {
bad_packet:
		TCP_INC_STATS_BH(TcpInErrs);
	} else {
		tcp_v6_send_reset(skb);
	}

discard_it:

	/*
	 *	Discard frame
	 */

	kfree_skb(skb);
	return 0;

discard_and_relse:
	sock_put(sk);
	goto discard_it;

do_time_wait:
	if (skb->len < (th->doff<<2) || tcp_checksum_complete(skb)) {
		TCP_INC_STATS_BH(TcpInErrs);
		tcp_tw_put((struct tcp_tw_bucket *) sk);	
		goto discard_it;
	}

	switch(tcp_timewait_state_process((struct tcp_tw_bucket *)sk,
					  skb, th, skb->len)) {
	case TCP_TW_SYN:
	{
		struct sock *sk2;

		sk2 = tcp_v6_lookup_listener(&skb->nh.ipv6h->daddr, ntohs(th->dest), tcp_v6_iif(skb));
		if (sk2 != NULL) {
			tcp_tw_deschedule((struct tcp_tw_bucket *)sk);
			tcp_timewait_kill((struct tcp_tw_bucket *)sk);
			tcp_tw_put((struct tcp_tw_bucket *)sk);
			sk = sk2;
			goto process;
		}
		/* Fall through to ACK */
	}
	case TCP_TW_ACK:
		tcp_v6_timewait_ack(sk, skb);
		break;
	case TCP_TW_RST:
		goto no_tcp_socket;
	case TCP_TW_SUCCESS:;
	}
	goto discard_it;
}

static int tcp_v6_rebuild_header(struct sock *sk)
{
	int err;
	struct dst_entry *dst;
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;

	dst = __sk_dst_check(sk, np->dst_cookie);

	if (dst == NULL) {
		struct flowi fl;

		fl.proto = IPPROTO_TCP;
		fl.nl_u.ip6_u.daddr = &np->daddr;
		fl.nl_u.ip6_u.saddr = &np->saddr;
		fl.fl6_flowlabel = np->flow_label;
		fl.oif = sk->bound_dev_if;
		fl.uli_u.ports.dport = sk->dport;
		fl.uli_u.ports.sport = sk->sport;

		if (np->opt && np->opt->srcrt) {
			struct rt0_hdr *rt0 = (struct rt0_hdr *) np->opt->srcrt;
			fl.nl_u.ip6_u.daddr = rt0->addr;
		}

		dst = ip6_route_output(sk, &fl);

		if (dst->error) {
			err = dst->error;
			dst_release(dst);
			sk->route_caps = 0;
			return err;
		}

		ip6_dst_store(sk, dst, NULL);
		sk->route_caps = dst->dev->features&~NETIF_F_IP_CSUM;
	}

	return 0;
}

static int tcp_v6_xmit(struct sk_buff *skb, int ipfragok)
{
	struct sock *sk = skb->sk;
	struct ipv6_pinfo * np = &sk->net_pinfo.af_inet6;
	struct flowi fl;
	struct dst_entry *dst;

	fl.proto = IPPROTO_TCP;
	fl.fl6_dst = &np->daddr;
	fl.fl6_src = &np->saddr;
	fl.fl6_flowlabel = np->flow_label;
	IP6_ECN_flow_xmit(sk, fl.fl6_flowlabel);
	fl.oif = sk->bound_dev_if;
	fl.uli_u.ports.sport = sk->sport;
	fl.uli_u.ports.dport = sk->dport;

	if (np->opt && np->opt->srcrt) {
		struct rt0_hdr *rt0 = (struct rt0_hdr *) np->opt->srcrt;
		fl.nl_u.ip6_u.daddr = rt0->addr;
	}

	dst = __sk_dst_check(sk, np->dst_cookie);

	if (dst == NULL) {
		dst = ip6_route_output(sk, &fl);

		if (dst->error) {
			sk->err_soft = -dst->error;
			dst_release(dst);
			return -sk->err_soft;
		}

		ip6_dst_store(sk, dst, NULL);
	}

	skb->dst = dst_clone(dst);

	/* Restore final destination back after routing done */
	fl.nl_u.ip6_u.daddr = &np->daddr;

	return ip6_xmit(sk, skb, &fl, np->opt);
}

static void v6_addr2sockaddr(struct sock *sk, struct sockaddr * uaddr)
{
	struct ipv6_pinfo * np = &sk->net_pinfo.af_inet6;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) uaddr;

	sin6->sin6_family = AF_INET6;
	memcpy(&sin6->sin6_addr, &np->daddr, sizeof(struct in6_addr));
	sin6->sin6_port	= sk->dport;
	/* We do not store received flowlabel for TCP */
	sin6->sin6_flowinfo = 0;
	sin6->sin6_scope_id = 0;
	if (sk->bound_dev_if && ipv6_addr_type(&sin6->sin6_addr)&IPV6_ADDR_LINKLOCAL)
		sin6->sin6_scope_id = sk->bound_dev_if;
}

static int tcp_v6_remember_stamp(struct sock *sk)
{
	/* Alas, not yet... */
	return 0;
}

static struct tcp_func ipv6_specific = {
	tcp_v6_xmit,
	tcp_v6_send_check,
	tcp_v6_rebuild_header,
	tcp_v6_conn_request,
	tcp_v6_syn_recv_sock,
	tcp_v6_remember_stamp,
	sizeof(struct ipv6hdr),

	ipv6_setsockopt,
	ipv6_getsockopt,
	v6_addr2sockaddr,
	sizeof(struct sockaddr_in6)
};

/*
 *	TCP over IPv4 via INET6 API
 */

static struct tcp_func ipv6_mapped = {
	ip_queue_xmit,
	tcp_v4_send_check,
	tcp_v4_rebuild_header,
	tcp_v6_conn_request,
	tcp_v6_syn_recv_sock,
	tcp_v4_remember_stamp,
	sizeof(struct iphdr),

	ipv6_setsockopt,
	ipv6_getsockopt,
	v6_addr2sockaddr,
	sizeof(struct sockaddr_in6)
};



/* NOTE: A lot of things set to zero explicitly by call to
 *       sk_alloc() so need not be done here.
 */
static int tcp_v6_init_sock(struct sock *sk)
{
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);

	skb_queue_head_init(&tp->out_of_order_queue);
	tcp_init_xmit_timers(sk);
	tcp_prequeue_init(tp);

	tp->rto  = TCP_TIMEOUT_INIT;
	tp->mdev = TCP_TIMEOUT_INIT;

	/* So many TCP implementations out there (incorrectly) count the
	 * initial SYN frame in their delayed-ACK and congestion control
	 * algorithms that we must have the following bandaid to talk
	 * efficiently to them.  -DaveM
	 */
	tp->snd_cwnd = 2;

	/* See draft-stevens-tcpca-spec-01 for discussion of the
	 * initialization of these values.
	 */
	tp->snd_ssthresh = 0x7fffffff;
	tp->snd_cwnd_clamp = ~0;
	tp->mss_cache = 536;

	tp->reordering = sysctl_tcp_reordering;

	sk->state = TCP_CLOSE;

	sk->tp_pinfo.af_tcp.af_specific = &ipv6_specific;

	sk->write_space = tcp_write_space;
	sk->use_write_queue = 1;

	sk->sndbuf = sysctl_tcp_wmem[1];
	sk->rcvbuf = sysctl_tcp_rmem[1];

	atomic_inc(&tcp_sockets_allocated);

	return 0;
}

static int tcp_v6_destroy_sock(struct sock *sk)
{
	struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);

	tcp_clear_xmit_timers(sk);

	/* Cleanup up the write buffer. */
  	tcp_writequeue_purge(sk);

	/* Cleans up our, hopefully empty, out_of_order_queue. */
  	__skb_queue_purge(&tp->out_of_order_queue);

	/* Clean prequeue, it must be empty really */
	__skb_queue_purge(&tp->ucopy.prequeue);

	/* Clean up a referenced TCP bind bucket. */
	if(sk->prev != NULL)
		tcp_put_port(sk);

	/* If sendmsg cached page exists, toss it. */
	if (tp->sndmsg_page != NULL)
		__free_page(tp->sndmsg_page);

	atomic_dec(&tcp_sockets_allocated);

	return inet6_destroy_sock(sk);
}

/* Proc filesystem TCPv6 sock list dumping. */
static void get_openreq6(struct sock *sk, struct open_request *req, char *tmpbuf, int i, int uid)
{
	struct in6_addr *dest, *src;
	int ttd = req->expires - jiffies;

	if (ttd < 0)
		ttd = 0;

	src = &req->af.v6_req.loc_addr;
	dest = &req->af.v6_req.rmt_addr;
	sprintf(tmpbuf,
		"%4d: %08X%08X%08X%08X:%04X %08X%08X%08X%08X:%04X "
		"%02X %08X:%08X %02X:%08X %08X %5d %8d %d %d %p",
		i,
		src->s6_addr32[0], src->s6_addr32[1],
		src->s6_addr32[2], src->s6_addr32[3],
		ntohs(sk->sport),
		dest->s6_addr32[0], dest->s6_addr32[1],
		dest->s6_addr32[2], dest->s6_addr32[3],
		ntohs(req->rmt_port),
		TCP_SYN_RECV,
		0,0, /* could print option size, but that is af dependent. */
		1,   /* timers active (only the expire timer) */  
		ttd, 
		req->retrans,
		uid,
		0,  /* non standard timer */  
		0, /* open_requests have no inode */
		0, req);
}

static void get_tcp6_sock(struct sock *sp, char *tmpbuf, int i)
{
	struct in6_addr *dest, *src;
	__u16 destp, srcp;
	int timer_active;
	unsigned long timer_expires;
	struct tcp_opt *tp = &sp->tp_pinfo.af_tcp;

	dest  = &sp->net_pinfo.af_inet6.daddr;
	src   = &sp->net_pinfo.af_inet6.rcv_saddr;
	destp = ntohs(sp->dport);
	srcp  = ntohs(sp->sport);
	if (tp->pending == TCP_TIME_RETRANS) {
		timer_active	= 1;
		timer_expires	= tp->timeout;
	} else if (tp->pending == TCP_TIME_PROBE0) {
		timer_active	= 4;
		timer_expires	= tp->timeout;
	} else if (timer_pending(&sp->timer)) {
		timer_active	= 2;
		timer_expires	= sp->timer.expires;
	} else {
		timer_active	= 0;
		timer_expires = jiffies;
	}

	sprintf(tmpbuf,
		"%4d: %08X%08X%08X%08X:%04X %08X%08X%08X%08X:%04X "
		"%02X %08X:%08X %02X:%08lX %08X %5d %8d %lu %d %p %u %u %u %u %d",
		i,
		src->s6_addr32[0], src->s6_addr32[1],
		src->s6_addr32[2], src->s6_addr32[3], srcp,
		dest->s6_addr32[0], dest->s6_addr32[1],
		dest->s6_addr32[2], dest->s6_addr32[3], destp,
		sp->state, 
		tp->write_seq-tp->snd_una, tp->rcv_nxt-tp->copied_seq,
		timer_active, timer_expires-jiffies,
		tp->retransmits,
		sock_i_uid(sp),
		tp->probes_out,
		sock_i_ino(sp),
		atomic_read(&sp->refcnt), sp,
		tp->rto, tp->ack.ato, (tp->ack.quick<<1)|tp->ack.pingpong,
		tp->snd_cwnd, tp->snd_ssthresh>=0xFFFF?-1:tp->snd_ssthresh
		);
}

static void get_timewait6_sock(struct tcp_tw_bucket *tw, char *tmpbuf, int i)
{
	struct in6_addr *dest, *src;
	__u16 destp, srcp;
	int ttd = tw->ttd - jiffies;

	if (ttd < 0)
		ttd = 0;

	dest  = &tw->v6_daddr;
	src   = &tw->v6_rcv_saddr;
	destp = ntohs(tw->dport);
	srcp  = ntohs(tw->sport);

	sprintf(tmpbuf,
		"%4d: %08X%08X%08X%08X:%04X %08X%08X%08X%08X:%04X "
		"%02X %08X:%08X %02X:%08X %08X %5d %8d %d %d %p",
		i,
		src->s6_addr32[0], src->s6_addr32[1],
		src->s6_addr32[2], src->s6_addr32[3], srcp,
		dest->s6_addr32[0], dest->s6_addr32[1],
		dest->s6_addr32[2], dest->s6_addr32[3], destp,
		tw->substate, 0, 0,
		3, ttd, 0, 0, 0, 0,
		atomic_read(&tw->refcnt), tw);
}

#define LINE_LEN 190
#define LINE_FMT "%-190s\n"

int tcp6_get_info(char *buffer, char **start, off_t offset, int length)
{
	int len = 0, num = 0, i;
	off_t begin, pos = 0;
	char tmpbuf[LINE_LEN+2];

	if (offset < LINE_LEN+1)
		len += sprintf(buffer, LINE_FMT,
			       "  sl  "						/* 6 */
			       "local_address                         "		/* 38 */
			       "remote_address                        "		/* 38 */
			       "st tx_queue rx_queue tr tm->when retrnsmt"	/* 41 */
			       "   uid  timeout inode");			/* 21 */
										/*----*/
										/*144 */

	pos = LINE_LEN+1;

	/* First, walk listening socket table. */
	tcp_listen_lock();
	for(i = 0; i < TCP_LHTABLE_SIZE; i++) {
		struct sock *sk = tcp_listening_hash[i];
		struct tcp_listen_opt *lopt;
		int k;

		for (sk = tcp_listening_hash[i]; sk; sk = sk->next, num++) {
			struct open_request *req;
			int uid;
			struct tcp_opt *tp = &(sk->tp_pinfo.af_tcp);

			if (sk->family != PF_INET6)
				continue;
			pos += LINE_LEN+1;
			if (pos >= offset) {
				get_tcp6_sock(sk, tmpbuf, num);
				len += sprintf(buffer+len, LINE_FMT, tmpbuf);
				if (pos >= offset + length) {
					tcp_listen_unlock();
					goto out_no_bh;
				}
			}

			uid = sock_i_uid(sk);
			read_lock_bh(&tp->syn_wait_lock);
			lopt = tp->listen_opt;
			if (lopt && lopt->qlen != 0) {
				for (k=0; k<TCP_SYNQ_HSIZE; k++) {
					for (req = lopt->syn_table[k]; req; req = req->dl_next, num++) {
						if (req->class->family != PF_INET6)
							continue;
						pos += LINE_LEN+1;
						if (pos <= offset)
							continue;
						get_openreq6(sk, req, tmpbuf, num, uid);
						len += sprintf(buffer+len, LINE_FMT, tmpbuf);
						if (pos >= offset + length) { 
							read_unlock_bh(&tp->syn_wait_lock);
							tcp_listen_unlock();
							goto out_no_bh;
						}
					}
				}
			}
			read_unlock_bh(&tp->syn_wait_lock);

			/* Completed requests are in normal socket hash table */
		}
	}
	tcp_listen_unlock();

	local_bh_disable();

	/* Next, walk established hash chain. */
	for (i = 0; i < tcp_ehash_size; i++) {
		struct tcp_ehash_bucket *head = &tcp_ehash[i];
		struct sock *sk;
		struct tcp_tw_bucket *tw;

		read_lock(&head->lock);
		for(sk = head->chain; sk; sk = sk->next, num++) {
			if (sk->family != PF_INET6)
				continue;
			pos += LINE_LEN+1;
			if (pos <= offset)
				continue;
			get_tcp6_sock(sk, tmpbuf, num);
			len += sprintf(buffer+len, LINE_FMT, tmpbuf);
			if (pos >= offset + length) {
				read_unlock(&head->lock);
				goto out;
			}
		}
		for (tw = (struct tcp_tw_bucket *)tcp_ehash[i+tcp_ehash_size].chain;
		     tw != NULL;
		     tw = (struct tcp_tw_bucket *)tw->next, num++) {
			if (tw->family != PF_INET6)
				continue;
			pos += LINE_LEN+1;
			if (pos <= offset)
				continue;
			get_timewait6_sock(tw, tmpbuf, num);
			len += sprintf(buffer+len, LINE_FMT, tmpbuf);
			if (pos >= offset + length) {
				read_unlock(&head->lock);
				goto out;
			}
		}
		read_unlock(&head->lock);
	}

out:
	local_bh_enable();
out_no_bh:

	begin = len - (pos - offset);
	*start = buffer + begin;
	len -= begin;
	if (len > length)
		len = length;
	if (len < 0)
		len = 0; 
	return len;
}

struct proto tcpv6_prot = {
	name:		"TCPv6",
	close:		tcp_close,
	connect:	tcp_v6_connect,
	disconnect:	tcp_disconnect,
	accept:		tcp_accept,
	ioctl:		tcp_ioctl,
	init:		tcp_v6_init_sock,
	destroy:	tcp_v6_destroy_sock,
	shutdown:	tcp_shutdown,
	setsockopt:	tcp_setsockopt,
	getsockopt:	tcp_getsockopt,
	sendmsg:	tcp_sendmsg,
	recvmsg:	tcp_recvmsg,
	backlog_rcv:	tcp_v6_do_rcv,
	hash:		tcp_v6_hash,
	unhash:		tcp_unhash,
	get_port:	tcp_v6_get_port,
};

static struct inet6_protocol tcpv6_protocol =
{
	tcp_v6_rcv,		/* TCP handler		*/
	tcp_v6_err,		/* TCP error control	*/
	NULL,			/* next			*/
	IPPROTO_TCP,		/* protocol ID		*/
	0,			/* copy			*/
	NULL,			/* data			*/
	"TCPv6"			/* name			*/
};

extern struct proto_ops inet6_stream_ops;

static struct inet_protosw tcpv6_protosw = {
	type:        SOCK_STREAM,
	protocol:    IPPROTO_TCP,
	prot:        &tcpv6_prot,
	ops:         &inet6_stream_ops,
	capability:  -1,
	no_check:    0,
	flags:       INET_PROTOSW_PERMANENT,
};

void __init tcpv6_init(void)
{
	/* register inet6 protocol */
	inet6_add_protocol(&tcpv6_protocol);
	inet6_register_protosw(&tcpv6_protosw);
}
