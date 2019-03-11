/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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

#include "opt_ipfw.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */

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
#include <net/if_var.h>
#include <net/route.h>
#include <net/ethernet.h>
#include <net/pfil.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#endif

#include <netgraph/ng_ipfw.h>

#include <netpfil/ipfw/ip_fw_private.h>

#include <machine/in_cksum.h>

VNET_DEFINE_STATIC(int, fw_enable) = 1;
#define V_fw_enable	VNET(fw_enable)

#ifdef INET6
VNET_DEFINE_STATIC(int, fw6_enable) = 1;
#define V_fw6_enable	VNET(fw6_enable)
#endif

VNET_DEFINE_STATIC(int, fwlink_enable) = 0;
#define V_fwlink_enable	VNET(fwlink_enable)

int ipfw_chg_hook(SYSCTL_HANDLER_ARGS);

/* Forward declarations. */
static int ipfw_divert(struct mbuf **, int, struct ipfw_rule_ref *, int);

#ifdef SYSCTL_NODE

SYSBEGIN(f1)

SYSCTL_DECL(_net_inet_ip_fw);
SYSCTL_PROC(_net_inet_ip_fw, OID_AUTO, enable,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE3,
    &VNET_NAME(fw_enable), 0, ipfw_chg_hook, "I", "Enable ipfw");
#ifdef INET6
SYSCTL_DECL(_net_inet6_ip6_fw);
SYSCTL_PROC(_net_inet6_ip6_fw, OID_AUTO, enable,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE3,
    &VNET_NAME(fw6_enable), 0, ipfw_chg_hook, "I", "Enable ipfw+6");
#endif /* INET6 */

SYSCTL_DECL(_net_link_ether);
SYSCTL_PROC(_net_link_ether, OID_AUTO, ipfw,
    CTLFLAG_VNET | CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_SECURE3,
    &VNET_NAME(fwlink_enable), 0, ipfw_chg_hook, "I",
    "Pass ether pkts through firewall");

SYSEND

#endif /* SYSCTL_NODE */

/*
 * The pfilter hook to pass packets to ipfw_chk and then to
 * dummynet, divert, netgraph or other modules.
 * The packet may be consumed.
 */
