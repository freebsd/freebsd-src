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
 *
 * $FreeBSD$
 */

#if !defined(KLD_MODULE)
#include "opt_ipfw.h"
#include "opt_ipdn.h"
#include "opt_ipdivert.h"
#include "opt_inet.h"
#ifndef INET
#error IPFIREWALL requires INET.
#endif /* INET */
#endif /* KLD_MODULE */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>

#include <net/if.h>
#include <net/route.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_fw.h>
#include <netinet/ip_divert.h>
#include <netinet/ip_dummynet.h>

#include <machine/in_cksum.h>

static	int ipfw_pfil_hooked = 0;

/* Dummynet hooks. */
ip_dn_ruledel_t	*ip_dn_ruledel_ptr = NULL;

#define	DIV_DIR_IN	1
#define	DIV_DIR_OUT	0

static int	ipfw_divert(struct mbuf **, int, int);

int
ipfw_check_in(void *arg, struct mbuf **m0, struct ifnet *ifp, int dir,
    struct inpcb *inp)
{
	struct ip_fw_args args;
	struct m_tag *dn_tag;
	int ipfw = 0;
	int divert;
#ifdef IPFIREWALL_FORWARD
	struct m_tag *fwd_tag;
#endif

	KASSERT(dir == PFIL_IN, ("ipfw_check_in wrong direction!"));

	if (!fw_enable)
		goto pass;

	bzero(&args, sizeof(args));

	dn_tag = m_tag_find(*m0, PACKET_TAG_DUMMYNET, NULL);
	if (dn_tag != NULL){
		struct dn_pkt_tag *dt;

		dt = (struct dn_pkt_tag *)(dn_tag+1);
		args.rule = dt->rule;

		m_tag_delete(*m0, dn_tag);
	}

again:
	args.m = *m0;
	args.inp = inp;
	ipfw = ipfw_chk(&args);
	*m0 = args.m;

	if ((ipfw & IP_FW_PORT_DENY_FLAG) || *m0 == NULL)
		goto drop;

	if (ipfw == 0 && args.next_hop == NULL)
		goto pass;

	if (DUMMYNET_LOADED && (ipfw & IP_FW_PORT_DYNT_FLAG) != 0) {
		ip_dn_io_ptr(*m0, ipfw & 0xffff, DN_TO_IP_IN, &args);
		*m0 = NULL;
		return 0;		/* packet consumed */
	}

	if (ipfw != 0 && (ipfw & IP_FW_PORT_DYNT_FLAG) == 0) {
		if ((ipfw & IP_FW_PORT_TEE_FLAG) != 0)
			divert = ipfw_divert(m0, DIV_DIR_IN, 1);
		else
			divert = ipfw_divert(m0, DIV_DIR_IN, 0);

		/* tee should continue again with the firewall. */
		if (divert) {
			*m0 = NULL;
			return 0;	/* packet consumed */
		} else {
			args.rule = NULL;
			goto again;	/* continue with packet */
		}
	}

#ifdef IPFIREWALL_FORWARD
	if (ipfw == 0 && args.next_hop != NULL) {
		fwd_tag = m_tag_get(PACKET_TAG_IPFORWARD,
				sizeof(struct sockaddr_in), M_NOWAIT);
		if (fwd_tag == NULL)
			goto drop;
		bcopy(args.next_hop, (fwd_tag+1), sizeof(struct sockaddr_in));
		m_tag_prepend(*m0, fwd_tag);

		if (in_localip(args.next_hop->sin_addr))
			(*m0)->m_flags |= M_FASTFWD_OURS;
		goto pass;
	}
#endif

drop:
	if (*m0)
		m_freem(*m0);
	*m0 = NULL;
	return (EACCES);
pass:
	return 0;	/* not filtered */
}

int
ipfw_check_out(void *arg, struct mbuf **m0, struct ifnet *ifp, int dir,
    struct inpcb *inp)
{
	struct ip_fw_args args;
	struct m_tag *dn_tag;
	int ipfw = 0;
	int divert;
#ifdef IPFIREWALL_FORWARD
	struct m_tag *fwd_tag;
#endif

	KASSERT(dir == PFIL_OUT, ("ipfw_check_out wrong direction!"));

	if (!fw_enable)
		goto pass;

	bzero(&args, sizeof(args));

	dn_tag = m_tag_find(*m0, PACKET_TAG_DUMMYNET, NULL);
	if (dn_tag != NULL) {
		struct dn_pkt_tag *dt;

		dt = (struct dn_pkt_tag *)(dn_tag+1);
		args.rule = dt->rule;

		m_tag_delete(*m0, dn_tag);
	}

again:
	args.m = *m0;
	args.oif = ifp;
	args.inp = inp;
	ipfw = ipfw_chk(&args);
	*m0 = args.m;

	if ((ipfw & IP_FW_PORT_DENY_FLAG) || *m0 == NULL)
		goto drop;

	if (ipfw == 0 && args.next_hop == NULL)
		goto pass;

	if (DUMMYNET_LOADED && (ipfw & IP_FW_PORT_DYNT_FLAG) != 0) {
		ip_dn_io_ptr(*m0, ipfw & 0xffff, DN_TO_IP_OUT, &args);
		*m0 = NULL;
		return 0;		/* packet consumed */
	}

