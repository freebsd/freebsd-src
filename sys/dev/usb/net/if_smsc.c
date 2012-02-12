/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * SMSC LAN9xxx devices.
 *
 */

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include "usbdevs.h"

#define	USB_DEBUG_VAR smsc_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/net/usb_ethernet.h>
#include <dev/usb/net/if_smscreg.h>
#include <dev/usb/usb_device.h>

/*
 * From looking at the Linux SMSC logs I believe the LAN95xx devices have
 * the following endpoints:
 *   Endpoints In 1 Out 2 Int 3 
 *
 */
enum {
	SMSC_BULK_DT_RD,
	SMSC_BULK_DT_WR,
	SMSC_INTR_DT_RD,
	SMSC_N_TRANSFER,
};

#ifdef USB_DEBUG
static int smsc_debug = 0;

SYSCTL_NODE(_hw_usb, OID_AUTO, smsc, CTLFLAG_RW, 0, "USB smsc");
SYSCTL_INT(_hw_usb_smsc, OID_AUTO, debug, CTLFLAG_RW, &smsc_debug, 0,
    "Debug level");
#endif

/*
 * Various supported device vendors/products.
 */
static const struct usb_device_id smsc_devs[] = {
#define	SMSC_DEV(p,i) { USB_VPI(USB_VENDOR_SMC2, USB_PRODUCT_SMC2_##p, i) }
	SMSC_DEV(LAN9514_ETH, 0),
#undef SMSC_DEV
};

struct smsc_softc {
	struct usb_ether  sc_ue;
	struct mtx        sc_mtx;
	struct usb_xfer  *sc_xfer[SMSC_N_TRANSFER];
	int               sc_phyno;

	/* The following stores the settings in the mac control (SMSC_REG_MAC_CR) register */
	uint32_t          sc_mac_cr;

	uint32_t          sc_flags;
#define	SMSC_FLAG_LINK      0x0001
#define	SMSC_FLAG_LAN9514   0x1000	/* LAN9514 */

};

#define	SMSC_LOCK(_sc)             mtx_lock(&(_sc)->sc_mtx)
#define	SMSC_UNLOCK(_sc)           mtx_unlock(&(_sc)->sc_mtx)
#define	SMSC_LOCK_ASSERT(_sc, t)   mtx_assert(&(_sc)->sc_mtx, t)

#define SMSC_TIMEOUT	100	/* 10*ms */

static device_probe_t smsc_probe;
static device_attach_t smsc_attach;
static device_detach_t smsc_detach;

static usb_callback_t smsc_bulk_read_callback;
static usb_callback_t smsc_bulk_write_callback;
static usb_callback_t smsc_intr_callback;

static miibus_readreg_t smsc_miibus_readreg;
static miibus_writereg_t smsc_miibus_writereg;
static miibus_statchg_t smsc_miibus_statchg;

static uether_fn_t smsc_attach_post;
static uether_fn_t smsc_init;
static uether_fn_t smsc_stop;
static uether_fn_t smsc_start;
static uether_fn_t smsc_tick;
static uether_fn_t smsc_setmulti;
static uether_fn_t smsc_setpromisc;

static int	smsc_ifmedia_upd(struct ifnet *);
static void	smsc_ifmedia_sts(struct ifnet *, struct ifmediareq *);

static int smsc_chip_init(struct smsc_softc *sc);

static const struct usb_config smsc_config[SMSC_N_TRANSFER] = {

	[SMSC_BULK_DT_WR] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.frames = 16,
		.bufsize = 16 * MCLBYTES,
		.flags = {.pipe_bof = 1,.force_short_xfer = 1,},
		.callback = smsc_bulk_write_callback,
		.timeout = 10000,	/* 10 seconds */
	},

	[SMSC_BULK_DT_RD] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = 18944,	/* bytes */
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.callback = smsc_bulk_read_callback,
		.timeout = 0,	/* no timeout */
	},

	[SMSC_INTR_DT_RD] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.bufsize = 0,	/* use wMaxPacketSize */
		.callback = smsc_intr_callback,
	},
};

static const struct usb_ether_methods smsc_ue_methods = {
	.ue_attach_post = smsc_attach_post,
	.ue_start = smsc_start,
	.ue_init = smsc_init,
	.ue_stop = smsc_stop,
	.ue_tick = smsc_tick,
	.ue_setmulti = smsc_setmulti,
	.ue_setpromisc = smsc_setpromisc,
	.ue_mii_upd = smsc_ifmedia_upd,
	.ue_mii_sts = smsc_ifmedia_sts,
};

/**
 *	smsc_read_reg - Reads a 32-bit register on the device
 *	@sc: driver soft context
 *
 *	
 *
 *	RETURNS:
 *	Register value or 0 if read failed
 */
static uint32_t
smsc_read_reg(struct smsc_softc *sc, uint32_t off)
{
	struct usb_device_request req;
	uint32_t buf;
	usb_error_t err;

	SMSC_LOCK_ASSERT(sc, MA_OWNED);

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = SMSC_UR_READ;
	USETW(req.wValue, 0);
	USETW(req.wIndex, off);
	USETW(req.wLength, 4);

	err = uether_do_request(&sc->sc_ue, &req, &buf, 1000);
	if (err != 0) {
		device_printf(sc->sc_ue.ue_dev, "Failed to read register 0x%0x, err = %d\n", off, err);
		return (0);
	}

	return le32toh(buf);
}

