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
 * ADMtek AN986 Pegasus USB to ethernet driver. Datasheet is available
 * from http://www.admtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Pegasus chip uses four USB "endpoints" to provide 10/100 ethernet
 * support: the control endpoint for reading/writing registers, burst
 * read endpoint for packet reception, burst write for packet transmission
 * and one for "interrupts." The chip uses the same RX filter scheme
 * as the other ADMtek ethernet parts: one perfect filter entry for the
 * the station address and a 64-bit multicast hash table. The chip supports
 * both MII and HomePNA attachments.
 *
 * Since the maximum data transfer speed of USB is supposed to be 12Mbps,
 * you're never really going to get 100Mbps speeds from this device. I
 * think the idea is to allow the device to connect to 10 or 100Mbps
 * networks, not necessarily to provide 100Mbps performance. Also, since
 * the controller uses an external PHY chip, it's possible that board
 * designers might simply choose a 10Mbps PHY.
 *
 * Registers are accessed using usbd_do_request(). Packet transfers are
 * done using usbd_transfer() and friends.
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

#include <machine/clock.h>      /* for DELAY */
#include <sys/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_ethersubr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/if_auereg.h>

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

/*
 * Various supported device vendors/products.
 */
Static struct aue_type aue_devs[] = {
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_XX1,		  PNA|PII },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_XX2,		  PII },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_UFE1000,	  LSYS },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_XX4,		  PNA },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_XX5,		  PNA },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_XX6,		  PII },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_XX7,		  PII },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_XX8,		  PII },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_XX9,		  PNA },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_XX10,	  0 },
    { USB_VENDOR_ABOCOM,	USB_PRODUCT_ABOCOM_DSB650TX_PNA,  0 },
    { USB_VENDOR_ACCTON,	USB_PRODUCT_ACCTON_USB320_EC,	  0 },
    { USB_VENDOR_ACCTON,	USB_PRODUCT_ACCTON_SS1001,	  PII },
    { USB_VENDOR_ADMTEK,	USB_PRODUCT_ADMTEK_PEGASUS,	  PNA },
    { USB_VENDOR_ADMTEK,	USB_PRODUCT_ADMTEK_PEGASUSII,	  PII },
    { USB_VENDOR_BELKIN,	USB_PRODUCT_BELKIN_USB2LAN,       PII },
    { USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USB100,	  0 },
    { USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USBLP100,  PNA },
    { USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USBEL100,  0 },
    { USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USBE100,	  PII },
    { USB_VENDOR_COREGA,	USB_PRODUCT_COREGA_FETHER_USB_TX, 0 },
    { USB_VENDOR_COREGA,	USB_PRODUCT_COREGA_FETHER_USB_TXS,PII },
    { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX4,	  LSYS|PII },
    { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX1,	  LSYS },
    { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX,	  LSYS },
    { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX_PNA,	  PNA },
    { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX3,	  LSYS|PII },
    { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX2,	  LSYS|PII },
    { USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650,	  LSYS },
    { USB_VENDOR_ELECOM,	USB_PRODUCT_ELECOM_LDUSBTX0,	  0 },
    { USB_VENDOR_ELECOM,	USB_PRODUCT_ELECOM_LDUSBTX1,	  LSYS },
    { USB_VENDOR_ELECOM,	USB_PRODUCT_ELECOM_LDUSBTX2,	  0 },
    { USB_VENDOR_ELECOM,	USB_PRODUCT_ELECOM_LDUSBTX3,	  LSYS },
    { USB_VENDOR_ELECOM,	USB_PRODUCT_ELECOM_LDUSBLTX,	  PII },
    { USB_VENDOR_ELSA,		USB_PRODUCT_ELSA_USB2ETHERNET,	  0 },
    { USB_VENDOR_IODATA,	USB_PRODUCT_IODATA_USBETTX,	  0 },
    { USB_VENDOR_IODATA,	USB_PRODUCT_IODATA_USBETTXS,	  PII },
    { USB_VENDOR_KINGSTON,	USB_PRODUCT_KINGSTON_KNU101TX,    0 },
    { USB_VENDOR_LINKSYS,	USB_PRODUCT_LINKSYS_USB10TX1,	  LSYS|PII },
    { USB_VENDOR_LINKSYS,	USB_PRODUCT_LINKSYS_USB10T,	  LSYS },
    { USB_VENDOR_LINKSYS,	USB_PRODUCT_LINKSYS_USB100TX,	  LSYS },
    { USB_VENDOR_LINKSYS,	USB_PRODUCT_LINKSYS_USB100H1,	  LSYS|PNA },
    { USB_VENDOR_LINKSYS,	USB_PRODUCT_LINKSYS_USB10TA,	  LSYS },
    { USB_VENDOR_LINKSYS,	USB_PRODUCT_LINKSYS_USB10TX2,	  LSYS|PII },
    { USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_LUATX1,	  0 },
    { USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_LUATX5,	  0 },
    { USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_LUA2TX5,	  PII },
    { USB_VENDOR_SIEMENS,	USB_PRODUCT_SIEMENS_SPEEDSTREAM,  PII },
    { USB_VENDOR_SMARTBRIDGES,	USB_PRODUCT_SMARTBRIDGES_SMARTNIC,PII },
    { USB_VENDOR_SMC,		USB_PRODUCT_SMC_2202USB,	  0 },
    { USB_VENDOR_SMC,		USB_PRODUCT_SMC_2206USB,	  PII },
    { USB_VENDOR_SOHOWARE,	USB_PRODUCT_SOHOWARE_NUB100,	  0 },
    { 0, 0, 0 }
};

