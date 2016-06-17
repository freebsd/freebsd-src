/*
 *	RAW sockets for IPv6
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	Adapted from linux/net/ipv4/raw.c
 *
 *	$Id: raw.c,v 1.50.2.1 2002/03/05 12:47:34 davem Exp $
 *
 *	Fixes:
 *	Hideaki YOSHIFUJI	:	sin6_scope_id support
 *	YOSHIFUJI,H.@USAGI	:	raw checksum (RFC2292(bis) compliance) 
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/icmpv6.h>
#include <asm/uaccess.h>
#include <asm/ioctls.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/ndisc.h>
#include <net/protocol.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/transp_v6.h>
#include <net/udp.h>
#include <net/inet_common.h>

#include <net/rawv6.h>

struct sock *raw_v6_htable[RAWV6_HTABLE_SIZE];
rwlock_t raw_v6_lock = RW_LOCK_UNLOCKED;

static void raw_v6_hash(struct sock *sk)
{
	struct sock **skp = &raw_v6_htable[sk->num & (RAWV6_HTABLE_SIZE - 1)];

	write_lock_bh(&raw_v6_lock);
	if ((sk->next = *skp) != NULL)
		(*skp)->pprev = &sk->next;
	*skp = sk;
	sk->pprev = skp;
	sock_prot_inc_use(sk->prot);
 	sock_hold(sk);
 	write_unlock_bh(&raw_v6_lock);
}

static void raw_v6_unhash(struct sock *sk)
{
 	write_lock_bh(&raw_v6_lock);
	if (sk->pprev) {
		if (sk->next)
			sk->next->pprev = sk->pprev;
		*sk->pprev = sk->next;
		sk->pprev = NULL;
		sock_prot_dec_use(sk->prot);
		__sock_put(sk);
	}
	write_unlock_bh(&raw_v6_lock);
}


/* Grumble... icmp and ip_input want to get at this... */
struct sock *__raw_v6_lookup(struct sock *sk, unsigned short num,
			     struct in6_addr *loc_addr, struct in6_addr *rmt_addr)
{
	struct sock *s = sk;
	int addr_type = ipv6_addr_type(loc_addr);

	for(s = sk; s; s = s->next) {
		if(s->num == num) {
			struct ipv6_pinfo *np = &s->net_pinfo.af_inet6;

			if (!ipv6_addr_any(&np->daddr) &&
			    ipv6_addr_cmp(&np->daddr, rmt_addr))
				continue;

			if (!ipv6_addr_any(&np->rcv_saddr)) {
				if (ipv6_addr_cmp(&np->rcv_saddr, loc_addr) == 0)
					break;
				if ((addr_type & IPV6_ADDR_MULTICAST) &&
				    inet6_mc_check(s, loc_addr, rmt_addr))
					break;
				continue;
			}
			break;
		}
	}
	return s;
}

/*
 *	0 - deliver
 *	1 - block
 */
static __inline__ int icmpv6_filter(struct sock *sk, struct sk_buff *skb)
{
	struct icmp6hdr *icmph;
	struct raw6_opt *opt;

	opt = &sk->tp_pinfo.tp_raw;
	if (pskb_may_pull(skb, sizeof(struct icmp6hdr))) {
		__u32 *data = &opt->filter.data[0];
		int bit_nr;

		icmph = (struct icmp6hdr *) skb->data;
		bit_nr = icmph->icmp6_type;

		return (data[bit_nr >> 5] & (1 << (bit_nr & 31))) != 0;
	}
	return 0;
}

/*
 *	demultiplex raw sockets.
 *	(should consider queueing the skb in the sock receive_queue
 *	without calling rawv6.c)
 */
struct sock * ipv6_raw_deliver(struct sk_buff *skb, int nexthdr)
{
	struct in6_addr *saddr;
	struct in6_addr *daddr;
	struct sock *sk, *sk2;
	__u8 hash;

	saddr = &skb->nh.ipv6h->saddr;
	daddr = saddr + 1;

	hash = nexthdr & (MAX_INET_PROTOS - 1);

	read_lock(&raw_v6_lock);
	sk = raw_v6_htable[hash];

