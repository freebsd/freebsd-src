/*-
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
 * IEEE 802.11 station scanning support.
 */
#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/kernel.h>
#include <sys/module.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

/*
 * Parameters for managing cache entries:
 *
 * o a station with STA_FAILS_MAX failures is not considered
 *   when picking a candidate
 * o a station that hasn't had an update in STA_PURGE_SCANS
 *   (background) scans is discarded
 * o after STA_FAILS_AGE seconds we clear the failure count
 */
#define	STA_FAILS_MAX	2		/* assoc failures before ignored */
#define	STA_FAILS_AGE	(2*60)		/* time before clearing fails (secs) */
#define	STA_PURGE_SCANS	2		/* age for purging entries (scans) */

/* XXX tunable */
#define	STA_RSSI_MIN	8		/* min acceptable rssi */
#define	STA_RSSI_MAX	40		/* max rssi for comparison */

#define RSSI_LPF_LEN		10
#define	RSSI_DUMMY_MARKER	0x127
#define	RSSI_EP_MULTIPLIER	(1<<7)	/* pow2 to optimize out * and / */
#define RSSI_IN(x)		((x) * RSSI_EP_MULTIPLIER)
#define LPF_RSSI(x, y, len) \
    ((x != RSSI_DUMMY_MARKER) ? (((x) * ((len) - 1) + (y)) / (len)) : (y))
#define RSSI_LPF(x, y) do {						\
    if ((y) >= -20)							\
    	x = LPF_RSSI((x), RSSI_IN((y)), RSSI_LPF_LEN);			\
} while (0)
#define	EP_RND(x, mul) \
	((((x)%(mul)) >= ((mul)/2)) ? howmany(x, mul) : (x)/(mul))
#define	RSSI_GET(x)	EP_RND(x, RSSI_EP_MULTIPLIER)

struct sta_entry {
	struct ieee80211_scan_entry base;
	TAILQ_ENTRY(sta_entry) se_list;
	LIST_ENTRY(sta_entry) se_hash;
	uint8_t		se_fails;		/* failure to associate count */
	uint8_t		se_seen;		/* seen during current scan */
	uint8_t		se_notseen;		/* not seen in previous scans */
	uint8_t		se_flags;
	uint32_t	se_avgrssi;		/* LPF rssi state */
	unsigned long	se_lastupdate;		/* time of last update */
	unsigned long	se_lastfail;		/* time of last failure */
	unsigned long	se_lastassoc;		/* time of last association */
	u_int		se_scangen;		/* iterator scan gen# */
};

#define	STA_HASHSIZE	32
/* simple hash is enough for variation of macaddr */
#define	STA_HASH(addr)	\
	(((const uint8_t *)(addr))[IEEE80211_ADDR_LEN - 1] % STA_HASHSIZE)

struct sta_table {
	struct mtx	st_lock;		/* on scan table */
	TAILQ_HEAD(, sta_entry) st_entry;	/* all entries */
	LIST_HEAD(, sta_entry) st_hash[STA_HASHSIZE];
	struct mtx	st_scanlock;		/* on st_scangen */
	u_int		st_scangen;		/* gen# for iterator */
	int		st_newscan;
};

static void sta_flush_table(struct sta_table *);
/*
 * match_bss returns a bitmask describing if an entry is suitable
 * for use.  If non-zero the entry was deemed not suitable and it's
 * contents explains why.  The following flags are or'd to to this
 * mask and can be used to figure out why the entry was rejected.
 */
#define	MATCH_CHANNEL	0x001	/* channel mismatch */
#define	MATCH_CAPINFO	0x002	/* capabilities mismatch, e.g. no ess */
#define	MATCH_PRIVACY	0x004	/* privacy mismatch */
#define	MATCH_RATE	0x008	/* rate set mismatch */
#define	MATCH_SSID	0x010	/* ssid mismatch */
#define	MATCH_BSSID	0x020	/* bssid mismatch */
#define	MATCH_FAILS	0x040	/* too many failed auth attempts */
#define	MATCH_NOTSEEN	0x080	/* not seen in recent scans */
#define	MATCH_RSSI	0x100	/* rssi deemed too low to use */
static int match_bss(struct ieee80211com *,
	const struct ieee80211_scan_state *, struct sta_entry *, int);

/* number of references from net80211 layer */
static	int nrefs = 0;

/*
 * Attach prior to any scanning work.
 */
static int
sta_attach(struct ieee80211_scan_state *ss)
{
	struct sta_table *st;

	MALLOC(st, struct sta_table *, sizeof(struct sta_table),
		M_80211_SCAN, M_NOWAIT | M_ZERO);
	if (st == NULL)
		return 0;
	mtx_init(&st->st_lock, "scantable", "802.11 scan table", MTX_DEF);
	mtx_init(&st->st_scanlock, "scangen", "802.11 scangen", MTX_DEF);
	TAILQ_INIT(&st->st_entry);
	ss->ss_priv = st;
	nrefs++;			/* NB: we assume caller locking */
	return 1;
}

/*
 * Cleanup any private state.
 */
static int
sta_detach(struct ieee80211_scan_state *ss)
{
	struct sta_table *st = ss->ss_priv;

	if (st != NULL) {
		sta_flush_table(st);
		mtx_destroy(&st->st_lock);
		mtx_destroy(&st->st_scanlock);
		FREE(st, M_80211_SCAN);
		KASSERT(nrefs > 0, ("imbalanced attach/detach"));
		nrefs--;		/* NB: we assume caller locking */
	}
	return 1;
}

/*
 * Flush all per-scan state.
 */
static int
sta_flush(struct ieee80211_scan_state *ss)
{
	struct sta_table *st = ss->ss_priv;

	mtx_lock(&st->st_lock);
	sta_flush_table(st);
	mtx_unlock(&st->st_lock);
	ss->ss_last = 0;
	return 0;
}

/*
 * Flush all entries in the scan cache.
 */
static void
sta_flush_table(struct sta_table *st)
{
	struct sta_entry *se, *next;

	TAILQ_FOREACH_SAFE(se, &st->st_entry, se_list, next) {
		TAILQ_REMOVE(&st->st_entry, se, se_list);
		LIST_REMOVE(se, se_hash);
		FREE(se, M_80211_SCAN);
	}
}

static void
saveie(uint8_t **iep, const uint8_t *ie)
{

	if (ie == NULL)
		*iep = NULL;
	else
		ieee80211_saveie(iep, ie);
}

