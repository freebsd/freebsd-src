/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
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
__FBSDID("$FreeBSD$");

/*
 * IEEE 802.11 protocol support.
 */

#include "opt_inet.h"
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>		/* XXX for ether_sprintf */

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_adhoc.h>
#include <net80211/ieee80211_sta.h>
#include <net80211/ieee80211_hostap.h>
#include <net80211/ieee80211_wds.h>
#include <net80211/ieee80211_monitor.h>
#include <net80211/ieee80211_input.h>

/* XXX tunables */
#define	AGGRESSIVE_MODE_SWITCH_HYSTERESIS	3	/* pkts / 100ms */
#define	HIGH_PRI_SWITCH_THRESH			10	/* pkts / 100ms */

const char *ieee80211_mgt_subtype_name[] = {
	"assoc_req",	"assoc_resp",	"reassoc_req",	"reassoc_resp",
	"probe_req",	"probe_resp",	"reserved#6",	"reserved#7",
	"beacon",	"atim",		"disassoc",	"auth",
	"deauth",	"action",	"reserved#14",	"reserved#15"
};
const char *ieee80211_ctl_subtype_name[] = {
	"reserved#0",	"reserved#1",	"reserved#2",	"reserved#3",
	"reserved#3",	"reserved#5",	"reserved#6",	"reserved#7",
	"reserved#8",	"reserved#9",	"ps_poll",	"rts",
	"cts",		"ack",		"cf_end",	"cf_end_ack"
};
const char *ieee80211_opmode_name[IEEE80211_OPMODE_MAX] = {
	"IBSS",		/* IEEE80211_M_IBSS */
	"STA",		/* IEEE80211_M_STA */
	"WDS",		/* IEEE80211_M_WDS */
	"AHDEMO",	/* IEEE80211_M_AHDEMO */
	"HOSTAP",	/* IEEE80211_M_HOSTAP */
	"MONITOR"	/* IEEE80211_M_MONITOR */
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

static void parent_updown(void *, int);
static int ieee80211_new_state_locked(struct ieee80211vap *,
	enum ieee80211_state, int);

static int
null_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
	const struct ieee80211_bpf_params *params)
{
	struct ifnet *ifp = ni->ni_ic->ic_ifp;

	if_printf(ifp, "missing ic_raw_xmit callback, drop frame\n");
	m_freem(m);
	return ENETDOWN;
}

void
ieee80211_proto_attach(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;

	/* override the 802.3 setting */
	ifp->if_hdrlen = ic->ic_headroom
		+ sizeof(struct ieee80211_qosframe_addr4)
		+ IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN
		+ IEEE80211_WEP_EXTIVLEN;
	/* XXX no way to recalculate on ifdetach */
	if (ALIGN(ifp->if_hdrlen) > max_linkhdr) {
		/* XXX sanity check... */
		max_linkhdr = ALIGN(ifp->if_hdrlen);
		max_hdr = max_linkhdr + max_protohdr;
		max_datalen = MHLEN - max_hdr;
	}
	ic->ic_protmode = IEEE80211_PROT_CTSONLY;

	TASK_INIT(&ic->ic_parent_task, 0, parent_updown, ifp);

	ic->ic_wme.wme_hipri_switch_hysteresis =
		AGGRESSIVE_MODE_SWITCH_HYSTERESIS;

	/* initialize management frame handlers */
	ic->ic_send_mgmt = ieee80211_send_mgmt;
	ic->ic_raw_xmit = null_raw_xmit;

	ieee80211_adhoc_attach(ic);
	ieee80211_sta_attach(ic);
	ieee80211_wds_attach(ic);
	ieee80211_hostap_attach(ic);
	ieee80211_monitor_attach(ic);
}