/**
 *	smsc_write_reg - Reads a 32-bit register on the device
 *	@sc: driver soft context
 *
 *	
 *
 *	RETURNS:
 *	Nothing
 */
static void
smsc_write_reg(struct smsc_softc *sc, uint32_t off, uint32_t data)
{
	struct usb_device_request req;
	uint32_t buf;
	usb_error_t err;

	SMSC_LOCK_ASSERT(sc, MA_OWNED);
	
	buf = htole32(data);

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = SMSC_UR_WRITE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, off);
	USETW(req.wLength, 4);

	err = uether_do_request(&sc->sc_ue, &req, &buf, 1000);
	if (err != 0)
		device_printf(sc->sc_ue.ue_dev, "Failed to write register 0x%0x, err = %d\n", off, err);

}

/**
 *	smsc_wait_for_bits - Reads data from eeprom
 *	@sc: driver soft context
 *	@reg: register number
 *	@bits: bit to wait for to clear
 *
 *	RETURNS:
 *	0 if succeeded, -1 if timed out
 */
static int
smsc_wait_for_bits(struct smsc_softc *sc, uint32_t reg, uint32_t bits)
{
	int i;
	uint32_t val;

	for (i = 0; i != SMSC_TIMEOUT; i++) {
		val = smsc_read_reg(sc, SMSC_REG_E2P_CMD);
		if (!(val & bits))
			break;
		if (uether_pause(&sc->sc_ue, hz / 100))
			break;
	}

	if (i == SMSC_TIMEOUT)
		return (-1);

	return (0);
}

/**
 *	smsc_read_eeprom - Reads data from eeprom
 *	@sc: driver soft context
 *	@off: EEPROM offset
 *	@data: memory to read data to
 *	@length: read length bytes
 *
 *	
 *
 *	RETURNS:
 *	0 on success, -1 otherwise
 */
static int
smsc_read_eeprom(struct smsc_softc *sc, uint32_t off, uint8_t *data, int length)
{
	int timedout, i;
	uint32_t val;

	if (smsc_wait_for_bits(sc, SMSC_REG_E2P_CMD, E2P_CMD_BUSY)) {
		device_printf(sc->sc_ue.ue_dev, "Timed-out waiting for busy EEPROM\n");
		return (-1);
	}

	for (i = 0; i < length; i++) {
		smsc_write_reg(sc, SMSC_REG_E2P_CMD,
			E2P_CMD_BUSY | E2P_CMD_READ | ((off+i) & E2P_CMD_ADDR));

		timedout = smsc_wait_for_bits(sc, SMSC_REG_E2P_CMD, E2P_CMD_BUSY);
		val = smsc_read_reg(sc, SMSC_REG_E2P_CMD);
		if (timedout || (val & E2P_CMD_TIMEOUT)) {
			device_printf(sc->sc_ue.ue_dev, 
				"Timed-out reading from EEPROM\n");
			return (-1);
		}

		val = smsc_read_reg(sc, SMSC_REG_E2P_DATA);
		data[i] = val & E2P_DATA_MASK;
	}

	return (0);
}

#if 0
/**
 *	smsc_write_eeprom - Reads data from eeprom
 *	@sc: driver soft context
 *	@off: EEPROM offset
 *	@data: memory to write
 *	@length: write length bytes
 *
 *	RETURNS:
 *	0 on success, -1 otherwise
 */
static int
smsc_write_eeprom(struct smsc_softc *sc, uint32_t off, uint8_t *data, int length)
{
	int timedout, i;
	uint32_t val;

	if (smsc_wait_for_bits(sc, SMSC_REG_E2P_CMD, E2P_CMD_BUSY)) {
		device_printf(sc->sc_ue.ue_dev, "Timed-out waiting for busy EEPROM\n");
		return (-1);
	}

	/*
	 * Write/Erase
	 */
	smsc_write_reg(sc, SMSC_REG_E2P_CMD, E2P_CMD_BUSY | E2P_CMD_EWEN);
	timedout = smsc_wait_for_bits(sc, SMSC_REG_E2P_CMD, E2P_CMD_BUSY);
	val = smsc_read_reg(sc, SMSC_REG_E2P_CMD);

	if (timedout || (val & E2P_CMD_TIMEOUT)) {
		device_printf(sc->sc_ue.ue_dev, "Timed-out erasing EEPROM\n");
		return (-1);
	}

	for (i = 0; i < length; i++) {
		val = data[i];
		smsc_write_reg(sc, SMSC_REG_E2P_DATA, val);
		smsc_write_reg(sc, SMSC_REG_E2P_CMD,
			E2P_CMD_BUSY | E2P_CMD_WRITE | ((off+i) & E2P_CMD_ADDR));

		timedout = smsc_wait_for_bits(sc, SMSC_REG_E2P_CMD, E2P_CMD_BUSY);
		val = smsc_read_reg(sc, SMSC_REG_E2P_CMD);

		if (timedout || (val & E2P_CMD_TIMEOUT)) {
			device_printf(sc->sc_ue.ue_dev, 
				"Timed-out writing EEPROM %d %x\n", i, val);
			return (-1);
		}
	}

	return (0);
}
#endif

