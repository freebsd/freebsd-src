/*-
 * Copyright (c) 2004, 2005
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 * Copyright (c) 2005-2006 Sam Leffler, Errno Consulting
 * Copyright (c) 2007 Andrew Thompson <thompsa@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/iwi/if_iwi.c,v 1.57.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $");

/*-
 * Intel(R) PRO/Wireless 2200BG/2225BG/2915ABG driver
 * http://www.intel.com/network/connectivity/products/wireless/prowireless_mobile.htm
 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <dev/iwi/if_iwireg.h>
#include <dev/iwi/if_iwivar.h>

#define IWI_DEBUG
#ifdef IWI_DEBUG
#define DPRINTF(x)	do { if (iwi_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (iwi_debug >= (n)) printf x; } while (0)
int iwi_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, iwi, CTLFLAG_RW, &iwi_debug, 0, "iwi debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

MODULE_DEPEND(iwi, pci,  1, 1, 1);
MODULE_DEPEND(iwi, wlan, 1, 1, 1);
MODULE_DEPEND(iwi, firmware, 1, 1, 1);

enum {
	IWI_LED_TX,
	IWI_LED_RX,
	IWI_LED_POLL,
};

struct iwi_ident {
	uint16_t	vendor;
	uint16_t	device;
	const char	*name;
};

static const struct iwi_ident iwi_ident_table[] = {
	{ 0x8086, 0x4220, "Intel(R) PRO/Wireless 2200BG" },
	{ 0x8086, 0x4221, "Intel(R) PRO/Wireless 2225BG" },
	{ 0x8086, 0x4223, "Intel(R) PRO/Wireless 2915ABG" },
	{ 0x8086, 0x4224, "Intel(R) PRO/Wireless 2915ABG" },

	{ 0, 0, NULL }
};

static void	iwi_dma_map_addr(void *, bus_dma_segment_t *, int, int);
static int	iwi_alloc_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *,
		    int);
static void	iwi_reset_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *);
static void	iwi_free_cmd_ring(struct iwi_softc *, struct iwi_cmd_ring *);
static int	iwi_alloc_tx_ring(struct iwi_softc *, struct iwi_tx_ring *,
		    int, bus_addr_t, bus_addr_t);
static void	iwi_reset_tx_ring(struct iwi_softc *, struct iwi_tx_ring *);
static void	iwi_free_tx_ring(struct iwi_softc *, struct iwi_tx_ring *);
static int	iwi_alloc_rx_ring(struct iwi_softc *, struct iwi_rx_ring *,
		    int);
static void	iwi_reset_rx_ring(struct iwi_softc *, struct iwi_rx_ring *);
static void	iwi_free_rx_ring(struct iwi_softc *, struct iwi_rx_ring *);
static struct	ieee80211_node *iwi_node_alloc(struct ieee80211_node_table *);
static void	iwi_node_free(struct ieee80211_node *);
static int	iwi_media_change(struct ifnet *);
static void	iwi_media_status(struct ifnet *, struct ifmediareq *);
static int	iwi_newstate(struct ieee80211com *, enum ieee80211_state, int);
static void	iwi_wme_init(struct iwi_softc *);
static int	iwi_wme_setparams(struct iwi_softc *);
static int	iwi_wme_update(struct ieee80211com *);
static uint16_t	iwi_read_prom_word(struct iwi_softc *, uint8_t);
static void	iwi_frame_intr(struct iwi_softc *, struct iwi_rx_data *, int,
		    struct iwi_frame *);
static void	iwi_notification_intr(struct iwi_softc *, struct iwi_notif *);
static void	iwi_rx_intr(struct iwi_softc *);
static void	iwi_tx_intr(struct iwi_softc *, struct iwi_tx_ring *);
static void	iwi_intr(void *);
static int	iwi_cmd(struct iwi_softc *, uint8_t, void *, uint8_t);
static void	iwi_write_ibssnode(struct iwi_softc *, const u_int8_t [], int);
static int	iwi_tx_start(struct ifnet *, struct mbuf *,
		    struct ieee80211_node *, int);
static void	iwi_start(struct ifnet *);
static void	iwi_watchdog(void *);
static int	iwi_ioctl(struct ifnet *, u_long, caddr_t);
static void	iwi_stop_master(struct iwi_softc *);
static int	iwi_reset(struct iwi_softc *);
static int	iwi_load_ucode(struct iwi_softc *, const struct iwi_fw *);
static int	iwi_load_firmware(struct iwi_softc *, const struct iwi_fw *);
static void	iwi_release_fw_dma(struct iwi_softc *sc);
static int	iwi_config(struct iwi_softc *);
static int	iwi_get_firmware(struct iwi_softc *);
static void	iwi_put_firmware(struct iwi_softc *);
static int	iwi_scanchan(struct iwi_softc *, unsigned long, int);
static void	iwi_scan_start(struct ieee80211com *);
static void	iwi_scan_end(struct ieee80211com *);
static void	iwi_scanabort(void *, int);
static void	iwi_set_channel(struct ieee80211com *);
static void	iwi_scan_curchan(struct ieee80211com *, unsigned long maxdwell);
#if 0
static void	iwi_scan_allchan(struct ieee80211com *, unsigned long maxdwell);
#endif
static void	iwi_scan_mindwell(struct ieee80211com *);
static void     iwi_assoc(struct ieee80211com *ic);
static void     iwi_disassoc(struct ieee80211com *);
static void	iwi_ops(void *, int);
static int	iwi_queue_cmd(struct iwi_softc *, int);
static int	iwi_auth_and_assoc(struct iwi_softc *);
static int	iwi_disassociate(struct iwi_softc *, int quiet);
static void	iwi_init(void *);
static void	iwi_init_locked(void *, int);
static void	iwi_stop(void *);
static void	iwi_restart(void *, int);
static int	iwi_getrfkill(struct iwi_softc *);
static void	iwi_radio_on(void *, int);
static void	iwi_radio_off(void *, int);
static void	iwi_sysctlattach(struct iwi_softc *);
static void	iwi_led_event(struct iwi_softc *, int);
static void	iwi_ledattach(struct iwi_softc *);

static int iwi_probe(device_t);
static int iwi_attach(device_t);
static int iwi_detach(device_t);
static int iwi_shutdown(device_t);
static int iwi_suspend(device_t);
static int iwi_resume(device_t);

static device_method_t iwi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		iwi_probe),
	DEVMETHOD(device_attach,	iwi_attach),
	DEVMETHOD(device_detach,	iwi_detach),
	DEVMETHOD(device_shutdown,	iwi_shutdown),
	DEVMETHOD(device_suspend,	iwi_suspend),
	DEVMETHOD(device_resume,	iwi_resume),

	{ 0, 0 }
};

static driver_t iwi_driver = {
	"iwi",
	iwi_methods,
	sizeof (struct iwi_softc)
};

static devclass_t iwi_devclass;

DRIVER_MODULE(iwi, pci, iwi_driver, iwi_devclass, 0, 0);

static __inline uint8_t
MEM_READ_1(struct iwi_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IWI_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_1(sc, IWI_CSR_INDIRECT_DATA);
}

static __inline uint32_t
MEM_READ_4(struct iwi_softc *sc, uint32_t addr)
{
	CSR_WRITE_4(sc, IWI_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_4(sc, IWI_CSR_INDIRECT_DATA);
}

static int
iwi_probe(device_t dev)
{
	const struct iwi_ident *ident;

	for (ident = iwi_ident_table; ident->name != NULL; ident++) {
		if (pci_get_vendor(dev) == ident->vendor &&
		    pci_get_device(dev) == ident->device) {
			device_set_desc(dev, ident->name);
			return 0;
		}
	}
	return ENXIO;
}

/* Base Address Register */
#define IWI_PCI_BAR0	0x10

static int
iwi_attach(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t val;
	int i, error, bands;

	sc->sc_dev = dev;

	IWI_LOCK_INIT(sc);
	IWI_CMD_LOCK_INIT(sc);

	sc->sc_unr = new_unrhdr(1, IWI_MAX_IBSSNODE-1, &sc->sc_mtx);

#if __FreeBSD_version >= 700000
	sc->sc_tq = taskqueue_create("iwi_taskq", M_NOWAIT | M_ZERO,
		taskqueue_thread_enqueue, &sc->sc_tq);
	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET, "%s taskq",
		device_get_nameunit(dev));
	sc->sc_tq2 = taskqueue_create("iwi_taskq2", M_NOWAIT | M_ZERO,
		taskqueue_thread_enqueue, &sc->sc_tq2);
	taskqueue_start_threads(&sc->sc_tq2, 1, PI_NET, "%s taskq2",
		device_get_nameunit(dev));
#else
	sc->sc_tq = taskqueue_create("iwi_taskq", M_NOWAIT | M_ZERO,
		taskqueue_thread_enqueue, &sc->sc_tq, &sc->sc_tqproc);
	kthread_create(taskqueue_thread_loop, &sc->sc_tq, &sc->sc_tqproc,
		0, 0, "%s taskq", device_get_nameunit(dev));
	sc->sc_tq2 = taskqueue_create("iwi_taskq2", M_NOWAIT | M_ZERO,
		taskqueue_thread_enqueue, &sc->sc_tq2, &sc->sc_tqproc);
	kthread_create(taskqueue_thread_loop, &sc->sc_tq2, &sc->sc_tqproc,
		0, 0, "%s taskq2", device_get_nameunit(dev));
#endif
	TASK_INIT(&sc->sc_radiontask, 0, iwi_radio_on, sc);
	TASK_INIT(&sc->sc_radiofftask, 0, iwi_radio_off, sc);
	TASK_INIT(&sc->sc_restarttask, 0, iwi_restart, sc);
	TASK_INIT(&sc->sc_opstask, 0, iwi_ops, sc);
	TASK_INIT(&sc->sc_scanaborttask, 0, iwi_scanabort, sc);
	callout_init_mtx(&sc->sc_wdtimer, &sc->sc_mtx, 0);

	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		device_printf(dev, "chip is in D%d power mode "
		    "-- setting to D0\n", pci_get_powerstate(dev));
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	}

	pci_write_config(dev, 0x41, 0, 1);

	/* enable bus-mastering */
	pci_enable_busmaster(dev);

	sc->mem_rid = IWI_PCI_BAR0;
	sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
	    RF_ACTIVE);
	if (sc->mem == NULL) {
		device_printf(dev, "could not allocate memory resource\n");
		goto fail;
	}

	sc->sc_st = rman_get_bustag(sc->mem);
	sc->sc_sh = rman_get_bushandle(sc->mem);

	sc->irq_rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq == NULL) {
		device_printf(dev, "could not allocate interrupt resource\n");
		goto fail;
	}

	if (iwi_reset(sc) != 0) {
		device_printf(dev, "could not reset adapter\n");
		goto fail;
	}

	/*
	 * Allocate rings.
	 */
	if (iwi_alloc_cmd_ring(sc, &sc->cmdq, IWI_CMD_RING_COUNT) != 0) {
		device_printf(dev, "could not allocate Cmd ring\n");
		goto fail;
	}

	for (i = 0; i < 4; i++) {
		error = iwi_alloc_tx_ring(sc, &sc->txq[i], IWI_TX_RING_COUNT,
		    IWI_CSR_TX1_RIDX + i * 4,
		    IWI_CSR_TX1_WIDX + i * 4);
		if (error != 0) {
			device_printf(dev, "could not allocate Tx ring %d\n",
				i+i);
			goto fail;
		}
	}

	if (iwi_alloc_rx_ring(sc, &sc->rxq, IWI_RX_RING_COUNT) != 0) {
		device_printf(dev, "could not allocate Rx ring\n");
		goto fail;
	}

	iwi_wme_init(sc);

	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		goto fail;
	}
	ic->ic_ifp = ifp;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = iwi_init;
	ifp->if_ioctl = iwi_ioctl;
	ifp->if_start = iwi_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ic->ic_wme.wme_update = iwi_wme_update;
	ic->ic_phytype = IEEE80211_T_OFDM; /* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	      IEEE80211_C_IBSS		/* IBSS mode supported */
	    | IEEE80211_C_MONITOR	/* monitor mode supported */
	    | IEEE80211_C_PMGT		/* power save supported */
	    | IEEE80211_C_SHPREAMBLE	/* short preamble supported */
	    | IEEE80211_C_WPA		/* 802.11i */
	    | IEEE80211_C_WME		/* 802.11e */
	    | IEEE80211_C_BGSCAN	/* capable of bg scanning */
	    ;

	/* read MAC address from EEPROM */
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 0);
	ic->ic_myaddr[0] = val & 0xff;
	ic->ic_myaddr[1] = val >> 8;
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 1);
	ic->ic_myaddr[2] = val & 0xff;
	ic->ic_myaddr[3] = val >> 8;
	val = iwi_read_prom_word(sc, IWI_EEPROM_MAC + 2);
	ic->ic_myaddr[4] = val & 0xff;
	ic->ic_myaddr[5] = val >> 8;
	
	bands = 0;
	setbit(&bands, IEEE80211_MODE_11B);
	setbit(&bands, IEEE80211_MODE_11G);
	if (pci_get_device(dev) >= 0x4223) 
		setbit(&bands, IEEE80211_MODE_11A);
	ieee80211_init_channels(ic, 0, CTRY_DEFAULT, bands, 0, 1);

	ieee80211_ifattach(ic);
	ic->ic_bmissthreshold = 10;		/* override default */
	/* override default methods */
	ic->ic_node_alloc = iwi_node_alloc;
	sc->sc_node_free = ic->ic_node_free;
	ic->ic_node_free = iwi_node_free;
	ic->ic_scan_start = iwi_scan_start;
	ic->ic_scan_end = iwi_scan_end;
	ic->ic_set_channel = iwi_set_channel;
	ic->ic_scan_curchan = iwi_scan_curchan;
	ic->ic_scan_mindwell = iwi_scan_mindwell;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = iwi_newstate;
	ieee80211_media_init(ic, iwi_media_change, iwi_media_status);

	bpfattach2(ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + sizeof (sc->sc_txtap),
	    &sc->sc_drvbpf);

	sc->sc_rxtap_len = sizeof sc->sc_rxtap;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IWI_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtap;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IWI_TX_RADIOTAP_PRESENT);

	iwi_sysctlattach(sc);
	iwi_ledattach(sc);

	/*
	 * Hook our interrupt after all initialization is complete.
	 */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, iwi_intr, sc, &sc->sc_ih);
	if (error != 0) {
		device_printf(dev, "could not set up interrupt\n");
		goto fail;
	}

	if (bootverbose)
		ieee80211_announce(ic);

	return 0;

fail:	iwi_detach(dev);
	return ENXIO;
}

