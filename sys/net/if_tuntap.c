/*	$NetBSD: if_tun.c,v 1.14 1994/06/29 06:36:25 cgd Exp $	*/
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
 * All rights reserved.
 * Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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
 * BASED ON:
 * -------------------------------------------------------------------------
 *
 * Copyright (c) 1988, Julian Onions <jpo@cs.nott.ac.uk>
 * Nottingham University 1987.
 *
 * This source may be freely distributed, however I would be interested
 * in any changes that are made.
 *
 * This driver takes packets off the IP i/f and hands them up to a
 * user process to have its wicked way with. This driver has it's
 * roots in a similar driver written by Phil Cockcroft (formerly) at
 * UCL. This driver is based much more on read/write/poll mode of
 * operation though.
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/jail.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/eventhandler.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/syslog.h>
#include <sys/ttycom.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/signalvar.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/random.h>
#include <sys/ctype.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/vnet.h>
#include <netinet/in.h>
#ifdef INET
#include <netinet/ip.h>
#endif
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <net/bpf.h>
#include <net/if_tap.h>
#include <net/if_tun.h>

#include <dev/virtio/network/virtio_net.h>

#include <sys/queue.h>
#include <sys/condvar.h>
#include <security/mac/mac_framework.h>

struct tuntap_driver;

/*
 * tun_list is protected by global tunmtx.  Other mutable fields are
 * protected by tun->tun_mtx, or by their owning subsystem.  tun_dev is
 * static for the duration of a tunnel interface.
 */
struct tuntap_softc {
	TAILQ_ENTRY(tuntap_softc)	 tun_list;
	struct cdev			*tun_alias;
	struct cdev			*tun_dev;
	u_short				 tun_flags;	/* misc flags */
#define	TUN_OPEN	0x0001
#define	TUN_INITED	0x0002
#define	TUN_UNUSED1	0x0008
#define	TUN_UNUSED2	0x0010
#define	TUN_LMODE	0x0020
#define	TUN_RWAIT	0x0040
#define	TUN_ASYNC	0x0080
#define	TUN_IFHEAD	0x0100
#define	TUN_DYING	0x0200
#define	TUN_L2		0x0400
#define	TUN_VMNET	0x0800

#define	TUN_DRIVER_IDENT_MASK	(TUN_L2 | TUN_VMNET)
#define	TUN_READY		(TUN_OPEN | TUN_INITED)

	pid_t			 tun_pid;	/* owning pid */
	struct ifnet		*tun_ifp;	/* the interface */
	struct sigio		*tun_sigio;	/* async I/O info */
	struct tuntap_driver	*tun_drv;	/* appropriate driver */
	struct selinfo		 tun_rsel;	/* read select */
	struct mtx		 tun_mtx;	/* softc field mutex */
	struct cv		 tun_cv;	/* for ref'd dev destroy */
	struct ether_addr	 tun_ether;	/* remote address */
	int			 tun_busy;	/* busy count */
	int			 tun_vhdrlen;	/* virtio-net header length */
};
#define	TUN2IFP(sc)	((sc)->tun_ifp)

#define	TUNDEBUG	if (tundebug) if_printf

#define	TUN_LOCK(tp)		mtx_lock(&(tp)->tun_mtx)
#define	TUN_UNLOCK(tp)		mtx_unlock(&(tp)->tun_mtx)
#define	TUN_LOCK_ASSERT(tp)	mtx_assert(&(tp)->tun_mtx, MA_OWNED);

#define	TUN_VMIO_FLAG_MASK	0x0fff

/*
 * Interface capabilities of a tap device that supports the virtio-net
 * header.
 */
#define TAP_VNET_HDR_CAPS	(IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6	\
				| IFCAP_VLAN_HWCSUM			\
				| IFCAP_TSO | IFCAP_LRO			\
				| IFCAP_VLAN_HWTSO)

#define TAP_ALL_OFFLOAD		(CSUM_TSO | CSUM_TCP | CSUM_UDP |\
				    CSUM_TCP_IPV6 | CSUM_UDP_IPV6)

/*
 * All mutable global variables in if_tun are locked using tunmtx, with
 * the exception of tundebug, which is used unlocked, and the drivers' *clones,
 * which are static after setup.
 */
static struct mtx tunmtx;
static eventhandler_tag arrival_tag;
static eventhandler_tag clone_tag;
static const char tunname[] = "tun";
static const char tapname[] = "tap";
static const char vmnetname[] = "vmnet";
static MALLOC_DEFINE(M_TUN, tunname, "Tunnel Interface");
static int tundebug = 0;
static int tundclone = 1;
static int tap_allow_uopen = 0;	/* allow user devfs cloning */
static int tapuponopen = 0;	/* IFF_UP on open() */
static int tapdclone = 1;	/* enable devfs cloning */

static TAILQ_HEAD(,tuntap_softc)	tunhead = TAILQ_HEAD_INITIALIZER(tunhead);
SYSCTL_INT(_debug, OID_AUTO, if_tun_debug, CTLFLAG_RW, &tundebug, 0, "");

static struct sx tun_ioctl_sx;
SX_SYSINIT(tun_ioctl_sx, &tun_ioctl_sx, "tun_ioctl");

