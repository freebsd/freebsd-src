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
 * IEEE 802.11 generic handler
 */

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/kernel.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

const char *ieee80211_phymode_name[] = {
	"auto",		/* IEEE80211_MODE_AUTO */
	"11a",		/* IEEE80211_MODE_11A */
	"11b",		/* IEEE80211_MODE_11B */
	"11g",		/* IEEE80211_MODE_11G */
	"FH",		/* IEEE80211_MODE_FH */
	"turboA",	/* IEEE80211_MODE_TURBO_A */
	"turboG",	/* IEEE80211_MODE_TURBO_G */
};

/* list of all instances */
SLIST_HEAD(ieee80211_list, ieee80211com);
static struct ieee80211_list ieee80211_list =
	SLIST_HEAD_INITIALIZER(ieee80211_list);
static u_int8_t ieee80211_vapmap[32];		/* enough for 256 */
static struct mtx ieee80211_vap_mtx;
MTX_SYSINIT(ieee80211, &ieee80211_vap_mtx, "net80211 instances", MTX_DEF);

static void
ieee80211_add_vap(struct ieee80211com *ic)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;
	u_int8_t b;

	mtx_lock(&ieee80211_vap_mtx);
	ic->ic_vap = 0;
	for (i = 0; i < N(ieee80211_vapmap) && ieee80211_vapmap[i] == 0xff; i++)
		ic->ic_vap += NBBY;
	if (i == N(ieee80211_vapmap))
		panic("vap table full");
	for (b = ieee80211_vapmap[i]; b & 1; b >>= 1)
		ic->ic_vap++;
	setbit(ieee80211_vapmap, ic->ic_vap);
	SLIST_INSERT_HEAD(&ieee80211_list, ic, ic_next);
	mtx_unlock(&ieee80211_vap_mtx);
#undef N
}

static void
ieee80211_remove_vap(struct ieee80211com *ic)
{
	mtx_lock(&ieee80211_vap_mtx);
	SLIST_REMOVE(&ieee80211_list, ic, ieee80211com, ic_next);
	KASSERT(ic->ic_vap < sizeof(ieee80211_vapmap)*NBBY,
		("invalid vap id %d", ic->ic_vap));
	KASSERT(isset(ieee80211_vapmap, ic->ic_vap),
		("vap id %d not allocated", ic->ic_vap));
	clrbit(ieee80211_vapmap, ic->ic_vap);
	mtx_unlock(&ieee80211_vap_mtx);
}

/*
 * Default reset method for use with the ioctl support.  This
 * method is invoked after any state change in the 802.11
 * layer that should be propagated to the hardware but not
 * require re-initialization of the 802.11 state machine (e.g
 * rescanning for an ap).  We always return ENETRESET which
 * should cause the driver to re-initialize the device. Drivers
 * can override this method to implement more optimized support.
 */
static int
ieee80211_default_reset(struct ifnet *ifp)
{
	return ENETRESET;
}

