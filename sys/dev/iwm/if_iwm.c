/*	$OpenBSD: if_iwm.c,v 1.39 2015/03/23 00:35:19 jsg Exp $	*/

/*
 * Copyright (c) 2014 genua mbh <info@genua.de>
 * Copyright (c) 2014 Fixup Software Ltd.
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

/*-
 * Based on BSD-licensed source modules in the Linux iwlwifi driver,
 * which were used as the reference documentation for this implementation.
 *
 * Driver version we are currently based off of is
 * Linux 3.14.3 (tag id a2df521e42b1d9a23f620ac79dbfe8655a8391dd)
 *
 ***********************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2013 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called COPYING.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2013 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2007-2010 Damien Bergamini <damien.bergamini@free.fr>
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/firmware.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/linker.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/resource.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <net/bpf.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/iwm/if_iwmreg.h>
#include <dev/iwm/if_iwmvar.h>
#include <dev/iwm/if_iwm_debug.h>
#include <dev/iwm/if_iwm_util.h>
#include <dev/iwm/if_iwm_binding.h>
#include <dev/iwm/if_iwm_phy_db.h>
#include <dev/iwm/if_iwm_mac_ctxt.h>
#include <dev/iwm/if_iwm_phy_ctxt.h>
#include <dev/iwm/if_iwm_time_event.h>
#include <dev/iwm/if_iwm_power.h>
#include <dev/iwm/if_iwm_scan.h>

#include <dev/iwm/if_iwm_pcie_trans.h>
#include <dev/iwm/if_iwm_led.h>

const uint8_t iwm_nvm_channels[] = {
	/* 2.4 GHz */
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	/* 5 GHz */
	36, 40, 44, 48, 52, 56, 60, 64,
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144,
	149, 153, 157, 161, 165
};
#define IWM_NUM_2GHZ_CHANNELS	14

_Static_assert(nitems(iwm_nvm_channels) <= IWM_NUM_CHANNELS,
    "IWM_NUM_CHANNELS is too small");

/*
 * XXX For now, there's simply a fixed set of rate table entries
 * that are populated.
 */
const struct iwm_rate {
	uint8_t rate;
	uint8_t plcp;
} iwm_rates[] = {
	{   2,	IWM_RATE_1M_PLCP  },
	{   4,	IWM_RATE_2M_PLCP  },
	{  11,	IWM_RATE_5M_PLCP  },
	{  22,	IWM_RATE_11M_PLCP },
	{  12,	IWM_RATE_6M_PLCP  },
	{  18,	IWM_RATE_9M_PLCP  },
	{  24,	IWM_RATE_12M_PLCP },
	{  36,	IWM_RATE_18M_PLCP },
	{  48,	IWM_RATE_24M_PLCP },
	{  72,	IWM_RATE_36M_PLCP },
	{  96,	IWM_RATE_48M_PLCP },
	{ 108,	IWM_RATE_54M_PLCP },
};
#define IWM_RIDX_CCK	0
#define IWM_RIDX_OFDM	4
#define IWM_RIDX_MAX	(nitems(iwm_rates)-1)
#define IWM_RIDX_IS_CCK(_i_) ((_i_) < IWM_RIDX_OFDM)
#define IWM_RIDX_IS_OFDM(_i_) ((_i_) >= IWM_RIDX_OFDM)

static int	iwm_store_cscheme(struct iwm_softc *, const uint8_t *, size_t);
static int	iwm_firmware_store_section(struct iwm_softc *,
                                           enum iwm_ucode_type,
                                           const uint8_t *, size_t);
static int	iwm_set_default_calib(struct iwm_softc *, const void *);
static void	iwm_fw_info_free(struct iwm_fw_info *);
static int	iwm_read_firmware(struct iwm_softc *, enum iwm_ucode_type);
static void	iwm_dma_map_addr(void *, bus_dma_segment_t *, int, int);
static int	iwm_dma_contig_alloc(bus_dma_tag_t, struct iwm_dma_info *,
                                     bus_size_t, bus_size_t);
static void	iwm_dma_contig_free(struct iwm_dma_info *);
static int	iwm_alloc_fwmem(struct iwm_softc *);
static void	iwm_free_fwmem(struct iwm_softc *);
static int	iwm_alloc_sched(struct iwm_softc *);
static void	iwm_free_sched(struct iwm_softc *);
static int	iwm_alloc_kw(struct iwm_softc *);
static void	iwm_free_kw(struct iwm_softc *);
static int	iwm_alloc_ict(struct iwm_softc *);
static void	iwm_free_ict(struct iwm_softc *);
static int	iwm_alloc_rx_ring(struct iwm_softc *, struct iwm_rx_ring *);
static void	iwm_disable_rx_dma(struct iwm_softc *);
static void	iwm_reset_rx_ring(struct iwm_softc *, struct iwm_rx_ring *);
static void	iwm_free_rx_ring(struct iwm_softc *, struct iwm_rx_ring *);
static int	iwm_alloc_tx_ring(struct iwm_softc *, struct iwm_tx_ring *,
                                  int);
static void	iwm_reset_tx_ring(struct iwm_softc *, struct iwm_tx_ring *);
static void	iwm_free_tx_ring(struct iwm_softc *, struct iwm_tx_ring *);
static void	iwm_enable_interrupts(struct iwm_softc *);
static void	iwm_restore_interrupts(struct iwm_softc *);
static void	iwm_disable_interrupts(struct iwm_softc *);
static void	iwm_ict_reset(struct iwm_softc *);
static int	iwm_allow_mcast(struct ieee80211vap *, struct iwm_softc *);
static void	iwm_stop_device(struct iwm_softc *);
static void	iwm_mvm_nic_config(struct iwm_softc *);
static int	iwm_nic_rx_init(struct iwm_softc *);
static int	iwm_nic_tx_init(struct iwm_softc *);
static int	iwm_nic_init(struct iwm_softc *);
static void	iwm_enable_txq(struct iwm_softc *, int, int);
static int	iwm_post_alive(struct iwm_softc *);
static int	iwm_nvm_read_chunk(struct iwm_softc *, uint16_t, uint16_t,
                                   uint16_t, uint8_t *, uint16_t *);
static int	iwm_nvm_read_section(struct iwm_softc *, uint16_t, uint8_t *,
				     uint16_t *);
static uint32_t	iwm_eeprom_channel_flags(uint16_t);
static void	iwm_add_channel_band(struct iwm_softc *,
		    struct ieee80211_channel[], int, int *, int, int,
		    const uint8_t[]);
static void	iwm_init_channel_map(struct ieee80211com *, int, int *,
		    struct ieee80211_channel[]);
static int	iwm_parse_nvm_data(struct iwm_softc *, const uint16_t *,
			           const uint16_t *, const uint16_t *, uint8_t,
				   uint8_t);
struct iwm_nvm_section;
static int	iwm_parse_nvm_sections(struct iwm_softc *,
                                       struct iwm_nvm_section *);
static int	iwm_nvm_init(struct iwm_softc *);
static int	iwm_firmware_load_chunk(struct iwm_softc *, uint32_t,
                                        const uint8_t *, uint32_t);
static int	iwm_load_firmware(struct iwm_softc *, enum iwm_ucode_type);
static int	iwm_start_fw(struct iwm_softc *, enum iwm_ucode_type);
static int	iwm_fw_alive(struct iwm_softc *, uint32_t);
static int	iwm_send_tx_ant_cfg(struct iwm_softc *, uint8_t);
static int	iwm_send_phy_cfg_cmd(struct iwm_softc *);
static int	iwm_mvm_load_ucode_wait_alive(struct iwm_softc *,
                                              enum iwm_ucode_type);
static int	iwm_run_init_mvm_ucode(struct iwm_softc *, int);
static int	iwm_rx_addbuf(struct iwm_softc *, int, int);
static int	iwm_mvm_calc_rssi(struct iwm_softc *, struct iwm_rx_phy_info *);
static int	iwm_mvm_get_signal_strength(struct iwm_softc *,
					    struct iwm_rx_phy_info *);
static void	iwm_mvm_rx_rx_phy_cmd(struct iwm_softc *,
                                      struct iwm_rx_packet *,
                                      struct iwm_rx_data *);
static int	iwm_get_noise(const struct iwm_mvm_statistics_rx_non_phy *);
static void	iwm_mvm_rx_rx_mpdu(struct iwm_softc *, struct iwm_rx_packet *,
                                   struct iwm_rx_data *);
static int	iwm_mvm_rx_tx_cmd_single(struct iwm_softc *,
                                         struct iwm_rx_packet *,
				         struct iwm_node *);
static void	iwm_mvm_rx_tx_cmd(struct iwm_softc *, struct iwm_rx_packet *,
                                  struct iwm_rx_data *);
static void	iwm_cmd_done(struct iwm_softc *, struct iwm_rx_packet *);
#if 0
static void	iwm_update_sched(struct iwm_softc *, int, int, uint8_t,
                                 uint16_t);
#endif
static const struct iwm_rate *
	iwm_tx_fill_cmd(struct iwm_softc *, struct iwm_node *,
			struct ieee80211_frame *, struct iwm_tx_cmd *);
static int	iwm_tx(struct iwm_softc *, struct mbuf *,
                       struct ieee80211_node *, int);
static int	iwm_raw_xmit(struct ieee80211_node *, struct mbuf *,
			     const struct ieee80211_bpf_params *);
static void	iwm_mvm_add_sta_cmd_v6_to_v5(struct iwm_mvm_add_sta_cmd_v6 *,
					     struct iwm_mvm_add_sta_cmd_v5 *);
static int	iwm_mvm_send_add_sta_cmd_status(struct iwm_softc *,
					        struct iwm_mvm_add_sta_cmd_v6 *,
                                                int *);
static int	iwm_mvm_sta_send_to_fw(struct iwm_softc *, struct iwm_node *,
                                       int);
static int	iwm_mvm_add_sta(struct iwm_softc *, struct iwm_node *);
static int	iwm_mvm_update_sta(struct iwm_softc *, struct iwm_node *);
static int	iwm_mvm_add_int_sta_common(struct iwm_softc *,
                                           struct iwm_int_sta *,
				           const uint8_t *, uint16_t, uint16_t);
static int	iwm_mvm_add_aux_sta(struct iwm_softc *);
static int	iwm_mvm_update_quotas(struct iwm_softc *, struct iwm_node *);
static int	iwm_auth(struct ieee80211vap *, struct iwm_softc *);
static int	iwm_assoc(struct ieee80211vap *, struct iwm_softc *);
static int	iwm_release(struct iwm_softc *, struct iwm_node *);
static struct ieee80211_node *
		iwm_node_alloc(struct ieee80211vap *,
		               const uint8_t[IEEE80211_ADDR_LEN]);
static void	iwm_setrates(struct iwm_softc *, struct iwm_node *);
static int	iwm_media_change(struct ifnet *);
static int	iwm_newstate(struct ieee80211vap *, enum ieee80211_state, int);
static void	iwm_endscan_cb(void *, int);
static int	iwm_init_hw(struct iwm_softc *);
static void	iwm_init(struct iwm_softc *);
static void	iwm_start(struct iwm_softc *);
static void	iwm_stop(struct iwm_softc *);
static void	iwm_watchdog(void *);
static void	iwm_parent(struct ieee80211com *);
#ifdef IWM_DEBUG
static const char *
		iwm_desc_lookup(uint32_t);
static void	iwm_nic_error(struct iwm_softc *);
#endif
static void	iwm_notif_intr(struct iwm_softc *);
static void	iwm_intr(void *);
static int	iwm_attach(device_t);
static void	iwm_preinit(void *);
static int	iwm_detach_local(struct iwm_softc *sc, int);
static void	iwm_init_task(void *);
static void	iwm_radiotap_attach(struct iwm_softc *);
static struct ieee80211vap *
		iwm_vap_create(struct ieee80211com *,
		               const char [IFNAMSIZ], int,
		               enum ieee80211_opmode, int,
		               const uint8_t [IEEE80211_ADDR_LEN],
		               const uint8_t [IEEE80211_ADDR_LEN]);
static void	iwm_vap_delete(struct ieee80211vap *);
static void	iwm_scan_start(struct ieee80211com *);
static void	iwm_scan_end(struct ieee80211com *);
static void	iwm_update_mcast(struct ieee80211com *);
static void	iwm_set_channel(struct ieee80211com *);
static void	iwm_scan_curchan(struct ieee80211_scan_state *, unsigned long);
static void	iwm_scan_mindwell(struct ieee80211_scan_state *);
static int	iwm_detach(device_t);

/*
 * Firmware parser.
 */

static int
iwm_store_cscheme(struct iwm_softc *sc, const uint8_t *data, size_t dlen)
{
	const struct iwm_fw_cscheme_list *l = (const void *)data;

	if (dlen < sizeof(*l) ||
	    dlen < sizeof(l->size) + l->size * sizeof(*l->cs))
		return EINVAL;

	/* we don't actually store anything for now, always use s/w crypto */

	return 0;
}

static int
iwm_firmware_store_section(struct iwm_softc *sc,
    enum iwm_ucode_type type, const uint8_t *data, size_t dlen)
{
	struct iwm_fw_sects *fws;
	struct iwm_fw_onesect *fwone;

	if (type >= IWM_UCODE_TYPE_MAX)
		return EINVAL;
	if (dlen < sizeof(uint32_t))
		return EINVAL;

	fws = &sc->sc_fw.fw_sects[type];
	if (fws->fw_count >= IWM_UCODE_SECT_MAX)
		return EINVAL;

	fwone = &fws->fw_sect[fws->fw_count];

	/* first 32bit are device load offset */
	memcpy(&fwone->fws_devoff, data, sizeof(uint32_t));

	/* rest is data */
	fwone->fws_data = data + sizeof(uint32_t);
	fwone->fws_len = dlen - sizeof(uint32_t);

	fws->fw_count++;
	fws->fw_totlen += fwone->fws_len;

	return 0;
}

/* iwlwifi: iwl-drv.c */
struct iwm_tlv_calib_data {
	uint32_t ucode_type;
	struct iwm_tlv_calib_ctrl calib;
} __packed;

static int
iwm_set_default_calib(struct iwm_softc *sc, const void *data)
{
	const struct iwm_tlv_calib_data *def_calib = data;
	uint32_t ucode_type = le32toh(def_calib->ucode_type);

	if (ucode_type >= IWM_UCODE_TYPE_MAX) {
		device_printf(sc->sc_dev,
		    "Wrong ucode_type %u for default "
		    "calibration.\n", ucode_type);
		return EINVAL;
	}

	sc->sc_default_calib[ucode_type].flow_trigger =
	    def_calib->calib.flow_trigger;
	sc->sc_default_calib[ucode_type].event_trigger =
	    def_calib->calib.event_trigger;

	return 0;
}

static void
iwm_fw_info_free(struct iwm_fw_info *fw)
{
	firmware_put(fw->fw_fp, FIRMWARE_UNLOAD);
	fw->fw_fp = NULL;
	/* don't touch fw->fw_status */
	memset(fw->fw_sects, 0, sizeof(fw->fw_sects));
}

static int
iwm_read_firmware(struct iwm_softc *sc, enum iwm_ucode_type ucode_type)
{
	struct iwm_fw_info *fw = &sc->sc_fw;
	const struct iwm_tlv_ucode_header *uhdr;
	struct iwm_ucode_tlv tlv;
	enum iwm_ucode_tlv_type tlv_type;
	const struct firmware *fwp;
	const uint8_t *data;
	int error = 0;
	size_t len;

	if (fw->fw_status == IWM_FW_STATUS_DONE &&
	    ucode_type != IWM_UCODE_TYPE_INIT)
		return 0;

	while (fw->fw_status == IWM_FW_STATUS_INPROGRESS)
		msleep(&sc->sc_fw, &sc->sc_mtx, 0, "iwmfwp", 0);
	fw->fw_status = IWM_FW_STATUS_INPROGRESS;

	if (fw->fw_fp != NULL)
		iwm_fw_info_free(fw);

	/*
	 * Load firmware into driver memory.
	 * fw_fp will be set.
	 */
	IWM_UNLOCK(sc);
	fwp = firmware_get(sc->sc_fwname);
	IWM_LOCK(sc);
	if (fwp == NULL) {
		device_printf(sc->sc_dev,
		    "could not read firmware %s (error %d)\n",
		    sc->sc_fwname, error);
		goto out;
	}
	fw->fw_fp = fwp;

	/*
	 * Parse firmware contents
	 */

	uhdr = (const void *)fw->fw_fp->data;
	if (*(const uint32_t *)fw->fw_fp->data != 0
	    || le32toh(uhdr->magic) != IWM_TLV_UCODE_MAGIC) {
		device_printf(sc->sc_dev, "invalid firmware %s\n",
		    sc->sc_fwname);
		error = EINVAL;
		goto out;
	}

	sc->sc_fwver = le32toh(uhdr->ver);
	data = uhdr->data;
	len = fw->fw_fp->datasize - sizeof(*uhdr);

	while (len >= sizeof(tlv)) {
		size_t tlv_len;
		const void *tlv_data;

		memcpy(&tlv, data, sizeof(tlv));
		tlv_len = le32toh(tlv.length);
		tlv_type = le32toh(tlv.type);

		len -= sizeof(tlv);
		data += sizeof(tlv);
		tlv_data = data;

		if (len < tlv_len) {
			device_printf(sc->sc_dev,
			    "firmware too short: %zu bytes\n",
			    len);
			error = EINVAL;
			goto parse_out;
		}

		switch ((int)tlv_type) {
		case IWM_UCODE_TLV_PROBE_MAX_LEN:
			if (tlv_len < sizeof(uint32_t)) {
				device_printf(sc->sc_dev,
				    "%s: PROBE_MAX_LEN (%d) < sizeof(uint32_t)\n",
				    __func__,
				    (int) tlv_len);
				error = EINVAL;
				goto parse_out;
			}
			sc->sc_capa_max_probe_len
			    = le32toh(*(const uint32_t *)tlv_data);
			/* limit it to something sensible */
			if (sc->sc_capa_max_probe_len > (1<<16)) {
				IWM_DPRINTF(sc, IWM_DEBUG_FIRMWARE_TLV,
				    "%s: IWM_UCODE_TLV_PROBE_MAX_LEN "
				    "ridiculous\n", __func__);
				error = EINVAL;
				goto parse_out;
			}
			break;
		case IWM_UCODE_TLV_PAN:
			if (tlv_len) {
				device_printf(sc->sc_dev,
				    "%s: IWM_UCODE_TLV_PAN: tlv_len (%d) > 0\n",
				    __func__,
				    (int) tlv_len);
				error = EINVAL;
				goto parse_out;
			}
			sc->sc_capaflags |= IWM_UCODE_TLV_FLAGS_PAN;
			break;
		case IWM_UCODE_TLV_FLAGS:
			if (tlv_len < sizeof(uint32_t)) {
				device_printf(sc->sc_dev,
				    "%s: IWM_UCODE_TLV_FLAGS: tlv_len (%d) < sizeof(uint32_t)\n",
				    __func__,
				    (int) tlv_len);
				error = EINVAL;
				goto parse_out;
			}
			/*
			 * Apparently there can be many flags, but Linux driver
			 * parses only the first one, and so do we.
			 *
			 * XXX: why does this override IWM_UCODE_TLV_PAN?
			 * Intentional or a bug?  Observations from
			 * current firmware file:
			 *  1) TLV_PAN is parsed first
			 *  2) TLV_FLAGS contains TLV_FLAGS_PAN
			 * ==> this resets TLV_PAN to itself... hnnnk
			 */
			sc->sc_capaflags = le32toh(*(const uint32_t *)tlv_data);
			break;
		case IWM_UCODE_TLV_CSCHEME:
			if ((error = iwm_store_cscheme(sc,
			    tlv_data, tlv_len)) != 0) {
				device_printf(sc->sc_dev,
				    "%s: iwm_store_cscheme(): returned %d\n",
				    __func__,
				    error);
				goto parse_out;
			}
			break;
		case IWM_UCODE_TLV_NUM_OF_CPU:
			if (tlv_len != sizeof(uint32_t)) {
				device_printf(sc->sc_dev,
				    "%s: IWM_UCODE_TLV_NUM_OF_CPU: tlv_len (%d) < sizeof(uint32_t)\n",
				    __func__,
				    (int) tlv_len);
				error = EINVAL;
				goto parse_out;
			}
			if (le32toh(*(const uint32_t*)tlv_data) != 1) {
				device_printf(sc->sc_dev,
				    "%s: driver supports "
				    "only TLV_NUM_OF_CPU == 1",
				    __func__);
				error = EINVAL;
				goto parse_out;
			}
			break;
		case IWM_UCODE_TLV_SEC_RT:
			if ((error = iwm_firmware_store_section(sc,
			    IWM_UCODE_TYPE_REGULAR, tlv_data, tlv_len)) != 0) {
				device_printf(sc->sc_dev,
				    "%s: IWM_UCODE_TYPE_REGULAR: iwm_firmware_store_section() failed; %d\n",
				    __func__,
				    error);
				goto parse_out;
			}
			break;
		case IWM_UCODE_TLV_SEC_INIT:
			if ((error = iwm_firmware_store_section(sc,
			    IWM_UCODE_TYPE_INIT, tlv_data, tlv_len)) != 0) {
				device_printf(sc->sc_dev,
				    "%s: IWM_UCODE_TYPE_INIT: iwm_firmware_store_section() failed; %d\n",
				    __func__,
				    error);
				goto parse_out;
			}
			break;
		case IWM_UCODE_TLV_SEC_WOWLAN:
			if ((error = iwm_firmware_store_section(sc,
			    IWM_UCODE_TYPE_WOW, tlv_data, tlv_len)) != 0) {
				device_printf(sc->sc_dev,
				    "%s: IWM_UCODE_TYPE_WOW: iwm_firmware_store_section() failed; %d\n",
				    __func__,
				    error);
				goto parse_out;
			}
			break;
		case IWM_UCODE_TLV_DEF_CALIB:
			if (tlv_len != sizeof(struct iwm_tlv_calib_data)) {
				device_printf(sc->sc_dev,
				    "%s: IWM_UCODE_TLV_DEV_CALIB: tlv_len (%d) < sizeof(iwm_tlv_calib_data) (%d)\n",
				    __func__,
				    (int) tlv_len,
				    (int) sizeof(struct iwm_tlv_calib_data));
				error = EINVAL;
				goto parse_out;
			}
			if ((error = iwm_set_default_calib(sc, tlv_data)) != 0) {
				device_printf(sc->sc_dev,
				    "%s: iwm_set_default_calib() failed: %d\n",
				    __func__,
				    error);
				goto parse_out;
			}
			break;
		case IWM_UCODE_TLV_PHY_SKU:
			if (tlv_len != sizeof(uint32_t)) {
				error = EINVAL;
				device_printf(sc->sc_dev,
				    "%s: IWM_UCODE_TLV_PHY_SKU: tlv_len (%d) < sizeof(uint32_t)\n",
				    __func__,
				    (int) tlv_len);
				goto parse_out;
			}
			sc->sc_fw_phy_config =
			    le32toh(*(const uint32_t *)tlv_data);
			break;

		case IWM_UCODE_TLV_API_CHANGES_SET:
		case IWM_UCODE_TLV_ENABLED_CAPABILITIES:
			/* ignore, not used by current driver */
			break;

		default:
			device_printf(sc->sc_dev,
			    "%s: unknown firmware section %d, abort\n",
			    __func__, tlv_type);
			error = EINVAL;
			goto parse_out;
		}

		len -= roundup(tlv_len, 4);
		data += roundup(tlv_len, 4);
	}

	KASSERT(error == 0, ("unhandled error"));

 parse_out:
	if (error) {
		device_printf(sc->sc_dev, "firmware parse error %d, "
		    "section type %d\n", error, tlv_type);
	}

	if (!(sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_PM_CMD_SUPPORT)) {
		device_printf(sc->sc_dev,
		    "device uses unsupported power ops\n");
		error = ENOTSUP;
	}

 out:
	if (error) {
		fw->fw_status = IWM_FW_STATUS_NONE;
		if (fw->fw_fp != NULL)
			iwm_fw_info_free(fw);
	} else
		fw->fw_status = IWM_FW_STATUS_DONE;
	wakeup(&sc->sc_fw);

	return error;
}

