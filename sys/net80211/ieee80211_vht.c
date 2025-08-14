/*-
 * Copyright (c) 2017 Adrian Chadd <adrian@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IEEE 802.11ac-2013 protocol support.
 */

#include "opt_inet.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h> 
#include <sys/endian.h>

#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_action.h>
#include <net80211/ieee80211_input.h>
#include <net80211/ieee80211_vht.h>

#define	ADDSHORT(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = (v) >> 8;			\
	frm += 2;				\
} while (0)
#define	ADDWORD(frm, v) do {			\
	frm[0] = (v) & 0xff;			\
	frm[1] = ((v) >> 8) & 0xff;		\
	frm[2] = ((v) >> 16) & 0xff;		\
	frm[3] = ((v) >> 24) & 0xff;		\
	frm += 4;				\
} while (0)

/*
 * Immediate TODO:
 *
 * + handle WLAN_ACTION_VHT_OPMODE_NOTIF and other VHT action frames
 * + ensure vhtinfo/vhtcap parameters correctly use the negotiated
 *   capabilities and ratesets
 * + group ID management operation
 */

/*
 * XXX TODO: handle WLAN_ACTION_VHT_OPMODE_NOTIF
 *
 * Look at mac80211/vht.c:ieee80211_vht_handle_opmode() for further details.
 */

static int
vht_recv_action_placeholder(struct ieee80211_node *ni,
    const struct ieee80211_frame *wh,
    const uint8_t *frm, const uint8_t *efrm)
{

#ifdef IEEE80211_DEBUG
	ieee80211_note(ni->ni_vap, "%s: called; fc=0x%.2x/0x%.2x",
	    __func__, wh->i_fc[0], wh->i_fc[1]);
#endif
	return (0);
}

static int
vht_send_action_placeholder(struct ieee80211_node *ni,
    int category, int action, void *arg0)
{

#ifdef IEEE80211_DEBUG
	ieee80211_note(ni->ni_vap, "%s: called; category=%d, action=%d",
	    __func__, category, action);
#endif
	return (EINVAL);
}

static void
ieee80211_vht_init(void)
{

	ieee80211_recv_action_register(IEEE80211_ACTION_CAT_VHT,
	    WLAN_ACTION_VHT_COMPRESSED_BF, vht_recv_action_placeholder);
	ieee80211_recv_action_register(IEEE80211_ACTION_CAT_VHT,
	    WLAN_ACTION_VHT_GROUPID_MGMT, vht_recv_action_placeholder);
	ieee80211_recv_action_register(IEEE80211_ACTION_CAT_VHT,
	    WLAN_ACTION_VHT_OPMODE_NOTIF, vht_recv_action_placeholder);

	ieee80211_send_action_register(IEEE80211_ACTION_CAT_VHT,
	    WLAN_ACTION_VHT_COMPRESSED_BF, vht_send_action_placeholder);
	ieee80211_send_action_register(IEEE80211_ACTION_CAT_VHT,
	    WLAN_ACTION_VHT_GROUPID_MGMT, vht_send_action_placeholder);
	ieee80211_send_action_register(IEEE80211_ACTION_CAT_VHT,
	    WLAN_ACTION_VHT_OPMODE_NOTIF, vht_send_action_placeholder);
}

SYSINIT(wlan_vht, SI_SUB_DRIVERS, SI_ORDER_FIRST, ieee80211_vht_init, NULL);

void
ieee80211_vht_attach(struct ieee80211com *ic)
{
}

void
ieee80211_vht_detach(struct ieee80211com *ic)
{
}

void
ieee80211_vht_vattach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (! IEEE80211_CONF_VHT(ic))
		return;

	vap->iv_vht_cap.vht_cap_info = ic->ic_vht_cap.vht_cap_info;
	vap->iv_vhtextcaps = ic->ic_vhtextcaps;

	/* XXX assume VHT80 support; should really check vhtcaps */
	vap->iv_vht_flags =
	    IEEE80211_FVHT_VHT
	    | IEEE80211_FVHT_USEVHT40
	    | IEEE80211_FVHT_USEVHT80;
	if (IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_IS_160MHZ(vap->iv_vht_cap.vht_cap_info))
		vap->iv_vht_flags |= IEEE80211_FVHT_USEVHT160;
	if (IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_IS_160_80P80MHZ(vap->iv_vht_cap.vht_cap_info))
		vap->iv_vht_flags |= IEEE80211_FVHT_USEVHT80P80;

	memcpy(&vap->iv_vht_cap.supp_mcs, &ic->ic_vht_cap.supp_mcs,
	    sizeof(struct ieee80211_vht_mcs_info));
}

void
ieee80211_vht_vdetach(struct ieee80211vap *vap)
{
}

