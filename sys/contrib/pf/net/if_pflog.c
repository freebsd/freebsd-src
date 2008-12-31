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

#ifdef __FreeBSD__
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_bpf.h"
#include "opt_pf.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/contrib/pf/net/if_pflog.c,v 1.21.6.1 2008/11/25 02:59:29 kensmith Exp $");

#ifdef DEV_BPF
#define	NBPFILTER	DEV_BPF
#else
#define	NBPFILTER	0
#endif

#ifdef DEV_PFLOG
#define	NPFLOG		DEV_PFLOG
#else
#define	NPFLOG		0
#endif

#else /* ! __FreeBSD__ */
#include "bpfilter.h"
#include "pflog.h"
#endif /* __FreeBSD__ */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socket.h>
#ifdef __FreeBSD__
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sockio.h>
#else
#include <sys/ioctl.h>
#endif

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_clone.h>
#endif
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

#ifdef __FreeBSD__
#include <machine/in_cksum.h>
#endif

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
#ifdef __FreeBSD__
static int pflog_clone_create(struct if_clone *, int, caddr_t);
static void pflog_clone_destroy(struct ifnet *);
#else
int	pflog_clone_create(struct if_clone *, int);
int	pflog_clone_destroy(struct ifnet *);
#endif

LIST_HEAD(, pflog_softc)	pflogif_list;
#ifdef __FreeBSD__
IFC_SIMPLE_DECLARE(pflog, 1);    
#else
struct if_clone	pflog_cloner =
    IF_CLONE_INITIALIZER("pflog", pflog_clone_create, pflog_clone_destroy);
#endif

struct ifnet	*pflogifs[PFLOGIFS_MAX];	/* for fast access */

#ifndef __FreeBSD__
extern int ifqmaxlen;
#endif

void
pflogattach(int npflog)
{
	int	i;
	LIST_INIT(&pflogif_list);
	for (i = 0; i < PFLOGIFS_MAX; i++)
		pflogifs[i] = NULL;
#ifndef __FreeBSD__
	(void) pflog_clone_create(&pflog_cloner, 0);
#endif
	if_clone_attach(&pflog_cloner);
}

#ifdef __FreeBSD__
static int
pflog_clone_create(struct if_clone *ifc, int unit, caddr_t param)
#else
int
pflog_clone_create(struct if_clone *ifc, int unit)
#endif
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
#ifdef __FreeBSD__
	ifp = pflogif->sc_ifp = if_alloc(IFT_PFLOG);
	if (ifp == NULL) {
		free(pflogif, M_DEVBUF);
		return (ENOSPC);
	}
	if_initname(ifp, ifc->ifc_name, unit);
#else
	ifp = &pflogif->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "pflog%d", unit);
#endif
	ifp->if_softc = pflogif;
	ifp->if_mtu = PFLOGMTU;
	ifp->if_ioctl = pflogioctl;
	ifp->if_output = pflogoutput;
	ifp->if_start = pflogstart;
#ifndef __FreeBSD__
	ifp->if_type = IFT_PFLOG;
#endif
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = PFLOG_HDRLEN;
	if_attach(ifp);
#ifndef __FreeBSD__
	if_alloc_sadl(ifp);
#endif

#if NBPFILTER > 0
#ifdef __FreeBSD__
	bpfattach(ifp, DLT_PFLOG, PFLOG_HDRLEN);
#else
	bpfattach(&pflogif->sc_if.if_bpf, ifp, DLT_PFLOG, PFLOG_HDRLEN);
#endif
#endif

	s = splnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	LIST_INSERT_HEAD(&pflogif_list, pflogif, sc_list);
	pflogifs[unit] = ifp;
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

	return (0);
}

#ifdef __FreeBSD__
static void
pflog_clone_destroy(struct ifnet *ifp)
#else
int
pflog_clone_destroy(struct ifnet *ifp)
#endif
{
	struct pflog_softc	*pflogif = ifp->if_softc;
	int			 s;

	s = splnet();
#ifdef __FreeBSD__
	PF_LOCK();
#endif
	pflogifs[pflogif->sc_unit] = NULL;
	LIST_REMOVE(pflogif, sc_list);
#ifdef __FreeBSD__
	PF_UNLOCK();
#endif
	splx(s);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);
#ifdef __FreeBSD__
	if_free(ifp);
#endif
	free(pflogif, M_DEVBUF);
#ifndef __FreeBSD__
	return (0);
#endif
}

/*
 * Start output on the pflog interface.
 */
void
pflogstart(struct ifnet *ifp)
{
	struct mbuf *m;
#ifndef __FreeBSD__
	int s;
#endif

	for (;;) {
#ifdef __FreeBSD__
		IF_LOCK(&ifp->if_snd);
		_IF_DROP(&ifp->if_snd);
		_IF_DEQUEUE(&ifp->if_snd, m);
		IF_UNLOCK(&ifp->if_snd);
#else
		s = splnet();
		IF_DROP(&ifp->if_snd);
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);
#endif

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
#ifdef __FreeBSD__
		if (ifp->if_flags & IFF_UP)
			ifp->if_drv_flags |= IFF_DRV_RUNNING;
		else
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
#else
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
#endif
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
#ifdef __FreeBSD__
		/* 
		 * XXX: This should not happen as we force an early lookup
		 * via debug.pfugidhack
		 */
		 ; /* empty */
#else
		pd->lookup.done = pf_socket_lookup(dir, pd);
#endif
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
#ifdef __FreeBSD__
	BPF_MTAP2(ifn, &hdr, PFLOG_HDRLEN, m);
#else
	bpf_mtap_hdr(ifn->if_bpf, (char *)&hdr, PFLOG_HDRLEN, m,
	    BPF_DIRECTION_OUT);
#endif
#endif

	return (0);
}

#ifdef __FreeBSD__
static int
pflog_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		pflogattach(1);
		PF_LOCK();
		pflog_packet_ptr = pflog_packet;
		PF_UNLOCK();
		break;
	case MOD_UNLOAD:
		PF_LOCK();
		pflog_packet_ptr = NULL;
		PF_UNLOCK();
		if_clone_detach(&pflog_cloner);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

static moduledata_t pflog_mod = { "pflog", pflog_modevent, 0 };

#define PFLOG_MODVER 1

DECLARE_MODULE(pflog, pflog_mod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);
MODULE_VERSION(pflog, PFLOG_MODVER);
MODULE_DEPEND(pflog, pf, PF_MODVER, PF_MODVER, PF_MODVER);
#endif /* __FreeBSD__ */
