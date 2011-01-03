/*-
 * Copyright (c) 2009-2010 Weongyo Jeong <weongyo@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * The Broadcom Wireless LAN controller driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/firmware.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/siba/siba_ids.h>
#include <dev/siba/sibareg.h>
#include <dev/siba/sibavar.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_phy.h>
#include <net80211/ieee80211_ratectl.h>

#include <dev/bwn/if_bwnreg.h>
#include <dev/bwn/if_bwnvar.h>

SYSCTL_NODE(_hw, OID_AUTO, bwn, CTLFLAG_RD, 0, "Broadcom driver parameters");

/*
 * Tunable & sysctl variables.
 */

#ifdef BWN_DEBUG
static	int bwn_debug = 0;
SYSCTL_INT(_hw_bwn, OID_AUTO, debug, CTLFLAG_RW, &bwn_debug, 0,
    "Broadcom debugging printfs");
TUNABLE_INT("hw.bwn.debug", &bwn_debug);
enum {
	BWN_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	BWN_DEBUG_RECV		= 0x00000002,	/* basic recv operation */
	BWN_DEBUG_STATE		= 0x00000004,	/* 802.11 state transitions */
	BWN_DEBUG_TXPOW		= 0x00000008,	/* tx power processing */
	BWN_DEBUG_RESET		= 0x00000010,	/* reset processing */
	BWN_DEBUG_OPS		= 0x00000020,	/* bwn_ops processing */
	BWN_DEBUG_BEACON	= 0x00000040,	/* beacon handling */
	BWN_DEBUG_WATCHDOG	= 0x00000080,	/* watchdog timeout */
	BWN_DEBUG_INTR		= 0x00000100,	/* ISR */
	BWN_DEBUG_CALIBRATE	= 0x00000200,	/* periodic calibration */
	BWN_DEBUG_NODE		= 0x00000400,	/* node management */
	BWN_DEBUG_LED		= 0x00000800,	/* led management */
	BWN_DEBUG_CMD		= 0x00001000,	/* cmd submission */
	BWN_DEBUG_LO		= 0x00002000,	/* LO */
	BWN_DEBUG_FW		= 0x00004000,	/* firmware */
	BWN_DEBUG_WME		= 0x00008000,	/* WME */
	BWN_DEBUG_RF		= 0x00010000,	/* RF */
	BWN_DEBUG_FATAL		= 0x80000000,	/* fatal errors */
	BWN_DEBUG_ANY		= 0xffffffff
};
#define	DPRINTF(sc, m, fmt, ...) do {			\
	if (sc->sc_debug & (m))				\
		printf(fmt, __VA_ARGS__);		\
} while (0)
#else
#define	DPRINTF(sc, m, fmt, ...) do { (void) sc; } while (0)
#endif

static int	bwn_bfp = 0;		/* use "Bad Frames Preemption" */
SYSCTL_INT(_hw_bwn, OID_AUTO, bfp, CTLFLAG_RW, &bwn_bfp, 0,
    "uses Bad Frames Preemption");
static int	bwn_bluetooth = 1;
SYSCTL_INT(_hw_bwn, OID_AUTO, bluetooth, CTLFLAG_RW, &bwn_bluetooth, 0,
    "turns on Bluetooth Coexistence");
static int	bwn_hwpctl = 0;
SYSCTL_INT(_hw_bwn, OID_AUTO, hwpctl, CTLFLAG_RW, &bwn_hwpctl, 0,
    "uses H/W power control");
static int	bwn_msi_disable = 0;		/* MSI disabled  */
TUNABLE_INT("hw.bwn.msi_disable", &bwn_msi_disable);
static int	bwn_usedma = 1;
SYSCTL_INT(_hw_bwn, OID_AUTO, usedma, CTLFLAG_RD, &bwn_usedma, 0,
    "uses DMA");
TUNABLE_INT("hw.bwn.usedma", &bwn_usedma);
static int	bwn_wme = 1;
SYSCTL_INT(_hw_bwn, OID_AUTO, wme, CTLFLAG_RW, &bwn_wme, 0,
    "uses WME support");

static int	bwn_attach_pre(struct bwn_softc *);
static int	bwn_attach_post(struct bwn_softc *);
static void	bwn_sprom_bugfixes(device_t);
static void	bwn_init(void *);
static int	bwn_init_locked(struct bwn_softc *);
static int	bwn_ioctl(struct ifnet *, u_long, caddr_t);
static void	bwn_start(struct ifnet *);
static int	bwn_attach_core(struct bwn_mac *);
static void	bwn_reset_core(struct bwn_mac *, uint32_t);
static int	bwn_phy_getinfo(struct bwn_mac *, int);
static int	bwn_chiptest(struct bwn_mac *);
static int	bwn_setup_channels(struct bwn_mac *, int, int);
static int	bwn_phy_g_attach(struct bwn_mac *);
static void	bwn_phy_g_detach(struct bwn_mac *);
static void	bwn_phy_g_init_pre(struct bwn_mac *);
static int	bwn_phy_g_prepare_hw(struct bwn_mac *);
static int	bwn_phy_g_init(struct bwn_mac *);
static void	bwn_phy_g_exit(struct bwn_mac *);
static uint16_t	bwn_phy_g_read(struct bwn_mac *, uint16_t);
static void	bwn_phy_g_write(struct bwn_mac *, uint16_t,
		    uint16_t);
static uint16_t	bwn_phy_g_rf_read(struct bwn_mac *, uint16_t);
static void	bwn_phy_g_rf_write(struct bwn_mac *, uint16_t,
		    uint16_t);
static int	bwn_phy_g_hwpctl(struct bwn_mac *);
static void	bwn_phy_g_rf_onoff(struct bwn_mac *, int);
static int	bwn_phy_g_switch_channel(struct bwn_mac *, uint32_t);
static uint32_t	bwn_phy_g_get_default_chan(struct bwn_mac *);
static void	bwn_phy_g_set_antenna(struct bwn_mac *, int);
static int	bwn_phy_g_im(struct bwn_mac *, int);
static int	bwn_phy_g_recalc_txpwr(struct bwn_mac *, int);
static void	bwn_phy_g_set_txpwr(struct bwn_mac *);
static void	bwn_phy_g_task_15s(struct bwn_mac *);
static void	bwn_phy_g_task_60s(struct bwn_mac *);
static uint16_t	bwn_phy_g_txctl(struct bwn_mac *);
static void	bwn_phy_switch_analog(struct bwn_mac *, int);
static uint16_t	bwn_shm_read_2(struct bwn_mac *, uint16_t, uint16_t);
static void	bwn_shm_write_2(struct bwn_mac *, uint16_t, uint16_t,
		    uint16_t);
static uint32_t	bwn_shm_read_4(struct bwn_mac *, uint16_t, uint16_t);
static void	bwn_shm_write_4(struct bwn_mac *, uint16_t, uint16_t,
		    uint32_t);
static void	bwn_shm_ctlword(struct bwn_mac *, uint16_t,
		    uint16_t);
static void	bwn_addchannels(struct ieee80211_channel [], int, int *,
		    const struct bwn_channelinfo *, int);
static int	bwn_raw_xmit(struct ieee80211_node *, struct mbuf *,
		    const struct ieee80211_bpf_params *);
static void	bwn_updateslot(struct ifnet *);
static void	bwn_update_promisc(struct ifnet *);
static void	bwn_wme_init(struct bwn_mac *);
static int	bwn_wme_update(struct ieee80211com *);
static void	bwn_wme_clear(struct bwn_softc *);
static void	bwn_wme_load(struct bwn_mac *);
static void	bwn_wme_loadparams(struct bwn_mac *,
		    const struct wmeParams *, uint16_t);
static void	bwn_scan_start(struct ieee80211com *);
static void	bwn_scan_end(struct ieee80211com *);
static void	bwn_set_channel(struct ieee80211com *);
static struct ieee80211vap *bwn_vap_create(struct ieee80211com *,
		    const char [IFNAMSIZ], int, int,
		    int, const uint8_t [IEEE80211_ADDR_LEN],
		    const uint8_t [IEEE80211_ADDR_LEN]);
static void	bwn_vap_delete(struct ieee80211vap *);
static void	bwn_stop(struct bwn_softc *, int);
static void	bwn_stop_locked(struct bwn_softc *, int);
static int	bwn_core_init(struct bwn_mac *);
static void	bwn_core_start(struct bwn_mac *);
static void	bwn_core_exit(struct bwn_mac *);
static void	bwn_bt_disable(struct bwn_mac *);
static int	bwn_chip_init(struct bwn_mac *);
static uint64_t	bwn_hf_read(struct bwn_mac *);
static void	bwn_hf_write(struct bwn_mac *, uint64_t);
static void	bwn_set_txretry(struct bwn_mac *, int, int);
static void	bwn_rate_init(struct bwn_mac *);
static void	bwn_set_phytxctl(struct bwn_mac *);
static void	bwn_spu_setdelay(struct bwn_mac *, int);
static void	bwn_bt_enable(struct bwn_mac *);
static void	bwn_set_macaddr(struct bwn_mac *);
static void	bwn_crypt_init(struct bwn_mac *);
static void	bwn_chip_exit(struct bwn_mac *);
static int	bwn_fw_fillinfo(struct bwn_mac *);
static int	bwn_fw_loaducode(struct bwn_mac *);
static int	bwn_gpio_init(struct bwn_mac *);
static int	bwn_fw_loadinitvals(struct bwn_mac *);
static int	bwn_phy_init(struct bwn_mac *);
static void	bwn_set_txantenna(struct bwn_mac *, int);
static void	bwn_set_opmode(struct bwn_mac *);
static void	bwn_rate_write(struct bwn_mac *, uint16_t, int);
static uint8_t	bwn_plcp_getcck(const uint8_t);
static uint8_t	bwn_plcp_getofdm(const uint8_t);
static void	bwn_pio_init(struct bwn_mac *);
static uint16_t	bwn_pio_idx2base(struct bwn_mac *, int);
static void	bwn_pio_set_txqueue(struct bwn_mac *, struct bwn_pio_txqueue *,
		    int);
static void	bwn_pio_setupqueue_rx(struct bwn_mac *,
		    struct bwn_pio_rxqueue *, int);
static void	bwn_destroy_queue_tx(struct bwn_pio_txqueue *);
static uint16_t	bwn_pio_read_2(struct bwn_mac *, struct bwn_pio_txqueue *,
		    uint16_t);
static void	bwn_pio_cancel_tx_packets(struct bwn_pio_txqueue *);
static int	bwn_pio_rx(struct bwn_pio_rxqueue *);
static uint8_t	bwn_pio_rxeof(struct bwn_pio_rxqueue *);
static void	bwn_pio_handle_txeof(struct bwn_mac *,
		    const struct bwn_txstatus *);
static uint16_t	bwn_pio_rx_read_2(struct bwn_pio_rxqueue *, uint16_t);
static uint32_t	bwn_pio_rx_read_4(struct bwn_pio_rxqueue *, uint16_t);
static void	bwn_pio_rx_write_2(struct bwn_pio_rxqueue *, uint16_t,
		    uint16_t);
static void	bwn_pio_rx_write_4(struct bwn_pio_rxqueue *, uint16_t,
		    uint32_t);
static int	bwn_pio_tx_start(struct bwn_mac *, struct ieee80211_node *,
		    struct mbuf *);
static struct bwn_pio_txqueue *bwn_pio_select(struct bwn_mac *, uint8_t);
static uint32_t	bwn_pio_write_multi_4(struct bwn_mac *,
		    struct bwn_pio_txqueue *, uint32_t, const void *, int);
static void	bwn_pio_write_4(struct bwn_mac *, struct bwn_pio_txqueue *,
		    uint16_t, uint32_t);
static uint16_t	bwn_pio_write_multi_2(struct bwn_mac *,
		    struct bwn_pio_txqueue *, uint16_t, const void *, int);
static uint16_t	bwn_pio_write_mbuf_2(struct bwn_mac *,
		    struct bwn_pio_txqueue *, uint16_t, struct mbuf *);
static struct bwn_pio_txqueue *bwn_pio_parse_cookie(struct bwn_mac *,
		    uint16_t, struct bwn_pio_txpkt **);
static void	bwn_dma_init(struct bwn_mac *);
static void	bwn_dma_rxdirectfifo(struct bwn_mac *, int, uint8_t);
static int	bwn_dma_mask2type(uint64_t);
static uint64_t	bwn_dma_mask(struct bwn_mac *);
static uint16_t	bwn_dma_base(int, int);
static void	bwn_dma_ringfree(struct bwn_dma_ring **);
static void	bwn_dma_32_getdesc(struct bwn_dma_ring *,
		    int, struct bwn_dmadesc_generic **,
		    struct bwn_dmadesc_meta **);
static void	bwn_dma_32_setdesc(struct bwn_dma_ring *,
		    struct bwn_dmadesc_generic *, bus_addr_t, uint16_t, int,
		    int, int);
static void	bwn_dma_32_start_transfer(struct bwn_dma_ring *, int);
static void	bwn_dma_32_suspend(struct bwn_dma_ring *);
static void	bwn_dma_32_resume(struct bwn_dma_ring *);
static int	bwn_dma_32_get_curslot(struct bwn_dma_ring *);
static void	bwn_dma_32_set_curslot(struct bwn_dma_ring *, int);
static void	bwn_dma_64_getdesc(struct bwn_dma_ring *,
		    int, struct bwn_dmadesc_generic **,
		    struct bwn_dmadesc_meta **);
static void	bwn_dma_64_setdesc(struct bwn_dma_ring *,
		    struct bwn_dmadesc_generic *, bus_addr_t, uint16_t, int,
		    int, int);
static void	bwn_dma_64_start_transfer(struct bwn_dma_ring *, int);
static void	bwn_dma_64_suspend(struct bwn_dma_ring *);
static void	bwn_dma_64_resume(struct bwn_dma_ring *);
static int	bwn_dma_64_get_curslot(struct bwn_dma_ring *);
static void	bwn_dma_64_set_curslot(struct bwn_dma_ring *, int);
static int	bwn_dma_allocringmemory(struct bwn_dma_ring *);
static void	bwn_dma_setup(struct bwn_dma_ring *);
static void	bwn_dma_free_ringmemory(struct bwn_dma_ring *);
static void	bwn_dma_cleanup(struct bwn_dma_ring *);
static void	bwn_dma_free_descbufs(struct bwn_dma_ring *);
static int	bwn_dma_tx_reset(struct bwn_mac *, uint16_t, int);
static void	bwn_dma_rx(struct bwn_dma_ring *);
static int	bwn_dma_rx_reset(struct bwn_mac *, uint16_t, int);
static void	bwn_dma_free_descbuf(struct bwn_dma_ring *,
		    struct bwn_dmadesc_meta *);
static void	bwn_dma_set_redzone(struct bwn_dma_ring *, struct mbuf *);
static int	bwn_dma_gettype(struct bwn_mac *);
static void	bwn_dma_ring_addr(void *, bus_dma_segment_t *, int, int);
static int	bwn_dma_freeslot(struct bwn_dma_ring *);
static int	bwn_dma_nextslot(struct bwn_dma_ring *, int);
static void	bwn_dma_rxeof(struct bwn_dma_ring *, int *);
static int	bwn_dma_newbuf(struct bwn_dma_ring *,
		    struct bwn_dmadesc_generic *, struct bwn_dmadesc_meta *,
		    int);
static void	bwn_dma_buf_addr(void *, bus_dma_segment_t *, int,
		    bus_size_t, int);
static uint8_t	bwn_dma_check_redzone(struct bwn_dma_ring *, struct mbuf *);
static void	bwn_dma_handle_txeof(struct bwn_mac *,
		    const struct bwn_txstatus *);
static int	bwn_dma_tx_start(struct bwn_mac *, struct ieee80211_node *,
		    struct mbuf *);
static int	bwn_dma_getslot(struct bwn_dma_ring *);
static struct bwn_dma_ring *bwn_dma_select(struct bwn_mac *,
		    uint8_t);
static int	bwn_dma_attach(struct bwn_mac *);
static struct bwn_dma_ring *bwn_dma_ringsetup(struct bwn_mac *,
		    int, int, int);
static struct bwn_dma_ring *bwn_dma_parse_cookie(struct bwn_mac *,
		    const struct bwn_txstatus *, uint16_t, int *);
static void	bwn_dma_free(struct bwn_mac *);
static void	bwn_phy_g_init_sub(struct bwn_mac *);
static uint8_t	bwn_has_hwpctl(struct bwn_mac *);
static void	bwn_phy_init_b5(struct bwn_mac *);
static void	bwn_phy_init_b6(struct bwn_mac *);
static void	bwn_phy_init_a(struct bwn_mac *);
static void	bwn_loopback_calcgain(struct bwn_mac *);
static uint16_t	bwn_rf_init_bcm2050(struct bwn_mac *);
static void	bwn_lo_g_init(struct bwn_mac *);
static void	bwn_lo_g_adjust(struct bwn_mac *);
static void	bwn_lo_get_powervector(struct bwn_mac *);
static struct bwn_lo_calib *bwn_lo_calibset(struct bwn_mac *,
		    const struct bwn_bbatt *, const struct bwn_rfatt *);
static void	bwn_lo_write(struct bwn_mac *, struct bwn_loctl *);
static void	bwn_phy_hwpctl_init(struct bwn_mac *);
static void	bwn_phy_g_switch_chan(struct bwn_mac *, int, uint8_t);
static void	bwn_phy_g_set_txpwr_sub(struct bwn_mac *,
		    const struct bwn_bbatt *, const struct bwn_rfatt *,
		    uint8_t);
static void	bwn_phy_g_set_bbatt(struct bwn_mac *, uint16_t);
static uint16_t	bwn_rf_2050_rfoverval(struct bwn_mac *, uint16_t, uint32_t);
static void	bwn_spu_workaround(struct bwn_mac *, uint8_t);
static void	bwn_wa_init(struct bwn_mac *);
static void	bwn_ofdmtab_write_2(struct bwn_mac *, uint16_t, uint16_t,
		    uint16_t);
static void	bwn_dummy_transmission(struct bwn_mac *, int, int);
static void	bwn_ofdmtab_write_4(struct bwn_mac *, uint16_t, uint16_t,
		    uint32_t);
static void	bwn_gtab_write(struct bwn_mac *, uint16_t, uint16_t,
		    uint16_t);
static void	bwn_ram_write(struct bwn_mac *, uint16_t, uint32_t);
static void	bwn_mac_suspend(struct bwn_mac *);
static void	bwn_mac_enable(struct bwn_mac *);
static void	bwn_psctl(struct bwn_mac *, uint32_t);
static int16_t	bwn_nrssi_read(struct bwn_mac *, uint16_t);
static void	bwn_nrssi_offset(struct bwn_mac *);
static void	bwn_nrssi_threshold(struct bwn_mac *);
static void	bwn_nrssi_slope_11g(struct bwn_mac *);
static void	bwn_set_all_gains(struct bwn_mac *, int16_t, int16_t,
		    int16_t);
static void	bwn_set_original_gains(struct bwn_mac *);
static void	bwn_hwpctl_early_init(struct bwn_mac *);
static void	bwn_hwpctl_init_gphy(struct bwn_mac *);
static uint16_t	bwn_phy_g_chan2freq(uint8_t);
static int	bwn_fw_gets(struct bwn_mac *, enum bwn_fwtype);
static int	bwn_fw_get(struct bwn_mac *, enum bwn_fwtype,
		    const char *, struct bwn_fwfile *);
static void	bwn_release_firmware(struct bwn_mac *);
static void	bwn_do_release_fw(struct bwn_fwfile *);
static uint16_t	bwn_fwcaps_read(struct bwn_mac *);
static int	bwn_fwinitvals_write(struct bwn_mac *,
		    const struct bwn_fwinitvals *, size_t, size_t);
static int	bwn_switch_channel(struct bwn_mac *, int);
static uint16_t	bwn_ant2phy(int);
static void	bwn_mac_write_bssid(struct bwn_mac *);
static void	bwn_mac_setfilter(struct bwn_mac *, uint16_t,
		    const uint8_t *);
static void	bwn_key_dowrite(struct bwn_mac *, uint8_t, uint8_t,
		    const uint8_t *, size_t, const uint8_t *);
static void	bwn_key_macwrite(struct bwn_mac *, uint8_t,
		    const uint8_t *);
static void	bwn_key_write(struct bwn_mac *, uint8_t, uint8_t,
		    const uint8_t *);
static void	bwn_phy_exit(struct bwn_mac *);
static void	bwn_core_stop(struct bwn_mac *);
static int	bwn_switch_band(struct bwn_softc *,
		    struct ieee80211_channel *);
static void	bwn_phy_reset(struct bwn_mac *);
static int	bwn_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static void	bwn_set_pretbtt(struct bwn_mac *);
static int	bwn_intr(void *);
static void	bwn_intrtask(void *, int);
static void	bwn_restart(struct bwn_mac *, const char *);
static void	bwn_intr_ucode_debug(struct bwn_mac *);
static void	bwn_intr_tbtt_indication(struct bwn_mac *);
static void	bwn_intr_atim_end(struct bwn_mac *);
static void	bwn_intr_beacon(struct bwn_mac *);
static void	bwn_intr_pmq(struct bwn_mac *);
static void	bwn_intr_noise(struct bwn_mac *);
static void	bwn_intr_txeof(struct bwn_mac *);
static void	bwn_hwreset(void *, int);
static void	bwn_handle_fwpanic(struct bwn_mac *);
static void	bwn_load_beacon0(struct bwn_mac *);
static void	bwn_load_beacon1(struct bwn_mac *);
static uint32_t	bwn_jssi_read(struct bwn_mac *);
static void	bwn_noise_gensample(struct bwn_mac *);
static void	bwn_handle_txeof(struct bwn_mac *,
		    const struct bwn_txstatus *);
static void	bwn_rxeof(struct bwn_mac *, struct mbuf *, const void *);
static void	bwn_phy_txpower_check(struct bwn_mac *, uint32_t);
static void	bwn_start_locked(struct ifnet *);
static int	bwn_tx_start(struct bwn_softc *, struct ieee80211_node *,
		    struct mbuf *);
static int	bwn_tx_isfull(struct bwn_softc *, struct mbuf *);
static int	bwn_set_txhdr(struct bwn_mac *,
		    struct ieee80211_node *, struct mbuf *, struct bwn_txhdr *,
		    uint16_t);
static void	bwn_plcp_genhdr(struct bwn_plcp4 *, const uint16_t,
		    const uint8_t);
static uint8_t	bwn_antenna_sanitize(struct bwn_mac *, uint8_t);
static uint8_t	bwn_get_fbrate(uint8_t);
static int	bwn_phy_shm_tssi_read(struct bwn_mac *, uint16_t);
static void	bwn_phy_g_setatt(struct bwn_mac *, int *, int *);
static void	bwn_phy_lock(struct bwn_mac *);
static void	bwn_phy_unlock(struct bwn_mac *);
static void	bwn_rf_lock(struct bwn_mac *);
static void	bwn_rf_unlock(struct bwn_mac *);
static void	bwn_txpwr(void *, int);
static void	bwn_tasks(void *);
static void	bwn_task_15s(struct bwn_mac *);
static void	bwn_task_30s(struct bwn_mac *);
static void	bwn_task_60s(struct bwn_mac *);
static int	bwn_plcp_get_ofdmrate(struct bwn_mac *, struct bwn_plcp6 *,
		    uint8_t);
static int	bwn_plcp_get_cckrate(struct bwn_mac *, struct bwn_plcp6 *);
static void	bwn_rx_radiotap(struct bwn_mac *, struct mbuf *,
		    const struct bwn_rxhdr4 *, struct bwn_plcp6 *, int,
		    int, int);
static void	bwn_tsf_read(struct bwn_mac *, uint64_t *);
static void	bwn_phy_g_dc_lookup_init(struct bwn_mac *, uint8_t);
static void	bwn_set_slot_time(struct bwn_mac *, uint16_t);
static void	bwn_watchdog(void *);
static void	bwn_dma_stop(struct bwn_mac *);
static void	bwn_pio_stop(struct bwn_mac *);
static void	bwn_dma_ringstop(struct bwn_dma_ring **);
static void	bwn_led_attach(struct bwn_mac *);
static void	bwn_led_newstate(struct bwn_mac *, enum ieee80211_state);
static void	bwn_led_event(struct bwn_mac *, int);
static void	bwn_led_blink_start(struct bwn_mac *, int, int);
static void	bwn_led_blink_next(void *);
static void	bwn_led_blink_end(void *);
static void	bwn_rfswitch(void *);
static void	bwn_rf_turnon(struct bwn_mac *);
static void	bwn_rf_turnoff(struct bwn_mac *);
static void	bwn_phy_lp_init_pre(struct bwn_mac *);
static int	bwn_phy_lp_init(struct bwn_mac *);
static uint16_t	bwn_phy_lp_read(struct bwn_mac *, uint16_t);
static void	bwn_phy_lp_write(struct bwn_mac *, uint16_t, uint16_t);
static void	bwn_phy_lp_maskset(struct bwn_mac *, uint16_t, uint16_t,
		    uint16_t);
static uint16_t	bwn_phy_lp_rf_read(struct bwn_mac *, uint16_t);
static void	bwn_phy_lp_rf_write(struct bwn_mac *, uint16_t, uint16_t);
static void	bwn_phy_lp_rf_onoff(struct bwn_mac *, int);
static int	bwn_phy_lp_switch_channel(struct bwn_mac *, uint32_t);
static uint32_t	bwn_phy_lp_get_default_chan(struct bwn_mac *);
static void	bwn_phy_lp_set_antenna(struct bwn_mac *, int);
static void	bwn_phy_lp_task_60s(struct bwn_mac *);
static void	bwn_phy_lp_readsprom(struct bwn_mac *);
static void	bwn_phy_lp_bbinit(struct bwn_mac *);
static void	bwn_phy_lp_txpctl_init(struct bwn_mac *);
static void	bwn_phy_lp_calib(struct bwn_mac *);
static void	bwn_phy_lp_switch_analog(struct bwn_mac *, int);
static int	bwn_phy_lp_b2062_switch_channel(struct bwn_mac *, uint8_t);
static int	bwn_phy_lp_b2063_switch_channel(struct bwn_mac *, uint8_t);
static void	bwn_phy_lp_set_anafilter(struct bwn_mac *, uint8_t);
static void	bwn_phy_lp_set_gaintbl(struct bwn_mac *, uint32_t);
static void	bwn_phy_lp_digflt_save(struct bwn_mac *);
static void	bwn_phy_lp_get_txpctlmode(struct bwn_mac *);
static void	bwn_phy_lp_set_txpctlmode(struct bwn_mac *, uint8_t);
static void	bwn_phy_lp_bugfix(struct bwn_mac *);
static void	bwn_phy_lp_digflt_restore(struct bwn_mac *);
static void	bwn_phy_lp_tblinit(struct bwn_mac *);
static void	bwn_phy_lp_bbinit_r2(struct bwn_mac *);
static void	bwn_phy_lp_bbinit_r01(struct bwn_mac *);
static void	bwn_phy_lp_b2062_init(struct bwn_mac *);
static void	bwn_phy_lp_b2063_init(struct bwn_mac *);
static void	bwn_phy_lp_rxcal_r2(struct bwn_mac *);
static void	bwn_phy_lp_rccal_r12(struct bwn_mac *);
static void	bwn_phy_lp_set_rccap(struct bwn_mac *);
static uint32_t	bwn_phy_lp_roundup(uint32_t, uint32_t, uint8_t);
static void	bwn_phy_lp_b2062_reset_pllbias(struct bwn_mac *);
static void	bwn_phy_lp_b2062_vco_calib(struct bwn_mac *);
static void	bwn_tab_write_multi(struct bwn_mac *, uint32_t, int,
		    const void *);
static void	bwn_tab_read_multi(struct bwn_mac *, uint32_t, int, void *);
static struct bwn_txgain
		bwn_phy_lp_get_txgain(struct bwn_mac *);
static uint8_t	bwn_phy_lp_get_bbmult(struct bwn_mac *);
static void	bwn_phy_lp_set_txgain(struct bwn_mac *, struct bwn_txgain *);
static void	bwn_phy_lp_set_bbmult(struct bwn_mac *, uint8_t);
static void	bwn_phy_lp_set_trsw_over(struct bwn_mac *, uint8_t, uint8_t);
static void	bwn_phy_lp_set_rxgain(struct bwn_mac *, uint32_t);
static void	bwn_phy_lp_set_deaf(struct bwn_mac *, uint8_t);
static int	bwn_phy_lp_calc_rx_iq_comp(struct bwn_mac *, uint16_t);
static void	bwn_phy_lp_clear_deaf(struct bwn_mac *, uint8_t);
static void	bwn_phy_lp_tblinit_r01(struct bwn_mac *);
static void	bwn_phy_lp_tblinit_r2(struct bwn_mac *);
static void	bwn_phy_lp_tblinit_txgain(struct bwn_mac *);
static void	bwn_tab_write(struct bwn_mac *, uint32_t, uint32_t);
static void	bwn_phy_lp_b2062_tblinit(struct bwn_mac *);
static void	bwn_phy_lp_b2063_tblinit(struct bwn_mac *);
static int	bwn_phy_lp_loopback(struct bwn_mac *);
static void	bwn_phy_lp_set_rxgain_idx(struct bwn_mac *, uint16_t);
static void	bwn_phy_lp_ddfs_turnon(struct bwn_mac *, int, int, int, int,
		    int);
static uint8_t	bwn_phy_lp_rx_iq_est(struct bwn_mac *, uint16_t, uint8_t,
		    struct bwn_phy_lp_iq_est *);
static void	bwn_phy_lp_ddfs_turnoff(struct bwn_mac *);
static uint32_t	bwn_tab_read(struct bwn_mac *, uint32_t);
static void	bwn_phy_lp_set_txgain_dac(struct bwn_mac *, uint16_t);
static void	bwn_phy_lp_set_txgain_pa(struct bwn_mac *, uint16_t);
static void	bwn_phy_lp_set_txgain_override(struct bwn_mac *);
static uint16_t	bwn_phy_lp_get_pa_gain(struct bwn_mac *);
static uint8_t	bwn_nbits(int32_t);
static void	bwn_phy_lp_gaintbl_write_multi(struct bwn_mac *, int, int,
		    struct bwn_txgain_entry *);
static void	bwn_phy_lp_gaintbl_write(struct bwn_mac *, int,
		    struct bwn_txgain_entry);
static void	bwn_phy_lp_gaintbl_write_r2(struct bwn_mac *, int,
		    struct bwn_txgain_entry);
static void	bwn_phy_lp_gaintbl_write_r01(struct bwn_mac *, int,
		    struct bwn_txgain_entry);
static void	bwn_sysctl_node(struct bwn_softc *);

static struct resource_spec bwn_res_spec_legacy[] = {
	{ SYS_RES_IRQ,		0,		RF_ACTIVE | RF_SHAREABLE },
	{ -1,			0,		0 }
};

static struct resource_spec bwn_res_spec_msi[] = {
	{ SYS_RES_IRQ,		1,		RF_ACTIVE },
	{ -1,			0,		0 }
};

static const struct bwn_channelinfo bwn_chantable_bg = {
	.channels = {
		{ 2412,  1, 30 }, { 2417,  2, 30 }, { 2422,  3, 30 },
		{ 2427,  4, 30 }, { 2432,  5, 30 }, { 2437,  6, 30 },
		{ 2442,  7, 30 }, { 2447,  8, 30 }, { 2452,  9, 30 },
		{ 2457, 10, 30 }, { 2462, 11, 30 }, { 2467, 12, 30 },
		{ 2472, 13, 30 }, { 2484, 14, 30 } },
	.nchannels = 14
};

static const struct bwn_channelinfo bwn_chantable_a = {
	.channels = {
		{ 5170,  34, 30 }, { 5180,  36, 30 }, { 5190,  38, 30 },
		{ 5200,  40, 30 }, { 5210,  42, 30 }, { 5220,  44, 30 },
		{ 5230,  46, 30 }, { 5240,  48, 30 }, { 5260,  52, 30 },
		{ 5280,  56, 30 }, { 5300,  60, 30 }, { 5320,  64, 30 },
		{ 5500, 100, 30 }, { 5520, 104, 30 }, { 5540, 108, 30 },
		{ 5560, 112, 30 }, { 5580, 116, 30 }, { 5600, 120, 30 },
		{ 5620, 124, 30 }, { 5640, 128, 30 }, { 5660, 132, 30 },
		{ 5680, 136, 30 }, { 5700, 140, 30 }, { 5745, 149, 30 },
		{ 5765, 153, 30 }, { 5785, 157, 30 }, { 5805, 161, 30 },
		{ 5825, 165, 30 }, { 5920, 184, 30 }, { 5940, 188, 30 },
		{ 5960, 192, 30 }, { 5980, 196, 30 }, { 6000, 200, 30 },
		{ 6020, 204, 30 }, { 6040, 208, 30 }, { 6060, 212, 30 },
		{ 6080, 216, 30 } },
	.nchannels = 37
};

static const struct bwn_channelinfo bwn_chantable_n = {
	.channels = {
		{ 5160,  32, 30 }, { 5170,  34, 30 }, { 5180,  36, 30 },
		{ 5190,  38, 30 }, { 5200,  40, 30 }, { 5210,  42, 30 },
		{ 5220,  44, 30 }, { 5230,  46, 30 }, { 5240,  48, 30 },
		{ 5250,  50, 30 }, { 5260,  52, 30 }, { 5270,  54, 30 },
		{ 5280,  56, 30 }, { 5290,  58, 30 }, { 5300,  60, 30 },
		{ 5310,  62, 30 }, { 5320,  64, 30 }, { 5330,  66, 30 },
		{ 5340,  68, 30 }, { 5350,  70, 30 }, { 5360,  72, 30 },
		{ 5370,  74, 30 }, { 5380,  76, 30 }, { 5390,  78, 30 },
		{ 5400,  80, 30 }, { 5410,  82, 30 }, { 5420,  84, 30 },
		{ 5430,  86, 30 }, { 5440,  88, 30 }, { 5450,  90, 30 },
		{ 5460,  92, 30 }, { 5470,  94, 30 }, { 5480,  96, 30 },
		{ 5490,  98, 30 }, { 5500, 100, 30 }, { 5510, 102, 30 },
		{ 5520, 104, 30 }, { 5530, 106, 30 }, { 5540, 108, 30 },
		{ 5550, 110, 30 }, { 5560, 112, 30 }, { 5570, 114, 30 },
		{ 5580, 116, 30 }, { 5590, 118, 30 }, { 5600, 120, 30 },
		{ 5610, 122, 30 }, { 5620, 124, 30 }, { 5630, 126, 30 },
		{ 5640, 128, 30 }, { 5650, 130, 30 }, { 5660, 132, 30 },
		{ 5670, 134, 30 }, { 5680, 136, 30 }, { 5690, 138, 30 },
		{ 5700, 140, 30 }, { 5710, 142, 30 }, { 5720, 144, 30 },
		{ 5725, 145, 30 }, { 5730, 146, 30 }, { 5735, 147, 30 },
		{ 5740, 148, 30 }, { 5745, 149, 30 }, { 5750, 150, 30 },
		{ 5755, 151, 30 }, { 5760, 152, 30 }, { 5765, 153, 30 },
		{ 5770, 154, 30 }, { 5775, 155, 30 }, { 5780, 156, 30 },
		{ 5785, 157, 30 }, { 5790, 158, 30 }, { 5795, 159, 30 },
		{ 5800, 160, 30 }, { 5805, 161, 30 }, { 5810, 162, 30 },
		{ 5815, 163, 30 }, { 5820, 164, 30 }, { 5825, 165, 30 },
		{ 5830, 166, 30 }, { 5840, 168, 30 }, { 5850, 170, 30 },
		{ 5860, 172, 30 }, { 5870, 174, 30 }, { 5880, 176, 30 },
		{ 5890, 178, 30 }, { 5900, 180, 30 }, { 5910, 182, 30 },
		{ 5920, 184, 30 }, { 5930, 186, 30 }, { 5940, 188, 30 },
		{ 5950, 190, 30 }, { 5960, 192, 30 }, { 5970, 194, 30 },
		{ 5980, 196, 30 }, { 5990, 198, 30 }, { 6000, 200, 30 },
		{ 6010, 202, 30 }, { 6020, 204, 30 }, { 6030, 206, 30 },
		{ 6040, 208, 30 }, { 6050, 210, 30 }, { 6060, 212, 30 },
		{ 6070, 214, 30 }, { 6080, 216, 30 }, { 6090, 218, 30 },
		{ 6100, 220, 30 }, { 6110, 222, 30 }, { 6120, 224, 30 },
		{ 6130, 226, 30 }, { 6140, 228, 30 } },
	.nchannels = 110
};

static const uint8_t bwn_b2063_chantable_data[33][12] = {
	{ 0x6f, 0x3c, 0x3c, 0x4, 0x5, 0x5, 0x5, 0x5, 0x77, 0x80, 0x80, 0x70 },
	{ 0x6f, 0x2c, 0x2c, 0x4, 0x5, 0x5, 0x5, 0x5, 0x77, 0x80, 0x80, 0x70 },
	{ 0x6f, 0x1c, 0x1c, 0x4, 0x5, 0x5, 0x5, 0x5, 0x77, 0x80, 0x80, 0x70 },
	{ 0x6e, 0x1c, 0x1c, 0x4, 0x5, 0x5, 0x5, 0x5, 0x77, 0x80, 0x80, 0x70 },
	{ 0x6e, 0xc, 0xc, 0x4, 0x5, 0x5, 0x5, 0x5, 0x77, 0x80, 0x80, 0x70 },
	{ 0x6a, 0xc, 0xc, 0, 0x2, 0x5, 0xd, 0xd, 0x77, 0x80, 0x20, 0 },
	{ 0x6a, 0xc, 0xc, 0, 0x1, 0x5, 0xd, 0xc, 0x77, 0x80, 0x20, 0 },
	{ 0x6a, 0xc, 0xc, 0, 0x1, 0x4, 0xc, 0xc, 0x77, 0x80, 0x20, 0 },
	{ 0x69, 0xc, 0xc, 0, 0x1, 0x4, 0xc, 0xc, 0x77, 0x70, 0x20, 0 },
	{ 0x69, 0xc, 0xc, 0, 0x1, 0x4, 0xb, 0xc, 0x77, 0x70, 0x20, 0 },
	{ 0x69, 0xc, 0xc, 0, 0, 0x4, 0xb, 0xb, 0x77, 0x60, 0x20, 0 },
	{ 0x69, 0xc, 0xc, 0, 0, 0x3, 0xa, 0xb, 0x77, 0x60, 0x20, 0 },
	{ 0x69, 0xc, 0xc, 0, 0, 0x3, 0xa, 0xa, 0x77, 0x60, 0x20, 0 },
	{ 0x68, 0xc, 0xc, 0, 0, 0x2, 0x9, 0x9, 0x77, 0x60, 0x20, 0 },
	{ 0x68, 0xc, 0xc, 0, 0, 0x1, 0x8, 0x8, 0x77, 0x50, 0x10, 0 },
	{ 0x67, 0xc, 0xc, 0, 0, 0, 0x8, 0x8, 0x77, 0x50, 0x10, 0 },
	{ 0x64, 0xc, 0xc, 0, 0, 0, 0x2, 0x1, 0x77, 0x20, 0, 0 },
	{ 0x64, 0xc, 0xc, 0, 0, 0, 0x1, 0x1, 0x77, 0x20, 0, 0 },
	{ 0x63, 0xc, 0xc, 0, 0, 0, 0x1, 0, 0x77, 0x10, 0, 0 },
	{ 0x63, 0xc, 0xc, 0, 0, 0, 0, 0, 0x77, 0x10, 0, 0 },
	{ 0x62, 0xc, 0xc, 0, 0, 0, 0, 0, 0x77, 0x10, 0, 0 },
	{ 0x62, 0xc, 0xc, 0, 0, 0, 0, 0, 0x77, 0, 0, 0 },
	{ 0x61, 0xc, 0xc, 0, 0, 0, 0, 0, 0x77, 0, 0, 0 },
	{ 0x60, 0xc, 0xc, 0, 0, 0, 0, 0, 0x77, 0, 0, 0 },
	{ 0x6e, 0xc, 0xc, 0, 0x9, 0xe, 0xf, 0xf, 0x77, 0xc0, 0x50, 0 },
	{ 0x6e, 0xc, 0xc, 0, 0x9, 0xd, 0xf, 0xf, 0x77, 0xb0, 0x50, 0 },
	{ 0x6e, 0xc, 0xc, 0, 0x8, 0xc, 0xf, 0xf, 0x77, 0xb0, 0x50, 0 },
	{ 0x6d, 0xc, 0xc, 0, 0x8, 0xc, 0xf, 0xf, 0x77, 0xa0, 0x40, 0 },
	{ 0x6d, 0xc, 0xc, 0, 0x8, 0xb, 0xf, 0xf, 0x77, 0xa0, 0x40, 0 },
	{ 0x6d, 0xc, 0xc, 0, 0x8, 0xa, 0xf, 0xf, 0x77, 0xa0, 0x40, 0 },
	{ 0x6c, 0xc, 0xc, 0, 0x7, 0x9, 0xf, 0xf, 0x77, 0x90, 0x40, 0 },
	{ 0x6c, 0xc, 0xc, 0, 0x6, 0x8, 0xf, 0xf, 0x77, 0x90, 0x40, 0 },
	{ 0x6c, 0xc, 0xc, 0, 0x5, 0x8, 0xf, 0xf, 0x77, 0x90, 0x40, 0 }
};

static const struct bwn_b206x_chan bwn_b2063_chantable[] = {
	{ 1, 2412, bwn_b2063_chantable_data[0] },
	{ 2, 2417, bwn_b2063_chantable_data[0] },
	{ 3, 2422, bwn_b2063_chantable_data[0] },
	{ 4, 2427, bwn_b2063_chantable_data[1] },
	{ 5, 2432, bwn_b2063_chantable_data[1] },
	{ 6, 2437, bwn_b2063_chantable_data[1] },
	{ 7, 2442, bwn_b2063_chantable_data[1] },
	{ 8, 2447, bwn_b2063_chantable_data[1] },
	{ 9, 2452, bwn_b2063_chantable_data[2] },
	{ 10, 2457, bwn_b2063_chantable_data[2] },
	{ 11, 2462, bwn_b2063_chantable_data[3] },
	{ 12, 2467, bwn_b2063_chantable_data[3] },
	{ 13, 2472, bwn_b2063_chantable_data[3] },
	{ 14, 2484, bwn_b2063_chantable_data[4] },
	{ 34, 5170, bwn_b2063_chantable_data[5] },
	{ 36, 5180, bwn_b2063_chantable_data[6] },
	{ 38, 5190, bwn_b2063_chantable_data[7] },
	{ 40, 5200, bwn_b2063_chantable_data[8] },
	{ 42, 5210, bwn_b2063_chantable_data[9] },
	{ 44, 5220, bwn_b2063_chantable_data[10] },
	{ 46, 5230, bwn_b2063_chantable_data[11] },
	{ 48, 5240, bwn_b2063_chantable_data[12] },
	{ 52, 5260, bwn_b2063_chantable_data[13] },
	{ 56, 5280, bwn_b2063_chantable_data[14] },
	{ 60, 5300, bwn_b2063_chantable_data[14] },
	{ 64, 5320, bwn_b2063_chantable_data[15] },
	{ 100, 5500, bwn_b2063_chantable_data[16] },
	{ 104, 5520, bwn_b2063_chantable_data[17] },
	{ 108, 5540, bwn_b2063_chantable_data[18] },
	{ 112, 5560, bwn_b2063_chantable_data[19] },
	{ 116, 5580, bwn_b2063_chantable_data[20] },
	{ 120, 5600, bwn_b2063_chantable_data[21] },
	{ 124, 5620, bwn_b2063_chantable_data[21] },
	{ 128, 5640, bwn_b2063_chantable_data[22] },
	{ 132, 5660, bwn_b2063_chantable_data[22] },
	{ 136, 5680, bwn_b2063_chantable_data[22] },
	{ 140, 5700, bwn_b2063_chantable_data[23] },
	{ 149, 5745, bwn_b2063_chantable_data[23] },
	{ 153, 5765, bwn_b2063_chantable_data[23] },
	{ 157, 5785, bwn_b2063_chantable_data[23] },
	{ 161, 5805, bwn_b2063_chantable_data[23] },
	{ 165, 5825, bwn_b2063_chantable_data[23] },
	{ 184, 4920, bwn_b2063_chantable_data[24] },
	{ 188, 4940, bwn_b2063_chantable_data[25] },
	{ 192, 4960, bwn_b2063_chantable_data[26] },
	{ 196, 4980, bwn_b2063_chantable_data[27] },
	{ 200, 5000, bwn_b2063_chantable_data[28] },
	{ 204, 5020, bwn_b2063_chantable_data[29] },
	{ 208, 5040, bwn_b2063_chantable_data[30] },
	{ 212, 5060, bwn_b2063_chantable_data[31] },
	{ 216, 5080, bwn_b2063_chantable_data[32] }
};

static const uint8_t bwn_b2062_chantable_data[22][12] = {
	{ 0xff, 0xff, 0xb5, 0x1b, 0x24, 0x32, 0x32, 0x88, 0x88, 0, 0, 0 },
	{ 0, 0x22, 0x20, 0x84, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0x11, 0x10, 0x83, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0, 0, 0x83, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0x11, 0x20, 0x83, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0x11, 0x10, 0x84, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0x11, 0, 0x83, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0, 0, 0x63, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0, 0, 0x62, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0, 0, 0x30, 0x3c, 0x77, 0x37, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0, 0, 0x20, 0x3c, 0x77, 0x37, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0, 0, 0x10, 0x3c, 0x77, 0x37, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0, 0, 0, 0x3c, 0x77, 0x37, 0xff, 0x88, 0, 0, 0 },
	{ 0x55, 0x77, 0x90, 0xf7, 0x3c, 0x77, 0x35, 0xff, 0xff, 0, 0, 0 },
	{ 0x44, 0x77, 0x80, 0xe7, 0x3c, 0x77, 0x35, 0xff, 0xff, 0, 0, 0 },
	{ 0x44, 0x66, 0x80, 0xe7, 0x3c, 0x77, 0x35, 0xff, 0xff, 0, 0, 0 },
	{ 0x33, 0x66, 0x70, 0xc7, 0x3c, 0x77, 0x35, 0xff, 0xff, 0, 0, 0 },
	{ 0x22, 0x55, 0x60, 0xd7, 0x3c, 0x77, 0x35, 0xff, 0xff, 0, 0, 0 },
	{ 0x22, 0x55, 0x60, 0xc7, 0x3c, 0x77, 0x35, 0xff, 0xff, 0, 0, 0 },
	{ 0x22, 0x44, 0x50, 0xc7, 0x3c, 0x77, 0x35, 0xff, 0xff, 0, 0, 0 },
	{ 0x11, 0x44, 0x50, 0xa5, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 },
	{ 0, 0x44, 0x40, 0xb6, 0x3c, 0x77, 0x35, 0xff, 0x88, 0, 0, 0 }
};

static const struct bwn_b206x_chan bwn_b2062_chantable[] = {
	{ 1, 2412, bwn_b2062_chantable_data[0] },
	{ 2, 2417, bwn_b2062_chantable_data[0] },
	{ 3, 2422, bwn_b2062_chantable_data[0] },
	{ 4, 2427, bwn_b2062_chantable_data[0] },
	{ 5, 2432, bwn_b2062_chantable_data[0] },
	{ 6, 2437, bwn_b2062_chantable_data[0] },
	{ 7, 2442, bwn_b2062_chantable_data[0] },
	{ 8, 2447, bwn_b2062_chantable_data[0] },
	{ 9, 2452, bwn_b2062_chantable_data[0] },
	{ 10, 2457, bwn_b2062_chantable_data[0] },
	{ 11, 2462, bwn_b2062_chantable_data[0] },
	{ 12, 2467, bwn_b2062_chantable_data[0] },
	{ 13, 2472, bwn_b2062_chantable_data[0] },
	{ 14, 2484, bwn_b2062_chantable_data[0] },
	{ 34, 5170, bwn_b2062_chantable_data[1] },
	{ 38, 5190, bwn_b2062_chantable_data[2] },
	{ 42, 5210, bwn_b2062_chantable_data[2] },
	{ 46, 5230, bwn_b2062_chantable_data[3] },
	{ 36, 5180, bwn_b2062_chantable_data[4] },
	{ 40, 5200, bwn_b2062_chantable_data[5] },
	{ 44, 5220, bwn_b2062_chantable_data[6] },
	{ 48, 5240, bwn_b2062_chantable_data[3] },
	{ 52, 5260, bwn_b2062_chantable_data[3] },
	{ 56, 5280, bwn_b2062_chantable_data[3] },
	{ 60, 5300, bwn_b2062_chantable_data[7] },
	{ 64, 5320, bwn_b2062_chantable_data[8] },
	{ 100, 5500, bwn_b2062_chantable_data[9] },
	{ 104, 5520, bwn_b2062_chantable_data[10] },
	{ 108, 5540, bwn_b2062_chantable_data[10] },
	{ 112, 5560, bwn_b2062_chantable_data[10] },
	{ 116, 5580, bwn_b2062_chantable_data[11] },
	{ 120, 5600, bwn_b2062_chantable_data[12] },
	{ 124, 5620, bwn_b2062_chantable_data[12] },
	{ 128, 5640, bwn_b2062_chantable_data[12] },
	{ 132, 5660, bwn_b2062_chantable_data[12] },
	{ 136, 5680, bwn_b2062_chantable_data[12] },
	{ 140, 5700, bwn_b2062_chantable_data[12] },
	{ 149, 5745, bwn_b2062_chantable_data[12] },
	{ 153, 5765, bwn_b2062_chantable_data[12] },
	{ 157, 5785, bwn_b2062_chantable_data[12] },
	{ 161, 5805, bwn_b2062_chantable_data[12] },
	{ 165, 5825, bwn_b2062_chantable_data[12] },
	{ 184, 4920, bwn_b2062_chantable_data[13] },
	{ 188, 4940, bwn_b2062_chantable_data[14] },
	{ 192, 4960, bwn_b2062_chantable_data[15] },
	{ 196, 4980, bwn_b2062_chantable_data[16] },
	{ 200, 5000, bwn_b2062_chantable_data[17] },
	{ 204, 5020, bwn_b2062_chantable_data[18] },
	{ 208, 5040, bwn_b2062_chantable_data[19] },
	{ 212, 5060, bwn_b2062_chantable_data[20] },
	{ 216, 5080, bwn_b2062_chantable_data[21] }
};

/* for LP PHY */
static const struct bwn_rxcompco bwn_rxcompco_5354[] = {
	{  1, -66, 15 }, {  2, -66, 15 }, {  3, -66, 15 }, {  4, -66, 15 },
	{  5, -66, 15 }, {  6, -66, 15 }, {  7, -66, 14 }, {  8, -66, 14 },
	{  9, -66, 14 }, { 10, -66, 14 }, { 11, -66, 14 }, { 12, -66, 13 },
	{ 13, -66, 13 }, { 14, -66, 13 },
};

/* for LP PHY */
static const struct bwn_rxcompco bwn_rxcompco_r12[] = {
	{   1, -64, 13 }, {   2, -64, 13 }, {   3, -64, 13 }, {   4, -64, 13 },
	{   5, -64, 12 }, {   6, -64, 12 }, {   7, -64, 12 }, {   8, -64, 12 },
	{   9, -64, 12 }, {  10, -64, 11 }, {  11, -64, 11 }, {  12, -64, 11 },
	{  13, -64, 11 }, {  14, -64, 10 }, {  34, -62, 24 }, {  38, -62, 24 },
	{  42, -62, 24 }, {  46, -62, 23 }, {  36, -62, 24 }, {  40, -62, 24 },
	{  44, -62, 23 }, {  48, -62, 23 }, {  52, -62, 23 }, {  56, -62, 22 },
	{  60, -62, 22 }, {  64, -62, 22 }, { 100, -62, 16 }, { 104, -62, 16 },
	{ 108, -62, 15 }, { 112, -62, 14 }, { 116, -62, 14 }, { 120, -62, 13 },
	{ 124, -62, 12 }, { 128, -62, 12 }, { 132, -62, 12 }, { 136, -62, 11 },
	{ 140, -62, 10 }, { 149, -61,  9 }, { 153, -61,  9 }, { 157, -61,  9 },
	{ 161, -61,  8 }, { 165, -61,  8 }, { 184, -62, 25 }, { 188, -62, 25 },
	{ 192, -62, 25 }, { 196, -62, 25 }, { 200, -62, 25 }, { 204, -62, 25 },
	{ 208, -62, 25 }, { 212, -62, 25 }, { 216, -62, 26 },
};

static const struct bwn_rxcompco bwn_rxcompco_r2 = { 0, -64, 0 };

static const uint8_t bwn_tab_sigsq_tbl[] = {
	0xde, 0xdc, 0xda, 0xd8, 0xd6, 0xd4, 0xd2, 0xcf, 0xcd,
	0xca, 0xc7, 0xc4, 0xc1, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe,
	0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0x00,
	0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe,
	0xbe, 0xbe, 0xbe, 0xbe, 0xc1, 0xc4, 0xc7, 0xca, 0xcd,
	0xcf, 0xd2, 0xd4, 0xd6, 0xd8, 0xda, 0xdc, 0xde,
};

static const uint8_t bwn_tab_pllfrac_tbl[] = {
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
};

static const uint16_t bwn_tabl_iqlocal_tbl[] = {
	0x0200, 0x0300, 0x0400, 0x0600, 0x0800, 0x0b00, 0x1000, 0x1001, 0x1002,
	0x1003, 0x1004, 0x1005, 0x1006, 0x1007, 0x1707, 0x2007, 0x2d07, 0x4007,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0200, 0x0300, 0x0400, 0x0600,
	0x0800, 0x0b00, 0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006,
	0x1007, 0x1707, 0x2007, 0x2d07, 0x4007, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x4000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

static const uint16_t bwn_tab_noise_g1[] = BWN_TAB_NOISE_G1;
static const uint16_t bwn_tab_noise_g2[] = BWN_TAB_NOISE_G2;
static const uint16_t bwn_tab_noisescale_g1[] = BWN_TAB_NOISESCALE_G1;
static const uint16_t bwn_tab_noisescale_g2[] = BWN_TAB_NOISESCALE_G2;
static const uint16_t bwn_tab_noisescale_g3[] = BWN_TAB_NOISESCALE_G3;
const uint8_t bwn_bitrev_table[256] = BWN_BITREV_TABLE;

#define	VENDOR_LED_ACT(vendor)				\
{							\
	.vid = PCI_VENDOR_##vendor,			\
	.led_act = { BWN_VENDOR_LED_ACT_##vendor }	\
}

static const struct {
	uint16_t	vid;
	uint8_t		led_act[BWN_LED_MAX];
} bwn_vendor_led_act[] = {
	VENDOR_LED_ACT(COMPAQ),
	VENDOR_LED_ACT(ASUSTEK)
};

static const uint8_t bwn_default_led_act[BWN_LED_MAX] =
	{ BWN_VENDOR_LED_ACT_DEFAULT };

#undef VENDOR_LED_ACT

static const struct {
	int		on_dur;
	int		off_dur;
} bwn_led_duration[109] = {
	[0]	= { 400, 100 },
	[2]	= { 150, 75 },
	[4]	= { 90, 45 },
	[11]	= { 66, 34 },
	[12]	= { 53, 26 },
	[18]	= { 42, 21 },
	[22]	= { 35, 17 },
	[24]	= { 32, 16 },
	[36]	= { 21, 10 },
	[48]	= { 16, 8 },
	[72]	= { 11, 5 },
	[96]	= { 9, 4 },
	[108]	= { 7, 3 }
};

static const uint16_t bwn_wme_shm_offsets[] = {
	[0] = BWN_WME_BESTEFFORT,
	[1] = BWN_WME_BACKGROUND,
	[2] = BWN_WME_VOICE,
	[3] = BWN_WME_VIDEO,
};

static const struct siba_devid bwn_devs[] = {
	SIBA_DEV(BROADCOM, 80211, 5, "Revision 5"),
	SIBA_DEV(BROADCOM, 80211, 6, "Revision 6"),
	SIBA_DEV(BROADCOM, 80211, 7, "Revision 7"),
	SIBA_DEV(BROADCOM, 80211, 9, "Revision 9"),
	SIBA_DEV(BROADCOM, 80211, 10, "Revision 10"),
	SIBA_DEV(BROADCOM, 80211, 11, "Revision 11"),
	SIBA_DEV(BROADCOM, 80211, 13, "Revision 13"),
	SIBA_DEV(BROADCOM, 80211, 15, "Revision 15"),
	SIBA_DEV(BROADCOM, 80211, 16, "Revision 16")
};

static int
bwn_probe(device_t dev)
{
	int i;

	for (i = 0; i < sizeof(bwn_devs) / sizeof(bwn_devs[0]); i++) {
		if (siba_get_vendor(dev) == bwn_devs[i].sd_vendor &&
		    siba_get_device(dev) == bwn_devs[i].sd_device &&
		    siba_get_revid(dev) == bwn_devs[i].sd_rev)
			return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
bwn_attach(device_t dev)
{
	struct bwn_mac *mac;
	struct bwn_softc *sc = device_get_softc(dev);
	int error, i, msic, reg;

	sc->sc_dev = dev;
#ifdef BWN_DEBUG
	sc->sc_debug = bwn_debug;
#endif

	if ((sc->sc_flags & BWN_FLAG_ATTACHED) == 0) {
		error = bwn_attach_pre(sc);
		if (error != 0)
			return (error);
		bwn_sprom_bugfixes(dev);
		sc->sc_flags |= BWN_FLAG_ATTACHED;
	}

	if (!TAILQ_EMPTY(&sc->sc_maclist)) {
		if (siba_get_pci_device(dev) != 0x4313 &&
		    siba_get_pci_device(dev) != 0x431a &&
		    siba_get_pci_device(dev) != 0x4321) {
			device_printf(sc->sc_dev,
			    "skip 802.11 cores\n");
			return (ENODEV);
		}
	}

	mac = (struct bwn_mac *)malloc(sizeof(*mac), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (mac == NULL)
		return (ENOMEM);
	mac->mac_sc = sc;
	mac->mac_status = BWN_MAC_STATUS_UNINIT;
	if (bwn_bfp != 0)
		mac->mac_flags |= BWN_MAC_FLAG_BADFRAME_PREEMP;

	TASK_INIT(&mac->mac_hwreset, 0, bwn_hwreset, mac);
	TASK_INIT(&mac->mac_intrtask, 0, bwn_intrtask, mac);
	TASK_INIT(&mac->mac_txpower, 0, bwn_txpwr, mac);

	error = bwn_attach_core(mac);
	if (error)
		goto fail0;
	bwn_led_attach(mac);

	device_printf(sc->sc_dev, "WLAN (chipid %#x rev %u) "
	    "PHY (analog %d type %d rev %d) RADIO (manuf %#x ver %#x rev %d)\n",
	    siba_get_chipid(sc->sc_dev), siba_get_revid(sc->sc_dev),
	    mac->mac_phy.analog, mac->mac_phy.type, mac->mac_phy.rev,
	    mac->mac_phy.rf_manuf, mac->mac_phy.rf_ver,
	    mac->mac_phy.rf_rev);
	if (mac->mac_flags & BWN_MAC_FLAG_DMA)
		device_printf(sc->sc_dev, "DMA (%d bits)\n",
		    mac->mac_method.dma.dmatype);
	else
		device_printf(sc->sc_dev, "PIO\n");

	/*
	 * setup PCI resources and interrupt.
	 */
	if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
		msic = pci_msi_count(dev);
		if (bootverbose)
			device_printf(sc->sc_dev, "MSI count : %d\n", msic);
	} else
		msic = 0;

	mac->mac_intr_spec = bwn_res_spec_legacy;
	if (msic == BWN_MSI_MESSAGES && bwn_msi_disable == 0) {
		if (pci_alloc_msi(dev, &msic) == 0) {
			device_printf(sc->sc_dev,
			    "Using %d MSI messages\n", msic);
			mac->mac_intr_spec = bwn_res_spec_msi;
			mac->mac_msi = 1;
		}
	}

	error = bus_alloc_resources(dev, mac->mac_intr_spec,
	    mac->mac_res_irq);
	if (error) {
		device_printf(sc->sc_dev,
		    "couldn't allocate IRQ resources (%d)\n", error);
		goto fail1;
	}

	if (mac->mac_msi == 0)
		error = bus_setup_intr(dev, mac->mac_res_irq[0],
		    INTR_TYPE_NET | INTR_MPSAFE, bwn_intr, NULL, mac,
		    &mac->mac_intrhand[0]);
	else {
		for (i = 0; i < BWN_MSI_MESSAGES; i++) {
			error = bus_setup_intr(dev, mac->mac_res_irq[i],
			    INTR_TYPE_NET | INTR_MPSAFE, bwn_intr, NULL, mac,
			    &mac->mac_intrhand[i]);
			if (error != 0) {
				device_printf(sc->sc_dev,
				    "couldn't setup interrupt (%d)\n", error);
				break;
			}
		}
	}

	TAILQ_INSERT_TAIL(&sc->sc_maclist, mac, mac_list);

	/*
	 * calls attach-post routine
	 */
	if ((sc->sc_flags & BWN_FLAG_ATTACHED) != 0)
		bwn_attach_post(sc);

	return (0);
fail1:
	if (msic == BWN_MSI_MESSAGES && bwn_msi_disable == 0)
		pci_release_msi(dev);
fail0:
	free(mac, M_DEVBUF);
	return (error);
}

static int
bwn_is_valid_ether_addr(uint8_t *addr)
{
	char zero_addr[6] = { 0, 0, 0, 0, 0, 0 };

	if ((addr[0] & 1) || (!bcmp(addr, zero_addr, ETHER_ADDR_LEN)))
		return (FALSE);

	return (TRUE);
}

static int
bwn_attach_post(struct bwn_softc *sc)
{
	struct ieee80211com *ic;
	struct ifnet *ifp = sc->sc_ifp;

	ic = ifp->if_l2com;
	ic->ic_ifp = ifp;
	/* XXX not right but it's not used anywhere important */
	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps =
		  IEEE80211_C_STA		/* station mode supported */
		| IEEE80211_C_MONITOR		/* monitor mode */
		| IEEE80211_C_AHDEMO		/* adhoc demo mode */
		| IEEE80211_C_SHPREAMBLE	/* short preamble supported */
		| IEEE80211_C_SHSLOT		/* short slot time supported */
		| IEEE80211_C_WME		/* WME/WMM supported */
		| IEEE80211_C_WPA		/* capable of WPA1+WPA2 */
		| IEEE80211_C_BGSCAN		/* capable of bg scanning */
		| IEEE80211_C_TXPMGT		/* capable of txpow mgt */
		;

	ic->ic_flags_ext |= IEEE80211_FEXT_SWBMISS;	/* s/w bmiss */

	/* call MI attach routine. */
	ieee80211_ifattach(ic,
	    bwn_is_valid_ether_addr(siba_sprom_get_mac_80211a(sc->sc_dev)) ?
	    siba_sprom_get_mac_80211a(sc->sc_dev) :
	    siba_sprom_get_mac_80211bg(sc->sc_dev));

	ic->ic_headroom = sizeof(struct bwn_txhdr);

	/* override default methods */
	ic->ic_raw_xmit = bwn_raw_xmit;
	ic->ic_updateslot = bwn_updateslot;
	ic->ic_update_promisc = bwn_update_promisc;
	ic->ic_wme.wme_update = bwn_wme_update;

	ic->ic_scan_start = bwn_scan_start;
	ic->ic_scan_end = bwn_scan_end;
	ic->ic_set_channel = bwn_set_channel;

	ic->ic_vap_create = bwn_vap_create;
	ic->ic_vap_delete = bwn_vap_delete;

	ieee80211_radiotap_attach(ic,
	    &sc->sc_tx_th.wt_ihdr, sizeof(sc->sc_tx_th),
	    BWN_TX_RADIOTAP_PRESENT,
	    &sc->sc_rx_th.wr_ihdr, sizeof(sc->sc_rx_th),
	    BWN_RX_RADIOTAP_PRESENT);

	bwn_sysctl_node(sc);

	if (bootverbose)
		ieee80211_announce(ic);
	return (0);
}

static void
bwn_phy_detach(struct bwn_mac *mac)
{

	if (mac->mac_phy.detach != NULL)
		mac->mac_phy.detach(mac);
}

static int
bwn_detach(device_t dev)
{
	struct bwn_softc *sc = device_get_softc(dev);
	struct bwn_mac *mac = sc->sc_curmac;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	int i;

	sc->sc_flags |= BWN_FLAG_INVALID;

	if (device_is_attached(sc->sc_dev)) {
		bwn_stop(sc, 1);
		bwn_dma_free(mac);
		callout_drain(&sc->sc_led_blink_ch);
		callout_drain(&sc->sc_rfswitch_ch);
		callout_drain(&sc->sc_task_ch);
		callout_drain(&sc->sc_watchdog_ch);
		bwn_phy_detach(mac);
		if (ifp != NULL) {
			ieee80211_draintask(ic, &mac->mac_hwreset);
			ieee80211_draintask(ic, &mac->mac_txpower);
			ieee80211_ifdetach(ic);
			if_free(ifp);
		}
	}
	taskqueue_drain(sc->sc_tq, &mac->mac_intrtask);
	taskqueue_free(sc->sc_tq);

	for (i = 0; i < BWN_MSI_MESSAGES; i++) {
		if (mac->mac_intrhand[i] != NULL) {
			bus_teardown_intr(dev, mac->mac_res_irq[i],
			    mac->mac_intrhand[i]);
			mac->mac_intrhand[i] = NULL;
		}
	}
	bus_release_resources(dev, mac->mac_intr_spec, mac->mac_res_irq);
	if (mac->mac_msi != 0)
		pci_release_msi(dev);

	BWN_LOCK_DESTROY(sc);
	return (0);
}

static int
bwn_attach_pre(struct bwn_softc *sc)
{
	struct ifnet *ifp;
	int error = 0;

	BWN_LOCK_INIT(sc);
	TAILQ_INIT(&sc->sc_maclist);
	callout_init_mtx(&sc->sc_rfswitch_ch, &sc->sc_mtx, 0);
	callout_init_mtx(&sc->sc_task_ch, &sc->sc_mtx, 0);
	callout_init_mtx(&sc->sc_watchdog_ch, &sc->sc_mtx, 0);

	sc->sc_tq = taskqueue_create_fast("bwn_taskq", M_NOWAIT,
		taskqueue_thread_enqueue, &sc->sc_tq);
	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET,
		"%s taskq", device_get_nameunit(sc->sc_dev));

	ifp = sc->sc_ifp = if_alloc(IFT_IEEE80211);
	if (ifp == NULL) {
		device_printf(sc->sc_dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}

	/* set these up early for if_printf use */
	if_initname(ifp, device_get_name(sc->sc_dev),
	    device_get_unit(sc->sc_dev));

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = bwn_init;
	ifp->if_ioctl = bwn_ioctl;
	ifp->if_start = bwn_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	ifp->if_snd.ifq_drv_maxlen = ifqmaxlen;
	IFQ_SET_READY(&ifp->if_snd);

	return (0);

fail:	BWN_LOCK_DESTROY(sc);
	return (error);
}

static void
bwn_sprom_bugfixes(device_t dev)
{
#define	BWN_ISDEV(_vendor, _device, _subvendor, _subdevice)		\
	((siba_get_pci_vendor(dev) == PCI_VENDOR_##_vendor) &&		\
	 (siba_get_pci_device(dev) == _device) &&			\
	 (siba_get_pci_subvendor(dev) == PCI_VENDOR_##_subvendor) &&	\
	 (siba_get_pci_subdevice(dev) == _subdevice))

	if (siba_get_pci_subvendor(dev) == PCI_VENDOR_APPLE &&
	    siba_get_pci_subdevice(dev) == 0x4e &&
	    siba_get_pci_revid(dev) > 0x40)
		siba_sprom_set_bf_lo(dev,
		    siba_sprom_get_bf_lo(dev) | BWN_BFL_PACTRL);
	if (siba_get_pci_subvendor(dev) == SIBA_BOARDVENDOR_DELL &&
	    siba_get_chipid(dev) == 0x4301 && siba_get_pci_revid(dev) == 0x74)
		siba_sprom_set_bf_lo(dev,
		    siba_sprom_get_bf_lo(dev) | BWN_BFL_BTCOEXIST);
	if (siba_get_type(dev) == SIBA_TYPE_PCI) {
		if (BWN_ISDEV(BROADCOM, 0x4318, ASUSTEK, 0x100f) ||
		    BWN_ISDEV(BROADCOM, 0x4320, DELL, 0x0003) ||
		    BWN_ISDEV(BROADCOM, 0x4320, HP, 0x12f8) ||
		    BWN_ISDEV(BROADCOM, 0x4320, LINKSYS, 0x0013) ||
		    BWN_ISDEV(BROADCOM, 0x4320, LINKSYS, 0x0014) ||
		    BWN_ISDEV(BROADCOM, 0x4320, LINKSYS, 0x0015) ||
		    BWN_ISDEV(BROADCOM, 0x4320, MOTOROLA, 0x7010))
			siba_sprom_set_bf_lo(dev,
			    siba_sprom_get_bf_lo(dev) & ~BWN_BFL_BTCOEXIST);
	}
#undef	BWN_ISDEV
}

static int
bwn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
#define	IS_RUNNING(ifp) \
	((ifp->if_flags & IFF_UP) && (ifp->if_drv_flags & IFF_DRV_RUNNING))
	struct bwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0, startall;

	switch (cmd) {
	case SIOCSIFFLAGS:
		startall = 0;
		if (IS_RUNNING(ifp)) {
			bwn_update_promisc(ifp);
		} else if (ifp->if_flags & IFF_UP) {
			if ((sc->sc_flags & BWN_FLAG_INVALID) == 0) {
				bwn_init(sc);
				startall = 1;
			}
		} else
			bwn_stop(sc, 1);
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
	return (error);
}

static void
bwn_start(struct ifnet *ifp)
{
	struct bwn_softc *sc = ifp->if_softc;

	BWN_LOCK(sc);
	bwn_start_locked(ifp);
	BWN_UNLOCK(sc);
}

static void
bwn_start_locked(struct ifnet *ifp)
{
	struct bwn_softc *sc = ifp->if_softc;
	struct bwn_mac *mac = sc->sc_curmac;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct ieee80211_key *k;
	struct mbuf *m;

	BWN_ASSERT_LOCKED(sc);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 || mac == NULL ||
	    mac->mac_status < BWN_MAC_STATUS_STARTED)
		return;

	for (;;) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);	/* XXX: LOCK */
		if (m == NULL)
			break;

		if (bwn_tx_isfull(sc, m))
			break;
		ni = (struct ieee80211_node *) m->m_pkthdr.rcvif;
		if (ni == NULL) {
			device_printf(sc->sc_dev, "unexpected NULL ni\n");
			m_freem(m);
			ifp->if_oerrors++;
			continue;
		}
		KASSERT(ni != NULL, ("%s:%d: fail", __func__, __LINE__));
		wh = mtod(m, struct ieee80211_frame *);
		if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
			k = ieee80211_crypto_encap(ni, m);
			if (k == NULL) {
				ieee80211_free_node(ni);
				m_freem(m);
				ifp->if_oerrors++;
				continue;
			}
		}
		wh = NULL;	/* Catch any invalid use */

		if (bwn_tx_start(sc, ni, m) != 0) {
			if (ni != NULL)
				ieee80211_free_node(ni);
			ifp->if_oerrors++;
			continue;
		}

		sc->sc_watchdog_timer = 5;
	}
}

static int
bwn_tx_isfull(struct bwn_softc *sc, struct mbuf *m)
{
	struct bwn_dma_ring *dr;
	struct bwn_mac *mac = sc->sc_curmac;
	struct bwn_pio_txqueue *tq;
	struct ifnet *ifp = sc->sc_ifp;
	int pktlen = roundup(m->m_pkthdr.len + BWN_HDRSIZE(mac), 4);

	BWN_ASSERT_LOCKED(sc);

	if (mac->mac_flags & BWN_MAC_FLAG_DMA) {
		dr = bwn_dma_select(mac, M_WME_GETAC(m));
		if (dr->dr_stop == 1 ||
		    bwn_dma_freeslot(dr) < BWN_TX_SLOTS_PER_FRAME) {
			dr->dr_stop = 1;
			goto full;
		}
	} else {
		tq = bwn_pio_select(mac, M_WME_GETAC(m));
		if (tq->tq_free == 0 || pktlen > tq->tq_size ||
		    pktlen > (tq->tq_size - tq->tq_used)) {
			tq->tq_stop = 1;
			goto full;
		}
	}
	return (0);
full:
	IFQ_DRV_PREPEND(&ifp->if_snd, m);
	ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	return (1);
}

static int
bwn_tx_start(struct bwn_softc *sc, struct ieee80211_node *ni, struct mbuf *m)
{
	struct bwn_mac *mac = sc->sc_curmac;
	int error;

	BWN_ASSERT_LOCKED(sc);

	if (m->m_pkthdr.len < IEEE80211_MIN_LEN || mac == NULL) {
		m_freem(m);
		return (ENXIO);
	}

	error = (mac->mac_flags & BWN_MAC_FLAG_DMA) ?
	    bwn_dma_tx_start(mac, ni, m) : bwn_pio_tx_start(mac, ni, m);
	if (error) {
		m_freem(m);
		return (error);
	}
	return (0);
}

static int
bwn_pio_tx_start(struct bwn_mac *mac, struct ieee80211_node *ni, struct mbuf *m)
{
	struct bwn_pio_txpkt *tp;
	struct bwn_pio_txqueue *tq = bwn_pio_select(mac, M_WME_GETAC(m));
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_txhdr txhdr;
	struct mbuf *m_new;
	uint32_t ctl32;
	int error;
	uint16_t ctl16;

	BWN_ASSERT_LOCKED(sc);

	/* XXX TODO send packets after DTIM */

	KASSERT(!TAILQ_EMPTY(&tq->tq_pktlist), ("%s: fail", __func__));
	tp = TAILQ_FIRST(&tq->tq_pktlist);
	tp->tp_ni = ni;
	tp->tp_m = m;

	error = bwn_set_txhdr(mac, ni, m, &txhdr, BWN_PIO_COOKIE(tq, tp));
	if (error) {
		device_printf(sc->sc_dev, "tx fail\n");
		return (error);
	}

	TAILQ_REMOVE(&tq->tq_pktlist, tp, tp_list);
	tq->tq_used += roundup(m->m_pkthdr.len + BWN_HDRSIZE(mac), 4);
	tq->tq_free--;

	if (siba_get_revid(sc->sc_dev) >= 8) {
		/*
		 * XXX please removes m_defrag(9)
		 */
		m_new = m_defrag(m, M_DONTWAIT);
		if (m_new == NULL) {
			device_printf(sc->sc_dev,
			    "%s: can't defrag TX buffer\n",
			    __func__);
			return (ENOBUFS);
		}
		if (m_new->m_next != NULL)
			device_printf(sc->sc_dev,
			    "TODO: fragmented packets for PIO\n");
		tp->tp_m = m_new;

		/* send HEADER */
		ctl32 = bwn_pio_write_multi_4(mac, tq,
		    (BWN_PIO_READ_4(mac, tq, BWN_PIO8_TXCTL) |
			BWN_PIO8_TXCTL_FRAMEREADY) & ~BWN_PIO8_TXCTL_EOF,
		    (const uint8_t *)&txhdr, BWN_HDRSIZE(mac));
		/* send BODY */
		ctl32 = bwn_pio_write_multi_4(mac, tq, ctl32,
		    mtod(m_new, const void *), m_new->m_pkthdr.len);
		bwn_pio_write_4(mac, tq, BWN_PIO_TXCTL,
		    ctl32 | BWN_PIO8_TXCTL_EOF);
	} else {
		ctl16 = bwn_pio_write_multi_2(mac, tq,
		    (bwn_pio_read_2(mac, tq, BWN_PIO_TXCTL) |
			BWN_PIO_TXCTL_FRAMEREADY) & ~BWN_PIO_TXCTL_EOF,
		    (const uint8_t *)&txhdr, BWN_HDRSIZE(mac));
		ctl16 = bwn_pio_write_mbuf_2(mac, tq, ctl16, m);
		BWN_PIO_WRITE_2(mac, tq, BWN_PIO_TXCTL,
		    ctl16 | BWN_PIO_TXCTL_EOF);
	}

	return (0);
}

static struct bwn_pio_txqueue *
bwn_pio_select(struct bwn_mac *mac, uint8_t prio)
{

	if ((mac->mac_flags & BWN_MAC_FLAG_WME) == 0)
		return (&mac->mac_method.pio.wme[WME_AC_BE]);

	switch (prio) {
	case 0:
		return (&mac->mac_method.pio.wme[WME_AC_BE]);
	case 1:
		return (&mac->mac_method.pio.wme[WME_AC_BK]);
	case 2:
		return (&mac->mac_method.pio.wme[WME_AC_VI]);
	case 3:
		return (&mac->mac_method.pio.wme[WME_AC_VO]);
	}
	KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	return (NULL);
}

static int
bwn_dma_tx_start(struct bwn_mac *mac, struct ieee80211_node *ni, struct mbuf *m)
{
#define	BWN_GET_TXHDRCACHE(slot)					\
	&(txhdr_cache[(slot / BWN_TX_SLOTS_PER_FRAME) * BWN_HDRSIZE(mac)])
	struct bwn_dma *dma = &mac->mac_method.dma;
	struct bwn_dma_ring *dr = bwn_dma_select(mac, M_WME_GETAC(m));
	struct bwn_dmadesc_generic *desc;
	struct bwn_dmadesc_meta *mt;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	uint8_t *txhdr_cache = (uint8_t *)dr->dr_txhdr_cache;
	int error, slot, backup[2] = { dr->dr_curslot, dr->dr_usedslot };

	BWN_ASSERT_LOCKED(sc);
	KASSERT(!dr->dr_stop, ("%s:%d: fail", __func__, __LINE__));

	/* XXX send after DTIM */

	slot = bwn_dma_getslot(dr);
	dr->getdesc(dr, slot, &desc, &mt);
	KASSERT(mt->mt_txtype == BWN_DMADESC_METATYPE_HEADER,
	    ("%s:%d: fail", __func__, __LINE__));

	error = bwn_set_txhdr(dr->dr_mac, ni, m,
	    (struct bwn_txhdr *)BWN_GET_TXHDRCACHE(slot),
	    BWN_DMA_COOKIE(dr, slot));
	if (error)
		goto fail;
	error = bus_dmamap_load(dr->dr_txring_dtag, mt->mt_dmap,
	    BWN_GET_TXHDRCACHE(slot), BWN_HDRSIZE(mac), bwn_dma_ring_addr,
	    &mt->mt_paddr, BUS_DMA_NOWAIT);
	if (error) {
		if_printf(ifp, "%s: can't load TX buffer (1) %d\n",
		    __func__, error);
		goto fail;
	}
	bus_dmamap_sync(dr->dr_txring_dtag, mt->mt_dmap,
	    BUS_DMASYNC_PREWRITE);
	dr->setdesc(dr, desc, mt->mt_paddr, BWN_HDRSIZE(mac), 1, 0, 0);
	bus_dmamap_sync(dr->dr_ring_dtag, dr->dr_ring_dmap,
	    BUS_DMASYNC_PREWRITE);

	slot = bwn_dma_getslot(dr);
	dr->getdesc(dr, slot, &desc, &mt);
	KASSERT(mt->mt_txtype == BWN_DMADESC_METATYPE_BODY &&
	    mt->mt_islast == 1, ("%s:%d: fail", __func__, __LINE__));
	mt->mt_m = m;
	mt->mt_ni = ni;

	error = bus_dmamap_load_mbuf(dma->txbuf_dtag, mt->mt_dmap, m,
	    bwn_dma_buf_addr, &mt->mt_paddr, BUS_DMA_NOWAIT);
	if (error && error != EFBIG) {
		if_printf(ifp, "%s: can't load TX buffer (1) %d\n",
		    __func__, error);
		goto fail;
	}
	if (error) {    /* error == EFBIG */
		struct mbuf *m_new;

		m_new = m_defrag(m, M_DONTWAIT);
		if (m_new == NULL) {
			if_printf(ifp, "%s: can't defrag TX buffer\n",
			    __func__);
			error = ENOBUFS;
			goto fail;
		} else {
			m = m_new;
		}

		mt->mt_m = m;
		error = bus_dmamap_load_mbuf(dma->txbuf_dtag, mt->mt_dmap,
		    m, bwn_dma_buf_addr, &mt->mt_paddr, BUS_DMA_NOWAIT);
		if (error) {
			if_printf(ifp, "%s: can't load TX buffer (2) %d\n",
			    __func__, error);
			goto fail;
		}
	}
	bus_dmamap_sync(dma->txbuf_dtag, mt->mt_dmap, BUS_DMASYNC_PREWRITE);
	dr->setdesc(dr, desc, mt->mt_paddr, m->m_pkthdr.len, 0, 1, 1);
	bus_dmamap_sync(dr->dr_ring_dtag, dr->dr_ring_dmap,
	    BUS_DMASYNC_PREWRITE);

	/* XXX send after DTIM */

	dr->start_transfer(dr, bwn_dma_nextslot(dr, slot));
	return (0);
fail:
	dr->dr_curslot = backup[0];
	dr->dr_usedslot = backup[1];
	return (error);
#undef BWN_GET_TXHDRCACHE
}

static void
bwn_watchdog(void *arg)
{
	struct bwn_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;

	if (sc->sc_watchdog_timer != 0 && --sc->sc_watchdog_timer == 0) {
		if_printf(ifp, "device timeout\n");
		ifp->if_oerrors++;
	}
	callout_schedule(&sc->sc_watchdog_ch, hz);
}

static int
bwn_attach_core(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	int error, have_bg = 0, have_a = 0;
	uint32_t high;

	KASSERT(siba_get_revid(sc->sc_dev) >= 5,
	    ("unsupported revision %d", siba_get_revid(sc->sc_dev)));

	siba_powerup(sc->sc_dev, 0);

	high = siba_read_4(sc->sc_dev, SIBA_TGSHIGH);
	bwn_reset_core(mac,
	    (high & BWN_TGSHIGH_HAVE_2GHZ) ? BWN_TGSLOW_SUPPORT_G : 0);
	error = bwn_phy_getinfo(mac, high);
	if (error)
		goto fail;

	have_a = (high & BWN_TGSHIGH_HAVE_5GHZ) ? 1 : 0;
	have_bg = (high & BWN_TGSHIGH_HAVE_2GHZ) ? 1 : 0;
	if (siba_get_pci_device(sc->sc_dev) != 0x4312 &&
	    siba_get_pci_device(sc->sc_dev) != 0x4319 &&
	    siba_get_pci_device(sc->sc_dev) != 0x4324) {
		have_a = have_bg = 0;
		if (mac->mac_phy.type == BWN_PHYTYPE_A)
			have_a = 1;
		else if (mac->mac_phy.type == BWN_PHYTYPE_G ||
		    mac->mac_phy.type == BWN_PHYTYPE_N ||
		    mac->mac_phy.type == BWN_PHYTYPE_LP)
			have_bg = 1;
		else
			KASSERT(0 == 1, ("%s: unknown phy type (%d)", __func__,
			    mac->mac_phy.type));
	}
	/* XXX turns off PHY A because it's not supported */
	if (mac->mac_phy.type != BWN_PHYTYPE_LP &&
	    mac->mac_phy.type != BWN_PHYTYPE_N) {
		have_a = 0;
		have_bg = 1;
	}

	if (mac->mac_phy.type == BWN_PHYTYPE_G) {
		mac->mac_phy.attach = bwn_phy_g_attach;
		mac->mac_phy.detach = bwn_phy_g_detach;
		mac->mac_phy.prepare_hw = bwn_phy_g_prepare_hw;
		mac->mac_phy.init_pre = bwn_phy_g_init_pre;
		mac->mac_phy.init = bwn_phy_g_init;
		mac->mac_phy.exit = bwn_phy_g_exit;
		mac->mac_phy.phy_read = bwn_phy_g_read;
		mac->mac_phy.phy_write = bwn_phy_g_write;
		mac->mac_phy.rf_read = bwn_phy_g_rf_read;
		mac->mac_phy.rf_write = bwn_phy_g_rf_write;
		mac->mac_phy.use_hwpctl = bwn_phy_g_hwpctl;
		mac->mac_phy.rf_onoff = bwn_phy_g_rf_onoff;
		mac->mac_phy.switch_analog = bwn_phy_switch_analog;
		mac->mac_phy.switch_channel = bwn_phy_g_switch_channel;
		mac->mac_phy.get_default_chan = bwn_phy_g_get_default_chan;
		mac->mac_phy.set_antenna = bwn_phy_g_set_antenna;
		mac->mac_phy.set_im = bwn_phy_g_im;
		mac->mac_phy.recalc_txpwr = bwn_phy_g_recalc_txpwr;
		mac->mac_phy.set_txpwr = bwn_phy_g_set_txpwr;
		mac->mac_phy.task_15s = bwn_phy_g_task_15s;
		mac->mac_phy.task_60s = bwn_phy_g_task_60s;
	} else if (mac->mac_phy.type == BWN_PHYTYPE_LP) {
		mac->mac_phy.init_pre = bwn_phy_lp_init_pre;
		mac->mac_phy.init = bwn_phy_lp_init;
		mac->mac_phy.phy_read = bwn_phy_lp_read;
		mac->mac_phy.phy_write = bwn_phy_lp_write;
		mac->mac_phy.phy_maskset = bwn_phy_lp_maskset;
		mac->mac_phy.rf_read = bwn_phy_lp_rf_read;
		mac->mac_phy.rf_write = bwn_phy_lp_rf_write;
		mac->mac_phy.rf_onoff = bwn_phy_lp_rf_onoff;
		mac->mac_phy.switch_analog = bwn_phy_lp_switch_analog;
		mac->mac_phy.switch_channel = bwn_phy_lp_switch_channel;
		mac->mac_phy.get_default_chan = bwn_phy_lp_get_default_chan;
		mac->mac_phy.set_antenna = bwn_phy_lp_set_antenna;
		mac->mac_phy.task_60s = bwn_phy_lp_task_60s;
	} else {
		device_printf(sc->sc_dev, "unsupported PHY type (%d)\n",
		    mac->mac_phy.type);
		error = ENXIO;
		goto fail;
	}

	mac->mac_phy.gmode = have_bg;
	if (mac->mac_phy.attach != NULL) {
		error = mac->mac_phy.attach(mac);
		if (error) {
			device_printf(sc->sc_dev, "failed\n");
			goto fail;
		}
	}

	bwn_reset_core(mac, have_bg ? BWN_TGSLOW_SUPPORT_G : 0);

	error = bwn_chiptest(mac);
	if (error)
		goto fail;
	error = bwn_setup_channels(mac, have_bg, have_a);
	if (error) {
		device_printf(sc->sc_dev, "failed to setup channels\n");
		goto fail;
	}

	if (sc->sc_curmac == NULL)
		sc->sc_curmac = mac;

	error = bwn_dma_attach(mac);
	if (error != 0) {
		device_printf(sc->sc_dev, "failed to initialize DMA\n");
		goto fail;
	}

	mac->mac_phy.switch_analog(mac, 0);

	siba_dev_down(sc->sc_dev, 0);
fail:
	siba_powerdown(sc->sc_dev);
	return (error);
}

static void
bwn_reset_core(struct bwn_mac *mac, uint32_t flags)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t low, ctl;

	flags |= (BWN_TGSLOW_PHYCLOCK_ENABLE | BWN_TGSLOW_PHYRESET);

	siba_dev_up(sc->sc_dev, flags);
	DELAY(2000);

	low = (siba_read_4(sc->sc_dev, SIBA_TGSLOW) | SIBA_TGSLOW_FGC) &
	    ~BWN_TGSLOW_PHYRESET;
	siba_write_4(sc->sc_dev, SIBA_TGSLOW, low);
	siba_read_4(sc->sc_dev, SIBA_TGSLOW);
	DELAY(1000);
	siba_write_4(sc->sc_dev, SIBA_TGSLOW, low & ~SIBA_TGSLOW_FGC);
	siba_read_4(sc->sc_dev, SIBA_TGSLOW);
	DELAY(1000);

	if (mac->mac_phy.switch_analog != NULL)
		mac->mac_phy.switch_analog(mac, 1);

	ctl = BWN_READ_4(mac, BWN_MACCTL) & ~BWN_MACCTL_GMODE;
	if (flags & BWN_TGSLOW_SUPPORT_G)
		ctl |= BWN_MACCTL_GMODE;
	BWN_WRITE_4(mac, BWN_MACCTL, ctl | BWN_MACCTL_IHR_ON);
}

static int
bwn_phy_getinfo(struct bwn_mac *mac, int tgshigh)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t tmp;

	/* PHY */
	tmp = BWN_READ_2(mac, BWN_PHYVER);
	phy->gmode = (tgshigh & BWN_TGSHIGH_HAVE_2GHZ) ? 1 : 0;
	phy->rf_on = 1;
	phy->analog = (tmp & BWN_PHYVER_ANALOG) >> 12;
	phy->type = (tmp & BWN_PHYVER_TYPE) >> 8;
	phy->rev = (tmp & BWN_PHYVER_VERSION);
	if ((phy->type == BWN_PHYTYPE_A && phy->rev >= 4) ||
	    (phy->type == BWN_PHYTYPE_B && phy->rev != 2 &&
		phy->rev != 4 && phy->rev != 6 && phy->rev != 7) ||
	    (phy->type == BWN_PHYTYPE_G && phy->rev > 9) ||
	    (phy->type == BWN_PHYTYPE_N && phy->rev > 4) ||
	    (phy->type == BWN_PHYTYPE_LP && phy->rev > 2))
		goto unsupphy;

	/* RADIO */
	if (siba_get_chipid(sc->sc_dev) == 0x4317) {
		if (siba_get_chiprev(sc->sc_dev) == 0)
			tmp = 0x3205017f;
		else if (siba_get_chiprev(sc->sc_dev) == 1)
			tmp = 0x4205017f;
		else
			tmp = 0x5205017f;
	} else {
		BWN_WRITE_2(mac, BWN_RFCTL, BWN_RFCTL_ID);
		tmp = BWN_READ_2(mac, BWN_RFDATALO);
		BWN_WRITE_2(mac, BWN_RFCTL, BWN_RFCTL_ID);
		tmp |= (uint32_t)BWN_READ_2(mac, BWN_RFDATAHI) << 16;
	}
	phy->rf_rev = (tmp & 0xf0000000) >> 28;
	phy->rf_ver = (tmp & 0x0ffff000) >> 12;
	phy->rf_manuf = (tmp & 0x00000fff);
	if (phy->rf_manuf != 0x17f)	/* 0x17f is broadcom */
		goto unsupradio;
	if ((phy->type == BWN_PHYTYPE_A && (phy->rf_ver != 0x2060 ||
	     phy->rf_rev != 1 || phy->rf_manuf != 0x17f)) ||
	    (phy->type == BWN_PHYTYPE_B && (phy->rf_ver & 0xfff0) != 0x2050) ||
	    (phy->type == BWN_PHYTYPE_G && phy->rf_ver != 0x2050) ||
	    (phy->type == BWN_PHYTYPE_N &&
	     phy->rf_ver != 0x2055 && phy->rf_ver != 0x2056) ||
	    (phy->type == BWN_PHYTYPE_LP &&
	     phy->rf_ver != 0x2062 && phy->rf_ver != 0x2063))
		goto unsupradio;

	return (0);
unsupphy:
	device_printf(sc->sc_dev, "unsupported PHY (type %#x, rev %#x, "
	    "analog %#x)\n",
	    phy->type, phy->rev, phy->analog);
	return (ENXIO);
unsupradio:
	device_printf(sc->sc_dev, "unsupported radio (manuf %#x, ver %#x, "
	    "rev %#x)\n",
	    phy->rf_manuf, phy->rf_ver, phy->rf_rev);
	return (ENXIO);
}

static int
bwn_chiptest(struct bwn_mac *mac)
{
#define	TESTVAL0	0x55aaaa55
#define	TESTVAL1	0xaa5555aa
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t v, backup;

	BWN_LOCK(sc);

	backup = bwn_shm_read_4(mac, BWN_SHARED, 0);

	bwn_shm_write_4(mac, BWN_SHARED, 0, TESTVAL0);
	if (bwn_shm_read_4(mac, BWN_SHARED, 0) != TESTVAL0)
		goto error;
	bwn_shm_write_4(mac, BWN_SHARED, 0, TESTVAL1);
	if (bwn_shm_read_4(mac, BWN_SHARED, 0) != TESTVAL1)
		goto error;

	bwn_shm_write_4(mac, BWN_SHARED, 0, backup);

	if ((siba_get_revid(sc->sc_dev) >= 3) &&
	    (siba_get_revid(sc->sc_dev) <= 10)) {
		BWN_WRITE_2(mac, BWN_TSF_CFP_START, 0xaaaa);
		BWN_WRITE_4(mac, BWN_TSF_CFP_START, 0xccccbbbb);
		if (BWN_READ_2(mac, BWN_TSF_CFP_START_LOW) != 0xbbbb)
			goto error;
		if (BWN_READ_2(mac, BWN_TSF_CFP_START_HIGH) != 0xcccc)
			goto error;
	}
	BWN_WRITE_4(mac, BWN_TSF_CFP_START, 0);

	v = BWN_READ_4(mac, BWN_MACCTL) | BWN_MACCTL_GMODE;
	if (v != (BWN_MACCTL_GMODE | BWN_MACCTL_IHR_ON))
		goto error;

	BWN_UNLOCK(sc);
	return (0);
error:
	BWN_UNLOCK(sc);
	device_printf(sc->sc_dev, "failed to validate the chipaccess\n");
	return (ENODEV);
}

#define	IEEE80211_CHAN_HTG	(IEEE80211_CHAN_HT | IEEE80211_CHAN_G)
#define	IEEE80211_CHAN_HTA	(IEEE80211_CHAN_HT | IEEE80211_CHAN_A)

static int
bwn_setup_channels(struct bwn_mac *mac, int have_bg, int have_a)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	memset(ic->ic_channels, 0, sizeof(ic->ic_channels));
	ic->ic_nchans = 0;

	if (have_bg)
		bwn_addchannels(ic->ic_channels, IEEE80211_CHAN_MAX,
		    &ic->ic_nchans, &bwn_chantable_bg, IEEE80211_CHAN_G);
	if (mac->mac_phy.type == BWN_PHYTYPE_N) {
		if (have_a)
			bwn_addchannels(ic->ic_channels, IEEE80211_CHAN_MAX,
			    &ic->ic_nchans, &bwn_chantable_n,
			    IEEE80211_CHAN_HTA);
	} else {
		if (have_a)
			bwn_addchannels(ic->ic_channels, IEEE80211_CHAN_MAX,
			    &ic->ic_nchans, &bwn_chantable_a,
			    IEEE80211_CHAN_A);
	}

	mac->mac_phy.supports_2ghz = have_bg;
	mac->mac_phy.supports_5ghz = have_a;

	return (ic->ic_nchans == 0 ? ENXIO : 0);
}

static uint32_t
bwn_shm_read_4(struct bwn_mac *mac, uint16_t way, uint16_t offset)
{
	uint32_t ret;

	BWN_ASSERT_LOCKED(mac->mac_sc);

	if (way == BWN_SHARED) {
		KASSERT((offset & 0x0001) == 0,
		    ("%s:%d warn", __func__, __LINE__));
		if (offset & 0x0003) {
			bwn_shm_ctlword(mac, way, offset >> 2);
			ret = BWN_READ_2(mac, BWN_SHM_DATA_UNALIGNED);
			ret <<= 16;
			bwn_shm_ctlword(mac, way, (offset >> 2) + 1);
			ret |= BWN_READ_2(mac, BWN_SHM_DATA);
			goto out;
		}
		offset >>= 2;
	}
	bwn_shm_ctlword(mac, way, offset);
	ret = BWN_READ_4(mac, BWN_SHM_DATA);
out:
	return (ret);
}

static uint16_t
bwn_shm_read_2(struct bwn_mac *mac, uint16_t way, uint16_t offset)
{
	uint16_t ret;

	BWN_ASSERT_LOCKED(mac->mac_sc);

	if (way == BWN_SHARED) {
		KASSERT((offset & 0x0001) == 0,
		    ("%s:%d warn", __func__, __LINE__));
		if (offset & 0x0003) {
			bwn_shm_ctlword(mac, way, offset >> 2);
			ret = BWN_READ_2(mac, BWN_SHM_DATA_UNALIGNED);
			goto out;
		}
		offset >>= 2;
	}
	bwn_shm_ctlword(mac, way, offset);
	ret = BWN_READ_2(mac, BWN_SHM_DATA);
out:

	return (ret);
}

static void
bwn_shm_ctlword(struct bwn_mac *mac, uint16_t way,
    uint16_t offset)
{
	uint32_t control;

	control = way;
	control <<= 16;
	control |= offset;
	BWN_WRITE_4(mac, BWN_SHM_CONTROL, control);
}

static void
bwn_shm_write_4(struct bwn_mac *mac, uint16_t way, uint16_t offset,
    uint32_t value)
{
	BWN_ASSERT_LOCKED(mac->mac_sc);

	if (way == BWN_SHARED) {
		KASSERT((offset & 0x0001) == 0,
		    ("%s:%d warn", __func__, __LINE__));
		if (offset & 0x0003) {
			bwn_shm_ctlword(mac, way, offset >> 2);
			BWN_WRITE_2(mac, BWN_SHM_DATA_UNALIGNED,
				    (value >> 16) & 0xffff);
			bwn_shm_ctlword(mac, way, (offset >> 2) + 1);
			BWN_WRITE_2(mac, BWN_SHM_DATA, value & 0xffff);
			return;
		}
		offset >>= 2;
	}
	bwn_shm_ctlword(mac, way, offset);
	BWN_WRITE_4(mac, BWN_SHM_DATA, value);
}

static void
bwn_shm_write_2(struct bwn_mac *mac, uint16_t way, uint16_t offset,
    uint16_t value)
{
	BWN_ASSERT_LOCKED(mac->mac_sc);

	if (way == BWN_SHARED) {
		KASSERT((offset & 0x0001) == 0,
		    ("%s:%d warn", __func__, __LINE__));
		if (offset & 0x0003) {
			bwn_shm_ctlword(mac, way, offset >> 2);
			BWN_WRITE_2(mac, BWN_SHM_DATA_UNALIGNED, value);
			return;
		}
		offset >>= 2;
	}
	bwn_shm_ctlword(mac, way, offset);
	BWN_WRITE_2(mac, BWN_SHM_DATA, value);
}

static void
bwn_addchan(struct ieee80211_channel *c, int freq, int flags, int ieee,
    int txpow)
{

	c->ic_freq = freq;
	c->ic_flags = flags;
	c->ic_ieee = ieee;
	c->ic_minpower = 0;
	c->ic_maxpower = 2 * txpow;
	c->ic_maxregpower = txpow;
}

static void
bwn_addchannels(struct ieee80211_channel chans[], int maxchans, int *nchans,
    const struct bwn_channelinfo *ci, int flags)
{
	struct ieee80211_channel *c;
	int i;

	c = &chans[*nchans];

	for (i = 0; i < ci->nchannels; i++) {
		const struct bwn_channel *hc;

		hc = &ci->channels[i];
		if (*nchans >= maxchans)
			break;
		bwn_addchan(c, hc->freq, flags, hc->ieee, hc->maxTxPow);
		c++, (*nchans)++;
		if (flags == IEEE80211_CHAN_G || flags == IEEE80211_CHAN_HTG) {
			/* g channel have a separate b-only entry */
			if (*nchans >= maxchans)
				break;
			c[0] = c[-1];
			c[-1].ic_flags = IEEE80211_CHAN_B;
			c++, (*nchans)++;
		}
		if (flags == IEEE80211_CHAN_HTG) {
			/* HT g channel have a separate g-only entry */
			if (*nchans >= maxchans)
				break;
			c[-1].ic_flags = IEEE80211_CHAN_G;
			c[0] = c[-1];
			c[0].ic_flags &= ~IEEE80211_CHAN_HT;
			c[0].ic_flags |= IEEE80211_CHAN_HT20;	/* HT20 */
			c++, (*nchans)++;
		}
		if (flags == IEEE80211_CHAN_HTA) {
			/* HT a channel have a separate a-only entry */
			if (*nchans >= maxchans)
				break;
			c[-1].ic_flags = IEEE80211_CHAN_A;
			c[0] = c[-1];
			c[0].ic_flags &= ~IEEE80211_CHAN_HT;
			c[0].ic_flags |= IEEE80211_CHAN_HT20;	/* HT20 */
			c++, (*nchans)++;
		}
	}
}

static int
bwn_phy_g_attach(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	unsigned int i;
	int16_t pab0, pab1, pab2;
	static int8_t bwn_phy_g_tssi2dbm_table[] = BWN_PHY_G_TSSI2DBM_TABLE;
	int8_t bg;

	bg = (int8_t)siba_sprom_get_tssi_bg(sc->sc_dev);
	pab0 = (int16_t)siba_sprom_get_pa0b0(sc->sc_dev);
	pab1 = (int16_t)siba_sprom_get_pa0b1(sc->sc_dev);
	pab2 = (int16_t)siba_sprom_get_pa0b2(sc->sc_dev);

	if ((siba_get_chipid(sc->sc_dev) == 0x4301) && (phy->rf_ver != 0x2050))
		device_printf(sc->sc_dev, "not supported anymore\n");

	pg->pg_flags = 0;
	if (pab0 == 0 || pab1 == 0 || pab2 == 0 || pab0 == -1 || pab1 == -1 ||
	    pab2 == -1) {
		pg->pg_idletssi = 52;
		pg->pg_tssi2dbm = bwn_phy_g_tssi2dbm_table;
		return (0);
	}

	pg->pg_idletssi = (bg == 0 || bg == -1) ? 62 : bg;
	pg->pg_tssi2dbm = (uint8_t *)malloc(64, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (pg->pg_tssi2dbm == NULL) {
		device_printf(sc->sc_dev, "failed to allocate buffer\n");
		return (ENOMEM);
	}
	for (i = 0; i < 64; i++) {
		int32_t m1, m2, f, q, delta;
		int8_t j = 0;

		m1 = BWN_TSSI2DBM(16 * pab0 + i * pab1, 32);
		m2 = MAX(BWN_TSSI2DBM(32768 + i * pab2, 256), 1);
		f = 256;

		do {
			if (j > 15) {
				device_printf(sc->sc_dev,
				    "failed to generate tssi2dBm\n");
				free(pg->pg_tssi2dbm, M_DEVBUF);
				return (ENOMEM);
			}
			q = BWN_TSSI2DBM(f * 4096 - BWN_TSSI2DBM(m2 * f, 16) *
			    f, 2048);
			delta = abs(q - f);
			f = q;
			j++;
		} while (delta >= 2);

		pg->pg_tssi2dbm[i] = MIN(MAX(BWN_TSSI2DBM(m1 * f, 8192), -127),
		    128);
	}

	pg->pg_flags |= BWN_PHY_G_FLAG_TSSITABLE_ALLOC;
	return (0);
}

static void
bwn_phy_g_detach(struct bwn_mac *mac)
{
	struct bwn_phy_g *pg = &mac->mac_phy.phy_g;

	if (pg->pg_flags & BWN_PHY_G_FLAG_TSSITABLE_ALLOC) {
		free(pg->pg_tssi2dbm, M_DEVBUF);
		pg->pg_tssi2dbm = NULL;
	}
	pg->pg_flags = 0;
}

static void
bwn_phy_g_init_pre(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	void *tssi2dbm;
	int idletssi;
	unsigned int i;

	tssi2dbm = pg->pg_tssi2dbm;
	idletssi = pg->pg_idletssi;

	memset(pg, 0, sizeof(*pg));

	pg->pg_tssi2dbm = tssi2dbm;
	pg->pg_idletssi = idletssi;

	memset(pg->pg_minlowsig, 0xff, sizeof(pg->pg_minlowsig));

	for (i = 0; i < N(pg->pg_nrssi); i++)
		pg->pg_nrssi[i] = -1000;
	for (i = 0; i < N(pg->pg_nrssi_lt); i++)
		pg->pg_nrssi_lt[i] = i;
	pg->pg_lofcal = 0xffff;
	pg->pg_initval = 0xffff;
	pg->pg_immode = BWN_IMMODE_NONE;
	pg->pg_ofdmtab_dir = BWN_OFDMTAB_DIR_UNKNOWN;
	pg->pg_avgtssi = 0xff;

	pg->pg_loctl.tx_bias = 0xff;
	TAILQ_INIT(&pg->pg_loctl.calib_list);
}

static int
bwn_phy_g_prepare_hw(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_txpwr_loctl *lo = &pg->pg_loctl;
	static const struct bwn_rfatt rfatt0[] = {
		{ 3, 0 }, { 1, 0 }, { 5, 0 }, { 7, 0 },	{ 9, 0 }, { 2, 0 },
		{ 0, 0 }, { 4, 0 }, { 6, 0 }, { 8, 0 }, { 1, 1 }, { 2, 1 },
		{ 3, 1 }, { 4, 1 }
	};
	static const struct bwn_rfatt rfatt1[] = {
		{ 2, 1 }, { 4, 1 }, { 6, 1 }, { 8, 1 }, { 10, 1 }, { 12, 1 },
		{ 14, 1 }
	};
	static const struct bwn_rfatt rfatt2[] = {
		{ 0, 1 }, { 2, 1 }, { 4, 1 }, { 6, 1 }, { 8, 1 }, { 9, 1 },
		{ 9, 1 }
	};
	static const struct bwn_bbatt bbatt_0[] = {
		{ 0 }, { 1 }, { 2 }, { 3 }, { 4 }, { 5 }, { 6 }, { 7 }, { 8 }
	};

	KASSERT(phy->type == BWN_PHYTYPE_G, ("%s fail", __func__));

	if (phy->rf_ver == 0x2050 && phy->rf_rev < 6)
		pg->pg_bbatt.att = 0;
	else
		pg->pg_bbatt.att = 2;

	/* prepare Radio Attenuation */
	pg->pg_rfatt.padmix = 0;

	if (siba_get_pci_subvendor(sc->sc_dev) == SIBA_BOARDVENDOR_BCM &&
	    siba_get_pci_subdevice(sc->sc_dev) == SIBA_BOARD_BCM4309G) {
		if (siba_get_pci_revid(sc->sc_dev) < 0x43) {
			pg->pg_rfatt.att = 2;
			goto done;
		} else if (siba_get_pci_revid(sc->sc_dev) < 0x51) {
			pg->pg_rfatt.att = 3;
			goto done;
		}
	}

	if (phy->type == BWN_PHYTYPE_A) {
		pg->pg_rfatt.att = 0x60;
		goto done;
	}

	switch (phy->rf_ver) {
	case 0x2050:
		switch (phy->rf_rev) {
		case 0:
			pg->pg_rfatt.att = 5;
			goto done;
		case 1:
			if (phy->type == BWN_PHYTYPE_G) {
				if (siba_get_pci_subvendor(sc->sc_dev) ==
				    SIBA_BOARDVENDOR_BCM &&
				    siba_get_pci_subdevice(sc->sc_dev) ==
				    SIBA_BOARD_BCM4309G &&
				    siba_get_pci_revid(sc->sc_dev) >= 30)
					pg->pg_rfatt.att = 3;
				else if (siba_get_pci_subvendor(sc->sc_dev) ==
				    SIBA_BOARDVENDOR_BCM &&
				    siba_get_pci_subdevice(sc->sc_dev) ==
				    SIBA_BOARD_BU4306)
					pg->pg_rfatt.att = 3;
				else
					pg->pg_rfatt.att = 1;
			} else {
				if (siba_get_pci_subvendor(sc->sc_dev) ==
				    SIBA_BOARDVENDOR_BCM &&
				    siba_get_pci_subdevice(sc->sc_dev) ==
				    SIBA_BOARD_BCM4309G &&
				    siba_get_pci_revid(sc->sc_dev) >= 30)
					pg->pg_rfatt.att = 7;
				else
					pg->pg_rfatt.att = 6;
			}
			goto done;
		case 2:
			if (phy->type == BWN_PHYTYPE_G) {
				if (siba_get_pci_subvendor(sc->sc_dev) ==
				    SIBA_BOARDVENDOR_BCM &&
				    siba_get_pci_subdevice(sc->sc_dev) ==
				    SIBA_BOARD_BCM4309G &&
				    siba_get_pci_revid(sc->sc_dev) >= 30)
					pg->pg_rfatt.att = 3;
				else if (siba_get_pci_subvendor(sc->sc_dev) ==
				    SIBA_BOARDVENDOR_BCM &&
				    siba_get_pci_subdevice(sc->sc_dev) ==
				    SIBA_BOARD_BU4306)
					pg->pg_rfatt.att = 5;
				else if (siba_get_chipid(sc->sc_dev) == 0x4320)
					pg->pg_rfatt.att = 4;
				else
					pg->pg_rfatt.att = 3;
			} else
				pg->pg_rfatt.att = 6;
			goto done;
		case 3:
			pg->pg_rfatt.att = 5;
			goto done;
		case 4:
		case 5:
			pg->pg_rfatt.att = 1;
			goto done;
		case 6:
		case 7:
			pg->pg_rfatt.att = 5;
			goto done;
		case 8:
			pg->pg_rfatt.att = 0xa;
			pg->pg_rfatt.padmix = 1;
			goto done;
		case 9:
		default:
			pg->pg_rfatt.att = 5;
			goto done;
		}
		break;
	case 0x2053:
		switch (phy->rf_rev) {
		case 1:
			pg->pg_rfatt.att = 6;
			goto done;
		}
		break;
	}
	pg->pg_rfatt.att = 5;
done:
	pg->pg_txctl = (bwn_phy_g_txctl(mac) << 4);

	if (!bwn_has_hwpctl(mac)) {
		lo->rfatt.array = rfatt0;
		lo->rfatt.len = N(rfatt0);
		lo->rfatt.min = 0;
		lo->rfatt.max = 9;
		goto genbbatt;
	}
	if (phy->rf_ver == 0x2050 && phy->rf_rev == 8) {
		lo->rfatt.array = rfatt1;
		lo->rfatt.len = N(rfatt1);
		lo->rfatt.min = 0;
		lo->rfatt.max = 14;
		goto genbbatt;
	}
	lo->rfatt.array = rfatt2;
	lo->rfatt.len = N(rfatt2);
	lo->rfatt.min = 0;
	lo->rfatt.max = 9;
genbbatt:
	lo->bbatt.array = bbatt_0;
	lo->bbatt.len = N(bbatt_0);
	lo->bbatt.min = 0;
	lo->bbatt.max = 8;

	BWN_READ_4(mac, BWN_MACCTL);
	if (phy->rev == 1) {
		phy->gmode = 0;
		bwn_reset_core(mac, 0);
		bwn_phy_g_init_sub(mac);
		phy->gmode = 1;
		bwn_reset_core(mac, BWN_TGSLOW_SUPPORT_G);
	}
	return (0);
}

static uint16_t
bwn_phy_g_txctl(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;

	if (phy->rf_ver != 0x2050)
		return (0);
	if (phy->rf_rev == 1)
		return (BWN_TXCTL_PA2DB | BWN_TXCTL_TXMIX);
	if (phy->rf_rev < 6)
		return (BWN_TXCTL_PA2DB);
	if (phy->rf_rev == 8)
		return (BWN_TXCTL_TXMIX);
	return (0);
}

static int
bwn_phy_g_init(struct bwn_mac *mac)
{

	bwn_phy_g_init_sub(mac);
	return (0);
}

static void
bwn_phy_g_exit(struct bwn_mac *mac)
{
	struct bwn_txpwr_loctl *lo = &mac->mac_phy.phy_g.pg_loctl;
	struct bwn_lo_calib *cal, *tmp;

	if (lo == NULL)
		return;
	TAILQ_FOREACH_SAFE(cal, &lo->calib_list, list, tmp) {
		TAILQ_REMOVE(&lo->calib_list, cal, list);
		free(cal, M_DEVBUF);
	}
}

static uint16_t
bwn_phy_g_read(struct bwn_mac *mac, uint16_t reg)
{

	BWN_WRITE_2(mac, BWN_PHYCTL, reg);
	return (BWN_READ_2(mac, BWN_PHYDATA));
}

static void
bwn_phy_g_write(struct bwn_mac *mac, uint16_t reg, uint16_t value)
{

	BWN_WRITE_2(mac, BWN_PHYCTL, reg);
	BWN_WRITE_2(mac, BWN_PHYDATA, value);
}

static uint16_t
bwn_phy_g_rf_read(struct bwn_mac *mac, uint16_t reg)
{

	KASSERT(reg != 1, ("%s:%d: fail", __func__, __LINE__));
	BWN_WRITE_2(mac, BWN_RFCTL, reg | 0x80);
	return (BWN_READ_2(mac, BWN_RFDATALO));
}

static void
bwn_phy_g_rf_write(struct bwn_mac *mac, uint16_t reg, uint16_t value)
{

	KASSERT(reg != 1, ("%s:%d: fail", __func__, __LINE__));
	BWN_WRITE_2(mac, BWN_RFCTL, reg);
	BWN_WRITE_2(mac, BWN_RFDATALO, value);
}

static int
bwn_phy_g_hwpctl(struct bwn_mac *mac)
{

	return (mac->mac_phy.rev >= 6);
}

static void
bwn_phy_g_rf_onoff(struct bwn_mac *mac, int on)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	unsigned int channel;
	uint16_t rfover, rfoverval;

	if (on) {
		if (phy->rf_on)
			return;

		BWN_PHY_WRITE(mac, 0x15, 0x8000);
		BWN_PHY_WRITE(mac, 0x15, 0xcc00);
		BWN_PHY_WRITE(mac, 0x15, (phy->gmode ? 0xc0 : 0x0));
		if (pg->pg_flags & BWN_PHY_G_FLAG_RADIOCTX_VALID) {
			BWN_PHY_WRITE(mac, BWN_PHY_RFOVER,
			    pg->pg_radioctx_over);
			BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
			    pg->pg_radioctx_overval);
			pg->pg_flags &= ~BWN_PHY_G_FLAG_RADIOCTX_VALID;
		}
		channel = phy->chan;
		bwn_phy_g_switch_chan(mac, 6, 1);
		bwn_phy_g_switch_chan(mac, channel, 0);
		return;
	}

	rfover = BWN_PHY_READ(mac, BWN_PHY_RFOVER);
	rfoverval = BWN_PHY_READ(mac, BWN_PHY_RFOVERVAL);
	pg->pg_radioctx_over = rfover;
	pg->pg_radioctx_overval = rfoverval;
	pg->pg_flags |= BWN_PHY_G_FLAG_RADIOCTX_VALID;
	BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, rfover | 0x008c);
	BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, rfoverval & 0xff73);
}

static int
bwn_phy_g_switch_channel(struct bwn_mac *mac, uint32_t newchan)
{

	if ((newchan < 1) || (newchan > 14))
		return (EINVAL);
	bwn_phy_g_switch_chan(mac, newchan, 0);

	return (0);
}

static uint32_t
bwn_phy_g_get_default_chan(struct bwn_mac *mac)
{

	return (1);
}

static void
bwn_phy_g_set_antenna(struct bwn_mac *mac, int antenna)
{
	struct bwn_phy *phy = &mac->mac_phy;
	uint64_t hf;
	int autodiv = 0;
	uint16_t tmp;

	if (antenna == BWN_ANTAUTO0 || antenna == BWN_ANTAUTO1)
		autodiv = 1;

	hf = bwn_hf_read(mac) & ~BWN_HF_UCODE_ANTDIV_HELPER;
	bwn_hf_write(mac, hf);

	BWN_PHY_WRITE(mac, BWN_PHY_BBANDCFG,
	    (BWN_PHY_READ(mac, BWN_PHY_BBANDCFG) & ~BWN_PHY_BBANDCFG_RXANT) |
	    ((autodiv ? BWN_ANTAUTO1 : antenna)
		<< BWN_PHY_BBANDCFG_RXANT_SHIFT));

	if (autodiv) {
		tmp = BWN_PHY_READ(mac, BWN_PHY_ANTDWELL);
		if (antenna == BWN_ANTAUTO1)
			tmp &= ~BWN_PHY_ANTDWELL_AUTODIV1;
		else
			tmp |= BWN_PHY_ANTDWELL_AUTODIV1;
		BWN_PHY_WRITE(mac, BWN_PHY_ANTDWELL, tmp);
	}
	tmp = BWN_PHY_READ(mac, BWN_PHY_ANTWRSETT);
	if (autodiv)
		tmp |= BWN_PHY_ANTWRSETT_ARXDIV;
	else
		tmp &= ~BWN_PHY_ANTWRSETT_ARXDIV;
	BWN_PHY_WRITE(mac, BWN_PHY_ANTWRSETT, tmp);
	if (phy->rev >= 2) {
		BWN_PHY_WRITE(mac, BWN_PHY_OFDM61,
		    BWN_PHY_READ(mac, BWN_PHY_OFDM61) | BWN_PHY_OFDM61_10);
		BWN_PHY_WRITE(mac, BWN_PHY_DIVSRCHGAINBACK,
		    (BWN_PHY_READ(mac, BWN_PHY_DIVSRCHGAINBACK) & 0xff00) |
		    0x15);
		if (phy->rev == 2)
			BWN_PHY_WRITE(mac, BWN_PHY_ADIVRELATED, 8);
		else
			BWN_PHY_WRITE(mac, BWN_PHY_ADIVRELATED,
			    (BWN_PHY_READ(mac, BWN_PHY_ADIVRELATED) & 0xff00) |
			    8);
	}
	if (phy->rev >= 6)
		BWN_PHY_WRITE(mac, BWN_PHY_OFDM9B, 0xdc);

	hf |= BWN_HF_UCODE_ANTDIV_HELPER;
	bwn_hf_write(mac, hf);
}

static int
bwn_phy_g_im(struct bwn_mac *mac, int mode)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;

	KASSERT(phy->type == BWN_PHYTYPE_G, ("%s: fail", __func__));
	KASSERT(mode == BWN_IMMODE_NONE, ("%s: fail", __func__));

	if (phy->rev == 0 || !phy->gmode)
		return (ENODEV);

	pg->pg_aci_wlan_automatic = 0;
	return (0);
}

static int
bwn_phy_g_recalc_txpwr(struct bwn_mac *mac, int ignore_tssi)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	unsigned int tssi;
	int cck, ofdm;
	int power;
	int rfatt, bbatt;
	unsigned int max;

	KASSERT(phy->type == BWN_PHYTYPE_G, ("%s: fail", __func__));

	cck = bwn_phy_shm_tssi_read(mac, BWN_SHARED_TSSI_CCK);
	ofdm = bwn_phy_shm_tssi_read(mac, BWN_SHARED_TSSI_OFDM_G);
	if (cck < 0 && ofdm < 0) {
		if (ignore_tssi == 0)
			return (BWN_TXPWR_RES_DONE);
		cck = 0;
		ofdm = 0;
	}
	tssi = (cck < 0) ? ofdm : ((ofdm < 0) ? cck : (cck + ofdm) / 2);
	if (pg->pg_avgtssi != 0xff)
		tssi = (tssi + pg->pg_avgtssi) / 2;
	pg->pg_avgtssi = tssi;
	KASSERT(tssi < BWN_TSSI_MAX, ("%s:%d: fail", __func__, __LINE__));

	max = siba_sprom_get_maxpwr_bg(sc->sc_dev);
	if (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_PACTRL)
		max -= 3;
	if (max >= 120) {
		device_printf(sc->sc_dev, "invalid max TX-power value\n");
		max = 80;
		siba_sprom_set_maxpwr_bg(sc->sc_dev, max);
	}

	power = MIN(MAX((phy->txpower < 0) ? 0 : (phy->txpower << 2), 0), max) -
	    (pg->pg_tssi2dbm[MIN(MAX(pg->pg_idletssi - pg->pg_curtssi +
	     tssi, 0x00), 0x3f)]);
	if (power == 0)
		return (BWN_TXPWR_RES_DONE);

	rfatt = -((power + 7) / 8);
	bbatt = (-(power / 2)) - (4 * rfatt);
	if ((rfatt == 0) && (bbatt == 0))
		return (BWN_TXPWR_RES_DONE);
	pg->pg_bbatt_delta = bbatt;
	pg->pg_rfatt_delta = rfatt;
	return (BWN_TXPWR_RES_NEED_ADJUST);
}

static void
bwn_phy_g_set_txpwr(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	int rfatt, bbatt;
	uint8_t txctl;

	bwn_mac_suspend(mac);

	BWN_ASSERT_LOCKED(sc);

	bbatt = pg->pg_bbatt.att;
	bbatt += pg->pg_bbatt_delta;
	rfatt = pg->pg_rfatt.att;
	rfatt += pg->pg_rfatt_delta;

	bwn_phy_g_setatt(mac, &bbatt, &rfatt);
	txctl = pg->pg_txctl;
	if ((phy->rf_ver == 0x2050) && (phy->rf_rev == 2)) {
		if (rfatt <= 1) {
			if (txctl == 0) {
				txctl = BWN_TXCTL_PA2DB | BWN_TXCTL_TXMIX;
				rfatt += 2;
				bbatt += 2;
			} else if (siba_sprom_get_bf_lo(sc->sc_dev) &
			    BWN_BFL_PACTRL) {
				bbatt += 4 * (rfatt - 2);
				rfatt = 2;
			}
		} else if (rfatt > 4 && txctl) {
			txctl = 0;
			if (bbatt < 3) {
				rfatt -= 3;
				bbatt += 2;
			} else {
				rfatt -= 2;
				bbatt -= 2;
			}
		}
	}
	pg->pg_txctl = txctl;
	bwn_phy_g_setatt(mac, &bbatt, &rfatt);
	pg->pg_rfatt.att = rfatt;
	pg->pg_bbatt.att = bbatt;

	DPRINTF(sc, BWN_DEBUG_TXPOW, "%s: adjust TX power\n", __func__);

	bwn_phy_lock(mac);
	bwn_rf_lock(mac);
	bwn_phy_g_set_txpwr_sub(mac, &pg->pg_bbatt, &pg->pg_rfatt,
	    pg->pg_txctl);
	bwn_rf_unlock(mac);
	bwn_phy_unlock(mac);

	bwn_mac_enable(mac);
}

static void
bwn_phy_g_task_15s(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_txpwr_loctl *lo = &pg->pg_loctl;
	unsigned long expire, now;
	struct bwn_lo_calib *cal, *tmp;
	uint8_t expired = 0;

	bwn_mac_suspend(mac);

	if (lo == NULL)
		goto fail;

	BWN_GETTIME(now);
	if (bwn_has_hwpctl(mac)) {
		expire = now - BWN_LO_PWRVEC_EXPIRE;
		if (time_before(lo->pwr_vec_read_time, expire)) {
			bwn_lo_get_powervector(mac);
			bwn_phy_g_dc_lookup_init(mac, 0);
		}
		goto fail;
	}

	expire = now - BWN_LO_CALIB_EXPIRE;
	TAILQ_FOREACH_SAFE(cal, &lo->calib_list, list, tmp) {
		if (!time_before(cal->calib_time, expire))
			continue;
		if (BWN_BBATTCMP(&cal->bbatt, &pg->pg_bbatt) &&
		    BWN_RFATTCMP(&cal->rfatt, &pg->pg_rfatt)) {
			KASSERT(!expired, ("%s:%d: fail", __func__, __LINE__));
			expired = 1;
		}

		DPRINTF(sc, BWN_DEBUG_LO, "expired BB %u RF %u %u I %d Q %d\n",
		    cal->bbatt.att, cal->rfatt.att, cal->rfatt.padmix,
		    cal->ctl.i, cal->ctl.q);

		TAILQ_REMOVE(&lo->calib_list, cal, list);
		free(cal, M_DEVBUF);
	}
	if (expired || TAILQ_EMPTY(&lo->calib_list)) {
		cal = bwn_lo_calibset(mac, &pg->pg_bbatt,
		    &pg->pg_rfatt);
		if (cal == NULL) {
			device_printf(sc->sc_dev,
			    "failed to recalibrate LO\n");
			goto fail;
		}
		TAILQ_INSERT_TAIL(&lo->calib_list, cal, list);
		bwn_lo_write(mac, &cal->ctl);
	}

fail:
	bwn_mac_enable(mac);
}

static void
bwn_phy_g_task_60s(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_softc *sc = mac->mac_sc;
	uint8_t old = phy->chan;

	if (!(siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_RSSI))
		return;

	bwn_mac_suspend(mac);
	bwn_nrssi_slope_11g(mac);
	if ((phy->rf_ver == 0x2050) && (phy->rf_rev == 8)) {
		bwn_switch_channel(mac, (old >= 8) ? 1 : 13);
		bwn_switch_channel(mac, old);
	}
	bwn_mac_enable(mac);
}

static void
bwn_phy_switch_analog(struct bwn_mac *mac, int on)
{

	BWN_WRITE_2(mac, BWN_PHY0, on ? 0 : 0xf4);
}

static int
bwn_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ifnet *ifp = ic->ic_ifp;
	struct bwn_softc *sc = ifp->if_softc;
	struct bwn_mac *mac = sc->sc_curmac;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    mac->mac_status < BWN_MAC_STATUS_STARTED) {
		ieee80211_free_node(ni);
		m_freem(m);
		return (ENETDOWN);
	}

	BWN_LOCK(sc);
	if (bwn_tx_isfull(sc, m)) {
		ieee80211_free_node(ni);
		m_freem(m);
		ifp->if_oerrors++;
		BWN_UNLOCK(sc);
		return (ENOBUFS);
	}

	if (bwn_tx_start(sc, ni, m) != 0) {
		if (ni != NULL)
			ieee80211_free_node(ni);
		ifp->if_oerrors++;
	}
	sc->sc_watchdog_timer = 5;
	BWN_UNLOCK(sc);
	return (0);
}

/*
 * Callback from the 802.11 layer to update the slot time
 * based on the current setting.  We use it to notify the
 * firmware of ERP changes and the f/w takes care of things
 * like slot time and preamble.
 */
static void
bwn_updateslot(struct ifnet *ifp)
{
	struct bwn_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = ifp->if_l2com;
	struct bwn_mac *mac;

	BWN_LOCK(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		mac = (struct bwn_mac *)sc->sc_curmac;
		bwn_set_slot_time(mac,
		    (ic->ic_flags & IEEE80211_F_SHSLOT) ? 9 : 20);
	}
	BWN_UNLOCK(sc);
}

/*
 * Callback from the 802.11 layer after a promiscuous mode change.
 * Note this interface does not check the operating mode as this
 * is an internal callback and we are expected to honor the current
 * state (e.g. this is used for setting the interface in promiscuous
 * mode when operating in hostap mode to do ACS).
 */
static void
bwn_update_promisc(struct ifnet *ifp)
{
	struct bwn_softc *sc = ifp->if_softc;
	struct bwn_mac *mac = sc->sc_curmac;

	BWN_LOCK(sc);
	mac = sc->sc_curmac;
	if (mac != NULL && mac->mac_status >= BWN_MAC_STATUS_INITED) {
		if (ifp->if_flags & IFF_PROMISC)
			sc->sc_filters |= BWN_MACCTL_PROMISC;
		else
			sc->sc_filters &= ~BWN_MACCTL_PROMISC;
		bwn_set_opmode(mac);
	}
	BWN_UNLOCK(sc);
}

/*
 * Callback from the 802.11 layer to update WME parameters.
 */
static int
bwn_wme_update(struct ieee80211com *ic)
{
	struct bwn_softc *sc = ic->ic_ifp->if_softc;
	struct bwn_mac *mac = sc->sc_curmac;
	struct wmeParams *wmep;
	int i;

	BWN_LOCK(sc);
	mac = sc->sc_curmac;
	if (mac != NULL && mac->mac_status >= BWN_MAC_STATUS_INITED) {
		bwn_mac_suspend(mac);
		for (i = 0; i < N(sc->sc_wmeParams); i++) {
			wmep = &ic->ic_wme.wme_chanParams.cap_wmeParams[i];
			bwn_wme_loadparams(mac, wmep, bwn_wme_shm_offsets[i]);
		}
		bwn_mac_enable(mac);
	}
	BWN_UNLOCK(sc);
	return (0);
}

static void
bwn_scan_start(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct bwn_softc *sc = ifp->if_softc;
	struct bwn_mac *mac;

	BWN_LOCK(sc);
	mac = sc->sc_curmac;
	if (mac != NULL && mac->mac_status >= BWN_MAC_STATUS_INITED) {
		sc->sc_filters |= BWN_MACCTL_BEACON_PROMISC;
		bwn_set_opmode(mac);
		/* disable CFP update during scan */
		bwn_hf_write(mac, bwn_hf_read(mac) | BWN_HF_SKIP_CFP_UPDATE);
	}
	BWN_UNLOCK(sc);
}

static void
bwn_scan_end(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct bwn_softc *sc = ifp->if_softc;
	struct bwn_mac *mac;

	BWN_LOCK(sc);
	mac = sc->sc_curmac;
	if (mac != NULL && mac->mac_status >= BWN_MAC_STATUS_INITED) {
		sc->sc_filters &= ~BWN_MACCTL_BEACON_PROMISC;
		bwn_set_opmode(mac);
		bwn_hf_write(mac, bwn_hf_read(mac) & ~BWN_HF_SKIP_CFP_UPDATE);
	}
	BWN_UNLOCK(sc);
}

static void
bwn_set_channel(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct bwn_softc *sc = ifp->if_softc;
	struct bwn_mac *mac = sc->sc_curmac;
	struct bwn_phy *phy = &mac->mac_phy;
	int chan, error;

	BWN_LOCK(sc);

	error = bwn_switch_band(sc, ic->ic_curchan);
	if (error)
		goto fail;
	bwn_mac_suspend(mac);
	bwn_set_txretry(mac, BWN_RETRY_SHORT, BWN_RETRY_LONG);
	chan = ieee80211_chan2ieee(ic, ic->ic_curchan);
	if (chan != phy->chan)
		bwn_switch_channel(mac, chan);

	/* TX power level */
	if (ic->ic_curchan->ic_maxpower != 0 &&
	    ic->ic_curchan->ic_maxpower != phy->txpower) {
		phy->txpower = ic->ic_curchan->ic_maxpower / 2;
		bwn_phy_txpower_check(mac, BWN_TXPWR_IGNORE_TIME |
		    BWN_TXPWR_IGNORE_TSSI);
	}

	bwn_set_txantenna(mac, BWN_ANT_DEFAULT);
	if (phy->set_antenna)
		phy->set_antenna(mac, BWN_ANT_DEFAULT);

	if (sc->sc_rf_enabled != phy->rf_on) {
		if (sc->sc_rf_enabled) {
			bwn_rf_turnon(mac);
			if (!(mac->mac_flags & BWN_MAC_FLAG_RADIO_ON))
				device_printf(sc->sc_dev,
				    "please turn on the RF switch\n");
		} else
			bwn_rf_turnoff(mac);
	}

	bwn_mac_enable(mac);

fail:
	/*
	 * Setup radio tap channel freq and flags
	 */
	sc->sc_tx_th.wt_chan_freq = sc->sc_rx_th.wr_chan_freq =
		htole16(ic->ic_curchan->ic_freq);
	sc->sc_tx_th.wt_chan_flags = sc->sc_rx_th.wr_chan_flags =
		htole16(ic->ic_curchan->ic_flags & 0xffff);

	BWN_UNLOCK(sc);
}

static struct ieee80211vap *
bwn_vap_create(struct ieee80211com *ic,
	const char name[IFNAMSIZ], int unit, int opmode, int flags,
	const uint8_t bssid[IEEE80211_ADDR_LEN],
	const uint8_t mac0[IEEE80211_ADDR_LEN])
{
	struct ifnet *ifp = ic->ic_ifp;
	struct bwn_softc *sc = ifp->if_softc;
	struct ieee80211vap *vap;
	struct bwn_vap *bvp;
	uint8_t mac[IEEE80211_ADDR_LEN];

	IEEE80211_ADDR_COPY(mac, mac0);
	switch (opmode) {
	case IEEE80211_M_HOSTAP:
	case IEEE80211_M_MBSS:
	case IEEE80211_M_STA:
	case IEEE80211_M_WDS:
	case IEEE80211_M_MONITOR:
	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		break;
	default:
		return (NULL);
	}

	IEEE80211_ADDR_COPY(sc->sc_macaddr, mac0);

	bvp = (struct bwn_vap *) malloc(sizeof(struct bwn_vap),
	    M_80211_VAP, M_NOWAIT | M_ZERO);
	if (bvp == NULL) {
		device_printf(sc->sc_dev, "failed to allocate a buffer\n");
		return (NULL);
	}
	vap = &bvp->bv_vap;
	ieee80211_vap_setup(ic, vap, name, unit, opmode, flags, bssid, mac);
	IEEE80211_ADDR_COPY(vap->iv_myaddr, mac);
	/* override with driver methods */
	bvp->bv_newstate = vap->iv_newstate;
	vap->iv_newstate = bwn_newstate;

	/* override max aid so sta's cannot assoc when we're out of sta id's */
	vap->iv_max_aid = BWN_STAID_MAX;

	ieee80211_ratectl_init(vap);

	/* complete setup */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status);
	return (vap);
}

static void
bwn_vap_delete(struct ieee80211vap *vap)
{
	struct bwn_vap *bvp = BWN_VAP(vap);

	ieee80211_ratectl_deinit(vap);
	ieee80211_vap_detach(vap);
	free(bvp, M_80211_VAP);
}

static void
bwn_init(void *arg)
{
	struct bwn_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	int error = 0;

	DPRINTF(sc, BWN_DEBUG_ANY, "%s: if_flags 0x%x\n",
		__func__, ifp->if_flags);

	BWN_LOCK(sc);
	error = bwn_init_locked(sc);
	BWN_UNLOCK(sc);

	if (error == 0)
		ieee80211_start_all(ic);	/* start all vap's */
}

static int
bwn_init_locked(struct bwn_softc *sc)
{
	struct bwn_mac *mac;
	struct ifnet *ifp = sc->sc_ifp;
	int error;

	BWN_ASSERT_LOCKED(sc);

	bzero(sc->sc_bssid, IEEE80211_ADDR_LEN);
	sc->sc_flags |= BWN_FLAG_NEED_BEACON_TP;
	sc->sc_filters = 0;
	bwn_wme_clear(sc);
	sc->sc_beacons[0] = sc->sc_beacons[1] = 0;
	sc->sc_rf_enabled = 1;

	mac = sc->sc_curmac;
	if (mac->mac_status == BWN_MAC_STATUS_UNINIT) {
		error = bwn_core_init(mac);
		if (error != 0)
			return (error);
	}
	if (mac->mac_status == BWN_MAC_STATUS_INITED)
		bwn_core_start(mac);

	bwn_set_opmode(mac);
	bwn_set_pretbtt(mac);
	bwn_spu_setdelay(mac, 0);
	bwn_set_macaddr(mac);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	callout_reset(&sc->sc_rfswitch_ch, hz, bwn_rfswitch, sc);
	callout_reset(&sc->sc_watchdog_ch, hz, bwn_watchdog, sc);

	return (0);
}

static void
bwn_stop(struct bwn_softc *sc, int statechg)
{

	BWN_LOCK(sc);
	bwn_stop_locked(sc, statechg);
	BWN_UNLOCK(sc);
}

static void
bwn_stop_locked(struct bwn_softc *sc, int statechg)
{
	struct bwn_mac *mac = sc->sc_curmac;
	struct ifnet *ifp = sc->sc_ifp;

	BWN_ASSERT_LOCKED(sc);

	if (mac->mac_status >= BWN_MAC_STATUS_INITED) {
		/* XXX FIXME opmode not based on VAP */
		bwn_set_opmode(mac);
		bwn_set_macaddr(mac);
	}

	if (mac->mac_status >= BWN_MAC_STATUS_STARTED)
		bwn_core_stop(mac);

	callout_stop(&sc->sc_led_blink_ch);
	sc->sc_led_blinking = 0;

	bwn_core_exit(mac);
	sc->sc_rf_enabled = 0;

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
}

static void
bwn_wme_clear(struct bwn_softc *sc)
{
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)
	struct wmeParams *p;
	unsigned int i;

	KASSERT(N(bwn_wme_shm_offsets) == N(sc->sc_wmeParams),
	    ("%s:%d: fail", __func__, __LINE__));

	for (i = 0; i < N(sc->sc_wmeParams); i++) {
		p = &(sc->sc_wmeParams[i]);

		switch (bwn_wme_shm_offsets[i]) {
		case BWN_WME_VOICE:
			p->wmep_txopLimit = 0;
			p->wmep_aifsn = 2;
			/* XXX FIXME: log2(cwmin) */
			p->wmep_logcwmin = MS(0x0001, WME_PARAM_LOGCWMIN);
			p->wmep_logcwmax = MS(0x0001, WME_PARAM_LOGCWMAX);
			break;
		case BWN_WME_VIDEO:
			p->wmep_txopLimit = 0;
			p->wmep_aifsn = 2;
			/* XXX FIXME: log2(cwmin) */
			p->wmep_logcwmin = MS(0x0001, WME_PARAM_LOGCWMIN);
			p->wmep_logcwmax = MS(0x0001, WME_PARAM_LOGCWMAX);
			break;
		case BWN_WME_BESTEFFORT:
			p->wmep_txopLimit = 0;
			p->wmep_aifsn = 3;
			/* XXX FIXME: log2(cwmin) */
			p->wmep_logcwmin = MS(0x0001, WME_PARAM_LOGCWMIN);
			p->wmep_logcwmax = MS(0x03ff, WME_PARAM_LOGCWMAX);
			break;
		case BWN_WME_BACKGROUND:
			p->wmep_txopLimit = 0;
			p->wmep_aifsn = 7;
			/* XXX FIXME: log2(cwmin) */
			p->wmep_logcwmin = MS(0x0001, WME_PARAM_LOGCWMIN);
			p->wmep_logcwmax = MS(0x03ff, WME_PARAM_LOGCWMAX);
			break;
		default:
			KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
		}
	}
}

static int
bwn_core_init(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint64_t hf;
	int error;

	KASSERT(mac->mac_status == BWN_MAC_STATUS_UNINIT,
	    ("%s:%d: fail", __func__, __LINE__));

	siba_powerup(sc->sc_dev, 0);
	if (!siba_dev_isup(sc->sc_dev))
		bwn_reset_core(mac,
		    mac->mac_phy.gmode ? BWN_TGSLOW_SUPPORT_G : 0);

	mac->mac_flags &= ~BWN_MAC_FLAG_DFQVALID;
	mac->mac_flags |= BWN_MAC_FLAG_RADIO_ON;
	mac->mac_phy.hwpctl = (bwn_hwpctl) ? 1 : 0;
	BWN_GETTIME(mac->mac_phy.nexttime);
	mac->mac_phy.txerrors = BWN_TXERROR_MAX;
	bzero(&mac->mac_stats, sizeof(mac->mac_stats));
	mac->mac_stats.link_noise = -95;
	mac->mac_reason_intr = 0;
	bzero(mac->mac_reason, sizeof(mac->mac_reason));
	mac->mac_intr_mask = BWN_INTR_MASKTEMPLATE;
#ifdef BWN_DEBUG
	if (sc->sc_debug & BWN_DEBUG_XMIT)
		mac->mac_intr_mask &= ~BWN_INTR_PHY_TXERR;
#endif
	mac->mac_suspended = 1;
	mac->mac_task_state = 0;
	memset(&mac->mac_noise, 0, sizeof(mac->mac_noise));

	mac->mac_phy.init_pre(mac);

	siba_pcicore_intr(sc->sc_dev);

	siba_fix_imcfglobug(sc->sc_dev);
	bwn_bt_disable(mac);
	if (mac->mac_phy.prepare_hw) {
		error = mac->mac_phy.prepare_hw(mac);
		if (error)
			goto fail0;
	}
	error = bwn_chip_init(mac);
	if (error)
		goto fail0;
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_COREREV,
	    siba_get_revid(sc->sc_dev));
	hf = bwn_hf_read(mac);
	if (mac->mac_phy.type == BWN_PHYTYPE_G) {
		hf |= BWN_HF_GPHY_SYM_WORKAROUND;
		if (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_PACTRL)
			hf |= BWN_HF_PAGAINBOOST_OFDM_ON;
		if (mac->mac_phy.rev == 1)
			hf |= BWN_HF_GPHY_DC_CANCELFILTER;
	}
	if (mac->mac_phy.rf_ver == 0x2050) {
		if (mac->mac_phy.rf_rev < 6)
			hf |= BWN_HF_FORCE_VCO_RECALC;
		if (mac->mac_phy.rf_rev == 6)
			hf |= BWN_HF_4318_TSSI;
	}
	if (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_CRYSTAL_NOSLOW)
		hf |= BWN_HF_SLOWCLOCK_REQ_OFF;
	if ((siba_get_type(sc->sc_dev) == SIBA_TYPE_PCI) &&
	    (siba_get_pcicore_revid(sc->sc_dev) <= 10))
		hf |= BWN_HF_PCI_SLOWCLOCK_WORKAROUND;
	hf &= ~BWN_HF_SKIP_CFP_UPDATE;
	bwn_hf_write(mac, hf);

	bwn_set_txretry(mac, BWN_RETRY_SHORT, BWN_RETRY_LONG);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_SHORT_RETRY_FALLBACK, 3);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_LONG_RETRY_FALLBACK, 2);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_PROBE_RESP_MAXTIME, 1);

	bwn_rate_init(mac);
	bwn_set_phytxctl(mac);

	bwn_shm_write_2(mac, BWN_SCRATCH, BWN_SCRATCH_CONT_MIN,
	    (mac->mac_phy.type == BWN_PHYTYPE_B) ? 0x1f : 0xf);
	bwn_shm_write_2(mac, BWN_SCRATCH, BWN_SCRATCH_CONT_MAX, 0x3ff);

	if (siba_get_type(sc->sc_dev) == SIBA_TYPE_PCMCIA || bwn_usedma == 0)
		bwn_pio_init(mac);
	else
		bwn_dma_init(mac);
	if (error)
		goto fail1;
	bwn_wme_init(mac);
	bwn_spu_setdelay(mac, 1);
	bwn_bt_enable(mac);

	siba_powerup(sc->sc_dev,
	    !(siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_CRYSTAL_NOSLOW));
	bwn_set_macaddr(mac);
	bwn_crypt_init(mac);

	/* XXX LED initializatin */

	mac->mac_status = BWN_MAC_STATUS_INITED;

	return (error);

fail1:
	bwn_chip_exit(mac);
fail0:
	siba_powerdown(sc->sc_dev);
	KASSERT(mac->mac_status == BWN_MAC_STATUS_UNINIT,
	    ("%s:%d: fail", __func__, __LINE__));
	return (error);
}

static void
bwn_core_start(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t tmp;

	KASSERT(mac->mac_status == BWN_MAC_STATUS_INITED,
	    ("%s:%d: fail", __func__, __LINE__));

	if (siba_get_revid(sc->sc_dev) < 5)
		return;

	while (1) {
		tmp = BWN_READ_4(mac, BWN_XMITSTAT_0);
		if (!(tmp & 0x00000001))
			break;
		tmp = BWN_READ_4(mac, BWN_XMITSTAT_1);
	}

	bwn_mac_enable(mac);
	BWN_WRITE_4(mac, BWN_INTR_MASK, mac->mac_intr_mask);
	callout_reset(&sc->sc_task_ch, hz * 15, bwn_tasks, mac);

	mac->mac_status = BWN_MAC_STATUS_STARTED;
}

static void
bwn_core_exit(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t macctl;

	BWN_ASSERT_LOCKED(mac->mac_sc);

	KASSERT(mac->mac_status <= BWN_MAC_STATUS_INITED,
	    ("%s:%d: fail", __func__, __LINE__));

	if (mac->mac_status != BWN_MAC_STATUS_INITED)
		return;
	mac->mac_status = BWN_MAC_STATUS_UNINIT;

	macctl = BWN_READ_4(mac, BWN_MACCTL);
	macctl &= ~BWN_MACCTL_MCODE_RUN;
	macctl |= BWN_MACCTL_MCODE_JMP0;
	BWN_WRITE_4(mac, BWN_MACCTL, macctl);

	bwn_dma_stop(mac);
	bwn_pio_stop(mac);
	bwn_chip_exit(mac);
	mac->mac_phy.switch_analog(mac, 0);
	siba_dev_down(sc->sc_dev, 0);
	siba_powerdown(sc->sc_dev);
}

static void
bwn_bt_disable(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;

	(void)sc;
	/* XXX do nothing yet */
}

static int
bwn_chip_init(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_phy *phy = &mac->mac_phy;
	uint32_t macctl;
	int error;

	macctl = BWN_MACCTL_IHR_ON | BWN_MACCTL_SHM_ON | BWN_MACCTL_STA;
	if (phy->gmode)
		macctl |= BWN_MACCTL_GMODE;
	BWN_WRITE_4(mac, BWN_MACCTL, macctl);

	error = bwn_fw_fillinfo(mac);
	if (error)
		return (error);
	error = bwn_fw_loaducode(mac);
	if (error)
		return (error);

	error = bwn_gpio_init(mac);
	if (error)
		return (error);

	error = bwn_fw_loadinitvals(mac);
	if (error) {
		siba_gpio_set(sc->sc_dev, 0);
		return (error);
	}
	phy->switch_analog(mac, 1);
	error = bwn_phy_init(mac);
	if (error) {
		siba_gpio_set(sc->sc_dev, 0);
		return (error);
	}
	if (phy->set_im)
		phy->set_im(mac, BWN_IMMODE_NONE);
	if (phy->set_antenna)
		phy->set_antenna(mac, BWN_ANT_DEFAULT);
	bwn_set_txantenna(mac, BWN_ANT_DEFAULT);

	if (phy->type == BWN_PHYTYPE_B)
		BWN_WRITE_2(mac, 0x005e, BWN_READ_2(mac, 0x005e) | 0x0004);
	BWN_WRITE_4(mac, 0x0100, 0x01000000);
	if (siba_get_revid(sc->sc_dev) < 5)
		BWN_WRITE_4(mac, 0x010c, 0x01000000);

	BWN_WRITE_4(mac, BWN_MACCTL,
	    BWN_READ_4(mac, BWN_MACCTL) & ~BWN_MACCTL_STA);
	BWN_WRITE_4(mac, BWN_MACCTL,
	    BWN_READ_4(mac, BWN_MACCTL) | BWN_MACCTL_STA);
	bwn_shm_write_2(mac, BWN_SHARED, 0x0074, 0x0000);

	bwn_set_opmode(mac);
	if (siba_get_revid(sc->sc_dev) < 3) {
		BWN_WRITE_2(mac, 0x060e, 0x0000);
		BWN_WRITE_2(mac, 0x0610, 0x8000);
		BWN_WRITE_2(mac, 0x0604, 0x0000);
		BWN_WRITE_2(mac, 0x0606, 0x0200);
	} else {
		BWN_WRITE_4(mac, 0x0188, 0x80000000);
		BWN_WRITE_4(mac, 0x018c, 0x02000000);
	}
	BWN_WRITE_4(mac, BWN_INTR_REASON, 0x00004000);
	BWN_WRITE_4(mac, BWN_DMA0_INTR_MASK, 0x0001dc00);
	BWN_WRITE_4(mac, BWN_DMA1_INTR_MASK, 0x0000dc00);
	BWN_WRITE_4(mac, BWN_DMA2_INTR_MASK, 0x0000dc00);
	BWN_WRITE_4(mac, BWN_DMA3_INTR_MASK, 0x0001dc00);
	BWN_WRITE_4(mac, BWN_DMA4_INTR_MASK, 0x0000dc00);
	BWN_WRITE_4(mac, BWN_DMA5_INTR_MASK, 0x0000dc00);
	siba_write_4(sc->sc_dev, SIBA_TGSLOW,
	    siba_read_4(sc->sc_dev, SIBA_TGSLOW) | 0x00100000);
	BWN_WRITE_2(mac, BWN_POWERUP_DELAY, siba_get_cc_powerdelay(sc->sc_dev));
	return (error);
}

/* read hostflags */
static uint64_t
bwn_hf_read(struct bwn_mac *mac)
{
	uint64_t ret;

	ret = bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_HFHI);
	ret <<= 16;
	ret |= bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_HFMI);
	ret <<= 16;
	ret |= bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_HFLO);
	return (ret);
}

static void
bwn_hf_write(struct bwn_mac *mac, uint64_t value)
{

	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_HFLO,
	    (value & 0x00000000ffffull));
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_HFMI,
	    (value & 0x0000ffff0000ull) >> 16);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_HFHI,
	    (value & 0xffff00000000ULL) >> 32);
}

static void
bwn_set_txretry(struct bwn_mac *mac, int s, int l)
{

	bwn_shm_write_2(mac, BWN_SCRATCH, BWN_SCRATCH_SHORT_RETRY, MIN(s, 0xf));
	bwn_shm_write_2(mac, BWN_SCRATCH, BWN_SCRATCH_LONG_RETRY, MIN(l, 0xf));
}

static void
bwn_rate_init(struct bwn_mac *mac)
{

	switch (mac->mac_phy.type) {
	case BWN_PHYTYPE_A:
	case BWN_PHYTYPE_G:
	case BWN_PHYTYPE_LP:
	case BWN_PHYTYPE_N:
		bwn_rate_write(mac, BWN_OFDM_RATE_6MB, 1);
		bwn_rate_write(mac, BWN_OFDM_RATE_12MB, 1);
		bwn_rate_write(mac, BWN_OFDM_RATE_18MB, 1);
		bwn_rate_write(mac, BWN_OFDM_RATE_24MB, 1);
		bwn_rate_write(mac, BWN_OFDM_RATE_36MB, 1);
		bwn_rate_write(mac, BWN_OFDM_RATE_48MB, 1);
		bwn_rate_write(mac, BWN_OFDM_RATE_54MB, 1);
		if (mac->mac_phy.type == BWN_PHYTYPE_A)
			break;
		/* FALLTHROUGH */
	case BWN_PHYTYPE_B:
		bwn_rate_write(mac, BWN_CCK_RATE_1MB, 0);
		bwn_rate_write(mac, BWN_CCK_RATE_2MB, 0);
		bwn_rate_write(mac, BWN_CCK_RATE_5MB, 0);
		bwn_rate_write(mac, BWN_CCK_RATE_11MB, 0);
		break;
	default:
		KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	}
}

static void
bwn_rate_write(struct bwn_mac *mac, uint16_t rate, int ofdm)
{
	uint16_t offset;

	if (ofdm) {
		offset = 0x480;
		offset += (bwn_plcp_getofdm(rate) & 0x000f) * 2;
	} else {
		offset = 0x4c0;
		offset += (bwn_plcp_getcck(rate) & 0x000f) * 2;
	}
	bwn_shm_write_2(mac, BWN_SHARED, offset + 0x20,
	    bwn_shm_read_2(mac, BWN_SHARED, offset));
}

static uint8_t
bwn_plcp_getcck(const uint8_t bitrate)
{

	switch (bitrate) {
	case BWN_CCK_RATE_1MB:
		return (0x0a);
	case BWN_CCK_RATE_2MB:
		return (0x14);
	case BWN_CCK_RATE_5MB:
		return (0x37);
	case BWN_CCK_RATE_11MB:
		return (0x6e);
	}
	KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	return (0);
}

static uint8_t
bwn_plcp_getofdm(const uint8_t bitrate)
{

	switch (bitrate) {
	case BWN_OFDM_RATE_6MB:
		return (0xb);
	case BWN_OFDM_RATE_9MB:
		return (0xf);
	case BWN_OFDM_RATE_12MB:
		return (0xa);
	case BWN_OFDM_RATE_18MB:
		return (0xe);
	case BWN_OFDM_RATE_24MB:
		return (0x9);
	case BWN_OFDM_RATE_36MB:
		return (0xd);
	case BWN_OFDM_RATE_48MB:
		return (0x8);
	case BWN_OFDM_RATE_54MB:
		return (0xc);
	}
	KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	return (0);
}

static void
bwn_set_phytxctl(struct bwn_mac *mac)
{
	uint16_t ctl;

	ctl = (BWN_TX_PHY_ENC_CCK | BWN_TX_PHY_ANT01AUTO |
	    BWN_TX_PHY_TXPWR);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_BEACON_PHYCTL, ctl);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_ACKCTS_PHYCTL, ctl);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_PROBE_RESP_PHYCTL, ctl);
}

static void
bwn_pio_init(struct bwn_mac *mac)
{
	struct bwn_pio *pio = &mac->mac_method.pio;

	BWN_WRITE_4(mac, BWN_MACCTL, BWN_READ_4(mac, BWN_MACCTL)
	    & ~BWN_MACCTL_BIGENDIAN);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_RX_PADOFFSET, 0);

	bwn_pio_set_txqueue(mac, &pio->wme[WME_AC_BK], 0);
	bwn_pio_set_txqueue(mac, &pio->wme[WME_AC_BE], 1);
	bwn_pio_set_txqueue(mac, &pio->wme[WME_AC_VI], 2);
	bwn_pio_set_txqueue(mac, &pio->wme[WME_AC_VO], 3);
	bwn_pio_set_txqueue(mac, &pio->mcast, 4);
	bwn_pio_setupqueue_rx(mac, &pio->rx, 0);
}

static void
bwn_pio_set_txqueue(struct bwn_mac *mac, struct bwn_pio_txqueue *tq,
    int index)
{
	struct bwn_pio_txpkt *tp;
	struct bwn_softc *sc = mac->mac_sc;
	unsigned int i;

	tq->tq_base = bwn_pio_idx2base(mac, index) + BWN_PIO_TXQOFFSET(mac);
	tq->tq_index = index;

	tq->tq_free = BWN_PIO_MAX_TXPACKETS;
	if (siba_get_revid(sc->sc_dev) >= 8)
		tq->tq_size = 1920;
	else {
		tq->tq_size = bwn_pio_read_2(mac, tq, BWN_PIO_TXQBUFSIZE);
		tq->tq_size -= 80;
	}

	TAILQ_INIT(&tq->tq_pktlist);
	for (i = 0; i < N(tq->tq_pkts); i++) {
		tp = &(tq->tq_pkts[i]);
		tp->tp_index = i;
		tp->tp_queue = tq;
		TAILQ_INSERT_TAIL(&tq->tq_pktlist, tp, tp_list);
	}
}

static uint16_t
bwn_pio_idx2base(struct bwn_mac *mac, int index)
{
	struct bwn_softc *sc = mac->mac_sc;
	static const uint16_t bases[] = {
		BWN_PIO_BASE0,
		BWN_PIO_BASE1,
		BWN_PIO_BASE2,
		BWN_PIO_BASE3,
		BWN_PIO_BASE4,
		BWN_PIO_BASE5,
		BWN_PIO_BASE6,
		BWN_PIO_BASE7,
	};
	static const uint16_t bases_rev11[] = {
		BWN_PIO11_BASE0,
		BWN_PIO11_BASE1,
		BWN_PIO11_BASE2,
		BWN_PIO11_BASE3,
		BWN_PIO11_BASE4,
		BWN_PIO11_BASE5,
	};

	if (siba_get_revid(sc->sc_dev) >= 11) {
		if (index >= N(bases_rev11))
			device_printf(sc->sc_dev, "%s: warning\n", __func__);
		return (bases_rev11[index]);
	}
	if (index >= N(bases))
		device_printf(sc->sc_dev, "%s: warning\n", __func__);
	return (bases[index]);
}

static void
bwn_pio_setupqueue_rx(struct bwn_mac *mac, struct bwn_pio_rxqueue *prq,
    int index)
{
	struct bwn_softc *sc = mac->mac_sc;

	prq->prq_mac = mac;
	prq->prq_rev = siba_get_revid(sc->sc_dev);
	prq->prq_base = bwn_pio_idx2base(mac, index) + BWN_PIO_RXQOFFSET(mac);
	bwn_dma_rxdirectfifo(mac, index, 1);
}

static void
bwn_destroy_pioqueue_tx(struct bwn_pio_txqueue *tq)
{
	if (tq == NULL)
		return;
	bwn_pio_cancel_tx_packets(tq);
}

static void
bwn_destroy_queue_tx(struct bwn_pio_txqueue *pio)
{

	bwn_destroy_pioqueue_tx(pio);
}

static uint16_t
bwn_pio_read_2(struct bwn_mac *mac, struct bwn_pio_txqueue *tq,
    uint16_t offset)
{

	return (BWN_READ_2(mac, tq->tq_base + offset));
}

static void
bwn_dma_rxdirectfifo(struct bwn_mac *mac, int idx, uint8_t enable)
{
	uint32_t ctl;
	int type;
	uint16_t base;

	type = bwn_dma_mask2type(bwn_dma_mask(mac));
	base = bwn_dma_base(type, idx);
	if (type == BWN_DMA_64BIT) {
		ctl = BWN_READ_4(mac, base + BWN_DMA64_RXCTL);
		ctl &= ~BWN_DMA64_RXDIRECTFIFO;
		if (enable)
			ctl |= BWN_DMA64_RXDIRECTFIFO;
		BWN_WRITE_4(mac, base + BWN_DMA64_RXCTL, ctl);
	} else {
		ctl = BWN_READ_4(mac, base + BWN_DMA32_RXCTL);
		ctl &= ~BWN_DMA32_RXDIRECTFIFO;
		if (enable)
			ctl |= BWN_DMA32_RXDIRECTFIFO;
		BWN_WRITE_4(mac, base + BWN_DMA32_RXCTL, ctl);
	}
}

static uint64_t
bwn_dma_mask(struct bwn_mac *mac)
{
	uint32_t tmp;
	uint16_t base;

	tmp = BWN_READ_4(mac, SIBA_TGSHIGH);
	if (tmp & SIBA_TGSHIGH_DMA64)
		return (BWN_DMA_BIT_MASK(64));
	base = bwn_dma_base(0, 0);
	BWN_WRITE_4(mac, base + BWN_DMA32_TXCTL, BWN_DMA32_TXADDREXT_MASK);
	tmp = BWN_READ_4(mac, base + BWN_DMA32_TXCTL);
	if (tmp & BWN_DMA32_TXADDREXT_MASK)
		return (BWN_DMA_BIT_MASK(32));

	return (BWN_DMA_BIT_MASK(30));
}

static int
bwn_dma_mask2type(uint64_t dmamask)
{

	if (dmamask == BWN_DMA_BIT_MASK(30))
		return (BWN_DMA_30BIT);
	if (dmamask == BWN_DMA_BIT_MASK(32))
		return (BWN_DMA_32BIT);
	if (dmamask == BWN_DMA_BIT_MASK(64))
		return (BWN_DMA_64BIT);
	KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	return (BWN_DMA_30BIT);
}

static void
bwn_pio_cancel_tx_packets(struct bwn_pio_txqueue *tq)
{
	struct bwn_pio_txpkt *tp;
	unsigned int i;

	for (i = 0; i < N(tq->tq_pkts); i++) {
		tp = &(tq->tq_pkts[i]);
		if (tp->tp_m) {
			m_freem(tp->tp_m);
			tp->tp_m = NULL;
		}
	}
}

static uint16_t
bwn_dma_base(int type, int controller_idx)
{
	static const uint16_t map64[] = {
		BWN_DMA64_BASE0,
		BWN_DMA64_BASE1,
		BWN_DMA64_BASE2,
		BWN_DMA64_BASE3,
		BWN_DMA64_BASE4,
		BWN_DMA64_BASE5,
	};
	static const uint16_t map32[] = {
		BWN_DMA32_BASE0,
		BWN_DMA32_BASE1,
		BWN_DMA32_BASE2,
		BWN_DMA32_BASE3,
		BWN_DMA32_BASE4,
		BWN_DMA32_BASE5,
	};

	if (type == BWN_DMA_64BIT) {
		KASSERT(controller_idx >= 0 && controller_idx < N(map64),
		    ("%s:%d: fail", __func__, __LINE__));
		return (map64[controller_idx]);
	}
	KASSERT(controller_idx >= 0 && controller_idx < N(map32),
	    ("%s:%d: fail", __func__, __LINE__));
	return (map32[controller_idx]);
}

static void
bwn_dma_init(struct bwn_mac *mac)
{
	struct bwn_dma *dma = &mac->mac_method.dma;

	/* setup TX DMA channels. */
	bwn_dma_setup(dma->wme[WME_AC_BK]);
	bwn_dma_setup(dma->wme[WME_AC_BE]);
	bwn_dma_setup(dma->wme[WME_AC_VI]);
	bwn_dma_setup(dma->wme[WME_AC_VO]);
	bwn_dma_setup(dma->mcast);
	/* setup RX DMA channel. */
	bwn_dma_setup(dma->rx);
}

static struct bwn_dma_ring *
bwn_dma_ringsetup(struct bwn_mac *mac, int controller_index,
    int for_tx, int type)
{
	struct bwn_dma *dma = &mac->mac_method.dma;
	struct bwn_dma_ring *dr;
	struct bwn_dmadesc_generic *desc;
	struct bwn_dmadesc_meta *mt;
	struct bwn_softc *sc = mac->mac_sc;
	int error, i;

	dr = malloc(sizeof(*dr), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (dr == NULL)
		goto out;
	dr->dr_numslots = BWN_RXRING_SLOTS;
	if (for_tx)
		dr->dr_numslots = BWN_TXRING_SLOTS;

	dr->dr_meta = malloc(dr->dr_numslots * sizeof(struct bwn_dmadesc_meta),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (dr->dr_meta == NULL)
		goto fail0;

	dr->dr_type = type;
	dr->dr_mac = mac;
	dr->dr_base = bwn_dma_base(type, controller_index);
	dr->dr_index = controller_index;
	if (type == BWN_DMA_64BIT) {
		dr->getdesc = bwn_dma_64_getdesc;
		dr->setdesc = bwn_dma_64_setdesc;
		dr->start_transfer = bwn_dma_64_start_transfer;
		dr->suspend = bwn_dma_64_suspend;
		dr->resume = bwn_dma_64_resume;
		dr->get_curslot = bwn_dma_64_get_curslot;
		dr->set_curslot = bwn_dma_64_set_curslot;
	} else {
		dr->getdesc = bwn_dma_32_getdesc;
		dr->setdesc = bwn_dma_32_setdesc;
		dr->start_transfer = bwn_dma_32_start_transfer;
		dr->suspend = bwn_dma_32_suspend;
		dr->resume = bwn_dma_32_resume;
		dr->get_curslot = bwn_dma_32_get_curslot;
		dr->set_curslot = bwn_dma_32_set_curslot;
	}
	if (for_tx) {
		dr->dr_tx = 1;
		dr->dr_curslot = -1;
	} else {
		if (dr->dr_index == 0) {
			dr->dr_rx_bufsize = BWN_DMA0_RX_BUFFERSIZE;
			dr->dr_frameoffset = BWN_DMA0_RX_FRAMEOFFSET;
		} else
			KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	}

	error = bwn_dma_allocringmemory(dr);
	if (error)
		goto fail2;

	if (for_tx) {
		/*
		 * Assumption: BWN_TXRING_SLOTS can be divided by
		 * BWN_TX_SLOTS_PER_FRAME
		 */
		KASSERT(BWN_TXRING_SLOTS % BWN_TX_SLOTS_PER_FRAME == 0,
		    ("%s:%d: fail", __func__, __LINE__));

		dr->dr_txhdr_cache =
		    malloc((dr->dr_numslots / BWN_TX_SLOTS_PER_FRAME) *
			BWN_HDRSIZE(mac), M_DEVBUF, M_NOWAIT | M_ZERO);
		KASSERT(dr->dr_txhdr_cache != NULL,
		    ("%s:%d: fail", __func__, __LINE__));

		/*
		 * Create TX ring DMA stuffs
		 */
		error = bus_dma_tag_create(dma->parent_dtag,
				    BWN_ALIGN, 0,
				    BUS_SPACE_MAXADDR,
				    BUS_SPACE_MAXADDR,
				    NULL, NULL,
				    BWN_HDRSIZE(mac),
				    1,
				    BUS_SPACE_MAXSIZE_32BIT,
				    0,
				    NULL, NULL,
				    &dr->dr_txring_dtag);
		if (error) {
			device_printf(sc->sc_dev,
			    "can't create TX ring DMA tag: TODO frees\n");
			goto fail1;
		}

		for (i = 0; i < dr->dr_numslots; i += 2) {
			dr->getdesc(dr, i, &desc, &mt);

			mt->mt_txtype = BWN_DMADESC_METATYPE_HEADER;
			mt->mt_m = NULL;
			mt->mt_ni = NULL;
			mt->mt_islast = 0;
			error = bus_dmamap_create(dr->dr_txring_dtag, 0,
			    &mt->mt_dmap);
			if (error) {
				device_printf(sc->sc_dev,
				     "can't create RX buf DMA map\n");
				goto fail1;
			}

			dr->getdesc(dr, i + 1, &desc, &mt);

			mt->mt_txtype = BWN_DMADESC_METATYPE_BODY;
			mt->mt_m = NULL;
			mt->mt_ni = NULL;
			mt->mt_islast = 1;
			error = bus_dmamap_create(dma->txbuf_dtag, 0,
			    &mt->mt_dmap);
			if (error) {
				device_printf(sc->sc_dev,
				     "can't create RX buf DMA map\n");
				goto fail1;
			}
		}
	} else {
		error = bus_dmamap_create(dma->rxbuf_dtag, 0,
		    &dr->dr_spare_dmap);
		if (error) {
			device_printf(sc->sc_dev,
			    "can't create RX buf DMA map\n");
			goto out;		/* XXX wrong! */
		}

		for (i = 0; i < dr->dr_numslots; i++) {
			dr->getdesc(dr, i, &desc, &mt);

			error = bus_dmamap_create(dma->rxbuf_dtag, 0,
			    &mt->mt_dmap);
			if (error) {
				device_printf(sc->sc_dev,
				    "can't create RX buf DMA map\n");
				goto out;	/* XXX wrong! */
			}
			error = bwn_dma_newbuf(dr, desc, mt, 1);
			if (error) {
				device_printf(sc->sc_dev,
				    "failed to allocate RX buf\n");
				goto out;	/* XXX wrong! */
			}
		}

		bus_dmamap_sync(dr->dr_ring_dtag, dr->dr_ring_dmap,
		    BUS_DMASYNC_PREWRITE);

		dr->dr_usedslot = dr->dr_numslots;
	}

      out:
	return (dr);

fail2:
	free(dr->dr_txhdr_cache, M_DEVBUF);
fail1:
	free(dr->dr_meta, M_DEVBUF);
fail0:
	free(dr, M_DEVBUF);
	return (NULL);
}

static void
bwn_dma_ringfree(struct bwn_dma_ring **dr)
{

	if (dr == NULL)
		return;

	bwn_dma_free_descbufs(*dr);
	bwn_dma_free_ringmemory(*dr);

	free((*dr)->dr_txhdr_cache, M_DEVBUF);
	free((*dr)->dr_meta, M_DEVBUF);
	free(*dr, M_DEVBUF);

	*dr = NULL;
}

static void
bwn_dma_32_getdesc(struct bwn_dma_ring *dr, int slot,
    struct bwn_dmadesc_generic **gdesc, struct bwn_dmadesc_meta **meta)
{
	struct bwn_dmadesc32 *desc;

	*meta = &(dr->dr_meta[slot]);
	desc = dr->dr_ring_descbase;
	desc = &(desc[slot]);

	*gdesc = (struct bwn_dmadesc_generic *)desc;
}

static void
bwn_dma_32_setdesc(struct bwn_dma_ring *dr,
    struct bwn_dmadesc_generic *desc, bus_addr_t dmaaddr, uint16_t bufsize,
    int start, int end, int irq)
{
	struct bwn_dmadesc32 *descbase = dr->dr_ring_descbase;
	struct bwn_softc *sc = dr->dr_mac->mac_sc;
	uint32_t addr, addrext, ctl;
	int slot;

	slot = (int)(&(desc->dma.dma32) - descbase);
	KASSERT(slot >= 0 && slot < dr->dr_numslots,
	    ("%s:%d: fail", __func__, __LINE__));

	addr = (uint32_t) (dmaaddr & ~SIBA_DMA_TRANSLATION_MASK);
	addrext = (uint32_t) (dmaaddr & SIBA_DMA_TRANSLATION_MASK) >> 30;
	addr |= siba_dma_translation(sc->sc_dev);
	ctl = bufsize & BWN_DMA32_DCTL_BYTECNT;
	if (slot == dr->dr_numslots - 1)
		ctl |= BWN_DMA32_DCTL_DTABLEEND;
	if (start)
		ctl |= BWN_DMA32_DCTL_FRAMESTART;
	if (end)
		ctl |= BWN_DMA32_DCTL_FRAMEEND;
	if (irq)
		ctl |= BWN_DMA32_DCTL_IRQ;
	ctl |= (addrext << BWN_DMA32_DCTL_ADDREXT_SHIFT)
	    & BWN_DMA32_DCTL_ADDREXT_MASK;

	desc->dma.dma32.control = htole32(ctl);
	desc->dma.dma32.address = htole32(addr);
}

static void
bwn_dma_32_start_transfer(struct bwn_dma_ring *dr, int slot)
{

	BWN_DMA_WRITE(dr, BWN_DMA32_TXINDEX,
	    (uint32_t)(slot * sizeof(struct bwn_dmadesc32)));
}

static void
bwn_dma_32_suspend(struct bwn_dma_ring *dr)
{

	BWN_DMA_WRITE(dr, BWN_DMA32_TXCTL,
	    BWN_DMA_READ(dr, BWN_DMA32_TXCTL) | BWN_DMA32_TXSUSPEND);
}

static void
bwn_dma_32_resume(struct bwn_dma_ring *dr)
{

	BWN_DMA_WRITE(dr, BWN_DMA32_TXCTL,
	    BWN_DMA_READ(dr, BWN_DMA32_TXCTL) & ~BWN_DMA32_TXSUSPEND);
}

static int
bwn_dma_32_get_curslot(struct bwn_dma_ring *dr)
{
	uint32_t val;

	val = BWN_DMA_READ(dr, BWN_DMA32_RXSTATUS);
	val &= BWN_DMA32_RXDPTR;

	return (val / sizeof(struct bwn_dmadesc32));
}

static void
bwn_dma_32_set_curslot(struct bwn_dma_ring *dr, int slot)
{

	BWN_DMA_WRITE(dr, BWN_DMA32_RXINDEX,
	    (uint32_t) (slot * sizeof(struct bwn_dmadesc32)));
}

static void
bwn_dma_64_getdesc(struct bwn_dma_ring *dr, int slot,
    struct bwn_dmadesc_generic **gdesc, struct bwn_dmadesc_meta **meta)
{
	struct bwn_dmadesc64 *desc;

	*meta = &(dr->dr_meta[slot]);
	desc = dr->dr_ring_descbase;
	desc = &(desc[slot]);

	*gdesc = (struct bwn_dmadesc_generic *)desc;
}

static void
bwn_dma_64_setdesc(struct bwn_dma_ring *dr,
    struct bwn_dmadesc_generic *desc, bus_addr_t dmaaddr, uint16_t bufsize,
    int start, int end, int irq)
{
	struct bwn_dmadesc64 *descbase = dr->dr_ring_descbase;
	struct bwn_softc *sc = dr->dr_mac->mac_sc;
	int slot;
	uint32_t ctl0 = 0, ctl1 = 0;
	uint32_t addrlo, addrhi;
	uint32_t addrext;

	slot = (int)(&(desc->dma.dma64) - descbase);
	KASSERT(slot >= 0 && slot < dr->dr_numslots,
	    ("%s:%d: fail", __func__, __LINE__));

	addrlo = (uint32_t) (dmaaddr & 0xffffffff);
	addrhi = (((uint64_t) dmaaddr >> 32) & ~SIBA_DMA_TRANSLATION_MASK);
	addrext = (((uint64_t) dmaaddr >> 32) & SIBA_DMA_TRANSLATION_MASK) >>
	    30;
	addrhi |= (siba_dma_translation(sc->sc_dev) << 1);
	if (slot == dr->dr_numslots - 1)
		ctl0 |= BWN_DMA64_DCTL0_DTABLEEND;
	if (start)
		ctl0 |= BWN_DMA64_DCTL0_FRAMESTART;
	if (end)
		ctl0 |= BWN_DMA64_DCTL0_FRAMEEND;
	if (irq)
		ctl0 |= BWN_DMA64_DCTL0_IRQ;
	ctl1 |= bufsize & BWN_DMA64_DCTL1_BYTECNT;
	ctl1 |= (addrext << BWN_DMA64_DCTL1_ADDREXT_SHIFT)
	    & BWN_DMA64_DCTL1_ADDREXT_MASK;

	desc->dma.dma64.control0 = htole32(ctl0);
	desc->dma.dma64.control1 = htole32(ctl1);
	desc->dma.dma64.address_low = htole32(addrlo);
	desc->dma.dma64.address_high = htole32(addrhi);
}

static void
bwn_dma_64_start_transfer(struct bwn_dma_ring *dr, int slot)
{

	BWN_DMA_WRITE(dr, BWN_DMA64_TXINDEX,
	    (uint32_t)(slot * sizeof(struct bwn_dmadesc64)));
}

static void
bwn_dma_64_suspend(struct bwn_dma_ring *dr)
{

	BWN_DMA_WRITE(dr, BWN_DMA64_TXCTL,
	    BWN_DMA_READ(dr, BWN_DMA64_TXCTL) | BWN_DMA64_TXSUSPEND);
}

static void
bwn_dma_64_resume(struct bwn_dma_ring *dr)
{

	BWN_DMA_WRITE(dr, BWN_DMA64_TXCTL,
	    BWN_DMA_READ(dr, BWN_DMA64_TXCTL) & ~BWN_DMA64_TXSUSPEND);
}

static int
bwn_dma_64_get_curslot(struct bwn_dma_ring *dr)
{
	uint32_t val;

	val = BWN_DMA_READ(dr, BWN_DMA64_RXSTATUS);
	val &= BWN_DMA64_RXSTATDPTR;

	return (val / sizeof(struct bwn_dmadesc64));
}

static void
bwn_dma_64_set_curslot(struct bwn_dma_ring *dr, int slot)
{

	BWN_DMA_WRITE(dr, BWN_DMA64_RXINDEX,
	    (uint32_t)(slot * sizeof(struct bwn_dmadesc64)));
}

static int
bwn_dma_allocringmemory(struct bwn_dma_ring *dr)
{
	struct bwn_mac *mac = dr->dr_mac;
	struct bwn_dma *dma = &mac->mac_method.dma;
	struct bwn_softc *sc = mac->mac_sc;
	int error;

	error = bus_dma_tag_create(dma->parent_dtag,
			    BWN_ALIGN, 0,
			    BUS_SPACE_MAXADDR,
			    BUS_SPACE_MAXADDR,
			    NULL, NULL,
			    BWN_DMA_RINGMEMSIZE,
			    1,
			    BUS_SPACE_MAXSIZE_32BIT,
			    0,
			    NULL, NULL,
			    &dr->dr_ring_dtag);
	if (error) {
		device_printf(sc->sc_dev,
		    "can't create TX ring DMA tag: TODO frees\n");
		return (-1);
	}

	error = bus_dmamem_alloc(dr->dr_ring_dtag,
	    &dr->dr_ring_descbase, BUS_DMA_WAITOK | BUS_DMA_ZERO,
	    &dr->dr_ring_dmap);
	if (error) {
		device_printf(sc->sc_dev,
		    "can't allocate DMA mem: TODO frees\n");
		return (-1);
	}
	error = bus_dmamap_load(dr->dr_ring_dtag, dr->dr_ring_dmap,
	    dr->dr_ring_descbase, BWN_DMA_RINGMEMSIZE,
	    bwn_dma_ring_addr, &dr->dr_ring_dmabase, BUS_DMA_NOWAIT);
	if (error) {
		device_printf(sc->sc_dev,
		    "can't load DMA mem: TODO free\n");
		return (-1);
	}

	return (0);
}

static void
bwn_dma_setup(struct bwn_dma_ring *dr)
{
	struct bwn_softc *sc = dr->dr_mac->mac_sc;
	uint64_t ring64;
	uint32_t addrext, ring32, value;
	uint32_t trans = siba_dma_translation(sc->sc_dev);

	if (dr->dr_tx) {
		dr->dr_curslot = -1;

		if (dr->dr_type == BWN_DMA_64BIT) {
			ring64 = (uint64_t)(dr->dr_ring_dmabase);
			addrext = ((ring64 >> 32) & SIBA_DMA_TRANSLATION_MASK)
			    >> 30;
			value = BWN_DMA64_TXENABLE;
			value |= (addrext << BWN_DMA64_TXADDREXT_SHIFT)
			    & BWN_DMA64_TXADDREXT_MASK;
			BWN_DMA_WRITE(dr, BWN_DMA64_TXCTL, value);
			BWN_DMA_WRITE(dr, BWN_DMA64_TXRINGLO,
			    (ring64 & 0xffffffff));
			BWN_DMA_WRITE(dr, BWN_DMA64_TXRINGHI,
			    ((ring64 >> 32) &
			    ~SIBA_DMA_TRANSLATION_MASK) | (trans << 1));
		} else {
			ring32 = (uint32_t)(dr->dr_ring_dmabase);
			addrext = (ring32 & SIBA_DMA_TRANSLATION_MASK) >> 30;
			value = BWN_DMA32_TXENABLE;
			value |= (addrext << BWN_DMA32_TXADDREXT_SHIFT)
			    & BWN_DMA32_TXADDREXT_MASK;
			BWN_DMA_WRITE(dr, BWN_DMA32_TXCTL, value);
			BWN_DMA_WRITE(dr, BWN_DMA32_TXRING,
			    (ring32 & ~SIBA_DMA_TRANSLATION_MASK) | trans);
		}
		return;
	}

	/*
	 * set for RX
	 */
	dr->dr_usedslot = dr->dr_numslots;

	if (dr->dr_type == BWN_DMA_64BIT) {
		ring64 = (uint64_t)(dr->dr_ring_dmabase);
		addrext = ((ring64 >> 32) & SIBA_DMA_TRANSLATION_MASK) >> 30;
		value = (dr->dr_frameoffset << BWN_DMA64_RXFROFF_SHIFT);
		value |= BWN_DMA64_RXENABLE;
		value |= (addrext << BWN_DMA64_RXADDREXT_SHIFT)
		    & BWN_DMA64_RXADDREXT_MASK;
		BWN_DMA_WRITE(dr, BWN_DMA64_RXCTL, value);
		BWN_DMA_WRITE(dr, BWN_DMA64_RXRINGLO, (ring64 & 0xffffffff));
		BWN_DMA_WRITE(dr, BWN_DMA64_RXRINGHI,
		    ((ring64 >> 32) & ~SIBA_DMA_TRANSLATION_MASK)
		    | (trans << 1));
		BWN_DMA_WRITE(dr, BWN_DMA64_RXINDEX, dr->dr_numslots *
		    sizeof(struct bwn_dmadesc64));
	} else {
		ring32 = (uint32_t)(dr->dr_ring_dmabase);
		addrext = (ring32 & SIBA_DMA_TRANSLATION_MASK) >> 30;
		value = (dr->dr_frameoffset << BWN_DMA32_RXFROFF_SHIFT);
		value |= BWN_DMA32_RXENABLE;
		value |= (addrext << BWN_DMA32_RXADDREXT_SHIFT)
		    & BWN_DMA32_RXADDREXT_MASK;
		BWN_DMA_WRITE(dr, BWN_DMA32_RXCTL, value);
		BWN_DMA_WRITE(dr, BWN_DMA32_RXRING,
		    (ring32 & ~SIBA_DMA_TRANSLATION_MASK) | trans);
		BWN_DMA_WRITE(dr, BWN_DMA32_RXINDEX, dr->dr_numslots *
		    sizeof(struct bwn_dmadesc32));
	}
}

static void
bwn_dma_free_ringmemory(struct bwn_dma_ring *dr)
{

	bus_dmamap_unload(dr->dr_ring_dtag, dr->dr_ring_dmap);
	bus_dmamem_free(dr->dr_ring_dtag, dr->dr_ring_descbase,
	    dr->dr_ring_dmap);
}

static void
bwn_dma_cleanup(struct bwn_dma_ring *dr)
{

	if (dr->dr_tx) {
		bwn_dma_tx_reset(dr->dr_mac, dr->dr_base, dr->dr_type);
		if (dr->dr_type == BWN_DMA_64BIT) {
			BWN_DMA_WRITE(dr, BWN_DMA64_TXRINGLO, 0);
			BWN_DMA_WRITE(dr, BWN_DMA64_TXRINGHI, 0);
		} else
			BWN_DMA_WRITE(dr, BWN_DMA32_TXRING, 0);
	} else {
		bwn_dma_rx_reset(dr->dr_mac, dr->dr_base, dr->dr_type);
		if (dr->dr_type == BWN_DMA_64BIT) {
			BWN_DMA_WRITE(dr, BWN_DMA64_RXRINGLO, 0);
			BWN_DMA_WRITE(dr, BWN_DMA64_RXRINGHI, 0);
		} else
			BWN_DMA_WRITE(dr, BWN_DMA32_RXRING, 0);
	}
}

static void
bwn_dma_free_descbufs(struct bwn_dma_ring *dr)
{
	struct bwn_dmadesc_generic *desc;
	struct bwn_dmadesc_meta *meta;
	struct bwn_mac *mac = dr->dr_mac;
	struct bwn_dma *dma = &mac->mac_method.dma;
	struct bwn_softc *sc = mac->mac_sc;
	int i;

	if (!dr->dr_usedslot)
		return;
	for (i = 0; i < dr->dr_numslots; i++) {
		dr->getdesc(dr, i, &desc, &meta);

		if (meta->mt_m == NULL) {
			if (!dr->dr_tx)
				device_printf(sc->sc_dev, "%s: not TX?\n",
				    __func__);
			continue;
		}
		if (dr->dr_tx) {
			if (meta->mt_txtype == BWN_DMADESC_METATYPE_HEADER)
				bus_dmamap_unload(dr->dr_txring_dtag,
				    meta->mt_dmap);
			else if (meta->mt_txtype == BWN_DMADESC_METATYPE_BODY)
				bus_dmamap_unload(dma->txbuf_dtag,
				    meta->mt_dmap);
		} else
			bus_dmamap_unload(dma->rxbuf_dtag, meta->mt_dmap);
		bwn_dma_free_descbuf(dr, meta);
	}
}

static int
bwn_dma_tx_reset(struct bwn_mac *mac, uint16_t base,
    int type)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t value;
	int i;
	uint16_t offset;

	for (i = 0; i < 10; i++) {
		offset = (type == BWN_DMA_64BIT) ? BWN_DMA64_TXSTATUS :
		    BWN_DMA32_TXSTATUS;
		value = BWN_READ_4(mac, base + offset);
		if (type == BWN_DMA_64BIT) {
			value &= BWN_DMA64_TXSTAT;
			if (value == BWN_DMA64_TXSTAT_DISABLED ||
			    value == BWN_DMA64_TXSTAT_IDLEWAIT ||
			    value == BWN_DMA64_TXSTAT_STOPPED)
				break;
		} else {
			value &= BWN_DMA32_TXSTATE;
			if (value == BWN_DMA32_TXSTAT_DISABLED ||
			    value == BWN_DMA32_TXSTAT_IDLEWAIT ||
			    value == BWN_DMA32_TXSTAT_STOPPED)
				break;
		}
		DELAY(1000);
	}
	offset = (type == BWN_DMA_64BIT) ? BWN_DMA64_TXCTL : BWN_DMA32_TXCTL;
	BWN_WRITE_4(mac, base + offset, 0);
	for (i = 0; i < 10; i++) {
		offset = (type == BWN_DMA_64BIT) ? BWN_DMA64_TXSTATUS :
						   BWN_DMA32_TXSTATUS;
		value = BWN_READ_4(mac, base + offset);
		if (type == BWN_DMA_64BIT) {
			value &= BWN_DMA64_TXSTAT;
			if (value == BWN_DMA64_TXSTAT_DISABLED) {
				i = -1;
				break;
			}
		} else {
			value &= BWN_DMA32_TXSTATE;
			if (value == BWN_DMA32_TXSTAT_DISABLED) {
				i = -1;
				break;
			}
		}
		DELAY(1000);
	}
	if (i != -1) {
		device_printf(sc->sc_dev, "%s: timed out\n", __func__);
		return (ENODEV);
	}
	DELAY(1000);

	return (0);
}

static int
bwn_dma_rx_reset(struct bwn_mac *mac, uint16_t base,
    int type)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t value;
	int i;
	uint16_t offset;

	offset = (type == BWN_DMA_64BIT) ? BWN_DMA64_RXCTL : BWN_DMA32_RXCTL;
	BWN_WRITE_4(mac, base + offset, 0);
	for (i = 0; i < 10; i++) {
		offset = (type == BWN_DMA_64BIT) ? BWN_DMA64_RXSTATUS :
		    BWN_DMA32_RXSTATUS;
		value = BWN_READ_4(mac, base + offset);
		if (type == BWN_DMA_64BIT) {
			value &= BWN_DMA64_RXSTAT;
			if (value == BWN_DMA64_RXSTAT_DISABLED) {
				i = -1;
				break;
			}
		} else {
			value &= BWN_DMA32_RXSTATE;
			if (value == BWN_DMA32_RXSTAT_DISABLED) {
				i = -1;
				break;
			}
		}
		DELAY(1000);
	}
	if (i != -1) {
		device_printf(sc->sc_dev, "%s: timed out\n", __func__);
		return (ENODEV);
	}

	return (0);
}

static void
bwn_dma_free_descbuf(struct bwn_dma_ring *dr,
    struct bwn_dmadesc_meta *meta)
{

	if (meta->mt_m != NULL) {
		m_freem(meta->mt_m);
		meta->mt_m = NULL;
	}
	if (meta->mt_ni != NULL) {
		ieee80211_free_node(meta->mt_ni);
		meta->mt_ni = NULL;
	}
}

static void
bwn_dma_set_redzone(struct bwn_dma_ring *dr, struct mbuf *m)
{
	struct bwn_rxhdr4 *rxhdr;
	unsigned char *frame;

	rxhdr = mtod(m, struct bwn_rxhdr4 *);
	rxhdr->frame_len = 0;

	KASSERT(dr->dr_rx_bufsize >= dr->dr_frameoffset +
	    sizeof(struct bwn_plcp6) + 2,
	    ("%s:%d: fail", __func__, __LINE__));
	frame = mtod(m, char *) + dr->dr_frameoffset;
	memset(frame, 0xff, sizeof(struct bwn_plcp6) + 2 /* padding */);
}

static uint8_t
bwn_dma_check_redzone(struct bwn_dma_ring *dr, struct mbuf *m)
{
	unsigned char *f = mtod(m, char *) + dr->dr_frameoffset;

	return ((f[0] & f[1] & f[2] & f[3] & f[4] & f[5] & f[6] & f[7])
	    == 0xff);
}

static void
bwn_wme_init(struct bwn_mac *mac)
{

	bwn_wme_load(mac);

	/* enable WME support. */
	bwn_hf_write(mac, bwn_hf_read(mac) | BWN_HF_EDCF);
	BWN_WRITE_2(mac, BWN_IFSCTL, BWN_READ_2(mac, BWN_IFSCTL) |
	    BWN_IFSCTL_USE_EDCF);
}

static void
bwn_spu_setdelay(struct bwn_mac *mac, int idle)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	uint16_t delay;	/* microsec */

	delay = (mac->mac_phy.type == BWN_PHYTYPE_A) ? 3700 : 1050;
	if (ic->ic_opmode == IEEE80211_M_IBSS || idle)
		delay = 500;
	if ((mac->mac_phy.rf_ver == 0x2050) && (mac->mac_phy.rf_rev == 8))
		delay = max(delay, (uint16_t)2400);

	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_SPU_WAKEUP, delay);
}

static void
bwn_bt_enable(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint64_t hf;

	if (bwn_bluetooth == 0)
		return;
	if ((siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_BTCOEXIST) == 0)
		return;
	if (mac->mac_phy.type != BWN_PHYTYPE_B && !mac->mac_phy.gmode)
		return;

	hf = bwn_hf_read(mac);
	if (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_BTCMOD)
		hf |= BWN_HF_BT_COEXISTALT;
	else
		hf |= BWN_HF_BT_COEXIST;
	bwn_hf_write(mac, hf);
}

static void
bwn_set_macaddr(struct bwn_mac *mac)
{

	bwn_mac_write_bssid(mac);
	bwn_mac_setfilter(mac, BWN_MACFILTER_SELF, mac->mac_sc->sc_macaddr);
}

static void
bwn_clear_keys(struct bwn_mac *mac)
{
	int i;

	for (i = 0; i < mac->mac_max_nr_keys; i++) {
		KASSERT(i >= 0 && i < mac->mac_max_nr_keys,
		    ("%s:%d: fail", __func__, __LINE__));

		bwn_key_dowrite(mac, i, BWN_SEC_ALGO_NONE,
		    NULL, BWN_SEC_KEYSIZE, NULL);
		if ((i <= 3) && !BWN_SEC_NEWAPI(mac)) {
			bwn_key_dowrite(mac, i + 4, BWN_SEC_ALGO_NONE,
			    NULL, BWN_SEC_KEYSIZE, NULL);
		}
		mac->mac_key[i].keyconf = NULL;
	}
}

static void
bwn_crypt_init(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;

	mac->mac_max_nr_keys = (siba_get_revid(sc->sc_dev) >= 5) ? 58 : 20;
	KASSERT(mac->mac_max_nr_keys <= N(mac->mac_key),
	    ("%s:%d: fail", __func__, __LINE__));
	mac->mac_ktp = bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_KEY_TABLEP);
	mac->mac_ktp *= 2;
	if (siba_get_revid(sc->sc_dev) >= 5)
		BWN_WRITE_2(mac, BWN_RCMTA_COUNT, mac->mac_max_nr_keys - 8);
	bwn_clear_keys(mac);
}

static void
bwn_chip_exit(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;

	bwn_phy_exit(mac);
	siba_gpio_set(sc->sc_dev, 0);
}

static int
bwn_fw_fillinfo(struct bwn_mac *mac)
{
	int error;

	error = bwn_fw_gets(mac, BWN_FWTYPE_DEFAULT);
	if (error == 0)
		return (0);
	error = bwn_fw_gets(mac, BWN_FWTYPE_OPENSOURCE);
	if (error == 0)
		return (0);
	return (error);
}

static int
bwn_gpio_init(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t mask = 0x1f, set = 0xf, value;

	BWN_WRITE_4(mac, BWN_MACCTL,
	    BWN_READ_4(mac, BWN_MACCTL) & ~BWN_MACCTL_GPOUT_MASK);
	BWN_WRITE_2(mac, BWN_GPIO_MASK,
	    BWN_READ_2(mac, BWN_GPIO_MASK) | 0x000f);

	if (siba_get_chipid(sc->sc_dev) == 0x4301) {
		mask |= 0x0060;
		set |= 0x0060;
	}
	if (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_PACTRL) {
		BWN_WRITE_2(mac, BWN_GPIO_MASK,
		    BWN_READ_2(mac, BWN_GPIO_MASK) | 0x0200);
		mask |= 0x0200;
		set |= 0x0200;
	}
	if (siba_get_revid(sc->sc_dev) >= 2)
		mask |= 0x0010;

	value = siba_gpio_get(sc->sc_dev);
	if (value == -1)
		return (0);
	siba_gpio_set(sc->sc_dev, (value & mask) | set);

	return (0);
}

static int
bwn_fw_loadinitvals(struct bwn_mac *mac)
{
#define	GETFWOFFSET(fwp, offset)				\
	((const struct bwn_fwinitvals *)((const char *)fwp.fw->data + offset))
	const size_t hdr_len = sizeof(struct bwn_fwhdr);
	const struct bwn_fwhdr *hdr;
	struct bwn_fw *fw = &mac->mac_fw;
	int error;

	hdr = (const struct bwn_fwhdr *)(fw->initvals.fw->data);
	error = bwn_fwinitvals_write(mac, GETFWOFFSET(fw->initvals, hdr_len),
	    be32toh(hdr->size), fw->initvals.fw->datasize - hdr_len);
	if (error)
		return (error);
	if (fw->initvals_band.fw) {
		hdr = (const struct bwn_fwhdr *)(fw->initvals_band.fw->data);
		error = bwn_fwinitvals_write(mac,
		    GETFWOFFSET(fw->initvals_band, hdr_len),
		    be32toh(hdr->size),
		    fw->initvals_band.fw->datasize - hdr_len);
	}
	return (error);
#undef GETFWOFFSET
}

static int
bwn_phy_init(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	int error;

	mac->mac_phy.chan = mac->mac_phy.get_default_chan(mac);
	mac->mac_phy.rf_onoff(mac, 1);
	error = mac->mac_phy.init(mac);
	if (error) {
		device_printf(sc->sc_dev, "PHY init failed\n");
		goto fail0;
	}
	error = bwn_switch_channel(mac,
	    mac->mac_phy.get_default_chan(mac));
	if (error) {
		device_printf(sc->sc_dev,
		    "failed to switch default channel\n");
		goto fail1;
	}
	return (0);
fail1:
	if (mac->mac_phy.exit)
		mac->mac_phy.exit(mac);
fail0:
	mac->mac_phy.rf_onoff(mac, 0);

	return (error);
}

static void
bwn_set_txantenna(struct bwn_mac *mac, int antenna)
{
	uint16_t ant;
	uint16_t tmp;

	ant = bwn_ant2phy(antenna);

	/* For ACK/CTS */
	tmp = bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_ACKCTS_PHYCTL);
	tmp = (tmp & ~BWN_TX_PHY_ANT) | ant;
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_ACKCTS_PHYCTL, tmp);
	/* For Probe Resposes */
	tmp = bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_PROBE_RESP_PHYCTL);
	tmp = (tmp & ~BWN_TX_PHY_ANT) | ant;
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_PROBE_RESP_PHYCTL, tmp);
}

static void
bwn_set_opmode(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint32_t ctl;
	uint16_t cfp_pretbtt;

	ctl = BWN_READ_4(mac, BWN_MACCTL);
	ctl &= ~(BWN_MACCTL_HOSTAP | BWN_MACCTL_PASS_CTL |
	    BWN_MACCTL_PASS_BADPLCP | BWN_MACCTL_PASS_BADFCS |
	    BWN_MACCTL_PROMISC | BWN_MACCTL_BEACON_PROMISC);
	ctl |= BWN_MACCTL_STA;

	if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
	    ic->ic_opmode == IEEE80211_M_MBSS)
		ctl |= BWN_MACCTL_HOSTAP;
	else if (ic->ic_opmode == IEEE80211_M_IBSS)
		ctl &= ~BWN_MACCTL_STA;
	ctl |= sc->sc_filters;

	if (siba_get_revid(sc->sc_dev) <= 4)
		ctl |= BWN_MACCTL_PROMISC;

	BWN_WRITE_4(mac, BWN_MACCTL, ctl);

	cfp_pretbtt = 2;
	if ((ctl & BWN_MACCTL_STA) && !(ctl & BWN_MACCTL_HOSTAP)) {
		if (siba_get_chipid(sc->sc_dev) == 0x4306 &&
		    siba_get_chiprev(sc->sc_dev) == 3)
			cfp_pretbtt = 100;
		else
			cfp_pretbtt = 50;
	}
	BWN_WRITE_2(mac, 0x612, cfp_pretbtt);
}

static int
bwn_dma_gettype(struct bwn_mac *mac)
{
	uint32_t tmp;
	uint16_t base;

	tmp = BWN_READ_4(mac, SIBA_TGSHIGH);
	if (tmp & SIBA_TGSHIGH_DMA64)
		return (BWN_DMA_64BIT);
	base = bwn_dma_base(0, 0);
	BWN_WRITE_4(mac, base + BWN_DMA32_TXCTL, BWN_DMA32_TXADDREXT_MASK);
	tmp = BWN_READ_4(mac, base + BWN_DMA32_TXCTL);
	if (tmp & BWN_DMA32_TXADDREXT_MASK)
		return (BWN_DMA_32BIT);

	return (BWN_DMA_30BIT);
}

static void
bwn_dma_ring_addr(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	if (!error) {
		KASSERT(nseg == 1, ("too many segments(%d)\n", nseg));
		*((bus_addr_t *)arg) = seg->ds_addr;
	}
}

static void
bwn_phy_g_init_sub(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t i, tmp;

	if (phy->rev == 1)
		bwn_phy_init_b5(mac);
	else
		bwn_phy_init_b6(mac);

	if (phy->rev >= 2 || phy->gmode)
		bwn_phy_init_a(mac);

	if (phy->rev >= 2) {
		BWN_PHY_WRITE(mac, BWN_PHY_ANALOGOVER, 0);
		BWN_PHY_WRITE(mac, BWN_PHY_ANALOGOVERVAL, 0);
	}
	if (phy->rev == 2) {
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, 0);
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xc0);
	}
	if (phy->rev > 5) {
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, 0x400);
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xc0);
	}
	if (phy->gmode || phy->rev >= 2) {
		tmp = BWN_PHY_READ(mac, BWN_PHY_VERSION_OFDM);
		tmp &= BWN_PHYVER_VERSION;
		if (tmp == 3 || tmp == 5) {
			BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0xc2), 0x1816);
			BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0xc3), 0x8006);
		}
		if (tmp == 5) {
			BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0xcc), 0x00ff,
			    0x1f00);
		}
	}
	if ((phy->rev <= 2 && phy->gmode) || phy->rev >= 2)
		BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0x7e), 0x78);
	if (phy->rf_rev == 8) {
		BWN_PHY_SET(mac, BWN_PHY_EXTG(0x01), 0x80);
		BWN_PHY_SET(mac, BWN_PHY_OFDM(0x3e), 0x4);
	}
	if (BWN_HAS_LOOPBACK(phy))
		bwn_loopback_calcgain(mac);

	if (phy->rf_rev != 8) {
		if (pg->pg_initval == 0xffff)
			pg->pg_initval = bwn_rf_init_bcm2050(mac);
		else
			BWN_RF_WRITE(mac, 0x0078, pg->pg_initval);
	}
	bwn_lo_g_init(mac);
	if (BWN_HAS_TXMAG(phy)) {
		BWN_RF_WRITE(mac, 0x52,
		    (BWN_RF_READ(mac, 0x52) & 0xff00)
		    | pg->pg_loctl.tx_bias |
		    pg->pg_loctl.tx_magn);
	} else {
		BWN_RF_SETMASK(mac, 0x52, 0xfff0, pg->pg_loctl.tx_bias);
	}
	if (phy->rev >= 6) {
		BWN_PHY_SETMASK(mac, BWN_PHY_CCK(0x36), 0x0fff,
		    (pg->pg_loctl.tx_bias << 12));
	}
	if (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_PACTRL)
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2e), 0x8075);
	else
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2e), 0x807f);
	if (phy->rev < 2)
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2f), 0x101);
	else
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2f), 0x202);
	if (phy->gmode || phy->rev >= 2) {
		bwn_lo_g_adjust(mac);
		BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, 0x8078);
	}

	if (!(siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_RSSI)) {
		for (i = 0; i < 64; i++) {
			BWN_PHY_WRITE(mac, BWN_PHY_NRSSI_CTRL, i);
			BWN_PHY_WRITE(mac, BWN_PHY_NRSSI_DATA,
			    (uint16_t)MIN(MAX(bwn_nrssi_read(mac, i) - 0xffff,
			    -32), 31));
		}
		bwn_nrssi_threshold(mac);
	} else if (phy->gmode || phy->rev >= 2) {
		if (pg->pg_nrssi[0] == -1000) {
			KASSERT(pg->pg_nrssi[1] == -1000,
			    ("%s:%d: fail", __func__, __LINE__));
			bwn_nrssi_slope_11g(mac);
		} else
			bwn_nrssi_threshold(mac);
	}
	if (phy->rf_rev == 8)
		BWN_PHY_WRITE(mac, BWN_PHY_EXTG(0x05), 0x3230);
	bwn_phy_hwpctl_init(mac);
	if ((siba_get_chipid(sc->sc_dev) == 0x4306
	     && siba_get_chippkg(sc->sc_dev) == 2) || 0) {
		BWN_PHY_MASK(mac, BWN_PHY_CRS0, 0xbfff);
		BWN_PHY_MASK(mac, BWN_PHY_OFDM(0xc3), 0x7fff);
	}
}

static uint8_t
bwn_has_hwpctl(struct bwn_mac *mac)
{

	if (mac->mac_phy.hwpctl == 0 || mac->mac_phy.use_hwpctl == NULL)
		return (0);
	return (mac->mac_phy.use_hwpctl(mac));
}

static void
bwn_phy_init_b5(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t offset, value;
	uint8_t old_channel;

	if (phy->analog == 1)
		BWN_RF_SET(mac, 0x007a, 0x0050);
	if ((siba_get_pci_subvendor(sc->sc_dev) != SIBA_BOARDVENDOR_BCM) &&
	    (siba_get_pci_subdevice(sc->sc_dev) != SIBA_BOARD_BU4306)) {
		value = 0x2120;
		for (offset = 0x00a8; offset < 0x00c7; offset++) {
			BWN_PHY_WRITE(mac, offset, value);
			value += 0x202;
		}
	}
	BWN_PHY_SETMASK(mac, 0x0035, 0xf0ff, 0x0700);
	if (phy->rf_ver == 0x2050)
		BWN_PHY_WRITE(mac, 0x0038, 0x0667);

	if (phy->gmode || phy->rev >= 2) {
		if (phy->rf_ver == 0x2050) {
			BWN_RF_SET(mac, 0x007a, 0x0020);
			BWN_RF_SET(mac, 0x0051, 0x0004);
		}
		BWN_WRITE_2(mac, BWN_PHY_RADIO, 0x0000);

		BWN_PHY_SET(mac, 0x0802, 0x0100);
		BWN_PHY_SET(mac, 0x042b, 0x2000);

		BWN_PHY_WRITE(mac, 0x001c, 0x186a);

		BWN_PHY_SETMASK(mac, 0x0013, 0x00ff, 0x1900);
		BWN_PHY_SETMASK(mac, 0x0035, 0xffc0, 0x0064);
		BWN_PHY_SETMASK(mac, 0x005d, 0xff80, 0x000a);
	}

	if (mac->mac_flags & BWN_MAC_FLAG_BADFRAME_PREEMP)
		BWN_PHY_SET(mac, BWN_PHY_RADIO_BITFIELD, (1 << 11));

	if (phy->analog == 1) {
		BWN_PHY_WRITE(mac, 0x0026, 0xce00);
		BWN_PHY_WRITE(mac, 0x0021, 0x3763);
		BWN_PHY_WRITE(mac, 0x0022, 0x1bc3);
		BWN_PHY_WRITE(mac, 0x0023, 0x06f9);
		BWN_PHY_WRITE(mac, 0x0024, 0x037e);
	} else
		BWN_PHY_WRITE(mac, 0x0026, 0xcc00);
	BWN_PHY_WRITE(mac, 0x0030, 0x00c6);
	BWN_WRITE_2(mac, 0x03ec, 0x3f22);

	if (phy->analog == 1)
		BWN_PHY_WRITE(mac, 0x0020, 0x3e1c);
	else
		BWN_PHY_WRITE(mac, 0x0020, 0x301c);

	if (phy->analog == 0)
		BWN_WRITE_2(mac, 0x03e4, 0x3000);

	old_channel = phy->chan;
	bwn_phy_g_switch_chan(mac, 7, 0);

	if (phy->rf_ver != 0x2050) {
		BWN_RF_WRITE(mac, 0x0075, 0x0080);
		BWN_RF_WRITE(mac, 0x0079, 0x0081);
	}

	BWN_RF_WRITE(mac, 0x0050, 0x0020);
	BWN_RF_WRITE(mac, 0x0050, 0x0023);

	if (phy->rf_ver == 0x2050) {
		BWN_RF_WRITE(mac, 0x0050, 0x0020);
		BWN_RF_WRITE(mac, 0x005a, 0x0070);
	}

	BWN_RF_WRITE(mac, 0x005b, 0x007b);
	BWN_RF_WRITE(mac, 0x005c, 0x00b0);
	BWN_RF_SET(mac, 0x007a, 0x0007);

	bwn_phy_g_switch_chan(mac, old_channel, 0);
	BWN_PHY_WRITE(mac, 0x0014, 0x0080);
	BWN_PHY_WRITE(mac, 0x0032, 0x00ca);
	BWN_PHY_WRITE(mac, 0x002a, 0x88a3);

	bwn_phy_g_set_txpwr_sub(mac, &pg->pg_bbatt, &pg->pg_rfatt,
	    pg->pg_txctl);

	if (phy->rf_ver == 0x2050)
		BWN_RF_WRITE(mac, 0x005d, 0x000d);

	BWN_WRITE_2(mac, 0x03e4, (BWN_READ_2(mac, 0x03e4) & 0xffc0) | 0x0004);
}

static void
bwn_loopback_calcgain(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t backup_phy[16] = { 0 };
	uint16_t backup_radio[3];
	uint16_t backup_bband;
	uint16_t i, j, loop_i_max;
	uint16_t trsw_rx;
	uint16_t loop1_outer_done, loop1_inner_done;

	backup_phy[0] = BWN_PHY_READ(mac, BWN_PHY_CRS0);
	backup_phy[1] = BWN_PHY_READ(mac, BWN_PHY_CCKBBANDCFG);
	backup_phy[2] = BWN_PHY_READ(mac, BWN_PHY_RFOVER);
	backup_phy[3] = BWN_PHY_READ(mac, BWN_PHY_RFOVERVAL);
	if (phy->rev != 1) {
		backup_phy[4] = BWN_PHY_READ(mac, BWN_PHY_ANALOGOVER);
		backup_phy[5] = BWN_PHY_READ(mac, BWN_PHY_ANALOGOVERVAL);
	}
	backup_phy[6] = BWN_PHY_READ(mac, BWN_PHY_CCK(0x5a));
	backup_phy[7] = BWN_PHY_READ(mac, BWN_PHY_CCK(0x59));
	backup_phy[8] = BWN_PHY_READ(mac, BWN_PHY_CCK(0x58));
	backup_phy[9] = BWN_PHY_READ(mac, BWN_PHY_CCK(0x0a));
	backup_phy[10] = BWN_PHY_READ(mac, BWN_PHY_CCK(0x03));
	backup_phy[11] = BWN_PHY_READ(mac, BWN_PHY_LO_MASK);
	backup_phy[12] = BWN_PHY_READ(mac, BWN_PHY_LO_CTL);
	backup_phy[13] = BWN_PHY_READ(mac, BWN_PHY_CCK(0x2b));
	backup_phy[14] = BWN_PHY_READ(mac, BWN_PHY_PGACTL);
	backup_phy[15] = BWN_PHY_READ(mac, BWN_PHY_LO_LEAKAGE);
	backup_bband = pg->pg_bbatt.att;
	backup_radio[0] = BWN_RF_READ(mac, 0x52);
	backup_radio[1] = BWN_RF_READ(mac, 0x43);
	backup_radio[2] = BWN_RF_READ(mac, 0x7a);

	BWN_PHY_MASK(mac, BWN_PHY_CRS0, 0x3fff);
	BWN_PHY_SET(mac, BWN_PHY_CCKBBANDCFG, 0x8000);
	BWN_PHY_SET(mac, BWN_PHY_RFOVER, 0x0002);
	BWN_PHY_MASK(mac, BWN_PHY_RFOVERVAL, 0xfffd);
	BWN_PHY_SET(mac, BWN_PHY_RFOVER, 0x0001);
	BWN_PHY_MASK(mac, BWN_PHY_RFOVERVAL, 0xfffe);
	if (phy->rev != 1) {
		BWN_PHY_SET(mac, BWN_PHY_ANALOGOVER, 0x0001);
		BWN_PHY_MASK(mac, BWN_PHY_ANALOGOVERVAL, 0xfffe);
		BWN_PHY_SET(mac, BWN_PHY_ANALOGOVER, 0x0002);
		BWN_PHY_MASK(mac, BWN_PHY_ANALOGOVERVAL, 0xfffd);
	}
	BWN_PHY_SET(mac, BWN_PHY_RFOVER, 0x000c);
	BWN_PHY_SET(mac, BWN_PHY_RFOVERVAL, 0x000c);
	BWN_PHY_SET(mac, BWN_PHY_RFOVER, 0x0030);
	BWN_PHY_SETMASK(mac, BWN_PHY_RFOVERVAL, 0xffcf, 0x10);

	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x5a), 0x0780);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x59), 0xc810);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x58), 0x000d);

	BWN_PHY_SET(mac, BWN_PHY_CCK(0x0a), 0x2000);
	if (phy->rev != 1) {
		BWN_PHY_SET(mac, BWN_PHY_ANALOGOVER, 0x0004);
		BWN_PHY_MASK(mac, BWN_PHY_ANALOGOVERVAL, 0xfffb);
	}
	BWN_PHY_SETMASK(mac, BWN_PHY_CCK(0x03), 0xff9f, 0x40);

	if (phy->rf_rev == 8)
		BWN_RF_WRITE(mac, 0x43, 0x000f);
	else {
		BWN_RF_WRITE(mac, 0x52, 0);
		BWN_RF_SETMASK(mac, 0x43, 0xfff0, 0x9);
	}
	bwn_phy_g_set_bbatt(mac, 11);

	if (phy->rev >= 3)
		BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, 0xc020);
	else
		BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, 0x8020);
	BWN_PHY_WRITE(mac, BWN_PHY_LO_CTL, 0);

	BWN_PHY_SETMASK(mac, BWN_PHY_CCK(0x2b), 0xffc0, 0x01);
	BWN_PHY_SETMASK(mac, BWN_PHY_CCK(0x2b), 0xc0ff, 0x800);

	BWN_PHY_SET(mac, BWN_PHY_RFOVER, 0x0100);
	BWN_PHY_MASK(mac, BWN_PHY_RFOVERVAL, 0xcfff);

	if (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_EXTLNA) {
		if (phy->rev >= 7) {
			BWN_PHY_SET(mac, BWN_PHY_RFOVER, 0x0800);
			BWN_PHY_SET(mac, BWN_PHY_RFOVERVAL, 0x8000);
		}
	}
	BWN_RF_MASK(mac, 0x7a, 0x00f7);

	j = 0;
	loop_i_max = (phy->rf_rev == 8) ? 15 : 9;
	for (i = 0; i < loop_i_max; i++) {
		for (j = 0; j < 16; j++) {
			BWN_RF_WRITE(mac, 0x43, i);
			BWN_PHY_SETMASK(mac, BWN_PHY_RFOVERVAL, 0xf0ff,
			    (j << 8));
			BWN_PHY_SETMASK(mac, BWN_PHY_PGACTL, 0x0fff, 0xa000);
			BWN_PHY_SET(mac, BWN_PHY_PGACTL, 0xf000);
			DELAY(20);
			if (BWN_PHY_READ(mac, BWN_PHY_LO_LEAKAGE) >= 0xdfc)
				goto done0;
		}
	}
done0:
	loop1_outer_done = i;
	loop1_inner_done = j;
	if (j >= 8) {
		BWN_PHY_SET(mac, BWN_PHY_RFOVERVAL, 0x30);
		trsw_rx = 0x1b;
		for (j = j - 8; j < 16; j++) {
			BWN_PHY_SETMASK(mac, BWN_PHY_RFOVERVAL, 0xf0ff, j << 8);
			BWN_PHY_SETMASK(mac, BWN_PHY_PGACTL, 0x0fff, 0xa000);
			BWN_PHY_SET(mac, BWN_PHY_PGACTL, 0xf000);
			DELAY(20);
			trsw_rx -= 3;
			if (BWN_PHY_READ(mac, BWN_PHY_LO_LEAKAGE) >= 0xdfc)
				goto done1;
		}
	} else
		trsw_rx = 0x18;
done1:

	if (phy->rev != 1) {
		BWN_PHY_WRITE(mac, BWN_PHY_ANALOGOVER, backup_phy[4]);
		BWN_PHY_WRITE(mac, BWN_PHY_ANALOGOVERVAL, backup_phy[5]);
	}
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x5a), backup_phy[6]);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x59), backup_phy[7]);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x58), backup_phy[8]);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x0a), backup_phy[9]);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x03), backup_phy[10]);
	BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, backup_phy[11]);
	BWN_PHY_WRITE(mac, BWN_PHY_LO_CTL, backup_phy[12]);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2b), backup_phy[13]);
	BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, backup_phy[14]);

	bwn_phy_g_set_bbatt(mac, backup_bband);

	BWN_RF_WRITE(mac, 0x52, backup_radio[0]);
	BWN_RF_WRITE(mac, 0x43, backup_radio[1]);
	BWN_RF_WRITE(mac, 0x7a, backup_radio[2]);

	BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, backup_phy[2] | 0x0003);
	DELAY(10);
	BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, backup_phy[2]);
	BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, backup_phy[3]);
	BWN_PHY_WRITE(mac, BWN_PHY_CRS0, backup_phy[0]);
	BWN_PHY_WRITE(mac, BWN_PHY_CCKBBANDCFG, backup_phy[1]);

	pg->pg_max_lb_gain =
	    ((loop1_inner_done * 6) - (loop1_outer_done * 4)) - 11;
	pg->pg_trsw_rx_gain = trsw_rx * 2;
}

static uint16_t
bwn_rf_init_bcm2050(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	uint32_t tmp1 = 0, tmp2 = 0;
	uint16_t rcc, i, j, pgactl, cck0, cck1, cck2, cck3, rfover, rfoverval,
	    analogover, analogoverval, crs0, classctl, lomask, loctl, syncctl,
	    radio0, radio1, radio2, reg0, reg1, reg2, radio78, reg, index;
	static const uint8_t rcc_table[] = {
		0x02, 0x03, 0x01, 0x0f,
		0x06, 0x07, 0x05, 0x0f,
		0x0a, 0x0b, 0x09, 0x0f,
		0x0e, 0x0f, 0x0d, 0x0f,
	};

	loctl = lomask = reg0 = classctl = crs0 = analogoverval = analogover =
	    rfoverval = rfover = cck3 = 0;
	radio0 = BWN_RF_READ(mac, 0x43);
	radio1 = BWN_RF_READ(mac, 0x51);
	radio2 = BWN_RF_READ(mac, 0x52);
	pgactl = BWN_PHY_READ(mac, BWN_PHY_PGACTL);
	cck0 = BWN_PHY_READ(mac, BWN_PHY_CCK(0x5a));
	cck1 = BWN_PHY_READ(mac, BWN_PHY_CCK(0x59));
	cck2 = BWN_PHY_READ(mac, BWN_PHY_CCK(0x58));

	if (phy->type == BWN_PHYTYPE_B) {
		cck3 = BWN_PHY_READ(mac, BWN_PHY_CCK(0x30));
		reg0 = BWN_READ_2(mac, 0x3ec);

		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x30), 0xff);
		BWN_WRITE_2(mac, 0x3ec, 0x3f3f);
	} else if (phy->gmode || phy->rev >= 2) {
		rfover = BWN_PHY_READ(mac, BWN_PHY_RFOVER);
		rfoverval = BWN_PHY_READ(mac, BWN_PHY_RFOVERVAL);
		analogover = BWN_PHY_READ(mac, BWN_PHY_ANALOGOVER);
		analogoverval = BWN_PHY_READ(mac, BWN_PHY_ANALOGOVERVAL);
		crs0 = BWN_PHY_READ(mac, BWN_PHY_CRS0);
		classctl = BWN_PHY_READ(mac, BWN_PHY_CLASSCTL);

		BWN_PHY_SET(mac, BWN_PHY_ANALOGOVER, 0x0003);
		BWN_PHY_MASK(mac, BWN_PHY_ANALOGOVERVAL, 0xfffc);
		BWN_PHY_MASK(mac, BWN_PHY_CRS0, 0x7fff);
		BWN_PHY_MASK(mac, BWN_PHY_CLASSCTL, 0xfffc);
		if (BWN_HAS_LOOPBACK(phy)) {
			lomask = BWN_PHY_READ(mac, BWN_PHY_LO_MASK);
			loctl = BWN_PHY_READ(mac, BWN_PHY_LO_CTL);
			if (phy->rev >= 3)
				BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, 0xc020);
			else
				BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, 0x8020);
			BWN_PHY_WRITE(mac, BWN_PHY_LO_CTL, 0);
		}

		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
		    bwn_rf_2050_rfoverval(mac, BWN_PHY_RFOVERVAL,
			BWN_LPD(0, 1, 1)));
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVER,
		    bwn_rf_2050_rfoverval(mac, BWN_PHY_RFOVER, 0));
	}
	BWN_WRITE_2(mac, 0x3e2, BWN_READ_2(mac, 0x3e2) | 0x8000);

	syncctl = BWN_PHY_READ(mac, BWN_PHY_SYNCCTL);
	BWN_PHY_MASK(mac, BWN_PHY_SYNCCTL, 0xff7f);
	reg1 = BWN_READ_2(mac, 0x3e6);
	reg2 = BWN_READ_2(mac, 0x3f4);

	if (phy->analog == 0)
		BWN_WRITE_2(mac, 0x03e6, 0x0122);
	else {
		if (phy->analog >= 2)
			BWN_PHY_SETMASK(mac, BWN_PHY_CCK(0x03), 0xffbf, 0x40);
		BWN_WRITE_2(mac, BWN_CHANNEL_EXT,
		    (BWN_READ_2(mac, BWN_CHANNEL_EXT) | 0x2000));
	}

	reg = BWN_RF_READ(mac, 0x60);
	index = (reg & 0x001e) >> 1;
	rcc = (((rcc_table[index] << 1) | (reg & 0x0001)) | 0x0020);

	if (phy->type == BWN_PHYTYPE_B)
		BWN_RF_WRITE(mac, 0x78, 0x26);
	if (phy->gmode || phy->rev >= 2) {
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
		    bwn_rf_2050_rfoverval(mac, BWN_PHY_RFOVERVAL,
			BWN_LPD(0, 1, 1)));
	}
	BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xbfaf);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2b), 0x1403);
	if (phy->gmode || phy->rev >= 2) {
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
		    bwn_rf_2050_rfoverval(mac, BWN_PHY_RFOVERVAL,
			BWN_LPD(0, 0, 1)));
	}
	BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xbfa0);
	BWN_RF_SET(mac, 0x51, 0x0004);
	if (phy->rf_rev == 8)
		BWN_RF_WRITE(mac, 0x43, 0x1f);
	else {
		BWN_RF_WRITE(mac, 0x52, 0);
		BWN_RF_SETMASK(mac, 0x43, 0xfff0, 0x0009);
	}
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x58), 0);

	for (i = 0; i < 16; i++) {
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x5a), 0x0480);
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x59), 0xc810);
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x58), 0x000d);
		if (phy->gmode || phy->rev >= 2) {
			BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
			    bwn_rf_2050_rfoverval(mac,
				BWN_PHY_RFOVERVAL, BWN_LPD(1, 0, 1)));
		}
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xafb0);
		DELAY(10);
		if (phy->gmode || phy->rev >= 2) {
			BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
			    bwn_rf_2050_rfoverval(mac,
				BWN_PHY_RFOVERVAL, BWN_LPD(1, 0, 1)));
		}
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xefb0);
		DELAY(10);
		if (phy->gmode || phy->rev >= 2) {
			BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
			    bwn_rf_2050_rfoverval(mac,
				BWN_PHY_RFOVERVAL, BWN_LPD(1, 0, 0)));
		}
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xfff0);
		DELAY(20);
		tmp1 += BWN_PHY_READ(mac, BWN_PHY_LO_LEAKAGE);
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x58), 0);
		if (phy->gmode || phy->rev >= 2) {
			BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
			    bwn_rf_2050_rfoverval(mac,
				BWN_PHY_RFOVERVAL, BWN_LPD(1, 0, 1)));
		}
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xafb0);
	}
	DELAY(10);

	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x58), 0);
	tmp1++;
	tmp1 >>= 9;

	for (i = 0; i < 16; i++) {
		radio78 = (BWN_BITREV4(i) << 1) | 0x0020;
		BWN_RF_WRITE(mac, 0x78, radio78);
		DELAY(10);
		for (j = 0; j < 16; j++) {
			BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x5a), 0x0d80);
			BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x59), 0xc810);
			BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x58), 0x000d);
			if (phy->gmode || phy->rev >= 2) {
				BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
				    bwn_rf_2050_rfoverval(mac,
					BWN_PHY_RFOVERVAL, BWN_LPD(1, 0, 1)));
			}
			BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xafb0);
			DELAY(10);
			if (phy->gmode || phy->rev >= 2) {
				BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
				    bwn_rf_2050_rfoverval(mac,
					BWN_PHY_RFOVERVAL, BWN_LPD(1, 0, 1)));
			}
			BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xefb0);
			DELAY(10);
			if (phy->gmode || phy->rev >= 2) {
				BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
				    bwn_rf_2050_rfoverval(mac,
					BWN_PHY_RFOVERVAL, BWN_LPD(1, 0, 0)));
			}
			BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xfff0);
			DELAY(10);
			tmp2 += BWN_PHY_READ(mac, BWN_PHY_LO_LEAKAGE);
			BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x58), 0);
			if (phy->gmode || phy->rev >= 2) {
				BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL,
				    bwn_rf_2050_rfoverval(mac,
					BWN_PHY_RFOVERVAL, BWN_LPD(1, 0, 1)));
			}
			BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xafb0);
		}
		tmp2++;
		tmp2 >>= 8;
		if (tmp1 < tmp2)
			break;
	}

	BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, pgactl);
	BWN_RF_WRITE(mac, 0x51, radio1);
	BWN_RF_WRITE(mac, 0x52, radio2);
	BWN_RF_WRITE(mac, 0x43, radio0);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x5a), cck0);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x59), cck1);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x58), cck2);
	BWN_WRITE_2(mac, 0x3e6, reg1);
	if (phy->analog != 0)
		BWN_WRITE_2(mac, 0x3f4, reg2);
	BWN_PHY_WRITE(mac, BWN_PHY_SYNCCTL, syncctl);
	bwn_spu_workaround(mac, phy->chan);
	if (phy->type == BWN_PHYTYPE_B) {
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x30), cck3);
		BWN_WRITE_2(mac, 0x3ec, reg0);
	} else if (phy->gmode) {
		BWN_WRITE_2(mac, BWN_PHY_RADIO,
			    BWN_READ_2(mac, BWN_PHY_RADIO)
			    & 0x7fff);
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, rfover);
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, rfoverval);
		BWN_PHY_WRITE(mac, BWN_PHY_ANALOGOVER, analogover);
		BWN_PHY_WRITE(mac, BWN_PHY_ANALOGOVERVAL,
			      analogoverval);
		BWN_PHY_WRITE(mac, BWN_PHY_CRS0, crs0);
		BWN_PHY_WRITE(mac, BWN_PHY_CLASSCTL, classctl);
		if (BWN_HAS_LOOPBACK(phy)) {
			BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, lomask);
			BWN_PHY_WRITE(mac, BWN_PHY_LO_CTL, loctl);
		}
	}

	return ((i > 15) ? radio78 : rcc);
}

static void
bwn_phy_init_b6(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t offset, val;
	uint8_t old_channel;

	KASSERT(!(phy->rf_rev == 6 || phy->rf_rev == 7),
	    ("%s:%d: fail", __func__, __LINE__));

	BWN_PHY_WRITE(mac, 0x003e, 0x817a);
	BWN_RF_WRITE(mac, 0x007a, BWN_RF_READ(mac, 0x007a) | 0x0058);
	if (phy->rf_rev == 4 || phy->rf_rev == 5) {
		BWN_RF_WRITE(mac, 0x51, 0x37);
		BWN_RF_WRITE(mac, 0x52, 0x70);
		BWN_RF_WRITE(mac, 0x53, 0xb3);
		BWN_RF_WRITE(mac, 0x54, 0x9b);
		BWN_RF_WRITE(mac, 0x5a, 0x88);
		BWN_RF_WRITE(mac, 0x5b, 0x88);
		BWN_RF_WRITE(mac, 0x5d, 0x88);
		BWN_RF_WRITE(mac, 0x5e, 0x88);
		BWN_RF_WRITE(mac, 0x7d, 0x88);
		bwn_hf_write(mac,
		    bwn_hf_read(mac) | BWN_HF_TSSI_RESET_PSM_WORKAROUN);
	}
	if (phy->rf_rev == 8) {
		BWN_RF_WRITE(mac, 0x51, 0);
		BWN_RF_WRITE(mac, 0x52, 0x40);
		BWN_RF_WRITE(mac, 0x53, 0xb7);
		BWN_RF_WRITE(mac, 0x54, 0x98);
		BWN_RF_WRITE(mac, 0x5a, 0x88);
		BWN_RF_WRITE(mac, 0x5b, 0x6b);
		BWN_RF_WRITE(mac, 0x5c, 0x0f);
		if (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_ALTIQ) {
			BWN_RF_WRITE(mac, 0x5d, 0xfa);
			BWN_RF_WRITE(mac, 0x5e, 0xd8);
		} else {
			BWN_RF_WRITE(mac, 0x5d, 0xf5);
			BWN_RF_WRITE(mac, 0x5e, 0xb8);
		}
		BWN_RF_WRITE(mac, 0x0073, 0x0003);
		BWN_RF_WRITE(mac, 0x007d, 0x00a8);
		BWN_RF_WRITE(mac, 0x007c, 0x0001);
		BWN_RF_WRITE(mac, 0x007e, 0x0008);
	}
	for (val = 0x1e1f, offset = 0x0088; offset < 0x0098; offset++) {
		BWN_PHY_WRITE(mac, offset, val);
		val -= 0x0202;
	}
	for (val = 0x3e3f, offset = 0x0098; offset < 0x00a8; offset++) {
		BWN_PHY_WRITE(mac, offset, val);
		val -= 0x0202;
	}
	for (val = 0x2120, offset = 0x00a8; offset < 0x00c8; offset++) {
		BWN_PHY_WRITE(mac, offset, (val & 0x3f3f));
		val += 0x0202;
	}
	if (phy->type == BWN_PHYTYPE_G) {
		BWN_RF_SET(mac, 0x007a, 0x0020);
		BWN_RF_SET(mac, 0x0051, 0x0004);
		BWN_PHY_SET(mac, 0x0802, 0x0100);
		BWN_PHY_SET(mac, 0x042b, 0x2000);
		BWN_PHY_WRITE(mac, 0x5b, 0);
		BWN_PHY_WRITE(mac, 0x5c, 0);
	}

	old_channel = phy->chan;
	bwn_phy_g_switch_chan(mac, (old_channel >= 8) ? 1 : 13, 0);

	BWN_RF_WRITE(mac, 0x0050, 0x0020);
	BWN_RF_WRITE(mac, 0x0050, 0x0023);
	DELAY(40);
	if (phy->rf_rev < 6 || phy->rf_rev == 8) {
		BWN_RF_WRITE(mac, 0x7c, BWN_RF_READ(mac, 0x7c) | 0x0002);
		BWN_RF_WRITE(mac, 0x50, 0x20);
	}
	if (phy->rf_rev <= 2) {
		BWN_RF_WRITE(mac, 0x7c, 0x20);
		BWN_RF_WRITE(mac, 0x5a, 0x70);
		BWN_RF_WRITE(mac, 0x5b, 0x7b);
		BWN_RF_WRITE(mac, 0x5c, 0xb0);
	}
	BWN_RF_SETMASK(mac, 0x007a, 0x00f8, 0x0007);

	bwn_phy_g_switch_chan(mac, old_channel, 0);

	BWN_PHY_WRITE(mac, 0x0014, 0x0200);
	if (phy->rf_rev >= 6)
		BWN_PHY_WRITE(mac, 0x2a, 0x88c2);
	else
		BWN_PHY_WRITE(mac, 0x2a, 0x8ac0);
	BWN_PHY_WRITE(mac, 0x0038, 0x0668);
	bwn_phy_g_set_txpwr_sub(mac, &pg->pg_bbatt, &pg->pg_rfatt,
	    pg->pg_txctl);
	if (phy->rf_rev <= 5)
		BWN_PHY_SETMASK(mac, 0x5d, 0xff80, 0x0003);
	if (phy->rf_rev <= 2)
		BWN_RF_WRITE(mac, 0x005d, 0x000d);

	if (phy->analog == 4) {
		BWN_WRITE_2(mac, 0x3e4, 9);
		BWN_PHY_MASK(mac, 0x61, 0x0fff);
	} else
		BWN_PHY_SETMASK(mac, 0x0002, 0xffc0, 0x0004);
	if (phy->type == BWN_PHYTYPE_B)
		KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	else if (phy->type == BWN_PHYTYPE_G)
		BWN_WRITE_2(mac, 0x03e6, 0x0);
}

static void
bwn_phy_init_a(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_softc *sc = mac->mac_sc;

	KASSERT(phy->type == BWN_PHYTYPE_A || phy->type == BWN_PHYTYPE_G,
	    ("%s:%d: fail", __func__, __LINE__));

	if (phy->rev >= 6) {
		if (phy->type == BWN_PHYTYPE_A)
			BWN_PHY_MASK(mac, BWN_PHY_OFDM(0x1b), ~0x1000);
		if (BWN_PHY_READ(mac, BWN_PHY_ENCORE) & BWN_PHY_ENCORE_EN)
			BWN_PHY_SET(mac, BWN_PHY_ENCORE, 0x0010);
		else
			BWN_PHY_MASK(mac, BWN_PHY_ENCORE, ~0x1010);
	}

	bwn_wa_init(mac);

	if (phy->type == BWN_PHYTYPE_G &&
	    (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_PACTRL))
		BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x6e), 0xe000, 0x3cf);
}

static void
bwn_wa_write_noisescale(struct bwn_mac *mac, const uint16_t *nst)
{
	int i;

	for (i = 0; i < BWN_TAB_NOISESCALE_SIZE; i++)
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_NOISESCALE, i, nst[i]);
}

static void
bwn_wa_agc(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;

	if (phy->rev == 1) {
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC1_R1, 0, 254);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC1_R1, 1, 13);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC1_R1, 2, 19);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC1_R1, 3, 25);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC2, 0, 0x2710);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC2, 1, 0x9b83);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC2, 2, 0x9b83);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC2, 3, 0x0f8d);
		BWN_PHY_WRITE(mac, BWN_PHY_LMS, 4);
	} else {
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC1, 0, 254);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC1, 1, 13);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC1, 2, 19);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC1, 3, 25);
	}

	BWN_PHY_SETMASK(mac, BWN_PHY_CCKSHIFTBITS_WA, (uint16_t)~0xff00,
	    0x5700);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x1a), ~0x007f, 0x000f);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x1a), ~0x3f80, 0x2b80);
	BWN_PHY_SETMASK(mac, BWN_PHY_ANTWRSETT, 0xf0ff, 0x0300);
	BWN_RF_SET(mac, 0x7a, 0x0008);
	BWN_PHY_SETMASK(mac, BWN_PHY_N1P1GAIN, ~0x000f, 0x0008);
	BWN_PHY_SETMASK(mac, BWN_PHY_P1P2GAIN, ~0x0f00, 0x0600);
	BWN_PHY_SETMASK(mac, BWN_PHY_N1N2GAIN, ~0x0f00, 0x0700);
	BWN_PHY_SETMASK(mac, BWN_PHY_N1P1GAIN, ~0x0f00, 0x0100);
	if (phy->rev == 1)
		BWN_PHY_SETMASK(mac, BWN_PHY_N1N2GAIN, ~0x000f, 0x0007);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x88), ~0x00ff, 0x001c);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x88), ~0x3f00, 0x0200);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x96), ~0x00ff, 0x001c);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x89), ~0x00ff, 0x0020);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x89), ~0x3f00, 0x0200);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x82), ~0x00ff, 0x002e);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x96), (uint16_t)~0xff00, 0x1a00);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x81), ~0x00ff, 0x0028);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x81), (uint16_t)~0xff00, 0x2c00);
	if (phy->rev == 1) {
		BWN_PHY_WRITE(mac, BWN_PHY_PEAK_COUNT, 0x092b);
		BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x1b), ~0x001e, 0x0002);
	} else {
		BWN_PHY_MASK(mac, BWN_PHY_OFDM(0x1b), ~0x001e);
		BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0x1f), 0x287a);
		BWN_PHY_SETMASK(mac, BWN_PHY_LPFGAINCTL, ~0x000f, 0x0004);
		if (phy->rev >= 6) {
			BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0x22), 0x287a);
			BWN_PHY_SETMASK(mac, BWN_PHY_LPFGAINCTL,
			    (uint16_t)~0xf000, 0x3000);
		}
	}
	BWN_PHY_SETMASK(mac, BWN_PHY_DIVSRCHIDX, 0x8080, 0x7874);
	BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0x8e), 0x1c00);
	if (phy->rev == 1) {
		BWN_PHY_SETMASK(mac, BWN_PHY_DIVP1P2GAIN, ~0x0f00, 0x0600);
		BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0x8b), 0x005e);
		BWN_PHY_SETMASK(mac, BWN_PHY_ANTWRSETT, ~0x00ff, 0x001e);
		BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0x8d), 0x0002);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC3_R1, 0, 0);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC3_R1, 1, 7);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC3_R1, 2, 16);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC3_R1, 3, 28);
	} else {
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC3, 0, 0);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC3, 1, 7);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC3, 2, 16);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC3, 3, 28);
	}
	if (phy->rev >= 6) {
		BWN_PHY_MASK(mac, BWN_PHY_OFDM(0x26), ~0x0003);
		BWN_PHY_MASK(mac, BWN_PHY_OFDM(0x26), ~0x1000);
	}
	BWN_PHY_READ(mac, BWN_PHY_VERSION_OFDM);
}

static void
bwn_wa_grev1(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	int i;
	static const uint16_t bwn_tab_finefreqg[] = BWN_TAB_FINEFREQ_G;
	static const uint32_t bwn_tab_retard[] = BWN_TAB_RETARD;
	static const uint32_t bwn_tab_rotor[] = BWN_TAB_ROTOR;

	KASSERT(phy->type == BWN_PHYTYPE_G, ("%s fail", __func__));

	/* init CRSTHRES and ANTDWELL */
	if (phy->rev == 1) {
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES1_R1, 0x4f19);
	} else if (phy->rev == 2) {
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES1, 0x1861);
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES2, 0x0271);
		BWN_PHY_SET(mac, BWN_PHY_ANTDWELL, 0x0800);
	} else {
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES1, 0x0098);
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES2, 0x0070);
		BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0xc9), 0x0080);
		BWN_PHY_SET(mac, BWN_PHY_ANTDWELL, 0x0800);
	}
	BWN_PHY_SETMASK(mac, BWN_PHY_CRS0, ~0x03c0, 0xd000);
	BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0x2c), 0x005a);
	BWN_PHY_WRITE(mac, BWN_PHY_CCKSHIFTBITS, 0x0026);

	/* XXX support PHY-A??? */
	for (i = 0; i < N(bwn_tab_finefreqg); i++)
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_DACRFPABB, i,
		    bwn_tab_finefreqg[i]);

	/* XXX support PHY-A??? */
	if (phy->rev == 1)
		for (i = 0; i < N(bwn_tab_noise_g1); i++)
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC2, i,
			    bwn_tab_noise_g1[i]);
	else
		for (i = 0; i < N(bwn_tab_noise_g2); i++)
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC2, i,
			    bwn_tab_noise_g2[i]);


	for (i = 0; i < N(bwn_tab_rotor); i++)
		bwn_ofdmtab_write_4(mac, BWN_OFDMTAB_ROTOR, i,
		    bwn_tab_rotor[i]);

	/* XXX support PHY-A??? */
	if (phy->rev >= 6) {
		if (BWN_PHY_READ(mac, BWN_PHY_ENCORE) &
		    BWN_PHY_ENCORE_EN)
			bwn_wa_write_noisescale(mac, bwn_tab_noisescale_g3);
		else
			bwn_wa_write_noisescale(mac, bwn_tab_noisescale_g2);
	} else
		bwn_wa_write_noisescale(mac, bwn_tab_noisescale_g1);

	for (i = 0; i < N(bwn_tab_retard); i++)
		bwn_ofdmtab_write_4(mac, BWN_OFDMTAB_ADVRETARD, i,
		    bwn_tab_retard[i]);

	if (phy->rev == 1) {
		for (i = 0; i < 16; i++)
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_WRSSI_R1,
			    i, 0x0020);
	} else {
		for (i = 0; i < 32; i++)
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_WRSSI, i, 0x0820);
	}

	bwn_wa_agc(mac);
}

static void
bwn_wa_grev26789(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	int i;
	static const uint16_t bwn_tab_sigmasqr2[] = BWN_TAB_SIGMASQR2;
	uint16_t ofdmrev;

	KASSERT(phy->type == BWN_PHYTYPE_G, ("%s fail", __func__));

	bwn_gtab_write(mac, BWN_GTAB_ORIGTR, 0, 0xc480);

	/* init CRSTHRES and ANTDWELL */
	if (phy->rev == 1)
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES1_R1, 0x4f19);
	else if (phy->rev == 2) {
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES1, 0x1861);
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES2, 0x0271);
		BWN_PHY_SET(mac, BWN_PHY_ANTDWELL, 0x0800);
	} else {
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES1, 0x0098);
		BWN_PHY_WRITE(mac, BWN_PHY_CRSTHRES2, 0x0070);
		BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0xc9), 0x0080);
		BWN_PHY_SET(mac, BWN_PHY_ANTDWELL, 0x0800);
	}

	for (i = 0; i < 64; i++)
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_RSSI, i, i);

	/* XXX support PHY-A??? */
	if (phy->rev == 1)
		for (i = 0; i < N(bwn_tab_noise_g1); i++)
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC2, i,
			    bwn_tab_noise_g1[i]);
	else
		for (i = 0; i < N(bwn_tab_noise_g2); i++)
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_AGC2, i,
			    bwn_tab_noise_g2[i]);

	/* XXX support PHY-A??? */
	if (phy->rev >= 6) {
		if (BWN_PHY_READ(mac, BWN_PHY_ENCORE) &
		    BWN_PHY_ENCORE_EN)
			bwn_wa_write_noisescale(mac, bwn_tab_noisescale_g3);
		else
			bwn_wa_write_noisescale(mac, bwn_tab_noisescale_g2);
	} else
		bwn_wa_write_noisescale(mac, bwn_tab_noisescale_g1);

	for (i = 0; i < N(bwn_tab_sigmasqr2); i++)
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_MINSIGSQ, i,
		    bwn_tab_sigmasqr2[i]);

	if (phy->rev == 1) {
		for (i = 0; i < 16; i++)
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_WRSSI_R1, i,
			    0x0020);
	} else {
		for (i = 0; i < 32; i++)
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_WRSSI, i, 0x0820);
	}

	bwn_wa_agc(mac);

	ofdmrev = BWN_PHY_READ(mac, BWN_PHY_VERSION_OFDM) & BWN_PHYVER_VERSION;
	if (ofdmrev > 2) {
		if (phy->type == BWN_PHYTYPE_A)
			BWN_PHY_WRITE(mac, BWN_PHY_PWRDOWN, 0x1808);
		else
			BWN_PHY_WRITE(mac, BWN_PHY_PWRDOWN, 0x1000);
	} else {
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_DAC, 3, 0x1044);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_DAC, 4, 0x7201);
		bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_DAC, 6, 0x0040);
	}

	bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_UNKNOWN_0F, 2, 15);
	bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_UNKNOWN_0F, 3, 20);
}

static void
bwn_wa_init(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_softc *sc = mac->mac_sc;

	KASSERT(phy->type == BWN_PHYTYPE_G, ("%s fail", __func__));

	switch (phy->rev) {
	case 1:
		bwn_wa_grev1(mac);
		break;
	case 2:
	case 6:
	case 7:
	case 8:
	case 9:
		bwn_wa_grev26789(mac);
		break;
	default:
		KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	}

	if (siba_get_pci_subvendor(sc->sc_dev) != SIBA_BOARDVENDOR_BCM ||
	    siba_get_pci_subdevice(sc->sc_dev) != SIBA_BOARD_BU4306 ||
	    siba_get_pci_revid(sc->sc_dev) != 0x17) {
		if (phy->rev < 2) {
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX_R1, 1,
			    0x0002);
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX_R1, 2,
			    0x0001);
		} else {
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX, 1, 0x0002);
			bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX, 2, 0x0001);
			if ((siba_sprom_get_bf_lo(sc->sc_dev) &
			     BWN_BFL_EXTLNA) &&
			    (phy->rev >= 7)) {
				BWN_PHY_MASK(mac, BWN_PHY_EXTG(0x11), 0xf7ff);
				bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX,
				    0x0020, 0x0001);
				bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX,
				    0x0021, 0x0001);
				bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX,
				    0x0022, 0x0001);
				bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX,
				    0x0023, 0x0000);
				bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX,
				    0x0000, 0x0000);
				bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_GAINX,
				    0x0003, 0x0002);
			}
		}
	}
	if (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_FEM) {
		BWN_PHY_WRITE(mac, BWN_PHY_GTABCTL, 0x3120);
		BWN_PHY_WRITE(mac, BWN_PHY_GTABDATA, 0xc480);
	}

	bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_UNKNOWN_11, 0, 0);
	bwn_ofdmtab_write_2(mac, BWN_OFDMTAB_UNKNOWN_11, 1, 0);
}

static void
bwn_ofdmtab_write_2(struct bwn_mac *mac, uint16_t table, uint16_t offset,
    uint16_t value)
{
	struct bwn_phy_g *pg = &mac->mac_phy.phy_g;
	uint16_t addr;

	addr = table + offset;
	if ((pg->pg_ofdmtab_dir != BWN_OFDMTAB_DIR_WRITE) ||
	    (addr - 1 != pg->pg_ofdmtab_addr)) {
		BWN_PHY_WRITE(mac, BWN_PHY_OTABLECTL, addr);
		pg->pg_ofdmtab_dir = BWN_OFDMTAB_DIR_WRITE;
	}
	pg->pg_ofdmtab_addr = addr;
	BWN_PHY_WRITE(mac, BWN_PHY_OTABLEI, value);
}

static void
bwn_ofdmtab_write_4(struct bwn_mac *mac, uint16_t table, uint16_t offset,
    uint32_t value)
{
	struct bwn_phy_g *pg = &mac->mac_phy.phy_g;
	uint16_t addr;

	addr = table + offset;
	if ((pg->pg_ofdmtab_dir != BWN_OFDMTAB_DIR_WRITE) ||
	    (addr - 1 != pg->pg_ofdmtab_addr)) {
		BWN_PHY_WRITE(mac, BWN_PHY_OTABLECTL, addr);
		pg->pg_ofdmtab_dir = BWN_OFDMTAB_DIR_WRITE;
	}
	pg->pg_ofdmtab_addr = addr;

	BWN_PHY_WRITE(mac, BWN_PHY_OTABLEI, value);
	BWN_PHY_WRITE(mac, BWN_PHY_OTABLEQ, (value >> 16));
}

static void
bwn_gtab_write(struct bwn_mac *mac, uint16_t table, uint16_t offset,
    uint16_t value)
{

	BWN_PHY_WRITE(mac, BWN_PHY_GTABCTL, table + offset);
	BWN_PHY_WRITE(mac, BWN_PHY_GTABDATA, value);
}

static void
bwn_dummy_transmission(struct bwn_mac *mac, int ofdm, int paon)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_softc *sc = mac->mac_sc;
	unsigned int i, max_loop;
	uint16_t value;
	uint32_t buffer[5] = {
		0x00000000, 0x00d40000, 0x00000000, 0x01000000, 0x00000000
	};

	if (ofdm) {
		max_loop = 0x1e;
		buffer[0] = 0x000201cc;
	} else {
		max_loop = 0xfa;
		buffer[0] = 0x000b846e;
	}

	BWN_ASSERT_LOCKED(mac->mac_sc);

	for (i = 0; i < 5; i++)
		bwn_ram_write(mac, i * 4, buffer[i]);

	BWN_WRITE_2(mac, 0x0568, 0x0000);
	BWN_WRITE_2(mac, 0x07c0,
	    (siba_get_revid(sc->sc_dev) < 11) ? 0x0000 : 0x0100);
	value = ((phy->type == BWN_PHYTYPE_A) ? 0x41 : 0x40);
	BWN_WRITE_2(mac, 0x050c, value);
	if (phy->type == BWN_PHYTYPE_LP)
		BWN_WRITE_2(mac, 0x0514, 0x1a02);
	BWN_WRITE_2(mac, 0x0508, 0x0000);
	BWN_WRITE_2(mac, 0x050a, 0x0000);
	BWN_WRITE_2(mac, 0x054c, 0x0000);
	BWN_WRITE_2(mac, 0x056a, 0x0014);
	BWN_WRITE_2(mac, 0x0568, 0x0826);
	BWN_WRITE_2(mac, 0x0500, 0x0000);
	if (phy->type == BWN_PHYTYPE_LP)
		BWN_WRITE_2(mac, 0x0502, 0x0050);
	else
		BWN_WRITE_2(mac, 0x0502, 0x0030);

	if (phy->rf_ver == 0x2050 && phy->rf_rev <= 0x5)
		BWN_RF_WRITE(mac, 0x0051, 0x0017);
	for (i = 0x00; i < max_loop; i++) {
		value = BWN_READ_2(mac, 0x050e);
		if (value & 0x0080)
			break;
		DELAY(10);
	}
	for (i = 0x00; i < 0x0a; i++) {
		value = BWN_READ_2(mac, 0x050e);
		if (value & 0x0400)
			break;
		DELAY(10);
	}
	for (i = 0x00; i < 0x19; i++) {
		value = BWN_READ_2(mac, 0x0690);
		if (!(value & 0x0100))
			break;
		DELAY(10);
	}
	if (phy->rf_ver == 0x2050 && phy->rf_rev <= 0x5)
		BWN_RF_WRITE(mac, 0x0051, 0x0037);
}

static void
bwn_ram_write(struct bwn_mac *mac, uint16_t offset, uint32_t val)
{
	uint32_t macctl;

	KASSERT(offset % 4 == 0, ("%s:%d: fail", __func__, __LINE__));

	macctl = BWN_READ_4(mac, BWN_MACCTL);
	if (macctl & BWN_MACCTL_BIGENDIAN)
		printf("TODO: need swap\n");

	BWN_WRITE_4(mac, BWN_RAM_CONTROL, offset);
	BWN_BARRIER(mac, BUS_SPACE_BARRIER_WRITE);
	BWN_WRITE_4(mac, BWN_RAM_DATA, val);
}

static void
bwn_lo_write(struct bwn_mac *mac, struct bwn_loctl *ctl)
{
	uint16_t value;

	KASSERT(mac->mac_phy.type == BWN_PHYTYPE_G,
	    ("%s:%d: fail", __func__, __LINE__));

	value = (uint8_t) (ctl->q);
	value |= ((uint8_t) (ctl->i)) << 8;
	BWN_PHY_WRITE(mac, BWN_PHY_LO_CTL, value);
}

static uint16_t
bwn_lo_calcfeed(struct bwn_mac *mac,
    uint16_t lna, uint16_t pga, uint16_t trsw_rx)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t rfover;
	uint16_t feedthrough;

	if (phy->gmode) {
		lna <<= BWN_PHY_RFOVERVAL_LNA_SHIFT;
		pga <<= BWN_PHY_RFOVERVAL_PGA_SHIFT;

		KASSERT((lna & ~BWN_PHY_RFOVERVAL_LNA) == 0,
		    ("%s:%d: fail", __func__, __LINE__));
		KASSERT((pga & ~BWN_PHY_RFOVERVAL_PGA) == 0,
		    ("%s:%d: fail", __func__, __LINE__));

		trsw_rx &= (BWN_PHY_RFOVERVAL_TRSWRX | BWN_PHY_RFOVERVAL_BW);

		rfover = BWN_PHY_RFOVERVAL_UNK | pga | lna | trsw_rx;
		if ((siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_EXTLNA) &&
		    phy->rev > 6)
			rfover |= BWN_PHY_RFOVERVAL_EXTLNA;

		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xe300);
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, rfover);
		DELAY(10);
		rfover |= BWN_PHY_RFOVERVAL_BW_LBW;
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, rfover);
		DELAY(10);
		rfover |= BWN_PHY_RFOVERVAL_BW_LPF;
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, rfover);
		DELAY(10);
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xf300);
	} else {
		pga |= BWN_PHY_PGACTL_UNKNOWN;
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, pga);
		DELAY(10);
		pga |= BWN_PHY_PGACTL_LOWBANDW;
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, pga);
		DELAY(10);
		pga |= BWN_PHY_PGACTL_LPF;
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, pga);
	}
	DELAY(21);
	feedthrough = BWN_PHY_READ(mac, BWN_PHY_LO_LEAKAGE);

	return (feedthrough);
}

static uint16_t
bwn_lo_txctl_regtable(struct bwn_mac *mac,
    uint16_t *value, uint16_t *pad_mix_gain)
{
	struct bwn_phy *phy = &mac->mac_phy;
	uint16_t reg, v, padmix;

	if (phy->type == BWN_PHYTYPE_B) {
		v = 0x30;
		if (phy->rf_rev <= 5) {
			reg = 0x43;
			padmix = 0;
		} else {
			reg = 0x52;
			padmix = 5;
		}
	} else {
		if (phy->rev >= 2 && phy->rf_rev == 8) {
			reg = 0x43;
			v = 0x10;
			padmix = 2;
		} else {
			reg = 0x52;
			v = 0x30;
			padmix = 5;
		}
	}
	if (value)
		*value = v;
	if (pad_mix_gain)
		*pad_mix_gain = padmix;

	return (reg);
}

static void
bwn_lo_measure_txctl_values(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_txpwr_loctl *lo = &pg->pg_loctl;
	uint16_t reg, mask;
	uint16_t trsw_rx, pga;
	uint16_t rf_pctl_reg;

	static const uint8_t tx_bias_values[] = {
		0x09, 0x08, 0x0a, 0x01, 0x00,
		0x02, 0x05, 0x04, 0x06,
	};
	static const uint8_t tx_magn_values[] = {
		0x70, 0x40,
	};

	if (!BWN_HAS_LOOPBACK(phy)) {
		rf_pctl_reg = 6;
		trsw_rx = 2;
		pga = 0;
	} else {
		int lb_gain;

		trsw_rx = 0;
		lb_gain = pg->pg_max_lb_gain / 2;
		if (lb_gain > 10) {
			rf_pctl_reg = 0;
			pga = abs(10 - lb_gain) / 6;
			pga = MIN(MAX(pga, 0), 15);
		} else {
			int cmp_val;
			int tmp;

			pga = 0;
			cmp_val = 0x24;
			if ((phy->rev >= 2) &&
			    (phy->rf_ver == 0x2050) && (phy->rf_rev == 8))
				cmp_val = 0x3c;
			tmp = lb_gain;
			if ((10 - lb_gain) < cmp_val)
				tmp = (10 - lb_gain);
			if (tmp < 0)
				tmp += 6;
			else
				tmp += 3;
			cmp_val /= 4;
			tmp /= 4;
			if (tmp >= cmp_val)
				rf_pctl_reg = cmp_val;
			else
				rf_pctl_reg = tmp;
		}
	}
	BWN_RF_SETMASK(mac, 0x43, 0xfff0, rf_pctl_reg);
	bwn_phy_g_set_bbatt(mac, 2);

	reg = bwn_lo_txctl_regtable(mac, &mask, NULL);
	mask = ~mask;
	BWN_RF_MASK(mac, reg, mask);

	if (BWN_HAS_TXMAG(phy)) {
		int i, j;
		int feedthrough;
		int min_feedth = 0xffff;
		uint8_t tx_magn, tx_bias;

		for (i = 0; i < N(tx_magn_values); i++) {
			tx_magn = tx_magn_values[i];
			BWN_RF_SETMASK(mac, 0x52, 0xff0f, tx_magn);
			for (j = 0; j < N(tx_bias_values); j++) {
				tx_bias = tx_bias_values[j];
				BWN_RF_SETMASK(mac, 0x52, 0xfff0, tx_bias);
				feedthrough = bwn_lo_calcfeed(mac, 0, pga,
				    trsw_rx);
				if (feedthrough < min_feedth) {
					lo->tx_bias = tx_bias;
					lo->tx_magn = tx_magn;
					min_feedth = feedthrough;
				}
				if (lo->tx_bias == 0)
					break;
			}
			BWN_RF_WRITE(mac, 0x52,
					  (BWN_RF_READ(mac, 0x52)
					   & 0xff00) | lo->tx_bias | lo->
					  tx_magn);
		}
	} else {
		lo->tx_magn = 0;
		lo->tx_bias = 0;
		BWN_RF_MASK(mac, 0x52, 0xfff0);
	}

	BWN_GETTIME(lo->txctl_measured_time);
}

static void
bwn_lo_get_powervector(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_txpwr_loctl *lo = &pg->pg_loctl;
	int i;
	uint64_t tmp;
	uint64_t power_vector = 0;

	for (i = 0; i < 8; i += 2) {
		tmp = bwn_shm_read_2(mac, BWN_SHARED, 0x310 + i);
		power_vector |= (tmp << (i * 8));
		bwn_shm_write_2(mac, BWN_SHARED, 0x310 + i, 0);
	}
	if (power_vector)
		lo->power_vector = power_vector;

	BWN_GETTIME(lo->pwr_vec_read_time);
}

static void
bwn_lo_measure_gain_values(struct bwn_mac *mac, int16_t max_rx_gain,
    int use_trsw_rx)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	uint16_t tmp;

	if (max_rx_gain < 0)
		max_rx_gain = 0;

	if (BWN_HAS_LOOPBACK(phy)) {
		int trsw_rx = 0;
		int trsw_rx_gain;

		if (use_trsw_rx) {
			trsw_rx_gain = pg->pg_trsw_rx_gain / 2;
			if (max_rx_gain >= trsw_rx_gain) {
				trsw_rx_gain = max_rx_gain - trsw_rx_gain;
				trsw_rx = 0x20;
			}
		} else
			trsw_rx_gain = max_rx_gain;
		if (trsw_rx_gain < 9) {
			pg->pg_lna_lod_gain = 0;
		} else {
			pg->pg_lna_lod_gain = 1;
			trsw_rx_gain -= 8;
		}
		trsw_rx_gain = MIN(MAX(trsw_rx_gain, 0), 0x2d);
		pg->pg_pga_gain = trsw_rx_gain / 3;
		if (pg->pg_pga_gain >= 5) {
			pg->pg_pga_gain -= 5;
			pg->pg_lna_gain = 2;
		} else
			pg->pg_lna_gain = 0;
	} else {
		pg->pg_lna_gain = 0;
		pg->pg_trsw_rx_gain = 0x20;
		if (max_rx_gain >= 0x14) {
			pg->pg_lna_lod_gain = 1;
			pg->pg_pga_gain = 2;
		} else if (max_rx_gain >= 0x12) {
			pg->pg_lna_lod_gain = 1;
			pg->pg_pga_gain = 1;
		} else if (max_rx_gain >= 0xf) {
			pg->pg_lna_lod_gain = 1;
			pg->pg_pga_gain = 0;
		} else {
			pg->pg_lna_lod_gain = 0;
			pg->pg_pga_gain = 0;
		}
	}

	tmp = BWN_RF_READ(mac, 0x7a);
	if (pg->pg_lna_lod_gain == 0)
		tmp &= ~0x0008;
	else
		tmp |= 0x0008;
	BWN_RF_WRITE(mac, 0x7a, tmp);
}

static void
bwn_lo_save(struct bwn_mac *mac, struct bwn_lo_g_value *sav)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_txpwr_loctl *lo = &pg->pg_loctl;
	struct timespec ts;
	uint16_t tmp;

	if (bwn_has_hwpctl(mac)) {
		sav->phy_lomask = BWN_PHY_READ(mac, BWN_PHY_LO_MASK);
		sav->phy_extg = BWN_PHY_READ(mac, BWN_PHY_EXTG(0x01));
		sav->phy_dacctl_hwpctl = BWN_PHY_READ(mac, BWN_PHY_DACCTL);
		sav->phy_cck4 = BWN_PHY_READ(mac, BWN_PHY_CCK(0x14));
		sav->phy_hpwr_tssictl = BWN_PHY_READ(mac, BWN_PHY_HPWR_TSSICTL);

		BWN_PHY_SET(mac, BWN_PHY_HPWR_TSSICTL, 0x100);
		BWN_PHY_SET(mac, BWN_PHY_EXTG(0x01), 0x40);
		BWN_PHY_SET(mac, BWN_PHY_DACCTL, 0x40);
		BWN_PHY_SET(mac, BWN_PHY_CCK(0x14), 0x200);
	}
	if (phy->type == BWN_PHYTYPE_B &&
	    phy->rf_ver == 0x2050 && phy->rf_rev < 6) {
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x16), 0x410);
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x17), 0x820);
	}
	if (phy->rev >= 2) {
		sav->phy_analogover = BWN_PHY_READ(mac, BWN_PHY_ANALOGOVER);
		sav->phy_analogoverval =
		    BWN_PHY_READ(mac, BWN_PHY_ANALOGOVERVAL);
		sav->phy_rfover = BWN_PHY_READ(mac, BWN_PHY_RFOVER);
		sav->phy_rfoverval = BWN_PHY_READ(mac, BWN_PHY_RFOVERVAL);
		sav->phy_classctl = BWN_PHY_READ(mac, BWN_PHY_CLASSCTL);
		sav->phy_cck3 = BWN_PHY_READ(mac, BWN_PHY_CCK(0x3e));
		sav->phy_crs0 = BWN_PHY_READ(mac, BWN_PHY_CRS0);

		BWN_PHY_MASK(mac, BWN_PHY_CLASSCTL, 0xfffc);
		BWN_PHY_MASK(mac, BWN_PHY_CRS0, 0x7fff);
		BWN_PHY_SET(mac, BWN_PHY_ANALOGOVER, 0x0003);
		BWN_PHY_MASK(mac, BWN_PHY_ANALOGOVERVAL, 0xfffc);
		if (phy->type == BWN_PHYTYPE_G) {
			if ((phy->rev >= 7) &&
			    (siba_sprom_get_bf_lo(sc->sc_dev) &
			     BWN_BFL_EXTLNA)) {
				BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, 0x933);
			} else {
				BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, 0x133);
			}
		} else {
			BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, 0);
		}
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x3e), 0);
	}
	sav->reg0 = BWN_READ_2(mac, 0x3f4);
	sav->reg1 = BWN_READ_2(mac, 0x3e2);
	sav->rf0 = BWN_RF_READ(mac, 0x43);
	sav->rf1 = BWN_RF_READ(mac, 0x7a);
	sav->phy_pgactl = BWN_PHY_READ(mac, BWN_PHY_PGACTL);
	sav->phy_cck2 = BWN_PHY_READ(mac, BWN_PHY_CCK(0x2a));
	sav->phy_syncctl = BWN_PHY_READ(mac, BWN_PHY_SYNCCTL);
	sav->phy_dacctl = BWN_PHY_READ(mac, BWN_PHY_DACCTL);

	if (!BWN_HAS_TXMAG(phy)) {
		sav->rf2 = BWN_RF_READ(mac, 0x52);
		sav->rf2 &= 0x00f0;
	}
	if (phy->type == BWN_PHYTYPE_B) {
		sav->phy_cck0 = BWN_PHY_READ(mac, BWN_PHY_CCK(0x30));
		sav->phy_cck1 = BWN_PHY_READ(mac, BWN_PHY_CCK(0x06));
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x30), 0x00ff);
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x06), 0x3f3f);
	} else {
		BWN_WRITE_2(mac, 0x3e2, BWN_READ_2(mac, 0x3e2)
			    | 0x8000);
	}
	BWN_WRITE_2(mac, 0x3f4, BWN_READ_2(mac, 0x3f4)
		    & 0xf000);

	tmp =
	    (phy->type == BWN_PHYTYPE_G) ? BWN_PHY_LO_MASK : BWN_PHY_CCK(0x2e);
	BWN_PHY_WRITE(mac, tmp, 0x007f);

	tmp = sav->phy_syncctl;
	BWN_PHY_WRITE(mac, BWN_PHY_SYNCCTL, tmp & 0xff7f);
	tmp = sav->rf1;
	BWN_RF_WRITE(mac, 0x007a, tmp & 0xfff0);

	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2a), 0x8a3);
	if (phy->type == BWN_PHYTYPE_G ||
	    (phy->type == BWN_PHYTYPE_B &&
	     phy->rf_ver == 0x2050 && phy->rf_rev >= 6)) {
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2b), 0x1003);
	} else
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2b), 0x0802);
	if (phy->rev >= 2)
		bwn_dummy_transmission(mac, 0, 1);
	bwn_phy_g_switch_chan(mac, 6, 0);
	BWN_RF_READ(mac, 0x51);
	if (phy->type == BWN_PHYTYPE_G)
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2f), 0);

	nanouptime(&ts);
	if (time_before(lo->txctl_measured_time,
	    (ts.tv_nsec / 1000000 + ts.tv_sec * 1000) - BWN_LO_TXCTL_EXPIRE))
		bwn_lo_measure_txctl_values(mac);

	if (phy->type == BWN_PHYTYPE_G && phy->rev >= 3)
		BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, 0xc078);
	else {
		if (phy->type == BWN_PHYTYPE_B)
			BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2e), 0x8078);
		else
			BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, 0x8078);
	}
}

static void
bwn_lo_restore(struct bwn_mac *mac, struct bwn_lo_g_value *sav)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	uint16_t tmp;

	if (phy->rev >= 2) {
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, 0xe300);
		tmp = (pg->pg_pga_gain << 8);
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, tmp | 0xa0);
		DELAY(5);
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, tmp | 0xa2);
		DELAY(2);
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, tmp | 0xa3);
	} else {
		tmp = (pg->pg_pga_gain | 0xefa0);
		BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, tmp);
	}
	if (phy->type == BWN_PHYTYPE_G) {
		if (phy->rev >= 3)
			BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2e), 0xc078);
		else
			BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2e), 0x8078);
		if (phy->rev >= 2)
			BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2f), 0x0202);
		else
			BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2f), 0x0101);
	}
	BWN_WRITE_2(mac, 0x3f4, sav->reg0);
	BWN_PHY_WRITE(mac, BWN_PHY_PGACTL, sav->phy_pgactl);
	BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x2a), sav->phy_cck2);
	BWN_PHY_WRITE(mac, BWN_PHY_SYNCCTL, sav->phy_syncctl);
	BWN_PHY_WRITE(mac, BWN_PHY_DACCTL, sav->phy_dacctl);
	BWN_RF_WRITE(mac, 0x43, sav->rf0);
	BWN_RF_WRITE(mac, 0x7a, sav->rf1);
	if (!BWN_HAS_TXMAG(phy)) {
		tmp = sav->rf2;
		BWN_RF_SETMASK(mac, 0x52, 0xff0f, tmp);
	}
	BWN_WRITE_2(mac, 0x3e2, sav->reg1);
	if (phy->type == BWN_PHYTYPE_B &&
	    phy->rf_ver == 0x2050 && phy->rf_rev <= 5) {
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x30), sav->phy_cck0);
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x06), sav->phy_cck1);
	}
	if (phy->rev >= 2) {
		BWN_PHY_WRITE(mac, BWN_PHY_ANALOGOVER, sav->phy_analogover);
		BWN_PHY_WRITE(mac, BWN_PHY_ANALOGOVERVAL,
			      sav->phy_analogoverval);
		BWN_PHY_WRITE(mac, BWN_PHY_CLASSCTL, sav->phy_classctl);
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVER, sav->phy_rfover);
		BWN_PHY_WRITE(mac, BWN_PHY_RFOVERVAL, sav->phy_rfoverval);
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x3e), sav->phy_cck3);
		BWN_PHY_WRITE(mac, BWN_PHY_CRS0, sav->phy_crs0);
	}
	if (bwn_has_hwpctl(mac)) {
		tmp = (sav->phy_lomask & 0xbfff);
		BWN_PHY_WRITE(mac, BWN_PHY_LO_MASK, tmp);
		BWN_PHY_WRITE(mac, BWN_PHY_EXTG(0x01), sav->phy_extg);
		BWN_PHY_WRITE(mac, BWN_PHY_DACCTL, sav->phy_dacctl_hwpctl);
		BWN_PHY_WRITE(mac, BWN_PHY_CCK(0x14), sav->phy_cck4);
		BWN_PHY_WRITE(mac, BWN_PHY_HPWR_TSSICTL, sav->phy_hpwr_tssictl);
	}
	bwn_phy_g_switch_chan(mac, sav->old_channel, 1);
}

static int
bwn_lo_probe_loctl(struct bwn_mac *mac,
    struct bwn_loctl *probe, struct bwn_lo_g_sm *d)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_loctl orig, test;
	struct bwn_loctl prev = { -100, -100 };
	static const struct bwn_loctl modifiers[] = {
		{  1,  1,}, {  1,  0,}, {  1, -1,}, {  0, -1,},
		{ -1, -1,}, { -1,  0,}, { -1,  1,}, {  0,  1,}
	};
	int begin, end, lower = 0, i;
	uint16_t feedth;

	if (d->curstate == 0) {
		begin = 1;
		end = 8;
	} else if (d->curstate % 2 == 0) {
		begin = d->curstate - 1;
		end = d->curstate + 1;
	} else {
		begin = d->curstate - 2;
		end = d->curstate + 2;
	}
	if (begin < 1)
		begin += 8;
	if (end > 8)
		end -= 8;

	memcpy(&orig, probe, sizeof(struct bwn_loctl));
	i = begin;
	d->curstate = i;
	while (1) {
		KASSERT(i >= 1 && i <= 8, ("%s:%d: fail", __func__, __LINE__));
		memcpy(&test, &orig, sizeof(struct bwn_loctl));
		test.i += modifiers[i - 1].i * d->multipler;
		test.q += modifiers[i - 1].q * d->multipler;
		if ((test.i != prev.i || test.q != prev.q) &&
		    (abs(test.i) <= 16 && abs(test.q) <= 16)) {
			bwn_lo_write(mac, &test);
			feedth = bwn_lo_calcfeed(mac, pg->pg_lna_gain,
			    pg->pg_pga_gain, pg->pg_trsw_rx_gain);
			if (feedth < d->feedth) {
				memcpy(probe, &test,
				    sizeof(struct bwn_loctl));
				lower = 1;
				d->feedth = feedth;
				if (d->nmeasure < 2 && !BWN_HAS_LOOPBACK(phy))
					break;
			}
		}
		memcpy(&prev, &test, sizeof(prev));
		if (i == end)
			break;
		if (i == 8)
			i = 1;
		else
			i++;
		d->curstate = i;
	}

	return (lower);
}

static void
bwn_lo_probe_sm(struct bwn_mac *mac, struct bwn_loctl *loctl, int *rxgain)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_lo_g_sm d;
	struct bwn_loctl probe;
	int lower, repeat, cnt = 0;
	uint16_t feedth;

	d.nmeasure = 0;
	d.multipler = 1;
	if (BWN_HAS_LOOPBACK(phy))
		d.multipler = 3;

	memcpy(&d.loctl, loctl, sizeof(struct bwn_loctl));
	repeat = (BWN_HAS_LOOPBACK(phy)) ? 4 : 1;

	do {
		bwn_lo_write(mac, &d.loctl);
		feedth = bwn_lo_calcfeed(mac, pg->pg_lna_gain,
		    pg->pg_pga_gain, pg->pg_trsw_rx_gain);
		if (feedth < 0x258) {
			if (feedth >= 0x12c)
				*rxgain += 6;
			else
				*rxgain += 3;
			feedth = bwn_lo_calcfeed(mac, pg->pg_lna_gain,
			    pg->pg_pga_gain, pg->pg_trsw_rx_gain);
		}
		d.feedth = feedth;
		d.curstate = 0;
		do {
			KASSERT(d.curstate >= 0 && d.curstate <= 8,
			    ("%s:%d: fail", __func__, __LINE__));
			memcpy(&probe, &d.loctl,
			       sizeof(struct bwn_loctl));
			lower = bwn_lo_probe_loctl(mac, &probe, &d);
			if (!lower)
				break;
			if ((probe.i == d.loctl.i) && (probe.q == d.loctl.q))
				break;
			memcpy(&d.loctl, &probe, sizeof(struct bwn_loctl));
			d.nmeasure++;
		} while (d.nmeasure < 24);
		memcpy(loctl, &d.loctl, sizeof(struct bwn_loctl));

		if (BWN_HAS_LOOPBACK(phy)) {
			if (d.feedth > 0x1194)
				*rxgain -= 6;
			else if (d.feedth < 0x5dc)
				*rxgain += 3;
			if (cnt == 0) {
				if (d.feedth <= 0x5dc) {
					d.multipler = 1;
					cnt++;
				} else
					d.multipler = 2;
			} else if (cnt == 2)
				d.multipler = 1;
		}
		bwn_lo_measure_gain_values(mac, *rxgain, BWN_HAS_LOOPBACK(phy));
	} while (++cnt < repeat);
}

static struct bwn_lo_calib *
bwn_lo_calibset(struct bwn_mac *mac,
    const struct bwn_bbatt *bbatt, const struct bwn_rfatt *rfatt)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_loctl loctl = { 0, 0 };
	struct bwn_lo_calib *cal;
	struct bwn_lo_g_value sval = { 0 };
	int rxgain;
	uint16_t pad, reg, value;

	sval.old_channel = phy->chan;
	bwn_mac_suspend(mac);
	bwn_lo_save(mac, &sval);

	reg = bwn_lo_txctl_regtable(mac, &value, &pad);
	BWN_RF_SETMASK(mac, 0x43, 0xfff0, rfatt->att);
	BWN_RF_SETMASK(mac, reg, ~value, (rfatt->padmix ? value :0));

	rxgain = (rfatt->att * 2) + (bbatt->att / 2);
	if (rfatt->padmix)
		rxgain -= pad;
	if (BWN_HAS_LOOPBACK(phy))
		rxgain += pg->pg_max_lb_gain;
	bwn_lo_measure_gain_values(mac, rxgain, BWN_HAS_LOOPBACK(phy));
	bwn_phy_g_set_bbatt(mac, bbatt->att);
	bwn_lo_probe_sm(mac, &loctl, &rxgain);

	bwn_lo_restore(mac, &sval);
	bwn_mac_enable(mac);

	cal = malloc(sizeof(*cal), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!cal) {
		device_printf(mac->mac_sc->sc_dev, "out of memory\n");
		return (NULL);
	}
	memcpy(&cal->bbatt, bbatt, sizeof(*bbatt));
	memcpy(&cal->rfatt, rfatt, sizeof(*rfatt));
	memcpy(&cal->ctl, &loctl, sizeof(loctl));

	BWN_GETTIME(cal->calib_time);

	return (cal);
}

static struct bwn_lo_calib *
bwn_lo_get_calib(struct bwn_mac *mac, const struct bwn_bbatt *bbatt,
    const struct bwn_rfatt *rfatt)
{
	struct bwn_txpwr_loctl *lo = &mac->mac_phy.phy_g.pg_loctl;
	struct bwn_lo_calib *c;

	TAILQ_FOREACH(c, &lo->calib_list, list) {
		if (!BWN_BBATTCMP(&c->bbatt, bbatt))
			continue;
		if (!BWN_RFATTCMP(&c->rfatt, rfatt))
			continue;
		return (c);
	}

	c = bwn_lo_calibset(mac, bbatt, rfatt);
	if (!c)
		return (NULL);
	TAILQ_INSERT_TAIL(&lo->calib_list, c, list);

	return (c);
}

static void
bwn_phy_g_dc_lookup_init(struct bwn_mac *mac, uint8_t update)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_txpwr_loctl *lo = &pg->pg_loctl;
	const struct bwn_rfatt *rfatt;
	const struct bwn_bbatt *bbatt;
	uint64_t pvector;
	int i;
	int rf_offset, bb_offset;
	uint8_t changed = 0;

	KASSERT(BWN_DC_LT_SIZE == 32, ("%s:%d: fail", __func__, __LINE__));
	KASSERT(lo->rfatt.len * lo->bbatt.len <= 64,
	    ("%s:%d: fail", __func__, __LINE__));

	pvector = lo->power_vector;
	if (!update && !pvector)
		return;

	bwn_mac_suspend(mac);

	for (i = 0; i < BWN_DC_LT_SIZE * 2; i++) {
		struct bwn_lo_calib *cal;
		int idx;
		uint16_t val;

		if (!update && !(pvector & (((uint64_t)1ULL) << i)))
			continue;
		bb_offset = i / lo->rfatt.len;
		rf_offset = i % lo->rfatt.len;
		bbatt = &(lo->bbatt.array[bb_offset]);
		rfatt = &(lo->rfatt.array[rf_offset]);

		cal = bwn_lo_calibset(mac, bbatt, rfatt);
		if (!cal) {
			device_printf(sc->sc_dev, "LO: Could not "
			    "calibrate DC table entry\n");
			continue;
		}
		val = (uint8_t)(cal->ctl.q);
		val |= ((uint8_t)(cal->ctl.i)) << 4;
		free(cal, M_DEVBUF);

		idx = i / 2;
		if (i % 2)
			lo->dc_lt[idx] = (lo->dc_lt[idx] & 0x00ff)
			    | ((val & 0x00ff) << 8);
		else
			lo->dc_lt[idx] = (lo->dc_lt[idx] & 0xff00)
			    | (val & 0x00ff);
		changed = 1;
	}
	if (changed) {
		for (i = 0; i < BWN_DC_LT_SIZE; i++)
			BWN_PHY_WRITE(mac, 0x3a0 + i, lo->dc_lt[i]);
	}
	bwn_mac_enable(mac);
}

static void
bwn_lo_fixup_rfatt(struct bwn_rfatt *rf)
{

	if (!rf->padmix)
		return;
	if ((rf->att != 1) && (rf->att != 2) && (rf->att != 3))
		rf->att = 4;
}

static void
bwn_lo_g_adjust(struct bwn_mac *mac)
{
	struct bwn_phy_g *pg = &mac->mac_phy.phy_g;
	struct bwn_lo_calib *cal;
	struct bwn_rfatt rf;

	memcpy(&rf, &pg->pg_rfatt, sizeof(rf));
	bwn_lo_fixup_rfatt(&rf);

	cal = bwn_lo_get_calib(mac, &pg->pg_bbatt, &rf);
	if (!cal)
		return;
	bwn_lo_write(mac, &cal->ctl);
}

static void
bwn_lo_g_init(struct bwn_mac *mac)
{

	if (!bwn_has_hwpctl(mac))
		return;

	bwn_lo_get_powervector(mac);
	bwn_phy_g_dc_lookup_init(mac, 1);
}

static void
bwn_mac_suspend(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	int i;
	uint32_t tmp;

	KASSERT(mac->mac_suspended >= 0,
	    ("%s:%d: fail", __func__, __LINE__));

	if (mac->mac_suspended == 0) {
		bwn_psctl(mac, BWN_PS_AWAKE);
		BWN_WRITE_4(mac, BWN_MACCTL,
			    BWN_READ_4(mac, BWN_MACCTL)
			    & ~BWN_MACCTL_ON);
		BWN_READ_4(mac, BWN_MACCTL);
		for (i = 35; i; i--) {
			tmp = BWN_READ_4(mac, BWN_INTR_REASON);
			if (tmp & BWN_INTR_MAC_SUSPENDED)
				goto out;
			DELAY(10);
		}
		for (i = 40; i; i--) {
			tmp = BWN_READ_4(mac, BWN_INTR_REASON);
			if (tmp & BWN_INTR_MAC_SUSPENDED)
				goto out;
			DELAY(1000);
		}
		device_printf(sc->sc_dev, "MAC suspend failed\n");
	}
out:
	mac->mac_suspended++;
}

static void
bwn_mac_enable(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t state;

	state = bwn_shm_read_2(mac, BWN_SHARED,
	    BWN_SHARED_UCODESTAT);
	if (state != BWN_SHARED_UCODESTAT_SUSPEND &&
	    state != BWN_SHARED_UCODESTAT_SLEEP)
		device_printf(sc->sc_dev, "warn: firmware state (%d)\n", state);

	mac->mac_suspended--;
	KASSERT(mac->mac_suspended >= 0,
	    ("%s:%d: fail", __func__, __LINE__));
	if (mac->mac_suspended == 0) {
		BWN_WRITE_4(mac, BWN_MACCTL,
		    BWN_READ_4(mac, BWN_MACCTL) | BWN_MACCTL_ON);
		BWN_WRITE_4(mac, BWN_INTR_REASON, BWN_INTR_MAC_SUSPENDED);
		BWN_READ_4(mac, BWN_MACCTL);
		BWN_READ_4(mac, BWN_INTR_REASON);
		bwn_psctl(mac, 0);
	}
}

static void
bwn_psctl(struct bwn_mac *mac, uint32_t flags)
{
	struct bwn_softc *sc = mac->mac_sc;
	int i;
	uint16_t ucstat;

	KASSERT(!((flags & BWN_PS_ON) && (flags & BWN_PS_OFF)),
	    ("%s:%d: fail", __func__, __LINE__));
	KASSERT(!((flags & BWN_PS_AWAKE) && (flags & BWN_PS_ASLEEP)),
	    ("%s:%d: fail", __func__, __LINE__));

	/* XXX forcibly awake and hwps-off */

	BWN_WRITE_4(mac, BWN_MACCTL,
	    (BWN_READ_4(mac, BWN_MACCTL) | BWN_MACCTL_AWAKE) &
	    ~BWN_MACCTL_HWPS);
	BWN_READ_4(mac, BWN_MACCTL);
	if (siba_get_revid(sc->sc_dev) >= 5) {
		for (i = 0; i < 100; i++) {
			ucstat = bwn_shm_read_2(mac, BWN_SHARED,
			    BWN_SHARED_UCODESTAT);
			if (ucstat != BWN_SHARED_UCODESTAT_SLEEP)
				break;
			DELAY(10);
		}
	}
}

static int16_t
bwn_nrssi_read(struct bwn_mac *mac, uint16_t offset)
{

	BWN_PHY_WRITE(mac, BWN_PHY_NRSSI_CTRL, offset);
	return ((int16_t)BWN_PHY_READ(mac, BWN_PHY_NRSSI_DATA));
}

static void
bwn_nrssi_threshold(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	int32_t a, b;
	int16_t tmp16;
	uint16_t tmpu16;

	KASSERT(phy->type == BWN_PHYTYPE_G, ("%s: fail", __func__));

	if (phy->gmode && (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_RSSI)) {
		if (!pg->pg_aci_wlan_automatic && pg->pg_aci_enable) {
			a = 0x13;
			b = 0x12;
		} else {
			a = 0xe;
			b = 0x11;
		}

		a = a * (pg->pg_nrssi[1] - pg->pg_nrssi[0]);
		a += (pg->pg_nrssi[0] << 6);
		a += (a < 32) ? 31 : 32;
		a = a >> 6;
		a = MIN(MAX(a, -31), 31);

		b = b * (pg->pg_nrssi[1] - pg->pg_nrssi[0]);
		b += (pg->pg_nrssi[0] << 6);
		if (b < 32)
			b += 31;
		else
			b += 32;
		b = b >> 6;
		b = MIN(MAX(b, -31), 31);

		tmpu16 = BWN_PHY_READ(mac, 0x048a) & 0xf000;
		tmpu16 |= ((uint32_t)b & 0x0000003f);
		tmpu16 |= (((uint32_t)a & 0x0000003f) << 6);
		BWN_PHY_WRITE(mac, 0x048a, tmpu16);
		return;
	}

	tmp16 = bwn_nrssi_read(mac, 0x20);
	if (tmp16 >= 0x20)
		tmp16 -= 0x40;
	BWN_PHY_SETMASK(mac, 0x048a, 0xf000, (tmp16 < 3) ? 0x09eb : 0x0aed);
}

static void
bwn_nrssi_slope_11g(struct bwn_mac *mac)
{
#define	SAVE_RF_MAX		3
#define	SAVE_PHY_COMM_MAX	4
#define	SAVE_PHY3_MAX		8
	static const uint16_t save_rf_regs[SAVE_RF_MAX] =
		{ 0x7a, 0x52, 0x43 };
	static const uint16_t save_phy_comm_regs[SAVE_PHY_COMM_MAX] =
		{ 0x15, 0x5a, 0x59, 0x58 };
	static const uint16_t save_phy3_regs[SAVE_PHY3_MAX] = {
		0x002e, 0x002f, 0x080f, BWN_PHY_G_LOCTL,
		0x0801, 0x0060, 0x0014, 0x0478
	};
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	int32_t i, tmp32, phy3_idx = 0;
	uint16_t delta, tmp;
	uint16_t save_rf[SAVE_RF_MAX];
	uint16_t save_phy_comm[SAVE_PHY_COMM_MAX];
	uint16_t save_phy3[SAVE_PHY3_MAX];
	uint16_t ant_div, phy0, chan_ex;
	int16_t nrssi0, nrssi1;

	KASSERT(phy->type == BWN_PHYTYPE_G,
	    ("%s:%d: fail", __func__, __LINE__));

	if (phy->rf_rev >= 9)
		return;
	if (phy->rf_rev == 8)
		bwn_nrssi_offset(mac);

	BWN_PHY_MASK(mac, BWN_PHY_G_CRS, 0x7fff);
	BWN_PHY_MASK(mac, 0x0802, 0xfffc);

	/*
	 * Save RF/PHY registers for later restoration
	 */
	ant_div = BWN_READ_2(mac, 0x03e2);
	BWN_WRITE_2(mac, 0x03e2, BWN_READ_2(mac, 0x03e2) | 0x8000);
	for (i = 0; i < SAVE_RF_MAX; ++i)
		save_rf[i] = BWN_RF_READ(mac, save_rf_regs[i]);
	for (i = 0; i < SAVE_PHY_COMM_MAX; ++i)
		save_phy_comm[i] = BWN_PHY_READ(mac, save_phy_comm_regs[i]);

	phy0 = BWN_READ_2(mac, BWN_PHY0);
	chan_ex = BWN_READ_2(mac, BWN_CHANNEL_EXT);
	if (phy->rev >= 3) {
		for (i = 0; i < SAVE_PHY3_MAX; ++i)
			save_phy3[i] = BWN_PHY_READ(mac, save_phy3_regs[i]);
		BWN_PHY_WRITE(mac, 0x002e, 0);
		BWN_PHY_WRITE(mac, BWN_PHY_G_LOCTL, 0);
		switch (phy->rev) {
		case 4:
		case 6:
		case 7:
			BWN_PHY_SET(mac, 0x0478, 0x0100);
			BWN_PHY_SET(mac, 0x0801, 0x0040);
			break;
		case 3:
		case 5:
			BWN_PHY_MASK(mac, 0x0801, 0xffbf);
			break;
		}
		BWN_PHY_SET(mac, 0x0060, 0x0040);
		BWN_PHY_SET(mac, 0x0014, 0x0200);
	}
	/*
	 * Calculate nrssi0
	 */
	BWN_RF_SET(mac, 0x007a, 0x0070);
	bwn_set_all_gains(mac, 0, 8, 0);
	BWN_RF_MASK(mac, 0x007a, 0x00f7);
	if (phy->rev >= 2) {
		BWN_PHY_SETMASK(mac, 0x0811, 0xffcf, 0x0030);
		BWN_PHY_SETMASK(mac, 0x0812, 0xffcf, 0x0010);
	}
	BWN_RF_SET(mac, 0x007a, 0x0080);
	DELAY(20);

	nrssi0 = (int16_t) ((BWN_PHY_READ(mac, 0x047f) >> 8) & 0x003f);
	if (nrssi0 >= 0x0020)
		nrssi0 -= 0x0040;

	/*
	 * Calculate nrssi1
	 */
	BWN_RF_MASK(mac, 0x007a, 0x007f);
	if (phy->rev >= 2)
		BWN_PHY_SETMASK(mac, 0x0003, 0xff9f, 0x0040);

	BWN_WRITE_2(mac, BWN_CHANNEL_EXT,
	    BWN_READ_2(mac, BWN_CHANNEL_EXT) | 0x2000);
	BWN_RF_SET(mac, 0x007a, 0x000f);
	BWN_PHY_WRITE(mac, 0x0015, 0xf330);
	if (phy->rev >= 2) {
		BWN_PHY_SETMASK(mac, 0x0812, 0xffcf, 0x0020);
		BWN_PHY_SETMASK(mac, 0x0811, 0xffcf, 0x0020);
	}

	bwn_set_all_gains(mac, 3, 0, 1);
	if (phy->rf_rev == 8) {
		BWN_RF_WRITE(mac, 0x0043, 0x001f);
	} else {
		tmp = BWN_RF_READ(mac, 0x0052) & 0xff0f;
		BWN_RF_WRITE(mac, 0x0052, tmp | 0x0060);
		tmp = BWN_RF_READ(mac, 0x0043) & 0xfff0;
		BWN_RF_WRITE(mac, 0x0043, tmp | 0x0009);
	}
	BWN_PHY_WRITE(mac, 0x005a, 0x0480);
	BWN_PHY_WRITE(mac, 0x0059, 0x0810);
	BWN_PHY_WRITE(mac, 0x0058, 0x000d);
	DELAY(20);
	nrssi1 = (int16_t) ((BWN_PHY_READ(mac, 0x047f) >> 8) & 0x003f);

	/*
	 * Install calculated narrow RSSI values
	 */
	if (nrssi1 >= 0x0020)
		nrssi1 -= 0x0040;
	if (nrssi0 == nrssi1)
		pg->pg_nrssi_slope = 0x00010000;
	else
		pg->pg_nrssi_slope = 0x00400000 / (nrssi0 - nrssi1);
	if (nrssi0 >= -4) {
		pg->pg_nrssi[0] = nrssi1;
		pg->pg_nrssi[1] = nrssi0;
	}

	/*
	 * Restore saved RF/PHY registers
	 */
	if (phy->rev >= 3) {
		for (phy3_idx = 0; phy3_idx < 4; ++phy3_idx) {
			BWN_PHY_WRITE(mac, save_phy3_regs[phy3_idx],
			    save_phy3[phy3_idx]);
		}
	}
	if (phy->rev >= 2) {
		BWN_PHY_MASK(mac, 0x0812, 0xffcf);
		BWN_PHY_MASK(mac, 0x0811, 0xffcf);
	}

	for (i = 0; i < SAVE_RF_MAX; ++i)
		BWN_RF_WRITE(mac, save_rf_regs[i], save_rf[i]);

	BWN_WRITE_2(mac, 0x03e2, ant_div);
	BWN_WRITE_2(mac, 0x03e6, phy0);
	BWN_WRITE_2(mac, BWN_CHANNEL_EXT, chan_ex);

	for (i = 0; i < SAVE_PHY_COMM_MAX; ++i)
		BWN_PHY_WRITE(mac, save_phy_comm_regs[i], save_phy_comm[i]);

	bwn_spu_workaround(mac, phy->chan);
	BWN_PHY_SET(mac, 0x0802, (0x0001 | 0x0002));
	bwn_set_original_gains(mac);
	BWN_PHY_SET(mac, BWN_PHY_G_CRS, 0x8000);
	if (phy->rev >= 3) {
		for (; phy3_idx < SAVE_PHY3_MAX; ++phy3_idx) {
			BWN_PHY_WRITE(mac, save_phy3_regs[phy3_idx],
			    save_phy3[phy3_idx]);
		}
	}

	delta = 0x1f - pg->pg_nrssi[0];
	for (i = 0; i < 64; i++) {
		tmp32 = (((i - delta) * pg->pg_nrssi_slope) / 0x10000) + 0x3a;
		tmp32 = MIN(MAX(tmp32, 0), 0x3f);
		pg->pg_nrssi_lt[i] = tmp32;
	}

	bwn_nrssi_threshold(mac);
#undef SAVE_RF_MAX
#undef SAVE_PHY_COMM_MAX
#undef SAVE_PHY3_MAX
}

static void
bwn_nrssi_offset(struct bwn_mac *mac)
{
#define	SAVE_RF_MAX		2
#define	SAVE_PHY_COMM_MAX	10
#define	SAVE_PHY6_MAX		8
	static const uint16_t save_rf_regs[SAVE_RF_MAX] =
		{ 0x7a, 0x43 };
	static const uint16_t save_phy_comm_regs[SAVE_PHY_COMM_MAX] = {
		0x0001, 0x0811, 0x0812, 0x0814,
		0x0815, 0x005a, 0x0059, 0x0058,
		0x000a, 0x0003
	};
	static const uint16_t save_phy6_regs[SAVE_PHY6_MAX] = {
		0x002e, 0x002f, 0x080f, 0x0810,
		0x0801, 0x0060, 0x0014, 0x0478
	};
	struct bwn_phy *phy = &mac->mac_phy;
	int i, phy6_idx = 0;
	uint16_t save_rf[SAVE_RF_MAX];
	uint16_t save_phy_comm[SAVE_PHY_COMM_MAX];
	uint16_t save_phy6[SAVE_PHY6_MAX];
	int16_t nrssi;
	uint16_t saved = 0xffff;

	for (i = 0; i < SAVE_PHY_COMM_MAX; ++i)
		save_phy_comm[i] = BWN_PHY_READ(mac, save_phy_comm_regs[i]);
	for (i = 0; i < SAVE_RF_MAX; ++i)
		save_rf[i] = BWN_RF_READ(mac, save_rf_regs[i]);

	BWN_PHY_MASK(mac, 0x0429, 0x7fff);
	BWN_PHY_SETMASK(mac, 0x0001, 0x3fff, 0x4000);
	BWN_PHY_SET(mac, 0x0811, 0x000c);
	BWN_PHY_SETMASK(mac, 0x0812, 0xfff3, 0x0004);
	BWN_PHY_MASK(mac, 0x0802, ~(0x1 | 0x2));
	if (phy->rev >= 6) {
		for (i = 0; i < SAVE_PHY6_MAX; ++i)
			save_phy6[i] = BWN_PHY_READ(mac, save_phy6_regs[i]);

		BWN_PHY_WRITE(mac, 0x002e, 0);
		BWN_PHY_WRITE(mac, 0x002f, 0);
		BWN_PHY_WRITE(mac, 0x080f, 0);
		BWN_PHY_WRITE(mac, 0x0810, 0);
		BWN_PHY_SET(mac, 0x0478, 0x0100);
		BWN_PHY_SET(mac, 0x0801, 0x0040);
		BWN_PHY_SET(mac, 0x0060, 0x0040);
		BWN_PHY_SET(mac, 0x0014, 0x0200);
	}
	BWN_RF_SET(mac, 0x007a, 0x0070);
	BWN_RF_SET(mac, 0x007a, 0x0080);
	DELAY(30);

	nrssi = (int16_t) ((BWN_PHY_READ(mac, 0x047f) >> 8) & 0x003f);
	if (nrssi >= 0x20)
		nrssi -= 0x40;
	if (nrssi == 31) {
		for (i = 7; i >= 4; i--) {
			BWN_RF_WRITE(mac, 0x007b, i);
			DELAY(20);
			nrssi = (int16_t) ((BWN_PHY_READ(mac, 0x047f) >> 8) &
			    0x003f);
			if (nrssi >= 0x20)
				nrssi -= 0x40;
			if (nrssi < 31 && saved == 0xffff)
				saved = i;
		}
		if (saved == 0xffff)
			saved = 4;
	} else {
		BWN_RF_MASK(mac, 0x007a, 0x007f);
		if (phy->rev != 1) {
			BWN_PHY_SET(mac, 0x0814, 0x0001);
			BWN_PHY_MASK(mac, 0x0815, 0xfffe);
		}
		BWN_PHY_SET(mac, 0x0811, 0x000c);
		BWN_PHY_SET(mac, 0x0812, 0x000c);
		BWN_PHY_SET(mac, 0x0811, 0x0030);
		BWN_PHY_SET(mac, 0x0812, 0x0030);
		BWN_PHY_WRITE(mac, 0x005a, 0x0480);
		BWN_PHY_WRITE(mac, 0x0059, 0x0810);
		BWN_PHY_WRITE(mac, 0x0058, 0x000d);
		if (phy->rev == 0)
			BWN_PHY_WRITE(mac, 0x0003, 0x0122);
		else
			BWN_PHY_SET(mac, 0x000a, 0x2000);
		if (phy->rev != 1) {
			BWN_PHY_SET(mac, 0x0814, 0x0004);
			BWN_PHY_MASK(mac, 0x0815, 0xfffb);
		}
		BWN_PHY_SETMASK(mac, 0x0003, 0xff9f, 0x0040);
		BWN_RF_SET(mac, 0x007a, 0x000f);
		bwn_set_all_gains(mac, 3, 0, 1);
		BWN_RF_SETMASK(mac, 0x0043, 0x00f0, 0x000f);
		DELAY(30);
		nrssi = (int16_t) ((BWN_PHY_READ(mac, 0x047f) >> 8) & 0x003f);
		if (nrssi >= 0x20)
			nrssi -= 0x40;
		if (nrssi == -32) {
			for (i = 0; i < 4; i++) {
				BWN_RF_WRITE(mac, 0x007b, i);
				DELAY(20);
				nrssi = (int16_t)((BWN_PHY_READ(mac,
				    0x047f) >> 8) & 0x003f);
				if (nrssi >= 0x20)
					nrssi -= 0x40;
				if (nrssi > -31 && saved == 0xffff)
					saved = i;
			}
			if (saved == 0xffff)
				saved = 3;
		} else
			saved = 0;
	}
	BWN_RF_WRITE(mac, 0x007b, saved);

	/*
	 * Restore saved RF/PHY registers
	 */
	if (phy->rev >= 6) {
		for (phy6_idx = 0; phy6_idx < 4; ++phy6_idx) {
			BWN_PHY_WRITE(mac, save_phy6_regs[phy6_idx],
			    save_phy6[phy6_idx]);
		}
	}
	if (phy->rev != 1) {
		for (i = 3; i < 5; i++)
			BWN_PHY_WRITE(mac, save_phy_comm_regs[i],
			    save_phy_comm[i]);
	}
	for (i = 5; i < SAVE_PHY_COMM_MAX; i++)
		BWN_PHY_WRITE(mac, save_phy_comm_regs[i], save_phy_comm[i]);

	for (i = SAVE_RF_MAX - 1; i >= 0; --i)
		BWN_RF_WRITE(mac, save_rf_regs[i], save_rf[i]);

	BWN_PHY_WRITE(mac, 0x0802, BWN_PHY_READ(mac, 0x0802) | 0x1 | 0x2);
	BWN_PHY_SET(mac, 0x0429, 0x8000);
	bwn_set_original_gains(mac);
	if (phy->rev >= 6) {
		for (; phy6_idx < SAVE_PHY6_MAX; ++phy6_idx) {
			BWN_PHY_WRITE(mac, save_phy6_regs[phy6_idx],
			    save_phy6[phy6_idx]);
		}
	}

	BWN_PHY_WRITE(mac, save_phy_comm_regs[0], save_phy_comm[0]);
	BWN_PHY_WRITE(mac, save_phy_comm_regs[2], save_phy_comm[2]);
	BWN_PHY_WRITE(mac, save_phy_comm_regs[1], save_phy_comm[1]);
}

static void
bwn_set_all_gains(struct bwn_mac *mac, int16_t first, int16_t second,
    int16_t third)
{
	struct bwn_phy *phy = &mac->mac_phy;
	uint16_t i;
	uint16_t start = 0x08, end = 0x18;
	uint16_t tmp;
	uint16_t table;

	if (phy->rev <= 1) {
		start = 0x10;
		end = 0x20;
	}

	table = BWN_OFDMTAB_GAINX;
	if (phy->rev <= 1)
		table = BWN_OFDMTAB_GAINX_R1;
	for (i = 0; i < 4; i++)
		bwn_ofdmtab_write_2(mac, table, i, first);

	for (i = start; i < end; i++)
		bwn_ofdmtab_write_2(mac, table, i, second);

	if (third != -1) {
		tmp = ((uint16_t) third << 14) | ((uint16_t) third << 6);
		BWN_PHY_SETMASK(mac, 0x04a0, 0xbfbf, tmp);
		BWN_PHY_SETMASK(mac, 0x04a1, 0xbfbf, tmp);
		BWN_PHY_SETMASK(mac, 0x04a2, 0xbfbf, tmp);
	}
	bwn_dummy_transmission(mac, 0, 1);
}

static void
bwn_set_original_gains(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	uint16_t i, tmp;
	uint16_t table;
	uint16_t start = 0x0008, end = 0x0018;

	if (phy->rev <= 1) {
		start = 0x0010;
		end = 0x0020;
	}

	table = BWN_OFDMTAB_GAINX;
	if (phy->rev <= 1)
		table = BWN_OFDMTAB_GAINX_R1;
	for (i = 0; i < 4; i++) {
		tmp = (i & 0xfffc);
		tmp |= (i & 0x0001) << 1;
		tmp |= (i & 0x0002) >> 1;

		bwn_ofdmtab_write_2(mac, table, i, tmp);
	}

	for (i = start; i < end; i++)
		bwn_ofdmtab_write_2(mac, table, i, i - start);

	BWN_PHY_SETMASK(mac, 0x04a0, 0xbfbf, 0x4040);
	BWN_PHY_SETMASK(mac, 0x04a1, 0xbfbf, 0x4040);
	BWN_PHY_SETMASK(mac, 0x04a2, 0xbfbf, 0x4000);
	bwn_dummy_transmission(mac, 0, 1);
}

static void
bwn_phy_hwpctl_init(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_rfatt old_rfatt, rfatt;
	struct bwn_bbatt old_bbatt, bbatt;
	struct bwn_softc *sc = mac->mac_sc;
	uint8_t old_txctl = 0;

	KASSERT(phy->type == BWN_PHYTYPE_G,
	    ("%s:%d: fail", __func__, __LINE__));

	if ((siba_get_pci_subvendor(sc->sc_dev) == SIBA_BOARDVENDOR_BCM) &&
	    (siba_get_pci_subdevice(sc->sc_dev) == SIBA_BOARD_BU4306))
		return;

	BWN_PHY_WRITE(mac, 0x0028, 0x8018);

	BWN_WRITE_2(mac, BWN_PHY0, BWN_READ_2(mac, BWN_PHY0) & 0xffdf);

	if (!phy->gmode)
		return;
	bwn_hwpctl_early_init(mac);
	if (pg->pg_curtssi == 0) {
		if (phy->rf_ver == 0x2050 && phy->analog == 0) {
			BWN_RF_SETMASK(mac, 0x0076, 0x00f7, 0x0084);
		} else {
			memcpy(&old_rfatt, &pg->pg_rfatt, sizeof(old_rfatt));
			memcpy(&old_bbatt, &pg->pg_bbatt, sizeof(old_bbatt));
			old_txctl = pg->pg_txctl;

			bbatt.att = 11;
			if (phy->rf_rev == 8) {
				rfatt.att = 15;
				rfatt.padmix = 1;
			} else {
				rfatt.att = 9;
				rfatt.padmix = 0;
			}
			bwn_phy_g_set_txpwr_sub(mac, &bbatt, &rfatt, 0);
		}
		bwn_dummy_transmission(mac, 0, 1);
		pg->pg_curtssi = BWN_PHY_READ(mac, BWN_PHY_TSSI);
		if (phy->rf_ver == 0x2050 && phy->analog == 0)
			BWN_RF_MASK(mac, 0x0076, 0xff7b);
		else
			bwn_phy_g_set_txpwr_sub(mac, &old_bbatt,
			    &old_rfatt, old_txctl);
	}
	bwn_hwpctl_init_gphy(mac);

	/* clear TSSI */
	bwn_shm_write_2(mac, BWN_SHARED, 0x0058, 0x7f7f);
	bwn_shm_write_2(mac, BWN_SHARED, 0x005a, 0x7f7f);
	bwn_shm_write_2(mac, BWN_SHARED, 0x0070, 0x7f7f);
	bwn_shm_write_2(mac, BWN_SHARED, 0x0072, 0x7f7f);
}

static void
bwn_hwpctl_early_init(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;

	if (!bwn_has_hwpctl(mac)) {
		BWN_PHY_WRITE(mac, 0x047a, 0xc111);
		return;
	}

	BWN_PHY_MASK(mac, 0x0036, 0xfeff);
	BWN_PHY_WRITE(mac, 0x002f, 0x0202);
	BWN_PHY_SET(mac, 0x047c, 0x0002);
	BWN_PHY_SET(mac, 0x047a, 0xf000);
	if (phy->rf_ver == 0x2050 && phy->rf_rev == 8) {
		BWN_PHY_SETMASK(mac, 0x047a, 0xff0f, 0x0010);
		BWN_PHY_SET(mac, 0x005d, 0x8000);
		BWN_PHY_SETMASK(mac, 0x004e, 0xffc0, 0x0010);
		BWN_PHY_WRITE(mac, 0x002e, 0xc07f);
		BWN_PHY_SET(mac, 0x0036, 0x0400);
	} else {
		BWN_PHY_SET(mac, 0x0036, 0x0200);
		BWN_PHY_SET(mac, 0x0036, 0x0400);
		BWN_PHY_MASK(mac, 0x005d, 0x7fff);
		BWN_PHY_MASK(mac, 0x004f, 0xfffe);
		BWN_PHY_SETMASK(mac, 0x004e, 0xffc0, 0x0010);
		BWN_PHY_WRITE(mac, 0x002e, 0xc07f);
		BWN_PHY_SETMASK(mac, 0x047a, 0xff0f, 0x0010);
	}
}

static void
bwn_hwpctl_init_gphy(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_txpwr_loctl *lo = &pg->pg_loctl;
	int i;
	uint16_t nr_written = 0, tmp, value;
	uint8_t rf, bb;

	if (!bwn_has_hwpctl(mac)) {
		bwn_hf_write(mac, bwn_hf_read(mac) & ~BWN_HF_HW_POWERCTL);
		return;
	}

	BWN_PHY_SETMASK(mac, 0x0036, 0xffc0,
	    (pg->pg_idletssi - pg->pg_curtssi));
	BWN_PHY_SETMASK(mac, 0x0478, 0xff00,
	    (pg->pg_idletssi - pg->pg_curtssi));

	for (i = 0; i < 32; i++)
		bwn_ofdmtab_write_2(mac, 0x3c20, i, pg->pg_tssi2dbm[i]);
	for (i = 32; i < 64; i++)
		bwn_ofdmtab_write_2(mac, 0x3c00, i - 32, pg->pg_tssi2dbm[i]);
	for (i = 0; i < 64; i += 2) {
		value = (uint16_t) pg->pg_tssi2dbm[i];
		value |= ((uint16_t) pg->pg_tssi2dbm[i + 1]) << 8;
		BWN_PHY_WRITE(mac, 0x380 + (i / 2), value);
	}

	for (rf = 0; rf < lo->rfatt.len; rf++) {
		for (bb = 0; bb < lo->bbatt.len; bb++) {
			if (nr_written >= 0x40)
				return;
			tmp = lo->bbatt.array[bb].att;
			tmp <<= 8;
			if (phy->rf_rev == 8)
				tmp |= 0x50;
			else
				tmp |= 0x40;
			tmp |= lo->rfatt.array[rf].att;
			BWN_PHY_WRITE(mac, 0x3c0 + nr_written, tmp);
			nr_written++;
		}
	}

	BWN_PHY_MASK(mac, 0x0060, 0xffbf);
	BWN_PHY_WRITE(mac, 0x0014, 0x0000);

	KASSERT(phy->rev >= 6, ("%s:%d: fail", __func__, __LINE__));
	BWN_PHY_SET(mac, 0x0478, 0x0800);
	BWN_PHY_MASK(mac, 0x0478, 0xfeff);
	BWN_PHY_MASK(mac, 0x0801, 0xffbf);

	bwn_phy_g_dc_lookup_init(mac, 1);
	bwn_hf_write(mac, bwn_hf_read(mac) | BWN_HF_HW_POWERCTL);
}

static void
bwn_phy_g_switch_chan(struct bwn_mac *mac, int channel, uint8_t spu)
{
	struct bwn_softc *sc = mac->mac_sc;

	if (spu != 0)
		bwn_spu_workaround(mac, channel);

	BWN_WRITE_2(mac, BWN_CHANNEL, bwn_phy_g_chan2freq(channel));

	if (channel == 14) {
		if (siba_sprom_get_ccode(sc->sc_dev) == SIBA_CCODE_JAPAN)
			bwn_hf_write(mac,
			    bwn_hf_read(mac) & ~BWN_HF_JAPAN_CHAN14_OFF);
		else
			bwn_hf_write(mac,
			    bwn_hf_read(mac) | BWN_HF_JAPAN_CHAN14_OFF);
		BWN_WRITE_2(mac, BWN_CHANNEL_EXT,
		    BWN_READ_2(mac, BWN_CHANNEL_EXT) | (1 << 11));
		return;
	}

	BWN_WRITE_2(mac, BWN_CHANNEL_EXT,
	    BWN_READ_2(mac, BWN_CHANNEL_EXT) & 0xf7bf);
}

static uint16_t
bwn_phy_g_chan2freq(uint8_t channel)
{
	static const uint8_t bwn_phy_g_rf_channels[] = BWN_PHY_G_RF_CHANNELS;

	KASSERT(channel >= 1 && channel <= 14,
	    ("%s:%d: fail", __func__, __LINE__));

	return (bwn_phy_g_rf_channels[channel - 1]);
}

static void
bwn_phy_g_set_txpwr_sub(struct bwn_mac *mac, const struct bwn_bbatt *bbatt,
    const struct bwn_rfatt *rfatt, uint8_t txctl)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_txpwr_loctl *lo = &pg->pg_loctl;
	uint16_t bb, rf;
	uint16_t tx_bias, tx_magn;

	bb = bbatt->att;
	rf = rfatt->att;
	tx_bias = lo->tx_bias;
	tx_magn = lo->tx_magn;
	if (tx_bias == 0xff)
		tx_bias = 0;

	pg->pg_txctl = txctl;
	memmove(&pg->pg_rfatt, rfatt, sizeof(*rfatt));
	pg->pg_rfatt.padmix = (txctl & BWN_TXCTL_TXMIX) ? 1 : 0;
	memmove(&pg->pg_bbatt, bbatt, sizeof(*bbatt));
	bwn_phy_g_set_bbatt(mac, bb);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_RADIO_ATT, rf);
	if (phy->rf_ver == 0x2050 && phy->rf_rev == 8)
		BWN_RF_WRITE(mac, 0x43, (rf & 0x000f) | (txctl & 0x0070));
	else {
		BWN_RF_SETMASK(mac, 0x43, 0xfff0, (rf & 0x000f));
		BWN_RF_SETMASK(mac, 0x52, ~0x0070, (txctl & 0x0070));
	}
	if (BWN_HAS_TXMAG(phy))
		BWN_RF_WRITE(mac, 0x52, tx_magn | tx_bias);
	else
		BWN_RF_SETMASK(mac, 0x52, 0xfff0, (tx_bias & 0x000f));
	bwn_lo_g_adjust(mac);
}

static void
bwn_phy_g_set_bbatt(struct bwn_mac *mac,
    uint16_t bbatt)
{
	struct bwn_phy *phy = &mac->mac_phy;

	if (phy->analog == 0) {
		BWN_WRITE_2(mac, BWN_PHY0,
		    (BWN_READ_2(mac, BWN_PHY0) & 0xfff0) | bbatt);
		return;
	}
	if (phy->analog > 1) {
		BWN_PHY_SETMASK(mac, BWN_PHY_DACCTL, 0xffc3, bbatt << 2);
		return;
	}
	BWN_PHY_SETMASK(mac, BWN_PHY_DACCTL, 0xff87, bbatt << 3);
}

static uint16_t
bwn_rf_2050_rfoverval(struct bwn_mac *mac, uint16_t reg, uint32_t lpd)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_g *pg = &phy->phy_g;
	struct bwn_softc *sc = mac->mac_sc;
	int max_lb_gain;
	uint16_t extlna;
	uint16_t i;

	if (phy->gmode == 0)
		return (0);

	if (BWN_HAS_LOOPBACK(phy)) {
		max_lb_gain = pg->pg_max_lb_gain;
		max_lb_gain += (phy->rf_rev == 8) ? 0x3e : 0x26;
		if (max_lb_gain >= 0x46) {
			extlna = 0x3000;
			max_lb_gain -= 0x46;
		} else if (max_lb_gain >= 0x3a) {
			extlna = 0x1000;
			max_lb_gain -= 0x3a;
		} else if (max_lb_gain >= 0x2e) {
			extlna = 0x2000;
			max_lb_gain -= 0x2e;
		} else {
			extlna = 0;
			max_lb_gain -= 0x10;
		}

		for (i = 0; i < 16; i++) {
			max_lb_gain -= (i * 6);
			if (max_lb_gain < 6)
				break;
		}

		if ((phy->rev < 7) ||
		    !(siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_EXTLNA)) {
			if (reg == BWN_PHY_RFOVER) {
				return (0x1b3);
			} else if (reg == BWN_PHY_RFOVERVAL) {
				extlna |= (i << 8);
				switch (lpd) {
				case BWN_LPD(0, 1, 1):
					return (0x0f92);
				case BWN_LPD(0, 0, 1):
				case BWN_LPD(1, 0, 1):
					return (0x0092 | extlna);
				case BWN_LPD(1, 0, 0):
					return (0x0093 | extlna);
				}
				KASSERT(0 == 1,
				    ("%s:%d: fail", __func__, __LINE__));
			}
			KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
		} else {
			if (reg == BWN_PHY_RFOVER)
				return (0x9b3);
			if (reg == BWN_PHY_RFOVERVAL) {
				if (extlna)
					extlna |= 0x8000;
				extlna |= (i << 8);
				switch (lpd) {
				case BWN_LPD(0, 1, 1):
					return (0x8f92);
				case BWN_LPD(0, 0, 1):
					return (0x8092 | extlna);
				case BWN_LPD(1, 0, 1):
					return (0x2092 | extlna);
				case BWN_LPD(1, 0, 0):
					return (0x2093 | extlna);
				}
				KASSERT(0 == 1,
				    ("%s:%d: fail", __func__, __LINE__));
			}
			KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
		}
		return (0);
	}

	if ((phy->rev < 7) ||
	    !(siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_EXTLNA)) {
		if (reg == BWN_PHY_RFOVER) {
			return (0x1b3);
		} else if (reg == BWN_PHY_RFOVERVAL) {
			switch (lpd) {
			case BWN_LPD(0, 1, 1):
				return (0x0fb2);
			case BWN_LPD(0, 0, 1):
				return (0x00b2);
			case BWN_LPD(1, 0, 1):
				return (0x30b2);
			case BWN_LPD(1, 0, 0):
				return (0x30b3);
			}
			KASSERT(0 == 1,
			    ("%s:%d: fail", __func__, __LINE__));
		}
		KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	} else {
		if (reg == BWN_PHY_RFOVER) {
			return (0x9b3);
		} else if (reg == BWN_PHY_RFOVERVAL) {
			switch (lpd) {
			case BWN_LPD(0, 1, 1):
				return (0x8fb2);
			case BWN_LPD(0, 0, 1):
				return (0x80b2);
			case BWN_LPD(1, 0, 1):
				return (0x20b2);
			case BWN_LPD(1, 0, 0):
				return (0x20b3);
			}
			KASSERT(0 == 1,
			    ("%s:%d: fail", __func__, __LINE__));
		}
		KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	}
	return (0);
}

static void
bwn_spu_workaround(struct bwn_mac *mac, uint8_t channel)
{

	if (mac->mac_phy.rf_ver != 0x2050 || mac->mac_phy.rf_rev >= 6)
		return;
	BWN_WRITE_2(mac, BWN_CHANNEL, (channel <= 10) ?
	    bwn_phy_g_chan2freq(channel + 4) : bwn_phy_g_chan2freq(1));
	DELAY(1000);
	BWN_WRITE_2(mac, BWN_CHANNEL, bwn_phy_g_chan2freq(channel));
}

static int
bwn_fw_gets(struct bwn_mac *mac, enum bwn_fwtype type)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_fw *fw = &mac->mac_fw;
	const uint8_t rev = siba_get_revid(sc->sc_dev);
	const char *filename;
	uint32_t high;
	int error;

	/* microcode */
	if (rev >= 5 && rev <= 10)
		filename = "ucode5";
	else if (rev >= 11 && rev <= 12)
		filename = "ucode11";
	else if (rev == 13)
		filename = "ucode13";
	else if (rev == 14)
		filename = "ucode14";
	else if (rev >= 15)
		filename = "ucode15";
	else {
		device_printf(sc->sc_dev, "no ucode for rev %d\n", rev);
		bwn_release_firmware(mac);
		return (EOPNOTSUPP);
	}
	error = bwn_fw_get(mac, type, filename, &fw->ucode);
	if (error) {
		bwn_release_firmware(mac);
		return (error);
	}

	/* PCM */
	KASSERT(fw->no_pcmfile == 0, ("%s:%d fail", __func__, __LINE__));
	if (rev >= 5 && rev <= 10) {
		error = bwn_fw_get(mac, type, "pcm5", &fw->pcm);
		if (error == ENOENT)
			fw->no_pcmfile = 1;
		else if (error) {
			bwn_release_firmware(mac);
			return (error);
		}
	} else if (rev < 11) {
		device_printf(sc->sc_dev, "no PCM for rev %d\n", rev);
		return (EOPNOTSUPP);
	}

	/* initvals */
	high = siba_read_4(sc->sc_dev, SIBA_TGSHIGH);
	switch (mac->mac_phy.type) {
	case BWN_PHYTYPE_A:
		if (rev < 5 || rev > 10)
			goto fail1;
		if (high & BWN_TGSHIGH_HAVE_2GHZ)
			filename = "a0g1initvals5";
		else
			filename = "a0g0initvals5";
		break;
	case BWN_PHYTYPE_G:
		if (rev >= 5 && rev <= 10)
			filename = "b0g0initvals5";
		else if (rev >= 13)
			filename = "b0g0initvals13";
		else
			goto fail1;
		break;
	case BWN_PHYTYPE_LP:
		if (rev == 13)
			filename = "lp0initvals13";
		else if (rev == 14)
			filename = "lp0initvals14";
		else if (rev >= 15)
			filename = "lp0initvals15";
		else
			goto fail1;
		break;
	case BWN_PHYTYPE_N:
		if (rev >= 11 && rev <= 12)
			filename = "n0initvals11";
		else
			goto fail1;
		break;
	default:
		goto fail1;
	}
	error = bwn_fw_get(mac, type, filename, &fw->initvals);
	if (error) {
		bwn_release_firmware(mac);
		return (error);
	}

	/* bandswitch initvals */
	switch (mac->mac_phy.type) {
	case BWN_PHYTYPE_A:
		if (rev >= 5 && rev <= 10) {
			if (high & BWN_TGSHIGH_HAVE_2GHZ)
				filename = "a0g1bsinitvals5";
			else
				filename = "a0g0bsinitvals5";
		} else if (rev >= 11)
			filename = NULL;
		else
			goto fail1;
		break;
	case BWN_PHYTYPE_G:
		if (rev >= 5 && rev <= 10)
			filename = "b0g0bsinitvals5";
		else if (rev >= 11)
			filename = NULL;
		else
			goto fail1;
		break;
	case BWN_PHYTYPE_LP:
		if (rev == 13)
			filename = "lp0bsinitvals13";
		else if (rev == 14)
			filename = "lp0bsinitvals14";
		else if (rev >= 15)
			filename = "lp0bsinitvals15";
		else
			goto fail1;
		break;
	case BWN_PHYTYPE_N:
		if (rev >= 11 && rev <= 12)
			filename = "n0bsinitvals11";
		else
			goto fail1;
		break;
	default:
		goto fail1;
	}
	error = bwn_fw_get(mac, type, filename, &fw->initvals_band);
	if (error) {
		bwn_release_firmware(mac);
		return (error);
	}
	return (0);
fail1:
	device_printf(sc->sc_dev, "no INITVALS for rev %d\n", rev);
	bwn_release_firmware(mac);
	return (EOPNOTSUPP);
}

static int
bwn_fw_get(struct bwn_mac *mac, enum bwn_fwtype type,
    const char *name, struct bwn_fwfile *bfw)
{
	const struct bwn_fwhdr *hdr;
	struct bwn_softc *sc = mac->mac_sc;
	const struct firmware *fw;
	char namebuf[64];

	if (name == NULL) {
		bwn_do_release_fw(bfw);
		return (0);
	}
	if (bfw->filename != NULL) {
		if (bfw->type == type && (strcmp(bfw->filename, name) == 0))
			return (0);
		bwn_do_release_fw(bfw);
	}

	snprintf(namebuf, sizeof(namebuf), "bwn%s_v4_%s%s",
	    (type == BWN_FWTYPE_OPENSOURCE) ? "-open" : "",
	    (mac->mac_phy.type == BWN_PHYTYPE_LP) ? "lp_" : "", name);
	/* XXX Sleeping on "fwload" with the non-sleepable locks held */
	fw = firmware_get(namebuf);
	if (fw == NULL) {
		device_printf(sc->sc_dev, "the fw file(%s) not found\n",
		    namebuf);
		return (ENOENT);
	}
	if (fw->datasize < sizeof(struct bwn_fwhdr))
		goto fail;
	hdr = (const struct bwn_fwhdr *)(fw->data);
	switch (hdr->type) {
	case BWN_FWTYPE_UCODE:
	case BWN_FWTYPE_PCM:
		if (be32toh(hdr->size) !=
		    (fw->datasize - sizeof(struct bwn_fwhdr)))
			goto fail;
		/* FALLTHROUGH */
	case BWN_FWTYPE_IV:
		if (hdr->ver != 1)
			goto fail;
		break;
	default:
		goto fail;
	}
	bfw->filename = name;
	bfw->fw = fw;
	bfw->type = type;
	return (0);
fail:
	device_printf(sc->sc_dev, "the fw file(%s) format error\n", namebuf);
	if (fw != NULL)
		firmware_put(fw, FIRMWARE_UNLOAD);
	return (EPROTO);
}

static void
bwn_release_firmware(struct bwn_mac *mac)
{

	bwn_do_release_fw(&mac->mac_fw.ucode);
	bwn_do_release_fw(&mac->mac_fw.pcm);
	bwn_do_release_fw(&mac->mac_fw.initvals);
	bwn_do_release_fw(&mac->mac_fw.initvals_band);
}

static void
bwn_do_release_fw(struct bwn_fwfile *bfw)
{

	if (bfw->fw != NULL)
		firmware_put(bfw->fw, FIRMWARE_UNLOAD);
	bfw->fw = NULL;
	bfw->filename = NULL;
}

static int
bwn_fw_loaducode(struct bwn_mac *mac)
{
#define	GETFWOFFSET(fwp, offset)	\
	((const uint32_t *)((const char *)fwp.fw->data + offset))
#define	GETFWSIZE(fwp, offset)	\
	((fwp.fw->datasize - offset) / sizeof(uint32_t))
	struct bwn_softc *sc = mac->mac_sc;
	const uint32_t *data;
	unsigned int i;
	uint32_t ctl;
	uint16_t date, fwcaps, time;
	int error = 0;

	ctl = BWN_READ_4(mac, BWN_MACCTL);
	ctl |= BWN_MACCTL_MCODE_JMP0;
	KASSERT(!(ctl & BWN_MACCTL_MCODE_RUN), ("%s:%d: fail", __func__,
	    __LINE__));
	BWN_WRITE_4(mac, BWN_MACCTL, ctl);
	for (i = 0; i < 64; i++)
		bwn_shm_write_2(mac, BWN_SCRATCH, i, 0);
	for (i = 0; i < 4096; i += 2)
		bwn_shm_write_2(mac, BWN_SHARED, i, 0);

	data = GETFWOFFSET(mac->mac_fw.ucode, sizeof(struct bwn_fwhdr));
	bwn_shm_ctlword(mac, BWN_UCODE | BWN_SHARED_AUTOINC, 0x0000);
	for (i = 0; i < GETFWSIZE(mac->mac_fw.ucode, sizeof(struct bwn_fwhdr));
	     i++) {
		BWN_WRITE_4(mac, BWN_SHM_DATA, be32toh(data[i]));
		DELAY(10);
	}

	if (mac->mac_fw.pcm.fw) {
		data = GETFWOFFSET(mac->mac_fw.pcm, sizeof(struct bwn_fwhdr));
		bwn_shm_ctlword(mac, BWN_HW, 0x01ea);
		BWN_WRITE_4(mac, BWN_SHM_DATA, 0x00004000);
		bwn_shm_ctlword(mac, BWN_HW, 0x01eb);
		for (i = 0; i < GETFWSIZE(mac->mac_fw.pcm,
		    sizeof(struct bwn_fwhdr)); i++) {
			BWN_WRITE_4(mac, BWN_SHM_DATA, be32toh(data[i]));
			DELAY(10);
		}
	}

	BWN_WRITE_4(mac, BWN_INTR_REASON, BWN_INTR_ALL);
	BWN_WRITE_4(mac, BWN_MACCTL,
	    (BWN_READ_4(mac, BWN_MACCTL) & ~BWN_MACCTL_MCODE_JMP0) |
	    BWN_MACCTL_MCODE_RUN);

	for (i = 0; i < 21; i++) {
		if (BWN_READ_4(mac, BWN_INTR_REASON) == BWN_INTR_MAC_SUSPENDED)
			break;
		if (i >= 20) {
			device_printf(sc->sc_dev, "ucode timeout\n");
			error = ENXIO;
			goto error;
		}
		DELAY(50000);
	}
	BWN_READ_4(mac, BWN_INTR_REASON);

	mac->mac_fw.rev = bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_UCODE_REV);
	if (mac->mac_fw.rev <= 0x128) {
		device_printf(sc->sc_dev, "the firmware is too old\n");
		error = EOPNOTSUPP;
		goto error;
	}
	mac->mac_fw.patch = bwn_shm_read_2(mac, BWN_SHARED,
	    BWN_SHARED_UCODE_PATCH);
	date = bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_UCODE_DATE);
	mac->mac_fw.opensource = (date == 0xffff);
	if (bwn_wme != 0)
		mac->mac_flags |= BWN_MAC_FLAG_WME;
	mac->mac_flags |= BWN_MAC_FLAG_HWCRYPTO;

	time = bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_UCODE_TIME);
	if (mac->mac_fw.opensource == 0) {
		device_printf(sc->sc_dev,
		    "firmware version (rev %u patch %u date %#x time %#x)\n",
		    mac->mac_fw.rev, mac->mac_fw.patch, date, time);
		if (mac->mac_fw.no_pcmfile)
			device_printf(sc->sc_dev,
			    "no HW crypto acceleration due to pcm5\n");
	} else {
		mac->mac_fw.patch = time;
		fwcaps = bwn_fwcaps_read(mac);
		if (!(fwcaps & BWN_FWCAPS_HWCRYPTO) || mac->mac_fw.no_pcmfile) {
			device_printf(sc->sc_dev,
			    "disabling HW crypto acceleration\n");
			mac->mac_flags &= ~BWN_MAC_FLAG_HWCRYPTO;
		}
		if (!(fwcaps & BWN_FWCAPS_WME)) {
			device_printf(sc->sc_dev, "disabling WME support\n");
			mac->mac_flags &= ~BWN_MAC_FLAG_WME;
		}
	}

	if (BWN_ISOLDFMT(mac))
		device_printf(sc->sc_dev, "using old firmware image\n");

	return (0);

error:
	BWN_WRITE_4(mac, BWN_MACCTL,
	    (BWN_READ_4(mac, BWN_MACCTL) & ~BWN_MACCTL_MCODE_RUN) |
	    BWN_MACCTL_MCODE_JMP0);

	return (error);
#undef GETFWSIZE
#undef GETFWOFFSET
}

/* OpenFirmware only */
static uint16_t
bwn_fwcaps_read(struct bwn_mac *mac)
{

	KASSERT(mac->mac_fw.opensource == 1,
	    ("%s:%d: fail", __func__, __LINE__));
	return (bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_FWCAPS));
}

static int
bwn_fwinitvals_write(struct bwn_mac *mac, const struct bwn_fwinitvals *ivals,
    size_t count, size_t array_size)
{
#define	GET_NEXTIV16(iv)						\
	((const struct bwn_fwinitvals *)((const uint8_t *)(iv) +	\
	    sizeof(uint16_t) + sizeof(uint16_t)))
#define	GET_NEXTIV32(iv)						\
	((const struct bwn_fwinitvals *)((const uint8_t *)(iv) +	\
	    sizeof(uint16_t) + sizeof(uint32_t)))
	struct bwn_softc *sc = mac->mac_sc;
	const struct bwn_fwinitvals *iv;
	uint16_t offset;
	size_t i;
	uint8_t bit32;

	KASSERT(sizeof(struct bwn_fwinitvals) == 6,
	    ("%s:%d: fail", __func__, __LINE__));
	iv = ivals;
	for (i = 0; i < count; i++) {
		if (array_size < sizeof(iv->offset_size))
			goto fail;
		array_size -= sizeof(iv->offset_size);
		offset = be16toh(iv->offset_size);
		bit32 = (offset & BWN_FWINITVALS_32BIT) ? 1 : 0;
		offset &= BWN_FWINITVALS_OFFSET_MASK;
		if (offset >= 0x1000)
			goto fail;
		if (bit32) {
			if (array_size < sizeof(iv->data.d32))
				goto fail;
			array_size -= sizeof(iv->data.d32);
			BWN_WRITE_4(mac, offset, be32toh(iv->data.d32));
			iv = GET_NEXTIV32(iv);
		} else {

			if (array_size < sizeof(iv->data.d16))
				goto fail;
			array_size -= sizeof(iv->data.d16);
			BWN_WRITE_2(mac, offset, be16toh(iv->data.d16));

			iv = GET_NEXTIV16(iv);
		}
	}
	if (array_size != 0)
		goto fail;
	return (0);
fail:
	device_printf(sc->sc_dev, "initvals: invalid format\n");
	return (EPROTO);
#undef GET_NEXTIV16
#undef GET_NEXTIV32
}

static int
bwn_switch_channel(struct bwn_mac *mac, int chan)
{
	struct bwn_phy *phy = &(mac->mac_phy);
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint16_t channelcookie, savedcookie;
	int error;

	if (chan == 0xffff)
		chan = phy->get_default_chan(mac);

	channelcookie = chan;
	if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan))
		channelcookie |= 0x100;
	savedcookie = bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_CHAN);
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_CHAN, channelcookie);
	error = phy->switch_channel(mac, chan);
	if (error)
		goto fail;

	mac->mac_phy.chan = chan;
	DELAY(8000);
	return (0);
fail:
	device_printf(sc->sc_dev, "failed to switch channel\n");
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_CHAN, savedcookie);
	return (error);
}

static uint16_t
bwn_ant2phy(int antenna)
{

	switch (antenna) {
	case BWN_ANT0:
		return (BWN_TX_PHY_ANT0);
	case BWN_ANT1:
		return (BWN_TX_PHY_ANT1);
	case BWN_ANT2:
		return (BWN_TX_PHY_ANT2);
	case BWN_ANT3:
		return (BWN_TX_PHY_ANT3);
	case BWN_ANTAUTO:
		return (BWN_TX_PHY_ANT01AUTO);
	}
	KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	return (0);
}

static void
bwn_wme_load(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	int i;

	KASSERT(N(bwn_wme_shm_offsets) == N(sc->sc_wmeParams),
	    ("%s:%d: fail", __func__, __LINE__));

	bwn_mac_suspend(mac);
	for (i = 0; i < N(sc->sc_wmeParams); i++)
		bwn_wme_loadparams(mac, &(sc->sc_wmeParams[i]),
		    bwn_wme_shm_offsets[i]);
	bwn_mac_enable(mac);
}

static void
bwn_wme_loadparams(struct bwn_mac *mac,
    const struct wmeParams *p, uint16_t shm_offset)
{
#define	SM(_v, _f)      (((_v) << _f##_S) & _f)
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t params[BWN_NR_WMEPARAMS];
	int slot, tmp;
	unsigned int i;

	slot = BWN_READ_2(mac, BWN_RNG) &
	    SM(p->wmep_logcwmin, WME_PARAM_LOGCWMIN);

	memset(&params, 0, sizeof(params));

	DPRINTF(sc, BWN_DEBUG_WME, "wmep_txopLimit %d wmep_logcwmin %d "
	    "wmep_logcwmax %d wmep_aifsn %d\n", p->wmep_txopLimit,
	    p->wmep_logcwmin, p->wmep_logcwmax, p->wmep_aifsn);

	params[BWN_WMEPARAM_TXOP] = p->wmep_txopLimit * 32;
	params[BWN_WMEPARAM_CWMIN] = SM(p->wmep_logcwmin, WME_PARAM_LOGCWMIN);
	params[BWN_WMEPARAM_CWMAX] = SM(p->wmep_logcwmax, WME_PARAM_LOGCWMAX);
	params[BWN_WMEPARAM_CWCUR] = SM(p->wmep_logcwmin, WME_PARAM_LOGCWMIN);
	params[BWN_WMEPARAM_AIFS] = p->wmep_aifsn;
	params[BWN_WMEPARAM_BSLOTS] = slot;
	params[BWN_WMEPARAM_REGGAP] = slot + p->wmep_aifsn;

	for (i = 0; i < N(params); i++) {
		if (i == BWN_WMEPARAM_STATUS) {
			tmp = bwn_shm_read_2(mac, BWN_SHARED,
			    shm_offset + (i * 2));
			tmp |= 0x100;
			bwn_shm_write_2(mac, BWN_SHARED, shm_offset + (i * 2),
			    tmp);
		} else {
			bwn_shm_write_2(mac, BWN_SHARED, shm_offset + (i * 2),
			    params[i]);
		}
	}
}

static void
bwn_mac_write_bssid(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t tmp;
	int i;
	uint8_t mac_bssid[IEEE80211_ADDR_LEN * 2];

	bwn_mac_setfilter(mac, BWN_MACFILTER_BSSID, sc->sc_bssid);
	memcpy(mac_bssid, sc->sc_macaddr, IEEE80211_ADDR_LEN);
	memcpy(mac_bssid + IEEE80211_ADDR_LEN, sc->sc_bssid,
	    IEEE80211_ADDR_LEN);

	for (i = 0; i < N(mac_bssid); i += sizeof(uint32_t)) {
		tmp = (uint32_t) (mac_bssid[i + 0]);
		tmp |= (uint32_t) (mac_bssid[i + 1]) << 8;
		tmp |= (uint32_t) (mac_bssid[i + 2]) << 16;
		tmp |= (uint32_t) (mac_bssid[i + 3]) << 24;
		bwn_ram_write(mac, 0x20 + i, tmp);
	}
}

static void
bwn_mac_setfilter(struct bwn_mac *mac, uint16_t offset,
    const uint8_t *macaddr)
{
	static const uint8_t zero[IEEE80211_ADDR_LEN] = { 0 };
	uint16_t data;

	if (!mac)
		macaddr = zero;

	offset |= 0x0020;
	BWN_WRITE_2(mac, BWN_MACFILTER_CONTROL, offset);

	data = macaddr[0];
	data |= macaddr[1] << 8;
	BWN_WRITE_2(mac, BWN_MACFILTER_DATA, data);
	data = macaddr[2];
	data |= macaddr[3] << 8;
	BWN_WRITE_2(mac, BWN_MACFILTER_DATA, data);
	data = macaddr[4];
	data |= macaddr[5] << 8;
	BWN_WRITE_2(mac, BWN_MACFILTER_DATA, data);
}

static void
bwn_key_dowrite(struct bwn_mac *mac, uint8_t index, uint8_t algorithm,
    const uint8_t *key, size_t key_len, const uint8_t *mac_addr)
{
	uint8_t buf[BWN_SEC_KEYSIZE] = { 0, };
	uint8_t per_sta_keys_start = 8;

	if (BWN_SEC_NEWAPI(mac))
		per_sta_keys_start = 4;

	KASSERT(index < mac->mac_max_nr_keys,
	    ("%s:%d: fail", __func__, __LINE__));
	KASSERT(key_len <= BWN_SEC_KEYSIZE,
	    ("%s:%d: fail", __func__, __LINE__));

	if (index >= per_sta_keys_start)
		bwn_key_macwrite(mac, index, NULL);
	if (key)
		memcpy(buf, key, key_len);
	bwn_key_write(mac, index, algorithm, buf);
	if (index >= per_sta_keys_start)
		bwn_key_macwrite(mac, index, mac_addr);

	mac->mac_key[index].algorithm = algorithm;
}

static void
bwn_key_macwrite(struct bwn_mac *mac, uint8_t index, const uint8_t *addr)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t addrtmp[2] = { 0, 0 };
	uint8_t start = 8;

	if (BWN_SEC_NEWAPI(mac))
		start = 4;

	KASSERT(index >= start,
	    ("%s:%d: fail", __func__, __LINE__));
	index -= start;

	if (addr) {
		addrtmp[0] = addr[0];
		addrtmp[0] |= ((uint32_t) (addr[1]) << 8);
		addrtmp[0] |= ((uint32_t) (addr[2]) << 16);
		addrtmp[0] |= ((uint32_t) (addr[3]) << 24);
		addrtmp[1] = addr[4];
		addrtmp[1] |= ((uint32_t) (addr[5]) << 8);
	}

	if (siba_get_revid(sc->sc_dev) >= 5) {
		bwn_shm_write_4(mac, BWN_RCMTA, (index * 2) + 0, addrtmp[0]);
		bwn_shm_write_2(mac, BWN_RCMTA, (index * 2) + 1, addrtmp[1]);
	} else {
		if (index >= 8) {
			bwn_shm_write_4(mac, BWN_SHARED,
			    BWN_SHARED_PSM + (index * 6) + 0, addrtmp[0]);
			bwn_shm_write_2(mac, BWN_SHARED,
			    BWN_SHARED_PSM + (index * 6) + 4, addrtmp[1]);
		}
	}
}

static void
bwn_key_write(struct bwn_mac *mac, uint8_t index, uint8_t algorithm,
    const uint8_t *key)
{
	unsigned int i;
	uint32_t offset;
	uint16_t kidx, value;

	kidx = BWN_SEC_KEY2FW(mac, index);
	bwn_shm_write_2(mac, BWN_SHARED,
	    BWN_SHARED_KEYIDX_BLOCK + (kidx * 2), (kidx << 4) | algorithm);

	offset = mac->mac_ktp + (index * BWN_SEC_KEYSIZE);
	for (i = 0; i < BWN_SEC_KEYSIZE; i += 2) {
		value = key[i];
		value |= (uint16_t)(key[i + 1]) << 8;
		bwn_shm_write_2(mac, BWN_SHARED, offset + i, value);
	}
}

static void
bwn_phy_exit(struct bwn_mac *mac)
{

	mac->mac_phy.rf_onoff(mac, 0);
	if (mac->mac_phy.exit != NULL)
		mac->mac_phy.exit(mac);
}

static void
bwn_dma_free(struct bwn_mac *mac)
{
	struct bwn_dma *dma;

	if ((mac->mac_flags & BWN_MAC_FLAG_DMA) == 0)
		return;
	dma = &mac->mac_method.dma;

	bwn_dma_ringfree(&dma->rx);
	bwn_dma_ringfree(&dma->wme[WME_AC_BK]);
	bwn_dma_ringfree(&dma->wme[WME_AC_BE]);
	bwn_dma_ringfree(&dma->wme[WME_AC_VI]);
	bwn_dma_ringfree(&dma->wme[WME_AC_VO]);
	bwn_dma_ringfree(&dma->mcast);
}

static void
bwn_core_stop(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;

	BWN_ASSERT_LOCKED(sc);

	if (mac->mac_status < BWN_MAC_STATUS_STARTED)
		return;

	callout_stop(&sc->sc_rfswitch_ch);
	callout_stop(&sc->sc_task_ch);
	callout_stop(&sc->sc_watchdog_ch);
	sc->sc_watchdog_timer = 0;
	BWN_WRITE_4(mac, BWN_INTR_MASK, 0);
	BWN_READ_4(mac, BWN_INTR_MASK);
	bwn_mac_suspend(mac);

	mac->mac_status = BWN_MAC_STATUS_INITED;
}

static int
bwn_switch_band(struct bwn_softc *sc, struct ieee80211_channel *chan)
{
	struct bwn_mac *up_dev = NULL;
	struct bwn_mac *down_dev;
	struct bwn_mac *mac;
	int err, status;
	uint8_t gmode;

	BWN_ASSERT_LOCKED(sc);

	TAILQ_FOREACH(mac, &sc->sc_maclist, mac_list) {
		if (IEEE80211_IS_CHAN_2GHZ(chan) &&
		    mac->mac_phy.supports_2ghz) {
			up_dev = mac;
			gmode = 1;
		} else if (IEEE80211_IS_CHAN_5GHZ(chan) &&
		    mac->mac_phy.supports_5ghz) {
			up_dev = mac;
			gmode = 0;
		} else {
			KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
			return (EINVAL);
		}
		if (up_dev != NULL)
			break;
	}
	if (up_dev == NULL) {
		device_printf(sc->sc_dev, "Could not find a device\n");
		return (ENODEV);
	}
	if (up_dev == sc->sc_curmac && sc->sc_curmac->mac_phy.gmode == gmode)
		return (0);

	device_printf(sc->sc_dev, "switching to %s-GHz band\n",
	    IEEE80211_IS_CHAN_2GHZ(chan) ? "2" : "5");

	down_dev = sc->sc_curmac;
	status = down_dev->mac_status;
	if (status >= BWN_MAC_STATUS_STARTED)
		bwn_core_stop(down_dev);
	if (status >= BWN_MAC_STATUS_INITED)
		bwn_core_exit(down_dev);

	if (down_dev != up_dev)
		bwn_phy_reset(down_dev);

	up_dev->mac_phy.gmode = gmode;
	if (status >= BWN_MAC_STATUS_INITED) {
		err = bwn_core_init(up_dev);
		if (err) {
			device_printf(sc->sc_dev,
			    "fatal: failed to initialize for %s-GHz\n",
			    IEEE80211_IS_CHAN_2GHZ(chan) ? "2" : "5");
			goto fail;
		}
	}
	if (status >= BWN_MAC_STATUS_STARTED)
		bwn_core_start(up_dev);
	KASSERT(up_dev->mac_status == status, ("%s: fail", __func__));
	sc->sc_curmac = up_dev;

	return (0);
fail:
	sc->sc_curmac = NULL;
	return (err);
}

static void
bwn_rf_turnon(struct bwn_mac *mac)
{

	bwn_mac_suspend(mac);
	mac->mac_phy.rf_onoff(mac, 1);
	mac->mac_phy.rf_on = 1;
	bwn_mac_enable(mac);
}

static void
bwn_rf_turnoff(struct bwn_mac *mac)
{

	bwn_mac_suspend(mac);
	mac->mac_phy.rf_onoff(mac, 0);
	mac->mac_phy.rf_on = 0;
	bwn_mac_enable(mac);
}

static void
bwn_phy_reset(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;

	siba_write_4(sc->sc_dev, SIBA_TGSLOW,
	    ((siba_read_4(sc->sc_dev, SIBA_TGSLOW) & ~BWN_TGSLOW_SUPPORT_G) |
	     BWN_TGSLOW_PHYRESET) | SIBA_TGSLOW_FGC);
	DELAY(1000);
	siba_write_4(sc->sc_dev, SIBA_TGSLOW,
	    (siba_read_4(sc->sc_dev, SIBA_TGSLOW) & ~SIBA_TGSLOW_FGC) |
	    BWN_TGSLOW_PHYRESET);
	DELAY(1000);
}

static int
bwn_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	const struct ieee80211_txparam *tp;
	struct bwn_vap *bvp = BWN_VAP(vap);
	struct ieee80211com *ic= vap->iv_ic;
	struct ifnet *ifp = ic->ic_ifp;
	enum ieee80211_state ostate = vap->iv_state;
	struct bwn_softc *sc = ifp->if_softc;
	struct bwn_mac *mac = sc->sc_curmac;
	int error;

	DPRINTF(sc, BWN_DEBUG_STATE, "%s: %s -> %s\n", __func__,
	    ieee80211_state_name[vap->iv_state],
	    ieee80211_state_name[nstate]);

	error = bvp->bv_newstate(vap, nstate, arg);
	if (error != 0)
		return (error);

	BWN_LOCK(sc);

	bwn_led_newstate(mac, nstate);

	/*
	 * Clear the BSSID when we stop a STA
	 */
	if (vap->iv_opmode == IEEE80211_M_STA) {
		if (ostate == IEEE80211_S_RUN && nstate != IEEE80211_S_RUN) {
			/*
			 * Clear out the BSSID.  If we reassociate to
			 * the same AP, this will reinialize things
			 * correctly...
			 */
			if (ic->ic_opmode == IEEE80211_M_STA &&
			    (sc->sc_flags & BWN_FLAG_INVALID) == 0) {
				memset(sc->sc_bssid, 0, IEEE80211_ADDR_LEN);
				bwn_set_macaddr(mac);
			}
		}
	}

	if (vap->iv_opmode == IEEE80211_M_MONITOR ||
	    vap->iv_opmode == IEEE80211_M_AHDEMO) {
		/* XXX nothing to do? */
	} else if (nstate == IEEE80211_S_RUN) {
		memcpy(sc->sc_bssid, vap->iv_bss->ni_bssid, IEEE80211_ADDR_LEN);
		memcpy(sc->sc_macaddr, IF_LLADDR(ifp), IEEE80211_ADDR_LEN);
		bwn_set_opmode(mac);
		bwn_set_pretbtt(mac);
		bwn_spu_setdelay(mac, 0);
		bwn_set_macaddr(mac);

		/* Initializes ratectl for a node. */
		tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];
		if (tp->ucastrate == IEEE80211_FIXED_RATE_NONE)
			ieee80211_ratectl_node_init(vap->iv_bss);
	}

	BWN_UNLOCK(sc);

	return (error);
}

static void
bwn_set_pretbtt(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	uint16_t pretbtt;

	if (ic->ic_opmode == IEEE80211_M_IBSS)
		pretbtt = 2;
	else
		pretbtt = (mac->mac_phy.type == BWN_PHYTYPE_A) ? 120 : 250;
	bwn_shm_write_2(mac, BWN_SHARED, BWN_SHARED_PRETBTT, pretbtt);
	BWN_WRITE_2(mac, BWN_TSF_CFP_PRETBTT, pretbtt);
}

static int
bwn_intr(void *arg)
{
	struct bwn_mac *mac = arg;
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t reason;

	if (mac->mac_status < BWN_MAC_STATUS_STARTED ||
	    (sc->sc_flags & BWN_FLAG_INVALID))
		return (FILTER_STRAY);

	reason = BWN_READ_4(mac, BWN_INTR_REASON);
	if (reason == 0xffffffff)	/* shared IRQ */
		return (FILTER_STRAY);
	reason &= mac->mac_intr_mask;
	if (reason == 0)
		return (FILTER_HANDLED);

	mac->mac_reason[0] = BWN_READ_4(mac, BWN_DMA0_REASON) & 0x0001dc00;
	mac->mac_reason[1] = BWN_READ_4(mac, BWN_DMA1_REASON) & 0x0000dc00;
	mac->mac_reason[2] = BWN_READ_4(mac, BWN_DMA2_REASON) & 0x0000dc00;
	mac->mac_reason[3] = BWN_READ_4(mac, BWN_DMA3_REASON) & 0x0001dc00;
	mac->mac_reason[4] = BWN_READ_4(mac, BWN_DMA4_REASON) & 0x0000dc00;
	BWN_WRITE_4(mac, BWN_INTR_REASON, reason);
	BWN_WRITE_4(mac, BWN_DMA0_REASON, mac->mac_reason[0]);
	BWN_WRITE_4(mac, BWN_DMA1_REASON, mac->mac_reason[1]);
	BWN_WRITE_4(mac, BWN_DMA2_REASON, mac->mac_reason[2]);
	BWN_WRITE_4(mac, BWN_DMA3_REASON, mac->mac_reason[3]);
	BWN_WRITE_4(mac, BWN_DMA4_REASON, mac->mac_reason[4]);

	/* Disable interrupts. */
	BWN_WRITE_4(mac, BWN_INTR_MASK, 0);

	mac->mac_reason_intr = reason;

	BWN_BARRIER(mac, BUS_SPACE_BARRIER_READ);
	BWN_BARRIER(mac, BUS_SPACE_BARRIER_WRITE);

	taskqueue_enqueue_fast(sc->sc_tq, &mac->mac_intrtask);
	return (FILTER_HANDLED);
}

static void
bwn_intrtask(void *arg, int npending)
{
	struct bwn_mac *mac = arg;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	uint32_t merged = 0;
	int i, tx = 0, rx = 0;

	BWN_LOCK(sc);
	if (mac->mac_status < BWN_MAC_STATUS_STARTED ||
	    (sc->sc_flags & BWN_FLAG_INVALID)) {
		BWN_UNLOCK(sc);
		return;
	}

	for (i = 0; i < N(mac->mac_reason); i++)
		merged |= mac->mac_reason[i];

	if (mac->mac_reason_intr & BWN_INTR_MAC_TXERR)
		device_printf(sc->sc_dev, "MAC trans error\n");

	if (mac->mac_reason_intr & BWN_INTR_PHY_TXERR) {
		DPRINTF(sc, BWN_DEBUG_INTR, "%s: PHY trans error\n", __func__);
		mac->mac_phy.txerrors--;
		if (mac->mac_phy.txerrors == 0) {
			mac->mac_phy.txerrors = BWN_TXERROR_MAX;
			bwn_restart(mac, "PHY TX errors");
		}
	}

	if (merged & (BWN_DMAINTR_FATALMASK | BWN_DMAINTR_NONFATALMASK)) {
		if (merged & BWN_DMAINTR_FATALMASK) {
			device_printf(sc->sc_dev,
			    "Fatal DMA error: %#x %#x %#x %#x %#x %#x\n",
			    mac->mac_reason[0], mac->mac_reason[1],
			    mac->mac_reason[2], mac->mac_reason[3],
			    mac->mac_reason[4], mac->mac_reason[5]);
			bwn_restart(mac, "DMA error");
			BWN_UNLOCK(sc);
			return;
		}
		if (merged & BWN_DMAINTR_NONFATALMASK) {
			device_printf(sc->sc_dev,
			    "DMA error: %#x %#x %#x %#x %#x %#x\n",
			    mac->mac_reason[0], mac->mac_reason[1],
			    mac->mac_reason[2], mac->mac_reason[3],
			    mac->mac_reason[4], mac->mac_reason[5]);
		}
	}

	if (mac->mac_reason_intr & BWN_INTR_UCODE_DEBUG)
		bwn_intr_ucode_debug(mac);
	if (mac->mac_reason_intr & BWN_INTR_TBTT_INDI)
		bwn_intr_tbtt_indication(mac);
	if (mac->mac_reason_intr & BWN_INTR_ATIM_END)
		bwn_intr_atim_end(mac);
	if (mac->mac_reason_intr & BWN_INTR_BEACON)
		bwn_intr_beacon(mac);
	if (mac->mac_reason_intr & BWN_INTR_PMQ)
		bwn_intr_pmq(mac);
	if (mac->mac_reason_intr & BWN_INTR_NOISESAMPLE_OK)
		bwn_intr_noise(mac);

	if (mac->mac_flags & BWN_MAC_FLAG_DMA) {
		if (mac->mac_reason[0] & BWN_DMAINTR_RX_DONE) {
			bwn_dma_rx(mac->mac_method.dma.rx);
			rx = 1;
		}
	} else
		rx = bwn_pio_rx(&mac->mac_method.pio.rx);

	KASSERT(!(mac->mac_reason[1] & BWN_DMAINTR_RX_DONE), ("%s", __func__));
	KASSERT(!(mac->mac_reason[2] & BWN_DMAINTR_RX_DONE), ("%s", __func__));
	KASSERT(!(mac->mac_reason[3] & BWN_DMAINTR_RX_DONE), ("%s", __func__));
	KASSERT(!(mac->mac_reason[4] & BWN_DMAINTR_RX_DONE), ("%s", __func__));
	KASSERT(!(mac->mac_reason[5] & BWN_DMAINTR_RX_DONE), ("%s", __func__));

	if (mac->mac_reason_intr & BWN_INTR_TX_OK) {
		bwn_intr_txeof(mac);
		tx = 1;
	}

	BWN_WRITE_4(mac, BWN_INTR_MASK, mac->mac_intr_mask);

	if (sc->sc_blink_led != NULL && sc->sc_led_blink) {
		int evt = BWN_LED_EVENT_NONE;

		if (tx && rx) {
			if (sc->sc_rx_rate > sc->sc_tx_rate)
				evt = BWN_LED_EVENT_RX;
			else
				evt = BWN_LED_EVENT_TX;
		} else if (tx) {
			evt = BWN_LED_EVENT_TX;
		} else if (rx) {
			evt = BWN_LED_EVENT_RX;
		} else if (rx == 0) {
			evt = BWN_LED_EVENT_POLL;
		}

		if (evt != BWN_LED_EVENT_NONE)
			bwn_led_event(mac, evt);
       }

	if ((ifp->if_drv_flags & IFF_DRV_OACTIVE) == 0) {
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			bwn_start_locked(ifp);
	}

	BWN_BARRIER(mac, BUS_SPACE_BARRIER_READ);
	BWN_BARRIER(mac, BUS_SPACE_BARRIER_WRITE);

	BWN_UNLOCK(sc);
}

static void
bwn_restart(struct bwn_mac *mac, const char *msg)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	if (mac->mac_status < BWN_MAC_STATUS_INITED)
		return;

	device_printf(sc->sc_dev, "HW reset: %s\n", msg);
	ieee80211_runtask(ic, &mac->mac_hwreset);
}

static void
bwn_intr_ucode_debug(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t reason;

	if (mac->mac_fw.opensource == 0)
		return;

	reason = bwn_shm_read_2(mac, BWN_SCRATCH, BWN_DEBUGINTR_REASON_REG);
	switch (reason) {
	case BWN_DEBUGINTR_PANIC:
		bwn_handle_fwpanic(mac);
		break;
	case BWN_DEBUGINTR_DUMP_SHM:
		device_printf(sc->sc_dev, "BWN_DEBUGINTR_DUMP_SHM\n");
		break;
	case BWN_DEBUGINTR_DUMP_REGS:
		device_printf(sc->sc_dev, "BWN_DEBUGINTR_DUMP_REGS\n");
		break;
	case BWN_DEBUGINTR_MARKER:
		device_printf(sc->sc_dev, "BWN_DEBUGINTR_MARKER\n");
		break;
	default:
		device_printf(sc->sc_dev,
		    "ucode debug unknown reason: %#x\n", reason);
	}

	bwn_shm_write_2(mac, BWN_SCRATCH, BWN_DEBUGINTR_REASON_REG,
	    BWN_DEBUGINTR_ACK);
}

static void
bwn_intr_tbtt_indication(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;

	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
		bwn_psctl(mac, 0);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		mac->mac_flags |= BWN_MAC_FLAG_DFQVALID;
}

static void
bwn_intr_atim_end(struct bwn_mac *mac)
{

	if (mac->mac_flags & BWN_MAC_FLAG_DFQVALID) {
		BWN_WRITE_4(mac, BWN_MACCMD,
		    BWN_READ_4(mac, BWN_MACCMD) | BWN_MACCMD_DFQ_VALID);
		mac->mac_flags &= ~BWN_MAC_FLAG_DFQVALID;
	}
}

static void
bwn_intr_beacon(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;
	uint32_t cmd, beacon0, beacon1;

	if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
	    ic->ic_opmode == IEEE80211_M_MBSS)
		return;

	mac->mac_intr_mask &= ~BWN_INTR_BEACON;

	cmd = BWN_READ_4(mac, BWN_MACCMD);
	beacon0 = (cmd & BWN_MACCMD_BEACON0_VALID);
	beacon1 = (cmd & BWN_MACCMD_BEACON1_VALID);

	if (beacon0 && beacon1) {
		BWN_WRITE_4(mac, BWN_INTR_REASON, BWN_INTR_BEACON);
		mac->mac_intr_mask |= BWN_INTR_BEACON;
		return;
	}

	if (sc->sc_flags & BWN_FLAG_NEED_BEACON_TP) {
		sc->sc_flags &= ~BWN_FLAG_NEED_BEACON_TP;
		bwn_load_beacon0(mac);
		bwn_load_beacon1(mac);
		cmd = BWN_READ_4(mac, BWN_MACCMD);
		cmd |= BWN_MACCMD_BEACON0_VALID;
		BWN_WRITE_4(mac, BWN_MACCMD, cmd);
	} else {
		if (!beacon0) {
			bwn_load_beacon0(mac);
			cmd = BWN_READ_4(mac, BWN_MACCMD);
			cmd |= BWN_MACCMD_BEACON0_VALID;
			BWN_WRITE_4(mac, BWN_MACCMD, cmd);
		} else if (!beacon1) {
			bwn_load_beacon1(mac);
			cmd = BWN_READ_4(mac, BWN_MACCMD);
			cmd |= BWN_MACCMD_BEACON1_VALID;
			BWN_WRITE_4(mac, BWN_MACCMD, cmd);
		}
	}
}

static void
bwn_intr_pmq(struct bwn_mac *mac)
{
	uint32_t tmp;

	while (1) {
		tmp = BWN_READ_4(mac, BWN_PS_STATUS);
		if (!(tmp & 0x00000008))
			break;
	}
	BWN_WRITE_2(mac, BWN_PS_STATUS, 0x0002);
}

static void
bwn_intr_noise(struct bwn_mac *mac)
{
	struct bwn_phy_g *pg = &mac->mac_phy.phy_g;
	uint16_t tmp;
	uint8_t noise[4];
	uint8_t i, j;
	int32_t average;

	if (mac->mac_phy.type != BWN_PHYTYPE_G)
		return;

	KASSERT(mac->mac_noise.noi_running, ("%s: fail", __func__));
	*((uint32_t *)noise) = htole32(bwn_jssi_read(mac));
	if (noise[0] == 0x7f || noise[1] == 0x7f || noise[2] == 0x7f ||
	    noise[3] == 0x7f)
		goto new;

	KASSERT(mac->mac_noise.noi_nsamples < 8,
	    ("%s:%d: fail", __func__, __LINE__));
	i = mac->mac_noise.noi_nsamples;
	noise[0] = MIN(MAX(noise[0], 0), N(pg->pg_nrssi_lt) - 1);
	noise[1] = MIN(MAX(noise[1], 0), N(pg->pg_nrssi_lt) - 1);
	noise[2] = MIN(MAX(noise[2], 0), N(pg->pg_nrssi_lt) - 1);
	noise[3] = MIN(MAX(noise[3], 0), N(pg->pg_nrssi_lt) - 1);
	mac->mac_noise.noi_samples[i][0] = pg->pg_nrssi_lt[noise[0]];
	mac->mac_noise.noi_samples[i][1] = pg->pg_nrssi_lt[noise[1]];
	mac->mac_noise.noi_samples[i][2] = pg->pg_nrssi_lt[noise[2]];
	mac->mac_noise.noi_samples[i][3] = pg->pg_nrssi_lt[noise[3]];
	mac->mac_noise.noi_nsamples++;
	if (mac->mac_noise.noi_nsamples == 8) {
		average = 0;
		for (i = 0; i < 8; i++) {
			for (j = 0; j < 4; j++)
				average += mac->mac_noise.noi_samples[i][j];
		}
		average = (((average / 32) * 125) + 64) / 128;
		tmp = (bwn_shm_read_2(mac, BWN_SHARED, 0x40c) / 128) & 0x1f;
		if (tmp >= 8)
			average += 2;
		else
			average -= 25;
		average -= (tmp == 8) ? 72 : 48;

		mac->mac_stats.link_noise = average;
		mac->mac_noise.noi_running = 0;
		return;
	}
new:
	bwn_noise_gensample(mac);
}

static int
bwn_pio_rx(struct bwn_pio_rxqueue *prq)
{
	struct bwn_mac *mac = prq->prq_mac;
	struct bwn_softc *sc = mac->mac_sc;
	unsigned int i;

	BWN_ASSERT_LOCKED(sc);

	if (mac->mac_status < BWN_MAC_STATUS_STARTED)
		return (0);

	for (i = 0; i < 5000; i++) {
		if (bwn_pio_rxeof(prq) == 0)
			break;
	}
	if (i >= 5000)
		device_printf(sc->sc_dev, "too many RX frames in PIO mode\n");
	return ((i > 0) ? 1 : 0);
}

static void
bwn_dma_rx(struct bwn_dma_ring *dr)
{
	int slot, curslot;

	KASSERT(!dr->dr_tx, ("%s:%d: fail", __func__, __LINE__));
	curslot = dr->get_curslot(dr);
	KASSERT(curslot >= 0 && curslot < dr->dr_numslots,
	    ("%s:%d: fail", __func__, __LINE__));

	slot = dr->dr_curslot;
	for (; slot != curslot; slot = bwn_dma_nextslot(dr, slot))
		bwn_dma_rxeof(dr, &slot);

	bus_dmamap_sync(dr->dr_ring_dtag, dr->dr_ring_dmap,
	    BUS_DMASYNC_PREWRITE);

	dr->set_curslot(dr, slot);
	dr->dr_curslot = slot;
}

static void
bwn_intr_txeof(struct bwn_mac *mac)
{
	struct bwn_txstatus stat;
	uint32_t stat0, stat1;
	uint16_t tmp;

	BWN_ASSERT_LOCKED(mac->mac_sc);

	while (1) {
		stat0 = BWN_READ_4(mac, BWN_XMITSTAT_0);
		if (!(stat0 & 0x00000001))
			break;
		stat1 = BWN_READ_4(mac, BWN_XMITSTAT_1);

		stat.cookie = (stat0 >> 16);
		stat.seq = (stat1 & 0x0000ffff);
		stat.phy_stat = ((stat1 & 0x00ff0000) >> 16);
		tmp = (stat0 & 0x0000ffff);
		stat.framecnt = ((tmp & 0xf000) >> 12);
		stat.rtscnt = ((tmp & 0x0f00) >> 8);
		stat.sreason = ((tmp & 0x001c) >> 2);
		stat.pm = (tmp & 0x0080) ? 1 : 0;
		stat.im = (tmp & 0x0040) ? 1 : 0;
		stat.ampdu = (tmp & 0x0020) ? 1 : 0;
		stat.ack = (tmp & 0x0002) ? 1 : 0;

		bwn_handle_txeof(mac, &stat);
	}
}

static void
bwn_hwreset(void *arg, int npending)
{
	struct bwn_mac *mac = arg;
	struct bwn_softc *sc = mac->mac_sc;
	int error = 0;
	int prev_status;

	BWN_LOCK(sc);

	prev_status = mac->mac_status;
	if (prev_status >= BWN_MAC_STATUS_STARTED)
		bwn_core_stop(mac);
	if (prev_status >= BWN_MAC_STATUS_INITED)
		bwn_core_exit(mac);

	if (prev_status >= BWN_MAC_STATUS_INITED) {
		error = bwn_core_init(mac);
		if (error)
			goto out;
	}
	if (prev_status >= BWN_MAC_STATUS_STARTED)
		bwn_core_start(mac);
out:
	if (error) {
		device_printf(sc->sc_dev, "%s: failed (%d)\n", __func__, error);
		sc->sc_curmac = NULL;
	}
	BWN_UNLOCK(sc);
}

static void
bwn_handle_fwpanic(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t reason;

	reason = bwn_shm_read_2(mac, BWN_SCRATCH, BWN_FWPANIC_REASON_REG);
	device_printf(sc->sc_dev,"fw panic (%u)\n", reason);

	if (reason == BWN_FWPANIC_RESTART)
		bwn_restart(mac, "ucode panic");
}

static void
bwn_load_beacon0(struct bwn_mac *mac)
{

	KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
}

static void
bwn_load_beacon1(struct bwn_mac *mac)
{

	KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
}

static uint32_t
bwn_jssi_read(struct bwn_mac *mac)
{
	uint32_t val = 0;

	val = bwn_shm_read_2(mac, BWN_SHARED, 0x08a);
	val <<= 16;
	val |= bwn_shm_read_2(mac, BWN_SHARED, 0x088);

	return (val);
}

static void
bwn_noise_gensample(struct bwn_mac *mac)
{
	uint32_t jssi = 0x7f7f7f7f;

	bwn_shm_write_2(mac, BWN_SHARED, 0x088, (jssi & 0x0000ffff));
	bwn_shm_write_2(mac, BWN_SHARED, 0x08a, (jssi & 0xffff0000) >> 16);
	BWN_WRITE_4(mac, BWN_MACCMD,
	    BWN_READ_4(mac, BWN_MACCMD) | BWN_MACCMD_BGNOISE);
}

static int
bwn_dma_freeslot(struct bwn_dma_ring *dr)
{
	BWN_ASSERT_LOCKED(dr->dr_mac->mac_sc);

	return (dr->dr_numslots - dr->dr_usedslot);
}

static int
bwn_dma_nextslot(struct bwn_dma_ring *dr, int slot)
{
	BWN_ASSERT_LOCKED(dr->dr_mac->mac_sc);

	KASSERT(slot >= -1 && slot <= dr->dr_numslots - 1,
	    ("%s:%d: fail", __func__, __LINE__));
	if (slot == dr->dr_numslots - 1)
		return (0);
	return (slot + 1);
}

static void
bwn_dma_rxeof(struct bwn_dma_ring *dr, int *slot)
{
	struct bwn_mac *mac = dr->dr_mac;
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_dma *dma = &mac->mac_method.dma;
	struct bwn_dmadesc_generic *desc;
	struct bwn_dmadesc_meta *meta;
	struct bwn_rxhdr4 *rxhdr;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	uint32_t macstat;
	int32_t tmp;
	int cnt = 0;
	uint16_t len;

	dr->getdesc(dr, *slot, &desc, &meta);

	bus_dmamap_sync(dma->rxbuf_dtag, meta->mt_dmap, BUS_DMASYNC_POSTREAD);
	m = meta->mt_m;

	if (bwn_dma_newbuf(dr, desc, meta, 0)) {
		ifp->if_ierrors++;
		return;
	}

	rxhdr = mtod(m, struct bwn_rxhdr4 *);
	len = le16toh(rxhdr->frame_len);
	if (len <= 0) {
		ifp->if_ierrors++;
		return;
	}
	if (bwn_dma_check_redzone(dr, m)) {
		device_printf(sc->sc_dev, "redzone error.\n");
		bwn_dma_set_redzone(dr, m);
		bus_dmamap_sync(dma->rxbuf_dtag, meta->mt_dmap,
		    BUS_DMASYNC_PREWRITE);
		return;
	}
	if (len > dr->dr_rx_bufsize) {
		tmp = len;
		while (1) {
			dr->getdesc(dr, *slot, &desc, &meta);
			bwn_dma_set_redzone(dr, meta->mt_m);
			bus_dmamap_sync(dma->rxbuf_dtag, meta->mt_dmap,
			    BUS_DMASYNC_PREWRITE);
			*slot = bwn_dma_nextslot(dr, *slot);
			cnt++;
			tmp -= dr->dr_rx_bufsize;
			if (tmp <= 0)
				break;
		}
		device_printf(sc->sc_dev, "too small buffer "
		       "(len %u buffer %u dropped %d)\n",
		       len, dr->dr_rx_bufsize, cnt);
		return;
	}
	macstat = le32toh(rxhdr->mac_status);
	if (macstat & BWN_RX_MAC_FCSERR) {
		if (!(mac->mac_sc->sc_filters & BWN_MACCTL_PASS_BADFCS)) {
			device_printf(sc->sc_dev, "RX drop\n");
			return;
		}
	}

	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len = len + dr->dr_frameoffset;
	m_adj(m, dr->dr_frameoffset);

	bwn_rxeof(dr->dr_mac, m, rxhdr);
}

static void
bwn_handle_txeof(struct bwn_mac *mac, const struct bwn_txstatus *status)
{
	struct bwn_dma_ring *dr;
	struct bwn_dmadesc_generic *desc;
	struct bwn_dmadesc_meta *meta;
	struct bwn_pio_txqueue *tq;
	struct bwn_pio_txpkt *tp = NULL;
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_stats *stats = &mac->mac_stats;
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	int retrycnt = 0, slot;

	BWN_ASSERT_LOCKED(mac->mac_sc);

	if (status->im)
		device_printf(sc->sc_dev, "TODO: STATUS IM\n");
	if (status->ampdu)
		device_printf(sc->sc_dev, "TODO: STATUS AMPDU\n");
	if (status->rtscnt) {
		if (status->rtscnt == 0xf)
			stats->rtsfail++;
		else
			stats->rts++;
	}

	if (mac->mac_flags & BWN_MAC_FLAG_DMA) {
		if (status->ack) {
			dr = bwn_dma_parse_cookie(mac, status,
			    status->cookie, &slot);
			if (dr == NULL) {
				device_printf(sc->sc_dev,
				    "failed to parse cookie\n");
				return;
			}
			while (1) {
				dr->getdesc(dr, slot, &desc, &meta);
				if (meta->mt_islast) {
					ni = meta->mt_ni;
					vap = ni->ni_vap;
					ieee80211_ratectl_tx_complete(vap, ni,
					    status->ack ?
					      IEEE80211_RATECTL_TX_SUCCESS :
					      IEEE80211_RATECTL_TX_FAILURE,
					    &retrycnt, 0);
					break;
				}
				slot = bwn_dma_nextslot(dr, slot);
			}
		}
		bwn_dma_handle_txeof(mac, status);
	} else {
		if (status->ack) {
			tq = bwn_pio_parse_cookie(mac, status->cookie, &tp);
			if (tq == NULL) {
				device_printf(sc->sc_dev,
				    "failed to parse cookie\n");
				return;
			}
			ni = tp->tp_ni;
			vap = ni->ni_vap;
			ieee80211_ratectl_tx_complete(vap, ni,
			    status->ack ?
			      IEEE80211_RATECTL_TX_SUCCESS :
			      IEEE80211_RATECTL_TX_FAILURE,
			    &retrycnt, 0);
		}
		bwn_pio_handle_txeof(mac, status);
	}

	bwn_phy_txpower_check(mac, 0);
}

static uint8_t
bwn_pio_rxeof(struct bwn_pio_rxqueue *prq)
{
	struct bwn_mac *mac = prq->prq_mac;
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_rxhdr4 rxhdr;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	uint32_t ctl32, macstat, v32;
	unsigned int i, padding;
	uint16_t ctl16, len, totlen, v16;
	unsigned char *mp;
	char *data;

	memset(&rxhdr, 0, sizeof(rxhdr));

	if (prq->prq_rev >= 8) {
		ctl32 = bwn_pio_rx_read_4(prq, BWN_PIO8_RXCTL);
		if (!(ctl32 & BWN_PIO8_RXCTL_FRAMEREADY))
			return (0);
		bwn_pio_rx_write_4(prq, BWN_PIO8_RXCTL,
		    BWN_PIO8_RXCTL_FRAMEREADY);
		for (i = 0; i < 10; i++) {
			ctl32 = bwn_pio_rx_read_4(prq, BWN_PIO8_RXCTL);
			if (ctl32 & BWN_PIO8_RXCTL_DATAREADY)
				goto ready;
			DELAY(10);
		}
	} else {
		ctl16 = bwn_pio_rx_read_2(prq, BWN_PIO_RXCTL);
		if (!(ctl16 & BWN_PIO_RXCTL_FRAMEREADY))
			return (0);
		bwn_pio_rx_write_2(prq, BWN_PIO_RXCTL,
		    BWN_PIO_RXCTL_FRAMEREADY);
		for (i = 0; i < 10; i++) {
			ctl16 = bwn_pio_rx_read_2(prq, BWN_PIO_RXCTL);
			if (ctl16 & BWN_PIO_RXCTL_DATAREADY)
				goto ready;
			DELAY(10);
		}
	}
	device_printf(sc->sc_dev, "%s: timed out\n", __func__);
	return (1);
ready:
	if (prq->prq_rev >= 8)
		siba_read_multi_4(sc->sc_dev, &rxhdr, sizeof(rxhdr),
		    prq->prq_base + BWN_PIO8_RXDATA);
	else
		siba_read_multi_2(sc->sc_dev, &rxhdr, sizeof(rxhdr),
		    prq->prq_base + BWN_PIO_RXDATA);
	len = le16toh(rxhdr.frame_len);
	if (len > 0x700) {
		device_printf(sc->sc_dev, "%s: len is too big\n", __func__);
		goto error;
	}
	if (len == 0) {
		device_printf(sc->sc_dev, "%s: len is 0\n", __func__);
		goto error;
	}

	macstat = le32toh(rxhdr.mac_status);
	if (macstat & BWN_RX_MAC_FCSERR) {
		if (!(mac->mac_sc->sc_filters & BWN_MACCTL_PASS_BADFCS)) {
			device_printf(sc->sc_dev, "%s: FCS error", __func__);
			goto error;
		}
	}

	padding = (macstat & BWN_RX_MAC_PADDING) ? 2 : 0;
	totlen = len + padding;
	KASSERT(totlen <= MCLBYTES, ("too big..\n"));
	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		device_printf(sc->sc_dev, "%s: out of memory", __func__);
		goto error;
	}
	mp = mtod(m, unsigned char *);
	if (prq->prq_rev >= 8) {
		siba_read_multi_4(sc->sc_dev, mp, (totlen & ~3),
		    prq->prq_base + BWN_PIO8_RXDATA);
		if (totlen & 3) {
			v32 = bwn_pio_rx_read_4(prq, BWN_PIO8_RXDATA);
			data = &(mp[totlen - 1]);
			switch (totlen & 3) {
			case 3:
				*data = (v32 >> 16);
				data--;
			case 2:
				*data = (v32 >> 8);
				data--;
			case 1:
				*data = v32;
			}
		}
	} else {
		siba_read_multi_2(sc->sc_dev, mp, (totlen & ~1),
		    prq->prq_base + BWN_PIO_RXDATA);
		if (totlen & 1) {
			v16 = bwn_pio_rx_read_2(prq, BWN_PIO_RXDATA);
			mp[totlen - 1] = v16;
		}
	}

	m->m_pkthdr.rcvif = ifp;
	m->m_len = m->m_pkthdr.len = totlen;

	bwn_rxeof(prq->prq_mac, m, &rxhdr);

	return (1);
error:
	if (prq->prq_rev >= 8)
		bwn_pio_rx_write_4(prq, BWN_PIO8_RXCTL,
		    BWN_PIO8_RXCTL_DATAREADY);
	else
		bwn_pio_rx_write_2(prq, BWN_PIO_RXCTL, BWN_PIO_RXCTL_DATAREADY);
	return (1);
}

static int
bwn_dma_newbuf(struct bwn_dma_ring *dr, struct bwn_dmadesc_generic *desc,
    struct bwn_dmadesc_meta *meta, int init)
{
	struct bwn_mac *mac = dr->dr_mac;
	struct bwn_dma *dma = &mac->mac_method.dma;
	struct bwn_rxhdr4 *hdr;
	bus_dmamap_t map;
	bus_addr_t paddr;
	struct mbuf *m;
	int error;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		error = ENOBUFS;

		/*
		 * If the NIC is up and running, we need to:
		 * - Clear RX buffer's header.
		 * - Restore RX descriptor settings.
		 */
		if (init)
			return (error);
		else
			goto back;
	}
	m->m_len = m->m_pkthdr.len = MCLBYTES;

	bwn_dma_set_redzone(dr, m);

	/*
	 * Try to load RX buf into temporary DMA map
	 */
	error = bus_dmamap_load_mbuf(dma->rxbuf_dtag, dr->dr_spare_dmap, m,
	    bwn_dma_buf_addr, &paddr, BUS_DMA_NOWAIT);
	if (error) {
		m_freem(m);

		/*
		 * See the comment above
		 */
		if (init)
			return (error);
		else
			goto back;
	}

	if (!init)
		bus_dmamap_unload(dma->rxbuf_dtag, meta->mt_dmap);
	meta->mt_m = m;
	meta->mt_paddr = paddr;

	/*
	 * Swap RX buf's DMA map with the loaded temporary one
	 */
	map = meta->mt_dmap;
	meta->mt_dmap = dr->dr_spare_dmap;
	dr->dr_spare_dmap = map;

back:
	/*
	 * Clear RX buf header
	 */
	hdr = mtod(meta->mt_m, struct bwn_rxhdr4 *);
	bzero(hdr, sizeof(*hdr));
	bus_dmamap_sync(dma->rxbuf_dtag, meta->mt_dmap,
	    BUS_DMASYNC_PREWRITE);

	/*
	 * Setup RX buf descriptor
	 */
	dr->setdesc(dr, desc, paddr, meta->mt_m->m_len -
	    sizeof(*hdr), 0, 0, 0);
	return (error);
}

static void
bwn_dma_buf_addr(void *arg, bus_dma_segment_t *seg, int nseg,
		 bus_size_t mapsz __unused, int error)
{

	if (!error) {
		KASSERT(nseg == 1, ("too many segments(%d)\n", nseg));
		*((bus_addr_t *)arg) = seg->ds_addr;
	}
}

static int
bwn_hwrate2ieeerate(int rate)
{

	switch (rate) {
	case BWN_CCK_RATE_1MB:
		return (2);
	case BWN_CCK_RATE_2MB:
		return (4);
	case BWN_CCK_RATE_5MB:
		return (11);
	case BWN_CCK_RATE_11MB:
		return (22);
	case BWN_OFDM_RATE_6MB:
		return (12);
	case BWN_OFDM_RATE_9MB:
		return (18);
	case BWN_OFDM_RATE_12MB:
		return (24);
	case BWN_OFDM_RATE_18MB:
		return (36);
	case BWN_OFDM_RATE_24MB:
		return (48);
	case BWN_OFDM_RATE_36MB:
		return (72);
	case BWN_OFDM_RATE_48MB:
		return (96);
	case BWN_OFDM_RATE_54MB:
		return (108);
	default:
		printf("Ooops\n");
		return (0);
	}
}

static void
bwn_rxeof(struct bwn_mac *mac, struct mbuf *m, const void *_rxhdr)
{
	const struct bwn_rxhdr4 *rxhdr = _rxhdr;
	struct bwn_plcp6 *plcp;
	struct bwn_softc *sc = mac->mac_sc;
	struct ieee80211_frame_min *wh;
	struct ieee80211_node *ni;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint32_t macstat;
	int padding, rate, rssi = 0, noise = 0, type;
	uint16_t phytype, phystat0, phystat3, chanstat;
	unsigned char *mp = mtod(m, unsigned char *);
	static int rx_mac_dec_rpt = 0;

	BWN_ASSERT_LOCKED(sc);

	phystat0 = le16toh(rxhdr->phy_status0);
	phystat3 = le16toh(rxhdr->phy_status3);
	macstat = le32toh(rxhdr->mac_status);
	chanstat = le16toh(rxhdr->channel);
	phytype = chanstat & BWN_RX_CHAN_PHYTYPE;

	if (macstat & BWN_RX_MAC_FCSERR)
		device_printf(sc->sc_dev, "TODO RX: RX_FLAG_FAILED_FCS_CRC\n");
	if (phystat0 & (BWN_RX_PHYST0_PLCPHCF | BWN_RX_PHYST0_PLCPFV))
		device_printf(sc->sc_dev, "TODO RX: RX_FLAG_FAILED_PLCP_CRC\n");
	if (macstat & BWN_RX_MAC_DECERR)
		goto drop;

	padding = (macstat & BWN_RX_MAC_PADDING) ? 2 : 0;
	if (m->m_pkthdr.len < (sizeof(struct bwn_plcp6) + padding)) {
		device_printf(sc->sc_dev, "frame too short (length=%d)\n",
		    m->m_pkthdr.len);
		goto drop;
	}
	plcp = (struct bwn_plcp6 *)(mp + padding);
	m_adj(m, sizeof(struct bwn_plcp6) + padding);
	if (m->m_pkthdr.len < IEEE80211_MIN_LEN) {
		device_printf(sc->sc_dev, "frame too short (length=%d)\n",
		    m->m_pkthdr.len);
		goto drop;
	}
	wh = mtod(m, struct ieee80211_frame_min *);

	if (macstat & BWN_RX_MAC_DEC && rx_mac_dec_rpt++ < 50)
		device_printf(sc->sc_dev,
		    "RX decryption attempted (old %d keyidx %#x)\n",
		    BWN_ISOLDFMT(mac),
		    (macstat & BWN_RX_MAC_KEYIDX) >> BWN_RX_MAC_KEYIDX_SHIFT);

	/* XXX calculating RSSI & noise & antenna */

	if (phystat0 & BWN_RX_PHYST0_OFDM)
		rate = bwn_plcp_get_ofdmrate(mac, plcp,
		    phytype == BWN_PHYTYPE_A);
	else
		rate = bwn_plcp_get_cckrate(mac, plcp);
	if (rate == -1) {
		if (!(mac->mac_sc->sc_filters & BWN_MACCTL_PASS_BADPLCP))
			goto drop;
	}
	sc->sc_rx_rate = bwn_hwrate2ieeerate(rate);

	/* RX radio tap */
	if (ieee80211_radiotap_active(ic))
		bwn_rx_radiotap(mac, m, rxhdr, plcp, rate, rssi, noise);
	m_adj(m, -IEEE80211_CRC_LEN);

	rssi = rxhdr->phy.abg.rssi;	/* XXX incorrect RSSI calculation? */
	noise = mac->mac_stats.link_noise;

	ifp->if_ipackets++;

	BWN_UNLOCK(sc);

	ni = ieee80211_find_rxnode(ic, wh);
	if (ni != NULL) {
		type = ieee80211_input(ni, m, rssi, noise);
		ieee80211_free_node(ni);
	} else
		type = ieee80211_input_all(ic, m, rssi, noise);

	BWN_LOCK(sc);
	return;
drop:
	device_printf(sc->sc_dev, "%s: dropped\n", __func__);
}

static void
bwn_dma_handle_txeof(struct bwn_mac *mac,
    const struct bwn_txstatus *status)
{
	struct bwn_dma *dma = &mac->mac_method.dma;
	struct bwn_dma_ring *dr;
	struct bwn_dmadesc_generic *desc;
	struct bwn_dmadesc_meta *meta;
	struct bwn_softc *sc = mac->mac_sc;
	struct ieee80211_node *ni;
	struct ifnet *ifp = sc->sc_ifp;
	struct mbuf *m;
	int slot;

	BWN_ASSERT_LOCKED(sc);

	dr = bwn_dma_parse_cookie(mac, status, status->cookie, &slot);
	if (dr == NULL) {
		device_printf(sc->sc_dev, "failed to parse cookie\n");
		return;
	}
	KASSERT(dr->dr_tx, ("%s:%d: fail", __func__, __LINE__));

	while (1) {
		KASSERT(slot >= 0 && slot < dr->dr_numslots,
		    ("%s:%d: fail", __func__, __LINE__));
		dr->getdesc(dr, slot, &desc, &meta);

		if (meta->mt_txtype == BWN_DMADESC_METATYPE_HEADER)
			bus_dmamap_unload(dr->dr_txring_dtag, meta->mt_dmap);
		else if (meta->mt_txtype == BWN_DMADESC_METATYPE_BODY)
			bus_dmamap_unload(dma->txbuf_dtag, meta->mt_dmap);

		if (meta->mt_islast) {
			KASSERT(meta->mt_m != NULL,
			    ("%s:%d: fail", __func__, __LINE__));

			ni = meta->mt_ni;
			m = meta->mt_m;
			if (ni != NULL) {
				/*
				 * Do any tx complete callback. Note this must
				 * be done before releasing the node reference.
				 */
				if (m->m_flags & M_TXCB)
					ieee80211_process_callback(ni, m, 0);
				ieee80211_free_node(ni);
				meta->mt_ni = NULL;
			}
			m_freem(m);
			meta->mt_m = NULL;
		} else {
			KASSERT(meta->mt_m == NULL,
			    ("%s:%d: fail", __func__, __LINE__));
		}

		dr->dr_usedslot--;
		if (meta->mt_islast) {
			ifp->if_opackets++;
			break;
		}
		slot = bwn_dma_nextslot(dr, slot);
	}
	sc->sc_watchdog_timer = 0;
	if (dr->dr_stop) {
		KASSERT(bwn_dma_freeslot(dr) >= BWN_TX_SLOTS_PER_FRAME,
		    ("%s:%d: fail", __func__, __LINE__));
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		dr->dr_stop = 0;
	}
}

static void
bwn_pio_handle_txeof(struct bwn_mac *mac,
    const struct bwn_txstatus *status)
{
	struct bwn_pio_txqueue *tq;
	struct bwn_pio_txpkt *tp = NULL;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;

	BWN_ASSERT_LOCKED(sc);

	tq = bwn_pio_parse_cookie(mac, status->cookie, &tp);
	if (tq == NULL)
		return;

	tq->tq_used -= roundup(tp->tp_m->m_pkthdr.len + BWN_HDRSIZE(mac), 4);
	tq->tq_free++;

	if (tp->tp_ni != NULL) {
		/*
		 * Do any tx complete callback.  Note this must
		 * be done before releasing the node reference.
		 */
		if (tp->tp_m->m_flags & M_TXCB)
			ieee80211_process_callback(tp->tp_ni, tp->tp_m, 0);
		ieee80211_free_node(tp->tp_ni);
		tp->tp_ni = NULL;
	}
	m_freem(tp->tp_m);
	tp->tp_m = NULL;
	TAILQ_INSERT_TAIL(&tq->tq_pktlist, tp, tp_list);

	ifp->if_opackets++;

	sc->sc_watchdog_timer = 0;
	if (tq->tq_stop) {
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		tq->tq_stop = 0;
	}
}

static void
bwn_phy_txpower_check(struct bwn_mac *mac, uint32_t flags)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_phy *phy = &mac->mac_phy;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	unsigned long now;
	int result;

	BWN_GETTIME(now);

	if (!(flags & BWN_TXPWR_IGNORE_TIME) && time_before(now, phy->nexttime))
		return;
	phy->nexttime = now + 2 * 1000;

	if (siba_get_pci_subvendor(sc->sc_dev) == SIBA_BOARDVENDOR_BCM &&
	    siba_get_pci_subdevice(sc->sc_dev) == SIBA_BOARD_BU4306)
		return;

	if (phy->recalc_txpwr != NULL) {
		result = phy->recalc_txpwr(mac,
		    (flags & BWN_TXPWR_IGNORE_TSSI) ? 1 : 0);
		if (result == BWN_TXPWR_RES_DONE)
			return;
		KASSERT(result == BWN_TXPWR_RES_NEED_ADJUST,
		    ("%s: fail", __func__));
		KASSERT(phy->set_txpwr != NULL, ("%s: fail", __func__));

		ieee80211_runtask(ic, &mac->mac_txpower);
	}
}

static uint16_t
bwn_pio_rx_read_2(struct bwn_pio_rxqueue *prq, uint16_t offset)
{

	return (BWN_READ_2(prq->prq_mac, prq->prq_base + offset));
}

static uint32_t
bwn_pio_rx_read_4(struct bwn_pio_rxqueue *prq, uint16_t offset)
{

	return (BWN_READ_4(prq->prq_mac, prq->prq_base + offset));
}

static void
bwn_pio_rx_write_2(struct bwn_pio_rxqueue *prq, uint16_t offset, uint16_t value)
{

	BWN_WRITE_2(prq->prq_mac, prq->prq_base + offset, value);
}

static void
bwn_pio_rx_write_4(struct bwn_pio_rxqueue *prq, uint16_t offset, uint32_t value)
{

	BWN_WRITE_4(prq->prq_mac, prq->prq_base + offset, value);
}

static int
bwn_ieeerate2hwrate(struct bwn_softc *sc, int rate)
{

	switch (rate) {
	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	case 12:
		return (BWN_OFDM_RATE_6MB);
	case 18:
		return (BWN_OFDM_RATE_9MB);
	case 24:
		return (BWN_OFDM_RATE_12MB);
	case 36:
		return (BWN_OFDM_RATE_18MB);
	case 48:
		return (BWN_OFDM_RATE_24MB);
	case 72:
		return (BWN_OFDM_RATE_36MB);
	case 96:
		return (BWN_OFDM_RATE_48MB);
	case 108:
		return (BWN_OFDM_RATE_54MB);
	/* CCK rates (NB: not IEEE std, device-specific) */
	case 2:
		return (BWN_CCK_RATE_1MB);
	case 4:
		return (BWN_CCK_RATE_2MB);
	case 11:
		return (BWN_CCK_RATE_5MB);
	case 22:
		return (BWN_CCK_RATE_11MB);
	}

	device_printf(sc->sc_dev, "unsupported rate %d\n", rate);
	return (BWN_CCK_RATE_1MB);
}

static int
bwn_set_txhdr(struct bwn_mac *mac, struct ieee80211_node *ni,
    struct mbuf *m, struct bwn_txhdr *txhdr, uint16_t cookie)
{
	const struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_softc *sc = mac->mac_sc;
	struct ieee80211_frame *wh;
	struct ieee80211_frame *protwh;
	struct ieee80211_frame_cts *cts;
	struct ieee80211_frame_rts *rts;
	const struct ieee80211_txparam *tp;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	struct mbuf *mprot;
	unsigned int len;
	uint32_t macctl = 0;
	int protdur, rts_rate, rts_rate_fb, ismcast, isshort, rix, type;
	uint16_t phyctl = 0;
	uint8_t rate, rate_fb;

	wh = mtod(m, struct ieee80211_frame *);
	memset(txhdr, 0, sizeof(*txhdr));

	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	isshort = (ic->ic_flags & IEEE80211_F_SHPREAMBLE) != 0;

	/*
	 * Find TX rate
	 */
	tp = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];
	if (type != IEEE80211_FC0_TYPE_DATA || (m->m_flags & M_EAPOL))
		rate = rate_fb = tp->mgmtrate;
	else if (ismcast)
		rate = rate_fb = tp->mcastrate;
	else if (tp->ucastrate != IEEE80211_FIXED_RATE_NONE)
		rate = rate_fb = tp->ucastrate;
	else {
		rix = ieee80211_ratectl_rate(ni, NULL, 0);
		rate = ni->ni_txrate;

		if (rix > 0)
			rate_fb = ni->ni_rates.rs_rates[rix - 1] &
			    IEEE80211_RATE_VAL;
		else
			rate_fb = rate;
	}

	sc->sc_tx_rate = rate;

	rate = bwn_ieeerate2hwrate(sc, rate);
	rate_fb = bwn_ieeerate2hwrate(sc, rate_fb);

	txhdr->phyrate = (BWN_ISOFDMRATE(rate)) ? bwn_plcp_getofdm(rate) :
	    bwn_plcp_getcck(rate);
	bcopy(wh->i_fc, txhdr->macfc, sizeof(txhdr->macfc));
	bcopy(wh->i_addr1, txhdr->addr1, IEEE80211_ADDR_LEN);

	if ((rate_fb == rate) ||
	    (*(u_int16_t *)wh->i_dur & htole16(0x8000)) ||
	    (*(u_int16_t *)wh->i_dur == htole16(0)))
		txhdr->dur_fb = *(u_int16_t *)wh->i_dur;
	else
		txhdr->dur_fb = ieee80211_compute_duration(ic->ic_rt,
		    m->m_pkthdr.len, rate, isshort);

	/* XXX TX encryption */
	bwn_plcp_genhdr(BWN_ISOLDFMT(mac) ?
	    (struct bwn_plcp4 *)(&txhdr->body.old.plcp) :
	    (struct bwn_plcp4 *)(&txhdr->body.new.plcp),
	    m->m_pkthdr.len + IEEE80211_CRC_LEN, rate);
	bwn_plcp_genhdr((struct bwn_plcp4 *)(&txhdr->plcp_fb),
	    m->m_pkthdr.len + IEEE80211_CRC_LEN, rate_fb);

	txhdr->eftypes |= (BWN_ISOFDMRATE(rate_fb)) ? BWN_TX_EFT_FB_OFDM :
	    BWN_TX_EFT_FB_CCK;
	txhdr->chan = phy->chan;
	phyctl |= (BWN_ISOFDMRATE(rate)) ? BWN_TX_PHY_ENC_OFDM :
	    BWN_TX_PHY_ENC_CCK;
	if (isshort && (rate == BWN_CCK_RATE_2MB || rate == BWN_CCK_RATE_5MB ||
	     rate == BWN_CCK_RATE_11MB))
		phyctl |= BWN_TX_PHY_SHORTPRMBL;

	/* XXX TX antenna selection */

	switch (bwn_antenna_sanitize(mac, 0)) {
	case 0:
		phyctl |= BWN_TX_PHY_ANT01AUTO;
		break;
	case 1:
		phyctl |= BWN_TX_PHY_ANT0;
		break;
	case 2:
		phyctl |= BWN_TX_PHY_ANT1;
		break;
	case 3:
		phyctl |= BWN_TX_PHY_ANT2;
		break;
	case 4:
		phyctl |= BWN_TX_PHY_ANT3;
		break;
	default:
		KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	}

	if (!ismcast)
		macctl |= BWN_TX_MAC_ACK;

	macctl |= (BWN_TX_MAC_HWSEQ | BWN_TX_MAC_START_MSDU);
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
	    m->m_pkthdr.len + IEEE80211_CRC_LEN > vap->iv_rtsthreshold)
		macctl |= BWN_TX_MAC_LONGFRAME;

	if (ic->ic_flags & IEEE80211_F_USEPROT) {
		/* XXX RTS rate is always 1MB??? */
		rts_rate = BWN_CCK_RATE_1MB;
		rts_rate_fb = bwn_get_fbrate(rts_rate);

		protdur = ieee80211_compute_duration(ic->ic_rt,
		    m->m_pkthdr.len, rate, isshort) +
		    + ieee80211_ack_duration(ic->ic_rt, rate, isshort);

		if (ic->ic_protmode == IEEE80211_PROT_CTSONLY) {
			cts = (struct ieee80211_frame_cts *)(BWN_ISOLDFMT(mac) ?
			    (txhdr->body.old.rts_frame) :
			    (txhdr->body.new.rts_frame));
			mprot = ieee80211_alloc_cts(ic, ni->ni_vap->iv_myaddr,
			    protdur);
			KASSERT(mprot != NULL, ("failed to alloc mbuf\n"));
			bcopy(mtod(mprot, uint8_t *), (uint8_t *)cts,
			    mprot->m_pkthdr.len);
			m_freem(mprot);
			macctl |= BWN_TX_MAC_SEND_CTSTOSELF;
			len = sizeof(struct ieee80211_frame_cts);
		} else {
			rts = (struct ieee80211_frame_rts *)(BWN_ISOLDFMT(mac) ?
			    (txhdr->body.old.rts_frame) :
			    (txhdr->body.new.rts_frame));
			protdur += ieee80211_ack_duration(ic->ic_rt, rate,
			    isshort);
			mprot = ieee80211_alloc_rts(ic, wh->i_addr1,
			    wh->i_addr2, protdur);
			KASSERT(mprot != NULL, ("failed to alloc mbuf\n"));
			bcopy(mtod(mprot, uint8_t *), (uint8_t *)rts,
			    mprot->m_pkthdr.len);
			m_freem(mprot);
			macctl |= BWN_TX_MAC_SEND_RTSCTS;
			len = sizeof(struct ieee80211_frame_rts);
		}
		len += IEEE80211_CRC_LEN;
		bwn_plcp_genhdr((struct bwn_plcp4 *)((BWN_ISOLDFMT(mac)) ?
		    &txhdr->body.old.rts_plcp :
		    &txhdr->body.new.rts_plcp), len, rts_rate);
		bwn_plcp_genhdr((struct bwn_plcp4 *)&txhdr->rts_plcp_fb, len,
		    rts_rate_fb);

		protwh = (struct ieee80211_frame *)(BWN_ISOLDFMT(mac) ?
		    (&txhdr->body.old.rts_frame) :
		    (&txhdr->body.new.rts_frame));
		txhdr->rts_dur_fb = *(u_int16_t *)protwh->i_dur;

		if (BWN_ISOFDMRATE(rts_rate)) {
			txhdr->eftypes |= BWN_TX_EFT_RTS_OFDM;
			txhdr->phyrate_rts = bwn_plcp_getofdm(rts_rate);
		} else {
			txhdr->eftypes |= BWN_TX_EFT_RTS_CCK;
			txhdr->phyrate_rts = bwn_plcp_getcck(rts_rate);
		}
		txhdr->eftypes |= (BWN_ISOFDMRATE(rts_rate_fb)) ?
		    BWN_TX_EFT_RTS_FBOFDM : BWN_TX_EFT_RTS_FBCCK;
	}

	if (BWN_ISOLDFMT(mac))
		txhdr->body.old.cookie = htole16(cookie);
	else
		txhdr->body.new.cookie = htole16(cookie);

	txhdr->macctl = htole32(macctl);
	txhdr->phyctl = htole16(phyctl);

	/*
	 * TX radio tap
	 */
	if (ieee80211_radiotap_active_vap(vap)) {
		sc->sc_tx_th.wt_flags = 0;
		if (wh->i_fc[1] & IEEE80211_FC1_WEP)
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		if (isshort &&
		    (rate == BWN_CCK_RATE_2MB || rate == BWN_CCK_RATE_5MB ||
		     rate == BWN_CCK_RATE_11MB))
			sc->sc_tx_th.wt_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		sc->sc_tx_th.wt_rate = rate;

		ieee80211_radiotap_tx(vap, m);
	}

	return (0);
}

static void
bwn_plcp_genhdr(struct bwn_plcp4 *plcp, const uint16_t octets,
    const uint8_t rate)
{
	uint32_t d, plen;
	uint8_t *raw = plcp->o.raw;

	if (BWN_ISOFDMRATE(rate)) {
		d = bwn_plcp_getofdm(rate);
		KASSERT(!(octets & 0xf000),
		    ("%s:%d: fail", __func__, __LINE__));
		d |= (octets << 5);
		plcp->o.data = htole32(d);
	} else {
		plen = octets * 16 / rate;
		if ((octets * 16 % rate) > 0) {
			plen++;
			if ((rate == BWN_CCK_RATE_11MB)
			    && ((octets * 8 % 11) < 4)) {
				raw[1] = 0x84;
			} else
				raw[1] = 0x04;
		} else
			raw[1] = 0x04;
		plcp->o.data |= htole32(plen << 16);
		raw[0] = bwn_plcp_getcck(rate);
	}
}

static uint8_t
bwn_antenna_sanitize(struct bwn_mac *mac, uint8_t n)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint8_t mask;

	if (n == 0)
		return (0);
	if (mac->mac_phy.gmode)
		mask = siba_sprom_get_ant_bg(sc->sc_dev);
	else
		mask = siba_sprom_get_ant_a(sc->sc_dev);
	if (!(mask & (1 << (n - 1))))
		return (0);
	return (n);
}

static uint8_t
bwn_get_fbrate(uint8_t bitrate)
{
	switch (bitrate) {
	case BWN_CCK_RATE_1MB:
		return (BWN_CCK_RATE_1MB);
	case BWN_CCK_RATE_2MB:
		return (BWN_CCK_RATE_1MB);
	case BWN_CCK_RATE_5MB:
		return (BWN_CCK_RATE_2MB);
	case BWN_CCK_RATE_11MB:
		return (BWN_CCK_RATE_5MB);
	case BWN_OFDM_RATE_6MB:
		return (BWN_CCK_RATE_5MB);
	case BWN_OFDM_RATE_9MB:
		return (BWN_OFDM_RATE_6MB);
	case BWN_OFDM_RATE_12MB:
		return (BWN_OFDM_RATE_9MB);
	case BWN_OFDM_RATE_18MB:
		return (BWN_OFDM_RATE_12MB);
	case BWN_OFDM_RATE_24MB:
		return (BWN_OFDM_RATE_18MB);
	case BWN_OFDM_RATE_36MB:
		return (BWN_OFDM_RATE_24MB);
	case BWN_OFDM_RATE_48MB:
		return (BWN_OFDM_RATE_36MB);
	case BWN_OFDM_RATE_54MB:
		return (BWN_OFDM_RATE_48MB);
	}
	KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	return (0);
}

static uint32_t
bwn_pio_write_multi_4(struct bwn_mac *mac, struct bwn_pio_txqueue *tq,
    uint32_t ctl, const void *_data, int len)
{
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t value = 0;
	const uint8_t *data = _data;

	ctl |= BWN_PIO8_TXCTL_0_7 | BWN_PIO8_TXCTL_8_15 |
	    BWN_PIO8_TXCTL_16_23 | BWN_PIO8_TXCTL_24_31;
	bwn_pio_write_4(mac, tq, BWN_PIO8_TXCTL, ctl);

	siba_write_multi_4(sc->sc_dev, data, (len & ~3),
	    tq->tq_base + BWN_PIO8_TXDATA);
	if (len & 3) {
		ctl &= ~(BWN_PIO8_TXCTL_8_15 | BWN_PIO8_TXCTL_16_23 |
		    BWN_PIO8_TXCTL_24_31);
		data = &(data[len - 1]);
		switch (len & 3) {
		case 3:
			ctl |= BWN_PIO8_TXCTL_16_23;
			value |= (uint32_t)(*data) << 16;
			data--;
		case 2:
			ctl |= BWN_PIO8_TXCTL_8_15;
			value |= (uint32_t)(*data) << 8;
			data--;
		case 1:
			value |= (uint32_t)(*data);
		}
		bwn_pio_write_4(mac, tq, BWN_PIO8_TXCTL, ctl);
		bwn_pio_write_4(mac, tq, BWN_PIO8_TXDATA, value);
	}

	return (ctl);
}

static void
bwn_pio_write_4(struct bwn_mac *mac, struct bwn_pio_txqueue *tq,
    uint16_t offset, uint32_t value)
{

	BWN_WRITE_4(mac, tq->tq_base + offset, value);
}

static uint16_t
bwn_pio_write_multi_2(struct bwn_mac *mac, struct bwn_pio_txqueue *tq,
    uint16_t ctl, const void *_data, int len)
{
	struct bwn_softc *sc = mac->mac_sc;
	const uint8_t *data = _data;

	ctl |= BWN_PIO_TXCTL_WRITELO | BWN_PIO_TXCTL_WRITEHI;
	BWN_PIO_WRITE_2(mac, tq, BWN_PIO_TXCTL, ctl);

	siba_write_multi_2(sc->sc_dev, data, (len & ~1),
	    tq->tq_base + BWN_PIO_TXDATA);
	if (len & 1) {
		ctl &= ~BWN_PIO_TXCTL_WRITEHI;
		BWN_PIO_WRITE_2(mac, tq, BWN_PIO_TXCTL, ctl);
		BWN_PIO_WRITE_2(mac, tq, BWN_PIO_TXDATA, data[len - 1]);
	}

	return (ctl);
}

static uint16_t
bwn_pio_write_mbuf_2(struct bwn_mac *mac, struct bwn_pio_txqueue *tq,
    uint16_t ctl, struct mbuf *m0)
{
	int i, j = 0;
	uint16_t data = 0;
	const uint8_t *buf;
	struct mbuf *m = m0;

	ctl |= BWN_PIO_TXCTL_WRITELO | BWN_PIO_TXCTL_WRITEHI;
	BWN_PIO_WRITE_2(mac, tq, BWN_PIO_TXCTL, ctl);

	for (; m != NULL; m = m->m_next) {
		buf = mtod(m, const uint8_t *);
		for (i = 0; i < m->m_len; i++) {
			if (!((j++) % 2))
				data |= buf[i];
			else {
				data |= (buf[i] << 8);
				BWN_PIO_WRITE_2(mac, tq, BWN_PIO_TXDATA, data);
				data = 0;
			}
		}
	}
	if (m0->m_pkthdr.len % 2) {
		ctl &= ~BWN_PIO_TXCTL_WRITEHI;
		BWN_PIO_WRITE_2(mac, tq, BWN_PIO_TXCTL, ctl);
		BWN_PIO_WRITE_2(mac, tq, BWN_PIO_TXDATA, data);
	}

	return (ctl);
}

static void
bwn_set_slot_time(struct bwn_mac *mac, uint16_t time)
{

	if (mac->mac_phy.type != BWN_PHYTYPE_G)
		return;
	BWN_WRITE_2(mac, 0x684, 510 + time);
	bwn_shm_write_2(mac, BWN_SHARED, 0x0010, time);
}

static struct bwn_dma_ring *
bwn_dma_select(struct bwn_mac *mac, uint8_t prio)
{

	if ((mac->mac_flags & BWN_MAC_FLAG_WME) == 0)
		return (mac->mac_method.dma.wme[WME_AC_BE]);

	switch (prio) {
	case 3:
		return (mac->mac_method.dma.wme[WME_AC_VO]);
	case 2:
		return (mac->mac_method.dma.wme[WME_AC_VI]);
	case 0:
		return (mac->mac_method.dma.wme[WME_AC_BE]);
	case 1:
		return (mac->mac_method.dma.wme[WME_AC_BK]);
	}
	KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	return (NULL);
}

static int
bwn_dma_getslot(struct bwn_dma_ring *dr)
{
	int slot;

	BWN_ASSERT_LOCKED(dr->dr_mac->mac_sc);

	KASSERT(dr->dr_tx, ("%s:%d: fail", __func__, __LINE__));
	KASSERT(!(dr->dr_stop), ("%s:%d: fail", __func__, __LINE__));
	KASSERT(bwn_dma_freeslot(dr) != 0, ("%s:%d: fail", __func__, __LINE__));

	slot = bwn_dma_nextslot(dr, dr->dr_curslot);
	KASSERT(!(slot & ~0x0fff), ("%s:%d: fail", __func__, __LINE__));
	dr->dr_curslot = slot;
	dr->dr_usedslot++;

	return (slot);
}

static int
bwn_phy_shm_tssi_read(struct bwn_mac *mac, uint16_t shm_offset)
{
	const uint8_t ofdm = (shm_offset != BWN_SHARED_TSSI_CCK);
	unsigned int a, b, c, d;
	unsigned int avg;
	uint32_t tmp;

	tmp = bwn_shm_read_4(mac, BWN_SHARED, shm_offset);
	a = tmp & 0xff;
	b = (tmp >> 8) & 0xff;
	c = (tmp >> 16) & 0xff;
	d = (tmp >> 24) & 0xff;
	if (a == 0 || a == BWN_TSSI_MAX || b == 0 || b == BWN_TSSI_MAX ||
	    c == 0 || c == BWN_TSSI_MAX || d == 0 || d == BWN_TSSI_MAX)
		return (ENOENT);
	bwn_shm_write_4(mac, BWN_SHARED, shm_offset,
	    BWN_TSSI_MAX | (BWN_TSSI_MAX << 8) |
	    (BWN_TSSI_MAX << 16) | (BWN_TSSI_MAX << 24));

	if (ofdm) {
		a = (a + 32) & 0x3f;
		b = (b + 32) & 0x3f;
		c = (c + 32) & 0x3f;
		d = (d + 32) & 0x3f;
	}

	avg = (a + b + c + d + 2) / 4;
	if (ofdm) {
		if (bwn_shm_read_2(mac, BWN_SHARED, BWN_SHARED_HFLO)
		    & BWN_HF_4DB_CCK_POWERBOOST)
			avg = (avg >= 13) ? (avg - 13) : 0;
	}
	return (avg);
}

static void
bwn_phy_g_setatt(struct bwn_mac *mac, int *bbattp, int *rfattp)
{
	struct bwn_txpwr_loctl *lo = &mac->mac_phy.phy_g.pg_loctl;
	int rfatt = *rfattp;
	int bbatt = *bbattp;

	while (1) {
		if (rfatt > lo->rfatt.max && bbatt > lo->bbatt.max - 4)
			break;
		if (rfatt < lo->rfatt.min && bbatt < lo->bbatt.min + 4)
			break;
		if (bbatt > lo->bbatt.max && rfatt > lo->rfatt.max - 1)
			break;
		if (bbatt < lo->bbatt.min && rfatt < lo->rfatt.min + 1)
			break;
		if (bbatt > lo->bbatt.max) {
			bbatt -= 4;
			rfatt += 1;
			continue;
		}
		if (bbatt < lo->bbatt.min) {
			bbatt += 4;
			rfatt -= 1;
			continue;
		}
		if (rfatt > lo->rfatt.max) {
			rfatt -= 1;
			bbatt += 4;
			continue;
		}
		if (rfatt < lo->rfatt.min) {
			rfatt += 1;
			bbatt -= 4;
			continue;
		}
		break;
	}

	*rfattp = MIN(MAX(rfatt, lo->rfatt.min), lo->rfatt.max);
	*bbattp = MIN(MAX(bbatt, lo->bbatt.min), lo->bbatt.max);
}

static void
bwn_phy_lock(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;

	KASSERT(siba_get_revid(sc->sc_dev) >= 3,
	    ("%s: unsupported rev %d", __func__, siba_get_revid(sc->sc_dev)));

	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
		bwn_psctl(mac, BWN_PS_AWAKE);
}

static void
bwn_phy_unlock(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ieee80211com *ic = sc->sc_ifp->if_l2com;

	KASSERT(siba_get_revid(sc->sc_dev) >= 3,
	    ("%s: unsupported rev %d", __func__, siba_get_revid(sc->sc_dev)));

	if (ic->ic_opmode != IEEE80211_M_HOSTAP)
		bwn_psctl(mac, 0);
}

static void
bwn_rf_lock(struct bwn_mac *mac)
{

	BWN_WRITE_4(mac, BWN_MACCTL,
	    BWN_READ_4(mac, BWN_MACCTL) | BWN_MACCTL_RADIO_LOCK);
	BWN_READ_4(mac, BWN_MACCTL);
	DELAY(10);
}

static void
bwn_rf_unlock(struct bwn_mac *mac)
{

	BWN_READ_2(mac, BWN_PHYVER);
	BWN_WRITE_4(mac, BWN_MACCTL,
	    BWN_READ_4(mac, BWN_MACCTL) & ~BWN_MACCTL_RADIO_LOCK);
}

static struct bwn_pio_txqueue *
bwn_pio_parse_cookie(struct bwn_mac *mac, uint16_t cookie,
    struct bwn_pio_txpkt **pack)
{
	struct bwn_pio *pio = &mac->mac_method.pio;
	struct bwn_pio_txqueue *tq = NULL;
	unsigned int index;

	switch (cookie & 0xf000) {
	case 0x1000:
		tq = &pio->wme[WME_AC_BK];
		break;
	case 0x2000:
		tq = &pio->wme[WME_AC_BE];
		break;
	case 0x3000:
		tq = &pio->wme[WME_AC_VI];
		break;
	case 0x4000:
		tq = &pio->wme[WME_AC_VO];
		break;
	case 0x5000:
		tq = &pio->mcast;
		break;
	}
	KASSERT(tq != NULL, ("%s:%d: fail", __func__, __LINE__));
	if (tq == NULL)
		return (NULL);
	index = (cookie & 0x0fff);
	KASSERT(index < N(tq->tq_pkts), ("%s:%d: fail", __func__, __LINE__));
	if (index >= N(tq->tq_pkts))
		return (NULL);
	*pack = &tq->tq_pkts[index];
	KASSERT(*pack != NULL, ("%s:%d: fail", __func__, __LINE__));
	return (tq);
}

static void
bwn_txpwr(void *arg, int npending)
{
	struct bwn_mac *mac = arg;
	struct bwn_softc *sc = mac->mac_sc;

	BWN_LOCK(sc);
	if (mac && mac->mac_status >= BWN_MAC_STATUS_STARTED &&
	    mac->mac_phy.set_txpwr != NULL)
		mac->mac_phy.set_txpwr(mac);
	BWN_UNLOCK(sc);
}

static void
bwn_task_15s(struct bwn_mac *mac)
{
	uint16_t reg;

	if (mac->mac_fw.opensource) {
		reg = bwn_shm_read_2(mac, BWN_SCRATCH, BWN_WATCHDOG_REG);
		if (reg) {
			bwn_restart(mac, "fw watchdog");
			return;
		}
		bwn_shm_write_2(mac, BWN_SCRATCH, BWN_WATCHDOG_REG, 1);
	}
	if (mac->mac_phy.task_15s)
		mac->mac_phy.task_15s(mac);

	mac->mac_phy.txerrors = BWN_TXERROR_MAX;
}

static void
bwn_task_30s(struct bwn_mac *mac)
{

	if (mac->mac_phy.type != BWN_PHYTYPE_G || mac->mac_noise.noi_running)
		return;
	mac->mac_noise.noi_running = 1;
	mac->mac_noise.noi_nsamples = 0;

	bwn_noise_gensample(mac);
}

static void
bwn_task_60s(struct bwn_mac *mac)
{

	if (mac->mac_phy.task_60s)
		mac->mac_phy.task_60s(mac);
	bwn_phy_txpower_check(mac, BWN_TXPWR_IGNORE_TIME);
}

static void
bwn_tasks(void *arg)
{
	struct bwn_mac *mac = arg;
	struct bwn_softc *sc = mac->mac_sc;

	BWN_ASSERT_LOCKED(sc);
	if (mac->mac_status != BWN_MAC_STATUS_STARTED)
		return;

	if (mac->mac_task_state % 4 == 0)
		bwn_task_60s(mac);
	if (mac->mac_task_state % 2 == 0)
		bwn_task_30s(mac);
	bwn_task_15s(mac);

	mac->mac_task_state++;
	callout_reset(&sc->sc_task_ch, hz * 15, bwn_tasks, mac);
}

static int
bwn_plcp_get_ofdmrate(struct bwn_mac *mac, struct bwn_plcp6 *plcp, uint8_t a)
{
	struct bwn_softc *sc = mac->mac_sc;

	KASSERT(a == 0, ("not support APHY\n"));

	switch (plcp->o.raw[0] & 0xf) {
	case 0xb:
		return (BWN_OFDM_RATE_6MB);
	case 0xf:
		return (BWN_OFDM_RATE_9MB);
	case 0xa:
		return (BWN_OFDM_RATE_12MB);
	case 0xe:
		return (BWN_OFDM_RATE_18MB);
	case 0x9:
		return (BWN_OFDM_RATE_24MB);
	case 0xd:
		return (BWN_OFDM_RATE_36MB);
	case 0x8:
		return (BWN_OFDM_RATE_48MB);
	case 0xc:
		return (BWN_OFDM_RATE_54MB);
	}
	device_printf(sc->sc_dev, "incorrect OFDM rate %d\n",
	    plcp->o.raw[0] & 0xf);
	return (-1);
}

static int
bwn_plcp_get_cckrate(struct bwn_mac *mac, struct bwn_plcp6 *plcp)
{
	struct bwn_softc *sc = mac->mac_sc;

	switch (plcp->o.raw[0]) {
	case 0x0a:
		return (BWN_CCK_RATE_1MB);
	case 0x14:
		return (BWN_CCK_RATE_2MB);
	case 0x37:
		return (BWN_CCK_RATE_5MB);
	case 0x6e:
		return (BWN_CCK_RATE_11MB);
	}
	device_printf(sc->sc_dev, "incorrect CCK rate %d\n", plcp->o.raw[0]);
	return (-1);
}

static void
bwn_rx_radiotap(struct bwn_mac *mac, struct mbuf *m,
    const struct bwn_rxhdr4 *rxhdr, struct bwn_plcp6 *plcp, int rate,
    int rssi, int noise)
{
	struct bwn_softc *sc = mac->mac_sc;
	const struct ieee80211_frame_min *wh;
	uint64_t tsf;
	uint16_t low_mactime_now;

	if (htole16(rxhdr->phy_status0) & BWN_RX_PHYST0_SHORTPRMBL)
		sc->sc_rx_th.wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

	wh = mtod(m, const struct ieee80211_frame_min *);
	if (wh->i_fc[1] & IEEE80211_FC1_WEP)
		sc->sc_rx_th.wr_flags |= IEEE80211_RADIOTAP_F_WEP;

	bwn_tsf_read(mac, &tsf);
	low_mactime_now = tsf;
	tsf = tsf & ~0xffffULL;
	tsf += le16toh(rxhdr->mac_time);
	if (low_mactime_now < le16toh(rxhdr->mac_time))
		tsf -= 0x10000;

	sc->sc_rx_th.wr_tsf = tsf;
	sc->sc_rx_th.wr_rate = rate;
	sc->sc_rx_th.wr_antsignal = rssi;
	sc->sc_rx_th.wr_antnoise = noise;
}

static void
bwn_tsf_read(struct bwn_mac *mac, uint64_t *tsf)
{
	uint32_t low, high;

	KASSERT(siba_get_revid(mac->mac_sc->sc_dev) >= 3,
	    ("%s:%d: fail", __func__, __LINE__));

	low = BWN_READ_4(mac, BWN_REV3PLUS_TSF_LOW);
	high = BWN_READ_4(mac, BWN_REV3PLUS_TSF_HIGH);
	*tsf = high;
	*tsf <<= 32;
	*tsf |= low;
}

static int
bwn_dma_attach(struct bwn_mac *mac)
{
	struct bwn_dma *dma = &mac->mac_method.dma;
	struct bwn_softc *sc = mac->mac_sc;
	bus_addr_t lowaddr = 0;
	int error;

	if (siba_get_type(sc->sc_dev) == SIBA_TYPE_PCMCIA || bwn_usedma == 0)
		return (0);

	KASSERT(siba_get_revid(sc->sc_dev) >= 5, ("%s: fail", __func__));

	mac->mac_flags |= BWN_MAC_FLAG_DMA;

	dma->dmatype = bwn_dma_gettype(mac);
	if (dma->dmatype == BWN_DMA_30BIT)
		lowaddr = BWN_BUS_SPACE_MAXADDR_30BIT;
	else if (dma->dmatype == BWN_DMA_32BIT)
		lowaddr = BUS_SPACE_MAXADDR_32BIT;
	else
		lowaddr = BUS_SPACE_MAXADDR;

	/*
	 * Create top level DMA tag
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(sc->sc_dev),	/* parent */
			       BWN_ALIGN, 0,		/* alignment, bounds */
			       lowaddr,			/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       MAXBSIZE,		/* maxsize */
			       BUS_SPACE_UNRESTRICTED,	/* nsegments */
			       BUS_SPACE_MAXSIZE,	/* maxsegsize */
			       0,			/* flags */
			       NULL, NULL,		/* lockfunc, lockarg */
			       &dma->parent_dtag);
	if (error) {
		device_printf(sc->sc_dev, "can't create parent DMA tag\n");
		return (error);
	}

	/*
	 * Create TX/RX mbuf DMA tag
	 */
	error = bus_dma_tag_create(dma->parent_dtag,
				1,
				0,
				BUS_SPACE_MAXADDR,
				BUS_SPACE_MAXADDR,
				NULL, NULL,
				MCLBYTES,
				1,
				BUS_SPACE_MAXSIZE_32BIT,
				0,
				NULL, NULL,
				&dma->rxbuf_dtag);
	if (error) {
		device_printf(sc->sc_dev, "can't create mbuf DMA tag\n");
		goto fail0;
	}
	error = bus_dma_tag_create(dma->parent_dtag,
				1,
				0,
				BUS_SPACE_MAXADDR,
				BUS_SPACE_MAXADDR,
				NULL, NULL,
				MCLBYTES,
				1,
				BUS_SPACE_MAXSIZE_32BIT,
				0,
				NULL, NULL,
				&dma->txbuf_dtag);
	if (error) {
		device_printf(sc->sc_dev, "can't create mbuf DMA tag\n");
		goto fail1;
	}

	dma->wme[WME_AC_BK] = bwn_dma_ringsetup(mac, 0, 1, dma->dmatype);
	if (!dma->wme[WME_AC_BK])
		goto fail2;

	dma->wme[WME_AC_BE] = bwn_dma_ringsetup(mac, 1, 1, dma->dmatype);
	if (!dma->wme[WME_AC_BE])
		goto fail3;

	dma->wme[WME_AC_VI] = bwn_dma_ringsetup(mac, 2, 1, dma->dmatype);
	if (!dma->wme[WME_AC_VI])
		goto fail4;

	dma->wme[WME_AC_VO] = bwn_dma_ringsetup(mac, 3, 1, dma->dmatype);
	if (!dma->wme[WME_AC_VO])
		goto fail5;

	dma->mcast = bwn_dma_ringsetup(mac, 4, 1, dma->dmatype);
	if (!dma->mcast)
		goto fail6;
	dma->rx = bwn_dma_ringsetup(mac, 0, 0, dma->dmatype);
	if (!dma->rx)
		goto fail7;

	return (error);

fail7:	bwn_dma_ringfree(&dma->mcast);
fail6:	bwn_dma_ringfree(&dma->wme[WME_AC_VO]);
fail5:	bwn_dma_ringfree(&dma->wme[WME_AC_VI]);
fail4:	bwn_dma_ringfree(&dma->wme[WME_AC_BE]);
fail3:	bwn_dma_ringfree(&dma->wme[WME_AC_BK]);
fail2:	bus_dma_tag_destroy(dma->txbuf_dtag);
fail1:	bus_dma_tag_destroy(dma->rxbuf_dtag);
fail0:	bus_dma_tag_destroy(dma->parent_dtag);
	return (error);
}

static struct bwn_dma_ring *
bwn_dma_parse_cookie(struct bwn_mac *mac, const struct bwn_txstatus *status,
    uint16_t cookie, int *slot)
{
	struct bwn_dma *dma = &mac->mac_method.dma;
	struct bwn_dma_ring *dr;
	struct bwn_softc *sc = mac->mac_sc;

	BWN_ASSERT_LOCKED(mac->mac_sc);

	switch (cookie & 0xf000) {
	case 0x1000:
		dr = dma->wme[WME_AC_BK];
		break;
	case 0x2000:
		dr = dma->wme[WME_AC_BE];
		break;
	case 0x3000:
		dr = dma->wme[WME_AC_VI];
		break;
	case 0x4000:
		dr = dma->wme[WME_AC_VO];
		break;
	case 0x5000:
		dr = dma->mcast;
		break;
	default:
		dr = NULL;
		KASSERT(0 == 1,
		    ("invalid cookie value %d", cookie & 0xf000));
	}
	*slot = (cookie & 0x0fff);
	if (*slot < 0 || *slot >= dr->dr_numslots) {
		/*
		 * XXX FIXME: sometimes H/W returns TX DONE events duplicately
		 * that it occurs events which have same H/W sequence numbers.
		 * When it's occurred just prints a WARNING msgs and ignores.
		 */
		KASSERT(status->seq == dma->lastseq,
		    ("%s:%d: fail", __func__, __LINE__));
		device_printf(sc->sc_dev,
		    "out of slot ranges (0 < %d < %d)\n", *slot,
		    dr->dr_numslots);
		return (NULL);
	}
	dma->lastseq = status->seq;
	return (dr);
}

static void
bwn_dma_stop(struct bwn_mac *mac)
{
	struct bwn_dma *dma;

	if ((mac->mac_flags & BWN_MAC_FLAG_DMA) == 0)
		return;
	dma = &mac->mac_method.dma;

	bwn_dma_ringstop(&dma->rx);
	bwn_dma_ringstop(&dma->wme[WME_AC_BK]);
	bwn_dma_ringstop(&dma->wme[WME_AC_BE]);
	bwn_dma_ringstop(&dma->wme[WME_AC_VI]);
	bwn_dma_ringstop(&dma->wme[WME_AC_VO]);
	bwn_dma_ringstop(&dma->mcast);
}

static void
bwn_dma_ringstop(struct bwn_dma_ring **dr)
{

	if (dr == NULL)
		return;

	bwn_dma_cleanup(*dr);
}

static void
bwn_pio_stop(struct bwn_mac *mac)
{
	struct bwn_pio *pio;

	if (mac->mac_flags & BWN_MAC_FLAG_DMA)
		return;
	pio = &mac->mac_method.pio;

	bwn_destroy_queue_tx(&pio->mcast);
	bwn_destroy_queue_tx(&pio->wme[WME_AC_VO]);
	bwn_destroy_queue_tx(&pio->wme[WME_AC_VI]);
	bwn_destroy_queue_tx(&pio->wme[WME_AC_BE]);
	bwn_destroy_queue_tx(&pio->wme[WME_AC_BK]);
}

static void
bwn_led_attach(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	const uint8_t *led_act = NULL;
	uint16_t val[BWN_LED_MAX];
	int i;

	sc->sc_led_idle = (2350 * hz) / 1000;
	sc->sc_led_blink = 1;

	for (i = 0; i < N(bwn_vendor_led_act); ++i) {
		if (siba_get_pci_subvendor(sc->sc_dev) ==
		    bwn_vendor_led_act[i].vid) {
			led_act = bwn_vendor_led_act[i].led_act;
			break;
		}
	}
	if (led_act == NULL)
		led_act = bwn_default_led_act;

	val[0] = siba_sprom_get_gpio0(sc->sc_dev);
	val[1] = siba_sprom_get_gpio1(sc->sc_dev);
	val[2] = siba_sprom_get_gpio2(sc->sc_dev);
	val[3] = siba_sprom_get_gpio3(sc->sc_dev);

	for (i = 0; i < BWN_LED_MAX; ++i) {
		struct bwn_led *led = &sc->sc_leds[i];

		if (val[i] == 0xff) {
			led->led_act = led_act[i];
		} else {
			if (val[i] & BWN_LED_ACT_LOW)
				led->led_flags |= BWN_LED_F_ACTLOW;
			led->led_act = val[i] & BWN_LED_ACT_MASK;
		}
		led->led_mask = (1 << i);

		if (led->led_act == BWN_LED_ACT_BLINK_SLOW ||
		    led->led_act == BWN_LED_ACT_BLINK_POLL ||
		    led->led_act == BWN_LED_ACT_BLINK) {
			led->led_flags |= BWN_LED_F_BLINK;
			if (led->led_act == BWN_LED_ACT_BLINK_POLL)
				led->led_flags |= BWN_LED_F_POLLABLE;
			else if (led->led_act == BWN_LED_ACT_BLINK_SLOW)
				led->led_flags |= BWN_LED_F_SLOW;

			if (sc->sc_blink_led == NULL) {
				sc->sc_blink_led = led;
				if (led->led_flags & BWN_LED_F_SLOW)
					BWN_LED_SLOWDOWN(sc->sc_led_idle);
			}
		}

		DPRINTF(sc, BWN_DEBUG_LED,
		    "%dth led, act %d, lowact %d\n", i,
		    led->led_act, led->led_flags & BWN_LED_F_ACTLOW);
	}
	callout_init_mtx(&sc->sc_led_blink_ch, &sc->sc_mtx, 0);
}

static __inline uint16_t
bwn_led_onoff(const struct bwn_led *led, uint16_t val, int on)
{

	if (led->led_flags & BWN_LED_F_ACTLOW)
		on = !on;
	if (on)
		val |= led->led_mask;
	else
		val &= ~led->led_mask;
	return val;
}

static void
bwn_led_newstate(struct bwn_mac *mac, enum ieee80211_state nstate)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint16_t val;
	int i;

	if (nstate == IEEE80211_S_INIT) {
		callout_stop(&sc->sc_led_blink_ch);
		sc->sc_led_blinking = 0;
	}

	if ((ic->ic_ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	val = BWN_READ_2(mac, BWN_GPIO_CONTROL);
	for (i = 0; i < BWN_LED_MAX; ++i) {
		struct bwn_led *led = &sc->sc_leds[i];
		int on;

		if (led->led_act == BWN_LED_ACT_UNKN ||
		    led->led_act == BWN_LED_ACT_NULL)
			continue;

		if ((led->led_flags & BWN_LED_F_BLINK) &&
		    nstate != IEEE80211_S_INIT)
			continue;

		switch (led->led_act) {
		case BWN_LED_ACT_ON:    /* Always on */
			on = 1;
			break;
		case BWN_LED_ACT_OFF:   /* Always off */
		case BWN_LED_ACT_5GHZ:  /* TODO: 11A */
			on = 0;
			break;
		default:
			on = 1;
			switch (nstate) {
			case IEEE80211_S_INIT:
				on = 0;
				break;
			case IEEE80211_S_RUN:
				if (led->led_act == BWN_LED_ACT_11G &&
				    ic->ic_curmode != IEEE80211_MODE_11G)
					on = 0;
				break;
			default:
				if (led->led_act == BWN_LED_ACT_ASSOC)
					on = 0;
				break;
			}
			break;
		}

		val = bwn_led_onoff(led, val, on);
	}
	BWN_WRITE_2(mac, BWN_GPIO_CONTROL, val);
}

static void
bwn_led_event(struct bwn_mac *mac, int event)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_led *led = sc->sc_blink_led;
	int rate;

	if (event == BWN_LED_EVENT_POLL) {
		if ((led->led_flags & BWN_LED_F_POLLABLE) == 0)
			return;
		if (ticks - sc->sc_led_ticks < sc->sc_led_idle)
			return;
	}

	sc->sc_led_ticks = ticks;
	if (sc->sc_led_blinking)
		return;

	switch (event) {
	case BWN_LED_EVENT_RX:
		rate = sc->sc_rx_rate;
		break;
	case BWN_LED_EVENT_TX:
		rate = sc->sc_tx_rate;
		break;
	case BWN_LED_EVENT_POLL:
		rate = 0;
		break;
	default:
		panic("unknown LED event %d\n", event);
		break;
	}
	bwn_led_blink_start(mac, bwn_led_duration[rate].on_dur,
	    bwn_led_duration[rate].off_dur);
}

static void
bwn_led_blink_start(struct bwn_mac *mac, int on_dur, int off_dur)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_led *led = sc->sc_blink_led;
	uint16_t val;

	val = BWN_READ_2(mac, BWN_GPIO_CONTROL);
	val = bwn_led_onoff(led, val, 1);
	BWN_WRITE_2(mac, BWN_GPIO_CONTROL, val);

	if (led->led_flags & BWN_LED_F_SLOW) {
		BWN_LED_SLOWDOWN(on_dur);
		BWN_LED_SLOWDOWN(off_dur);
	}

	sc->sc_led_blinking = 1;
	sc->sc_led_blink_offdur = off_dur;

	callout_reset(&sc->sc_led_blink_ch, on_dur, bwn_led_blink_next, mac);
}

static void
bwn_led_blink_next(void *arg)
{
	struct bwn_mac *mac = arg;
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t val;

	val = BWN_READ_2(mac, BWN_GPIO_CONTROL);
	val = bwn_led_onoff(sc->sc_blink_led, val, 0);
	BWN_WRITE_2(mac, BWN_GPIO_CONTROL, val);

	callout_reset(&sc->sc_led_blink_ch, sc->sc_led_blink_offdur,
	    bwn_led_blink_end, mac);
}

static void
bwn_led_blink_end(void *arg)
{
	struct bwn_mac *mac = arg;
	struct bwn_softc *sc = mac->mac_sc;

	sc->sc_led_blinking = 0;
}

static int
bwn_suspend(device_t dev)
{
	struct bwn_softc *sc = device_get_softc(dev);

	bwn_stop(sc, 1);
	return (0);
}

static int
bwn_resume(device_t dev)
{
	struct bwn_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->sc_ifp;

	if (ifp->if_flags & IFF_UP)
		bwn_init(sc);
	return (0);
}

static void
bwn_rfswitch(void *arg)
{
	struct bwn_softc *sc = arg;
	struct bwn_mac *mac = sc->sc_curmac;
	int cur = 0, prev = 0;

	KASSERT(mac->mac_status >= BWN_MAC_STATUS_STARTED,
	    ("%s: invalid MAC status %d", __func__, mac->mac_status));

	if (mac->mac_phy.rf_rev >= 3 || mac->mac_phy.type == BWN_PHYTYPE_LP) {
		if (!(BWN_READ_4(mac, BWN_RF_HWENABLED_HI)
			& BWN_RF_HWENABLED_HI_MASK))
			cur = 1;
	} else {
		if (BWN_READ_2(mac, BWN_RF_HWENABLED_LO)
		    & BWN_RF_HWENABLED_LO_MASK)
			cur = 1;
	}

	if (mac->mac_flags & BWN_MAC_FLAG_RADIO_ON)
		prev = 1;

	if (cur != prev) {
		if (cur)
			mac->mac_flags |= BWN_MAC_FLAG_RADIO_ON;
		else
			mac->mac_flags &= ~BWN_MAC_FLAG_RADIO_ON;

		device_printf(sc->sc_dev,
		    "status of RF switch is changed to %s\n",
		    cur ? "ON" : "OFF");
		if (cur != mac->mac_phy.rf_on) {
			if (cur)
				bwn_rf_turnon(mac);
			else
				bwn_rf_turnoff(mac);
		}
	}

	callout_schedule(&sc->sc_rfswitch_ch, hz);
}

static void
bwn_phy_lp_init_pre(struct bwn_mac *mac)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_lp *plp = &phy->phy_lp;

	plp->plp_antenna = BWN_ANT_DEFAULT;
}

static int
bwn_phy_lp_init(struct bwn_mac *mac)
{
	static const struct bwn_stxtable tables[] = {
		{ 2,  6, 0x3d, 3, 0x01 }, { 1, 12, 0x4c, 1, 0x01 },
		{ 1,  8, 0x50, 0, 0x7f }, { 0,  8, 0x44, 0, 0xff },
		{ 1,  0, 0x4a, 0, 0xff }, { 0,  4, 0x4d, 0, 0xff },
		{ 1,  4, 0x4e, 0, 0xff }, { 0, 12, 0x4f, 0, 0x0f },
		{ 1,  0, 0x4f, 4, 0x0f }, { 3,  0, 0x49, 0, 0x0f },
		{ 4,  3, 0x46, 4, 0x07 }, { 3, 15, 0x46, 0, 0x01 },
		{ 4,  0, 0x46, 1, 0x07 }, { 3,  8, 0x48, 4, 0x07 },
		{ 3, 11, 0x48, 0, 0x0f }, { 3,  4, 0x49, 4, 0x0f },
		{ 2, 15, 0x45, 0, 0x01 }, { 5, 13, 0x52, 4, 0x07 },
		{ 6,  0, 0x52, 7, 0x01 }, { 5,  3, 0x41, 5, 0x07 },
		{ 5,  6, 0x41, 0, 0x0f }, { 5, 10, 0x42, 5, 0x07 },
		{ 4, 15, 0x42, 0, 0x01 }, { 5,  0, 0x42, 1, 0x07 },
		{ 4, 11, 0x43, 4, 0x0f }, { 4,  7, 0x43, 0, 0x0f },
		{ 4,  6, 0x45, 1, 0x01 }, { 2,  7, 0x40, 4, 0x0f },
		{ 2, 11, 0x40, 0, 0x0f }
	};
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	const struct bwn_stxtable *st;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	int i, error;
	uint16_t tmp;

	bwn_phy_lp_readsprom(mac);	/* XXX bad place */
	bwn_phy_lp_bbinit(mac);

	/* initialize RF */
	BWN_PHY_SET(mac, BWN_PHY_4WIRECTL, 0x2);
	DELAY(1);
	BWN_PHY_MASK(mac, BWN_PHY_4WIRECTL, 0xfffd);
	DELAY(1);

	if (mac->mac_phy.rf_ver == 0x2062)
		bwn_phy_lp_b2062_init(mac);
	else {
		bwn_phy_lp_b2063_init(mac);

		/* synchronize stx table. */
		for (i = 0; i < N(tables); i++) {
			st = &tables[i];
			tmp = BWN_RF_READ(mac, st->st_rfaddr);
			tmp >>= st->st_rfshift;
			tmp <<= st->st_physhift;
			BWN_PHY_SETMASK(mac,
			    BWN_PHY_OFDM(0xf2 + st->st_phyoffset),
			    ~(st->st_mask << st->st_physhift), tmp);
		}

		BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0xf0), 0x5f80);
		BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0xf1), 0);
	}

	/* calibrate RC */
	if (mac->mac_phy.rev >= 2)
		bwn_phy_lp_rxcal_r2(mac);
	else if (!plp->plp_rccap) {
		if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			bwn_phy_lp_rccal_r12(mac);
	} else
		bwn_phy_lp_set_rccap(mac);

	error = bwn_phy_lp_switch_channel(mac, 7);
	if (error)
		device_printf(sc->sc_dev,
		    "failed to change channel 7 (%d)\n", error);
	bwn_phy_lp_txpctl_init(mac);
	bwn_phy_lp_calib(mac);
	return (0);
}

static uint16_t
bwn_phy_lp_read(struct bwn_mac *mac, uint16_t reg)
{

	BWN_WRITE_2(mac, BWN_PHYCTL, reg);
	return (BWN_READ_2(mac, BWN_PHYDATA));
}

static void
bwn_phy_lp_write(struct bwn_mac *mac, uint16_t reg, uint16_t value)
{

	BWN_WRITE_2(mac, BWN_PHYCTL, reg);
	BWN_WRITE_2(mac, BWN_PHYDATA, value);
}

static void
bwn_phy_lp_maskset(struct bwn_mac *mac, uint16_t reg, uint16_t mask,
    uint16_t set)
{

	BWN_WRITE_2(mac, BWN_PHYCTL, reg);
	BWN_WRITE_2(mac, BWN_PHYDATA,
	    (BWN_READ_2(mac, BWN_PHYDATA) & mask) | set);
}

static uint16_t
bwn_phy_lp_rf_read(struct bwn_mac *mac, uint16_t reg)
{

	KASSERT(reg != 1, ("unaccessible register %d", reg));
	if (mac->mac_phy.rev < 2 && reg != 0x4001)
		reg |= 0x100;
	if (mac->mac_phy.rev >= 2)
		reg |= 0x200;
	BWN_WRITE_2(mac, BWN_RFCTL, reg);
	return BWN_READ_2(mac, BWN_RFDATALO);
}

static void
bwn_phy_lp_rf_write(struct bwn_mac *mac, uint16_t reg, uint16_t value)
{

	KASSERT(reg != 1, ("unaccessible register %d", reg));
	BWN_WRITE_2(mac, BWN_RFCTL, reg);
	BWN_WRITE_2(mac, BWN_RFDATALO, value);
}

static void
bwn_phy_lp_rf_onoff(struct bwn_mac *mac, int on)
{

	if (on) {
		BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_0, 0xe0ff);
		BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2,
		    (mac->mac_phy.rev >= 2) ? 0xf7f7 : 0xffe7);
		return;
	}

	if (mac->mac_phy.rev >= 2) {
		BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0x83ff);
		BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x1f00);
		BWN_PHY_MASK(mac, BWN_PHY_AFE_DDFS, 0x80ff);
		BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0xdfff);
		BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2, 0x0808);
		return;
	}

	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xe0ff);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x1f00);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0xfcff);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2, 0x0018);
}

static int
bwn_phy_lp_switch_channel(struct bwn_mac *mac, uint32_t chan)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_lp *plp = &phy->phy_lp;
	int error;

	if (phy->rf_ver == 0x2063) {
		error = bwn_phy_lp_b2063_switch_channel(mac, chan);
		if (error)
			return (error);
	} else {
		error = bwn_phy_lp_b2062_switch_channel(mac, chan);
		if (error)
			return (error);
		bwn_phy_lp_set_anafilter(mac, chan);
		bwn_phy_lp_set_gaintbl(mac, ieee80211_ieee2mhz(chan, 0));
	}

	plp->plp_chan = chan;
	BWN_WRITE_2(mac, BWN_CHANNEL, chan);
	return (0);
}

static uint32_t
bwn_phy_lp_get_default_chan(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	return (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan) ? 1 : 36);
}

static void
bwn_phy_lp_set_antenna(struct bwn_mac *mac, int antenna)
{
	struct bwn_phy *phy = &mac->mac_phy;
	struct bwn_phy_lp *plp = &phy->phy_lp;

	if (phy->rev >= 2 || antenna > BWN_ANTAUTO1)
		return;

	bwn_hf_write(mac, bwn_hf_read(mac) & ~BWN_HF_UCODE_ANTDIV_HELPER);
	BWN_PHY_SETMASK(mac, BWN_PHY_CRSGAIN_CTL, 0xfffd, antenna & 0x2);
	BWN_PHY_SETMASK(mac, BWN_PHY_CRSGAIN_CTL, 0xfffe, antenna & 0x1);
	bwn_hf_write(mac, bwn_hf_read(mac) | BWN_HF_UCODE_ANTDIV_HELPER);
	plp->plp_antenna = antenna;
}

static void
bwn_phy_lp_task_60s(struct bwn_mac *mac)
{

	bwn_phy_lp_calib(mac);
}

static void
bwn_phy_lp_readsprom(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
		plp->plp_txisoband_m = siba_sprom_get_tri2g(sc->sc_dev);
		plp->plp_bxarch = siba_sprom_get_bxa2g(sc->sc_dev);
		plp->plp_rxpwroffset = siba_sprom_get_rxpo2g(sc->sc_dev);
		plp->plp_rssivf = siba_sprom_get_rssismf2g(sc->sc_dev);
		plp->plp_rssivc = siba_sprom_get_rssismc2g(sc->sc_dev);
		plp->plp_rssigs = siba_sprom_get_rssisav2g(sc->sc_dev);
		return;
	}

	plp->plp_txisoband_l = siba_sprom_get_tri5gl(sc->sc_dev);
	plp->plp_txisoband_m = siba_sprom_get_tri5g(sc->sc_dev);
	plp->plp_txisoband_h = siba_sprom_get_tri5gh(sc->sc_dev);
	plp->plp_bxarch = siba_sprom_get_bxa5g(sc->sc_dev);
	plp->plp_rxpwroffset = siba_sprom_get_rxpo5g(sc->sc_dev);
	plp->plp_rssivf = siba_sprom_get_rssismf5g(sc->sc_dev);
	plp->plp_rssivc = siba_sprom_get_rssismc5g(sc->sc_dev);
	plp->plp_rssigs = siba_sprom_get_rssisav5g(sc->sc_dev);
}

static void
bwn_phy_lp_bbinit(struct bwn_mac *mac)
{

	bwn_phy_lp_tblinit(mac);
	if (mac->mac_phy.rev >= 2)
		bwn_phy_lp_bbinit_r2(mac);
	else
		bwn_phy_lp_bbinit_r01(mac);
}

static void
bwn_phy_lp_txpctl_init(struct bwn_mac *mac)
{
	struct bwn_txgain gain_2ghz = { 4, 12, 12, 0 };
	struct bwn_txgain gain_5ghz = { 7, 15, 14, 0 };
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	bwn_phy_lp_set_txgain(mac,
	    IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan) ? &gain_2ghz : &gain_5ghz);
	bwn_phy_lp_set_bbmult(mac, 150);
}

static void
bwn_phy_lp_calib(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	const struct bwn_rxcompco *rc = NULL;
	struct bwn_txgain ogain;
	int i, omode, oafeovr, orf, obbmult;
	uint8_t mode, fc = 0;

	if (plp->plp_chanfullcal != plp->plp_chan) {
		plp->plp_chanfullcal = plp->plp_chan;
		fc = 1;
	}

	bwn_mac_suspend(mac);

	/* BlueTooth Coexistance Override */
	BWN_WRITE_2(mac, BWN_BTCOEX_CTL, 0x3);
	BWN_WRITE_2(mac, BWN_BTCOEX_TXCTL, 0xff);

	if (mac->mac_phy.rev >= 2)
		bwn_phy_lp_digflt_save(mac);
	bwn_phy_lp_get_txpctlmode(mac);
	mode = plp->plp_txpctlmode;
	bwn_phy_lp_set_txpctlmode(mac, BWN_PHYLP_TXPCTL_OFF);
	if (mac->mac_phy.rev == 0 && mode != BWN_PHYLP_TXPCTL_OFF)
		bwn_phy_lp_bugfix(mac);
	if (mac->mac_phy.rev >= 2 && fc == 1) {
		bwn_phy_lp_get_txpctlmode(mac);
		omode = plp->plp_txpctlmode;
		oafeovr = BWN_PHY_READ(mac, BWN_PHY_AFE_CTL_OVR) & 0x40;
		if (oafeovr)
			ogain = bwn_phy_lp_get_txgain(mac);
		orf = BWN_PHY_READ(mac, BWN_PHY_RF_PWR_OVERRIDE) & 0xff;
		obbmult = bwn_phy_lp_get_bbmult(mac);
		bwn_phy_lp_set_txpctlmode(mac, BWN_PHYLP_TXPCTL_OFF);
		if (oafeovr)
			bwn_phy_lp_set_txgain(mac, &ogain);
		bwn_phy_lp_set_bbmult(mac, obbmult);
		bwn_phy_lp_set_txpctlmode(mac, omode);
		BWN_PHY_SETMASK(mac, BWN_PHY_RF_PWR_OVERRIDE, 0xff00, orf);
	}
	bwn_phy_lp_set_txpctlmode(mac, mode);
	if (mac->mac_phy.rev >= 2)
		bwn_phy_lp_digflt_restore(mac);

	/* do RX IQ Calculation; assumes that noise is true. */
	if (siba_get_chipid(sc->sc_dev) == 0x5354) {
		for (i = 0; i < N(bwn_rxcompco_5354); i++) {
			if (bwn_rxcompco_5354[i].rc_chan == plp->plp_chan)
				rc = &bwn_rxcompco_5354[i];
		}
	} else if (mac->mac_phy.rev >= 2)
		rc = &bwn_rxcompco_r2;
	else {
		for (i = 0; i < N(bwn_rxcompco_r12); i++) {
			if (bwn_rxcompco_r12[i].rc_chan == plp->plp_chan)
				rc = &bwn_rxcompco_r12[i];
		}
	}
	if (rc == NULL)
		goto fail;

	BWN_PHY_SETMASK(mac, BWN_PHY_RX_COMP_COEFF_S, 0xff00, rc->rc_c1);
	BWN_PHY_SETMASK(mac, BWN_PHY_RX_COMP_COEFF_S, 0x00ff, rc->rc_c0 << 8);

	bwn_phy_lp_set_trsw_over(mac, 1 /* TX */, 0 /* RX */);

	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
		BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x8);
		BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xfff7, 0);
	} else {
		BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x20);
		BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xffdf, 0);
	}

	bwn_phy_lp_set_rxgain(mac, 0x2d5d);
	BWN_PHY_MASK(mac, BWN_PHY_AFE_CTL_OVR, 0xfffe);
	BWN_PHY_MASK(mac, BWN_PHY_AFE_CTL_OVRVAL, 0xfffe);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x800);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0x800);
	bwn_phy_lp_set_deaf(mac, 0);
	/* XXX no checking return value? */
	(void)bwn_phy_lp_calc_rx_iq_comp(mac, 0xfff0);
	bwn_phy_lp_clear_deaf(mac, 0);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_0, 0xfffc);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_0, 0xfff7);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_0, 0xffdf);

	/* disable RX GAIN override. */
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_0, 0xfffe);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_0, 0xffef);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_0, 0xffbf);
	if (mac->mac_phy.rev >= 2) {
		BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2, 0xfeff);
		if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
			BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2, 0xfbff);
			BWN_PHY_MASK(mac, BWN_PHY_OFDM(0xe5), 0xfff7);
		}
	} else {
		BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2, 0xfdff);
	}

	BWN_PHY_MASK(mac, BWN_PHY_AFE_CTL_OVR, 0xfffe);
	BWN_PHY_MASK(mac, BWN_PHY_AFE_CTL_OVRVAL, 0xf7ff);
fail:
	bwn_mac_enable(mac);
}

static void
bwn_phy_lp_switch_analog(struct bwn_mac *mac, int on)
{

	if (on) {
		BWN_PHY_MASK(mac, BWN_PHY_AFE_CTL_OVR, 0xfff8);
		return;
	}

	BWN_PHY_SET(mac, BWN_PHY_AFE_CTL_OVRVAL, 0x0007);
	BWN_PHY_SET(mac, BWN_PHY_AFE_CTL_OVR, 0x0007);
}

static int
bwn_phy_lp_b2063_switch_channel(struct bwn_mac *mac, uint8_t chan)
{
	static const struct bwn_b206x_chan *bc = NULL;
	struct bwn_softc *sc = mac->mac_sc;
	uint32_t count, freqref, freqvco, freqxtal, val[3], timeout, timeoutref,
	    tmp[6];
	uint16_t old, scale, tmp16;
	int i, div;

	for (i = 0; i < N(bwn_b2063_chantable); i++) {
		if (bwn_b2063_chantable[i].bc_chan == chan) {
			bc = &bwn_b2063_chantable[i];
			break;
		}
	}
	if (bc == NULL)
		return (EINVAL);

	BWN_RF_WRITE(mac, BWN_B2063_LOGEN_VCOBUF1, bc->bc_data[0]);
	BWN_RF_WRITE(mac, BWN_B2063_LOGEN_MIXER2, bc->bc_data[1]);
	BWN_RF_WRITE(mac, BWN_B2063_LOGEN_BUF2, bc->bc_data[2]);
	BWN_RF_WRITE(mac, BWN_B2063_LOGEN_RCCR1, bc->bc_data[3]);
	BWN_RF_WRITE(mac, BWN_B2063_A_RX_1ST3, bc->bc_data[4]);
	BWN_RF_WRITE(mac, BWN_B2063_A_RX_2ND1, bc->bc_data[5]);
	BWN_RF_WRITE(mac, BWN_B2063_A_RX_2ND4, bc->bc_data[6]);
	BWN_RF_WRITE(mac, BWN_B2063_A_RX_2ND7, bc->bc_data[7]);
	BWN_RF_WRITE(mac, BWN_B2063_A_RX_PS6, bc->bc_data[8]);
	BWN_RF_WRITE(mac, BWN_B2063_TX_RF_CTL2, bc->bc_data[9]);
	BWN_RF_WRITE(mac, BWN_B2063_TX_RF_CTL5, bc->bc_data[10]);
	BWN_RF_WRITE(mac, BWN_B2063_PA_CTL11, bc->bc_data[11]);

	old = BWN_RF_READ(mac, BWN_B2063_COM15);
	BWN_RF_SET(mac, BWN_B2063_COM15, 0x1e);

	freqxtal = siba_get_cc_pmufreq(sc->sc_dev) * 1000;
	freqvco = bc->bc_freq << ((bc->bc_freq > 4000) ? 1 : 2);
	freqref = freqxtal * 3;
	div = (freqxtal <= 26000000 ? 1 : 2);
	timeout = ((((8 * freqxtal) / (div * 5000000)) + 1) >> 1) - 1;
	timeoutref = ((((8 * freqxtal) / (div * (timeout + 1))) +
		999999) / 1000000) + 1;

	BWN_RF_WRITE(mac, BWN_B2063_JTAG_VCO_CALIB3, 0x2);
	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_VCO_CALIB6,
	    0xfff8, timeout >> 2);
	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_VCO_CALIB7,
	    0xff9f,timeout << 5);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_VCO_CALIB5, timeoutref);

	val[0] = bwn_phy_lp_roundup(freqxtal, 1000000, 16);
	val[1] = bwn_phy_lp_roundup(freqxtal, 1000000 * div, 16);
	val[2] = bwn_phy_lp_roundup(freqvco, 3, 16);

	count = (bwn_phy_lp_roundup(val[2], val[1] + 16, 16) * (timeout + 1) *
	    (timeoutref + 1)) - 1;
	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_VCO_CALIB7,
	    0xf0, count >> 8);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_VCO_CALIB8, count & 0xff);

	tmp[0] = ((val[2] * 62500) / freqref) << 4;
	tmp[1] = ((val[2] * 62500) % freqref) << 4;
	while (tmp[1] >= freqref) {
		tmp[0]++;
		tmp[1] -= freqref;
	}
	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_SG1, 0xffe0, tmp[0] >> 4);
	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_SG2, 0xfe0f, tmp[0] << 4);
	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_SG2, 0xfff0, tmp[0] >> 16);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_SG3, (tmp[1] >> 8) & 0xff);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_SG4, tmp[1] & 0xff);

	BWN_RF_WRITE(mac, BWN_B2063_JTAG_LF1, 0xb9);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_LF2, 0x88);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_LF3, 0x28);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_LF4, 0x63);

	tmp[2] = ((41 * (val[2] - 3000)) /1200) + 27;
	tmp[3] = bwn_phy_lp_roundup(132000 * tmp[0], 8451, 16);

	if ((tmp[3] + tmp[2] - 1) / tmp[2] > 60) {
		scale = 1;
		tmp[4] = ((tmp[3] + tmp[2]) / (tmp[2] << 1)) - 8;
	} else {
		scale = 0;
		tmp[4] = ((tmp[3] + (tmp[2] >> 1)) / tmp[2]) - 8;
	}
	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_CP2, 0xffc0, tmp[4]);
	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_CP2, 0xffbf, scale << 6);

	tmp[5] = bwn_phy_lp_roundup(100 * val[0], val[2], 16) * (tmp[4] * 8) *
	    (scale + 1);
	if (tmp[5] > 150)
		tmp[5] = 0;

	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_CP3, 0xffe0, tmp[5]);
	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_CP3, 0xffdf, scale << 5);

	BWN_RF_SETMASK(mac, BWN_B2063_JTAG_XTAL_12, 0xfffb, 0x4);
	if (freqxtal > 26000000)
		BWN_RF_SET(mac, BWN_B2063_JTAG_XTAL_12, 0x2);
	else
		BWN_RF_MASK(mac, BWN_B2063_JTAG_XTAL_12, 0xfd);

	if (val[0] == 45)
		BWN_RF_SET(mac, BWN_B2063_JTAG_VCO1, 0x2);
	else
		BWN_RF_MASK(mac, BWN_B2063_JTAG_VCO1, 0xfd);

	BWN_RF_SET(mac, BWN_B2063_PLL_SP2, 0x3);
	DELAY(1);
	BWN_RF_MASK(mac, BWN_B2063_PLL_SP2, 0xfffc);

	/* VCO Calibration */
	BWN_RF_MASK(mac, BWN_B2063_PLL_SP1, ~0x40);
	tmp16 = BWN_RF_READ(mac, BWN_B2063_JTAG_CALNRST) & 0xf8;
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_CALNRST, tmp16);
	DELAY(1);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_CALNRST, tmp16 | 0x4);
	DELAY(1);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_CALNRST, tmp16 | 0x6);
	DELAY(1);
	BWN_RF_WRITE(mac, BWN_B2063_JTAG_CALNRST, tmp16 | 0x7);
	DELAY(300);
	BWN_RF_SET(mac, BWN_B2063_PLL_SP1, 0x40);

	BWN_RF_WRITE(mac, BWN_B2063_COM15, old);
	return (0);
}

static int
bwn_phy_lp_b2062_switch_channel(struct bwn_mac *mac, uint8_t chan)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	const struct bwn_b206x_chan *bc = NULL;
	uint32_t freqxtal = siba_get_cc_pmufreq(sc->sc_dev) * 1000;
	uint32_t tmp[9];
	int i;

	for (i = 0; i < N(bwn_b2062_chantable); i++) {
		if (bwn_b2062_chantable[i].bc_chan == chan) {
			bc = &bwn_b2062_chantable[i];
			break;
		}
	}

	if (bc == NULL)
		return (EINVAL);

	BWN_RF_SET(mac, BWN_B2062_S_RFPLLCTL14, 0x04);
	BWN_RF_WRITE(mac, BWN_B2062_N_LGENATUNE0, bc->bc_data[0]);
	BWN_RF_WRITE(mac, BWN_B2062_N_LGENATUNE2, bc->bc_data[1]);
	BWN_RF_WRITE(mac, BWN_B2062_N_LGENATUNE3, bc->bc_data[2]);
	BWN_RF_WRITE(mac, BWN_B2062_N_TX_TUNE, bc->bc_data[3]);
	BWN_RF_WRITE(mac, BWN_B2062_S_LGENG_CTL1, bc->bc_data[4]);
	BWN_RF_WRITE(mac, BWN_B2062_N_LGENACTL5, bc->bc_data[5]);
	BWN_RF_WRITE(mac, BWN_B2062_N_LGENACTL6, bc->bc_data[6]);
	BWN_RF_WRITE(mac, BWN_B2062_N_TX_PGA, bc->bc_data[7]);
	BWN_RF_WRITE(mac, BWN_B2062_N_TX_PAD, bc->bc_data[8]);

	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL33, 0xcc);
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL34, 0x07);
	bwn_phy_lp_b2062_reset_pllbias(mac);
	tmp[0] = freqxtal / 1000;
	tmp[1] = plp->plp_div * 1000;
	tmp[2] = tmp[1] * ieee80211_ieee2mhz(chan, 0);
	if (ieee80211_ieee2mhz(chan, 0) < 4000)
		tmp[2] *= 2;
	tmp[3] = 48 * tmp[0];
	tmp[5] = tmp[2] / tmp[3];
	tmp[6] = tmp[2] % tmp[3];
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL26, tmp[5]);
	tmp[4] = tmp[6] * 0x100;
	tmp[5] = tmp[4] / tmp[3];
	tmp[6] = tmp[4] % tmp[3];
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL27, tmp[5]);
	tmp[4] = tmp[6] * 0x100;
	tmp[5] = tmp[4] / tmp[3];
	tmp[6] = tmp[4] % tmp[3];
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL28, tmp[5]);
	tmp[4] = tmp[6] * 0x100;
	tmp[5] = tmp[4] / tmp[3];
	tmp[6] = tmp[4] % tmp[3];
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL29,
	    tmp[5] + ((2 * tmp[6]) / tmp[3]));
	tmp[7] = BWN_RF_READ(mac, BWN_B2062_S_RFPLLCTL19);
	tmp[8] = ((2 * tmp[2] * (tmp[7] + 1)) + (3 * tmp[0])) / (6 * tmp[0]);
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL23, (tmp[8] >> 8) + 16);
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL24, tmp[8] & 0xff);

	bwn_phy_lp_b2062_vco_calib(mac);
	if (BWN_RF_READ(mac, BWN_B2062_S_RFPLLCTL3) & 0x10) {
		BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL33, 0xfc);
		BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL34, 0);
		bwn_phy_lp_b2062_reset_pllbias(mac);
		bwn_phy_lp_b2062_vco_calib(mac);
		if (BWN_RF_READ(mac, BWN_B2062_S_RFPLLCTL3) & 0x10) {
			BWN_RF_MASK(mac, BWN_B2062_S_RFPLLCTL14, ~0x04);
			return (EIO);
		}
	}
	BWN_RF_MASK(mac, BWN_B2062_S_RFPLLCTL14, ~0x04);
	return (0);
}

static void
bwn_phy_lp_set_anafilter(struct bwn_mac *mac, uint8_t channel)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	uint16_t tmp = (channel == 14);

	if (mac->mac_phy.rev < 2) {
		BWN_PHY_SETMASK(mac, BWN_PHY_LP_PHY_CTL, 0xfcff, tmp << 9);
		if ((mac->mac_phy.rev == 1) && (plp->plp_rccap))
			bwn_phy_lp_set_rccap(mac);
		return;
	}

	BWN_RF_WRITE(mac, BWN_B2063_TX_BB_SP3, 0x3f);
}

static void
bwn_phy_lp_set_gaintbl(struct bwn_mac *mac, uint32_t freq)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint16_t iso, tmp[3];

	KASSERT(mac->mac_phy.rev < 2, ("%s:%d: fail", __func__, __LINE__));

	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
		iso = plp->plp_txisoband_m;
	else if (freq <= 5320)
		iso = plp->plp_txisoband_l;
	else if (freq <= 5700)
		iso = plp->plp_txisoband_m;
	else
		iso = plp->plp_txisoband_h;

	tmp[0] = ((iso - 26) / 12) << 12;
	tmp[1] = tmp[0] + 0x1000;
	tmp[2] = tmp[0] + 0x2000;

	bwn_tab_write_multi(mac, BWN_TAB_2(13, 0), 3, tmp);
	bwn_tab_write_multi(mac, BWN_TAB_2(12, 0), 3, tmp);
}

static void
bwn_phy_lp_digflt_save(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	int i;
	static const uint16_t addr[] = {
		BWN_PHY_OFDM(0xc1), BWN_PHY_OFDM(0xc2),
		BWN_PHY_OFDM(0xc3), BWN_PHY_OFDM(0xc4),
		BWN_PHY_OFDM(0xc5), BWN_PHY_OFDM(0xc6),
		BWN_PHY_OFDM(0xc7), BWN_PHY_OFDM(0xc8),
		BWN_PHY_OFDM(0xcf),
	};
	static const uint16_t val[] = {
		0xde5e, 0xe832, 0xe331, 0x4d26,
		0x0026, 0x1420, 0x0020, 0xfe08,
		0x0008,
	};

	for (i = 0; i < N(addr); i++) {
		plp->plp_digfilt[i] = BWN_PHY_READ(mac, addr[i]);
		BWN_PHY_WRITE(mac, addr[i], val[i]);
	}
}

static void
bwn_phy_lp_get_txpctlmode(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	uint16_t ctl;

	ctl = BWN_PHY_READ(mac, BWN_PHY_TX_PWR_CTL_CMD);
	switch (ctl & BWN_PHY_TX_PWR_CTL_CMD_MODE) {
	case BWN_PHY_TX_PWR_CTL_CMD_MODE_OFF:
		plp->plp_txpctlmode = BWN_PHYLP_TXPCTL_OFF;
		break;
	case BWN_PHY_TX_PWR_CTL_CMD_MODE_SW:
		plp->plp_txpctlmode = BWN_PHYLP_TXPCTL_ON_SW;
		break;
	case BWN_PHY_TX_PWR_CTL_CMD_MODE_HW:
		plp->plp_txpctlmode = BWN_PHYLP_TXPCTL_ON_HW;
		break;
	default:
		plp->plp_txpctlmode = BWN_PHYLP_TXPCTL_UNKNOWN;
		device_printf(sc->sc_dev, "unknown command mode\n");
		break;
	}
}

static void
bwn_phy_lp_set_txpctlmode(struct bwn_mac *mac, uint8_t mode)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	uint16_t ctl;
	uint8_t old;

	bwn_phy_lp_get_txpctlmode(mac);
	old = plp->plp_txpctlmode;
	if (old == mode)
		return;
	plp->plp_txpctlmode = mode;

	if (old != BWN_PHYLP_TXPCTL_ON_HW && mode == BWN_PHYLP_TXPCTL_ON_HW) {
		BWN_PHY_SETMASK(mac, BWN_PHY_TX_PWR_CTL_CMD, 0xff80,
		    plp->plp_tssiidx);
		BWN_PHY_SETMASK(mac, BWN_PHY_TX_PWR_CTL_NNUM,
		    0x8fff, ((uint16_t)plp->plp_tssinpt << 16));

		/* disable TX GAIN override */
		if (mac->mac_phy.rev < 2)
			BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2, 0xfeff);
		else {
			BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2, 0xff7f);
			BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2, 0xbfff);
		}
		BWN_PHY_MASK(mac, BWN_PHY_AFE_CTL_OVR, 0xffbf);

		plp->plp_txpwridx = -1;
	}
	if (mac->mac_phy.rev >= 2) {
		if (mode == BWN_PHYLP_TXPCTL_ON_HW)
			BWN_PHY_SET(mac, BWN_PHY_OFDM(0xd0), 0x2);
		else
			BWN_PHY_MASK(mac, BWN_PHY_OFDM(0xd0), 0xfffd);
	}

	/* writes TX Power Control mode */
	switch (plp->plp_txpctlmode) {
	case BWN_PHYLP_TXPCTL_OFF:
		ctl = BWN_PHY_TX_PWR_CTL_CMD_MODE_OFF;
		break;
	case BWN_PHYLP_TXPCTL_ON_HW:
		ctl = BWN_PHY_TX_PWR_CTL_CMD_MODE_HW;
		break;
	case BWN_PHYLP_TXPCTL_ON_SW:
		ctl = BWN_PHY_TX_PWR_CTL_CMD_MODE_SW;
		break;
	default:
		ctl = 0;
		KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	}
	BWN_PHY_SETMASK(mac, BWN_PHY_TX_PWR_CTL_CMD,
	    (uint16_t)~BWN_PHY_TX_PWR_CTL_CMD_MODE, ctl);
}

static void
bwn_phy_lp_bugfix(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	const unsigned int size = 256;
	struct bwn_txgain tg;
	uint32_t rxcomp, txgain, coeff, rfpwr, *tabs;
	uint16_t tssinpt, tssiidx, value[2];
	uint8_t mode;
	int8_t txpwridx;

	tabs = (uint32_t *)malloc(sizeof(uint32_t) * size, M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (tabs == NULL) {
		device_printf(sc->sc_dev, "failed to allocate buffer.\n");
		return;
	}

	bwn_phy_lp_get_txpctlmode(mac);
	mode = plp->plp_txpctlmode;
	txpwridx = plp->plp_txpwridx;
	tssinpt = plp->plp_tssinpt;
	tssiidx = plp->plp_tssiidx;

	bwn_tab_read_multi(mac,
	    (mac->mac_phy.rev < 2) ? BWN_TAB_4(10, 0x140) :
	    BWN_TAB_4(7, 0x140), size, tabs);

	bwn_phy_lp_tblinit(mac);
	bwn_phy_lp_bbinit(mac);
	bwn_phy_lp_txpctl_init(mac);
	bwn_phy_lp_rf_onoff(mac, 1);
	bwn_phy_lp_set_txpctlmode(mac, BWN_PHYLP_TXPCTL_OFF);

	bwn_tab_write_multi(mac,
	    (mac->mac_phy.rev < 2) ? BWN_TAB_4(10, 0x140) :
	    BWN_TAB_4(7, 0x140), size, tabs);

	BWN_WRITE_2(mac, BWN_CHANNEL, plp->plp_chan);
	plp->plp_tssinpt = tssinpt;
	plp->plp_tssiidx = tssiidx;
	bwn_phy_lp_set_anafilter(mac, plp->plp_chan);
	if (txpwridx != -1) {
		/* set TX power by index */
		plp->plp_txpwridx = txpwridx;
		bwn_phy_lp_get_txpctlmode(mac);
		if (plp->plp_txpctlmode != BWN_PHYLP_TXPCTL_OFF)
			bwn_phy_lp_set_txpctlmode(mac, BWN_PHYLP_TXPCTL_ON_SW);
		if (mac->mac_phy.rev >= 2) {
			rxcomp = bwn_tab_read(mac,
			    BWN_TAB_4(7, txpwridx + 320));
			txgain = bwn_tab_read(mac,
			    BWN_TAB_4(7, txpwridx + 192));
			tg.tg_pad = (txgain >> 16) & 0xff;
			tg.tg_gm = txgain & 0xff;
			tg.tg_pga = (txgain >> 8) & 0xff;
			tg.tg_dac = (rxcomp >> 28) & 0xff;
			bwn_phy_lp_set_txgain(mac, &tg);
		} else {
			rxcomp = bwn_tab_read(mac,
			    BWN_TAB_4(10, txpwridx + 320));
			txgain = bwn_tab_read(mac,
			    BWN_TAB_4(10, txpwridx + 192));
			BWN_PHY_SETMASK(mac, BWN_PHY_TX_GAIN_CTL_OVERRIDE_VAL,
			    0xf800, (txgain >> 4) & 0x7fff);
			bwn_phy_lp_set_txgain_dac(mac, txgain & 0x7);
			bwn_phy_lp_set_txgain_pa(mac, (txgain >> 24) & 0x7f);
		}
		bwn_phy_lp_set_bbmult(mac, (rxcomp >> 20) & 0xff);

		/* set TX IQCC */
		value[0] = (rxcomp >> 10) & 0x3ff;
		value[1] = rxcomp & 0x3ff;
		bwn_tab_write_multi(mac, BWN_TAB_2(0, 80), 2, value);

		coeff = bwn_tab_read(mac,
		    (mac->mac_phy.rev >= 2) ? BWN_TAB_4(7, txpwridx + 448) :
		    BWN_TAB_4(10, txpwridx + 448));
		bwn_tab_write(mac, BWN_TAB_2(0, 85), coeff & 0xffff);
		if (mac->mac_phy.rev >= 2) {
			rfpwr = bwn_tab_read(mac,
			    BWN_TAB_4(7, txpwridx + 576));
			BWN_PHY_SETMASK(mac, BWN_PHY_RF_PWR_OVERRIDE, 0xff00,
			    rfpwr & 0xffff);
		}
		bwn_phy_lp_set_txgain_override(mac);
	}
	if (plp->plp_rccap)
		bwn_phy_lp_set_rccap(mac);
	bwn_phy_lp_set_antenna(mac, plp->plp_antenna);
	bwn_phy_lp_set_txpctlmode(mac, mode);
	free(tabs, M_DEVBUF);
}

static void
bwn_phy_lp_digflt_restore(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	int i;
	static const uint16_t addr[] = {
		BWN_PHY_OFDM(0xc1), BWN_PHY_OFDM(0xc2),
		BWN_PHY_OFDM(0xc3), BWN_PHY_OFDM(0xc4),
		BWN_PHY_OFDM(0xc5), BWN_PHY_OFDM(0xc6),
		BWN_PHY_OFDM(0xc7), BWN_PHY_OFDM(0xc8),
		BWN_PHY_OFDM(0xcf),
	};

	for (i = 0; i < N(addr); i++)
		BWN_PHY_WRITE(mac, addr[i], plp->plp_digfilt[i]);
}

static void
bwn_phy_lp_tblinit(struct bwn_mac *mac)
{
	uint32_t freq = ieee80211_ieee2mhz(bwn_phy_lp_get_default_chan(mac), 0);

	if (mac->mac_phy.rev < 2) {
		bwn_phy_lp_tblinit_r01(mac);
		bwn_phy_lp_tblinit_txgain(mac);
		bwn_phy_lp_set_gaintbl(mac, freq);
		return;
	}

	bwn_phy_lp_tblinit_r2(mac);
	bwn_phy_lp_tblinit_txgain(mac);
}

struct bwn_wpair {
	uint16_t		reg;
	uint16_t		value;
};

struct bwn_smpair {
	uint16_t		offset;
	uint16_t		mask;
	uint16_t		set;
};

static void
bwn_phy_lp_bbinit_r2(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	static const struct bwn_wpair v1[] = {
		{ BWN_PHY_AFE_DAC_CTL, 0x50 },
		{ BWN_PHY_AFE_CTL, 0x8800 },
		{ BWN_PHY_AFE_CTL_OVR, 0 },
		{ BWN_PHY_AFE_CTL_OVRVAL, 0 },
		{ BWN_PHY_RF_OVERRIDE_0, 0 },
		{ BWN_PHY_RF_OVERRIDE_2, 0 },
		{ BWN_PHY_OFDM(0xf9), 0 },
		{ BWN_PHY_TR_LOOKUP_1, 0 }
	};
	static const struct bwn_smpair v2[] = {
		{ BWN_PHY_OFDMSYNCTHRESH0, 0xff00, 0xb4 },
		{ BWN_PHY_DCOFFSETTRANSIENT, 0xf8ff, 0x200 },
		{ BWN_PHY_DCOFFSETTRANSIENT, 0xff00, 0x7f },
		{ BWN_PHY_GAINDIRECTMISMATCH, 0xff0f, 0x40 },
		{ BWN_PHY_PREAMBLECONFIRMTO, 0xff00, 0x2 }
	};
	static const struct bwn_smpair v3[] = {
		{ BWN_PHY_OFDM(0xfe), 0xffe0, 0x1f },
		{ BWN_PHY_OFDM(0xff), 0xffe0, 0xc },
		{ BWN_PHY_OFDM(0x100), 0xff00, 0x19 },
		{ BWN_PHY_OFDM(0xff), 0x03ff, 0x3c00 },
		{ BWN_PHY_OFDM(0xfe), 0xfc1f, 0x3e0 },
		{ BWN_PHY_OFDM(0xff), 0xffe0, 0xc },
		{ BWN_PHY_OFDM(0x100), 0x00ff, 0x1900 },
		{ BWN_PHY_CLIPCTRTHRESH, 0x83ff, 0x5800 },
		{ BWN_PHY_CLIPCTRTHRESH, 0xffe0, 0x12 },
		{ BWN_PHY_GAINMISMATCH, 0x0fff, 0x9000 },

	};
	int i;

	for (i = 0; i < N(v1); i++)
		BWN_PHY_WRITE(mac, v1[i].reg, v1[i].value);
	BWN_PHY_SET(mac, BWN_PHY_ADC_COMPENSATION_CTL, 0x10);
	for (i = 0; i < N(v2); i++)
		BWN_PHY_SETMASK(mac, v2[i].offset, v2[i].mask, v2[i].set);

	BWN_PHY_MASK(mac, BWN_PHY_CRSGAIN_CTL, ~0x4000);
	BWN_PHY_MASK(mac, BWN_PHY_CRSGAIN_CTL, ~0x2000);
	BWN_PHY_SET(mac, BWN_PHY_OFDM(0x10a), 0x1);
	if (siba_get_pci_revid(sc->sc_dev) >= 0x18) {
		bwn_tab_write(mac, BWN_TAB_4(17, 65), 0xec);
		BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x10a), 0xff01, 0x14);
	} else {
		BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0x10a), 0xff01, 0x10);
	}
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0xdf), 0xff00, 0xf4);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0xdf), 0x00ff, 0xf100);
	BWN_PHY_WRITE(mac, BWN_PHY_CLIPTHRESH, 0x48);
	BWN_PHY_SETMASK(mac, BWN_PHY_HIGAINDB, 0xff00, 0x46);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0xe4), 0xff00, 0x10);
	BWN_PHY_SETMASK(mac, BWN_PHY_PWR_THRESH1, 0xfff0, 0x9);
	BWN_PHY_MASK(mac, BWN_PHY_GAINDIRECTMISMATCH, ~0xf);
	BWN_PHY_SETMASK(mac, BWN_PHY_VERYLOWGAINDB, 0x00ff, 0x5500);
	BWN_PHY_SETMASK(mac, BWN_PHY_CLIPCTRTHRESH, 0xfc1f, 0xa0);
	BWN_PHY_SETMASK(mac, BWN_PHY_GAINDIRECTMISMATCH, 0xe0ff, 0x300);
	BWN_PHY_SETMASK(mac, BWN_PHY_HIGAINDB, 0x00ff, 0x2a00);
	if ((siba_get_chipid(sc->sc_dev) == 0x4325) &&
	    (siba_get_chiprev(sc->sc_dev) == 0)) {
		BWN_PHY_SETMASK(mac, BWN_PHY_LOWGAINDB, 0x00ff, 0x2100);
		BWN_PHY_SETMASK(mac, BWN_PHY_VERYLOWGAINDB, 0xff00, 0xa);
	} else {
		BWN_PHY_SETMASK(mac, BWN_PHY_LOWGAINDB, 0x00ff, 0x1e00);
		BWN_PHY_SETMASK(mac, BWN_PHY_VERYLOWGAINDB, 0xff00, 0xd);
	}
	for (i = 0; i < N(v3); i++)
		BWN_PHY_SETMASK(mac, v3[i].offset, v3[i].mask, v3[i].set);
	if ((siba_get_chipid(sc->sc_dev) == 0x4325) &&
	    (siba_get_chiprev(sc->sc_dev) == 0)) {
		bwn_tab_write(mac, BWN_TAB_2(0x08, 0x14), 0);
		bwn_tab_write(mac, BWN_TAB_2(0x08, 0x12), 0x40);
	}

	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
		BWN_PHY_SET(mac, BWN_PHY_CRSGAIN_CTL, 0x40);
		BWN_PHY_SETMASK(mac, BWN_PHY_CRSGAIN_CTL, 0xf0ff, 0xb00);
		BWN_PHY_SETMASK(mac, BWN_PHY_SYNCPEAKCNT, 0xfff8, 0x6);
		BWN_PHY_SETMASK(mac, BWN_PHY_MINPWR_LEVEL, 0x00ff, 0x9d00);
		BWN_PHY_SETMASK(mac, BWN_PHY_MINPWR_LEVEL, 0xff00, 0xa1);
		BWN_PHY_MASK(mac, BWN_PHY_IDLEAFTERPKTRXTO, 0x00ff);
	} else
		BWN_PHY_MASK(mac, BWN_PHY_CRSGAIN_CTL, ~0x40);

	BWN_PHY_SETMASK(mac, BWN_PHY_CRS_ED_THRESH, 0xff00, 0xb3);
	BWN_PHY_SETMASK(mac, BWN_PHY_CRS_ED_THRESH, 0x00ff, 0xad00);
	BWN_PHY_SETMASK(mac, BWN_PHY_INPUT_PWRDB, 0xff00, plp->plp_rxpwroffset);
	BWN_PHY_SET(mac, BWN_PHY_RESET_CTL, 0x44);
	BWN_PHY_WRITE(mac, BWN_PHY_RESET_CTL, 0x80);
	BWN_PHY_WRITE(mac, BWN_PHY_AFE_RSSI_CTL_0, 0xa954);
	BWN_PHY_WRITE(mac, BWN_PHY_AFE_RSSI_CTL_1,
	    0x2000 | ((uint16_t)plp->plp_rssigs << 10) |
	    ((uint16_t)plp->plp_rssivc << 4) | plp->plp_rssivf);

	if ((siba_get_chipid(sc->sc_dev) == 0x4325) &&
	    (siba_get_chiprev(sc->sc_dev) == 0)) {
		BWN_PHY_SET(mac, BWN_PHY_AFE_ADC_CTL_0, 0x1c);
		BWN_PHY_SETMASK(mac, BWN_PHY_AFE_CTL, 0x00ff, 0x8800);
		BWN_PHY_SETMASK(mac, BWN_PHY_AFE_ADC_CTL_1, 0xfc3c, 0x0400);
	}

	bwn_phy_lp_digflt_save(mac);
}

static void
bwn_phy_lp_bbinit_r01(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	static const struct bwn_smpair v1[] = {
		{ BWN_PHY_CLIPCTRTHRESH, 0xffe0, 0x0005 },
		{ BWN_PHY_CLIPCTRTHRESH, 0xfc1f, 0x0180 },
		{ BWN_PHY_CLIPCTRTHRESH, 0x83ff, 0x3c00 },
		{ BWN_PHY_GAINDIRECTMISMATCH, 0xfff0, 0x0005 },
		{ BWN_PHY_GAIN_MISMATCH_LIMIT, 0xffc0, 0x001a },
		{ BWN_PHY_CRS_ED_THRESH, 0xff00, 0x00b3 },
		{ BWN_PHY_CRS_ED_THRESH, 0x00ff, 0xad00 }
	};
	static const struct bwn_smpair v2[] = {
		{ BWN_PHY_TR_LOOKUP_1, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_1, 0x3f00, 0x0900 },
		{ BWN_PHY_TR_LOOKUP_2, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_2, 0xc0ff, 0x0b00 },
		{ BWN_PHY_TR_LOOKUP_3, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_3, 0xc0ff, 0x0400 },
		{ BWN_PHY_TR_LOOKUP_4, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_4, 0xc0ff, 0x0b00 },
		{ BWN_PHY_TR_LOOKUP_5, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_5, 0xc0ff, 0x0900 },
		{ BWN_PHY_TR_LOOKUP_6, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_6, 0xc0ff, 0x0b00 },
		{ BWN_PHY_TR_LOOKUP_7, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_7, 0xc0ff, 0x0900 },
		{ BWN_PHY_TR_LOOKUP_8, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_8, 0xc0ff, 0x0b00 }
	};
	static const struct bwn_smpair v3[] = {
		{ BWN_PHY_TR_LOOKUP_1, 0xffc0, 0x0001 },
		{ BWN_PHY_TR_LOOKUP_1, 0xc0ff, 0x0400 },
		{ BWN_PHY_TR_LOOKUP_2, 0xffc0, 0x0001 },
		{ BWN_PHY_TR_LOOKUP_2, 0xc0ff, 0x0500 },
		{ BWN_PHY_TR_LOOKUP_3, 0xffc0, 0x0002 },
		{ BWN_PHY_TR_LOOKUP_3, 0xc0ff, 0x0800 },
		{ BWN_PHY_TR_LOOKUP_4, 0xffc0, 0x0002 },
		{ BWN_PHY_TR_LOOKUP_4, 0xc0ff, 0x0a00 }
	};
	static const struct bwn_smpair v4[] = {
		{ BWN_PHY_TR_LOOKUP_1, 0xffc0, 0x0004 },
		{ BWN_PHY_TR_LOOKUP_1, 0xc0ff, 0x0800 },
		{ BWN_PHY_TR_LOOKUP_2, 0xffc0, 0x0004 },
		{ BWN_PHY_TR_LOOKUP_2, 0xc0ff, 0x0c00 },
		{ BWN_PHY_TR_LOOKUP_3, 0xffc0, 0x0002 },
		{ BWN_PHY_TR_LOOKUP_3, 0xc0ff, 0x0100 },
		{ BWN_PHY_TR_LOOKUP_4, 0xffc0, 0x0002 },
		{ BWN_PHY_TR_LOOKUP_4, 0xc0ff, 0x0300 }
	};
	static const struct bwn_smpair v5[] = {
		{ BWN_PHY_TR_LOOKUP_1, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_1, 0xc0ff, 0x0900 },
		{ BWN_PHY_TR_LOOKUP_2, 0xffc0, 0x000a },
		{ BWN_PHY_TR_LOOKUP_2, 0xc0ff, 0x0b00 },
		{ BWN_PHY_TR_LOOKUP_3, 0xffc0, 0x0006 },
		{ BWN_PHY_TR_LOOKUP_3, 0xc0ff, 0x0500 },
		{ BWN_PHY_TR_LOOKUP_4, 0xffc0, 0x0006 },
		{ BWN_PHY_TR_LOOKUP_4, 0xc0ff, 0x0700 }
	};
	int i;
	uint16_t tmp, tmp2;

	BWN_PHY_MASK(mac, BWN_PHY_AFE_DAC_CTL, 0xf7ff);
	BWN_PHY_WRITE(mac, BWN_PHY_AFE_CTL, 0);
	BWN_PHY_WRITE(mac, BWN_PHY_AFE_CTL_OVR, 0);
	BWN_PHY_WRITE(mac, BWN_PHY_RF_OVERRIDE_0, 0);
	BWN_PHY_WRITE(mac, BWN_PHY_RF_OVERRIDE_2, 0);
	BWN_PHY_SET(mac, BWN_PHY_AFE_DAC_CTL, 0x0004);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDMSYNCTHRESH0, 0xff00, 0x0078);
	BWN_PHY_SETMASK(mac, BWN_PHY_CLIPCTRTHRESH, 0x83ff, 0x5800);
	BWN_PHY_WRITE(mac, BWN_PHY_ADC_COMPENSATION_CTL, 0x0016);
	BWN_PHY_SETMASK(mac, BWN_PHY_AFE_ADC_CTL_0, 0xfff8, 0x0004);
	BWN_PHY_SETMASK(mac, BWN_PHY_VERYLOWGAINDB, 0x00ff, 0x5400);
	BWN_PHY_SETMASK(mac, BWN_PHY_HIGAINDB, 0x00ff, 0x2400);
	BWN_PHY_SETMASK(mac, BWN_PHY_LOWGAINDB, 0x00ff, 0x2100);
	BWN_PHY_SETMASK(mac, BWN_PHY_VERYLOWGAINDB, 0xff00, 0x0006);
	BWN_PHY_MASK(mac, BWN_PHY_RX_RADIO_CTL, 0xfffe);
	for (i = 0; i < N(v1); i++)
		BWN_PHY_SETMASK(mac, v1[i].offset, v1[i].mask, v1[i].set);
	BWN_PHY_SETMASK(mac, BWN_PHY_INPUT_PWRDB,
	    0xff00, plp->plp_rxpwroffset);
	if ((siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_FEM) &&
	    ((IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) ||
	   (siba_sprom_get_bf_hi(sc->sc_dev) & BWN_BFH_LDO_PAREF))) {
		siba_cc_pmu_set_ldovolt(sc->sc_dev, SIBA_LDO_PAREF, 0x28);
		siba_cc_pmu_set_ldoparef(sc->sc_dev, 1);
		if (mac->mac_phy.rev == 0)
			BWN_PHY_SETMASK(mac, BWN_PHY_LP_RF_SIGNAL_LUT,
			    0xffcf, 0x0010);
		bwn_tab_write(mac, BWN_TAB_2(11, 7), 60);
	} else {
		siba_cc_pmu_set_ldoparef(sc->sc_dev, 0);
		BWN_PHY_SETMASK(mac, BWN_PHY_LP_RF_SIGNAL_LUT, 0xffcf, 0x0020);
		bwn_tab_write(mac, BWN_TAB_2(11, 7), 100);
	}
	tmp = plp->plp_rssivf | plp->plp_rssivc << 4 | 0xa000;
	BWN_PHY_WRITE(mac, BWN_PHY_AFE_RSSI_CTL_0, tmp);
	if (siba_sprom_get_bf_hi(sc->sc_dev) & BWN_BFH_RSSIINV)
		BWN_PHY_SETMASK(mac, BWN_PHY_AFE_RSSI_CTL_1, 0xf000, 0x0aaa);
	else
		BWN_PHY_SETMASK(mac, BWN_PHY_AFE_RSSI_CTL_1, 0xf000, 0x02aa);
	bwn_tab_write(mac, BWN_TAB_2(11, 1), 24);
	BWN_PHY_SETMASK(mac, BWN_PHY_RX_RADIO_CTL,
	    0xfff9, (plp->plp_bxarch << 1));
	if (mac->mac_phy.rev == 1 &&
	    (siba_sprom_get_bf_hi(sc->sc_dev) & BWN_BFH_FEM_BT)) {
		for (i = 0; i < N(v2); i++)
			BWN_PHY_SETMASK(mac, v2[i].offset, v2[i].mask,
			    v2[i].set);
	} else if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan) ||
	    (siba_get_pci_subdevice(sc->sc_dev) == 0x048a) ||
	    ((mac->mac_phy.rev == 0) &&
	     (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_FEM))) {
		for (i = 0; i < N(v3); i++)
			BWN_PHY_SETMASK(mac, v3[i].offset, v3[i].mask,
			    v3[i].set);
	} else if (mac->mac_phy.rev == 1 ||
		  (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_FEM)) {
		for (i = 0; i < N(v4); i++)
			BWN_PHY_SETMASK(mac, v4[i].offset, v4[i].mask,
			    v4[i].set);
	} else {
		for (i = 0; i < N(v5); i++)
			BWN_PHY_SETMASK(mac, v5[i].offset, v5[i].mask,
			    v5[i].set);
	}
	if (mac->mac_phy.rev == 1 &&
	    (siba_sprom_get_bf_hi(sc->sc_dev) & BWN_BFH_LDO_PAREF)) {
		BWN_PHY_COPY(mac, BWN_PHY_TR_LOOKUP_5, BWN_PHY_TR_LOOKUP_1);
		BWN_PHY_COPY(mac, BWN_PHY_TR_LOOKUP_6, BWN_PHY_TR_LOOKUP_2);
		BWN_PHY_COPY(mac, BWN_PHY_TR_LOOKUP_7, BWN_PHY_TR_LOOKUP_3);
		BWN_PHY_COPY(mac, BWN_PHY_TR_LOOKUP_8, BWN_PHY_TR_LOOKUP_4);
	}
	if ((siba_sprom_get_bf_hi(sc->sc_dev) & BWN_BFH_FEM_BT) &&
	    (siba_get_chipid(sc->sc_dev) == 0x5354) &&
	    (siba_get_chippkg(sc->sc_dev) == SIBA_CHIPPACK_BCM4712S)) {
		BWN_PHY_SET(mac, BWN_PHY_CRSGAIN_CTL, 0x0006);
		BWN_PHY_WRITE(mac, BWN_PHY_GPIO_SELECT, 0x0005);
		BWN_PHY_WRITE(mac, BWN_PHY_GPIO_OUTEN, 0xffff);
		bwn_hf_write(mac, bwn_hf_read(mac) | BWN_HF_PR45960W);
	}
	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
		BWN_PHY_SET(mac, BWN_PHY_LP_PHY_CTL, 0x8000);
		BWN_PHY_SET(mac, BWN_PHY_CRSGAIN_CTL, 0x0040);
		BWN_PHY_SETMASK(mac, BWN_PHY_MINPWR_LEVEL, 0x00ff, 0xa400);
		BWN_PHY_SETMASK(mac, BWN_PHY_CRSGAIN_CTL, 0xf0ff, 0x0b00);
		BWN_PHY_SETMASK(mac, BWN_PHY_SYNCPEAKCNT, 0xfff8, 0x0007);
		BWN_PHY_SETMASK(mac, BWN_PHY_DSSS_CONFIRM_CNT, 0xfff8, 0x0003);
		BWN_PHY_SETMASK(mac, BWN_PHY_DSSS_CONFIRM_CNT, 0xffc7, 0x0020);
		BWN_PHY_MASK(mac, BWN_PHY_IDLEAFTERPKTRXTO, 0x00ff);
	} else {
		BWN_PHY_MASK(mac, BWN_PHY_LP_PHY_CTL, 0x7fff);
		BWN_PHY_MASK(mac, BWN_PHY_CRSGAIN_CTL, 0xffbf);
	}
	if (mac->mac_phy.rev == 1) {
		tmp = BWN_PHY_READ(mac, BWN_PHY_CLIPCTRTHRESH);
		tmp2 = (tmp & 0x03e0) >> 5;
		tmp2 |= tmp2 << 5;
		BWN_PHY_WRITE(mac, BWN_PHY_4C3, tmp2);
		tmp = BWN_PHY_READ(mac, BWN_PHY_GAINDIRECTMISMATCH);
		tmp2 = (tmp & 0x1f00) >> 8;
		tmp2 |= tmp2 << 5;
		BWN_PHY_WRITE(mac, BWN_PHY_4C4, tmp2);
		tmp = BWN_PHY_READ(mac, BWN_PHY_VERYLOWGAINDB);
		tmp2 = tmp & 0x00ff;
		tmp2 |= tmp << 8;
		BWN_PHY_WRITE(mac, BWN_PHY_4C5, tmp2);
	}
}

struct bwn_b2062_freq {
	uint16_t		freq;
	uint8_t			value[6];
};

static void
bwn_phy_lp_b2062_init(struct bwn_mac *mac)
{
#define	CALC_CTL7(freq, div)						\
	(((800000000 * (div) + (freq)) / (2 * (freq)) - 8) & 0xff)
#define	CALC_CTL18(freq, div)						\
	((((100 * (freq) + 16000000 * (div)) / (32000000 * (div))) - 1) & 0xff)
#define	CALC_CTL19(freq, div)						\
	((((2 * (freq) + 1000000 * (div)) / (2000000 * (div))) - 1) & 0xff)
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	static const struct bwn_b2062_freq freqdata_tab[] = {
		{ 12000, { 6, 6, 6, 6, 10, 6 } },
		{ 13000, { 4, 4, 4, 4, 11, 7 } },
		{ 14400, { 3, 3, 3, 3, 12, 7 } },
		{ 16200, { 3, 3, 3, 3, 13, 8 } },
		{ 18000, { 2, 2, 2, 2, 14, 8 } },
		{ 19200, { 1, 1, 1, 1, 14, 9 } }
	};
	static const struct bwn_wpair v1[] = {
		{ BWN_B2062_N_TXCTL3, 0 },
		{ BWN_B2062_N_TXCTL4, 0 },
		{ BWN_B2062_N_TXCTL5, 0 },
		{ BWN_B2062_N_TXCTL6, 0 },
		{ BWN_B2062_N_PDNCTL0, 0x40 },
		{ BWN_B2062_N_PDNCTL0, 0 },
		{ BWN_B2062_N_CALIB_TS, 0x10 },
		{ BWN_B2062_N_CALIB_TS, 0 }
	};
	const struct bwn_b2062_freq *f = NULL;
	uint32_t xtalfreq, ref;
	unsigned int i;

	bwn_phy_lp_b2062_tblinit(mac);

	for (i = 0; i < N(v1); i++)
		BWN_RF_WRITE(mac, v1[i].reg, v1[i].value);
	if (mac->mac_phy.rev > 0)
		BWN_RF_WRITE(mac, BWN_B2062_S_BG_CTL1,
		    (BWN_RF_READ(mac, BWN_B2062_N_COM2) >> 1) | 0x80);
	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
		BWN_RF_SET(mac, BWN_B2062_N_TSSI_CTL0, 0x1);
	else
		BWN_RF_MASK(mac, BWN_B2062_N_TSSI_CTL0, ~0x1);

	KASSERT(siba_get_cc_caps(sc->sc_dev) & SIBA_CC_CAPS_PMU,
	    ("%s:%d: fail", __func__, __LINE__));
	xtalfreq = siba_get_cc_pmufreq(sc->sc_dev) * 1000;
	KASSERT(xtalfreq != 0, ("%s:%d: fail", __func__, __LINE__));

	if (xtalfreq <= 30000000) {
		plp->plp_div = 1;
		BWN_RF_MASK(mac, BWN_B2062_S_RFPLLCTL1, 0xfffb);
	} else {
		plp->plp_div = 2;
		BWN_RF_SET(mac, BWN_B2062_S_RFPLLCTL1, 0x4);
	}

	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL7,
	    CALC_CTL7(xtalfreq, plp->plp_div));
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL18,
	    CALC_CTL18(xtalfreq, plp->plp_div));
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL19,
	    CALC_CTL19(xtalfreq, plp->plp_div));

	ref = (1000 * plp->plp_div + 2 * xtalfreq) / (2000 * plp->plp_div);
	ref &= 0xffff;
	for (i = 0; i < N(freqdata_tab); i++) {
		if (ref < freqdata_tab[i].freq) {
			f = &freqdata_tab[i];
			break;
		}
	}
	if (f == NULL)
		f = &freqdata_tab[N(freqdata_tab) - 1];
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL8,
	    ((uint16_t)(f->value[1]) << 4) | f->value[0]);
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL9,
	    ((uint16_t)(f->value[3]) << 4) | f->value[2]);
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL10, f->value[4]);
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL11, f->value[5]);
#undef CALC_CTL7
#undef CALC_CTL18
#undef CALC_CTL19
}

static void
bwn_phy_lp_b2063_init(struct bwn_mac *mac)
{

	bwn_phy_lp_b2063_tblinit(mac);
	BWN_RF_WRITE(mac, BWN_B2063_LOGEN_SP5, 0);
	BWN_RF_SET(mac, BWN_B2063_COM8, 0x38);
	BWN_RF_WRITE(mac, BWN_B2063_REG_SP1, 0x56);
	BWN_RF_MASK(mac, BWN_B2063_RX_BB_CTL2, ~0x2);
	BWN_RF_WRITE(mac, BWN_B2063_PA_SP7, 0);
	BWN_RF_WRITE(mac, BWN_B2063_TX_RF_SP6, 0x20);
	BWN_RF_WRITE(mac, BWN_B2063_TX_RF_SP9, 0x40);
	if (mac->mac_phy.rev == 2) {
		BWN_RF_WRITE(mac, BWN_B2063_PA_SP3, 0xa0);
		BWN_RF_WRITE(mac, BWN_B2063_PA_SP4, 0xa0);
		BWN_RF_WRITE(mac, BWN_B2063_PA_SP2, 0x18);
	} else {
		BWN_RF_WRITE(mac, BWN_B2063_PA_SP3, 0x20);
		BWN_RF_WRITE(mac, BWN_B2063_PA_SP2, 0x20);
	}
}

static void
bwn_phy_lp_rxcal_r2(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	static const struct bwn_wpair v1[] = {
		{ BWN_B2063_RX_BB_SP8, 0x0 },
		{ BWN_B2063_RC_CALIB_CTL1, 0x7e },
		{ BWN_B2063_RC_CALIB_CTL1, 0x7c },
		{ BWN_B2063_RC_CALIB_CTL2, 0x15 },
		{ BWN_B2063_RC_CALIB_CTL3, 0x70 },
		{ BWN_B2063_RC_CALIB_CTL4, 0x52 },
		{ BWN_B2063_RC_CALIB_CTL5, 0x1 },
		{ BWN_B2063_RC_CALIB_CTL1, 0x7d }
	};
	static const struct bwn_wpair v2[] = {
		{ BWN_B2063_TX_BB_SP3, 0x0 },
		{ BWN_B2063_RC_CALIB_CTL1, 0x7e },
		{ BWN_B2063_RC_CALIB_CTL1, 0x7c },
		{ BWN_B2063_RC_CALIB_CTL2, 0x55 },
		{ BWN_B2063_RC_CALIB_CTL3, 0x76 }
	};
	uint32_t freqxtal = siba_get_cc_pmufreq(sc->sc_dev) * 1000;
	int i;
	uint8_t tmp;

	tmp = BWN_RF_READ(mac, BWN_B2063_RX_BB_SP8) & 0xff;

	for (i = 0; i < 2; i++)
		BWN_RF_WRITE(mac, v1[i].reg, v1[i].value);
	BWN_RF_MASK(mac, BWN_B2063_PLL_SP1, 0xf7);
	for (i = 2; i < N(v1); i++)
		BWN_RF_WRITE(mac, v1[i].reg, v1[i].value);
	for (i = 0; i < 10000; i++) {
		if (BWN_RF_READ(mac, BWN_B2063_RC_CALIB_CTL6) & 0x2)
			break;
		DELAY(1000);
	}

	if (!(BWN_RF_READ(mac, BWN_B2063_RC_CALIB_CTL6) & 0x2))
		BWN_RF_WRITE(mac, BWN_B2063_RX_BB_SP8, tmp);

	tmp = BWN_RF_READ(mac, BWN_B2063_TX_BB_SP3) & 0xff;

	for (i = 0; i < N(v2); i++)
		BWN_RF_WRITE(mac, v2[i].reg, v2[i].value);
	if (freqxtal == 24000000) {
		BWN_RF_WRITE(mac, BWN_B2063_RC_CALIB_CTL4, 0xfc);
		BWN_RF_WRITE(mac, BWN_B2063_RC_CALIB_CTL5, 0x0);
	} else {
		BWN_RF_WRITE(mac, BWN_B2063_RC_CALIB_CTL4, 0x13);
		BWN_RF_WRITE(mac, BWN_B2063_RC_CALIB_CTL5, 0x1);
	}
	BWN_RF_WRITE(mac, BWN_B2063_PA_SP7, 0x7d);
	for (i = 0; i < 10000; i++) {
		if (BWN_RF_READ(mac, BWN_B2063_RC_CALIB_CTL6) & 0x2)
			break;
		DELAY(1000);
	}
	if (!(BWN_RF_READ(mac, BWN_B2063_RC_CALIB_CTL6) & 0x2))
		BWN_RF_WRITE(mac, BWN_B2063_TX_BB_SP3, tmp);
	BWN_RF_WRITE(mac, BWN_B2063_RC_CALIB_CTL1, 0x7e);
}

static void
bwn_phy_lp_rccal_r12(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	struct bwn_phy_lp_iq_est ie;
	struct bwn_txgain tx_gains;
	static const uint32_t pwrtbl[21] = {
		0x10000, 0x10557, 0x10e2d, 0x113e0, 0x10f22, 0x0ff64,
		0x0eda2, 0x0e5d4, 0x0efd1, 0x0fbe8, 0x0b7b8, 0x04b35,
		0x01a5e, 0x00a0b, 0x00444, 0x001fd, 0x000ff, 0x00088,
		0x0004c, 0x0002c, 0x0001a,
	};
	uint32_t npwr, ipwr, sqpwr, tmp;
	int loopback, i, j, sum, error;
	uint16_t save[7];
	uint8_t txo, bbmult, txpctlmode;

	error = bwn_phy_lp_switch_channel(mac, 7);
	if (error)
		device_printf(sc->sc_dev,
		    "failed to change channel to 7 (%d)\n", error);
	txo = (BWN_PHY_READ(mac, BWN_PHY_AFE_CTL_OVR) & 0x40) ? 1 : 0;
	bbmult = bwn_phy_lp_get_bbmult(mac);
	if (txo)
		tx_gains = bwn_phy_lp_get_txgain(mac);

	save[0] = BWN_PHY_READ(mac, BWN_PHY_RF_OVERRIDE_0);
	save[1] = BWN_PHY_READ(mac, BWN_PHY_RF_OVERRIDE_VAL_0);
	save[2] = BWN_PHY_READ(mac, BWN_PHY_AFE_CTL_OVR);
	save[3] = BWN_PHY_READ(mac, BWN_PHY_AFE_CTL_OVRVAL);
	save[4] = BWN_PHY_READ(mac, BWN_PHY_RF_OVERRIDE_2);
	save[5] = BWN_PHY_READ(mac, BWN_PHY_RF_OVERRIDE_2_VAL);
	save[6] = BWN_PHY_READ(mac, BWN_PHY_LP_PHY_CTL);

	bwn_phy_lp_get_txpctlmode(mac);
	txpctlmode = plp->plp_txpctlmode;
	bwn_phy_lp_set_txpctlmode(mac, BWN_PHYLP_TXPCTL_OFF);

	/* disable CRS */
	bwn_phy_lp_set_deaf(mac, 1);
	bwn_phy_lp_set_trsw_over(mac, 0, 1);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xfffb);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x4);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xfff7);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x8);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0x10);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x10);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xffdf);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x20);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xffbf);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x40);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0x7);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0x38);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0xff3f);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0x100);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0xfdff);
	BWN_PHY_WRITE(mac, BWN_PHY_PS_CTL_OVERRIDE_VAL0, 0);
	BWN_PHY_WRITE(mac, BWN_PHY_PS_CTL_OVERRIDE_VAL1, 1);
	BWN_PHY_WRITE(mac, BWN_PHY_PS_CTL_OVERRIDE_VAL2, 0x20);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0xfbff);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0xf7ff);
	BWN_PHY_WRITE(mac, BWN_PHY_TX_GAIN_CTL_OVERRIDE_VAL, 0);
	BWN_PHY_WRITE(mac, BWN_PHY_RX_GAIN_CTL_OVERRIDE_VAL, 0x45af);
	BWN_PHY_WRITE(mac, BWN_PHY_RF_OVERRIDE_2, 0x3ff);

	loopback = bwn_phy_lp_loopback(mac);
	if (loopback == -1)
		goto done;
	bwn_phy_lp_set_rxgain_idx(mac, loopback);
	BWN_PHY_SETMASK(mac, BWN_PHY_LP_PHY_CTL, 0xffbf, 0x40);
	BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0xfff8, 0x1);
	BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0xffc7, 0x8);
	BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL, 0xff3f, 0xc0);

	tmp = 0;
	memset(&ie, 0, sizeof(ie));
	for (i = 128; i <= 159; i++) {
		BWN_RF_WRITE(mac, BWN_B2062_N_RXBB_CALIB2, i);
		sum = 0;
		for (j = 5; j <= 25; j++) {
			bwn_phy_lp_ddfs_turnon(mac, 1, 1, j, j, 0);
			if (!(bwn_phy_lp_rx_iq_est(mac, 1000, 32, &ie)))
				goto done;
			sqpwr = ie.ie_ipwr + ie.ie_qpwr;
			ipwr = ((pwrtbl[j - 5] >> 3) + 1) >> 1;
			npwr = bwn_phy_lp_roundup(sqpwr, (j == 5) ? sqpwr : 0,
			    12);
			sum += ((ipwr - npwr) * (ipwr - npwr));
			if ((i == 128) || (sum < tmp)) {
				plp->plp_rccap = i;
				tmp = sum;
			}
		}
	}
	bwn_phy_lp_ddfs_turnoff(mac);
done:
	/* restore CRS */
	bwn_phy_lp_clear_deaf(mac, 1);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_0, 0xff80);
	BWN_PHY_MASK(mac, BWN_PHY_RF_OVERRIDE_2, 0xfc00);

	BWN_PHY_WRITE(mac, BWN_PHY_RF_OVERRIDE_VAL_0, save[1]);
	BWN_PHY_WRITE(mac, BWN_PHY_RF_OVERRIDE_0, save[0]);
	BWN_PHY_WRITE(mac, BWN_PHY_AFE_CTL_OVRVAL, save[3]);
	BWN_PHY_WRITE(mac, BWN_PHY_AFE_CTL_OVR, save[2]);
	BWN_PHY_WRITE(mac, BWN_PHY_RF_OVERRIDE_2_VAL, save[5]);
	BWN_PHY_WRITE(mac, BWN_PHY_RF_OVERRIDE_2, save[4]);
	BWN_PHY_WRITE(mac, BWN_PHY_LP_PHY_CTL, save[6]);

	bwn_phy_lp_set_bbmult(mac, bbmult);
	if (txo)
		bwn_phy_lp_set_txgain(mac, &tx_gains);
	bwn_phy_lp_set_txpctlmode(mac, txpctlmode);
	if (plp->plp_rccap)
		bwn_phy_lp_set_rccap(mac);
}

static void
bwn_phy_lp_set_rccap(struct bwn_mac *mac)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	uint8_t rc_cap = (plp->plp_rccap & 0x1f) >> 1;

	if (mac->mac_phy.rev == 1)
		rc_cap = MIN(rc_cap + 5, 15);

	BWN_RF_WRITE(mac, BWN_B2062_N_RXBB_CALIB2,
	    MAX(plp->plp_rccap - 4, 0x80));
	BWN_RF_WRITE(mac, BWN_B2062_N_TXCTL_A, rc_cap | 0x80);
	BWN_RF_WRITE(mac, BWN_B2062_S_RXG_CNT16,
	    ((plp->plp_rccap & 0x1f) >> 2) | 0x80);
}

static uint32_t
bwn_phy_lp_roundup(uint32_t value, uint32_t div, uint8_t pre)
{
	uint32_t i, q, r;

	if (div == 0)
		return (0);

	for (i = 0, q = value / div, r = value % div; i < pre; i++) {
		q <<= 1;
		if (r << 1 >= div) {
			q++;
			r = (r << 1) - div;
		}
	}
	if (r << 1 >= div)
		q++;
	return (q);
}

static void
bwn_phy_lp_b2062_reset_pllbias(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;

	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL2, 0xff);
	DELAY(20);
	if (siba_get_chipid(sc->sc_dev) == 0x5354) {
		BWN_RF_WRITE(mac, BWN_B2062_N_COM1, 4);
		BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL2, 4);
	} else {
		BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL2, 0);
	}
	DELAY(5);
}

static void
bwn_phy_lp_b2062_vco_calib(struct bwn_mac *mac)
{

	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL21, 0x42);
	BWN_RF_WRITE(mac, BWN_B2062_S_RFPLLCTL21, 0x62);
	DELAY(200);
}

static void
bwn_phy_lp_b2062_tblinit(struct bwn_mac *mac)
{
#define	FLAG_A	0x01
#define	FLAG_G	0x02
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	static const struct bwn_b206x_rfinit_entry bwn_b2062_init_tab[] = {
		{ BWN_B2062_N_COM4, 0x1, 0x0, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_PDNCTL1, 0x0, 0xca, FLAG_G, },
		{ BWN_B2062_N_PDNCTL3, 0x0, 0x0, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_PDNCTL4, 0x15, 0x2a, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_LGENC, 0xDB, 0xff, FLAG_A, },
		{ BWN_B2062_N_LGENATUNE0, 0xdd, 0x0, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_LGENATUNE2, 0xdd, 0x0, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_LGENATUNE3, 0x77, 0xB5, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_LGENACTL3, 0x0, 0xff, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_LGENACTL7, 0x33, 0x33, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_RXA_CTL1, 0x0, 0x0, FLAG_G, },
		{ BWN_B2062_N_RXBB_CTL0, 0x82, 0x80, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_RXBB_GAIN1, 0x4, 0x4, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_RXBB_GAIN2, 0x0, 0x0, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_TXCTL4, 0x3, 0x3, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_TXCTL5, 0x2, 0x2, FLAG_A | FLAG_G, },
		{ BWN_B2062_N_TX_TUNE, 0x88, 0x1b, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_COM4, 0x1, 0x0, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_PDS_CTL0, 0xff, 0xff, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_LGENG_CTL0, 0xf8, 0xd8, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_LGENG_CTL1, 0x3c, 0x24, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_LGENG_CTL8, 0x88, 0x80, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_LGENG_CTL10, 0x88, 0x80, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL0, 0x98, 0x98, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL1, 0x10, 0x10, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL5, 0x43, 0x43, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL6, 0x47, 0x47, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL7, 0xc, 0xc, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL8, 0x11, 0x11, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL9, 0x11, 0x11, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL10, 0xe, 0xe, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL11, 0x8, 0x8, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL12, 0x33, 0x33, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL13, 0xa, 0xa, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL14, 0x6, 0x6, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL18, 0x3e, 0x3e, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL19, 0x13, 0x13, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL21, 0x62, 0x62, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL22, 0x7, 0x7, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL23, 0x16, 0x16, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL24, 0x5c, 0x5c, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL25, 0x95, 0x95, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL30, 0xa0, 0xa0, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL31, 0x4, 0x4, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL33, 0xcc, 0xcc, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RFPLLCTL34, 0x7, 0x7, FLAG_A | FLAG_G, },
		{ BWN_B2062_S_RXG_CNT8, 0xf, 0xf, FLAG_A, },
	};
	const struct bwn_b206x_rfinit_entry *br;
	unsigned int i;

	for (i = 0; i < N(bwn_b2062_init_tab); i++) {
		br = &bwn_b2062_init_tab[i];
		if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
			if (br->br_flags & FLAG_G)
				BWN_RF_WRITE(mac, br->br_offset, br->br_valueg);
		} else {
			if (br->br_flags & FLAG_A)
				BWN_RF_WRITE(mac, br->br_offset, br->br_valuea);
		}
	}
#undef FLAG_A
#undef FLAG_B
}

static void
bwn_phy_lp_b2063_tblinit(struct bwn_mac *mac)
{
#define	FLAG_A	0x01
#define	FLAG_G	0x02
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	static const struct bwn_b206x_rfinit_entry bwn_b2063_init_tab[] = {
		{ BWN_B2063_COM1, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_COM10, 0x1, 0x0, FLAG_A, },
		{ BWN_B2063_COM16, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_COM17, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_COM18, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_COM19, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_COM20, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_COM21, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_COM22, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_COM23, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_COM24, 0x0, 0x0, FLAG_G, },
		{ BWN_B2063_LOGEN_SP1, 0xe8, 0xd4, FLAG_A | FLAG_G, },
		{ BWN_B2063_LOGEN_SP2, 0xa7, 0x53, FLAG_A | FLAG_G, },
		{ BWN_B2063_LOGEN_SP4, 0xf0, 0xf, FLAG_A | FLAG_G, },
		{ BWN_B2063_G_RX_SP1, 0x1f, 0x5e, FLAG_G, },
		{ BWN_B2063_G_RX_SP2, 0x7f, 0x7e, FLAG_G, },
		{ BWN_B2063_G_RX_SP3, 0x30, 0xf0, FLAG_G, },
		{ BWN_B2063_G_RX_SP7, 0x7f, 0x7f, FLAG_A | FLAG_G, },
		{ BWN_B2063_G_RX_SP10, 0xc, 0xc, FLAG_A | FLAG_G, },
		{ BWN_B2063_A_RX_SP1, 0x3c, 0x3f, FLAG_A, },
		{ BWN_B2063_A_RX_SP2, 0xfc, 0xfe, FLAG_A, },
		{ BWN_B2063_A_RX_SP7, 0x8, 0x8, FLAG_A | FLAG_G, },
		{ BWN_B2063_RX_BB_SP4, 0x60, 0x60, FLAG_A | FLAG_G, },
		{ BWN_B2063_RX_BB_SP8, 0x30, 0x30, FLAG_A | FLAG_G, },
		{ BWN_B2063_TX_RF_SP3, 0xc, 0xb, FLAG_A | FLAG_G, },
		{ BWN_B2063_TX_RF_SP4, 0x10, 0xf, FLAG_A | FLAG_G, },
		{ BWN_B2063_PA_SP1, 0x3d, 0xfd, FLAG_A | FLAG_G, },
		{ BWN_B2063_TX_BB_SP1, 0x2, 0x2, FLAG_A | FLAG_G, },
		{ BWN_B2063_BANDGAP_CTL1, 0x56, 0x56, FLAG_A | FLAG_G, },
		{ BWN_B2063_JTAG_VCO2, 0xF7, 0xF7, FLAG_A | FLAG_G, },
		{ BWN_B2063_G_RX_MIX3, 0x71, 0x71, FLAG_A | FLAG_G, },
		{ BWN_B2063_G_RX_MIX4, 0x71, 0x71, FLAG_A | FLAG_G, },
		{ BWN_B2063_A_RX_1ST2, 0xf0, 0x30, FLAG_A, },
		{ BWN_B2063_A_RX_PS6, 0x77, 0x77, FLAG_A | FLAG_G, },
		{ BWN_B2063_A_RX_MIX4, 0x3, 0x3, FLAG_A | FLAG_G, },
		{ BWN_B2063_A_RX_MIX5, 0xf, 0xf, FLAG_A | FLAG_G, },
		{ BWN_B2063_A_RX_MIX6, 0xf, 0xf, FLAG_A | FLAG_G, },
		{ BWN_B2063_RX_TIA_CTL1, 0x77, 0x77, FLAG_A | FLAG_G, },
		{ BWN_B2063_RX_TIA_CTL3, 0x77, 0x77, FLAG_A | FLAG_G, },
		{ BWN_B2063_RX_BB_CTL2, 0x4, 0x4, FLAG_A | FLAG_G, },
		{ BWN_B2063_PA_CTL1, 0x0, 0x4, FLAG_A, },
		{ BWN_B2063_VREG_CTL1, 0x3, 0x3, FLAG_A | FLAG_G, },
	};
	const struct bwn_b206x_rfinit_entry *br;
	unsigned int i;

	for (i = 0; i < N(bwn_b2063_init_tab); i++) {
		br = &bwn_b2063_init_tab[i];
		if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
			if (br->br_flags & FLAG_G)
				BWN_RF_WRITE(mac, br->br_offset, br->br_valueg);
		} else {
			if (br->br_flags & FLAG_A)
				BWN_RF_WRITE(mac, br->br_offset, br->br_valuea);
		}
	}
#undef FLAG_A
#undef FLAG_B
}

static void
bwn_tab_read_multi(struct bwn_mac *mac, uint32_t typenoffset,
    int count, void *_data)
{
	unsigned int i;
	uint32_t offset, type;
	uint8_t *data = _data;

	type = BWN_TAB_GETTYPE(typenoffset);
	offset = BWN_TAB_GETOFFSET(typenoffset);
	KASSERT(offset <= 0xffff, ("%s:%d: fail", __func__, __LINE__));

	BWN_PHY_WRITE(mac, BWN_PHY_TABLE_ADDR, offset);

	for (i = 0; i < count; i++) {
		switch (type) {
		case BWN_TAB_8BIT:
			*data = BWN_PHY_READ(mac, BWN_PHY_TABLEDATALO) & 0xff;
			data++;
			break;
		case BWN_TAB_16BIT:
			*((uint16_t *)data) = BWN_PHY_READ(mac,
			    BWN_PHY_TABLEDATALO);
			data += 2;
			break;
		case BWN_TAB_32BIT:
			*((uint32_t *)data) = BWN_PHY_READ(mac,
			    BWN_PHY_TABLEDATAHI);
			*((uint32_t *)data) <<= 16;
			*((uint32_t *)data) |= BWN_PHY_READ(mac,
			    BWN_PHY_TABLEDATALO);
			data += 4;
			break;
		default:
			KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
		}
	}
}

static void
bwn_tab_write_multi(struct bwn_mac *mac, uint32_t typenoffset,
    int count, const void *_data)
{
	uint32_t offset, type, value;
	const uint8_t *data = _data;
	unsigned int i;

	type = BWN_TAB_GETTYPE(typenoffset);
	offset = BWN_TAB_GETOFFSET(typenoffset);
	KASSERT(offset <= 0xffff, ("%s:%d: fail", __func__, __LINE__));

	BWN_PHY_WRITE(mac, BWN_PHY_TABLE_ADDR, offset);

	for (i = 0; i < count; i++) {
		switch (type) {
		case BWN_TAB_8BIT:
			value = *data;
			data++;
			KASSERT(!(value & ~0xff),
			    ("%s:%d: fail", __func__, __LINE__));
			BWN_PHY_WRITE(mac, BWN_PHY_TABLEDATALO, value);
			break;
		case BWN_TAB_16BIT:
			value = *((const uint16_t *)data);
			data += 2;
			KASSERT(!(value & ~0xffff),
			    ("%s:%d: fail", __func__, __LINE__));
			BWN_PHY_WRITE(mac, BWN_PHY_TABLEDATALO, value);
			break;
		case BWN_TAB_32BIT:
			value = *((const uint32_t *)data);
			data += 4;
			BWN_PHY_WRITE(mac, BWN_PHY_TABLEDATAHI, value >> 16);
			BWN_PHY_WRITE(mac, BWN_PHY_TABLEDATALO, value);
			break;
		default:
			KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
		}
	}
}

static struct bwn_txgain
bwn_phy_lp_get_txgain(struct bwn_mac *mac)
{
	struct bwn_txgain tg;
	uint16_t tmp;

	tg.tg_dac = (BWN_PHY_READ(mac, BWN_PHY_AFE_DAC_CTL) & 0x380) >> 7;
	if (mac->mac_phy.rev < 2) {
		tmp = BWN_PHY_READ(mac,
		    BWN_PHY_TX_GAIN_CTL_OVERRIDE_VAL) & 0x7ff;
		tg.tg_gm = tmp & 0x0007;
		tg.tg_pga = (tmp & 0x0078) >> 3;
		tg.tg_pad = (tmp & 0x780) >> 7;
		return (tg);
	}

	tmp = BWN_PHY_READ(mac, BWN_PHY_TX_GAIN_CTL_OVERRIDE_VAL);
	tg.tg_pad = BWN_PHY_READ(mac, BWN_PHY_OFDM(0xfb)) & 0xff;
	tg.tg_gm = tmp & 0xff;
	tg.tg_pga = (tmp >> 8) & 0xff;
	return (tg);
}

static uint8_t
bwn_phy_lp_get_bbmult(struct bwn_mac *mac)
{

	return (bwn_tab_read(mac, BWN_TAB_2(0, 87)) & 0xff00) >> 8;
}

static void
bwn_phy_lp_set_txgain(struct bwn_mac *mac, struct bwn_txgain *tg)
{
	uint16_t pa;

	if (mac->mac_phy.rev < 2) {
		BWN_PHY_SETMASK(mac, BWN_PHY_TX_GAIN_CTL_OVERRIDE_VAL, 0xf800,
		    (tg->tg_pad << 7) | (tg->tg_pga << 3) | tg->tg_gm);
		bwn_phy_lp_set_txgain_dac(mac, tg->tg_dac);
		bwn_phy_lp_set_txgain_override(mac);
		return;
	}

	pa = bwn_phy_lp_get_pa_gain(mac);
	BWN_PHY_WRITE(mac, BWN_PHY_TX_GAIN_CTL_OVERRIDE_VAL,
	    (tg->tg_pga << 8) | tg->tg_gm);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0xfb), 0x8000,
	    tg->tg_pad | (pa << 6));
	BWN_PHY_WRITE(mac, BWN_PHY_OFDM(0xfc), (tg->tg_pga << 8) | tg->tg_gm);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0xfd), 0x8000,
	    tg->tg_pad | (pa << 8));
	bwn_phy_lp_set_txgain_dac(mac, tg->tg_dac);
	bwn_phy_lp_set_txgain_override(mac);
}

static void
bwn_phy_lp_set_bbmult(struct bwn_mac *mac, uint8_t bbmult)
{

	bwn_tab_write(mac, BWN_TAB_2(0, 87), (uint16_t)bbmult << 8);
}

static void
bwn_phy_lp_set_trsw_over(struct bwn_mac *mac, uint8_t tx, uint8_t rx)
{
	uint16_t trsw = (tx << 1) | rx;

	BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xfffc, trsw);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x3);
}

static void
bwn_phy_lp_set_rxgain(struct bwn_mac *mac, uint32_t gain)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint16_t ext_lna, high_gain, lna, low_gain, trsw, tmp;

	if (mac->mac_phy.rev < 2) {
		trsw = gain & 0x1;
		lna = (gain & 0xfffc) | ((gain & 0xc) >> 2);
		ext_lna = (gain & 2) >> 1;

		BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xfffe, trsw);
		BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL,
		    0xfbff, ext_lna << 10);
		BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL,
		    0xf7ff, ext_lna << 11);
		BWN_PHY_WRITE(mac, BWN_PHY_RX_GAIN_CTL_OVERRIDE_VAL, lna);
	} else {
		low_gain = gain & 0xffff;
		high_gain = (gain >> 16) & 0xf;
		ext_lna = (gain >> 21) & 0x1;
		trsw = ~(gain >> 20) & 0x1;

		BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0xfffe, trsw);
		BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL,
		    0xfdff, ext_lna << 9);
		BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL,
		    0xfbff, ext_lna << 10);
		BWN_PHY_WRITE(mac, BWN_PHY_RX_GAIN_CTL_OVERRIDE_VAL, low_gain);
		BWN_PHY_SETMASK(mac, BWN_PHY_AFE_DDFS, 0xfff0, high_gain);
		if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
			tmp = (gain >> 2) & 0x3;
			BWN_PHY_SETMASK(mac, BWN_PHY_RF_OVERRIDE_2_VAL,
			    0xe7ff, tmp<<11);
			BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0xe6), 0xffe7,
			    tmp << 3);
		}
	}

	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x1);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x10);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x40);
	if (mac->mac_phy.rev >= 2) {
		BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2, 0x100);
		if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
			BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2, 0x400);
			BWN_PHY_SET(mac, BWN_PHY_OFDM(0xe5), 0x8);
		}
		return;
	}
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2, 0x200);
}

static void
bwn_phy_lp_set_deaf(struct bwn_mac *mac, uint8_t user)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;

	if (user)
		plp->plp_crsusr_off = 1;
	else
		plp->plp_crssys_off = 1;

	BWN_PHY_SETMASK(mac, BWN_PHY_CRSGAIN_CTL, 0xff1f, 0x80);
}

static void
bwn_phy_lp_clear_deaf(struct bwn_mac *mac, uint8_t user)
{
	struct bwn_phy_lp *plp = &mac->mac_phy.phy_lp;
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;

	if (user)
		plp->plp_crsusr_off = 0;
	else
		plp->plp_crssys_off = 0;

	if (plp->plp_crsusr_off || plp->plp_crssys_off)
		return;

	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
		BWN_PHY_SETMASK(mac, BWN_PHY_CRSGAIN_CTL, 0xff1f, 0x60);
	else
		BWN_PHY_SETMASK(mac, BWN_PHY_CRSGAIN_CTL, 0xff1f, 0x20);
}

static unsigned int
bwn_sqrt(struct bwn_mac *mac, unsigned int x)
{
	/* Table holding (10 * sqrt(x)) for x between 1 and 256. */
	static uint8_t sqrt_table[256] = {
		10, 14, 17, 20, 22, 24, 26, 28,
		30, 31, 33, 34, 36, 37, 38, 40,
		41, 42, 43, 44, 45, 46, 47, 48,
		50, 50, 51, 52, 53, 54, 55, 56,
		57, 58, 59, 60, 60, 61, 62, 63,
		64, 64, 65, 66, 67, 67, 68, 69,
		70, 70, 71, 72, 72, 73, 74, 74,
		75, 76, 76, 77, 78, 78, 79, 80,
		80, 81, 81, 82, 83, 83, 84, 84,
		85, 86, 86, 87, 87, 88, 88, 89,
		90, 90, 91, 91, 92, 92, 93, 93,
		94, 94, 95, 95, 96, 96, 97, 97,
		98, 98, 99, 100, 100, 100, 101, 101,
		102, 102, 103, 103, 104, 104, 105, 105,
		106, 106, 107, 107, 108, 108, 109, 109,
		110, 110, 110, 111, 111, 112, 112, 113,
		113, 114, 114, 114, 115, 115, 116, 116,
		117, 117, 117, 118, 118, 119, 119, 120,
		120, 120, 121, 121, 122, 122, 122, 123,
		123, 124, 124, 124, 125, 125, 126, 126,
		126, 127, 127, 128, 128, 128, 129, 129,
		130, 130, 130, 131, 131, 131, 132, 132,
		133, 133, 133, 134, 134, 134, 135, 135,
		136, 136, 136, 137, 137, 137, 138, 138,
		138, 139, 139, 140, 140, 140, 141, 141,
		141, 142, 142, 142, 143, 143, 143, 144,
		144, 144, 145, 145, 145, 146, 146, 146,
		147, 147, 147, 148, 148, 148, 149, 149,
		150, 150, 150, 150, 151, 151, 151, 152,
		152, 152, 153, 153, 153, 154, 154, 154,
		155, 155, 155, 156, 156, 156, 157, 157,
		157, 158, 158, 158, 159, 159, 159, 160
	};

	if (x == 0)
		return (0);
	if (x >= 256) {
		unsigned int tmp;

		for (tmp = 0; x >= (2 * tmp) + 1; x -= (2 * tmp++) + 1)
			/* do nothing */ ;
		return (tmp);
	}
	return (sqrt_table[x - 1] / 10);
}

static int
bwn_phy_lp_calc_rx_iq_comp(struct bwn_mac *mac, uint16_t sample)
{
#define	CALC_COEFF(_v, _x, _y, _z)	do {				\
	int _t;								\
	_t = _x - 20;							\
	if (_t >= 0) {							\
		_v = ((_y << (30 - _x)) + (_z >> (1 + _t))) / (_z >> _t); \
	} else {							\
		_v = ((_y << (30 - _x)) + (_z << (-1 - _t))) / (_z << -_t); \
	}								\
} while (0)
#define	CALC_COEFF2(_v, _x, _y, _z)	do {				\
	int _t;								\
	_t = _x - 11;							\
	if (_t >= 0)							\
		_v = (_y << (31 - _x)) / (_z >> _t);			\
	else								\
		_v = (_y << (31 - _x)) / (_z << -_t);			\
} while (0)
	struct bwn_phy_lp_iq_est ie;
	uint16_t v0, v1;
	int tmp[2], ret;

	v1 = BWN_PHY_READ(mac, BWN_PHY_RX_COMP_COEFF_S);
	v0 = v1 >> 8;
	v1 |= 0xff;

	BWN_PHY_SETMASK(mac, BWN_PHY_RX_COMP_COEFF_S, 0xff00, 0x00c0);
	BWN_PHY_MASK(mac, BWN_PHY_RX_COMP_COEFF_S, 0x00ff);

	ret = bwn_phy_lp_rx_iq_est(mac, sample, 32, &ie);
	if (ret == 0)
		goto done;

	if (ie.ie_ipwr + ie.ie_qpwr < 2) {
		ret = 0;
		goto done;
	}

	CALC_COEFF(tmp[0], bwn_nbits(ie.ie_iqprod), ie.ie_iqprod, ie.ie_ipwr);
	CALC_COEFF2(tmp[1], bwn_nbits(ie.ie_qpwr), ie.ie_qpwr, ie.ie_ipwr);

	tmp[1] = -bwn_sqrt(mac, tmp[1] - (tmp[0] * tmp[0]));
	v0 = tmp[0] >> 3;
	v1 = tmp[1] >> 4;
done:
	BWN_PHY_SETMASK(mac, BWN_PHY_RX_COMP_COEFF_S, 0xff00, v1);
	BWN_PHY_SETMASK(mac, BWN_PHY_RX_COMP_COEFF_S, 0x00ff, v0 << 8);
	return ret;
#undef CALC_COEFF
#undef CALC_COEFF2
}

static void
bwn_phy_lp_tblinit_r01(struct bwn_mac *mac)
{
	static const uint16_t noisescale[] = {
		0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4,
		0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa400, 0xa4a4, 0xa4a4,
		0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4,
		0xa4a4, 0xa4a4, 0x00a4, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x4c00, 0x2d36, 0x0000, 0x0000, 0x4c00, 0x2d36,
	};
	static const uint16_t crsgainnft[] = {
		0x0366, 0x036a, 0x036f, 0x0364, 0x0367, 0x036d, 0x0374, 0x037f,
		0x036f, 0x037b, 0x038a, 0x0378, 0x0367, 0x036d, 0x0375, 0x0381,
		0x0374, 0x0381, 0x0392, 0x03a9, 0x03c4, 0x03e1, 0x0001, 0x001f,
		0x0040, 0x005e, 0x007f, 0x009e, 0x00bd, 0x00dd, 0x00fd, 0x011d,
		0x013d,
	};
	static const uint16_t filterctl[] = {
		0xa0fc, 0x10fc, 0x10db, 0x20b7, 0xff93, 0x10bf, 0x109b, 0x2077,
		0xff53, 0x0127,
	};
	static const uint32_t psctl[] = {
		0x00010000, 0x000000a0, 0x00040000, 0x00000048, 0x08080101,
		0x00000080, 0x08080101, 0x00000040, 0x08080101, 0x000000c0,
		0x08a81501, 0x000000c0, 0x0fe8fd01, 0x000000c0, 0x08300105,
		0x000000c0, 0x08080201, 0x000000c0, 0x08280205, 0x000000c0,
		0xe80802fe, 0x000000c7, 0x28080206, 0x000000c0, 0x08080202,
		0x000000c0, 0x0ba87602, 0x000000c0, 0x1068013d, 0x000000c0,
		0x10280105, 0x000000c0, 0x08880102, 0x000000c0, 0x08280106,
		0x000000c0, 0xe80801fd, 0x000000c7, 0xa8080115, 0x000000c0,
	};
	static const uint16_t ofdmcckgain_r0[] = {
		0x0001, 0x0001, 0x0001, 0x0001, 0x1001, 0x2001, 0x3001, 0x4001,
		0x5001, 0x6001, 0x7001, 0x7011, 0x7021, 0x2035, 0x2045, 0x2055,
		0x2065, 0x2075, 0x006d, 0x007d, 0x014d, 0x015d, 0x115d, 0x035d,
		0x135d, 0x055d, 0x155d, 0x0d5d, 0x1d5d, 0x2d5d, 0x555d, 0x655d,
		0x755d,
	};
	static const uint16_t ofdmcckgain_r1[] = {
		0x5000, 0x6000, 0x7000, 0x0001, 0x1001, 0x2001, 0x3001, 0x4001,
		0x5001, 0x6001, 0x7001, 0x7011, 0x7021, 0x2035, 0x2045, 0x2055,
		0x2065, 0x2075, 0x006d, 0x007d, 0x014d, 0x015d, 0x115d, 0x035d,
		0x135d, 0x055d, 0x155d, 0x0d5d, 0x1d5d, 0x2d5d, 0x555d, 0x655d,
		0x755d,
	};
	static const uint16_t gaindelta[] = {
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000,
	};
	static const uint32_t txpwrctl[] = {
		0x00000050, 0x0000004f, 0x0000004e, 0x0000004d, 0x0000004c,
		0x0000004b, 0x0000004a, 0x00000049, 0x00000048, 0x00000047,
		0x00000046, 0x00000045, 0x00000044, 0x00000043, 0x00000042,
		0x00000041, 0x00000040, 0x0000003f, 0x0000003e, 0x0000003d,
		0x0000003c, 0x0000003b, 0x0000003a, 0x00000039, 0x00000038,
		0x00000037, 0x00000036, 0x00000035, 0x00000034, 0x00000033,
		0x00000032, 0x00000031, 0x00000030, 0x0000002f, 0x0000002e,
		0x0000002d, 0x0000002c, 0x0000002b, 0x0000002a, 0x00000029,
		0x00000028, 0x00000027, 0x00000026, 0x00000025, 0x00000024,
		0x00000023, 0x00000022, 0x00000021, 0x00000020, 0x0000001f,
		0x0000001e, 0x0000001d, 0x0000001c, 0x0000001b, 0x0000001a,
		0x00000019, 0x00000018, 0x00000017, 0x00000016, 0x00000015,
		0x00000014, 0x00000013, 0x00000012, 0x00000011, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x000075a0, 0x000075a0, 0x000075a1,
		0x000075a1, 0x000075a2, 0x000075a2, 0x000075a3, 0x000075a3,
		0x000074b0, 0x000074b0, 0x000074b1, 0x000074b1, 0x000074b2,
		0x000074b2, 0x000074b3, 0x000074b3, 0x00006d20, 0x00006d20,
		0x00006d21, 0x00006d21, 0x00006d22, 0x00006d22, 0x00006d23,
		0x00006d23, 0x00004660, 0x00004660, 0x00004661, 0x00004661,
		0x00004662, 0x00004662, 0x00004663, 0x00004663, 0x00003e60,
		0x00003e60, 0x00003e61, 0x00003e61, 0x00003e62, 0x00003e62,
		0x00003e63, 0x00003e63, 0x00003660, 0x00003660, 0x00003661,
		0x00003661, 0x00003662, 0x00003662, 0x00003663, 0x00003663,
		0x00002e60, 0x00002e60, 0x00002e61, 0x00002e61, 0x00002e62,
		0x00002e62, 0x00002e63, 0x00002e63, 0x00002660, 0x00002660,
		0x00002661, 0x00002661, 0x00002662, 0x00002662, 0x00002663,
		0x00002663, 0x000025e0, 0x000025e0, 0x000025e1, 0x000025e1,
		0x000025e2, 0x000025e2, 0x000025e3, 0x000025e3, 0x00001de0,
		0x00001de0, 0x00001de1, 0x00001de1, 0x00001de2, 0x00001de2,
		0x00001de3, 0x00001de3, 0x00001d60, 0x00001d60, 0x00001d61,
		0x00001d61, 0x00001d62, 0x00001d62, 0x00001d63, 0x00001d63,
		0x00001560, 0x00001560, 0x00001561, 0x00001561, 0x00001562,
		0x00001562, 0x00001563, 0x00001563, 0x00000d60, 0x00000d60,
		0x00000d61, 0x00000d61, 0x00000d62, 0x00000d62, 0x00000d63,
		0x00000d63, 0x00000ce0, 0x00000ce0, 0x00000ce1, 0x00000ce1,
		0x00000ce2, 0x00000ce2, 0x00000ce3, 0x00000ce3, 0x00000e10,
		0x00000e10, 0x00000e11, 0x00000e11, 0x00000e12, 0x00000e12,
		0x00000e13, 0x00000e13, 0x00000bf0, 0x00000bf0, 0x00000bf1,
		0x00000bf1, 0x00000bf2, 0x00000bf2, 0x00000bf3, 0x00000bf3,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x04200000, 0x04000000,
		0x04200000, 0x04000000, 0x04200000, 0x04000000, 0x04200000,
		0x04000000, 0x04200000, 0x04000000, 0x000000ff, 0x000002fc,
		0x0000fa08, 0x00000305, 0x00000206, 0x00000304, 0x0000fb04,
		0x0000fcff, 0x000005fb, 0x0000fd01, 0x00000401, 0x00000006,
		0x0000ff03, 0x000007fc, 0x0000fc08, 0x00000203, 0x0000fffb,
		0x00000600, 0x0000fa01, 0x0000fc03, 0x0000fe06, 0x0000fe00,
		0x00000102, 0x000007fd, 0x000004fb, 0x000006ff, 0x000004fd,
		0x0000fdfa, 0x000007fb, 0x0000fdfa, 0x0000fa06, 0x00000500,
		0x0000f902, 0x000007fa, 0x0000fafa, 0x00000500, 0x000007fa,
		0x00000700, 0x00000305, 0x000004ff, 0x00000801, 0x00000503,
		0x000005f9, 0x00000404, 0x0000fb08, 0x000005fd, 0x00000501,
		0x00000405, 0x0000fb03, 0x000007fc, 0x00000403, 0x00000303,
		0x00000402, 0x0000faff, 0x0000fe05, 0x000005fd, 0x0000fe01,
		0x000007fa, 0x00000202, 0x00000504, 0x00000102, 0x000008fe,
		0x0000fa04, 0x0000fafc, 0x0000fe08, 0x000000f9, 0x000002fa,
		0x000003fe, 0x00000304, 0x000004f9, 0x00000100, 0x0000fd06,
		0x000008fc, 0x00000701, 0x00000504, 0x0000fdfe, 0x0000fdfc,
		0x000003fe, 0x00000704, 0x000002fc, 0x000004f9, 0x0000fdfd,
		0x0000fa07, 0x00000205, 0x000003fd, 0x000005fb, 0x000004f9,
		0x00000804, 0x0000fc06, 0x0000fcf9, 0x00000100, 0x0000fe05,
		0x00000408, 0x0000fb02, 0x00000304, 0x000006fe, 0x000004fa,
		0x00000305, 0x000008fc, 0x00000102, 0x000001fd, 0x000004fc,
		0x0000fe03, 0x00000701, 0x000001fb, 0x000001f9, 0x00000206,
		0x000006fd, 0x00000508, 0x00000700, 0x00000304, 0x000005fe,
		0x000005ff, 0x0000fa04, 0x00000303, 0x0000fefb, 0x000007f9,
		0x0000fefc, 0x000004fd, 0x000005fc, 0x0000fffd, 0x0000fc08,
		0x0000fbf9, 0x0000fd07, 0x000008fb, 0x0000fe02, 0x000006fb,
		0x00000702,
	};

	KASSERT(mac->mac_phy.rev < 2, ("%s:%d: fail", __func__, __LINE__));

	bwn_tab_write_multi(mac, BWN_TAB_1(2, 0), N(bwn_tab_sigsq_tbl),
	    bwn_tab_sigsq_tbl);
	bwn_tab_write_multi(mac, BWN_TAB_2(1, 0), N(noisescale), noisescale);
	bwn_tab_write_multi(mac, BWN_TAB_2(14, 0), N(crsgainnft), crsgainnft);
	bwn_tab_write_multi(mac, BWN_TAB_2(8, 0), N(filterctl), filterctl);
	bwn_tab_write_multi(mac, BWN_TAB_4(9, 0), N(psctl), psctl);
	bwn_tab_write_multi(mac, BWN_TAB_1(6, 0), N(bwn_tab_pllfrac_tbl),
	    bwn_tab_pllfrac_tbl);
	bwn_tab_write_multi(mac, BWN_TAB_2(0, 0), N(bwn_tabl_iqlocal_tbl),
	    bwn_tabl_iqlocal_tbl);
	if (mac->mac_phy.rev == 0) {
		bwn_tab_write_multi(mac, BWN_TAB_2(13, 0), N(ofdmcckgain_r0),
		    ofdmcckgain_r0);
		bwn_tab_write_multi(mac, BWN_TAB_2(12, 0), N(ofdmcckgain_r0),
		    ofdmcckgain_r0);
	} else {
		bwn_tab_write_multi(mac, BWN_TAB_2(13, 0), N(ofdmcckgain_r1),
		    ofdmcckgain_r1);
		bwn_tab_write_multi(mac, BWN_TAB_2(12, 0), N(ofdmcckgain_r1),
		    ofdmcckgain_r1);
	}
	bwn_tab_write_multi(mac, BWN_TAB_2(15, 0), N(gaindelta), gaindelta);
	bwn_tab_write_multi(mac, BWN_TAB_4(10, 0), N(txpwrctl), txpwrctl);
}

static void
bwn_phy_lp_tblinit_r2(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	int i;
	static const uint16_t noisescale[] = {
		0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
		0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
		0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
		0x00a4, 0x00a4, 0x0000, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
		0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
		0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4,
		0x00a4, 0x00a4, 0x00a4, 0x00a4, 0x00a4
	};
	static const uint32_t filterctl[] = {
		0x000141fc, 0x000021fc, 0x000021b7, 0x0000416f, 0x0001ff27,
		0x0000217f, 0x00002137, 0x000040ef, 0x0001fea7, 0x0000024f
	};
	static const uint32_t psctl[] = {
		0x00e38e08, 0x00e08e38, 0x00000000, 0x00000000, 0x00000000,
		0x00002080, 0x00006180, 0x00003002, 0x00000040, 0x00002042,
		0x00180047, 0x00080043, 0x00000041, 0x000020c1, 0x00046006,
		0x00042002, 0x00040000, 0x00002003, 0x00180006, 0x00080002
	};
	static const uint32_t gainidx[] = {
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x10000001, 0x00000000,
		0x20000082, 0x00000000, 0x40000104, 0x00000000, 0x60004207,
		0x00000001, 0x7000838a, 0x00000001, 0xd021050d, 0x00000001,
		0xe041c683, 0x00000001, 0x50828805, 0x00000000, 0x80e34288,
		0x00000000, 0xb144040b, 0x00000000, 0xe1a6058e, 0x00000000,
		0x12064711, 0x00000001, 0xb0a18612, 0x00000010, 0xe1024794,
		0x00000010, 0x11630915, 0x00000011, 0x31c3ca1b, 0x00000011,
		0xc1848a9c, 0x00000018, 0xf1e50da0, 0x00000018, 0x22468e21,
		0x00000019, 0x4286d023, 0x00000019, 0xa347d0a4, 0x00000019,
		0xb36811a6, 0x00000019, 0xf3e89227, 0x00000019, 0x0408d329,
		0x0000001a, 0x244953aa, 0x0000001a, 0x346994ab, 0x0000001a,
		0x54aa152c, 0x0000001a, 0x64ca55ad, 0x0000001a, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x10000001, 0x00000000, 0x20000082,
		0x00000000, 0x40000104, 0x00000000, 0x60004207, 0x00000001,
		0x7000838a, 0x00000001, 0xd021050d, 0x00000001, 0xe041c683,
		0x00000001, 0x50828805, 0x00000000, 0x80e34288, 0x00000000,
		0xb144040b, 0x00000000, 0xe1a6058e, 0x00000000, 0x12064711,
		0x00000001, 0xb0a18612, 0x00000010, 0xe1024794, 0x00000010,
		0x11630915, 0x00000011, 0x31c3ca1b, 0x00000011, 0xc1848a9c,
		0x00000018, 0xf1e50da0, 0x00000018, 0x22468e21, 0x00000019,
		0x4286d023, 0x00000019, 0xa347d0a4, 0x00000019, 0xb36811a6,
		0x00000019, 0xf3e89227, 0x00000019, 0x0408d329, 0x0000001a,
		0x244953aa, 0x0000001a, 0x346994ab, 0x0000001a, 0x54aa152c,
		0x0000001a, 0x64ca55ad, 0x0000001a
	};
	static const uint16_t auxgainidx[] = {
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0001, 0x0002, 0x0004, 0x0016, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0002,
		0x0004, 0x0016
	};
	static const uint16_t swctl[] = {
		0x0128, 0x0128, 0x0009, 0x0009, 0x0028, 0x0028, 0x0028, 0x0028,
		0x0128, 0x0128, 0x0009, 0x0009, 0x0028, 0x0028, 0x0028, 0x0028,
		0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009,
		0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018,
		0x0128, 0x0128, 0x0009, 0x0009, 0x0028, 0x0028, 0x0028, 0x0028,
		0x0128, 0x0128, 0x0009, 0x0009, 0x0028, 0x0028, 0x0028, 0x0028,
		0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009,
		0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018, 0x0018
	};
	static const uint8_t hf[] = {
		0x4b, 0x36, 0x24, 0x18, 0x49, 0x34, 0x23, 0x17, 0x48,
		0x33, 0x23, 0x17, 0x48, 0x33, 0x23, 0x17
	};
	static const uint32_t gainval[] = {
		0x00000008, 0x0000000e, 0x00000014, 0x0000001a, 0x000000fb,
		0x00000004, 0x00000008, 0x0000000d, 0x00000001, 0x00000004,
		0x00000007, 0x0000000a, 0x0000000d, 0x00000010, 0x00000012,
		0x00000015, 0x00000000, 0x00000006, 0x0000000c, 0x00000000,
		0x00000000, 0x00000000, 0x00000012, 0x00000000, 0x00000000,
		0x00000000, 0x00000018, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x0000001e, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000003,
		0x00000006, 0x00000009, 0x0000000c, 0x0000000f, 0x00000012,
		0x00000015, 0x00000018, 0x0000001b, 0x0000001e, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000009,
		0x000000f1, 0x00000000, 0x00000000
	};
	static const uint16_t gain[] = {
		0x0000, 0x0400, 0x0800, 0x0802, 0x0804, 0x0806, 0x0807, 0x0808,
		0x080a, 0x080b, 0x080c, 0x080e, 0x080f, 0x0810, 0x0812, 0x0813,
		0x0814, 0x0816, 0x0817, 0x081a, 0x081b, 0x081f, 0x0820, 0x0824,
		0x0830, 0x0834, 0x0837, 0x083b, 0x083f, 0x0840, 0x0844, 0x0857,
		0x085b, 0x085f, 0x08d7, 0x08db, 0x08df, 0x0957, 0x095b, 0x095f,
		0x0b57, 0x0b5b, 0x0b5f, 0x0f5f, 0x135f, 0x175f, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
	};
	static const uint32_t papdeps[] = {
		0x00000000, 0x00013ffc, 0x0001dff3, 0x0001bff0, 0x00023fe9,
		0x00021fdf, 0x00028fdf, 0x00033fd2, 0x00039fcb, 0x00043fc7,
		0x0004efc2, 0x00055fb5, 0x0005cfb0, 0x00063fa8, 0x00068fa3,
		0x00071f98, 0x0007ef92, 0x00084f8b, 0x0008df82, 0x00097f77,
		0x0009df69, 0x000a3f62, 0x000adf57, 0x000b6f4c, 0x000bff41,
		0x000c9f39, 0x000cff30, 0x000dbf27, 0x000e4f1e, 0x000edf16,
		0x000f7f13, 0x00102f11, 0x00110f10, 0x0011df11, 0x0012ef15,
		0x00143f1c, 0x00158f27, 0x00172f35, 0x00193f47, 0x001baf5f,
		0x001e6f7e, 0x0021cfa4, 0x0025bfd2, 0x002a2008, 0x002fb047,
		0x00360090, 0x003d40e0, 0x0045c135, 0x004fb189, 0x005ae1d7,
		0x0067221d, 0x0075025a, 0x007ff291, 0x007ff2bf, 0x007ff2e3,
		0x007ff2ff, 0x007ff315, 0x007ff329, 0x007ff33f, 0x007ff356,
		0x007ff36e, 0x007ff39c, 0x007ff441, 0x007ff506
	};
	static const uint32_t papdmult[] = {
		0x001111e0, 0x00652051, 0x00606055, 0x005b005a, 0x00555060,
		0x00511065, 0x004c806b, 0x0047d072, 0x00444078, 0x00400080,
		0x003ca087, 0x0039408f, 0x0035e098, 0x0032e0a1, 0x003030aa,
		0x002d80b4, 0x002ae0bf, 0x002880ca, 0x002640d6, 0x002410e3,
		0x002220f0, 0x002020ff, 0x001e510e, 0x001ca11e, 0x001b012f,
		0x00199140, 0x00182153, 0x0016c168, 0x0015817d, 0x00145193,
		0x001321ab, 0x001211c5, 0x001111e0, 0x001021fc, 0x000f321a,
		0x000e523a, 0x000d925c, 0x000cd27f, 0x000c12a5, 0x000b62cd,
		0x000ac2f8, 0x000a2325, 0x00099355, 0x00091387, 0x000883bd,
		0x000813f5, 0x0007a432, 0x00073471, 0x0006c4b5, 0x000664fc,
		0x00061547, 0x0005b598, 0x000565ec, 0x00051646, 0x0004d6a5,
		0x0004870a, 0x00044775, 0x000407e6, 0x0003d85e, 0x000398dd,
		0x00036963, 0x000339f2, 0x00030a89, 0x0002db28
	};
	static const uint32_t gainidx_a0[] = {
		0x001111e0, 0x00652051, 0x00606055, 0x005b005a, 0x00555060,
		0x00511065, 0x004c806b, 0x0047d072, 0x00444078, 0x00400080,
		0x003ca087, 0x0039408f, 0x0035e098, 0x0032e0a1, 0x003030aa,
		0x002d80b4, 0x002ae0bf, 0x002880ca, 0x002640d6, 0x002410e3,
		0x002220f0, 0x002020ff, 0x001e510e, 0x001ca11e, 0x001b012f,
		0x00199140, 0x00182153, 0x0016c168, 0x0015817d, 0x00145193,
		0x001321ab, 0x001211c5, 0x001111e0, 0x001021fc, 0x000f321a,
		0x000e523a, 0x000d925c, 0x000cd27f, 0x000c12a5, 0x000b62cd,
		0x000ac2f8, 0x000a2325, 0x00099355, 0x00091387, 0x000883bd,
		0x000813f5, 0x0007a432, 0x00073471, 0x0006c4b5, 0x000664fc,
		0x00061547, 0x0005b598, 0x000565ec, 0x00051646, 0x0004d6a5,
		0x0004870a, 0x00044775, 0x000407e6, 0x0003d85e, 0x000398dd,
		0x00036963, 0x000339f2, 0x00030a89, 0x0002db28
	};
	static const uint16_t auxgainidx_a0[] = {
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0002, 0x0014, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0002, 0x0014
	};
	static const uint32_t gainval_a0[] = {
		0x00000008, 0x0000000e, 0x00000014, 0x0000001a, 0x000000fb,
		0x00000004, 0x00000008, 0x0000000d, 0x00000001, 0x00000004,
		0x00000007, 0x0000000a, 0x0000000d, 0x00000010, 0x00000012,
		0x00000015, 0x00000000, 0x00000006, 0x0000000c, 0x00000000,
		0x00000000, 0x00000000, 0x00000012, 0x00000000, 0x00000000,
		0x00000000, 0x00000018, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x0000001e, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000003,
		0x00000006, 0x00000009, 0x0000000c, 0x0000000f, 0x00000012,
		0x00000015, 0x00000018, 0x0000001b, 0x0000001e, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000f,
		0x000000f7, 0x00000000, 0x00000000
	};
	static const uint16_t gain_a0[] = {
		0x0000, 0x0002, 0x0004, 0x0006, 0x0007, 0x0008, 0x000a, 0x000b,
		0x000c, 0x000e, 0x000f, 0x0010, 0x0012, 0x0013, 0x0014, 0x0016,
		0x0017, 0x001a, 0x001b, 0x001f, 0x0020, 0x0024, 0x0030, 0x0034,
		0x0037, 0x003b, 0x003f, 0x0040, 0x0044, 0x0057, 0x005b, 0x005f,
		0x00d7, 0x00db, 0x00df, 0x0157, 0x015b, 0x015f, 0x0357, 0x035b,
		0x035f, 0x075f, 0x0b5f, 0x0f5f, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
	};

	KASSERT(mac->mac_phy.rev < 2, ("%s:%d: fail", __func__, __LINE__));

	for (i = 0; i < 704; i++)
		bwn_tab_write(mac, BWN_TAB_4(7, i), 0);

	bwn_tab_write_multi(mac, BWN_TAB_1(2, 0), N(bwn_tab_sigsq_tbl),
	    bwn_tab_sigsq_tbl);
	bwn_tab_write_multi(mac, BWN_TAB_2(1, 0), N(noisescale), noisescale);
	bwn_tab_write_multi(mac, BWN_TAB_4(11, 0), N(filterctl), filterctl);
	bwn_tab_write_multi(mac, BWN_TAB_4(12, 0), N(psctl), psctl);
	bwn_tab_write_multi(mac, BWN_TAB_4(13, 0), N(gainidx), gainidx);
	bwn_tab_write_multi(mac, BWN_TAB_2(14, 0), N(auxgainidx), auxgainidx);
	bwn_tab_write_multi(mac, BWN_TAB_2(15, 0), N(swctl), swctl);
	bwn_tab_write_multi(mac, BWN_TAB_1(16, 0), N(hf), hf);
	bwn_tab_write_multi(mac, BWN_TAB_4(17, 0), N(gainval), gainval);
	bwn_tab_write_multi(mac, BWN_TAB_2(18, 0), N(gain), gain);
	bwn_tab_write_multi(mac, BWN_TAB_1(6, 0), N(bwn_tab_pllfrac_tbl),
	    bwn_tab_pllfrac_tbl);
	bwn_tab_write_multi(mac, BWN_TAB_2(0, 0), N(bwn_tabl_iqlocal_tbl),
	    bwn_tabl_iqlocal_tbl);
	bwn_tab_write_multi(mac, BWN_TAB_4(9, 0), N(papdeps), papdeps);
	bwn_tab_write_multi(mac, BWN_TAB_4(10, 0), N(papdmult), papdmult);

	if ((siba_get_chipid(sc->sc_dev) == 0x4325) &&
	    (siba_get_chiprev(sc->sc_dev) == 0)) {
		bwn_tab_write_multi(mac, BWN_TAB_4(13, 0), N(gainidx_a0),
		    gainidx_a0);
		bwn_tab_write_multi(mac, BWN_TAB_2(14, 0), N(auxgainidx_a0),
		    auxgainidx_a0);
		bwn_tab_write_multi(mac, BWN_TAB_4(17, 0), N(gainval_a0),
		    gainval_a0);
		bwn_tab_write_multi(mac, BWN_TAB_2(18, 0), N(gain_a0), gain_a0);
	}
}

static void
bwn_phy_lp_tblinit_txgain(struct bwn_mac *mac)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	static struct bwn_txgain_entry txgain_r2[] = {
		{ 255, 255, 203, 0, 152 }, { 255, 255, 203, 0, 147 },
		{ 255, 255, 203, 0, 143 }, { 255, 255, 203, 0, 139 },
		{ 255, 255, 203, 0, 135 }, { 255, 255, 203, 0, 131 },
		{ 255, 255, 203, 0, 128 }, { 255, 255, 203, 0, 124 },
		{ 255, 255, 203, 0, 121 }, { 255, 255, 203, 0, 117 },
		{ 255, 255, 203, 0, 114 }, { 255, 255, 203, 0, 111 },
		{ 255, 255, 203, 0, 107 }, { 255, 255, 203, 0, 104 },
		{ 255, 255, 203, 0, 101 }, { 255, 255, 203, 0, 99 },
		{ 255, 255, 203, 0, 96 }, { 255, 255, 203, 0, 93 },
		{ 255, 255, 203, 0, 90 }, { 255, 255, 203, 0, 88 },
		{ 255, 255, 203, 0, 85 }, { 255, 255, 203, 0, 83 },
		{ 255, 255, 203, 0, 81 }, { 255, 255, 203, 0, 78 },
		{ 255, 255, 203, 0, 76 }, { 255, 255, 203, 0, 74 },
		{ 255, 255, 203, 0, 72 }, { 255, 255, 203, 0, 70 },
		{ 255, 255, 203, 0, 68 }, { 255, 255, 203, 0, 66 },
		{ 255, 255, 203, 0, 64 }, { 255, 255, 197, 0, 64 },
		{ 255, 255, 192, 0, 64 }, { 255, 255, 186, 0, 64 },
		{ 255, 255, 181, 0, 64 }, { 255, 255, 176, 0, 64 },
		{ 255, 255, 171, 0, 64 }, { 255, 255, 166, 0, 64 },
		{ 255, 255, 161, 0, 64 }, { 255, 255, 157, 0, 64 },
		{ 255, 255, 152, 0, 64 }, { 255, 255, 148, 0, 64 },
		{ 255, 255, 144, 0, 64 }, { 255, 255, 140, 0, 64 },
		{ 255, 255, 136, 0, 64 }, { 255, 255, 132, 0, 64 },
		{ 255, 255, 128, 0, 64 }, { 255, 255, 124, 0, 64 },
		{ 255, 255, 121, 0, 64 }, { 255, 255, 117, 0, 64 },
		{ 255, 255, 114, 0, 64 }, { 255, 255, 111, 0, 64 },
		{ 255, 255, 108, 0, 64 }, { 255, 255, 105, 0, 64 },
		{ 255, 255, 102, 0, 64 }, { 255, 255, 99, 0, 64 },
		{ 255, 255, 96, 0, 64 }, { 255, 255, 93, 0, 64 },
		{ 255, 255, 91, 0, 64 }, { 255, 255, 88, 0, 64 },
		{ 255, 255, 86, 0, 64 }, { 255, 255, 83, 0, 64 },
		{ 255, 255, 81, 0, 64 }, { 255, 255, 79, 0, 64 },
		{ 255, 255, 76, 0, 64 }, { 255, 255, 74, 0, 64 },
		{ 255, 255, 72, 0, 64 }, { 255, 255, 70, 0, 64 },
		{ 255, 255, 68, 0, 64 }, { 255, 255, 66, 0, 64 },
		{ 255, 255, 64, 0, 64 }, { 255, 248, 64, 0, 64 },
		{ 255, 248, 62, 0, 64 }, { 255, 241, 62, 0, 64 },
		{ 255, 241, 60, 0, 64 }, { 255, 234, 60, 0, 64 },
		{ 255, 234, 59, 0, 64 }, { 255, 227, 59, 0, 64 },
		{ 255, 227, 57, 0, 64 }, { 255, 221, 57, 0, 64 },
		{ 255, 221, 55, 0, 64 }, { 255, 215, 55, 0, 64 },
		{ 255, 215, 54, 0, 64 }, { 255, 208, 54, 0, 64 },
		{ 255, 208, 52, 0, 64 }, { 255, 203, 52, 0, 64 },
		{ 255, 203, 51, 0, 64 }, { 255, 197, 51, 0, 64 },
		{ 255, 197, 49, 0, 64 }, { 255, 191, 49, 0, 64 },
		{ 255, 191, 48, 0, 64 }, { 255, 186, 48, 0, 64 },
		{ 255, 186, 47, 0, 64 }, { 255, 181, 47, 0, 64 },
		{ 255, 181, 45, 0, 64 }, { 255, 175, 45, 0, 64 },
		{ 255, 175, 44, 0, 64 }, { 255, 170, 44, 0, 64 },
		{ 255, 170, 43, 0, 64 }, { 255, 166, 43, 0, 64 },
		{ 255, 166, 42, 0, 64 }, { 255, 161, 42, 0, 64 },
		{ 255, 161, 40, 0, 64 }, { 255, 156, 40, 0, 64 },
		{ 255, 156, 39, 0, 64 }, { 255, 152, 39, 0, 64 },
		{ 255, 152, 38, 0, 64 }, { 255, 148, 38, 0, 64 },
		{ 255, 148, 37, 0, 64 }, { 255, 143, 37, 0, 64 },
		{ 255, 143, 36, 0, 64 }, { 255, 139, 36, 0, 64 },
		{ 255, 139, 35, 0, 64 }, { 255, 135, 35, 0, 64 },
		{ 255, 135, 34, 0, 64 }, { 255, 132, 34, 0, 64 },
		{ 255, 132, 33, 0, 64 }, { 255, 128, 33, 0, 64 },
		{ 255, 128, 32, 0, 64 }, { 255, 124, 32, 0, 64 },
		{ 255, 124, 31, 0, 64 }, { 255, 121, 31, 0, 64 },
		{ 255, 121, 30, 0, 64 }, { 255, 117, 30, 0, 64 },
		{ 255, 117, 29, 0, 64 }, { 255, 114, 29, 0, 64 },
		{ 255, 114, 29, 0, 64 }, { 255, 111, 29, 0, 64 },
	};
	static struct bwn_txgain_entry txgain_2ghz_r2[] = {
		{ 7, 99, 255, 0, 64 }, { 7, 96, 255, 0, 64 },
		{ 7, 93, 255, 0, 64 }, { 7, 90, 255, 0, 64 },
		{ 7, 88, 255, 0, 64 }, { 7, 85, 255, 0, 64 },
		{ 7, 83, 255, 0, 64 }, { 7, 81, 255, 0, 64 },
		{ 7, 78, 255, 0, 64 }, { 7, 76, 255, 0, 64 },
		{ 7, 74, 255, 0, 64 }, { 7, 72, 255, 0, 64 },
		{ 7, 70, 255, 0, 64 }, { 7, 68, 255, 0, 64 },
		{ 7, 66, 255, 0, 64 }, { 7, 64, 255, 0, 64 },
		{ 7, 64, 255, 0, 64 }, { 7, 62, 255, 0, 64 },
		{ 7, 62, 248, 0, 64 }, { 7, 60, 248, 0, 64 },
		{ 7, 60, 241, 0, 64 }, { 7, 59, 241, 0, 64 },
		{ 7, 59, 234, 0, 64 }, { 7, 57, 234, 0, 64 },
		{ 7, 57, 227, 0, 64 }, { 7, 55, 227, 0, 64 },
		{ 7, 55, 221, 0, 64 }, { 7, 54, 221, 0, 64 },
		{ 7, 54, 215, 0, 64 }, { 7, 52, 215, 0, 64 },
		{ 7, 52, 208, 0, 64 }, { 7, 51, 208, 0, 64 },
		{ 7, 51, 203, 0, 64 }, { 7, 49, 203, 0, 64 },
		{ 7, 49, 197, 0, 64 }, { 7, 48, 197, 0, 64 },
		{ 7, 48, 191, 0, 64 }, { 7, 47, 191, 0, 64 },
		{ 7, 47, 186, 0, 64 }, { 7, 45, 186, 0, 64 },
		{ 7, 45, 181, 0, 64 }, { 7, 44, 181, 0, 64 },
		{ 7, 44, 175, 0, 64 }, { 7, 43, 175, 0, 64 },
		{ 7, 43, 170, 0, 64 }, { 7, 42, 170, 0, 64 },
		{ 7, 42, 166, 0, 64 }, { 7, 40, 166, 0, 64 },
		{ 7, 40, 161, 0, 64 }, { 7, 39, 161, 0, 64 },
		{ 7, 39, 156, 0, 64 }, { 7, 38, 156, 0, 64 },
		{ 7, 38, 152, 0, 64 }, { 7, 37, 152, 0, 64 },
		{ 7, 37, 148, 0, 64 }, { 7, 36, 148, 0, 64 },
		{ 7, 36, 143, 0, 64 }, { 7, 35, 143, 0, 64 },
		{ 7, 35, 139, 0, 64 }, { 7, 34, 139, 0, 64 },
		{ 7, 34, 135, 0, 64 }, { 7, 33, 135, 0, 64 },
		{ 7, 33, 132, 0, 64 }, { 7, 32, 132, 0, 64 },
		{ 7, 32, 128, 0, 64 }, { 7, 31, 128, 0, 64 },
		{ 7, 31, 124, 0, 64 }, { 7, 30, 124, 0, 64 },
		{ 7, 30, 121, 0, 64 }, { 7, 29, 121, 0, 64 },
		{ 7, 29, 117, 0, 64 }, { 7, 29, 117, 0, 64 },
		{ 7, 29, 114, 0, 64 }, { 7, 28, 114, 0, 64 },
		{ 7, 28, 111, 0, 64 }, { 7, 27, 111, 0, 64 },
		{ 7, 27, 108, 0, 64 }, { 7, 26, 108, 0, 64 },
		{ 7, 26, 104, 0, 64 }, { 7, 25, 104, 0, 64 },
		{ 7, 25, 102, 0, 64 }, { 7, 25, 102, 0, 64 },
		{ 7, 25, 99, 0, 64 }, { 7, 24, 99, 0, 64 },
		{ 7, 24, 96, 0, 64 }, { 7, 23, 96, 0, 64 },
		{ 7, 23, 93, 0, 64 }, { 7, 23, 93, 0, 64 },
		{ 7, 23, 90, 0, 64 }, { 7, 22, 90, 0, 64 },
		{ 7, 22, 88, 0, 64 }, { 7, 21, 88, 0, 64 },
		{ 7, 21, 85, 0, 64 }, { 7, 21, 85, 0, 64 },
		{ 7, 21, 83, 0, 64 }, { 7, 20, 83, 0, 64 },
		{ 7, 20, 81, 0, 64 }, { 7, 20, 81, 0, 64 },
		{ 7, 20, 78, 0, 64 }, { 7, 19, 78, 0, 64 },
		{ 7, 19, 76, 0, 64 }, { 7, 19, 76, 0, 64 },
		{ 7, 19, 74, 0, 64 }, { 7, 18, 74, 0, 64 },
		{ 7, 18, 72, 0, 64 }, { 7, 18, 72, 0, 64 },
		{ 7, 18, 70, 0, 64 }, { 7, 17, 70, 0, 64 },
		{ 7, 17, 68, 0, 64 }, { 7, 17, 68, 0, 64 },
		{ 7, 17, 66, 0, 64 }, { 7, 16, 66, 0, 64 },
		{ 7, 16, 64, 0, 64 }, { 7, 16, 64, 0, 64 },
		{ 7, 16, 62, 0, 64 }, { 7, 15, 62, 0, 64 },
		{ 7, 15, 60, 0, 64 }, { 7, 15, 60, 0, 64 },
		{ 7, 15, 59, 0, 64 }, { 7, 14, 59, 0, 64 },
		{ 7, 14, 57, 0, 64 }, { 7, 14, 57, 0, 64 },
		{ 7, 14, 55, 0, 64 }, { 7, 14, 55, 0, 64 },
		{ 7, 14, 54, 0, 64 }, { 7, 13, 54, 0, 64 },
		{ 7, 13, 52, 0, 64 }, { 7, 13, 52, 0, 64 },
	};
	static struct bwn_txgain_entry txgain_5ghz_r2[] = {
		{ 255, 255, 255, 0, 152 }, { 255, 255, 255, 0, 147 },
		{ 255, 255, 255, 0, 143 }, { 255, 255, 255, 0, 139 },
		{ 255, 255, 255, 0, 135 }, { 255, 255, 255, 0, 131 },
		{ 255, 255, 255, 0, 128 }, { 255, 255, 255, 0, 124 },
		{ 255, 255, 255, 0, 121 }, { 255, 255, 255, 0, 117 },
		{ 255, 255, 255, 0, 114 }, { 255, 255, 255, 0, 111 },
		{ 255, 255, 255, 0, 107 }, { 255, 255, 255, 0, 104 },
		{ 255, 255, 255, 0, 101 }, { 255, 255, 255, 0, 99 },
		{ 255, 255, 255, 0, 96 }, { 255, 255, 255, 0, 93 },
		{ 255, 255, 255, 0, 90 }, { 255, 255, 255, 0, 88 },
		{ 255, 255, 255, 0, 85 }, { 255, 255, 255, 0, 83 },
		{ 255, 255, 255, 0, 81 }, { 255, 255, 255, 0, 78 },
		{ 255, 255, 255, 0, 76 }, { 255, 255, 255, 0, 74 },
		{ 255, 255, 255, 0, 72 }, { 255, 255, 255, 0, 70 },
		{ 255, 255, 255, 0, 68 }, { 255, 255, 255, 0, 66 },
		{ 255, 255, 255, 0, 64 }, { 255, 255, 248, 0, 64 },
		{ 255, 255, 241, 0, 64 }, { 255, 255, 234, 0, 64 },
		{ 255, 255, 227, 0, 64 }, { 255, 255, 221, 0, 64 },
		{ 255, 255, 215, 0, 64 }, { 255, 255, 208, 0, 64 },
		{ 255, 255, 203, 0, 64 }, { 255, 255, 197, 0, 64 },
		{ 255, 255, 191, 0, 64 }, { 255, 255, 186, 0, 64 },
		{ 255, 255, 181, 0, 64 }, { 255, 255, 175, 0, 64 },
		{ 255, 255, 170, 0, 64 }, { 255, 255, 166, 0, 64 },
		{ 255, 255, 161, 0, 64 }, { 255, 255, 156, 0, 64 },
		{ 255, 255, 152, 0, 64 }, { 255, 255, 148, 0, 64 },
		{ 255, 255, 143, 0, 64 }, { 255, 255, 139, 0, 64 },
		{ 255, 255, 135, 0, 64 }, { 255, 255, 132, 0, 64 },
		{ 255, 255, 128, 0, 64 }, { 255, 255, 124, 0, 64 },
		{ 255, 255, 121, 0, 64 }, { 255, 255, 117, 0, 64 },
		{ 255, 255, 114, 0, 64 }, { 255, 255, 111, 0, 64 },
		{ 255, 255, 108, 0, 64 }, { 255, 255, 104, 0, 64 },
		{ 255, 255, 102, 0, 64 }, { 255, 255, 99, 0, 64 },
		{ 255, 255, 96, 0, 64 }, { 255, 255, 93, 0, 64 },
		{ 255, 255, 90, 0, 64 }, { 255, 255, 88, 0, 64 },
		{ 255, 255, 85, 0, 64 }, { 255, 255, 83, 0, 64 },
		{ 255, 255, 81, 0, 64 }, { 255, 255, 78, 0, 64 },
		{ 255, 255, 76, 0, 64 }, { 255, 255, 74, 0, 64 },
		{ 255, 255, 72, 0, 64 }, { 255, 255, 70, 0, 64 },
		{ 255, 255, 68, 0, 64 }, { 255, 255, 66, 0, 64 },
		{ 255, 255, 64, 0, 64 }, { 255, 255, 64, 0, 64 },
		{ 255, 255, 62, 0, 64 }, { 255, 248, 62, 0, 64 },
		{ 255, 248, 60, 0, 64 }, { 255, 241, 60, 0, 64 },
		{ 255, 241, 59, 0, 64 }, { 255, 234, 59, 0, 64 },
		{ 255, 234, 57, 0, 64 }, { 255, 227, 57, 0, 64 },
		{ 255, 227, 55, 0, 64 }, { 255, 221, 55, 0, 64 },
		{ 255, 221, 54, 0, 64 }, { 255, 215, 54, 0, 64 },
		{ 255, 215, 52, 0, 64 }, { 255, 208, 52, 0, 64 },
		{ 255, 208, 51, 0, 64 }, { 255, 203, 51, 0, 64 },
		{ 255, 203, 49, 0, 64 }, { 255, 197, 49, 0, 64 },
		{ 255, 197, 48, 0, 64 }, { 255, 191, 48, 0, 64 },
		{ 255, 191, 47, 0, 64 }, { 255, 186, 47, 0, 64 },
		{ 255, 186, 45, 0, 64 }, { 255, 181, 45, 0, 64 },
		{ 255, 181, 44, 0, 64 }, { 255, 175, 44, 0, 64 },
		{ 255, 175, 43, 0, 64 }, { 255, 170, 43, 0, 64 },
		{ 255, 170, 42, 0, 64 }, { 255, 166, 42, 0, 64 },
		{ 255, 166, 40, 0, 64 }, { 255, 161, 40, 0, 64 },
		{ 255, 161, 39, 0, 64 }, { 255, 156, 39, 0, 64 },
		{ 255, 156, 38, 0, 64 }, { 255, 152, 38, 0, 64 },
		{ 255, 152, 37, 0, 64 }, { 255, 148, 37, 0, 64 },
		{ 255, 148, 36, 0, 64 }, { 255, 143, 36, 0, 64 },
		{ 255, 143, 35, 0, 64 }, { 255, 139, 35, 0, 64 },
		{ 255, 139, 34, 0, 64 }, { 255, 135, 34, 0, 64 },
		{ 255, 135, 33, 0, 64 }, { 255, 132, 33, 0, 64 },
		{ 255, 132, 32, 0, 64 }, { 255, 128, 32, 0, 64 }
	};
	static struct bwn_txgain_entry txgain_r0[] = {
		{ 7, 15, 14, 0, 152 }, { 7, 15, 14, 0, 147 },
		{ 7, 15, 14, 0, 143 }, { 7, 15, 14, 0, 139 },
		{ 7, 15, 14, 0, 135 }, { 7, 15, 14, 0, 131 },
		{ 7, 15, 14, 0, 128 }, { 7, 15, 14, 0, 124 },
		{ 7, 15, 14, 0, 121 }, { 7, 15, 14, 0, 117 },
		{ 7, 15, 14, 0, 114 }, { 7, 15, 14, 0, 111 },
		{ 7, 15, 14, 0, 107 }, { 7, 15, 14, 0, 104 },
		{ 7, 15, 14, 0, 101 }, { 7, 15, 14, 0, 99 },
		{ 7, 15, 14, 0, 96 }, { 7, 15, 14, 0, 93 },
		{ 7, 15, 14, 0, 90 }, { 7, 15, 14, 0, 88 },
		{ 7, 15, 14, 0, 85 }, { 7, 15, 14, 0, 83 },
		{ 7, 15, 14, 0, 81 }, { 7, 15, 14, 0, 78 },
		{ 7, 15, 14, 0, 76 }, { 7, 15, 14, 0, 74 },
		{ 7, 15, 14, 0, 72 }, { 7, 15, 14, 0, 70 },
		{ 7, 15, 14, 0, 68 }, { 7, 15, 14, 0, 66 },
		{ 7, 15, 14, 0, 64 }, { 7, 15, 14, 0, 62 },
		{ 7, 15, 14, 0, 60 }, { 7, 15, 14, 0, 59 },
		{ 7, 15, 14, 0, 57 }, { 7, 15, 13, 0, 72 },
		{ 7, 15, 13, 0, 70 }, { 7, 15, 13, 0, 68 },
		{ 7, 15, 13, 0, 66 }, { 7, 15, 13, 0, 64 },
		{ 7, 15, 13, 0, 62 }, { 7, 15, 13, 0, 60 },
		{ 7, 15, 13, 0, 59 }, { 7, 15, 13, 0, 57 },
		{ 7, 15, 12, 0, 71 }, { 7, 15, 12, 0, 69 },
		{ 7, 15, 12, 0, 67 }, { 7, 15, 12, 0, 65 },
		{ 7, 15, 12, 0, 63 }, { 7, 15, 12, 0, 62 },
		{ 7, 15, 12, 0, 60 }, { 7, 15, 12, 0, 58 },
		{ 7, 15, 12, 0, 57 }, { 7, 15, 11, 0, 70 },
		{ 7, 15, 11, 0, 68 }, { 7, 15, 11, 0, 66 },
		{ 7, 15, 11, 0, 65 }, { 7, 15, 11, 0, 63 },
		{ 7, 15, 11, 0, 61 }, { 7, 15, 11, 0, 59 },
		{ 7, 15, 11, 0, 58 }, { 7, 15, 10, 0, 71 },
		{ 7, 15, 10, 0, 69 }, { 7, 15, 10, 0, 67 },
		{ 7, 15, 10, 0, 65 }, { 7, 15, 10, 0, 63 },
		{ 7, 15, 10, 0, 61 }, { 7, 15, 10, 0, 60 },
		{ 7, 15, 10, 0, 58 }, { 7, 15, 10, 0, 56 },
		{ 7, 15, 9, 0, 70 }, { 7, 15, 9, 0, 68 },
		{ 7, 15, 9, 0, 66 }, { 7, 15, 9, 0, 64 },
		{ 7, 15, 9, 0, 62 }, { 7, 15, 9, 0, 60 },
		{ 7, 15, 9, 0, 59 }, { 7, 14, 9, 0, 72 },
		{ 7, 14, 9, 0, 70 }, { 7, 14, 9, 0, 68 },
		{ 7, 14, 9, 0, 66 }, { 7, 14, 9, 0, 64 },
		{ 7, 14, 9, 0, 62 }, { 7, 14, 9, 0, 60 },
		{ 7, 14, 9, 0, 59 }, { 7, 13, 9, 0, 72 },
		{ 7, 13, 9, 0, 70 }, { 7, 13, 9, 0, 68 },
		{ 7, 13, 9, 0, 66 }, { 7, 13, 9, 0, 64 },
		{ 7, 13, 9, 0, 63 }, { 7, 13, 9, 0, 61 },
		{ 7, 13, 9, 0, 59 }, { 7, 13, 9, 0, 57 },
		{ 7, 13, 8, 0, 72 }, { 7, 13, 8, 0, 70 },
		{ 7, 13, 8, 0, 68 }, { 7, 13, 8, 0, 66 },
		{ 7, 13, 8, 0, 64 }, { 7, 13, 8, 0, 62 },
		{ 7, 13, 8, 0, 60 }, { 7, 13, 8, 0, 59 },
		{ 7, 12, 8, 0, 72 }, { 7, 12, 8, 0, 70 },
		{ 7, 12, 8, 0, 68 }, { 7, 12, 8, 0, 66 },
		{ 7, 12, 8, 0, 64 }, { 7, 12, 8, 0, 62 },
		{ 7, 12, 8, 0, 61 }, { 7, 12, 8, 0, 59 },
		{ 7, 12, 7, 0, 73 }, { 7, 12, 7, 0, 71 },
		{ 7, 12, 7, 0, 69 }, { 7, 12, 7, 0, 67 },
		{ 7, 12, 7, 0, 65 }, { 7, 12, 7, 0, 63 },
		{ 7, 12, 7, 0, 61 }, { 7, 12, 7, 0, 59 },
		{ 7, 11, 7, 0, 72 }, { 7, 11, 7, 0, 70 },
		{ 7, 11, 7, 0, 68 }, { 7, 11, 7, 0, 66 },
		{ 7, 11, 7, 0, 65 }, { 7, 11, 7, 0, 63 },
		{ 7, 11, 7, 0, 61 }, { 7, 11, 7, 0, 59 },
		{ 7, 11, 6, 0, 73 }, { 7, 11, 6, 0, 71 }
	};
	static struct bwn_txgain_entry txgain_2ghz_r0[] = {
		{ 4, 15, 9, 0, 64 }, { 4, 15, 9, 0, 62 },
		{ 4, 15, 9, 0, 60 }, { 4, 15, 9, 0, 59 },
		{ 4, 14, 9, 0, 72 }, { 4, 14, 9, 0, 70 },
		{ 4, 14, 9, 0, 68 }, { 4, 14, 9, 0, 66 },
		{ 4, 14, 9, 0, 64 }, { 4, 14, 9, 0, 62 },
		{ 4, 14, 9, 0, 60 }, { 4, 14, 9, 0, 59 },
		{ 4, 13, 9, 0, 72 }, { 4, 13, 9, 0, 70 },
		{ 4, 13, 9, 0, 68 }, { 4, 13, 9, 0, 66 },
		{ 4, 13, 9, 0, 64 }, { 4, 13, 9, 0, 63 },
		{ 4, 13, 9, 0, 61 }, { 4, 13, 9, 0, 59 },
		{ 4, 13, 9, 0, 57 }, { 4, 13, 8, 0, 72 },
		{ 4, 13, 8, 0, 70 }, { 4, 13, 8, 0, 68 },
		{ 4, 13, 8, 0, 66 }, { 4, 13, 8, 0, 64 },
		{ 4, 13, 8, 0, 62 }, { 4, 13, 8, 0, 60 },
		{ 4, 13, 8, 0, 59 }, { 4, 12, 8, 0, 72 },
		{ 4, 12, 8, 0, 70 }, { 4, 12, 8, 0, 68 },
		{ 4, 12, 8, 0, 66 }, { 4, 12, 8, 0, 64 },
		{ 4, 12, 8, 0, 62 }, { 4, 12, 8, 0, 61 },
		{ 4, 12, 8, 0, 59 }, { 4, 12, 7, 0, 73 },
		{ 4, 12, 7, 0, 71 }, { 4, 12, 7, 0, 69 },
		{ 4, 12, 7, 0, 67 }, { 4, 12, 7, 0, 65 },
		{ 4, 12, 7, 0, 63 }, { 4, 12, 7, 0, 61 },
		{ 4, 12, 7, 0, 59 }, { 4, 11, 7, 0, 72 },
		{ 4, 11, 7, 0, 70 }, { 4, 11, 7, 0, 68 },
		{ 4, 11, 7, 0, 66 }, { 4, 11, 7, 0, 65 },
		{ 4, 11, 7, 0, 63 }, { 4, 11, 7, 0, 61 },
		{ 4, 11, 7, 0, 59 }, { 4, 11, 6, 0, 73 },
		{ 4, 11, 6, 0, 71 }, { 4, 11, 6, 0, 69 },
		{ 4, 11, 6, 0, 67 }, { 4, 11, 6, 0, 65 },
		{ 4, 11, 6, 0, 63 }, { 4, 11, 6, 0, 61 },
		{ 4, 11, 6, 0, 60 }, { 4, 10, 6, 0, 72 },
		{ 4, 10, 6, 0, 70 }, { 4, 10, 6, 0, 68 },
		{ 4, 10, 6, 0, 66 }, { 4, 10, 6, 0, 64 },
		{ 4, 10, 6, 0, 62 }, { 4, 10, 6, 0, 60 },
		{ 4, 10, 6, 0, 59 }, { 4, 10, 5, 0, 72 },
		{ 4, 10, 5, 0, 70 }, { 4, 10, 5, 0, 68 },
		{ 4, 10, 5, 0, 66 }, { 4, 10, 5, 0, 64 },
		{ 4, 10, 5, 0, 62 }, { 4, 10, 5, 0, 60 },
		{ 4, 10, 5, 0, 59 }, { 4, 9, 5, 0, 70 },
		{ 4, 9, 5, 0, 68 }, { 4, 9, 5, 0, 66 },
		{ 4, 9, 5, 0, 64 }, { 4, 9, 5, 0, 63 },
		{ 4, 9, 5, 0, 61 }, { 4, 9, 5, 0, 59 },
		{ 4, 9, 4, 0, 71 }, { 4, 9, 4, 0, 69 },
		{ 4, 9, 4, 0, 67 }, { 4, 9, 4, 0, 65 },
		{ 4, 9, 4, 0, 63 }, { 4, 9, 4, 0, 62 },
		{ 4, 9, 4, 0, 60 }, { 4, 9, 4, 0, 58 },
		{ 4, 8, 4, 0, 70 }, { 4, 8, 4, 0, 68 },
		{ 4, 8, 4, 0, 66 }, { 4, 8, 4, 0, 65 },
		{ 4, 8, 4, 0, 63 }, { 4, 8, 4, 0, 61 },
		{ 4, 8, 4, 0, 59 }, { 4, 7, 4, 0, 68 },
		{ 4, 7, 4, 0, 66 }, { 4, 7, 4, 0, 64 },
		{ 4, 7, 4, 0, 62 }, { 4, 7, 4, 0, 61 },
		{ 4, 7, 4, 0, 59 }, { 4, 7, 3, 0, 67 },
		{ 4, 7, 3, 0, 65 }, { 4, 7, 3, 0, 63 },
		{ 4, 7, 3, 0, 62 }, { 4, 7, 3, 0, 60 },
		{ 4, 6, 3, 0, 65 }, { 4, 6, 3, 0, 63 },
		{ 4, 6, 3, 0, 61 }, { 4, 6, 3, 0, 60 },
		{ 4, 6, 3, 0, 58 }, { 4, 5, 3, 0, 68 },
		{ 4, 5, 3, 0, 66 }, { 4, 5, 3, 0, 64 },
		{ 4, 5, 3, 0, 62 }, { 4, 5, 3, 0, 60 },
		{ 4, 5, 3, 0, 59 }, { 4, 5, 3, 0, 57 },
		{ 4, 4, 2, 0, 83 }, { 4, 4, 2, 0, 81 },
		{ 4, 4, 2, 0, 78 }, { 4, 4, 2, 0, 76 },
		{ 4, 4, 2, 0, 74 }, { 4, 4, 2, 0, 72 }
	};
	static struct bwn_txgain_entry txgain_5ghz_r0[] = {
		{ 7, 15, 15, 0, 99 }, { 7, 15, 15, 0, 96 },
		{ 7, 15, 15, 0, 93 }, { 7, 15, 15, 0, 90 },
		{ 7, 15, 15, 0, 88 }, { 7, 15, 15, 0, 85 },
		{ 7, 15, 15, 0, 83 }, { 7, 15, 15, 0, 81 },
		{ 7, 15, 15, 0, 78 }, { 7, 15, 15, 0, 76 },
		{ 7, 15, 15, 0, 74 }, { 7, 15, 15, 0, 72 },
		{ 7, 15, 15, 0, 70 }, { 7, 15, 15, 0, 68 },
		{ 7, 15, 15, 0, 66 }, { 7, 15, 15, 0, 64 },
		{ 7, 15, 15, 0, 62 }, { 7, 15, 15, 0, 60 },
		{ 7, 15, 15, 0, 59 }, { 7, 15, 15, 0, 57 },
		{ 7, 15, 15, 0, 55 }, { 7, 15, 14, 0, 72 },
		{ 7, 15, 14, 0, 70 }, { 7, 15, 14, 0, 68 },
		{ 7, 15, 14, 0, 66 }, { 7, 15, 14, 0, 64 },
		{ 7, 15, 14, 0, 62 }, { 7, 15, 14, 0, 60 },
		{ 7, 15, 14, 0, 58 }, { 7, 15, 14, 0, 56 },
		{ 7, 15, 14, 0, 55 }, { 7, 15, 13, 0, 71 },
		{ 7, 15, 13, 0, 69 }, { 7, 15, 13, 0, 67 },
		{ 7, 15, 13, 0, 65 }, { 7, 15, 13, 0, 63 },
		{ 7, 15, 13, 0, 62 }, { 7, 15, 13, 0, 60 },
		{ 7, 15, 13, 0, 58 }, { 7, 15, 13, 0, 56 },
		{ 7, 15, 12, 0, 72 }, { 7, 15, 12, 0, 70 },
		{ 7, 15, 12, 0, 68 }, { 7, 15, 12, 0, 66 },
		{ 7, 15, 12, 0, 64 }, { 7, 15, 12, 0, 62 },
		{ 7, 15, 12, 0, 60 }, { 7, 15, 12, 0, 59 },
		{ 7, 15, 12, 0, 57 }, { 7, 15, 11, 0, 73 },
		{ 7, 15, 11, 0, 71 }, { 7, 15, 11, 0, 69 },
		{ 7, 15, 11, 0, 67 }, { 7, 15, 11, 0, 65 },
		{ 7, 15, 11, 0, 63 }, { 7, 15, 11, 0, 61 },
		{ 7, 15, 11, 0, 60 }, { 7, 15, 11, 0, 58 },
		{ 7, 15, 10, 0, 71 }, { 7, 15, 10, 0, 69 },
		{ 7, 15, 10, 0, 67 }, { 7, 15, 10, 0, 65 },
		{ 7, 15, 10, 0, 63 }, { 7, 15, 10, 0, 61 },
		{ 7, 15, 10, 0, 60 }, { 7, 15, 10, 0, 58 },
		{ 7, 15, 9, 0, 70 }, { 7, 15, 9, 0, 68 },
		{ 7, 15, 9, 0, 66 }, { 7, 15, 9, 0, 64 },
		{ 7, 15, 9, 0, 62 }, { 7, 15, 9, 0, 61 },
		{ 7, 15, 9, 0, 59 }, { 7, 15, 9, 0, 57 },
		{ 7, 15, 9, 0, 56 }, { 7, 14, 9, 0, 68 },
		{ 7, 14, 9, 0, 66 }, { 7, 14, 9, 0, 65 },
		{ 7, 14, 9, 0, 63 }, { 7, 14, 9, 0, 61 },
		{ 7, 14, 9, 0, 59 }, { 7, 14, 9, 0, 58 },
		{ 7, 13, 9, 0, 70 }, { 7, 13, 9, 0, 68 },
		{ 7, 13, 9, 0, 66 }, { 7, 13, 9, 0, 64 },
		{ 7, 13, 9, 0, 63 }, { 7, 13, 9, 0, 61 },
		{ 7, 13, 9, 0, 59 }, { 7, 13, 9, 0, 57 },
		{ 7, 13, 8, 0, 70 }, { 7, 13, 8, 0, 68 },
		{ 7, 13, 8, 0, 66 }, { 7, 13, 8, 0, 64 },
		{ 7, 13, 8, 0, 62 }, { 7, 13, 8, 0, 60 },
		{ 7, 13, 8, 0, 59 }, { 7, 13, 8, 0, 57 },
		{ 7, 12, 8, 0, 70 }, { 7, 12, 8, 0, 68 },
		{ 7, 12, 8, 0, 66 }, { 7, 12, 8, 0, 64 },
		{ 7, 12, 8, 0, 62 }, { 7, 12, 8, 0, 61 },
		{ 7, 12, 8, 0, 59 }, { 7, 12, 8, 0, 57 },
		{ 7, 12, 7, 0, 70 }, { 7, 12, 7, 0, 68 },
		{ 7, 12, 7, 0, 66 }, { 7, 12, 7, 0, 64 },
		{ 7, 12, 7, 0, 62 }, { 7, 12, 7, 0, 61 },
		{ 7, 12, 7, 0, 59 }, { 7, 12, 7, 0, 57 },
		{ 7, 11, 7, 0, 70 }, { 7, 11, 7, 0, 68 },
		{ 7, 11, 7, 0, 66 }, { 7, 11, 7, 0, 64 },
		{ 7, 11, 7, 0, 62 }, { 7, 11, 7, 0, 61 },
		{ 7, 11, 7, 0, 59 }, { 7, 11, 7, 0, 57 },
		{ 7, 11, 6, 0, 69 }, { 7, 11, 6, 0, 67 },
		{ 7, 11, 6, 0, 65 }, { 7, 11, 6, 0, 63 },
		{ 7, 11, 6, 0, 62 }, { 7, 11, 6, 0, 60 }
	};
	static struct bwn_txgain_entry txgain_r1[] = {
		{ 7, 15, 14, 0, 152 }, { 7, 15, 14, 0, 147 },
		{ 7, 15, 14, 0, 143 }, { 7, 15, 14, 0, 139 },
		{ 7, 15, 14, 0, 135 }, { 7, 15, 14, 0, 131 },
		{ 7, 15, 14, 0, 128 }, { 7, 15, 14, 0, 124 },
		{ 7, 15, 14, 0, 121 }, { 7, 15, 14, 0, 117 },
		{ 7, 15, 14, 0, 114 }, { 7, 15, 14, 0, 111 },
		{ 7, 15, 14, 0, 107 }, { 7, 15, 14, 0, 104 },
		{ 7, 15, 14, 0, 101 }, { 7, 15, 14, 0, 99 },
		{ 7, 15, 14, 0, 96 }, { 7, 15, 14, 0, 93 },
		{ 7, 15, 14, 0, 90 }, { 7, 15, 14, 0, 88 },
		{ 7, 15, 14, 0, 85 }, { 7, 15, 14, 0, 83 },
		{ 7, 15, 14, 0, 81 }, { 7, 15, 14, 0, 78 },
		{ 7, 15, 14, 0, 76 }, { 7, 15, 14, 0, 74 },
		{ 7, 15, 14, 0, 72 }, { 7, 15, 14, 0, 70 },
		{ 7, 15, 14, 0, 68 }, { 7, 15, 14, 0, 66 },
		{ 7, 15, 14, 0, 64 }, { 7, 15, 14, 0, 62 },
		{ 7, 15, 14, 0, 60 }, { 7, 15, 14, 0, 59 },
		{ 7, 15, 14, 0, 57 }, { 7, 15, 13, 0, 72 },
		{ 7, 15, 13, 0, 70 }, { 7, 15, 14, 0, 68 },
		{ 7, 15, 14, 0, 66 }, { 7, 15, 14, 0, 64 },
		{ 7, 15, 14, 0, 62 }, { 7, 15, 14, 0, 60 },
		{ 7, 15, 14, 0, 59 }, { 7, 15, 14, 0, 57 },
		{ 7, 15, 13, 0, 72 }, { 7, 15, 13, 0, 70 },
		{ 7, 15, 13, 0, 68 }, { 7, 15, 13, 0, 66 },
		{ 7, 15, 13, 0, 64 }, { 7, 15, 13, 0, 62 },
		{ 7, 15, 13, 0, 60 }, { 7, 15, 13, 0, 59 },
		{ 7, 15, 13, 0, 57 }, { 7, 15, 12, 0, 71 },
		{ 7, 15, 12, 0, 69 }, { 7, 15, 12, 0, 67 },
		{ 7, 15, 12, 0, 65 }, { 7, 15, 12, 0, 63 },
		{ 7, 15, 12, 0, 62 }, { 7, 15, 12, 0, 60 },
		{ 7, 15, 12, 0, 58 }, { 7, 15, 12, 0, 57 },
		{ 7, 15, 11, 0, 70 }, { 7, 15, 11, 0, 68 },
		{ 7, 15, 11, 0, 66 }, { 7, 15, 11, 0, 65 },
		{ 7, 15, 11, 0, 63 }, { 7, 15, 11, 0, 61 },
		{ 7, 15, 11, 0, 59 }, { 7, 15, 11, 0, 58 },
		{ 7, 15, 10, 0, 71 }, { 7, 15, 10, 0, 69 },
		{ 7, 15, 10, 0, 67 }, { 7, 15, 10, 0, 65 },
		{ 7, 15, 10, 0, 63 }, { 7, 15, 10, 0, 61 },
		{ 7, 15, 10, 0, 60 }, { 7, 15, 10, 0, 58 },
		{ 7, 15, 10, 0, 56 }, { 7, 15, 9, 0, 70 },
		{ 7, 15, 9, 0, 68 }, { 7, 15, 9, 0, 66 },
		{ 7, 15, 9, 0, 64 }, { 7, 15, 9, 0, 62 },
		{ 7, 15, 9, 0, 60 }, { 7, 15, 9, 0, 59 },
		{ 7, 14, 9, 0, 72 }, { 7, 14, 9, 0, 70 },
		{ 7, 14, 9, 0, 68 }, { 7, 14, 9, 0, 66 },
		{ 7, 14, 9, 0, 64 }, { 7, 14, 9, 0, 62 },
		{ 7, 14, 9, 0, 60 }, { 7, 14, 9, 0, 59 },
		{ 7, 13, 9, 0, 72 }, { 7, 13, 9, 0, 70 },
		{ 7, 13, 9, 0, 68 }, { 7, 13, 9, 0, 66 },
		{ 7, 13, 9, 0, 64 }, { 7, 13, 9, 0, 63 },
		{ 7, 13, 9, 0, 61 }, { 7, 13, 9, 0, 59 },
		{ 7, 13, 9, 0, 57 }, { 7, 13, 8, 0, 72 },
		{ 7, 13, 8, 0, 70 }, { 7, 13, 8, 0, 68 },
		{ 7, 13, 8, 0, 66 }, { 7, 13, 8, 0, 64 },
		{ 7, 13, 8, 0, 62 }, { 7, 13, 8, 0, 60 },
		{ 7, 13, 8, 0, 59 }, { 7, 12, 8, 0, 72 },
		{ 7, 12, 8, 0, 70 }, { 7, 12, 8, 0, 68 },
		{ 7, 12, 8, 0, 66 }, { 7, 12, 8, 0, 64 },
		{ 7, 12, 8, 0, 62 }, { 7, 12, 8, 0, 61 },
		{ 7, 12, 8, 0, 59 }, { 7, 12, 7, 0, 73 },
		{ 7, 12, 7, 0, 71 }, { 7, 12, 7, 0, 69 },
		{ 7, 12, 7, 0, 67 }, { 7, 12, 7, 0, 65 },
		{ 7, 12, 7, 0, 63 }, { 7, 12, 7, 0, 61 },
		{ 7, 12, 7, 0, 59 }, { 7, 11, 7, 0, 72 },
		{ 7, 11, 7, 0, 70 }, { 7, 11, 7, 0, 68 },
		{ 7, 11, 7, 0, 66 }, { 7, 11, 7, 0, 65 },
		{ 7, 11, 7, 0, 63 }, { 7, 11, 7, 0, 61 },
		{ 7, 11, 7, 0, 59 }, { 7, 11, 6, 0, 73 },
		{ 7, 11, 6, 0, 71 }
	};
	static struct bwn_txgain_entry txgain_2ghz_r1[] = {
		{ 4, 15, 15, 0, 90 }, { 4, 15, 15, 0, 88 },
		{ 4, 15, 15, 0, 85 }, { 4, 15, 15, 0, 83 },
		{ 4, 15, 15, 0, 81 }, { 4, 15, 15, 0, 78 },
		{ 4, 15, 15, 0, 76 }, { 4, 15, 15, 0, 74 },
		{ 4, 15, 15, 0, 72 }, { 4, 15, 15, 0, 70 },
		{ 4, 15, 15, 0, 68 }, { 4, 15, 15, 0, 66 },
		{ 4, 15, 15, 0, 64 }, { 4, 15, 15, 0, 62 },
		{ 4, 15, 15, 0, 60 }, { 4, 15, 15, 0, 59 },
		{ 4, 15, 14, 0, 72 }, { 4, 15, 14, 0, 70 },
		{ 4, 15, 14, 0, 68 }, { 4, 15, 14, 0, 66 },
		{ 4, 15, 14, 0, 64 }, { 4, 15, 14, 0, 62 },
		{ 4, 15, 14, 0, 60 }, { 4, 15, 14, 0, 59 },
		{ 4, 15, 13, 0, 72 }, { 4, 15, 13, 0, 70 },
		{ 4, 15, 13, 0, 68 }, { 4, 15, 13, 0, 66 },
		{ 4, 15, 13, 0, 64 }, { 4, 15, 13, 0, 62 },
		{ 4, 15, 13, 0, 60 }, { 4, 15, 13, 0, 59 },
		{ 4, 15, 12, 0, 72 }, { 4, 15, 12, 0, 70 },
		{ 4, 15, 12, 0, 68 }, { 4, 15, 12, 0, 66 },
		{ 4, 15, 12, 0, 64 }, { 4, 15, 12, 0, 62 },
		{ 4, 15, 12, 0, 60 }, { 4, 15, 12, 0, 59 },
		{ 4, 15, 11, 0, 72 }, { 4, 15, 11, 0, 70 },
		{ 4, 15, 11, 0, 68 }, { 4, 15, 11, 0, 66 },
		{ 4, 15, 11, 0, 64 }, { 4, 15, 11, 0, 62 },
		{ 4, 15, 11, 0, 60 }, { 4, 15, 11, 0, 59 },
		{ 4, 15, 10, 0, 72 }, { 4, 15, 10, 0, 70 },
		{ 4, 15, 10, 0, 68 }, { 4, 15, 10, 0, 66 },
		{ 4, 15, 10, 0, 64 }, { 4, 15, 10, 0, 62 },
		{ 4, 15, 10, 0, 60 }, { 4, 15, 10, 0, 59 },
		{ 4, 15, 9, 0, 72 }, { 4, 15, 9, 0, 70 },
		{ 4, 15, 9, 0, 68 }, { 4, 15, 9, 0, 66 },
		{ 4, 15, 9, 0, 64 }, { 4, 15, 9, 0, 62 },
		{ 4, 15, 9, 0, 60 }, { 4, 15, 9, 0, 59 },
		{ 4, 14, 9, 0, 72 }, { 4, 14, 9, 0, 70 },
		{ 4, 14, 9, 0, 68 }, { 4, 14, 9, 0, 66 },
		{ 4, 14, 9, 0, 64 }, { 4, 14, 9, 0, 62 },
		{ 4, 14, 9, 0, 60 }, { 4, 14, 9, 0, 59 },
		{ 4, 13, 9, 0, 72 }, { 4, 13, 9, 0, 70 },
		{ 4, 13, 9, 0, 68 }, { 4, 13, 9, 0, 66 },
		{ 4, 13, 9, 0, 64 }, { 4, 13, 9, 0, 63 },
		{ 4, 13, 9, 0, 61 }, { 4, 13, 9, 0, 59 },
		{ 4, 13, 9, 0, 57 }, { 4, 13, 8, 0, 72 },
		{ 4, 13, 8, 0, 70 }, { 4, 13, 8, 0, 68 },
		{ 4, 13, 8, 0, 66 }, { 4, 13, 8, 0, 64 },
		{ 4, 13, 8, 0, 62 }, { 4, 13, 8, 0, 60 },
		{ 4, 13, 8, 0, 59 }, { 4, 12, 8, 0, 72 },
		{ 4, 12, 8, 0, 70 }, { 4, 12, 8, 0, 68 },
		{ 4, 12, 8, 0, 66 }, { 4, 12, 8, 0, 64 },
		{ 4, 12, 8, 0, 62 }, { 4, 12, 8, 0, 61 },
		{ 4, 12, 8, 0, 59 }, { 4, 12, 7, 0, 73 },
		{ 4, 12, 7, 0, 71 }, { 4, 12, 7, 0, 69 },
		{ 4, 12, 7, 0, 67 }, { 4, 12, 7, 0, 65 },
		{ 4, 12, 7, 0, 63 }, { 4, 12, 7, 0, 61 },
		{ 4, 12, 7, 0, 59 }, { 4, 11, 7, 0, 72 },
		{ 4, 11, 7, 0, 70 }, { 4, 11, 7, 0, 68 },
		{ 4, 11, 7, 0, 66 }, { 4, 11, 7, 0, 65 },
		{ 4, 11, 7, 0, 63 }, { 4, 11, 7, 0, 61 },
		{ 4, 11, 7, 0, 59 }, { 4, 11, 6, 0, 73 },
		{ 4, 11, 6, 0, 71 }, { 4, 11, 6, 0, 69 },
		{ 4, 11, 6, 0, 67 }, { 4, 11, 6, 0, 65 },
		{ 4, 11, 6, 0, 63 }, { 4, 11, 6, 0, 61 },
		{ 4, 11, 6, 0, 60 }, { 4, 10, 6, 0, 72 },
		{ 4, 10, 6, 0, 70 }, { 4, 10, 6, 0, 68 },
		{ 4, 10, 6, 0, 66 }, { 4, 10, 6, 0, 64 },
		{ 4, 10, 6, 0, 62 }, { 4, 10, 6, 0, 60 }
	};
	static struct bwn_txgain_entry txgain_5ghz_r1[] = {
		{ 7, 15, 15, 0, 99 }, { 7, 15, 15, 0, 96 },
		{ 7, 15, 15, 0, 93 }, { 7, 15, 15, 0, 90 },
		{ 7, 15, 15, 0, 88 }, { 7, 15, 15, 0, 85 },
		{ 7, 15, 15, 0, 83 }, { 7, 15, 15, 0, 81 },
		{ 7, 15, 15, 0, 78 }, { 7, 15, 15, 0, 76 },
		{ 7, 15, 15, 0, 74 }, { 7, 15, 15, 0, 72 },
		{ 7, 15, 15, 0, 70 }, { 7, 15, 15, 0, 68 },
		{ 7, 15, 15, 0, 66 }, { 7, 15, 15, 0, 64 },
		{ 7, 15, 15, 0, 62 }, { 7, 15, 15, 0, 60 },
		{ 7, 15, 15, 0, 59 }, { 7, 15, 15, 0, 57 },
		{ 7, 15, 15, 0, 55 }, { 7, 15, 14, 0, 72 },
		{ 7, 15, 14, 0, 70 }, { 7, 15, 14, 0, 68 },
		{ 7, 15, 14, 0, 66 }, { 7, 15, 14, 0, 64 },
		{ 7, 15, 14, 0, 62 }, { 7, 15, 14, 0, 60 },
		{ 7, 15, 14, 0, 58 }, { 7, 15, 14, 0, 56 },
		{ 7, 15, 14, 0, 55 }, { 7, 15, 13, 0, 71 },
		{ 7, 15, 13, 0, 69 }, { 7, 15, 13, 0, 67 },
		{ 7, 15, 13, 0, 65 }, { 7, 15, 13, 0, 63 },
		{ 7, 15, 13, 0, 62 }, { 7, 15, 13, 0, 60 },
		{ 7, 15, 13, 0, 58 }, { 7, 15, 13, 0, 56 },
		{ 7, 15, 12, 0, 72 }, { 7, 15, 12, 0, 70 },
		{ 7, 15, 12, 0, 68 }, { 7, 15, 12, 0, 66 },
		{ 7, 15, 12, 0, 64 }, { 7, 15, 12, 0, 62 },
		{ 7, 15, 12, 0, 60 }, { 7, 15, 12, 0, 59 },
		{ 7, 15, 12, 0, 57 }, { 7, 15, 11, 0, 73 },
		{ 7, 15, 11, 0, 71 }, { 7, 15, 11, 0, 69 },
		{ 7, 15, 11, 0, 67 }, { 7, 15, 11, 0, 65 },
		{ 7, 15, 11, 0, 63 }, { 7, 15, 11, 0, 61 },
		{ 7, 15, 11, 0, 60 }, { 7, 15, 11, 0, 58 },
		{ 7, 15, 10, 0, 71 }, { 7, 15, 10, 0, 69 },
		{ 7, 15, 10, 0, 67 }, { 7, 15, 10, 0, 65 },
		{ 7, 15, 10, 0, 63 }, { 7, 15, 10, 0, 61 },
		{ 7, 15, 10, 0, 60 }, { 7, 15, 10, 0, 58 },
		{ 7, 15, 9, 0, 70 }, { 7, 15, 9, 0, 68 },
		{ 7, 15, 9, 0, 66 }, { 7, 15, 9, 0, 64 },
		{ 7, 15, 9, 0, 62 }, { 7, 15, 9, 0, 61 },
		{ 7, 15, 9, 0, 59 }, { 7, 15, 9, 0, 57 },
		{ 7, 15, 9, 0, 56 }, { 7, 14, 9, 0, 68 },
		{ 7, 14, 9, 0, 66 }, { 7, 14, 9, 0, 65 },
		{ 7, 14, 9, 0, 63 }, { 7, 14, 9, 0, 61 },
		{ 7, 14, 9, 0, 59 }, { 7, 14, 9, 0, 58 },
		{ 7, 13, 9, 0, 70 }, { 7, 13, 9, 0, 68 },
		{ 7, 13, 9, 0, 66 }, { 7, 13, 9, 0, 64 },
		{ 7, 13, 9, 0, 63 }, { 7, 13, 9, 0, 61 },
		{ 7, 13, 9, 0, 59 }, { 7, 13, 9, 0, 57 },
		{ 7, 13, 8, 0, 70 }, { 7, 13, 8, 0, 68 },
		{ 7, 13, 8, 0, 66 }, { 7, 13, 8, 0, 64 },
		{ 7, 13, 8, 0, 62 }, { 7, 13, 8, 0, 60 },
		{ 7, 13, 8, 0, 59 }, { 7, 13, 8, 0, 57 },
		{ 7, 12, 8, 0, 70 }, { 7, 12, 8, 0, 68 },
		{ 7, 12, 8, 0, 66 }, { 7, 12, 8, 0, 64 },
		{ 7, 12, 8, 0, 62 }, { 7, 12, 8, 0, 61 },
		{ 7, 12, 8, 0, 59 }, { 7, 12, 8, 0, 57 },
		{ 7, 12, 7, 0, 70 }, { 7, 12, 7, 0, 68 },
		{ 7, 12, 7, 0, 66 }, { 7, 12, 7, 0, 64 },
		{ 7, 12, 7, 0, 62 }, { 7, 12, 7, 0, 61 },
		{ 7, 12, 7, 0, 59 }, { 7, 12, 7, 0, 57 },
		{ 7, 11, 7, 0, 70 }, { 7, 11, 7, 0, 68 },
		{ 7, 11, 7, 0, 66 }, { 7, 11, 7, 0, 64 },
		{ 7, 11, 7, 0, 62 }, { 7, 11, 7, 0, 61 },
		{ 7, 11, 7, 0, 59 }, { 7, 11, 7, 0, 57 },
		{ 7, 11, 6, 0, 69 }, { 7, 11, 6, 0, 67 },
		{ 7, 11, 6, 0, 65 }, { 7, 11, 6, 0, 63 },
		{ 7, 11, 6, 0, 62 }, { 7, 11, 6, 0, 60 }
	};

	if (mac->mac_phy.rev != 0 && mac->mac_phy.rev != 1) {
		if (siba_sprom_get_bf_hi(sc->sc_dev) & BWN_BFH_NOPA)
			bwn_phy_lp_gaintbl_write_multi(mac, 0, 128, txgain_r2);
		else if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			bwn_phy_lp_gaintbl_write_multi(mac, 0, 128,
			    txgain_2ghz_r2);
		else
			bwn_phy_lp_gaintbl_write_multi(mac, 0, 128,
			    txgain_5ghz_r2);
		return;
	}

	if (mac->mac_phy.rev == 0) {
		if ((siba_sprom_get_bf_hi(sc->sc_dev) & BWN_BFH_NOPA) ||
		    (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_HGPA))
			bwn_phy_lp_gaintbl_write_multi(mac, 0, 128, txgain_r0);
		else if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
			bwn_phy_lp_gaintbl_write_multi(mac, 0, 128,
			    txgain_2ghz_r0);
		else
			bwn_phy_lp_gaintbl_write_multi(mac, 0, 128,
			    txgain_5ghz_r0);
		return;
	}

	if ((siba_sprom_get_bf_hi(sc->sc_dev) & BWN_BFH_NOPA) ||
	    (siba_sprom_get_bf_lo(sc->sc_dev) & BWN_BFL_HGPA))
		bwn_phy_lp_gaintbl_write_multi(mac, 0, 128, txgain_r1);
	else if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
		bwn_phy_lp_gaintbl_write_multi(mac, 0, 128, txgain_2ghz_r1);
	else
		bwn_phy_lp_gaintbl_write_multi(mac, 0, 128, txgain_5ghz_r1);
}

static void
bwn_tab_write(struct bwn_mac *mac, uint32_t typeoffset, uint32_t value)
{
	uint32_t offset, type;

	type = BWN_TAB_GETTYPE(typeoffset);
	offset = BWN_TAB_GETOFFSET(typeoffset);
	KASSERT(offset <= 0xffff, ("%s:%d: fail", __func__, __LINE__));

	switch (type) {
	case BWN_TAB_8BIT:
		KASSERT(!(value & ~0xff), ("%s:%d: fail", __func__, __LINE__));
		BWN_PHY_WRITE(mac, BWN_PHY_TABLE_ADDR, offset);
		BWN_PHY_WRITE(mac, BWN_PHY_TABLEDATALO, value);
		break;
	case BWN_TAB_16BIT:
		KASSERT(!(value & ~0xffff),
		    ("%s:%d: fail", __func__, __LINE__));
		BWN_PHY_WRITE(mac, BWN_PHY_TABLE_ADDR, offset);
		BWN_PHY_WRITE(mac, BWN_PHY_TABLEDATALO, value);
		break;
	case BWN_TAB_32BIT:
		BWN_PHY_WRITE(mac, BWN_PHY_TABLE_ADDR, offset);
		BWN_PHY_WRITE(mac, BWN_PHY_TABLEDATAHI, value >> 16);
		BWN_PHY_WRITE(mac, BWN_PHY_TABLEDATALO, value);
		break;
	default:
		KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
	}
}

static int
bwn_phy_lp_loopback(struct bwn_mac *mac)
{
	struct bwn_phy_lp_iq_est ie;
	int i, index = -1;
	uint32_t tmp;

	memset(&ie, 0, sizeof(ie));

	bwn_phy_lp_set_trsw_over(mac, 1, 1);
	BWN_PHY_SET(mac, BWN_PHY_AFE_CTL_OVR, 1);
	BWN_PHY_MASK(mac, BWN_PHY_AFE_CTL_OVRVAL, 0xfffe);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x800);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0x800);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x8);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0x8);
	BWN_RF_WRITE(mac, BWN_B2062_N_TXCTL_A, 0x80);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_0, 0x80);
	BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_VAL_0, 0x80);
	for (i = 0; i < 32; i++) {
		bwn_phy_lp_set_rxgain_idx(mac, i);
		bwn_phy_lp_ddfs_turnon(mac, 1, 1, 5, 5, 0);
		if (!(bwn_phy_lp_rx_iq_est(mac, 1000, 32, &ie)))
			continue;
		tmp = (ie.ie_ipwr + ie.ie_qpwr) / 1000;
		if ((tmp > 4000) && (tmp < 10000)) {
			index = i;
			break;
		}
	}
	bwn_phy_lp_ddfs_turnoff(mac);
	return (index);
}

static void
bwn_phy_lp_set_rxgain_idx(struct bwn_mac *mac, uint16_t idx)
{

	bwn_phy_lp_set_rxgain(mac, bwn_tab_read(mac, BWN_TAB_2(12, idx)));
}

static void
bwn_phy_lp_ddfs_turnon(struct bwn_mac *mac, int i_on, int q_on,
    int incr1, int incr2, int scale_idx)
{

	bwn_phy_lp_ddfs_turnoff(mac);
	BWN_PHY_MASK(mac, BWN_PHY_AFE_DDFS_POINTER_INIT, 0xff80);
	BWN_PHY_MASK(mac, BWN_PHY_AFE_DDFS_POINTER_INIT, 0x80ff);
	BWN_PHY_SETMASK(mac, BWN_PHY_AFE_DDFS_INCR_INIT, 0xff80, incr1);
	BWN_PHY_SETMASK(mac, BWN_PHY_AFE_DDFS_INCR_INIT, 0x80ff, incr2 << 8);
	BWN_PHY_SETMASK(mac, BWN_PHY_AFE_DDFS, 0xfff7, i_on << 3);
	BWN_PHY_SETMASK(mac, BWN_PHY_AFE_DDFS, 0xffef, q_on << 4);
	BWN_PHY_SETMASK(mac, BWN_PHY_AFE_DDFS, 0xff9f, scale_idx << 5);
	BWN_PHY_MASK(mac, BWN_PHY_AFE_DDFS, 0xfffb);
	BWN_PHY_SET(mac, BWN_PHY_AFE_DDFS, 0x2);
	BWN_PHY_SET(mac, BWN_PHY_LP_PHY_CTL, 0x20);
}

static uint8_t
bwn_phy_lp_rx_iq_est(struct bwn_mac *mac, uint16_t sample, uint8_t time,
    struct bwn_phy_lp_iq_est *ie)
{
	int i;

	BWN_PHY_MASK(mac, BWN_PHY_CRSGAIN_CTL, 0xfff7);
	BWN_PHY_WRITE(mac, BWN_PHY_IQ_NUM_SMPLS_ADDR, sample);
	BWN_PHY_SETMASK(mac, BWN_PHY_IQ_ENABLE_WAIT_TIME_ADDR, 0xff00, time);
	BWN_PHY_MASK(mac, BWN_PHY_IQ_ENABLE_WAIT_TIME_ADDR, 0xfeff);
	BWN_PHY_SET(mac, BWN_PHY_IQ_ENABLE_WAIT_TIME_ADDR, 0x200);

	for (i = 0; i < 500; i++) {
		if (!(BWN_PHY_READ(mac,
		    BWN_PHY_IQ_ENABLE_WAIT_TIME_ADDR) & 0x200))
			break;
		DELAY(1000);
	}
	if ((BWN_PHY_READ(mac, BWN_PHY_IQ_ENABLE_WAIT_TIME_ADDR) & 0x200)) {
		BWN_PHY_SET(mac, BWN_PHY_CRSGAIN_CTL, 0x8);
		return 0;
	}

	ie->ie_iqprod = BWN_PHY_READ(mac, BWN_PHY_IQ_ACC_HI_ADDR);
	ie->ie_iqprod <<= 16;
	ie->ie_iqprod |= BWN_PHY_READ(mac, BWN_PHY_IQ_ACC_LO_ADDR);
	ie->ie_ipwr = BWN_PHY_READ(mac, BWN_PHY_IQ_I_PWR_ACC_HI_ADDR);
	ie->ie_ipwr <<= 16;
	ie->ie_ipwr |= BWN_PHY_READ(mac, BWN_PHY_IQ_I_PWR_ACC_LO_ADDR);
	ie->ie_qpwr = BWN_PHY_READ(mac, BWN_PHY_IQ_Q_PWR_ACC_HI_ADDR);
	ie->ie_qpwr <<= 16;
	ie->ie_qpwr |= BWN_PHY_READ(mac, BWN_PHY_IQ_Q_PWR_ACC_LO_ADDR);

	BWN_PHY_SET(mac, BWN_PHY_CRSGAIN_CTL, 0x8);
	return 1;
}

static uint32_t
bwn_tab_read(struct bwn_mac *mac, uint32_t typeoffset)
{
	uint32_t offset, type, value;

	type = BWN_TAB_GETTYPE(typeoffset);
	offset = BWN_TAB_GETOFFSET(typeoffset);
	KASSERT(offset <= 0xffff, ("%s:%d: fail", __func__, __LINE__));

	switch (type) {
	case BWN_TAB_8BIT:
		BWN_PHY_WRITE(mac, BWN_PHY_TABLE_ADDR, offset);
		value = BWN_PHY_READ(mac, BWN_PHY_TABLEDATALO) & 0xff;
		break;
	case BWN_TAB_16BIT:
		BWN_PHY_WRITE(mac, BWN_PHY_TABLE_ADDR, offset);
		value = BWN_PHY_READ(mac, BWN_PHY_TABLEDATALO);
		break;
	case BWN_TAB_32BIT:
		BWN_PHY_WRITE(mac, BWN_PHY_TABLE_ADDR, offset);
		value = BWN_PHY_READ(mac, BWN_PHY_TABLEDATAHI);
		value <<= 16;
		value |= BWN_PHY_READ(mac, BWN_PHY_TABLEDATALO);
		break;
	default:
		KASSERT(0 == 1, ("%s:%d: fail", __func__, __LINE__));
		value = 0;
	}

	return (value);
}

static void
bwn_phy_lp_ddfs_turnoff(struct bwn_mac *mac)
{

	BWN_PHY_MASK(mac, BWN_PHY_AFE_DDFS, 0xfffd);
	BWN_PHY_MASK(mac, BWN_PHY_LP_PHY_CTL, 0xffdf);
}

static void
bwn_phy_lp_set_txgain_dac(struct bwn_mac *mac, uint16_t dac)
{
	uint16_t ctl;

	ctl = BWN_PHY_READ(mac, BWN_PHY_AFE_DAC_CTL) & 0xc7f;
	ctl |= dac << 7;
	BWN_PHY_SETMASK(mac, BWN_PHY_AFE_DAC_CTL, 0xf000, ctl);
}

static void
bwn_phy_lp_set_txgain_pa(struct bwn_mac *mac, uint16_t gain)
{

	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0xfb), 0xe03f, gain << 6);
	BWN_PHY_SETMASK(mac, BWN_PHY_OFDM(0xfd), 0x80ff, gain << 8);
}

static void
bwn_phy_lp_set_txgain_override(struct bwn_mac *mac)
{

	if (mac->mac_phy.rev < 2)
		BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2, 0x100);
	else {
		BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2, 0x80);
		BWN_PHY_SET(mac, BWN_PHY_RF_OVERRIDE_2, 0x4000);
	}
	BWN_PHY_SET(mac, BWN_PHY_AFE_CTL_OVR, 0x40);
}

static uint16_t
bwn_phy_lp_get_pa_gain(struct bwn_mac *mac)
{

	return BWN_PHY_READ(mac, BWN_PHY_OFDM(0xfb)) & 0x7f;
}

static uint8_t
bwn_nbits(int32_t val)
{
	uint32_t tmp;
	uint8_t nbits = 0;

	for (tmp = abs(val); tmp != 0; tmp >>= 1)
		nbits++;
	return (nbits);
}

static void
bwn_phy_lp_gaintbl_write_multi(struct bwn_mac *mac, int offset, int count,
    struct bwn_txgain_entry *table)
{
	int i;

	for (i = offset; i < count; i++)
		bwn_phy_lp_gaintbl_write(mac, i, table[i]);
}

static void
bwn_phy_lp_gaintbl_write(struct bwn_mac *mac, int offset,
    struct bwn_txgain_entry data)
{

	if (mac->mac_phy.rev >= 2)
		bwn_phy_lp_gaintbl_write_r2(mac, offset, data);
	else
		bwn_phy_lp_gaintbl_write_r01(mac, offset, data);
}

static void
bwn_phy_lp_gaintbl_write_r2(struct bwn_mac *mac, int offset,
    struct bwn_txgain_entry te)
{
	struct bwn_softc *sc = mac->mac_sc;
	struct ifnet *ifp = sc->sc_ifp;
	struct ieee80211com *ic = ifp->if_l2com;
	uint32_t tmp;

	KASSERT(mac->mac_phy.rev >= 2, ("%s:%d: fail", __func__, __LINE__));

	tmp = (te.te_pad << 16) | (te.te_pga << 8) | te.te_gm;
	if (mac->mac_phy.rev >= 3) {
		tmp |= ((IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) ?
		    (0x10 << 24) : (0x70 << 24));
	} else {
		tmp |= ((IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) ?
		    (0x14 << 24) : (0x7f << 24));
	}
	bwn_tab_write(mac, BWN_TAB_4(7, 0xc0 + offset), tmp);
	bwn_tab_write(mac, BWN_TAB_4(7, 0x140 + offset),
	    te.te_bbmult << 20 | te.te_dac << 28);
}

static void
bwn_phy_lp_gaintbl_write_r01(struct bwn_mac *mac, int offset,
    struct bwn_txgain_entry te)
{

	KASSERT(mac->mac_phy.rev < 2, ("%s:%d: fail", __func__, __LINE__));

	bwn_tab_write(mac, BWN_TAB_4(10, 0xc0 + offset),
	    (te.te_pad << 11) | (te.te_pga << 7) | (te.te_gm  << 4) |
	    te.te_dac);
	bwn_tab_write(mac, BWN_TAB_4(10, 0x140 + offset), te.te_bbmult << 20);
}

static void
bwn_sysctl_node(struct bwn_softc *sc)
{
	device_t dev = sc->sc_dev;
	struct bwn_mac *mac;
	struct bwn_stats *stats;

	/* XXX assume that count of MAC is only 1. */

	if ((mac = sc->sc_curmac) == NULL)
		return;
	stats = &mac->mac_stats;

	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "linknoise", CTLFLAG_RW, &stats->rts, 0, "Noise level");
	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "rts", CTLFLAG_RW, &stats->rts, 0, "RTS");
	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "rtsfail", CTLFLAG_RW, &stats->rtsfail, 0, "RTS failed to send");

#ifdef BWN_DEBUG
	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "debug", CTLFLAG_RW, &sc->sc_debug, 0, "Debug flags");
#endif
}

static device_method_t bwn_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bwn_probe),
	DEVMETHOD(device_attach,	bwn_attach),
	DEVMETHOD(device_detach,	bwn_detach),
	DEVMETHOD(device_suspend,	bwn_suspend),
	DEVMETHOD(device_resume,	bwn_resume),
	KOBJMETHOD_END
};
static driver_t bwn_driver = {
	"bwn",
	bwn_methods,
	sizeof(struct bwn_softc)
};
static devclass_t bwn_devclass;
DRIVER_MODULE(bwn, siba_bwn, bwn_driver, bwn_devclass, 0, 0);
MODULE_DEPEND(bwn, siba_bwn, 1, 1, 1);
MODULE_DEPEND(bwn, wlan, 1, 1, 1);		/* 802.11 media layer */
MODULE_DEPEND(bwn, firmware, 1, 1, 1);		/* firmware support */
MODULE_DEPEND(bwn, wlan_amrr, 1, 1, 1);
