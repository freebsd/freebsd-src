/*-
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * CATC USB-EL1210A USB to ethernet driver. Used in the CATC Netmate
 * adapters and others.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The CATC USB-EL1210A provides USB ethernet support at 10Mbps. The
 * RX filter uses a 512-bit multicast hash table, single perfect entry
 * for the station address, and promiscuous mode. Unlike the ADMtek
 * and KLSI chips, the CATC ASIC supports read and write combining
 * mode where multiple packets can be transfered using a single bulk
 * transaction, which helps performance a great deal.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <sys/bus.h>
#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include "usbdevs.h"
#include <dev/usb/usb_ethersubr.h>

#include <dev/usb/if_cuereg.h>

/*
 * Various supported device vendors/products.
 */
static struct cue_type cue_devs[] = {
	{ USB_VENDOR_CATC, USB_PRODUCT_CATC_NETMATE },
	{ USB_VENDOR_CATC, USB_PRODUCT_CATC_NETMATE2 },
	{ USB_VENDOR_SMARTBRIDGES, USB_PRODUCT_SMARTBRIDGES_SMARTLINK },
	{ 0, 0 }
};

static int cue_match(device_t);
static int cue_attach(device_t);
static int cue_detach(device_t);

static int cue_encap(struct cue_softc *, struct mbuf *, int);
static void cue_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void cue_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void cue_tick(void *);
static void cue_rxstart(struct ifnet *);
static void cue_start(struct ifnet *);
static int cue_ioctl(struct ifnet *, u_long, caddr_t);
static void cue_init(void *);
static void cue_stop(struct cue_softc *);
static void cue_watchdog(struct ifnet *);
static void cue_shutdown(device_t);

static void cue_setmulti(struct cue_softc *);
static uint32_t cue_mchash(const uint8_t *);
static void cue_reset(struct cue_softc *);

static int cue_csr_read_1(struct cue_softc *, int);
static int cue_csr_write_1(struct cue_softc *, int, int);
static int cue_csr_read_2(struct cue_softc *, int);
#ifdef notdef
static int cue_csr_write_2(struct cue_softc *, int, int);
#endif
static int cue_mem(struct cue_softc *, int, int, void *, int);
static int cue_getmac(struct cue_softc *, void *);

static device_method_t cue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cue_match),
	DEVMETHOD(device_attach,	cue_attach),
	DEVMETHOD(device_detach,	cue_detach),
	DEVMETHOD(device_shutdown,	cue_shutdown),

	{ 0, 0 }
};

static driver_t cue_driver = {
	"cue",
	cue_methods,
	sizeof(struct cue_softc)
};

static devclass_t cue_devclass;

DRIVER_MODULE(cue, uhub, cue_driver, cue_devclass, usbd_driver_load, 0);
MODULE_DEPEND(cue, usb, 1, 1, 1);
MODULE_DEPEND(cue, ether, 1, 1, 1);

#define CUE_SETBIT(sc, reg, x)				\
	cue_csr_write_1(sc, reg, cue_csr_read_1(sc, reg) | (x))

#define CUE_CLRBIT(sc, reg, x)				\
	cue_csr_write_1(sc, reg, cue_csr_read_1(sc, reg) & ~(x))

static int
cue_csr_read_1(struct cue_softc *sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	u_int8_t		val = 0;

	if (sc->cue_dying)
		return(0);

	CUE_LOCK(sc);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request(sc->cue_udev, &req, &val);

	CUE_UNLOCK(sc);

	if (err)
		return(0);

	return(val);
}

static int
cue_csr_read_2(struct cue_softc *sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	u_int16_t		val = 0;

	if (sc->cue_dying)
		return(0);

	CUE_LOCK(sc);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	err = usbd_do_request(sc->cue_udev, &req, &val);

	CUE_UNLOCK(sc);

	if (err)
		return(0);

	return(val);
}

static int
cue_csr_write_1(struct cue_softc *sc, int reg, int val)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->cue_dying)
		return(0);

	CUE_LOCK(sc);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->cue_udev, &req, NULL);

	CUE_UNLOCK(sc);

	if (err)
		return(-1);

	return(0);
}

#ifdef notdef
static int
cue_csr_write_2(struct cue_softc *sc, int reg, int val)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->cue_dying)
		return(0);

	CUE_LOCK(sc);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 0);

	err = usbd_do_request(sc->cue_udev, &req, NULL);

	CUE_UNLOCK(sc);

	if (err)
		return(-1);

	return(0);
}
#endif

