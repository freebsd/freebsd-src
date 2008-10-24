/*-
 * Copyright (c) 2007
 *	Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2008
 *	Benjamin Close <benjsc@FreeBSD.org>
 * Copyright (c) 2008 Sam Leffler, Errno Consulting
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

/*
 * Driver for Intel Wireless WiFi Link 4965AGN 802.11 network adapters.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/endian.h>
#include <sys/firmware.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>

#include <dev/iwn/if_iwnreg.h>
#include <dev/iwn/if_iwnvar.h>

static int	iwn_probe(device_t);
static int	iwn_attach(device_t);
static int 	iwn_detach(device_t);
static int	iwn_cleanup(device_t);
static struct ieee80211vap *iwn_vap_create(struct ieee80211com *,
		    const char name[IFNAMSIZ], int unit, int opmode,
		    int flags, const uint8_t bssid[IEEE80211_ADDR_LEN],
		    const uint8_t mac[IEEE80211_ADDR_LEN]);
static void	iwn_vap_delete(struct ieee80211vap *);
static int	iwn_shutdown(device_t);
static int	iwn_suspend(device_t);
static int	iwn_resume(device_t);
static int	iwn_dma_contig_alloc(struct iwn_softc *, struct iwn_dma_info *,
		    void **, bus_size_t, bus_size_t, int);
static void	iwn_dma_contig_free(struct iwn_dma_info *);
int		iwn_alloc_shared(struct iwn_softc *);
void		iwn_free_shared(struct iwn_softc *);
int		iwn_alloc_kw(struct iwn_softc *);
void		iwn_free_kw(struct iwn_softc *);
int		iwn_alloc_fwmem(struct iwn_softc *);
void		iwn_free_fwmem(struct iwn_softc *);
struct		iwn_rbuf *iwn_alloc_rbuf(struct iwn_softc *);
void		iwn_free_rbuf(void *, void *);
int		iwn_alloc_rpool(struct iwn_softc *);
void		iwn_free_rpool(struct iwn_softc *);
int		iwn_alloc_rx_ring(struct iwn_softc *, struct iwn_rx_ring *);
void		iwn_reset_rx_ring(struct iwn_softc *, struct iwn_rx_ring *);
void		iwn_free_rx_ring(struct iwn_softc *, struct iwn_rx_ring *);
int		iwn_alloc_tx_ring(struct iwn_softc *, struct iwn_tx_ring *,
		    int);
void		iwn_reset_tx_ring(struct iwn_softc *, struct iwn_tx_ring *);
void		iwn_free_tx_ring(struct iwn_softc *, struct iwn_tx_ring *);
static struct ieee80211_node *iwn_node_alloc(struct ieee80211vap *,
		    const uint8_t [IEEE80211_ADDR_LEN]);
void		iwn_newassoc(struct ieee80211_node *, int);
int		iwn_media_change(struct ifnet *);
int		iwn_newstate(struct ieee80211vap *, enum ieee80211_state, int);
void		iwn_mem_lock(struct iwn_softc *);
void		iwn_mem_unlock(struct iwn_softc *);
uint32_t	iwn_mem_read(struct iwn_softc *, uint32_t);
void		iwn_mem_write(struct iwn_softc *, uint32_t, uint32_t);
void		iwn_mem_write_region_4(struct iwn_softc *, uint32_t,
		    const uint32_t *, int);
int		iwn_eeprom_lock(struct iwn_softc *);
void		iwn_eeprom_unlock(struct iwn_softc *);
int		iwn_read_prom_data(struct iwn_softc *, uint32_t, void *, int);
int		iwn_transfer_microcode(struct iwn_softc *, const uint8_t *, int);
int		iwn_transfer_firmware(struct iwn_softc *);
int		iwn_load_firmware(struct iwn_softc *);
void		iwn_unload_firmware(struct iwn_softc *);
static void	iwn_timer_timeout(void *);
static void	iwn_calib_reset(struct iwn_softc *);
void		iwn_ampdu_rx_start(struct iwn_softc *, struct iwn_rx_desc *);
void		iwn_rx_intr(struct iwn_softc *, struct iwn_rx_desc *,
		    struct iwn_rx_data *);
void		iwn_rx_statistics(struct iwn_softc *, struct iwn_rx_desc *);
void		iwn_tx_intr(struct iwn_softc *, struct iwn_rx_desc *);
void		iwn_cmd_intr(struct iwn_softc *, struct iwn_rx_desc *);
static void	iwn_bmiss(void *, int);
void		iwn_notif_intr(struct iwn_softc *);
void		iwn_intr(void *);
void		iwn_read_eeprom(struct iwn_softc *);
static void	iwn_read_eeprom_channels(struct iwn_softc *);
void		iwn_print_power_group(struct iwn_softc *, int);
uint8_t		iwn_plcp_signal(int);
int		iwn_tx_data(struct iwn_softc *, struct mbuf *,
		    struct ieee80211_node *, struct iwn_tx_ring *);
void		iwn_start(struct ifnet *);
void		iwn_start_locked(struct ifnet *);
static int	iwn_raw_xmit(struct ieee80211_node *, struct mbuf *,
		    const struct ieee80211_bpf_params *);
static void	iwn_watchdog(struct iwn_softc *);
int		iwn_ioctl(struct ifnet *, u_long, caddr_t);
int		iwn_cmd(struct iwn_softc *, int, const void *, int, int);
int		iwn_set_link_quality(struct iwn_softc *, uint8_t,
		    const struct ieee80211_channel *, int);
int		iwn_set_key(struct ieee80211com *, struct ieee80211_node *,
		    const struct ieee80211_key *);
int		iwn_wme_update(struct ieee80211com *);
void		iwn_set_led(struct iwn_softc *, uint8_t, uint8_t, uint8_t);
int		iwn_set_critical_temp(struct iwn_softc *);
void		iwn_enable_tsf(struct iwn_softc *, struct ieee80211_node *);
void		iwn_power_calibration(struct iwn_softc *, int);
int		iwn_set_txpower(struct iwn_softc *,
		    struct ieee80211_channel *, int);
int8_t		iwn_get_rssi(struct iwn_softc *, const struct iwn_rx_stat *);
int		iwn_get_noise(const struct iwn_rx_general_stats *);
int		iwn_get_temperature(struct iwn_softc *);
int		iwn_init_sensitivity(struct iwn_softc *);
void		iwn_compute_differential_gain(struct iwn_softc *,
		    const struct iwn_rx_general_stats *);
void		iwn_tune_sensitivity(struct iwn_softc *,
		    const struct iwn_rx_stats *);
int		iwn_send_sensitivity(struct iwn_softc *);
int		iwn_auth(struct iwn_softc *);
int		iwn_run(struct iwn_softc *);
int		iwn_scan(struct iwn_softc *);
int		iwn_config(struct iwn_softc *);
void		iwn_post_alive(struct iwn_softc *);
void		iwn_stop_master(struct iwn_softc *);
int		iwn_reset(struct iwn_softc *);
void		iwn_hw_config(struct iwn_softc *);
void		iwn_init_locked(struct iwn_softc *);
void		iwn_init(void *);
void		iwn_stop_locked(struct iwn_softc *);
void		iwn_stop(struct iwn_softc *);
static void 	iwn_scan_start(struct ieee80211com *);
static void 	iwn_scan_end(struct ieee80211com *);
static void 	iwn_set_channel(struct ieee80211com *);
static void 	iwn_scan_curchan(struct ieee80211_scan_state *, unsigned long);
static void 	iwn_scan_mindwell(struct ieee80211_scan_state *);
static void 	iwn_ops(void *, int);
static int 	iwn_queue_cmd( struct iwn_softc *, int, int, int);
static void	iwn_bpfattach(struct iwn_softc *);
static void	iwn_sysctlattach(struct iwn_softc *);

#define IWN_DEBUG
#ifdef IWN_DEBUG
enum {
	IWN_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	IWN_DEBUG_RECV		= 0x00000002,	/* basic recv operation */
	IWN_DEBUG_STATE		= 0x00000004,	/* 802.11 state transitions */
	IWN_DEBUG_TXPOW		= 0x00000008,	/* tx power processing */
	IWN_DEBUG_RESET		= 0x00000010,	/* reset processing */
	IWN_DEBUG_OPS		= 0x00000020,	/* iwn_ops processing */
	IWN_DEBUG_BEACON 	= 0x00000040,	/* beacon handling */
	IWN_DEBUG_WATCHDOG 	= 0x00000080,	/* watchdog timeout */
	IWN_DEBUG_INTR		= 0x00000100,	/* ISR */
	IWN_DEBUG_CALIBRATE	= 0x00000200,	/* periodic calibration */
	IWN_DEBUG_NODE		= 0x00000400,	/* node management */
	IWN_DEBUG_LED		= 0x00000800,	/* led management */
	IWN_DEBUG_CMD		= 0x00001000,	/* cmd submission */
	IWN_DEBUG_FATAL		= 0x80000000,	/* fatal errors */
	IWN_DEBUG_ANY		= 0xffffffff
};

#define DPRINTF(sc, m, fmt, ...) do {			\
	if (sc->sc_debug & (m))				\
		printf(fmt, __VA_ARGS__);		\
} while (0)

static const char *iwn_ops_str(int);
static const char *iwn_intr_str(uint8_t);
#else
#define DPRINTF(sc, m, fmt, ...) do { (void) sc; } while (0)
#endif

struct iwn_ident {
	uint16_t	vendor;
	uint16_t	device;
	const char	*name;
};

static const struct iwn_ident iwn_ident_table [] = {
        { 0x8086, 0x4229, "Intel(R) PRO/Wireless 4965BGN" },
        { 0x8086, 0x422D, "Intel(R) PRO/Wireless 4965BGN" },
        { 0x8086, 0x4230, "Intel(R) PRO/Wireless 4965BGN" },
        { 0x8086, 0x4233, "Intel(R) PRO/Wireless 4965BGN" },
        { 0, 0, NULL }
};

static int
iwn_probe(device_t dev)
{
        const struct iwn_ident *ident;

        for (ident = iwn_ident_table; ident->name != NULL; ident++) {
                if (pci_get_vendor(dev) == ident->vendor &&
                    pci_get_device(dev) == ident->device) {
                        device_set_desc(dev, ident->name);
                        return 0;
                }
        }
        return ENXIO;
}

static int
iwn_attach(device_t dev)
{
	struct iwn_softc *sc = (struct iwn_softc *)device_get_softc(dev);
	struct ieee80211com *ic;
	struct ifnet *ifp;
	int i, error, result;

	sc->sc_dev = dev;

	/* XXX */
	if (pci_get_powerstate(dev) != PCI_POWERSTATE_D0) {
		device_printf(dev, "chip is in D%d power mode "
		    "-- setting to D0\n", pci_get_powerstate(dev));
		pci_set_powerstate(dev, PCI_POWERSTATE_D0);
	}

	/* clear device specific PCI configuration register 0x41 */
	pci_write_config(dev, 0x41, 0, 1);

	/* enable bus-mastering */
	pci_enable_busmaster(dev);

	sc->mem_rid= PCIR_BAR(0);
	sc->mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
					 RF_ACTIVE);
	if (sc->mem == NULL ) {
		device_printf(dev, "could not allocate memory resources\n");
		error = ENOMEM; 
		return error;
	}

	sc->sc_st = rman_get_bustag(sc->mem);
	sc->sc_sh = rman_get_bushandle(sc->mem);
	sc->irq_rid = 0;
	if ((result = pci_msi_count(dev)) == 1 &&
	    pci_alloc_msi(dev, &result) == 0)
		sc->irq_rid = 1;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
					 RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq == NULL) {
		device_printf(dev, "could not allocate interrupt resource\n");
		error = ENOMEM;
		return error;
	}

	IWN_LOCK_INIT(sc);
	IWN_CMD_LOCK_INIT(sc);
	callout_init_mtx(&sc->sc_timer_to, &sc->sc_mtx, 0);

        /*
         * Create the taskqueues used by the driver. Primarily
         * sc_tq handles most the task
         */
        sc->sc_tq = taskqueue_create("iwn_taskq", M_NOWAIT | M_ZERO,
                taskqueue_thread_enqueue, &sc->sc_tq);
        taskqueue_start_threads(&sc->sc_tq, 1, PI_NET, "%s taskq",
                device_get_nameunit(dev));

        TASK_INIT(&sc->sc_ops_task, 0, iwn_ops, sc );
	TASK_INIT(&sc->sc_bmiss_task, 0, iwn_bmiss, sc);

	/*
	 * Put adapter into a known state.
	 */
	error = iwn_reset(sc);
	if (error != 0) {
		device_printf(dev,
		    "could not reset adapter, error %d\n", error);
		goto fail;
	}

	/*
	 * Allocate DMA memory for firmware transfers.
	 */
	error = iwn_alloc_fwmem(sc);
	if (error != 0) {
		device_printf(dev,
		    "could not allocate firmware memory, error %d\n", error);
		goto fail;
	}

	/*
	 * Allocate a "keep warm" page.
	 */
	error = iwn_alloc_kw(sc);
	if (error != 0) {
		device_printf(dev,
		    "could not allocate keep-warm page, error %d\n", error);
		goto fail;
	}

	/*
	 * Allocate shared area (communication area).
	 */
	error = iwn_alloc_shared(sc);
	if (error != 0) {
		device_printf(dev,
		    "could not allocate shared area, error %d\n", error);
		goto fail;
	}

	/*
	 * Allocate Tx rings.
	 */
	for (i = 0; i < IWN_NTXQUEUES; i++) {
		error = iwn_alloc_tx_ring(sc, &sc->txq[i], i);
		if (error != 0) {
			device_printf(dev,
			    "could not allocate Tx ring %d, error %d\n",
			    i, error);
			goto fail;
		}
	}

	error = iwn_alloc_rx_ring(sc, &sc->rxq);
	if (error != 0 ){
		device_printf(dev,
		    "could not allocate Rx ring, error %d\n", error);
		goto fail;
	}

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
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
		| IEEE80211_C_WPA
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
#if 0
		| IEEE80211_C_BGSCAN		/* background scanning */
		| IEEE80211_C_IBSS		/* ibss/adhoc mode */
#endif
		| IEEE80211_C_WME		/* WME */
		;
#if 0
	/* XXX disable until HT channel setup works */
	ic->ic_htcaps =
		  IEEE80211_HTCAP_SMPS_ENA	/* SM PS mode enabled */
		| IEEE80211_HTCAP_CHWIDTH40	/* 40MHz channel width */
		| IEEE80211_HTCAP_SHORTGI20	/* short GI in 20MHz */
		| IEEE80211_HTCAP_SHORTGI40	/* short GI in 40MHz */
		| IEEE80211_HTCAP_RXSTBC_2STREAM/* 1-2 spatial streams */
		| IEEE80211_HTCAP_MAXAMSDU_3839	/* max A-MSDU length */
		/* s/w capabilities */
		| IEEE80211_HTC_HT		/* HT operation */
		| IEEE80211_HTC_AMPDU		/* tx A-MPDU */
		| IEEE80211_HTC_AMSDU		/* tx A-MSDU */
		;
#endif
	/* read supported channels and MAC address from EEPROM */
	iwn_read_eeprom(sc);

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = iwn_init;
	ifp->if_ioctl = iwn_ioctl;
	ifp->if_start = iwn_start;
        IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ieee80211_ifattach(ic);
	ic->ic_vap_create = iwn_vap_create;
	ic->ic_vap_delete = iwn_vap_delete;
	ic->ic_raw_xmit = iwn_raw_xmit;
	ic->ic_node_alloc = iwn_node_alloc;
	ic->ic_newassoc = iwn_newassoc;
        ic->ic_wme.wme_update = iwn_wme_update;
        ic->ic_scan_start = iwn_scan_start;
        ic->ic_scan_end = iwn_scan_end;
        ic->ic_set_channel = iwn_set_channel;
        ic->ic_scan_curchan = iwn_scan_curchan;
        ic->ic_scan_mindwell = iwn_scan_mindwell;

	iwn_bpfattach(sc);
	iwn_sysctlattach(sc);

        /*
         * Hook our interrupt after all initialization is complete.
         */
        error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, iwn_intr, sc, &sc->sc_ih);
        if (error != 0) {
                device_printf(dev, "could not set up interrupt, error %d\n", error);
                goto fail;
        }

        ieee80211_announce(ic);
	return 0;
fail:
	iwn_cleanup(dev);
	return error;
}

static int
iwn_detach(device_t dev)
{
	iwn_cleanup(dev);
        return 0;
}

/*
 * Cleanup any device resources that were allocated
 */
int
iwn_cleanup(device_t dev)
{
	struct iwn_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	int i;

	if (ifp != NULL) {
		iwn_stop(sc);
		callout_drain(&sc->sc_timer_to);
		bpfdetach(ifp);
		ieee80211_ifdetach(ic);
	}

	iwn_unload_firmware(sc);

	iwn_free_rx_ring(sc, &sc->rxq);
	for (i = 0; i < IWN_NTXQUEUES; i++)
		iwn_free_tx_ring(sc, &sc->txq[i]);
	iwn_free_kw(sc);
	iwn_free_fwmem(sc);
	if (sc->irq != NULL) {
		bus_teardown_intr(dev, sc->irq, sc->sc_ih);
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, sc->irq);
		if (sc->irq_rid == 1)
			pci_release_msi(dev);
	}
	if (sc->mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, sc->mem);
	if (ifp != NULL)
		if_free(ifp);
	taskqueue_free(sc->sc_tq);
	IWN_CMD_LOCK_DESTROY(sc);
	IWN_LOCK_DESTROY(sc);
	return 0;
}

static struct ieee80211vap *
iwn_vap_create(struct ieee80211com *ic,
	const char name[IFNAMSIZ], int unit, int opmode, int flags,
	const uint8_t bssid[IEEE80211_ADDR_LEN],
	const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct iwn_vap *ivp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))		/* only one at a time */
		return NULL;
	ivp = (struct iwn_vap *) malloc(sizeof(struct iwn_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (ivp == NULL)
		return NULL;
	vap = &ivp->iv_vap;
	ieee80211_vap_setup(ic, vap, name, unit, opmode, flags, bssid, mac);
	vap->iv_bmissthreshold = 10;		/* override default */
	/* override with driver methods */
	ivp->iv_newstate = vap->iv_newstate;
	vap->iv_newstate = iwn_newstate;

	ieee80211_amrr_init(&ivp->iv_amrr, vap,
	    IEEE80211_AMRR_MIN_SUCCESS_THRESHOLD,
	    IEEE80211_AMRR_MAX_SUCCESS_THRESHOLD,
	    500 /*ms*/);

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change, ieee80211_media_status);
	ic->ic_opmode = opmode;
	return vap;
}

static void
iwn_vap_delete(struct ieee80211vap *vap)
{
	struct iwn_vap *ivp = IWN_VAP(vap);

	ieee80211_amrr_cleanup(&ivp->iv_amrr);
	ieee80211_vap_detach(vap);
	free(ivp, M_80211_VAP);
}

static int
iwn_shutdown(device_t dev)
{
	struct iwn_softc *sc = device_get_softc(dev);

	iwn_stop(sc);
	return 0;
}

static int
iwn_suspend(device_t dev)
{
	struct iwn_softc *sc = device_get_softc(dev);

	iwn_stop(sc);
	return 0;
}

static int
iwn_resume(device_t dev)
{
	struct iwn_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;

	pci_write_config(dev, 0x41, 0, 1);

	if (ifp->if_flags & IFF_UP)
		iwn_init(sc);
	return 0;
}

static void
iwn_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
        if (error != 0)
                return;
        KASSERT(nsegs == 1, ("too many DMA segments, %d should be 1", nsegs));
        *(bus_addr_t *)arg = segs[0].ds_addr;
}