/*
 * Process a beacon or probe response frame; create an
 * entry in the scan cache or update any previous entry.
 */
static int
sta_add(struct ieee80211_scan_state *ss, 
	const struct ieee80211_scanparams *sp,
	const struct ieee80211_frame *wh,
	int subtype, int rssi, int noise, int rstamp)
{
#define	ISPROBE(_st)	((_st) == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
#define	PICK1ST(_ss) \
	((ss->ss_flags & (IEEE80211_SCAN_PICK1ST | IEEE80211_SCAN_GOTPICK)) == \
	IEEE80211_SCAN_PICK1ST)
	struct sta_table *st = ss->ss_priv;
	const uint8_t *macaddr = wh->i_addr2;
	struct ieee80211com *ic = ss->ss_ic;
	struct sta_entry *se;
	struct ieee80211_scan_entry *ise;
	int hash, offchan;

	hash = STA_HASH(macaddr);

	mtx_lock(&st->st_lock);
	LIST_FOREACH(se, &st->st_hash[hash], se_hash)
		if (IEEE80211_ADDR_EQ(se->base.se_macaddr, macaddr))
			goto found;
	MALLOC(se, struct sta_entry *, sizeof(struct sta_entry),
		M_80211_SCAN, M_NOWAIT | M_ZERO);
	if (se == NULL) {
		mtx_unlock(&st->st_lock);
		return 0;
	}
	se->se_scangen = st->st_scangen-1;
	se->se_avgrssi = RSSI_DUMMY_MARKER;
	IEEE80211_ADDR_COPY(se->base.se_macaddr, macaddr);
	TAILQ_INSERT_TAIL(&st->st_entry, se, se_list);
	LIST_INSERT_HEAD(&st->st_hash[hash], se, se_hash);
found:
	ise = &se->base;
	/* XXX ap beaconing multiple ssid w/ same bssid */
	if (sp->ssid[1] != 0 &&
	    (ISPROBE(subtype) || ise->se_ssid[1] == 0))
		memcpy(ise->se_ssid, sp->ssid, 2+sp->ssid[1]);
	KASSERT(sp->rates[1] <= IEEE80211_RATE_MAXSIZE,
		("rate set too large: %u", sp->rates[1]));
	memcpy(ise->se_rates, sp->rates, 2+sp->rates[1]);
	if (sp->xrates != NULL) {
		/* XXX validate xrates[1] */
		KASSERT(sp->xrates[1] + sp->rates[1] <= IEEE80211_RATE_MAXSIZE,
			("xrate set too large: %u", sp->xrates[1]));
		memcpy(ise->se_xrates, sp->xrates, 2+sp->xrates[1]);
	} else
		ise->se_xrates[1] = 0;
	IEEE80211_ADDR_COPY(ise->se_bssid, wh->i_addr3);
	offchan = (IEEE80211_CHAN2IEEE(sp->curchan) != sp->bchan &&
	    ic->ic_phytype != IEEE80211_T_FH);
	if (!offchan) {
		/*
		 * Record rssi data using extended precision LPF filter.
		 *
		 * NB: use only on-channel data to insure we get a good
		 *     estimate of the signal we'll see when associated.
		 */
		RSSI_LPF(se->se_avgrssi, rssi);
		ise->se_rssi = RSSI_GET(se->se_avgrssi);
		ise->se_noise = noise;
	}
	ise->se_rstamp = rstamp;
	memcpy(ise->se_tstamp.data, sp->tstamp, sizeof(ise->se_tstamp));
	ise->se_intval = sp->bintval;
	ise->se_capinfo = sp->capinfo;
	/*
	 * Beware of overriding se_chan for frames seen
	 * off-channel; this can cause us to attempt an
	 * assocation on the wrong channel.
	 */
	if (offchan) {
		struct ieee80211_channel *c;
		/*
		 * Off-channel, locate the home/bss channel for the sta
		 * using the value broadcast in the DSPARMS ie.
		 */
		c = ieee80211_find_channel_byieee(ic, sp->bchan,
		    sp->curchan->ic_flags);
		if (c != NULL) {
			ise->se_chan = c;
		} else if (ise->se_chan == NULL) {
			/* should not happen, pick something */
			ise->se_chan = sp->curchan;
		}
	} else
		ise->se_chan = sp->curchan;
	ise->se_fhdwell = sp->fhdwell;
	ise->se_fhindex = sp->fhindex;
	ise->se_erp = sp->erp;
	ise->se_timoff = sp->timoff;
	if (sp->tim != NULL) {
		const struct ieee80211_tim_ie *tim =
		    (const struct ieee80211_tim_ie *) sp->tim;
		ise->se_dtimperiod = tim->tim_period;
	}
	saveie(&ise->se_wme_ie, sp->wme);
	saveie(&ise->se_wpa_ie, sp->wpa);
	saveie(&ise->se_rsn_ie, sp->rsn);
	saveie(&ise->se_ath_ie, sp->ath);
	saveie(&ise->se_htcap_ie, sp->htcap);
	saveie(&ise->se_htinfo_ie, sp->htinfo);

	/* clear failure count after STA_FAIL_AGE passes */
	if (se->se_fails && (ticks - se->se_lastfail) > STA_FAILS_AGE*hz) {
		se->se_fails = 0;
		IEEE80211_NOTE_MAC(ic, IEEE80211_MSG_SCAN, macaddr,
		    "%s: fails %u", __func__, se->se_fails);
	}

	se->se_lastupdate = ticks;		/* update time */
	se->se_seen = 1;
	se->se_notseen = 0;

	mtx_unlock(&st->st_lock);

	/*
	 * If looking for a quick choice and nothing's
	 * been found check here.
	 */
	if (PICK1ST(ss) && match_bss(ic, ss, se, IEEE80211_MSG_SCAN) == 0)
		ss->ss_flags |= IEEE80211_SCAN_GOTPICK;

	return 1;
#undef PICK1ST
#undef ISPROBE
}

/*
 * Check if a channel is excluded by user request.
 */
static int
isexcluded(struct ieee80211com *ic, const struct ieee80211_channel *c)
{
	return (isclr(ic->ic_chan_active, c->ic_ieee) ||
	    (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
	     c->ic_freq != ic->ic_des_chan->ic_freq));
}