static int
cue_mem(struct cue_softc *sc, int cmd, int addr, void *buf, int len)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->cue_dying)
		return(0);

	CUE_LOCK(sc);

	if (cmd == CUE_CMD_READSRAM)
		req.bmRequestType = UT_READ_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = cmd;
	USETW(req.wValue, 0);
	USETW(req.wIndex, addr);
	USETW(req.wLength, len);

	err = usbd_do_request(sc->cue_udev, &req, buf);

	CUE_UNLOCK(sc);

	if (err)
		return(-1);

	return(0);
}

static int
cue_getmac(struct cue_softc *sc, void *buf)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->cue_dying)
		return(0);

	CUE_LOCK(sc);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_GET_MACADDR;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, ETHER_ADDR_LEN);

	err = usbd_do_request(sc->cue_udev, &req, buf);

	CUE_UNLOCK(sc);

	if (err) {
		device_printf(sc->cue_dev, "read MAC address failed\n");
		return(-1);
	}

	return(0);
}

#define CUE_BITS	9

static uint32_t
cue_mchash(const uint8_t *addr)
{
	uint32_t crc;

	/* Compute CRC for the address value. */
	crc = ether_crc32_le(addr, ETHER_ADDR_LEN);

	return (crc & ((1 << CUE_BITS) - 1));
}

static void
cue_setmulti(struct cue_softc *sc)
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int32_t		h = 0, i;

	ifp = sc->cue_ifp;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		for (i = 0; i < CUE_MCAST_TABLE_LEN; i++)
			sc->cue_mctab[i] = 0xFF;
		cue_mem(sc, CUE_CMD_WRITESRAM, CUE_MCAST_TABLE_ADDR,
		    &sc->cue_mctab, CUE_MCAST_TABLE_LEN);
		return;
	}

	/* first, zot all the existing hash bits */
	for (i = 0; i < CUE_MCAST_TABLE_LEN; i++)
		sc->cue_mctab[i] = 0;

	/* now program new ones */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = cue_mchash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		sc->cue_mctab[h >> 3] |= 1 << (h & 0x7);
	}
	IF_ADDR_UNLOCK(ifp);

	/*
	 * Also include the broadcast address in the filter
	 * so we can receive broadcast frames.
 	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		h = cue_mchash(ifp->if_broadcastaddr);
		sc->cue_mctab[h >> 3] |= 1 << (h & 0x7);
	}

	cue_mem(sc, CUE_CMD_WRITESRAM, CUE_MCAST_TABLE_ADDR,
	    &sc->cue_mctab, CUE_MCAST_TABLE_LEN);

	return;
}

static void
cue_reset(struct cue_softc *sc)
{
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->cue_dying)
		return;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = CUE_CMD_RESET;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->cue_udev, &req, NULL);
	if (err)
		device_printf(sc->cue_dev, "reset failed\n");

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
        return;
}

/*
 * Probe for a Pegasus chip.
 */
