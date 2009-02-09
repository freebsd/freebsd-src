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
 * Registers are accessed using usb2_ether_do_request(). Packet
 * transfers are done using usb2_transfer() and friends.
 */

#include <dev/usb2/include/usb2_devid.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	USB_DEBUG_VAR aue_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_lookup.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/ethernet/usb2_ethernet.h>
#include <dev/usb2/ethernet/if_auereg.h>

#if USB_DEBUG
static int aue_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, aue, CTLFLAG_RW, 0, "USB aue");
SYSCTL_INT(_hw_usb2_aue, OID_AUTO, debug, CTLFLAG_RW, &aue_debug, 0,
    "Debug level");
#endif

/*
 * Various supported device vendors/products.
 */
static const struct usb2_device_id aue_devs[] = {
    {USB_VPI(USB_VENDOR_3COM, USB_PRODUCT_3COM_3C460B, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_DSB650TX_PNA, 0)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_UFE1000, AUE_FLAG_LSYS)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_XX10, 0)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_XX1, AUE_FLAG_PNA | AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_XX2, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_XX4, AUE_FLAG_PNA)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_XX5, AUE_FLAG_PNA)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_XX6, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_XX7, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_XX8, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ABOCOM, USB_PRODUCT_ABOCOM_XX9, AUE_FLAG_PNA)},
    {USB_VPI(USB_VENDOR_ACCTON, USB_PRODUCT_ACCTON_SS1001, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ACCTON, USB_PRODUCT_ACCTON_USB320_EC, 0)},
    {USB_VPI(USB_VENDOR_ADMTEK, USB_PRODUCT_ADMTEK_PEGASUSII_2, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ADMTEK, USB_PRODUCT_ADMTEK_PEGASUSII_3, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ADMTEK, USB_PRODUCT_ADMTEK_PEGASUSII_4, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ADMTEK, USB_PRODUCT_ADMTEK_PEGASUSII, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ADMTEK, USB_PRODUCT_ADMTEK_PEGASUS, AUE_FLAG_PNA | AUE_FLAG_DUAL_PHY)},
    {USB_VPI(USB_VENDOR_AEI, USB_PRODUCT_AEI_FASTETHERNET, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ALLIEDTELESYN, USB_PRODUCT_ALLIEDTELESYN_ATUSB100, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ATEN, USB_PRODUCT_ATEN_UC110T, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_BELKIN, USB_PRODUCT_BELKIN_USB2LAN, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_BILLIONTON, USB_PRODUCT_BILLIONTON_USB100, 0)},
    {USB_VPI(USB_VENDOR_BILLIONTON, USB_PRODUCT_BILLIONTON_USBE100, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_BILLIONTON, USB_PRODUCT_BILLIONTON_USBEL100, 0)},
    {USB_VPI(USB_VENDOR_BILLIONTON, USB_PRODUCT_BILLIONTON_USBLP100, AUE_FLAG_PNA)},
    {USB_VPI(USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB_TXS, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_COREGA, USB_PRODUCT_COREGA_FETHER_USB_TX, 0)},
    {USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650TX1, AUE_FLAG_LSYS)},
    {USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650TX2, AUE_FLAG_LSYS | AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650TX3, AUE_FLAG_LSYS | AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650TX4, AUE_FLAG_LSYS | AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650TX_PNA, AUE_FLAG_PNA)},
    {USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650TX, AUE_FLAG_LSYS)},
    {USB_VPI(USB_VENDOR_DLINK, USB_PRODUCT_DLINK_DSB650, AUE_FLAG_LSYS)},
    {USB_VPI(USB_VENDOR_ELCON, USB_PRODUCT_ELCON_PLAN, AUE_FLAG_PNA | AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ELECOM, USB_PRODUCT_ELECOM_LDUSB20, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ELECOM, USB_PRODUCT_ELECOM_LDUSBLTX, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_ELECOM, USB_PRODUCT_ELECOM_LDUSBTX0, 0)},
    {USB_VPI(USB_VENDOR_ELECOM, USB_PRODUCT_ELECOM_LDUSBTX1, AUE_FLAG_LSYS)},
    {USB_VPI(USB_VENDOR_ELECOM, USB_PRODUCT_ELECOM_LDUSBTX2, 0)},
    {USB_VPI(USB_VENDOR_ELECOM, USB_PRODUCT_ELECOM_LDUSBTX3, AUE_FLAG_LSYS)},
    {USB_VPI(USB_VENDOR_ELSA, USB_PRODUCT_ELSA_USB2ETHERNET, 0)},
    {USB_VPI(USB_VENDOR_GIGABYTE, USB_PRODUCT_GIGABYTE_GNBR402W, 0)},
    {USB_VPI(USB_VENDOR_HAWKING, USB_PRODUCT_HAWKING_UF100, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_HP, USB_PRODUCT_HP_HN210E, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_IODATA, USB_PRODUCT_IODATA_USBETTXS, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_IODATA, USB_PRODUCT_IODATA_USBETTX, 0)},
    {USB_VPI(USB_VENDOR_KINGSTON, USB_PRODUCT_KINGSTON_KNU101TX, 0)},
    {USB_VPI(USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB100H1, AUE_FLAG_LSYS | AUE_FLAG_PNA)},
    {USB_VPI(USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB100TX, AUE_FLAG_LSYS)},
    {USB_VPI(USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB10TA, AUE_FLAG_LSYS)},
    {USB_VPI(USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB10TX1, AUE_FLAG_LSYS | AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB10TX2, AUE_FLAG_LSYS | AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_LINKSYS, USB_PRODUCT_LINKSYS_USB10T, AUE_FLAG_LSYS)},
    {USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_LUA2TX5, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_LUATX1, 0)},
    {USB_VPI(USB_VENDOR_MELCO, USB_PRODUCT_MELCO_LUATX5, 0)},
    {USB_VPI(USB_VENDOR_MICROSOFT, USB_PRODUCT_MICROSOFT_MN110, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_NETGEAR, USB_PRODUCT_NETGEAR_FA101, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_SIEMENS, USB_PRODUCT_SIEMENS_SPEEDSTREAM, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_SIIG2, USB_PRODUCT_SIIG2_USBTOETHER, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_SMARTBRIDGES, USB_PRODUCT_SMARTBRIDGES_SMARTNIC, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_SMC, USB_PRODUCT_SMC_2202USB, 0)},
    {USB_VPI(USB_VENDOR_SMC, USB_PRODUCT_SMC_2206USB, AUE_FLAG_PII)},
    {USB_VPI(USB_VENDOR_SOHOWARE, USB_PRODUCT_SOHOWARE_NUB100, 0)},
    {USB_VPI(USB_VENDOR_SOHOWARE, USB_PRODUCT_SOHOWARE_NUB110, AUE_FLAG_PII)},
};

