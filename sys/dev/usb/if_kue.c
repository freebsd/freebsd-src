/*
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
 *
 * $FreeBSD$
 */

/*
 * Kawasaki LSI KL5KUSB101B USB to ethernet adapter driver.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The KLSI USB to ethernet adapter chip contains an USB serial interface,
 * ethernet MAC and embedded microcontroller (called the QT Engine).
 * The chip must have firmware loaded into it before it will operate.
 * Packets are passed between the chip and host via bulk transfers.
 * There is an interrupt endpoint mentioned in the software spec, however
 * it's currently unused. This device is 10Mbps half-duplex only, hence
 * there is no media selection logic. The MAC supports a 128 entry
 * multicast filter, though the exact size of the filter can depend
 * on the firmware. Curiously, while the software spec describes various
 * ethernet statistics counters, my sample adapter and firmware combination
 * claims not to support any statistics counters at all.
 *
 * Note that once we load the firmware in the device, we have to be
 * careful not to load it again: if you restart your computer but
 * leave the adapter attached to the USB controller, it may remain
 * powered on and retain its firmware. In this case, we don't need
 * to load the firmware a second time.
 *
 * Special thanks to Rob Furr for providing an ADS Technologies
 * adapter for development and testing. No monkeys were harmed during
 * the development of this driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_ethersubr.h>

#include <dev/usb/if_kuereg.h>
#include <dev/usb/kue_fw.h>

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

MODULE_DEPEND(if_kue, usb, 1, 1, 1);

/*
 * Various supported device vendors/products.
 */
Static struct kue_type kue_devs[] = {
	{ USB_VENDOR_AOX, USB_PRODUCT_AOX_USB101 },
	{ USB_VENDOR_KLSI, USB_PRODUCT_AOX_USB101 },
	{ USB_VENDOR_ADS, USB_PRODUCT_ADS_UBS10BT },
	{ USB_VENDOR_ATEN, USB_PRODUCT_ATEN_UC10T },
	{ USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_EA101 },
	{ USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_ENET },
	{ USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_ENET2 },
	{ USB_VENDOR_ENTREGA, USB_PRODUCT_ENTREGA_E45 },
	{ USB_VENDOR_3COM, USB_PRODUCT_3COM_3C19250 },
	{ USB_VENDOR_COREGA, USB_PRODUCT_COREGA_ETHER_USB_T },
	{ USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650C },
	{ USB_VENDOR_SMC, USB_PRODUCT_SMC_2102USB },
	{ USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB10T },
	{ USB_VENDOR_KLSI, USB_PRODUCT_KLSI_DUH3E10BT },
	{ USB_VENDOR_PERACOM, USB_PRODUCT_PERACOM_ENET3 },
	{ USB_VENDOR_IODATA, USB_PRODUCT_IODATA_USBETT },
	{ USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_URE450 },
	{ 0, 0 }
};

Static struct usb_qdat kue_qdat;

Static int kue_match(device_ptr_t);
Static int kue_attach(device_ptr_t);
Static int kue_detach(device_ptr_t);
Static void kue_shutdown(device_ptr_t);
Static int kue_tx_list_init(struct kue_softc *);
Static int kue_rx_list_init(struct kue_softc *);
Static int kue_newbuf(struct kue_softc *, struct kue_chain *, struct mbuf *);
Static int kue_encap(struct kue_softc *, struct mbuf *, int);
Static void kue_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void kue_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void kue_start(struct ifnet *);
Static void kue_rxstart(struct ifnet *);
Static int kue_ioctl(struct ifnet *, u_long, caddr_t);
Static void kue_init(void *);
Static void kue_stop(struct kue_softc *);
Static void kue_watchdog(struct ifnet *);

Static void kue_setmulti(struct kue_softc *);
Static void kue_reset(struct kue_softc *);

Static usbd_status kue_do_request(usbd_device_handle,
				  usb_device_request_t *, void *);
Static usbd_status kue_ctl(struct kue_softc *, int, u_int8_t,
			   u_int16_t, char *, int);
Static usbd_status kue_setword(struct kue_softc *, u_int8_t, u_int16_t);
Static int kue_load_fw(struct kue_softc *);