	/*
	 *	The first socket found will be delivered after
	 *	delivery to transport protocols.
	 */

	if (sk == NULL)
		goto out;

	sk = __raw_v6_lookup(sk, nexthdr, daddr, saddr);

	if (sk) {
		sk2 = sk;

		while ((sk2 = __raw_v6_lookup(sk2->next, nexthdr, daddr, saddr))) {
			struct sk_buff *buff;

			if (nexthdr == IPPROTO_ICMPV6 &&
			    icmpv6_filter(sk2, skb))
				continue;

			buff = skb_clone(skb, GFP_ATOMIC);
			if (buff)
				rawv6_rcv(sk2, buff);
		}
	}

	if (sk && nexthdr == IPPROTO_ICMPV6 && icmpv6_filter(sk, skb))
		sk = NULL;

out:
	if (sk)
		sock_hold(sk);
	read_unlock(&raw_v6_lock);
	return sk;
}

/* This cleans up af_inet6 a bit. -DaveM */
static int rawv6_bind(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *) uaddr;
	__u32 v4addr = 0;
	int addr_type;
	int err;

	if (addr_len < SIN6_LEN_RFC2133)
		return -EINVAL;
	addr_type = ipv6_addr_type(&addr->sin6_addr);

	/* Raw sockets are IPv6 only */
	if (addr_type == IPV6_ADDR_MAPPED)
		return(-EADDRNOTAVAIL);

	lock_sock(sk);

	err = -EINVAL;
	if (sk->state != TCP_CLOSE)
		goto out;

	if (addr_type & IPV6_ADDR_LINKLOCAL) {
		if (addr_len >= sizeof(struct sockaddr_in6) &&
		    addr->sin6_scope_id) {
			/* Override any existing binding, if another one
			 * is supplied by user.
			 */
			sk->bound_dev_if = addr->sin6_scope_id;
		}

		/* Binding to link-local address requires an interface */
		if (sk->bound_dev_if == 0)
			goto out;
	}

	/* Check if the address belongs to the host. */
	if (addr_type != IPV6_ADDR_ANY) {
		/* ipv4 addr of the socket is invalid.  Only the
		 * unpecified and mapped address have a v4 equivalent.
		 */
		v4addr = LOOPBACK4_IPV6;
		if (!(addr_type & IPV6_ADDR_MULTICAST))	{
			err = -EADDRNOTAVAIL;
			if (!ipv6_chk_addr(&addr->sin6_addr, NULL))
				goto out;
		}
	}

	sk->rcv_saddr = v4addr;
	sk->saddr = v4addr;
	ipv6_addr_copy(&sk->net_pinfo.af_inet6.rcv_saddr, &addr->sin6_addr);
	if (!(addr_type & IPV6_ADDR_MULTICAST))
		ipv6_addr_copy(&sk->net_pinfo.af_inet6.saddr, &addr->sin6_addr);
	err = 0;
out:
	release_sock(sk);
	return err;
}

void rawv6_err(struct sock *sk, struct sk_buff *skb,
	       struct inet6_skb_parm *opt,
	       int type, int code, int offset, u32 info)
{
	int err;
	int harderr;

	/* Report error on raw socket, if:
	   1. User requested recverr.
	   2. Socket is connected (otherwise the error indication
	      is useless without recverr and error is hard.
	 */
	if (!sk->net_pinfo.af_inet6.recverr && sk->state != TCP_ESTABLISHED)
		return;

	harderr = icmpv6_err_convert(type, code, &err);
	if (type == ICMPV6_PKT_TOOBIG)
		harderr = (sk->net_pinfo.af_inet6.pmtudisc == IPV6_PMTUDISC_DO);

	if (sk->net_pinfo.af_inet6.recverr) {
		u8 *payload = skb->data;
		if (!sk->protinfo.af_inet.hdrincl)
			payload += offset;
		ipv6_icmp_error(sk, skb, err, 0, ntohl(info), payload);
	}

	if (sk->net_pinfo.af_inet6.recverr || harderr) {
		sk->err = err;
		sk->error_report(sk);
	}
}