SYSCTL_DECL(_net_link);
/* tun */
static SYSCTL_NODE(_net_link, OID_AUTO, tun, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IP tunnel software network interface");
SYSCTL_INT(_net_link_tun, OID_AUTO, devfs_cloning, CTLFLAG_RWTUN, &tundclone, 0,
    "Enable legacy devfs interface creation");

/* tap */
static SYSCTL_NODE(_net_link, OID_AUTO, tap, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Ethernet tunnel software network interface");
SYSCTL_INT(_net_link_tap, OID_AUTO, user_open, CTLFLAG_RW, &tap_allow_uopen, 0,
    "Enable legacy devfs interface creation for all users");
SYSCTL_INT(_net_link_tap, OID_AUTO, up_on_open, CTLFLAG_RW, &tapuponopen, 0,
    "Bring interface up when /dev/tap is opened");
SYSCTL_INT(_net_link_tap, OID_AUTO, devfs_cloning, CTLFLAG_RWTUN, &tapdclone, 0,
    "Enable legacy devfs interface creation");
SYSCTL_INT(_net_link_tap, OID_AUTO, debug, CTLFLAG_RW, &tundebug, 0, "");

static int	tun_create_device(struct tuntap_driver *drv, int unit,
    struct ucred *cr, struct cdev **dev, const char *name);
static int	tun_busy_locked(struct tuntap_softc *tp);
static void	tun_unbusy_locked(struct tuntap_softc *tp);
static int	tun_busy(struct tuntap_softc *tp);
static void	tun_unbusy(struct tuntap_softc *tp);

static int	tuntap_name2info(const char *name, int *unit, int *flags);
static void	tunclone(void *arg, struct ucred *cred, char *name,
		    int namelen, struct cdev **dev);
static void	tuncreate(struct cdev *dev);
static void	tundtor(void *data);
static void	tunrename(void *arg, struct ifnet *ifp);
static int	tunifioctl(struct ifnet *, u_long, caddr_t);
static void	tuninit(struct ifnet *);
static void	tunifinit(void *xtp);
static int	tuntapmodevent(module_t, int, void *);
static int	tunoutput(struct ifnet *, struct mbuf *,
		    const struct sockaddr *, struct route *ro);
static void	tunstart(struct ifnet *);
static void	tunstart_l2(struct ifnet *);

static int	tun_clone_match(struct if_clone *ifc, const char *name);
static int	tap_clone_match(struct if_clone *ifc, const char *name);
static int	vmnet_clone_match(struct if_clone *ifc, const char *name);
static int	tun_clone_create(struct if_clone *, char *, size_t, caddr_t);
static int	tun_clone_destroy(struct if_clone *, struct ifnet *);
static void	tun_vnethdr_set(struct ifnet *ifp, int vhdrlen);

static d_open_t		tunopen;
static d_read_t		tunread;
static d_write_t	tunwrite;
static d_ioctl_t	tunioctl;
static d_poll_t		tunpoll;
static d_kqfilter_t	tunkqfilter;

static int		tunkqread(struct knote *, long);
static int		tunkqwrite(struct knote *, long);
static void		tunkqdetach(struct knote *);

static struct filterops tun_read_filterops = {
	.f_isfd =	1,
	.f_attach =	NULL,
	.f_detach =	tunkqdetach,
	.f_event =	tunkqread,
};

static struct filterops tun_write_filterops = {
	.f_isfd =	1,
	.f_attach =	NULL,
	.f_detach =	tunkqdetach,
	.f_event =	tunkqwrite,
};

static struct tuntap_driver {
	struct cdevsw		 cdevsw;
	int			 ident_flags;
	struct unrhdr		*unrhdr;
	struct clonedevs	*clones;
	ifc_match_t		*clone_match_fn;
	ifc_create_t		*clone_create_fn;
	ifc_destroy_t		*clone_destroy_fn;
} tuntap_drivers[] = {
	{
		.ident_flags =	0,
		.cdevsw =	{
		    .d_version =	D_VERSION,
		    .d_flags =		D_NEEDMINOR,
		    .d_open =		tunopen,
		    .d_read =		tunread,
		    .d_write =		tunwrite,
		    .d_ioctl =		tunioctl,
		    .d_poll =		tunpoll,
		    .d_kqfilter =	tunkqfilter,
		    .d_name =		tunname,
		},
		.clone_match_fn =	tun_clone_match,
		.clone_create_fn =	tun_clone_create,
		.clone_destroy_fn =	tun_clone_destroy,
	},
	{
		.ident_flags =	TUN_L2,
		.cdevsw =	{
		    .d_version =	D_VERSION,
		    .d_flags =		D_NEEDMINOR,
		    .d_open =		tunopen,
		    .d_read =		tunread,
		    .d_write =		tunwrite,
		    .d_ioctl =		tunioctl,
		    .d_poll =		tunpoll,
		    .d_kqfilter =	tunkqfilter,
		    .d_name =		tapname,
		},
		.clone_match_fn =	tap_clone_match,
		.clone_create_fn =	tun_clone_create,
		.clone_destroy_fn =	tun_clone_destroy,
	},
	{
		.ident_flags =	TUN_L2 | TUN_VMNET,
		.cdevsw =	{
		    .d_version =	D_VERSION,
		    .d_flags =		D_NEEDMINOR,
		    .d_open =		tunopen,
		    .d_read =		tunread,
		    .d_write =		tunwrite,
		    .d_ioctl =		tunioctl,
		    .d_poll =		tunpoll,
		    .d_kqfilter =	tunkqfilter,
		    .d_name =		vmnetname,
		},
		.clone_match_fn =	vmnet_clone_match,
		.clone_create_fn =	tun_clone_create,
		.clone_destroy_fn =	tun_clone_destroy,
	},
};

struct tuntap_driver_cloner {
	SLIST_ENTRY(tuntap_driver_cloner)	 link;
	struct tuntap_driver			*drv;
	struct if_clone				*cloner;
};

VNET_DEFINE_STATIC(SLIST_HEAD(, tuntap_driver_cloner), tuntap_driver_cloners) =
    SLIST_HEAD_INITIALIZER(tuntap_driver_cloners);

#define	V_tuntap_driver_cloners	VNET(tuntap_driver_cloners)

/*
 * Mechanism for marking a tunnel device as busy so that we can safely do some
 * orthogonal operations (such as operations on devices) without racing against
 * tun_destroy.  tun_destroy will wait on the condvar if we're at all busy or
 * open, to be woken up when the condition is alleviated.
 */
static int
tun_busy_locked(struct tuntap_softc *tp)
{

	TUN_LOCK_ASSERT(tp);
	if ((tp->tun_flags & TUN_DYING) != 0) {
		/*
		 * Perhaps unintuitive, but the device is busy going away.
		 * Other interpretations of EBUSY from tun_busy make little
		 * sense, since making a busy device even more busy doesn't
		 * sound like a problem.
		 */
		return (EBUSY);
	}

	++tp->tun_busy;
	return (0);
}

static void
tun_unbusy_locked(struct tuntap_softc *tp)
{

	TUN_LOCK_ASSERT(tp);
	KASSERT(tp->tun_busy != 0, ("tun_unbusy: called for non-busy tunnel"));

	--tp->tun_busy;
	/* Wake up anything that may be waiting on our busy tunnel. */
	if (tp->tun_busy == 0)
		cv_broadcast(&tp->tun_cv);
}

static int
tun_busy(struct tuntap_softc *tp)
{
	int ret;

	TUN_LOCK(tp);
	ret = tun_busy_locked(tp);
	TUN_UNLOCK(tp);
	return (ret);
}

static void
tun_unbusy(struct tuntap_softc *tp)
{

	TUN_LOCK(tp);
	tun_unbusy_locked(tp);
	TUN_UNLOCK(tp);
}

/*
 * Sets unit and/or flags given the device name.  Must be called with correct
 * vnet context.
 */
static int
tuntap_name2info(const char *name, int *outunit, int *outflags)
{
	struct tuntap_driver *drv;
	struct tuntap_driver_cloner *drvc;
	char *dname;
	int flags, unit;
	bool found;

	if (name == NULL)
		return (EINVAL);

	/*
	 * Needed for dev_stdclone, but dev_stdclone will not modify, it just
	 * wants to be able to pass back a char * through the second param. We
	 * will always set that as NULL here, so we'll fake it.
	 */
	dname = __DECONST(char *, name);
	found = false;

	KASSERT(!SLIST_EMPTY(&V_tuntap_driver_cloners),
	    ("tuntap_driver_cloners failed to initialize"));
	SLIST_FOREACH(drvc, &V_tuntap_driver_cloners, link) {
		KASSERT(drvc->drv != NULL,
		    ("tuntap_driver_cloners entry not properly initialized"));
		drv = drvc->drv;

		if (strcmp(name, drv->cdevsw.d_name) == 0) {
			found = true;
			unit = -1;
			flags = drv->ident_flags;
			break;
		}

		if (dev_stdclone(dname, NULL, drv->cdevsw.d_name, &unit) == 1) {
			found = true;
			flags = drv->ident_flags;
			break;
		}
	}

	if (!found)
		return (ENXIO);

	if (outunit != NULL)
		*outunit = unit;
	if (outflags != NULL)
		*outflags = flags;
	return (0);
}

/*
 * Get driver information from a set of flags specified.  Masks the identifying
 * part of the flags and compares it against all of the available
 * tuntap_drivers. Must be called with correct vnet context.
 */
static struct tuntap_driver *
tuntap_driver_from_flags(int tun_flags)
{
	struct tuntap_driver *drv;
	struct tuntap_driver_cloner *drvc;

	KASSERT(!SLIST_EMPTY(&V_tuntap_driver_cloners),
	    ("tuntap_driver_cloners failed to initialize"));
	SLIST_FOREACH(drvc, &V_tuntap_driver_cloners, link) {
		KASSERT(drvc->drv != NULL,
		    ("tuntap_driver_cloners entry not properly initialized"));
		drv = drvc->drv;
		if ((tun_flags & TUN_DRIVER_IDENT_MASK) == drv->ident_flags)
			return (drv);
	}

	return (NULL);
}

static int
tun_clone_match(struct if_clone *ifc, const char *name)
{
	int tunflags;

	if (tuntap_name2info(name, NULL, &tunflags) == 0) {
		if ((tunflags & TUN_L2) == 0)
			return (1);
	}

	return (0);
}

static int
tap_clone_match(struct if_clone *ifc, const char *name)
{
	int tunflags;

	if (tuntap_name2info(name, NULL, &tunflags) == 0) {
		if ((tunflags & (TUN_L2 | TUN_VMNET)) == TUN_L2)
			return (1);
	}

	return (0);
}

static int
vmnet_clone_match(struct if_clone *ifc, const char *name)
{
	int tunflags;

	if (tuntap_name2info(name, NULL, &tunflags) == 0) {
		if ((tunflags & TUN_VMNET) != 0)
			return (1);
	}

	return (0);
}

static int
tun_clone_create(struct if_clone *ifc, char *name, size_t len, caddr_t params)
{
	struct tuntap_driver *drv;
	struct cdev *dev;
	int err, i, tunflags, unit;

	tunflags = 0;
	/* The name here tells us exactly what we're creating */
	err = tuntap_name2info(name, &unit, &tunflags);
	if (err != 0)
		return (err);

	drv = tuntap_driver_from_flags(tunflags);
	if (drv == NULL)
		return (ENXIO);

	if (unit != -1) {
		/* If this unit number is still available that's okay. */
		if (alloc_unr_specific(drv->unrhdr, unit) == -1)
			return (EEXIST);
	} else {
		unit = alloc_unr(drv->unrhdr);
	}

	snprintf(name, IFNAMSIZ, "%s%d", drv->cdevsw.d_name, unit);

	/* find any existing device, or allocate new unit number */
	dev = NULL;
	i = clone_create(&drv->clones, &drv->cdevsw, &unit, &dev, 0);
	if (i == 0)
		dev_ref(dev);
	/* No preexisting struct cdev *, create one */
	if (i != 0)
		i = tun_create_device(drv, unit, NULL, &dev, name);
	if (i == 0)
		tuncreate(dev);

	return (i);
}

static void
tunclone(void *arg, struct ucred *cred, char *name, int namelen,
    struct cdev **dev)
{
	char devname[SPECNAMELEN + 1];
	struct tuntap_driver *drv;
	int append_unit, i, u, tunflags;
	bool mayclone;

	if (*dev != NULL)
		return;

	tunflags = 0;
	CURVNET_SET(CRED_TO_VNET(cred));
	if (tuntap_name2info(name, &u, &tunflags) != 0)
		goto out;	/* Not recognized */

	if (u != -1 && u > IF_MAXUNIT)
		goto out;	/* Unit number too high */

	mayclone = priv_check_cred(cred, PRIV_NET_IFCREATE) == 0;
	if ((tunflags & TUN_L2) != 0) {
		/* tap/vmnet allow user open with a sysctl */
		mayclone = (mayclone || tap_allow_uopen) && tapdclone;
	} else {
		mayclone = mayclone && tundclone;
	}

	/*
	 * If tun cloning is enabled, only the superuser can create an
	 * interface.
	 */
	if (!mayclone)
		goto out;

	if (u == -1)
		append_unit = 1;
	else
		append_unit = 0;

	drv = tuntap_driver_from_flags(tunflags);
	if (drv == NULL)
		goto out;

	/* find any existing device, or allocate new unit number */
	i = clone_create(&drv->clones, &drv->cdevsw, &u, dev, 0);
	if (i == 0)
		dev_ref(*dev);
	if (i) {
		if (append_unit) {
			namelen = snprintf(devname, sizeof(devname), "%s%d",
			    name, u);
			name = devname;
		}

		i = tun_create_device(drv, u, cred, dev, name);
	}
	if (i == 0)
		if_clone_create(name, namelen, NULL);
out:
	CURVNET_RESTORE();
}

static void
tun_destroy(struct tuntap_softc *tp)
{

	TUN_LOCK(tp);
	tp->tun_flags |= TUN_DYING;
	if (tp->tun_busy != 0)
		cv_wait_unlock(&tp->tun_cv, &tp->tun_mtx);
	else
		TUN_UNLOCK(tp);

	CURVNET_SET(TUN2IFP(tp)->if_vnet);

	/* destroy_dev will take care of any alias. */
	destroy_dev(tp->tun_dev);
	seldrain(&tp->tun_rsel);
	knlist_clear(&tp->tun_rsel.si_note, 0);
	knlist_destroy(&tp->tun_rsel.si_note);
	if ((tp->tun_flags & TUN_L2) != 0) {
		ether_ifdetach(TUN2IFP(tp));
	} else {
		bpfdetach(TUN2IFP(tp));
		if_detach(TUN2IFP(tp));
	}
	sx_xlock(&tun_ioctl_sx);
	TUN2IFP(tp)->if_softc = NULL;
	sx_xunlock(&tun_ioctl_sx);
	free_unr(tp->tun_drv->unrhdr, TUN2IFP(tp)->if_dunit);
	if_free(TUN2IFP(tp));
	mtx_destroy(&tp->tun_mtx);
	cv_destroy(&tp->tun_cv);
	free(tp, M_TUN);
	CURVNET_RESTORE();
}

static int
tun_clone_destroy(struct if_clone *ifc __unused, struct ifnet *ifp)
{
	struct tuntap_softc *tp = ifp->if_softc;

	mtx_lock(&tunmtx);
	TAILQ_REMOVE(&tunhead, tp, tun_list);
	mtx_unlock(&tunmtx);
	tun_destroy(tp);

	return (0);
}

static void
vnet_tun_init(const void *unused __unused)
{
	struct tuntap_driver *drv;
	struct tuntap_driver_cloner *drvc;
	int i;

	for (i = 0; i < nitems(tuntap_drivers); ++i) {
		drv = &tuntap_drivers[i];
		drvc = malloc(sizeof(*drvc), M_TUN, M_WAITOK | M_ZERO);

		drvc->drv = drv;
		drvc->cloner = if_clone_advanced(drv->cdevsw.d_name, 0,
		    drv->clone_match_fn, drv->clone_create_fn,
		    drv->clone_destroy_fn);
		SLIST_INSERT_HEAD(&V_tuntap_driver_cloners, drvc, link);
	};
}
VNET_SYSINIT(vnet_tun_init, SI_SUB_PROTO_IF, SI_ORDER_ANY,
		vnet_tun_init, NULL);

static void
vnet_tun_uninit(const void *unused __unused)
{
	struct tuntap_driver_cloner *drvc;

	while (!SLIST_EMPTY(&V_tuntap_driver_cloners)) {
		drvc = SLIST_FIRST(&V_tuntap_driver_cloners);
		SLIST_REMOVE_HEAD(&V_tuntap_driver_cloners, link);

		if_clone_detach(drvc->cloner);
		free(drvc, M_TUN);
	}
}
VNET_SYSUNINIT(vnet_tun_uninit, SI_SUB_PROTO_IF, SI_ORDER_ANY,
    vnet_tun_uninit, NULL);

static void
tun_uninit(const void *unused __unused)
{
	struct tuntap_driver *drv;
	struct tuntap_softc *tp;
	int i;

	EVENTHANDLER_DEREGISTER(ifnet_arrival_event, arrival_tag);
	EVENTHANDLER_DEREGISTER(dev_clone, clone_tag);
	drain_dev_clone_events();

	mtx_lock(&tunmtx);
	while ((tp = TAILQ_FIRST(&tunhead)) != NULL) {
		TAILQ_REMOVE(&tunhead, tp, tun_list);
		mtx_unlock(&tunmtx);
		tun_destroy(tp);
		mtx_lock(&tunmtx);
	}
	mtx_unlock(&tunmtx);
	for (i = 0; i < nitems(tuntap_drivers); ++i) {
		drv = &tuntap_drivers[i];
		delete_unrhdr(drv->unrhdr);
		clone_cleanup(&drv->clones);
	}
	mtx_destroy(&tunmtx);
}
SYSUNINIT(tun_uninit, SI_SUB_PROTO_IF, SI_ORDER_ANY, tun_uninit, NULL);

static struct tuntap_driver *
tuntap_driver_from_ifnet(const struct ifnet *ifp)
{
	struct tuntap_driver *drv;
	int i;

	if (ifp == NULL)
		return (NULL);

	for (i = 0; i < nitems(tuntap_drivers); ++i) {
		drv = &tuntap_drivers[i];
		if (strcmp(ifp->if_dname, drv->cdevsw.d_name) == 0)
			return (drv);
	}

	return (NULL);
}

static int
tuntapmodevent(module_t mod, int type, void *data)
{
	struct tuntap_driver *drv;
	int i;

	switch (type) {
	case MOD_LOAD:
		mtx_init(&tunmtx, "tunmtx", NULL, MTX_DEF);
		for (i = 0; i < nitems(tuntap_drivers); ++i) {
			drv = &tuntap_drivers[i];
			clone_setup(&drv->clones);
			drv->unrhdr = new_unrhdr(0, IF_MAXUNIT, &tunmtx);
		}
		arrival_tag = EVENTHANDLER_REGISTER(ifnet_arrival_event,
		   tunrename, 0, 1000);
		if (arrival_tag == NULL)
			return (ENOMEM);
		clone_tag = EVENTHANDLER_REGISTER(dev_clone, tunclone, 0, 1000);
		if (clone_tag == NULL)
			return (ENOMEM);
		break;
	case MOD_UNLOAD:
		/* See tun_uninit, so it's done after the vnet_sysuninit() */
		break;
	default:
		return EOPNOTSUPP;
	}
	return 0;
}

static moduledata_t tuntap_mod = {
	"if_tuntap",
	tuntapmodevent,
	0
};

/* We'll only ever have these two, so no need for a macro. */
static moduledata_t tun_mod = { "if_tun", NULL, 0 };
static moduledata_t tap_mod = { "if_tap", NULL, 0 };

DECLARE_MODULE(if_tuntap, tuntap_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_tuntap, 1);
DECLARE_MODULE(if_tun, tun_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_tun, 1);
DECLARE_MODULE(if_tap, tap_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_tap, 1);

static int
tun_create_device(struct tuntap_driver *drv, int unit, struct ucred *cr,
    struct cdev **dev, const char *name)
{
	struct make_dev_args args;
	struct tuntap_softc *tp;
	int error;

	tp = malloc(sizeof(*tp), M_TUN, M_WAITOK | M_ZERO);
	mtx_init(&tp->tun_mtx, "tun_mtx", NULL, MTX_DEF);
	cv_init(&tp->tun_cv, "tun_condvar");
	tp->tun_flags = drv->ident_flags;
	tp->tun_drv = drv;

	make_dev_args_init(&args);
	if (cr != NULL)
		args.mda_flags = MAKEDEV_REF | MAKEDEV_CHECKNAME;
	args.mda_devsw = &drv->cdevsw;
	args.mda_cr = cr;
	args.mda_uid = UID_UUCP;
	args.mda_gid = GID_DIALER;
	args.mda_mode = 0600;
	args.mda_unit = unit;
	args.mda_si_drv1 = tp;
	error = make_dev_s(&args, dev, "%s", name);
	if (error != 0) {
		free(tp, M_TUN);
		return (error);
	}

	KASSERT((*dev)->si_drv1 != NULL,
	    ("Failed to set si_drv1 at %s creation", name));
	tp->tun_dev = *dev;
	knlist_init_mtx(&tp->tun_rsel.si_note, &tp->tun_mtx);
	mtx_lock(&tunmtx);
	TAILQ_INSERT_TAIL(&tunhead, tp, tun_list);
	mtx_unlock(&tunmtx);
	return (0);
}

static void
tunstart(struct ifnet *ifp)
{
	struct tuntap_softc *tp = ifp->if_softc;
	struct mbuf *m;

	TUNDEBUG(ifp, "starting\n");
	if (ALTQ_IS_ENABLED(&ifp->if_snd)) {
		IFQ_LOCK(&ifp->if_snd);
		IFQ_POLL_NOLOCK(&ifp->if_snd, m);
		if (m == NULL) {
			IFQ_UNLOCK(&ifp->if_snd);
			return;
		}
		IFQ_UNLOCK(&ifp->if_snd);
	}

	TUN_LOCK(tp);
	if (tp->tun_flags & TUN_RWAIT) {
		tp->tun_flags &= ~TUN_RWAIT;
		wakeup(tp);
	}
	selwakeuppri(&tp->tun_rsel, PZERO + 1);
	KNOTE_LOCKED(&tp->tun_rsel.si_note, 0);
	if (tp->tun_flags & TUN_ASYNC && tp->tun_sigio) {
		TUN_UNLOCK(tp);
		pgsigio(&tp->tun_sigio, SIGIO, 0);
	} else
		TUN_UNLOCK(tp);
}

/*
 * tunstart_l2
 *
 * queue packets from higher level ready to put out
 */
static void
tunstart_l2(struct ifnet *ifp)
{
	struct tuntap_softc	*tp = ifp->if_softc;

	TUNDEBUG(ifp, "starting\n");

	/*
	 * do not junk pending output if we are in VMnet mode.
	 * XXX: can this do any harm because of queue overflow?
	 */

	TUN_LOCK(tp);
	if (((tp->tun_flags & TUN_VMNET) == 0) &&
	    ((tp->tun_flags & TUN_READY) != TUN_READY)) {
		struct mbuf *m;

		/* Unlocked read. */
		TUNDEBUG(ifp, "not ready, tun_flags = 0x%x\n", tp->tun_flags);

		for (;;) {
			IF_DEQUEUE(&ifp->if_snd, m);
			if (m != NULL) {
				m_freem(m);
				if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			} else
				break;
		}
		TUN_UNLOCK(tp);

		return;
	}

	ifp->if_drv_flags |= IFF_DRV_OACTIVE;

	if (!IFQ_IS_EMPTY(&ifp->if_snd)) {
		if (tp->tun_flags & TUN_RWAIT) {
			tp->tun_flags &= ~TUN_RWAIT;
			wakeup(tp);
		}

		if ((tp->tun_flags & TUN_ASYNC) && (tp->tun_sigio != NULL)) {
			TUN_UNLOCK(tp);
			pgsigio(&tp->tun_sigio, SIGIO, 0);
			TUN_LOCK(tp);
		}

		selwakeuppri(&tp->tun_rsel, PZERO+1);
		KNOTE_LOCKED(&tp->tun_rsel.si_note, 0);
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1); /* obytes are counted in ether_output */
	}

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	TUN_UNLOCK(tp);
} /* tunstart_l2 */

/* XXX: should return an error code so it can fail. */
static void
tuncreate(struct cdev *dev)
{
	struct tuntap_driver *drv;
	struct tuntap_softc *tp;
	struct ifnet *ifp;
	struct ether_addr eaddr;
	int iflags;
	u_char type;

	tp = dev->si_drv1;
	KASSERT(tp != NULL,
	    ("si_drv1 should have been initialized at creation"));

	drv = tp->tun_drv;
	iflags = IFF_MULTICAST;
	if ((tp->tun_flags & TUN_L2) != 0) {
		type = IFT_ETHER;
		iflags |= IFF_BROADCAST | IFF_SIMPLEX;
	} else {
		type = IFT_PPP;
		iflags |= IFF_POINTOPOINT;
	}
	ifp = tp->tun_ifp = if_alloc(type);
	if (ifp == NULL)
		panic("%s%d: failed to if_alloc() interface.\n",
		    drv->cdevsw.d_name, dev2unit(dev));
	ifp->if_softc = tp;
	if_initname(ifp, drv->cdevsw.d_name, dev2unit(dev));
	ifp->if_ioctl = tunifioctl;
	ifp->if_flags = iflags;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_capabilities |= IFCAP_LINKSTATE;
	ifp->if_capenable |= IFCAP_LINKSTATE;

	if ((tp->tun_flags & TUN_L2) != 0) {
		ifp->if_init = tunifinit;
		ifp->if_start = tunstart_l2;

		ether_gen_addr(ifp, &eaddr);
		ether_ifattach(ifp, eaddr.octet);
	} else {
		ifp->if_mtu = TUNMTU;
		ifp->if_start = tunstart;
		ifp->if_output = tunoutput;

		ifp->if_snd.ifq_drv_maxlen = 0;
		IFQ_SET_READY(&ifp->if_snd);

		if_attach(ifp);
		bpfattach(ifp, DLT_NULL, sizeof(u_int32_t));
	}

	TUN_LOCK(tp);
	tp->tun_flags |= TUN_INITED;
	TUN_UNLOCK(tp);

	TUNDEBUG(ifp, "interface %s is created, minor = %#x\n",
	    ifp->if_xname, dev2unit(dev));
}

static void
tunrename(void *arg __unused, struct ifnet *ifp)
{
	struct tuntap_softc *tp;
	int error;

	if ((ifp->if_flags & IFF_RENAMING) == 0)
		return;

	if (tuntap_driver_from_ifnet(ifp) == NULL)
		return;

	/*
	 * We need to grab the ioctl sx long enough to make sure the softc is
	 * still there.  If it is, we can safely try to busy the tun device.
	 * The busy may fail if the device is currently dying, in which case
	 * we do nothing.  If it doesn't fail, the busy count stops the device
	 * from dying until we've created the alias (that will then be
	 * subsequently destroyed).
	 */
	sx_xlock(&tun_ioctl_sx);
	tp = ifp->if_softc;
	if (tp == NULL) {
		sx_xunlock(&tun_ioctl_sx);
		return;
	}
	error = tun_busy(tp);
	sx_xunlock(&tun_ioctl_sx);
	if (error != 0)
		return;
	if (tp->tun_alias != NULL) {
		destroy_dev(tp->tun_alias);
		tp->tun_alias = NULL;
	}

	if (strcmp(ifp->if_xname, tp->tun_dev->si_name) == 0)
		goto out;

	/*
	 * Failure's ok, aliases are created on a best effort basis.  If a
	 * tun user/consumer decides to rename the interface to conflict with
	 * another device (non-ifnet) on the system, we will assume they know
	 * what they are doing.  make_dev_alias_p won't touch tun_alias on
	 * failure, so we use it but ignore the return value.
	 */
	make_dev_alias_p(MAKEDEV_CHECKNAME, &tp->tun_alias, tp->tun_dev, "%s",
	    ifp->if_xname);
out:
	tun_unbusy(tp);
}

static int
tunopen(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct ifnet	*ifp;
	struct tuntap_softc *tp;
	int error, tunflags;

	tunflags = 0;
	CURVNET_SET(TD_TO_VNET(td));
	error = tuntap_name2info(dev->si_name, NULL, &tunflags);
	if (error != 0) {
		CURVNET_RESTORE();
		return (error);	/* Shouldn't happen */
	}

	tp = dev->si_drv1;
	KASSERT(tp != NULL,
	    ("si_drv1 should have been initialized at creation"));

	TUN_LOCK(tp);
	if ((tp->tun_flags & TUN_INITED) == 0) {
		TUN_UNLOCK(tp);
		CURVNET_RESTORE();
		return (ENXIO);
	}
	if ((tp->tun_flags & (TUN_OPEN | TUN_DYING)) != 0) {
		TUN_UNLOCK(tp);
		CURVNET_RESTORE();
		return (EBUSY);
	}

	error = tun_busy_locked(tp);
	KASSERT(error == 0, ("Must be able to busy an unopen tunnel"));
	ifp = TUN2IFP(tp);

	if ((tp->tun_flags & TUN_L2) != 0) {
		bcopy(IF_LLADDR(ifp), tp->tun_ether.octet,
		    sizeof(tp->tun_ether.octet));

		ifp->if_drv_flags |= IFF_DRV_RUNNING;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

		if (tapuponopen)
			ifp->if_flags |= IFF_UP;
	}

	tp->tun_pid = td->td_proc->p_pid;
	tp->tun_flags |= TUN_OPEN;

	if_link_state_change(ifp, LINK_STATE_UP);
	TUNDEBUG(ifp, "open\n");
	TUN_UNLOCK(tp);

	/*
	 * This can fail with either ENOENT or EBUSY.  This is in the middle of
	 * d_open, so ENOENT should not be possible.  EBUSY is possible, but
	 * the only cdevpriv dtor being set will be tundtor and the softc being
	 * passed is constant for a given cdev.  We ignore the possible error
	 * because of this as either "unlikely" or "not actually a problem."
	 */
	(void)devfs_set_cdevpriv(tp, tundtor);
	CURVNET_RESTORE();
	return (0);
}

/*
 * tundtor - tear down the device - mark i/f down & delete
 * routing info
 */
static void
tundtor(void *data)
{
	struct proc *p;
	struct tuntap_softc *tp;
	struct ifnet *ifp;
	bool l2tun;

	tp = data;
	p = curproc;
	ifp = TUN2IFP(tp);

	TUN_LOCK(tp);

	/*
	 * Realistically, we can't be obstinate here.  This only means that the
	 * tuntap device was closed out of order, and the last closer wasn't the
	 * controller.  These are still good to know about, though, as software
	 * should avoid multiple processes with a tuntap device open and
	 * ill-defined transfer of control (e.g., handoff, TUNSIFPID, close in
	 * parent).
	 */
	if (p->p_pid != tp->tun_pid) {
		log(LOG_INFO,
		    "pid %d (%s), %s: tun/tap protocol violation, non-controlling process closed last.\n",
		    p->p_pid, p->p_comm, tp->tun_dev->si_name);
	}

	/*
	 * junk all pending output
	 */
	CURVNET_SET(ifp->if_vnet);

	l2tun = false;
	if ((tp->tun_flags & TUN_L2) != 0) {
		l2tun = true;
		IF_DRAIN(&ifp->if_snd);
	} else {
		IFQ_PURGE(&ifp->if_snd);
	}

	/* For vmnet, we won't do most of the address/route bits */
	if ((tp->tun_flags & TUN_VMNET) != 0 ||
	    (l2tun && (ifp->if_flags & IFF_LINK0) != 0))
		goto out;

	if (ifp->if_flags & IFF_UP) {
		TUN_UNLOCK(tp);
		if_down(ifp);
		TUN_LOCK(tp);
	}

	/* Delete all addresses and routes which reference this interface. */
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		TUN_UNLOCK(tp);
		if_purgeaddrs(ifp);
		TUN_LOCK(tp);
	}

out:
	if_link_state_change(ifp, LINK_STATE_DOWN);
	CURVNET_RESTORE();

	funsetown(&tp->tun_sigio);
	selwakeuppri(&tp->tun_rsel, PZERO + 1);
	KNOTE_LOCKED(&tp->tun_rsel.si_note, 0);
	TUNDEBUG (ifp, "closed\n");
	tp->tun_flags &= ~TUN_OPEN;
	tp->tun_pid = 0;
	tun_vnethdr_set(ifp, 0);

	tun_unbusy_locked(tp);
	TUN_UNLOCK(tp);
}

