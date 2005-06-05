/*-
 * Copyright (c) 2004
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 * VIA Networking Technologies VT612x PCI gigabit ethernet NIC driver.
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Networking Software Engineer
 * Wind River Systems
 */

/*
 * The VIA Networking VT6122 is a 32bit, 33/66Mhz PCI device that
 * combines a tri-speed ethernet MAC and PHY, with the following
 * features:
 *
 *	o Jumbo frame support up to 16K
 *	o Transmit and receive flow control
 *	o IPv4 checksum offload
 *	o VLAN tag insertion and stripping
 *	o TCP large send
 *	o 64-bit multicast hash table filter
 *	o 64 entry CAM filter
 *	o 16K RX FIFO and 48K TX FIFO memory
 *	o Interrupt moderation
 *
 * The VT6122 supports up to four transmit DMA queues. The descriptors
 * in the transmit ring can address up to 7 data fragments; frames which
 * span more than 7 data buffers must be coalesced, but in general the
 * BSD TCP/IP stack rarely generates frames more than 2 or 3 fragments
 * long. The receive descriptors address only a single buffer.
 *
 * There are two peculiar design issues with the VT6122. One is that
 * receive data buffers must be aligned on a 32-bit boundary. This is
 * not a problem where the VT6122 is used as a LOM device in x86-based
 * systems, but on architectures that generate unaligned access traps, we
 * have to do some copying.
 *
 * The other issue has to do with the way 64-bit addresses are handled.
 * The DMA descriptors only allow you to specify 48 bits of addressing
 * information. The remaining 16 bits are specified using one of the
 * I/O registers. If you only have a 32-bit system, then this isn't
 * an issue, but if you have a 64-bit system and more than 4GB of
 * memory, you must have to make sure your network data buffers reside
 * in the same 48-bit 'segment.'
 *
 * Special thanks to Ryan Fu at VIA Networking for providing documentation
 * and sample NICs for testing.
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

MODULE_DEPEND(vge, pci, 1, 1, 1);
MODULE_DEPEND(vge, ether, 1, 1, 1);
MODULE_DEPEND(vge, miibus, 1, 1, 1);

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#include <dev/vge/if_vgereg.h>
#include <dev/vge/if_vgevar.h>

#define VGE_CSUM_FEATURES    (CSUM_IP | CSUM_TCP | CSUM_UDP)

/*
 * Various supported device vendors/types and their names.
 */
static struct vge_type vge_devs[] = {
	{ VIA_VENDORID, VIA_DEVICEID_61XX,
		"VIA Networking Gigabit Ethernet" },
	{ 0, 0, NULL }
};

static int vge_probe		(device_t);
static int vge_attach		(device_t);
static int vge_detach		(device_t);

static int vge_encap		(struct vge_softc *, struct mbuf *, int);

static void vge_dma_map_addr	(void *, bus_dma_segment_t *, int, int);
static void vge_dma_map_rx_desc	(void *, bus_dma_segment_t *, int,
				    bus_size_t, int);
static void vge_dma_map_tx_desc	(void *, bus_dma_segment_t *, int,
				    bus_size_t, int);
static int vge_allocmem		(device_t, struct vge_softc *);
static int vge_newbuf		(struct vge_softc *, int, struct mbuf *);
static int vge_rx_list_init	(struct vge_softc *);
static int vge_tx_list_init	(struct vge_softc *);
#ifdef VGE_FIXUP_RX
static __inline void vge_fixup_rx
				(struct mbuf *);
#endif
static void vge_rxeof		(struct vge_softc *);
static void vge_txeof		(struct vge_softc *);
static void vge_intr		(void *);
static void vge_tick		(void *);
static void vge_tx_task		(void *, int);
static void vge_start		(struct ifnet *);
static int vge_ioctl		(struct ifnet *, u_long, caddr_t);
static void vge_init		(void *);
static void vge_stop		(struct vge_softc *);
static void vge_watchdog	(struct ifnet *);
static int vge_suspend		(device_t);
static int vge_resume		(device_t);
static void vge_shutdown	(device_t);
static int vge_ifmedia_upd	(struct ifnet *);
static void vge_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

#ifdef VGE_EEPROM
static void vge_eeprom_getword	(struct vge_softc *, int, u_int16_t *);
#endif
static void vge_read_eeprom	(struct vge_softc *, caddr_t, int, int, int);

static void vge_miipoll_start	(struct vge_softc *);
static void vge_miipoll_stop	(struct vge_softc *);
static int vge_miibus_readreg	(device_t, int, int);
static int vge_miibus_writereg	(device_t, int, int, int);
static void vge_miibus_statchg	(device_t);

static void vge_cam_clear	(struct vge_softc *);
static int vge_cam_set		(struct vge_softc *, uint8_t *);
#if __FreeBSD_version < 502113
static uint32_t vge_mchash	(uint8_t *);
#endif
static void vge_setmulti	(struct vge_softc *);
static void vge_reset		(struct vge_softc *);

#define VGE_PCI_LOIO             0x10
#define VGE_PCI_LOMEM            0x14

static device_method_t vge_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vge_probe),
	DEVMETHOD(device_attach,	vge_attach),
	DEVMETHOD(device_detach,	vge_detach),
	DEVMETHOD(device_suspend,	vge_suspend),
	DEVMETHOD(device_resume,	vge_resume),
	DEVMETHOD(device_shutdown,	vge_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	vge_miibus_readreg),
	DEVMETHOD(miibus_writereg,	vge_miibus_writereg),
	DEVMETHOD(miibus_statchg,	vge_miibus_statchg),

	{ 0, 0 }
};

static driver_t vge_driver = {
	"vge",
	vge_methods,
	sizeof(struct vge_softc)
};

static devclass_t vge_devclass;

DRIVER_MODULE(vge, pci, vge_driver, vge_devclass, 0, 0);
DRIVER_MODULE(vge, cardbus, vge_driver, vge_devclass, 0, 0);
DRIVER_MODULE(miibus, vge, miibus_driver, miibus_devclass, 0, 0);

#ifdef VGE_EEPROM
/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void
vge_eeprom_getword(sc, addr, dest)
	struct vge_softc	*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/*
	 * Enter EEPROM embedded programming mode. In order to
	 * access the EEPROM at all, we first have to set the
	 * EELOAD bit in the CHIPCFG2 register.
	 */
	CSR_SETBIT_1(sc, VGE_CHIPCFG2, VGE_CHIPCFG2_EELOAD);
	CSR_SETBIT_1(sc, VGE_EECSR, VGE_EECSR_EMBP/*|VGE_EECSR_ECS*/);

	/* Select the address of the word we want to read */
	CSR_WRITE_1(sc, VGE_EEADDR, addr);

	/* Issue read command */
	CSR_SETBIT_1(sc, VGE_EECMD, VGE_EECMD_ERD);

	/* Wait for the done bit to be set. */
	for (i = 0; i < VGE_TIMEOUT; i++) {
		if (CSR_READ_1(sc, VGE_EECMD) & VGE_EECMD_EDONE)
			break;
	}

	if (i == VGE_TIMEOUT) {
		device_printf(sc->vge_dev, "EEPROM read timed out\n");
		*dest = 0;
		return;
	}

	/* Read the result */
	word = CSR_READ_2(sc, VGE_EERDDAT);

	/* Turn off EEPROM access mode. */
	CSR_CLRBIT_1(sc, VGE_EECSR, VGE_EECSR_EMBP/*|VGE_EECSR_ECS*/);
	CSR_CLRBIT_1(sc, VGE_CHIPCFG2, VGE_CHIPCFG2_EELOAD);

	*dest = word;

	return;
}
#endif

/*
 * Read a sequence of words from the EEPROM.
 */
static void
vge_read_eeprom(sc, dest, off, cnt, swap)
	struct vge_softc	*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
#ifdef VGE_EEPROM
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		vge_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}
#else
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		dest[i] = CSR_READ_1(sc, VGE_PAR0 + i);
#endif
}

static void
vge_miipoll_stop(sc)
	struct vge_softc	*sc;
{
	int			i;

	CSR_WRITE_1(sc, VGE_MIICMD, 0);

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if (CSR_READ_1(sc, VGE_MIISTS) & VGE_MIISTS_IIDL)
			break;
	}

	if (i == VGE_TIMEOUT)
		device_printf(sc->vge_dev, "failed to idle MII autopoll\n");

	return;
}

static void
vge_miipoll_start(sc)
	struct vge_softc	*sc;
{
	int			i;

	/* First, make sure we're idle. */