static int 
iwn_dma_contig_alloc(struct iwn_softc *sc, struct iwn_dma_info *dma,
	void **kvap, bus_size_t size, bus_size_t alignment, int flags)
{
	int error, lalignment, i;

	/*
	 * FreeBSD can't guarrenty 16k alignment at the moment (11/2007) so
	 * we allocate an extra 12k with 4k alignement and walk through
	 * it trying to find where the alignment is. It's a nasty fix for
	 * a bigger problem.
	*/
	DPRINTF(sc, IWN_DEBUG_RESET,
	    "Size: %zd - alignment %zd\n", size, alignment);
	if (alignment == 0x4000) {
		size += 12*1024;
		lalignment = 4096;
		DPRINTF(sc, IWN_DEBUG_RESET, "%s\n",
		    "Attempting to find a 16k boundary");
	} else
		lalignment = alignment;
	dma->size = size;
	dma->tag = NULL;

	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), lalignment,
	    0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, size,
	    1, size, flags, NULL, NULL, &dma->tag);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: bus_dma_tag_create failed, error %d\n",
		    __func__, error);
		goto fail;
	}
	error = bus_dmamem_alloc(dma->tag, (void **)&dma->vaddr,
	    flags | BUS_DMA_ZERO, &dma->map);
	if (error != 0) {
		device_printf(sc->sc_dev,
		   "%s: bus_dmamem_alloc failed, error %d\n",
		   __func__, error);
		goto fail;
	}
	if (alignment == 0x4000) {
		for (i = 0; i < 3 && (((uintptr_t)dma->vaddr) & 0x3fff); i++) {
			DPRINTF(sc, IWN_DEBUG_RESET,  "%s\n",
			    "Memory Unaligned, shifting pointer by 4k");
			dma->vaddr += 4096;
			size -= 4096;
		}
		if ((((uintptr_t)dma->vaddr ) & (alignment-1))) {
			DPRINTF(sc, IWN_DEBUG_ANY,
			    "%s: failed to align memory, vaddr %p, align %zd\n",
			    __func__, dma->vaddr, alignment);
			error = ENOMEM;
			goto fail;
		}
	}

	error = bus_dmamap_load(dma->tag, dma->map, dma->vaddr,
	    size, iwn_dma_map_addr, &dma->paddr, flags);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: bus_dmamap_load failed, error %d\n", __func__, error);
		goto fail;
	}

	if (kvap != NULL)
		*kvap = dma->vaddr;
	return 0;
fail:
	iwn_dma_contig_free(dma);
	return error;
}

static void
iwn_dma_contig_free(struct iwn_dma_info *dma)
{
	if (dma->tag != NULL) {
		if (dma->map != NULL) {
			if (dma->paddr == 0) {
				bus_dmamap_sync(dma->tag, dma->map,
				    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(dma->tag, dma->map);
			}
			bus_dmamem_free(dma->tag, &dma->vaddr, dma->map);
		}
		bus_dma_tag_destroy(dma->tag);
	}
}

int
iwn_alloc_shared(struct iwn_softc *sc)
{
	/* must be aligned on a 1KB boundary */
	return iwn_dma_contig_alloc(sc, &sc->shared_dma,
	    (void **)&sc->shared, sizeof (struct iwn_shared), 1024,
	    BUS_DMA_NOWAIT);
}

void
iwn_free_shared(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->shared_dma);
}

int
iwn_alloc_kw(struct iwn_softc *sc)
{
	/* must be aligned on a 4k boundary */
	return iwn_dma_contig_alloc(sc, &sc->kw_dma, NULL,
	    PAGE_SIZE, PAGE_SIZE, BUS_DMA_NOWAIT);
}

void
iwn_free_kw(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->kw_dma);
}

int
iwn_alloc_fwmem(struct iwn_softc *sc)
{
	/* allocate enough contiguous space to store text and data */
	return iwn_dma_contig_alloc(sc, &sc->fw_dma, NULL,
	    IWN_FW_MAIN_TEXT_MAXSZ + IWN_FW_MAIN_DATA_MAXSZ, 16,
	    BUS_DMA_NOWAIT);
}

void
iwn_free_fwmem(struct iwn_softc *sc)
{
	iwn_dma_contig_free(&sc->fw_dma);
}

int
iwn_alloc_rx_ring(struct iwn_softc *sc, struct iwn_rx_ring *ring)
{
	int i, error;

	ring->cur = 0;

	error = iwn_dma_contig_alloc(sc, &ring->desc_dma,
	    (void **)&ring->desc, IWN_RX_RING_COUNT * sizeof (uint32_t),
	    IWN_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
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
	for (i = 0; i < IWN_RX_RING_COUNT; i++) {
		struct iwn_rx_data *data = &ring->data[i];
		struct mbuf *m;
		bus_addr_t paddr;

		error = bus_dmamap_create(ring->data_dmat, 0, &data->map);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "%s: bus_dmamap_create failed, error %d\n",
			    __func__, error);
			goto fail;
		}
		m = m_getjcl(M_DONTWAIT, MT_DATA, M_PKTHDR, MJUMPAGESIZE);
		if (m == NULL) {
			device_printf(sc->sc_dev,
			   "%s: could not allocate rx mbuf\n", __func__);
			error = ENOMEM;
			goto fail;
		}
		/* map page */
		error = bus_dmamap_load(ring->data_dmat, data->map,
		    mtod(m, caddr_t), MJUMPAGESIZE,
		    iwn_dma_map_addr, &paddr, BUS_DMA_NOWAIT);
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
		/* Rx buffers are aligned on a 256-byte boundary */
		ring->desc[i] = htole32(paddr >> 8);
	}
	bus_dmamap_sync(ring->desc_dma.tag, ring->desc_dma.map,
	    BUS_DMASYNC_PREWRITE);
	return 0;
fail:
	iwn_free_rx_ring(sc, ring);
	return error;
}

void
iwn_reset_rx_ring(struct iwn_softc *sc, struct iwn_rx_ring *ring)
{
	int ntries;

	iwn_mem_lock(sc);

	IWN_WRITE(sc, IWN_RX_CONFIG, 0);
	for (ntries = 0; ntries < 100; ntries++) {
		if (IWN_READ(sc, IWN_RX_STATUS) & IWN_RX_IDLE)
			break;
		DELAY(10);
	}
#ifdef IWN_DEBUG
	if (ntries == 100)
		DPRINTF(sc, IWN_DEBUG_ANY, "%s\n", "timeout resetting Rx ring");
#endif
	iwn_mem_unlock(sc);

	ring->cur = 0;
}

void
iwn_free_rx_ring(struct iwn_softc *sc, struct iwn_rx_ring *ring)
{
	int i;

	iwn_dma_contig_free(&ring->desc_dma);

	for (i = 0; i < IWN_RX_RING_COUNT; i++)
		if (ring->data[i].m != NULL)
			m_freem(ring->data[i].m);
}

int
iwn_alloc_tx_ring(struct iwn_softc *sc, struct iwn_tx_ring *ring, int qid)
{
	bus_size_t size;
	int i, error;

	ring->qid = qid;
	ring->queued = 0;
	ring->cur = 0;

	size = IWN_TX_RING_COUNT * sizeof(struct iwn_tx_desc);
	error = iwn_dma_contig_alloc(sc, &ring->desc_dma,
	    (void **)&ring->desc, size, IWN_RING_DMA_ALIGN, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not allocate tx ring DMA memory, error %d\n",
		    __func__, error);
		goto fail;
	}

	size = IWN_TX_RING_COUNT * sizeof(struct iwn_tx_cmd);
	error = iwn_dma_contig_alloc(sc, &ring->cmd_dma,
	    (void **)&ring->cmd, size, 4, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not allocate tx cmd DMA memory, error %d\n",
		    __func__, error);
		goto fail;
	}

        error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev), 1, 0, 
	    BUS_SPACE_MAXADDR_32BIT,
            BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, IWN_MAX_SCATTER - 1,
            MCLBYTES, BUS_DMA_NOWAIT, NULL, NULL, &ring->data_dmat);
        if (error != 0) {
                device_printf(sc->sc_dev,
		    "%s: bus_dma_tag_create_failed, error %d\n",
		    __func__, error);
                goto fail;
        }

	for (i = 0; i < IWN_TX_RING_COUNT; i++) {
		struct iwn_tx_data *data = &ring->data[i];

		error = bus_dmamap_create(ring->data_dmat, 0, &data->map);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "%s: bus_dmamap_create failed, error %d\n",
			    __func__, error);
			goto fail;
		}
		bus_dmamap_sync(ring->data_dmat, data->map, 
		    BUS_DMASYNC_PREWRITE);
	}
	return 0;
fail:
	iwn_free_tx_ring(sc, ring);
	return error;
}

void
iwn_reset_tx_ring(struct iwn_softc *sc, struct iwn_tx_ring *ring)
{
	uint32_t tmp;
	int i, ntries;

	iwn_mem_lock(sc);

	IWN_WRITE(sc, IWN_TX_CONFIG(ring->qid), 0);
	for (ntries = 0; ntries < 20; ntries++) {
		tmp = IWN_READ(sc, IWN_TX_STATUS);
		if ((tmp & IWN_TX_IDLE(ring->qid)) == IWN_TX_IDLE(ring->qid))
			break;
		DELAY(10);
	}
#ifdef IWN_DEBUG
	if (ntries == 20)
		DPRINTF(sc, IWN_DEBUG_RESET,
		    "%s: timeout resetting Tx ring %d\n", __func__, ring->qid);
#endif
	iwn_mem_unlock(sc);

	for (i = 0; i < IWN_TX_RING_COUNT; i++) {
		struct iwn_tx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_unload(ring->data_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
	}

	ring->queued = 0;
	ring->cur = 0;
}

void
iwn_free_tx_ring(struct iwn_softc *sc, struct iwn_tx_ring *ring)
{
	int i;

	iwn_dma_contig_free(&ring->desc_dma);
	iwn_dma_contig_free(&ring->cmd_dma);

	if (ring->data != NULL) {
		for (i = 0; i < IWN_TX_RING_COUNT; i++) {
			struct iwn_tx_data *data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_unload(ring->data_dmat, data->map);
				m_freem(data->m);
			}
		}
	}
}

struct ieee80211_node *
iwn_node_alloc(struct ieee80211vap *vap, const uint8_t mac[IEEE80211_ADDR_LEN])
{
	return malloc(sizeof (struct iwn_node), M_80211_NODE,M_NOWAIT | M_ZERO);
}

void
iwn_newassoc(struct ieee80211_node *ni, int isnew)
{
	struct ieee80211vap *vap = ni->ni_vap;

	ieee80211_amrr_node_init(&IWN_VAP(vap)->iv_amrr,
	   &IWN_NODE(ni)->amn, ni);
}

int
iwn_media_change(struct ifnet *ifp)
{
	int error = ieee80211_media_change(ifp);
	/* NB: only the fixed rate can change and that doesn't need a reset */
	return (error == ENETRESET ? 0 : error);
}

int
iwn_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct iwn_vap *ivp = IWN_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct iwn_softc *sc = ic->ic_ifp->if_softc;
	int error;

	DPRINTF(sc, IWN_DEBUG_STATE, "%s: %s -> %s\n", __func__,
		ieee80211_state_name[vap->iv_state],
		ieee80211_state_name[nstate]);

	IWN_LOCK(sc);
	callout_stop(&sc->sc_timer_to);
	IWN_UNLOCK(sc);

	/*
	 * Some state transitions require issuing a configure request
	 * to the adapter.  This must be done in a blocking context
	 * so we toss control to the task q thread where the state
	 * change will be finished after the command completes.
	 */
	if (nstate == IEEE80211_S_AUTH && vap->iv_state != IEEE80211_S_AUTH) {
		/* !AUTH -> AUTH requires adapter config */
		error = iwn_queue_cmd(sc, IWN_AUTH, arg, IWN_QUEUE_NORMAL);
		return (error != 0 ? error : EINPROGRESS);
	}
	if (nstate == IEEE80211_S_RUN && vap->iv_state != IEEE80211_S_RUN) {
		/*
		 * !RUN -> RUN requires setting the association id
		 * which is done with a firmware cmd.  We also defer
		 * starting the timers until that work is done.
		 */
		error = iwn_queue_cmd(sc, IWN_RUN, arg, IWN_QUEUE_NORMAL);
		return (error != 0 ? error : EINPROGRESS);
	}
	if (nstate == IEEE80211_S_RUN) {
		/*
		 * RUN -> RUN transition; just restart the timers.
		 */
		iwn_calib_reset(sc);
	}
	return ivp->iv_newstate(vap, nstate, arg);
}

/*
 * Grab exclusive access to NIC memory.
 */
void
iwn_mem_lock(struct iwn_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = IWN_READ(sc, IWN_GPIO_CTL);
	IWN_WRITE(sc, IWN_GPIO_CTL, tmp | IWN_GPIO_MAC);

	/* spin until we actually get the lock */
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((IWN_READ(sc, IWN_GPIO_CTL) &
		    (IWN_GPIO_CLOCK | IWN_GPIO_SLEEP)) == IWN_GPIO_CLOCK)
			break;
		DELAY(10);
	}
	if (ntries == 1000)
		device_printf(sc->sc_dev,
		    "%s: could not lock memory\n", __func__);
}

/*
 * Release lock on NIC memory.
 */
void
iwn_mem_unlock(struct iwn_softc *sc)
{
	uint32_t tmp = IWN_READ(sc, IWN_GPIO_CTL);
	IWN_WRITE(sc, IWN_GPIO_CTL, tmp & ~IWN_GPIO_MAC);
}

uint32_t
iwn_mem_read(struct iwn_softc *sc, uint32_t addr)
{
	IWN_WRITE(sc, IWN_READ_MEM_ADDR, IWN_MEM_4 | addr);
	return IWN_READ(sc, IWN_READ_MEM_DATA);
}

void
iwn_mem_write(struct iwn_softc *sc, uint32_t addr, uint32_t data)
{
	IWN_WRITE(sc, IWN_WRITE_MEM_ADDR, IWN_MEM_4 | addr);
	IWN_WRITE(sc, IWN_WRITE_MEM_DATA, data);
}

void
iwn_mem_write_region_4(struct iwn_softc *sc, uint32_t addr,
    const uint32_t *data, int wlen)
{
	for (; wlen > 0; wlen--, data++, addr += 4)
		iwn_mem_write(sc, addr, *data);
}

int
iwn_eeprom_lock(struct iwn_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = IWN_READ(sc, IWN_HWCONFIG);
	IWN_WRITE(sc, IWN_HWCONFIG, tmp | IWN_HW_EEPROM_LOCKED);

	/* spin until we actually get the lock */
	for (ntries = 0; ntries < 100; ntries++) {
		if (IWN_READ(sc, IWN_HWCONFIG) & IWN_HW_EEPROM_LOCKED)
			return 0;
		DELAY(10);
	}
	return ETIMEDOUT;
}

void
iwn_eeprom_unlock(struct iwn_softc *sc)
{
	uint32_t tmp = IWN_READ(sc, IWN_HWCONFIG);
	IWN_WRITE(sc, IWN_HWCONFIG, tmp & ~IWN_HW_EEPROM_LOCKED);
}

/*
 * Read `len' bytes from the EEPROM.  We access the EEPROM through the MAC
 * instead of using the traditional bit-bang method.
 */
int
iwn_read_prom_data(struct iwn_softc *sc, uint32_t addr, void *data, int len)
{
	uint8_t *out = data;
	uint32_t val;
	int ntries, tmp;

	iwn_mem_lock(sc);
	for (; len > 0; len -= 2, addr++) {
		IWN_WRITE(sc, IWN_EEPROM_CTL, addr << 2);
		tmp = IWN_READ(sc, IWN_EEPROM_CTL);	
		IWN_WRITE(sc, IWN_EEPROM_CTL, tmp & ~IWN_EEPROM_MSK );

		for (ntries = 0; ntries < 10; ntries++) {
			if ((val = IWN_READ(sc, IWN_EEPROM_CTL)) &
			    IWN_EEPROM_READY)
				break;
			DELAY(5);
		}
		if (ntries == 10) {
			device_printf(sc->sc_dev,"could not read EEPROM\n");
			return ETIMEDOUT;
		}
		*out++ = val >> 16;
		if (len > 1)
			*out++ = val >> 24;
	}
	iwn_mem_unlock(sc);

	return 0;
}

/*
 * The firmware boot code is small and is intended to be copied directly into
 * the NIC internal memory.
 */
int
iwn_transfer_microcode(struct iwn_softc *sc, const uint8_t *ucode, int size)
{
	int ntries;

	size /= sizeof (uint32_t);

	iwn_mem_lock(sc);

	/* copy microcode image into NIC memory */
	iwn_mem_write_region_4(sc, IWN_MEM_UCODE_BASE,
	    (const uint32_t *)ucode, size);

	iwn_mem_write(sc, IWN_MEM_UCODE_SRC, 0);
	iwn_mem_write(sc, IWN_MEM_UCODE_DST, IWN_FW_TEXT);
	iwn_mem_write(sc, IWN_MEM_UCODE_SIZE, size);

	/* run microcode */
	iwn_mem_write(sc, IWN_MEM_UCODE_CTL, IWN_UC_RUN);

	/* wait for transfer to complete */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (!(iwn_mem_read(sc, IWN_MEM_UCODE_CTL) & IWN_UC_RUN))
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		iwn_mem_unlock(sc);
		device_printf(sc->sc_dev,
		    "%s: could not load boot firmware\n", __func__);
		return ETIMEDOUT;
	}
	iwn_mem_write(sc, IWN_MEM_UCODE_CTL, IWN_UC_ENABLE);

	iwn_mem_unlock(sc);

	return 0;
}

int
iwn_load_firmware(struct iwn_softc *sc)
{
	int error;

	KASSERT(sc->fw_fp == NULL, ("firmware already loaded"));

	IWN_UNLOCK(sc);
	/* load firmware image from disk */
	sc->fw_fp = firmware_get("iwnfw");
	if (sc->fw_fp == NULL) {
		device_printf(sc->sc_dev,
		    "%s: could not load firmare image \"iwnfw\"\n", __func__);
		error = EINVAL;
	} else
		error = 0;
	IWN_LOCK(sc);
	return error;
}

int
iwn_transfer_firmware(struct iwn_softc *sc)
{
	struct iwn_dma_info *dma = &sc->fw_dma;
	const struct iwn_firmware_hdr *hdr;
	const uint8_t *init_text, *init_data, *main_text, *main_data;
	const uint8_t *boot_text;
	uint32_t init_textsz, init_datasz, main_textsz, main_datasz;
	uint32_t boot_textsz;
	int error = 0;
	const struct firmware *fp = sc->fw_fp;

	/* extract firmware header information */
	if (fp->datasize < sizeof (struct iwn_firmware_hdr)) {
		device_printf(sc->sc_dev,
		    "%s: truncated firmware header: %zu bytes, expecting %zu\n",
		    __func__, fp->datasize, sizeof (struct iwn_firmware_hdr));
		error = EINVAL;
		goto fail;
	}
	hdr = (const struct iwn_firmware_hdr *)fp->data;
	main_textsz = le32toh(hdr->main_textsz);
	main_datasz = le32toh(hdr->main_datasz);
	init_textsz = le32toh(hdr->init_textsz);
	init_datasz = le32toh(hdr->init_datasz);
	boot_textsz = le32toh(hdr->boot_textsz);

	/* sanity-check firmware segments sizes */
	if (main_textsz > IWN_FW_MAIN_TEXT_MAXSZ ||
	    main_datasz > IWN_FW_MAIN_DATA_MAXSZ ||
	    init_textsz > IWN_FW_INIT_TEXT_MAXSZ ||
	    init_datasz > IWN_FW_INIT_DATA_MAXSZ ||
	    boot_textsz > IWN_FW_BOOT_TEXT_MAXSZ ||
	    (boot_textsz & 3) != 0) {
		device_printf(sc->sc_dev,
		    "%s: invalid firmware header, main [%d,%d], init [%d,%d] "
		    "boot %d\n", __func__, main_textsz, main_datasz,
		    init_textsz, init_datasz, boot_textsz);
		error = EINVAL;
		goto fail;
	}

	/* check that all firmware segments are present */
	if (fp->datasize < sizeof (struct iwn_firmware_hdr) + main_textsz +
	    main_datasz + init_textsz + init_datasz + boot_textsz) {
		device_printf(sc->sc_dev, "%s: firmware file too short: "
		    "%zu bytes, main [%d, %d], init [%d,%d] boot %d\n",
		    __func__, fp->datasize, main_textsz, main_datasz,
		    init_textsz, init_datasz, boot_textsz);
		error = EINVAL;
		goto fail;
	}

	/* get pointers to firmware segments */
	main_text = (const uint8_t *)(hdr + 1);
	main_data = main_text + main_textsz;
	init_text = main_data + main_datasz;
	init_data = init_text + init_textsz;
	boot_text = init_data + init_datasz;

	/* copy initialization images into pre-allocated DMA-safe memory */
	memcpy(dma->vaddr, init_data, init_datasz);
	memcpy(dma->vaddr + IWN_FW_INIT_DATA_MAXSZ, init_text, init_textsz);

	/* tell adapter where to find initialization images */
	iwn_mem_lock(sc);
	iwn_mem_write(sc, IWN_MEM_DATA_BASE, dma->paddr >> 4);
	iwn_mem_write(sc, IWN_MEM_DATA_SIZE, init_datasz);
	iwn_mem_write(sc, IWN_MEM_TEXT_BASE,
	    (dma->paddr + IWN_FW_INIT_DATA_MAXSZ) >> 4);
	iwn_mem_write(sc, IWN_MEM_TEXT_SIZE, init_textsz);
	iwn_mem_unlock(sc);

	/* load firmware boot code */
	error = iwn_transfer_microcode(sc, boot_text, boot_textsz);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not load boot firmware, error %d\n",
		    __func__, error);
		goto fail;
	}

	/* now press "execute" ;-) */
	IWN_WRITE(sc, IWN_RESET, 0);

	/* wait at most one second for first alive notification */
	error = msleep(sc, &sc->sc_mtx, PCATCH, "iwninit", hz);
	if (error != 0) {
		/* this isn't what was supposed to happen.. */
		device_printf(sc->sc_dev,
		    "%s: timeout waiting for first alive notice, error %d\n",
		    __func__, error);
		goto fail;
	}

	/* copy runtime images into pre-allocated DMA-safe memory */
	memcpy(dma->vaddr, main_data, main_datasz);
	memcpy(dma->vaddr + IWN_FW_MAIN_DATA_MAXSZ, main_text, main_textsz);

	/* tell adapter where to find runtime images */
	iwn_mem_lock(sc);
	iwn_mem_write(sc, IWN_MEM_DATA_BASE, dma->paddr >> 4);
	iwn_mem_write(sc, IWN_MEM_DATA_SIZE, main_datasz);
	iwn_mem_write(sc, IWN_MEM_TEXT_BASE,
	    (dma->paddr + IWN_FW_MAIN_DATA_MAXSZ) >> 4);
	iwn_mem_write(sc, IWN_MEM_TEXT_SIZE, IWN_FW_UPDATED | main_textsz);
	iwn_mem_unlock(sc);

	/* wait at most one second for second alive notification */
	error = msleep(sc, &sc->sc_mtx, PCATCH, "iwninit", hz);
	if (error != 0) {
		/* this isn't what was supposed to happen.. */
		device_printf(sc->sc_dev,
		   "%s: timeout waiting for second alive notice, error %d\n",
		   __func__, error);
		goto fail;
	}
	return 0;
