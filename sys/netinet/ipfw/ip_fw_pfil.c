/*-
 * Copyright (c) 2004 Andre Oppermann, Internet Business Solutions AG
 * All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if !defined(KLD_MODULE)
#include "opt_ipfw.h"
#include "opt_ipdn.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#endif /* KLD_MODULE */
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/pfil.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/ipfw/ip_fw_private.h>
#include <netgraph/ng_ipfw.h>

#include <machine/in_cksum.h>

static VNET_DEFINE(int, fw_enable) = 1;
#define V_fw_enable	VNET(fw_enable)

#ifdef INET6
static VNET_DEFINE(int, fw6_enable) = 1;
#define V_fw6_enable	VNET(fw6_enable)
#endif

int ipfw_chg_hook(SYSCTL_HANDLER_ARGS);

/* Forward declarations. */
static int ipfw_divert(struct mbuf **, int, struct ipfw_rule_ref *, int);

#ifdef SYSCTL_NODE

SYSBEGIN(f1)

SYSCTL_DECL(_net_inet_ip_fw);
SYSCTL_VNET_PROC(_net_inet_ip_fw, OID_AUTO, enable,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE3, &VNET_NAME(fw_enable), 0,
    ipfw_chg_hook, "I", "Enable ipfw");
#ifdef INET6
SYSCTL_DECL(_net_inet6_ip6_fw);
SYSCTL_VNET_PROC(_net_inet6_ip6_fw, OID_AUTO, enable,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE3, &VNET_NAME(fw6_enable), 0,
    ipfw_chg_hook, "I", "Enable ipfw+6");
#endif /* INET6 */

SYSEND

#endif /* SYSCTL_NODE */

/*
 * The pfilter hook to pass packets to ipfw_chk and then to
 * dummynet, divert, netgraph or other modules.
 * The packet may be consumed.
 */
int
ipfw_check_hook(void *arg, struct mbuf **m0, struct ifnet *ifp, int dir,
    struct inpcb *inp)
{
	struct ip_fw_args args;
	struct m_tag *tag;
	int ipfw;
	int ret;

	/* all the processing now uses ip_len in net format */
	if (mtod(*m0, struct ip *)->ip_v == 4)
		SET_NET_IPLEN(mtod(*m0, struct ip *));

	/* convert dir to IPFW values */
	dir = (dir == PFIL_IN) ? DIR_IN : DIR_OUT;
	bzero(&args, sizeof(args));

again:
	/*
	 * extract and remove the tag if present. If we are left
	 * with onepass, optimize the outgoing path.
	 */
	tag = m_tag_locate(*m0, MTAG_IPFW_RULE, 0, NULL);
	if (tag != NULL) {
		args.rule = *((struct ipfw_rule_ref *)(tag+1));
		m_tag_delete(*m0, tag);
		if (args.rule.info & IPFW_ONEPASS) {
			SET_HOST_IPLEN(mtod(*m0, struct ip *));
			return 0;
		}
	}

	args.m = *m0;
	args.oif = dir == DIR_OUT ? ifp : NULL;
	args.inp = inp;

	ipfw = ipfw_chk(&args);
	*m0 = args.m;

	KASSERT(*m0 != NULL || ipfw == IP_FW_DENY, ("%s: m0 is NULL",
	    __func__));