void
ieee80211_proto_detach(struct ieee80211com *ic)
{
	ieee80211_monitor_detach(ic);
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
	ifp->if_hdrlen = ic->ic_ifp->if_hdrlen;

	vap->iv_rtsthreshold = IEEE80211_RTS_DEFAULT;
	vap->iv_fragthreshold = IEEE80211_FRAG_DEFAULT;
	vap->iv_bmiss_max = IEEE80211_BMISS_MAX;
	callout_init(&vap->iv_swbmiss, CALLOUT_MPSAFE);
	callout_init(&vap->iv_mgtsend, CALLOUT_MPSAFE);
	/*
	 * Install default tx rate handling: no fixed rate, lowest
	 * supported rate for mgmt and multicast frames.  Default
	 * max retry count.  These settings can be changed by the
	 * driver and/or user applications.
	 */
	for (i = IEEE80211_MODE_11A; i < IEEE80211_MODE_11NA; i++) {
		const struct ieee80211_rateset *rs = &ic->ic_sup_rates[i];

		vap->iv_txparms[i].ucastrate = IEEE80211_FIXED_RATE_NONE;
		/* NB: we default to min supported rate for channel */
		vap->iv_txparms[i].mgmtrate =
		    rs->rs_rates[0] & IEEE80211_RATE_VAL;
		vap->iv_txparms[i].mcastrate = 
		    rs->rs_rates[0] & IEEE80211_RATE_VAL;
		vap->iv_txparms[i].maxretry = IEEE80211_TXMAX_DEFAULT;
	}
	for (; i < IEEE80211_MODE_MAX; i++) {
		vap->iv_txparms[i].ucastrate = IEEE80211_FIXED_RATE_NONE;
		/* NB: default to MCS 0 */
		vap->iv_txparms[i].mgmtrate = 0 | 0x80;
		vap->iv_txparms[i].mcastrate = 0 | 0x80;
		vap->iv_txparms[i].maxretry = IEEE80211_TXMAX_DEFAULT;
	}
	vap->iv_roaming = IEEE80211_ROAMING_AUTO;

	vap->iv_update_beacon = null_update_beacon;
	vap->iv_deliver_data = ieee80211_deliver_data;

	/* attach support for operating mode */
	ic->ic_vattach[vap->iv_opmode](vap);
}

void
ieee80211_proto_vdetach(struct ieee80211vap *vap)
{
#define	FREEAPPIE(ie) do { \
	if (ie != NULL) \
		FREE(ie, M_80211_NODE_IE); \
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
	printf("wlan: %s acl policy registered\n", iac->iac_name);
	acl = iac;
}

