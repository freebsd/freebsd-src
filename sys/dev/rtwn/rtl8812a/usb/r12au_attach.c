/*-
 * Copyright (c) 2016 Andriy Voskoboinyk <avos@FreeBSD.org>
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
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/linker.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#include <dev/rtwn/if_rtwnreg.h>
#include <dev/rtwn/if_rtwnvar.h>

#include <dev/rtwn/if_rtwn_nop.h>

#include <dev/rtwn/usb/rtwn_usb_var.h>

#include <dev/rtwn/rtl8192c/usb/r92cu.h>

#include <dev/rtwn/rtl8188e/r88e.h>

#include <dev/rtwn/rtl8812a/r12a_priv.h>
#include <dev/rtwn/rtl8812a/r12a_reg.h>
#include <dev/rtwn/rtl8812a/r12a_var.h>

#include <dev/rtwn/rtl8812a/usb/r12au.h>
#include <dev/rtwn/rtl8812a/usb/r12au_tx_desc.h>

void	r12au_attach(struct rtwn_usb_softc *);

static void
r12au_postattach(struct rtwn_softc *sc)
{
	struct rtwn_usb_softc *uc = RTWN_USB_SOFTC(sc);
	struct r12a_softc *rs = sc->sc_priv;

	if (usbd_get_speed(uc->uc_udev) == USB_SPEED_SUPER) {
		rs->ac_usb_dma_size = 0x07;
		rs->ac_usb_dma_time = 0x1a;
	} else {
		rs->ac_usb_dma_size = 0x01;
		rs->ac_usb_dma_time = 0x10;
	}

	if (rs->chip & R12A_CHIP_C_CUT)
		sc->sc_rf_read = r12a_c_cut_rf_read;
	else
		sc->sc_rf_read = r12a_rf_read;

	if (rs->board_type == R92C_BOARD_TYPE_MINICARD ||
	    rs->board_type == R92C_BOARD_TYPE_SOLO ||
	    rs->board_type == R92C_BOARD_TYPE_COMBO)
		sc->sc_set_led = r88e_set_led;
	else
		sc->sc_set_led = r12a_set_led;

	if (!(rs->ext_pa_2g || rs->ext_lna_2g ||
	    rs->ext_pa_5g || rs->ext_lna_5g))
		sc->mac_prog = &rtl8812au_mac_no_ext_pa_lna[0];

	sc->sc_ic.ic_ioctl = r12a_ioctl_net;
}

void
r12a_vap_preattach(struct rtwn_softc *sc, struct ieee80211vap *vap)
{
	struct r12a_softc *rs = sc->sc_priv;
	if_t ifp = vap->iv_ifp;

	if_setcapabilities(ifp, IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6);
	RTWN_LOCK(sc);
	if (rs->rs_flags & R12A_RXCKSUM_EN)
		if_setcapenablebit(ifp, IFCAP_RXCSUM, 0);
	if (rs->rs_flags & R12A_RXCKSUM6_EN)
		if_setcapenablebit(ifp, IFCAP_RXCSUM_IPV6, 0);
	RTWN_UNLOCK(sc);
}

static void
r12a_attach_private(struct rtwn_softc *sc)
{
	struct r12a_softc *rs;

	rs = malloc(sizeof(struct r12a_softc), M_RTWN_PRIV, M_WAITOK | M_ZERO);

	rs->rs_flags			= R12A_RXCKSUM_EN | R12A_RXCKSUM6_EN;

	rs->rs_fix_spur			= r12a_fix_spur;
	rs->rs_set_band_2ghz		= r12a_set_band_2ghz;
	rs->rs_set_band_5ghz		= r12a_set_band_5ghz;
	rs->rs_init_burstlen		= r12au_init_burstlen;
	rs->rs_init_ampdu_fwhw		= r12au_init_ampdu_fwhw;
	rs->rs_crystalcap_write		= r12a_crystalcap_write;
#ifndef RTWN_WITHOUT_UCODE
	rs->rs_iq_calib_fw_supported	= r12a_iq_calib_fw_supported;
#endif
	rs->rs_iq_calib_sw		= r12a_iq_calib_sw;

	rs->ampdu_max_time		= 0x70;
	rs->ampdu_max_size		= 0x1ffff; /* 128k */

	sc->sc_priv = rs;
}

