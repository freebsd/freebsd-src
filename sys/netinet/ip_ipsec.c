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
#include <net/route.h>
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

#ifdef IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#include <netipsec/key.h>
#endif /*IPSEC*/

extern	struct protosw inetsw[];

#ifdef IPSEC
#ifdef IPSEC_FILTERTUNNEL
static VNET_DEFINE(int, ip4_ipsec_filtertunnel) = 1;
#else
static VNET_DEFINE(int, ip4_ipsec_filtertunnel) = 0;
#endif
#define	V_ip4_ipsec_filtertunnel VNET(ip4_ipsec_filtertunnel)

SYSCTL_DECL(_net_inet_ipsec);
SYSCTL_VNET_INT(_net_inet_ipsec, OID_AUTO, filtertunnel,
	CTLFLAG_RW, &VNET_NAME(ip4_ipsec_filtertunnel), 0,
	"If set filter packets from an IPsec tunnel.");
#endif /* IPSEC */

/*
 * Check if we have to jump over firewall processing for this packet.
 * Called from ip_input().
 * 1 = jump over firewall, 0 = packet goes through firewall.
 */
int
ip_ipsec_filtertunnel(struct mbuf *m)
{
#ifdef IPSEC

	/*
	 * Bypass packet filtering for packets previously handled by IPsec.
	 */
	if (!V_ip4_ipsec_filtertunnel &&
	    m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL) != NULL)
		return 1;
#endif
	return 0;
}

/*
 * Check if this packet has an active SA and needs to be dropped instead
 * of forwarded.
 * Called from ip_input().
 * 1 = drop packet, 0 = forward packet.
 */
int
ip_ipsec_fwd(struct mbuf *m)
{
#ifdef IPSEC
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secpolicy *sp;
	int error;

	mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
	if (mtag != NULL) {
		tdbi = (struct tdb_ident *)(mtag + 1);
		sp = ipsec_getpolicy(tdbi, IPSEC_DIR_INBOUND);
	} else {
		sp = ipsec_getpolicybyaddr(m, IPSEC_DIR_INBOUND,
					   IP_FORWARDING, &error);   
	}
	if (sp == NULL) {	/* NB: can happen if error */
		/*XXX error stat???*/
		DPRINTF(("ip_input: no SP for forwarding\n"));	/*XXX*/
		return 1;
	}

	/*
	 * Check security policy against packet attributes.
	 */
	error = ipsec_in_reject(sp, m);
	KEY_FREESP(&sp);
	if (error) {
		IPSTAT_INC(ips_cantforward);
		return 1;
	}
#endif /* IPSEC */
	return 0;
}

/*
 * Check if protocol type doesn't have a further header and do IPSEC
 * decryption or reject right now.  Protocols with further headers get
 * their IPSEC treatment within the protocol specific processing.
 * Called from ip_input().
 * 1 = drop packet, 0 = continue processing packet.
 */
int
ip_ipsec_input(struct mbuf *m)
{
#ifdef IPSEC
	struct ip *ip = mtod(m, struct ip *);
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secpolicy *sp;
	int error;
	/*
	 * enforce IPsec policy checking if we are seeing last header.
	 * note that we do not visit this with protocols with pcb layer
	 * code - like udp/tcp/raw ip.
	 */
	if ((inetsw[ip_protox[ip->ip_p]].pr_flags & PR_LASTHDR) != 0) {
		/*
		 * Check if the packet has already had IPsec processing
		 * done.  If so, then just pass it along.  This tag gets
		 * set during AH, ESP, etc. input handling, before the
		 * packet is returned to the ip input queue for delivery.
		 */ 
		mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
		if (mtag != NULL) {
			tdbi = (struct tdb_ident *)(mtag + 1);
			sp = ipsec_getpolicy(tdbi, IPSEC_DIR_INBOUND);
		} else {
			sp = ipsec_getpolicybyaddr(m, IPSEC_DIR_INBOUND,
						   IP_FORWARDING, &error);   
		}
		if (sp != NULL) {
			/*
			 * Check security policy against packet attributes.
			 */
			error = ipsec_in_reject(sp, m);
			KEY_FREESP(&sp);
		} else {
			/* XXX error stat??? */
			error = EINVAL;
			DPRINTF(("ip_input: no SP, packet discarded\n"));/*XXX*/
			return 1;
		}
		if (error)
			return 1;
	}
#endif /* IPSEC */
	return 0;
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
	struct secpolicy *sp = NULL;
	int ipsecerror;
	int ipsechdr;
	struct route *ro;
	sp = ipsec_getpolicybyaddr(m,
				   IPSEC_DIR_OUTBOUND,
				   IP_FORWARDING,
				   &ipsecerror);
	if (sp != NULL) {
		/* count IPsec header size */
		ipsechdr = ipsec_hdrsiz(m, IPSEC_DIR_OUTBOUND, NULL);

		/*
		 * find the correct route for outer IPv4
		 * header, compute tunnel MTU.
		 */
		if (sp->req != NULL &&
		    sp->req->sav != NULL &&
		    sp->req->sav->sah != NULL) {
			ro = &sp->req->sav->sah->route_cache.sa_route;
			if (ro->ro_rt && ro->ro_rt->rt_ifp) {
				mtu =
				    ro->ro_rt->rt_rmx.rmx_mtu ?
				    ro->ro_rt->rt_rmx.rmx_mtu :
				    ro->ro_rt->rt_ifp->if_mtu;
				mtu -= ipsechdr;
			}
		}
		KEY_FREESP(&sp);
	}
	return mtu;
}

