/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * Copyright (c) 2012 IEEE
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

#include <sys/cdefs.h>
/*
 * IEEE 802.11 protocol support.
 */

#include "opt_inet.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_private.h>
#include <net/ethernet.h>		/* XXX for ether_sprintf */

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_adhoc.h>
#include <net80211/ieee80211_sta.h>
#include <net80211/ieee80211_hostap.h>
#include <net80211/ieee80211_wds.h>
#ifdef IEEE80211_SUPPORT_MESH
#include <net80211/ieee80211_mesh.h>
#endif
#include <net80211/ieee80211_monitor.h>
#include <net80211/ieee80211_input.h>

/* XXX tunables */
#define	AGGRESSIVE_MODE_SWITCH_HYSTERESIS	3	/* pkts / 100ms */
#define	HIGH_PRI_SWITCH_THRESH			10	/* pkts / 100ms */

const char *mgt_subtype_name[] = {
	"assoc_req",	"assoc_resp",	"reassoc_req",	"reassoc_resp",
	"probe_req",	"probe_resp",	"timing_adv",	"reserved#7",
	"beacon",	"atim",		"disassoc",	"auth",
	"deauth",	"action",	"action_noack",	"reserved#15"
};
const char *ctl_subtype_name[] = {
	"reserved#0",	"reserved#1",	"reserved#2",	"reserved#3",
	"reserved#4",	"reserved#5",	"reserved#6",	"control_wrap",
	"bar",		"ba",		"ps_poll",	"rts",
	"cts",		"ack",		"cf_end",	"cf_end_ack"
};
const char *ieee80211_opmode_name[IEEE80211_OPMODE_MAX] = {
	"IBSS",		/* IEEE80211_M_IBSS */
	"STA",		/* IEEE80211_M_STA */
	"WDS",		/* IEEE80211_M_WDS */
	"AHDEMO",	/* IEEE80211_M_AHDEMO */
	"HOSTAP",	/* IEEE80211_M_HOSTAP */
	"MONITOR",	/* IEEE80211_M_MONITOR */
	"MBSS"		/* IEEE80211_M_MBSS */
};
const char *ieee80211_state_name[IEEE80211_S_MAX] = {
	"INIT",		/* IEEE80211_S_INIT */
	"SCAN",		/* IEEE80211_S_SCAN */
	"AUTH",		/* IEEE80211_S_AUTH */
	"ASSOC",	/* IEEE80211_S_ASSOC */
	"CAC",		/* IEEE80211_S_CAC */
	"RUN",		/* IEEE80211_S_RUN */
	"CSA",		/* IEEE80211_S_CSA */
	"SLEEP",	/* IEEE80211_S_SLEEP */
};
const char *ieee80211_wme_acnames[] = {
	"WME_AC_BE",
	"WME_AC_BK",
	"WME_AC_VI",
	"WME_AC_VO",
	"WME_UPSD",
};

/*
 * Reason code descriptions were (mostly) obtained from
 * IEEE Std 802.11-2012, pp. 442-445 Table 8-36.
 */
const char *
ieee80211_reason_to_string(uint16_t reason)
{
	switch (reason) {
	case IEEE80211_REASON_UNSPECIFIED:
		return ("unspecified");
	case IEEE80211_REASON_AUTH_EXPIRE:
		return ("previous authentication is expired");
	case IEEE80211_REASON_AUTH_LEAVE:
		return ("sending STA is leaving/has left IBSS or ESS");
	case IEEE80211_REASON_ASSOC_EXPIRE:
		return ("disassociated due to inactivity");
	case IEEE80211_REASON_ASSOC_TOOMANY:
		return ("too many associated STAs");
	case IEEE80211_REASON_NOT_AUTHED:
		return ("class 2 frame received from nonauthenticated STA");
	case IEEE80211_REASON_NOT_ASSOCED:
		return ("class 3 frame received from nonassociated STA");
	case IEEE80211_REASON_ASSOC_LEAVE:
		return ("sending STA is leaving/has left BSS");
	case IEEE80211_REASON_ASSOC_NOT_AUTHED:
		return ("STA requesting (re)association is not authenticated");
	case IEEE80211_REASON_DISASSOC_PWRCAP_BAD:
		return ("information in the Power Capability element is "
			"unacceptable");
	case IEEE80211_REASON_DISASSOC_SUPCHAN_BAD:
		return ("information in the Supported Channels element is "
			"unacceptable");
	case IEEE80211_REASON_IE_INVALID:
		return ("invalid element");
	case IEEE80211_REASON_MIC_FAILURE:
		return ("MIC failure");
	case IEEE80211_REASON_4WAY_HANDSHAKE_TIMEOUT:
		return ("4-Way handshake timeout");
	case IEEE80211_REASON_GROUP_KEY_UPDATE_TIMEOUT:
		return ("group key update timeout");
	case IEEE80211_REASON_IE_IN_4WAY_DIFFERS:
		return ("element in 4-Way handshake different from "
			"(re)association request/probe response/beacon frame");
	case IEEE80211_REASON_GROUP_CIPHER_INVALID:
		return ("invalid group cipher");
	case IEEE80211_REASON_PAIRWISE_CIPHER_INVALID:
		return ("invalid pairwise cipher");
	case IEEE80211_REASON_AKMP_INVALID:
		return ("invalid AKMP");
	case IEEE80211_REASON_UNSUPP_RSN_IE_VERSION:
		return ("unsupported version in RSN IE");
	case IEEE80211_REASON_INVALID_RSN_IE_CAP:
		return ("invalid capabilities in RSN IE");
	case IEEE80211_REASON_802_1X_AUTH_FAILED:
		return ("IEEE 802.1X authentication failed");
	case IEEE80211_REASON_CIPHER_SUITE_REJECTED:
		return ("cipher suite rejected because of the security "
			"policy");
	case IEEE80211_REASON_UNSPECIFIED_QOS:
		return ("unspecified (QoS-related)");
	case IEEE80211_REASON_INSUFFICIENT_BW:
		return ("QoS AP lacks sufficient bandwidth for this QoS STA");
	case IEEE80211_REASON_TOOMANY_FRAMES:
		return ("too many frames need to be acknowledged");
	case IEEE80211_REASON_OUTSIDE_TXOP:
		return ("STA is transmitting outside the limits of its TXOPs");
	case IEEE80211_REASON_LEAVING_QBSS:
		return ("requested from peer STA (the STA is "
			"resetting/leaving the BSS)");
	case IEEE80211_REASON_BAD_MECHANISM:
		return ("requested from peer STA (it does not want to use "
			"the mechanism)");
	case IEEE80211_REASON_SETUP_NEEDED:
		return ("requested from peer STA (setup is required for the "
			"used mechanism)");
	case IEEE80211_REASON_TIMEOUT:
		return ("requested from peer STA (timeout)");
	case IEEE80211_REASON_PEER_LINK_CANCELED:
		return ("SME cancels the mesh peering instance (not related "
			"to the maximum number of peer mesh STAs)");
	case IEEE80211_REASON_MESH_MAX_PEERS:
		return ("maximum number of peer mesh STAs was reached");
	case IEEE80211_REASON_MESH_CPVIOLATION:
		return ("the received information violates the Mesh "
			"Configuration policy configured in the mesh STA "
			"profile");
	case IEEE80211_REASON_MESH_CLOSE_RCVD:
		return ("the mesh STA has received a Mesh Peering Close "
			"message requesting to close the mesh peering");
	case IEEE80211_REASON_MESH_MAX_RETRIES:
		return ("the mesh STA has resent dot11MeshMaxRetries Mesh "
			"Peering Open messages, without receiving a Mesh "
			"Peering Confirm message");
	case IEEE80211_REASON_MESH_CONFIRM_TIMEOUT:
		return ("the confirmTimer for the mesh peering instance times "
			"out");
	case IEEE80211_REASON_MESH_INVALID_GTK:
		return ("the mesh STA fails to unwrap the GTK or the values "
			"in the wrapped contents do not match");
	case IEEE80211_REASON_MESH_INCONS_PARAMS:
		return ("the mesh STA receives inconsistent information about "
			"the mesh parameters between Mesh Peering Management "
			"frames");
	case IEEE80211_REASON_MESH_INVALID_SECURITY:
		return ("the mesh STA fails the authenticated mesh peering "
			"exchange because due to failure in selecting "
			"pairwise/group ciphersuite");
	case IEEE80211_REASON_MESH_PERR_NO_PROXY:
		return ("the mesh STA does not have proxy information for "
			"this external destination");
	case IEEE80211_REASON_MESH_PERR_NO_FI:
		return ("the mesh STA does not have forwarding information "
			"for this destination");
	case IEEE80211_REASON_MESH_PERR_DEST_UNREACH:
		return ("the mesh STA determines that the link to the next "
			"hop of an active path in its forwarding information "
			"is no longer usable");
	case IEEE80211_REASON_MESH_MAC_ALRDY_EXISTS_MBSS:
		return ("the MAC address of the STA already exists in the "
			"mesh BSS");
	case IEEE80211_REASON_MESH_CHAN_SWITCH_REG:
		return ("the mesh STA performs channel switch to meet "
			"regulatory requirements");
	case IEEE80211_REASON_MESH_CHAN_SWITCH_UNSPEC:
		return ("the mesh STA performs channel switch with "
			"unspecified reason");
	default:
		return ("reserved/unknown");
	}
}

static void beacon_miss(void *, int);
static void beacon_swmiss(void *, int);
static void parent_updown(void *, int);
static void update_mcast(void *, int);
static void update_promisc(void *, int);
static void update_channel(void *, int);
static void update_chw(void *, int);
static void vap_update_wme(void *, int);
static void vap_update_slot(void *, int);
static void restart_vaps(void *, int);
static void vap_update_erp_protmode(void *, int);
static void vap_update_preamble(void *, int);
static void vap_update_ht_protmode(void *, int);
static void ieee80211_newstate_cb(void *, int);
static struct ieee80211_node *vap_update_bss(struct ieee80211vap *,
    struct ieee80211_node *);

static int
null_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{

	ic_printf(ni->ni_ic, "missing ic_raw_xmit callback, drop frame\n");
	m_freem(m);
	return ENETDOWN;
}

void
ieee80211_proto_attach(struct ieee80211com *ic)
{
	uint8_t hdrlen;

	/* override the 802.3 setting */
	hdrlen = ic->ic_headroom
		+ sizeof(struct ieee80211_qosframe_addr4)
		+ IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN
		+ IEEE80211_WEP_EXTIVLEN;
	/* XXX no way to recalculate on ifdetach */
	max_linkhdr_grow(ALIGN(hdrlen));
	//ic->ic_protmode = IEEE80211_PROT_CTSONLY;

	TASK_INIT(&ic->ic_parent_task, 0, parent_updown, ic);
	TASK_INIT(&ic->ic_mcast_task, 0, update_mcast, ic);
	TASK_INIT(&ic->ic_promisc_task, 0, update_promisc, ic);
	TASK_INIT(&ic->ic_chan_task, 0, update_channel, ic);
	TASK_INIT(&ic->ic_bmiss_task, 0, beacon_miss, ic);
	TASK_INIT(&ic->ic_chw_task, 0, update_chw, ic);
	TASK_INIT(&ic->ic_restart_task, 0, restart_vaps, ic);

	ic->ic_wme.wme_hipri_switch_hysteresis =
		AGGRESSIVE_MODE_SWITCH_HYSTERESIS;

	/* initialize management frame handlers */
	ic->ic_send_mgmt = ieee80211_send_mgmt;
	ic->ic_raw_xmit = null_raw_xmit;

	ieee80211_adhoc_attach(ic);
	ieee80211_sta_attach(ic);
	ieee80211_wds_attach(ic);
	ieee80211_hostap_attach(ic);
#ifdef IEEE80211_SUPPORT_MESH
	ieee80211_mesh_attach(ic);
#endif
	ieee80211_monitor_attach(ic);
}

void
ieee80211_proto_detach(struct ieee80211com *ic)
{
	ieee80211_monitor_detach(ic);
#ifdef IEEE80211_SUPPORT_MESH
	ieee80211_mesh_detach(ic);
#endif
	ieee80211_hostap_detach(ic);
	ieee80211_wds_detach(ic);
	ieee80211_adhoc_detach(ic);
	ieee80211_sta_detach(ic);
}

static void
null_update_beacon(struct ieee80211vap *vap, int item)
{
}