	/* breaking out of the switch means drop */
	ret = 0;	/* default return value for pass */
	switch (ipfw) {
	case IP_FW_PASS:
		/* next_hop may be set by ipfw_chk */
                if (args.next_hop == NULL)
                        break; /* pass */
#ifndef IPFIREWALL_FORWARD
		ret = EACCES;
#else
	    {
		struct m_tag *fwd_tag;

		/* Incoming packets should not be tagged so we do not
		 * m_tag_find. Outgoing packets may be tagged, so we
		 * reuse the tag if present.
		 */
		fwd_tag = (dir == DIR_IN) ? NULL :
			m_tag_find(*m0, PACKET_TAG_IPFORWARD, NULL);
		if (fwd_tag != NULL) {
			m_tag_unlink(*m0, fwd_tag);
		} else {
			fwd_tag = m_tag_get(PACKET_TAG_IPFORWARD,
				sizeof(struct sockaddr_in), M_NOWAIT);
			if (fwd_tag == NULL) {
				ret = EACCES;
				break; /* i.e. drop */
			}
		}
		bcopy(args.next_hop, (fwd_tag+1), sizeof(struct sockaddr_in));
		m_tag_prepend(*m0, fwd_tag);

		if (in_localip(args.next_hop->sin_addr))
			(*m0)->m_flags |= M_FASTFWD_OURS;
	    }
#endif
		break;

	case IP_FW_DENY:
		ret = EACCES;
		break; /* i.e. drop */

	case IP_FW_DUMMYNET:
		ret = EACCES;
		if (ip_dn_io_ptr == NULL)
			break; /* i.e. drop */
		if (mtod(*m0, struct ip *)->ip_v == 4)
			ret = ip_dn_io_ptr(m0, dir, &args);
		else if (mtod(*m0, struct ip *)->ip_v == 6)
			ret = ip_dn_io_ptr(m0, dir | PROTO_IPV6, &args);
		else
			break; /* drop it */
		/*
		 * XXX should read the return value.
		 * dummynet normally eats the packet and sets *m0=NULL
		 * unless the packet can be sent immediately. In this
		 * case args is updated and we should re-run the
		 * check without clearing args.
		 */
		if (*m0 != NULL)
			goto again;
		break;

	case IP_FW_TEE:
	case IP_FW_DIVERT:
		if (ip_divert_ptr == NULL) {
			ret = EACCES;
			break; /* i.e. drop */
		}
		ret = ipfw_divert(m0, dir, &args.rule,
			(ipfw == IP_FW_TEE) ? 1 : 0);
		/* continue processing for the original packet (tee). */
		if (*m0)
			goto again;
		break;

	case IP_FW_NGTEE:
	case IP_FW_NETGRAPH:
		if (ng_ipfw_input_p == NULL) {
			ret = EACCES;
			break; /* i.e. drop */
		}
		ret = ng_ipfw_input_p(m0, dir, &args,
			(ipfw == IP_FW_NGTEE) ? 1 : 0);
		if (ipfw == IP_FW_NGTEE) /* ignore errors for NGTEE */
			goto again;	/* continue with packet */
		break;

	case IP_FW_NAT:
	case IP_FW_REASS:
		goto again;		/* continue with packet */
	
	default:
		KASSERT(0, ("%s: unknown retval", __func__));
	}

	if (ret != 0) {
		if (*m0)
			FREE_PKT(*m0);
		*m0 = NULL;
	}
	if (*m0 && mtod(*m0, struct ip *)->ip_v == 4)
		SET_HOST_IPLEN(mtod(*m0, struct ip *));
	return ret;
}

