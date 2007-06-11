/*-
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
 *
 * Copyright (c) 2006
 *      Alfred Perlstein <alfred@freebsd.org>. All rights reserved.
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
 * ADMtek AN986 Pegasus and AN8511 Pegasus II USB to ethernet driver.
 * Datasheet is available from http://www.admtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 *
 * SMP locking by Alfred Perlstein <alfred@freebsd.org>.
 * RED Inc.
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
#include <sys/kdb.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
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

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/usb/if_auereg.h>

MODULE_DEPEND(aue, usb, 1, 1, 1);
MODULE_DEPEND(aue, ether, 1, 1, 1);
MODULE_DEPEND(aue, miibus, 1, 1, 1);

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

/*
 * Various supported device vendors/products.
 */
struct aue_type {
	struct usb_devno	aue_dev;
	u_int16_t		aue_flags;
#define LSYS	0x0001		/* use Linksys reset */
#define PNA	0x0002		/* has Home PNA */
#define PII	0x0004		/* Pegasus II chip */
};

static const struct aue_type aue_devs[] = {
 {{ USB_VENDOR_3COM,		USB_PRODUCT_3COM_3C460B},         PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX1},	  PNA|PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX2},	  PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_UFE1000},	  LSYS },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX4},	  PNA },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX5},	  PNA },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX6},	  PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX7},	  PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX8},	  PII },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX9},	  PNA },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_XX10},	  0 },
 {{ USB_VENDOR_ABOCOM,		USB_PRODUCT_ABOCOM_DSB650TX_PNA}, 0 },
 {{ USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_USB320_EC},	  0 },
 {{ USB_VENDOR_ACCTON,		USB_PRODUCT_ACCTON_SS1001},	  PII },
 {{ USB_VENDOR_ADMTEK,		USB_PRODUCT_ADMTEK_PEGASUS},	  PNA },
 {{ USB_VENDOR_ADMTEK,		USB_PRODUCT_ADMTEK_PEGASUSII},	  PII },
 {{ USB_VENDOR_ADMTEK,		USB_PRODUCT_ADMTEK_PEGASUSII_2},  PII },
 {{ USB_VENDOR_BELKIN,		USB_PRODUCT_BELKIN_USB2LAN},	  PII },
 {{ USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USB100},	  0 },
 {{ USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USBLP100}, PNA },
 {{ USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USBEL100}, 0 },
 {{ USB_VENDOR_BILLIONTON,	USB_PRODUCT_BILLIONTON_USBE100},  PII },
 {{ USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_FETHER_USB_TX}, 0 },
 {{ USB_VENDOR_COREGA,		USB_PRODUCT_COREGA_FETHER_USB_TXS},PII },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX4},	  LSYS|PII },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX1},	  LSYS },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX},	  LSYS },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX_PNA},  PNA },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX3},	  LSYS|PII },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650TX2},	  LSYS|PII },
 {{ USB_VENDOR_DLINK,		USB_PRODUCT_DLINK_DSB650},	  LSYS },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBTX0},	  0 },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBTX1},	  LSYS },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBTX2},	  0 },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBTX3},	  LSYS },
 {{ USB_VENDOR_ELECOM,		USB_PRODUCT_ELECOM_LDUSBLTX},	  PII },
 {{ USB_VENDOR_ELSA,		USB_PRODUCT_ELSA_USB2ETHERNET},	  0 },
 {{ USB_VENDOR_HAWKING,		USB_PRODUCT_HAWKING_UF100},       PII },
 {{ USB_VENDOR_HP,		USB_PRODUCT_HP_HN210E},           PII },
 {{ USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_USBETTX},	  0 },
 {{ USB_VENDOR_IODATA,		USB_PRODUCT_IODATA_USBETTXS},	  PII },
 {{ USB_VENDOR_KINGSTON,	USB_PRODUCT_KINGSTON_KNU101TX},   0 },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB10TX1},	  LSYS|PII },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB10T},	  LSYS },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB100TX},	  LSYS },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB100H1},	  LSYS|PNA },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB10TA},	  LSYS },
 {{ USB_VENDOR_LINKSYS,		USB_PRODUCT_LINKSYS_USB10TX2},	  LSYS|PII },
 {{ USB_VENDOR_MICROSOFT,	USB_PRODUCT_MICROSOFT_MN110},	  PII },
 {{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_LUATX1},	  0 },
 {{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_LUATX5},	  0 },
 {{ USB_VENDOR_MELCO,		USB_PRODUCT_MELCO_LUA2TX5},	  PII },
 {{ USB_VENDOR_NETGEAR,		USB_PRODUCT_NETGEAR_FA101},	  PII },
 {{ USB_VENDOR_SIEMENS,		USB_PRODUCT_SIEMENS_SPEEDSTREAM}, PII },
 {{ USB_VENDOR_SMARTBRIDGES,	USB_PRODUCT_SMARTBRIDGES_SMARTNIC},PII },
 {{ USB_VENDOR_SMC,		USB_PRODUCT_SMC_2202USB},	  0 },
 {{ USB_VENDOR_SMC,		USB_PRODUCT_SMC_2206USB},	  PII },
 {{ USB_VENDOR_SOHOWARE,	USB_PRODUCT_SOHOWARE_NUB100},	  0 },
};
#define aue_lookup(v, p) ((const struct aue_type *)usb_lookup(aue_devs, v, p))