void
ieee80211_proto_vattach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ifnet *ifp = vap->iv_ifp;
	int i;

	/* override the 802.3 setting */
	ifp->if_hdrlen = ic->ic_headroom
                + sizeof(struct ieee80211_qosframe_addr4)
                + IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN
                + IEEE80211_WEP_EXTIVLEN;

	vap->iv_rtsthreshold = IEEE80211_RTS_DEFAULT;
	vap->iv_fragthreshold = IEEE80211_FRAG_DEFAULT;
	vap->iv_bmiss_max = IEEE80211_BMISS_MAX;
	callout_init_mtx(&vap->iv_swbmiss, IEEE80211_LOCK_OBJ(ic), 0);
	callout_init(&vap->iv_mgtsend, 1);
	for (i = 0; i < NET80211_IV_NSTATE_NUM; i++)
		TASK_INIT(&vap->iv_nstate_task[i], 0, ieee80211_newstate_cb, vap);
	TASK_INIT(&vap->iv_swbmiss_task, 0, beacon_swmiss, vap);
	TASK_INIT(&vap->iv_wme_task, 0, vap_update_wme, vap);
	TASK_INIT(&vap->iv_slot_task, 0, vap_update_slot, vap);
	TASK_INIT(&vap->iv_erp_protmode_task, 0, vap_update_erp_protmode, vap);
	TASK_INIT(&vap->iv_ht_protmode_task, 0, vap_update_ht_protmode, vap);
	TASK_INIT(&vap->iv_preamble_task, 0, vap_update_preamble, vap);
	/*
	 * Install default tx rate handling: no fixed rate, lowest
	 * supported rate for mgmt and multicast frames.  Default
	 * max retry count.  These settings can be changed by the
	 * driver and/or user applications.
	 */
	for (i = IEEE80211_MODE_11A; i < IEEE80211_MODE_MAX; i++) {
		if (isclr(ic->ic_modecaps, i))
			continue;

		const struct ieee80211_rateset *rs = &ic->ic_sup_rates[i];

		vap->iv_txparms[i].ucastrate = IEEE80211_FIXED_RATE_NONE;

		/*
		 * Setting the management rate to MCS 0 assumes that the
		 * BSS Basic rate set is empty and the BSS Basic MCS set
		 * is not.
		 *
		 * Since we're not checking this, default to the lowest
		 * defined rate for this mode.
		 *
		 * At least one 11n AP (DLINK DIR-825) is reported to drop
		 * some MCS management traffic (eg BA response frames.)
		 *
		 * See also: 9.6.0 of the 802.11n-2009 specification.
		 */
#ifdef	NOTYET
		if (i == IEEE80211_MODE_11NA || i == IEEE80211_MODE_11NG) {
			vap->iv_txparms[i].mgmtrate = 0 | IEEE80211_RATE_MCS;
			vap->iv_txparms[i].mcastrate = 0 | IEEE80211_RATE_MCS;
		} else {
			vap->iv_txparms[i].mgmtrate =
			    rs->rs_rates[0] & IEEE80211_RATE_VAL;
			vap->iv_txparms[i].mcastrate = 
			    rs->rs_rates[0] & IEEE80211_RATE_VAL;
		}
#endif
		vap->iv_txparms[i].mgmtrate = rs->rs_rates[0] & IEEE80211_RATE_VAL;
		vap->iv_txparms[i].mcastrate = rs->rs_rates[0] & IEEE80211_RATE_VAL;
		vap->iv_txparms[i].maxretry = IEEE80211_TXMAX_DEFAULT;
	}
	vap->iv_roaming = IEEE80211_ROAMING_AUTO;

	vap->iv_update_beacon = null_update_beacon;
	vap->iv_deliver_data = ieee80211_deliver_data;
	vap->iv_protmode = IEEE80211_PROT_CTSONLY;
	vap->iv_update_bss = vap_update_bss;

	/* attach support for operating mode */
	ic->ic_vattach[vap->iv_opmode](vap);
}

void
ieee80211_proto_vdetach(struct ieee80211vap *vap)
{
#define	FREEAPPIE(ie) do { \
	if (ie != NULL) \
		IEEE80211_FREE(ie, M_80211_NODE_IE); \
} while (0)
	/*
	 * Detach operating mode module.
	 */
	if (vap->iv_opdetach != NULL)
		vap->iv_opdetach(vap);
	/*
	 * This should not be needed as we detach when reseting
	 * the state but be conservative here since the
	 * authenticator may do things like spawn kernel threads.
	 */
	if (vap->iv_auth->ia_detach != NULL)
		vap->iv_auth->ia_detach(vap);
	/*
	 * Detach any ACL'ator.
	 */
	if (vap->iv_acl != NULL)
		vap->iv_acl->iac_detach(vap);

	FREEAPPIE(vap->iv_appie_beacon);
	FREEAPPIE(vap->iv_appie_probereq);
	FREEAPPIE(vap->iv_appie_proberesp);
	FREEAPPIE(vap->iv_appie_assocreq);
	FREEAPPIE(vap->iv_appie_assocresp);
	FREEAPPIE(vap->iv_appie_wpa);
#undef FREEAPPIE
}

/*
 * Simple-minded authenticator module support.
 */

#define	IEEE80211_AUTH_MAX	(IEEE80211_AUTH_WPA+1)
/* XXX well-known names */
static const char *auth_modnames[IEEE80211_AUTH_MAX] = {
	"wlan_internal",	/* IEEE80211_AUTH_NONE */
	"wlan_internal",	/* IEEE80211_AUTH_OPEN */
	"wlan_internal",	/* IEEE80211_AUTH_SHARED */
	"wlan_xauth",		/* IEEE80211_AUTH_8021X	 */
	"wlan_internal",	/* IEEE80211_AUTH_AUTO */
	"wlan_xauth",		/* IEEE80211_AUTH_WPA */
};
static const struct ieee80211_authenticator *authenticators[IEEE80211_AUTH_MAX];

static const struct ieee80211_authenticator auth_internal = {
	.ia_name		= "wlan_internal",
	.ia_attach		= NULL,
	.ia_detach		= NULL,
	.ia_node_join		= NULL,
	.ia_node_leave		= NULL,
};

/*
 * Setup internal authenticators once; they are never unregistered.
 */
static void
ieee80211_auth_setup(void)
{
	ieee80211_authenticator_register(IEEE80211_AUTH_OPEN, &auth_internal);
	ieee80211_authenticator_register(IEEE80211_AUTH_SHARED, &auth_internal);
	ieee80211_authenticator_register(IEEE80211_AUTH_AUTO, &auth_internal);
}
SYSINIT(wlan_auth, SI_SUB_DRIVERS, SI_ORDER_FIRST, ieee80211_auth_setup, NULL);

const struct ieee80211_authenticator *
ieee80211_authenticator_get(int auth)
{
	if (auth >= IEEE80211_AUTH_MAX)
		return NULL;
	if (authenticators[auth] == NULL)
		ieee80211_load_module(auth_modnames[auth]);
	return authenticators[auth];
}

void
ieee80211_authenticator_register(int type,
	const struct ieee80211_authenticator *auth)
{
	if (type >= IEEE80211_AUTH_MAX)
		return;
	authenticators[type] = auth;
}

void
ieee80211_authenticator_unregister(int type)
{

	if (type >= IEEE80211_AUTH_MAX)
		return;
	authenticators[type] = NULL;
}

/*
 * Very simple-minded ACL module support.
 */
/* XXX just one for now */
static	const struct ieee80211_aclator *acl = NULL;

void
ieee80211_aclator_register(const struct ieee80211_aclator *iac)
{
	net80211_printf("wlan: %s acl policy registered\n", iac->iac_name);
	acl = iac;
}

void
ieee80211_aclator_unregister(const struct ieee80211_aclator *iac)
{
	if (acl == iac)
		acl = NULL;
	net80211_printf("wlan: %s acl policy unregistered\n", iac->iac_name);
}

const struct ieee80211_aclator *
ieee80211_aclator_get(const char *name)
{
	if (acl == NULL)
		ieee80211_load_module("wlan_acl");
	return acl != NULL && strcmp(acl->iac_name, name) == 0 ? acl : NULL;
}

void
ieee80211_print_essid(const uint8_t *essid, int len)
{
	const uint8_t *p;
	int i;

	if (len > IEEE80211_NWID_LEN)
		len = IEEE80211_NWID_LEN;
	/* determine printable or not */
	for (i = 0, p = essid; i < len; i++, p++) {
		if (*p < ' ' || *p > 0x7e)
			break;
	}
	if (i == len) {
		net80211_printf("\"");
		for (i = 0, p = essid; i < len; i++, p++)
			net80211_printf("%c", *p);
		net80211_printf("\"");
	} else {
		net80211_printf("0x");
		for (i = 0, p = essid; i < len; i++, p++)
			net80211_printf("%02x", *p);
	}
}

void
ieee80211_dump_pkt(struct ieee80211com *ic,
	const uint8_t *buf, int len, int rate, int rssi)
{
	const struct ieee80211_frame *wh;
	int i;

	wh = (const struct ieee80211_frame *)buf;
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		net80211_printf("NODS %s", ether_sprintf(wh->i_addr2));
		net80211_printf("->%s", ether_sprintf(wh->i_addr1));
		net80211_printf("(%s)", ether_sprintf(wh->i_addr3));
		break;
	case IEEE80211_FC1_DIR_TODS:
		net80211_printf("TODS %s", ether_sprintf(wh->i_addr2));
		net80211_printf("->%s", ether_sprintf(wh->i_addr3));
		net80211_printf("(%s)", ether_sprintf(wh->i_addr1));
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		net80211_printf("FRDS %s", ether_sprintf(wh->i_addr3));
		net80211_printf("->%s", ether_sprintf(wh->i_addr1));
		net80211_printf("(%s)", ether_sprintf(wh->i_addr2));
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		net80211_printf("DSDS %s", ether_sprintf((const uint8_t *)&wh[1]));
		net80211_printf("->%s", ether_sprintf(wh->i_addr3));
		net80211_printf("(%s", ether_sprintf(wh->i_addr2));
		net80211_printf("->%s)", ether_sprintf(wh->i_addr1));
		break;
	}
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_DATA:
		net80211_printf(" data");
		break;
	case IEEE80211_FC0_TYPE_MGT:
		net80211_printf(" %s", ieee80211_mgt_subtype_name(wh->i_fc[0]));
		break;
	default:
		net80211_printf(" type#%d", wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);
		break;
	}
	if (IEEE80211_QOS_HAS_SEQ(wh)) {
		const struct ieee80211_qosframe *qwh = 
			(const struct ieee80211_qosframe *)buf;
		net80211_printf(" QoS [TID %u%s]", qwh->i_qos[0] & IEEE80211_QOS_TID,
			qwh->i_qos[0] & IEEE80211_QOS_ACKPOLICY ? " ACM" : "");
	}
	if (IEEE80211_IS_PROTECTED(wh)) {
		int off;

		off = ieee80211_anyhdrspace(ic, wh);
		net80211_printf(" WEP [IV %.02x %.02x %.02x",
			buf[off+0], buf[off+1], buf[off+2]);
		if (buf[off+IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV)
			net80211_printf(" %.02x %.02x %.02x",
				buf[off+4], buf[off+5], buf[off+6]);
		net80211_printf(" KID %u]", buf[off+IEEE80211_WEP_IVLEN] >> 6);
	}
	if (rate >= 0)
		net80211_printf(" %dM", rate / 2);
	if (rssi >= 0)
		net80211_printf(" +%d", rssi);
	net80211_printf("\n");
	if (len > 0) {
		for (i = 0; i < len; i++) {
			if ((i & 1) == 0)
				net80211_printf(" ");
			net80211_printf("%02x", buf[i]);
		}
		net80211_printf("\n");
	}
}

static __inline int
findrix(const struct ieee80211_rateset *rs, int r)
{
	int i;

	for (i = 0; i < rs->rs_nrates; i++)
		if ((rs->rs_rates[i] & IEEE80211_RATE_VAL) == r)
			return i;
	return -1;
}

int
ieee80211_fix_rate(struct ieee80211_node *ni,
	struct ieee80211_rateset *nrs, int flags)
{
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211com *ic = ni->ni_ic;
	int i, j, rix, error;
	int okrate, badrate, fixedrate, ucastrate;
	const struct ieee80211_rateset *srs;
	uint8_t r;