fail:
	return error;
}

void
iwn_unload_firmware(struct iwn_softc *sc)
{
        if (sc->fw_fp != NULL) {
                firmware_put(sc->fw_fp, FIRMWARE_UNLOAD);
                sc->fw_fp = NULL;
        }
}

static void
iwn_timer_timeout(void *arg)
{
	struct iwn_softc *sc = arg;

	IWN_LOCK_ASSERT(sc);

	if (sc->calib_cnt && --sc->calib_cnt == 0) {
		DPRINTF(sc, IWN_DEBUG_CALIBRATE, "%s\n",
		    "send statistics request");
		(void) iwn_cmd(sc, IWN_CMD_GET_STATISTICS, NULL, 0, 1);
		sc->calib_cnt = 60;	/* do calibration every 60s */
	}
	iwn_watchdog(sc);		/* NB: piggyback tx watchdog */
	callout_reset(&sc->sc_timer_to, hz, iwn_timer_timeout, sc);
}

static void
iwn_calib_reset(struct iwn_softc *sc)
{
	callout_reset(&sc->sc_timer_to, hz, iwn_timer_timeout, sc);
	sc->calib_cnt = 60;		/* do calibration every 60s */
}

void
iwn_ampdu_rx_start(struct iwn_softc *sc, struct iwn_rx_desc *desc)
{
	struct iwn_rx_stat *stat;

	DPRINTF(sc, IWN_DEBUG_RECV, "%s\n", "received AMPDU stats");
	/* save Rx statistics, they will be used on IWN_AMPDU_RX_DONE */
	stat = (struct iwn_rx_stat *)(desc + 1);
	memcpy(&sc->last_rx_stat, stat, sizeof (*stat));
	sc->last_rx_valid = 1;
}

static __inline int
maprate(int iwnrate)
{
	switch (iwnrate) {
	/* CCK rates */
	case  10: return   2;
	case  20: return   4;
	case  55: return  11;
	case 110: return  22;
	/* OFDM rates */
	case 0xd: return  12;
	case 0xf: return  18;
	case 0x5: return  24;
	case 0x7: return  36;
	case 0x9: return  48;
	case 0xb: return  72;
	case 0x1: return  96;
	case 0x3: return 108;
	/* XXX MCS */
	}
	/* unknown rate: should not happen */
	return 0;
}

void
iwn_rx_intr(struct iwn_softc *sc, struct iwn_rx_desc *desc,
    struct iwn_rx_data *data)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct iwn_rx_ring *ring = &sc->rxq;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct mbuf *m, *mnew;
	struct iwn_rx_stat *stat;
	caddr_t head;
	uint32_t *tail;
	int8_t rssi, nf;
	int len, error;
	bus_addr_t paddr;

	if (desc->type == IWN_AMPDU_RX_DONE) {
		/* check for prior AMPDU_RX_START */
		if (!sc->last_rx_valid) {
			DPRINTF(sc, IWN_DEBUG_ANY,
			    "%s: missing AMPDU_RX_START\n", __func__);
			ifp->if_ierrors++;
			return;
		}
		sc->last_rx_valid = 0;
		stat = &sc->last_rx_stat;
	} else
		stat = (struct iwn_rx_stat *)(desc + 1);

	if (stat->cfg_phy_len > IWN_STAT_MAXLEN) {
		device_printf(sc->sc_dev,
		    "%s: invalid rx statistic header, len %d\n",
		    __func__, stat->cfg_phy_len);
		ifp->if_ierrors++;
		return;
	}
	if (desc->type == IWN_AMPDU_RX_DONE) {
		struct iwn_rx_ampdu *ampdu = (struct iwn_rx_ampdu *)(desc + 1);
		head = (caddr_t)(ampdu + 1);
		len = le16toh(ampdu->len);
	} else {
		head = (caddr_t)(stat + 1) + stat->cfg_phy_len;
		len = le16toh(stat->len);
	}

	/* discard Rx frames with bad CRC early */
	tail = (uint32_t *)(head + len);
	if ((le32toh(*tail) & IWN_RX_NOERROR) != IWN_RX_NOERROR) {
		DPRINTF(sc, IWN_DEBUG_RECV, "%s: rx flags error %x\n",
		    __func__, le32toh(*tail));
		ifp->if_ierrors++;
		return;
	}
	if (len < sizeof (struct ieee80211_frame)) {
		DPRINTF(sc, IWN_DEBUG_RECV, "%s: frame too short: %d\n",
		    __func__, len);
		ic->ic_stats.is_rx_tooshort++;
		ifp->if_ierrors++;
		return;
	}

	/* XXX don't need mbuf, just dma buffer */
	mnew = m_getjcl(M_DONTWAIT, MT_DATA, M_PKTHDR, MJUMPAGESIZE);
	if (mnew == NULL) {
		DPRINTF(sc, IWN_DEBUG_ANY, "%s: no mbuf to restock ring\n",
		    __func__);
		ic->ic_stats.is_rx_nobuf++;
		ifp->if_ierrors++;
		return;
	}
	error = bus_dmamap_load(ring->data_dmat, data->map,
	    mtod(mnew, caddr_t), MJUMPAGESIZE,
	    iwn_dma_map_addr, &paddr, BUS_DMA_NOWAIT);
	if (error != 0 && error != EFBIG) {
		device_printf(sc->sc_dev,
		    "%s: bus_dmamap_load failed, error %d\n", __func__, error);
		m_freem(mnew);
		ic->ic_stats.is_rx_nobuf++;	/* XXX need stat */
		ifp->if_ierrors++;
		return;
	}
	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_PREWRITE);

	/* finalize mbuf and swap in new one */
	m = data->m;
	m->m_pkthdr.rcvif = ifp;
	m->m_data = head;
	m->m_pkthdr.len = m->m_len = len;

	data->m = mnew;
	/* update Rx descriptor */
	ring->desc[ring->cur] = htole32(paddr >> 8);

	rssi = iwn_get_rssi(sc, stat);

	/* grab a reference to the source node */
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	nf = (ni != NULL && ni->ni_vap->iv_state == IEEE80211_S_RUN &&
	    (ic->ic_flags & IEEE80211_F_SCAN) == 0) ? sc->noise : -95;

	if (bpf_peers_present(ifp->if_bpf)) {
		struct iwn_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_dbm_antsignal = rssi;
		tap->wr_dbm_antnoise = nf;
		tap->wr_rate = maprate(stat->rate);
		tap->wr_tsft = htole64(stat->tstamp);

		if (stat->flags & htole16(IWN_CONFIG_SHPREAMBLE))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_rxtap_len, m);
	}

	IWN_UNLOCK(sc);

	/* send the frame to the 802.11 layer */
	if (ni != NULL) {
		(void) ieee80211_input(ni, m, rssi - nf, nf, 0);
		ieee80211_free_node(ni);
	} else
		(void) ieee80211_input_all(ic, m, rssi - nf, nf, 0);

	IWN_LOCK(sc);
}

void
iwn_rx_statistics(struct iwn_softc *sc, struct iwn_rx_desc *desc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_stats *stats = (struct iwn_stats *)(desc + 1);

	/* beacon stats are meaningful only when associated and not scanning */
	if (vap->iv_state != IEEE80211_S_RUN ||
	    (ic->ic_flags & IEEE80211_F_SCAN))
		return;

	DPRINTF(sc, IWN_DEBUG_CALIBRATE, "%s: cmd %d\n", __func__, desc->type);
	iwn_calib_reset(sc);

	/* test if temperature has changed */
	if (stats->general.temp != sc->rawtemp) {
		int temp;

		sc->rawtemp = stats->general.temp;
		temp = iwn_get_temperature(sc);
		DPRINTF(sc, IWN_DEBUG_CALIBRATE, "%s: temperature %d\n",
		    __func__, temp);

		/* update Tx power if need be */
		iwn_power_calibration(sc, temp);
	}

	if (desc->type != IWN_BEACON_STATISTICS)
		return;	/* reply to a statistics request */

	sc->noise = iwn_get_noise(&stats->rx.general);
	DPRINTF(sc, IWN_DEBUG_CALIBRATE, "%s: noise %d\n", __func__, sc->noise);

	/* test that RSSI and noise are present in stats report */
	if (stats->rx.general.flags != htole32(1)) {
		DPRINTF(sc, IWN_DEBUG_ANY, "%s\n",
		    "received statistics without RSSI");
		return;
	}

	if (calib->state == IWN_CALIB_STATE_ASSOC)
		iwn_compute_differential_gain(sc, &stats->rx.general);
	else if (calib->state == IWN_CALIB_STATE_RUN)
		iwn_tune_sensitivity(sc, &stats->rx);
}

void
iwn_tx_intr(struct iwn_softc *sc, struct iwn_rx_desc *desc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct iwn_tx_ring *ring = &sc->txq[desc->qid & 0xf];
	struct iwn_tx_data *data = &ring->data[desc->idx];
	struct iwn_tx_stat *stat = (struct iwn_tx_stat *)(desc + 1);
	struct iwn_node *wn = IWN_NODE(data->ni);
	struct mbuf *m;
	struct ieee80211_node *ni;
	uint32_t status;

	KASSERT(data->ni != NULL, ("no node"));

	DPRINTF(sc, IWN_DEBUG_XMIT, "%s: "
	    "qid %d idx %d retries %d nkill %d rate %x duration %d status %x\n",
	    __func__, desc->qid, desc->idx, stat->ntries,
	    stat->nkill, stat->rate, le16toh(stat->duration),
	    le32toh(stat->status));

	/*
	 * Update rate control statistics for the node.
	 */
	status = le32toh(stat->status) & 0xff;
	if (status & 0x80) {
		DPRINTF(sc, IWN_DEBUG_ANY, "%s: status 0x%x\n",
		    __func__, le32toh(stat->status));
		ifp->if_oerrors++;
		ieee80211_amrr_tx_complete(&wn->amn,
		    IEEE80211_AMRR_FAILURE, stat->ntries);
	} else {
		ieee80211_amrr_tx_complete(&wn->amn,
		    IEEE80211_AMRR_SUCCESS, stat->ntries);
	}

	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(ring->data_dmat, data->map);

	m = data->m, data->m = NULL;
	ni = data->ni, data->ni = NULL;

	if (m->m_flags & M_TXCB) {
		/*
		 * Channels marked for "radar" require traffic to be received
		 * to unlock before we can transmit.  Until traffic is seen
		 * any attempt to transmit is returned immediately with status
		 * set to IWN_TX_FAIL_TX_LOCKED.  Unfortunately this can easily
		 * happen on first authenticate after scanning.  To workaround
		 * this we ignore a failure of this sort in AUTH state so the
		 * 802.11 layer will fall back to using a timeout to wait for
		 * the AUTH reply.  This allows the firmware time to see
		 * traffic so a subsequent retry of AUTH succeeds.  It's
		 * unclear why the firmware does not maintain state for
		 * channels recently visited as this would allow immediate
		 * use of the channel after a scan (where we see traffic).
		 */
		if (status == IWN_TX_FAIL_TX_LOCKED &&
		    ni->ni_vap->iv_state == IEEE80211_S_AUTH)
			ieee80211_process_callback(ni, m, 0);
		else
			ieee80211_process_callback(ni, m,
			    (status & IWN_TX_FAIL) != 0);
	}
	m_freem(m);
	ieee80211_free_node(ni);

	ring->queued--;

	sc->sc_tx_timer = 0;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	iwn_start_locked(ifp);
}

void
iwn_cmd_intr(struct iwn_softc *sc, struct iwn_rx_desc *desc)
{
	struct iwn_tx_ring *ring = &sc->txq[4];
	struct iwn_tx_data *data;

	if ((desc->qid & 0xf) != 4)
		return;	/* not a command ack */

	data = &ring->data[desc->idx];

	/* if the command was mapped in a mbuf, free it */
	if (data->m != NULL) {
		bus_dmamap_unload(ring->data_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
	}

	wakeup(&ring->cmd[desc->idx]);
}

static void
iwn_bmiss(void *arg, int npending)
{
	struct iwn_softc *sc = arg;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;

	ieee80211_beacon_miss(ic);
}

void
iwn_notif_intr(struct iwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	uint16_t hw;

	hw = le16toh(sc->shared->closed_count) & 0xfff;
	while (sc->rxq.cur != hw) {
		struct iwn_rx_data *data = &sc->rxq.data[sc->rxq.cur];
		struct iwn_rx_desc *desc = (void *)data->m->m_ext.ext_buf;

		DPRINTF(sc, IWN_DEBUG_RECV,
		    "%s: qid %x idx %d flags %x type %d(%s) len %d\n",
		    __func__, desc->qid, desc->idx, desc->flags,
		    desc->type, iwn_intr_str(desc->type),
		    le16toh(desc->len));

		if (!(desc->qid & 0x80))	/* reply to a command */
			iwn_cmd_intr(sc, desc);

		switch (desc->type) {
		case IWN_RX_DONE:
		case IWN_AMPDU_RX_DONE:
			iwn_rx_intr(sc, desc, data);
			break;

		case IWN_AMPDU_RX_START:
			iwn_ampdu_rx_start(sc, desc);
			break;

		case IWN_TX_DONE:
			/* a 802.11 frame has been transmitted */
			iwn_tx_intr(sc, desc);
			break;

		case IWN_RX_STATISTICS:
		case IWN_BEACON_STATISTICS:
			iwn_rx_statistics(sc, desc);
			break;

		case IWN_BEACON_MISSED: {
			struct iwn_beacon_missed *miss =
			    (struct iwn_beacon_missed *)(desc + 1);
			int misses = le32toh(miss->consecutive);

			/* XXX not sure why we're notified w/ zero */
			if (misses == 0)
				break;
			DPRINTF(sc, IWN_DEBUG_STATE,
			    "%s: beacons missed %d/%d\n", __func__,
			    misses, le32toh(miss->total));
			/*
			 * If more than 5 consecutive beacons are missed,
			 * reinitialize the sensitivity state machine.
			 */
			if (vap->iv_state == IEEE80211_S_RUN && misses > 5)
				(void) iwn_init_sensitivity(sc);
			if (misses >= vap->iv_bmissthreshold)
				taskqueue_enqueue(taskqueue_swi,
				    &sc->sc_bmiss_task);
			break;
		}
		case IWN_UC_READY: {
			struct iwn_ucode_info *uc =
			    (struct iwn_ucode_info *)(desc + 1);

			/* the microcontroller is ready */
			DPRINTF(sc, IWN_DEBUG_RESET,
			    "microcode alive notification version=%d.%d "
			    "subtype=%x alive=%x\n", uc->major, uc->minor,
			    uc->subtype, le32toh(uc->valid));

			if (le32toh(uc->valid) != 1) {
				device_printf(sc->sc_dev,
				"microcontroller initialization failed");
				break;
			}
			if (uc->subtype == IWN_UCODE_INIT) {
				/* save microcontroller's report */
				memcpy(&sc->ucode_info, uc, sizeof (*uc));
			}
			break;
		}
		case IWN_STATE_CHANGED: {
			uint32_t *status = (uint32_t *)(desc + 1);

			/*
			 * State change allows hardware switch change to be
			 * noted. However, we handle this in iwn_intr as we
			 * get both the enable/disble intr.
			 */
			DPRINTF(sc, IWN_DEBUG_INTR, "state changed to %x\n",
			    le32toh(*status));
			break;
		}
		case IWN_START_SCAN: {
			struct iwn_start_scan *scan =
			    (struct iwn_start_scan *)(desc + 1);

			DPRINTF(sc, IWN_DEBUG_ANY,
			    "%s: scanning channel %d status %x\n",
			    __func__, scan->chan, le32toh(scan->status));
			break;
		}
		case IWN_STOP_SCAN: {
			struct iwn_stop_scan *scan =
			    (struct iwn_stop_scan *)(desc + 1);

			DPRINTF(sc, IWN_DEBUG_STATE,
			    "scan finished nchan=%d status=%d chan=%d\n",
			    scan->nchan, scan->status, scan->chan);

			iwn_queue_cmd(sc, IWN_SCAN_NEXT, 0, IWN_QUEUE_NORMAL);
			break;
		}
		}
		sc->rxq.cur = (sc->rxq.cur + 1) % IWN_RX_RING_COUNT;
	}

	/* tell the firmware what we have processed */
	hw = (hw == 0) ? IWN_RX_RING_COUNT - 1 : hw - 1;
	IWN_WRITE(sc, IWN_RX_WIDX, hw & ~7);
}

void
iwn_intr(void *arg)
{
	struct iwn_softc *sc = arg;
	uint32_t r1, r2;

	IWN_LOCK(sc);

	/* disable interrupts */
	IWN_WRITE(sc, IWN_MASK, 0);

	r1 = IWN_READ(sc, IWN_INTR);
	r2 = IWN_READ(sc, IWN_INTR_STATUS);

	if (r1 == 0 && r2 == 0) {
		IWN_WRITE(sc, IWN_MASK, IWN_INTR_MASK);
		goto done;	/* not for us */
	}

	if (r1 == 0xffffffff)
		goto done;	/* hardware gone */

	/* ack interrupts */
	IWN_WRITE(sc, IWN_INTR, r1);
	IWN_WRITE(sc, IWN_INTR_STATUS, r2);

	DPRINTF(sc, IWN_DEBUG_INTR, "interrupt reg1=%x reg2=%x\n", r1, r2);

	if (r1 & IWN_RF_TOGGLED) {
		uint32_t tmp = IWN_READ(sc, IWN_GPIO_CTL);
		device_printf(sc->sc_dev, "RF switch: radio %s\n",
		    (tmp & IWN_GPIO_RF_ENABLED) ? "enabled" : "disabled");
		if (tmp & IWN_GPIO_RF_ENABLED)
			iwn_queue_cmd(sc, IWN_RADIO_ENABLE, 0, IWN_QUEUE_CLEAR);
		else
			iwn_queue_cmd(sc, IWN_RADIO_DISABLE, 0, IWN_QUEUE_CLEAR);
	}
	if (r1 & IWN_CT_REACHED)
		device_printf(sc->sc_dev, "critical temperature reached!\n");
	if (r1 & (IWN_SW_ERROR | IWN_HW_ERROR)) {
		device_printf(sc->sc_dev, "error, INTR=%b STATUS=0x%x\n",
		    r1, IWN_INTR_BITS, r2);
		iwn_queue_cmd(sc, IWN_REINIT, 0, IWN_QUEUE_CLEAR);
		goto done;
	}
	if ((r1 & (IWN_RX_INTR | IWN_SW_RX_INTR)) || (r2 & IWN_RX_STATUS_INTR))
		iwn_notif_intr(sc);
	if (r1 & IWN_ALIVE_INTR)
		wakeup(sc);

	/* re-enable interrupts */
	IWN_WRITE(sc, IWN_MASK, IWN_INTR_MASK);
done:
	IWN_UNLOCK(sc);
}

uint8_t
iwn_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates (returned values are device-dependent) */
	case 2:		return 10;
	case 4:		return 20;
	case 11:	return 55;
	case 22:	return 110;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	/* R1-R4, (u)ral is R4-R1 */
	case 12:	return 0xd;
	case 18:	return 0xf;
	case 24:	return 0x5;
	case 36:	return 0x7;
	case 48:	return 0x9;
	case 72:	return 0xb;
	case 96:	return 0x1;
	case 108:	return 0x3;
	case 120:	return 0x3;
	}
	/* unknown rate (should not get there) */
	return 0;
}

