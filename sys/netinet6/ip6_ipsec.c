/*-
 * Copyright (c) 1982, 1986, 1988, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#include "opt_inet.h"
#include "opt_sctp.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#ifdef SCTP
#include <netinet/sctp_crc32.h>
#endif

#include <machine/in_cksum.h>

#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#include <netipsec/xform.h>
#include <netipsec/key.h>
#ifdef IPSEC_DEBUG
#include <netipsec/key_debug.h>
#else
#define	KEYDEBUG(lev,arg)
#endif

#include <netinet6/ip6_ipsec.h>
#include <netinet6/ip6_var.h>

extern	struct protosw inet6sw[];

static VNET_DEFINE(int, ip6_ipsec6_filtertunnel) = 0;
#define	V_ip6_ipsec6_filtertunnel	VNET(ip6_ipsec6_filtertunnel)

SYSCTL_DECL(_net_inet6_ipsec6);
SYSCTL_INT(_net_inet6_ipsec6, OID_AUTO, filtertunnel,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip6_ipsec6_filtertunnel),  0,
	"If set filter packets from an IPsec tunnel.");

/*
 * Check if we have to jump over firewall processing for this packet.
 * Called from ip6_input().
 * 1 = jump over firewall, 0 = packet goes through firewall.
 */
int
ip6_ipsec_filtertunnel(struct mbuf *m)
{

	/*
	 * Bypass packet filtering for packets previously handled by IPsec.
	 */
	if (!V_ip6_ipsec6_filtertunnel &&
	    m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL) != NULL)
		return (1);
	return (0);
}

/*
 * Check if protocol type doesn't have a further header and do IPSEC
 * decryption or reject right now.  Protocols with further headers get
 * their IPSEC treatment within the protocol specific processing.
 * Called from ip6_input().
 * 1 = drop packet, 0 = continue processing packet.
 */
int
ip6_ipsec_input(struct mbuf *m, int nxt)
{

	/*
	 * enforce IPsec policy checking if we are seeing last header.
	 * note that we do not visit this with protocols with pcb layer
	 * code - like udp/tcp/raw ip.
	 */
	if ((inet6sw[ip6_protox[nxt]].pr_flags & PR_LASTHDR) != 0)
		return (ipsec6_in_reject(m, NULL));
	return (0);
}

/*
 * Called from ip6_output().
 * 0 = continue processing packet
 * 1 = packet was consumed, stop processing
 */
int
ip6_ipsec_output(struct mbuf *m, struct inpcb *inp, int *error)
{
	struct secpolicy *sp;
	/*
	 * Check the security policy (SP) for the packet and, if
	 * required, do IPsec-related processing.  There are two
	 * cases here; the first time a packet is sent through
	 * it will be untagged and handled by ipsec4_checkpolicy.
	 * If the packet is resubmitted to ip6_output (e.g. after
	 * AH, ESP, etc. processing), there will be a tag to bypass
	 * the lookup and related policy checking.
	 */
	*error = 0;
	if (m_tag_find(m, PACKET_TAG_IPSEC_OUT_DONE, NULL) != NULL)
		return (0);
	sp = ipsec6_checkpolicy(m, inp, error);
	/*
	 * There are four return cases:
	 *    sp != NULL		    apply IPsec policy
	 *    sp == NULL, error == 0	    no IPsec handling needed
	 *    sp == NULL, error == -EINVAL  discard packet w/o error
	 *    sp == NULL, error != 0	    discard packet, report error
	 */
	if (sp != NULL) {
		/*
		 * Do delayed checksums now because we send before
		 * this is done in the normal processing path.
		 */
#ifdef INET
		if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
			in_delayed_cksum(m);
			m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
		}
#endif
		if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA_IPV6) {
			in6_delayed_cksum(m, m->m_pkthdr.len -
			    sizeof(struct ip6_hdr), sizeof(struct ip6_hdr));
			m->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA_IPV6;
		}
