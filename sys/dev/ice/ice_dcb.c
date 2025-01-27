/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "ice_common.h"
#include "ice_sched.h"
#include "ice_dcb.h"

/**
 * ice_aq_get_lldp_mib
 * @hw: pointer to the HW struct
 * @bridge_type: type of bridge requested
 * @mib_type: Local, Remote or both Local and Remote MIBs
 * @buf: pointer to the caller-supplied buffer to store the MIB block
 * @buf_size: size of the buffer (in bytes)
 * @local_len: length of the returned Local LLDP MIB
 * @remote_len: length of the returned Remote LLDP MIB
 * @cd: pointer to command details structure or NULL
 *
 * Requests the complete LLDP MIB (entire packet). (0x0A00)
 */
int
ice_aq_get_lldp_mib(struct ice_hw *hw, u8 bridge_type, u8 mib_type, void *buf,
		    u16 buf_size, u16 *local_len, u16 *remote_len,
		    struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_get_mib *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.lldp_get_mib;

	if (buf_size == 0 || !buf)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_get_mib);

	cmd->type = mib_type & ICE_AQ_LLDP_MIB_TYPE_M;
	cmd->type |= (bridge_type << ICE_AQ_LLDP_BRID_TYPE_S) &
		ICE_AQ_LLDP_BRID_TYPE_M;

	desc.datalen = CPU_TO_LE16(buf_size);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status) {
		if (local_len)
			*local_len = LE16_TO_CPU(cmd->local_len);
		if (remote_len)
			*remote_len = LE16_TO_CPU(cmd->remote_len);
	}

	return status;
}

/**
 * ice_aq_cfg_lldp_mib_change
 * @hw: pointer to the HW struct
 * @ena_update: Enable or Disable event posting
 * @cd: pointer to command details structure or NULL
 *
 * Enable or Disable posting of an event on ARQ when LLDP MIB
 * associated with the interface changes (0x0A01)
 */
int
ice_aq_cfg_lldp_mib_change(struct ice_hw *hw, bool ena_update,
			   struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_set_mib_change *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_set_event;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_set_mib_change);

	if (!ena_update)
		cmd->command |= ICE_AQ_LLDP_MIB_UPDATE_DIS;
	else
		cmd->command |= ICE_AQ_LLDP_MIB_PENDING_ENABLE <<
				ICE_AQ_LLDP_MIB_PENDING_S;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_add_delete_lldp_tlv
 * @hw: pointer to the HW struct
 * @bridge_type: type of bridge
 * @add_lldp_tlv: add (true) or delete (false) TLV
 * @buf: buffer with TLV to add or delete
 * @buf_size: length of the buffer
 * @tlv_len: length of the TLV to be added/deleted
 * @mib_len: length of the LLDP MIB returned in response
 * @cd: pointer to command details structure or NULL
 *
 * (Add tlv)
 * Add the specified TLV to LLDP Local MIB for the given bridge type,
 * it is responsibility of the caller to make sure that the TLV is not
 * already present in the LLDPDU.
 * In return firmware will write the complete LLDP MIB with the newly
 * added TLV in the response buffer. (0x0A02)
 *
 * (Delete tlv)
 * Delete the specified TLV from LLDP Local MIB for the given bridge type.
 * The firmware places the entire LLDP MIB in the response buffer. (0x0A04)
 */
int
ice_aq_add_delete_lldp_tlv(struct ice_hw *hw, u8 bridge_type, bool add_lldp_tlv,
			   void *buf, u16 buf_size, u16 tlv_len, u16 *mib_len,
			   struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_add_delete_tlv *cmd;
	struct ice_aq_desc desc;
	int status;

	if (tlv_len == 0)
		return ICE_ERR_PARAM;

	cmd = &desc.params.lldp_add_delete_tlv;

	if (add_lldp_tlv)
		ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_add_tlv);
	else
		ice_fill_dflt_direct_cmd_desc(&desc,
					      ice_aqc_opc_lldp_delete_tlv);

	desc.flags |= CPU_TO_LE16((u16)(ICE_AQ_FLAG_RD));

	cmd->type = ((bridge_type << ICE_AQ_LLDP_BRID_TYPE_S) &
		     ICE_AQ_LLDP_BRID_TYPE_M);
	cmd->len = CPU_TO_LE16(tlv_len);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status && mib_len)
		*mib_len = LE16_TO_CPU(desc.datalen);

	return status;
}

/**
 * ice_aq_update_lldp_tlv
 * @hw: pointer to the HW struct
 * @bridge_type: type of bridge
 * @buf: buffer with TLV to update
 * @buf_size: size of the buffer holding original and updated TLVs
 * @old_len: Length of the Original TLV
 * @new_len: Length of the Updated TLV
 * @offset: offset of the updated TLV in the buff
 * @mib_len: length of the returned LLDP MIB
 * @cd: pointer to command details structure or NULL
 *
 * Update the specified TLV to the LLDP Local MIB for the given bridge type.
 * Firmware will place the complete LLDP MIB in response buffer with the
 * updated TLV. (0x0A03)
 */
int
ice_aq_update_lldp_tlv(struct ice_hw *hw, u8 bridge_type, void *buf,
		       u16 buf_size, u16 old_len, u16 new_len, u16 offset,
		       u16 *mib_len, struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_update_tlv *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.lldp_update_tlv;

	if (offset == 0 || old_len == 0 || new_len == 0)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_update_tlv);

	desc.flags |= CPU_TO_LE16((u16)(ICE_AQ_FLAG_RD));

	cmd->type = ((bridge_type << ICE_AQ_LLDP_BRID_TYPE_S) &
		     ICE_AQ_LLDP_BRID_TYPE_M);
	cmd->old_len = CPU_TO_LE16(old_len);
	cmd->new_offset = CPU_TO_LE16(offset);
	cmd->new_len = CPU_TO_LE16(new_len);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status && mib_len)
		*mib_len = LE16_TO_CPU(desc.datalen);

	return status;
}

/**
 * ice_aq_stop_lldp
 * @hw: pointer to the HW struct
 * @shutdown_lldp_agent: True if LLDP Agent needs to be Shutdown
 *			 False if LLDP Agent needs to be Stopped
 * @persist: True if Stop/Shutdown of LLDP Agent needs to be persistent across
 *	     reboots
 * @cd: pointer to command details structure or NULL
 *
 * Stop or Shutdown the embedded LLDP Agent (0x0A05)
 */
int
ice_aq_stop_lldp(struct ice_hw *hw, bool shutdown_lldp_agent, bool persist,
		 struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_stop *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_stop;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_stop);

	if (shutdown_lldp_agent)
		cmd->command |= ICE_AQ_LLDP_AGENT_SHUTDOWN;

	if (persist)
		cmd->command |= ICE_AQ_LLDP_AGENT_PERSIST_DIS;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_start_lldp
 * @hw: pointer to the HW struct
 * @persist: True if Start of LLDP Agent needs to be persistent across reboots
 * @cd: pointer to command details structure or NULL
 *
 * Start the embedded LLDP Agent on all ports. (0x0A06)
 */
int
ice_aq_start_lldp(struct ice_hw *hw, bool persist, struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_start *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_start;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_start);

	cmd->command = ICE_AQ_LLDP_AGENT_START;

	if (persist)
		cmd->command |= ICE_AQ_LLDP_AGENT_PERSIST_ENA;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_get_dcbx_status
 * @hw: pointer to the HW struct
 *
 * Get the DCBX status from the Firmware
 */
u8 ice_get_dcbx_status(struct ice_hw *hw)
{
	u32 reg;

	reg = rd32(hw, PRTDCB_GENS);
	return (u8)((reg & PRTDCB_GENS_DCBX_STATUS_M) >>
		    PRTDCB_GENS_DCBX_STATUS_S);
}

/**
 * ice_parse_ieee_ets_common_tlv
 * @buf: Data buffer to be parsed for ETS CFG/REC data
 * @ets_cfg: Container to store parsed data
 *
 * Parses the common data of IEEE 802.1Qaz ETS CFG/REC TLV
 */