#if 0
static void
vht_announce(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
}
#endif

static int
vht_mcs_to_num(int m)
{

	switch (m) {
	case IEEE80211_VHT_MCS_SUPPORT_0_7:
		return (7);
	case IEEE80211_VHT_MCS_SUPPORT_0_8:
		return (8);
	case IEEE80211_VHT_MCS_SUPPORT_0_9:
		return (9);
	default:
		return (0);
	}
}

void
ieee80211_vht_announce(struct ieee80211com *ic)
{
	int i, tx, rx;

	if (! IEEE80211_CONF_VHT(ic))
		return;

	/* Channel width */
	ic_printf(ic, "[VHT] Channel Widths: 20MHz, 40MHz, 80MHz%s%s\n",
	    (IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_IS_160MHZ(ic->ic_vht_cap.vht_cap_info)) ?
		", 160MHz" : "",
	    (IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_IS_160_80P80MHZ(ic->ic_vht_cap.vht_cap_info)) ?
		 ", 80+80MHz" : "");
	/* Features */
	ic_printf(ic, "[VHT] Features: %b\n", ic->ic_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_BITS);

	/* For now, just 5GHz VHT.  Worry about 2GHz VHT later */
	for (i = 0; i < 8; i++) {
		/* Each stream is 2 bits */
		tx = (ic->ic_vht_cap.supp_mcs.tx_mcs_map >> (2*i)) & 0x3;
		rx = (ic->ic_vht_cap.supp_mcs.rx_mcs_map >> (2*i)) & 0x3;
		if (tx == 3 && rx == 3)
			continue;
		ic_printf(ic, "[VHT] NSS %d: TX MCS 0..%d, RX MCS 0..%d\n",
		    i + 1, vht_mcs_to_num(tx), vht_mcs_to_num(rx));
	}
}

void
ieee80211_vht_node_init(struct ieee80211_node *ni)
{

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_11N, ni,
	    "%s: called", __func__);
	ni->ni_flags |= IEEE80211_NODE_VHT;
}

void
ieee80211_vht_node_cleanup(struct ieee80211_node *ni)
{

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_11N, ni,
	    "%s: called", __func__);
	ni->ni_flags &= ~IEEE80211_NODE_VHT;
	ni->ni_vhtcap = 0;
	bzero(&ni->ni_vht_mcsinfo, sizeof(struct ieee80211_vht_mcs_info));
}

/*
 * Parse an 802.11ac VHT operation IE.
 *
 * 802.11-2020 9.4.2.158 (VHT Operation element)
 */
void
ieee80211_parse_vhtopmode(struct ieee80211_node *ni, const uint8_t *ie)
{
	/* vht operation */
	ni->ni_vht_chanwidth = ie[2];
	ni->ni_vht_chan1 = ie[3];
	ni->ni_vht_chan2 = ie[4];
	ni->ni_vht_basicmcs = le16dec(ie + 5);

#if 0
	net80211_vap_printf(ni->ni_vap,
	    "%s: chan1=%d, chan2=%d, chanwidth=%d, basicmcs=0x%04x\n",
	    __func__, ni->ni_vht_chan1, ni->ni_vht_chan2, ni->ni_vht_chanwidth,
	    ni->ni_vht_basicmcs);
#endif
}

/*
 * Parse an 802.11ac VHT capability IE.
 *
 * 802.11-2020 9.4.2.157 (VHT Capabilities element)
 */
void
ieee80211_parse_vhtcap(struct ieee80211_node *ni, const uint8_t *ie)
{

	/* vht capability */
	ni->ni_vhtcap = le32dec(ie + 2);

	/* suppmcs */
	ni->ni_vht_mcsinfo.rx_mcs_map = le16dec(ie + 6);
	ni->ni_vht_mcsinfo.rx_highest = le16dec(ie + 8);
	ni->ni_vht_mcsinfo.tx_mcs_map = le16dec(ie + 10);
	ni->ni_vht_mcsinfo.tx_highest = le16dec(ie + 12);
}

int
ieee80211_vht_updateparams(struct ieee80211_node *ni,
    const uint8_t *vhtcap_ie,
    const uint8_t *vhtop_ie)
{

	//printf("%s: called\n", __func__);

	ieee80211_parse_vhtcap(ni, vhtcap_ie);
	ieee80211_parse_vhtopmode(ni, vhtop_ie);
	return (0);
}