/* determine if a given rate is CCK or OFDM */
#define IWN_RATE_IS_OFDM(rate) ((rate) >= 12 && (rate) != 22)

int
iwn_tx_data(struct iwn_softc *sc, struct mbuf *m0, struct ieee80211_node *ni,
    struct iwn_tx_ring *ring)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = sc->sc_ifp;
	const struct ieee80211_txparam *tp;
	struct iwn_tx_desc *desc;
	struct iwn_tx_data *data;
	struct iwn_tx_cmd *cmd;
	struct iwn_cmd_data *tx;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	bus_addr_t paddr;
	uint32_t flags;
	uint16_t timeout;
	uint8_t type;
	u_int hdrlen;
	struct mbuf *mnew;
	int rate, error, pad, nsegs, i, ismcast, id;
	bus_dma_segment_t segs[IWN_MAX_SCATTER];

	IWN_LOCK_ASSERT(sc);

	wh = mtod(m0, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	hdrlen = ieee80211_anyhdrsize(wh);

	/* pick a tx rate */
	/* XXX ni_chan */
	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];
	if (type == IEEE80211_FC0_TYPE_MGT)
		rate = tp->mgmtrate;
	else if (ismcast)
		rate = tp->mcastrate;
	else if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE)
		rate = tp->ucastrate;
	else {
		(void) ieee80211_amrr_choose(ni, &IWN_NODE(ni)->amn);
		rate = ni->ni_txrate;
	}

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		k = ieee80211_crypto_encap(ni, m0);
		if (k == NULL) {
			m_freem(m0);
			return ENOBUFS;
		}
		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	} else
		k = NULL;

	if (bpf_peers_present(ifp->if_bpf)) {
		struct iwn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		if (k != NULL)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m0);
	}

	flags = IWN_TX_AUTO_SEQ;
	/* XXX honor ACM */
	if (!ismcast)
		flags |= IWN_TX_NEED_ACK;

	if (ismcast || type != IEEE80211_FC0_TYPE_DATA)
		id = IWN_ID_BROADCAST;
	else
		id = IWN_ID_BSS;

	/* check if RTS/CTS or CTS-to-self protection must be used */
	if (!ismcast) {
		/* multicast frames are not sent at OFDM rates in 802.11b/g */
		if (m0->m_pkthdr.len+IEEE80211_CRC_LEN > vap->iv_rtsthreshold) {
			flags |= IWN_TX_NEED_RTS | IWN_TX_FULL_TXOP;
		} else if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
		    IWN_RATE_IS_OFDM(rate)) {
			if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				flags |= IWN_TX_NEED_CTS | IWN_TX_FULL_TXOP;
			else if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				flags |= IWN_TX_NEED_RTS | IWN_TX_FULL_TXOP;
		}
	}

	if (type == IEEE80211_FC0_TYPE_MGT) {
		uint8_t subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		/* tell h/w to set timestamp in probe responses */
		if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			flags |= IWN_TX_INSERT_TSTAMP;

		if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
		    subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)
			timeout = htole16(3);
		else
			timeout = htole16(2);
	} else
		timeout = htole16(0);

	if (hdrlen & 3) {
		/* first segment's length must be a multiple of 4 */
		flags |= IWN_TX_NEED_PADDING;
		pad = 4 - (hdrlen & 3);
	} else
		pad = 0;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	cmd = &ring->cmd[ring->cur];
	cmd->code = IWN_CMD_TX_DATA;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	tx = (struct iwn_cmd_data *)cmd->data;
	/* NB: no need to bzero tx, all fields are reinitialized here */
	tx->id = id;
	tx->flags = htole32(flags);
	tx->len = htole16(m0->m_pkthdr.len);
	tx->rate = iwn_plcp_signal(rate);
	tx->rts_ntries = 60;		/* XXX? */
	tx->data_ntries = 15;		/* XXX? */
	tx->lifetime = htole32(IWN_LIFETIME_INFINITE);
	tx->timeout = timeout;

	if (k != NULL) {
		/* XXX fill in */;
	} else
		tx->security = 0;

	/* XXX alternate between Ant A and Ant B ? */
	tx->rflags = IWN_RFLAG_ANT_B;
	if (tx->id == IWN_ID_BROADCAST) {
		tx->ridx = IWN_MAX_TX_RETRIES - 1;
		if (!IWN_RATE_IS_OFDM(rate))
			tx->rflags |= IWN_RFLAG_CCK;
	} else {
		tx->ridx = 0;
		/* tell adapter to ignore rflags */
		tx->flags |= htole32(IWN_TX_USE_NODE_RATE);
	}

	/* copy and trim IEEE802.11 header */
	memcpy((uint8_t *)(tx + 1), wh, hdrlen);
	m_adj(m0, hdrlen);

	error = bus_dmamap_load_mbuf_sg(ring->data_dmat, data->map, m0, segs,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		if (error == EFBIG) {
			/* too many fragments, linearize */
			mnew = m_collapse(m0, M_DONTWAIT, IWN_MAX_SCATTER);
			if (mnew == NULL) {
				IWN_UNLOCK(sc);
				device_printf(sc->sc_dev,
				    "%s: could not defrag mbuf\n", __func__);
				m_freem(m0);
				return ENOBUFS;
			}
			m0 = mnew;
			error = bus_dmamap_load_mbuf_sg(ring->data_dmat,
			    data->map, m0, segs, &nsegs, BUS_DMA_NOWAIT);
		}
		if (error != 0) {
			IWN_UNLOCK(sc);
			device_printf(sc->sc_dev,
			    "%s: bus_dmamap_load_mbuf_sg failed, error %d\n",
			     __func__, error);
			m_freem(m0);
			return error;
		}
	}

	data->m = m0;
	data->ni = ni;

	DPRINTF(sc, IWN_DEBUG_XMIT, "%s: qid %d idx %d len %d nsegs %d\n",
	    __func__, ring->qid, ring->cur, m0->m_pkthdr.len, nsegs);

	paddr = ring->cmd_dma.paddr + ring->cur * sizeof (struct iwn_tx_cmd);
	tx->loaddr = htole32(paddr + 4 +
	    offsetof(struct iwn_cmd_data, ntries));
	tx->hiaddr = 0;	/* limit to 32-bit physical addresses */

	/* first scatter/gather segment is used by the tx data command */
	IWN_SET_DESC_NSEGS(desc, 1 + nsegs);
	IWN_SET_DESC_SEG(desc, 0, paddr, 4 + sizeof (*tx) + hdrlen + pad);
	for (i = 1; i <= nsegs; i++) {
		IWN_SET_DESC_SEG(desc, i, segs[i - 1].ds_addr,
		     segs[i - 1].ds_len);
	}
	sc->shared->len[ring->qid][ring->cur] =
	    htole16(hdrlen + m0->m_pkthdr.len + 8);

	if (ring->cur < IWN_TX_WINDOW)
		sc->shared->len[ring->qid][ring->cur + IWN_TX_RING_COUNT] =
			htole16(hdrlen + m0->m_pkthdr.len + 8);

	ring->queued++;

	/* kick Tx ring */
	ring->cur = (ring->cur + 1) % IWN_TX_RING_COUNT;
	IWN_WRITE(sc, IWN_TX_WIDX, ring->qid << 8 | ring->cur);

	ifp->if_opackets++;
	sc->sc_tx_timer = 5;

	return 0;
}

void
iwn_start(struct ifnet *ifp)
{
	struct iwn_softc *sc = ifp->if_softc;

	IWN_LOCK(sc);
	iwn_start_locked(ifp);
	IWN_UNLOCK(sc);
}

void
iwn_start_locked(struct ifnet *ifp)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni;
	struct iwn_tx_ring *txq;
	struct mbuf *m;
	int pri;

	IWN_LOCK_ASSERT(sc);

	for (;;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		pri = M_WME_GETAC(m);
		txq = &sc->txq[pri];
		m = ieee80211_encap(ni, m);
		if (m == NULL) {
			ifp->if_oerrors++;
			ieee80211_free_node(ni);
			continue;
		}
		if (txq->queued >= IWN_TX_RING_COUNT - 8) {
			/* XXX not right */
			/* ring is nearly full, stop flow */
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		}
		if (iwn_tx_data(sc, m, ni, txq) != 0) {
			ifp->if_oerrors++;
			ieee80211_free_node(ni);
			IWN_UNLOCK(sc);
			break;
		}
	}
}

static int
iwn_tx_handoff(struct iwn_softc *sc,
	struct iwn_tx_ring *ring,
	struct iwn_tx_cmd *cmd,
	struct iwn_cmd_data *tx,
	struct ieee80211_node *ni,
	struct mbuf *m0, u_int hdrlen, int pad)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct iwn_tx_desc *desc;
	struct iwn_tx_data *data;
	bus_addr_t paddr;
	struct mbuf *mnew;
	int error, nsegs, i;
	bus_dma_segment_t segs[IWN_MAX_SCATTER];

	/* copy and trim IEEE802.11 header */
	memcpy((uint8_t *)(tx + 1), mtod(m0, uint8_t *), hdrlen);
	m_adj(m0, hdrlen);

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	error = bus_dmamap_load_mbuf_sg(ring->data_dmat, data->map, m0, segs,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		if (error == EFBIG) {
			/* too many fragments, linearize */
			mnew = m_collapse(m0, M_DONTWAIT, IWN_MAX_SCATTER);
			if (mnew == NULL) {
				IWN_UNLOCK(sc);
				device_printf(sc->sc_dev,
				    "%s: could not defrag mbuf\n", __func__);
				m_freem(m0);
				return ENOBUFS;
			}
			m0 = mnew;
			error = bus_dmamap_load_mbuf_sg(ring->data_dmat,
			    data->map, m0, segs, &nsegs, BUS_DMA_NOWAIT);
		}
		if (error != 0) {
			IWN_UNLOCK(sc);
			device_printf(sc->sc_dev,
			    "%s: bus_dmamap_load_mbuf_sg failed, error %d\n",
			     __func__, error);
			m_freem(m0);
			return error;
		}
	}

	data->m = m0;
	data->ni = ni;

	DPRINTF(sc, IWN_DEBUG_XMIT, "%s: qid %d idx %d len %d nsegs %d\n",
	    __func__, ring->qid, ring->cur, m0->m_pkthdr.len, nsegs);

	paddr = ring->cmd_dma.paddr + ring->cur * sizeof (struct iwn_tx_cmd);
	tx->loaddr = htole32(paddr + 4 +
	    offsetof(struct iwn_cmd_data, ntries));
	tx->hiaddr = 0;	/* limit to 32-bit physical addresses */

	/* first scatter/gather segment is used by the tx data command */
	IWN_SET_DESC_NSEGS(desc, 1 + nsegs);
	IWN_SET_DESC_SEG(desc, 0, paddr, 4 + sizeof (*tx) + hdrlen + pad);
	for (i = 1; i <= nsegs; i++) {
		IWN_SET_DESC_SEG(desc, i, segs[i - 1].ds_addr,
		     segs[i - 1].ds_len);
	}
	sc->shared->len[ring->qid][ring->cur] =
	    htole16(hdrlen + m0->m_pkthdr.len + 8);

	if (ring->cur < IWN_TX_WINDOW)
		sc->shared->len[ring->qid][ring->cur + IWN_TX_RING_COUNT] =
			htole16(hdrlen + m0->m_pkthdr.len + 8);

	ring->queued++;

	/* kick Tx ring */
	ring->cur = (ring->cur + 1) % IWN_TX_RING_COUNT;
	IWN_WRITE(sc, IWN_TX_WIDX, ring->qid << 8 | ring->cur);

	ifp->if_opackets++;
	sc->sc_tx_timer = 5;

	return 0;
}

static int
iwn_tx_data_raw(struct iwn_softc *sc, struct mbuf *m0,
    struct ieee80211_node *ni, struct iwn_tx_ring *ring,
    const struct ieee80211_bpf_params *params)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct iwn_tx_cmd *cmd;
	struct iwn_cmd_data *tx;
	struct ieee80211_frame *wh;
	uint32_t flags;
	uint8_t type, subtype;
	u_int hdrlen;
	int rate, pad;

	IWN_LOCK_ASSERT(sc);

	wh = mtod(m0, struct ieee80211_frame *);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	hdrlen = ieee80211_anyhdrsize(wh);

	flags = IWN_TX_AUTO_SEQ;
	if ((params->ibp_flags & IEEE80211_BPF_NOACK) == 0)
		flags |= IWN_TX_NEED_ACK;
	if (params->ibp_flags & IEEE80211_BPF_RTS)
		flags |= IWN_TX_NEED_RTS | IWN_TX_FULL_TXOP;
	if (params->ibp_flags & IEEE80211_BPF_CTS)
		flags |= IWN_TX_NEED_CTS | IWN_TX_FULL_TXOP;
	if (type == IEEE80211_FC0_TYPE_MGT &&
	    subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
		/* tell h/w to set timestamp in probe responses */
		flags |= IWN_TX_INSERT_TSTAMP;
	}
	if (hdrlen & 3) {
		/* first segment's length must be a multiple of 4 */
		flags |= IWN_TX_NEED_PADDING;
		pad = 4 - (hdrlen & 3);
	} else
		pad = 0;

	/* pick a tx rate */
	rate = params->ibp_rate0;

	if (bpf_peers_present(ifp->if_bpf)) {
		struct iwn_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;

		bpf_mtap2(ifp->if_bpf, tap, sc->sc_txtap_len, m0);
	}

	cmd = &ring->cmd[ring->cur];
	cmd->code = IWN_CMD_TX_DATA;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	tx = (struct iwn_cmd_data *)cmd->data;
	/* NB: no need to bzero tx, all fields are reinitialized here */
	tx->id = IWN_ID_BROADCAST;
	tx->flags = htole32(flags);
	tx->len = htole16(m0->m_pkthdr.len);
	tx->rate = iwn_plcp_signal(rate);
	tx->rts_ntries = params->ibp_try1;		/* XXX? */
	tx->data_ntries = params->ibp_try0;
	tx->lifetime = htole32(IWN_LIFETIME_INFINITE);
	/* XXX use try count? */
	if (type == IEEE80211_FC0_TYPE_MGT) {
		if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
		    subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)
			tx->timeout = htole16(3);
		else
			tx->timeout = htole16(2);
	} else
		tx->timeout = htole16(0);
	tx->security = 0;
	/* XXX alternate between Ant A and Ant B ? */
	tx->rflags = IWN_RFLAG_ANT_B;	/* XXX params->ibp_pri >> 2 */
	tx->ridx = IWN_MAX_TX_RETRIES - 1;
	if (!IWN_RATE_IS_OFDM(rate))
		tx->rflags |= IWN_RFLAG_CCK;

	return iwn_tx_handoff(sc, ring, cmd, tx, ni, m0, hdrlen, pad);
}

static int
iwn_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct iwn_softc *sc = ifp->if_softc;
	struct iwn_tx_ring *txq;
	int error;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		ieee80211_free_node(ni);
		m_freem(m);
		return ENETDOWN;
	}

	IWN_LOCK(sc);
	if (params == NULL)
		txq = &sc->txq[M_WME_GETAC(m)];
	else
		txq = &sc->txq[params->ibp_pri & 3];
	if (txq->queued >= IWN_TX_RING_COUNT - 8) {
		/* XXX not right */
		/* ring is nearly full, stop flow */
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	}
	if (params == NULL) {
		/*
		 * Legacy path; interpret frame contents to decide
		 * precisely how to send the frame.
		 */
		error = iwn_tx_data(sc, m, ni, txq);
	} else {
		/*
		 * Caller supplied explicit parameters to use in
		 * sending the frame.
		 */
		error = iwn_tx_data_raw(sc, m, ni, txq, params);
	}
	if (error != 0) {
		/* NB: m is reclaimed on tx failure */
		ieee80211_free_node(ni);
		ifp->if_oerrors++;
	}
	IWN_UNLOCK(sc);
	return error;
}

static void
iwn_watchdog(struct iwn_softc *sc)
{
	if (sc->sc_tx_timer > 0 && --sc->sc_tx_timer == 0) {
		struct ifnet *ifp = sc->sc_ifp;

		if_printf(ifp, "device timeout\n");
		iwn_queue_cmd(sc, IWN_REINIT, 0, IWN_QUEUE_CLEAR);
	}
}

int
iwn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct iwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0, startall = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		IWN_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				iwn_init_locked(sc);
				startall = 1;
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				iwn_stop_locked(sc);
		}
		IWN_UNLOCK(sc);
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

void
iwn_read_eeprom(struct iwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	char domain[4];
	uint16_t val;
	int i, error;

	if ((error = iwn_eeprom_lock(sc)) != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not lock EEPROM, error %d\n", __func__, error);
		return;
	}
	/* read and print regulatory domain */
	iwn_read_prom_data(sc, IWN_EEPROM_DOMAIN, domain, 4);
	device_printf(sc->sc_dev,"Reg Domain: %.4s", domain);

	/* read and print MAC address */
	iwn_read_prom_data(sc, IWN_EEPROM_MAC, ic->ic_myaddr, 6);
	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	/* read the list of authorized channels */
	iwn_read_eeprom_channels(sc);

	/* read maximum allowed Tx power for 2GHz and 5GHz bands */
	iwn_read_prom_data(sc, IWN_EEPROM_MAXPOW, &val, 2);
	sc->maxpwr2GHz = val & 0xff;
	sc->maxpwr5GHz = val >> 8;
	/* check that EEPROM values are correct */
	if (sc->maxpwr5GHz < 20 || sc->maxpwr5GHz > 50)
		sc->maxpwr5GHz = 38;
	if (sc->maxpwr2GHz < 20 || sc->maxpwr2GHz > 50)
		sc->maxpwr2GHz = 38;
	DPRINTF(sc, IWN_DEBUG_RESET, "maxpwr 2GHz=%d 5GHz=%d\n",
	    sc->maxpwr2GHz, sc->maxpwr5GHz);

	/* read voltage at which samples were taken */
	iwn_read_prom_data(sc, IWN_EEPROM_VOLTAGE, &val, 2);
	sc->eeprom_voltage = (int16_t)le16toh(val);
	DPRINTF(sc, IWN_DEBUG_RESET, "voltage=%d (in 0.3V)\n",
	    sc->eeprom_voltage);

	/* read power groups */
	iwn_read_prom_data(sc, IWN_EEPROM_BANDS, sc->bands, sizeof sc->bands);
#ifdef IWN_DEBUG
	if (sc->sc_debug & IWN_DEBUG_ANY) {
		for (i = 0; i < IWN_NBANDS; i++)
			iwn_print_power_group(sc, i);
	}
#endif
	iwn_eeprom_unlock(sc);
}

struct iwn_chan_band {
	uint32_t	addr;	/* offset in EEPROM */
	uint32_t	flags;	/* net80211 flags */
	uint8_t		nchan;
#define IWN_MAX_CHAN_PER_BAND	14
	uint8_t		chan[IWN_MAX_CHAN_PER_BAND];
};