static int
cue_match(device_t self)
{
	USB_MATCH_START(cue, uaa);
	struct cue_type			*t;

	if (!uaa->iface)
		return(UMATCH_NONE);

	t = cue_devs;
	while(t->cue_vid) {
		if (uaa->vendor == t->cue_vid &&
		    uaa->product == t->cue_did) {
			return(UMATCH_VENDOR_PRODUCT);
		}
		t++;
	}

	return(UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
cue_attach(device_t self)
{
	USB_ATTACH_START(cue, sc, uaa);
	u_char			eaddr[ETHER_ADDR_LEN];
	struct ifnet		*ifp;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

	sc->cue_dev = self;
	sc->cue_iface = uaa->iface;
	sc->cue_udev = uaa->device;

	if (usbd_set_config_no(sc->cue_udev, CUE_CONFIG_NO, 0)) {
		device_printf(sc->cue_dev, "getting interface handle failed\n");
		return ENXIO;
	}

	id = usbd_get_interface_descriptor(uaa->iface);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(uaa->iface, i);
		if (!ed) {
			device_printf(sc->cue_dev, "couldn't get ep %d\n", i);
			return ENXIO;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->cue_ed[CUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->cue_ed[CUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->cue_ed[CUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	mtx_init(&sc->cue_mtx, device_get_nameunit(self), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	CUE_LOCK(sc);

#ifdef notdef
	/* Reset the adapter. */
	cue_reset(sc);
#endif
	/*
	 * Get station address.
	 */
	cue_getmac(sc, &eaddr);

	ifp = sc->cue_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(sc->cue_dev, "can not if_alloc()\n");
		CUE_UNLOCK(sc);
		mtx_destroy(&sc->cue_mtx);
		return ENXIO;
	}
	ifp->if_softc = sc;
	if_initname(ifp, "cue", device_get_unit(sc->cue_dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT;
	ifp->if_ioctl = cue_ioctl;
	ifp->if_start = cue_start;
	ifp->if_watchdog = cue_watchdog;
	ifp->if_init = cue_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	sc->cue_qdat.ifp = ifp;
	sc->cue_qdat.if_rxstart = cue_rxstart;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);
	callout_handle_init(&sc->cue_stat_ch);
	usb_register_netisr();
	sc->cue_dying = 0;

	CUE_UNLOCK(sc);
	return 0;
}

static int
cue_detach(device_t dev)
{
	struct cue_softc	*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	CUE_LOCK(sc);
	ifp = sc->cue_ifp;

	sc->cue_dying = 1;
	untimeout(cue_tick, sc, sc->cue_stat_ch);
	ether_ifdetach(ifp);
	if_free(ifp);

	if (sc->cue_ep[CUE_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_TX]);
	if (sc->cue_ep[CUE_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_RX]);
	if (sc->cue_ep[CUE_ENDPT_INTR] != NULL)
		usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_INTR]);

	CUE_UNLOCK(sc);
	mtx_destroy(&sc->cue_mtx);

	return(0);
}

static void
cue_rxstart(struct ifnet *ifp)
{
	struct cue_softc	*sc;
	struct ue_chain	*c;

	sc = ifp->if_softc;
	CUE_LOCK(sc);
	c = &sc->cue_cdata.ue_rx_chain[sc->cue_cdata.ue_rx_prod];

	c->ue_mbuf = usb_ether_newbuf();
	if (c->ue_mbuf == NULL) {
		device_printf(sc->cue_dev, "no memory for rx list "
		    "-- packet dropped!\n");
		ifp->if_ierrors++;
		CUE_UNLOCK(sc);
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->ue_xfer, sc->cue_ep[CUE_ENDPT_RX],
	    c, mtod(c->ue_mbuf, char *), UE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, cue_rxeof);
	usbd_transfer(c->ue_xfer);
	CUE_UNLOCK(sc);

	return;
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
cue_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct cue_softc	*sc;
	struct ue_chain	*c;
        struct mbuf		*m;
        struct ifnet		*ifp;
	int			total_len = 0;
	u_int16_t		len;

	c = priv;
	sc = c->ue_sc;
	CUE_LOCK(sc);
	ifp = sc->cue_ifp;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		CUE_UNLOCK(sc);
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			CUE_UNLOCK(sc);
			return;
		}
		if (usbd_ratecheck(&sc->cue_rx_notice))
			device_printf(sc->cue_dev, "usb error on rx: %s\n",
			    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->cue_ep[CUE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	m = c->ue_mbuf;
	len = *mtod(m, u_int16_t *);

	/* No errors; receive the packet. */
	total_len = len;

	if (len < sizeof(struct ether_header)) {
		ifp->if_ierrors++;
		goto done;
	}

	ifp->if_ipackets++;
	m_adj(m, sizeof(u_int16_t));
	m->m_pkthdr.rcvif = (void *)&sc->cue_qdat;
	m->m_pkthdr.len = m->m_len = total_len;

	/* Put the packet on the special USB input queue. */
	usb_ether_input(m);
	CUE_UNLOCK(sc);

	return;
done:
	/* Setup new transfer. */
	usbd_setup_xfer(c->ue_xfer, sc->cue_ep[CUE_ENDPT_RX],
	    c, mtod(c->ue_mbuf, char *), UE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, cue_rxeof);
	usbd_transfer(c->ue_xfer);
	CUE_UNLOCK(sc);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void
cue_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct cue_softc	*sc;
	struct ue_chain	*c;
	struct ifnet		*ifp;
	usbd_status		err;

	c = priv;
	sc = c->ue_sc;
	CUE_LOCK(sc);
	ifp = sc->cue_ifp;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			CUE_UNLOCK(sc);
			return;
		}
		device_printf(sc->cue_dev, "usb error on tx: %s\n",
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->cue_ep[CUE_ENDPT_TX]);
		CUE_UNLOCK(sc);
		return;
	}

	ifp->if_timer = 0;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	usbd_get_xfer_status(c->ue_xfer, NULL, NULL, NULL, &err);

	if (c->ue_mbuf != NULL) {
		c->ue_mbuf->m_pkthdr.rcvif = ifp;
		usb_tx_done(c->ue_mbuf);
		c->ue_mbuf = NULL;
	}

	if (err)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	CUE_UNLOCK(sc);

	return;
}

static void
cue_tick(void *xsc)
{
	struct cue_softc	*sc;
	struct ifnet		*ifp;

	sc = xsc;

	if (sc == NULL)
		return;

	CUE_LOCK(sc);

	ifp = sc->cue_ifp;

	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_SINGLECOLL);
	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_MULTICOLL);
	ifp->if_collisions += cue_csr_read_2(sc, CUE_TX_EXCESSCOLL);

	if (cue_csr_read_2(sc, CUE_RX_FRAMEERR))
		ifp->if_ierrors++;

	sc->cue_stat_ch = timeout(cue_tick, sc, hz);

	CUE_UNLOCK(sc);

	return;
}

static int
cue_encap(struct cue_softc *sc, struct mbuf *m, int idx)
{
	int			total_len;
	struct ue_chain	*c;
	usbd_status		err;

	c = &sc->cue_cdata.ue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->ue_buf + 2);
	c->ue_mbuf = m;

	total_len = m->m_pkthdr.len + 2;

	/* The first two bytes are the frame length */
	c->ue_buf[0] = (u_int8_t)m->m_pkthdr.len;
	c->ue_buf[1] = (u_int8_t)(m->m_pkthdr.len >> 8);

	usbd_setup_xfer(c->ue_xfer, sc->cue_ep[CUE_ENDPT_TX],
	    c, c->ue_buf, total_len, 0, 10000, cue_txeof);

	/* Transmit */
	err = usbd_transfer(c->ue_xfer);
	if (err != USBD_IN_PROGRESS) {
		cue_stop(sc);
		return(EIO);
	}

	sc->cue_cdata.ue_tx_cnt++;

	return(0);
}

static void
cue_start(struct ifnet *ifp)
{
	struct cue_softc	*sc;
	struct mbuf		*m_head = NULL;

	sc = ifp->if_softc;
	CUE_LOCK(sc);

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE) {
		CUE_UNLOCK(sc);
		return;
	}

	IF_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL) {
		CUE_UNLOCK(sc);
		return;
	}

	if (cue_encap(sc, m_head, 0)) {
		IF_PREPEND(&ifp->if_snd, m_head);
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		CUE_UNLOCK(sc);
		return;
	}

	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
	BPF_MTAP(ifp, m_head);

	ifp->if_drv_flags |= IFF_DRV_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
	CUE_UNLOCK(sc);

	return;
}

