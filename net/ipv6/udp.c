/*
 *	UDP over IPv6
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	Based on linux/ipv4/udp.c
 *
 *	$Id: udp.c,v 1.64.2.1 2002/03/05 12:47:34 davem Exp $
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

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/init.h>
#include <asm/uaccess.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/ndisc.h>
#include <net/protocol.h>
#include <net/transp_v6.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/inet_common.h>

#include <net/checksum.h>

struct udp_mib udp_stats_in6[NR_CPUS*2];

/* Grrr, addr_type already calculated by caller, but I don't want
 * to add some silly "cookie" argument to this method just for that.
 */
static int udp_v6_get_port(struct sock *sk, unsigned short snum)
{
	write_lock_bh(&udp_hash_lock);
	if (snum == 0) {
		int best_size_so_far, best, result, i;

		if (udp_port_rover > sysctl_local_port_range[1] ||
		    udp_port_rover < sysctl_local_port_range[0])
			udp_port_rover = sysctl_local_port_range[0];
		best_size_so_far = 32767;
		best = result = udp_port_rover;
		for (i = 0; i < UDP_HTABLE_SIZE; i++, result++) {
			struct sock *sk;
			int size;

			sk = udp_hash[result & (UDP_HTABLE_SIZE - 1)];
			if (!sk) {
				if (result > sysctl_local_port_range[1])
					result = sysctl_local_port_range[0] +
						((result - sysctl_local_port_range[0]) &
						 (UDP_HTABLE_SIZE - 1));
				goto gotit;
			}
			size = 0;
			do {
				if (++size >= best_size_so_far)
					goto next;
			} while ((sk = sk->next) != NULL);
			best_size_so_far = size;
			best = result;
		next:;
		}
		result = best;
		for(;; result += UDP_HTABLE_SIZE) {
			if (result > sysctl_local_port_range[1])
				result = sysctl_local_port_range[0]
					+ ((result - sysctl_local_port_range[0]) &
					   (UDP_HTABLE_SIZE - 1));
			if (!udp_lport_inuse(result))
				break;
		}
gotit:
		udp_port_rover = snum = result;
	} else {
		struct sock *sk2;
		int addr_type = ipv6_addr_type(&sk->net_pinfo.af_inet6.rcv_saddr);

		for (sk2 = udp_hash[snum & (UDP_HTABLE_SIZE - 1)];
		     sk2 != NULL;
		     sk2 = sk2->next) {
			if (sk2->num == snum &&
			    sk2 != sk &&
			    (!sk2->bound_dev_if ||
			     !sk->bound_dev_if ||
			     sk2->bound_dev_if == sk->bound_dev_if) &&
			    ((!sk2->rcv_saddr && !ipv6_only_sock(sk)) ||
			     (sk2->family == AF_INET6 && 
			      ipv6_addr_any(&sk2->net_pinfo.af_inet6.rcv_saddr) &&
			      !(ipv6_only_sock(sk2) && addr_type == IPV6_ADDR_MAPPED)) ||
			     (addr_type == IPV6_ADDR_ANY && 
			      (!ipv6_only_sock(sk) || 
			       !(sk2->family == AF_INET6 ? (ipv6_addr_type(&sk2->net_pinfo.af_inet6.rcv_saddr) == IPV6_ADDR_MAPPED) : 1))) ||
			     (sk2->family == AF_INET6 && 
			      !ipv6_addr_cmp(&sk->net_pinfo.af_inet6.rcv_saddr,
					     &sk2->net_pinfo.af_inet6.rcv_saddr)) ||
			     (addr_type == IPV6_ADDR_MAPPED &&
			      !ipv6_only_sock(sk2) &&
			      (!sk2->rcv_saddr || 
			       !sk->rcv_saddr ||
			       sk->rcv_saddr == sk2->rcv_saddr))) &&
			    (!sk2->reuse || !sk->reuse))
				goto fail;
		}
	}

	sk->num = snum;
	if (sk->pprev == NULL) {
		struct sock **skp = &udp_hash[snum & (UDP_HTABLE_SIZE - 1)];
		if ((sk->next = *skp) != NULL)
			(*skp)->pprev = &sk->next;
		*skp = sk;
		sk->pprev = skp;
		sock_prot_inc_use(sk->prot);
		sock_hold(sk);
	}
	write_unlock_bh(&udp_hash_lock);
	return 0;

fail:
	write_unlock_bh(&udp_hash_lock);
	return 1;
}