static void
ice_parse_ieee_ets_common_tlv(u8 *buf, struct ice_dcb_ets_cfg *ets_cfg)
{
	u8 offset = 0;
	int i;

	/* Priority Assignment Table (4 octets)
	 * Octets:|    1    |    2    |    3    |    4    |
	 *        -----------------------------------------
	 *        |pri0|pri1|pri2|pri3|pri4|pri5|pri6|pri7|
	 *        -----------------------------------------
	 *   Bits:|7  4|3  0|7  4|3  0|7  4|3  0|7  4|3  0|
	 *        -----------------------------------------
	 */
	for (i = 0; i < 4; i++) {
		ets_cfg->prio_table[i * 2] =
			((buf[offset] & ICE_IEEE_ETS_PRIO_1_M) >>
			 ICE_IEEE_ETS_PRIO_1_S);
		ets_cfg->prio_table[i * 2 + 1] =
			((buf[offset] & ICE_IEEE_ETS_PRIO_0_M) >>
			 ICE_IEEE_ETS_PRIO_0_S);
		offset++;
	}

	/* TC Bandwidth Table (8 octets)
	 * Octets:| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
	 *        ---------------------------------
	 *        |tc0|tc1|tc2|tc3|tc4|tc5|tc6|tc7|
	 *        ---------------------------------
	 *
	 * TSA Assignment Table (8 octets)
	 * Octets:| 9 | 10| 11| 12| 13| 14| 15| 16|
	 *        ---------------------------------
	 *        |tc0|tc1|tc2|tc3|tc4|tc5|tc6|tc7|
	 *        ---------------------------------
	 */
	ice_for_each_traffic_class(i) {
		ets_cfg->tcbwtable[i] = buf[offset];
		ets_cfg->tsatable[i] = buf[ICE_MAX_TRAFFIC_CLASS + offset++];
	}
}

/**
 * ice_parse_ieee_etscfg_tlv
 * @tlv: IEEE 802.1Qaz ETS CFG TLV
 * @dcbcfg: Local store to update ETS CFG data
 *
 * Parses IEEE 802.1Qaz ETS CFG TLV
 */
static void
ice_parse_ieee_etscfg_tlv(struct ice_lldp_org_tlv *tlv,
			  struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etscfg;
	u8 *buf = tlv->tlvinfo;

	/* First Octet post subtype
	 * --------------------------
	 * |will-|CBS  | Re-  | Max |
	 * |ing  |     |served| TCs |
	 * --------------------------
	 * |1bit | 1bit|3 bits|3bits|
	 */
	etscfg = &dcbcfg->etscfg;
	etscfg->willing = ((buf[0] & ICE_IEEE_ETS_WILLING_M) >>
			   ICE_IEEE_ETS_WILLING_S);
	etscfg->cbs = ((buf[0] & ICE_IEEE_ETS_CBS_M) >> ICE_IEEE_ETS_CBS_S);
	etscfg->maxtcs = ((buf[0] & ICE_IEEE_ETS_MAXTC_M) >>
			  ICE_IEEE_ETS_MAXTC_S);

	/* Begin parsing at Priority Assignment Table (offset 1 in buf) */
	ice_parse_ieee_ets_common_tlv(&buf[1], etscfg);
}

/**
 * ice_parse_ieee_etsrec_tlv
 * @tlv: IEEE 802.1Qaz ETS REC TLV
 * @dcbcfg: Local store to update ETS REC data
 *
 * Parses IEEE 802.1Qaz ETS REC TLV
 */
static void
ice_parse_ieee_etsrec_tlv(struct ice_lldp_org_tlv *tlv,
			  struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;

	/* Begin parsing at Priority Assignment Table (offset 1 in buf) */
	ice_parse_ieee_ets_common_tlv(&buf[1], &dcbcfg->etsrec);
}

/**
 * ice_parse_ieee_pfccfg_tlv
 * @tlv: IEEE 802.1Qaz PFC CFG TLV
 * @dcbcfg: Local store to update PFC CFG data
 *
 * Parses IEEE 802.1Qaz PFC CFG TLV
 */
static void
ice_parse_ieee_pfccfg_tlv(struct ice_lldp_org_tlv *tlv,
			  struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;

	/* ----------------------------------------
	 * |will-|MBC  | Re-  | PFC |  PFC Enable  |
	 * |ing  |     |served| cap |              |
	 * -----------------------------------------
	 * |1bit | 1bit|2 bits|4bits| 1 octet      |
	 */
	dcbcfg->pfc.willing = ((buf[0] & ICE_IEEE_PFC_WILLING_M) >>
			       ICE_IEEE_PFC_WILLING_S);
	dcbcfg->pfc.mbc = ((buf[0] & ICE_IEEE_PFC_MBC_M) >> ICE_IEEE_PFC_MBC_S);
	dcbcfg->pfc.pfccap = ((buf[0] & ICE_IEEE_PFC_CAP_M) >>
			      ICE_IEEE_PFC_CAP_S);
	dcbcfg->pfc.pfcena = buf[1];
}

/**
 * ice_parse_ieee_app_tlv
 * @tlv: IEEE 802.1Qaz APP TLV
 * @dcbcfg: Local store to update APP PRIO data
 *
 * Parses IEEE 802.1Qaz APP PRIO TLV
 */
static void
ice_parse_ieee_app_tlv(struct ice_lldp_org_tlv *tlv,
		       struct ice_dcbx_cfg *dcbcfg)
{
	u16 offset = 0;
	u16 typelen;
	int i = 0;
	u16 len;
	u8 *buf;

	typelen = NTOHS(tlv->typelen);
	len = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);
	buf = tlv->tlvinfo;

	/* Removing sizeof(ouisubtype) and reserved byte from len.
	 * Remaining len div 3 is number of APP TLVs.
	 */
	len -= (sizeof(tlv->ouisubtype) + 1);

	/* Move offset to App Priority Table */
	offset++;

	/* Application Priority Table (3 octets)
	 * Octets:|         1          |    2    |    3    |
	 *        -----------------------------------------
	 *        |Priority|Rsrvd| Sel |    Protocol ID    |
	 *        -----------------------------------------
	 *   Bits:|23    21|20 19|18 16|15                0|
	 *        -----------------------------------------
	 */
	while (offset < len) {
		dcbcfg->app[i].priority = ((buf[offset] &
					    ICE_IEEE_APP_PRIO_M) >>
					   ICE_IEEE_APP_PRIO_S);
		dcbcfg->app[i].selector = ((buf[offset] &
					    ICE_IEEE_APP_SEL_M) >>
					   ICE_IEEE_APP_SEL_S);
		dcbcfg->app[i].prot_id = (buf[offset + 1] << 0x8) |
			buf[offset + 2];
		/* Move to next app */
		offset += 3;
		i++;
		if (i >= ICE_DCBX_MAX_APPS)
			break;
	}

	dcbcfg->numapps = i;
}

/**
 * ice_parse_ieee_tlv
 * @tlv: IEEE 802.1Qaz TLV
 * @dcbcfg: Local store to update ETS REC data
 *
 * Get the TLV subtype and send it to parsing function
 * based on the subtype value
 */
static void
ice_parse_ieee_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u32 ouisubtype;
	u8 subtype;

	ouisubtype = NTOHL(tlv->ouisubtype);
	subtype = (u8)((ouisubtype & ICE_LLDP_TLV_SUBTYPE_M) >>
		       ICE_LLDP_TLV_SUBTYPE_S);
	switch (subtype) {
	case ICE_IEEE_SUBTYPE_ETS_CFG:
		ice_parse_ieee_etscfg_tlv(tlv, dcbcfg);
		break;
	case ICE_IEEE_SUBTYPE_ETS_REC:
		ice_parse_ieee_etsrec_tlv(tlv, dcbcfg);
		break;
	case ICE_IEEE_SUBTYPE_PFC_CFG:
		ice_parse_ieee_pfccfg_tlv(tlv, dcbcfg);
		break;
	case ICE_IEEE_SUBTYPE_APP_PRI:
		ice_parse_ieee_app_tlv(tlv, dcbcfg);
		break;
	default:
		break;
	}
}

/**
 * ice_parse_cee_pgcfg_tlv
 * @tlv: CEE DCBX PG CFG TLV
 * @dcbcfg: Local store to update ETS CFG data
 *
 * Parses CEE DCBX PG CFG TLV
 */
