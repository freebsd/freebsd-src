/*	$OpenBSD: if_pflog.c,v 1.22 2006/12/15 09:31:20 otto Exp $	*/
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and 
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece, 
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
 * Copyright (c) 2001, Angelos D. Keromytis, Niels Provos.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include "bpfilter.h"
#include "pflog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/bpf.h>

#ifdef	INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/nd6.h>
#endif /* INET6 */

#include <net/pfvar.h>
#include <net/if_pflog.h>

#define PFLOGMTU	(32768 + MHLEN + MLEN)

#ifdef PFLOGDEBUG
#define DPRINTF(x)    do { if (pflogdebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

void	pflogattach(int);
int	pflogoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	    	       struct rtentry *);
int	pflogioctl(struct ifnet *, u_long, caddr_t);
void	pflogstart(struct ifnet *);
int	pflog_clone_create(struct if_clone *, int);
int	pflog_clone_destroy(struct ifnet *);

LIST_HEAD(, pflog_softc)	pflogif_list;
struct if_clone	pflog_cloner =
    IF_CLONE_INITIALIZER("pflog", pflog_clone_create, pflog_clone_destroy);

struct ifnet	*pflogifs[PFLOGIFS_MAX];	/* for fast access */

extern int ifqmaxlen;

void
pflogattach(int npflog)
{
	int	i;
	LIST_INIT(&pflogif_list);
	for (i = 0; i < PFLOGIFS_MAX; i++)
		pflogifs[i] = NULL;
	(void) pflog_clone_create(&pflog_cloner, 0);
	if_clone_attach(&pflog_cloner);
}

int
pflog_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet *ifp;
	struct pflog_softc *pflogif;
	int s;

	if (unit >= PFLOGIFS_MAX)
		return (EINVAL);

	if ((pflogif = malloc(sizeof(*pflogif), M_DEVBUF, M_NOWAIT)) == NULL)
		return (ENOMEM);
	bzero(pflogif, sizeof(*pflogif));

	pflogif->sc_unit = unit;
	ifp = &pflogif->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "pflog%d", unit);
	ifp->if_softc = pflogif;
	ifp->if_mtu = PFLOGMTU;
	ifp->if_ioctl = pflogioctl;
	ifp->if_output = pflogoutput;
	ifp->if_start = pflogstart;
	ifp->if_type = IFT_PFLOG;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = PFLOG_HDRLEN;
	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&pflogif->sc_if.if_bpf, ifp, DLT_PFLOG, PFLOG_HDRLEN);
#endif

	s = splnet();
	LIST_INSERT_HEAD(&pflogif_list, pflogif, sc_list);
	pflogifs[unit] = ifp;
	splx(s);

	return (0);
}

int
pflog_clone_destroy(struct ifnet *ifp)
{
	struct pflog_softc	*pflogif = ifp->if_softc;
	int			 s;

	s = splnet();
	pflogifs[pflogif->sc_unit] = NULL;
	LIST_REMOVE(pflogif, sc_list);
	splx(s);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);
	free(pflogif, M_DEVBUF);
	return (0);
}

/*
 * Start output on the pflog interface.
 */
void
pflogstart(struct ifnet *ifp)
{
	struct mbuf *m;
	int s;

	for (;;) {
		s = splnet();
		IF_DROP(&ifp->if_snd);
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			return;
		else
			m_freem(m);
	}
}

int
pflogoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	m_freem(m);
	return (0);
}

/* ARGSUSED */
int
pflogioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

int
pflog_packet(struct pfi_kif *kif, struct mbuf *m, sa_family_t af, u_int8_t dir,
    u_int8_t reason, struct pf_rule *rm, struct pf_rule *am,
    struct pf_ruleset *ruleset, struct pf_pdesc *pd)
{
#if NBPFILTER > 0
	struct ifnet *ifn;
	struct pfloghdr hdr;

	if (kif == NULL || m == NULL || rm == NULL || pd == NULL)
		return (-1);

	if ((ifn = pflogifs[rm->logif]) == NULL || !ifn->if_bpf)
		return (0);

	bzero(&hdr, sizeof(hdr));
	hdr.length = PFLOG_REAL_HDRLEN;
	hdr.af = af;
	hdr.action = rm->action;
	hdr.reason = reason;
	memcpy(hdr.ifname, kif->pfik_name, sizeof(hdr.ifname));

	if (am == NULL) {
		hdr.rulenr = htonl(rm->nr);
		hdr.subrulenr = -1;
	} else {
		hdr.rulenr = htonl(am->nr);
		hdr.subrulenr = htonl(rm->nr);
		if (ruleset != NULL && ruleset->anchor != NULL)
			strlcpy(hdr.ruleset, ruleset->anchor->name,
			    sizeof(hdr.ruleset));
	}
	if (rm->log & PF_LOG_SOCKET_LOOKUP && !pd->lookup.done)
		pd->lookup.done = pf_socket_lookup(dir, pd);
	if (pd->lookup.done > 0) {
		hdr.uid = pd->lookup.uid;
		hdr.pid = pd->lookup.pid;
	} else {
		hdr.uid = UID_MAX;
		hdr.pid = NO_PID;
	}
	hdr.rule_uid = rm->cuid;
	hdr.rule_pid = rm->cpid;
	hdr.dir = dir;

#ifdef INET
	if (af == AF_INET && dir == PF_OUT) {
		struct ip *ip;

		ip = mtod(m, struct ip *);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, ip->ip_hl << 2);
	}
#endif /* INET */

	ifn->if_opackets++;
	ifn->if_obytes += m->m_pkthdr.len;
	bpf_mtap_hdr(ifn->if_bpf, (char *)&hdr, PFLOG_HDRLEN, m,
	    BPF_DIRECTION_OUT);
#endif

	return (0);
}
