/*
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
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/ttycom.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <machine/bus.h>	/* XXX: Shouldn't really be required! */
#include <sys/rman.h>
#include <sys/queue.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>

#include <netinet/in.h>

#include <net/if_tapvar.h>
#include <net/if_tap.h>


#define CDEV_NAME	"tap"
#define CDEV_MAJOR	149
#define TAPDEBUG	if (tapdebug) printf

#define TAP		"tap"
#define VMNET		"vmnet"
#define TAPMAXUNIT	0x7fff
#define VMNET_DEV_MASK	0x00800000
		/*	0x007f00ff	*/

/* module */
static int 		tapmodevent(module_t, int, void *);

/* device */
static void		tapclone(void *, char *, int, dev_t *);
static void		tapcreate(dev_t);

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
	/* open */	tapopen,
	/* close */	tapclose,
	/* read */	tapread,
	/* write */	tapwrite,
	/* ioctl */	tapioctl,
	/* poll */	tappoll,
	/* mmap */	nommap,
	/* startegy */	nostrategy,
	/* dev name */	CDEV_NAME,
	/* dev major */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static int			tapdebug = 0;        /* debug flag   */
static SLIST_HEAD(, tap_softc)	taphead;             /* first device */
static udev_t			tapbasedev = NOUDEV; /* base device  */
static struct rman		tapdevunits[2];      /* device units */
#define		tapunits	tapdevunits
#define		vmnetunits	(tapdevunits + 1)