void
ieee80211_aclator_unregister(const struct ieee80211_aclator *iac)
{
	if (acl == iac)
		acl = NULL;
	printf("wlan: %s acl policy unregistered\n", iac->iac_name);
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
		printf("\"");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%c", *p);
		printf("\"");
	} else {
		printf("0x");
		for (i = 0, p = essid; i < len; i++, p++)
			printf("%02x", *p);
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
		printf("NODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr3));
		break;
	case IEEE80211_FC1_DIR_TODS:
		printf("TODS %s", ether_sprintf(wh->i_addr2));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s)", ether_sprintf(wh->i_addr1));
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		printf("FRDS %s", ether_sprintf(wh->i_addr3));
		printf("->%s", ether_sprintf(wh->i_addr1));
		printf("(%s)", ether_sprintf(wh->i_addr2));
		break;
	case IEEE80211_FC1_DIR_DSTODS:
		printf("DSDS %s", ether_sprintf((const uint8_t *)&wh[1]));
		printf("->%s", ether_sprintf(wh->i_addr3));
		printf("(%s", ether_sprintf(wh->i_addr2));
		printf("->%s)", ether_sprintf(wh->i_addr1));
		break;
	}
	switch (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) {
	case IEEE80211_FC0_TYPE_DATA:
		printf(" data");
		break;
	case IEEE80211_FC0_TYPE_MGT:
		printf(" %s", ieee80211_mgt_subtype_name[
		    (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK)
		    >> IEEE80211_FC0_SUBTYPE_SHIFT]);
		break;
	default:
		printf(" type#%d", wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK);
		break;
	}
	if (IEEE80211_QOS_HAS_SEQ(wh)) {
		const struct ieee80211_qosframe *qwh = 
			(const struct ieee80211_qosframe *)buf;
		printf(" QoS [TID %u%s]", qwh->i_qos[0] & IEEE80211_QOS_TID,
			qwh->i_qos[0] & IEEE80211_QOS_ACKPOLICY ? " ACM" : "");
	}
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		int off;

		off = ieee80211_anyhdrspace(ic, wh);
		printf(" WEP [IV %.02x %.02x %.02x",
			buf[off+0], buf[off+1], buf[off+2]);
		if (buf[off+IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV)
			printf(" %.02x %.02x %.02x",
				buf[off+4], buf[off+5], buf[off+6]);
		printf(" KID %u]", buf[off+IEEE80211_WEP_IVLEN] >> 6);
	}
	if (rate >= 0)
		printf(" %dM", rate / 2);
	if (rssi >= 0)
		printf(" +%d", rssi);
	printf("\n");
	if (len > 0) {
		for (i = 0; i < len; i++) {
			if ((i & 1) == 0)
				printf(" ");
			printf("%02x", buf[i]);
		}
		printf("\n");
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
#define	RV(v)	((v) & IEEE80211_RATE_VAL)
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
				if (RV(nrs->rs_rates[i]) > RV(nrs->rs_rates[j])) {
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
		return RV(okrate);
#undef RV
}

/*
 * Reset 11g-related state.
 */
void
ieee80211_reset_erp(struct ieee80211com *ic)
{
	ic->ic_flags &= ~IEEE80211_F_USEPROT;
	ic->ic_nonerpsta = 0;
	ic->ic_longslotsta = 0;
	/*
	 * Short slot time is enabled only when operating in 11g
	 * and not in an IBSS.  We must also honor whether or not
	 * the driver is capable of doing it.
	 */
	ieee80211_set_shortslottime(ic,
		IEEE80211_IS_CHAN_A(ic->ic_curchan) ||
		IEEE80211_IS_CHAN_HT(ic->ic_curchan) ||
		(IEEE80211_IS_CHAN_ANYG(ic->ic_curchan) &&
		ic->ic_opmode == IEEE80211_M_HOSTAP &&
		(ic->ic_caps & IEEE80211_C_SHSLOT)));
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
}

/*
 * Set the short slot time state and notify the driver.
 */
void
ieee80211_set_shortslottime(struct ieee80211com *ic, int onoff)
{
	if (onoff)
		ic->ic_flags |= IEEE80211_F_SHSLOT;
	else
		ic->ic_flags &= ~IEEE80211_F_SHSLOT;
	/* notify driver */
	if (ic->ic_updateslot != NULL)
		ic->ic_updateslot(ic->ic_ifp);
}

/*
 * Check if the specified rate set supports ERP.
 * NB: the rate set is assumed to be sorted.
 */
int
ieee80211_iserp_rateset(const struct ieee80211_rateset *rs)
{
#define N(a)	(sizeof(a) / sizeof(a[0]))
	static const int rates[] = { 2, 4, 11, 22, 12, 24, 48 };
	int i, j;

	if (rs->rs_nrates < N(rates))
		return 0;
	for (i = 0; i < N(rates); i++) {
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
#undef N
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
	    { .rs_nrates = 0 },		/* IEEE80211_MODE_AUTO */
	    { 3, { 12, 24, 48 } },	/* IEEE80211_MODE_11A */
	    { 2, { 2, 4 } },		/* IEEE80211_MODE_11B */
	    { 4, { 2, 4, 11, 22 } },	/* IEEE80211_MODE_11G (mixed b/g) */
	    { .rs_nrates = 0 },		/* IEEE80211_MODE_FH */
	    { 3, { 12, 24, 48 } },	/* IEEE80211_MODE_TURBO_A */
	    { 4, { 2, 4, 11, 22 } },	/* IEEE80211_MODE_TURBO_G (mixed b/g) */
	    { 3, { 12, 24, 48 } },	/* IEEE80211_MODE_STURBO_A */
	    { 3, { 12, 24, 48 } },	/* IEEE80211_MODE_11NA */
	    { 4, { 2, 4, 11, 22 } },	/* IEEE80211_MODE_11NG (mixed b/g) */
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
	{ 3, 4,  6,  0, 0 },	/* IEEE80211_MODE_AUTO */
	{ 3, 4,  6,  0, 0 },	/* IEEE80211_MODE_11A */
	{ 3, 4,  6,  0, 0 },	/* IEEE80211_MODE_11B */
	{ 3, 4,  6,  0, 0 },	/* IEEE80211_MODE_11G */
	{ 3, 4,  6,  0, 0 },	/* IEEE80211_MODE_FH */
	{ 2, 3,  5,  0, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 2, 3,  5,  0, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 2, 3,  5,  0, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 3, 4,  6,  0, 0 },	/* IEEE80211_MODE_11NA */
	{ 3, 4,  6,  0, 0 },	/* IEEE80211_MODE_11NG */
};
static const struct phyParamType phyParamForAC_BK[IEEE80211_MODE_MAX] = {
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_AUTO */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_11A */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_11B */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_11G */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_FH */
	{ 7, 3, 10,  0, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 7, 3, 10,  0, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 7, 3, 10,  0, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_11NA */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_11NG */
};
static const struct phyParamType phyParamForAC_VI[IEEE80211_MODE_MAX] = {
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_AUTO */
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_11A */
	{ 1, 3, 4, 188, 0 },	/* IEEE80211_MODE_11B */
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_11G */
	{ 1, 3, 4, 188, 0 },	/* IEEE80211_MODE_FH */
	{ 1, 2, 3,  94, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 1, 2, 3,  94, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 1, 2, 3,  94, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_11NA */
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_11NG */
};
static const struct phyParamType phyParamForAC_VO[IEEE80211_MODE_MAX] = {
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_AUTO */
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_11A */
	{ 1, 2, 3, 102, 0 },	/* IEEE80211_MODE_11B */
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_11G */
	{ 1, 2, 3, 102, 0 },	/* IEEE80211_MODE_FH */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_11NA */
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_11NG */
};

static const struct phyParamType bssPhyParamForAC_BE[IEEE80211_MODE_MAX] = {
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_AUTO */
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_11A */
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_11B */
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_11G */
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_FH */
	{ 2, 3, 10,  0, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 2, 3, 10,  0, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 2, 3, 10,  0, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_11NA */
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_11NG */
};
static const struct phyParamType bssPhyParamForAC_VI[IEEE80211_MODE_MAX] = {
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_AUTO */
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_11A */
	{ 2, 3, 4, 188, 0 },	/* IEEE80211_MODE_11B */
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_11G */
	{ 2, 3, 4, 188, 0 },	/* IEEE80211_MODE_FH */
	{ 2, 2, 3,  94, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 2, 2, 3,  94, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 2, 2, 3,  94, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_11NA */
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_11NG */
};
static const struct phyParamType bssPhyParamForAC_VO[IEEE80211_MODE_MAX] = {
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_AUTO */
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_11A */
	{ 2, 2, 3, 102, 0 },	/* IEEE80211_MODE_11B */
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_11G */
	{ 2, 2, 3, 102, 0 },	/* IEEE80211_MODE_FH */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_11NA */
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_11NG */
};

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

	if ((ic->ic_caps & IEEE80211_C_WME) == 0)
		return;

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
			wmep->wmep_acm = pPhyParam->acm;
			wmep->wmep_aifsn = pPhyParam->aifsn;	
			wmep->wmep_logcwmin = pPhyParam->logcwmin;	
			wmep->wmep_logcwmax = pPhyParam->logcwmax;		
			wmep->wmep_txopLimit = pPhyParam->txopLimit;
		} else {
			wmep->wmep_acm = pBssPhyParam->acm;
			wmep->wmep_aifsn = pBssPhyParam->aifsn;	
			wmep->wmep_logcwmin = pBssPhyParam->logcwmin;	
			wmep->wmep_logcwmax = pBssPhyParam->logcwmax;		
			wmep->wmep_txopLimit = pBssPhyParam->txopLimit;

		}	
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
			"%s: %s chan [acm %u aifsn %u log2(cwmin) %u "
			"log2(cwmax) %u txpoLimit %u]\n", __func__
			, ieee80211_wme_acnames[i]
			, wmep->wmep_acm
			, wmep->wmep_aifsn
			, wmep->wmep_logcwmin
			, wmep->wmep_logcwmax
			, wmep->wmep_txopLimit
		);

		wmep = &wme->wme_wmeBssChanParams.cap_wmeParams[i];
		wmep->wmep_acm = pBssPhyParam->acm;
		wmep->wmep_aifsn = pBssPhyParam->aifsn;	
		wmep->wmep_logcwmin = pBssPhyParam->logcwmin;	
		wmep->wmep_logcwmax = pBssPhyParam->logcwmax;		
		wmep->wmep_txopLimit = pBssPhyParam->txopLimit;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
			"%s: %s  bss [acm %u aifsn %u log2(cwmin) %u "
			"log2(cwmax) %u txpoLimit %u]\n", __func__
			, ieee80211_wme_acnames[i]
			, wmep->wmep_acm
			, wmep->wmep_aifsn
			, wmep->wmep_logcwmin
			, wmep->wmep_logcwmax
			, wmep->wmep_txopLimit
		);
	}
	/* NB: check ic_bss to avoid NULL deref on initial attach */
	if (vap->iv_bss != NULL) {
		/*
		 * Calculate agressive mode switching threshold based
		 * on beacon interval.  This doesn't need locking since
		 * we're only called before entering the RUN state at
		 * which point we start sending beacon frames.
		 */
		wme->wme_hipri_switch_thresh =
			(HIGH_PRI_SWITCH_THRESH * vap->iv_bss->ni_intval) / 100;
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
	static const paramType phyParam[IEEE80211_MODE_MAX] = {
		{ 2, 4, 10, 64, 0 },	/* IEEE80211_MODE_AUTO */
		{ 2, 4, 10, 64, 0 },	/* IEEE80211_MODE_11A */
		{ 2, 5, 10, 64, 0 },	/* IEEE80211_MODE_11B */
		{ 2, 4, 10, 64, 0 },	/* IEEE80211_MODE_11G */
		{ 2, 5, 10, 64, 0 },	/* IEEE80211_MODE_FH */
		{ 1, 3, 10, 64, 0 },	/* IEEE80211_MODE_TURBO_A */
		{ 1, 3, 10, 64, 0 },	/* IEEE80211_MODE_TURBO_G */
		{ 1, 3, 10, 64, 0 },	/* IEEE80211_MODE_STURBO_A */
		{ 2, 4, 10, 64, 0 },	/* IEEE80211_MODE_11NA */ /*XXXcheck*/
		{ 2, 4, 10, 64, 0 },	/* IEEE80211_MODE_11NG */ /*XXXcheck*/
	};
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	const struct wmeParams *wmep;
	struct wmeParams *chanp, *bssp;
	enum ieee80211_phymode mode;
	int i;

       	/* set up the channel access parameters for the physical device */
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
	 * This implements agressive mode as found in certain
	 * vendors' AP's.  When there is significant high
	 * priority (VI/VO) traffic in the BSS throttle back BE
	 * traffic by using conservative parameters.  Otherwise
	 * BE uses agressive params to optimize performance of
	 * legacy/non-QoS traffic.
	 */
        if ((vap->iv_opmode == IEEE80211_M_HOSTAP &&
	     (wme->wme_flags & WME_F_AGGRMODE) != 0) ||
	    (vap->iv_opmode == IEEE80211_M_STA &&
	     (vap->iv_bss->ni_flags & IEEE80211_NODE_QOS) == 0) ||
	    (vap->iv_flags & IEEE80211_F_WME) == 0) {
		chanp = &wme->wme_chanParams.cap_wmeParams[WME_AC_BE];
		bssp = &wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE];

		chanp->wmep_aifsn = bssp->wmep_aifsn = phyParam[mode].aifsn;
		chanp->wmep_logcwmin = bssp->wmep_logcwmin =
			phyParam[mode].logcwmin;
		chanp->wmep_logcwmax = bssp->wmep_logcwmax =
			phyParam[mode].logcwmax;
		chanp->wmep_txopLimit = bssp->wmep_txopLimit =
			(vap->iv_flags & IEEE80211_F_BURST) ?
				phyParam[mode].txopLimit : 0;		
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
			"%s: %s [acm %u aifsn %u log2(cwmin) %u "
			"log2(cwmax) %u txpoLimit %u]\n", __func__
			, ieee80211_wme_acnames[WME_AC_BE]
			, chanp->wmep_acm
			, chanp->wmep_aifsn
			, chanp->wmep_logcwmin
			, chanp->wmep_logcwmax
			, chanp->wmep_txopLimit
		);
	}
	
	/* XXX multi-bss */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP &&
	    ic->ic_sta_assoc < 2 && (wme->wme_flags & WME_F_AGGRMODE) != 0) {
        	static const uint8_t logCwMin[IEEE80211_MODE_MAX] = {
              		3,	/* IEEE80211_MODE_AUTO */
              		3,	/* IEEE80211_MODE_11A */
              		4,	/* IEEE80211_MODE_11B */
              		3,	/* IEEE80211_MODE_11G */
              		4,	/* IEEE80211_MODE_FH */
              		3,	/* IEEE80211_MODE_TURBO_A */
              		3,	/* IEEE80211_MODE_TURBO_G */
              		3,	/* IEEE80211_MODE_STURBO_A */
              		3,	/* IEEE80211_MODE_11NA */
              		3,	/* IEEE80211_MODE_11NG */
		};
		chanp = &wme->wme_chanParams.cap_wmeParams[WME_AC_BE];
		bssp = &wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE];

		chanp->wmep_logcwmin = bssp->wmep_logcwmin = logCwMin[mode];
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WME,
			"%s: %s log2(cwmin) %u\n", __func__
			, ieee80211_wme_acnames[WME_AC_BE]
			, chanp->wmep_logcwmin
		);
    	}	
	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {	/* XXX ibss? */
		/*
		 * Arrange for a beacon update and bump the parameter
		 * set number so associated stations load the new values.
		 */
		wme->wme_bssChanParams.cap_info =
			(wme->wme_bssChanParams.cap_info+1) & WME_QOSINFO_COUNT;
		ieee80211_beacon_notify(vap, IEEE80211_BEACON_WME);
	}

	wme->wme_update(ic);

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