static int
iwi_detach(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	IWI_LOCK_DECL;

	if (ifp != NULL) {
		IWI_LOCK(sc);
		iwi_stop(sc);
		IWI_UNLOCK(sc);
		bpfdetach(ifp);
		ieee80211_ifdetach(ic);
	}

	callout_drain(&sc->sc_wdtimer);
	iwi_put_firmware(sc);
	iwi_release_fw_dma(sc);

	iwi_free_cmd_ring(sc, &sc->cmdq);
	iwi_free_tx_ring(sc, &sc->txq[0]);
	iwi_free_tx_ring(sc, &sc->txq[1]);
	iwi_free_tx_ring(sc, &sc->txq[2]);
	iwi_free_tx_ring(sc, &sc->txq[3]);
	iwi_free_rx_ring(sc, &sc->rxq);

	if (sc->irq != NULL) {
		bus_teardown_intr(dev, sc->irq, sc->sc_ih);
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);
	}

	if (sc->mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, sc->mem);

	if (ifp != NULL)
		if_free(ifp);

	taskqueue_free(sc->sc_tq);
	taskqueue_free(sc->sc_tq2);

	if (sc->sc_unr != NULL)
		delete_unrhdr(sc->sc_unr);

	IWI_LOCK_DESTROY(sc);
	IWI_CMD_LOCK_DESTROY(sc);

	return 0;
}

static void
iwi_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error != 0)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));

	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
iwi_alloc_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring, int count)
{
	int error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = 0;

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 4, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    count * IWI_CMD_DESC_SIZE, 1, count * IWI_CMD_DESC_SIZE, 0, 
	    NULL, NULL, &ring->desc_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create desc DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(ring->desc_dmat, (void **)&ring->desc,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &ring->desc_map);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(ring->desc_dmat, ring->desc_map, ring->desc,
	    count * IWI_CMD_DESC_SIZE, iwi_dma_map_addr, &ring->physaddr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	return 0;

fail:	iwi_free_cmd_ring(sc, ring);
	return error;
}

static void
iwi_reset_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring)
{
	ring->queued = 0;
	ring->cur = ring->next = 0;
}

static void
iwi_free_cmd_ring(struct iwi_softc *sc, struct iwi_cmd_ring *ring)
{
	if (ring->desc != NULL) {
		bus_dmamap_sync(ring->desc_dmat, ring->desc_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->desc_dmat, ring->desc_map);
		bus_dmamem_free(ring->desc_dmat, ring->desc, ring->desc_map);
	}

	if (ring->desc_dmat != NULL)
		bus_dma_tag_destroy(ring->desc_dmat);	
}

static int
iwi_alloc_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring, int count,
    bus_addr_t csr_ridx, bus_addr_t csr_widx)
{
	int i, error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = 0;
	ring->csr_ridx = csr_ridx;
	ring->csr_widx = csr_widx;

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 4, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    count * IWI_TX_DESC_SIZE, 1, count * IWI_TX_DESC_SIZE, 0, NULL, 
	    NULL, &ring->desc_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create desc DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(ring->desc_dmat, (void **)&ring->desc,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &ring->desc_map);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not allocate DMA memory\n");
		goto fail;
	}

	error = bus_dmamap_load(ring->desc_dmat, ring->desc_map, ring->desc,
	    count * IWI_TX_DESC_SIZE, iwi_dma_map_addr, &ring->physaddr, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not load desc DMA map\n");
		goto fail;
	}

	ring->data = malloc(count * sizeof (struct iwi_tx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		device_printf(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0,
	BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	IWI_MAX_NSEG, MCLBYTES, 0, NULL, NULL, &ring->data_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create data DMA tag\n");
		goto fail;
	}

	for (i = 0; i < count; i++) {
		error = bus_dmamap_create(ring->data_dmat, 0,
		    &ring->data[i].map);
		if (error != 0) {
			device_printf(sc->sc_dev, "could not create DMA map\n");
			goto fail;
		}
	}

	return 0;

fail:	iwi_free_tx_ring(sc, ring);
	return error;
}

static void
iwi_reset_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring)
{
	struct iwi_tx_data *data;
	int i;

	for (i = 0; i < ring->count; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->data_dmat, data->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->data_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}

		if (data->ni != NULL) {
			ieee80211_free_node(data->ni);
			data->ni = NULL;
		}
	}

	ring->queued = 0;
	ring->cur = ring->next = 0;
}

static void
iwi_free_tx_ring(struct iwi_softc *sc, struct iwi_tx_ring *ring)
{
	struct iwi_tx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(ring->desc_dmat, ring->desc_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->desc_dmat, ring->desc_map);
		bus_dmamem_free(ring->desc_dmat, ring->desc, ring->desc_map);
	}

	if (ring->desc_dmat != NULL)
		bus_dma_tag_destroy(ring->desc_dmat);

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(ring->data_dmat, data->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(ring->data_dmat, data->map);
				m_freem(data->m);
			}

			if (data->ni != NULL)
				ieee80211_free_node(data->ni);

			if (data->map != NULL)
				bus_dmamap_destroy(ring->data_dmat, data->map);
		}

		free(ring->data, M_DEVBUF);
	}

	if (ring->data_dmat != NULL)
		bus_dma_tag_destroy(ring->data_dmat);
}

static int
iwi_alloc_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring, int count)
{
	struct iwi_rx_data *data;
	int i, error;

	ring->count = count;
	ring->cur = 0;

	ring->data = malloc(count * sizeof (struct iwi_rx_data), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ring->data == NULL) {
		device_printf(sc->sc_dev, "could not allocate soft data\n");
		error = ENOMEM;
		goto fail;
	}

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
	    1, MCLBYTES, 0, NULL, NULL, &ring->data_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create data DMA tag\n");
		goto fail;
	}

	for (i = 0; i < count; i++) {
		data = &ring->data[i];

		error = bus_dmamap_create(ring->data_dmat, 0, &data->map);
		if (error != 0) {
			device_printf(sc->sc_dev, "could not create DMA map\n");
			goto fail;
		}

		data->m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (data->m == NULL) {
			device_printf(sc->sc_dev,
			    "could not allocate rx mbuf\n");
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(ring->data_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, iwi_dma_map_addr,
		    &data->physaddr, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not load rx buf DMA map");
			goto fail;
		}

		data->reg = IWI_CSR_RX_BASE + i * 4;
	}

	return 0;

fail:	iwi_free_rx_ring(sc, ring);
	return error;
}

static void
iwi_reset_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring)
{
	ring->cur = 0;
}

static void
iwi_free_rx_ring(struct iwi_softc *sc, struct iwi_rx_ring *ring)
{
	struct iwi_rx_data *data;
	int i;

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(ring->data_dmat, data->map,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(ring->data_dmat, data->map);
				m_freem(data->m);
			}

			if (data->map != NULL)
				bus_dmamap_destroy(ring->data_dmat, data->map);
		}

		free(ring->data, M_DEVBUF);
	}

	if (ring->data_dmat != NULL)
		bus_dma_tag_destroy(ring->data_dmat);
}

static int
iwi_shutdown(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);
	IWI_LOCK_DECL;

	IWI_LOCK(sc);
	iwi_stop(sc);
	IWI_UNLOCK(sc);
	iwi_put_firmware(sc);		/* ??? XXX */

	return 0;
}

static int
iwi_suspend(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);
	IWI_LOCK_DECL;

	IWI_LOCK(sc);
	iwi_stop(sc);
	IWI_UNLOCK(sc);

	return 0;
}

static int
iwi_resume(device_t dev)
{
	struct iwi_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ic.ic_ifp;
	IWI_LOCK_DECL;

	IWI_LOCK(sc);

	pci_write_config(dev, 0x41, 0, 1);

	if (ifp->if_flags & IFF_UP) {
		ifp->if_init(ifp->if_softc);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			ifp->if_start(ifp);
	}

	IWI_UNLOCK(sc);

	return 0;
}

static struct ieee80211_node *
iwi_node_alloc(struct ieee80211_node_table *nt)
{
	struct iwi_node *in;

	in = malloc(sizeof (struct iwi_node), M_80211_NODE, M_NOWAIT | M_ZERO);
	if (in == NULL)
		return NULL;

	in->in_station = -1;

	return &in->in_node;
}

static void
iwi_node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct iwi_softc *sc = ic->ic_ifp->if_softc;
	struct iwi_node *in = (struct iwi_node *)ni;

	if (in->in_station != -1) {
		DPRINTF(("%s mac %6D station %u\n", __func__,
		    ni->ni_macaddr, ":", in->in_station));
		free_unr(sc->sc_unr, in->in_station);
	}

	sc->sc_node_free(ni);
}

static int
iwi_media_change(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;
	int error;
	IWI_LOCK_DECL;

	IWI_LOCK(sc);

	error = ieee80211_media_change(ifp);
	if (error == ENETRESET &&
	    (ifp->if_flags & IFF_UP) && (ifp->if_drv_flags & IFF_DRV_RUNNING))
		iwi_init_locked(sc, 0);

	IWI_UNLOCK(sc);

	return error;
}

/* 
 * Convert h/w rate code to IEEE rate code.
 */
static int
iwi_cvtrate(int iwirate)
{
	switch (iwirate) {
	case IWI_RATE_DS1:	return 2;
	case IWI_RATE_DS2:	return 4;
	case IWI_RATE_DS5:	return 11;
	case IWI_RATE_DS11:	return 22;
	case IWI_RATE_OFDM6:	return 12;
	case IWI_RATE_OFDM9:	return 18;
	case IWI_RATE_OFDM12:	return 24;
	case IWI_RATE_OFDM18:	return 36;
	case IWI_RATE_OFDM24:	return 48;
	case IWI_RATE_OFDM36:	return 72;
	case IWI_RATE_OFDM48:	return 96;
	case IWI_RATE_OFDM54:	return 108;
	}
	return 0;
}

/*
 * The firmware automatically adapts the transmit speed.  We report its current
 * value here.
 */
static void
iwi_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int rate;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;

	/* read current transmission rate from adapter */
	rate = iwi_cvtrate(CSR_READ_4(sc, IWI_CSR_CURRENT_TX_RATE));
	imr->ifm_active |= ieee80211_rate2media(ic, rate, ic->ic_curmode);

	if (ic->ic_opmode == IEEE80211_M_IBSS)
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
	else if (ic->ic_opmode == IEEE80211_M_MONITOR)
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
}

static int
iwi_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_softc *sc = ifp->if_softc;
	int error = 0;

	DPRINTF(("%s: %s -> %s flags 0x%x\n", __func__,
		ieee80211_state_name[ic->ic_state],
		ieee80211_state_name[nstate], sc->flags));

	/* XXX state change race with taskqueue */
	switch (nstate) {
	case IEEE80211_S_AUTH:
		iwi_assoc(ic);
		break;
	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_IBSS) {
			/*
			 * XXX when joining an ibss network we are called
			 * with a SCAN -> RUN transition on scan complete.
			 * Use that to call iwi_auth_and_assoc.  On completing
			 * the join we are then called again with an
			 * AUTH -> RUN transition and we want to do nothing.
			 * This is all totally bogus and needs to be redone.
			 */
			if (ic->ic_state == IEEE80211_S_SCAN)
				iwi_assoc(ic);
		} 
		break;
	case IEEE80211_S_INIT:
		/*
		 * NB: don't try to do this if iwi_stop_master has
		 *     shutdown the firmware and disabled interrupts.
		 */
		if (ic->ic_state == IEEE80211_S_RUN &&
		    (sc->flags & IWI_FLAG_FW_INITED))
			iwi_disassoc(ic);
		if (ic->ic_state == IEEE80211_S_SCAN &&
		    (sc->fw_state == IWI_FW_SCANNING))
			ieee80211_cancel_scan(ic);
		break;
	case IEEE80211_S_ASSOC:
		/*
		 * If we are not transitioning from AUTH the resend the
		 * association request.
		 */
		if (ic->ic_state != IEEE80211_S_AUTH)
			iwi_assoc(ic);
		break;
	default:
		break;
	}
	return (error != 0) ? error : sc->sc_newstate(ic, nstate, arg);

}

/*
 * WME parameters coming from IEEE 802.11e specification.  These values are
 * already declared in ieee80211_proto.c, but they are static so they can't
 * be reused here.
 */
static const struct wmeParams iwi_wme_cck_params[WME_NUM_AC] = {
	{ 0, 3, 5,  7,   0 },	/* WME_AC_BE */
	{ 0, 3, 5, 10,   0 },	/* WME_AC_BK */
	{ 0, 2, 4,  5, 188 },	/* WME_AC_VI */
	{ 0, 2, 3,  4, 102 }	/* WME_AC_VO */
};

static const struct wmeParams iwi_wme_ofdm_params[WME_NUM_AC] = {
	{ 0, 3, 4,  6,   0 },	/* WME_AC_BE */
	{ 0, 3, 4, 10,   0 },	/* WME_AC_BK */
	{ 0, 2, 3,  4,  94 },	/* WME_AC_VI */
	{ 0, 2, 2,  3,  47 }	/* WME_AC_VO */
};
#define IWI_EXP2(v)	htole16((1 << (v)) - 1)
#define IWI_USEC(v)	htole16(IEEE80211_TXOP_TO_US(v))

static void
iwi_wme_init(struct iwi_softc *sc)
{
	const struct wmeParams *wmep;
	int ac;

	memset(sc->wme, 0, sizeof sc->wme);
	for (ac = 0; ac < WME_NUM_AC; ac++) {
		/* set WME values for CCK modulation */
		wmep = &iwi_wme_cck_params[ac];
		sc->wme[1].aifsn[ac] = wmep->wmep_aifsn;
		sc->wme[1].cwmin[ac] = IWI_EXP2(wmep->wmep_logcwmin);
		sc->wme[1].cwmax[ac] = IWI_EXP2(wmep->wmep_logcwmax);
		sc->wme[1].burst[ac] = IWI_USEC(wmep->wmep_txopLimit);
		sc->wme[1].acm[ac]   = wmep->wmep_acm;

		/* set WME values for OFDM modulation */
		wmep = &iwi_wme_ofdm_params[ac];
		sc->wme[2].aifsn[ac] = wmep->wmep_aifsn;
		sc->wme[2].cwmin[ac] = IWI_EXP2(wmep->wmep_logcwmin);
		sc->wme[2].cwmax[ac] = IWI_EXP2(wmep->wmep_logcwmax);
		sc->wme[2].burst[ac] = IWI_USEC(wmep->wmep_txopLimit);
		sc->wme[2].acm[ac]   = wmep->wmep_acm;
	}
}