/*
 * DMA resource routines
 */

static void
iwm_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
        if (error != 0)
                return;
	KASSERT(nsegs == 1, ("too many DMA segments, %d should be 1", nsegs));
        *(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
iwm_dma_contig_alloc(bus_dma_tag_t tag, struct iwm_dma_info *dma,
    bus_size_t size, bus_size_t alignment)
{
	int error;

	dma->tag = NULL;
	dma->size = size;

	error = bus_dma_tag_create(tag, alignment,
            0, BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, size,
            1, size, 0, NULL, NULL, &dma->tag);
        if (error != 0)
                goto fail;

        error = bus_dmamem_alloc(dma->tag, (void **)&dma->vaddr,
            BUS_DMA_NOWAIT | BUS_DMA_ZERO | BUS_DMA_COHERENT, &dma->map);
        if (error != 0)
                goto fail;

        error = bus_dmamap_load(dma->tag, dma->map, dma->vaddr, size,
            iwm_dma_map_addr, &dma->paddr, BUS_DMA_NOWAIT);
        if (error != 0)
                goto fail;

	bus_dmamap_sync(dma->tag, dma->map, BUS_DMASYNC_PREWRITE);

	return 0;

fail:	iwm_dma_contig_free(dma);
	return error;
}

static void
iwm_dma_contig_free(struct iwm_dma_info *dma)
{
	if (dma->map != NULL) {
		if (dma->vaddr != NULL) {
			bus_dmamap_sync(dma->tag, dma->map,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(dma->tag, dma->map);
			bus_dmamem_free(dma->tag, dma->vaddr, dma->map);
			dma->vaddr = NULL;
		}
		bus_dmamap_destroy(dma->tag, dma->map);
		dma->map = NULL;
	}
	if (dma->tag != NULL) {
		bus_dma_tag_destroy(dma->tag);
		dma->tag = NULL;
	}

}

/* fwmem is used to load firmware onto the card */
static int
iwm_alloc_fwmem(struct iwm_softc *sc)
{
	/* Must be aligned on a 16-byte boundary. */
	return iwm_dma_contig_alloc(sc->sc_dmat, &sc->fw_dma,
	    sc->sc_fwdmasegsz, 16);
}

static void
iwm_free_fwmem(struct iwm_softc *sc)
{
	iwm_dma_contig_free(&sc->fw_dma);
}

/* tx scheduler rings.  not used? */
static int
iwm_alloc_sched(struct iwm_softc *sc)
{
	int rv;

	/* TX scheduler rings must be aligned on a 1KB boundary. */
	rv = iwm_dma_contig_alloc(sc->sc_dmat, &sc->sched_dma,
	    nitems(sc->txq) * sizeof(struct iwm_agn_scd_bc_tbl), 1024);
	return rv;
}

static void
iwm_free_sched(struct iwm_softc *sc)
{
	iwm_dma_contig_free(&sc->sched_dma);
}

/* keep-warm page is used internally by the card.  see iwl-fh.h for more info */
static int
iwm_alloc_kw(struct iwm_softc *sc)
{
	return iwm_dma_contig_alloc(sc->sc_dmat, &sc->kw_dma, 4096, 4096);
}

static void
iwm_free_kw(struct iwm_softc *sc)
{
	iwm_dma_contig_free(&sc->kw_dma);
}

/* interrupt cause table */
static int
iwm_alloc_ict(struct iwm_softc *sc)
{
	return iwm_dma_contig_alloc(sc->sc_dmat, &sc->ict_dma,
	    IWM_ICT_SIZE, 1<<IWM_ICT_PADDR_SHIFT);
}

static void
iwm_free_ict(struct iwm_softc *sc)
{
	iwm_dma_contig_free(&sc->ict_dma);
}

static int
iwm_alloc_rx_ring(struct iwm_softc *sc, struct iwm_rx_ring *ring)
{
	bus_size_t size;
	int i, error;

	ring->cur = 0;

	/* Allocate RX descriptors (256-byte aligned). */
	size = IWM_RX_RING_COUNT * sizeof(uint32_t);
	error = iwm_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma, size, 256);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate RX ring DMA memory\n");
		goto fail;
	}
	ring->desc = ring->desc_dma.vaddr;

	/* Allocate RX status area (16-byte aligned). */
	error = iwm_dma_contig_alloc(sc->sc_dmat, &ring->stat_dma,
	    sizeof(*ring->stat), 16);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate RX status DMA memory\n");
		goto fail;
	}
	ring->stat = ring->stat_dma.vaddr;

        /* Create RX buffer DMA tag. */
        error = bus_dma_tag_create(sc->sc_dmat, 1, 0,
            BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
            IWM_RBUF_SIZE, 1, IWM_RBUF_SIZE, 0, NULL, NULL, &ring->data_dmat);
        if (error != 0) {
                device_printf(sc->sc_dev,
                    "%s: could not create RX buf DMA tag, error %d\n",
                    __func__, error);
                goto fail;
        }

	/*
	 * Allocate and map RX buffers.
	 */
	for (i = 0; i < IWM_RX_RING_COUNT; i++) {
		if ((error = iwm_rx_addbuf(sc, IWM_RBUF_SIZE, i)) != 0) {
			goto fail;
		}
	}
	return 0;

fail:	iwm_free_rx_ring(sc, ring);
	return error;
}

static void
iwm_disable_rx_dma(struct iwm_softc *sc)
{

	/* XXX print out if we can't lock the NIC? */
	if (iwm_nic_lock(sc)) {
		/* XXX handle if RX stop doesn't finish? */
		(void) iwm_pcie_rx_stop(sc);
		iwm_nic_unlock(sc);
	}
}

static void
iwm_reset_rx_ring(struct iwm_softc *sc, struct iwm_rx_ring *ring)
{
	/* Reset the ring state */
	ring->cur = 0;
	memset(sc->rxq.stat, 0, sizeof(*sc->rxq.stat));
}

static void
iwm_free_rx_ring(struct iwm_softc *sc, struct iwm_rx_ring *ring)
{
	int i;

	iwm_dma_contig_free(&ring->desc_dma);
	iwm_dma_contig_free(&ring->stat_dma);

	for (i = 0; i < IWM_RX_RING_COUNT; i++) {
		struct iwm_rx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->data_dmat, data->map,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(ring->data_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
		if (data->map != NULL) {
			bus_dmamap_destroy(ring->data_dmat, data->map);
			data->map = NULL;
		}
	}
	if (ring->data_dmat != NULL) {
		bus_dma_tag_destroy(ring->data_dmat);
		ring->data_dmat = NULL;
	}
}

static int
iwm_alloc_tx_ring(struct iwm_softc *sc, struct iwm_tx_ring *ring, int qid)
{
	bus_addr_t paddr;
	bus_size_t size;
	int i, error;

	ring->qid = qid;
	ring->queued = 0;
	ring->cur = 0;

	/* Allocate TX descriptors (256-byte aligned). */
	size = IWM_TX_RING_COUNT * sizeof (struct iwm_tfd);
	error = iwm_dma_contig_alloc(sc->sc_dmat, &ring->desc_dma, size, 256);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate TX ring DMA memory\n");
		goto fail;
	}
	ring->desc = ring->desc_dma.vaddr;

	/*
	 * We only use rings 0 through 9 (4 EDCA + cmd) so there is no need
	 * to allocate commands space for other rings.
	 */
	if (qid > IWM_MVM_CMD_QUEUE)
		return 0;

	size = IWM_TX_RING_COUNT * sizeof(struct iwm_device_cmd);
	error = iwm_dma_contig_alloc(sc->sc_dmat, &ring->cmd_dma, size, 4);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "could not allocate TX cmd DMA memory\n");
		goto fail;
	}
	ring->cmd = ring->cmd_dma.vaddr;

	error = bus_dma_tag_create(sc->sc_dmat, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES,
            IWM_MAX_SCATTER - 2, MCLBYTES, 0, NULL, NULL, &ring->data_dmat);
	if (error != 0) {
		device_printf(sc->sc_dev, "could not create TX buf DMA tag\n");
		goto fail;
	}

	paddr = ring->cmd_dma.paddr;
	for (i = 0; i < IWM_TX_RING_COUNT; i++) {
		struct iwm_tx_data *data = &ring->data[i];

		data->cmd_paddr = paddr;
		data->scratch_paddr = paddr + sizeof(struct iwm_cmd_header)
		    + offsetof(struct iwm_tx_cmd, scratch);
		paddr += sizeof(struct iwm_device_cmd);

		error = bus_dmamap_create(ring->data_dmat, 0, &data->map);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "could not create TX buf DMA map\n");
			goto fail;
		}
	}
	KASSERT(paddr == ring->cmd_dma.paddr + size,
	    ("invalid physical address"));
	return 0;

fail:	iwm_free_tx_ring(sc, ring);
	return error;
}

static void
iwm_reset_tx_ring(struct iwm_softc *sc, struct iwm_tx_ring *ring)
{
	int i;

	for (i = 0; i < IWM_TX_RING_COUNT; i++) {
		struct iwm_tx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->data_dmat, data->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->data_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
	}
	/* Clear TX descriptors. */
	memset(ring->desc, 0, ring->desc_dma.size);
	bus_dmamap_sync(ring->desc_dma.tag, ring->desc_dma.map,
	    BUS_DMASYNC_PREWRITE);
	sc->qfullmsk &= ~(1 << ring->qid);
	ring->queued = 0;
	ring->cur = 0;
}

static void
iwm_free_tx_ring(struct iwm_softc *sc, struct iwm_tx_ring *ring)
{
	int i;

	iwm_dma_contig_free(&ring->desc_dma);
	iwm_dma_contig_free(&ring->cmd_dma);

	for (i = 0; i < IWM_TX_RING_COUNT; i++) {
		struct iwm_tx_data *data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->data_dmat, data->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->data_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}
		if (data->map != NULL) {
			bus_dmamap_destroy(ring->data_dmat, data->map);
			data->map = NULL;
		}
	}
	if (ring->data_dmat != NULL) {
		bus_dma_tag_destroy(ring->data_dmat);
		ring->data_dmat = NULL;
	}
}

/*
 * High-level hardware frobbing routines
 */

static void
iwm_enable_interrupts(struct iwm_softc *sc)
{
	sc->sc_intmask = IWM_CSR_INI_SET_MASK;
	IWM_WRITE(sc, IWM_CSR_INT_MASK, sc->sc_intmask);
}

static void
iwm_restore_interrupts(struct iwm_softc *sc)
{
	IWM_WRITE(sc, IWM_CSR_INT_MASK, sc->sc_intmask);
}

static void
iwm_disable_interrupts(struct iwm_softc *sc)
{
	/* disable interrupts */
	IWM_WRITE(sc, IWM_CSR_INT_MASK, 0);

	/* acknowledge all interrupts */
	IWM_WRITE(sc, IWM_CSR_INT, ~0);
	IWM_WRITE(sc, IWM_CSR_FH_INT_STATUS, ~0);
}

static void
iwm_ict_reset(struct iwm_softc *sc)
{
	iwm_disable_interrupts(sc);

	/* Reset ICT table. */
	memset(sc->ict_dma.vaddr, 0, IWM_ICT_SIZE);
	sc->ict_cur = 0;

	/* Set physical address of ICT table (4KB aligned). */
	IWM_WRITE(sc, IWM_CSR_DRAM_INT_TBL_REG,
	    IWM_CSR_DRAM_INT_TBL_ENABLE
	    | IWM_CSR_DRAM_INIT_TBL_WRAP_CHECK
	    | sc->ict_dma.paddr >> IWM_ICT_PADDR_SHIFT);

	/* Switch to ICT interrupt mode in driver. */
	sc->sc_flags |= IWM_FLAG_USE_ICT;

	/* Re-enable interrupts. */
	IWM_WRITE(sc, IWM_CSR_INT, ~0);
	iwm_enable_interrupts(sc);
}

/* iwlwifi pcie/trans.c */

/*
 * Since this .. hard-resets things, it's time to actually
 * mark the first vap (if any) as having no mac context.
 * It's annoying, but since the driver is potentially being
 * stop/start'ed whilst active (thanks openbsd port!) we
 * have to correctly track this.
 */
static void
iwm_stop_device(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	int chnl, ntries;
	int qid;

	/* tell the device to stop sending interrupts */
	iwm_disable_interrupts(sc);

	/*
	 * FreeBSD-local: mark the first vap as not-uploaded,
	 * so the next transition through auth/assoc
	 * will correctly populate the MAC context.
	 */
	if (vap) {
		struct iwm_vap *iv = IWM_VAP(vap);
		iv->is_uploaded = 0;
	}

	/* device going down, Stop using ICT table */
	sc->sc_flags &= ~IWM_FLAG_USE_ICT;

	/* stop tx and rx.  tx and rx bits, as usual, are from if_iwn */

	iwm_write_prph(sc, IWM_SCD_TXFACT, 0);

	/* Stop all DMA channels. */
	if (iwm_nic_lock(sc)) {
		for (chnl = 0; chnl < IWM_FH_TCSR_CHNL_NUM; chnl++) {
			IWM_WRITE(sc,
			    IWM_FH_TCSR_CHNL_TX_CONFIG_REG(chnl), 0);
			for (ntries = 0; ntries < 200; ntries++) {
				uint32_t r;

				r = IWM_READ(sc, IWM_FH_TSSR_TX_STATUS_REG);
				if (r & IWM_FH_TSSR_TX_STATUS_REG_MSK_CHNL_IDLE(
				    chnl))
					break;
				DELAY(20);
			}
		}
		iwm_nic_unlock(sc);
	}
	iwm_disable_rx_dma(sc);

	/* Stop RX ring. */
	iwm_reset_rx_ring(sc, &sc->rxq);

	/* Reset all TX rings. */
	for (qid = 0; qid < nitems(sc->txq); qid++)
		iwm_reset_tx_ring(sc, &sc->txq[qid]);

	/*
	 * Power-down device's busmaster DMA clocks
	 */
	iwm_write_prph(sc, IWM_APMG_CLK_DIS_REG, IWM_APMG_CLK_VAL_DMA_CLK_RQT);
	DELAY(5);

	/* Make sure (redundant) we've released our request to stay awake */
	IWM_CLRBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);

	/* Stop the device, and put it in low power state */
	iwm_apm_stop(sc);

	/* Upon stop, the APM issues an interrupt if HW RF kill is set.
	 * Clean again the interrupt here
	 */
	iwm_disable_interrupts(sc);
	/* stop and reset the on-board processor */
	IWM_WRITE(sc, IWM_CSR_RESET, IWM_CSR_RESET_REG_FLAG_NEVO_RESET);

	/*
	 * Even if we stop the HW, we still want the RF kill
	 * interrupt
	 */
	iwm_enable_rfkill_int(sc);
	iwm_check_rfkill(sc);
}

/* iwlwifi: mvm/ops.c */
static void
iwm_mvm_nic_config(struct iwm_softc *sc)
{
	uint8_t radio_cfg_type, radio_cfg_step, radio_cfg_dash;
	uint32_t reg_val = 0;

	radio_cfg_type = (sc->sc_fw_phy_config & IWM_FW_PHY_CFG_RADIO_TYPE) >>
	    IWM_FW_PHY_CFG_RADIO_TYPE_POS;
	radio_cfg_step = (sc->sc_fw_phy_config & IWM_FW_PHY_CFG_RADIO_STEP) >>
	    IWM_FW_PHY_CFG_RADIO_STEP_POS;
	radio_cfg_dash = (sc->sc_fw_phy_config & IWM_FW_PHY_CFG_RADIO_DASH) >>
	    IWM_FW_PHY_CFG_RADIO_DASH_POS;

	/* SKU control */
	reg_val |= IWM_CSR_HW_REV_STEP(sc->sc_hw_rev) <<
	    IWM_CSR_HW_IF_CONFIG_REG_POS_MAC_STEP;
	reg_val |= IWM_CSR_HW_REV_DASH(sc->sc_hw_rev) <<
	    IWM_CSR_HW_IF_CONFIG_REG_POS_MAC_DASH;

	/* radio configuration */
	reg_val |= radio_cfg_type << IWM_CSR_HW_IF_CONFIG_REG_POS_PHY_TYPE;
	reg_val |= radio_cfg_step << IWM_CSR_HW_IF_CONFIG_REG_POS_PHY_STEP;
	reg_val |= radio_cfg_dash << IWM_CSR_HW_IF_CONFIG_REG_POS_PHY_DASH;

	IWM_WRITE(sc, IWM_CSR_HW_IF_CONFIG_REG, reg_val);

	IWM_DPRINTF(sc, IWM_DEBUG_RESET,
	    "Radio type=0x%x-0x%x-0x%x\n", radio_cfg_type,
	    radio_cfg_step, radio_cfg_dash);

	/*
	 * W/A : NIC is stuck in a reset state after Early PCIe power off
	 * (PCIe power is lost before PERST# is asserted), causing ME FW
	 * to lose ownership and not being able to obtain it back.
	 */
	iwm_set_bits_mask_prph(sc, IWM_APMG_PS_CTRL_REG,
	    IWM_APMG_PS_CTRL_EARLY_PWR_OFF_RESET_DIS,
	    ~IWM_APMG_PS_CTRL_EARLY_PWR_OFF_RESET_DIS);
}

static int
iwm_nic_rx_init(struct iwm_softc *sc)
{
	if (!iwm_nic_lock(sc))
		return EBUSY;

	/*
	 * Initialize RX ring.  This is from the iwn driver.
	 */
	memset(sc->rxq.stat, 0, sizeof(*sc->rxq.stat));

	/* stop DMA */
	iwm_disable_rx_dma(sc);
	IWM_WRITE(sc, IWM_FH_MEM_RCSR_CHNL0_RBDCB_WPTR, 0);
	IWM_WRITE(sc, IWM_FH_MEM_RCSR_CHNL0_FLUSH_RB_REQ, 0);
	IWM_WRITE(sc, IWM_FH_RSCSR_CHNL0_RDPTR, 0);
	IWM_WRITE(sc, IWM_FH_RSCSR_CHNL0_RBDCB_WPTR_REG, 0);

	/* Set physical address of RX ring (256-byte aligned). */
	IWM_WRITE(sc,
	    IWM_FH_RSCSR_CHNL0_RBDCB_BASE_REG, sc->rxq.desc_dma.paddr >> 8);

	/* Set physical address of RX status (16-byte aligned). */
	IWM_WRITE(sc,
	    IWM_FH_RSCSR_CHNL0_STTS_WPTR_REG, sc->rxq.stat_dma.paddr >> 4);

	/* Enable RX. */
	/*
	 * Note: Linux driver also sets this:
	 *  (IWM_RX_RB_TIMEOUT << IWM_FH_RCSR_RX_CONFIG_REG_IRQ_RBTH_POS) |
	 *
	 * It causes weird behavior.  YMMV.
	 */
	IWM_WRITE(sc, IWM_FH_MEM_RCSR_CHNL0_CONFIG_REG,
	    IWM_FH_RCSR_RX_CONFIG_CHNL_EN_ENABLE_VAL		|
	    IWM_FH_RCSR_CHNL0_RX_IGNORE_RXF_EMPTY		|  /* HW bug */
	    IWM_FH_RCSR_CHNL0_RX_CONFIG_IRQ_DEST_INT_HOST_VAL	|
	    IWM_FH_RCSR_RX_CONFIG_REG_VAL_RB_SIZE_4K		|
	    IWM_RX_QUEUE_SIZE_LOG << IWM_FH_RCSR_RX_CONFIG_RBDCB_SIZE_POS);

	IWM_WRITE_1(sc, IWM_CSR_INT_COALESCING, IWM_HOST_INT_TIMEOUT_DEF);

	/* W/A for interrupt coalescing bug in 7260 and 3160 */
	if (sc->host_interrupt_operation_mode)
		IWM_SETBITS(sc, IWM_CSR_INT_COALESCING, IWM_HOST_INT_OPER_MODE);

	/*
	 * Thus sayeth el jefe (iwlwifi) via a comment:
	 *
	 * This value should initially be 0 (before preparing any
 	 * RBs), should be 8 after preparing the first 8 RBs (for example)
	 */
	IWM_WRITE(sc, IWM_FH_RSCSR_CHNL0_WPTR, 8);

	iwm_nic_unlock(sc);

	return 0;
}

static int
iwm_nic_tx_init(struct iwm_softc *sc)
{
	int qid;

	if (!iwm_nic_lock(sc))
		return EBUSY;

	/* Deactivate TX scheduler. */
	iwm_write_prph(sc, IWM_SCD_TXFACT, 0);

	/* Set physical address of "keep warm" page (16-byte aligned). */
	IWM_WRITE(sc, IWM_FH_KW_MEM_ADDR_REG, sc->kw_dma.paddr >> 4);

	/* Initialize TX rings. */
	for (qid = 0; qid < nitems(sc->txq); qid++) {
		struct iwm_tx_ring *txq = &sc->txq[qid];

		/* Set physical address of TX ring (256-byte aligned). */
		IWM_WRITE(sc, IWM_FH_MEM_CBBC_QUEUE(qid),
		    txq->desc_dma.paddr >> 8);
		IWM_DPRINTF(sc, IWM_DEBUG_XMIT,
		    "%s: loading ring %d descriptors (%p) at %lx\n",
		    __func__,
		    qid, txq->desc,
		    (unsigned long) (txq->desc_dma.paddr >> 8));
	}
	iwm_nic_unlock(sc);

	return 0;
}

static int
iwm_nic_init(struct iwm_softc *sc)
{
	int error;

	iwm_apm_init(sc);
	iwm_set_pwr(sc);

	iwm_mvm_nic_config(sc);

	if ((error = iwm_nic_rx_init(sc)) != 0)
		return error;

	/*
	 * Ditto for TX, from iwn
	 */
	if ((error = iwm_nic_tx_init(sc)) != 0)
		return error;

	IWM_DPRINTF(sc, IWM_DEBUG_RESET,
	    "%s: shadow registers enabled\n", __func__);
	IWM_SETBITS(sc, IWM_CSR_MAC_SHADOW_REG_CTRL, 0x800fffff);

	return 0;
}

