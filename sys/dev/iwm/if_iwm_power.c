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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <dev/iwm/if_iwm_power.h>

/*
 * BEGIN mvm/power.c
 */

#define IWM_POWER_KEEP_ALIVE_PERIOD_SEC    25

static int
iwm_mvm_beacon_filter_send_cmd(struct iwm_softc *sc,
	struct iwm_beacon_filter_cmd *cmd)
{
	int ret;

	ret = iwm_mvm_send_cmd_pdu(sc, IWM_REPLY_BEACON_FILTERING_CMD,
	    IWM_CMD_SYNC, sizeof(struct iwm_beacon_filter_cmd), cmd);

	if (!ret) {
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "ba_enable_beacon_abort is: %d\n",
		    le32toh(cmd->ba_enable_beacon_abort));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "ba_escape_timer is: %d\n",
		    le32toh(cmd->ba_escape_timer));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "bf_debug_flag is: %d\n",
		    le32toh(cmd->bf_debug_flag));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "bf_enable_beacon_filter is: %d\n",
		    le32toh(cmd->bf_enable_beacon_filter));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "bf_energy_delta is: %d\n",
		    le32toh(cmd->bf_energy_delta));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "bf_escape_timer is: %d\n",
		    le32toh(cmd->bf_escape_timer));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "bf_roaming_energy_delta is: %d\n",
		    le32toh(cmd->bf_roaming_energy_delta));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "bf_roaming_state is: %d\n",
		    le32toh(cmd->bf_roaming_state));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "bf_temp_threshold is: %d\n",
		    le32toh(cmd->bf_temp_threshold));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "bf_temp_fast_filter is: %d\n",
		    le32toh(cmd->bf_temp_fast_filter));
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "bf_temp_slow_filter is: %d\n",
		    le32toh(cmd->bf_temp_slow_filter));
	}
	return ret;
}

static void
iwm_mvm_beacon_filter_set_cqm_params(struct iwm_softc *sc,
	struct iwm_node *in, struct iwm_beacon_filter_cmd *cmd)
{
	cmd->ba_enable_beacon_abort = htole32(sc->sc_bf.ba_enabled);
}

static int
iwm_mvm_update_beacon_abort(struct iwm_softc *sc, struct iwm_node *in,
	int enable)
{
	struct iwm_beacon_filter_cmd cmd = {
		IWM_BF_CMD_CONFIG_DEFAULTS,
		.bf_enable_beacon_filter = htole32(1),
		.ba_enable_beacon_abort = htole32(enable),
	};

	if (!sc->sc_bf.bf_enabled)
		return 0;

	sc->sc_bf.ba_enabled = enable;
	iwm_mvm_beacon_filter_set_cqm_params(sc, in, &cmd);
	return iwm_mvm_beacon_filter_send_cmd(sc, &cmd);
}

static void
iwm_mvm_power_log(struct iwm_softc *sc, struct iwm_mac_power_cmd *cmd)
{
	IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
	    "Sending power table command on mac id 0x%X for "
	    "power level %d, flags = 0x%X\n",
	    cmd->id_and_color, IWM_POWER_SCHEME_CAM, le16toh(cmd->flags));
	IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
	    "Keep alive = %u sec\n", le16toh(cmd->keep_alive_seconds));

	if (!(cmd->flags & htole16(IWM_POWER_FLAGS_POWER_MANAGEMENT_ENA_MSK))) {
		IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
		    "Disable power management\n");
		return;
	}
	KASSERT(0, ("unhandled power management"));

#if 0
	DPRINTF(mvm, "Rx timeout = %u usec\n",
			le32_to_cpu(cmd->rx_data_timeout));
	DPRINTF(mvm, "Tx timeout = %u usec\n",
			le32_to_cpu(cmd->tx_data_timeout));
	if (cmd->flags & cpu_to_le16(IWM_POWER_FLAGS_SKIP_OVER_DTIM_MSK))
		DPRINTF(mvm, "DTIM periods to skip = %u\n",
				cmd->skip_dtim_periods);
	if (cmd->flags & cpu_to_le16(IWM_POWER_FLAGS_LPRX_ENA_MSK))
		DPRINTF(mvm, "LP RX RSSI threshold = %u\n",
				cmd->lprx_rssi_threshold);
	if (cmd->flags & cpu_to_le16(IWM_POWER_FLAGS_ADVANCE_PM_ENA_MSK)) {
		DPRINTF(mvm, "uAPSD enabled\n");
		DPRINTF(mvm, "Rx timeout (uAPSD) = %u usec\n",
				le32_to_cpu(cmd->rx_data_timeout_uapsd));
		DPRINTF(mvm, "Tx timeout (uAPSD) = %u usec\n",
				le32_to_cpu(cmd->tx_data_timeout_uapsd));
		DPRINTF(mvm, "QNDP TID = %d\n", cmd->qndp_tid);
		DPRINTF(mvm, "ACs flags = 0x%x\n", cmd->uapsd_ac_flags);
		DPRINTF(mvm, "Max SP = %d\n", cmd->uapsd_max_sp);
	}
