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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * IEEE 802.11 regdomain support.
 */

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/kernel.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_regdomain.h>

void
ieee80211_regdomain_attach(struct ieee80211com *ic)
{
	ic->ic_regdomain = 0;			/* XXX */
	ic->ic_countrycode = CTRY_UNITED_STATES;/* XXX */
	ic->ic_location = 1+2;			/* both */
}

void
ieee80211_regdomain_detach(struct ieee80211com *ic)
{
}

static void
addchan(struct ieee80211com *ic, int ieee, int flags)
{
	struct ieee80211_channel *c;

	c = &ic->ic_channels[ic->ic_nchans++];
	c->ic_freq = ieee80211_ieee2mhz(ieee, flags);
	c->ic_ieee = ieee;
	c->ic_flags = flags;
}

/*
 * Setup the channel list for the specified regulatory domain,
 * country code, and operating modes.  This interface is used
 * when a driver does not obtain the channel list from another
 * source (such as firmware).
 */
void
ieee80211_init_channels(struct ieee80211com *ic,
	int rd, enum ISOCountryCode cc, int bands, int outdoor, int ecm)
{
	int i;

	/* XXX just do something for now */
	ic->ic_nchans = 0;
	if (isset(&bands, IEEE80211_MODE_11B) ||
	    isset(&bands, IEEE80211_MODE_11G)) {
		for (i = 1; i <= (ecm ? 14 : 11); i++) {
			if (isset(&bands, IEEE80211_MODE_11B))
				addchan(ic, i, IEEE80211_CHAN_B);
			if (isset(&bands, IEEE80211_MODE_11G))
				addchan(ic, i, IEEE80211_CHAN_G);
		}
	}
	if (isset(&bands, IEEE80211_MODE_11A)) {
		for (i = 36; i <= 64; i += 4)
			addchan(ic, i, IEEE80211_CHAN_A);
		for (i = 100; i <= 140; i += 4)
			addchan(ic, i, IEEE80211_CHAN_A);
		for (i = 149; i <= 161; i += 4)
			addchan(ic, i, IEEE80211_CHAN_A);
	}
	ic->ic_regdomain = rd;
	ic->ic_countrycode = cc;
	ic->ic_location = outdoor;
}

/*
 * Add Country Information IE.
 */
uint8_t *
ieee80211_add_countryie(uint8_t *frm, struct ieee80211com *ic,
	enum ISOCountryCode cc, int location)
{
#define	CHAN_UNINTERESTING \
    (IEEE80211_CHAN_TURBO | IEEE80211_CHAN_STURBO | \
     IEEE80211_CHAN_HALF | IEEE80211_CHAN_QUARTER)
	/* XXX what about auto? */
	/* flag set of channels to be excluded */
	static const int skipflags[IEEE80211_MODE_MAX] = {
	    CHAN_UNINTERESTING,				/* MODE_AUTO */
	    CHAN_UNINTERESTING | IEEE80211_CHAN_2GHZ,	/* MODE_11A */
	    CHAN_UNINTERESTING | IEEE80211_CHAN_5GHZ,	/* MODE_11B */
	    CHAN_UNINTERESTING | IEEE80211_CHAN_5GHZ,	/* MODE_11G */
	    CHAN_UNINTERESTING | IEEE80211_CHAN_OFDM |	/* MODE_FH */
	        IEEE80211_CHAN_CCK | IEEE80211_CHAN_DYN,
	    CHAN_UNINTERESTING | IEEE80211_CHAN_2GHZ,	/* MODE_TURBO_A */
	    CHAN_UNINTERESTING | IEEE80211_CHAN_5GHZ,	/* MODE_TURBO_G */
	    CHAN_UNINTERESTING | IEEE80211_CHAN_2GHZ,	/* MODE_STURBO_A */
	    CHAN_UNINTERESTING | IEEE80211_CHAN_2GHZ,	/* MODE_11NA */
	    CHAN_UNINTERESTING | IEEE80211_CHAN_5GHZ,	/* MODE_11NG */
	};
	struct ieee80211_country_ie *ie = (struct ieee80211_country_ie *)frm;
	const char *iso_name;
	uint8_t nextchan, chans[IEEE80211_CHAN_BYTES];
	int i, skip;