static void
ice_parse_cee_pgcfg_tlv(struct ice_cee_feat_tlv *tlv,
			struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etscfg;
	u8 *buf = tlv->tlvinfo;
	u16 offset = 0;
	int i;

	etscfg = &dcbcfg->etscfg;

	if (tlv->en_will_err & ICE_CEE_FEAT_TLV_WILLING_M)
		etscfg->willing = 1;

	etscfg->cbs = 0;
	/* Priority Group Table (4 octets)
	 * Octets:|    1    |    2    |    3    |    4    |
	 *        -----------------------------------------
	 *        |pri0|pri1|pri2|pri3|pri4|pri5|pri6|pri7|
	 *        -----------------------------------------
	 *   Bits:|7  4|3  0|7  4|3  0|7  4|3  0|7  4|3  0|
	 *        -----------------------------------------
	 */
	for (i = 0; i < 4; i++) {
		etscfg->prio_table[i * 2] =
			((buf[offset] & ICE_CEE_PGID_PRIO_1_M) >>
			 ICE_CEE_PGID_PRIO_1_S);
		etscfg->prio_table[i * 2 + 1] =
			((buf[offset] & ICE_CEE_PGID_PRIO_0_M) >>
			 ICE_CEE_PGID_PRIO_0_S);
		offset++;
	}

	/* PG Percentage Table (8 octets)
	 * Octets:| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
	 *        ---------------------------------
	 *        |pg0|pg1|pg2|pg3|pg4|pg5|pg6|pg7|
	 *        ---------------------------------
	 */
	ice_for_each_traffic_class(i) {
		etscfg->tcbwtable[i] = buf[offset++];

		if (etscfg->prio_table[i] == ICE_CEE_PGID_STRICT)
			dcbcfg->etscfg.tsatable[i] = ICE_IEEE_TSA_STRICT;
		else
			dcbcfg->etscfg.tsatable[i] = ICE_IEEE_TSA_ETS;
	}

	/* Number of TCs supported (1 octet) */
	etscfg->maxtcs = buf[offset];
}

/**
 * ice_parse_cee_pfccfg_tlv
 * @tlv: CEE DCBX PFC CFG TLV
 * @dcbcfg: Local store to update PFC CFG data
 *
 * Parses CEE DCBX PFC CFG TLV
 */
static void
ice_parse_cee_pfccfg_tlv(struct ice_cee_feat_tlv *tlv,
			 struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;

	if (tlv->en_will_err & ICE_CEE_FEAT_TLV_WILLING_M)
		dcbcfg->pfc.willing = 1;

	/* ------------------------
	 * | PFC Enable | PFC TCs |
	 * ------------------------
	 * | 1 octet    | 1 octet |
	 */
	dcbcfg->pfc.pfcena = buf[0];
	dcbcfg->pfc.pfccap = buf[1];
}

/**
 * ice_parse_cee_app_tlv
 * @tlv: CEE DCBX APP TLV
 * @dcbcfg: Local store to update APP PRIO data
 *
 * Parses CEE DCBX APP PRIO TLV
 */
static void
ice_parse_cee_app_tlv(struct ice_cee_feat_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u16 len, typelen, offset = 0;
	struct ice_cee_app_prio *app;
	u8 i;

	typelen = NTOHS(tlv->hdr.typelen);
	len = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);

	dcbcfg->numapps = len / sizeof(*app);
	if (!dcbcfg->numapps)
		return;
	if (dcbcfg->numapps > ICE_DCBX_MAX_APPS)
		dcbcfg->numapps = ICE_DCBX_MAX_APPS;

	for (i = 0; i < dcbcfg->numapps; i++) {
		u8 up, selector;

		app = (struct ice_cee_app_prio *)(tlv->tlvinfo + offset);
		for (up = 0; up < ICE_MAX_USER_PRIORITY; up++)
			if (app->prio_map & BIT(up))
				break;

		dcbcfg->app[i].priority = up;

		/* Get Selector from lower 2 bits, and convert to IEEE */
		selector = (app->upper_oui_sel & ICE_CEE_APP_SELECTOR_M);
		switch (selector) {
		case ICE_CEE_APP_SEL_ETHTYPE:
			dcbcfg->app[i].selector = ICE_APP_SEL_ETHTYPE;
			break;
		case ICE_CEE_APP_SEL_TCPIP:
			dcbcfg->app[i].selector = ICE_APP_SEL_TCPIP;
			break;
		default:
			/* Keep selector as it is for unknown types */
			dcbcfg->app[i].selector = selector;
		}

		dcbcfg->app[i].prot_id = NTOHS(app->protocol);
		/* Move to next app */
		offset += sizeof(*app);
	}
}

/**
 * ice_parse_cee_tlv
 * @tlv: CEE DCBX TLV
 * @dcbcfg: Local store to update DCBX config data
 *
 * Get the TLV subtype and send it to parsing function
 * based on the subtype value
 */
static void
ice_parse_cee_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_cee_feat_tlv *sub_tlv;
	u8 subtype, feat_tlv_count = 0;
	u16 len, tlvlen, typelen;
	u32 ouisubtype;

	ouisubtype = NTOHL(tlv->ouisubtype);
	subtype = (u8)((ouisubtype & ICE_LLDP_TLV_SUBTYPE_M) >>
		       ICE_LLDP_TLV_SUBTYPE_S);
	/* Return if not CEE DCBX */
	if (subtype != ICE_CEE_DCBX_TYPE)
		return;

	typelen = NTOHS(tlv->typelen);
	tlvlen = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);
	len = sizeof(tlv->typelen) + sizeof(ouisubtype) +
		sizeof(struct ice_cee_ctrl_tlv);
	/* Return if no CEE DCBX Feature TLVs */
	if (tlvlen <= len)
		return;

	sub_tlv = (struct ice_cee_feat_tlv *)((char *)tlv + len);
	while (feat_tlv_count < ICE_CEE_MAX_FEAT_TYPE) {
		u16 sublen;

		typelen = NTOHS(sub_tlv->hdr.typelen);
		sublen = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);
		subtype = (u8)((typelen & ICE_LLDP_TLV_TYPE_M) >>
			       ICE_LLDP_TLV_TYPE_S);
		switch (subtype) {
		case ICE_CEE_SUBTYPE_PG_CFG:
			ice_parse_cee_pgcfg_tlv(sub_tlv, dcbcfg);
			break;
		case ICE_CEE_SUBTYPE_PFC_CFG:
			ice_parse_cee_pfccfg_tlv(sub_tlv, dcbcfg);
			break;
		case ICE_CEE_SUBTYPE_APP_PRI:
			ice_parse_cee_app_tlv(sub_tlv, dcbcfg);
			break;
		default:
			return;	/* Invalid Sub-type return */
		}
		feat_tlv_count++;
		/* Move to next sub TLV */
		sub_tlv = (struct ice_cee_feat_tlv *)
			  ((char *)sub_tlv + sizeof(sub_tlv->hdr.typelen) +
			   sublen);
	}
}

/**
 * ice_parse_org_tlv
 * @tlv: Organization specific TLV
 * @dcbcfg: Local store to update ETS REC data
 *
 * Currently only IEEE 802.1Qaz TLV is supported, all others
 * will be returned
 */
static void
ice_parse_org_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u32 ouisubtype;
	u32 oui;

	ouisubtype = NTOHL(tlv->ouisubtype);
	oui = ((ouisubtype & ICE_LLDP_TLV_OUI_M) >> ICE_LLDP_TLV_OUI_S);
	switch (oui) {
	case ICE_IEEE_8021QAZ_OUI:
		ice_parse_ieee_tlv(tlv, dcbcfg);
		break;
	case ICE_CEE_DCBX_OUI:
		ice_parse_cee_tlv(tlv, dcbcfg);
		break;
	default:
		break;
	}
}

/**
 * ice_lldp_to_dcb_cfg
 * @lldpmib: LLDPDU to be parsed
 * @dcbcfg: store for LLDPDU data
 *
 * Parse DCB configuration from the LLDPDU
 */