static void udp_v6_hash(struct sock *sk)
{
	BUG();
}

static void udp_v6_unhash(struct sock *sk)
{
 	write_lock_bh(&udp_hash_lock);
	if (sk->pprev) {
		if (sk->next)
			sk->next->pprev = sk->pprev;
		*sk->pprev = sk->next;
		sk->pprev = NULL;
		sk->num = 0;
		sock_prot_dec_use(sk->prot);
		__sock_put(sk);
	}
	write_unlock_bh(&udp_hash_lock);
}

static struct sock *udp_v6_lookup(struct in6_addr *saddr, u16 sport,
				  struct in6_addr *daddr, u16 dport, int dif)
{
	struct sock *sk, *result = NULL;
	unsigned short hnum = ntohs(dport);
	int badness = -1;

 	read_lock(&udp_hash_lock);
	for(sk = udp_hash[hnum & (UDP_HTABLE_SIZE - 1)]; sk != NULL; sk = sk->next) {
		if((sk->num == hnum)		&&
		   (sk->family == PF_INET6)) {
			struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
			int score = 0;
			if(sk->dport) {
				if(sk->dport != sport)
					continue;
				score++;
			}
			if(!ipv6_addr_any(&np->rcv_saddr)) {
				if(ipv6_addr_cmp(&np->rcv_saddr, daddr))
					continue;
				score++;
			}
			if(!ipv6_addr_any(&np->daddr)) {
				if(ipv6_addr_cmp(&np->daddr, saddr))
					continue;
				score++;
			}
			if(sk->bound_dev_if) {
				if(sk->bound_dev_if != dif)
					continue;
				score++;
			}
			if(score == 4) {
				result = sk;
				break;
			} else if(score > badness) {
				result = sk;
				badness = score;
			}
		}
	}
	if (result)
		sock_hold(result);
 	read_unlock(&udp_hash_lock);
	return result;
}

/*
 *
 */

