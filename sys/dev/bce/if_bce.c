/*-
 * Copyright (c) 2006 Broadcom Corporation
 *	David Christensen <davidch@broadcom.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Broadcom Corporation nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
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
 * The following controllers are supported by this driver:
 *   BCM5706C A2, A3
 *   BCM5708C B1
 *
 * The following controllers are not supported by this driver:
 * (These are not "Production" versions of the controller.)
 * 
 *   BCM5706C A0, A1
 *   BCM5706S A0, A1, A2, A3
 *   BCM5708C A0, B0
 *   BCM5708S A0, B0, B1
 */

#include "opt_bce.h"

#include <dev/bce/if_bcereg.h>
#include <dev/bce/if_bcefw.h>

/* 4.x compat */
#define BPF_MTAP(_ifp,_m) do {					\
			if ((_ifp)->if_bpf)			\
				bpf_mtap((_ifp), (_m));		\
			} while (0)

#define ETHER_ALIGN                     2
#define	ETHER_VLAN_ENCAP_LEN	4	/* len of 802.1Q VLAN encapsulation */

#define	IFQ_DRV_IS_EMPTY(q)	((q)->ifq_head == NULL)

#define	IF_Kbps(x)	((x) * 1000)		/* kilobits/sec. */
#define	IF_Mbps(x)	(IF_Kbps((x) * 1000))	/* megabits/sec. */
#define	IF_Gbps(x)	(IF_Mbps((x) * 1000))	/* gigabits/sec. */

/****************************************************************************/
/* BCE Driver Version                                                       */
/****************************************************************************/
char bce_driver_version[] = "v0.9.5";


/****************************************************************************/
/* BCE Debug Options                                                        */
/****************************************************************************/
#ifdef BCE_DEBUG
	u32 bce_debug = BCE_WARN;

	/*          0 = Never              */
	/*          1 = 1 in 2,147,483,648 */
	/*        256 = 1 in     8,388,608 */
	/*       2048 = 1 in     1,048,576 */
	/*      65536 = 1 in        32,768 */
	/*    1048576 = 1 in         2,048 */
	/*  268435456 =	1 in             8 */
	/*  536870912 = 1 in             4 */
	/* 1073741824 = 1 in             2 */

	/* Controls how often the l2_fhdr frame error check will fail. */
	int bce_debug_l2fhdr_status_check = 0;

	/* Controls how often the unexpected attention check will fail. */
	int bce_debug_unexpected_attention = 0;

	/* Controls how often to simulate an mbuf allocation failure. */
	int bce_debug_mbuf_allocation_failure = 0;

	/* Controls how often to simulate a DMA mapping failure. */
	int bce_debug_dma_map_addr_failure = 0;

	/* Controls how often to simulate a bootcode failure. */
	int bce_debug_bootcode_running_failure = 0;
#endif


/****************************************************************************/
/* PCI Device ID Table                                                      */
/*                                                                          */
/* Used by bce_probe() to identify the devices supported by this driver.    */
/****************************************************************************/
#define BCE_DEVDESC_MAX		64

static struct bce_type bce_devs[] = {
	/* BCM5706C Controllers and OEM boards. */
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM5706,  HP_VENDORID, 0x3101,
		"HP NC370T Multifunction Gigabit Server Adapter" },
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM5706,  HP_VENDORID, 0x3106,
		"HP NC370i Multifunction Gigabit Server Adapter" },
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM5706,  PCI_ANY_ID,  PCI_ANY_ID,
		"Broadcom NetXtreme II BCM5706 1000Base-T" },

	/* BCM5706S controllers and OEM boards. */
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM5706S, HP_VENDORID, 0x3102,
		"HP NC370F Multifunction Gigabit Server Adapter" },
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM5706S, PCI_ANY_ID,  PCI_ANY_ID,
		"Broadcom NetXtreme II BCM5706 1000Base-SX" },

	/* BCM5708C controllers and OEM boards. */
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM5708,  PCI_ANY_ID,  PCI_ANY_ID,
		"Broadcom NetXtreme II BCM5708 1000Base-T" },

	/* BCM5708S controllers and OEM boards. */
	{ BRCM_VENDORID, BRCM_DEVICEID_BCM5708,  PCI_ANY_ID,  PCI_ANY_ID,
		"Broadcom NetXtreme II BCM5708 1000Base-T" },
	{ 0, 0, 0, 0, NULL }
};


/****************************************************************************/
/* Supported Flash NVRAM device data.                                       */
/****************************************************************************/
static struct flash_spec flash_table[] =
{
	/* Slow EEPROM */
	{0x00000000, 0x40830380, 0x009f0081, 0xa184a053, 0xaf000400,
	 1, SEEPROM_PAGE_BITS, SEEPROM_PAGE_SIZE,
	 SEEPROM_BYTE_ADDR_MASK, SEEPROM_TOTAL_SIZE,
	 "EEPROM - slow"},
	/* Expansion entry 0001 */
	{0x08000002, 0x4b808201, 0x00050081, 0x03840253, 0xaf020406,
	 0, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 0001"},
	/* Saifun SA25F010 (non-buffered flash) */
	/* strap, cfg1, & write1 need updates */
	{0x04000001, 0x47808201, 0x00050081, 0x03840253, 0xaf020406,
	 0, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAL_SIZE*2,
	 "Non-buffered flash (128kB)"},
	/* Saifun SA25F020 (non-buffered flash) */
	/* strap, cfg1, & write1 need updates */
	{0x0c000003, 0x4f808201, 0x00050081, 0x03840253, 0xaf020406,
	 0, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAL_SIZE*4,
	 "Non-buffered flash (256kB)"},
	/* Expansion entry 0100 */
	{0x11000000, 0x53808201, 0x00050081, 0x03840253, 0xaf020406,
	 0, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 0100"},
	/* Entry 0101: ST M45PE10 (non-buffered flash, TetonII B0) */
	{0x19000002, 0x5b808201, 0x000500db, 0x03840253, 0xaf020406,
	 0, ST_MICRO_FLASH_PAGE_BITS, ST_MICRO_FLASH_PAGE_SIZE,
	 ST_MICRO_FLASH_BYTE_ADDR_MASK, ST_MICRO_FLASH_BASE_TOTAL_SIZE*2,
	 "Entry 0101: ST M45PE10 (128kB non-bufferred)"},
	/* Entry 0110: ST M45PE20 (non-buffered flash)*/
	{0x15000001, 0x57808201, 0x000500db, 0x03840253, 0xaf020406,
	 0, ST_MICRO_FLASH_PAGE_BITS, ST_MICRO_FLASH_PAGE_SIZE,
	 ST_MICRO_FLASH_BYTE_ADDR_MASK, ST_MICRO_FLASH_BASE_TOTAL_SIZE*4,
	 "Entry 0110: ST M45PE20 (256kB non-bufferred)"},
	/* Saifun SA25F005 (non-buffered flash) */
	/* strap, cfg1, & write1 need updates */
	{0x1d000003, 0x5f808201, 0x00050081, 0x03840253, 0xaf020406,
	 0, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, SAIFUN_FLASH_BASE_TOTAL_SIZE,
	 "Non-buffered flash (64kB)"},
	/* Fast EEPROM */
	{0x22000000, 0x62808380, 0x009f0081, 0xa184a053, 0xaf000400,
	 1, SEEPROM_PAGE_BITS, SEEPROM_PAGE_SIZE,
	 SEEPROM_BYTE_ADDR_MASK, SEEPROM_TOTAL_SIZE,
	 "EEPROM - fast"},
	/* Expansion entry 1001 */
	{0x2a000002, 0x6b808201, 0x00050081, 0x03840253, 0xaf020406,
	 0, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1001"},
	/* Expansion entry 1010 */
	{0x26000001, 0x67808201, 0x00050081, 0x03840253, 0xaf020406,
	 0, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1010"},
	/* ATMEL AT45DB011B (buffered flash) */
	{0x2e000003, 0x6e808273, 0x00570081, 0x68848353, 0xaf000400,
	 1, BUFFERED_FLASH_PAGE_BITS, BUFFERED_FLASH_PAGE_SIZE,
	 BUFFERED_FLASH_BYTE_ADDR_MASK, BUFFERED_FLASH_TOTAL_SIZE,
	 "Buffered flash (128kB)"},
	/* Expansion entry 1100 */
	{0x33000000, 0x73808201, 0x00050081, 0x03840253, 0xaf020406,
	 0, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1100"},
	/* Expansion entry 1101 */
	{0x3b000002, 0x7b808201, 0x00050081, 0x03840253, 0xaf020406,
	 0, SAIFUN_FLASH_PAGE_BITS, SAIFUN_FLASH_PAGE_SIZE,
	 SAIFUN_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1101"},
	/* Ateml Expansion entry 1110 */
	{0x37000001, 0x76808273, 0x00570081, 0x68848353, 0xaf000400,
	 1, BUFFERED_FLASH_PAGE_BITS, BUFFERED_FLASH_PAGE_SIZE,
	 BUFFERED_FLASH_BYTE_ADDR_MASK, 0,
	 "Entry 1110 (Atmel)"},
	/* ATMEL AT45DB021B (buffered flash) */
	{0x3f000003, 0x7e808273, 0x00570081, 0x68848353, 0xaf000400,
	 1, BUFFERED_FLASH_PAGE_BITS, BUFFERED_FLASH_PAGE_SIZE,
	 BUFFERED_FLASH_BYTE_ADDR_MASK, BUFFERED_FLASH_TOTAL_SIZE*2,
	 "Buffered flash (256kB)"},
};


/****************************************************************************/
/* FreeBSD device entry points.                                             */
/****************************************************************************/
static int  bce_probe				(device_t);
static int  bce_attach				(device_t);
static int  bce_detach				(device_t);
static void bce_shutdown			(device_t);


/****************************************************************************/
/* BCE Debug Data Structure Dump Routines                                   */
/****************************************************************************/
#ifdef BCE_DEBUG
static void bce_dump_mbuf 			(struct bce_softc *, struct mbuf *);
static void bce_dump_tx_mbuf_chain	(struct bce_softc *, int, int);
static void bce_dump_rx_mbuf_chain	(struct bce_softc *, int, int);
static void bce_dump_txbd			(struct bce_softc *, int, struct tx_bd *);
static void bce_dump_rxbd			(struct bce_softc *, int, struct rx_bd *);
static void bce_dump_l2fhdr			(struct bce_softc *, int, struct l2_fhdr *);
static void bce_dump_tx_chain		(struct bce_softc *, int, int);
static void bce_dump_rx_chain		(struct bce_softc *, int, int);
static void bce_dump_status_block	(struct bce_softc *);
static void bce_dump_stats_block	(struct bce_softc *);
static void bce_dump_driver_state	(struct bce_softc *);
static void bce_dump_hw_state		(struct bce_softc *);
static void bce_breakpoint			(struct bce_softc *);
#endif


/****************************************************************************/
/* BCE Register/Memory Access Routines                                      */
/****************************************************************************/
static u32  bce_reg_rd_ind			(struct bce_softc *, u32);
static void bce_reg_wr_ind			(struct bce_softc *, u32, u32);
static void bce_ctx_wr				(struct bce_softc *, u32, u32, u32);
static int  bce_miibus_read_reg		(device_t, int, int);
static int  bce_miibus_write_reg	(device_t, int, int, int);
static void bce_miibus_statchg		(device_t);


/****************************************************************************/
/* BCE NVRAM Access Routines                                                */
/****************************************************************************/
static int  bce_acquire_nvram_lock	(struct bce_softc *);
static int  bce_release_nvram_lock	(struct bce_softc *);
static void bce_enable_nvram_access	(struct bce_softc *);
static void	bce_disable_nvram_access(struct bce_softc *);
static int  bce_nvram_read_dword	(struct bce_softc *, u32, u8 *, u32);
static int  bce_init_nvram			(struct bce_softc *);
static int  bce_nvram_read			(struct bce_softc *, u32, u8 *, int);
static int  bce_nvram_test			(struct bce_softc *);
#ifdef BCE_NVRAM_WRITE_SUPPORT
static int  bce_enable_nvram_write	(struct bce_softc *);
static void bce_disable_nvram_write	(struct bce_softc *);
static int  bce_nvram_erase_page	(struct bce_softc *, u32);
static int  bce_nvram_write_dword	(struct bce_softc *, u32, u8 *, u32);
static int  bce_nvram_write			(struct bce_softc *, u32, u8 *, int);
#endif

/****************************************************************************/
/*                                                                          */
/****************************************************************************/
static void bce_dma_map_addr		(void *, bus_dma_segment_t *, int, int);
static void bce_dma_map_rx_desc		(void *, bus_dma_segment_t *, int, bus_size_t, int);
static void bce_dma_map_tx_desc		(void *, bus_dma_segment_t *, int, bus_size_t, int);
static int  bce_dma_alloc			(device_t);
static void bce_dma_free			(struct bce_softc *);
static void bce_release_resources	(struct bce_softc *);

/****************************************************************************/
/* BCE Firmware Synchronization and Load                                    */
/****************************************************************************/
static int  bce_fw_sync				(struct bce_softc *, u32);
static void bce_load_rv2p_fw		(struct bce_softc *, u32 *, u32, u32);
static void bce_load_cpu_fw			(struct bce_softc *, struct cpu_reg *, struct fw_info *);
static void bce_init_cpus			(struct bce_softc *);

static void bce_stop				(struct bce_softc *);
static int  bce_reset				(struct bce_softc *, u32);
static int  bce_chipinit 			(struct bce_softc *);
static int  bce_blockinit 			(struct bce_softc *);
static int  bce_get_buf				(struct bce_softc *, struct mbuf *, u16 *, u16 *, u32 *);

static int  bce_init_tx_chain		(struct bce_softc *);
static int  bce_init_rx_chain		(struct bce_softc *);
static void bce_free_rx_chain		(struct bce_softc *);
static void bce_free_tx_chain		(struct bce_softc *);

static int  bce_tx_encap			(struct bce_softc *, struct mbuf *, u16 *, u16 *, u32 *);
static void bce_start				(struct ifnet *);
static int  bce_ioctl				(struct ifnet *, u_long, caddr_t);
static void bce_watchdog			(struct ifnet *);
static int  bce_ifmedia_upd			(struct ifnet *);
static void bce_ifmedia_sts			(struct ifnet *, struct ifmediareq *);
static void bce_init				(void *);

static void bce_init_context		(struct bce_softc *);
static void bce_get_mac_addr		(struct bce_softc *);
static void bce_set_mac_addr		(struct bce_softc *);
static void bce_phy_intr			(struct bce_softc *);
static void bce_rx_intr				(struct bce_softc *);
static void bce_tx_intr				(struct bce_softc *);
static void bce_disable_intr		(struct bce_softc *);
static void bce_enable_intr			(struct bce_softc *);

#ifdef DEVICE_POLLING
static void bce_poll				(struct ifnet *, enum poll_cmd, int);
#endif
static void bce_intr				(void *);
static void bce_set_rx_mode			(struct bce_softc *);
static void bce_stats_update		(struct bce_softc *);
static void bce_tick				(void *);
static void bce_add_sysctls			(struct bce_softc *);
static void bce_remove_sysctls			(struct bce_softc *);


/****************************************************************************/
/* FreeBSD device dispatch table.                                           */
/****************************************************************************/
static device_method_t bce_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bce_probe),
	DEVMETHOD(device_attach,	bce_attach),
	DEVMETHOD(device_detach,	bce_detach),
	DEVMETHOD(device_shutdown,	bce_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	bce_miibus_read_reg),
	DEVMETHOD(miibus_writereg,	bce_miibus_write_reg),
	DEVMETHOD(miibus_statchg,	bce_miibus_statchg),

	{ 0, 0 }
};

static driver_t bce_driver = {
	"bce",
	bce_methods,
	sizeof(struct bce_softc)
};

static devclass_t bce_devclass;

MODULE_DEPEND(bce, pci, 1, 1, 1);
MODULE_DEPEND(bce, ether, 1, 1, 1);
MODULE_DEPEND(bce, miibus, 1, 1, 1);

DRIVER_MODULE(bce, pci, bce_driver, bce_devclass, 0, 0);
DRIVER_MODULE(miibus, bce, miibus_driver, miibus_devclass, 0, 0);

static uint32_t
ether_crc32_le(const uint8_t *buf, size_t len)
{
	static const uint32_t crctab[] = {
		0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};
	size_t i;
	uint32_t crc;

	crc = 0xffffffff;	/* initial value */

	for (i = 0; i < len; i++) {
		crc ^= buf[i];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
		crc = (crc >> 4) ^ crctab[crc & 0xf];
	}

	return (crc);
}

/****************************************************************************/
/* Device probe function.                                                   */
/*                                                                          */
/* Compares the device to the driver's list of supported devices and        */
/* reports back to the OS whether this is the right driver for the device.  */
/*                                                                          */
/* Returns:                                                                 */
/*   BUS_PROBE_DEFAULT on success, positive value on failure.               */
/****************************************************************************/
static int
bce_probe(device_t dev)
{
	struct bce_type *t;
	struct bce_softc *sc;
	char *descbuf;
	u16 vid = 0, did = 0, svid = 0, sdid = 0;

	t = bce_devs;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(struct bce_softc));
	sc->bce_unit = device_get_unit(dev);
	sc->bce_dev = dev;

	/* Get the data for the device to be probed. */
	vid  = pci_get_vendor(dev);
	did  = pci_get_device(dev);
	svid = pci_get_subvendor(dev);
	sdid = pci_get_subdevice(dev);

	DBPRINT(sc, BCE_VERBOSE_LOAD, 
		"%s(); VID = 0x%04X, DID = 0x%04X, SVID = 0x%04X, "
		"SDID = 0x%04X\n", __FUNCTION__, vid, did, svid, sdid);

	/* Look through the list of known devices for a match. */
	while(t->bce_name != NULL) {

		if ((vid == t->bce_vid) && (did == t->bce_did) && 
			((svid == t->bce_svid) || (t->bce_svid == PCI_ANY_ID)) &&
			((sdid == t->bce_sdid) || (t->bce_sdid == PCI_ANY_ID))) {

			descbuf = malloc(BCE_DEVDESC_MAX, M_TEMP, M_NOWAIT);

			if (descbuf == NULL)
				return(ENOMEM);

			/* Print out the device identity. */
			snprintf(descbuf, BCE_DEVDESC_MAX, "%s (%c%d), %s", 
				t->bce_name,
			    (((pci_read_config(dev, PCIR_REVID, 4) & 0xf0) >> 4) + 'A'),
			    (pci_read_config(dev, PCIR_REVID, 4) & 0xf),
			    bce_driver_version);

			device_set_desc_copy(dev, descbuf);
			free(descbuf, M_TEMP);
			return(0);
		}
		t++;
	}

	DBPRINT(sc, BCE_VERBOSE_LOAD, "%s(%d): No IOCTL match found!\n", 
		__FILE__, __LINE__);

	return(ENXIO);
}


/****************************************************************************/
/* Device attach function.                                                  */
/*                                                                          */
/* Allocates device resources, performs secondary chip identification,      */
/* resets and initializes the hardware, and initializes driver instance     */
/* variables.                                                               */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_attach(device_t dev)
{
	struct bce_softc *sc;
	struct ifnet *ifp;
	u32 val;
	int mbuf, rid, rc = 0, s;

	s = splimp();

	sc = device_get_softc(dev);
	sc->bce_dev = dev;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	mbuf = device_get_unit(dev);
	sc->bce_unit = mbuf;

	pci_enable_busmaster(dev);

	/* Allocate PCI memory resources. */
	rid = PCIR_BAR(0);
	sc->bce_res = bus_alloc_resource_any(
		dev, 							/* dev */
		SYS_RES_MEMORY, 				/* type */
		&rid,							/* rid */
	    RF_ACTIVE | PCI_RF_DENSE);		/* flags */

	if (sc->bce_res == NULL) {
		BCE_PRINTF(sc, "%s(%d): PCI memory allocation failed\n", 
			__FILE__, __LINE__);
		rc = ENXIO;
		goto bce_attach_fail;
	}

	/* Get various resource handles. */
	sc->bce_btag    = rman_get_bustag(sc->bce_res);
	sc->bce_bhandle = rman_get_bushandle(sc->bce_res);
	sc->bce_vhandle = (vm_offset_t) rman_get_virtual(sc->bce_res);

	/* Allocate PCI IRQ resources. */
	rid = 0;
	sc->bce_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->bce_irq == NULL) {
		BCE_PRINTF(sc, "%s(%d): PCI map interrupt failed\n", 
			__FILE__, __LINE__);
		rc = ENXIO;
		goto bce_attach_fail;
	}

	/*
	 * Configure byte swap and enable indirect register access.
	 * Rely on CPU to do target byte swapping on big endian systems.
	 * Access to registers outside of PCI configurtion space are not
	 * valid until this is done.
	 */
	pci_write_config(dev, BCE_PCICFG_MISC_CONFIG,
			       BCE_PCICFG_MISC_CONFIG_REG_WINDOW_ENA |
			       BCE_PCICFG_MISC_CONFIG_TARGET_MB_WORD_SWAP, 4);

	/* Save ASIC revsion info. */
	sc->bce_chipid =  REG_RD(sc, BCE_MISC_ID);

	/* Weed out any non-production controller revisions. */
	switch(BCE_CHIP_ID(sc)) {
		case BCE_CHIP_ID_5706_A0:
		case BCE_CHIP_ID_5706_A1:
		case BCE_CHIP_ID_5708_A0:
		case BCE_CHIP_ID_5708_B0:
			BCE_PRINTF(sc, "%s(%d): Unsupported controller revision (%c%d)!\n",
				__FILE__, __LINE__, 
				(((pci_read_config(dev, PCIR_REVID, 4) & 0xf0) >> 4) + 'A'),
			    (pci_read_config(dev, PCIR_REVID, 4) & 0xf));
			rc = ENODEV;
			goto bce_attach_fail;
	}

	if (BCE_CHIP_BOND_ID(sc) & BCE_CHIP_BOND_ID_SERDES_BIT) {
		BCE_PRINTF(sc, "%s(%d): SerDes controllers are not supported!\n",
			__FILE__, __LINE__);
		rc = ENODEV;
		goto bce_attach_fail;
	}

	/* 
	 * The embedded PCIe to PCI-X bridge (EPB) 
	 * in the 5708 cannot address memory above 
	 * 40 bits (E7_5708CB1_23043 & E6_5708SB1_23043). 
	 */
	if (BCE_CHIP_NUM(sc) == BCE_CHIP_NUM_5708)
		sc->max_bus_addr = BCE_BUS_SPACE_MAXADDR;
	else
		sc->max_bus_addr = BUS_SPACE_MAXADDR;

	/*
	 * Find the base address for shared memory access.
	 * Newer versions of bootcode use a signature and offset
	 * while older versions use a fixed address.
	 */
	val = REG_RD_IND(sc, BCE_SHM_HDR_SIGNATURE);
	if ((val & BCE_SHM_HDR_SIGNATURE_SIG_MASK) == BCE_SHM_HDR_SIGNATURE_SIG)
		sc->bce_shmem_base = REG_RD_IND(sc, BCE_SHM_HDR_ADDR_0);
	else
		sc->bce_shmem_base = HOST_VIEW_SHMEM_BASE;

	DBPRINT(sc, BCE_INFO, "bce_shmem_base = 0x%08X\n", sc->bce_shmem_base);

	/* Set initial device and PHY flags */
	sc->bce_flags = 0;
	sc->bce_phy_flags = 0;

	/* Get PCI bus information (speed and type). */
	val = REG_RD(sc, BCE_PCICFG_MISC_STATUS);
	if (val & BCE_PCICFG_MISC_STATUS_PCIX_DET) {
		u32 clkreg;

		sc->bce_flags |= BCE_PCIX_FLAG;

		clkreg = REG_RD(sc, BCE_PCICFG_PCI_CLOCK_CONTROL_BITS);

		clkreg &= BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET;
		switch (clkreg) {
		case BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_133MHZ:
			sc->bus_speed_mhz = 133;
			break;

		case BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_95MHZ:
			sc->bus_speed_mhz = 100;
			break;

		case BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_66MHZ:
		case BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_80MHZ:
			sc->bus_speed_mhz = 66;
			break;

		case BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_48MHZ:
		case BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_55MHZ:
			sc->bus_speed_mhz = 50;
			break;

		case BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_LOW:
		case BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_32MHZ:
		case BCE_PCICFG_PCI_CLOCK_CONTROL_BITS_PCI_CLK_SPD_DET_38MHZ:
			sc->bus_speed_mhz = 33;
			break;
		}
	} else {
		if (val & BCE_PCICFG_MISC_STATUS_M66EN)
			sc->bus_speed_mhz = 66;
		else
			sc->bus_speed_mhz = 33;
	}

	if (val & BCE_PCICFG_MISC_STATUS_32BIT_DET)
		sc->bce_flags |= BCE_PCI_32BIT_FLAG;

	BCE_PRINTF(sc, "ASIC ID 0x%08X; Revision (%c%d); PCI%s %s %dMHz\n",
		sc->bce_chipid,
		((BCE_CHIP_ID(sc) & 0xf000) >> 12) + 'A',
		((BCE_CHIP_ID(sc) & 0x0ff0) >> 4),
		((sc->bce_flags & BCE_PCIX_FLAG) ? "-X" : ""),
		((sc->bce_flags & BCE_PCI_32BIT_FLAG) ? "32-bit" : "64-bit"),
		sc->bus_speed_mhz);

	/* Reset the controller. */
	if (bce_reset(sc, BCE_DRV_MSG_CODE_RESET)) {
		rc = ENXIO;
		goto bce_attach_fail;
	}

	/* Initialize the controller. */
	if (bce_chipinit(sc)) {
		BCE_PRINTF(sc, "%s(%d): Controller initialization failed!\n",
			__FILE__, __LINE__);
		rc = ENXIO;
		goto bce_attach_fail;
	}

	/* Perform NVRAM test. */
	if (bce_nvram_test(sc)) {
		BCE_PRINTF(sc, "%s(%d): NVRAM test failed!\n",
			__FILE__, __LINE__);
		rc = ENXIO;
		goto bce_attach_fail;
	}

	/* Fetch the permanent Ethernet MAC address. */
	bce_get_mac_addr(sc);
	device_printf(dev, "Ethernet address: %6D\n", sc->arpcom.ac_enaddr,
	    ":");

	/*
	 * Trip points control how many BDs
	 * should be ready before generating an
	 * interrupt while ticks control how long
	 * a BD can sit in the chain before
	 * generating an interrupt.  Set the default 
	 * values for the RX and TX rings.
	 */

#ifdef BCE_DRBUG
	/* Force more frequent interrupts. */
	sc->bce_tx_quick_cons_trip_int = 1;
	sc->bce_tx_quick_cons_trip     = 1;
	sc->bce_tx_ticks_int           = 0;
	sc->bce_tx_ticks               = 0;

	sc->bce_rx_quick_cons_trip_int = 1;
	sc->bce_rx_quick_cons_trip     = 1;
	sc->bce_rx_ticks_int           = 0;
	sc->bce_rx_ticks               = 0;
#else
	sc->bce_tx_quick_cons_trip_int = 20;
	sc->bce_tx_quick_cons_trip     = 20;
	sc->bce_tx_ticks_int           = 80;
	sc->bce_tx_ticks               = 80;

	sc->bce_rx_quick_cons_trip_int = 6;
	sc->bce_rx_quick_cons_trip     = 6;
	sc->bce_rx_ticks_int           = 18;
	sc->bce_rx_ticks               = 18;
#endif

	/* Update statistics once every second. */
	sc->bce_stats_ticks = 1000000 & 0xffff00;

	/*
	 * The copper based NetXtreme II controllers
	 * use an integrated PHY at address 1 while
	 * the SerDes controllers use a PHY at
	 * address 2.
	 */
	sc->bce_phy_addr = 1;

	if (BCE_CHIP_BOND_ID(sc) & BCE_CHIP_BOND_ID_SERDES_BIT) {
		sc->bce_phy_flags |= BCE_PHY_SERDES_FLAG;
		sc->bce_flags |= BCE_NO_WOL_FLAG;
		if (BCE_CHIP_NUM(sc) == BCE_CHIP_NUM_5708) {
			sc->bce_phy_addr = 2;
			val = REG_RD_IND(sc, sc->bce_shmem_base +
					 BCE_SHARED_HW_CFG_CONFIG);
			if (val & BCE_SHARED_HW_CFG_PHY_2_5G)
				sc->bce_phy_flags |= BCE_PHY_2_5G_CAPABLE_FLAG;
		}
	}

	/* Allocate DMA memory resources. */
	if (bce_dma_alloc(dev)) {
		BCE_PRINTF(sc, "%s(%d): DMA resource allocation failed!\n",
		    __FILE__, __LINE__);
		rc = ENXIO;
		goto bce_attach_fail;
	}

	/* Initialize the ifnet interface. */
	ifp = &sc->arpcom.ac_if;
	ifp->if_softc        = sc;
	ifp->if_unit	     = device_get_unit(dev);
	ifp->if_name	     = "bce";
	ifp->if_flags        = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl        = bce_ioctl;
	ifp->if_output	     = ether_output;
	ifp->if_start        = bce_start;
	ifp->if_timer        = 0;
	ifp->if_watchdog     = bce_watchdog;
	ifp->if_init         = bce_init;
	ifp->if_mtu          = ETHERMTU;
	ifp->if_hwassist     = BCE_IF_HWASSIST;
	ifp->if_capabilities = BCE_IF_CAPABILITIES;
	ifp->if_capenable    = ifp->if_capabilities;

	/* Assume a standard 1500 byte MTU size for mbuf allocations. */
	sc->mbuf_alloc_size  = MCLBYTES;
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	ifp->if_snd.ifq_maxlen = USABLE_TX_BD;
	if (sc->bce_phy_flags & BCE_PHY_2_5G_CAPABLE_FLAG)
		ifp->if_baudrate = IF_Gbps(2.5);
	else
		ifp->if_baudrate = IF_Gbps(1);

	if (sc->bce_phy_flags & BCE_PHY_SERDES_FLAG) {
		BCE_PRINTF(sc, "%s(%d): SerDes is not supported by this driver!\n", 
			__FILE__, __LINE__);
		rc = ENODEV;
		goto bce_attach_fail;
	} else {
		/* Look for our PHY. */
		if (mii_phy_probe(dev, &sc->bce_miibus, bce_ifmedia_upd,
			bce_ifmedia_sts)) {
			BCE_PRINTF(sc, "%s(%d): PHY probe failed!\n", 
				__FILE__, __LINE__);
			rc = ENXIO;
			goto bce_attach_fail;
		}
	}

	/* Attach to the Ethernet interface list. */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);

	/* Tell the upper layers we support long frames. */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

	callout_init(&sc->bce_stat_ch);

	/* Hookup IRQ last. */
	rc = bus_setup_intr(dev, sc->bce_irq, INTR_TYPE_NET,
	   bce_intr, sc, &sc->bce_intrhand);

	if (rc) {
		BCE_PRINTF(sc, "%s(%d): Failed to setup IRQ!\n", 
			__FILE__, __LINE__);
		bce_detach(dev);
		goto bce_attach_exit;
	}

	/* Print some important debugging info. */
	DBRUN(BCE_INFO, bce_dump_driver_state(sc));

	/* Add the supported sysctls to the kernel. */
	bce_add_sysctls(sc);

	goto bce_attach_exit;

bce_attach_fail:
	bce_release_resources(sc);

bce_attach_exit:

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	splx(s);

	return(rc);
}


/****************************************************************************/
/* Device detach function.                                                  */
/*                                                                          */
/* Stops the controller, resets the controller, and releases resources.     */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_detach(device_t dev)
{
	struct bce_softc *sc;
	struct ifnet *ifp;
	int s;

	sc = device_get_softc(dev);

	s = splimp();

	bce_remove_sysctls(sc);

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	ifp = &sc->arpcom.ac_if;

	/* Stop and reset the controller. */
	bce_stop(sc);
	bce_reset(sc, BCE_DRV_MSG_CODE_RESET);

	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);