static void
tuninit(struct ifnet *ifp)
{
	struct tuntap_softc *tp = ifp->if_softc;

	TUNDEBUG(ifp, "tuninit\n");

	TUN_LOCK(tp);
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	if ((tp->tun_flags & TUN_L2) == 0) {
		ifp->if_flags |= IFF_UP;
		getmicrotime(&ifp->if_lastchange);
		TUN_UNLOCK(tp);
	} else {
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		TUN_UNLOCK(tp);
		/* attempt to start output */
		tunstart_l2(ifp);
	}

}

/*
 * Used only for l2 tunnel.
 */
static void
tunifinit(void *xtp)
{
	struct tuntap_softc *tp;

	tp = (struct tuntap_softc *)xtp;
	tuninit(tp->tun_ifp);
}

/*
 * To be called under TUN_LOCK. Update ifp->if_hwassist according to the
 * current value of ifp->if_capenable.
 */
static void
tun_caps_changed(struct ifnet *ifp)
{
	uint64_t hwassist = 0;

	TUN_LOCK_ASSERT((struct tuntap_softc *)ifp->if_softc);
	if (ifp->if_capenable & IFCAP_TXCSUM)
		hwassist |= CSUM_TCP | CSUM_UDP;
	if (ifp->if_capenable & IFCAP_TXCSUM_IPV6)
		hwassist |= CSUM_TCP_IPV6
		    | CSUM_UDP_IPV6;
	if (ifp->if_capenable & IFCAP_TSO4)
		hwassist |= CSUM_IP_TSO;
	if (ifp->if_capenable & IFCAP_TSO6)
		hwassist |= CSUM_IP6_TSO;
	ifp->if_hwassist = hwassist;
}

