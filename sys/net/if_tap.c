/*-
 * Copyright (C) 1999-2000 by Maksim Yevmenkin <m_evmenkin@yahoo.com>
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
 * BASED ON:
 * -------------------------------------------------------------------------
 *
 * Copyright (c) 1988, Julian Onions <jpo@cs.nott.ac.uk>
 * Nottingham University 1987.
 */

/*
 * $FreeBSD$
 * $Id: if_tap.c,v 0.21 2000/07/23 21:46:02 max Exp $
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/selinfo.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/ttycom.h>
#include <sys/uio.h>
#include <sys/queue.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/if_types.h>

#include <netinet/in.h>

#include <net/if_tapvar.h>
#include <net/if_tap.h>


#define CDEV_NAME	"tap"
#define TAPDEBUG	if (tapdebug) printf

#define TAP		"tap"
#define VMNET		"vmnet"
#define TAPMAXUNIT	0x7fff
#define VMNET_DEV_MASK	CLONE_FLAG0

/* module */
static int		tapmodevent(module_t, int, void *);

/* device */
static void		tapclone(void *, struct ucred *, char *, int,
			    struct cdev **);
static void		tapcreate(struct cdev *);

/* network interface */
static void		tapifstart(struct ifnet *);
static int		tapifioctl(struct ifnet *, u_long, caddr_t);
static void		tapifinit(void *);

/* character device */
static d_open_t		tapopen;
static d_close_t	tapclose;
static d_read_t		tapread;
static d_write_t	tapwrite;
static d_ioctl_t	tapioctl;
static d_poll_t		tappoll;

static struct cdevsw	tap_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_PSEUDO | D_NEEDGIANT,
	.d_open =	tapopen,
	.d_close =	tapclose,
	.d_read =	tapread,
	.d_write =	tapwrite,
	.d_ioctl =	tapioctl,
	.d_poll =	tappoll,
	.d_name =	CDEV_NAME,
};

/*
 * All global variables in if_tap.c are locked with tapmtx, with the
 * exception of tapdebug, which is accessed unlocked; tapclones is
 * static at runtime.
 */
static struct mtx		tapmtx;
static int			tapdebug = 0;        /* debug flag   */
static int			tapuopen = 0;        /* allow user open() */	     
static SLIST_HEAD(, tap_softc)	taphead;             /* first device */
static struct clonedevs 	*tapclones;

MALLOC_DECLARE(M_TAP);
MALLOC_DEFINE(M_TAP, CDEV_NAME, "Ethernet tunnel interface");
SYSCTL_INT(_debug, OID_AUTO, if_tap_debug, CTLFLAG_RW, &tapdebug, 0, "");

SYSCTL_DECL(_net_link);
SYSCTL_NODE(_net_link, OID_AUTO, tap, CTLFLAG_RW, 0,
    "Ethernet tunnel software network interface");
SYSCTL_INT(_net_link_tap, OID_AUTO, user_open, CTLFLAG_RW, &tapuopen, 0,
	"Allow user to open /dev/tap (based on node permissions)");
SYSCTL_INT(_net_link_tap, OID_AUTO, debug, CTLFLAG_RW, &tapdebug, 0, "");

DEV_MODULE(if_tap, tapmodevent, NULL);

/*
 * tapmodevent
 *
 * module event handler
 */
