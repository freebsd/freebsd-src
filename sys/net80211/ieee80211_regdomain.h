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
 * $FreeBSD: src/sys/net80211/ieee80211_regdomain.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */
#ifndef _NET80211_IEEE80211_REGDOMAIN_H_
#define _NET80211_IEEE80211_REGDOMAIN_H_

/*
 * 802.11 regulatory domain definitions.
 */

/*
 * ISO 3166 Country/Region Codes
 * http://ftp.ics.uci.edu/pub/ietf/http/related/iso3166.txt
 */
enum ISOCountryCode {
	CTRY_AFGHANISTAN	= 4,
	CTRY_ALBANIA		= 8,	/* Albania */
	CTRY_ALGERIA		= 12,	/* Algeria */
	CTRY_AMERICAN_SAMOA	= 16,
	CTRY_ANDORRA		= 20,
	CTRY_ANGOLA		= 24,
	CTRY_ANGUILLA		= 660,
	/* XXX correct remainder */
	CTRY_ARGENTINA		= 32,	/* Argentina */
	CTRY_ARMENIA		= 51,	/* Armenia */
	CTRY_AUSTRALIA		= 36,	/* Australia */
	CTRY_AUSTRIA		= 40,	/* Austria */
	CTRY_AZERBAIJAN		= 31,	/* Azerbaijan */
	CTRY_BAHRAIN		= 48,	/* Bahrain */
	CTRY_BELARUS		= 112,	/* Belarus */
	CTRY_BELGIUM		= 56,	/* Belgium */
	CTRY_BELIZE		= 84,	/* Belize */
	CTRY_BOLIVIA		= 68,	/* Bolivia */
	CTRY_BRAZIL		= 76,	/* Brazil */
	CTRY_BRUNEI_DARUSSALAM	= 96,	/* Brunei Darussalam */
	CTRY_BULGARIA		= 100,	/* Bulgaria */
	CTRY_CANADA		= 124,	/* Canada */
	CTRY_CHILE		= 152,	/* Chile */
	CTRY_CHINA		= 156,	/* People's Republic of China */
	CTRY_COLOMBIA		= 170,	/* Colombia */
	CTRY_COSTA_RICA		= 188,	/* Costa Rica */
	CTRY_CROATIA		= 191,	/* Croatia */
	CTRY_CYPRUS		= 196,	/* Cyprus */
	CTRY_CZECH		= 203,	/* Czech Republic */
	CTRY_DENMARK		= 208,	/* Denmark */
	CTRY_DOMINICAN_REPUBLIC	= 214,	/* Dominican Republic */
	CTRY_ECUADOR		= 218,	/* Ecuador */
	CTRY_EGYPT		= 818,	/* Egypt */
	CTRY_EL_SALVADOR	= 222,	/* El Salvador */
	CTRY_ESTONIA		= 233,	/* Estonia */
	CTRY_FAEROE_ISLANDS	= 234,	/* Faeroe Islands */
	CTRY_FINLAND		= 246,	/* Finland */
	CTRY_FRANCE		= 250,	/* France */
	CTRY_FRANCE2		= 255,	/* France2 */
	CTRY_GEORGIA		= 268,	/* Georgia */
	CTRY_GERMANY		= 276,	/* Germany */
	CTRY_GREECE		= 300,	/* Greece */
	CTRY_GUATEMALA		= 320,	/* Guatemala */
	CTRY_HONDURAS		= 340,	/* Honduras */
	CTRY_HONG_KONG		= 344,	/* Hong Kong S.A.R., P.R.C. */
	CTRY_HUNGARY		= 348,	/* Hungary */
	CTRY_ICELAND		= 352,	/* Iceland */
	CTRY_INDIA		= 356,	/* India */
	CTRY_INDONESIA		= 360,	/* Indonesia */
	CTRY_IRAN		= 364,	/* Iran */
	CTRY_IRAQ		= 368,	/* Iraq */
	CTRY_IRELAND		= 372,	/* Ireland */
	CTRY_ISRAEL		= 376,	/* Israel */
	CTRY_ITALY		= 380,	/* Italy */
	CTRY_JAMAICA		= 388,	/* Jamaica */
	CTRY_JAPAN		= 392,	/* Japan */
	CTRY_JAPAN1		= 393,	/* Japan (JP1) */
	CTRY_JAPAN2		= 394,	/* Japan (JP0) */
	CTRY_JAPAN3		= 395,	/* Japan (JP1-1) */
	CTRY_JAPAN4		= 396,	/* Japan (JE1) */
	CTRY_JAPAN5		= 397,	/* Japan (JE2) */
	CTRY_JORDAN		= 400,	/* Jordan */
	CTRY_KAZAKHSTAN		= 398,	/* Kazakhstan */
	CTRY_KENYA		= 404,	/* Kenya */
	CTRY_KOREA_NORTH	= 408,	/* North Korea */
	CTRY_KOREA_ROC		= 410,	/* South Korea */
	CTRY_KOREA_ROC2		= 411,	/* South Korea */
	CTRY_KUWAIT		= 414,	/* Kuwait */
	CTRY_LATVIA		= 428,	/* Latvia */
	CTRY_LEBANON		= 422,	/* Lebanon */
	CTRY_LIBYA		= 434,	/* Libya */
	CTRY_LIECHTENSTEIN	= 438,	/* Liechtenstein */
	CTRY_LITHUANIA		= 440,	/* Lithuania */
	CTRY_LUXEMBOURG		= 442,	/* Luxembourg */
	CTRY_MACAU		= 446,	/* Macau */
	CTRY_MACEDONIA		= 807,	/* the Former Yugoslav Republic of Macedonia */
	CTRY_MALAYSIA		= 458,	/* Malaysia */
	CTRY_MEXICO		= 484,	/* Mexico */
	CTRY_MONACO		= 492,	/* Principality of Monaco */
	CTRY_MOROCCO		= 504,	/* Morocco */
	CTRY_NETHERLANDS	= 528,	/* Netherlands */
	CTRY_NEW_ZEALAND	= 554,	/* New Zealand */
	CTRY_NICARAGUA		= 558,	/* Nicaragua */
	CTRY_NORWAY		= 578,	/* Norway */
	CTRY_OMAN		= 512,	/* Oman */
	CTRY_PAKISTAN		= 586,	/* Islamic Republic of Pakistan */
	CTRY_PANAMA		= 591,	/* Panama */
	CTRY_PARAGUAY		= 600,	/* Paraguay */
	CTRY_PERU		= 604,	/* Peru */
	CTRY_PHILIPPINES	= 608,	/* Republic of the Philippines */
	CTRY_POLAND		= 616,	/* Poland */
	CTRY_PORTUGAL		= 620,	/* Portugal */
	CTRY_PUERTO_RICO	= 630,	/* Puerto Rico */
	CTRY_QATAR		= 634,	/* Qatar */
	CTRY_ROMANIA		= 642,	/* Romania */
	CTRY_RUSSIA		= 643,	/* Russia */
	CTRY_SAUDI_ARABIA	= 682,	/* Saudi Arabia */
	CTRY_SINGAPORE		= 702,	/* Singapore */
	CTRY_SLOVAKIA		= 703,	/* Slovak Republic */
	CTRY_SLOVENIA		= 705,	/* Slovenia */
	CTRY_SOUTH_AFRICA	= 710,	/* South Africa */
	CTRY_SPAIN		= 724,	/* Spain */
	CTRY_SWEDEN		= 752,	/* Sweden */
	CTRY_SWITZERLAND	= 756,	/* Switzerland */
	CTRY_SYRIA		= 760,	/* Syria */
	CTRY_TAIWAN		= 158,	/* Taiwan */
	CTRY_THAILAND		= 764,	/* Thailand */
	CTRY_TRINIDAD_Y_TOBAGO	= 780,	/* Trinidad y Tobago */
	CTRY_TUNISIA		= 788,	/* Tunisia */
	CTRY_TURKEY		= 792,	/* Turkey */
	CTRY_UAE		= 784,	/* U.A.E. */
	CTRY_UKRAINE		= 804,	/* Ukraine */
	CTRY_UNITED_KINGDOM	= 826,	/* United Kingdom */
	CTRY_UNITED_STATES	= 840,	/* United States */
	CTRY_URUGUAY		= 858,	/* Uruguay */
	CTRY_UZBEKISTAN		= 860,	/* Uzbekistan */
	CTRY_VENEZUELA		= 862,	/* Venezuela */
	CTRY_VIET_NAM		= 704,	/* Viet Nam */
	CTRY_YEMEN		= 887,	/* Yemen */
	CTRY_ZIMBABWE		= 716,	/* Zimbabwe */
};

#if defined(__KERNEL__) || defined(_KERNEL)
#define CTRY_DEBUG                0x1ff   /* debug */
#define CTRY_DEFAULT              0       /* default */

void	ieee80211_regdomain_attach(struct ieee80211com *);
void	ieee80211_regdomain_detach(struct ieee80211com *);

void	ieee80211_init_channels(struct ieee80211com *ic,
	    int rd, enum ISOCountryCode cc, int bands, int outdoor, int ecm);
uint8_t	*ieee80211_add_countryie(uint8_t *, struct ieee80211com *,
	    enum ISOCountryCode cc, int location);
const char *ieee80211_cctoiso(enum ISOCountryCode);
int	ieee80211_isotocc(const char iso[2]);
#endif /* defined(__KERNEL__) || defined(_KERNEL) */
#endif /* _NET80211_IEEE80211_REGDOMAIN_H_ */
