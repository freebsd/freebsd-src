/*	$NetBSD: if_tun.c,v 1.14 1994/06/29 06:36:25 cgd Exp $	*/

/*
 * Copyright (c) 1988, Julian Onions <jpo@cs.nott.ac.uk>
 * Nottingham University 1987.
 *
 * This source may be freely distributed, however I would be interested
 * in any changes that are made.
 *
 * This driver takes packets off the IP i/f and hands them up to a
 * user process to have it's wicked way with. This driver has it's
 * roots in a similar driver written by Phil Cockcroft (formerly) at
 * UCL. This driver is based much more on read/write/select mode of
 * operation though.
 */

#include "tun.h"
#if NTUN > 0

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/buf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/file.h>
#include <sys/signalvar.h>
#ifdef __FreeBSD__
#include <sys/kernel.h>
#endif
#include <sys/conf.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <sys/time.h>
#include <net/bpf.h>
#endif

#include <net/if_tun.h>

#ifdef __FreeBSD__
static void tunattach __P((void *));
PSEUDO_SET(tunattach, if_tun);
#endif

#ifdef JREMOD
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#define CDEV_MAJOR 52
#endif /*JREMOD*/

#define TUNDEBUG	if (tundebug) printf
int	tundebug = 0;

struct tun_softc tunctl[NTUN];

int	tunoutput __P((struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *rt));
int	tunifioctl __P((struct ifnet *, int, caddr_t));

static struct cdevsw tuncdevsw = {
	tunopen,	tunclose,	tunread,	tunwrite,
	tunioctl,	nullstop,	noreset,	nodevtotty,
	tunselect,	nommap,		nostrategy
};
extern dev_t tuncdev;

static int tuninit __P((int));

static void
tunattach(dummy)
	void *dummy;
{
	register int i;
	struct ifnet *ifp;

	/*
	 * In case we are an LKM, set up device switch.
	 */
	cdevsw[major(tuncdev)] = tuncdevsw;

	for (i = 0; i < NTUN; i++) {
		tunctl[i].tun_flags = TUN_INITED;

		ifp = &tunctl[i].tun_if;
		ifp->if_unit = i;
		ifp->if_name = "tun";
		ifp->if_mtu = TUNMTU;
		ifp->if_ioctl = tunifioctl;
		ifp->if_output = tunoutput;
		ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
		ifp->if_snd.ifq_maxlen = ifqmaxlen;
		ifp->if_collisions = 0;
		ifp->if_ierrors = 0;
		ifp->if_oerrors = 0;
		ifp->if_ipackets = 0;
		ifp->if_opackets = 0;
		if_attach(ifp);
#if NBPFILTER > 0
		bpfattach(&tunctl[i].tun_bpf, ifp, DLT_NULL, sizeof(u_int));
#endif
	}
}

/*
 * tunnel open - must be superuser & the device must be
 * configured in
 */
int
tunopen(dev, flag, mode, p)
	dev_t	dev;
	int	flag, mode;
	struct proc *p;
{
	struct ifnet	*ifp;
	struct tun_softc *tp;
	register int	unit, error;

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);

	if ((unit = minor(dev)) >= NTUN)
		return (ENXIO);
	tp = &tunctl[unit];
	if (tp->tun_flags & TUN_OPEN)
		return ENXIO;
	ifp = &tp->tun_if;
	tp->tun_flags |= TUN_OPEN;
	TUNDEBUG("%s%d: open\n", ifp->if_name, ifp->if_unit);
	return (0);
}

/*
 * tunclose - close the device - mark i/f down & delete
 * routing info
 */