static int
tapmodevent(mod, type, data)
	module_t	 mod;
	int		 type;
	void		*data;
{
	static eventhandler_tag	 eh_tag = NULL;
	struct tap_softc	*tp = NULL;
	struct ifnet		*ifp = NULL;
	int			 s;

	switch (type) {
	case MOD_LOAD:

		/* intitialize device */

		mtx_init(&tapmtx, "tapmtx", NULL, MTX_DEF);
		SLIST_INIT(&taphead);

		clone_setup(&tapclones);
		eh_tag = EVENTHANDLER_REGISTER(dev_clone, tapclone, 0, 1000);
		if (eh_tag == NULL) {
			clone_cleanup(&tapclones);
			mtx_destroy(&tapmtx);
			return (ENOMEM);
		}
		return (0);

	case MOD_UNLOAD:
		/*
		 * The EBUSY algorithm here can't quite atomically
		 * guarantee that this is race-free since we have to
		 * release the tap mtx to deregister the clone handler.
		 */
		mtx_lock(&tapmtx);
		SLIST_FOREACH(tp, &taphead, tap_next) {
			mtx_lock(&tp->tap_mtx);
			if (tp->tap_flags & TAP_OPEN) {
				mtx_unlock(&tp->tap_mtx);
				mtx_unlock(&tapmtx);
				return (EBUSY);
			}
			mtx_unlock(&tp->tap_mtx);
		}
		mtx_unlock(&tapmtx);

		EVENTHANDLER_DEREGISTER(dev_clone, eh_tag);

		mtx_lock(&tapmtx);
		while ((tp = SLIST_FIRST(&taphead)) != NULL) {
			SLIST_REMOVE_HEAD(&taphead, tap_next);
			mtx_unlock(&tapmtx);

			ifp = tp->tap_ifp;

			TAPDEBUG("detaching %s\n", ifp->if_xname);

			/* Unlocked read. */
			KASSERT(!(tp->tap_flags & TAP_OPEN), 
				("%s flags is out of sync", ifp->if_xname));

			destroy_dev(tp->tap_dev);
			s = splimp();
			ether_ifdetach(ifp);
			if_free_type(ifp, IFT_ETHER);
			splx(s);

			mtx_destroy(&tp->tap_mtx);
			free(tp, M_TAP);
			mtx_lock(&tapmtx);
		}
		mtx_unlock(&tapmtx);
		clone_cleanup(&tapclones);

		mtx_destroy(&tapmtx);

		break;

	default:
		return (EOPNOTSUPP);
	}

	return (0);
} /* tapmodevent */


/*
 * DEVFS handler
 *
 * We need to support two kind of devices - tap and vmnet
 */
static void
tapclone(arg, cred, name, namelen, dev)
	void	*arg;
	struct ucred *cred;
	char	*name;
	int	 namelen;
	struct cdev **dev;
{
	u_int		extra;
	int		i, unit;
	char		*device_name = name;

	if (*dev != NULL)
		return;

	device_name = TAP;
	extra = 0;
	if (strcmp(name, TAP) == 0) {
		unit = -1;
	} else if (strcmp(name, VMNET) == 0) {
		device_name = VMNET;
		extra = VMNET_DEV_MASK;
		unit = -1;
	} else if (dev_stdclone(name, NULL, device_name, &unit) != 1) {
		device_name = VMNET;
		extra = VMNET_DEV_MASK;
		if (dev_stdclone(name, NULL, device_name, &unit) != 1)
			return;
	}

	/* find any existing device, or allocate new unit number */
	i = clone_create(&tapclones, &tap_cdevsw, &unit, dev, extra);
	if (i) {
		*dev = make_dev(&tap_cdevsw, unit2minor(unit | extra),
		     UID_ROOT, GID_WHEEL, 0600, "%s%d", device_name, unit);
		if (*dev != NULL) {
			dev_ref(*dev);
			(*dev)->si_flags |= SI_CHEAPCLONE;
		}
	}
} /* tapclone */


/*
 * tapcreate
 *
 * to create interface
 */
static void
tapcreate(dev)
	struct cdev *dev;
{
	struct ifnet		*ifp = NULL;
	struct tap_softc	*tp = NULL;
	unsigned short		 macaddr_hi;
	int			 unit, s;
	char			*name = NULL;
	u_char			eaddr[6];

	dev->si_flags &= ~SI_CHEAPCLONE;

	/* allocate driver storage and create device */
	MALLOC(tp, struct tap_softc *, sizeof(*tp), M_TAP, M_WAITOK | M_ZERO);
	mtx_init(&tp->tap_mtx, "tap_mtx", NULL, MTX_DEF);
	mtx_lock(&tapmtx);
	SLIST_INSERT_HEAD(&taphead, tp, tap_next);
	mtx_unlock(&tapmtx);

	unit = dev2unit(dev);