	error = 0;
	okrate = badrate = 0;
	ucastrate = vap->iv_txparms[ieee80211_chan2mode(ni->ni_chan)].ucastrate;
	if (ucastrate != IEEE80211_FIXED_RATE_NONE) {
		/*
		 * Workaround awkwardness with fixed rate.  We are called
		 * to check both the legacy rate set and the HT rate set
		 * but we must apply any legacy fixed rate check only to the
		 * legacy rate set and vice versa.  We cannot tell what type
		 * of rate set we've been given (legacy or HT) but we can
		 * distinguish the fixed rate type (MCS have 0x80 set).
		 * So to deal with this the caller communicates whether to
		 * check MCS or legacy rate using the flags and we use the
		 * type of any fixed rate to avoid applying an MCS to a
		 * legacy rate and vice versa.
		 */
		if (ucastrate & 0x80) {
			if (flags & IEEE80211_F_DOFRATE)
				flags &= ~IEEE80211_F_DOFRATE;
		} else if ((ucastrate & 0x80) == 0) {
			if (flags & IEEE80211_F_DOFMCS)
				flags &= ~IEEE80211_F_DOFMCS;
		}
		/* NB: required to make MCS match below work */
		ucastrate &= IEEE80211_RATE_VAL;
	}
	fixedrate = IEEE80211_FIXED_RATE_NONE;
	/*
	 * XXX we are called to process both MCS and legacy rates;
	 * we must use the appropriate basic rate set or chaos will
	 * ensue; for now callers that want MCS must supply
	 * IEEE80211_F_DOBRS; at some point we'll need to split this
	 * function so there are two variants, one for MCS and one
	 * for legacy rates.
	 */
	if (flags & IEEE80211_F_DOBRS)
		srs = (const struct ieee80211_rateset *)
		    ieee80211_get_suphtrates(ic, ni->ni_chan);
	else
		srs = ieee80211_get_suprates(ic, ni->ni_chan);
	for (i = 0; i < nrs->rs_nrates; ) {
		if (flags & IEEE80211_F_DOSORT) {
			/*
			 * Sort rates.
			 */
			for (j = i + 1; j < nrs->rs_nrates; j++) {
				if (IEEE80211_RV(nrs->rs_rates[i]) >
				    IEEE80211_RV(nrs->rs_rates[j])) {
					r = nrs->rs_rates[i];
					nrs->rs_rates[i] = nrs->rs_rates[j];
					nrs->rs_rates[j] = r;
				}
			}
		}
		r = nrs->rs_rates[i] & IEEE80211_RATE_VAL;
		badrate = r;
		/*
		 * Check for fixed rate.
		 */
		if (r == ucastrate)
			fixedrate = r;
		/*
		 * Check against supported rates.
		 */
		rix = findrix(srs, r);
		if (flags & IEEE80211_F_DONEGO) {
			if (rix < 0) {
				/*
				 * A rate in the node's rate set is not
				 * supported.  If this is a basic rate and we
				 * are operating as a STA then this is an error.
				 * Otherwise we just discard/ignore the rate.
				 */
				if ((flags & IEEE80211_F_JOIN) &&
				    (nrs->rs_rates[i] & IEEE80211_RATE_BASIC))
					error++;
			} else if ((flags & IEEE80211_F_JOIN) == 0) {
				/*
				 * Overwrite with the supported rate
				 * value so any basic rate bit is set.
				 */
				nrs->rs_rates[i] = srs->rs_rates[rix];
			}
		}
		if ((flags & IEEE80211_F_DODEL) && rix < 0) {
			/*
			 * Delete unacceptable rates.
			 */
			nrs->rs_nrates--;
			for (j = i; j < nrs->rs_nrates; j++)
				nrs->rs_rates[j] = nrs->rs_rates[j + 1];
			nrs->rs_rates[j] = 0;
			continue;
		}
		if (rix >= 0)
			okrate = nrs->rs_rates[i];
		i++;
	}
	if (okrate == 0 || error != 0 ||
	    ((flags & (IEEE80211_F_DOFRATE|IEEE80211_F_DOFMCS)) &&
	     fixedrate != ucastrate)) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_XRATE | IEEE80211_MSG_11N, ni,
		    "%s: flags 0x%x okrate %d error %d fixedrate 0x%x "
		    "ucastrate %x\n", __func__, fixedrate, ucastrate, flags);
		return badrate | IEEE80211_RATE_BASIC;
	} else
		return IEEE80211_RV(okrate);
}

/*
 * Reset 11g-related state.
 *
 * This is for per-VAP ERP/11g state.
 *
 * Eventually everything in ieee80211_reset_erp() will be
 * per-VAP and in here.
 */
void
ieee80211_vap_reset_erp(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	vap->iv_nonerpsta = 0;
	vap->iv_longslotsta = 0;

	vap->iv_flags &= ~IEEE80211_F_USEPROT;
	/*
	 * Set short preamble and ERP barker-preamble flags.
	 */
	if (IEEE80211_IS_CHAN_A(ic->ic_curchan) ||
	    (vap->iv_caps & IEEE80211_C_SHPREAMBLE)) {
		vap->iv_flags |= IEEE80211_F_SHPREAMBLE;
		vap->iv_flags &= ~IEEE80211_F_USEBARKER;
	} else {
		vap->iv_flags &= ~IEEE80211_F_SHPREAMBLE;
		vap->iv_flags |= IEEE80211_F_USEBARKER;
	}

	/*
	 * Short slot time is enabled only when operating in 11g
	 * and not in an IBSS.  We must also honor whether or not
	 * the driver is capable of doing it.
	 */
	ieee80211_vap_set_shortslottime(vap,
		IEEE80211_IS_CHAN_A(ic->ic_curchan) ||
		IEEE80211_IS_CHAN_HT(ic->ic_curchan) ||
		(IEEE80211_IS_CHAN_ANYG(ic->ic_curchan) &&
		vap->iv_opmode == IEEE80211_M_HOSTAP &&
		(ic->ic_caps & IEEE80211_C_SHSLOT)));
}

/*
 * Reset 11g-related state.
 *
 * Note this resets the global state and a caller should schedule
 * a re-check of all the VAPs after setup to update said state.
 */
void
ieee80211_reset_erp(struct ieee80211com *ic)
{
#if 0
	ic->ic_flags &= ~IEEE80211_F_USEPROT;
	/*
	 * Set short preamble and ERP barker-preamble flags.
	 */
	if (IEEE80211_IS_CHAN_A(ic->ic_curchan) ||
	    (ic->ic_caps & IEEE80211_C_SHPREAMBLE)) {
		ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
		ic->ic_flags &= ~IEEE80211_F_USEBARKER;
	} else {
		ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
		ic->ic_flags |= IEEE80211_F_USEBARKER;
	}
#endif
	/* XXX TODO: schedule a new per-VAP ERP calculation */
}

static struct ieee80211_node *
vap_update_bss(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211_node *obss;

	IEEE80211_LOCK_ASSERT(vap->iv_ic);

	obss = vap->iv_bss;
	vap->iv_bss = ni;

	return (obss);
}

/*
 * Deferred slot time update.
 *
 * For per-VAP slot time configuration, call the VAP
 * method if the VAP requires it.  Otherwise, just call the
 * older global method.
 *
 * If the per-VAP method is called then it's expected that
 * the driver/firmware will take care of turning the per-VAP
 * flags into slot time configuration.
 *
 * If the per-VAP method is not called then the global flags will be
 * flipped into sync with the VAPs; ic_flags IEEE80211_F_SHSLOT will
 * be set only if all of the vaps will have it set.
 *
 * Look at the comments for vap_update_erp_protmode() for more
 * background; this assumes all VAPs are on the same channel.
 */
static void
vap_update_slot(void *arg, int npending)
{
	struct ieee80211vap *vap = arg;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211vap *iv;
	int num_shslot = 0, num_lgslot = 0;

	/*
	 * Per-VAP path - we've already had the flags updated;
	 * so just notify the driver and move on.
	 */
	if (vap->iv_updateslot != NULL) {
		vap->iv_updateslot(vap);
		return;
	}

	/*
	 * Iterate over all of the VAP flags to update the
	 * global flag.
	 *
	 * If all vaps have short slot enabled then flip on
	 * short slot.  If any vap has it disabled then
	 * we leave it globally disabled.  This should provide
	 * correct behaviour in a multi-BSS scenario where
	 * at least one VAP has short slot disabled for some
	 * reason.
	 */
	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(iv, &ic->ic_vaps, iv_next) {
		if (iv->iv_flags & IEEE80211_F_SHSLOT)
			num_shslot++;
		else
			num_lgslot++;
	}

	/*
	 * It looks backwards but - if the number of short slot VAPs
	 * is zero then we're not short slot.  Else, we have one
	 * or more short slot VAPs and we're checking to see if ANY
	 * of them have short slot disabled.
	 */
	if (num_shslot == 0)
		ic->ic_flags &= ~IEEE80211_F_SHSLOT;
	else if (num_lgslot == 0)
		ic->ic_flags |= IEEE80211_F_SHSLOT;
	IEEE80211_UNLOCK(ic);

	/*
	 * Call the driver with our new global slot time flags.
	 */
	if (ic->ic_updateslot != NULL)
		ic->ic_updateslot(ic);
}

/*
 * Deferred ERP protmode update.
 *
 * This currently calculates the global ERP protection mode flag
 * based on each of the VAPs.  Any VAP with it enabled is enough
 * for the global flag to be enabled.  All VAPs with it disabled
 * is enough for it to be disabled.
 *
 * This may make sense right now for the supported hardware where
 * net80211 is controlling the single channel configuration, but
 * offload firmware that's doing channel changes (eg off-channel
 * TDLS, off-channel STA, off-channel P2P STA/AP) may get some
 * silly looking flag updates.
 *
 * Ideally the protection mode calculation is done based on the
 * channel, and all VAPs using that channel will inherit it.
 * But until that's what net80211 does, this wil have to do.
 */
static void
vap_update_erp_protmode(void *arg, int npending)
{
	struct ieee80211vap *vap = arg;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211vap *iv;
	int enable_protmode = 0;
	int non_erp_present = 0;

	/*
	 * Iterate over all of the VAPs to calculate the overlapping
	 * ERP protection mode configuration and ERP present math.
	 *
	 * For now we assume that if a driver can handle this per-VAP
	 * then it'll ignore the ic->ic_protmode variant and instead
	 * will look at the vap related flags.
	 */
	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(iv, &ic->ic_vaps, iv_next) {
		if (iv->iv_flags & IEEE80211_F_USEPROT)
			enable_protmode = 1;
		if (iv->iv_flags_ext & IEEE80211_FEXT_NONERP_PR)
			non_erp_present = 1;
	}

	if (enable_protmode)
		ic->ic_flags |= IEEE80211_F_USEPROT;
	else
		ic->ic_flags &= ~IEEE80211_F_USEPROT;

	if (non_erp_present)
		ic->ic_flags_ext |= IEEE80211_FEXT_NONERP_PR;
	else
		ic->ic_flags_ext &= ~IEEE80211_FEXT_NONERP_PR;

	/* Beacon update on all VAPs */
	ieee80211_notify_erp_locked(ic);

	IEEE80211_UNLOCK(ic);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
	    "%s: called; enable_protmode=%d, non_erp_present=%d\n",
	    __func__, enable_protmode, non_erp_present);

	/*
	 * Now that the global configuration flags are calculated,
	 * notify the VAP about its configuration.
	 *
	 * The global flags will be used when assembling ERP IEs
	 * for multi-VAP operation, even if it's on a different
	 * channel.  Yes, that's going to need fixing in the
	 * future.
	 */
	if (vap->iv_erp_protmode_update != NULL)
		vap->iv_erp_protmode_update(vap);
}

/*
 * Deferred ERP short preamble/barker update.
 *
 * All VAPs need to use short preamble for it to be globally
 * enabled or not.
 *
 * Look at the comments for vap_update_erp_protmode() for more
 * background; this assumes all VAPs are on the same channel.
 */
static void
vap_update_preamble(void *arg, int npending)
{
	struct ieee80211vap *vap = arg;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211vap *iv;
	int barker_count = 0, short_preamble_count = 0, count = 0;

	/*
	 * Iterate over all of the VAPs to calculate the overlapping
	 * short or long preamble configuration.
	 *
	 * For now we assume that if a driver can handle this per-VAP
	 * then it'll ignore the ic->ic_flags variant and instead
	 * will look at the vap related flags.
	 */
	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(iv, &ic->ic_vaps, iv_next) {
		if (iv->iv_flags & IEEE80211_F_USEBARKER)
			barker_count++;
		if (iv->iv_flags & IEEE80211_F_SHPREAMBLE)
			short_preamble_count++;
		count++;
	}

	/*
	 * As with vap_update_erp_protmode(), the global flags are
	 * currently used for beacon IEs.
	 */
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
	    "%s: called; barker_count=%d, short_preamble_count=%d\n",
	    __func__, barker_count, short_preamble_count);

	/*
	 * Only flip on short preamble if all of the VAPs support
	 * it.
	 */
	if (barker_count == 0 && short_preamble_count == count) {
		ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
		ic->ic_flags &= ~IEEE80211_F_USEBARKER;
	} else {
		ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
		ic->ic_flags |= IEEE80211_F_USEBARKER;
	}
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
	  "%s: global barker=%d preamble=%d\n",
	  __func__,
	  !! (ic->ic_flags & IEEE80211_F_USEBARKER),
	  !! (ic->ic_flags & IEEE80211_F_SHPREAMBLE));

	/* Beacon update on all VAPs */
	ieee80211_notify_erp_locked(ic);

	IEEE80211_UNLOCK(ic);

	/* Driver notification */
	if (vap->iv_preamble_update != NULL)
		vap->iv_preamble_update(vap);
}

/*
 * Deferred HT protmode update and beacon update.
 *
 * Look at the comments for vap_update_erp_protmode() for more
 * background; this assumes all VAPs are on the same channel.
 */