	/* If we have a child device on the MII bus remove it too. */
	if (sc->bce_phy_flags & BCE_PHY_SERDES_FLAG) {
		ifmedia_removeall(&sc->bce_ifmedia);
	} else {
		bus_generic_detach(dev);
		device_delete_child(dev, sc->bce_miibus);
	}

	/* Release all remaining resources. */
	bce_release_resources(sc);

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	splx(s);

	return(0);
}


/****************************************************************************/
/* Device shutdown function.                                                */
/*                                                                          */
/* Stops and resets the controller.                                         */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing                                                                */
/****************************************************************************/
static void
bce_shutdown(device_t dev)
{
	struct bce_softc *sc = device_get_softc(dev);

	bce_stop(sc);
	bce_reset(sc, BCE_DRV_MSG_CODE_RESET);
}


/****************************************************************************/
/* Indirect register read.                                                  */
/*                                                                          */
/* Reads NetXtreme II registers using an index/data register pair in PCI    */
/* configuration space.  Using this mechanism avoids issues with posted     */
/* reads but is much slower than memory-mapped I/O.                         */
/*                                                                          */
/* Returns:                                                                 */
/*   The value of the register.                                             */
/****************************************************************************/
static u32
bce_reg_rd_ind(struct bce_softc *sc, u32 offset)
{
	device_t dev;
	dev = sc->bce_dev;

	pci_write_config(dev, BCE_PCICFG_REG_WINDOW_ADDRESS, offset, 4);
#ifdef BCE_DEBUG
	{
		u32 val;
		val = pci_read_config(dev, BCE_PCICFG_REG_WINDOW, 4);
		DBPRINT(sc, BCE_EXCESSIVE, "%s(); offset = 0x%08X, val = 0x%08X\n",
			__FUNCTION__, offset, val);
		return val;
	}
#else
	return pci_read_config(dev, BCE_PCICFG_REG_WINDOW, 4);
#endif
}


/****************************************************************************/
/* Indirect register write.                                                 */
/*                                                                          */
/* Writes NetXtreme II registers using an index/data register pair in PCI   */
/* configuration space.  Using this mechanism avoids issues with posted     */
/* writes but is muchh slower than memory-mapped I/O.                       */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_reg_wr_ind(struct bce_softc *sc, u32 offset, u32 val)
{
	device_t dev;
	dev = sc->bce_dev;

	DBPRINT(sc, BCE_EXCESSIVE, "%s(); offset = 0x%08X, val = 0x%08X\n",
		__FUNCTION__, offset, val);

	pci_write_config(dev, BCE_PCICFG_REG_WINDOW_ADDRESS, offset, 4);
	pci_write_config(dev, BCE_PCICFG_REG_WINDOW, val, 4);
}


/****************************************************************************/
/* Context memory write.                                                    */
/*                                                                          */
/* The NetXtreme II controller uses context memory to track connection      */
/* information for L2 and higher network protocols.                         */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_ctx_wr(struct bce_softc *sc, u32 cid_addr, u32 offset, u32 val)
{

	DBPRINT(sc, BCE_EXCESSIVE, "%s(); cid_addr = 0x%08X, offset = 0x%08X, "
		"val = 0x%08X\n", __FUNCTION__, cid_addr, offset, val);

	offset += cid_addr;
	REG_WR(sc, BCE_CTX_DATA_ADR, offset);
	REG_WR(sc, BCE_CTX_DATA, val);
}


/****************************************************************************/
/* PHY register read.                                                       */
/*                                                                          */
/* Implements register reads on the MII bus.                                */
/*                                                                          */
/* Returns:                                                                 */
/*   The value of the register.                                             */
/****************************************************************************/
static int
bce_miibus_read_reg(device_t dev, int phy, int reg)
{
	struct bce_softc *sc;
	u32 val;
	int i, s;

	sc = device_get_softc(dev);

	/* Make sure we are accessing the correct PHY address. */
	if (phy != sc->bce_phy_addr) {
		DBPRINT(sc, BCE_VERBOSE, "Invalid PHY address %d for PHY read!\n", phy);
		return(0);
	}

	s = splimp();

	if (sc->bce_phy_flags & BCE_PHY_INT_MODE_AUTO_POLLING_FLAG) {
		val = REG_RD(sc, BCE_EMAC_MDIO_MODE);
		val &= ~BCE_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(sc, BCE_EMAC_MDIO_MODE, val);
		REG_RD(sc, BCE_EMAC_MDIO_MODE);

		DELAY(40);
	}

	val = BCE_MIPHY(phy) | BCE_MIREG(reg) |
		BCE_EMAC_MDIO_COMM_COMMAND_READ | BCE_EMAC_MDIO_COMM_DISEXT |
		BCE_EMAC_MDIO_COMM_START_BUSY;
	REG_WR(sc, BCE_EMAC_MDIO_COMM, val);

	for (i = 0; i < BCE_PHY_TIMEOUT; i++) {
		DELAY(10);

		val = REG_RD(sc, BCE_EMAC_MDIO_COMM);
		if (!(val & BCE_EMAC_MDIO_COMM_START_BUSY)) {
			DELAY(5);

			val = REG_RD(sc, BCE_EMAC_MDIO_COMM);
			val &= BCE_EMAC_MDIO_COMM_DATA;

			break;
		}
	}

	if (val & BCE_EMAC_MDIO_COMM_START_BUSY) {
		BCE_PRINTF(sc, "%s(%d): Error: PHY read timeout! phy = %d, reg = 0x%04X\n",
			__FILE__, __LINE__, phy, reg);
		val = 0x0;
	} else {
		val = REG_RD(sc, BCE_EMAC_MDIO_COMM);
	}

	DBPRINT(sc, BCE_EXCESSIVE, "%s(): phy = %d, reg = 0x%04X, val = 0x%04X\n",
		__FUNCTION__, phy, (u16) reg & 0xffff, (u16) val & 0xffff);

	if (sc->bce_phy_flags & BCE_PHY_INT_MODE_AUTO_POLLING_FLAG) {
		val = REG_RD(sc, BCE_EMAC_MDIO_MODE);
		val |= BCE_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(sc, BCE_EMAC_MDIO_MODE, val);
		REG_RD(sc, BCE_EMAC_MDIO_MODE);

		DELAY(40);
	}

	splx(s);

	return (val & 0xffff);

}


/****************************************************************************/
/* PHY register write.                                                      */
/*                                                                          */
/* Implements register writes on the MII bus.                               */
/*                                                                          */
/* Returns:                                                                 */
/*   The value of the register.                                             */
/****************************************************************************/
static int
bce_miibus_write_reg(device_t dev, int phy, int reg, int val)
{
	struct bce_softc *sc;
	u32 val1;
	int i, s;

	sc = device_get_softc(dev);

	/* Make sure we are accessing the correct PHY address. */
	if (phy != sc->bce_phy_addr) {
		DBPRINT(sc, BCE_WARN, "Invalid PHY address %d for PHY write!\n", phy);
		return(0);
	}

	s = splimp();

	DBPRINT(sc, BCE_EXCESSIVE, "%s(): phy = %d, reg = 0x%04X, val = 0x%04X\n",
		__FUNCTION__, phy, (u16) reg & 0xffff, (u16) val & 0xffff);

	if (sc->bce_phy_flags & BCE_PHY_INT_MODE_AUTO_POLLING_FLAG) {
		val1 = REG_RD(sc, BCE_EMAC_MDIO_MODE);
		val1 &= ~BCE_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(sc, BCE_EMAC_MDIO_MODE, val1);
		REG_RD(sc, BCE_EMAC_MDIO_MODE);

		DELAY(40);
	}

	val1 = BCE_MIPHY(phy) | BCE_MIREG(reg) | val |
		BCE_EMAC_MDIO_COMM_COMMAND_WRITE |
		BCE_EMAC_MDIO_COMM_START_BUSY | BCE_EMAC_MDIO_COMM_DISEXT;
	REG_WR(sc, BCE_EMAC_MDIO_COMM, val1);

	for (i = 0; i < BCE_PHY_TIMEOUT; i++) {
		DELAY(10);

		val1 = REG_RD(sc, BCE_EMAC_MDIO_COMM);
		if (!(val1 & BCE_EMAC_MDIO_COMM_START_BUSY)) {
			DELAY(5);
			break;
		}
	}

	if (val1 & BCE_EMAC_MDIO_COMM_START_BUSY)
		BCE_PRINTF(sc, "%s(%d): PHY write timeout!\n", 
			__FILE__, __LINE__);

	if (sc->bce_phy_flags & BCE_PHY_INT_MODE_AUTO_POLLING_FLAG) {
		val1 = REG_RD(sc, BCE_EMAC_MDIO_MODE);
		val1 |= BCE_EMAC_MDIO_MODE_AUTO_POLL;

		REG_WR(sc, BCE_EMAC_MDIO_MODE, val1);
		REG_RD(sc, BCE_EMAC_MDIO_MODE);

		DELAY(40);
	}

	splx(s);

	return 0;
}


/****************************************************************************/
/* MII bus status change.                                                   */
/*                                                                          */
/* Called by the MII bus driver when the PHY establishes link to set the    */
/* MAC interface registers.                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_miibus_statchg(device_t dev)
{
	struct bce_softc *sc;
	struct mii_data *mii;

	sc = device_get_softc(dev);

	mii = device_get_softc(sc->bce_miibus);

	BCE_CLRBIT(sc, BCE_EMAC_MODE, BCE_EMAC_MODE_PORT);

	/* Set MII or GMII inerface based on the speed negotiated by the PHY. */
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_TX) {
		DBPRINT(sc, BCE_INFO, "Setting GMII interface.\n");
		BCE_SETBIT(sc, BCE_EMAC_MODE, BCE_EMAC_MODE_PORT_GMII);
	} else {
		DBPRINT(sc, BCE_INFO, "Setting MII interface.\n");
		BCE_SETBIT(sc, BCE_EMAC_MODE, BCE_EMAC_MODE_PORT_MII);
	}

	/* Set half or full duplex based on the duplicity negotiated by the PHY. */
	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		DBPRINT(sc, BCE_INFO, "Setting Full-Duplex interface.\n");
		BCE_CLRBIT(sc, BCE_EMAC_MODE, BCE_EMAC_MODE_HALF_DUPLEX);
	} else {
		DBPRINT(sc, BCE_INFO, "Setting Half-Duplex interface.\n");
		BCE_SETBIT(sc, BCE_EMAC_MODE, BCE_EMAC_MODE_HALF_DUPLEX);
	}
}


/****************************************************************************/
/* Acquire NVRAM lock.                                                      */
/*                                                                          */
/* Before the NVRAM can be accessed the caller must acquire an NVRAM lock.  */
/* Locks 0 and 2 are reserved, lock 1 is used by firmware and lock 2 is     */
/* for use by the driver.                                                   */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_acquire_nvram_lock(struct bce_softc *sc)
{
	u32 val;
	int j;

	DBPRINT(sc, BCE_VERBOSE, "Acquiring NVRAM lock.\n");

	/* Request access to the flash interface. */
	REG_WR(sc, BCE_NVM_SW_ARB, BCE_NVM_SW_ARB_ARB_REQ_SET2);
	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		val = REG_RD(sc, BCE_NVM_SW_ARB);
		if (val & BCE_NVM_SW_ARB_ARB_ARB2)
			break;

		DELAY(5);
	}

	if (j >= NVRAM_TIMEOUT_COUNT) {
		DBPRINT(sc, BCE_WARN, "Timeout acquiring NVRAM lock!\n");
		return EBUSY;
	}

	return 0;
}


/****************************************************************************/
/* Release NVRAM lock.                                                      */
/*                                                                          */
/* When the caller is finished accessing NVRAM the lock must be released.   */
/* Locks 0 and 2 are reserved, lock 1 is used by firmware and lock 2 is     */
/* for use by the driver.                                                   */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_release_nvram_lock(struct bce_softc *sc)
{
	int j;
	u32 val;

	DBPRINT(sc, BCE_VERBOSE, "Releasing NVRAM lock.\n");

	/*
	 * Relinquish nvram interface.
	 */
	REG_WR(sc, BCE_NVM_SW_ARB, BCE_NVM_SW_ARB_ARB_REQ_CLR2);

	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		val = REG_RD(sc, BCE_NVM_SW_ARB);
		if (!(val & BCE_NVM_SW_ARB_ARB_ARB2))
			break;

		DELAY(5);
	}

	if (j >= NVRAM_TIMEOUT_COUNT) {
		DBPRINT(sc, BCE_WARN, "Timeout reeasing NVRAM lock!\n");
		return EBUSY;
	}

	return 0;
}


#ifdef BCE_NVRAM_WRITE_SUPPORT
/****************************************************************************/
/* Enable NVRAM write access.                                               */
/*                                                                          */
/* Before writing to NVRAM the caller must enable NVRAM writes.             */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_enable_nvram_write(struct bce_softc *sc)
{
	u32 val;

	DBPRINT(sc, BCE_VERBOSE, "Enabling NVRAM write.\n");

	val = REG_RD(sc, BCE_MISC_CFG);
	REG_WR(sc, BCE_MISC_CFG, val | BCE_MISC_CFG_NVM_WR_EN_PCI);

	if (!sc->bce_flash_info->buffered) {
		int j;

		REG_WR(sc, BCE_NVM_COMMAND, BCE_NVM_COMMAND_DONE);
		REG_WR(sc, BCE_NVM_COMMAND,	BCE_NVM_COMMAND_WREN | BCE_NVM_COMMAND_DOIT);

		for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
			DELAY(5);

			val = REG_RD(sc, BCE_NVM_COMMAND);
			if (val & BCE_NVM_COMMAND_DONE)
				break;
		}

		if (j >= NVRAM_TIMEOUT_COUNT) {
			DBPRINT(sc, BCE_WARN, "Timeout writing NVRAM!\n");
			return EBUSY;
		}
	}
	return 0;
}


/****************************************************************************/
/* Disable NVRAM write access.                                              */
/*                                                                          */
/* When the caller is finished writing to NVRAM write access must be        */
/* disabled.                                                                */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_disable_nvram_write(struct bce_softc *sc)
{
	u32 val;

	DBPRINT(sc, BCE_VERBOSE,  "Disabling NVRAM write.\n");

	val = REG_RD(sc, BCE_MISC_CFG);
	REG_WR(sc, BCE_MISC_CFG, val & ~BCE_MISC_CFG_NVM_WR_EN);
}
#endif


/****************************************************************************/
/* Enable NVRAM access.                                                     */
/*                                                                          */
/* Before accessing NVRAM for read or write operations the caller must      */
/* enabled NVRAM access.                                                    */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_enable_nvram_access(struct bce_softc *sc)
{
	u32 val;

	DBPRINT(sc, BCE_VERBOSE, "Enabling NVRAM access.\n");

	val = REG_RD(sc, BCE_NVM_ACCESS_ENABLE);
	/* Enable both bits, even on read. */
	REG_WR(sc, BCE_NVM_ACCESS_ENABLE,
	       val | BCE_NVM_ACCESS_ENABLE_EN | BCE_NVM_ACCESS_ENABLE_WR_EN);
}


/****************************************************************************/
/* Disable NVRAM access.                                                    */
/*                                                                          */
/* When the caller is finished accessing NVRAM access must be disabled.     */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_disable_nvram_access(struct bce_softc *sc)
{
	u32 val;

	DBPRINT(sc, BCE_VERBOSE, "Disabling NVRAM access.\n");

	val = REG_RD(sc, BCE_NVM_ACCESS_ENABLE);

	/* Disable both bits, even after read. */
	REG_WR(sc, BCE_NVM_ACCESS_ENABLE,
		val & ~(BCE_NVM_ACCESS_ENABLE_EN |
			BCE_NVM_ACCESS_ENABLE_WR_EN));
}


#ifdef BCE_NVRAM_WRITE_SUPPORT
/****************************************************************************/
/* Erase NVRAM page before writing.                                         */
/*                                                                          */
/* Non-buffered flash parts require that a page be erased before it is      */
/* written.                                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_nvram_erase_page(struct bce_softc *sc, u32 offset)
{
	u32 cmd;
	int j;

	/* Buffered flash doesn't require an erase. */
	if (sc->bce_flash_info->buffered)
		return 0;

	DBPRINT(sc, BCE_VERBOSE, "Erasing NVRAM page.\n");

	/* Build an erase command. */
	cmd = BCE_NVM_COMMAND_ERASE | BCE_NVM_COMMAND_WR |
	      BCE_NVM_COMMAND_DOIT;

	/*
	 * Clear the DONE bit separately, set the NVRAM adress to erase,
	 * and issue the erase command.
	 */
	REG_WR(sc, BCE_NVM_COMMAND, BCE_NVM_COMMAND_DONE);
	REG_WR(sc, BCE_NVM_ADDR, offset & BCE_NVM_ADDR_NVM_ADDR_VALUE);
	REG_WR(sc, BCE_NVM_COMMAND, cmd);

	/* Wait for completion. */
	 */
	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		u32 val;

		DELAY(5);

		val = REG_RD(sc, BCE_NVM_COMMAND);
		if (val & BCE_NVM_COMMAND_DONE)
			break;
	}

	if (j >= NVRAM_TIMEOUT_COUNT) {
		DBPRINT(sc, BCE_WARN, "Timeout erasing NVRAM.\n");
		return EBUSY;
	}

	return 0;
}
#endif /* BCE_NVRAM_WRITE_SUPPORT */


/****************************************************************************/
/* Read a dword (32 bits) from NVRAM.                                       */
/*                                                                          */
/* Read a 32 bit word from NVRAM.  The caller is assumed to have already    */
/* obtained the NVRAM lock and enabled the controller for NVRAM access.     */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success and the 32 bit value read, positive value on failure.     */
/****************************************************************************/
static int
bce_nvram_read_dword(struct bce_softc *sc, u32 offset, u8 *ret_val,
							u32 cmd_flags)
{
	u32 cmd;
	int i, rc = 0;

	/* Build the command word. */
	cmd = BCE_NVM_COMMAND_DOIT | cmd_flags;

	/* Calculate the offset for buffered flash. */
	if (sc->bce_flash_info->buffered) {
		offset = ((offset / sc->bce_flash_info->page_size) <<
			   sc->bce_flash_info->page_bits) +
			  (offset % sc->bce_flash_info->page_size);
	}

	/*
	 * Clear the DONE bit separately, set the address to read,
	 * and issue the read.
	 */
	REG_WR(sc, BCE_NVM_COMMAND, BCE_NVM_COMMAND_DONE);
	REG_WR(sc, BCE_NVM_ADDR, offset & BCE_NVM_ADDR_NVM_ADDR_VALUE);
	REG_WR(sc, BCE_NVM_COMMAND, cmd);

	/* Wait for completion. */
	for (i = 0; i < NVRAM_TIMEOUT_COUNT; i++) {
		u32 val;

		DELAY(5);

		val = REG_RD(sc, BCE_NVM_COMMAND);
		if (val & BCE_NVM_COMMAND_DONE) {
			val = REG_RD(sc, BCE_NVM_READ);

			val = bce_be32toh(val);
			memcpy(ret_val, &val, 4);
			break;
		}
	}

	/* Check for errors. */
	if (i >= NVRAM_TIMEOUT_COUNT) {
		BCE_PRINTF(sc, "%s(%d): Timeout error reading NVRAM at offset 0x%08X!\n",
			__FILE__, __LINE__, offset);
		rc = EBUSY;
	}

	return(rc);
}


#ifdef BCE_NVRAM_WRITE_SUPPORT
/****************************************************************************/
/* Write a dword (32 bits) to NVRAM.                                        */
/*                                                                          */
/* Write a 32 bit word to NVRAM.  The caller is assumed to have already     */
/* obtained the NVRAM lock, enabled the controller for NVRAM access, and    */
/* enabled NVRAM write access.                                              */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_nvram_write_dword(struct bce_softc *sc, u32 offset, u8 *val,
	u32 cmd_flags)
{
	u32 cmd, val32;
	int j;

	/* Build the command word. */
	cmd = BCE_NVM_COMMAND_DOIT | BCE_NVM_COMMAND_WR | cmd_flags;

	/* Calculate the offset for buffered flash. */
	if (sc->bce_flash_info->buffered) {
		offset = ((offset / sc->bce_flash_info->page_size) <<
			  sc->bce_flash_info->page_bits) +
			 (offset % sc->bce_flash_info->page_size);
	}

	/*
	 * Clear the DONE bit separately, convert NVRAM data to big-endian,
	 * set the NVRAM address to write, and issue the write command
	 */
	REG_WR(sc, BCE_NVM_COMMAND, BCE_NVM_COMMAND_DONE);
	memcpy(&val32, val, 4);
	val32 = htobe32(val32);
	REG_WR(sc, BCE_NVM_WRITE, val32);
	REG_WR(sc, BCE_NVM_ADDR, offset & BCE_NVM_ADDR_NVM_ADDR_VALUE);
	REG_WR(sc, BCE_NVM_COMMAND, cmd);

	/* Wait for completion. */
	for (j = 0; j < NVRAM_TIMEOUT_COUNT; j++) {
		DELAY(5);

		if (REG_RD(sc, BCE_NVM_COMMAND) & BCE_NVM_COMMAND_DONE)
			break;
	}
	if (j >= NVRAM_TIMEOUT_COUNT) {
		BCE_PRINTF(sc, "%s(%d): Timeout error writing NVRAM at offset 0x%08X\n",
			__FILE__, __LINE__, offset);
		return EBUSY;
	}

	return 0;
}
#endif /* BCE_NVRAM_WRITE_SUPPORT */