	/* select device: tap or vmnet */
	if (unit & VMNET_DEV_MASK) {
		name = VMNET;
		tp->tap_flags |= TAP_VMNET;
	} else
		name = TAP;

	unit &= TAPMAXUNIT;

	TAPDEBUG("tapcreate(%s%d). minor = %#x\n", name, unit, minor(dev));

	/* generate fake MAC address: 00 bd xx xx xx unit_no */
	macaddr_hi = htons(0x00bd);
	bcopy(&macaddr_hi, eaddr, sizeof(short));
	bcopy(&ticks, &eaddr[2], sizeof(long));
	eaddr[5] = (u_char)unit;

	/* fill the rest and attach interface */
	ifp = tp->tap_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL)
		panic("%s%d: can not if_alloc()", name, unit);
	ifp->if_softc = tp;
	if_initname(ifp, name, unit);
	ifp->if_init = tapifinit;
	ifp->if_start = tapifstart;
	ifp->if_ioctl = tapifioctl;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = (IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST);
	ifp->if_snd.ifq_maxlen = ifqmaxlen;

	dev->si_drv1 = tp;
	tp->tap_dev = dev;

	s = splimp();
	ether_ifattach(ifp, eaddr);
	splx(s);

	mtx_lock(&tp->tap_mtx);
	tp->tap_flags |= TAP_INITED;
	mtx_unlock(&tp->tap_mtx);

	TAPDEBUG("interface %s is created. minor = %#x\n", 
		ifp->if_xname, minor(dev));
} /* tapcreate */


/*
 * tapopen
 *
 * to open tunnel. must be superuser
 */
static int
tapopen(dev, flag, mode, td)
	struct cdev *dev;
	int		 flag;
	int		 mode;
	struct thread	*td;
{
	struct tap_softc	*tp = NULL;
	struct ifnet		*ifp = NULL;
	int			 s;

	if (tapuopen == 0 && suser(td) != 0)
		return (EPERM);

	if ((dev2unit(dev) & CLONE_UNITMASK) > TAPMAXUNIT)
		return (ENXIO);

	/*
	 * XXXRW: Non-atomic test-and-set of si_drv1.  Currently protected
	 * by Giant, but the race actually exists under memory pressure as
	 * well even when running with Giant, as malloc() may sleep.
	 */
	tp = dev->si_drv1;
	if (tp == NULL) {
		tapcreate(dev);
		tp = dev->si_drv1;
	}

	mtx_lock(&tp->tap_mtx);
	if (tp->tap_flags & TAP_OPEN) {
		mtx_unlock(&tp->tap_mtx);
		return (EBUSY);
	}

	bcopy(IF_LLADDR(tp->tap_ifp), tp->ether_addr, sizeof(tp->ether_addr));
	tp->tap_pid = td->td_proc->p_pid;
	tp->tap_flags |= TAP_OPEN;
	ifp = tp->tap_ifp;
	mtx_unlock(&tp->tap_mtx);

	s = splimp();
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	splx(s);

	TAPDEBUG("%s is open. minor = %#x\n", ifp->if_xname, minor(dev));

	return (0);
} /* tapopen */


/*
 * tapclose
 *
 * close the device - mark i/f down & delete routing info
 */
static int
tapclose(dev, foo, bar, td)
	struct cdev *dev;
	int		 foo;
	int		 bar;
	struct thread	*td;
{
	struct ifaddr *ifa;
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = tp->tap_ifp;
	int			s;

	/* junk all pending output */
	IF_DRAIN(&ifp->if_snd);

	/*
	 * do not bring the interface down, and do not anything with
	 * interface, if we are in VMnet mode. just close the device.
	 */

