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
#include <dev/iwm/if_iwm_scan.h>

/*
 * BEGIN mvm/scan.c
 */

#define IWM_PLCP_QUIET_THRESH 1
#define IWM_ACTIVE_QUIET_TIME 10
#define LONG_OUT_TIME_PERIOD (600 * IEEE80211_DUR_TU)
#define SHORT_OUT_TIME_PERIOD (200 * IEEE80211_DUR_TU)
#define SUSPEND_TIME_PERIOD (100 * IEEE80211_DUR_TU)

static uint16_t
iwm_mvm_scan_rx_chain(struct iwm_softc *sc)
{
	uint16_t rx_chain;
	uint8_t rx_ant;

	rx_ant = IWM_FW_VALID_RX_ANT(sc);
	rx_chain = rx_ant << IWM_PHY_RX_CHAIN_VALID_POS;
	rx_chain |= rx_ant << IWM_PHY_RX_CHAIN_FORCE_MIMO_SEL_POS;
	rx_chain |= rx_ant << IWM_PHY_RX_CHAIN_FORCE_SEL_POS;
	rx_chain |= 0x1 << IWM_PHY_RX_CHAIN_DRIVER_FORCE_POS;
	return htole16(rx_chain);
}

static uint32_t
iwm_mvm_scan_max_out_time(struct iwm_softc *sc, uint32_t flags, int is_assoc)
{
	if (!is_assoc)
		return 0;
	if (flags & 0x1)
		return htole32(SHORT_OUT_TIME_PERIOD);
	return htole32(LONG_OUT_TIME_PERIOD);
}

static uint32_t
iwm_mvm_scan_suspend_time(struct iwm_softc *sc, int is_assoc)
{
	if (!is_assoc)
		return 0;
	return htole32(SUSPEND_TIME_PERIOD);
}

static uint32_t
iwm_mvm_scan_rxon_flags(struct iwm_softc *sc, int flags)
{
	if (flags & IEEE80211_CHAN_2GHZ)
		return htole32(IWM_PHY_BAND_24);
	else
		return htole32(IWM_PHY_BAND_5);
}

static uint32_t
iwm_mvm_scan_rate_n_flags(struct iwm_softc *sc, int flags, int no_cck)
{
	uint32_t tx_ant;
	int i, ind;

	for (i = 0, ind = sc->sc_scan_last_antenna;
	    i < IWM_RATE_MCS_ANT_NUM; i++) {
		ind = (ind + 1) % IWM_RATE_MCS_ANT_NUM;
		if (IWM_FW_VALID_TX_ANT(sc) & (1 << ind)) {
			sc->sc_scan_last_antenna = ind;
			break;
		}
	}
	tx_ant = (1 << sc->sc_scan_last_antenna) << IWM_RATE_MCS_ANT_POS;

	if ((flags & IEEE80211_CHAN_2GHZ) && !no_cck)
		return htole32(IWM_RATE_1M_PLCP | IWM_RATE_MCS_CCK_MSK |
				   tx_ant);
	else
		return htole32(IWM_RATE_6M_PLCP | tx_ant);
}

/*
 * If req->n_ssids > 0, it means we should do an active scan.
 * In case of active scan w/o directed scan, we receive a zero-length SSID
 * just to notify that this scan is active and not passive.
 * In order to notify the FW of the number of SSIDs we wish to scan (including
 * the zero-length one), we need to set the corresponding bits in chan->type,
 * one for each SSID, and set the active bit (first). If the first SSID is
 * already included in the probe template, so we need to set only
 * req->n_ssids - 1 bits in addition to the first bit.
 */
static uint16_t
iwm_mvm_get_active_dwell(struct iwm_softc *sc, int flags, int n_ssids)
{
	if (flags & IEEE80211_CHAN_2GHZ)
		return 30  + 3 * (n_ssids + 1);
	return 20  + 2 * (n_ssids + 1);
}

static uint16_t
iwm_mvm_get_passive_dwell(struct iwm_softc *sc, int flags)
{
	return (flags & IEEE80211_CHAN_2GHZ) ? 100 + 20 : 100 + 10;
}