enum iwm_mvm_tx_fifo {
	IWM_MVM_TX_FIFO_BK = 0,
	IWM_MVM_TX_FIFO_BE,
	IWM_MVM_TX_FIFO_VI,
	IWM_MVM_TX_FIFO_VO,
	IWM_MVM_TX_FIFO_MCAST = 5,
};

const uint8_t iwm_mvm_ac_to_tx_fifo[] = {
	IWM_MVM_TX_FIFO_VO,
	IWM_MVM_TX_FIFO_VI,
	IWM_MVM_TX_FIFO_BE,
	IWM_MVM_TX_FIFO_BK,
};

static void
iwm_enable_txq(struct iwm_softc *sc, int qid, int fifo)
{
	if (!iwm_nic_lock(sc)) {
		device_printf(sc->sc_dev,
		    "%s: cannot enable txq %d\n",
		    __func__,
		    qid);
		return; /* XXX return EBUSY */
	}

	/* unactivate before configuration */
	iwm_write_prph(sc, IWM_SCD_QUEUE_STATUS_BITS(qid),
	    (0 << IWM_SCD_QUEUE_STTS_REG_POS_ACTIVE)
	    | (1 << IWM_SCD_QUEUE_STTS_REG_POS_SCD_ACT_EN));

	if (qid != IWM_MVM_CMD_QUEUE) {
		iwm_set_bits_prph(sc, IWM_SCD_QUEUECHAIN_SEL, (1 << qid));
	}

	iwm_clear_bits_prph(sc, IWM_SCD_AGGR_SEL, (1 << qid));

	IWM_WRITE(sc, IWM_HBUS_TARG_WRPTR, qid << 8 | 0);
	iwm_write_prph(sc, IWM_SCD_QUEUE_RDPTR(qid), 0);

	iwm_write_mem32(sc, sc->sched_base + IWM_SCD_CONTEXT_QUEUE_OFFSET(qid), 0);
	/* Set scheduler window size and frame limit. */
	iwm_write_mem32(sc,
	    sc->sched_base + IWM_SCD_CONTEXT_QUEUE_OFFSET(qid) +
	    sizeof(uint32_t),
	    ((IWM_FRAME_LIMIT << IWM_SCD_QUEUE_CTX_REG2_WIN_SIZE_POS) &
	    IWM_SCD_QUEUE_CTX_REG2_WIN_SIZE_MSK) |
	    ((IWM_FRAME_LIMIT << IWM_SCD_QUEUE_CTX_REG2_FRAME_LIMIT_POS) &
	    IWM_SCD_QUEUE_CTX_REG2_FRAME_LIMIT_MSK));

	iwm_write_prph(sc, IWM_SCD_QUEUE_STATUS_BITS(qid),
	    (1 << IWM_SCD_QUEUE_STTS_REG_POS_ACTIVE) |
	    (fifo << IWM_SCD_QUEUE_STTS_REG_POS_TXF) |
	    (1 << IWM_SCD_QUEUE_STTS_REG_POS_WSL) |
	    IWM_SCD_QUEUE_STTS_REG_MSK);

	iwm_nic_unlock(sc);

	IWM_DPRINTF(sc, IWM_DEBUG_XMIT,
	    "%s: enabled txq %d FIFO %d\n",
	    __func__, qid, fifo);
}

static int
iwm_post_alive(struct iwm_softc *sc)
{
	int nwords;
	int error, chnl;

	if (!iwm_nic_lock(sc))
		return EBUSY;

	if (sc->sched_base != iwm_read_prph(sc, IWM_SCD_SRAM_BASE_ADDR)) {
		device_printf(sc->sc_dev,
		    "%s: sched addr mismatch",
		    __func__);
		error = EINVAL;
		goto out;
	}

	iwm_ict_reset(sc);

	/* Clear TX scheduler state in SRAM. */
	nwords = (IWM_SCD_TRANS_TBL_MEM_UPPER_BOUND -
	    IWM_SCD_CONTEXT_MEM_LOWER_BOUND)
	    / sizeof(uint32_t);
	error = iwm_write_mem(sc,
	    sc->sched_base + IWM_SCD_CONTEXT_MEM_LOWER_BOUND,
	    NULL, nwords);
	if (error)
		goto out;

	/* Set physical address of TX scheduler rings (1KB aligned). */
	iwm_write_prph(sc, IWM_SCD_DRAM_BASE_ADDR, sc->sched_dma.paddr >> 10);

	iwm_write_prph(sc, IWM_SCD_CHAINEXT_EN, 0);

	/* enable command channel */
	iwm_enable_txq(sc, IWM_MVM_CMD_QUEUE, 7);

	iwm_write_prph(sc, IWM_SCD_TXFACT, 0xff);

	/* Enable DMA channels. */
	for (chnl = 0; chnl < IWM_FH_TCSR_CHNL_NUM; chnl++) {
		IWM_WRITE(sc, IWM_FH_TCSR_CHNL_TX_CONFIG_REG(chnl),
		    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE |
		    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_ENABLE);
	}

	IWM_SETBITS(sc, IWM_FH_TX_CHICKEN_BITS_REG,
	    IWM_FH_TX_CHICKEN_BITS_SCD_AUTO_RETRY_EN);

	/* Enable L1-Active */
	iwm_clear_bits_prph(sc, IWM_APMG_PCIDEV_STT_REG,
	    IWM_APMG_PCIDEV_STT_VAL_L1_ACT_DIS);

 out:
 	iwm_nic_unlock(sc);
	return error;
}

/*
 * NVM read access and content parsing.  We do not support
 * external NVM or writing NVM.
 * iwlwifi/mvm/nvm.c
 */

/* list of NVM sections we are allowed/need to read */
const int nvm_to_read[] = {
	IWM_NVM_SECTION_TYPE_HW,
	IWM_NVM_SECTION_TYPE_SW,
	IWM_NVM_SECTION_TYPE_CALIBRATION,
	IWM_NVM_SECTION_TYPE_PRODUCTION,
};

/* Default NVM size to read */
#define IWM_NVM_DEFAULT_CHUNK_SIZE (2*1024)
#define IWM_MAX_NVM_SECTION_SIZE 7000

#define IWM_NVM_WRITE_OPCODE 1
#define IWM_NVM_READ_OPCODE 0

static int
iwm_nvm_read_chunk(struct iwm_softc *sc, uint16_t section,
	uint16_t offset, uint16_t length, uint8_t *data, uint16_t *len)
{
	offset = 0;
	struct iwm_nvm_access_cmd nvm_access_cmd = {
		.offset = htole16(offset),
		.length = htole16(length),
		.type = htole16(section),
		.op_code = IWM_NVM_READ_OPCODE,
	};
	struct iwm_nvm_access_resp *nvm_resp;
	struct iwm_rx_packet *pkt;
	struct iwm_host_cmd cmd = {
		.id = IWM_NVM_ACCESS_CMD,
		.flags = IWM_CMD_SYNC | IWM_CMD_WANT_SKB |
		    IWM_CMD_SEND_IN_RFKILL,
		.data = { &nvm_access_cmd, },
	};
	int ret, bytes_read, offset_read;
	uint8_t *resp_data;

	cmd.len[0] = sizeof(struct iwm_nvm_access_cmd);

	ret = iwm_send_cmd(sc, &cmd);
	if (ret)
		return ret;

	pkt = cmd.resp_pkt;
	if (pkt->hdr.flags & IWM_CMD_FAILED_MSK) {
		device_printf(sc->sc_dev,
		    "%s: Bad return from IWM_NVM_ACCES_COMMAND (0x%08X)\n",
		    __func__, pkt->hdr.flags);
		ret = EIO;
		goto exit;
	}

	/* Extract NVM response */
	nvm_resp = (void *)pkt->data;

	ret = le16toh(nvm_resp->status);
	bytes_read = le16toh(nvm_resp->length);
	offset_read = le16toh(nvm_resp->offset);
	resp_data = nvm_resp->data;
	if (ret) {
		device_printf(sc->sc_dev,
		    "%s: NVM access command failed with status %d\n",
		    __func__, ret);
		ret = EINVAL;
		goto exit;
	}

	if (offset_read != offset) {
		device_printf(sc->sc_dev,
		    "%s: NVM ACCESS response with invalid offset %d\n",
		    __func__, offset_read);
		ret = EINVAL;
		goto exit;
	}

	memcpy(data + offset, resp_data, bytes_read);
	*len = bytes_read;

 exit:
	iwm_free_resp(sc, &cmd);
	return ret;
}

/*
 * Reads an NVM section completely.
 * NICs prior to 7000 family doesn't have a real NVM, but just read
 * section 0 which is the EEPROM. Because the EEPROM reading is unlimited
 * by uCode, we need to manually check in this case that we don't
 * overflow and try to read more than the EEPROM size.
 * For 7000 family NICs, we supply the maximal size we can read, and
 * the uCode fills the response with as much data as we can,
 * without overflowing, so no check is needed.
 */
static int
iwm_nvm_read_section(struct iwm_softc *sc,
	uint16_t section, uint8_t *data, uint16_t *len)
{
	uint16_t length, seglen;
	int error;

	/* Set nvm section read length */
	length = seglen = IWM_NVM_DEFAULT_CHUNK_SIZE;
	*len = 0;

	/* Read the NVM until exhausted (reading less than requested) */
	while (seglen == length) {
		error = iwm_nvm_read_chunk(sc,
		    section, *len, length, data, &seglen);
		if (error) {
			device_printf(sc->sc_dev,
			    "Cannot read NVM from section "
			    "%d offset %d, length %d\n",
			    section, *len, length);
			return error;
		}
		*len += seglen;
	}

	IWM_DPRINTF(sc, IWM_DEBUG_RESET,
	    "NVM section %d read completed\n", section);
	return 0;
}

/*
 * BEGIN IWM_NVM_PARSE
 */

/* iwlwifi/iwl-nvm-parse.c */

/* NVM offsets (in words) definitions */
enum wkp_nvm_offsets {
	/* NVM HW-Section offset (in words) definitions */
	IWM_HW_ADDR = 0x15,

/* NVM SW-Section offset (in words) definitions */
	IWM_NVM_SW_SECTION = 0x1C0,
	IWM_NVM_VERSION = 0,
	IWM_RADIO_CFG = 1,
	IWM_SKU = 2,
	IWM_N_HW_ADDRS = 3,
	IWM_NVM_CHANNELS = 0x1E0 - IWM_NVM_SW_SECTION,

/* NVM calibration section offset (in words) definitions */
	IWM_NVM_CALIB_SECTION = 0x2B8,
	IWM_XTAL_CALIB = 0x316 - IWM_NVM_CALIB_SECTION
};

/* SKU Capabilities (actual values from NVM definition) */
enum nvm_sku_bits {
	IWM_NVM_SKU_CAP_BAND_24GHZ	= (1 << 0),
	IWM_NVM_SKU_CAP_BAND_52GHZ	= (1 << 1),
	IWM_NVM_SKU_CAP_11N_ENABLE	= (1 << 2),
	IWM_NVM_SKU_CAP_11AC_ENABLE	= (1 << 3),
};

/* radio config bits (actual values from NVM definition) */
#define IWM_NVM_RF_CFG_DASH_MSK(x)   (x & 0x3)         /* bits 0-1   */
#define IWM_NVM_RF_CFG_STEP_MSK(x)   ((x >> 2)  & 0x3) /* bits 2-3   */
#define IWM_NVM_RF_CFG_TYPE_MSK(x)   ((x >> 4)  & 0x3) /* bits 4-5   */
#define IWM_NVM_RF_CFG_PNUM_MSK(x)   ((x >> 6)  & 0x3) /* bits 6-7   */
#define IWM_NVM_RF_CFG_TX_ANT_MSK(x) ((x >> 8)  & 0xF) /* bits 8-11  */
#define IWM_NVM_RF_CFG_RX_ANT_MSK(x) ((x >> 12) & 0xF) /* bits 12-15 */

#define DEFAULT_MAX_TX_POWER 16

/**
 * enum iwm_nvm_channel_flags - channel flags in NVM
 * @IWM_NVM_CHANNEL_VALID: channel is usable for this SKU/geo
 * @IWM_NVM_CHANNEL_IBSS: usable as an IBSS channel
 * @IWM_NVM_CHANNEL_ACTIVE: active scanning allowed
 * @IWM_NVM_CHANNEL_RADAR: radar detection required
 * XXX cannot find this (DFS) flag in iwl-nvm-parse.c
 * @IWM_NVM_CHANNEL_DFS: dynamic freq selection candidate
 * @IWM_NVM_CHANNEL_WIDE: 20 MHz channel okay (?)
 * @IWM_NVM_CHANNEL_40MHZ: 40 MHz channel okay (?)
 * @IWM_NVM_CHANNEL_80MHZ: 80 MHz channel okay (?)
 * @IWM_NVM_CHANNEL_160MHZ: 160 MHz channel okay (?)
 */
enum iwm_nvm_channel_flags {
	IWM_NVM_CHANNEL_VALID = (1 << 0),
	IWM_NVM_CHANNEL_IBSS = (1 << 1),
	IWM_NVM_CHANNEL_ACTIVE = (1 << 3),
	IWM_NVM_CHANNEL_RADAR = (1 << 4),
	IWM_NVM_CHANNEL_DFS = (1 << 7),
	IWM_NVM_CHANNEL_WIDE = (1 << 8),
	IWM_NVM_CHANNEL_40MHZ = (1 << 9),
	IWM_NVM_CHANNEL_80MHZ = (1 << 10),
	IWM_NVM_CHANNEL_160MHZ = (1 << 11),
};

/*
 * Translate EEPROM flags to net80211.
 */
static uint32_t
iwm_eeprom_channel_flags(uint16_t ch_flags)
{
	uint32_t nflags;

	nflags = 0;
	if ((ch_flags & IWM_NVM_CHANNEL_ACTIVE) == 0)
		nflags |= IEEE80211_CHAN_PASSIVE;
	if ((ch_flags & IWM_NVM_CHANNEL_IBSS) == 0)
		nflags |= IEEE80211_CHAN_NOADHOC;
	if (ch_flags & IWM_NVM_CHANNEL_RADAR) {
		nflags |= IEEE80211_CHAN_DFS;
		/* Just in case. */
		nflags |= IEEE80211_CHAN_NOADHOC;
	}

	return (nflags);
}

static void
iwm_add_channel_band(struct iwm_softc *sc, struct ieee80211_channel chans[],
    int maxchans, int *nchans, int ch_idx, int ch_num, const uint8_t bands[])
{
	const uint16_t * const nvm_ch_flags = sc->sc_nvm.nvm_ch_flags;
	uint32_t nflags;
	uint16_t ch_flags;
	uint8_t ieee;
	int error;

	for (; ch_idx < ch_num; ch_idx++) {
		ch_flags = le16_to_cpup(nvm_ch_flags + ch_idx);
		ieee = iwm_nvm_channels[ch_idx];

		if (!(ch_flags & IWM_NVM_CHANNEL_VALID)) {
			IWM_DPRINTF(sc, IWM_DEBUG_EEPROM,
			    "Ch. %d Flags %x [%sGHz] - No traffic\n",
			    ieee, ch_flags,
			    (ch_idx >= IWM_NUM_2GHZ_CHANNELS) ?
			    "5.2" : "2.4");
			continue;
		}

		nflags = iwm_eeprom_channel_flags(ch_flags);
		error = ieee80211_add_channel(chans, maxchans, nchans,
		    ieee, 0, 0, nflags, bands);
		if (error != 0)
			break;

		IWM_DPRINTF(sc, IWM_DEBUG_EEPROM,
		    "Ch. %d Flags %x [%sGHz] - Added\n",
		    ieee, ch_flags,
		    (ch_idx >= IWM_NUM_2GHZ_CHANNELS) ?
		    "5.2" : "2.4");
	}
}

static void
iwm_init_channel_map(struct ieee80211com *ic, int maxchans, int *nchans,
    struct ieee80211_channel chans[])
{
	struct iwm_softc *sc = ic->ic_softc;
	struct iwm_nvm_data *data = &sc->sc_nvm;
	uint8_t bands[IEEE80211_MODE_BYTES];

	memset(bands, 0, sizeof(bands));
	/* 1-13: 11b/g channels. */
	setbit(bands, IEEE80211_MODE_11B);
	setbit(bands, IEEE80211_MODE_11G);
	iwm_add_channel_band(sc, chans, maxchans, nchans, 0,
	    IWM_NUM_2GHZ_CHANNELS - 1, bands);

	/* 14: 11b channel only. */
	clrbit(bands, IEEE80211_MODE_11G);
	iwm_add_channel_band(sc, chans, maxchans, nchans,
	    IWM_NUM_2GHZ_CHANNELS - 1, IWM_NUM_2GHZ_CHANNELS, bands);

	if (data->sku_cap_band_52GHz_enable) {
		memset(bands, 0, sizeof(bands));
		setbit(bands, IEEE80211_MODE_11A);
		iwm_add_channel_band(sc, chans, maxchans, nchans,
		    IWM_NUM_2GHZ_CHANNELS, nitems(iwm_nvm_channels), bands);
	}
}

static int
iwm_parse_nvm_data(struct iwm_softc *sc,
	const uint16_t *nvm_hw, const uint16_t *nvm_sw,
	const uint16_t *nvm_calib, uint8_t tx_chains, uint8_t rx_chains)
{
	struct iwm_nvm_data *data = &sc->sc_nvm;
	uint8_t hw_addr[IEEE80211_ADDR_LEN];
	uint16_t radio_cfg, sku;

	data->nvm_version = le16_to_cpup(nvm_sw + IWM_NVM_VERSION);

	radio_cfg = le16_to_cpup(nvm_sw + IWM_RADIO_CFG);
	data->radio_cfg_type = IWM_NVM_RF_CFG_TYPE_MSK(radio_cfg);
	data->radio_cfg_step = IWM_NVM_RF_CFG_STEP_MSK(radio_cfg);
	data->radio_cfg_dash = IWM_NVM_RF_CFG_DASH_MSK(radio_cfg);
	data->radio_cfg_pnum = IWM_NVM_RF_CFG_PNUM_MSK(radio_cfg);
	data->valid_tx_ant = IWM_NVM_RF_CFG_TX_ANT_MSK(radio_cfg);
	data->valid_rx_ant = IWM_NVM_RF_CFG_RX_ANT_MSK(radio_cfg);

	sku = le16_to_cpup(nvm_sw + IWM_SKU);
	data->sku_cap_band_24GHz_enable = sku & IWM_NVM_SKU_CAP_BAND_24GHZ;
	data->sku_cap_band_52GHz_enable = sku & IWM_NVM_SKU_CAP_BAND_52GHZ;
	data->sku_cap_11n_enable = 0;

	if (!data->valid_tx_ant || !data->valid_rx_ant) {
		device_printf(sc->sc_dev,
		    "%s: invalid antennas (0x%x, 0x%x)\n",
		    __func__, data->valid_tx_ant,
		    data->valid_rx_ant);
		return EINVAL;
	}

	data->n_hw_addrs = le16_to_cpup(nvm_sw + IWM_N_HW_ADDRS);

	data->xtal_calib[0] = *(nvm_calib + IWM_XTAL_CALIB);
	data->xtal_calib[1] = *(nvm_calib + IWM_XTAL_CALIB + 1);

	/* The byte order is little endian 16 bit, meaning 214365 */
	IEEE80211_ADDR_COPY(hw_addr, nvm_hw + IWM_HW_ADDR);
	data->hw_addr[0] = hw_addr[1];
	data->hw_addr[1] = hw_addr[0];
	data->hw_addr[2] = hw_addr[3];
	data->hw_addr[3] = hw_addr[2];
	data->hw_addr[4] = hw_addr[5];
	data->hw_addr[5] = hw_addr[4];

	memcpy(data->nvm_ch_flags, &nvm_sw[IWM_NVM_CHANNELS],
	    sizeof(data->nvm_ch_flags));
	data->calib_version = 255;   /* TODO:
					this value will prevent some checks from
					failing, we need to check if this
					field is still needed, and if it does,
					where is it in the NVM */

	return 0;
}

/*
 * END NVM PARSE
 */

struct iwm_nvm_section {
	uint16_t length;
	const uint8_t *data;
};

static int
iwm_parse_nvm_sections(struct iwm_softc *sc, struct iwm_nvm_section *sections)
{
	const uint16_t *hw, *sw, *calib;

	/* Checking for required sections */
	if (!sections[IWM_NVM_SECTION_TYPE_SW].data ||
	    !sections[IWM_NVM_SECTION_TYPE_HW].data) {
		device_printf(sc->sc_dev,
		    "%s: Can't parse empty NVM sections\n",
		    __func__);
		return ENOENT;
	}

	hw = (const uint16_t *)sections[IWM_NVM_SECTION_TYPE_HW].data;
	sw = (const uint16_t *)sections[IWM_NVM_SECTION_TYPE_SW].data;
	calib = (const uint16_t *)sections[IWM_NVM_SECTION_TYPE_CALIBRATION].data;
	return iwm_parse_nvm_data(sc, hw, sw, calib,
	    IWM_FW_VALID_TX_ANT(sc), IWM_FW_VALID_RX_ANT(sc));
}