static void
vap_update_ht_protmode(void *arg, int npending)
{
	struct ieee80211vap *vap = arg;
	struct ieee80211vap *iv;
	struct ieee80211com *ic = vap->iv_ic;
	int num_vaps = 0, num_pure = 0;
	int num_optional = 0, num_ht2040 = 0, num_nonht = 0;
	int num_ht_sta = 0, num_ht40_sta = 0, num_sta = 0;
	int num_nonhtpr = 0;

	/*
	 * Iterate over all of the VAPs to calculate everything.
	 *
	 * There are a few different flags to calculate:
	 *
	 * + whether there's HT only or HT+legacy stations;
	 * + whether there's HT20, HT40, or HT20+HT40 stations;
	 * + whether the desired protection mode is mixed, pure or
	 *   one of the two above.
	 *
	 * For now we assume that if a driver can handle this per-VAP
	 * then it'll ignore the ic->ic_htprotmode / ic->ic_curhtprotmode
	 * variant and instead will look at the vap related variables.
	 *
	 * XXX TODO: non-greenfield STAs present (IEEE80211_HTINFO_NONGF_PRESENT) !
	 */

	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(iv, &ic->ic_vaps, iv_next) {
		num_vaps++;
		/* overlapping BSSes advertising non-HT status present */
		if (iv->iv_flags_ht & IEEE80211_FHT_NONHT_PR)
			num_nonht++;
		/* Operating mode flags */
		if (iv->iv_curhtprotmode & IEEE80211_HTINFO_NONHT_PRESENT)
			num_nonhtpr++;
		switch (iv->iv_curhtprotmode & IEEE80211_HTINFO_OPMODE) {
		case IEEE80211_HTINFO_OPMODE_PURE:
			num_pure++;
			break;
		case IEEE80211_HTINFO_OPMODE_PROTOPT:
			num_optional++;
			break;
		case IEEE80211_HTINFO_OPMODE_HT20PR:
			num_ht2040++;
			break;
		}

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_11N,
		    "%s: vap %s: nonht_pr=%d, curhtprotmode=0x%02x\n",
		    __func__,
		    ieee80211_get_vap_ifname(iv),
		    !! (iv->iv_flags_ht & IEEE80211_FHT_NONHT_PR),
		    iv->iv_curhtprotmode);

		num_ht_sta += iv->iv_ht_sta_assoc;
		num_ht40_sta += iv->iv_ht40_sta_assoc;
		num_sta += iv->iv_sta_assoc;
	}

	/*
	 * Step 1 - if any VAPs indicate NONHT_PR set (overlapping BSS
	 * non-HT present), set it here.  This shouldn't be used by
	 * anything but the old overlapping BSS logic so if any drivers
	 * consume it, it's up to date.
	 */
	if (num_nonht > 0)
		ic->ic_flags_ht |= IEEE80211_FHT_NONHT_PR;
	else
		ic->ic_flags_ht &= ~IEEE80211_FHT_NONHT_PR;

	/*
	 * Step 2 - default HT protection mode to MIXED (802.11-2016 10.26.3.1.)
	 *
	 * + If all VAPs are PURE, we can stay PURE.
	 * + If all VAPs are PROTOPT, we can go to PROTOPT.
	 * + If any VAP has HT20PR then it sees at least a HT40+HT20 station.
	 *   Note that we may have a VAP with one HT20 and a VAP with one HT40;
	 *   So we look at the sum ht and sum ht40 sta counts; if we have a
	 *   HT station and the HT20 != HT40 count, we have to do HT20PR here.
	 *   Note all stations need to be HT for this to be an option.
	 * + The fall-through is MIXED, because it means we have some odd
	 *   non HT40-involved combination of opmode and this is the most
	 *   sensible default.
	 */
	ic->ic_curhtprotmode = IEEE80211_HTINFO_OPMODE_MIXED;

	if (num_pure == num_vaps)
		ic->ic_curhtprotmode = IEEE80211_HTINFO_OPMODE_PURE;

	if (num_optional == num_vaps)
		ic->ic_curhtprotmode = IEEE80211_HTINFO_OPMODE_PROTOPT;

	/*
	 * Note: we need /a/ HT40 station somewhere for this to
	 * be a possibility.
	 */
	if ((num_ht2040 > 0) ||
	    ((num_ht_sta > 0) && (num_ht40_sta > 0) &&
	     (num_ht_sta != num_ht40_sta)))
		ic->ic_curhtprotmode = IEEE80211_HTINFO_OPMODE_HT20PR;

	/*
	 * Step 3 - if any of the stations across the VAPs are
	 * non-HT then this needs to be flipped back to MIXED.
	 */
	if (num_ht_sta != num_sta)
		ic->ic_curhtprotmode = IEEE80211_HTINFO_OPMODE_MIXED;

	/*
	 * Step 4 - If we see any overlapping BSS non-HT stations
	 * via beacons then flip on NONHT_PRESENT.
	 */
	if (num_nonhtpr > 0)
		ic->ic_curhtprotmode |= IEEE80211_HTINFO_NONHT_PRESENT;

	/* Notify all VAPs to potentially update their beacons */
	TAILQ_FOREACH(iv, &ic->ic_vaps, iv_next)
		ieee80211_htinfo_notify(iv);

	IEEE80211_UNLOCK(ic);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_11N,
	  "%s: global: nonht_pr=%d ht_opmode=0x%02x\n",
	  __func__,
	  !! (ic->ic_flags_ht & IEEE80211_FHT_NONHT_PR),
	  ic->ic_curhtprotmode);

	/* Driver update */
	if (vap->iv_ht_protmode_update != NULL)
		vap->iv_ht_protmode_update(vap);
}

/*
 * Set the short slot time state and notify the driver.
 *
 * This is the per-VAP slot time state.
 */
void
ieee80211_vap_set_shortslottime(struct ieee80211vap *vap, int onoff)
{
	struct ieee80211com *ic = vap->iv_ic;

	/* XXX lock? */

	/*
	 * Only modify the per-VAP slot time.
	 */
	if (onoff)
		vap->iv_flags |= IEEE80211_F_SHSLOT;
	else
		vap->iv_flags &= ~IEEE80211_F_SHSLOT;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
	    "%s: called; onoff=%d\n", __func__, onoff);
	/* schedule the deferred slot flag update and update */
	ieee80211_runtask(ic, &vap->iv_slot_task);
}

/*
 * Update the VAP short /long / barker preamble state and
 * update beacon state if needed.
 *
 * For now it simply copies the global flags into the per-vap
 * flags and schedules the callback.  Later this will support
 * both global and per-VAP flags, especially useful for
 * and STA+STA multi-channel operation (eg p2p).
 */
void
ieee80211_vap_update_preamble(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	/* XXX lock? */

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
	    "%s: called\n", __func__);
	/* schedule the deferred slot flag update and update */
	ieee80211_runtask(ic, &vap->iv_preamble_task);
}

/*
 * Update the VAP 11g protection mode and update beacon state
 * if needed.
 */
void
ieee80211_vap_update_erp_protmode(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	/* XXX lock? */

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
	    "%s: called\n", __func__);
	/* schedule the deferred slot flag update and update */
	ieee80211_runtask(ic, &vap->iv_erp_protmode_task);
}

/*
 * Update the VAP 11n protection mode and update beacon state
 * if needed.
 */
void
ieee80211_vap_update_ht_protmode(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	/* XXX lock? */

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
	    "%s: called\n", __func__);
	/* schedule the deferred protmode update */
	ieee80211_runtask(ic, &vap->iv_ht_protmode_task);
}

/*
 * Check if the specified rate set supports ERP.
 * NB: the rate set is assumed to be sorted.
 */
int
ieee80211_iserp_rateset(const struct ieee80211_rateset *rs)
{
	static const int rates[] = { 2, 4, 11, 22, 12, 24, 48 };
	int i, j;

	if (rs->rs_nrates < nitems(rates))
		return 0;
	for (i = 0; i < nitems(rates); i++) {
		for (j = 0; j < rs->rs_nrates; j++) {
			int r = rs->rs_rates[j] & IEEE80211_RATE_VAL;
			if (rates[i] == r)
				goto next;
			if (r > rates[i])
				return 0;
		}
		return 0;
	next:
		;
	}
	return 1;
}

/*
 * Mark the basic rates for the rate table based on the
 * operating mode.  For real 11g we mark all the 11b rates
 * and 6, 12, and 24 OFDM.  For 11b compatibility we mark only
 * 11b rates.  There's also a pseudo 11a-mode used to mark only
 * the basic OFDM rates.
 */
static void
setbasicrates(struct ieee80211_rateset *rs,
    enum ieee80211_phymode mode, int add)
{
	static const struct ieee80211_rateset basic[IEEE80211_MODE_MAX] = {
	    [IEEE80211_MODE_11A]	= { 3, { 12, 24, 48 } },
	    [IEEE80211_MODE_11B]	= { 2, { 2, 4 } },
					    /* NB: mixed b/g */
	    [IEEE80211_MODE_11G]	= { 4, { 2, 4, 11, 22 } },
	    [IEEE80211_MODE_TURBO_A]	= { 3, { 12, 24, 48 } },
	    [IEEE80211_MODE_TURBO_G]	= { 4, { 2, 4, 11, 22 } },
	    [IEEE80211_MODE_STURBO_A]	= { 3, { 12, 24, 48 } },
	    [IEEE80211_MODE_HALF]	= { 3, { 6, 12, 24 } },
	    [IEEE80211_MODE_QUARTER]	= { 3, { 3, 6, 12 } },
	    [IEEE80211_MODE_11NA]	= { 3, { 12, 24, 48 } },
					    /* NB: mixed b/g */
	    [IEEE80211_MODE_11NG]	= { 4, { 2, 4, 11, 22 } },
					    /* NB: mixed b/g */
	    [IEEE80211_MODE_VHT_2GHZ]	= { 4, { 2, 4, 11, 22 } },
	    [IEEE80211_MODE_VHT_5GHZ]	= { 3, { 12, 24, 48 } },
	};
	int i, j;

	for (i = 0; i < rs->rs_nrates; i++) {
		if (!add)
			rs->rs_rates[i] &= IEEE80211_RATE_VAL;
		for (j = 0; j < basic[mode].rs_nrates; j++)
			if (basic[mode].rs_rates[j] == rs->rs_rates[i]) {
				rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
				break;
			}
	}
}

/*
 * Set the basic rates in a rate set.
 */
void
ieee80211_setbasicrates(struct ieee80211_rateset *rs,
    enum ieee80211_phymode mode)
{
	setbasicrates(rs, mode, 0);
}

/*
 * Add basic rates to a rate set.
 */
void
ieee80211_addbasicrates(struct ieee80211_rateset *rs,
    enum ieee80211_phymode mode)
{
	setbasicrates(rs, mode, 1);
}

/*
 * WME protocol support.
 *
 * The default 11a/b/g/n parameters come from the WiFi Alliance WMM
 * System Interopability Test Plan (v1.4, Appendix F) and the 802.11n
 * Draft 2.0 Test Plan (Appendix D).
 *
 * Static/Dynamic Turbo mode settings come from Atheros.
 */
typedef struct phyParamType {
	uint8_t		aifsn;
	uint8_t		logcwmin;
	uint8_t		logcwmax;
	uint16_t	txopLimit;
	uint8_t 	acm;
} paramType;

static const struct phyParamType phyParamForAC_BE[IEEE80211_MODE_MAX] = {
	[IEEE80211_MODE_AUTO]	= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_11A]	= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_11B]	= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_11G]	= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_FH]	= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_TURBO_A]= { 2, 3,  5,  0, 0 },
	[IEEE80211_MODE_TURBO_G]= { 2, 3,  5,  0, 0 },
	[IEEE80211_MODE_STURBO_A]={ 2, 3,  5,  0, 0 },
	[IEEE80211_MODE_HALF]	= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_QUARTER]= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_11NA]	= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_11NG]	= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_VHT_2GHZ]	= { 3, 4,  6,  0, 0 },
	[IEEE80211_MODE_VHT_5GHZ]	= { 3, 4,  6,  0, 0 },
};
static const struct phyParamType phyParamForAC_BK[IEEE80211_MODE_MAX] = {
	[IEEE80211_MODE_AUTO]	= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_11A]	= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_11B]	= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_11G]	= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_FH]	= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_TURBO_A]= { 7, 3, 10,  0, 0 },
	[IEEE80211_MODE_TURBO_G]= { 7, 3, 10,  0, 0 },
	[IEEE80211_MODE_STURBO_A]={ 7, 3, 10,  0, 0 },
	[IEEE80211_MODE_HALF]	= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_QUARTER]= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_11NA]	= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_11NG]	= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_VHT_2GHZ]	= { 7, 4, 10,  0, 0 },
	[IEEE80211_MODE_VHT_5GHZ]	= { 7, 4, 10,  0, 0 },
};
static const struct phyParamType phyParamForAC_VI[IEEE80211_MODE_MAX] = {
	[IEEE80211_MODE_AUTO]	= { 1, 3, 4,  94, 0 },
	[IEEE80211_MODE_11A]	= { 1, 3, 4,  94, 0 },
	[IEEE80211_MODE_11B]	= { 1, 3, 4, 188, 0 },
	[IEEE80211_MODE_11G]	= { 1, 3, 4,  94, 0 },
	[IEEE80211_MODE_FH]	= { 1, 3, 4, 188, 0 },
	[IEEE80211_MODE_TURBO_A]= { 1, 2, 3,  94, 0 },
	[IEEE80211_MODE_TURBO_G]= { 1, 2, 3,  94, 0 },
	[IEEE80211_MODE_STURBO_A]={ 1, 2, 3,  94, 0 },
	[IEEE80211_MODE_HALF]	= { 1, 3, 4,  94, 0 },
	[IEEE80211_MODE_QUARTER]= { 1, 3, 4,  94, 0 },
	[IEEE80211_MODE_11NA]	= { 1, 3, 4,  94, 0 },
	[IEEE80211_MODE_11NG]	= { 1, 3, 4,  94, 0 },
	[IEEE80211_MODE_VHT_2GHZ]	= { 1, 3, 4,  94, 0 },
	[IEEE80211_MODE_VHT_5GHZ]	= { 1, 3, 4,  94, 0 },
};
static const struct phyParamType phyParamForAC_VO[IEEE80211_MODE_MAX] = {
	[IEEE80211_MODE_AUTO]	= { 1, 2, 3,  47, 0 },
	[IEEE80211_MODE_11A]	= { 1, 2, 3,  47, 0 },
	[IEEE80211_MODE_11B]	= { 1, 2, 3, 102, 0 },
	[IEEE80211_MODE_11G]	= { 1, 2, 3,  47, 0 },
	[IEEE80211_MODE_FH]	= { 1, 2, 3, 102, 0 },
	[IEEE80211_MODE_TURBO_A]= { 1, 2, 2,  47, 0 },
	[IEEE80211_MODE_TURBO_G]= { 1, 2, 2,  47, 0 },
	[IEEE80211_MODE_STURBO_A]={ 1, 2, 2,  47, 0 },
	[IEEE80211_MODE_HALF]	= { 1, 2, 3,  47, 0 },
	[IEEE80211_MODE_QUARTER]= { 1, 2, 3,  47, 0 },
	[IEEE80211_MODE_11NA]	= { 1, 2, 3,  47, 0 },
	[IEEE80211_MODE_11NG]	= { 1, 2, 3,  47, 0 },
	[IEEE80211_MODE_VHT_2GHZ]	= { 1, 2, 3,  47, 0 },
	[IEEE80211_MODE_VHT_5GHZ]	= { 1, 2, 3,  47, 0 },
};