static void
parent_updown(void *arg, int npending)
{
	struct ifnet *parent = arg;

	parent->if_ioctl(parent, SIOCSIFFLAGS, NULL);
}

/*
 * Start a vap running.  If this is the first vap to be
 * set running on the underlying device then we
 * automatically bring the device up.
 */
void
ieee80211_start_locked(struct ieee80211vap *vap)
{
	struct ifnet *ifp = vap->iv_ifp;
	struct ieee80211com *ic = vap->iv_ic;
	struct ifnet *parent = ic->ic_ifp;

	IEEE80211_LOCK_ASSERT(ic);

	IEEE80211_DPRINTF(vap,
		IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
		"start running, %d vaps running\n", ic->ic_nrunning);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		/*
		 * Mark us running.  Note that it's ok to do this first;
		 * if we need to bring the parent device up we defer that
		 * to avoid dropping the com lock.  We expect the device
		 * to respond to being marked up by calling back into us
		 * through ieee80211_start_all at which point we'll come
		 * back in here and complete the work.
		 */
		ifp->if_drv_flags |= IFF_DRV_RUNNING;
		/*
		 * We are not running; if this we are the first vap
		 * to be brought up auto-up the parent if necessary.
		 */
		if (ic->ic_nrunning++ == 0 &&
		    (parent->if_drv_flags & IFF_DRV_RUNNING) == 0) {
			IEEE80211_DPRINTF(vap,
			    IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
			    "%s: up parent %s\n", __func__, parent->if_xname);
			parent->if_flags |= IFF_UP;
			taskqueue_enqueue(taskqueue_thread, &ic->ic_parent_task);
			return;
		}
	}
	/*
	 * If the parent is up and running, then kick the
	 * 802.11 state machine as appropriate.
	 */
	if ((parent->if_drv_flags & IFF_DRV_RUNNING) &&
	    vap->iv_roaming != IEEE80211_ROAMING_MANUAL) {
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
			/* XXX needed? */
			ieee80211_new_state_locked(vap, IEEE80211_S_INIT, 0);
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

	/*
	 * This routine is publicly accessible through the vap's
	 * if_init method so guard against calls during detach.
	 * ieee80211_vap_detach null's the backpointer before
	 * tearing down state to signal any callback should be
	 * rejected/ignored.
	 */
	if (vap != NULL) {
		IEEE80211_DPRINTF(vap,
		    IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
		    "%s\n", __func__);

		IEEE80211_LOCK(vap->iv_ic);
		ieee80211_start_locked(vap);
		IEEE80211_UNLOCK(vap->iv_ic);
	}
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
	struct ifnet *ifp = vap->iv_ifp;
	struct ifnet *parent = ic->ic_ifp;

	IEEE80211_LOCK_ASSERT(ic);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
	    "stop running, %d vaps running\n", ic->ic_nrunning);

	ieee80211_new_state_locked(vap, IEEE80211_S_INIT, -1);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;	/* mark us stopped */
		if (--ic->ic_nrunning == 0 &&
		    (parent->if_drv_flags & IFF_DRV_RUNNING)) {
			IEEE80211_DPRINTF(vap,
			    IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
			    "down parent %s\n", parent->if_xname);
			parent->if_flags &= ~IFF_UP;
			taskqueue_enqueue(taskqueue_thread, &ic->ic_parent_task);
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
 * Switch between turbo and non-turbo operating modes.
 * Use the specified channel flags to locate the new
 * channel, update 802.11 state, and then call back into
 * the driver to effect the change.
 */
void
ieee80211_dturbo_switch(struct ieee80211vap *vap, int newflags)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_channel *chan;

	chan = ieee80211_find_channel(ic, ic->ic_bsschan->ic_freq, newflags);
	if (chan == NULL) {		/* XXX should not happen */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SUPERG,
		    "%s: no channel with freq %u flags 0x%x\n",
		    __func__, ic->ic_bsschan->ic_freq, newflags);
		return;
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SUPERG,
	    "%s: %s -> %s (freq %u flags 0x%x)\n", __func__,
	    ieee80211_phymode_name[ieee80211_chan2mode(ic->ic_bsschan)],
	    ieee80211_phymode_name[ieee80211_chan2mode(chan)],
	    chan->ic_freq, chan->ic_flags);

	ic->ic_bsschan = chan;
	ic->ic_prevchan = ic->ic_curchan;
	ic->ic_curchan = chan;
	ic->ic_set_channel(ic);
	/* NB: do not need to reset ERP state 'cuz we're in sta mode */
}

