/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>		/* XXX for ether_sprintf */

#include <net80211/ieee80211_var.h>

/* XXX tunables */
#define	AGGRESSIVE_MODE_SWITCH_HYSTERESIS	3	/* pkts / 100ms */
#define	HIGH_PRI_SWITCH_THRESH			10	/* pkts / 100ms */

#define	IEEE80211_RATE2MBS(r)	(((r) & IEEE80211_RATE_VAL) / 2)

const char *ieee80211_mgt_subtype_name[] = {
	"assoc_req",	"assoc_resp",	"reassoc_req",	"reassoc_resp",
	"probe_req",	"probe_resp",	"reserved#6",	"reserved#7",
	"beacon",	"atim",		"disassoc",	"auth",
	"deauth",	"reserved#13",	"reserved#14",	"reserved#15"
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
	"#2",
	"AHDEMO",	/* IEEE80211_M_AHDEMO */
	"#4", "#5",
	"HOSTAP",	/* IEEE80211_M_HOSTAP */
	"#7",
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

static int ieee80211_newstate(struct ieee80211com *, enum ieee80211_state, int);

void
ieee80211_proto_attach(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;

	/* XXX room for crypto  */
	ifp->if_hdrlen = sizeof(struct ieee80211_qosframe_addr4);

	ic->ic_rtsthreshold = IEEE80211_RTS_DEFAULT;
	ic->ic_fragthreshold = IEEE80211_FRAG_DEFAULT;
	ic->ic_fixed_rate = IEEE80211_FIXED_RATE_NONE;
	ic->ic_bmiss_max = IEEE80211_BMISS_MAX;
	callout_init(&ic->ic_swbmiss, CALLOUT_MPSAFE);
	callout_init(&ic->ic_mgtsend, CALLOUT_MPSAFE);
	ic->ic_mcast_rate = IEEE80211_MCAST_RATE_DEFAULT;
	ic->ic_protmode = IEEE80211_PROT_CTSONLY;
	ic->ic_roaming = IEEE80211_ROAMING_AUTO;

	ic->ic_wme.wme_hipri_switch_hysteresis =
		AGGRESSIVE_MODE_SWITCH_HYSTERESIS;

	mtx_init(&ic->ic_mgtq.ifq_mtx, ifp->if_xname, "mgmt send q", MTX_DEF);

	/* protocol state change handler */
	ic->ic_newstate = ieee80211_newstate;

	/* initialize management frame handlers */
	ic->ic_recv_mgmt = ieee80211_recv_mgmt;
	ic->ic_send_mgmt = ieee80211_send_mgmt;
	ic->ic_raw_xmit = ieee80211_raw_xmit;
}

void
ieee80211_proto_detach(struct ieee80211com *ic)
{

	/*
	 * This should not be needed as we detach when reseting
	 * the state but be conservative here since the
	 * authenticator may do things like spawn kernel threads.
	 */
	if (ic->ic_auth->ia_detach)
		ic->ic_auth->ia_detach(ic);

	ieee80211_drain_ifq(&ic->ic_mgtq);
	mtx_destroy(&ic->ic_mgtq.ifq_mtx);

	/*
	 * Detach any ACL'ator.
	 */
	if (ic->ic_acl != NULL)
		ic->ic_acl->iac_detach(ic);
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
	struct ieee80211com *ic = ni->ni_ic;
	int i, j, rix, error;
	int okrate, badrate, fixedrate;
	const struct ieee80211_rateset *srs;
	uint8_t r;

	error = 0;
	okrate = badrate = 0;
	fixedrate = IEEE80211_FIXED_RATE_NONE;
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
		if (r == ic->ic_fixed_rate)
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
	    ((flags & IEEE80211_F_DOFRATE) && fixedrate != ic->ic_fixed_rate))
		return badrate | IEEE80211_RATE_BASIC;
	else
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
ieee80211_iserp_rateset(struct ieee80211com *ic, struct ieee80211_rateset *rs)
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
 * Mark the basic rates for the 11g rate table based on the
 * operating mode.  For real 11g we mark all the 11b rates
 * and 6, 12, and 24 OFDM.  For 11b compatibility we mark only
 * 11b rates.  There's also a pseudo 11a-mode used to mark only
 * the basic OFDM rates.
 */
void
ieee80211_set11gbasicrates(struct ieee80211_rateset *rs, enum ieee80211_phymode mode)
{
	static const struct ieee80211_rateset basic[IEEE80211_MODE_MAX] = {
	    { .rs_nrates = 0 },		/* IEEE80211_MODE_AUTO */
	    { 3, { 12, 24, 48 } },	/* IEEE80211_MODE_11A */
	    { 2, { 2, 4 } },		/* IEEE80211_MODE_11B */
	    { 4, { 2, 4, 11, 22 } },	/* IEEE80211_MODE_11G (mixed b/g) */
	    { .rs_nrates = 0 },		/* IEEE80211_MODE_FH */
					/* IEEE80211_MODE_PUREG (not yet) */
	    { 7, { 2, 4, 11, 22, 12, 24, 48 } },
	    { 3, { 12, 24, 48 } },	/* IEEE80211_MODE_11NA */
					/* IEEE80211_MODE_11NG (mixed b/g) */
	    { 7, { 2, 4, 11, 22, 12, 24, 48 } },
	};
	int i, j;

	for (i = 0; i < rs->rs_nrates; i++) {
		rs->rs_rates[i] &= IEEE80211_RATE_VAL;
		for (j = 0; j < basic[mode].rs_nrates; j++)
			if (basic[mode].rs_rates[j] == rs->rs_rates[i]) {
				rs->rs_rates[i] |= IEEE80211_RATE_BASIC;
				break;
			}
	}
}

/*
 * WME protocol support.  The following parameters come from the spec.
 */
typedef struct phyParamType {
	uint8_t aifsn;
	uint8_t logcwmin;
	uint8_t logcwmax;
	uint16_t txopLimit;
	uint8_t acm;
} paramType;

static const struct phyParamType phyParamForAC_BE[IEEE80211_MODE_MAX] = {
	{ 3, 4, 6,   0, 0 },	/* IEEE80211_MODE_AUTO */
	{ 3, 4, 6,   0, 0 },	/* IEEE80211_MODE_11A */
	{ 3, 5, 7,   0, 0 },	/* IEEE80211_MODE_11B */
	{ 3, 4, 6,   0, 0 },	/* IEEE80211_MODE_11G */
	{ 3, 5, 7,   0, 0 },	/* IEEE80211_MODE_FH */
	{ 2, 3, 5,   0, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 2, 3, 5,   0, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 2, 3, 5,   0, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 3, 4, 6,   0, 0 },	/* IEEE80211_MODE_11NA */	/* XXXcheck*/
	{ 3, 4, 6,   0, 0 },	/* IEEE80211_MODE_11NG */	/* XXXcheck*/
};
static const struct phyParamType phyParamForAC_BK[IEEE80211_MODE_MAX] = {
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_AUTO */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_11A */
	{ 7, 5, 10,  0, 0 },	/* IEEE80211_MODE_11B */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_11G */
	{ 7, 5, 10,  0, 0 },	/* IEEE80211_MODE_FH */
	{ 7, 3, 10,  0, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 7, 3, 10,  0, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 7, 3, 10,  0, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_11NA */
	{ 7, 4, 10,  0, 0 },	/* IEEE80211_MODE_11NG */
};
static const struct phyParamType phyParamForAC_VI[IEEE80211_MODE_MAX] = {
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_AUTO */
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_11A */
	{ 1, 4, 5, 188, 0 },	/* IEEE80211_MODE_11B */
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_11G */
	{ 1, 4, 5, 188, 0 },	/* IEEE80211_MODE_FH */
	{ 1, 2, 3,  94, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 1, 2, 3,  94, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 1, 2, 3,  94, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_11NA */
	{ 1, 3, 4,  94, 0 },	/* IEEE80211_MODE_11NG */
};
static const struct phyParamType phyParamForAC_VO[IEEE80211_MODE_MAX] = {
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_AUTO */
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_11A */
	{ 1, 3, 4, 102, 0 },	/* IEEE80211_MODE_11B */
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_11G */
	{ 1, 3, 4, 102, 0 },	/* IEEE80211_MODE_FH */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_11NA */
	{ 1, 2, 3,  47, 0 },	/* IEEE80211_MODE_11NG */
};

static const struct phyParamType bssPhyParamForAC_BE[IEEE80211_MODE_MAX] = {
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_AUTO */
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_11A */
	{ 3, 5, 10,  0, 0 },	/* IEEE80211_MODE_11B */
	{ 3, 4, 10,  0, 0 },	/* IEEE80211_MODE_11G */
	{ 3, 5, 10,  0, 0 },	/* IEEE80211_MODE_FH */
	{ 2, 3, 10,  0, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 2, 3, 10,  0, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 2, 3, 10,  0, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 1, 4, 10,  0, 0 },	/* IEEE80211_MODE_11NA */
	{ 1, 4, 10,  0, 0 },	/* IEEE80211_MODE_11NG */
};
static const struct phyParamType bssPhyParamForAC_VI[IEEE80211_MODE_MAX] = {
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_AUTO */
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_11A */
	{ 2, 4, 5, 188, 0 },	/* IEEE80211_MODE_11B */
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_11G */
	{ 2, 4, 5, 188, 0 },	/* IEEE80211_MODE_FH */
	{ 2, 2, 3,  94, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 2, 2, 3,  94, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 2, 2, 3,  94, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_11NA */
	{ 2, 3, 4,  94, 0 },	/* IEEE80211_MODE_11NG */
};
static const struct phyParamType bssPhyParamForAC_VO[IEEE80211_MODE_MAX] = {
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_AUTO */
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_11A */
	{ 2, 3, 4, 102, 0 },	/* IEEE80211_MODE_11B */
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_11G */
	{ 2, 3, 4, 102, 0 },	/* IEEE80211_MODE_FH */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_TURBO_A */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_TURBO_G */
	{ 1, 2, 2,  47, 0 },	/* IEEE80211_MODE_STURBO_A */
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_11NA */
	{ 2, 2, 3,  47, 0 },	/* IEEE80211_MODE_11NG */
};

void
ieee80211_wme_initparams(struct ieee80211com *ic)
{
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	const paramType *pPhyParam, *pBssPhyParam;
	struct wmeParams *wmep;
	enum ieee80211_phymode mode;
	int i;

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
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
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
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
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
	if (ic->ic_bss != NULL) {
		/*
		 * Calculate agressive mode switching threshold based
		 * on beacon interval.  This doesn't need locking since
		 * we're only called before entering the RUN state at
		 * which point we start sending beacon frames.
		 */
		wme->wme_hipri_switch_thresh =
			(HIGH_PRI_SWITCH_THRESH * ic->ic_bss->ni_intval) / 100;
		ieee80211_wme_updateparams(ic);
	}
}

/*
 * Update WME parameters for ourself and the BSS.
 */
void
ieee80211_wme_updateparams_locked(struct ieee80211com *ic)
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
        if ((ic->ic_opmode == IEEE80211_M_HOSTAP &&
	     (wme->wme_flags & WME_F_AGGRMODE) != 0) ||
	    (ic->ic_opmode == IEEE80211_M_STA &&
	     (ic->ic_bss->ni_flags & IEEE80211_NODE_QOS) == 0) ||
	    (ic->ic_flags & IEEE80211_F_WME) == 0) {
		chanp = &wme->wme_chanParams.cap_wmeParams[WME_AC_BE];
		bssp = &wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE];

		chanp->wmep_aifsn = bssp->wmep_aifsn = phyParam[mode].aifsn;
		chanp->wmep_logcwmin = bssp->wmep_logcwmin =
			phyParam[mode].logcwmin;
		chanp->wmep_logcwmax = bssp->wmep_logcwmax =
			phyParam[mode].logcwmax;
		chanp->wmep_txopLimit = bssp->wmep_txopLimit =
			(ic->ic_flags & IEEE80211_F_BURST) ?
				phyParam[mode].txopLimit : 0;		
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
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
	
	if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
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
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
			"%s: %s log2(cwmin) %u\n", __func__
			, ieee80211_wme_acnames[WME_AC_BE]
			, chanp->wmep_logcwmin
		);
    	}	
	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {	/* XXX ibss? */
		/*
		 * Arrange for a beacon update and bump the parameter
		 * set number so associated stations load the new values.
		 */
		wme->wme_bssChanParams.cap_info =
			(wme->wme_bssChanParams.cap_info+1) & WME_QOSINFO_COUNT;
		ic->ic_flags |= IEEE80211_F_WMEUPDATE;
	}

	wme->wme_update(ic);

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_WME,
		"%s: WME params updated, cap_info 0x%x\n", __func__,
		ic->ic_opmode == IEEE80211_M_STA ?
			wme->wme_wmeChanParams.cap_info :
			wme->wme_bssChanParams.cap_info);
}