static int
iwm_nvm_init(struct iwm_softc *sc)
{
	struct iwm_nvm_section nvm_sections[IWM_NVM_NUM_OF_SECTIONS];
	int i, section, error;
	uint16_t len;
	uint8_t *nvm_buffer, *temp;

	/* Read From FW NVM */
	IWM_DPRINTF(sc, IWM_DEBUG_EEPROM,
	    "%s: Read NVM\n",
	    __func__);

	/* TODO: find correct NVM max size for a section */
	nvm_buffer = malloc(IWM_OTP_LOW_IMAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (nvm_buffer == NULL)
		return (ENOMEM);
	for (i = 0; i < nitems(nvm_to_read); i++) {
		section = nvm_to_read[i];
		KASSERT(section <= nitems(nvm_sections),
		    ("too many sections"));

		error = iwm_nvm_read_section(sc, section, nvm_buffer, &len);
		if (error)
			break;

		temp = malloc(len, M_DEVBUF, M_NOWAIT);
		if (temp == NULL) {
			error = ENOMEM;
			break;
		}
		memcpy(temp, nvm_buffer, len);
		nvm_sections[section].data = temp;
		nvm_sections[section].length = len;
	}
	free(nvm_buffer, M_DEVBUF);
	if (error)
		return error;

	return iwm_parse_nvm_sections(sc, nvm_sections);
}

/*
 * Firmware loading gunk.  This is kind of a weird hybrid between the
 * iwn driver and the Linux iwlwifi driver.
 */

static int
iwm_firmware_load_chunk(struct iwm_softc *sc, uint32_t dst_addr,
	const uint8_t *section, uint32_t byte_cnt)
{
	struct iwm_dma_info *dma = &sc->fw_dma;
	int error;

	/* Copy firmware section into pre-allocated DMA-safe memory. */
	memcpy(dma->vaddr, section, byte_cnt);
	bus_dmamap_sync(dma->tag, dma->map, BUS_DMASYNC_PREWRITE);

	if (!iwm_nic_lock(sc))
		return EBUSY;

	sc->sc_fw_chunk_done = 0;

	IWM_WRITE(sc, IWM_FH_TCSR_CHNL_TX_CONFIG_REG(IWM_FH_SRVC_CHNL),
	    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_PAUSE);
	IWM_WRITE(sc, IWM_FH_SRVC_CHNL_SRAM_ADDR_REG(IWM_FH_SRVC_CHNL),
	    dst_addr);
	IWM_WRITE(sc, IWM_FH_TFDIB_CTRL0_REG(IWM_FH_SRVC_CHNL),
	    dma->paddr & IWM_FH_MEM_TFDIB_DRAM_ADDR_LSB_MSK);
	IWM_WRITE(sc, IWM_FH_TFDIB_CTRL1_REG(IWM_FH_SRVC_CHNL),
	    (iwm_get_dma_hi_addr(dma->paddr)
	      << IWM_FH_MEM_TFDIB_REG1_ADDR_BITSHIFT) | byte_cnt);
	IWM_WRITE(sc, IWM_FH_TCSR_CHNL_TX_BUF_STS_REG(IWM_FH_SRVC_CHNL),
	    1 << IWM_FH_TCSR_CHNL_TX_BUF_STS_REG_POS_TB_NUM |
	    1 << IWM_FH_TCSR_CHNL_TX_BUF_STS_REG_POS_TB_IDX |
	    IWM_FH_TCSR_CHNL_TX_BUF_STS_REG_VAL_TFDB_VALID);
	IWM_WRITE(sc, IWM_FH_TCSR_CHNL_TX_CONFIG_REG(IWM_FH_SRVC_CHNL),
	    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CHNL_ENABLE    |
	    IWM_FH_TCSR_TX_CONFIG_REG_VAL_DMA_CREDIT_DISABLE |
	    IWM_FH_TCSR_TX_CONFIG_REG_VAL_CIRQ_HOST_ENDTFD);

	iwm_nic_unlock(sc);

	/* wait 1s for this segment to load */
	while (!sc->sc_fw_chunk_done)
		if ((error = msleep(&sc->sc_fw, &sc->sc_mtx, 0, "iwmfw", hz)) != 0)
			break;

	return error;
}

static int
iwm_load_firmware(struct iwm_softc *sc, enum iwm_ucode_type ucode_type)
{
	struct iwm_fw_sects *fws;
	int error, i, w;
	const void *data;
	uint32_t dlen;
	uint32_t offset;

	sc->sc_uc.uc_intr = 0;

	fws = &sc->sc_fw.fw_sects[ucode_type];
	for (i = 0; i < fws->fw_count; i++) {
		data = fws->fw_sect[i].fws_data;
		dlen = fws->fw_sect[i].fws_len;
		offset = fws->fw_sect[i].fws_devoff;
		IWM_DPRINTF(sc, IWM_DEBUG_FIRMWARE_TLV,
		    "LOAD FIRMWARE type %d offset %u len %d\n",
		    ucode_type, offset, dlen);
		error = iwm_firmware_load_chunk(sc, offset, data, dlen);
		if (error) {
			device_printf(sc->sc_dev,
			    "%s: chunk %u of %u returned error %02d\n",
			    __func__, i, fws->fw_count, error);
			return error;
		}
	}

	/* wait for the firmware to load */
	IWM_WRITE(sc, IWM_CSR_RESET, 0);

	for (w = 0; !sc->sc_uc.uc_intr && w < 10; w++) {
		error = msleep(&sc->sc_uc, &sc->sc_mtx, 0, "iwmuc", hz/10);
	}

	return error;
}

/* iwlwifi: pcie/trans.c */
static int
iwm_start_fw(struct iwm_softc *sc, enum iwm_ucode_type ucode_type)
{
	int error;

	IWM_WRITE(sc, IWM_CSR_INT, ~0);

	if ((error = iwm_nic_init(sc)) != 0) {
		device_printf(sc->sc_dev, "unable to init nic\n");
		return error;
	}

	/* make sure rfkill handshake bits are cleared */
	IWM_WRITE(sc, IWM_CSR_UCODE_DRV_GP1_CLR, IWM_CSR_UCODE_SW_BIT_RFKILL);
	IWM_WRITE(sc, IWM_CSR_UCODE_DRV_GP1_CLR,
	    IWM_CSR_UCODE_DRV_GP1_BIT_CMD_BLOCKED);

	/* clear (again), then enable host interrupts */
	IWM_WRITE(sc, IWM_CSR_INT, ~0);
	iwm_enable_interrupts(sc);

	/* really make sure rfkill handshake bits are cleared */
	/* maybe we should write a few times more?  just to make sure */
	IWM_WRITE(sc, IWM_CSR_UCODE_DRV_GP1_CLR, IWM_CSR_UCODE_SW_BIT_RFKILL);
	IWM_WRITE(sc, IWM_CSR_UCODE_DRV_GP1_CLR, IWM_CSR_UCODE_SW_BIT_RFKILL);

	/* Load the given image to the HW */
	return iwm_load_firmware(sc, ucode_type);
}

static int
iwm_fw_alive(struct iwm_softc *sc, uint32_t sched_base)
{
	return iwm_post_alive(sc);
}

static int
iwm_send_tx_ant_cfg(struct iwm_softc *sc, uint8_t valid_tx_ant)
{
	struct iwm_tx_ant_cfg_cmd tx_ant_cmd = {
		.valid = htole32(valid_tx_ant),
	};

	return iwm_mvm_send_cmd_pdu(sc, IWM_TX_ANT_CONFIGURATION_CMD,
	    IWM_CMD_SYNC, sizeof(tx_ant_cmd), &tx_ant_cmd);
}

/* iwlwifi: mvm/fw.c */
static int
iwm_send_phy_cfg_cmd(struct iwm_softc *sc)
{
	struct iwm_phy_cfg_cmd phy_cfg_cmd;
	enum iwm_ucode_type ucode_type = sc->sc_uc_current;

	/* Set parameters */
	phy_cfg_cmd.phy_cfg = htole32(sc->sc_fw_phy_config);
	phy_cfg_cmd.calib_control.event_trigger =
	    sc->sc_default_calib[ucode_type].event_trigger;
	phy_cfg_cmd.calib_control.flow_trigger =
	    sc->sc_default_calib[ucode_type].flow_trigger;

	IWM_DPRINTF(sc, IWM_DEBUG_CMD | IWM_DEBUG_RESET,
	    "Sending Phy CFG command: 0x%x\n", phy_cfg_cmd.phy_cfg);
	return iwm_mvm_send_cmd_pdu(sc, IWM_PHY_CONFIGURATION_CMD, IWM_CMD_SYNC,
	    sizeof(phy_cfg_cmd), &phy_cfg_cmd);
}

static int
iwm_mvm_load_ucode_wait_alive(struct iwm_softc *sc,
	enum iwm_ucode_type ucode_type)
{
	enum iwm_ucode_type old_type = sc->sc_uc_current;
	int error;

	if ((error = iwm_read_firmware(sc, ucode_type)) != 0)
		return error;

	sc->sc_uc_current = ucode_type;
	error = iwm_start_fw(sc, ucode_type);
	if (error) {
		sc->sc_uc_current = old_type;
		return error;
	}

	return iwm_fw_alive(sc, sc->sched_base);
}

/*
 * mvm misc bits
 */

/*
 * follows iwlwifi/fw.c
 */
static int
iwm_run_init_mvm_ucode(struct iwm_softc *sc, int justnvm)
{
	int error;

	/* do not operate with rfkill switch turned on */
	if ((sc->sc_flags & IWM_FLAG_RFKILL) && !justnvm) {
		device_printf(sc->sc_dev,
		    "radio is disabled by hardware switch\n");
		return EPERM;
	}

	sc->sc_init_complete = 0;
	if ((error = iwm_mvm_load_ucode_wait_alive(sc,
	    IWM_UCODE_TYPE_INIT)) != 0)
		return error;

	if (justnvm) {
		if ((error = iwm_nvm_init(sc)) != 0) {
			device_printf(sc->sc_dev, "failed to read nvm\n");
			return error;
		}
		IEEE80211_ADDR_COPY(sc->sc_ic.ic_macaddr, sc->sc_nvm.hw_addr);

		sc->sc_scan_cmd_len = sizeof(struct iwm_scan_cmd)
		    + sc->sc_capa_max_probe_len
		    + IWM_MAX_NUM_SCAN_CHANNELS
		    * sizeof(struct iwm_scan_channel);
		sc->sc_scan_cmd = malloc(sc->sc_scan_cmd_len, M_DEVBUF,
		    M_NOWAIT);
		if (sc->sc_scan_cmd == NULL)
			return (ENOMEM);

		return 0;
	}

	/* Send TX valid antennas before triggering calibrations */
	if ((error = iwm_send_tx_ant_cfg(sc, IWM_FW_VALID_TX_ANT(sc))) != 0)
		return error;

	/*
	* Send phy configurations command to init uCode
	* to start the 16.0 uCode init image internal calibrations.
	*/
	if ((error = iwm_send_phy_cfg_cmd(sc)) != 0 ) {
		device_printf(sc->sc_dev,
		    "%s: failed to run internal calibration: %d\n",
		    __func__, error);
		return error;
	}

	/*
	 * Nothing to do but wait for the init complete notification
	 * from the firmware
	 */
	while (!sc->sc_init_complete)
		if ((error = msleep(&sc->sc_init_complete, &sc->sc_mtx,
		    0, "iwminit", 2*hz)) != 0)
			break;

	return error;
}

/*
 * receive side
 */

/* (re)stock rx ring, called at init-time and at runtime */
static int
iwm_rx_addbuf(struct iwm_softc *sc, int size, int idx)
{
	struct iwm_rx_ring *ring = &sc->rxq;
	struct iwm_rx_data *data = &ring->data[idx];
	struct mbuf *m;
	int error;
	bus_addr_t paddr;

	m = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, IWM_RBUF_SIZE);
	if (m == NULL)
		return ENOBUFS;

	if (data->m != NULL)
		bus_dmamap_unload(ring->data_dmat, data->map);

	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
	error = bus_dmamap_create(ring->data_dmat, 0, &data->map);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "%s: could not create RX buf DMA map, error %d\n",
		    __func__, error);
		goto fail;
	}
	data->m = m;
	error = bus_dmamap_load(ring->data_dmat, data->map,
	    mtod(data->m, void *), IWM_RBUF_SIZE, iwm_dma_map_addr,
	    &paddr, BUS_DMA_NOWAIT);
	if (error != 0 && error != EFBIG) {
		device_printf(sc->sc_dev,
		    "%s: can't not map mbuf, error %d\n", __func__,
		    error);
		goto fail;
	}
	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_PREREAD);

	/* Update RX descriptor. */
	ring->desc[idx] = htole32(paddr >> 8);
	bus_dmamap_sync(ring->desc_dma.tag, ring->desc_dma.map,
	    BUS_DMASYNC_PREWRITE);

	return 0;
fail:
	return error;
}

/* iwlwifi: mvm/rx.c */
#define IWM_RSSI_OFFSET 50
static int
iwm_mvm_calc_rssi(struct iwm_softc *sc, struct iwm_rx_phy_info *phy_info)
{
	int rssi_a, rssi_b, rssi_a_dbm, rssi_b_dbm, max_rssi_dbm;
	uint32_t agc_a, agc_b;
	uint32_t val;

	val = le32toh(phy_info->non_cfg_phy[IWM_RX_INFO_AGC_IDX]);
	agc_a = (val & IWM_OFDM_AGC_A_MSK) >> IWM_OFDM_AGC_A_POS;
	agc_b = (val & IWM_OFDM_AGC_B_MSK) >> IWM_OFDM_AGC_B_POS;

	val = le32toh(phy_info->non_cfg_phy[IWM_RX_INFO_RSSI_AB_IDX]);
	rssi_a = (val & IWM_OFDM_RSSI_INBAND_A_MSK) >> IWM_OFDM_RSSI_A_POS;
	rssi_b = (val & IWM_OFDM_RSSI_INBAND_B_MSK) >> IWM_OFDM_RSSI_B_POS;

	/*
	 * dBm = rssi dB - agc dB - constant.
	 * Higher AGC (higher radio gain) means lower signal.
	 */
	rssi_a_dbm = rssi_a - IWM_RSSI_OFFSET - agc_a;
	rssi_b_dbm = rssi_b - IWM_RSSI_OFFSET - agc_b;
	max_rssi_dbm = MAX(rssi_a_dbm, rssi_b_dbm);

	IWM_DPRINTF(sc, IWM_DEBUG_RECV,
	    "Rssi In A %d B %d Max %d AGCA %d AGCB %d\n",
	    rssi_a_dbm, rssi_b_dbm, max_rssi_dbm, agc_a, agc_b);

	return max_rssi_dbm;
}

/* iwlwifi: mvm/rx.c */
/*
 * iwm_mvm_get_signal_strength - use new rx PHY INFO API
 * values are reported by the fw as positive values - need to negate
 * to obtain their dBM.  Account for missing antennas by replacing 0
 * values by -256dBm: practically 0 power and a non-feasible 8 bit value.
 */
static int
iwm_mvm_get_signal_strength(struct iwm_softc *sc, struct iwm_rx_phy_info *phy_info)
{
	int energy_a, energy_b, energy_c, max_energy;
	uint32_t val;

	val = le32toh(phy_info->non_cfg_phy[IWM_RX_INFO_ENERGY_ANT_ABC_IDX]);
	energy_a = (val & IWM_RX_INFO_ENERGY_ANT_A_MSK) >>
	    IWM_RX_INFO_ENERGY_ANT_A_POS;
	energy_a = energy_a ? -energy_a : -256;
	energy_b = (val & IWM_RX_INFO_ENERGY_ANT_B_MSK) >>
	    IWM_RX_INFO_ENERGY_ANT_B_POS;
	energy_b = energy_b ? -energy_b : -256;
	energy_c = (val & IWM_RX_INFO_ENERGY_ANT_C_MSK) >>
	    IWM_RX_INFO_ENERGY_ANT_C_POS;
	energy_c = energy_c ? -energy_c : -256;
	max_energy = MAX(energy_a, energy_b);
	max_energy = MAX(max_energy, energy_c);

	IWM_DPRINTF(sc, IWM_DEBUG_RECV,
	    "energy In A %d B %d C %d , and max %d\n",
	    energy_a, energy_b, energy_c, max_energy);

	return max_energy;
}

static void
iwm_mvm_rx_rx_phy_cmd(struct iwm_softc *sc,
	struct iwm_rx_packet *pkt, struct iwm_rx_data *data)
{
	struct iwm_rx_phy_info *phy_info = (void *)pkt->data;

	IWM_DPRINTF(sc, IWM_DEBUG_RECV, "received PHY stats\n");
	bus_dmamap_sync(sc->rxq.data_dmat, data->map, BUS_DMASYNC_POSTREAD);

	memcpy(&sc->sc_last_phy_info, phy_info, sizeof(sc->sc_last_phy_info));
}

/*
 * Retrieve the average noise (in dBm) among receivers.
 */
static int
iwm_get_noise(const struct iwm_mvm_statistics_rx_non_phy *stats)
{
	int i, total, nbant, noise;

	total = nbant = noise = 0;
	for (i = 0; i < 3; i++) {
		noise = le32toh(stats->beacon_silence_rssi[i]) & 0xff;
		if (noise) {
			total += noise;
			nbant++;
		}
	}

	/* There should be at least one antenna but check anyway. */
	return (nbant == 0) ? -127 : (total / nbant) - 107;
}

/*
 * iwm_mvm_rx_rx_mpdu - IWM_REPLY_RX_MPDU_CMD handler
 *
 * Handles the actual data of the Rx packet from the fw
 */
static void
iwm_mvm_rx_rx_mpdu(struct iwm_softc *sc,
	struct iwm_rx_packet *pkt, struct iwm_rx_data *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	struct ieee80211_rx_stats rxs;
	struct mbuf *m;
	struct iwm_rx_phy_info *phy_info;
	struct iwm_rx_mpdu_res_start *rx_res;
	uint32_t len;
	uint32_t rx_pkt_status;
	int rssi;

	bus_dmamap_sync(sc->rxq.data_dmat, data->map, BUS_DMASYNC_POSTREAD);

	phy_info = &sc->sc_last_phy_info;
	rx_res = (struct iwm_rx_mpdu_res_start *)pkt->data;
	wh = (struct ieee80211_frame *)(pkt->data + sizeof(*rx_res));
	len = le16toh(rx_res->byte_count);
	rx_pkt_status = le32toh(*(uint32_t *)(pkt->data + sizeof(*rx_res) + len));

	m = data->m;
	m->m_data = pkt->data + sizeof(*rx_res);
	m->m_pkthdr.len = m->m_len = len;

	if (__predict_false(phy_info->cfg_phy_cnt > 20)) {
		device_printf(sc->sc_dev,
		    "dsp size out of range [0,20]: %d\n",
		    phy_info->cfg_phy_cnt);
		return;
	}

	if (!(rx_pkt_status & IWM_RX_MPDU_RES_STATUS_CRC_OK) ||
	    !(rx_pkt_status & IWM_RX_MPDU_RES_STATUS_OVERRUN_OK)) {
		IWM_DPRINTF(sc, IWM_DEBUG_RECV,
		    "Bad CRC or FIFO: 0x%08X.\n", rx_pkt_status);
		return; /* drop */
	}

	if (sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_RX_ENERGY_API) {
		rssi = iwm_mvm_get_signal_strength(sc, phy_info);
	} else {
		rssi = iwm_mvm_calc_rssi(sc, phy_info);
	}
	rssi = (0 - IWM_MIN_DBM) + rssi;	/* normalize */
	rssi = MIN(rssi, sc->sc_max_rssi);	/* clip to max. 100% */

	/* replenish ring for the buffer we're going to feed to the sharks */
	if (iwm_rx_addbuf(sc, IWM_RBUF_SIZE, sc->rxq.cur) != 0) {
		device_printf(sc->sc_dev, "%s: unable to add more buffers\n",
		    __func__);
		return;
	}

	ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);

	IWM_DPRINTF(sc, IWM_DEBUG_RECV,
	    "%s: phy_info: channel=%d, flags=0x%08x\n",
	    __func__,
	    le16toh(phy_info->channel),
	    le16toh(phy_info->phy_flags));

	/*
	 * Populate an RX state struct with the provided information.
	 */
	bzero(&rxs, sizeof(rxs));
	rxs.r_flags |= IEEE80211_R_IEEE | IEEE80211_R_FREQ;
	rxs.r_flags |= IEEE80211_R_NF | IEEE80211_R_RSSI;
	rxs.c_ieee = le16toh(phy_info->channel);
	if (le16toh(phy_info->phy_flags & IWM_RX_RES_PHY_FLAGS_BAND_24)) {
		rxs.c_freq = ieee80211_ieee2mhz(rxs.c_ieee, IEEE80211_CHAN_2GHZ);
	} else {
		rxs.c_freq = ieee80211_ieee2mhz(rxs.c_ieee, IEEE80211_CHAN_5GHZ);
	}
	rxs.rssi = rssi - sc->sc_noise;
	rxs.nf = sc->sc_noise;

	if (ieee80211_radiotap_active_vap(vap)) {
		struct iwm_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		if (phy_info->phy_flags & htole16(IWM_PHY_INFO_FLAG_SHPREAMBLE))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		tap->wr_chan_freq = htole16(rxs.c_freq);
		/* XXX only if ic->ic_curchan->ic_ieee == rxs.c_ieee */
		tap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		tap->wr_dbm_antsignal = (int8_t)rssi;
		tap->wr_dbm_antnoise = (int8_t)sc->sc_noise;
		tap->wr_tsft = phy_info->system_timestamp;
		switch (phy_info->rate) {
		/* CCK rates. */
		case  10: tap->wr_rate =   2; break;
		case  20: tap->wr_rate =   4; break;
		case  55: tap->wr_rate =  11; break;
		case 110: tap->wr_rate =  22; break;
		/* OFDM rates. */
		case 0xd: tap->wr_rate =  12; break;
		case 0xf: tap->wr_rate =  18; break;
		case 0x5: tap->wr_rate =  24; break;
		case 0x7: tap->wr_rate =  36; break;
		case 0x9: tap->wr_rate =  48; break;
		case 0xb: tap->wr_rate =  72; break;
		case 0x1: tap->wr_rate =  96; break;
		case 0x3: tap->wr_rate = 108; break;
		/* Unknown rate: should not happen. */
		default:  tap->wr_rate =   0;
		}
	}

	IWM_UNLOCK(sc);
	if (ni != NULL) {
		IWM_DPRINTF(sc, IWM_DEBUG_RECV, "input m %p\n", m);
		ieee80211_input_mimo(ni, m, &rxs);
		ieee80211_free_node(ni);
	} else {
		IWM_DPRINTF(sc, IWM_DEBUG_RECV, "inputall m %p\n", m);
		ieee80211_input_mimo_all(ic, m, &rxs);
	}
	IWM_LOCK(sc);
}

static int
iwm_mvm_rx_tx_cmd_single(struct iwm_softc *sc, struct iwm_rx_packet *pkt,
	struct iwm_node *in)
{
	struct iwm_mvm_tx_resp *tx_resp = (void *)pkt->data;
	struct ieee80211_node *ni = &in->in_ni;
	struct ieee80211vap *vap = ni->ni_vap;
	int status = le16toh(tx_resp->status.status) & IWM_TX_STATUS_MSK;
	int failack = tx_resp->failure_frame;

	KASSERT(tx_resp->frame_count == 1, ("too many frames"));

	/* Update rate control statistics. */
	IWM_DPRINTF(sc, IWM_DEBUG_XMIT, "%s: status=0x%04x, seq=%d, fc=%d, btc=%d, frts=%d, ff=%d, irate=%08x, wmt=%d\n",
	    __func__,
	    (int) le16toh(tx_resp->status.status),
	    (int) le16toh(tx_resp->status.sequence),
	    tx_resp->frame_count,
	    tx_resp->bt_kill_count,
	    tx_resp->failure_rts,
	    tx_resp->failure_frame,
	    le32toh(tx_resp->initial_rate),
	    (int) le16toh(tx_resp->wireless_media_time));

	if (status != IWM_TX_STATUS_SUCCESS &&
	    status != IWM_TX_STATUS_DIRECT_DONE) {
		ieee80211_ratectl_tx_complete(vap, ni,
		    IEEE80211_RATECTL_TX_FAILURE, &failack, NULL);
		return (1);
	} else {
		ieee80211_ratectl_tx_complete(vap, ni,
		    IEEE80211_RATECTL_TX_SUCCESS, &failack, NULL);
		return (0);
	}
}