static const struct phyParamType bssPhyParamForAC_BE[IEEE80211_MODE_MAX] = {
	[IEEE80211_MODE_AUTO]	= { 3, 4, 10,  0, 0 },
	[IEEE80211_MODE_11A]	= { 3, 4, 10,  0, 0 },
	[IEEE80211_MODE_11B]	= { 3, 4, 10,  0, 0 },
	[IEEE80211_MODE_11G]	= { 3, 4, 10,  0, 0 },
	[IEEE80211_MODE_FH]	= { 3, 4, 10,  0, 0 },
	[IEEE80211_MODE_TURBO_A]= { 2, 3, 10,  0, 0 },
	[IEEE80211_MODE_TURBO_G]= { 2, 3, 10,  0, 0 },
	[IEEE80211_MODE_STURBO_A]={ 2, 3, 10,  0, 0 },
	[IEEE80211_MODE_HALF]	= { 3, 4, 10,  0, 0 },
	[IEEE80211_MODE_QUARTER]= { 3, 4, 10,  0, 0 },
	[IEEE80211_MODE_11NA]	= { 3, 4, 10,  0, 0 },
	[IEEE80211_MODE_11NG]	= { 3, 4, 10,  0, 0 },
};
static const struct phyParamType bssPhyParamForAC_VI[IEEE80211_MODE_MAX] = {
	[IEEE80211_MODE_AUTO]	= { 2, 3, 4,  94, 0 },
	[IEEE80211_MODE_11A]	= { 2, 3, 4,  94, 0 },
	[IEEE80211_MODE_11B]	= { 2, 3, 4, 188, 0 },
	[IEEE80211_MODE_11G]	= { 2, 3, 4,  94, 0 },
	[IEEE80211_MODE_FH]	= { 2, 3, 4, 188, 0 },
	[IEEE80211_MODE_TURBO_A]= { 2, 2, 3,  94, 0 },
	[IEEE80211_MODE_TURBO_G]= { 2, 2, 3,  94, 0 },
	[IEEE80211_MODE_STURBO_A]={ 2, 2, 3,  94, 0 },
	[IEEE80211_MODE_HALF]	= { 2, 3, 4,  94, 0 },
	[IEEE80211_MODE_QUARTER]= { 2, 3, 4,  94, 0 },
	[IEEE80211_MODE_11NA]	= { 2, 3, 4,  94, 0 },
	[IEEE80211_MODE_11NG]	= { 2, 3, 4,  94, 0 },
};
static const struct phyParamType bssPhyParamForAC_VO[IEEE80211_MODE_MAX] = {
	[IEEE80211_MODE_AUTO]	= { 2, 2, 3,  47, 0 },
	[IEEE80211_MODE_11A]	= { 2, 2, 3,  47, 0 },
	[IEEE80211_MODE_11B]	= { 2, 2, 3, 102, 0 },
	[IEEE80211_MODE_11G]	= { 2, 2, 3,  47, 0 },
	[IEEE80211_MODE_FH]	= { 2, 2, 3, 102, 0 },
	[IEEE80211_MODE_TURBO_A]= { 1, 2, 2,  47, 0 },
	[IEEE80211_MODE_TURBO_G]= { 1, 2, 2,  47, 0 },
	[IEEE80211_MODE_STURBO_A]={ 1, 2, 2,  47, 0 },
	[IEEE80211_MODE_HALF]	= { 2, 2, 3,  47, 0 },
	[IEEE80211_MODE_QUARTER]= { 2, 2, 3,  47, 0 },
	[IEEE80211_MODE_11NA]	= { 2, 2, 3,  47, 0 },
	[IEEE80211_MODE_11NG]	= { 2, 2, 3,  47, 0 },
};

static void
_setifsparams(struct wmeParams *wmep, const paramType *phy)
{
	wmep->wmep_aifsn = phy->aifsn;
	wmep->wmep_logcwmin = phy->logcwmin;	
	wmep->wmep_logcwmax = phy->logcwmax;		
	wmep->wmep_txopLimit = phy->txopLimit;
}

static void
setwmeparams(struct ieee80211vap *vap, const char *type, int ac,
	struct wmeParams *wmep, const paramType *phy)
{
	wmep->wmep_acm = phy->acm;
	_setifsparams(wmep, phy);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
	    "set %s (%s) [acm %u aifsn %u logcwmin %u logcwmax %u txop %u]\n",
	    ieee80211_wme_acnames[ac], type,
	    wmep->wmep_acm, wmep->wmep_aifsn, wmep->wmep_logcwmin,
	    wmep->wmep_logcwmax, wmep->wmep_txopLimit);
}

static void
ieee80211_wme_initparams_locked(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	const paramType *pPhyParam, *pBssPhyParam;
	struct wmeParams *wmep;
	enum ieee80211_phymode mode;
	int i;

	IEEE80211_LOCK_ASSERT(ic);

	if ((ic->ic_caps & IEEE80211_C_WME) == 0 || ic->ic_nrunning > 1)
		return;

	/*
	 * Clear the wme cap_info field so a qoscount from a previous
	 * vap doesn't confuse later code which only parses the beacon
	 * field and updates hardware when said field changes.
	 * Otherwise the hardware is programmed with defaults, not what
	 * the beacon actually announces.
	 *
	 * Note that we can't ever have 0xff as an actual value;
	 * the only valid values are 0..15.
	 */
	wme->wme_wmeChanParams.cap_info = 0xfe;

	/*
	 * Select mode; we can be called early in which case we
	 * always use auto mode.  We know we'll be called when
	 * entering the RUN state with bsschan setup properly
	 * so state will eventually get set correctly
	 */
	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC)
		mode = ieee80211_chan2mode(ic->ic_bsschan);
	else
		mode = IEEE80211_MODE_AUTO;
	for (i = 0; i < WME_NUM_AC; i++) {
		switch (i) {
		case WME_AC_BK:
			pPhyParam = &phyParamForAC_BK[mode];
			pBssPhyParam = &phyParamForAC_BK[mode];
			break;
		case WME_AC_VI:
			pPhyParam = &phyParamForAC_VI[mode];
			pBssPhyParam = &bssPhyParamForAC_VI[mode];
			break;
		case WME_AC_VO:
			pPhyParam = &phyParamForAC_VO[mode];
			pBssPhyParam = &bssPhyParamForAC_VO[mode];
			break;
		case WME_AC_BE:
		default:
			pPhyParam = &phyParamForAC_BE[mode];
			pBssPhyParam = &bssPhyParamForAC_BE[mode];
			break;
		}
		wmep = &wme->wme_wmeChanParams.cap_wmeParams[i];
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			setwmeparams(vap, "chan", i, wmep, pPhyParam);
		} else {
			setwmeparams(vap, "chan", i, wmep, pBssPhyParam);
		}	
		wmep = &wme->wme_wmeBssChanParams.cap_wmeParams[i];
		setwmeparams(vap, "bss ", i, wmep, pBssPhyParam);
	}
	/* NB: check ic_bss to avoid NULL deref on initial attach */
	if (vap->iv_bss != NULL) {
		/*
		 * Calculate aggressive mode switching threshold based
		 * on beacon interval.  This doesn't need locking since
		 * we're only called before entering the RUN state at
		 * which point we start sending beacon frames.
		 */
		wme->wme_hipri_switch_thresh =
			(HIGH_PRI_SWITCH_THRESH * vap->iv_bss->ni_intval) / 100;
		wme->wme_flags &= ~WME_F_AGGRMODE;
		ieee80211_wme_updateparams(vap);
	}
}

void
ieee80211_wme_initparams(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK(ic);
	ieee80211_wme_initparams_locked(vap);
	IEEE80211_UNLOCK(ic);
}

/*
 * Update WME parameters for ourself and the BSS.
 */
void
ieee80211_wme_updateparams_locked(struct ieee80211vap *vap)
{
	static const paramType aggrParam[IEEE80211_MODE_MAX] = {
	    [IEEE80211_MODE_AUTO]	= { 2, 4, 10, 64, 0 },
	    [IEEE80211_MODE_11A]	= { 2, 4, 10, 64, 0 },
	    [IEEE80211_MODE_11B]	= { 2, 5, 10, 64, 0 },
	    [IEEE80211_MODE_11G]	= { 2, 4, 10, 64, 0 },
	    [IEEE80211_MODE_FH]		= { 2, 5, 10, 64, 0 },
	    [IEEE80211_MODE_TURBO_A]	= { 1, 3, 10, 64, 0 },
	    [IEEE80211_MODE_TURBO_G]	= { 1, 3, 10, 64, 0 },
	    [IEEE80211_MODE_STURBO_A]	= { 1, 3, 10, 64, 0 },
	    [IEEE80211_MODE_HALF]	= { 2, 4, 10, 64, 0 },
	    [IEEE80211_MODE_QUARTER]	= { 2, 4, 10, 64, 0 },
	    [IEEE80211_MODE_11NA]	= { 2, 4, 10, 64, 0 },	/* XXXcheck*/
	    [IEEE80211_MODE_11NG]	= { 2, 4, 10, 64, 0 },	/* XXXcheck*/
	    [IEEE80211_MODE_VHT_2GHZ]	= { 2, 4, 10, 64, 0 },	/* XXXcheck*/
	    [IEEE80211_MODE_VHT_5GHZ]	= { 2, 4, 10, 64, 0 },	/* XXXcheck*/
	};
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	const struct wmeParams *wmep;
	struct wmeParams *chanp, *bssp;
	enum ieee80211_phymode mode;
	int i;
	int do_aggrmode = 0;

       	/*
	 * Set up the channel access parameters for the physical
	 * device.  First populate the configured settings.
	 */
	for (i = 0; i < WME_NUM_AC; i++) {
		chanp = &wme->wme_chanParams.cap_wmeParams[i];
		wmep = &wme->wme_wmeChanParams.cap_wmeParams[i];
		chanp->wmep_aifsn = wmep->wmep_aifsn;
		chanp->wmep_logcwmin = wmep->wmep_logcwmin;
		chanp->wmep_logcwmax = wmep->wmep_logcwmax;
		chanp->wmep_txopLimit = wmep->wmep_txopLimit;

		chanp = &wme->wme_bssChanParams.cap_wmeParams[i];
		wmep = &wme->wme_wmeBssChanParams.cap_wmeParams[i];
		chanp->wmep_aifsn = wmep->wmep_aifsn;
		chanp->wmep_logcwmin = wmep->wmep_logcwmin;
		chanp->wmep_logcwmax = wmep->wmep_logcwmax;
		chanp->wmep_txopLimit = wmep->wmep_txopLimit;
	}

	/*
	 * Select mode; we can be called early in which case we
	 * always use auto mode.  We know we'll be called when
	 * entering the RUN state with bsschan setup properly
	 * so state will eventually get set correctly
	 */
	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC)
		mode = ieee80211_chan2mode(ic->ic_bsschan);
	else
		mode = IEEE80211_MODE_AUTO;

	/*
	 * This implements aggressive mode as found in certain
	 * vendors' AP's.  When there is significant high
	 * priority (VI/VO) traffic in the BSS throttle back BE
	 * traffic by using conservative parameters.  Otherwise
	 * BE uses aggressive params to optimize performance of
	 * legacy/non-QoS traffic.
	 */

	/* Hostap? Only if aggressive mode is enabled */
        if (vap->iv_opmode == IEEE80211_M_HOSTAP &&
	     (wme->wme_flags & WME_F_AGGRMODE) != 0)
		do_aggrmode = 1;

	/*
	 * Station? Only if we're in a non-QoS BSS.
	 */
	else if ((vap->iv_opmode == IEEE80211_M_STA &&
	     (vap->iv_bss->ni_flags & IEEE80211_NODE_QOS) == 0))
		do_aggrmode = 1;

	/*
	 * IBSS? Only if we have WME enabled.
	 */
	else if ((vap->iv_opmode == IEEE80211_M_IBSS) &&
	    (vap->iv_flags & IEEE80211_F_WME))
		do_aggrmode = 1;

	/*
	 * If WME is disabled on this VAP, default to aggressive mode
	 * regardless of the configuration.
	 */
	if ((vap->iv_flags & IEEE80211_F_WME) == 0)
		do_aggrmode = 1;

	/* XXX WDS? */

	/* XXX MBSS? */

	if (do_aggrmode) {
		chanp = &wme->wme_chanParams.cap_wmeParams[WME_AC_BE];
		bssp = &wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE];

		chanp->wmep_aifsn = bssp->wmep_aifsn = aggrParam[mode].aifsn;
		chanp->wmep_logcwmin = bssp->wmep_logcwmin =
		    aggrParam[mode].logcwmin;
		chanp->wmep_logcwmax = bssp->wmep_logcwmax =
		    aggrParam[mode].logcwmax;
		chanp->wmep_txopLimit = bssp->wmep_txopLimit =
		    (vap->iv_flags & IEEE80211_F_BURST) ?
			aggrParam[mode].txopLimit : 0;		
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
		    "update %s (chan+bss) [acm %u aifsn %u logcwmin %u "
		    "logcwmax %u txop %u]\n", ieee80211_wme_acnames[WME_AC_BE],
		    chanp->wmep_acm, chanp->wmep_aifsn, chanp->wmep_logcwmin,
		    chanp->wmep_logcwmax, chanp->wmep_txopLimit);
	}

	/*
	 * Change the contention window based on the number of associated
	 * stations.  If the number of associated stations is 1 and
	 * aggressive mode is enabled, lower the contention window even
	 * further.
	 */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP &&
	    vap->iv_sta_assoc < 2 && (wme->wme_flags & WME_F_AGGRMODE) != 0) {
		static const uint8_t logCwMin[IEEE80211_MODE_MAX] = {
		    [IEEE80211_MODE_AUTO]	= 3,
		    [IEEE80211_MODE_11A]	= 3,
		    [IEEE80211_MODE_11B]	= 4,
		    [IEEE80211_MODE_11G]	= 3,
		    [IEEE80211_MODE_FH]		= 4,
		    [IEEE80211_MODE_TURBO_A]	= 3,
		    [IEEE80211_MODE_TURBO_G]	= 3,
		    [IEEE80211_MODE_STURBO_A]	= 3,
		    [IEEE80211_MODE_HALF]	= 3,
		    [IEEE80211_MODE_QUARTER]	= 3,
		    [IEEE80211_MODE_11NA]	= 3,
		    [IEEE80211_MODE_11NG]	= 3,
		    [IEEE80211_MODE_VHT_2GHZ]	= 3,
		    [IEEE80211_MODE_VHT_5GHZ]	= 3,
		};
		chanp = &wme->wme_chanParams.cap_wmeParams[WME_AC_BE];
		bssp = &wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE];

		chanp->wmep_logcwmin = bssp->wmep_logcwmin = logCwMin[mode];
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
		    "update %s (chan+bss) logcwmin %u\n",
		    ieee80211_wme_acnames[WME_AC_BE], chanp->wmep_logcwmin);
	}

	/* schedule the deferred WME update */
	ieee80211_runtask(ic, &vap->iv_wme_task);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
	    "%s: WME params updated, cap_info 0x%x\n", __func__,
	    vap->iv_opmode == IEEE80211_M_STA ?
		wme->wme_wmeChanParams.cap_info :
		wme->wme_bssChanParams.cap_info);
}

