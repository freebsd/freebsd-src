/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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

/*
 * IPsec output processing.
 */
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_var.h>
#include <netinet/ip_ecn.h>
#ifdef INET6
#include <netinet6/ip6_ecn.h>
#endif

#include <netinet/ip6.h>
#ifdef INET6
#include <netinet6/ip6_var.h>
#endif
#include <netinet/in_pcb.h>
#ifdef INET6
#include <netinet/icmp6.h>
#endif

#include <netipsec/ipsec.h>
#ifdef INET6
#include <netipsec/ipsec6.h>
#endif
#include <netipsec/ah_var.h>
#include <netipsec/esp_var.h>
#include <netipsec/ipcomp_var.h>

#include <netipsec/xform.h>

#include <netipsec/key.h>
#include <netipsec/keydb.h>
#include <netipsec/key_debug.h>

#include <machine/in_cksum.h>

int
ipsec_process_done(struct mbuf *m, struct ipsecrequest *isr)
{
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
	struct secasvar *sav;
	struct secasindex *saidx;
	int error;

#if 0
	SPLASSERT(net, "ipsec_process_done");
#endif

	KASSERT(m != NULL, ("ipsec_process_done: null mbuf"));
	KASSERT(isr != NULL, ("ipsec_process_done: null ISR"));
	sav = isr->sav;
	KASSERT(sav != NULL, ("ipsec_process_done: null SA"));
	KASSERT(sav->sah != NULL, ("ipsec_process_done: null SAH"));

	saidx = &sav->sah->saidx;
	switch (saidx->dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		/* Fix the header length, for AH processing. */
		mtod(m, struct ip *)->ip_len = htons(m->m_pkthdr.len);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		/* Fix the header length, for AH processing. */
		if (m->m_pkthdr.len < sizeof (struct ip6_hdr)) {
			error = ENXIO;
			goto bad;
		}
		if (m->m_pkthdr.len - sizeof (struct ip6_hdr) > IPV6_MAXPACKET) {
			/* No jumbogram support. */
			error = ENXIO;	/*?*/
			goto bad;
		}
		mtod(m, struct ip6_hdr *)->ip6_plen =
			htons(m->m_pkthdr.len - sizeof(struct ip6_hdr));
		break;
#endif /* INET6 */
	default:
		DPRINTF(("ipsec_process_done: unknown protocol family %u\n",
		    saidx->dst.sa.sa_family));
		error = ENXIO;
		goto bad;
	}

	/*
	 * Add a record of what we've done or what needs to be done to the
	 * packet.
	 */
	mtag = m_tag_get(PACKET_TAG_IPSEC_OUT_DONE,
			sizeof(struct tdb_ident), M_NOWAIT);
	if (mtag == NULL) {
		DPRINTF(("ipsec_process_done: could not get packet tag\n"));
		error = ENOMEM;
		goto bad;
	}

	tdbi = (struct tdb_ident *)(mtag + 1);
	tdbi->dst = saidx->dst;
	tdbi->proto = saidx->proto;
	tdbi->spi = sav->spi;
	m_tag_prepend(m, mtag);

	/*
	 * If there's another (bundled) SA to apply, do so.
	 * Note that this puts a burden on the kernel stack size.
	 * If this is a problem we'll need to introduce a queue
	 * to set the packet on so we can unwind the stack before
	 * doing further processing.
	 */
	if (isr->next) {
		newipsecstat.ips_out_bundlesa++;
		return ipsec4_process_packet(m, isr->next, 0, 0);
	}
	key_sa_recordxfer(sav, m);		/* record data transfer */

	/*
	 * We're done with IPsec processing, transmit the packet using the
	 * appropriate network protocol (IP or IPv6). SPD lookup will be
	 * performed again there.
	 */
	switch (saidx->dst.sa.sa_family) {
#ifdef INET
	struct ip *ip;
	case AF_INET:
		ip = mtod(m, struct ip *);
		ip->ip_len = ntohs(ip->ip_len);
		ip->ip_off = ntohs(ip->ip_off);

		return ip_output(m, NULL, NULL, IP_RAWOUTPUT, NULL, NULL);
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		/*
		 * We don't need massage, IPv6 header fields are always in
		 * net endian.
		 */
		return ip6_output(m, NULL, NULL, 0, NULL, NULL, NULL);
#endif /* INET6 */
	}
	panic("ipsec_process_done");
bad:
	m_freem(m);
	KEY_FREESAV(&sav);
	return (error);
}