/* prototypes */

static device_probe_t aue_probe;
static device_attach_t aue_attach;
static device_detach_t aue_detach;
static device_shutdown_t aue_shutdown;
static miibus_readreg_t aue_miibus_readreg;
static miibus_writereg_t aue_miibus_writereg;
static miibus_statchg_t aue_miibus_statchg;

static usb2_callback_t aue_intr_callback;
static usb2_callback_t aue_bulk_read_callback;
static usb2_callback_t aue_bulk_write_callback;

static usb2_ether_fn_t aue_attach_post;
static usb2_ether_fn_t aue_init;
static usb2_ether_fn_t aue_stop;
static usb2_ether_fn_t aue_start;
static usb2_ether_fn_t aue_tick;
static usb2_ether_fn_t aue_setmulti;
static usb2_ether_fn_t aue_setpromisc;

static uint8_t	aue_csr_read_1(struct aue_softc *, uint16_t);
static uint16_t	aue_csr_read_2(struct aue_softc *, uint16_t);
static void	aue_csr_write_1(struct aue_softc *, uint16_t, uint8_t);
static void	aue_csr_write_2(struct aue_softc *, uint16_t, uint16_t);
static void	aue_eeprom_getword(struct aue_softc *, int, uint16_t *);
static void	aue_read_eeprom(struct aue_softc *, uint8_t *, uint16_t,
		    uint16_t);
static void	aue_reset(struct aue_softc *);
static void	aue_reset_pegasus_II(struct aue_softc *);

static int	aue_ifmedia_upd(struct ifnet *);
static void	aue_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static const struct usb2_config aue_config[AUE_N_TRANSFER] = {

	[AUE_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.mh.bufsize = (MCLBYTES + 2),
		.mh.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.mh.callback = aue_bulk_write_callback,
		.mh.timeout = 10000,	/* 10 seconds */
	},

	[AUE_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.bufsize = (MCLBYTES + 4 + ETHER_CRC_LEN),
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.callback = aue_bulk_read_callback,
	},

	[AUE_INTR_DT_RD] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.mh.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.mh.bufsize = 0,	/* use wMaxPacketSize */
		.mh.callback = aue_intr_callback,
	},
};