void
ieee80211_wme_updateparams(struct ieee80211com *ic)
{

	if (ic->ic_caps & IEEE80211_C_WME) {
		IEEE80211_BEACON_LOCK(ic);
		ieee80211_wme_updateparams_locked(ic);
		IEEE80211_BEACON_UNLOCK(ic);
	}
}

/*
 * Start a device.  If this is the first vap running on the
 * underlying device then we first bring it up.
 */
int
ieee80211_init(struct ieee80211com *ic, int forcescan)
{

	IEEE80211_DPRINTF(ic,
		IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
		"%s\n", "start running");

	/*
	 * Kick the 802.11 state machine as appropriate.
	 */
	if (ic->ic_roaming != IEEE80211_ROAMING_MANUAL) {
		if (ic->ic_opmode == IEEE80211_M_STA) {
			/*
			 * Try to be intelligent about clocking the state
			 * machine.  If we're currently in RUN state then
			 * we should be able to apply any new state/parameters
			 * simply by re-associating.  Otherwise we need to
			 * re-scan to select an appropriate ap.
			 */
			if (ic->ic_state != IEEE80211_S_RUN || forcescan)
				ieee80211_new_state(ic, IEEE80211_S_SCAN, 0);
			else
				ieee80211_new_state(ic, IEEE80211_S_ASSOC, 1);
		} else {
			/*
			 * For monitor+wds modes there's nothing to do but
			 * start running.  Otherwise, if this is the first
			 * vap to be brought up, start a scan which may be
			 * preempted if the station is locked to a particular
			 * channel.
			 */
			if (ic->ic_opmode == IEEE80211_M_MONITOR ||
			    ic->ic_opmode == IEEE80211_M_WDS) {
				ic->ic_state = IEEE80211_S_INIT;	/* XXX*/
				ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
			} else
				ieee80211_new_state(ic, IEEE80211_S_SCAN, 0);
		}
	}
	return 0;
}

