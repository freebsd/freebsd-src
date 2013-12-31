/*-
 * Copyright (c) 2006,2007
 *	Damien Bergamini <damien.bergamini@free.fr>
 *	Benjamin Close <Benjamin.Close@clearchain.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define VERSION "20071127"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Driver for Intel PRO/Wireless 3945ABG 802.11 network adapters.
 *
 * The 3945ABG network adapter doesn't use traditional hardware as
 * many other adaptors do. Instead at run time the eeprom is set into a known
 * state and told to load boot firmware. The boot firmware loads an init and a
 * main  binary firmware image into SRAM on the card via DMA.
 * Once the firmware is loaded, the driver/hw then
 * communicate by way of circular dma rings via the SRAM to the firmware.
 *
 * There is 6 memory rings. 1 command ring, 1 rx data ring & 4 tx data rings.
 * The 4 tx data rings allow for prioritization QoS.
 *
 * The rx data ring consists of 32 dma buffers. Two registers are used to
 * indicate where in the ring the driver and the firmware are up to. The
 * driver sets the initial read index (reg1) and the initial write index (reg2),
 * the firmware updates the read index (reg1) on rx of a packet and fires an
 * interrupt. The driver then processes the buffers starting at reg1 indicating
 * to the firmware which buffers have been accessed by updating reg2. At the
 * same time allocating new memory for the processed buffer.
 *
 * A similar thing happens with the tx rings. The difference is the firmware
 * stop processing buffers once the queue is full and until confirmation
 * of a successful transmition (tx_intr) has occurred.
 *
 * The command ring operates in the same manner as the tx queues.
 *
 * All communication direct to the card (ie eeprom) is classed as Stage1
 * communication
 *
 * All communication via the firmware to the card is classed as State2.
 * The firmware consists of 2 parts. A bootstrap firmware and a runtime
 * firmware. The bootstrap firmware and runtime firmware are loaded
 * from host memory via dma to the card then told to execute. From this point
 * on the majority of communications between the driver and the card goes
 * via the firmware.
 */

#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/linker.h>
#include <sys/firmware.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_ratectl.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <dev/wpi/if_wpireg.h>
#include <dev/wpi/if_wpivar.h>

#define WPI_DEBUG

#ifdef WPI_DEBUG
#define DPRINTF(x)	do { if (wpi_debug != 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (wpi_debug & n) printf x; } while (0)
#define	WPI_DEBUG_SET	(wpi_debug != 0)

enum {
	WPI_DEBUG_UNUSED	= 0x00000001,   /* Unused */
	WPI_DEBUG_HW		= 0x00000002,   /* Stage 1 (eeprom) debugging */
	WPI_DEBUG_TX		= 0x00000004,   /* Stage 2 TX intrp debugging*/
	WPI_DEBUG_RX		= 0x00000008,   /* Stage 2 RX intrp debugging */
	WPI_DEBUG_CMD		= 0x00000010,   /* Stage 2 CMD intrp debugging*/
	WPI_DEBUG_FIRMWARE	= 0x00000020,   /* firmware(9) loading debug  */
	WPI_DEBUG_DMA		= 0x00000040,   /* DMA (de)allocations/syncs  */
	WPI_DEBUG_SCANNING	= 0x00000080,   /* Stage 2 Scanning debugging */
	WPI_DEBUG_NOTIFY	= 0x00000100,   /* State 2 Noftif intr debug */
	WPI_DEBUG_TEMP		= 0x00000200,   /* TXPower/Temp Calibration */
	WPI_DEBUG_OPS		= 0x00000400,   /* wpi_ops taskq debug */
	WPI_DEBUG_WATCHDOG	= 0x00000800,   /* Watch dog debug */
	WPI_DEBUG_ANY		= 0xffffffff
};

static int wpi_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, wpi, CTLFLAG_RW, &wpi_debug, 0, "wpi debug level");
TUNABLE_INT("debug.wpi", &wpi_debug);

#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#define WPI_DEBUG_SET	0
#endif

struct wpi_ident {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subdevice;
	const char	*name;
};

static const struct wpi_ident wpi_ident_table[] = {
	/* The below entries support ABG regardless of the subid */
	{ 0x8086, 0x4222,    0x0, "Intel(R) PRO/Wireless 3945ABG" },
	{ 0x8086, 0x4227,    0x0, "Intel(R) PRO/Wireless 3945ABG" },
	/* The below entries only support BG */
	{ 0x8086, 0x4222, 0x1005, "Intel(R) PRO/Wireless 3945BG"  },
	{ 0x8086, 0x4222, 0x1034, "Intel(R) PRO/Wireless 3945BG"  },
	{ 0x8086, 0x4227, 0x1014, "Intel(R) PRO/Wireless 3945BG"  },
	{ 0x8086, 0x4222, 0x1044, "Intel(R) PRO/Wireless 3945BG"  },
	{ 0, 0, 0, NULL }
};

static struct ieee80211vap *wpi_vap_create(struct ieee80211com *,
		    const char [IFNAMSIZ], int, enum ieee80211_opmode, int,
		    const uint8_t [IEEE80211_ADDR_LEN],
		    const uint8_t [IEEE80211_ADDR_LEN]);
static void	wpi_vap_delete(struct ieee80211vap *);
static int	wpi_dma_contig_alloc(struct wpi_softc *, struct wpi_dma_info *,
		    void **, bus_size_t, bus_size_t, int);
static void	wpi_dma_contig_free(struct wpi_dma_info *);
static void	wpi_dma_map_addr(void *, bus_dma_segment_t *, int, int);
static int	wpi_alloc_shared(struct wpi_softc *);
static void	wpi_free_shared(struct wpi_softc *);
static int	wpi_alloc_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
static void	wpi_reset_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
static void	wpi_free_rx_ring(struct wpi_softc *, struct wpi_rx_ring *);
static int	wpi_alloc_tx_ring(struct wpi_softc *, struct wpi_tx_ring *,
		    int, int);
static void	wpi_reset_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
static void	wpi_free_tx_ring(struct wpi_softc *, struct wpi_tx_ring *);
static int	wpi_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static void	wpi_mem_lock(struct wpi_softc *);
static void	wpi_mem_unlock(struct wpi_softc *);
static uint32_t	wpi_mem_read(struct wpi_softc *, uint16_t);
static void	wpi_mem_write(struct wpi_softc *, uint16_t, uint32_t);
static void	wpi_mem_write_region_4(struct wpi_softc *, uint16_t,
		    const uint32_t *, int);
static uint16_t	wpi_read_prom_data(struct wpi_softc *, uint32_t, void *, int);
static int	wpi_alloc_fwmem(struct wpi_softc *);
static void	wpi_free_fwmem(struct wpi_softc *);
static int	wpi_load_firmware(struct wpi_softc *);
static void	wpi_unload_firmware(struct wpi_softc *);
static int	wpi_load_microcode(struct wpi_softc *, const uint8_t *, int);
static void	wpi_rx_intr(struct wpi_softc *, struct wpi_rx_desc *,
		    struct wpi_rx_data *);
static void	wpi_tx_intr(struct wpi_softc *, struct wpi_rx_desc *);
static void	wpi_cmd_intr(struct wpi_softc *, struct wpi_rx_desc *);
static void	wpi_notif_intr(struct wpi_softc *);
static void	wpi_intr(void *);
static uint8_t	wpi_plcp_signal(int);
static void	wpi_watchdog(void *);
static int	wpi_tx_data(struct wpi_softc *, struct mbuf *,
		    struct ieee80211_node *, int);
static void	wpi_start(struct ifnet *);
static void	wpi_start_locked(struct ifnet *);
static int	wpi_raw_xmit(struct ieee80211_node *, struct mbuf *,
		    const struct ieee80211_bpf_params *);
static void	wpi_scan_start(struct ieee80211com *);
static void	wpi_scan_end(struct ieee80211com *);
static void	wpi_set_channel(struct ieee80211com *);
static void	wpi_scan_curchan(struct ieee80211_scan_state *, unsigned long);
static void	wpi_scan_mindwell(struct ieee80211_scan_state *);
static int	wpi_ioctl(struct ifnet *, u_long, caddr_t);
static void	wpi_read_eeprom(struct wpi_softc *,
		    uint8_t macaddr[IEEE80211_ADDR_LEN]);
static void	wpi_read_eeprom_channels(struct wpi_softc *, int);
static void	wpi_read_eeprom_group(struct wpi_softc *, int);
static int	wpi_cmd(struct wpi_softc *, int, const void *, int, int);
static int	wpi_wme_update(struct ieee80211com *);
static int	wpi_mrr_setup(struct wpi_softc *);
static void	wpi_set_led(struct wpi_softc *, uint8_t, uint8_t, uint8_t);
static void	wpi_enable_tsf(struct wpi_softc *, struct ieee80211_node *);
#if 0
static int	wpi_setup_beacon(struct wpi_softc *, struct ieee80211_node *);
#endif
static int	wpi_auth(struct wpi_softc *, struct ieee80211vap *);
static int	wpi_run(struct wpi_softc *, struct ieee80211vap *);
static int	wpi_scan(struct wpi_softc *);
static int	wpi_config(struct wpi_softc *);
static void	wpi_stop_master(struct wpi_softc *);
static int	wpi_power_up(struct wpi_softc *);
static int	wpi_reset(struct wpi_softc *);
static void	wpi_hwreset(void *, int);
static void	wpi_rfreset(void *, int);
static void	wpi_hw_config(struct wpi_softc *);
static void	wpi_init(void *);
static void	wpi_init_locked(struct wpi_softc *, int);
static void	wpi_stop(struct wpi_softc *);
static void	wpi_stop_locked(struct wpi_softc *);

static int	wpi_set_txpower(struct wpi_softc *, struct ieee80211_channel *,
		    int);
static void	wpi_calib_timeout(void *);
static void	wpi_power_calibration(struct wpi_softc *, int);
static int	wpi_get_power_index(struct wpi_softc *,
		    struct wpi_power_group *, struct ieee80211_channel *, int);
#ifdef WPI_DEBUG
static const char *wpi_cmd_str(int);
#endif
static int wpi_probe(device_t);
static int wpi_attach(device_t);
static int wpi_detach(device_t);
static int wpi_shutdown(device_t);
static int wpi_suspend(device_t);
static int wpi_resume(device_t);

static device_method_t wpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		wpi_probe),
	DEVMETHOD(device_attach,	wpi_attach),
	DEVMETHOD(device_detach,	wpi_detach),
	DEVMETHOD(device_shutdown,	wpi_shutdown),
	DEVMETHOD(device_suspend,	wpi_suspend),
	DEVMETHOD(device_resume,	wpi_resume),

	DEVMETHOD_END
};

static driver_t wpi_driver = {
	"wpi",
	wpi_methods,
	sizeof (struct wpi_softc)
};

static devclass_t wpi_devclass;

DRIVER_MODULE(wpi, pci, wpi_driver, wpi_devclass, NULL, NULL);

MODULE_VERSION(wpi, 1);

static const uint8_t wpi_ridx_to_plcp[] = {
	/* OFDM: IEEE Std 802.11a-1999, pp. 14 Table 80 */
	/* R1-R4 (ral/ural is R4-R1) */
	0xd, 0xf, 0x5, 0x7, 0x9, 0xb, 0x1, 0x3,
	/* CCK: device-dependent */
	10, 20, 55, 110
};

static const uint8_t wpi_ridx_to_rate[] = {
	12, 18, 24, 36, 48, 72, 96, 108, /* OFDM */
	2, 4, 11, 22 /*CCK */
};

static int
wpi_probe(device_t dev)
{
	const struct wpi_ident *ident;

	for (ident = wpi_ident_table; ident->name != NULL; ident++) {
		if (pci_get_vendor(dev) == ident->vendor &&
		    pci_get_device(dev) == ident->device) {
			device_set_desc(dev, ident->name);
			return (BUS_PROBE_DEFAULT);
		}
	}
	return ENXIO;
}

/**
 * Load the firmare image from disk to the allocated dma buffer.
 * we also maintain the reference to the firmware pointer as there
 * is times where we may need to reload the firmware but we are not
 * in a context that can access the filesystem (ie taskq cause by restart)
 *
 * @return 0 on success, an errno on failure
 */