static device_method_t aue_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, aue_probe),
	DEVMETHOD(device_attach, aue_attach),
	DEVMETHOD(device_detach, aue_detach),
	DEVMETHOD(device_shutdown, aue_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg, aue_miibus_readreg),
	DEVMETHOD(miibus_writereg, aue_miibus_writereg),
	DEVMETHOD(miibus_statchg, aue_miibus_statchg),

	{0, 0}
};

static driver_t aue_driver = {
	.name = "aue",
	.methods = aue_methods,
	.size = sizeof(struct aue_softc)
};

static devclass_t aue_devclass;

DRIVER_MODULE(aue, ushub, aue_driver, aue_devclass, NULL, 0);
DRIVER_MODULE(miibus, aue, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(aue, usb2_ethernet, 1, 1, 1);
MODULE_DEPEND(aue, usb2_core, 1, 1, 1);
MODULE_DEPEND(aue, ether, 1, 1, 1);
MODULE_DEPEND(aue, miibus, 1, 1, 1);

static const struct usb2_ether_methods aue_ue_methods = {
	.ue_attach_post = aue_attach_post,
	.ue_start = aue_start,
	.ue_init = aue_init,
	.ue_stop = aue_stop,
	.ue_tick = aue_tick,
	.ue_setmulti = aue_setmulti,
	.ue_setpromisc = aue_setpromisc,
	.ue_mii_upd = aue_ifmedia_upd,
	.ue_mii_sts = aue_ifmedia_sts,
};

#define	AUE_SETBIT(sc, reg, x) \
	aue_csr_write_1(sc, reg, aue_csr_read_1(sc, reg) | (x))

#define	AUE_CLRBIT(sc, reg, x) \
	aue_csr_write_1(sc, reg, aue_csr_read_1(sc, reg) & ~(x))

static uint8_t
aue_csr_read_1(struct aue_softc *sc, uint16_t reg)
{
	struct usb2_device_request req;
	usb2_error_t err;
	uint8_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	err = usb2_ether_do_request(&sc->sc_ue, &req, &val, 1000);
	if (err)
		return (0);
	return (val);
}

static uint16_t
aue_csr_read_2(struct aue_softc *sc, uint16_t reg)
{
	struct usb2_device_request req;
	usb2_error_t err;
	uint16_t val;

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = AUE_UR_READREG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	err = usb2_ether_do_request(&sc->sc_ue, &req, &val, 1000);
	if (err)
		return (0);
	return (le16toh(val));
}

static void
aue_csr_write_1(struct aue_softc *sc, uint16_t reg, uint8_t val)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	req.wValue[0] = val;
	req.wValue[1] = 0;
	USETW(req.wIndex, reg);
	USETW(req.wLength, 1);

	if (usb2_ether_do_request(&sc->sc_ue, &req, &val, 1000)) {
		/* error ignored */
	}
}

static void
aue_csr_write_2(struct aue_softc *sc, uint16_t reg, uint16_t val)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AUE_UR_WRITEREG;
	USETW(req.wValue, val);
	USETW(req.wIndex, reg);
	USETW(req.wLength, 2);

	val = htole16(val);

	if (usb2_ether_do_request(&sc->sc_ue, &req, &val, 1000)) {
		/* error ignored */
	}
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void
aue_eeprom_getword(struct aue_softc *sc, int addr, uint16_t *dest)
{
	int i;
	uint16_t word = 0;

	aue_csr_write_1(sc, AUE_EE_REG, addr);
	aue_csr_write_1(sc, AUE_EE_CTL, AUE_EECTL_READ);

	for (i = 0; i != AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_EE_CTL) & AUE_EECTL_DONE)
			break;
		if (usb2_ether_pause(&sc->sc_ue, hz / 100))
			break;
	}

	if (i == AUE_TIMEOUT)
		device_printf(sc->sc_ue.ue_dev, "EEPROM read timed out\n");

	word = aue_csr_read_2(sc, AUE_EE_DATA);
	*dest = word;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void