/**
 *	smsc_miibus_readreg - Reads a MII/MDIO register
 *	@dev: usb ether device
 *	@phy: the number of phy writing to
 *	@reg: the register address
 *	@val: the value to write
 *
 *
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static int
smsc_miibus_readreg(device_t dev, int phy, int reg)
{
	struct smsc_softc *sc = device_get_softc(dev);
	int locked;
	uint32_t addr;
	uint32_t val = 0;
	int i;

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		SMSC_LOCK(sc);

	addr = (phy << 11) | (reg << 6) | MII_READ;
	smsc_write_reg(sc, SMSC_REG_MII_ADDR, addr);

	for (i = 0; i != SMSC_TIMEOUT; i++) {
		if (!(smsc_read_reg(sc, SMSC_REG_MII_ADDR) & MII_BUSY))
			break;
		if (uether_pause(&sc->sc_ue, hz / 100))
			break;
	}

	if (i == SMSC_TIMEOUT)
		device_printf(sc->sc_ue.ue_dev, "MII read timed out\n");

	val = smsc_read_reg(sc, SMS_REG_MII_DATA);

	if (!locked)
		SMSC_UNLOCK(sc);

	return (val & 0xFFFF);
}

/**
 *	smsc_miibus_writereg - Writes a MII/MDIO register
 *	@dev: usb ether device
 *	@phy: the number of phy writing to
 *	@reg: the register address
 *	@val: the value to write
 *
 *
 *
 *	RETURNS:
 *	0 
 */
static int
smsc_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct smsc_softc *sc = device_get_softc(dev);
	int locked;
	uint32_t addr;
	int i;

	if (sc->sc_phyno != phy)
		return (0);

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		SMSC_LOCK(sc);

	val = htole32(val);
	smsc_write_reg(sc, SMS_REG_MII_DATA, val);

	addr = (phy << 11) | (reg << 6) | MII_WRITE;
	smsc_write_reg(sc, SMSC_REG_MII_ADDR, addr);

	for (i = 0; i != SMSC_TIMEOUT; i++) {
		if (!(smsc_read_reg(sc, SMSC_REG_MII_ADDR) & MII_BUSY))
			break;
		if (uether_pause(&sc->sc_ue, hz / 100))
			break;
	}

	if (i == SMSC_TIMEOUT)
		device_printf(sc->sc_ue.ue_dev, "MII write timed out\n");

	if (!locked)
		SMSC_UNLOCK(sc);
	
	return (0);
}



/**
 *	smsc_miibus_statchg - Called when the MII status changes
 *	@dev: usb ether device
 *
 *
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_miibus_statchg(device_t dev)
{
	struct smsc_softc *sc = device_get_softc(dev);
	struct mii_data *mii = uether_getmii(&sc->sc_ue);
	struct ifnet *ifp;
	int locked;

	locked = mtx_owned(&sc->sc_mtx);
	if (!locked)
		SMSC_LOCK(sc);

	ifp = uether_getifp(&sc->sc_ue);
	if (mii == NULL || ifp == NULL ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		goto done;
 
	/* Use the MII status to determine link status */
	sc->sc_flags &= ~SMSC_FLAG_LINK;
	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
			case IFM_10_T:
			case IFM_100_TX:
				sc->sc_flags |= SMSC_FLAG_LINK;
				break;
			case IFM_1000_T:
				/* Gigabit ethernet not supported by chipset */
				break;
			default:
				break;
		}
	}
 
	/* Lost link, do nothing. */
	if ((sc->sc_flags & SMSC_FLAG_LINK) == 0)
		goto done;
	
	/* Enable/disable full duplex operation */
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0) {
		sc->sc_mac_cr &= ~MAC_CR_RCVOWN;
		sc->sc_mac_cr |= MAC_CR_FDPX;
	} else {
		sc->sc_mac_cr &= ~MAC_CR_FDPX;
		sc->sc_mac_cr |= MAC_CR_RCVOWN;
	}
	
	smsc_write_reg(sc, SMSC_REG_MAC_CR, sc->sc_mac_cr);
	
done:
	if (!locked)
		SMSC_UNLOCK(sc);
}

/**
 *	smsc_ifmedia_upd - Set media options
 *	@ifp: interface pointer
 *
 *	Basically boilerplate code that simply calls the mii functions to set the
 *	media options.
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static int
smsc_ifmedia_upd(struct ifnet *ifp)
{
	struct smsc_softc *sc = ifp->if_softc;
	struct mii_data *mii = uether_getmii(&sc->sc_ue);
	int err;

	SMSC_LOCK_ASSERT(sc, MA_OWNED);

	if (mii->mii_instance) {
		struct mii_softc *miisc;

		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	err = mii_mediachg(mii);
	return (err);
}

/**
 *	smsc_ifmedia_sts - Report current media status
 *	@ifp: 
 *	@ifmr: 
 *
 *	Basically boilerplate code that simply calls the mii functions to get the
 *	media status.
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct smsc_softc *sc = ifp->if_softc;
	struct mii_data *mii = uether_getmii(&sc->sc_ue);

	SMSC_LOCK(sc);
	
	mii_pollstat(mii);
	
	SMSC_UNLOCK(sc);
	
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

/**
 *	smsc_hash - Calculate the hash of a mac address
 *	@addr: The mac address to calculate the has on
 *
 *
 *	RETURNS:
 *	Returns a value from 0-63 which is the hash of the mac address.
 */
static inline uint32_t
smsc_hash(uint8_t addr[ETHER_ADDR_LEN])
{
	return (ether_crc32_be(addr, ETHER_ADDR_LEN) >> 26) & 0x3f;
}

