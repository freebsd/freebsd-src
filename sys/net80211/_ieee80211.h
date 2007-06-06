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
 *
 * $FreeBSD$
 */
#ifndef _NET80211__IEEE80211_H_
#define _NET80211__IEEE80211_H_

enum ieee80211_phytype {
	IEEE80211_T_DS,			/* direct sequence spread spectrum */
	IEEE80211_T_FH,			/* frequency hopping */
	IEEE80211_T_OFDM,		/* frequency division multiplexing */
	IEEE80211_T_TURBO,		/* high rate OFDM, aka turbo mode */
};
#define	IEEE80211_T_CCK	IEEE80211_T_DS	/* more common nomenclature */

/* XXX not really a mode; there are really multiple PHY's */
enum ieee80211_phymode {
	IEEE80211_MODE_AUTO	= 0,	/* autoselect */
	IEEE80211_MODE_11A	= 1,	/* 5GHz, OFDM */
	IEEE80211_MODE_11B	= 2,	/* 2GHz, CCK */
	IEEE80211_MODE_11G	= 3,	/* 2GHz, OFDM */
	IEEE80211_MODE_FH	= 4,	/* 2GHz, GFSK */
	IEEE80211_MODE_TURBO_A	= 5,	/* 5GHz, OFDM, 2x clock */
	IEEE80211_MODE_TURBO_G	= 6,	/* 2GHz, OFDM, 2x clock */
};
#define	IEEE80211_MODE_MAX	(IEEE80211_MODE_TURBO_G+1)

enum ieee80211_opmode {
	IEEE80211_M_STA		= 1,	/* infrastructure station */
	IEEE80211_M_IBSS 	= 0,	/* IBSS (adhoc) station */
	IEEE80211_M_AHDEMO	= 3,	/* Old lucent compatible adhoc demo */
	IEEE80211_M_HOSTAP	= 6,	/* Software Access Point */
	IEEE80211_M_MONITOR	= 8	/* Monitor mode */
};
#define	IEEE80211_OPMODE_MAX	(IEEE80211_M_MONITOR+1)

/*
 * 802.11g protection mode.
 */
enum ieee80211_protmode {
	IEEE80211_PROT_NONE	= 0,	/* no protection */
	IEEE80211_PROT_CTSONLY	= 1,	/* CTS to self */
	IEEE80211_PROT_RTSCTS	= 2,	/* RTS-CTS */
};

/*
 * Authentication mode.
 */
enum ieee80211_authmode {
	IEEE80211_AUTH_NONE	= 0,
	IEEE80211_AUTH_OPEN	= 1,		/* open */
	IEEE80211_AUTH_SHARED	= 2,		/* shared-key */
	IEEE80211_AUTH_8021X	= 3,		/* 802.1x */
	IEEE80211_AUTH_AUTO	= 4,		/* auto-select/accept */
	/* NB: these are used only for ioctls */
	IEEE80211_AUTH_WPA	= 5,		/* WPA/RSN w/ 802.1x/PSK */
};

/*
 * Roaming mode is effectively who controls the operation
 * of the 802.11 state machine when operating as a station.
 * State transitions are controlled either by the driver
 * (typically when management frames are processed by the
 * hardware/firmware), the host (auto/normal operation of
 * the 802.11 layer), or explicitly through ioctl requests
 * when applications like wpa_supplicant want control.
 */
enum ieee80211_roamingmode {
	IEEE80211_ROAMING_DEVICE= 0,	/* driver/hardware control */
	IEEE80211_ROAMING_AUTO	= 1,	/* 802.11 layer control */
	IEEE80211_ROAMING_MANUAL= 2,	/* application control */
};

/*
 * Channels are specified by frequency and attributes.
 */
struct ieee80211_channel {
	u_int16_t	ic_freq;	/* setting in Mhz */
	u_int16_t	ic_flags;	/* see below */
};

#define	IEEE80211_CHAN_MAX	255
#define	IEEE80211_CHAN_BYTES	32	/* howmany(IEEE80211_CHAN_MAX, NBBY) */
#define	IEEE80211_CHAN_ANY	0xffff	/* token for ``any channel'' */
#define	IEEE80211_CHAN_ANYC \
	((struct ieee80211_channel *) IEEE80211_CHAN_ANY)