aue_read_eeprom(struct aue_softc *sc, uint8_t *dest,
    uint16_t off, uint16_t len)
{
	uint16_t *ptr = (uint16_t *)dest;
	int i;

	for (i = 0; i != len; i++, ptr++)
		aue_eeprom_getword(sc, off + i, ptr);
}

static int
aue_miibus_readreg(device_t dev, int phy, int reg)
{
	struct aue_softc *sc = device_get_softc(dev);
	int i, locked;
	uint16_t val = 0;

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		AUE_LOCK(sc);

	/*
	 * The Am79C901 HomePNA PHY actually contains two transceivers: a 1Mbps
	 * HomePNA PHY and a 10Mbps full/half duplex ethernet PHY with NWAY
	 * autoneg. However in the ADMtek adapter, only the 1Mbps PHY is
	 * actually connected to anything, so we ignore the 10Mbps one. It
	 * happens to be configured for MII address 3, so we filter that out.
	 */
	if (sc->sc_flags & AUE_FLAG_DUAL_PHY) {
		if (phy == 3)
			goto done;
#if 0
		if (phy != 1)
			goto done;
#endif
	}
	aue_csr_write_1(sc, AUE_PHY_ADDR, phy);
	aue_csr_write_1(sc, AUE_PHY_CTL, reg | AUE_PHYCTL_READ);

	for (i = 0; i != AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_PHY_CTL) & AUE_PHYCTL_DONE)
			break;
		if (usb2_ether_pause(&sc->sc_ue, hz / 100))
			break;
	}

	if (i == AUE_TIMEOUT)
		device_printf(sc->sc_ue.ue_dev, "MII read timed out\n");

	val = aue_csr_read_2(sc, AUE_PHY_DATA);

done:
	if (!locked)
		AUE_UNLOCK(sc);
	return (val);
}

static int
aue_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct aue_softc *sc = device_get_softc(dev);
	int i;
	int locked;

	if (phy == 3)
		return (0);

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		AUE_LOCK(sc);

	aue_csr_write_2(sc, AUE_PHY_DATA, data);
	aue_csr_write_1(sc, AUE_PHY_ADDR, phy);
	aue_csr_write_1(sc, AUE_PHY_CTL, reg | AUE_PHYCTL_WRITE);

	for (i = 0; i != AUE_TIMEOUT; i++) {
		if (aue_csr_read_1(sc, AUE_PHY_CTL) & AUE_PHYCTL_DONE)
			break;
		if (usb2_ether_pause(&sc->sc_ue, hz / 100))
			break;
	}

	if (i == AUE_TIMEOUT)
		device_printf(sc->sc_ue.ue_dev, "MII read timed out\n");

	if (!locked)
		AUE_UNLOCK(sc);
	return (0);
}

static void
aue_miibus_statchg(device_t dev)
{
	struct aue_softc *sc = device_get_softc(dev);
	struct mii_data *mii = GET_MII(sc);
	int locked;

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		AUE_LOCK(sc);

	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_RX_ENB | AUE_CTL0_TX_ENB);
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX)
		AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);
	else
		AUE_CLRBIT(sc, AUE_CTL1, AUE_CTL1_SPEEDSEL);

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
	if (sc->sc_flags & AUE_FLAG_LSYS) {
		uint16_t auxmode;

		auxmode = aue_miibus_readreg(dev, 0, 0x1b);
		aue_miibus_writereg(dev, 0, 0x1b, auxmode | 0x04);
	}
	if (!locked)
		AUE_UNLOCK(sc);
}

#define	AUE_BITS	6
static void
aue_setmulti(struct usb2_ether *ue)
{
	struct aue_softc *sc = usb2_ether_getsc(ue);
	struct ifnet *ifp = usb2_ether_getifp(ue);
	struct ifmultiaddr *ifma;
	uint32_t h = 0;
	uint32_t i;
	uint8_t hashtbl[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	AUE_LOCK_ASSERT(sc, MA_OWNED);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);
		return;
	}

	AUE_CLRBIT(sc, AUE_CTL0, AUE_CTL0_ALLMULTI);

	/* now program new ones */
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_le(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) & ((1 << AUE_BITS) - 1);
		hashtbl[(h >> 3)] |=  1 << (h & 0x7);
	}
	IF_ADDR_UNLOCK(ifp);

	/* write the hashtable */
	for (i = 0; i != 8; i++)
		aue_csr_write_1(sc, AUE_MAR0 + i, hashtbl[i]);
}

