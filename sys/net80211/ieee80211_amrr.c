/*	$OpenBSD: ieee80211_amrr.c,v 1.1 2006/06/17 19:07:19 damien Exp $	*/

/*-
 * Copyright (c) 2006
 *	Damien Bergamini <damien.bergamini@free.fr>
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

/*-
 * Naive implementation of the Adaptive Multi Rate Retry algorithm:
 *
 * "IEEE 802.11 Rate Adaptation: A Practical Approach"
 *  Mathieu Lacage, Hossein Manshaei, Thierry Turletti
 *  INRIA Sophia - Projet Planete
 *  http://www-sop.inria.fr/rapports/sophia/RR-5208.html
 */
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>

#define is_success(amn)	\
	((amn)->amn_retrycnt < (amn)->amn_txcnt / 10)
#define is_failure(amn)	\
	((amn)->amn_retrycnt > (amn)->amn_txcnt / 3)
#define is_enough(amn)		\
	((amn)->amn_txcnt > 10)

static void amrr_sysctlattach(struct ieee80211_amrr *amrr,
	struct sysctl_ctx_list *ctx, struct sysctl_oid *tree);

/* number of references from net80211 layer */
static	int nrefs = 0;

void
ieee80211_amrr_setinterval(struct ieee80211_amrr *amrr, int msecs)
{
	int t;

	if (msecs < 100)
		msecs = 100;
	t = msecs_to_ticks(msecs);
	amrr->amrr_interval = (t < 1) ? 1 : t;
}

void
ieee80211_amrr_init(struct ieee80211_amrr *amrr,
    struct ieee80211vap *vap, int amin, int amax, int interval)
{
	/* XXX bounds check? */
	amrr->amrr_min_success_threshold = amin;
	amrr->amrr_max_success_threshold = amax;
	ieee80211_amrr_setinterval(amrr, interval);

	amrr_sysctlattach(amrr, vap->iv_sysctl, vap->iv_oid);
}

void
ieee80211_amrr_cleanup(struct ieee80211_amrr *amrr)
{
}

void
ieee80211_amrr_node_init(struct ieee80211_amrr *amrr,
    struct ieee80211_amrr_node *amn, struct ieee80211_node *ni)
{
	const struct ieee80211_rateset *rs = &ni->ni_rates;

	amn->amn_amrr = amrr;
	amn->amn_success = 0;
	amn->amn_recovery = 0;
	amn->amn_txcnt = amn->amn_retrycnt = 0;
	amn->amn_success_threshold = amrr->amrr_min_success_threshold;

	/* pick initial rate */
	for (amn->amn_rix = rs->rs_nrates - 1;
	     amn->amn_rix > 0 && (rs->rs_rates[amn->amn_rix] & IEEE80211_RATE_VAL) > 72;
	     amn->amn_rix--)
		;
	ni->ni_txrate = rs->rs_rates[amn->amn_rix] & IEEE80211_RATE_VAL;
	amn->amn_ticks = ticks;

	IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_RATECTL, ni,
	    "AMRR initial rate %d", ni->ni_txrate);
}

static int
amrr_update(struct ieee80211_amrr *amrr, struct ieee80211_amrr_node *amn,
    struct ieee80211_node *ni)
{
	int rix = amn->amn_rix;

	KASSERT(is_enough(amn), ("txcnt %d", amn->amn_txcnt));

	if (is_success(amn)) {
		amn->amn_success++;
		if (amn->amn_success >= amn->amn_success_threshold &&
		    rix + 1 < ni->ni_rates.rs_nrates) {
			amn->amn_recovery = 1;
			amn->amn_success = 0;
			rix++;
			IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_RATECTL, ni,
			    "AMRR increasing rate %d (txcnt=%d retrycnt=%d)",
			    ni->ni_rates.rs_rates[rix] & IEEE80211_RATE_VAL,
			    amn->amn_txcnt, amn->amn_retrycnt);
		} else {
			amn->amn_recovery = 0;
		}
	} else if (is_failure(amn)) {
		amn->amn_success = 0;
		if (rix > 0) {
			if (amn->amn_recovery) {
				amn->amn_success_threshold *= 2;
				if (amn->amn_success_threshold >
				    amrr->amrr_max_success_threshold)
					amn->amn_success_threshold =
					    amrr->amrr_max_success_threshold;
			} else {
				amn->amn_success_threshold =
				    amrr->amrr_min_success_threshold;
			}
			rix--;
			IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_RATECTL, ni,
			    "AMRR decreasing rate %d (txcnt=%d retrycnt=%d)",
			    ni->ni_rates.rs_rates[rix] & IEEE80211_RATE_VAL,
			    amn->amn_txcnt, amn->amn_retrycnt);
		}
		amn->amn_recovery = 0;
	}

	/* reset counters */
	amn->amn_txcnt = 0;
	amn->amn_retrycnt = 0;

	return rix;
}

/*
 * Return the rate index to use in sending a data frame.
 * Update our internal state if it's been long enough.
 * If the rate changes we also update ni_txrate to match.
 */
int
ieee80211_amrr_choose(struct ieee80211_node *ni,
    struct ieee80211_amrr_node *amn)
{
	struct ieee80211_amrr *amrr = amn->amn_amrr;
	int rix;

	if (is_enough(amn) && (ticks - amn->amn_ticks) > amrr->amrr_interval) {
		rix = amrr_update(amrr, amn, ni);
		if (rix != amn->amn_rix) {
			/* update public rate */
			ni->ni_txrate =
			    ni->ni_rates.rs_rates[rix] & IEEE80211_RATE_VAL;
			amn->amn_rix = rix;
		}
		amn->amn_ticks = ticks;
	} else
		rix = amn->amn_rix;
	return rix;
}

static int
amrr_sysctl_interval(SYSCTL_HANDLER_ARGS)
{
	struct ieee80211_amrr *amrr = arg1;
	int msecs = ticks_to_msecs(amrr->amrr_interval);
	int error;

	error = sysctl_handle_int(oidp, &msecs, 0, req);
	if (error || !req->newptr)
		return error;
	ieee80211_amrr_setinterval(amrr, msecs);
	return 0;
}

static void
amrr_sysctlattach(struct ieee80211_amrr *amrr,
    struct sysctl_ctx_list *ctx, struct sysctl_oid *tree)
{

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "amrr_rate_interval", CTLTYPE_INT | CTLFLAG_RW, amrr,
	    0, amrr_sysctl_interval, "I", "amrr operation interval (ms)");
	/* XXX bounds check values */
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "amrr_max_sucess_threshold", CTLFLAG_RW,
	    &amrr->amrr_max_success_threshold, 0, "");
	SYSCTL_ADD_INT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "amrr_min_sucess_threshold", CTLFLAG_RW,
	    &amrr->amrr_min_success_threshold, 0, "");
}

/*
 * Module glue.
 */
IEEE80211_RATE_MODULE(amrr, 1);