	CSR_WRITE_1(sc, VGE_MIICMD, 0);
	CSR_WRITE_1(sc, VGE_MIIADDR, VGE_MIIADDR_SWMPL);

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if (CSR_READ_1(sc, VGE_MIISTS) & VGE_MIISTS_IIDL)
			break;
	}

	if (i == VGE_TIMEOUT) {
		device_printf(sc->vge_dev, "failed to idle MII autopoll\n");
		return;
	}

	/* Now enable auto poll mode. */

	CSR_WRITE_1(sc, VGE_MIICMD, VGE_MIICMD_MAUTO);

	/* And make sure it started. */

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if ((CSR_READ_1(sc, VGE_MIISTS) & VGE_MIISTS_IIDL) == 0)
			break;
	}

	if (i == VGE_TIMEOUT)
		device_printf(sc->vge_dev, "failed to start MII autopoll\n");

	return;
}

static int
vge_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct vge_softc	*sc;
	int			i;
	u_int16_t		rval = 0;

	sc = device_get_softc(dev);

	if (phy != (CSR_READ_1(sc, VGE_MIICFG) & 0x1F))
		return(0);

	VGE_LOCK(sc);
	vge_miipoll_stop(sc);

	/* Specify the register we want to read. */
	CSR_WRITE_1(sc, VGE_MIIADDR, reg);

	/* Issue read command. */
	CSR_SETBIT_1(sc, VGE_MIICMD, VGE_MIICMD_RCMD);

	/* Wait for the read command bit to self-clear. */
	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if ((CSR_READ_1(sc, VGE_MIICMD) & VGE_MIICMD_RCMD) == 0)
			break;
	}

	if (i == VGE_TIMEOUT)
		device_printf(sc->vge_dev, "MII read timed out\n");
	else
		rval = CSR_READ_2(sc, VGE_MIIDATA);

	vge_miipoll_start(sc);
	VGE_UNLOCK(sc);

	return (rval);
}

static int
vge_miibus_writereg(dev, phy, reg, data)
	device_t		dev;
	int			phy, reg, data;
{
	struct vge_softc	*sc;
	int			i, rval = 0;

	sc = device_get_softc(dev);

	if (phy != (CSR_READ_1(sc, VGE_MIICFG) & 0x1F))
		return(0);

	VGE_LOCK(sc);
	vge_miipoll_stop(sc);

	/* Specify the register we want to write. */
	CSR_WRITE_1(sc, VGE_MIIADDR, reg);

	/* Specify the data we want to write. */
	CSR_WRITE_2(sc, VGE_MIIDATA, data);

	/* Issue write command. */
	CSR_SETBIT_1(sc, VGE_MIICMD, VGE_MIICMD_WCMD);

	/* Wait for the write command bit to self-clear. */
	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if ((CSR_READ_1(sc, VGE_MIICMD) & VGE_MIICMD_WCMD) == 0)
			break;
	}

	if (i == VGE_TIMEOUT) {
		device_printf(sc->vge_dev, "MII write timed out\n");
		rval = EIO;
	}

	vge_miipoll_start(sc);
	VGE_UNLOCK(sc);

	return (rval);
}

static void
vge_cam_clear(sc)
	struct vge_softc	*sc;
{
	int			i;

	/*
	 * Turn off all the mask bits. This tells the chip
	 * that none of the entries in the CAM filter are valid.
	 * desired entries will be enabled as we fill the filter in.
	 */

	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_CAMMASK);
	CSR_WRITE_1(sc, VGE_CAMADDR, VGE_CAMADDR_ENABLE);
	for (i = 0; i < 8; i++)
		CSR_WRITE_1(sc, VGE_CAM0 + i, 0);

	/* Clear the VLAN filter too. */

	CSR_WRITE_1(sc, VGE_CAMADDR, VGE_CAMADDR_ENABLE|VGE_CAMADDR_AVSEL|0);
	for (i = 0; i < 8; i++)
		CSR_WRITE_1(sc, VGE_CAM0 + i, 0);

	CSR_WRITE_1(sc, VGE_CAMADDR, 0);
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_MAR);

	sc->vge_camidx = 0;

	return;
}

static int
vge_cam_set(sc, addr)
	struct vge_softc	*sc;
	uint8_t			*addr;
{
	int			i, error = 0;

	if (sc->vge_camidx == VGE_CAM_MAXADDRS)
		return(ENOSPC);

	/* Select the CAM data page. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_CAMDATA);

	/* Set the filter entry we want to update and enable writing. */
	CSR_WRITE_1(sc, VGE_CAMADDR, VGE_CAMADDR_ENABLE|sc->vge_camidx);

	/* Write the address to the CAM registers */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		CSR_WRITE_1(sc, VGE_CAM0 + i, addr[i]);

	/* Issue a write command. */
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_WRITE);

	/* Wake for it to clear. */
	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(1);
		if ((CSR_READ_1(sc, VGE_CAMCTL) & VGE_CAMCTL_WRITE) == 0)
			break;
	}

	if (i == VGE_TIMEOUT) {
		device_printf(sc->vge_dev, "setting CAM filter failed\n");
		error = EIO;
		goto fail;
	}

	/* Select the CAM mask page. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_CAMMASK);

	/* Set the mask bit that enables this filter. */
	CSR_SETBIT_1(sc, VGE_CAM0 + (sc->vge_camidx/8),
	    1<<(sc->vge_camidx & 7));

	sc->vge_camidx++;

fail:
	/* Turn off access to CAM. */
	CSR_WRITE_1(sc, VGE_CAMADDR, 0);
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_MAR);

	return (error);
}

#if __FreeBSD_version < 502113
static uint32_t
vge_mchash(addr)
        uint8_t			*addr;
{
	uint32_t		crc, carry;
	int			idx, bit;
	uint8_t			data;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (data & 0x01);
			crc <<= 1;
			if (carry)
				crc = (crc ^ 0x04c11db6) | carry;
		}
	}

	return(crc);
}
#endif

/*
 * Program the multicast filter. We use the 64-entry CAM filter
 * for perfect filtering. If there's more than 64 multicast addresses,
 * we use the hash filter insted.
 */
static void
vge_setmulti(sc)
	struct vge_softc	*sc;
{
	struct ifnet		*ifp;
	int			error = 0/*, h = 0*/;
	struct ifmultiaddr	*ifma;
	u_int32_t		h, hashes[2] = { 0, 0 };

	ifp = &sc->arpcom.ac_if;

	/* First, zot all the multicast entries. */
	vge_cam_clear(sc);
	CSR_WRITE_4(sc, VGE_MAR0, 0);
	CSR_WRITE_4(sc, VGE_MAR1, 0);

	/*
	 * If the user wants allmulti or promisc mode, enable reception
	 * of all multicast frames.
	 */
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		CSR_WRITE_4(sc, VGE_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, VGE_MAR1, 0xFFFFFFFF);
		return;
	}

	/* Now program new ones */
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		error = vge_cam_set(sc,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
		if (error)
			break;
	}

	/* If there were too many addresses, use the hash filter. */
	if (error) {
		vge_cam_clear(sc);

		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
#if __FreeBSD_version < 502113
			h = vge_mchash(LLADDR((struct sockaddr_dl *)
			    ifma->ifma_addr)) >> 26;
#else
			h = ether_crc32_be(LLADDR((struct sockaddr_dl *)
			    ifma->ifma_addr), ETHER_ADDR_LEN) >> 26;
#endif
			if (h < 32)
				hashes[0] |= (1 << h);
			else
				hashes[1] |= (1 << (h - 32));
		}

		CSR_WRITE_4(sc, VGE_MAR0, hashes[0]);
		CSR_WRITE_4(sc, VGE_MAR1, hashes[1]);
	}

	return;
}

static void
vge_reset(sc)
	struct vge_softc		*sc;
{
	register int		i;

	CSR_WRITE_1(sc, VGE_CRS1, VGE_CR1_SOFTRESET);

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(5);
		if ((CSR_READ_1(sc, VGE_CRS1) & VGE_CR1_SOFTRESET) == 0)
			break;
	}

	if (i == VGE_TIMEOUT) {
		device_printf(sc->vge_dev, "soft reset timed out");
		CSR_WRITE_1(sc, VGE_CRS3, VGE_CR3_STOP_FORCE);
		DELAY(2000);
	}

	DELAY(5000);

	CSR_SETBIT_1(sc, VGE_EECSR, VGE_EECSR_RELOAD);

	for (i = 0; i < VGE_TIMEOUT; i++) {
		DELAY(5);
		if ((CSR_READ_1(sc, VGE_EECSR) & VGE_EECSR_RELOAD) == 0)
			break;
	}

	if (i == VGE_TIMEOUT) {
		device_printf(sc->vge_dev, "EEPROM reload timed out\n");
		return;
	}

	CSR_CLRBIT_1(sc, VGE_CHIPCFG0, VGE_CHIPCFG0_PACPI);

	return;
}

