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
	"sturboA",	/* IEEE80211_MODE_STURBO_A */
	"11na",		/* IEEE80211_MODE_11NA */
	"11ng",		/* IEEE80211_MODE_11NG */
};

/*
 * Default supported rates for 802.11 operation (in IEEE .5Mb units).
 */
#define	B(r)	((r) | IEEE80211_RATE_BASIC)
static const struct ieee80211_rateset ieee80211_rateset_11a =
	{ 8, { B(12), 18, B(24), 36, B(48), 72, 96, 108 } };
static const struct ieee80211_rateset ieee80211_rateset_half =
	{ 8, { B(6), 9, B(12), 18, B(24), 36, 48, 54 } };
static const struct ieee80211_rateset ieee80211_rateset_quarter =
	{ 8, { B(3), 4, B(6), 9, B(12), 18, 24, 27 } };
static const struct ieee80211_rateset ieee80211_rateset_11b =
	{ 4, { B(2), B(4), B(11), B(22) } };
/* NB: OFDM rates are handled specially based on mode */
static const struct ieee80211_rateset ieee80211_rateset_11g =
	{ 12, { B(2), B(4), B(11), B(22), 12, 18, 24, 36, 48, 72, 96, 108 } };
#undef B

static	int media_status(enum ieee80211_opmode ,
		const struct ieee80211_channel *);

/* list of all instances */
SLIST_HEAD(ieee80211_list, ieee80211com);
static struct ieee80211_list ieee80211_list =
	SLIST_HEAD_INITIALIZER(ieee80211_list);
static uint8_t ieee80211_vapmap[32];		/* enough for 256 */
static struct mtx ieee80211_vap_mtx;
MTX_SYSINIT(ieee80211, &ieee80211_vap_mtx, "net80211 instances", MTX_DEF);

static void
ieee80211_add_vap(struct ieee80211com *ic)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;
	uint8_t b;

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

/*
 * Fill in 802.11 available channel set, mark
 * all available channels as active, and pick
 * a default channel if not already specified.
 */