Static struct usb_qdat aue_qdat;

Static int aue_match(device_t);
Static int aue_attach(device_t);
Static int aue_detach(device_t);

Static void aue_reset_pegasus_II(struct aue_softc *);
Static int aue_tx_list_init(struct aue_softc *);
Static int aue_rx_list_init(struct aue_softc *);
Static int aue_newbuf(struct aue_softc *, struct aue_chain *, struct mbuf *);
Static int aue_encap(struct aue_softc *, struct mbuf *, int);
#ifdef AUE_INTR_PIPE
Static void aue_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
#endif
Static void aue_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void aue_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
Static void aue_tick(void *);
Static void aue_rxstart(struct ifnet *);
Static void aue_start(struct ifnet *);
Static int aue_ioctl(struct ifnet *, u_long, caddr_t);
Static void aue_init(void *);
Static void aue_stop(struct aue_softc *);
Static void aue_watchdog(struct ifnet *);
Static void aue_shutdown(device_t);
Static int aue_ifmedia_upd(struct ifnet *);
Static void aue_ifmedia_sts(struct ifnet *, struct ifmediareq *);

Static void aue_eeprom_getword(struct aue_softc *, int, u_int16_t *);
Static void aue_read_eeprom(struct aue_softc *, caddr_t, int, int, int);
Static int aue_miibus_readreg(device_t, int, int);
Static int aue_miibus_writereg(device_t, int, int, int);
Static void aue_miibus_statchg(device_t);

Static void aue_setmulti(struct aue_softc *);
Static u_int32_t aue_crc(caddr_t);
Static void aue_reset(struct aue_softc *);

Static int csr_read_1(struct aue_softc *, int);
Static int csr_write_1(struct aue_softc *, int, int);
Static int csr_read_2(struct aue_softc *, int);
Static int csr_write_2(struct aue_softc *, int, int);

Static device_method_t aue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aue_match),
	DEVMETHOD(device_attach,	aue_attach),
	DEVMETHOD(device_detach,	aue_detach),
	DEVMETHOD(device_shutdown,	aue_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	aue_miibus_readreg),
	DEVMETHOD(miibus_writereg,	aue_miibus_writereg),
	DEVMETHOD(miibus_statchg,	aue_miibus_statchg),

	{ 0, 0 }
};

Static driver_t aue_driver = {
	"aue",
	aue_methods,
	sizeof(struct aue_softc)
};

Static devclass_t aue_devclass;

DRIVER_MODULE(if_aue, uhub, aue_driver, aue_devclass, usbd_driver_load, 0);
DRIVER_MODULE(miibus, aue, miibus_driver, miibus_devclass, 0, 0);

#define AUE_SETBIT(sc, reg, x)				\
	csr_write_1(sc, reg, csr_read_1(sc, reg) | (x))

#define AUE_CLRBIT(sc, reg, x)				\
	csr_write_1(sc, reg, csr_read_1(sc, reg) & ~(x))

Static int
csr_read_1(struct aue_softc *sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	u_int8_t		val = 0;
	int			s;

	if (sc->aue_gone)
		return(0);

	s = splusb();

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request_flags(sc->aue_udev, &req,
	    &val, USBD_NO_TSLEEP, NULL);

	splx(s);

	if (err)
		return(0);

	return(val);
}

Static int
csr_read_2(struct aue_softc *sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	u_int16_t		val = 0;
	int			s;

	if (sc->aue_gone)
		return(0);

	s = splusb();

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	err = usbd_do_request_flags(sc->aue_udev, &req,
	    &val, USBD_NO_TSLEEP, NULL);

	splx(s);

	if (err)
		return(0);

	return(val);
}