static int aue_match(device_t);
static int aue_attach(device_t);
static int aue_detach(device_t);

static void aue_reset_pegasus_II(struct aue_softc *sc);
static int aue_encap(struct aue_softc *, struct mbuf *, int);
#ifdef AUE_INTR_PIPE
static void aue_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
#endif
static void aue_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void aue_rxeof_thread(struct aue_softc *sc);
static void aue_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
static void aue_txeof_thread(struct aue_softc *);
static void aue_task_sched(struct aue_softc *, int);
static void aue_task(void *xsc, int pending);
static void aue_tick(void *);
static void aue_rxstart(struct ifnet *);
static void aue_rxstart_thread(struct aue_softc *);
static void aue_start(struct ifnet *);
static void aue_start_thread(struct aue_softc *);
static int aue_ioctl(struct ifnet *, u_long, caddr_t);
static void aue_init(void *);
static void aue_init_body(struct aue_softc *);
static void aue_stop(struct aue_softc *);
static void aue_watchdog(struct aue_softc *);
static void aue_shutdown(device_t);
static int aue_ifmedia_upd(struct ifnet *);
static void aue_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static void aue_eeprom_getword(struct aue_softc *, int, u_int16_t *);
static void aue_read_eeprom(struct aue_softc *, caddr_t, int, int, int);
static int aue_miibus_readreg(device_t, int, int);
static int aue_miibus_writereg(device_t, int, int, int);
static void aue_miibus_statchg(device_t);

static void aue_setmulti(struct aue_softc *);
static void aue_reset(struct aue_softc *);

static int aue_csr_read_1(struct aue_softc *, int);
static int aue_csr_write_1(struct aue_softc *, int, int);
static int aue_csr_read_2(struct aue_softc *, int);
static int aue_csr_write_2(struct aue_softc *, int, int);