/****************************************************************************/
/* Initialize NVRAM access.                                                 */
/*                                                                          */
/* Identify the NVRAM device in use and prepare the NVRAM interface to      */
/* access that device.                                                      */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_init_nvram(struct bce_softc *sc)
{
	u32 val;
	int j, entry_count, rc;
	struct flash_spec *flash;

	DBPRINT(sc,BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/* Determine the selected interface. */
	val = REG_RD(sc, BCE_NVM_CFG1);

	entry_count = sizeof(flash_table) / sizeof(struct flash_spec);

	rc = 0;

	/*
	 * Flash reconfiguration is required to support additional
	 * NVRAM devices not directly supported in hardware.
	 * Check if the flash interface was reconfigured
	 * by the bootcode.
	 */

	if (val & 0x40000000) {
		/* Flash interface reconfigured by bootcode. */

		DBPRINT(sc,BCE_INFO_LOAD, 
			"bce_init_nvram(): Flash WAS reconfigured.\n");

		for (j = 0, flash = &flash_table[0]; j < entry_count;
		     j++, flash++) {
			if ((val & FLASH_BACKUP_STRAP_MASK) ==
			    (flash->config1 & FLASH_BACKUP_STRAP_MASK)) {
				sc->bce_flash_info = flash;
				break;
			}
		}
	} else {
		/* Flash interface not yet reconfigured. */
		u32 mask;

		DBPRINT(sc,BCE_INFO_LOAD, 
			"bce_init_nvram(): Flash was NOT reconfigured.\n");

		if (val & (1 << 23))
			mask = FLASH_BACKUP_STRAP_MASK;
		else
			mask = FLASH_STRAP_MASK;

		/* Look for the matching NVRAM device configuration data. */
		for (j = 0, flash = &flash_table[0]; j < entry_count; j++, flash++) {

			/* Check if the device matches any of the known devices. */
			if ((val & mask) == (flash->strapping & mask)) {
				/* Found a device match. */
				sc->bce_flash_info = flash;

				/* Request access to the flash interface. */
				if ((rc = bce_acquire_nvram_lock(sc)) != 0)
					return rc;

				/* Reconfigure the flash interface. */
				bce_enable_nvram_access(sc);
				REG_WR(sc, BCE_NVM_CFG1, flash->config1);
				REG_WR(sc, BCE_NVM_CFG2, flash->config2);
				REG_WR(sc, BCE_NVM_CFG3, flash->config3);
				REG_WR(sc, BCE_NVM_WRITE1, flash->write1);
				bce_disable_nvram_access(sc);
				bce_release_nvram_lock(sc);

				break;
			}
		}
	}

	/* Check if a matching device was found. */
	if (j == entry_count) {
		sc->bce_flash_info = NULL;
		BCE_PRINTF(sc, "%s(%d): Unknown Flash NVRAM found!\n", 
			__FILE__, __LINE__);
		rc = ENODEV;
	}

	/* Write the flash config data to the shared memory interface. */
	val = REG_RD_IND(sc, sc->bce_shmem_base + BCE_SHARED_HW_CFG_CONFIG2);
	val &= BCE_SHARED_HW_CFG2_NVM_SIZE_MASK;
	if (val)
		sc->bce_flash_size = val;
	else
		sc->bce_flash_size = sc->bce_flash_info->total_size;

	DBPRINT(sc, BCE_INFO_LOAD, "bce_init_nvram() flash->total_size = 0x%08X\n",
		sc->bce_flash_info->total_size);

	DBPRINT(sc,BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	return rc;
}


/****************************************************************************/
/* Read an arbitrary range of data from NVRAM.                              */
/*                                                                          */
/* Prepares the NVRAM interface for access and reads the requested data     */
/* into the supplied buffer.                                                */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success and the data read, positive value on failure.             */
/****************************************************************************/
static int
bce_nvram_read(struct bce_softc *sc, u32 offset, u8 *ret_buf,
	int buf_size)
{
	int rc = 0;
	u32 cmd_flags, offset32, len32, extra;

	if (buf_size == 0)
		return 0;

	/* Request access to the flash interface. */
	if ((rc = bce_acquire_nvram_lock(sc)) != 0)
		return rc;

	/* Enable access to flash interface */
	bce_enable_nvram_access(sc);

	len32 = buf_size;
	offset32 = offset;
	extra = 0;

	cmd_flags = 0;

	if (offset32 & 3) {
		u8 buf[4];
		u32 pre_len;

		offset32 &= ~3;
		pre_len = 4 - (offset & 3);

		if (pre_len >= len32) {
			pre_len = len32;
			cmd_flags = BCE_NVM_COMMAND_FIRST | BCE_NVM_COMMAND_LAST;
		}
		else {
			cmd_flags = BCE_NVM_COMMAND_FIRST;
		}

		rc = bce_nvram_read_dword(sc, offset32, buf, cmd_flags);

		if (rc)
			return rc;

		memcpy(ret_buf, buf + (offset & 3), pre_len);

		offset32 += 4;
		ret_buf += pre_len;
		len32 -= pre_len;
	}

	if (len32 & 3) {
		extra = 4 - (len32 & 3);
		len32 = (len32 + 4) & ~3;
	}

	if (len32 == 4) {
		u8 buf[4];

		if (cmd_flags)
			cmd_flags = BCE_NVM_COMMAND_LAST;
		else
			cmd_flags = BCE_NVM_COMMAND_FIRST |
				    BCE_NVM_COMMAND_LAST;

		rc = bce_nvram_read_dword(sc, offset32, buf, cmd_flags);

		memcpy(ret_buf, buf, 4 - extra);
	}
	else if (len32 > 0) {
		u8 buf[4];

		/* Read the first word. */
		if (cmd_flags)
			cmd_flags = 0;
		else
			cmd_flags = BCE_NVM_COMMAND_FIRST;

		rc = bce_nvram_read_dword(sc, offset32, ret_buf, cmd_flags);

		/* Advance to the next dword. */
		offset32 += 4;
		ret_buf += 4;
		len32 -= 4;

		while (len32 > 4 && rc == 0) {
			rc = bce_nvram_read_dword(sc, offset32, ret_buf, 0);

			/* Advance to the next dword. */
			offset32 += 4;
			ret_buf += 4;
			len32 -= 4;
		}

		if (rc)
			return rc;

		cmd_flags = BCE_NVM_COMMAND_LAST;
		rc = bce_nvram_read_dword(sc, offset32, buf, cmd_flags);

		memcpy(ret_buf, buf, 4 - extra);
	}

	/* Disable access to flash interface and release the lock. */
	bce_disable_nvram_access(sc);
	bce_release_nvram_lock(sc);

	return rc;
}


#ifdef BCE_NVRAM_WRITE_SUPPORT
/****************************************************************************/
/* Write an arbitrary range of data from NVRAM.                             */
/*                                                                          */
/* Prepares the NVRAM interface for write access and writes the requested   */
/* data from the supplied buffer.  The caller is responsible for            */
/* calculating any appropriate CRCs.                                        */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_nvram_write(struct bce_softc *sc, u32 offset, u8 *data_buf,
	int buf_size)
{
	u32 written, offset32, len32;
	u8 *buf, start[4], end[4];
	int rc = 0;
	int align_start, align_end;

	buf = data_buf;
	offset32 = offset;
	len32 = buf_size;
	align_start = align_end = 0;

	if ((align_start = (offset32 & 3))) {
		offset32 &= ~3;
		len32 += align_start;
		if ((rc = bce_nvram_read(sc, offset32, start, 4)))
			return rc;
	}

	if (len32 & 3) {
	       	if ((len32 > 4) || !align_start) {
			align_end = 4 - (len32 & 3);
			len32 += align_end;
			if ((rc = bce_nvram_read(sc, offset32 + len32 - 4,
				end, 4))) {
				return rc;
			}
		}
	}

	if (align_start || align_end) {
		buf = malloc(len32, M_DEVBUF, M_NOWAIT);
		if (buf == 0)
			return ENOMEM;
		if (align_start) {
			memcpy(buf, start, 4);
		}
		if (align_end) {
			memcpy(buf + len32 - 4, end, 4);
		}
		memcpy(buf + align_start, data_buf, buf_size);
	}

	written = 0;
	while ((written < len32) && (rc == 0)) {
		u32 page_start, page_end, data_start, data_end;
		u32 addr, cmd_flags;
		int i;
		u8 flash_buffer[264];

	    /* Find the page_start addr */
		page_start = offset32 + written;
		page_start -= (page_start % sc->bce_flash_info->page_size);
		/* Find the page_end addr */
		page_end = page_start + sc->bce_flash_info->page_size;
		/* Find the data_start addr */
		data_start = (written == 0) ? offset32 : page_start;
		/* Find the data_end addr */
		data_end = (page_end > offset32 + len32) ?
			(offset32 + len32) : page_end;

		/* Request access to the flash interface. */
		if ((rc = bce_acquire_nvram_lock(sc)) != 0)
			goto nvram_write_end;

		/* Enable access to flash interface */
		bce_enable_nvram_access(sc);

		cmd_flags = BCE_NVM_COMMAND_FIRST;
		if (sc->bce_flash_info->buffered == 0) {
			int j;

			/* Read the whole page into the buffer
			 * (non-buffer flash only) */
			for (j = 0; j < sc->bce_flash_info->page_size; j += 4) {
				if (j == (sc->bce_flash_info->page_size - 4)) {
					cmd_flags |= BCE_NVM_COMMAND_LAST;
				}
				rc = bce_nvram_read_dword(sc,
					page_start + j,
					&flash_buffer[j],
					cmd_flags);

				if (rc)
					goto nvram_write_end;

				cmd_flags = 0;
			}
		}

		/* Enable writes to flash interface (unlock write-protect) */
		if ((rc = bce_enable_nvram_write(sc)) != 0)
			goto nvram_write_end;

		/* Erase the page */
		if ((rc = bce_nvram_erase_page(sc, page_start)) != 0)
			goto nvram_write_end;

		/* Re-enable the write again for the actual write */
		bce_enable_nvram_write(sc);

		/* Loop to write back the buffer data from page_start to
		 * data_start */
		i = 0;
		if (sc->bce_flash_info->buffered == 0) {
			for (addr = page_start; addr < data_start;
				addr += 4, i += 4) {

				rc = bce_nvram_write_dword(sc, addr,
					&flash_buffer[i], cmd_flags);

				if (rc != 0)
					goto nvram_write_end;

				cmd_flags = 0;
			}
		}

		/* Loop to write the new data from data_start to data_end */
		for (addr = data_start; addr < data_end; addr += 4, i++) {
			if ((addr == page_end - 4) ||
				((sc->bce_flash_info->buffered) &&
				 (addr == data_end - 4))) {

				cmd_flags |= BCE_NVM_COMMAND_LAST;
			}
			rc = bce_nvram_write_dword(sc, addr, buf,
				cmd_flags);

			if (rc != 0)
				goto nvram_write_end;

			cmd_flags = 0;
			buf += 4;
		}

		/* Loop to write back the buffer data from data_end
		 * to page_end */
		if (sc->bce_flash_info->buffered == 0) {
			for (addr = data_end; addr < page_end;
				addr += 4, i += 4) {

				if (addr == page_end-4) {
					cmd_flags = BCE_NVM_COMMAND_LAST;
                		}
				rc = bce_nvram_write_dword(sc, addr,
					&flash_buffer[i], cmd_flags);

				if (rc != 0)
					goto nvram_write_end;

				cmd_flags = 0;
			}
		}

		/* Disable writes to flash interface (lock write-protect) */
		bce_disable_nvram_write(sc);

		/* Disable access to flash interface */
		bce_disable_nvram_access(sc);
		bce_release_nvram_lock(sc);

		/* Increment written */
		written += data_end - data_start;
	}

nvram_write_end:
	if (align_start || align_end)
		free(buf, M_DEVBUF);

	return rc;
}
#endif /* BCE_NVRAM_WRITE_SUPPORT */


/****************************************************************************/
/* Verifies that NVRAM is accessible and contains valid data.               */
/*                                                                          */
/* Reads the configuration data from NVRAM and verifies that the CRC is     */
/* correct.                                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   0 on success, positive value on failure.                               */
/****************************************************************************/
static int
bce_nvram_test(struct bce_softc *sc)
{
	u32 buf[BCE_NVRAM_SIZE / 4];
	u8 *data = (u8 *) buf;
	int rc = 0;
	u32 magic, csum;


	/*
	 * Check that the device NVRAM is valid by reading
	 * the magic value at offset 0.
	 */
	if ((rc = bce_nvram_read(sc, 0, data, 4)) != 0)
		goto bce_nvram_test_done;


    magic = bce_be32toh(buf[0]);
	if (magic != BCE_NVRAM_MAGIC) {
		rc = ENODEV;
		BCE_PRINTF(sc, "%s(%d): Invalid NVRAM magic value! Expected: 0x%08X, "
			"Found: 0x%08X\n",
			__FILE__, __LINE__, BCE_NVRAM_MAGIC, magic);
		goto bce_nvram_test_done;
	}

	/*
	 * Verify that the device NVRAM includes valid
	 * configuration data.
	 */
	if ((rc = bce_nvram_read(sc, 0x100, data, BCE_NVRAM_SIZE)) != 0)
		goto bce_nvram_test_done;

	csum = ether_crc32_le(data, 0x100);
	if (csum != BCE_CRC32_RESIDUAL) {
		rc = ENODEV;
		BCE_PRINTF(sc, "%s(%d): Invalid Manufacturing Information NVRAM CRC! "
			"Expected: 0x%08X, Found: 0x%08X\n",
			__FILE__, __LINE__, BCE_CRC32_RESIDUAL, csum);
		goto bce_nvram_test_done;
	}

	csum = ether_crc32_le(data + 0x100, 0x100);
	if (csum != BCE_CRC32_RESIDUAL) {
		BCE_PRINTF(sc, "%s(%d): Invalid Feature Configuration Information "
			"NVRAM CRC! Expected: 0x%08X, Found: 08%08X\n",
			__FILE__, __LINE__, BCE_CRC32_RESIDUAL, csum);
		rc = ENODEV;
	}

bce_nvram_test_done:
	return rc;
}


/****************************************************************************/
/* Free any DMA memory owned by the driver.                                 */
/*                                                                          */
/* Scans through each data structre that requires DMA memory and frees      */
/* the memory if allocated.                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_dma_free(struct bce_softc *sc)
{
	int i;

	DBPRINT(sc,BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/* Destroy the status block. */
	if (sc->status_block != NULL)
		bus_dmamem_free(
			sc->status_tag,
		    sc->status_block,
		    sc->status_map);

	if (sc->status_map != NULL) {
		bus_dmamap_unload(
			sc->status_tag,
		    sc->status_map);
		bus_dmamap_destroy(sc->status_tag,
		    sc->status_map);
	}

	if (sc->status_tag != NULL)
		bus_dma_tag_destroy(sc->status_tag);


	/* Destroy the statistics block. */
	if (sc->stats_block != NULL)
		bus_dmamem_free(
			sc->stats_tag,
		    sc->stats_block,
		    sc->stats_map);

	if (sc->stats_map != NULL) {
		bus_dmamap_unload(
			sc->stats_tag,
		    sc->stats_map);
		bus_dmamap_destroy(sc->stats_tag,
		    sc->stats_map);
	}

	if (sc->stats_tag != NULL)
		bus_dma_tag_destroy(sc->stats_tag);


	/* Free, unmap and destroy all TX buffer descriptor chain pages. */
	for (i = 0; i < TX_PAGES; i++ ) {
		if (sc->tx_bd_chain[i] != NULL)
			bus_dmamem_free(
				sc->tx_bd_chain_tag,
			    sc->tx_bd_chain[i],
			    sc->tx_bd_chain_map[i]);

		if (sc->tx_bd_chain_map[i] != NULL) {
			bus_dmamap_unload(
				sc->tx_bd_chain_tag,
		    	sc->tx_bd_chain_map[i]);
			bus_dmamap_destroy(
				sc->tx_bd_chain_tag,
			    sc->tx_bd_chain_map[i]);
		}

	}

	/* Destroy the TX buffer descriptor tag. */
	if (sc->tx_bd_chain_tag != NULL)
		bus_dma_tag_destroy(sc->tx_bd_chain_tag);


	/* Free, unmap and destroy all RX buffer descriptor chain pages. */
	for (i = 0; i < RX_PAGES; i++ ) {
		if (sc->rx_bd_chain[i] != NULL)
			bus_dmamem_free(
				sc->rx_bd_chain_tag,
			    sc->rx_bd_chain[i],
			    sc->rx_bd_chain_map[i]);

		if (sc->rx_bd_chain_map[i] != NULL) {
			bus_dmamap_unload(
				sc->rx_bd_chain_tag,
		    	sc->rx_bd_chain_map[i]);
			bus_dmamap_destroy(
				sc->rx_bd_chain_tag,
			    sc->rx_bd_chain_map[i]);
		}
	}

	/* Destroy the RX buffer descriptor tag. */
	if (sc->rx_bd_chain_tag != NULL)
		bus_dma_tag_destroy(sc->rx_bd_chain_tag);


	/* Unload and destroy the TX mbuf maps. */
	for (i = 0; i < TOTAL_TX_BD; i++) {
		if (sc->tx_mbuf_map[i] != NULL) {
			bus_dmamap_unload(sc->tx_mbuf_tag, 
				sc->tx_mbuf_map[i]);
			bus_dmamap_destroy(sc->tx_mbuf_tag, 
	 			sc->tx_mbuf_map[i]);
		}
	}

	/* Destroy the TX mbuf tag. */
	if (sc->tx_mbuf_tag != NULL)
		bus_dma_tag_destroy(sc->tx_mbuf_tag);


	/* Unload and destroy the RX mbuf maps. */
	for (i = 0; i < TOTAL_RX_BD; i++) {
		if (sc->rx_mbuf_map[i] != NULL) {
			bus_dmamap_unload(sc->rx_mbuf_tag, 
				sc->rx_mbuf_map[i]);
			bus_dmamap_destroy(sc->rx_mbuf_tag, 
	 			sc->rx_mbuf_map[i]);
		}
	}

	/* Destroy the RX mbuf tag. */
	if (sc->rx_mbuf_tag != NULL)
		bus_dma_tag_destroy(sc->rx_mbuf_tag);


	/* Destroy the parent tag */
	if (sc->parent_tag != NULL)
		bus_dma_tag_destroy(sc->parent_tag);

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

}


/****************************************************************************/
/* Get DMA memory from the OS.                                              */
/*                                                                          */
/* Validates that the OS has provided DMA buffers in response to a          */
/* bus_dmamap_load() call and saves the physical address of those buffers.  */
/* When the callback is used the OS will return 0 for the mapping function  */
/* (bus_dmamap_load()) so we use the value of map_arg->maxsegs to pass any  */
/* failures back to the caller.                                             */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct bce_dmamap_arg *map_arg = arg;
	struct bce_softc *sc = map_arg->sc;

	/* Simulate a mapping failure. */
	DBRUNIF(DB_RANDOMTRUE(bce_debug_dma_map_addr_failure),
		BCE_PRINTF(sc, "%s(%d): Simulating DMA mapping error.\n",
			__FILE__, __LINE__);
		error = ENOMEM);
		
	/* Check for an error and signal the caller that an error occurred. */
	if (error || (nseg > map_arg->maxsegs)) {
		BCE_PRINTF(sc, "%s(%d): DMA mapping error! error = %d, "
		"nseg = %d, maxsegs = %d\n",
			__FILE__, __LINE__, error, nseg, map_arg->maxsegs);
		map_arg->maxsegs = 0;
		goto bce_dma_map_addr_exit;
	}

	map_arg->busaddr = segs->ds_addr;

bce_dma_map_addr_exit:
	return;
}

static void
bce_dma_map_rx_desc(void *arg, bus_dma_segment_t *segs,
	int nseg, bus_size_t mapsize, int error)
{
	struct bce_dmamap_arg *map_arg;
	struct bce_softc *sc;
	struct rx_bd *rxbd = NULL;
	u16 prod, chain_prod;
	u32	prod_bseq;
#ifdef BCE_DEBUG
	u16 debug_prod;
#endif
	int i;

	map_arg = arg;
	sc = map_arg->sc;

	if (error) {
		DBPRINT(sc, BCE_WARN, "%s(): Called with error = %d\n",
			__FUNCTION__, error);
		return;
	}

	/* Signal error to caller if there's too many segments */
	if (nseg > map_arg->maxsegs) {
		DBPRINT(sc, BCE_WARN,
			"%s(): Mapped RX descriptors: max segs = %d, "
			"actual segs = %d\n",
			__FUNCTION__, map_arg->maxsegs, nseg);

		map_arg->maxsegs = 0;
		return;
	}

	/* prod points to an empty rx_bd at this point. */
	prod       = map_arg->prod;
	chain_prod = map_arg->chain_prod;
	prod_bseq  = map_arg->prod_bseq;

#ifdef BCE_DEBUG
	debug_prod = chain_prod;
#endif

	/* Setup the rx_bd for the first segment. */
	rxbd = &sc->rx_bd_chain[RX_PAGE(chain_prod)][RX_IDX(chain_prod)];

	rxbd->rx_bd_haddr_lo  = htole32(BCE_ADDR_LO(segs[0].ds_addr));
	rxbd->rx_bd_haddr_hi  = htole32(BCE_ADDR_HI(segs[0].ds_addr));
	rxbd->rx_bd_len       = htole32(segs[0].ds_len);
	rxbd->rx_bd_flags     = htole32(RX_BD_FLAGS_START);
	prod_bseq += segs[0].ds_len;

	for (i = 1; i < nseg; i++) {

		prod = NEXT_RX_BD(prod);
		chain_prod = RX_CHAIN_IDX(prod); 

		rxbd = &sc->rx_bd_chain[RX_PAGE(chain_prod)][RX_IDX(chain_prod)];

		rxbd->rx_bd_haddr_lo  = htole32(BCE_ADDR_LO(segs[i].ds_addr));
		rxbd->rx_bd_haddr_hi  = htole32(BCE_ADDR_HI(segs[i].ds_addr));
		rxbd->rx_bd_len       = htole32(segs[i].ds_len);
		rxbd->rx_bd_flags     = 0;
		prod_bseq += segs[i].ds_len;
	}

	rxbd->rx_bd_flags |= htole32(RX_BD_FLAGS_END);

	DBRUN(BCE_VERBOSE_RECV, bce_dump_rx_mbuf_chain(sc, debug_prod, nseg));

	DBPRINT(sc, BCE_VERBOSE_RECV, "%s(exit): prod = 0x%04X, chain_prod = 0x%04X, "
		"prod_bseq = 0x%08X\n", __FUNCTION__, prod, chain_prod, prod_bseq);

	/* prod points to the last rx_bd at this point. */
	map_arg->maxsegs    = nseg;
	map_arg->prod       = prod;
	map_arg->chain_prod = chain_prod;
	map_arg->prod_bseq  = prod_bseq;
}

/****************************************************************************/
/* Map TX buffers into TX buffer descriptors.                               */
/*                                                                          */
/* Given a series of DMA memory containting an outgoing frame, map the      */
/* segments into the tx_bd structure used by the hardware.                  */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_dma_map_tx_desc(void *arg, bus_dma_segment_t *segs,
	int nseg, bus_size_t mapsize, int error)
{
	struct bce_dmamap_arg *map_arg;
	struct bce_softc *sc;
	struct tx_bd *txbd = NULL;
	int i = 0;
	u16 prod, chain_prod;
	u32	prod_bseq;
#ifdef BCE_DEBUG
	u16 debug_prod;
#endif

	map_arg = arg;
	sc = map_arg->sc;

	if (error) {
		DBPRINT(sc, BCE_WARN, "%s(): Called with error = %d\n",
			__FUNCTION__, error);
		return;
	}

	/* Signal error to caller if there's too many segments */
	if (nseg > map_arg->maxsegs) {
		DBPRINT(sc, BCE_WARN,
			"%s(): Mapped TX descriptors: max segs = %d, "
			"actual segs = %d\n",
			__FUNCTION__, map_arg->maxsegs, nseg);

		map_arg->maxsegs = 0;
		return;
	}

	/* prod points to an empty tx_bd at this point. */
	prod       = map_arg->prod;
	chain_prod = map_arg->chain_prod;
	prod_bseq  = map_arg->prod_bseq;

#ifdef BCE_DEBUG
	debug_prod = chain_prod;
#endif

	DBPRINT(sc, BCE_INFO_SEND,
		"%s(): Start: prod = 0x%04X, chain_prod = %04X, "
		"prod_bseq = 0x%08X\n",
		__FUNCTION__, prod, chain_prod, prod_bseq);

	/*
	 * Cycle through each mbuf segment that makes up
	 * the outgoing frame, gathering the mapping info
	 * for that segment and creating a tx_bd to for
	 * the mbuf.
	 */

	txbd = &map_arg->tx_chain[TX_PAGE(chain_prod)][TX_IDX(chain_prod)];

	/* Setup the first tx_bd for the first segment. */
	txbd->tx_bd_haddr_lo       = htole32(BCE_ADDR_LO(segs[i].ds_addr));
	txbd->tx_bd_haddr_hi       = htole32(BCE_ADDR_HI(segs[i].ds_addr));
	txbd->tx_bd_mss_nbytes     = htole16(segs[i].ds_len);
	txbd->tx_bd_vlan_tag_flags = htole16(map_arg->tx_flags |
			TX_BD_FLAGS_START);
	prod_bseq += segs[i].ds_len;

	/* Setup any remaing segments. */
	for (i = 1; i < nseg; i++) {
		prod       = NEXT_TX_BD(prod);
		chain_prod = TX_CHAIN_IDX(prod);

		txbd = &map_arg->tx_chain[TX_PAGE(chain_prod)][TX_IDX(chain_prod)];

		txbd->tx_bd_haddr_lo       = htole32(BCE_ADDR_LO(segs[i].ds_addr));
		txbd->tx_bd_haddr_hi       = htole32(BCE_ADDR_HI(segs[i].ds_addr));
		txbd->tx_bd_mss_nbytes     = htole16(segs[i].ds_len);
		txbd->tx_bd_vlan_tag_flags = htole16(map_arg->tx_flags);

		prod_bseq += segs[i].ds_len;
	}

	/* Set the END flag on the last TX buffer descriptor. */
	txbd->tx_bd_vlan_tag_flags |= htole16(TX_BD_FLAGS_END);

	DBRUN(BCE_INFO_SEND, bce_dump_tx_chain(sc, debug_prod, nseg));

	DBPRINT(sc, BCE_INFO_SEND,
		"%s(): End: prod = 0x%04X, chain_prod = %04X, "
		"prod_bseq = 0x%08X\n",
		__FUNCTION__, prod, chain_prod, prod_bseq);

	/* prod points to the last tx_bd at this point. */
	map_arg->maxsegs    = nseg;
	map_arg->prod       = prod;
	map_arg->chain_prod = chain_prod;
	map_arg->prod_bseq  = prod_bseq;
}