Static int
csr_write_1(struct aue_softc *sc, int reg, int val)
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;

	if (sc->aue_gone)
		return(0);

	s = splusb();

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request_flags(sc->aue_udev, &req,
	    &val, USBD_NO_TSLEEP, NULL);

	splx(s);

	if (err)
		return(-1);

	return(0);
}

Static int
csr_write_2(struct aue_softc *sc, int reg, int val)
{
	usb_device_request_t	req;
	usbd_status		err;
	int			s;

	if (sc->aue_gone)
		return(0);

	s = splusb();

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	err = usbd_do_request_flags(sc->aue_udev, &req,
	    &val, USBD_NO_TSLEEP, NULL);

	splx(s);

	if (err)
		return(-1);

	return(0);
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
Static void
aue_eeprom_getword(struct aue_softc *sc, int addr, u_int16_t *dest)
{
	register int		i;
	u_int16_t		word = 0;

	csr_write_1(sc, AUE_EE_REG, addr);
	csr_write_1(sc, AUE_EE_CTL, AUE_EECTL_READ);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (csr_read_1(sc, AUE_EE_CTL) &
		    AUE_EECTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("aue%d: EEPROM read timed out\n",
		    sc->aue_unit);
	}

	word = csr_read_2(sc, AUE_EE_DATA);
	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
Static void
aue_read_eeprom(struct aue_softc *sc, caddr_t dest, int off, int cnt, int swap)
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		aue_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}

Static int
aue_miibus_readreg(device_t dev, int phy, int reg)
{
	struct aue_softc	*sc;
	int			i;
	u_int16_t		val = 0;

	sc = device_get_softc(dev);

	/*
	 * The Am79C901 HomePNA PHY actually contains
	 * two transceivers: a 1Mbps HomePNA PHY and a
	 * 10Mbps full/half duplex ethernet PHY with
	 * NWAY autoneg. However in the ADMtek adapter,
	 * only the 1Mbps PHY is actually connected to
	 * anything, so we ignore the 10Mbps one. It
	 * happens to be configured for MII address 3,
	 * so we filter that out.
	 */
	if (sc->aue_info->aue_vid == USB_VENDOR_ADMTEK &&
	    sc->aue_info->aue_did == USB_PRODUCT_ADMTEK_PEGASUS) {
		if (phy == 3)
			return(0);
#ifdef notdef
		if (phy != 1)
			return(0);
#endif
	}

	csr_write_1(sc, AUE_PHY_ADDR, phy);
	csr_write_1(sc, AUE_PHY_CTL, reg|AUE_PHYCTL_READ);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (csr_read_1(sc, AUE_PHY_CTL) &
		    AUE_PHYCTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("aue%d: MII read timed out\n",
		    sc->aue_unit);
	}

	val = csr_read_2(sc, AUE_PHY_DATA);

	return(val);
}

Static int
aue_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct aue_softc	*sc;
	int			i;

	if (phy == 3)
		return(0);

	sc = device_get_softc(dev);

	csr_write_2(sc, AUE_PHY_DATA, data);
	csr_write_1(sc, AUE_PHY_ADDR, phy);
	csr_write_1(sc, AUE_PHY_CTL, reg|AUE_PHYCTL_WRITE);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (csr_read_1(sc, AUE_PHY_CTL) &
		    AUE_PHYCTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("aue%d: MII read timed out\n",
		    sc->aue_unit);
	}

	return(0);
}

Static void
aue_miibus_statchg(device_t dev)
{
	struct aue_softc	*sc;
	struct mii_data		*mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->aue_miibus);

	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_RX_ENB|AUE_CTL0_TX_ENB);
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX) {
		AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);
	} else {
		AUE_CLRBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);
	}

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_DUPLEX);
	} else {
		AUE_CLRBIT(sc, AUE_CTL1, AUE_CTL1_DUPLEX);
	}
	AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_RX_ENB|AUE_CTL0_TX_ENB);

	/*
	 * Set the LED modes on the LinkSys adapter.
	 * This turns on the 'dual link LED' bin in the auxmode
	 * register of the Broadcom PHY.
	 */
	if (sc->aue_info->aue_flags & LSYS) {
		u_int16_t		auxmode;
		auxmode = aue_miibus_readreg(dev, 0, 0x1b);
		aue_miibus_writereg(dev, 0, 0x1b, auxmode | 0x04);
	}

	return;
}

#define AUE_POLY	0xEDB88320
#define AUE_BITS	6