static struct ipsecrequest *
ipsec_nextisr(
	struct mbuf *m,
	struct ipsecrequest *isr,
	int af,
	struct secasindex *saidx,
	int *error
)
{
#define IPSEC_OSTAT(x,y,z) (isr->saidx.proto == IPPROTO_ESP ? (x)++ : \
			    isr->saidx.proto == IPPROTO_AH ? (y)++ : (z)++)
	struct secasvar *sav;

#if 0
	SPLASSERT(net, "ipsec_nextisr");
#endif
	KASSERT(af == AF_INET || af == AF_INET6,
		("ipsec_nextisr: invalid address family %u", af));
again:
	/*
	 * Craft SA index to search for proper SA.  Note that
	 * we only fillin unspecified SA peers for transport
	 * mode; for tunnel mode they must already be filled in.
	 */
	*saidx = isr->saidx;
	if (isr->saidx.mode == IPSEC_MODE_TRANSPORT) {
		/* Fillin unspecified SA peers only for transport mode */
		if (af == AF_INET) {
			struct sockaddr_in *sin;
			struct ip *ip = mtod(m, struct ip *);

			if (saidx->src.sa.sa_len == 0) {
				sin = &saidx->src.sin;
				sin->sin_len = sizeof(*sin);
				sin->sin_family = AF_INET;
				sin->sin_port = IPSEC_PORT_ANY;
				sin->sin_addr = ip->ip_src;
			}
			if (saidx->dst.sa.sa_len == 0) {
				sin = &saidx->dst.sin;
				sin->sin_len = sizeof(*sin);
				sin->sin_family = AF_INET;
				sin->sin_port = IPSEC_PORT_ANY;
				sin->sin_addr = ip->ip_dst;
			}
		} else {
			struct sockaddr_in6 *sin6;
			struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);

			if (saidx->src.sin6.sin6_len == 0) {
				sin6 = (struct sockaddr_in6 *)&saidx->src;
				sin6->sin6_len = sizeof(*sin6);
				sin6->sin6_family = AF_INET6;
				sin6->sin6_port = IPSEC_PORT_ANY;
				sin6->sin6_addr = ip6->ip6_src;
				if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src)) {
					/* fix scope id for comparing SPD */
					sin6->sin6_addr.s6_addr16[1] = 0;
					sin6->sin6_scope_id =
					    ntohs(ip6->ip6_src.s6_addr16[1]);
				}
			}
			if (saidx->dst.sin6.sin6_len == 0) {
				sin6 = (struct sockaddr_in6 *)&saidx->dst;
				sin6->sin6_len = sizeof(*sin6);
				sin6->sin6_family = AF_INET6;
				sin6->sin6_port = IPSEC_PORT_ANY;
				sin6->sin6_addr = ip6->ip6_dst;
				if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst)) {
					/* fix scope id for comparing SPD */
					sin6->sin6_addr.s6_addr16[1] = 0;
					sin6->sin6_scope_id =
					    ntohs(ip6->ip6_dst.s6_addr16[1]);
				}
			}
		}
	}

	/*
	 * Lookup SA and validate it.
	 */
	*error = key_checkrequest(isr, saidx);
	if (*error != 0) {
		/*
		 * IPsec processing is required, but no SA found.
		 * I assume that key_acquire() had been called
		 * to get/establish the SA. Here I discard
		 * this packet because it is responsibility for
		 * upper layer to retransmit the packet.
		 */
		newipsecstat.ips_out_nosa++;
		goto bad;
	}
	sav = isr->sav;
	if (sav == NULL) {		/* XXX valid return */
		KASSERT(ipsec_get_reqlevel(isr) == IPSEC_LEVEL_USE,
			("ipsec_nextisr: no SA found, but required; level %u",
			ipsec_get_reqlevel(isr)));
		isr = isr->next;
		if (isr == NULL) {
			/*XXXstatistic??*/
			*error = EINVAL;		/*XXX*/
			return isr;
		}
		goto again;
	}

	/*
	 * Check system global policy controls.
	 */
	if ((isr->saidx.proto == IPPROTO_ESP && !esp_enable) ||
	    (isr->saidx.proto == IPPROTO_AH && !ah_enable) ||
	    (isr->saidx.proto == IPPROTO_IPCOMP && !ipcomp_enable)) {
		DPRINTF(("ipsec_nextisr: IPsec outbound packet dropped due"
			" to policy (check your sysctls)\n"));
		IPSEC_OSTAT(espstat.esps_pdrops, ahstat.ahs_pdrops,
		    ipcompstat.ipcomps_pdrops);
		*error = EHOSTUNREACH;
		goto bad;
	}

	/*
	 * Sanity check the SA contents for the caller
	 * before they invoke the xform output method.
	 */
	if (sav->tdb_xform == NULL) {
		DPRINTF(("ipsec_nextisr: no transform for SA\n"));
		IPSEC_OSTAT(espstat.esps_noxform, ahstat.ahs_noxform,
		    ipcompstat.ipcomps_noxform);
		*error = EHOSTUNREACH;
		goto bad;
	}
	return isr;