#endif
}

static void
iwm_mvm_power_build_cmd(struct iwm_softc *sc, struct iwm_node *in,
	struct iwm_mac_power_cmd *cmd)
{
	struct ieee80211_node *ni = &in->in_ni;
	int dtimper, dtimper_msec;
	int keep_alive;
	struct ieee80211com *ic = sc->sc_ic;
	struct ieee80211vap *vap = TAILQ_FIRST(&ic->ic_vaps);

	cmd->id_and_color = htole32(IWM_FW_CMD_ID_AND_COLOR(IWM_DEFAULT_MACID,
	    IWM_DEFAULT_COLOR));
	dtimper = vap->iv_dtim_period ?: 1;

	/*
	 * Regardless of power management state the driver must set
	 * keep alive period. FW will use it for sending keep alive NDPs
	 * immediately after association. Check that keep alive period
	 * is at least 3 * DTIM
	 */
	dtimper_msec = dtimper * ni->ni_intval;
	keep_alive
	    = MAX(3 * dtimper_msec, 1000 * IWM_POWER_KEEP_ALIVE_PERIOD_SEC);
	keep_alive = roundup(keep_alive, 1000) / 1000;
	cmd->keep_alive_seconds = htole16(keep_alive);
}

int
iwm_mvm_power_mac_update_mode(struct iwm_softc *sc, struct iwm_node *in)
{
	int ret;
	int ba_enable;
	struct iwm_mac_power_cmd cmd;

	memset(&cmd, 0, sizeof(cmd));

	iwm_mvm_power_build_cmd(sc, in, &cmd);
	iwm_mvm_power_log(sc, &cmd);

	if ((ret = iwm_mvm_send_cmd_pdu(sc, IWM_MAC_PM_POWER_TABLE,
	    IWM_CMD_SYNC, sizeof(cmd), &cmd)) != 0)
		return ret;

	ba_enable = !!(cmd.flags &
	    htole16(IWM_POWER_FLAGS_POWER_MANAGEMENT_ENA_MSK));
	return iwm_mvm_update_beacon_abort(sc, in, ba_enable);
}

int
iwm_mvm_power_update_device(struct iwm_softc *sc)
{
	struct iwm_device_power_cmd cmd = {
		.flags = htole16(IWM_DEVICE_POWER_FLAGS_POWER_SAVE_ENA_MSK),
	};

	if (!(sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_DEVICE_PS_CMD))
		return 0;

	cmd.flags |= htole16(IWM_DEVICE_POWER_FLAGS_CAM_MSK);
	IWM_DPRINTF(sc, IWM_DEBUG_PWRSAVE | IWM_DEBUG_CMD,
	    "Sending device power command with flags = 0x%X\n", cmd.flags);

	return iwm_mvm_send_cmd_pdu(sc,
	    IWM_POWER_TABLE_CMD, IWM_CMD_SYNC, sizeof(cmd), &cmd);
}

int
iwm_mvm_enable_beacon_filter(struct iwm_softc *sc, struct iwm_node *in)
{
	struct iwm_beacon_filter_cmd cmd = {
		IWM_BF_CMD_CONFIG_DEFAULTS,
		.bf_enable_beacon_filter = htole32(1),
	};
	int ret;

	iwm_mvm_beacon_filter_set_cqm_params(sc, in, &cmd);
	ret = iwm_mvm_beacon_filter_send_cmd(sc, &cmd);

	if (ret == 0)
		sc->sc_bf.bf_enabled = 1;

	return ret;
}

int
iwm_mvm_disable_beacon_filter(struct iwm_softc *sc)
{
	struct iwm_beacon_filter_cmd cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	if ((sc->sc_capaflags & IWM_UCODE_TLV_FLAGS_BF_UPDATED) == 0)
		return 0;

	ret = iwm_mvm_beacon_filter_send_cmd(sc, &cmd);
	if (ret == 0)
		sc->sc_bf.bf_enabled = 0;

	return ret;
}

#if 0
static int
iwm_mvm_update_beacon_filter(struct iwm_softc *sc, struct iwm_node *in)
{
	if (!sc->sc_bf.bf_enabled)
		return 0;

	return iwm_mvm_enable_beacon_filter(sc, in);
}
#endif