int
tunclose(dev_t dev, int foo, int bar, struct proc *p)
{
	register int	unit = minor(dev), s;
	struct tun_softc *tp = &tunctl[unit];
	struct ifnet	*ifp = &tp->tun_if;
	struct mbuf	*m;

	tp->tun_flags &= ~TUN_OPEN;

	/*
	 * junk all pending output
	 */
	do {
		s = splimp();
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);
		if (m)
			m_freem(m);
	} while (m);

	if (ifp->if_flags & IFF_UP) {
		s = splimp();
		if_down(ifp);
		if (ifp->if_flags & IFF_RUNNING) {
		    /* find internet addresses and delete routes */
		    register struct ifaddr *ifa;
		    for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr->sa_family == AF_INET) {
			    rtinit(ifa, (int)RTM_DELETE,
				   tp->tun_flags & TUN_DSTADDR ? RTF_HOST : 0);
			}
		    }
		}
		splx(s);
	}
	tp->tun_pgrp = 0;
	selwakeup(&tp->tun_rsel);

	TUNDEBUG ("%s%d: closed\n", ifp->if_name, ifp->if_unit);
	return (0);
}

static int
tuninit(unit)
	int	unit;
{
	struct tun_softc *tp = &tunctl[unit];
	struct ifnet	*ifp = &tp->tun_if;
	register struct ifaddr *ifa;

	TUNDEBUG("%s%d: tuninit\n", ifp->if_name, ifp->if_unit);

	ifp->if_flags |= IFF_UP | IFF_RUNNING;

	for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
		if (ifa->ifa_addr->sa_family == AF_INET) {
		    struct sockaddr_in *si;

		    si = (struct sockaddr_in *)ifa->ifa_addr;
		    if (si && si->sin_addr.s_addr)
			    tp->tun_flags |= TUN_IASET;

		    si = (struct sockaddr_in *)ifa->ifa_dstaddr;
		    if (si && si->sin_addr.s_addr)
			    tp->tun_flags |= TUN_DSTADDR;
		}

	return 0;
}

/*
 * Process an ioctl request.
 */
int
tunifioctl(ifp, cmd, data)
	struct ifnet *ifp;
	int	cmd;
	caddr_t	data;
{
	register struct ifreq *ifr = (struct ifreq *)data;
	int		error = 0, s;

	s = splimp();
	switch(cmd) {
	case SIOCSIFADDR:
		tuninit(ifp->if_unit);
		TUNDEBUG("%s%d: address set\n",
			 ifp->if_name, ifp->if_unit);
		break;
	case SIOCSIFDSTADDR:
		tuninit(ifp->if_unit);
		TUNDEBUG("%s%d: destination address set\n",
			 ifp->if_name, ifp->if_unit);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifr->ifr_addr.sa_family) {

#ifdef INET
		case AF_INET:
			break;
#endif

		default:
			error = EAFNOSUPPORT;
			break;
		}
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
int
tunoutput(ifp, m0, dst, rt)
	struct ifnet   *ifp;
	struct mbuf    *m0;
	struct sockaddr *dst;
	struct rtentry *rt;
{
	struct tun_softc *tp = &tunctl[ifp->if_unit];
	struct proc	*p;
	int		s;

	TUNDEBUG ("%s%d: tunoutput\n", ifp->if_name, ifp->if_unit);

	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG ("%s%d: not ready 0%o\n", ifp->if_name,
			  ifp->if_unit, tp->tun_flags);
		m_freem (m0);
		return EHOSTDOWN;
	}

#if NBPFILTER > 0
	/* BPF write needs to be handled specially */
	if (dst->sa_family == AF_UNSPEC) {
		dst->sa_family = *(mtod(m0, int *));
		m0->m_len -= sizeof(int);
		m0->m_pkthdr.len -= sizeof(int);
		m0->m_data += sizeof(int);
	}

	if (tp->tun_bpf) {
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a dummy header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer to it).
		 */
		struct mbuf m;
		u_int af = dst->sa_family;

		m.m_next = m0;
		m.m_len = 4;
		m.m_data = (char *)&af;

		bpf_mtap(tp->tun_bpf, &m);
	}