/* do the divert, return 1 on error 0 on success */
static int
ipfw_divert(struct mbuf **m0, int incoming, struct ipfw_rule_ref *rule,
	int tee)
{
	/*
	 * ipfw_chk() has already tagged the packet with the divert tag.
	 * If tee is set, copy packet and return original.
	 * If not tee, consume packet and send it to divert socket.
	 */
	struct mbuf *clone;
	struct ip *ip;
	struct m_tag *tag;

	/* Cloning needed for tee? */
	if (tee == 0) {
		clone = *m0;	/* use the original mbuf */
		*m0 = NULL;
	} else {
		clone = m_dup(*m0, M_DONTWAIT);
		/* If we cannot duplicate the mbuf, we sacrifice the divert
		 * chain and continue with the tee-ed packet.
		 */
		if (clone == NULL)
			return 1;
	}

	/*
	 * Divert listeners can normally handle non-fragmented packets,
	 * but we can only reass in the non-tee case.
	 * This means that listeners on a tee rule may get fragments,
	 * and have to live with that.
	 * Note that we now have the 'reass' ipfw option so if we care
	 * we can do it before a 'tee'.
	 */
	ip = mtod(clone, struct ip *);
	if (!tee && ntohs(ip->ip_off) & (IP_MF | IP_OFFMASK)) {
		int hlen;
		struct mbuf *reass;

		SET_HOST_IPLEN(ip); /* ip_reass wants host order */
		reass = ip_reass(clone); /* Reassemble packet. */
		if (reass == NULL)
			return 0; /* not an error */
		/* if reass = NULL then it was consumed by ip_reass */
		/*
		 * IP header checksum fixup after reassembly and leave header
		 * in network byte order.
		 */
		ip = mtod(reass, struct ip *);
		hlen = ip->ip_hl << 2;
		SET_NET_IPLEN(ip);
		ip->ip_sum = 0;
		if (hlen == sizeof(struct ip))
			ip->ip_sum = in_cksum_hdr(ip);
		else
			ip->ip_sum = in_cksum(reass, hlen);
		clone = reass;
	}
	/* attach a tag to the packet with the reinject info */
	tag = m_tag_alloc(MTAG_IPFW_RULE, 0,
		    sizeof(struct ipfw_rule_ref), M_NOWAIT);
	if (tag == NULL) {
		FREE_PKT(clone);
		return 1;
	}
	*((struct ipfw_rule_ref *)(tag+1)) = *rule;
	m_tag_prepend(clone, tag);

	/* Do the dirty job... */
	ip_divert_ptr(clone, incoming);
	return 0;
}

/*
 * attach or detach hooks for a given protocol family
 */
static int
ipfw_hook(int onoff, int pf)
{
	struct pfil_head *pfh;

	pfh = pfil_head_get(PFIL_TYPE_AF, pf);
	if (pfh == NULL)
		return ENOENT;

	(void) (onoff ? pfil_add_hook : pfil_remove_hook)
	    (ipfw_check_hook, NULL, PFIL_IN | PFIL_OUT | PFIL_WAITOK, pfh);

	return 0;
}

int
ipfw_attach_hooks(int arg)
{
	int error = 0;

	if (arg == 0) /* detach */
		ipfw_hook(0, AF_INET);
        else if (V_fw_enable && ipfw_hook(1, AF_INET) != 0) {
                error = ENOENT; /* see ip_fw_pfil.c::ipfw_hook() */
                printf("ipfw_hook() error\n");
        }
#ifdef INET6
	if (arg == 0) /* detach */
		ipfw_hook(0, AF_INET6);
        else if (V_fw6_enable && ipfw_hook(1, AF_INET6) != 0) {
                error = ENOENT;
                printf("ipfw6_hook() error\n");
        }
#endif
	return error;
}

int
ipfw_chg_hook(SYSCTL_HANDLER_ARGS)
{
	int enable;
	int oldenable;
	int error;
	int af;

	if (arg1 == &VNET_NAME(fw_enable)) {
		enable = V_fw_enable;
		af = AF_INET;
	}
#ifdef INET6
	else if (arg1 == &VNET_NAME(fw6_enable)) {
		enable = V_fw6_enable;
		af = AF_INET6;
	}
#endif
	else 
		return (EINVAL);

	oldenable = enable;

	error = sysctl_handle_int(oidp, &enable, 0, req);

	if (error)
		return (error);

	enable = (enable) ? 1 : 0;

	if (enable == oldenable)
		return (0);

	error = ipfw_hook(enable, af);
	if (error)
		return (error);
	if (af == AF_INET)
		V_fw_enable = enable;
#ifdef INET6
	else if (af == AF_INET6)
		V_fw6_enable = enable;
#endif

	return (0);
}
/* end of file */