/****************************************************************************/
/* Allocate any DMA memory needed by the driver.                            */
/*                                                                          */
/* Allocates DMA memory needed for the various global structures needed by  */
/* hardware.                                                                */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_dma_alloc(device_t dev)
{
	struct bce_softc *sc;
	int i, error, rc = 0;
	struct bce_dmamap_arg map_arg;

	sc = device_get_softc(dev);

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
	if (bus_dma_tag_create(NULL,		/* parent     */
			BCE_DMA_ALIGN,				/* alignment  */
			BCE_DMA_BOUNDARY,			/* boundary   */
			sc->max_bus_addr,			/* lowaddr    */
			BUS_SPACE_MAXADDR,			/* highaddr   */
			NULL, 						/* filterfunc */
			NULL,						/* filterarg  */
			MAXBSIZE, 					/* maxsize    */
			BUS_SPACE_UNRESTRICTED,		/* nsegments  */
			BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
			0,							/* flags      */
			&sc->parent_tag)) {
		BCE_PRINTF(sc, "%s(%d): Could not allocate parent DMA tag!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	/*
	 * Create a DMA tag for the status block, allocate and clear the
	 * memory, map the memory into DMA space, and fetch the physical 
	 * address of the block.
	 */
	if (bus_dma_tag_create(
			sc->parent_tag,			/* parent      */
	    	BCE_DMA_ALIGN,			/* alignment   */
	    	BCE_DMA_BOUNDARY,		/* boundary    */
	    	sc->max_bus_addr,		/* lowaddr     */
	    	BUS_SPACE_MAXADDR,		/* highaddr    */
	    	NULL, 					/* filterfunc  */
	    	NULL, 					/* filterarg   */
	    	BCE_STATUS_BLK_SZ, 		/* maxsize     */
	    	1,						/* nsegments   */
	    	BCE_STATUS_BLK_SZ, 		/* maxsegsize  */
	    	0,						/* flags       */
	    	&sc->status_tag)) {
		BCE_PRINTF(sc, "%s(%d): Could not allocate status block DMA tag!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	if(bus_dmamem_alloc(
			sc->status_tag,				/* dmat        */
	    	(void **)&sc->status_block,	/* vaddr       */
	    	BUS_DMA_NOWAIT,					/* flags       */
	    	&sc->status_map)) {
		BCE_PRINTF(sc, "%s(%d): Could not allocate status block DMA memory!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	bzero((char *)sc->status_block, BCE_STATUS_BLK_SZ);

	map_arg.sc = sc;
	map_arg.maxsegs = 1;

	error = bus_dmamap_load(
			sc->status_tag,	   		/* dmat        */
	    	sc->status_map,	   		/* map         */
	    	sc->status_block,	 	/* buf         */
	    	BCE_STATUS_BLK_SZ,	 	/* buflen      */
	    	bce_dma_map_addr, 	 	/* callback    */
	    	&map_arg,			 	/* callbackarg */
	    	BUS_DMA_NOWAIT);		/* flags       */
	    	
	if(error || (map_arg.maxsegs == 0)) {
		BCE_PRINTF(sc, "%s(%d): Could not map status block DMA memory!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	sc->status_block_paddr = map_arg.busaddr;
	/* DRC - Fix for 64 bit addresses. */
	DBPRINT(sc, BCE_INFO, "status_block_paddr = 0x%08X\n",
		(u32) sc->status_block_paddr);

	/*
	 * Create a DMA tag for the statistics block, allocate and clear the
	 * memory, map the memory into DMA space, and fetch the physical 
	 * address of the block.
	 */
	if (bus_dma_tag_create(
			sc->parent_tag,			/* parent      */
	    	BCE_DMA_ALIGN,	 		/* alignment   */
	    	BCE_DMA_BOUNDARY, 		/* boundary    */
	    	sc->max_bus_addr,		/* lowaddr     */
	    	BUS_SPACE_MAXADDR,		/* highaddr    */
	    	NULL,		 	  		/* filterfunc  */
	    	NULL, 			  		/* filterarg   */
	    	BCE_STATS_BLK_SZ, 		/* maxsize     */
	    	1,				  		/* nsegments   */
	    	BCE_STATS_BLK_SZ, 		/* maxsegsize  */
	    	0, 				  		/* flags       */
	    	&sc->stats_tag)) {
		BCE_PRINTF(sc, "%s(%d): Could not allocate statistics block DMA tag!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	if (bus_dmamem_alloc(
			sc->stats_tag,				/* dmat        */
	    	(void **)&sc->stats_block,	/* vaddr       */
	    	BUS_DMA_NOWAIT,	 			/* flags       */
	    	&sc->stats_map)) {
		BCE_PRINTF(sc, "%s(%d): Could not allocate statistics block DMA memory!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	bzero((char *)sc->stats_block, BCE_STATS_BLK_SZ);

	map_arg.sc = sc;
	map_arg.maxsegs = 1;

	error = bus_dmamap_load(
			sc->stats_tag,	 	/* dmat        */
	    	sc->stats_map,	 	/* map         */
	    	sc->stats_block, 	/* buf         */
	    	BCE_STATS_BLK_SZ,	/* buflen      */
	    	bce_dma_map_addr,	/* callback    */
	    	&map_arg, 		 	/* callbackarg */
	    	BUS_DMA_NOWAIT);	/* flags       */

	if(error || (map_arg.maxsegs == 0)) {
		BCE_PRINTF(sc, "%s(%d): Could not map statistics block DMA memory!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	sc->stats_block_paddr = map_arg.busaddr;
	/* DRC - Fix for 64 bit address. */
	DBPRINT(sc,BCE_INFO, "stats_block_paddr = 0x%08X\n", 
		(u32) sc->stats_block_paddr);

	/*
	 * Create a DMA tag for the TX buffer descriptor chain,
	 * allocate and clear the  memory, and fetch the
	 * physical address of the block.
	 */
	if(bus_dma_tag_create(
			sc->parent_tag,		  /* parent      */
	    	BCM_PAGE_SIZE,		  /* alignment   */
	    	BCE_DMA_BOUNDARY,	  /* boundary    */
			sc->max_bus_addr,	  /* lowaddr     */
			BUS_SPACE_MAXADDR, 	  /* highaddr    */
			NULL, 				  /* filterfunc  */ 
			NULL, 				  /* filterarg   */
			BCE_TX_CHAIN_PAGE_SZ, /* maxsize     */
			1,			  		  /* nsegments   */
			BCE_TX_CHAIN_PAGE_SZ, /* maxsegsize  */
			0,				 	  /* flags       */
			&sc->tx_bd_chain_tag)) {
		BCE_PRINTF(sc, "%s(%d): Could not allocate TX descriptor chain DMA tag!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	for (i = 0; i < TX_PAGES; i++) {

		if(bus_dmamem_alloc(
				sc->tx_bd_chain_tag,			/* tag   */
	    		(void **)&sc->tx_bd_chain[i],	/* vaddr */
	    		BUS_DMA_NOWAIT,					/* flags */
		    	&sc->tx_bd_chain_map[i])) {
			BCE_PRINTF(sc, "%s(%d): Could not allocate TX descriptor "
				"chain DMA memory!\n", __FILE__, __LINE__);
			rc = ENOMEM;
			goto bce_dma_alloc_exit;
		}

		map_arg.maxsegs = 1;
		map_arg.sc = sc;

		error = bus_dmamap_load(
				sc->tx_bd_chain_tag,	 /* dmat        */
	    		sc->tx_bd_chain_map[i],	 /* map         */
	    		sc->tx_bd_chain[i],		 /* buf         */
		    	BCE_TX_CHAIN_PAGE_SZ,  	 /* buflen      */
		    	bce_dma_map_addr, 	   	 /* callback    */
	    		&map_arg, 			   	 /* callbackarg */
	    		BUS_DMA_NOWAIT);	   	 /* flags       */

		if(error || (map_arg.maxsegs == 0)) {
			BCE_PRINTF(sc, "%s(%d): Could not map TX descriptor chain DMA memory!\n",
				__FILE__, __LINE__);
			rc = ENOMEM;
			goto bce_dma_alloc_exit;
		}

		sc->tx_bd_chain_paddr[i] = map_arg.busaddr;
		/* DRC - Fix for 64 bit systems. */
		DBPRINT(sc, BCE_INFO, "tx_bd_chain_paddr[%d] = 0x%08X\n", 
			i, (u32) sc->tx_bd_chain_paddr[i]);
	}

	/* Create a DMA tag for TX mbufs. */
	if (bus_dma_tag_create(
			sc->parent_tag,	 	 	/* parent      */
	    	BCE_DMA_ALIGN,	 		/* alignment   */
	    	BCE_DMA_BOUNDARY, 		/* boundary    */
			sc->max_bus_addr,		/* lowaddr     */
			BUS_SPACE_MAXADDR,		/* highaddr    */
			NULL, 			  		/* filterfunc  */
			NULL, 			  		/* filterarg   */
			MCLBYTES * BCE_MAX_SEGMENTS,	/* maxsize     */
			BCE_MAX_SEGMENTS,  		/* nsegments   */
			MCLBYTES,				/* maxsegsize  */
			0,				 		/* flags       */
	    	&sc->tx_mbuf_tag)) {
		BCE_PRINTF(sc, "%s(%d): Could not allocate TX mbuf DMA tag!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	/* Create DMA maps for the TX mbufs clusters. */
	for (i = 0; i < TOTAL_TX_BD; i++) {
		if (bus_dmamap_create(sc->tx_mbuf_tag, BUS_DMA_NOWAIT, 
			&sc->tx_mbuf_map[i])) {
			BCE_PRINTF(sc, "%s(%d): Unable to create TX mbuf DMA map!\n",
				__FILE__, __LINE__);
			rc = ENOMEM;
			goto bce_dma_alloc_exit;
		}
	}

	/*
	 * Create a DMA tag for the RX buffer descriptor chain,
	 * allocate and clear the  memory, and fetch the physical
	 * address of the blocks.
	 */
	if (bus_dma_tag_create(
			sc->parent_tag,			/* parent      */
	    	BCM_PAGE_SIZE,			/* alignment   */
	    	BCE_DMA_BOUNDARY,		/* boundary    */
			BUS_SPACE_MAXADDR,		/* lowaddr     */
			sc->max_bus_addr,		/* lowaddr     */
			NULL,					/* filter      */
			NULL, 					/* filterarg   */
			BCE_RX_CHAIN_PAGE_SZ,	/* maxsize     */
			1, 						/* nsegments   */
			BCE_RX_CHAIN_PAGE_SZ,	/* maxsegsize  */
			0,				 		/* flags       */
			&sc->rx_bd_chain_tag)) {
		BCE_PRINTF(sc, "%s(%d): Could not allocate RX descriptor chain DMA tag!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	for (i = 0; i < RX_PAGES; i++) {

		if (bus_dmamem_alloc(
				sc->rx_bd_chain_tag,			/* tag   */
	    		(void **)&sc->rx_bd_chain[i], 	/* vaddr */
	    		BUS_DMA_NOWAIT,				  	/* flags */
		    	&sc->rx_bd_chain_map[i])) {
			BCE_PRINTF(sc, "%s(%d): Could not allocate RX descriptor chain "
				"DMA memory!\n", __FILE__, __LINE__);
			rc = ENOMEM;
			goto bce_dma_alloc_exit;
		}

		bzero((char *)sc->rx_bd_chain[i], BCE_RX_CHAIN_PAGE_SZ);

		map_arg.maxsegs = 1;
		map_arg.sc = sc;

		error = bus_dmamap_load(
				sc->rx_bd_chain_tag,	/* dmat        */
	    		sc->rx_bd_chain_map[i],	/* map         */
	    		sc->rx_bd_chain[i],		/* buf         */
		    	BCE_RX_CHAIN_PAGE_SZ,  	/* buflen      */
		    	bce_dma_map_addr,	   	/* callback    */
	    		&map_arg,			   	/* callbackarg */
	    		BUS_DMA_NOWAIT);		/* flags       */

		if(error || (map_arg.maxsegs == 0)) {
			BCE_PRINTF(sc, "%s(%d): Could not map RX descriptor chain DMA memory!\n",
				__FILE__, __LINE__);
			rc = ENOMEM;
			goto bce_dma_alloc_exit;
		}

		sc->rx_bd_chain_paddr[i] = map_arg.busaddr;
		/* DRC - Fix for 64 bit systems. */
		DBPRINT(sc, BCE_INFO, "rx_bd_chain_paddr[%d] = 0x%08X\n",
			i, (u32) sc->rx_bd_chain_paddr[i]);
	}

	/*
	 * Create a DMA tag for RX mbufs.
	 */
	if (bus_dma_tag_create(
			sc->parent_tag,			/* parent      */
	    	BCE_DMA_ALIGN,		  	/* alignment   */
	    	BCE_DMA_BOUNDARY,	  	/* boundary    */
			sc->max_bus_addr,	  	/* lowaddr     */
			BUS_SPACE_MAXADDR, 	  	/* highaddr    */
			NULL, 				  	/* filterfunc  */
			NULL, 				  	/* filterarg   */
			MCLBYTES,				/* maxsize     */
			BCE_MAX_SEGMENTS,  		/* nsegments   */
			MCLBYTES,				/* maxsegsize  */
			0,				 	  	/* flags       */
	    	&sc->rx_mbuf_tag)) {
		BCE_PRINTF(sc, "%s(%d): Could not allocate RX mbuf DMA tag!\n",
			__FILE__, __LINE__);
		rc = ENOMEM;
		goto bce_dma_alloc_exit;
	}

	/* Create DMA maps for the RX mbuf clusters. */
	for (i = 0; i < TOTAL_RX_BD; i++) {
		if (bus_dmamap_create(sc->rx_mbuf_tag, BUS_DMA_NOWAIT,
				&sc->rx_mbuf_map[i])) {
			BCE_PRINTF(sc, "%s(%d): Unable to create RX mbuf DMA map!\n",
				__FILE__, __LINE__);
			rc = ENOMEM;
			goto bce_dma_alloc_exit;
		}
	}

bce_dma_alloc_exit:
	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	return(rc);
}


/****************************************************************************/
/* Release all resources used by the driver.                                */
/*                                                                          */
/* Releases all resources acquired by the driver including interrupts,      */
/* interrupt handler, interfaces, mutexes, and DMA memory.                  */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_release_resources(struct bce_softc *sc)
{
	device_t dev;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	dev = sc->bce_dev;

	bce_dma_free(sc);

	if (sc->bce_intrhand != NULL)
		bus_teardown_intr(dev, sc->bce_irq, sc->bce_intrhand);

	if (sc->bce_irq != NULL)
		bus_release_resource(dev,
			SYS_RES_IRQ,
			0,
			sc->bce_irq);

	if (sc->bce_res != NULL)
		bus_release_resource(dev,
			SYS_RES_MEMORY,
		    PCIR_BAR(0),
		    sc->bce_res);

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

}


/****************************************************************************/
/* Firmware synchronization.                                                */
/*                                                                          */
/* Before performing certain events such as a chip reset, synchronize with  */
/* the firmware first.                                                      */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_fw_sync(struct bce_softc *sc, u32 msg_data)
{
	int i, rc = 0;
	u32 val;

	/* GCC 2.95.x is dumb */
	val = 0;

	/* Don't waste any time if we've timed out before. */
	if (sc->bce_fw_timed_out) {
		rc = EBUSY;
		goto bce_fw_sync_exit;
	}

	/* Increment the message sequence number. */
	sc->bce_fw_wr_seq++;
	msg_data |= sc->bce_fw_wr_seq;

 	DBPRINT(sc, BCE_VERBOSE, "bce_fw_sync(): msg_data = 0x%08X\n", msg_data);

	/* Send the message to the bootcode driver mailbox. */
	REG_WR_IND(sc, sc->bce_shmem_base + BCE_DRV_MB, msg_data);

	/* Wait for the bootcode to acknowledge the message. */
	for (i = 0; i < FW_ACK_TIME_OUT_MS; i++) {
		/* Check for a response in the bootcode firmware mailbox. */
		val = REG_RD_IND(sc, sc->bce_shmem_base + BCE_FW_MB);
		if ((val & BCE_FW_MSG_ACK) == (msg_data & BCE_DRV_MSG_SEQ))
			break;
		DELAY(1000);
	}

	/* If we've timed out, tell the bootcode that we've stopped waiting. */
	if (((val & BCE_FW_MSG_ACK) != (msg_data & BCE_DRV_MSG_SEQ)) &&
		((msg_data & BCE_DRV_MSG_DATA) != BCE_DRV_MSG_DATA_WAIT0)) {

		BCE_PRINTF(sc, "%s(%d): Firmware synchronization timeout! "
			"msg_data = 0x%08X\n",
			__FILE__, __LINE__, msg_data);

		msg_data &= ~BCE_DRV_MSG_CODE;
		msg_data |= BCE_DRV_MSG_CODE_FW_TIMEOUT;

		REG_WR_IND(sc, sc->bce_shmem_base + BCE_DRV_MB, msg_data);

		sc->bce_fw_timed_out = 1;
		rc = EBUSY;
	}

bce_fw_sync_exit:
	return (rc);
}


/****************************************************************************/
/* Load Receive Virtual 2 Physical (RV2P) processor firmware.               */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_load_rv2p_fw(struct bce_softc *sc, u32 *rv2p_code, 
	u32 rv2p_code_len, u32 rv2p_proc)
{
	int i;
	u32 val;

	for (i = 0; i < rv2p_code_len; i += 8) {
		REG_WR(sc, BCE_RV2P_INSTR_HIGH, *rv2p_code);
		rv2p_code++;
		REG_WR(sc, BCE_RV2P_INSTR_LOW, *rv2p_code);
		rv2p_code++;

		if (rv2p_proc == RV2P_PROC1) {
			val = (i / 8) | BCE_RV2P_PROC1_ADDR_CMD_RDWR;
			REG_WR(sc, BCE_RV2P_PROC1_ADDR_CMD, val);
		}
		else {
			val = (i / 8) | BCE_RV2P_PROC2_ADDR_CMD_RDWR;
			REG_WR(sc, BCE_RV2P_PROC2_ADDR_CMD, val);
		}
	}

	/* Reset the processor, un-stall is done later. */
	if (rv2p_proc == RV2P_PROC1) {
		REG_WR(sc, BCE_RV2P_COMMAND, BCE_RV2P_COMMAND_PROC1_RESET);
	}
	else {
		REG_WR(sc, BCE_RV2P_COMMAND, BCE_RV2P_COMMAND_PROC2_RESET);
	}
}


/****************************************************************************/
/* Load RISC processor firmware.                                            */
/*                                                                          */
/* Loads firmware from the file if_bcefw.h into the scratchpad memory       */
/* associated with a particular processor.                                  */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_load_cpu_fw(struct bce_softc *sc, struct cpu_reg *cpu_reg,
	struct fw_info *fw)
{
	u32 offset;
	u32 val;

	/* Halt the CPU. */
	val = REG_RD_IND(sc, cpu_reg->mode);
	val |= cpu_reg->mode_value_halt;
	REG_WR_IND(sc, cpu_reg->mode, val);
	REG_WR_IND(sc, cpu_reg->state, cpu_reg->state_value_clear);

	/* Load the Text area. */
	offset = cpu_reg->spad_base + (fw->text_addr - cpu_reg->mips_view_base);
	if (fw->text) {
		int j;

		for (j = 0; j < (fw->text_len / 4); j++, offset += 4) {
			REG_WR_IND(sc, offset, fw->text[j]);
	        }
	}

	/* Load the Data area. */
	offset = cpu_reg->spad_base + (fw->data_addr - cpu_reg->mips_view_base);
	if (fw->data) {
		int j;

		for (j = 0; j < (fw->data_len / 4); j++, offset += 4) {
			REG_WR_IND(sc, offset, fw->data[j]);
		}
	}

	/* Load the SBSS area. */
	offset = cpu_reg->spad_base + (fw->sbss_addr - cpu_reg->mips_view_base);
	if (fw->sbss) {
		int j;

		for (j = 0; j < (fw->sbss_len / 4); j++, offset += 4) {
			REG_WR_IND(sc, offset, fw->sbss[j]);
		}
	}

	/* Load the BSS area. */
	offset = cpu_reg->spad_base + (fw->bss_addr - cpu_reg->mips_view_base);
	if (fw->bss) {
		int j;

		for (j = 0; j < (fw->bss_len/4); j++, offset += 4) {
			REG_WR_IND(sc, offset, fw->bss[j]);
		}
	}

	/* Load the Read-Only area. */
	offset = cpu_reg->spad_base +
		(fw->rodata_addr - cpu_reg->mips_view_base);
	if (fw->rodata) {
		int j;

		for (j = 0; j < (fw->rodata_len / 4); j++, offset += 4) {
			REG_WR_IND(sc, offset, fw->rodata[j]);
		}
	}

	/* Clear the pre-fetch instruction. */
	REG_WR_IND(sc, cpu_reg->inst, 0);
	REG_WR_IND(sc, cpu_reg->pc, fw->start_addr);

	/* Start the CPU. */
	val = REG_RD_IND(sc, cpu_reg->mode);
	val &= ~cpu_reg->mode_value_halt;
	REG_WR_IND(sc, cpu_reg->state, cpu_reg->state_value_clear);
	REG_WR_IND(sc, cpu_reg->mode, val);
}


/****************************************************************************/
/* Initialize the RV2P, RX, TX, TPAT, and COM CPUs.                         */
/*                                                                          */
/* Loads the firmware for each CPU and starts the CPU.                      */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_init_cpus(struct bce_softc *sc)
{
	struct cpu_reg cpu_reg;
	struct fw_info fw;

	/* Initialize the RV2P processor. */
	bce_load_rv2p_fw(sc, bce_rv2p_proc1, sizeof(bce_rv2p_proc1), RV2P_PROC1);
	bce_load_rv2p_fw(sc, bce_rv2p_proc2, sizeof(bce_rv2p_proc2), RV2P_PROC2);

	/* Initialize the RX Processor. */
	cpu_reg.mode = BCE_RXP_CPU_MODE;
	cpu_reg.mode_value_halt = BCE_RXP_CPU_MODE_SOFT_HALT;
	cpu_reg.mode_value_sstep = BCE_RXP_CPU_MODE_STEP_ENA;
	cpu_reg.state = BCE_RXP_CPU_STATE;
	cpu_reg.state_value_clear = 0xffffff;
	cpu_reg.gpr0 = BCE_RXP_CPU_REG_FILE;
	cpu_reg.evmask = BCE_RXP_CPU_EVENT_MASK;
	cpu_reg.pc = BCE_RXP_CPU_PROGRAM_COUNTER;
	cpu_reg.inst = BCE_RXP_CPU_INSTRUCTION;
	cpu_reg.bp = BCE_RXP_CPU_HW_BREAKPOINT;
	cpu_reg.spad_base = BCE_RXP_SCRATCH;
	cpu_reg.mips_view_base = 0x8000000;

	fw.ver_major = bce_RXP_b06FwReleaseMajor;
	fw.ver_minor = bce_RXP_b06FwReleaseMinor;
	fw.ver_fix = bce_RXP_b06FwReleaseFix;
	fw.start_addr = bce_RXP_b06FwStartAddr;

	fw.text_addr = bce_RXP_b06FwTextAddr;
	fw.text_len = bce_RXP_b06FwTextLen;
	fw.text_index = 0;
	fw.text = bce_RXP_b06FwText;

	fw.data_addr = bce_RXP_b06FwDataAddr;
	fw.data_len = bce_RXP_b06FwDataLen;
	fw.data_index = 0;
	fw.data = bce_RXP_b06FwData;

	fw.sbss_addr = bce_RXP_b06FwSbssAddr;
	fw.sbss_len = bce_RXP_b06FwSbssLen;
	fw.sbss_index = 0;
	fw.sbss = bce_RXP_b06FwSbss;

	fw.bss_addr = bce_RXP_b06FwBssAddr;
	fw.bss_len = bce_RXP_b06FwBssLen;
	fw.bss_index = 0;
	fw.bss = bce_RXP_b06FwBss;

	fw.rodata_addr = bce_RXP_b06FwRodataAddr;
	fw.rodata_len = bce_RXP_b06FwRodataLen;
	fw.rodata_index = 0;
	fw.rodata = bce_RXP_b06FwRodata;

	DBPRINT(sc, BCE_INFO_RESET, "Loading RX firmware.\n");
	bce_load_cpu_fw(sc, &cpu_reg, &fw);

	/* Initialize the TX Processor. */
	cpu_reg.mode = BCE_TXP_CPU_MODE;
	cpu_reg.mode_value_halt = BCE_TXP_CPU_MODE_SOFT_HALT;
	cpu_reg.mode_value_sstep = BCE_TXP_CPU_MODE_STEP_ENA;
	cpu_reg.state = BCE_TXP_CPU_STATE;
	cpu_reg.state_value_clear = 0xffffff;
	cpu_reg.gpr0 = BCE_TXP_CPU_REG_FILE;
	cpu_reg.evmask = BCE_TXP_CPU_EVENT_MASK;
	cpu_reg.pc = BCE_TXP_CPU_PROGRAM_COUNTER;
	cpu_reg.inst = BCE_TXP_CPU_INSTRUCTION;
	cpu_reg.bp = BCE_TXP_CPU_HW_BREAKPOINT;
	cpu_reg.spad_base = BCE_TXP_SCRATCH;
	cpu_reg.mips_view_base = 0x8000000;

	fw.ver_major = bce_TXP_b06FwReleaseMajor;
	fw.ver_minor = bce_TXP_b06FwReleaseMinor;
	fw.ver_fix = bce_TXP_b06FwReleaseFix;
	fw.start_addr = bce_TXP_b06FwStartAddr;

	fw.text_addr = bce_TXP_b06FwTextAddr;
	fw.text_len = bce_TXP_b06FwTextLen;
	fw.text_index = 0;
	fw.text = bce_TXP_b06FwText;

	fw.data_addr = bce_TXP_b06FwDataAddr;
	fw.data_len = bce_TXP_b06FwDataLen;
	fw.data_index = 0;
	fw.data = bce_TXP_b06FwData;

	fw.sbss_addr = bce_TXP_b06FwSbssAddr;
	fw.sbss_len = bce_TXP_b06FwSbssLen;
	fw.sbss_index = 0;
	fw.sbss = bce_TXP_b06FwSbss;

	fw.bss_addr = bce_TXP_b06FwBssAddr;
	fw.bss_len = bce_TXP_b06FwBssLen;
	fw.bss_index = 0;
	fw.bss = bce_TXP_b06FwBss;

	fw.rodata_addr = bce_TXP_b06FwRodataAddr;
	fw.rodata_len = bce_TXP_b06FwRodataLen;
	fw.rodata_index = 0;
	fw.rodata = bce_TXP_b06FwRodata;

	DBPRINT(sc, BCE_INFO_RESET, "Loading TX firmware.\n");
	bce_load_cpu_fw(sc, &cpu_reg, &fw);

	/* Initialize the TX Patch-up Processor. */
	cpu_reg.mode = BCE_TPAT_CPU_MODE;
	cpu_reg.mode_value_halt = BCE_TPAT_CPU_MODE_SOFT_HALT;
	cpu_reg.mode_value_sstep = BCE_TPAT_CPU_MODE_STEP_ENA;
	cpu_reg.state = BCE_TPAT_CPU_STATE;
	cpu_reg.state_value_clear = 0xffffff;
	cpu_reg.gpr0 = BCE_TPAT_CPU_REG_FILE;
	cpu_reg.evmask = BCE_TPAT_CPU_EVENT_MASK;
	cpu_reg.pc = BCE_TPAT_CPU_PROGRAM_COUNTER;
	cpu_reg.inst = BCE_TPAT_CPU_INSTRUCTION;
	cpu_reg.bp = BCE_TPAT_CPU_HW_BREAKPOINT;
	cpu_reg.spad_base = BCE_TPAT_SCRATCH;
	cpu_reg.mips_view_base = 0x8000000;

	fw.ver_major = bce_TPAT_b06FwReleaseMajor;
	fw.ver_minor = bce_TPAT_b06FwReleaseMinor;
	fw.ver_fix = bce_TPAT_b06FwReleaseFix;
	fw.start_addr = bce_TPAT_b06FwStartAddr;

	fw.text_addr = bce_TPAT_b06FwTextAddr;
	fw.text_len = bce_TPAT_b06FwTextLen;
	fw.text_index = 0;
	fw.text = bce_TPAT_b06FwText;

	fw.data_addr = bce_TPAT_b06FwDataAddr;
	fw.data_len = bce_TPAT_b06FwDataLen;
	fw.data_index = 0;
	fw.data = bce_TPAT_b06FwData;

	fw.sbss_addr = bce_TPAT_b06FwSbssAddr;
	fw.sbss_len = bce_TPAT_b06FwSbssLen;
	fw.sbss_index = 0;
	fw.sbss = bce_TPAT_b06FwSbss;

	fw.bss_addr = bce_TPAT_b06FwBssAddr;
	fw.bss_len = bce_TPAT_b06FwBssLen;
	fw.bss_index = 0;
	fw.bss = bce_TPAT_b06FwBss;

	fw.rodata_addr = bce_TPAT_b06FwRodataAddr;
	fw.rodata_len = bce_TPAT_b06FwRodataLen;
	fw.rodata_index = 0;
	fw.rodata = bce_TPAT_b06FwRodata;

	DBPRINT(sc, BCE_INFO_RESET, "Loading TPAT firmware.\n");
	bce_load_cpu_fw(sc, &cpu_reg, &fw);

	/* Initialize the Completion Processor. */
	cpu_reg.mode = BCE_COM_CPU_MODE;
	cpu_reg.mode_value_halt = BCE_COM_CPU_MODE_SOFT_HALT;
	cpu_reg.mode_value_sstep = BCE_COM_CPU_MODE_STEP_ENA;
	cpu_reg.state = BCE_COM_CPU_STATE;
	cpu_reg.state_value_clear = 0xffffff;
	cpu_reg.gpr0 = BCE_COM_CPU_REG_FILE;
	cpu_reg.evmask = BCE_COM_CPU_EVENT_MASK;
	cpu_reg.pc = BCE_COM_CPU_PROGRAM_COUNTER;
	cpu_reg.inst = BCE_COM_CPU_INSTRUCTION;
	cpu_reg.bp = BCE_COM_CPU_HW_BREAKPOINT;
	cpu_reg.spad_base = BCE_COM_SCRATCH;
	cpu_reg.mips_view_base = 0x8000000;

	fw.ver_major = bce_COM_b06FwReleaseMajor;
	fw.ver_minor = bce_COM_b06FwReleaseMinor;
	fw.ver_fix = bce_COM_b06FwReleaseFix;
	fw.start_addr = bce_COM_b06FwStartAddr;

	fw.text_addr = bce_COM_b06FwTextAddr;
	fw.text_len = bce_COM_b06FwTextLen;
	fw.text_index = 0;
	fw.text = bce_COM_b06FwText;

	fw.data_addr = bce_COM_b06FwDataAddr;
	fw.data_len = bce_COM_b06FwDataLen;
	fw.data_index = 0;
	fw.data = bce_COM_b06FwData;

	fw.sbss_addr = bce_COM_b06FwSbssAddr;
	fw.sbss_len = bce_COM_b06FwSbssLen;
	fw.sbss_index = 0;
	fw.sbss = bce_COM_b06FwSbss;

	fw.bss_addr = bce_COM_b06FwBssAddr;
	fw.bss_len = bce_COM_b06FwBssLen;
	fw.bss_index = 0;
	fw.bss = bce_COM_b06FwBss;

	fw.rodata_addr = bce_COM_b06FwRodataAddr;
	fw.rodata_len = bce_COM_b06FwRodataLen;
	fw.rodata_index = 0;
	fw.rodata = bce_COM_b06FwRodata;

	DBPRINT(sc, BCE_INFO_RESET, "Loading COM firmware.\n");
	bce_load_cpu_fw(sc, &cpu_reg, &fw);
}


/****************************************************************************/
/* Initialize context memory.                                               */
/*                                                                          */
/* Clears the memory associated with each Context ID (CID).                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_init_context(struct bce_softc *sc)
{
	u32 vcid;

	vcid = 96;
	while (vcid) {
		u32 vcid_addr, pcid_addr, offset;

		vcid--;

   		vcid_addr = GET_CID_ADDR(vcid);
		pcid_addr = vcid_addr;

		REG_WR(sc, BCE_CTX_VIRT_ADDR, 0x00);
		REG_WR(sc, BCE_CTX_PAGE_TBL, pcid_addr);

		/* Zero out the context. */
		for (offset = 0; offset < PHY_CTX_SIZE; offset += 4) {
			CTX_WR(sc, 0x00, offset, 0);
		}

		REG_WR(sc, BCE_CTX_VIRT_ADDR, vcid_addr);
		REG_WR(sc, BCE_CTX_PAGE_TBL, pcid_addr);
	}
}


/****************************************************************************/
/* Fetch the permanent MAC address of the controller.                       */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_get_mac_addr(struct bce_softc *sc)
{
	u32 mac_lo = 0, mac_hi = 0;

	/*
	 * The NetXtreme II bootcode populates various NIC
	 * power-on and runtime configuration items in a
	 * shared memory area.  The factory configured MAC
	 * address is available from both NVRAM and the
	 * shared memory area so we'll read the value from
	 * shared memory for speed.
	 */

	mac_hi = REG_RD_IND(sc, sc->bce_shmem_base +
		BCE_PORT_HW_CFG_MAC_UPPER);
	mac_lo = REG_RD_IND(sc, sc->bce_shmem_base +
		BCE_PORT_HW_CFG_MAC_LOWER);

	if ((mac_lo == 0) && (mac_hi == 0)) {
		BCE_PRINTF(sc, "%s(%d): Invalid Ethernet address!\n", 
			__FILE__, __LINE__);
	} else {
		sc->arpcom.ac_enaddr[0] = (u_char)(mac_hi >> 8);
		sc->arpcom.ac_enaddr[1] = (u_char)(mac_hi >> 0);
		sc->arpcom.ac_enaddr[2] = (u_char)(mac_lo >> 24);
		sc->arpcom.ac_enaddr[3] = (u_char)(mac_lo >> 16);
		sc->arpcom.ac_enaddr[4] = (u_char)(mac_lo >> 8);
		sc->arpcom.ac_enaddr[5] = (u_char)(mac_lo >> 0);
	}

	DBPRINT(sc, BCE_INFO, "Permanent Ethernet address = %6D\n", sc->arpcom.ac_enaddr, ":");
}


/****************************************************************************/
/* Program the MAC address.                                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_set_mac_addr(struct bce_softc *sc)
{
	u32 val;
	u8 *mac_addr = sc->arpcom.ac_enaddr;

	DBPRINT(sc, BCE_INFO, "Setting Ethernet address = %6D\n", sc->arpcom.ac_enaddr, ":");

	val = (mac_addr[0] << 8) | mac_addr[1];

	REG_WR(sc, BCE_EMAC_MAC_MATCH0, val);

	val = (mac_addr[2] << 24) | (mac_addr[3] << 16) |
		(mac_addr[4] << 8) | mac_addr[5];

	REG_WR(sc, BCE_EMAC_MAC_MATCH1, val);
}


/****************************************************************************/
/* Stop the controller.                                                     */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_stop(struct bce_softc *sc)
{
	struct ifnet *ifp;
	struct ifmedia_entry *ifm;
	struct mii_data *mii = NULL;
	int mtmp, itmp;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	ifp = &sc->arpcom.ac_if;

	mii = device_get_softc(sc->bce_miibus);

	callout_stop(&sc->bce_stat_ch);

	/* Disable the transmit/receive blocks. */
	REG_WR(sc, BCE_MISC_ENABLE_CLR_BITS, 0x5ffffff);
	REG_RD(sc, BCE_MISC_ENABLE_CLR_BITS);
	DELAY(20);

	bce_disable_intr(sc);

	/* Tell firmware that the driver is going away. */
	bce_reset(sc, BCE_DRV_MSG_CODE_SUSPEND_NO_WOL);

	/* Free the RX lists. */
	bce_free_rx_chain(sc);

	/* Free TX buffers. */
	bce_free_tx_chain(sc);

	/*
	 * Isolate/power down the PHY, but leave the media selection
	 * unchanged so that things will be put back to normal when
	 * we bring the interface back up.
	 */

	itmp = ifp->if_flags;
	ifp->if_flags |= IFF_UP;
	/*
	 * If we are called from bce_detach(), mii is already NULL.
	 */
	if (mii != NULL) {
		ifm = mii->mii_media.ifm_cur;
		mtmp = ifm->ifm_media;
		ifm->ifm_media = IFM_ETHER | IFM_NONE;
		mii_mediachg(mii);
		ifm->ifm_media = mtmp;
	}

	ifp->if_flags = itmp;
	ifp->if_timer = 0;

	sc->bce_link = 0;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

}


static int
bce_reset(struct bce_softc *sc, u32 reset_code)
{
	u32 val;
	int i, rc = 0;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/* Wait for pending PCI transactions to complete. */
	REG_WR(sc, BCE_MISC_ENABLE_CLR_BITS,
	       BCE_MISC_ENABLE_CLR_BITS_TX_DMA_ENABLE |
	       BCE_MISC_ENABLE_CLR_BITS_DMA_ENGINE_ENABLE |
	       BCE_MISC_ENABLE_CLR_BITS_RX_DMA_ENABLE |
	       BCE_MISC_ENABLE_CLR_BITS_HOST_COALESCE_ENABLE);
	val = REG_RD(sc, BCE_MISC_ENABLE_CLR_BITS);
	DELAY(5);

	/* Assume bootcode is running. */
	sc->bce_fw_timed_out = 0;

	/* Give the firmware a chance to prepare for the reset. */
	rc = bce_fw_sync(sc, BCE_DRV_MSG_DATA_WAIT0 | reset_code);
	if (rc)
		goto bce_reset_exit;

	/* Set a firmware reminder that this is a soft reset. */
	REG_WR_IND(sc, sc->bce_shmem_base + BCE_DRV_RESET_SIGNATURE,
		   BCE_DRV_RESET_SIGNATURE_MAGIC);

	/* Dummy read to force the chip to complete all current transactions. */
	val = REG_RD(sc, BCE_MISC_ID);

	/* Chip reset. */
	val = BCE_PCICFG_MISC_CONFIG_CORE_RST_REQ |
	      BCE_PCICFG_MISC_CONFIG_REG_WINDOW_ENA |
	      BCE_PCICFG_MISC_CONFIG_TARGET_MB_WORD_SWAP;
	REG_WR(sc, BCE_PCICFG_MISC_CONFIG, val);

	/* Allow up to 30us for reset to complete. */
	for (i = 0; i < 10; i++) {
		val = REG_RD(sc, BCE_PCICFG_MISC_CONFIG);
		if ((val & (BCE_PCICFG_MISC_CONFIG_CORE_RST_REQ |
			    BCE_PCICFG_MISC_CONFIG_CORE_RST_BSY)) == 0) {
			break;
		}
		DELAY(10);
	}

	/* Check that reset completed successfully. */
	if (val & (BCE_PCICFG_MISC_CONFIG_CORE_RST_REQ |
		   BCE_PCICFG_MISC_CONFIG_CORE_RST_BSY)) {
		BCE_PRINTF(sc, "%s(%d): Reset failed!\n", 
			__FILE__, __LINE__);
		rc = EBUSY;
		goto bce_reset_exit;
	}

	/* Make sure byte swapping is properly configured. */
	val = REG_RD(sc, BCE_PCI_SWAP_DIAG0);
	if (val != 0x01020304) {
		BCE_PRINTF(sc, "%s(%d): Byte swap is incorrect!\n", 
			__FILE__, __LINE__);
		rc = ENODEV;
		goto bce_reset_exit;
	}

	/* Just completed a reset, assume that firmware is running again. */
	sc->bce_fw_timed_out = 0;

	/* Wait for the firmware to finish its initialization. */
	rc = bce_fw_sync(sc, BCE_DRV_MSG_DATA_WAIT1 | reset_code);
	if (rc)
		BCE_PRINTF(sc, "%s(%d): Firmware did not complete initialization!\n",
			__FILE__, __LINE__);

bce_reset_exit:
	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	return (rc);
}


static int
bce_chipinit(struct bce_softc *sc)
{
	u32 val;
	int rc = 0;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/* Make sure the interrupt is not active. */
	REG_WR(sc, BCE_PCICFG_INT_ACK_CMD, BCE_PCICFG_INT_ACK_CMD_MASK_INT);

	/* Initialize DMA byte/word swapping, configure the number of DMA  */
	/* channels and PCI clock compensation delay.                      */
	val = BCE_DMA_CONFIG_DATA_BYTE_SWAP |
	      BCE_DMA_CONFIG_DATA_WORD_SWAP |
#if BYTE_ORDER == BIG_ENDIAN
	      BCE_DMA_CONFIG_CNTL_BYTE_SWAP |
#endif
	      BCE_DMA_CONFIG_CNTL_WORD_SWAP |
	      DMA_READ_CHANS << 12 |
	      DMA_WRITE_CHANS << 16;

	val |= (0x2 << 20) | BCE_DMA_CONFIG_CNTL_PCI_COMP_DLY;

	if ((sc->bce_flags & BCE_PCIX_FLAG) && (sc->bus_speed_mhz == 133))
		val |= BCE_DMA_CONFIG_PCI_FAST_CLK_CMP;

	/*
	 * This setting resolves a problem observed on certain Intel PCI
	 * chipsets that cannot handle multiple outstanding DMA operations.
	 * See errata E9_5706A1_65.
	 */
	if ((BCE_CHIP_NUM(sc) == BCE_CHIP_NUM_5706) &&
	    (BCE_CHIP_ID(sc) != BCE_CHIP_ID_5706_A0) &&
	    !(sc->bce_flags & BCE_PCIX_FLAG))
		val |= BCE_DMA_CONFIG_CNTL_PING_PONG_DMA;

	REG_WR(sc, BCE_DMA_CONFIG, val);

	/* Clear the PCI-X relaxed ordering bit. See errata E3_5708CA0_570. */
	if (sc->bce_flags & BCE_PCIX_FLAG) {
		u16 val;

		val = pci_read_config(sc->bce_dev, BCE_PCI_PCIX_CMD, 2);
		pci_write_config(sc->bce_dev, BCE_PCI_PCIX_CMD, val & ~0x2, 2);
	}

	/* Enable the RX_V2P and Context state machines before access. */
	REG_WR(sc, BCE_MISC_ENABLE_SET_BITS,
	       BCE_MISC_ENABLE_SET_BITS_HOST_COALESCE_ENABLE |
	       BCE_MISC_ENABLE_STATUS_BITS_RX_V2P_ENABLE |
	       BCE_MISC_ENABLE_STATUS_BITS_CONTEXT_ENABLE);

	/* Initialize context mapping and zero out the quick contexts. */
	bce_init_context(sc);

	/* Initialize the on-boards CPUs */
	bce_init_cpus(sc);

	/* Prepare NVRAM for access. */
	if (bce_init_nvram(sc)) {
		rc = ENODEV;
		goto bce_chipinit_exit;
	}

	/* Set the kernel bypass block size */
	val = REG_RD(sc, BCE_MQ_CONFIG);
	val &= ~BCE_MQ_CONFIG_KNL_BYP_BLK_SIZE;
	val |= BCE_MQ_CONFIG_KNL_BYP_BLK_SIZE_256;
	REG_WR(sc, BCE_MQ_CONFIG, val);

	val = 0x10000 + (MAX_CID_CNT * MB_KERNEL_CTX_SIZE);
	REG_WR(sc, BCE_MQ_KNL_BYP_WIND_START, val);
	REG_WR(sc, BCE_MQ_KNL_WIND_END, val);

	val = (BCM_PAGE_BITS - 8) << 24;
	REG_WR(sc, BCE_RV2P_CONFIG, val);

	/* Configure page size. */
	val = REG_RD(sc, BCE_TBDR_CONFIG);
	val &= ~BCE_TBDR_CONFIG_PAGE_SIZE;
	val |= (BCM_PAGE_BITS - 8) << 24 | 0x40;
	REG_WR(sc, BCE_TBDR_CONFIG, val);

bce_chipinit_exit:
	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	return(rc);
}


/****************************************************************************/
/* Initialize the controller in preparation to send/receive traffic.        */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_blockinit(struct bce_softc *sc)
{
	u32 reg, val;
	int rc = 0;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/* Load the hardware default MAC address. */
	bce_set_mac_addr(sc);

	/* Set the Ethernet backoff seed value */
	val = sc->arpcom.ac_enaddr[0]         + (sc->arpcom.ac_enaddr[1] << 8) +
	      (sc->arpcom.ac_enaddr[2] << 16) + (sc->arpcom.ac_enaddr[3]     ) +
	      (sc->arpcom.ac_enaddr[4] << 8)  + (sc->arpcom.ac_enaddr[5] << 16);
	REG_WR(sc, BCE_EMAC_BACKOFF_SEED, val);

	sc->last_status_idx = 0;
	sc->rx_mode = BCE_EMAC_RX_MODE_SORT_MODE;

	/* Set up link change interrupt generation. */
	REG_WR(sc, BCE_EMAC_ATTENTION_ENA, BCE_EMAC_ATTENTION_ENA_LINK);

	/* Program the physical address of the status block. */
	REG_WR(sc, BCE_HC_STATUS_ADDR_L,
		BCE_ADDR_LO(sc->status_block_paddr));
	REG_WR(sc, BCE_HC_STATUS_ADDR_H,
		BCE_ADDR_HI(sc->status_block_paddr));

	/* Program the physical address of the statistics block. */
	REG_WR(sc, BCE_HC_STATISTICS_ADDR_L,
		BCE_ADDR_LO(sc->stats_block_paddr));
	REG_WR(sc, BCE_HC_STATISTICS_ADDR_H,
		BCE_ADDR_HI(sc->stats_block_paddr));

	/* Program various host coalescing parameters. */
	REG_WR(sc, BCE_HC_TX_QUICK_CONS_TRIP,
		(sc->bce_tx_quick_cons_trip_int << 16) | sc->bce_tx_quick_cons_trip);
	REG_WR(sc, BCE_HC_RX_QUICK_CONS_TRIP,
		(sc->bce_rx_quick_cons_trip_int << 16) | sc->bce_rx_quick_cons_trip);
	REG_WR(sc, BCE_HC_COMP_PROD_TRIP,
		(sc->bce_comp_prod_trip_int << 16) | sc->bce_comp_prod_trip);
	REG_WR(sc, BCE_HC_TX_TICKS,
		(sc->bce_tx_ticks_int << 16) | sc->bce_tx_ticks);
	REG_WR(sc, BCE_HC_RX_TICKS,
		(sc->bce_rx_ticks_int << 16) | sc->bce_rx_ticks);
	REG_WR(sc, BCE_HC_COM_TICKS,
		(sc->bce_com_ticks_int << 16) | sc->bce_com_ticks);
	REG_WR(sc, BCE_HC_CMD_TICKS,
		(sc->bce_cmd_ticks_int << 16) | sc->bce_cmd_ticks);
	REG_WR(sc, BCE_HC_STATS_TICKS,
		(sc->bce_stats_ticks & 0xffff00));
	REG_WR(sc, BCE_HC_STAT_COLLECT_TICKS,
		0xbb8);  /* 3ms */
	REG_WR(sc, BCE_HC_CONFIG,
		(BCE_HC_CONFIG_RX_TMR_MODE | BCE_HC_CONFIG_TX_TMR_MODE |
		BCE_HC_CONFIG_COLLECT_STATS));

	/* Clear the internal statistics counters. */
	REG_WR(sc, BCE_HC_COMMAND, BCE_HC_COMMAND_CLR_STAT_NOW);

	/* Verify that bootcode is running. */
	reg = REG_RD_IND(sc, sc->bce_shmem_base + BCE_DEV_INFO_SIGNATURE);

	DBRUNIF(DB_RANDOMTRUE(bce_debug_bootcode_running_failure),
		BCE_PRINTF(sc, "%s(%d): Simulating bootcode failure.\n",
			__FILE__, __LINE__);
		reg = 0);

	if ((reg & BCE_DEV_INFO_SIGNATURE_MAGIC_MASK) !=
	    BCE_DEV_INFO_SIGNATURE_MAGIC) {
		BCE_PRINTF(sc, "%s(%d): Bootcode not running! Found: 0x%08X, "
			"Expected: 08%08X\n", __FILE__, __LINE__,
			(reg & BCE_DEV_INFO_SIGNATURE_MAGIC_MASK),
			BCE_DEV_INFO_SIGNATURE_MAGIC);
		rc = ENODEV;
		goto bce_blockinit_exit;
	}

	/* Check if any management firmware is running. */
	reg = REG_RD_IND(sc, sc->bce_shmem_base + BCE_PORT_FEATURE);
	if (reg & (BCE_PORT_FEATURE_ASF_ENABLED | BCE_PORT_FEATURE_IMD_ENABLED)) {
		DBPRINT(sc, BCE_INFO, "Management F/W Enabled.\n");
		sc->bce_flags |= BCE_MFW_ENABLE_FLAG;
	}

	sc->bce_fw_ver = REG_RD_IND(sc, sc->bce_shmem_base + BCE_DEV_INFO_BC_REV);
	DBPRINT(sc, BCE_INFO, "bootcode rev = 0x%08X\n", sc->bce_fw_ver);

	/* Allow bootcode to apply any additional fixes before enabling MAC. */
	rc = bce_fw_sync(sc, BCE_DRV_MSG_DATA_WAIT2 | BCE_DRV_MSG_CODE_RESET);

	/* Enable link state change interrupt generation. */
	REG_WR(sc, BCE_HC_ATTN_BITS_ENABLE, STATUS_ATTN_BITS_LINK_STATE);

	/* Enable all remaining blocks in the MAC. */
	REG_WR(sc, BCE_MISC_ENABLE_SET_BITS, 0x5ffffff);
	REG_RD(sc, BCE_MISC_ENABLE_SET_BITS);
	DELAY(20);

bce_blockinit_exit:
	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	return (rc);
}


/****************************************************************************/
/* Encapsulate an mbuf cluster into the rx_bd chain.                        */
/*                                                                          */
/* The NetXtreme II can support Jumbo frames by using multiple rx_bd's.     */
/* This routine will map an mbuf cluster into 1 or more rx_bd's as          */
/* necessary.                                                               */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_get_buf(struct bce_softc *sc, struct mbuf *m, u16 *prod, u16 *chain_prod, 
	u32 *prod_bseq)
{
	bus_dmamap_t		map;
	struct bce_dmamap_arg map_arg;
	struct mbuf *m_new = NULL;
	int error, rc = 0;

	DBPRINT(sc, (BCE_VERBOSE_RESET | BCE_VERBOSE_RECV), "Entering %s()\n", 
		__FUNCTION__);

	/* Make sure the inputs are valid. */
	DBRUNIF((*chain_prod > MAX_RX_BD),
		BCE_PRINTF(sc, "%s(%d): RX producer out of range: 0x%04X > 0x%04X\n",
		__FILE__, __LINE__, *chain_prod, (u16) MAX_RX_BD));

	DBPRINT(sc, BCE_VERBOSE_RECV, "%s(enter): prod = 0x%04X, chain_prod = 0x%04X, "
		"prod_bseq = 0x%08X\n", __FUNCTION__, *prod, *chain_prod, *prod_bseq);

	if (m == NULL) {

		DBRUNIF(DB_RANDOMTRUE(bce_debug_mbuf_allocation_failure),
			BCE_PRINTF(sc, "%s(%d): Simulating mbuf allocation failure.\n", 
				__FILE__, __LINE__);
			sc->mbuf_alloc_failed++;
			rc = ENOBUFS;
			goto bce_get_buf_exit);

		/* This is a new mbuf allocation. */
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {

			DBPRINT(sc, BCE_WARN, "%s(%d): RX mbuf header allocation failed!\n", 
				__FILE__, __LINE__);

			DBRUNIF(1, sc->mbuf_alloc_failed++);

			rc = ENOBUFS;
			goto bce_get_buf_exit;
		}

		DBRUNIF(1, sc->rx_mbuf_alloc++);
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {

			DBPRINT(sc, BCE_WARN, "%s(%d): RX mbuf chain allocation failed!\n", 
				__FILE__, __LINE__);
			
			m_freem(m_new);

			DBRUNIF(1, sc->rx_mbuf_alloc--);
			DBRUNIF(1, sc->mbuf_alloc_failed++);

			rc = ENOBUFS;
			goto bce_get_buf_exit;
		}
			
		m_new->m_len = m_new->m_pkthdr.len = sc->mbuf_alloc_size;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = sc->mbuf_alloc_size;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	/* Map the mbuf cluster into device memory. */
	map = sc->rx_mbuf_map[*chain_prod];
	map_arg.sc         = sc;
	map_arg.prod       = *prod;
	map_arg.chain_prod = *chain_prod;
	map_arg.prod_bseq  = *prod_bseq;
	map_arg.maxsegs    = sc->free_rx_bd; /* XXX: 4? */
	error = bus_dmamap_load_mbuf(sc->rx_mbuf_tag, map, m_new,
	    bce_dma_map_rx_desc, &map_arg, BUS_DMA_NOWAIT);

	if (error) {
		BCE_PRINTF(sc, "%s(%d): Error mapping mbuf into RX chain!\n",
			__FILE__, __LINE__);

		m_freem(m_new);

		DBRUNIF(1, sc->rx_mbuf_alloc--);

		rc = ENOBUFS;
		goto bce_get_buf_exit;
	}

	/* Watch for overflow. */
	DBRUNIF((sc->free_rx_bd > USABLE_RX_BD),
		BCE_PRINTF(sc, "%s(%d): Too many free rx_bd (0x%04X > 0x%04X)!\n", 
			__FILE__, __LINE__, sc->free_rx_bd, (u16) USABLE_RX_BD));

	DBRUNIF((sc->free_rx_bd < sc->rx_low_watermark), 
		sc->rx_low_watermark = sc->free_rx_bd);

	/* Update indices from the callback */
	*prod       = map_arg.prod;
	*chain_prod = map_arg.chain_prod;
	*prod_bseq  = map_arg.prod_bseq;

	/* Save the mbuf and update our counter. */
	sc->rx_mbuf_ptr[*chain_prod] = m_new;
	sc->free_rx_bd -= map_arg.maxsegs;

bce_get_buf_exit:
	DBPRINT(sc, (BCE_VERBOSE_RESET | BCE_VERBOSE_RECV), "Exiting %s()\n", 
		__FUNCTION__);

	return(rc);
}


/****************************************************************************/
/* Allocate memory and initialize the TX data structures.                   */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_init_tx_chain(struct bce_softc *sc)
{
	struct tx_bd *txbd;
	u32 val;
	int i, rc = 0;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/* Set the initial TX producer/consumer indices. */
	sc->tx_prod        = 0;
	sc->tx_cons        = 0;
	sc->tx_prod_bseq   = 0;
	sc->used_tx_bd = 0;
	DBRUNIF(1, sc->tx_hi_watermark = USABLE_TX_BD);

	/*
	 * The NetXtreme II supports a linked-list structre called
	 * a Buffer Descriptor Chain (or BD chain).  A BD chain
	 * consists of a series of 1 or more chain pages, each of which
	 * consists of a fixed number of BD entries.
	 * The last BD entry on each page is a pointer to the next page
	 * in the chain, and the last pointer in the BD chain
	 * points back to the beginning of the chain.
	 */

	/* Set the TX next pointer chain entries. */
	for (i = 0; i < TX_PAGES; i++) {
		int j;

		txbd = &sc->tx_bd_chain[i][USABLE_TX_BD_PER_PAGE];

		/* Check if we've reached the last page. */
		if (i == (TX_PAGES - 1))
			j = 0;
		else
			j = i + 1;

		txbd->tx_bd_haddr_hi = htole32(BCE_ADDR_HI(sc->tx_bd_chain_paddr[j]));
		txbd->tx_bd_haddr_lo = htole32(BCE_ADDR_LO(sc->tx_bd_chain_paddr[j]));
	}

	/*
	 * Initialize the context ID for an L2 TX chain.
	 */
	val = BCE_L2CTX_TYPE_TYPE_L2;
	val |= BCE_L2CTX_TYPE_SIZE_L2;
	CTX_WR(sc, GET_CID_ADDR(TX_CID), BCE_L2CTX_TYPE, val);

	val = BCE_L2CTX_CMD_TYPE_TYPE_L2 | (8 << 16);
	CTX_WR(sc, GET_CID_ADDR(TX_CID), BCE_L2CTX_CMD_TYPE, val);

	/* Point the hardware to the first page in the chain. */
	val = BCE_ADDR_HI(sc->tx_bd_chain_paddr[0]);
	CTX_WR(sc, GET_CID_ADDR(TX_CID), BCE_L2CTX_TBDR_BHADDR_HI, val);
	val = BCE_ADDR_LO(sc->tx_bd_chain_paddr[0]);
	CTX_WR(sc, GET_CID_ADDR(TX_CID), BCE_L2CTX_TBDR_BHADDR_LO, val);

	DBRUN(BCE_VERBOSE_SEND, bce_dump_tx_chain(sc, 0, TOTAL_TX_BD));

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	return(rc);
}


/****************************************************************************/
/* Free memory and clear the TX data structures.                            */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_free_tx_chain(struct bce_softc *sc)
{
	int i;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/* Unmap, unload, and free any mbufs still in the TX mbuf chain. */
	for (i = 0; i < TOTAL_TX_BD; i++) {
		if (sc->tx_mbuf_ptr[i] != NULL) {
			if (sc->tx_mbuf_map != NULL)
				bus_dmamap_sync(sc->tx_mbuf_tag, sc->tx_mbuf_map[i],
					BUS_DMASYNC_POSTWRITE);
			m_freem(sc->tx_mbuf_ptr[i]);
			sc->tx_mbuf_ptr[i] = NULL;
			DBRUNIF(1, sc->tx_mbuf_alloc--);
		}			
	}

	/* Clear each TX chain page. */
	for (i = 0; i < TX_PAGES; i++)
		bzero((char *)sc->tx_bd_chain[i], BCE_TX_CHAIN_PAGE_SZ);

	/* Check if we lost any mbufs in the process. */
	DBRUNIF((sc->tx_mbuf_alloc),
		BCE_PRINTF(sc, "%s(%d): Memory leak! Lost %d mbufs "
			"from tx chain!\n",
			__FILE__, __LINE__, sc->tx_mbuf_alloc));

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);
}


/****************************************************************************/
/* Allocate memory and initialize the RX data structures.                   */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_init_rx_chain(struct bce_softc *sc)
{
	struct rx_bd *rxbd;
	int i, rc = 0;
	u16 prod, chain_prod;
	u32 prod_bseq, val;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/* Initialize the RX producer and consumer indices. */
	sc->rx_prod        = 0;
	sc->rx_cons        = 0;
	sc->rx_prod_bseq   = 0;
	sc->free_rx_bd     = BCE_RX_SLACK_SPACE;
	DBRUNIF(1, sc->rx_low_watermark = USABLE_RX_BD);

	/* Initialize the RX next pointer chain entries. */
	for (i = 0; i < RX_PAGES; i++) {
		int j;

		rxbd = &sc->rx_bd_chain[i][USABLE_RX_BD_PER_PAGE];

		/* Check if we've reached the last page. */
		if (i == (RX_PAGES - 1))
			j = 0;
		else
			j = i + 1;

		/* Setup the chain page pointers. */
		rxbd->rx_bd_haddr_hi = htole32(BCE_ADDR_HI(sc->rx_bd_chain_paddr[j]));
		rxbd->rx_bd_haddr_lo = htole32(BCE_ADDR_LO(sc->rx_bd_chain_paddr[j]));
	}

	/* Initialize the context ID for an L2 RX chain. */
	val = BCE_L2CTX_CTX_TYPE_CTX_BD_CHN_TYPE_VALUE;
	val |= BCE_L2CTX_CTX_TYPE_SIZE_L2;
	val |= 0x02 << 8;
	CTX_WR(sc, GET_CID_ADDR(RX_CID), BCE_L2CTX_CTX_TYPE, val);

	/* Point the hardware to the first page in the chain. */
	val = BCE_ADDR_HI(sc->rx_bd_chain_paddr[0]);
	CTX_WR(sc, GET_CID_ADDR(RX_CID), BCE_L2CTX_NX_BDHADDR_HI, val);
	val = BCE_ADDR_LO(sc->rx_bd_chain_paddr[0]);
	CTX_WR(sc, GET_CID_ADDR(RX_CID), BCE_L2CTX_NX_BDHADDR_LO, val);

	/* Allocate mbuf clusters for the rx_bd chain. */
	prod = prod_bseq = 0;
	while (prod < BCE_RX_SLACK_SPACE) {
		chain_prod = RX_CHAIN_IDX(prod);
		if (bce_get_buf(sc, NULL, &prod, &chain_prod, &prod_bseq)) {
			BCE_PRINTF(sc, "%s(%d): Error filling RX chain: rx_bd[0x%04X]!\n",
				__FILE__, __LINE__, chain_prod);
			rc = ENOBUFS;
			break;
		}
		prod = NEXT_RX_BD(prod);
	}

	/* Save the RX chain producer index. */
	sc->rx_prod      = prod;
	sc->rx_prod_bseq = prod_bseq;

	for (i = 0; i < RX_PAGES; i++) {
		bus_dmamap_sync(
			sc->rx_bd_chain_tag,
	    	sc->rx_bd_chain_map[i],
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}

	/* Tell the chip about the waiting rx_bd's. */
	REG_WR16(sc, MB_RX_CID_ADDR + BCE_L2CTX_HOST_BDIDX, sc->rx_prod);
	REG_WR(sc, MB_RX_CID_ADDR + BCE_L2CTX_HOST_BSEQ, sc->rx_prod_bseq);

	DBRUN(BCE_VERBOSE_RECV, bce_dump_rx_chain(sc, 0, TOTAL_RX_BD));

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	return(rc);
}


/****************************************************************************/
/* Free memory and clear the RX data structures.                            */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_free_rx_chain(struct bce_softc *sc)
{
	int i;

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	/* Free any mbufs still in the RX mbuf chain. */
	for (i = 0; i < TOTAL_RX_BD; i++) {
		if (sc->rx_mbuf_ptr[i] != NULL) {
			if (sc->rx_mbuf_map[i] != NULL)
				bus_dmamap_sync(sc->rx_mbuf_tag, sc->rx_mbuf_map[i],
					BUS_DMASYNC_POSTREAD);
			m_freem(sc->rx_mbuf_ptr[i]);
			sc->rx_mbuf_ptr[i] = NULL;
			DBRUNIF(1, sc->rx_mbuf_alloc--);
		}
	}

	/* Clear each RX chain page. */
	for (i = 0; i < RX_PAGES; i++)
		bzero((char *)sc->rx_bd_chain[i], BCE_RX_CHAIN_PAGE_SZ);

	/* Check if we lost any mbufs in the process. */
	DBRUNIF((sc->rx_mbuf_alloc),
		BCE_PRINTF(sc, "%s(%d): Memory leak! Lost %d mbufs from rx chain!\n",
			__FILE__, __LINE__, sc->rx_mbuf_alloc));

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);
}


/****************************************************************************/
/* Set media options.                                                       */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_ifmedia_upd(struct ifnet *ifp)
{
	struct bce_softc *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;
	int rc = 0;

	sc = ifp->if_softc;
	ifm = &sc->bce_ifmedia;

	/* DRC - ToDo: Add SerDes support. */

	mii = device_get_softc(sc->bce_miibus);
	sc->bce_link = 0;
	if (mii->mii_instance) {
		struct mii_softc *miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		    miisc = LIST_NEXT(miisc, mii_list))
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return(rc);
}


/****************************************************************************/
/* Reports current media status.                                            */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct bce_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;

	mii = device_get_softc(sc->bce_miibus);

	/* DRC - ToDo: Add SerDes support. */

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}


/****************************************************************************/
/* Handles PHY generated interrupt events.                                  */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_phy_intr(struct bce_softc *sc)
{
	u32 new_link_state, old_link_state;

	new_link_state = sc->status_block->status_attn_bits &
		STATUS_ATTN_BITS_LINK_STATE;
	old_link_state = sc->status_block->status_attn_bits_ack &
		STATUS_ATTN_BITS_LINK_STATE;

	/* Handle any changes if the link state has changed. */
	if (new_link_state != old_link_state) {

		DBRUN(BCE_VERBOSE_INTR, bce_dump_status_block(sc));

		sc->bce_link = 0;
		callout_stop(&sc->bce_stat_ch);
		bce_tick(sc);

		/* Update the status_attn_bits_ack field in the status block. */
		if (new_link_state) {
			REG_WR(sc, BCE_PCICFG_STATUS_BIT_SET_CMD,
				STATUS_ATTN_BITS_LINK_STATE);
			DBPRINT(sc, BCE_INFO, "Link is now UP.\n");
		}
		else {
			REG_WR(sc, BCE_PCICFG_STATUS_BIT_CLEAR_CMD,
				STATUS_ATTN_BITS_LINK_STATE);
			DBPRINT(sc, BCE_INFO, "Link is now DOWN.\n");
		}

	}

	/* Acknowledge the link change interrupt. */
	REG_WR(sc, BCE_EMAC_STATUS, BCE_EMAC_STATUS_LINK_CHANGE);
}


/****************************************************************************/
/* Handles received frame interrupt events.                                 */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_rx_intr(struct bce_softc *sc)
{
	struct status_block *sblk = sc->status_block;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	u16 hw_cons, sw_cons, sw_chain_cons, sw_prod, sw_chain_prod;
	u32 sw_prod_bseq;
	struct l2_fhdr *l2fhdr;
	int i;

	DBRUNIF(1, sc->rx_interrupts++);

	/* Prepare the RX chain pages to be accessed by the host CPU. */
	for (i = 0; i < RX_PAGES; i++)
		bus_dmamap_sync(sc->rx_bd_chain_tag,
		    sc->rx_bd_chain_map[i], BUS_DMASYNC_POSTWRITE);

	/* Get the hardware's view of the RX consumer index. */
	hw_cons = sc->hw_rx_cons = sblk->status_rx_quick_consumer_index0;
	if ((hw_cons & USABLE_RX_BD_PER_PAGE) == USABLE_RX_BD_PER_PAGE)
		hw_cons++;

	/* Get working copies of the driver's view of the RX indices. */
	sw_cons = sc->rx_cons;
	sw_prod = sc->rx_prod;
	sw_prod_bseq = sc->rx_prod_bseq;

	DBPRINT(sc, BCE_INFO_RECV, "%s(enter): sw_prod = 0x%04X, "
		"sw_cons = 0x%04X, sw_prod_bseq = 0x%08X\n",
		__FUNCTION__, sw_prod, sw_cons, 
		sw_prod_bseq);

	/* Prevent speculative reads from getting ahead of the status block. */
	bus_space_barrier(sc->bce_btag, sc->bce_bhandle, 0, 0, 
		BUS_SPACE_BARRIER_READ);

	DBRUNIF((sc->free_rx_bd < sc->rx_low_watermark),
		sc->rx_low_watermark = sc->free_rx_bd);

	/* 
	 * Scan through the receive chain as long 
	 * as there is work to do.
	 */
	while (sw_cons != hw_cons) {
		struct mbuf *m;
		struct rx_bd *rxbd;
		unsigned int len;
		u32 status;

		/* Convert the producer/consumer indices to an actual rx_bd index. */
		sw_chain_cons = RX_CHAIN_IDX(sw_cons);
		sw_chain_prod = RX_CHAIN_IDX(sw_prod);

		/* Get the used rx_bd. */
		rxbd = &sc->rx_bd_chain[RX_PAGE(sw_chain_cons)][RX_IDX(sw_chain_cons)];
		sc->free_rx_bd++;
	
		DBRUN(BCE_VERBOSE_RECV, 
			BCE_PRINTF(sc, "%s(): ", __FUNCTION__); 
			bce_dump_rxbd(sc, sw_chain_cons, rxbd));

#ifdef DEVICE_POLLING
		if (ifp->if_ipending & IFF_POLLING) {
			if (sc->bce_rxcycles <= 0)
				break;
			sc->bce_rxcycles--;
		}
#endif

		/* The mbuf is stored with the last rx_bd entry of a packet. */
		if (sc->rx_mbuf_ptr[sw_chain_cons] != NULL) {

			/* Validate that this is the last rx_bd. */
			DBRUNIF((!(rxbd->rx_bd_flags & RX_BD_FLAGS_END)),
				BCE_PRINTF(sc, "%s(%d): Unexpected mbuf found in rx_bd[0x%04X]!\n",
				__FILE__, __LINE__, sw_chain_cons);
				bce_breakpoint(sc));

			/* DRC - ToDo: If the received packet is small, say less */
			/*             than 128 bytes, allocate a new mbuf here, */
			/*             copy the data to that mbuf, and recycle   */
			/*             the mapped jumbo frame.                   */

			/* Unmap the mbuf from DMA space. */
			bus_dmamap_sync(sc->rx_mbuf_tag, 
			    sc->rx_mbuf_map[sw_chain_cons],
		    	BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->rx_mbuf_tag,
			    sc->rx_mbuf_map[sw_chain_cons]);

			/* Remove the mbuf from the driver's chain. */
			m = sc->rx_mbuf_ptr[sw_chain_cons];
			sc->rx_mbuf_ptr[sw_chain_cons] = NULL;

			/*
			 * Frames received on the NetXteme II are prepended 
			 * with the l2_fhdr structure which provides status
			 * information about the received frame (including
			 * VLAN tags and checksum info) and are also
			 * automatically adjusted to align the IP header
			 * (i.e. two null bytes are inserted before the 
			 * Ethernet header).
			 */
			l2fhdr = mtod(m, struct l2_fhdr *);

			len    = l2fhdr->l2_fhdr_pkt_len;
			status = l2fhdr->l2_fhdr_status;

			DBRUNIF(DB_RANDOMTRUE(bce_debug_l2fhdr_status_check),
				BCE_PRINTF(sc, "Simulating l2_fhdr status error.\n");
				status = status | L2_FHDR_ERRORS_PHY_DECODE);

			/* Watch for unusual sized frames. */
			DBRUNIF(((len < BCE_MIN_MTU) || (len > BCE_MAX_JUMBO_ETHER_MTU_VLAN)),
				BCE_PRINTF(sc, "%s(%d): Unusual frame size found. "
					"Min(%d), Actual(%d), Max(%d)\n", 
					__FILE__, __LINE__, (int) BCE_MIN_MTU, 
					len, (int) BCE_MAX_JUMBO_ETHER_MTU_VLAN);
				bce_dump_mbuf(sc, m);
		 		bce_breakpoint(sc));

			len -= ETHER_CRC_LEN;

			/* Check the received frame for errors. */
			if (status &  (L2_FHDR_ERRORS_BAD_CRC | 
				L2_FHDR_ERRORS_PHY_DECODE | L2_FHDR_ERRORS_ALIGNMENT | 
				L2_FHDR_ERRORS_TOO_SHORT  | L2_FHDR_ERRORS_GIANT_FRAME)) {

				ifp->if_ierrors++;
				DBRUNIF(1, sc->l2fhdr_status_errors++);

				/* Reuse the mbuf for a new frame. */
				if (bce_get_buf(sc, m, &sw_prod, &sw_chain_prod, &sw_prod_bseq)) {

					DBRUNIF(1, bce_breakpoint(sc));
					panic("bce%d: Can't reuse RX mbuf!\n", sc->bce_unit);

				}
				goto bce_rx_int_next_rx;
			}

			/* 
			 * Get a new mbuf for the rx_bd.   If no new
			 * mbufs are available then reuse the current mbuf,
			 * log an ierror on the interface, and generate
			 * an error in the system log.
			 */
			if (bce_get_buf(sc, NULL, &sw_prod, &sw_chain_prod, &sw_prod_bseq)) {

				DBRUN(BCE_WARN, 
					BCE_PRINTF(sc, "%s(%d): Failed to allocate "
					"new mbuf, incoming frame dropped!\n", 
					__FILE__, __LINE__));

				ifp->if_ierrors++;

				/* Try and reuse the exisitng mbuf. */
				if (bce_get_buf(sc, m, &sw_prod, &sw_chain_prod, &sw_prod_bseq)) {

					DBRUNIF(1, bce_breakpoint(sc));
					panic("bce%d: Double mbuf allocation failure!", sc->bce_unit);

				}
				goto bce_rx_int_next_rx;
			}

			/* Skip over the l2_fhdr when passing the data up the stack. */
			m_adj(m, sizeof(struct l2_fhdr) + ETHER_ALIGN);

			/* Adjust the packet length to match the received data. */
			m->m_pkthdr.len = m->m_len = len;

			/* Send the packet to the appropriate interface. */
			m->m_pkthdr.rcvif = ifp;

			DBRUN(BCE_VERBOSE_RECV,
				struct ether_header *eh;
				eh = mtod(m, struct ether_header *);
				BCE_PRINTF(sc, "%s(): to: %6D, from: %6D, type: 0x%04X\n",
					__FUNCTION__, eh->ether_dhost, ":", 
					eh->ether_shost, ":", htons(eh->ether_type)));

			/* Validate the checksum if offload enabled. */
			if (ifp->if_capenable & IFCAP_RXCSUM) {

				/* Check for an IP datagram. */
				if (status & L2_FHDR_STATUS_IP_DATAGRAM) {
					m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;

					/* Check if the IP checksum is valid. */
					if ((l2fhdr->l2_fhdr_ip_xsum ^ 0xffff) == 0)
						m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
					else
						DBPRINT(sc, BCE_WARN_SEND, 
							"%s(): Invalid IP checksum = 0x%04X!\n",
							__FUNCTION__, l2fhdr->l2_fhdr_ip_xsum);
				}

				/* Check for a valid TCP/UDP frame. */
				if (status & (L2_FHDR_STATUS_TCP_SEGMENT |
					L2_FHDR_STATUS_UDP_DATAGRAM)) {

					/* Check for a good TCP/UDP checksum. */
					if ((status & (L2_FHDR_ERRORS_TCP_XSUM |
						      L2_FHDR_ERRORS_UDP_XSUM)) == 0) {
						m->m_pkthdr.csum_data =
						    l2fhdr->l2_fhdr_tcp_udp_xsum;
						m->m_pkthdr.csum_flags |= (CSUM_DATA_VALID 
							| CSUM_PSEUDO_HDR);
					} else
						DBPRINT(sc, BCE_WARN_SEND, 
							"%s(): Invalid TCP/UDP checksum = 0x%04X!\n",
							__FUNCTION__, l2fhdr->l2_fhdr_tcp_udp_xsum);
				}
			}		

			/* Pass the mbuf off to the upper layers. */
			ifp->if_ipackets++;
			DBPRINT(sc, BCE_VERBOSE_RECV, "%s(): Passing received frame up.\n",
				__FUNCTION__);
			ether_input(ifp, NULL, m);
			DBRUNIF(1, sc->rx_mbuf_alloc--);

bce_rx_int_next_rx:
			sw_prod = NEXT_RX_BD(sw_prod);
		}

		sw_cons = NEXT_RX_BD(sw_cons);

		/* Refresh hw_cons to see if there's new work */
		if (sw_cons == hw_cons) {
			hw_cons = sc->hw_rx_cons = sblk->status_rx_quick_consumer_index0;
			if ((hw_cons & USABLE_RX_BD_PER_PAGE) == USABLE_RX_BD_PER_PAGE)
				hw_cons++;
		}

		/* Prevent speculative reads from getting ahead of the status block. */
		bus_space_barrier(sc->bce_btag, sc->bce_bhandle, 0, 0, 
			BUS_SPACE_BARRIER_READ);
	}

	for (i = 0; i < RX_PAGES; i++)
		bus_dmamap_sync(sc->rx_bd_chain_tag,
		    sc->rx_bd_chain_map[i], BUS_DMASYNC_PREWRITE);

	sc->rx_cons = sw_cons;
	sc->rx_prod = sw_prod;
	sc->rx_prod_bseq = sw_prod_bseq;

	REG_WR16(sc, MB_RX_CID_ADDR + BCE_L2CTX_HOST_BDIDX, sc->rx_prod);
	REG_WR(sc, MB_RX_CID_ADDR + BCE_L2CTX_HOST_BSEQ, sc->rx_prod_bseq);

	DBPRINT(sc, BCE_INFO_RECV, "%s(exit): rx_prod = 0x%04X, "
		"rx_cons = 0x%04X, rx_prod_bseq = 0x%08X\n",
		__FUNCTION__, sc->rx_prod, sc->rx_cons, sc->rx_prod_bseq);
}


/****************************************************************************/
/* Handles transmit completion interrupt events.                            */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_tx_intr(struct bce_softc *sc)
{
	struct status_block *sblk = sc->status_block;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	u16 hw_tx_cons, sw_tx_cons, sw_tx_chain_cons;

	DBRUNIF(1, sc->tx_interrupts++);

	/* Get the hardware's view of the TX consumer index. */
	hw_tx_cons = sc->hw_tx_cons = sblk->status_tx_quick_consumer_index0;

	/* Skip to the next entry if this is a chain page pointer. */
	if ((hw_tx_cons & USABLE_TX_BD_PER_PAGE) == USABLE_TX_BD_PER_PAGE)
		hw_tx_cons++;

	sw_tx_cons = sc->tx_cons;

	/* Prevent speculative reads from getting ahead of the status block. */
	bus_space_barrier(sc->bce_btag, sc->bce_bhandle, 0, 0, 
		BUS_SPACE_BARRIER_READ);

	/* Cycle through any completed TX chain page entries. */
	while (sw_tx_cons != hw_tx_cons) {
#ifdef BCE_DEBUG
		struct tx_bd *txbd = NULL;
#endif
		sw_tx_chain_cons = TX_CHAIN_IDX(sw_tx_cons);

		DBPRINT(sc, BCE_INFO_SEND,
			"%s(): hw_tx_cons = 0x%04X, sw_tx_cons = 0x%04X, "
			"sw_tx_chain_cons = 0x%04X\n",
			__FUNCTION__, hw_tx_cons, sw_tx_cons, sw_tx_chain_cons);

		DBRUNIF((sw_tx_chain_cons > MAX_TX_BD),
			BCE_PRINTF(sc, "%s(%d): TX chain consumer out of range! "
				" 0x%04X > 0x%04X\n",
				__FILE__, __LINE__, sw_tx_chain_cons, 
				(int) MAX_TX_BD);
			bce_breakpoint(sc));

		DBRUNIF(1,
			txbd = &sc->tx_bd_chain[TX_PAGE(sw_tx_chain_cons)]
				[TX_IDX(sw_tx_chain_cons)]);
		
		DBRUNIF((txbd == NULL),
			BCE_PRINTF(sc, "%s(%d): Unexpected NULL tx_bd[0x%04X]!\n", 
				__FILE__, __LINE__, sw_tx_chain_cons);
			bce_breakpoint(sc));

		DBRUN(BCE_INFO_SEND, 
			BCE_PRINTF(sc, "%s(): ", __FUNCTION__);
			bce_dump_txbd(sc, sw_tx_chain_cons, txbd));

		/*
		 * Free the associated mbuf. Remember
		 * that only the last tx_bd of a packet
		 * has an mbuf pointer and DMA map.
		 */
		if (sc->tx_mbuf_ptr[sw_tx_chain_cons] != NULL) {

			/* Validate that this is the last tx_bd. */
			DBRUNIF((!(txbd->tx_bd_vlan_tag_flags & TX_BD_FLAGS_END)),
				BCE_PRINTF(sc, "%s(%d): tx_bd END flag not set but "
				"txmbuf == NULL!\n", __FILE__, __LINE__);
				bce_breakpoint(sc));

			DBRUN(BCE_INFO_SEND, 
				BCE_PRINTF(sc, "%s(): Unloading map/freeing mbuf "
					"from tx_bd[0x%04X]\n", __FUNCTION__, sw_tx_chain_cons));

			/* Unmap the mbuf. */
			bus_dmamap_unload(sc->tx_mbuf_tag,
			    sc->tx_mbuf_map[sw_tx_chain_cons]);
	
			/* Free the mbuf. */
			m_freem(sc->tx_mbuf_ptr[sw_tx_chain_cons]);
			sc->tx_mbuf_ptr[sw_tx_chain_cons] = NULL;
			DBRUNIF(1, sc->tx_mbuf_alloc--);

			ifp->if_opackets++;
		}

		sc->used_tx_bd--;
		sw_tx_cons = NEXT_TX_BD(sw_tx_cons);

		/* Refresh hw_cons to see if there's new work. */
		hw_tx_cons = sc->hw_tx_cons = sblk->status_tx_quick_consumer_index0;
		if ((hw_tx_cons & USABLE_TX_BD_PER_PAGE) == USABLE_TX_BD_PER_PAGE)
			hw_tx_cons++;

		/* Prevent speculative reads from getting ahead of the status block. */
		bus_space_barrier(sc->bce_btag, sc->bce_bhandle, 0, 0, 
			BUS_SPACE_BARRIER_READ);
	}

	/* Clear the TX timeout timer. */
	ifp->if_timer = 0;

	/* Clear the tx hardware queue full flag. */
	if ((sc->used_tx_bd + BCE_TX_SLACK_SPACE) < USABLE_TX_BD) {
		DBRUNIF((ifp->if_flags & IFF_OACTIVE),
			BCE_PRINTF(sc, "%s(): TX chain is open for business! Used tx_bd = %d\n", 
				__FUNCTION__, sc->used_tx_bd));
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	sc->tx_cons = sw_tx_cons;
}


/****************************************************************************/
/* Disables interrupt generation.                                           */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_disable_intr(struct bce_softc *sc)
{
	REG_WR(sc, BCE_PCICFG_INT_ACK_CMD,
	       BCE_PCICFG_INT_ACK_CMD_MASK_INT);
	REG_RD(sc, BCE_PCICFG_INT_ACK_CMD);
}


/****************************************************************************/
/* Enables interrupt generation.                                            */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_enable_intr(struct bce_softc *sc)
{
	u32 val;

	REG_WR(sc, BCE_PCICFG_INT_ACK_CMD,
	       BCE_PCICFG_INT_ACK_CMD_INDEX_VALID |
	       BCE_PCICFG_INT_ACK_CMD_MASK_INT | sc->last_status_idx);

	REG_WR(sc, BCE_PCICFG_INT_ACK_CMD,
	       BCE_PCICFG_INT_ACK_CMD_INDEX_VALID | sc->last_status_idx);

	val = REG_RD(sc, BCE_HC_COMMAND);
	REG_WR(sc, BCE_HC_COMMAND, val | BCE_HC_COMMAND_COAL_NOW);
}


/****************************************************************************/
/* Handles controller initialization.                                       */
/*                                                                          */
/* Must be called from a locked routine.                                    */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_init(void *xsc)
{
	struct bce_softc *sc = xsc;
	struct ifnet *ifp;
	u32 ether_mtu;
	int s;

	s = splimp();

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	ifp = &sc->arpcom.ac_if;

	/* Check if the driver is still running and bail out if it is. */
	if (ifp->if_flags & IFF_RUNNING)
		goto bce_init_locked_exit;

	bce_stop(sc);

	if (bce_reset(sc, BCE_DRV_MSG_CODE_RESET)) {
		BCE_PRINTF(sc, "%s(%d): Controller reset failed!\n", 
			__FILE__, __LINE__);
		goto bce_init_locked_exit;
	}

	if (bce_chipinit(sc)) {
		BCE_PRINTF(sc, "%s(%d): Controller initialization failed!\n", 
			__FILE__, __LINE__);
		goto bce_init_locked_exit;
	}

	if (bce_blockinit(sc)) {
		BCE_PRINTF(sc, "%s(%d): Block initialization failed!\n", 
			__FILE__, __LINE__);
		goto bce_init_locked_exit;
	}

	/* Load our MAC address. */
	bce_set_mac_addr(sc);

	/* Calculate and program the Ethernet MTU size. */
	ether_mtu = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN + ifp->if_mtu + 
		ETHER_CRC_LEN;

	DBPRINT(sc, BCE_INFO, "%s(): setting mtu = %d\n",__FUNCTION__, ether_mtu);

	/* 
	 * Program the mtu, enabling jumbo frame 
	 * support if necessary.  Also set the mbuf
	 * allocation count for RX frames.
	 */
	REG_WR(sc, BCE_EMAC_RX_MTU_SIZE, ether_mtu);
	sc->mbuf_alloc_size = MCLBYTES;

	/* Calculate the RX Ethernet frame size for rx_bd's. */
	sc->max_frame_size = sizeof(struct l2_fhdr) + 2 + ether_mtu + 8;

	DBPRINT(sc, BCE_INFO, 
		"%s(): mclbytes = %d, mbuf_alloc_size = %d, "
		"max_frame_size = %d\n",
		__FUNCTION__, (int) MCLBYTES, sc->mbuf_alloc_size, sc->max_frame_size);

	/* Program appropriate promiscuous/multicast filtering. */
	bce_set_rx_mode(sc);

	/* Init RX buffer descriptor chain. */
	bce_init_rx_chain(sc);

	/* Init TX buffer descriptor chain. */
	bce_init_tx_chain(sc);

#ifdef DEVICE_POLLING
	/* Disable interrupts if we are polling. */
	if (ifp->if_ipending & IFF_POLLING) {
		bce_disable_intr(sc);

		REG_WR(sc, BCE_HC_RX_QUICK_CONS_TRIP,
			(1 << 16) | sc->bce_rx_quick_cons_trip);
		REG_WR(sc, BCE_HC_TX_QUICK_CONS_TRIP,
			(1 << 16) | sc->bce_tx_quick_cons_trip);
	} else
#endif
	/* Enable host interrupts. */
	bce_enable_intr(sc);

	bce_ifmedia_upd(ifp);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	callout_reset(&sc->bce_stat_ch, hz, bce_tick, sc);

bce_init_locked_exit:
	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	splx(s);

	return;
}


/****************************************************************************/
/* Encapsultes an mbuf cluster into the tx_bd chain structure and makes the */
/* memory visible to the controller.                                        */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_tx_encap(struct bce_softc *sc, struct mbuf *m_head, u16 *prod,
	u16 *chain_prod, u32 *prod_bseq)
{
	u32 vlan_tag_flags = 0;
	struct bce_dmamap_arg map_arg;
	bus_dmamap_t map;
	int i, error, rc = 0;

	/* Transfer any checksum offload flags to the bd. */
	if (m_head->m_pkthdr.csum_flags) {
		if (m_head->m_pkthdr.csum_flags & CSUM_IP)
			vlan_tag_flags |= TX_BD_FLAGS_IP_CKSUM;
		if (m_head->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP))
			vlan_tag_flags |= TX_BD_FLAGS_TCP_UDP_CKSUM;
	}

	/* Map the mbuf into DMAable memory. */
	map = sc->tx_mbuf_map[*chain_prod];
	map_arg.sc         = sc;
	map_arg.prod       = *prod;
	map_arg.chain_prod = *chain_prod;
	map_arg.prod_bseq  = *prod_bseq;
	map_arg.tx_flags   = vlan_tag_flags;
	map_arg.maxsegs    = USABLE_TX_BD - sc->used_tx_bd - 
		BCE_TX_SLACK_SPACE;

	KASSERT(map_arg.maxsegs > 0, ("Invalid TX maxsegs value!"));

	for (i = 0; i < TX_PAGES; i++)
		map_arg.tx_chain[i] = sc->tx_bd_chain[i];

	/* Map the mbuf into our DMA address space. */
	error = bus_dmamap_load_mbuf(sc->tx_mbuf_tag, map, m_head,
	    bce_dma_map_tx_desc, &map_arg, BUS_DMA_NOWAIT);

	if (error || map_arg.maxsegs == 0) {
		if (error == EFBIG && map_arg.maxsegs != 0) {
			struct mbuf *m0;

			m0 = m_defrag(m_head, M_DONTWAIT);
			if (m0 != NULL) {
				m_head = m0;
				error = bus_dmamap_load_mbuf(sc->tx_mbuf_tag,
				    map, m_head, bce_dma_map_tx_desc, &map_arg,
				    BUS_DMA_NOWAIT);
			}
		}
		if (error) {
			BCE_PRINTF(sc,
			    "%s(%d): Error mapping mbuf into TX chain!\n",
			    __FILE__, __LINE__);
			rc = ENOBUFS;
			goto bce_tx_encap_exit;
		}
	}

	/*
	 * Ensure that the map for this transmission
	 * is placed at the array index of the last
	 * descriptor in this chain.  This is done
	 * because a single map is used for all 
	 * segments of the mbuf and we don't want to
	 * delete the map before all of the segments
	 * have been freed.
	 */
	sc->tx_mbuf_map[*chain_prod] = 
		sc->tx_mbuf_map[map_arg.chain_prod];
	sc->tx_mbuf_map[map_arg.chain_prod] = map;
	sc->tx_mbuf_ptr[map_arg.chain_prod] = m_head;
	sc->used_tx_bd += map_arg.maxsegs;

	DBRUNIF((sc->used_tx_bd > sc->tx_hi_watermark), 
		sc->tx_hi_watermark = sc->used_tx_bd);

	DBRUNIF(1, sc->tx_mbuf_alloc++);

	DBRUN(BCE_VERBOSE_SEND, bce_dump_tx_mbuf_chain(sc, *chain_prod, 
		map_arg.maxsegs));

	/* prod still points the last used tx_bd at this point. */
	*prod       = map_arg.prod;
	*chain_prod = map_arg.chain_prod;
	*prod_bseq  = map_arg.prod_bseq;

bce_tx_encap_exit:

	return(rc);
}


/****************************************************************************/
/* Main transmit routine when called from another routine with a lock.      */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_start(struct ifnet *ifp)
{
	struct bce_softc *sc = ifp->if_softc;
	struct mbuf *m_head = NULL;
	int count = 0;
	u16 tx_prod, tx_chain_prod;
	u32	tx_prod_bseq;

	/* If there's no link or the transmit queue is empty then just exit. */
	if (!sc->bce_link || IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		DBPRINT(sc, BCE_INFO_SEND, "%s(): No link or transmit queue empty.\n", 
			__FUNCTION__);
		goto bce_start_locked_exit;
	}

	/* prod points to the next free tx_bd. */
	tx_prod = sc->tx_prod;
	tx_chain_prod = TX_CHAIN_IDX(tx_prod);
	tx_prod_bseq = sc->tx_prod_bseq;

	DBPRINT(sc, BCE_INFO_SEND,
		"%s(): Start: tx_prod = 0x%04X, tx_chain_prod = %04X, "
		"tx_prod_bseq = 0x%08X\n",
		__FUNCTION__, tx_prod, tx_chain_prod, tx_prod_bseq);

	/* Keep adding entries while there is space in the ring. */
	while(sc->tx_mbuf_ptr[tx_chain_prod] == NULL) {

		/* Check for any frames to send. */
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, place the mbuf back at the
		 * head of the queue and set the OACTIVE flag
		 * to wait for the NIC to drain the chain.
		 */
		if (bce_tx_encap(sc, m_head, &tx_prod, &tx_chain_prod, &tx_prod_bseq)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			DBPRINT(sc, BCE_INFO_SEND,
				"TX chain is closed for business! Total tx_bd used = %d\n", 
				sc->used_tx_bd);
			break;
		}

		count++;

		/* Send a copy of the frame to any BPF listeners. */
		BPF_MTAP(ifp, m_head);

		tx_prod = NEXT_TX_BD(tx_prod);
		tx_chain_prod = TX_CHAIN_IDX(tx_prod);
	}

	if (count == 0) {
		/* no packets were dequeued */
		DBPRINT(sc, BCE_VERBOSE_SEND, "%s(): No packets were dequeued\n", 
			__FUNCTION__);
		goto bce_start_locked_exit;
	}

	/* Update the driver's counters. */
	sc->tx_prod      = tx_prod;
	sc->tx_prod_bseq = tx_prod_bseq;

	DBPRINT(sc, BCE_INFO_SEND,
		"%s(): End: tx_prod = 0x%04X, tx_chain_prod = 0x%04X, "
		"tx_prod_bseq = 0x%08X\n",
		__FUNCTION__, tx_prod, tx_chain_prod, tx_prod_bseq);

	/* Start the transmit. */
	REG_WR16(sc, MB_TX_CID_ADDR + BCE_L2CTX_TX_HOST_BIDX, sc->tx_prod);
	REG_WR(sc, MB_TX_CID_ADDR + BCE_L2CTX_TX_HOST_BSEQ, sc->tx_prod_bseq);

	/* Set the tx timeout. */
	ifp->if_timer = BCE_TX_TIMEOUT;

bce_start_locked_exit:
	return;
}


/****************************************************************************/
/* Handles any IOCTL calls from the operating system.                       */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct bce_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	struct mii_data *mii;
	int error = 0, s;

	s = splimp();

	DBPRINT(sc, BCE_VERBOSE_RESET, "Entering %s()\n", __FUNCTION__);

	switch(command) {

		/* Set the MTU. */
		case SIOCSIFMTU:
			/* Check that the MTU setting is supported. */
			if ((ifr->ifr_mtu < BCE_MIN_MTU) ||
				(ifr->ifr_mtu > BCE_MAX_STD_MTU)) {
				error = EINVAL;
				break;
			}

			DBPRINT(sc, BCE_INFO, "Setting new MTU of %d\n", ifr->ifr_mtu);

			ifp->if_mtu = ifr->ifr_mtu;
			ifp->if_flags &= ~IFF_RUNNING;
			bce_init(sc);
			break;

		/* Set interface. */
		case SIOCSIFFLAGS:
			DBPRINT(sc, BCE_VERBOSE, "Received SIOCSIFFLAGS\n");

			/* Check if the interface is up. */
			if (ifp->if_flags & IFF_UP) {
				/* Change the promiscuous/multicast flags as necessary. */
				bce_set_rx_mode(sc);
			} else {
				/* The interface is down.  Check if the driver is running. */
				if (ifp->if_flags & IFF_RUNNING) {
					bce_stop(sc);
				}
			}

			error = 0;

			break;

		/* Add/Delete multicast address */
		case SIOCADDMULTI:
		case SIOCDELMULTI:
			DBPRINT(sc, BCE_VERBOSE, "Received SIOCADDMULTI/SIOCDELMULTI\n");

			if (ifp->if_flags & IFF_RUNNING) {
				bce_set_rx_mode(sc);
				error = 0;
			}

			break;

		/* Set/Get Interface media */
		case SIOCSIFMEDIA:
		case SIOCGIFMEDIA:
			DBPRINT(sc, BCE_VERBOSE, "Received SIOCSIFMEDIA/SIOCGIFMEDIA\n");

			DBPRINT(sc, BCE_VERBOSE, "bce_phy_flags = 0x%08X\n",
				sc->bce_phy_flags);

			if (sc->bce_phy_flags & BCE_PHY_SERDES_FLAG) {
				DBPRINT(sc, BCE_VERBOSE, "SerDes media set/get\n");

				error = ifmedia_ioctl(ifp, ifr,
				    &sc->bce_ifmedia, command);
			} else {
				DBPRINT(sc, BCE_VERBOSE, "Copper media set/get\n");
				mii = device_get_softc(sc->bce_miibus);
				error = ifmedia_ioctl(ifp, ifr,
				    &mii->mii_media, command);
			}
			break;

		/* Set interface capability */
		case SIOCSIFCAP:
			ifp->if_capenable = ifr->ifr_reqcap;
			if (ifp->if_capenable & IFCAP_HWCSUM)
				ifp->if_hwassist = BCE_IF_HWASSIST;
			else
				ifp->if_hwassist = 0;
			break;
		case SIOCSIFADDR:
		case SIOCGIFADDR:
			/* We don't know how to handle the IOCTL, pass it on. */
			error = ether_ioctl(ifp, command, data);
			break;
			
		default:
			DBPRINT(sc, BCE_INFO, "Received unsupported IOCTL: 0x%08X\n",
				(u32) command);
			error = EINVAL;
			break;
	}

	DBPRINT(sc, BCE_VERBOSE_RESET, "Exiting %s()\n", __FUNCTION__);

	splx(s);

	return(error);
}


/****************************************************************************/
/* Transmit timeout handler.                                                */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_watchdog(struct ifnet *ifp)
{
	struct bce_softc *sc = ifp->if_softc;

	DBRUN(BCE_WARN_SEND, 
		bce_dump_driver_state(sc);
		bce_dump_status_block(sc));

	BCE_PRINTF(sc, "%s(%d): Watchdog timeout occurred, resetting!\n", 
		__FILE__, __LINE__);

	/* DBRUN(BCE_FATAL, bce_breakpoint(sc)); */

	ifp->if_flags &= ~IFF_RUNNING;

	bce_init(sc);
	ifp->if_oerrors++;

}


#ifdef DEVICE_POLLING
static void
bce_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct bce_softc *sc = ifp->if_softc;

	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}

	if (cmd == POLL_DEREGISTER) {
		/* Re-enable interrupts. */
		bce_enable_intr(sc);

		REG_WR(sc, BCE_HC_TX_QUICK_CONS_TRIP,
		    (sc->bce_tx_quick_cons_trip_int << 16) |
		    sc->bce_tx_quick_cons_trip);
		REG_WR(sc, BCE_HC_RX_QUICK_CONS_TRIP,
		    (sc->bce_rx_quick_cons_trip_int << 16) |
		    sc->bce_rx_quick_cons_trip);
		return;
	}

	sc->bce_rxcycles = count;

	bus_dmamap_sync(sc->status_tag, sc->status_map,
	    BUS_DMASYNC_POSTWRITE);

	/* Check for any completed RX frames. */
	if (sc->status_block->status_rx_quick_consumer_index0 != 
		sc->hw_rx_cons)
		bce_rx_intr(sc);

	/* Check for any completed TX frames. */
	if (sc->status_block->status_tx_quick_consumer_index0 != 
		sc->hw_tx_cons)
		bce_tx_intr(sc);

	/* Check for new frames to transmit. */
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		bce_start(ifp);

}

#endif /* DEVICE_POLLING */


#if 0
static inline int
bce_has_work(struct bce_softc *sc)
{
	struct status_block *stat = sc->status_block;

	if ((stat->status_rx_quick_consumer_index0 != sc->hw_rx_cons) ||
	    (stat->status_tx_quick_consumer_index0 != sc->hw_tx_cons))
		return 1;

	if (((stat->status_attn_bits & STATUS_ATTN_BITS_LINK_STATE) != 0) !=
	    bp->link_up)
		return 1;

	return 0;
}
#endif


/*
 * Interrupt handler.
 */
/****************************************************************************/
/* Main interrupt entry point.  Verifies that the controller generated the  */
/* interrupt and then calls a separate routine for handle the various       */
/* interrupt causes (PHY, TX, RX).                                          */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static void
bce_intr(void *xsc)
{
	struct bce_softc *sc;
	struct ifnet *ifp;
	u32 status_attn_bits;

	sc = xsc;
	ifp = &sc->arpcom.ac_if;

	DBRUNIF(1, sc->interrupts_generated++);

#ifdef DEVICE_POLLING
	if (ifp->if_ipending & IFF_POLLING)
		return;
	if (ifp->if_capenable & IFCAP_POLLING &&
	    ether_poll_register(bce_poll, ifp)) {
		/* Disable interrupts. */
		bce_disable_intr(sc);

		REG_WR(sc, BCE_HC_RX_QUICK_CONS_TRIP,
		    (1 << 16) | sc->bce_rx_quick_cons_trip);
		REG_WR(sc, BCE_HC_TX_QUICK_CONS_TRIP,
		    (1 << 16) | sc->bce_tx_quick_cons_trip);

		bce_poll(ifp, 0, 1);
		return;
	}
#endif

	bus_dmamap_sync(sc->status_tag, sc->status_map,
	    BUS_DMASYNC_POSTWRITE);

	/*
	 * If the hardware status block index
	 * matches the last value read by the
	 * driver and we haven't asserted our
	 * interrupt then there's nothing to do.
	 */
	if ((sc->status_block->status_idx == sc->last_status_idx) && 
		(REG_RD(sc, BCE_PCICFG_MISC_STATUS) & BCE_PCICFG_MISC_STATUS_INTA_VALUE))
		goto bce_intr_exit;

	/* Ack the interrupt and stop others from occuring. */
	REG_WR(sc, BCE_PCICFG_INT_ACK_CMD,
		BCE_PCICFG_INT_ACK_CMD_USE_INT_HC_PARAM |
		BCE_PCICFG_INT_ACK_CMD_MASK_INT);

	/* Keep processing data as long as there is work to do. */
	for (;;) {

		status_attn_bits = sc->status_block->status_attn_bits;

		DBRUNIF(DB_RANDOMTRUE(bce_debug_unexpected_attention),
			BCE_PRINTF(sc, "Simulating unexpected status attention bit set.");
			status_attn_bits = status_attn_bits | STATUS_ATTN_BITS_PARITY_ERROR);

		/* Was it a link change interrupt? */
		if ((status_attn_bits & STATUS_ATTN_BITS_LINK_STATE) !=
			(sc->status_block->status_attn_bits_ack & STATUS_ATTN_BITS_LINK_STATE))
			bce_phy_intr(sc);

		/* If any other attention is asserted then the chip is toast. */
		if (((status_attn_bits & ~STATUS_ATTN_BITS_LINK_STATE) !=
			(sc->status_block->status_attn_bits_ack & 
			~STATUS_ATTN_BITS_LINK_STATE))) {

			DBRUN(1, sc->unexpected_attentions++);

			BCE_PRINTF(sc, "%s(%d): Fatal attention detected: 0x%08X\n", 
				__FILE__, __LINE__, sc->status_block->status_attn_bits);

			DBRUN(BCE_FATAL, 
				if (bce_debug_unexpected_attention == 0)
					bce_breakpoint(sc));

			bce_init(sc);
			goto bce_intr_exit;
		}

		/* Check for any completed RX frames. */
		if (sc->status_block->status_rx_quick_consumer_index0 != sc->hw_rx_cons)
			bce_rx_intr(sc);

		/* Check for any completed TX frames. */
		if (sc->status_block->status_tx_quick_consumer_index0 != sc->hw_tx_cons)
			bce_tx_intr(sc);

		/* Save the status block index value for use during the next interrupt. */
		sc->last_status_idx = sc->status_block->status_idx;

		/* Prevent speculative reads from getting ahead of the status block. */
		bus_space_barrier(sc->bce_btag, sc->bce_bhandle, 0, 0, 
			BUS_SPACE_BARRIER_READ);

		/* If there's no work left then exit the interrupt service routine. */
		if ((sc->status_block->status_rx_quick_consumer_index0 == sc->hw_rx_cons) &&
	    	(sc->status_block->status_tx_quick_consumer_index0 == sc->hw_tx_cons))
			break;
	
	}

	bus_dmamap_sync(sc->status_tag,	sc->status_map,
	    BUS_DMASYNC_PREWRITE);

	/* Re-enable interrupts. */
	REG_WR(sc, BCE_PCICFG_INT_ACK_CMD,
	       BCE_PCICFG_INT_ACK_CMD_INDEX_VALID | sc->last_status_idx |
	       BCE_PCICFG_INT_ACK_CMD_MASK_INT);
	REG_WR(sc, BCE_PCICFG_INT_ACK_CMD,
	       BCE_PCICFG_INT_ACK_CMD_INDEX_VALID | sc->last_status_idx);

	/* Handle any frames that arrived while handling the interrupt. */
	if (ifp->if_flags & IFF_RUNNING && !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		bce_start(ifp);

bce_intr_exit:
}


/****************************************************************************/
/* Programs the various packet receive modes (broadcast and multicast).     */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_set_rx_mode(struct bce_softc *sc)
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	u32 hashes[4] = { 0, 0, 0, 0 };
	u32 rx_mode, sort_mode;
	int h, i;

	ifp = &sc->arpcom.ac_if;

	/* Initialize receive mode default settings. */
	rx_mode   = sc->rx_mode & ~(BCE_EMAC_RX_MODE_PROMISCUOUS |
			    BCE_EMAC_RX_MODE_KEEP_VLAN_TAG);
	sort_mode = 1 | BCE_RPM_SORT_USER0_BC_EN;

	/*
	 * ASF/IPMI/UMP firmware requires that VLAN tag stripping
	 * be enabled.
	 */
	if (!(sc->bce_flags & BCE_MFW_ENABLE_FLAG))
		rx_mode |= BCE_EMAC_RX_MODE_KEEP_VLAN_TAG;

	/*
	 * Check for promiscuous, all multicast, or selected
	 * multicast address filtering.
	 */
	if (ifp->if_flags & IFF_PROMISC) {
		DBPRINT(sc, BCE_INFO, "Enabling promiscuous mode.\n");

		/* Enable promiscuous mode. */
		rx_mode |= BCE_EMAC_RX_MODE_PROMISCUOUS;
		sort_mode |= BCE_RPM_SORT_USER0_PROM_EN;
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		DBPRINT(sc, BCE_INFO, "Enabling all multicast mode.\n");

		/* Enable all multicast addresses. */
		for (i = 0; i < NUM_MC_HASH_REGISTERS; i++) {
			REG_WR(sc, BCE_EMAC_MULTICAST_HASH0 + (i * 4), 0xffffffff);
       	}
		sort_mode |= BCE_RPM_SORT_USER0_MC_EN;
	} else {
		/* Accept one or more multicast(s). */
		DBPRINT(sc, BCE_INFO, "Enabling selective multicast mode.\n");

		LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			h = ether_crc32_le(LLADDR((struct sockaddr_dl *)
		    	ifma->ifma_addr), ETHER_ADDR_LEN) & 0x7F;
			hashes[(h & 0x60) >> 5] |= 1 << (h & 0x1F);
		}

		for (i = 0; i < 4; i++)
			REG_WR(sc, BCE_EMAC_MULTICAST_HASH0 + (i * 4), hashes[i]);

		sort_mode |= BCE_RPM_SORT_USER0_MC_HSH_EN;
	}

	/* Only make changes if the recive mode has actually changed. */
	if (rx_mode != sc->rx_mode) {
		DBPRINT(sc, BCE_VERBOSE, "Enabling new receive mode: 0x%08X\n", 
			rx_mode);

		sc->rx_mode = rx_mode;
		REG_WR(sc, BCE_EMAC_RX_MODE, rx_mode);
	}

	/* Disable and clear the exisitng sort before enabling a new sort. */
	REG_WR(sc, BCE_RPM_SORT_USER0, 0x0);
	REG_WR(sc, BCE_RPM_SORT_USER0, sort_mode);
	REG_WR(sc, BCE_RPM_SORT_USER0, sort_mode | BCE_RPM_SORT_USER0_ENA);
}


/****************************************************************************/
/* Called periodically to updates statistics from the controllers           */
/* statistics block.                                                        */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_stats_update(struct bce_softc *sc)
{
	struct ifnet *ifp;
	struct statistics_block *stats;

	DBPRINT(sc, BCE_EXCESSIVE, "Entering %s()\n", __FUNCTION__);

	ifp = &sc->arpcom.ac_if;

	stats = (struct statistics_block *) sc->stats_block;

	/* 
	 * Update the interface statistics from the
	 * hardware statistics.
	 */
	ifp->if_collisions = (u_long) stats->stat_EtherStatsCollisions;

	ifp->if_ibytes  = BCE_STATS(IfHCInOctets);

	ifp->if_obytes  = BCE_STATS(IfHCOutOctets);

	ifp->if_imcasts = BCE_STATS(IfHCInMulticastPkts);

	ifp->if_omcasts = BCE_STATS(IfHCOutMulticastPkts);

	ifp->if_ierrors = (u_long) stats->stat_EtherStatsUndersizePkts +
				      (u_long) stats->stat_EtherStatsOverrsizePkts +
					  (u_long) stats->stat_IfInMBUFDiscards +
					  (u_long) stats->stat_Dot3StatsAlignmentErrors +
					  (u_long) stats->stat_Dot3StatsFCSErrors;

	ifp->if_oerrors = (u_long) stats->stat_emac_tx_stat_dot3statsinternalmactransmiterrors +
					  (u_long) stats->stat_Dot3StatsExcessiveCollisions +
					  (u_long) stats->stat_Dot3StatsLateCollisions;

	/* 
	 * Certain controllers don't report 
	 * carrier sense errors correctly.
	 * See errata E11_5708CA0_1165. 
	 */
	if (!(BCE_CHIP_NUM(sc) == BCE_CHIP_NUM_5706) &&
	    !(BCE_CHIP_ID(sc) == BCE_CHIP_ID_5708_A0))
		ifp->if_oerrors += (u_long) stats->stat_Dot3StatsCarrierSenseErrors;

	/*
	 * Update the sysctl statistics from the
	 * hardware statistics.
	 */
	sc->stat_IfHCInOctets = 
		((u64) stats->stat_IfHCInOctets_hi << 32) + 
		 (u64) stats->stat_IfHCInOctets_lo;

	sc->stat_IfHCInBadOctets =
		((u64) stats->stat_IfHCInBadOctets_hi << 32) + 
		 (u64) stats->stat_IfHCInBadOctets_lo;

	sc->stat_IfHCOutOctets =
		((u64) stats->stat_IfHCOutOctets_hi << 32) +
		 (u64) stats->stat_IfHCOutOctets_lo;

	sc->stat_IfHCOutBadOctets =
		((u64) stats->stat_IfHCOutBadOctets_hi << 32) +
		 (u64) stats->stat_IfHCOutBadOctets_lo;

	sc->stat_IfHCInUcastPkts =
		((u64) stats->stat_IfHCInUcastPkts_hi << 32) +
		 (u64) stats->stat_IfHCInUcastPkts_lo;

	sc->stat_IfHCInMulticastPkts =
		((u64) stats->stat_IfHCInMulticastPkts_hi << 32) +
		 (u64) stats->stat_IfHCInMulticastPkts_lo;

	sc->stat_IfHCInBroadcastPkts =
		((u64) stats->stat_IfHCInBroadcastPkts_hi << 32) +
		 (u64) stats->stat_IfHCInBroadcastPkts_lo;

	sc->stat_IfHCOutUcastPkts =
		((u64) stats->stat_IfHCOutUcastPkts_hi << 32) +
		 (u64) stats->stat_IfHCOutUcastPkts_lo;

	sc->stat_IfHCOutMulticastPkts =
		((u64) stats->stat_IfHCOutMulticastPkts_hi << 32) +
		 (u64) stats->stat_IfHCOutMulticastPkts_lo;

	sc->stat_IfHCOutBroadcastPkts =
		((u64) stats->stat_IfHCOutBroadcastPkts_hi << 32) +
		 (u64) stats->stat_IfHCOutBroadcastPkts_lo;

	sc->stat_emac_tx_stat_dot3statsinternalmactransmiterrors =
		stats->stat_emac_tx_stat_dot3statsinternalmactransmiterrors;

	sc->stat_Dot3StatsCarrierSenseErrors =
		stats->stat_Dot3StatsCarrierSenseErrors;

	sc->stat_Dot3StatsFCSErrors = 
		stats->stat_Dot3StatsFCSErrors;

	sc->stat_Dot3StatsAlignmentErrors =
		stats->stat_Dot3StatsAlignmentErrors;

	sc->stat_Dot3StatsSingleCollisionFrames =
		stats->stat_Dot3StatsSingleCollisionFrames;

	sc->stat_Dot3StatsMultipleCollisionFrames =
		stats->stat_Dot3StatsMultipleCollisionFrames;

	sc->stat_Dot3StatsDeferredTransmissions =
		stats->stat_Dot3StatsDeferredTransmissions;

	sc->stat_Dot3StatsExcessiveCollisions =
		stats->stat_Dot3StatsExcessiveCollisions;

	sc->stat_Dot3StatsLateCollisions =
		stats->stat_Dot3StatsLateCollisions;

	sc->stat_EtherStatsCollisions =
		stats->stat_EtherStatsCollisions;

	sc->stat_EtherStatsFragments =
		stats->stat_EtherStatsFragments;

	sc->stat_EtherStatsJabbers =
		stats->stat_EtherStatsJabbers;

	sc->stat_EtherStatsUndersizePkts =
		stats->stat_EtherStatsUndersizePkts;

	sc->stat_EtherStatsOverrsizePkts =
		stats->stat_EtherStatsOverrsizePkts;

	sc->stat_EtherStatsPktsRx64Octets =
		stats->stat_EtherStatsPktsRx64Octets;

	sc->stat_EtherStatsPktsRx65Octetsto127Octets =
		stats->stat_EtherStatsPktsRx65Octetsto127Octets;

	sc->stat_EtherStatsPktsRx128Octetsto255Octets =
		stats->stat_EtherStatsPktsRx128Octetsto255Octets;

	sc->stat_EtherStatsPktsRx256Octetsto511Octets =
		stats->stat_EtherStatsPktsRx256Octetsto511Octets;

	sc->stat_EtherStatsPktsRx512Octetsto1023Octets =
		stats->stat_EtherStatsPktsRx512Octetsto1023Octets;

	sc->stat_EtherStatsPktsRx1024Octetsto1522Octets =
		stats->stat_EtherStatsPktsRx1024Octetsto1522Octets;

	sc->stat_EtherStatsPktsRx1523Octetsto9022Octets =
		stats->stat_EtherStatsPktsRx1523Octetsto9022Octets;

	sc->stat_EtherStatsPktsTx64Octets =
		stats->stat_EtherStatsPktsTx64Octets;

	sc->stat_EtherStatsPktsTx65Octetsto127Octets =
		stats->stat_EtherStatsPktsTx65Octetsto127Octets;

	sc->stat_EtherStatsPktsTx128Octetsto255Octets =
		stats->stat_EtherStatsPktsTx128Octetsto255Octets;

	sc->stat_EtherStatsPktsTx256Octetsto511Octets =
		stats->stat_EtherStatsPktsTx256Octetsto511Octets;

	sc->stat_EtherStatsPktsTx512Octetsto1023Octets =
		stats->stat_EtherStatsPktsTx512Octetsto1023Octets;

	sc->stat_EtherStatsPktsTx1024Octetsto1522Octets =
		stats->stat_EtherStatsPktsTx1024Octetsto1522Octets;

	sc->stat_EtherStatsPktsTx1523Octetsto9022Octets =
		stats->stat_EtherStatsPktsTx1523Octetsto9022Octets;

	sc->stat_XonPauseFramesReceived =
		stats->stat_XonPauseFramesReceived;

	sc->stat_XoffPauseFramesReceived =
		stats->stat_XoffPauseFramesReceived;

	sc->stat_OutXonSent =
		stats->stat_OutXonSent;

	sc->stat_OutXoffSent =
		stats->stat_OutXoffSent;

	sc->stat_FlowControlDone =
		stats->stat_FlowControlDone;

	sc->stat_MacControlFramesReceived =
		stats->stat_MacControlFramesReceived;

	sc->stat_XoffStateEntered =
		stats->stat_XoffStateEntered;

	sc->stat_IfInFramesL2FilterDiscards =
		stats->stat_IfInFramesL2FilterDiscards;

	sc->stat_IfInRuleCheckerDiscards =
		stats->stat_IfInRuleCheckerDiscards;

	sc->stat_IfInFTQDiscards =
		stats->stat_IfInFTQDiscards;

	sc->stat_IfInMBUFDiscards =
		stats->stat_IfInMBUFDiscards;

	sc->stat_IfInRuleCheckerP4Hit =
		stats->stat_IfInRuleCheckerP4Hit;

	sc->stat_CatchupInRuleCheckerDiscards =
		stats->stat_CatchupInRuleCheckerDiscards;

	sc->stat_CatchupInFTQDiscards =
		stats->stat_CatchupInFTQDiscards;

	sc->stat_CatchupInMBUFDiscards =
		stats->stat_CatchupInMBUFDiscards;

	sc->stat_CatchupInRuleCheckerP4Hit =
		stats->stat_CatchupInRuleCheckerP4Hit;

	DBPRINT(sc, BCE_EXCESSIVE, "Exiting %s()\n", __FUNCTION__);
}


static void
bce_tick(void *xsc)
{
	struct bce_softc *sc = xsc;
	struct mii_data *mii = NULL;
	struct ifnet *ifp;
	u32 msg;
	int s;

	ifp = &sc->arpcom.ac_if;

	s = splimp();

	/* Tell the firmware that the driver is still running. */
#ifdef BCE_DEBUG
	msg = (u32) BCE_DRV_MSG_DATA_PULSE_CODE_ALWAYS_ALIVE;
#else
	msg = (u32) ++sc->bce_fw_drv_pulse_wr_seq;
#endif
	REG_WR_IND(sc, sc->bce_shmem_base + BCE_DRV_PULSE_MB, msg);

	/* Update the statistics from the hardware statistics block. */
	bce_stats_update(sc);

	/* Schedule the next tick. */
	callout_reset(
		&sc->bce_stat_ch,		/* callout */
		hz, 					/* ticks */
		bce_tick, 				/* function */
		sc);					/* function argument */

	/* If link is up already up then we're done. */
	if (sc->bce_link)
		goto bce_tick_locked_exit;

	/* DRC - ToDo: Add SerDes support and check SerDes link here. */

	mii = device_get_softc(sc->bce_miibus);
	mii_tick(mii);

	/* Check if the link has come up. */
	if (!sc->bce_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->bce_link++;
		if ((IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_TX ||
		    IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_SX) &&
		    bootverbose)
			BCE_PRINTF(sc, "Gigabit link up\n");
		/* Now that link is up, handle any outstanding TX traffic. */
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			bce_start(ifp);
	}

bce_tick_locked_exit:
	splx(s);
	return;
}


#ifdef BCE_DEBUG
/****************************************************************************/
/* Allows the driver state to be dumped through the sysctl interface.       */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_sysctl_driver_state(SYSCTL_HANDLER_ARGS)
{
        int error;
        int result;
        struct bce_softc *sc;

        result = -1;
        error = sysctl_handle_int(oidp, &result, 0, req);

        if (error || !req->newptr)
                return (error);

        if (result == 1) {
                sc = (struct bce_softc *)arg1;
                bce_dump_driver_state(sc);
        }

        return error;
}


/****************************************************************************/
/* Allows the hardware state to be dumped through the sysctl interface.     */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_sysctl_hw_state(SYSCTL_HANDLER_ARGS)
{
        int error;
        int result;
        struct bce_softc *sc;

        result = -1;
        error = sysctl_handle_int(oidp, &result, 0, req);

        if (error || !req->newptr)
                return (error);

        if (result == 1) {
                sc = (struct bce_softc *)arg1;
                bce_dump_hw_state(sc);
        }

        return error;
}


/****************************************************************************/
/*                                                                          */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_sysctl_dump_rx_chain(SYSCTL_HANDLER_ARGS)
{
        int error;
        int result;
        struct bce_softc *sc;

        result = -1;
        error = sysctl_handle_int(oidp, &result, 0, req);

        if (error || !req->newptr)
                return (error);

        if (result == 1) {
                sc = (struct bce_softc *)arg1;
                bce_dump_rx_chain(sc, 0, USABLE_RX_BD);
        }

        return error;
}


/****************************************************************************/
/*                                                                          */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static int
bce_sysctl_breakpoint(SYSCTL_HANDLER_ARGS)
{
        int error;
        int result;
        struct bce_softc *sc;

        result = -1;
        error = sysctl_handle_int(oidp, &result, 0, req);

        if (error || !req->newptr)
                return (error);

        if (result == 1) {
                sc = (struct bce_softc *)arg1;
                bce_breakpoint(sc);
        }

        return error;
}
#endif


/****************************************************************************/
/* Adds any sysctl parameters for tuning or debugging purposes.             */
/*                                                                          */
/* Returns:                                                                 */
/*   0 for success, positive value for failure.                             */
/****************************************************************************/
static void
bce_add_sysctls(struct bce_softc *sc)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;

	ctx = &sc->sysctl_ctx;
	sysctl_ctx_init(ctx);
	sc->sysctl_tree = SYSCTL_ADD_NODE(ctx, SYSCTL_STATIC_CHILDREN(_hw),
	    OID_AUTO, device_get_nameunit(sc->bce_dev), CTLFLAG_RD, 0, "");
	children = SYSCTL_CHILDREN(sc->sysctl_tree);

	SYSCTL_ADD_STRING(ctx, children, OID_AUTO,
		"driver_version",
		CTLFLAG_RD, &bce_driver_version,
		0, "bce driver version");

#ifdef BCE_DEBUG
	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		"rx_low_watermark",
		CTLFLAG_RD, &sc->rx_low_watermark,
		0, "Lowest level of free rx_bd's");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		"tx_hi_watermark",
		CTLFLAG_RD, &sc->tx_hi_watermark,
		0, "Highest level of used tx_bd's");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		"l2fhdr_status_errors",
		CTLFLAG_RD, &sc->l2fhdr_status_errors,
		0, "l2_fhdr status errors");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		"unexpected_attentions",
		CTLFLAG_RD, &sc->unexpected_attentions,
		0, "unexpected attentions");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		"lost_status_block_updates",
		CTLFLAG_RD, &sc->lost_status_block_updates,
		0, "lost status block updates");

	SYSCTL_ADD_INT(ctx, children, OID_AUTO, 
		"mbuf_alloc_failed",
		CTLFLAG_RD, &sc->mbuf_alloc_failed,
		0, "mbuf cluster allocation failures");
#endif 

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHcInOctets",
		CTLFLAG_RD, &sc->stat_IfHCInOctets,
		"Bytes received");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHCInBadOctets",
		CTLFLAG_RD, &sc->stat_IfHCInBadOctets,
		"Bad bytes received");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHCOutOctets",
		CTLFLAG_RD, &sc->stat_IfHCOutOctets,
		"Bytes sent");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHCOutBadOctets",
		CTLFLAG_RD, &sc->stat_IfHCOutBadOctets,
		"Bad bytes sent");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHCInUcastPkts",
		CTLFLAG_RD, &sc->stat_IfHCInUcastPkts,
		"Unicast packets received");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHCInMulticastPkts",
		CTLFLAG_RD, &sc->stat_IfHCInMulticastPkts,
		"Multicast packets received");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHCInBroadcastPkts",
		CTLFLAG_RD, &sc->stat_IfHCInBroadcastPkts,
		"Broadcast packets received");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHCOutUcastPkts",
		CTLFLAG_RD, &sc->stat_IfHCOutUcastPkts,
		"Unicast packets sent");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHCOutMulticastPkts",
		CTLFLAG_RD, &sc->stat_IfHCOutMulticastPkts,
		"Multicast packets sent");

	SYSCTL_ADD_ULONG(ctx, children, OID_AUTO, 
		"stat_IfHCOutBroadcastPkts",
		CTLFLAG_RD, &sc->stat_IfHCOutBroadcastPkts,
		"Broadcast packets sent");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_emac_tx_stat_dot3statsinternalmactransmiterrors",
		CTLFLAG_RD, &sc->stat_emac_tx_stat_dot3statsinternalmactransmiterrors,
		0, "Internal MAC transmit errors");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_Dot3StatsCarrierSenseErrors",
		CTLFLAG_RD, &sc->stat_Dot3StatsCarrierSenseErrors,
		0, "Carrier sense errors");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_Dot3StatsFCSErrors",
		CTLFLAG_RD, &sc->stat_Dot3StatsFCSErrors,
		0, "Frame check sequence errors");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_Dot3StatsAlignmentErrors",
		CTLFLAG_RD, &sc->stat_Dot3StatsAlignmentErrors,
		0, "Alignment errors");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_Dot3StatsSingleCollisionFrames",
		CTLFLAG_RD, &sc->stat_Dot3StatsSingleCollisionFrames,
		0, "Single Collision Frames");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_Dot3StatsMultipleCollisionFrames",
		CTLFLAG_RD, &sc->stat_Dot3StatsMultipleCollisionFrames,
		0, "Multiple Collision Frames");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_Dot3StatsDeferredTransmissions",
		CTLFLAG_RD, &sc->stat_Dot3StatsDeferredTransmissions,
		0, "Deferred Transmissions");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_Dot3StatsExcessiveCollisions",
		CTLFLAG_RD, &sc->stat_Dot3StatsExcessiveCollisions,
		0, "Excessive Collisions");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_Dot3StatsLateCollisions",
		CTLFLAG_RD, &sc->stat_Dot3StatsLateCollisions,
		0, "Late Collisions");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsCollisions",
		CTLFLAG_RD, &sc->stat_EtherStatsCollisions,
		0, "Collisions");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsFragments",
		CTLFLAG_RD, &sc->stat_EtherStatsFragments,
		0, "Fragments");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsJabbers",
		CTLFLAG_RD, &sc->stat_EtherStatsJabbers,
		0, "Jabbers");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsUndersizePkts",
		CTLFLAG_RD, &sc->stat_EtherStatsUndersizePkts,
		0, "Undersize packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsOverrsizePkts",
		CTLFLAG_RD, &sc->stat_EtherStatsOverrsizePkts,
		0, "stat_EtherStatsOverrsizePkts");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsRx64Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsRx64Octets,
		0, "Bytes received in 64 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsRx65Octetsto127Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsRx65Octetsto127Octets,
		0, "Bytes received in 65 to 127 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsRx128Octetsto255Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsRx128Octetsto255Octets,
		0, "Bytes received in 128 to 255 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsRx256Octetsto511Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsRx256Octetsto511Octets,
		0, "Bytes received in 256 to 511 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsRx512Octetsto1023Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsRx512Octetsto1023Octets,
		0, "Bytes received in 512 to 1023 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsRx1024Octetsto1522Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsRx1024Octetsto1522Octets,
		0, "Bytes received in 1024 t0 1522 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsRx1523Octetsto9022Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsRx1523Octetsto9022Octets,
		0, "Bytes received in 1523 to 9022 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsTx64Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsTx64Octets,
		0, "Bytes sent in 64 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsTx65Octetsto127Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsTx65Octetsto127Octets,
		0, "Bytes sent in 65 to 127 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsTx128Octetsto255Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsTx128Octetsto255Octets,
		0, "Bytes sent in 128 to 255 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsTx256Octetsto511Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsTx256Octetsto511Octets,
		0, "Bytes sent in 256 to 511 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsTx512Octetsto1023Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsTx512Octetsto1023Octets,
		0, "Bytes sent in 512 to 1023 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsTx1024Octetsto1522Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsTx1024Octetsto1522Octets,
		0, "Bytes sent in 1024 to 1522 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_EtherStatsPktsTx1523Octetsto9022Octets",
		CTLFLAG_RD, &sc->stat_EtherStatsPktsTx1523Octetsto9022Octets,
		0, "Bytes sent in 1523 to 9022 byte packets");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_XonPauseFramesReceived",
		CTLFLAG_RD, &sc->stat_XonPauseFramesReceived,
		0, "XON pause frames receved");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_XoffPauseFramesReceived",
		CTLFLAG_RD, &sc->stat_XoffPauseFramesReceived,
		0, "XOFF pause frames received");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_OutXonSent",
		CTLFLAG_RD, &sc->stat_OutXonSent,
		0, "XON pause frames sent");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_OutXoffSent",
		CTLFLAG_RD, &sc->stat_OutXoffSent,
		0, "XOFF pause frames sent");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_FlowControlDone",
		CTLFLAG_RD, &sc->stat_FlowControlDone,
		0, "Flow control done");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_MacControlFramesReceived",
		CTLFLAG_RD, &sc->stat_MacControlFramesReceived,
		0, "MAC control frames received");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_XoffStateEntered",
		CTLFLAG_RD, &sc->stat_XoffStateEntered,
		0, "XOFF state entered");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_IfInFramesL2FilterDiscards",
		CTLFLAG_RD, &sc->stat_IfInFramesL2FilterDiscards,
		0, "Received L2 packets discarded");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_IfInRuleCheckerDiscards",
		CTLFLAG_RD, &sc->stat_IfInRuleCheckerDiscards,
		0, "Received packets discarded by rule");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_IfInFTQDiscards",
		CTLFLAG_RD, &sc->stat_IfInFTQDiscards,
		0, "Received packet FTQ discards");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_IfInMBUFDiscards",
		CTLFLAG_RD, &sc->stat_IfInMBUFDiscards,
		0, "Received packets discarded due to lack of controller buffer memory");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_IfInRuleCheckerP4Hit",
		CTLFLAG_RD, &sc->stat_IfInRuleCheckerP4Hit,
		0, "Received packets rule checker hits");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_CatchupInRuleCheckerDiscards",
		CTLFLAG_RD, &sc->stat_CatchupInRuleCheckerDiscards,
		0, "Received packets discarded in Catchup path");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_CatchupInFTQDiscards",
		CTLFLAG_RD, &sc->stat_CatchupInFTQDiscards,
		0, "Received packets discarded in FTQ in Catchup path");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_CatchupInMBUFDiscards",
		CTLFLAG_RD, &sc->stat_CatchupInMBUFDiscards,
		0, "Received packets discarded in controller buffer memory in Catchup path");

	SYSCTL_ADD_UINT(ctx, children, OID_AUTO, 
		"stat_CatchupInRuleCheckerP4Hit",
		CTLFLAG_RD, &sc->stat_CatchupInRuleCheckerP4Hit,
		0, "Received packets rule checker hits in Catchup path");