static int
iwm_mvm_scan_fill_channels(struct iwm_softc *sc, struct iwm_scan_cmd *cmd,
	int flags, int n_ssids, int basic_ssid)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t passive_dwell = iwm_mvm_get_passive_dwell(sc, flags);
	uint16_t active_dwell = iwm_mvm_get_active_dwell(sc, flags, n_ssids);
	struct iwm_scan_channel *chan = (struct iwm_scan_channel *)
		(cmd->data + le16toh(cmd->tx_cmd.len));
	int type = (1 << n_ssids) - 1;
	struct ieee80211_channel *c;
	int nchan, j;

	if (!basic_ssid)
		type |= (1 << n_ssids);

	for (nchan = j = 0; j < ic->ic_nchans; j++) {
		c = &ic->ic_channels[j];
		/* For 2GHz, only populate 11b channels */
		/* For 5GHz, only populate 11a channels */
		/*
		 * Catch other channels, in case we have 900MHz channels or
		 * something in the chanlist.
		 */
		if ((flags & IEEE80211_CHAN_2GHZ) && (! IEEE80211_IS_CHAN_B(c))) {
			continue;
		} else if ((flags & IEEE80211_CHAN_5GHZ) && (! IEEE80211_IS_CHAN_A(c))) {
			continue;
		} else {
			IWM_DPRINTF(sc, IWM_DEBUG_RESET | IWM_DEBUG_EEPROM,
			    "%s: skipping channel (freq=%d, ieee=%d, flags=0x%08x)\n",
			    __func__,
			    c->ic_freq,
			    c->ic_ieee,
			    c->ic_flags);
		}
		IWM_DPRINTF(sc, IWM_DEBUG_RESET | IWM_DEBUG_EEPROM,
		    "Adding channel %d (%d Mhz) to the list\n",
			nchan, c->ic_freq);
		chan->channel = htole16(ieee80211_mhz2ieee(c->ic_freq, flags));
		chan->type = htole32(type);
		if (c->ic_flags & IEEE80211_CHAN_PASSIVE)
			chan->type &= htole32(~IWM_SCAN_CHANNEL_TYPE_ACTIVE);
		chan->active_dwell = htole16(active_dwell);
		chan->passive_dwell = htole16(passive_dwell);
		chan->iteration_count = htole16(1);
		chan++;
		nchan++;
	}
	if (nchan == 0)
		device_printf(sc->sc_dev,
		    "%s: NO CHANNEL!\n", __func__);
	return nchan;
}

/*
 * Fill in probe request with the following parameters:
 * TA is our vif HW address, which mac80211 ensures we have.
 * Packet is broadcasted, so this is both SA and DA.
 * The probe request IE is made out of two: first comes the most prioritized
 * SSID if a directed scan is requested. Second comes whatever extra
 * information was given to us as the scan request IE.
 */
static uint16_t
iwm_mvm_fill_probe_req(struct iwm_softc *sc, struct ieee80211_frame *frame,
	const uint8_t *ta, int n_ssids, const uint8_t *ssid, int ssid_len,
	const uint8_t *ie, int ie_len, int left)
{
	uint8_t *pos = NULL;

	/* Make sure there is enough space for the probe request,
	 * two mandatory IEs and the data */
	left -= sizeof(*frame);
	if (left < 0)
		return 0;

	frame->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_MGT |
	    IEEE80211_FC0_SUBTYPE_PROBE_REQ;
	frame->i_fc[1] = IEEE80211_FC1_DIR_NODS;
	IEEE80211_ADDR_COPY(frame->i_addr1, ieee80211broadcastaddr);
	IEEE80211_ADDR_COPY(frame->i_addr2, ta);
	IEEE80211_ADDR_COPY(frame->i_addr3, ieee80211broadcastaddr);

	/* for passive scans, no need to fill anything */
	if (n_ssids == 0)
		return sizeof(*frame);

	/* points to the payload of the request */
	pos = (uint8_t *)frame + sizeof(*frame);

	/* fill in our SSID IE */
	left -= ssid_len + 2;
	if (left < 0)
		return 0;

	pos = ieee80211_add_ssid(pos, ssid, ssid_len);

	if (ie && ie_len && left >= ie_len) {
		memcpy(pos, ie, ie_len);
		pos += ie_len;
	}

	return pos - (uint8_t *)frame;
}