int ice_lldp_to_dcb_cfg(u8 *lldpmib, struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_lldp_org_tlv *tlv;
	u16 offset = 0;
	int ret = 0;
	u16 typelen;
	u16 type;
	u16 len;

	if (!lldpmib || !dcbcfg)
		return ICE_ERR_PARAM;

	/* set to the start of LLDPDU */
	lldpmib += ETH_HEADER_LEN;
	tlv = (struct ice_lldp_org_tlv *)lldpmib;
	while (1) {
		typelen = NTOHS(tlv->typelen);
		type = ((typelen & ICE_LLDP_TLV_TYPE_M) >> ICE_LLDP_TLV_TYPE_S);
		len = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);
		offset += sizeof(typelen) + len;

		/* END TLV or beyond LLDPDU size */
		if (type == ICE_TLV_TYPE_END || offset > ICE_LLDPDU_SIZE)
			break;

		switch (type) {
		case ICE_TLV_TYPE_ORG:
			ice_parse_org_tlv(tlv, dcbcfg);
			break;
		default:
			break;
		}

		/* Move to next TLV */
		tlv = (struct ice_lldp_org_tlv *)
		      ((char *)tlv + sizeof(tlv->typelen) + len);
	}

	return ret;
}

/**
 * ice_aq_get_dcb_cfg
 * @hw: pointer to the HW struct
 * @mib_type: MIB type for the query
 * @bridgetype: bridge type for the query (remote)
 * @dcbcfg: store for LLDPDU data
 *
 * Query DCB configuration from the firmware
 */
int
ice_aq_get_dcb_cfg(struct ice_hw *hw, u8 mib_type, u8 bridgetype,
		   struct ice_dcbx_cfg *dcbcfg)
{
	u8 *lldpmib;
	int ret;

	/* Allocate the LLDPDU */
	lldpmib = (u8 *)ice_malloc(hw, ICE_LLDPDU_SIZE);
	if (!lldpmib)
		return ICE_ERR_NO_MEMORY;

	ret = ice_aq_get_lldp_mib(hw, bridgetype, mib_type, (void *)lldpmib,
				  ICE_LLDPDU_SIZE, NULL, NULL, NULL);

	if (!ret)
		/* Parse LLDP MIB to get DCB configuration */
		ret = ice_lldp_to_dcb_cfg(lldpmib, dcbcfg);

	ice_free(hw, lldpmib);

	return ret;
}

/**
 * ice_aq_dcb_ignore_pfc - Ignore PFC for given TCs
 * @hw: pointer to the HW struct
 * @tcmap: TC map for request/release any ignore PFC condition
 * @request: request (true) or release (false) ignore PFC condition
 * @tcmap_ret: return TCs for which PFC is currently ignored
 * @cd: pointer to command details structure or NULL
 *
 * This sends out request/release to ignore PFC condition for a TC.
 * It will return the TCs for which PFC is currently ignored. (0x0301)
 */
int
ice_aq_dcb_ignore_pfc(struct ice_hw *hw, u8 tcmap, bool request, u8 *tcmap_ret,
		      struct ice_sq_cd *cd)
{
	struct ice_aqc_pfc_ignore *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.pfc_ignore;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_pfc_ignore);

	if (request)
		cmd->cmd_flags = ICE_AQC_PFC_IGNORE_SET;

	cmd->tc_bitmap = tcmap;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);

	if (!status && tcmap_ret)
		*tcmap_ret = cmd->tc_bitmap;

	return status;
}

/**
 * ice_aq_start_stop_dcbx - Start/Stop DCBX service in FW
 * @hw: pointer to the HW struct
 * @start_dcbx_agent: True if DCBX Agent needs to be started
 *		      False if DCBX Agent needs to be stopped
 * @dcbx_agent_status: FW indicates back the DCBX agent status
 *		       True if DCBX Agent is active
 *		       False if DCBX Agent is stopped
 * @cd: pointer to command details structure or NULL
 *
 * Start/Stop the embedded dcbx Agent. In case that this wrapper function
 * returns 0, caller will need to check if FW returns back the same
 * value as stated in dcbx_agent_status, and react accordingly. (0x0A09)
 */
int
ice_aq_start_stop_dcbx(struct ice_hw *hw, bool start_dcbx_agent,
		       bool *dcbx_agent_status, struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_stop_start_specific_agent *cmd;
	enum ice_adminq_opc opcode;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.lldp_agent_ctrl;

	opcode = ice_aqc_opc_lldp_stop_start_specific_agent;

	ice_fill_dflt_direct_cmd_desc(&desc, opcode);

	if (start_dcbx_agent)
		cmd->command = ICE_AQC_START_STOP_AGENT_START_DCBX;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);

	*dcbx_agent_status = false;

	if (!status &&
	    cmd->command == ICE_AQC_START_STOP_AGENT_START_DCBX)
		*dcbx_agent_status = true;

	return status;
}

/**
 * ice_aq_get_cee_dcb_cfg
 * @hw: pointer to the HW struct
 * @buff: response buffer that stores CEE operational configuration
 * @cd: pointer to command details structure or NULL
 *
 * Get CEE DCBX mode operational configuration from firmware (0x0A07)
 */
int
ice_aq_get_cee_dcb_cfg(struct ice_hw *hw,
		       struct ice_aqc_get_cee_dcb_cfg_resp *buff,
		       struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_cee_dcb_cfg);

	return ice_aq_send_cmd(hw, &desc, (void *)buff, sizeof(*buff), cd);
}

/**
 * ice_aq_query_pfc_mode - Query PFC mode
 * @hw: pointer to the HW struct
 * @pfcmode_ret: Return PFC mode
 * @cd: pointer to command details structure or NULL
 *
 * This will return an indication if DSCP-based PFC or VLAN-based PFC
 * is enabled. (0x0302)
 */
int
ice_aq_query_pfc_mode(struct ice_hw *hw, u8 *pfcmode_ret, struct ice_sq_cd *cd)
{
	struct ice_aqc_set_query_pfc_mode *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.set_query_pfc_mode;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_query_pfc_mode);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);

	if (!status)
		*pfcmode_ret = cmd->pfc_mode;

	return status;
}

/**
 * ice_aq_set_pfc_mode - Set PFC mode
 * @hw: pointer to the HW struct
 * @pfc_mode: value of PFC mode to set
 * @cd: pointer to command details structure or NULL
 *
 * This AQ call configures the PFC mdoe to DSCP-based PFC mode or VLAN
 * -based PFC (0x0303)
 */
int
ice_aq_set_pfc_mode(struct ice_hw *hw, u8 pfc_mode, struct ice_sq_cd *cd)
{
	struct ice_aqc_set_query_pfc_mode *cmd;
	struct ice_aq_desc desc;
	int status;

	if (pfc_mode > ICE_AQC_PFC_DSCP_BASED_PFC)
		return ICE_ERR_PARAM;

	cmd = &desc.params.set_query_pfc_mode;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_pfc_mode);

	cmd->pfc_mode = pfc_mode;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (status)
		return status;

	/* FW will write the PFC mode set back into cmd->pfc_mode, but if DCB is
	 * disabled, FW will write back 0 to cmd->pfc_mode. After the AQ has
	 * been executed, check if cmd->pfc_mode is what was requested. If not,
	 * return an error.
	 */
	if (cmd->pfc_mode != pfc_mode)
		return ICE_ERR_NOT_SUPPORTED;

	return 0;
}

/**
 * ice_aq_set_dcb_parameters - Set DCB parameters
 * @hw: pointer to the HW struct
 * @dcb_enable: True if DCB configuration needs to be applied
 * @cd: pointer to command details structure or NULL
 *
 * This AQ command will tell FW if it will apply or not apply the default DCB
 * configuration when link up (0x0306).
 */
int
ice_aq_set_dcb_parameters(struct ice_hw *hw, bool dcb_enable,
			  struct ice_sq_cd *cd)
{
	struct ice_aqc_set_dcb_params *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_dcb_params;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_dcb_params);

	cmd->valid_flags = ICE_AQC_LINK_UP_DCB_CFG_VALID;
	if (dcb_enable)
		cmd->cmd_flags = ICE_AQC_LINK_UP_DCB_CFG;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_cee_to_dcb_cfg
 * @cee_cfg: pointer to CEE configuration struct
 * @pi: port information structure
 *
 * Convert CEE configuration from firmware to DCB configuration
 */
