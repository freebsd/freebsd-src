/*	$NetBSD: if_tun.c,v 1.14 1994/06/29 06:36:25 cgd Exp $	*/

/*
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
 *
 * $FreeBSD$
 */

#include "opt_inet.h"
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mac.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/ttycom.h>
#include <sys/poll.h>
#include <sys/signalvar.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <machine/bus.h>	/* XXX Shouldn't really be required ! */
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/intrq.h>
#ifdef INET
#include <netinet/in.h>
#endif
#include <net/bpf.h>
#include <net/if_tunvar.h>
#include <net/if_tun.h>

#define TUNDEBUG	if (tundebug) printf
#define	TUNNAME		"tun"
#define	TUN_MAXUNIT	0x7fff	/* ifp->if_unit is only 15 bits */

static MALLOC_DEFINE(M_TUN, TUNNAME, "Tunnel Interface");
static int tundebug = 0;
static struct tun_softc *tunhead = NULL;
static struct rman tununits;
static udev_t tunbasedev = NOUDEV;
SYSCTL_INT(_debug, OID_AUTO, if_tun_debug, CTLFLAG_RW, &tundebug, 0, "");

static void	tunclone(void *arg, char *name, int namelen, dev_t *dev);
static void	tuncreate(dev_t dev);
static int	tunifioctl(struct ifnet *, u_long, caddr_t);
static int	tuninit(struct ifnet *);
static int	tunmodevent(module_t, int, void *);
static int	tunoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *rt);
static void	tunstart(struct ifnet *);

static d_open_t		tunopen;
static d_close_t	tunclose;
static d_read_t		tunread;
static d_write_t	tunwrite;
static d_ioctl_t	tunioctl;
static d_poll_t		tunpoll;