/**
 * @brief calculate the supported MCS rates for this node
 *
 * This is called once a node has finished association /
 * joined a BSS.  The vhtcap / vhtop IEs are from the
 * peer.  The transmit rate tables need to be combined
 * together to setup the list of available rates.
 *
 * This must be called after the ieee80211_node VHT fields
 * have been parsed / populated by either ieee80211_vht_updateparams() or
 * ieee80211_parse_vhtcap(),
 *
 * This does not take into account the channel bandwidth,
 * which (a) may change during operation, and (b) depends
 * upon packet to packet rate transmission selection.
 * There are various rate combinations which are not
 * available in various channel widths and those will
 * need to be masked off separately.
 *
 * (See 802.11-2020 21.5 Parameters for VHT-MCSs for the
 * tables and supported rates.)
 *
 * ALSO: i need to do some filtering based on the HT set too.
 * (That should be done here too, and in the negotiation, sigh.)
 * (See 802.11-2016 10.7.12.3 Additional rate selection constraints
 * for VHT PPDUs)
 *
 * @param ni	struct ieee80211_node to configure
 */
void
ieee80211_setup_vht_rates(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
	uint32_t val, val1, val2;
	uint16_t tx_mcs_map = 0;
	int i;

	/*
	 * Merge our tx_mcs_map with the peer rx_mcs_map to determine what
	 * can be actually transmitted to the peer.
	 */

	for (i = 0; i < 8; i++) {
		/*
		 * Merge the two together; remember that 0..2 is in order
		 * of increasing MCS support, but 3 equals
		 * IEEE80211_VHT_MCS_NOT_SUPPORTED so must "win".
		 */
		val1 = (vap->iv_vht_cap.supp_mcs.tx_mcs_map >> (i*2)) & 0x3;
		val2 = (ni->ni_vht_mcsinfo.rx_mcs_map >> (i*2)) & 0x3;
		val = MIN(val1, val2);
		if (val1 == IEEE80211_VHT_MCS_NOT_SUPPORTED ||
		    val2 == IEEE80211_VHT_MCS_NOT_SUPPORTED)
			val = IEEE80211_VHT_MCS_NOT_SUPPORTED;
		tx_mcs_map |= (val << (i*2));
	}

	/* Store the TX MCS map somewhere in the node that can be used */
	ni->ni_vht_tx_map = tx_mcs_map;
}

void
ieee80211_vht_timeout(struct ieee80211vap *vap)
{
}

void
ieee80211_vht_node_join(struct ieee80211_node *ni)
{

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_11N, ni,
	    "%s: called", __func__);
}

void
ieee80211_vht_node_leave(struct ieee80211_node *ni)
{

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_11N, ni,
	    "%s: called", __func__);
}

/*
 * Calculate the VHTCAP IE for a given node.
 *
 * This includes calculating the capability intersection based on the
 * current operating mode and intersection of the TX/RX MCS maps.
 *
 * The standard only makes it clear about MCS rate negotiation
 * and MCS basic rates (which must be a subset of the general
 * negotiated rates).  It doesn't make it clear that the AP should
 * figure out the minimum functional overlap with the STA and
 * support that.
 *
 * Note: this is in host order, not in 802.11 endian order.
 *
 * TODO: ensure I re-read 9.7.11 Rate Selection for VHT STAs.
 *
 * TODO: investigate what we should negotiate for MU-MIMO beamforming
 *       options.
 *
 * opmode is '1' for "vhtcap as if I'm a STA", 0 otherwise.
 */
void
ieee80211_vht_get_vhtcap_ie(struct ieee80211_node *ni,
    struct ieee80211_vht_cap *vhtcap, int opmode)
{
	struct ieee80211vap *vap = ni->ni_vap;
//	struct ieee80211com *ic = vap->iv_ic;
	uint32_t val, val1, val2;
	uint32_t new_vhtcap;
	int i;

	/*
	 * Capabilities - it depends on whether we are a station
	 * or not.
	 */
	new_vhtcap = 0;

	/*
	 * Station - use our desired configuration based on
	 * local config, local device bits and the already-learnt
	 * vhtcap/vhtinfo IE in the node.
	 */