static struct ieee80211_channel *
find11gchannel(struct ieee80211com *ic, int i, int freq)
{
	struct ieee80211_channel *c;
	int j;

	/*
	 * The normal ordering in the channel list is b channel
	 * immediately followed by g so optimize the search for
	 * this.  We'll still do a full search just in case.
	 */
	for (j = i+1; j < ic->ic_nchans; j++) {
		c = &ic->ic_channels[j];
		if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
			return c;
	}
	for (j = 0; j < i; j++) {
		c = &ic->ic_channels[j];
		if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
			return c;
	}
	return NULL;
}
static const u_int chanflags[IEEE80211_MODE_MAX] = {
	IEEE80211_CHAN_B,	/* IEEE80211_MODE_AUTO */
	IEEE80211_CHAN_A,	/* IEEE80211_MODE_11A */
	IEEE80211_CHAN_B,	/* IEEE80211_MODE_11B */
	IEEE80211_CHAN_G,	/* IEEE80211_MODE_11G */
	IEEE80211_CHAN_FHSS,	/* IEEE80211_MODE_FH */
	IEEE80211_CHAN_A,	/* IEEE80211_MODE_TURBO_A (check base channel)*/
	IEEE80211_CHAN_G,	/* IEEE80211_MODE_TURBO_G */
	IEEE80211_CHAN_ST,	/* IEEE80211_MODE_STURBO_A */
	IEEE80211_CHAN_A,	/* IEEE80211_MODE_11NA (check legacy) */
	IEEE80211_CHAN_G,	/* IEEE80211_MODE_11NG (check legacy) */
};

static void
add_channels(struct ieee80211com *ic,
	struct ieee80211_scan_state *ss,
	enum ieee80211_phymode mode, const uint16_t freq[], int nfreq)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	struct ieee80211_channel *c, *cg;
	u_int modeflags;
	int i;

	KASSERT(mode < N(chanflags), ("Unexpected mode %u", mode));
	modeflags = chanflags[mode];
	for (i = 0; i < nfreq; i++) {
		if (ss->ss_last >= IEEE80211_SCAN_MAX)
			break;

		c = ieee80211_find_channel(ic, freq[i], modeflags);
		if (c != NULL && isexcluded(ic, c))
			continue;
		if (mode == IEEE80211_MODE_AUTO) {
			/*
			 * XXX special-case 11b/g channels so we select
			 *     the g channel if both are present or there
			 *     are only g channels.
			 */
			if (c == NULL || IEEE80211_IS_CHAN_B(c)) {
				cg = find11gchannel(ic, i, freq[i]);
				if (cg != NULL)
					c = cg;
			}
		}
		if (c == NULL)
			continue;

		ss->ss_chans[ss->ss_last++] = c;
	}
#undef N
}

static const uint16_t rcl1[] =		/* 8 FCC channel: 52, 56, 60, 64, 36, 40, 44, 48 */
{ 5260, 5280, 5300, 5320, 5180, 5200, 5220, 5240 };
static const uint16_t rcl2[] =		/* 4 MKK channels: 34, 38, 42, 46 */
{ 5170, 5190, 5210, 5230 };
static const uint16_t rcl3[] =		/* 2.4Ghz ch: 1,6,11,7,13 */
{ 2412, 2437, 2462, 2442, 2472 };
static const uint16_t rcl4[] =		/* 5 FCC channel: 149, 153, 161, 165 */
{ 5745, 5765, 5785, 5805, 5825 };
static const uint16_t rcl7[] =		/* 11 ETSI channel: 100,104,108,112,116,120,124,128,132,136,140 */
{ 5500, 5520, 5540, 5560, 5580, 5600, 5620, 5640, 5660, 5680, 5700 };
static const uint16_t rcl8[] =		/* 2.4Ghz ch: 2,3,4,5,8,9,10,12 */
{ 2417, 2422, 2427, 2432, 2447, 2452, 2457, 2467 };
static const uint16_t rcl9[] =		/* 2.4Ghz ch: 14 */
{ 2484 };
static const uint16_t rcl10[] =		/* Added Korean channels 2312-2372 */
{ 2312, 2317, 2322, 2327, 2332, 2337, 2342, 2347, 2352, 2357, 2362, 2367, 2372 };
static const uint16_t rcl11[] =		/* Added Japan channels in 4.9/5.0 spectrum */
{ 5040, 5060, 5080, 4920, 4940, 4960, 4980 };
#ifdef ATH_TURBO_SCAN
static const uint16_t rcl5[] =		/* 3 static turbo channels */
{ 5210, 5250, 5290 };
static const uint16_t rcl6[] =		/* 2 static turbo channels */
{ 5760, 5800 };
static const uint16_t rcl6x[] =		/* 4 FCC3 turbo channels */
{ 5540, 5580, 5620, 5660 };
static const uint16_t rcl12[] =		/* 2.4Ghz Turbo channel 6 */
{ 2437 };
static const uint16_t rcl13[] =		/* dynamic Turbo channels */
{ 5200, 5240, 5280, 5765, 5805 };
#endif /* ATH_TURBO_SCAN */

struct scanlist {
	uint16_t	mode;
	uint16_t	count;
	const uint16_t	*list;
};

#define	X(a)	.count = sizeof(a)/sizeof(a[0]), .list = a

static const struct scanlist staScanTable[] = {
	{ IEEE80211_MODE_11B,   	X(rcl3) },
	{ IEEE80211_MODE_11A,   	X(rcl1) },
	{ IEEE80211_MODE_11A,   	X(rcl2) },
	{ IEEE80211_MODE_11B,   	X(rcl8) },
	{ IEEE80211_MODE_11B,   	X(rcl9) },
	{ IEEE80211_MODE_11A,   	X(rcl4) },
#ifdef ATH_TURBO_SCAN
	{ IEEE80211_MODE_STURBO_A,	X(rcl5) },
	{ IEEE80211_MODE_STURBO_A,	X(rcl6) },
	{ IEEE80211_MODE_TURBO_A,	X(rcl6x) },
	{ IEEE80211_MODE_TURBO_A,	X(rcl13) },
#endif /* ATH_TURBO_SCAN */
	{ IEEE80211_MODE_11A,		X(rcl7) },
	{ IEEE80211_MODE_11B,		X(rcl10) },
	{ IEEE80211_MODE_11A,		X(rcl11) },
#ifdef ATH_TURBO_SCAN
	{ IEEE80211_MODE_TURBO_G,	X(rcl12) },
#endif /* ATH_TURBO_SCAN */
	{ .list = NULL }
};

