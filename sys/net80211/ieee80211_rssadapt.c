/*	$FreeBSD$	*/
/* $NetBSD: ieee80211_rssadapt.c,v 1.9 2005/02/26 22:45:09 perry Exp $ */
/*-
 * Copyright (c) 2003, 2004 David Young.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_media.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_rssadapt.h>

struct rssadapt_expavgctl {
	/* RSS threshold decay. */
	u_int rc_decay_denom;
	u_int rc_decay_old;
	/* RSS threshold update. */
	u_int rc_thresh_denom;
	u_int rc_thresh_old;
	/* RSS average update. */
	u_int rc_avgrssi_denom;
	u_int rc_avgrssi_old;
};

static struct rssadapt_expavgctl master_expavgctl = {
	rc_decay_denom : 16,
	rc_decay_old : 15,
	rc_thresh_denom : 8,
	rc_thresh_old : 4,
	rc_avgrssi_denom : 8,
	rc_avgrssi_old : 4
};

#ifdef interpolate
#undef interpolate
#endif
#define interpolate(parm, old, new) ((parm##_old * (old) + \
                                     (parm##_denom - parm##_old) * (new)) / \
				    parm##_denom)

static void rssadapt_sysctlattach(struct ieee80211_rssadapt *rs,
	struct sysctl_ctx_list *ctx, struct sysctl_oid *tree);

/* number of references from net80211 layer */
static	int nrefs = 0;

void
ieee80211_rssadapt_setinterval(struct ieee80211_rssadapt *rs, int msecs)
{
	int t;

	if (msecs < 100)
		msecs = 100;
	t = msecs_to_ticks(msecs);
	rs->interval = (t < 1) ? 1 : t;
}

void
ieee80211_rssadapt_init(struct ieee80211_rssadapt *rs, struct ieee80211vap *vap, int interval)
{
	rs->vap = vap;
	ieee80211_rssadapt_setinterval(rs, interval);

	rssadapt_sysctlattach(rs, vap->iv_sysctl, vap->iv_oid);
}

void
ieee80211_rssadapt_cleanup(struct ieee80211_rssadapt *rs)
{
}

static void
rssadapt_updatestats(struct ieee80211_rssadapt_node *ra)
{
	long interval;

	ra->ra_pktrate = (ra->ra_pktrate + 10*(ra->ra_nfail + ra->ra_nok))/2;
	ra->ra_nfail = ra->ra_nok = 0;

	/*
	 * A node is eligible for its rate to be raised every 1/10 to 10
	 * seconds, more eligible in proportion to recent packet rates.
	 */
	interval = MAX(10*1000, 10*1000 / MAX(1, 10 * ra->ra_pktrate));
	ra->ra_raise_interval = msecs_to_ticks(interval);
}

void
ieee80211_rssadapt_node_init(struct ieee80211_rssadapt *rsa,
    struct ieee80211_rssadapt_node *ra, struct ieee80211_node *ni)
{
	const struct ieee80211_rateset *rs = &ni->ni_rates;

	ra->ra_rs = rsa;
	ra->ra_rates = *rs;
	rssadapt_updatestats(ra);

	/* pick initial rate */
	for (ra->ra_rix = rs->rs_nrates - 1;
	     ra->ra_rix > 0 && (rs->rs_rates[ra->ra_rix] & IEEE80211_RATE_VAL) > 72;
	     ra->ra_rix--)
		;
	ni->ni_txrate = rs->rs_rates[ra->ra_rix] & IEEE80211_RATE_VAL;
	ra->ra_ticks = ticks;

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_RATECTL, ni,
	    "RSSADAPT initial rate %d", ni->ni_txrate);
}

static __inline int
bucket(int pktlen)
{
	int i, top, thridx;

	for (i = 0, top = IEEE80211_RSSADAPT_BKT0;
	     i < IEEE80211_RSSADAPT_BKTS;
	     i++, top <<= IEEE80211_RSSADAPT_BKTPOWER) {
		thridx = i;
		if (pktlen <= top)
			break;
	}
	return thridx;
}

int
ieee80211_rssadapt_choose(struct ieee80211_node *ni,
    struct ieee80211_rssadapt_node *ra, u_int pktlen)
{
	const struct ieee80211_rateset *rs = &ra->ra_rates;
	uint16_t (*thrs)[IEEE80211_RATE_SIZE];
	int rix, rssi;