static device_method_t aue_methods[] = {
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

static driver_t aue_driver = {
	"aue",
	aue_methods,
	sizeof(struct aue_softc)
};

static devclass_t aue_devclass;

DRIVER_MODULE(aue, uhub, aue_driver, aue_devclass, usbd_driver_load, 0);
DRIVER_MODULE(miibus, aue, miibus_driver, miibus_devclass, 0, 0);

#define AUE_SETBIT(sc, reg, x)				\
	aue_csr_write_1(sc, reg, aue_csr_read_1(sc, reg) | (x))

#define AUE_CLRBIT(sc, reg, x)				\
	aue_csr_write_1(sc, reg, aue_csr_read_1(sc, reg) & ~(x))

static int
aue_csr_read_1(struct aue_softc *sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	u_int8_t		val = 0;

	AUE_SXASSERTLOCKED(sc);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request(sc->aue_udev, &req, &val);

	if (err) {
		return (0);
	}

	return (val);
}

static int
aue_csr_read_2(struct aue_softc *sc, int reg)
{
	usb_device_request_t	req;
	usbd_status		err;
	u_int16_t		val = 0;

	AUE_SXASSERTLOCKED(sc);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	err = usbd_do_request(sc->aue_udev, &req, &val);

	if (err) {
		return (0);
	}

	return (val);
}

static int
aue_csr_write_1(struct aue_softc *sc, int reg, int val)
{
	usb_device_request_t	req;
	usbd_status		err;

	AUE_SXASSERTLOCKED(sc);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usbd_do_request(sc->aue_udev, &req, &val);

	if (err) {
		return (-1);
	}

	return (0);
}

static int
aue_csr_write_2(struct aue_softc *sc, int reg, int val)
{
	usb_device_request_t	req;
	usbd_status		err;

	AUE_SXASSERTLOCKED(sc);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	err = usbd_do_request(sc->aue_udev, &req, &val);

	if (err) {
		return (-1);
	}

	return (0);
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void
aue_eeprom_getword(struct aue_softc *sc, int addr, u_int16_t *dest)
{
	int		i;
	u_int16_t	word = 0;

	aue_csr_write_1(sc, AUE_EE_REG, addr);
	aue_csr_write_1(sc, AUE_EE_CTL, AUE_EECTL_READ);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_EE_CTL) & AUE_EECTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("aue%d: EEPROM read timed out\n",
		    sc->aue_unit);
	}

	word = aue_csr_read_2(sc, AUE_EE_DATA);
	*dest = word;

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void
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

static int
aue_miibus_readreg(device_t dev, int phy, int reg)
{
	struct aue_softc	*sc = USBGETSOFTC(dev);
	int			i;
	u_int16_t		val = 0;

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
	if (sc->aue_vendor == USB_VENDOR_ADMTEK &&
	    sc->aue_product == USB_PRODUCT_ADMTEK_PEGASUS) {
		if (phy == 3)
			return (0);
#ifdef notdef
		if (phy != 1)
			return (0);
#endif
	}

	aue_csr_write_1(sc, AUE_PHY_ADDR, phy);
	aue_csr_write_1(sc, AUE_PHY_CTL, reg | AUE_PHYCTL_READ);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_PHY_CTL) & AUE_PHYCTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("aue%d: MII read timed out\n", sc->aue_unit);
	}

	val = aue_csr_read_2(sc, AUE_PHY_DATA);

	return (val);
}

static int
aue_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct aue_softc	*sc = USBGETSOFTC(dev);
	int			i;

	if (phy == 3)
		return (0);

	aue_csr_write_2(sc, AUE_PHY_DATA, data);
	aue_csr_write_1(sc, AUE_PHY_ADDR, phy);
	aue_csr_write_1(sc, AUE_PHY_CTL, reg | AUE_PHYCTL_WRITE);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_PHY_CTL) & AUE_PHYCTL_DONE)
			break;
	}

	if (i == AUE_TIMEOUT) {
		printf("aue%d: MII read timed out\n",
		    sc->aue_unit);
	}

	return(0);
}

static void
aue_miibus_statchg(device_t dev)
{
	struct aue_softc	*sc = USBGETSOFTC(dev);
	struct mii_data		*mii = GET_MII(sc);

	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_RX_ENB | AUE_CTL0_TX_ENB);
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX) {
		AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);
	} else {
		AUE_CLRBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);
	}

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_DUPLEX);
	else
		AUE_CLRBIT(sc, AUE_CTL1, AUE_CTL1_DUPLEX);

	AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_RX_ENB | AUE_CTL0_TX_ENB);

	/*
	 * Set the LED modes on the LinkSys adapter.
	 * This turns on the 'dual link LED' bin in the auxmode
	 * register of the Broadcom PHY.
	 */
	if (sc->aue_flags & LSYS) {
		u_int16_t auxmode;
		auxmode = aue_miibus_readreg(dev, 0, 0x1b);
		aue_miibus_writereg(dev, 0, 0x1b, auxmode | 0x04);
	}

	return;
}

#define AUE_BITS	6

static void
aue_setmulti(struct aue_softc *sc)
{
	struct ifnet		*ifp;
	struct ifmultiaddr	*ifma;
	u_int32_t		h = 0, i;
	u_int8_t		hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	AUE_SXASSERTLOCKED(sc);
	ifp = sc->aue_ifp;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);
		return;
	}

	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);

	/* now program new ones */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
	{
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_le(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) & ((1 << AUE_BITS) - 1);
		hashtbl[(h >> 3)] |=  1 << (h & 0x7);
	}
	IF_ADDR_UNLOCK(ifp);

	/* write the hashtable */
	for (i = 0; i < 8; i++)
		aue_csr_write_1(sc, AUE_MAR0 + i, hashtbl[i]);

	return;
}

static void
aue_reset_pegasus_II(struct aue_softc *sc)
{
	/* Magic constants taken from Linux driver. */
	aue_csr_write_1(sc, AUE_REG_1D, 0);
	aue_csr_write_1(sc, AUE_REG_7B, 2);
#if 0
	if ((sc->aue_flags & HAS_HOME_PNA) && mii_mode)
		aue_csr_write_1(sc, AUE_REG_81, 6);
	else
#endif
		aue_csr_write_1(sc, AUE_REG_81, 2);
}