	ie->ie = IEEE80211_ELEMID_COUNTRY;
	iso_name = ieee80211_cctoiso(cc);
	if (iso_name == NULL) {
		if_printf(ic->ic_ifp, "bad country code %d ignored\n", cc);
		iso_name = "  ";
	}
	ie->cc[0] = iso_name[0];
	ie->cc[1] = iso_name[1];
	/* 
	 * Indoor/Outdoor portion of country string.
	 * NB: this is not quite right, since we should have one of:
	 *     'I' indoor only
	 *     'O' outdoor only
	 *     ' ' all enviroments
	 */
	ie->cc[2] = ((location & 3) == 3 ? ' ' : location & 1 ? 'I' : 'O');

	/* 
	 * Run-length encoded channel+max tx power info.
	 */
	frm = (uint8_t *)&ie->band[0];
	nextchan = 0;			/* NB: impossible channel # */
	memset(chans, 0, sizeof(chans));
	skip = skipflags[ic->ic_curmode];
	for (i = 0; i < ic->ic_nchans; i++) {
		const struct ieee80211_channel *c = &ic->ic_channels[i];

		if (isset(chans, c->ic_ieee))		/* suppress dup's */
			continue;
		if ((c->ic_flags & skip) == 0)		/* skip band, etc. */
			continue;
		setbit(chans, c->ic_ieee);
		if (c->ic_ieee != nextchan ||
		    c->ic_maxregpower != frm[-1]) {	/* new run */
			/* XXX max of 83 runs */
			frm[0] = c->ic_ieee;		/* starting channel # */
			frm[1] = 1;			/* # channels in run */
			frm[2] = c->ic_maxregpower;	/* tx power cap */
			frm += 3;
			nextchan = c->ic_ieee + 1;	/* overflow? */
		} else {				/* extend run */
			frm[-2]++;
			nextchan++;
		}
	}
	ie->len = frm - ie->cc;
	if (ie->len & 1)		/* pad to multiple of 2 */
		ie->len++;
	return frm;
#undef CHAN_UNINTERESTING
}

/*
 * Country Code Table for code-to-string conversion.
 */