/*
 * To be called under TUN_LOCK. Update tp->tun_vhdrlen and adjust
 * if_capabilities and if_capenable as needed.
 */
static void
tun_vnethdr_set(struct ifnet *ifp, int vhdrlen)
{
	struct tuntap_softc *tp = ifp->if_softc;

	TUN_LOCK_ASSERT(tp);

	if (tp->tun_vhdrlen == vhdrlen)
		return;

	/*
	 * Update if_capabilities to reflect the
	 * functionalities offered by the virtio-net
	 * header.
	 */
	if (vhdrlen != 0)
		ifp->if_capabilities |=
			TAP_VNET_HDR_CAPS;
	else
		ifp->if_capabilities &=
			~TAP_VNET_HDR_CAPS;
	/*
	 * Disable any capabilities that we don't
	 * support anymore.
	 */
	ifp->if_capenable &= ifp->if_capabilities;
	tun_caps_changed(ifp);
	tp->tun_vhdrlen = vhdrlen;

	TUNDEBUG(ifp, "vnet_hdr_len=%d, if_capabilities=%x\n",
	    vhdrlen, ifp->if_capabilities);
}

/*
 * Process an ioctl request.
 */
static int
tunifioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct tuntap_softc *tp;
	struct ifstat *ifs;
	struct ifmediareq	*ifmr;
	int		dummy, error = 0;
	bool		l2tun;

	ifmr = NULL;
	sx_xlock(&tun_ioctl_sx);
	tp = ifp->if_softc;
	if (tp == NULL) {
		error = ENXIO;
		goto bad;
	}
	l2tun = (tp->tun_flags & TUN_L2) != 0;
	switch(cmd) {
	case SIOCGIFSTATUS:
		ifs = (struct ifstat *)data;
		TUN_LOCK(tp);
		if (tp->tun_pid)
			snprintf(ifs->ascii, sizeof(ifs->ascii),
			    "\tOpened by PID %d\n", tp->tun_pid);
		else
			ifs->ascii[0] = '\0';
		TUN_UNLOCK(tp);
		break;
	case SIOCSIFADDR:
		if (l2tun)
			error = ether_ioctl(ifp, cmd, data);
		else
			tuninit(ifp);
		if (error == 0)
		    TUNDEBUG(ifp, "address set\n");
		break;
	case SIOCSIFMTU:
		ifp->if_mtu = ifr->ifr_mtu;
		TUNDEBUG(ifp, "mtu set\n");
		break;
	case SIOCSIFFLAGS:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;
	case SIOCGIFMEDIA:
		if (!l2tun) {
			error = EINVAL;
			break;
		}

		ifmr = (struct ifmediareq *)data;
		dummy = ifmr->ifm_count;
		ifmr->ifm_count = 1;
		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_active = IFM_ETHER;
		if (tp->tun_flags & TUN_OPEN)
			ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_current = ifmr->ifm_active;
		if (dummy >= 1) {
			int media = IFM_ETHER;
			error = copyout(&media, ifmr->ifm_ulist, sizeof(int));
		}
		break;
	case SIOCSIFCAP:
		TUN_LOCK(tp);
		ifp->if_capenable = ifr->ifr_reqcap;
		tun_caps_changed(ifp);
		TUN_UNLOCK(tp);
		VLAN_CAPABILITIES(ifp);
		break;
	default:
		if (l2tun) {
			error = ether_ioctl(ifp, cmd, data);
		} else {
			error = EINVAL;
		}
	}