bad:
	KASSERT(*error != 0, ("ipsec_nextisr: error return w/ no error code"));
	return NULL;
#undef IPSEC_OSTAT
}

#ifdef INET
/*
 * IPsec output logic for IPv4.
 */
int
ipsec4_process_packet(
	struct mbuf *m,
	struct ipsecrequest *isr,
	int flags,
	int tunalready)
{
	struct secasindex saidx;
	struct secasvar *sav;
	struct ip *ip;
	int error, i, off;

	KASSERT(m != NULL, ("ipsec4_process_packet: null mbuf"));
	KASSERT(isr != NULL, ("ipsec4_process_packet: null isr"));

	mtx_lock(&isr->lock);		/* insure SA contents don't change */

	isr = ipsec_nextisr(m, isr, AF_INET, &saidx, &error);
	if (isr == NULL)
		goto bad;

	sav = isr->sav;
	if (!tunalready) {
		union sockaddr_union *dst = &sav->sah->saidx.dst;
		int setdf;

		/*
		 * Collect IP_DF state from the outer header.
		 */
		if (dst->sa.sa_family == AF_INET) {
			if (m->m_len < sizeof (struct ip) &&
			    (m = m_pullup(m, sizeof (struct ip))) == NULL) {
				error = ENOBUFS;
				goto bad;
			}
			ip = mtod(m, struct ip *);
			/* Honor system-wide control of how to handle IP_DF */
			switch (ip4_ipsec_dfbit) {
			case 0:			/* clear in outer header */
			case 1:			/* set in outer header */
				setdf = ip4_ipsec_dfbit;
				break;
			default:		/* propagate to outer header */
				setdf = ntohs(ip->ip_off & IP_DF);
				break;
			}
		} else {
			ip = NULL;		/* keep compiler happy */
			setdf = 0;
		}
		/* Do the appropriate encapsulation, if necessary */
		if (isr->saidx.mode == IPSEC_MODE_TUNNEL || /* Tunnel requ'd */
		    dst->sa.sa_family != AF_INET ||	    /* PF mismatch */
#if 0
		    (sav->flags & SADB_X_SAFLAGS_TUNNEL) || /* Tunnel requ'd */
		    sav->tdb_xform->xf_type == XF_IP4 ||    /* ditto */
#endif
		    (dst->sa.sa_family == AF_INET &&	    /* Proxy */
		     dst->sin.sin_addr.s_addr != INADDR_ANY &&
		     dst->sin.sin_addr.s_addr != ip->ip_dst.s_addr)) {
			struct mbuf *mp;

			/* Fix IPv4 header checksum and length */
			if (m->m_len < sizeof (struct ip) &&
			    (m = m_pullup(m, sizeof (struct ip))) == NULL) {
				error = ENOBUFS;
				goto bad;
			}
			ip = mtod(m, struct ip *);
			ip->ip_len = htons(m->m_pkthdr.len);
			ip->ip_sum = 0;
#ifdef _IP_VHL
			if (ip->ip_vhl == IP_VHL_BORING)
				ip->ip_sum = in_cksum_hdr(ip);
			else
				ip->ip_sum = in_cksum(m,
					_IP_VHL_HL(ip->ip_vhl) << 2);
#else
			ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
#endif

			/* Encapsulate the packet */
			error = ipip_output(m, isr, &mp, 0, 0);
			if (mp == NULL && !error) {
				/* Should never happen. */
				DPRINTF(("ipsec4_process_packet: ipip_output "
					"returns no mbuf and no error!"));
				error = EFAULT;
			}
			if (error) {
				if (mp)
					m_freem(mp);
				goto bad;
			}
			m = mp, mp = NULL;
			/*
			 * ipip_output clears IP_DF in the new header.  If
			 * we need to propagate IP_DF from the outer header,
			 * then we have to do it here.
			 *
			 * XXX shouldn't assume what ipip_output does.
			 */
			if (dst->sa.sa_family == AF_INET && setdf) {
				if (m->m_len < sizeof (struct ip) &&
				    (m = m_pullup(m, sizeof (struct ip))) == NULL) {
					error = ENOBUFS;
					goto bad;
				}
				ip = mtod(m, struct ip *);
				ip->ip_off = ntohs(ip->ip_off);
				ip->ip_off |= IP_DF;
				ip->ip_off = htons(ip->ip_off);
			}
		}
	}

	/*
	 * Dispatch to the appropriate IPsec transform logic.  The
	 * packet will be returned for transmission after crypto
	 * processing, etc. are completed.  For encapsulation we
	 * bypass this call because of the explicit call done above
	 * (necessary to deal with IP_DF handling for IPv4).
	 *
	 * NB: m & sav are ``passed to caller'' who's reponsible for
	 *     for reclaiming their resources.
	 */
	if (sav->tdb_xform->xf_type != XF_IP4) {
		ip = mtod(m, struct ip *);
		i = ip->ip_hl << 2;
		off = offsetof(struct ip, ip_p);
		error = (*sav->tdb_xform->xf_output)(m, isr, NULL, i, off);
	} else {
		error = ipsec_process_done(m, isr);
	}
	mtx_unlock(&isr->lock);
	return error;
bad:
	mtx_unlock(&isr->lock);
	if (m)
		m_freem(m);
	return error;
}
#endif