static int
iwi_wme_setparams(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	const struct wmeParams *wmep;
	int ac;

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		/* set WME values for current operating mode */
		wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[ac];
		sc->wme[0].aifsn[ac] = wmep->wmep_aifsn;
		sc->wme[0].cwmin[ac] = IWI_EXP2(wmep->wmep_logcwmin);
		sc->wme[0].cwmax[ac] = IWI_EXP2(wmep->wmep_logcwmax);
		sc->wme[0].burst[ac] = IWI_USEC(wmep->wmep_txopLimit);
		sc->wme[0].acm[ac]   = wmep->wmep_acm;
	}

	DPRINTF(("Setting WME parameters\n"));
	return iwi_cmd(sc, IWI_CMD_SET_WME_PARAMS, sc->wme, sizeof sc->wme);
}
#undef IWI_USEC
#undef IWI_EXP2

static int
iwi_wme_update(struct ieee80211com *ic)
{
	struct iwi_softc *sc = ic->ic_ifp->if_softc;

	/*
	 * We may be called to update the WME parameters in
	 * the adapter at various places.  If we're already
	 * associated then initiate the request immediately
	 * (via the taskqueue); otherwise we assume the params
	 * will get sent down to the adapter as part of the
	 * work iwi_auth_and_assoc does.
	 */
	return (iwi_queue_cmd(sc, IWI_SET_WME));
}

static int
iwi_wme_setie(struct iwi_softc *sc)
{
	struct ieee80211_wme_info wme;

	memset(&wme, 0, sizeof wme);
	wme.wme_id = IEEE80211_ELEMID_VENDOR;
	wme.wme_len = sizeof (struct ieee80211_wme_info) - 2;
	wme.wme_oui[0] = 0x00;
	wme.wme_oui[1] = 0x50;
	wme.wme_oui[2] = 0xf2;
	wme.wme_type = WME_OUI_TYPE;
	wme.wme_subtype = WME_INFO_OUI_SUBTYPE;
	wme.wme_version = WME_VERSION;
	wme.wme_info = 0;

	DPRINTF(("Setting WME IE (len=%u)\n", wme.wme_len));
	return iwi_cmd(sc, IWI_CMD_SET_WMEIE, &wme, sizeof wme);
}

/*
 * Read 16 bits at address 'addr' from the serial EEPROM.
 */
static uint16_t
iwi_read_prom_word(struct iwi_softc *sc, uint8_t addr)
{
	uint32_t tmp;
	uint16_t val;
	int n;

	/* clock C once before the first command */
	IWI_EEPROM_CTL(sc, 0);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);

	/* write start bit (1) */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D | IWI_EEPROM_C);

	/* write READ opcode (10) */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_D | IWI_EEPROM_C);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);

	/* write address A7-A0 */
	for (n = 7; n >= 0; n--) {
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S |
		    (((addr >> n) & 1) << IWI_EEPROM_SHIFT_D));
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S |
		    (((addr >> n) & 1) << IWI_EEPROM_SHIFT_D) | IWI_EEPROM_C);
	}

	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);

	/* read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S | IWI_EEPROM_C);
		IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
		tmp = MEM_READ_4(sc, IWI_MEM_EEPROM_CTL);
		val |= ((tmp & IWI_EEPROM_Q) >> IWI_EEPROM_SHIFT_Q) << n;
	}

	IWI_EEPROM_CTL(sc, 0);

	/* clear Chip Select and clock C */
	IWI_EEPROM_CTL(sc, IWI_EEPROM_S);
	IWI_EEPROM_CTL(sc, 0);
	IWI_EEPROM_CTL(sc, IWI_EEPROM_C);

	return val;
}

static void
iwi_setcurchan(struct iwi_softc *sc, int chan)
{
	struct ieee80211com *ic = &sc->sc_ic;

	sc->curchan = chan;

	sc->sc_rxtap.wr_chan_freq = sc->sc_txtap.wt_chan_freq =
		htole16(ic->ic_curchan->ic_freq);
	sc->sc_rxtap.wr_chan_flags = sc->sc_txtap.wt_chan_flags =
		htole16(ic->ic_curchan->ic_flags);
}

static void
iwi_frame_intr(struct iwi_softc *sc, struct iwi_rx_data *data, int i,
    struct iwi_frame *frame)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct mbuf *mnew, *m;
	struct ieee80211_node *ni;
	int type, error, framelen;
	IWI_LOCK_DECL;

	framelen = le16toh(frame->len);
	if (framelen < IEEE80211_MIN_LEN || framelen > MCLBYTES) {
		/*
		 * XXX >MCLBYTES is bogus as it means the h/w dma'd
		 *     out of bounds; need to figure out how to limit
		 *     frame size in the firmware
		 */
		/* XXX stat */
		DPRINTFN(1,
		    ("drop rx frame len=%u chan=%u rssi=%u rssi_dbm=%u\n",
		    le16toh(frame->len), frame->chan, frame->rssi,
		    frame->rssi_dbm));
		return;
	}

	DPRINTFN(5, ("received frame len=%u chan=%u rssi=%u rssi_dbm=%u\n",
	    le16toh(frame->len), frame->chan, frame->rssi, frame->rssi_dbm));

	if (frame->chan != sc->curchan)
		iwi_setcurchan(sc, frame->chan);

	/*
	 * Try to allocate a new mbuf for this ring element and load it before
	 * processing the current mbuf. If the ring element cannot be loaded,
	 * drop the received packet and reuse the old mbuf. In the unlikely
	 * case that the old mbuf can't be reloaded either, explicitly panic.
	 */
	mnew = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (mnew == NULL) {
		ifp->if_ierrors++;
		return;
	}

	bus_dmamap_unload(sc->rxq.data_dmat, data->map);

	error = bus_dmamap_load(sc->rxq.data_dmat, data->map,
	    mtod(mnew, void *), MCLBYTES, iwi_dma_map_addr, &data->physaddr,
	    0);
	if (error != 0) {
		m_freem(mnew);

		/* try to reload the old mbuf */
		error = bus_dmamap_load(sc->rxq.data_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, iwi_dma_map_addr,
		    &data->physaddr, 0);
		if (error != 0) {
			/* very unlikely that it will fail... */
			panic("%s: could not load old rx mbuf",
			    device_get_name(sc->sc_dev));
		}
		ifp->if_ierrors++;
		return;
	}

	/*
	 * New mbuf successfully loaded, update Rx ring and continue
	 * processing.
	 */
	m = data->m;
	data->m = mnew;
	CSR_WRITE_4(sc, data->reg, data->physaddr);

	/* finalize mbuf */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = sizeof (struct iwi_hdr) +
	    sizeof (struct iwi_frame) + framelen;

	m_adj(m, sizeof (struct iwi_hdr) + sizeof (struct iwi_frame));

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct iwi_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_rate = iwi_cvtrate(frame->rate);
		tap->wr_antsignal = frame->signal;
		tap->wr_antenna = frame->antenna;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_rxtap_len, m);
	}
	IWI_UNLOCK(sc);

	ni = ieee80211_find_rxnode(ic, mtod(m, struct ieee80211_frame_min *));

	/* send the frame to the 802.11 layer */
	type = ieee80211_input(ic, m, ni, frame->rssi_dbm, 0, 0);

	/* node is no longer needed */
	ieee80211_free_node(ni);

	IWI_LOCK(sc);
	if (sc->sc_softled) {
		/*
		 * Blink for any data frame.  Otherwise do a
		 * heartbeat-style blink when idle.  The latter
		 * is mainly for station mode where we depend on
		 * periodic beacon frames to trigger the poll event.
		 */
		if (type == IEEE80211_FC0_TYPE_DATA) {
			sc->sc_rxrate = frame->rate;
			iwi_led_event(sc, IWI_LED_RX);
		} else if (ticks - sc->sc_ledevent >= sc->sc_ledidle)
			iwi_led_event(sc, IWI_LED_POLL);
	}
}

/* unaligned little endian access */     
#define LE_READ_2(p)					\
	((u_int16_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)					\
	((u_int32_t)					\
	 ((((const u_int8_t *)(p))[0]      ) |		\
	  (((const u_int8_t *)(p))[1] <<  8) |		\
	  (((const u_int8_t *)(p))[2] << 16) |		\
	  (((const u_int8_t *)(p))[3] << 24)))

#define	IEEE80211_VERIFY_LENGTH(_len, _minlen) do {			\
	if ((_len) < (_minlen)) {					\
		return;							\
	}								\
} while (0)

static int __inline
iswmeoui(const u_int8_t *frm)
{
	return frm[1] > 3 && LE_READ_4(frm+2) == ((WME_OUI_TYPE<<24)|WME_OUI);
}

/*
 * Check for an association response frame to see if QoS
 * has been negotiated.  We parse just enough to figure
 * out if we're supposed to use QoS.  The proper solution
 * is to pass the frame up so ieee80211_input can do the
 * work but that's made hard by how things currently are
 * done in the driver.
 */
static void
iwi_checkforqos(struct iwi_softc *sc, const struct ieee80211_frame *wh, int len)
{
#define	SUBTYPE(wh)	((wh)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK)
	const uint8_t *frm, *efrm, *wme;
	struct ieee80211_node *ni;
	uint16_t capinfo, status, associd;

	/* NB: +8 for capinfo, status, associd, and first ie */
	if (!(sizeof(*wh)+8 < len && len < IEEE80211_MAX_LEN) ||
	    SUBTYPE(wh) != IEEE80211_FC0_SUBTYPE_ASSOC_RESP)
		return;
	/*
	 * asresp frame format
	 *	[2] capability information
	 *	[2] status
	 *	[2] association ID
	 *	[tlv] supported rates
	 *	[tlv] extended supported rates
	 *	[tlv] WME
	 */
	frm = (const uint8_t *)&wh[1];
	efrm = ((const uint8_t *) wh) + len;

	capinfo = le16toh(*(const uint16_t *)frm);
	frm += 2;
	status = le16toh(*(const uint16_t *)frm);
	frm += 2;
	associd = le16toh(*(const uint16_t *)frm);
	frm += 2;

	wme = NULL;
	while (frm < efrm) {
		IEEE80211_VERIFY_LENGTH(efrm - frm, frm[1]);
		switch (*frm) {
		case IEEE80211_ELEMID_VENDOR:
			if (iswmeoui(frm))
				wme = frm;
			break;
		}
		frm += frm[1] + 2;
	}

	ni = sc->sc_ic.ic_bss;
	ni->ni_capinfo = capinfo;
	ni->ni_associd = associd;
	if (wme != NULL)
		ni->ni_flags |= IEEE80211_NODE_QOS;
	else
		ni->ni_flags &= ~IEEE80211_NODE_QOS;
#undef SUBTYPE
}

static void
iwi_notification_intr(struct iwi_softc *sc, struct iwi_notif *notif)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_notif_scan_channel *chan;
	struct iwi_notif_scan_complete *scan;
	struct iwi_notif_authentication *auth;
	struct iwi_notif_association *assoc;
	struct iwi_notif_beacon_state *beacon;

	switch (notif->type) {
	case IWI_NOTIF_TYPE_SCAN_CHANNEL:
		chan = (struct iwi_notif_scan_channel *)(notif + 1);

		DPRINTFN(3, ("Scan of channel %u complete (%u)\n",
		    ieee80211_ieee2mhz(chan->nchan, 0), chan->nchan));

		/* Reset the timer, the scan is still going */
		sc->sc_state_timer = 3;
		break;

	case IWI_NOTIF_TYPE_SCAN_COMPLETE:
		scan = (struct iwi_notif_scan_complete *)(notif + 1);

		DPRINTFN(2, ("Scan completed (%u, %u)\n", scan->nchan,
		    scan->status));

		IWI_STATE_END(sc, IWI_FW_SCANNING);

		if (scan->status == IWI_SCAN_COMPLETED)
			ieee80211_scan_next(ic);

		break;

	case IWI_NOTIF_TYPE_AUTHENTICATION:
		auth = (struct iwi_notif_authentication *)(notif + 1);

		switch (auth->state) {
		case IWI_AUTH_SUCCESS:
			DPRINTFN(2, ("Authentication succeeeded\n"));
			ieee80211_node_authorize(ic->ic_bss);
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, -1);
			break;

		case IWI_AUTH_FAIL:
			DPRINTFN(2, ("Authentication failed\n"));
			sc->flags &= ~IWI_FLAG_ASSOCIATED;
			IWI_STATE_END(sc, IWI_FW_ASSOCIATING);
			/* XXX */
			break;

		case IWI_AUTH_SENT_1:
		case IWI_AUTH_RECV_2:
		case IWI_AUTH_SEQ1_PASS:
			break;

		case IWI_AUTH_SEQ1_FAIL:
			DPRINTFN(2, ("Initial authentication handshake failed; "
				"you probably need shared key\n"));
			IWI_STATE_END(sc, IWI_FW_ASSOCIATING);
			/* XXX retry shared key when in auto */
			break;

		default:
			device_printf(sc->sc_dev,
			    "unknown authentication state %u\n", auth->state);
		}
		break;

	case IWI_NOTIF_TYPE_ASSOCIATION:
		assoc = (struct iwi_notif_association *)(notif + 1);

		switch (assoc->state) {
		case IWI_AUTH_SUCCESS:
			/* re-association, do nothing */
			break;

		case IWI_ASSOC_SUCCESS:
			DPRINTFN(2, ("Association succeeded\n"));
			sc->flags |= IWI_FLAG_ASSOCIATED;
			IWI_STATE_END(sc, IWI_FW_ASSOCIATING);
			iwi_checkforqos(sc,
			    (const struct ieee80211_frame *)(assoc+1),
			    le16toh(notif->len) - sizeof(*assoc));
			ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
			break;

		case IWI_ASSOC_INIT:
			switch (sc->fw_state) {
				case IWI_FW_ASSOCIATING:
					DPRINTFN(2, ("Association failed\n"));
					IWI_STATE_END(sc, IWI_FW_ASSOCIATING);
					ieee80211_new_state(ic,
					    IEEE80211_S_SCAN, -1);
					break;

				case IWI_FW_DISASSOCIATING:
					DPRINTFN(2, ("Dissassociated\n"));
					IWI_STATE_END(sc,
					    IWI_FW_DISASSOCIATING);
					break;
			}
			sc->flags &= ~IWI_FLAG_ASSOCIATED;
			break;

		default:
			device_printf(sc->sc_dev,
			    "unknown association state %u\n", assoc->state);
		}
		break;

	case IWI_NOTIF_TYPE_BEACON:
		/* XXX check struct length */
		beacon = (struct iwi_notif_beacon_state *)(notif + 1);

		DPRINTFN(5, ("Beacon state (%u, %u)\n",
		    beacon->state, le32toh(beacon->number)));

		if (beacon->state == IWI_BEACON_MISS) {
			/*
			 * The firmware notifies us of every beacon miss
			 * so we need to track the count against the
			 * configured threshold before notifying the
			 * 802.11 layer.
			 * XXX try to roam, drop assoc only on much higher count
			 */
			if (le32toh(beacon->number) >= ic->ic_bmissthreshold) {
				DPRINTF(("Beacon miss: %u >= %u\n",
				    le32toh(beacon->number),
				    ic->ic_bmissthreshold));
				ieee80211_beacon_miss(ic);
			}
		}
		break;

	case IWI_NOTIF_TYPE_CALIBRATION:
	case IWI_NOTIF_TYPE_NOISE:
	case IWI_NOTIF_TYPE_LINK_QUALITY:
		DPRINTFN(5, ("Notification (%u)\n", notif->type));
		break;

	default:
		DPRINTF(("unknown notification type %u flags 0x%x len %u\n",
		    notif->type, notif->flags, le16toh(notif->len)));
	}
}