	mtx_lock(&tp->tap_mtx);
	if (((tp->tap_flags & TAP_VMNET) == 0) && (ifp->if_flags & IFF_UP)) {
		mtx_unlock(&tp->tap_mtx);
		s = splimp();
		if_down(ifp);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				rtinit(ifa, (int)RTM_DELETE, 0);
			}
			if_purgeaddrs(ifp);
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		}
		splx(s);
	} else
		mtx_unlock(&tp->tap_mtx);

	funsetown(&tp->tap_sigio);
	selwakeuppri(&tp->tap_rsel, PZERO+1);

	mtx_lock(&tp->tap_mtx);
	tp->tap_flags &= ~TAP_OPEN;
	tp->tap_pid = 0;
	mtx_unlock(&tp->tap_mtx);

	TAPDEBUG("%s is closed. minor = %#x\n", 
		ifp->if_xname, minor(dev));

	return (0);
} /* tapclose */


/*
 * tapifinit
 *
 * network interface initialization function
 */
static void
tapifinit(xtp)
	void	*xtp;
{
	struct tap_softc	*tp = (struct tap_softc *)xtp;
	struct ifnet		*ifp = tp->tap_ifp;

	TAPDEBUG("initializing %s\n", ifp->if_xname);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	/* attempt to start output */
	tapifstart(ifp);
} /* tapifinit */


/*
 * tapifioctl
 *
 * Process an ioctl request on network interface
 */
static int
tapifioctl(ifp, cmd, data)
	struct ifnet	*ifp;
	u_long		 cmd;
	caddr_t		 data;
{
	struct tap_softc	*tp = (struct tap_softc *)(ifp->if_softc);
	struct ifstat		*ifs = NULL;
	int			 s, dummy;

	switch (cmd) {
		case SIOCSIFFLAGS: /* XXX -- just like vmnet does */
		case SIOCADDMULTI:
		case SIOCDELMULTI:
			break;

		case SIOCGIFSTATUS:
			s = splimp();
			ifs = (struct ifstat *)data;
			dummy = strlen(ifs->ascii);
			mtx_lock(&tp->tap_mtx);
			if (tp->tap_pid != 0 && dummy < sizeof(ifs->ascii))
				snprintf(ifs->ascii + dummy,
					sizeof(ifs->ascii) - dummy,
					"\tOpened by PID %d\n", tp->tap_pid);
			mtx_unlock(&tp->tap_mtx);
			splx(s);
			break;

		default:
			s = splimp();
			dummy = ether_ioctl(ifp, cmd, data);
			splx(s);
			return (dummy);
	}

	return (0);
} /* tapifioctl */


/*
 * tapifstart
 *
 * queue packets from higher level ready to put out
 */
static void
tapifstart(ifp)
	struct ifnet	*ifp;
{
	struct tap_softc	*tp = ifp->if_softc;
	int			 s;

	TAPDEBUG("%s starting\n", ifp->if_xname);

	/*
	 * do not junk pending output if we are in VMnet mode.
	 * XXX: can this do any harm because of queue overflow?
	 */

	mtx_lock(&tp->tap_mtx);
	if (((tp->tap_flags & TAP_VMNET) == 0) &&
	    ((tp->tap_flags & TAP_READY) != TAP_READY)) {
		struct mbuf	*m = NULL;

		mtx_unlock(&tp->tap_mtx);

		/* Unlocked read. */
		TAPDEBUG("%s not ready, tap_flags = 0x%x\n", ifp->if_xname, 
		    tp->tap_flags);

		s = splimp();
		do {
			IF_DEQUEUE(&ifp->if_snd, m);
			if (m != NULL)
				m_freem(m);
			ifp->if_oerrors ++;
		} while (m != NULL);
		splx(s);

		return;
	}
	mtx_unlock(&tp->tap_mtx);

	s = splimp();
	ifp->if_drv_flags |= IFF_DRV_OACTIVE;

	if (ifp->if_snd.ifq_len != 0) {
		mtx_lock(&tp->tap_mtx);
		if (tp->tap_flags & TAP_RWAIT) {
			tp->tap_flags &= ~TAP_RWAIT;
			wakeup(tp);
		}

		if ((tp->tap_flags & TAP_ASYNC) && (tp->tap_sigio != NULL)) {
			mtx_unlock(&tp->tap_mtx);
			pgsigio(&tp->tap_sigio, SIGIO, 0);
		} else
			mtx_unlock(&tp->tap_mtx);

		selwakeuppri(&tp->tap_rsel, PZERO+1);
		ifp->if_opackets ++; /* obytes are counted in ether_output */
	}

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	splx(s);
} /* tapifstart */