/*
 * 
 * Called from ip_output().
 * 1 = drop packet, 0 = continue processing packet,
 * -1 = packet was reinjected and stop processing packet
 */
int
ip_ipsec_output(struct mbuf **m, struct inpcb *inp, int *flags, int *error)
{
#ifdef IPSEC
	struct secpolicy *sp = NULL;
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
	/*
	 * Check the security policy (SP) for the packet and, if
	 * required, do IPsec-related processing.  There are two
	 * cases here; the first time a packet is sent through
	 * it will be untagged and handled by ipsec4_checkpolicy.
	 * If the packet is resubmitted to ip_output (e.g. after
	 * AH, ESP, etc. processing), there will be a tag to bypass
	 * the lookup and related policy checking.
	 */
	mtag = m_tag_find(*m, PACKET_TAG_IPSEC_PENDING_TDB, NULL);
	if (mtag != NULL) {
		tdbi = (struct tdb_ident *)(mtag + 1);
		sp = ipsec_getpolicy(tdbi, IPSEC_DIR_OUTBOUND);
		if (sp == NULL)
			*error = -EINVAL;	/* force silent drop */
		m_tag_delete(*m, mtag);
	} else {
		sp = ipsec4_checkpolicy(*m, IPSEC_DIR_OUTBOUND, *flags,
					error, inp);
	}
	/*
	 * There are four return cases:
	 *    sp != NULL	 	    apply IPsec policy
	 *    sp == NULL, error == 0	    no IPsec handling needed
	 *    sp == NULL, error == -EINVAL  discard packet w/o error
	 *    sp == NULL, error != 0	    discard packet, report error
	 */
	if (sp != NULL) {
		/* Loop detection, check if ipsec processing already done */
		KASSERT(sp->req != NULL, ("ip_output: no ipsec request"));
		for (mtag = m_tag_first(*m); mtag != NULL;
		     mtag = m_tag_next(*m, mtag)) {
			if (mtag->m_tag_cookie != MTAG_ABI_COMPAT)
				continue;
			if (mtag->m_tag_id != PACKET_TAG_IPSEC_OUT_DONE &&
			    mtag->m_tag_id != PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED)
				continue;
			/*
			 * Check if policy has an SA associated with it.
			 * This can happen when an SP has yet to acquire
			 * an SA; e.g. on first reference.  If it occurs,
			 * then we let ipsec4_process_packet do its thing.
			 */
			if (sp->req->sav == NULL)
				break;
			tdbi = (struct tdb_ident *)(mtag + 1);
			if (tdbi->spi == sp->req->sav->spi &&
			    tdbi->proto == sp->req->sav->sah->saidx.proto &&
			    bcmp(&tdbi->dst, &sp->req->sav->sah->saidx.dst,
				 sizeof (union sockaddr_union)) == 0) {
				/*
				 * No IPsec processing is needed, free
				 * reference to SP.
				 *
				 * NB: null pointer to avoid free at
				 *     done: below.
				 */
				KEY_FREESP(&sp), sp = NULL;
				goto done;
			}
		}

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
		*error = ipsec4_process_packet(*m, sp->req, *flags, 0);
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
		} else {
			/* No IPsec processing for this packet. */
		}
	}
done:
	if (sp != NULL)
		KEY_FREESP(&sp);
	return 0;
reinjected:
	if (sp != NULL)
		KEY_FREESP(&sp);
	return -1;
bad:
	if (sp != NULL)
		KEY_FREESP(&sp);
	return 1;
#endif /* IPSEC */
	return 0;
}
