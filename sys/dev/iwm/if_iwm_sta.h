/*-
 * Based on BSD-licensed source modules in the Linux iwlwifi driver,
 * which were used as the reference documentation for this implementation.
 *
 * Driver version we are currently based off of is
 * Linux 4.7.3 (tag id d7f6728f57e3ecbb7ef34eb7d9f564d514775d75)
 *
 ***********************************************************************
 *
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * Copyright(c) 2015 - 2016 Intel Deutschland GmbH
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
 *  Intel Linux Wireless <linuxwifi@intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2012 - 2014 Intel Corporation. All rights reserved.
 * Copyright(c) 2013 - 2014 Intel Mobile Communications GmbH
 * Copyright(c) 2015 - 2016 Intel Deutschland GmbH
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
 *
 *****************************************************************************/


#ifndef __IF_IWM_STA_H__
#define __IF_IWM_STA_H__

/**
 * DOC: station table - introduction
 *
 * The station table is a list of data structure that reprensent the stations.
 * In STA/P2P client mode, the driver will hold one station for the AP/ GO.
 * In GO/AP mode, the driver will have as many stations as associated clients.
 * All these stations are reflected in the fw's station table. The driver
 * keeps the fw's station table up to date with the ADD_STA command. Stations
 * can be removed by the REMOVE_STA command.
 *
 * All the data related to a station is held in the structure %iwl_sta
 * which is embed in the mac80211's %ieee80211_sta (in the drv_priv) area.
 * This data includes the index of the station in the fw, per tid information
 * (sequence numbers, Block-ack state machine, etc...). The stations are
 * created and deleted by the %sta_state callback from %ieee80211_ops.
 *
 * The driver holds a map: %fw_id_to_mac_id that allows to fetch a
 * %ieee80211_sta (and the %iwl_sta embedded into it) based on a fw
 * station index. That way, the driver is able to get the tid related data in
 * O(1) in time sensitive paths (Tx / Tx response / BA notification). These
 * paths are triggered by the fw, and the driver needs to get a pointer to the
 * %ieee80211 structure. This map helps to get that pointer quickly.
 */

/**
 * DOC: station table - locking
 *
 * As stated before, the station is created / deleted by mac80211's %sta_state
 * callback from %ieee80211_ops which can sleep. The next paragraph explains
 * the locking of a single stations, the next ones relates to the station
 * table.
 *
 * The station holds the sequence number per tid. So this data needs to be
 * accessed in the Tx path (which is softIRQ). It also holds the Block-Ack
 * information (the state machine / and the logic that checks if the queues
 * were drained), so it also needs to be accessible from the Tx response flow.
 * In short, the station needs to be access from sleepable context as well as
 * from tasklets, so the station itself needs a spinlock.
 *
 * The writers of %fw_id_to_mac_id map are serialized by the global mutex of
 * the mvm op_mode. This is possible since %sta_state can sleep.
 * The pointers in this map are RCU protected, hence we won't replace the
 * station while we have Tx / Tx response / BA notification running.
 *
 * If a station is deleted while it still has packets in its A-MPDU queues,
 * then the reclaim flow will notice that there is no station in the map for
 * sta_id and it will dump the responses.
 */

/**
 * DOC: station table - internal stations
 *
 * The FW needs a few internal stations that are not reflected in
 * mac80211, such as broadcast station in AP / GO mode, or AUX sta for
 * scanning and P2P device (during the GO negotiation).
 * For these kind of stations we have %iwl_int_sta struct which holds the
 * data relevant for them from both %iwl_sta and %ieee80211_sta.
 * Usually the data for these stations is static, so no locking is required,
 * and no TID data as this is also not needed.
 * One thing to note, is that these stations have an ID in the fw, but not
 * in mac80211. In order to "reserve" them a sta_id in %fw_id_to_mac_id
 * we fill ERR_PTR(EINVAL) in this mapping and all other dereferencing of
 * pointers from this mapping need to check that the value is not error
 * or NULL.
 *
 * Currently there is only one auxiliary station for scanning, initialized
 * on init.
 */

