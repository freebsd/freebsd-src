/*
 *	common UDP/RAW code
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<roque@di.fc.ul.pt>	
 *
 *	$Id: datagram.c,v 1.23 2001/09/01 00:31:50 davem Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in6.h>
#include <linux/ipv6.h>
#include <linux/route.h>

#include <net/ipv6.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <net/transp_v6.h>

#include <linux/errqueue.h>
#include <asm/uaccess.h>

void ipv6_icmp_error(struct sock *sk, struct sk_buff *skb, int err, 
		     u16 port, u32 info, u8 *payload)
{
	struct icmp6hdr *icmph = (struct icmp6hdr *)skb->h.raw;
	struct sock_exterr_skb *serr;

	if (!sk->net_pinfo.af_inet6.recverr)
		return;

	skb = skb_clone(skb, GFP_ATOMIC);
	if (!skb)
		return;

	serr = SKB_EXT_ERR(skb);
	serr->ee.ee_errno = err;
	serr->ee.ee_origin = SO_EE_ORIGIN_ICMP6;
	serr->ee.ee_type = icmph->icmp6_type; 
	serr->ee.ee_code = icmph->icmp6_code;
	serr->ee.ee_pad = 0;
	serr->ee.ee_info = info;
	serr->ee.ee_data = 0;
	serr->addr_offset = (u8*)&(((struct ipv6hdr*)(icmph+1))->daddr) - skb->nh.raw;
	serr->port = port;

	skb->h.raw = payload;
	__skb_pull(skb, payload - skb->data);

	if (sock_queue_err_skb(sk, skb))
		kfree_skb(skb);
}

void ipv6_local_error(struct sock *sk, int err, struct flowi *fl, u32 info)
{
	struct sock_exterr_skb *serr;
	struct ipv6hdr *iph;
	struct sk_buff *skb;

	if (!sk->net_pinfo.af_inet6.recverr)
		return;

	skb = alloc_skb(sizeof(struct ipv6hdr), GFP_ATOMIC);
	if (!skb)
		return;

	iph = (struct ipv6hdr*)skb_put(skb, sizeof(struct ipv6hdr));
	skb->nh.ipv6h = iph;
	memcpy(&iph->daddr, fl->fl6_dst, 16);

	serr = SKB_EXT_ERR(skb);
	serr->ee.ee_errno = err;
	serr->ee.ee_origin = SO_EE_ORIGIN_LOCAL;
	serr->ee.ee_type = 0; 
	serr->ee.ee_code = 0;
	serr->ee.ee_pad = 0;
	serr->ee.ee_info = info;
	serr->ee.ee_data = 0;
	serr->addr_offset = (u8*)&iph->daddr - skb->nh.raw;
	serr->port = fl->uli_u.ports.dport;

	skb->h.raw = skb->tail;
	__skb_pull(skb, skb->tail - skb->data);

	if (sock_queue_err_skb(sk, skb))
		kfree_skb(skb);
}

/* 
 *	Handle MSG_ERRQUEUE
 */