/**
 *	smsc_setmulti - Setup multicast
 *	@ue: 
 *
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_setmulti(struct usb_ether *ue)
{
	struct smsc_softc *sc = uether_getsc(ue);
	struct ifnet *ifp = uether_getifp(ue);
	struct ifmultiaddr *ifma;
	uint32_t hashtbl[2] = { 0, 0 };
	uint32_t hash;

	SMSC_LOCK_ASSERT(sc, MA_OWNED);

	if (ifp->if_flags & IFF_PROMISC) {
		/* Enter promiscuous mode and set the bits accordingly */
		sc->sc_mac_cr |= MAC_CR_PRMS;
		sc->sc_mac_cr &= ~(MAC_CR_MCPAS | MAC_CR_HPFILT);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		/* Enter multicaste mode and set the bits accordingly */
		sc->sc_mac_cr |= MAC_CR_MCPAS;
		sc->sc_mac_cr &= ~(MAC_CR_PRMS | MAC_CR_HPFILT);
		
	} else {
		/* Take the lock of the mac address list before hashing each of them */
		if_maddr_rlock(ifp);

		if (!TAILQ_EMPTY(&ifp->if_multiaddrs)) {
			/* We are filtering on a set of address so calculate hashes of each
			 * of the address and set the corresponding bits in the register.
			 */
			sc->sc_mac_cr |= MAC_CR_HPFILT;
			sc->sc_mac_cr &= ~(MAC_CR_PRMS | MAC_CR_MCPAS);
		
			TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
				if (ifma->ifma_addr->sa_family != AF_LINK)
					continue;

				hash = smsc_hash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
				hashtbl[hash >> 5] |= 1 << (hash & 0x1F);
			}
		} else {
			/* Only receive packets with destination set to our mac address */
			sc->sc_mac_cr &= ~(MAC_CR_PRMS | MAC_CR_MCPAS | MAC_CR_HPFILT);
		}

		if_maddr_runlock(ifp);
	}

	/* Write the hash table and mac control registers */
	smsc_write_reg(sc, SMSC_REG_HASHH, hashtbl[1]);
	smsc_write_reg(sc, SMSC_REG_HASHL, hashtbl[0]);
	smsc_write_reg(sc, SMSC_REG_MAC_CR, sc->sc_mac_cr);
}

/**
 *	smsc_setpromisc - Setup promiscuous mode
 *	@ue: 
 *
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_setpromisc(struct usb_ether *ue)
{
	struct smsc_softc *sc = uether_getsc(ue);
	struct ifnet *ifp = uether_getifp(ue);

	device_printf(sc->sc_ue.ue_dev, "promiscuous mode enabled\n");

	SMSC_LOCK_ASSERT(sc, MA_OWNED);

	/* Set/clear the promiscuous bit based on setting */
	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_mac_cr |= MAC_CR_PRMS;
	} else {
		sc->sc_mac_cr &= ~MAC_CR_PRMS;
	}

	/* Write mac control registers */
	smsc_write_reg(sc, SMSC_REG_MAC_CR, sc->sc_mac_cr);
}

/**
 *	smsc_set_mac_address - Sets the mac address in the device
 *	@sc: driver soft context
 *	@addr: pointer to array contain at least 6 bytes of the mac
 *
 *	Writes the MAC address into the device, usually this doesn't need to be
 *	done because typically the MAC is read from the attached EEPROM.
 *
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_set_mac_address(struct smsc_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	/* Program the lower 4 bytes of the MAC */
	tmp = addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0];
	smsc_write_reg(sc, SMSC_REG_ADDRL, tmp);
		
	/* Program the upper 2 bytes of the MAC */
	tmp = addr[5] << 8 | addr[4];
	smsc_write_reg(sc, SMSC_REG_ADDRH, tmp);
}

/**
 *	smsc_reset - Reset the SMSC interface
 *	@sc: device soft context
 *
 *	
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_reset(struct smsc_softc *sc)
{
	struct usb_config_descriptor *cd;
	usb_error_t err;

	cd = usbd_get_config_descriptor(sc->sc_ue.ue_udev);

	err = usbd_req_set_config(sc->sc_ue.ue_udev, &sc->sc_mtx,
	    cd->bConfigurationValue);

	if (err)
		device_printf(sc->sc_ue.ue_dev, "reset failed (ignored)\n");

	/* Wait a little while for the chip to get its brains in order. */
	uether_pause(&sc->sc_ue, hz / 100);

	/* Reinitialize controller to achieve full reset. */
	smsc_chip_init(sc);
}


/**
 *	smsc_init - Initialises the LAN95xx chip
 *	@ue: USB ether interface
 *
 *	
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_init(struct usb_ether *ue)
{
	struct smsc_softc *sc = uether_getsc(ue);
	struct ifnet *ifp = uether_getifp(ue);

	SMSC_LOCK_ASSERT(sc, MA_OWNED);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
		return;

	/* Cancel pending I/O */
	smsc_stop(ue);

	/* Reset the ethernet interface */
	smsc_reset(sc);

	/* Set MAC address. */
	smsc_set_mac_address(sc, IF_LLADDR(ifp));

	/* Load the multicast filter. */
	smsc_setmulti(ue);

	usbd_xfer_set_stall(sc->sc_xfer[SMSC_BULK_DT_WR]);

	/* Indicate we are up and running */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	/* Switch to selected media. */
	smsc_ifmedia_upd(ifp);
	smsc_start(ue);
}


