/*-
 * Copyright (c) 1998, 2001 Nicolas Souchu
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * I2C bus IP driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/filio.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/time.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>

#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/netisr.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbus_if.h"

#define PCF_MASTER_ADDRESS 0xaa

#define ICHDRLEN	sizeof(u_int32_t)
#define ICMTU		1500		/* default mtu */

struct ic_softc {
	struct ifnet ic_if;

	u_char ic_addr;			/* peer I2C address */

	int ic_sending;

	char *ic_obuf;
	char *ic_ifbuf;
	char *ic_cp;

	int ic_xfercnt;

	int ic_iferrs;
};

static devclass_t ic_devclass;

static int icprobe(device_t);
static int icattach(device_t);

static int icioctl(struct ifnet *, u_long, caddr_t);
static int icoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
		struct rtentry *);

static void icintr(device_t, int, char *);

static device_method_t ic_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		icprobe),
	DEVMETHOD(device_attach,	icattach),

	/* iicbus interface */
	DEVMETHOD(iicbus_intr,		icintr),

	{ 0, 0 }
};

static driver_t ic_driver = {
	"ic",
	ic_methods,
	sizeof(struct ic_softc),
};

/*
 * icprobe()
 */
static int
icprobe(device_t dev)
{
	return (0);
}
	
/*
 * icattach()
 */
static int
icattach(device_t dev)
{
	struct ic_softc *sc = (struct ic_softc *)device_get_softc(dev);
	struct ifnet *ifp = &sc->ic_if;

	sc->ic_addr = PCF_MASTER_ADDRESS;	/* XXX only PCF masters */

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ICMTU;
	ifp->if_flags = IFF_SIMPLEX | IFF_POINTOPOINT | IFF_MULTICAST |
	    IFF_NEEDSGIANT;
	ifp->if_ioctl = icioctl;
	ifp->if_output = icoutput;
	ifp->if_type = IFT_PARA;
	ifp->if_hdrlen = 0;
	ifp->if_addrlen = 0;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	if_attach(ifp);

	bpfattach(ifp, DLT_NULL, ICHDRLEN);

	return (0);
}

/*
 * iciotcl()
 */
static int
icioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
    device_t icdev = devclass_get_device(ic_devclass, ifp->if_dunit);
    device_t parent = device_get_parent(icdev);
    struct ic_softc *sc = (struct ic_softc *)device_get_softc(icdev);

    struct ifaddr *ifa = (struct ifaddr *)data;
    struct ifreq *ifr = (struct ifreq *)data;

    u_char *iptr, *optr;
    int error;

    switch (cmd) {

    case SIOCSIFDSTADDR:
    case SIOCAIFADDR:
    case SIOCSIFADDR:
	if (ifa->ifa_addr->sa_family != AF_INET)
	    return EAFNOSUPPORT;
	ifp->if_flags |= IFF_UP;
	/* FALLTHROUGH */
    case SIOCSIFFLAGS:
	if ((!(ifp->if_flags & IFF_UP)) && (ifp->if_flags & IFF_RUNNING)) {

	    /* XXX disable PCF */
	    ifp->if_flags &= ~IFF_RUNNING;

	    /* IFF_UP is not set, try to release the bus anyway */
	    iicbus_release_bus(parent, icdev);
	    break;
	}
	if (((ifp->if_flags & IFF_UP)) && (!(ifp->if_flags & IFF_RUNNING))) {

	    if ((error = iicbus_request_bus(parent, icdev, IIC_WAIT|IIC_INTR)))
		return (error);

	    sc->ic_obuf = malloc(sc->ic_if.if_mtu + ICHDRLEN,
				  M_DEVBUF, M_WAITOK);
	    if (!sc->ic_obuf) {
		iicbus_release_bus(parent, icdev);
		return ENOBUFS;
	    }

	    sc->ic_ifbuf = malloc(sc->ic_if.if_mtu + ICHDRLEN,
				  M_DEVBUF, M_WAITOK);
	    if (!sc->ic_ifbuf) {
		iicbus_release_bus(parent, icdev);
		return ENOBUFS;
	    }

	    iicbus_reset(parent, IIC_FASTEST, 0, NULL);

	    ifp->if_flags |= IFF_RUNNING;
	}
	break;

    case SIOCSIFMTU:
	/* save previous buffers */
	iptr = sc->ic_ifbuf;
	optr = sc->ic_obuf;

	/* allocate input buffer */
	sc->ic_ifbuf = malloc(ifr->ifr_mtu+ICHDRLEN, M_DEVBUF, M_NOWAIT);
	if (!sc->ic_ifbuf) {

	    sc->ic_ifbuf = iptr;
	    sc->ic_obuf = optr;

	    return ENOBUFS;
	}

	/* allocate output buffer */
	sc->ic_ifbuf = malloc(ifr->ifr_mtu+ICHDRLEN, M_DEVBUF, M_NOWAIT);
	if (!sc->ic_obuf) {

	    free(sc->ic_ifbuf,M_DEVBUF);

	    sc->ic_ifbuf = iptr;
	    sc->ic_obuf = optr;

	    return ENOBUFS;
	}

	if (iptr)
	    free(iptr,M_DEVBUF);

	if (optr)
	    free(optr,M_DEVBUF);

	sc->ic_if.if_mtu = ifr->ifr_mtu;
	break;

    case SIOCGIFMTU:
	ifr->ifr_mtu = sc->ic_if.if_mtu;
	break;

    case SIOCADDMULTI:
    case SIOCDELMULTI:
	if (ifr == 0) {
	    return EAFNOSUPPORT;		/* XXX */
	}
	switch (ifr->ifr_addr.sa_family) {

	case AF_INET:
	    break;

	default:
	    return EAFNOSUPPORT;
	}
	break;

    default:
	return EINVAL;
    }
    return 0;
}