int ipv6_recv_error(struct sock *sk, struct msghdr *msg, int len)
{
	struct sock_exterr_skb *serr;
	struct sk_buff *skb, *skb2;
	struct sockaddr_in6 *sin;
	struct {
		struct sock_extended_err ee;
		struct sockaddr_in6	 offender;
	} errhdr;
	int err;
	int copied;

	err = -EAGAIN;
	skb = skb_dequeue(&sk->error_queue);
	if (skb == NULL)
		goto out;

	copied = skb->len;
	if (copied > len) {
		msg->msg_flags |= MSG_TRUNC;
		copied = len;
	}
	err = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);
	if (err)
		goto out_free_skb;

	sock_recv_timestamp(msg, sk, skb);

	serr = SKB_EXT_ERR(skb);

	sin = (struct sockaddr_in6 *)msg->msg_name;
	if (sin) {
		sin->sin6_family = AF_INET6;
		sin->sin6_flowinfo = 0;
		sin->sin6_port = serr->port; 
		sin->sin6_scope_id = 0;
		if (serr->ee.ee_origin == SO_EE_ORIGIN_ICMP6) {
			memcpy(&sin->sin6_addr, skb->nh.raw + serr->addr_offset, 16);
			if (sk->net_pinfo.af_inet6.sndflow)
				sin->sin6_flowinfo = *(u32*)(skb->nh.raw + serr->addr_offset - 24) & IPV6_FLOWINFO_MASK;
			if (ipv6_addr_type(&sin->sin6_addr) & IPV6_ADDR_LINKLOCAL) {
				struct inet6_skb_parm *opt = (struct inet6_skb_parm *) skb->cb;
				sin->sin6_scope_id = opt->iif;
			}
		} else {
			ipv6_addr_set(&sin->sin6_addr, 0, 0,
				      htonl(0xffff),
				      *(u32*)(skb->nh.raw + serr->addr_offset));
		}
	}

	memcpy(&errhdr.ee, &serr->ee, sizeof(struct sock_extended_err));
	sin = &errhdr.offender;
	sin->sin6_family = AF_UNSPEC;
	if (serr->ee.ee_origin != SO_EE_ORIGIN_LOCAL) {
		sin->sin6_family = AF_INET6;
		sin->sin6_flowinfo = 0;
		sin->sin6_scope_id = 0;
		if (serr->ee.ee_origin == SO_EE_ORIGIN_ICMP6) {
			memcpy(&sin->sin6_addr, &skb->nh.ipv6h->saddr, 16);
			if (sk->net_pinfo.af_inet6.rxopt.all)
				datagram_recv_ctl(sk, msg, skb);
			if (ipv6_addr_type(&sin->sin6_addr) & IPV6_ADDR_LINKLOCAL) {
				struct inet6_skb_parm *opt = (struct inet6_skb_parm *) skb->cb;
				sin->sin6_scope_id = opt->iif;
			}
		} else {
			ipv6_addr_set(&sin->sin6_addr, 0, 0,
				      htonl(0xffff),
				      skb->nh.iph->saddr);
			if (sk->protinfo.af_inet.cmsg_flags)
				ip_cmsg_recv(msg, skb);
		}
	}

	put_cmsg(msg, SOL_IPV6, IPV6_RECVERR, sizeof(errhdr), &errhdr);

	/* Now we could try to dump offended packet options */

	msg->msg_flags |= MSG_ERRQUEUE;
	err = copied;

	/* Reset and regenerate socket error */
	spin_lock_irq(&sk->error_queue.lock);
	sk->err = 0;
	if ((skb2 = skb_peek(&sk->error_queue)) != NULL) {
		sk->err = SKB_EXT_ERR(skb2)->ee.ee_errno;
		spin_unlock_irq(&sk->error_queue.lock);
		sk->error_report(sk);
	} else {
		spin_unlock_irq(&sk->error_queue.lock);
	}

out_free_skb:	
	kfree_skb(skb);
out:
	return err;
}