	/* Limit MPDU size to the smaller of the two */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_MAX_MPDU_MASK);
	if (opmode == 1) {
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_MAX_MPDU_MASK);
	}
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val, IEEE80211_VHTCAP_MAX_MPDU_MASK);

	/* Limit supp channel config */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_MASK);
	if (opmode == 1) {
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_MASK);
	}
	if ((val2 == 2) &&
	    ((vap->iv_vht_flags & IEEE80211_FVHT_USEVHT80P80) == 0))
		val2 = 1;
	if ((val2 == 1) &&
	    ((vap->iv_vht_flags & IEEE80211_FVHT_USEVHT160) == 0))
		val2 = 0;
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	     IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_MASK);

	/* RX LDPC */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_RXLDPC);
	if (opmode == 1) {
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_RXLDPC);
	}
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val, IEEE80211_VHTCAP_RXLDPC);

	/* Short-GI 80 */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_SHORT_GI_80);
	if (opmode == 1) {
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_SHORT_GI_80);
	}
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val, IEEE80211_VHTCAP_SHORT_GI_80);

	/* Short-GI 160 */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_SHORT_GI_160);
	if (opmode == 1) {
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_SHORT_GI_160);
	}
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val, IEEE80211_VHTCAP_SHORT_GI_160);

	/*
	 * STBC is slightly more complicated.
	 *
	 * In non-STA mode, we just announce our capabilities and that
	 * is that.
	 *
	 * In STA mode, we should calculate our capabilities based on
	 * local capabilities /and/ what the remote says. So:
	 *
	 * + Only TX STBC if we support it and the remote supports RX STBC;
	 * + Only announce RX STBC if we support it and the remote supports
	 *   TX STBC;
	 * + RX STBC should be the minimum of local and remote RX STBC;
	 */

	/* TX STBC */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_TXSTBC);
	if (opmode == 1) {
		/* STA mode - enable it only if node RXSTBC is non-zero */
		val2 = !! _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_RXSTBC_MASK);
	}
	val = MIN(val1, val2);
	if ((vap->iv_vht_flags & IEEE80211_FVHT_STBC_TX) == 0)
		val = 0;
	new_vhtcap |= _IEEE80211_SHIFTMASK(val, IEEE80211_VHTCAP_TXSTBC);

	/* RX STBC1..4 */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_RXSTBC_MASK);
	if (opmode == 1) {
		/* STA mode - enable it only if node TXSTBC is non-zero */
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		   IEEE80211_VHTCAP_TXSTBC);
	}
	val = MIN(val1, val2);
	if ((vap->iv_vht_flags & IEEE80211_FVHT_STBC_RX) == 0)
		val = 0;
	new_vhtcap |= _IEEE80211_SHIFTMASK(val, IEEE80211_VHTCAP_RXSTBC_MASK);

	/*
	 * Finally - if RXSTBC is 0, then don't enable TXSTBC.
	 * Strictly speaking a device can TXSTBC and not RXSTBC, but
	 * it would be silly.
	 */
	if (val == 0)
		new_vhtcap &= ~IEEE80211_VHTCAP_TXSTBC;

	/*
	 * Some of these fields require other fields to exist.
	 * So before using it, the parent field needs to be checked
	 * otherwise the overridden value may be wrong.
	 *
	 * For example, if SU beamformee is set to 0, then BF STS
	 * needs to be 0.
	 */

	/* SU Beamformer capable */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_SU_BEAMFORMER_CAPABLE);
	if (opmode == 1) {
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_SU_BEAMFORMER_CAPABLE);
	}
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	    IEEE80211_VHTCAP_SU_BEAMFORMER_CAPABLE);

	/* SU Beamformee capable */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_SU_BEAMFORMEE_CAPABLE);
	if (opmode == 1) {
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_SU_BEAMFORMEE_CAPABLE);
	}
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	    IEEE80211_VHTCAP_SU_BEAMFORMEE_CAPABLE);

	/* Beamformee STS capability - only if SU beamformee capable */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_BEAMFORMEE_STS_MASK);
	if (opmode == 1) {
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_BEAMFORMEE_STS_MASK);
	}
	val = MIN(val1, val2);
	if ((new_vhtcap & IEEE80211_VHTCAP_SU_BEAMFORMEE_CAPABLE) == 0)
		val = 0;
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	    IEEE80211_VHTCAP_BEAMFORMEE_STS_MASK);

	/* Sounding dimensions - only if SU beamformer capable */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_SOUNDING_DIMENSIONS_MASK);
	if (opmode == 1)
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_SOUNDING_DIMENSIONS_MASK);
	val = MIN(val1, val2);
	if ((new_vhtcap & IEEE80211_VHTCAP_SU_BEAMFORMER_CAPABLE) == 0)
		val = 0;
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	    IEEE80211_VHTCAP_SOUNDING_DIMENSIONS_MASK);

	/*
	 * MU Beamformer capable - only if SU BFF capable, MU BFF capable
	 * and STA (not AP)
	 */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_MU_BEAMFORMER_CAPABLE);
	if (opmode == 1)
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_MU_BEAMFORMER_CAPABLE);
	val = MIN(val1, val2);
	if ((new_vhtcap & IEEE80211_VHTCAP_SU_BEAMFORMER_CAPABLE) == 0)
		val = 0;
	if (opmode != 1)	/* Only enable for STA mode */
		val = 0;
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	   IEEE80211_VHTCAP_SU_BEAMFORMER_CAPABLE);

	/*
	 * MU Beamformee capable - only if SU BFE capable, MU BFE capable
	 * and AP (not STA)
	 */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_MU_BEAMFORMEE_CAPABLE);
	if (opmode == 1)
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_MU_BEAMFORMEE_CAPABLE);
	val = MIN(val1, val2);
	if ((new_vhtcap & IEEE80211_VHTCAP_SU_BEAMFORMEE_CAPABLE) == 0)
		val = 0;
	if (opmode != 0)	/* Only enable for AP mode */
		val = 0;
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	   IEEE80211_VHTCAP_SU_BEAMFORMEE_CAPABLE);

	/* VHT TXOP PS */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_VHT_TXOP_PS);
	if (opmode == 1)
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_VHT_TXOP_PS);
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val, IEEE80211_VHTCAP_VHT_TXOP_PS);

	/* HTC_VHT */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_HTC_VHT);
	if (opmode == 1)
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_HTC_VHT);
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val, IEEE80211_VHTCAP_HTC_VHT);

	/* A-MPDU length max */
	/* XXX TODO: we need a userland config knob for this */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK);
	if (opmode == 1)
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK);
	val = MIN(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	    IEEE80211_VHTCAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK);

	/*
	 * Link adaptation is only valid if HTC-VHT capable is 1.
	 * Otherwise, always set it to 0.
	 */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_VHT_LINK_ADAPTATION_VHT_MASK);
	if (opmode == 1)
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_VHT_LINK_ADAPTATION_VHT_MASK);
	val = MIN(val1, val2);
	if ((new_vhtcap & IEEE80211_VHTCAP_HTC_VHT) == 0)
		val = 0;
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	    IEEE80211_VHTCAP_VHT_LINK_ADAPTATION_VHT_MASK);

	/*
	 * The following two options are 0 if the pattern may change, 1 if it
	 * does not change.  So, downgrade to the higher value.
	 */

	/* RX antenna pattern */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_RX_ANTENNA_PATTERN);
	if (opmode == 1)
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_RX_ANTENNA_PATTERN);
	val = MAX(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	    IEEE80211_VHTCAP_RX_ANTENNA_PATTERN);

	/* TX antenna pattern */
	val2 = val1 = _IEEE80211_MASKSHIFT(vap->iv_vht_cap.vht_cap_info,
	    IEEE80211_VHTCAP_TX_ANTENNA_PATTERN);
	if (opmode == 1)
		val2 = _IEEE80211_MASKSHIFT(ni->ni_vhtcap,
		    IEEE80211_VHTCAP_TX_ANTENNA_PATTERN);
	val = MAX(val1, val2);
	new_vhtcap |= _IEEE80211_SHIFTMASK(val,
	    IEEE80211_VHTCAP_TX_ANTENNA_PATTERN);

	/*
	 * MCS set - again, we announce what we want to use
	 * based on configuration, device capabilities and
	 * already-learnt vhtcap/vhtinfo IE information.
	 */

	/* MCS set - start with whatever the device supports */
	vhtcap->supp_mcs.rx_mcs_map = vap->iv_vht_cap.supp_mcs.rx_mcs_map;
	vhtcap->supp_mcs.rx_highest = 0;
	vhtcap->supp_mcs.tx_mcs_map = vap->iv_vht_cap.supp_mcs.tx_mcs_map;
	vhtcap->supp_mcs.tx_highest = 0;

	vhtcap->vht_cap_info = new_vhtcap;

	/*
	 * Now, if we're a STA, mask off whatever the AP doesn't support.
	 * Ie, we continue to state we can receive whatever we can do,
	 * but we only announce that we will transmit rates that meet
	 * the AP requirement.
	 *
	 * Note: 0 - MCS0..7; 1 - MCS0..8; 2 - MCS0..9; 3 = not supported.
	 * We can't just use MIN() because '3' means "no", so special case it.
	 */
	if (opmode) {
		for (i = 0; i < 8; i++) {
			val1 = (vhtcap->supp_mcs.tx_mcs_map >> (i*2)) & 0x3;
			val2 = (ni->ni_vht_mcsinfo.tx_mcs_map >> (i*2)) & 0x3;
			val = MIN(val1, val2);
			if (val1 == 3 || val2 == 3)
				val = 3;
			vhtcap->supp_mcs.tx_mcs_map &= ~(0x3 << (i*2));
			vhtcap->supp_mcs.tx_mcs_map |= (val << (i*2));
		}
	}
}