static void
ice_cee_to_dcb_cfg(struct ice_aqc_get_cee_dcb_cfg_resp *cee_cfg,
		   struct ice_port_info *pi)
{
	u32 status, tlv_status = LE32_TO_CPU(cee_cfg->tlv_status);
	u32 ice_aqc_cee_status_mask, ice_aqc_cee_status_shift;
	u8 i, j, err, sync, oper, app_index, ice_app_sel_type;
	u16 app_prio = LE16_TO_CPU(cee_cfg->oper_app_prio);
	u16 ice_aqc_cee_app_mask, ice_aqc_cee_app_shift;
	struct ice_dcbx_cfg *cmp_dcbcfg, *dcbcfg;
	u16 ice_app_prot_id_type;

	dcbcfg = &pi->qos_cfg.local_dcbx_cfg;
	dcbcfg->dcbx_mode = ICE_DCBX_MODE_CEE;
	dcbcfg->tlv_status = tlv_status;

	/* CEE PG data */
	dcbcfg->etscfg.maxtcs = cee_cfg->oper_num_tc;

	/* Note that the FW creates the oper_prio_tc nibbles reversed
	 * from those in the CEE Priority Group sub-TLV.
	 */
	for (i = 0; i < ICE_MAX_TRAFFIC_CLASS / 2; i++) {
		dcbcfg->etscfg.prio_table[i * 2] =
			((cee_cfg->oper_prio_tc[i] & ICE_CEE_PGID_PRIO_0_M) >>
			 ICE_CEE_PGID_PRIO_0_S);
		dcbcfg->etscfg.prio_table[i * 2 + 1] =
			((cee_cfg->oper_prio_tc[i] & ICE_CEE_PGID_PRIO_1_M) >>
			 ICE_CEE_PGID_PRIO_1_S);
	}

	ice_for_each_traffic_class(i) {
		dcbcfg->etscfg.tcbwtable[i] = cee_cfg->oper_tc_bw[i];

		if (dcbcfg->etscfg.prio_table[i] == ICE_CEE_PGID_STRICT) {
			/* Map it to next empty TC */
			dcbcfg->etscfg.prio_table[i] = cee_cfg->oper_num_tc - 1;
			dcbcfg->etscfg.tsatable[i] = ICE_IEEE_TSA_STRICT;
		} else {
			dcbcfg->etscfg.tsatable[i] = ICE_IEEE_TSA_ETS;
		}
	}

	/* CEE PFC data */
	dcbcfg->pfc.pfcena = cee_cfg->oper_pfc_en;
	dcbcfg->pfc.pfccap = ICE_MAX_TRAFFIC_CLASS;

	/* CEE APP TLV data */
	if (dcbcfg->app_mode == ICE_DCBX_APPS_NON_WILLING)
		cmp_dcbcfg = &pi->qos_cfg.desired_dcbx_cfg;
	else
		cmp_dcbcfg = &pi->qos_cfg.remote_dcbx_cfg;

	app_index = 0;
	for (i = 0; i < 3; i++) {
		if (i == 0) {
			/* FCoE APP */
			ice_aqc_cee_status_mask = ICE_AQC_CEE_FCOE_STATUS_M;
			ice_aqc_cee_status_shift = ICE_AQC_CEE_FCOE_STATUS_S;
			ice_aqc_cee_app_mask = ICE_AQC_CEE_APP_FCOE_M;
			ice_aqc_cee_app_shift = ICE_AQC_CEE_APP_FCOE_S;
			ice_app_sel_type = ICE_APP_SEL_ETHTYPE;
			ice_app_prot_id_type = ICE_APP_PROT_ID_FCOE;
		} else if (i == 1) {
			/* iSCSI APP */
			ice_aqc_cee_status_mask = ICE_AQC_CEE_ISCSI_STATUS_M;
			ice_aqc_cee_status_shift = ICE_AQC_CEE_ISCSI_STATUS_S;
			ice_aqc_cee_app_mask = ICE_AQC_CEE_APP_ISCSI_M;
			ice_aqc_cee_app_shift = ICE_AQC_CEE_APP_ISCSI_S;
			ice_app_sel_type = ICE_APP_SEL_TCPIP;
			ice_app_prot_id_type = ICE_APP_PROT_ID_ISCSI;

			for (j = 0; j < cmp_dcbcfg->numapps; j++) {
				u16 prot_id = cmp_dcbcfg->app[j].prot_id;
				u8 sel = cmp_dcbcfg->app[j].selector;

				if  (sel == ICE_APP_SEL_TCPIP &&
				     (prot_id == ICE_APP_PROT_ID_ISCSI ||
				      prot_id == ICE_APP_PROT_ID_ISCSI_860)) {
					ice_app_prot_id_type = prot_id;
					break;
				}
			}
		} else {
			/* FIP APP */
			ice_aqc_cee_status_mask = ICE_AQC_CEE_FIP_STATUS_M;
			ice_aqc_cee_status_shift = ICE_AQC_CEE_FIP_STATUS_S;
			ice_aqc_cee_app_mask = ICE_AQC_CEE_APP_FIP_M;
			ice_aqc_cee_app_shift = ICE_AQC_CEE_APP_FIP_S;
			ice_app_sel_type = ICE_APP_SEL_ETHTYPE;
			ice_app_prot_id_type = ICE_APP_PROT_ID_FIP;
		}

		status = (tlv_status & ice_aqc_cee_status_mask) >>
			 ice_aqc_cee_status_shift;
		err = (status & ICE_TLV_STATUS_ERR) ? 1 : 0;
		sync = (status & ICE_TLV_STATUS_SYNC) ? 1 : 0;
		oper = (status & ICE_TLV_STATUS_OPER) ? 1 : 0;
		/* Add FCoE/iSCSI/FIP APP if Error is False and
		 * Oper/Sync is True
		 */
		if (!err && sync && oper) {
			dcbcfg->app[app_index].priority =
				(u8)((app_prio & ice_aqc_cee_app_mask) >>
				     ice_aqc_cee_app_shift);
			dcbcfg->app[app_index].selector = ice_app_sel_type;
			dcbcfg->app[app_index].prot_id = ice_app_prot_id_type;
			app_index++;
		}
	}

	dcbcfg->numapps = app_index;
}

/**
 * ice_get_ieee_or_cee_dcb_cfg
 * @pi: port information structure
 * @dcbx_mode: mode of DCBX (IEEE or CEE)
 *
 * Get IEEE or CEE mode DCB configuration from the Firmware
 */
STATIC int
ice_get_ieee_or_cee_dcb_cfg(struct ice_port_info *pi, u8 dcbx_mode)
{
	struct ice_dcbx_cfg *dcbx_cfg = NULL;
	int ret;

	if (!pi)
		return ICE_ERR_PARAM;

	if (dcbx_mode == ICE_DCBX_MODE_IEEE)
		dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;
	else if (dcbx_mode == ICE_DCBX_MODE_CEE)
		dcbx_cfg = &pi->qos_cfg.desired_dcbx_cfg;

	/* Get Local DCB Config in case of ICE_DCBX_MODE_IEEE
	 * or get CEE DCB Desired Config in case of ICE_DCBX_MODE_CEE
	 */
	ret = ice_aq_get_dcb_cfg(pi->hw, ICE_AQ_LLDP_MIB_LOCAL,
				 ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID, dcbx_cfg);
	if (ret)
		goto out;

	/* Get Remote DCB Config */
	dcbx_cfg = &pi->qos_cfg.remote_dcbx_cfg;
	ret = ice_aq_get_dcb_cfg(pi->hw, ICE_AQ_LLDP_MIB_REMOTE,
				 ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID, dcbx_cfg);
	/* Don't treat ENOENT as an error for Remote MIBs */
	if (pi->hw->adminq.sq_last_status == ICE_AQ_RC_ENOENT)
		ret = 0;

out:
	return ret;
}

/**
 * ice_get_dcb_cfg
 * @pi: port information structure
 *
 * Get DCB configuration from the Firmware
 */
