/*-
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 1997, 1998, 1999, 2001
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
 * Broadcom BCM570x family gigabit ethernet driver for FreeBSD.
 *
 * The Broadcom BCM5700 is based on technology originally developed by
 * Alteon Networks as part of the Tigon I and Tigon II gigabit ethernet
 * MAC chips. The BCM5700, sometimes refered to as the Tigon III, has
 * two on-board MIPS R4000 CPUs and can have as much as 16MB of external
 * SSRAM. The BCM5700 supports TCP, UDP and IP checksum offload, jumbo
 * frames, highly configurable RX filtering, and 16 RX and TX queues
 * (which, along with RX filter rules, can be used for QOS applications).
 * Other features, such as TCP segmentation, may be available as part
 * of value-added firmware updates. Unlike the Tigon I and Tigon II,
 * firmware images can be stored in hardware and need not be compiled
 * into the driver.
 *
 * The BCM5700 supports the PCI v2.2 and PCI-X v1.0 standards, and will
 * function in a 32-bit/64-bit 33/66Mhz bus, or a 64-bit/133Mhz bus.
 *
 * The BCM5701 is a single-chip solution incorporating both the BCM5700
 * MAC and a BCM5401 10/100/1000 PHY. Unlike the BCM5700, the BCM5701
 * does not support external SSRAM.
 *
 * Broadcom also produces a variation of the BCM5700 under the "Altima"
 * brand name, which is functionally similar but lacks PCI-X support.
 *
 * Without external SSRAM, you can only have at most 4 TX rings,
 * and the use of the mini RX ring is disabled. This seems to imply
 * that these features are simply not available on the BCM5701. As a
 * result, this driver does not implement any support for the mini RX
 * ring.
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"
#include <dev/mii/brgphyreg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/bge/if_bgereg.h>

#include "opt_bge.h"

#define BGE_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP)

MODULE_DEPEND(bge, pci, 1, 1, 1);
MODULE_DEPEND(bge, ether, 1, 1, 1);
MODULE_DEPEND(bge, miibus, 1, 1, 1);

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

/*
 * Various supported device vendors/types and their names. Note: the
 * spec seems to indicate that the hardware still has Alteon's vendor
 * ID burned into it, though it will always be overriden by the vendor
 * ID in the EEPROM. Just to be safe, we cover all possibilities.
 */
#define BGE_DEVDESC_MAX		64	/* Maximum device description length */

static struct bge_type bge_devs[] = {
	{ ALT_VENDORID,	ALT_DEVICEID_BCM5700,
		"Broadcom BCM5700 Gigabit Ethernet" },
	{ ALT_VENDORID,	ALT_DEVICEID_BCM5701,
		"Broadcom BCM5701 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5700,
		"Broadcom BCM5700 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5701,
		"Broadcom BCM5701 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5702,
		"Broadcom BCM5702 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5702X,
		"Broadcom BCM5702X Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5703,
		"Broadcom BCM5703 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5703X,
		"Broadcom BCM5703X Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5704C,
		"Broadcom BCM5704C Dual Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5704S,
		"Broadcom BCM5704S Dual Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5705,
		"Broadcom BCM5705 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5705K,
		"Broadcom BCM5705K Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5705M,
		"Broadcom BCM5705M Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5705M_ALT,
		"Broadcom BCM5705M Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5714C,
		"Broadcom BCM5714C Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5721,
		"Broadcom BCM5721 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5750,
		"Broadcom BCM5750 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5750M,
		"Broadcom BCM5750M Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5751,
		"Broadcom BCM5751 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5751M,
		"Broadcom BCM5751M Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5752,
		"Broadcom BCM5752 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5782,
		"Broadcom BCM5782 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5788,
		"Broadcom BCM5788 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5789,
		"Broadcom BCM5789 Gigabit Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5901,
		"Broadcom BCM5901 Fast Ethernet" },
	{ BCOM_VENDORID, BCOM_DEVICEID_BCM5901A2,
		"Broadcom BCM5901A2 Fast Ethernet" },
	{ SK_VENDORID, SK_DEVICEID_ALTIMA,
		"SysKonnect Gigabit Ethernet" },
	{ ALTIMA_VENDORID, ALTIMA_DEVICE_AC1000,
		"Altima AC1000 Gigabit Ethernet" },
	{ ALTIMA_VENDORID, ALTIMA_DEVICE_AC1002,
		"Altima AC1002 Gigabit Ethernet" },
	{ ALTIMA_VENDORID, ALTIMA_DEVICE_AC9100,
		"Altima AC9100 Gigabit Ethernet" },
	{ 0, 0, NULL }
};

static int bge_probe		(device_t);
static int bge_attach		(device_t);
static int bge_detach		(device_t);
static void bge_release_resources
				(struct bge_softc *);
static void bge_dma_map_addr	(void *, bus_dma_segment_t *, int, int);
static void bge_dma_map_tx_desc	(void *, bus_dma_segment_t *, int,
				    bus_size_t, int);
static int bge_dma_alloc	(device_t);
static void bge_dma_free	(struct bge_softc *);

static void bge_txeof		(struct bge_softc *);
static void bge_rxeof		(struct bge_softc *);

static void bge_tick_locked	(struct bge_softc *);
static void bge_tick		(void *);
static void bge_stats_update	(struct bge_softc *);
static void bge_stats_update_regs
				(struct bge_softc *);
static int bge_encap		(struct bge_softc *, struct mbuf *,
					u_int32_t *);

static void bge_intr		(void *);
static void bge_start_locked	(struct ifnet *);
static void bge_start		(struct ifnet *);
static int bge_ioctl		(struct ifnet *, u_long, caddr_t);
static void bge_init_locked	(struct bge_softc *);
static void bge_init		(void *);
static void bge_stop		(struct bge_softc *);
static void bge_watchdog		(struct ifnet *);
static void bge_shutdown		(device_t);
static int bge_ifmedia_upd	(struct ifnet *);
static void bge_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

static u_int8_t	bge_eeprom_getbyte	(struct bge_softc *, int, u_int8_t *);
static int bge_read_eeprom	(struct bge_softc *, caddr_t, int, int);

static void bge_setmulti	(struct bge_softc *);

static void bge_handle_events	(struct bge_softc *);
static int bge_alloc_jumbo_mem	(struct bge_softc *);
static void bge_free_jumbo_mem	(struct bge_softc *);
static void *bge_jalloc		(struct bge_softc *);
static void bge_jfree		(void *, void *);
static int bge_newbuf_std	(struct bge_softc *, int, struct mbuf *);
static int bge_newbuf_jumbo	(struct bge_softc *, int, struct mbuf *);
static int bge_init_rx_ring_std	(struct bge_softc *);
static void bge_free_rx_ring_std	(struct bge_softc *);
static int bge_init_rx_ring_jumbo	(struct bge_softc *);
static void bge_free_rx_ring_jumbo	(struct bge_softc *);
static void bge_free_tx_ring	(struct bge_softc *);
static int bge_init_tx_ring	(struct bge_softc *);

static int bge_chipinit		(struct bge_softc *);
static int bge_blockinit	(struct bge_softc *);

#ifdef notdef
static u_int8_t bge_vpd_readbyte(struct bge_softc *, int);
static void bge_vpd_read_res	(struct bge_softc *, struct vpd_res *, int);
static void bge_vpd_read	(struct bge_softc *);
#endif

static u_int32_t bge_readmem_ind
				(struct bge_softc *, int);
static void bge_writemem_ind	(struct bge_softc *, int, int);
#ifdef notdef
static u_int32_t bge_readreg_ind
				(struct bge_softc *, int);
#endif
static void bge_writereg_ind	(struct bge_softc *, int, int);

static int bge_miibus_readreg	(device_t, int, int);
static int bge_miibus_writereg	(device_t, int, int, int);
static void bge_miibus_statchg	(device_t);

static void bge_reset		(struct bge_softc *);

static device_method_t bge_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bge_probe),
	DEVMETHOD(device_attach,	bge_attach),
	DEVMETHOD(device_detach,	bge_detach),
	DEVMETHOD(device_shutdown,	bge_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	bge_miibus_readreg),
	DEVMETHOD(miibus_writereg,	bge_miibus_writereg),
	DEVMETHOD(miibus_statchg,	bge_miibus_statchg),

	{ 0, 0 }
};

static driver_t bge_driver = {
	"bge",
	bge_methods,
	sizeof(struct bge_softc)
};

static devclass_t bge_devclass;

DRIVER_MODULE(bge, pci, bge_driver, bge_devclass, 0, 0);
DRIVER_MODULE(miibus, bge, miibus_driver, miibus_devclass, 0, 0);

static u_int32_t
bge_readmem_ind(sc, off)
	struct bge_softc *sc;
	int off;
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_MEMWIN_BASEADDR, off, 4);
	return(pci_read_config(dev, BGE_PCI_MEMWIN_DATA, 4));
}

static void
bge_writemem_ind(sc, off, val)
	struct bge_softc *sc;
	int off, val;
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_MEMWIN_BASEADDR, off, 4);
	pci_write_config(dev, BGE_PCI_MEMWIN_DATA, val, 4);

	return;
}

#ifdef notdef
static u_int32_t
bge_readreg_ind(sc, off)
	struct bge_softc *sc;
	int off;
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_REG_BASEADDR, off, 4);
	return(pci_read_config(dev, BGE_PCI_REG_DATA, 4));
}
#endif

static void
bge_writereg_ind(sc, off, val)
	struct bge_softc *sc;
	int off, val;
{
	device_t dev;

	dev = sc->bge_dev;

	pci_write_config(dev, BGE_PCI_REG_BASEADDR, off, 4);
	pci_write_config(dev, BGE_PCI_REG_DATA, val, 4);

	return;
}

/*
 * Map a single buffer address.
 */

static void
bge_dma_map_addr(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg;
	int error;
{
	struct bge_dmamap_arg *ctx;

	if (error)
		return;

	ctx = arg;

	if (nseg > ctx->bge_maxsegs) {
		ctx->bge_maxsegs = 0;
		return;
	}

	ctx->bge_busaddr = segs->ds_addr;

	return;
}

/*
 * Map an mbuf chain into an TX ring.
 */

static void
bge_dma_map_tx_desc(arg, segs, nseg, mapsize, error)
	void *arg;
	bus_dma_segment_t *segs;
	int nseg;
	bus_size_t mapsize;
	int error;
{
	struct bge_dmamap_arg *ctx;
	struct bge_tx_bd *d = NULL;
	int i = 0, idx;

	if (error)
		return;

	ctx = arg;

	/* Signal error to caller if there's too many segments */
	if (nseg > ctx->bge_maxsegs) {
		ctx->bge_maxsegs = 0;
		return;
	}

	idx = ctx->bge_idx;
	while(1) {
		d = &ctx->bge_ring[idx];
		d->bge_addr.bge_addr_lo =
		    htole32(BGE_ADDR_LO(segs[i].ds_addr));
		d->bge_addr.bge_addr_hi =
		    htole32(BGE_ADDR_HI(segs[i].ds_addr));
		d->bge_len = htole16(segs[i].ds_len);
		d->bge_flags = htole16(ctx->bge_flags);
		i++;
		if (i == nseg)
			break;
		BGE_INC(idx, BGE_TX_RING_CNT);
	}

	d->bge_flags |= htole16(BGE_TXBDFLAG_END);
	ctx->bge_maxsegs = nseg;
	ctx->bge_idx = idx;

	return;
}


#ifdef notdef
static u_int8_t
bge_vpd_readbyte(sc, addr)
	struct bge_softc *sc;
	int addr;
{
	int i;
	device_t dev;
	u_int32_t val;

	dev = sc->bge_dev;
	pci_write_config(dev, BGE_PCI_VPD_ADDR, addr, 2);
	for (i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (pci_read_config(dev, BGE_PCI_VPD_ADDR, 2) & BGE_VPD_FLAG)
			break;
	}

	if (i == BGE_TIMEOUT) {
		printf("bge%d: VPD read timed out\n", sc->bge_unit);
		return(0);
	}

	val = pci_read_config(dev, BGE_PCI_VPD_DATA, 4);

	return((val >> ((addr % 4) * 8)) & 0xFF);
}

static void
bge_vpd_read_res(sc, res, addr)
	struct bge_softc *sc;
	struct vpd_res *res;
	int addr;
{
	int i;
	u_int8_t *ptr;

	ptr = (u_int8_t *)res;
	for (i = 0; i < sizeof(struct vpd_res); i++)
		ptr[i] = bge_vpd_readbyte(sc, i + addr);

	return;
}

static void
bge_vpd_read(sc)
	struct bge_softc *sc;
{
	int pos = 0, i;
	struct vpd_res res;

	if (sc->bge_vpd_prodname != NULL)
		free(sc->bge_vpd_prodname, M_DEVBUF);
	if (sc->bge_vpd_readonly != NULL)
		free(sc->bge_vpd_readonly, M_DEVBUF);
	sc->bge_vpd_prodname = NULL;
	sc->bge_vpd_readonly = NULL;

	bge_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_ID) {
		printf("bge%d: bad VPD resource id: expected %x got %x\n",
			sc->bge_unit, VPD_RES_ID, res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->bge_vpd_prodname = malloc(res.vr_len + 1, M_DEVBUF, M_NOWAIT);
	for (i = 0; i < res.vr_len; i++)
		sc->bge_vpd_prodname[i] = bge_vpd_readbyte(sc, i + pos);
	sc->bge_vpd_prodname[i] = '\0';
	pos += i;

	bge_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_READ) {
		printf("bge%d: bad VPD resource id: expected %x got %x\n",
		    sc->bge_unit, VPD_RES_READ, res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->bge_vpd_readonly = malloc(res.vr_len, M_DEVBUF, M_NOWAIT);
	for (i = 0; i < res.vr_len + 1; i++)
		sc->bge_vpd_readonly[i] = bge_vpd_readbyte(sc, i + pos);

	return;
}
#endif

/*
 * Read a byte of data stored in the EEPROM at address 'addr.' The
 * BCM570x supports both the traditional bitbang interface and an
 * auto access interface for reading the EEPROM. We use the auto
 * access method.
 */
static u_int8_t
bge_eeprom_getbyte(sc, addr, dest)
	struct bge_softc *sc;
	int addr;
	u_int8_t *dest;
{
	int i;
	u_int32_t byte = 0;

	/*
	 * Enable use of auto EEPROM access so we can avoid
	 * having to use the bitbang method.
	 */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_AUTO_EEPROM);

	/* Reset the EEPROM, load the clock period. */
	CSR_WRITE_4(sc, BGE_EE_ADDR,
	    BGE_EEADDR_RESET|BGE_EEHALFCLK(BGE_HALFCLK_384SCL));
	DELAY(20);

	/* Issue the read EEPROM command. */
	CSR_WRITE_4(sc, BGE_EE_ADDR, BGE_EE_READCMD | addr);

	/* Wait for completion */
	for(i = 0; i < BGE_TIMEOUT * 10; i++) {
		DELAY(10);
		if (CSR_READ_4(sc, BGE_EE_ADDR) & BGE_EEADDR_DONE)
			break;
	}

	if (i == BGE_TIMEOUT) {
		printf("bge%d: eeprom read timed out\n", sc->bge_unit);
		return(0);
	}

	/* Get result. */
	byte = CSR_READ_4(sc, BGE_EE_DATA);

	*dest = (byte >> ((addr % 4) * 8)) & 0xFF;

	return(0);
}

/*
 * Read a sequence of bytes from the EEPROM.
 */
static int
bge_read_eeprom(sc, dest, off, cnt)
	struct bge_softc *sc;
	caddr_t dest;
	int off;
	int cnt;
{
	int err = 0, i;
	u_int8_t byte = 0;

	for (i = 0; i < cnt; i++) {
		err = bge_eeprom_getbyte(sc, off + i, &byte);
		if (err)
			break;
		*(dest + i) = byte;
	}

	return(err ? 1 : 0);
}

static int
bge_miibus_readreg(dev, phy, reg)
	device_t dev;
	int phy, reg;
{
	struct bge_softc *sc;
	u_int32_t val, autopoll;
	int i;

	sc = device_get_softc(dev);

	/*
	 * Broadcom's own driver always assumes the internal
	 * PHY is at GMII address 1. On some chips, the PHY responds
	 * to accesses at all addresses, which could cause us to
	 * bogusly attach the PHY 32 times at probe type. Always
	 * restricting the lookup to address 1 is simpler than
	 * trying to figure out which chips revisions should be
	 * special-cased.
	 */
	if (phy != 1)
		return(0);

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_CLRBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	CSR_WRITE_4(sc, BGE_MI_COMM, BGE_MICMD_READ|BGE_MICOMM_BUSY|
	    BGE_MIPHY(phy)|BGE_MIREG(reg));

	for (i = 0; i < BGE_TIMEOUT; i++) {
		val = CSR_READ_4(sc, BGE_MI_COMM);
		if (!(val & BGE_MICOMM_BUSY))
			break;
	}

	if (i == BGE_TIMEOUT) {
		printf("bge%d: PHY read timed out\n", sc->bge_unit);
		val = 0;
		goto done;
	}

	val = CSR_READ_4(sc, BGE_MI_COMM);

done:
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	if (val & BGE_MICOMM_READFAIL)
		return(0);

	return(val & 0xFFFF);
}

static int
bge_miibus_writereg(dev, phy, reg, val)
	device_t dev;
	int phy, reg, val;
{
	struct bge_softc *sc;
	u_int32_t autopoll;
	int i;

	sc = device_get_softc(dev);

	/* Reading with autopolling on may trigger PCI errors */
	autopoll = CSR_READ_4(sc, BGE_MI_MODE);
	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_CLRBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	CSR_WRITE_4(sc, BGE_MI_COMM, BGE_MICMD_WRITE|BGE_MICOMM_BUSY|
	    BGE_MIPHY(phy)|BGE_MIREG(reg)|val);

	for (i = 0; i < BGE_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, BGE_MI_COMM) & BGE_MICOMM_BUSY))
			break;
	}

	if (autopoll & BGE_MIMODE_AUTOPOLL) {
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL);
		DELAY(40);
	}

	if (i == BGE_TIMEOUT) {
		printf("bge%d: PHY read timed out\n", sc->bge_unit);
		return(0);
	}

	return(0);
}