#endif

	switch(dst->sa_family) {
#ifdef INET
	case AF_INET:
		s = splimp();
		if (IF_QFULL(&ifp->if_snd)) {
			IF_DROP(&ifp->if_snd);
			m_freem(m0);
			splx(s);
			ifp->if_collisions++;
			return (ENOBUFS);
		}
		IF_ENQUEUE(&ifp->if_snd, m0);
		splx(s);
		ifp->if_opackets++;
		break;
#endif
	default:
		m_freem(m0);
		return EAFNOSUPPORT;
	}

	if (tp->tun_flags & TUN_RWAIT) {
		tp->tun_flags &= ~TUN_RWAIT;
		wakeup((caddr_t)tp);
	}
	if (tp->tun_flags & TUN_ASYNC && tp->tun_pgrp) {
		if (tp->tun_pgrp > 0)
			gsignal(tp->tun_pgrp, SIGIO);
		else if (p = pfind(-tp->tun_pgrp))
			psignal(p, SIGIO);
	}
	selwakeup(&tp->tun_rsel);
	return 0;
}

/*
 * the cdevsw interface is now pretty minimal.
 */
int
tunioctl(dev, cmd, data, flag, p)
	dev_t		dev;
	int		cmd;
	caddr_t		data;
	int		flag;
	struct proc	*p;
{
	int		unit = minor(dev), s;
	struct tun_softc *tp = &tunctl[unit];
 	struct tuninfo *tunp;

	switch (cmd) {
 	case TUNSIFINFO:
 	        tunp = (struct tuninfo *)data;
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
	case FIONBIO:
		if (*(int *)data)
			tp->tun_flags |= TUN_NBIO;
		else
			tp->tun_flags &= ~TUN_NBIO;
		break;
	case FIOASYNC:
		if (*(int *)data)
			tp->tun_flags |= TUN_ASYNC;
		else
			tp->tun_flags &= ~TUN_ASYNC;
		break;
	case FIONREAD:
		s = splimp();
		if (tp->tun_if.if_snd.ifq_head)
			*(int *)data = tp->tun_if.if_snd.ifq_head->m_len;
		else
			*(int *)data = 0;
		splx(s);
		break;
	case TIOCSPGRP:
		tp->tun_pgrp = *(int *)data;
		break;
	case TIOCGPGRP:
		*(int *)data = tp->tun_pgrp;
		break;
	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * The cdevsw read interface - reads a packet at a time, or at
 * least as much of a packet as can be read.
 */
int
tunread(dev_t dev, struct uio *uio, int flag)
{
	int		unit = minor(dev);
	struct tun_softc *tp = &tunctl[unit];
	struct ifnet	*ifp = &tp->tun_if;
	struct mbuf	*m, *m0;
	int		error=0, len, s;

	TUNDEBUG ("%s%d: read\n", ifp->if_name, ifp->if_unit);
	if ((tp->tun_flags & TUN_READY) != TUN_READY) {
		TUNDEBUG ("%s%d: not ready 0%o\n", ifp->if_name,
			  ifp->if_unit, tp->tun_flags);
		return EHOSTDOWN;
	}

	tp->tun_flags &= ~TUN_RWAIT;

	s = splimp();
	do {
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == 0) {
			if (tp->tun_flags & TUN_NBIO) {
				splx(s);
				return EWOULDBLOCK;
			}
			tp->tun_flags |= TUN_RWAIT;
			tsleep((caddr_t)tp, PZERO + 1, "tunread", 0);
		}
	} while (m0 == 0);
	splx(s);

	while (m0 && uio->uio_resid > 0 && error == 0) {
		len = min(uio->uio_resid, m0->m_len);
		if (len == 0)
			break;
		error = uiomove(mtod(m0, caddr_t), len, uio);
		MFREE(m0, m);
		m0 = m;
	}

	if (m0) {
		TUNDEBUG("Dropping mbuf\n");
		m_freem(m0);
	}
	return error;
}

/*
 * the cdevsw write interface - an atomic write is a packet - or else!
 */
int
tunwrite(dev_t dev, struct uio *uio, int flag)
{
	int		unit = minor (dev);
	struct ifnet	*ifp = &tunctl[unit].tun_if;
	struct mbuf	*top, **mp, *m;
	int		error=0, s, tlen, mlen;

	TUNDEBUG("%s%d: tunwrite\n", ifp->if_name, ifp->if_unit);

	if (uio->uio_resid < 0 || uio->uio_resid > TUNMTU) {
		TUNDEBUG("%s%d: len=%d!\n", ifp->if_name, ifp->if_unit,
		    uio->uio_resid);
		return EIO;
	}
	tlen = uio->uio_resid;

	/* get a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;
	mlen = MHLEN;

	top = 0;
	mp = &top;
	while (error == 0 && uio->uio_resid > 0) {
		m->m_len = min(mlen, uio->uio_resid);
		error = uiomove(mtod (m, caddr_t), m->m_len, uio);
		*mp = m;
		mp = &m->m_next;
		if (uio->uio_resid > 0) {
			MGET (m, M_DONTWAIT, MT_DATA);
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
		return error;
	}

	top->m_pkthdr.len = tlen;
	top->m_pkthdr.rcvif = ifp;

#if NBPFILTER > 0
	if (tunctl[unit].tun_bpf) {
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a dummy header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer to it).
		 */
		struct mbuf m;
		u_int af = AF_INET;

		m.m_next = top;
		m.m_len = 4;
		m.m_data = (char *)&af;

		bpf_mtap(tunctl[unit].tun_bpf, &m);
	}
#endif