Static device_method_t kue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		kue_match),
	DEVMETHOD(device_attach,	kue_attach),
	DEVMETHOD(device_detach,	kue_detach),
	DEVMETHOD(device_shutdown,	kue_shutdown),

	{ 0, 0 }
};

Static driver_t kue_driver = {
	"kue",
	kue_methods,
	sizeof(struct kue_softc)
};

Static devclass_t kue_devclass;

DRIVER_MODULE(if_kue, uhub, kue_driver, kue_devclass, usbd_driver_load, 0);

/*
 * We have a custom do_request function which is almost like the
 * regular do_request function, except it has a much longer timeout.
 * Why? Because we need to make requests over the control endpoint
 * to download the firmware to the device, which can take longer
 * than the default timeout.
 */
Static usbd_status
kue_do_request(usbd_device_handle dev, usb_device_request_t *req, void *data)
{
	usbd_xfer_handle	xfer;
	usbd_status		err;

	xfer = usbd_alloc_xfer(dev);
	usbd_setup_default_xfer(xfer, dev, 0, 500000, req,
	    data, UGETW(req->wLength), USBD_SHORT_XFER_OK|USBD_NO_TSLEEP, 0);
	err = usbd_sync_transfer(xfer);
	usbd_free_xfer(xfer);
	return(err);
}

Static usbd_status
kue_setword(struct kue_softc *sc, u_int8_t breq, u_int16_t word)
{
	usbd_device_handle	dev;
	usb_device_request_t	req;
	usbd_status		err;

	if (sc->kue_dying)
		return(USBD_NORMAL_COMPLETION);

	dev = sc->kue_udev;

	KUE_LOCK(sc);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;

	req.bRequest = breq;
	USETW(req.wValue, word);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	err = kue_do_request(dev, &req, NULL);

	KUE_UNLOCK(sc);

	return(err);
}

Static usbd_status
kue_ctl(struct kue_softc *sc, int rw, u_int8_t breq, u_int16_t val,
	char *data, int len)
{
	usbd_device_handle	dev;
	usb_device_request_t	req;
	usbd_status		err;

	dev = sc->kue_udev;

	if (sc->kue_dying)
		return(USBD_NORMAL_COMPLETION);

	KUE_LOCK(sc);

	if (rw == KUE_CTL_WRITE)
		req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	else
		req.bmRequestType = UT_READ_VENDOR_DEVICE;

	req.bRequest = breq;
	USETW(req.wValue, val);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);

	err = kue_do_request(dev, &req, data);

	KUE_UNLOCK(sc);

	return(err);
}

Static int
kue_load_fw(struct kue_softc *sc)
{
	usbd_status		err;
	usb_device_descriptor_t	*dd;
	int			hwrev;

	dd = &sc->kue_udev->ddesc;
	hwrev = UGETW(dd->bcdDevice);

	/*
	 * First, check if we even need to load the firmware.
	 * If the device was still attached when the system was
	 * rebooted, it may already have firmware loaded in it.
	 * If this is the case, we don't need to do it again.
	 * And in fact, if we try to load it again, we'll hang,
	 * so we have to avoid this condition if we don't want
	 * to look stupid.
	 *
	 * We can test this quickly by checking the bcdRevision
	 * code. The NIC will return a different revision code if
	 * it's probed while the firmware is still loaded and
	 * running.
	 */
	if (hwrev == 0x0202)
		return(0);

	/* Load code segment */
	err = kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SEND_SCAN,
	    0, kue_code_seg, sizeof(kue_code_seg));
	if (err) {
		printf("kue%d: failed to load code segment: %s\n",
		    sc->kue_unit, usbd_errstr(err));
			return(ENXIO);
	}

	/* Load fixup segment */
	err = kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SEND_SCAN,
	    0, kue_fix_seg, sizeof(kue_fix_seg));
	if (err) {
		printf("kue%d: failed to load fixup segment: %s\n",
		    sc->kue_unit, usbd_errstr(err));
			return(ENXIO);
	}

	/* Send trigger command. */
	err = kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SEND_SCAN,
	    0, kue_trig_seg, sizeof(kue_trig_seg));
	if (err) {
		printf("kue%d: failed to load trigger segment: %s\n",
		    sc->kue_unit, usbd_errstr(err));
			return(ENXIO);
	}

	return(0);
}