void
r12a_detach_private(struct rtwn_softc *sc)
{
	struct r12a_softc *rs = sc->sc_priv;

	free(rs, M_RTWN_PRIV);
}

static void
r12a_read_chipid_vendor(struct rtwn_softc *sc, uint32_t reg_sys_cfg)
{
	struct r12a_softc *rs = sc->sc_priv;

	if (MS(reg_sys_cfg, R92C_SYS_CFG_CHIP_VER_RTL) == 1)
		rs->chip |= R12A_CHIP_C_CUT;
}

static void
r12au_adj_devcaps(struct rtwn_softc *sc)
{
	struct r12a_softc *rs = sc->sc_priv;
	struct ieee80211com *ic = &sc->sc_ic;

	if (rs->chip & R12A_CHIP_C_CUT) {
		ic->ic_htcaps |= IEEE80211_HTCAP_LDPC |
				 IEEE80211_HTC_TXLDPC;
	}

	ic->ic_htcaps |=
	    IEEE80211_HTCAP_CHWIDTH40 | /* 40 MHz channel width */
	    IEEE80211_HTCAP_SHORTGI40 /* short GI in 40MHz */
	;

	/* TODO: STBC */

	/* VHT config */
	ic->ic_flags_ext |= IEEE80211_FEXT_VHT;
	ic->ic_vht_cap.vht_cap_info =
	    IEEE80211_VHTCAP_MAX_MPDU_LENGTH_11454 |
	    IEEE80211_VHTCAP_SHORT_GI_80 |
	    IEEE80211_VHTCAP_TXSTBC |
	    IEEE80211_VHTCAP_RXSTBC_1 |
	    IEEE80211_VHTCAP_HTC_VHT |
	    _IEEE80211_SHIFTMASK(7,
	      IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK);

	rtwn_attach_vht_cap_info_mcs(sc);
}