int udpv6_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	struct sockaddr_in6	*usin = (struct sockaddr_in6 *) uaddr;
	struct ipv6_pinfo      	*np = &sk->net_pinfo.af_inet6;
	struct in6_addr		*daddr;
	struct in6_addr		saddr;
	struct dst_entry	*dst;
	struct flowi		fl;
	struct ip6_flowlabel	*flowlabel = NULL;
	int			addr_type;
	int			err;

	if (usin->sin6_family == AF_INET) {
		if (__ipv6_only_sock(sk))
			return -EAFNOSUPPORT;
		err = udp_connect(sk, uaddr, addr_len);
		goto ipv4_connected;
	}

	if (addr_len < SIN6_LEN_RFC2133)
	  	return -EINVAL;

	if (usin->sin6_family != AF_INET6) 
	  	return -EAFNOSUPPORT;

	fl.fl6_flowlabel = 0;
	if (np->sndflow) {
		fl.fl6_flowlabel = usin->sin6_flowinfo&IPV6_FLOWINFO_MASK;
		if (fl.fl6_flowlabel&IPV6_FLOWLABEL_MASK) {
			flowlabel = fl6_sock_lookup(sk, fl.fl6_flowlabel);
			if (flowlabel == NULL)
				return -EINVAL;
			ipv6_addr_copy(&usin->sin6_addr, &flowlabel->dst);
		}
	}

	addr_type = ipv6_addr_type(&usin->sin6_addr);

	if (addr_type == IPV6_ADDR_ANY) {
		/*
		 *	connect to self
		 */
		usin->sin6_addr.s6_addr[15] = 0x01;
	}

	daddr = &usin->sin6_addr;

	if (addr_type == IPV6_ADDR_MAPPED) {
		struct sockaddr_in sin;

		if (__ipv6_only_sock(sk))
			return -ENETUNREACH;

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = daddr->s6_addr32[3];
		sin.sin_port = usin->sin6_port;

		err = udp_connect(sk, (struct sockaddr*) &sin, sizeof(sin));

ipv4_connected:
		if (err < 0)
			return err;
		
		ipv6_addr_set(&np->daddr, 0, 0, 
			      htonl(0x0000ffff),
			      sk->daddr);

		if(ipv6_addr_any(&np->saddr)) {
			ipv6_addr_set(&np->saddr, 0, 0, 
				      htonl(0x0000ffff),
				      sk->saddr);
		}

		if(ipv6_addr_any(&np->rcv_saddr)) {
			ipv6_addr_set(&np->rcv_saddr, 0, 0, 
				      htonl(0x0000ffff),
				      sk->rcv_saddr);
		}
		return 0;
	}

	if (addr_type&IPV6_ADDR_LINKLOCAL) {
		if (addr_len >= sizeof(struct sockaddr_in6) &&
		    usin->sin6_scope_id) {
			if (sk->bound_dev_if && sk->bound_dev_if != usin->sin6_scope_id) {
				fl6_sock_release(flowlabel);
				return -EINVAL;
			}
			sk->bound_dev_if = usin->sin6_scope_id;
			if (!sk->bound_dev_if && (addr_type&IPV6_ADDR_MULTICAST))
				fl.oif = np->mcast_oif;
		}

		/* Connect to link-local address requires an interface */
		if (sk->bound_dev_if == 0)
			return -EINVAL;
	}

	ipv6_addr_copy(&np->daddr, daddr);
	np->flow_label = fl.fl6_flowlabel;

	sk->dport = usin->sin6_port;

	/*
	 *	Check for a route to destination an obtain the
	 *	destination cache for it.
	 */

	fl.proto = IPPROTO_UDP;
	fl.fl6_dst = &np->daddr;
	fl.fl6_src = &saddr;
	fl.oif = sk->bound_dev_if;
	fl.uli_u.ports.dport = sk->dport;
	fl.uli_u.ports.sport = sk->sport;

	if (!fl.oif && (addr_type&IPV6_ADDR_MULTICAST))
		fl.oif = np->mcast_oif;

	if (flowlabel) {
		if (flowlabel->opt && flowlabel->opt->srcrt) {
			struct rt0_hdr *rt0 = (struct rt0_hdr *) flowlabel->opt->srcrt;
			fl.fl6_dst = rt0->addr;
		}
	} else if (np->opt && np->opt->srcrt) {
		struct rt0_hdr *rt0 = (struct rt0_hdr *) np->opt->srcrt;
		fl.fl6_dst = rt0->addr;
	}

	dst = ip6_route_output(sk, &fl);

	if ((err = dst->error) != 0) {
		dst_release(dst);
		fl6_sock_release(flowlabel);
		return err;
	}

	ip6_dst_store(sk, dst, fl.fl6_dst);

	/* get the source adddress used in the apropriate device */

	err = ipv6_get_saddr(dst, daddr, &saddr);

	if (err == 0) {
		if(ipv6_addr_any(&np->saddr))
			ipv6_addr_copy(&np->saddr, &saddr);

		if(ipv6_addr_any(&np->rcv_saddr)) {
			ipv6_addr_copy(&np->rcv_saddr, &saddr);
			sk->rcv_saddr = LOOPBACK4_IPV6;
		}
		sk->state = TCP_ESTABLISHED;
	}
	fl6_sock_release(flowlabel);

	return err;
}

static void udpv6_close(struct sock *sk, long timeout)
{
	inet_sock_release(sk);
}

/*
 * 	This should be easy, if there is something there we
 * 	return it, otherwise we block.
 */