Static void
kue_setmulti(struct kue_softc *sc)
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	int			i = 0;

	ifp = &sc->arpcom.ac_if;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		sc->kue_rxfilt |= KUE_RXFILT_ALLMULTI;
		sc->kue_rxfilt &= ~KUE_RXFILT_MULTICAST;
		kue_setword(sc, KUE_CMD_SET_PKT_FILTER, sc->kue_rxfilt);
		return;
	}

	sc->kue_rxfilt &= ~KUE_RXFILT_ALLMULTI;

	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		/*
		 * If there are too many addresses for the
		 * internal filter, switch over to allmulti mode.
		 */
		if (i == KUE_MCFILTCNT(sc))
			break;
		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    KUE_MCFILT(sc, i), ETHER_ADDR_LEN);
		i++;
	}

	if (i == KUE_MCFILTCNT(sc))
		sc->kue_rxfilt |= KUE_RXFILT_ALLMULTI;
	else {
		sc->kue_rxfilt |= KUE_RXFILT_MULTICAST;
		kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SET_MCAST_FILTERS,
		    i, sc->kue_mcfilters, i * ETHER_ADDR_LEN);
	}

	kue_setword(sc, KUE_CMD_SET_PKT_FILTER, sc->kue_rxfilt);

	return;
}

/*
 * Issue a SET_CONFIGURATION command to reset the MAC. This should be
 * done after the firmware is loaded into the adapter in order to
 * bring it into proper operation.
 */
Static void
kue_reset(struct kue_softc *sc)
{
	if (usbd_set_config_no(sc->kue_udev, KUE_CONFIG_NO, 0)) {
		printf("kue%d: getting interface handle failed\n",
		    sc->kue_unit);
	}

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
        return;
}

/*
 * Probe for a KLSI chip.
 */
