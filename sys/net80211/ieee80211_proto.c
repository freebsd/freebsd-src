/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
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
const char *ieee80211_state_name[IEEE80211_S_MAX] = {
	"INIT",		/* IEEE80211_S_INIT */
	"SCAN",		/* IEEE80211_S_SCAN */
	"AUTH",		/* IEEE80211_S_AUTH */
	"ASSOC",	/* IEEE80211_S_ASSOC */
	"RUN"		/* IEEE80211_S_RUN */
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

#ifdef notdef
	ic->ic_rtsthreshold = IEEE80211_RTS_DEFAULT;
#else
	ic->ic_rtsthreshold = IEEE80211_RTS_MAX;
#endif
	ic->ic_fragthreshold = 2346;		/* XXX not used yet */
	ic->ic_fixed_rate = -1;			/* no fixed rate */
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

	IF_DRAIN(&ic->ic_mgtq);
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
ieee80211_print_essid(const u_int8_t *essid, int len)
{
	const u_int8_t *p; 
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
ieee80211_dump_pkt(const u_int8_t *buf, int len, int rate, int rssi)
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
		printf("DSDS %s", ether_sprintf((const u_int8_t *)&wh[1]));
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
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		int i;
		printf(" WEP [IV");
		for (i = 0; i < IEEE80211_WEP_IVLEN; i++)
			printf(" %.02x", buf[sizeof(*wh)+i]);
		printf(" KID %u]", buf[sizeof(*wh)+i] >> 6);
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

int
ieee80211_fix_rate(struct ieee80211com *ic, struct ieee80211_node *ni, int flags)
{
#define	RV(v)	((v) & IEEE80211_RATE_VAL)
	int i, j, ignore, error;
	int okrate, badrate, fixedrate;
	struct ieee80211_rateset *srs, *nrs;
	u_int8_t r;

	/*
	 * If the fixed rate check was requested but no
	 * fixed has been defined then just remove it.
	 */
	if ((flags & IEEE80211_F_DOFRATE) && ic->ic_fixed_rate < 0)
		flags &= ~IEEE80211_F_DOFRATE;
	error = 0;
	okrate = badrate = fixedrate = 0;
	srs = &ic->ic_sup_rates[ieee80211_chan2mode(ic, ni->ni_chan)];
	nrs = &ni->ni_rates;
	for (i = 0; i < nrs->rs_nrates; ) {
		ignore = 0;
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
		if (flags & IEEE80211_F_DOFRATE) {
			/*
			 * Check any fixed rate is included. 
			 */
			if (r == RV(srs->rs_rates[ic->ic_fixed_rate]))
				fixedrate = r;
		}
		if (flags & IEEE80211_F_DONEGO) {
			/*
			 * Check against supported rates.
			 */
			for (j = 0; j < srs->rs_nrates; j++) {
				if (r == RV(srs->rs_rates[j])) {
					/*
					 * Overwrite with the supported rate
					 * value so any basic rate bit is set.
					 * This insures that response we send
					 * to stations have the necessary basic
					 * rate bit set.
					 */
					nrs->rs_rates[i] = srs->rs_rates[j];
					break;
				}
			}
			if (j == srs->rs_nrates) {
				/*
				 * A rate in the node's rate set is not
				 * supported.  If this is a basic rate and we
				 * are operating as an AP then this is an error.
				 * Otherwise we just discard/ignore the rate.
				 * Note that this is important for 11b stations
				 * when they want to associate with an 11g AP.
				 */
				if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
				    (nrs->rs_rates[i] & IEEE80211_RATE_BASIC))
					error++;
				ignore++;
			}
		}
		if (flags & IEEE80211_F_DODEL) {
			/*
			 * Delete unacceptable rates.
			 */
			if (ignore) {
				nrs->rs_nrates--;
				for (j = i; j < nrs->rs_nrates; j++)
					nrs->rs_rates[j] = nrs->rs_rates[j + 1];
				nrs->rs_rates[j] = 0;
				continue;
			}
		}
		if (!ignore)
			okrate = nrs->rs_rates[i];
		i++;
	}
	if (okrate == 0 || error != 0 ||
	    ((flags & IEEE80211_F_DOFRATE) && fixedrate == 0))
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
		ic->ic_curmode == IEEE80211_MODE_11A ||
		(ic->ic_curmode == IEEE80211_MODE_11G &&
		ic->ic_opmode == IEEE80211_M_HOSTAP &&
		(ic->ic_caps & IEEE80211_C_SHSLOT)));
	/*
	 * Set short preamble and ERP barker-preamble flags.
	 */
	if (ic->ic_curmode == IEEE80211_MODE_11A ||
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
	static const struct ieee80211_rateset basic[] = {
	    { 0 },			/* IEEE80211_MODE_AUTO */
	    { 3, { 12, 24, 48 } },	/* IEEE80211_MODE_11A */
	    { 2, { 2, 4 } },		/* IEEE80211_MODE_11B */
	    { 4, { 2, 4, 11, 22 } },	/* IEEE80211_MODE_11G (mixed b/g) */
	    { 0 },			/* IEEE80211_MODE_FH */
					/* IEEE80211_MODE_PUREG (not yet) */
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
	u_int8_t aifsn; 
	u_int8_t logcwmin;
	u_int8_t logcwmax; 
	u_int16_t txopLimit;
	u_int8_t acm;
} paramType;

static const struct phyParamType phyParamForAC_BE[IEEE80211_MODE_MAX] = {
	{ 3, 4, 6 },		/* IEEE80211_MODE_AUTO */
	{ 3, 4, 6 },		/* IEEE80211_MODE_11A */ 
	{ 3, 5, 7 },		/* IEEE80211_MODE_11B */ 
	{ 3, 4, 6 },		/* IEEE80211_MODE_11G */ 
	{ 3, 5, 7 },		/* IEEE80211_MODE_FH */ 
	{ 2, 3, 5 },		/* IEEE80211_MODE_TURBO_A */ 
	{ 2, 3, 5 },		/* IEEE80211_MODE_TURBO_G */ 
};
static const struct phyParamType phyParamForAC_BK[IEEE80211_MODE_MAX] = {
	{ 7, 4, 10 },		/* IEEE80211_MODE_AUTO */
	{ 7, 4, 10 },		/* IEEE80211_MODE_11A */ 
	{ 7, 5, 10 },		/* IEEE80211_MODE_11B */ 
	{ 7, 4, 10 },		/* IEEE80211_MODE_11G */ 
	{ 7, 5, 10 },		/* IEEE80211_MODE_FH */ 
	{ 7, 3, 10 },		/* IEEE80211_MODE_TURBO_A */ 
	{ 7, 3, 10 },		/* IEEE80211_MODE_TURBO_G */ 
};
static const struct phyParamType phyParamForAC_VI[IEEE80211_MODE_MAX] = {
	{ 1, 3, 4,  94 },	/* IEEE80211_MODE_AUTO */
	{ 1, 3, 4,  94 },	/* IEEE80211_MODE_11A */ 
	{ 1, 4, 5, 188 },	/* IEEE80211_MODE_11B */ 
	{ 1, 3, 4,  94 },	/* IEEE80211_MODE_11G */ 
	{ 1, 4, 5, 188 },	/* IEEE80211_MODE_FH */ 
	{ 1, 2, 3,  94 },	/* IEEE80211_MODE_TURBO_A */ 
	{ 1, 2, 3,  94 },	/* IEEE80211_MODE_TURBO_G */ 
};
static const struct phyParamType phyParamForAC_VO[IEEE80211_MODE_MAX] = {
	{ 1, 2, 3,  47 },	/* IEEE80211_MODE_AUTO */
	{ 1, 2, 3,  47 },	/* IEEE80211_MODE_11A */ 
	{ 1, 3, 4, 102 },	/* IEEE80211_MODE_11B */ 
	{ 1, 2, 3,  47 },	/* IEEE80211_MODE_11G */ 
	{ 1, 3, 4, 102 },	/* IEEE80211_MODE_FH */ 
	{ 1, 2, 2,  47 },	/* IEEE80211_MODE_TURBO_A */ 
	{ 1, 2, 2,  47 },	/* IEEE80211_MODE_TURBO_G */ 
};

static const struct phyParamType bssPhyParamForAC_BE[IEEE80211_MODE_MAX] = {
	{ 3, 4, 10 },		/* IEEE80211_MODE_AUTO */
	{ 3, 4, 10 },		/* IEEE80211_MODE_11A */ 
	{ 3, 5, 10 },		/* IEEE80211_MODE_11B */ 
	{ 3, 4, 10 },		/* IEEE80211_MODE_11G */ 
	{ 3, 5, 10 },		/* IEEE80211_MODE_FH */ 
	{ 2, 3, 10 },		/* IEEE80211_MODE_TURBO_A */ 
	{ 2, 3, 10 },		/* IEEE80211_MODE_TURBO_G */ 
};
static const struct phyParamType bssPhyParamForAC_VI[IEEE80211_MODE_MAX] = {
	{ 2, 3, 4,  94 },	/* IEEE80211_MODE_AUTO */
	{ 2, 3, 4,  94 },	/* IEEE80211_MODE_11A */ 
	{ 2, 4, 5, 188 },	/* IEEE80211_MODE_11B */ 
	{ 2, 3, 4,  94 },	/* IEEE80211_MODE_11G */ 
	{ 2, 4, 5, 188 },	/* IEEE80211_MODE_FH */ 
	{ 2, 2, 3,  94 },	/* IEEE80211_MODE_TURBO_A */ 
	{ 2, 2, 3,  94 },	/* IEEE80211_MODE_TURBO_G */ 
};
static const struct phyParamType bssPhyParamForAC_VO[IEEE80211_MODE_MAX] = {
	{ 2, 2, 3,  47 },	/* IEEE80211_MODE_AUTO */
	{ 2, 2, 3,  47 },	/* IEEE80211_MODE_11A */ 
	{ 2, 3, 4, 102 },	/* IEEE80211_MODE_11B */ 
	{ 2, 2, 3,  47 },	/* IEEE80211_MODE_11G */ 
	{ 2, 3, 4, 102 },	/* IEEE80211_MODE_FH */ 
	{ 1, 2, 2,  47 },	/* IEEE80211_MODE_TURBO_A */ 
	{ 1, 2, 2,  47 },	/* IEEE80211_MODE_TURBO_G */ 
};

void
ieee80211_wme_initparams(struct ieee80211com *ic)
{
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	const paramType *pPhyParam, *pBssPhyParam;
	struct wmeParams *wmep;
	int i;

	if ((ic->ic_caps & IEEE80211_C_WME) == 0)
		return;

	for (i = 0; i < WME_NUM_AC; i++) {
		switch (i) {
		case WME_AC_BK:
			pPhyParam = &phyParamForAC_BK[ic->ic_curmode];
			pBssPhyParam = &phyParamForAC_BK[ic->ic_curmode];
			break;
		case WME_AC_VI:
			pPhyParam = &phyParamForAC_VI[ic->ic_curmode];
			pBssPhyParam = &bssPhyParamForAC_VI[ic->ic_curmode];
			break;
		case WME_AC_VO:
			pPhyParam = &phyParamForAC_VO[ic->ic_curmode];
			pBssPhyParam = &bssPhyParamForAC_VO[ic->ic_curmode];
			break;
		case WME_AC_BE:
		default:
			pPhyParam = &phyParamForAC_BE[ic->ic_curmode];
			pBssPhyParam = &bssPhyParamForAC_BE[ic->ic_curmode];
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
		{ 2, 4, 10, 64 },	/* IEEE80211_MODE_AUTO */ 
		{ 2, 4, 10, 64 },	/* IEEE80211_MODE_11A */ 
		{ 2, 5, 10, 64 },	/* IEEE80211_MODE_11B */ 
		{ 2, 4, 10, 64 },	/* IEEE80211_MODE_11G */ 
		{ 2, 5, 10, 64 },	/* IEEE80211_MODE_FH */ 
		{ 1, 3, 10, 64 },	/* IEEE80211_MODE_TURBO_A */ 
		{ 1, 3, 10, 64 },	/* IEEE80211_MODE_TURBO_G */ 
	};
	struct ieee80211_wme_state *wme = &ic->ic_wme;
	const struct wmeParams *wmep;
	struct wmeParams *chanp, *bssp;
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
	 * This implements agressive mode as found in certain
	 * vendors' AP's.  When there is significant high
	 * priority (VI/VO) traffic in the BSS throttle back BE
	 * traffic by using conservative parameters.  Otherwise
	 * BE uses agressive params to optimize performance of
	 * legacy/non-QoS traffic.
	 */
        if ((ic->ic_opmode == IEEE80211_M_HOSTAP &&
	     (wme->wme_flags & WME_F_AGGRMODE) == 0) ||
	    (ic->ic_opmode != IEEE80211_M_HOSTAP &&
	     (ic->ic_bss->ni_flags & IEEE80211_NODE_QOS) == 0) ||
	    (ic->ic_flags & IEEE80211_F_WME) == 0) {
		chanp = &wme->wme_chanParams.cap_wmeParams[WME_AC_BE];
		bssp = &wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE];

		chanp->wmep_aifsn = bssp->wmep_aifsn =
			phyParam[ic->ic_curmode].aifsn;
		chanp->wmep_logcwmin = bssp->wmep_logcwmin =
			phyParam[ic->ic_curmode].logcwmin;
		chanp->wmep_logcwmax = bssp->wmep_logcwmax =
			phyParam[ic->ic_curmode].logcwmax;
		chanp->wmep_txopLimit = bssp->wmep_txopLimit =
			(ic->ic_caps & IEEE80211_C_BURST) ?
				phyParam[ic->ic_curmode].txopLimit : 0;		
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
	    ic->ic_sta_assoc < 2 && (wme->wme_flags & WME_F_AGGRMODE) == 0) {
        	static const u_int8_t logCwMin[IEEE80211_MODE_MAX] = {
              		3,	/* IEEE80211_MODE_AUTO */
              		3,	/* IEEE80211_MODE_11A */
              		4,	/* IEEE80211_MODE_11B */
              		3,	/* IEEE80211_MODE_11G */
              		4,	/* IEEE80211_MODE_FH */
              		3,	/* IEEE80211_MODE_TURBO_A */
              		3,	/* IEEE80211_MODE_TURBO_G */
		};
		chanp = &wme->wme_chanParams.cap_wmeParams[WME_AC_BE];
		bssp = &wme->wme_bssChanParams.cap_wmeParams[WME_AC_BE];

		chanp->wmep_logcwmin = bssp->wmep_logcwmin = 
			logCwMin[ic->ic_curmode];
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

static int
ieee80211_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;
	enum ieee80211_state ostate;

	ostate = ic->ic_state;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_STATE, "%s: %s -> %s\n", __func__,
		ieee80211_state_name[ostate], ieee80211_state_name[nstate]);
	ic->ic_state = nstate;			/* state transition */
	ni = ic->ic_bss;			/* NB: no reference held */
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
				nt = &ic->ic_sta;
				IEEE80211_NODE_LOCK(nt);
				TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
					if (ni->ni_associd == 0)
						continue;
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DISASSOC,
					    IEEE80211_REASON_ASSOC_LEAVE);
				}
				IEEE80211_NODE_UNLOCK(nt);
				break;
			default:
				break;
			}
			goto reset;
		case IEEE80211_S_ASSOC:
			switch (ic->ic_opmode) {
			case IEEE80211_M_STA:
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_LEAVE);
				break;
			case IEEE80211_M_HOSTAP:
				nt = &ic->ic_sta;
				IEEE80211_NODE_LOCK(nt);
				TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
					IEEE80211_SEND_MGMT(ic, ni,
					    IEEE80211_FC0_SUBTYPE_DEAUTH,
					    IEEE80211_REASON_AUTH_LEAVE);
				}
				IEEE80211_NODE_UNLOCK(nt);
				break;
			default:
				break;
			}
			goto reset;
		case IEEE80211_S_SCAN:
			ieee80211_cancel_scan(ic);
			goto reset;
		case IEEE80211_S_AUTH:
		reset:
			ic->ic_mgt_timer = 0;
			IF_DRAIN(&ic->ic_mgtq);
			ieee80211_reset_bss(ic);
			break;
		}
		if (ic->ic_auth->ia_detach != NULL)
			ic->ic_auth->ia_detach(ic);
		break;
	case IEEE80211_S_SCAN:
		switch (ostate) {
		case IEEE80211_S_INIT:
			if ((ic->ic_opmode == IEEE80211_M_HOSTAP ||
			     ic->ic_opmode == IEEE80211_M_IBSS ||
			     ic->ic_opmode == IEEE80211_M_AHDEMO) &&
			    ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
				/*
				 * AP operation and we already have a channel;
				 * bypass the scan and startup immediately.
				 */
				ieee80211_create_ibss(ic, ic->ic_des_chan);
			} else {
				ieee80211_begin_scan(ic, arg);
			}
			break;
		case IEEE80211_S_SCAN:
			/*
			 * Scan next. If doing an active scan and the
			 * channel is not marked passive-only then send
			 * a probe request.  Otherwise just listen for
			 * beacons on the channel.
			 */
			if ((ic->ic_flags & IEEE80211_F_ASCAN) &&
			    (ni->ni_chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0) {
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_PROBE_REQ, 0);
			}
			break;
		case IEEE80211_S_RUN:
			/* beacon miss */
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_STATE,
				"no recent beacons from %s; rescanning\n",
				ether_sprintf(ic->ic_bss->ni_bssid));
			ieee80211_sta_leave(ic, ni);
			ic->ic_flags &= ~IEEE80211_F_SIBSS;	/* XXX */
			/* FALLTHRU */
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			/* timeout restart scan */
			ni = ieee80211_find_node(&ic->ic_scan,
				ic->ic_bss->ni_macaddr);
			if (ni != NULL) {
				ni->ni_fails++;
				ieee80211_unref_node(&ni);
			}
			if (ic->ic_roaming == IEEE80211_ROAMING_AUTO)
				ieee80211_begin_scan(ic, arg);
			break;
		}
		break;
	case IEEE80211_S_AUTH:
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_AUTH, 1);
			break;
		case IEEE80211_S_AUTH:
		case IEEE80211_S_ASSOC:
			switch (arg) {
			case IEEE80211_FC0_SUBTYPE_AUTH:
				/* ??? */
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_AUTH, 2);
				break;
			case IEEE80211_FC0_SUBTYPE_DEAUTH:
				/* ignore and retry scan on timeout */
				break;
			}
			break;
		case IEEE80211_S_RUN:
			switch (arg) {
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
		}
		break;
	case IEEE80211_S_ASSOC:
		switch (ostate) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
		case IEEE80211_S_ASSOC:
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
				"%s: invalid transition\n", __func__);
			break;
		case IEEE80211_S_AUTH:
			IEEE80211_SEND_MGMT(ic, ni,
			    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 0);
			break;
		case IEEE80211_S_RUN:
			ieee80211_sta_leave(ic, ni);
			if (ic->ic_roaming == IEEE80211_ROAMING_AUTO) {
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_ASSOC_REQ, 1);
			}
			break;
		}
		break;
	case IEEE80211_S_RUN:
		if (ic->ic_flags & IEEE80211_F_WPA) {
			/* XXX validate prerequisites */
		}
		switch (ostate) {
		case IEEE80211_S_INIT:
			if (ic->ic_opmode == IEEE80211_M_MONITOR)
				break;
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
					ieee80211_chan2ieee(ic, ni->ni_chan),
					IEEE80211_RATE2MBS(ni->ni_rates.rs_rates[ni->ni_txrate]));
			}
#endif
			ic->ic_mgt_timer = 0;
			if (ic->ic_opmode == IEEE80211_M_STA)
				ieee80211_notify_node_join(ic, ni, 
					arg == IEEE80211_FC0_SUBTYPE_ASSOC_RESP);
			if_start(ifp);		/* XXX not authorized yet */
			break;
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
			ieee80211_node_authorize(ic, ni);
		/*
		 * Enable inactivity processing.
		 * XXX
		 */
		ic->ic_scan.nt_inact_timer = IEEE80211_INACT_WAIT;
		ic->ic_sta.nt_inact_timer = IEEE80211_INACT_WAIT;
		break;
	}
	return 0;
}