static inline int rawv6_rcv_skb(struct sock * sk, struct sk_buff * skb)
{
#if defined(CONFIG_FILTER)
	if (sk->filter && skb->ip_summed != CHECKSUM_UNNECESSARY) {
		if ((unsigned short)csum_fold(skb_checksum(skb, 0, skb->len, skb->csum))) {
			IP6_INC_STATS_BH(Ip6InDiscards);
			kfree_skb(skb);
			return 0;
		}
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	}
#endif
	/* Charge it to the socket. */
	if (sock_queue_rcv_skb(sk,skb)<0) {
		IP6_INC_STATS_BH(Ip6InDiscards);
		kfree_skb(skb);
		return 0;
	}

	IP6_INC_STATS_BH(Ip6InDelivers);
	return 0;
}

/*
 *	This is next to useless... 
 *	if we demultiplex in network layer we don't need the extra call
 *	just to queue the skb... 
 *	maybe we could have the network decide uppon a hint if it 
 *	should call raw_rcv for demultiplexing
 */
int rawv6_rcv(struct sock *sk, struct sk_buff *skb)
{
	if (!sk->tp_pinfo.tp_raw.checksum)
		skb->ip_summed = CHECKSUM_UNNECESSARY;

	if (skb->ip_summed != CHECKSUM_UNNECESSARY) {
		if (skb->ip_summed == CHECKSUM_HW) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			if (csum_ipv6_magic(&skb->nh.ipv6h->saddr,
					    &skb->nh.ipv6h->daddr,
					    skb->len, sk->num, skb->csum)) {
				NETDEBUG(if (net_ratelimit()) printk(KERN_DEBUG "raw v6 hw csum failure.\n"));
				skb->ip_summed = CHECKSUM_NONE;
			}
		}
		if (skb->ip_summed == CHECKSUM_NONE)
			skb->csum = ~csum_ipv6_magic(&skb->nh.ipv6h->saddr,
						     &skb->nh.ipv6h->daddr,
						     skb->len, sk->num, 0);
	}

	if (sk->protinfo.af_inet.hdrincl) {
		if (skb->ip_summed != CHECKSUM_UNNECESSARY &&
		    (unsigned short)csum_fold(skb_checksum(skb, 0, skb->len, skb->csum))) {
			IP6_INC_STATS_BH(Ip6InDiscards);
			kfree_skb(skb);
			return 0;
		}
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	}

	rawv6_rcv_skb(sk, skb);
	return 0;
}


/*
 *	This should be easy, if there is something there
 *	we return it, otherwise we block.
 */

int rawv6_recvmsg(struct sock *sk, struct msghdr *msg, int len,
		  int noblock, int flags, int *addr_len)
{
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)msg->msg_name;
	struct sk_buff *skb;
	int copied, err;

	if (flags & MSG_OOB)
		return -EOPNOTSUPP;
		
	if (addr_len) 
		*addr_len=sizeof(*sin6);

	if (flags & MSG_ERRQUEUE)
		return ipv6_recv_error(sk, msg, len);

	skb = skb_recv_datagram(sk, flags, noblock, &err);
	if (!skb)
		goto out;

	copied = skb->len;
  	if (copied > len) {
  		copied = len;
  		msg->msg_flags |= MSG_TRUNC;
  	}

	if (skb->ip_summed==CHECKSUM_UNNECESSARY) {
		err = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);
	} else if (msg->msg_flags&MSG_TRUNC) {
		if ((unsigned short)csum_fold(skb_checksum(skb, 0, skb->len, skb->csum)))
			goto csum_copy_err;
		err = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);
	} else {
		err = skb_copy_and_csum_datagram_iovec(skb, 0, msg->msg_iov);
		if (err == -EINVAL)
			goto csum_copy_err;
	}
	if (err)
		goto out_free;

	/* Copy the address. */
	if (sin6) {
		sin6->sin6_family = AF_INET6;
		memcpy(&sin6->sin6_addr, &skb->nh.ipv6h->saddr, 
		       sizeof(struct in6_addr));
		sin6->sin6_flowinfo = 0;
		sin6->sin6_scope_id = 0;
		if (ipv6_addr_type(&sin6->sin6_addr) & IPV6_ADDR_LINKLOCAL) {
			struct inet6_skb_parm *opt = (struct inet6_skb_parm *) skb->cb;
			sin6->sin6_scope_id = opt->iif;
		}
	}

	sock_recv_timestamp(msg, sk, skb);

	if (sk->net_pinfo.af_inet6.rxopt.all)
		datagram_recv_ctl(sk, msg, skb);
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

	/* Error for blocking case is chosen to masquerade
	   as some normal condition.
	 */
	err = (flags&MSG_DONTWAIT) ? -EAGAIN : -EHOSTUNREACH;
	IP6_INC_STATS_USER(Ip6InDiscards);
	goto out_free;
}