static void
iwn_read_eeprom_band(struct iwn_softc *sc, const struct iwn_chan_band *band)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct iwn_eeprom_chan channels[IWN_MAX_CHAN_PER_BAND];
	struct ieee80211_channel *c;
	int i, chan, flags;

	iwn_read_prom_data(sc, band->addr, channels,
	    band->nchan * sizeof (struct iwn_eeprom_chan));

	for (i = 0; i < band->nchan; i++) {
		if (!(channels[i].flags & IWN_EEPROM_CHAN_VALID)) {
			DPRINTF(sc, IWN_DEBUG_RESET,
			    "skip chan %d flags 0x%x maxpwr %d\n",
			    band->chan[i], channels[i].flags,
			    channels[i].maxpwr);
			continue;
		}
		chan = band->chan[i];

		/* translate EEPROM flags to net80211 */
		flags = 0;
		if ((channels[i].flags & IWN_EEPROM_CHAN_ACTIVE) == 0)
			flags |= IEEE80211_CHAN_PASSIVE;
		if ((channels[i].flags & IWN_EEPROM_CHAN_IBSS) == 0)
			flags |= IEEE80211_CHAN_NOADHOC;
		if (channels[i].flags & IWN_EEPROM_CHAN_RADAR) {
			flags |= IEEE80211_CHAN_DFS;
			/* XXX apparently IBSS may still be marked */
			flags |= IEEE80211_CHAN_NOADHOC;
		}

		DPRINTF(sc, IWN_DEBUG_RESET,
		    "add chan %d flags 0x%x maxpwr %d\n",
		    chan, channels[i].flags, channels[i].maxpwr);

		c = &ic->ic_channels[ic->ic_nchans++];
		c->ic_ieee = chan;
		c->ic_freq = ieee80211_ieee2mhz(chan, band->flags);
		c->ic_maxregpower = channels[i].maxpwr;
		c->ic_maxpower = 2*c->ic_maxregpower;
		if (band->flags & IEEE80211_CHAN_2GHZ) {
			/* G =>'s B is supported */
			c->ic_flags = IEEE80211_CHAN_B | flags;

			c = &ic->ic_channels[ic->ic_nchans++];
			c[0] = c[-1];
			c->ic_flags = IEEE80211_CHAN_G | flags;
		} else {	/* 5GHz band */
			c->ic_flags = IEEE80211_CHAN_A | flags;
		}
		/* XXX no constraints on using HT20 */
		/* add HT20, HT40 added separately */
		c = &ic->ic_channels[ic->ic_nchans++];
		c[0] = c[-1];
		c->ic_flags |= IEEE80211_CHAN_HT20;
		/* XXX NARROW =>'s 1/2 and 1/4 width? */
	}
}

static void
iwn_read_eeprom_ht40(struct iwn_softc *sc, const struct iwn_chan_band *band)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct iwn_eeprom_chan channels[IWN_MAX_CHAN_PER_BAND];
	struct ieee80211_channel *c, *cent, *extc;
	int i;

	iwn_read_prom_data(sc, band->addr, channels,
	    band->nchan * sizeof (struct iwn_eeprom_chan));

	for (i = 0; i < band->nchan; i++) {
		if (!(channels[i].flags & IWN_EEPROM_CHAN_VALID) ||
		    !(channels[i].flags & IWN_EEPROM_CHAN_WIDE)) {
			DPRINTF(sc, IWN_DEBUG_RESET,
			    "skip chan %d flags 0x%x maxpwr %d\n",
			    band->chan[i], channels[i].flags,
			    channels[i].maxpwr);
			continue;
		}
		/*
		 * Each entry defines an HT40 channel pair; find the
		 * center channel, then the extension channel above.
		 */
		cent = ieee80211_find_channel_byieee(ic, band->chan[i],
		    band->flags & ~IEEE80211_CHAN_HT);
		if (cent == NULL) {	/* XXX shouldn't happen */
			device_printf(sc->sc_dev,
			    "%s: no entry for channel %d\n",
			    __func__, band->chan[i]);
			continue;
		}
		extc = ieee80211_find_channel(ic, cent->ic_freq+20,
		    band->flags & ~IEEE80211_CHAN_HT);
		if (extc == NULL) {
			DPRINTF(sc, IWN_DEBUG_RESET,
			    "skip chan %d, extension channel not found\n",
			    band->chan[i]);
			continue;
		}

		DPRINTF(sc, IWN_DEBUG_RESET,
		    "add ht40 chan %d flags 0x%x maxpwr %d\n",
		    band->chan[i], channels[i].flags, channels[i].maxpwr);

		c = &ic->ic_channels[ic->ic_nchans++];
		c[0] = cent[0];
		c->ic_extieee = extc->ic_ieee;
		c->ic_flags &= ~IEEE80211_CHAN_HT;
		c->ic_flags |= IEEE80211_CHAN_HT40U;
		c = &ic->ic_channels[ic->ic_nchans++];
		c[0] = extc[0];
		c->ic_extieee = cent->ic_ieee;
		c->ic_flags &= ~IEEE80211_CHAN_HT;
		c->ic_flags |= IEEE80211_CHAN_HT40D;
	}
}

static void
iwn_read_eeprom_channels(struct iwn_softc *sc)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	static const struct iwn_chan_band iwn_bands[] = {
	    { IWN_EEPROM_BAND1, IEEE80211_CHAN_G, 14,
		{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 } },
	    { IWN_EEPROM_BAND2, IEEE80211_CHAN_A, 13,
		{ 183, 184, 185, 187, 188, 189, 192, 196, 7, 8, 11, 12, 16 } },
	    { IWN_EEPROM_BAND3, IEEE80211_CHAN_A, 12,
		{ 34, 36, 38, 40, 42, 44, 46, 48, 52, 56, 60, 64 } },
	    { IWN_EEPROM_BAND4, IEEE80211_CHAN_A, 11,
		{ 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140 } },
	    { IWN_EEPROM_BAND5, IEEE80211_CHAN_A, 6,
		{ 145, 149, 153, 157, 161, 165 } },
	    { IWN_EEPROM_BAND6, IEEE80211_CHAN_G | IEEE80211_CHAN_HT40, 7,
		{ 1, 2, 3, 4, 5, 6, 7 } },
	    { IWN_EEPROM_BAND7, IEEE80211_CHAN_A | IEEE80211_CHAN_HT40, 11,
		{ 36, 44, 52, 60, 100, 108, 116, 124, 132, 149, 157 } }
	};
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	int i;

	/* read the list of authorized channels */
	for (i = 0; i < N(iwn_bands)-2; i++)
		iwn_read_eeprom_band(sc, &iwn_bands[i]);
	for (; i < N(iwn_bands); i++)
		iwn_read_eeprom_ht40(sc, &iwn_bands[i]);
	ieee80211_sort_channels(ic->ic_channels, ic->ic_nchans);
#undef N
}

#ifdef IWN_DEBUG
void
iwn_print_power_group(struct iwn_softc *sc, int i)
{
	struct iwn_eeprom_band *band = &sc->bands[i];
	struct iwn_eeprom_chan_samples *chans = band->chans;
	int j, c;

	printf("===band %d===\n", i);
	printf("chan lo=%d, chan hi=%d\n", band->lo, band->hi);
	printf("chan1 num=%d\n", chans[0].num);
	for (c = 0; c < IWN_NTXCHAINS; c++) {
		for (j = 0; j < IWN_NSAMPLES; j++) {
			printf("chain %d, sample %d: temp=%d gain=%d "
			    "power=%d pa_det=%d\n", c, j,
			    chans[0].samples[c][j].temp,
			    chans[0].samples[c][j].gain,
			    chans[0].samples[c][j].power,
			    chans[0].samples[c][j].pa_det);
		}
	}
	printf("chan2 num=%d\n", chans[1].num);
	for (c = 0; c < IWN_NTXCHAINS; c++) {
		for (j = 0; j < IWN_NSAMPLES; j++) {
			printf("chain %d, sample %d: temp=%d gain=%d "
			    "power=%d pa_det=%d\n", c, j,
			    chans[1].samples[c][j].temp,
			    chans[1].samples[c][j].gain,
			    chans[1].samples[c][j].power,
			    chans[1].samples[c][j].pa_det);
		}
	}
}
#endif

/*
 * Send a command to the firmware.
 */
int
iwn_cmd(struct iwn_softc *sc, int code, const void *buf, int size, int async)
{
	struct iwn_tx_ring *ring = &sc->txq[4];
	struct iwn_tx_desc *desc;
	struct iwn_tx_cmd *cmd;
	bus_addr_t paddr;

	IWN_LOCK_ASSERT(sc);

	KASSERT(size <= sizeof cmd->data, ("Command too big"));

	desc = &ring->desc[ring->cur];
	cmd = &ring->cmd[ring->cur];

	cmd->code = code;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;
	memcpy(cmd->data, buf, size);

	paddr = ring->cmd_dma.paddr + ring->cur * sizeof (struct iwn_tx_cmd);

	IWN_SET_DESC_NSEGS(desc, 1);
	IWN_SET_DESC_SEG(desc, 0, paddr, 4 + size);
	sc->shared->len[ring->qid][ring->cur] = htole16(8);
	if (ring->cur < IWN_TX_WINDOW) {
	    sc->shared->len[ring->qid][ring->cur + IWN_TX_RING_COUNT] =
		htole16(8);
	}

	DPRINTF(sc, IWN_DEBUG_CMD, "%s: %s (0x%x) flags %d qid %d idx %d\n",
	    __func__, iwn_intr_str(cmd->code), cmd->code,
	    cmd->flags, cmd->qid, cmd->idx);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % IWN_TX_RING_COUNT;
	IWN_WRITE(sc, IWN_TX_WIDX, ring->qid << 8 | ring->cur);

	return async ? 0 : msleep(cmd, &sc->sc_mtx, PCATCH, "iwncmd", hz);
}

static const uint8_t iwn_ridx_to_plcp[] = {
	10, 20, 55, 110, /* CCK */
	0xd, 0xf, 0x5, 0x7, 0x9, 0xb, 0x1, 0x3, 0x3 /* OFDM R1-R4 */
};
static const uint8_t iwn_siso_mcs_to_plcp[] = {
	0, 0, 0, 0, 			/* CCK */
	0, 0, 1, 2, 3, 4, 5, 6, 7	/* HT */
};
static const uint8_t iwn_mimo_mcs_to_plcp[] = {
	0, 0, 0, 0, 			/* CCK */
	8, 8, 9, 10, 11, 12, 13, 14, 15	/* HT */
};
static const uint8_t iwn_prev_ridx[] = {
	/* NB: allow fallback from CCK11 to OFDM9 and from OFDM6 to CCK5 */
	0, 0, 1, 5,			/* CCK */
	2, 4, 3, 6, 7, 8, 9, 10, 10	/* OFDM */
};

/*
 * Configure hardware link parameters for the specified
 * node operating on the specified channel.
 */
int
iwn_set_link_quality(struct iwn_softc *sc, uint8_t id,
	const struct ieee80211_channel *c, int async)
{
	struct iwn_cmd_link_quality lq;
	int i, ridx;

	memset(&lq, 0, sizeof(lq));
	lq.id = id;
	if (IEEE80211_IS_CHAN_HT(c)) {
		lq.mimo = 1;
		lq.ssmask = 0x1;
	} else
		lq.ssmask = 0x2;

	if (id == IWN_ID_BSS)
		ridx = IWN_RATE_OFDM54;
	else if (IEEE80211_IS_CHAN_A(c))
		ridx = IWN_RATE_OFDM6;
	else
		ridx = IWN_RATE_CCK1;
	for (i = 0; i < IWN_MAX_TX_RETRIES; i++) {
		/* XXX toggle antenna for retry patterns */
		if (IEEE80211_IS_CHAN_HT40(c)) {
			lq.table[i].rate = iwn_mimo_mcs_to_plcp[ridx]
					 | IWN_RATE_MCS;
			lq.table[i].rflags = IWN_RFLAG_HT
					 | IWN_RFLAG_HT40
					 | IWN_RFLAG_ANT_A;
			/* XXX shortGI */
		} else if (IEEE80211_IS_CHAN_HT(c)) {
			lq.table[i].rate = iwn_siso_mcs_to_plcp[ridx]
					 | IWN_RATE_MCS;
			lq.table[i].rflags = IWN_RFLAG_HT
					 | IWN_RFLAG_ANT_A;
			/* XXX shortGI */
		} else {
			lq.table[i].rate = iwn_ridx_to_plcp[ridx];
			if (ridx <= IWN_RATE_CCK11)
				lq.table[i].rflags = IWN_RFLAG_CCK;
			lq.table[i].rflags |= IWN_RFLAG_ANT_B;
		}
		ridx = iwn_prev_ridx[ridx];
	}

	lq.dsmask = 0x3;
	lq.ampdu_disable = 3;
	lq.ampdu_limit = htole16(4000);
#ifdef IWN_DEBUG
	if (sc->sc_debug & IWN_DEBUG_STATE) {
		printf("%s: set link quality for node %d, mimo %d ssmask %d\n",
		    __func__, id, lq.mimo, lq.ssmask);
		printf("%s:", __func__);
		for (i = 0; i < IWN_MAX_TX_RETRIES; i++)
			printf(" %d:%x", lq.table[i].rate, lq.table[i].rflags);
		printf("\n");
	}
#endif
	return iwn_cmd(sc, IWN_CMD_TX_LINK_QUALITY, &lq, sizeof(lq), async);
}

#if 0

/*
 * Install a pairwise key into the hardware.
 */
int
iwn_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    const struct ieee80211_key *k)
{
	struct iwn_softc *sc = ic->ic_softc;
	struct iwn_node_info node;

	if (k->k_flags & IEEE80211_KEY_GROUP)
		return 0;

	memset(&node, 0, sizeof node);

	switch (k->k_cipher) {
	case IEEE80211_CIPHER_CCMP:
		node.security = htole16(IWN_CIPHER_CCMP);
		memcpy(node.key, k->k_key, k->k_len);
		break;
	default:
		return 0;
	}

	node.id = IWN_ID_BSS;
	IEEE80211_ADDR_COPY(node.macaddr, ni->ni_macaddr);
	node.control = IWN_NODE_UPDATE;
	node.flags = IWN_FLAG_SET_KEY;

	return iwn_cmd(sc, IWN_CMD_ADD_NODE, &node, sizeof node, 1);
}
#endif

int
iwn_wme_update(struct ieee80211com *ic)
{
#define IWN_EXP2(x)	((1 << (x)) - 1)	/* CWmin = 2^ECWmin - 1 */
#define	IWN_TXOP_TO_US(v)		(v<<5)
	struct iwn_softc *sc = ic->ic_ifp->if_softc;
	struct iwn_edca_params cmd;
	int i;

	memset(&cmd, 0, sizeof cmd);
	cmd.flags = htole32(IWN_EDCA_UPDATE);
	for (i = 0; i < WME_NUM_AC; i++) {
		const struct wmeParams *wmep =
		    &ic->ic_wme.wme_chanParams.cap_wmeParams[i];
		cmd.ac[i].aifsn = wmep->wmep_aifsn;
		cmd.ac[i].cwmin = htole16(IWN_EXP2(wmep->wmep_logcwmin));
		cmd.ac[i].cwmax = htole16(IWN_EXP2(wmep->wmep_logcwmax));
		cmd.ac[i].txoplimit =
		    htole16(IWN_TXOP_TO_US(wmep->wmep_txopLimit));
	}
	IWN_LOCK(sc);
	(void) iwn_cmd(sc, IWN_CMD_EDCA_PARAMS, &cmd, sizeof cmd, 1 /*async*/);
	IWN_UNLOCK(sc);
	return 0;
#undef IWN_TXOP_TO_US
#undef IWN_EXP2
}

void
iwn_set_led(struct iwn_softc *sc, uint8_t which, uint8_t off, uint8_t on)
{
	struct iwn_cmd_led led;

	led.which = which;
	led.unit = htole32(100000);	/* on/off in unit of 100ms */
	led.off = off;
	led.on = on;

	(void) iwn_cmd(sc, IWN_CMD_SET_LED, &led, sizeof led, 1);
}

/*
 * Set the critical temperature at which the firmware will automatically stop
 * the radio transmitter.
 */
int
iwn_set_critical_temp(struct iwn_softc *sc)
{
	struct iwn_ucode_info *uc = &sc->ucode_info;
	struct iwn_critical_temp crit;
	uint32_t r1, r2, r3, temp;

	r1 = le32toh(uc->temp[0].chan20MHz);
	r2 = le32toh(uc->temp[1].chan20MHz);
	r3 = le32toh(uc->temp[2].chan20MHz);
	/* inverse function of iwn_get_temperature() */
	temp = r2 + (IWN_CTOK(110) * (r3 - r1)) / 259;

	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_CTEMP_STOP_RF);

	memset(&crit, 0, sizeof crit);
	crit.tempR = htole32(temp);
	DPRINTF(sc, IWN_DEBUG_RESET, "setting critical temp to %u\n", temp);
	return iwn_cmd(sc, IWN_CMD_SET_CRITICAL_TEMP, &crit, sizeof crit, 0);
}

void
iwn_enable_tsf(struct iwn_softc *sc, struct ieee80211_node *ni)
{
	struct iwn_cmd_tsf tsf;
	uint64_t val, mod;

	memset(&tsf, 0, sizeof tsf);
	memcpy(&tsf.tstamp, ni->ni_tstamp.data, sizeof (uint64_t));
	tsf.bintval = htole16(ni->ni_intval);
	tsf.lintval = htole16(10);

	/* XXX all wrong */
	/* compute remaining time until next beacon */
	val = (uint64_t)ni->ni_intval * 1024;	/* msecs -> usecs */
	DPRINTF(sc, IWN_DEBUG_ANY, "%s: val = %ju %s\n", __func__,
	    val, val == 0 ? "correcting" : "");
	if (val == 0)
		val = 1;
	mod = le64toh(tsf.tstamp) % val;
	tsf.binitval = htole32((uint32_t)(val - mod));

	DPRINTF(sc, IWN_DEBUG_RESET, "TSF bintval=%u tstamp=%ju, init=%u\n",
	    ni->ni_intval, le64toh(tsf.tstamp), (uint32_t)(val - mod));

	if (iwn_cmd(sc, IWN_CMD_TSF, &tsf, sizeof tsf, 1) != 0)
		device_printf(sc->sc_dev,
		    "%s: could not enable TSF\n", __func__);
}

void
iwn_power_calibration(struct iwn_softc *sc, int temp)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
#if 0
	KASSERT(ic->ic_state == IEEE80211_S_RUN, ("not running"));
#endif
	DPRINTF(sc, IWN_DEBUG_CALIBRATE, "%s: temperature %d->%d\n",
	    __func__, sc->temp, temp);

	/* adjust Tx power if need be (delta >= 3°C) */
	if (abs(temp - sc->temp) < 3)
		return;

	sc->temp = temp;

	DPRINTF(sc, IWN_DEBUG_CALIBRATE, "%s: set Tx power for channel %d\n",
	    __func__, ieee80211_chan2ieee(ic, ic->ic_bsschan));
	if (iwn_set_txpower(sc, ic->ic_bsschan, 1) != 0) {
		/* just warn, too bad for the automatic calibration... */
		device_printf(sc->sc_dev,
		    "%s: could not adjust Tx power\n", __func__);
	}
}

/*
 * Set Tx power for a given channel (each rate has its own power settings).
 * This function takes into account the regulatory information from EEPROM,
 * the current temperature and the current voltage.
 */