/*
 * Add a VHTCAP field.
 *
 * If in station mode, we announce what we would like our
 * desired configuration to be.
 *
 * Else, we announce our capabilities based on our current
 * configuration.
 */
uint8_t *
ieee80211_add_vhtcap(uint8_t *frm, struct ieee80211_node *ni)
{
	struct ieee80211_vht_cap vhtcap;

	ieee80211_vht_get_vhtcap_ie(ni, &vhtcap, 1);

	frm[0] = IEEE80211_ELEMID_VHT_CAP;
	frm[1] = sizeof(vhtcap);
	frm += 2;

	/* 32-bit VHT capability */
	ADDWORD(frm, vhtcap.vht_cap_info);

	/* suppmcs */
	ADDSHORT(frm, vhtcap.supp_mcs.rx_mcs_map);
	ADDSHORT(frm, vhtcap.supp_mcs.rx_highest);
	ADDSHORT(frm, vhtcap.supp_mcs.tx_mcs_map);
	ADDSHORT(frm, vhtcap.supp_mcs.tx_highest);

	return (frm);
}

/*
 * Non-associated probe requests.  Add VHT capabilities based on
 * the current channel configuration.  No BSS yet.
 */
uint8_t *
ieee80211_add_vhtcap_ch(uint8_t *frm, struct ieee80211vap *vap,
    struct ieee80211_channel *c)
{
	struct ieee80211_vht_cap *vhtcap;