/**
 *	smsc_intr_callback - Inteerupt callback used to process the USB packet
 *	@xfer: the USB transfer
 *	@error:
 *
 *	
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);
}

/**
 *	smsc_bulk_read_callback - Read callback used to process the USB packet
 *	@xfer: the USB transfer
 *	@error:
 *
 *	
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_bulk_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct smsc_softc *sc = usbd_xfer_softc(xfer);
	struct usb_ether *ue = &sc->sc_ue;
	struct ifnet *ifp = uether_getifp(ue);
	struct usb_page_cache *pc;
	uint32_t rxhdr;
	uint16_t pktlen;
	uint32_t len;

	int actlen;
	int frames;
	static int count = 0;

	usbd_xfer_status(xfer, &actlen, NULL, &frames, NULL);
	count++;
	switch (USB_GET_STATE(xfer)) {
		case USB_ST_TRANSFERRED:
			/* From looking at the linux driver it appears the received packet
			 * is prefixed with a 32-bit header, which contains information like
			 * the status of the received packet.
			 *
			 * Also there maybe multiple packets in the USB frame, each will
			 * have a header and each needs to have it's own mbuf allocated and
			 * populated for it.
			 */
			if (actlen < sizeof(rxhdr) + ETHER_CRC_LEN) {
				ifp->if_ierrors++;
				printf("DROP\n");
				goto tr_setup;
			}
			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_out(pc, 0, &rxhdr, sizeof(rxhdr));
			actlen -= sizeof(rxhdr);
			rxhdr = le32toh(rxhdr);
			pktlen = (uint16_t)((rxhdr & SMSC_RX_STATUS_FL_MASK) >> SMSC_RX_STATUS_FL_SHIFT);
			len = min(pktlen, actlen);
			if (rxhdr & SMSC_RX_STATUS_ES) {
				ifp->if_ierrors++;
				printf("DROP\n");
				goto tr_setup;
			}
			uether_rxbuf(ue, pc, 2 + sizeof(rxhdr), len);
			/* FALLTHROUGH */
			
		case USB_ST_SETUP:
tr_setup:
			usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
			usbd_transfer_submit(xfer);
			uether_rxflush(ue);
			return;

		default:
			device_printf(sc->sc_ue.ue_dev, "bulk read error, %s", usbd_errstr(error));
			if (error != USB_ERR_CANCELLED) {
				usbd_xfer_set_stall(xfer);
				goto tr_setup;
			}
			device_printf(sc->sc_ue.ue_dev, "start rx %i", usbd_xfer_max_len(xfer));
			return;
	}
}

/**
 *	smsc_bulk_write_callback - Write callback used to send ethernet frame
 *	@xfer: the USB transfer
 *	@error:
 *
 *	
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_bulk_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct smsc_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = uether_getifp(&sc->sc_ue);
	struct usb_page_cache *pc;
	struct mbuf *m;
	uint32_t txhdr;
	uint32_t frm_len = 0;
	uint32_t csum_prefix = 0;

	switch (USB_GET_STATE(xfer)) {
		case USB_ST_TRANSFERRED:
			ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
			/* FALLTHROUGH */

		case USB_ST_SETUP:
tr_setup:
			if ((sc->sc_flags & SMSC_FLAG_LINK) == 0 ||
				(ifp->if_drv_flags & IFF_DRV_OACTIVE) != 0) {
				/* Don't send anything if there is no link or controller is
				 * busy.
				 */
				return;
			}
			
			/* Pull the frame of the queue */
			IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
			if (m == NULL)
				return;

			/* Get the frame so we copy in the header and frame data */
			pc = usbd_xfer_get_frame(xfer, 0);
			
			/* Check if we can use the H/W checksumming */
			if ((ifp->if_capenable & IFCAP_TXCSUM) &&
			    ((m->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP)) != 0)) {
				panic("HW checksumming support is not implemented\n");
			}

			/* Each frame is prefixed with two 32-bit values describing the
			 * length and checksum offloading request.
			 */
			txhdr = m->m_pkthdr.len | SMSC_TX_CMD_A_FIRST_SEG | SMSC_TX_CMD_A_LAST_SEG;
			txhdr = htole32(txhdr);
			usbd_copy_in(pc, 0, &txhdr, sizeof(txhdr));
			
			txhdr = m->m_pkthdr.len;
			if (csum_prefix)
				txhdr |= SMSC_TX_CMD_B_CSUM_ENABLE;
			txhdr = htole32(txhdr);
			usbd_copy_in(pc, 4, &txhdr, sizeof(txhdr));
			
			frm_len += 8;
				

			/* Next copy in the actual packet */
			usbd_m_copy_in(pc, frm_len, m, 0, m->m_pkthdr.len);
			frm_len += m->m_pkthdr.len;

			/* Set the length of the transfer including the header */
			usbd_xfer_set_frame_len(xfer, 0, frm_len);

			/* Update the number of packets sent */
			ifp->if_opackets++;

			/* If there's a BPF listener, bounce a copy of this frame to him */
			BPF_MTAP(ifp, m);

			m_freem(m);

			usbd_transfer_submit(xfer);
			return;
			
		default:
			device_printf(sc->sc_ue.ue_dev, "usb error on tx: %s\n", usbd_errstr(error));
			ifp->if_oerrors++;
			if (error != USB_ERR_CANCELLED) {
				usbd_xfer_set_stall(xfer);
				goto tr_setup;
			}
			return;
	}
}