#define CDEV_MAJOR 52
static struct cdevsw tun_cdevsw = {
	/* open */	tunopen,
	/* close */	tunclose,
	/* read */	tunread,
	/* write */	tunwrite,
	/* ioctl */	tunioctl,
	/* poll */	tunpoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	TUNNAME,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static void
tunclone(void *arg, char *name, int namelen, dev_t *dev)
{
	struct resource *r;
	int err;
	int u;

	if (*dev != NODEV)
		return;

	if (strcmp(name, TUNNAME) == 0) {
		r = rman_reserve_resource(&tununits, 0, TUN_MAXUNIT, 1,
		    RF_ALLOCATED | RF_ACTIVE, NULL);
		u = rman_get_start(r);
		err = rman_release_resource(r);
		KASSERT(err == 0, ("Unexpected failure releasing resource"));
		*dev = makedev(CDEV_MAJOR, unit2minor(u));
		if ((*dev)->si_flags & SI_NAMED)
			return;	/* Already make_dev()d */
	} else if (dev_stdclone(name, NULL, TUNNAME, &u) != 1)
		return;	/* Don't recognise the name */

	*dev = make_dev(&tun_cdevsw, unit2minor(u),
	    UID_ROOT, GID_WHEEL, 0600, "tun%d", u);

	/*
	 * All devices depend on tunbasedev so that we can simply
	 * destroy_dev() this device at module unload time to get
	 * rid of all our make_dev()d resources.
	 */
	if (tunbasedev == NOUDEV)
		tunbasedev = (*dev)->si_udev;
	else {
		(*dev)->si_flags |= SI_CHEAPCLONE;
		dev_depends(udev2dev(tunbasedev, 0), *dev);
	}
}

static int
tunmodevent(module_t mod, int type, void *data) 
{
	static eventhandler_tag tag;
	struct tun_softc *tp;
	dev_t dev;
	int err;

	switch (type) { 
	case MOD_LOAD: 
		tag = EVENTHANDLER_REGISTER(dev_clone, tunclone, 0, 1000);
		if (tag == NULL)
			return (ENOMEM);
		tununits.rm_type = RMAN_ARRAY;
		tununits.rm_descr = "open if_tun units";
		err = rman_init(&tununits);
		if (err != 0) {
			cdevsw_remove(&tun_cdevsw);
			EVENTHANDLER_DEREGISTER(dev_clone, tag);
			return (err);
		}
		err = rman_manage_region(&tununits, 0, TUN_MAXUNIT);
		if (err != 0) {
			printf("%s: tununits: rman_manage_region: Failed %d\n",
			    TUNNAME, err);
			rman_fini(&tununits);
			cdevsw_remove(&tun_cdevsw);
			EVENTHANDLER_DEREGISTER(dev_clone, tag);
			return (err);
		}
		break; 
	case MOD_UNLOAD: 
		err = rman_fini(&tununits);
		if (err != 0)
			return (err);
		EVENTHANDLER_DEREGISTER(dev_clone, tag);

		while (tunhead != NULL) {
			KASSERT((tunhead->tun_flags & TUN_OPEN) == 0,
			    ("tununits is out of sync - unit %d",
			    tunhead->tun_if.if_unit));
			tp = tunhead;
			dev = makedev(tun_cdevsw.d_maj,
			    unit2minor(tp->tun_if.if_unit));
			KASSERT(dev->si_drv1 == tp, ("Bad makedev result"));
			tunhead = tp->next;
			bpfdetach(&tp->tun_if);
			if_detach(&tp->tun_if);
			KASSERT(dev->si_flags & SI_NAMED, ("Missing make_dev"));
			free(tp, M_TUN);
		}

		/*
		 * Destroying tunbasedev results in all of our make_dev()s
		 * conveniently going away.
		 */
		if (tunbasedev != NOUDEV)
			destroy_dev(udev2dev(tunbasedev, 0));

		break;
	} 
	return 0; 
} 

static moduledata_t tun_mod = { 
	"if_tun", 
	tunmodevent, 
	0
}; 

DECLARE_MODULE(if_tun, tun_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);

static void
tunstart(struct ifnet *ifp)
{
	struct tun_softc *tp = ifp->if_softc;

	if (tp->tun_flags & TUN_RWAIT) {
		tp->tun_flags &= ~TUN_RWAIT;
		wakeup((caddr_t)tp);
	}
	if (tp->tun_flags & TUN_ASYNC && tp->tun_sigio)
		pgsigio(&tp->tun_sigio, SIGIO, 0);
	selwakeup(&tp->tun_rsel);
}

static void
tuncreate(dev_t dev)
{
	struct tun_softc *sc;
	struct ifnet *ifp;

	if (!(dev->si_flags & SI_NAMED))
		dev = make_dev(&tun_cdevsw, minor(dev),
		    UID_UUCP, GID_DIALER, 0600, "tun%d", dev2unit(dev));

	MALLOC(sc, struct tun_softc *, sizeof(*sc), M_TUN, M_ZERO);
	sc->tun_flags = TUN_INITED;
	sc->next = tunhead;
	tunhead = sc;

	ifp = &sc->tun_if;
	ifp->if_unit = dev2unit(dev);
	ifp->if_name = TUNNAME;
	ifp->if_mtu = TUNMTU;
	ifp->if_ioctl = tunifioctl;
	ifp->if_output = tunoutput;
	ifp->if_start = tunstart;
	ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
	ifp->if_type = IFT_PPP;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_softc = sc;
	if_attach(ifp);
	bpfattach(ifp, DLT_NULL, sizeof(u_int));
	dev->si_drv1 = sc;
}

static int
tunopen(dev_t dev, int flag, int mode, struct thread *td)
{
	struct resource *r;
	struct ifnet	*ifp;
	struct tun_softc *tp;
	int unit;

	unit = dev2unit(dev);
	if (unit > TUN_MAXUNIT)
		return (ENXIO);

	r = rman_reserve_resource(&tununits, unit, unit, 1,
	    RF_ALLOCATED | RF_ACTIVE, NULL);
	if (r == NULL)
		return (EBUSY);

	dev->si_flags &= ~SI_CHEAPCLONE;

	tp = dev->si_drv1;
	if (!tp) {
		tuncreate(dev);
		tp = dev->si_drv1;
	}
	KASSERT(!(tp->tun_flags & TUN_OPEN), ("Resource & flags out-of-sync"));
	tp->r_unit = r;
	tp->tun_pid = td->td_proc->p_pid;
	ifp = &tp->tun_if;
	tp->tun_flags |= TUN_OPEN;
	TUNDEBUG("%s%d: open\n", ifp->if_name, ifp->if_unit);

	return (0);
}

/*
 * tunclose - close the device - mark i/f down & delete
 * routing info
 */
static	int
tunclose(dev_t dev, int foo, int bar, struct thread *td)
{
	struct tun_softc *tp;
	struct ifnet *ifp;
	int s;
	int err;

	tp = dev->si_drv1;
	ifp = &tp->tun_if;

	KASSERT(tp->r_unit, ("Unit %d not marked open", ifp->if_unit));
	tp->tun_flags &= ~TUN_OPEN;
	tp->tun_pid = 0;

	/*
	 * junk all pending output
	 */
	IF_DRAIN(&ifp->if_snd);

	if (ifp->if_flags & IFF_UP) {
		s = splimp();
		if_down(ifp);
		splx(s);
	}

	if (ifp->if_flags & IFF_RUNNING) {
		register struct ifaddr *ifa;

		s = splimp();
		/* find internet addresses and delete routes */
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link)
			if (ifa->ifa_addr->sa_family == AF_INET)
				rtinit(ifa, (int)RTM_DELETE,
				    tp->tun_flags & TUN_DSTADDR ? RTF_HOST : 0);
		ifp->if_flags &= ~IFF_RUNNING;
		splx(s);
	}