bad:
	sx_xunlock(&tun_ioctl_sx);
	return (error);
}

/*
 * tunoutput - queue packets from higher level ready to put out.
 */
static int
tunoutput(struct ifnet *ifp, struct mbuf *m0, const struct sockaddr *dst,
    struct route *ro)
{
	struct tuntap_softc *tp = ifp->if_softc;
	u_short cached_tun_flags;
	int error;
	u_int32_t af;

	TUNDEBUG (ifp, "tunoutput\n");

#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m0);
	if (error) {
		m_freem(m0);
		return (error);
	}
#endif

	/* Could be unlocked read? */
	TUN_LOCK(tp);
	cached_tun_flags = tp->tun_flags;
	TUN_UNLOCK(tp);
	if ((cached_tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG (ifp, "not ready 0%o\n", tp->tun_flags);
		m_freem (m0);
		return (EHOSTDOWN);
	}

	if ((ifp->if_flags & IFF_UP) != IFF_UP) {
		m_freem (m0);
		return (EHOSTDOWN);
	}

	/* BPF writes need to be handled specially. */
	if (dst->sa_family == AF_UNSPEC)
		bcopy(dst->sa_data, &af, sizeof(af));
	else
		af = RO_GET_FAMILY(ro, dst);

	BPF_MTAP2(ifp, &af, sizeof(af), m0);

	/* prepend sockaddr? this may abort if the mbuf allocation fails */
	if (cached_tun_flags & TUN_LMODE) {
		/* allocate space for sockaddr */
		M_PREPEND(m0, dst->sa_len, M_NOWAIT);

		/* if allocation failed drop packet */
		if (m0 == NULL) {
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (ENOBUFS);
		} else {
			bcopy(dst, m0->m_data, dst->sa_len);
		}
	}

	if (cached_tun_flags & TUN_IFHEAD) {
		/* Prepend the address family */
		M_PREPEND(m0, 4, M_NOWAIT);

		/* if allocation failed drop packet */
		if (m0 == NULL) {
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (ENOBUFS);
		} else
			*(u_int32_t *)m0->m_data = htonl(af);
	} else {
#ifdef INET
		if (af != AF_INET)
#endif
		{
			m_freem(m0);
			return (EAFNOSUPPORT);
		}
	}

	error = (ifp->if_transmit)(ifp, m0);
	if (error)
		return (ENOBUFS);
	if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
	return (0);
}

/*
 * the cdevsw interface is now pretty minimal.
 */
static	int
tunioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag,
    struct thread *td)
{
	struct ifreq ifr, *ifrp;
	struct tuntap_softc *tp = dev->si_drv1;
	struct ifnet *ifp = TUN2IFP(tp);
	struct tuninfo *tunp;
	int error, iflags, ival;
	bool	l2tun;