/*
 * icintr()
 */
static void
icintr (device_t dev, int event, char *ptr)
{
	struct ic_softc *sc = (struct ic_softc *)device_get_softc(dev);
	int unit = device_get_unit(dev);
	int s, len;
	struct mbuf *top;
	
	s = splhigh();

	switch (event) {

	case INTR_GENERAL:
	case INTR_START:
		sc->ic_cp = sc->ic_ifbuf;
		sc->ic_xfercnt = 0;
		break;

	case INTR_STOP:

	  /* if any error occured during transfert,
	   * drop the packet */
	  if (sc->ic_iferrs)
	    goto err;

	  if ((len = sc->ic_xfercnt) == 0)
		break;					/* ignore */

	  if (len <= ICHDRLEN)
	    goto err;

	  len -= ICHDRLEN;
	  sc->ic_if.if_ipackets ++;
	  sc->ic_if.if_ibytes += len;

	  BPF_TAP(&sc->ic_if, sc->ic_ifbuf, len + ICHDRLEN);

	  top = m_devget(sc->ic_ifbuf + ICHDRLEN, len, 0, &sc->ic_if, 0);

	  if (top)
	    netisr_dispatch(NETISR_IP, top);
	  break;

	err:
	  printf("ic%d: errors (%d)!\n", unit, sc->ic_iferrs);

	  sc->ic_iferrs = 0;			/* reset error count */
	  sc->ic_if.if_ierrors ++;

	  break;

	case INTR_RECEIVE:
		if (sc->ic_xfercnt >= sc->ic_if.if_mtu+ICHDRLEN) {
			sc->ic_iferrs ++;

		} else {
			*sc->ic_cp++ = *ptr;
			sc->ic_xfercnt ++;
		}
		break;

	case INTR_NOACK:			/* xfer terminated by master */
		break;

	case INTR_TRANSMIT:
		*ptr = 0xff;					/* XXX */
	  	break;

	case INTR_ERROR:
		sc->ic_iferrs ++;
		break;

	default:
		panic("%s: unknown event (%d)!", __func__, event);
	}

	splx(s);
	return;
}

/*
 * icoutput()
 */
static int
icoutput(struct ifnet *ifp, struct mbuf *m,
	struct sockaddr *dst, struct rtentry *rt)
{
	device_t icdev = devclass_get_device(ic_devclass, ifp->if_dunit);
	device_t parent = device_get_parent(icdev);
	struct ic_softc *sc = (struct ic_softc *)device_get_softc(icdev);

	int s, len, sent;
	struct mbuf *mm;
	u_char *cp;
	u_int32_t hdr = dst->sa_family;

	ifp->if_flags |= IFF_RUNNING;

	s = splhigh();

	/* already sending? */
	if (sc->ic_sending) {
		ifp->if_oerrors ++;
		goto error;
	}
		
	/* insert header */
	bcopy ((char *)&hdr, sc->ic_obuf, ICHDRLEN);

	cp = sc->ic_obuf + ICHDRLEN;
	len = 0;
	mm = m;
	do {
		if (len + mm->m_len > sc->ic_if.if_mtu) {
			/* packet to large */
			ifp->if_oerrors ++;
			goto error;
		}
			
		bcopy(mtod(mm,char *), cp, mm->m_len);
		cp += mm->m_len;
		len += mm->m_len;

	} while ((mm = mm->m_next));

	BPF_MTAP2(ifp, &hdr, sizeof(hdr), m);

	sc->ic_sending = 1;

	m_freem(m);
	splx(s);

	/* send the packet */
	if (iicbus_block_write(parent, sc->ic_addr, sc->ic_obuf,
				len + ICHDRLEN, &sent))

		ifp->if_oerrors ++;
	else {
		ifp->if_opackets ++;
		ifp->if_obytes += len;
	}

	sc->ic_sending = 0;

	return (0);

error:
	m_freem(m);
	splx(s);

	return(0);
}

DRIVER_MODULE(ic, iicbus, ic_driver, ic_devclass, 0, 0);
MODULE_DEPEND(ic, iicbus, IICBUS_MINVER, IICBUS_PREFVER, IICBUS_MAXVER);
MODULE_VERSION(ic, 1);