	funsetown(&tp->tun_sigio);
	selwakeup(&tp->tun_rsel);

	TUNDEBUG ("%s%d: closed\n", ifp->if_name, ifp->if_unit);
	err = rman_release_resource(tp->r_unit);
	KASSERT(err == 0, ("Unit %d failed to release", ifp->if_unit));

	return (0);
}

static int
tuninit(struct ifnet *ifp)
{
	struct tun_softc *tp = ifp->if_softc;
	register struct ifaddr *ifa;
	int error = 0;

	TUNDEBUG("%s%d: tuninit\n", ifp->if_name, ifp->if_unit);

	ifp->if_flags |= IFF_UP | IFF_RUNNING;
	getmicrotime(&ifp->if_lastchange);

	for (ifa = TAILQ_FIRST(&ifp->if_addrhead); ifa; 
	     ifa = TAILQ_NEXT(ifa, ifa_link)) {
		if (ifa->ifa_addr == NULL)
			error = EFAULT;
			/* XXX: Should maybe return straight off? */
		else {
#ifdef INET
			if (ifa->ifa_addr->sa_family == AF_INET) {
			    struct sockaddr_in *si;

			    si = (struct sockaddr_in *)ifa->ifa_addr;
			    if (si->sin_addr.s_addr)
				    tp->tun_flags |= TUN_IASET;

			    si = (struct sockaddr_in *)ifa->ifa_dstaddr;
			    if (si && si->sin_addr.s_addr)
				    tp->tun_flags |= TUN_DSTADDR;
			}
#endif
		}
	}
	return (error);
}

/*
 * Process an ioctl request.
 */
static int
tunifioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct tun_softc *tp = ifp->if_softc;
	struct ifstat *ifs;
	int		error = 0, s;

	s = splimp();
	switch(cmd) {
	case SIOCGIFSTATUS:
		ifs = (struct ifstat *)data;
		if (tp->tun_pid)
			sprintf(ifs->ascii + strlen(ifs->ascii),
			    "\tOpened by PID %d\n", tp->tun_pid);
		break;
	case SIOCSIFADDR:
		error = tuninit(ifp);
		TUNDEBUG("%s%d: address set, error=%d\n",
			 ifp->if_name, ifp->if_unit, error);
		break;
	case SIOCSIFDSTADDR:
		error = tuninit(ifp);
		TUNDEBUG("%s%d: destination address set, error=%d\n",
			 ifp->if_name, ifp->if_unit, error);
		break;
	case SIOCSIFMTU:
		ifp->if_mtu = ifr->ifr_mtu;
		TUNDEBUG("%s%d: mtu set\n", ifp->if_name, ifp->if_unit);
		break;
	case SIOCSIFFLAGS:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;
	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}