/*
 * Probe for a VIA gigabit chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
vge_probe(dev)
	device_t		dev;
{
	struct vge_type		*t;
	struct vge_softc	*sc;

	t = vge_devs;
	sc = device_get_softc(dev);

	while (t->vge_name != NULL) {
		if ((pci_get_vendor(dev) == t->vge_vid) &&
		    (pci_get_device(dev) == t->vge_did)) {
			device_set_desc(dev, t->vge_name);
			return (BUS_PROBE_DEFAULT);
		}
		t++;
	}

	return (ENXIO);
}

static void
vge_dma_map_rx_desc(arg, segs, nseg, mapsize, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	bus_size_t		mapsize;
	int			error;
{

	struct vge_dmaload_arg	*ctx;
	struct vge_rx_desc	*d = NULL;

	if (error)
		return;

	ctx = arg;

	/* Signal error to caller if there's too many segments */
	if (nseg > ctx->vge_maxsegs) {
		ctx->vge_maxsegs = 0;
		return;
	}

	/*
	 * Map the segment array into descriptors.
	 */

	d = &ctx->sc->vge_ldata.vge_rx_list[ctx->vge_idx];

	/* If this descriptor is still owned by the chip, bail. */

	if (le32toh(d->vge_sts) & VGE_RDSTS_OWN) {
		device_printf(ctx->sc->vge_dev,
		    "tried to map busy descriptor\n");
		ctx->vge_maxsegs = 0;
		return;
	}

	d->vge_buflen = htole16(VGE_BUFLEN(segs[0].ds_len) | VGE_RXDESC_I);
	d->vge_addrlo = htole32(VGE_ADDR_LO(segs[0].ds_addr));
	d->vge_addrhi = htole16(VGE_ADDR_HI(segs[0].ds_addr) & 0xFFFF);
	d->vge_sts = 0;
	d->vge_ctl = 0;

	ctx->vge_maxsegs = 1;

	return;
}

static void
vge_dma_map_tx_desc(arg, segs, nseg, mapsize, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	bus_size_t		mapsize;
	int			error;
{
	struct vge_dmaload_arg	*ctx;
	struct vge_tx_desc	*d = NULL;
	struct vge_tx_frag	*f;
	int			i = 0;

	if (error)
		return;

	ctx = arg;

	/* Signal error to caller if there's too many segments */
	if (nseg > ctx->vge_maxsegs) {
		ctx->vge_maxsegs = 0;
		return;
	}

	/* Map the segment array into descriptors. */

	d = &ctx->sc->vge_ldata.vge_tx_list[ctx->vge_idx];

	/* If this descriptor is still owned by the chip, bail. */

	if (le32toh(d->vge_sts) & VGE_TDSTS_OWN) {
		ctx->vge_maxsegs = 0;
		return;
	}

	for (i = 0; i < nseg; i++) {
		f = &d->vge_frag[i];
		f->vge_buflen = htole16(VGE_BUFLEN(segs[i].ds_len));
		f->vge_addrlo = htole32(VGE_ADDR_LO(segs[i].ds_addr));
		f->vge_addrhi = htole16(VGE_ADDR_HI(segs[i].ds_addr) & 0xFFFF);
	}

	/* Argh. This chip does not autopad short frames */

	if (ctx->vge_m0->m_pkthdr.len < VGE_MIN_FRAMELEN) {
		f = &d->vge_frag[i];
		f->vge_buflen = htole16(VGE_BUFLEN(VGE_MIN_FRAMELEN -
		    ctx->vge_m0->m_pkthdr.len));
		f->vge_addrlo = htole32(VGE_ADDR_LO(segs[0].ds_addr));
		f->vge_addrhi = htole16(VGE_ADDR_HI(segs[0].ds_addr) & 0xFFFF);
		ctx->vge_m0->m_pkthdr.len = VGE_MIN_FRAMELEN;
		i++;
	}

	/*
	 * When telling the chip how many segments there are, we
	 * must use nsegs + 1 instead of just nsegs. Darned if I
	 * know why.
	 */
	i++;

	d->vge_sts = ctx->vge_m0->m_pkthdr.len << 16;
	d->vge_ctl = ctx->vge_flags|(i << 28)|VGE_TD_LS_NORM;

	if (ctx->vge_m0->m_pkthdr.len > ETHERMTU + ETHER_HDR_LEN)
		d->vge_ctl |= VGE_TDCTL_JUMBO;

	ctx->vge_maxsegs = nseg;

	return;
}

/*
 * Map a single buffer address.
 */

static void
vge_dma_map_addr(arg, segs, nseg, error)
	void			*arg;
	bus_dma_segment_t	*segs;
	int			nseg;
	int			error;
{
	bus_addr_t		*addr;

	if (error)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));
	addr = arg;
	*addr = segs->ds_addr;

	return;
}

static int
vge_allocmem(dev, sc)
	device_t		dev;
	struct vge_softc		*sc;
{
	int			error;
	int			nseg;
	int			i;