static void
iwm_mvm_rx_tx_cmd(struct iwm_softc *sc,
	struct iwm_rx_packet *pkt, struct iwm_rx_data *data)
{
	struct iwm_cmd_header *cmd_hdr = &pkt->hdr;
	int idx = cmd_hdr->idx;
	int qid = cmd_hdr->qid;
	struct iwm_tx_ring *ring = &sc->txq[qid];
	struct iwm_tx_data *txd = &ring->data[idx];
	struct iwm_node *in = txd->in;
	struct mbuf *m = txd->m;
	int status;

	KASSERT(txd->done == 0, ("txd not done"));
	KASSERT(txd->in != NULL, ("txd without node"));
	KASSERT(txd->m != NULL, ("txd without mbuf"));

	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_POSTREAD);

	sc->sc_tx_timer = 0;

	status = iwm_mvm_rx_tx_cmd_single(sc, pkt, in);

	/* Unmap and free mbuf. */
	bus_dmamap_sync(ring->data_dmat, txd->map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(ring->data_dmat, txd->map);

	IWM_DPRINTF(sc, IWM_DEBUG_XMIT,
	    "free txd %p, in %p\n", txd, txd->in);
	txd->done = 1;
	txd->m = NULL;
	txd->in = NULL;

	ieee80211_tx_complete(&in->in_ni, m, status);

	if (--ring->queued < IWM_TX_RING_LOMARK) {
		sc->qfullmsk &= ~(1 << ring->qid);
		if (sc->qfullmsk == 0) {
			/*
			 * Well, we're in interrupt context, but then again
			 * I guess net80211 does all sorts of stunts in
			 * interrupt context, so maybe this is no biggie.
			 */
			iwm_start(sc);
		}
	}
}

/*
 * transmit side
 */

/*
 * Process a "command done" firmware notification.  This is where we wakeup
 * processes waiting for a synchronous command completion.
 * from if_iwn
 */
static void
iwm_cmd_done(struct iwm_softc *sc, struct iwm_rx_packet *pkt)
{
	struct iwm_tx_ring *ring = &sc->txq[IWM_MVM_CMD_QUEUE];
	struct iwm_tx_data *data;

	if (pkt->hdr.qid != IWM_MVM_CMD_QUEUE) {
		return;	/* Not a command ack. */
	}

	data = &ring->data[pkt->hdr.idx];

	/* If the command was mapped in an mbuf, free it. */
	if (data->m != NULL) {
		bus_dmamap_sync(ring->data_dmat, data->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->data_dmat, data->map);
		m_freem(data->m);
		data->m = NULL;
	}
	wakeup(&ring->desc[pkt->hdr.idx]);
}

#if 0
/*
 * necessary only for block ack mode
 */
void
iwm_update_sched(struct iwm_softc *sc, int qid, int idx, uint8_t sta_id,
	uint16_t len)
{
	struct iwm_agn_scd_bc_tbl *scd_bc_tbl;
	uint16_t w_val;

	scd_bc_tbl = sc->sched_dma.vaddr;

	len += 8; /* magic numbers came naturally from paris */
	if (sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_DW_BC_TABLE)
		len = roundup(len, 4) / 4;

	w_val = htole16(sta_id << 12 | len);

	/* Update TX scheduler. */
	scd_bc_tbl[qid].tfd_offset[idx] = w_val;
	bus_dmamap_sync(sc->sched_dma.tag, sc->sched_dma.map,
	    BUS_DMASYNC_PREWRITE);

	/* I really wonder what this is ?!? */
	if (idx < IWM_TFD_QUEUE_SIZE_BC_DUP) {
		scd_bc_tbl[qid].tfd_offset[IWM_TFD_QUEUE_SIZE_MAX + idx] = w_val;
		bus_dmamap_sync(sc->sched_dma.tag, sc->sched_dma.map,
		    BUS_DMASYNC_PREWRITE);
	}
}
#endif

/*
 * Take an 802.11 (non-n) rate, find the relevant rate
 * table entry.  return the index into in_ridx[].
 *
 * The caller then uses that index back into in_ridx
 * to figure out the rate index programmed /into/
 * the firmware for this given node.
 */
static int
iwm_tx_rateidx_lookup(struct iwm_softc *sc, struct iwm_node *in,
    uint8_t rate)
{
	int i;
	uint8_t r;

	for (i = 0; i < nitems(in->in_ridx); i++) {
		r = iwm_rates[in->in_ridx[i]].rate;
		if (rate == r)
			return (i);
	}
	/* XXX Return the first */
	/* XXX TODO: have it return the /lowest/ */
	return (0);
}

/*
 * Fill in the rate related information for a transmit command.
 */
static const struct iwm_rate *
iwm_tx_fill_cmd(struct iwm_softc *sc, struct iwm_node *in,
	struct ieee80211_frame *wh, struct iwm_tx_cmd *tx)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni = &in->in_ni;
	const struct iwm_rate *rinfo;
	int type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	int ridx, rate_flags;

	tx->rts_retry_limit = IWM_RTS_DFAULT_RETRY_LIMIT;
	tx->data_retry_limit = IWM_DEFAULT_TX_RETRY;

	/*
	 * XXX TODO: everything about the rate selection here is terrible!
	 */

	if (type == IEEE80211_FC0_TYPE_DATA) {
		int i;
		/* for data frames, use RS table */
		(void) ieee80211_ratectl_rate(ni, NULL, 0);
		i = iwm_tx_rateidx_lookup(sc, in, ni->ni_txrate);
		ridx = in->in_ridx[i];

		/* This is the index into the programmed table */
		tx->initial_rate_index = i;
		tx->tx_flags |= htole32(IWM_TX_CMD_FLG_STA_RATE);
		IWM_DPRINTF(sc, IWM_DEBUG_XMIT | IWM_DEBUG_TXRATE,
		    "%s: start with i=%d, txrate %d\n",
		    __func__, i, iwm_rates[ridx].rate);
	} else {
		/*
		 * For non-data, use the lowest supported rate for the given
		 * operational mode.
		 *
		 * Note: there may not be any rate control information available.
		 * This driver currently assumes if we're transmitting data
		 * frames, use the rate control table.  Grr.
		 *
		 * XXX TODO: use the configured rate for the traffic type!
		 * XXX TODO: this should be per-vap, not curmode; as we later
		 * on we'll want to handle off-channel stuff (eg TDLS).
		 */
		if (ic->ic_curmode == IEEE80211_MODE_11A) {
			/*
			 * XXX this assumes the mode is either 11a or not 11a;
			 * definitely won't work for 11n.
			 */
			ridx = IWM_RIDX_OFDM;
		} else {
			ridx = IWM_RIDX_CCK;
		}
	}

	rinfo = &iwm_rates[ridx];

	IWM_DPRINTF(sc, IWM_DEBUG_TXRATE, "%s: ridx=%d; rate=%d, CCK=%d\n",
	    __func__, ridx,
	    rinfo->rate,
	    !! (IWM_RIDX_IS_CCK(ridx))
	    );

	/* XXX TODO: hard-coded TX antenna? */
	rate_flags = 1 << IWM_RATE_MCS_ANT_POS;
	if (IWM_RIDX_IS_CCK(ridx))
		rate_flags |= IWM_RATE_MCS_CCK_MSK;
	tx->rate_n_flags = htole32(rate_flags | rinfo->plcp);

	return rinfo;
}

#define TB0_SIZE 16
static int
iwm_tx(struct iwm_softc *sc, struct mbuf *m, struct ieee80211_node *ni, int ac)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct iwm_node *in = IWM_NODE(ni);
	struct iwm_tx_ring *ring;
	struct iwm_tx_data *data;
	struct iwm_tfd *desc;
	struct iwm_device_cmd *cmd;
	struct iwm_tx_cmd *tx;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k = NULL;
	struct mbuf *m1;
	const struct iwm_rate *rinfo;
	uint32_t flags;
	u_int hdrlen;
	bus_dma_segment_t *seg, segs[IWM_MAX_SCATTER];
	int nsegs;
	uint8_t tid, type;
	int i, totlen, error, pad;

	wh = mtod(m, struct ieee80211_frame *);
	hdrlen = ieee80211_anyhdrsize(wh);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	tid = 0;
	ring = &sc->txq[ac];
	desc = &ring->desc[ring->cur];
	memset(desc, 0, sizeof(*desc));
	data = &ring->data[ring->cur];

	/* Fill out iwm_tx_cmd to send to the firmware */
	cmd = &ring->cmd[ring->cur];
	cmd->hdr.code = IWM_TX_CMD;
	cmd->hdr.flags = 0;
	cmd->hdr.qid = ring->qid;
	cmd->hdr.idx = ring->cur;

	tx = (void *)cmd->data;
	memset(tx, 0, sizeof(*tx));

	rinfo = iwm_tx_fill_cmd(sc, in, wh, tx);

	/* Encrypt the frame if need be. */
	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		/* Retrieve key for TX && do software encryption. */
		k = ieee80211_crypto_encap(ni, m);
		if (k == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		/* 802.11 header may have moved. */
		wh = mtod(m, struct ieee80211_frame *);
	}

	if (ieee80211_radiotap_active_vap(vap)) {
		struct iwm_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ni->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ni->ni_chan->ic_flags);
		tap->wt_rate = rinfo->rate;
		if (k != NULL)
			tap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		ieee80211_radiotap_tx(vap, m);
	}


	totlen = m->m_pkthdr.len;

	flags = 0;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= IWM_TX_CMD_FLG_ACK;
	}

	if (type != IEEE80211_FC0_TYPE_DATA
	    && (totlen + IEEE80211_CRC_LEN > vap->iv_rtsthreshold)
	    && !IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		flags |= IWM_TX_CMD_FLG_PROT_REQUIRE;
	}

	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    type != IEEE80211_FC0_TYPE_DATA)
		tx->sta_id = sc->sc_aux_sta.sta_id;
	else
		tx->sta_id = IWM_STATION_ID;

	if (type == IEEE80211_FC0_TYPE_MGT) {
		uint8_t subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

		if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ ||
		    subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)
			tx->pm_frame_timeout = htole16(3);
		else
			tx->pm_frame_timeout = htole16(2);
	} else {
		tx->pm_frame_timeout = htole16(0);
	}

	if (hdrlen & 3) {
		/* First segment length must be a multiple of 4. */
		flags |= IWM_TX_CMD_FLG_MH_PAD;
		pad = 4 - (hdrlen & 3);
	} else
		pad = 0;

	tx->driver_txop = 0;
	tx->next_frame_len = 0;

	tx->len = htole16(totlen);
	tx->tid_tspec = tid;
	tx->life_time = htole32(IWM_TX_CMD_LIFE_TIME_INFINITE);

	/* Set physical address of "scratch area". */
	tx->dram_lsb_ptr = htole32(data->scratch_paddr);
	tx->dram_msb_ptr = iwm_get_dma_hi_addr(data->scratch_paddr);

	/* Copy 802.11 header in TX command. */
	memcpy(((uint8_t *)tx) + sizeof(*tx), wh, hdrlen);

	flags |= IWM_TX_CMD_FLG_BT_DIS | IWM_TX_CMD_FLG_SEQ_CTL;

	tx->sec_ctl = 0;
	tx->tx_flags |= htole32(flags);

	/* Trim 802.11 header. */
	m_adj(m, hdrlen);
	error = bus_dmamap_load_mbuf_sg(ring->data_dmat, data->map, m,
	    segs, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		if (error != EFBIG) {
			device_printf(sc->sc_dev, "can't map mbuf (error %d)\n",
			    error);
			m_freem(m);
			return error;
		}
		/* Too many DMA segments, linearize mbuf. */
		m1 = m_collapse(m, M_NOWAIT, IWM_MAX_SCATTER - 2);
		if (m1 == NULL) {
			device_printf(sc->sc_dev,
			    "%s: could not defrag mbuf\n", __func__);
			m_freem(m);
			return (ENOBUFS);
		}
		m = m1;

		error = bus_dmamap_load_mbuf_sg(ring->data_dmat, data->map, m,
		    segs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			device_printf(sc->sc_dev, "can't map mbuf (error %d)\n",
			    error);
			m_freem(m);
			return error;
		}
	}
	data->m = m;
	data->in = in;
	data->done = 0;

	IWM_DPRINTF(sc, IWM_DEBUG_XMIT,
	    "sending txd %p, in %p\n", data, data->in);
	KASSERT(data->in != NULL, ("node is NULL"));

	IWM_DPRINTF(sc, IWM_DEBUG_XMIT,
	    "sending data: qid=%d idx=%d len=%d nsegs=%d txflags=0x%08x rate_n_flags=0x%08x rateidx=%d\n",
	    ring->qid, ring->cur, totlen, nsegs,
	    le32toh(tx->tx_flags),
	    le32toh(tx->rate_n_flags),
	    (int) tx->initial_rate_index
	    );

	/* Fill TX descriptor. */
	desc->num_tbs = 2 + nsegs;

	desc->tbs[0].lo = htole32(data->cmd_paddr);
	desc->tbs[0].hi_n_len = htole16(iwm_get_dma_hi_addr(data->cmd_paddr)) |
	    (TB0_SIZE << 4);
	desc->tbs[1].lo = htole32(data->cmd_paddr + TB0_SIZE);
	desc->tbs[1].hi_n_len = htole16(iwm_get_dma_hi_addr(data->cmd_paddr)) |
	    ((sizeof(struct iwm_cmd_header) + sizeof(*tx)
	      + hdrlen + pad - TB0_SIZE) << 4);

	/* Other DMA segments are for data payload. */
	for (i = 0; i < nsegs; i++) {
		seg = &segs[i];
		desc->tbs[i+2].lo = htole32(seg->ds_addr);
		desc->tbs[i+2].hi_n_len = \
		    htole16(iwm_get_dma_hi_addr(seg->ds_addr))
		    | ((seg->ds_len) << 4);
	}

	bus_dmamap_sync(ring->data_dmat, data->map,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->cmd_dma.tag, ring->cmd_dma.map,
	    BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(ring->desc_dma.tag, ring->desc_dma.map,
	    BUS_DMASYNC_PREWRITE);

#if 0
	iwm_update_sched(sc, ring->qid, ring->cur, tx->sta_id, le16toh(tx->len));
#endif

	/* Kick TX ring. */
	ring->cur = (ring->cur + 1) % IWM_TX_RING_COUNT;
	IWM_WRITE(sc, IWM_HBUS_TARG_WRPTR, ring->qid << 8 | ring->cur);

	/* Mark TX ring as full if we reach a certain threshold. */
	if (++ring->queued > IWM_TX_RING_HIMARK) {
		sc->qfullmsk |= 1 << ring->qid;
	}

	return 0;
}

static int
iwm_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct iwm_softc *sc = ic->ic_softc;
	int error = 0;

	IWM_DPRINTF(sc, IWM_DEBUG_XMIT,
	    "->%s begin\n", __func__);

	if ((sc->sc_flags & IWM_FLAG_HW_INITED) == 0) {
		m_freem(m);
		IWM_DPRINTF(sc, IWM_DEBUG_XMIT,
		    "<-%s not RUNNING\n", __func__);
		return (ENETDOWN);
        }

	IWM_LOCK(sc);
	/* XXX fix this */
        if (params == NULL) {
		error = iwm_tx(sc, m, ni, 0);
	} else {
		error = iwm_tx(sc, m, ni, 0);
	}
	sc->sc_tx_timer = 5;
	IWM_UNLOCK(sc);

        return (error);
}

/*
 * mvm/tx.c
 */

#if 0
/*
 * Note that there are transports that buffer frames before they reach
 * the firmware. This means that after flush_tx_path is called, the
 * queue might not be empty. The race-free way to handle this is to:
 * 1) set the station as draining
 * 2) flush the Tx path
 * 3) wait for the transport queues to be empty
 */
int
iwm_mvm_flush_tx_path(struct iwm_softc *sc, int tfd_msk, int sync)
{
	struct iwm_tx_path_flush_cmd flush_cmd = {
		.queues_ctl = htole32(tfd_msk),
		.flush_ctl = htole16(IWM_DUMP_TX_FIFO_FLUSH),
	};
	int ret;

	ret = iwm_mvm_send_cmd_pdu(sc, IWM_TXPATH_FLUSH,
	    sync ? IWM_CMD_SYNC : IWM_CMD_ASYNC,
	    sizeof(flush_cmd), &flush_cmd);
	if (ret)
                device_printf(sc->sc_dev,
		    "Flushing tx queue failed: %d\n", ret);
	return ret;
}
#endif

/*
 * BEGIN mvm/sta.c
 */

static void
iwm_mvm_add_sta_cmd_v6_to_v5(struct iwm_mvm_add_sta_cmd_v6 *cmd_v6,
	struct iwm_mvm_add_sta_cmd_v5 *cmd_v5)
{
	memset(cmd_v5, 0, sizeof(*cmd_v5));

	cmd_v5->add_modify = cmd_v6->add_modify;
	cmd_v5->tid_disable_tx = cmd_v6->tid_disable_tx;
	cmd_v5->mac_id_n_color = cmd_v6->mac_id_n_color;
	IEEE80211_ADDR_COPY(cmd_v5->addr, cmd_v6->addr);
	cmd_v5->sta_id = cmd_v6->sta_id;
	cmd_v5->modify_mask = cmd_v6->modify_mask;
	cmd_v5->station_flags = cmd_v6->station_flags;
	cmd_v5->station_flags_msk = cmd_v6->station_flags_msk;
	cmd_v5->add_immediate_ba_tid = cmd_v6->add_immediate_ba_tid;
	cmd_v5->remove_immediate_ba_tid = cmd_v6->remove_immediate_ba_tid;
	cmd_v5->add_immediate_ba_ssn = cmd_v6->add_immediate_ba_ssn;
	cmd_v5->sleep_tx_count = cmd_v6->sleep_tx_count;
	cmd_v5->sleep_state_flags = cmd_v6->sleep_state_flags;
	cmd_v5->assoc_id = cmd_v6->assoc_id;
	cmd_v5->beamform_flags = cmd_v6->beamform_flags;
	cmd_v5->tfd_queue_msk = cmd_v6->tfd_queue_msk;
}

static int
iwm_mvm_send_add_sta_cmd_status(struct iwm_softc *sc,
	struct iwm_mvm_add_sta_cmd_v6 *cmd, int *status)
{
	struct iwm_mvm_add_sta_cmd_v5 cmd_v5;

	if (sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_STA_KEY_CMD) {
		return iwm_mvm_send_cmd_pdu_status(sc, IWM_ADD_STA,
		    sizeof(*cmd), cmd, status);
	}

	iwm_mvm_add_sta_cmd_v6_to_v5(cmd, &cmd_v5);

	return iwm_mvm_send_cmd_pdu_status(sc, IWM_ADD_STA, sizeof(cmd_v5),
	    &cmd_v5, status);
}

/* send station add/update command to firmware */
static int
iwm_mvm_sta_send_to_fw(struct iwm_softc *sc, struct iwm_node *in, int update)
{
	struct iwm_mvm_add_sta_cmd_v6 add_sta_cmd;
	int ret;
	uint32_t status;

	memset(&add_sta_cmd, 0, sizeof(add_sta_cmd));

	add_sta_cmd.sta_id = IWM_STATION_ID;
	add_sta_cmd.mac_id_n_color
	    = htole32(IWM_FW_CMD_ID_AND_COLOR(IWM_DEFAULT_MACID,
	        IWM_DEFAULT_COLOR));
	if (!update) {
		add_sta_cmd.tfd_queue_msk = htole32(0xf);
		IEEE80211_ADDR_COPY(&add_sta_cmd.addr, in->in_ni.ni_bssid);
	}
	add_sta_cmd.add_modify = update ? 1 : 0;
	add_sta_cmd.station_flags_msk
	    |= htole32(IWM_STA_FLG_FAT_EN_MSK | IWM_STA_FLG_MIMO_EN_MSK);

	status = IWM_ADD_STA_SUCCESS;
	ret = iwm_mvm_send_add_sta_cmd_status(sc, &add_sta_cmd, &status);
	if (ret)
		return ret;

	switch (status) {
	case IWM_ADD_STA_SUCCESS:
		break;
	default:
		ret = EIO;
		device_printf(sc->sc_dev, "IWM_ADD_STA failed\n");
		break;
	}

	return ret;
}

static int
iwm_mvm_add_sta(struct iwm_softc *sc, struct iwm_node *in)
{
	int ret;

	ret = iwm_mvm_sta_send_to_fw(sc, in, 0);
	if (ret)
		return ret;

	return 0;
}

static int
iwm_mvm_update_sta(struct iwm_softc *sc, struct iwm_node *in)
{
	return iwm_mvm_sta_send_to_fw(sc, in, 1);
}

static int
iwm_mvm_add_int_sta_common(struct iwm_softc *sc, struct iwm_int_sta *sta,
	const uint8_t *addr, uint16_t mac_id, uint16_t color)
{
	struct iwm_mvm_add_sta_cmd_v6 cmd;
	int ret;
	uint32_t status;

	memset(&cmd, 0, sizeof(cmd));
	cmd.sta_id = sta->sta_id;
	cmd.mac_id_n_color = htole32(IWM_FW_CMD_ID_AND_COLOR(mac_id, color));

	cmd.tfd_queue_msk = htole32(sta->tfd_queue_msk);

	if (addr)
		IEEE80211_ADDR_COPY(cmd.addr, addr);

	ret = iwm_mvm_send_add_sta_cmd_status(sc, &cmd, &status);
	if (ret)
		return ret;

	switch (status) {
	case IWM_ADD_STA_SUCCESS:
		IWM_DPRINTF(sc, IWM_DEBUG_RESET,
		    "%s: Internal station added.\n", __func__);
		return 0;
	default:
		device_printf(sc->sc_dev,
		    "%s: Add internal station failed, status=0x%x\n",
		    __func__, status);
		ret = EIO;
		break;
	}
	return ret;
}

static int
iwm_mvm_add_aux_sta(struct iwm_softc *sc)
{
	int ret;

	sc->sc_aux_sta.sta_id = 3;
	sc->sc_aux_sta.tfd_queue_msk = 0;

	ret = iwm_mvm_add_int_sta_common(sc,
	    &sc->sc_aux_sta, NULL, IWM_MAC_INDEX_AUX, 0);

	if (ret)
		memset(&sc->sc_aux_sta, 0, sizeof(sc->sc_aux_sta));
	return ret;
}

/*
 * END mvm/sta.c
 */

/*
 * BEGIN mvm/quota.c
 */

