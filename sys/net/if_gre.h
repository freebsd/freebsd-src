/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * Copyright (c) 2014 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
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
 *
 * $NetBSD: if_gre.h,v 1.13 2003/11/10 08:51:52 wiz Exp $
 * $FreeBSD$
 */

#ifndef _NET_IF_GRE_H_
#define _NET_IF_GRE_H_

#ifdef _KERNEL
/* GRE header according to RFC 2784 and RFC 2890 */
struct grehdr {
	uint16_t	gre_flags;	/* GRE flags */
#define	GRE_FLAGS_CP	0x8000		/* checksum present */
#define	GRE_FLAGS_KP	0x2000		/* key present */
#define	GRE_FLAGS_SP	0x1000		/* sequence present */
#define	GRE_FLAGS_MASK	(GRE_FLAGS_CP|GRE_FLAGS_KP|GRE_FLAGS_SP)
	uint16_t	gre_proto;	/* protocol type */
	uint32_t	gre_opts[0];	/* optional fields */
} __packed;

#ifdef INET
struct greip {
	struct ip	gi_ip;
	struct grehdr	gi_gre;
} __packed;
#endif

#ifdef INET6
struct greip6 {
	struct ip6_hdr	gi6_ip6;
	struct grehdr	gi6_gre;
} __packed;
#endif

struct gre_softc {
	struct ifnet		*gre_ifp;
	LIST_ENTRY(gre_softc)	gre_list;
	struct rmlock		gre_lock;
	int			gre_family;	/* AF of delivery header */
	uint32_t		gre_iseq;
	uint32_t		gre_oseq;
	uint32_t		gre_key;
	uint32_t		gre_options;
	u_int			gre_fibnum;
	u_int			gre_hlen;	/* header size */
	union {
		void		*hdr;
#ifdef INET
		struct greip	*gihdr;
#endif
#ifdef INET6
		struct greip6	*gi6hdr;
#endif
	} gre_uhdr;
	const struct encaptab	*gre_ecookie;
};
#define	GRE2IFP(sc)		((sc)->gre_ifp)
#define	GRE_LOCK_INIT(sc)	rm_init(&(sc)->gre_lock, "gre softc")
#define	GRE_LOCK_DESTROY(sc)	rm_destroy(&(sc)->gre_lock)
#define	GRE_RLOCK_TRACKER	struct rm_priotracker gre_tracker
#define	GRE_RLOCK(sc)		rm_rlock(&(sc)->gre_lock, &gre_tracker)
#define	GRE_RUNLOCK(sc)		rm_runlock(&(sc)->gre_lock, &gre_tracker)
#define	GRE_RLOCK_ASSERT(sc)	rm_assert(&(sc)->gre_lock, RA_RLOCKED)
#define	GRE_WLOCK(sc)		rm_wlock(&(sc)->gre_lock)
#define	GRE_WUNLOCK(sc)		rm_wunlock(&(sc)->gre_lock)
#define	GRE_WLOCK_ASSERT(sc)	rm_assert(&(sc)->gre_lock, RA_WLOCKED)

#define	gre_hdr			gre_uhdr.hdr
#define	gre_gihdr		gre_uhdr.gihdr
#define	gre_gi6hdr		gre_uhdr.gi6hdr
#define	gre_oip			gre_gihdr->gi_ip
#define	gre_oip6		gre_gi6hdr->gi6_ip6

int	gre_input(struct mbuf **, int *, int);
#ifdef INET
int	in_gre_attach(struct gre_softc *);
int	in_gre_output(struct mbuf *, int, int);
#endif
#ifdef INET6
int	in6_gre_attach(struct gre_softc *);
int	in6_gre_output(struct mbuf *, int, int);
#endif
/*
 * CISCO uses special type for GRE tunnel created as part of WCCP
 * connection, while in fact those packets are just IPv4 encapsulated
 * into GRE.
 */
#define ETHERTYPE_WCCP		0x883E
#endif /* _KERNEL */

#define GRESADDRS	_IOW('i', 101, struct ifreq)
#define GRESADDRD	_IOW('i', 102, struct ifreq)
#define GREGADDRS	_IOWR('i', 103, struct ifreq)
#define GREGADDRD	_IOWR('i', 104, struct ifreq)
#define GRESPROTO	_IOW('i' , 105, struct ifreq)
#define GREGPROTO	_IOWR('i', 106, struct ifreq)

#define	GREGKEY		_IOWR('i', 107, struct ifreq)
#define	GRESKEY		_IOW('i', 108, struct ifreq)
#define	GREGOPTS	_IOWR('i', 109, struct ifreq)
#define	GRESOPTS	_IOW('i', 110, struct ifreq)

#define	GRE_ENABLE_CSUM		0x0001
#define	GRE_ENABLE_SEQ		0x0002
#define	GRE_OPTMASK		(GRE_ENABLE_CSUM|GRE_ENABLE_SEQ)

#endif /* _NET_IF_GRE_H_ */