USB_MATCH(kue)
{
	USB_MATCH_START(kue, uaa);
	struct kue_type			*t;

	if (!uaa->iface)
		return(UMATCH_NONE);

	t = kue_devs;
	while(t->kue_vid) {
		if (uaa->vendor == t->kue_vid &&
		    uaa->product == t->kue_did) {
			return(UMATCH_VENDOR_PRODUCT);
		}
		t++;
	}

	return(UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(kue)
{
	USB_ATTACH_START(kue, sc, uaa);
	char			devinfo[1024];
	struct ifnet		*ifp;
	usbd_status		err;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

	bzero(sc, sizeof(struct kue_softc));
	sc->kue_iface = uaa->iface;
	sc->kue_udev = uaa->device;
	sc->kue_unit = device_get_unit(self);

	id = usbd_get_interface_descriptor(uaa->iface);

	usbd_devinfo(uaa->device, 0, devinfo);
	device_set_desc_copy(self, devinfo);
	printf("%s: %s\n", USBDEVNAME(self), devinfo);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(uaa->iface, i);
		if (!ed) {
			printf("kue%d: couldn't get ep %d\n",
			    sc->kue_unit, i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->kue_ed[KUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->kue_ed[KUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->kue_ed[KUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	mtx_init(&sc->kue_mtx, device_get_nameunit(self), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	KUE_LOCK(sc);

	/* Load the firmware into the NIC. */
	if (kue_load_fw(sc)) {
		KUE_UNLOCK(sc);
		mtx_destroy(&sc->kue_mtx);
		USB_ATTACH_ERROR_RETURN;
	}

	/* Reset the adapter. */
	kue_reset(sc);

	/* Read ethernet descriptor */
	err = kue_ctl(sc, KUE_CTL_READ, KUE_CMD_GET_ETHER_DESCRIPTOR,
	    0, (char *)&sc->kue_desc, sizeof(sc->kue_desc));

	sc->kue_mcfilters = malloc(KUE_MCFILTCNT(sc) * ETHER_ADDR_LEN,
	    M_USBDEV, M_NOWAIT);

	/*
	 * A KLSI chip was detected. Inform the world.
	 */
	printf("kue%d: Ethernet address: %6D\n", sc->kue_unit,
	    sc->kue_desc.kue_macaddr, ":");

	bcopy(sc->kue_desc.kue_macaddr,
	    (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = sc->kue_unit;
	ifp->if_name = "kue";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = kue_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = kue_start;
	ifp->if_watchdog = kue_watchdog;
	ifp->if_init = kue_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	kue_qdat.ifp = ifp;
	kue_qdat.if_rxstart = kue_rxstart;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	usb_register_netisr();
	sc->kue_dying = 0;

	KUE_UNLOCK(sc);

	USB_ATTACH_SUCCESS_RETURN;
}

Static int
kue_detach(device_ptr_t dev)
{
	struct kue_softc	*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	KUE_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	sc->kue_dying = 1;

	if (ifp != NULL)
		ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);

	if (sc->kue_ep[KUE_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->kue_ep[KUE_ENDPT_TX]);
	if (sc->kue_ep[KUE_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->kue_ep[KUE_ENDPT_RX]);
	if (sc->kue_ep[KUE_ENDPT_INTR] != NULL)
		usbd_abort_pipe(sc->kue_ep[KUE_ENDPT_INTR]);

	if (sc->kue_mcfilters != NULL)
		free(sc->kue_mcfilters, M_USBDEV);

	KUE_UNLOCK(sc);
	mtx_destroy(&sc->kue_mtx);

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
Static int
kue_newbuf(struct kue_softc *sc, struct kue_chain *c, struct mbuf *m)
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("kue%d: no memory for rx list "
			    "-- packet dropped!\n", sc->kue_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("kue%d: no memory for rx list "
			    "-- packet dropped!\n", sc->kue_unit);
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	c->kue_mbuf = m_new;

	return(0);
}

Static int
kue_rx_list_init(struct kue_softc *sc)
{
	struct kue_cdata	*cd;
	struct kue_chain	*c;
	int			i;

	cd = &sc->kue_cdata;
	for (i = 0; i < KUE_RX_LIST_CNT; i++) {
		c = &cd->kue_rx_chain[i];
		c->kue_sc = sc;
		c->kue_idx = i;
		if (kue_newbuf(sc, c, NULL) == ENOBUFS)
			return(ENOBUFS);
		if (c->kue_xfer == NULL) {
			c->kue_xfer = usbd_alloc_xfer(sc->kue_udev);
			if (c->kue_xfer == NULL)
				return(ENOBUFS);
		}
	}

	return(0);
}

Static int
kue_tx_list_init(struct kue_softc *sc)
{
	struct kue_cdata	*cd;
	struct kue_chain	*c;
	int			i;

	cd = &sc->kue_cdata;
	for (i = 0; i < KUE_TX_LIST_CNT; i++) {
		c = &cd->kue_tx_chain[i];
		c->kue_sc = sc;
		c->kue_idx = i;
		c->kue_mbuf = NULL;
		if (c->kue_xfer == NULL) {
			c->kue_xfer = usbd_alloc_xfer(sc->kue_udev);
			if (c->kue_xfer == NULL)
				return(ENOBUFS);
		}
		c->kue_buf = malloc(KUE_BUFSZ, M_USBDEV, M_NOWAIT);
		if (c->kue_buf == NULL)
			return(ENOBUFS);
	}

	return(0);
}

Static void
kue_rxstart(struct ifnet *ifp)
{
	struct kue_softc	*sc;
	struct kue_chain	*c;

	sc = ifp->if_softc;
	KUE_LOCK(sc);
	c = &sc->kue_cdata.kue_rx_chain[sc->kue_cdata.kue_rx_prod];

	if (kue_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->kue_xfer, sc->kue_ep[KUE_ENDPT_RX],
	    c, mtod(c->kue_mbuf, char *), KUE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, kue_rxeof);
	usbd_transfer(c->kue_xfer);

	KUE_UNLOCK(sc);

	return;
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
Static void kue_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv,
		      usbd_status status)
{
	struct kue_softc	*sc;
	struct kue_chain	*c;
        struct mbuf		*m;
        struct ifnet		*ifp;
	int			total_len = 0;
	u_int16_t		len;

	c = priv;
	sc = c->kue_sc;
	KUE_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_RUNNING)) {
		KUE_UNLOCK(sc);
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			KUE_UNLOCK(sc);
			return;
		}
		if (usbd_ratecheck(&sc->kue_rx_notice))
			printf("kue%d: usb error on rx: %s\n", sc->kue_unit,
			    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->kue_ep[KUE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);
	m = c->kue_mbuf;
	if (total_len <= 1)
		goto done;

	len = *mtod(m, u_int16_t *);
	m_adj(m, sizeof(u_int16_t));

	/* No errors; receive the packet. */
	total_len = len;

	if (len < sizeof(struct ether_header)) {
		ifp->if_ierrors++;
		goto done;
	}

	ifp->if_ipackets++;
	m->m_pkthdr.rcvif = (struct ifnet *)&kue_qdat;
	m->m_pkthdr.len = m->m_len = total_len;

	/* Put the packet on the special USB input queue. */
	usb_ether_input(m);
	KUE_UNLOCK(sc);

	return;
done:

	/* Setup new transfer. */
	usbd_setup_xfer(c->kue_xfer, sc->kue_ep[KUE_ENDPT_RX],
	    c, mtod(c->kue_mbuf, char *), KUE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, kue_rxeof);
	usbd_transfer(c->kue_xfer);
	KUE_UNLOCK(sc);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

Static void
kue_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct kue_softc	*sc;
	struct kue_chain	*c;
	struct ifnet		*ifp;
	usbd_status		err;

	c = priv;
	sc = c->kue_sc;
	KUE_LOCK(sc);

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			KUE_UNLOCK(sc);
			return;
		}
		printf("kue%d: usb error on tx: %s\n", sc->kue_unit,
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->kue_ep[KUE_ENDPT_TX]);
		KUE_UNLOCK(sc);
		return;
	}

	usbd_get_xfer_status(c->kue_xfer, NULL, NULL, NULL, &err);

	if (c->kue_mbuf != NULL) {
		c->kue_mbuf->m_pkthdr.rcvif = ifp;
		usb_tx_done(c->kue_mbuf);
		c->kue_mbuf = NULL;
	}

	if (err)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	KUE_UNLOCK(sc);

	return;
}

Static int
kue_encap(struct kue_softc *sc, struct mbuf *m, int idx)
{
	int			total_len;
	struct kue_chain	*c;
	usbd_status		err;

	c = &sc->kue_cdata.kue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->kue_buf + 2);
	c->kue_mbuf = m;

	total_len = m->m_pkthdr.len + 2;
	total_len += 64 - (total_len % 64);

	/* Frame length is specified in the first 2 bytes of the buffer. */
	c->kue_buf[0] = (u_int8_t)m->m_pkthdr.len;
	c->kue_buf[1] = (u_int8_t)(m->m_pkthdr.len >> 8);

	usbd_setup_xfer(c->kue_xfer, sc->kue_ep[KUE_ENDPT_TX],
	    c, c->kue_buf, total_len, 0, 10000, kue_txeof);

	/* Transmit */
	err = usbd_transfer(c->kue_xfer);
	if (err != USBD_IN_PROGRESS) {
		kue_stop(sc);
		return(EIO);
	}

	sc->kue_cdata.kue_tx_cnt++;

	return(0);
}

Static void
kue_start(struct ifnet *ifp)
{
	struct kue_softc	*sc;
	struct mbuf		*m_head = NULL;

	sc = ifp->if_softc;
	KUE_LOCK(sc);

	if (ifp->if_flags & IFF_OACTIVE) {
		KUE_UNLOCK(sc);
		return;
	}

	IF_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL) {
		KUE_UNLOCK(sc);
		return;
	}

	if (kue_encap(sc, m_head, 0)) {
		IF_PREPEND(&ifp->if_snd, m_head);
		ifp->if_flags |= IFF_OACTIVE;
		KUE_UNLOCK(sc);
		return;
	}

	/*
	 * If there's a BPF listener, bounce a copy of this frame
	 * to him.
	 */
	if (ifp->if_bpf)
		bpf_mtap(ifp, m_head);

	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
	KUE_UNLOCK(sc);

	return;
}

Static void
kue_init(void *xsc)
{
	struct kue_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct kue_chain	*c;
	usbd_status		err;
	int			i;

	KUE_LOCK(sc);

	if (ifp->if_flags & IFF_RUNNING) {
		KUE_UNLOCK(sc);
		return;
	}

	/* Set MAC address */
	kue_ctl(sc, KUE_CTL_WRITE, KUE_CMD_SET_MAC,
	    0, sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->kue_rxfilt = KUE_RXFILT_UNICAST|KUE_RXFILT_BROADCAST;

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		sc->kue_rxfilt |= KUE_RXFILT_PROMISC;

	kue_setword(sc, KUE_CMD_SET_PKT_FILTER, sc->kue_rxfilt);

	/* I'm not sure how to tune these. */
#ifdef notdef
	/*
	 * Leave this one alone for now; setting it
	 * wrong causes lockups on some machines/controllers.
	 */
	kue_setword(sc, KUE_CMD_SET_SOFS, 1);
#endif
	kue_setword(sc, KUE_CMD_SET_URB_SIZE, 64);

	/* Init TX ring. */
	if (kue_tx_list_init(sc) == ENOBUFS) {
		printf("kue%d: tx list init failed\n", sc->kue_unit);
		KUE_UNLOCK(sc);
		return;
	}

	/* Init RX ring. */
	if (kue_rx_list_init(sc) == ENOBUFS) {
		printf("kue%d: rx list init failed\n", sc->kue_unit);
		KUE_UNLOCK(sc);
		return;
	}

	/* Load the multicast filter. */
	kue_setmulti(sc);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->kue_iface, sc->kue_ed[KUE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->kue_ep[KUE_ENDPT_RX]);
	if (err) {
		printf("kue%d: open rx pipe failed: %s\n",
		    sc->kue_unit, usbd_errstr(err));
		KUE_UNLOCK(sc);
		return;
	}

	err = usbd_open_pipe(sc->kue_iface, sc->kue_ed[KUE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->kue_ep[KUE_ENDPT_TX]);
	if (err) {
		printf("kue%d: open tx pipe failed: %s\n",
		    sc->kue_unit, usbd_errstr(err));
		KUE_UNLOCK(sc);
		return;
	}

	/* Start up the receive pipe. */
	for (i = 0; i < KUE_RX_LIST_CNT; i++) {
		c = &sc->kue_cdata.kue_rx_chain[i];
		usbd_setup_xfer(c->kue_xfer, sc->kue_ep[KUE_ENDPT_RX],
		    c, mtod(c->kue_mbuf, char *), KUE_BUFSZ,
	    	USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, kue_rxeof);
		usbd_transfer(c->kue_xfer);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	KUE_UNLOCK(sc);

	return;
}

Static int
kue_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct kue_softc	*sc = ifp->if_softc;
	int			error = 0;

	KUE_LOCK(sc);

	switch(command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->kue_if_flags & IFF_PROMISC)) {
				sc->kue_rxfilt |= KUE_RXFILT_PROMISC;
				kue_setword(sc, KUE_CMD_SET_PKT_FILTER,
				    sc->kue_rxfilt);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->kue_if_flags & IFF_PROMISC) {
				sc->kue_rxfilt &= ~KUE_RXFILT_PROMISC;
				kue_setword(sc, KUE_CMD_SET_PKT_FILTER,
				    sc->kue_rxfilt);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				kue_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				kue_stop(sc);
		}
		sc->kue_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		kue_setmulti(sc);
		error = 0;
		break;
	default:
		error = EINVAL;
		break;
	}

	KUE_UNLOCK(sc);

	return(error);
}

Static void
kue_watchdog(struct ifnet *ifp)
{
	struct kue_softc	*sc;
	struct kue_chain	*c;
	usbd_status		stat;

	sc = ifp->if_softc;
	KUE_LOCK(sc);
	ifp->if_oerrors++;
	printf("kue%d: watchdog timeout\n", sc->kue_unit);

	c = &sc->kue_cdata.kue_tx_chain[0];
	usbd_get_xfer_status(c->kue_xfer, NULL, NULL, NULL, &stat);
	kue_txeof(c->kue_xfer, c, stat);

	if (ifp->if_snd.ifq_head != NULL)
		kue_start(ifp);
	KUE_UNLOCK(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
kue_stop(struct kue_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	KUE_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	/* Stop transfers. */
	if (sc->kue_ep[KUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->kue_ep[KUE_ENDPT_RX]);
		if (err) {
			printf("kue%d: abort rx pipe failed: %s\n",
		    	sc->kue_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->kue_ep[KUE_ENDPT_RX]);
		if (err) {
			printf("kue%d: close rx pipe failed: %s\n",
		    	sc->kue_unit, usbd_errstr(err));
		}
		sc->kue_ep[KUE_ENDPT_RX] = NULL;
	}

	if (sc->kue_ep[KUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->kue_ep[KUE_ENDPT_TX]);
		if (err) {
			printf("kue%d: abort tx pipe failed: %s\n",
		    	sc->kue_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->kue_ep[KUE_ENDPT_TX]);
		if (err) {
			printf("kue%d: close tx pipe failed: %s\n",
			    sc->kue_unit, usbd_errstr(err));
		}
		sc->kue_ep[KUE_ENDPT_TX] = NULL;
	}

	if (sc->kue_ep[KUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->kue_ep[KUE_ENDPT_INTR]);
		if (err) {
			printf("kue%d: abort intr pipe failed: %s\n",
		    	sc->kue_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->kue_ep[KUE_ENDPT_INTR]);
		if (err) {
			printf("kue%d: close intr pipe failed: %s\n",
			    sc->kue_unit, usbd_errstr(err));
		}
		sc->kue_ep[KUE_ENDPT_INTR] = NULL;
	}

	/* Free RX resources. */
	for (i = 0; i < KUE_RX_LIST_CNT; i++) {
		if (sc->kue_cdata.kue_rx_chain[i].kue_buf != NULL) {
			free(sc->kue_cdata.kue_rx_chain[i].kue_buf, M_USBDEV);
			sc->kue_cdata.kue_rx_chain[i].kue_buf = NULL;
		}
		if (sc->kue_cdata.kue_rx_chain[i].kue_mbuf != NULL) {
			m_freem(sc->kue_cdata.kue_rx_chain[i].kue_mbuf);
			sc->kue_cdata.kue_rx_chain[i].kue_mbuf = NULL;
		}
		if (sc->kue_cdata.kue_rx_chain[i].kue_xfer != NULL) {
			usbd_free_xfer(sc->kue_cdata.kue_rx_chain[i].kue_xfer);
			sc->kue_cdata.kue_rx_chain[i].kue_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < KUE_TX_LIST_CNT; i++) {
		if (sc->kue_cdata.kue_tx_chain[i].kue_buf != NULL) {
			free(sc->kue_cdata.kue_tx_chain[i].kue_buf, M_USBDEV);
			sc->kue_cdata.kue_tx_chain[i].kue_buf = NULL;
		}
		if (sc->kue_cdata.kue_tx_chain[i].kue_mbuf != NULL) {
			m_freem(sc->kue_cdata.kue_tx_chain[i].kue_mbuf);
			sc->kue_cdata.kue_tx_chain[i].kue_mbuf = NULL;
		}
		if (sc->kue_cdata.kue_tx_chain[i].kue_xfer != NULL) {
			usbd_free_xfer(sc->kue_cdata.kue_tx_chain[i].kue_xfer);
			sc->kue_cdata.kue_tx_chain[i].kue_xfer = NULL;
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	KUE_UNLOCK(sc);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
Static void
kue_shutdown(device_ptr_t dev)
{
	struct kue_softc	*sc;

	sc = device_get_softc(dev);

	kue_stop(sc);

	return;
}