MALLOC_DECLARE(M_TAP);
MALLOC_DEFINE(M_TAP, CDEV_NAME, "Ethernet tunnel interface");
SYSCTL_INT(_debug, OID_AUTO, if_tap_debug, CTLFLAG_RW, &tapdebug, 0, "");
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
	int			 error, s;

	switch (type) {
	case MOD_LOAD:
		/* initialize resources */
		tapunits->rm_type = RMAN_ARRAY;
		tapunits->rm_descr = "open tap units";
		vmnetunits->rm_type = RMAN_ARRAY;
		vmnetunits->rm_descr = "open vmnet units";

		error = rman_init(tapunits);
		if (error != 0)
			goto bail;

		error = rman_init(vmnetunits);
		if (error != 0)
			goto bail1;

		error = rman_manage_region(tapunits, 0, TAPMAXUNIT);
		if (error != 0)
			goto bail2;

		error = rman_manage_region(vmnetunits, 0, TAPMAXUNIT);
		if (error != 0)
			goto bail2;

		/* intitialize device */

		SLIST_INIT(&taphead);

		eh_tag = EVENTHANDLER_REGISTER(dev_clone, tapclone, 0, 1000);
		if (eh_tag == NULL) {
			error = ENOMEM;
			goto bail2;
		}

		if (!devfs_present) {
			error = cdevsw_add(&tap_cdevsw);
			if (error != 0) {
				EVENTHANDLER_DEREGISTER(dev_clone, eh_tag);
				goto bail2;
			}
		}

		return (0);
bail2:
		rman_fini(vmnetunits);
bail1:
		rman_fini(tapunits);
bail:
		return (error);

	case MOD_UNLOAD:
		SLIST_FOREACH(tp, &taphead, tap_next)
			if (tp->tap_unit != NULL)
				return (EBUSY);

		EVENTHANDLER_DEREGISTER(dev_clone, eh_tag);

		error = rman_fini(tapunits);
		KASSERT((error == 0), ("Could not fini tap units"));
		error = rman_fini(vmnetunits);
		KASSERT((error == 0), ("Could not fini vmnet units"));

		while ((tp = SLIST_FIRST(&taphead)) != NULL) {
			SLIST_REMOVE_HEAD(&taphead, tap_next);

			ifp = &tp->tap_if;

			TAPDEBUG("detaching %s%d\n", ifp->if_name,ifp->if_unit);

			KASSERT(!(tp->tap_flags & TAP_OPEN), 
				("%s%d flags is out of sync", ifp->if_name, 
				ifp->if_unit));

			/* XXX makedev check? nah.. not right now :) */

			s = splimp();
			ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
			splx(s);

			free(tp, M_TAP);
		}

 		if (tapbasedev != NOUDEV)
			destroy_dev(udev2dev(tapbasedev, 0));

                if (!devfs_present)
                        cdevsw_remove(&tap_cdevsw);

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
tapclone(arg, name, namelen, dev)
	void	*arg;
	char	*name;
	int	 namelen;
	dev_t	*dev;
{
	int		 unit, minor = 0 /* XXX avoid warning */ , error;
	char		*device_name = name;
	struct resource	*r = NULL;

	if (*dev != NODEV)
		return;

	if (strcmp(device_name, TAP) == 0) {
		/* get first free tap unit */
		r = rman_reserve_resource(tapunits, 0, TAPMAXUNIT, 1, 
			RF_ALLOCATED | RF_ACTIVE, NULL);
		unit = rman_get_start(r);
		minor = unit2minor(unit);
	}
	else if (strcmp(device_name, VMNET) == 0) {
		/* get first free vmnet unit */
		r = rman_reserve_resource(vmnetunits, 0, TAPMAXUNIT, 1, 
			RF_ALLOCATED | RF_ACTIVE, NULL);
		unit = rman_get_start(r);
		minor = unit2minor(unit) | VMNET_DEV_MASK;
	}

	if (r != NULL) { /* need cloning */
		TAPDEBUG("%s%d is available. minor = %#x\n",
			device_name, unit, minor);

		error = rman_release_resource(r);
		KASSERT((error == 0), ("Could not release tap/vmnet unit"));

		/* check if device for the unit has been created */
		*dev = makedev(CDEV_MAJOR, minor);
		if ((*dev)->si_flags & SI_NAMED) {
			TAPDEBUG("%s%d device exists. minor = %#x\n", 
				device_name, unit, minor);
			return; /* device has been created */
		}
	} else { /* try to match name/unit, first try tap then vmnet */
		device_name = TAP;
		if (dev_stdclone(name, NULL, device_name, &unit) != 1) {
			device_name = VMNET;

			if (dev_stdclone(name, NULL, device_name, &unit) != 1)
				return;

			minor = unit2minor(unit) | VMNET_DEV_MASK;
		} else
			minor = unit2minor(unit);
	}

	TAPDEBUG("make_dev(%s%d). minor = %#x\n", device_name, unit, minor);

	*dev = make_dev(&tap_cdevsw, minor, UID_ROOT, GID_WHEEL, 0600, "%s%d",
			device_name, unit);

	if (tapbasedev == NOUDEV)
		tapbasedev = (*dev)->si_udev;
	else {
		(*dev)->si_flags |= SI_CHEAPCLONE;
		dev_depends(udev2dev(tapbasedev, 0), *dev);
	}
} /* tapclone */


/*
 * tapcreate
 *
 * to create interface
 */
static void
tapcreate(dev)
	dev_t	dev;
{
	struct ifnet		*ifp = NULL;
	struct tap_softc	*tp = NULL;
	unsigned short		 macaddr_hi;
	int			 unit, s;
	char			*name = NULL;

	/* allocate driver storage and create device */
	MALLOC(tp, struct tap_softc *, sizeof(*tp), M_TAP, M_WAITOK | M_ZERO);
	SLIST_INSERT_HEAD(&taphead, tp, tap_next);

	unit = dev2unit(dev) & TAPMAXUNIT;

	/* select device: tap or vmnet */
	if (minor(dev) & VMNET_DEV_MASK) {
		name = VMNET;
		tp->tap_flags |= TAP_VMNET;
	} else
		name = TAP;

	TAPDEBUG("tapcreate(%s%d). minor = %#x\n", name, unit, minor(dev));

	if (!(dev->si_flags & SI_NAMED))
		dev = make_dev(&tap_cdevsw, minor(dev), UID_ROOT, GID_WHEEL, 
						0600, "%s%d", name, unit);

	/* generate fake MAC address: 00 bd xx xx xx unit_no */
	macaddr_hi = htons(0x00bd);
	bcopy(&macaddr_hi, &tp->arpcom.ac_enaddr[0], sizeof(short));
	bcopy(&ticks, &tp->arpcom.ac_enaddr[2], sizeof(long));
	tp->arpcom.ac_enaddr[5] = (u_char)unit;

	/* fill the rest and attach interface */	
	ifp = &tp->tap_if;
	ifp->if_softc = tp;
	ifp->if_unit = unit;
	ifp->if_name = name;
	ifp->if_init = tapifinit;
	ifp->if_output = ether_output;
	ifp->if_start = tapifstart;
	ifp->if_ioctl = tapifioctl;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = (IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST);
	ifp->if_snd.ifq_maxlen = ifqmaxlen;

	dev->si_drv1 = tp;

	s = splimp();
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	splx(s);

	tp->tap_flags |= TAP_INITED;

	TAPDEBUG("interface %s%d is created. minor = %#x\n", 
		ifp->if_name, ifp->if_unit, minor(dev));
} /* tapcreate */


/*
 * tapopen 
 *
 * to open tunnel. must be superuser
 */
static int
tapopen(dev, flag, mode, td)
	dev_t		 dev;
	int		 flag;
	int		 mode;
	struct thread	*td;
{
	struct tap_softc	*tp = NULL;
	int			 unit, error;
	struct resource		*r = NULL;

	if ((error = suser(td)) != 0)
		return (error);

	unit = dev2unit(dev) & TAPMAXUNIT;

	if (minor(dev) & VMNET_DEV_MASK)
		r = rman_reserve_resource(vmnetunits, unit, unit, 1, 
			RF_ALLOCATED | RF_ACTIVE, NULL);
	else
		r = rman_reserve_resource(tapunits, unit, unit, 1, 
			RF_ALLOCATED | RF_ACTIVE, NULL);

	if (r == NULL)
		return (EBUSY);

	dev->si_flags &= ~SI_CHEAPCLONE;

	tp = dev->si_drv1;
	if (tp == NULL) {
		tapcreate(dev);
		tp = dev->si_drv1;
	}

	KASSERT(!(tp->tap_flags & TAP_OPEN), 
		("%s%d flags is out of sync", tp->tap_if.if_name, unit));

	bcopy(tp->arpcom.ac_enaddr, tp->ether_addr, sizeof(tp->ether_addr));

	tp->tap_unit = r;
	tp->tap_pid = td->td_proc->p_pid;
	tp->tap_flags |= TAP_OPEN;

	TAPDEBUG("%s%d is open. minor = %#x\n", 
		tp->tap_if.if_name, unit, minor(dev));

	return (0);
} /* tapopen */


/*
 * tapclose
 *
 * close the device - mark i/f down & delete routing info
 */
static int
tapclose(dev, foo, bar, td)
	dev_t		 dev;
	int		 foo;
	int		 bar;
	struct thread	*td;
{
	int			 s, error;
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = &tp->tap_if;

	KASSERT((tp->tap_unit != NULL),
		("%s%d is not open", ifp->if_name, ifp->if_unit));

	/* junk all pending output */
	IF_DRAIN(&ifp->if_snd);

	/*
	 * do not bring the interface down, and do not anything with
	 * interface, if we are in VMnet mode. just close the device.
	 */

	if (((tp->tap_flags & TAP_VMNET) == 0) && (ifp->if_flags & IFF_UP)) {
		s = splimp();
		if_down(ifp);
		if (ifp->if_flags & IFF_RUNNING) {
			/* find internet addresses and delete routes */
			struct ifaddr	*ifa = NULL;

			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				if (ifa->ifa_addr->sa_family == AF_INET) {
					rtinit(ifa, (int)RTM_DELETE, 0);

					/* remove address from interface */
					bzero(ifa->ifa_addr, 
						   sizeof(*(ifa->ifa_addr)));
					bzero(ifa->ifa_dstaddr, 
						   sizeof(*(ifa->ifa_dstaddr)));
					bzero(ifa->ifa_netmask, 
						   sizeof(*(ifa->ifa_netmask)));
				}
			}

			ifp->if_flags &= ~IFF_RUNNING;
		}
		splx(s);
	}

	funsetown(tp->tap_sigio);
	selwakeup(&tp->tap_rsel);

	tp->tap_flags &= ~TAP_OPEN;
	tp->tap_pid = 0;
	error = rman_release_resource(tp->tap_unit);
	KASSERT((error == 0), 
		("%s%d could not release unit", ifp->if_name, ifp->if_unit)); 
	tp->tap_unit = NULL;

	TAPDEBUG("%s%d is closed. minor = %#x\n", 
		ifp->if_name, ifp->if_unit, minor(dev));

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
	struct ifnet		*ifp = &tp->tap_if;

	TAPDEBUG("initializing %s%d\n", ifp->if_name, ifp->if_unit);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* attempt to start output */
	tapifstart(ifp);
} /* tapifinit */