void
ieee80211_wme_updateparams(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	if (ic->ic_caps & IEEE80211_C_WME) {
		IEEE80211_LOCK(ic);
		ieee80211_wme_updateparams_locked(vap);
		IEEE80211_UNLOCK(ic);
	}
}

/*
 * Fetch the WME parameters for the given VAP.
 *
 * When net80211 grows p2p, etc support, this may return different
 * parameters for each VAP.
 */
void
ieee80211_wme_vap_getparams(struct ieee80211vap *vap, struct chanAccParams *wp)
{

	memcpy(wp, &vap->iv_ic->ic_wme.wme_chanParams, sizeof(*wp));
}

/*
 * For NICs which only support one set of WME parameters (ie, softmac NICs)
 * there may be different VAP WME parameters but only one is "active".
 * This returns the "NIC" WME parameters for the currently active
 * context.
 */
void
ieee80211_wme_ic_getparams(struct ieee80211com *ic, struct chanAccParams *wp)
{

	memcpy(wp, &ic->ic_wme.wme_chanParams, sizeof(*wp));
}

/*
 * Return whether to use QoS on a given WME queue.
 *
 * This is intended to be called from the transmit path of softmac drivers
 * which are setting NoAck bits in transmit descriptors.
 *
 * Ideally this would be set in some transmit field before the packet is
 * queued to the driver but net80211 isn't quite there yet.
 */
int
ieee80211_wme_vap_ac_is_noack(struct ieee80211vap *vap, int ac)
{
	/* Bounds/sanity check */
	if (ac < 0 || ac >= WME_NUM_AC)
		return (0);

	/* Again, there's only one global context for now */
	return (!! vap->iv_ic->ic_wme.wme_chanParams.cap_wmeParams[ac].wmep_noackPolicy);
}

static void
parent_updown(void *arg, int npending)
{
	struct ieee80211com *ic = arg;

	ic->ic_parent(ic);
}

static void
update_mcast(void *arg, int npending)
{
	struct ieee80211com *ic = arg;

	ic->ic_update_mcast(ic);
}

static void
update_promisc(void *arg, int npending)
{
	struct ieee80211com *ic = arg;

	ic->ic_update_promisc(ic);
}

static void
update_channel(void *arg, int npending)
{
	struct ieee80211com *ic = arg;

	ic->ic_set_channel(ic);
	ieee80211_radiotap_chan_change(ic);
}

static void
update_chw(void *arg, int npending)
{
	struct ieee80211com *ic = arg;

	/*
	 * XXX should we defer the channel width _config_ update until now?
	 */
	ic->ic_update_chw(ic);
}

/*
 * Deferred WME parameter and beacon update.
 *
 * In preparation for per-VAP WME configuration, call the VAP
 * method if the VAP requires it.  Otherwise, just call the
 * older global method.  There isn't a per-VAP WME configuration
 * just yet so for now just use the global configuration.
 */
static void
vap_update_wme(void *arg, int npending)
{
	struct ieee80211vap *vap = arg;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_wme_state *wme = &ic->ic_wme;

	/* Driver update */
	if (vap->iv_wme_update != NULL)
		vap->iv_wme_update(vap,
		    ic->ic_wme.wme_chanParams.cap_wmeParams);
	else
		ic->ic_wme.wme_update(ic);

	IEEE80211_LOCK(ic);
	/*
	 * Arrange for the beacon update.
	 *
	 * XXX what about MBSS, WDS?
	 */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP
	    || vap->iv_opmode == IEEE80211_M_IBSS) {
		/*
		 * Arrange for a beacon update and bump the parameter
		 * set number so associated stations load the new values.
		 */
		wme->wme_bssChanParams.cap_info =
			(wme->wme_bssChanParams.cap_info+1) & WME_QOSINFO_COUNT;
		ieee80211_beacon_notify(vap, IEEE80211_BEACON_WME);
	}
	IEEE80211_UNLOCK(ic);
}

static void
restart_vaps(void *arg, int npending)
{
	struct ieee80211com *ic = arg;

	ieee80211_suspend_all(ic);
	ieee80211_resume_all(ic);
}

/*
 * Block until the parent is in a known state.  This is
 * used after any operations that dispatch a task (e.g.
 * to auto-configure the parent device up/down).
 */
void
ieee80211_waitfor_parent(struct ieee80211com *ic)
{
	taskqueue_block(ic->ic_tq);
	ieee80211_draintask(ic, &ic->ic_parent_task);
	ieee80211_draintask(ic, &ic->ic_mcast_task);
	ieee80211_draintask(ic, &ic->ic_promisc_task);
	ieee80211_draintask(ic, &ic->ic_chan_task);
	ieee80211_draintask(ic, &ic->ic_bmiss_task);
	ieee80211_draintask(ic, &ic->ic_chw_task);
	taskqueue_unblock(ic->ic_tq);
}

/*
 * Check to see whether the current channel needs reset.
 *
 * Some devices don't handle being given an invalid channel
 * in their operating mode very well (eg wpi(4) will throw a
 * firmware exception.)
 *
 * Return 0 if we're ok, 1 if the channel needs to be reset.
 *
 * See PR kern/202502.
 */
static int
ieee80211_start_check_reset_chan(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	if ((vap->iv_opmode == IEEE80211_M_IBSS &&
	     IEEE80211_IS_CHAN_NOADHOC(ic->ic_curchan)) ||
	    (vap->iv_opmode == IEEE80211_M_HOSTAP &&
	     IEEE80211_IS_CHAN_NOHOSTAP(ic->ic_curchan)))
		return (1);
	return (0);
}

/*
 * Reset the curchan to a known good state.
 */
static void
ieee80211_start_reset_chan(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	ic->ic_curchan = &ic->ic_channels[0];
}

/*
 * Start a vap running.  If this is the first vap to be
 * set running on the underlying device then we
 * automatically bring the device up.
 */
void
ieee80211_start_locked(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK_ASSERT(ic);

	IEEE80211_DPRINTF(vap,
		IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
		"start running, %d vaps running\n", ic->ic_nrunning);

	if (!ieee80211_vap_ifp_check_is_running(vap)) {
		/*
		 * Mark us running.  Note that it's ok to do this first;
		 * if we need to bring the parent device up we defer that
		 * to avoid dropping the com lock.  We expect the device
		 * to respond to being marked up by calling back into us
		 * through ieee80211_start_all at which point we'll come
		 * back in here and complete the work.
		 */
		ieee80211_vap_ifp_set_running_state(vap, true);
		ieee80211_notify_ifnet_change(vap, IFF_DRV_RUNNING);

		/*
		 * We are not running; if this we are the first vap
		 * to be brought up auto-up the parent if necessary.
		 */
		if (ic->ic_nrunning++ == 0) {
			/* reset the channel to a known good channel */
			if (ieee80211_start_check_reset_chan(vap))
				ieee80211_start_reset_chan(vap);

			IEEE80211_DPRINTF(vap,
			    IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
			    "%s: up parent %s\n", __func__, ic->ic_name);
			ieee80211_runtask(ic, &ic->ic_parent_task);
			return;
		}
	}
	/*
	 * If the parent is up and running, then kick the
	 * 802.11 state machine as appropriate.
	 */
	if (vap->iv_roaming != IEEE80211_ROAMING_MANUAL) {
		if (vap->iv_opmode == IEEE80211_M_STA) {
#if 0
			/* XXX bypasses scan too easily; disable for now */
			/*
			 * Try to be intelligent about clocking the state
			 * machine.  If we're currently in RUN state then
			 * we should be able to apply any new state/parameters
			 * simply by re-associating.  Otherwise we need to
			 * re-scan to select an appropriate ap.
			 */ 
			if (vap->iv_state >= IEEE80211_S_RUN)
				ieee80211_new_state_locked(vap,
				    IEEE80211_S_ASSOC, 1);
			else
#endif
				ieee80211_new_state_locked(vap,
				    IEEE80211_S_SCAN, 0);
		} else {
			/*
			 * For monitor+wds mode there's nothing to do but
			 * start running.  Otherwise if this is the first
			 * vap to be brought up, start a scan which may be
			 * preempted if the station is locked to a particular
			 * channel.
			 */
			vap->iv_flags_ext |= IEEE80211_FEXT_REINIT;
			if (vap->iv_opmode == IEEE80211_M_MONITOR ||
			    vap->iv_opmode == IEEE80211_M_WDS)
				ieee80211_new_state_locked(vap,
				    IEEE80211_S_RUN, -1);
			else
				ieee80211_new_state_locked(vap,
				    IEEE80211_S_SCAN, 0);
		}
	}
}

/*
 * Start a single vap.
 */
void
ieee80211_init(void *arg)
{
	struct ieee80211vap *vap = arg;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
	    "%s\n", __func__);

	IEEE80211_LOCK(vap->iv_ic);
	ieee80211_start_locked(vap);
	IEEE80211_UNLOCK(vap->iv_ic);
}

/*
 * Start all runnable vap's on a device.
 */
void
ieee80211_start_all(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		struct ifnet *ifp = vap->iv_ifp;
		if (IFNET_IS_UP_RUNNING(ifp))	/* NB: avoid recursion */
			ieee80211_start_locked(vap);
	}
	IEEE80211_UNLOCK(ic);
}

/*
 * Stop a vap.  We force it down using the state machine
 * then mark it's ifnet not running.  If this is the last
 * vap running on the underlying device then we close it
 * too to insure it will be properly initialized when the
 * next vap is brought up.
 */
void
ieee80211_stop_locked(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK_ASSERT(ic);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
	    "stop running, %d vaps running\n", ic->ic_nrunning);

	ieee80211_new_state_locked(vap, IEEE80211_S_INIT, -1);
	if (ieee80211_vap_ifp_check_is_running(vap)) {
		/* mark us stopped */
		ieee80211_vap_ifp_set_running_state(vap, false);
		ieee80211_notify_ifnet_change(vap, IFF_DRV_RUNNING);
		if (--ic->ic_nrunning == 0) {
			IEEE80211_DPRINTF(vap,
			    IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
			    "down parent %s\n", ic->ic_name);
			ieee80211_runtask(ic, &ic->ic_parent_task);
		}
	}
}

void
ieee80211_stop(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK(ic);
	ieee80211_stop_locked(vap);
	IEEE80211_UNLOCK(ic);
}

/*
 * Stop all vap's running on a device.
 */