static void
iwi_rx_intr(struct iwi_softc *sc)
{
	struct iwi_rx_data *data;
	struct iwi_hdr *hdr;
	uint32_t hw;

	hw = CSR_READ_4(sc, IWI_CSR_RX_RIDX);

	for (; sc->rxq.cur != hw;) {
		data = &sc->rxq.data[sc->rxq.cur];

		bus_dmamap_sync(sc->rxq.data_dmat, data->map,
		    BUS_DMASYNC_POSTREAD);

		hdr = mtod(data->m, struct iwi_hdr *);

		switch (hdr->type) {
		case IWI_HDR_TYPE_FRAME:
			iwi_frame_intr(sc, data, sc->rxq.cur,
			    (struct iwi_frame *)(hdr + 1));
			break;

		case IWI_HDR_TYPE_NOTIF:
			iwi_notification_intr(sc,
			    (struct iwi_notif *)(hdr + 1));
			break;

		default:
			device_printf(sc->sc_dev, "unknown hdr type %u\n",
			    hdr->type);
		}

		DPRINTFN(15, ("rx done idx=%u\n", sc->rxq.cur));

		sc->rxq.cur = (sc->rxq.cur + 1) % IWI_RX_RING_COUNT;
	}

	/* tell the firmware what we have processed */
	hw = (hw == 0) ? IWI_RX_RING_COUNT - 1 : hw - 1;
	CSR_WRITE_4(sc, IWI_CSR_RX_WIDX, hw);
}

static void
iwi_tx_intr(struct iwi_softc *sc, struct iwi_tx_ring *txq)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_tx_data *data;
	uint32_t hw;

	hw = CSR_READ_4(sc, txq->csr_ridx);

	for (; txq->next != hw;) {
		data = &txq->data[txq->next];

		bus_dmamap_sync(txq->data_dmat, data->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(txq->data_dmat, data->map);
		if (data->m->m_flags & M_TXCB)
			ieee80211_process_callback(data->ni, data->m, 0/*XXX*/);
		m_freem(data->m);
		data->m = NULL;
		ieee80211_free_node(data->ni);
		data->ni = NULL;

		DPRINTFN(15, ("tx done idx=%u\n", txq->next));

		ifp->if_opackets++;

		txq->queued--;
		txq->next = (txq->next + 1) % IWI_TX_RING_COUNT;
	}

	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	if (sc->sc_softled)
		iwi_led_event(sc, IWI_LED_TX);

	iwi_start(ifp);
}

static void
iwi_intr(void *arg)
{
	struct iwi_softc *sc = arg;
	uint32_t r;
	IWI_LOCK_DECL;

	IWI_LOCK(sc);

	if ((r = CSR_READ_4(sc, IWI_CSR_INTR)) == 0 || r == 0xffffffff) {
		IWI_UNLOCK(sc);
		return;
	}

	/* acknowledge interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR, r);

	if (r & IWI_INTR_FATAL_ERROR) {
		device_printf(sc->sc_dev, "firmware error\n");
		/* don't restart if the interface isn't up */
		if (sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING)
			taskqueue_enqueue(sc->sc_tq2, &sc->sc_restarttask);

		sc->flags &= ~IWI_FLAG_BUSY;
		sc->sc_busy_timer = 0;
		wakeup(sc);
	}

	if (r & IWI_INTR_FW_INITED) {
		if (!(r & (IWI_INTR_FATAL_ERROR | IWI_INTR_PARITY_ERROR)))
			wakeup(sc);
	}

	if (r & IWI_INTR_RADIO_OFF)
		taskqueue_enqueue(sc->sc_tq, &sc->sc_radiofftask);

	if (r & IWI_INTR_CMD_DONE) {
		sc->flags &= ~IWI_FLAG_BUSY;
		sc->sc_busy_timer = 0;
		wakeup(sc);
	}

	if (r & IWI_INTR_TX1_DONE)
		iwi_tx_intr(sc, &sc->txq[0]);

	if (r & IWI_INTR_TX2_DONE)
		iwi_tx_intr(sc, &sc->txq[1]);

	if (r & IWI_INTR_TX3_DONE)
		iwi_tx_intr(sc, &sc->txq[2]);

	if (r & IWI_INTR_TX4_DONE)
		iwi_tx_intr(sc, &sc->txq[3]);

	if (r & IWI_INTR_RX_DONE)
		iwi_rx_intr(sc);

	if (r & IWI_INTR_PARITY_ERROR) {
		/* XXX rate-limit */
		device_printf(sc->sc_dev, "parity error\n");
	}

	IWI_UNLOCK(sc);
}

static int
iwi_cmd(struct iwi_softc *sc, uint8_t type, void *data, uint8_t len)
{
	struct iwi_cmd_desc *desc;

	IWI_LOCK_ASSERT(sc);

	if (sc->flags & IWI_FLAG_BUSY) {
		device_printf(sc->sc_dev, "%s: cmd %d not sent, busy\n",
			__func__, type);
		return EAGAIN;
	}
	sc->flags |= IWI_FLAG_BUSY;
	sc->sc_busy_timer = 2;

	desc = &sc->cmdq.desc[sc->cmdq.cur];

	desc->hdr.type = IWI_HDR_TYPE_COMMAND;
	desc->hdr.flags = IWI_HDR_FLAG_IRQ;
	desc->type = type;
	desc->len = len;
	memcpy(desc->data, data, len);

	bus_dmamap_sync(sc->cmdq.desc_dmat, sc->cmdq.desc_map,
	    BUS_DMASYNC_PREWRITE);

	DPRINTFN(2, ("sending command idx=%u type=%u len=%u\n", sc->cmdq.cur,
	    type, len));

	sc->cmdq.cur = (sc->cmdq.cur + 1) % IWI_CMD_RING_COUNT;
	CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, sc->cmdq.cur);

	return msleep(sc, &sc->sc_mtx, 0, "iwicmd", hz);
}

static void
iwi_write_ibssnode(struct iwi_softc *sc,
	const u_int8_t addr[IEEE80211_ADDR_LEN], int entry)
{
	struct iwi_ibssnode node;

	/* write node information into NIC memory */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.bssid, addr);

	DPRINTF(("%s mac %6D station %u\n", __func__, node.bssid, ":", entry));

	CSR_WRITE_REGION_1(sc,
	    IWI_CSR_NODE_BASE + entry * sizeof node,
	    (uint8_t *)&node, sizeof node);
}

static int
iwi_tx_start(struct ifnet *ifp, struct mbuf *m0, struct ieee80211_node *ni,
    int ac)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_node *in = (struct iwi_node *)ni;
	const struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	const struct chanAccParams *cap;
	struct iwi_tx_ring *txq = &sc->txq[ac];
	struct iwi_tx_data *data;
	struct iwi_tx_desc *desc;
	struct mbuf *mnew;
	bus_dma_segment_t segs[IWI_MAX_NSEG];
	int error, nsegs, hdrlen, i;
	int ismcast, flags, xflags, staid;

	IWI_LOCK_ASSERT(sc);
	wh = mtod(m0, const struct ieee80211_frame *);
	/* NB: only data frames use this path */
	hdrlen = ieee80211_hdrsize(wh);
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	flags = xflags = 0;

	if (!ismcast)
		flags |= IWI_DATA_FLAG_NEED_ACK;
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		flags |= IWI_DATA_FLAG_SHPREAMBLE;
	if (IEEE80211_QOS_HAS_SEQ(wh)) {
		xflags |= IWI_DATA_XFLAG_QOS;
		cap = &ic->ic_wme.wme_chanParams;
		if (!cap->cap_wmeParams[ac].wmep_noackPolicy)
			flags &= ~IWI_DATA_FLAG_NEED_ACK;
	}

	/*
	 * This is only used in IBSS mode where the firmware expect an index
	 * in a h/w table instead of a destination address.
	 */
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		if (!ismcast) {
			if (in->in_station == -1) {
				in->in_station = alloc_unr(sc->sc_unr);
				if (in->in_station == -1) {
					/* h/w table is full */
					m_freem(m0);
					ieee80211_free_node(ni);
					ifp->if_oerrors++;
					return 0;
				}
				iwi_write_ibssnode(sc,
					ni->ni_macaddr, in->in_station);
			}
			staid = in->in_station;
		} else {
			/*
			 * Multicast addresses have no associated node
			 * so there will be no station entry.  We reserve
			 * entry 0 for one mcast address and use that.
			 * If there are many being used this will be
			 * expensive and we'll need to do a better job
			 * but for now this handles the broadcast case.
			 */
			if (!IEEE80211_ADDR_EQ(wh->i_addr1, sc->sc_mcast)) {
				IEEE80211_ADDR_COPY(sc->sc_mcast, wh->i_addr1);
				iwi_write_ibssnode(sc, sc->sc_mcast, 0);
			}
			staid = 0;
		}
	} else
		staid = 0;

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ic, ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	if (bpf_peers_present(sc->sc_drvbpf)) {
		struct iwi_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;

		bpf_mtap2(sc->sc_drvbpf, tap, sc->sc_txtap_len, m0);
	}

	data = &txq->data[txq->cur];
	desc = &txq->desc[txq->cur];

	/* save and trim IEEE802.11 header */
	m_copydata(m0, 0, hdrlen, (caddr_t)&desc->wh);
	m_adj(m0, hdrlen);

	error = bus_dmamap_load_mbuf_sg(txq->data_dmat, data->map, m0, segs,
	    &nsegs, 0);
	if (error != 0 && error != EFBIG) {
		device_printf(sc->sc_dev, "could not map mbuf (error %d)\n",
		    error);
		m_freem(m0);
		return error;
	}
	if (error != 0) {
		mnew = m_defrag(m0, M_DONTWAIT);
		if (mnew == NULL) {
			device_printf(sc->sc_dev,
			    "could not defragment mbuf\n");
			m_freem(m0);
			return ENOBUFS;
		}
		m0 = mnew;

		error = bus_dmamap_load_mbuf_sg(txq->data_dmat, data->map,
		    m0, segs, &nsegs, 0);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not map mbuf (error %d)\n", error);
			m_freem(m0);
			return error;
		}
	}

	data->m = m0;
	data->ni = ni;

	desc->hdr.type = IWI_HDR_TYPE_DATA;
	desc->hdr.flags = IWI_HDR_FLAG_IRQ;
	desc->station = staid;
	desc->cmd = IWI_DATA_CMD_TX;
	desc->len = htole16(m0->m_pkthdr.len);
	desc->flags = flags;
	desc->xflags = xflags;

#if 0
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		desc->wep_txkey = ic->ic_crypto.cs_def_txkey;
	else
#endif
		desc->flags |= IWI_DATA_FLAG_NO_WEP;

	desc->nseg = htole32(nsegs);
	for (i = 0; i < nsegs; i++) {
		desc->seg_addr[i] = htole32(segs[i].ds_addr);
		desc->seg_len[i]  = htole16(segs[i].ds_len);
	}

	bus_dmamap_sync(txq->data_dmat, data->map, BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(txq->desc_dmat, txq->desc_map, BUS_DMASYNC_PREWRITE);

	DPRINTFN(5, ("sending data frame txq=%u idx=%u len=%u nseg=%u\n",
	    ac, txq->cur, le16toh(desc->len), nsegs));

	txq->queued++;
	txq->cur = (txq->cur + 1) % IWI_TX_RING_COUNT;
	CSR_WRITE_4(sc, txq->csr_widx, txq->cur);

	return 0;
}

static void
iwi_start(struct ifnet *ifp)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m0;
	struct ether_header *eh;
	struct ieee80211_node *ni;
	int ac;
	IWI_LOCK_DECL;

	IWI_LOCK(sc);

	if (ic->ic_state != IEEE80211_S_RUN) {
		IWI_UNLOCK(sc);
		return;
	}

	for (;;) {
		IF_DEQUEUE(&ic->ic_mgtq, m0);
		if (m0 == NULL) {
			IFQ_DRV_DEQUEUE(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			/*
			 * Cancel any background scan.
			 */
			if (ic->ic_flags & IEEE80211_F_SCAN)
				ieee80211_cancel_scan(ic);

			if (m0->m_len < sizeof (struct ether_header) &&
			    (m0 = m_pullup(m0, sizeof (struct ether_header))) == NULL) {
				ifp->if_oerrors++;
				continue;
			}
			eh = mtod(m0, struct ether_header *);
			ni = ieee80211_find_txnode(ic, eh->ether_dhost);
			if (ni == NULL) {
				m_freem(m0);
				ifp->if_oerrors++;
				continue;
			}

			/* classify mbuf so we can find which tx ring to use */
			if (ieee80211_classify(ic, m0, ni) != 0) {
				m_freem(m0);
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				continue;
			}

			/* XXX does not belong here */
			/* no QoS encapsulation for EAPOL frames */
			ac = (eh->ether_type != htons(ETHERTYPE_PAE)) ?
			    M_WME_GETAC(m0) : WME_AC_BE;

			if (sc->txq[ac].queued > IWI_TX_RING_COUNT - 8) {
				/* there is no place left in this ring */
				IFQ_DRV_PREPEND(&ifp->if_snd, m0);
				ifp->if_drv_flags |= IFF_DRV_OACTIVE;
				break;
			}

			BPF_MTAP(ifp, m0);

			m0 = ieee80211_encap(ic, m0, ni);
			if (m0 == NULL) {
				ieee80211_free_node(ni);
				ifp->if_oerrors++;
				continue;
			}
		} else {
			ni = (struct ieee80211_node *) m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
			/* XXX no way to send mgt frames (yet), discard */
			m_freem(m0);
			ieee80211_free_node(ni);
			continue;
		}

		if (bpf_peers_present(ic->ic_rawbpf))
			bpf_mtap(ic->ic_rawbpf, m0);

		if (iwi_tx_start(ifp, m0, ni, ac) != 0) {
			ieee80211_free_node(ni);
			ifp->if_oerrors++;
			break;
		}

		sc->sc_tx_timer = 5;
		ic->ic_lastdata = ticks;
	}

	IWI_UNLOCK(sc);
}