static int
wpi_load_firmware(struct wpi_softc *sc)
{
	const struct firmware *fp;
	struct wpi_dma_info *dma = &sc->fw_dma;
	const struct wpi_firmware_hdr *hdr;
	const uint8_t *itext, *idata, *rtext, *rdata, *btext;
	uint32_t itextsz, idatasz, rtextsz, rdatasz, btextsz;
	int error;

	DPRINTFN(WPI_DEBUG_FIRMWARE,
	    ("Attempting Loading Firmware from wpi_fw module\n"));

	WPI_UNLOCK(sc);

	if (sc->fw_fp == NULL && (sc->fw_fp = firmware_get("wpifw")) == NULL) {
		device_printf(sc->sc_dev,
		    "could not load firmware image 'wpifw'\n");
		error = ENOENT;
		WPI_LOCK(sc);
		goto fail;
	}

	fp = sc->fw_fp;

	WPI_LOCK(sc);

	/* Validate the firmware is minimum a particular version */
	if (fp->version < WPI_FW_MINVERSION) {
	    device_printf(sc->sc_dev,
			   "firmware version is too old. Need %d, got %d\n",
			   WPI_FW_MINVERSION,
			   fp->version);
	    error = ENXIO;
	    goto fail;
	}

	if (fp->datasize < sizeof (struct wpi_firmware_hdr)) {
		device_printf(sc->sc_dev,
		    "firmware file too short: %zu bytes\n", fp->datasize);
		error = ENXIO;
		goto fail;
	}

	hdr = (const struct wpi_firmware_hdr *)fp->data;

	/*     |  RUNTIME FIRMWARE   |    INIT FIRMWARE    | BOOT FW  |
	   |HDR|<--TEXT-->|<--DATA-->|<--TEXT-->|<--DATA-->|<--TEXT-->| */

	rtextsz = le32toh(hdr->rtextsz);
	rdatasz = le32toh(hdr->rdatasz);
	itextsz = le32toh(hdr->itextsz);
	idatasz = le32toh(hdr->idatasz);
	btextsz = le32toh(hdr->btextsz);

	/* check that all firmware segments are present */
	if (fp->datasize < sizeof (struct wpi_firmware_hdr) +
		rtextsz + rdatasz + itextsz + idatasz + btextsz) {
		device_printf(sc->sc_dev,
		    "firmware file too short: %zu bytes\n", fp->datasize);
		error = ENXIO; /* XXX appropriate error code? */
		goto fail;
	}

	/* get pointers to firmware segments */
	rtext = (const uint8_t *)(hdr + 1);
	rdata = rtext + rtextsz;
	itext = rdata + rdatasz;
	idata = itext + itextsz;
	btext = idata + idatasz;

	DPRINTFN(WPI_DEBUG_FIRMWARE,
	    ("Firmware Version: Major %d, Minor %d, Driver %d, \n"
	     "runtime (text: %u, data: %u) init (text: %u, data %u) boot (text %u)\n",
	     (le32toh(hdr->version) & 0xff000000) >> 24,
	     (le32toh(hdr->version) & 0x00ff0000) >> 16,
	     (le32toh(hdr->version) & 0x0000ffff),
	     rtextsz, rdatasz,
	     itextsz, idatasz, btextsz));

	DPRINTFN(WPI_DEBUG_FIRMWARE,("rtext 0x%x\n", *(const uint32_t *)rtext));
	DPRINTFN(WPI_DEBUG_FIRMWARE,("rdata 0x%x\n", *(const uint32_t *)rdata));
	DPRINTFN(WPI_DEBUG_FIRMWARE,("itext 0x%x\n", *(const uint32_t *)itext));
	DPRINTFN(WPI_DEBUG_FIRMWARE,("idata 0x%x\n", *(const uint32_t *)idata));
	DPRINTFN(WPI_DEBUG_FIRMWARE,("btext 0x%x\n", *(const uint32_t *)btext));

	/* sanity checks */
	if (rtextsz > WPI_FW_MAIN_TEXT_MAXSZ ||
	    rdatasz > WPI_FW_MAIN_DATA_MAXSZ ||
	    itextsz > WPI_FW_INIT_TEXT_MAXSZ ||
	    idatasz > WPI_FW_INIT_DATA_MAXSZ ||
	    btextsz > WPI_FW_BOOT_TEXT_MAXSZ ||
	    (btextsz & 3) != 0) {
		device_printf(sc->sc_dev, "firmware invalid\n");
		error = EINVAL;
		goto fail;
	}

	/* copy initialization images into pre-allocated DMA-safe memory */
	memcpy(dma->vaddr, idata, idatasz);
	memcpy(dma->vaddr + WPI_FW_INIT_DATA_MAXSZ, itext, itextsz);

	bus_dmamap_sync(dma->tag, dma->map, BUS_DMASYNC_PREWRITE);

	/* tell adapter where to find initialization images */
	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_DATA_BASE, dma->paddr);
	wpi_mem_write(sc, WPI_MEM_DATA_SIZE, idatasz);
	wpi_mem_write(sc, WPI_MEM_TEXT_BASE,
	    dma->paddr + WPI_FW_INIT_DATA_MAXSZ);
	wpi_mem_write(sc, WPI_MEM_TEXT_SIZE, itextsz);
	wpi_mem_unlock(sc);

	/* load firmware boot code */
	if ((error = wpi_load_microcode(sc, btext, btextsz)) != 0) {
	    device_printf(sc->sc_dev, "Failed to load microcode\n");
	    goto fail;
	}

	/* now press "execute" */
	WPI_WRITE(sc, WPI_RESET, 0);

	/* wait at most one second for the first alive notification */
	if ((error = msleep(sc, &sc->sc_mtx, PCATCH, "wpiinit", hz)) != 0) {
		device_printf(sc->sc_dev,
		    "timeout waiting for adapter to initialize\n");
		goto fail;
	}

	/* copy runtime images into pre-allocated DMA-sage memory */
	memcpy(dma->vaddr, rdata, rdatasz);
	memcpy(dma->vaddr + WPI_FW_MAIN_DATA_MAXSZ, rtext, rtextsz);
	bus_dmamap_sync(dma->tag, dma->map, BUS_DMASYNC_PREWRITE);

	/* tell adapter where to find runtime images */
	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_DATA_BASE, dma->paddr);
	wpi_mem_write(sc, WPI_MEM_DATA_SIZE, rdatasz);
	wpi_mem_write(sc, WPI_MEM_TEXT_BASE,
	    dma->paddr + WPI_FW_MAIN_DATA_MAXSZ);
	wpi_mem_write(sc, WPI_MEM_TEXT_SIZE, WPI_FW_UPDATED | rtextsz);
	wpi_mem_unlock(sc);

	/* wait at most one second for the first alive notification */
	if ((error = msleep(sc, &sc->sc_mtx, PCATCH, "wpiinit", hz)) != 0) {
		device_printf(sc->sc_dev,
		    "timeout waiting for adapter to initialize2\n");
		goto fail;
	}

	DPRINTFN(WPI_DEBUG_FIRMWARE,
	    ("Firmware loaded to driver successfully\n"));
	return error;
fail:
	wpi_unload_firmware(sc);
	return error;
}

/**
 * Free the referenced firmware image
 */
static void
wpi_unload_firmware(struct wpi_softc *sc)
{

	if (sc->fw_fp) {
		WPI_UNLOCK(sc);
		firmware_put(sc->fw_fp, FIRMWARE_UNLOAD);
		WPI_LOCK(sc);
		sc->fw_fp = NULL;
	}
}

static int
wpi_attach(device_t dev)
{
	struct wpi_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	struct ieee80211com *ic;
	int ac, error, rid, supportsa = 1;
	uint32_t tmp;
	const struct wpi_ident *ident;
	uint8_t macaddr[IEEE80211_ADDR_LEN];

	sc->sc_dev = dev;

	if (bootverbose || WPI_DEBUG_SET)
	    device_printf(sc->sc_dev,"Driver Revision %s\n", VERSION);

	/*
	 * Some card's only support 802.11b/g not a, check to see if
	 * this is one such card. A 0x0 in the subdevice table indicates
	 * the entire subdevice range is to be ignored.
	 */
	for (ident = wpi_ident_table; ident->name != NULL; ident++) {
		if (ident->subdevice &&
		    pci_get_subdevice(dev) == ident->subdevice) {
		    supportsa = 0;
		    break;
		}
	}

	/* Create the tasks that can be queued */
	TASK_INIT(&sc->sc_restarttask, 0, wpi_hwreset, sc);
	TASK_INIT(&sc->sc_radiotask, 0, wpi_rfreset, sc);

	WPI_LOCK_INIT(sc);

	callout_init_mtx(&sc->calib_to, &sc->sc_mtx, 0);
	callout_init_mtx(&sc->watchdog_to, &sc->sc_mtx, 0);

	/* disable the retry timeout register */
	pci_write_config(dev, 0x41, 0, 1);

	/* enable bus-mastering */
	pci_enable_busmaster(dev);

	rid = PCIR_BAR(0);
	sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		error = ENOMEM;
		goto fail;
	}

	sc->sc_st = rman_get_bustag(sc->mem);
	sc->sc_sh = rman_get_bushandle(sc->mem);

	rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq == NULL) {
		device_printf(dev, "could not allocate interrupt resource\n");
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Allocate DMA memory for firmware transfers.
	 */
	if ((error = wpi_alloc_fwmem(sc)) != 0) {
		printf(": could not allocate firmware memory\n");
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Put adapter into a known state.
	 */
	if ((error = wpi_reset(sc)) != 0) {
		device_printf(dev, "could not reset adapter\n");
		goto fail;
	}

	wpi_mem_lock(sc);
	tmp = wpi_mem_read(sc, WPI_MEM_PCIDEV);
	if (bootverbose || WPI_DEBUG_SET)
	    device_printf(sc->sc_dev, "Hardware Revision (0x%X)\n", tmp);

	wpi_mem_unlock(sc);

	/* Allocate shared page */
	if ((error = wpi_alloc_shared(sc)) != 0) {
		device_printf(dev, "could not allocate shared page\n");
		goto fail;
	}

	/* tx data queues  - 4 for QoS purposes */
	for (ac = 0; ac < WME_NUM_AC; ac++) {
		error = wpi_alloc_tx_ring(sc, &sc->txq[ac], WPI_TX_RING_COUNT, ac);
		if (error != 0) {
		    device_printf(dev, "could not allocate Tx ring %d\n",ac);
		    goto fail;
		}
	}

	/* command queue to talk to the card's firmware */
	error = wpi_alloc_tx_ring(sc, &sc->cmdq, WPI_CMD_RING_COUNT, 4);
	if (error != 0) {
		device_printf(dev, "could not allocate command ring\n");
		goto fail;
	}

	/* receive data queue */
	error = wpi_alloc_rx_ring(sc, &sc->rxq);
	if (error != 0) {
		device_printf(dev, "could not allocate Rx ring\n");
		goto fail;
	}

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		error = ENOMEM;
		goto fail;
	}
	ic = ifp->if_l2com;

	ic->ic_ifp = ifp;
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */

	/* set device capabilities */
	ic->ic_caps =
		  IEEE80211_C_STA		/* station mode supported */
		| IEEE80211_C_MONITOR		/* monitor mode supported */
		| IEEE80211_C_TXPMGT		/* tx power management */
		| IEEE80211_C_SHSLOT		/* short slot time supported */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_WPA		/* 802.11i */
/* XXX looks like WME is partly supported? */
#if 0
		| IEEE80211_C_IBSS		/* IBSS mode support */
		| IEEE80211_C_BGSCAN		/* capable of bg scanning */
		| IEEE80211_C_WME		/* 802.11e */
		| IEEE80211_C_HOSTAP		/* Host access point mode */
#endif
		;

	/*
	 * Read in the eeprom and also setup the channels for
	 * net80211. We don't set the rates as net80211 does this for us
	 */
	wpi_read_eeprom(sc, macaddr);

	if (bootverbose || WPI_DEBUG_SET) {
	    device_printf(sc->sc_dev, "Regulatory Domain: %.4s\n", sc->domain);
	    device_printf(sc->sc_dev, "Hardware Type: %c\n",
			  sc->type > 1 ? 'B': '?');
	    device_printf(sc->sc_dev, "Hardware Revision: %c\n",
			  ((le16toh(sc->rev) & 0xf0) == 0xd0) ? 'D': '?');
	    device_printf(sc->sc_dev, "SKU %s support 802.11a\n",
			  supportsa ? "does" : "does not");

	    /* XXX hw_config uses the PCIDEV for the Hardware rev. Must check
	       what sc->rev really represents - benjsc 20070615 */
	}

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = wpi_init;
	ifp->if_ioctl = wpi_ioctl;
	ifp->if_start = wpi_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	ieee80211_ifattach(ic, macaddr);
	/* override default methods */
	ic->ic_raw_xmit = wpi_raw_xmit;
	ic->ic_wme.wme_update = wpi_wme_update;
	ic->ic_scan_start = wpi_scan_start;
	ic->ic_scan_end = wpi_scan_end;
	ic->ic_set_channel = wpi_set_channel;
	ic->ic_scan_curchan = wpi_scan_curchan;
	ic->ic_scan_mindwell = wpi_scan_mindwell;

	ic->ic_vap_create = wpi_vap_create;
	ic->ic_vap_delete = wpi_vap_delete;

	ieee80211_radiotap_attach(ic,
	    &sc->sc_txtap.wt_ihdr, sizeof(sc->sc_txtap),
		WPI_TX_RADIOTAP_PRESENT,
	    &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
		WPI_RX_RADIOTAP_PRESENT);

	/*
	 * Hook our interrupt after all initialization is complete.
	 */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET |INTR_MPSAFE,
	    NULL, wpi_intr, sc, &sc->sc_ih);
	if (error != 0) {
		device_printf(dev, "could not set up interrupt\n");
		goto fail;
	}

	if (bootverbose)
		ieee80211_announce(ic);
#ifdef XXX_DEBUG
	ieee80211_announce_channels(ic);
#endif
	return 0;

fail:	wpi_detach(dev);
	return ENXIO;
}

static int
wpi_detach(device_t dev)
{
	struct wpi_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic;
	int ac;

	if (sc->irq != NULL)
		bus_teardown_intr(dev, sc->irq, sc->sc_ih);

	if (ifp != NULL) {
		ic = ifp->if_l2com;

		ieee80211_draintask(ic, &sc->sc_restarttask);
		ieee80211_draintask(ic, &sc->sc_radiotask);
		wpi_stop(sc);
		callout_drain(&sc->watchdog_to);
		callout_drain(&sc->calib_to);
		ieee80211_ifdetach(ic);
	}

	WPI_LOCK(sc);
	if (sc->txq[0].data_dmat) {
		for (ac = 0; ac < WME_NUM_AC; ac++)
			wpi_free_tx_ring(sc, &sc->txq[ac]);

		wpi_free_tx_ring(sc, &sc->cmdq);
		wpi_free_rx_ring(sc, &sc->rxq);
		wpi_free_shared(sc);
	}

	if (sc->fw_fp != NULL) {
		wpi_unload_firmware(sc);
	}

	if (sc->fw_dma.tag)
		wpi_free_fwmem(sc);
	WPI_UNLOCK(sc);

	if (sc->irq != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, rman_get_rid(sc->irq),
		    sc->irq);
	if (sc->mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem), sc->mem);

	if (ifp != NULL)
		if_free(ifp);

	WPI_LOCK_DESTROY(sc);

	return 0;
}

static struct ieee80211vap *
wpi_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit,
    enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct wpi_vap *wvp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))		/* only one at a time */
		return NULL;
	wvp = (struct wpi_vap *) malloc(sizeof(struct wpi_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (wvp == NULL)
		return NULL;
	vap = &wvp->vap;
	ieee80211_vap_setup(ic, vap, name, unit, opmode, flags, bssid, mac);
	/* override with driver methods */
	wvp->newstate = vap->iv_newstate;
	vap->iv_newstate = wpi_newstate;

	ieee80211_ratectl_init(vap);
	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change, ieee80211_media_status);
	ic->ic_opmode = opmode;
	return vap;
}

static void
wpi_vap_delete(struct ieee80211vap *vap)
{
	struct wpi_vap *wvp = WPI_VAP(vap);

	ieee80211_ratectl_deinit(vap);
	ieee80211_vap_detach(vap);
	free(wvp, M_80211_VAP);
}

static void
wpi_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error != 0)
		return;

	KASSERT(nsegs == 1, ("too many DMA segments, %d should be 1", nsegs));

	*(bus_addr_t *)arg = segs[0].ds_addr;
}

/*
 * Allocates a contiguous block of dma memory of the requested size and
 * alignment. Due to limitations of the FreeBSD dma subsystem as of 20071217,
 * allocations greater than 4096 may fail. Hence if the requested alignment is
 * greater we allocate 'alignment' size extra memory and shift the vaddr and
 * paddr after the dma load. This bypasses the problem at the cost of a little
 * more memory.
 */
