/*-
 * Copyright (c) 2005-2007 Sam Leffler, Errno Consulting
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
 *
 * $FreeBSD$
 */
#ifndef _NET80211_IEEE80211_SCAN_H_
#define _NET80211_IEEE80211_SCAN_H_

#define	IEEE80211_SCAN_MAX	IEEE80211_CHAN_MAX

struct ieee80211_scanner;

struct ieee80211_scan_ssid {
	int		len;				/* length in bytes */
	uint8_t		ssid[IEEE80211_NWID_LEN];	/* ssid contents */
};
#define	IEEE80211_SCAN_MAX_SSID	1

struct ieee80211_scan_state {
	struct ieee80211com *ss_ic;
	const struct ieee80211_scanner *ss_ops;	/* policy hookup, see below */
	void		*ss_priv;		/* scanner private state */
	uint16_t	ss_flags;
#define	IEEE80211_SCAN_NOPICK	0x0001		/* scan only, no selection */
#define	IEEE80211_SCAN_ACTIVE	0x0002		/* active scan (probe req) */
#define	IEEE80211_SCAN_PICK1ST	0x0004		/* ``hey sailor'' mode */
#define	IEEE80211_SCAN_BGSCAN	0x0008		/* bg scan, exit ps at end */
#define	IEEE80211_SCAN_ONCE	0x0010		/* do one complete pass */
#define	IEEE80211_SCAN_GOTPICK	0x1000		/* got candidate, can stop */
	uint8_t		ss_nssid;		/* # ssid's to probe/match */
	struct ieee80211_scan_ssid ss_ssid[IEEE80211_SCAN_MAX_SSID];
						/* ssid's to probe/match */
						/* ordered channel set */
	struct ieee80211_channel *ss_chans[IEEE80211_SCAN_MAX];
	uint16_t	ss_next;		/* ix of next chan to scan */
	uint16_t	ss_last;		/* ix+1 of last chan to scan */
	unsigned long	ss_mindwell;		/* min dwell on channel */
	unsigned long	ss_maxdwell;		/* max dwell on channel */
};

/*
 * The upper 16 bits of the flags word is used to communicate
 * information to the scanning code that is NOT recorded in
 * ss_flags.  It might be better to split this stuff out into
 * a separate variable to avoid confusion.
 */
#define	IEEE80211_SCAN_FLUSH	0x10000		/* flush candidate table */
#define	IEEE80211_SCAN_NOSSID	0x20000		/* don't update ssid list */

struct ieee80211com;
void	ieee80211_scan_attach(struct ieee80211com *);
void	ieee80211_scan_detach(struct ieee80211com *);

void	ieee80211_scan_dump_channels(const struct ieee80211_scan_state *);

int	ieee80211_scan_update(struct ieee80211com *);
#define	IEEE80211_SCAN_FOREVER	0x7fffffff
int	ieee80211_start_scan(struct ieee80211com *, int flags, u_int duration,
		u_int nssid, const struct ieee80211_scan_ssid ssids[]);
int	ieee80211_check_scan(struct ieee80211com *, int flags, u_int duration,
		u_int nssid, const struct ieee80211_scan_ssid ssids[]);
int	ieee80211_bg_scan(struct ieee80211com *);
void	ieee80211_cancel_scan(struct ieee80211com *);
void	ieee80211_scan_next(struct ieee80211com *);

struct ieee80211_scanparams;
void	ieee80211_add_scan(struct ieee80211com *,
		const struct ieee80211_scanparams *,
		const struct ieee80211_frame *,
		int subtype, int rssi, int noise, int rstamp);
void	ieee80211_scan_timeout(struct ieee80211com *);

void	ieee80211_scan_assoc_success(struct ieee80211com *,
		const uint8_t mac[IEEE80211_ADDR_LEN]);
enum {
	IEEE80211_SCAN_FAIL_TIMEOUT	= 1,	/* no response to mgmt frame */
	IEEE80211_SCAN_FAIL_STATUS	= 2	/* negative response to " " */
};
void	ieee80211_scan_assoc_fail(struct ieee80211com *,
		const uint8_t mac[IEEE80211_ADDR_LEN], int reason);
void	ieee80211_scan_flush(struct ieee80211com *);

struct ieee80211_scan_entry;
typedef void ieee80211_scan_iter_func(void *,
		const struct ieee80211_scan_entry *);
void	ieee80211_scan_iterate(struct ieee80211com *,
		ieee80211_scan_iter_func, void *);

/*
 * Parameters supplied when adding/updating an entry in a
 * scan cache.  Pointer variables should be set to NULL
 * if no data is available.  Pointer references can be to
 * local data; any information that is saved will be copied.
 * All multi-byte values must be in host byte order.
 */