static int
checktable(const struct scanlist *scan, const struct ieee80211_channel *c)
{
	int i;

	for (; scan->list != NULL; scan++) {
		for (i = 0; i < scan->count; i++)
			if (scan->list[i] == c->ic_freq) 
				return 1;
	}
	return 0;
}

/*
 * Start a station-mode scan by populating the channel list.
 */
static int
sta_start(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct sta_table *st = ss->ss_priv;
	const struct scanlist *scan;
	enum ieee80211_phymode mode;
	struct ieee80211_channel *c;
	int i;

	ss->ss_last = 0;
	/*
	 * Use the table of ordered channels to construct the list
	 * of channels for scanning.  Any channels in the ordered
	 * list not in the master list will be discarded.
	 */
	for (scan = staScanTable; scan->list != NULL; scan++) {
		mode = scan->mode;
		if (ic->ic_des_mode != IEEE80211_MODE_AUTO) {
			/*
			 * If a desired mode was specified, scan only 
			 * channels that satisfy that constraint.
			 */
			if (ic->ic_des_mode != mode) {
				/*
				 * The scan table marks 2.4Ghz channels as b
				 * so if the desired mode is 11g, then use
				 * the 11b channel list but upgrade the mode.
				 */
				if (ic->ic_des_mode != IEEE80211_MODE_11G ||
				    mode != IEEE80211_MODE_11B)
					continue;
				mode = IEEE80211_MODE_11G;	/* upgrade */
			}
		} else {
			/*
			 * This lets add_channels upgrade an 11b channel
			 * to 11g if available.
			 */
			if (mode == IEEE80211_MODE_11B)
				mode = IEEE80211_MODE_AUTO;
		}
#ifdef IEEE80211_F_XR
		/* XR does not operate on turbo channels */
		if ((ic->ic_flags & IEEE80211_F_XR) &&
		    (mode == IEEE80211_MODE_TURBO_A ||
		     mode == IEEE80211_MODE_TURBO_G ||
		     mode == IEEE80211_MODE_STURBO_A))
			continue;
#endif
		/*
		 * Add the list of the channels; any that are not
		 * in the master channel list will be discarded.
		 */
		add_channels(ic, ss, mode, scan->list, scan->count);
	}

	/*
	 * Add the channels from the ic (from HAL) that are not present
	 * in the staScanTable.
	 */
	for (i = 0; i < ic->ic_nchans; i++) {
		if (ss->ss_last >= IEEE80211_SCAN_MAX)
			break;

		c = &ic->ic_channels[i];
		/*
		 * Ignore dynamic turbo channels; we scan them
		 * in normal mode (i.e. not boosted).  Likewise
		 * for HT channels, they get scanned using
		 * legacy rates.
		 */
		if (IEEE80211_IS_CHAN_DTURBO(c) || IEEE80211_IS_CHAN_HT(c))
			continue;

		/*
		 * If a desired mode was specified, scan only 
		 * channels that satisfy that constraint.
		 */
		if (ic->ic_des_mode != IEEE80211_MODE_AUTO &&
		    ic->ic_des_mode != ieee80211_chan2mode(c))
			continue;

		/*
		 * Skip channels excluded by user request.
		 */
		if (isexcluded(ic, c))
			continue;

		/*
		 * Add the channel unless it is listed in the
		 * fixed scan order tables.  This insures we
		 * don't sweep back in channels we filtered out
		 * above.
		 */
		if (checktable(staScanTable, c))
			continue;

		/* Add channel to scanning list. */
		ss->ss_chans[ss->ss_last++] = c;
	}

	ss->ss_next = 0;
	/* XXX tunables */
	ss->ss_mindwell = msecs_to_ticks(20);		/* 20ms */
	ss->ss_maxdwell = msecs_to_ticks(200);		/* 200ms */

#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(ic)) {
		if_printf(ic->ic_ifp, "scan set ");
		ieee80211_scan_dump_channels(ss);
		printf(" dwell min %ld max %ld\n",
			ss->ss_mindwell, ss->ss_maxdwell);
	}
#endif /* IEEE80211_DEBUG */

	st->st_newscan = 1;

	return 0;
#undef N
}

/*
 * Restart a bg scan.
 */
static int
sta_restart(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
	struct sta_table *st = ss->ss_priv;

	st->st_newscan = 1;
	return 0;
}

/*
 * Cancel an ongoing scan.
 */
static int
sta_cancel(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
	return 0;
}

static uint8_t
maxrate(const struct ieee80211_scan_entry *se)
{
	uint8_t rmax, r;
	int i;

	rmax = 0;
	for (i = 0; i < se->se_rates[1]; i++) {
		r = se->se_rates[2+i] & IEEE80211_RATE_VAL;
		if (r > rmax)
			rmax = r;
	}
	for (i = 0; i < se->se_xrates[1]; i++) {
		r = se->se_xrates[2+i] & IEEE80211_RATE_VAL;
		if (r > rmax)
			rmax = r;
	}
	return rmax;
}

/*
 * Compare the capabilities of two entries and decide which is
 * more desirable (return >0 if a is considered better).  Note
 * that we assume compatibility/usability has already been checked
 * so we don't need to (e.g. validate whether privacy is supported).
 * Used to select the best scan candidate for association in a BSS.
 */
static int
sta_compare(const struct sta_entry *a, const struct sta_entry *b)
{
#define	PREFER(_a,_b,_what) do {			\
	if (((_a) ^ (_b)) & (_what))			\
		return ((_a) & (_what)) ? 1 : -1;	\
} while (0)
	uint8_t maxa, maxb;
	int8_t rssia, rssib;
	int weight;

	/* privacy support */
	PREFER(a->base.se_capinfo, b->base.se_capinfo,
		IEEE80211_CAPINFO_PRIVACY);

	/* compare count of previous failures */
	weight = b->se_fails - a->se_fails;
	if (abs(weight) > 1)
		return weight;

	/*
	 * Compare rssi.  If the two are considered equivalent
	 * then fallback to other criteria.  We threshold the
	 * comparisons to avoid selecting an ap purely by rssi
	 * when both values may be good but one ap is otherwise
	 * more desirable (e.g. an 11b-only ap with stronger
	 * signal than an 11g ap).
	 */
	rssia = MIN(a->base.se_rssi, STA_RSSI_MAX);
	rssib = MIN(b->base.se_rssi, STA_RSSI_MAX);
	if (abs(rssib - rssia) < 5) {
		/* best/max rate preferred if signal level close enough XXX */
		maxa = maxrate(&a->base);
		maxb = maxrate(&b->base);
		if (maxa != maxb)
			return maxa - maxb;
		/* XXX use freq for channel preference */
		/* for now just prefer 5Ghz band to all other bands */
		if (IEEE80211_IS_CHAN_5GHZ(a->base.se_chan) &&
		   !IEEE80211_IS_CHAN_5GHZ(b->base.se_chan))
			return 1;
		if (!IEEE80211_IS_CHAN_5GHZ(a->base.se_chan) &&
		     IEEE80211_IS_CHAN_5GHZ(b->base.se_chan))
			return -1;
	}
	/* all things being equal, use signal level */
	return a->base.se_rssi - b->base.se_rssi;