	memset(frm, 0, 2 + sizeof(*vhtcap));
	frm[0] = IEEE80211_ELEMID_VHT_CAP;
	frm[1] = sizeof(*vhtcap);
	frm += 2;

	/* 32-bit VHT capability */
	ADDWORD(frm, vap->iv_vht_cap.vht_cap_info);

	/* supp_mcs */
	ADDSHORT(frm, vap->iv_vht_cap.supp_mcs.rx_mcs_map);
	ADDSHORT(frm, vap->iv_vht_cap.supp_mcs.rx_highest);
	ADDSHORT(frm, vap->iv_vht_cap.supp_mcs.tx_mcs_map);
	ADDSHORT(frm, vap->iv_vht_cap.supp_mcs.tx_highest);

	return (frm);
}

static uint8_t
ieee80211_vht_get_chwidth_ie(const struct ieee80211vap *vap,
    const struct ieee80211_channel *c)
{

	/*
	 * XXX TODO: look at the node configuration as
	 * well?
	 */

	if (IEEE80211_IS_CHAN_VHT80P80(c))
		return IEEE80211_VHT_CHANWIDTH_80P80MHZ;
	if (IEEE80211_IS_CHAN_VHT160(c))
		return IEEE80211_VHT_CHANWIDTH_160MHZ;
	if (IEEE80211_IS_CHAN_VHT80(c))
		return IEEE80211_VHT_CHANWIDTH_80MHZ;
	if (IEEE80211_IS_CHAN_VHT40(c))
		return IEEE80211_VHT_CHANWIDTH_USE_HT;
	if (IEEE80211_IS_CHAN_VHT20(c))
		return IEEE80211_VHT_CHANWIDTH_USE_HT;

	/* We shouldn't get here */
	net80211_vap_printf(vap,
	    "%s: called on a non-VHT channel (freq=%d, flags=0x%08x\n",
	    __func__, (int) c->ic_freq, c->ic_flags);
	return IEEE80211_VHT_CHANWIDTH_USE_HT;
}

/*
 * Note: this just uses the current channel information;
 * it doesn't use the node info after parsing.
 *
 * XXX TODO: need to make the basic MCS set configurable.
 * XXX TODO: read 802.11-2013 to determine what to set
 *           chwidth to when scanning.  I have a feeling
 *           it isn't involved in scanning and we shouldn't
 *           be sending it; and I don't yet know what to set
 *           it to for IBSS or hostap where the peer may be
 *           a completely different channel width to us.
 */
uint8_t *
ieee80211_add_vhtinfo(uint8_t *frm, struct ieee80211_node *ni)
{

	frm[0] = IEEE80211_ELEMID_VHT_OPMODE;
	frm[1] = sizeof(struct ieee80211_vht_operation);
	frm += 2;

	/* 8-bit chanwidth */
	*frm++ = ieee80211_vht_get_chwidth_ie(ni->ni_vap, ni->ni_chan);

	/* 8-bit freq1 */
	*frm++ = ni->ni_chan->ic_vht_ch_freq1;

	/* 8-bit freq2 */
	*frm++ = ni->ni_chan->ic_vht_ch_freq2;

	/* 16-bit basic MCS set - just MCS0..7 for NSS=1 for now */
	ADDSHORT(frm, 0xfffc);

	return (frm);
}

void
ieee80211_vht_update_cap(struct ieee80211_node *ni, const uint8_t *vhtcap_ie)
{

	ieee80211_parse_vhtcap(ni, vhtcap_ie);
}

