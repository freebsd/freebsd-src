/*	$FreeBSD$	*/
/*	$KAME: if_gif.h,v 1.17 2000/09/11 11:36:41 sumikawa Exp $	*/

/*-
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NET_IF_GIF_H_
#define _NET_IF_GIF_H_

#ifdef _KERNEL
#include "opt_inet.h"
#include "opt_inet6.h"

#include <netinet/in.h>

struct ip;
struct ip6_hdr;
struct encaptab;

extern	void (*ng_gif_input_p)(struct ifnet *ifp, struct mbuf **mp,
		int af);
extern	void (*ng_gif_input_orphan_p)(struct ifnet *ifp, struct mbuf *m,
		int af);
extern	int  (*ng_gif_output_p)(struct ifnet *ifp, struct mbuf **mp);
extern	void (*ng_gif_attach_p)(struct ifnet *ifp);
extern	void (*ng_gif_detach_p)(struct ifnet *ifp);

struct gif_softc {
	struct ifnet		*gif_ifp;
	struct rmlock		gif_lock;
	const struct encaptab	*gif_ecookie;
	int			gif_family;
	int			gif_flags;
	u_int			gif_fibnum;
	u_int			gif_options;
	void			*gif_netgraph;	/* netgraph node info */
	union {
		void		*hdr;
		struct ip	*iphdr;
#ifdef INET6
		struct ip6_hdr	*ip6hdr;
#endif
	} gif_uhdr;
	LIST_ENTRY(gif_softc)	gif_list; /* all gif's are linked */
};
#define	GIF2IFP(sc)	((sc)->gif_ifp)
#define	GIF_LOCK_INIT(sc)	rm_init(&(sc)->gif_lock, "gif softc")
#define	GIF_LOCK_DESTROY(sc)	rm_destroy(&(sc)->gif_lock)
#define	GIF_RLOCK_TRACKER	struct rm_priotracker gif_tracker
#define	GIF_RLOCK(sc)		rm_rlock(&(sc)->gif_lock, &gif_tracker)
#define	GIF_RUNLOCK(sc)		rm_runlock(&(sc)->gif_lock, &gif_tracker)
#define	GIF_RLOCK_ASSERT(sc)	rm_assert(&(sc)->gif_lock, RA_RLOCKED)
#define	GIF_WLOCK(sc)		rm_wlock(&(sc)->gif_lock)
#define	GIF_WUNLOCK(sc)		rm_wunlock(&(sc)->gif_lock)
#define	GIF_WLOCK_ASSERT(sc)	rm_assert(&(sc)->gif_lock, RA_WLOCKED)

#define	gif_iphdr	gif_uhdr.iphdr
#define	gif_hdr		gif_uhdr.hdr
#ifdef INET6
#define	gif_ip6hdr	gif_uhdr.ip6hdr
#endif

#define GIF_MTU		(1280)	/* Default MTU */
#define	GIF_MTU_MIN	(1280)	/* Minimum MTU */
#define	GIF_MTU_MAX	(8192)	/* Maximum MTU */

struct etherip_header {
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int	eip_resvl:4,	/* reserved */
		eip_ver:4;	/* version */
#endif
#if BYTE_ORDER == BIG_ENDIAN
	u_int	eip_ver:4,	/* version */
		eip_resvl:4;	/* reserved */
#endif
	u_int8_t eip_resvh;	/* reserved */
} __packed;

#define ETHERIP_VERSION			0x3
/* mbuf adjust factor to force 32-bit alignment of IP header */
#define	ETHERIP_ALIGN		2

/* Prototypes */
void gif_input(struct mbuf *, struct ifnet *, int, uint8_t);
int gif_output(struct ifnet *, struct mbuf *, const struct sockaddr *,
	       struct route *);
int gif_encapcheck(const struct mbuf *, int, int, void *);
#ifdef INET
int in_gif_output(struct ifnet *, struct mbuf *, int, uint8_t);
int in_gif_encapcheck(const struct mbuf *, int, int, void *);
int in_gif_attach(struct gif_softc *);
#endif
#ifdef INET6
int in6_gif_output(struct ifnet *, struct mbuf *, int, uint8_t);
int in6_gif_encapcheck(const struct mbuf *, int, int, void *);
int in6_gif_attach(struct gif_softc *);
#endif
#endif /* _KERNEL */

#define GIFGOPTS	_IOWR('i', 150, struct ifreq)
#define GIFSOPTS	_IOW('i', 151, struct ifreq)

#define	GIF_IGNORE_SOURCE	0x0002
#define	GIF_OPTMASK		(GIF_IGNORE_SOURCE)

#endif /* _NET_IF_GIF_H_ */