static int
wpi_dma_contig_alloc(struct wpi_softc *sc, struct wpi_dma_info *dma,
    void **kvap, bus_size_t size, bus_size_t alignment, int flags)
{
	int error;
	bus_size_t align;
	bus_size_t reqsize;

	DPRINTFN(WPI_DEBUG_DMA,
	    ("Size: %zd - alignment %zd\n", size, alignment));

	dma->size = size;
	dma->tag = NULL;

	if (alignment > 4096) {
		align = PAGE_SIZE;
		reqsize = size + alignment;
	} else {
		align = alignment;
		reqsize = size;
	}
	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), align,
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, reqsize,
	    1, reqsize, flags,
	    NULL, NULL, &dma->tag);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not create shared page DMA tag\n");
		goto fail;
	}
	error = bus_dmamem_alloc(dma->tag, (void **)&dma->vaddr_start,
	    flags | BUS_DMA_ZERO, &dma->map);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate shared page DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(dma->tag, dma->map, dma->vaddr_start,
	    reqsize,  wpi_dma_map_addr, &dma->paddr_start, flags);

	/* Save the original pointers so we can free all the memory */
	dma->paddr = dma->paddr_start;
	dma->vaddr = dma->vaddr_start;

	/*
	 * Check the alignment and increment by 4096 until we get the
	 * requested alignment. Fail if can't obtain the alignment
	 * we requested.
	 */
	if ((dma->paddr & (alignment -1 )) != 0) {
		int i;

		for (i = 0; i < alignment / 4096; i++) {
			if ((dma->paddr & (alignment - 1 )) == 0)
				break;
			dma->paddr += 4096;
			dma->vaddr += 4096;
		}
		if (i == alignment / 4096) {
			device_printf(sc->sc_dev,
			    "alignment requirement was not satisfied\n");
			goto fail;
		}
	}

	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not load shared page DMA map\n");
		goto fail;
	}

	if (kvap != NULL)
		*kvap = dma->vaddr;

	return 0;

fail:
	wpi_dma_contig_free(dma);
	return error;
}

static void
wpi_dma_contig_free(struct wpi_dma_info *dma)
{
	if (dma->tag) {
		if (dma->map != NULL) {
			if (dma->paddr_start != 0) {
				bus_dmamap_sync(dma->tag, dma->map,
				    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(dma->tag, dma->map);
			}
			bus_dmamem_free(dma->tag, &dma->vaddr_start, dma->map);
		}
		bus_dma_tag_destroy(dma->tag);
	}
}

/*
 * Allocate a shared page between host and NIC.
 */
static int
wpi_alloc_shared(struct wpi_softc *sc)
{
	int error;

	error = wpi_dma_contig_alloc(sc, &sc->shared_dma,
	    (void **)&sc->shared, sizeof (struct wpi_shared),
	    PAGE_SIZE,
	    BUS_DMA_NOWAIT);

	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate shared area DMA memory\n");
	}

	return error;
}

static void
wpi_free_shared(struct wpi_softc *sc)
{
	wpi_dma_contig_free(&sc->shared_dma);
}

static int
wpi_alloc_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{

	int i, error;

	ring->cur = 0;

	error = wpi_dma_contig_alloc(sc, &ring->desc_dma,
	    (void **)&ring->desc, WPI_RX_RING_COUNT * sizeof (uint32_t),
	    WPI_RING_DMA_ALIGN, BUS_DMA_NOWAIT);

	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not allocate rx ring DMA memory, error %d\n",
		    __func__, error);
		goto fail;
	}

        error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0, 
	    BUS_SPACE_MAXADDR_32BIT,
            BUS_SPACE_MAXADDR, NULL, NULL, MJUMPAGESIZE, 1,
            MJUMPAGESIZE, BUS_DMA_NOWAIT, NULL, NULL, &ring->data_dmat);
        if (error != 0) {
                device_printf(sc->sc_dev,
		    "%s: bus_dma_tag_create_failed, error %d\n",
		    __func__, error);
                goto fail;
        }

	/*
	 * Setup Rx buffers.
	 */
	for (i = 0; i < WPI_RX_RING_COUNT; i++) {
		struct wpi_rx_data *data = &ring->data[i];
		struct mbuf *m;
		bus_addr_t paddr;

		error = bus_dmamap_create(ring->data_dmat, 0, &data->map);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "%s: bus_dmamap_create failed, error %d\n",
			    __func__, error);
			goto fail;
		}
		m = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, MJUMPAGESIZE);
		if (m == NULL) {
			device_printf(sc->sc_dev,
			   "%s: could not allocate rx mbuf\n", __func__);
			error = ENOMEM;
			goto fail;
		}
		/* map page */
		error = bus_dmamap_load(ring->data_dmat, data->map,
		    mtod(m, caddr_t), MJUMPAGESIZE,
		    wpi_dma_map_addr, &paddr, BUS_DMA_NOWAIT);
		if (error != 0 && error != EFBIG) {
			device_printf(sc->sc_dev,
			    "%s: bus_dmamap_load failed, error %d\n",
			    __func__, error);
			m_freem(m);
			error = ENOMEM;	/* XXX unique code */
			goto fail;
		}
		bus_dmamap_sync(ring->data_dmat, data->map, 
		    BUS_DMASYNC_PREWRITE);

		data->m = m;
		ring->desc[i] = htole32(paddr);
	}
	bus_dmamap_sync(ring->desc_dma.tag, ring->desc_dma.map,
	    BUS_DMASYNC_PREWRITE);
	return 0;
fail:
	wpi_free_rx_ring(sc, ring);
	return error;
}

static void
wpi_reset_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{
	int ntries;

	wpi_mem_lock(sc);

	WPI_WRITE(sc, WPI_RX_CONFIG, 0);

	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_RX_STATUS) & WPI_RX_IDLE)
			break;
		DELAY(10);
	}

	wpi_mem_unlock(sc);

#ifdef WPI_DEBUG
	if (ntries == 100 && wpi_debug > 0)
		device_printf(sc->sc_dev, "timeout resetting Rx ring\n");
#endif

	ring->cur = 0;
}

static void
wpi_free_rx_ring(struct wpi_softc *sc, struct wpi_rx_ring *ring)
{
	int i;

	wpi_dma_contig_free(&ring->desc_dma);

	for (i = 0; i < WPI_RX_RING_COUNT; i++) {
		struct wpi_rx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->data_dmat, data->map,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(ring->data_dmat, data->map);
			m_freem(data->m);
		}
		if (data->map != NULL)
			bus_dmamap_destroy(ring->data_dmat, data->map);
	}
}

static int
wpi_alloc_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring, int count,
	int qid)
{
	struct wpi_tx_data *data;
	int i, error;

	ring->qid = qid;
	ring->count = count;
	ring->queued = 0;
	ring->cur = 0;
	ring->data = NULL;

	error = wpi_dma_contig_alloc(sc, &ring->desc_dma,
		(void **)&ring->desc, count * sizeof (struct wpi_tx_desc),
		WPI_RING_DMA_ALIGN, BUS_DMA_NOWAIT);

	if (error != 0) {
	    device_printf(sc->sc_dev, "could not allocate tx dma memory\n");
	    goto fail;
	}

	/* update shared page with ring's base address */
	sc->shared->txbase[qid] = htole32(ring->desc_dma.paddr);

	error = wpi_dma_contig_alloc(sc, &ring->cmd_dma, (void **)&ring->cmd,
		count * sizeof (struct wpi_tx_cmd), WPI_RING_DMA_ALIGN,
		BUS_DMA_NOWAIT);

	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate tx command DMA memory\n");
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct wpi_tx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		device_printf(sc->sc_dev,
		    "could not allocate tx data slots\n");
		goto fail;
	}

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	    WPI_MAX_SCATTER - 1, MCLBYTES, BUS_DMA_NOWAIT, NULL, NULL,
	    &ring->data_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create data DMA tag\n");
		goto fail;
	}

	for (i = 0; i < count; i++) {
		data = &ring->data[i];

		error = bus_dmamap_create(ring->data_dmat, 0, &data->map);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not create tx buf DMA map\n");
			goto fail;
		}
		bus_dmamap_sync(ring->data_dmat, data->map,
		    BUS_DMASYNC_PREWRITE);
	}

	return 0;

fail:
	wpi_free_tx_ring(sc, ring);
	return error;
}

static void
wpi_reset_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring)
{
	struct wpi_tx_data *data;
	int i, ntries;

	wpi_mem_lock(sc);

	WPI_WRITE(sc, WPI_TX_CONFIG(ring->qid), 0);
	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_TX_STATUS) & WPI_TX_IDLE(ring->qid))
			break;
		DELAY(10);
	}
#ifdef WPI_DEBUG
	if (ntries == 100 && wpi_debug > 0)
		device_printf(sc->sc_dev, "timeout resetting Tx ring %d\n",
		    ring->qid);
#endif
	wpi_mem_unlock(sc);

	for (i = 0; i < ring->count; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_unload(ring->data_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
	}

	ring->queued = 0;
	ring->cur = 0;
}

static void
wpi_free_tx_ring(struct wpi_softc *sc, struct wpi_tx_ring *ring)
{
	struct wpi_tx_data *data;
	int i;

	wpi_dma_contig_free(&ring->desc_dma);
	wpi_dma_contig_free(&ring->cmd_dma);

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(ring->data_dmat, data->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(ring->data_dmat, data->map);
				m_freem(data->m);
				data->m = NULL;
			}
		}
		free(ring->data, M_DEVBUF);
	}

	if (ring->data_dmat != NULL)
		bus_dma_tag_destroy(ring->data_dmat);
}

static int
wpi_shutdown(device_t dev)
{
	struct wpi_softc *sc = device_get_softc(dev);

	WPI_LOCK(sc);
	wpi_stop_locked(sc);
	wpi_unload_firmware(sc);
	WPI_UNLOCK(sc);

	return 0;
}

static int
wpi_suspend(device_t dev)
{
	struct wpi_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;

	ieee80211_suspend_all(ic);
	return 0;
}

static int
wpi_resume(device_t dev)
{
	struct wpi_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;

	pci_write_config(dev, 0x41, 0, 1);

	ieee80211_resume_all(ic);
	return 0;
}

/**
 * Called by net80211 when ever there is a change to 80211 state machine
 */
static int
wpi_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct wpi_vap *wvp = WPI_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct wpi_softc *sc = ifp->if_softc;
	int error;

	DPRINTF(("%s: %s -> %s flags 0x%x\n", __func__,
		ieee80211_state_name[vap->iv_state],
		ieee80211_state_name[nstate], sc->flags));

	IEEE80211_UNLOCK(ic);
	WPI_LOCK(sc);
	if (nstate == IEEE80211_S_SCAN && vap->iv_state != IEEE80211_S_INIT) {
		/*
		 * On !INIT -> SCAN transitions, we need to clear any possible
		 * knowledge about associations.
		 */
		error = wpi_config(sc);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "%s: device config failed, error %d\n",
			    __func__, error);
		}
	}
	if (nstate == IEEE80211_S_AUTH ||
	    (nstate == IEEE80211_S_ASSOC && vap->iv_state == IEEE80211_S_RUN)) {
		/*
		 * The node must be registered in the firmware before auth.
		 * Also the associd must be cleared on RUN -> ASSOC
		 * transitions.
		 */
		error = wpi_auth(sc, vap);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "%s: could not move to auth state, error %d\n",
			    __func__, error);
		}
	}
	if (nstate == IEEE80211_S_RUN && vap->iv_state != IEEE80211_S_RUN) {
		error = wpi_run(sc, vap);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "%s: could not move to run state, error %d\n",
			    __func__, error);
		}
	}
	if (nstate == IEEE80211_S_RUN) {
		/* RUN -> RUN transition; just restart the timers */
		wpi_calib_timeout(sc);
		/* XXX split out rate control timer */
	}
	WPI_UNLOCK(sc);
	IEEE80211_LOCK(ic);
	return wvp->newstate(vap, nstate, arg);
}

/*
 * Grab exclusive access to NIC memory.
 */
static void
wpi_mem_lock(struct wpi_softc *sc)
{
	int ntries;
	uint32_t tmp;

	tmp = WPI_READ(sc, WPI_GPIO_CTL);
	WPI_WRITE(sc, WPI_GPIO_CTL, tmp | WPI_GPIO_MAC);

	/* spin until we actually get the lock */
	for (ntries = 0; ntries < 100; ntries++) {
		if ((WPI_READ(sc, WPI_GPIO_CTL) &
			(WPI_GPIO_CLOCK | WPI_GPIO_SLEEP)) == WPI_GPIO_CLOCK)
			break;
		DELAY(10);
	}
	if (ntries == 100)
		device_printf(sc->sc_dev, "could not lock memory\n");
}

/*
 * Release lock on NIC memory.
 */
static void
wpi_mem_unlock(struct wpi_softc *sc)
{
	uint32_t tmp = WPI_READ(sc, WPI_GPIO_CTL);
	WPI_WRITE(sc, WPI_GPIO_CTL, tmp & ~WPI_GPIO_MAC);
}

static uint32_t
wpi_mem_read(struct wpi_softc *sc, uint16_t addr)
{
	WPI_WRITE(sc, WPI_READ_MEM_ADDR, WPI_MEM_4 | addr);
	return WPI_READ(sc, WPI_READ_MEM_DATA);
}

static void
wpi_mem_write(struct wpi_softc *sc, uint16_t addr, uint32_t data)
{
	WPI_WRITE(sc, WPI_WRITE_MEM_ADDR, WPI_MEM_4 | addr);
	WPI_WRITE(sc, WPI_WRITE_MEM_DATA, data);
}

static void
wpi_mem_write_region_4(struct wpi_softc *sc, uint16_t addr,
    const uint32_t *data, int wlen)
{
	for (; wlen > 0; wlen--, data++, addr+=4)
		wpi_mem_write(sc, addr, *data);
}

/*
 * Read data from the EEPROM.  We access EEPROM through the MAC instead of
 * using the traditional bit-bang method. Data is read up until len bytes have
 * been obtained.
 */
static uint16_t
wpi_read_prom_data(struct wpi_softc *sc, uint32_t addr, void *data, int len)
{
	int ntries;
	uint32_t val;
	uint8_t *out = data;

	wpi_mem_lock(sc);

	for (; len > 0; len -= 2, addr++) {
		WPI_WRITE(sc, WPI_EEPROM_CTL, addr << 2);

		for (ntries = 0; ntries < 10; ntries++) {
			if ((val = WPI_READ(sc, WPI_EEPROM_CTL)) & WPI_EEPROM_READY)
				break;
			DELAY(5);
		}

		if (ntries == 10) {
			device_printf(sc->sc_dev, "could not read EEPROM\n");
			return ETIMEDOUT;
		}

		*out++= val >> 16;
		if (len > 1)
			*out ++= val >> 24;
	}

	wpi_mem_unlock(sc);

	return 0;
}

/*
 * The firmware text and data segments are transferred to the NIC using DMA.
 * The driver just copies the firmware into DMA-safe memory and tells the NIC
 * where to find it.  Once the NIC has copied the firmware into its internal
 * memory, we can free our local copy in the driver.
 */