#ifdef BCE_DEBUG
	SYSCTL_ADD_PROC(ctx, children, OID_AUTO,
		"driver_state", CTLTYPE_INT | CTLFLAG_RW,
		(void *)sc, 0,
		bce_sysctl_driver_state, "I", "Drive state information");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO,
		"hw_state", CTLTYPE_INT | CTLFLAG_RW,
		(void *)sc, 0,
		bce_sysctl_hw_state, "I", "Hardware state information");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO,
		"dump_rx_chain", CTLTYPE_INT | CTLFLAG_RW,
		(void *)sc, 0,
		bce_sysctl_dump_rx_chain, "I", "Dump rx_bd chain");

	SYSCTL_ADD_PROC(ctx, children, OID_AUTO,
		"breakpoint", CTLTYPE_INT | CTLFLAG_RW,
		(void *)sc, 0,
		bce_sysctl_breakpoint, "I", "Driver breakpoint");
#endif

}

static void
bce_remove_sysctls(struct bce_softc *sc)
{

	sysctl_ctx_free(&sc->sysctl_ctx);
	sc->sysctl_tree = NULL;
}

/****************************************************************************/
/* BCE Debug Routines                                                       */
/****************************************************************************/
#ifdef BCE_DEBUG

/****************************************************************************/
/* Prints out information about an mbuf.                                    */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_dump_mbuf(struct bce_softc *sc, struct mbuf *m)
{
	u32 val_hi, val_lo;
	struct mbuf *mp = m;

	if (m == NULL) {
		/* Index out of range. */
		printf("mbuf ptr is null!\n");
		return;
	}

	while (mp) {
		val_hi = BCE_ADDR_HI(mp);
		val_lo = BCE_ADDR_LO(mp);
		BCE_PRINTF(sc, "mbuf: vaddr = 0x%08X:%08X, m_len = %d, m_flags = ", 
			   val_hi, val_lo, mp->m_len);

		if (mp->m_flags & M_EXT)
			printf("M_EXT ");
		if (mp->m_flags & M_PKTHDR)
			printf("M_PKTHDR ");
		printf("\n");

		if (mp->m_flags & M_EXT) {
			val_hi = BCE_ADDR_HI(mp->m_ext.ext_buf);
			val_lo = BCE_ADDR_LO(mp->m_ext.ext_buf);
			BCE_PRINTF(sc, "- m_ext: vaddr = 0x%08X:%08X, ext_size = 0x%04X\n", 
				val_hi, val_lo, mp->m_ext.ext_size);
		}

		mp = mp->m_next;
	}


}