void
ieee80211_beacon_miss(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	if (ic->ic_flags & IEEE80211_F_SCAN)
		return;
	/* XXX locking */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		/*
		 * We only pass events through for sta vap's in RUN state;
		 * may be too restrictive but for now this saves all the
		 * handlers duplicating these checks.
		 */
		if (vap->iv_opmode == IEEE80211_M_STA &&
		    vap->iv_state == IEEE80211_S_RUN &&
		    vap->iv_bmiss != NULL)
			vap->iv_bmiss(vap);
	}
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

	/* XXX sleep state? */
	KASSERT(vap->iv_state == IEEE80211_S_RUN,
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
			vap->iv_bmiss(vap);
		if (vap->iv_bmiss_count == 0)	/* don't re-arm timer */
			return;
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
	ic->ic_csa_count = count;
	/* XXX record mode? */
	ic->ic_flags |= IEEE80211_F_CSAPENDING;
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_opmode == IEEE80211_M_HOSTAP ||
		    vap->iv_opmode == IEEE80211_M_IBSS)
			ieee80211_beacon_notify(vap, IEEE80211_BEACON_CSA);
		/* switch to CSA state to block outbound traffic */
		if (vap->iv_state == IEEE80211_S_RUN)
			ieee80211_new_state_locked(vap, IEEE80211_S_CSA, 0);
	}
	ieee80211_notify_csa(ic, c, mode, count);
}