/**
 * DOC: station table - AP Station in STA mode
 *
 * %iwl_vif includes the index of the AP station in the fw's STA table:
 * %ap_sta_id. To get the point to the corresponding %ieee80211_sta,
 * &fw_id_to_mac_id can be used. Due to the way the fw works, we must not remove
 * the AP station from the fw before setting the MAC context as unassociated.
 * Hence, %fw_id_to_mac_id[%ap_sta_id] will be NULLed when the AP station is
 * removed by mac80211, but the station won't be removed in the fw until the
 * VIF is set as unassociated. Then, %ap_sta_id will be invalidated.
 */

/**
 * DOC: station table - Drain vs. Flush
 *
 * Flush means that all the frames in the SCD queue are dumped regardless the
 * station to which they were sent. We do that when we disassociate and before
 * we remove the STA of the AP. The flush can be done synchronously against the
 * fw.
 * Drain means that the fw will drop all the frames sent to a specific station.
 * This is useful when a client (if we are IBSS / GO or AP) disassociates. In
 * that case, we need to drain all the frames for that client from the AC queues
 * that are shared with the other clients. Only then, we can remove the STA in
 * the fw. In order to do so, we track the non-AMPDU packets for each station.
 * If mac80211 removes a STA and if it still has non-AMPDU packets pending in
 * the queues, we mark this station as %EBUSY in %fw_id_to_mac_id, and drop all
 * the frames for this STA (%iwl_rm_sta). When the last frame is dropped
 * (we know about it with its Tx response), we remove the station in fw and set
 * it as %NULL in %fw_id_to_mac_id: this is the purpose of
 * %iwl_sta_drained_wk.
 */

/**
 * DOC: station table - fw restart
 *
 * When the fw asserts, or we have any other issue that requires to reset the
 * driver, we require mac80211 to reconfigure the driver. Since the private
 * data of the stations is embed in mac80211's %ieee80211_sta, that data will
 * not be zeroed and needs to be reinitialized manually.
 * %IWL_STATUS_IN_HW_RESTART is set during restart and that will hint us
 * that we must not allocate a new sta_id but reuse the previous one. This
 * means that the stations being re-added after the reset will have the same
 * place in the fw as before the reset. We do need to zero the %fw_id_to_mac_id
 * map, since the stations aren't in the fw any more. Internal stations that
 * are not added by mac80211 will be re-added in the init flow that is called
 * after the restart: mac80211 call's %iwl_mac_start which calls to
 * %iwl_up.
 */

/**
 * Send the STA info to the FW.
 *
 * @sc: the iwm_softc* to use
 * @sta: the STA
 * @update: this is true if the FW is being updated about a STA it already knows
 *	about. Otherwise (if this is a new STA), this should be false.
 * @flags: if update==true, this marks what is being changed via ORs of values
 *	from enum iwm_sta_modify_flag. Otherwise, this is ignored.
 */
extern	int iwm_sta_send_to_fw(struct iwm_softc *sc, struct iwm_node *in,
				   boolean_t update);
extern	int iwm_add_sta(struct iwm_softc *sc, struct iwm_node *in);
extern	int iwm_update_sta(struct iwm_softc *sc, struct iwm_node *in);
extern	int iwm_rm_sta(struct iwm_softc *sc, struct ieee80211vap *vap,
			   boolean_t is_assoc);
extern	int iwm_rm_sta_id(struct iwm_softc *sc, struct ieee80211vap *vap);

extern	int iwm_add_aux_sta(struct iwm_softc *sc);
extern	void iwm_del_aux_sta(struct iwm_softc *sc);

extern	int iwm_drain_sta(struct iwm_softc *sc, struct iwm_vap *ivp,
			      boolean_t drain);

#endif /* __IF_IWM_STA_H__ */