#ifdef SCTP
		if (m->m_pkthdr.csum_flags & CSUM_SCTP_IPV6) {
			sctp_delayed_cksum(m, sizeof(struct ip6_hdr));
			m->m_pkthdr.csum_flags &= ~CSUM_SCTP_IPV6;
		}
#endif

		/* NB: callee frees mbuf and releases reference to SP */
		*error = ipsec6_process_packet(m, sp, inp);
		if (*error == EJUSTRETURN) {
			/*
			 * We had a SP with a level of 'use' and no SA. We
			 * will just continue to process the packet without
			 * IPsec processing and return without error.
			 */
			*error = 0;
			return (0);
		}
		return (1);	/* mbuf consumed by IPsec */
	} else {	/* sp == NULL */
		if (*error != 0) {
			/*
			 * Hack: -EINVAL is used to signal that a packet
			 * should be silently discarded.  This is typically
			 * because we have DISCARD policy or asked key
			 * management for an SP and it was delayed (e.g.
			 * kicked up to IKE).
			 * XXX: maybe return EACCES to the caller would
			 *      be more useful?
			 */
			if (*error == -EINVAL)
				*error = 0;
			m_freem(m);
			return (1);
		}
		/* No IPsec processing for this packet. */
	}
	return (0);
}

/*
 * Called from ip6_forward().
 * 1 = drop packet, 0 = forward packet.
 */
int
ip6_ipsec_forward(struct mbuf *m, int *error)
{
	struct secpolicy *sp;
	int idx;

	/*
	 * Check if this packet has an active inbound SP and needs to be
	 * dropped instead of forwarded.
	 */
	if (ipsec6_in_reject(m, NULL) != 0) {
		*error = EACCES;
		return (0);
	}
	/*
	 * Now check outbound SP.
	 */
	sp = ipsec6_checkpolicy(m, NULL, error);
	/*
	 * There are four return cases:
	 *    sp != NULL		    apply IPsec policy
	 *    sp == NULL, error == 0	    no IPsec handling needed
	 *    sp == NULL, error == -EINVAL  discard packet w/o error
	 *    sp == NULL, error != 0	    discard packet, report error
	 */
	if (sp != NULL) {
		/*
		 * We have SP with IPsec transform, but we should check that
		 * it has tunnel mode request, because we can't use transport
		 * mode when forwarding.
		 *
		 * RFC2473 says:
		 * "A tunnel IPv6 packet resulting from the encapsulation of
		 * an original packet is considered an IPv6 packet originating
		 * from the tunnel entry-point node."
		 * So, we don't need MTU checking, after IPsec processing
		 * we will just fragment it if needed.
		 */
		for (idx = 0; idx < sp->tcount; idx++) {
			if (sp->req[idx]->saidx.mode == IPSEC_MODE_TUNNEL)
				break;
		}
		if (idx == sp->tcount) {
			*error = EACCES;
			IPSEC6STAT_INC(ips_out_inval);
			key_freesp(&sp);
			return (0);
		}
		/* NB: callee frees mbuf and releases reference to SP */
		*error = ipsec6_process_packet(m, sp, NULL);
		if (*error == EJUSTRETURN) {
			/*
			 * We had a SP with a level of 'use' and no SA. We
			 * will just continue to process the packet without
			 * IPsec processing and return without error.
			 */
			*error = 0;
			return (0);
		}
		return (1);	/* mbuf consumed by IPsec */
	} else {	/* sp == NULL */
		if (*error != 0) {
			/*
			 * Hack: -EINVAL is used to signal that a packet
			 * should be silently discarded.  This is typically
			 * because we have DISCARD policy or asked key
			 * management for an SP and it was delayed (e.g.
			 * kicked up to IKE).
			 * XXX: maybe return EACCES to the caller would
			 *      be more useful?
			 */
			if (*error == -EINVAL)
				*error = 0;
			m_freem(m);
			return (1);
		}
		/* No IPsec processing for this packet. */
	}
	return (0);
}