#undef PREFER
}

/*
 * Check rate set suitability and return the best supported rate.
 */
static int
check_rate(struct ieee80211com *ic, const struct ieee80211_scan_entry *se)
{
#define	RV(v)	((v) & IEEE80211_RATE_VAL)
	const struct ieee80211_rateset *srs;
	int i, j, nrs, r, okrate, badrate, fixedrate;
	const uint8_t *rs;

	okrate = badrate = fixedrate = 0;

	srs = ieee80211_get_suprates(ic, se->se_chan);
	nrs = se->se_rates[1];
	rs = se->se_rates+2;
	fixedrate = IEEE80211_FIXED_RATE_NONE;
again:
	for (i = 0; i < nrs; i++) {
		r = RV(rs[i]);
		badrate = r;
		/*
		 * Check any fixed rate is included. 
		 */
		if (r == ic->ic_fixed_rate)
			fixedrate = r;
		/*
		 * Check against our supported rates.
		 */
		for (j = 0; j < srs->rs_nrates; j++)
			if (r == RV(srs->rs_rates[j])) {
				if (r > okrate)		/* NB: track max */
					okrate = r;
				break;
			}

		if (j == srs->rs_nrates && (rs[i] & IEEE80211_RATE_BASIC)) {
			/*
			 * Don't try joining a BSS, if we don't support
			 * one of its basic rates.
			 */
			okrate = 0;
			goto back;
		}
	}
	if (rs == se->se_rates+2) {
		/* scan xrates too; sort of an algol68-style for loop */
		nrs = se->se_xrates[1];
		rs = se->se_xrates+2;
		goto again;
	}

back:
	if (okrate == 0 || ic->ic_fixed_rate != fixedrate)
		return badrate | IEEE80211_RATE_BASIC;
	else
		return RV(okrate);
#undef RV
}

static int
match_ssid(const uint8_t *ie,
	int nssid, const struct ieee80211_scan_ssid ssids[])
{
	int i;

	for (i = 0; i < nssid; i++) {
		if (ie[1] == ssids[i].len &&
		     memcmp(ie+2, ssids[i].ssid, ie[1]) == 0)
			return 1;
	}
	return 0;
}

/*
 * Test a scan candidate for suitability/compatibility.
 */
static int
match_bss(struct ieee80211com *ic,
	const struct ieee80211_scan_state *ss, struct sta_entry *se0,
	int debug)
{
	struct ieee80211_scan_entry *se = &se0->base;
	uint8_t rate;
	int fail;

	fail = 0;
	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, se->se_chan)))
		fail |= MATCH_CHANNEL;
	/*
	 * NB: normally the desired mode is used to construct
	 * the channel list, but it's possible for the scan
	 * cache to include entries for stations outside this
	 * list so we check the desired mode here to weed them
	 * out.
	 */
	if (ic->ic_des_mode != IEEE80211_MODE_AUTO &&
	    (se->se_chan->ic_flags & IEEE80211_CHAN_ALLTURBO) !=
	    chanflags[ic->ic_des_mode])
		fail |= MATCH_CHANNEL;
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		if ((se->se_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			fail |= MATCH_CAPINFO;
	} else {
		if ((se->se_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= MATCH_CAPINFO;
	}
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		if ((se->se_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			fail |= MATCH_PRIVACY;
	} else {
		/* XXX does this mean privacy is supported or required? */
		if (se->se_capinfo & IEEE80211_CAPINFO_PRIVACY)
			fail |= MATCH_PRIVACY;
	}
	rate = check_rate(ic, se);
	if (rate & IEEE80211_RATE_BASIC)
		fail |= MATCH_RATE;
	if (ss->ss_nssid != 0 &&
	    !match_ssid(se->se_ssid, ss->ss_nssid, ss->ss_ssid))
		fail |= MATCH_SSID;
	if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, se->se_bssid))
		fail |=  MATCH_BSSID;
	if (se0->se_fails >= STA_FAILS_MAX)
		fail |= MATCH_FAILS;
	/* NB: entries may be present awaiting purge, skip */
	if (se0->se_notseen >= STA_PURGE_SCANS)
		fail |= MATCH_NOTSEEN;
	if (se->se_rssi < STA_RSSI_MIN)
		fail |= MATCH_RSSI;
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg(ic, debug)) {
		printf(" %c %s",
		    fail & MATCH_FAILS ? '=' :
		    fail & MATCH_NOTSEEN ? '^' :
		    fail ? '-' : '+', ether_sprintf(se->se_macaddr));
		printf(" %s%c", ether_sprintf(se->se_bssid),
		    fail & MATCH_BSSID ? '!' : ' ');
		printf(" %3d%c", ieee80211_chan2ieee(ic, se->se_chan),
			fail & MATCH_CHANNEL ? '!' : ' ');
		printf(" %+4d%c", se->se_rssi, fail & MATCH_RSSI ? '!' : ' ');
		printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
		    fail & MATCH_RATE ? '!' : ' ');
		printf(" %4s%c",
		    (se->se_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
		    (se->se_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
		    "????",
		    fail & MATCH_CAPINFO ? '!' : ' ');
		printf(" %3s%c ",
		    (se->se_capinfo & IEEE80211_CAPINFO_PRIVACY) ?
		    "wep" : "no",
		    fail & MATCH_PRIVACY ? '!' : ' ');
		ieee80211_print_essid(se->se_ssid+2, se->se_ssid[1]);
		printf("%s\n", fail & MATCH_SSID ? "!" : "");
	}
#endif
	return fail;
}

static void
sta_update_notseen(struct sta_table *st)
{
	struct sta_entry *se;

	mtx_lock(&st->st_lock);
	TAILQ_FOREACH(se, &st->st_entry, se_list) {
		/*
		 * If seen the reset and don't bump the count;
		 * otherwise bump the ``not seen'' count.  Note
		 * that this insures that stations for which we
		 * see frames while not scanning but not during
		 * this scan will not be penalized.
		 */
		if (se->se_seen)
			se->se_seen = 0;
		else
			se->se_notseen++;
	}
	mtx_unlock(&st->st_lock);
}

static void
sta_dec_fails(struct sta_table *st)
{
	struct sta_entry *se;

	mtx_lock(&st->st_lock);
	TAILQ_FOREACH(se, &st->st_entry, se_list)
		if (se->se_fails)
			se->se_fails--;
	mtx_unlock(&st->st_lock);
}

static struct sta_entry *
select_bss(struct ieee80211_scan_state *ss, struct ieee80211com *ic, int debug)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se, *selbs = NULL;

	IEEE80211_DPRINTF(ic, debug, " %s\n",
	    "macaddr          bssid         chan  rssi  rate flag  wep  essid");
	mtx_lock(&st->st_lock);
	TAILQ_FOREACH(se, &st->st_entry, se_list) {
		if (match_bss(ic, ss, se, debug) == 0) {
			if (selbs == NULL)
				selbs = se;
			else if (sta_compare(se, selbs) > 0)
				selbs = se;
		}
	}
	mtx_unlock(&st->st_lock);

	return selbs;
}