static void
cue_init(void *xsc)
{
	struct cue_softc	*sc = xsc;
	struct ifnet		*ifp = sc->cue_ifp;
	struct ue_chain	*c;
	usbd_status		err;
	int			i;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	CUE_LOCK(sc);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
#ifdef foo
	cue_reset(sc);
#endif

	/* Set MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		cue_csr_write_1(sc, CUE_PAR0 - i, IF_LLADDR(sc->cue_ifp)[i]);

	/* Enable RX logic. */
	cue_csr_write_1(sc, CUE_ETHCTL, CUE_ETHCTL_RX_ON|CUE_ETHCTL_MCAST_ON);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		CUE_SETBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
	} else {
		CUE_CLRBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
	}

	/* Init TX ring. */
	if (usb_ether_tx_list_init(sc, &sc->cue_cdata,
	    sc->cue_udev) == ENOBUFS) {
		device_printf(sc->cue_dev, "tx list init failed\n");
		CUE_UNLOCK(sc);
		return;
	}

	/* Init RX ring. */
	if (usb_ether_rx_list_init(sc, &sc->cue_cdata,
	    sc->cue_udev) == ENOBUFS) {
		device_printf(sc->cue_dev, "rx list init failed\n");
		CUE_UNLOCK(sc);
		return;
	}

	/* Load the multicast filter. */
	cue_setmulti(sc);

	/*
	 * Set the number of RX and TX buffers that we want
	 * to reserve inside the ASIC.
	 */
	cue_csr_write_1(sc, CUE_RX_BUFPKTS, CUE_RX_FRAMES);
	cue_csr_write_1(sc, CUE_TX_BUFPKTS, CUE_TX_FRAMES);

	/* Set advanced operation modes. */
	cue_csr_write_1(sc, CUE_ADVANCED_OPMODES,
	    CUE_AOP_EMBED_RXLEN|0x01); /* 1 wait state */

	/* Program the LED operation. */
	cue_csr_write_1(sc, CUE_LEDCTL, CUE_LEDCTL_FOLLOW_LINK);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->cue_iface, sc->cue_ed[CUE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->cue_ep[CUE_ENDPT_RX]);
	if (err) {
		device_printf(sc->cue_dev, "open rx pipe failed: %s\n",
		    usbd_errstr(err));
		CUE_UNLOCK(sc);
		return;
	}
	err = usbd_open_pipe(sc->cue_iface, sc->cue_ed[CUE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->cue_ep[CUE_ENDPT_TX]);
	if (err) {
		device_printf(sc->cue_dev, "open tx pipe failed: %s\n",
		    usbd_errstr(err));
		CUE_UNLOCK(sc);
		return;
	}

	/* Start up the receive pipe. */
	for (i = 0; i < UE_RX_LIST_CNT; i++) {
		c = &sc->cue_cdata.ue_rx_chain[i];
		usbd_setup_xfer(c->ue_xfer, sc->cue_ep[CUE_ENDPT_RX],
		    c, mtod(c->ue_mbuf, char *), UE_BUFSZ,
		    USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, cue_rxeof);
		usbd_transfer(c->ue_xfer);
	}

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	CUE_UNLOCK(sc);

	sc->cue_stat_ch = timeout(cue_tick, sc, hz);

	return;
}