int
iwn_set_txpower(struct iwn_softc *sc, struct ieee80211_channel *ch, int async)
{
/* fixed-point arithmetic division using a n-bit fractional part */
#define fdivround(a, b, n)	\
	((((1 << n) * (a)) / (b) + (1 << n) / 2) / (1 << n))
/* linear interpolation */
#define interpolate(x, x1, y1, x2, y2, n)	\
	((y1) + fdivround(((int)(x) - (x1)) * ((y2) - (y1)), (x2) - (x1), n))

	static const int tdiv[IWN_NATTEN_GROUPS] = { 9, 8, 8, 8, 6 };
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct iwn_ucode_info *uc = &sc->ucode_info;
	struct iwn_cmd_txpower cmd;
	struct iwn_eeprom_chan_samples *chans;
	const uint8_t *rf_gain, *dsp_gain;
	int32_t vdiff, tdiff;
	int i, c, grp, maxpwr;
	u_int chan;

	/* get channel number */
	chan = ieee80211_chan2ieee(ic, ch);

	memset(&cmd, 0, sizeof cmd);
	cmd.band = IEEE80211_IS_CHAN_5GHZ(ch) ? 0 : 1;
	cmd.chan = chan;

	if (IEEE80211_IS_CHAN_5GHZ(ch)) {
		maxpwr   = sc->maxpwr5GHz;
		rf_gain  = iwn_rf_gain_5ghz;
		dsp_gain = iwn_dsp_gain_5ghz;
	} else {
		maxpwr   = sc->maxpwr2GHz;
		rf_gain  = iwn_rf_gain_2ghz;
		dsp_gain = iwn_dsp_gain_2ghz;
	}

	/* compute voltage compensation */
	vdiff = ((int32_t)le32toh(uc->volt) - sc->eeprom_voltage) / 7;
	if (vdiff > 0)
		vdiff *= 2;
	if (abs(vdiff) > 2)
		vdiff = 0;
	DPRINTF(sc, IWN_DEBUG_CALIBRATE | IWN_DEBUG_TXPOW,
	    "%s: voltage compensation=%d (UCODE=%d, EEPROM=%d)\n",
	    __func__, vdiff, le32toh(uc->volt), sc->eeprom_voltage);

	/* get channel's attenuation group */
	if (chan <= 20)		/* 1-20 */
		grp = 4;
	else if (chan <= 43)	/* 34-43 */
		grp = 0;
	else if (chan <= 70)	/* 44-70 */
		grp = 1;
	else if (chan <= 124)	/* 71-124 */
		grp = 2;
	else			/* 125-200 */
		grp = 3;
	DPRINTF(sc, IWN_DEBUG_CALIBRATE | IWN_DEBUG_TXPOW,
	    "%s: chan %d, attenuation group=%d\n", __func__, chan, grp);

	/* get channel's sub-band */
	for (i = 0; i < IWN_NBANDS; i++)
		if (sc->bands[i].lo != 0 &&
		    sc->bands[i].lo <= chan && chan <= sc->bands[i].hi)
			break;
	chans = sc->bands[i].chans;
	DPRINTF(sc, IWN_DEBUG_CALIBRATE | IWN_DEBUG_TXPOW,
	    "%s: chan %d sub-band=%d\n", __func__, chan, i);

	for (c = 0; c < IWN_NTXCHAINS; c++) {
		uint8_t power, gain, temp;
		int maxchpwr, pwr, ridx, idx;

		power = interpolate(chan,
		    chans[0].num, chans[0].samples[c][1].power,
		    chans[1].num, chans[1].samples[c][1].power, 1);
		gain  = interpolate(chan,
		    chans[0].num, chans[0].samples[c][1].gain,
		    chans[1].num, chans[1].samples[c][1].gain, 1);
		temp  = interpolate(chan,
		    chans[0].num, chans[0].samples[c][1].temp,
		    chans[1].num, chans[1].samples[c][1].temp, 1);
		DPRINTF(sc, IWN_DEBUG_CALIBRATE | IWN_DEBUG_TXPOW,
		    "%s: Tx chain %d: power=%d gain=%d temp=%d\n",
		    __func__, c, power, gain, temp);

		/* compute temperature compensation */
		tdiff = ((sc->temp - temp) * 2) / tdiv[grp];
		DPRINTF(sc, IWN_DEBUG_CALIBRATE | IWN_DEBUG_TXPOW,
		    "%s: temperature compensation=%d (current=%d, EEPROM=%d)\n",
		    __func__, tdiff, sc->temp, temp);

		for (ridx = 0; ridx <= IWN_RIDX_MAX; ridx++) {
			maxchpwr = ch->ic_maxpower;
			if ((ridx / 8) & 1) {
				/* MIMO: decrease Tx power (-3dB) */
				maxchpwr -= 6;
			}

			pwr = maxpwr - 10;

			/* decrease power for highest OFDM rates */
			if ((ridx % 8) == 5)		/* 48Mbit/s */
				pwr -= 5;
			else if ((ridx % 8) == 6)	/* 54Mbit/s */
				pwr -= 7;
			else if ((ridx % 8) == 7)	/* 60Mbit/s */
				pwr -= 10;

			if (pwr > maxchpwr)
				pwr = maxchpwr;

			idx = gain - (pwr - power) - tdiff - vdiff;
			if ((ridx / 8) & 1)	/* MIMO */
				idx += (int32_t)le32toh(uc->atten[grp][c]);

			if (cmd.band == 0)
				idx += 9;	/* 5GHz */
			if (ridx == IWN_RIDX_MAX)
				idx += 5;	/* CCK */

			/* make sure idx stays in a valid range */
			if (idx < 0)
				idx = 0;
			else if (idx > IWN_MAX_PWR_INDEX)
				idx = IWN_MAX_PWR_INDEX;

			DPRINTF(sc, IWN_DEBUG_CALIBRATE | IWN_DEBUG_TXPOW,
			    "%s: Tx chain %d, rate idx %d: power=%d\n",
			    __func__, c, ridx, idx);
			cmd.power[ridx].rf_gain[c] = rf_gain[idx];
			cmd.power[ridx].dsp_gain[c] = dsp_gain[idx];
		}
	}

	DPRINTF(sc, IWN_DEBUG_CALIBRATE | IWN_DEBUG_TXPOW,
	    "%s: set tx power for chan %d\n", __func__, chan);
	return iwn_cmd(sc, IWN_CMD_TXPOWER, &cmd, sizeof cmd, async);

#undef interpolate
#undef fdivround
}

/*
 * Get the best (maximum) RSSI among the
 * connected antennas and convert to dBm.
 */
int8_t
iwn_get_rssi(struct iwn_softc *sc, const struct iwn_rx_stat *stat)
{
	int mask, agc, rssi;

	mask = (le16toh(stat->antenna) >> 4) & 0x7;
	agc  = (le16toh(stat->agc) >> 7) & 0x7f;

	rssi = 0;
#if 0
	if (mask & (1 << 0))	/* Ant A */
		rssi = max(rssi, stat->rssi[0]);
	if (mask & (1 << 1))	/* Ant B */
		rssi = max(rssi, stat->rssi[2]);
	if (mask & (1 << 2))	/* Ant C */
		rssi = max(rssi, stat->rssi[4]);
#else
	rssi = max(rssi, stat->rssi[0]);
	rssi = max(rssi, stat->rssi[2]);
	rssi = max(rssi, stat->rssi[4]);
#endif
	DPRINTF(sc, IWN_DEBUG_RECV, "%s: agc %d mask 0x%x rssi %d %d %d "
	    "result %d\n", __func__, agc, mask,
	    stat->rssi[0], stat->rssi[2], stat->rssi[4],
	    rssi - agc - IWN_RSSI_TO_DBM);
	return rssi - agc - IWN_RSSI_TO_DBM;
}

/*
 * Get the average noise among Rx antennas (in dBm).
 */
int
iwn_get_noise(const struct iwn_rx_general_stats *stats)
{
	int i, total, nbant, noise;

	total = nbant = 0;
	for (i = 0; i < 3; i++) {
		noise = le32toh(stats->noise[i]) & 0xff;
		if (noise != 0) {
			total += noise;
			nbant++;
		}
	}
	/* there should be at least one antenna but check anyway */
	return (nbant == 0) ? -127 : (total / nbant) - 107;
}

/*
 * Read temperature (in degC) from the on-board thermal sensor.
 */
int
iwn_get_temperature(struct iwn_softc *sc)
{
	struct iwn_ucode_info *uc = &sc->ucode_info;
	int32_t r1, r2, r3, r4, temp;

	r1 = le32toh(uc->temp[0].chan20MHz);
	r2 = le32toh(uc->temp[1].chan20MHz);
	r3 = le32toh(uc->temp[2].chan20MHz);
	r4 = le32toh(sc->rawtemp);

	if (r1 == r3)	/* prevents division by 0 (should not happen) */
		return 0;

	/* sign-extend 23-bit R4 value to 32-bit */
	r4 = (r4 << 8) >> 8;
	/* compute temperature */
	temp = (259 * (r4 - r2)) / (r3 - r1);
	temp = (temp * 97) / 100 + 8;

	return IWN_KTOC(temp);
}

/*
 * Initialize sensitivity calibration state machine.
 */
int
iwn_init_sensitivity(struct iwn_softc *sc)
{
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_phy_calib_cmd cmd;
	int error;

	/* reset calibration state */
	memset(calib, 0, sizeof (*calib));
	calib->state = IWN_CALIB_STATE_INIT;
	calib->cck_state = IWN_CCK_STATE_HIFA;
	/* initial values taken from the reference driver */
	calib->corr_ofdm_x1     = 105;
	calib->corr_ofdm_mrc_x1 = 220;
	calib->corr_ofdm_x4     =  90;
	calib->corr_ofdm_mrc_x4 = 170;
	calib->corr_cck_x4      = 125;
	calib->corr_cck_mrc_x4  = 200;
	calib->energy_cck       = 100;

	/* write initial sensitivity values */
	error = iwn_send_sensitivity(sc);
	if (error != 0)
		return error;

	memset(&cmd, 0, sizeof cmd);
	cmd.code = IWN_SET_DIFF_GAIN;
	/* differential gains initially set to 0 for all 3 antennas */
	DPRINTF(sc, IWN_DEBUG_CALIBRATE, "%s: calibrate phy\n", __func__);
	return iwn_cmd(sc, IWN_PHY_CALIB, &cmd, sizeof cmd, 1);
}

/*
 * Collect noise and RSSI statistics for the first 20 beacons received
 * after association and use them to determine connected antennas and
 * set differential gains.
 */
void
iwn_compute_differential_gain(struct iwn_softc *sc,
    const struct iwn_rx_general_stats *stats)
{
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_phy_calib_cmd cmd;
	int i, val;

	/* accumulate RSSI and noise for all 3 antennas */
	for (i = 0; i < 3; i++) {
		calib->rssi[i] += le32toh(stats->rssi[i]) & 0xff;
		calib->noise[i] += le32toh(stats->noise[i]) & 0xff;
	}

	/* we update differential gain only once after 20 beacons */
	if (++calib->nbeacons < 20)
		return;

	/* determine antenna with highest average RSSI */
	val = max(calib->rssi[0], calib->rssi[1]);
	val = max(calib->rssi[2], val);

	/* determine which antennas are connected */
	sc->antmsk = 0;
	for (i = 0; i < 3; i++)
		if (val - calib->rssi[i] <= 15 * 20)
			sc->antmsk |= 1 << i;
	/* if neither Ant A and Ant B are connected.. */
	if ((sc->antmsk & (1 << 0 | 1 << 1)) == 0)
		sc->antmsk |= 1 << 1;	/* ..mark Ant B as connected! */

	/* get minimal noise among connected antennas */
	val = INT_MAX;	/* ok, there's at least one */
	for (i = 0; i < 3; i++)
		if (sc->antmsk & (1 << i))
			val = min(calib->noise[i], val);

	memset(&cmd, 0, sizeof cmd);
	cmd.code = IWN_SET_DIFF_GAIN;
	/* set differential gains for connected antennas */
	for (i = 0; i < 3; i++) {
		if (sc->antmsk & (1 << i)) {
			cmd.gain[i] = (calib->noise[i] - val) / 30;
			/* limit differential gain to 3 */
			cmd.gain[i] = min(cmd.gain[i], 3);
			cmd.gain[i] |= IWN_GAIN_SET;
		}
	}
	DPRINTF(sc, IWN_DEBUG_CALIBRATE,
	    "%s: set differential gains Ant A/B/C: %x/%x/%x (%x)\n",
	    __func__,cmd.gain[0], cmd.gain[1], cmd.gain[2], sc->antmsk);
	if (iwn_cmd(sc, IWN_PHY_CALIB, &cmd, sizeof cmd, 1) == 0)
		calib->state = IWN_CALIB_STATE_RUN;
}

/*
 * Tune RF Rx sensitivity based on the number of false alarms detected
 * during the last beacon period.
 */
void
iwn_tune_sensitivity(struct iwn_softc *sc, const struct iwn_rx_stats *stats)
{
#define inc_clip(val, inc, max)			\
	if ((val) < (max)) {			\
		if ((val) < (max) - (inc))	\
			(val) += (inc);		\
		else				\
			(val) = (max);		\
		needs_update = 1;		\
	}
#define dec_clip(val, dec, min)			\
	if ((val) > (min)) {			\
		if ((val) > (min) + (dec))	\
			(val) -= (dec);		\
		else				\
			(val) = (min);		\
		needs_update = 1;		\
	}

	struct iwn_calib_state *calib = &sc->calib;
	uint32_t val, rxena, fa;
	uint32_t energy[3], energy_min;
	uint8_t noise[3], noise_ref;
	int i, needs_update = 0;

	/* check that we've been enabled long enough */
	if ((rxena = le32toh(stats->general.load)) == 0)
		return;

	/* compute number of false alarms since last call for OFDM */
	fa  = le32toh(stats->ofdm.bad_plcp) - calib->bad_plcp_ofdm;
	fa += le32toh(stats->ofdm.fa) - calib->fa_ofdm;
	fa *= 200 * 1024;	/* 200TU */

	/* save counters values for next call */
	calib->bad_plcp_ofdm = le32toh(stats->ofdm.bad_plcp);
	calib->fa_ofdm = le32toh(stats->ofdm.fa);

	if (fa > 50 * rxena) {
		/* high false alarm count, decrease sensitivity */
		DPRINTF(sc, IWN_DEBUG_CALIBRATE,
		    "%s: OFDM high false alarm count: %u\n", __func__, fa);
		inc_clip(calib->corr_ofdm_x1,     1, 140);
		inc_clip(calib->corr_ofdm_mrc_x1, 1, 270);
		inc_clip(calib->corr_ofdm_x4,     1, 120);
		inc_clip(calib->corr_ofdm_mrc_x4, 1, 210);

	} else if (fa < 5 * rxena) {
		/* low false alarm count, increase sensitivity */
		DPRINTF(sc, IWN_DEBUG_CALIBRATE,
		    "%s: OFDM low false alarm count: %u\n", __func__, fa);
		dec_clip(calib->corr_ofdm_x1,     1, 105);
		dec_clip(calib->corr_ofdm_mrc_x1, 1, 220);
		dec_clip(calib->corr_ofdm_x4,     1,  85);
		dec_clip(calib->corr_ofdm_mrc_x4, 1, 170);
	}

	/* compute maximum noise among 3 antennas */
	for (i = 0; i < 3; i++)
		noise[i] = (le32toh(stats->general.noise[i]) >> 8) & 0xff;
	val = max(noise[0], noise[1]);
	val = max(noise[2], val);
	/* insert it into our samples table */
	calib->noise_samples[calib->cur_noise_sample] = val;
	calib->cur_noise_sample = (calib->cur_noise_sample + 1) % 20;

	/* compute maximum noise among last 20 samples */
	noise_ref = calib->noise_samples[0];
	for (i = 1; i < 20; i++)
		noise_ref = max(noise_ref, calib->noise_samples[i]);

	/* compute maximum energy among 3 antennas */
	for (i = 0; i < 3; i++)
		energy[i] = le32toh(stats->general.energy[i]);
	val = min(energy[0], energy[1]);
	val = min(energy[2], val);
	/* insert it into our samples table */
	calib->energy_samples[calib->cur_energy_sample] = val;
	calib->cur_energy_sample = (calib->cur_energy_sample + 1) % 10;

	/* compute minimum energy among last 10 samples */
	energy_min = calib->energy_samples[0];
	for (i = 1; i < 10; i++)
		energy_min = max(energy_min, calib->energy_samples[i]);
	energy_min += 6;

	/* compute number of false alarms since last call for CCK */
	fa  = le32toh(stats->cck.bad_plcp) - calib->bad_plcp_cck;
	fa += le32toh(stats->cck.fa) - calib->fa_cck;
	fa *= 200 * 1024;	/* 200TU */

	/* save counters values for next call */
	calib->bad_plcp_cck = le32toh(stats->cck.bad_plcp);
	calib->fa_cck = le32toh(stats->cck.fa);

	if (fa > 50 * rxena) {
		/* high false alarm count, decrease sensitivity */
		DPRINTF(sc, IWN_DEBUG_CALIBRATE,
		    "%s: CCK high false alarm count: %u\n", __func__, fa);
		calib->cck_state = IWN_CCK_STATE_HIFA;
		calib->low_fa = 0;

		if (calib->corr_cck_x4 > 160) {
			calib->noise_ref = noise_ref;
			if (calib->energy_cck > 2)
				dec_clip(calib->energy_cck, 2, energy_min);
		}
		if (calib->corr_cck_x4 < 160) {
			calib->corr_cck_x4 = 161;
			needs_update = 1;
		} else
			inc_clip(calib->corr_cck_x4, 3, 200);

		inc_clip(calib->corr_cck_mrc_x4, 3, 400);

	} else if (fa < 5 * rxena) {
		/* low false alarm count, increase sensitivity */
		DPRINTF(sc, IWN_DEBUG_CALIBRATE,
		    "%s: CCK low false alarm count: %u\n", __func__, fa);
		calib->cck_state = IWN_CCK_STATE_LOFA;
		calib->low_fa++;

		if (calib->cck_state != 0 &&
		    ((calib->noise_ref - noise_ref) > 2 ||
		     calib->low_fa > 100)) {
			inc_clip(calib->energy_cck,      2,  97);
			dec_clip(calib->corr_cck_x4,     3, 125);
			dec_clip(calib->corr_cck_mrc_x4, 3, 200);
		}
	} else {
		/* not worth to increase or decrease sensitivity */
		DPRINTF(sc, IWN_DEBUG_CALIBRATE,
		    "%s: CCK normal false alarm count: %u\n", __func__, fa);
		calib->low_fa = 0;
		calib->noise_ref = noise_ref;

		if (calib->cck_state == IWN_CCK_STATE_HIFA) {
			/* previous interval had many false alarms */
			dec_clip(calib->energy_cck, 8, energy_min);
		}
		calib->cck_state = IWN_CCK_STATE_INIT;
	}

	if (needs_update)
		(void)iwn_send_sensitivity(sc);
#undef dec_clip
#undef inc_clip
}

int
iwn_send_sensitivity(struct iwn_softc *sc)
{
	struct iwn_calib_state *calib = &sc->calib;
	struct iwn_sensitivity_cmd cmd;

	memset(&cmd, 0, sizeof cmd);
	cmd.which = IWN_SENSITIVITY_WORKTBL;
	/* OFDM modulation */
	cmd.corr_ofdm_x1     = htole16(calib->corr_ofdm_x1);
	cmd.corr_ofdm_mrc_x1 = htole16(calib->corr_ofdm_mrc_x1);
	cmd.corr_ofdm_x4     = htole16(calib->corr_ofdm_x4);
	cmd.corr_ofdm_mrc_x4 = htole16(calib->corr_ofdm_mrc_x4);
	cmd.energy_ofdm      = htole16(100);
	cmd.energy_ofdm_th   = htole16(62);
	/* CCK modulation */
	cmd.corr_cck_x4      = htole16(calib->corr_cck_x4);
	cmd.corr_cck_mrc_x4  = htole16(calib->corr_cck_mrc_x4);
	cmd.energy_cck       = htole16(calib->energy_cck);
	/* Barker modulation: use default values */
	cmd.corr_barker      = htole16(190);
	cmd.corr_barker_mrc  = htole16(390);

	DPRINTF(sc, IWN_DEBUG_RESET, 
	    "%s: set sensitivity %d/%d/%d/%d/%d/%d/%d\n", __func__,
	    calib->corr_ofdm_x1, calib->corr_ofdm_mrc_x1, calib->corr_ofdm_x4,
	    calib->corr_ofdm_mrc_x4, calib->corr_cck_x4,
	    calib->corr_cck_mrc_x4, calib->energy_cck);
	return iwn_cmd(sc, IWN_SENSITIVITY, &cmd, sizeof cmd, 1);
}