int udpv6_recvmsg(struct sock *sk, struct msghdr *msg, int len,
		  int noblock, int flags, int *addr_len)
{
  	struct sk_buff *skb;
  	int copied, err;

  	if (addr_len)
  		*addr_len=sizeof(struct sockaddr_in6);
  
	if (flags & MSG_ERRQUEUE)
		return ipv6_recv_error(sk, msg, len);

try_again:
	skb = skb_recv_datagram(sk, flags, noblock, &err);
	if (!skb)
		goto out;

 	copied = skb->len - sizeof(struct udphdr);
  	if (copied > len) {
  		copied = len;
  		msg->msg_flags |= MSG_TRUNC;
  	}

	if (skb->ip_summed==CHECKSUM_UNNECESSARY) {
		err = skb_copy_datagram_iovec(skb, sizeof(struct udphdr), msg->msg_iov,
					      copied);
	} else if (msg->msg_flags&MSG_TRUNC) {
		if ((unsigned short)csum_fold(skb_checksum(skb, 0, skb->len, skb->csum)))
			goto csum_copy_err;
		err = skb_copy_datagram_iovec(skb, sizeof(struct udphdr), msg->msg_iov,
					      copied);
	} else {
		err = skb_copy_and_csum_datagram_iovec(skb, sizeof(struct udphdr), msg->msg_iov);
		if (err == -EINVAL)
			goto csum_copy_err;
	}
	if (err)
		goto out_free;

	sock_recv_timestamp(msg, sk, skb);

	/* Copy the address. */
	if (msg->msg_name) {
		struct sockaddr_in6 *sin6;
	  
		sin6 = (struct sockaddr_in6 *) msg->msg_name;
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = skb->h.uh->source;
		sin6->sin6_flowinfo = 0;
		sin6->sin6_scope_id = 0;

		if (skb->protocol == htons(ETH_P_IP)) {
			ipv6_addr_set(&sin6->sin6_addr, 0, 0,
				      htonl(0xffff), skb->nh.iph->saddr);
			if (sk->protinfo.af_inet.cmsg_flags)
				ip_cmsg_recv(msg, skb);
		} else {
			memcpy(&sin6->sin6_addr, &skb->nh.ipv6h->saddr,
			       sizeof(struct in6_addr));

			if (sk->net_pinfo.af_inet6.rxopt.all)
				datagram_recv_ctl(sk, msg, skb);
			if (ipv6_addr_type(&sin6->sin6_addr) & IPV6_ADDR_LINKLOCAL) {
				struct inet6_skb_parm *opt = (struct inet6_skb_parm *) skb->cb;
				sin6->sin6_scope_id = opt->iif;
			}
		}
  	}
	err = copied;

out_free:
	skb_free_datagram(sk, skb);
out:
	return err;

csum_copy_err:
	/* Clear queue. */
	if (flags&MSG_PEEK) {
		int clear = 0;
		spin_lock_irq(&sk->receive_queue.lock);
		if (skb == skb_peek(&sk->receive_queue)) {
			__skb_unlink(skb, &sk->receive_queue);
			clear = 1;
		}
		spin_unlock_irq(&sk->receive_queue.lock);
		if (clear)
			kfree_skb(skb);
	}

	skb_free_datagram(sk, skb);

	if (flags & MSG_DONTWAIT) {
		UDP6_INC_STATS_USER(UdpInErrors);
		return -EAGAIN;
	}
	goto try_again;
}

void udpv6_err(struct sk_buff *skb, struct inet6_skb_parm *opt,
	       int type, int code, int offset, __u32 info)
{
	struct ipv6hdr *hdr = (struct ipv6hdr*)skb->data;
	struct net_device *dev = skb->dev;
	struct in6_addr *saddr = &hdr->saddr;
	struct in6_addr *daddr = &hdr->daddr;
	struct udphdr *uh = (struct udphdr*)(skb->data+offset);
	struct sock *sk;
	int err;

	sk = udp_v6_lookup(daddr, uh->dest, saddr, uh->source, dev->ifindex);
   
	if (sk == NULL)
		return;

	if (!icmpv6_err_convert(type, code, &err) &&
	    !sk->net_pinfo.af_inet6.recverr)
		goto out;

	if (sk->state!=TCP_ESTABLISHED &&
	    !sk->net_pinfo.af_inet6.recverr)
		goto out;

	if (sk->net_pinfo.af_inet6.recverr)
		ipv6_icmp_error(sk, skb, err, uh->dest, ntohl(info), (u8 *)(uh+1));

	sk->err = err;
	sk->error_report(sk);
out:
	sock_put(sk);
}