/*
 * tapioctl
 *
 * the cdevsw interface is now pretty minimal
 */
static int
tapioctl(dev, cmd, data, flag, td)
	struct cdev *dev;
	u_long		 cmd;
	caddr_t		 data;
	int		 flag;
	struct thread	*td;
{
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = tp->tap_ifp;
	struct tapinfo		*tapp = NULL;
	int			 s;
	int			 f;

	switch (cmd) {
		case TAPSIFINFO:
			s = splimp();
			tapp = (struct tapinfo *)data;
			ifp->if_mtu = tapp->mtu;
			ifp->if_type = tapp->type;
			ifp->if_baudrate = tapp->baudrate;
			splx(s);
			break;

		case TAPGIFINFO:
			tapp = (struct tapinfo *)data;
			tapp->mtu = ifp->if_mtu;
			tapp->type = ifp->if_type;
			tapp->baudrate = ifp->if_baudrate;
			break;

		case TAPSDEBUG:
			tapdebug = *(int *)data;
			break;

		case TAPGDEBUG:
			*(int *)data = tapdebug;
			break;

		case FIONBIO:
			break;

		case FIOASYNC:
			s = splimp();
			mtx_lock(&tp->tap_mtx);
			if (*(int *)data)
				tp->tap_flags |= TAP_ASYNC;
			else
				tp->tap_flags &= ~TAP_ASYNC;
			mtx_unlock(&tp->tap_mtx);
			splx(s);
			break;

		case FIONREAD:
			s = splimp();
			if (ifp->if_snd.ifq_head) {
				struct mbuf	*mb = ifp->if_snd.ifq_head;

				for(*(int *)data = 0;mb != NULL;mb = mb->m_next)
					*(int *)data += mb->m_len;
			} else
				*(int *)data = 0;
			splx(s);
			break;

		case FIOSETOWN:
			return (fsetown(*(int *)data, &tp->tap_sigio));

		case FIOGETOWN:
			*(int *)data = fgetown(&tp->tap_sigio);
			return (0);

		/* this is deprecated, FIOSETOWN should be used instead */
		case TIOCSPGRP:
			return (fsetown(-(*(int *)data), &tp->tap_sigio));

		/* this is deprecated, FIOGETOWN should be used instead */
		case TIOCGPGRP:
			*(int *)data = -fgetown(&tp->tap_sigio);
			return (0);

		/* VMware/VMnet port ioctl's */

		case SIOCGIFFLAGS:	/* get ifnet flags */
			bcopy(&ifp->if_flags, data, sizeof(ifp->if_flags));
			break;

		case VMIO_SIOCSIFFLAGS: /* VMware/VMnet SIOCSIFFLAGS */
			f = *(int *)data;
			f &= 0x0fff;
			f &= ~IFF_CANTCHANGE;
			f |= IFF_UP;

			s = splimp();
			ifp->if_flags = f | (ifp->if_flags & IFF_CANTCHANGE);
			splx(s);
			break;

		case OSIOCGIFADDR:	/* get MAC address of the remote side */
		case SIOCGIFADDR:
			mtx_lock(&tp->tap_mtx);
			bcopy(tp->ether_addr, data, sizeof(tp->ether_addr));
			mtx_unlock(&tp->tap_mtx);
			break;

		case SIOCSIFADDR:	/* set MAC address of the remote side */
			mtx_lock(&tp->tap_mtx);
			bcopy(data, tp->ether_addr, sizeof(tp->ether_addr));
			mtx_unlock(&tp->tap_mtx);
			break;

		default:
			return (ENOTTY);
	}
	return (0);
} /* tapioctl */


/*
 * tapread
 *
 * the cdevsw read interface - reads a packet at a time, or at
 * least as much of a packet as can be read
 */