static void
aue_reset(struct aue_softc *sc)
{
	int		i;

	AUE_SXASSERTLOCKED(sc);
	AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_RESETMAC);

	for (i = 0; i < AUE_TIMEOUT; i++) {
		if (!(aue_csr_read_1(sc, AUE_CTL1) & AUE_CTL1_RESETMAC))
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
	aue_csr_write_1(sc, AUE_GPIO0, AUE_GPIO_OUT0|AUE_GPIO_SEL0);
	aue_csr_write_1(sc, AUE_GPIO0, AUE_GPIO_OUT0|AUE_GPIO_SEL0|AUE_GPIO_SEL1);

	if (sc->aue_flags & LSYS) {
		/* Grrr. LinkSys has to be different from everyone else. */
		aue_csr_write_1(sc, AUE_GPIO0,
		    AUE_GPIO_SEL0 | AUE_GPIO_SEL1);
		aue_csr_write_1(sc, AUE_GPIO0,
		    AUE_GPIO_SEL0 | AUE_GPIO_SEL1 | AUE_GPIO_OUT0);
	}

	if (sc->aue_flags & PII)
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

	if (uaa->iface != NULL)
		return (UMATCH_NONE);

	return (aue_lookup(uaa->vendor, uaa->product) != NULL ?
		UMATCH_VENDOR_PRODUCT : UMATCH_NONE);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
USB_ATTACH(aue)
{
	USB_ATTACH_START(aue, sc, uaa);
	u_char			eaddr[ETHER_ADDR_LEN];
	struct ifnet		*ifp;
	usbd_interface_handle	iface;
	usbd_status		err;
	usb_interface_descriptor_t	*id;
	usb_endpoint_descriptor_t	*ed;
	int			i;

	sc->aue_dev = self;
	sc->aue_udev = uaa->device;
	sc->aue_unit = device_get_unit(self);

	if (usbd_set_config_no(sc->aue_udev, AUE_CONFIG_NO, 0)) {
		device_printf(self, "getting interface handle failed\n");
		USB_ATTACH_ERROR_RETURN;
	}

	err = usbd_device2interface_handle(uaa->device, AUE_IFACE_IDX, &iface);
	if (err) {
		device_printf(self, "getting interface handle failed\n");
		USB_ATTACH_ERROR_RETURN;
	}

	sc->aue_iface = iface;
	sc->aue_flags = aue_lookup(uaa->vendor, uaa->product)->aue_flags;

	sc->aue_product = uaa->product;
	sc->aue_vendor = uaa->vendor;

	id = usbd_get_interface_descriptor(sc->aue_iface);

	/* Find endpoints. */
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(iface, i);
		if (ed == NULL) {
			device_printf(self, "couldn't get ep %d\n", i);
			USB_ATTACH_ERROR_RETURN;
		}
		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->aue_ed[AUE_ENDPT_RX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK) {
			sc->aue_ed[AUE_ENDPT_TX] = ed->bEndpointAddress;
		} else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
			   UE_GET_XFERTYPE(ed->bmAttributes) == UE_INTERRUPT) {
			sc->aue_ed[AUE_ENDPT_INTR] = ed->bEndpointAddress;
		}
	}

	mtx_init(&sc->aue_mtx, device_get_nameunit(self), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	sx_init(&sc->aue_sx, device_get_nameunit(self));
	TASK_INIT(&sc->aue_task, 0, aue_task, sc);
	usb_ether_task_init(self, 0, &sc->aue_taskqueue);
	AUE_SXLOCK(sc);

	/* Reset the adapter. */
	aue_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	aue_read_eeprom(sc, (caddr_t)&eaddr, 0, 3, 0);

	ifp = sc->aue_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(self, "can not if_alloc()\n");
		AUE_SXUNLOCK(sc);
		mtx_destroy(&sc->aue_mtx);
		sx_destroy(&sc->aue_sx);
		usb_ether_task_destroy(&sc->aue_taskqueue);
		USB_ATTACH_ERROR_RETURN;
	}
	ifp->if_softc = sc;
	if_initname(ifp, "aue", sc->aue_unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = aue_ioctl;
	ifp->if_start = aue_start;
	ifp->if_init = aue_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

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
		device_printf(self, "MII without any PHY!\n");
		if_free(ifp);
		AUE_SXUNLOCK(sc);
		mtx_destroy(&sc->aue_mtx);
		sx_destroy(&sc->aue_sx);
		usb_ether_task_destroy(&sc->aue_taskqueue);
		USB_ATTACH_ERROR_RETURN;
	}

	sc->aue_qdat.ifp = ifp;
	sc->aue_qdat.if_rxstart = aue_rxstart;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);
	usb_register_netisr();
	sc->aue_dying = 0;
	sc->aue_link = 1;

	AUE_SXUNLOCK(sc);
	USB_ATTACH_SUCCESS_RETURN;
}