static void
bge_miibus_statchg(dev)
	device_t dev;
{
	struct bge_softc *sc;
	struct mii_data *mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->bge_miibus);

	BGE_CLRBIT(sc, BGE_MAC_MODE, BGE_MACMODE_PORTMODE);
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T) {
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_PORTMODE_GMII);
	} else {
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_PORTMODE_MII);
	}

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		BGE_CLRBIT(sc, BGE_MAC_MODE, BGE_MACMODE_HALF_DUPLEX);
	} else {
		BGE_SETBIT(sc, BGE_MAC_MODE, BGE_MACMODE_HALF_DUPLEX);
	}

	return;
}

/*
 * Handle events that have triggered interrupts.
 */
static void
bge_handle_events(sc)
	struct bge_softc		*sc;
{

	return;
}

/*
 * Memory management for jumbo frames.
 */

static int
bge_alloc_jumbo_mem(sc)
	struct bge_softc		*sc;
{
	caddr_t			ptr;
	register int		i, error;
	struct bge_jpool_entry   *entry;

	/* Create tag for jumbo buffer block */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_JMEM, 1, BGE_JMEM, 0, NULL, NULL,
	    &sc->bge_cdata.bge_jumbo_tag);

	if (error) {
		printf("bge%d: could not allocate jumbo dma tag\n",
		    sc->bge_unit);
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for jumbo buffer block */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_jumbo_tag,
	    (void **)&sc->bge_ldata.bge_jumbo_buf, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_jumbo_map);

	if (error)
		return (ENOMEM);

	SLIST_INIT(&sc->bge_jfree_listhead);
	SLIST_INIT(&sc->bge_jinuse_listhead);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array.
	 */
	ptr = sc->bge_ldata.bge_jumbo_buf;
	for (i = 0; i < BGE_JSLOTS; i++) {
		sc->bge_cdata.bge_jslots[i] = ptr;
		ptr += BGE_JLEN;
		entry = malloc(sizeof(struct bge_jpool_entry),
		    M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			bge_free_jumbo_mem(sc);
			sc->bge_ldata.bge_jumbo_buf = NULL;
			printf("bge%d: no memory for jumbo "
			    "buffer queue!\n", sc->bge_unit);
			return(ENOBUFS);
		}
		entry->slot = i;
		SLIST_INSERT_HEAD(&sc->bge_jfree_listhead,
		    entry, jpool_entries);
	}

	return(0);
}

static void
bge_free_jumbo_mem(sc)
	struct bge_softc *sc;
{
	int i;
	struct bge_jpool_entry *entry;

	for (i = 0; i < BGE_JSLOTS; i++) {
		entry = SLIST_FIRST(&sc->bge_jfree_listhead);
		SLIST_REMOVE_HEAD(&sc->bge_jfree_listhead, jpool_entries);
		free(entry, M_DEVBUF);
	}

	/* Destroy jumbo buffer block */

	if (sc->bge_ldata.bge_rx_jumbo_ring)
		bus_dmamem_free(sc->bge_cdata.bge_jumbo_tag,
		    sc->bge_ldata.bge_jumbo_buf,
		    sc->bge_cdata.bge_jumbo_map);

	if (sc->bge_cdata.bge_rx_jumbo_ring_map)
		bus_dmamap_destroy(sc->bge_cdata.bge_jumbo_tag,
		    sc->bge_cdata.bge_jumbo_map);

	if (sc->bge_cdata.bge_jumbo_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_jumbo_tag);

	return;
}

/*
 * Allocate a jumbo buffer.
 */
static void *
bge_jalloc(sc)
	struct bge_softc		*sc;
{
	struct bge_jpool_entry   *entry;

	entry = SLIST_FIRST(&sc->bge_jfree_listhead);

	if (entry == NULL) {
		printf("bge%d: no free jumbo buffers\n", sc->bge_unit);
		return(NULL);
	}

	SLIST_REMOVE_HEAD(&sc->bge_jfree_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->bge_jinuse_listhead, entry, jpool_entries);
	return(sc->bge_cdata.bge_jslots[entry->slot]);
}

/*
 * Release a jumbo buffer.
 */
static void
bge_jfree(buf, args)
	void *buf;
	void *args;
{
	struct bge_jpool_entry *entry;
	struct bge_softc *sc;
	int i;

	/* Extract the softc struct pointer. */
	sc = (struct bge_softc *)args;

	if (sc == NULL)
		panic("bge_jfree: can't find softc pointer!");

	/* calculate the slot this buffer belongs to */

	i = ((vm_offset_t)buf
	     - (vm_offset_t)sc->bge_ldata.bge_jumbo_buf) / BGE_JLEN;

	if ((i < 0) || (i >= BGE_JSLOTS))
		panic("bge_jfree: asked to free buffer that we don't manage!");

	entry = SLIST_FIRST(&sc->bge_jinuse_listhead);
	if (entry == NULL)
		panic("bge_jfree: buffer not in use!");
	entry->slot = i;
	SLIST_REMOVE_HEAD(&sc->bge_jinuse_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->bge_jfree_listhead, entry, jpool_entries);

	return;
}


/*
 * Intialize a standard receive ring descriptor.
 */
static int
bge_newbuf_std(sc, i, m)
	struct bge_softc	*sc;
	int			i;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	struct bge_rx_bd	*r;
	struct bge_dmamap_arg	ctx;
	int			error;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			return(ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	if (!sc->bge_rx_alignment_bug)
		m_adj(m_new, ETHER_ALIGN);
	sc->bge_cdata.bge_rx_std_chain[i] = m_new;
	r = &sc->bge_ldata.bge_rx_std_ring[i];
	ctx.bge_maxsegs = 1;
	ctx.sc = sc;
	error = bus_dmamap_load(sc->bge_cdata.bge_mtag,
	    sc->bge_cdata.bge_rx_std_dmamap[i], mtod(m_new, void *),
	    m_new->m_len, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);
	if (error || ctx.bge_maxsegs == 0) {
		if (m == NULL)
			m_freem(m_new);
		return(ENOMEM);
	}
	r->bge_addr.bge_addr_lo = htole32(BGE_ADDR_LO(ctx.bge_busaddr));
	r->bge_addr.bge_addr_hi = htole32(BGE_ADDR_HI(ctx.bge_busaddr));
	r->bge_flags = htole16(BGE_RXBDFLAG_END);
	r->bge_len = htole16(m_new->m_len);
	r->bge_idx = htole16(i);

	bus_dmamap_sync(sc->bge_cdata.bge_mtag,
	    sc->bge_cdata.bge_rx_std_dmamap[i],
	    BUS_DMASYNC_PREREAD);

	return(0);
}

/*
 * Initialize a jumbo receive ring descriptor. This allocates
 * a jumbo buffer from the pool managed internally by the driver.
 */
static int
bge_newbuf_jumbo(sc, i, m)
	struct bge_softc *sc;
	int i;
	struct mbuf *m;
{
	struct mbuf *m_new = NULL;
	struct bge_rx_bd *r;
	struct bge_dmamap_arg ctx;
	int error;

	if (m == NULL) {
		caddr_t			*buf = NULL;

		/* Allocate the mbuf. */
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			return(ENOBUFS);
		}

		/* Allocate the jumbo buffer */
		buf = bge_jalloc(sc);
		if (buf == NULL) {
			m_freem(m_new);
			printf("bge%d: jumbo allocation failed "
			    "-- packet dropped!\n", sc->bge_unit);
			return(ENOBUFS);
		}

		/* Attach the buffer to the mbuf. */
		m_new->m_data = (void *) buf;
		m_new->m_len = m_new->m_pkthdr.len = BGE_JUMBO_FRAMELEN;
		MEXTADD(m_new, buf, BGE_JUMBO_FRAMELEN, bge_jfree,
		    (struct bge_softc *)sc, 0, EXT_NET_DRV);
	} else {
		m_new = m;
		m_new->m_data = m_new->m_ext.ext_buf;
		m_new->m_ext.ext_size = BGE_JUMBO_FRAMELEN;
	}

	if (!sc->bge_rx_alignment_bug)
		m_adj(m_new, ETHER_ALIGN);
	/* Set up the descriptor. */
	sc->bge_cdata.bge_rx_jumbo_chain[i] = m_new;
	r = &sc->bge_ldata.bge_rx_jumbo_ring[i];
	ctx.bge_maxsegs = 1;
	ctx.sc = sc;
	error = bus_dmamap_load(sc->bge_cdata.bge_mtag_jumbo,
	    sc->bge_cdata.bge_rx_jumbo_dmamap[i], mtod(m_new, void *),
	    m_new->m_len, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);
	if (error || ctx.bge_maxsegs == 0) {
		if (m == NULL)
			m_freem(m_new);
		return(ENOMEM);
	}
	r->bge_addr.bge_addr_lo = htole32(BGE_ADDR_LO(ctx.bge_busaddr));
	r->bge_addr.bge_addr_hi = htole32(BGE_ADDR_HI(ctx.bge_busaddr));
	r->bge_flags = htole16(BGE_RXBDFLAG_END|BGE_RXBDFLAG_JUMBO_RING);
	r->bge_len = htole16(m_new->m_len);
	r->bge_idx = htole16(i);

	bus_dmamap_sync(sc->bge_cdata.bge_mtag,
	    sc->bge_cdata.bge_rx_jumbo_dmamap[i],
	    BUS_DMASYNC_PREREAD);

	return(0);
}

/*
 * The standard receive ring has 512 entries in it. At 2K per mbuf cluster,
 * that's 1MB or memory, which is a lot. For now, we fill only the first
 * 256 ring entries and hope that our CPU is fast enough to keep up with
 * the NIC.
 */
static int
bge_init_rx_ring_std(sc)
	struct bge_softc *sc;
{
	int i;

	for (i = 0; i < BGE_SSLOTS; i++) {
		if (bge_newbuf_std(sc, i, NULL) == ENOBUFS)
			return(ENOBUFS);
	};

	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc->bge_std = i - 1;
	CSR_WRITE_4(sc, BGE_MBX_RX_STD_PROD_LO, sc->bge_std);

	return(0);
}

static void
bge_free_rx_ring_std(sc)
	struct bge_softc *sc;
{
	int i;

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_std_chain[i] != NULL) {
			m_freem(sc->bge_cdata.bge_rx_std_chain[i]);
			sc->bge_cdata.bge_rx_std_chain[i] = NULL;
			bus_dmamap_unload(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[i]);
		}
		bzero((char *)&sc->bge_ldata.bge_rx_std_ring[i],
		    sizeof(struct bge_rx_bd));
	}

	return;
}

static int
bge_init_rx_ring_jumbo(sc)
	struct bge_softc *sc;
{
	int i;
	struct bge_rcb *rcb;

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (bge_newbuf_jumbo(sc, i, NULL) == ENOBUFS)
			return(ENOBUFS);
	};

	bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
	    sc->bge_cdata.bge_rx_jumbo_ring_map,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc->bge_jumbo = i - 1;

	rcb = &sc->bge_ldata.bge_info.bge_jumbo_rx_rcb;
	rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(0, 0);
	CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);

	CSR_WRITE_4(sc, BGE_MBX_RX_JUMBO_PROD_LO, sc->bge_jumbo);

	return(0);
}