/*
 * Pick an ap or ibss network to join or find a channel
 * to use to start an ibss network.
 */
static int
sta_pick_bss(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *selbs;

	KASSERT(ic->ic_opmode == IEEE80211_M_STA,
		("wrong mode %u", ic->ic_opmode));

	if (st->st_newscan) {
		sta_update_notseen(st);
		st->st_newscan = 0;
	}
	if (ss->ss_flags & IEEE80211_SCAN_NOPICK) {
		/*
		 * Manual/background scan, don't select+join the
		 * bss, just return.  The scanning framework will
		 * handle notification that this has completed.
		 */
		ss->ss_flags &= ~IEEE80211_SCAN_NOPICK;
		return 1;
	}
	/*
	 * Automatic sequencing; look for a candidate and
	 * if found join the network.
	 */
	/* NB: unlocked read should be ok */
	if (TAILQ_FIRST(&st->st_entry) == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			"%s: no scan candidate\n", __func__);
notfound:
		/*
		 * If nothing suitable was found decrement
		 * the failure counts so entries will be
		 * reconsidered the next time around.  We
		 * really want to do this only for sta's
		 * where we've previously had some success.
		 */
		sta_dec_fails(st);
		st->st_newscan = 1;
		return 0;			/* restart scan */
	}
	selbs = select_bss(ss, ic, IEEE80211_MSG_SCAN);
	if (selbs == NULL || !ieee80211_sta_join(ic, &selbs->base))
		goto notfound;
	return 1;				/* terminate scan */
}

/*
 * Lookup an entry in the scan cache.  We assume we're
 * called from the bottom half or such that we don't need
 * to block the bottom half so that it's safe to return
 * a reference to an entry w/o holding the lock on the table.
 */
static struct sta_entry *
sta_lookup(struct sta_table *st, const uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct sta_entry *se;
	int hash = STA_HASH(macaddr);

	mtx_lock(&st->st_lock);
	LIST_FOREACH(se, &st->st_hash[hash], se_hash)
		if (IEEE80211_ADDR_EQ(se->base.se_macaddr, macaddr))
			break;
	mtx_unlock(&st->st_lock);

	return se;		/* NB: unlocked */
}

static void
sta_roam_check(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
	struct ieee80211_node *ni = ic->ic_bss;
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se, *selbs;
	uint8_t roamRate, curRate;
	int8_t roamRssi, curRssi;

	se = sta_lookup(st, ni->ni_macaddr);
	if (se == NULL) {
		/* XXX something is wrong */
		return;
	}

	/* XXX do we need 11g too? */
	if (IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan)) {
		roamRate = ic->ic_roam.rate11b;
		roamRssi = ic->ic_roam.rssi11b;
	} else if (IEEE80211_IS_CHAN_B(ic->ic_bsschan)) {
		roamRate = ic->ic_roam.rate11bOnly;
		roamRssi = ic->ic_roam.rssi11bOnly;
	} else {
		roamRate = ic->ic_roam.rate11a;
		roamRssi = ic->ic_roam.rssi11a;
	}
	/* NB: the most up to date rssi is in the node, not the scan cache */
	curRssi = ic->ic_node_getrssi(ni);
	if (ic->ic_fixed_rate == IEEE80211_FIXED_RATE_NONE) {
		curRate = ni->ni_rates.rs_rates[ni->ni_txrate] & IEEE80211_RATE_VAL;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ROAM,
		    "%s: currssi %d currate %u roamrssi %d roamrate %u\n",
		    __func__, curRssi, curRate, roamRssi, roamRate);
	} else {
		curRate = roamRate;	/* NB: insure compare below fails */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ROAM,
		    "%s: currssi %d roamrssi %d\n", __func__, curRssi, roamRssi);
	}
	/*
	 * Check if a new ap should be used and switch.
	 * XXX deauth current ap
	 */
	if (curRate < roamRate || curRssi < roamRssi) {
		if (time_after(ticks, ic->ic_lastscan + ic->ic_scanvalid)) {
			/*
			 * Scan cache contents are too old; force a scan now
			 * if possible so we have current state to make a
			 * decision with.  We don't kick off a bg scan if
			 * we're using dynamic turbo and boosted or if the
			 * channel is busy.
			 * XXX force immediate switch on scan complete
			 */
			if (!IEEE80211_IS_CHAN_DTURBO(ic->ic_curchan) &&
			    time_after(ticks, ic->ic_lastdata + ic->ic_bgscanidle))
				ieee80211_bg_scan(ic);
			return;
		}
		se->base.se_rssi = curRssi;
		selbs = select_bss(ss, ic, IEEE80211_MSG_ROAM);
		if (selbs != NULL && selbs != se) {
			IEEE80211_DPRINTF(ic,
			    IEEE80211_MSG_ROAM | IEEE80211_MSG_DEBUG,
			    "%s: ROAM: curRate %u, roamRate %u, "
			    "curRssi %d, roamRssi %d\n", __func__,
			    curRate, roamRate, curRssi, roamRssi);
			ieee80211_sta_join(ic, &selbs->base);
		}
	}
}