/*
 *	Sending...
 */

struct rawv6_fakehdr {
	struct iovec	*iov;
	struct sock	*sk;
	__u32		len;
	__u32		cksum;
	__u32		proto;
	struct in6_addr *daddr;
};

static int rawv6_getfrag(const void *data, struct in6_addr *saddr, 
			  char *buff, unsigned int offset, unsigned int len)
{
	struct iovec *iov = (struct iovec *) data;

	return memcpy_fromiovecend(buff, iov, offset, len);
}

static int rawv6_frag_cksum(const void *data, struct in6_addr *addr,
			     char *buff, unsigned int offset, 
			     unsigned int len)
{
	struct rawv6_fakehdr *hdr = (struct rawv6_fakehdr *) data;
	
	if (csum_partial_copy_fromiovecend(buff, hdr->iov, offset, 
						    len, &hdr->cksum))
		return -EFAULT;
	
	if (offset == 0) {
		struct sock *sk;
		struct raw6_opt *opt;
		struct in6_addr *daddr;
		
		sk = hdr->sk;
		opt = &sk->tp_pinfo.tp_raw;

		if (hdr->daddr)
			daddr = hdr->daddr;
		else
			daddr = addr + 1;
		
		hdr->cksum = csum_ipv6_magic(addr, daddr, hdr->len,
					     hdr->proto, hdr->cksum);
		
		if (opt->offset + 1 < len) {
			__u16 *csum;

			csum = (__u16 *) (buff + opt->offset);
			if (*csum) {
				/* in case cksum was not initialized */
				__u32 sum = hdr->cksum;
				sum += *csum;
				*csum = hdr->cksum = (sum + (sum>>16));
			} else {
				*csum = hdr->cksum;
			}
		} else {
			if (net_ratelimit())
				printk(KERN_DEBUG "icmp: cksum offset too big\n");
			return -EINVAL;
		}
	}	
	return 0; 
}