static pfil_return_t
ipfw_check_packet(struct mbuf **m0, struct ifnet *ifp, int dir,
    void *ruleset __unused, struct inpcb *inp)
{
	struct ip_fw_args args;
	struct m_tag *tag;
	pfil_return_t ret;
	int ipfw;

	/* convert dir to IPFW values */
	dir = (dir & PFIL_IN) ? DIR_IN : DIR_OUT;
	args.flags = 0;
again:
	/*
	 * extract and remove the tag if present. If we are left
	 * with onepass, optimize the outgoing path.
	 */
	tag = m_tag_locate(*m0, MTAG_IPFW_RULE, 0, NULL);
	if (tag != NULL) {
		args.rule = *((struct ipfw_rule_ref *)(tag+1));
		m_tag_delete(*m0, tag);
		if (args.rule.info & IPFW_ONEPASS)
			return (0);
		args.flags |= IPFW_ARGS_REF;
	}

	args.m = *m0;
	args.oif = dir == DIR_OUT ? ifp : NULL;
	args.inp = inp;

	ipfw = ipfw_chk(&args);
	*m0 = args.m;

	KASSERT(*m0 != NULL || ipfw == IP_FW_DENY, ("%s: m0 is NULL",
	    __func__));

	ret = PFIL_PASS;
	switch (ipfw) {
	case IP_FW_PASS:
		/* next_hop may be set by ipfw_chk */
		if ((args.flags & (IPFW_ARGS_NH4 | IPFW_ARGS_NH4PTR |
		    IPFW_ARGS_NH6 | IPFW_ARGS_NH6PTR)) == 0)
			break;
#if (!defined(INET6) && !defined(INET))
		ret = PFIL_DROPPED;
#else
	    {
		void *psa;
		size_t len;
#ifdef INET
		if (args.flags & (IPFW_ARGS_NH4 | IPFW_ARGS_NH4PTR)) {
			MPASS((args.flags & (IPFW_ARGS_NH4 |
			    IPFW_ARGS_NH4PTR)) != (IPFW_ARGS_NH4 |
			    IPFW_ARGS_NH4PTR));
			MPASS((args.flags & (IPFW_ARGS_NH6 |
			    IPFW_ARGS_NH6PTR)) == 0);
			len = sizeof(struct sockaddr_in);
			psa = (args.flags & IPFW_ARGS_NH4) ?
			    &args.hopstore : args.next_hop;
			if (in_localip(satosin(psa)->sin_addr))
				(*m0)->m_flags |= M_FASTFWD_OURS;
			(*m0)->m_flags |= M_IP_NEXTHOP;
		}
#endif /* INET */
#ifdef INET6
		if (args.flags & (IPFW_ARGS_NH6 | IPFW_ARGS_NH6PTR)) {
			MPASS((args.flags & (IPFW_ARGS_NH6 |
			    IPFW_ARGS_NH6PTR)) != (IPFW_ARGS_NH6 |
			    IPFW_ARGS_NH6PTR));
			MPASS((args.flags & (IPFW_ARGS_NH4 |
			    IPFW_ARGS_NH4PTR)) == 0);
			len = sizeof(struct sockaddr_in6);
			psa = args.next_hop6;
			(*m0)->m_flags |= M_IP6_NEXTHOP;
		}
#endif /* INET6 */
		/*
		 * Incoming packets should not be tagged so we do not
		 * m_tag_find. Outgoing packets may be tagged, so we
		 * reuse the tag if present.
		 */
		tag = (dir == DIR_IN) ? NULL :
			m_tag_find(*m0, PACKET_TAG_IPFORWARD, NULL);
		if (tag != NULL) {
			m_tag_unlink(*m0, tag);
		} else {
			tag = m_tag_get(PACKET_TAG_IPFORWARD, len,
			    M_NOWAIT);
			if (tag == NULL) {
				ret = PFIL_DROPPED;
				break;
			}
		}
		if ((args.flags & IPFW_ARGS_NH6) == 0)
			bcopy(psa, tag + 1, len);
		m_tag_prepend(*m0, tag);
		ret = 0;
#ifdef INET6
		/* IPv6 next hop needs additional handling */
		if (args.flags & (IPFW_ARGS_NH6 | IPFW_ARGS_NH6PTR)) {
			struct sockaddr_in6 *sa6;

			sa6 = satosin6(tag + 1);
			if (args.flags & IPFW_ARGS_NH6) {
				sa6->sin6_family = AF_INET6;
				sa6->sin6_len = sizeof(*sa6);
				sa6->sin6_addr = args.hopstore6.sin6_addr;
				sa6->sin6_port = args.hopstore6.sin6_port;
				sa6->sin6_scope_id =
				    args.hopstore6.sin6_scope_id;
			}
			/*
			 * If nh6 address is link-local we should convert
			 * it to kernel internal form before doing any
			 * comparisons.
			 */
			if (sa6_embedscope(sa6, V_ip6_use_defzone) != 0) {
				ret = PFIL_DROPPED;
				break;
			}
			if (in6_localip(&sa6->sin6_addr))
				(*m0)->m_flags |= M_FASTFWD_OURS;
		}
#endif /* INET6 */
	    }
#endif /* INET || INET6 */
		break;

	case IP_FW_DENY:
		ret = PFIL_DROPPED;
		break;

	case IP_FW_DUMMYNET:
		if (ip_dn_io_ptr == NULL) {
			ret = PFIL_DROPPED;
			break;
		}
		MPASS(args.flags & IPFW_ARGS_REF);
		if (mtod(*m0, struct ip *)->ip_v == 4)
			(void )ip_dn_io_ptr(m0, dir, &args);
		else if (mtod(*m0, struct ip *)->ip_v == 6)
			(void )ip_dn_io_ptr(m0, dir | PROTO_IPV6, &args);
		else {
			ret = PFIL_DROPPED;
			break;
		}
		/*
		 * XXX should read the return value.
		 * dummynet normally eats the packet and sets *m0=NULL
		 * unless the packet can be sent immediately. In this
		 * case args is updated and we should re-run the
		 * check without clearing args.
		 */
		if (*m0 != NULL)
			goto again;
		ret = PFIL_CONSUMED;
		break;

	case IP_FW_TEE:
	case IP_FW_DIVERT:
		if (ip_divert_ptr == NULL) {
			ret = PFIL_DROPPED;
			break;
		}
		MPASS(args.flags & IPFW_ARGS_REF);
		(void )ipfw_divert(m0, dir, &args.rule,
			(ipfw == IP_FW_TEE) ? 1 : 0);
		/* continue processing for the original packet (tee). */
		if (*m0)
			goto again;
		ret = PFIL_CONSUMED;
		break;

	case IP_FW_NGTEE:
	case IP_FW_NETGRAPH:
		if (ng_ipfw_input_p == NULL) {
			ret = PFIL_DROPPED;
			break;
		}
		MPASS(args.flags & IPFW_ARGS_REF);
		(void )ng_ipfw_input_p(m0, dir, &args,
			(ipfw == IP_FW_NGTEE) ? 1 : 0);
		if (ipfw == IP_FW_NGTEE) /* ignore errors for NGTEE */
			goto again;	/* continue with packet */
		ret = PFIL_CONSUMED;
		break;

	case IP_FW_NAT:
		/* honor one-pass in case of successful nat */
		if (V_fw_one_pass)
			break;
		goto again;

	case IP_FW_REASS:
		goto again;		/* continue with packet */

	case IP_FW_NAT64:
		ret = PFIL_CONSUMED;
		break;

	default:
		KASSERT(0, ("%s: unknown retval", __func__));
	}

	if (ret != PFIL_PASS) {
		if (*m0)
			FREE_PKT(*m0);
		*m0 = NULL;
	}

	return (ret);
}