Static u_int32_t
aue_crc(caddr_t addr)
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? AUE_POLY : 0);
	}

	return (crc & ((1 << AUE_BITS) - 1));
}

Static void
aue_setmulti(struct aue_softc *sc)
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int32_t		h = 0, i;

	ifp = &sc->arpcom.ac_if;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);
		return;
	}

	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);

	/* first, zot all the existing hash bits */
	for (i = 0; i < 8; i++)
		csr_write_1(sc, AUE_MAR0 + i, 0);

	/* now program new ones */
	for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
	    ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = aue_crc(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		AUE_SETBIT(sc, AUE_MAR + (h >> 3), 1 << (h & 0x7));
	}

	return;
}

Static void
aue_reset_pegasus_II(struct aue_softc *sc)
{
	/* Magic constants taken from Linux driver. */
	csr_write_1(sc, AUE_REG_1D, 0);
	csr_write_1(sc, AUE_REG_7B, 2);
#if 0
	if ((sc->aue_flags & HAS_HOME_PNA) && mii_mode)
		csr_write_1(sc, AUE_REG_81, 6);
	else
#endif
		csr_write_1(sc, AUE_REG_81, 2);
}

Static void
aue_reset(struct aue_softc *sc)
{
	register int		i;

	AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_RESETMAC);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (!(csr_read_1(sc, AUE_CTL1) & AUE_CTL1_RESETMAC))
			break;
	}

	if (i == AUE_TIMEOUT)
		printf("aue%d: reset failed\n", sc->aue_unit);

	/*
	 * The PHY(s) attached to the Pegasus chip may be held
	 * in reset until we flip on the GPIO outputs. Make sure
	 * to set the GPIO pins high so that the PHY(s) will
	 * be enabled.
	 *
	 * Note: We force all of the GPIO pins low first, *then*
	 * enable the ones we want.
	 */
	csr_write_1(sc, AUE_GPIO0, AUE_GPIO_OUT0|AUE_GPIO_SEL0);
	csr_write_1(sc, AUE_GPIO0, AUE_GPIO_OUT0|AUE_GPIO_SEL0|AUE_GPIO_SEL1);

	/* Grrr. LinkSys has to be different from everyone else. */
	if (sc->aue_info->aue_flags & LSYS) {
		csr_write_1(sc, AUE_GPIO0, AUE_GPIO_SEL0|AUE_GPIO_SEL1);
		csr_write_1(sc, AUE_GPIO0, AUE_GPIO_SEL0|AUE_GPIO_SEL1|
			AUE_GPIO_OUT0);
	}

	if (sc->aue_info->aue_flags & PII)
                aue_reset_pegasus_II(sc);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(10000);

        return;
}

/*
 * Probe for a Pegasus chip.
 */