static int
wpi_load_microcode(struct wpi_softc *sc, const uint8_t *fw, int size)
{
	int error, ntries;

	DPRINTFN(WPI_DEBUG_HW,("Loading microcode  size 0x%x\n", size));

	size /= sizeof(uint32_t);

	wpi_mem_lock(sc);

	wpi_mem_write_region_4(sc, WPI_MEM_UCODE_BASE,
	    (const uint32_t *)fw, size);

	wpi_mem_write(sc, WPI_MEM_UCODE_SRC, 0);
	wpi_mem_write(sc, WPI_MEM_UCODE_DST, WPI_FW_TEXT);
	wpi_mem_write(sc, WPI_MEM_UCODE_SIZE, size);

	/* run microcode */
	wpi_mem_write(sc, WPI_MEM_UCODE_CTL, WPI_UC_RUN);

	/* wait while the adapter is busy copying the firmware */
	for (error = 0, ntries = 0; ntries < 1000; ntries++) {
		uint32_t status = WPI_READ(sc, WPI_TX_STATUS);
		DPRINTFN(WPI_DEBUG_HW,
		    ("firmware status=0x%x, val=0x%x, result=0x%x\n", status,
		     WPI_TX_IDLE(6), status & WPI_TX_IDLE(6)));
		if (status & WPI_TX_IDLE(6)) {
			DPRINTFN(WPI_DEBUG_HW,
			    ("Status Match! - ntries = %d\n", ntries));
			break;
		}
		DELAY(10);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev, "timeout transferring firmware\n");
		error = ETIMEDOUT;
	}

	/* start the microcode executing */
	wpi_mem_write(sc, WPI_MEM_UCODE_CTL, WPI_UC_ENABLE);

	wpi_mem_unlock(sc);

	return (error);
}

static void
wpi_rx_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc,
	struct wpi_rx_data *data)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct wpi_rx_ring *ring = &sc->rxq;
	struct wpi_rx_stat *stat;
	struct wpi_rx_head *head;
	struct wpi_rx_tail *tail;
	struct ieee80211_node *ni;
	struct mbuf *m, *mnew;
	bus_addr_t paddr;
	int error;

	stat = (struct wpi_rx_stat *)(desc + 1);

	if (stat->len > WPI_STAT_MAXLEN) {
		device_printf(sc->sc_dev, "invalid rx statistic header\n");
		ifp->if_ierrors++;
		return;
	}

	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_POSTREAD);
	head = (struct wpi_rx_head *)((caddr_t)(stat + 1) + stat->len);
	tail = (struct wpi_rx_tail *)((caddr_t)(head + 1) + le16toh(head->len));

	DPRINTFN(WPI_DEBUG_RX, ("rx intr: idx=%d len=%d stat len=%d rssi=%d "
	    "rate=%x chan=%d tstamp=%ju\n", ring->cur, le32toh(desc->len),
	    le16toh(head->len), (int8_t)stat->rssi, head->rate, head->chan,
	    (uintmax_t)le64toh(tail->tstamp)));

	/* discard Rx frames with bad CRC early */
	if ((le32toh(tail->flags) & WPI_RX_NOERROR) != WPI_RX_NOERROR) {
		DPRINTFN(WPI_DEBUG_RX, ("%s: rx flags error %x\n", __func__,
		    le32toh(tail->flags)));
		ifp->if_ierrors++;
		return;
	}
	if (le16toh(head->len) < sizeof (struct ieee80211_frame)) {
		DPRINTFN(WPI_DEBUG_RX, ("%s: frame too short: %d\n", __func__,
		    le16toh(head->len)));
		ifp->if_ierrors++;
		return;
	}

	/* XXX don't need mbuf, just dma buffer */
	mnew = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, MJUMPAGESIZE);
	if (mnew == NULL) {
		DPRINTFN(WPI_DEBUG_RX, ("%s: no mbuf to restock ring\n",
		    __func__));
		ifp->if_ierrors++;
		return;
	}
	bus_dmamap_unload(ring->data_dmat, data->map);

	error = bus_dmamap_load(ring->data_dmat, data->map,
	    mtod(mnew, caddr_t), MJUMPAGESIZE,
	    wpi_dma_map_addr, &paddr, BUS_DMA_NOWAIT);
	if (error != 0 && error != EFBIG) {
		device_printf(sc->sc_dev,
		    "%s: bus_dmamap_load failed, error %d\n", __func__, error);
		m_freem(mnew);
		ifp->if_ierrors++;
		return;
	}
	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_PREWRITE);

	/* finalize mbuf and swap in new one */
	m = data->m;
	m->m_pkthdr.rcvif = ifp;
	m->m_data = (caddr_t)(head + 1);
	m->m_pkthdr.len = m->m_len = le16toh(head->len);

	data->m = mnew;
	/* update Rx descriptor */
	ring->desc[ring->cur] = htole32(paddr);

	if (ieee80211_radiotap_active(ic)) {
		struct wpi_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_chan_freq =
			htole16(ic->ic_channels[head->chan].ic_freq);
		tap->wr_chan_flags =
			htole16(ic->ic_channels[head->chan].ic_flags);
		tap->wr_dbm_antsignal = (int8_t)(stat->rssi - WPI_RSSI_OFFSET);
		tap->wr_dbm_antnoise = (int8_t)le16toh(stat->noise);
		tap->wr_tsft = tail->tstamp;
		tap->wr_antenna = (le16toh(head->flags) >> 4) & 0xf;
		switch (head->rate) {
		/* CCK rates */
		case  10: tap->wr_rate =   2; break;
		case  20: tap->wr_rate =   4; break;
		case  55: tap->wr_rate =  11; break;
		case 110: tap->wr_rate =  22; break;
		/* OFDM rates */
		case 0xd: tap->wr_rate =  12; break;
		case 0xf: tap->wr_rate =  18; break;
		case 0x5: tap->wr_rate =  24; break;
		case 0x7: tap->wr_rate =  36; break;
		case 0x9: tap->wr_rate =  48; break;
		case 0xb: tap->wr_rate =  72; break;
		case 0x1: tap->wr_rate =  96; break;
		case 0x3: tap->wr_rate = 108; break;
		/* unknown rate: should not happen */
		default:  tap->wr_rate =   0;
		}
		if (le16toh(head->flags) & 0x4)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
	}

	WPI_UNLOCK(sc);

	ni = ieee80211_find_rxnode(ic, mtod(m, struct ieee80211_frame_min *));
	if (ni != NULL) {
		(void) ieee80211_input(ni, m, stat->rssi, 0);
		ieee80211_free_node(ni);
	} else
		(void) ieee80211_input_all(ic, m, stat->rssi, 0);

	WPI_LOCK(sc);
}

static void
wpi_tx_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct wpi_tx_ring *ring = &sc->txq[desc->qid & 0x3];
	struct wpi_tx_data *txdata = &ring->data[desc->idx];
	struct wpi_tx_stat *stat = (struct wpi_tx_stat *)(desc + 1);
	struct ieee80211_node *ni = txdata->ni;
	struct ieee80211vap *vap = ni->ni_vap;
	int retrycnt = 0;

	DPRINTFN(WPI_DEBUG_TX, ("tx done: qid=%d idx=%d retries=%d nkill=%d "
	    "rate=%x duration=%d status=%x\n", desc->qid, desc->idx,
	    stat->ntries, stat->nkill, stat->rate, le32toh(stat->duration),
	    le32toh(stat->status)));

	/*
	 * Update rate control statistics for the node.
	 * XXX we should not count mgmt frames since they're always sent at
	 * the lowest available bit-rate.
	 * XXX frames w/o ACK shouldn't be used either
	 */
	if (stat->ntries > 0) {
		DPRINTFN(WPI_DEBUG_TX, ("%d retries\n", stat->ntries));
		retrycnt = 1;
	}
	ieee80211_ratectl_tx_complete(vap, ni, IEEE80211_RATECTL_TX_SUCCESS,
	    &retrycnt, NULL);

	/* XXX oerrors should only count errors !maxtries */
	if ((le32toh(stat->status) & 0xff) != 1)
		ifp->if_oerrors++;
	else
		ifp->if_opackets++;

	bus_dmamap_sync(ring->data_dmat, txdata->map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(ring->data_dmat, txdata->map);
	/* XXX handle M_TXCB? */
	m_freem(txdata->m);
	txdata->m = NULL;
	ieee80211_free_node(txdata->ni);
	txdata->ni = NULL;

	ring->queued--;

	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	wpi_start_locked(ifp);
}

static void
wpi_cmd_intr(struct wpi_softc *sc, struct wpi_rx_desc *desc)
{
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_data *data;

	DPRINTFN(WPI_DEBUG_CMD, ("cmd notification qid=%x idx=%d flags=%x "
				 "type=%s len=%d\n", desc->qid, desc->idx,
				 desc->flags, wpi_cmd_str(desc->type),
				 le32toh(desc->len)));

	if ((desc->qid & 7) != 4)
		return;	/* not a command ack */

	data = &ring->data[desc->idx];

	/* if the command was mapped in a mbuf, free it */
	if (data->m != NULL) {
		bus_dmamap_unload(ring->data_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
	}

	sc->flags &= ~WPI_FLAG_BUSY;
	wakeup(&ring->cmd[desc->idx]);
}

static void
wpi_notif_intr(struct wpi_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct wpi_rx_desc *desc;
	struct wpi_rx_data *data;
	uint32_t hw;

	bus_dmamap_sync(sc->shared_dma.tag, sc->shared_dma.map,
	    BUS_DMASYNC_POSTREAD);

	hw = le32toh(sc->shared->next);
	while (sc->rxq.cur != hw) {
		data = &sc->rxq.data[sc->rxq.cur];

		bus_dmamap_sync(sc->rxq.data_dmat, data->map,
		    BUS_DMASYNC_POSTREAD);
		desc = (void *)data->m->m_ext.ext_buf;

		DPRINTFN(WPI_DEBUG_NOTIFY,
			 ("notify qid=%x idx=%d flags=%x type=%d len=%d\n",
			  desc->qid,
			  desc->idx,
			  desc->flags,
			  desc->type,
			  le32toh(desc->len)));

		if (!(desc->qid & 0x80))	/* reply to a command */
			wpi_cmd_intr(sc, desc);

		switch (desc->type) {
		case WPI_RX_DONE:
			/* a 802.11 frame was received */
			wpi_rx_intr(sc, desc, data);
			break;

		case WPI_TX_DONE:
			/* a 802.11 frame has been transmitted */
			wpi_tx_intr(sc, desc);
			break;

		case WPI_UC_READY:
		{
			struct wpi_ucode_info *uc =
				(struct wpi_ucode_info *)(desc + 1);

			/* the microcontroller is ready */
			DPRINTF(("microcode alive notification version %x "
				"alive %x\n", le32toh(uc->version),
				le32toh(uc->valid)));

			if (le32toh(uc->valid) != 1) {
				device_printf(sc->sc_dev,
				    "microcontroller initialization failed\n");
				wpi_stop_locked(sc);
			}
			break;
		}
		case WPI_STATE_CHANGED:
		{
			uint32_t *status = (uint32_t *)(desc + 1);

			/* enabled/disabled notification */
			DPRINTF(("state changed to %x\n", le32toh(*status)));

			if (le32toh(*status) & 1) {
				device_printf(sc->sc_dev,
				    "Radio transmitter is switched off\n");
				sc->flags |= WPI_FLAG_HW_RADIO_OFF;
				ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
				/* Disable firmware commands */
				WPI_WRITE(sc, WPI_UCODE_SET, WPI_DISABLE_CMD);
			}
			break;
		}
		case WPI_START_SCAN:
		{
#ifdef WPI_DEBUG
			struct wpi_start_scan *scan =
				(struct wpi_start_scan *)(desc + 1);
#endif

			DPRINTFN(WPI_DEBUG_SCANNING,
				 ("scanning channel %d status %x\n",
			    scan->chan, le32toh(scan->status)));
			break;
		}
		case WPI_STOP_SCAN:
		{
#ifdef WPI_DEBUG
			struct wpi_stop_scan *scan =
				(struct wpi_stop_scan *)(desc + 1);
#endif
			struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

			DPRINTFN(WPI_DEBUG_SCANNING,
			    ("scan finished nchan=%d status=%d chan=%d\n",
			     scan->nchan, scan->status, scan->chan));

			sc->sc_scan_timer = 0;
			ieee80211_scan_next(vap);
			break;
		}
		case WPI_MISSED_BEACON:
		{
			struct wpi_missed_beacon *beacon =
				(struct wpi_missed_beacon *)(desc + 1);
			struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

			if (le32toh(beacon->consecutive) >=
			    vap->iv_bmissthreshold) {
				DPRINTF(("Beacon miss: %u >= %u\n",
					 le32toh(beacon->consecutive),
					 vap->iv_bmissthreshold));
				ieee80211_beacon_miss(ic);
			}
			break;
		}
		}

		sc->rxq.cur = (sc->rxq.cur + 1) % WPI_RX_RING_COUNT;
	}

	/* tell the firmware what we have processed */
	hw = (hw == 0) ? WPI_RX_RING_COUNT - 1 : hw - 1;
	WPI_WRITE(sc, WPI_RX_WIDX, hw & ~7);
}

static void
wpi_intr(void *arg)
{
	struct wpi_softc *sc = arg;
	uint32_t r;

	WPI_LOCK(sc);

	r = WPI_READ(sc, WPI_INTR);
	if (r == 0 || r == 0xffffffff) {
		WPI_UNLOCK(sc);
		return;
	}

	/* disable interrupts */
	WPI_WRITE(sc, WPI_MASK, 0);
	/* ack interrupts */
	WPI_WRITE(sc, WPI_INTR, r);

	if (r & (WPI_SW_ERROR | WPI_HW_ERROR)) {
		struct ifnet *ifp = sc->sc_ifp;
		struct ieee80211com *ic = ifp->if_l2com;
		struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

		device_printf(sc->sc_dev, "fatal firmware error\n");
		DPRINTFN(6,("(%s)\n", (r & WPI_SW_ERROR) ? "(Software Error)" :
				"(Hardware Error)"));
		if (vap != NULL)
			ieee80211_cancel_scan(vap);
		ieee80211_runtask(ic, &sc->sc_restarttask);
		sc->flags &= ~WPI_FLAG_BUSY;
		WPI_UNLOCK(sc);
		return;
	}

	if (r & WPI_RX_INTR)
		wpi_notif_intr(sc);

	if (r & WPI_ALIVE_INTR)	/* firmware initialized */
		wakeup(sc);

	/* re-enable interrupts */
	if (sc->sc_ifp->if_flags & IFF_UP)
		WPI_WRITE(sc, WPI_MASK, WPI_INTR_MASK);

	WPI_UNLOCK(sc);
}

static uint8_t
wpi_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates (returned values are device-dependent) */
	case 2:		return 10;
	case 4:		return 20;
	case 11:	return 55;
	case 22:	return 110;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	/* R1-R4 (ral/ural is R4-R1) */
	case 12:	return 0xd;
	case 18:	return 0xf;
	case 24:	return 0x5;
	case 36:	return 0x7;
	case 48:	return 0x9;
	case 72:	return 0xb;
	case 96:	return 0x1;
	case 108:	return 0x3;

	/* unsupported rates (should not get there) */
	default:	return 0;
	}
}

/* quickly determine if a given rate is CCK or OFDM */
#define WPI_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