static int
aue_detach(device_t dev)
{
	struct aue_softc	*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	AUE_SXLOCK(sc);
	ifp = sc->aue_ifp;
	ether_ifdetach(ifp);
	sc->aue_dying = 1;
	AUE_SXUNLOCK(sc);
	callout_drain(&sc->aue_tick_callout);
	usb_ether_task_drain(&sc->aue_taskqueue, &sc->aue_task);
	usb_ether_task_destroy(&sc->aue_taskqueue);
	if_free(ifp);

	if (sc->aue_ep[AUE_ENDPT_TX] != NULL)
		usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_TX]);
	if (sc->aue_ep[AUE_ENDPT_RX] != NULL)
		usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_RX]);
#ifdef AUE_INTR_PIPE
	if (sc->aue_ep[AUE_ENDPT_INTR] != NULL)
		usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
#endif

	mtx_destroy(&sc->aue_mtx);
	sx_destroy(&sc->aue_sx);

	return (0);
}

static void
aue_rxstart(struct ifnet *ifp)
{
	struct aue_softc	*sc = ifp->if_softc;
	aue_task_sched(sc, AUE_TASK_RXSTART);
}

static void
aue_rxstart_thread(struct aue_softc *sc)
{
	struct ue_chain	*c;
	struct ifnet *ifp;

	ifp = sc->aue_ifp;

	sc = ifp->if_softc;
	AUE_SXASSERTLOCKED(sc);
	c = &sc->aue_cdata.ue_rx_chain[sc->aue_cdata.ue_rx_prod];

	c->ue_mbuf = usb_ether_newbuf();
	if (c->ue_mbuf == NULL) {
		device_printf(sc->aue_dev, "no memory for rx list -- packet "
		    "dropped!\n");
		ifp->if_ierrors++;
		AUE_UNLOCK(sc);
		return;
	}

	/* Setup new transfer. */
	usbd_setup_xfer(c->ue_xfer, sc->aue_ep[AUE_ENDPT_RX],
	    c, mtod(c->ue_mbuf, char *), UE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, aue_rxeof);
	usbd_transfer(c->ue_xfer);

	return;
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
static void
aue_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ue_chain	*c = priv;
	c->ue_status = status;
	aue_task_sched(c->ue_sc, AUE_TASK_RXEOF);
}

static void
aue_rxeof_thread(struct aue_softc *sc)
{
	struct ue_chain	*c = &(sc->aue_cdata.ue_rx_chain[0]);
        struct mbuf		*m;
        struct ifnet		*ifp;
	int			total_len = 0;
	struct aue_rxpkt	r;
	usbd_status		status = c->ue_status;


	AUE_SXASSERTLOCKED(sc);
	ifp = sc->aue_ifp;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		return;
	}

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			return;
		}
		if (usbd_ratecheck(&sc->aue_rx_notice))
			device_printf(sc->aue_dev, "usb error on rx: %s\n",
			    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_RX]);
		goto done;
	}

	usbd_get_xfer_status(c->ue_xfer, NULL, NULL, &total_len, NULL);

	if (total_len <= 4 + ETHER_CRC_LEN) {
		ifp->if_ierrors++;
		goto done;
	}

	m = c->ue_mbuf;
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
	m->m_pkthdr.rcvif = (void *)&sc->aue_qdat;
	m->m_pkthdr.len = m->m_len = total_len;

	/* Put the packet on the special USB input queue. */
	usb_ether_input(m);
	return;
done:

	/* Setup new transfer. */
	usbd_setup_xfer(c->ue_xfer, sc->aue_ep[AUE_ENDPT_RX],
	    c, mtod(c->ue_mbuf, char *), UE_BUFSZ, USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, aue_rxeof);
	usbd_transfer(c->ue_xfer);

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