USB_MATCH(aue)
{
	USB_MATCH_START(aue, uaa);
	struct aue_type			*t;

	if (!uaa->iface)
		return(UMATCH_NONE);

	t = aue_devs;
	while(t->aue_vid) {
		if (uaa->vendor == t->aue_vid &&
		    uaa->product == t->aue_did) {
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
USB_ATTACH(aue)
{
	USB_ATTACH_START(aue, sc, uaa);
	char			devinfo[1024];
	int			s;
	u_char			eaddr[ETHER_ADDR_LEN];
	struct ifnet		*ifp;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;
	struct aue_type		*t;

	s = splimp();

	bzero(sc, sizeof(struct aue_softc));
	sc->aue_iface = uaa->iface;
	sc->aue_udev = uaa->device;
	sc->aue_unit = device_get_unit(self);

	if (usbd_set_config_no(sc->aue_udev, AUE_CONFIG_NO, 0)) {
		printf("aue%d: getting interface handle failed\n",
		    sc->aue_unit);
		splx(s);
		USB_ATTACH_ERROR_RETURN;
	}

	t = aue_devs;
	while(t->aue_vid) {
		if (uaa->vendor == t->aue_vid &&
		    uaa->product == t->aue_did) {
			sc->aue_info = t;
			break;
		}
		t++;
	}

	id = usbd_get_interface_descriptor(uaa->iface);

	usbd_devinfo(uaa->device, 0, devinfo);
	device_set_desc_copy(self, devinfo);
	printf("%s: %s\n", USBDEVNAME(self), devinfo);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(uaa->iface, i);
		if (!ed) {
			printf("aue%d: couldn't get ep %d\n",
			    sc->aue_unit, i);
			splx(s);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->aue_ed[AUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_BULK) {
			sc->aue_ed[AUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    (ed->bmAttributes & UE_XFERTYPE) == UE_INTERRUPT) {
			sc->aue_ed[AUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	/* Reset the adapter. */
	aue_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	aue_read_eeprom(sc, (caddr_t)&eaddr, 0, 3, 0);

	/*
	 * A Pegasus chip was detected. Inform the world.
	 */
	printf("aue%d: Ethernet address: %6D\n", sc->aue_unit, eaddr, ":");

	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_unit = sc->aue_unit;
	ifp->if_name = "aue";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = aue_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = aue_start;
	ifp->if_watchdog = aue_watchdog;
	ifp->if_init = aue_init;
	ifp->if_baudrate = 10000000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	/*
	 * Do MII setup.
	 * NOTE: Doing this causes child devices to be attached to us,
	 * which we would normally disconnect at in the detach routine
	 * using device_delete_child(). However the USB code is set up
	 * such that when this driver is removed, all children devices
	 * are removed as well. In effect, the USB code ends up detaching
	 * all of our children for us, so we don't have to do is ourselves
	 * in aue_detach(). It's important to point this out since if
	 * we *do* try to detach the child devices ourselves, we will
	 * end up getting the children deleted twice, which will crash
	 * the system.
	 */
	if (mii_phy_probe(self, &sc->aue_miibus,
	    aue_ifmedia_upd, aue_ifmedia_sts)) {
		printf("aue%d: MII without any PHY!\n", sc->aue_unit);
		splx(s);
		USB_ATTACH_ERROR_RETURN;
	}

	aue_qdat.ifp = ifp;
	aue_qdat.if_rxstart = aue_rxstart;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	callout_handle_init(&sc->aue_stat_ch);
	usb_register_netisr();
	sc->aue_gone = 0;

	splx(s);
	USB_ATTACH_SUCCESS_RETURN;
}

Static int
aue_detach(device_ptr_t dev)
{
	struct aue_softc	*sc;
	struct ifnet		*ifp;
	int			s;

	s = splusb();

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	sc->aue_gone = 1;
	untimeout(aue_tick, sc, sc->aue_stat_ch);
	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);

	if (sc->aue_ep[AUE_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_TX]);
	if (sc->aue_ep[AUE_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_RX]);
#ifdef AUE_INTR_PIPE
	if (sc->aue_ep[AUE_ENDPT_INTR] != NULL)
		usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
#endif
	splx(s);

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
Static int
aue_newbuf(struct aue_softc *sc, struct aue_chain *c, struct mbuf *m)
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("aue%d: no memory for rx list "
			    "-- packet dropped!\n", sc->aue_unit);
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("aue%d: no memory for rx list "
			    "-- packet dropped!\n", sc->aue_unit);
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, ETHER_ALIGN);
	c->aue_mbuf = m_new;

	return(0);
}

Static int
aue_rx_list_init(struct aue_softc *sc)
{
	struct aue_cdata	*cd;
	struct aue_chain	*c;
	int			i;

	cd = &sc->aue_cdata;
	for (i = 0; i < AUE_RX_LIST_CNT; i++) {
		c = &cd->aue_rx_chain[i];
		c->aue_sc = sc;
		c->aue_idx = i;
		if (aue_newbuf(sc, c, NULL) == ENOBUFS)
			return(ENOBUFS);
		if (c->aue_xfer == NULL) {
			c->aue_xfer = usbd_alloc_xfer(sc->aue_udev);
			if (c->aue_xfer == NULL)
				return(ENOBUFS);
		}
	}

	return(0);
}

Static int
aue_tx_list_init(struct aue_softc *sc)
{
	struct aue_cdata	*cd;
	struct aue_chain	*c;
	int			i;

	cd = &sc->aue_cdata;
	for (i = 0; i < AUE_TX_LIST_CNT; i++) {
		c = &cd->aue_tx_chain[i];
		c->aue_sc = sc;
		c->aue_idx = i;
		c->aue_mbuf = NULL;
		if (c->aue_xfer == NULL) {
			c->aue_xfer = usbd_alloc_xfer(sc->aue_udev);
			if (c->aue_xfer == NULL)
				return(ENOBUFS);
		}
		c->aue_buf = malloc(AUE_BUFSZ, M_USBDEV, M_NOWAIT);
		if (c->aue_buf == NULL)
			return(ENOBUFS);
	}

	return(0);
}

#ifdef AUE_INTR_PIPE
Static void
aue_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct aue_softc	*sc;
	struct ifnet		*ifp;
	struct aue_intrpkt	*p;
	int			s;

	s = splimp();

	sc = priv;
	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_RUNNING)) {
		splx(s);
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		printf("aue%d: usb error on intr: %s\n", sc->aue_unit,
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_RX]);
		splx(s);
		return;
	}

	usbd_get_xfer_status(xfer, NULL, (void **)&p, NULL, NULL);

	if (p->aue_txstat0)
		ifp->if_oerrors++;

	if (p->aue_txstat0 & (AUE_TXSTAT0_LATECOLL & AUE_TXSTAT0_EXCESSCOLL))
		ifp->if_collisions++;

	splx(s);
	return;
}
#endif

Static void
aue_rxstart(struct ifnet *ifp)
{
	struct aue_softc	*sc;
	struct aue_chain	*c;

	sc = ifp->if_softc;
	c = &sc->aue_cdata.aue_rx_chain[sc->aue_cdata.aue_rx_prod];

	if (aue_newbuf(sc, c, NULL) == ENOBUFS) {
		ifp->if_ierrors++;
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->aue_xfer, sc->aue_ep[AUE_ENDPT_RX],
	    c, mtod(c->aue_mbuf, char *), AUE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, aue_rxeof);
	usbd_transfer(c->aue_xfer);

	return;
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
Static void
aue_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct aue_chain	*c = priv;
	struct aue_softc	*sc = c->aue_sc;
        struct mbuf		*m;
        struct ifnet		*ifp;
	int			total_len = 0;
	struct aue_rxpkt	r;

	c = priv;
	sc = c->aue_sc;
	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;
		printf("aue%d: usb error on rx: %s\n", sc->aue_unit,
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &total_len, NULL);

	if (total_len <= 4 + ETHER_CRC_LEN) {
		ifp->if_ierrors++;
		goto done;
	}

	m = c->aue_mbuf;
	bcopy(mtod(m, char *) + total_len - 4, (char *)&r, sizeof(r));

	/* Turn off all the non-error bits in the rx status word. */
	r.aue_rxstat &= AUE_RXSTAT_MASK;

	if (r.aue_rxstat) {
		ifp->if_ierrors++;
		goto done;
	}

	/* No errors; receive the packet. */
	total_len -= (4 + ETHER_CRC_LEN);

	ifp->if_ipackets++;
	m->m_pkthdr.rcvif = (struct ifnet *)&aue_qdat;
	m->m_pkthdr.len = m->m_len = total_len;

	/* Put the packet on the special USB input queue. */
	usb_ether_input(m);

	return;
done:

	/* Setup new transfer. */
	usbd_setup_xfer(xfer, sc->aue_ep[AUE_ENDPT_RX],
	    c, mtod(c->aue_mbuf, char *), AUE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, aue_rxeof);
	usbd_transfer(xfer);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

Static void
aue_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct aue_softc	*sc;
	struct aue_chain	*c;
	struct ifnet		*ifp;
	usbd_status		err;
	int			s;

	s = splimp();

	c = priv;
	sc = c->aue_sc;
	ifp = &sc->arpcom.ac_if;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			splx(s);
			return;
		}
		printf("aue%d: usb error on tx: %s\n", sc->aue_unit,
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_TX]);
		splx(s);
		return;
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	usbd_get_xfer_status(c->aue_xfer, NULL, NULL, NULL, &err);

	if (c->aue_mbuf != NULL) {
		c->aue_mbuf->m_pkthdr.rcvif = ifp;
		usb_tx_done(c->aue_mbuf);
		c->aue_mbuf = NULL;
	}

	if (err)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	splx(s);

	return;
}