/*
 * Construct the data packet for a transmit buffer and acutally put
 * the buffer onto the transmit ring, kicking the card to process the
 * the buffer.
 */
static int
wpi_tx_data(struct wpi_softc *sc, struct mbuf *m0, struct ieee80211_node *ni,
	int ac)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	const struct chanAccParams *cap = &ic->ic_wme.wme_chanParams;
	struct wpi_tx_ring *ring = &sc->txq[ac];
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_cmd_data *tx;
	struct ieee80211_frame *wh;
	const struct ieee80211_txparam *tp;
	struct ieee80211_key *k;
	struct mbuf *mnew;
	int i, error, nsegs, rate, hdrlen, ismcast;
	bus_dma_segment_t segs[WPI_MAX_SCATTER];

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	wh = mtod(m0, struct ieee80211_frame *);

	hdrlen = ieee80211_hdrsize(wh);
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}
		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	cmd = &ring->cmd[ring->cur];
	cmd->code = WPI_CMD_TX_DATA;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	tx = (struct wpi_cmd_data *)cmd->data;
	tx->flags = htole32(WPI_TX_AUTO_SEQ);
	tx->timeout = htole16(0);
	tx->ofdm_mask = 0xff;
	tx->cck_mask = 0x0f;
	tx->lifetime = htole32(WPI_LIFETIME_INFINITE);
	tx->id = ismcast ? WPI_ID_BROADCAST : WPI_ID_BSS;
	tx->len = htole16(m0->m_pkthdr.len);

	if (!ismcast) {
		if ((ni->ni_flags & IEEE80211_NODE_QOS) == 0 ||
		    !cap->cap_wmeParams[ac].wmep_noackPolicy)
			tx->flags |= htole32(WPI_TX_NEED_ACK);
		if (m0->m_pkthdr.len + IEEE80211_CRC_LEN > vap->iv_rtsthreshold) {
			tx->flags |= htole32(WPI_TX_NEED_RTS|WPI_TX_FULL_TXOP);
			tx->rts_ntries = 7;
		}
	}
	/* pick a rate */
	tp = &vap->iv_txparms[ieee80211_chan2mode(ni->ni_chan)];
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGT) {
		uint8_t subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		/* tell h/w to set timestamp in probe responses */
		if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			tx->flags |= htole32(WPI_TX_INSERT_TSTAMP);
		if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
		    subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)
			tx->timeout = htole16(3);
		else
			tx->timeout = htole16(2);
		rate = tp->mgmtrate;
	} else if (ismcast) {
		rate = tp->mcastrate;
	} else if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE) {
		rate = tp->ucastrate;
	} else {
		(void) ieee80211_ratectl_rate(ni, NULL, 0);
		rate = ni->ni_txrate;
	}
	tx->rate = wpi_plcp_signal(rate);

	/* be very persistant at sending frames out */
#if 0
	tx->data_ntries = tp->maxretry;
#else
	tx->data_ntries = 15;		/* XXX way too high */
#endif

	if (ieee80211_radiotap_active_vap(vap)) {
		struct wpi_tx_radiotap_header *tap = &sc->sc_txtap;
		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_hwqueue = ac;
		if (wh->i_fc[1] & IEEE80211_FC1_WEP)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		ieee80211_radiotap_tx(vap, m0);
	}

	/* save and trim IEEE802.11 header */
	m_copydata(m0, 0, hdrlen, (caddr_t)&tx->wh);
	m_adj(m0, hdrlen);

	error = bus_dmamap_load_mbuf_sg(ring->data_dmat, data->map, m0, segs,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error != 0 && error != EFBIG) {
		device_printf(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}
	if (error != 0) {
		/* XXX use m_collapse */
		mnew = m_defrag(m0, M_NOWAIT);
		if (mnew == NULL) {
			device_printf(sc->sc_dev,
			    "could not defragment mbuf\n");
			m_freem(m0);
			return ENOBUFS;
		}
		m0 = mnew;

		error = bus_dmamap_load_mbuf_sg(ring->data_dmat, data->map,
		    m0, segs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not map mbuf (error %d)\n", error);
			m_freem(m0);
			return error;
		}
	}

	data->m = m0;
	data->ni = ni;

	DPRINTFN(WPI_DEBUG_TX, ("sending data: qid=%d idx=%d len=%d nsegs=%d\n",
	    ring->qid, ring->cur, m0->m_pkthdr.len, nsegs));

	/* first scatter/gather segment is used by the tx data command */
	desc->flags = htole32(WPI_PAD32(m0->m_pkthdr.len) << 28 |
	    (1 + nsegs) << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
	    ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + sizeof (struct wpi_cmd_data));
	for (i = 1; i <= nsegs; i++) {
		desc->segs[i].addr = htole32(segs[i - 1].ds_addr);
		desc->segs[i].len  = htole32(segs[i - 1].ds_len);
	}

	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->desc_dma.tag, ring->desc_dma.map,
	    BUS_DMASYNC_PREWRITE);

	ring->queued++;

	/* kick ring */
	ring->cur = (ring->cur + 1) % WPI_TX_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;
}

/**
 * Process data waiting to be sent on the IFNET output queue
 */
static void
wpi_start(struct ifnet *ifp)
{
	struct wpi_softc *sc = ifp->if_softc;

	WPI_LOCK(sc);
	wpi_start_locked(ifp);
	WPI_UNLOCK(sc);
}

static void
wpi_start_locked(struct ifnet *ifp)
{
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	struct mbuf *m;
	int ac;

	WPI_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	for (;;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		ac = M_WME_GETAC(m);
		if (sc->txq[ac].queued > sc->txq[ac].count - 8) {
			/* there is no place left in this ring */
			IFQ_DRV_PREPEND(&ifp->if_snd, m);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
		if (wpi_tx_data(sc, m, ni, ac) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			break;
		}
		sc->sc_tx_timer = 5;
	}
}

static int
wpi_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct wpi_softc *sc = ifp->if_softc;

	/* prevent management frames from being sent if we're not ready */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		m_freem(m);
		ieee80211_free_node(ni);
		return ENETDOWN;
	}
	WPI_LOCK(sc);

	/* management frames go into ring 0 */
	if (sc->txq[0].queued > sc->txq[0].count - 8) {
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		m_freem(m);
		WPI_UNLOCK(sc);
		ieee80211_free_node(ni);
		return ENOBUFS;		/* XXX */
	}

	ifp->if_opackets++;
	if (wpi_tx_data(sc, m, ni, 0) != 0)
		goto bad;
	sc->sc_tx_timer = 5;
	callout_reset(&sc->watchdog_to, hz, wpi_watchdog, sc);

	WPI_UNLOCK(sc);
	return 0;
bad:
	ifp->if_oerrors++;
	WPI_UNLOCK(sc);
	ieee80211_free_node(ni);
	return EIO;		/* XXX */
}

static int
wpi_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct wpi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0, startall = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		WPI_LOCK(sc);
		if ((ifp->if_flags & IFF_UP)) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				wpi_init_locked(sc, 0);
				startall = 1;
			}
		} else if ((ifp->if_drv_flags & IFF_DRV_RUNNING) ||
			   (sc->flags & WPI_FLAG_HW_RADIO_OFF))
			wpi_stop_locked(sc);
		WPI_UNLOCK(sc);
		if (startall)
			ieee80211_start_all(ic);
		break;
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &ic->ic_media, cmd);
		break;
	case SIOCGIFADDR:
		error = ether_ioctl(ifp, cmd, data);
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

/*
 * Extract various information from EEPROM.
 */
static void
wpi_read_eeprom(struct wpi_softc *sc, uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	int i;

	/* read the hardware capabilities, revision and SKU type */
	wpi_read_prom_data(sc, WPI_EEPROM_CAPABILITIES, &sc->cap,1);
	wpi_read_prom_data(sc, WPI_EEPROM_REVISION, &sc->rev,2);
	wpi_read_prom_data(sc, WPI_EEPROM_TYPE, &sc->type, 1);

	/* read the regulatory domain */
	wpi_read_prom_data(sc, WPI_EEPROM_DOMAIN, sc->domain, 4);

	/* read in the hw MAC address */
	wpi_read_prom_data(sc, WPI_EEPROM_MAC, macaddr, 6);

	/* read the list of authorized channels */
	for (i = 0; i < WPI_CHAN_BANDS_COUNT; i++)
		wpi_read_eeprom_channels(sc,i);

	/* read the power level calibration info for each group */
	for (i = 0; i < WPI_POWER_GROUPS_COUNT; i++)
		wpi_read_eeprom_group(sc,i);
}

/*
 * Send a command to the firmware.
 */
static int
wpi_cmd(struct wpi_softc *sc, int code, const void *buf, int size, int async)
{
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_desc *desc;
	struct wpi_tx_cmd *cmd;

#ifdef WPI_DEBUG
	if (!async) {
		WPI_LOCK_ASSERT(sc);
	}
#endif

	DPRINTFN(WPI_DEBUG_CMD,("wpi_cmd %d size %d async %d\n", code, size,
		    async));

	if (sc->flags & WPI_FLAG_BUSY) {
		device_printf(sc->sc_dev, "%s: cmd %d not sent, busy\n",
		    __func__, code);
		return EAGAIN;
	}
	sc->flags|= WPI_FLAG_BUSY;

	KASSERT(size <= sizeof cmd->data, ("command %d too large: %d bytes",
	    code, size));

	desc = &ring->desc[ring->cur];
	cmd = &ring->cmd[ring->cur];

	cmd->code = code;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;
	memcpy(cmd->data, buf, size);

	desc->flags = htole32(WPI_PAD32(size) << 28 | 1 << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
		ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + size);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_CMD_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	if (async) {
		sc->flags &= ~ WPI_FLAG_BUSY;
		return 0;
	}

	return msleep(cmd, &sc->sc_mtx, PCATCH, "wpicmd", hz);
}

static int
wpi_wme_update(struct ieee80211com *ic)
{
#define WPI_EXP2(v)	htole16((1 << (v)) - 1)
#define WPI_USEC(v)	htole16(IEEE80211_TXOP_TO_US(v))
	struct wpi_softc *sc = ic->ic_ifp->if_softc;
	const struct wmeParams *wmep;
	struct wpi_wme_setup wme;
	int ac;

	/* don't override default WME values if WME is not actually enabled */
	if (!(ic->ic_flags & IEEE80211_F_WME))
		return 0;

	wme.flags = 0;
	for (ac = 0; ac < WME_NUM_AC; ac++) {
		wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[ac];
		wme.ac[ac].aifsn = wmep->wmep_aifsn;
		wme.ac[ac].cwmin = WPI_EXP2(wmep->wmep_logcwmin);
		wme.ac[ac].cwmax = WPI_EXP2(wmep->wmep_logcwmax);
		wme.ac[ac].txop  = WPI_USEC(wmep->wmep_txopLimit);

		DPRINTF(("setting WME for queue %d aifsn=%d cwmin=%d cwmax=%d "
		    "txop=%d\n", ac, wme.ac[ac].aifsn, wme.ac[ac].cwmin,
		    wme.ac[ac].cwmax, wme.ac[ac].txop));
	}
	return wpi_cmd(sc, WPI_CMD_SET_WME, &wme, sizeof wme, 1);
#undef WPI_USEC
#undef WPI_EXP2
}

/*
 * Configure h/w multi-rate retries.
 */
static int
wpi_mrr_setup(struct wpi_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct wpi_mrr_setup mrr;
	int i, error;

	memset(&mrr, 0, sizeof (struct wpi_mrr_setup));

	/* CCK rates (not used with 802.11a) */
	for (i = WPI_CCK1; i <= WPI_CCK11; i++) {
		mrr.rates[i].flags = 0;
		mrr.rates[i].signal = wpi_ridx_to_plcp[i];
		/* fallback to the immediate lower CCK rate (if any) */
		mrr.rates[i].next = (i == WPI_CCK1) ? WPI_CCK1 : i - 1;
		/* try one time at this rate before falling back to "next" */
		mrr.rates[i].ntries = 1;
	}

	/* OFDM rates (not used with 802.11b) */
	for (i = WPI_OFDM6; i <= WPI_OFDM54; i++) {
		mrr.rates[i].flags = 0;
		mrr.rates[i].signal = wpi_ridx_to_plcp[i];
		/* fallback to the immediate lower OFDM rate (if any) */
		/* we allow fallback from OFDM/6 to CCK/2 in 11b/g mode */
		mrr.rates[i].next = (i == WPI_OFDM6) ?
		    ((ic->ic_curmode == IEEE80211_MODE_11A) ?
			WPI_OFDM6 : WPI_CCK2) :
		    i - 1;
		/* try one time at this rate before falling back to "next" */
		mrr.rates[i].ntries = 1;
	}

	/* setup MRR for control frames */
	mrr.which = WPI_MRR_CTL;
	error = wpi_cmd(sc, WPI_CMD_MRR_SETUP, &mrr, sizeof mrr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not setup MRR for control frames\n");
		return error;
	}

	/* setup MRR for data frames */
	mrr.which = WPI_MRR_DATA;
	error = wpi_cmd(sc, WPI_CMD_MRR_SETUP, &mrr, sizeof mrr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not setup MRR for data frames\n");
		return error;
	}

	return 0;
}

static void
wpi_set_led(struct wpi_softc *sc, uint8_t which, uint8_t off, uint8_t on)
{
	struct wpi_cmd_led led;

	led.which = which;
	led.unit = htole32(100000);	/* on/off in unit of 100ms */
	led.off = off;
	led.on = on;

	(void)wpi_cmd(sc, WPI_CMD_SET_LED, &led, sizeof led, 1);
}

static void
wpi_enable_tsf(struct wpi_softc *sc, struct ieee80211_node *ni)
{
	struct wpi_cmd_tsf tsf;
	uint64_t val, mod;

	memset(&tsf, 0, sizeof tsf);
	memcpy(&tsf.tstamp, ni->ni_tstamp.data, 8);
	tsf.bintval = htole16(ni->ni_intval);
	tsf.lintval = htole16(10);

	/* compute remaining time until next beacon */
	val = (uint64_t)ni->ni_intval  * 1024;	/* msec -> usec */
	mod = le64toh(tsf.tstamp) % val;
	tsf.binitval = htole32((uint32_t)(val - mod));

	if (wpi_cmd(sc, WPI_CMD_TSF, &tsf, sizeof tsf, 1) != 0)
		device_printf(sc->sc_dev, "could not enable TSF\n");
}

#if 0
/*
 * Build a beacon frame that the firmware will broadcast periodically in
 * IBSS or HostAP modes.
 */