/*
 * Switch between turbo and non-turbo operating modes.
 * Use the specified channel flags to locate the new
 * channel, update 802.11 state, and then call back into
 * the driver to effect the change.
 */
void
ieee80211_dturbo_switch(struct ieee80211com *ic, int newflags)
{
	struct ieee80211_channel *chan;

	chan = ieee80211_find_channel(ic, ic->ic_bsschan->ic_freq, newflags);
	if (chan == NULL) {		/* XXX should not happen */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SUPERG,
		    "%s: no channel with freq %u flags 0x%x\n",
		    __func__, ic->ic_bsschan->ic_freq, newflags);
		return;
	}

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SUPERG,
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

	if (ic->ic_flags & IEEE80211_F_SCAN) {
		/* XXX check ic_curchan != ic_bsschan? */
		return;
	}
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_STATE | IEEE80211_MSG_DEBUG,
		"%s\n", "beacon miss");

	/*
	 * Our handling is only meaningful for stations that are
	 * associated; any other conditions else will be handled
	 * through different means (e.g. the tx timeout on mgt frames).
	 */
	if (ic->ic_opmode != IEEE80211_M_STA || ic->ic_state != IEEE80211_S_RUN)
		return;

	if (++ic->ic_bmiss_count < ic->ic_bmiss_max) {
		/*
		 * Send a directed probe req before falling back to a scan;
		 * if we receive a response ic_bmiss_count will be reset.
		 * Some cards mistakenly report beacon miss so this avoids
		 * the expensive scan if the ap is still there.
		 */
		ieee80211_send_probereq(ic->ic_bss, ic->ic_myaddr,
			ic->ic_bss->ni_bssid, ic->ic_bss->ni_bssid,
			ic->ic_bss->ni_essid, ic->ic_bss->ni_esslen,
			ic->ic_opt_ie, ic->ic_opt_ie_len);
		return;
	}
	ic->ic_bmiss_count = 0;
	if (ic->ic_roaming == IEEE80211_ROAMING_AUTO) {
		/*
		 * If we receive a beacon miss interrupt when using
		 * dynamic turbo, attempt to switch modes before
		 * reassociating.
		 */
		if (IEEE80211_ATH_CAP(ic, ic->ic_bss, IEEE80211_NODE_TURBOP))
			ieee80211_dturbo_switch(ic,
			    ic->ic_bsschan->ic_flags ^ IEEE80211_CHAN_TURBO);
		/*
		 * Try to reassociate before scanning for a new ap.
		 */
		ieee80211_new_state(ic, IEEE80211_S_ASSOC, 1);
	} else {
		/*
		 * Somebody else is controlling state changes (e.g.
		 * a user-mode app) don't do anything that would
		 * confuse them; just drop into scan mode so they'll
		 * notified of the state change and given control.
		 */
		ieee80211_new_state(ic, IEEE80211_S_SCAN, 0);
	}
}