static int
iwm_mvm_update_quotas(struct iwm_softc *sc, struct iwm_node *in)
{
	struct iwm_time_quota_cmd cmd;
	int i, idx, ret, num_active_macs, quota, quota_rem;
	int colors[IWM_MAX_BINDINGS] = { -1, -1, -1, -1, };
	int n_ifs[IWM_MAX_BINDINGS] = {0, };
	uint16_t id;

	memset(&cmd, 0, sizeof(cmd));

	/* currently, PHY ID == binding ID */
	if (in) {
		id = in->in_phyctxt->id;
		KASSERT(id < IWM_MAX_BINDINGS, ("invalid id"));
		colors[id] = in->in_phyctxt->color;

		if (1)
			n_ifs[id] = 1;
	}

	/*
	 * The FW's scheduling session consists of
	 * IWM_MVM_MAX_QUOTA fragments. Divide these fragments
	 * equally between all the bindings that require quota
	 */
	num_active_macs = 0;
	for (i = 0; i < IWM_MAX_BINDINGS; i++) {
		cmd.quotas[i].id_and_color = htole32(IWM_FW_CTXT_INVALID);
		num_active_macs += n_ifs[i];
	}

	quota = 0;
	quota_rem = 0;
	if (num_active_macs) {
		quota = IWM_MVM_MAX_QUOTA / num_active_macs;
		quota_rem = IWM_MVM_MAX_QUOTA % num_active_macs;
	}

	for (idx = 0, i = 0; i < IWM_MAX_BINDINGS; i++) {
		if (colors[i] < 0)
			continue;

		cmd.quotas[idx].id_and_color =
			htole32(IWM_FW_CMD_ID_AND_COLOR(i, colors[i]));

		if (n_ifs[i] <= 0) {
			cmd.quotas[idx].quota = htole32(0);
			cmd.quotas[idx].max_duration = htole32(0);
		} else {
			cmd.quotas[idx].quota = htole32(quota * n_ifs[i]);
			cmd.quotas[idx].max_duration = htole32(0);
		}
		idx++;
	}

	/* Give the remainder of the session to the first binding */
	cmd.quotas[0].quota = htole32(le32toh(cmd.quotas[0].quota) + quota_rem);

	ret = iwm_mvm_send_cmd_pdu(sc, IWM_TIME_QUOTA_CMD, IWM_CMD_SYNC,
	    sizeof(cmd), &cmd);
	if (ret)
		device_printf(sc->sc_dev,
		    "%s: Failed to send quota: %d\n", __func__, ret);
	return ret;
}

/*
 * END mvm/quota.c
 */

/*
 * ieee80211 routines
 */

/*
 * Change to AUTH state in 80211 state machine.  Roughly matches what
 * Linux does in bss_info_changed().
 */