void
r12au_attach(struct rtwn_usb_softc *uc)
{
	struct rtwn_softc *sc		= &uc->uc_sc;

	/* USB part. */
	uc->uc_align_rx			= r12au_align_rx;
	uc->tx_agg_desc_num		= 1;

	/* Common part. */
	sc->sc_flags			= RTWN_FLAG_EXT_HDR;

	sc->sc_set_chan			= r12a_set_chan;
	sc->sc_fill_tx_desc		= r12a_fill_tx_desc;
	sc->sc_fill_tx_desc_raw 	= r12a_fill_tx_desc_raw;
	sc->sc_fill_tx_desc_null	= r12a_fill_tx_desc_null;
	sc->sc_dump_tx_desc		= r12au_dump_tx_desc;
	sc->sc_tx_radiotap_flags	= r12a_tx_radiotap_flags;
	sc->sc_rx_radiotap_flags	= r12a_rx_radiotap_flags;
	sc->sc_get_rx_stats		= r12a_get_rx_stats;
	sc->sc_get_rssi_cck		= r88e_get_rssi_cck;
	sc->sc_get_rssi_ofdm		= r88e_get_rssi_ofdm;
	sc->sc_classify_intr		= r12au_classify_intr;
	sc->sc_handle_tx_report		= r12a_ratectl_tx_complete;
	sc->sc_handle_tx_report2	= rtwn_nop_softc_uint8_int;
	sc->sc_handle_c2h_report	= r12a_handle_c2h_report;
	sc->sc_check_frame		= r12a_check_frame_checksum;
	sc->sc_rf_write			= r12a_rf_write;
	sc->sc_check_condition		= r12a_check_condition;
	sc->sc_efuse_postread		= rtwn_nop_softc;
	sc->sc_parse_rom		= r12a_parse_rom;
	sc->sc_power_on			= r12a_power_on;
	sc->sc_power_off		= r12a_power_off;
#ifndef RTWN_WITHOUT_UCODE
	sc->sc_fw_reset			= r12a_fw_reset;
	sc->sc_fw_download_enable	= r12a_fw_download_enable;
#endif
	sc->sc_llt_init			= r92c_llt_init;
	sc->sc_set_page_size		= r12a_set_page_size;
	sc->sc_lc_calib			= r12a_lc_calib;
	sc->sc_iq_calib			= r12a_iq_calib;
	sc->sc_read_chipid_vendor	= r12a_read_chipid_vendor;
	sc->sc_adj_devcaps		= r12au_adj_devcaps;
	sc->sc_vap_preattach		= r12a_vap_preattach;
	sc->sc_postattach		= r12au_postattach;
	sc->sc_detach_private		= r12a_detach_private;
#ifndef RTWN_WITHOUT_UCODE
	sc->sc_set_media_status		= r12a_set_media_status;
	sc->sc_set_rsvd_page		= r88e_set_rsvd_page;
	sc->sc_set_pwrmode		= r12a_set_pwrmode;
	sc->sc_set_rssi			= rtwn_nop_softc;	/* XXX TODO */
#else
	sc->sc_set_media_status		= rtwn_nop_softc_int;
#endif
	sc->sc_beacon_init		= r12a_beacon_init;
	sc->sc_beacon_enable		= r92c_beacon_enable;
	sc->sc_sta_beacon_enable	= r12a_sta_beacon_enable;
	sc->sc_beacon_set_rate		= r12a_beacon_set_rate;
	sc->sc_beacon_select		= rtwn_nop_softc_int;
	sc->sc_temp_measure		= r88e_temp_measure;
	sc->sc_temp_read		= r88e_temp_read;
	sc->sc_init_tx_agg		= r92cu_init_tx_agg;
	sc->sc_init_rx_agg		= r12au_init_rx_agg;
	sc->sc_init_ampdu		= r12au_init_ampdu;
	sc->sc_init_intr		= r12a_init_intr;
	sc->sc_init_edca		= r12a_init_edca;
	sc->sc_init_bb			= r12a_init_bb;
	sc->sc_init_rf			= r12a_init_rf;
	sc->sc_init_antsel		= r12a_init_antsel;
	sc->sc_post_init		= r12au_post_init;
	sc->sc_init_bcnq1_boundary	= rtwn_nop_int_softc;
	sc->sc_set_tx_power		= rtwn_nop_int_softc_vap;

	sc->chan_list_5ghz[0]		= r12a_chan_5ghz_0;
	sc->chan_list_5ghz[1]		= r12a_chan_5ghz_1;
	sc->chan_list_5ghz[2]		= r12a_chan_5ghz_2;
	sc->chan_num_5ghz[0]		= nitems(r12a_chan_5ghz_0);
	sc->chan_num_5ghz[1]		= nitems(r12a_chan_5ghz_1);
	sc->chan_num_5ghz[2]		= nitems(r12a_chan_5ghz_2);

	sc->mac_prog			= &rtl8812au_mac[0];
	sc->mac_size			= nitems(rtl8812au_mac);
	sc->bb_prog			= &rtl8812au_bb[0];
	sc->bb_size			= nitems(rtl8812au_bb);
	sc->agc_prog			= &rtl8812au_agc[0];
	sc->agc_size			= nitems(rtl8812au_agc);
	sc->rf_prog			= &rtl8812au_rf[0];

	sc->name			= "RTL8812AU";
	sc->fwname			= "rtwn-rtl8812aufw";
	sc->fwsig			= 0x950;

	sc->page_count			= R12A_TX_PAGE_COUNT;
	sc->pktbuf_count		= R12A_TXPKTBUF_COUNT;

	sc->ackto			= 0x80;
	sc->npubqpages			= R12A_PUBQ_NPAGES;
	sc->page_size			= R12A_TX_PAGE_SIZE;

	sc->txdesc_len			= sizeof(struct r12au_tx_desc);
	sc->efuse_maxlen		= R12A_EFUSE_MAX_LEN;
	sc->efuse_maplen		= R12A_EFUSE_MAP_LEN;
	sc->rx_dma_size			= R12A_RX_DMA_BUFFER_SIZE;

	sc->macid_limit			= R12A_MACID_MAX + 1;
	sc->cam_entry_limit		= R12A_CAM_ENTRY_COUNT;
	sc->fwsize_limit		= R12A_MAX_FW_SIZE;
	sc->temp_delta			= R88E_CALIB_THRESHOLD;

	sc->bcn_status_reg[0]		= R92C_TDECTRL;
	sc->bcn_status_reg[1]		= R92C_TDECTRL;
	sc->rcr				= R12A_RCR_DIS_CHK_14 |
					  R12A_RCR_VHT_ACK |
					  R12A_RCR_TCP_OFFLD_EN;

	sc->ntxchains			= 2;
	sc->nrxchains			= 2;

	sc->sc_ht40			= 1;

	r12a_attach_private(sc);
}