	l2tun = (tp->tun_flags & TUN_L2) != 0;
	if (l2tun) {
		/* tap specific ioctls */
		switch(cmd) {
		/* VMware/VMnet port ioctl's */
#if defined(COMPAT_FREEBSD6) || defined(COMPAT_FREEBSD5) || \
    defined(COMPAT_FREEBSD4)
		case _IO('V', 0):
			ival = IOCPARM_IVAL(data);
			data = (caddr_t)&ival;
			/* FALLTHROUGH */
#endif
		case VMIO_SIOCSIFFLAGS: /* VMware/VMnet SIOCSIFFLAGS */
			iflags = *(int *)data;
			iflags &= TUN_VMIO_FLAG_MASK;
			iflags &= ~IFF_CANTCHANGE;
			iflags |= IFF_UP;

			TUN_LOCK(tp);
			ifp->if_flags = iflags |
			    (ifp->if_flags & IFF_CANTCHANGE);
			TUN_UNLOCK(tp);

			return (0);
		case SIOCGIFADDR:	/* get MAC address of the remote side */
			TUN_LOCK(tp);
			bcopy(&tp->tun_ether.octet, data,
			    sizeof(tp->tun_ether.octet));
			TUN_UNLOCK(tp);

			return (0);
		case SIOCSIFADDR:	/* set MAC address of the remote side */
			TUN_LOCK(tp);
			bcopy(data, &tp->tun_ether.octet,
			    sizeof(tp->tun_ether.octet));
			TUN_UNLOCK(tp);

			return (0);
		case TAPSVNETHDR:
			ival = *(int *)data;
			if (ival != 0 &&
			    ival != sizeof(struct virtio_net_hdr) &&
			    ival != sizeof(struct virtio_net_hdr_mrg_rxbuf)) {
				return (EINVAL);
			}
			TUN_LOCK(tp);
			tun_vnethdr_set(ifp, ival);
			TUN_UNLOCK(tp);

			return (0);
		case TAPGVNETHDR:
			TUN_LOCK(tp);
			*(int *)data = tp->tun_vhdrlen;
			TUN_UNLOCK(tp);

			return (0);
		}

		/* Fall through to the common ioctls if unhandled */
	} else {
		switch (cmd) {
		case TUNSLMODE:
			TUN_LOCK(tp);
			if (*(int *)data) {
				tp->tun_flags |= TUN_LMODE;
				tp->tun_flags &= ~TUN_IFHEAD;
			} else
				tp->tun_flags &= ~TUN_LMODE;
			TUN_UNLOCK(tp);

			return (0);
		case TUNSIFHEAD:
			TUN_LOCK(tp);
			if (*(int *)data) {
				tp->tun_flags |= TUN_IFHEAD;
				tp->tun_flags &= ~TUN_LMODE;
			} else
				tp->tun_flags &= ~TUN_IFHEAD;
			TUN_UNLOCK(tp);

			return (0);
		case TUNGIFHEAD:
			TUN_LOCK(tp);
			*(int *)data = (tp->tun_flags & TUN_IFHEAD) ? 1 : 0;
			TUN_UNLOCK(tp);

			return (0);
		case TUNSIFMODE:
			/* deny this if UP */
			if (TUN2IFP(tp)->if_flags & IFF_UP)
				return (EBUSY);

			switch (*(int *)data & ~IFF_MULTICAST) {
			case IFF_POINTOPOINT:
			case IFF_BROADCAST:
				TUN_LOCK(tp);
				TUN2IFP(tp)->if_flags &=
				    ~(IFF_BROADCAST|IFF_POINTOPOINT|IFF_MULTICAST);
				TUN2IFP(tp)->if_flags |= *(int *)data;
				TUN_UNLOCK(tp);

				break;
			default:
				return (EINVAL);
			}

			return (0);
		case TUNSIFPID:
			TUN_LOCK(tp);
			tp->tun_pid = curthread->td_proc->p_pid;
			TUN_UNLOCK(tp);

			return (0);
		}
		/* Fall through to the common ioctls if unhandled */
	}