/****************************************************************************/
/* Prints out the mbufs in the TX mbuf chain.                               */
/*                                                                          */
/* Returns:                                                                 */
/*   Nothing.                                                               */
/****************************************************************************/
static void
bce_dump_tx_mbuf_chain(struct bce_softc *sc, int chain_prod, int count)
{
	struct mbuf *m;

	BCE_PRINTF(sc,
		"----------------------------"
		"  tx mbuf data  "
		"----------------------------\n");

	for (int i = 0; i < count; i++) {
	 	m = sc->tx_mbuf_ptr[chain_prod];
		BCE_PRINTF(sc, "txmbuf[%d]\n", chain_prod);
		bce_dump_mbuf(sc, m);
		chain_prod = TX_CHAIN_IDX(NEXT_TX_BD(chain_prod));
	}

	BCE_PRINTF(sc,
		"----------------------------"
		"----------------"
		"----------------------------\n");
}


/*
 * This routine prints the RX mbuf chain.
 */
static void
bce_dump_rx_mbuf_chain(struct bce_softc *sc, int chain_prod, int count)
{
	struct mbuf *m;

	BCE_PRINTF(sc,
		"----------------------------"
		"  rx mbuf data  "
		"----------------------------\n");

	for (int i = 0; i < count; i++) {
	 	m = sc->rx_mbuf_ptr[chain_prod];
		BCE_PRINTF(sc, "rxmbuf[0x%04X]\n", chain_prod);
		bce_dump_mbuf(sc, m);
		chain_prod = RX_CHAIN_IDX(NEXT_RX_BD(chain_prod));
	}


	BCE_PRINTF(sc,
		"----------------------------"
		"----------------"
		"----------------------------\n");
}