/*
 * Complete an 802.11h channel switch started by ieee80211_csa_startswitch.
 * We clear state and move all vap's in CSA state to RUN state
 * so they can again transmit.
 */
void
ieee80211_csa_completeswitch(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	IEEE80211_LOCK_ASSERT(ic);

	KASSERT(ic->ic_flags & IEEE80211_F_CSAPENDING, ("csa not pending"));

	ieee80211_setcurchan(ic, ic->ic_csa_newchan);
	ic->ic_csa_newchan = NULL;
	ic->ic_flags &= ~IEEE80211_F_CSAPENDING;

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		if (vap->iv_state == IEEE80211_S_CSA)
			ieee80211_new_state_locked(vap, IEEE80211_S_RUN, 0);
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
		if (vap->iv_state == IEEE80211_S_CAC)
			ieee80211_new_state_locked(vap, IEEE80211_S_RUN, 0);
	IEEE80211_UNLOCK(ic);
}

/*
 * Force all vap's other than the specified vap to the INIT state
 * and mark them as waiting for a scan to complete.  These vaps
 * will be brought up when the scan completes and the scanning vap
 * reaches RUN state by wakeupwaiting.
 * XXX if we do this in threads we can use sleep/wakeup.
 */
static void
markwaiting(struct ieee80211vap *vap0)
{
	struct ieee80211com *ic = vap0->iv_ic;
	struct ieee80211vap *vap;

	IEEE80211_LOCK_ASSERT(ic);

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap == vap0)
			continue;
		if (vap->iv_state != IEEE80211_S_INIT) {
			vap->iv_newstate(vap, IEEE80211_S_INIT, 0);
			vap->iv_flags_ext |= IEEE80211_FEXT_SCANWAIT;
		}
	}
}