static int
wpi_setup_beacon(struct wpi_softc *sc, struct ieee80211_node *ni)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_cmd_beacon *bcn;
	struct ieee80211_beacon_offsets bo;
	struct mbuf *m0;
	bus_addr_t physaddr;
	int error;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	m0 = ieee80211_beacon_alloc(ic, ni, &bo);
	if (m0 == NULL) {
		device_printf(sc->sc_dev, "could not allocate beacon frame\n");
		return ENOMEM;
	}

	cmd = &ring->cmd[ring->cur];
	cmd->code = WPI_CMD_SET_BEACON;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	bcn = (struct wpi_cmd_beacon *)cmd->data;
	memset(bcn, 0, sizeof (struct wpi_cmd_beacon));
	bcn->id = WPI_ID_BROADCAST;
	bcn->ofdm_mask = 0xff;
	bcn->cck_mask = 0x0f;
	bcn->lifetime = htole32(WPI_LIFETIME_INFINITE);
	bcn->len = htole16(m0->m_pkthdr.len);
	bcn->rate = (ic->ic_curmode == IEEE80211_MODE_11A) ?
		wpi_plcp_signal(12) : wpi_plcp_signal(2);
	bcn->flags = htole32(WPI_TX_AUTO_SEQ | WPI_TX_INSERT_TSTAMP);

	/* save and trim IEEE802.11 header */
	m_copydata(m0, 0, sizeof (struct ieee80211_frame), (caddr_t)&bcn->wh);
	m_adj(m0, sizeof (struct ieee80211_frame));

	/* assume beacon frame is contiguous */
	error = bus_dmamap_load(ring->data_dmat, data->map, mtod(m0, void *),
	    m0->m_pkthdr.len, wpi_dma_map_addr, &physaddr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not map beacon\n");
		m_freem(m0);
		return error;
	}

	data->m = m0;

	/* first scatter/gather segment is used by the beacon command */
	desc->flags = htole32(WPI_PAD32(m0->m_pkthdr.len) << 28 | 2 << 24);
	desc->segs[0].addr = htole32(ring->cmd_dma.paddr +
		ring->cur * sizeof (struct wpi_tx_cmd));
	desc->segs[0].len  = htole32(4 + sizeof (struct wpi_cmd_beacon));
	desc->segs[1].addr = htole32(physaddr);
	desc->segs[1].len  = htole32(m0->m_pkthdr.len);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_CMD_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;
}
#endif

static int
wpi_auth(struct wpi_softc *sc, struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;
	struct wpi_node_info node;
	int error;


	/* update adapter's configuration */
	sc->config.associd = 0;
	sc->config.filter &= ~htole32(WPI_FILTER_BSS);
	IEEE80211_ADDR_COPY(sc->config.bssid, ni->ni_bssid);
	sc->config.chan = ieee80211_chan2ieee(ic, ni->ni_chan);
	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan)) {
		sc->config.flags |= htole32(WPI_CONFIG_AUTO |
		    WPI_CONFIG_24GHZ);
	} else {
		sc->config.flags &= ~htole32(WPI_CONFIG_AUTO |
		    WPI_CONFIG_24GHZ);
	}
	if (IEEE80211_IS_CHAN_A(ni->ni_chan)) {
		sc->config.cck_mask  = 0;
		sc->config.ofdm_mask = 0x15;
	} else if (IEEE80211_IS_CHAN_B(ni->ni_chan)) {
		sc->config.cck_mask  = 0x03;
		sc->config.ofdm_mask = 0;
	} else {
		/* XXX assume 802.11b/g */
		sc->config.cck_mask  = 0x0f;
		sc->config.ofdm_mask = 0x15;
	}

	DPRINTF(("config chan %d flags %x cck %x ofdm %x\n", sc->config.chan,
		sc->config.flags, sc->config.cck_mask, sc->config.ofdm_mask));
	error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config,
		sizeof (struct wpi_config), 1);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not configure\n");
		return error;
	}

	/* configuration has changed, set Tx power accordingly */
	if ((error = wpi_set_txpower(sc, ni->ni_chan, 1)) != 0) {
		device_printf(sc->sc_dev, "could not set Tx power\n");
		return error;
	}

	/* add default node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.bssid, ni->ni_bssid);
	node.id = WPI_ID_BSS;
	node.rate = (ic->ic_curmode == IEEE80211_MODE_11A) ?
	    wpi_plcp_signal(12) : wpi_plcp_signal(2);
	node.action = htole32(WPI_ACTION_SET_RATE);
	node.antenna = WPI_ANTENNA_BOTH;
	error = wpi_cmd(sc, WPI_CMD_ADD_NODE, &node, sizeof node, 1);
	if (error != 0)
		device_printf(sc->sc_dev, "could not add BSS node\n");

	return (error);
}

static int
wpi_run(struct wpi_softc *sc, struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni = vap->iv_bss;
	int error;

	if (vap->iv_opmode == IEEE80211_M_MONITOR) {
		/* link LED blinks while monitoring */
		wpi_set_led(sc, WPI_LED_LINK, 5, 5);
		return 0;
	}

	wpi_enable_tsf(sc, ni);

	/* update adapter's configuration */
	sc->config.associd = htole16(ni->ni_associd & ~0xc000);
	/* short preamble/slot time are negotiated when associating */
	sc->config.flags &= ~htole32(WPI_CONFIG_SHPREAMBLE |
	    WPI_CONFIG_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		sc->config.flags |= htole32(WPI_CONFIG_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		sc->config.flags |= htole32(WPI_CONFIG_SHPREAMBLE);
	sc->config.filter |= htole32(WPI_FILTER_BSS);

	/* XXX put somewhere HC_QOS_SUPPORT_ASSOC + HC_IBSS_START */

	DPRINTF(("config chan %d flags %x\n", sc->config.chan,
		    sc->config.flags));
	error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config, sizeof (struct
		    wpi_config), 1);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not update configuration\n");
		return error;
	}

	error = wpi_set_txpower(sc, ni->ni_chan, 1);
	if (error != 0) {
		device_printf(sc->sc_dev, "could set txpower\n");
		return error;
	}

	/* link LED always on while associated */
	wpi_set_led(sc, WPI_LED_LINK, 0, 1);

	/* start automatic rate control timer */
	callout_reset(&sc->calib_to, 60*hz, wpi_calib_timeout, sc);

	return (error);
}

/*
 * Send a scan request to the firmware.  Since this command is huge, we map it
 * into a mbufcluster instead of using the pre-allocated set of commands. Note,
 * much of this code is similar to that in wpi_cmd but because we must manually
 * construct the probe & channels, we duplicate what's needed here. XXX In the
 * future, this function should be modified to use wpi_cmd to help cleanup the
 * code base.
 */
static int
wpi_scan(struct wpi_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_scan_state *ss = ic->ic_scan;
	struct wpi_tx_ring *ring = &sc->cmdq;
	struct wpi_tx_desc *desc;
	struct wpi_tx_data *data;
	struct wpi_tx_cmd *cmd;
	struct wpi_scan_hdr *hdr;
	struct wpi_scan_chan *chan;
	struct ieee80211_frame *wh;
	struct ieee80211_rateset *rs;
	struct ieee80211_channel *c;
	enum ieee80211_phymode mode;
	uint8_t *frm;
	int nrates, pktlen, error, i, nssid;
	bus_addr_t physaddr;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	data->m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (data->m == NULL) {
		device_printf(sc->sc_dev,
		    "could not allocate mbuf for scan command\n");
		return ENOMEM;
	}

	cmd = mtod(data->m, struct wpi_tx_cmd *);
	cmd->code = WPI_CMD_SCAN;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	hdr = (struct wpi_scan_hdr *)cmd->data;
	memset(hdr, 0, sizeof(struct wpi_scan_hdr));

	/*
	 * Move to the next channel if no packets are received within 5 msecs
	 * after sending the probe request (this helps to reduce the duration
	 * of active scans).
	 */
	hdr->quiet = htole16(5);
	hdr->threshold = htole16(1);

	if (IEEE80211_IS_CHAN_A(ic->ic_curchan)) {
		/* send probe requests at 6Mbps */
		hdr->tx.rate = wpi_ridx_to_plcp[WPI_OFDM6];

		/* Enable crc checking */
		hdr->promotion = htole16(1);
	} else {
		hdr->flags = htole32(WPI_CONFIG_24GHZ | WPI_CONFIG_AUTO);
		/* send probe requests at 1Mbps */
		hdr->tx.rate = wpi_ridx_to_plcp[WPI_CCK1];
	}
	hdr->tx.id = WPI_ID_BROADCAST;
	hdr->tx.lifetime = htole32(WPI_LIFETIME_INFINITE);
	hdr->tx.flags = htole32(WPI_TX_AUTO_SEQ);

	memset(hdr->scan_essids, 0, sizeof(hdr->scan_essids));
	nssid = MIN(ss->ss_nssid, WPI_SCAN_MAX_ESSIDS);
	for (i = 0; i < nssid; i++) {
		hdr->scan_essids[i].id = IEEE80211_ELEMID_SSID;
		hdr->scan_essids[i].esslen = MIN(ss->ss_ssid[i].len, 32);
		memcpy(hdr->scan_essids[i].essid, ss->ss_ssid[i].ssid,
		    hdr->scan_essids[i].esslen);
#ifdef WPI_DEBUG
		if (wpi_debug & WPI_DEBUG_SCANNING) {
			printf("Scanning Essid: ");
			ieee80211_print_essid(hdr->scan_essids[i].essid,
			    hdr->scan_essids[i].esslen);
			printf("\n");
		}
#endif
	}

	/*
	 * Build a probe request frame.  Most of the following code is a
	 * copy & paste of what is done in net80211.
	 */
	wh = (struct ieee80211_frame *)&hdr->scan_essids[4];
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
		IEEE80211_FC0_SUBTYPE_PROBE_REQ;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	IEEE80211_ADDR_COPY(wh->i_addr1, ifp->if_broadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, IF_LLADDR(ifp));
	IEEE80211_ADDR_COPY(wh->i_addr3, ifp->if_broadcastaddr);
	*(u_int16_t *)&wh->i_dur[0] = 0;	/* filled by h/w */
	*(u_int16_t *)&wh->i_seq[0] = 0;	/* filled by h/w */

	frm = (uint8_t *)(wh + 1);

	/* add essid IE, the hardware will fill this in for us */
	*frm++ = IEEE80211_ELEMID_SSID;
	*frm++ = 0;

	mode = ieee80211_chan2mode(ic->ic_curchan);
	rs = &ic->ic_sup_rates[mode];

	/* add supported rates IE */
	*frm++ = IEEE80211_ELEMID_RATES;
	nrates = rs->rs_nrates;
	if (nrates > IEEE80211_RATE_SIZE)
		nrates = IEEE80211_RATE_SIZE;
	*frm++ = nrates;
	memcpy(frm, rs->rs_rates, nrates);
	frm += nrates;

	/* add supported xrates IE */
	if (rs->rs_nrates > IEEE80211_RATE_SIZE) {
		nrates = rs->rs_nrates - IEEE80211_RATE_SIZE;
		*frm++ = IEEE80211_ELEMID_XRATES;
		*frm++ = nrates;
		memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
		frm += nrates;
	}

	/* setup length of probe request */
	hdr->tx.len = htole16(frm - (uint8_t *)wh);

	/*
	 * Construct information about the channel that we
	 * want to scan. The firmware expects this to be directly
	 * after the scan probe request
	 */
	c = ic->ic_curchan;
	chan = (struct wpi_scan_chan *)frm;
	chan->chan = ieee80211_chan2ieee(ic, c);
	chan->flags = 0;
	if (!(c->ic_flags & IEEE80211_CHAN_PASSIVE)) {
		chan->flags |= WPI_CHAN_ACTIVE;
		if (nssid != 0)
			chan->flags |= WPI_CHAN_DIRECT;
	}
	chan->gain_dsp = 0x6e; /* Default level */
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		chan->active = htole16(10);
		chan->passive = htole16(ss->ss_maxdwell);
		chan->gain_radio = 0x3b;
	} else {
		chan->active = htole16(20);
		chan->passive = htole16(ss->ss_maxdwell);
		chan->gain_radio = 0x28;
	}

	DPRINTFN(WPI_DEBUG_SCANNING,
	    ("Scanning %u Passive: %d\n",
	     chan->chan,
	     c->ic_flags & IEEE80211_CHAN_PASSIVE));

	hdr->nchan++;
	chan++;

	frm += sizeof (struct wpi_scan_chan);
#if 0
	// XXX All Channels....
	for (c  = &ic->ic_channels[1];
	     c <= &ic->ic_channels[IEEE80211_CHAN_MAX]; c++) {
		if ((c->ic_flags & ic->ic_curchan->ic_flags) != ic->ic_curchan->ic_flags)
			continue;

		chan->chan = ieee80211_chan2ieee(ic, c);
		chan->flags = 0;
		if (!(c->ic_flags & IEEE80211_CHAN_PASSIVE)) {
		    chan->flags |= WPI_CHAN_ACTIVE;
		    if (ic->ic_des_ssid[0].len != 0)
			chan->flags |= WPI_CHAN_DIRECT;
		}
		chan->gain_dsp = 0x6e; /* Default level */
		if (IEEE80211_IS_CHAN_5GHZ(c)) {
			chan->active = htole16(10);
			chan->passive = htole16(110);
			chan->gain_radio = 0x3b;
		} else {
			chan->active = htole16(20);
			chan->passive = htole16(120);
			chan->gain_radio = 0x28;
		}

		DPRINTFN(WPI_DEBUG_SCANNING,
			 ("Scanning %u Passive: %d\n",
			  chan->chan,
			  c->ic_flags & IEEE80211_CHAN_PASSIVE));

		hdr->nchan++;
		chan++;

		frm += sizeof (struct wpi_scan_chan);
	}
#endif

	hdr->len = htole16(frm - (uint8_t *)hdr);
	pktlen = frm - (uint8_t *)cmd;

	error = bus_dmamap_load(ring->data_dmat, data->map, cmd, pktlen,
	    wpi_dma_map_addr, &physaddr, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not map scan command\n");
		m_freem(data->m);
		data->m = NULL;
		return error;
	}

	desc->flags = htole32(WPI_PAD32(pktlen) << 28 | 1 << 24);
	desc->segs[0].addr = htole32(physaddr);
	desc->segs[0].len  = htole32(pktlen);

	bus_dmamap_sync(ring->desc_dma.tag, ring->desc_dma.map,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_PREWRITE);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % WPI_CMD_RING_COUNT;
	WPI_WRITE(sc, WPI_TX_WIDX, ring->qid << 8 | ring->cur);

	sc->sc_scan_timer = 5;
	return 0;	/* will be notified async. of failure/success */
}

/**
 * Configure the card to listen to a particular channel, this transisions the
 * card in to being able to receive frames from remote devices.
 */