static void
aue_reset_pegasus_II(struct aue_softc *sc)
{
	/* Magic constants taken from Linux driver. */
	aue_csr_write_1(sc, AUE_REG_1D, 0);
	aue_csr_write_1(sc, AUE_REG_7B, 2);
#if 0
	if ((sc->sc_flags & HAS_HOME_PNA) && mii_mode)
		aue_csr_write_1(sc, AUE_REG_81, 6);
	else
#endif
		aue_csr_write_1(sc, AUE_REG_81, 2);
}

static void
aue_reset(struct aue_softc *sc)
{
	int i;

	AUE_SETBIT(sc, AUE_CTL1, AUE_CTL1_RESETMAC);

	for (i = 0; i != AUE_TIMEOUT; i++) {
		if (!(aue_csr_read_1(sc, AUE_CTL1) & AUE_CTL1_RESETMAC))
			break;
		if (usb2_ether_pause(&sc->sc_ue, hz / 100))
			break;
	}

	if (i == AUE_TIMEOUT)
		device_printf(sc->sc_ue.ue_dev, "reset failed\n");

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

	if (sc->sc_flags & AUE_FLAG_LSYS) {
		/* Grrr. LinkSys has to be different from everyone else. */
		aue_csr_write_1(sc, AUE_GPIO0, AUE_GPIO_SEL0|AUE_GPIO_SEL1);
		aue_csr_write_1(sc, AUE_GPIO0,
		    AUE_GPIO_SEL0|AUE_GPIO_SEL1|AUE_GPIO_OUT0);
	}
	if (sc->sc_flags & AUE_FLAG_PII)
		aue_reset_pegasus_II(sc);

	/* Wait a little while for the chip to get its brains in order: */
	usb2_ether_pause(&sc->sc_ue, hz / 100);
}

static void
aue_attach_post(struct usb2_ether *ue)
{
	struct aue_softc *sc = usb2_ether_getsc(ue);

	/* reset the adapter */
	aue_reset(sc);

	/* get station address from the EEPROM */
	aue_read_eeprom(sc, ue->ue_eaddr, 0, 3);
}

/*
 * Probe for a Pegasus chip.
 */
static int
aue_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb2_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != AUE_CONFIG_INDEX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != AUE_IFACE_IDX)
		return (ENXIO);
	/*
	 * Belkin USB Bluetooth dongles of the F8T012xx1 model series conflict
	 * with older Belkin USB2LAN adapters.  Skip if_aue if we detect one of
	 * the devices that look like Bluetooth adapters.
	 */
	if (uaa->info.idVendor == USB_VENDOR_BELKIN &&
	    uaa->info.idProduct == USB_PRODUCT_BELKIN_F8T012 &&
	    uaa->info.bcdDevice == 0x0413)
		return (ENXIO);

	return (usb2_lookup_id_by_uaa(aue_devs, sizeof(aue_devs), uaa));
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
aue_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct aue_softc *sc = device_get_softc(dev);
	struct usb2_ether *ue = &sc->sc_ue;
	uint8_t iface_index;
	int error;

	sc->sc_flags = USB_GET_DRIVER_INFO(uaa);

	if (uaa->info.bcdDevice >= 0x0201) {
		/* XXX currently undocumented */
		sc->sc_flags |= AUE_FLAG_VER_2;
	}

	device_set_usb2_desc(dev);
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	iface_index = AUE_IFACE_IDX;
	error = usb2_transfer_setup(uaa->device, &iface_index,
	    sc->sc_xfer, aue_config, AUE_N_TRANSFER,
	    sc, &sc->sc_mtx);
	if (error) {
		device_printf(dev, "allocating USB transfers failed!\n");
		goto detach;
	}

	ue->ue_sc = sc;
	ue->ue_dev = dev;
	ue->ue_udev = uaa->device;
	ue->ue_mtx = &sc->sc_mtx;
	ue->ue_methods = &aue_ue_methods;

	error = usb2_ether_ifattach(ue);
	if (error) {
		device_printf(dev, "could not attach interface\n");
		goto detach;
	}
	return (0);			/* success */

detach:
	aue_detach(dev);
	return (ENXIO);			/* failure */
}