static int
cue_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct cue_softc	*sc = ifp->if_softc;
	int			error = 0;

	CUE_LOCK(sc);

	switch(command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->cue_if_flags & IFF_PROMISC)) {
				CUE_SETBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
				cue_setmulti(sc);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->cue_if_flags & IFF_PROMISC) {
				CUE_CLRBIT(sc, CUE_ETHCTL, CUE_ETHCTL_PROMISC);
				cue_setmulti(sc);
			} else if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				cue_init(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				cue_stop(sc);
		}
		sc->cue_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		cue_setmulti(sc);
		error = 0;
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	CUE_UNLOCK(sc);

	return(error);
}

static void
cue_watchdog(struct ifnet *ifp)
{
	struct cue_softc	*sc;
	struct ue_chain	*c;
	usbd_status		stat;

	sc = ifp->if_softc;
	CUE_LOCK(sc);

	ifp->if_oerrors++;
	device_printf(sc->cue_dev, "watchdog timeout\n");

	c = &sc->cue_cdata.ue_tx_chain[0];
	usbd_get_xfer_status(c->ue_xfer, NULL, NULL, NULL, &stat);
	cue_txeof(c->ue_xfer, c, stat);

	if (ifp->if_snd.ifq_head != NULL)
		cue_start(ifp);
	CUE_UNLOCK(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
cue_stop(struct cue_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;

	CUE_LOCK(sc);

	ifp = sc->cue_ifp;
	ifp->if_timer = 0;

	cue_csr_write_1(sc, CUE_ETHCTL, 0);
	cue_reset(sc);
	untimeout(cue_tick, sc, sc->cue_stat_ch);

	/* Stop transfers. */
	if (sc->cue_ep[CUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_RX]);
		if (err) {
			device_printf(sc->cue_dev, "abort rx pipe failed: %s\n",
		    	usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->cue_ep[CUE_ENDPT_RX]);
		if (err) {
			device_printf(sc->cue_dev, "close rx pipe failed: %s\n",
		    	usbd_errstr(err));
		}
		sc->cue_ep[CUE_ENDPT_RX] = NULL;
	}

	if (sc->cue_ep[CUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_TX]);
		if (err) {
			device_printf(sc->cue_dev, "abort tx pipe failed: %s\n",
		    	usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->cue_ep[CUE_ENDPT_TX]);
		if (err) {
			device_printf(sc->cue_dev, "close tx pipe failed: %s\n",
			    usbd_errstr(err));
		}
		sc->cue_ep[CUE_ENDPT_TX] = NULL;
	}

	if (sc->cue_ep[CUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->cue_ep[CUE_ENDPT_INTR]);
		if (err) {
			device_printf(sc->cue_dev, "abort intr pipe failed: %s\n",
		    	usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->cue_ep[CUE_ENDPT_INTR]);
		if (err) {
			device_printf(sc->cue_dev, "close intr pipe failed: %s\n",
			    usbd_errstr(err));
		}
		sc->cue_ep[CUE_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	usb_ether_rx_list_free(&sc->cue_cdata);
	/* Free TX resources. */
	usb_ether_tx_list_free(&sc->cue_cdata);

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	CUE_UNLOCK(sc);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
cue_shutdown(device_t dev)
{
	struct cue_softc	*sc;

	sc = device_get_softc(dev);

	CUE_LOCK(sc);
	cue_reset(sc);
	cue_stop(sc);
	CUE_UNLOCK(sc);

	return;
}