/*
 * tunoutput - queue packets from higher level ready to put out.
 */
static int
tunoutput(
	struct ifnet *ifp,
	struct mbuf *m0,
	struct sockaddr *dst,
	struct rtentry *rt)
{
	struct tun_softc *tp = ifp->if_softc;
#ifdef MAC
	int error;
#endif

	TUNDEBUG ("%s%d: tunoutput\n", ifp->if_name, ifp->if_unit);

#ifdef MAC
	error = mac_check_ifnet_transmit(ifp, m0);
	if (error) {
		m_freem(m0);
		return (error);
	}
#endif

	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG ("%s%d: not ready 0%o\n", ifp->if_name,
			  ifp->if_unit, tp->tun_flags);
		m_freem (m0);
		return (EHOSTDOWN);
	}

	if ((ifp->if_flags & IFF_UP) != IFF_UP) {
		m_freem (m0);
		return (EHOSTDOWN);
	}

	/* BPF write needs to be handled specially */
	if (dst->sa_family == AF_UNSPEC) {
		dst->sa_family = *(mtod(m0, int *));
		m0->m_len -= sizeof(int);
		m0->m_pkthdr.len -= sizeof(int);
		m0->m_data += sizeof(int);
	}

	if (ifp->if_bpf) {
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a dummy header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer to it).
		 */
		struct mbuf m;
		uint32_t af = dst->sa_family;

		m.m_next = m0;
		m.m_len = 4;
		m.m_data = (char *)&af;

		BPF_MTAP(ifp, &m);
	}

	/* prepend sockaddr? this may abort if the mbuf allocation fails */
	if (tp->tun_flags & TUN_LMODE) {
		/* allocate space for sockaddr */
		M_PREPEND(m0, dst->sa_len, M_NOWAIT);

		/* if allocation failed drop packet */
		if (m0 == NULL) {
			ifp->if_iqdrops++;
			ifp->if_oerrors++;
			return (ENOBUFS);
		} else {
			bcopy(dst, m0->m_data, dst->sa_len);
		}
	}

	if (tp->tun_flags & TUN_IFHEAD) {
		/* Prepend the address family */
		M_PREPEND(m0, 4, M_NOWAIT);

		/* if allocation failed drop packet */
		if (m0 == NULL) {
			ifp->if_iqdrops++;
			ifp->if_oerrors++;
			return (ENOBUFS);
		} else
			*(u_int32_t *)m0->m_data = htonl(dst->sa_family);
	} else {
#ifdef INET
		if (dst->sa_family != AF_INET)
#endif
		{
			m_freem(m0);
			return (EAFNOSUPPORT);
		}
	}

	if (! IF_HANDOFF(&ifp->if_snd, m0, ifp)) {
		ifp->if_collisions++;
		return (ENOBUFS);
	}
	ifp->if_opackets++;
	return (0);
}

/*
 * the cdevsw interface is now pretty minimal.
 */
