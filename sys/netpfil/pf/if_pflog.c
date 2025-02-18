/*-
 * SPDX-License-Identifier: ISC
 *
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
 *
 *	$OpenBSD: if_pflog.c,v 1.26 2007/10/18 21:58:18 mpf Exp $
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_bpf.h"
#include "opt_pf.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_pflog.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/vnet.h>
#include <net/pfvar.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#endif
#ifdef	INET
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#endif /* INET6 */

#ifdef INET
#include <machine/in_cksum.h>
#endif /* INET */

#define PFLOGMTU	(32768 + MHLEN + MLEN)

#ifdef PFLOGDEBUG
#define DPRINTF(x)    do { if (pflogdebug) printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

static int	pflogoutput(struct ifnet *, struct mbuf *,
		    const struct sockaddr *, struct route *);
static void	pflogattach(int);
static int	pflogifs_resize(size_t);
static int	pflogioctl(struct ifnet *, u_long, caddr_t);
static void	pflogstart(struct ifnet *);
static int	pflog_clone_create(struct if_clone *, char *, size_t,
		    struct ifc_data *, struct ifnet **);
static int	pflog_clone_destroy(struct if_clone *, struct ifnet *, uint32_t);

static const char pflogname[] = "pflog";

VNET_DEFINE_STATIC(struct if_clone *, pflog_cloner);
#define	V_pflog_cloner		VNET(pflog_cloner)

VNET_DEFINE_STATIC(int, npflogifs) = 0;
#define	V_npflogifs		VNET(npflogifs)
VNET_DEFINE(struct ifnet **, pflogifs);	/* for fast access */
#define	V_pflogifs		VNET(pflogifs)

static void
pflogattach(int npflog __unused)
{
	struct if_clone_addreq req = {
		.create_f = pflog_clone_create,
		.destroy_f = pflog_clone_destroy,
		.flags = IFC_F_AUTOUNIT,
	};
	V_pflog_cloner = ifc_attach_cloner(pflogname, &req);
	struct ifc_data ifd = { .unit = 0 };
	ifc_create_ifp(pflogname, &ifd, NULL);
}

static int
pflogifs_resize(size_t n)
{
	struct ifnet **p;
	int i;

	if (n > SIZE_MAX / sizeof(struct ifnet *))
		return (EINVAL);
	if (n == 0)
		p = NULL;
	else if ((p = malloc(n * sizeof(struct ifnet *), M_DEVBUF,
	    M_NOWAIT | M_ZERO)) == NULL)
		return (ENOMEM);
	for (i = 0; i < n; i++) {
		if (i < V_npflogifs)
			p[i] = V_pflogifs[i];
		else
			p[i] = NULL;
	}

	if (V_pflogifs)
		free(V_pflogifs, M_DEVBUF);
	V_pflogifs = p;
	V_npflogifs = n;

	return (0);
}

static int
pflog_clone_create(struct if_clone *ifc, char *name, size_t maxlen,
    struct ifc_data *ifd, struct ifnet **ifpp)
{
	struct ifnet *ifp;

	ifp = if_alloc(IFT_PFLOG);
	if_initname(ifp, pflogname, ifd->unit);
	ifp->if_mtu = PFLOGMTU;
	ifp->if_ioctl = pflogioctl;
	ifp->if_output = pflogoutput;
	ifp->if_start = pflogstart;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = PFLOG_HDRLEN;
	if_attach(ifp);

	bpfattach(ifp, DLT_PFLOG, PFLOG_HDRLEN);

	if (ifd->unit + 1 > V_npflogifs &&
	    pflogifs_resize(ifd->unit + 1) != 0) {
		pflog_clone_destroy(ifc, ifp, IFC_F_FORCE);
		return (ENOMEM);
	}
	V_pflogifs[ifd->unit] = ifp;
	*ifpp = ifp;

	return (0);
}

static int
pflog_clone_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	int i;

	if (ifp->if_dunit == 0 && (flags & IFC_F_FORCE) == 0)
		return (EINVAL);

	for (i = 0; i < V_npflogifs; i++)
		if (V_pflogifs[i] == ifp)
			V_pflogifs[i] = NULL;

	bpfdetach(ifp);
	if_detach(ifp);
	if_free(ifp);

	return (0);
}

