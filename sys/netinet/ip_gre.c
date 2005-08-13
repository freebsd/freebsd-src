/*	$NetBSD: ip_gre.c,v 1.29 2003/09/05 23:02:43 itojun Exp $ */
/*	 $FreeBSD$ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
 *
 * IPv6-over-GRE contributed by Gert Doering <gert@greenie.muc.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * deencapsulate tunneled packets and send them on
 * output half is in net/if_gre.[ch]
 * This currently handles IPPROTO_GRE, IPPROTO_MOBILE
 */

#include "opt_inet.h"
#include "opt_atalk.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/raw_cb.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_gre.h>
#include <machine/in_cksum.h>
#else
#error ip_gre input without IP?
#endif

#ifdef NETATALK
#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/at_extern.h>
#endif

/* Needs IP headers. */
#include <net/if_gre.h>

#include <machine/stdarg.h>

#if 1
void gre_inet_ntoa(struct in_addr in);	/* XXX */
#endif

static struct gre_softc *gre_lookup(struct mbuf *, u_int8_t);

static int	gre_input2(struct mbuf *, int, u_char);

/*
 * De-encapsulate a packet and feed it back through ip input (this
 * routine is called whenever IP gets a packet with proto type
 * IPPROTO_GRE and a local destination address).
 * This really is simple
 */
void
#if __STDC__
gre_input(struct mbuf *m, ...)
#else
gre_input(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	int off, ret, proto;
	va_list ap;

	va_start(ap, m);
	off = va_arg(ap, int);
	va_end(ap);
	proto = (mtod(m, struct ip *))->ip_p;

	ret = gre_input2(m, off, proto);
	/*
	 * ret == 0 : packet not processed, meaning that
	 * no matching tunnel that is up is found.
	 * we inject it to raw ip socket to see if anyone picks it up.
	 */
	if (ret == 0)
		rip_input(m, off);
}

/*
 * decapsulate.
 * Does the real work and is called from gre_input() (above)
 * returns 0 if packet is not yet processed
 * and 1 if it needs no further processing
 * proto is the protocol number of the "calling" foo_input()
 * routine.
 */
static int
gre_input2(struct mbuf *m ,int hlen, u_char proto)
{
	struct greip *gip;
	int isr;
	struct gre_softc *sc;
	u_int16_t flags;
	u_int32_t af;

	if ((sc = gre_lookup(m, proto)) == NULL) {
		/* No matching tunnel or tunnel is down. */
		return (0);
	}

	if (m->m_len < sizeof(*gip)) {
		m = m_pullup(m, sizeof(*gip));
		if (m == NULL)
			return (ENOBUFS);
	}
	gip = mtod(m, struct greip *);

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	switch (proto) {
	case IPPROTO_GRE:
		hlen += sizeof(struct gre_h);

		/* process GRE flags as packet can be of variable len */
		flags = ntohs(gip->gi_flags);

		/* Checksum & Offset are present */
		if ((flags & GRE_CP) | (flags & GRE_RP))
			hlen += 4;
		/* We don't support routing fields (variable length) */
		if (flags & GRE_RP)
			return (0);
		if (flags & GRE_KP)
			hlen += 4;
		if (flags & GRE_SP)
			hlen += 4;

		switch (ntohs(gip->gi_ptype)) { /* ethertypes */
		case WCCP_PROTOCOL_TYPE:
			if (sc->wccp_ver == WCCP_V2)
				hlen += 4;
			/* FALLTHROUGH */
		case ETHERTYPE_IP:	/* shouldn't need a schednetisr(), */
			isr = NETISR_IP;/* as we are in ip_input */
			af = AF_INET;
			break;
#ifdef INET6
		case ETHERTYPE_IPV6:
			isr = NETISR_IPV6;
			af = AF_INET6;
			break;
#endif
#ifdef NETATALK
		case ETHERTYPE_ATALK:
			isr = NETISR_ATALK1;
			af = AF_APPLETALK;
			break;
#endif
		default:	   /* others not yet supported */
			return (0);
		}
		break;
	default:
		/* others not yet supported */
		return (0);
	}

	if (hlen > m->m_pkthdr.len) {
		m_freem(m);
		return (EINVAL);
	}
	/* Unlike NetBSD, in FreeBSD m_adj() adjusts m->m_pkthdr.len as well */
	m_adj(m, hlen);

	if (sc->sc_if.if_bpf) {
		bpf_mtap2(sc->sc_if.if_bpf, &af, sizeof(af), m);
	}

	m->m_pkthdr.rcvif = &sc->sc_if;

	netisr_dispatch(isr, m);

	return (1);	/* packet is done, no further processing needed */
}