/*
 * tapifioctl
 *
 * Process an ioctl request on network interface
 */
int
tapifioctl(ifp, cmd, data)
	struct ifnet	*ifp;
	u_long		 cmd;
	caddr_t		 data;
{
	struct tap_softc 	*tp = (struct tap_softc *)(ifp->if_softc);
	struct ifstat		*ifs = NULL;
	int			 s, dummy;

	switch (cmd) {
		case SIOCSIFADDR:
		case SIOCGIFADDR:
		case SIOCSIFMTU:
			s = splimp();
			dummy = ether_ioctl(ifp, cmd, data);
			splx(s);
			return (dummy);

		case SIOCSIFFLAGS: /* XXX -- just like vmnet does */
		case SIOCADDMULTI:
		case SIOCDELMULTI:
			break;

		case SIOCGIFSTATUS:
			s = splimp();
			ifs = (struct ifstat *)data;
			dummy = strlen(ifs->ascii);
			if (tp->tap_pid != 0 && dummy < sizeof(ifs->ascii))
				snprintf(ifs->ascii + dummy,
					sizeof(ifs->ascii) - dummy,
					"\tOpened by PID %d\n", tp->tap_pid);
			splx(s);
			break;

		default:
			return (EINVAL);
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

	TAPDEBUG("%s%d starting\n", ifp->if_name, ifp->if_unit);

	/*
	 * do not junk pending output if we are in VMnet mode.
	 * XXX: can this do any harm because of queue overflow?
	 */

	if (((tp->tap_flags & TAP_VMNET) == 0) && 
	    ((tp->tap_flags & TAP_READY) != TAP_READY)) {
		struct mbuf	*m = NULL;

		TAPDEBUG("%s%d not ready, tap_flags = 0x%x\n", ifp->if_name, 
			ifp->if_unit, tp->tap_flags);

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

	s = splimp();
	ifp->if_flags |= IFF_OACTIVE;

	if (ifp->if_snd.ifq_len != 0) {
		if (tp->tap_flags & TAP_RWAIT) {
			tp->tap_flags &= ~TAP_RWAIT;
			wakeup((caddr_t)tp);
		}

		if ((tp->tap_flags & TAP_ASYNC) && (tp->tap_sigio != NULL))
			pgsigio(tp->tap_sigio, SIGIO, 0);

		selwakeup(&tp->tap_rsel);
		ifp->if_opackets ++; /* obytes are counted in ether_output */
	}

	ifp->if_flags &= ~IFF_OACTIVE;
	splx(s);
} /* tapifstart */


/*
 * tapioctl
 *
 * the cdevsw interface is now pretty minimal
 */
static int
tapioctl(dev, cmd, data, flag, td)
	dev_t		 dev;
	u_long		 cmd;
	caddr_t		 data;
	int		 flag;
	struct thread	*td;
{
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = &tp->tap_if;
 	struct tapinfo		*tapp = NULL;
	int			 s;
	short			 f;

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
			if (*(int *)data)
				tp->tap_flags |= TAP_ASYNC;
			else
				tp->tap_flags &= ~TAP_ASYNC;
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
			*(int *)data = fgetown(tp->tap_sigio);
			return (0);

		/* this is deprecated, FIOSETOWN should be used instead */
		case TIOCSPGRP:
			return (fsetown(-(*(int *)data), &tp->tap_sigio));

		/* this is deprecated, FIOGETOWN should be used instead */
		case TIOCGPGRP:
			*(int *)data = -fgetown(tp->tap_sigio);
			return (0);

		/* VMware/VMnet port ioctl's */

		case SIOCGIFFLAGS:	/* get ifnet flags */
			bcopy(&ifp->if_flags, data, sizeof(ifp->if_flags));
			break;

		case VMIO_SIOCSIFFLAGS: /* VMware/VMnet SIOCSIFFLAGS */
			f = *(short *)data;
			f &= 0x0fff;
			f &= ~IFF_CANTCHANGE;
			f |= IFF_UP;

			s = splimp();
			ifp->if_flags = f | (ifp->if_flags & IFF_CANTCHANGE);
			splx(s);
			break;

		case OSIOCGIFADDR:	/* get MAC address of the remote side */
		case SIOCGIFADDR:
			bcopy(tp->ether_addr, data, sizeof(tp->ether_addr));
			break;

		case SIOCSIFADDR:	/* set MAC address of the remote side */
			bcopy(data, tp->ether_addr, sizeof(tp->ether_addr));
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
	dev_t		 dev;
	struct uio	*uio;
	int		 flag;
{
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = &tp->tap_if;
	struct mbuf		*m = NULL;
	int			 error = 0, len, s;

	TAPDEBUG("%s%d reading, minor = %#x\n", 
		ifp->if_name, ifp->if_unit, minor(dev));

	if ((tp->tap_flags & TAP_READY) != TAP_READY) {
		TAPDEBUG("%s%d not ready. minor = %#x, tap_flags = 0x%x\n",
			ifp->if_name, ifp->if_unit, minor(dev), tp->tap_flags);

		return (EHOSTDOWN);
	}

	tp->tap_flags &= ~TAP_RWAIT;

	/* sleep until we get a packet */
	do {
		s = splimp();
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL) {
			if (flag & IO_NDELAY)
				return (EWOULDBLOCK);
			
			tp->tap_flags |= TAP_RWAIT;
			error = tsleep((caddr_t)tp,PCATCH|(PZERO+1),"taprd",0);
			if (error)
				return (error);
		}
	} while (m == NULL);

	/* feed packet to bpf */
	if (ifp->if_bpf != NULL)
		bpf_mtap(ifp, m);

	/* xfer packet to user space */
	while ((m != NULL) && (uio->uio_resid > 0) && (error == 0)) {
		len = min(uio->uio_resid, m->m_len);
		if (len == 0)
			break;

		error = uiomove(mtod(m, caddr_t), len, uio);
		m = m_free(m);
	}

	if (m != NULL) {
		TAPDEBUG("%s%d dropping mbuf, minor = %#x\n", ifp->if_name, 
			ifp->if_unit, minor(dev));
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
	dev_t		 dev;
	struct uio	*uio;
	int		 flag;
{
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = &tp->tap_if;
	struct mbuf		*top = NULL, **mp = NULL, *m = NULL;
	struct ether_header	*eh = NULL;
	int		 	 error = 0, tlen, mlen;

	TAPDEBUG("%s%d writting, minor = %#x\n", 
		ifp->if_name, ifp->if_unit, minor(dev));

	if (uio->uio_resid == 0)
		return (0);

	if ((uio->uio_resid < 0) || (uio->uio_resid > TAPMRU)) {
		TAPDEBUG("%s%d invalid packet len = %d, minor = %#x\n",
			ifp->if_name, ifp->if_unit, uio->uio_resid, minor(dev));

		return (EIO);
	}
	tlen = uio->uio_resid;

	/* get a header mbuf */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	mlen = MHLEN;

	top = 0;
	mp = &top;
	while ((error == 0) && (uio->uio_resid > 0)) {
		m->m_len = min(mlen, uio->uio_resid);
		error = uiomove(mtod(m, caddr_t), m->m_len, uio);
		*mp = m;
		mp = &m->m_next;
		if (uio->uio_resid > 0) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				error = ENOBUFS;
				break;
			}
			mlen = MLEN;
		}
	}
	if (error) {
		ifp->if_ierrors ++;
		if (top)
			m_freem(top);
		return (error);
	}

	top->m_pkthdr.len = tlen;
	top->m_pkthdr.rcvif = ifp;
	
	/*
	 * Ethernet bridge and bpf are handled in ether_input
	 *
	 * adjust mbuf and give packet to the ether_input
	 */

	eh = mtod(top, struct ether_header *);
	m_adj(top, sizeof(struct ether_header));
	ether_input(ifp, eh, top);
	ifp->if_ipackets ++; /* ibytes are counted in ether_input */

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
	dev_t		 dev;
	int		 events;
	struct thread	*td;
{
	struct tap_softc	*tp = dev->si_drv1;
	struct ifnet		*ifp = &tp->tap_if;
	int		 	 s, revents = 0;

	TAPDEBUG("%s%d polling, minor = %#x\n", 
		ifp->if_name, ifp->if_unit, minor(dev));

	s = splimp();
	if (events & (POLLIN | POLLRDNORM)) {
		if (ifp->if_snd.ifq_len > 0) {
			TAPDEBUG("%s%d have data in queue. len = %d, " \
				"minor = %#x\n", ifp->if_name, ifp->if_unit,
				ifp->if_snd.ifq_len, minor(dev));

			revents |= (events & (POLLIN | POLLRDNORM));
		} else {
			TAPDEBUG("%s%d waiting for data, minor = %#x\n",
				ifp->if_name, ifp->if_unit, minor(dev));

			selrecord(td, &tp->tap_rsel);
		}
	}

	if (events & (POLLOUT | POLLWRNORM))
		revents |= (events & (POLLOUT | POLLWRNORM));

	splx(s);
	return (revents);
} /* tappoll */