/*
 * Start output on the pflog interface.
 */
static void
pflogstart(struct ifnet *ifp)
{
	struct mbuf *m;

	for (;;) {
		IF_LOCK(&ifp->if_snd);
		_IF_DEQUEUE(&ifp->if_snd, m);
		IF_UNLOCK(&ifp->if_snd);

		if (m == NULL)
			return;
		else
			m_freem(m);
	}
}

static int
pflogoutput(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
	struct route *rt)
{
	m_freem(m);
	return (0);
}

/* ARGSUSED */
static int
pflogioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_drv_flags |= IFF_DRV_RUNNING;
		else
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

static int
pflog_packet(uint8_t action, u_int8_t reason,
    struct pf_krule *rm, struct pf_krule *am,
    struct pf_kruleset *ruleset, struct pf_pdesc *pd, int lookupsafe)
{
	struct ifnet *ifn;
	struct pfloghdr hdr;

	if (rm == NULL || pd == NULL)
		return (1);

	ifn = V_pflogifs[rm->logif];
	if (ifn == NULL || !bpf_peers_present(ifn->if_bpf))
		return (0);

	bzero(&hdr, sizeof(hdr));
	hdr.length = PFLOG_REAL_HDRLEN;
	hdr.af = pd->af;
	hdr.action = action;
	hdr.reason = reason;
	memcpy(hdr.ifname, pd->kif->pfik_name, sizeof(hdr.ifname));

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
	hdr.ridentifier = htonl(rm->ridentifier);
	/*
	 * XXXGL: we avoid pf_socket_lookup() when we are holding
	 * state lock, since this leads to unsafe LOR.
	 * These conditions are very very rare, however.
	 */
	if (rm->log & PF_LOG_SOCKET_LOOKUP && !pd->lookup.done && lookupsafe)
		pd->lookup.done = pf_socket_lookup(pd);
	if (pd->lookup.done > 0)
		hdr.uid = pd->lookup.uid;
	else
		hdr.uid = UID_MAX;
	hdr.pid = NO_PID;
	hdr.rule_uid = rm->cuid;
	hdr.rule_pid = rm->cpid;
	hdr.dir = pd->dir;

#ifdef INET
	if (pd->af == AF_INET && pd->dir == PF_OUT) {
		struct ip *ip;

		ip = mtod(pd->m, struct ip *);
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(pd->m, ip->ip_hl << 2);
	}
#endif /* INET */

	if_inc_counter(ifn, IFCOUNTER_OPACKETS, 1);
	if_inc_counter(ifn, IFCOUNTER_OBYTES, pd->m->m_pkthdr.len);
	bpf_mtap2(ifn->if_bpf, &hdr, PFLOG_HDRLEN, pd->m);

	return (0);
}

static void
vnet_pflog_init(const void *unused __unused)
{

	pflogattach(1);
}
VNET_SYSINIT(vnet_pflog_init, SI_SUB_PROTO_FIREWALL, SI_ORDER_ANY,
    vnet_pflog_init, NULL);

static void
vnet_pflog_uninit(const void *unused __unused)
{

	ifc_detach_cloner(V_pflog_cloner);
}
/*
 * Detach after pf is gone; otherwise we might touch pflog memory
 * from within pf after freeing pflog.
 */
VNET_SYSUNINIT(vnet_pflog_uninit, SI_SUB_INIT_IF, SI_ORDER_SECOND,
    vnet_pflog_uninit, NULL);

static int
pflog_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		PF_RULES_WLOCK();
		pflog_packet_ptr = pflog_packet;
		PF_RULES_WUNLOCK();
		break;
	case MOD_UNLOAD:
		PF_RULES_WLOCK();
		pflog_packet_ptr = NULL;
		PF_RULES_WUNLOCK();
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return error;
}

static moduledata_t pflog_mod = { pflogname, pflog_modevent, 0 };

#define PFLOG_MODVER 1

/* Do not run before pf is initialized as we depend on its locks. */
DECLARE_MODULE(pflog, pflog_mod, SI_SUB_PROTO_FIREWALL, SI_ORDER_ANY);
MODULE_VERSION(pflog, PFLOG_MODVER);
MODULE_DEPEND(pflog, pf, PF_MODVER, PF_MODVER, PF_MODVER);