static void
bge_free_rx_ring_jumbo(sc)
	struct bge_softc *sc;
{
	int i;

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_jumbo_chain[i] != NULL) {
			m_freem(sc->bge_cdata.bge_rx_jumbo_chain[i]);
			sc->bge_cdata.bge_rx_jumbo_chain[i] = NULL;
			bus_dmamap_unload(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
		}
		bzero((char *)&sc->bge_ldata.bge_rx_jumbo_ring[i],
		    sizeof(struct bge_rx_bd));
	}

	return;
}

static void
bge_free_tx_ring(sc)
	struct bge_softc *sc;
{
	int i;

	if (sc->bge_ldata.bge_tx_ring == NULL)
		return;

	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_tx_chain[i] != NULL) {
			m_freem(sc->bge_cdata.bge_tx_chain[i]);
			sc->bge_cdata.bge_tx_chain[i] = NULL;
			bus_dmamap_unload(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_tx_dmamap[i]);
		}
		bzero((char *)&sc->bge_ldata.bge_tx_ring[i],
		    sizeof(struct bge_tx_bd));
	}

	return;
}

static int
bge_init_tx_ring(sc)
	struct bge_softc *sc;
{
	sc->bge_txcnt = 0;
	sc->bge_tx_saved_considx = 0;

	CSR_WRITE_4(sc, BGE_MBX_TX_HOST_PROD0_LO, 0);
	/* 5700 b2 errata */
	if (sc->bge_chiprev == BGE_CHIPREV_5700_BX)
		CSR_WRITE_4(sc, BGE_MBX_TX_HOST_PROD0_LO, 0);

	CSR_WRITE_4(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);
	/* 5700 b2 errata */
	if (sc->bge_chiprev == BGE_CHIPREV_5700_BX)
		CSR_WRITE_4(sc, BGE_MBX_TX_NIC_PROD0_LO, 0);

	return(0);
}

static void
bge_setmulti(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	u_int32_t hashes[4] = { 0, 0, 0, 0 };
	int h, i;

	BGE_LOCK_ASSERT(sc);

	ifp = &sc->arpcom.ac_if;

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		for (i = 0; i < 4; i++)
			CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), 0xFFFFFFFF);
		return;
	}

	/* First, zot all the existing filters. */
	for (i = 0; i < 4; i++)
		CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), 0);

	/* Now program new ones. */
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		h = ether_crc32_le(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) & 0x7F;
		hashes[(h & 0x60) >> 5] |= 1 << (h & 0x1F);
	}

	for (i = 0; i < 4; i++)
		CSR_WRITE_4(sc, BGE_MAR0 + (i * 4), hashes[i]);

	return;
}

/*
 * Do endian, PCI and DMA initialization. Also check the on-board ROM
 * self-test results.
 */
static int
bge_chipinit(sc)
	struct bge_softc *sc;
{
	int			i;
	u_int32_t		dma_rw_ctl;

	/* Set endianness before we access any non-PCI registers. */
#if BYTE_ORDER == BIG_ENDIAN
	pci_write_config(sc->bge_dev, BGE_PCI_MISC_CTL,
	    BGE_BIGENDIAN_INIT, 4);
#else
	pci_write_config(sc->bge_dev, BGE_PCI_MISC_CTL,
	    BGE_LITTLEENDIAN_INIT, 4);
#endif

	/*
	 * Check the 'ROM failed' bit on the RX CPU to see if
	 * self-tests passed.
	 */
	if (CSR_READ_4(sc, BGE_RXCPU_MODE) & BGE_RXCPUMODE_ROMFAIL) {
		printf("bge%d: RX CPU self-diagnostics failed!\n",
		    sc->bge_unit);
		return(ENODEV);
	}

	/* Clear the MAC control register */
	CSR_WRITE_4(sc, BGE_MAC_MODE, 0);

	/*
	 * Clear the MAC statistics block in the NIC's
	 * internal memory.
	 */
	for (i = BGE_STATS_BLOCK;
	    i < BGE_STATS_BLOCK_END + 1; i += sizeof(u_int32_t))
		BGE_MEMWIN_WRITE(sc, i, 0);

	for (i = BGE_STATUS_BLOCK;
	    i < BGE_STATUS_BLOCK_END + 1; i += sizeof(u_int32_t))
		BGE_MEMWIN_WRITE(sc, i, 0);

	/* Set up the PCI DMA control register. */
	if (sc->bge_pcie) {
		dma_rw_ctl = BGE_PCI_READ_CMD|BGE_PCI_WRITE_CMD |
		    (0xf << BGE_PCIDMARWCTL_RD_WAT_SHIFT) |
		    (0x2 << BGE_PCIDMARWCTL_WR_WAT_SHIFT);
	} else if (pci_read_config(sc->bge_dev, BGE_PCI_PCISTATE, 4) &
	    BGE_PCISTATE_PCI_BUSMODE) {
		/* Conventional PCI bus */
		dma_rw_ctl = BGE_PCI_READ_CMD|BGE_PCI_WRITE_CMD |
		    (0x7 << BGE_PCIDMARWCTL_RD_WAT_SHIFT) |
		    (0x7 << BGE_PCIDMARWCTL_WR_WAT_SHIFT) |
		    (0x0F);
	} else {
		/* PCI-X bus */
		/*
		 * The 5704 uses a different encoding of read/write
		 * watermarks.
		 */
		if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
			dma_rw_ctl = BGE_PCI_READ_CMD|BGE_PCI_WRITE_CMD |
			    (0x7 << BGE_PCIDMARWCTL_RD_WAT_SHIFT) |
			    (0x3 << BGE_PCIDMARWCTL_WR_WAT_SHIFT);
		else
			dma_rw_ctl = BGE_PCI_READ_CMD|BGE_PCI_WRITE_CMD |
			    (0x3 << BGE_PCIDMARWCTL_RD_WAT_SHIFT) |
			    (0x3 << BGE_PCIDMARWCTL_WR_WAT_SHIFT) |
			    (0x0F);

		/*
		 * 5703 and 5704 need ONEDMA_AT_ONCE as a workaround
		 * for hardware bugs.
		 */
		if (sc->bge_asicrev == BGE_ASICREV_BCM5703 ||
		    sc->bge_asicrev == BGE_ASICREV_BCM5704) {
			u_int32_t tmp;

			tmp = CSR_READ_4(sc, BGE_PCI_CLKCTL) & 0x1f;
			if (tmp == 0x6 || tmp == 0x7)
				dma_rw_ctl |= BGE_PCIDMARWCTL_ONEDMA_ATONCE;
		}
	}

	if (sc->bge_asicrev == BGE_ASICREV_BCM5703 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5704 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750)
		dma_rw_ctl &= ~BGE_PCIDMARWCTL_MINDMA;
	pci_write_config(sc->bge_dev, BGE_PCI_DMA_RW_CTL, dma_rw_ctl, 4);

	/*
	 * Set up general mode register.
	 */
	CSR_WRITE_4(sc, BGE_MODE_CTL, BGE_MODECTL_WORDSWAP_NONFRAME|
	    BGE_MODECTL_BYTESWAP_DATA|BGE_MODECTL_WORDSWAP_DATA|
	    BGE_MODECTL_MAC_ATTN_INTR|BGE_MODECTL_HOST_SEND_BDS|
	    BGE_MODECTL_TX_NO_PHDR_CSUM|BGE_MODECTL_RX_NO_PHDR_CSUM);

	/*
	 * Disable memory write invalidate.  Apparently it is not supported
	 * properly by these devices.
	 */
	PCI_CLRBIT(sc->bge_dev, BGE_PCI_CMD, PCIM_CMD_MWIEN, 4);

#ifdef __brokenalpha__
	/*
	 * Must insure that we do not cross an 8K (bytes) boundary
	 * for DMA reads.  Our highest limit is 1K bytes.  This is a
	 * restriction on some ALPHA platforms with early revision
	 * 21174 PCI chipsets, such as the AlphaPC 164lx
	 */
	PCI_SETBIT(sc->bge_dev, BGE_PCI_DMA_RW_CTL,
	    BGE_PCI_READ_BNDRY_1024BYTES, 4);
#endif

	/* Set the timer prescaler (always 66Mhz) */
	CSR_WRITE_4(sc, BGE_MISC_CFG, 65 << 1/*BGE_32BITTIME_66MHZ*/);

	return(0);
}

static int
bge_blockinit(sc)
	struct bge_softc *sc;
{
	struct bge_rcb *rcb;
	volatile struct bge_rcb *vrcb;
	int i;

	/*
	 * Initialize the memory window pointer register so that
	 * we can access the first 32K of internal NIC RAM. This will
	 * allow us to set up the TX send ring RCBs and the RX return
	 * ring RCBs, plus other things which live in NIC memory.
	 */
	CSR_WRITE_4(sc, BGE_PCI_MEMWIN_BASEADDR, 0);