	/*
	 * Allocate map for RX mbufs.
	 */
	nseg = 32;
	error = bus_dma_tag_create(sc->vge_parent_tag, ETHER_ALIGN, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
	    NULL, MCLBYTES * nseg, nseg, MCLBYTES, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->vge_ldata.vge_mtag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/*
	 * Allocate map for TX descriptor list.
	 */
	error = bus_dma_tag_create(sc->vge_parent_tag, VGE_RING_ALIGN,
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
	    NULL, VGE_TX_LIST_SZ, 1, VGE_TX_LIST_SZ, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->vge_ldata.vge_tx_list_tag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for the TX ring */

	error = bus_dmamem_alloc(sc->vge_ldata.vge_tx_list_tag,
	    (void **)&sc->vge_ldata.vge_tx_list, BUS_DMA_NOWAIT | BUS_DMA_ZERO,
	    &sc->vge_ldata.vge_tx_list_map);
	if (error)
		return (ENOMEM);

	/* Load the map for the TX ring. */

	error = bus_dmamap_load(sc->vge_ldata.vge_tx_list_tag,
	     sc->vge_ldata.vge_tx_list_map, sc->vge_ldata.vge_tx_list,
	     VGE_TX_LIST_SZ, vge_dma_map_addr,
	     &sc->vge_ldata.vge_tx_list_addr, BUS_DMA_NOWAIT);

	/* Create DMA maps for TX buffers */

	for (i = 0; i < VGE_TX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->vge_ldata.vge_mtag, 0,
			    &sc->vge_ldata.vge_tx_dmamap[i]);
		if (error) {
			device_printf(dev, "can't create DMA map for TX\n");
			return (ENOMEM);
		}
	}

	/*
	 * Allocate map for RX descriptor list.
	 */
	error = bus_dma_tag_create(sc->vge_parent_tag, VGE_RING_ALIGN,
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL,
	    NULL, VGE_TX_LIST_SZ, 1, VGE_TX_LIST_SZ, BUS_DMA_ALLOCNOW,
	    NULL, NULL, &sc->vge_ldata.vge_rx_list_tag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for the RX ring */

	error = bus_dmamem_alloc(sc->vge_ldata.vge_rx_list_tag,
	    (void **)&sc->vge_ldata.vge_rx_list, BUS_DMA_NOWAIT | BUS_DMA_ZERO,
	    &sc->vge_ldata.vge_rx_list_map);
	if (error)
		return (ENOMEM);

	/* Load the map for the RX ring. */

	error = bus_dmamap_load(sc->vge_ldata.vge_rx_list_tag,
	     sc->vge_ldata.vge_rx_list_map, sc->vge_ldata.vge_rx_list,
	     VGE_TX_LIST_SZ, vge_dma_map_addr,
	     &sc->vge_ldata.vge_rx_list_addr, BUS_DMA_NOWAIT);

	/* Create DMA maps for RX buffers */

	for (i = 0; i < VGE_RX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->vge_ldata.vge_mtag, 0,
			    &sc->vge_ldata.vge_rx_dmamap[i]);
		if (error) {
			device_printf(dev, "can't create DMA map for RX\n");
			return (ENOMEM);
		}
	}

	return (0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
vge_attach(dev)
	device_t		dev;
{
	u_char			eaddr[ETHER_ADDR_LEN];
	struct vge_softc	*sc;
	struct ifnet		*ifp;
	int			unit, error = 0, rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	sc->vge_dev = dev;

	mtx_init(&sc->vge_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = VGE_PCI_LOMEM;
	sc->vge_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->vge_res == NULL) {
		printf ("vge%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->vge_btag = rman_get_bustag(sc->vge_res);
	sc->vge_bhandle = rman_get_bushandle(sc->vge_res);

	/* Allocate interrupt */
	rid = 0;
	sc->vge_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);

	if (sc->vge_irq == NULL) {
		printf("vge%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	/* Reset the adapter. */
	vge_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	vge_read_eeprom(sc, (caddr_t)eaddr, VGE_EE_EADDR, 3, 0);

	sc->vge_unit = unit;
	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

#if __FreeBSD_version < 502113
	printf("vge%d: Ethernet address: %6D\n", unit, eaddr, ":");
#endif

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define VGE_NSEG_NEW 32
	error = bus_dma_tag_create(NULL,	/* parent */
			1, 0,			/* alignment, boundary */
			BUS_SPACE_MAXADDR_32BIT,/* lowaddr */
			BUS_SPACE_MAXADDR,	/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MAXBSIZE, VGE_NSEG_NEW,	/* maxsize, nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			BUS_DMA_ALLOCNOW,	/* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->vge_parent_tag);
	if (error)
		goto fail;

	error = vge_allocmem(dev, sc);

	if (error)
		goto fail;

	/* Do MII setup */
	if (mii_phy_probe(dev, &sc->vge_miibus,
	    vge_ifmedia_upd, vge_ifmedia_sts)) {
		printf("vge%d: MII without any phy!\n", sc->vge_unit);
		error = ENXIO;
		goto fail;
	}

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = vge_ioctl;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	ifp->if_start = vge_start;
	ifp->if_hwassist = VGE_CSUM_FEATURES;
	ifp->if_capabilities |= IFCAP_HWCSUM|IFCAP_VLAN_HWTAGGING;
#ifdef DEVICE_POLLING
#ifdef IFCAP_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif
#endif
	ifp->if_watchdog = vge_watchdog;
	ifp->if_init = vge_init;
	ifp->if_baudrate = 1000000000;
	ifp->if_snd.ifq_maxlen = VGE_IFQ_MAXLEN;
	ifp->if_capenable = ifp->if_capabilities;

	TASK_INIT(&sc->vge_txtask, 0, vge_tx_task, ifp);

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->vge_irq, INTR_TYPE_NET|INTR_MPSAFE,
	    vge_intr, sc, &sc->vge_intrhand);

	if (error) {
		printf("vge%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error)
		vge_detach(dev);

	return (error);
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
static int
vge_detach(dev)
	device_t		dev;
{
	struct vge_softc		*sc;
	struct ifnet		*ifp;
	int			i;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->vge_mtx), ("vge mutex not initialized"));
	ifp = &sc->arpcom.ac_if;

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		vge_stop(sc);
		/*
		 * Force off the IFF_UP flag here, in case someone
		 * still had a BPF descriptor attached to this
		 * interface. If they do, ether_ifattach() will cause
		 * the BPF code to try and clear the promisc mode
		 * flag, which will bubble down to vge_ioctl(),
		 * which will try to call vge_init() again. This will
		 * turn the NIC back on and restart the MII ticker,
		 * which will panic the system when the kernel tries
		 * to invoke the vge_tick() function that isn't there
		 * anymore.
		 */
		ifp->if_flags &= ~IFF_UP;
		ether_ifdetach(ifp);
	}
	if (sc->vge_miibus)
		device_delete_child(dev, sc->vge_miibus);
	bus_generic_detach(dev);

	if (sc->vge_intrhand)
		bus_teardown_intr(dev, sc->vge_irq, sc->vge_intrhand);
	if (sc->vge_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->vge_irq);
	if (sc->vge_res)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    VGE_PCI_LOMEM, sc->vge_res);

	/* Unload and free the RX DMA ring memory and map */

	if (sc->vge_ldata.vge_rx_list_tag) {
		bus_dmamap_unload(sc->vge_ldata.vge_rx_list_tag,
		    sc->vge_ldata.vge_rx_list_map);
		bus_dmamem_free(sc->vge_ldata.vge_rx_list_tag,
		    sc->vge_ldata.vge_rx_list,
		    sc->vge_ldata.vge_rx_list_map);
		bus_dma_tag_destroy(sc->vge_ldata.vge_rx_list_tag);
	}

	/* Unload and free the TX DMA ring memory and map */

	if (sc->vge_ldata.vge_tx_list_tag) {
		bus_dmamap_unload(sc->vge_ldata.vge_tx_list_tag,
		    sc->vge_ldata.vge_tx_list_map);
		bus_dmamem_free(sc->vge_ldata.vge_tx_list_tag,
		    sc->vge_ldata.vge_tx_list,
		    sc->vge_ldata.vge_tx_list_map);
		bus_dma_tag_destroy(sc->vge_ldata.vge_tx_list_tag);
	}

	/* Destroy all the RX and TX buffer maps */

	if (sc->vge_ldata.vge_mtag) {
		for (i = 0; i < VGE_TX_DESC_CNT; i++)
			bus_dmamap_destroy(sc->vge_ldata.vge_mtag,
			    sc->vge_ldata.vge_tx_dmamap[i]);
		for (i = 0; i < VGE_RX_DESC_CNT; i++)
			bus_dmamap_destroy(sc->vge_ldata.vge_mtag,
			    sc->vge_ldata.vge_rx_dmamap[i]);
		bus_dma_tag_destroy(sc->vge_ldata.vge_mtag);
	}

	if (sc->vge_parent_tag)
		bus_dma_tag_destroy(sc->vge_parent_tag);

	mtx_destroy(&sc->vge_mtx);

	return (0);
}

static int
vge_newbuf(sc, idx, m)
	struct vge_softc	*sc;
	int			idx;
	struct mbuf		*m;
{
	struct vge_dmaload_arg	arg;
	struct mbuf		*n = NULL;
	int			i, error;

	if (m == NULL) {
		n = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (n == NULL)
			return (ENOBUFS);
		m = n;
	} else
		m->m_data = m->m_ext.ext_buf;


#ifdef VGE_FIXUP_RX
	/*
	 * This is part of an evil trick to deal with non-x86 platforms.
	 * The VIA chip requires RX buffers to be aligned on 32-bit
	 * boundaries, but that will hose non-x86 machines. To get around
	 * this, we leave some empty space at the start of each buffer
	 * and for non-x86 hosts, we copy the buffer back two bytes
	 * to achieve word alignment. This is slightly more efficient
	 * than allocating a new buffer, copying the contents, and
	 * discarding the old buffer.
	 */
	m->m_len = m->m_pkthdr.len = MCLBYTES - VGE_ETHER_ALIGN;
	m_adj(m, VGE_ETHER_ALIGN);
#else
	m->m_len = m->m_pkthdr.len = MCLBYTES;
#endif

	arg.sc = sc;
	arg.vge_idx = idx;
	arg.vge_maxsegs = 1;
	arg.vge_flags = 0;

	error = bus_dmamap_load_mbuf(sc->vge_ldata.vge_mtag,
	    sc->vge_ldata.vge_rx_dmamap[idx], m, vge_dma_map_rx_desc,
	    &arg, BUS_DMA_NOWAIT);
	if (error || arg.vge_maxsegs != 1) {
		if (n != NULL)
			m_freem(n);
		return (ENOMEM);
	}

	/*
	 * Note: the manual fails to document the fact that for
	 * proper opration, the driver needs to replentish the RX
	 * DMA ring 4 descriptors at a time (rather than one at a
	 * time, like most chips). We can allocate the new buffers
	 * but we should not set the OWN bits until we're ready
	 * to hand back 4 of them in one shot.
	 */

#define VGE_RXCHUNK 4
	sc->vge_rx_consumed++;
	if (sc->vge_rx_consumed == VGE_RXCHUNK) {
		for (i = idx; i != idx - sc->vge_rx_consumed; i--)
			sc->vge_ldata.vge_rx_list[i].vge_sts |=
			    htole32(VGE_RDSTS_OWN);
		sc->vge_rx_consumed = 0;
	}

	sc->vge_ldata.vge_rx_mbuf[idx] = m;

	bus_dmamap_sync(sc->vge_ldata.vge_mtag,
	    sc->vge_ldata.vge_rx_dmamap[idx],
	    BUS_DMASYNC_PREREAD);

	return (0);
}

static int
vge_tx_list_init(sc)
	struct vge_softc		*sc;
{
	bzero ((char *)sc->vge_ldata.vge_tx_list, VGE_TX_LIST_SZ);
	bzero ((char *)&sc->vge_ldata.vge_tx_mbuf,
	    (VGE_TX_DESC_CNT * sizeof(struct mbuf *)));

	bus_dmamap_sync(sc->vge_ldata.vge_tx_list_tag,
	    sc->vge_ldata.vge_tx_list_map, BUS_DMASYNC_PREWRITE);
	sc->vge_ldata.vge_tx_prodidx = 0;
	sc->vge_ldata.vge_tx_considx = 0;
	sc->vge_ldata.vge_tx_free = VGE_TX_DESC_CNT;

	return (0);
}

static int
vge_rx_list_init(sc)
	struct vge_softc		*sc;
{
	int			i;

	bzero ((char *)sc->vge_ldata.vge_rx_list, VGE_RX_LIST_SZ);
	bzero ((char *)&sc->vge_ldata.vge_rx_mbuf,
	    (VGE_RX_DESC_CNT * sizeof(struct mbuf *)));

	sc->vge_rx_consumed = 0;

	for (i = 0; i < VGE_RX_DESC_CNT; i++) {
		if (vge_newbuf(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
	}

	/* Flush the RX descriptors */

	bus_dmamap_sync(sc->vge_ldata.vge_rx_list_tag,
	    sc->vge_ldata.vge_rx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->vge_ldata.vge_rx_prodidx = 0;
	sc->vge_rx_consumed = 0;
	sc->vge_head = sc->vge_tail = NULL;

	return (0);
}

#ifdef VGE_FIXUP_RX
static __inline void
vge_fixup_rx(m)
	struct mbuf		*m;
{
	int			i;
	uint16_t		*src, *dst;

	src = mtod(m, uint16_t *);
	dst = src - 1;

	for (i = 0; i < (m->m_len / sizeof(uint16_t) + 1); i++)
		*dst++ = *src++;

	m->m_data -= ETHER_ALIGN;

	return;
}
#endif

/*
 * RX handler. We support the reception of jumbo frames that have
 * been fragmented across multiple 2K mbuf cluster buffers.
 */
static void
vge_rxeof(sc)
	struct vge_softc	*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			i, total_len;
	int			lim = 0;
	struct vge_rx_desc	*cur_rx;
	u_int32_t		rxstat, rxctl;

	VGE_LOCK_ASSERT(sc);
	ifp = &sc->arpcom.ac_if;
	i = sc->vge_ldata.vge_rx_prodidx;

	/* Invalidate the descriptor memory */

	bus_dmamap_sync(sc->vge_ldata.vge_rx_list_tag,
	    sc->vge_ldata.vge_rx_list_map,
	    BUS_DMASYNC_POSTREAD);

	while (!VGE_OWN(&sc->vge_ldata.vge_rx_list[i])) {

#ifdef DEVICE_POLLING
		if (ifp->if_flags & IFF_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif /* DEVICE_POLLING */

		cur_rx = &sc->vge_ldata.vge_rx_list[i];
		m = sc->vge_ldata.vge_rx_mbuf[i];
		total_len = VGE_RXBYTES(cur_rx);
		rxstat = le32toh(cur_rx->vge_sts);
		rxctl = le32toh(cur_rx->vge_ctl);

		/* Invalidate the RX mbuf and unload its map */

		bus_dmamap_sync(sc->vge_ldata.vge_mtag,
		    sc->vge_ldata.vge_rx_dmamap[i],
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->vge_ldata.vge_mtag,
		    sc->vge_ldata.vge_rx_dmamap[i]);

		/*
		 * If the 'start of frame' bit is set, this indicates
		 * either the first fragment in a multi-fragment receive,
		 * or an intermediate fragment. Either way, we want to
		 * accumulate the buffers.
		 */
		if (rxstat & VGE_RXPKT_SOF) {
			m->m_len = MCLBYTES - VGE_ETHER_ALIGN;
			if (sc->vge_head == NULL)
				sc->vge_head = sc->vge_tail = m;
			else {
				m->m_flags &= ~M_PKTHDR;
				sc->vge_tail->m_next = m;
				sc->vge_tail = m;
			}
			vge_newbuf(sc, i, NULL);
			VGE_RX_DESC_INC(i);
			continue;
		}

		/*
		 * Bad/error frames will have the RXOK bit cleared.
		 * However, there's one error case we want to allow:
		 * if a VLAN tagged frame arrives and the chip can't
		 * match it against the CAM filter, it considers this
		 * a 'VLAN CAM filter miss' and clears the 'RXOK' bit.
		 * We don't want to drop the frame though: our VLAN
		 * filtering is done in software.
		 */
		if (!(rxstat & VGE_RDSTS_RXOK) && !(rxstat & VGE_RDSTS_VIDM)
		    && !(rxstat & VGE_RDSTS_CSUMERR)) {
			ifp->if_ierrors++;
			/*
			 * If this is part of a multi-fragment packet,
			 * discard all the pieces.
			 */
			if (sc->vge_head != NULL) {
				m_freem(sc->vge_head);
				sc->vge_head = sc->vge_tail = NULL;
			}
			vge_newbuf(sc, i, m);
			VGE_RX_DESC_INC(i);
			continue;
		}

		/*
		 * If allocating a replacement mbuf fails,
		 * reload the current one.
		 */

		if (vge_newbuf(sc, i, NULL)) {
			ifp->if_ierrors++;
			if (sc->vge_head != NULL) {
				m_freem(sc->vge_head);
				sc->vge_head = sc->vge_tail = NULL;
			}
			vge_newbuf(sc, i, m);
			VGE_RX_DESC_INC(i);
			continue;
		}

		VGE_RX_DESC_INC(i);

		if (sc->vge_head != NULL) {
			m->m_len = total_len % (MCLBYTES - VGE_ETHER_ALIGN);
			/*
			 * Special case: if there's 4 bytes or less
			 * in this buffer, the mbuf can be discarded:
			 * the last 4 bytes is the CRC, which we don't
			 * care about anyway.
			 */
			if (m->m_len <= ETHER_CRC_LEN) {
				sc->vge_tail->m_len -=
				    (ETHER_CRC_LEN - m->m_len);
				m_freem(m);
			} else {
				m->m_len -= ETHER_CRC_LEN;
				m->m_flags &= ~M_PKTHDR;
				sc->vge_tail->m_next = m;
			}
			m = sc->vge_head;
			sc->vge_head = sc->vge_tail = NULL;
			m->m_pkthdr.len = total_len - ETHER_CRC_LEN;
		} else
			m->m_pkthdr.len = m->m_len =
			    (total_len - ETHER_CRC_LEN);

#ifdef VGE_FIXUP_RX
		vge_fixup_rx(m);
#endif
		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		/* Do RX checksumming if enabled */
		if (ifp->if_capenable & IFCAP_RXCSUM) {

			/* Check IP header checksum */
			if (rxctl & VGE_RDCTL_IPPKT)
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
			if (rxctl & VGE_RDCTL_IPCSUMOK)
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;

			/* Check TCP/UDP checksum */
			if (rxctl & (VGE_RDCTL_TCPPKT|VGE_RDCTL_UDPPKT) &&
			    rxctl & VGE_RDCTL_PROTOCSUMOK) {
				m->m_pkthdr.csum_flags |=
				    CSUM_DATA_VALID|CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			}
		}

		if (rxstat & VGE_RDSTS_VTAG)
			VLAN_INPUT_TAG(ifp, m,
			    ntohs((rxctl & VGE_RDCTL_VLANID)), continue);

		VGE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		VGE_LOCK(sc);

		lim++;
		if (lim == VGE_RX_DESC_CNT)
			break;

	}

	/* Flush the RX DMA ring */

	bus_dmamap_sync(sc->vge_ldata.vge_rx_list_tag,
	    sc->vge_ldata.vge_rx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->vge_ldata.vge_rx_prodidx = i;
	CSR_WRITE_2(sc, VGE_RXDESC_RESIDUECNT, lim);


	return;
}

static void
vge_txeof(sc)
	struct vge_softc		*sc;
{
	struct ifnet		*ifp;
	u_int32_t		txstat;
	int			idx;

	ifp = &sc->arpcom.ac_if;
	idx = sc->vge_ldata.vge_tx_considx;

	/* Invalidate the TX descriptor list */

	bus_dmamap_sync(sc->vge_ldata.vge_tx_list_tag,
	    sc->vge_ldata.vge_tx_list_map,
	    BUS_DMASYNC_POSTREAD);

	while (idx != sc->vge_ldata.vge_tx_prodidx) {

		txstat = le32toh(sc->vge_ldata.vge_tx_list[idx].vge_sts);
		if (txstat & VGE_TDSTS_OWN)
			break;

		m_freem(sc->vge_ldata.vge_tx_mbuf[idx]);
		sc->vge_ldata.vge_tx_mbuf[idx] = NULL;
		bus_dmamap_unload(sc->vge_ldata.vge_mtag,
		    sc->vge_ldata.vge_tx_dmamap[idx]);
		if (txstat & (VGE_TDSTS_EXCESSCOLL|VGE_TDSTS_COLL))
			ifp->if_collisions++;
		if (txstat & VGE_TDSTS_TXERR)
			ifp->if_oerrors++;
		else
			ifp->if_opackets++;

		sc->vge_ldata.vge_tx_free++;
		VGE_TX_DESC_INC(idx);
	}

	/* No changes made to the TX ring, so no flush needed */

	if (idx != sc->vge_ldata.vge_tx_considx) {
		sc->vge_ldata.vge_tx_considx = idx;
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
	}

	/*
	 * If not all descriptors have been released reaped yet,
	 * reload the timer so that we will eventually get another
	 * interrupt that will cause us to re-enter this routine.
	 * This is done in case the transmitter has gone idle.
	 */
	if (sc->vge_ldata.vge_tx_free != VGE_TX_DESC_CNT) {
		CSR_WRITE_1(sc, VGE_CRS1, VGE_CR1_TIMER0_ENABLE);
	}

	return;
}

static void
vge_tick(xsc)
	void			*xsc;
{
	struct vge_softc	*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;
	VGE_LOCK(sc);
	mii = device_get_softc(sc->vge_miibus);

	mii_tick(mii);
	if (sc->vge_link) {
		if (!(mii->mii_media_status & IFM_ACTIVE)) {
			sc->vge_link = 0;
			if_link_state_change(&sc->arpcom.ac_if,
			    LINK_STATE_DOWN);
		}
	} else {
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			sc->vge_link = 1;
			if_link_state_change(&sc->arpcom.ac_if,
			    LINK_STATE_UP);
#if __FreeBSD_version < 502114
			if (ifp->if_snd.ifq_head != NULL)
#else
			if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
#endif
				taskqueue_enqueue(taskqueue_swi,
				    &sc->vge_txtask);
		}
	}

	VGE_UNLOCK(sc);

	return;
}

#ifdef DEVICE_POLLING
static void
vge_poll (struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct vge_softc *sc = ifp->if_softc;

	VGE_LOCK(sc);
#ifdef IFCAP_POLLING
	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}
#endif
	if (cmd == POLL_DEREGISTER) { /* final call, enable interrupts */
		CSR_WRITE_4(sc, VGE_IMR, VGE_INTRS);
		CSR_WRITE_4(sc, VGE_ISR, 0xFFFFFFFF);
		CSR_WRITE_1(sc, VGE_CRS3, VGE_CR3_INT_GMSK);
		goto done;
	}

	sc->rxcycles = count;
	vge_rxeof(sc);
	vge_txeof(sc);

#if __FreeBSD_version < 502114
	if (ifp->if_snd.ifq_head != NULL)
#else
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
#endif
		taskqueue_enqueue(taskqueue_swi, &sc->vge_txtask);

	if (cmd == POLL_AND_CHECK_STATUS) { /* also check status register */
		u_int32_t       status;
		status = CSR_READ_4(sc, VGE_ISR);
		if (status == 0xFFFFFFFF)
			goto done;
		if (status)
			CSR_WRITE_4(sc, VGE_ISR, status);

		/*
		 * XXX check behaviour on receiver stalls.
		 */

		if (status & VGE_ISR_TXDMA_STALL ||
		    status & VGE_ISR_RXDMA_STALL)
			vge_init(sc);

		if (status & (VGE_ISR_RXOFLOW|VGE_ISR_RXNODESC)) {
			vge_rxeof(sc);
			ifp->if_ierrors++;
			CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_RUN);
			CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_WAK);
		}
	}
done:
	VGE_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

static void
vge_intr(arg)
	void			*arg;
{
	struct vge_softc	*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;

	if (sc->suspended) {
		return;
	}

	VGE_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP)) {
		VGE_UNLOCK(sc);
		return;
	}

#ifdef DEVICE_POLLING
	if  (ifp->if_flags & IFF_POLLING)
		goto done;
	if (
#ifdef IFCAP_POLLING
	    (ifp->if_capenable & IFCAP_POLLING) &&
#endif
	    ether_poll_register(vge_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_4(sc, VGE_IMR, 0);
		CSR_WRITE_1(sc, VGE_CRC3, VGE_CR3_INT_GMSK);
		vge_poll(ifp, 0, 1);
		goto done;
	}

#endif /* DEVICE_POLLING */

	/* Disable interrupts */
	CSR_WRITE_1(sc, VGE_CRC3, VGE_CR3_INT_GMSK);

	for (;;) {

		status = CSR_READ_4(sc, VGE_ISR);
		/* If the card has gone away the read returns 0xffff. */
		if (status == 0xFFFFFFFF)
			break;

		if (status)
			CSR_WRITE_4(sc, VGE_ISR, status);

		if ((status & VGE_INTRS) == 0)
			break;

		if (status & (VGE_ISR_RXOK|VGE_ISR_RXOK_HIPRIO))
			vge_rxeof(sc);

		if (status & (VGE_ISR_RXOFLOW|VGE_ISR_RXNODESC)) {
			vge_rxeof(sc);
			CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_RUN);
			CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_WAK);
		}

		if (status & (VGE_ISR_TXOK0|VGE_ISR_TIMER0))
			vge_txeof(sc);

		if (status & (VGE_ISR_TXDMA_STALL|VGE_ISR_RXDMA_STALL))
			vge_init(sc);

		if (status & VGE_ISR_LINKSTS)
			vge_tick(sc);
	}

	/* Re-enable interrupts */
	CSR_WRITE_1(sc, VGE_CRS3, VGE_CR3_INT_GMSK);

#ifdef DEVICE_POLLING
done:
#endif
	VGE_UNLOCK(sc);

#if __FreeBSD_version < 502114
	if (ifp->if_snd.ifq_head != NULL)
#else
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
#endif
		taskqueue_enqueue(taskqueue_swi, &sc->vge_txtask);

	return;
}

static int
vge_encap(sc, m_head, idx)
	struct vge_softc	*sc;
	struct mbuf		*m_head;
	int			idx;
{
	struct mbuf		*m_new = NULL;
	struct vge_dmaload_arg	arg;
	bus_dmamap_t		map;
	int			error;
	struct m_tag		*mtag;

	if (sc->vge_ldata.vge_tx_free <= 2)
		return (EFBIG);

	arg.vge_flags = 0;

	if (m_head->m_pkthdr.csum_flags & CSUM_IP)
		arg.vge_flags |= VGE_TDCTL_IPCSUM;
	if (m_head->m_pkthdr.csum_flags & CSUM_TCP)
		arg.vge_flags |= VGE_TDCTL_TCPCSUM;
	if (m_head->m_pkthdr.csum_flags & CSUM_UDP)
		arg.vge_flags |= VGE_TDCTL_UDPCSUM;

	arg.sc = sc;
	arg.vge_idx = idx;
	arg.vge_m0 = m_head;
	arg.vge_maxsegs = VGE_TX_FRAGS;

	map = sc->vge_ldata.vge_tx_dmamap[idx];
	error = bus_dmamap_load_mbuf(sc->vge_ldata.vge_mtag, map,
	    m_head, vge_dma_map_tx_desc, &arg, BUS_DMA_NOWAIT);

	if (error && error != EFBIG) {
		printf("vge%d: can't map mbuf (error %d)\n",
		    sc->vge_unit, error);
		return (ENOBUFS);
	}

	/* Too many segments to map, coalesce into a single mbuf */

	if (error || arg.vge_maxsegs == 0) {
		m_new = m_defrag(m_head, M_DONTWAIT);
		if (m_new == NULL)
			return (1);
		else
			m_head = m_new;

		arg.sc = sc;
		arg.vge_m0 = m_head;
		arg.vge_idx = idx;
		arg.vge_maxsegs = 1;

		error = bus_dmamap_load_mbuf(sc->vge_ldata.vge_mtag, map,
		    m_head, vge_dma_map_tx_desc, &arg, BUS_DMA_NOWAIT);
		if (error) {
			printf("vge%d: can't map mbuf (error %d)\n",
			    sc->vge_unit, error);
			return (EFBIG);
		}
	}

	sc->vge_ldata.vge_tx_mbuf[idx] = m_head;
	sc->vge_ldata.vge_tx_free--;

	/*
	 * Set up hardware VLAN tagging.
	 */

	mtag = VLAN_OUTPUT_TAG(&sc->arpcom.ac_if, m_head);
	if (mtag != NULL)
		sc->vge_ldata.vge_tx_list[idx].vge_ctl |=
		    htole32(htons(VLAN_TAG_VALUE(mtag)) | VGE_TDCTL_VTAG);

	sc->vge_ldata.vge_tx_list[idx].vge_sts |= htole32(VGE_TDSTS_OWN);

	return (0);
}

static void
vge_tx_task(arg, npending)
	void			*arg;
	int			npending;
{
	struct ifnet		*ifp;

	ifp = arg;
	vge_start(ifp);

	return;
}

/*
 * Main transmit routine.
 */

static void
vge_start(ifp)
	struct ifnet		*ifp;
{
	struct vge_softc	*sc;
	struct mbuf		*m_head = NULL;
	int			idx, pidx = 0;

	sc = ifp->if_softc;
	VGE_LOCK(sc);

	if (!sc->vge_link || ifp->if_flags & IFF_OACTIVE) {
		VGE_UNLOCK(sc);
		return;
	}

#if __FreeBSD_version < 502114
	if (ifp->if_snd.ifq_head == NULL) {
#else
	if (IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
#endif
		VGE_UNLOCK(sc);
		return;
	}

	idx = sc->vge_ldata.vge_tx_prodidx;

	pidx = idx - 1;
	if (pidx < 0)
		pidx = VGE_TX_DESC_CNT - 1;


	while (sc->vge_ldata.vge_tx_mbuf[idx] == NULL) {
#if __FreeBSD_version < 502114
		IF_DEQUEUE(&ifp->if_snd, m_head);
#else
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
#endif
		if (m_head == NULL)
			break;

		if (vge_encap(sc, m_head, idx)) {
#if __FreeBSD_version >= 502114
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
#else
			IF_PREPEND(&ifp->if_snd, m_head);
#endif
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		sc->vge_ldata.vge_tx_list[pidx].vge_frag[0].vge_buflen |=
		    htole16(VGE_TXDESC_Q);

		pidx = idx;
		VGE_TX_DESC_INC(idx);

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);
	}

	if (idx == sc->vge_ldata.vge_tx_prodidx) {
		VGE_UNLOCK(sc);
		return;
	}

	/* Flush the TX descriptors */

	bus_dmamap_sync(sc->vge_ldata.vge_tx_list_tag,
	    sc->vge_ldata.vge_tx_list_map,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	/* Issue a transmit command. */
	CSR_WRITE_2(sc, VGE_TXQCSRS, VGE_TXQCSR_WAK0);

	sc->vge_ldata.vge_tx_prodidx = idx;

	/*
	 * Use the countdown timer for interrupt moderation.
	 * 'TX done' interrupts are disabled. Instead, we reset the
	 * countdown timer, which will begin counting until it hits
	 * the value in the SSTIMER register, and then trigger an
	 * interrupt. Each time we set the TIMER0_ENABLE bit, the
	 * the timer count is reloaded. Only when the transmitter
	 * is idle will the timer hit 0 and an interrupt fire.
	 */
	CSR_WRITE_1(sc, VGE_CRS1, VGE_CR1_TIMER0_ENABLE);

	VGE_UNLOCK(sc);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void
vge_init(xsc)
	void			*xsc;
{
	struct vge_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii;
	int			i;

	VGE_LOCK(sc);
	mii = device_get_softc(sc->vge_miibus);

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	vge_stop(sc);
	vge_reset(sc);

	/*
	 * Initialize the RX and TX descriptors and mbufs.
	 */

	vge_rx_list_init(sc);
	vge_tx_list_init(sc);

	/* Set our station address */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		CSR_WRITE_1(sc, VGE_PAR0 + i, sc->arpcom.ac_enaddr[i]);

	/*
	 * Set receive FIFO threshold. Also allow transmission and
	 * reception of VLAN tagged frames.
	 */
	CSR_CLRBIT_1(sc, VGE_RXCFG, VGE_RXCFG_FIFO_THR|VGE_RXCFG_VTAGOPT);
	CSR_SETBIT_1(sc, VGE_RXCFG, VGE_RXFIFOTHR_128BYTES|VGE_VTAG_OPT2);

	/* Set DMA burst length */
	CSR_CLRBIT_1(sc, VGE_DMACFG0, VGE_DMACFG0_BURSTLEN);
	CSR_SETBIT_1(sc, VGE_DMACFG0, VGE_DMABURST_128);

	CSR_SETBIT_1(sc, VGE_TXCFG, VGE_TXCFG_ARB_PRIO|VGE_TXCFG_NONBLK);

	/* Set collision backoff algorithm */
	CSR_CLRBIT_1(sc, VGE_CHIPCFG1, VGE_CHIPCFG1_CRANDOM|
	    VGE_CHIPCFG1_CAP|VGE_CHIPCFG1_MBA|VGE_CHIPCFG1_BAKOPT);
	CSR_SETBIT_1(sc, VGE_CHIPCFG1, VGE_CHIPCFG1_OFSET);

	/* Disable LPSEL field in priority resolution */
	CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_LPSEL_DIS);

	/*
	 * Load the addresses of the DMA queues into the chip.
	 * Note that we only use one transmit queue.
	 */

	CSR_WRITE_4(sc, VGE_TXDESC_ADDR_LO0,
	    VGE_ADDR_LO(sc->vge_ldata.vge_tx_list_addr));
	CSR_WRITE_2(sc, VGE_TXDESCNUM, VGE_TX_DESC_CNT - 1);

	CSR_WRITE_4(sc, VGE_RXDESC_ADDR_LO,
	    VGE_ADDR_LO(sc->vge_ldata.vge_rx_list_addr));
	CSR_WRITE_2(sc, VGE_RXDESCNUM, VGE_RX_DESC_CNT - 1);
	CSR_WRITE_2(sc, VGE_RXDESC_RESIDUECNT, VGE_RX_DESC_CNT);

	/* Enable and wake up the RX descriptor queue */
	CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_RUN);
	CSR_WRITE_1(sc, VGE_RXQCSRS, VGE_RXQCSR_WAK);

	/* Enable the TX descriptor queue */
	CSR_WRITE_2(sc, VGE_TXQCSRS, VGE_TXQCSR_RUN0);

	/* Set up the receive filter -- allow large frames for VLANs. */
	CSR_WRITE_1(sc, VGE_RXCTL, VGE_RXCTL_RX_UCAST|VGE_RXCTL_RX_GIANT);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		CSR_SETBIT_1(sc, VGE_RXCTL, VGE_RXCTL_RX_PROMISC);
	}

	/* Set capture broadcast bit to capture broadcast frames. */
	if (ifp->if_flags & IFF_BROADCAST) {
		CSR_SETBIT_1(sc, VGE_RXCTL, VGE_RXCTL_RX_BCAST);
	}

	/* Set multicast bit to capture multicast frames. */
	if (ifp->if_flags & IFF_MULTICAST) {
		CSR_SETBIT_1(sc, VGE_RXCTL, VGE_RXCTL_RX_MCAST);
	}

	/* Init the cam filter. */
	vge_cam_clear(sc);

	/* Init the multicast filter. */
	vge_setmulti(sc);

	/* Enable flow control */

	CSR_WRITE_1(sc, VGE_CRS2, 0x8B);

	/* Enable jumbo frame reception (if desired) */

	/* Start the MAC. */
	CSR_WRITE_1(sc, VGE_CRC0, VGE_CR0_STOP);
	CSR_WRITE_1(sc, VGE_CRS1, VGE_CR1_NOPOLL);
	CSR_WRITE_1(sc, VGE_CRS0,
	    VGE_CR0_TX_ENABLE|VGE_CR0_RX_ENABLE|VGE_CR0_START);

	/*
	 * Configure one-shot timer for microsecond
	 * resulution and load it for 500 usecs.
	 */
	CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_TIMER0_RES);
	CSR_WRITE_2(sc, VGE_SSTIMER, 400);

	/*
	 * Configure interrupt moderation for receive. Enable
	 * the holdoff counter and load it, and set the RX
	 * suppression count to the number of descriptors we
	 * want to allow before triggering an interrupt.
	 * The holdoff timer is in units of 20 usecs.
	 */

#ifdef notyet
	CSR_WRITE_1(sc, VGE_INTCTL1, VGE_INTCTL_TXINTSUP_DISABLE);
	/* Select the interrupt holdoff timer page. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_INTHLDOFF);
	CSR_WRITE_1(sc, VGE_INTHOLDOFF, 10); /* ~200 usecs */

	/* Enable use of the holdoff timer. */
	CSR_WRITE_1(sc, VGE_CRS3, VGE_CR3_INT_HOLDOFF);
	CSR_WRITE_1(sc, VGE_INTCTL1, VGE_INTCTL_SC_RELOAD);

	/* Select the RX suppression threshold page. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_RXSUPPTHR);
	CSR_WRITE_1(sc, VGE_RXSUPPTHR, 64); /* interrupt after 64 packets */

	/* Restore the page select bits. */
	CSR_CLRBIT_1(sc, VGE_CAMCTL, VGE_CAMCTL_PAGESEL);
	CSR_SETBIT_1(sc, VGE_CAMCTL, VGE_PAGESEL_MAR);
#endif

#ifdef DEVICE_POLLING
	/*
	 * Disable interrupts if we are polling.
	 */
	if (ifp->if_flags & IFF_POLLING) {
		CSR_WRITE_4(sc, VGE_IMR, 0);
		CSR_WRITE_1(sc, VGE_CRC3, VGE_CR3_INT_GMSK);
	} else	/* otherwise ... */
#endif /* DEVICE_POLLING */
	{
	/*
	 * Enable interrupts.
	 */
		CSR_WRITE_4(sc, VGE_IMR, VGE_INTRS);
		CSR_WRITE_4(sc, VGE_ISR, 0);
		CSR_WRITE_1(sc, VGE_CRS3, VGE_CR3_INT_GMSK);
	}

	mii_mediachg(mii);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->vge_if_flags = 0;
	sc->vge_link = 0;

	VGE_UNLOCK(sc);

	return;
}

/*
 * Set media options.
 */
static int
vge_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct vge_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->vge_miibus);
	mii_mediachg(mii);