	s = splimp();
	if (IF_QFULL (&ipintrq)) {
		IF_DROP(&ipintrq);
		splx(s);
		ifp->if_collisions++;
		m_freem(top);
		return ENOBUFS;
	}
	IF_ENQUEUE(&ipintrq, top);
	splx(s);
	ifp->if_ipackets++;
	schednetisr(NETISR_IP);
	return error;
}

/*
 * tunselect - the select interface, this is only useful on reads
 * really. The write detect always returns true, write never blocks
 * anyway, it either accepts the packet or drops it.
 */
int
tunselect(dev_t dev, int rw, struct proc *p)
{
	int		unit = minor(dev), s;
	struct tun_softc *tp = &tunctl[unit];
	struct ifnet	*ifp = &tp->tun_if;

	s = splimp();
	TUNDEBUG("%s%d: tunselect\n", ifp->if_name, ifp->if_unit);

	switch (rw) {
	case FREAD:
		if (ifp->if_snd.ifq_len > 0) {
			splx(s);
			TUNDEBUG("%s%d: tunselect q=%d\n", ifp->if_name,
			    ifp->if_unit, ifp->if_snd.ifq_len);
			return 1;
		}
		selrecord(p, &tp->tun_rsel);
		break;
	case FWRITE:
		splx(s);
		return 1;
	}
	splx(s);
	TUNDEBUG("%s%d: tunselect waiting\n", ifp->if_name, ifp->if_unit);
	return 0;
}

#ifdef JREMOD
struct cdevsw tun_cdevsw = 
	{ tunopen,      tunclose,       tunread,        tunwrite,       /*52*/
	  tunioctl,     nostop,         nullreset,      nodevtotty,/* tunnel */
	  tunselect,    nommap,         NULL };

static tun_devsw_installed = 0;

static void 	tun_drvinit(void *unused)
{
	dev_t dev;

	if( ! tun_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&tun_cdevsw,NULL);
		tun_devsw_installed = 1;
#ifdef DEVFS
		{
			int x;
/* default for a simple device with no probe routine (usually delete this) */
			x=devfs_add_devsw(
/*	path	name	devsw		minor	type   uid gid perm*/
	"/",	"tun",	major(dev),	0,	DV_CHR,	0,  0, 0600);
		}
    	}
#endif
}

SYSINIT(tundev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,tun_drvinit,NULL)

#endif /* JREMOD */

#endif  /* NTUN */