/*
 * Software beacon miss handling.  Check if any beacons
 * were received in the last period.  If not post a
 * beacon miss; otherwise reset the counter.
 */
static void
ieee80211_swbmiss(void *arg)
{
	struct ieee80211com *ic = arg;

	if (ic->ic_swbmiss_count == 0) {
		ieee80211_beacon_miss(ic);
		if (ic->ic_bmiss_count == 0)	/* don't re-arm timer */
			return;
	} else
		ic->ic_swbmiss_count = 0;
	callout_reset(&ic->ic_swbmiss, ic->ic_swbmiss_period,
		ieee80211_swbmiss, ic);
}

static void
sta_disassoc(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = arg;

	if (ni->ni_associd != 0) {
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DISASSOC,
			IEEE80211_REASON_ASSOC_LEAVE);
		ieee80211_node_leave(ic, ni);
	}
}

static void
sta_deauth(void *arg, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = arg;

	IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_DEAUTH,
		IEEE80211_REASON_ASSOC_LEAVE);
}

/*
 * Handle deauth with reason.  We retry only for
 * the cases where we might succeed.  Otherwise
 * we downgrade the ap and scan.
 */
static void
sta_authretry(struct ieee80211com *ic, struct ieee80211_node *ni, int reason)
{
	switch (reason) {
	case IEEE80211_STATUS_SUCCESS:
	case IEEE80211_STATUS_TIMEOUT:
	case IEEE80211_REASON_ASSOC_EXPIRE:
	case IEEE80211_REASON_NOT_AUTHED:
	case IEEE80211_REASON_NOT_ASSOCED:
	case IEEE80211_REASON_ASSOC_LEAVE:
	case IEEE80211_REASON_ASSOC_NOT_AUTHED:
		IEEE80211_SEND_MGMT(ic, ni, IEEE80211_FC0_SUBTYPE_AUTH, 1);
		break;
	default:
		ieee80211_scan_assoc_fail(ic, ic->ic_bss->ni_macaddr, reason);
		if (ic->ic_roaming == IEEE80211_ROAMING_AUTO)
			ieee80211_check_scan(ic,
				IEEE80211_SCAN_ACTIVE,
				IEEE80211_SCAN_FOREVER,
				ic->ic_des_nssid, ic->ic_des_ssid);
		break;
	}
}