#ifdef INET6
/*
 * Chop IP6 header from the payload.
 */
static struct mbuf *
ipsec6_splithdr(struct mbuf *m)
{
	struct mbuf *mh;
	struct ip6_hdr *ip6;
	int hlen;

	KASSERT(m->m_len >= sizeof (struct ip6_hdr),
		("ipsec6_splithdr: first mbuf too short, len %u", m->m_len));
	ip6 = mtod(m, struct ip6_hdr *);
	hlen = sizeof(struct ip6_hdr);
	if (m->m_len > hlen) {
		MGETHDR(mh, M_DONTWAIT, MT_HEADER);
		if (!mh) {
			m_freem(m);
			return NULL;
		}
		M_MOVE_PKTHDR(mh, m);
		MH_ALIGN(mh, hlen);
		m->m_len -= hlen;
		m->m_data += hlen;
		mh->m_next = m;
		m = mh;
		m->m_len = hlen;
		bcopy((caddr_t)ip6, mtod(m, caddr_t), hlen);
	} else if (m->m_len < hlen) {
		m = m_pullup(m, hlen);
		if (!m)
			return NULL;
	}
	return m;
}

/*
 * IPsec output logic for IPv6, transport mode.
 */
int
ipsec6_output_trans(
	struct ipsec_output_state *state,
	u_char *nexthdrp,
	struct mbuf *mprev,
	struct secpolicy *sp,
	int flags,
	int *tun)
{
	struct ipsecrequest *isr;
	struct secasindex saidx;
	int error = 0;
	struct mbuf *m;

	KASSERT(state != NULL, ("ipsec6_output: null state"));
	KASSERT(state->m != NULL, ("ipsec6_output: null m"));
	KASSERT(nexthdrp != NULL, ("ipsec6_output: null nexthdrp"));
	KASSERT(mprev != NULL, ("ipsec6_output: null mprev"));
	KASSERT(sp != NULL, ("ipsec6_output: null sp"));
	KASSERT(tun != NULL, ("ipsec6_output: null tun"));

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec6_output_trans: applyed SP\n");
		kdebug_secpolicy(sp));

	isr = sp->req;
	if (isr->saidx.mode == IPSEC_MODE_TUNNEL) {
		/* the rest will be handled by ipsec6_output_tunnel() */
		*tun = 1;		/* need tunnel-mode processing */
		return 0;
	}

	*tun = 0;
	m = state->m;

	isr = ipsec_nextisr(m, isr, AF_INET6, &saidx, &error);
	if (isr == NULL) {
#ifdef notdef
		/* XXX should notification be done for all errors ? */
		/*
		 * Notify the fact that the packet is discarded
		 * to ourselves. I believe this is better than
		 * just silently discarding. (jinmei@kame.net)
		 * XXX: should we restrict the error to TCP packets?
		 * XXX: should we directly notify sockets via
		 *      pfctlinputs?
		 */
		icmp6_error(m, ICMP6_DST_UNREACH,
			    ICMP6_DST_UNREACH_ADMIN, 0);
		m = NULL;	/* NB: icmp6_error frees mbuf */
#endif
		goto bad;
	}

	return (*isr->sav->tdb_xform->xf_output)(m, isr, NULL,
		sizeof (struct ip6_hdr),
		offsetof(struct ip6_hdr, ip6_nxt));