int ice_get_dcb_cfg(struct ice_port_info *pi)
{
	struct ice_aqc_get_cee_dcb_cfg_resp cee_cfg;
	struct ice_dcbx_cfg *dcbx_cfg;
	int ret;

	if (!pi)
		return ICE_ERR_PARAM;

	ret = ice_aq_get_cee_dcb_cfg(pi->hw, &cee_cfg, NULL);
	if (!ret) {
		/* CEE mode */
		ret = ice_get_ieee_or_cee_dcb_cfg(pi, ICE_DCBX_MODE_CEE);
		ice_cee_to_dcb_cfg(&cee_cfg, pi);
	} else if (pi->hw->adminq.sq_last_status == ICE_AQ_RC_ENOENT) {
		/* CEE mode not enabled try querying IEEE data */
		dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;
		dcbx_cfg->dcbx_mode = ICE_DCBX_MODE_IEEE;
		ret = ice_get_ieee_or_cee_dcb_cfg(pi, ICE_DCBX_MODE_IEEE);
	}

	return ret;
}

/**
 * ice_get_dcb_cfg_from_mib_change
 * @pi: port information structure
 * @event: pointer to the admin queue receive event
 *
 * Set DCB configuration from received MIB Change event
 */
void ice_get_dcb_cfg_from_mib_change(struct ice_port_info *pi,
				     struct ice_rq_event_info *event)
{
	struct ice_dcbx_cfg *dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;
	struct ice_aqc_lldp_get_mib *mib;
	u8 change_type, dcbx_mode;

	mib = (struct ice_aqc_lldp_get_mib *)&event->desc.params.raw;

	change_type = mib->type & ICE_AQ_LLDP_MIB_TYPE_M;
	if (change_type == ICE_AQ_LLDP_MIB_REMOTE)
		dcbx_cfg = &pi->qos_cfg.remote_dcbx_cfg;

	dcbx_mode = ((mib->type & ICE_AQ_LLDP_DCBX_M) >>
		     ICE_AQ_LLDP_DCBX_S);

	switch (dcbx_mode) {
	case ICE_AQ_LLDP_DCBX_IEEE:
		dcbx_cfg->dcbx_mode = ICE_DCBX_MODE_IEEE;
		ice_lldp_to_dcb_cfg(event->msg_buf, dcbx_cfg);
		break;

	case ICE_AQ_LLDP_DCBX_CEE:
		pi->qos_cfg.desired_dcbx_cfg = pi->qos_cfg.local_dcbx_cfg;
		ice_cee_to_dcb_cfg((struct ice_aqc_get_cee_dcb_cfg_resp *)
				   event->msg_buf, pi);
		break;
	}
}

/**
 * ice_init_dcb
 * @hw: pointer to the HW struct
 * @enable_mib_change: enable MIB change event
 *
 * Update DCB configuration from the Firmware
 */
int ice_init_dcb(struct ice_hw *hw, bool enable_mib_change)
{
	struct ice_qos_cfg *qos_cfg = &hw->port_info->qos_cfg;
	int ret = 0;

	if (!hw->func_caps.common_cap.dcb)
		return ICE_ERR_NOT_SUPPORTED;

	qos_cfg->is_sw_lldp = true;

	/* Get DCBX status */
	qos_cfg->dcbx_status = ice_get_dcbx_status(hw);

	if (qos_cfg->dcbx_status == ICE_DCBX_STATUS_DONE ||
	    qos_cfg->dcbx_status == ICE_DCBX_STATUS_IN_PROGRESS ||
	    qos_cfg->dcbx_status == ICE_DCBX_STATUS_NOT_STARTED) {
		/* Get current DCBX configuration */
		ret = ice_get_dcb_cfg(hw->port_info);
		if (ret)
			return ret;
		qos_cfg->is_sw_lldp = false;
	} else if (qos_cfg->dcbx_status == ICE_DCBX_STATUS_DIS) {
		return ICE_ERR_NOT_READY;
	}

	/* Configure the LLDP MIB change event */
	if (enable_mib_change) {
		ret = ice_aq_cfg_lldp_mib_change(hw, true, NULL);
		if (ret)
			qos_cfg->is_sw_lldp = true;
	}

	return ret;
}

/**
 * ice_cfg_lldp_mib_change
 * @hw: pointer to the HW struct
 * @ena_mib: enable/disable MIB change event
 *
 * Configure (disable/enable) MIB
 */
int ice_cfg_lldp_mib_change(struct ice_hw *hw, bool ena_mib)
{
	struct ice_qos_cfg *qos_cfg = &hw->port_info->qos_cfg;
	int ret;

	if (!hw->func_caps.common_cap.dcb)
		return ICE_ERR_NOT_SUPPORTED;

	/* Get DCBX status */
	qos_cfg->dcbx_status = ice_get_dcbx_status(hw);

	if (qos_cfg->dcbx_status == ICE_DCBX_STATUS_DIS)
		return ICE_ERR_NOT_READY;

	ret = ice_aq_cfg_lldp_mib_change(hw, ena_mib, NULL);
	if (!ret)
		qos_cfg->is_sw_lldp = !ena_mib;

	return ret;
}

/**
 * ice_add_ieee_ets_common_tlv
 * @buf: Data buffer to be populated with ice_dcb_ets_cfg data
 * @ets_cfg: Container for ice_dcb_ets_cfg data
 *
 * Populate the TLV buffer with ice_dcb_ets_cfg data
 */
static void
ice_add_ieee_ets_common_tlv(u8 *buf, struct ice_dcb_ets_cfg *ets_cfg)
{
	u8 priority0, priority1;
	u8 offset = 0;
	int i;

	/* Priority Assignment Table (4 octets)
	 * Octets:|    1    |    2    |    3    |    4    |
	 *        -----------------------------------------
	 *        |pri0|pri1|pri2|pri3|pri4|pri5|pri6|pri7|
	 *        -----------------------------------------
	 *   Bits:|7  4|3  0|7  4|3  0|7  4|3  0|7  4|3  0|
	 *        -----------------------------------------
	 */
	for (i = 0; i < ICE_MAX_TRAFFIC_CLASS / 2; i++) {
		priority0 = ets_cfg->prio_table[i * 2] & 0xF;
		priority1 = ets_cfg->prio_table[i * 2 + 1] & 0xF;
		buf[offset] = (priority0 << ICE_IEEE_ETS_PRIO_1_S) | priority1;
		offset++;
	}

	/* TC Bandwidth Table (8 octets)
	 * Octets:| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
	 *        ---------------------------------
	 *        |tc0|tc1|tc2|tc3|tc4|tc5|tc6|tc7|
	 *        ---------------------------------
	 *
	 * TSA Assignment Table (8 octets)
	 * Octets:| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
	 *        ---------------------------------
	 *        |tc0|tc1|tc2|tc3|tc4|tc5|tc6|tc7|
	 *        ---------------------------------
	 */
	ice_for_each_traffic_class(i) {
		buf[offset] = ets_cfg->tcbwtable[i];
		buf[ICE_MAX_TRAFFIC_CLASS + offset] = ets_cfg->tsatable[i];
		offset++;
	}
}

/**
 * ice_add_ieee_ets_tlv - Prepare ETS TLV in IEEE format
 * @tlv: Fill the ETS config data in IEEE format
 * @dcbcfg: Local store which holds the DCB Config
 *
 * Prepare IEEE 802.1Qaz ETS CFG TLV
 */
static void
ice_add_ieee_ets_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etscfg;
	u8 *buf = tlv->tlvinfo;
	u8 maxtcwilling = 0;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_IEEE_ETS_TLV_LEN);
	tlv->typelen = HTONS(typelen);

	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_ETS_CFG);
	tlv->ouisubtype = HTONL(ouisubtype);

	/* First Octet post subtype
	 * --------------------------
	 * |will-|CBS  | Re-  | Max |
	 * |ing  |     |served| TCs |
	 * --------------------------
	 * |1bit | 1bit|3 bits|3bits|
	 */
	etscfg = &dcbcfg->etscfg;
	if (etscfg->willing)
		maxtcwilling = BIT(ICE_IEEE_ETS_WILLING_S);
	maxtcwilling |= etscfg->maxtcs & ICE_IEEE_ETS_MAXTC_M;
	buf[0] = maxtcwilling;

	/* Begin adding at Priority Assignment Table (offset 1 in buf) */
	ice_add_ieee_ets_common_tlv(&buf[1], etscfg);
}