static void
aue_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct ue_chain	*c = priv;
	c->ue_status = status;
	aue_task_sched(c->ue_sc, AUE_TASK_TXEOF);
}

static void
aue_txeof_thread(struct aue_softc *sc)
{
	struct ue_chain	*c = &(sc->aue_cdata.ue_tx_chain[0]);
	struct ifnet		*ifp;
	usbd_status		err, status;

	AUE_SXASSERTLOCKED(sc);
	status = c->ue_status;
	ifp = sc->aue_ifp;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED) {
			return;
		}
		device_printf(sc->aue_dev, "usb error on tx: %s\n",
		    usbd_errstr(status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->aue_ep[AUE_ENDPT_TX]);
		return;
	}

	sc->aue_timer = 0;
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

	return;
}

static void
aue_tick(void *xsc)
{
	struct aue_softc	*sc = xsc;

	aue_task_sched(sc, AUE_TASK_TICK);
}

static void
aue_tick_thread(struct aue_softc *sc)
{
	struct ifnet		*ifp;
	struct mii_data		*mii;

	AUE_SXASSERTLOCKED(sc);
	ifp = sc->aue_ifp;
	/*
	 * If a timer is set (non-zero) then decrement it
	 * and if it hits zero, then call the watchdog routine.
	 */
	if (sc->aue_timer != 0 && --sc->aue_timer == 0) {
		aue_watchdog(sc);
	}
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		return;
	}

	mii = GET_MII(sc);
	if (mii == NULL) {
		goto resched;
	}

	mii_tick(mii);
	if (!sc->aue_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->aue_link++;
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			aue_start_thread(sc);
	}
resched:
	(void) callout_reset(&sc->aue_tick_callout, hz, aue_tick, sc);
	return;
}

static int
aue_encap(struct aue_softc *sc, struct mbuf *m, int idx)
{
	int			total_len;
	struct ue_chain	*c;
	usbd_status		err;

	AUE_SXASSERTLOCKED(sc);

	c = &sc->aue_cdata.ue_tx_chain[idx];

	/*
	 * Copy the mbuf data into a contiguous buffer, leaving two
	 * bytes at the beginning to hold the frame length.
	 */
	m_copydata(m, 0, m->m_pkthdr.len, c->ue_buf + 2);
	c->ue_mbuf = m;

	total_len = m->m_pkthdr.len + 2;

	/*
	 * The ADMtek documentation says that the packet length is
	 * supposed to be specified in the first two bytes of the
	 * transfer, however it actually seems to ignore this info
	 * and base the frame size on the bulk transfer length.
	 */
	c->ue_buf[0] = (u_int8_t)m->m_pkthdr.len;
	c->ue_buf[1] = (u_int8_t)(m->m_pkthdr.len >> 8);

	usbd_setup_xfer(c->ue_xfer, sc->aue_ep[AUE_ENDPT_TX],
	    c, c->ue_buf, total_len, USBD_FORCE_SHORT_XFER,
	    10000, aue_txeof);

	/* Transmit */
	err = usbd_transfer(c->ue_xfer);
	if (err != USBD_IN_PROGRESS) {
		aue_stop(sc);
		return (EIO);
	}

	sc->aue_cdata.ue_tx_cnt++;

	return (0);
}


static void
aue_start(struct ifnet *ifp)
{
	struct aue_softc	*sc = ifp->if_softc;
	aue_task_sched(sc, AUE_TASK_START);
}