	/* Note: the BCM5704 has a smaller mbuf space than other chips. */

	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		/* Configure mbuf memory pool */
		if (sc->bge_extram) {
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_BASEADDR,
			    BGE_EXT_SSRAM);
			if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
				CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x10000);
			else
				CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x18000);
		} else {
			CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_BASEADDR,
			    BGE_BUFFPOOL_1);
			if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
				CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x10000);
			else
				CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_LEN, 0x18000);
		}

		/* Configure DMA resource pool */
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_BASEADDR,
		    BGE_DMA_DESCRIPTORS);
		CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LEN, 0x2000);
	}

	/* Configure mbuf pool watermarks */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750) {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x0);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x10);
	} else {
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x50);
		CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x20);
	}
	CSR_WRITE_4(sc, BGE_BMAN_MBUFPOOL_HIWAT, 0x60);

	/* Configure DMA resource watermarks */
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_LOWAT, 5);
	CSR_WRITE_4(sc, BGE_BMAN_DMA_DESCPOOL_HIWAT, 10);

	/* Enable buffer manager */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		CSR_WRITE_4(sc, BGE_BMAN_MODE,
		    BGE_BMANMODE_ENABLE|BGE_BMANMODE_LOMBUF_ATTN);

		/* Poll for buffer manager start indication */
		for (i = 0; i < BGE_TIMEOUT; i++) {
			if (CSR_READ_4(sc, BGE_BMAN_MODE) & BGE_BMANMODE_ENABLE)
				break;
			DELAY(10);
		}

		if (i == BGE_TIMEOUT) {
			printf("bge%d: buffer manager failed to start\n",
			    sc->bge_unit);
			return(ENXIO);
		}
	}

	/* Enable flow-through queues */
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);

	/* Wait until queue initialization is complete */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		if (CSR_READ_4(sc, BGE_FTQ_RESET) == 0)
			break;
		DELAY(10);
	}

	if (i == BGE_TIMEOUT) {
		printf("bge%d: flow-through queue init failed\n",
		    sc->bge_unit);
		return(ENXIO);
	}

	/* Initialize the standard RX ring control block */
	rcb = &sc->bge_ldata.bge_info.bge_std_rx_rcb;
	rcb->bge_hostaddr.bge_addr_lo =
	    BGE_ADDR_LO(sc->bge_ldata.bge_rx_std_ring_paddr);
	rcb->bge_hostaddr.bge_addr_hi =
	    BGE_ADDR_HI(sc->bge_ldata.bge_rx_std_ring_paddr);
	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, BUS_DMASYNC_PREREAD);
	if (sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750)
		rcb->bge_maxlen_flags = BGE_RCB_MAXLEN_FLAGS(512, 0);
	else
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(BGE_MAX_FRAMELEN, 0);
	if (sc->bge_extram)
		rcb->bge_nicaddr = BGE_EXT_STD_RX_RINGS;
	else
		rcb->bge_nicaddr = BGE_STD_RX_RINGS;
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_HI, rcb->bge_hostaddr.bge_addr_hi);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_HADDR_LO, rcb->bge_hostaddr.bge_addr_lo);

	CSR_WRITE_4(sc, BGE_RX_STD_RCB_MAXLEN_FLAGS, rcb->bge_maxlen_flags);
	CSR_WRITE_4(sc, BGE_RX_STD_RCB_NICADDR, rcb->bge_nicaddr);

	/*
	 * Initialize the jumbo RX ring control block
	 * We set the 'ring disabled' bit in the flags
	 * field until we're actually ready to start
	 * using this ring (i.e. once we set the MTU
	 * high enough to require it).
	 */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		rcb = &sc->bge_ldata.bge_info.bge_jumbo_rx_rcb;

		rcb->bge_hostaddr.bge_addr_lo =
		    BGE_ADDR_LO(sc->bge_ldata.bge_rx_jumbo_ring_paddr);
		rcb->bge_hostaddr.bge_addr_hi =
		    BGE_ADDR_HI(sc->bge_ldata.bge_rx_jumbo_ring_paddr);
		bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map,
		    BUS_DMASYNC_PREREAD);
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(BGE_MAX_FRAMELEN,
		    BGE_RCB_FLAG_RING_DISABLED);
		if (sc->bge_extram)
			rcb->bge_nicaddr = BGE_EXT_JUMBO_RX_RINGS;
		else
			rcb->bge_nicaddr = BGE_JUMBO_RX_RINGS;
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_HI,
		    rcb->bge_hostaddr.bge_addr_hi);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_HADDR_LO,
		    rcb->bge_hostaddr.bge_addr_lo);

		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);
		CSR_WRITE_4(sc, BGE_RX_JUMBO_RCB_NICADDR, rcb->bge_nicaddr);

		/* Set up dummy disabled mini ring RCB */
		rcb = &sc->bge_ldata.bge_info.bge_mini_rx_rcb;
		rcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED);
		CSR_WRITE_4(sc, BGE_RX_MINI_RCB_MAXLEN_FLAGS,
		    rcb->bge_maxlen_flags);
	}

	/*
	 * Set the BD ring replentish thresholds. The recommended
	 * values are 1/8th the number of descriptors allocated to
	 * each ring.
	 */
	CSR_WRITE_4(sc, BGE_RBDI_STD_REPL_THRESH, BGE_STD_RX_RING_CNT/8);
	CSR_WRITE_4(sc, BGE_RBDI_JUMBO_REPL_THRESH, BGE_JUMBO_RX_RING_CNT/8);

	/*
	 * Disable all unused send rings by setting the 'ring disabled'
	 * bit in the flags field of all the TX send ring control blocks.
	 * These are located in NIC memory.
	 */
	vrcb = (volatile struct bge_rcb *)(sc->bge_vhandle + BGE_MEMWIN_START +
	    BGE_SEND_RING_RCB);
	for (i = 0; i < BGE_TX_RINGS_EXTSSRAM_MAX; i++) {
		vrcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED);
		vrcb->bge_nicaddr = 0;
		vrcb++;
	}

	/* Configure TX RCB 0 (we use only the first ring) */
	vrcb = (volatile struct bge_rcb *)(sc->bge_vhandle + BGE_MEMWIN_START +
	    BGE_SEND_RING_RCB);
	vrcb->bge_hostaddr.bge_addr_lo =
	    htole32(BGE_ADDR_LO(sc->bge_ldata.bge_tx_ring_paddr));
	vrcb->bge_hostaddr.bge_addr_hi =
	    htole32(BGE_ADDR_HI(sc->bge_ldata.bge_tx_ring_paddr));
	vrcb->bge_nicaddr = BGE_NIC_TXRING_ADDR(0, BGE_TX_RING_CNT);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		vrcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(BGE_TX_RING_CNT, 0);

	/* Disable all unused RX return rings */
	vrcb = (volatile struct bge_rcb *)(sc->bge_vhandle + BGE_MEMWIN_START +
	    BGE_RX_RETURN_RING_RCB);
	for (i = 0; i < BGE_RX_RINGS_MAX; i++) {
		vrcb->bge_hostaddr.bge_addr_hi = 0;
		vrcb->bge_hostaddr.bge_addr_lo = 0;
		vrcb->bge_maxlen_flags =
		    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt,
		    BGE_RCB_FLAG_RING_DISABLED);
		vrcb->bge_nicaddr = 0;
		CSR_WRITE_4(sc, BGE_MBX_RX_CONS0_LO +
		    (i * (sizeof(u_int64_t))), 0);
		vrcb++;
	}

	/* Initialize RX ring indexes */
	CSR_WRITE_4(sc, BGE_MBX_RX_STD_PROD_LO, 0);
	CSR_WRITE_4(sc, BGE_MBX_RX_JUMBO_PROD_LO, 0);
	CSR_WRITE_4(sc, BGE_MBX_RX_MINI_PROD_LO, 0);

	/*
	 * Set up RX return ring 0
	 * Note that the NIC address for RX return rings is 0x00000000.
	 * The return rings live entirely within the host, so the
	 * nicaddr field in the RCB isn't used.
	 */
	vrcb = (volatile struct bge_rcb *)(sc->bge_vhandle + BGE_MEMWIN_START +
	    BGE_RX_RETURN_RING_RCB);
	vrcb->bge_hostaddr.bge_addr_lo =
	    BGE_ADDR_LO(sc->bge_ldata.bge_rx_return_ring_paddr);
	vrcb->bge_hostaddr.bge_addr_hi =
	    BGE_ADDR_HI(sc->bge_ldata.bge_rx_return_ring_paddr);
	bus_dmamap_sync(sc->bge_cdata.bge_rx_return_ring_tag,
	    sc->bge_cdata.bge_rx_return_ring_map, BUS_DMASYNC_PREWRITE);
	vrcb->bge_nicaddr = 0x00000000;
	vrcb->bge_maxlen_flags =
	    BGE_RCB_MAXLEN_FLAGS(sc->bge_return_ring_cnt, 0);

	/* Set random backoff seed for TX */
	CSR_WRITE_4(sc, BGE_TX_RANDOM_BACKOFF,
	    sc->arpcom.ac_enaddr[0] + sc->arpcom.ac_enaddr[1] +
	    sc->arpcom.ac_enaddr[2] + sc->arpcom.ac_enaddr[3] +
	    sc->arpcom.ac_enaddr[4] + sc->arpcom.ac_enaddr[5] +
	    BGE_TX_BACKOFF_SEED_MASK);

	/* Set inter-packet gap */
	CSR_WRITE_4(sc, BGE_TX_LENGTHS, 0x2620);

	/*
	 * Specify which ring to use for packets that don't match
	 * any RX rules.
	 */
	CSR_WRITE_4(sc, BGE_RX_RULES_CFG, 0x08);

	/*
	 * Configure number of RX lists. One interrupt distribution
	 * list, sixteen active lists, one bad frames class.
	 */
	CSR_WRITE_4(sc, BGE_RXLP_CFG, 0x181);

	/* Inialize RX list placement stats mask. */
	CSR_WRITE_4(sc, BGE_RXLP_STATS_ENABLE_MASK, 0x007FFFFF);
	CSR_WRITE_4(sc, BGE_RXLP_STATS_CTL, 0x1);

	/* Disable host coalescing until we get it set up */
	CSR_WRITE_4(sc, BGE_HCC_MODE, 0x00000000);

	/* Poll to make sure it's shut down. */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, BGE_HCC_MODE) & BGE_HCCMODE_ENABLE))
			break;
		DELAY(10);
	}

	if (i == BGE_TIMEOUT) {
		printf("bge%d: host coalescing engine failed to idle\n",
		    sc->bge_unit);
		return(ENXIO);
	}

	/* Set up host coalescing defaults */
	CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS, sc->bge_rx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS, sc->bge_tx_coal_ticks);
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS, sc->bge_rx_max_coal_bds);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS, sc->bge_tx_max_coal_bds);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		CSR_WRITE_4(sc, BGE_HCC_RX_COAL_TICKS_INT, 0);
		CSR_WRITE_4(sc, BGE_HCC_TX_COAL_TICKS_INT, 0);
	}
	CSR_WRITE_4(sc, BGE_HCC_RX_MAX_COAL_BDS_INT, 0);
	CSR_WRITE_4(sc, BGE_HCC_TX_MAX_COAL_BDS_INT, 0);

	/* Set up address of statistics block */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_HI,
		    BGE_ADDR_HI(sc->bge_ldata.bge_stats_paddr));
		CSR_WRITE_4(sc, BGE_HCC_STATS_ADDR_LO,
		    BGE_ADDR_LO(sc->bge_ldata.bge_stats_paddr));
		CSR_WRITE_4(sc, BGE_HCC_STATS_BASEADDR, BGE_STATS_BLOCK);
		CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_BASEADDR, BGE_STATUS_BLOCK);
		CSR_WRITE_4(sc, BGE_HCC_STATS_TICKS, sc->bge_stat_ticks);
	}

	/* Set up address of status block */
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_HI,
	    BGE_ADDR_HI(sc->bge_ldata.bge_status_block_paddr));
	CSR_WRITE_4(sc, BGE_HCC_STATUSBLK_ADDR_LO,
	    BGE_ADDR_LO(sc->bge_ldata.bge_status_block_paddr));
	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, BUS_DMASYNC_PREWRITE);
	sc->bge_ldata.bge_status_block->bge_idx[0].bge_rx_prod_idx = 0;
	sc->bge_ldata.bge_status_block->bge_idx[0].bge_tx_cons_idx = 0;

	/* Turn on host coalescing state machine */
	CSR_WRITE_4(sc, BGE_HCC_MODE, BGE_HCCMODE_ENABLE);

	/* Turn on RX BD completion state machine and enable attentions */
	CSR_WRITE_4(sc, BGE_RBDC_MODE,
	    BGE_RBDCMODE_ENABLE|BGE_RBDCMODE_ATTN);

	/* Turn on RX list placement state machine */
	CSR_WRITE_4(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);

	/* Turn on RX list selector state machine. */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		CSR_WRITE_4(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);

	/* Turn on DMA, clear stats */
	CSR_WRITE_4(sc, BGE_MAC_MODE, BGE_MACMODE_TXDMA_ENB|
	    BGE_MACMODE_RXDMA_ENB|BGE_MACMODE_RX_STATS_CLEAR|
	    BGE_MACMODE_TX_STATS_CLEAR|BGE_MACMODE_RX_STATS_ENB|
	    BGE_MACMODE_TX_STATS_ENB|BGE_MACMODE_FRMHDR_DMA_ENB|
	    (sc->bge_tbi ? BGE_PORTMODE_TBI : BGE_PORTMODE_MII));

	/* Set misc. local control, enable interrupts on attentions */
	CSR_WRITE_4(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_ONATTN);

#ifdef notdef
	/* Assert GPIO pins for PHY reset */
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_MISCIO_OUT0|
	    BGE_MLC_MISCIO_OUT1|BGE_MLC_MISCIO_OUT2);
	BGE_SETBIT(sc, BGE_MISC_LOCAL_CTL, BGE_MLC_MISCIO_OUTEN0|
	    BGE_MLC_MISCIO_OUTEN1|BGE_MLC_MISCIO_OUTEN2);
#endif

	/* Turn on DMA completion state machine */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		CSR_WRITE_4(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);

	/* Turn on write DMA state machine */
	CSR_WRITE_4(sc, BGE_WDMA_MODE,
	    BGE_WDMAMODE_ENABLE|BGE_WDMAMODE_ALL_ATTNS);

	/* Turn on read DMA state machine */
	CSR_WRITE_4(sc, BGE_RDMA_MODE,
	    BGE_RDMAMODE_ENABLE|BGE_RDMAMODE_ALL_ATTNS);

	/* Turn on RX data completion state machine */
	CSR_WRITE_4(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);

	/* Turn on RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);

	/* Turn on RX data and RX BD initiator state machine */
	CSR_WRITE_4(sc, BGE_RDBDI_MODE, BGE_RDBDIMODE_ENABLE);

	/* Turn on Mbuf cluster free state machine */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		CSR_WRITE_4(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);

	/* Turn on send BD completion state machine */
	CSR_WRITE_4(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	/* Turn on send data completion state machine */
	CSR_WRITE_4(sc, BGE_SDC_MODE, BGE_SDCMODE_ENABLE);

	/* Turn on send data initiator state machine */
	CSR_WRITE_4(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);

	/* Turn on send BD initiator state machine */
	CSR_WRITE_4(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);

	/* Turn on send BD selector state machine */
	CSR_WRITE_4(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);

	CSR_WRITE_4(sc, BGE_SDI_STATS_ENABLE_MASK, 0x007FFFFF);
	CSR_WRITE_4(sc, BGE_SDI_STATS_CTL,
	    BGE_SDISTATSCTL_ENABLE|BGE_SDISTATSCTL_FASTER);

	/* ack/clear link change events */
	CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED|
	    BGE_MACSTAT_CFG_CHANGED|BGE_MACSTAT_MI_COMPLETE|
	    BGE_MACSTAT_LINK_CHANGED);
	CSR_WRITE_4(sc, BGE_MI_STS, 0);

	/* Enable PHY auto polling (for MII/GMII only) */
	if (sc->bge_tbi) {
		CSR_WRITE_4(sc, BGE_MI_STS, BGE_MISTS_LINK);
	} else {
		BGE_SETBIT(sc, BGE_MI_MODE, BGE_MIMODE_AUTOPOLL|10<<16);
		if (sc->bge_asicrev == BGE_ASICREV_BCM5700)
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
	}

	/* Enable link state change attentions. */
	BGE_SETBIT(sc, BGE_MAC_EVT_ENB, BGE_EVTENB_LINK_CHANGED);

	return(0);
}

/*
 * Probe for a Broadcom chip. Check the PCI vendor and device IDs
 * against our list and return its name if we find a match. Note
 * that since the Broadcom controller contains VPD support, we
 * can get the device name string from the controller itself instead
 * of the compiled-in string. This is a little slow, but it guarantees
 * we'll always announce the right product name.
 */
static int
bge_probe(dev)
	device_t dev;
{
	struct bge_type *t;
	struct bge_softc *sc;
	char *descbuf;

	t = bge_devs;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(struct bge_softc));
	sc->bge_unit = device_get_unit(dev);
	sc->bge_dev = dev;

	while(t->bge_name != NULL) {
		if ((pci_get_vendor(dev) == t->bge_vid) &&
		    (pci_get_device(dev) == t->bge_did)) {
#ifdef notdef
			bge_vpd_read(sc);
			device_set_desc(dev, sc->bge_vpd_prodname);
#endif
			descbuf = malloc(BGE_DEVDESC_MAX, M_TEMP, M_NOWAIT);
			if (descbuf == NULL)
				return(ENOMEM);
			snprintf(descbuf, BGE_DEVDESC_MAX,
			    "%s, ASIC rev. %#04x", t->bge_name,
			    pci_read_config(dev, BGE_PCI_MISC_CTL, 4) >> 16);
			device_set_desc_copy(dev, descbuf);
			if (pci_get_subvendor(dev) == DELL_VENDORID)
				sc->bge_no_3_led = 1;
			free(descbuf, M_TEMP);
			return(0);
		}
		t++;
	}

	return(ENXIO);
}

static void
bge_dma_free(sc)
	struct bge_softc *sc;
{
	int i;