static void
iwi_watchdog(void *arg)
{
	struct iwi_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	IWI_LOCK_ASSERT(sc);

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			if_printf(ifp, "device timeout\n");
			ifp->if_oerrors++;
			taskqueue_enqueue(sc->sc_tq2, &sc->sc_restarttask);
		}
	}
	if (sc->sc_rfkill_timer > 0) {
		if (--sc->sc_rfkill_timer == 0) {
			/*
			 * Check for a change in rfkill state.  We get an
			 * interrupt when a radio is disabled but not when
			 * it is enabled so we must poll for the latter.
			 */
			if (!iwi_getrfkill(sc))
				taskqueue_enqueue(sc->sc_tq, &sc->sc_radiontask);
			else
				sc->sc_rfkill_timer = 2;
		}
	}
	if (sc->sc_state_timer > 0) {
		if (--sc->sc_state_timer == 0) {
			if_printf(ifp, "firmware stuck in state %d, resetting\n",
			    sc->fw_state);
			taskqueue_enqueue(sc->sc_tq2, &sc->sc_restarttask);
			if (sc->fw_state == IWI_FW_SCANNING)
				ieee80211_cancel_scan(&sc->sc_ic);
			sc->sc_state_timer = 3;
		}
	}
	if (sc->sc_busy_timer > 0) {
		if (--sc->sc_busy_timer == 0) {
			if_printf(ifp, "firmware command timeout, resetting\n");
			taskqueue_enqueue(sc->sc_tq2, &sc->sc_restarttask);
		}
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		callout_reset(&sc->sc_wdtimer, hz, iwi_watchdog, sc);
}

static int
iwi_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct iwi_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int error = 0;
	IWI_LOCK_DECL;

	IWI_LOCK(sc);

	/*
	 * wait until pending iwi_cmd() are completed, to avoid races
	 * that could cause problems.
	 */
	while (sc->flags & IWI_FLAG_BUSY)
		msleep(sc, &sc->sc_mtx, 0, "iwiioctl", hz);

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				iwi_init_locked(sc, 0);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				iwi_stop(sc);
			else {
				/*
				 * If device was stopped due to rfkill then
				 * marked down we'll have the polling thread
				 * running; stop it explicitly.
				 */
				sc->sc_rfkill_timer = 0;
			}
		}
		break;

	default:
		error = ieee80211_ioctl(ic, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & IFF_UP) &&
		    (ifp->if_drv_flags & IFF_DRV_RUNNING) &&
		    (ic->ic_roaming != IEEE80211_ROAMING_MANUAL))
			iwi_init_locked(sc, 0);
		error = 0;
	}

	IWI_UNLOCK(sc);

	return error;
}

static void
iwi_stop_master(struct iwi_softc *sc)
{
	uint32_t tmp;
	int ntries;

	/* disable interrupts */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, 0);

	CSR_WRITE_4(sc, IWI_CSR_RST, IWI_RST_STOP_MASTER);
	for (ntries = 0; ntries < 5; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_RST) & IWI_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 5)
		device_printf(sc->sc_dev, "timeout waiting for master\n");

	tmp = CSR_READ_4(sc, IWI_CSR_RST);
	CSR_WRITE_4(sc, IWI_CSR_RST, tmp | IWI_RST_PRINCETON_RESET);

	sc->flags &= ~IWI_FLAG_FW_INITED;
}

static int
iwi_reset(struct iwi_softc *sc)
{
	uint32_t tmp;
	int i, ntries;

	iwi_stop_master(sc);

	tmp = CSR_READ_4(sc, IWI_CSR_CTL);
	CSR_WRITE_4(sc, IWI_CSR_CTL, tmp | IWI_CTL_INIT);

	CSR_WRITE_4(sc, IWI_CSR_READ_INT, IWI_READ_INT_INIT_HOST);

	/* wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_CTL) & IWI_CTL_CLOCK_READY)
			break;
		DELAY(200);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "timeout waiting for clock stabilization\n");
		return EIO;
	}

	tmp = CSR_READ_4(sc, IWI_CSR_RST);
	CSR_WRITE_4(sc, IWI_CSR_RST, tmp | IWI_RST_SOFT_RESET);

	DELAY(10);

	tmp = CSR_READ_4(sc, IWI_CSR_CTL);
	CSR_WRITE_4(sc, IWI_CSR_CTL, tmp | IWI_CTL_INIT);

	/* clear NIC memory */
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_ADDR, 0);
	for (i = 0; i < 0xc000; i++)
		CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, 0);

	return 0;
}

static const struct iwi_firmware_ohdr *
iwi_setup_ofw(struct iwi_softc *sc, struct iwi_fw *fw)
{
	const struct firmware *fp = fw->fp;
	const struct iwi_firmware_ohdr *hdr;

	if (fp->datasize < sizeof (struct iwi_firmware_ohdr)) {
		device_printf(sc->sc_dev, "image '%s' too small\n", fp->name);
		return NULL;
	}
	hdr = (const struct iwi_firmware_ohdr *)fp->data;
	if ((IWI_FW_GET_MAJOR(le32toh(hdr->version)) != IWI_FW_REQ_MAJOR) ||
	    (IWI_FW_GET_MINOR(le32toh(hdr->version)) != IWI_FW_REQ_MINOR)) {
		device_printf(sc->sc_dev, "version for '%s' %d.%d != %d.%d\n",
		    fp->name, IWI_FW_GET_MAJOR(le32toh(hdr->version)),
		    IWI_FW_GET_MINOR(le32toh(hdr->version)), IWI_FW_REQ_MAJOR,
		    IWI_FW_REQ_MINOR);
		return NULL;
	}
	fw->data = ((const char *) fp->data) + sizeof(struct iwi_firmware_ohdr);
	fw->size = fp->datasize - sizeof(struct iwi_firmware_ohdr);
	fw->name = fp->name;
	return hdr;
}

static const struct iwi_firmware_ohdr *
iwi_setup_oucode(struct iwi_softc *sc, struct iwi_fw *fw)
{
	const struct iwi_firmware_ohdr *hdr;

	hdr = iwi_setup_ofw(sc, fw);
	if (hdr != NULL && le32toh(hdr->mode) != IWI_FW_MODE_UCODE) {
		device_printf(sc->sc_dev, "%s is not a ucode image\n",
		    fw->name);
		hdr = NULL;
	}
	return hdr;
}

static void
iwi_getfw(struct iwi_fw *fw, const char *fwname,
	  struct iwi_fw *uc, const char *ucname)
{
	if (fw->fp == NULL)
		fw->fp = firmware_get(fwname);
	/* NB: pre-3.0 ucode is packaged separately */
	if (uc->fp == NULL && fw->fp != NULL && fw->fp->version < 300)
		uc->fp = firmware_get(ucname);
}

/*
 * Get the required firmware images if not already loaded.
 * Note that we hold firmware images so long as the device
 * is marked up in case we need to reload them on device init.
 * This is necessary because we re-init the device sometimes
 * from a context where we cannot read from the filesystem
 * (e.g. from the taskqueue thread when rfkill is re-enabled).
 * XXX return 0 on success, 1 on error.
 *
 * NB: the order of get'ing and put'ing images here is
 * intentional to support handling firmware images bundled
 * by operating mode and/or all together in one file with
 * the boot firmware as "master".
 */
static int
iwi_get_firmware(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	const struct iwi_firmware_hdr *hdr;
	const struct firmware *fp;

	/* invalidate cached firmware on mode change */
	if (sc->fw_mode != ic->ic_opmode)
		iwi_put_firmware(sc);

	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		iwi_getfw(&sc->fw_fw, "iwi_bss", &sc->fw_uc, "iwi_ucode_bss");
		break;

	case IEEE80211_M_IBSS:
		iwi_getfw(&sc->fw_fw, "iwi_ibss", &sc->fw_uc, "iwi_ucode_ibss");
		break;

	case IEEE80211_M_MONITOR:
		iwi_getfw(&sc->fw_fw, "iwi_monitor",
			  &sc->fw_uc, "iwi_ucode_monitor");
		break;

	default:
		break;
	}
	fp = sc->fw_fw.fp;
	if (fp == NULL) {
		device_printf(sc->sc_dev, "could not load firmware\n");
		goto bad;
	}
	if (fp->version < 300) {
		/*
		 * Firmware prior to 3.0 was packaged as separate
		 * boot, firmware, and ucode images.  Verify the
		 * ucode image was read in, retrieve the boot image
		 * if needed, and check version stamps for consistency.
		 * The version stamps in the data are also checked
		 * above; this is a bit paranoid but is a cheap
		 * safeguard against mis-packaging.
		 */
		if (sc->fw_uc.fp == NULL) {
			device_printf(sc->sc_dev, "could not load ucode\n");
			goto bad;
		}
		if (sc->fw_boot.fp == NULL) {
			sc->fw_boot.fp = firmware_get("iwi_boot");
			if (sc->fw_boot.fp == NULL) {
				device_printf(sc->sc_dev,
					"could not load boot firmware\n");
				goto bad;
			}
		}
		if (sc->fw_boot.fp->version != sc->fw_fw.fp->version ||
		    sc->fw_boot.fp->version != sc->fw_uc.fp->version) {
			device_printf(sc->sc_dev,
			    "firmware version mismatch: "
			    "'%s' is %d, '%s' is %d, '%s' is %d\n",
			    sc->fw_boot.fp->name, sc->fw_boot.fp->version,
			    sc->fw_uc.fp->name, sc->fw_uc.fp->version,
			    sc->fw_fw.fp->name, sc->fw_fw.fp->version
			);
			goto bad;
		}
		/*
		 * Check and setup each image.
		 */
		if (iwi_setup_oucode(sc, &sc->fw_uc) == NULL ||
		    iwi_setup_ofw(sc, &sc->fw_boot) == NULL ||
		    iwi_setup_ofw(sc, &sc->fw_fw) == NULL)
			goto bad;
	} else {
		/*
		 * Check and setup combined image.
		 */
		if (fp->datasize < sizeof(struct iwi_firmware_hdr)) {
			device_printf(sc->sc_dev, "image '%s' too small\n",
			    fp->name);
			goto bad;
		}
		hdr = (const struct iwi_firmware_hdr *)fp->data;
		if (fp->datasize < sizeof(*hdr) + le32toh(hdr->bsize) + le32toh(hdr->usize)
				+ le32toh(hdr->fsize)) {
			device_printf(sc->sc_dev, "image '%s' too small (2)\n",
			    fp->name);
			goto bad;
		}
		sc->fw_boot.data = ((const char *) fp->data) + sizeof(*hdr);
		sc->fw_boot.size = le32toh(hdr->bsize);
		sc->fw_boot.name = fp->name;
		sc->fw_uc.data = sc->fw_boot.data + sc->fw_boot.size;
		sc->fw_uc.size = le32toh(hdr->usize);
		sc->fw_uc.name = fp->name;
		sc->fw_fw.data = sc->fw_uc.data + sc->fw_uc.size;
		sc->fw_fw.size = le32toh(hdr->fsize);
		sc->fw_fw.name = fp->name;
	}
#if 0
	device_printf(sc->sc_dev, "boot %d ucode %d fw %d bytes\n",
		sc->fw_boot.size, sc->fw_uc.size, sc->fw_fw.size);
#endif

	sc->fw_mode = ic->ic_opmode;
	return 0;
bad:
	iwi_put_firmware(sc);
	return 1;
}

static void
iwi_put_fw(struct iwi_fw *fw)
{
	if (fw->fp != NULL) {
		firmware_put(fw->fp, FIRMWARE_UNLOAD);
		fw->fp = NULL;
	}
	fw->data = NULL;
	fw->size = 0;
	fw->name = NULL;
}

/*
 * Release any cached firmware images.
 */
static void
iwi_put_firmware(struct iwi_softc *sc)
{
	iwi_put_fw(&sc->fw_uc);
	iwi_put_fw(&sc->fw_fw);
	iwi_put_fw(&sc->fw_boot);
}

