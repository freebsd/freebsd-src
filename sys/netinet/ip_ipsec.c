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
#include <netinet/ip_ipsec.h>

#include <machine/in_cksum.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#ifdef IPSEC_DEBUG
#include <netkey/key_debug.h>
#else
#define	KEYDEBUG(lev,arg)
#endif
#endif /*IPSEC*/

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#include <netipsec/key.h>
#endif /*FAST_IPSEC*/

extern	struct protosw inetsw[];

/*
 * Check if we have to jump over firewall processing for this packet.
 * Called from ip_input().
 * 1 = jump over firewall, 0 = packet goes through firewall.
 */
int
ip_ipsec_filtergif(struct mbuf *m)
{
#if defined(IPSEC) && !defined(IPSEC_FILTERGIF)
	/*
	 * Bypass packet filtering for packets from a tunnel (gif).
	 */
	if (ipsec_getnhist(m))
		return 1;
#endif
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
ip_ipsec_fwd(struct mbuf *m)
{
#ifdef FAST_IPSEC
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secpolicy *sp;
	int s, error;
#endif /* FAST_IPSEC */
#ifdef IPSEC
	/*
	 * Enforce inbound IPsec SPD.
	 */
	if (ipsec4_in_reject(m, NULL)) {
		ipsecstat.in_polvio++;
		return 1;
	}
#endif /* IPSEC */
#ifdef FAST_IPSEC
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
ip_ipsec_input(struct mbuf *m)
{
	struct ip *ip = mtod(m, struct ip *);
#ifdef FAST_IPSEC
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct secpolicy *sp;
	int s, error;
#endif /* FAST_IPSEC */
#ifdef IPSEC
	/*
	 * enforce IPsec policy checking if we are seeing last header.
	 * note that we do not visit this with protocols with pcb layer
	 * code - like udp/tcp/raw ip.
	 */
	if ((inetsw[ip_protox[ip->ip_p]].pr_flags & PR_LASTHDR) != 0 &&
	    ipsec4_in_reject(m, NULL)) {
		ipsecstat.in_polvio++;
		return 1;
	}
#endif
#ifdef FAST_IPSEC
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
 * Compute the MTU for a forwarded packet that gets IPSEC encapsulated.
 * Called from ip_forward().
 * Returns MTU suggestion for ICMP needfrag reply.
 */
int
ip_ipsec_mtu(struct mbuf *m)
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
#ifdef IPSEC
	sp = ipsec4_getpolicybyaddr(m,
				    IPSEC_DIR_OUTBOUND,
				    IP_FORWARDING,
				    &ipsecerror);
#else /* FAST_IPSEC */
	sp = ipsec_getpolicybyaddr(m,
				   IPSEC_DIR_OUTBOUND,
				   IP_FORWARDING,
				   &ipsecerror);
#endif
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
#ifdef IPSEC
		key_freesp(sp);
#else /* FAST_IPSEC */
		KEY_FREESP(&sp);
#endif
	}
	return mtu;
}

/*
 * 
 * Called from ip_output().
 * 1 = drop packet, 0 = continue processing packet,
 * -1 = packet was reinjected and stop processing packet (FAST_IPSEC only)
 */
int
ip_ipsec_output(struct mbuf **m, struct inpcb *inp, int *flags, int *error,
    struct route **ro, struct route *iproute, struct sockaddr_in **dst,
    struct in_ifaddr **ia, struct ifnet **ifp)
{
	struct secpolicy *sp = NULL;
	struct ip *ip = mtod(*m, struct ip *);
#ifdef IPSEC
	struct ipsec_output_state state;
#endif
#ifdef FAST_IPSEC
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
	int s;
#endif /* FAST_IPSEC */
#ifdef IPSEC
	/* get SP for this packet */
	if (inp == NULL)
		sp = ipsec4_getpolicybyaddr(*m, IPSEC_DIR_OUTBOUND,
		    *flags, error);
	else
		sp = ipsec4_getpolicybypcb(*m, IPSEC_DIR_OUTBOUND, inp, error);

	if (sp == NULL) {
		ipsecstat.out_inval++;
		goto bad;
	}

	/* check policy */
	switch (sp->policy) {
	case IPSEC_POLICY_DISCARD:
		/*
		 * This packet is just discarded.
		 */
		ipsecstat.out_polvio++;
		goto bad;

	case IPSEC_POLICY_BYPASS:
	case IPSEC_POLICY_NONE:
	case IPSEC_POLICY_TCP:
		/* no need to do IPsec. */
		goto done;
	
	case IPSEC_POLICY_IPSEC:
		if (sp->req == NULL) {
			/* acquire a policy */
			*error = key_spdacquire(sp);
			goto bad;
		}
		break;

	case IPSEC_POLICY_ENTRUST:
	default:
		printf("%s: Invalid policy found. %d\n", __func__, sp->policy);
	}

	bzero(&state, sizeof(state));
	state.m = *m;
	if (*flags & IP_ROUTETOIF) {
		state.ro = iproute;
		bzero(iproute, sizeof(iproute));
	} else
		state.ro = *ro;
	state.dst = (struct sockaddr *)(*dst);

	ip->ip_sum = 0;

	/*
	 * XXX
	 * delayed checksums are not currently compatible with IPsec
	 */
	if ((*m)->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
		in_delayed_cksum(*m);
		(*m)->m_pkthdr.csum_flags &= ~CSUM_DELAY_DATA;
	}

	ip->ip_len = htons(ip->ip_len);
	ip->ip_off = htons(ip->ip_off);

	*error = ipsec4_output(&state, sp, *flags);

	*m = state.m;
	if (*flags & IP_ROUTETOIF) {
		/*
		 * if we have tunnel mode SA, we may need to ignore
		 * IP_ROUTETOIF.
		 */
		if (state.ro != iproute || state.ro->ro_rt != NULL) {
			*flags &= ~IP_ROUTETOIF;
			*ro = state.ro;
		}
	} else
		*ro = state.ro;
	*dst = (struct sockaddr_in *)state.dst;
	if (*error != 0) {
		/* mbuf is already reclaimed in ipsec4_output. */
		*m = NULL;
		switch (*error) {
		case EHOSTUNREACH:
		case ENETUNREACH:
		case EMSGSIZE:
		case ENOBUFS:
		case ENOMEM:
			break;
		default:
			printf("ip4_output (ipsec): error code %d\n", *error);
			/*fall through*/
		case ENOENT:
			/* don't show these error codes to the user */
			*error = 0;
			break;
		}
		goto bad;
	}

	/* be sure to update variables that are affected by ipsec4_output() */
	if ((*ro)->ro_rt == NULL) {
		if ((*flags & IP_ROUTETOIF) == 0) {
			printf("ip_output: "
				"can't update route after IPsec processing\n");
			*error = EHOSTUNREACH;	/*XXX*/
			goto bad;
		}
	} else {
		if (state.encap) {
			*ia = ifatoia((*ro)->ro_rt->rt_ifa);
			*ifp = (*ro)->ro_rt->rt_ifp;
		}
	}
	ip = mtod(*m, struct ip *);

	/* make it flipped, again. */
	ip->ip_len = ntohs(ip->ip_len);
	ip->ip_off = ntohs(ip->ip_off);

done:
	if (sp != NULL) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP ip_output call free SP:%p\n", sp));
		key_freesp(sp);
	}
	return 0;
bad:
	if (sp != NULL) {
		KEYDEBUG(KEYDEBUG_IPSEC_STAMP,
			printf("DP ip_output call free SP:%p\n", sp));
		key_freesp(sp);
	}
	return 1;
#endif /*IPSEC*/
#ifdef FAST_IPSEC
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
	s = splnet();
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

		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);

		/* NB: callee frees mbuf */
		*error = ipsec4_process_packet(*m, sp->req, *flags, 0);
		/*
		 * Preserve KAME behaviour: ENOENT can be returned
		 * when an SA acquire is in progress.  Don't propagate
		 * this to user-level; it confuses applications.
		 *
		 * XXX this will go away when the SADB is redone.
		 */
		if (*error == ENOENT)
			*error = 0;
		splx(s);
		goto reinjected;
	} else {	/* sp == NULL */
		splx(s);

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
#ifdef notyet
		/*
		 * If deferred crypto processing is needed, check that
		 * the interface supports it.
		 */ 
		mtag = m_tag_find(*m, PACKET_TAG_IPSEC_OUT_CRYPTO_NEEDED, NULL);
		if (mtag != NULL && ((*ifp)->if_capenable & IFCAP_IPSEC) == 0) {
			/* notify IPsec to do its own crypto */
			ipsp_skipcrypto_unmark((struct tdb_ident *)(mtag + 1));
			*error = EHOSTUNREACH;
			goto bad;
		}
#endif
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
#endif /* FAST_IPSEC */
	return 0;
}