static inline int udpv6_queue_rcv_skb(struct sock * sk, struct sk_buff *skb)
{
#if defined(CONFIG_FILTER)
	if (sk->filter && skb->ip_summed != CHECKSUM_UNNECESSARY) {
		if ((unsigned short)csum_fold(skb_checksum(skb, 0, skb->len, skb->csum))) {
			UDP6_INC_STATS_BH(UdpInErrors);
			IP6_INC_STATS_BH(Ip6InDiscards);
			kfree_skb(skb);
			return 0;
		}
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	}
#endif
	if (sock_queue_rcv_skb(sk,skb)<0) {
		UDP6_INC_STATS_BH(UdpInErrors);
		IP6_INC_STATS_BH(Ip6InDiscards);
		kfree_skb(skb);
		return 0;
	}
  	IP6_INC_STATS_BH(Ip6InDelivers);
	UDP6_INC_STATS_BH(UdpInDatagrams);
	return 0;
}

static struct sock *udp_v6_mcast_next(struct sock *sk,
				      u16 loc_port, struct in6_addr *loc_addr,
				      u16 rmt_port, struct in6_addr *rmt_addr,
				      int dif)
{
	struct sock *s = sk;
	unsigned short num = ntohs(loc_port);
	for(; s; s = s->next) {
		if(s->num == num) {
			struct ipv6_pinfo *np = &s->net_pinfo.af_inet6;
			if(s->dport) {
				if(s->dport != rmt_port)
					continue;
			}
			if(!ipv6_addr_any(&np->daddr) &&
			   ipv6_addr_cmp(&np->daddr, rmt_addr))
				continue;

			if (s->bound_dev_if && s->bound_dev_if != dif)
				continue;

			if(!ipv6_addr_any(&np->rcv_saddr)) {
				if(ipv6_addr_cmp(&np->rcv_saddr, loc_addr) == 0)
					return s;
				continue;
			}
			if(!inet6_mc_check(s, loc_addr, rmt_addr))
				continue;
			return s;
		}
	}
	return NULL;
}

/*
 * Note: called only from the BH handler context,
 * so we don't need to lock the hashes.
 */
static void udpv6_mcast_deliver(struct udphdr *uh,
				struct in6_addr *saddr, struct in6_addr *daddr,
				struct sk_buff *skb)
{
	struct sock *sk, *sk2;
	struct sk_buff *buff;
	int dif;

	read_lock(&udp_hash_lock);
	sk = udp_hash[ntohs(uh->dest) & (UDP_HTABLE_SIZE - 1)];
	dif = skb->dev->ifindex;
	sk = udp_v6_mcast_next(sk, uh->dest, daddr, uh->source, saddr, dif);
	if (!sk)
		goto free_skb;

	buff = NULL;
	sk2 = sk;
	while((sk2 = udp_v6_mcast_next(sk2->next, uh->dest, daddr,
						  uh->source, saddr, dif))) {
		if (!buff) {
			buff = skb_clone(skb, GFP_ATOMIC);
			if (!buff)
				continue;
		}
		if (sock_queue_rcv_skb(sk2, buff) >= 0)
			buff = NULL;
	}
	if (buff)
		kfree_skb(buff);
	if (sock_queue_rcv_skb(sk, skb) < 0) {
free_skb:
		kfree_skb(skb);
	}
	read_unlock(&udp_hash_lock);
}