static	int
tunioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	int		s;
	int		error;
	struct tun_softc *tp = dev->si_drv1;
 	struct tuninfo *tunp;

	switch (cmd) {
 	case TUNSIFINFO:
 		tunp = (struct tuninfo *)data;
		if (tunp->mtu < IF_MINMTU)
			return (EINVAL);
 		if (tp->tun_if.if_mtu != tunp->mtu
		&& (error = suser(td)) != 0)
			return (error);
 		tp->tun_if.if_mtu = tunp->mtu;
 		tp->tun_if.if_type = tunp->type;
 		tp->tun_if.if_baudrate = tunp->baudrate;
 		break;
 	case TUNGIFINFO:
 		tunp = (struct tuninfo *)data;
 		tunp->mtu = tp->tun_if.if_mtu;
 		tunp->type = tp->tun_if.if_type;
 		tunp->baudrate = tp->tun_if.if_baudrate;
 		break;
	case TUNSDEBUG:
		tundebug = *(int *)data;
		break;
	case TUNGDEBUG:
		*(int *)data = tundebug;
		break;
	case TUNSLMODE:
		if (*(int *)data) {
			tp->tun_flags |= TUN_LMODE;
			tp->tun_flags &= ~TUN_IFHEAD;
		} else
			tp->tun_flags &= ~TUN_LMODE;
		break;
	case TUNSIFHEAD:
		if (*(int *)data) {
			tp->tun_flags |= TUN_IFHEAD;
			tp->tun_flags &= ~TUN_LMODE;
		} else 
			tp->tun_flags &= ~TUN_IFHEAD;
		break;
	case TUNGIFHEAD:
		*(int *)data = (tp->tun_flags & TUN_IFHEAD) ? 1 : 0;
		break;
	case TUNSIFMODE:
		/* deny this if UP */
		if (tp->tun_if.if_flags & IFF_UP)
			return(EBUSY);

		switch (*(int *)data & ~IFF_MULTICAST) {
		case IFF_POINTOPOINT:
		case IFF_BROADCAST:
			tp->tun_if.if_flags &=
			    ~(IFF_BROADCAST|IFF_POINTOPOINT|IFF_MULTICAST);
			tp->tun_if.if_flags |= *(int *)data;
			break;
		default:
			return(EINVAL);
		}
		break;
	case TUNSIFPID:
		tp->tun_pid = curthread->td_proc->p_pid;
		break;
	case FIONBIO:
		break;
	case FIOASYNC:
		if (*(int *)data)
			tp->tun_flags |= TUN_ASYNC;
		else
			tp->tun_flags &= ~TUN_ASYNC;
		break;
	case FIONREAD:
		s = splimp();
		if (tp->tun_if.if_snd.ifq_head) {
			struct mbuf *mb = tp->tun_if.if_snd.ifq_head;
			for( *(int *)data = 0; mb != 0; mb = mb->m_next) 
				*(int *)data += mb->m_len;
		} else
			*(int *)data = 0;
		splx(s);
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
tunread(dev_t dev, struct uio *uio, int flag)
{
	struct tun_softc *tp = dev->si_drv1;
	struct ifnet	*ifp = &tp->tun_if;
	struct mbuf	*m;
	int		error=0, len, s;

	TUNDEBUG ("%s%d: read\n", ifp->if_name, ifp->if_unit);
	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG ("%s%d: not ready 0%o\n", ifp->if_name,
			  ifp->if_unit, tp->tun_flags);
		return (EHOSTDOWN);
	}

	tp->tun_flags &= ~TUN_RWAIT;

	s = splimp();
	do {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL) {
			if (flag & IO_NDELAY) {
				splx(s);
				return (EWOULDBLOCK);
			}
			tp->tun_flags |= TUN_RWAIT;
			if((error = tsleep((caddr_t)tp, PCATCH | (PZERO + 1),
					"tunread", 0)) != 0) {
				splx(s);
				return (error);
			}
		}
	} while (m == NULL);
	splx(s);

	while (m && uio->uio_resid > 0 && error == 0) {
		len = min(uio->uio_resid, m->m_len);
		if (len != 0)
			error = uiomove(mtod(m, caddr_t), len, uio);
		m = m_free(m);
	}

	if (m) {
		TUNDEBUG("%s%d: Dropping mbuf\n", ifp->if_name, ifp->if_unit);
		m_freem(m);
	}
	return (error);
}

/*
 * the cdevsw write interface - an atomic write is a packet - or else!
 */