void
ieee80211_ifattach(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	struct ieee80211_channel *c;
	int i;

	ether_ifattach(ifp, ic->ic_myaddr);
	bpfattach2(ifp, DLT_IEEE802_11,
	    sizeof(struct ieee80211_frame_addr4), &ic->ic_rawbpf);

	ieee80211_crypto_attach(ic);

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
			if (IEEE80211_IS_CHAN_FHSS(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_FH;
			if (IEEE80211_IS_CHAN_T(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_TURBO_A;
			if (IEEE80211_IS_CHAN_108G(c))
				ic->ic_modecaps |= 1<<IEEE80211_MODE_TURBO_G;
			if (ic->ic_curchan == NULL) {
				/* arbitrarily pick the first channel */
				ic->ic_curchan = &ic->ic_channels[i];
			}
		}
	}
	/* validate ic->ic_curmode */
	if ((ic->ic_modecaps & (1<<ic->ic_curmode)) == 0)
		ic->ic_curmode = IEEE80211_MODE_AUTO;
	ic->ic_des_chan = IEEE80211_CHAN_ANYC;	/* any channel is ok */
#if 0
	/*
	 * Enable WME by default if we're capable.
	 */
	if (ic->ic_caps & IEEE80211_C_WME)
		ic->ic_flags |= IEEE80211_F_WME;
#endif
	(void) ieee80211_setmode(ic, ic->ic_curmode);

	if (ic->ic_bintval == 0)
		ic->ic_bintval = IEEE80211_BINTVAL_DEFAULT;
	ic->ic_bmisstimeout = 7*ic->ic_bintval;	/* default 7 beacons */
	ic->ic_dtim_period = IEEE80211_DTIM_DEFAULT;
	IEEE80211_BEACON_LOCK_INIT(ic, "beacon");

	if (ic->ic_lintval == 0)
		ic->ic_lintval = ic->ic_bintval;
	ic->ic_txpowlimit = IEEE80211_TXPOWER_MAX;

	ieee80211_node_attach(ic);
	ieee80211_proto_attach(ic);

	ieee80211_add_vap(ic);

	ieee80211_sysctl_attach(ic);		/* NB: requires ic_vap */

	/*
	 * Install a default reset method for the ioctl support.
	 * The driver is expected to fill this in before calling us.
	 */
	if (ic->ic_reset == NULL)
		ic->ic_reset = ieee80211_default_reset;
}

void
ieee80211_ifdetach(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;

	ieee80211_remove_vap(ic);

	ieee80211_sysctl_detach(ic);
	ieee80211_proto_detach(ic);
	ieee80211_crypto_detach(ic);
	ieee80211_node_detach(ic);
	ifmedia_removeall(&ic->ic_media);

	IEEE80211_BEACON_LOCK_DESTROY(ic);

	bpfdetach(ifp);
	ether_ifdetach(ifp);
}

/*
 * Convert MHz frequency to IEEE channel number.
 */
int
ieee80211_mhz2ieee(u_int freq, u_int flags)
{
#define IS_CHAN_IN_PUBLIC_SAFETY_BAND(_c) ((_c) > 4940 && (_c) < 4990)
	if (flags & IEEE80211_CHAN_2GHZ) {	/* 2GHz band */
		if (freq == 2484)
			return 14;
		if (freq < 2484)
			return ((int) freq - 2407) / 5;
		else
			return 15 + ((freq - 2512) / 20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {	/* 5Ghz band */
		if (IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq))
			return ((freq * 10) +
				(((freq % 5) == 2) ? 5 : 0) - 49400) / 5;
		if (freq <= 5000)
			return (freq - 4000) / 5;
		else
			return (freq - 5000) / 5;
	} else {				/* either, guess */
		if (freq == 2484)
			return 14;
		if (freq < 2484)
			return ((int) freq - 2407) / 5;
		if (freq < 5000) {
			if (IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq))
				return ((freq * 10) +
					(((freq % 5) == 2) ? 5 : 0) - 49400)/5;
			else if (freq > 4900)
				return (freq - 4000) / 5;
			else
				return 15 + ((freq - 2512) / 20);
		}
		return (freq - 5000) / 5;
	}
#undef IS_CHAN_IN_PUBLIC_SAFETY_BAND
}

/*
 * Convert channel to IEEE channel number.
 */
int
ieee80211_chan2ieee(struct ieee80211com *ic, struct ieee80211_channel *c)
{
	if (ic->ic_channels <= c && c <= &ic->ic_channels[IEEE80211_CHAN_MAX])
		return c - ic->ic_channels;
	else if (c == IEEE80211_CHAN_ANYC)
		return IEEE80211_CHAN_ANY;
	else if (c != NULL) {
		if_printf(ic->ic_ifp, "invalid channel freq %u flags %x\n",
			c->ic_freq, c->ic_flags);
		return 0;		/* XXX */
	} else {
		if_printf(ic->ic_ifp, "invalid channel (NULL)\n");
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
ieee80211_media_init(struct ieee80211com *ic,
	ifm_change_cb_t media_change, ifm_stat_cb_t media_stat)
{
#define	ADD(_ic, _s, _o) \
	ifmedia_add(&(_ic)->ic_media, \
		IFM_MAKEWORD(IFM_IEEE80211, (_s), (_o), 0), 0, NULL)
	struct ifnet *ifp = ic->ic_ifp;
	struct ifmediareq imr;
	int i, j, mode, rate, maxrate, mword, mopt, r;
	struct ieee80211_rateset *rs;
	struct ieee80211_rateset allrates;

	/*
	 * Do late attach work that must wait for any subclass
	 * (i.e. driver) work such as overriding methods.
	 */
	ieee80211_node_lateattach(ic);

	/*
	 * Fill in media characteristics.
	 */
	ifmedia_init(&ic->ic_media, 0, media_change, media_stat);
	maxrate = 0;
	memset(&allrates, 0, sizeof(allrates));
	for (mode = IEEE80211_MODE_AUTO; mode < IEEE80211_MODE_MAX; mode++) {
		static const u_int mopts[] = { 
			IFM_AUTO,
			IFM_IEEE80211_11A,
			IFM_IEEE80211_11B,
			IFM_IEEE80211_11G,
			IFM_IEEE80211_FH,
			IFM_IEEE80211_11A | IFM_IEEE80211_TURBO,
			IFM_IEEE80211_11G | IFM_IEEE80211_TURBO,
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
		rs = &ic->ic_sup_rates[mode];
		for (i = 0; i < rs->rs_nrates; i++) {
			rate = rs->rs_rates[i];
			mword = ieee80211_rate2media(ic, rate, mode);
			if (mword == 0)
				continue;
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

void
ieee80211_announce(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	int i, mode, rate, mword;
	struct ieee80211_rateset *rs;

	for (mode = IEEE80211_MODE_11A; mode < IEEE80211_MODE_MAX; mode++) {
		if ((ic->ic_modecaps & (1<<mode)) == 0)
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
		}
		printf("\n");
	}
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
 * Find an instance by it's mac address.
 */
struct ieee80211com *
ieee80211_find_vap(const u_int8_t mac[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic;

	/* XXX lock */
	SLIST_FOREACH(ic, &ieee80211_list, ic_next)
		if (IEEE80211_ADDR_EQ(mac, ic->ic_myaddr))
			return ic;
	return NULL;
}

static struct ieee80211com *
ieee80211_find_instance(struct ifnet *ifp)
{
	struct ieee80211com *ic;

	/* XXX lock */
	/* XXX not right for multiple instances but works for now */
	SLIST_FOREACH(ic, &ieee80211_list, ic_next)
		if (ic->ic_ifp == ifp)
			return ic;
	return NULL;
}

/*
 * Handle a media change request.
 */
int
ieee80211_media_change(struct ifnet *ifp)
{
	struct ieee80211com *ic;
	struct ifmedia_entry *ime;
	enum ieee80211_opmode newopmode;
	enum ieee80211_phymode newphymode;
	int i, j, newrate, error = 0;

	ic = ieee80211_find_instance(ifp);
	if (!ic) {
		if_printf(ifp, "%s: no 802.11 instance!\n", __func__);
		return EINVAL;
	}
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
	case IFM_IEEE80211_FH:
		newphymode = IEEE80211_MODE_FH;
		break;
	case IFM_AUTO:
		newphymode = IEEE80211_MODE_AUTO;
		break;
	default:
		return EINVAL;
	}
	/*
	 * Turbo mode is an ``option''.
	 * XXX does not apply to AUTO
	 */
	if (ime->ifm_media & IFM_IEEE80211_TURBO) {
		if (newphymode == IEEE80211_MODE_11A)
			newphymode = IEEE80211_MODE_TURBO_A;
		else if (newphymode == IEEE80211_MODE_11G)
			newphymode = IEEE80211_MODE_TURBO_G;
		else
			return EINVAL;
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
			break;
		}
		/*
		 * Yech, slot time may change depending on the
		 * operating mode so reset it to be sure everything
		 * is setup appropriately.
		 */
		ieee80211_reset_erp(ic);
		ieee80211_wme_initparams(ic);	/* after opmode change */
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
	struct ieee80211com *ic;
	struct ieee80211_rateset *rs;

	ic = ieee80211_find_instance(ifp);
	if (!ic) {
		if_printf(ifp, "%s: no 802.11 instance!\n", __func__);
		return;
	}
	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;
	/*
	 * Calculate a current rate if possible.
	 */
	if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE) {
		/*
		 * A fixed rate is set, report that.
		 */
		rs = &ic->ic_sup_rates[ic->ic_curmode];
		imr->ifm_active |= ieee80211_rate2media(ic,
			rs->rs_rates[ic->ic_fixed_rate], ic->ic_curmode);
	} else if (ic->ic_opmode == IEEE80211_M_STA) {
		/*
		 * In station mode report the current transmit rate.
		 */
		rs = &ic->ic_bss->ni_rates;
		imr->ifm_active |= ieee80211_rate2media(ic,
			rs->rs_rates[ic->ic_bss->ni_txrate], ic->ic_curmode);
	} else
		imr->ifm_active |= IFM_AUTO;
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
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
		imr->ifm_active |= IFM_IEEE80211_11A;
		break;
	case IEEE80211_MODE_11B:
		imr->ifm_active |= IFM_IEEE80211_11B;
		break;
	case IEEE80211_MODE_11G:
		imr->ifm_active |= IFM_IEEE80211_11G;
		break;
	case IEEE80211_MODE_FH:
		imr->ifm_active |= IFM_IEEE80211_FH;
		break;
	case IEEE80211_MODE_TURBO_A:
		imr->ifm_active |= IFM_IEEE80211_11A
				|  IFM_IEEE80211_TURBO;
		break;
	case IEEE80211_MODE_TURBO_G:
		imr->ifm_active |= IFM_IEEE80211_11G
				|  IFM_IEEE80211_TURBO;
		break;
	}
}

void
ieee80211_watchdog(struct ieee80211com *ic)
{
	struct ieee80211_node_table *nt;
	int need_inact_timer = 0;

	if (ic->ic_state != IEEE80211_S_INIT) {
		if (ic->ic_mgt_timer && --ic->ic_mgt_timer == 0)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, 0);
		nt = &ic->ic_scan;
		if (nt->nt_inact_timer) {
			if (--nt->nt_inact_timer == 0)
				nt->nt_timeout(nt);
			need_inact_timer += nt->nt_inact_timer;
		}
		nt = &ic->ic_sta;
		if (nt->nt_inact_timer) {
			if (--nt->nt_inact_timer == 0)
				nt->nt_timeout(nt);
			need_inact_timer += nt->nt_inact_timer;
		}
	}
	if (ic->ic_mgt_timer != 0 || need_inact_timer)
		ic->ic_ifp->if_timer = 1;
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
		IEEE80211_CHAN_FHSS,	/* IEEE80211_MODE_FH */
		IEEE80211_CHAN_T,	/* IEEE80211_MODE_TURBO_A */
		IEEE80211_CHAN_108G,	/* IEEE80211_MODE_TURBO_G */
	};
	struct ieee80211_channel *c;
	u_int modeflags;
	int i;

	/* validate new mode */
	if ((ic->ic_modecaps & (1<<mode)) == 0) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"%s: mode %u not supported (caps 0x%x)\n",
			__func__, mode, ic->ic_modecaps);
		return EINVAL;
	}

	/*
	 * Verify at least one channel is present in the available
	 * channel list before committing to the new mode.
	 */
	KASSERT(mode < N(chanflags), ("Unexpected mode %u", mode));
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
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ANY,
			"%s: no channels found for mode %u\n", __func__, mode);
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
		KASSERT(ic->ic_ibss_chan != NULL &&
		    isset(ic->ic_chan_active,
			ieee80211_chan2ieee(ic, ic->ic_ibss_chan)),
		    ("Bad IBSS channel %u",
		     ieee80211_chan2ieee(ic, ic->ic_ibss_chan)));
	}
	/*
	 * If the desired channel is set but no longer valid then reset it.
	 */
	if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
	    isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ic->ic_des_chan)))
		ic->ic_des_chan = IEEE80211_CHAN_ANYC;

	/*
	 * Do mode-specific rate setup.
	 */
	if (mode == IEEE80211_MODE_11G) {
		/*
		 * Use a mixed 11b/11g rate set.
		 */
		ieee80211_set11gbasicrates(&ic->ic_sup_rates[mode],
			IEEE80211_MODE_11G);
	} else if (mode == IEEE80211_MODE_11B) {
		/*
		 * Force pure 11b rate set.
		 */
		ieee80211_set11gbasicrates(&ic->ic_sup_rates[mode],
			IEEE80211_MODE_11B);
	}
	/*
	 * Setup an initial rate set according to the
	 * current/default channel selected above.  This
	 * will be changed when scanning but must exist
	 * now so driver have a consistent state of ic_ibss_chan.
	 */
	if (ic->ic_bss)		/* NB: can be called before lateattach */
		ic->ic_bss->ni_rates = ic->ic_sup_rates[mode];

	ic->ic_curmode = mode;
	ieee80211_reset_erp(ic);	/* reset ERP state */
	ieee80211_wme_initparams(ic);	/* reset WME stat */

	return 0;