struct ieee80211_scanparams {
	uint16_t	capinfo;	/* 802.11 capabilities */
	uint16_t	fhdwell;	/* FHSS dwell interval */
	struct ieee80211_channel *curchan;
	uint8_t		bchan;		/* chan# advertised inside beacon */
	uint8_t		fhindex;
	uint8_t		erp;
	uint16_t	bintval;
	uint8_t		timoff;
	uint8_t		*tim;
	uint8_t		*tstamp;
	uint8_t		*country;
	uint8_t		*ssid;
	uint8_t		*rates;
	uint8_t		*xrates;
	uint8_t		*doth;
	uint8_t		*wpa;
	uint8_t		*rsn;
	uint8_t		*wme;
	uint8_t		*htcap;
	uint8_t		*htinfo;
	uint8_t		*ath;
};

/*
 * Scan cache entry format used when exporting data from a policy
 * module; this data may be represented some other way internally.
 */
struct ieee80211_scan_entry {
	uint8_t		se_macaddr[IEEE80211_ADDR_LEN];
	uint8_t		se_bssid[IEEE80211_ADDR_LEN];
	uint8_t		se_ssid[2+IEEE80211_NWID_LEN];
	uint8_t		se_rates[2+IEEE80211_RATE_MAXSIZE];
	uint8_t		se_xrates[2+IEEE80211_RATE_MAXSIZE];
	uint32_t	se_rstamp;	/* recv timestamp */
	union {
		uint8_t		data[8];
		uint64_t	tsf;
	} se_tstamp;			/* from last rcv'd beacon */
	uint16_t	se_intval;	/* beacon interval (host byte order) */
	uint16_t	se_capinfo;	/* capabilities (host byte order) */
	struct ieee80211_channel *se_chan;/* channel where sta found */
	uint16_t	se_timoff;	/* byte offset to TIM ie */
	uint16_t	se_fhdwell;	/* FH only (host byte order) */
	uint8_t		se_fhindex;	/* FH only */
	uint8_t		se_erp;		/* ERP from beacon/probe resp */
	int8_t		se_rssi;	/* avg'd recv ssi */
	int8_t		se_noise;	/* noise floor */
	uint8_t		se_dtimperiod;	/* DTIM period */
	uint8_t		*se_wpa_ie;	/* captured WPA ie */
	uint8_t		*se_rsn_ie;	/* captured RSN ie */
	uint8_t		*se_wme_ie;	/* captured WME ie */
	uint8_t		*se_htcap_ie;	/* captured HTP cap ie */
	uint8_t		*se_htinfo_ie;	/* captured HTP info ie */
	uint8_t		*se_ath_ie;	/* captured Atheros ie */
	u_int		se_age;		/* age of entry (0 on create) */
};
MALLOC_DECLARE(M_80211_SCAN);

/*
 * Template for an in-kernel scan policy module.
 * Modules register with the scanning code and are
 * typically loaded as needed.
 */
struct ieee80211_scanner {
	const char *scan_name;		/* printable name */
	int	(*scan_attach)(struct ieee80211_scan_state *);
	int	(*scan_detach)(struct ieee80211_scan_state *);
	int	(*scan_start)(struct ieee80211_scan_state *,
			struct ieee80211com *);
	int	(*scan_restart)(struct ieee80211_scan_state *,
			struct ieee80211com *);
	int	(*scan_cancel)(struct ieee80211_scan_state *,
			struct ieee80211com *);
	int	(*scan_end)(struct ieee80211_scan_state *,
			struct ieee80211com *);
	int	(*scan_flush)(struct ieee80211_scan_state *);
	/* add an entry to the cache */
	int	(*scan_add)(struct ieee80211_scan_state *,
			const struct ieee80211_scanparams *,
			const struct ieee80211_frame *,
			int subtype, int rssi, int noise, int rstamp);
	/* age and/or purge entries in the cache */
	void	(*scan_age)(struct ieee80211_scan_state *);
	/* note that association failed for an entry */
	void	(*scan_assoc_fail)(struct ieee80211_scan_state *,
			const uint8_t macaddr[IEEE80211_ADDR_LEN],
			int reason);
	/* note that association succeed for an entry */
	void	(*scan_assoc_success)(struct ieee80211_scan_state *,
			const uint8_t macaddr[IEEE80211_ADDR_LEN]);
	/* iterate over entries in the scan cache */
	void	(*scan_iterate)(struct ieee80211_scan_state *,
			ieee80211_scan_iter_func *, void *);
};
void	ieee80211_scanner_register(enum ieee80211_opmode,
		const struct ieee80211_scanner *);
void	ieee80211_scanner_unregister(enum ieee80211_opmode,
		const struct ieee80211_scanner *);
void	ieee80211_scanner_unregister_all(const struct ieee80211_scanner *);
const struct ieee80211_scanner *ieee80211_scanner_get(enum ieee80211_opmode);
#endif /* _NET80211_IEEE80211_SCAN_H_ */