static	int
tunwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tun_softc *tp = dev->si_drv1;
	struct ifnet	*ifp = &tp->tun_if;
	struct mbuf	*top, **mp, *m;
	int		error=0, tlen, mlen;
	uint32_t	family;

	TUNDEBUG("%s%d: tunwrite\n", ifp->if_name, ifp->if_unit);

	if ((ifp->if_flags & IFF_UP) != IFF_UP)
		/* ignore silently */
		return (0);

	if (uio->uio_resid == 0)
		return (0);

	if (uio->uio_resid < 0 || uio->uio_resid > TUNMRU) {
		TUNDEBUG("%s%d: len=%d!\n", ifp->if_name, ifp->if_unit,
		    uio->uio_resid);
		return (EIO);
	}
	tlen = uio->uio_resid;

	/* get a header mbuf */
	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	mlen = MHLEN;

	top = 0;
	mp = &top;
	while (error == 0 && uio->uio_resid > 0) {
		m->m_len = min(mlen, uio->uio_resid);
		error = uiomove(mtod (m, caddr_t), m->m_len, uio);
		*mp = m;
		mp = &m->m_next;
		if (uio->uio_resid > 0) {
			MGET (m, M_NOWAIT, MT_DATA);
			if (m == 0) {
				error = ENOBUFS;
				break;
			}
			mlen = MLEN;
		}
	}
	if (error) {
		if (top)
			m_freem (top);
		ifp->if_ierrors++;
		return (error);
	}

	top->m_pkthdr.len = tlen;
	top->m_pkthdr.rcvif = ifp;
#ifdef MAC
	mac_create_mbuf_from_ifnet(ifp, top);
#endif

	if (ifp->if_bpf) {
		if (tp->tun_flags & TUN_IFHEAD) {
			/*
			 * Conveniently, we already have a 4-byte address
			 * family prepended to our packet !
			 * Inconveniently, it's in the wrong byte order !
			 */
			if ((top = m_pullup(top, sizeof(family))) == NULL)
				return (ENOBUFS);
			*mtod(top, u_int32_t *) =
			    ntohl(*mtod(top, u_int32_t *));
			BPF_MTAP(ifp, top);
			*mtod(top, u_int32_t *) =
			    htonl(*mtod(top, u_int32_t *));
		} else {
			/*
			 * We need to prepend the address family as
			 * a four byte field.  Cons up a dummy header
			 * to pacify bpf.  This is safe because bpf
			 * will only read from the mbuf (i.e., it won't
			 * try to free it or keep a pointer to it).
			 */
			struct mbuf m;
			uint32_t af = AF_INET;

			m.m_next = top;
			m.m_len = 4;
			m.m_data = (char *)&af;

			BPF_MTAP(ifp, &m);
		}
	}

	if (tp->tun_flags & TUN_IFHEAD) {
		if (top->m_len < sizeof(family) &&
		    (top = m_pullup(top, sizeof(family))) == NULL)
			return (ENOBUFS);
		family = ntohl(*mtod(top, u_int32_t *));
		m_adj(top, sizeof(family));
	} else
		family = AF_INET;

	ifp->if_ibytes += top->m_pkthdr.len;
	ifp->if_ipackets++;

	return (family_enqueue(family, top));
}

/*
 * tunpoll - the poll interface, this is only useful on reads
 * really. The write detect always returns true, write never blocks
 * anyway, it either accepts the packet or drops it.
 */
static	int
tunpoll(dev_t dev, int events, struct thread *td)
{
	int		s;
	struct tun_softc *tp = dev->si_drv1;
	struct ifnet	*ifp = &tp->tun_if;
	int		revents = 0;

	s = splimp();
	TUNDEBUG("%s%d: tunpoll\n", ifp->if_name, ifp->if_unit);

	if (events & (POLLIN | POLLRDNORM)) {
		if (ifp->if_snd.ifq_len > 0) {
			TUNDEBUG("%s%d: tunpoll q=%d\n", ifp->if_name,
			    ifp->if_unit, ifp->if_snd.ifq_len);
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			TUNDEBUG("%s%d: tunpoll waiting\n", ifp->if_name,
			    ifp->if_unit);
			selrecord(td, &tp->tun_rsel);
		}
	}
	if (events & (POLLOUT | POLLWRNORM))
		revents |= events & (POLLOUT | POLLWRNORM);

	splx(s);
	return (revents);
}