bad:
	if (m)
		m_freem(m);
	state->m = NULL;
	return error;
}

static int
ipsec6_encapsulate(struct mbuf *m, struct secasvar *sav)
{
	struct ip6_hdr *oip6;
	struct ip6_hdr *ip6;
	size_t plen;

	/* can't tunnel between different AFs */
	if (sav->sah->saidx.src.sa.sa_family != AF_INET6 ||
	    sav->sah->saidx.dst.sa.sa_family != AF_INET6) {
		m_freem(m);
		return EINVAL;
	}
	KASSERT(m->m_len != sizeof (struct ip6_hdr),
		("ipsec6_encapsulate: mbuf wrong size; len %u", m->m_len));


	/*
	 * grow the mbuf to accomodate the new IPv6 header.
	 */
	plen = m->m_pkthdr.len;
	if (M_LEADINGSPACE(m->m_next) < sizeof(struct ip6_hdr)) {
		struct mbuf *n;
		MGET(n, M_DONTWAIT, MT_DATA);
		if (!n) {
			m_freem(m);
			return ENOBUFS;
		}
		n->m_len = sizeof(struct ip6_hdr);
		n->m_next = m->m_next;
		m->m_next = n;
		m->m_pkthdr.len += sizeof(struct ip6_hdr);
		oip6 = mtod(n, struct ip6_hdr *);
	} else {
		m->m_next->m_len += sizeof(struct ip6_hdr);
		m->m_next->m_data -= sizeof(struct ip6_hdr);
		m->m_pkthdr.len += sizeof(struct ip6_hdr);
		oip6 = mtod(m->m_next, struct ip6_hdr *);
	}
	ip6 = mtod(m, struct ip6_hdr *);
	bcopy((caddr_t)ip6, (caddr_t)oip6, sizeof(struct ip6_hdr));

	/* Fake link-local scope-class addresses */
	if (IN6_IS_SCOPE_LINKLOCAL(&oip6->ip6_src))
		oip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_SCOPE_LINKLOCAL(&oip6->ip6_dst))
		oip6->ip6_dst.s6_addr16[1] = 0;

	/* construct new IPv6 header. see RFC 2401 5.1.2.2 */
	/* ECN consideration. */
	ip6_ecn_ingress(ip6_ipsec_ecn, &ip6->ip6_flow, &oip6->ip6_flow);
	if (plen < IPV6_MAXPACKET - sizeof(struct ip6_hdr))
		ip6->ip6_plen = htons(plen);
	else {
		/* ip6->ip6_plen will be updated in ip6_output() */
	}
	ip6->ip6_nxt = IPPROTO_IPV6;
	sav->sah->saidx.src.sin6.sin6_addr = ip6->ip6_src;
	sav->sah->saidx.dst.sin6.sin6_addr = ip6->ip6_dst;
	ip6->ip6_hlim = IPV6_DEFHLIM;

	/* XXX Should ip6_src be updated later ? */

	return 0;
}