static int rawv6_sendmsg(struct sock *sk, struct msghdr *msg, int len)
{
	struct ipv6_txoptions opt_space;
	struct sockaddr_in6 * sin6 = (struct sockaddr_in6 *) msg->msg_name;
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct ipv6_txoptions *opt = NULL;
	struct ip6_flowlabel *flowlabel = NULL;
	struct flowi fl;
	int addr_len = msg->msg_namelen;
	struct in6_addr *daddr;
	struct raw6_opt *raw_opt;
	int hlimit = -1;
	u16 proto;
	int err;

	/* Rough check on arithmetic overflow,
	   better check is made in ip6_build_xmit
	 */
	if (len < 0)
		return -EMSGSIZE;

	/* Mirror BSD error message compatibility */
	if (msg->msg_flags & MSG_OOB)		
		return -EOPNOTSUPP;

	/*
	 *	Get and verify the address. 
	 */

	fl.fl6_flowlabel = 0;
	fl.oif = 0;

	if (sin6) {
		if (addr_len < SIN6_LEN_RFC2133) 
			return -EINVAL;

		if (sin6->sin6_family && sin6->sin6_family != AF_INET6) 
			return(-EINVAL);

		/* port is the proto value [0..255] carried in nexthdr */
		proto = ntohs(sin6->sin6_port);

		if (!proto)
			proto = sk->num;

		if (proto > 255)
			return(-EINVAL);

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
		
		proto = sk->num;
		daddr = &(sk->net_pinfo.af_inet6.daddr);
		fl.fl6_flowlabel = np->flow_label;
	}

	if (ipv6_addr_any(daddr)) {
		/* 
		 * unspecfied destination address 
		 * treated as error... is this correct ?
		 */
		return(-EINVAL);
	}

	if (fl.oif == 0)
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

	raw_opt = &sk->tp_pinfo.tp_raw;

	fl.proto = proto;
	fl.fl6_dst = daddr;
	if (fl.fl6_src == NULL && !ipv6_addr_any(&np->saddr))
		fl.fl6_src = &np->saddr;
	fl.uli_u.icmpt.type = 0;
	fl.uli_u.icmpt.code = 0;
	
	if (raw_opt->checksum) {
		struct rawv6_fakehdr hdr;
		
		hdr.iov = msg->msg_iov;
		hdr.sk  = sk;
		hdr.len = len;
		hdr.cksum = 0;
		hdr.proto = proto;

		if (opt && opt->srcrt)
			hdr.daddr = daddr;
		else
			hdr.daddr = NULL;

		err = ip6_build_xmit(sk, rawv6_frag_cksum, &hdr, &fl, len,
				     opt, hlimit, msg->msg_flags);
	} else {
		err = ip6_build_xmit(sk, rawv6_getfrag, msg->msg_iov, &fl, len,
				     opt, hlimit, msg->msg_flags);
	}

	fl6_sock_release(flowlabel);

	return err<0?err:len;
}

static int rawv6_seticmpfilter(struct sock *sk, int level, int optname, 
			       char *optval, int optlen)
{
	switch (optname) {
	case ICMPV6_FILTER:
		if (optlen > sizeof(struct icmp6_filter))
			optlen = sizeof(struct icmp6_filter);
		if (copy_from_user(&sk->tp_pinfo.tp_raw.filter, optval, optlen))
			return -EFAULT;
		return 0;
	default:
		return -ENOPROTOOPT;
	};

	return 0;
}

static int rawv6_geticmpfilter(struct sock *sk, int level, int optname, 
			       char *optval, int *optlen)
{
	int len;

	switch (optname) {
	case ICMPV6_FILTER:
		if (get_user(len, optlen))
			return -EFAULT;
		if (len < 0)
			return -EINVAL;
		if (len > sizeof(struct icmp6_filter))
			len = sizeof(struct icmp6_filter);
		if (put_user(len, optlen))
			return -EFAULT;
		if (copy_to_user(optval, &sk->tp_pinfo.tp_raw.filter, len))
			return -EFAULT;
		return 0;
	default:
		return -ENOPROTOOPT;
	};

	return 0;
}


static int rawv6_setsockopt(struct sock *sk, int level, int optname, 
			    char *optval, int optlen)
{
	struct raw6_opt *opt = &sk->tp_pinfo.tp_raw;
	int val;

	switch(level) {
		case SOL_RAW:
			break;

		case SOL_ICMPV6:
			if (sk->num != IPPROTO_ICMPV6)
				return -EOPNOTSUPP;
			return rawv6_seticmpfilter(sk, level, optname, optval,
						   optlen);
		case SOL_IPV6:
			if (optname == IPV6_CHECKSUM)
				break;
		default:
			return ipv6_setsockopt(sk, level, optname, optval,
					       optlen);
	};

  	if (get_user(val, (int *)optval))
		return -EFAULT;

	switch (optname) {
		case IPV6_CHECKSUM:
			/* You may get strange result with a positive odd offset;
			   RFC2292bis agrees with me. */
			if (val > 0 && (val&1))
				return(-EINVAL);
			if (val < 0) {
				opt->checksum = 0;
			} else {
				opt->checksum = 1;
				opt->offset = val;
			}

			return 0;
			break;

		default:
			return(-ENOPROTOOPT);
	}
}