static int
aue_detach(device_t dev)
{
	struct aue_softc *sc = device_get_softc(dev);
	struct usb2_ether *ue = &sc->sc_ue;

	usb2_transfer_unsetup(sc->sc_xfer, AUE_N_TRANSFER);
	usb2_ether_ifdetach(ue);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
aue_intr_callback(struct usb2_xfer *xfer)
{
	struct aue_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = usb2_ether_getifp(&sc->sc_ue);
	struct aue_intrpkt pkt;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:

		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) &&
		    xfer->actlen >= sizeof(pkt)) {

			usb2_copy_out(xfer->frbuffers, 0, &pkt, sizeof(pkt));

			if (pkt.aue_txstat0)
				ifp->if_oerrors++;
			if (pkt.aue_txstat0 & (AUE_TXSTAT0_LATECOLL &
			    AUE_TXSTAT0_EXCESSCOLL))
				ifp->if_collisions++;
		}
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		xfer->frlengths[0] = xfer->max_data_length;
		usb2_start_hardware(xfer);
		return;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		return;
	}
}

static void
aue_bulk_read_callback(struct usb2_xfer *xfer)
{
	struct aue_softc *sc = xfer->priv_sc;
	struct usb2_ether *ue = &sc->sc_ue;
	struct ifnet *ifp = usb2_ether_getifp(ue);
	struct aue_rxpkt stat;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "received %d bytes\n", xfer->actlen);

		if (sc->sc_flags & AUE_FLAG_VER_2) {

			if (xfer->actlen == 0) {
				ifp->if_ierrors++;
				goto tr_setup;
			}
		} else {

			if (xfer->actlen <= (sizeof(stat) + ETHER_CRC_LEN)) {
				ifp->if_ierrors++;
				goto tr_setup;
			}
			usb2_copy_out(xfer->frbuffers,
			    xfer->actlen - sizeof(stat), &stat, sizeof(stat));

			/*
			 * turn off all the non-error bits in the rx status
			 * word:
			 */
			stat.aue_rxstat &= AUE_RXSTAT_MASK;
			if (stat.aue_rxstat) {
				ifp->if_ierrors++;
				goto tr_setup;
			}
			/* No errors; receive the packet. */
			xfer->actlen -= (sizeof(stat) + ETHER_CRC_LEN);
		}
		usb2_ether_rxbuf(ue, xfer->frbuffers, 0, xfer->actlen);

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		xfer->frlengths[0] = xfer->max_data_length;
		usb2_start_hardware(xfer);
		usb2_ether_rxflush(ue);
		return;

	default:			/* Error */
		DPRINTF("bulk read error, %s\n",
		    usb2_errstr(xfer->error));

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		return;
	}
}

static void
aue_bulk_write_callback(struct usb2_xfer *xfer)
{
	struct aue_softc *sc = xfer->priv_sc;
	struct ifnet *ifp = usb2_ether_getifp(&sc->sc_ue);
	struct mbuf *m;
	uint8_t buf[2];

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(11, "transfer of %d bytes complete\n", xfer->actlen);
		ifp->if_opackets++;

		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		if ((sc->sc_flags & AUE_FLAG_LINK) == 0) {
			/*
			 * don't send anything if there is no link !
			 */
			return;
		}
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);

		if (m == NULL)
			return;
		if (m->m_pkthdr.len > MCLBYTES)
			m->m_pkthdr.len = MCLBYTES;
		if (sc->sc_flags & AUE_FLAG_VER_2) {

			xfer->frlengths[0] = m->m_pkthdr.len;

			usb2_m_copy_in(xfer->frbuffers, 0,
			    m, 0, m->m_pkthdr.len);

		} else {

			xfer->frlengths[0] = (m->m_pkthdr.len + 2);

			/*
		         * The ADMtek documentation says that the
		         * packet length is supposed to be specified
		         * in the first two bytes of the transfer,
		         * however it actually seems to ignore this
		         * info and base the frame size on the bulk
		         * transfer length.
		         */
			buf[0] = (uint8_t)(m->m_pkthdr.len);
			buf[1] = (uint8_t)(m->m_pkthdr.len >> 8);

			usb2_copy_in(xfer->frbuffers, 0, buf, 2);

			usb2_m_copy_in(xfer->frbuffers, 2,
			    m, 0, m->m_pkthdr.len);
		}

		/*
		 * if there's a BPF listener, bounce a copy
		 * of this frame to him:
		 */
		BPF_MTAP(ifp, m);

		m_freem(m);

		usb2_start_hardware(xfer);
		return;

	default:			/* Error */
		DPRINTFN(11, "transfer error, %s\n",
		    usb2_errstr(xfer->error));

		ifp->if_oerrors++;

		if (xfer->error != USB_ERR_CANCELLED) {
			/* try to clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		return;
	}
}

static void
aue_tick(struct usb2_ether *ue)
{
	struct aue_softc *sc = usb2_ether_getsc(ue);
	struct mii_data *mii = GET_MII(sc);

	AUE_LOCK_ASSERT(sc, MA_OWNED);

	mii_tick(mii);
	if ((sc->sc_flags & AUE_FLAG_LINK) == 0
	    && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->sc_flags |= AUE_FLAG_LINK;
		aue_start(ue);
	}
}

static void
aue_start(struct usb2_ether *ue)
{
	struct aue_softc *sc = usb2_ether_getsc(ue);

	/*
	 * start the USB transfers, if not already started:
	 */
	usb2_transfer_start(sc->sc_xfer[AUE_INTR_DT_RD]);
	usb2_transfer_start(sc->sc_xfer[AUE_BULK_DT_RD]);
	usb2_transfer_start(sc->sc_xfer[AUE_BULK_DT_WR]);
}