/*
 * Age entries in the scan cache.
 * XXX also do roaming since it's convenient
 */
static void
sta_age(struct ieee80211_scan_state *ss)
{
	struct ieee80211com *ic = ss->ss_ic;
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se, *next;

	mtx_lock(&st->st_lock);
	TAILQ_FOREACH_SAFE(se, &st->st_entry, se_list, next) {
		if (se->se_notseen > STA_PURGE_SCANS) {
			TAILQ_REMOVE(&st->st_entry, se, se_list);
			LIST_REMOVE(se, se_hash);
			FREE(se, M_80211_SCAN);
		}
	}
	mtx_unlock(&st->st_lock);
	/*
	 * If rate control is enabled check periodically to see if
	 * we should roam from our current connection to one that
	 * might be better.  This only applies when we're operating
	 * in sta mode and automatic roaming is set.
	 * XXX defer if busy
	 * XXX repeater station
	 * XXX do when !bgscan?
	 */
	KASSERT(ic->ic_opmode == IEEE80211_M_STA,
		("wrong mode %u", ic->ic_opmode));
	if (ic->ic_roaming == IEEE80211_ROAMING_AUTO &&
	    (ic->ic_flags & IEEE80211_F_BGSCAN) &&
	    ic->ic_state >= IEEE80211_S_RUN)
		/* XXX vap is implicit */
		sta_roam_check(ss, ic);
}

/*
 * Iterate over the entries in the scan cache, invoking
 * the callback function on each one.
 */
static void
sta_iterate(struct ieee80211_scan_state *ss, 
	ieee80211_scan_iter_func *f, void *arg)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se;
	u_int gen;

	mtx_lock(&st->st_scanlock);
	gen = st->st_scangen++;
restart:
	mtx_lock(&st->st_lock);
	TAILQ_FOREACH(se, &st->st_entry, se_list) {
		if (se->se_scangen != gen) {
			se->se_scangen = gen;
			/* update public state */
			se->base.se_age = ticks - se->se_lastupdate;
			mtx_unlock(&st->st_lock);
			(*f)(arg, &se->base);
			goto restart;
		}
	}
	mtx_unlock(&st->st_lock);

	mtx_unlock(&st->st_scanlock);
}

static void
sta_assoc_fail(struct ieee80211_scan_state *ss,
	const uint8_t macaddr[IEEE80211_ADDR_LEN], int reason)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se;

	se = sta_lookup(st, macaddr);
	if (se != NULL) {
		se->se_fails++;
		se->se_lastfail = ticks;
		IEEE80211_NOTE_MAC(ss->ss_ic, IEEE80211_MSG_SCAN,
		    macaddr, "%s: reason %u fails %u",
		    __func__, reason, se->se_fails);
	}
}

static void
sta_assoc_success(struct ieee80211_scan_state *ss,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se;

	se = sta_lookup(st, macaddr);
	if (se != NULL) {
#if 0
		se->se_fails = 0;
		IEEE80211_NOTE_MAC(ss->ss_ic, IEEE80211_MSG_SCAN,
		    macaddr, "%s: fails %u",
		    __func__, se->se_fails);
#endif
		se->se_lastassoc = ticks;
	}
}

static const struct ieee80211_scanner sta_default = {
	.scan_name		= "default",
	.scan_attach		= sta_attach,
	.scan_detach		= sta_detach,
	.scan_start		= sta_start,
	.scan_restart		= sta_restart,
	.scan_cancel		= sta_cancel,
	.scan_end		= sta_pick_bss,
	.scan_flush		= sta_flush,
	.scan_add		= sta_add,
	.scan_age		= sta_age,
	.scan_iterate		= sta_iterate,
	.scan_assoc_fail	= sta_assoc_fail,
	.scan_assoc_success	= sta_assoc_success,
};

/*
 * Adhoc mode-specific support.
 */

static const uint16_t adhocWorld[] =		/* 36, 40, 44, 48 */
{ 5180, 5200, 5220, 5240 };
static const uint16_t adhocFcc3[] =		/* 36, 40, 44, 48 145, 149, 153, 157, 161, 165 */
{ 5180, 5200, 5220, 5240, 5725, 5745, 5765, 5785, 5805, 5825 };
static const uint16_t adhocMkk[] =		/* 34, 38, 42, 46 */
{ 5170, 5190, 5210, 5230 };
static const uint16_t adhoc11b[] =		/* 10, 11 */
{ 2457, 2462 };

static const struct scanlist adhocScanTable[] = {
	{ IEEE80211_MODE_11B,   	X(adhoc11b) },
	{ IEEE80211_MODE_11A,   	X(adhocWorld) },
	{ IEEE80211_MODE_11A,   	X(adhocFcc3) },
	{ IEEE80211_MODE_11B,   	X(adhocMkk) },
	{ .list = NULL }
};
#undef X

/*
 * Start an adhoc-mode scan by populating the channel list.
 */
static int
adhoc_start(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct sta_table *st = ss->ss_priv;
	const struct scanlist *scan;
	enum ieee80211_phymode mode;
	
	ss->ss_last = 0;
	/*
	 * Use the table of ordered channels to construct the list
	 * of channels for scanning.  Any channels in the ordered
	 * list not in the master list will be discarded.
	 */
	for (scan = adhocScanTable; scan->list != NULL; scan++) {
		mode = scan->mode;
		if (ic->ic_des_mode != IEEE80211_MODE_AUTO) {
			/*
			 * If a desired mode was specified, scan only 
			 * channels that satisfy that constraint.
			 */
			if (ic->ic_des_mode != mode) {
				/*
				 * The scan table marks 2.4Ghz channels as b
				 * so if the desired mode is 11g, then use
				 * the 11b channel list but upgrade the mode.
				 */
				if (ic->ic_des_mode != IEEE80211_MODE_11G ||
				    mode != IEEE80211_MODE_11B)
					continue;
				mode = IEEE80211_MODE_11G;	/* upgrade */
			}
		} else {
			/*
			 * This lets add_channels upgrade an 11b channel
			 * to 11g if available.
			 */
			if (mode == IEEE80211_MODE_11B)
				mode = IEEE80211_MODE_AUTO;
		}
#ifdef IEEE80211_F_XR
		/* XR does not operate on turbo channels */
		if ((ic->ic_flags & IEEE80211_F_XR) &&
		    (mode == IEEE80211_MODE_TURBO_A ||
		     mode == IEEE80211_MODE_TURBO_G))
			continue;
#endif
		/*
		 * Add the list of the channels; any that are not
		 * in the master channel list will be discarded.
		 */
		add_channels(ic, ss, mode, scan->list, scan->count);
	}
	ss->ss_next = 0;
	/* XXX tunables */
	ss->ss_mindwell = msecs_to_ticks(200);		/* 200ms */
	ss->ss_maxdwell = msecs_to_ticks(200);		/* 200ms */

#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(ic)) {
		if_printf(ic->ic_ifp, "scan set ");
		ieee80211_scan_dump_channels(ss);
		printf(" dwell min %ld max %ld\n",
			ss->ss_mindwell, ss->ss_maxdwell);
	}