static void
aue_start_thread(struct aue_softc *sc)
{
	struct ifnet		*ifp = sc->aue_ifp;
	struct mbuf		*m_head = NULL;

	AUE_SXASSERTLOCKED(sc);

	if (!sc->aue_link) {
		return;
	}

	if (ifp->if_drv_flags & IFF_DRV_OACTIVE) {
		return;
	}

	IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
	if (m_head == NULL) {
		return;
	}

	if (aue_encap(sc, m_head, 0)) {
		IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
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
	sc->aue_timer = 5;

	return;
}

static void
aue_init(void *xsc)
{
	struct aue_softc	*sc = xsc;

	AUE_SXLOCK(sc);
	aue_init_body(sc);
	AUE_SXUNLOCK(sc);
}

static void
aue_init_body(struct aue_softc *sc)
{
	struct ifnet		*ifp = sc->aue_ifp;
	struct mii_data		*mii = GET_MII(sc);
	struct ue_chain	*c;
	usbd_status		err;
	int			i;

	AUE_SXASSERTLOCKED(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		return;
	}

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	aue_reset(sc);

	/* Set MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		aue_csr_write_1(sc, AUE_PAR0 + i, IF_LLADDR(sc->aue_ifp)[i]);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
	else
		AUE_CLRBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);

	/* Init TX ring. */
	if (usb_ether_tx_list_init(sc, &sc->aue_cdata,
	    sc->aue_udev) == ENOBUFS) {
		device_printf(sc->aue_dev, "tx list init failed\n");
		return;
	}

	/* Init RX ring. */
	if (usb_ether_rx_list_init(sc, &sc->aue_cdata,
	    sc->aue_udev) == ENOBUFS) {
		device_printf(sc->aue_dev, "rx list init failed\n");
		return;
	}


	/* Load the multicast filter. */
	aue_setmulti(sc);

	/* Enable RX and TX */
	aue_csr_write_1(sc, AUE_CTL0, AUE_CTL0_RXSTAT_APPEND | AUE_CTL0_RX_ENB);
	AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_TX_ENB);
	AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_EP3_CLR);

	mii_mediachg(mii);

	/* Open RX and TX pipes. */
	err = usbd_open_pipe(sc->aue_iface, sc->aue_ed[AUE_ENDPT_RX],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_RX]);
	if (err) {
		device_printf(sc->aue_dev, "open rx pipe failed: %s\n",
		    usbd_errstr(err));
		return;
	}
	err = usbd_open_pipe(sc->aue_iface, sc->aue_ed[AUE_ENDPT_TX],
	    USBD_EXCLUSIVE_USE, &sc->aue_ep[AUE_ENDPT_TX]);
	if (err) {
		device_printf(sc->aue_dev, "open tx pipe failed: %s\n",
		    usbd_errstr(err));
		return;
	}


	/* Start up the receive pipe. */
	for (i = 0; i < UE_RX_LIST_CNT; i++) {
		c = &sc->aue_cdata.ue_rx_chain[i];
		usbd_setup_xfer(c->ue_xfer, sc->aue_ep[AUE_ENDPT_RX],
		    c, mtod(c->ue_mbuf, char *), UE_BUFSZ,
	    	USBD_SHORT_XFER_OK, USBD_NO_TIMEOUT, aue_rxeof);
		usbd_transfer(c->ue_xfer);
	}

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_init(&sc->aue_tick_callout, 1);
	(void) callout_reset(&sc->aue_tick_callout, hz, aue_tick, sc);
	return;
}

/*
 * Set media options.
 */
static int
aue_ifmedia_upd(struct ifnet *ifp)
{
	struct aue_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = GET_MII(sc);

	sc->aue_link = 0;
	if (mii->mii_instance) {
		struct mii_softc	*miisc;
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			 mii_phy_reset(miisc);
	}
	mii_mediachg(mii);
	sc->aue_link = 1;

	return (0);
}

/*
 * Report current media status.
 */
static void
aue_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct aue_softc	*sc = ifp->if_softc;
	struct mii_data		*mii = GET_MII(sc);

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static int
aue_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct aue_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct mii_data		*mii;
	int			error = 0;

	/*
	 * This prevents recursion in the interface while it's
	 * being torn down.
	 */
	if (sc->aue_dying)
		return(0);

	AUE_GIANTLOCK();

	switch(command) {
	case SIOCSIFFLAGS:
		AUE_SXLOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->aue_if_flags & IFF_PROMISC)) {
				AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->aue_if_flags & IFF_PROMISC) {
				AUE_CLRBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
			} else if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				aue_init_body(sc);
			}
			sc->aue_dying = 0;
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				aue_stop(sc);
		}
		sc->aue_if_flags = ifp->if_flags;
		AUE_SXUNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		AUE_SXLOCK(sc);
		aue_setmulti(sc);
		AUE_SXUNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		AUE_SXLOCK(sc);
		mii = GET_MII(sc);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		AUE_SXUNLOCK(sc);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	AUE_GIANTUNLOCK();

	return (error);
}