	/* Destroy DMA maps for RX buffers */

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_std_dmamap[i])
			bus_dmamap_destroy(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[i]);
	}

	/* Destroy DMA maps for jumbo RX buffers */

	for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_rx_jumbo_dmamap[i])
			bus_dmamap_destroy(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
	}

	/* Destroy DMA maps for TX buffers */

	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		if (sc->bge_cdata.bge_tx_dmamap[i])
			bus_dmamap_destroy(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_tx_dmamap[i]);
	}

	if (sc->bge_cdata.bge_mtag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_mtag);


	/* Destroy standard RX ring */

	if (sc->bge_ldata.bge_rx_std_ring)
		bus_dmamem_free(sc->bge_cdata.bge_rx_std_ring_tag,
		    sc->bge_ldata.bge_rx_std_ring,
		    sc->bge_cdata.bge_rx_std_ring_map);

	if (sc->bge_cdata.bge_rx_std_ring_map) {
		bus_dmamap_unload(sc->bge_cdata.bge_rx_std_ring_tag,
		    sc->bge_cdata.bge_rx_std_ring_map);
		bus_dmamap_destroy(sc->bge_cdata.bge_rx_std_ring_tag,
		    sc->bge_cdata.bge_rx_std_ring_map);
	}

	if (sc->bge_cdata.bge_rx_std_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_std_ring_tag);

	/* Destroy jumbo RX ring */

	if (sc->bge_ldata.bge_rx_jumbo_ring)
		bus_dmamem_free(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_ldata.bge_rx_jumbo_ring,
		    sc->bge_cdata.bge_rx_jumbo_ring_map);

	if (sc->bge_cdata.bge_rx_jumbo_ring_map) {
		bus_dmamap_unload(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map);
		bus_dmamap_destroy(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map);
	}

	if (sc->bge_cdata.bge_rx_jumbo_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_jumbo_ring_tag);

	/* Destroy RX return ring */

	if (sc->bge_ldata.bge_rx_return_ring)
		bus_dmamem_free(sc->bge_cdata.bge_rx_return_ring_tag,
		    sc->bge_ldata.bge_rx_return_ring,
		    sc->bge_cdata.bge_rx_return_ring_map);

	if (sc->bge_cdata.bge_rx_return_ring_map) {
		bus_dmamap_unload(sc->bge_cdata.bge_rx_return_ring_tag,
		    sc->bge_cdata.bge_rx_return_ring_map);
		bus_dmamap_destroy(sc->bge_cdata.bge_rx_return_ring_tag,
		    sc->bge_cdata.bge_rx_return_ring_map);
	}

	if (sc->bge_cdata.bge_rx_return_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_rx_return_ring_tag);

	/* Destroy TX ring */

	if (sc->bge_ldata.bge_tx_ring)
		bus_dmamem_free(sc->bge_cdata.bge_tx_ring_tag,
		    sc->bge_ldata.bge_tx_ring,
		    sc->bge_cdata.bge_tx_ring_map);

	if (sc->bge_cdata.bge_tx_ring_map) {
		bus_dmamap_unload(sc->bge_cdata.bge_tx_ring_tag,
		    sc->bge_cdata.bge_tx_ring_map);
		bus_dmamap_destroy(sc->bge_cdata.bge_tx_ring_tag,
		    sc->bge_cdata.bge_tx_ring_map);
	}

	if (sc->bge_cdata.bge_tx_ring_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_tx_ring_tag);

	/* Destroy status block */

	if (sc->bge_ldata.bge_status_block)
		bus_dmamem_free(sc->bge_cdata.bge_status_tag,
		    sc->bge_ldata.bge_status_block,
		    sc->bge_cdata.bge_status_map);

	if (sc->bge_cdata.bge_status_map) {
		bus_dmamap_unload(sc->bge_cdata.bge_status_tag,
		    sc->bge_cdata.bge_status_map);
		bus_dmamap_destroy(sc->bge_cdata.bge_status_tag,
		    sc->bge_cdata.bge_status_map);
	}

	if (sc->bge_cdata.bge_status_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_status_tag);

	/* Destroy statistics block */

	if (sc->bge_ldata.bge_stats)
		bus_dmamem_free(sc->bge_cdata.bge_stats_tag,
		    sc->bge_ldata.bge_stats,
		    sc->bge_cdata.bge_stats_map);

	if (sc->bge_cdata.bge_stats_map) {
		bus_dmamap_unload(sc->bge_cdata.bge_stats_tag,
		    sc->bge_cdata.bge_stats_map);
		bus_dmamap_destroy(sc->bge_cdata.bge_stats_tag,
		    sc->bge_cdata.bge_stats_map);
	}

	if (sc->bge_cdata.bge_stats_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_stats_tag);

	/* Destroy the parent tag */

	if (sc->bge_cdata.bge_parent_tag)
		bus_dma_tag_destroy(sc->bge_cdata.bge_parent_tag);

	return;
}

static int
bge_dma_alloc(dev)
	device_t dev;
{
	struct bge_softc *sc;
	int nseg, i, error;
	struct bge_dmamap_arg ctx;

	sc = device_get_softc(dev);

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define BGE_NSEG_NEW 32
	error = bus_dma_tag_create(NULL,	/* parent */
			PAGE_SIZE, 0,		/* alignment, boundary */
			BUS_SPACE_MAXADDR,	/* lowaddr */
			BUS_SPACE_MAXADDR_32BIT,/* highaddr */
			NULL, NULL,		/* filter, filterarg */
			MAXBSIZE, BGE_NSEG_NEW,	/* maxsize, nsegments */
			BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
			0,			/* flags */
			NULL, NULL,		/* lockfunc, lockarg */
			&sc->bge_cdata.bge_parent_tag);

	/*
	 * Create tag for RX mbufs.
	 */
	nseg = 32;
	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag, 1,
	    0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, MCLBYTES * nseg, nseg, MCLBYTES, BUS_DMA_ALLOCNOW, NULL, NULL,
	    &sc->bge_cdata.bge_mtag);

	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Create DMA maps for RX buffers */

	for (i = 0; i < BGE_STD_RX_RING_CNT; i++) {
		error = bus_dmamap_create(sc->bge_cdata.bge_mtag, 0,
			    &sc->bge_cdata.bge_rx_std_dmamap[i]);
		if (error) {
			device_printf(dev, "can't create DMA map for RX\n");
			return(ENOMEM);
		}
	}

	/* Create DMA maps for TX buffers */

	for (i = 0; i < BGE_TX_RING_CNT; i++) {
		error = bus_dmamap_create(sc->bge_cdata.bge_mtag, 0,
			    &sc->bge_cdata.bge_tx_dmamap[i]);
		if (error) {
			device_printf(dev, "can't create DMA map for RX\n");
			return(ENOMEM);
		}
	}

	/* Create tag for standard RX ring */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_STD_RX_RING_SZ, 1, BGE_STD_RX_RING_SZ, 0,
	    NULL, NULL, &sc->bge_cdata.bge_rx_std_ring_tag);

	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for standard RX ring */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_rx_std_ring_tag,
	    (void **)&sc->bge_ldata.bge_rx_std_ring, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_rx_std_ring_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_rx_std_ring, BGE_STD_RX_RING_SZ);

	/* Load the address of the standard RX ring */

	ctx.bge_maxsegs = 1;
	ctx.sc = sc;

	error = bus_dmamap_load(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, sc->bge_ldata.bge_rx_std_ring,
	    BGE_STD_RX_RING_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_rx_std_ring_paddr = ctx.bge_busaddr;

	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {

		/*
		 * Create tag for jumbo mbufs.
		 * This is really a bit of a kludge. We allocate a special
		 * jumbo buffer pool which (thanks to the way our DMA
		 * memory allocation works) will consist of contiguous
		 * pages. This means that even though a jumbo buffer might
		 * be larger than a page size, we don't really need to
		 * map it into more than one DMA segment. However, the
		 * default mbuf tag will result in multi-segment mappings,
		 * so we have to create a special jumbo mbuf tag that
		 * lets us get away with mapping the jumbo buffers as
		 * a single segment. I think eventually the driver should
		 * be changed so that it uses ordinary mbufs and cluster
		 * buffers, i.e. jumbo frames can span multiple DMA
		 * descriptors. But that's a project for another day.
		 */

		error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
		    1, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
		    NULL, MCLBYTES * nseg, nseg, BGE_JLEN, 0, NULL, NULL,
		    &sc->bge_cdata.bge_mtag_jumbo);

		if (error) {
			device_printf(dev, "could not allocate dma tag\n");
			return (ENOMEM);
		}

		/* Create tag for jumbo RX ring */

		error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
		    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
		    NULL, BGE_JUMBO_RX_RING_SZ, 1, BGE_JUMBO_RX_RING_SZ, 0,
		    NULL, NULL, &sc->bge_cdata.bge_rx_jumbo_ring_tag);

		if (error) {
			device_printf(dev, "could not allocate dma tag\n");
			return (ENOMEM);
		}

		/* Allocate DMA'able memory for jumbo RX ring */

		error = bus_dmamem_alloc(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    (void **)&sc->bge_ldata.bge_rx_jumbo_ring, BUS_DMA_NOWAIT,
		    &sc->bge_cdata.bge_rx_jumbo_ring_map);
		if (error)
			return (ENOMEM);

		bzero((char *)sc->bge_ldata.bge_rx_jumbo_ring,
		    BGE_JUMBO_RX_RING_SZ);

		/* Load the address of the jumbo RX ring */

		ctx.bge_maxsegs = 1;
		ctx.sc = sc;

		error = bus_dmamap_load(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map,
		    sc->bge_ldata.bge_rx_jumbo_ring, BGE_JUMBO_RX_RING_SZ,
		    bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

		if (error)
			return (ENOMEM);

		sc->bge_ldata.bge_rx_jumbo_ring_paddr = ctx.bge_busaddr;

		/* Create DMA maps for jumbo RX buffers */

		for (i = 0; i < BGE_JUMBO_RX_RING_CNT; i++) {
			error = bus_dmamap_create(sc->bge_cdata.bge_mtag_jumbo,
				    0, &sc->bge_cdata.bge_rx_jumbo_dmamap[i]);
			if (error) {
				device_printf(dev,
				    "can't create DMA map for RX\n");
				return(ENOMEM);
			}
		}

	}

	/* Create tag for RX return ring */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_RX_RTN_RING_SZ(sc), 1, BGE_RX_RTN_RING_SZ(sc), 0,
	    NULL, NULL, &sc->bge_cdata.bge_rx_return_ring_tag);

	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for RX return ring */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_rx_return_ring_tag,
	    (void **)&sc->bge_ldata.bge_rx_return_ring, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_rx_return_ring_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_rx_return_ring,
	    BGE_RX_RTN_RING_SZ(sc));

	/* Load the address of the RX return ring */

	ctx.bge_maxsegs = 1;
	ctx.sc = sc;

	error = bus_dmamap_load(sc->bge_cdata.bge_rx_return_ring_tag,
	    sc->bge_cdata.bge_rx_return_ring_map,
	    sc->bge_ldata.bge_rx_return_ring, BGE_RX_RTN_RING_SZ(sc),
	    bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_rx_return_ring_paddr = ctx.bge_busaddr;

	/* Create tag for TX ring */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_TX_RING_SZ, 1, BGE_TX_RING_SZ, 0, NULL, NULL,
	    &sc->bge_cdata.bge_tx_ring_tag);

	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for TX ring */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_tx_ring_tag,
	    (void **)&sc->bge_ldata.bge_tx_ring, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_tx_ring_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_tx_ring, BGE_TX_RING_SZ);

	/* Load the address of the TX ring */

	ctx.bge_maxsegs = 1;
	ctx.sc = sc;

	error = bus_dmamap_load(sc->bge_cdata.bge_tx_ring_tag,
	    sc->bge_cdata.bge_tx_ring_map, sc->bge_ldata.bge_tx_ring,
	    BGE_TX_RING_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_tx_ring_paddr = ctx.bge_busaddr;

	/* Create tag for status block */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_STATUS_BLK_SZ, 1, BGE_STATUS_BLK_SZ, 0,
	    NULL, NULL, &sc->bge_cdata.bge_status_tag);

	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for status block */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_status_tag,
	    (void **)&sc->bge_ldata.bge_status_block, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_status_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_status_block, BGE_STATUS_BLK_SZ);

	/* Load the address of the status block */

	ctx.sc = sc;
	ctx.bge_maxsegs = 1;

	error = bus_dmamap_load(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, sc->bge_ldata.bge_status_block,
	    BGE_STATUS_BLK_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_status_block_paddr = ctx.bge_busaddr;

	/* Create tag for statistics block */

	error = bus_dma_tag_create(sc->bge_cdata.bge_parent_tag,
	    PAGE_SIZE, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL,
	    NULL, BGE_STATS_SZ, 1, BGE_STATS_SZ, 0, NULL, NULL,
	    &sc->bge_cdata.bge_stats_tag);

	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		return (ENOMEM);
	}

	/* Allocate DMA'able memory for statistics block */

	error = bus_dmamem_alloc(sc->bge_cdata.bge_stats_tag,
	    (void **)&sc->bge_ldata.bge_stats, BUS_DMA_NOWAIT,
	    &sc->bge_cdata.bge_stats_map);
	if (error)
		return (ENOMEM);

	bzero((char *)sc->bge_ldata.bge_stats, BGE_STATS_SZ);

	/* Load the address of the statstics block */

	ctx.sc = sc;
	ctx.bge_maxsegs = 1;

	error = bus_dmamap_load(sc->bge_cdata.bge_stats_tag,
	    sc->bge_cdata.bge_stats_map, sc->bge_ldata.bge_stats,
	    BGE_STATS_SZ, bge_dma_map_addr, &ctx, BUS_DMA_NOWAIT);

	if (error)
		return (ENOMEM);

	sc->bge_ldata.bge_stats_paddr = ctx.bge_busaddr;

	return(0);
}