Static void
aue_tick(void *xsc)
{
	struct aue_softc	*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;
	int			s;

	s = splimp();

	sc = xsc;

	if (sc == NULL) {
		splx(s);
		return;
	}

	ifp = &sc->arpcom.ac_if;
	mii = device_get_softc(sc->aue_miibus);
	if (mii == NULL) {
		splx(s);
		return;
	}

	mii_tick(mii);
	if (!sc->aue_link) {
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
			sc->aue_link++;
			if (ifp->if_snd.ifq_head != NULL)
				aue_start(ifp);
	}

	sc->aue_stat_ch = timeout(aue_tick, sc, hz);

	splx(s);

	return;
}

Static int
aue_encap(struct aue_softc *sc, struct mbuf *m, int idx)
{
	int			total_len;
	struct aue_chain	*c;
	usbd_status		err;

	c = &sc->aue_cdata.aue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->aue_buf + 2);
	c->aue_mbuf = m;

	total_len = m->m_pkthdr.len + 2;

	/*
	 * The ADMtek documentation says that the packet length is
	 * supposed to be specified in the first two bytes of the
	 * transfer, however it actually seems to ignore this info
	 * and base the frame size on the bulk transfer length.
	 */
	c->aue_buf[0] = (u_int8_t)m->m_pkthdr.len;
	c->aue_buf[1] = (u_int8_t)(m->m_pkthdr.len >> 8);

	usbd_setup_xfer(c->aue_xfer, sc->aue_ep[AUE_ENDPT_TX],
	    c, c->aue_buf, total_len, USBD_FORCE_SHORT_XFER,
	    10000, aue_txeof);

	/* Transmit */
	err = usbd_transfer(c->aue_xfer);
	if (err != USBD_IN_PROGRESS) {
		aue_stop(sc);
		return(EIO);
	}

	sc->aue_cdata.aue_tx_cnt++;

	return(0);
}