int udpv6_rcv(struct sk_buff *skb)
{
	struct sock *sk;
  	struct udphdr *uh;
	struct net_device *dev = skb->dev;
	struct in6_addr *saddr, *daddr;
	u32 ulen = 0;

	if (!pskb_may_pull(skb, sizeof(struct udphdr)))
		goto short_packet;

	saddr = &skb->nh.ipv6h->saddr;
	daddr = &skb->nh.ipv6h->daddr;
	uh = skb->h.uh;

	ulen = ntohs(uh->len);

	/* Check for jumbo payload */
	if (ulen == 0)
		ulen = skb->len;

	if (ulen > skb->len || ulen < sizeof(*uh))
		goto short_packet;

	if (uh->check == 0) {
		/* IPv6 draft-v2 section 8.1 says that we SHOULD log
		   this error. Well, it is reasonable.
		 */
		if (net_ratelimit())
			printk(KERN_INFO "IPv6: udp checksum is 0\n");
		goto discard;
	}

	if (ulen < skb->len) {
		if (__pskb_trim(skb, ulen))
			goto discard;
		saddr = &skb->nh.ipv6h->saddr;
		daddr = &skb->nh.ipv6h->daddr;
		uh = skb->h.uh;
	}

	if (skb->ip_summed==CHECKSUM_HW) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		if (csum_ipv6_magic(saddr, daddr, ulen, IPPROTO_UDP, skb->csum)) {
			NETDEBUG(if (net_ratelimit()) printk(KERN_DEBUG "udp v6 hw csum failure.\n"));
			skb->ip_summed = CHECKSUM_NONE;
		}
	}
	if (skb->ip_summed != CHECKSUM_UNNECESSARY)
		skb->csum = ~csum_ipv6_magic(saddr, daddr, ulen, IPPROTO_UDP, 0);

	/* 
	 *	Multicast receive code 
	 */
	if (ipv6_addr_type(daddr) & IPV6_ADDR_MULTICAST) {
		udpv6_mcast_deliver(uh, saddr, daddr, skb);
		return 0;
	}

	/* Unicast */

	/* 
	 * check socket cache ... must talk to Alan about his plans
	 * for sock caches... i'll skip this for now.
	 */
	sk = udp_v6_lookup(saddr, uh->source, daddr, uh->dest, dev->ifindex);

	if (sk == NULL) {
		if (skb->ip_summed != CHECKSUM_UNNECESSARY &&
		    (unsigned short)csum_fold(skb_checksum(skb, 0, skb->len, skb->csum)))
			goto discard;
		UDP6_INC_STATS_BH(UdpNoPorts);

		icmpv6_send(skb, ICMPV6_DEST_UNREACH, ICMPV6_PORT_UNREACH, 0, dev);

		kfree_skb(skb);
		return(0);
	}
	
	/* deliver */
	
	udpv6_queue_rcv_skb(sk, skb);
	sock_put(sk);
	return(0);

short_packet:	
	if (net_ratelimit())
		printk(KERN_DEBUG "UDP: short packet: %d/%u\n", ulen, skb->len);

discard:
	UDP6_INC_STATS_BH(UdpInErrors);
	kfree_skb(skb);
	return(0);	
}

/*
 *	Sending
 */

struct udpv6fakehdr 
{
	struct udphdr	uh;
	struct iovec	*iov;
	__u32		wcheck;
	__u32		pl_len;
	struct in6_addr *daddr;
};

/*
 *	with checksum
 */

static int udpv6_getfrag(const void *data, struct in6_addr *addr,
			 char *buff, unsigned int offset, unsigned int len)
{
	struct udpv6fakehdr *udh = (struct udpv6fakehdr *) data;
	char *dst;
	int final = 0;
	int clen = len;

	dst = buff;

	if (offset) {
		offset -= sizeof(struct udphdr);
	} else {
		dst += sizeof(struct udphdr);
		final = 1;
		clen -= sizeof(struct udphdr);
	}

	if (csum_partial_copy_fromiovecend(dst, udh->iov, offset,
					   clen, &udh->wcheck))
		return -EFAULT;

	if (final) {
		struct in6_addr *daddr;
		
		udh->wcheck = csum_partial((char *)udh, sizeof(struct udphdr),
					   udh->wcheck);

		if (udh->daddr) {
			daddr = udh->daddr;
		} else {
			/*
			 *	use packet destination address
			 *	this should improve cache locality
			 */
			daddr = addr + 1;
		}
		udh->uh.check = csum_ipv6_magic(addr, daddr,
						udh->pl_len, IPPROTO_UDP,
						udh->wcheck);
		if (udh->uh.check == 0)
			udh->uh.check = -1;

		memcpy(buff, udh, sizeof(struct udphdr));
	}
	return 0;
}