static int
bge_attach(dev)
	device_t dev;
{
	struct ifnet *ifp;
	struct bge_softc *sc;
	u_int32_t hwcfg = 0;
	u_int32_t mac_addr = 0;
	int unit, error = 0, rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	sc->bge_dev = dev;
	sc->bge_unit = unit;

	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = BGE_PCI_BAR0;
	sc->bge_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE|PCI_RF_DENSE);

	if (sc->bge_res == NULL) {
		printf ("bge%d: couldn't map memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->bge_btag = rman_get_bustag(sc->bge_res);
	sc->bge_bhandle = rman_get_bushandle(sc->bge_res);
	sc->bge_vhandle = (vm_offset_t)rman_get_virtual(sc->bge_res);

	/* Allocate interrupt */
	rid = 0;

	sc->bge_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->bge_irq == NULL) {
		printf("bge%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->bge_unit = unit;

	BGE_LOCK_INIT(sc, device_get_nameunit(dev));

	/* Save ASIC rev. */

	sc->bge_chipid =
	    pci_read_config(dev, BGE_PCI_MISC_CTL, 4) &
	    BGE_PCIMISCCTL_ASICREV;
	sc->bge_asicrev = BGE_ASICREV(sc->bge_chipid);
	sc->bge_chiprev = BGE_CHIPREV(sc->bge_chipid);

	/*
	 * Treat the 5714 and the 5752 like the 5750 until we have more info
	 * on this chip.
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5714 || 
            sc->bge_asicrev == BGE_ASICREV_BCM5752)
		sc->bge_asicrev = BGE_ASICREV_BCM5750;

	/*
	 * XXX: Broadcom Linux driver.  Not in specs or eratta.
	 * PCI-Express?
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5750) {
		u_int32_t v;

		v = pci_read_config(dev, BGE_PCI_MSI_CAPID, 4);
		if (((v >> 8) & 0xff) == BGE_PCIE_CAPID_REG) {
			v = pci_read_config(dev, BGE_PCIE_CAPID_REG, 4);
			if ((v & 0xff) == BGE_PCIE_CAPID)
				sc->bge_pcie = 1;
		}
	}

	/* Try to reset the chip. */
	bge_reset(sc);

	if (bge_chipinit(sc)) {
		printf("bge%d: chip initialization failed\n", sc->bge_unit);
		bge_release_resources(sc);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Get station address from the EEPROM.
	 */
	mac_addr = bge_readmem_ind(sc, 0x0c14);
	if ((mac_addr >> 16) == 0x484b) {
		sc->arpcom.ac_enaddr[0] = (u_char)(mac_addr >> 8);
		sc->arpcom.ac_enaddr[1] = (u_char)mac_addr;
		mac_addr = bge_readmem_ind(sc, 0x0c18);
		sc->arpcom.ac_enaddr[2] = (u_char)(mac_addr >> 24);
		sc->arpcom.ac_enaddr[3] = (u_char)(mac_addr >> 16);
		sc->arpcom.ac_enaddr[4] = (u_char)(mac_addr >> 8);
		sc->arpcom.ac_enaddr[5] = (u_char)mac_addr;
	} else if (bge_read_eeprom(sc, (caddr_t)&sc->arpcom.ac_enaddr,
	    BGE_EE_MAC_OFFSET + 2, ETHER_ADDR_LEN)) {
		printf("bge%d: failed to read station address\n", unit);
		bge_release_resources(sc);
		error = ENXIO;
		goto fail;
	}

	/* 5705 limits RX return ring to 512 entries. */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750)
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT_5705;
	else
		sc->bge_return_ring_cnt = BGE_RETURN_RING_CNT;

	if (bge_dma_alloc(dev)) {
		printf ("bge%d: failed to allocate DMA resources\n",
		    sc->bge_unit);
		bge_release_resources(sc);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Try to allocate memory for jumbo buffers.
	 * The 5705 does not appear to support jumbo frames.
	 */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		if (bge_alloc_jumbo_mem(sc)) {
			printf("bge%d: jumbo buffer allocation "
			    "failed\n", sc->bge_unit);
			bge_release_resources(sc);
			error = ENXIO;
			goto fail;
		}
	}

	/* Set default tuneable values. */
	sc->bge_stat_ticks = BGE_TICKS_PER_SEC;
	sc->bge_rx_coal_ticks = 150;
	sc->bge_tx_coal_ticks = 150;
	sc->bge_rx_max_coal_bds = 64;
	sc->bge_tx_max_coal_bds = 128;

	/* Set up ifnet structure */
	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = bge_ioctl;
	ifp->if_start = bge_start;
	ifp->if_watchdog = bge_watchdog;
	ifp->if_init = bge_init;
	ifp->if_mtu = ETHERMTU;
	ifp->if_snd.ifq_maxlen = BGE_TX_RING_CNT - 1;
	ifp->if_hwassist = BGE_CSUM_FEATURES;
	/* NB: the code for RX csum offload is disabled for now */
	ifp->if_capabilities = IFCAP_TXCSUM | IFCAP_VLAN_HWTAGGING |
	    IFCAP_VLAN_MTU;
	ifp->if_capenable = ifp->if_capabilities;

	/*
	 * Figure out what sort of media we have by checking the
	 * hardware config word in the first 32k of NIC internal memory,
	 * or fall back to examining the EEPROM if necessary.
	 * Note: on some BCM5700 cards, this value appears to be unset.
	 * If that's the case, we have to rely on identifying the NIC
	 * by its PCI subsystem ID, as we do below for the SysKonnect
	 * SK-9D41.
	 */
	if (bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_SIG) == BGE_MAGIC_NUMBER)
		hwcfg = bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM_NICCFG);
	else {
		bge_read_eeprom(sc, (caddr_t)&hwcfg,
				BGE_EE_HWCFG_OFFSET, sizeof(hwcfg));
		hwcfg = ntohl(hwcfg);
	}

	if ((hwcfg & BGE_HWCFG_MEDIA) == BGE_MEDIA_FIBER)
		sc->bge_tbi = 1;

	/* The SysKonnect SK-9D41 is a 1000baseSX card. */
	if ((pci_read_config(dev, BGE_PCI_SUBSYS, 4) >> 16) == SK_SUBSYSID_9D41)
		sc->bge_tbi = 1;

	if (sc->bge_tbi) {
		ifmedia_init(&sc->bge_ifmedia, IFM_IMASK,
		    bge_ifmedia_upd, bge_ifmedia_sts);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER|IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->bge_ifmedia,
		    IFM_ETHER|IFM_1000_SX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->bge_ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->bge_ifmedia, IFM_ETHER|IFM_AUTO);
		sc->bge_ifmedia.ifm_media = sc->bge_ifmedia.ifm_cur->ifm_media;
	} else {
		/*
		 * Do transceiver setup.
		 */
		if (mii_phy_probe(dev, &sc->bge_miibus,
		    bge_ifmedia_upd, bge_ifmedia_sts)) {
			printf("bge%d: MII without any PHY!\n", sc->bge_unit);
			bge_release_resources(sc);
			bge_free_jumbo_mem(sc);
			error = ENXIO;
			goto fail;
		}
	}

	/*
	 * When using the BCM5701 in PCI-X mode, data corruption has
	 * been observed in the first few bytes of some received packets.
	 * Aligning the packet buffer in memory eliminates the corruption.
	 * Unfortunately, this misaligns the packet payloads.  On platforms
	 * which do not support unaligned accesses, we will realign the
	 * payloads by copying the received packets.
	 */
	switch (sc->bge_chipid) {
	case BGE_CHIPID_BCM5701_A0:
	case BGE_CHIPID_BCM5701_B0:
	case BGE_CHIPID_BCM5701_B2:
	case BGE_CHIPID_BCM5701_B5:
		/* If in PCI-X mode, work around the alignment bug. */
		if ((pci_read_config(dev, BGE_PCI_PCISTATE, 4) &
		    (BGE_PCISTATE_PCI_BUSMODE | BGE_PCISTATE_PCI_BUSSPEED)) ==
		    BGE_PCISTATE_PCI_BUSSPEED)
			sc->bge_rx_alignment_bug = 1;
		break;
	}

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, sc->arpcom.ac_enaddr);
	callout_init(&sc->bge_stat_ch, CALLOUT_MPSAFE);

	/*
	 * Hookup IRQ last.
	 */
	error = bus_setup_intr(dev, sc->bge_irq, INTR_TYPE_NET | INTR_MPSAFE,
	   bge_intr, sc, &sc->bge_intrhand);

	if (error) {
		bge_release_resources(sc);
		printf("bge%d: couldn't set up irq\n", unit);
	}

fail:
	return(error);
}

static int
bge_detach(dev)
	device_t dev;
{
	struct bge_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = &sc->arpcom.ac_if;

	BGE_LOCK(sc);
	bge_stop(sc);
	bge_reset(sc);
	BGE_UNLOCK(sc);

	ether_ifdetach(ifp);

	if (sc->bge_tbi) {
		ifmedia_removeall(&sc->bge_ifmedia);
	} else {
		bus_generic_detach(dev);
		device_delete_child(dev, sc->bge_miibus);
	}

	bge_release_resources(sc);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		bge_free_jumbo_mem(sc);

	return(0);
}

static void
bge_release_resources(sc)
	struct bge_softc *sc;
{
	device_t dev;

	dev = sc->bge_dev;

	if (sc->bge_vpd_prodname != NULL)
		free(sc->bge_vpd_prodname, M_DEVBUF);

	if (sc->bge_vpd_readonly != NULL)
		free(sc->bge_vpd_readonly, M_DEVBUF);

	if (sc->bge_intrhand != NULL)
		bus_teardown_intr(dev, sc->bge_irq, sc->bge_intrhand);

	if (sc->bge_irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->bge_irq);

	if (sc->bge_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    BGE_PCI_BAR0, sc->bge_res);

	bge_dma_free(sc);

	if (mtx_initialized(&sc->bge_mtx))	/* XXX */
		BGE_LOCK_DESTROY(sc);

	return;
}

static void
bge_reset(sc)
	struct bge_softc *sc;
{
	device_t dev;
	u_int32_t cachesize, command, pcistate, reset;
	int i, val = 0;

	dev = sc->bge_dev;

	/* Save some important PCI state. */
	cachesize = pci_read_config(dev, BGE_PCI_CACHESZ, 4);
	command = pci_read_config(dev, BGE_PCI_CMD, 4);
	pcistate = pci_read_config(dev, BGE_PCI_PCISTATE, 4);

	pci_write_config(dev, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS|BGE_PCIMISCCTL_MASK_PCI_INTR|
	    BGE_PCIMISCCTL_ENDIAN_WORDSWAP|BGE_PCIMISCCTL_PCISTATE_RW, 4);

	reset = BGE_MISCCFG_RESET_CORE_CLOCKS|(65<<1);

	/* XXX: Broadcom Linux driver. */
	if (sc->bge_pcie) {
		if (CSR_READ_4(sc, 0x7e2c) == 0x60)	/* PCIE 1.0 */
			CSR_WRITE_4(sc, 0x7e2c, 0x20);
		if (sc->bge_chipid != BGE_CHIPID_BCM5750_A0) {
			/* Prevent PCIE link training during global reset */
			CSR_WRITE_4(sc, BGE_MISC_CFG, (1<<29));
			reset |= (1<<29);
		}
	}

	/* Issue global reset */
	bge_writereg_ind(sc, BGE_MISC_CFG, reset);

	DELAY(1000);

	/* XXX: Broadcom Linux driver. */
	if (sc->bge_pcie) {
		if (sc->bge_chipid == BGE_CHIPID_BCM5750_A0) {
			uint32_t v;

			DELAY(500000); /* wait for link training to complete */
			v = pci_read_config(dev, 0xc4, 4);
			pci_write_config(dev, 0xc4, v | (1<<15), 4);
		}
		/* Set PCIE max payload size and clear error status. */
		pci_write_config(dev, 0xd8, 0xf5000, 4);
	}

	/* Reset some of the PCI state that got zapped by reset */
	pci_write_config(dev, BGE_PCI_MISC_CTL,
	    BGE_PCIMISCCTL_INDIRECT_ACCESS|BGE_PCIMISCCTL_MASK_PCI_INTR|
	    BGE_PCIMISCCTL_ENDIAN_WORDSWAP|BGE_PCIMISCCTL_PCISTATE_RW, 4);
	pci_write_config(dev, BGE_PCI_CACHESZ, cachesize, 4);
	pci_write_config(dev, BGE_PCI_CMD, command, 4);
	bge_writereg_ind(sc, BGE_MISC_CFG, (65 << 1));

	/* Enable memory arbiter. */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		CSR_WRITE_4(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);

	/*
	 * Prevent PXE restart: write a magic number to the
	 * general communications memory at 0xB50.
	 */
	bge_writemem_ind(sc, BGE_SOFTWARE_GENCOMM, BGE_MAGIC_NUMBER);
	/*
	 * Poll the value location we just wrote until
	 * we see the 1's complement of the magic number.
	 * This indicates that the firmware initialization
	 * is complete.
	 */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		val = bge_readmem_ind(sc, BGE_SOFTWARE_GENCOMM);
		if (val == ~BGE_MAGIC_NUMBER)
			break;
		DELAY(10);
	}

	if (i == BGE_TIMEOUT) {
		printf("bge%d: firmware handshake timed out\n", sc->bge_unit);
		return;
	}

	/*
	 * XXX Wait for the value of the PCISTATE register to
	 * return to its original pre-reset state. This is a
	 * fairly good indicator of reset completion. If we don't
	 * wait for the reset to fully complete, trying to read
	 * from the device's non-PCI registers may yield garbage
	 * results.
	 */
	for (i = 0; i < BGE_TIMEOUT; i++) {
		if (pci_read_config(dev, BGE_PCI_PCISTATE, 4) == pcistate)
			break;
		DELAY(10);
	}

	/* Fix up byte swapping */
	CSR_WRITE_4(sc, BGE_MODE_CTL, BGE_MODECTL_BYTESWAP_NONFRAME|
	    BGE_MODECTL_BYTESWAP_DATA);

	CSR_WRITE_4(sc, BGE_MAC_MODE, 0);

	/*
	 * The 5704 in TBI mode apparently needs some special
	 * adjustment to insure the SERDES drive level is set
	 * to 1.2V.
	 */
	if (sc->bge_asicrev == BGE_ASICREV_BCM5704 && sc->bge_tbi) {
		uint32_t serdescfg;
		serdescfg = CSR_READ_4(sc, BGE_SERDES_CFG);
		serdescfg = (serdescfg & ~0xFFF) | 0x880;
		CSR_WRITE_4(sc, BGE_SERDES_CFG, serdescfg);
	}

	/* XXX: Broadcom Linux driver. */
	if (sc->bge_pcie && sc->bge_chipid != BGE_CHIPID_BCM5750_A0) {
		uint32_t v;

		v = CSR_READ_4(sc, 0x7c00);
		CSR_WRITE_4(sc, 0x7c00, v | (1<<25));
	}
	DELAY(10000);

	return;
}

/*
 * Frame reception handling. This is called if there's a frame
 * on the receive return list.
 *
 * Note: we have to be able to handle two possibilities here:
 * 1) the frame is from the jumbo recieve ring
 * 2) the frame is from the standard receive ring
 */

static void
bge_rxeof(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	int stdcnt = 0, jumbocnt = 0;

	BGE_LOCK_ASSERT(sc);

	ifp = &sc->arpcom.ac_if;

	bus_dmamap_sync(sc->bge_cdata.bge_rx_return_ring_tag,
	    sc->bge_cdata.bge_rx_return_ring_map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map, BUS_DMASYNC_POSTREAD);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map,
		    BUS_DMASYNC_POSTREAD);
	}

	while(sc->bge_rx_saved_considx !=
	    sc->bge_ldata.bge_status_block->bge_idx[0].bge_rx_prod_idx) {
		struct bge_rx_bd	*cur_rx;
		u_int32_t		rxidx;
		struct ether_header	*eh;
		struct mbuf		*m = NULL;
		u_int16_t		vlan_tag = 0;
		int			have_tag = 0;

		cur_rx =
	    &sc->bge_ldata.bge_rx_return_ring[sc->bge_rx_saved_considx];

		rxidx = cur_rx->bge_idx;
		BGE_INC(sc->bge_rx_saved_considx, sc->bge_return_ring_cnt);

		if (cur_rx->bge_flags & BGE_RXBDFLAG_VLAN_TAG) {
			have_tag = 1;
			vlan_tag = cur_rx->bge_vlan_tag;
		}

		if (cur_rx->bge_flags & BGE_RXBDFLAG_JUMBO_RING) {
			BGE_INC(sc->bge_jumbo, BGE_JUMBO_RX_RING_CNT);
			bus_dmamap_sync(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[rxidx],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_cdata.bge_mtag_jumbo,
			    sc->bge_cdata.bge_rx_jumbo_dmamap[rxidx]);
			m = sc->bge_cdata.bge_rx_jumbo_chain[rxidx];
			sc->bge_cdata.bge_rx_jumbo_chain[rxidx] = NULL;
			jumbocnt++;
			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				ifp->if_ierrors++;
				bge_newbuf_jumbo(sc, sc->bge_jumbo, m);
				continue;
			}
			if (bge_newbuf_jumbo(sc,
			    sc->bge_jumbo, NULL) == ENOBUFS) {
				ifp->if_ierrors++;
				bge_newbuf_jumbo(sc, sc->bge_jumbo, m);
				continue;
			}
		} else {
			BGE_INC(sc->bge_std, BGE_STD_RX_RING_CNT);
			bus_dmamap_sync(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[rxidx],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_rx_std_dmamap[rxidx]);
			m = sc->bge_cdata.bge_rx_std_chain[rxidx];
			sc->bge_cdata.bge_rx_std_chain[rxidx] = NULL;
			stdcnt++;
			if (cur_rx->bge_flags & BGE_RXBDFLAG_ERROR) {
				ifp->if_ierrors++;
				bge_newbuf_std(sc, sc->bge_std, m);
				continue;
			}
			if (bge_newbuf_std(sc, sc->bge_std,
			    NULL) == ENOBUFS) {
				ifp->if_ierrors++;
				bge_newbuf_std(sc, sc->bge_std, m);
				continue;
			}
		}

		ifp->if_ipackets++;