void
ieee80211_stop_all(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		struct ifnet *ifp = vap->iv_ifp;
		if (IFNET_IS_UP_RUNNING(ifp))	/* NB: avoid recursion */
			ieee80211_stop_locked(vap);
	}
	IEEE80211_UNLOCK(ic);

	ieee80211_waitfor_parent(ic);
}

/*
 * Stop all vap's running on a device and arrange
 * for those that were running to be resumed.
 */
void
ieee80211_suspend_all(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		struct ifnet *ifp = vap->iv_ifp;
		if (IFNET_IS_UP_RUNNING(ifp)) {	/* NB: avoid recursion */
			vap->iv_flags_ext |= IEEE80211_FEXT_RESUME;
			ieee80211_stop_locked(vap);
		}
	}
	IEEE80211_UNLOCK(ic);

	ieee80211_waitfor_parent(ic);
}

/*
 * Start all vap's marked for resume.
 */
void
ieee80211_resume_all(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		struct ifnet *ifp = vap->iv_ifp;
		if (!IFNET_IS_UP_RUNNING(ifp) &&
		    (vap->iv_flags_ext & IEEE80211_FEXT_RESUME)) {
			vap->iv_flags_ext &= ~IEEE80211_FEXT_RESUME;
			ieee80211_start_locked(vap);
		}
	}
	IEEE80211_UNLOCK(ic);
}

/*
 * Restart all vap's running on a device.
 */
void
ieee80211_restart_all(struct ieee80211com *ic)
{
	/*
	 * NB: do not use ieee80211_runtask here, we will
	 * block & drain net80211 taskqueue.
	 */
	taskqueue_enqueue(taskqueue_thread, &ic->ic_restart_task);
}

void
ieee80211_beacon_miss(struct ieee80211com *ic)
{
	IEEE80211_LOCK(ic);
	if ((ic->ic_flags & IEEE80211_F_SCAN) == 0) {
		/* Process in a taskq, the handler may reenter the driver */
		ieee80211_runtask(ic, &ic->ic_bmiss_task);
	}
	IEEE80211_UNLOCK(ic);
}

static void
beacon_miss(void *arg, int npending)
{
	struct ieee80211com *ic = arg;
	struct ieee80211vap *vap;

	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		/*
		 * We only pass events through for sta vap's in RUN+ state;
		 * may be too restrictive but for now this saves all the
		 * handlers duplicating these checks.
		 */
		if (vap->iv_opmode == IEEE80211_M_STA &&
		    vap->iv_state >= IEEE80211_S_RUN &&
		    vap->iv_bmiss != NULL)
			vap->iv_bmiss(vap);
	}
	IEEE80211_UNLOCK(ic);
}

static void
beacon_swmiss(void *arg, int npending)
{
	struct ieee80211vap *vap = arg;
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK(ic);
	if (vap->iv_state >= IEEE80211_S_RUN) {
		/* XXX Call multiple times if npending > zero? */
		vap->iv_bmiss(vap);
	}
	IEEE80211_UNLOCK(ic);
}

/*
 * Software beacon miss handling.  Check if any beacons
 * were received in the last period.  If not post a
 * beacon miss; otherwise reset the counter.
 */
void
ieee80211_swbmiss(void *arg)
{
	struct ieee80211vap *vap = arg;
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK_ASSERT(ic);

	KASSERT(vap->iv_state >= IEEE80211_S_RUN,
	    ("wrong state %d", vap->iv_state));

	if (ic->ic_flags & IEEE80211_F_SCAN) {
		/*
		 * If scanning just ignore and reset state.  If we get a
		 * bmiss after coming out of scan because we haven't had
		 * time to receive a beacon then we should probe the AP
		 * before posting a real bmiss (unless iv_bmiss_max has
		 * been artifiically lowered).  A cleaner solution might
		 * be to disable the timer on scan start/end but to handle
		 * case of multiple sta vap's we'd need to disable the
		 * timers of all affected vap's.
		 */
		vap->iv_swbmiss_count = 0;
	} else if (vap->iv_swbmiss_count == 0) {
		if (vap->iv_bmiss != NULL)
			ieee80211_runtask(ic, &vap->iv_swbmiss_task);
	} else
		vap->iv_swbmiss_count = 0;
	callout_reset(&vap->iv_swbmiss, vap->iv_swbmiss_period,
		ieee80211_swbmiss, vap);
}

/*
 * Start an 802.11h channel switch.  We record the parameters,
 * mark the operation pending, notify each vap through the
 * beacon update mechanism so it can update the beacon frame
 * contents, and then switch vap's to CSA state to block outbound
 * traffic.  Devices that handle CSA directly can use the state
 * switch to do the right thing so long as they call
 * ieee80211_csa_completeswitch when it's time to complete the
 * channel change.  Devices that depend on the net80211 layer can
 * use ieee80211_beacon_update to handle the countdown and the
 * channel switch.
 */
void
ieee80211_csa_startswitch(struct ieee80211com *ic,
	struct ieee80211_channel *c, int mode, int count)
{
	struct ieee80211vap *vap;

	IEEE80211_LOCK_ASSERT(ic);

	ic->ic_csa_newchan = c;
	ic->ic_csa_mode = mode;
	ic->ic_csa_count = count;
	ic->ic_flags |= IEEE80211_F_CSAPENDING;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
		    vap->iv_opmode == IEEE80211_M_IBSS ||
		    vap->iv_opmode == IEEE80211_M_MBSS)
			ieee80211_beacon_notify(vap, IEEE80211_BEACON_CSA);
		/* switch to CSA state to block outbound traffic */
		if (vap->iv_state == IEEE80211_S_RUN)
			ieee80211_new_state_locked(vap, IEEE80211_S_CSA, 0);
	}
	ieee80211_notify_csa(ic, c, mode, count);
}

/*
 * Complete the channel switch by transitioning all CSA VAPs to RUN.
 * This is called by both the completion and cancellation functions
 * so each VAP is placed back in the RUN state and can thus transmit.
 */
static void
csa_completeswitch(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	ic->ic_csa_newchan = NULL;
	ic->ic_flags &= ~IEEE80211_F_CSAPENDING;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		if (vap->iv_state == IEEE80211_S_CSA)
			ieee80211_new_state_locked(vap, IEEE80211_S_RUN, 0);
}

/*
 * Complete an 802.11h channel switch started by ieee80211_csa_startswitch.
 * We clear state and move all vap's in CSA state to RUN state
 * so they can again transmit.
 *
 * Although this may not be completely correct, update the BSS channel
 * for each VAP to the newly configured channel. The setcurchan sets
 * the current operating channel for the interface (so the radio does
 * switch over) but the VAP BSS isn't updated, leading to incorrectly
 * reported information via ioctl.
 */
void
ieee80211_csa_completeswitch(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	IEEE80211_LOCK_ASSERT(ic);

	KASSERT(ic->ic_flags & IEEE80211_F_CSAPENDING, ("csa not pending"));

	ieee80211_setcurchan(ic, ic->ic_csa_newchan);
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		if (vap->iv_state == IEEE80211_S_CSA)
			vap->iv_bss->ni_chan = ic->ic_curchan;

	csa_completeswitch(ic);
}

/*
 * Cancel an 802.11h channel switch started by ieee80211_csa_startswitch.
 * We clear state and move all vap's in CSA state to RUN state
 * so they can again transmit.
 */
void
ieee80211_csa_cancelswitch(struct ieee80211com *ic)
{
	IEEE80211_LOCK_ASSERT(ic);

	csa_completeswitch(ic);
}

/*
 * Complete a DFS CAC started by ieee80211_dfs_cac_start.
 * We clear state and move all vap's in CAC state to RUN state.
 */
void
ieee80211_cac_completeswitch(struct ieee80211vap *vap0)
{
	struct ieee80211com *ic = vap0->iv_ic;
	struct ieee80211vap *vap;

	IEEE80211_LOCK(ic);
	/*
	 * Complete CAC state change for lead vap first; then
	 * clock all the other vap's waiting.
	 */
	KASSERT(vap0->iv_state == IEEE80211_S_CAC,
	    ("wrong state %d", vap0->iv_state));
	ieee80211_new_state_locked(vap0, IEEE80211_S_RUN, 0);

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		if (vap->iv_state == IEEE80211_S_CAC && vap != vap0)
			ieee80211_new_state_locked(vap, IEEE80211_S_RUN, 0);
	IEEE80211_UNLOCK(ic);
}

/*
 * Force all vap's other than the specified vap to the INIT state
 * and mark them as waiting for a scan to complete.  These vaps
 * will be brought up when the scan completes and the scanning vap
 * reaches RUN state by wakeupwaiting.
 */
static void
markwaiting(struct ieee80211vap *vap0)
{
	struct ieee80211com *ic = vap0->iv_ic;
	struct ieee80211vap *vap;

	IEEE80211_LOCK_ASSERT(ic);

	/*
	 * A vap list entry can not disappear since we are running on the
	 * taskqueue and a vap destroy will queue and drain another state
	 * change task.
	 */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap == vap0)
			continue;
		if (vap->iv_state != IEEE80211_S_INIT) {
			/* NB: iv_newstate may drop the lock */
			vap->iv_newstate(vap, IEEE80211_S_INIT, 0);
			IEEE80211_LOCK_ASSERT(ic);
			vap->iv_flags_ext |= IEEE80211_FEXT_SCANWAIT;
		}
	}
}

/*
 * Wakeup all vap's waiting for a scan to complete.  This is the
 * companion to markwaiting (above) and is used to coordinate
 * multiple vaps scanning.
 * This is called from the state taskqueue.
 */
static void
wakeupwaiting(struct ieee80211vap *vap0)
{
	struct ieee80211com *ic = vap0->iv_ic;
	struct ieee80211vap *vap;

	IEEE80211_LOCK_ASSERT(ic);

	/*
	 * A vap list entry can not disappear since we are running on the
	 * taskqueue and a vap destroy will queue and drain another state
	 * change task.
	 */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap == vap0)
			continue;
		if (vap->iv_flags_ext & IEEE80211_FEXT_SCANWAIT) {
			vap->iv_flags_ext &= ~IEEE80211_FEXT_SCANWAIT;
			/* NB: sta's cannot go INIT->RUN */
			/* NB: iv_newstate may drop the lock */

			/*
			 * This is problematic if the interface has OACTIVE
			 * set.  Only the deferred ieee80211_newstate_cb()
			 * will end up actually /clearing/ the OACTIVE
			 * flag on a state transition to RUN from a non-RUN
			 * state.
			 *
			 * But, we're not actually deferring this callback;
			 * and when the deferred call occurs it shows up as
			 * a RUN->RUN transition!  So the flag isn't/wasn't
			 * cleared!
			 *
			 * I'm also not sure if it's correct to actually
			 * do the transitions here fully through the deferred
			 * paths either as other things can be invoked as
			 * part of that state machine.
			 *
			 * So just keep this in mind when looking at what
			 * the markwaiting/wakeupwaiting routines are doing
			 * and how they invoke vap state changes.
			 */

			vap->iv_newstate(vap,
			    vap->iv_opmode == IEEE80211_M_STA ?
			        IEEE80211_S_SCAN : IEEE80211_S_RUN, 0);
			IEEE80211_LOCK_ASSERT(ic);
		}
	}
}

static int
_ieee80211_newstate_get_next_empty_slot(struct ieee80211vap *vap)
{
	int nstate_num;

	IEEE80211_LOCK_ASSERT(vap->iv_ic);

	if (vap->iv_nstate_n >= NET80211_IV_NSTATE_NUM)
		return (-1);

	nstate_num = vap->iv_nstate_b + vap->iv_nstate_n;
	nstate_num %= NET80211_IV_NSTATE_NUM;
	vap->iv_nstate_n++;

	return (nstate_num);
}

static int
_ieee80211_newstate_get_next_pending_slot(struct ieee80211vap *vap)
{
	int nstate_num;

	IEEE80211_LOCK_ASSERT(vap->iv_ic);

	KASSERT(vap->iv_nstate_n > 0, ("%s: vap %p iv_nstate_n %d\n",
	    __func__, vap, vap->iv_nstate_n));

	nstate_num = vap->iv_nstate_b;
	vap->iv_nstate_b++;
	if (vap->iv_nstate_b >= NET80211_IV_NSTATE_NUM)
		vap->iv_nstate_b = 0;
	vap->iv_nstate_n--;

	return (nstate_num);
}

static int
_ieee80211_newstate_get_npending(struct ieee80211vap *vap)
{

	IEEE80211_LOCK_ASSERT(vap->iv_ic);

	return (vap->iv_nstate_n);
}

/*
 * Handle post state change work common to all operating modes.
 */