static int udpv6_sendmsg(struct sock *sk, struct msghdr *msg, int ulen)
{
	struct ipv6_txoptions opt_space;
	struct udpv6fakehdr udh;
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) msg->msg_name;
	struct ipv6_txoptions *opt = NULL;
	struct ip6_flowlabel *flowlabel = NULL;
	struct flowi fl;
	int addr_len = msg->msg_namelen;
	struct in6_addr *daddr;
	int len = ulen + sizeof(struct udphdr);
	int addr_type;
	int hlimit = -1;
	
	int err;
	
	/* Rough check on arithmetic overflow,
	   better check is made in ip6_build_xmit
	   */
	if (ulen < 0 || ulen > INT_MAX - sizeof(struct udphdr))
		return -EMSGSIZE;
	
	fl.fl6_flowlabel = 0;
	fl.oif = 0;

	if (sin6) {
		if (sin6->sin6_family == AF_INET) {
			if (__ipv6_only_sock(sk))
				return -ENETUNREACH;
			return udp_sendmsg(sk, msg, ulen);
		}

		if (addr_len < SIN6_LEN_RFC2133)
			return -EINVAL;

		if (sin6->sin6_family && sin6->sin6_family != AF_INET6)
			return -EINVAL;

		if (sin6->sin6_port == 0)
			return -EINVAL;

		udh.uh.dest = sin6->sin6_port;
		daddr = &sin6->sin6_addr;

		if (np->sndflow) {
			fl.fl6_flowlabel = sin6->sin6_flowinfo&IPV6_FLOWINFO_MASK;
			if (fl.fl6_flowlabel&IPV6_FLOWLABEL_MASK) {
				flowlabel = fl6_sock_lookup(sk, fl.fl6_flowlabel);
				if (flowlabel == NULL)
					return -EINVAL;
				daddr = &flowlabel->dst;
			}
		}

		/* Otherwise it will be difficult to maintain sk->dst_cache. */
		if (sk->state == TCP_ESTABLISHED &&
		    !ipv6_addr_cmp(daddr, &sk->net_pinfo.af_inet6.daddr))
			daddr = &sk->net_pinfo.af_inet6.daddr;

		if (addr_len >= sizeof(struct sockaddr_in6) &&
		    sin6->sin6_scope_id &&
		    ipv6_addr_type(daddr)&IPV6_ADDR_LINKLOCAL)
			fl.oif = sin6->sin6_scope_id;
	} else {
		if (sk->state != TCP_ESTABLISHED)
			return -EDESTADDRREQ;

		udh.uh.dest = sk->dport;
		daddr = &sk->net_pinfo.af_inet6.daddr;
		fl.fl6_flowlabel = np->flow_label;
	}

	addr_type = ipv6_addr_type(daddr);

	if (addr_type == IPV6_ADDR_MAPPED) {
		struct sockaddr_in sin;

		if (__ipv6_only_sock(sk))
			return -ENETUNREACH;

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = daddr->s6_addr32[3];
		sin.sin_port = udh.uh.dest;
		msg->msg_name = (struct sockaddr *)(&sin);
		msg->msg_namelen = sizeof(sin);
		fl6_sock_release(flowlabel);

		return udp_sendmsg(sk, msg, ulen);
	}

	udh.daddr = NULL;
	if (!fl.oif)
		fl.oif = sk->bound_dev_if;
	fl.fl6_src = NULL;

	if (msg->msg_controllen) {
		opt = &opt_space;
		memset(opt, 0, sizeof(struct ipv6_txoptions));

		err = datagram_send_ctl(msg, &fl, opt, &hlimit);
		if (err < 0) {
			fl6_sock_release(flowlabel);
			return err;
		}
		if ((fl.fl6_flowlabel&IPV6_FLOWLABEL_MASK) && !flowlabel) {
			flowlabel = fl6_sock_lookup(sk, fl.fl6_flowlabel);
			if (flowlabel == NULL)
				return -EINVAL;
		}
		if (!(opt->opt_nflen|opt->opt_flen))
			opt = NULL;
	}
	if (opt == NULL)
		opt = np->opt;
	if (flowlabel)
		opt = fl6_merge_options(&opt_space, flowlabel, opt);
	if (opt && opt->srcrt)
		udh.daddr = daddr;

	udh.uh.source = sk->sport;
	udh.uh.len = len < 0x10000 ? htons(len) : 0;
	udh.uh.check = 0;
	udh.iov = msg->msg_iov;
	udh.wcheck = 0;
	udh.pl_len = len;

	fl.proto = IPPROTO_UDP;
	fl.fl6_dst = daddr;
	if (fl.fl6_src == NULL && !ipv6_addr_any(&np->saddr))
		fl.fl6_src = &np->saddr;
	fl.uli_u.ports.dport = udh.uh.dest;
	fl.uli_u.ports.sport = udh.uh.source;

	err = ip6_build_xmit(sk, udpv6_getfrag, &udh, &fl, len, opt, hlimit,
			     msg->msg_flags);

	fl6_sock_release(flowlabel);

	if (err < 0)
		return err;

	UDP6_INC_STATS_USER(UdpOutDatagrams);
	return ulen;
}