int datagram_recv_ctl(struct sock *sk, struct msghdr *msg, struct sk_buff *skb)
{
	struct ipv6_pinfo *np = &sk->net_pinfo.af_inet6;
	struct inet6_skb_parm *opt = (struct inet6_skb_parm *) skb->cb;

	if (np->rxopt.bits.rxinfo) {
		struct in6_pktinfo src_info;

		src_info.ipi6_ifindex = opt->iif;
		ipv6_addr_copy(&src_info.ipi6_addr, &skb->nh.ipv6h->daddr);
		put_cmsg(msg, SOL_IPV6, IPV6_PKTINFO, sizeof(src_info), &src_info);
	}

	if (np->rxopt.bits.rxhlim) {
		int hlim = skb->nh.ipv6h->hop_limit;
		put_cmsg(msg, SOL_IPV6, IPV6_HOPLIMIT, sizeof(hlim), &hlim);
	}

	if (np->rxopt.bits.rxflow && (*(u32*)skb->nh.raw & IPV6_FLOWINFO_MASK)) {
		u32 flowinfo = *(u32*)skb->nh.raw & IPV6_FLOWINFO_MASK;
		put_cmsg(msg, SOL_IPV6, IPV6_FLOWINFO, sizeof(flowinfo), &flowinfo);
	}
	if (np->rxopt.bits.hopopts && opt->hop) {
		u8 *ptr = skb->nh.raw + opt->hop;
		put_cmsg(msg, SOL_IPV6, IPV6_HOPOPTS, (ptr[1]+1)<<3, ptr);
	}
	if (np->rxopt.bits.dstopts && opt->dst0) {
		u8 *ptr = skb->nh.raw + opt->dst0;
		put_cmsg(msg, SOL_IPV6, IPV6_DSTOPTS, (ptr[1]+1)<<3, ptr);
	}
	if (np->rxopt.bits.srcrt && opt->srcrt) {
		struct ipv6_rt_hdr *rthdr = (struct ipv6_rt_hdr *)(skb->nh.raw + opt->srcrt);
		put_cmsg(msg, SOL_IPV6, IPV6_RTHDR, (rthdr->hdrlen+1) << 3, rthdr);
	}
	if (np->rxopt.bits.authhdr && opt->auth) {
		u8 *ptr = skb->nh.raw + opt->auth;
		put_cmsg(msg, SOL_IPV6, IPV6_AUTHHDR, (ptr[1]+1)<<2, ptr);
	}
	if (np->rxopt.bits.dstopts && opt->dst1) {
		u8 *ptr = skb->nh.raw + opt->dst1;
		put_cmsg(msg, SOL_IPV6, IPV6_DSTOPTS, (ptr[1]+1)<<3, ptr);
	}
	return 0;
}