static void
aue_watchdog(struct aue_softc *sc)
{
	struct ifnet		*ifp = sc->aue_ifp;
	struct ue_chain	*c;
	usbd_status		stat;

	AUE_SXASSERTLOCKED(sc);
	ifp->if_oerrors++;
	device_printf(sc->aue_dev, "watchdog timeout\n");

	c = &sc->aue_cdata.ue_tx_chain[0];
	usbd_get_xfer_status(c->ue_xfer, NULL, NULL, NULL, &stat);
	c->ue_status = stat;
	aue_txeof_thread(sc);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		aue_start_thread(sc);
	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
aue_stop(struct aue_softc *sc)
{
	usbd_status		err;
	struct ifnet		*ifp;

	AUE_SXASSERTLOCKED(sc);
	ifp = sc->aue_ifp;
	sc->aue_timer = 0;

	aue_csr_write_1(sc, AUE_CTL0, 0);
	aue_csr_write_1(sc, AUE_CTL1, 0);
	aue_reset(sc);
	sc->aue_dying = 1;

	/* Stop transfers. */
	if (sc->aue_ep[AUE_ENDPT_RX] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_RX]);
		if (err) {
			device_printf(sc->aue_dev,
			    "abort rx pipe failed: %s\n", usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_RX]);
		if (err) {
			device_printf(sc->aue_dev,
			    "close rx pipe failed: %s\n", usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_RX] = NULL;
	}

	if (sc->aue_ep[AUE_ENDPT_TX] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_TX]);
		if (err) {
			device_printf(sc->aue_dev,
			    "abort tx pipe failed: %s\n", usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_TX]);
		if (err) {
			device_printf(sc->aue_dev,
			    "close tx pipe failed: %s\n", usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_TX] = NULL;
	}

#ifdef AUE_INTR_PIPE
	if (sc->aue_ep[AUE_ENDPT_INTR] != NULL) {
		err = usbd_abort_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
		if (err) {
			device_printf(sc->aue_dev,
			    "abort intr pipe failed: %s\n", usbd_errstr(err));
		}
		err = usbd_close_pipe(sc->aue_ep[AUE_ENDPT_INTR]);
		if (err) {
			device_printf(sc->aue_dev,
			    "close intr pipe failed: %s\n", usbd_errstr(err));
		}
		sc->aue_ep[AUE_ENDPT_INTR] = NULL;
	}
#endif

	/* Free RX resources. */
	usb_ether_rx_list_free(&sc->aue_cdata);
	/* Free TX resources. */
	usb_ether_tx_list_free(&sc->aue_cdata);

#ifdef AUE_INTR_PIPE
	free(sc->aue_cdata.ue_ibuf, M_USBDEV);
	sc->aue_cdata.ue_ibuf = NULL;
#endif

	sc->aue_link = 0;

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
aue_shutdown(device_t dev)
{
	struct aue_softc	*sc;

	sc = device_get_softc(dev);
	AUE_SXLOCK(sc);
	sc->aue_dying++;
	aue_reset(sc);
	aue_stop(sc);
	AUE_SXUNLOCK(sc);

	return;
}

static void
aue_task_sched(struct aue_softc *sc, int task)
{

	AUE_LOCK(sc);
	sc->aue_deferedtasks |= task;
	usb_ether_task_enqueue(&sc->aue_taskqueue, &sc->aue_task);
	AUE_UNLOCK(sc);
}

/*
 * We defer all interrupt operations to this function.
 *
 * This allows us to do more complex operations, such as synchronous
 * usb io that normally would not be allowed from interrupt context.
 */
static void
aue_task(void *arg, int pending)
{
	struct aue_softc *sc = arg;
	int tasks;

	for ( ;; ) {
		AUE_LOCK(sc);
		tasks = sc->aue_deferedtasks;
		sc->aue_deferedtasks = 0;
		AUE_UNLOCK(sc);

		if (tasks == 0)
			break;

		AUE_GIANTLOCK();	// XXX: usb not giant safe
		AUE_SXLOCK(sc);
		if (sc->aue_dying) {
			AUE_SXUNLOCK(sc);
			break;
		}
		if ((tasks & AUE_TASK_TICK) != 0) {
			aue_tick_thread(sc);
		}
		if ((tasks & AUE_TASK_START) != 0) {
			aue_start_thread(sc);
		}
		if ((tasks & AUE_TASK_RXSTART) != 0) {
			aue_rxstart_thread(sc);
		}
		if ((tasks & AUE_TASK_RXEOF) != 0) {
			aue_rxeof_thread(sc);
		}
		if ((tasks & AUE_TASK_TXEOF) != 0) {
			aue_txeof_thread(sc);
		}
		AUE_SXUNLOCK(sc);
		AUE_GIANTUNLOCK();	// XXX: usb not giant safe
	}
}