#endif /* IEEE80211_DEBUG */

	st->st_newscan = 1;

	return 0;
#undef N
}

/*
 * Select a channel to start an adhoc network on.
 * The channel list was populated with appropriate
 * channels so select one that looks least occupied.
 * XXX need regulatory domain constraints
 */
static struct ieee80211_channel *
adhoc_pick_channel(struct ieee80211_scan_state *ss)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se;
	struct ieee80211_channel *c, *bestchan;
	int i, bestrssi, maxrssi;

	bestchan = NULL;
	bestrssi = -1;

	mtx_lock(&st->st_lock);
	for (i = 0; i < ss->ss_last; i++) {
		c = ss->ss_chans[i];
		maxrssi = 0;
		TAILQ_FOREACH(se, &st->st_entry, se_list) {
			if (se->base.se_chan != c)
				continue;
			if (se->base.se_rssi > maxrssi)
				maxrssi = se->base.se_rssi;
		}
		if (bestchan == NULL || maxrssi < bestrssi)
			bestchan = c;
	}
	mtx_unlock(&st->st_lock);

	return bestchan;
}

/*
 * Pick an ibss network to join or find a channel
 * to use to start an ibss network.
 */
static int
adhoc_pick_bss(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *selbs;
	struct ieee80211_channel *chan;

	KASSERT(ic->ic_opmode == IEEE80211_M_IBSS ||
		ic->ic_opmode == IEEE80211_M_AHDEMO,
		("wrong opmode %u", ic->ic_opmode));

	if (st->st_newscan) {
		sta_update_notseen(st);
		st->st_newscan = 0;
	}
	if (ss->ss_flags & IEEE80211_SCAN_NOPICK) {
		/*
		 * Manual/background scan, don't select+join the
		 * bss, just return.  The scanning framework will
		 * handle notification that this has completed.
		 */
		ss->ss_flags &= ~IEEE80211_SCAN_NOPICK;
		return 1;
	}
	/*
	 * Automatic sequencing; look for a candidate and
	 * if found join the network.
	 */
	/* NB: unlocked read should be ok */
	if (TAILQ_FIRST(&st->st_entry) == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			"%s: no scan candidate\n", __func__);
notfound:
		if (ic->ic_des_nssid) {
			/*
			 * No existing adhoc network to join and we have
			 * an ssid; start one up.  If no channel was
			 * specified, try to select a channel.
			 */
			if (ic->ic_des_chan == IEEE80211_CHAN_ANYC)
				chan = ieee80211_ht_adjust_channel(ic,
				    adhoc_pick_channel(ss), ic->ic_flags_ext);
			else
				chan = ic->ic_des_chan;
			if (chan != NULL) {
				ieee80211_create_ibss(ic, chan);
				return 1;
			}
		}
		/*
		 * If nothing suitable was found decrement
		 * the failure counts so entries will be
		 * reconsidered the next time around.  We
		 * really want to do this only for sta's
		 * where we've previously had some success.
		 */
		sta_dec_fails(st);
		st->st_newscan = 1;
		return 0;			/* restart scan */
	}
	selbs = select_bss(ss, ic, IEEE80211_MSG_SCAN);
	if (selbs == NULL || !ieee80211_sta_join(ic, &selbs->base))
		goto notfound;
	return 1;				/* terminate scan */
}

/*
 * Age entries in the scan cache.
 */
static void
adhoc_age(struct ieee80211_scan_state *ss)
{
	struct sta_table *st = ss->ss_priv;
	struct sta_entry *se, *next;

	mtx_lock(&st->st_lock);
	TAILQ_FOREACH_SAFE(se, &st->st_entry, se_list, next) {
		if (se->se_notseen > STA_PURGE_SCANS) {
			TAILQ_REMOVE(&st->st_entry, se, se_list);
			LIST_REMOVE(se, se_hash);
			FREE(se, M_80211_SCAN);
		}
	}
	mtx_unlock(&st->st_lock);
}

static const struct ieee80211_scanner adhoc_default = {
	.scan_name		= "default",
	.scan_attach		= sta_attach,
	.scan_detach		= sta_detach,
	.scan_start		= adhoc_start,
	.scan_restart		= sta_restart,
	.scan_cancel		= sta_cancel,
	.scan_end		= adhoc_pick_bss,
	.scan_flush		= sta_flush,
	.scan_add		= sta_add,
	.scan_age		= adhoc_age,
	.scan_iterate		= sta_iterate,
	.scan_assoc_fail	= sta_assoc_fail,
	.scan_assoc_success	= sta_assoc_success,
};

/*
 * Module glue.
 */
static int
wlan_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		ieee80211_scanner_register(IEEE80211_M_STA, &sta_default);
		ieee80211_scanner_register(IEEE80211_M_IBSS, &adhoc_default);
		ieee80211_scanner_register(IEEE80211_M_AHDEMO, &adhoc_default);
		return 0;
	case MOD_UNLOAD:
	case MOD_QUIESCE:
		if (nrefs) {
			printf("wlan_scan_sta: still in use (%u dynamic refs)\n",
				nrefs);
			return EBUSY;
		}
		if (type == MOD_UNLOAD) {
			ieee80211_scanner_unregister_all(&sta_default);
			ieee80211_scanner_unregister_all(&adhoc_default);
		}
		return 0;
	}
	return EINVAL;
}

static moduledata_t wlan_mod = {
	"wlan_scan_sta",
	wlan_modevent,
	0
};
DECLARE_MODULE(wlan_scan_sta, wlan_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(wlan_scan_sta, 1);
MODULE_DEPEND(wlan_scan_sta, wlan, 1, 1, 1);