static int
tapread(dev, uio, flag)
	struct cdev *dev;
	struct uio	*uio;
	int		 flag;
{
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = tp->tap_ifp;
	struct mbuf		*m = NULL;
	int			 error = 0, len, s;

	TAPDEBUG("%s reading, minor = %#x\n", ifp->if_xname, minor(dev));

	mtx_lock(&tp->tap_mtx);
	if ((tp->tap_flags & TAP_READY) != TAP_READY) {
		mtx_unlock(&tp->tap_mtx);

		/* Unlocked read. */
		TAPDEBUG("%s not ready. minor = %#x, tap_flags = 0x%x\n",
			ifp->if_xname, minor(dev), tp->tap_flags);

		return (EHOSTDOWN);
	}

	tp->tap_flags &= ~TAP_RWAIT;
	mtx_unlock(&tp->tap_mtx);

	/* sleep until we get a packet */
	do {
		s = splimp();
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL) {
			if (flag & O_NONBLOCK)
				return (EWOULDBLOCK);

			mtx_lock(&tp->tap_mtx);
			tp->tap_flags |= TAP_RWAIT;
			mtx_unlock(&tp->tap_mtx);
			error = tsleep(tp,PCATCH|(PZERO+1),"taprd",0);
			if (error)
				return (error);
		}
	} while (m == NULL);

	/* feed packet to bpf */
	BPF_MTAP(ifp, m);

	/* xfer packet to user space */
	while ((m != NULL) && (uio->uio_resid > 0) && (error == 0)) {
		len = min(uio->uio_resid, m->m_len);
		if (len == 0)
			break;

		error = uiomove(mtod(m, void *), len, uio);
		m = m_free(m);
	}

	if (m != NULL) {
		TAPDEBUG("%s dropping mbuf, minor = %#x\n", ifp->if_xname, 
			minor(dev));
		m_freem(m);
	}

	return (error);
} /* tapread */


/*
 * tapwrite
 *
 * the cdevsw write interface - an atomic write is a packet - or else!
 */
static int
tapwrite(dev, uio, flag)
	struct cdev *dev;
	struct uio	*uio;
	int		 flag;
{
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = tp->tap_ifp;
	struct mbuf		*m;
	int			 error = 0;

	TAPDEBUG("%s writting, minor = %#x\n", 
		ifp->if_xname, minor(dev));

	if (uio->uio_resid == 0)
		return (0);

	if ((uio->uio_resid < 0) || (uio->uio_resid > TAPMRU)) {
		TAPDEBUG("%s invalid packet len = %d, minor = %#x\n",
			ifp->if_xname, uio->uio_resid, minor(dev));

		return (EIO);
	}

	if ((m = m_uiotombuf(uio, M_DONTWAIT, 0, ETHER_ALIGN)) == NULL) {
		ifp->if_ierrors ++;
		return (error);
	}

	m->m_pkthdr.rcvif = ifp;

	/* Pass packet up to parent. */
	(*ifp->if_input)(ifp, m);
	ifp->if_ipackets ++; /* ibytes are counted in parent */

	return (0);
} /* tapwrite */


/*
 * tappoll
 *
 * the poll interface, this is only useful on reads
 * really. the write detect always returns true, write never blocks
 * anyway, it either accepts the packet or drops it
 */
static int
tappoll(dev, events, td)
	struct cdev *dev;
	int		 events;
	struct thread	*td;
{
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = tp->tap_ifp;
	int			 s, revents = 0;

	TAPDEBUG("%s polling, minor = %#x\n", 
		ifp->if_xname, minor(dev));

	s = splimp();
	if (events & (POLLIN | POLLRDNORM)) {
		if (ifp->if_snd.ifq_len > 0) {
			TAPDEBUG("%s have data in queue. len = %d, " \
				"minor = %#x\n", ifp->if_xname,
				ifp->if_snd.ifq_len, minor(dev));

			revents |= (events & (POLLIN | POLLRDNORM));
		} else {
			TAPDEBUG("%s waiting for data, minor = %#x\n",
				ifp->if_xname, minor(dev));

			selrecord(td, &tp->tap_rsel);
		}
	}

	if (events & (POLLOUT | POLLWRNORM))
		revents |= (events & (POLLOUT | POLLWRNORM));

	splx(s);
	return (revents);
} /* tappoll */