/**
 *	smsc_tick - Called periodically to monitor the start of the LAN95xx chip
 *	@ue: USB ether interface
 *
 *	
 *
 *	RETURNS:
 *	Nothing
 */
static void
smsc_tick(struct usb_ether *ue)
{
	struct smsc_softc *sc = uether_getsc(ue);
	struct mii_data *mii = uether_getmii(&sc->sc_ue);;

	SMSC_LOCK_ASSERT(sc, MA_OWNED);

	mii_tick(mii);
	if ((sc->sc_flags & SMSC_FLAG_LINK) == 0) {
		smsc_miibus_statchg(ue->ue_dev);
		if ((sc->sc_flags & SMSC_FLAG_LINK) != 0)
			smsc_start(ue);
	}
}

/**
 *	smsc_start - Starts communication with the LAN95xx chip
 *	@ue: USB ether interface
 *
 *	
 *
 *	RETURNS:
 *	Nothing
 */
static void
smsc_start(struct usb_ether *ue)
{
	struct smsc_softc *sc = uether_getsc(ue);

	/*
	 * start the USB transfers, if not already started:
	 */
	usbd_transfer_start(sc->sc_xfer[SMSC_BULK_DT_RD]);
	usbd_transfer_start(sc->sc_xfer[SMSC_BULK_DT_WR]);
	usbd_transfer_start(sc->sc_xfer[SMSC_INTR_DT_RD]);
}

/**
 *	smsc_stop - Stops communication with the LAN95xx chip
 *	@ue: USB ether interface
 *
 *	
 *
 *	RETURNS:
 *	Nothing
 */
static void
smsc_stop(struct usb_ether *ue)
{
	struct smsc_softc *sc = uether_getsc(ue);
	struct ifnet *ifp = uether_getifp(ue);

	SMSC_LOCK_ASSERT(sc, MA_OWNED);

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->sc_flags &= ~SMSC_FLAG_LINK;

	/*
	 * stop all the transfers, if not already stopped:
	 */
	usbd_transfer_stop(sc->sc_xfer[SMSC_BULK_DT_WR]);
	usbd_transfer_stop(sc->sc_xfer[SMSC_BULK_DT_RD]);
	usbd_transfer_stop(sc->sc_xfer[SMSC_INTR_DT_RD]);
}