static void
aue_init(struct usb2_ether *ue)
{
	struct aue_softc *sc = usb2_ether_getsc(ue);
	struct ifnet *ifp = usb2_ether_getifp(ue);
	int i;

	AUE_LOCK_ASSERT(sc, MA_OWNED);

	/*
	 * Cancel pending I/O
	 */
	aue_reset(sc);

	/* Set MAC address */
	for (i = 0; i != ETHER_ADDR_LEN; i++)
		aue_csr_write_1(sc, AUE_PAR0 + i, IF_LLADDR(ifp)[i]);

	/* update promiscuous setting */
	aue_setpromisc(ue);

	/* Load the multicast filter. */
	aue_setmulti(ue);

	/* Enable RX and TX */
	aue_csr_write_1(sc, AUE_CTL0, AUE_CTL0_RXSTAT_APPEND | AUE_CTL0_RX_ENB);
	AUE_SETBIT(sc, AUE_CTL0, AUE_CTL0_TX_ENB);
	AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_EP3_CLR);

	usb2_transfer_set_stall(sc->sc_xfer[AUE_BULK_DT_WR]);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	aue_start(ue);
}

static void
aue_setpromisc(struct usb2_ether *ue)
{
	struct aue_softc *sc = usb2_ether_getsc(ue);
	struct ifnet *ifp = usb2_ether_getifp(ue);

	AUE_LOCK_ASSERT(sc, MA_OWNED);

	/* if we want promiscuous mode, set the allframes bit: */
	if (ifp->if_flags & IFF_PROMISC)
		AUE_SETBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
	else
		AUE_CLRBIT(sc, AUE_CTL2, AUE_CTL2_RX_PROMISC);
}

/*
 * Set media options.
 */
static int
aue_ifmedia_upd(struct ifnet *ifp)
{
	struct aue_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);

	AUE_LOCK_ASSERT(sc, MA_OWNED);

        sc->sc_flags &= ~AUE_FLAG_LINK;
	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);
	return (0);
}

/*
 * Report current media status.
 */
static void
aue_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct aue_softc *sc = ifp->if_softc;
	struct mii_data *mii = GET_MII(sc);

	AUE_LOCK(sc);
	mii_pollstat(mii);
	AUE_UNLOCK(sc);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
aue_stop(struct usb2_ether *ue)
{
	struct aue_softc *sc = usb2_ether_getsc(ue);
	struct ifnet *ifp = usb2_ether_getifp(ue);

	AUE_LOCK_ASSERT(sc, MA_OWNED);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	sc->sc_flags &= ~AUE_FLAG_LINK;

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usb2_transfer_stop(sc->sc_xfer[AUE_BULK_DT_WR]);
	usb2_transfer_stop(sc->sc_xfer[AUE_BULK_DT_RD]);
	usb2_transfer_stop(sc->sc_xfer[AUE_INTR_DT_RD]);

	aue_csr_write_1(sc, AUE_CTL0, 0);
	aue_csr_write_1(sc, AUE_CTL1, 0);
	aue_reset(sc);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static int
aue_shutdown(device_t dev)
{
	struct aue_softc *sc = device_get_softc(dev);

	usb2_ether_ifshutdown(&sc->sc_ue);

	return (0);
}