static int
wpi_config(struct wpi_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct wpi_power power;
	struct wpi_bluetooth bluetooth;
	struct wpi_node_info node;
	int error;

	/* set power mode */
	memset(&power, 0, sizeof power);
	power.flags = htole32(WPI_POWER_CAM|0x8);
	error = wpi_cmd(sc, WPI_CMD_SET_POWER_MODE, &power, sizeof power, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not set power mode\n");
		return error;
	}

	/* configure bluetooth coexistence */
	memset(&bluetooth, 0, sizeof bluetooth);
	bluetooth.flags = 3;
	bluetooth.lead = 0xaa;
	bluetooth.kill = 1;
	error = wpi_cmd(sc, WPI_CMD_BLUETOOTH, &bluetooth, sizeof bluetooth,
	    0);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not configure bluetooth coexistence\n");
		return error;
	}

	/* configure adapter */
	memset(&sc->config, 0, sizeof (struct wpi_config));
	IEEE80211_ADDR_COPY(sc->config.myaddr, IF_LLADDR(ifp));
	/*set default channel*/
	sc->config.chan = htole16(ieee80211_chan2ieee(ic, ic->ic_curchan));
	sc->config.flags = htole32(WPI_CONFIG_TSF);
	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
		sc->config.flags |= htole32(WPI_CONFIG_AUTO |
		    WPI_CONFIG_24GHZ);
	}
	sc->config.filter = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
	case IEEE80211_M_WDS:	/* No know setup, use STA for now */
		sc->config.mode = WPI_MODE_STA;
		sc->config.filter |= htole32(WPI_FILTER_MULTICAST);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		sc->config.mode = WPI_MODE_IBSS;
		sc->config.filter |= htole32(WPI_FILTER_BEACON |
					     WPI_FILTER_MULTICAST);
		break;
	case IEEE80211_M_HOSTAP:
		sc->config.mode = WPI_MODE_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		sc->config.mode = WPI_MODE_MONITOR;
		sc->config.filter |= htole32(WPI_FILTER_MULTICAST |
			WPI_FILTER_CTL | WPI_FILTER_PROMISC);
		break;
	default:
		device_printf(sc->sc_dev, "unknown opmode %d\n", ic->ic_opmode);
		return EINVAL;
	}
	sc->config.cck_mask  = 0x0f;	/* not yet negotiated */
	sc->config.ofdm_mask = 0xff;	/* not yet negotiated */
	error = wpi_cmd(sc, WPI_CMD_CONFIGURE, &sc->config,
		sizeof (struct wpi_config), 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "configure command failed\n");
		return error;
	}

	/* configuration has changed, set Tx power accordingly */
	if ((error = wpi_set_txpower(sc, ic->ic_curchan, 0)) != 0) {
	    device_printf(sc->sc_dev, "could not set Tx power\n");
	    return error;
	}

	/* add broadcast node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.bssid, ifp->if_broadcastaddr);
	node.id = WPI_ID_BROADCAST;
	node.rate = wpi_plcp_signal(2);
	error = wpi_cmd(sc, WPI_CMD_ADD_NODE, &node, sizeof node, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not add broadcast node\n");
		return error;
	}

	/* Setup rate scalling */
	error = wpi_mrr_setup(sc);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not setup MRR\n");
		return error;
	}

	return 0;
}

static void
wpi_stop_master(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	DPRINTFN(WPI_DEBUG_HW,("Disabling Firmware execution\n"));

	tmp = WPI_READ(sc, WPI_RESET);
	WPI_WRITE(sc, WPI_RESET, tmp | WPI_STOP_MASTER | WPI_NEVO_RESET);

	tmp = WPI_READ(sc, WPI_GPIO_CTL);
	if ((tmp & WPI_GPIO_PWR_STATUS) == WPI_GPIO_PWR_SLEEP)
		return;	/* already asleep */

	for (ntries = 0; ntries < 100; ntries++) {
		if (WPI_READ(sc, WPI_RESET) & WPI_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev, "timeout waiting for master\n");
	}
}

static int
wpi_power_up(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	wpi_mem_lock(sc);
	tmp = wpi_mem_read(sc, WPI_MEM_POWER);
	wpi_mem_write(sc, WPI_MEM_POWER, tmp & ~0x03000000);
	wpi_mem_unlock(sc);

	for (ntries = 0; ntries < 5000; ntries++) {
		if (WPI_READ(sc, WPI_GPIO_STATUS) & WPI_POWERED)
			break;
		DELAY(10);
	}
	if (ntries == 5000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for NIC to power up\n");
		return ETIMEDOUT;
	}
	return 0;
}

static int
wpi_reset(struct wpi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	DPRINTFN(WPI_DEBUG_HW,
	    ("Resetting the card - clearing any uploaded firmware\n"));

	/* clear any pending interrupts */
	WPI_WRITE(sc, WPI_INTR, 0xffffffff);

	tmp = WPI_READ(sc, WPI_PLL_CTL);
	WPI_WRITE(sc, WPI_PLL_CTL, tmp | WPI_PLL_INIT);

	tmp = WPI_READ(sc, WPI_CHICKEN);
	WPI_WRITE(sc, WPI_CHICKEN, tmp | WPI_CHICKEN_RXNOLOS);

	tmp = WPI_READ(sc, WPI_GPIO_CTL);
	WPI_WRITE(sc, WPI_GPIO_CTL, tmp | WPI_GPIO_INIT);

	/* wait for clock stabilization */
	for (ntries = 0; ntries < 25000; ntries++) {
		if (WPI_READ(sc, WPI_GPIO_CTL) & WPI_GPIO_CLOCK)
			break;
		DELAY(10);
	}
	if (ntries == 25000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for clock stabilization\n");
		return ETIMEDOUT;
	}

	/* initialize EEPROM */
	tmp = WPI_READ(sc, WPI_EEPROM_STATUS);

	if ((tmp & WPI_EEPROM_VERSION) == 0) {
		device_printf(sc->sc_dev, "EEPROM not found\n");
		return EIO;
	}
	WPI_WRITE(sc, WPI_EEPROM_STATUS, tmp & ~WPI_EEPROM_LOCKED);

	return 0;
}

static void
wpi_hw_config(struct wpi_softc *sc)
{
	uint32_t rev, hw;

	/* voodoo from the Linux "driver".. */
	hw = WPI_READ(sc, WPI_HWCONFIG);

	rev = pci_read_config(sc->sc_dev, PCIR_REVID, 1);
	if ((rev & 0xc0) == 0x40)
		hw |= WPI_HW_ALM_MB;
	else if (!(rev & 0x80))
		hw |= WPI_HW_ALM_MM;

	if (sc->cap == 0x80)
		hw |= WPI_HW_SKU_MRC;

	hw &= ~WPI_HW_REV_D;
	if ((le16toh(sc->rev) & 0xf0) == 0xd0)
		hw |= WPI_HW_REV_D;

	if (sc->type > 1)
		hw |= WPI_HW_TYPE_B;

	WPI_WRITE(sc, WPI_HWCONFIG, hw);
}

static void
wpi_rfkill_resume(struct wpi_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	int ntries;

	/* enable firmware again */
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_DISABLE_CMD);

	/* wait for thermal sensors to calibrate */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((sc->temp = (int)WPI_READ(sc, WPI_TEMPERATURE)) != 0)
			break;
		DELAY(10);
	}

	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for thermal calibration\n");
		return;
	}
	DPRINTFN(WPI_DEBUG_TEMP,("temperature %d\n", sc->temp));

	if (wpi_config(sc) != 0) {
		device_printf(sc->sc_dev, "device config failed\n");
		return;
	}

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->flags &= ~WPI_FLAG_HW_RADIO_OFF;

	if (vap != NULL) {
		if ((ic->ic_flags & IEEE80211_F_SCAN) == 0) {
			if (vap->iv_opmode != IEEE80211_M_MONITOR) {
				ieee80211_beacon_miss(ic);
				wpi_set_led(sc, WPI_LED_LINK, 0, 1);
			} else
				wpi_set_led(sc, WPI_LED_LINK, 5, 5);
		} else {
			ieee80211_scan_next(vap);
			wpi_set_led(sc, WPI_LED_LINK, 20, 2);
		}
	}

	callout_reset(&sc->watchdog_to, hz, wpi_watchdog, sc);
}

static void
wpi_init_locked(struct wpi_softc *sc, int force)
{
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t tmp;
	int ntries, qid;

	wpi_stop_locked(sc);
	(void)wpi_reset(sc);

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_CLOCK1, 0xa00);
	DELAY(20);
	tmp = wpi_mem_read(sc, WPI_MEM_PCIDEV);
	wpi_mem_write(sc, WPI_MEM_PCIDEV, tmp | 0x800);
	wpi_mem_unlock(sc);

	(void)wpi_power_up(sc);
	wpi_hw_config(sc);

	/* init Rx ring */
	wpi_mem_lock(sc);
	WPI_WRITE(sc, WPI_RX_BASE, sc->rxq.desc_dma.paddr);
	WPI_WRITE(sc, WPI_RX_RIDX_PTR, sc->shared_dma.paddr +
	    offsetof(struct wpi_shared, next));
	WPI_WRITE(sc, WPI_RX_WIDX, (WPI_RX_RING_COUNT - 1) & ~7);
	WPI_WRITE(sc, WPI_RX_CONFIG, 0xa9601010);
	wpi_mem_unlock(sc);

	/* init Tx rings */
	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_MODE, 2); /* bypass mode */
	wpi_mem_write(sc, WPI_MEM_RA, 1);   /* enable RA0 */
	wpi_mem_write(sc, WPI_MEM_TXCFG, 0x3f); /* enable all 6 Tx rings */
	wpi_mem_write(sc, WPI_MEM_BYPASS1, 0x10000);
	wpi_mem_write(sc, WPI_MEM_BYPASS2, 0x30002);
	wpi_mem_write(sc, WPI_MEM_MAGIC4, 4);
	wpi_mem_write(sc, WPI_MEM_MAGIC5, 5);

	WPI_WRITE(sc, WPI_TX_BASE_PTR, sc->shared_dma.paddr);
	WPI_WRITE(sc, WPI_MSG_CONFIG, 0xffff05a5);

	for (qid = 0; qid < 6; qid++) {
		WPI_WRITE(sc, WPI_TX_CTL(qid), 0);
		WPI_WRITE(sc, WPI_TX_BASE(qid), 0);
		WPI_WRITE(sc, WPI_TX_CONFIG(qid), 0x80200008);
	}
	wpi_mem_unlock(sc);

	/* clear "radio off" and "disable command" bits (reversed logic) */
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_DISABLE_CMD);
	sc->flags &= ~WPI_FLAG_HW_RADIO_OFF;

	/* clear any pending interrupts */
	WPI_WRITE(sc, WPI_INTR, 0xffffffff);

	/* enable interrupts */
	WPI_WRITE(sc, WPI_MASK, WPI_INTR_MASK);

	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);
	WPI_WRITE(sc, WPI_UCODE_CLR, WPI_RADIO_OFF);

	if ((wpi_load_firmware(sc)) != 0) {
	    device_printf(sc->sc_dev,
		"A problem occurred loading the firmware to the driver\n");
	    return;
	}

	/* At this point the firmware is up and running. If the hardware
	 * RF switch is turned off thermal calibration will fail, though
	 * the card is still happy to continue to accept commands, catch
	 * this case and schedule a task to watch for it to be turned on.
	 */
	wpi_mem_lock(sc);
	tmp = wpi_mem_read(sc, WPI_MEM_HW_RADIO_OFF);
	wpi_mem_unlock(sc);

	if (!(tmp & 0x1)) {
		sc->flags |= WPI_FLAG_HW_RADIO_OFF;
		device_printf(sc->sc_dev,"Radio Transmitter is switched off\n");
		goto out;
	}

	/* wait for thermal sensors to calibrate */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((sc->temp = (int)WPI_READ(sc, WPI_TEMPERATURE)) != 0)
			break;
		DELAY(10);
	}

	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for thermal sensors calibration\n");
		return;
	}
	DPRINTFN(WPI_DEBUG_TEMP,("temperature %d\n", sc->temp));

	if (wpi_config(sc) != 0) {
		device_printf(sc->sc_dev, "device config failed\n");
		return;
	}

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
out:
	callout_reset(&sc->watchdog_to, hz, wpi_watchdog, sc);
}

static void
wpi_init(void *arg)
{
	struct wpi_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	WPI_LOCK(sc);
	wpi_init_locked(sc, 0);
	WPI_UNLOCK(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		ieee80211_start_all(ic);		/* start all vaps */
}

static void
wpi_stop_locked(struct wpi_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t tmp;
	int ac;

	sc->sc_tx_timer = 0;
	sc->sc_scan_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->flags &= ~WPI_FLAG_HW_RADIO_OFF;
	callout_stop(&sc->watchdog_to);
	callout_stop(&sc->calib_to);

	/* disable interrupts */
	WPI_WRITE(sc, WPI_MASK, 0);
	WPI_WRITE(sc, WPI_INTR, WPI_INTR_MASK);
	WPI_WRITE(sc, WPI_INTR_STATUS, 0xff);
	WPI_WRITE(sc, WPI_INTR_STATUS, 0x00070000);

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_MODE, 0);
	wpi_mem_unlock(sc);

	/* reset all Tx rings */
	for (ac = 0; ac < 4; ac++)
		wpi_reset_tx_ring(sc, &sc->txq[ac]);
	wpi_reset_tx_ring(sc, &sc->cmdq);

	/* reset Rx ring */
	wpi_reset_rx_ring(sc, &sc->rxq);

	wpi_mem_lock(sc);
	wpi_mem_write(sc, WPI_MEM_CLOCK2, 0x200);
	wpi_mem_unlock(sc);

	DELAY(5);

	wpi_stop_master(sc);

	tmp = WPI_READ(sc, WPI_RESET);
	WPI_WRITE(sc, WPI_RESET, tmp | WPI_SW_RESET);
	sc->flags &= ~WPI_FLAG_BUSY;
}

static void
wpi_stop(struct wpi_softc *sc)
{
	WPI_LOCK(sc);
	wpi_stop_locked(sc);
	WPI_UNLOCK(sc);
}

static void
wpi_calib_timeout(void *arg)
{
	struct wpi_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	int temp;

	if (vap->iv_state != IEEE80211_S_RUN)
		return;

	/* update sensor data */
	temp = (int)WPI_READ(sc, WPI_TEMPERATURE);
	DPRINTFN(WPI_DEBUG_TEMP,("Temp in calibration is: %d\n", temp));

	wpi_power_calibration(sc, temp);

	callout_reset(&sc->calib_to, 60*hz, wpi_calib_timeout, sc);
}

/*
 * This function is called periodically (every 60 seconds) to adjust output
 * power to temperature changes.
 */
static void
wpi_power_calibration(struct wpi_softc *sc, int temp)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	/* sanity-check read value */
	if (temp < -260 || temp > 25) {
		/* this can't be correct, ignore */
		DPRINTFN(WPI_DEBUG_TEMP,
		    ("out-of-range temperature reported: %d\n", temp));
		return;
	}

	DPRINTFN(WPI_DEBUG_TEMP,("temperature %d->%d\n", sc->temp, temp));

	/* adjust Tx power if need be */
	if (abs(temp - sc->temp) <= 6)
		return;

	sc->temp = temp;

	if (wpi_set_txpower(sc, vap->iv_bss->ni_chan, 1) != 0) {
		/* just warn, too bad for the automatic calibration... */
		device_printf(sc->sc_dev,"could not adjust Tx power\n");
	}
}