static void
ieee80211_chan_init(struct ieee80211com *ic)
{
#define	DEFAULTRATES(m, def) do { \
	if (isset(ic->ic_modecaps, m) && ic->ic_sup_rates[m].rs_nrates == 0) \
		ic->ic_sup_rates[m] = def; \
} while (0)
	struct ieee80211_channel *c;
	int i;

	KASSERT(0 < ic->ic_nchans && ic->ic_nchans < IEEE80211_CHAN_MAX,
		("invalid number of channels specified: %u", ic->ic_nchans));
	memset(ic->ic_chan_avail, 0, sizeof(ic->ic_chan_avail));
	setbit(ic->ic_modecaps, IEEE80211_MODE_AUTO);
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		KASSERT(c->ic_flags != 0, ("channel with no flags"));
		KASSERT(c->ic_ieee < IEEE80211_CHAN_MAX,
			("channel with bogus ieee number %u", c->ic_ieee));
		setbit(ic->ic_chan_avail, c->ic_ieee);
		/*
		 * Identify mode capabilities.
		 */
		if (IEEE80211_IS_CHAN_A(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11A);
		if (IEEE80211_IS_CHAN_B(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11B);
		if (IEEE80211_IS_CHAN_ANYG(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11G);
		if (IEEE80211_IS_CHAN_FHSS(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_FH);
		if (IEEE80211_IS_CHAN_108A(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_TURBO_A);
		if (IEEE80211_IS_CHAN_108G(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_TURBO_G);
		if (IEEE80211_IS_CHAN_ST(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_STURBO_A);
		if (IEEE80211_IS_CHAN_HTA(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11NA);
		if (IEEE80211_IS_CHAN_HTG(c))
			setbit(ic->ic_modecaps, IEEE80211_MODE_11NG);
	}
	/* initialize candidate channels to all available */
	memcpy(ic->ic_chan_active, ic->ic_chan_avail,
		sizeof(ic->ic_chan_avail));

	ic->ic_des_chan = IEEE80211_CHAN_ANYC;	/* any channel is ok */
	ic->ic_bsschan = IEEE80211_CHAN_ANYC;
	/* arbitrarily pick the first channel */
	ic->ic_curchan = &ic->ic_channels[0];

	/* fillin well-known rate sets if driver has not specified */
	DEFAULTRATES(IEEE80211_MODE_11B,	 ieee80211_rateset_11b);
	DEFAULTRATES(IEEE80211_MODE_11G,	 ieee80211_rateset_11g);
	DEFAULTRATES(IEEE80211_MODE_11A,	 ieee80211_rateset_11a);
	DEFAULTRATES(IEEE80211_MODE_TURBO_A,	 ieee80211_rateset_11a);
	DEFAULTRATES(IEEE80211_MODE_TURBO_G,	 ieee80211_rateset_11g);

	/*
	 * Set auto mode to reset active channel state and any desired channel.
	 */
	(void) ieee80211_setmode(ic, IEEE80211_MODE_AUTO);
#undef DEFAULTRATES
}

void
ieee80211_ifattach(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;

	ether_ifattach(ifp, ic->ic_myaddr);
	ifp->if_output = ieee80211_output;

	bpfattach2(ifp, DLT_IEEE802_11,
	    sizeof(struct ieee80211_frame_addr4), &ic->ic_rawbpf);

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

	/*
	 * Fill in 802.11 available channel set, mark all
	 * available channels as active, and pick a default
	 * channel if not already specified.
	 */
	ieee80211_chan_init(ic);

	if (ic->ic_caps & IEEE80211_C_BGSCAN)	/* enable if capable */
		ic->ic_flags |= IEEE80211_F_BGSCAN;
#if 0
	/* XXX not until WME+WPA issues resolved */
	if (ic->ic_caps & IEEE80211_C_WME)	/* enable if capable */
		ic->ic_flags |= IEEE80211_F_WME;
#endif
	if (ic->ic_caps & IEEE80211_C_BURST)
		ic->ic_flags |= IEEE80211_F_BURST;
	ic->ic_flags |= IEEE80211_F_DOTH;	/* XXX out of caps, just ena */

	ic->ic_bintval = IEEE80211_BINTVAL_DEFAULT;
	ic->ic_bmissthreshold = IEEE80211_HWBMISS_DEFAULT;
	ic->ic_dtim_period = IEEE80211_DTIM_DEFAULT;
	IEEE80211_LOCK_INIT(ic, "ieee80211com");
	IEEE80211_BEACON_LOCK_INIT(ic, "beacon");

	ic->ic_lintval = ic->ic_bintval;
	ic->ic_txpowlimit = IEEE80211_TXPOWER_MAX;

	ieee80211_crypto_attach(ic);
	ieee80211_node_attach(ic);
	ieee80211_power_attach(ic);
	ieee80211_proto_attach(ic);
	ieee80211_ht_attach(ic);
	ieee80211_scan_attach(ic);

	ieee80211_add_vap(ic);

	ieee80211_sysctl_attach(ic);		/* NB: requires ic_vap */

	/*
	 * Install a default reset method for the ioctl support.
	 * The driver is expected to fill this in before calling us.
	 */
	if (ic->ic_reset == NULL)
		ic->ic_reset = ieee80211_default_reset;

	KASSERT(ifp->if_spare2 == NULL, ("oops, hosed"));
	ifp->if_spare2 = ic;			/* XXX temp backpointer */
}

void
ieee80211_ifdetach(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;

	ieee80211_remove_vap(ic);

	ieee80211_sysctl_detach(ic);
	ieee80211_scan_detach(ic);
	ieee80211_ht_detach(ic);
	/* NB: must be called before ieee80211_node_detach */
	ieee80211_proto_detach(ic);
	ieee80211_crypto_detach(ic);
	ieee80211_power_detach(ic);
	ieee80211_node_detach(ic);
	ifmedia_removeall(&ic->ic_media);

	IEEE80211_LOCK_DESTROY(ic);
	IEEE80211_BEACON_LOCK_DESTROY(ic);

	bpfdetach(ifp);
	ether_ifdetach(ifp);
}

static __inline int
mapgsm(u_int freq, u_int flags)
{
	freq *= 10;
	if (flags & IEEE80211_CHAN_QUARTER)
		freq += 5;
	else if (flags & IEEE80211_CHAN_HALF)
		freq += 10;
	else
		freq += 20;
	/* NB: there is no 907/20 wide but leave room */
	return (freq - 906*10) / 5;
}

static __inline int
mappsb(u_int freq, u_int flags)
{
	return 37 + ((freq * 10) + ((freq % 5) == 2 ? 5 : 0) - 49400) / 5;
}

/*
 * Convert MHz frequency to IEEE channel number.
 */
int
ieee80211_mhz2ieee(u_int freq, u_int flags)
{
#define	IS_FREQ_IN_PSB(_freq) ((_freq) > 4940 && (_freq) < 4990)
	if (flags & IEEE80211_CHAN_GSM)
		return mapgsm(freq, flags);
	if (flags & IEEE80211_CHAN_2GHZ) {	/* 2GHz band */
		if (freq == 2484)
			return 14;
		if (freq < 2484)
			return ((int) freq - 2407) / 5;
		else
			return 15 + ((freq - 2512) / 20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {	/* 5Ghz band */
		if (freq <= 5000) {
			/* XXX check regdomain? */
			if (IS_FREQ_IN_PSB(freq))
				return mappsb(freq, flags);
			return (freq - 4000) / 5;
		} else
			return (freq - 5000) / 5;
	} else {				/* either, guess */
		if (freq == 2484)
			return 14;
		if (freq < 2484) {
			if (907 <= freq && freq <= 922)
				return mapgsm(freq, flags);
			return ((int) freq - 2407) / 5;
		}
		if (freq < 5000) {
			if (IS_FREQ_IN_PSB(freq))
				return mappsb(freq, flags);
			else if (freq > 4900)
				return (freq - 4000) / 5;
			else
				return 15 + ((freq - 2512) / 20);
		}
		return (freq - 5000) / 5;
	}
#undef IS_FREQ_IN_PSB
}

/*
 * Convert channel to IEEE channel number.
 */
int
ieee80211_chan2ieee(struct ieee80211com *ic, const struct ieee80211_channel *c)
{
	if (c == NULL) {
		if_printf(ic->ic_ifp, "invalid channel (NULL)\n");
		return 0;		/* XXX */
	}
	return (c == IEEE80211_CHAN_ANYC ?  IEEE80211_CHAN_ANY : c->ic_ieee);
}

/*
 * Convert IEEE channel number to MHz frequency.
 */
u_int
ieee80211_ieee2mhz(u_int chan, u_int flags)
{
	if (flags & IEEE80211_CHAN_GSM)
		return 907 + 5 * (chan / 10);
	if (flags & IEEE80211_CHAN_2GHZ) {	/* 2GHz band */
		if (chan == 14)
			return 2484;
		if (chan < 14)
			return 2407 + chan*5;
		else
			return 2512 + ((chan-15)*20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {/* 5Ghz band */
		if (flags & (IEEE80211_CHAN_HALF|IEEE80211_CHAN_QUARTER)) {
			chan -= 37;
			return 4940 + chan*5 + (chan % 5 ? 2 : 0);
		}
		return 5000 + (chan*5);
	} else {				/* either, guess */
		/* XXX can't distinguish PSB+GSM channels */
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
 * Locate a channel given a frequency+flags.  We cache
 * the previous lookup to optimize swithing between two
 * channels--as happens with dynamic turbo.
 */
struct ieee80211_channel *
ieee80211_find_channel(struct ieee80211com *ic, int freq, int flags)
{
	struct ieee80211_channel *c;
	int i;

	flags &= IEEE80211_CHAN_ALLTURBO;
	c = ic->ic_prevchan;
	if (c != NULL && c->ic_freq == freq &&
	    (c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)
		return c;
	/* brute force search */
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (c->ic_freq == freq &&
		    (c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)
			return c;
	}
	return NULL;
}

static void
addmedia(struct ieee80211com *ic, int mode, int mword)
{
#define	TURBO(m)	((m) | IFM_IEEE80211_TURBO)
#define	ADD(_ic, _s, _o) \
	ifmedia_add(&(_ic)->ic_media, \
		IFM_MAKEWORD(IFM_IEEE80211, (_s), (_o), 0), 0, NULL)
	static const u_int mopts[IEEE80211_MODE_MAX] = { 
		IFM_AUTO,			/* IEEE80211_MODE_AUTO */
		IFM_IEEE80211_11A,		/* IEEE80211_MODE_11A */
		IFM_IEEE80211_11B,		/* IEEE80211_MODE_11B */
		IFM_IEEE80211_11G,		/* IEEE80211_MODE_11G */
		IFM_IEEE80211_FH,		/* IEEE80211_MODE_FH */
		TURBO(IFM_IEEE80211_11A),	/* IEEE80211_MODE_TURBO_A */
		TURBO(IFM_IEEE80211_11G),	/* IEEE80211_MODE_TURBO_G */
		TURBO(IFM_IEEE80211_11A),	/* IEEE80211_MODE_STURBO_A */
		IFM_IEEE80211_11NA,		/* IEEE80211_MODE_11NA */
		IFM_IEEE80211_11NG,		/* IEEE80211_MODE_11NG */
	};
	u_int mopt;

	KASSERT(mode < IEEE80211_MODE_MAX, ("bad mode %u", mode));
	mopt = mopts[mode];
	KASSERT(mopt != 0 || mode == IEEE80211_MODE_AUTO,
	    ("no media mapping for mode %u", mode));

	ADD(ic, mword, mopt);	/* e.g. 11a auto */
	if (ic->ic_caps & IEEE80211_C_IBSS)
		ADD(ic, mword, mopt | IFM_IEEE80211_ADHOC);
	if (ic->ic_caps & IEEE80211_C_HOSTAP)
		ADD(ic, mword, mopt | IFM_IEEE80211_HOSTAP);
	if (ic->ic_caps & IEEE80211_C_AHDEMO)
		ADD(ic, mword, mopt | IFM_IEEE80211_ADHOC | IFM_FLAG0);
	if (ic->ic_caps & IEEE80211_C_MONITOR)
		ADD(ic, mword, mopt | IFM_IEEE80211_MONITOR);
#undef ADD
#undef TURBO
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
	struct ifnet *ifp = ic->ic_ifp;
	int i, j, mode, rate, maxrate, mword, r;
	const struct ieee80211_rateset *rs;
	struct ieee80211_rateset allrates;

	/* NB: this works because the structure is initialized to zero */
	if (LIST_EMPTY(&ic->ic_media.ifm_list)) {
		/*
		 * Do late attach work that must wait for any subclass
		 * (i.e. driver) work such as overriding methods.
		 */
		ieee80211_node_lateattach(ic);
	} else {
		/*
		 * We are re-initializing the channel list; clear
		 * the existing media state as the media routines
		 * don't suppress duplicates.
		 */
		ifmedia_removeall(&ic->ic_media);
		ieee80211_chan_init(ic);
	}
	ieee80211_power_lateattach(ic);

	/*
	 * Fill in media characteristics.
	 */
	ifmedia_init(&ic->ic_media, 0, media_change, media_stat);
	maxrate = 0;
	/*
	 * Add media for legacy operating modes.
	 */
	memset(&allrates, 0, sizeof(allrates));
	for (mode = IEEE80211_MODE_AUTO; mode < IEEE80211_MODE_11NA; mode++) {
		if (isclr(ic->ic_modecaps, mode))
			continue;
		addmedia(ic, mode, IFM_AUTO);
		if (mode == IEEE80211_MODE_AUTO)
			continue;
		rs = &ic->ic_sup_rates[mode];
		for (i = 0; i < rs->rs_nrates; i++) {
			rate = rs->rs_rates[i];
			mword = ieee80211_rate2media(ic, rate, mode);
			if (mword == 0)
				continue;
			addmedia(ic, mode, mword);
			/*
			 * Add legacy rate to the collection of all rates.
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
		/* NB: remove media options from mword */
		addmedia(ic, IEEE80211_MODE_AUTO, IFM_SUBTYPE(mword));
	}
	/*
	 * Add HT/11n media.  Note that we do not have enough
	 * bits in the media subtype to express the MCS so we
	 * use a "placeholder" media subtype and any fixed MCS
	 * must be specified with a different mechanism.
	 */
	for (; mode < IEEE80211_MODE_MAX; mode++) {
		if (isclr(ic->ic_modecaps, mode))
			continue;
		addmedia(ic, mode, IFM_AUTO);
		addmedia(ic, mode, IFM_IEEE80211_MCS);
	}
	if (isset(ic->ic_modecaps, IEEE80211_MODE_11NA) ||
	    isset(ic->ic_modecaps, IEEE80211_MODE_11NG)) {
		addmedia(ic, IEEE80211_MODE_AUTO, IFM_IEEE80211_MCS);
		/* XXX could walk htrates */
		/* XXX known array size */
		if (ieee80211_htrates[15] > maxrate)
			maxrate = ieee80211_htrates[15];
	}

	/* NB: strip explicit mode; we're actually in autoselect */
	ifmedia_set(&ic->ic_media,
		media_status(ic->ic_opmode, ic->ic_curchan) &~ IFM_MMASK);

	if (maxrate)
		ifp->if_baudrate = IF_Mbps(maxrate);
}

const struct ieee80211_rateset *
ieee80211_get_suprates(struct ieee80211com *ic, const struct ieee80211_channel *c)
{
	if (IEEE80211_IS_CHAN_HALF(c))
		return &ieee80211_rateset_half;
	if (IEEE80211_IS_CHAN_QUARTER(c))
		return &ieee80211_rateset_quarter;
	if (IEEE80211_IS_CHAN_HTA(c))
		return &ic->ic_sup_rates[IEEE80211_MODE_11A];
	if (IEEE80211_IS_CHAN_HTG(c)) {
		/* XXX does this work for basic rates? */
		return &ic->ic_sup_rates[IEEE80211_MODE_11G];
	}
	return &ic->ic_sup_rates[ieee80211_chan2mode(c)];
}

void
ieee80211_announce(struct ieee80211com *ic)
{
	struct ifnet *ifp = ic->ic_ifp;
	int i, mode, rate, mword;
	const struct ieee80211_rateset *rs;

	/* NB: skip AUTO since it has no rates */
	for (mode = IEEE80211_MODE_AUTO+1; mode < IEEE80211_MODE_11NA; mode++) {
		if (isclr(ic->ic_modecaps, mode))
			continue;
		if_printf(ifp, "%s rates: ", ieee80211_phymode_name[mode]);
		rs = &ic->ic_sup_rates[mode];
		for (i = 0; i < rs->rs_nrates; i++) {
			mword = ieee80211_rate2media(ic, rs->rs_rates[i], mode);
			if (mword == 0)
				continue;
			rate = ieee80211_media2rate(mword);
			printf("%s%d%sMbps", (i != 0 ? " " : ""),
			    rate / 2, ((rate & 0x1) != 0 ? ".5" : ""));
		}
		printf("\n");
	}
	ieee80211_ht_announce(ic);
}

void
ieee80211_announce_channels(struct ieee80211com *ic)
{
	const struct ieee80211_channel *c;
	char type;
	int i, cw;

	printf("Chan  Freq  CW  RegPwr  MinPwr  MaxPwr\n");
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];
		if (IEEE80211_IS_CHAN_ST(c))
			type = 'S';
		else if (IEEE80211_IS_CHAN_108A(c))
			type = 'T';
		else if (IEEE80211_IS_CHAN_108G(c))
			type = 'G';
		else if (IEEE80211_IS_CHAN_HT(c))
			type = 'n';
		else if (IEEE80211_IS_CHAN_A(c))
			type = 'a';
		else if (IEEE80211_IS_CHAN_ANYG(c))
			type = 'g';
		else if (IEEE80211_IS_CHAN_B(c))
			type = 'b';
		else
			type = 'f';
		if (IEEE80211_IS_CHAN_HT40(c) || IEEE80211_IS_CHAN_TURBO(c))
			cw = 40;
		else if (IEEE80211_IS_CHAN_HALF(c))
			cw = 10;
		else if (IEEE80211_IS_CHAN_QUARTER(c))
			cw = 5;
		else
			cw = 20;
		printf("%4d  %4d%c %2d%c %6d  %4d.%d  %4d.%d\n"
			, c->ic_ieee, c->ic_freq, type
			, cw
			, IEEE80211_IS_CHAN_HT40U(c) ? '+' :
			  IEEE80211_IS_CHAN_HT40D(c) ? '-' : ' '
			, c->ic_maxregpower
			, c->ic_minpower / 2, c->ic_minpower & 1 ? 5 : 0
			, c->ic_maxpower / 2, c->ic_maxpower & 1 ? 5 : 0
		);
	}
}

/*
 * Find an instance by it's mac address.
 */
struct ieee80211com *
ieee80211_find_vap(const uint8_t mac[IEEE80211_ADDR_LEN])
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
 * Convert a media specification to a rate index and possibly a mode
 * (if the rate is fixed and the mode is specified as ``auto'' then
 * we need to lock down the mode so the index is meanginful).
 */
static int
checkrate(struct ieee80211com *ic, enum ieee80211_phymode mode, int rate)
{

	/*
	 * Check the rate table for the specified/current phy.
	 */
	if (mode == IEEE80211_MODE_AUTO) {
		int i;
		/*
		 * In autoselect mode search for the rate.
		 */
		for (i = IEEE80211_MODE_11A; i < IEEE80211_MODE_MAX; i++) {
			if (isset(ic->ic_modecaps, i) &&
			    findrate(ic, i, rate) != -1)
				return 1;
		}
		return 0;
	} else {
		/*
		 * Mode is fixed, check for rate.
		 */
		return (findrate(ic, mode, rate) != -1);
	}
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
	int newrate, error = 0;

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
	case IFM_IEEE80211_11NA:
		newphymode = IEEE80211_MODE_11NA;
		break;
	case IFM_IEEE80211_11NG:
		newphymode = IEEE80211_MODE_11NG;
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
		if (newphymode == IEEE80211_MODE_11A) {
			if (ic->ic_flags & IEEE80211_F_TURBOP)
				newphymode = IEEE80211_MODE_TURBO_A;
			else
				newphymode = IEEE80211_MODE_STURBO_A;
		} else if (newphymode == IEEE80211_MODE_11G)
			newphymode = IEEE80211_MODE_TURBO_G;
		else
			return EINVAL;
	}
	/* XXX HT40 +/- */
	/*
	 * Next, the fixed/variable rate.
	 */
	newrate = ic->ic_fixed_rate;
	if (IFM_SUBTYPE(ime->ifm_media) != IFM_AUTO) {
		/*
		 * Convert media subtype to rate.
		 */
		newrate = ieee80211_media2rate(ime->ifm_media);
		if (newrate == 0 || !checkrate(ic, newphymode, newrate))
			return EINVAL;
	} else
		newrate = IEEE80211_FIXED_RATE_NONE;

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
	 * Handle phy mode change.
	 */
	if (ic->ic_des_mode != newphymode) {		/* change phy mode */
		ic->ic_des_mode = newphymode;
		error = ENETRESET;
	}

	/*
	 * Committed to changes, install the rate setting.
	 */
	if (ic->ic_fixed_rate != newrate) {
		ic->ic_fixed_rate = newrate;		/* set fixed tx rate */
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
		case IEEE80211_M_WDS:
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

/*
 * Common code to calculate the media status word
 * from the operating mode and channel state.
 */
static int
media_status(enum ieee80211_opmode opmode, const struct ieee80211_channel *chan)
{
	int status;

	status = IFM_IEEE80211;
	switch (opmode) {
	case IEEE80211_M_STA:
		break;
	case IEEE80211_M_IBSS:
		status |= IFM_IEEE80211_ADHOC;
		break;
	case IEEE80211_M_HOSTAP:
		status |= IFM_IEEE80211_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		status |= IFM_IEEE80211_MONITOR;
		break;
	case IEEE80211_M_AHDEMO:
		status |= IFM_IEEE80211_ADHOC | IFM_FLAG0;
		break;
	case IEEE80211_M_WDS:
		/* should not come here */
		break;
	}
	if (IEEE80211_IS_CHAN_HTA(chan)) {
		status |= IFM_IEEE80211_11NA;
	} else if (IEEE80211_IS_CHAN_HTG(chan)) {
		status |= IFM_IEEE80211_11NG;
	} else if (IEEE80211_IS_CHAN_A(chan)) {
		status |= IFM_IEEE80211_11A;
	} else if (IEEE80211_IS_CHAN_B(chan)) {
		status |= IFM_IEEE80211_11B;
	} else if (IEEE80211_IS_CHAN_ANYG(chan)) {
		status |= IFM_IEEE80211_11G;
	} else if (IEEE80211_IS_CHAN_FHSS(chan)) {
		status |= IFM_IEEE80211_FH;
	}
	/* XXX else complain? */

	if (IEEE80211_IS_CHAN_TURBO(chan))
		status |= IFM_IEEE80211_TURBO;

	return status;
}

void
ieee80211_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct ieee80211com *ic;
	enum ieee80211_phymode mode;
	const struct ieee80211_rateset *rs;

	ic = ieee80211_find_instance(ifp);
	if (!ic) {
		if_printf(ifp, "%s: no 802.11 instance!\n", __func__);
		return;
	}
	imr->ifm_status = IFM_AVALID;
	/*
	 * NB: use the current channel's mode to lock down a xmit
	 * rate only when running; otherwise we may have a mismatch
	 * in which case the rate will not be convertible.
	 */
	if (ic->ic_state == IEEE80211_S_RUN) {
		imr->ifm_status |= IFM_ACTIVE;
		mode = ieee80211_chan2mode(ic->ic_curchan);
	} else
		mode = IEEE80211_MODE_AUTO;
	imr->ifm_active = media_status(ic->ic_opmode, ic->ic_curchan);
	/*
	 * Calculate a current rate if possible.
	 */
	if (ic->ic_fixed_rate != IEEE80211_FIXED_RATE_NONE) {
		/*
		 * A fixed rate is set, report that.
		 */
		imr->ifm_active |= ieee80211_rate2media(ic,
			ic->ic_fixed_rate, mode);
	} else if (ic->ic_opmode == IEEE80211_M_STA) {
		/*
		 * In station mode report the current transmit rate.
		 * XXX HT rate
		 */
		rs = &ic->ic_bss->ni_rates;
		imr->ifm_active |= ieee80211_rate2media(ic,
			rs->rs_rates[ic->ic_bss->ni_txrate], mode);
	} else
		imr->ifm_active |= IFM_AUTO;
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
	/*
	 * Adjust basic rates in 11b/11g supported rate set.
	 * Note that if operating on a hal/quarter rate channel
	 * this is a noop as those rates sets are different
	 * and used instead.
	 */
	if (mode == IEEE80211_MODE_11G || mode == IEEE80211_MODE_11B)
		ieee80211_set11gbasicrates(&ic->ic_sup_rates[mode], mode);

	ic->ic_curmode = mode;
	ieee80211_reset_erp(ic);	/* reset ERP state */
	ieee80211_wme_initparams(ic);	/* reset WME stat */

	return 0;
}

/*
 * Return the phy mode for with the specified channel.
 */
enum ieee80211_phymode
ieee80211_chan2mode(const struct ieee80211_channel *chan)
{

	if (IEEE80211_IS_CHAN_HTA(chan))
		return IEEE80211_MODE_11NA;
	else if (IEEE80211_IS_CHAN_HTG(chan))
		return IEEE80211_MODE_11NG;
	else if (IEEE80211_IS_CHAN_108G(chan))
		return IEEE80211_MODE_TURBO_G;
	else if (IEEE80211_IS_CHAN_ST(chan))
		return IEEE80211_MODE_STURBO_A;
	else if (IEEE80211_IS_CHAN_TURBO(chan))
		return IEEE80211_MODE_TURBO_A;
	else if (IEEE80211_IS_CHAN_A(chan))
		return IEEE80211_MODE_11A;
	else if (IEEE80211_IS_CHAN_ANYG(chan))
		return IEEE80211_MODE_11G;
	else if (IEEE80211_IS_CHAN_B(chan))
		return IEEE80211_MODE_11B;
	else if (IEEE80211_IS_CHAN_FHSS(chan))
		return IEEE80211_MODE_FH;

	/* NB: should not get here */
	printf("%s: cannot map channel to mode; freq %u flags 0x%x\n",
		__func__, chan->ic_freq, chan->ic_flags);
	return IEEE80211_MODE_11B;
}

struct ratemedia {
	u_int	match;	/* rate + mode */
	u_int	media;	/* if_media rate */
};

static int
findmedia(const struct ratemedia rates[], int n, u_int match)
{
	int i;

	for (i = 0; i < n; i++)
		if (rates[i].match == match)
			return rates[i].media;
	return IFM_AUTO;
}

/*
 * Convert IEEE80211 rate value to ifmedia subtype.
 * Rate is either a legacy rate in units of 0.5Mbps
 * or an MCS index.
 */
int
ieee80211_rate2media(struct ieee80211com *ic, int rate, enum ieee80211_phymode mode)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const struct ratemedia rates[] = {
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
		{   6 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM3 },
		{   9 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM4 },
		{  54 | IFM_IEEE80211_11A, IFM_IEEE80211_OFDM27 },
		/* NB: OFDM72 doesn't realy exist so we don't handle it */
	};
	static const struct ratemedia htrates[] = {
		{   0, IFM_IEEE80211_MCS },
		{   1, IFM_IEEE80211_MCS },
		{   2, IFM_IEEE80211_MCS },
		{   3, IFM_IEEE80211_MCS },
		{   4, IFM_IEEE80211_MCS },
		{   5, IFM_IEEE80211_MCS },
		{   6, IFM_IEEE80211_MCS },
		{   7, IFM_IEEE80211_MCS },
		{   8, IFM_IEEE80211_MCS },
		{   9, IFM_IEEE80211_MCS },
		{  10, IFM_IEEE80211_MCS },
		{  11, IFM_IEEE80211_MCS },
		{  12, IFM_IEEE80211_MCS },
		{  13, IFM_IEEE80211_MCS },
		{  14, IFM_IEEE80211_MCS },
		{  15, IFM_IEEE80211_MCS },
	};
	int m;

	/*
	 * Check 11n rates first for match as an MCS.
	 */
	if (mode == IEEE80211_MODE_11NA) {
		if (rate & IEEE80211_RATE_MCS) {
			rate &= ~IEEE80211_RATE_MCS;
			m = findmedia(htrates, N(htrates), rate);
			if (m != IFM_AUTO)
				return m | IFM_IEEE80211_11NA;
		}
	} else if (mode == IEEE80211_MODE_11NG) {
		/* NB: 12 is ambiguous, it will be treated as an MCS */
		if (rate & IEEE80211_RATE_MCS) {
			rate &= ~IEEE80211_RATE_MCS;
			m = findmedia(htrates, N(htrates), rate);
			if (m != IFM_AUTO)
				return m | IFM_IEEE80211_11NG;
		}
	}
	rate &= IEEE80211_RATE_VAL;
	switch (mode) {
	case IEEE80211_MODE_11A:
	case IEEE80211_MODE_11NA:
	case IEEE80211_MODE_TURBO_A:
	case IEEE80211_MODE_STURBO_A:
		return findmedia(rates, N(rates), rate | IFM_IEEE80211_11A);
	case IEEE80211_MODE_11B:
		return findmedia(rates, N(rates), rate | IFM_IEEE80211_11B);
	case IEEE80211_MODE_FH:
		return findmedia(rates, N(rates), rate | IFM_IEEE80211_FH);
	case IEEE80211_MODE_AUTO:
		/* NB: ic may be NULL for some drivers */
		if (ic && ic->ic_phytype == IEEE80211_T_FH)
			return findmedia(rates, N(rates),
			    rate | IFM_IEEE80211_FH);
		/* NB: hack, 11g matches both 11b+11a rates */
		/* fall thru... */
	case IEEE80211_MODE_11G:
	case IEEE80211_MODE_11NG:
	case IEEE80211_MODE_TURBO_G:
		return findmedia(rates, N(rates), rate | IFM_IEEE80211_11G);
	}
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
		0,		/* IFM_IEEE80211_DS354k */
		0,		/* IFM_IEEE80211_DS512k */
		6,		/* IFM_IEEE80211_OFDM3 */
		9,		/* IFM_IEEE80211_OFDM4 */
		54,		/* IFM_IEEE80211_OFDM27 */
		-1,		/* IFM_IEEE80211_MCS */
	};
	return IFM_SUBTYPE(mword) < N(ieeerates) ?
		ieeerates[IFM_SUBTYPE(mword)] : 0;
#undef N
}