int
iwn_auth(struct iwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);	/*XXX*/
	struct ieee80211_node *ni = vap->iv_bss;
	struct iwn_node_info node;
	int error;

	sc->calib.state = IWN_CALIB_STATE_INIT;

	/* update adapter's configuration */
	sc->config.associd = 0;
	IEEE80211_ADDR_COPY(sc->config.bssid, ni->ni_bssid);
	sc->config.chan = htole16(ieee80211_chan2ieee(ic, ni->ni_chan));
	sc->config.flags = htole32(IWN_CONFIG_TSF);
	if (IEEE80211_IS_CHAN_2GHZ(ni->ni_chan))
		sc->config.flags |= htole32(IWN_CONFIG_AUTO | IWN_CONFIG_24GHZ);
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
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		sc->config.flags |= htole32(IWN_CONFIG_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		sc->config.flags |= htole32(IWN_CONFIG_SHPREAMBLE);
	sc->config.filter &= ~htole32(IWN_FILTER_BSS);

	DPRINTF(sc, IWN_DEBUG_STATE,
	   "%s: config chan %d mode %d flags 0x%x cck 0x%x ofdm 0x%x "
	   "ht_single 0x%x ht_dual 0x%x rxchain 0x%x "
	   "myaddr %6D wlap %6D bssid %6D associd %d filter 0x%x\n",
	   __func__,
	   le16toh(sc->config.chan), sc->config.mode, le32toh(sc->config.flags),
	   sc->config.cck_mask, sc->config.ofdm_mask,
	   sc->config.ht_single_mask, sc->config.ht_dual_mask,
	   le16toh(sc->config.rxchain),
	   sc->config.myaddr, ":", sc->config.wlap, ":", sc->config.bssid, ":",
	   le16toh(sc->config.associd), le32toh(sc->config.filter));
	error = iwn_cmd(sc, IWN_CMD_CONFIGURE, &sc->config,
	    sizeof (struct iwn_config), 1);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not configure, error %d\n", __func__, error);
		return error;
	}
	sc->sc_curchan = ic->ic_curchan;

	/* configuration has changed, set Tx power accordingly */
	error = iwn_set_txpower(sc, ni->ni_chan, 1);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not set Tx power, error %d\n", __func__, error);
		return error;
	}

	/*
	 * Reconfiguring clears the adapter's nodes table so we must
	 * add the broadcast node again.
	 */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, ifp->if_broadcastaddr);
	node.id = IWN_ID_BROADCAST;
	DPRINTF(sc, IWN_DEBUG_STATE, "%s: add broadcast node\n", __func__);
	error = iwn_cmd(sc, IWN_CMD_ADD_NODE, &node, sizeof node, 1);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not add broadcast node, error %d\n",
		    __func__, error);
		return error;
	}
	error = iwn_set_link_quality(sc, node.id, ic->ic_curchan, 1);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not setup MRR for broadcast node, error %d\n",
		    __func__, error);
		return error;
	}

	return 0;
}

/*
 * Configure the adapter for associated state.
 */
int
iwn_run(struct iwn_softc *sc)
{
#define	MS(v,x)	(((v) & x) >> x##_S)
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);	/*XXX*/
	struct ieee80211_node *ni = vap->iv_bss;
	struct iwn_node_info node;
	int error, maxrxampdu, ampdudensity;

	sc->calib.state = IWN_CALIB_STATE_INIT;

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		/* link LED blinks while monitoring */
		iwn_set_led(sc, IWN_LED_LINK, 5, 5);
		return 0;
	}

	iwn_enable_tsf(sc, ni);

	/* update adapter's configuration */
	sc->config.associd = htole16(IEEE80211_AID(ni->ni_associd));
	/* short preamble/slot time are negotiated when associating */
	sc->config.flags &= ~htole32(IWN_CONFIG_SHPREAMBLE | IWN_CONFIG_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		sc->config.flags |= htole32(IWN_CONFIG_SHSLOT);
	if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
		sc->config.flags |= htole32(IWN_CONFIG_SHPREAMBLE);
	if (IEEE80211_IS_CHAN_HT(ni->ni_chan)) {
		sc->config.flags &= ~htole32(IWN_CONFIG_HT);
		if (IEEE80211_IS_CHAN_HT40U(ni->ni_chan))
			sc->config.flags |= htole32(IWN_CONFIG_HT40U);
		else if (IEEE80211_IS_CHAN_HT40D(ni->ni_chan))
			sc->config.flags |= htole32(IWN_CONFIG_HT40D);
		else
			sc->config.flags |= htole32(IWN_CONFIG_HT20);
		sc->config.rxchain = htole16(
			  (3 << IWN_RXCHAIN_VALID_S)
			| (3 << IWN_RXCHAIN_MIMO_CNT_S)
			| (1 << IWN_RXCHAIN_CNT_S)
			| IWN_RXCHAIN_MIMO_FORCE);

		maxrxampdu = MS(ni->ni_htparam, IEEE80211_HTCAP_MAXRXAMPDU);
		ampdudensity = MS(ni->ni_htparam, IEEE80211_HTCAP_MPDUDENSITY);
	} else
		maxrxampdu = ampdudensity = 0;
	sc->config.filter |= htole32(IWN_FILTER_BSS);

	DPRINTF(sc, IWN_DEBUG_STATE,
	   "%s: config chan %d mode %d flags 0x%x cck 0x%x ofdm 0x%x "
	   "ht_single 0x%x ht_dual 0x%x rxchain 0x%x "
	   "myaddr %6D wlap %6D bssid %6D associd %d filter 0x%x\n",
	   __func__,
	   le16toh(sc->config.chan), sc->config.mode, le32toh(sc->config.flags),
	   sc->config.cck_mask, sc->config.ofdm_mask,
	   sc->config.ht_single_mask, sc->config.ht_dual_mask,
	   le16toh(sc->config.rxchain),
	   sc->config.myaddr, ":", sc->config.wlap, ":", sc->config.bssid, ":",
	   le16toh(sc->config.associd), le32toh(sc->config.filter));
	error = iwn_cmd(sc, IWN_CMD_CONFIGURE, &sc->config,
	    sizeof (struct iwn_config), 1);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not update configuration, error %d\n",
		    __func__, error);
		return error;
	}
	sc->sc_curchan = ni->ni_chan;

	/* configuration has changed, set Tx power accordingly */
	error = iwn_set_txpower(sc, ni->ni_chan, 1);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not set Tx power, error %d\n", __func__, error);
		return error;
	}

	/* add BSS node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, ni->ni_macaddr);
	node.id = IWN_ID_BSS;
	node.htflags = htole32(
	    (maxrxampdu << IWN_MAXRXAMPDU_S) |
	    (ampdudensity << IWN_MPDUDENSITY_S));
	DPRINTF(sc, IWN_DEBUG_STATE, "%s: add BSS node, id %d htflags 0x%x\n",
	    __func__, node.id, le32toh(node.htflags));
	error = iwn_cmd(sc, IWN_CMD_ADD_NODE, &node, sizeof node, 1);
	if (error != 0) {
		device_printf(sc->sc_dev,"could not add BSS node\n");
		return error;
	}
	error = iwn_set_link_quality(sc, node.id, ni->ni_chan, 1);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not setup MRR for node %d, error %d\n",
		    __func__, node.id, error);
		return error;
	}

	if (ic->ic_opmode == IEEE80211_M_STA) {
		/* fake a join to init the tx rate */
		iwn_newassoc(ni, 1);
	}

	error = iwn_init_sensitivity(sc);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not set sensitivity, error %d\n",
		    __func__, error);
		return error;
	}

	/* start/restart periodic calibration timer */
	sc->calib.state = IWN_CALIB_STATE_ASSOC;
	iwn_calib_reset(sc);

	/* link LED always on while associated */
	iwn_set_led(sc, IWN_LED_LINK, 0, 1);

	return 0;
#undef MS
}

/*
 * Send a scan request to the firmware.  Since this command is huge, we map it
 * into a mbuf instead of using the pre-allocated set of commands.
 */
int
iwn_scan(struct iwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211_scan_state *ss = ic->ic_scan;	/*XXX*/
	struct iwn_tx_ring *ring = &sc->txq[4];
	struct iwn_tx_desc *desc;
	struct iwn_tx_data *data;
	struct iwn_tx_cmd *cmd;
	struct iwn_cmd_data *tx;
	struct iwn_scan_hdr *hdr;
	struct iwn_scan_essid *essid;
	struct iwn_scan_chan *chan;
	struct ieee80211_frame *wh;
	struct ieee80211_rateset *rs;
	struct ieee80211_channel *c;
	enum ieee80211_phymode mode;
	uint8_t *frm;
	int pktlen, error, nrates;
	bus_addr_t physaddr;

	desc = &ring->desc[ring->cur];
	data = &ring->data[ring->cur];

	/* XXX malloc */
	data->m = m_getcl(M_DONTWAIT, MT_DATA, 0);
	if (data->m == NULL) {
		device_printf(sc->sc_dev,
		    "%s: could not allocate mbuf for scan command\n", __func__);
		return ENOMEM;
	}

	cmd = mtod(data->m, struct iwn_tx_cmd *);
	cmd->code = IWN_CMD_SCAN;
	cmd->flags = 0;
	cmd->qid = ring->qid;
	cmd->idx = ring->cur;

	hdr = (struct iwn_scan_hdr *)cmd->data;
	memset(hdr, 0, sizeof (struct iwn_scan_hdr));

	/* XXX use scan state */
	/*
	 * Move to the next channel if no packets are received within 5 msecs
	 * after sending the probe request (this helps to reduce the duration
	 * of active scans).
	 */
	hdr->quiet = htole16(5);	/* timeout in milliseconds */
	hdr->plcp_threshold = htole16(1);	/* min # of packets */

	/* select Ant B and Ant C for scanning */
	hdr->rxchain = htole16(0x3e1 | (7 << IWN_RXCHAIN_VALID_S));

	tx = (struct iwn_cmd_data *)(hdr + 1);
	memset(tx, 0, sizeof (struct iwn_cmd_data));
	tx->flags = htole32(IWN_TX_AUTO_SEQ | 0x200);	/* XXX */
	tx->id = IWN_ID_BROADCAST;
	tx->lifetime = htole32(IWN_LIFETIME_INFINITE);
	tx->rflags = IWN_RFLAG_ANT_B;

	if (IEEE80211_IS_CHAN_A(ic->ic_curchan)) {
		hdr->crc_threshold = htole16(1);
		/* send probe requests at 6Mbps */
		tx->rate = iwn_ridx_to_plcp[IWN_RATE_OFDM6];
	} else {
		hdr->flags = htole32(IWN_CONFIG_24GHZ | IWN_CONFIG_AUTO);
		/* send probe requests at 1Mbps */
		tx->rate = iwn_ridx_to_plcp[IWN_RATE_CCK1];
		tx->rflags |= IWN_RFLAG_CCK;
	}

	essid = (struct iwn_scan_essid *)(tx + 1);
	memset(essid, 0, 4 * sizeof (struct iwn_scan_essid));
	essid[0].id  = IEEE80211_ELEMID_SSID;
	essid[0].len = ss->ss_ssid[0].len;
	memcpy(essid[0].data, ss->ss_ssid[0].ssid, ss->ss_ssid[0].len);

	/*
	 * Build a probe request frame.  Most of the following code is a
	 * copy & paste of what is done in net80211.
	 */
	wh = (struct ieee80211_frame *)&essid[4];
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_PROBE_REQ;
	wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	IEEE80211_ADDR_COPY(wh->i_addr1, ifp->if_broadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ifp->if_broadcastaddr);
	*(u_int16_t *)&wh->i_dur[0] = 0;	/* filled by h/w */
	*(u_int16_t *)&wh->i_seq[0] = 0;	/* filled by h/w */

	frm = (uint8_t *)(wh + 1);

	/* add SSID IE */
        *frm++ = IEEE80211_ELEMID_SSID;
        *frm++ = ss->ss_ssid[0].len;
        memcpy(frm, ss->ss_ssid[0].ssid, ss->ss_ssid[0].len);
	frm += ss->ss_ssid[0].len;

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
		*frm++ = (uint8_t)nrates;
		memcpy(frm, rs->rs_rates + IEEE80211_RATE_SIZE, nrates);
		frm += nrates;
	}

	/* setup length of probe request */
	tx->len = htole16(frm - (uint8_t *)wh);

	c = ic->ic_curchan;
	chan = (struct iwn_scan_chan *)frm;
	chan->chan = ieee80211_chan2ieee(ic, c);
	chan->flags = 0;
	if ((c->ic_flags & IEEE80211_CHAN_PASSIVE) == 0) {
		chan->flags |= IWN_CHAN_ACTIVE;
		if (ss->ss_nssid > 0)
			chan->flags |= IWN_CHAN_DIRECT;
	}
	chan->dsp_gain = 0x6e;
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		chan->rf_gain = 0x3b;
		chan->active  = htole16(10);
		chan->passive = htole16(110);
	} else {
		chan->rf_gain = 0x28;
		chan->active  = htole16(20);
		chan->passive = htole16(120);
	}

	DPRINTF(sc, IWN_DEBUG_STATE, "%s: chan %u flags 0x%x rf_gain 0x%x "
	    "dsp_gain 0x%x active 0x%x passive 0x%x\n", __func__,
	    chan->chan, chan->flags, chan->rf_gain, chan->dsp_gain,
	    chan->active, chan->passive);
	hdr->nchan++;
	chan++;

	frm += sizeof (struct iwn_scan_chan);

	hdr->len = htole16(frm - (uint8_t *)hdr);
	pktlen = frm - (uint8_t *)cmd;

	error = bus_dmamap_load(ring->data_dmat, data->map, cmd, pktlen,
	    iwn_dma_map_addr, &physaddr, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not map scan command, error %d\n",
		    __func__, error);
		m_freem(data->m);
		data->m = NULL;
		return error;
	}

	IWN_SET_DESC_NSEGS(desc, 1);
	IWN_SET_DESC_SEG(desc, 0, physaddr, pktlen);
	sc->shared->len[ring->qid][ring->cur] = htole16(8);
	if (ring->cur < IWN_TX_WINDOW)
		sc->shared->len[ring->qid][ring->cur + IWN_TX_RING_COUNT] =
		    htole16(8);

	bus_dmamap_sync(ring->desc_dma.tag, ring->desc_dma.map,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_PREWRITE);

	/* kick cmd ring */
	ring->cur = (ring->cur + 1) % IWN_TX_RING_COUNT;
	IWN_WRITE(sc, IWN_TX_WIDX, ring->qid << 8 | ring->cur);

	return 0;	/* will be notified async. of failure/success */
}

int
iwn_config(struct iwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct iwn_power power;
	struct iwn_bluetooth bluetooth;
	struct iwn_node_info node;
	int error;

	/* set power mode */
	memset(&power, 0, sizeof power);
	power.flags = htole16(IWN_POWER_CAM | 0x8);
	DPRINTF(sc, IWN_DEBUG_RESET, "%s: set power mode\n", __func__);
	error = iwn_cmd(sc, IWN_CMD_SET_POWER_MODE, &power, sizeof power, 0);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not set power mode, error %d\n",
		    __func__, error);
		return error;
	}

	/* configure bluetooth coexistence */
	memset(&bluetooth, 0, sizeof bluetooth);
	bluetooth.flags = 3;
	bluetooth.lead = 0xaa;
	bluetooth.kill = 1;
	DPRINTF(sc, IWN_DEBUG_RESET, "%s: config bluetooth coexistence\n",
	    __func__);
	error = iwn_cmd(sc, IWN_CMD_BLUETOOTH, &bluetooth, sizeof bluetooth,
	    0);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not configure bluetooth coexistence, error %d\n",
		    __func__, error);
		return error;
	}

	/* configure adapter */
	memset(&sc->config, 0, sizeof (struct iwn_config));
	IEEE80211_ADDR_COPY(sc->config.myaddr, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(sc->config.wlap, ic->ic_myaddr);
	/* set default channel */
	sc->config.chan = htole16(ieee80211_chan2ieee(ic, ic->ic_curchan));
	sc->config.flags = htole32(IWN_CONFIG_TSF);
	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
		sc->config.flags |= htole32(IWN_CONFIG_AUTO | IWN_CONFIG_24GHZ);
	sc->config.filter = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		sc->config.mode = IWN_MODE_STA;
		sc->config.filter |= htole32(IWN_FILTER_MULTICAST);
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		sc->config.mode = IWN_MODE_IBSS;
		break;
	case IEEE80211_M_HOSTAP:
		sc->config.mode = IWN_MODE_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		sc->config.mode = IWN_MODE_MONITOR;
		sc->config.filter |= htole32(IWN_FILTER_MULTICAST |
		    IWN_FILTER_CTL | IWN_FILTER_PROMISC);
		break;
	default:
		break;
	}
	sc->config.cck_mask  = 0x0f;	/* not yet negotiated */
	sc->config.ofdm_mask = 0xff;	/* not yet negotiated */
	sc->config.ht_single_mask = 0xff;
	sc->config.ht_dual_mask = 0xff;
	sc->config.rxchain = htole16(0x2800 | (7 << IWN_RXCHAIN_VALID_S));

	DPRINTF(sc, IWN_DEBUG_STATE,
	   "%s: config chan %d mode %d flags 0x%x cck 0x%x ofdm 0x%x "
	   "ht_single 0x%x ht_dual 0x%x rxchain 0x%x "
	   "myaddr %6D wlap %6D bssid %6D associd %d filter 0x%x\n",
	   __func__,
	   le16toh(sc->config.chan), sc->config.mode, le32toh(sc->config.flags),
	   sc->config.cck_mask, sc->config.ofdm_mask,
	   sc->config.ht_single_mask, sc->config.ht_dual_mask,
	   le16toh(sc->config.rxchain),
	   sc->config.myaddr, ":", sc->config.wlap, ":", sc->config.bssid, ":",
	   le16toh(sc->config.associd), le32toh(sc->config.filter));
	error = iwn_cmd(sc, IWN_CMD_CONFIGURE, &sc->config,
	    sizeof (struct iwn_config), 0);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: configure command failed, error %d\n",
		    __func__, error);
		return error;
	}
	sc->sc_curchan = ic->ic_curchan;

	/* configuration has changed, set Tx power accordingly */
	error = iwn_set_txpower(sc, ic->ic_curchan, 0);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not set Tx power, error %d\n", __func__, error);
		return error;
	}

	/* add broadcast node */
	memset(&node, 0, sizeof node);
	IEEE80211_ADDR_COPY(node.macaddr, ic->ic_ifp->if_broadcastaddr);
	node.id = IWN_ID_BROADCAST;
	node.rate = iwn_plcp_signal(2);
	DPRINTF(sc, IWN_DEBUG_RESET, "%s: add broadcast node\n", __func__);
	error = iwn_cmd(sc, IWN_CMD_ADD_NODE, &node, sizeof node, 0);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not add broadcast node, error %d\n",
		    __func__, error);
		return error;
	}
	error = iwn_set_link_quality(sc, node.id, ic->ic_curchan, 0);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not setup MRR for node %d, error %d\n",
		    __func__, node.id, error);
		return error;
	}

	error = iwn_set_critical_temp(sc);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not set critical temperature, error %d\n",
		    __func__, error);
		return error;
	}
	return 0;
}

/*
 * Do post-alive initialization of the NIC (after firmware upload).
 */
void
iwn_post_alive(struct iwn_softc *sc)
{
	uint32_t base;
	uint16_t offset;
	int qid;

	iwn_mem_lock(sc);

	/* clear SRAM */
	base = iwn_mem_read(sc, IWN_SRAM_BASE);
	for (offset = 0x380; offset < 0x520; offset += 4) {
		IWN_WRITE(sc, IWN_MEM_WADDR, base + offset);
		IWN_WRITE(sc, IWN_MEM_WDATA, 0);
	}

	/* shared area is aligned on a 1K boundary */
	iwn_mem_write(sc, IWN_SRAM_BASE, sc->shared_dma.paddr >> 10);
	iwn_mem_write(sc, IWN_SELECT_QCHAIN, 0);

	for (qid = 0; qid < IWN_NTXQUEUES; qid++) {
		iwn_mem_write(sc, IWN_QUEUE_RIDX(qid), 0);
		IWN_WRITE(sc, IWN_TX_WIDX, qid << 8 | 0);

		/* set sched. window size */
		IWN_WRITE(sc, IWN_MEM_WADDR, base + IWN_QUEUE_OFFSET(qid));
		IWN_WRITE(sc, IWN_MEM_WDATA, 64);
		/* set sched. frame limit */
		IWN_WRITE(sc, IWN_MEM_WADDR, base + IWN_QUEUE_OFFSET(qid) + 4);
		IWN_WRITE(sc, IWN_MEM_WDATA, 10 << 16);
	}

	/* enable interrupts for all 16 queues */
	iwn_mem_write(sc, IWN_QUEUE_INTR_MASK, 0xffff);

	/* identify active Tx rings (0-7) */
	iwn_mem_write(sc, IWN_TX_ACTIVE, 0xff);

	/* mark Tx rings (4 EDCA + cmd + 2 HCCA) as active */
	for (qid = 0; qid < 7; qid++) {
		iwn_mem_write(sc, IWN_TXQ_STATUS(qid),
		    IWN_TXQ_STATUS_ACTIVE | qid << 1);
	}

	iwn_mem_unlock(sc);
}