/**
 * ice_add_ieee_etsrec_tlv - Prepare ETS Recommended TLV in IEEE format
 * @tlv: Fill ETS Recommended TLV in IEEE format
 * @dcbcfg: Local store which holds the DCB Config
 *
 * Prepare IEEE 802.1Qaz ETS REC TLV
 */
static void
ice_add_ieee_etsrec_tlv(struct ice_lldp_org_tlv *tlv,
			struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etsrec;
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_IEEE_ETS_TLV_LEN);
	tlv->typelen = HTONS(typelen);

	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_ETS_REC);
	tlv->ouisubtype = HTONL(ouisubtype);

	etsrec = &dcbcfg->etsrec;

	/* First Octet is reserved */
	/* Begin adding at Priority Assignment Table (offset 1 in buf) */
	ice_add_ieee_ets_common_tlv(&buf[1], etsrec);
}

/**
 * ice_add_ieee_pfc_tlv - Prepare PFC TLV in IEEE format
 * @tlv: Fill PFC TLV in IEEE format
 * @dcbcfg: Local store which holds the PFC CFG data
 *
 * Prepare IEEE 802.1Qaz PFC CFG TLV
 */
static void
ice_add_ieee_pfc_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_IEEE_PFC_TLV_LEN);
	tlv->typelen = HTONS(typelen);

	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_PFC_CFG);
	tlv->ouisubtype = HTONL(ouisubtype);

	/* ----------------------------------------
	 * |will-|MBC  | Re-  | PFC |  PFC Enable  |
	 * |ing  |     |served| cap |              |
	 * -----------------------------------------
	 * |1bit | 1bit|2 bits|4bits| 1 octet      |
	 */
	if (dcbcfg->pfc.willing)
		buf[0] = BIT(ICE_IEEE_PFC_WILLING_S);

	if (dcbcfg->pfc.mbc)
		buf[0] |= BIT(ICE_IEEE_PFC_MBC_S);

	buf[0] |= dcbcfg->pfc.pfccap & 0xF;
	buf[1] = dcbcfg->pfc.pfcena;
}

/**
 * ice_add_ieee_app_pri_tlv -  Prepare APP TLV in IEEE format
 * @tlv: Fill APP TLV in IEEE format
 * @dcbcfg: Local store which holds the APP CFG data
 *
 * Prepare IEEE 802.1Qaz APP CFG TLV
 */
static void
ice_add_ieee_app_pri_tlv(struct ice_lldp_org_tlv *tlv,
			 struct ice_dcbx_cfg *dcbcfg)
{
	u16 typelen, len, offset = 0;
	u8 priority, selector, i = 0;
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;

	/* No APP TLVs then just return */
	if (dcbcfg->numapps == 0)
		return;
	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_APP_PRI);
	tlv->ouisubtype = HTONL(ouisubtype);

	/* Move offset to App Priority Table */
	offset++;
	/* Application Priority Table (3 octets)
	 * Octets:|         1          |    2    |    3    |
	 *        -----------------------------------------
	 *        |Priority|Rsrvd| Sel |    Protocol ID    |
	 *        -----------------------------------------
	 *   Bits:|23    21|20 19|18 16|15                0|
	 *        -----------------------------------------
	 */
	while (i < dcbcfg->numapps) {
		priority = dcbcfg->app[i].priority & 0x7;
		selector = dcbcfg->app[i].selector & 0x7;
		buf[offset] = (priority << ICE_IEEE_APP_PRIO_S) | selector;
		buf[offset + 1] = (dcbcfg->app[i].prot_id >> 0x8) & 0xFF;
		buf[offset + 2] = dcbcfg->app[i].prot_id & 0xFF;
		/* Move to next app */
		offset += 3;
		i++;
		if (i >= ICE_DCBX_MAX_APPS)
			break;
	}
	/* len includes size of ouisubtype + 1 reserved + 3*numapps */
	len = sizeof(tlv->ouisubtype) + 1 + (i * 3);
	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) | (len & 0x1FF));
	tlv->typelen = HTONS(typelen);
}

/**
 * ice_add_dscp_up_tlv - Prepare DSCP to UP TLV
 * @tlv: location to build the TLV data
 * @dcbcfg: location of data to convert to TLV
 */
static void
ice_add_dscp_up_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;
	int i;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_DSCP_UP_TLV_LEN);
	tlv->typelen = HTONS(typelen);

	ouisubtype = (u32)((ICE_DSCP_OUI << ICE_LLDP_TLV_OUI_S) |
			   ICE_DSCP_SUBTYPE_DSCP2UP);
	tlv->ouisubtype = HTONL(ouisubtype);

	/* bytes 0 - 63 - IPv4 DSCP2UP LUT */
	for (i = 0; i < ICE_DSCP_NUM_VAL; i++) {
		/* IPv4 mapping */
		buf[i] = dcbcfg->dscp_map[i];
		/* IPv6 mapping */
		buf[i + ICE_DSCP_IPV6_OFFSET] = dcbcfg->dscp_map[i];
	}

	/* byte 64 - IPv4 untagged traffic */
	buf[i] = 0;

	/* byte 144 - IPv6 untagged traffic */
	buf[i + ICE_DSCP_IPV6_OFFSET] = 0;
}

#define ICE_BYTES_PER_TC	8
/**
 * ice_add_dscp_enf_tlv - Prepare DSCP Enforcement TLV
 * @tlv: location to build the TLV data
 */
static void
ice_add_dscp_enf_tlv(struct ice_lldp_org_tlv *tlv)
{
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_DSCP_ENF_TLV_LEN);
	tlv->typelen = HTONS(typelen);

	ouisubtype = (u32)((ICE_DSCP_OUI << ICE_LLDP_TLV_OUI_S) |
			   ICE_DSCP_SUBTYPE_ENFORCE);
	tlv->ouisubtype = HTONL(ouisubtype);

	/* Allow all DSCP values to be valid for all TC's (IPv4 and IPv6) */
	memset(buf, 0, 2 * (ICE_MAX_TRAFFIC_CLASS * ICE_BYTES_PER_TC));
}

/**
 * ice_add_dscp_tc_bw_tlv - Prepare DSCP BW for TC TLV
 * @tlv: location to build the TLV data
 * @dcbcfg: location of the data to convert to TLV
 */
static void
ice_add_dscp_tc_bw_tlv(struct ice_lldp_org_tlv *tlv,
		       struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etscfg;
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u8 offset = 0;
	u16 typelen;
	int i;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_DSCP_TC_BW_TLV_LEN);
	tlv->typelen = HTONS(typelen);

	ouisubtype = (u32)((ICE_DSCP_OUI << ICE_LLDP_TLV_OUI_S) |
			   ICE_DSCP_SUBTYPE_TCBW);
	tlv->ouisubtype = HTONL(ouisubtype);

	/* First Octect after subtype
	 * ----------------------------
	 * | RSV | CBS | RSV | Max TCs |
	 * | 1b  | 1b  | 3b  | 3b      |
	 * ----------------------------
	 */
	etscfg = &dcbcfg->etscfg;
	buf[0] = etscfg->maxtcs & ICE_IEEE_ETS_MAXTC_M;

	/* bytes 1 - 4 reserved */
	offset = 5;

	/* TC BW table
	 * bytes 0 - 7 for TC 0 - 7
	 *
	 * TSA Assignment table
	 * bytes 8 - 15 for TC 0 - 7
	 */
	for (i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		buf[offset] = etscfg->tcbwtable[i];
		buf[offset + ICE_MAX_TRAFFIC_CLASS] = etscfg->tsatable[i];
		offset++;
	}
}

/**
 * ice_add_dscp_pfc_tlv - Prepare DSCP PFC TLV
 * @tlv: Fill PFC TLV in IEEE format
 * @dcbcfg: Local store which holds the PFC CFG data
 */
static void
ice_add_dscp_pfc_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_DSCP_PFC_TLV_LEN);
	tlv->typelen = HTONS(typelen);

	ouisubtype = (u32)((ICE_DSCP_OUI << ICE_LLDP_TLV_OUI_S) |
			   ICE_DSCP_SUBTYPE_PFC);
	tlv->ouisubtype = HTONL(ouisubtype);

	buf[0] = dcbcfg->pfc.pfccap & 0xF;
	buf[1] = dcbcfg->pfc.pfcena;
}