#ifndef __i386__
		/*
		 * The i386 allows unaligned accesses, but for other
		 * platforms we must make sure the payload is aligned.
		 */
		if (sc->bge_rx_alignment_bug) {
			bcopy(m->m_data, m->m_data + ETHER_ALIGN,
			    cur_rx->bge_len);
			m->m_data += ETHER_ALIGN;
		}
#endif
		eh = mtod(m, struct ether_header *);
		m->m_pkthdr.len = m->m_len = cur_rx->bge_len - ETHER_CRC_LEN;
		m->m_pkthdr.rcvif = ifp;

#if 0 /* currently broken for some packets, possibly related to TCP options */
		if (ifp->if_capenable & IFCAP_RXCSUM) {
			m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
			if ((cur_rx->bge_ip_csum ^ 0xffff) == 0)
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			if (cur_rx->bge_flags & BGE_RXBDFLAG_TCP_UDP_CSUM) {
				m->m_pkthdr.csum_data =
				    cur_rx->bge_tcp_udp_csum;
				m->m_pkthdr.csum_flags |= CSUM_DATA_VALID;
			}
		}
#endif

		/*
		 * If we received a packet with a vlan tag,
		 * attach that information to the packet.
		 */
		if (have_tag)
			VLAN_INPUT_TAG(ifp, m, vlan_tag, continue);

		BGE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		BGE_LOCK(sc);
	}

	bus_dmamap_sync(sc->bge_cdata.bge_rx_return_ring_tag,
	    sc->bge_cdata.bge_rx_return_ring_map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->bge_cdata.bge_rx_std_ring_tag,
	    sc->bge_cdata.bge_rx_std_ring_map,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_PREWRITE);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		bus_dmamap_sync(sc->bge_cdata.bge_rx_jumbo_ring_tag,
		    sc->bge_cdata.bge_rx_jumbo_ring_map,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}

	CSR_WRITE_4(sc, BGE_MBX_RX_CONS0_LO, sc->bge_rx_saved_considx);
	if (stdcnt)
		CSR_WRITE_4(sc, BGE_MBX_RX_STD_PROD_LO, sc->bge_std);
	if (jumbocnt)
		CSR_WRITE_4(sc, BGE_MBX_RX_JUMBO_PROD_LO, sc->bge_jumbo);

	return;
}

static void
bge_txeof(sc)
	struct bge_softc *sc;
{
	struct bge_tx_bd *cur_tx = NULL;
	struct ifnet *ifp;

	BGE_LOCK_ASSERT(sc);

	ifp = &sc->arpcom.ac_if;

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	while (sc->bge_tx_saved_considx !=
	    sc->bge_ldata.bge_status_block->bge_idx[0].bge_tx_cons_idx) {
		u_int32_t		idx = 0;

		idx = sc->bge_tx_saved_considx;
		cur_tx = &sc->bge_ldata.bge_tx_ring[idx];
		if (cur_tx->bge_flags & BGE_TXBDFLAG_END)
			ifp->if_opackets++;
		if (sc->bge_cdata.bge_tx_chain[idx] != NULL) {
			m_freem(sc->bge_cdata.bge_tx_chain[idx]);
			sc->bge_cdata.bge_tx_chain[idx] = NULL;
			bus_dmamap_unload(sc->bge_cdata.bge_mtag,
			    sc->bge_cdata.bge_tx_dmamap[idx]);
		}
		sc->bge_txcnt--;
		BGE_INC(sc->bge_tx_saved_considx, BGE_TX_RING_CNT);
		ifp->if_timer = 0;
	}

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

static void
bge_intr(xsc)
	void *xsc;
{
	struct bge_softc *sc;
	struct ifnet *ifp;
	u_int32_t statusword;
	u_int32_t status, mimode;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	BGE_LOCK(sc);

	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, BUS_DMASYNC_POSTWRITE);

	statusword =
	    atomic_readandclear_32(&sc->bge_ldata.bge_status_block->bge_status);

#ifdef notdef
	/* Avoid this for now -- checking this register is expensive. */
	/* Make sure this is really our interrupt. */
	if (!(CSR_READ_4(sc, BGE_MISC_LOCAL_CTL) & BGE_MLC_INTR_STATE))
		return;
#endif
	/* Ack interrupt and stop others from occuring. */
	CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 1);

	/*
	 * Process link state changes.
	 * Grrr. The link status word in the status block does
	 * not work correctly on the BCM5700 rev AX and BX chips,
	 * according to all available information. Hence, we have
	 * to enable MII interrupts in order to properly obtain
	 * async link changes. Unfortunately, this also means that
	 * we have to read the MAC status register to detect link
	 * changes, thereby adding an additional register access to
	 * the interrupt handler.
	 */

	if (sc->bge_asicrev == BGE_ASICREV_BCM5700) {

		status = CSR_READ_4(sc, BGE_MAC_STS);
		if (status & BGE_MACSTAT_MI_INTERRUPT) {
			sc->bge_link = 0;
			callout_stop(&sc->bge_stat_ch);
			bge_tick_locked(sc);
			/* Clear the interrupt */
			CSR_WRITE_4(sc, BGE_MAC_EVT_ENB,
			    BGE_EVTENB_MI_INTERRUPT);
			bge_miibus_readreg(sc->bge_dev, 1, BRGPHY_MII_ISR);
			bge_miibus_writereg(sc->bge_dev, 1, BRGPHY_MII_IMR,
			    BRGPHY_INTRS);
		}
	} else {
		if (statusword & BGE_STATFLAG_LINKSTATE_CHANGED) {
			/*
			 * Sometimes PCS encoding errors are detected in
			 * TBI mode (on fiber NICs), and for some reason
			 * the chip will signal them as link changes.
			 * If we get a link change event, but the 'PCS
			 * encoding error' bit in the MAC status register
			 * is set, don't bother doing a link check.
			 * This avoids spurious "gigabit link up" messages
			 * that sometimes appear on fiber NICs during
			 * periods of heavy traffic. (There should be no
			 * effect on copper NICs.)
			 *
			 * If we do have a copper NIC (bge_tbi == 0) then
			 * check that the AUTOPOLL bit is set before
			 * processing the event as a real link change.
			 * Turning AUTOPOLL on and off in the MII read/write
			 * functions will often trigger a link status
			 * interrupt for no reason.
			 */
			status = CSR_READ_4(sc, BGE_MAC_STS);
			mimode = CSR_READ_4(sc, BGE_MI_MODE);
			if (!(status & (BGE_MACSTAT_PORT_DECODE_ERROR|
			    BGE_MACSTAT_MI_COMPLETE)) && (!sc->bge_tbi &&
			    (mimode & BGE_MIMODE_AUTOPOLL))) {
				sc->bge_link = 0;
				callout_stop(&sc->bge_stat_ch);
				bge_tick_locked(sc);
			}
			/* Clear the interrupt */
			CSR_WRITE_4(sc, BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED|
			    BGE_MACSTAT_CFG_CHANGED|BGE_MACSTAT_MI_COMPLETE|
			    BGE_MACSTAT_LINK_CHANGED);

			/* Force flush the status block cached by PCI bridge */
			CSR_READ_4(sc, BGE_MBX_IRQ0_LO);
		}
	}

	if (ifp->if_flags & IFF_RUNNING) {
		/* Check RX return ring producer/consumer */
		bge_rxeof(sc);

		/* Check TX ring producer/consumer */
		bge_txeof(sc);
	}

	bus_dmamap_sync(sc->bge_cdata.bge_status_tag,
	    sc->bge_cdata.bge_status_map, BUS_DMASYNC_PREWRITE);

	bge_handle_events(sc);

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 0);

	if (ifp->if_flags & IFF_RUNNING && ifp->if_snd.ifq_head != NULL)
		bge_start_locked(ifp);

	BGE_UNLOCK(sc);

	return;
}

static void
bge_tick_locked(sc)
	struct bge_softc *sc;
{
	struct mii_data *mii = NULL;
	struct ifmedia *ifm = NULL;
	struct ifnet *ifp;

	ifp = &sc->arpcom.ac_if;

	BGE_LOCK_ASSERT(sc);

	if (sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
	    sc->bge_asicrev == BGE_ASICREV_BCM5750)
		bge_stats_update_regs(sc);
	else
		bge_stats_update(sc);
	callout_reset(&sc->bge_stat_ch, hz, bge_tick, sc);
	if (sc->bge_link)
		return;

	if (sc->bge_tbi) {
		ifm = &sc->bge_ifmedia;
		if (CSR_READ_4(sc, BGE_MAC_STS) &
		    BGE_MACSTAT_TBI_PCS_SYNCHED) {
			sc->bge_link++;
			if (sc->bge_asicrev == BGE_ASICREV_BCM5704)
				BGE_CLRBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_TBI_SEND_CFGS);
			CSR_WRITE_4(sc, BGE_MAC_STS, 0xFFFFFFFF);
			if (bootverbose)
				printf("bge%d: gigabit link up\n",
				    sc->bge_unit);
			if (ifp->if_snd.ifq_head != NULL)
				bge_start_locked(ifp);
		}
		return;
	}

	mii = device_get_softc(sc->bge_miibus);
	mii_tick(mii);

	if (!sc->bge_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->bge_link++;
		if ((IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T ||
		    IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_SX) &&
		    bootverbose)
			printf("bge%d: gigabit link up\n", sc->bge_unit);
		if (ifp->if_snd.ifq_head != NULL)
			bge_start_locked(ifp);
	}

	return;
}

static void
bge_tick(xsc)
	void *xsc;
{
	struct bge_softc *sc;

	sc = xsc;

	BGE_LOCK(sc);
	bge_tick_locked(sc);
	BGE_UNLOCK(sc);
}

static void
bge_stats_update_regs(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	struct bge_mac_stats_regs stats;
	u_int32_t *s;
	int i;

	ifp = &sc->arpcom.ac_if;

	s = (u_int32_t *)&stats;
	for (i = 0; i < sizeof(struct bge_mac_stats_regs); i += 4) {
		*s = CSR_READ_4(sc, BGE_RX_STATS + i);
		s++;
	}

	ifp->if_collisions +=
	   (stats.dot3StatsSingleCollisionFrames +
	   stats.dot3StatsMultipleCollisionFrames +
	   stats.dot3StatsExcessiveCollisions +
	   stats.dot3StatsLateCollisions) -
	   ifp->if_collisions;

	return;
}

static void
bge_stats_update(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	struct bge_stats *stats;

	ifp = &sc->arpcom.ac_if;

	stats = (struct bge_stats *)(sc->bge_vhandle +
	    BGE_MEMWIN_START + BGE_STATS_BLOCK);

	ifp->if_collisions +=
	   (stats->txstats.dot3StatsSingleCollisionFrames.bge_addr_lo +
	   stats->txstats.dot3StatsMultipleCollisionFrames.bge_addr_lo +
	   stats->txstats.dot3StatsExcessiveCollisions.bge_addr_lo +
	   stats->txstats.dot3StatsLateCollisions.bge_addr_lo) -
	   ifp->if_collisions;

#ifdef notdef
	ifp->if_collisions +=
	   (sc->bge_rdata->bge_info.bge_stats.dot3StatsSingleCollisionFrames +
	   sc->bge_rdata->bge_info.bge_stats.dot3StatsMultipleCollisionFrames +
	   sc->bge_rdata->bge_info.bge_stats.dot3StatsExcessiveCollisions +
	   sc->bge_rdata->bge_info.bge_stats.dot3StatsLateCollisions) -
	   ifp->if_collisions;
#endif

	return;
}

/*
 * Encapsulate an mbuf chain in the tx ring  by coupling the mbuf data
 * pointers to descriptors.
 */
static int
bge_encap(sc, m_head, txidx)
	struct bge_softc *sc;
	struct mbuf *m_head;
	u_int32_t *txidx;
{
	struct bge_tx_bd	*f = NULL;
	u_int16_t		csum_flags = 0;
	struct m_tag		*mtag;
	struct bge_dmamap_arg	ctx;
	bus_dmamap_t		map;
	int			error;


	if (m_head->m_pkthdr.csum_flags) {
		if (m_head->m_pkthdr.csum_flags & CSUM_IP)
			csum_flags |= BGE_TXBDFLAG_IP_CSUM;
		if (m_head->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP))
			csum_flags |= BGE_TXBDFLAG_TCP_UDP_CSUM;
		if (m_head->m_flags & M_LASTFRAG)
			csum_flags |= BGE_TXBDFLAG_IP_FRAG_END;
		else if (m_head->m_flags & M_FRAG)
			csum_flags |= BGE_TXBDFLAG_IP_FRAG;
	}

	mtag = VLAN_OUTPUT_TAG(&sc->arpcom.ac_if, m_head);

	ctx.sc = sc;
	ctx.bge_idx = *txidx;
	ctx.bge_ring = sc->bge_ldata.bge_tx_ring;
	ctx.bge_flags = csum_flags;
	/*
	 * Sanity check: avoid coming within 16 descriptors
	 * of the end of the ring.
	 */
	ctx.bge_maxsegs = (BGE_TX_RING_CNT - sc->bge_txcnt) - 16;

	map = sc->bge_cdata.bge_tx_dmamap[*txidx];
	error = bus_dmamap_load_mbuf(sc->bge_cdata.bge_mtag, map,
	    m_head, bge_dma_map_tx_desc, &ctx, BUS_DMA_NOWAIT);

	if (error || ctx.bge_maxsegs == 0 /*||
	    ctx.bge_idx == sc->bge_tx_saved_considx*/)
		return (ENOBUFS);

	/*
	 * Insure that the map for this transmission
	 * is placed at the array index of the last descriptor
	 * in this chain.
	 */
	sc->bge_cdata.bge_tx_dmamap[*txidx] =
	    sc->bge_cdata.bge_tx_dmamap[ctx.bge_idx];
	sc->bge_cdata.bge_tx_dmamap[ctx.bge_idx] = map;
	sc->bge_cdata.bge_tx_chain[ctx.bge_idx] = m_head;
	sc->bge_txcnt += ctx.bge_maxsegs;
	f = &sc->bge_ldata.bge_tx_ring[*txidx];
	if (mtag != NULL) {
		f->bge_flags |= htole16(BGE_TXBDFLAG_VLAN_TAG);
		f->bge_vlan_tag = htole16(VLAN_TAG_VALUE(mtag));
	} else {
		f->bge_vlan_tag = 0;
	}

	BGE_INC(ctx.bge_idx, BGE_TX_RING_CNT);
	*txidx = ctx.bge_idx;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void