static struct inet6_protocol udpv6_protocol = 
{
	udpv6_rcv,		/* UDP handler		*/
	udpv6_err,		/* UDP error control	*/
	NULL,			/* next			*/
	IPPROTO_UDP,		/* protocol ID		*/
	0,			/* copy			*/
	NULL,			/* data			*/
	"UDPv6"			/* name			*/
};

#define LINE_LEN 190
#define LINE_FMT "%-190s\n"

static void get_udp6_sock(struct sock *sp, char *tmpbuf, int i)
{
	struct in6_addr *dest, *src;
	__u16 destp, srcp;

	dest  = &sp->net_pinfo.af_inet6.daddr;
	src   = &sp->net_pinfo.af_inet6.rcv_saddr;
	destp = ntohs(sp->dport);
	srcp  = ntohs(sp->sport);
	sprintf(tmpbuf,
		"%4d: %08X%08X%08X%08X:%04X %08X%08X%08X%08X:%04X "
		"%02X %08X:%08X %02X:%08lX %08X %5d %8d %lu %d %p",
		i,
		src->s6_addr32[0], src->s6_addr32[1],
		src->s6_addr32[2], src->s6_addr32[3], srcp,
		dest->s6_addr32[0], dest->s6_addr32[1],
		dest->s6_addr32[2], dest->s6_addr32[3], destp,
		sp->state, 
		atomic_read(&sp->wmem_alloc), atomic_read(&sp->rmem_alloc),
		0, 0L, 0,
		sock_i_uid(sp), 0,
		sock_i_ino(sp),
		atomic_read(&sp->refcnt), sp);
}

int udp6_get_info(char *buffer, char **start, off_t offset, int length)
{
	int len = 0, num = 0, i;
	off_t pos = 0;
	off_t begin;
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
	read_lock(&udp_hash_lock);
	for (i = 0; i < UDP_HTABLE_SIZE; i++) {
		struct sock *sk;

		for (sk = udp_hash[i]; sk; sk = sk->next, num++) {
			if (sk->family != PF_INET6)
				continue;
			pos += LINE_LEN+1;
			if (pos <= offset)
				continue;
			get_udp6_sock(sk, tmpbuf, i);
			len += sprintf(buffer+len, LINE_FMT, tmpbuf);
			if(len >= length)
				goto out;
		}
	}
out:
	read_unlock(&udp_hash_lock);
	begin = len - (pos - offset);
	*start = buffer + begin;
	len -= begin;
	if(len > length)
		len = length;
	if (len < 0)
		len = 0; 
	return len;
}

struct proto udpv6_prot = {
	name:		"UDP",
	close:		udpv6_close,
	connect:	udpv6_connect,
	disconnect:	udp_disconnect,
	ioctl:		udp_ioctl,
	destroy:	inet6_destroy_sock,
	setsockopt:	ipv6_setsockopt,
	getsockopt:	ipv6_getsockopt,
	sendmsg:	udpv6_sendmsg,
	recvmsg:	udpv6_recvmsg,
	backlog_rcv:	udpv6_queue_rcv_skb,
	hash:		udp_v6_hash,
	unhash:		udp_v6_unhash,
	get_port:	udp_v6_get_port,
};

extern struct proto_ops inet6_dgram_ops;

static struct inet_protosw udpv6_protosw = {
	type:        SOCK_DGRAM,
	protocol:    IPPROTO_UDP,
	prot:        &udpv6_prot,
	ops:         &inet6_dgram_ops,
	capability:  -1,
	no_check:    UDP_CSUM_DEFAULT,
	flags:       INET_PROTOSW_PERMANENT,
};


void __init udpv6_init(void)
{
	inet6_add_protocol(&udpv6_protocol);
	inet6_register_protosw(&udpv6_protosw);
}