static int
ieee80211_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate;

	ostate = ic->ic_state;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_STATE, "%s: %s -> %s\n", __func__,
		ieee80211_state_name[ostate], ieee80211_state_name[nstate]);
	ic->ic_state = nstate;			/* state transition */
	callout_stop(&ic->ic_mgtsend);		/* XXX callout_drain */
	if (ostate != IEEE80211_S_SCAN)
		ieee80211_cancel_scan(ic);	/* background scan */
	ni = ic->ic_bss;			/* NB: no reference held */
	if (ic->ic_flags_ext & IEEE80211_FEXT_SWBMISS)
		callout_stop(&ic->ic_swbmiss);
	switch (nstate) {
	case IEEE80211_S_INIT:
		switch (ostate) {
		case IEEE80211_S_INIT:
			break;
		case IEEE80211_S_RUN:
			switch (ic->ic_opmode) {
			case IEEE80211_M_STA:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_ASSOC_LEAVE);
				ieee80211_sta_leave(ic, ni);
				break;
			case IEEE80211_M_HOSTAP:
				ieee80211_iterate_nodes(&ic->ic_sta,
					sta_disassoc, ic);
				break;
			default:
				break;
			}
			break;
		case IEEE80211_S_ASSOC:
			switch (ic->ic_opmode) {
			case IEEE80211_M_STA:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_LEAVE);
				break;
			case IEEE80211_M_HOSTAP:
				ieee80211_iterate_nodes(&ic->ic_sta,
					sta_deauth, ic);
				break;
			default:
				break;
			}
			break;
		case IEEE80211_S_SCAN:
			ieee80211_cancel_scan(ic);
			break;
		case IEEE80211_S_AUTH:
			break;
		default:
			break;
		}
		if (ostate != IEEE80211_S_INIT) {
			/* NB: optimize INIT -> INIT case */
			ieee80211_drain_ifq(&ic->ic_mgtq);
			ieee80211_reset_bss(ic);
			ieee80211_scan_flush(ic);
		}
		if (ic->ic_auth->ia_detach != NULL)
			ic->ic_auth->ia_detach(ic);
		break;
	case IEEE80211_S_SCAN:
		switch (ostate) {
		case IEEE80211_S_INIT:
		createibss:
			if ((ic->ic_opmode == IEEE80211_M_HOSTAP ||
			     ic->ic_opmode == IEEE80211_M_IBSS ||
			     ic->ic_opmode == IEEE80211_M_AHDEMO) &&
			    ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
				/*
				 * Already have a channel; bypass the
				 * scan and startup immediately.  Because
				 * of this explicitly sync the scanner state.
				 */
				ieee80211_scan_update(ic);
				ieee80211_create_ibss(ic, ic->ic_des_chan);
			} else {
				ieee80211_check_scan(ic,
					IEEE80211_SCAN_ACTIVE |
					IEEE80211_SCAN_FLUSH,
					IEEE80211_SCAN_FOREVER,
					ic->ic_des_nssid, ic->ic_des_ssid);
			}
			break;
		case IEEE80211_S_SCAN:
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			/*
			 * These can happen either because of a timeout
			 * on an assoc/auth response or because of a
			 * change in state that requires a reset.  For
			 * the former we're called with a non-zero arg
			 * that is the cause for the failure; pass this
			 * to the scan code so it can update state.
			 * Otherwise trigger a new scan unless we're in
			 * manual roaming mode in which case an application
			 * must issue an explicit scan request.
			 */
			if (arg != 0)
				ieee80211_scan_assoc_fail(ic,
					ic->ic_bss->ni_macaddr, arg);
			if (ic->ic_roaming == IEEE80211_ROAMING_AUTO)
				ieee80211_check_scan(ic,
					IEEE80211_SCAN_ACTIVE,
					IEEE80211_SCAN_FOREVER,
					ic->ic_des_nssid, ic->ic_des_ssid);
			break;
		case IEEE80211_S_RUN:		/* beacon miss */
			if (ic->ic_opmode == IEEE80211_M_STA) {
				ieee80211_sta_leave(ic, ni);
				ic->ic_flags &= ~IEEE80211_F_SIBSS;	/* XXX */
				if (ic->ic_roaming == IEEE80211_ROAMING_AUTO)
					ieee80211_check_scan(ic,
						IEEE80211_SCAN_ACTIVE,
						IEEE80211_SCAN_FOREVER,
						ic->ic_des_nssid,
						ic->ic_des_ssid);
			} else {
				ieee80211_iterate_nodes(&ic->ic_sta,
					sta_disassoc, ic);
				goto createibss;
			}
			break;
		default:
			break;
		}
		break;
	case IEEE80211_S_AUTH:
		KASSERT(ic->ic_opmode == IEEE80211_M_STA,
			("switch to %s state when operating in mode %u",
			 ieee80211_state_name[nstate], ic->ic_opmode));
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_AUTH, 1);
			break;
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			switch (arg & 0xff) {
			case IEEE80211_FC0_SUBTYPE_AUTH:
				/* ??? */
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH, 2);
				break;
			case IEEE80211_FC0_SUBTYPE_DEAUTH:
				sta_authretry(ic, ni, arg>>8);
				break;
			}
			break;
		case IEEE80211_S_RUN:
			switch (arg & 0xff) {
			case IEEE80211_FC0_SUBTYPE_AUTH:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH, 2);
				ic->ic_state = ostate;	/* stay RUN */
				break;
			case IEEE80211_FC0_SUBTYPE_DEAUTH:
				ieee80211_sta_leave(ic, ni);
				if (ic->ic_roaming == IEEE80211_ROAMING_AUTO) {
					/* try to reauth */
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_AUTH, 1);
				}
				break;
			}
			break;
		default:
			break;
		}
		break;
	case IEEE80211_S_ASSOC:
		KASSERT(ic->ic_opmode == IEEE80211_M_STA,
			("switch to %s state when operating in mode %u",
			 ieee80211_state_name[nstate], ic->ic_opmode));
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
				"%s: invalid transition\n", __func__);
			break;
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 0);
			break;
		case IEEE80211_S_RUN:
			ieee80211_sta_leave(ic, ni);
			if (ic->ic_roaming == IEEE80211_ROAMING_AUTO) {
				IEEE80211_SEND_MGMT(ic, ni, arg ?
				    IEEE80211_FC0_SUBTYPE_REASSOC_REQ :
				    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 0);
			}
			break;
		default:
			break;
		}
		break;
	case IEEE80211_S_RUN:
		if (ic->ic_flags & IEEE80211_F_WPA) {
			/* XXX validate prerequisites */
		}
		switch (ostate) {
		case IEEE80211_S_INIT:
			if (ic->ic_opmode == IEEE80211_M_MONITOR ||
			    ic->ic_opmode == IEEE80211_M_WDS ||
			    ic->ic_opmode == IEEE80211_M_HOSTAP) {
				/*
				 * Already have a channel; bypass the
				 * scan and startup immediately.  Because
				 * of this explicitly sync the scanner state.
				 */
				ieee80211_scan_update(ic);
				ieee80211_create_ibss(ic, ic->ic_curchan);
				break;
			}
			/* fall thru... */
		case IEEE80211_S_AUTH:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
				"%s: invalid transition\n", __func__);
			/* fall thru... */
		case IEEE80211_S_RUN:
			break;
		case IEEE80211_S_SCAN:		/* adhoc/hostap mode */
		case IEEE80211_S_ASSOC:		/* infra mode */
			KASSERT(ni->ni_txrate < ni->ni_rates.rs_nrates,
				("%s: bogus xmit rate %u setup\n", __func__,
					ni->ni_txrate));