bge_start_locked(ifp)
	struct ifnet *ifp;
{
	struct bge_softc *sc;
	struct mbuf *m_head = NULL;
	u_int32_t prodidx = 0;
	int count = 0;

	sc = ifp->if_softc;

	if (!sc->bge_link && ifp->if_snd.ifq_len < 10)
		return;

	prodidx = CSR_READ_4(sc, BGE_MBX_TX_HOST_PROD0_LO);

	while(sc->bge_cdata.bge_tx_chain[prodidx] == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * XXX
		 * The code inside the if() block is never reached since we
		 * must mark CSUM_IP_FRAGS in our if_hwassist to start getting
		 * requests to checksum TCP/UDP in a fragmented packet.
		 *
		 * XXX
		 * safety overkill.  If this is a fragmented packet chain
		 * with delayed TCP/UDP checksums, then only encapsulate
		 * it if we have enough descriptors to handle the entire
		 * chain at once.
		 * (paranoia -- may not actually be needed)
		 */
		if (m_head->m_flags & M_FIRSTFRAG &&
		    m_head->m_pkthdr.csum_flags & (CSUM_DELAY_DATA)) {
			if ((BGE_TX_RING_CNT - sc->bge_txcnt) <
			    m_head->m_pkthdr.csum_data + 16) {
				IF_PREPEND(&ifp->if_snd, m_head);
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
		}

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (bge_encap(sc, m_head, &prodidx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
		++count;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);
	}

	if (count == 0) {
		/* no packets were dequeued */
		return;
	}

	/* Transmit */
	CSR_WRITE_4(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);
	/* 5700 b2 errata */
	if (sc->bge_chiprev == BGE_CHIPREV_5700_BX)
		CSR_WRITE_4(sc, BGE_MBX_TX_HOST_PROD0_LO, prodidx);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void
bge_start(ifp)
	struct ifnet *ifp;
{
	struct bge_softc *sc;

	sc = ifp->if_softc;
	BGE_LOCK(sc);
	bge_start_locked(ifp);
	BGE_UNLOCK(sc);
}

static void
bge_init_locked(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	u_int16_t *m;

	BGE_LOCK_ASSERT(sc);

	ifp = &sc->arpcom.ac_if;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	/* Cancel pending I/O and flush buffers. */
	bge_stop(sc);
	bge_reset(sc);
	bge_chipinit(sc);

	/*
	 * Init the various state machines, ring
	 * control blocks and firmware.
	 */
	if (bge_blockinit(sc)) {
		printf("bge%d: initialization failure\n", sc->bge_unit);
		return;
	}

	ifp = &sc->arpcom.ac_if;

	/* Specify MTU. */
	CSR_WRITE_4(sc, BGE_RX_MTU, ifp->if_mtu +
	    ETHER_HDR_LEN + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN);

	/* Load our MAC address. */
	m = (u_int16_t *)&sc->arpcom.ac_enaddr[0];
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_LO, htons(m[0]));
	CSR_WRITE_4(sc, BGE_MAC_ADDR1_HI, (htons(m[1]) << 16) | htons(m[2]));

	/* Enable or disable promiscuous mode as needed. */
	if (ifp->if_flags & IFF_PROMISC) {
		BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
	} else {
		BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
	}

	/* Program multicast filter. */
	bge_setmulti(sc);

	/* Init RX ring. */
	bge_init_rx_ring_std(sc);

	/*
	 * Workaround for a bug in 5705 ASIC rev A0. Poll the NIC's
	 * memory to insure that the chip has in fact read the first
	 * entry of the ring.
	 */
	if (sc->bge_chipid == BGE_CHIPID_BCM5705_A0) {
		u_int32_t		v, i;
		for (i = 0; i < 10; i++) {
			DELAY(20);
			v = bge_readmem_ind(sc, BGE_STD_RX_RINGS + 8);
			if (v == (MCLBYTES - ETHER_ALIGN))
				break;
		}
		if (i == 10)
			printf ("bge%d: 5705 A0 chip failed to load RX ring\n",
			    sc->bge_unit);
	}

	/* Init jumbo RX ring. */
	if (ifp->if_mtu > (ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN))
		bge_init_rx_ring_jumbo(sc);

	/* Init our RX return ring index */
	sc->bge_rx_saved_considx = 0;

	/* Init TX ring. */
	bge_init_tx_ring(sc);

	/* Turn on transmitter */
	BGE_SETBIT(sc, BGE_TX_MODE, BGE_TXMODE_ENABLE);

	/* Turn on receiver */
	BGE_SETBIT(sc, BGE_RX_MODE, BGE_RXMODE_ENABLE);

	/* Tell firmware we're alive. */
	BGE_SETBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/* Enable host interrupts. */
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_CLEAR_INTA);
	BGE_CLRBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 0);

	bge_ifmedia_upd(ifp);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	callout_reset(&sc->bge_stat_ch, hz, bge_tick, sc);

	return;
}

static void
bge_init(xsc)
	void *xsc;
{
	struct bge_softc *sc = xsc;

	BGE_LOCK(sc);
	bge_init_locked(sc);
	BGE_UNLOCK(sc);

	return;
}

/*
 * Set media options.
 */
static int
bge_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	struct bge_softc *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;

	sc = ifp->if_softc;
	ifm = &sc->bge_ifmedia;

	/* If this is a 1000baseX NIC, enable the TBI port. */
	if (sc->bge_tbi) {
		if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
			return(EINVAL);
		switch(IFM_SUBTYPE(ifm->ifm_media)) {
		case IFM_AUTO:
#ifndef BGE_FAKE_AUTONEG
			/*
			 * The BCM5704 ASIC appears to have a special
			 * mechanism for programming the autoneg
			 * advertisement registers in TBI mode.
			 */
			if (sc->bge_asicrev == BGE_ASICREV_BCM5704) {
				uint32_t sgdig;
				CSR_WRITE_4(sc, BGE_TX_TBI_AUTONEG, 0);
				sgdig = CSR_READ_4(sc, BGE_SGDIG_CFG);
				sgdig |= BGE_SGDIGCFG_AUTO|
				    BGE_SGDIGCFG_PAUSE_CAP|
				    BGE_SGDIGCFG_ASYM_PAUSE;
				CSR_WRITE_4(sc, BGE_SGDIG_CFG,
				    sgdig|BGE_SGDIGCFG_SEND);
				DELAY(5);
				CSR_WRITE_4(sc, BGE_SGDIG_CFG, sgdig);
			}
#endif
			break;
		case IFM_1000_SX:
			if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX) {
				BGE_CLRBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			} else {
				BGE_SETBIT(sc, BGE_MAC_MODE,
				    BGE_MACMODE_HALF_DUPLEX);
			}
			break;
		default:
			return(EINVAL);
		}
		return(0);
	}

	mii = device_get_softc(sc->bge_miibus);
	sc->bge_link = 0;
	if (mii->mii_instance) {
		struct mii_softc *miisc;
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
static void
bge_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct bge_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;

	if (sc->bge_tbi) {
		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_active = IFM_ETHER;
		if (CSR_READ_4(sc, BGE_MAC_STS) &
		    BGE_MACSTAT_TBI_PCS_SYNCHED)
			ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= IFM_1000_SX;
		if (CSR_READ_4(sc, BGE_MAC_MODE) & BGE_MACMODE_HALF_DUPLEX)
			ifmr->ifm_active |= IFM_HDX;
		else
			ifmr->ifm_active |= IFM_FDX;
		return;
	}

	mii = device_get_softc(sc->bge_miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}

static int
bge_ioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct bge_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	int mask, error = 0;
	struct mii_data *mii;

	switch(command) {
	case SIOCSIFMTU:
		/* Disallow jumbo frames on 5705. */
		if (((sc->bge_asicrev == BGE_ASICREV_BCM5705 ||
		      sc->bge_asicrev == BGE_ASICREV_BCM5750) &&
		    ifr->ifr_mtu > ETHERMTU) || ifr->ifr_mtu > BGE_JUMBO_MTU)
			error = EINVAL;
		else {
			ifp->if_mtu = ifr->ifr_mtu;
			ifp->if_flags &= ~IFF_RUNNING;
			bge_init(sc);
		}
		break;
	case SIOCSIFFLAGS:
		BGE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			/*
			 * If only the state of the PROMISC flag changed,
			 * then just use the 'set promisc mode' command
			 * instead of reinitializing the entire NIC. Doing
			 * a full re-init means reloading the firmware and
			 * waiting for it to start up, which may take a
			 * second or two.
			 */
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->bge_if_flags & IFF_PROMISC)) {
				BGE_SETBIT(sc, BGE_RX_MODE,
				    BGE_RXMODE_RX_PROMISC);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->bge_if_flags & IFF_PROMISC) {
				BGE_CLRBIT(sc, BGE_RX_MODE,
				    BGE_RXMODE_RX_PROMISC);
			} else
				bge_init_locked(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				bge_stop(sc);
			}
		}
		sc->bge_if_flags = ifp->if_flags;
		BGE_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_RUNNING) {
			BGE_LOCK(sc);
			bge_setmulti(sc);
			BGE_UNLOCK(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (sc->bge_tbi) {
			error = ifmedia_ioctl(ifp, ifr,
			    &sc->bge_ifmedia, command);
		} else {
			mii = device_get_softc(sc->bge_miibus);
			error = ifmedia_ioctl(ifp, ifr,
			    &mii->mii_media, command);
		}
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		/* NB: the code for RX csum offload is disabled for now */
		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			if (IFCAP_TXCSUM & ifp->if_capenable)
				ifp->if_hwassist = BGE_CSUM_FEATURES;
			else
				ifp->if_hwassist = 0;
		}
		error = 0;
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return(error);
}

static void
bge_watchdog(ifp)
	struct ifnet *ifp;
{
	struct bge_softc *sc;

	sc = ifp->if_softc;

	printf("bge%d: watchdog timeout -- resetting\n", sc->bge_unit);

	ifp->if_flags &= ~IFF_RUNNING;
	bge_init(sc);

	ifp->if_oerrors++;

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void
bge_stop(sc)
	struct bge_softc *sc;
{
	struct ifnet *ifp;
	struct ifmedia_entry *ifm;
	struct mii_data *mii = NULL;
	int mtmp, itmp;

	BGE_LOCK_ASSERT(sc);

	ifp = &sc->arpcom.ac_if;

	if (!sc->bge_tbi)
		mii = device_get_softc(sc->bge_miibus);

	callout_stop(&sc->bge_stat_ch);

	/*
	 * Disable all of the receiver blocks
	 */
	BGE_CLRBIT(sc, BGE_RX_MODE, BGE_RXMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		BGE_CLRBIT(sc, BGE_RXLS_MODE, BGE_RXLSMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RDBDI_MODE, BGE_RBDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RDC_MODE, BGE_RDCMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RBDC_MODE, BGE_RBDCMODE_ENABLE);

	/*
	 * Disable all of the transmit blocks
	 */
	BGE_CLRBIT(sc, BGE_SRS_MODE, BGE_SRSMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SDI_MODE, BGE_SDIMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_RDMA_MODE, BGE_RDMAMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SDC_MODE, BGE_SDCMODE_ENABLE);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		BGE_CLRBIT(sc, BGE_DMAC_MODE, BGE_DMACMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	/*
	 * Shut down all of the memory managers and related
	 * state machines.
	 */
	BGE_CLRBIT(sc, BGE_HCC_MODE, BGE_HCCMODE_ENABLE);
	BGE_CLRBIT(sc, BGE_WDMA_MODE, BGE_WDMAMODE_ENABLE);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		BGE_CLRBIT(sc, BGE_MBCF_MODE, BGE_MBCFMODE_ENABLE);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0xFFFFFFFF);
	CSR_WRITE_4(sc, BGE_FTQ_RESET, 0);
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750) {
		BGE_CLRBIT(sc, BGE_BMAN_MODE, BGE_BMANMODE_ENABLE);
		BGE_CLRBIT(sc, BGE_MARB_MODE, BGE_MARBMODE_ENABLE);
	}

	/* Disable host interrupts. */
	BGE_SETBIT(sc, BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	CSR_WRITE_4(sc, BGE_MBX_IRQ0_LO, 1);

	/*
	 * Tell firmware we're shutting down.
	 */
	BGE_CLRBIT(sc, BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/* Free the RX lists. */
	bge_free_rx_ring_std(sc);

	/* Free jumbo RX list. */
	if (sc->bge_asicrev != BGE_ASICREV_BCM5705 &&
	    sc->bge_asicrev != BGE_ASICREV_BCM5750)
		bge_free_rx_ring_jumbo(sc);

	/* Free TX buffers. */
	bge_free_tx_ring(sc);

	/*
	 * Isolate/power down the PHY, but leave the media selection
	 * unchanged so that things will be put back to normal when
	 * we bring the interface back up.
	 */
	if (!sc->bge_tbi) {
		itmp = ifp->if_flags;
		ifp->if_flags |= IFF_UP;
		ifm = mii->mii_media.ifm_cur;
		mtmp = ifm->ifm_media;
		ifm->ifm_media = IFM_ETHER|IFM_NONE;
		mii_mediachg(mii);
		ifm->ifm_media = mtmp;
		ifp->if_flags = itmp;
	}

	sc->bge_link = 0;

	sc->bge_tx_saved_considx = BGE_TXCONS_UNSET;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
bge_shutdown(dev)
	device_t dev;
{
	struct bge_softc *sc;

	sc = device_get_softc(dev);

	BGE_LOCK(sc);
	bge_stop(sc);
	bge_reset(sc);
	BGE_UNLOCK(sc);

	return;
}