Static void
aue_start(struct ifnet *ifp)
{
	struct aue_softc	*sc;
	struct mbuf		*m_head = NULL;

	sc = ifp->if_softc;

	if (!sc->aue_link)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	IF_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL)
		return;

	if (aue_encap(sc, m_head, 0)) {
		IF_PREPEND(&ifp->if_snd, m_head);
		ifp->if_flags |= IFF_OACTIVE;
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

	return;
}

Static void
aue_init(void *xsc)
{
	struct aue_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii;
	struct aue_chain	*c;
	usbd_status		err;
	int			i, s;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splimp();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	aue_reset(sc);

	mii = device_get_softc(sc->aue_miibus);

	/* Set MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		csr_write_1(sc, AUE_PAR0 + i, sc->arpcom.ac_enaddr[i]);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
	} else {
		AUE_CLRBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
	}

	/* Init TX ring. */
	if (aue_tx_list_init(sc) == ENOBUFS) {
		printf("aue%d: tx list init failed\n", sc->aue_unit);
		splx(s);
		return;
	}

	/* Init RX ring. */
	if (aue_rx_list_init(sc) == ENOBUFS) {
		printf("aue%d: rx list init failed\n", sc->aue_unit);
		splx(s);
		return;
	}

#ifdef AUE_INTR_PIPE
	sc->aue_cdata.aue_ibuf = malloc(AUE_INTR_PKTLEN, M_USBDEV, M_NOWAIT);
#endif

	/* Load the multicast filter. */
	aue_setmulti(sc);

	/* Enable RX and TX */
	csr_write_1(sc, AUE_CTL0, AUE_CTL0_RXSTAT_APPEND|AUE_CTL0_RX_ENB);
	AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_TX_ENB);
	AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_EP3_CLR);
	mii_mediachg(mii);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->aue_iface, sc->aue_ed[AUE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_RX]);
	if (err) {
		printf("aue%d: open rx pipe failed: %s\n",
		    sc->aue_unit, usbd_errstr(err));
		splx(s);
		return;
	}
	err = usbd_open_pipe(sc->aue_iface, sc->aue_ed[AUE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_TX]);
	if (err) {
		printf("aue%d: open tx pipe failed: %s\n",
		    sc->aue_unit, usbd_errstr(err));
		splx(s);
		return;
	}

#ifdef AUE_INTR_PIPE
	err = usbd_open_pipe_intr(sc->aue_iface, sc->aue_ed[AUE_ENDPT_INTR],
	    USBD_SHORT_XFER_OK, &sc->aue_ep[AUE_ENDPT_INTR], sc,
	    sc->aue_cdata.aue_ibuf, AUE_INTR_PKTLEN, aue_intr,
	    AUE_INTR_INTERVAL);
	if (err) {
		printf("aue%d: open intr pipe failed: %s\n",
		    sc->aue_unit, usbd_errstr(err));
		splx(s);
		return;
	}
#endif

	/* Start up the receive pipe. */
	for (i = 0; i < AUE_RX_LIST_CNT; i++) {
		c = &sc->aue_cdata.aue_rx_chain[i];
		usbd_setup_xfer(c->aue_xfer, sc->aue_ep[AUE_ENDPT_RX],
		    c, mtod(c->aue_mbuf, char *), AUE_BUFSZ,
	    	USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, aue_rxeof);
		usbd_transfer(c->aue_xfer);
	}

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	sc->aue_stat_ch = timeout(aue_tick, sc, hz);

	return;
}

/*
 * Set media options.
 */
Static int
aue_ifmedia_upd(struct ifnet *ifp)
{
	struct aue_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;

	mii = device_get_softc(sc->aue_miibus);
	sc->aue_link = 0;
	if (mii->mii_instance) {
		struct mii_softc	*miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		    miisc = LIST_NEXT(miisc, mii_list))
			 mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return(0);
}

/*
 * Report current media status.
 */
