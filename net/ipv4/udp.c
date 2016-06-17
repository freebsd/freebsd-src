/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		The User Datagram Protocol (UDP).
 *
 * Version:	$Id: udp.c,v 1.100.2.4 2002/03/05 12:47:34 davem Exp $
 *
 * Authors:	Ross Biro, <bir7@leland.Stanford.Edu>
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Arnt Gulbrandsen, <agulbra@nvg.unit.no>
 *		Alan Cox, <Alan.Cox@linux.org>
 *
 * Fixes:
 *		Alan Cox	:	verify_area() calls
 *		Alan Cox	: 	stopped close while in use off icmp
 *					messages. Not a fix but a botch that
 *					for udp at least is 'valid'.
 *		Alan Cox	:	Fixed icmp handling properly
 *		Alan Cox	: 	Correct error for oversized datagrams
 *		Alan Cox	:	Tidied select() semantics. 
 *		Alan Cox	:	udp_err() fixed properly, also now 
 *					select and read wake correctly on errors
 *		Alan Cox	:	udp_send verify_area moved to avoid mem leak
 *		Alan Cox	:	UDP can count its memory
 *		Alan Cox	:	send to an unknown connection causes
 *					an ECONNREFUSED off the icmp, but
 *					does NOT close.
 *		Alan Cox	:	Switched to new sk_buff handlers. No more backlog!
 *		Alan Cox	:	Using generic datagram code. Even smaller and the PEEK
 *					bug no longer crashes it.
 *		Fred Van Kempen	: 	Net2e support for sk->broadcast.
 *		Alan Cox	:	Uses skb_free_datagram
 *		Alan Cox	:	Added get/set sockopt support.
 *		Alan Cox	:	Broadcasting without option set returns EACCES.
 *		Alan Cox	:	No wakeup calls. Instead we now use the callbacks.
 *		Alan Cox	:	Use ip_tos and ip_ttl
 *		Alan Cox	:	SNMP Mibs
 *		Alan Cox	:	MSG_DONTROUTE, and 0.0.0.0 support.
 *		Matt Dillon	:	UDP length checks.
 *		Alan Cox	:	Smarter af_inet used properly.
 *		Alan Cox	:	Use new kernel side addressing.
 *		Alan Cox	:	Incorrect return on truncated datagram receive.
 *	Arnt Gulbrandsen 	:	New udp_send and stuff
 *		Alan Cox	:	Cache last socket
 *		Alan Cox	:	Route cache
 *		Jon Peatfield	:	Minor efficiency fix to sendto().
 *		Mike Shaver	:	RFC1122 checks.
 *		Alan Cox	:	Nonblocking error fix.
 *	Willy Konynenberg	:	Transparent proxying support.
 *		Mike McLagan	:	Routing by source
 *		David S. Miller	:	New socket lookup architecture.
 *					Last socket cache retained as it
 *					does have a high hit rate.
 *		Olaf Kirch	:	Don't linearise iovec on sendmsg.
 *		Andi Kleen	:	Some cleanups, cache destination entry
 *					for connect. 
 *	Vitaly E. Lavrov	:	Transparent proxy revived after year coma.
 *		Melvin Smith	:	Check msg_name not msg_namelen in sendto(),
 *					return ENOTCONN for unconnected sockets (POSIX)
 *		Janos Farkas	:	don't deliver multi/broadcasts to a different
 *					bound-to-device socket
 *	YOSHIFUJI Hideaki @USAGI and:	Support IPV6_V6ONLY socket option, which
 *	Alexey Kuznetsov:		allow both IPv4 and IPv6 sockets to bind
 *					a single port at the same time.
 *
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
 
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/ioctls.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/config.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <net/snmp.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/protocol.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/udp.h>
#include <net/icmp.h>
#include <net/route.h>
#include <net/inet_common.h>
#include <net/checksum.h>

/*
 *	Snmp MIB for the UDP layer
 */

struct udp_mib		udp_statistics[NR_CPUS*2];