/* bits 0-3 are for private use by drivers */
/* channel attributes */
#define	IEEE80211_CHAN_TURBO	0x0010	/* Turbo channel */
#define	IEEE80211_CHAN_CCK	0x0020	/* CCK channel */
#define	IEEE80211_CHAN_OFDM	0x0040	/* OFDM channel */
#define	IEEE80211_CHAN_2GHZ	0x0080	/* 2 GHz spectrum channel. */
#define	IEEE80211_CHAN_5GHZ	0x0100	/* 5 GHz spectrum channel */
#define	IEEE80211_CHAN_PASSIVE	0x0200	/* Only passive scan allowed */
#define	IEEE80211_CHAN_DYN	0x0400	/* Dynamic CCK-OFDM channel */
#define	IEEE80211_CHAN_GFSK	0x0800	/* GFSK channel (FHSS PHY) */
#define	IEEE80211_CHAN_GSM	0x1000	/* 900 MHz spectrum channel */
#define	IEEE80211_CHAN_HALF	0x4000	/* Half rate channel */
#define	IEEE80211_CHAN_QUARTER	0x8000	/* Quarter rate channel */

/*
 * Useful combinations of channel characteristics.
 */
#define	IEEE80211_CHAN_FHSS \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_GFSK)
#define	IEEE80211_CHAN_A \
	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM)
#define	IEEE80211_CHAN_B \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_CCK)
#define	IEEE80211_CHAN_PUREG \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM)
#define	IEEE80211_CHAN_G \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_DYN)
#define	IEEE80211_CHAN_T \
	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM | IEEE80211_CHAN_TURBO)
#define	IEEE80211_CHAN_108G \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM | IEEE80211_CHAN_TURBO)

#define	IEEE80211_CHAN_ALL \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_GFSK | \
	 IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM | IEEE80211_CHAN_DYN)
#define	IEEE80211_CHAN_ALLTURBO \
	(IEEE80211_CHAN_ALL | IEEE80211_CHAN_TURBO)

#define	IEEE80211_IS_CHAN_FHSS(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_FHSS) == IEEE80211_CHAN_FHSS)
#define	IEEE80211_IS_CHAN_A(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_A) == IEEE80211_CHAN_A)
#define	IEEE80211_IS_CHAN_B(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_B) == IEEE80211_CHAN_B)
#define	IEEE80211_IS_CHAN_PUREG(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_PUREG) == IEEE80211_CHAN_PUREG)
#define	IEEE80211_IS_CHAN_G(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_G) == IEEE80211_CHAN_G)
#define	IEEE80211_IS_CHAN_ANYG(_c) \
	(IEEE80211_IS_CHAN_PUREG(_c) || IEEE80211_IS_CHAN_G(_c))
#define	IEEE80211_IS_CHAN_T(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_T) == IEEE80211_CHAN_T)
#define	IEEE80211_IS_CHAN_108G(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_108G) == IEEE80211_CHAN_108G)

#define	IEEE80211_IS_CHAN_2GHZ(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_2GHZ) != 0)
#define	IEEE80211_IS_CHAN_5GHZ(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_5GHZ) != 0)
#define	IEEE80211_IS_CHAN_OFDM(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_OFDM) != 0)
#define	IEEE80211_IS_CHAN_CCK(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_CCK) != 0)
#define	IEEE80211_IS_CHAN_GFSK(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_GFSK) != 0)
#define	IEEE80211_IS_CHAN_HALF(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_HALF) != 0)
#define	IEEE80211_IS_CHAN_QUARTER(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_QUARTER) != 0)
#define	IEEE80211_IS_CHAN_FULL(_c) \
	(((_c)->ic_flags & (IEEE80211_CHAN_QUARTER | IEEE80211_CHAN_HALF)) == 0)
#define	IEEE80211_IS_CHAN_GSM(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_GSM) != 0)
#define	IEEE80211_IS_CHAN_PASSIVE(_c) \
	(((_c)->ic_flags & IEEE80211_CHAN_PASSIVE) != 0)

/* ni_chan encoding for FH phy */
#define	IEEE80211_FH_CHANMOD	80
#define	IEEE80211_FH_CHAN(set,pat)	(((set)-1)*IEEE80211_FH_CHANMOD+(pat))
#define	IEEE80211_FH_CHANSET(chan)	((chan)/IEEE80211_FH_CHANMOD+1)
#define	IEEE80211_FH_CHANPAT(chan)	((chan)%IEEE80211_FH_CHANMOD)

/*
 * 802.11 rate set.
 */
#define	IEEE80211_RATE_SIZE	8		/* 802.11 standard */
#define	IEEE80211_RATE_MAXSIZE	15		/* max rates we'll handle */

struct ieee80211_rateset {
	u_int8_t		rs_nrates;
	u_int8_t		rs_rates[IEEE80211_RATE_MAXSIZE];
};

#endif /* _NET80211__IEEE80211_H_ */