/*
 * IPsec output logic for IPv6, tunnel mode.
 */
int
ipsec6_output_tunnel(struct ipsec_output_state *state, struct secpolicy *sp, int flags)
{
	struct ip6_hdr *ip6;
	struct ipsecrequest *isr;
	struct secasindex saidx;
	int error;
	struct sockaddr_in6* dst6;
	struct mbuf *m;

	KASSERT(state != NULL, ("ipsec6_output: null state"));
	KASSERT(state->m != NULL, ("ipsec6_output: null m"));
	KASSERT(sp != NULL, ("ipsec6_output: null sp"));

	KEYDEBUG(KEYDEBUG_IPSEC_DATA,
		printf("ipsec6_output_tunnel: applyed SP\n");
		kdebug_secpolicy(sp));

	m = state->m;
	/*
	 * transport mode ipsec (before the 1st tunnel mode) is already
	 * processed by ipsec6_output_trans().
	 */
	for (isr = sp->req; isr; isr = isr->next) {
		if (isr->saidx.mode == IPSEC_MODE_TUNNEL)
			break;
	}
	isr = ipsec_nextisr(m, isr, AF_INET6, &saidx, &error);
	if (isr == NULL)
		goto bad;

	/*
	 * There may be the case that SA status will be changed when
	 * we are refering to one. So calling splsoftnet().
	 */
	if (isr->saidx.mode == IPSEC_MODE_TUNNEL) {
		/*
		 * build IPsec tunnel.
		 */
		/* XXX should be processed with other familiy */
		if (isr->sav->sah->saidx.src.sa.sa_family != AF_INET6) {
			ipseclog((LOG_ERR, "ipsec6_output_tunnel: "
			    "family mismatched between inner and outer, spi=%u\n",
			    ntohl(isr->sav->spi)));
			newipsecstat.ips_out_inval++;
			error = EAFNOSUPPORT;
			goto bad;
		}

		m = ipsec6_splithdr(m);
		if (!m) {
			newipsecstat.ips_out_nomem++;
			error = ENOMEM;
			goto bad;
		}
		error = ipsec6_encapsulate(m, isr->sav);
		if (error) {
			m = NULL;
			goto bad;
		}
		ip6 = mtod(m, struct ip6_hdr *);

		state->ro = &isr->sav->sah->sa_route;
		state->dst = (struct sockaddr *)&state->ro->ro_dst;
		dst6 = (struct sockaddr_in6 *)state->dst;
		if (state->ro->ro_rt
		 && ((state->ro->ro_rt->rt_flags & RTF_UP) == 0
		  || !IN6_ARE_ADDR_EQUAL(&dst6->sin6_addr, &ip6->ip6_dst))) {
			RTFREE(state->ro->ro_rt);
			state->ro->ro_rt = NULL;
		}
		if (state->ro->ro_rt == 0) {
			bzero(dst6, sizeof(*dst6));
			dst6->sin6_family = AF_INET6;
			dst6->sin6_len = sizeof(*dst6);
			dst6->sin6_addr = ip6->ip6_dst;
			rtalloc(state->ro);
		}
		if (state->ro->ro_rt == 0) {
			ip6stat.ip6s_noroute++;
			newipsecstat.ips_out_noroute++;
			error = EHOSTUNREACH;
			goto bad;
		}

		/* adjust state->dst if tunnel endpoint is offlink */
		if (state->ro->ro_rt->rt_flags & RTF_GATEWAY) {
			state->dst = (struct sockaddr *)state->ro->ro_rt->rt_gateway;
			dst6 = (struct sockaddr_in6 *)state->dst;
		}
	}

	m = ipsec6_splithdr(m);
	if (!m) {
		newipsecstat.ips_out_nomem++;
		error = ENOMEM;
		goto bad;
	}
	ip6 = mtod(m, struct ip6_hdr *);
	return (*isr->sav->tdb_xform->xf_output)(m, isr, NULL,
		sizeof (struct ip6_hdr),
		offsetof(struct ip6_hdr, ip6_nxt));
bad:
	if (m)
		m_freem(m);
	state->m = NULL;
	return error;
}
#endif /*INET6*/