void
iwn_stop_master(struct iwn_softc *sc)
{
	uint32_t tmp;
	int ntries;

	tmp = IWN_READ(sc, IWN_RESET);
	IWN_WRITE(sc, IWN_RESET, tmp | IWN_STOP_MASTER);

	tmp = IWN_READ(sc, IWN_GPIO_CTL);
	if ((tmp & IWN_GPIO_PWR_STATUS) == IWN_GPIO_PWR_SLEEP)
		return;	/* already asleep */

	for (ntries = 0; ntries < 100; ntries++) {
		if (IWN_READ(sc, IWN_RESET) & IWN_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 100)
		device_printf(sc->sc_dev,
		    "%s: timeout waiting for master\n", __func__);
}

int
iwn_reset(struct iwn_softc *sc)
{
	uint32_t tmp;
	int ntries;

	/* clear any pending interrupts */
	IWN_WRITE(sc, IWN_INTR, 0xffffffff);

	tmp = IWN_READ(sc, IWN_CHICKEN);
	IWN_WRITE(sc, IWN_CHICKEN, tmp | IWN_CHICKEN_DISLOS);

	tmp = IWN_READ(sc, IWN_GPIO_CTL);
	IWN_WRITE(sc, IWN_GPIO_CTL, tmp | IWN_GPIO_INIT);

	/* wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (IWN_READ(sc, IWN_GPIO_CTL) & IWN_GPIO_CLOCK)
			break;
		DELAY(10);
	}
	if (ntries == 1000) {
		device_printf(sc->sc_dev,
		    "%s: timeout waiting for clock stabilization\n", __func__);
		return ETIMEDOUT;
	}
	return 0;
}

void
iwn_hw_config(struct iwn_softc *sc)
{
	uint32_t tmp, hw;

	/* enable interrupts mitigation */
	IWN_WRITE(sc, IWN_INTR_MIT, 512 / 32);

	/* voodoo from the reference driver */
	tmp = pci_read_config(sc->sc_dev, PCIR_REVID,1);
	if ((tmp & 0x80) && (tmp & 0x7f) < 8) {
		/* enable "no snoop" field */
		tmp = pci_read_config(sc->sc_dev, 0xe8, 1);
		tmp &= ~IWN_DIS_NOSNOOP;
		/* clear device specific PCI configuration register 0x41 */
		pci_write_config(sc->sc_dev, 0xe8, tmp, 1);
	}

	/* disable L1 entry to work around a hardware bug */
	tmp = pci_read_config(sc->sc_dev, 0xf0, 1);
	tmp &= ~IWN_ENA_L1;
	pci_write_config(sc->sc_dev, 0xf0, tmp, 1 );

	hw = IWN_READ(sc, IWN_HWCONFIG);
	IWN_WRITE(sc, IWN_HWCONFIG, hw | 0x310);

	iwn_mem_lock(sc);
	tmp = iwn_mem_read(sc, IWN_MEM_POWER);
	iwn_mem_write(sc, IWN_MEM_POWER, tmp | IWN_POWER_RESET);
	DELAY(5);
	tmp = iwn_mem_read(sc, IWN_MEM_POWER);
	iwn_mem_write(sc, IWN_MEM_POWER, tmp & ~IWN_POWER_RESET);
	iwn_mem_unlock(sc);
}

void
iwn_init_locked(struct iwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t tmp;
	int error, qid;

	IWN_LOCK_ASSERT(sc);

	/* load the firmware */
	if (sc->fw_fp == NULL && (error = iwn_load_firmware(sc)) != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not load firmware, error %d\n", __func__, error);
		return;
	}

	error = iwn_reset(sc);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not reset adapter, error %d\n", __func__, error);
		return;
	}

	iwn_mem_lock(sc);
	iwn_mem_read(sc, IWN_CLOCK_CTL);
	iwn_mem_write(sc, IWN_CLOCK_CTL, 0xa00);
	iwn_mem_read(sc, IWN_CLOCK_CTL);
	iwn_mem_unlock(sc);

	DELAY(20);

	iwn_mem_lock(sc);
	tmp = iwn_mem_read(sc, IWN_MEM_PCIDEV);
	iwn_mem_write(sc, IWN_MEM_PCIDEV, tmp | 0x800);
	iwn_mem_unlock(sc);

	iwn_mem_lock(sc);
	tmp = iwn_mem_read(sc, IWN_MEM_POWER);
	iwn_mem_write(sc, IWN_MEM_POWER, tmp & ~0x03000000);
	iwn_mem_unlock(sc);

	iwn_hw_config(sc);

	/* init Rx ring */
	iwn_mem_lock(sc);
	IWN_WRITE(sc, IWN_RX_CONFIG, 0);
	IWN_WRITE(sc, IWN_RX_WIDX, 0);
	/* Rx ring is aligned on a 256-byte boundary */
	IWN_WRITE(sc, IWN_RX_BASE, sc->rxq.desc_dma.paddr >> 8);
	/* shared area is aligned on a 16-byte boundary */
	IWN_WRITE(sc, IWN_RW_WIDX_PTR, (sc->shared_dma.paddr +
	    offsetof(struct iwn_shared, closed_count)) >> 4);
	IWN_WRITE(sc, IWN_RX_CONFIG, 0x80601000);
	iwn_mem_unlock(sc);

	IWN_WRITE(sc, IWN_RX_WIDX, (IWN_RX_RING_COUNT - 1) & ~7);

	iwn_mem_lock(sc);
	iwn_mem_write(sc, IWN_TX_ACTIVE, 0);

	/* set physical address of "keep warm" page */
	IWN_WRITE(sc, IWN_KW_BASE, sc->kw_dma.paddr >> 4);

	/* init Tx rings */
	for (qid = 0; qid < IWN_NTXQUEUES; qid++) {
		struct iwn_tx_ring *txq = &sc->txq[qid];
		IWN_WRITE(sc, IWN_TX_BASE(qid), txq->desc_dma.paddr >> 8);
		IWN_WRITE(sc, IWN_TX_CONFIG(qid), 0x80000008);
	}
	iwn_mem_unlock(sc);

	/* clear "radio off" and "disable command" bits (reversed logic) */
	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_RADIO_OFF);
	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_DISABLE_CMD);

	/* clear any pending interrupts */
	IWN_WRITE(sc, IWN_INTR, 0xffffffff);
	/* enable interrupts */
	IWN_WRITE(sc, IWN_MASK, IWN_INTR_MASK);

	/* not sure why/if this is necessary... */
	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_RADIO_OFF);
	IWN_WRITE(sc, IWN_UCODE_CLR, IWN_RADIO_OFF);

	/* check that the radio is not disabled by RF switch */
	if (!(IWN_READ(sc, IWN_GPIO_CTL) & IWN_GPIO_RF_ENABLED)) {
		device_printf(sc->sc_dev,
		    "radio is disabled by hardware switch\n");
		return;
	}

	error = iwn_transfer_firmware(sc);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not load firmware, error %d\n", __func__, error);
		return;
	}

	/* firmware has notified us that it is alive.. */
	iwn_post_alive(sc);	/* ..do post alive initialization */

	sc->rawtemp = sc->ucode_info.temp[3].chan20MHz;
	sc->temp = iwn_get_temperature(sc);
	DPRINTF(sc, IWN_DEBUG_RESET, "%s: temperature=%d\n",
	   __func__, sc->temp);

	error = iwn_config(sc);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not configure device, error %d\n",
		    __func__, error);
		return;
	}

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
}

void
iwn_init(void *arg)
{
	struct iwn_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	IWN_LOCK(sc);
	iwn_init_locked(sc);
	IWN_UNLOCK(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		ieee80211_start_all(ic);
}

void
iwn_stop_locked(struct iwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t tmp;
	int i;

	IWN_LOCK_ASSERT(sc);

	IWN_WRITE(sc, IWN_RESET, IWN_NEVO_RESET);

	sc->sc_tx_timer = 0;
	callout_stop(&sc->sc_timer_to);
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/* disable interrupts */
	IWN_WRITE(sc, IWN_MASK, 0);
	IWN_WRITE(sc, IWN_INTR, 0xffffffff);
	IWN_WRITE(sc, IWN_INTR_STATUS, 0xffffffff);

	/* Clear any commands left in the taskq command buffer */
	memset(sc->sc_cmd, 0, sizeof(sc->sc_cmd));

	/* reset all Tx rings */
	for (i = 0; i < IWN_NTXQUEUES; i++)
		iwn_reset_tx_ring(sc, &sc->txq[i]);

	/* reset Rx ring */
	iwn_reset_rx_ring(sc, &sc->rxq);

	iwn_mem_lock(sc);
	iwn_mem_write(sc, IWN_MEM_CLOCK2, 0x200);
	iwn_mem_unlock(sc);

	DELAY(5);
	iwn_stop_master(sc);

	tmp = IWN_READ(sc, IWN_RESET);
	IWN_WRITE(sc, IWN_RESET, tmp | IWN_SW_RESET);
}

void
iwn_stop(struct iwn_softc *sc)
{
	IWN_LOCK(sc);
	iwn_stop_locked(sc);
	IWN_UNLOCK(sc);
}

/*
 * Callback from net80211 to start a scan.
 */
static void
iwn_scan_start(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwn_softc *sc = ifp->if_softc;

	iwn_queue_cmd(sc, IWN_SCAN_START, 0, IWN_QUEUE_NORMAL);
}

/*
 * Callback from net80211 to terminate a scan.
 */
static void
iwn_scan_end(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwn_softc *sc = ifp->if_softc;

	iwn_queue_cmd(sc, IWN_SCAN_STOP, 0, IWN_QUEUE_NORMAL);
}

/*
 * Callback from net80211 to force a channel change.
 */
static void
iwn_set_channel(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct iwn_softc *sc = ifp->if_softc;
	const struct ieee80211_channel *c = ic->ic_curchan;

	if (c != sc->sc_curchan) {
		sc->sc_rxtap.wr_chan_freq = htole16(c->ic_freq);
		sc->sc_rxtap.wr_chan_flags = htole16(c->ic_flags);
		sc->sc_txtap.wt_chan_freq = htole16(c->ic_freq);
		sc->sc_txtap.wt_chan_flags = htole16(c->ic_flags);
		iwn_queue_cmd(sc, IWN_SET_CHAN, 0, IWN_QUEUE_NORMAL);
	}
}

/*
 * Callback from net80211 to start scanning of the current channel.
 */
static void
iwn_scan_curchan(struct ieee80211_scan_state *ss, unsigned long maxdwell)
{
	struct ieee80211vap *vap = ss->ss_vap;
	struct iwn_softc *sc = vap->iv_ic->ic_ifp->if_softc;

	iwn_queue_cmd(sc, IWN_SCAN_CURCHAN, 0, IWN_QUEUE_NORMAL);
}

/*
 * Callback from net80211 to handle the minimum dwell time being met.
 * The intent is to terminate the scan but we just let the firmware
 * notify us when it's finished as we have no safe way to abort it.
 */
static void
iwn_scan_mindwell(struct ieee80211_scan_state *ss)
{
	/* NB: don't try to abort scan; wait for firmware to finish */
}

/*
 * Carry out work in the taskq context.
 */
static void
iwn_ops(void *arg0, int pending)
{
	struct iwn_softc *sc = arg0;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ieee80211vap *vap;
	int cmd, arg, error;
	enum ieee80211_state nstate;

	for (;;) {
		IWN_CMD_LOCK(sc);
		cmd = sc->sc_cmd[sc->sc_cmd_cur];
		if (cmd == 0) {
			/* No more commands to process */
			IWN_CMD_UNLOCK(sc);
			return;
		}
		if ((sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 &&
		    cmd != IWN_RADIO_ENABLE ) {
			IWN_CMD_UNLOCK(sc);
			return;
		}
		arg = sc->sc_cmd_arg[sc->sc_cmd_cur];
		sc->sc_cmd[sc->sc_cmd_cur] = 0;		/* free the slot */
		sc->sc_cmd_cur = (sc->sc_cmd_cur + 1) % IWN_CMD_MAXOPS;
		IWN_CMD_UNLOCK(sc);

		IWN_LOCK(sc);		/* NB: sync debug printfs on smp */
		DPRINTF(sc, IWN_DEBUG_OPS, "%s: %s (cmd 0x%x)\n",
		    __func__, iwn_ops_str(cmd), cmd);

		vap = TAILQ_FIRST(&ic->ic_vaps);	/* XXX */
		switch (cmd) {
		case IWN_SCAN_START:
			/* make the link LED blink while we're scanning */
			iwn_set_led(sc, IWN_LED_LINK, 20, 2);
			break;
		case IWN_SCAN_STOP:
			break;
		case IWN_SCAN_NEXT:
			ieee80211_scan_next(vap);
			break;
		case IWN_SCAN_CURCHAN:
			error = iwn_scan(sc);
			if (error != 0) {
				IWN_UNLOCK(sc);
				ieee80211_cancel_scan(vap);
				IWN_LOCK(sc);
				return;
			}
			break;
		case IWN_SET_CHAN:
			error = iwn_config(sc);
			if (error != 0) {
				DPRINTF(sc, IWN_DEBUG_STATE,
				    "%s: set chan failed, cancel scan\n",
				    __func__);
				IWN_UNLOCK(sc);
				//XXX Handle failed scan correctly
				ieee80211_cancel_scan(vap);
				return;
			}
			break;
		case IWN_AUTH:
		case IWN_RUN:
			if (cmd == IWN_AUTH) {
				error = iwn_auth(sc);
				nstate = IEEE80211_S_AUTH;
			} else {
				error = iwn_run(sc);
				nstate = IEEE80211_S_RUN;
			}
			if (error == 0) {
				IWN_UNLOCK(sc);
				IEEE80211_LOCK(ic);
				IWN_VAP(vap)->iv_newstate(vap, nstate, arg);
				if (vap->iv_newstate_cb != NULL)
					vap->iv_newstate_cb(vap, nstate, arg);
				IEEE80211_UNLOCK(ic);
				IWN_LOCK(sc);
			} else {
				device_printf(sc->sc_dev,
				    "%s: %s state change failed, error %d\n",
				    __func__, ieee80211_state_name[nstate],
				    error);
			}
			break;
		case IWN_REINIT:
			IWN_UNLOCK(sc);
			iwn_init(sc);
			IWN_LOCK(sc);
			ieee80211_notify_radio(ic, 1);
			break;
		case IWN_RADIO_ENABLE:
			KASSERT(sc->fw_fp != NULL,
			    ("Fware Not Loaded, can't load from tq"));
			IWN_UNLOCK(sc);
			iwn_init(sc);
			IWN_LOCK(sc);
			break;
		case IWN_RADIO_DISABLE:
			ieee80211_notify_radio(ic, 0);
			iwn_stop_locked(sc);
			break;
		}
		IWN_UNLOCK(sc);
	}
}

/*
 * Queue a command for execution in the taskq thread.
 * This is needed as the net80211 callbacks do not allow
 * sleeping, since we need to sleep to confirm commands have
 * been processed by the firmware, we must defer execution to 
 * a sleep enabled thread.
 */
static int
iwn_queue_cmd(struct iwn_softc *sc, int cmd, int arg, int clear)
{
	IWN_CMD_LOCK(sc);
	if (clear) {
		sc->sc_cmd[0] = cmd;
		sc->sc_cmd_arg[0] = arg;
		sc->sc_cmd_cur = 0;
		sc->sc_cmd_next = 1;
	} else {
		if (sc->sc_cmd[sc->sc_cmd_next] != 0) {
			IWN_CMD_UNLOCK(sc);
			DPRINTF(sc, IWN_DEBUG_ANY, "%s: command %d dropped\n",
			    __func__, cmd);
			return EBUSY;
		}
		sc->sc_cmd[sc->sc_cmd_next] = cmd;
		sc->sc_cmd_arg[sc->sc_cmd_next] = arg;
		sc->sc_cmd_next = (sc->sc_cmd_next + 1) % IWN_CMD_MAXOPS;
	}
	taskqueue_enqueue(sc->sc_tq, &sc->sc_ops_task);
	IWN_CMD_UNLOCK(sc);
	return 0;
}

static void
iwn_bpfattach(struct iwn_softc *sc)
{
	struct ifnet *ifp = sc->sc_ifp;

        bpfattach(ifp, DLT_IEEE802_11_RADIO,
            sizeof (struct ieee80211_frame) + sizeof (sc->sc_txtap));

        sc->sc_rxtap_len = sizeof sc->sc_rxtap;
        sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
        sc->sc_rxtap.wr_ihdr.it_present = htole32(IWN_RX_RADIOTAP_PRESENT);

        sc->sc_txtap_len = sizeof sc->sc_txtap;
        sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
        sc->sc_txtap.wt_ihdr.it_present = htole32(IWN_TX_RADIOTAP_PRESENT);
}

static void
iwn_sysctlattach(struct iwn_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->sc_dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(sc->sc_dev);

#ifdef IWN_DEBUG
	sc->sc_debug = 0;
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->sc_debug, 0, "control debugging printfs");
#endif
}

#ifdef IWN_DEBUG
static const char *
iwn_ops_str(int cmd)
{
	switch (cmd) {
	case IWN_SCAN_START:	return "SCAN_START";
	case IWN_SCAN_CURCHAN:	return "SCAN_CURCHAN";
	case IWN_SCAN_STOP:	return "SCAN_STOP";
	case IWN_SET_CHAN:	return "SET_CHAN";
	case IWN_AUTH:		return "AUTH";
	case IWN_SCAN_NEXT:	return "SCAN_NEXT";
	case IWN_RUN:		return "RUN";
	case IWN_RADIO_ENABLE:	return "RADIO_ENABLE";
	case IWN_RADIO_DISABLE:	return "RADIO_DISABLE";
	case IWN_REINIT:	return "REINIT";
	}
	return "UNKNOWN COMMAND";
}

static const char *
iwn_intr_str(uint8_t cmd)
{
	switch (cmd) {
	/* Notifications */
	case IWN_UC_READY:		return "UC_READY";
	case IWN_ADD_NODE_DONE:		return "ADD_NODE_DONE";
	case IWN_TX_DONE:		return "TX_DONE";
	case IWN_START_SCAN:		return "START_SCAN";
	case IWN_STOP_SCAN:		return "STOP_SCAN";
	case IWN_RX_STATISTICS:		return "RX_STATS";
	case IWN_BEACON_STATISTICS:	return "BEACON_STATS";
	case IWN_STATE_CHANGED:		return "STATE_CHANGED";
	case IWN_BEACON_MISSED:		return "BEACON_MISSED";
	case IWN_AMPDU_RX_START:	return "AMPDU_RX_START";
	case IWN_AMPDU_RX_DONE:		return "AMPDU_RX_DONE";
	case IWN_RX_DONE:		return "RX_DONE";

	/* Command Notifications */
	case IWN_CMD_CONFIGURE:		return "IWN_CMD_CONFIGURE";
	case IWN_CMD_ASSOCIATE:		return "IWN_CMD_ASSOCIATE";
	case IWN_CMD_EDCA_PARAMS:	return "IWN_CMD_EDCA_PARAMS";
	case IWN_CMD_TSF:		return "IWN_CMD_TSF";
	case IWN_CMD_TX_LINK_QUALITY:	return "IWN_CMD_TX_LINK_QUALITY";
	case IWN_CMD_SET_LED:		return "IWN_CMD_SET_LED";
	case IWN_CMD_SET_POWER_MODE:	return "IWN_CMD_SET_POWER_MODE";
	case IWN_CMD_SCAN:		return "IWN_CMD_SCAN";
	case IWN_CMD_TXPOWER:		return "IWN_CMD_TXPOWER";
	case IWN_CMD_BLUETOOTH:		return "IWN_CMD_BLUETOOTH";
	case IWN_CMD_SET_CRITICAL_TEMP:	return "IWN_CMD_SET_CRITICAL_TEMP";
	case IWN_SENSITIVITY:		return "IWN_SENSITIVITY";
	case IWN_PHY_CALIB:		return "IWN_PHY_CALIB";
	}
	return "UNKNOWN INTR NOTIF/CMD";
}
#endif /* IWN_DEBUG */

static device_method_t iwn_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,         iwn_probe),
        DEVMETHOD(device_attach,        iwn_attach),
        DEVMETHOD(device_detach,        iwn_detach),
        DEVMETHOD(device_shutdown,      iwn_shutdown),
        DEVMETHOD(device_suspend,       iwn_suspend),
        DEVMETHOD(device_resume,        iwn_resume),

        { 0, 0 }
};

static driver_t iwn_driver = {
        "iwn",
        iwn_methods,
        sizeof (struct iwn_softc)
};
static devclass_t iwn_devclass;
DRIVER_MODULE(iwn, pci, iwn_driver, iwn_devclass, 0, 0);
MODULE_DEPEND(iwn, pci, 1, 1, 1);
MODULE_DEPEND(iwn, firmware, 1, 1, 1);
MODULE_DEPEND(iwn, wlan, 1, 1, 1);
MODULE_DEPEND(iwn, wlan_amrr, 1, 1, 1);