static int
iwm_auth(struct ieee80211vap *vap, struct iwm_softc *sc)
{
	struct ieee80211_node *ni;
	struct iwm_node *in;
	struct iwm_vap *iv = IWM_VAP(vap);
	uint32_t duration;
	int error;

	/*
	 * XXX i have a feeling that the vap node is being
	 * freed from underneath us. Grr.
	 */
	ni = ieee80211_ref_node(vap->iv_bss);
	in = IWM_NODE(ni);
	IWM_DPRINTF(sc, IWM_DEBUG_RESET | IWM_DEBUG_STATE,
	    "%s: called; vap=%p, bss ni=%p\n",
	    __func__,
	    vap,
	    ni);

	in->in_assoc = 0;

	error = iwm_allow_mcast(vap, sc);
	if (error) {
		device_printf(sc->sc_dev,
		    "%s: failed to set multicast\n", __func__);
		goto out;
	}

	/*
	 * This is where it deviates from what Linux does.
	 *
	 * Linux iwlwifi doesn't reset the nic each time, nor does it
	 * call ctxt_add() here.  Instead, it adds it during vap creation,
	 * and always does does a mac_ctx_changed().
	 *
	 * The openbsd port doesn't attempt to do that - it reset things
	 * at odd states and does the add here.
	 *
	 * So, until the state handling is fixed (ie, we never reset
	 * the NIC except for a firmware failure, which should drag
	 * the NIC back to IDLE, re-setup and re-add all the mac/phy
	 * contexts that are required), let's do a dirty hack here.
	 */
	if (iv->is_uploaded) {
		if ((error = iwm_mvm_mac_ctxt_changed(sc, vap)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to update MAC\n", __func__);
			goto out;
		}
		if ((error = iwm_mvm_phy_ctxt_changed(sc, &sc->sc_phyctxt[0],
		    in->in_ni.ni_chan, 1, 1)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed update phy ctxt\n", __func__);
			goto out;
		}
		in->in_phyctxt = &sc->sc_phyctxt[0];

		if ((error = iwm_mvm_binding_update(sc, in)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: binding update cmd\n", __func__);
			goto out;
		}
		if ((error = iwm_mvm_update_sta(sc, in)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to update sta\n", __func__);
			goto out;
		}
	} else {
		if ((error = iwm_mvm_mac_ctxt_add(sc, vap)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to add MAC\n", __func__);
			goto out;
		}
		if ((error = iwm_mvm_phy_ctxt_changed(sc, &sc->sc_phyctxt[0],
		    in->in_ni.ni_chan, 1, 1)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed add phy ctxt!\n", __func__);
			error = ETIMEDOUT;
			goto out;
		}
		in->in_phyctxt = &sc->sc_phyctxt[0];

		if ((error = iwm_mvm_binding_add_vif(sc, in)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: binding add cmd\n", __func__);
			goto out;
		}
		if ((error = iwm_mvm_add_sta(sc, in)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to add sta\n", __func__);
			goto out;
		}
	}

	/*
	 * Prevent the FW from wandering off channel during association
	 * by "protecting" the session with a time event.
	 */
	/* XXX duration is in units of TU, not MS */
	duration = IWM_MVM_TE_SESSION_PROTECTION_MAX_TIME_MS;
	iwm_mvm_protect_session(sc, in, duration, 500 /* XXX magic number */);
	DELAY(100);

	error = 0;
out:
	ieee80211_free_node(ni);
	return (error);
}

static int
iwm_assoc(struct ieee80211vap *vap, struct iwm_softc *sc)
{
	struct iwm_node *in = IWM_NODE(vap->iv_bss);
	int error;

	if ((error = iwm_mvm_update_sta(sc, in)) != 0) {
		device_printf(sc->sc_dev,
		    "%s: failed to update STA\n", __func__);
		return error;
	}

	in->in_assoc = 1;
	if ((error = iwm_mvm_mac_ctxt_changed(sc, vap)) != 0) {
		device_printf(sc->sc_dev,
		    "%s: failed to update MAC\n", __func__);
		return error;
	}

	return 0;
}

static int
iwm_release(struct iwm_softc *sc, struct iwm_node *in)
{
	/*
	 * Ok, so *technically* the proper set of calls for going
	 * from RUN back to SCAN is:
	 *
	 * iwm_mvm_power_mac_disable(sc, in);
	 * iwm_mvm_mac_ctxt_changed(sc, in);
	 * iwm_mvm_rm_sta(sc, in);
	 * iwm_mvm_update_quotas(sc, NULL);
	 * iwm_mvm_mac_ctxt_changed(sc, in);
	 * iwm_mvm_binding_remove_vif(sc, in);
	 * iwm_mvm_mac_ctxt_remove(sc, in);
	 *
	 * However, that freezes the device not matter which permutations
	 * and modifications are attempted.  Obviously, this driver is missing
	 * something since it works in the Linux driver, but figuring out what
	 * is missing is a little more complicated.  Now, since we're going
	 * back to nothing anyway, we'll just do a complete device reset.
	 * Up your's, device!
	 */
	//iwm_mvm_flush_tx_path(sc, 0xf, 1);
	iwm_stop_device(sc);
	iwm_init_hw(sc);
	if (in)
		in->in_assoc = 0;
	return 0;

#if 0
	int error;

	iwm_mvm_power_mac_disable(sc, in);

	if ((error = iwm_mvm_mac_ctxt_changed(sc, in)) != 0) {
		device_printf(sc->sc_dev, "mac ctxt change fail 1 %d\n", error);
		return error;
	}

	if ((error = iwm_mvm_rm_sta(sc, in)) != 0) {
		device_printf(sc->sc_dev, "sta remove fail %d\n", error);
		return error;
	}
	error = iwm_mvm_rm_sta(sc, in);
	in->in_assoc = 0;
	iwm_mvm_update_quotas(sc, NULL);
	if ((error = iwm_mvm_mac_ctxt_changed(sc, in)) != 0) {
		device_printf(sc->sc_dev, "mac ctxt change fail 2 %d\n", error);
		return error;
	}
	iwm_mvm_binding_remove_vif(sc, in);

	iwm_mvm_mac_ctxt_remove(sc, in);

	return error;
#endif
}

static struct ieee80211_node *
iwm_node_alloc(struct ieee80211vap *vap, const uint8_t mac[IEEE80211_ADDR_LEN])
{
	return malloc(sizeof (struct iwm_node), M_80211_NODE,
	    M_NOWAIT | M_ZERO);
}

static void
iwm_setrates(struct iwm_softc *sc, struct iwm_node *in)
{
	struct ieee80211_node *ni = &in->in_ni;
	struct iwm_lq_cmd *lq = &in->in_lq;
	int nrates = ni->ni_rates.rs_nrates;
	int i, ridx, tab = 0;
	int txant = 0;

	if (nrates > nitems(lq->rs_table)) {
		device_printf(sc->sc_dev,
		    "%s: node supports %d rates, driver handles "
		    "only %zu\n", __func__, nrates, nitems(lq->rs_table));
		return;
	}
	if (nrates == 0) {
		device_printf(sc->sc_dev,
		    "%s: node supports 0 rates, odd!\n", __func__);
		return;
	}

	/*
	 * XXX .. and most of iwm_node is not initialised explicitly;
	 * it's all just 0x0 passed to the firmware.
	 */

	/* first figure out which rates we should support */
	/* XXX TODO: this isn't 11n aware /at all/ */
	memset(&in->in_ridx, -1, sizeof(in->in_ridx));
	IWM_DPRINTF(sc, IWM_DEBUG_TXRATE,
	    "%s: nrates=%d\n", __func__, nrates);

	/*
	 * Loop over nrates and populate in_ridx from the highest
	 * rate to the lowest rate.  Remember, in_ridx[] has
	 * IEEE80211_RATE_MAXSIZE entries!
	 */
	for (i = 0; i < min(nrates, IEEE80211_RATE_MAXSIZE); i++) {
		int rate = ni->ni_rates.rs_rates[(nrates - 1) - i] & IEEE80211_RATE_VAL;

		/* Map 802.11 rate to HW rate index. */
		for (ridx = 0; ridx <= IWM_RIDX_MAX; ridx++)
			if (iwm_rates[ridx].rate == rate)
				break;
		if (ridx > IWM_RIDX_MAX) {
			device_printf(sc->sc_dev,
			    "%s: WARNING: device rate for %d not found!\n",
			    __func__, rate);
		} else {
			IWM_DPRINTF(sc, IWM_DEBUG_TXRATE,
			    "%s: rate: i: %d, rate=%d, ridx=%d\n",
			    __func__,
			    i,
			    rate,
			    ridx);
			in->in_ridx[i] = ridx;
		}
	}

	/* then construct a lq_cmd based on those */
	memset(lq, 0, sizeof(*lq));
	lq->sta_id = IWM_STATION_ID;

	/*
	 * are these used? (we don't do SISO or MIMO)
	 * need to set them to non-zero, though, or we get an error.
	 */
	lq->single_stream_ant_msk = 1;
	lq->dual_stream_ant_msk = 1;

	/*
	 * Build the actual rate selection table.
	 * The lowest bits are the rates.  Additionally,
	 * CCK needs bit 9 to be set.  The rest of the bits
	 * we add to the table select the tx antenna
	 * Note that we add the rates in the highest rate first
	 * (opposite of ni_rates).
	 */
	/*
	 * XXX TODO: this should be looping over the min of nrates
	 * and LQ_MAX_RETRY_NUM.  Sigh.
	 */
	for (i = 0; i < nrates; i++) {
		int nextant;

		if (txant == 0)
			txant = IWM_FW_VALID_TX_ANT(sc);
		nextant = 1<<(ffs(txant)-1);
		txant &= ~nextant;

		/*
		 * Map the rate id into a rate index into
		 * our hardware table containing the
		 * configuration to use for this rate.
		 */
		ridx = in->in_ridx[i];
		tab = iwm_rates[ridx].plcp;
		tab |= nextant << IWM_RATE_MCS_ANT_POS;
		if (IWM_RIDX_IS_CCK(ridx))
			tab |= IWM_RATE_MCS_CCK_MSK;
		IWM_DPRINTF(sc, IWM_DEBUG_TXRATE,
		    "station rate i=%d, rate=%d, hw=%x\n",
		    i, iwm_rates[ridx].rate, tab);
		lq->rs_table[i] = htole32(tab);
	}
	/* then fill the rest with the lowest possible rate */
	for (i = nrates; i < nitems(lq->rs_table); i++) {
		KASSERT(tab != 0, ("invalid tab"));
		lq->rs_table[i] = htole32(tab);
	}
}

static int
iwm_media_change(struct ifnet *ifp)
{
	struct ieee80211vap *vap = ifp->if_softc;
	struct ieee80211com *ic = vap->iv_ic;
	struct iwm_softc *sc = ic->ic_softc;
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	IWM_LOCK(sc);
	if (ic->ic_nrunning > 0) {
		iwm_stop(sc);
		iwm_init(sc);
	}
	IWM_UNLOCK(sc);
	return error;
}


static int
iwm_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct iwm_vap *ivp = IWM_VAP(vap);
	struct ieee80211com *ic = vap->iv_ic;
	struct iwm_softc *sc = ic->ic_softc;
	struct iwm_node *in;
	int error;

	IWM_DPRINTF(sc, IWM_DEBUG_STATE,
	    "switching state %s -> %s\n",
	    ieee80211_state_name[vap->iv_state],
	    ieee80211_state_name[nstate]);
	IEEE80211_UNLOCK(ic);
	IWM_LOCK(sc);

	if (vap->iv_state == IEEE80211_S_SCAN && nstate != vap->iv_state)
		iwm_led_blink_stop(sc);

	/* disable beacon filtering if we're hopping out of RUN */
	if (vap->iv_state == IEEE80211_S_RUN && nstate != vap->iv_state) {
		iwm_mvm_disable_beacon_filter(sc);

		if (((in = IWM_NODE(vap->iv_bss)) != NULL))
			in->in_assoc = 0;

		iwm_release(sc, NULL);

		/*
		 * It's impossible to directly go RUN->SCAN. If we iwm_release()
		 * above then the card will be completely reinitialized,
		 * so the driver must do everything necessary to bring the card
		 * from INIT to SCAN.
		 *
		 * Additionally, upon receiving deauth frame from AP,
		 * OpenBSD 802.11 stack puts the driver in IEEE80211_S_AUTH
		 * state. This will also fail with this driver, so bring the FSM
		 * from IEEE80211_S_RUN to IEEE80211_S_SCAN in this case as well.
		 *
		 * XXX TODO: fix this for FreeBSD!
		 */
		if (nstate == IEEE80211_S_SCAN ||
		    nstate == IEEE80211_S_AUTH ||
		    nstate == IEEE80211_S_ASSOC) {
			IWM_DPRINTF(sc, IWM_DEBUG_STATE,
			    "Force transition to INIT; MGT=%d\n", arg);
			IWM_UNLOCK(sc);
			IEEE80211_LOCK(ic);
			vap->iv_newstate(vap, IEEE80211_S_INIT, arg);
			IWM_DPRINTF(sc, IWM_DEBUG_STATE,
			    "Going INIT->SCAN\n");
			nstate = IEEE80211_S_SCAN;
			IEEE80211_UNLOCK(ic);
			IWM_LOCK(sc);
		}
	}

	switch (nstate) {
	case IEEE80211_S_INIT:
		sc->sc_scanband = 0;
		break;

	case IEEE80211_S_AUTH:
		if ((error = iwm_auth(vap, sc)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: could not move to auth state: %d\n",
			    __func__, error);
			break;
		}
		break;

	case IEEE80211_S_ASSOC:
		if ((error = iwm_assoc(vap, sc)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to associate: %d\n", __func__,
			    error);
			break;
		}
		break;

	case IEEE80211_S_RUN:
	{
		struct iwm_host_cmd cmd = {
			.id = IWM_LQ_CMD,
			.len = { sizeof(in->in_lq), },
			.flags = IWM_CMD_SYNC,
		};

		/* Update the association state, now we have it all */
		/* (eg associd comes in at this point */
		error = iwm_assoc(vap, sc);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to update association state: %d\n",
			    __func__,
			    error);
			break;
		}

		in = IWM_NODE(vap->iv_bss);
		iwm_mvm_power_mac_update_mode(sc, in);
		iwm_mvm_enable_beacon_filter(sc, in);
		iwm_mvm_update_quotas(sc, in);
		iwm_setrates(sc, in);

		cmd.data[0] = &in->in_lq;
		if ((error = iwm_send_cmd(sc, &cmd)) != 0) {
			device_printf(sc->sc_dev,
			    "%s: IWM_LQ_CMD failed\n", __func__);
		}

		break;
	}

	default:
		break;
	}
	IWM_UNLOCK(sc);
	IEEE80211_LOCK(ic);

	return (ivp->iv_newstate(vap, nstate, arg));
}

void
iwm_endscan_cb(void *arg, int pending)
{
	struct iwm_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int done;
	int error;

	IWM_DPRINTF(sc, IWM_DEBUG_SCAN | IWM_DEBUG_TRACE,
	    "%s: scan ended\n",
	    __func__);

	IWM_LOCK(sc);
	if (sc->sc_scanband == IEEE80211_CHAN_2GHZ &&
	    sc->sc_nvm.sku_cap_band_52GHz_enable) {
		done = 0;
		if ((error = iwm_mvm_scan_request(sc,
		    IEEE80211_CHAN_5GHZ, 0, NULL, 0)) != 0) {
			device_printf(sc->sc_dev,
			    "could not initiate 5 GHz scan\n");
			done = 1;
		}
	} else {
		done = 1;
	}

	if (done) {
		IWM_UNLOCK(sc);
		ieee80211_scan_done(TAILQ_FIRST(&ic->ic_vaps));
		IWM_LOCK(sc);
		sc->sc_scanband = 0;
	}
	IWM_UNLOCK(sc);
}

static int
iwm_init_hw(struct iwm_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int error, i, qid;

	if ((error = iwm_start_hw(sc)) != 0)
		return error;

	if ((error = iwm_run_init_mvm_ucode(sc, 0)) != 0) {
		return error;
	}

	/*
	 * should stop and start HW since that INIT
	 * image just loaded
	 */
	iwm_stop_device(sc);
	if ((error = iwm_start_hw(sc)) != 0) {
		device_printf(sc->sc_dev, "could not initialize hardware\n");
		return error;
	}

	/* omstart, this time with the regular firmware */
	error = iwm_mvm_load_ucode_wait_alive(sc, IWM_UCODE_TYPE_REGULAR);
	if (error) {
		device_printf(sc->sc_dev, "could not load firmware\n");
		goto error;
	}

	if ((error = iwm_send_tx_ant_cfg(sc, IWM_FW_VALID_TX_ANT(sc))) != 0)
		goto error;

	/* Send phy db control command and then phy db calibration*/
	if ((error = iwm_send_phy_db_data(sc)) != 0)
		goto error;

	if ((error = iwm_send_phy_cfg_cmd(sc)) != 0)
		goto error;

	/* Add auxiliary station for scanning */
	if ((error = iwm_mvm_add_aux_sta(sc)) != 0)
		goto error;

	for (i = 0; i < IWM_NUM_PHY_CTX; i++) {
		/*
		 * The channel used here isn't relevant as it's
		 * going to be overwritten in the other flows.
		 * For now use the first channel we have.
		 */
		if ((error = iwm_mvm_phy_ctxt_add(sc,
		    &sc->sc_phyctxt[i], &ic->ic_channels[1], 1, 1)) != 0)
			goto error;
	}

	error = iwm_mvm_power_update_device(sc);
	if (error)
		goto error;

	/* Mark TX rings as active. */
	for (qid = 0; qid < 4; qid++) {
		iwm_enable_txq(sc, qid, qid);
	}

	return 0;

 error:
	iwm_stop_device(sc);
	return error;
}

/* Allow multicast from our BSSID. */
static int
iwm_allow_mcast(struct ieee80211vap *vap, struct iwm_softc *sc)
{
	struct ieee80211_node *ni = vap->iv_bss;
	struct iwm_mcast_filter_cmd *cmd;
	size_t size;
	int error;

	size = roundup(sizeof(*cmd), 4);
	cmd = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cmd == NULL)
		return ENOMEM;
	cmd->filter_own = 1;
	cmd->port_id = 0;
	cmd->count = 0;
	cmd->pass_all = 1;
	IEEE80211_ADDR_COPY(cmd->bssid, ni->ni_bssid);

	error = iwm_mvm_send_cmd_pdu(sc, IWM_MCAST_FILTER_CMD,
	    IWM_CMD_SYNC, size, cmd);
	free(cmd, M_DEVBUF);

	return (error);
}

static void
iwm_init(struct iwm_softc *sc)
{
	int error;

	if (sc->sc_flags & IWM_FLAG_HW_INITED) {
		return;
	}
	sc->sc_generation++;
	sc->sc_flags &= ~IWM_FLAG_STOPPED;

	if ((error = iwm_init_hw(sc)) != 0) {
		iwm_stop(sc);
		return;
	}

	/*
 	 * Ok, firmware loaded and we are jogging
	 */
	sc->sc_flags |= IWM_FLAG_HW_INITED;
	callout_reset(&sc->sc_watchdog_to, hz, iwm_watchdog, sc);
}

static int
iwm_transmit(struct ieee80211com *ic, struct mbuf *m)
{
	struct iwm_softc *sc;
	int error;

	sc = ic->ic_softc;

	IWM_LOCK(sc);
	if ((sc->sc_flags & IWM_FLAG_HW_INITED) == 0) {
		IWM_UNLOCK(sc);
		return (ENXIO);
	}
	error = mbufq_enqueue(&sc->sc_snd, m);
	if (error) {
		IWM_UNLOCK(sc);
		return (error);
	}
	iwm_start(sc);
	IWM_UNLOCK(sc);
	return (0);
}

/*
 * Dequeue packets from sendq and call send.
 */
static void
iwm_start(struct iwm_softc *sc)
{
	struct ieee80211_node *ni;
	struct mbuf *m;
	int ac = 0;

	IWM_DPRINTF(sc, IWM_DEBUG_XMIT | IWM_DEBUG_TRACE, "->%s\n", __func__);
	while (sc->qfullmsk == 0 &&
		(m = mbufq_dequeue(&sc->sc_snd)) != NULL) {
		ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		if (iwm_tx(sc, m, ni, ac) != 0) {
			if_inc_counter(ni->ni_vap->iv_ifp,
			    IFCOUNTER_OERRORS, 1);
			ieee80211_free_node(ni);
			continue;
		}
		sc->sc_tx_timer = 15;
	}
	IWM_DPRINTF(sc, IWM_DEBUG_XMIT | IWM_DEBUG_TRACE, "<-%s\n", __func__);
}

static void
iwm_stop(struct iwm_softc *sc)
{

	sc->sc_flags &= ~IWM_FLAG_HW_INITED;
	sc->sc_flags |= IWM_FLAG_STOPPED;
	sc->sc_generation++;
	sc->sc_scanband = 0;
	iwm_led_blink_stop(sc);
	sc->sc_tx_timer = 0;
	iwm_stop_device(sc);
}

static void
iwm_watchdog(void *arg)
{
	struct iwm_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			device_printf(sc->sc_dev, "device timeout\n");
#ifdef IWM_DEBUG
			iwm_nic_error(sc);
#endif
			ieee80211_restart_all(ic);
			counter_u64_add(ic->ic_oerrors, 1);
			return;
		}
	}
	callout_reset(&sc->sc_watchdog_to, hz, iwm_watchdog, sc);
}

static void
iwm_parent(struct ieee80211com *ic)
{
	struct iwm_softc *sc = ic->ic_softc;
	int startall = 0;

	IWM_LOCK(sc);
	if (ic->ic_nrunning > 0) {
		if (!(sc->sc_flags & IWM_FLAG_HW_INITED)) {
			iwm_init(sc);
			startall = 1;
		}
	} else if (sc->sc_flags & IWM_FLAG_HW_INITED)
		iwm_stop(sc);
	IWM_UNLOCK(sc);
	if (startall)
		ieee80211_start_all(ic);
}

/*
 * The interrupt side of things
 */

/*
 * error dumping routines are from iwlwifi/mvm/utils.c
 */

/*
 * Note: This structure is read from the device with IO accesses,
 * and the reading already does the endian conversion. As it is
 * read with uint32_t-sized accesses, any members with a different size
 * need to be ordered correctly though!
 */
struct iwm_error_event_table {
	uint32_t valid;		/* (nonzero) valid, (0) log is empty */
	uint32_t error_id;		/* type of error */
	uint32_t pc;			/* program counter */
	uint32_t blink1;		/* branch link */
	uint32_t blink2;		/* branch link */
	uint32_t ilink1;		/* interrupt link */
	uint32_t ilink2;		/* interrupt link */
	uint32_t data1;		/* error-specific data */
	uint32_t data2;		/* error-specific data */
	uint32_t data3;		/* error-specific data */
	uint32_t bcon_time;		/* beacon timer */
	uint32_t tsf_low;		/* network timestamp function timer */
	uint32_t tsf_hi;		/* network timestamp function timer */
	uint32_t gp1;		/* GP1 timer register */
	uint32_t gp2;		/* GP2 timer register */
	uint32_t gp3;		/* GP3 timer register */
	uint32_t ucode_ver;		/* uCode version */
	uint32_t hw_ver;		/* HW Silicon version */
	uint32_t brd_ver;		/* HW board version */
	uint32_t log_pc;		/* log program counter */
	uint32_t frame_ptr;		/* frame pointer */
	uint32_t stack_ptr;		/* stack pointer */
	uint32_t hcmd;		/* last host command header */
	uint32_t isr0;		/* isr status register LMPM_NIC_ISR0:
				 * rxtx_flag */
	uint32_t isr1;		/* isr status register LMPM_NIC_ISR1:
				 * host_flag */
	uint32_t isr2;		/* isr status register LMPM_NIC_ISR2:
				 * enc_flag */
	uint32_t isr3;		/* isr status register LMPM_NIC_ISR3:
				 * time_flag */
	uint32_t isr4;		/* isr status register LMPM_NIC_ISR4:
				 * wico interrupt */
	uint32_t isr_pref;		/* isr status register LMPM_NIC_PREF_STAT */
	uint32_t wait_event;		/* wait event() caller address */
	uint32_t l2p_control;	/* L2pControlField */
	uint32_t l2p_duration;	/* L2pDurationField */
	uint32_t l2p_mhvalid;	/* L2pMhValidBits */
	uint32_t l2p_addr_match;	/* L2pAddrMatchStat */
	uint32_t lmpm_pmg_sel;	/* indicate which clocks are turned on
				 * (LMPM_PMG_SEL) */
	uint32_t u_timestamp;	/* indicate when the date and time of the
				 * compilation */
	uint32_t flow_handler;	/* FH read/write pointers, RX credit */
} __packed;

#define ERROR_START_OFFSET  (1 * sizeof(uint32_t))
#define ERROR_ELEM_SIZE     (7 * sizeof(uint32_t))

#ifdef IWM_DEBUG
struct {
	const char *name;
	uint8_t num;
} advanced_lookup[] = {
	{ "NMI_INTERRUPT_WDG", 0x34 },
	{ "SYSASSERT", 0x35 },
	{ "UCODE_VERSION_MISMATCH", 0x37 },
	{ "BAD_COMMAND", 0x38 },
	{ "NMI_INTERRUPT_DATA_ACTION_PT", 0x3C },
	{ "FATAL_ERROR", 0x3D },
	{ "NMI_TRM_HW_ERR", 0x46 },
	{ "NMI_INTERRUPT_TRM", 0x4C },
	{ "NMI_INTERRUPT_BREAK_POINT", 0x54 },
	{ "NMI_INTERRUPT_WDG_RXF_FULL", 0x5C },
	{ "NMI_INTERRUPT_WDG_NO_RBD_RXF_FULL", 0x64 },
	{ "NMI_INTERRUPT_HOST", 0x66 },
	{ "NMI_INTERRUPT_ACTION_PT", 0x7C },
	{ "NMI_INTERRUPT_UNKNOWN", 0x84 },
	{ "NMI_INTERRUPT_INST_ACTION_PT", 0x86 },
	{ "ADVANCED_SYSASSERT", 0 },
};

static const char *
iwm_desc_lookup(uint32_t num)
{
	int i;

	for (i = 0; i < nitems(advanced_lookup) - 1; i++)
		if (advanced_lookup[i].num == num)
			return advanced_lookup[i].name;

	/* No entry matches 'num', so it is the last: ADVANCED_SYSASSERT */
	return advanced_lookup[i].name;
}

/*
 * Support for dumping the error log seemed like a good idea ...
 * but it's mostly hex junk and the only sensible thing is the
 * hw/ucode revision (which we know anyway).  Since it's here,
 * I'll just leave it in, just in case e.g. the Intel guys want to
 * help us decipher some "ADVANCED_SYSASSERT" later.
 */
static void
iwm_nic_error(struct iwm_softc *sc)
{
	struct iwm_error_event_table table;
	uint32_t base;

	device_printf(sc->sc_dev, "dumping device error log\n");
	base = sc->sc_uc.uc_error_event_table;
	if (base < 0x800000 || base >= 0x80C000) {
		device_printf(sc->sc_dev,
		    "Not valid error log pointer 0x%08x\n", base);
		return;
	}

	if (iwm_read_mem(sc, base, &table, sizeof(table)/sizeof(uint32_t)) != 0) {
		device_printf(sc->sc_dev, "reading errlog failed\n");
		return;
	}

	if (!table.valid) {
		device_printf(sc->sc_dev, "errlog not found, skipping\n");
		return;
	}

	if (ERROR_START_OFFSET <= table.valid * ERROR_ELEM_SIZE) {
		device_printf(sc->sc_dev, "Start IWL Error Log Dump:\n");
		device_printf(sc->sc_dev, "Status: 0x%x, count: %d\n",
		    sc->sc_flags, table.valid);
	}

	device_printf(sc->sc_dev, "0x%08X | %-28s\n", table.error_id,
		iwm_desc_lookup(table.error_id));
	device_printf(sc->sc_dev, "%08X | uPc\n", table.pc);
	device_printf(sc->sc_dev, "%08X | branchlink1\n", table.blink1);
	device_printf(sc->sc_dev, "%08X | branchlink2\n", table.blink2);
	device_printf(sc->sc_dev, "%08X | interruptlink1\n", table.ilink1);
	device_printf(sc->sc_dev, "%08X | interruptlink2\n", table.ilink2);
	device_printf(sc->sc_dev, "%08X | data1\n", table.data1);
	device_printf(sc->sc_dev, "%08X | data2\n", table.data2);
	device_printf(sc->sc_dev, "%08X | data3\n", table.data3);
	device_printf(sc->sc_dev, "%08X | beacon time\n", table.bcon_time);
	device_printf(sc->sc_dev, "%08X | tsf low\n", table.tsf_low);
	device_printf(sc->sc_dev, "%08X | tsf hi\n", table.tsf_hi);
	device_printf(sc->sc_dev, "%08X | time gp1\n", table.gp1);
	device_printf(sc->sc_dev, "%08X | time gp2\n", table.gp2);
	device_printf(sc->sc_dev, "%08X | time gp3\n", table.gp3);
	device_printf(sc->sc_dev, "%08X | uCode version\n", table.ucode_ver);
	device_printf(sc->sc_dev, "%08X | hw version\n", table.hw_ver);
	device_printf(sc->sc_dev, "%08X | board version\n", table.brd_ver);
	device_printf(sc->sc_dev, "%08X | hcmd\n", table.hcmd);
	device_printf(sc->sc_dev, "%08X | isr0\n", table.isr0);
	device_printf(sc->sc_dev, "%08X | isr1\n", table.isr1);
	device_printf(sc->sc_dev, "%08X | isr2\n", table.isr2);
	device_printf(sc->sc_dev, "%08X | isr3\n", table.isr3);
	device_printf(sc->sc_dev, "%08X | isr4\n", table.isr4);
	device_printf(sc->sc_dev, "%08X | isr_pref\n", table.isr_pref);
	device_printf(sc->sc_dev, "%08X | wait_event\n", table.wait_event);
	device_printf(sc->sc_dev, "%08X | l2p_control\n", table.l2p_control);
	device_printf(sc->sc_dev, "%08X | l2p_duration\n", table.l2p_duration);
	device_printf(sc->sc_dev, "%08X | l2p_mhvalid\n", table.l2p_mhvalid);
	device_printf(sc->sc_dev, "%08X | l2p_addr_match\n", table.l2p_addr_match);
	device_printf(sc->sc_dev, "%08X | lmpm_pmg_sel\n", table.lmpm_pmg_sel);
	device_printf(sc->sc_dev, "%08X | timestamp\n", table.u_timestamp);
	device_printf(sc->sc_dev, "%08X | flow_handler\n", table.flow_handler);
}
#endif

#define SYNC_RESP_STRUCT(_var_, _pkt_)					\
do {									\
	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_POSTREAD);\
	_var_ = (void *)((_pkt_)+1);					\
} while (/*CONSTCOND*/0)

#define SYNC_RESP_PTR(_ptr_, _len_, _pkt_)				\
do {									\
	bus_dmamap_sync(ring->data_dmat, data->map, BUS_DMASYNC_POSTREAD);\
	_ptr_ = (void *)((_pkt_)+1);					\
} while (/*CONSTCOND*/0)

#define ADVANCE_RXQ(sc) (sc->rxq.cur = (sc->rxq.cur + 1) % IWM_RX_RING_COUNT);

/*
 * Process an IWM_CSR_INT_BIT_FH_RX or IWM_CSR_INT_BIT_SW_RX interrupt.
 * Basic structure from if_iwn
 */
static void
iwm_notif_intr(struct iwm_softc *sc)
{
	uint16_t hw;

	bus_dmamap_sync(sc->rxq.stat_dma.tag, sc->rxq.stat_dma.map,
	    BUS_DMASYNC_POSTREAD);

	hw = le16toh(sc->rxq.stat->closed_rb_num) & 0xfff;
	while (sc->rxq.cur != hw) {
		struct iwm_rx_ring *ring = &sc->rxq;
		struct iwm_rx_data *data = &sc->rxq.data[sc->rxq.cur];
		struct iwm_rx_packet *pkt;
		struct iwm_cmd_response *cresp;
		int qid, idx;

		bus_dmamap_sync(sc->rxq.data_dmat, data->map,
		    BUS_DMASYNC_POSTREAD);
		pkt = mtod(data->m, struct iwm_rx_packet *);

		qid = pkt->hdr.qid & ~0x80;
		idx = pkt->hdr.idx;

		IWM_DPRINTF(sc, IWM_DEBUG_INTR,
		    "rx packet qid=%d idx=%d flags=%x type=%x %d %d\n",
		    pkt->hdr.qid & ~0x80, pkt->hdr.idx, pkt->hdr.flags,
		    pkt->hdr.code, sc->rxq.cur, hw);

		/*
		 * randomly get these from the firmware, no idea why.
		 * they at least seem harmless, so just ignore them for now
		 */
		if (__predict_false((pkt->hdr.code == 0 && qid == 0 && idx == 0)
		    || pkt->len_n_flags == htole32(0x55550000))) {
			ADVANCE_RXQ(sc);
			continue;
		}

		switch (pkt->hdr.code) {
		case IWM_REPLY_RX_PHY_CMD:
			iwm_mvm_rx_rx_phy_cmd(sc, pkt, data);
			break;

		case IWM_REPLY_RX_MPDU_CMD:
			iwm_mvm_rx_rx_mpdu(sc, pkt, data);
			break;

		case IWM_TX_CMD:
			iwm_mvm_rx_tx_cmd(sc, pkt, data);
			break;

		case IWM_MISSED_BEACONS_NOTIFICATION: {
			struct iwm_missed_beacons_notif *resp;
			int missed;

			/* XXX look at mac_id to determine interface ID */
			struct ieee80211com *ic = &sc->sc_ic;
			struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

			SYNC_RESP_STRUCT(resp, pkt);
			missed = le32toh(resp->consec_missed_beacons);

			IWM_DPRINTF(sc, IWM_DEBUG_BEACON | IWM_DEBUG_STATE,
			    "%s: MISSED_BEACON: mac_id=%d, "
			    "consec_since_last_rx=%d, consec=%d, num_expect=%d "
			    "num_rx=%d\n",
			    __func__,
			    le32toh(resp->mac_id),
			    le32toh(resp->consec_missed_beacons_since_last_rx),
			    le32toh(resp->consec_missed_beacons),
			    le32toh(resp->num_expected_beacons),
			    le32toh(resp->num_recvd_beacons));

			/* Be paranoid */
			if (vap == NULL)
				break;

			/* XXX no net80211 locking? */
			if (vap->iv_state == IEEE80211_S_RUN &&
			    (ic->ic_flags & IEEE80211_F_SCAN) == 0) {
				if (missed > vap->iv_bmissthreshold) {
					/* XXX bad locking; turn into task */
					IWM_UNLOCK(sc);
					ieee80211_beacon_miss(ic);
					IWM_LOCK(sc);
				}
			}

			break; }

		case IWM_MVM_ALIVE: {
			struct iwm_mvm_alive_resp *resp;
			SYNC_RESP_STRUCT(resp, pkt);

			sc->sc_uc.uc_error_event_table
			    = le32toh(resp->error_event_table_ptr);
			sc->sc_uc.uc_log_event_table
			    = le32toh(resp->log_event_table_ptr);
			sc->sched_base = le32toh(resp->scd_base_ptr);
			sc->sc_uc.uc_ok = resp->status == IWM_ALIVE_STATUS_OK;

			sc->sc_uc.uc_intr = 1;
			wakeup(&sc->sc_uc);
			break; }

		case IWM_CALIB_RES_NOTIF_PHY_DB: {
			struct iwm_calib_res_notif_phy_db *phy_db_notif;
			SYNC_RESP_STRUCT(phy_db_notif, pkt);

			iwm_phy_db_set_section(sc, phy_db_notif);

			break; }

		case IWM_STATISTICS_NOTIFICATION: {
			struct iwm_notif_statistics *stats;
			SYNC_RESP_STRUCT(stats, pkt);
			memcpy(&sc->sc_stats, stats, sizeof(sc->sc_stats));
			sc->sc_noise = iwm_get_noise(&stats->rx.general);
			break; }

		case IWM_NVM_ACCESS_CMD:
			if (sc->sc_wantresp == ((qid << 16) | idx)) {
				bus_dmamap_sync(sc->rxq.data_dmat, data->map,
				    BUS_DMASYNC_POSTREAD);
				memcpy(sc->sc_cmd_resp,
				    pkt, sizeof(sc->sc_cmd_resp));
			}
			break;

		case IWM_PHY_CONFIGURATION_CMD:
		case IWM_TX_ANT_CONFIGURATION_CMD:
		case IWM_ADD_STA:
		case IWM_MAC_CONTEXT_CMD:
		case IWM_REPLY_SF_CFG_CMD:
		case IWM_POWER_TABLE_CMD:
		case IWM_PHY_CONTEXT_CMD:
		case IWM_BINDING_CONTEXT_CMD:
		case IWM_TIME_EVENT_CMD:
		case IWM_SCAN_REQUEST_CMD:
		case IWM_REPLY_BEACON_FILTERING_CMD:
		case IWM_MAC_PM_POWER_TABLE:
		case IWM_TIME_QUOTA_CMD:
		case IWM_REMOVE_STA:
		case IWM_TXPATH_FLUSH:
		case IWM_LQ_CMD:
			SYNC_RESP_STRUCT(cresp, pkt);
			if (sc->sc_wantresp == ((qid << 16) | idx)) {
				memcpy(sc->sc_cmd_resp,
				    pkt, sizeof(*pkt)+sizeof(*cresp));
			}
			break;

		/* ignore */
		case 0x6c: /* IWM_PHY_DB_CMD, no idea why it's not in fw-api.h */
			break;

		case IWM_INIT_COMPLETE_NOTIF:
			sc->sc_init_complete = 1;
			wakeup(&sc->sc_init_complete);
			break;

		case IWM_SCAN_COMPLETE_NOTIFICATION: {
			struct iwm_scan_complete_notif *notif;
			SYNC_RESP_STRUCT(notif, pkt);
			taskqueue_enqueue(sc->sc_tq, &sc->sc_es_task);
			break; }

		case IWM_REPLY_ERROR: {
			struct iwm_error_resp *resp;
			SYNC_RESP_STRUCT(resp, pkt);

			device_printf(sc->sc_dev,
			    "firmware error 0x%x, cmd 0x%x\n",
			    le32toh(resp->error_type),
			    resp->cmd_id);
			break; }

		case IWM_TIME_EVENT_NOTIFICATION: {
			struct iwm_time_event_notif *notif;
			SYNC_RESP_STRUCT(notif, pkt);

			IWM_DPRINTF(sc, IWM_DEBUG_INTR,
			    "TE notif status = 0x%x action = 0x%x\n",
			        notif->status, notif->action);
			break; }

		case IWM_MCAST_FILTER_CMD:
			break;

		default:
			device_printf(sc->sc_dev,
			    "frame %d/%d %x UNHANDLED (this should "
			    "not happen)\n", qid, idx,
			    pkt->len_n_flags);
			break;
		}

		/*
		 * Why test bit 0x80?  The Linux driver:
		 *
		 * There is one exception:  uCode sets bit 15 when it
		 * originates the response/notification, i.e. when the
		 * response/notification is not a direct response to a
		 * command sent by the driver.  For example, uCode issues
		 * IWM_REPLY_RX when it sends a received frame to the driver;
		 * it is not a direct response to any driver command.
		 *
		 * Ok, so since when is 7 == 15?  Well, the Linux driver
		 * uses a slightly different format for pkt->hdr, and "qid"
		 * is actually the upper byte of a two-byte field.
		 */
		if (!(pkt->hdr.qid & (1 << 7))) {
			iwm_cmd_done(sc, pkt);
		}

		ADVANCE_RXQ(sc);
	}

	IWM_CLRBITS(sc, IWM_CSR_GP_CNTRL,
	    IWM_CSR_GP_CNTRL_REG_FLAG_MAC_ACCESS_REQ);

	/*
	 * Tell the firmware what we have processed.
	 * Seems like the hardware gets upset unless we align
	 * the write by 8??
	 */
	hw = (hw == 0) ? IWM_RX_RING_COUNT - 1 : hw - 1;
	IWM_WRITE(sc, IWM_FH_RSCSR_CHNL0_WPTR, hw & ~7);
}

static void
iwm_intr(void *arg)
{
	struct iwm_softc *sc = arg;
	int handled = 0;
	int r1, r2, rv = 0;
	int isperiodic = 0;

	IWM_LOCK(sc);
	IWM_WRITE(sc, IWM_CSR_INT_MASK, 0);

	if (sc->sc_flags & IWM_FLAG_USE_ICT) {
		uint32_t *ict = sc->ict_dma.vaddr;
		int tmp;

		tmp = htole32(ict[sc->ict_cur]);
		if (!tmp)
			goto out_ena;

		/*
		 * ok, there was something.  keep plowing until we have all.
		 */
		r1 = r2 = 0;
		while (tmp) {
			r1 |= tmp;
			ict[sc->ict_cur] = 0;
			sc->ict_cur = (sc->ict_cur+1) % IWM_ICT_COUNT;
			tmp = htole32(ict[sc->ict_cur]);
		}

		/* this is where the fun begins.  don't ask */
		if (r1 == 0xffffffff)
			r1 = 0;

		/* i am not expected to understand this */
		if (r1 & 0xc0000)
			r1 |= 0x8000;
		r1 = (0xff & r1) | ((0xff00 & r1) << 16);
	} else {
		r1 = IWM_READ(sc, IWM_CSR_INT);
		/* "hardware gone" (where, fishing?) */
		if (r1 == 0xffffffff || (r1 & 0xfffffff0) == 0xa5a5a5a0)
			goto out;
		r2 = IWM_READ(sc, IWM_CSR_FH_INT_STATUS);
	}
	if (r1 == 0 && r2 == 0) {
		goto out_ena;
	}

	IWM_WRITE(sc, IWM_CSR_INT, r1 | ~sc->sc_intmask);

	/* ignored */
	handled |= (r1 & (IWM_CSR_INT_BIT_ALIVE /*| IWM_CSR_INT_BIT_SCD*/));

	if (r1 & IWM_CSR_INT_BIT_SW_ERR) {
		int i;
		struct ieee80211com *ic = &sc->sc_ic;
		struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

#ifdef IWM_DEBUG
		iwm_nic_error(sc);
#endif
		/* Dump driver status (TX and RX rings) while we're here. */
		device_printf(sc->sc_dev, "driver status:\n");
		for (i = 0; i < IWM_MVM_MAX_QUEUES; i++) {
			struct iwm_tx_ring *ring = &sc->txq[i];
			device_printf(sc->sc_dev,
			    "  tx ring %2d: qid=%-2d cur=%-3d "
			    "queued=%-3d\n",
			    i, ring->qid, ring->cur, ring->queued);
		}
		device_printf(sc->sc_dev,
		    "  rx ring: cur=%d\n", sc->rxq.cur);
		device_printf(sc->sc_dev,
		    "  802.11 state %d\n", (vap == NULL) ? -1 : vap->iv_state);

		/* Don't stop the device; just do a VAP restart */
		IWM_UNLOCK(sc);

		if (vap == NULL) {
			printf("%s: null vap\n", __func__);
			return;
		}

		device_printf(sc->sc_dev, "%s: controller panicked, iv_state = %d; "
		    "restarting\n", __func__, vap->iv_state);

		/* XXX TODO: turn this into a callout/taskqueue */
		ieee80211_restart_all(ic);
		return;
	}

	if (r1 & IWM_CSR_INT_BIT_HW_ERR) {
		handled |= IWM_CSR_INT_BIT_HW_ERR;
		device_printf(sc->sc_dev, "hardware error, stopping device\n");
		iwm_stop(sc);
		rv = 1;
		goto out;
	}

	/* firmware chunk loaded */
	if (r1 & IWM_CSR_INT_BIT_FH_TX) {
		IWM_WRITE(sc, IWM_CSR_FH_INT_STATUS, IWM_CSR_FH_INT_TX_MASK);
		handled |= IWM_CSR_INT_BIT_FH_TX;
		sc->sc_fw_chunk_done = 1;
		wakeup(&sc->sc_fw);
	}

	if (r1 & IWM_CSR_INT_BIT_RF_KILL) {
		handled |= IWM_CSR_INT_BIT_RF_KILL;
		if (iwm_check_rfkill(sc)) {
			device_printf(sc->sc_dev,
			    "%s: rfkill switch, disabling interface\n",
			    __func__);
			iwm_stop(sc);
		}
	}

	/*
	 * The Linux driver uses periodic interrupts to avoid races.
	 * We cargo-cult like it's going out of fashion.
	 */
	if (r1 & IWM_CSR_INT_BIT_RX_PERIODIC) {
		handled |= IWM_CSR_INT_BIT_RX_PERIODIC;
		IWM_WRITE(sc, IWM_CSR_INT, IWM_CSR_INT_BIT_RX_PERIODIC);
		if ((r1 & (IWM_CSR_INT_BIT_FH_RX | IWM_CSR_INT_BIT_SW_RX)) == 0)
			IWM_WRITE_1(sc,
			    IWM_CSR_INT_PERIODIC_REG, IWM_CSR_INT_PERIODIC_DIS);
		isperiodic = 1;
	}

	if ((r1 & (IWM_CSR_INT_BIT_FH_RX | IWM_CSR_INT_BIT_SW_RX)) || isperiodic) {
		handled |= (IWM_CSR_INT_BIT_FH_RX | IWM_CSR_INT_BIT_SW_RX);
		IWM_WRITE(sc, IWM_CSR_FH_INT_STATUS, IWM_CSR_FH_INT_RX_MASK);

		iwm_notif_intr(sc);

		/* enable periodic interrupt, see above */
		if (r1 & (IWM_CSR_INT_BIT_FH_RX | IWM_CSR_INT_BIT_SW_RX) && !isperiodic)
			IWM_WRITE_1(sc, IWM_CSR_INT_PERIODIC_REG,
			    IWM_CSR_INT_PERIODIC_ENA);
	}

	if (__predict_false(r1 & ~handled))
		IWM_DPRINTF(sc, IWM_DEBUG_INTR,
		    "%s: unhandled interrupts: %x\n", __func__, r1);
	rv = 1;

 out_ena:
	iwm_restore_interrupts(sc);
 out:
	IWM_UNLOCK(sc);
	return;
}

/*
 * Autoconf glue-sniffing
 */
#define	PCI_VENDOR_INTEL		0x8086
#define	PCI_PRODUCT_INTEL_WL_3160_1	0x08b3
#define	PCI_PRODUCT_INTEL_WL_3160_2	0x08b4
#define	PCI_PRODUCT_INTEL_WL_7260_1	0x08b1
#define	PCI_PRODUCT_INTEL_WL_7260_2	0x08b2
#define	PCI_PRODUCT_INTEL_WL_7265_1	0x095a
#define	PCI_PRODUCT_INTEL_WL_7265_2	0x095b

static const struct iwm_devices {
	uint16_t	device;
	const char	*name;
} iwm_devices[] = {
	{ PCI_PRODUCT_INTEL_WL_3160_1, "Intel Dual Band Wireless AC 3160" },
	{ PCI_PRODUCT_INTEL_WL_3160_2, "Intel Dual Band Wireless AC 3160" },
	{ PCI_PRODUCT_INTEL_WL_7260_1, "Intel Dual Band Wireless AC 7260" },
	{ PCI_PRODUCT_INTEL_WL_7260_2, "Intel Dual Band Wireless AC 7260" },
	{ PCI_PRODUCT_INTEL_WL_7265_1, "Intel Dual Band Wireless AC 7265" },
	{ PCI_PRODUCT_INTEL_WL_7265_2, "Intel Dual Band Wireless AC 7265" },
};

static int
iwm_probe(device_t dev)
{
	int i;

	for (i = 0; i < nitems(iwm_devices); i++)
		if (pci_get_vendor(dev) == PCI_VENDOR_INTEL &&
		    pci_get_device(dev) == iwm_devices[i].device) {
			device_set_desc(dev, iwm_devices[i].name);
			return (BUS_PROBE_DEFAULT);
		}

	return (ENXIO);
}

static int
iwm_dev_check(device_t dev)
{
	struct iwm_softc *sc;

	sc = device_get_softc(dev);

	switch (pci_get_device(dev)) {
	case PCI_PRODUCT_INTEL_WL_3160_1:
	case PCI_PRODUCT_INTEL_WL_3160_2:
		sc->sc_fwname = "iwm3160fw";
		sc->host_interrupt_operation_mode = 1;
		return (0);
	case PCI_PRODUCT_INTEL_WL_7260_1:
	case PCI_PRODUCT_INTEL_WL_7260_2:
		sc->sc_fwname = "iwm7260fw";
		sc->host_interrupt_operation_mode = 1;
		return (0);
	case PCI_PRODUCT_INTEL_WL_7265_1:
	case PCI_PRODUCT_INTEL_WL_7265_2:
		sc->sc_fwname = "iwm7265fw";
		sc->host_interrupt_operation_mode = 0;
		return (0);
	default:
		device_printf(dev, "unknown adapter type\n");
		return ENXIO;
	}
}

static int
iwm_pci_attach(device_t dev)
{
	struct iwm_softc *sc;
	int count, error, rid;
	uint16_t reg;

	sc = device_get_softc(dev);

	/* Clear device-specific "PCI retry timeout" register (41h). */
	reg = pci_read_config(dev, 0x40, sizeof(reg));
	pci_write_config(dev, 0x40, reg & ~0xff00, sizeof(reg));

	/* Enable bus-mastering and hardware bug workaround. */
	pci_enable_busmaster(dev);
	reg = pci_read_config(dev, PCIR_STATUS, sizeof(reg));
	/* if !MSI */
	if (reg & PCIM_STATUS_INTxSTATE) {
		reg &= ~PCIM_STATUS_INTxSTATE;
	}
	pci_write_config(dev, PCIR_STATUS, reg, sizeof(reg));

	rid = PCIR_BAR(0);
	sc->sc_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->sc_mem == NULL) {
		device_printf(sc->sc_dev, "can't map mem space\n");
		return (ENXIO);
	}
	sc->sc_st = rman_get_bustag(sc->sc_mem);
	sc->sc_sh = rman_get_bushandle(sc->sc_mem);

	/* Install interrupt handler. */
	count = 1;
	rid = 0;
	if (pci_alloc_msi(dev, &count) == 0)
		rid = 1;
	sc->sc_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE |
	    (rid != 0 ? 0 : RF_SHAREABLE));
	if (sc->sc_irq == NULL) {
		device_printf(dev, "can't map interrupt\n");
			return (ENXIO);
	}
	error = bus_setup_intr(dev, sc->sc_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, iwm_intr, sc, &sc->sc_ih);
	if (sc->sc_ih == NULL) {
		device_printf(dev, "can't establish interrupt");
			return (ENXIO);
	}
	sc->sc_dmat = bus_get_dma_tag(sc->sc_dev);

	return (0);
}