static int rawv6_getsockopt(struct sock *sk, int level, int optname, 
			    char *optval, int *optlen)
{
	struct raw6_opt *opt = &sk->tp_pinfo.tp_raw;
	int val, len;

	switch(level) {
		case SOL_RAW:
			break;

		case SOL_ICMPV6:
			if (sk->num != IPPROTO_ICMPV6)
				return -EOPNOTSUPP;
			return rawv6_geticmpfilter(sk, level, optname, optval,
						   optlen);
		case SOL_IPV6:
			if (optname == IPV6_CHECKSUM)
				break;
		default:
			return ipv6_getsockopt(sk, level, optname, optval,
					       optlen);
	};

	if (get_user(len,optlen))
		return -EFAULT;

	switch (optname) {
	case IPV6_CHECKSUM:
		if (opt->checksum == 0)
			val = -1;
		else
			val = opt->offset;
		break;

	default:
		return -ENOPROTOOPT;
	}

	len = min_t(unsigned int, sizeof(int), len);

	if (put_user(len, optlen))
		return -EFAULT;
	if (copy_to_user(optval,&val,len))
		return -EFAULT;
	return 0;
}

static int rawv6_ioctl(struct sock *sk, int cmd, unsigned long arg)
{
	switch(cmd) {
		case SIOCOUTQ:
		{
			int amount = atomic_read(&sk->wmem_alloc);
			return put_user(amount, (int *)arg);
		}
		case SIOCINQ:
		{
			struct sk_buff *skb;
			int amount = 0;

			spin_lock_irq(&sk->receive_queue.lock);
			skb = skb_peek(&sk->receive_queue);
			if (skb != NULL)
				amount = skb->tail - skb->h.raw;
			spin_unlock_irq(&sk->receive_queue.lock);
			return put_user(amount, (int *)arg);
		}

		default:
			return -ENOIOCTLCMD;
	}
}

static void rawv6_close(struct sock *sk, long timeout)
{
	if (sk->num == IPPROTO_RAW)
		ip6_ra_control(sk, -1, NULL);

	inet_sock_release(sk);
}

static int rawv6_init_sk(struct sock *sk)
{
	if (sk->num == IPPROTO_ICMPV6){
		struct raw6_opt *opt = &sk->tp_pinfo.tp_raw;
		opt->checksum = 1;
		opt->offset = 2;
	}
	return(0);
}

#define LINE_LEN 190
#define LINE_FMT "%-190s\n"

static void get_raw6_sock(struct sock *sp, char *tmpbuf, int i)
{
	struct in6_addr *dest, *src;
	__u16 destp, srcp;

	dest  = &sp->net_pinfo.af_inet6.daddr;
	src   = &sp->net_pinfo.af_inet6.rcv_saddr;
	destp = 0;
	srcp  = sp->num;
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

int raw6_get_info(char *buffer, char **start, off_t offset, int length)
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
	read_lock(&raw_v6_lock);
	for (i = 0; i < RAWV6_HTABLE_SIZE; i++) {
		struct sock *sk;

		for (sk = raw_v6_htable[i]; sk; sk = sk->next, num++) {
			if (sk->family != PF_INET6)
				continue;
			pos += LINE_LEN+1;
			if (pos <= offset)
				continue;
			get_raw6_sock(sk, tmpbuf, i);
			len += sprintf(buffer+len, LINE_FMT, tmpbuf);
			if(len >= length)
				goto out;
		}
	}
out:
	read_unlock(&raw_v6_lock);
	begin = len - (pos - offset);
	*start = buffer + begin;
	len -= begin;
	if(len > length)
		len = length;
	if (len < 0)
		len = 0; 
	return len;
}

struct proto rawv6_prot = {
	name:		"RAW",
	close:		rawv6_close,
	connect:	udpv6_connect,
	disconnect:	udp_disconnect,
	ioctl:		rawv6_ioctl,
	init:		rawv6_init_sk,
	destroy:	inet6_destroy_sock,
	setsockopt:	rawv6_setsockopt,
	getsockopt:	rawv6_getsockopt,
	sendmsg:	rawv6_sendmsg,
	recvmsg:	rawv6_recvmsg,
	bind:		rawv6_bind,
	backlog_rcv:	rawv6_rcv_skb,
	hash:		raw_v6_hash,
	unhash:		raw_v6_unhash,
};