static void
bce_dump_txbd(struct bce_softc *sc, int idx, struct tx_bd *txbd)
{
	if (idx > MAX_TX_BD)
		/* Index out of range. */
		BCE_PRINTF(sc, "tx_bd[0x%04X]: Invalid tx_bd index!\n", idx);
	else if ((idx & USABLE_TX_BD_PER_PAGE) == USABLE_TX_BD_PER_PAGE)
		/* TX Chain page pointer. */
		BCE_PRINTF(sc, "tx_bd[0x%04X]: haddr = 0x%08X:%08X, chain page pointer\n", 
			idx, txbd->tx_bd_haddr_hi, txbd->tx_bd_haddr_lo);
	else
		/* Normal tx_bd entry. */
		BCE_PRINTF(sc, "tx_bd[0x%04X]: haddr = 0x%08X:%08X, nbytes = 0x%08X, "
			"flags = 0x%08X\n", idx, 
			txbd->tx_bd_haddr_hi, txbd->tx_bd_haddr_lo,
			txbd->tx_bd_mss_nbytes, txbd->tx_bd_vlan_tag_flags);
}


static void
bce_dump_rxbd(struct bce_softc *sc, int idx, struct rx_bd *rxbd)
{
	if (idx > MAX_RX_BD)
		/* Index out of range. */
		BCE_PRINTF(sc, "rx_bd[0x%04X]: Invalid rx_bd index!\n", idx);
	else if ((idx & USABLE_RX_BD_PER_PAGE) == USABLE_RX_BD_PER_PAGE)
		/* TX Chain page pointer. */
		BCE_PRINTF(sc, "rx_bd[0x%04X]: haddr = 0x%08X:%08X, chain page pointer\n", 
			idx, rxbd->rx_bd_haddr_hi, rxbd->rx_bd_haddr_lo);
	else
		/* Normal tx_bd entry. */
		BCE_PRINTF(sc, "rx_bd[0x%04X]: haddr = 0x%08X:%08X, nbytes = 0x%08X, "
			"flags = 0x%08X\n", idx, 
			rxbd->rx_bd_haddr_hi, rxbd->rx_bd_haddr_lo,
			rxbd->rx_bd_len, rxbd->rx_bd_flags);
}