	return (0);
}

/*
 * Report current media status.
 */
static void
vge_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct vge_softc	*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->vge_miibus);

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static void
vge_miibus_statchg(dev)
	device_t		dev;
{
	struct vge_softc	*sc;
	struct mii_data		*mii;
	struct ifmedia_entry	*ife;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->vge_miibus);
	ife = mii->mii_media.ifm_cur;

	/*
	 * If the user manually selects a media mode, we need to turn
	 * on the forced MAC mode bit in the DIAGCTL register. If the
	 * user happens to choose a full duplex mode, we also need to
	 * set the 'force full duplex' bit. This applies only to
	 * 10Mbps and 100Mbps speeds. In autoselect mode, forced MAC
	 * mode is disabled, and in 1000baseT mode, full duplex is
	 * always implied, so we turn on the forced mode bit but leave
	 * the FDX bit cleared.
	 */

	switch (IFM_SUBTYPE(ife->ifm_media)) {
	case IFM_AUTO:
		CSR_CLRBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_MACFORCE);
		CSR_CLRBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_FDXFORCE);
		break;
	case IFM_1000_T:
		CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_MACFORCE);
		CSR_CLRBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_FDXFORCE);
		break;
	case IFM_100_TX:
	case IFM_10_T:
		CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_MACFORCE);
		if ((ife->ifm_media & IFM_GMASK) == IFM_FDX) {
			CSR_SETBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_FDXFORCE);
		} else {
			CSR_CLRBIT_1(sc, VGE_DIAGCTL, VGE_DIAGCTL_FDXFORCE);
		}
		break;
	default:
		device_printf(dev, "unknown media type: %x\n",
		    IFM_SUBTYPE(ife->ifm_media));
		break;
	}

	return;
}