static void
iwm_pci_detach(device_t dev)
{
	struct iwm_softc *sc = device_get_softc(dev);

	if (sc->sc_irq != NULL) {
		bus_teardown_intr(dev, sc->sc_irq, sc->sc_ih);
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->sc_irq), sc->sc_irq);
		pci_release_msi(dev);
        }
	if (sc->sc_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->sc_mem), sc->sc_mem);
}



static int
iwm_attach(device_t dev)
{
	struct iwm_softc *sc = device_get_softc(dev);
	struct ieee80211com *ic = &sc->sc_ic;
	int error;
	int txq_i, i;

	sc->sc_dev = dev;
	IWM_LOCK_INIT(sc);
	mbufq_init(&sc->sc_snd, ifqmaxlen);
	callout_init_mtx(&sc->sc_watchdog_to, &sc->sc_mtx, 0);
	callout_init_mtx(&sc->sc_led_blink_to, &sc->sc_mtx, 0);
	TASK_INIT(&sc->sc_es_task, 0, iwm_endscan_cb, sc);
	sc->sc_tq = taskqueue_create("iwm_taskq", M_WAITOK,
            taskqueue_thread_enqueue, &sc->sc_tq);
        error = taskqueue_start_threads(&sc->sc_tq, 1, 0, "iwm_taskq");
        if (error != 0) {
                device_printf(dev, "can't start threads, error %d\n",
		    error);
		goto fail;
        }

	/* PCI attach */
	error = iwm_pci_attach(dev);
	if (error != 0)
		goto fail;

	sc->sc_wantresp = -1;

	/* Check device type */
	error = iwm_dev_check(dev);
	if (error != 0)
		goto fail;

	sc->sc_fwdmasegsz = IWM_FWDMASEGSZ;

	/*
	 * We now start fiddling with the hardware
	 */
	sc->sc_hw_rev = IWM_READ(sc, IWM_CSR_HW_REV);
	if (iwm_prepare_card_hw(sc) != 0) {
		device_printf(dev, "could not initialize hardware\n");
		goto fail;
	}

	/* Allocate DMA memory for firmware transfers. */
	if ((error = iwm_alloc_fwmem(sc)) != 0) {
		device_printf(dev, "could not allocate memory for firmware\n");
		goto fail;
	}

	/* Allocate "Keep Warm" page. */
	if ((error = iwm_alloc_kw(sc)) != 0) {
		device_printf(dev, "could not allocate keep warm page\n");
		goto fail;
	}

	/* We use ICT interrupts */
	if ((error = iwm_alloc_ict(sc)) != 0) {
		device_printf(dev, "could not allocate ICT table\n");
		goto fail;
	}

	/* Allocate TX scheduler "rings". */
	if ((error = iwm_alloc_sched(sc)) != 0) {
		device_printf(dev, "could not allocate TX scheduler rings\n");
		goto fail;
	}

	/* Allocate TX rings */
	for (txq_i = 0; txq_i < nitems(sc->txq); txq_i++) {
		if ((error = iwm_alloc_tx_ring(sc,
		    &sc->txq[txq_i], txq_i)) != 0) {
			device_printf(dev,
			    "could not allocate TX ring %d\n",
			    txq_i);
			goto fail;
		}
	}

	/* Allocate RX ring. */
	if ((error = iwm_alloc_rx_ring(sc, &sc->rxq)) != 0) {
		device_printf(dev, "could not allocate RX ring\n");
		goto fail;
	}

	/* Clear pending interrupts. */
	IWM_WRITE(sc, IWM_CSR_INT, 0xffffffff);

	ic->ic_softc = sc;
	ic->ic_name = device_get_nameunit(sc->sc_dev);
	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */

	/* Set device capabilities. */
	ic->ic_caps =
	    IEEE80211_C_STA |
	    IEEE80211_C_WPA |		/* WPA/RSN */
	    IEEE80211_C_WME |
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE	/* short preamble supported */
//	    IEEE80211_C_BGSCAN		/* capable of bg scanning */
	    ;
	for (i = 0; i < nitems(sc->sc_phyctxt); i++) {
		sc->sc_phyctxt[i].id = i;
		sc->sc_phyctxt[i].color = 0;
		sc->sc_phyctxt[i].ref = 0;
		sc->sc_phyctxt[i].channel = NULL;
	}

	/* Max RSSI */
	sc->sc_max_rssi = IWM_MAX_DBM - IWM_MIN_DBM;
	sc->sc_preinit_hook.ich_func = iwm_preinit;
	sc->sc_preinit_hook.ich_arg = sc;
	if (config_intrhook_establish(&sc->sc_preinit_hook) != 0) {
		device_printf(dev, "config_intrhook_establish failed\n");
		goto fail;
	}

#ifdef IWM_DEBUG
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "debug",
	    CTLFLAG_RW, &sc->sc_debug, 0, "control debugging");
#endif

	IWM_DPRINTF(sc, IWM_DEBUG_RESET | IWM_DEBUG_TRACE,
	    "<-%s\n", __func__);

	return 0;

	/* Free allocated memory if something failed during attachment. */
fail:
	iwm_detach_local(sc, 0);

	return ENXIO;
}

static int
iwm_update_edca(struct ieee80211com *ic)
{
	struct iwm_softc *sc = ic->ic_softc;

	device_printf(sc->sc_dev, "%s: called\n", __func__);
	return (0);
}

static void
iwm_preinit(void *arg)
{
	struct iwm_softc *sc = arg;
	device_t dev = sc->sc_dev;
	struct ieee80211com *ic = &sc->sc_ic;
	int error;

	IWM_DPRINTF(sc, IWM_DEBUG_RESET | IWM_DEBUG_TRACE,
	    "->%s\n", __func__);

	IWM_LOCK(sc);
	if ((error = iwm_start_hw(sc)) != 0) {
		device_printf(dev, "could not initialize hardware\n");
		IWM_UNLOCK(sc);
		goto fail;
	}

	error = iwm_run_init_mvm_ucode(sc, 1);
	iwm_stop_device(sc);
	if (error) {
		IWM_UNLOCK(sc);
		goto fail;
	}
	device_printf(dev,
	    "revision: 0x%x, firmware %d.%d (API ver. %d)\n",
	    sc->sc_hw_rev & IWM_CSR_HW_REV_TYPE_MSK,
	    IWM_UCODE_MAJOR(sc->sc_fwver),
	    IWM_UCODE_MINOR(sc->sc_fwver),
	    IWM_UCODE_API(sc->sc_fwver));

	/* not all hardware can do 5GHz band */
	if (!sc->sc_nvm.sku_cap_band_52GHz_enable)
		memset(&ic->ic_sup_rates[IEEE80211_MODE_11A], 0,
		    sizeof(ic->ic_sup_rates[IEEE80211_MODE_11A]));
	IWM_UNLOCK(sc);

	iwm_init_channel_map(ic, IEEE80211_CHAN_MAX, &ic->ic_nchans,
	    ic->ic_channels);

	/*
	 * At this point we've committed - if we fail to do setup,
	 * we now also have to tear down the net80211 state.
	 */
	ieee80211_ifattach(ic);
	ic->ic_vap_create = iwm_vap_create;
	ic->ic_vap_delete = iwm_vap_delete;
	ic->ic_raw_xmit = iwm_raw_xmit;
	ic->ic_node_alloc = iwm_node_alloc;
	ic->ic_scan_start = iwm_scan_start;
	ic->ic_scan_end = iwm_scan_end;
	ic->ic_update_mcast = iwm_update_mcast;
	ic->ic_getradiocaps = iwm_init_channel_map;
	ic->ic_set_channel = iwm_set_channel;
	ic->ic_scan_curchan = iwm_scan_curchan;
	ic->ic_scan_mindwell = iwm_scan_mindwell;
	ic->ic_wme.wme_update = iwm_update_edca;
	ic->ic_parent = iwm_parent;
	ic->ic_transmit = iwm_transmit;
	iwm_radiotap_attach(sc);
	if (bootverbose)
		ieee80211_announce(ic);

	IWM_DPRINTF(sc, IWM_DEBUG_RESET | IWM_DEBUG_TRACE,
	    "<-%s\n", __func__);
	config_intrhook_disestablish(&sc->sc_preinit_hook);

	return;
fail:
	config_intrhook_disestablish(&sc->sc_preinit_hook);
	iwm_detach_local(sc, 0);
}

/*
 * Attach the interface to 802.11 radiotap.
 */
static void
iwm_radiotap_attach(struct iwm_softc *sc)
{
        struct ieee80211com *ic = &sc->sc_ic;

	IWM_DPRINTF(sc, IWM_DEBUG_RESET | IWM_DEBUG_TRACE,
	    "->%s begin\n", __func__);
        ieee80211_radiotap_attach(ic,
            &sc->sc_txtap.wt_ihdr, sizeof(sc->sc_txtap),
                IWM_TX_RADIOTAP_PRESENT,
            &sc->sc_rxtap.wr_ihdr, sizeof(sc->sc_rxtap),
                IWM_RX_RADIOTAP_PRESENT);
	IWM_DPRINTF(sc, IWM_DEBUG_RESET | IWM_DEBUG_TRACE,
	    "->%s end\n", __func__);
}

static struct ieee80211vap *
iwm_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ], int unit,
    enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct iwm_vap *ivp;
	struct ieee80211vap *vap;

	if (!TAILQ_EMPTY(&ic->ic_vaps))         /* only one at a time */
		return NULL;
	ivp = malloc(sizeof(struct iwm_vap), M_80211_VAP, M_WAITOK | M_ZERO);
	vap = &ivp->iv_vap;
	ieee80211_vap_setup(ic, vap, name, unit, opmode, flags, bssid);
	vap->iv_bmissthreshold = 10;            /* override default */
	/* Override with driver methods. */
	ivp->iv_newstate = vap->iv_newstate;
	vap->iv_newstate = iwm_newstate;

	ieee80211_ratectl_init(vap);
	/* Complete setup. */
	ieee80211_vap_attach(vap, iwm_media_change, ieee80211_media_status,
	    mac);
	ic->ic_opmode = opmode;

	return vap;
}

static void
iwm_vap_delete(struct ieee80211vap *vap)
{
	struct iwm_vap *ivp = IWM_VAP(vap);

	ieee80211_ratectl_deinit(vap);
	ieee80211_vap_detach(vap);
	free(ivp, M_80211_VAP);
}

static void
iwm_scan_start(struct ieee80211com *ic)
{
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
        struct iwm_softc *sc = ic->ic_softc;
	int error;

	if (sc->sc_scanband)
		return;
	IWM_LOCK(sc);
	error = iwm_mvm_scan_request(sc, IEEE80211_CHAN_2GHZ, 0, NULL, 0);
	if (error) {
		device_printf(sc->sc_dev, "could not initiate 2 GHz scan\n");
		IWM_UNLOCK(sc);
		ieee80211_cancel_scan(vap);
		sc->sc_scanband = 0;
	} else {
		iwm_led_blink_start(sc);
		IWM_UNLOCK(sc);
	}
}

static void
iwm_scan_end(struct ieee80211com *ic)
{
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	struct iwm_softc *sc = ic->ic_softc;

	IWM_LOCK(sc);
	iwm_led_blink_stop(sc);
	if (vap->iv_state == IEEE80211_S_RUN)
		iwm_mvm_led_enable(sc);
	IWM_UNLOCK(sc);
}

static void
iwm_update_mcast(struct ieee80211com *ic)
{
}

static void
iwm_set_channel(struct ieee80211com *ic)
{
}

static void
iwm_scan_curchan(struct ieee80211_scan_state *ss, unsigned long maxdwell)
{
}

static void
iwm_scan_mindwell(struct ieee80211_scan_state *ss)
{
	return;
}

void
iwm_init_task(void *arg1)
{
	struct iwm_softc *sc = arg1;

	IWM_LOCK(sc);
	while (sc->sc_flags & IWM_FLAG_BUSY)
		msleep(&sc->sc_flags, &sc->sc_mtx, 0, "iwmpwr", 0);
	sc->sc_flags |= IWM_FLAG_BUSY;
	iwm_stop(sc);
	if (sc->sc_ic.ic_nrunning > 0)
		iwm_init(sc);
	sc->sc_flags &= ~IWM_FLAG_BUSY;
	wakeup(&sc->sc_flags);
	IWM_UNLOCK(sc);
}

static int
iwm_resume(device_t dev)
{
	struct iwm_softc *sc = device_get_softc(dev);
	int do_reinit = 0;
	uint16_t reg;

	/* Clear device-specific "PCI retry timeout" register (41h). */
	reg = pci_read_config(dev, 0x40, sizeof(reg));
	pci_write_config(dev, 0x40, reg & ~0xff00, sizeof(reg));
	iwm_init_task(device_get_softc(dev));

	IWM_LOCK(sc);
	if (sc->sc_flags & IWM_FLAG_DORESUME) {
		sc->sc_flags &= ~IWM_FLAG_DORESUME;
		do_reinit = 1;
	}
	IWM_UNLOCK(sc);

	if (do_reinit)
		ieee80211_resume_all(&sc->sc_ic);

	return 0;
}

static int
iwm_suspend(device_t dev)
{
	int do_stop = 0;
	struct iwm_softc *sc = device_get_softc(dev);

	do_stop = !! (sc->sc_ic.ic_nrunning > 0);

	ieee80211_suspend_all(&sc->sc_ic);

	if (do_stop) {
		IWM_LOCK(sc);
		iwm_stop(sc);
		sc->sc_flags |= IWM_FLAG_DORESUME;
		IWM_UNLOCK(sc);
	}

	return (0);
}

static int
iwm_detach_local(struct iwm_softc *sc, int do_net80211)
{
	struct iwm_fw_info *fw = &sc->sc_fw;
	device_t dev = sc->sc_dev;
	int i;

	if (sc->sc_tq) {
		taskqueue_drain_all(sc->sc_tq);
		taskqueue_free(sc->sc_tq);
	}
	callout_drain(&sc->sc_led_blink_to);
	callout_drain(&sc->sc_watchdog_to);
	iwm_stop_device(sc);
	if (do_net80211)
		ieee80211_ifdetach(&sc->sc_ic);

	/* Free descriptor rings */
	for (i = 0; i < nitems(sc->txq); i++)
		iwm_free_tx_ring(sc, &sc->txq[i]);

	/* Free firmware */
	if (fw->fw_fp != NULL)
		iwm_fw_info_free(fw);

	/* Free scheduler */
	iwm_free_sched(sc);
	if (sc->ict_dma.vaddr != NULL)
		iwm_free_ict(sc);
	if (sc->kw_dma.vaddr != NULL)
		iwm_free_kw(sc);
	if (sc->fw_dma.vaddr != NULL)
		iwm_free_fwmem(sc);

	/* Finished with the hardware - detach things */
	iwm_pci_detach(dev);

	mbufq_drain(&sc->sc_snd);
	IWM_LOCK_DESTROY(sc);

	return (0);
}

static int
iwm_detach(device_t dev)
{
	struct iwm_softc *sc = device_get_softc(dev);

	return (iwm_detach_local(sc, 1));
}

static device_method_t iwm_pci_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,         iwm_probe),
        DEVMETHOD(device_attach,        iwm_attach),
        DEVMETHOD(device_detach,        iwm_detach),
        DEVMETHOD(device_suspend,       iwm_suspend),
        DEVMETHOD(device_resume,        iwm_resume),

        DEVMETHOD_END
};

static driver_t iwm_pci_driver = {
        "iwm",
        iwm_pci_methods,
        sizeof (struct iwm_softc)
};

static devclass_t iwm_devclass;

DRIVER_MODULE(iwm, pci, iwm_pci_driver, iwm_devclass, NULL, NULL);
MODULE_DEPEND(iwm, firmware, 1, 1, 1);
MODULE_DEPEND(iwm, pci, 1, 1, 1);
MODULE_DEPEND(iwm, wlan, 1, 1, 1);
