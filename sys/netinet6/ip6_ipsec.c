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
 *
 * $FreeBSD$
 */

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

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>

#include <machine/in_cksum.h>

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#include <netipsec/xform.h>
#include <netipsec/key.h>
#ifdef IPSEC_DEBUG
#include <netipsec/key_debug.h>
#else
#define	KEYDEBUG(lev,arg)
#endif
#endif /*FAST_IPSEC*/

#include <netinet6/ip6_ipsec.h>

extern	struct protosw inet6sw[];

/*
 * Check if we have to jump over firewall processing for this packet.
 * Called from ip_input().
 * 1 = jump over firewall, 0 = packet goes through firewall.
 */
int
ip6_ipsec_filtergif(struct mbuf *m)
{
#if defined(FAST_IPSEC) && !defined(IPSEC_FILTERGIF)
	/*
	 * Bypass packet filtering for packets from a tunnel (gif).
	 */
	if (m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL) != NULL)
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
ip6_ipsec_fwd(struct mbuf *m)
{
#ifdef FAST_IPSEC
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secpolicy *sp;
	int s, error;
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
	s = splnet();
	if (mtag != NULL) {
		tdbi = (struct tdb_ident *)(mtag + 1);
		sp = ipsec_getpolicy(tdbi, IPSEC_DIR_INBOUND);
	} else {
		sp = ipsec_getpolicybyaddr(m, IPSEC_DIR_INBOUND,
					   IP_FORWARDING, &error);   
	}
	if (sp == NULL) {	/* NB: can happen if error */
		splx(s);
		/*XXX error stat???*/
		DPRINTF(("ip_input: no SP for forwarding\n"));	/*XXX*/
		return 1;
	}

	/*
	 * Check security policy against packet attributes.
	 */
	error = ipsec_in_reject(sp, m);
	KEY_FREESP(&sp);
	splx(s);
	if (error) {
		ipstat.ips_cantforward++;
		return 1;
	}
#endif /* FAST_IPSEC */
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
ip6_ipsec_input(struct mbuf *m, int nxt)

{
#ifdef FAST_IPSEC
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secpolicy *sp;
	int s, error;
	/*
	 * enforce IPsec policy checking if we are seeing last header.
	 * note that we do not visit this with protocols with pcb layer
	 * code - like udp/tcp/raw ip.
	 */
	if ((inet6sw[ip6_protox[nxt]].pr_flags & PR_LASTHDR) != 0 &&
	    ipsec6_in_reject(m, NULL)) {

		/*
		 * Check if the packet has already had IPsec processing
		 * done.  If so, then just pass it along.  This tag gets
		 * set during AH, ESP, etc. input handling, before the
		 * packet is returned to the ip input queue for delivery.
		 */ 
		mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
		s = splnet();
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
		splx(s);
		if (error)
			return 1;
	}
#endif /* FAST_IPSEC */
	return 0;
}

/*
 * Called from ip6_output().
 * 1 = drop packet, 0 = continue processing packet,
 * -1 = packet was reinjected and stop processing packet (FAST_IPSEC only)
 */ 

int
ip6_ipsec_output(struct mbuf **m, struct inpcb *inp, int *flags, int *error,
		 struct ifnet **ifp, struct secpolicy **sp)
{
#ifdef FAST_IPSEC
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
	int s;
	if (sp == NULL)
		return 1;
	mtag = m_tag_find(*m, PACKET_TAG_IPSEC_PENDING_TDB, NULL);
	if (mtag != NULL) {
		tdbi = (struct tdb_ident *)(mtag + 1);
		*sp = ipsec_getpolicy(tdbi, IPSEC_DIR_OUTBOUND);
		if (*sp == NULL)
			*error = -EINVAL;	/* force silent drop */
		m_tag_delete(*m, mtag);
	} else {
		*sp = ipsec4_checkpolicy(*m, IPSEC_DIR_OUTBOUND, *flags,
					error, inp);
	}

	/*
	 * There are four return cases:
	 *    sp != NULL	 	    apply IPsec policy
	 *    sp == NULL, error == 0	    no IPsec handling needed
	 *    sp == NULL, error == -EINVAL  discard packet w/o error
	 *    sp == NULL, error != 0	    discard packet, report error
	 */
	if (*sp != NULL) {
		/* Loop detection, check if ipsec processing already done */
		KASSERT((*sp)->req != NULL, ("ip_output: no ipsec request"));
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
			if ((*sp)->req->sav == NULL)
				break;
			tdbi = (struct tdb_ident *)(mtag + 1);
			if (tdbi->spi == (*sp)->req->sav->spi &&
			    tdbi->proto == (*sp)->req->sav->sah->saidx.proto &&
			    bcmp(&tdbi->dst, &(*sp)->req->sav->sah->saidx.dst,
				 sizeof (union sockaddr_union)) == 0) {
				/*
				 * No IPsec processing is needed, free
				 * reference to SP.
				 *
				 * NB: null pointer to avoid free at
				 *     done: below.
				 */
				KEY_FREESP(sp), sp = NULL;
				splx(s);
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

		/*
		 * Preserve KAME behaviour: ENOENT can be returned
		 * when an SA acquire is in progress.  Don't propagate
		 * this to user-level; it confuses applications.
		 *
		 * XXX this will go away when the SADB is redone.
		 */
		if (*error == ENOENT)
			*error = 0;
		goto do_ipsec;
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
		if (*sp != NULL)
			KEY_FREESP(sp);
	return 0;
do_ipsec:
	return -1;
bad:
	if (sp != NULL)
		if (*sp != NULL)
			KEY_FREESP(sp);
	return 1;
#endif /* FAST_IPSEC */
	return 0;
}

/*
 * Compute the MTU for a forwarded packet that gets IPSEC encapsulated.
 * Called from ip_forward().
 * Returns MTU suggestion for ICMP needfrag reply.
 */
int
ip6_ipsec_mtu(struct mbuf *m)
{
	int mtu = 0;
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
#ifdef FAST_IPSEC
	sp = ipsec_getpolicybyaddr(m,
				   IPSEC_DIR_OUTBOUND,
				   IP_FORWARDING,
				   &ipsecerror);
#endif /* FAST_IPSEC */
	if (sp != NULL) {
		/* count IPsec header size */
		ipsechdr = ipsec4_hdrsiz(m,
					 IPSEC_DIR_OUTBOUND,
					 NULL);

		/*
		 * find the correct route for outer IPv4
		 * header, compute tunnel MTU.
		 */
		if (sp->req != NULL &&
		    sp->req->sav != NULL &&
		    sp->req->sav->sah != NULL) {
			ro = &sp->req->sav->sah->sa_route;
			if (ro->ro_rt && ro->ro_rt->rt_ifp) {
				mtu =
				    ro->ro_rt->rt_rmx.rmx_mtu ?
				    ro->ro_rt->rt_rmx.rmx_mtu :
				    ro->ro_rt->rt_ifp->if_mtu;
				mtu -= ipsechdr;
			}
		}
#ifdef FAST_IPSEC
		KEY_FREESP(&sp);
#endif /* FAST_IPSEC */
	}
	return mtu;
}