/**
 * ice_add_dcb_tlv - Add all IEEE or DSCP TLVs
 * @tlv: Fill TLV data in IEEE format
 * @dcbcfg: Local store which holds the DCB Config
 * @tlvid: Type of IEEE TLV
 *
 * Add tlv information
 */
static void
ice_add_dcb_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg,
		u16 tlvid)
{
	if (dcbcfg->pfc_mode == ICE_QOS_MODE_VLAN) {
		switch (tlvid) {
		case ICE_IEEE_TLV_ID_ETS_CFG:
			ice_add_ieee_ets_tlv(tlv, dcbcfg);
			break;
		case ICE_IEEE_TLV_ID_ETS_REC:
			ice_add_ieee_etsrec_tlv(tlv, dcbcfg);
			break;
		case ICE_IEEE_TLV_ID_PFC_CFG:
			ice_add_ieee_pfc_tlv(tlv, dcbcfg);
			break;
		case ICE_IEEE_TLV_ID_APP_PRI:
			ice_add_ieee_app_pri_tlv(tlv, dcbcfg);
			break;
		default:
			break;
		}
	} else {
		/* pfc_mode == ICE_QOS_MODE_DSCP */
		switch (tlvid) {
		case ICE_TLV_ID_DSCP_UP:
			ice_add_dscp_up_tlv(tlv, dcbcfg);
			break;
		case ICE_TLV_ID_DSCP_ENF:
			ice_add_dscp_enf_tlv(tlv);
			break;
		case ICE_TLV_ID_DSCP_TC_BW:
			ice_add_dscp_tc_bw_tlv(tlv, dcbcfg);
			break;
		case ICE_TLV_ID_DSCP_TO_PFC:
			ice_add_dscp_pfc_tlv(tlv, dcbcfg);
			break;
		default:
			break;
		}
	}
}

/**
 * ice_dcb_cfg_to_lldp - Convert DCB configuration to MIB format
 * @lldpmib: pointer to the HW struct
 * @miblen: length of LLDP MIB
 * @dcbcfg: Local store which holds the DCB Config
 *
 * Convert the DCB configuration to MIB format
 */
void ice_dcb_cfg_to_lldp(u8 *lldpmib, u16 *miblen, struct ice_dcbx_cfg *dcbcfg)
{
	u16 len, offset = 0, tlvid = ICE_TLV_ID_START;
	struct ice_lldp_org_tlv *tlv;
	u16 typelen;

	tlv = (struct ice_lldp_org_tlv *)lldpmib;
	while (1) {
		ice_add_dcb_tlv(tlv, dcbcfg, tlvid++);
		typelen = NTOHS(tlv->typelen);
		len = (typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S;
		if (len)
			offset += len + 2;
		/* END TLV or beyond LLDPDU size */
		if (tlvid >= ICE_TLV_ID_END_OF_LLDPPDU ||
		    offset > ICE_LLDPDU_SIZE)
			break;
		/* Move to next TLV */
		if (len)
			tlv = (struct ice_lldp_org_tlv *)
				((char *)tlv + sizeof(tlv->typelen) + len);
	}
	*miblen = offset;
}

/**
 * ice_set_dcb_cfg - Set the local LLDP MIB to FW
 * @pi: port information structure
 *
 * Set DCB configuration to the Firmware
 */
int ice_set_dcb_cfg(struct ice_port_info *pi)
{
	u8 mib_type, *lldpmib = NULL;
	struct ice_dcbx_cfg *dcbcfg;
	struct ice_hw *hw;
	u16 miblen;
	int ret;

	if (!pi)
		return ICE_ERR_PARAM;

	hw = pi->hw;

	/* update the HW local config */
	dcbcfg = &pi->qos_cfg.local_dcbx_cfg;
	/* Allocate the LLDPDU */
	lldpmib = (u8 *)ice_malloc(hw, ICE_LLDPDU_SIZE);
	if (!lldpmib)
		return ICE_ERR_NO_MEMORY;

	mib_type = SET_LOCAL_MIB_TYPE_LOCAL_MIB;
	if (dcbcfg->app_mode == ICE_DCBX_APPS_NON_WILLING)
		mib_type |= SET_LOCAL_MIB_TYPE_CEE_NON_WILLING;

	ice_dcb_cfg_to_lldp(lldpmib, &miblen, dcbcfg);
	ret = ice_aq_set_lldp_mib(hw, mib_type, (void *)lldpmib, miblen,
				  NULL);

	ice_free(hw, lldpmib);

	return ret;
}

/**
 * ice_aq_query_port_ets - query port ETS configuration
 * @pi: port information structure
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @cd: pointer to command details structure or NULL
 *
 * query current port ETS configuration
 */
int
ice_aq_query_port_ets(struct ice_port_info *pi,
		      struct ice_aqc_port_ets_elem *buf, u16 buf_size,
		      struct ice_sq_cd *cd)
{
	struct ice_aqc_query_port_ets *cmd;
	struct ice_aq_desc desc;
	int status;

	if (!pi || !pi->root)
		return ICE_ERR_PARAM;
	cmd = &desc.params.port_ets;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_query_port_ets);
	cmd->port_teid = pi->root->info.node_teid;

	status = ice_aq_send_cmd(pi->hw, &desc, buf, buf_size, cd);
	return status;
}

/**
 * ice_update_port_tc_tree_cfg - update TC tree configuration
 * @pi: port information structure
 * @buf: pointer to buffer
 *
 * update the SW DB with the new TC changes
 */
int
ice_update_port_tc_tree_cfg(struct ice_port_info *pi,
			    struct ice_aqc_port_ets_elem *buf)
{
	struct ice_sched_node *node, *tc_node;
	struct ice_aqc_txsched_elem_data elem;
	u32 teid1, teid2;
	int status = 0;
	u8 i, j;

	if (!pi)
		return ICE_ERR_PARAM;
	/* suspend the missing TC nodes */
	for (i = 0; i < pi->root->num_children; i++) {
		teid1 = LE32_TO_CPU(pi->root->children[i]->info.node_teid);
		ice_for_each_traffic_class(j) {
			teid2 = LE32_TO_CPU(buf->tc_node_teid[j]);
			if (teid1 == teid2)
				break;
		}
		if (j < ICE_MAX_TRAFFIC_CLASS)
			continue;
		/* TC is missing */
		pi->root->children[i]->in_use = false;
	}
	/* add the new TC nodes */
	ice_for_each_traffic_class(j) {
		teid2 = LE32_TO_CPU(buf->tc_node_teid[j]);
		if (teid2 == ICE_INVAL_TEID)
			continue;
		/* Is it already present in the tree ? */
		for (i = 0; i < pi->root->num_children; i++) {
			tc_node = pi->root->children[i];
			if (!tc_node)
				continue;
			teid1 = LE32_TO_CPU(tc_node->info.node_teid);
			if (teid1 == teid2) {
				tc_node->tc_num = j;
				tc_node->in_use = true;
				break;
			}
		}
		if (i < pi->root->num_children)
			continue;
		/* new TC */
		status = ice_sched_query_elem(pi->hw, teid2, &elem);
		if (!status)
			status = ice_sched_add_node(pi, 1, &elem, NULL);
		if (status)
			break;
		/* update the TC number */
		node = ice_sched_find_node_by_teid(pi->root, teid2);
		if (node)
			node->tc_num = j;
	}
	return status;
}

/**
 * ice_query_port_ets - query port ETS configuration
 * @pi: port information structure
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @cd: pointer to command details structure or NULL
 *
 * query current port ETS configuration and update the
 * SW DB with the TC changes
 */
int
ice_query_port_ets(struct ice_port_info *pi,
		   struct ice_aqc_port_ets_elem *buf, u16 buf_size,
		   struct ice_sq_cd *cd)
{
	int status;

	ice_acquire_lock(&pi->sched_lock);
	status = ice_aq_query_port_ets(pi, buf, buf_size, cd);
	if (!status)
		status = ice_update_port_tc_tree_cfg(pi, buf);
	ice_release_lock(&pi->sched_lock);
	return status;
}
