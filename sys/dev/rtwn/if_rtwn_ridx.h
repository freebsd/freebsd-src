/*	$OpenBSD: if_urtwn.c,v 1.16 2011/02/10 17:26:40 jakemsr Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2014 Kevin Lo <kevlo@FreeBSD.org>
 * Copyright (c) 2015-2016 Andriy Voskoboinyk <avos@FreeBSD.org>
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

#ifndef IF_RTWN_RIDX_H
#define IF_RTWN_RIDX_H

/* HW rate indices. */

/*
 * Note - these are also used as offsets into the TX power table
 * array.
 */
#define RTWN_RIDX_CCK1		0
#define RTWN_RIDX_CCK2		1
#define RTWN_RIDX_CCK55		2
#define RTWN_RIDX_CCK11		3
#define RTWN_RIDX_OFDM6		4
#define RTWN_RIDX_OFDM9		5
#define RTWN_RIDX_OFDM12	6
#define RTWN_RIDX_OFDM18	7
#define RTWN_RIDX_OFDM24	8
#define RTWN_RIDX_OFDM36	9
#define RTWN_RIDX_OFDM48	10
#define RTWN_RIDX_OFDM54	11

#define RTWN_RIDX_HT_MCS_SHIFT	12
#define RTWN_RIDX_HT_MCS(i)	(RTWN_RIDX_HT_MCS_SHIFT + (i))
#define RTWN_RIDX_TO_MCS(ridx)	((ridx) - RTWN_RIDX_HT_MCS_SHIFT)

/* HT supports up to MCS31, so goes from 12 -> 43 */

#define RTWN_RIDX_LEGACY_HT_COUNT	44

/*
 * VHT supports MCS0..9 for up to 4 spatial streams, so
 * goes from 44 -> 83.
 */
#define RTWN_RIDX_VHT_MCS_SHIFT	44
#define RTWN_RIDX_VHT_MCS(s, i)	(RTWN_RIDX_VHT_MCS_SHIFT + ((10*(s)) + (i)))

/*
 * The total amount of rate indexes, CCK, OFDM, HT MCS0..31,
 * VHT MCS0..9 for 1-4 streams.
 */
#define RTWN_RIDX_COUNT		84

#define RTWN_RIDX_UNKNOWN	(uint8_t)-1

#define RTWN_RATE_IS_CCK(rate)	((rate) <= RTWN_RIDX_CCK11)
#define RTWN_RATE_IS_OFDM(rate) \
	((rate) >= RTWN_RIDX_OFDM6 && (rate) <= RTWN_RIDX_OFDM54)
#define RTWN_RATE_IS_HT(rate) \
	((rate) >= RTWN_RIDX_HT_MCS_SHIFT && (rate) < RTWN_RIDX_VHT_MCS_SHIFT)
#define RTWN_RATE_IS_VHT(rate) \
	((rate) >= RTWN_RIDX_VHT_MCS_SHIFT && (rate) <= RTWN_RIDX_COUNT)

static const uint8_t ridx2rate[] =
	{ 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108 };

static __inline uint8_t
rate2ridx(uint8_t rate)
{
	if (rate & IEEE80211_RATE_MCS) {
		return ((rate & 0xf) + RTWN_RIDX_HT_MCS_SHIFT);
	}
	switch (rate) {
	/* 11g */
	case 12:	return (RTWN_RIDX_OFDM6);
	case 18:	return (RTWN_RIDX_OFDM9);
	case 24:	return (RTWN_RIDX_OFDM12);
	case 36:	return (RTWN_RIDX_OFDM18);
	case 48:	return (RTWN_RIDX_OFDM24);
	case 72:	return (RTWN_RIDX_OFDM36);
	case 96:	return (RTWN_RIDX_OFDM48);
	case 108:	return (RTWN_RIDX_OFDM54);
	/* 11b */
	case 2:		return (RTWN_RIDX_CCK1);
	case 4:		return (RTWN_RIDX_CCK2);
	case 11:	return (RTWN_RIDX_CCK55);
	case 22:	return (RTWN_RIDX_CCK11);
	default:
		printf("%s: called; unknown rate (%d)\n", __func__, rate);
		return (RTWN_RIDX_UNKNOWN);
	}
}

/* XXX move to net80211 */
static __inline__ uint8_t
rtwn_ctl_mcsrate(const struct ieee80211_rate_table *rt, uint8_t ridx)
{
	uint8_t cix, rate;

	/* Check if we are using MCS rate. */
	KASSERT(RTWN_RATE_IS_HT(ridx), ("bad mcs rate index %d", ridx));

	rate = RTWN_RIDX_TO_MCS(ridx) | IEEE80211_RATE_MCS;
	cix = rt->info[rt->rateCodeToIndex[rate]].ctlRateIndex;
	KASSERT(cix != (uint8_t)-1, ("rate %d (%d) has no info", rate, ridx));
	return (rt->info[cix].dot11Rate);
}

/* VHT version of rtwn_ctl_mcsrate */
/* XXX TODO: also should move this to net80211 */
static __inline__ uint8_t
rtwn_ctl_vhtrate(const struct ieee80211_rate_table *rt, uint8_t ridx)
{

	/* Check if we are using VHT MCS rate. */
	KASSERT(RTWN_RATE_IS_VHT(ridx), ("bad mcs rate index %d", ridx));

	/* TODO: there's no VHT tables, so for now just stick to OFDM12 */
	return (24);
}

#endif	/* IF_RTWN_RIDX_H */