/**
 *	smsc_phy_initialize - Initialises the in-built SMSC phy
 *	@sc: driver soft context
 *
 *	
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static int
smsc_phy_initialize(struct smsc_softc *sc)
{
	int bmcr;

	SMSC_LOCK_ASSERT(sc, MA_OWNED);

	/* Reset phy and wait for reset to complete */
	smsc_miibus_writereg(sc->sc_ue.ue_dev, sc->sc_phyno, MII_BMCR, BMCR_RESET);
	if (smsc_wait_for_bits(sc, MII_BMCR, MII_BMCR)) {
		device_printf(sc->sc_ue.ue_dev, "PHY reset timed out");
		return -1;
	}

	smsc_miibus_writereg(sc->sc_ue.ue_dev, sc->sc_phyno, MII_ANAR,
	                     ANAR_10 | ANAR_10_FD | ANAR_TX | ANAR_TX_FD |  /* all modes */
	                     ANAR_CSMA | 
	                     ANAR_FC |
	                     ANAR_PAUSE_ASYM);

	/* Read to clear */
	smsc_miibus_readreg(sc->sc_ue.ue_dev, sc->sc_phyno, MII_PHY_INT_SRC);

	/* Set the default PHY interrupt mask */
	smsc_miibus_writereg(sc->sc_ue.ue_dev, sc->sc_phyno, MII_PHY_INT_MASK,
	                     PHY_INT_MASK_DEFAULT);
	
	/* Restart auto-negotation */
	bmcr = smsc_miibus_readreg(sc->sc_ue.ue_dev, sc->sc_phyno, MII_BMCR);
	bmcr |= BMCR_STARTNEG;
	smsc_miibus_writereg(sc->sc_ue.ue_dev, sc->sc_phyno, MII_BMCR, bmcr);
	
	return 0;
}
 
 
/**
 *	smsc_chip_init - Initialises the chip after power on
 *	@sc: driver soft context
 *
 *	
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static int
smsc_chip_init(struct smsc_softc *sc)
{
	int err = 0;
	uint32_t reg_val;
	int burst_cap;

	/* Enter H/W config mode */
	smsc_write_reg(sc, SMSC_REG_HW_CFG, HW_CFG_LRST);

	if (smsc_wait_for_bits(sc, SMSC_REG_HW_CFG, HW_CFG_LRST)) {
		device_printf(sc->sc_ue.ue_dev, "Timed-out waiting for completion of Lite Reset\n");
		goto reset_failed;
	}

	/* Reset the PHY */
	smsc_write_reg(sc, SMSC_REG_PM_CTRL, PM_CTRL_PHY_RST);

	if (smsc_wait_for_bits(sc, SMSC_REG_PM_CTRL, PM_CTRL_PHY_RST)) {
		device_printf(sc->sc_ue.ue_dev, "timed out waiting for PHY Reset\n");
		goto reset_failed;
	}

	/* Don't know what the HW_CFG_BIR bit is, but following the reset sequence
	 * as used in the Linux driver.
	 */
	reg_val = smsc_read_reg(sc, SMSC_REG_HW_CFG);
	reg_val |= HW_CFG_BIR;
	smsc_write_reg(sc, SMSC_REG_HW_CFG, reg_val);

	/* 
	 * There is a so called 'turbo mode' that the linux driver supports, it
	 * seems to allow you to jam multiple frames per Rx transaction.  I support
	 * fo this isn't enabed in this driver yet as still unsure of the layout
	 * of the packets.
	 *
	 * So since we don't (yet?) support this mode the burst_cap(ability) is set
	 * to zero and the maximum URB sze is 2048.
	 */
	if (usbd_get_speed(sc->sc_ue.ue_udev) == USB_SPEED_HIGH)
		burst_cap = 32;
	else
		burst_cap = 128;
	
	/* 
	 * Write the burst capability - number of ether frames that can be in a
	 * single RX transaction.
	 */
	smsc_write_reg(sc, SMSC_REG_BURST_CAP, burst_cap);

	/* Set the default bulk in delay */
	smsc_write_reg(sc, SMSC_REG_BULK_IN_DLY, 0x00002000);

	/* Initialise the RX interface */
	reg_val = smsc_read_reg(sc, SMSC_REG_HW_CFG);
	reg_val &= ~HW_CFG_RXDOFF;

	/* Set Rx data offset=2, Make IP header aligns on word boundary. */
	reg_val |= ETHER_ALIGN << 9;

	smsc_write_reg(sc, SMSC_REG_HW_CFG, reg_val);

	/* Clear the status register ? */
	smsc_write_reg(sc, SMSC_REG_INT_STS, 0xffffffff);

	/* Read and display the revision register */
	reg_val = smsc_read_reg(sc, SMSC_REG_ID_REV);

	device_printf(sc->sc_ue.ue_dev, "chip 0x%04x, rev. %04x\n", 
	    (reg_val & ID_REV_CHIP_ID_MASK) >> ID_REV_CHIP_ID_SHIFT, 
	    (reg_val & ID_REV_CHIP_REV_MASK));

	reg_val = LED_GPIO_CFG_SPD_LED | LED_GPIO_CFG_LNK_LED | LED_GPIO_CFG_FDX_LED;
	smsc_write_reg(sc, SMSC_REG_LED_GPIO_CFG, reg_val);

	/* Init Tx */
	smsc_write_reg(sc, SMSC_REG_FLOW, 0);

	smsc_write_reg(sc, SMSC_REG_AFC_CFG, AFC_CFG_DEFAULT);

	/* Read the current MAC configuration */
	sc->sc_mac_cr = smsc_read_reg(sc, SMSC_REG_MAC_CR);
	
	smsc_write_reg(sc, SMSC_REG_VLAN1, (uint32_t)ETHERTYPE_VLAN);

	/* Initialise the PHY */
	if (smsc_phy_initialize(sc) < 0) {
		err = ENXIO;
		goto reset_failed;
	}
	
	reg_val = smsc_read_reg(sc, SMSC_REG_INT_EP_CTL);
	/* enable PHY interrupts */
	reg_val |= INT_EP_CTL_PHY_INT;

	smsc_write_reg(sc, SMSC_REG_INT_EP_CTL, reg_val);

	/*
	 * Start TX
	 */
	sc->sc_mac_cr |= MAC_CR_TXEN;
	smsc_write_reg(sc, SMSC_REG_MAC_CR, sc->sc_mac_cr);
	smsc_write_reg(sc, SMSC_REG_TX_CFG, TX_CFG_ON);

	/*
	 * Start RX
	 */
	sc->sc_mac_cr |= MAC_CR_RXEN;
	smsc_write_reg(sc, SMSC_REG_MAC_CR, sc->sc_mac_cr);

	return (0);
	
reset_failed:

	device_printf(sc->sc_ue.ue_dev, "smsc_chip_init failed: error %d\n", err);
	return (err);
}