Static void
aue_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct aue_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;

	mii = device_get_softc(sc->aue_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

Static int
aue_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct aue_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			s, error = 0;

	s = splimp();

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
			    !(sc->aue_if_flags & IFF_PROMISC)) {
				AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->aue_if_flags & IFF_PROMISC) {
				AUE_CLRBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				aue_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				aue_stop(sc);
		}
		sc->aue_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		aue_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->aue_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	(void)splx(s);

	return(error);
}

Static void
aue_watchdog(struct ifnet *ifp)
{
	struct aue_softc	*sc;
	struct aue_chain	*c;
	usbd_status		stat;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	printf("aue%d: watchdog timeout\n", sc->aue_unit);

	c = &sc->aue_cdata.aue_tx_chain[0];
	usbd_get_xfer_status(c->aue_xfer, NULL, NULL, NULL, &stat);
	aue_txeof(c->aue_xfer, c, stat);

	if (ifp->if_snd.ifq_head != NULL)
		aue_start(ifp);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
Static void
aue_stop(struct aue_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;
	int			i;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	csr_write_1(sc, AUE_CTL0, 0);
	csr_write_1(sc, AUE_CTL1, 0);
	aue_reset(sc);
	untimeout(aue_tick, sc, sc->aue_stat_ch);

	/* Stop transfers. */
	if (sc->aue_ep[AUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_RX]);
		if (err) {
			printf("aue%d: abort rx pipe failed: %s\n",
		    	sc->aue_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_RX]);
		if (err) {
			printf("aue%d: close rx pipe failed: %s\n",
		    	sc->aue_unit, usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_RX] = NULL;
	}

	if (sc->aue_ep[AUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_TX]);
		if (err) {
			printf("aue%d: abort tx pipe failed: %s\n",
		    	sc->aue_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_TX]);
		if (err) {
			printf("aue%d: close tx pipe failed: %s\n",
			    sc->aue_unit, usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_TX] = NULL;
	}

#ifdef AUE_INTR_PIPE
	if (sc->aue_ep[AUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
		if (err) {
			printf("aue%d: abort intr pipe failed: %s\n",
		    	sc->aue_unit, usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
		if (err) {
			printf("aue%d: close intr pipe failed: %s\n",
			    sc->aue_unit, usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_INTR] = NULL;
	}
#endif

	/* Free RX resources. */
	for (i = 0; i < AUE_RX_LIST_CNT; i++) {
		if (sc->aue_cdata.aue_rx_chain[i].aue_buf != NULL) {
			free(sc->aue_cdata.aue_rx_chain[i].aue_buf, M_USBDEV);
			sc->aue_cdata.aue_rx_chain[i].aue_buf = NULL;
		}
		if (sc->aue_cdata.aue_rx_chain[i].aue_mbuf != NULL) {
			m_freem(sc->aue_cdata.aue_rx_chain[i].aue_mbuf);
			sc->aue_cdata.aue_rx_chain[i].aue_mbuf = NULL;
		}
		if (sc->aue_cdata.aue_rx_chain[i].aue_xfer != NULL) {
			usbd_free_xfer(sc->aue_cdata.aue_rx_chain[i].aue_xfer);
			sc->aue_cdata.aue_rx_chain[i].aue_xfer = NULL;
		}
	}

	/* Free TX resources. */
	for (i = 0; i < AUE_TX_LIST_CNT; i++) {
		if (sc->aue_cdata.aue_tx_chain[i].aue_buf != NULL) {
			free(sc->aue_cdata.aue_tx_chain[i].aue_buf, M_USBDEV);
			sc->aue_cdata.aue_tx_chain[i].aue_buf = NULL;
		}
		if (sc->aue_cdata.aue_tx_chain[i].aue_mbuf != NULL) {
			m_freem(sc->aue_cdata.aue_tx_chain[i].aue_mbuf);
			sc->aue_cdata.aue_tx_chain[i].aue_mbuf = NULL;
		}
		if (sc->aue_cdata.aue_tx_chain[i].aue_xfer != NULL) {
			usbd_free_xfer(sc->aue_cdata.aue_tx_chain[i].aue_xfer);
			sc->aue_cdata.aue_tx_chain[i].aue_xfer = NULL;
		}
	}

#ifdef AUE_INTR_PIPE
	free(sc->aue_cdata.aue_ibuf, M_USBDEV);
	sc->aue_cdata.aue_ibuf = NULL;
#endif

	sc->aue_link = 0;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
Static void
aue_shutdown(device_ptr_t dev)
{
	struct aue_softc	*sc;

	sc = device_get_softc(dev);

	aue_reset(sc);
	aue_stop(sc);

	return;
}