/*
 * input routine for IPPRPOTO_MOBILE
 * This is a little bit diffrent from the other modes, as the
 * encapsulating header was not prepended, but instead inserted
 * between IP header and payload
 */

void
#if __STDC__
gre_mobile_input(struct mbuf *m, ...)
#else
gre_mobile_input(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	struct ip *ip;
	struct mobip_h *mip;
	struct gre_softc *sc;
	int hlen;
	va_list ap;
	int msiz;

	va_start(ap, m);
	hlen = va_arg(ap, int);
	va_end(ap);

	if ((sc = gre_lookup(m, IPPROTO_MOBILE)) == NULL) {
		/* No matching tunnel or tunnel is down. */
		m_freem(m);
		return;
	}

	if (m->m_len < sizeof(*mip)) {
		m = m_pullup(m, sizeof(*mip));
		if (m == NULL)
			return;
	}
	ip = mtod(m, struct ip *);
	mip = mtod(m, struct mobip_h *);

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	if (ntohs(mip->mh.proto) & MOB_H_SBIT) {
		msiz = MOB_H_SIZ_L;
		mip->mi.ip_src.s_addr = mip->mh.osrc;
	} else
		msiz = MOB_H_SIZ_S;

	if (m->m_len < (ip->ip_hl << 2) + msiz) {
		m = m_pullup(m, (ip->ip_hl << 2) + msiz);
		if (m == NULL)
			return;
		ip = mtod(m, struct ip *);
		mip = mtod(m, struct mobip_h *);
	}

	mip->mi.ip_dst.s_addr = mip->mh.odst;
	mip->mi.ip_p = (ntohs(mip->mh.proto) >> 8);

	if (gre_in_cksum((u_int16_t *)&mip->mh, msiz) != 0) {
		m_freem(m);
		return;
	}

	bcopy((caddr_t)(ip) + (ip->ip_hl << 2) + msiz, (caddr_t)(ip) +
	    (ip->ip_hl << 2), m->m_len - msiz - (ip->ip_hl << 2));
	m->m_len -= msiz;
	m->m_pkthdr.len -= msiz;

	/*
	 * On FreeBSD, rip_input() supplies us with ip->ip_len
	 * already converted into host byteorder and also decreases
	 * it by the lengh of IP header, however, ip_input() expects
	 * that this field is in the original format (network byteorder
	 * and full size of IP packet), so that adjust accordingly.
	 */
	ip->ip_len = htons(ip->ip_len + sizeof(struct ip) - msiz);

	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m, (ip->ip_hl << 2));

	if (sc->sc_if.if_bpf) {
		u_int32_t af = AF_INET;
		bpf_mtap2(sc->sc_if.if_bpf, &af, sizeof(af), m);
	}

	m->m_pkthdr.rcvif = &sc->sc_if;

	netisr_dispatch(NETISR_IP, m);
}

/*
 * Find the gre interface associated with our src/dst/proto set.
 *
 * XXXRW: Need some sort of drain/refcount mechanism so that the softc
 * reference remains valid after it's returned from gre_lookup().  Right
 * now, I'm thinking it should be reference-counted with a gre_dropref()
 * when the caller is done with the softc.  This is complicated by how
 * to handle destroying the gre softc; probably using a gre_drain() in
 * in_gre.c during destroy.
 */
static struct gre_softc *
gre_lookup(m, proto)
	struct mbuf *m;
	u_int8_t proto;
{
	struct ip *ip = mtod(m, struct ip *);
	struct gre_softc *sc;

	mtx_lock(&gre_mtx);
	for (sc = LIST_FIRST(&gre_softc_list); sc != NULL;
	     sc = LIST_NEXT(sc, sc_list)) {
		if ((sc->g_dst.s_addr == ip->ip_src.s_addr) &&
		    (sc->g_src.s_addr == ip->ip_dst.s_addr) &&
		    (sc->g_proto == proto) &&
		    ((sc->sc_if.if_flags & IFF_UP) != 0)) {
			mtx_unlock(&gre_mtx);
			return (sc);
		}
	}
	mtx_unlock(&gre_mtx);

	return (NULL);
}