static int
iwi_load_ucode(struct iwi_softc *sc, const struct iwi_fw *fw)
{
	uint32_t tmp;
	const uint16_t *w;
	const char *uc = fw->data;
	size_t size = fw->size;
	int i, ntries, error;

	IWI_LOCK_ASSERT(sc);
	error = 0;
	CSR_WRITE_4(sc, IWI_CSR_RST, CSR_READ_4(sc, IWI_CSR_RST) |
	    IWI_RST_STOP_MASTER);
	for (ntries = 0; ntries < 5; ntries++) {
		if (CSR_READ_4(sc, IWI_CSR_RST) & IWI_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 5) {
		device_printf(sc->sc_dev, "timeout waiting for master\n");
		error = EIO;
		goto fail;
	}

	MEM_WRITE_4(sc, 0x3000e0, 0x80000000);
	DELAY(5000);

	tmp = CSR_READ_4(sc, IWI_CSR_RST);
	tmp &= ~IWI_RST_PRINCETON_RESET;
	CSR_WRITE_4(sc, IWI_CSR_RST, tmp);

	DELAY(5000);
	MEM_WRITE_4(sc, 0x3000e0, 0);
	DELAY(1000);
	MEM_WRITE_4(sc, IWI_MEM_EEPROM_EVENT, 1);
	DELAY(1000);
	MEM_WRITE_4(sc, IWI_MEM_EEPROM_EVENT, 0);
	DELAY(1000);
	MEM_WRITE_1(sc, 0x200000, 0x00);
	MEM_WRITE_1(sc, 0x200000, 0x40);
	DELAY(1000);

	/* write microcode into adapter memory */
	for (w = (const uint16_t *)uc; size > 0; w++, size -= 2)
		MEM_WRITE_2(sc, 0x200010, htole16(*w));

	MEM_WRITE_1(sc, 0x200000, 0x00);
	MEM_WRITE_1(sc, 0x200000, 0x80);

	/* wait until we get an answer */
	for (ntries = 0; ntries < 100; ntries++) {
		if (MEM_READ_1(sc, 0x200000) & 1)
			break;
		DELAY(100);
	}
	if (ntries == 100) {
		device_printf(sc->sc_dev,
		    "timeout waiting for ucode to initialize\n");
		error = EIO;
		goto fail;
	}

	/* read the answer or the firmware will not initialize properly */
	for (i = 0; i < 7; i++)
		MEM_READ_4(sc, 0x200004);

	MEM_WRITE_1(sc, 0x200000, 0x00);

fail:
	return error;
}

/* macro to handle unaligned little endian data in firmware image */
#define GETLE32(p) ((p)[0] | (p)[1] << 8 | (p)[2] << 16 | (p)[3] << 24)

static int
iwi_load_firmware(struct iwi_softc *sc, const struct iwi_fw *fw)
{
	u_char *p, *end;
	uint32_t sentinel, ctl, src, dst, sum, len, mlen, tmp;
	int ntries, error;

	IWI_LOCK_ASSERT(sc);
	/* copy firmware image to DMA memory */
	memcpy(sc->fw_virtaddr, fw->data, fw->size);

	/* make sure the adapter will get up-to-date values */
	bus_dmamap_sync(sc->fw_dmat, sc->fw_map, BUS_DMASYNC_PREWRITE);

	/* tell the adapter where the command blocks are stored */
	MEM_WRITE_4(sc, 0x3000a0, 0x27000);

	/*
	 * Store command blocks into adapter's internal memory using register
	 * indirections. The adapter will read the firmware image through DMA
	 * using information stored in command blocks.
	 */
	src = sc->fw_physaddr;
	p = sc->fw_virtaddr;
	end = p + fw->size;
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_ADDR, 0x27000);

	while (p < end) {
		dst = GETLE32(p); p += 4; src += 4;
		len = GETLE32(p); p += 4; src += 4;
		p += len;

		while (len > 0) {
			mlen = min(len, IWI_CB_MAXDATALEN);

			ctl = IWI_CB_DEFAULT_CTL | mlen;
			sum = ctl ^ src ^ dst;

			/* write a command block */
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, ctl);
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, src);
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, dst);
			CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, sum);

			src += mlen;
			dst += mlen;
			len -= mlen;
		}
	}

	/* write a fictive final command block (sentinel) */
	sentinel = CSR_READ_4(sc, IWI_CSR_AUTOINC_ADDR);
	CSR_WRITE_4(sc, IWI_CSR_AUTOINC_DATA, 0);

	tmp = CSR_READ_4(sc, IWI_CSR_RST);
	tmp &= ~(IWI_RST_MASTER_DISABLED | IWI_RST_STOP_MASTER);
	CSR_WRITE_4(sc, IWI_CSR_RST, tmp);

	/* tell the adapter to start processing command blocks */
	MEM_WRITE_4(sc, 0x3000a4, 0x540100);

	/* wait until the adapter reaches the sentinel */
	for (ntries = 0; ntries < 400; ntries++) {
		if (MEM_READ_4(sc, 0x3000d0) >= sentinel)
			break;
		DELAY(100);
	}
	/* sync dma, just in case */
	bus_dmamap_sync(sc->fw_dmat, sc->fw_map, BUS_DMASYNC_POSTWRITE);
	if (ntries == 400) {
		device_printf(sc->sc_dev,
		    "timeout processing command blocks for %s firmware\n",
		    fw->name);
		return EIO;
	}

	/* we're done with command blocks processing */
	MEM_WRITE_4(sc, 0x3000a4, 0x540c00);

	/* allow interrupts so we know when the firmware is ready */
	CSR_WRITE_4(sc, IWI_CSR_INTR_MASK, IWI_INTR_MASK);

	/* tell the adapter to initialize the firmware */
	CSR_WRITE_4(sc, IWI_CSR_RST, 0);

	tmp = CSR_READ_4(sc, IWI_CSR_CTL);
	CSR_WRITE_4(sc, IWI_CSR_CTL, tmp | IWI_CTL_ALLOW_STANDBY);

	/* wait at most one second for firmware initialization to complete */
	if ((error = msleep(sc, &sc->sc_mtx, 0, "iwiinit", hz)) != 0) {
		device_printf(sc->sc_dev, "timeout waiting for %s firmware "
		    "initialization to complete\n", fw->name);
	}

	return error;
}

static int
iwi_setpowermode(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t data;

	if (ic->ic_flags & IEEE80211_F_PMGTON) {
		/* XXX set more fine-grained operation */
		data = htole32(IWI_POWER_MODE_MAX);
	} else
		data = htole32(IWI_POWER_MODE_CAM);

	DPRINTF(("Setting power mode to %u\n", le32toh(data)));
	return iwi_cmd(sc, IWI_CMD_SET_POWER_MODE, &data, sizeof data);
}

static int
iwi_setwepkeys(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct iwi_wep_key wepkey;
	struct ieee80211_key *wk;
	int error, i;

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		wk = &ic->ic_crypto.cs_nw_keys[i];

		wepkey.cmd = IWI_WEP_KEY_CMD_SETKEY;
		wepkey.idx = i;
		wepkey.len = wk->wk_keylen;
		memset(wepkey.key, 0, sizeof wepkey.key);
		memcpy(wepkey.key, wk->wk_key, wk->wk_keylen);
		DPRINTF(("Setting wep key index %u len %u\n", wepkey.idx,
		    wepkey.len));
		error = iwi_cmd(sc, IWI_CMD_SET_WEP_KEY, &wepkey,
		    sizeof wepkey);
		if (error != 0)
			return error;
	}
	return 0;
}