struct sock *udp_hash[UDP_HTABLE_SIZE];
rwlock_t udp_hash_lock = RW_LOCK_UNLOCKED;

/* Shared by v4/v6 udp. */
int udp_port_rover;

static int udp_v4_get_port(struct sock *sk, unsigned short snum)
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
		for(i = 0; i < (1 << 16) / UDP_HTABLE_SIZE; i++, result += UDP_HTABLE_SIZE) {
			if (result > sysctl_local_port_range[1])
				result = sysctl_local_port_range[0]
					+ ((result - sysctl_local_port_range[0]) &
					   (UDP_HTABLE_SIZE - 1));
			if (!udp_lport_inuse(result))
				break;
		}
		if (i >= (1 << 16) / UDP_HTABLE_SIZE)
			goto fail;
gotit:
		udp_port_rover = snum = result;
	} else {
		struct sock *sk2;

		for (sk2 = udp_hash[snum & (UDP_HTABLE_SIZE - 1)];
		     sk2 != NULL;
		     sk2 = sk2->next) {
			if (sk2->num == snum &&
			    sk2 != sk &&
			    !ipv6_only_sock(sk2) &&
			    (!sk2->bound_dev_if ||
			     !sk->bound_dev_if ||
			     sk2->bound_dev_if == sk->bound_dev_if) &&
			    (!sk2->rcv_saddr ||
			     !sk->rcv_saddr ||
			     sk2->rcv_saddr == sk->rcv_saddr) &&
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

static void udp_v4_hash(struct sock *sk)
{
	BUG();
}

static void udp_v4_unhash(struct sock *sk)
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

/* UDP is nearly always wildcards out the wazoo, it makes no sense to try
 * harder than this. -DaveM
 */
struct sock *udp_v4_lookup_longway(u32 saddr, u16 sport, u32 daddr, u16 dport, int dif)
{
	struct sock *sk, *result = NULL;
	unsigned short hnum = ntohs(dport);
	int badness = -1;

	for(sk = udp_hash[hnum & (UDP_HTABLE_SIZE - 1)]; sk != NULL; sk = sk->next) {
		if(sk->num == hnum && !ipv6_only_sock(sk)) {
			int score;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
			score = sk->family == PF_INET ? 1 : 0;
#else
			score = 1;
#endif
			if(sk->rcv_saddr) {
				if(sk->rcv_saddr != daddr)
					continue;
				score+=2;
			}
			if(sk->daddr) {
				if(sk->daddr != saddr)
					continue;
				score+=2;
			}
			if(sk->dport) {
				if(sk->dport != sport)
					continue;
				score+=2;
			}
			if(sk->bound_dev_if) {
				if(sk->bound_dev_if != dif)
					continue;
				score+=2;
			}
			if(score == 9) {
				result = sk;
				break;
			} else if(score > badness) {
				result = sk;
				badness = score;
			}
		}
	}
	return result;
}

__inline__ struct sock *udp_v4_lookup(u32 saddr, u16 sport, u32 daddr, u16 dport, int dif)
{
	struct sock *sk;

	read_lock(&udp_hash_lock);
	sk = udp_v4_lookup_longway(saddr, sport, daddr, dport, dif);
	if (sk)
		sock_hold(sk);
	read_unlock(&udp_hash_lock);
	return sk;
}

extern int ip_mc_sf_allow(struct sock *sk, u32 local, u32 rmt, int dif);

static inline struct sock *udp_v4_mcast_next(struct sock *sk,
					     u16 loc_port, u32 loc_addr,
					     u16 rmt_port, u32 rmt_addr,
					     int dif)
{
	struct sock *s = sk;
	unsigned short hnum = ntohs(loc_port);
	for(; s; s = s->next) {
		if ((s->num != hnum)					||
		    (s->daddr && s->daddr!=rmt_addr)			||
		    (s->dport != rmt_port && s->dport != 0)			||
		    (s->rcv_saddr  && s->rcv_saddr != loc_addr)		||
		    ipv6_only_sock(s)					||
		    (s->bound_dev_if && s->bound_dev_if != dif))
			continue;
		if (!ip_mc_sf_allow(sk, loc_addr, rmt_addr, dif))
			continue;
		break;
  	}
  	return s;
}

/*
 * This routine is called by the ICMP module when it gets some
 * sort of error condition.  If err < 0 then the socket should
 * be closed and the error returned to the user.  If err > 0
 * it's just the icmp type << 8 | icmp code.  
 * Header points to the ip header of the error packet. We move
 * on past this. Then (as it used to claim before adjustment)
 * header points to the first 8 bytes of the udp header.  We need
 * to find the appropriate port.
 */

void udp_err(struct sk_buff *skb, u32 info)
{
	struct iphdr *iph = (struct iphdr*)skb->data;
	struct udphdr *uh = (struct udphdr*)(skb->data+(iph->ihl<<2));
	int type = skb->h.icmph->type;
	int code = skb->h.icmph->code;
	struct sock *sk;
	int harderr;
	int err;

	sk = udp_v4_lookup(iph->daddr, uh->dest, iph->saddr, uh->source, skb->dev->ifindex);
	if (sk == NULL) {
		ICMP_INC_STATS_BH(IcmpInErrors);
    	  	return;	/* No socket for error */
	}

	err = 0;
	harderr = 0;

	switch (type) {
	default:
	case ICMP_TIME_EXCEEDED:
		err = EHOSTUNREACH;
		break;
	case ICMP_SOURCE_QUENCH:
		goto out;
	case ICMP_PARAMETERPROB:
		err = EPROTO;
		harderr = 1;
		break;
	case ICMP_DEST_UNREACH:
		if (code == ICMP_FRAG_NEEDED) { /* Path MTU discovery */
			if (sk->protinfo.af_inet.pmtudisc != IP_PMTUDISC_DONT) {
				err = EMSGSIZE;
				harderr = 1;
				break;
			}
			goto out;
		}
		err = EHOSTUNREACH;
		if (code <= NR_ICMP_UNREACH) {
			harderr = icmp_err_convert[code].fatal;
			err = icmp_err_convert[code].errno;
		}
		break;
	}

	/*
	 *      RFC1122: OK.  Passes ICMP errors back to application, as per 
	 *	4.1.3.3.
	 */
	if (!sk->protinfo.af_inet.recverr) {
		if (!harderr || sk->state != TCP_ESTABLISHED)
			goto out;
	} else {
		ip_icmp_error(sk, skb, err, uh->dest, info, (u8*)(uh+1));
	}
	sk->err = err;
	sk->error_report(sk);
out:
	sock_put(sk);
}


static unsigned short udp_check(struct udphdr *uh, int len, unsigned long saddr, unsigned long daddr, unsigned long base)
{
	return(csum_tcpudp_magic(saddr, daddr, len, IPPROTO_UDP, base));
}

struct udpfakehdr 
{
	struct udphdr uh;
	u32 saddr;
	u32 daddr;
	struct iovec *iov;
	u32 wcheck;
};

/*
 *	Copy and checksum a UDP packet from user space into a buffer.
 */
 
static int udp_getfrag(const void *p, char * to, unsigned int offset, unsigned int fraglen) 
{
	struct udpfakehdr *ufh = (struct udpfakehdr *)p;
	if (offset==0) {
		if (csum_partial_copy_fromiovecend(to+sizeof(struct udphdr), ufh->iov, offset,
						   fraglen-sizeof(struct udphdr), &ufh->wcheck))
			return -EFAULT;
 		ufh->wcheck = csum_partial((char *)ufh, sizeof(struct udphdr),
					   ufh->wcheck);
		ufh->uh.check = csum_tcpudp_magic(ufh->saddr, ufh->daddr, 
					  ntohs(ufh->uh.len),
					  IPPROTO_UDP, ufh->wcheck);
		if (ufh->uh.check == 0)
			ufh->uh.check = -1;
		memcpy(to, ufh, sizeof(struct udphdr));
		return 0;
	}
	if (csum_partial_copy_fromiovecend(to, ufh->iov, offset-sizeof(struct udphdr),
					   fraglen, &ufh->wcheck))
		return -EFAULT;
	return 0;
}

/*
 *	Copy a UDP packet from user space into a buffer without checksumming.
 */
 
static int udp_getfrag_nosum(const void *p, char * to, unsigned int offset, unsigned int fraglen) 
{
	struct udpfakehdr *ufh = (struct udpfakehdr *)p;

	if (offset==0) {
		memcpy(to, ufh, sizeof(struct udphdr));
		return memcpy_fromiovecend(to+sizeof(struct udphdr), ufh->iov, offset,
					   fraglen-sizeof(struct udphdr));
	}
	return memcpy_fromiovecend(to, ufh->iov, offset-sizeof(struct udphdr),
				   fraglen);
}

int udp_sendmsg(struct sock *sk, struct msghdr *msg, int len)
{
	int ulen = len + sizeof(struct udphdr);
	struct ipcm_cookie ipc;
	struct udpfakehdr ufh;
	struct rtable *rt = NULL;
	int free = 0;
	int connected = 0;
	u32 daddr;
	u8  tos;
	int err;

	/* This check is ONLY to check for arithmetic overflow
	   on integer(!) len. Not more! Real check will be made
	   in ip_build_xmit --ANK

	   BTW socket.c -> af_*.c -> ... make multiple
	   invalid conversions size_t -> int. We MUST repair it f.e.
	   by replacing all of them with size_t and revise all
	   the places sort of len += sizeof(struct iphdr)
	   If len was ULONG_MAX-10 it would be cathastrophe  --ANK
	 */

	if (len < 0 || len > 0xFFFF)
		return -EMSGSIZE;

	/* 
	 *	Check the flags.
	 */

	if (msg->msg_flags&MSG_OOB)	/* Mirror BSD error message compatibility */
		return -EOPNOTSUPP;

	/*
	 *	Get and verify the address. 
	 */
	 
	if (msg->msg_name) {
		struct sockaddr_in * usin = (struct sockaddr_in*)msg->msg_name;
		if (msg->msg_namelen < sizeof(*usin))
			return -EINVAL;
		if (usin->sin_family != AF_INET) {
			if (usin->sin_family != AF_UNSPEC)
				return -EINVAL;
		}

		ufh.daddr = usin->sin_addr.s_addr;
		ufh.uh.dest = usin->sin_port;
		if (ufh.uh.dest == 0)
			return -EINVAL;
	} else {
		if (sk->state != TCP_ESTABLISHED)
			return -EDESTADDRREQ;
		ufh.daddr = sk->daddr;
		ufh.uh.dest = sk->dport;
		/* Open fast path for connected socket.
		   Route will not be used, if at least one option is set.
		 */
		connected = 1;
  	}
	ipc.addr = sk->saddr;
	ufh.uh.source = sk->sport;

	ipc.opt = NULL;
	ipc.oif = sk->bound_dev_if;
	if (msg->msg_controllen) {
		err = ip_cmsg_send(msg, &ipc);
		if (err)
			return err;
		if (ipc.opt)
			free = 1;
		connected = 0;
	}
	if (!ipc.opt)
		ipc.opt = sk->protinfo.af_inet.opt;

	ufh.saddr = ipc.addr;
	ipc.addr = daddr = ufh.daddr;

	if (ipc.opt && ipc.opt->srr) {
		if (!daddr)
			return -EINVAL;
		daddr = ipc.opt->faddr;
		connected = 0;
	}
	tos = RT_TOS(sk->protinfo.af_inet.tos);
	if (sk->localroute || (msg->msg_flags&MSG_DONTROUTE) || 
	    (ipc.opt && ipc.opt->is_strictroute)) {
		tos |= RTO_ONLINK;
		connected = 0;
	}

	if (MULTICAST(daddr)) {
		if (!ipc.oif)
			ipc.oif = sk->protinfo.af_inet.mc_index;
		if (!ufh.saddr)
			ufh.saddr = sk->protinfo.af_inet.mc_addr;
		connected = 0;
	}

	if (connected)
		rt = (struct rtable*)sk_dst_check(sk, 0);

	if (rt == NULL) {
		err = ip_route_output(&rt, daddr, ufh.saddr, tos, ipc.oif);
		if (err)
			goto out;

		err = -EACCES;
		if (rt->rt_flags&RTCF_BROADCAST && !sk->broadcast) 
			goto out;
		if (connected)
			sk_dst_set(sk, dst_clone(&rt->u.dst));
	}

	if (msg->msg_flags&MSG_CONFIRM)
		goto do_confirm;
back_from_confirm:

	ufh.saddr = rt->rt_src;
	if (!ipc.addr)
		ufh.daddr = ipc.addr = rt->rt_dst;
	ufh.uh.len = htons(ulen);
	ufh.uh.check = 0;
	ufh.iov = msg->msg_iov;
	ufh.wcheck = 0;

	/* RFC1122: OK.  Provides the checksumming facility (MUST) as per */
	/* 4.1.3.4. It's configurable by the application via setsockopt() */
	/* (MAY) and it defaults to on (MUST). */

	err = ip_build_xmit(sk,
			    (sk->no_check == UDP_CSUM_NOXMIT ?
			     udp_getfrag_nosum :
			     udp_getfrag),
			    &ufh, ulen, &ipc, rt, msg->msg_flags);

out:
	ip_rt_put(rt);
	if (free)
		kfree(ipc.opt);
	if (!err) {
		UDP_INC_STATS_USER(UdpOutDatagrams);
		return len;
	}
	return err;

do_confirm:
	dst_confirm(&rt->u.dst);
	if (!(msg->msg_flags&MSG_PROBE) || len)
		goto back_from_confirm;
	err = 0;
	goto out;
}

/*
 *	IOCTL requests applicable to the UDP protocol
 */
 
int udp_ioctl(struct sock *sk, int cmd, unsigned long arg)
{
	switch(cmd) 
	{
		case SIOCOUTQ:
		{
			int amount = atomic_read(&sk->wmem_alloc);
			return put_user(amount, (int *)arg);
		}

		case SIOCINQ:
		{
			struct sk_buff *skb;
			unsigned long amount;

			amount = 0;
			spin_lock_irq(&sk->receive_queue.lock);
			skb = skb_peek(&sk->receive_queue);
			if (skb != NULL) {
				/*
				 * We will only return the amount
				 * of this packet since that is all
				 * that will be read.
				 */
				amount = skb->len - sizeof(struct udphdr);
			}
			spin_unlock_irq(&sk->receive_queue.lock);
			return put_user(amount, (int *)arg);
		}

		default:
			return -ENOIOCTLCMD;
	}
	return(0);
}

static __inline__ int __udp_checksum_complete(struct sk_buff *skb)
{
	return (unsigned short)csum_fold(skb_checksum(skb, 0, skb->len, skb->csum));
}

static __inline__ int udp_checksum_complete(struct sk_buff *skb)
{
	return skb->ip_summed != CHECKSUM_UNNECESSARY &&
		__udp_checksum_complete(skb);
}

/*
 * 	This should be easy, if there is something there we
 * 	return it, otherwise we block.
 */

int udp_recvmsg(struct sock *sk, struct msghdr *msg, int len,
		int noblock, int flags, int *addr_len)
{
  	struct sockaddr_in *sin = (struct sockaddr_in *)msg->msg_name;
  	struct sk_buff *skb;
  	int copied, err;

	/*
	 *	Check any passed addresses
	 */
	if (addr_len)
		*addr_len=sizeof(*sin);

	if (flags & MSG_ERRQUEUE)
		return ip_recv_error(sk, msg, len);

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
		if (__udp_checksum_complete(skb))
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
	if (sin)
	{
		sin->sin_family = AF_INET;
		sin->sin_port = skb->h.uh->source;
		sin->sin_addr.s_addr = skb->nh.iph->saddr;
		memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
  	}
	if (sk->protinfo.af_inet.cmsg_flags)
		ip_cmsg_recv(msg, skb);
	err = copied;
  
out_free:
  	skb_free_datagram(sk, skb);
out:
  	return err;

csum_copy_err:
	UDP_INC_STATS_BH(UdpInErrors);

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

	if (noblock)
		return -EAGAIN;	
	goto try_again;
}

int udp_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	struct sockaddr_in *usin = (struct sockaddr_in *) uaddr;
	struct rtable *rt;
	u32 saddr;
	int oif;
	int err;

	
	if (addr_len < sizeof(*usin)) 
	  	return -EINVAL;

	if (usin->sin_family != AF_INET) 
	  	return -EAFNOSUPPORT;

	sk_dst_reset(sk);

	oif = sk->bound_dev_if;
	saddr = sk->saddr;
	if (MULTICAST(usin->sin_addr.s_addr)) {
		if (!oif)
			oif = sk->protinfo.af_inet.mc_index;
		if (!saddr)
			saddr = sk->protinfo.af_inet.mc_addr;
	}
	err = ip_route_connect(&rt, usin->sin_addr.s_addr, saddr,
			       RT_CONN_FLAGS(sk), oif);
	if (err)
		return err;
	if ((rt->rt_flags&RTCF_BROADCAST) && !sk->broadcast) {
		ip_rt_put(rt);
		return -EACCES;
	}
  	if(!sk->saddr)
	  	sk->saddr = rt->rt_src;		/* Update source address */
	if(!sk->rcv_saddr)
		sk->rcv_saddr = rt->rt_src;
	sk->daddr = rt->rt_dst;
	sk->dport = usin->sin_port;
	sk->state = TCP_ESTABLISHED;
	sk->protinfo.af_inet.id = jiffies;

	sk_dst_set(sk, &rt->u.dst);
	return(0);
}

int udp_disconnect(struct sock *sk, int flags)
{
	/*
	 *	1003.1g - break association.
	 */
	 
	sk->state = TCP_CLOSE;
	sk->daddr = 0;
	sk->dport = 0;
	sk->bound_dev_if = 0;
	if (!(sk->userlocks&SOCK_BINDADDR_LOCK)) {
		sk->rcv_saddr = 0;
		sk->saddr = 0;
#if defined(CONFIG_IPV6) || defined(CONFIG_IPV6_MODULE)
		memset(&sk->net_pinfo.af_inet6.saddr, 0, 16);
		memset(&sk->net_pinfo.af_inet6.rcv_saddr, 0, 16);
#endif
	}
	if (!(sk->userlocks&SOCK_BINDPORT_LOCK)) {
		sk->prot->unhash(sk);
		sk->sport = 0;
	}
	sk_dst_reset(sk);
	return 0;
}

static void udp_close(struct sock *sk, long timeout)
{
	inet_sock_release(sk);
}

static int udp_queue_rcv_skb(struct sock * sk, struct sk_buff *skb)
{
	/*
	 *	Charge it to the socket, dropping if the queue is full.
	 */

#if defined(CONFIG_FILTER)
	if (sk->filter && skb->ip_summed != CHECKSUM_UNNECESSARY) {
		if (__udp_checksum_complete(skb)) {
			UDP_INC_STATS_BH(UdpInErrors);
			IP_INC_STATS_BH(IpInDiscards);
			ip_statistics[smp_processor_id()*2].IpInDelivers--;
			kfree_skb(skb);
			return -1;
		}
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	}
#endif

	if (sock_queue_rcv_skb(sk,skb)<0) {
		UDP_INC_STATS_BH(UdpInErrors);
		IP_INC_STATS_BH(IpInDiscards);
		ip_statistics[smp_processor_id()*2].IpInDelivers--;
		kfree_skb(skb);
		return -1;
	}
	UDP_INC_STATS_BH(UdpInDatagrams);
	return 0;
}

/*
 *	Multicasts and broadcasts go to each listener.
 *
 *	Note: called only from the BH handler context,
 *	so we don't need to lock the hashes.
 */
static int udp_v4_mcast_deliver(struct sk_buff *skb, struct udphdr *uh,
				 u32 saddr, u32 daddr)
{
	struct sock *sk;
	int dif;

	read_lock(&udp_hash_lock);
	sk = udp_hash[ntohs(uh->dest) & (UDP_HTABLE_SIZE - 1)];
	dif = skb->dev->ifindex;
	sk = udp_v4_mcast_next(sk, uh->dest, daddr, uh->source, saddr, dif);
	if (sk) {
		struct sock *sknext = NULL;

		do {
			struct sk_buff *skb1 = skb;

			sknext = udp_v4_mcast_next(sk->next, uh->dest, daddr,
						   uh->source, saddr, dif);
			if(sknext)
				skb1 = skb_clone(skb, GFP_ATOMIC);

			if(skb1)
				udp_queue_rcv_skb(sk, skb1);
			sk = sknext;
		} while(sknext);
	} else
		kfree_skb(skb);
	read_unlock(&udp_hash_lock);
	return 0;
}

/* Initialize UDP checksum. If exited with zero value (success),
 * CHECKSUM_UNNECESSARY means, that no more checks are required.
 * Otherwise, csum completion requires chacksumming packet body,
 * including udp header and folding it to skb->csum.
 */
static int udp_checksum_init(struct sk_buff *skb, struct udphdr *uh,
			     unsigned short ulen, u32 saddr, u32 daddr)
{
	if (uh->check == 0) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else if (skb->ip_summed == CHECKSUM_HW) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		if (!udp_check(uh, ulen, saddr, daddr, skb->csum))
			return 0;
		NETDEBUG(if (net_ratelimit()) printk(KERN_DEBUG "udp v4 hw csum failure.\n"));
		skb->ip_summed = CHECKSUM_NONE;
	}
	if (skb->ip_summed != CHECKSUM_UNNECESSARY)
		skb->csum = csum_tcpudp_nofold(saddr, daddr, ulen, IPPROTO_UDP, 0);
	/* Probably, we should checksum udp header (it should be in cache
	 * in any case) and data in tiny packets (< rx copybreak).
	 */
	return 0;
}

/*
 *	All we need to do is get the socket, and then do a checksum. 
 */
 
int udp_rcv(struct sk_buff *skb)
{
  	struct sock *sk;
  	struct udphdr *uh;
	unsigned short ulen;
	struct rtable *rt = (struct rtable*)skb->dst;
	u32 saddr = skb->nh.iph->saddr;
	u32 daddr = skb->nh.iph->daddr;
	int len = skb->len;

  	IP_INC_STATS_BH(IpInDelivers);

	/*
	 *	Validate the packet and the UDP length.
	 */
	if (!pskb_may_pull(skb, sizeof(struct udphdr)))
		goto no_header;

  	uh = skb->h.uh;

	ulen = ntohs(uh->len);

	if (ulen > len || ulen < sizeof(*uh))
		goto short_packet;

	if (pskb_trim(skb, ulen))
		goto short_packet;

	if (udp_checksum_init(skb, uh, ulen, saddr, daddr) < 0)
		goto csum_error;

	if(rt->rt_flags & (RTCF_BROADCAST|RTCF_MULTICAST))
		return udp_v4_mcast_deliver(skb, uh, saddr, daddr);

	sk = udp_v4_lookup(saddr, uh->source, daddr, uh->dest, skb->dev->ifindex);

	if (sk != NULL) {
		udp_queue_rcv_skb(sk, skb);
		sock_put(sk);
		return 0;
	}

	/* No socket. Drop packet silently, if checksum is wrong */
	if (udp_checksum_complete(skb))
		goto csum_error;

	UDP_INC_STATS_BH(UdpNoPorts);
	icmp_send(skb, ICMP_DEST_UNREACH, ICMP_PORT_UNREACH, 0);

	/*
	 * Hmm.  We got an UDP packet to a port to which we
	 * don't wanna listen.  Ignore it.
	 */
	kfree_skb(skb);
	return(0);

short_packet:
	NETDEBUG(if (net_ratelimit())
		 printk(KERN_DEBUG "UDP: short packet: %u.%u.%u.%u:%u %d/%d to %u.%u.%u.%u:%u\n",
			NIPQUAD(saddr),
			ntohs(uh->source),
			ulen,
			len,
			NIPQUAD(daddr),
			ntohs(uh->dest)));
no_header:
	UDP_INC_STATS_BH(UdpInErrors);
	kfree_skb(skb);
	return(0);

csum_error:
	/* 
	 * RFC1122: OK.  Discards the bad packet silently (as far as 
	 * the network is concerned, anyway) as per 4.1.3.4 (MUST). 
	 */
	NETDEBUG(if (net_ratelimit())
		 printk(KERN_DEBUG "UDP: bad checksum. From %d.%d.%d.%d:%d to %d.%d.%d.%d:%d ulen %d\n",
			NIPQUAD(saddr),
			ntohs(uh->source),
			NIPQUAD(daddr),
			ntohs(uh->dest),
			ulen));
	UDP_INC_STATS_BH(UdpInErrors);
	kfree_skb(skb);
	return(0);
}

static void get_udp_sock(struct sock *sp, char *tmpbuf, int i)
{
	unsigned int dest, src;
	__u16 destp, srcp;

	dest  = sp->daddr;
	src   = sp->rcv_saddr;
	destp = ntohs(sp->dport);
	srcp  = ntohs(sp->sport);
	sprintf(tmpbuf, "%4d: %08X:%04X %08X:%04X"
		" %02X %08X:%08X %02X:%08lX %08X %5d %8d %lu %d %p",
		i, src, srcp, dest, destp, sp->state, 
		atomic_read(&sp->wmem_alloc), atomic_read(&sp->rmem_alloc),
		0, 0L, 0,
		sock_i_uid(sp), 0,
		sock_i_ino(sp),
		atomic_read(&sp->refcnt), sp);
}

int udp_get_info(char *buffer, char **start, off_t offset, int length)
{
	int len = 0, num = 0, i;
	off_t pos = 0;
	off_t begin;
	char tmpbuf[129];

	if (offset < 128) 
		len += sprintf(buffer, "%-127s\n",
			       "  sl  local_address rem_address   st tx_queue "
			       "rx_queue tr tm->when retrnsmt   uid  timeout inode");
	pos = 128;
	read_lock(&udp_hash_lock);
	for (i = 0; i < UDP_HTABLE_SIZE; i++) {
		struct sock *sk;

		for (sk = udp_hash[i]; sk; sk = sk->next, num++) {
			if (sk->family != PF_INET)
				continue;
			pos += 128;
			if (pos <= offset)
				continue;
			get_udp_sock(sk, tmpbuf, i);
			len += sprintf(buffer+len, "%-127s\n", tmpbuf);
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

struct proto udp_prot = {
 	name:		"UDP",
	close:		udp_close,
	connect:	udp_connect,
	disconnect:	udp_disconnect,
	ioctl:		udp_ioctl,
	setsockopt:	ip_setsockopt,
	getsockopt:	ip_getsockopt,
	sendmsg:	udp_sendmsg,
	recvmsg:	udp_recvmsg,
	backlog_rcv:	udp_queue_rcv_skb,
	hash:		udp_v4_hash,
	unhash:		udp_v4_unhash,
	get_port:	udp_v4_get_port,
};