static int
vge_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct vge_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error = 0;

	switch (command) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > VGE_JUMBO_MTU)
			error = EINVAL;
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->vge_if_flags & IFF_PROMISC)) {
				CSR_SETBIT_1(sc, VGE_RXCTL,
				    VGE_RXCTL_RX_PROMISC);
				vge_setmulti(sc);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->vge_if_flags & IFF_PROMISC) {
				CSR_CLRBIT_1(sc, VGE_RXCTL,
				    VGE_RXCTL_RX_PROMISC);
				vge_setmulti(sc);
                        } else
				vge_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				vge_stop(sc);
		}
		sc->vge_if_flags = ifp->if_flags;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		vge_setmulti(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->vge_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	case SIOCSIFCAP:
#ifdef IFCAP_POLLING
		ifp->if_capenable &= ~(IFCAP_HWCSUM | IFCAP_POLLING);
#else
		ifp->if_capenable &= ~(IFCAP_HWCSUM);
#endif
		ifp->if_capenable |=
#ifdef IFCAP_POLLING
		    ifr->ifr_reqcap & (IFCAP_HWCSUM | IFCAP_POLLING);
#else
		    ifr->ifr_reqcap & (IFCAP_HWCSUM);
#endif
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist = VGE_CSUM_FEATURES;
		else
			ifp->if_hwassist = 0;
		if (ifp->if_flags & IFF_RUNNING)
			vge_init(sc);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static void
vge_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct vge_softc		*sc;

	sc = ifp->if_softc;
	VGE_LOCK(sc);
	printf("vge%d: watchdog timeout\n", sc->vge_unit);
	ifp->if_oerrors++;

	vge_txeof(sc);
	vge_rxeof(sc);

	vge_init(sc);

	VGE_UNLOCK(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
vge_stop(sc)
	struct vge_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	VGE_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif /* DEVICE_POLLING */

	CSR_WRITE_1(sc, VGE_CRC3, VGE_CR3_INT_GMSK);
	CSR_WRITE_1(sc, VGE_CRS0, VGE_CR0_STOP);
	CSR_WRITE_4(sc, VGE_ISR, 0xFFFFFFFF);
	CSR_WRITE_2(sc, VGE_TXQCSRC, 0xFFFF);
	CSR_WRITE_1(sc, VGE_RXQCSRC, 0xFF);
	CSR_WRITE_4(sc, VGE_RXDESC_ADDR_LO, 0);

	if (sc->vge_head != NULL) {
		m_freem(sc->vge_head);
		sc->vge_head = sc->vge_tail = NULL;
	}

	/* Free the TX list buffers. */

	for (i = 0; i < VGE_TX_DESC_CNT; i++) {
		if (sc->vge_ldata.vge_tx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->vge_ldata.vge_mtag,
			    sc->vge_ldata.vge_tx_dmamap[i]);
			m_freem(sc->vge_ldata.vge_tx_mbuf[i]);
			sc->vge_ldata.vge_tx_mbuf[i] = NULL;
		}
	}

	/* Free the RX list buffers. */

	for (i = 0; i < VGE_RX_DESC_CNT; i++) {
		if (sc->vge_ldata.vge_rx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->vge_ldata.vge_mtag,
			    sc->vge_ldata.vge_rx_dmamap[i]);
			m_freem(sc->vge_ldata.vge_rx_mbuf[i]);
			sc->vge_ldata.vge_rx_mbuf[i] = NULL;
		}
	}

	VGE_UNLOCK(sc);

	return;
}

/*
 * Device suspend routine.  Stop the interface and save some PCI
 * settings in case the BIOS doesn't restore them properly on
 * resume.
 */
static int
vge_suspend(dev)
	device_t		dev;
{
	struct vge_softc	*sc;
	int			i;

	sc = device_get_softc(dev);

	vge_stop(sc);

	sc->suspended = 1;

	return (0);
}

/*
 * Device resume routine.  Restore some PCI settings in case the BIOS
 * doesn't, re-enable busmastering, and restart the interface if
 * appropriate.
 */
static int
vge_resume(dev)
	device_t		dev;
{
	struct vge_softc	*sc;
	struct ifnet		*ifp;
	int			i;

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	/* reenable busmastering */
	pci_enable_busmaster(dev);
	pci_enable_io(dev, SYS_RES_MEMORY);

	/* reinitialize interface if necessary */
	if (ifp->if_flags & IFF_UP)
		vge_init(sc);

	sc->suspended = 0;

	return (0);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
vge_shutdown(dev)
	device_t		dev;
{
	struct vge_softc		*sc;

	sc = device_get_softc(dev);

	vge_stop(sc);
}