static int
iwi_config(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_configuration config;
	struct iwi_rateset rs;
	struct iwi_txpower power;
	uint32_t data;
	int error, i;
	IWI_LOCK_ASSERT(sc);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, IF_LLADDR(ifp));
	DPRINTF(("Setting MAC address to %6D\n", ic->ic_myaddr, ":"));
	error = iwi_cmd(sc, IWI_CMD_SET_MAC_ADDRESS, ic->ic_myaddr,
	    IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;

	memset(&config, 0, sizeof config);
	config.bluetooth_coexistence = sc->bluetooth;
	config.silence_threshold = 0x1e;
	config.antenna = sc->antenna;
	config.multicast_enabled = 1;
	config.answer_pbreq = (ic->ic_opmode == IEEE80211_M_IBSS) ? 1 : 0;
	config.disable_unicast_decryption = 1;
	config.disable_multicast_decryption = 1;
	DPRINTF(("Configuring adapter\n"));
	error = iwi_cmd(sc, IWI_CMD_SET_CONFIG, &config, sizeof config);
	if (error != 0)
		return error;

	error = iwi_setpowermode(sc);
	if (error != 0)
		return error;

	data = htole32(ic->ic_rtsthreshold);
	DPRINTF(("Setting RTS threshold to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_RTS_THRESHOLD, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(ic->ic_fragthreshold);
	DPRINTF(("Setting fragmentation threshold to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_FRAG_THRESHOLD, &data, sizeof data);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		power.mode = IWI_MODE_11B;
		power.nchan = 11;
		for (i = 0; i < 11; i++) {
			power.chan[i].chan = i + 1;
			power.chan[i].power = IWI_TXPOWER_MAX;
		}
		DPRINTF(("Setting .11b channels tx power\n"));
		error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power);
		if (error != 0)
			return error;

		power.mode = IWI_MODE_11G;
		DPRINTF(("Setting .11g channels tx power\n"));
		error = iwi_cmd(sc, IWI_CMD_SET_TX_POWER, &power, sizeof power);
		if (error != 0)
			return error;
	}

	memset(&rs, 0, sizeof rs);
	rs.mode = IWI_MODE_11G;
	rs.type = IWI_RATESET_TYPE_SUPPORTED;
	rs.nrates = ic->ic_sup_rates[IEEE80211_MODE_11G].rs_nrates;
	memcpy(rs.rates, ic->ic_sup_rates[IEEE80211_MODE_11G].rs_rates,
	    rs.nrates);
	DPRINTF(("Setting .11bg supported rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs);
	if (error != 0)
		return error;

	memset(&rs, 0, sizeof rs);
	rs.mode = IWI_MODE_11A;
	rs.type = IWI_RATESET_TYPE_SUPPORTED;
	rs.nrates = ic->ic_sup_rates[IEEE80211_MODE_11A].rs_nrates;
	memcpy(rs.rates, ic->ic_sup_rates[IEEE80211_MODE_11A].rs_rates,
	    rs.nrates);
	DPRINTF(("Setting .11a supported rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs);
	if (error != 0)
		return error;

	/* if we have a desired ESSID, set it now */
	if (ic->ic_des_ssid[0].len != 0) {
#ifdef IWI_DEBUG
		if (iwi_debug > 0) {
			printf("Setting desired ESSID to ");
			ieee80211_print_essid(ic->ic_des_ssid[0].ssid,
			    ic->ic_des_ssid[0].len);
			printf("\n");
		}
#endif
		error = iwi_cmd(sc, IWI_CMD_SET_ESSID, ic->ic_des_ssid[0].ssid,
		    ic->ic_des_ssid[0].len);
		if (error != 0)
			return error;
	}

	data = htole32(arc4random());
	DPRINTF(("Setting initialization vector to %u\n", le32toh(data)));
	error = iwi_cmd(sc, IWI_CMD_SET_IV, &data, sizeof data);
	if (error != 0)
		return error;

	error = iwi_setwepkeys(sc);
	if (error != 0)
		return error;

	/* enable adapter */
	DPRINTF(("Enabling adapter\n"));
	return iwi_cmd(sc, IWI_CMD_ENABLE, NULL, 0);
}

static __inline void
set_scan_type(struct iwi_scan_ext *scan, int ix, int scan_type)
{
	uint8_t *st = &scan->scan_type[ix / 2];
	if (ix % 2)
		*st = (*st & 0xf0) | ((scan_type & 0xf) << 0);
	else
		*st = (*st & 0x0f) | ((scan_type & 0xf) << 4);
}

static int
scan_type(const struct ieee80211_scan_state *ss,
	const struct ieee80211_channel *chan)
{
	/* We can only set one essid for a directed scan */
	if (ss->ss_nssid != 0)
		return IWI_SCAN_TYPE_BDIRECTED;
	if ((ss->ss_flags & IEEE80211_SCAN_ACTIVE) &&
	    (chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0)
		return IWI_SCAN_TYPE_BROADCAST;
	return IWI_SCAN_TYPE_PASSIVE;
}

static __inline int
scan_band(const struct ieee80211_channel *c)
{
	return IEEE80211_IS_CHAN_5GHZ(c) ?  IWI_CHAN_5GHZ : IWI_CHAN_2GHZ;
}

/*
 * Start a scan on the current channel or all channels.
 */
static int
iwi_scanchan(struct iwi_softc *sc, unsigned long maxdwell, int mode)
{
	struct ieee80211com *ic;
	struct ieee80211_channel *chan;
	struct ieee80211_scan_state *ss;
	struct iwi_scan_ext scan;
	int error = 0;

	IWI_LOCK_ASSERT(sc);
	if (sc->fw_state == IWI_FW_SCANNING) {
		/*
		 * This should not happen as we only trigger scan_next after
		 * completion
		 */
		DPRINTF(("%s: called too early - still scanning\n", __func__));
		return (EBUSY);
	}
	IWI_STATE_BEGIN(sc, IWI_FW_SCANNING);

	ic = &sc->sc_ic;
	ss = ic->ic_scan;

	memset(&scan, 0, sizeof scan);
	scan.full_scan_index = htole32(++sc->sc_scangen);
	scan.dwell_time[IWI_SCAN_TYPE_PASSIVE] = htole16(maxdwell);
	if (ic->ic_flags_ext & IEEE80211_FEXT_BGSCAN) {
		/*
		 * Use very short dwell times for when we send probe request
		 * frames.  Without this bg scans hang.  Ideally this should
		 * be handled with early-termination as done by net80211 but
		 * that's not feasible (aborting a scan is problematic).
		 */
		scan.dwell_time[IWI_SCAN_TYPE_BROADCAST] = htole16(30);
		scan.dwell_time[IWI_SCAN_TYPE_BDIRECTED] = htole16(30);
	} else {
		scan.dwell_time[IWI_SCAN_TYPE_BROADCAST] = htole16(maxdwell);
		scan.dwell_time[IWI_SCAN_TYPE_BDIRECTED] = htole16(maxdwell);
	}

	/* We can only set one essid for a directed scan */
	if (ss->ss_nssid != 0) {
		error = iwi_cmd(sc, IWI_CMD_SET_ESSID, ss->ss_ssid[0].ssid,
		    ss->ss_ssid[0].len);
		if (error)
			return (error);
	}

	if (mode == IWI_SCAN_ALLCHAN) {
		int i, next, band, b, bstart;
		/*
		 * Convert scan list to run-length encoded channel list
		 * the firmware requires (preserving the order setup by
		 * net80211).  The first entry in each run specifies the
		 * band and the count of items in the run.
		 */
		next = 0;		/* next open slot */
		bstart = 0;		/* NB: not needed, silence compiler */
		band = -1;		/* NB: impossible value */
		KASSERT(ss->ss_last > 0, ("no channels"));
		for (i = 0; i < ss->ss_last; i++) {
			chan = ss->ss_chans[i];
			b = scan_band(chan);
			if (b != band) {
				if (band != -1)
					scan.channels[bstart] =
					    (next - bstart) | band;
				/* NB: this allocates a slot for the run-len */
				band = b, bstart = next++;
			}
			if (next >= IWI_SCAN_CHANNELS) {
				DPRINTF(("truncating scan list\n"));
				break;
			}
			scan.channels[next] = ieee80211_chan2ieee(ic, chan);
			set_scan_type(&scan, next, scan_type(ss, chan));
			next++;
		}
		scan.channels[bstart] = (next - bstart) | band;
	} else {
		/* Scan the current channel only */
		chan = ic->ic_curchan;
		scan.channels[0] = 1 | scan_band(chan);
		scan.channels[1] = ieee80211_chan2ieee(ic, chan);
		set_scan_type(&scan, 1, scan_type(ss, chan));
	}
#ifdef IWI_DEBUG
	if (iwi_debug > 0) {
		static const char *scantype[8] =
		   { "PSTOP", "PASV", "DIR", "BCAST", "BDIR", "5", "6", "7" };
		int i;
		printf("Scan request: index %u dwell %d/%d/%d\n"
		    , le32toh(scan.full_scan_index)
		    , le16toh(scan.dwell_time[IWI_SCAN_TYPE_PASSIVE])
		    , le16toh(scan.dwell_time[IWI_SCAN_TYPE_BROADCAST])
		    , le16toh(scan.dwell_time[IWI_SCAN_TYPE_BDIRECTED])
		);
		i = 0;
		do {
			int run = scan.channels[i];
			if (run == 0)
				break;
			printf("Scan %d %s channels:", run & 0x3f,
			    run & IWI_CHAN_2GHZ ? "2.4GHz" : "5GHz");
			for (run &= 0x3f, i++; run > 0; run--, i++) {
				uint8_t type = scan.scan_type[i/2];
				printf(" %u/%s", scan.channels[i],
				    scantype[(i & 1 ? type : type>>4) & 7]);
			}
			printf("\n");
		} while (i < IWI_SCAN_CHANNELS);
	}
#endif

	return (iwi_cmd(sc, IWI_CMD_SCAN_EXT, &scan, sizeof scan));
}

static void
iwi_scanabort(void *arg, int npending)
{
	struct iwi_softc *sc = arg;
	IWI_LOCK_DECL;

	IWI_LOCK(sc);
	sc->flags &= ~IWI_FLAG_CHANNEL_SCAN;
	/* NB: make sure we're still scanning */
	if (sc->fw_state == IWI_FW_SCANNING)
		iwi_cmd(sc, IWI_CMD_ABORT_SCAN, NULL, 0);
	IWI_UNLOCK(sc);
}

static int
iwi_set_sensitivity(struct iwi_softc *sc, int8_t rssi_dbm)
{
	struct iwi_sensitivity sens;

	DPRINTF(("Setting sensitivity to %d\n", rssi_dbm));

	memset(&sens, 0, sizeof sens);
	sens.rssi = htole16(rssi_dbm);
	return iwi_cmd(sc, IWI_CMD_SET_SENSITIVITY, &sens, sizeof sens);
}

static int
iwi_auth_and_assoc(struct iwi_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_node *ni = ic->ic_bss;
	struct iwi_configuration config;
	struct iwi_associate *assoc = &sc->assoc;
	struct iwi_rateset rs;
	uint16_t capinfo;
	int error, mode;

	IWI_LOCK_ASSERT(sc);

	if (sc->flags & IWI_FLAG_ASSOCIATED) {
		DPRINTF(("Already associated\n"));
		return (-1);
	}

	IWI_STATE_BEGIN(sc, IWI_FW_ASSOCIATING);
	error = 0;
	mode = 0;

	if (IEEE80211_IS_CHAN_A(ic->ic_curchan))
		mode = IWI_MODE_11A;
	else if (IEEE80211_IS_CHAN_G(ic->ic_curchan))
		mode = IWI_MODE_11G;
	if (IEEE80211_IS_CHAN_B(ic->ic_curchan))
		mode = IWI_MODE_11B;

	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
		memset(&config, 0, sizeof config);
		config.bluetooth_coexistence = sc->bluetooth;
		config.antenna = sc->antenna;
		config.multicast_enabled = 1;
		if (mode == IWI_MODE_11G)
			config.use_protection = 1;
		config.answer_pbreq =
		    (ic->ic_opmode == IEEE80211_M_IBSS) ? 1 : 0;
		config.disable_unicast_decryption = 1;
		config.disable_multicast_decryption = 1;
		DPRINTF(("Configuring adapter\n"));
		error = iwi_cmd(sc, IWI_CMD_SET_CONFIG, &config, sizeof config);
		if (error != 0)
			goto done;
	}

#ifdef IWI_DEBUG
	if (iwi_debug > 0) {
		printf("Setting ESSID to ");
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		printf("\n");
	}
#endif
	error = iwi_cmd(sc, IWI_CMD_SET_ESSID, ni->ni_essid, ni->ni_esslen);
	if (error != 0)
		goto done;

	/* the rate set has already been "negotiated" */
	memset(&rs, 0, sizeof rs);
	rs.mode = mode;
	rs.type = IWI_RATESET_TYPE_NEGOTIATED;
	rs.nrates = ni->ni_rates.rs_nrates;
	if (rs.nrates > IWI_RATESET_SIZE) {
		DPRINTF(("Truncating negotiated rate set from %u\n",
		    rs.nrates));
		rs.nrates = IWI_RATESET_SIZE;
	}
	memcpy(rs.rates, ni->ni_rates.rs_rates, rs.nrates);
	DPRINTF(("Setting negotiated rates (%u)\n", rs.nrates));
	error = iwi_cmd(sc, IWI_CMD_SET_RATES, &rs, sizeof rs);
	if (error != 0)
		goto done;

	memset(assoc, 0, sizeof *assoc);

	if ((ic->ic_flags & IEEE80211_F_WME) && ni->ni_ies.wme_ie != NULL) {
		/* NB: don't treat WME setup as failure */
		if (iwi_wme_setparams(sc) == 0 && iwi_wme_setie(sc) == 0)
			assoc->policy |= htole16(IWI_POLICY_WME);
		/* XXX complain on failure? */
	}

	if (ic->ic_opt_ie != NULL) {
		DPRINTF(("Setting optional IE (len=%u)\n", ic->ic_opt_ie_len));
		error = iwi_cmd(sc, IWI_CMD_SET_OPTIE, ic->ic_opt_ie,
		    ic->ic_opt_ie_len);
		if (error != 0)
			goto done;
	}

	error = iwi_set_sensitivity(sc, ni->ni_rssi);
	if (error != 0)
		goto done;

	assoc->mode = mode;
	assoc->chan = ic->ic_curchan->ic_ieee;
	/*
	 * NB: do not arrange for shared key auth w/o privacy
	 *     (i.e. a wep key); it causes a firmware error.
	 */
	if ((ic->ic_flags & IEEE80211_F_PRIVACY) &&
	    ni->ni_authmode == IEEE80211_AUTH_SHARED) {
		assoc->auth = IWI_AUTH_SHARED;
		/*
		 * It's possible to have privacy marked but no default
		 * key setup.  This typically is due to a user app bug
		 * but if we blindly grab the key the firmware will
		 * barf so avoid it for now.
		 */ 
		if (ic->ic_crypto.cs_def_txkey != IEEE80211_KEYIX_NONE)
			assoc->auth |= ic->ic_crypto.cs_def_txkey << 4;

		error = iwi_setwepkeys(sc);
		if (error != 0)
			goto done;
	}
	if (ic->ic_flags & IEEE80211_F_WPA)
		assoc->policy |= htole16(IWI_POLICY_WPA);
	if (ic->ic_opmode == IEEE80211_M_IBSS && ni->ni_tstamp.tsf == 0)
		assoc->type = IWI_HC_IBSS_START;
	else
		assoc->type = IWI_HC_ASSOC;
	memcpy(assoc->tstamp, ni->ni_tstamp.data, 8);

	if (ic->ic_opmode == IEEE80211_M_IBSS)
		capinfo = IEEE80211_CAPINFO_IBSS;
	else
		capinfo = IEEE80211_CAPINFO_ESS;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
	    IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
		capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
	if (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME)
		capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
	assoc->capinfo = htole16(capinfo);

	assoc->lintval = htole16(ic->ic_lintval);
	assoc->intval = htole16(ni->ni_intval);
	IEEE80211_ADDR_COPY(assoc->bssid, ni->ni_bssid);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		IEEE80211_ADDR_COPY(assoc->dst, ifp->if_broadcastaddr);
	else
		IEEE80211_ADDR_COPY(assoc->dst, ni->ni_bssid);

	DPRINTF(("%s bssid %6D dst %6D channel %u policy 0x%x "
	    "auth %u capinfo 0x%x lintval %u bintval %u\n",
	    assoc->type == IWI_HC_IBSS_START ? "Start" : "Join",
	    assoc->bssid, ":", assoc->dst, ":",
	    assoc->chan, le16toh(assoc->policy), assoc->auth,
	    le16toh(assoc->capinfo), le16toh(assoc->lintval),
	    le16toh(assoc->intval)));
	error = iwi_cmd(sc, IWI_CMD_ASSOCIATE, assoc, sizeof *assoc);
done:
	if (error)
		IWI_STATE_END(sc, IWI_FW_ASSOCIATING);

	return (error);
}

static int
iwi_disassociate(struct iwi_softc *sc, int quiet)
{
	struct iwi_associate *assoc = &sc->assoc;

	if ((sc->flags & IWI_FLAG_ASSOCIATED) == 0) {
		DPRINTF(("Not associated\n"));
		return (-1);
	}

	IWI_STATE_BEGIN(sc, IWI_FW_DISASSOCIATING);

	if (quiet)
		assoc->type = IWI_HC_DISASSOC_QUIET;
	else
		assoc->type = IWI_HC_DISASSOC;

	DPRINTF(("Trying to disassociate from %6D channel %u\n",
	    assoc->bssid, ":", assoc->chan));
	return iwi_cmd(sc, IWI_CMD_ASSOCIATE, assoc, sizeof *assoc);
}

static void
iwi_init(void *priv)
{
	struct iwi_softc *sc = priv;
	IWI_LOCK_DECL;

	IWI_LOCK(sc);
	iwi_init_locked(sc, 0);
	IWI_UNLOCK(sc);
}

/*
 * release dma resources for the firmware
 */
static void
iwi_release_fw_dma(struct iwi_softc *sc)
{
	if (sc->fw_flags & IWI_FW_HAVE_PHY)
		bus_dmamap_unload(sc->fw_dmat, sc->fw_map);
	if (sc->fw_flags & IWI_FW_HAVE_MAP)
		bus_dmamem_free(sc->fw_dmat, sc->fw_virtaddr, sc->fw_map);
	if (sc->fw_flags & IWI_FW_HAVE_DMAT)
		bus_dma_tag_destroy(sc->fw_dmat);

	sc->fw_flags = 0;
	sc->fw_dma_size = 0;
	sc->fw_dmat = NULL;
	sc->fw_map = NULL;
	sc->fw_physaddr = 0;
	sc->fw_virtaddr = NULL;
}

/*
 * allocate the dma descriptor for the firmware.
 * Return 0 on success, 1 on error.
 * Must be called unlocked, protected by IWI_FLAG_FW_LOADING.
 */
static int
iwi_init_fw_dma(struct iwi_softc *sc, int size)
{
	if (sc->fw_dma_size >= size)
		return 0;
	if (bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 4, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    size, 1, size, 0, NULL, NULL, &sc->fw_dmat) != 0) {
		device_printf(sc->sc_dev,
		    "could not create firmware DMA tag\n");
		goto error;
	}
	sc->fw_flags |= IWI_FW_HAVE_DMAT;
	if (bus_dmamem_alloc(sc->fw_dmat, &sc->fw_virtaddr, 0,
	    &sc->fw_map) != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate firmware DMA memory\n");
		goto error;
	}
	sc->fw_flags |= IWI_FW_HAVE_MAP;
	if (bus_dmamap_load(sc->fw_dmat, sc->fw_map, sc->fw_virtaddr,
	    size, iwi_dma_map_addr, &sc->fw_physaddr, 0) != 0) {
		device_printf(sc->sc_dev, "could not load firmware DMA map\n");
		goto error;
	}
	sc->fw_flags |= IWI_FW_HAVE_PHY;
	sc->fw_dma_size = size;
	return 0;

error:
	iwi_release_fw_dma(sc);
	return 1;
}

static void
iwi_init_locked(void *priv, int force)
{
	struct iwi_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_rx_data *data;
	int i;
	IWI_LOCK_DECL;

	IWI_LOCK_ASSERT(sc);
	if (sc->fw_state == IWI_FW_LOADING) {
		device_printf(sc->sc_dev, "%s: already loading\n", __func__);
		return;		/* XXX: condvar? */
	}

	iwi_stop(sc);
	IWI_STATE_BEGIN(sc, IWI_FW_LOADING);

	if (iwi_reset(sc) != 0) {
		device_printf(sc->sc_dev, "could not reset adapter\n");
		goto fail;
	}

	IWI_UNLOCK(sc);
	if (iwi_get_firmware(sc)) {
		IWI_LOCK(sc);
		goto fail;
	}

	/* allocate DMA memory for mapping firmware image */
	i = sc->fw_fw.size;
	if (sc->fw_boot.size > i)
		i = sc->fw_boot.size;
	/* XXX do we dma the ucode as well ? */
	if (sc->fw_uc.size > i)
		i = sc->fw_uc.size;
	if (iwi_init_fw_dma(sc, i)) {
		IWI_LOCK(sc);
		goto fail;
	}
	IWI_LOCK(sc);

	if (iwi_load_firmware(sc, &sc->fw_boot) != 0) {
		device_printf(sc->sc_dev,
		    "could not load boot firmware %s\n", sc->fw_boot.name);
		goto fail;
	}

	if (iwi_load_ucode(sc, &sc->fw_uc) != 0) {
		device_printf(sc->sc_dev,
		    "could not load microcode %s\n", sc->fw_uc.name);
		goto fail;
	}

	iwi_stop_master(sc);

	CSR_WRITE_4(sc, IWI_CSR_CMD_BASE, sc->cmdq.physaddr);
	CSR_WRITE_4(sc, IWI_CSR_CMD_SIZE, sc->cmdq.count);
	CSR_WRITE_4(sc, IWI_CSR_CMD_WIDX, sc->cmdq.cur);

	CSR_WRITE_4(sc, IWI_CSR_TX1_BASE, sc->txq[0].physaddr);
	CSR_WRITE_4(sc, IWI_CSR_TX1_SIZE, sc->txq[0].count);
	CSR_WRITE_4(sc, IWI_CSR_TX1_WIDX, sc->txq[0].cur);

	CSR_WRITE_4(sc, IWI_CSR_TX2_BASE, sc->txq[1].physaddr);
	CSR_WRITE_4(sc, IWI_CSR_TX2_SIZE, sc->txq[1].count);
	CSR_WRITE_4(sc, IWI_CSR_TX2_WIDX, sc->txq[1].cur);

	CSR_WRITE_4(sc, IWI_CSR_TX3_BASE, sc->txq[2].physaddr);
	CSR_WRITE_4(sc, IWI_CSR_TX3_SIZE, sc->txq[2].count);
	CSR_WRITE_4(sc, IWI_CSR_TX3_WIDX, sc->txq[2].cur);

	CSR_WRITE_4(sc, IWI_CSR_TX4_BASE, sc->txq[3].physaddr);
	CSR_WRITE_4(sc, IWI_CSR_TX4_SIZE, sc->txq[3].count);
	CSR_WRITE_4(sc, IWI_CSR_TX4_WIDX, sc->txq[3].cur);

	for (i = 0; i < sc->rxq.count; i++) {
		data = &sc->rxq.data[i];
		CSR_WRITE_4(sc, data->reg, data->physaddr);
	}

	CSR_WRITE_4(sc, IWI_CSR_RX_WIDX, sc->rxq.count - 1);

	if (iwi_load_firmware(sc, &sc->fw_fw) != 0) {
		device_printf(sc->sc_dev,
		    "could not load main firmware %s\n", sc->fw_fw.name);
		goto fail;
	}
	sc->flags |= IWI_FLAG_FW_INITED;

	if (iwi_config(sc) != 0) {
		device_printf(sc->sc_dev, "device configuration failed\n");
		goto fail;
	}

	if (ic->ic_opmode != IEEE80211_M_MONITOR) {
		/*
		 * NB: When restarting the adapter clock the state
		 * machine regardless of the roaming mode; otherwise
		 * we need to notify user apps so they can manually
		 * get us going again.
		 */
		if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL || force)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	} else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);

	callout_reset(&sc->sc_wdtimer, hz, iwi_watchdog, sc);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	IWI_STATE_END(sc, IWI_FW_LOADING);
	return;

fail:	ifp->if_flags &= ~IFF_UP;
	IWI_STATE_END(sc, IWI_FW_LOADING);
	iwi_stop(sc);
	iwi_put_firmware(sc);
}

static void
iwi_stop(void *priv)
{
	struct iwi_softc *sc = priv;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = ic->ic_ifp;

	IWI_LOCK_ASSERT(sc);
	if (sc->sc_softled) {
		callout_stop(&sc->sc_ledtimer);
		sc->sc_blinking = 0;
	}

	callout_stop(&sc->sc_wdtimer);
	iwi_stop_master(sc);

	CSR_WRITE_4(sc, IWI_CSR_RST, IWI_RST_SOFT_RESET);

	/* reset rings */
	iwi_reset_cmd_ring(sc, &sc->cmdq);
	iwi_reset_tx_ring(sc, &sc->txq[0]);
	iwi_reset_tx_ring(sc, &sc->txq[1]);
	iwi_reset_tx_ring(sc, &sc->txq[2]);
	iwi_reset_tx_ring(sc, &sc->txq[3]);
	iwi_reset_rx_ring(sc, &sc->rxq);

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	memset(sc->sc_cmd, 0, sizeof(sc->sc_cmd));
	sc->sc_tx_timer = 0;
	sc->sc_rfkill_timer = 0;
	sc->sc_state_timer = 0;
	sc->sc_busy_timer = 0;
	sc->flags &= ~(IWI_FLAG_BUSY | IWI_FLAG_ASSOCIATED);
	sc->fw_state = IWI_FW_IDLE;
	wakeup(sc);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
}

static void
iwi_restart(void *arg, int npending)
{
	struct iwi_softc *sc = arg;
	IWI_LOCK_DECL;

	IWI_LOCK(sc);
	iwi_init_locked(sc, 1);		/* NB: force state machine */
	IWI_UNLOCK(sc);
}

/*
 * Return whether or not the radio is enabled in hardware
 * (i.e. the rfkill switch is "off").
 */
static int
iwi_getrfkill(struct iwi_softc *sc)
{
	return (CSR_READ_4(sc, IWI_CSR_IO) & IWI_IO_RADIO_ENABLED) == 0;
}

static void
iwi_radio_on(void *arg, int pending)
{
	struct iwi_softc *sc = arg;

	device_printf(sc->sc_dev, "radio turned on\n");
	iwi_init(sc);
}

static void
iwi_radio_off(void *arg, int pending)
{
	struct iwi_softc *sc = arg;
	IWI_LOCK_DECL;

	device_printf(sc->sc_dev, "radio turned off\n");
	IWI_LOCK(sc);
	iwi_stop(sc);
	sc->sc_rfkill_timer = 2;
	IWI_UNLOCK(sc);
}

static int
iwi_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	struct iwi_softc *sc = arg1;
	uint32_t size, buf[128];

	memset(buf, 0, sizeof buf);

	if (!(sc->flags & IWI_FLAG_FW_INITED))
		return SYSCTL_OUT(req, buf, sizeof buf);

	size = min(CSR_READ_4(sc, IWI_CSR_TABLE0_SIZE), 128 - 1);
	CSR_READ_REGION_4(sc, IWI_CSR_TABLE0_BASE, &buf[1], size);

	return SYSCTL_OUT(req, buf, size);
}

static int
iwi_sysctl_radio(SYSCTL_HANDLER_ARGS)
{
	struct iwi_softc *sc = arg1;
	int val = !iwi_getrfkill(sc);

	return SYSCTL_OUT(req, &val, sizeof val);
}

/*
 * Add sysctl knobs.
 */
static void
iwi_sysctlattach(struct iwi_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "radio",
	    CTLTYPE_INT | CTLFLAG_RD, sc, 0, iwi_sysctl_radio, "I",
	    "radio transmitter switch state (0=off, 1=on)");

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "stats",
	    CTLTYPE_OPAQUE | CTLFLAG_RD, sc, 0, iwi_sysctl_stats, "S",
	    "statistics");

	sc->bluetooth = 0;
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "bluetooth",
	    CTLFLAG_RW, &sc->bluetooth, 0, "bluetooth coexistence");

	sc->antenna = IWI_ANTENNA_AUTO;
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "antenna",
	    CTLFLAG_RW, &sc->antenna, 0, "antenna (0=auto)");
}