#undef N
}

/*
 * Return the phy mode for with the specified channel so the
 * caller can select a rate set.  This is problematic for channels
 * where multiple operating modes are possible (e.g. 11g+11b).
 * In those cases we defer to the current operating mode when set.
 */
enum ieee80211_phymode
ieee80211_chan2mode(struct ieee80211com *ic, struct ieee80211_channel *chan)
{
	if (IEEE80211_IS_CHAN_5GHZ(chan)) {
		/*
		 * This assumes all 11a turbo channels are also
		 * usable withut turbo, which is currently true.
		 */
		if (ic->ic_curmode == IEEE80211_MODE_TURBO_A)
			return IEEE80211_MODE_TURBO_A;
		return IEEE80211_MODE_11A;
	} else if (IEEE80211_IS_CHAN_FHSS(chan))
		return IEEE80211_MODE_FH;
	else if (chan->ic_flags & (IEEE80211_CHAN_OFDM|IEEE80211_CHAN_DYN)) {
		/*
		 * This assumes all 11g channels are also usable
		 * for 11b, which is currently true.
		 */
		if (ic->ic_curmode == IEEE80211_MODE_TURBO_G)
			return IEEE80211_MODE_TURBO_G;
		if (ic->ic_curmode == IEEE80211_MODE_11B)
			return IEEE80211_MODE_11B;
		return IEEE80211_MODE_11G;
	} else
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
		{   2 | IFM_IEEE80211_FH, IFM_IEEE80211_FH1 },
		{   4 | IFM_IEEE80211_FH, IFM_IEEE80211_FH2 },
		{   2 | IFM_IEEE80211_11B, IFM_IEEE80211_DS1 },
		{   4 | IFM_IEEE80211_11B, IFM_IEEE80211_DS2 },
		{  11 | IFM_IEEE80211_11B, IFM_IEEE80211_DS5 },
		{  22 | IFM_IEEE80211_11B, IFM_IEEE80211_DS11 },
		{  44 | IFM_IEEE80211_11B, IFM_IEEE80211_DS22 },
		{  12 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM6 },
		{  18 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM9 },
		{  24 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM12 },
		{  36 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM18 },
		{  48 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM24 },
		{  72 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM36 },
		{  96 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM54 },
		{   2 | IFM_IEEE80211_11G, IFM_IEEE80211_DS1 },
		{   4 | IFM_IEEE80211_11G, IFM_IEEE80211_DS2 },
		{  11 | IFM_IEEE80211_11G, IFM_IEEE80211_DS5 },
		{  22 | IFM_IEEE80211_11G, IFM_IEEE80211_DS11 },
		{  12 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM6 },
		{  18 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM9 },
		{  24 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM12 },
		{  36 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM18 },
		{  48 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM24 },
		{  72 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM36 },
		{  96 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM48 },
		{ 108 | IFM_IEEE80211_11G, IFM_IEEE80211_OFDM54 },
		/* NB: OFDM72 doesn't realy exist so we don't handle it */
	};
	u_int mask, i;

	mask = rate & IEEE80211_RATE_VAL;
	switch (mode) {
	case IEEE80211_MODE_11A:
	case IEEE80211_MODE_TURBO_A:
		mask |= IFM_IEEE80211_11A;
		break;
	case IEEE80211_MODE_11B:
		mask |= IFM_IEEE80211_11B;
		break;
	case IEEE80211_MODE_FH:
		mask |= IFM_IEEE80211_FH;
		break;
	case IEEE80211_MODE_AUTO:
		/* NB: ic may be NULL for some drivers */
		if (ic && ic->ic_phytype == IEEE80211_T_FH) {
			mask |= IFM_IEEE80211_FH;
			break;
		}
		/* NB: hack, 11g matches both 11b+11a rates */
		/* fall thru... */
	case IEEE80211_MODE_11G:
	case IEEE80211_MODE_TURBO_G:
		mask |= IFM_IEEE80211_11G;
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