/**
 *	smsc_attach_post - Called after the driver attached to the USB interface
 *	@ue: the USB ethernet device
 *
 *	This is where the chip is intialised for the first time.  This is different
 *	from the smsc_init() function in that that one is designed to setup the
 *	H/W to match the UE settings and can be called after a reset.
 *
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static void
smsc_attach_post(struct usb_ether *ue)
{
	struct smsc_softc *sc = uether_getsc(ue);
	uint32_t mac_h, mac_l;
	uint32_t has_eeprom_address;
	int i;

	/* Setup some of the basics */
	sc->sc_phyno = 0;

	/* Initialise the chip for the first time */
	smsc_chip_init(sc);

	/* 
	 * Attempt to get the mac address, if the EEPROM is not attached this
	 * will just return FF:FF:FF:FF:FF:FF, so in such cases we invent a MAC
	 * address based on the tick time.
	 *
	 */
	has_eeprom_address = 0;
	if (smsc_read_eeprom(sc, 1, sc->sc_ue.ue_eaddr, ETHER_ADDR_LEN) == 0) {
		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			if ((sc->sc_ue.ue_eaddr[i] != 0) && 
			    (sc->sc_ue.ue_eaddr[i] != 0xff))
				has_eeprom_address = 1;
		}
	}
		
	if (!has_eeprom_address) {
		mac_l = smsc_read_reg(sc, SMSC_REG_ADDRL);
		mac_h = smsc_read_reg(sc, SMSC_REG_ADDRH);

		/* Create a temporary fake mac address */
		if ((mac_h == 0x0000ffff) || (mac_l == 0xffffffff)) {
			device_printf(sc->sc_ue.ue_dev, "Using random MAC address\n");
			
			/* 
			 * Use hardcoded MAC for debug purposes
			 * TODO: generate actual random MAC. 
			 */
			sc->sc_ue.ue_eaddr[0] = 0x0E;
			sc->sc_ue.ue_eaddr[1] = 0x60;
			sc->sc_ue.ue_eaddr[2] = 0x33;
			sc->sc_ue.ue_eaddr[3] = 0xB1;
			sc->sc_ue.ue_eaddr[4] = 0x46;
			sc->sc_ue.ue_eaddr[5] = 0x01;
		} else {
			sc->sc_ue.ue_eaddr[5] = (uint8_t)((mac_h >> 8) & 0xff);
			sc->sc_ue.ue_eaddr[4] = (uint8_t)((mac_h) & 0xff);
			sc->sc_ue.ue_eaddr[3] = (uint8_t)((mac_l >> 24) & 0xff);
			sc->sc_ue.ue_eaddr[2] = (uint8_t)((mac_l >> 16) & 0xff);
			sc->sc_ue.ue_eaddr[1] = (uint8_t)((mac_l >> 8) & 0xff);
			sc->sc_ue.ue_eaddr[0] = (uint8_t)((mac_l) & 0xff);
		}
	}
	
	device_printf(sc->sc_ue.ue_dev,
	              "MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
				  sc->sc_ue.ue_eaddr[0], sc->sc_ue.ue_eaddr[1],
				  sc->sc_ue.ue_eaddr[2], sc->sc_ue.ue_eaddr[3],
				  sc->sc_ue.ue_eaddr[4], sc->sc_ue.ue_eaddr[5]);
}

/**
 *	smsc_probe - Probe the interface. 
 *	@dev: smsc device handle
 *
 *	Allocate softc structures, do ifmedia setup and ethernet/BPF attach.
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static int
smsc_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != SMSC_CONFIG_INDEX)
		return (ENXIO);
	if (uaa->info.bIfaceIndex != SMSC_IFACE_IDX)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(smsc_devs, sizeof(smsc_devs), uaa));
}


/**
 *	smsc_attach - Attach the interface. 
 *	@dev: smsc device handle
 *
 *	Allocate softc structures, do ifmedia setup and ethernet/BPF attach.
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static int
smsc_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct smsc_softc *sc = device_get_softc(dev);
	struct usb_ether *ue = &sc->sc_ue;
	uint8_t iface_index;
	int err;

	sc->sc_flags = USB_GET_DRIVER_INFO(uaa);

	device_set_usb_desc(dev);

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	/* Setup the endpoints for the SMSC LAN95xx devices */
	iface_index = SMSC_IFACE_IDX;
	err = usbd_transfer_setup(uaa->device, &iface_index, sc->sc_xfer,
	                          smsc_config, SMSC_N_TRANSFER, sc, &sc->sc_mtx);
	if (err) {
		device_printf(dev, "allocating USB transfers failed\n");
		goto detach;
	}

	ue->ue_sc = sc;
	ue->ue_dev = dev;
	ue->ue_udev = uaa->device;
	ue->ue_mtx = &sc->sc_mtx;
	ue->ue_methods = &smsc_ue_methods;

	err = uether_ifattach(ue);
	if (err) {
		device_printf(dev, "could not attach interface\n");
		goto detach;
	}
	return (0);			/* success */

detach:
	smsc_detach(dev);
	return (ENXIO);			/* failure */
}

/**
 *	smsc_detach - Detach the interface. 
 *	@dev: smsc device handle
 *
 *	
 *
 *	RETURNS:
 *	Returns 0 on success or a negative error code.
 */
static int
smsc_detach(device_t dev)
{
	struct smsc_softc *sc = device_get_softc(dev);
	struct usb_ether *ue = &sc->sc_ue;

	usbd_transfer_unsetup(sc->sc_xfer, SMSC_N_TRANSFER);
	uether_ifdetach(ue);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static device_method_t smsc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, smsc_probe),
	DEVMETHOD(device_attach, smsc_attach),
	DEVMETHOD(device_detach, smsc_detach),

	/* bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),
	DEVMETHOD(bus_driver_added, bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg, smsc_miibus_readreg),
	DEVMETHOD(miibus_writereg, smsc_miibus_writereg),
	DEVMETHOD(miibus_statchg, smsc_miibus_statchg),

	{0, 0}
};

static driver_t smsc_driver = {
	.name = "smsc",
	.methods = smsc_methods,
	.size = sizeof(struct smsc_softc),
};

static devclass_t smsc_devclass;

DRIVER_MODULE(smsc, uhub, smsc_driver, smsc_devclass, NULL, 0);
DRIVER_MODULE(miibus, smsc, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(smsc, uether, 1, 1, 1);
MODULE_DEPEND(smsc, usb, 1, 1, 1);
MODULE_DEPEND(smsc, ether, 1, 1, 1);
MODULE_DEPEND(smsc, miibus, 1, 1, 1);
MODULE_VERSION(smsc, 1);