/*
 * LED support.
 *
 * Different cards have different capabilities.  Some have three
 * led's while others have only one.  The linux ipw driver defines
 * led's for link state (associated or not), band (11a, 11g, 11b),
 * and for link activity.  We use one led and vary the blink rate
 * according to the tx/rx traffic a la the ath driver.
 */

static __inline uint32_t
iwi_toggle_event(uint32_t r)
{
	return r &~ (IWI_RST_STANDBY | IWI_RST_GATE_ODMA |
		     IWI_RST_GATE_IDMA | IWI_RST_GATE_ADMA);
}

static uint32_t
iwi_read_event(struct iwi_softc *sc)
{
	return MEM_READ_4(sc, IWI_MEM_EEPROM_EVENT);
}

static void
iwi_write_event(struct iwi_softc *sc, uint32_t v)
{
	MEM_WRITE_4(sc, IWI_MEM_EEPROM_EVENT, v);
}

static void
iwi_led_done(void *arg)
{
	struct iwi_softc *sc = arg;

	sc->sc_blinking = 0;
}

/*
 * Turn the activity LED off: flip the pin and then set a timer so no
 * update will happen for the specified duration.
 */
static void
iwi_led_off(void *arg)
{
	struct iwi_softc *sc = arg;
	uint32_t v;

	v = iwi_read_event(sc);
	v &= ~sc->sc_ledpin;
	iwi_write_event(sc, iwi_toggle_event(v));
	callout_reset(&sc->sc_ledtimer, sc->sc_ledoff, iwi_led_done, sc);
}

/*
 * Blink the LED according to the specified on/off times.
 */
static void
iwi_led_blink(struct iwi_softc *sc, int on, int off)
{
	uint32_t v;

	v = iwi_read_event(sc);
	v |= sc->sc_ledpin;
	iwi_write_event(sc, iwi_toggle_event(v));
	sc->sc_blinking = 1;
	sc->sc_ledoff = off;
	callout_reset(&sc->sc_ledtimer, on, iwi_led_off, sc);
}

static void
iwi_led_event(struct iwi_softc *sc, int event)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	/* NB: on/off times from the Atheros NDIS driver, w/ permission */
	static const struct {
		u_int		rate;		/* tx/rx iwi rate */
		u_int16_t	timeOn;		/* LED on time (ms) */
		u_int16_t	timeOff;	/* LED off time (ms) */
	} blinkrates[] = {
		{ IWI_RATE_OFDM54, 40,  10 },
		{ IWI_RATE_OFDM48, 44,  11 },
		{ IWI_RATE_OFDM36, 50,  13 },
		{ IWI_RATE_OFDM24, 57,  14 },
		{ IWI_RATE_OFDM18, 67,  16 },
		{ IWI_RATE_OFDM12, 80,  20 },
		{ IWI_RATE_DS11,  100,  25 },
		{ IWI_RATE_OFDM9, 133,  34 },
		{ IWI_RATE_OFDM6, 160,  40 },
		{ IWI_RATE_DS5,   200,  50 },
		{            6,   240,  58 },	/* XXX 3Mb/s if it existed */
		{ IWI_RATE_DS2,   267,  66 },
		{ IWI_RATE_DS1,   400, 100 },
		{            0,   500, 130 },	/* unknown rate/polling */
	};
	uint32_t txrate;
	int j = 0;			/* XXX silence compiler */

	sc->sc_ledevent = ticks;	/* time of last event */
	if (sc->sc_blinking)		/* don't interrupt active blink */
		return;
	switch (event) {
	case IWI_LED_POLL:
		j = N(blinkrates)-1;
		break;
	case IWI_LED_TX:
		/* read current transmission rate from adapter */
		txrate = CSR_READ_4(sc, IWI_CSR_CURRENT_TX_RATE);
		if (blinkrates[sc->sc_txrix].rate != txrate) {
			for (j = 0; j < N(blinkrates)-1; j++)
				if (blinkrates[j].rate == txrate)
					break;
			sc->sc_txrix = j;
		} else
			j = sc->sc_txrix;
		break;
	case IWI_LED_RX:
		if (blinkrates[sc->sc_rxrix].rate != sc->sc_rxrate) {
			for (j = 0; j < N(blinkrates)-1; j++)
				if (blinkrates[j].rate == sc->sc_rxrate)
					break;
			sc->sc_rxrix = j;
		} else
			j = sc->sc_rxrix;
		break;
	}
	/* XXX beware of overflow */
	iwi_led_blink(sc, (blinkrates[j].timeOn * hz) / 1000,
		(blinkrates[j].timeOff * hz) / 1000);
#undef N
}

static int
iwi_sysctl_softled(SYSCTL_HANDLER_ARGS)
{
	struct iwi_softc *sc = arg1;
	int softled = sc->sc_softled;
	int error;

	error = sysctl_handle_int(oidp, &softled, 0, req);
	if (error || !req->newptr)
		return error;
	softled = (softled != 0);
	if (softled != sc->sc_softled) {
		if (softled) {
			uint32_t v = iwi_read_event(sc);
			v &= ~sc->sc_ledpin;
			iwi_write_event(sc, iwi_toggle_event(v));
		}
		sc->sc_softled = softled;
	}
	return 0;
}

static void
iwi_ledattach(struct iwi_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

	sc->sc_blinking = 0;
	sc->sc_ledstate = 1;
	sc->sc_ledidle = (2700*hz)/1000;	/* 2.7sec */
	callout_init_mtx(&sc->sc_ledtimer, &sc->sc_mtx, 0);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"softled", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
		iwi_sysctl_softled, "I", "enable/disable software LED support");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"ledpin", CTLFLAG_RW, &sc->sc_ledpin, 0,
		"pin setting to turn activity LED on");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"ledidle", CTLFLAG_RW, &sc->sc_ledidle, 0,
		"idle time for inactivity LED (ticks)");
	/* XXX for debugging */
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		"nictype", CTLFLAG_RD, &sc->sc_nictype, 0,
		"NIC type from EEPROM");

	sc->sc_ledpin = IWI_RST_LED_ACTIVITY;
	sc->sc_softled = 1;

	sc->sc_nictype = (iwi_read_prom_word(sc, IWI_EEPROM_NIC) >> 8) & 0xff;
	if (sc->sc_nictype == 1) {
		/*
		 * NB: led's are reversed.
		 */
		sc->sc_ledpin = IWI_RST_LED_ASSOCIATED;
	}
}

static void
iwi_ops(void *arg, int npending)
{
	struct iwi_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	IWI_LOCK_DECL;
	int cmd;

again:
	IWI_CMD_LOCK(sc);
	cmd = sc->sc_cmd[sc->sc_cmd_cur];
	if (cmd == 0) {
		/* No more commands to process */
		IWI_CMD_UNLOCK(sc);
		return;
	}
	sc->sc_cmd[sc->sc_cmd_cur] = 0;	/* free the slot */
	sc->sc_cmd_cur = (sc->sc_cmd_cur + 1) % IWI_CMD_MAXOPS;
	IWI_CMD_UNLOCK(sc);

	IWI_LOCK(sc);
	while  (sc->fw_state != IWI_FW_IDLE || (sc->flags & IWI_FLAG_BUSY)) {
		msleep(sc, &sc->sc_mtx, 0, "iwicmd", hz/10);
	}

	if (!(sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		IWI_UNLOCK(sc);
		return;
	}

	switch (cmd) {
	case IWI_ASSOC:
		iwi_auth_and_assoc(sc);
		break;
	case IWI_DISASSOC:
		iwi_disassociate(sc, 0);
		break;
	case IWI_SET_WME:
		if (ic->ic_state == IEEE80211_S_RUN)
			(void) iwi_wme_setparams(sc);
		break;
	case IWI_SCAN_START:
		sc->flags |= IWI_FLAG_CHANNEL_SCAN;
		break;
	case IWI_SCAN_CURCHAN:
	case IWI_SCAN_ALLCHAN:
		if (!(sc->flags & IWI_FLAG_CHANNEL_SCAN)) {
			DPRINTF(("%s: ic_scan_curchan while not scanning\n",
			    __func__));
			goto done;
		}
		if (iwi_scanchan(sc, sc->sc_maxdwell, cmd))
			ieee80211_cancel_scan(ic);

		break;
	}
done:
	IWI_UNLOCK(sc);

	/* Take another pass */
	goto again;
}

static int
iwi_queue_cmd(struct iwi_softc *sc, int cmd)
{
	IWI_CMD_LOCK(sc);
	if (sc->sc_cmd[sc->sc_cmd_next] != 0) {
		IWI_CMD_UNLOCK(sc);
		DPRINTF(("%s: command %d dropped\n", __func__, cmd));
		return (EBUSY);
	}

	sc->sc_cmd[sc->sc_cmd_next] = cmd;
	sc->sc_cmd_next = (sc->sc_cmd_next + 1) % IWI_CMD_MAXOPS;
	taskqueue_enqueue(sc->sc_tq, &sc->sc_opstask);
	IWI_CMD_UNLOCK(sc);
	return (0);
}

static void
iwi_scan_start(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_softc *sc = ifp->if_softc;

	iwi_queue_cmd(sc, IWI_SCAN_START);
}

static void
iwi_set_channel(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_softc *sc = ifp->if_softc;
	if (sc->fw_state == IWI_FW_IDLE)
		iwi_setcurchan(sc, ic->ic_curchan->ic_ieee);
}

static void
iwi_scan_curchan(struct ieee80211com *ic, unsigned long maxdwell)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_softc *sc = ifp->if_softc;

	sc->sc_maxdwell = maxdwell;
	iwi_queue_cmd(sc, IWI_SCAN_CURCHAN);
}

#if 0
static void
iwi_scan_allchan(struct ieee80211com *ic, unsigned long maxdwell)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_softc *sc = ifp->if_softc;

	sc->sc_maxdwell = maxdwell;
	iwi_queue_cmd(sc, IWI_SCAN_ALLCHAN);
}
#endif

static void
iwi_scan_mindwell(struct ieee80211com *ic)
{
	/* NB: don't try to abort scan; wait for firmware to finish */
}

static void
iwi_scan_end(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_softc *sc = ifp->if_softc;

	taskqueue_enqueue(sc->sc_tq2, &sc->sc_scanaborttask);
}

static void
iwi_assoc(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_softc *sc = ifp->if_softc;

	/* The firmware will fail if we are already associated */
	if (sc->flags & IWI_FLAG_ASSOCIATED)
		iwi_disassoc(ic);

	iwi_queue_cmd(sc, IWI_ASSOC);
}

static void
iwi_disassoc(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwi_softc *sc = ifp->if_softc;

	iwi_queue_cmd(sc, IWI_DISASSOC);
}