	if ((ticks - ra->ra_ticks) > ra->ra_rs->interval) {
		rssadapt_updatestats(ra);
		ra->ra_ticks = ticks;
	}

	thrs = &ra->ra_rate_thresh[bucket(pktlen)];

	/* XXX this is average rssi, should be using last value */
	rssi = ni->ni_ic->ic_node_getrssi(ni);
	for (rix = rs->rs_nrates-1; rix >= 0; rix--)
		if ((*thrs)[rix] < (rssi << 8))
			break;
	if (rix != ra->ra_rix) {
		/* update public rate */
		ni->ni_txrate = ni->ni_rates.rs_rates[rix] & IEEE80211_RATE_VAL;
		ra->ra_rix = rix;

		IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_RATECTL, ni,
		    "RSSADAPT new rate %d (pktlen %d rssi %d)",
		    ni->ni_txrate, pktlen, rssi);
	}
	return rix;
}

/*
 * Adapt the data rate to suit the conditions.  When a transmitted
 * packet is dropped after RAL_RSSADAPT_RETRY_LIMIT retransmissions,
 * raise the RSS threshold for transmitting packets of similar length at
 * the same data rate.
 */
void
ieee80211_rssadapt_lower_rate(struct ieee80211_rssadapt_node *ra,
    int pktlen, int rssi)
{
	uint16_t last_thr;
	uint16_t (*thrs)[IEEE80211_RATE_SIZE];
	u_int rix;

	thrs = &ra->ra_rate_thresh[bucket(pktlen)];

	rix = ra->ra_rix;
	last_thr = (*thrs)[rix];
	(*thrs)[rix] = interpolate(master_expavgctl.rc_thresh,
	    last_thr, (rssi << 8));

	IEEE80211_DPRINTF(ra->ra_rs->vap, IEEE80211_MSG_RATECTL,
	    "RSSADAPT lower threshold for rate %d (last_thr %d new thr %d rssi %d)\n",
	    ra->ra_rates.rs_rates[rix + 1] & IEEE80211_RATE_VAL,
	    last_thr, (*thrs)[rix], rssi);
}

void
ieee80211_rssadapt_raise_rate(struct ieee80211_rssadapt_node *ra,
    int pktlen, int rssi)
{
	uint16_t (*thrs)[IEEE80211_RATE_SIZE];
	uint16_t newthr, oldthr;
	int rix;

	thrs = &ra->ra_rate_thresh[bucket(pktlen)];

	rix = ra->ra_rix;
	if ((*thrs)[rix + 1] > (*thrs)[rix]) {
		oldthr = (*thrs)[rix + 1];
		if ((*thrs)[rix] == 0)
			newthr = (rssi << 8);
		else
			newthr = (*thrs)[rix];
		(*thrs)[rix + 1] = interpolate(master_expavgctl.rc_decay,
		    oldthr, newthr);

		IEEE80211_DPRINTF(ra->ra_rs->vap, IEEE80211_MSG_RATECTL,
		    "RSSADAPT raise threshold for rate %d (oldthr %d newthr %d rssi %d)\n",
		    ra->ra_rates.rs_rates[rix + 1] & IEEE80211_RATE_VAL,
		    oldthr, newthr, rssi);

		ra->ra_last_raise = ticks;
	}
}

static int
rssadapt_sysctl_interval(SYSCTL_HANDLER_ARGS)
{
	struct ieee80211_rssadapt *rs = arg1;
	int msecs = ticks_to_msecs(rs->interval);
	int error;

	error = sysctl_handle_int(oidp, &msecs, 0, req);
	if (error || !req->newptr)
		return error;
	ieee80211_rssadapt_setinterval(rs, msecs);
	return 0;
}

static void
rssadapt_sysctlattach(struct ieee80211_rssadapt *rs,
    struct sysctl_ctx_list *ctx, struct sysctl_oid *tree)
{

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "rssadapt_rate_interval", CTLTYPE_INT | CTLFLAG_RW, rs,
	    0, rssadapt_sysctl_interval, "I", "rssadapt operation interval (ms)");
}

/*
 * Module glue.
 */
IEEE80211_RATE_MODULE(rssadapt, 1);