static void
bce_dump_l2fhdr(struct bce_softc *sc, int idx, struct l2_fhdr *l2fhdr)
{
	BCE_PRINTF(sc, "l2_fhdr[0x%04X]: status = 0x%08X, "
		"pkt_len = 0x%04X, vlan = 0x%04x, ip_xsum = 0x%04X, "
		"tcp_udp_xsum = 0x%04X\n", idx,
		l2fhdr->l2_fhdr_status, l2fhdr->l2_fhdr_pkt_len,
		l2fhdr->l2_fhdr_vlan_tag, l2fhdr->l2_fhdr_ip_xsum,
		l2fhdr->l2_fhdr_tcp_udp_xsum);
}


/*
 * This routine prints the TX chain.
 */
static void
bce_dump_tx_chain(struct bce_softc *sc, int tx_prod, int count)
{
	struct tx_bd *txbd;

	/* First some info about the tx_bd chain structure. */
	BCE_PRINTF(sc,
		"----------------------------"
		"  tx_bd  chain  "
		"----------------------------\n");

	BCE_PRINTF(sc, "page size      = 0x%08X, tx chain pages        = 0x%08X\n",
		(u32) BCM_PAGE_SIZE, (u32) TX_PAGES);

	BCE_PRINTF(sc, "tx_bd per page = 0x%08X, usable tx_bd per page = 0x%08X\n",
		(u32) TOTAL_TX_BD_PER_PAGE, (u32) USABLE_TX_BD_PER_PAGE);

	BCE_PRINTF(sc, "total tx_bd    = 0x%08X\n", (u32) TOTAL_TX_BD);

	BCE_PRINTF(sc, ""
		"-----------------------------"
		"   tx_bd data   "
		"-----------------------------\n");

	/* Now print out the tx_bd's themselves. */
	for (int i = 0; i < count; i++) {
	 	txbd = &sc->tx_bd_chain[TX_PAGE(tx_prod)][TX_IDX(tx_prod)];
		bce_dump_txbd(sc, tx_prod, txbd);
		tx_prod = TX_CHAIN_IDX(NEXT_TX_BD(tx_prod));
	}

	BCE_PRINTF(sc,
		"-----------------------------"
		"--------------"
		"-----------------------------\n");
}


/*
 * This routine prints the RX chain.
 */
static void
bce_dump_rx_chain(struct bce_softc *sc, int rx_prod, int count)
{
	struct rx_bd *rxbd;

	/* First some info about the tx_bd chain structure. */
	BCE_PRINTF(sc,
		"----------------------------"
		"  rx_bd  chain  "
		"----------------------------\n");

	BCE_PRINTF(sc, "----- RX_BD Chain -----\n");

	BCE_PRINTF(sc, "page size      = 0x%08X, rx chain pages        = 0x%08X\n",
		(u32) BCM_PAGE_SIZE, (u32) RX_PAGES);

	BCE_PRINTF(sc, "rx_bd per page = 0x%08X, usable rx_bd per page = 0x%08X\n",
		(u32) TOTAL_RX_BD_PER_PAGE, (u32) USABLE_RX_BD_PER_PAGE);

	BCE_PRINTF(sc, "total rx_bd    = 0x%08X\n", (u32) TOTAL_RX_BD);

	BCE_PRINTF(sc,
		"----------------------------"
		"   rx_bd data   "
		"----------------------------\n");

	/* Now print out the rx_bd's themselves. */
	for (int i = 0; i < count; i++) {
		rxbd = &sc->rx_bd_chain[RX_PAGE(rx_prod)][RX_IDX(rx_prod)];
		bce_dump_rxbd(sc, rx_prod, rxbd);
		rx_prod = RX_CHAIN_IDX(NEXT_RX_BD(rx_prod));
	}

	BCE_PRINTF(sc,
		"----------------------------"
		"--------------"
		"----------------------------\n");
}


/*
 * This routine prints the status block.
 */
static void
bce_dump_status_block(struct bce_softc *sc)
{
	struct status_block *sblk;

	sblk = sc->status_block;

   	BCE_PRINTF(sc, "----------------------------- Status Block "
		"-----------------------------\n");

	BCE_PRINTF(sc, "attn_bits  = 0x%08X, attn_bits_ack = 0x%08X, index = 0x%04X\n",
		sblk->status_attn_bits, sblk->status_attn_bits_ack,
		sblk->status_idx);

	BCE_PRINTF(sc, "rx_cons0   = 0x%08X, tx_cons0      = 0x%08X\n",
		sblk->status_rx_quick_consumer_index0,
		sblk->status_tx_quick_consumer_index0);

	BCE_PRINTF(sc, "status_idx = 0x%04X\n", sblk->status_idx);

	/* Theses indices are not used for normal L2 drivers. */
	if (sblk->status_rx_quick_consumer_index1 || 
		sblk->status_tx_quick_consumer_index1)
		BCE_PRINTF(sc, "rx_cons1  = 0x%08X, tx_cons1      = 0x%08X\n",
			sblk->status_rx_quick_consumer_index1,
			sblk->status_tx_quick_consumer_index1);

	if (sblk->status_rx_quick_consumer_index2 || 
		sblk->status_tx_quick_consumer_index2)
		BCE_PRINTF(sc, "rx_cons2  = 0x%08X, tx_cons2      = 0x%08X\n",
			sblk->status_rx_quick_consumer_index2,
			sblk->status_tx_quick_consumer_index2);

	if (sblk->status_rx_quick_consumer_index3 || 
		sblk->status_tx_quick_consumer_index3)
		BCE_PRINTF(sc, "rx_cons3  = 0x%08X, tx_cons3      = 0x%08X\n",
			sblk->status_rx_quick_consumer_index3,
			sblk->status_tx_quick_consumer_index3);

	if (sblk->status_rx_quick_consumer_index4 || 
		sblk->status_rx_quick_consumer_index5)
		BCE_PRINTF(sc, "rx_cons4  = 0x%08X, rx_cons5      = 0x%08X\n",
			sblk->status_rx_quick_consumer_index4,
			sblk->status_rx_quick_consumer_index5);

	if (sblk->status_rx_quick_consumer_index6 || 
		sblk->status_rx_quick_consumer_index7)
		BCE_PRINTF(sc, "rx_cons6  = 0x%08X, rx_cons7      = 0x%08X\n",
			sblk->status_rx_quick_consumer_index6,
			sblk->status_rx_quick_consumer_index7);

	if (sblk->status_rx_quick_consumer_index8 || 
		sblk->status_rx_quick_consumer_index9)
		BCE_PRINTF(sc, "rx_cons8  = 0x%08X, rx_cons9      = 0x%08X\n",
			sblk->status_rx_quick_consumer_index8,
			sblk->status_rx_quick_consumer_index9);

	if (sblk->status_rx_quick_consumer_index10 || 
		sblk->status_rx_quick_consumer_index11)
		BCE_PRINTF(sc, "rx_cons10 = 0x%08X, rx_cons11     = 0x%08X\n",
			sblk->status_rx_quick_consumer_index10,
			sblk->status_rx_quick_consumer_index11);

	if (sblk->status_rx_quick_consumer_index12 || 
		sblk->status_rx_quick_consumer_index13)
		BCE_PRINTF(sc, "rx_cons12 = 0x%08X, rx_cons13     = 0x%08X\n",
			sblk->status_rx_quick_consumer_index12,
			sblk->status_rx_quick_consumer_index13);

	if (sblk->status_rx_quick_consumer_index14 || 
		sblk->status_rx_quick_consumer_index15)
		BCE_PRINTF(sc, "rx_cons14 = 0x%08X, rx_cons15     = 0x%08X\n",
			sblk->status_rx_quick_consumer_index14,
			sblk->status_rx_quick_consumer_index15);

	if (sblk->status_completion_producer_index || 
		sblk->status_cmd_consumer_index)
		BCE_PRINTF(sc, "com_prod  = 0x%08X, cmd_cons      = 0x%08X\n",
			sblk->status_completion_producer_index,
			sblk->status_cmd_consumer_index);

	BCE_PRINTF(sc, "-------------------------------------------"
		"-----------------------------\n");
}


/*
 * This routine prints the statistics block.
 */
static void
bce_dump_stats_block(struct bce_softc *sc)
{
	struct statistics_block *sblk;

	sblk = sc->stats_block;

	BCE_PRINTF(sc, ""
		"-----------------------------"
		" Stats  Block "
		"-----------------------------\n");

	BCE_PRINTF(sc, "IfHcInOctets         = 0x%08X:%08X, "
		"IfHcInBadOctets      = 0x%08X:%08X\n",
		sblk->stat_IfHCInOctets_hi, sblk->stat_IfHCInOctets_lo,
		sblk->stat_IfHCInBadOctets_hi, sblk->stat_IfHCInBadOctets_lo);

	BCE_PRINTF(sc, "IfHcOutOctets        = 0x%08X:%08X, "
		"IfHcOutBadOctets     = 0x%08X:%08X\n",
		sblk->stat_IfHCOutOctets_hi, sblk->stat_IfHCOutOctets_lo,
		sblk->stat_IfHCOutBadOctets_hi, sblk->stat_IfHCOutBadOctets_lo);

	BCE_PRINTF(sc, "IfHcInUcastPkts      = 0x%08X:%08X, "
		"IfHcInMulticastPkts  = 0x%08X:%08X\n",
		sblk->stat_IfHCInUcastPkts_hi, sblk->stat_IfHCInUcastPkts_lo,
		sblk->stat_IfHCInMulticastPkts_hi, sblk->stat_IfHCInMulticastPkts_lo);

	BCE_PRINTF(sc, "IfHcInBroadcastPkts  = 0x%08X:%08X, "
		"IfHcOutUcastPkts     = 0x%08X:%08X\n",
		sblk->stat_IfHCInBroadcastPkts_hi, sblk->stat_IfHCInBroadcastPkts_lo,
		sblk->stat_IfHCOutUcastPkts_hi, sblk->stat_IfHCOutUcastPkts_lo);

	BCE_PRINTF(sc, "IfHcOutMulticastPkts = 0x%08X:%08X, IfHcOutBroadcastPkts = 0x%08X:%08X\n",
		sblk->stat_IfHCOutMulticastPkts_hi, sblk->stat_IfHCOutMulticastPkts_lo,
		sblk->stat_IfHCOutBroadcastPkts_hi, sblk->stat_IfHCOutBroadcastPkts_lo);

	if (sblk->stat_emac_tx_stat_dot3statsinternalmactransmiterrors)
		BCE_PRINTF(sc, "0x%08X : "
		"emac_tx_stat_dot3statsinternalmactransmiterrors\n", 
		sblk->stat_emac_tx_stat_dot3statsinternalmactransmiterrors);

	if (sblk->stat_Dot3StatsCarrierSenseErrors)
		BCE_PRINTF(sc, "0x%08X : Dot3StatsCarrierSenseErrors\n",
			sblk->stat_Dot3StatsCarrierSenseErrors);

	if (sblk->stat_Dot3StatsFCSErrors)
		BCE_PRINTF(sc, "0x%08X : Dot3StatsFCSErrors\n",
			sblk->stat_Dot3StatsFCSErrors);

	if (sblk->stat_Dot3StatsAlignmentErrors)
		BCE_PRINTF(sc, "0x%08X : Dot3StatsAlignmentErrors\n",
			sblk->stat_Dot3StatsAlignmentErrors);

	if (sblk->stat_Dot3StatsSingleCollisionFrames)
		BCE_PRINTF(sc, "0x%08X : Dot3StatsSingleCollisionFrames\n",
			sblk->stat_Dot3StatsSingleCollisionFrames);

	if (sblk->stat_Dot3StatsMultipleCollisionFrames)
		BCE_PRINTF(sc, "0x%08X : Dot3StatsMultipleCollisionFrames\n",
			sblk->stat_Dot3StatsMultipleCollisionFrames);
	
	if (sblk->stat_Dot3StatsDeferredTransmissions)
		BCE_PRINTF(sc, "0x%08X : Dot3StatsDeferredTransmissions\n",
			sblk->stat_Dot3StatsDeferredTransmissions);

	if (sblk->stat_Dot3StatsExcessiveCollisions)
		BCE_PRINTF(sc, "0x%08X : Dot3StatsExcessiveCollisions\n",
			sblk->stat_Dot3StatsExcessiveCollisions);

	if (sblk->stat_Dot3StatsLateCollisions)
		BCE_PRINTF(sc, "0x%08X : Dot3StatsLateCollisions\n",
			sblk->stat_Dot3StatsLateCollisions);

	if (sblk->stat_EtherStatsCollisions)
		BCE_PRINTF(sc, "0x%08X : EtherStatsCollisions\n",
			sblk->stat_EtherStatsCollisions);

	if (sblk->stat_EtherStatsFragments) 
		BCE_PRINTF(sc, "0x%08X : EtherStatsFragments\n",
			sblk->stat_EtherStatsFragments);

	if (sblk->stat_EtherStatsJabbers)
		BCE_PRINTF(sc, "0x%08X : EtherStatsJabbers\n",
			sblk->stat_EtherStatsJabbers);

	if (sblk->stat_EtherStatsUndersizePkts)
		BCE_PRINTF(sc, "0x%08X : EtherStatsUndersizePkts\n",
			sblk->stat_EtherStatsUndersizePkts);

	if (sblk->stat_EtherStatsOverrsizePkts)
		BCE_PRINTF(sc, "0x%08X : EtherStatsOverrsizePkts\n",
			sblk->stat_EtherStatsOverrsizePkts);

	if (sblk->stat_EtherStatsPktsRx64Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsRx64Octets\n",
			sblk->stat_EtherStatsPktsRx64Octets);

	if (sblk->stat_EtherStatsPktsRx65Octetsto127Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsRx65Octetsto127Octets\n",
			sblk->stat_EtherStatsPktsRx65Octetsto127Octets);

	if (sblk->stat_EtherStatsPktsRx128Octetsto255Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsRx128Octetsto255Octets\n",
			sblk->stat_EtherStatsPktsRx128Octetsto255Octets);

	if (sblk->stat_EtherStatsPktsRx256Octetsto511Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsRx256Octetsto511Octets\n",
			sblk->stat_EtherStatsPktsRx256Octetsto511Octets);

	if (sblk->stat_EtherStatsPktsRx512Octetsto1023Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsRx512Octetsto1023Octets\n",
			sblk->stat_EtherStatsPktsRx512Octetsto1023Octets);

	if (sblk->stat_EtherStatsPktsRx1024Octetsto1522Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsRx1024Octetsto1522Octets\n",
			sblk->stat_EtherStatsPktsRx1024Octetsto1522Octets);

	if (sblk->stat_EtherStatsPktsRx1523Octetsto9022Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsRx1523Octetsto9022Octets\n",
			sblk->stat_EtherStatsPktsRx1523Octetsto9022Octets);

	if (sblk->stat_EtherStatsPktsTx64Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsTx64Octets\n",
			sblk->stat_EtherStatsPktsTx64Octets);

	if (sblk->stat_EtherStatsPktsTx65Octetsto127Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsTx65Octetsto127Octets\n",
			sblk->stat_EtherStatsPktsTx65Octetsto127Octets);

	if (sblk->stat_EtherStatsPktsTx128Octetsto255Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsTx128Octetsto255Octets\n",
			sblk->stat_EtherStatsPktsTx128Octetsto255Octets);

	if (sblk->stat_EtherStatsPktsTx256Octetsto511Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsTx256Octetsto511Octets\n",
			sblk->stat_EtherStatsPktsTx256Octetsto511Octets);

	if (sblk->stat_EtherStatsPktsTx512Octetsto1023Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsTx512Octetsto1023Octets\n",
			sblk->stat_EtherStatsPktsTx512Octetsto1023Octets);

	if (sblk->stat_EtherStatsPktsTx1024Octetsto1522Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsTx1024Octetsto1522Octets\n",
			sblk->stat_EtherStatsPktsTx1024Octetsto1522Octets);

	if (sblk->stat_EtherStatsPktsTx1523Octetsto9022Octets)
		BCE_PRINTF(sc, "0x%08X : EtherStatsPktsTx1523Octetsto9022Octets\n",
			sblk->stat_EtherStatsPktsTx1523Octetsto9022Octets);

	if (sblk->stat_XonPauseFramesReceived)
		BCE_PRINTF(sc, "0x%08X : XonPauseFramesReceived\n",
			sblk->stat_XonPauseFramesReceived);

	if (sblk->stat_XoffPauseFramesReceived)
	   BCE_PRINTF(sc, "0x%08X : XoffPauseFramesReceived\n",
			sblk->stat_XoffPauseFramesReceived);

	if (sblk->stat_OutXonSent)
		BCE_PRINTF(sc, "0x%08X : OutXonSent\n",
			sblk->stat_OutXonSent);

	if (sblk->stat_OutXoffSent)
		BCE_PRINTF(sc, "0x%08X : OutXoffSent\n",
			sblk->stat_OutXoffSent);

	if (sblk->stat_FlowControlDone)
		BCE_PRINTF(sc, "0x%08X : FlowControlDone\n",
			sblk->stat_FlowControlDone);

	if (sblk->stat_MacControlFramesReceived)
		BCE_PRINTF(sc, "0x%08X : MacControlFramesReceived\n",
			sblk->stat_MacControlFramesReceived);

	if (sblk->stat_XoffStateEntered)
		BCE_PRINTF(sc, "0x%08X : XoffStateEntered\n",
			sblk->stat_XoffStateEntered);

	if (sblk->stat_IfInFramesL2FilterDiscards)
		BCE_PRINTF(sc, "0x%08X : IfInFramesL2FilterDiscards\n",
			sblk->stat_IfInFramesL2FilterDiscards);

	if (sblk->stat_IfInRuleCheckerDiscards)
		BCE_PRINTF(sc, "0x%08X : IfInRuleCheckerDiscards\n",
			sblk->stat_IfInRuleCheckerDiscards);

	if (sblk->stat_IfInFTQDiscards)
		BCE_PRINTF(sc, "0x%08X : IfInFTQDiscards\n",
			sblk->stat_IfInFTQDiscards);

	if (sblk->stat_IfInMBUFDiscards)
		BCE_PRINTF(sc, "0x%08X : IfInMBUFDiscards\n",
			sblk->stat_IfInMBUFDiscards);

	if (sblk->stat_IfInRuleCheckerP4Hit)
		BCE_PRINTF(sc, "0x%08X : IfInRuleCheckerP4Hit\n",
			sblk->stat_IfInRuleCheckerP4Hit);

	if (sblk->stat_CatchupInRuleCheckerDiscards)
		BCE_PRINTF(sc, "0x%08X : CatchupInRuleCheckerDiscards\n",
			sblk->stat_CatchupInRuleCheckerDiscards);

	if (sblk->stat_CatchupInFTQDiscards)
		BCE_PRINTF(sc, "0x%08X : CatchupInFTQDiscards\n",
			sblk->stat_CatchupInFTQDiscards);

	if (sblk->stat_CatchupInMBUFDiscards)
		BCE_PRINTF(sc, "0x%08X : CatchupInMBUFDiscards\n",
			sblk->stat_CatchupInMBUFDiscards);

	if (sblk->stat_CatchupInRuleCheckerP4Hit)
		BCE_PRINTF(sc, "0x%08X : CatchupInRuleCheckerP4Hit\n",
			sblk->stat_CatchupInRuleCheckerP4Hit);

	BCE_PRINTF(sc,
		"-----------------------------"
		"--------------"
		"-----------------------------\n");
}


static void
bce_dump_driver_state(struct bce_softc *sc)
{
	u32 val_hi, val_lo;

	BCE_PRINTF(sc,
		"-----------------------------"
		" Driver State "
		"-----------------------------\n");

	val_hi = BCE_ADDR_HI(sc);
	val_lo = BCE_ADDR_LO(sc);
	BCE_PRINTF(sc, "0x%08X:%08X - (sc) driver softc structure virtual address\n",
		val_hi, val_lo);

	val_hi = BCE_ADDR_HI(sc->bce_vhandle);
	val_lo = BCE_ADDR_LO(sc->bce_vhandle);
	BCE_PRINTF(sc, "0x%08X:%08X - (sc->bce_vhandle) PCI BAR virtual address\n",
		val_hi, val_lo);

	val_hi = BCE_ADDR_HI(sc->status_block);
	val_lo = BCE_ADDR_LO(sc->status_block);
	BCE_PRINTF(sc, "0x%08X:%08X - (sc->status_block) status block virtual address\n",
		val_hi, val_lo);

	val_hi = BCE_ADDR_HI(sc->stats_block);
	val_lo = BCE_ADDR_LO(sc->stats_block);
	BCE_PRINTF(sc, "0x%08X:%08X - (sc->stats_block) statistics block virtual address\n",
		val_hi, val_lo);

	val_hi = BCE_ADDR_HI(sc->tx_bd_chain);
	val_lo = BCE_ADDR_LO(sc->tx_bd_chain);
	BCE_PRINTF(sc,
		"0x%08X:%08X - (sc->tx_bd_chain) tx_bd chain virtual adddress\n",
		val_hi, val_lo);

	val_hi = BCE_ADDR_HI(sc->rx_bd_chain);
	val_lo = BCE_ADDR_LO(sc->rx_bd_chain);
	BCE_PRINTF(sc,
		"0x%08X:%08X - (sc->rx_bd_chain) rx_bd chain virtual address\n",
		val_hi, val_lo);

	val_hi = BCE_ADDR_HI(sc->tx_mbuf_ptr);
	val_lo = BCE_ADDR_LO(sc->tx_mbuf_ptr);
	BCE_PRINTF(sc,
		"0x%08X:%08X - (sc->tx_mbuf_ptr) tx mbuf chain virtual address\n",
		val_hi, val_lo);

	val_hi = BCE_ADDR_HI(sc->rx_mbuf_ptr);
	val_lo = BCE_ADDR_LO(sc->rx_mbuf_ptr);
	BCE_PRINTF(sc, 
		"0x%08X:%08X - (sc->rx_mbuf_ptr) rx mbuf chain virtual address\n",
		val_hi, val_lo);

	BCE_PRINTF(sc, "         0x%08X - (sc->interrupts_generated) h/w intrs\n",
		sc->interrupts_generated);
	
	BCE_PRINTF(sc, "         0x%08X - (sc->rx_interrupts) rx interrupts handled\n",
		sc->rx_interrupts);

	BCE_PRINTF(sc, "         0x%08X - (sc->tx_interrupts) tx interrupts handled\n",
		sc->tx_interrupts);

	BCE_PRINTF(sc, "         0x%08X - (sc->last_status_idx) status block index\n",
		sc->last_status_idx);

	BCE_PRINTF(sc, "         0x%08X - (sc->tx_prod) tx producer index\n",
		sc->tx_prod);

	BCE_PRINTF(sc, "         0x%08X - (sc->tx_cons) tx consumer index\n",
		sc->tx_cons);

	BCE_PRINTF(sc, "         0x%08X - (sc->tx_prod_bseq) tx producer bseq index\n",
		sc->tx_prod_bseq);

	BCE_PRINTF(sc, "         0x%08X - (sc->rx_prod) rx producer index\n",
		sc->rx_prod);

	BCE_PRINTF(sc, "         0x%08X - (sc->rx_cons) rx consumer index\n",
		sc->rx_cons);

	BCE_PRINTF(sc, "         0x%08X - (sc->rx_prod_bseq) rx producer bseq index\n",
		sc->rx_prod_bseq);

	BCE_PRINTF(sc, "         0x%08X - (sc->rx_mbuf_alloc) rx mbufs allocated\n",
		sc->rx_mbuf_alloc);

	BCE_PRINTF(sc, "         0x%08X - (sc->free_rx_bd) free rx_bd's\n",
		sc->free_rx_bd);

	BCE_PRINTF(sc, "0x%08X/%08X - (sc->rx_low_watermark) rx low watermark\n",
		sc->rx_low_watermark, (u32) USABLE_RX_BD);

	BCE_PRINTF(sc, "         0x%08X - (sc->txmbuf_alloc) tx mbufs allocated\n",
		sc->tx_mbuf_alloc);

	BCE_PRINTF(sc, "         0x%08X - (sc->rx_mbuf_alloc) rx mbufs allocated\n",
		sc->rx_mbuf_alloc);

	BCE_PRINTF(sc, "         0x%08X - (sc->used_tx_bd) used tx_bd's\n",
		sc->used_tx_bd);

	BCE_PRINTF(sc, "0x%08X/%08X - (sc->tx_hi_watermark) tx hi watermark\n",
		sc->tx_hi_watermark, (u32) USABLE_TX_BD);

	BCE_PRINTF(sc, "         0x%08X - (sc->mbuf_alloc_failed) failed mbuf alloc\n",
		sc->mbuf_alloc_failed);

	BCE_PRINTF(sc,
		"-----------------------------"
		"--------------"
		"-----------------------------\n");
}


static void
bce_dump_hw_state(struct bce_softc *sc)
{
	u32 val1;

	BCE_PRINTF(sc,
		"----------------------------"
		" Hardware State "
		"----------------------------\n");

	BCE_PRINTF(sc, "0x%08X : bootcode version\n", sc->bce_fw_ver);

	val1 = REG_RD(sc, BCE_MISC_ENABLE_STATUS_BITS);
	BCE_PRINTF(sc, "0x%08X : (0x%04X) misc_enable_status_bits\n",
		val1, BCE_MISC_ENABLE_STATUS_BITS);

	val1 = REG_RD(sc, BCE_DMA_STATUS);
	BCE_PRINTF(sc, "0x%08X : (0x%04X) dma_status\n", val1, BCE_DMA_STATUS);

	val1 = REG_RD(sc, BCE_CTX_STATUS);
	BCE_PRINTF(sc, "0x%08X : (0x%04X) ctx_status\n", val1, BCE_CTX_STATUS);

	val1 = REG_RD(sc, BCE_EMAC_STATUS);
	BCE_PRINTF(sc, "0x%08X : (0x%04X) emac_status\n", val1, BCE_EMAC_STATUS);

	val1 = REG_RD(sc, BCE_RPM_STATUS);
	BCE_PRINTF(sc, "0x%08X : (0x%04X) rpm_status\n", val1, BCE_RPM_STATUS);

	val1 = REG_RD(sc, BCE_TBDR_STATUS);
	BCE_PRINTF(sc, "0x%08X : (0x%04X) tbdr_status\n", val1, BCE_TBDR_STATUS);

	val1 = REG_RD(sc, BCE_TDMA_STATUS);
	BCE_PRINTF(sc, "0x%08X : (0x%04X) tdma_status\n", val1, BCE_TDMA_STATUS);

	val1 = REG_RD(sc, BCE_HC_STATUS);
	BCE_PRINTF(sc, "0x%08X : (0x%04X) hc_status\n", val1, BCE_HC_STATUS);

	BCE_PRINTF(sc, 
		"----------------------------"
		"----------------"
		"----------------------------\n");

	BCE_PRINTF(sc, 
		"----------------------------"
		" Register  Dump "
		"----------------------------\n");

	for (int i = 0x400; i < 0x8000; i += 0x10)
		BCE_PRINTF(sc, "0x%04X: 0x%08X 0x%08X 0x%08X 0x%08X\n",
			i, REG_RD(sc, i), REG_RD(sc, i + 0x4),
			REG_RD(sc, i + 0x8), REG_RD(sc, i + 0xC));

	BCE_PRINTF(sc, 
		"----------------------------"
		"----------------"
		"----------------------------\n");
}


static void
bce_breakpoint(struct bce_softc *sc)
{

	/* Unreachable code to shut the compiler up about unused functions. */
	if (0) {
   		bce_dump_txbd(sc, 0, NULL);
		bce_dump_rxbd(sc, 0, NULL);
		bce_dump_tx_mbuf_chain(sc, 0, USABLE_TX_BD);
		bce_dump_rx_mbuf_chain(sc, 0, USABLE_RX_BD);
		bce_dump_l2fhdr(sc, 0, NULL);
		bce_dump_tx_chain(sc, 0, USABLE_TX_BD);
		bce_dump_rx_chain(sc, 0, USABLE_RX_BD);
		bce_dump_status_block(sc);
		bce_dump_stats_block(sc);
		bce_dump_driver_state(sc);
		bce_dump_hw_state(sc);
	}

	bce_dump_driver_state(sc);
	/* Print the important status block fields. */
	bce_dump_status_block(sc);

	/* Call the debugger. */
	breakpoint();

	return;
}
#endif