static struct ieee80211_channel *
findvhtchan(struct ieee80211com *ic, struct ieee80211_channel *c, int vhtflags)
{

	return (ieee80211_find_channel(ic, c->ic_freq,
	    (c->ic_flags & ~IEEE80211_CHAN_VHT) | vhtflags));
}

/*
 * Handle channel promotion to VHT, similar to ieee80211_ht_adjust_channel().
 */
struct ieee80211_channel *
ieee80211_vht_adjust_channel(struct ieee80211com *ic,
    struct ieee80211_channel *chan, int flags)
{
	struct ieee80211_channel *c;

	/* First case - handle channel demotion - if VHT isn't set */
	if ((flags & IEEE80211_FVHT_MASK) == 0) {
#if 0
		net80211_ic_printf(ic,
		    "%s: demoting channel %d/0x%08x\n", __func__,
		    chan->ic_ieee, chan->ic_flags);
#endif
		c = ieee80211_find_channel(ic, chan->ic_freq,
		    chan->ic_flags & ~IEEE80211_CHAN_VHT);
		if (c == NULL)
			c = chan;
#if 0
		net80211_ic_printf(ic, "%s: .. to %d/0x%08x\n", __func__,
		    c->ic_ieee, c->ic_flags);
#endif
		return (c);
	}

	/*
	 * We can upgrade to VHT - attempt to do so
	 *
	 * Note: we don't clear the HT flags, these are the hints
	 * for HT40U/HT40D when selecting VHT40 or larger channels.
	 */
	c = NULL;
	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT160))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT160);

	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT80P80))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT80P80);

	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT80))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT80);

	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT40))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT40U);
	if ((c == NULL) && (flags & IEEE80211_FVHT_USEVHT40))
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT40D);
	/*
	 * If we get here, VHT20 is always possible because we checked
	 * for IEEE80211_FVHT_VHT above.
	 */
	if (c == NULL)
		c = findvhtchan(ic, chan, IEEE80211_CHAN_VHT20);

	if (c != NULL)
		chan = c;

#if 0
	net80211_ic_printf(ic, "%s: selected %d/0x%08x\n", __func__,
	    c->ic_ieee, c->ic_flags);
#endif
	return (chan);
}

/*
 * Calculate the VHT operation IE for a given node.
 *
 * This includes calculating the suitable channel width/parameters
 * and basic MCS set.
 *
 * TODO: ensure I read 9.7.11 Rate Selection for VHT STAs.
 * TODO: ensure I read 10.39.7 - BSS Basic VHT-MCS and NSS set operation.
 */
void
ieee80211_vht_get_vhtinfo_ie(struct ieee80211_node *ni,
    struct ieee80211_vht_operation *vhtop, int opmode)
{
	net80211_vap_printf(ni->ni_vap, "%s: called; TODO!\n", __func__);
}

/*
 * Return true if VHT rates can be used for the given node.
 */
bool
ieee80211_vht_check_tx_vht(const struct ieee80211_node *ni)
{
	const struct ieee80211vap *vap;
	const struct ieee80211_channel *bss_chan;

	if (ni == NULL || ni->ni_chan == IEEE80211_CHAN_ANYC ||
	    ni->ni_vap == NULL || ni->ni_vap->iv_bss == NULL)
		return (false);

	vap = ni->ni_vap;
	bss_chan = vap->iv_bss->ni_chan;

	if (bss_chan == IEEE80211_CHAN_ANYC)
		return (false);

	return (IEEE80211_IS_CHAN_VHT(ni->ni_chan));
}

/*
 * Return true if VHT40 rates can be transmitted to the given node.
 *
 * This verifies that the BSS is VHT40 capable and the current
 * node channel width is 40MHz.
 */
static bool
ieee80211_vht_check_tx_vht40(const struct ieee80211_node *ni)
{
	struct ieee80211vap *vap;
	struct ieee80211_channel *bss_chan;

	if (!ieee80211_vht_check_tx_vht(ni))
		return (false);

	vap = ni->ni_vap;
	bss_chan = vap->iv_bss->ni_chan;

	return (IEEE80211_IS_CHAN_VHT40(bss_chan) &&
	    IEEE80211_IS_CHAN_VHT40(ni->ni_chan) &&
	    (ni->ni_chw == NET80211_STA_RX_BW_40));
}

/*
 * Return true if VHT80 rates can be transmitted to the given node.
 *
 * This verifies that the BSS is VHT80 capable and the current
 * node channel width is 80MHz.
 */