/**
 * Read the eeprom to find out what channels are valid for the given
 * band and update net80211 with what we find.
 */
static void
wpi_read_eeprom_channels(struct wpi_softc *sc, int n)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	const struct wpi_chan_band *band = &wpi_bands[n];
	struct wpi_eeprom_chan channels[WPI_MAX_CHAN_PER_BAND];
	struct ieee80211_channel *c;
	int chan, i, passive;

	wpi_read_prom_data(sc, band->addr, channels,
	    band->nchan * sizeof (struct wpi_eeprom_chan));

	for (i = 0; i < band->nchan; i++) {
		if (!(channels[i].flags & WPI_EEPROM_CHAN_VALID)) {
			DPRINTFN(WPI_DEBUG_HW,
			    ("Channel Not Valid: %d, band %d\n",
			     band->chan[i],n));
			continue;
		}

		passive = 0;
		chan = band->chan[i];
		c = &ic->ic_channels[ic->ic_nchans++];

		/* is active scan allowed on this channel? */
		if (!(channels[i].flags & WPI_EEPROM_CHAN_ACTIVE)) {
			passive = IEEE80211_CHAN_PASSIVE;
		}

		if (n == 0) {	/* 2GHz band */
			c->ic_ieee = chan;
			c->ic_freq = ieee80211_ieee2mhz(chan,
			    IEEE80211_CHAN_2GHZ);
			c->ic_flags = IEEE80211_CHAN_B | passive;

			c = &ic->ic_channels[ic->ic_nchans++];
			c->ic_ieee = chan;
			c->ic_freq = ieee80211_ieee2mhz(chan,
			    IEEE80211_CHAN_2GHZ);
			c->ic_flags = IEEE80211_CHAN_G | passive;

		} else {	/* 5GHz band */
			/*
			 * Some 3945ABG adapters support channels 7, 8, 11
			 * and 12 in the 2GHz *and* 5GHz bands.
			 * Because of limitations in our net80211(9) stack,
			 * we can't support these channels in 5GHz band.
			 * XXX not true; just need to map to proper frequency
			 */
			if (chan <= 14)
				continue;

			c->ic_ieee = chan;
			c->ic_freq = ieee80211_ieee2mhz(chan,
			    IEEE80211_CHAN_5GHZ);
			c->ic_flags = IEEE80211_CHAN_A | passive;
		}

		/* save maximum allowed power for this channel */
		sc->maxpwr[chan] = channels[i].maxpwr;

#if 0
		// XXX We can probably use this an get rid of maxpwr - ben 20070617
		ic->ic_channels[chan].ic_maxpower = channels[i].maxpwr;
		//ic->ic_channels[chan].ic_minpower...
		//ic->ic_channels[chan].ic_maxregtxpower...
#endif

		DPRINTF(("adding chan %d (%dMHz) flags=0x%x maxpwr=%d"
		    " passive=%d, offset %d\n", chan, c->ic_freq,
		    channels[i].flags, sc->maxpwr[chan],
		    (c->ic_flags & IEEE80211_CHAN_PASSIVE) != 0,
		    ic->ic_nchans));
	}
}

static void
wpi_read_eeprom_group(struct wpi_softc *sc, int n)
{
	struct wpi_power_group *group = &sc->groups[n];
	struct wpi_eeprom_group rgroup;
	int i;

	wpi_read_prom_data(sc, WPI_EEPROM_POWER_GRP + n * 32, &rgroup,
	    sizeof rgroup);

	/* save power group information */
	group->chan   = rgroup.chan;
	group->maxpwr = rgroup.maxpwr;
	/* temperature at which the samples were taken */
	group->temp   = (int16_t)le16toh(rgroup.temp);

	DPRINTF(("power group %d: chan=%d maxpwr=%d temp=%d\n", n,
		    group->chan, group->maxpwr, group->temp));

	for (i = 0; i < WPI_SAMPLES_COUNT; i++) {
		group->samples[i].index = rgroup.samples[i].index;
		group->samples[i].power = rgroup.samples[i].power;

		DPRINTF(("\tsample %d: index=%d power=%d\n", i,
			    group->samples[i].index, group->samples[i].power));
	}
}

/*
 * Update Tx power to match what is defined for channel `c'.
 */
static int
wpi_set_txpower(struct wpi_softc *sc, struct ieee80211_channel *c, int async)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct wpi_power_group *group;
	struct wpi_cmd_txpower txpower;
	u_int chan;
	int i;

	/* get channel number */
	chan = ieee80211_chan2ieee(ic, c);

	/* find the power group to which this channel belongs */
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		for (group = &sc->groups[1]; group < &sc->groups[4]; group++)
			if (chan <= group->chan)
				break;
	} else
		group = &sc->groups[0];

	memset(&txpower, 0, sizeof txpower);
	txpower.band = IEEE80211_IS_CHAN_5GHZ(c) ? 0 : 1;
	txpower.channel = htole16(chan);

	/* set Tx power for all OFDM and CCK rates */
	for (i = 0; i <= 11 ; i++) {
		/* retrieve Tx power for this channel/rate combination */
		int idx = wpi_get_power_index(sc, group, c,
		    wpi_ridx_to_rate[i]);

		txpower.rates[i].rate = wpi_ridx_to_plcp[i];

		if (IEEE80211_IS_CHAN_5GHZ(c)) {
			txpower.rates[i].gain_radio = wpi_rf_gain_5ghz[idx];
			txpower.rates[i].gain_dsp = wpi_dsp_gain_5ghz[idx];
		} else {
			txpower.rates[i].gain_radio = wpi_rf_gain_2ghz[idx];
			txpower.rates[i].gain_dsp = wpi_dsp_gain_2ghz[idx];
		}
		DPRINTFN(WPI_DEBUG_TEMP,("chan %d/rate %d: power index %d\n",
			    chan, wpi_ridx_to_rate[i], idx));
	}

	return wpi_cmd(sc, WPI_CMD_TXPOWER, &txpower, sizeof txpower, async);
}

/*
 * Determine Tx power index for a given channel/rate combination.
 * This takes into account the regulatory information from EEPROM and the
 * current temperature.
 */
static int
wpi_get_power_index(struct wpi_softc *sc, struct wpi_power_group *group,
    struct ieee80211_channel *c, int rate)
{
/* fixed-point arithmetic division using a n-bit fractional part */
#define fdivround(a, b, n)      \
	((((1 << n) * (a)) / (b) + (1 << n) / 2) / (1 << n))

/* linear interpolation */
#define interpolate(x, x1, y1, x2, y2, n)       \
	((y1) + fdivround(((x) - (x1)) * ((y2) - (y1)), (x2) - (x1), n))

	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct wpi_power_sample *sample;
	int pwr, idx;
	u_int chan;

	/* get channel number */
	chan = ieee80211_chan2ieee(ic, c);

	/* default power is group's maximum power - 3dB */
	pwr = group->maxpwr / 2;

	/* decrease power for highest OFDM rates to reduce distortion */
	switch (rate) {
		case 72:	/* 36Mb/s */
			pwr -= IEEE80211_IS_CHAN_2GHZ(c) ? 0 :  5;
			break;
		case 96:	/* 48Mb/s */
			pwr -= IEEE80211_IS_CHAN_2GHZ(c) ? 7 : 10;
			break;
		case 108:	/* 54Mb/s */
			pwr -= IEEE80211_IS_CHAN_2GHZ(c) ? 9 : 12;
			break;
	}

	/* never exceed channel's maximum allowed Tx power */
	pwr = min(pwr, sc->maxpwr[chan]);

	/* retrieve power index into gain tables from samples */
	for (sample = group->samples; sample < &group->samples[3]; sample++)
		if (pwr > sample[1].power)
			break;
	/* fixed-point linear interpolation using a 19-bit fractional part */
	idx = interpolate(pwr, sample[0].power, sample[0].index,
	    sample[1].power, sample[1].index, 19);

	/*
	 *  Adjust power index based on current temperature
	 *	- if colder than factory-calibrated: decreate output power
	 *	- if warmer than factory-calibrated: increase output power
	 */
	idx -= (sc->temp - group->temp) * 11 / 100;

	/* decrease power for CCK rates (-5dB) */
	if (!WPI_RATE_IS_OFDM(rate))
		idx += 10;

	/* keep power index in a valid range */
	if (idx < 0)
		return 0;
	if (idx > WPI_MAX_PWR_INDEX)
		return WPI_MAX_PWR_INDEX;
	return idx;

#undef interpolate
#undef fdivround
}

/**
 * Called by net80211 framework to indicate that a scan
 * is starting. This function doesn't actually do the scan,
 * wpi_scan_curchan starts things off. This function is more
 * of an early warning from the framework we should get ready
 * for the scan.
 */
static void
wpi_scan_start(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct wpi_softc *sc = ifp->if_softc;

	WPI_LOCK(sc);
	wpi_set_led(sc, WPI_LED_LINK, 20, 2);
	WPI_UNLOCK(sc);
}

/**
 * Called by the net80211 framework, indicates that the
 * scan has ended. If there is a scan in progress on the card
 * then it should be aborted.
 */
static void
wpi_scan_end(struct ieee80211com *ic)
{
	/* XXX ignore */
}

/**
 * Called by the net80211 framework to indicate to the driver
 * that the channel should be changed
 */
static void
wpi_set_channel(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct wpi_softc *sc = ifp->if_softc;
	int error;

	/*
	 * Only need to set the channel in Monitor mode. AP scanning and auth
	 * are already taken care of by their respective firmware commands.
	 */
	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		WPI_LOCK(sc);
		error = wpi_config(sc);
		WPI_UNLOCK(sc);
		if (error != 0)
			device_printf(sc->sc_dev,
			    "error %d settting channel\n", error);
	}
}

/**
 * Called by net80211 to indicate that we need to scan the current
 * channel. The channel is previously be set via the wpi_set_channel
 * callback.
 */
static void
wpi_scan_curchan(struct ieee80211_scan_state *ss, unsigned long maxdwell)
{
	struct ieee80211vap *vap = ss->ss_vap;
	struct ifnet *ifp = vap->iv_ic->ic_ifp;
	struct wpi_softc *sc = ifp->if_softc;

	WPI_LOCK(sc);
	if (wpi_scan(sc))
		ieee80211_cancel_scan(vap);
	WPI_UNLOCK(sc);
}

/**
 * Called by the net80211 framework to indicate
 * the minimum dwell time has been met, terminate the scan.
 * We don't actually terminate the scan as the firmware will notify
 * us when it's finished and we have no way to interrupt it.
 */
static void
wpi_scan_mindwell(struct ieee80211_scan_state *ss)
{
	/* NB: don't try to abort scan; wait for firmware to finish */
}

static void
wpi_hwreset(void *arg, int pending)
{
	struct wpi_softc *sc = arg;

	WPI_LOCK(sc);
	wpi_init_locked(sc, 0);
	WPI_UNLOCK(sc);
}

static void
wpi_rfreset(void *arg, int pending)
{
	struct wpi_softc *sc = arg;

	WPI_LOCK(sc);
	wpi_rfkill_resume(sc);
	WPI_UNLOCK(sc);
}

/*
 * Allocate DMA-safe memory for firmware transfer.
 */
static int
wpi_alloc_fwmem(struct wpi_softc *sc)
{
	/* allocate enough contiguous space to store text and data */
	return wpi_dma_contig_alloc(sc, &sc->fw_dma, NULL,
	    WPI_FW_MAIN_TEXT_MAXSZ + WPI_FW_MAIN_DATA_MAXSZ, 1,
	    BUS_DMA_NOWAIT);
}

static void
wpi_free_fwmem(struct wpi_softc *sc)
{
	wpi_dma_contig_free(&sc->fw_dma);
}

/**
 * Called every second, wpi_watchdog used by the watch dog timer
 * to check that the card is still alive
 */
static void
wpi_watchdog(void *arg)
{
	struct wpi_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint32_t tmp;

	DPRINTFN(WPI_DEBUG_WATCHDOG,("Watchdog: tick\n"));

	if (sc->flags & WPI_FLAG_HW_RADIO_OFF) {
		/* No need to lock firmware memory */
		tmp = wpi_mem_read(sc, WPI_MEM_HW_RADIO_OFF);

		if ((tmp & 0x1) == 0) {
			/* Radio kill switch is still off */
			callout_reset(&sc->watchdog_to, hz, wpi_watchdog, sc);
			return;
		}

		device_printf(sc->sc_dev, "Hardware Switch Enabled\n");
		ieee80211_runtask(ic, &sc->sc_radiotask);
		return;
	}

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			device_printf(sc->sc_dev,"device timeout\n");
			ifp->if_oerrors++;
			ieee80211_runtask(ic, &sc->sc_restarttask);
		}
	}
	if (sc->sc_scan_timer > 0) {
		struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
		if (--sc->sc_scan_timer == 0 && vap != NULL) {
			device_printf(sc->sc_dev,"scan timeout\n");
			ieee80211_cancel_scan(vap);
			ieee80211_runtask(ic, &sc->sc_restarttask);
		}
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		callout_reset(&sc->watchdog_to, hz, wpi_watchdog, sc);
}

#ifdef WPI_DEBUG
static const char *wpi_cmd_str(int cmd)
{
	switch (cmd) {
	case WPI_DISABLE_CMD:	return "WPI_DISABLE_CMD";
	case WPI_CMD_CONFIGURE:	return "WPI_CMD_CONFIGURE";
	case WPI_CMD_ASSOCIATE:	return "WPI_CMD_ASSOCIATE";
	case WPI_CMD_SET_WME:	return "WPI_CMD_SET_WME";
	case WPI_CMD_TSF:	return "WPI_CMD_TSF";
	case WPI_CMD_ADD_NODE:	return "WPI_CMD_ADD_NODE";
	case WPI_CMD_TX_DATA:	return "WPI_CMD_TX_DATA";
	case WPI_CMD_MRR_SETUP:	return "WPI_CMD_MRR_SETUP";
	case WPI_CMD_SET_LED:	return "WPI_CMD_SET_LED";
	case WPI_CMD_SET_POWER_MODE: return "WPI_CMD_SET_POWER_MODE";
	case WPI_CMD_SCAN:	return "WPI_CMD_SCAN";
	case WPI_CMD_SET_BEACON:return "WPI_CMD_SET_BEACON";
	case WPI_CMD_TXPOWER:	return "WPI_CMD_TXPOWER";
	case WPI_CMD_BLUETOOTH:	return "WPI_CMD_BLUETOOTH";

	default:
		KASSERT(1, ("Unknown Command: %d\n", cmd));
		return "UNKNOWN CMD";	/* Make the compiler happy */
	}
}
#endif

MODULE_DEPEND(wpi, pci,  1, 1, 1);
MODULE_DEPEND(wpi, wlan, 1, 1, 1);
MODULE_DEPEND(wpi, firmware, 1, 1, 1);