int
iwm_mvm_scan_request(struct iwm_softc *sc, int flags,
	int n_ssids, uint8_t *ssid, int ssid_len)
{
	struct iwm_host_cmd hcmd = {
		.id = IWM_SCAN_REQUEST_CMD,
		.len = { 0, },
		.data = { sc->sc_scan_cmd, },
		.flags = IWM_CMD_SYNC,
		.dataflags = { IWM_HCMD_DFL_NOCOPY, },
	};
	struct iwm_scan_cmd *cmd = sc->sc_scan_cmd;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);
	int is_assoc = 0;
	int ret;
	uint32_t status;
	int basic_ssid =
	    !(sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_NO_BASIC_SSID);

	sc->sc_scanband = flags & (IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_5GHZ);

	IWM_DPRINTF(sc, IWM_DEBUG_SCAN,
	    "Handling ieee80211 scan request\n");
	memset(cmd, 0, sc->sc_scan_cmd_len);

	cmd->quiet_time = htole16(IWM_ACTIVE_QUIET_TIME);
	cmd->quiet_plcp_th = htole16(IWM_PLCP_QUIET_THRESH);
	cmd->rxchain_sel_flags = iwm_mvm_scan_rx_chain(sc);
	cmd->max_out_time = iwm_mvm_scan_max_out_time(sc, 0, is_assoc);
	cmd->suspend_time = iwm_mvm_scan_suspend_time(sc, is_assoc);
	cmd->rxon_flags = iwm_mvm_scan_rxon_flags(sc, flags);
	cmd->filter_flags = htole32(IWM_MAC_FILTER_ACCEPT_GRP |
	    IWM_MAC_FILTER_IN_BEACON);

	cmd->type = htole32(IWM_SCAN_TYPE_FORCED);
	cmd->repeats = htole32(1);

	/*
	 * If the user asked for passive scan, don't change to active scan if
	 * you see any activity on the channel - remain passive.
	 */
	if (n_ssids > 0) {
		cmd->passive2active = htole16(1);
		cmd->scan_flags |= IWM_SCAN_FLAGS_PASSIVE2ACTIVE;
#if 0
		if (basic_ssid) {
			ssid = req->ssids[0].ssid;
			ssid_len = req->ssids[0].ssid_len;
		}
#endif
	} else {
		cmd->passive2active = 0;
		cmd->scan_flags &= ~IWM_SCAN_FLAGS_PASSIVE2ACTIVE;
	}

	cmd->tx_cmd.tx_flags = htole32(IWM_TX_CMD_FLG_SEQ_CTL |
	    IWM_TX_CMD_FLG_BT_DIS);
	cmd->tx_cmd.sta_id = sc->sc_aux_sta.sta_id;
	cmd->tx_cmd.life_time = htole32(IWM_TX_CMD_LIFE_TIME_INFINITE);
	cmd->tx_cmd.rate_n_flags = iwm_mvm_scan_rate_n_flags(sc, flags, 1/*XXX*/);

	cmd->tx_cmd.len = htole16(iwm_mvm_fill_probe_req(sc,
			    (struct ieee80211_frame *)cmd->data,
			    vap ? vap->iv_myaddr : ic->ic_macaddr, n_ssids,
			    ssid, ssid_len, NULL, 0,
			    sc->sc_capa_max_probe_len));

	cmd->channel_count
	    = iwm_mvm_scan_fill_channels(sc, cmd, flags, n_ssids, basic_ssid);

	cmd->len = htole16(sizeof(struct iwm_scan_cmd) +
		le16toh(cmd->tx_cmd.len) +
		(cmd->channel_count * sizeof(struct iwm_scan_channel)));
	hcmd.len[0] = le16toh(cmd->len);

	status = IWM_SCAN_RESPONSE_OK;
	ret = iwm_mvm_send_cmd_status(sc, &hcmd, &status);
	if (!ret && status == IWM_SCAN_RESPONSE_OK) {
		IWM_DPRINTF(sc, IWM_DEBUG_SCAN,
		    "Scan request was sent successfully\n");
	} else {
		/*
		 * If the scan failed, it usually means that the FW was unable
		 * to allocate the time events. Warn on it, but maybe we
		 * should try to send the command again with different params.
		 */
		ret = EIO;
	}
	return ret;
}