static bool
ieee80211_vht_check_tx_vht80(const struct ieee80211_node *ni)
{
	struct ieee80211vap *vap;
	struct ieee80211_channel *bss_chan;

	if (!ieee80211_vht_check_tx_vht(ni))
		return (false);

	vap = ni->ni_vap;
	bss_chan = vap->iv_bss->ni_chan;

	/*
	 * ni_chw represents 20MHz or 40MHz from the HT
	 * TX width action frame / HT channel negotiation.
	 * If a HT TX width action frame sets it to 20MHz
	 * then reject doing 80MHz.
	 */
	return (IEEE80211_IS_CHAN_VHT80(bss_chan) &&
	    IEEE80211_IS_CHAN_VHT80(ni->ni_chan) &&
	    (ni->ni_chw != NET80211_STA_RX_BW_20));
}

/*
 * Return true if VHT 160 rates can be transmitted to the given node.
 *
 * This verifies that the BSS is VHT80+80 or VHT160 capable and the current
 * node channel width is 80+80MHz or 160MHz.
 */
static bool
ieee80211_vht_check_tx_vht160(const struct ieee80211_node *ni)
{
	struct ieee80211vap *vap;
	struct ieee80211_channel *bss_chan;

	if (!ieee80211_vht_check_tx_vht(ni))
		return (false);

	vap = ni->ni_vap;
	bss_chan = vap->iv_bss->ni_chan;

	/*
	 * ni_chw represents 20MHz or 40MHz from the HT
	 * TX width action frame / HT channel negotiation.
	 * If a HT TX width action frame sets it to 20MHz
	 * then reject doing 160MHz.
	 */
	if (ni->ni_chw == NET80211_STA_RX_BW_20)
		return (false);

	if (IEEE80211_IS_CHAN_VHT160(bss_chan) &&
	    IEEE80211_IS_CHAN_VHT160(ni->ni_chan))
		return (true);

	if (IEEE80211_IS_CHAN_VHT80P80(bss_chan) &&
	    IEEE80211_IS_CHAN_VHT80P80(ni->ni_chan))
		return (true);

	return (false);
}

/**
 * @brief Check if the given transmit bandwidth is available to the given node
 *
 * This checks that the node and BSS both allow the given bandwidth,
 * and that the current node bandwidth (which can dynamically change)
 * also allows said bandwidth.
 *
 * This relies on the channels having the flags for the narrower
 * channels as well - eg a VHT160 channel will have the CHAN_VHT80,
 * CHAN_VHT40, CHAN_VHT flags also set.
 *
 * @param ni		the ieee80211_node to check
 * @param bw		the required bandwidth to check
 *
 * @returns true if it is allowed, false otherwise
 */
bool
ieee80211_vht_check_tx_bw(const struct ieee80211_node *ni,
    enum net80211_sta_rx_bw bw)
{

	switch (bw) {
	case NET80211_STA_RX_BW_20:
		return (ieee80211_vht_check_tx_vht(ni));
	case NET80211_STA_RX_BW_40:
		return (ieee80211_vht_check_tx_vht40(ni));
	case NET80211_STA_RX_BW_80:
		return (ieee80211_vht_check_tx_vht80(ni));
	case NET80211_STA_RX_BW_160:
		return (ieee80211_vht_check_tx_vht160(ni));
	case NET80211_STA_RX_BW_320:
		return (false);
	default:
		return (false);
	}
}

/**
 * @brief Check if the given VHT bw/nss/mcs combination is valid
 *        for the give node.
 *
 * This checks whether the given VHT bw/nss/mcs is valid based on
 * the negotiated rate mask in the node.
 *
 * @param ni	struct ieee80211_node node to check
 * @param bw	channel bandwidth to check
 * @param nss	NSS
 * @param mcs	MCS
 * @returns True if this combination is available, false otherwise.
 */
bool
ieee80211_vht_node_check_tx_valid_mcs(const struct ieee80211_node *ni,
    enum net80211_sta_rx_bw bw, uint8_t nss, uint8_t mcs)
{
	uint8_t mc;

	/* Validate arguments */
	if (nss < 1 || nss > 8)
		return (false);
	if (mcs > 9)
		return (false);

	/* Check our choice of rate is actually valid */
	if (!ieee80211_phy_vht_validate_mcs(bw, nss, mcs))
		return (false);

	/*
	 * Next, check if the MCS rate is available for the
	 * given NSS.
	 */
	mc = ni->ni_vht_tx_map >> (2*(nss-1)) & 0x3;
	switch (mc) {
	case IEEE80211_VHT_MCS_NOT_SUPPORTED:
		/* Not supported at this NSS */
		return (false);
	case IEEE80211_VHT_MCS_SUPPORT_0_9:
		return (mcs <= 9);
	case IEEE80211_VHT_MCS_SUPPORT_0_8:
		return (mcs <= 8);
	case IEEE80211_VHT_MCS_SUPPORT_0_7:
		return (mcs <= 7);
	default:
		return (false);
	}
}