	if (ipfw != 0 && (ipfw & IP_FW_PORT_DYNT_FLAG) == 0) {
		if ((ipfw & IP_FW_PORT_TEE_FLAG) != 0)
			divert = ipfw_divert(m0, DIV_DIR_OUT, 1);
		else
			divert = ipfw_divert(m0, DIV_DIR_OUT, 0);

		if (divert) {
			*m0 = NULL;
			return 0;	/* packet consumed */
		} else {
			args.rule = NULL;
			goto again;	/* continue with packet */
		}
        }

#ifdef IPFIREWALL_FORWARD
	if (ipfw == 0 && args.next_hop != NULL) {
		/* Overwrite existing tag. */
		fwd_tag = m_tag_find(*m0, PACKET_TAG_IPFORWARD, NULL);
		if (fwd_tag == NULL) {
			fwd_tag = m_tag_get(PACKET_TAG_IPFORWARD,
				sizeof(struct sockaddr_in), M_NOWAIT);
			if (fwd_tag == NULL)
				goto drop;
		} else
			m_tag_unlink(*m0, fwd_tag);
		bcopy(args.next_hop, (fwd_tag+1), sizeof(struct sockaddr_in));
		m_tag_prepend(*m0, fwd_tag);

		if (in_localip(args.next_hop->sin_addr))
			(*m0)->m_flags |= M_FASTFWD_OURS;
		goto pass;
	}
#endif

drop:
	if (*m0)
		m_freem(*m0);
	*m0 = NULL;
	return (EACCES);
pass:
	return 0;	/* not filtered */
}

static int
ipfw_divert(struct mbuf **m, int incoming, int tee)
{
	/*
	 * ipfw_chk() has already tagged the packet with the divert tag.
	 * If tee is set, copy packet and return original.
	 * If not tee, consume packet and send it to divert socket.
	 */
#ifdef IPDIVERT
	struct mbuf *clone, *reass;
	struct ip *ip;
	int hlen;

	reass = NULL;

	/* Cloning needed for tee? */
	if (tee)
		clone = m_dup(*m, M_DONTWAIT);
	else
		clone = *m;

	/* In case m_dup was unable to allocate mbufs. */
	if (clone == NULL)
		goto teeout;

	/*
	 * Divert listeners can only handle non-fragmented packets.
	 * However when tee is set we will *not* de-fragment the packets;
	 * Doing do would put the reassembly into double-jeopardy.  On top
	 * of that someone doing a tee will probably want to get the packet
	 * in its original form.
	 */
	ip = mtod(clone, struct ip *);
	if (!tee && ip->ip_off & (IP_MF | IP_OFFMASK)) {

		/* Reassemble packet. */
		reass = ip_reass(clone);

		/*
		 * IP header checksum fixup after reassembly and leave header
		 * in network byte order.
		 */
		if (reass != NULL) {
			ip = mtod(reass, struct ip *);
			hlen = ip->ip_hl << 2;
			ip->ip_len = htons(ip->ip_len);
			ip->ip_off = htons(ip->ip_off);
			ip->ip_sum = 0;
			if (hlen == sizeof(struct ip))
				ip->ip_sum = in_cksum_hdr(ip);
			else
				ip->ip_sum = in_cksum(reass, hlen);
			clone = reass;
		} else
			clone = NULL;
	} else {
		/* Convert header to network byte order. */
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
	}

	/* Do the dirty job... */
	if (clone)
		divert_packet(clone, incoming);

teeout:
	/*
	 * For tee we leave the divert tag attached to original packet.
	 * It will then continue rule evaluation after the tee rule.
	 */
	if (tee)
		return 0;

	/* Packet diverted and consumed */
	return 1;
#else
	m_freem(*m);
	return 1;
#endif	/* ipdivert */
}

static int
ipfw_hook(void)
{
	struct pfil_head *pfh_inet;

	if (ipfw_pfil_hooked)
		return EEXIST;

	pfh_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
	if (pfh_inet == NULL)
		return ENOENT;

	pfil_add_hook(ipfw_check_in, NULL, PFIL_IN | PFIL_WAITOK, pfh_inet);
	pfil_add_hook(ipfw_check_out, NULL, PFIL_OUT | PFIL_WAITOK, pfh_inet);

	return 0;
}

static int
ipfw_unhook(void)
{
	struct pfil_head *pfh_inet;

	if (!ipfw_pfil_hooked)
		return ENOENT;

	pfh_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
	if (pfh_inet == NULL)
		return ENOENT;

	pfil_remove_hook(ipfw_check_in, NULL, PFIL_IN | PFIL_WAITOK, pfh_inet);
	pfil_remove_hook(ipfw_check_out, NULL, PFIL_OUT | PFIL_WAITOK, pfh_inet);

	return 0;
}

static int
ipfw_modevent(module_t mod, int type, void *unused)
{
	int err = 0;

	switch (type) {
	case MOD_LOAD:
		if (ipfw_pfil_hooked) {
			printf("IP firewall already loaded\n");
			err = EEXIST;
		} else {
			if ((err = ipfw_init()) != 0) {
				printf("ipfw_init() error\n");
				break;
			}
			if ((err = ipfw_hook()) != 0) {
				printf("ipfw_hook() error\n");
				break;
			}
			ipfw_pfil_hooked = 1;
		}
		break;

	case MOD_UNLOAD:
		if (ipfw_pfil_hooked) {
			if ((err = ipfw_unhook()) > 0)
				break;
			ipfw_destroy();
			ipfw_pfil_hooked = 0;
		} else {
			printf("IP firewall already unloaded\n");
		}
		break;

	default:
		return EOPNOTSUPP;
		break;
	}
	return err;
}

static moduledata_t ipfwmod = {
	"ipfw",
	ipfw_modevent,
	0
};
DECLARE_MODULE(ipfw, ipfwmod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);
MODULE_VERSION(ipfw, 2);