/*
 * ipfw processing for ethernet packets (in and out).
 */
static pfil_return_t
ipfw_check_frame(struct mbuf **m0, struct ifnet *ifp, int dir,
    void *ruleset __unused, struct inpcb *inp)
{
	struct ip_fw_args args;
	struct ether_header save_eh;
	struct ether_header *eh;
	struct m_tag *mtag;
	struct mbuf *m;
	pfil_return_t ret;
	int i;

	args.flags = IPFW_ARGS_ETHER;
again:
	/* fetch start point from rule, if any.  remove the tag if present. */
	mtag = m_tag_locate(*m0, MTAG_IPFW_RULE, 0, NULL);
	if (mtag != NULL) {
		args.rule = *((struct ipfw_rule_ref *)(mtag+1));
		m_tag_delete(*m0, mtag);
		if (args.rule.info & IPFW_ONEPASS)
			return (0);
		args.flags |= IPFW_ARGS_REF;
	}

	/* I need some amt of data to be contiguous */
	m = *m0;
	i = min(m->m_pkthdr.len, max_protohdr);
	if (m->m_len < i) {
		m = m_pullup(m, i);
		if (m == NULL) {
			*m0 = m;
			return (0);
		}
	}
	eh = mtod(m, struct ether_header *);
	save_eh = *eh;			/* save copy for restore below */
	m_adj(m, ETHER_HDR_LEN);	/* strip ethernet header */

	args.m = m;		/* the packet we are looking at		*/
	args.oif = dir & PFIL_OUT ? ifp: NULL;	/* destination, if any	*/
	args.eh = &save_eh;	/* MAC header for bridged/MAC packets	*/
	args.inp = inp;	/* used by ipfw uid/gid/jail rules	*/
	i = ipfw_chk(&args);
	m = args.m;
	if (m != NULL) {
		/*
		 * Restore Ethernet header, as needed, in case the
		 * mbuf chain was replaced by ipfw.
		 */
		M_PREPEND(m, ETHER_HDR_LEN, M_NOWAIT);
		if (m == NULL) {
			*m0 = NULL;
			return (0);
		}
		if (eh != mtod(m, struct ether_header *))
			bcopy(&save_eh, mtod(m, struct ether_header *),
				ETHER_HDR_LEN);
	}
	*m0 = m;

	ret = PFIL_PASS;
	/* Check result of ipfw_chk() */
	switch (i) {
	case IP_FW_PASS:
		break;

	case IP_FW_DENY:
		ret = PFIL_DROPPED;
		break;

	case IP_FW_DUMMYNET:
		if (ip_dn_io_ptr == NULL) {
			ret = PFIL_DROPPED;
			break;
		}
		*m0 = NULL;
		dir = (dir & PFIL_IN) ? DIR_IN : DIR_OUT;
		MPASS(args.flags & IPFW_ARGS_REF);
		ip_dn_io_ptr(&m, dir | PROTO_LAYER2, &args);
		return (PFIL_CONSUMED);

	case IP_FW_NGTEE:
	case IP_FW_NETGRAPH:
		if (ng_ipfw_input_p == NULL) {
			ret = PFIL_DROPPED;
			break;
		}
		MPASS(args.flags & IPFW_ARGS_REF);
		(void )ng_ipfw_input_p(m0, (dir & PFIL_IN) ? DIR_IN : DIR_OUT,
			&args, (i == IP_FW_NGTEE) ? 1 : 0);
		if (i == IP_FW_NGTEE) /* ignore errors for NGTEE */
			goto again;	/* continue with packet */
		ret = PFIL_CONSUMED;
		break;

	default:
		KASSERT(0, ("%s: unknown retval", __func__));
	}

	if (ret != PFIL_PASS) {
		if (*m0)
			FREE_PKT(*m0);
		*m0 = NULL;
	}

	return (ret);
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
	struct ip *ip = mtod(*m0, struct ip *);
	struct m_tag *tag;

	/* Cloning needed for tee? */
	if (tee == 0) {
		clone = *m0;	/* use the original mbuf */
		*m0 = NULL;
	} else {
		clone = m_dup(*m0, M_NOWAIT);
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
	if (!tee) switch (ip->ip_v) {
	case IPVERSION:
	    if (ntohs(ip->ip_off) & (IP_MF | IP_OFFMASK)) {
		int hlen;
		struct mbuf *reass;

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
		ip->ip_sum = 0;
		if (hlen == sizeof(struct ip))
			ip->ip_sum = in_cksum_hdr(ip);
		else
			ip->ip_sum = in_cksum(reass, hlen);
		clone = reass;
	    }
	    break;
#ifdef INET6
	case IPV6_VERSION >> 4:
	    {
	    struct ip6_hdr *const ip6 = mtod(clone, struct ip6_hdr *);

		if (ip6->ip6_nxt == IPPROTO_FRAGMENT) {
			int nxt, off;

			off = sizeof(struct ip6_hdr);
			nxt = frag6_input(&clone, &off, 0);
			if (nxt == IPPROTO_DONE)
				return (0);
		}
		break;
	    }
#endif
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
VNET_DEFINE_STATIC(pfil_hook_t, ipfw_inet_hook);
#define	V_ipfw_inet_hook	VNET(ipfw_inet_hook)
#ifdef INET6
VNET_DEFINE_STATIC(pfil_hook_t, ipfw_inet6_hook);
#define	V_ipfw_inet6_hook	VNET(ipfw_inet6_hook)
#endif
VNET_DEFINE_STATIC(pfil_hook_t, ipfw_link_hook);
#define	V_ipfw_link_hook	VNET(ipfw_link_hook)

static int
ipfw_hook(int onoff, int pf)
{
	struct pfil_hook_args pha;
	struct pfil_link_args pla;
	pfil_hook_t *h;

	pha.pa_version = PFIL_VERSION;
	pha.pa_flags = PFIL_IN | PFIL_OUT;
	pha.pa_modname = "ipfw";
	pha.pa_ruleset = NULL;

	pla.pa_version = PFIL_VERSION;
	pla.pa_flags = PFIL_IN | PFIL_OUT |
	    PFIL_HEADPTR | PFIL_HOOKPTR;

	switch (pf) {
	case AF_INET:
		pha.pa_func = ipfw_check_packet;
		pha.pa_type = PFIL_TYPE_IP4;
		pha.pa_rulname = "default";
		h = &V_ipfw_inet_hook;
		pla.pa_head = V_inet_pfil_head;
		break;
#ifdef INET6
	case AF_INET6:
		pha.pa_func = ipfw_check_packet;
		pha.pa_type = PFIL_TYPE_IP6;
		pha.pa_rulname = "default6";
		h = &V_ipfw_inet6_hook;
		pla.pa_head = V_inet6_pfil_head;
		break;
#endif
	case AF_LINK:
		pha.pa_func = ipfw_check_frame;
		pha.pa_type = PFIL_TYPE_ETHERNET;
		pha.pa_rulname = "default-link";
		h = &V_ipfw_link_hook;
		pla.pa_head = V_link_pfil_head;
		break;
	}

	if (onoff) {
		*h = pfil_add_hook(&pha);
		pla.pa_hook = *h;
		(void)pfil_link(&pla);
	} else
		if (*h != NULL)
			pfil_remove_hook(*h);

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
	if (arg == 0) /* detach */
		ipfw_hook(0, AF_LINK);
	else if (V_fwlink_enable && ipfw_hook(1, AF_LINK) != 0) {
                error = ENOENT;
                printf("ipfw_link_hook() error\n");
        }
	return error;
}

int
ipfw_chg_hook(SYSCTL_HANDLER_ARGS)
{
	int newval;
	int error;
	int af;

	if (arg1 == &V_fw_enable)
		af = AF_INET;
#ifdef INET6
	else if (arg1 == &V_fw6_enable)
		af = AF_INET6;
#endif
	else if (arg1 == &V_fwlink_enable)
		af = AF_LINK;
	else 
		return (EINVAL);

	newval = *(int *)arg1;
	/* Handle sysctl change */
	error = sysctl_handle_int(oidp, &newval, 0, req);

	if (error)
		return (error);

	/* Formalize new value */
	newval = (newval) ? 1 : 0;

	if (*(int *)arg1 == newval)
		return (0);

	error = ipfw_hook(newval, af);
	if (error)
		return (error);
	*(int *)arg1 = newval;

	return (0);
}
/* end of file */
