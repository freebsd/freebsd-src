/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
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
 * IEEE 802.11 generic handler
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <machine/atomic.h>
 
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_llc.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

#ifdef INET
#include <netinet/in.h> 
#include <netinet/if_ether.h>
#endif

#ifdef IEEE80211_DEBUG
int	ieee80211_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, ieee80211, CTLFLAG_RW, &ieee80211_debug,
	    0, "IEEE 802.11 media debugging printfs");
#endif

static void ieee80211_set11gbasicrates(struct ieee80211_rateset *,
		enum ieee80211_phymode);

static const char *ieee80211_phymode_name[] = {
	"auto",		/* IEEE80211_MODE_AUTO */
	"11a",		/* IEEE80211_MODE_11A */
	"11b",		/* IEEE80211_MODE_11B */
	"11g",		/* IEEE80211_MODE_11G */
	"turbo",	/* IEEE80211_MODE_TURBO	*/
};

void
ieee80211_ifattach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_channel *c;
	int i;

	ether_ifattach(ifp, ic->ic_myaddr);
	bpfattach2(ifp, DLT_IEEE802_11,
	    sizeof(struct ieee80211_frame_addr4), &ic->ic_rawbpf);
	ieee80211_crypto_attach(ifp);

	/*
	 * Fill in 802.11 available channel set, mark
	 * all available channels as active, and pick
	 * a default channel if not already specified.
	 */
	memset(ic->ic_chan_avail, 0, sizeof(ic->ic_chan_avail));
	ic->ic_modecaps |= 1<<IEEE80211_MODE_AUTO;
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		c = &ic->ic_channels[i];
		if (c->ic_flags) {
			/*
			 * Verify driver passed us valid data.
			 */
			if (i != ieee80211_chan2ieee(ic, c)) {
				if_printf(ifp, "bad channel ignored; "
					"freq %u flags %x number %u\n",
					c->ic_freq, c->ic_flags, i);
				c->ic_flags = 0;	/* NB: remove */
				continue;
			}
			setbit(ic->ic_chan_avail, i);
			/*
			 * Identify mode capabilities.
			 */
			if (IEEE80211_IS_CHAN_A(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_11A;
			if (IEEE80211_IS_CHAN_B(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_11B;
			if (IEEE80211_IS_CHAN_PUREG(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_11G;
			if (IEEE80211_IS_CHAN_T(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_TURBO;
		}
	}
	/* validate ic->ic_curmode */
	if ((ic->ic_modecaps & (1<<ic->ic_curmode)) == 0)
		ic->ic_curmode = IEEE80211_MODE_AUTO;

	(void) ieee80211_setmode(ic, ic->ic_curmode);

	ic->ic_des_chan = IEEE80211_CHAN_ANYC;	/* any channel is ok */
	if (ic->ic_lintval == 0)
		ic->ic_lintval = 100;		/* default sleep */
	ic->ic_bmisstimeout = 7*ic->ic_lintval;	/* default 7 beacons */

	ieee80211_node_attach(ifp);
	ieee80211_proto_attach(ifp);
}

void
ieee80211_ifdetach(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	ieee80211_proto_detach(ifp);
	ieee80211_crypto_detach(ifp);
	ieee80211_node_detach(ifp);
	ifmedia_removeall(&ic->ic_media);
	bpfdetach(ifp);
	ether_ifdetach(ifp);
}

/*
 * Convert MHz frequency to IEEE channel number.
 */
u_int
ieee80211_mhz2ieee(u_int freq, u_int flags)
{
	if (flags & IEEE80211_CHAN_2GHZ) {	/* 2GHz band */
		if (freq == 2484)
			return 14;
		if (freq < 2484)
			return (freq - 2407) / 5;
		else
			return 15 + ((freq - 2512) / 20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {	/* 5Ghz band */
		return (freq - 5000) / 5;
	} else {				/* either, guess */
		if (freq == 2484)
			return 14;
		if (freq < 2484)
			return (freq - 2407) / 5;
		if (freq < 5000)
			return 15 + ((freq - 2512) / 20);
		return (freq - 5000) / 5;
	}
}

/*
 * Convert channel to IEEE channel number.
 */
u_int
ieee80211_chan2ieee(struct ieee80211com *ic, struct ieee80211_channel *c)
{
	if (ic->ic_channels <= c && c <= &ic->ic_channels[IEEE80211_CHAN_MAX])
		return c - ic->ic_channels;
	else if (c == IEEE80211_CHAN_ANYC)
		return IEEE80211_CHAN_ANY;
	else if (c != NULL) {
		if_printf(&ic->ic_if, "invalid channel freq %u flags %x\n",
			c->ic_freq, c->ic_flags);
		return 0;		/* XXX */
	} else {
		if_printf(&ic->ic_if, "invalid channel (NULL)\n");
		return 0;		/* XXX */
	}
}

/*
 * Convert IEEE channel number to MHz frequency.
 */
u_int
ieee80211_ieee2mhz(u_int chan, u_int flags)
{
	if (flags & IEEE80211_CHAN_2GHZ) {	/* 2GHz band */
		if (chan == 14)
			return 2484;
		if (chan < 14)
			return 2407 + chan*5;
		else
			return 2512 + ((chan-15)*20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {/* 5Ghz band */
		return 5000 + (chan*5);
	} else {				/* either, guess */
		if (chan == 14)
			return 2484;
		if (chan < 14)			/* 0-13 */
			return 2407 + chan*5;
		if (chan < 27)			/* 15-26 */
			return 2512 + ((chan-15)*20);
		return 5000 + (chan*5);
	}
}

/*
 * Setup the media data structures according to the channel and
 * rate tables.  This must be called by the driver after
 * ieee80211_attach and before most anything else.
 */
void
ieee80211_media_init(struct ifnet *ifp,
	ifm_change_cb_t media_change, ifm_stat_cb_t media_stat)
{
#define	ADD(_ic, _s, _o) \
	ifmedia_add(&(_ic)->ic_media, \
		IFM_MAKEWORD(IFM_IEEE80211, (_s), (_o), 0), 0, NULL)
	struct ieee80211com *ic = (void *)ifp;
	struct ifmediareq imr;
	int i, j, mode, rate, maxrate, mword, mopt, r;
	struct ieee80211_rateset *rs;
	struct ieee80211_rateset allrates;

	/*
	 * Do late attach work that must wait for any subclass
	 * (i.e. driver) work such as overriding methods.
	 */
	ieee80211_node_lateattach(ifp);

	/*
	 * Fill in media characteristics.
	 */
	ifmedia_init(&ic->ic_media, 0, media_change, media_stat);
	maxrate = 0;
	memset(&allrates, 0, sizeof(allrates));
	for (mode = IEEE80211_MODE_AUTO; mode < IEEE80211_MODE_MAX; mode++) {
		static const u_int mopts[] = { 
			IFM_AUTO,
			IFM_MAKEMODE(IFM_IEEE80211_11A),
			IFM_MAKEMODE(IFM_IEEE80211_11B),
			IFM_MAKEMODE(IFM_IEEE80211_11G),
			IFM_MAKEMODE(IFM_IEEE80211_11A) | IFM_IEEE80211_TURBO,
		};
		if ((ic->ic_modecaps & (1<<mode)) == 0)
			continue;
		mopt = mopts[mode];
		ADD(ic, IFM_AUTO, mopt);	/* e.g. 11a auto */
		if (ic->ic_caps & IEEE80211_C_IBSS)
			ADD(ic, IFM_AUTO, mopt | IFM_IEEE80211_ADHOC);
		if (ic->ic_caps & IEEE80211_C_HOSTAP)
			ADD(ic, IFM_AUTO, mopt | IFM_IEEE80211_HOSTAP);
		if (ic->ic_caps & IEEE80211_C_AHDEMO)
			ADD(ic, IFM_AUTO, mopt | IFM_IEEE80211_ADHOC | IFM_FLAG0);
		if (ic->ic_caps & IEEE80211_C_MONITOR)
			ADD(ic, IFM_AUTO, mopt | IFM_IEEE80211_MONITOR);
		if (mode == IEEE80211_MODE_AUTO)
			continue;
		if_printf(ifp, "%s rates: ", ieee80211_phymode_name[mode]);
		rs = &ic->ic_sup_rates[mode];
		for (i = 0; i < rs->rs_nrates; i++) {
			rate = rs->rs_rates[i];
			mword = ieee80211_rate2media(ic, rate, mode);
			if (mword == 0)
				continue;
			printf("%s%d%sMbps", (i != 0 ? " " : ""),
			    (rate & IEEE80211_RATE_VAL) / 2,
			    ((rate & 0x1) != 0 ? ".5" : ""));
			ADD(ic, mword, mopt);
			if (ic->ic_caps & IEEE80211_C_IBSS)
				ADD(ic, mword, mopt | IFM_IEEE80211_ADHOC);
			if (ic->ic_caps & IEEE80211_C_HOSTAP)
				ADD(ic, mword, mopt | IFM_IEEE80211_HOSTAP);
			if (ic->ic_caps & IEEE80211_C_AHDEMO)
				ADD(ic, mword, mopt | IFM_IEEE80211_ADHOC | IFM_FLAG0);
			if (ic->ic_caps & IEEE80211_C_MONITOR)
				ADD(ic, mword, mopt | IFM_IEEE80211_MONITOR);
			/*
			 * Add rate to the collection of all rates.
			 */
			r = rate & IEEE80211_RATE_VAL;
			for (j = 0; j < allrates.rs_nrates; j++)
				if (allrates.rs_rates[j] == r)
					break;
			if (j == allrates.rs_nrates) {
				/* unique, add to the set */
				allrates.rs_rates[j] = r;
				allrates.rs_nrates++;
			}
			rate = (rate & IEEE80211_RATE_VAL) / 2;
			if (rate > maxrate)
				maxrate = rate;
		}
		printf("\n");
	}
	for (i = 0; i < allrates.rs_nrates; i++) {
		mword = ieee80211_rate2media(ic, allrates.rs_rates[i],
				IEEE80211_MODE_AUTO);
		if (mword == 0)
			continue;
		mword = IFM_SUBTYPE(mword);	/* remove media options */
		ADD(ic, mword, 0);
		if (ic->ic_caps & IEEE80211_C_IBSS)
			ADD(ic, mword, IFM_IEEE80211_ADHOC);
		if (ic->ic_caps & IEEE80211_C_HOSTAP)
			ADD(ic, mword, IFM_IEEE80211_HOSTAP);
		if (ic->ic_caps & IEEE80211_C_AHDEMO)
			ADD(ic, mword, IFM_IEEE80211_ADHOC | IFM_FLAG0);
		if (ic->ic_caps & IEEE80211_C_MONITOR)
			ADD(ic, mword, IFM_IEEE80211_MONITOR);
	}
	ieee80211_media_status(ifp, &imr);
	ifmedia_set(&ic->ic_media, imr.ifm_active);

	if (maxrate)
		ifp->if_baudrate = IF_Mbps(maxrate);
#undef ADD
}

static int
findrate(struct ieee80211com *ic, enum ieee80211_phymode mode, int rate)
{
#define	IEEERATE(_ic,_m,_i) \
	((_ic)->ic_sup_rates[_m].rs_rates[_i] & IEEE80211_RATE_VAL)
	int i, nrates = ic->ic_sup_rates[mode].rs_nrates;
	for (i = 0; i < nrates; i++)
		if (IEEERATE(ic, mode, i) == rate)
			return i;
	return -1;
#undef IEEERATE
}

/*
 * Handle a media change request.
 */
int
ieee80211_media_change(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ifmedia_entry *ime;
	enum ieee80211_opmode newopmode;
	enum ieee80211_phymode newphymode;
	int i, j, newrate, error = 0;

	ime = ic->ic_media.ifm_cur;
	/*
	 * First, identify the phy mode.
	 */
	switch (IFM_MODE(ime->ifm_media)) {
	case IFM_IEEE80211_11A:
		newphymode = IEEE80211_MODE_11A;
		break;
	case IFM_IEEE80211_11B:
		newphymode = IEEE80211_MODE_11B;
		break;
	case IFM_IEEE80211_11G:
		newphymode = IEEE80211_MODE_11G;
		break;
	case IFM_AUTO:
		newphymode = IEEE80211_MODE_AUTO;
		break;
	default:
		return EINVAL;
	}
	/*
	 * Turbo mode is an ``option''.  Eventually it
	 * needs to be applied to 11g too.
	 */
	if (ime->ifm_media & IFM_IEEE80211_TURBO) {
		if (newphymode != IEEE80211_MODE_11A)
			return EINVAL;
		newphymode = IEEE80211_MODE_TURBO;
	}
	/*
	 * Validate requested mode is available.
	 */
	if ((ic->ic_modecaps & (1<<newphymode)) == 0)
		return EINVAL;

	/*
	 * Next, the fixed/variable rate.
	 */
	i = -1;
	if (IFM_SUBTYPE(ime->ifm_media) != IFM_AUTO) {
		/*
		 * Convert media subtype to rate.
		 */
		newrate = ieee80211_media2rate(ime->ifm_media);
		if (newrate == 0)
			return EINVAL;
		/*
		 * Check the rate table for the specified/current phy.
		 */
		if (newphymode == IEEE80211_MODE_AUTO) {
			/*
			 * In autoselect mode search for the rate.
			 */
			for (j = IEEE80211_MODE_11A;
			     j < IEEE80211_MODE_MAX; j++) {
				if ((ic->ic_modecaps & (1<<j)) == 0)
					continue;
				i = findrate(ic, j, newrate);
				if (i != -1) {
					/* lock mode too */
					newphymode = j;
					break;
				}
			}
		} else {
			i = findrate(ic, newphymode, newrate);
		}
		if (i == -1)			/* mode/rate mismatch */
			return EINVAL;
	}
	/* NB: defer rate setting to later */

	/*
	 * Deduce new operating mode but don't install it just yet.
	 */
	if ((ime->ifm_media & (IFM_IEEE80211_ADHOC|IFM_FLAG0)) ==
	    (IFM_IEEE80211_ADHOC|IFM_FLAG0))
		newopmode = IEEE80211_M_AHDEMO;
	else if (ime->ifm_media & IFM_IEEE80211_HOSTAP)
		newopmode = IEEE80211_M_HOSTAP;
	else if (ime->ifm_media & IFM_IEEE80211_ADHOC)
		newopmode = IEEE80211_M_IBSS;
	else if (ime->ifm_media & IFM_IEEE80211_MONITOR)
		newopmode = IEEE80211_M_MONITOR;
	else
		newopmode = IEEE80211_M_STA;

	/*
	 * Autoselect doesn't make sense when operating as an AP.
	 * If no phy mode has been selected, pick one and lock it
	 * down so rate tables can be used in forming beacon frames
	 * and the like.
	 */
	if (newopmode == IEEE80211_M_HOSTAP &&
	    newphymode == IEEE80211_MODE_AUTO) {
		for (j = IEEE80211_MODE_11A; j < IEEE80211_MODE_MAX; j++)
			if (ic->ic_modecaps & (1<<j)) {
				newphymode = j;
				break;
			}
	}

	/*
	 * Handle phy mode change.
	 */
	if (ic->ic_curmode != newphymode) {		/* change phy mode */
		error = ieee80211_setmode(ic, newphymode);
		if (error != 0)
			return error;
		error = ENETRESET;
	}

	/*
	 * Committed to changes, install the rate setting.
	 */
	if (ic->ic_fixed_rate != i) {
		ic->ic_fixed_rate = i;			/* set fixed tx rate */
		error = ENETRESET;
	}

	/*
	 * Handle operating mode change.
	 */
	if (ic->ic_opmode != newopmode) {
		ic->ic_opmode = newopmode;
		switch (newopmode) {
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
		case IEEE80211_M_STA:
		case IEEE80211_M_MONITOR:
			ic->ic_flags &= ~IEEE80211_F_IBSSON;
			break;
		case IEEE80211_M_IBSS:
			ic->ic_flags |= IEEE80211_F_IBSSON;
#ifdef notdef
			if (ic->ic_curmode == IEEE80211_MODE_11G)
				ieee80211_set11gbasicrates(
					&ic->ic_suprates[newphymode],
					IEEE80211_MODE_11B);
#endif
			break;
		}
		error = ENETRESET;
	}
#ifdef notdef
	if (error == 0)
		ifp->if_baudrate = ifmedia_baudrate(ime->ifm_media);
#endif
	return error;
}

void
ieee80211_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ieee80211_node *ni = NULL;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;
	imr->ifm_active |= IFM_AUTO;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		ni = ic->ic_bss;
		/* calculate rate subtype */
		imr->ifm_active |= ieee80211_rate2media(ic,
			ni->ni_rates.rs_rates[ni->ni_txrate], ic->ic_curmode);
		break;
	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_M_AHDEMO:
		/* should not come here */
		break;
	case IEEE80211_M_HOSTAP:
		imr->ifm_active |= IFM_IEEE80211_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
		break;
	}
	switch (ic->ic_curmode) {
	case IEEE80211_MODE_11A:
		imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11A);
		break;
	case IEEE80211_MODE_11B:
		imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11B);
		break;
	case IEEE80211_MODE_11G:
		imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11G);
		break;
	case IEEE80211_MODE_TURBO:
		imr->ifm_active |= IFM_MAKEMODE(IFM_IEEE80211_11A)
				|  IFM_IEEE80211_TURBO;
		break;
	}
}

void
ieee80211_watchdog(struct ifnet *ifp)
{
	struct ieee80211com *ic = (void *)ifp;

	if (ic->ic_mgt_timer && --ic->ic_mgt_timer == 0)
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	if (ic->ic_inact_timer && --ic->ic_inact_timer == 0)
		ieee80211_timeout_nodes(ic);

	if (ic->ic_mgt_timer != 0 || ic->ic_inact_timer != 0)
		ifp->if_timer = 1;
}

/*
 * Mark the basic rates for the 11g rate table based on the
 * operating mode.  For real 11g we mark all the 11b rates
 * and 6, 12, and 24 OFDM.  For 11b compatibility we mark only
 * 11b rates.  There's also a pseudo 11a-mode used to mark only
 * the basic OFDM rates.
 */
static void
ieee80211_set11gbasicrates(struct ieee80211_rateset *rs, enum ieee80211_phymode mode)
{
	static const struct ieee80211_rateset basic[] = {
	    { 3, { 12, 24, 48 } },		/* IEEE80211_MODE_11A */
	    { 4, { 2, 4, 11, 22 } },		/* IEEE80211_MODE_11B */
	    { 7, { 2, 4, 11, 22, 12, 24, 48 } },/* IEEE80211_MODE_11G */
	    { 0 },				/* IEEE80211_MODE_TURBO	*/
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
 * Set the current phy mode and recalculate the active channel
 * set based on the available channels for this mode.  Also
 * select a new default/current channel if the current one is
 * inappropriate for this mode.
 */
int
ieee80211_setmode(struct ieee80211com *ic, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const u_int chanflags[] = {
		0,			/* IEEE80211_MODE_AUTO */
		IEEE80211_CHAN_A,	/* IEEE80211_MODE_11A */
		IEEE80211_CHAN_B,	/* IEEE80211_MODE_11B */
		IEEE80211_CHAN_PUREG,	/* IEEE80211_MODE_11G */
		IEEE80211_CHAN_T,	/* IEEE80211_MODE_TURBO	*/
	};
	struct ieee80211_channel *c;
	u_int modeflags;
	int i;

	/* validate new mode */
	if ((ic->ic_modecaps & (1<<mode)) == 0) {
		IEEE80211_DPRINTF(("%s: mode %u not supported (caps 0x%x)\n",
			__func__, mode, ic->ic_modecaps));
		return EINVAL;
	}

	/*
	 * Verify at least one channel is present in the available
	 * channel list before committing to the new mode.
	 */
	KASSERT(mode < N(chanflags), ("Unexpected mode %u\n", mode));
	modeflags = chanflags[mode];
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		c = &ic->ic_channels[i];
		if (mode == IEEE80211_MODE_AUTO) {
			/* ignore turbo channels for autoselect */
			if ((c->ic_flags &~ IEEE80211_CHAN_TURBO) != 0)
				break;
		} else {
			if ((c->ic_flags & modeflags) == modeflags)
				break;
		}
	}
	if (i > IEEE80211_CHAN_MAX) {
		IEEE80211_DPRINTF(("%s: no channels found for mode %u\n",
			__func__, mode));
		return EINVAL;
	}

	/*
	 * Calculate the active channel set.
	 */
	memset(ic->ic_chan_active, 0, sizeof(ic->ic_chan_active));
	for (i = 0; i <= IEEE80211_CHAN_MAX; i++) {
		c = &ic->ic_channels[i];
		if (mode == IEEE80211_MODE_AUTO) {
			/* take anything but pure turbo channels */
			if ((c->ic_flags &~ IEEE80211_CHAN_TURBO) != 0)
				setbit(ic->ic_chan_active, i);
		} else {
			if ((c->ic_flags & modeflags) == modeflags)
				setbit(ic->ic_chan_active, i);
		}
	}
	/*
	 * If no current/default channel is setup or the current
	 * channel is wrong for the mode then pick the first
	 * available channel from the active list.  This is likely
	 * not the right one.
	 */
	if (ic->ic_ibss_chan == NULL ||
	    isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ic->ic_ibss_chan))) {
		for (i = 0; i <= IEEE80211_CHAN_MAX; i++)
			if (isset(ic->ic_chan_active, i)) {
				ic->ic_ibss_chan = &ic->ic_channels[i];
				break;
			}
	}

	/*
	 * Set/reset state flags that influence beacon contents, etc.
	 *
	 * XXX what if we have stations already associated???
	 * XXX probably not right for autoselect?
	 */
	if (mode == IEEE80211_MODE_11G) {
		if (ic->ic_caps & IEEE80211_C_SHSLOT)
			ic->ic_flags |= IEEE80211_F_SHSLOT;
		if (ic->ic_caps & IEEE80211_C_SHPREAMBLE)
			ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
		ieee80211_set11gbasicrates(&ic->ic_sup_rates[mode],
			IEEE80211_MODE_11G);
	} else {
		ic->ic_flags &= ~(IEEE80211_F_SHSLOT | IEEE80211_F_SHPREAMBLE);
	}

	ic->ic_curmode = mode;
	return 0;
#undef N
}

/*
 * Return the phy mode for with the specified channel so the
 * caller can select a rate set.  This is problematic and the
 * work here assumes how things work elsewhere in this code.
 */
enum ieee80211_phymode
ieee80211_chan2mode(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
	/*
	 * NB: this assumes the channel would not be supplied to us
	 *     unless it was already compatible with the current mode.
	 */
	if (ic->ic_curmode != IEEE80211_MODE_AUTO)
		return ic->ic_curmode;
	/*
	 * In autoselect mode; deduce a mode based on the channel
	 * characteristics.  We assume that turbo-only channels
	 * are not considered when the channel set is constructed.
	 */
	if (IEEE80211_IS_CHAN_5GHZ(chan))
		return IEEE80211_MODE_11A;
	else if (chan->ic_flags & (IEEE80211_CHAN_OFDM|IEEE80211_CHAN_DYN))
		return IEEE80211_MODE_11G;
	else
		return IEEE80211_MODE_11B;
}

/*
 * convert IEEE80211 rate value to ifmedia subtype.
 * ieee80211 rate is in unit of 0.5Mbps.
 */
int
ieee80211_rate2media(struct ieee80211com *ic, int rate, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const struct {
		u_int	m;	/* rate + mode */
		u_int	r;	/* if_media rate */
	} rates[] = {
		{   2 | IFM_MAKEMODE(IFM_IEEE80211_11B), IFM_IEEE80211_DS1 },
		{   4 | IFM_MAKEMODE(IFM_IEEE80211_11B), IFM_IEEE80211_DS2 },
		{  11 | IFM_MAKEMODE(IFM_IEEE80211_11B), IFM_IEEE80211_DS5 },
		{  22 | IFM_MAKEMODE(IFM_IEEE80211_11B), IFM_IEEE80211_DS11 },
		{  44 | IFM_MAKEMODE(IFM_IEEE80211_11B), IFM_IEEE80211_DS22 },
		{  12 | IFM_MAKEMODE(IFM_IEEE80211_11A), IFM_IEEE80211_OFDM6 },
		{  18 | IFM_MAKEMODE(IFM_IEEE80211_11A), IFM_IEEE80211_OFDM9 },
		{  24 | IFM_MAKEMODE(IFM_IEEE80211_11A), IFM_IEEE80211_OFDM12 },
		{  36 | IFM_MAKEMODE(IFM_IEEE80211_11A), IFM_IEEE80211_OFDM18 },
		{  48 | IFM_MAKEMODE(IFM_IEEE80211_11A), IFM_IEEE80211_OFDM24 },
		{  72 | IFM_MAKEMODE(IFM_IEEE80211_11A), IFM_IEEE80211_OFDM36 },
		{  96 | IFM_MAKEMODE(IFM_IEEE80211_11A), IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_MAKEMODE(IFM_IEEE80211_11A), IFM_IEEE80211_OFDM54 },
		{   2 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_DS1 },
		{   4 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_DS2 },
		{  11 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_DS5 },
		{  22 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_DS11 },
		{  12 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_OFDM6 },
		{  18 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_OFDM9 },
		{  24 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_OFDM12 },
		{  36 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_OFDM18 },
		{  48 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_OFDM24 },
		{  72 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_OFDM36 },
		{  96 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_MAKEMODE(IFM_IEEE80211_11G), IFM_IEEE80211_OFDM54 },
		/* NB: OFDM72 doesn't realy exist so we don't handle it */
	};
	u_int mask, i;

	mask = rate & IEEE80211_RATE_VAL;
	switch (mode) {
	case IEEE80211_MODE_11A:
	case IEEE80211_MODE_TURBO:
		mask |= IFM_MAKEMODE(IFM_IEEE80211_11A);
		break;
	case IEEE80211_MODE_11B:
		mask |= IFM_MAKEMODE(IFM_IEEE80211_11B);
		break;
	case IEEE80211_MODE_AUTO:
		/* NB: ic may be NULL for some drivers */
		if (ic && ic->ic_phytype == IEEE80211_T_FH) {
			/* must handle these specially */
			switch (mask) {
			case 2:		return IFM_IEEE80211_FH1;
			case 4:		return IFM_IEEE80211_FH2;
			}
			return IFM_AUTO;
		}
		/* NB: hack, 11g matches both 11b+11a rates */
		/* fall thru... */
	case IEEE80211_MODE_11G:
		mask |= IFM_MAKEMODE(IFM_IEEE80211_11G);
		break;
	}
	for (i = 0; i < N(rates); i++)
		if (rates[i].m == mask)
			return rates[i].r;
	return IFM_AUTO;
#undef N
}

int
ieee80211_media2rate(int mword)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const int ieeerates[] = {
		-1,		/* IFM_AUTO */
		0,		/* IFM_MANUAL */
		0,		/* IFM_NONE */
		2,		/* IFM_IEEE80211_FH1 */
		4,		/* IFM_IEEE80211_FH2 */
		2,		/* IFM_IEEE80211_DS1 */
		4,		/* IFM_IEEE80211_DS2 */
		11,		/* IFM_IEEE80211_DS5 */
		22,		/* IFM_IEEE80211_DS11 */
		44,		/* IFM_IEEE80211_DS22 */
		12,		/* IFM_IEEE80211_OFDM6 */
		18,		/* IFM_IEEE80211_OFDM9 */
		24,		/* IFM_IEEE80211_OFDM12 */
		36,		/* IFM_IEEE80211_OFDM18 */
		48,		/* IFM_IEEE80211_OFDM24 */
		72,		/* IFM_IEEE80211_OFDM36 */
		96,		/* IFM_IEEE80211_OFDM48 */
		108,		/* IFM_IEEE80211_OFDM54 */
		144,		/* IFM_IEEE80211_OFDM72 */
	};
	return IFM_SUBTYPE(mword) < N(ieeerates) ?
		ieeerates[IFM_SUBTYPE(mword)] : 0;
#undef N
}

/*
 * Module glue.
 *
 * NB: the module name is "wlan" for compatibility with NetBSD.
 */

static int
ieee80211_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		if (bootverbose)
			printf("wlan: <802.11 Link Layer>\n");
		return 0;
	case MOD_UNLOAD:
		return 0;
	}
	return EINVAL;
}

static moduledata_t ieee80211_mod = {
	"wlan",
	ieee80211_modevent,
	0
};
DECLARE_MODULE(wlan, ieee80211_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(wlan, 1);
MODULE_DEPEND(wlan, rc4, 1, 1, 1);