	switch (cmd) {
	case TUNGIFNAME:
		ifrp = (struct ifreq *)data;
		strlcpy(ifrp->ifr_name, TUN2IFP(tp)->if_xname, IFNAMSIZ);

		return (0);
	case TUNSIFINFO:
		tunp = (struct tuninfo *)data;
		if (TUN2IFP(tp)->if_type != tunp->type)
			return (EPROTOTYPE);
		TUN_LOCK(tp);
		if (TUN2IFP(tp)->if_mtu != tunp->mtu) {
			strlcpy(ifr.ifr_name, if_name(TUN2IFP(tp)), IFNAMSIZ);
			ifr.ifr_mtu = tunp->mtu;
			CURVNET_SET(TUN2IFP(tp)->if_vnet);
			error = ifhwioctl(SIOCSIFMTU, TUN2IFP(tp),
			    (caddr_t)&ifr, td);
			CURVNET_RESTORE();
			if (error) {
				TUN_UNLOCK(tp);
				return (error);
			}
		}
		TUN2IFP(tp)->if_baudrate = tunp->baudrate;
		TUN_UNLOCK(tp);
		break;
	case TUNGIFINFO:
		tunp = (struct tuninfo *)data;
		TUN_LOCK(tp);
		tunp->mtu = TUN2IFP(tp)->if_mtu;
		tunp->type = TUN2IFP(tp)->if_type;
		tunp->baudrate = TUN2IFP(tp)->if_baudrate;
		TUN_UNLOCK(tp);
		break;
	case TUNSDEBUG:
		tundebug = *(int *)data;
		break;
	case TUNGDEBUG:
		*(int *)data = tundebug;
		break;
	case FIONBIO:
		break;
	case FIOASYNC:
		TUN_LOCK(tp);
		if (*(int *)data)
			tp->tun_flags |= TUN_ASYNC;
		else
			tp->tun_flags &= ~TUN_ASYNC;
		TUN_UNLOCK(tp);
		break;
	case FIONREAD:
		if (!IFQ_IS_EMPTY(&TUN2IFP(tp)->if_snd)) {
			struct mbuf *mb;
			IFQ_LOCK(&TUN2IFP(tp)->if_snd);
			IFQ_POLL_NOLOCK(&TUN2IFP(tp)->if_snd, mb);
			for (*(int *)data = 0; mb != NULL; mb = mb->m_next)
				*(int *)data += mb->m_len;
			IFQ_UNLOCK(&TUN2IFP(tp)->if_snd);
		} else
			*(int *)data = 0;
		break;
	case FIOSETOWN:
		return (fsetown(*(int *)data, &tp->tun_sigio));

	case FIOGETOWN:
		*(int *)data = fgetown(&tp->tun_sigio);
		return (0);

	/* This is deprecated, FIOSETOWN should be used instead. */
	case TIOCSPGRP:
		return (fsetown(-(*(int *)data), &tp->tun_sigio));

	/* This is deprecated, FIOGETOWN should be used instead. */
	case TIOCGPGRP:
		*(int *)data = -fgetown(&tp->tun_sigio);
		return (0);

	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * The cdevsw read interface - reads a packet at a time, or at
 * least as much of a packet as can be read.
 */
static	int
tunread(struct cdev *dev, struct uio *uio, int flag)
{
	struct tuntap_softc *tp = dev->si_drv1;
	struct ifnet	*ifp = TUN2IFP(tp);
	struct mbuf	*m;
	size_t		len;
	int		error = 0;

	TUNDEBUG (ifp, "read\n");
	TUN_LOCK(tp);
	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUN_UNLOCK(tp);
		TUNDEBUG (ifp, "not ready 0%o\n", tp->tun_flags);
		return (EHOSTDOWN);
	}

	tp->tun_flags &= ~TUN_RWAIT;

	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m != NULL)
			break;
		if (flag & O_NONBLOCK) {
			TUN_UNLOCK(tp);
			return (EWOULDBLOCK);
		}
		tp->tun_flags |= TUN_RWAIT;
		error = mtx_sleep(tp, &tp->tun_mtx, PCATCH | (PZERO + 1),
		    "tunread", 0);
		if (error != 0) {
			TUN_UNLOCK(tp);
			return (error);
		}
	}
	TUN_UNLOCK(tp);

	if ((tp->tun_flags & TUN_L2) != 0)
		BPF_MTAP(ifp, m);

	len = min(tp->tun_vhdrlen, uio->uio_resid);
	if (len > 0) {
		struct virtio_net_hdr_mrg_rxbuf vhdr;

		bzero(&vhdr, sizeof(vhdr));
		if (m->m_pkthdr.csum_flags & TAP_ALL_OFFLOAD) {
			m = virtio_net_tx_offload(ifp, m, false, &vhdr.hdr);
		}

		TUNDEBUG(ifp, "txvhdr: f %u, gt %u, hl %u, "
		    "gs %u, cs %u, co %u\n", vhdr.hdr.flags,
		    vhdr.hdr.gso_type, vhdr.hdr.hdr_len,
		    vhdr.hdr.gso_size, vhdr.hdr.csum_start,
		    vhdr.hdr.csum_offset);
		error = uiomove(&vhdr, len, uio);
	}

	while (m && uio->uio_resid > 0 && error == 0) {
		len = min(uio->uio_resid, m->m_len);
		if (len != 0)
			error = uiomove(mtod(m, void *), len, uio);
		m = m_free(m);
	}

	if (m) {
		TUNDEBUG(ifp, "Dropping mbuf\n");
		m_freem(m);
	}
	return (error);
}

static int
tunwrite_l2(struct tuntap_softc *tp, struct mbuf *m,
	    struct virtio_net_hdr_mrg_rxbuf *vhdr)
{
	struct epoch_tracker et;
	struct ether_header *eh;
	struct ifnet *ifp;

	ifp = TUN2IFP(tp);

	/*
	 * Only pass a unicast frame to ether_input(), if it would
	 * actually have been received by non-virtual hardware.
	 */
	if (m->m_len < sizeof(struct ether_header)) {
		m_freem(m);
		return (0);
	}