/*
 * Wakeup all vap's waiting for a scan to complete.  This is the
 * companion to markwaiting (above) and is used to coordinate
 * multiple vaps scanning.
 */
static void
wakeupwaiting(struct ieee80211vap *vap0)
{
	struct ieee80211com *ic = vap0->iv_ic;
	struct ieee80211vap *vap;

	IEEE80211_LOCK_ASSERT(ic);

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap == vap0)
			continue;
		if (vap->iv_flags_ext & IEEE80211_FEXT_SCANWAIT) {
			vap->iv_flags_ext &= ~IEEE80211_FEXT_SCANWAIT;
			/* NB: sta's cannot go INIT->RUN */
			vap->iv_newstate(vap,
			    vap->iv_opmode == IEEE80211_M_STA ?
			        IEEE80211_S_SCAN : IEEE80211_S_RUN, 0);
		}
	}
}

/*
 * Handle post state change work common to all operating modes.
 */
static void
ieee80211_newstate_cb(struct ieee80211vap *vap, 
	enum ieee80211_state nstate, int arg)
{
	struct ieee80211com *ic = vap->iv_ic;

	IEEE80211_LOCK_ASSERT(ic);

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
	    "%s: %s arg %d\n", __func__, ieee80211_state_name[nstate], arg);

	if (nstate == IEEE80211_S_RUN) {
		/*
		 * OACTIVE may be set on the vap if the upper layer
		 * tried to transmit (e.g. IPv6 NDP) before we reach
		 * RUN state.  Clear it and restart xmit.
		 *
		 * Note this can also happen as a result of SLEEP->RUN
		 * (i.e. coming out of power save mode).
		 */
		vap->iv_ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		if_start(vap->iv_ifp);

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

		/* XXX NB: cast for altq */
		ieee80211_flush_ifq((struct ifqueue *)&ic->ic_ifp->if_snd, vap);
	}
	vap->iv_newstate_cb = NULL;
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
static int
ieee80211_new_state_locked(struct ieee80211vap *vap,
	enum ieee80211_state nstate, int arg)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211vap *vp;
	enum ieee80211_state ostate;
	int nrunning, nscanning, rc;

	IEEE80211_LOCK_ASSERT(ic);

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
	ostate = vap->iv_state;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
	    "%s: %s -> %s (nrunning %d nscanning %d)\n", __func__,
	    ieee80211_state_name[ostate], ieee80211_state_name[nstate],
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
				rc = 0;
				goto done;
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
		} else {
			/*
			 * SCAN was forced; e.g. on beacon miss.  Force
			 * other running vap's to INIT state and mark
			 * them as waiting for the scan to complete.  This
			 * insures they don't interfere with our scanning.
			 *
			 * XXX not always right, assumes ap follows sta
			 */
			markwaiting(vap);
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
			rc = 0;
			goto done;
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
		if (ostate == IEEE80211_S_INIT ) {
			/* XXX don't believe this */
			/* INIT -> INIT. nothing to do */
			vap->iv_flags_ext &= ~IEEE80211_FEXT_SCANWAIT;
		}
		/* fall thru... */
	default:
		break;
	}
	/* XXX on transition RUN->CAC do we need to set nstate = iv_state? */
	if (ostate != nstate) {
		/*
		 * Arrange for work to happen after state change completes.
		 * If this happens asynchronously the caller must arrange
		 * for the com lock to be held.
		 */
		vap->iv_newstate_cb = ieee80211_newstate_cb;
	}
	rc = vap->iv_newstate(vap, nstate, arg);
	if (rc == 0 && vap->iv_newstate_cb != NULL)
		vap->iv_newstate_cb(vap, nstate, arg);
done:
	return rc;
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
