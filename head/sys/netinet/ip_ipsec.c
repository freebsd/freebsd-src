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

#include "opt_ipsec.h"
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#include <netinet/ip_ipsec.h>
#ifdef SCTP
#include <netinet/sctp_crc32.h>
#endif

#include <machine/in_cksum.h>

#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#include <netipsec/key.h>

extern	struct protosw inetsw[];

#ifdef IPSEC_FILTERTUNNEL
static VNET_DEFINE(int, ip4_ipsec_filtertunnel) = 1;
#else
static VNET_DEFINE(int, ip4_ipsec_filtertunnel) = 0;
#endif
#define	V_ip4_ipsec_filtertunnel VNET(ip4_ipsec_filtertunnel)

SYSCTL_DECL(_net_inet_ipsec);
SYSCTL_INT(_net_inet_ipsec, OID_AUTO, filtertunnel,
	CTLFLAG_VNET | CTLFLAG_RW, &VNET_NAME(ip4_ipsec_filtertunnel), 0,
	"If set filter packets from an IPsec tunnel.");

/*
 * Check if we have to jump over firewall processing for this packet.
 * Called from ip_input().
 * 1 = jump over firewall, 0 = packet goes through firewall.
 */
int
ip_ipsec_filtertunnel(struct mbuf *m)
{

	/*
	 * Bypass packet filtering for packets previously handled by IPsec.
	 */
	if (!V_ip4_ipsec_filtertunnel &&
	    m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL) != NULL)
		return 1;
	return 0;
}

/*
 * Check if this packet has an active SA and needs to be dropped instead
 * of forwarded.
 * Called from ip_forward().
 * 1 = drop packet, 0 = forward packet.
 */
int
ip_ipsec_fwd(struct mbuf *m)
{

	return (ipsec4_in_reject(m, NULL));
}

/*
 * Check if protocol type doesn't have a further header and do IPSEC
 * decryption or reject right now.  Protocols with further headers get
 * their IPSEC treatment within the protocol specific processing.
 * Called from ip_input().
 * 1 = drop packet, 0 = continue processing packet.
 */
int
ip_ipsec_input(struct mbuf *m, int nxt)
{
	/*
	 * enforce IPsec policy checking if we are seeing last header.
	 * note that we do not visit this with protocols with pcb layer
	 * code - like udp/tcp/raw ip.
	 */
	if ((inetsw[ip_protox[nxt]].pr_flags & PR_LASTHDR) != 0)
		return (ipsec4_in_reject(m, NULL));
	return (0);
}

/*
 * Compute the MTU for a forwarded packet that gets IPSEC encapsulated.
 * Called from ip_forward().
 * Returns MTU suggestion for ICMP needfrag reply.
 */
int
ip_ipsec_mtu(struct mbuf *m, int mtu)
{
	/*
	 * If the packet is routed over IPsec tunnel, tell the
	 * originator the tunnel MTU.
	 *	tunnel MTU = if MTU - sizeof(IP) - ESP/AH hdrsiz
	 * XXX quickhack!!!
	 */
	return (mtu - ipsec_hdrsiz(m, IPSEC_DIR_OUTBOUND, NULL));
}

/*
 * 
 * Called from ip_output().
 * 1 = drop packet, 0 = continue processing packet,
 * -1 = packet was reinjected and stop processing packet
 */
int
ip_ipsec_output(struct mbuf **m, struct inpcb *inp, int *error)
{
	struct secpolicy *sp;
	/*
	 * Check the security policy (SP) for the packet and, if
	 * required, do IPsec-related processing.  There are two
	 * cases here; the first time a packet is sent through
	 * it will be untagged and handled by ipsec4_checkpolicy.
	 * If the packet is resubmitted to ip_output (e.g. after
	 * AH, ESP, etc. processing), there will be a tag to bypass
	 * the lookup and related policy checking.
	 */
	if (m_tag_find(*m, PACKET_TAG_IPSEC_OUT_DONE, NULL) != NULL) {
		*error = 0;
		return (0);
	}
	sp = ipsec4_checkpolicy(*m, IPSEC_DIR_OUTBOUND, error, inp);
	/*
	 * There are four return cases:
	 *    sp != NULL	 	    apply IPsec policy
	 *    sp == NULL, error == 0	    no IPsec handling needed
	 *    sp == NULL, error == -EINVAL  discard packet w/o error
	 *    sp == NULL, error != 0	    discard packet, report error
	 */
	if (sp != NULL) {
		/*
		 * Do delayed checksums now because we send before
		 * this is done in the normal processing path.
		 */
		if ((*m)->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
			in_delayed_cksum(*m);
			(*m)->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
		}
#ifdef SCTP
		if ((*m)->m_pkthdr.csum_flags & CSUM_SCTP) {
			struct ip *ip = mtod(*m, struct ip *);

			sctp_delayed_cksum(*m, (uint32_t)(ip->ip_hl << 2));
			(*m)->m_pkthdr.csum_flags &= ~CSUM_SCTP;
		}
#endif

		/* NB: callee frees mbuf */
		*error = ipsec4_process_packet(*m, sp->req);
		KEY_FREESP(&sp);
		if (*error == EJUSTRETURN) {
			/*
			 * We had a SP with a level of 'use' and no SA. We
			 * will just continue to process the packet without
			 * IPsec processing and return without error.
			 */
			*error = 0;
			goto done;
		}
		/*
		 * Preserve KAME behaviour: ENOENT can be returned
		 * when an SA acquire is in progress.  Don't propagate
		 * this to user-level; it confuses applications.
		 *
		 * XXX this will go away when the SADB is redone.
		 */
		if (*error == ENOENT)
			*error = 0;
		goto reinjected;
	} else {	/* sp == NULL */

		if (*error != 0) {
			/*
			 * Hack: -EINVAL is used to signal that a packet
			 * should be silently discarded.  This is typically
			 * because we asked key management for an SA and
			 * it was delayed (e.g. kicked up to IKE).
			 */
			if (*error == -EINVAL)
				*error = 0;
			goto bad;
		}
		/* No IPsec processing for this packet. */
	}
done:
	return (0);
reinjected:
	return (-1);
bad:
	if (sp != NULL)
		KEY_FREESP(&sp);
	return 1;
}