	eh = mtod(m, struct ether_header *);

	if (eh && (ifp->if_flags & IFF_PROMISC) == 0 &&
	    !ETHER_IS_MULTICAST(eh->ether_dhost) &&
	    bcmp(eh->ether_dhost, IF_LLADDR(ifp), ETHER_ADDR_LEN) != 0) {
		m_freem(m);
		return (0);
	}

	if (vhdr != NULL && virtio_net_rx_csum(m, &vhdr->hdr)) {
		m_freem(m);
		return (0);
	}

	/* Pass packet up to parent. */
	CURVNET_SET(ifp->if_vnet);
	NET_EPOCH_ENTER(et);
	(*ifp->if_input)(ifp, m);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	/* ibytes are counted in parent */
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	return (0);
}

static int
tunwrite_l3(struct tuntap_softc *tp, struct mbuf *m)
{
	struct epoch_tracker et;
	struct ifnet *ifp;
	int family, isr;

	ifp = TUN2IFP(tp);
	/* Could be unlocked read? */
	TUN_LOCK(tp);
	if (tp->tun_flags & TUN_IFHEAD) {
		TUN_UNLOCK(tp);
		if (m->m_len < sizeof(family) &&
		(m = m_pullup(m, sizeof(family))) == NULL)
			return (ENOBUFS);
		family = ntohl(*mtod(m, u_int32_t *));
		m_adj(m, sizeof(family));
	} else {
		TUN_UNLOCK(tp);
		family = AF_INET;
	}

	BPF_MTAP2(ifp, &family, sizeof(family), m);

	switch (family) {
#ifdef INET
	case AF_INET:
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		isr = NETISR_IPV6;
		break;
#endif
	default:
		m_freem(m);
		return (EAFNOSUPPORT);
	}
	random_harvest_queue(m, sizeof(*m), RANDOM_NET_TUN);
	if_inc_counter(ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	CURVNET_SET(ifp->if_vnet);
	M_SETFIB(m, ifp->if_fib);
	NET_EPOCH_ENTER(et);
	netisr_dispatch(isr, m);
	NET_EPOCH_EXIT(et);
	CURVNET_RESTORE();
	return (0);
}

/*
 * the cdevsw write interface - an atomic write is a packet - or else!
 */
static	int
tunwrite(struct cdev *dev, struct uio *uio, int flag)
{
	struct virtio_net_hdr_mrg_rxbuf vhdr;
	struct tuntap_softc *tp;
	struct ifnet	*ifp;
	struct mbuf	*m;
	uint32_t	mru;
	int		align, vhdrlen, error;
	bool		l2tun;

	tp = dev->si_drv1;
	ifp = TUN2IFP(tp);
	TUNDEBUG(ifp, "tunwrite\n");
	if ((ifp->if_flags & IFF_UP) != IFF_UP)
		/* ignore silently */
		return (0);

	if (uio->uio_resid == 0)
		return (0);

	l2tun = (tp->tun_flags & TUN_L2) != 0;
	mru = l2tun ? TAPMRU : TUNMRU;
	vhdrlen = tp->tun_vhdrlen;
	align = 0;
	if (l2tun) {
		align = ETHER_ALIGN;
		mru += vhdrlen;
	} else if ((tp->tun_flags & TUN_IFHEAD) != 0)
		mru += sizeof(uint32_t);	/* family */
	if (uio->uio_resid < 0 || uio->uio_resid > mru) {
		TUNDEBUG(ifp, "len=%zd!\n", uio->uio_resid);
		return (EIO);
	}

	if (vhdrlen > 0) {
		error = uiomove(&vhdr, vhdrlen, uio);
		if (error != 0)
			return (error);
		TUNDEBUG(ifp, "txvhdr: f %u, gt %u, hl %u, "
		    "gs %u, cs %u, co %u\n", vhdr.hdr.flags,
		    vhdr.hdr.gso_type, vhdr.hdr.hdr_len,
		    vhdr.hdr.gso_size, vhdr.hdr.csum_start,
		    vhdr.hdr.csum_offset);
	}

	if ((m = m_uiotombuf(uio, M_NOWAIT, 0, align, M_PKTHDR)) == NULL) {
		if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
		return (ENOBUFS);
	}

	m->m_pkthdr.rcvif = ifp;
#ifdef MAC
	mac_ifnet_create_mbuf(ifp, m);
#endif

	if (l2tun)
		return (tunwrite_l2(tp, m, vhdrlen > 0 ? &vhdr : NULL));

	return (tunwrite_l3(tp, m));
}

/*
 * tunpoll - the poll interface, this is only useful on reads
 * really. The write detect always returns true, write never blocks
 * anyway, it either accepts the packet or drops it.
 */
static	int
tunpoll(struct cdev *dev, int events, struct thread *td)
{
	struct tuntap_softc *tp = dev->si_drv1;
	struct ifnet	*ifp = TUN2IFP(tp);
	int		revents = 0;

	TUNDEBUG(ifp, "tunpoll\n");

	if (events & (POLLIN | POLLRDNORM)) {
		IFQ_LOCK(&ifp->if_snd);
		if (!IFQ_IS_EMPTY(&ifp->if_snd)) {
			TUNDEBUG(ifp, "tunpoll q=%d\n", ifp->if_snd.ifq_len);
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			TUNDEBUG(ifp, "tunpoll waiting\n");
			selrecord(td, &tp->tun_rsel);
		}
		IFQ_UNLOCK(&ifp->if_snd);
	}
	revents |= events & (POLLOUT | POLLWRNORM);

	return (revents);
}

/*
 * tunkqfilter - support for the kevent() system call.
 */
static int
tunkqfilter(struct cdev *dev, struct knote *kn)
{
	struct tuntap_softc	*tp = dev->si_drv1;
	struct ifnet	*ifp = TUN2IFP(tp);

	switch(kn->kn_filter) {
	case EVFILT_READ:
		TUNDEBUG(ifp, "%s kqfilter: EVFILT_READ, minor = %#x\n",
		    ifp->if_xname, dev2unit(dev));
		kn->kn_fop = &tun_read_filterops;
		break;

	case EVFILT_WRITE:
		TUNDEBUG(ifp, "%s kqfilter: EVFILT_WRITE, minor = %#x\n",
		    ifp->if_xname, dev2unit(dev));
		kn->kn_fop = &tun_write_filterops;
		break;

	default:
		TUNDEBUG(ifp, "%s kqfilter: invalid filter, minor = %#x\n",
		    ifp->if_xname, dev2unit(dev));
		return(EINVAL);
	}

	kn->kn_hook = tp;
	knlist_add(&tp->tun_rsel.si_note, kn, 0);

	return (0);
}

/*
 * Return true of there is data in the interface queue.
 */
static int
tunkqread(struct knote *kn, long hint)
{
	int			ret;
	struct tuntap_softc	*tp = kn->kn_hook;
	struct cdev		*dev = tp->tun_dev;
	struct ifnet	*ifp = TUN2IFP(tp);

	if ((kn->kn_data = ifp->if_snd.ifq_len) > 0) {
		TUNDEBUG(ifp,
		    "%s have data in the queue.  Len = %d, minor = %#x\n",
		    ifp->if_xname, ifp->if_snd.ifq_len, dev2unit(dev));
		ret = 1;
	} else {
		TUNDEBUG(ifp,
		    "%s waiting for data, minor = %#x\n", ifp->if_xname,
		    dev2unit(dev));
		ret = 0;
	}

	return (ret);
}

/*
 * Always can write, always return MTU in kn->data.
 */
static int
tunkqwrite(struct knote *kn, long hint)
{
	struct tuntap_softc	*tp = kn->kn_hook;
	struct ifnet	*ifp = TUN2IFP(tp);

	kn->kn_data = ifp->if_mtu;

	return (1);
}

static void
tunkqdetach(struct knote *kn)
{
	struct tuntap_softc	*tp = kn->kn_hook;

	knlist_remove(&tp->tun_rsel.si_note, kn, 0);
}