static const struct {
	enum ISOCountryCode iso_code;	   
	const char*	iso_name;
} country_strings[] = {
    { CTRY_DEBUG,	 	"DB" },		/* NB: nonstandard */
    { CTRY_DEFAULT,	 	"NA" },		/* NB: nonstandard */
    { CTRY_ALBANIA,		"AL" },
    { CTRY_ALGERIA,		"DZ" },
    { CTRY_ARGENTINA,		"AR" },
    { CTRY_ARMENIA,		"AM" },
    { CTRY_AUSTRALIA,		"AU" },
    { CTRY_AUSTRIA,		"AT" },
    { CTRY_AZERBAIJAN,		"AZ" },
    { CTRY_BAHRAIN,		"BH" },
    { CTRY_BELARUS,		"BY" },
    { CTRY_BELGIUM,		"BE" },
    { CTRY_BELIZE,		"BZ" },
    { CTRY_BOLIVIA,		"BO" },
    { CTRY_BRAZIL,		"BR" },
    { CTRY_BRUNEI_DARUSSALAM,	"BN" },
    { CTRY_BULGARIA,		"BG" },
    { CTRY_CANADA,		"CA" },
    { CTRY_CHILE,		"CL" },
    { CTRY_CHINA,		"CN" },
    { CTRY_COLOMBIA,		"CO" },
    { CTRY_COSTA_RICA,		"CR" },
    { CTRY_CROATIA,		"HR" },
    { CTRY_CYPRUS,		"CY" },
    { CTRY_CZECH,		"CZ" },
    { CTRY_DENMARK,		"DK" },
    { CTRY_DOMINICAN_REPUBLIC,	"DO" },
    { CTRY_ECUADOR,		"EC" },
    { CTRY_EGYPT,		"EG" },
    { CTRY_EL_SALVADOR,		"SV" },    
    { CTRY_ESTONIA,		"EE" },
    { CTRY_FINLAND,		"FI" },
    { CTRY_FRANCE,		"FR" },
    { CTRY_FRANCE2,		"F2" },
    { CTRY_GEORGIA,		"GE" },
    { CTRY_GERMANY,		"DE" },
    { CTRY_GREECE,		"GR" },
    { CTRY_GUATEMALA,		"GT" },
    { CTRY_HONDURAS,		"HN" },
    { CTRY_HONG_KONG,		"HK" },
    { CTRY_HUNGARY,		"HU" },
    { CTRY_ICELAND,		"IS" },
    { CTRY_INDIA,		"IN" },
    { CTRY_INDONESIA,		"ID" },
    { CTRY_IRAN,		"IR" },
    { CTRY_IRELAND,		"IE" },
    { CTRY_ISRAEL,		"IL" },
    { CTRY_ITALY,		"IT" },
    { CTRY_JAMAICA,		"JM" },
    { CTRY_JAPAN,		"JP" },
    { CTRY_JAPAN1,		"J1" },
    { CTRY_JAPAN2,		"J2" },    
    { CTRY_JAPAN3,		"J3" },
    { CTRY_JAPAN4,		"J4" },
    { CTRY_JAPAN5,		"J5" },    
    { CTRY_JORDAN,		"JO" },
    { CTRY_KAZAKHSTAN,		"KZ" },
    { CTRY_KOREA_NORTH,		"KP" },
    { CTRY_KOREA_ROC,		"KR" },
    { CTRY_KOREA_ROC2,		"K2" },
    { CTRY_KUWAIT,		"KW" },
    { CTRY_LATVIA,		"LV" },
    { CTRY_LEBANON,		"LB" },
    { CTRY_LIECHTENSTEIN,	"LI" },
    { CTRY_LITHUANIA,		"LT" },
    { CTRY_LUXEMBOURG,		"LU" },
    { CTRY_MACAU,		"MO" },
    { CTRY_MACEDONIA,		"MK" },
    { CTRY_MALAYSIA,		"MY" },
    { CTRY_MEXICO,		"MX" },
    { CTRY_MONACO,		"MC" },
    { CTRY_MOROCCO,		"MA" },
    { CTRY_NETHERLANDS,		"NL" },
    { CTRY_NEW_ZEALAND,		"NZ" },
    { CTRY_NORWAY,		"NO" },
    { CTRY_OMAN,		"OM" },
    { CTRY_PAKISTAN,		"PK" },
    { CTRY_PANAMA,		"PA" },
    { CTRY_PERU,		"PE" },
    { CTRY_PHILIPPINES,		"PH" },
    { CTRY_POLAND,		"PL" },
    { CTRY_PORTUGAL,		"PT" },
    { CTRY_PUERTO_RICO,		"PR" },
    { CTRY_QATAR,		"QA" },
    { CTRY_ROMANIA,		"RO" },
    { CTRY_RUSSIA,		"RU" },
    { CTRY_SAUDI_ARABIA,	"SA" },
    { CTRY_SINGAPORE,		"SG" },
    { CTRY_SLOVAKIA,		"SK" },
    { CTRY_SLOVENIA,		"SI" },
    { CTRY_SOUTH_AFRICA,	"ZA" },
    { CTRY_SPAIN,		"ES" },
    { CTRY_SWEDEN,		"SE" },
    { CTRY_SWITZERLAND,		"CH" },
    { CTRY_SYRIA,		"SY" },
    { CTRY_TAIWAN,		"TW" },
    { CTRY_THAILAND,		"TH" },
    { CTRY_TRINIDAD_Y_TOBAGO,	"TT" },
    { CTRY_TUNISIA,		"TN" },
    { CTRY_TURKEY,		"TR" },
    { CTRY_UKRAINE,		"UA" },
    { CTRY_UAE,			"AE" },
    { CTRY_UNITED_KINGDOM,	"GB" },
    { CTRY_UNITED_STATES,	"US" },
    { CTRY_URUGUAY,		"UY" },
    { CTRY_UZBEKISTAN,		"UZ" },    
    { CTRY_VENEZUELA,		"VE" },
    { CTRY_VIET_NAM,		"VN" },
    { CTRY_YEMEN,		"YE" },
    { CTRY_ZIMBABWE,		"ZW" }    
};

const char *
ieee80211_cctoiso(enum ISOCountryCode cc)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(country_strings); i++) {
		if (country_strings[i].iso_code == cc)
			return country_strings[i].iso_name;
	}
	return NULL;
#undef N
}

int
ieee80211_isotocc(const char iso[2])
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	int i;

	for (i = 0; i < N(country_strings); i++) {
		if (country_strings[i].iso_name[0] == iso[0] &&
		    country_strings[i].iso_name[1] == iso[1])
			return country_strings[i].iso_code;
	}
	return -1;
#undef N
}