int datagram_send_ctl(struct msghdr *msg, struct flowi *fl,
		      struct ipv6_txoptions *opt,
		      int *hlimit)
{
	struct in6_pktinfo *src_info;
	struct cmsghdr *cmsg;
	struct ipv6_rt_hdr *rthdr;
	struct ipv6_opt_hdr *hdr;
	int len;
	int err = 0;

	for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg)) {

		if (cmsg->cmsg_len < sizeof(struct cmsghdr) ||
		    (unsigned long)(((char*)cmsg - (char*)msg->msg_control)
				    + cmsg->cmsg_len) > msg->msg_controllen) {
			err = -EINVAL;
			goto exit_f;
		}

		if (cmsg->cmsg_level != SOL_IPV6)
			continue;

		switch (cmsg->cmsg_type) {
 		case IPV6_PKTINFO:
 			if (cmsg->cmsg_len < CMSG_LEN(sizeof(struct in6_pktinfo))) {
				err = -EINVAL;
				goto exit_f;
			}

			src_info = (struct in6_pktinfo *)CMSG_DATA(cmsg);
			
			if (src_info->ipi6_ifindex) {
				if (fl->oif && src_info->ipi6_ifindex != fl->oif)
					return -EINVAL;
				fl->oif = src_info->ipi6_ifindex;
			}

			if (!ipv6_addr_any(&src_info->ipi6_addr)) {
				if (!ipv6_chk_addr(&src_info->ipi6_addr, NULL)) {
					err = -EINVAL;
					goto exit_f;
				}

				fl->fl6_src = &src_info->ipi6_addr;
			}

			break;

		case IPV6_FLOWINFO:
                        if (cmsg->cmsg_len < CMSG_LEN(4)) {
				err = -EINVAL;
				goto exit_f;
			}

			if (fl->fl6_flowlabel&IPV6_FLOWINFO_MASK) {
				if ((fl->fl6_flowlabel^*(u32 *)CMSG_DATA(cmsg))&~IPV6_FLOWINFO_MASK) {
					err = -EINVAL;
					goto exit_f;
				}
			}
			fl->fl6_flowlabel = IPV6_FLOWINFO_MASK & *(u32 *)CMSG_DATA(cmsg);
			break;

		case IPV6_HOPOPTS:
                        if (opt->hopopt || cmsg->cmsg_len < CMSG_LEN(sizeof(struct ipv6_opt_hdr))) {
				err = -EINVAL;
				goto exit_f;
			}

			hdr = (struct ipv6_opt_hdr *)CMSG_DATA(cmsg);
			len = ((hdr->hdrlen + 1) << 3);
			if (cmsg->cmsg_len < CMSG_LEN(len)) {
				err = -EINVAL;
				goto exit_f;
			}
			if (!capable(CAP_NET_RAW)) {
				err = -EPERM;
				goto exit_f;
			}
			opt->opt_nflen += len;
			opt->hopopt = hdr;
			break;

		case IPV6_DSTOPTS:
                        if (cmsg->cmsg_len < CMSG_LEN(sizeof(struct ipv6_opt_hdr))) {
				err = -EINVAL;
				goto exit_f;
			}

			hdr = (struct ipv6_opt_hdr *)CMSG_DATA(cmsg);
			len = ((hdr->hdrlen + 1) << 3);
			if (cmsg->cmsg_len < CMSG_LEN(len)) {
				err = -EINVAL;
				goto exit_f;
			}
			if (!capable(CAP_NET_RAW)) {
				err = -EPERM;
				goto exit_f;
			}
			if (opt->dst1opt) {
				err = -EINVAL;
				goto exit_f;
			}
			opt->opt_flen += len;
			opt->dst1opt = hdr;
			break;

		case IPV6_AUTHHDR:
                        if (cmsg->cmsg_len < CMSG_LEN(sizeof(struct ipv6_opt_hdr))) {
				err = -EINVAL;
				goto exit_f;
			}

			hdr = (struct ipv6_opt_hdr *)CMSG_DATA(cmsg);
			len = ((hdr->hdrlen + 2) << 2);
			if (cmsg->cmsg_len < CMSG_LEN(len)) {
				err = -EINVAL;
				goto exit_f;
			}
			if (len & ~7) {
				err = -EINVAL;
				goto exit_f;
			}
			opt->opt_flen += len;
			opt->auth = hdr;
			break;

		case IPV6_RTHDR:
                        if (cmsg->cmsg_len < CMSG_LEN(sizeof(struct ipv6_rt_hdr))) {
				err = -EINVAL;
				goto exit_f;
			}

			rthdr = (struct ipv6_rt_hdr *)CMSG_DATA(cmsg);

			/*
			 *	TYPE 0
			 */
			if (rthdr->type) {
				err = -EINVAL;
				goto exit_f;
			}

			len = ((rthdr->hdrlen + 1) << 3);

                        if (cmsg->cmsg_len < CMSG_LEN(len)) {
				err = -EINVAL;
				goto exit_f;
			}

			/* segments left must also match */
			if ((rthdr->hdrlen >> 1) != rthdr->segments_left) {
				err = -EINVAL;
				goto exit_f;
			}

			opt->opt_nflen += len;
			opt->srcrt = rthdr;

			if (opt->dst1opt) {
				int dsthdrlen = ((opt->dst1opt->hdrlen+1)<<3);

				opt->opt_nflen += dsthdrlen;
				opt->dst0opt = opt->dst1opt;
				opt->dst1opt = NULL;
				opt->opt_flen -= dsthdrlen;
			}

			break;

		case IPV6_HOPLIMIT:
			if (cmsg->cmsg_len != CMSG_LEN(sizeof(int))) {
				err = -EINVAL;
				goto exit_f;
			}

			*hlimit = *(int *)CMSG_DATA(cmsg);
			break;

		default:
			if (net_ratelimit())
				printk(KERN_DEBUG "invalid cmsg type: %d\n", cmsg->cmsg_type);
			err = -EINVAL;
			break;
		};
	}

exit_f:
	return err;
}