#ifdef IEEE80211_DEBUG
			if (ieee80211_msg_debug(ic)) {
				if (ic->ic_opmode == IEEE80211_M_STA)
					if_printf(ifp, "associated ");
				else
					if_printf(ifp, "synchronized ");
				printf("with %s ssid ",
				    ether_sprintf(ni->ni_bssid));
				ieee80211_print_essid(ic->ic_bss->ni_essid,
				    ni->ni_esslen);
				printf(" channel %d start %uMb\n",
					ieee80211_chan2ieee(ic, ic->ic_curchan),
					IEEE80211_RATE2MBS(ni->ni_rates.rs_rates[ni->ni_txrate]));
			}
#endif
			if (ic->ic_opmode == IEEE80211_M_STA) {
				ieee80211_scan_assoc_success(ic,
					ni->ni_macaddr);
				ieee80211_notify_node_join(ic, ni, 
					arg == IEEE80211_FC0_SUBTYPE_ASSOC_RESP);
			}
			if_start(ifp);		/* XXX not authorized yet */
			break;
		default:
			break;
		}
		if (ostate != IEEE80211_S_RUN &&
		    ic->ic_opmode == IEEE80211_M_STA &&
		    (ic->ic_flags_ext & IEEE80211_FEXT_SWBMISS)) {
			/*
			 * Start s/w beacon miss timer for devices w/o
			 * hardware support.  We fudge a bit here since
			 * we're doing this in software.
			 */
			ic->ic_swbmiss_period = IEEE80211_TU_TO_TICKS(
				2 * ic->ic_bmissthreshold * ni->ni_intval);
			ic->ic_swbmiss_count = 0;
			callout_reset(&ic->ic_swbmiss, ic->ic_swbmiss_period,
				ieee80211_swbmiss, ic);
		}
		/*
		 * Start/stop the authenticator when operating as an
		 * AP.  We delay until here to allow configuration to
		 * happen out of order.
		 */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP && /* XXX IBSS/AHDEMO */
		    ic->ic_auth->ia_attach != NULL) {
			/* XXX check failure */
			ic->ic_auth->ia_attach(ic);
		} else if (ic->ic_auth->ia_detach != NULL) {
			ic->ic_auth->ia_detach(ic);
		}
		/*
		 * When 802.1x is not in use mark the port authorized
		 * at this point so traffic can flow.
		 */
		if (ni->ni_authmode != IEEE80211_AUTH_8021X)
			ieee80211_node_authorize(ni);
		/*
		 * Enable inactivity processing.
		 * XXX
		 */
		callout_reset(&ic->ic_inact, IEEE80211_INACT_WAIT*hz,
			ieee80211_node_timeout, ic);
		break;
	default:
		break;
	}
	return 0;
}