static void
ieee80211_newstate_cb(void *xvap, int npending)
{
	struct ieee80211vap *vap = xvap;
	struct ieee80211com *ic = vap->iv_ic;
	enum ieee80211_state nstate, ostate;
	int arg, rc, nstate_num;

	KASSERT(npending == 1, ("%s: vap %p with npending %d != 1\n",
	    __func__, vap, npending));
	IEEE80211_LOCK(ic);
	nstate_num = _ieee80211_newstate_get_next_pending_slot(vap);

	/*
	 * Update the historic fields for now as they are used in some
	 * drivers and reduce code changes for now.
	 */
	vap->iv_nstate = nstate = vap->iv_nstates[nstate_num];
	arg = vap->iv_nstate_args[nstate_num];

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
	    "%s:%d: running state update %s -> %s (%d)\n",
	    __func__, __LINE__,
	    ieee80211_state_name[vap->iv_state],
	    ieee80211_state_name[nstate],
	    npending);

	if (vap->iv_flags_ext & IEEE80211_FEXT_REINIT) {
		/*
		 * We have been requested to drop back to the INIT before
		 * proceeding to the new state.
		 */
		/* Deny any state changes while we are here. */
		vap->iv_nstate = IEEE80211_S_INIT;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
		    "%s: %s -> %s arg %d -> %s arg %d\n", __func__,
		    ieee80211_state_name[vap->iv_state],
		    ieee80211_state_name[vap->iv_nstate], 0,
		    ieee80211_state_name[nstate], arg);
		vap->iv_newstate(vap, vap->iv_nstate, 0);
		IEEE80211_LOCK_ASSERT(ic);
		vap->iv_flags_ext &= ~(IEEE80211_FEXT_REINIT |
		    IEEE80211_FEXT_STATEWAIT);
		/* enqueue new state transition after cancel_scan() task */
		ieee80211_new_state_locked(vap, nstate, arg);
		goto done;
	}

	ostate = vap->iv_state;
	if (nstate == IEEE80211_S_SCAN && ostate != IEEE80211_S_INIT) {
		/*
		 * SCAN was forced; e.g. on beacon miss.  Force other running
		 * vap's to INIT state and mark them as waiting for the scan to
		 * complete.  This insures they don't interfere with our
		 * scanning.  Since we are single threaded the vaps can not
		 * transition again while we are executing.
		 *
		 * XXX not always right, assumes ap follows sta
		 */
		markwaiting(vap);
	}
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
	    "%s: %s -> %s arg %d\n", __func__,
	    ieee80211_state_name[ostate], ieee80211_state_name[nstate], arg);

	rc = vap->iv_newstate(vap, nstate, arg);
	IEEE80211_LOCK_ASSERT(ic);
	vap->iv_flags_ext &= ~IEEE80211_FEXT_STATEWAIT;
	if (rc != 0) {
		/* State transition failed */
		KASSERT(rc != EINPROGRESS, ("iv_newstate was deferred"));
		KASSERT(nstate != IEEE80211_S_INIT,
		    ("INIT state change failed"));
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
		    "%s: %s returned error %d\n", __func__,
		    ieee80211_state_name[nstate], rc);
		goto done;
	}

	/*
	 * Handle the case of a RUN->RUN transition occuring when STA + AP
	 * VAPs occur on the same radio.
	 *
	 * The mark and wakeup waiting routines call iv_newstate() directly,
	 * but they do not end up deferring state changes here.
	 * Thus, although the VAP newstate method sees a transition
	 * of RUN->INIT->RUN, the deferred path here only sees a RUN->RUN
	 * transition.  If OACTIVE is set then it is never cleared.
	 *
	 * So, if we're here and the state is RUN, just clear OACTIVE.
	 * At some point if the markwaiting/wakeupwaiting paths end up
	 * also invoking the deferred state updates then this will
	 * be no-op code - and also if OACTIVE is finally retired, it'll
	 * also be no-op code.
	 */
	if (nstate == IEEE80211_S_RUN) {
		/*
		 * OACTIVE may be set on the vap if the upper layer
		 * tried to transmit (e.g. IPv6 NDP) before we reach
		 * RUN state.  Clear it and restart xmit.
		 *
		 * Note this can also happen as a result of SLEEP->RUN
		 * (i.e. coming out of power save mode).
		 *
		 * Historically this was done only for a state change
		 * but is needed earlier; see next comment.  The 2nd half
		 * of the work is still only done in case of an actual
		 * state change below.
		 */
		/*
		 * Unblock the VAP queue; a RUN->RUN state can happen
		 * on a STA+AP setup on the AP vap.  See wakeupwaiting().
		 */
		vap->iv_ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

		/*
		 * XXX TODO Kick-start a VAP queue - this should be a method!
		 */
	}

	/* No actual transition, skip post processing */
	if (ostate == nstate)
		goto done;

	if (nstate == IEEE80211_S_RUN) {

		/* bring up any vaps waiting on us */
		wakeupwaiting(vap);
	} else if (nstate == IEEE80211_S_INIT) {
		/*
		 * Flush the scan cache if we did the last scan (XXX?)
		 * and flush any frames on send queues from this vap.
		 * Note the mgt q is used only for legacy drivers and
		 * will go away shortly.
		 */
		ieee80211_scan_flush(vap);

		/*
		 * XXX TODO: ic/vap queue flush
		 */
	}
done:
	IEEE80211_UNLOCK(ic);
}

/*
 * Public interface for initiating a state machine change.
 * This routine single-threads the request and coordinates
 * the scheduling of multiple vaps for the purpose of selecting
 * an operating channel.  Specifically the following scenarios
 * are handled:
 * o only one vap can be selecting a channel so on transition to
 *   SCAN state if another vap is already scanning then
 *   mark the caller for later processing and return without
 *   doing anything (XXX? expectations by caller of synchronous operation)
 * o only one vap can be doing CAC of a channel so on transition to
 *   CAC state if another vap is already scanning for radar then
 *   mark the caller for later processing and return without
 *   doing anything (XXX? expectations by caller of synchronous operation)
 * o if another vap is already running when a request is made
 *   to SCAN then an operating channel has been chosen; bypass
 *   the scan and just join the channel
 *
 * Note that the state change call is done through the iv_newstate
 * method pointer so any driver routine gets invoked.  The driver
 * will normally call back into operating mode-specific
 * ieee80211_newstate routines (below) unless it needs to completely
 * bypass the state machine (e.g. because the firmware has it's
 * own idea how things should work).  Bypassing the net80211 layer
 * is usually a mistake and indicates lack of proper integration
 * with the net80211 layer.
 */
int
ieee80211_new_state_locked(struct ieee80211vap *vap,
	enum ieee80211_state nstate, int arg)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211vap *vp;
	enum ieee80211_state ostate;
	int nrunning, nscanning, nstate_num;

	IEEE80211_LOCK_ASSERT(ic);

	if (vap->iv_flags_ext & IEEE80211_FEXT_STATEWAIT) {
		if (vap->iv_nstate == IEEE80211_S_INIT ||
		    ((vap->iv_state == IEEE80211_S_INIT ||
		    (vap->iv_flags_ext & IEEE80211_FEXT_REINIT)) &&
		    vap->iv_nstate == IEEE80211_S_SCAN &&
		    nstate > IEEE80211_S_SCAN)) {
			/*
			 * XXX The vap is being stopped/started,
			 * do not allow any other state changes
			 * until this is completed.
			 */
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
			    "%s:%d: %s -> %s (%s) transition discarded\n",
			    __func__, __LINE__,
			    ieee80211_state_name[vap->iv_state],
			    ieee80211_state_name[nstate],
			    ieee80211_state_name[vap->iv_nstate]);
			return -1;
		}
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
	    "%s:%d: starting state update %s -> %s (%s)\n",
	    __func__, __LINE__,
	    ieee80211_state_name[vap->iv_state],
	    ieee80211_state_name[vap->iv_nstate],
	    ieee80211_state_name[nstate]);

	nrunning = nscanning = 0;
	/* XXX can track this state instead of calculating */
	TAILQ_FOREACH(vp, &ic->ic_vaps, iv_next) {
		if (vp != vap) {
			if (vp->iv_state >= IEEE80211_S_RUN)
				nrunning++;
			/* XXX doesn't handle bg scan */
			/* NB: CAC+AUTH+ASSOC treated like SCAN */
			else if (vp->iv_state > IEEE80211_S_INIT)
				nscanning++;
		}
	}
	/*
	 * Look ahead for the "old state" at that point when the last queued
	 * state transition is run.
	 */
	if (vap->iv_nstate_n == 0) {
		ostate = vap->iv_state;
	} else {
		nstate_num = (vap->iv_nstate_b + vap->iv_nstate_n - 1) % NET80211_IV_NSTATE_NUM;
		ostate = vap->iv_nstates[nstate_num];
	}
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
	    "%s: %s -> %s (arg %d) (nrunning %d nscanning %d)\n", __func__,
	    ieee80211_state_name[ostate], ieee80211_state_name[nstate], arg,
	    nrunning, nscanning);
	switch (nstate) {
	case IEEE80211_S_SCAN:
		if (ostate == IEEE80211_S_INIT) {
			/*
			 * INIT -> SCAN happens on initial bringup.
			 */
			KASSERT(!(nscanning && nrunning),
			    ("%d scanning and %d running", nscanning, nrunning));
			if (nscanning) {
				/*
				 * Someone is scanning, defer our state
				 * change until the work has completed.
				 */
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
				    "%s: defer %s -> %s\n",
				    __func__, ieee80211_state_name[ostate],
				    ieee80211_state_name[nstate]);
				vap->iv_flags_ext |= IEEE80211_FEXT_SCANWAIT;
				return 0;
			}
			if (nrunning) {
				/*
				 * Someone is operating; just join the channel
				 * they have chosen.
				 */
				/* XXX kill arg? */
				/* XXX check each opmode, adhoc? */
				if (vap->iv_opmode == IEEE80211_M_STA)
					nstate = IEEE80211_S_SCAN;
				else
					nstate = IEEE80211_S_RUN;
#ifdef IEEE80211_DEBUG
				if (nstate != IEEE80211_S_SCAN) {
					IEEE80211_DPRINTF(vap,
					    IEEE80211_MSG_STATE,
					    "%s: override, now %s -> %s\n",
					    __func__,
					    ieee80211_state_name[ostate],
					    ieee80211_state_name[nstate]);
				}
#endif
			}
		}
		break;
	case IEEE80211_S_RUN:
		if (vap->iv_opmode == IEEE80211_M_WDS &&
		    (vap->iv_flags_ext & IEEE80211_FEXT_WDSLEGACY) &&
		    nscanning) {
			/*
			 * Legacy WDS with someone else scanning; don't
			 * go online until that completes as we should
			 * follow the other vap to the channel they choose.
			 */
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
			     "%s: defer %s -> %s (legacy WDS)\n", __func__,
			     ieee80211_state_name[ostate],
			     ieee80211_state_name[nstate]);
			vap->iv_flags_ext |= IEEE80211_FEXT_SCANWAIT;
			return 0;
		}
		if (vap->iv_opmode == IEEE80211_M_HOSTAP &&
		    IEEE80211_IS_CHAN_DFS(ic->ic_bsschan) &&
		    (vap->iv_flags_ext & IEEE80211_FEXT_DFS) &&
		    !IEEE80211_IS_CHAN_CACDONE(ic->ic_bsschan)) {
			/*
			 * This is a DFS channel, transition to CAC state
			 * instead of RUN.  This allows us to initiate
			 * Channel Availability Check (CAC) as specified
			 * by 11h/DFS.
			 */
			nstate = IEEE80211_S_CAC;
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
			     "%s: override %s -> %s (DFS)\n", __func__,
			     ieee80211_state_name[ostate],
			     ieee80211_state_name[nstate]);
		}
		break;
	case IEEE80211_S_INIT:
		/* cancel any scan in progress */
		ieee80211_cancel_scan(vap);
		if (ostate == IEEE80211_S_INIT ) {
			/* XXX don't believe this */
			/* INIT -> INIT. nothing to do */
			vap->iv_flags_ext &= ~IEEE80211_FEXT_SCANWAIT;
		}
		/* fall thru... */
	default:
		break;
	}
	/*
	 * Defer the state change to a thread.
	 * We support up-to NET80211_IV_NSTATE_NUM pending state changes
	 * using a separate task for each. Otherwise, if we enqueue
	 * more than one state change they will be folded together,
	 * npedning will be > 1 and we may run then out of sequence with
	 * other events.
	 * This is kind-of a hack after 10 years but we know how to provoke
	 * these cases now (and seen them in the wild).
	 */
	nstate_num = _ieee80211_newstate_get_next_empty_slot(vap);
	if (nstate_num == -1) {
		/*
		 * This is really bad and we should just go kaboom.
		 * Instead drop it.  No one checks the return code anyway.
		 */
		ic_printf(ic, "%s:%d: pending %s -> %s (now to %s) "
		    "transition lost. %d/%d pending state changes:\n",
		    __func__, __LINE__,
		    ieee80211_state_name[vap->iv_state],
		    ieee80211_state_name[vap->iv_nstate],
		    ieee80211_state_name[nstate],
		    _ieee80211_newstate_get_npending(vap),
		    NET80211_IV_NSTATE_NUM);

		return (EAGAIN);
	}
	vap->iv_nstates[nstate_num] = nstate;
	vap->iv_nstate_args[nstate_num] = arg;
	vap->iv_flags_ext |= IEEE80211_FEXT_STATEWAIT;
	ieee80211_runtask(ic, &vap->iv_nstate_task[nstate_num]);
	return EINPROGRESS;
}

int
ieee80211_new_state(struct ieee80211vap *vap,
	enum ieee80211_state nstate, int arg)
{
	struct ieee80211com *ic = vap->iv_ic;
	int rc;

	IEEE80211_LOCK(ic);
	rc = ieee80211_new_state_locked(vap, nstate, arg);
	IEEE80211_UNLOCK(ic);
	return rc;
}
