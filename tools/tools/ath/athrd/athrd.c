/*-
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */
#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_eeprom_v3.h"		/* XXX */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#define	IEEE80211_CHAN_MAX	255
#define IEEE80211_REGCLASSIDS_MAX	10

int		ath_hal_debug = 0;
HAL_CTRY_CODE	cc = CTRY_DEFAULT;
HAL_REG_DOMAIN	rd = 169;		/* FCC */
HAL_BOOL	outdoor = AH_TRUE;
HAL_BOOL	Amode = 1;
HAL_BOOL	Bmode = 1;
HAL_BOOL	Gmode = 1;
HAL_BOOL	HT20mode = 1;
HAL_BOOL	HT40mode = 1;
HAL_BOOL	turbo5Disable = AH_FALSE;
HAL_BOOL	turbo2Disable = AH_FALSE;

u_int16_t	_numCtls = 8;
u_int16_t	_ctl[32] =
	{ 0x10, 0x13, 0x40, 0x30, 0x11, 0x31, 0x12, 0x32 };
RD_EDGES_POWER	_rdEdgesPower[NUM_EDGES*NUM_CTLS] = {
	{ 5180, 28, 0 },	/* 0x10 */
	{ 5240, 60, 0 },
	{ 5260, 36, 0 },
	{ 5320, 27, 0 },
	{ 5745, 36, 0 },
	{ 5765, 36, 0 },
	{ 5805, 36, 0 },
	{ 5825, 36, 0 },

	{ 5210, 28, 0 },	/* 0x13 */
	{ 5250, 28, 0 },
	{ 5290, 30, 0 },
	{ 5760, 36, 0 },
	{ 5800, 36, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },

	{ 5170, 60, 0 },	/* 0x40 */
	{ 5230, 60, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },

	{ 5180, 33, 0 },	/* 0x30 */
	{ 5320, 33, 0 },
	{ 5500, 34, 0 },
	{ 5700, 34, 0 },
	{ 5745, 35, 0 },
	{ 5765, 35, 0 },
	{ 5785, 35, 0 },
	{ 5825, 35, 0 },

	{ 2412, 36, 0 },	/* 0x11 */
	{ 2417, 36, 0 },
	{ 2422, 36, 0 },
	{ 2432, 36, 0 },
	{ 2442, 36, 0 },
	{ 2457, 36, 0 },
	{ 2467, 36, 0 },
	{ 2472, 36, 0 },

	{ 2412, 36, 0 },	/* 0x31 */
	{ 2417, 36, 0 },
	{ 2422, 36, 0 },
	{ 2432, 36, 0 },
	{ 2442, 36, 0 },
	{ 2457, 36, 0 },
	{ 2467, 36, 0 },
	{ 2472, 36, 0 },

	{ 2412, 36, 0 },	/* 0x12 */
	{ 2417, 36, 0 },
	{ 2422, 36, 0 },
	{ 2432, 36, 0 },
	{ 2442, 36, 0 },
	{ 2457, 36, 0 },
	{ 2467, 36, 0 },
	{ 2472, 36, 0 },

	{ 2412, 28, 0 },	/* 0x32 */
	{ 2417, 28, 0 },
	{ 2422, 28, 0 },
	{ 2432, 28, 0 },
	{ 2442, 28, 0 },
	{ 2457, 28, 0 },
	{ 2467, 28, 0 },
	{ 2472, 28, 0 },
};

u_int16_t	turbo2WMaxPower5 = 32;
u_int16_t	turbo2WMaxPower2;
int8_t		antennaGainMax[2] = { 0, 0 };	/* XXX */
int		eeversion = AR_EEPROM_VER3_1;
TRGT_POWER_ALL_MODES tpow = {
	8, {
	    { 22, 24, 28, 32, 5180 },
	    { 22, 24, 28, 32, 5200 },
	    { 22, 24, 28, 32, 5320 },
	    { 26, 30, 34, 34, 5500 },
	    { 26, 30, 34, 34, 5700 },
	    { 20, 30, 34, 36, 5745 },
	    { 20, 30, 34, 36, 5825 },
	    { 20, 30, 34, 36, 5850 },
	},
	2, {
	    { 23, 27, 31, 34, 2412 },
	    { 23, 27, 31, 34, 2447 },
	},
	2, {
	    { 36, 36, 36, 36, 2412 },
	    { 36, 36, 36, 36, 2484 },
	}
};
#define	numTargetPwr_11a	tpow.numTargetPwr_11a
#define	trgtPwr_11a		tpow.trgtPwr_11a
#define	numTargetPwr_11g	tpow.numTargetPwr_11g
#define	trgtPwr_11g		tpow.trgtPwr_11g
#define	numTargetPwr_11b	tpow.numTargetPwr_11b
#define	trgtPwr_11b		tpow.trgtPwr_11b

static HAL_BOOL
getChannelEdges(struct ath_hal *ah, u_int16_t flags, u_int16_t *low, u_int16_t *high)
{
	struct ath_hal_private *ahp = AH_PRIVATE(ah);
	HAL_CAPABILITIES *pCap = &ahp->ah_caps;

	if (flags & CHANNEL_5GHZ) {
		*low = pCap->halLow5GhzChan;
		*high = pCap->halHigh5GhzChan;
		return AH_TRUE;
	}
	if (flags & CHANNEL_2GHZ) {
		*low = pCap->halLow2GhzChan;
		*high = pCap->halHigh2GhzChan;
		return AH_TRUE;
	}
	return AH_FALSE;
}

static u_int
getWirelessModes(struct ath_hal *ah)
{
	u_int mode = 0;

	if (Amode) {
		mode = HAL_MODE_11A;
		if (!turbo5Disable)
			mode |= HAL_MODE_TURBO;
	}
	if (Bmode)
		mode |= HAL_MODE_11B;
	if (Gmode) {
		mode |= HAL_MODE_11G;
		if (!turbo2Disable) 
			mode |= HAL_MODE_108G;
	}
	if (HT20mode)
		mode |= HAL_MODE_11NG_HT20|HAL_MODE_11NA_HT20;
	if (HT40mode)
		mode |= HAL_MODE_11NG_HT40PLUS|HAL_MODE_11NA_HT40PLUS
		     |  HAL_MODE_11NG_HT40MINUS|HAL_MODE_11NA_HT40MINUS
		     ;
	return mode;
}

/*
 * Country/Region Codes from MS WINNLS.H
 * Numbering from ISO 3166
 */
enum CountryCode {
    CTRY_ALBANIA              = 8,       /* Albania */
    CTRY_ALGERIA              = 12,      /* Algeria */
    CTRY_ARGENTINA            = 32,      /* Argentina */
    CTRY_ARMENIA              = 51,      /* Armenia */
    CTRY_AUSTRALIA            = 36,      /* Australia */
    CTRY_AUSTRIA              = 40,      /* Austria */
    CTRY_AZERBAIJAN           = 31,      /* Azerbaijan */
    CTRY_BAHRAIN              = 48,      /* Bahrain */
    CTRY_BELARUS              = 112,     /* Belarus */
    CTRY_BELGIUM              = 56,      /* Belgium */
    CTRY_BELIZE               = 84,      /* Belize */
    CTRY_BOLIVIA              = 68,      /* Bolivia */
    CTRY_BRAZIL               = 76,      /* Brazil */
    CTRY_BRUNEI_DARUSSALAM    = 96,      /* Brunei Darussalam */
    CTRY_BULGARIA             = 100,     /* Bulgaria */
    CTRY_CANADA               = 124,     /* Canada */
    CTRY_CHILE                = 152,     /* Chile */
    CTRY_CHINA                = 156,     /* People's Republic of China */
    CTRY_COLOMBIA             = 170,     /* Colombia */
    CTRY_COSTA_RICA           = 188,     /* Costa Rica */
    CTRY_CROATIA              = 191,     /* Croatia */
    CTRY_CYPRUS               = 196,
    CTRY_CZECH                = 203,     /* Czech Republic */
    CTRY_DENMARK              = 208,     /* Denmark */
    CTRY_DOMINICAN_REPUBLIC   = 214,     /* Dominican Republic */
    CTRY_ECUADOR              = 218,     /* Ecuador */
    CTRY_EGYPT                = 818,     /* Egypt */
    CTRY_EL_SALVADOR          = 222,     /* El Salvador */
    CTRY_ESTONIA              = 233,     /* Estonia */
    CTRY_FAEROE_ISLANDS       = 234,     /* Faeroe Islands */
    CTRY_FINLAND              = 246,     /* Finland */
    CTRY_FRANCE               = 250,     /* France */
    CTRY_FRANCE2              = 255,     /* France2 */
    CTRY_GEORGIA              = 268,     /* Georgia */
    CTRY_GERMANY              = 276,     /* Germany */
    CTRY_GREECE               = 300,     /* Greece */
    CTRY_GSM                  = 843,     /* 900MHz/GSM */
    CTRY_GUATEMALA            = 320,     /* Guatemala */
    CTRY_HONDURAS             = 340,     /* Honduras */
    CTRY_HONG_KONG            = 344,     /* Hong Kong S.A.R., P.R.C. */
    CTRY_HUNGARY              = 348,     /* Hungary */
    CTRY_ICELAND              = 352,     /* Iceland */
    CTRY_INDIA                = 356,     /* India */
    CTRY_INDONESIA            = 360,     /* Indonesia */
    CTRY_IRAN                 = 364,     /* Iran */
    CTRY_IRAQ                 = 368,     /* Iraq */
    CTRY_IRELAND              = 372,     /* Ireland */
    CTRY_ISRAEL               = 376,     /* Israel */
    CTRY_ITALY                = 380,     /* Italy */
    CTRY_JAMAICA              = 388,     /* Jamaica */
    CTRY_JAPAN                = 392,     /* Japan */
    CTRY_JAPAN1               = 393,     /* Japan (JP1) */
    CTRY_JAPAN2               = 394,     /* Japan (JP0) */
    CTRY_JAPAN3               = 395,     /* Japan (JP1-1) */
    CTRY_JAPAN4               = 396,     /* Japan (JE1) */
    CTRY_JAPAN5               = 397,     /* Japan (JE2) */
    CTRY_JAPAN6               = 399,     /* Japan (JP6) */

    CTRY_JAPAN7		      = 4007,	 /* Japan (J7) */
    CTRY_JAPAN8		      = 4008,	 /* Japan (J8) */
    CTRY_JAPAN9		      = 4009,	 /* Japan (J9) */

    CTRY_JAPAN10	      = 4010,	 /* Japan (J10) */
    CTRY_JAPAN11	      = 4011,	 /* Japan (J11) */
    CTRY_JAPAN12	      = 4012,	 /* Japan (J12) */

    CTRY_JAPAN13	      = 4013,	 /* Japan (J13) */
    CTRY_JAPAN14	      = 4014,	 /* Japan (J14) */
    CTRY_JAPAN15	      = 4015,	 /* Japan (J15) */

    CTRY_JAPAN16	      = 4016,	 /* Japan (J16) */
    CTRY_JAPAN17	      = 4017,	 /* Japan (J17) */
    CTRY_JAPAN18	      = 4018,	 /* Japan (J18) */

    CTRY_JAPAN19	      = 4019,	 /* Japan (J19) */
    CTRY_JAPAN20	      = 4020,	 /* Japan (J20) */
    CTRY_JAPAN21	      = 4021,	 /* Japan (J21) */

    CTRY_JAPAN22	      = 4022,	 /* Japan (J22) */
    CTRY_JAPAN23	      = 4023,	 /* Japan (J23) */
    CTRY_JAPAN24	      = 4024,	 /* Japan (J24) */
 
    CTRY_JORDAN               = 400,     /* Jordan */
    CTRY_KAZAKHSTAN           = 398,     /* Kazakhstan */
    CTRY_KENYA                = 404,     /* Kenya */
    CTRY_KOREA_NORTH          = 408,     /* North Korea */
    CTRY_KOREA_ROC            = 410,     /* South Korea */
    CTRY_KOREA_ROC2           = 411,     /* South Korea */
    CTRY_KOREA_ROC3           = 412,     /* South Korea */
    CTRY_KUWAIT               = 414,     /* Kuwait */
    CTRY_LATVIA               = 428,     /* Latvia */
    CTRY_LEBANON              = 422,     /* Lebanon */
    CTRY_LIBYA                = 434,     /* Libya */
    CTRY_LIECHTENSTEIN        = 438,     /* Liechtenstein */
    CTRY_LITHUANIA            = 440,     /* Lithuania */
    CTRY_LUXEMBOURG           = 442,     /* Luxembourg */
    CTRY_MACAU                = 446,     /* Macau */
    CTRY_MACEDONIA            = 807,     /* the Former Yugoslav Republic of Macedonia */
    CTRY_MALAYSIA             = 458,     /* Malaysia */
    CTRY_MALTA		          = 470,	 /* Malta */
    CTRY_MEXICO               = 484,     /* Mexico */
    CTRY_MONACO               = 492,     /* Principality of Monaco */
    CTRY_MOROCCO              = 504,     /* Morocco */
    CTRY_NETHERLANDS          = 528,     /* Netherlands */
    CTRY_NEW_ZEALAND          = 554,     /* New Zealand */
    CTRY_NICARAGUA            = 558,     /* Nicaragua */
    CTRY_NORWAY               = 578,     /* Norway */
    CTRY_OMAN                 = 512,     /* Oman */
    CTRY_PAKISTAN             = 586,     /* Islamic Republic of Pakistan */
    CTRY_PANAMA               = 591,     /* Panama */
    CTRY_PARAGUAY             = 600,     /* Paraguay */
    CTRY_PERU                 = 604,     /* Peru */
    CTRY_PHILIPPINES          = 608,     /* Republic of the Philippines */
    CTRY_POLAND               = 616,     /* Poland */
    CTRY_PORTUGAL             = 620,     /* Portugal */
    CTRY_PUERTO_RICO          = 630,     /* Puerto Rico */
    CTRY_QATAR                = 634,     /* Qatar */
    CTRY_ROMANIA              = 642,     /* Romania */
    CTRY_RUSSIA               = 643,     /* Russia */
    CTRY_SAUDI_ARABIA         = 682,     /* Saudi Arabia */
    CTRY_SINGAPORE            = 702,     /* Singapore */
    CTRY_SLOVAKIA             = 703,     /* Slovak Republic */
    CTRY_SLOVENIA             = 705,     /* Slovenia */
    CTRY_SOUTH_AFRICA         = 710,     /* South Africa */
    CTRY_SPAIN                = 724,     /* Spain */
    CTRY_SWEDEN               = 752,     /* Sweden */
    CTRY_SWITZERLAND          = 756,     /* Switzerland */
    CTRY_SYRIA                = 760,     /* Syria */
    CTRY_TAIWAN               = 158,     /* Taiwan */
    CTRY_THAILAND             = 764,     /* Thailand */
    CTRY_TRINIDAD_Y_TOBAGO    = 780,     /* Trinidad y Tobago */
    CTRY_TUNISIA              = 788,     /* Tunisia */
    CTRY_TURKEY               = 792,     /* Turkey */
    CTRY_UAE                  = 784,     /* U.A.E. */
    CTRY_UKRAINE              = 804,     /* Ukraine */
    CTRY_UNITED_KINGDOM       = 826,     /* United Kingdom */
    CTRY_UNITED_STATES        = 840,     /* United States */
    CTRY_UNITED_STATES_FCC49  = 842,     /* United States (Public Safety)*/
    CTRY_URUGUAY              = 858,     /* Uruguay */
    CTRY_UZBEKISTAN           = 860,     /* Uzbekistan */
    CTRY_VENEZUELA            = 862,     /* Venezuela */
    CTRY_VIET_NAM             = 704,     /* Viet Nam */
    CTRY_YEMEN                = 887,     /* Yemen */
    CTRY_ZIMBABWE             = 716      /* Zimbabwe */
};


/* Enumerated Regulatory Domain Information 8 bit values indicate that
 * the regdomain is really a pair of unitary regdomains.  12 bit values
 * are the real unitary regdomains and are the only ones which have the
 * frequency bitmasks and flags set.
 */

enum EnumRd {
	/*
	 * The following regulatory domain definitions are
	 * found in the EEPROM. Each regulatory domain
	 * can operate in either a 5GHz or 2.4GHz wireless mode or
	 * both 5GHz and 2.4GHz wireless modes.
	 * In general, the value holds no special
	 * meaning and is used to decode into either specific
	 * 2.4GHz or 5GHz wireless mode for that particular
	 * regulatory domain.
	 */
	NO_ENUMRD	= 0x00,
	NULL1_WORLD	= 0x03,		/* For 11b-only countries (no 11a allowed) */
	NULL1_ETSIB	= 0x07,		/* Israel */
	NULL1_ETSIC	= 0x08,
	NULL1_GSM	= 0x09,		/* GSM-only operation */
	FCC1_FCCA	= 0x10,		/* USA */
	FCC1_WORLD	= 0x11,		/* Hong Kong */
	FCC4_FCCA	= 0x12,		/* USA - Public Safety */

	FCC2_FCCA	= 0x20,		/* Canada */
	FCC2_WORLD	= 0x21,		/* Australia & HK */
	FCC2_ETSIC	= 0x22,
	FRANCE_RES	= 0x31,		/* Legacy France for OEM */
	FCC3_FCCA	= 0x3A,		/* USA & Canada w/5470 band, 11h, DFS enabled */
	FCC3_WORLD  = 0x3B,     /* USA & Canada w/5470 band, 11h, DFS enabled */

	ETSI1_WORLD	= 0x37,
	ETSI3_ETSIA	= 0x32,		/* France (optional) */
	ETSI2_WORLD	= 0x35,		/* Hungary & others */
	ETSI3_WORLD	= 0x36,		/* France & others */
	ETSI4_WORLD	= 0x30,
	ETSI4_ETSIC	= 0x38,
	ETSI5_WORLD	= 0x39,
	ETSI6_WORLD	= 0x34,		/* Bulgaria */
	ETSI_RESERVED	= 0x33,		/* Reserved (Do not used) */

	MKK1_MKKA	= 0x40,		/* Japan (JP1) */
	MKK1_MKKB	= 0x41,		/* Japan (JP0) */
	APL4_WORLD	= 0x42,		/* Singapore */
	MKK2_MKKA	= 0x43,		/* Japan with 4.9G channels */
	APL_RESERVED	= 0x44,		/* Reserved (Do not used)  */
	APL2_WORLD	= 0x45,		/* Korea */
	APL2_APLC	= 0x46,
	APL3_WORLD	= 0x47,
	MKK1_FCCA	= 0x48,		/* Japan (JP1-1) */
	APL2_APLD	= 0x49,		/* Korea with 2.3G channels */
	MKK1_MKKA1	= 0x4A,		/* Japan (JE1) */
	MKK1_MKKA2	= 0x4B,		/* Japan (JE2) */
	MKK1_MKKC	= 0x4C,		/* Japan (MKK1_MKKA,except Ch14) */

	APL3_FCCA   = 0x50,
	APL1_WORLD	= 0x52,		/* Latin America */
	APL1_FCCA	= 0x53,
	APL1_APLA	= 0x54,
	APL1_ETSIC	= 0x55,
	APL2_ETSIC	= 0x56,		/* Venezuela */
	APL5_WORLD	= 0x58,		/* Chile */
	APL6_WORLD	= 0x5B,		/* Singapore */
	APL7_FCCA   = 0x5C,     /* Taiwan 5.47 Band */
	APL8_WORLD  = 0x5D,     /* Malaysia 5GHz */
	APL9_WORLD  = 0x5E,     /* Korea 5GHz */

	/*
	 * World mode SKUs
	 */
	WOR0_WORLD	= 0x60,		/* World0 (WO0 SKU) */
	WOR1_WORLD	= 0x61,		/* World1 (WO1 SKU) */
	WOR2_WORLD	= 0x62,		/* World2 (WO2 SKU) */
	WOR3_WORLD	= 0x63,		/* World3 (WO3 SKU) */
	WOR4_WORLD	= 0x64,		/* World4 (WO4 SKU) */	
	WOR5_ETSIC	= 0x65,		/* World5 (WO5 SKU) */    

	WOR01_WORLD	= 0x66,		/* World0-1 (WW0-1 SKU) */
	WOR02_WORLD	= 0x67,		/* World0-2 (WW0-2 SKU) */
	EU1_WORLD	= 0x68,		/* Same as World0-2 (WW0-2 SKU), except active scan ch1-13. No ch14 */

	WOR9_WORLD	= 0x69,		/* World9 (WO9 SKU) */	
	WORA_WORLD	= 0x6A,		/* WorldA (WOA SKU) */	

	MKK3_MKKB	= 0x80,		/* Japan UNI-1 even + MKKB */
	MKK3_MKKA2	= 0x81,		/* Japan UNI-1 even + MKKA2 */
	MKK3_MKKC	= 0x82,		/* Japan UNI-1 even + MKKC */

	MKK4_MKKB	= 0x83,		/* Japan UNI-1 even + UNI-2 + MKKB */
	MKK4_MKKA2	= 0x84,		/* Japan UNI-1 even + UNI-2 + MKKA2 */
	MKK4_MKKC	= 0x85,		/* Japan UNI-1 even + UNI-2 + MKKC */

	MKK5_MKKB	= 0x86,		/* Japan UNI-1 even + UNI-2 + mid-band + MKKB */
	MKK5_MKKA2	= 0x87,		/* Japan UNI-1 even + UNI-2 + mid-band + MKKA2 */
	MKK5_MKKC	= 0x88,		/* Japan UNI-1 even + UNI-2 + mid-band + MKKC */

	MKK6_MKKB	= 0x89,		/* Japan UNI-1 even + UNI-1 odd MKKB */
	MKK6_MKKA2	= 0x8A,		/* Japan UNI-1 even + UNI-1 odd + MKKA2 */
	MKK6_MKKC	= 0x8B,		/* Japan UNI-1 even + UNI-1 odd + MKKC */

	MKK7_MKKB	= 0x8C,		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + MKKB */
	MKK7_MKKA2	= 0x8D,		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + MKKA2 */
	MKK7_MKKC	= 0x8E,		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + MKKC */

	MKK8_MKKB	= 0x8F,		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + mid-band + MKKB */
	MKK8_MKKA2	= 0x90,		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + mid-band + MKKA2 */
	MKK8_MKKC	= 0x91,		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + mid-band + MKKC */

	/* Following definitions are used only by s/w to map old
 	 * Japan SKUs.
	 */
	MKK3_MKKA       = 0xF0,         /* Japan UNI-1 even + MKKA */
	MKK3_MKKA1      = 0xF1,         /* Japan UNI-1 even + MKKA1 */
	MKK3_FCCA       = 0xF2,         /* Japan UNI-1 even + FCCA */
	MKK4_MKKA       = 0xF3,         /* Japan UNI-1 even + UNI-2 + MKKA */
	MKK4_MKKA1      = 0xF4,         /* Japan UNI-1 even + UNI-2 + MKKA1 */
	MKK4_FCCA       = 0xF5,         /* Japan UNI-1 even + UNI-2 + FCCA */
	MKK9_MKKA       = 0xF6,         /* Japan UNI-1 even + 4.9GHz */
	MKK10_MKKA      = 0xF7,         /* Japan UNI-1 even + UNI-2 + 4.9GHz */

	/*
	 * Regulator domains ending in a number (e.g. APL1,
	 * MK1, ETSI4, etc) apply to 5GHz channel and power
	 * information.  Regulator domains ending in a letter
	 * (e.g. APLA, FCCA, etc) apply to 2.4GHz channel and
	 * power information.
	 */
	APL1		= 0x0150,	/* LAT & Asia */
	APL2		= 0x0250,	/* LAT & Asia */
	APL3		= 0x0350,	/* Taiwan */
	APL4		= 0x0450,	/* Jordan */
	APL5		= 0x0550,	/* Chile */
	APL6		= 0x0650,	/* Singapore */
	APL8		= 0x0850,	/* Malaysia */
	APL9		= 0x0950,	/* Korea (South) ROC 3 */

	ETSI1		= 0x0130,	/* Europe & others */
	ETSI2		= 0x0230,	/* Europe & others */
	ETSI3		= 0x0330,	/* Europe & others */
	ETSI4		= 0x0430,	/* Europe & others */
	ETSI5		= 0x0530,	/* Europe & others */
	ETSI6		= 0x0630,	/* Europe & others */
	ETSIA		= 0x0A30,	/* France */
	ETSIB		= 0x0B30,	/* Israel */
	ETSIC		= 0x0C30,	/* Latin America */

	FCC1		= 0x0110,	/* US & others */
	FCC2		= 0x0120,	/* Canada, Australia & New Zealand */
	FCC3		= 0x0160,	/* US w/new middle band & DFS */    
	FCC4          	= 0x0165,     	/* US Public Safety */
	FCCA		= 0x0A10,	 

	APLD		= 0x0D50,	/* South Korea */

	MKK1		= 0x0140,	/* Japan (UNI-1 odd)*/
	MKK2		= 0x0240,	/* Japan (4.9 GHz + UNI-1 odd) */
	MKK3		= 0x0340,	/* Japan (UNI-1 even) */
	MKK4		= 0x0440,	/* Japan (UNI-1 even + UNI-2) */
	MKK5		= 0x0540,	/* Japan (UNI-1 even + UNI-2 + mid-band) */
	MKK6		= 0x0640,	/* Japan (UNI-1 odd + UNI-1 even) */
	MKK7		= 0x0740,	/* Japan (UNI-1 odd + UNI-1 even + UNI-2 */
	MKK8		= 0x0840,	/* Japan (UNI-1 odd + UNI-1 even + UNI-2 + mid-band) */
	MKK9            = 0x0940,       /* Japan (UNI-1 even + 4.9 GHZ) */
	MKK10           = 0x0B40,       /* Japan (UNI-1 even + UNI-2 + 4.9 GHZ) */
	MKKA		= 0x0A40,	/* Japan */
	MKKC		= 0x0A50,

	NULL1		= 0x0198,
	WORLD		= 0x0199,
	GSM		= 0x019a,
	DEBUG_REG_DMN	= 0x01ff,
};
#define DEF_REGDMN		FCC1_FCCA

static struct {
	const char *name;
	HAL_REG_DOMAIN rd;
} domains[] = {
#define	D(_x)	{ #_x, _x }
	D(NO_ENUMRD),
	D(NULL1_WORLD),		/* For 11b-only countries (no 11a allowed) */
	D(NULL1_ETSIB),		/* Israel */
	D(NULL1_ETSIC),
	D(NULL1_GSM),		/* GSM-only operation */
	D(FCC1_FCCA),		/* USA */
	D(FCC1_WORLD),		/* Hong Kong */
	D(FCC4_FCCA),		/* USA - Public Safety */

	D(FCC2_FCCA),		/* Canada */
	D(FCC2_WORLD),		/* Australia & HK */
	D(FCC2_ETSIC),
	D(FRANCE_RES),		/* Legacy France for OEM */
	D(FCC3_FCCA),
	D(FCC3_WORLD),

	D(ETSI1_WORLD),
	D(ETSI3_ETSIA),		/* France (optional) */
	D(ETSI2_WORLD),		/* Hungary & others */
	D(ETSI3_WORLD),		/* France & others */
	D(ETSI4_WORLD),
	D(ETSI4_ETSIC),
	D(ETSI5_WORLD),
	D(ETSI6_WORLD),		/* Bulgaria */
	D(ETSI_RESERVED),		/* Reserved (Do not used) */

	D(MKK1_MKKA),		/* Japan (JP1) */
	D(MKK1_MKKB),		/* Japan (JP0) */
	D(APL4_WORLD),		/* Singapore */
	D(MKK2_MKKA),		/* Japan with 4.9G channels */
	D(APL_RESERVED),		/* Reserved (Do not used)  */
	D(APL2_WORLD),		/* Korea */
	D(APL2_APLC),
	D(APL3_WORLD),
	D(MKK1_FCCA),		/* Japan (JP1-1) */
	D(APL2_APLD),		/* Korea with 2.3G channels */
	D(MKK1_MKKA1),		/* Japan (JE1) */
	D(MKK1_MKKA2),		/* Japan (JE2) */
	D(MKK1_MKKC),

	D(APL3_FCCA),
	D(APL1_WORLD),		/* Latin America */
	D(APL1_FCCA),
	D(APL1_APLA),
	D(APL1_ETSIC),
	D(APL2_ETSIC),		/* Venezuela */
	D(APL5_WORLD),		/* Chile */
	D(APL6_WORLD),		/* Singapore */
	D(APL7_FCCA),     /* Taiwan 5.47 Band */
	D(APL8_WORLD),     /* Malaysia 5GHz */
	D(APL9_WORLD),     /* Korea 5GHz */

	D(WOR0_WORLD),		/* World0 (WO0 SKU) */
	D(WOR1_WORLD),		/* World1 (WO1 SKU) */
	D(WOR2_WORLD),		/* World2 (WO2 SKU) */
	D(WOR3_WORLD),		/* World3 (WO3 SKU) */
	D(WOR4_WORLD),		/* World4 (WO4 SKU) */	
	D(WOR5_ETSIC),		/* World5 (WO5 SKU) */    

	D(WOR01_WORLD),		/* World0-1 (WW0-1 SKU) */
	D(WOR02_WORLD),		/* World0-2 (WW0-2 SKU) */
	D(EU1_WORLD),

	D(WOR9_WORLD),		/* World9 (WO9 SKU) */	
	D(WORA_WORLD),		/* WorldA (WOA SKU) */	

	D(MKK3_MKKB),		/* Japan UNI-1 even + MKKB */
	D(MKK3_MKKA2),		/* Japan UNI-1 even + MKKA2 */
	D(MKK3_MKKC),		/* Japan UNI-1 even + MKKC */

	D(MKK4_MKKB),		/* Japan UNI-1 even + UNI-2 + MKKB */
	D(MKK4_MKKA2),		/* Japan UNI-1 even + UNI-2 + MKKA2 */
	D(MKK4_MKKC),		/* Japan UNI-1 even + UNI-2 + MKKC */

	D(MKK5_MKKB),		/* Japan UNI-1 even + UNI-2 + mid-band + MKKB */
	D(MKK5_MKKA2),		/* Japan UNI-1 even + UNI-2 + mid-band + MKKA2 */
	D(MKK5_MKKC),		/* Japan UNI-1 even + UNI-2 + mid-band + MKKC */

	D(MKK6_MKKB),		/* Japan UNI-1 even + UNI-1 odd MKKB */
	D(MKK6_MKKA2),		/* Japan UNI-1 even + UNI-1 odd + MKKA2 */
	D(MKK6_MKKC),		/* Japan UNI-1 even + UNI-1 odd + MKKC */

	D(MKK7_MKKB),		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + MKKB */
	D(MKK7_MKKA2),		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + MKKA2 */
	D(MKK7_MKKC),		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + MKKC */

	D(MKK8_MKKB),		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + mid-band + MKKB */
	D(MKK8_MKKA2),		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + mid-band + MKKA2 */
	D(MKK8_MKKC),		/* Japan UNI-1 even + UNI-1 odd + UNI-2 + mid-band + MKKC */

	D(MKK3_MKKA),         /* Japan UNI-1 even + MKKA */
	D(MKK3_MKKA1),         /* Japan UNI-1 even + MKKA1 */
	D(MKK3_FCCA),         /* Japan UNI-1 even + FCCA */
	D(MKK4_MKKA),         /* Japan UNI-1 even + UNI-2 + MKKA */
	D(MKK4_MKKA1),         /* Japan UNI-1 even + UNI-2 + MKKA1 */
	D(MKK4_FCCA),         /* Japan UNI-1 even + UNI-2 + FCCA */
	D(MKK9_MKKA),         /* Japan UNI-1 even + 4.9GHz */
	D(MKK10_MKKA),         /* Japan UNI-1 even + UNI-2 + 4.9GHz */

	D(APL1),	/* LAT & Asia */
	D(APL2),	/* LAT & Asia */
	D(APL3),	/* Taiwan */
	D(APL4),	/* Jordan */
	D(APL5),	/* Chile */
	D(APL6),	/* Singapore */
	D(APL8),	/* Malaysia */
	D(APL9),	/* Korea (South) ROC 3 */

	D(ETSI1),	/* Europe & others */
	D(ETSI2),	/* Europe & others */
	D(ETSI3),	/* Europe & others */
	D(ETSI4),	/* Europe & others */
	D(ETSI5),	/* Europe & others */
	D(ETSI6),	/* Europe & others */
	D(ETSIA),	/* France */
	D(ETSIB),	/* Israel */
	D(ETSIC),	/* Latin America */

	D(FCC1),	/* US & others */
	D(FCC2),
	D(FCC3),	/* US w/new middle band & DFS */    
	D(FCC4),     	/* US Public Safety */
	D(FCCA),	 

	D(APLD),	/* South Korea */

	D(MKK1),	/* Japan (UNI-1 odd)*/
	D(MKK2),	/* Japan (4.9 GHz + UNI-1 odd) */
	D(MKK3),	/* Japan (UNI-1 even) */
	D(MKK4),	/* Japan (UNI-1 even + UNI-2) */
	D(MKK5),	/* Japan (UNI-1 even + UNI-2 + mid-band) */
	D(MKK6),	/* Japan (UNI-1 odd + UNI-1 even) */
	D(MKK7),	/* Japan (UNI-1 odd + UNI-1 even + UNI-2 */
	D(MKK8),	/* Japan (UNI-1 odd + UNI-1 even + UNI-2 + mid-band) */
	D(MKK9),       /* Japan (UNI-1 even + 4.9 GHZ) */
	D(MKK10),       /* Japan (UNI-1 even + UNI-2 + 4.9 GHZ) */
	D(MKKA),	/* Japan */
	D(MKKC),

	D(NULL1),
	D(WORLD),
	D(GSM),
	D(DEBUG_REG_DMN),
#undef D
};

static HAL_BOOL
rdlookup(const char *name, HAL_REG_DOMAIN *rd)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;

	for (i = 0; i < N(domains); i++)
		if (strcasecmp(domains[i].name, name) == 0) {
			*rd = domains[i].rd;
			return AH_TRUE;
		}
	return AH_FALSE;
#undef N
}

static const char *
getrdname(HAL_REG_DOMAIN rd)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;

	for (i = 0; i < N(domains); i++)
		if (domains[i].rd == rd)
			return domains[i].name;
	return NULL;
#undef N
}

static void
rdlist()
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;

	printf("\nRegulatory domains:\n\n");
	for (i = 0; i < N(domains); i++)
		printf("%-15s%s", domains[i].name,
			((i+1)%5) == 0 ? "\n" : "");
	printf("\n");
#undef N
}

typedef struct {
	HAL_CTRY_CODE	countryCode;	   
	HAL_REG_DOMAIN	regDmnEnum;
	const char*	isoName;
	const char*	name;
	HAL_BOOL	allow11g;
	HAL_BOOL	allow11aTurbo;
	HAL_BOOL	allow11gTurbo;
	u_int16_t	outdoorChanStart;
} COUNTRY_CODE_TO_ENUM_RD;
 
#define	YES	AH_TRUE
#define	NO	AH_FALSE
/* Index into table to avoid DEBUG and NO COUNTRY SET entries */
#define CTRY_ONLY_INDEX 2
/*
 * Country Code Table to Enumerated RD
 */

static COUNTRY_CODE_TO_ENUM_RD allCountries[] = {
    {CTRY_DEBUG,       NO_ENUMRD,     "DB", "DEBUG", YES, YES, YES, 7000 },
    {CTRY_DEFAULT,     DEF_REGDMN,    "NA", "NO_COUNTRY_SET", YES, YES, YES, 7000 },
    {CTRY_ALBANIA,     NULL1_WORLD,   "AL", "ALBANIA",        YES, NO,  YES, 7000 },
    {CTRY_ALGERIA,     NULL1_WORLD,   "DZ", "ALGERIA",        YES, NO,  YES, 7000 },
    {CTRY_ARGENTINA,   APL3_WORLD,    "AR", "ARGENTINA",      NO,  NO,  NO,  7000 },
    {CTRY_ARMENIA,     ETSI4_WORLD,   "AM", "ARMENIA",        YES, NO,  YES, 7000 },
    {CTRY_AUSTRALIA,   FCC2_WORLD,    "AU", "AUSTRALIA",      YES, YES, YES, 7000 },
    {CTRY_AUSTRIA,     ETSI1_WORLD,   "AT", "AUSTRIA",        YES, NO,  YES, 7000 },
    {CTRY_AZERBAIJAN,  ETSI4_WORLD,   "AZ", "AZERBAIJAN",     YES, YES, YES, 7000 },
    {CTRY_BAHRAIN,     APL6_WORLD,   "BH", "BAHRAIN",        YES, NO,  YES, 7000 },
    {CTRY_BELARUS,     NULL1_WORLD,   "BY", "BELARUS",        YES, NO,  YES, 7000 },
    {CTRY_BELGIUM,     ETSI1_WORLD,   "BE", "BELGIUM",        YES, NO,  YES, 7000 },
    {CTRY_BELIZE,      APL1_ETSIC,    "BZ", "BELIZE",         YES, YES, YES, 7000 },
    {CTRY_BOLIVIA,     APL1_ETSIC,    "BO", "BOLVIA",         YES, YES, YES, 7000 },
    {CTRY_BRAZIL,      FCC3_WORLD,    "BR", "BRAZIL",         YES,  NO,  NO, 7000 },
    {CTRY_BRUNEI_DARUSSALAM,APL1_WORLD,"BN", "BRUNEI DARUSSALAM", YES, YES, YES, 7000 },
    {CTRY_BULGARIA,    ETSI6_WORLD,   "BG", "BULGARIA",       YES, NO,  YES, 7000 },
    {CTRY_CANADA,      FCC2_FCCA,     "CA", "CANADA",         YES, YES, YES, 7000 },
    {CTRY_CHILE,       APL6_WORLD,    "CL", "CHILE",          YES, YES, YES, 7000 },
    {CTRY_CHINA,       APL1_WORLD,    "CN", "CHINA",          YES, YES, YES, 7000 },
    {CTRY_COLOMBIA,    FCC1_FCCA,     "CO", "COLOMBIA",       YES, NO,  YES, 7000 },
    {CTRY_COSTA_RICA,  NULL1_WORLD,   "CR", "COSTA RICA",     YES, NO,  YES, 7000 },
    {CTRY_CROATIA,     ETSI3_WORLD,   "HR", "CROATIA",        YES, NO,  YES, 7000 },
    {CTRY_CYPRUS,      ETSI1_WORLD,   "CY", "CYPRUS",         YES, YES, YES, 7000 },
    {CTRY_CZECH,       ETSI3_WORLD,   "CZ", "CZECH REPUBLIC", YES,  NO, YES,  7000 },
    {CTRY_DENMARK,     ETSI1_WORLD,   "DK", "DENMARK",        YES,  NO, YES, 7000 },
    {CTRY_DOMINICAN_REPUBLIC,FCC1_FCCA,"DO", "DOMINICAN REPUBLIC", YES, YES, YES, 7000 },
    {CTRY_ECUADOR,     NULL1_WORLD,   "EC", "ECUADOR",        NO,  NO,  NO,  7000 },
    {CTRY_EGYPT,       ETSI3_WORLD,   "EG", "EGYPT",          YES, NO,  YES, 7000 },
    {CTRY_EL_SALVADOR, NULL1_WORLD,   "SV", "EL SALVADOR",    YES, NO,  YES, 7000 },    
    {CTRY_ESTONIA,     ETSI1_WORLD,   "EE", "ESTONIA",        YES, NO,  YES, 7000 },
    {CTRY_FINLAND,     ETSI1_WORLD,   "FI", "FINLAND",        YES, NO,  YES, 7000 },
    {CTRY_FRANCE,      ETSI3_WORLD,   "FR", "FRANCE",         YES, NO,  YES, 7000 },
    {CTRY_FRANCE2,     ETSI3_WORLD,   "F2", "FRANCE_RES",     YES, NO,  YES, 7000 },
    {CTRY_GEORGIA,     ETSI4_WORLD,   "GE", "GEORGIA",        YES, YES, YES, 7000 },
    {CTRY_GERMANY,     ETSI1_WORLD,   "DE", "GERMANY",        YES, NO,  YES, 7000 },
    {CTRY_GREECE,      ETSI1_WORLD,   "GR", "GREECE",         YES, NO,  YES, 7000 },
    {CTRY_GSM,         NULL1_GSM,     "GS", "GSM",            YES, NO,   NO, 7000 },
    {CTRY_GUATEMALA,   FCC1_FCCA,     "GT", "GUATEMALA",      YES, YES, YES, 7000 },
    {CTRY_HONDURAS,    NULL1_WORLD,   "HN", "HONDURAS",       YES, NO,  YES, 7000 },
    {CTRY_HONG_KONG,   FCC2_WORLD,    "HK", "HONG KONG",      YES, YES, YES, 7000 },
    {CTRY_HUNGARY,     ETSI1_WORLD,   "HU", "HUNGARY",        YES, NO,  YES, 7000 },
    {CTRY_ICELAND,     ETSI1_WORLD,   "IS", "ICELAND",        YES, NO,  YES, 7000 },
    {CTRY_INDIA,       APL6_WORLD,    "IN", "INDIA",          YES, NO,  YES, 7000 },
    {CTRY_INDONESIA,   APL1_WORLD,    "ID", "INDONESIA",      YES, NO,  YES, 7000 },
    {CTRY_IRAN,        APL1_WORLD,    "IR", "IRAN",           YES, YES, YES, 7000 },
    {CTRY_IRELAND,     ETSI1_WORLD,   "IE", "IRELAND",        YES, NO,  YES, 7000 },
    {CTRY_ISRAEL,      NULL1_WORLD,   "IL", "ISRAEL",         YES, NO,  YES, 7000 },
    {CTRY_ITALY,       ETSI1_WORLD,   "IT", "ITALY",          YES, NO,  YES, 7000 },
    {CTRY_JAPAN,       MKK1_MKKA,     "JP", "JAPAN",          YES, NO,  NO,  7000 },
    {CTRY_JAPAN1,      MKK1_MKKB,     "JP", "JAPAN1",         YES, NO,  NO,  7000 },
    {CTRY_JAPAN2,      MKK1_FCCA,     "JP", "JAPAN2",         YES, NO,  NO,  7000 },    
    {CTRY_JAPAN3,      MKK2_MKKA,     "JP", "JAPAN3",         YES, NO,  NO,  7000 },
    {CTRY_JAPAN4,      MKK1_MKKA1,    "JP", "JAPAN4",         YES, NO,  NO,  7000 },
    {CTRY_JAPAN5,      MKK1_MKKA2,    "JP", "JAPAN5",         YES, NO,  NO,  7000 },    
    {CTRY_JAPAN6,      MKK1_MKKC,     "JP", "JAPAN6",         YES, NO,  NO,  7000 },    

    {CTRY_JAPAN7,      MKK3_MKKB,     "JP", "JAPAN7",         YES, NO,  NO,  7000 },
    {CTRY_JAPAN8,      MKK3_MKKA2,    "JP", "JAPAN8",       YES, NO,  NO,  7000 },    
    {CTRY_JAPAN9,      MKK3_MKKC,     "JP", "JAPAN9",       YES, NO,  NO,  7000 },    

    {CTRY_JAPAN10,      MKK4_MKKB,     "JP", "JAPAN10",       YES, NO,  NO,  7000 },
    {CTRY_JAPAN11,      MKK4_MKKA2,    "JP", "JAPAN11",       YES, NO,  NO,  7000 },    
    {CTRY_JAPAN12,      MKK4_MKKC,     "JP", "JAPAN12",       YES, NO,  NO,  7000 },    

    {CTRY_JAPAN13,      MKK5_MKKB,     "JP", "JAPAN13",       YES, NO,  NO,  7000 },
    {CTRY_JAPAN14,      MKK5_MKKA2,    "JP", "JAPAN14",       YES, NO,  NO,  7000 },    
    {CTRY_JAPAN15,      MKK5_MKKC,     "JP", "JAPAN15",       YES, NO,  NO,  7000 },    

    {CTRY_JAPAN16,      MKK6_MKKB,     "JP", "JAPAN16",       YES, NO,  NO,  7000 },
    {CTRY_JAPAN17,      MKK6_MKKA2,    "JP", "JAPAN17",       YES, NO,  NO,  7000 },    
    {CTRY_JAPAN18,      MKK6_MKKC,     "JP", "JAPAN18",       YES, NO,  NO,  7000 },    

    {CTRY_JAPAN19,      MKK7_MKKB,     "JP", "JAPAN19",       YES, NO,  NO,  7000 },
    {CTRY_JAPAN20,      MKK7_MKKA2,    "JP", "JAPAN20",       YES, NO,  NO,  7000 },    
    {CTRY_JAPAN21,      MKK7_MKKC,     "JP", "JAPAN21",       YES, NO,  NO,  7000 },    

    {CTRY_JAPAN22,      MKK8_MKKB,     "JP", "JAPAN22",       YES, NO,  NO,  7000 },
    {CTRY_JAPAN23,      MKK8_MKKA2,    "JP", "JAPAN23",       YES, NO,  NO,  7000 },    
    {CTRY_JAPAN24,      MKK8_MKKC,     "JP", "JAPAN24",       YES, NO,  NO,  7000 },    

    {CTRY_JORDAN,      APL4_WORLD,    "JO", "JORDAN",         YES, NO,  YES, 7000 },
    {CTRY_KAZAKHSTAN,  NULL1_WORLD,   "KZ", "KAZAKHSTAN",     YES, NO,  YES, 7000 },
    {CTRY_KOREA_NORTH, APL2_WORLD,    "KP", "NORTH KOREA",    YES, YES, YES, 7000 },
    {CTRY_KOREA_ROC,   APL2_WORLD,    "KR", "KOREA REPUBLIC", YES, NO,   NO, 7000 },
    {CTRY_KOREA_ROC2,  APL2_WORLD,    "K2", "KOREA REPUBLIC2",YES, NO,   NO, 7000 },
    {CTRY_KOREA_ROC3,  APL9_WORLD,    "K3", "KOREA REPUBLIC3",YES, NO,   NO, 7000 },
    {CTRY_KUWAIT,      NULL1_WORLD,   "KW", "KUWAIT",         YES, NO,  YES, 7000 },
    {CTRY_LATVIA,      ETSI1_WORLD,   "LV", "LATVIA",         YES, NO,  YES, 7000 },
    {CTRY_LEBANON,     NULL1_WORLD,   "LB", "LEBANON",        YES, NO,  YES, 7000 },
    {CTRY_LIECHTENSTEIN,ETSI1_WORLD,  "LI", "LIECHTENSTEIN",  YES, NO,  YES, 7000 },
    {CTRY_LITHUANIA,   ETSI1_WORLD,   "LT", "LITHUANIA",      YES, NO,  YES, 7000 },
    {CTRY_LUXEMBOURG,  ETSI1_WORLD,   "LU", "LUXEMBOURG",     YES, NO,  YES, 7000 },
    {CTRY_MACAU,       FCC2_WORLD,    "MO", "MACAU",          YES, YES, YES, 7000 },
    {CTRY_MACEDONIA,   NULL1_WORLD,   "MK", "MACEDONIA",      YES, NO,  YES, 7000 },
    {CTRY_MALAYSIA,    APL8_WORLD,    "MY", "MALAYSIA",       YES, NO,  NO, 7000 },
    {CTRY_MALTA,       ETSI1_WORLD,   "MT", "MALTA",          YES, NO,  YES, 7000 },
    {CTRY_MEXICO,      FCC1_FCCA,     "MX", "MEXICO",         YES, YES, YES, 7000 },
    {CTRY_MONACO,      ETSI4_WORLD,   "MC", "MONACO",         YES, YES, YES, 7000 },
    {CTRY_MOROCCO,     NULL1_WORLD,   "MA", "MOROCCO",        YES, NO,  YES, 7000 },
    {CTRY_NETHERLANDS, ETSI1_WORLD,   "NL", "NETHERLANDS",    YES, NO,  YES, 7000 },
    {CTRY_NEW_ZEALAND, FCC2_ETSIC,    "NZ", "NEW ZEALAND",    YES, NO,  YES, 7000 },
    {CTRY_NORWAY,      ETSI1_WORLD,   "NO", "NORWAY",         YES, NO,  YES, 7000 },
    {CTRY_OMAN,        APL6_WORLD,    "OM", "OMAN",           YES, NO,  YES, 7000 },
    {CTRY_PAKISTAN,    NULL1_WORLD,   "PK", "PAKISTAN",       YES, NO,  YES, 7000 },
    {CTRY_PANAMA,      FCC1_FCCA,     "PA", "PANAMA",         YES, YES, YES, 7000 },
    {CTRY_PERU,        APL1_WORLD,    "PE", "PERU",           YES, NO,  YES, 7000 },
    {CTRY_PHILIPPINES, APL1_WORLD,    "PH", "PHILIPPINES",    YES, YES, YES, 7000 },
    {CTRY_POLAND,      ETSI1_WORLD,   "PL", "POLAND",         YES, NO,  YES, 7000 },
    {CTRY_PORTUGAL,    ETSI1_WORLD,   "PT", "PORTUGAL",       YES, NO,  YES, 7000 },
    {CTRY_PUERTO_RICO, FCC1_FCCA,     "PR", "PUERTO RICO",    YES, YES, YES, 7000 },
    {CTRY_QATAR,       NULL1_WORLD,   "QA", "QATAR",          YES, NO,  YES, 7000 },
    {CTRY_ROMANIA,     NULL1_WORLD,   "RO", "ROMANIA",        YES, NO,  YES, 7000 },
    {CTRY_RUSSIA,      NULL1_WORLD,   "RU", "RUSSIA",         YES, NO,  YES, 7000 },
    {CTRY_SAUDI_ARABIA,NULL1_WORLD,   "SA", "SAUDI ARABIA",   YES, NO,  YES, 7000 },
    {CTRY_SINGAPORE,   APL6_WORLD,    "SG", "SINGAPORE",      YES, YES, YES, 7000 },
    {CTRY_SLOVAKIA,    ETSI1_WORLD,   "SK", "SLOVAK REPUBLIC",YES, NO,  YES, 7000 },
    {CTRY_SLOVENIA,    ETSI1_WORLD,   "SI", "SLOVENIA",       YES, NO,  YES, 7000 },
    {CTRY_SOUTH_AFRICA,FCC3_WORLD,    "ZA", "SOUTH AFRICA",   YES,  NO, YES, 7000 },
    {CTRY_SPAIN,       ETSI1_WORLD,   "ES", "SPAIN",          YES, NO,  YES, 7000 },
    {CTRY_SWEDEN,      ETSI1_WORLD,   "SE", "SWEDEN",         YES, NO,  YES, 7000 },
    {CTRY_SWITZERLAND, ETSI1_WORLD,   "CH", "SWITZERLAND",    YES, NO,  YES, 7000 },
    {CTRY_SYRIA,       NULL1_WORLD,   "SY", "SYRIA",          YES, NO,  YES, 7000 },
    {CTRY_TAIWAN,      APL3_FCCA,    "TW", "TAIWAN",         YES, YES, YES, 7000 },
    {CTRY_THAILAND,    NULL1_WORLD,   "TH", "THAILAND",       YES, NO, YES, 7000 },
    {CTRY_TRINIDAD_Y_TOBAGO,ETSI4_WORLD,"TT", "TRINIDAD & TOBAGO", YES, NO, YES, 7000 },
    {CTRY_TUNISIA,     ETSI3_WORLD,   "TN", "TUNISIA",        YES, NO,  YES, 7000 },
    {CTRY_TURKEY,      ETSI3_WORLD,   "TR", "TURKEY",         YES, NO,  YES, 7000 },
    {CTRY_UKRAINE,     NULL1_WORLD,   "UA", "UKRAINE",        YES, NO,  YES, 7000 },
    {CTRY_UAE,         NULL1_WORLD,   "AE", "UNITED ARAB EMIRATES", YES, NO, YES, 7000 },
    {CTRY_UNITED_KINGDOM, ETSI1_WORLD,"GB", "UNITED KINGDOM", YES, NO,  YES, 7000 },
    {CTRY_UNITED_STATES, FCC1_FCCA,   "US", "UNITED STATES",  YES, YES, YES, 5825 },
    {CTRY_UNITED_STATES_FCC49, FCC4_FCCA,   "PS", "UNITED STATES (PUBLIC SAFETY)",  YES, YES, YES, 7000 },
    {CTRY_URUGUAY,     APL2_WORLD,    "UY", "URUGUAY",        YES, NO,  YES, 7000 },
    {CTRY_UZBEKISTAN,  FCC3_FCCA,     "UZ", "UZBEKISTAN",     YES, YES, YES, 7000 },    
    {CTRY_VENEZUELA,   APL2_ETSIC,    "VE", "VENEZUELA",      YES, NO,  YES, 7000 },
    {CTRY_VIET_NAM,    NULL1_WORLD,   "VN", "VIET NAM",       YES, NO,  YES, 7000 },
    {CTRY_YEMEN,       NULL1_WORLD,   "YE", "YEMEN",          YES, NO,  YES, 7000 },
    {CTRY_ZIMBABWE,    NULL1_WORLD,   "ZW", "ZIMBABWE",       YES, NO,  YES, 7000 }    
};
#undef	YES
#undef	NO

static HAL_BOOL
cclookup(const char *name, HAL_REG_DOMAIN *rd, HAL_CTRY_CODE *cc)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;

	for (i = 0; i < N(allCountries); i++)
		if (strcasecmp(allCountries[i].isoName, name) == 0 ||
		    strcasecmp(allCountries[i].name, name) == 0) {
			*rd = allCountries[i].regDmnEnum;
			*cc = allCountries[i].countryCode;
			return AH_TRUE;
		}
	return AH_FALSE;
#undef N
}

static const char *
getccname(HAL_CTRY_CODE cc)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;

	for (i = 0; i < N(allCountries); i++)
		if (allCountries[i].countryCode == cc)
			return allCountries[i].name;
	return NULL;
#undef N
}

static const char *
getccisoname(HAL_CTRY_CODE cc)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;

	for (i = 0; i < N(allCountries); i++)
		if (allCountries[i].countryCode == cc)
			return allCountries[i].isoName;
	return NULL;
#undef N
}

static void
cclist()
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;

	printf("\nCountry codes:\n");
	for (i = 0; i < N(allCountries); i++)
		printf("%2s %-15.15s%s",
			allCountries[i].isoName,
			allCountries[i].name,
			((i+1)%4) == 0 ? "\n" : " ");
	printf("\n");
#undef N
}

static HAL_BOOL
setRateTable(struct ath_hal *ah, HAL_CHANNEL *chan, 
		   int16_t tpcScaleReduction, int16_t powerLimit,
                   int16_t *pMinPower, int16_t *pMaxPower);

static void
calctxpower(struct ath_hal *ah, int nchan, HAL_CHANNEL *chans,
	int16_t tpcScaleReduction, int16_t powerLimit, int16_t *txpow)
{
	int16_t minpow;
	int i;

	for (i = 0; i < nchan; i++)
		if (!setRateTable(ah, &chans[i],
		    tpcScaleReduction, powerLimit, &minpow, &txpow[i])) {
			printf("unable to set rate table\n");
			exit(-1);
		}
}

int	n = 1;
const char *sep = "";
int	dopassive = 0;
int	showchannels = 0;
int	isdfs = 0;
int	is4ms = 0;

static int
anychan(const HAL_CHANNEL *chans, int nc, int flag)
{
	int i;

	for (i = 0; i < nc; i++)
		if ((chans[i].privFlags & flag) != 0)
			return 1;
	return 0;
}

static __inline int
mapgsm(u_int freq, u_int flags)
{
	freq *= 10;
	if (flags & CHANNEL_QUARTER)
		freq += 5;
	else if (flags & CHANNEL_HALF)
		freq += 10;
	else
		freq += 20;
	return (freq - 24220) / 5;
}

static __inline int
mappsb(u_int freq, u_int flags)
{
	return ((freq * 10) + (((freq % 5) == 2) ? 5 : 0) - 49400) / 5;
}

/*
 * Convert GHz frequency to IEEE channel number.
 */
int
ath_hal_mhz2ieee(struct ath_hal *ah, u_int freq, u_int flags)
{
	if (flags & CHANNEL_2GHZ) {	/* 2GHz band */
		if (freq == 2484)
			return 14;
		if (freq < 2484) {
			if (ath_hal_isgsmsku(ah))
				return mapgsm(freq, flags);
			return ((int)freq - 2407) / 5;
		} else
			return 15 + ((freq - 2512) / 20);
	} else if (flags & CHANNEL_5GHZ) {/* 5Ghz band */
		if (ath_hal_ispublicsafetysku(ah) &&
		    IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq)) {
			return mappsb(freq, flags);
		} else if ((flags & CHANNEL_A) && (freq <= 5000)) {
			return (freq - 4000) / 5;
		} else {
			return (freq - 5000) / 5;
		}
	} else {			/* either, guess */
		if (freq == 2484)
			return 14;
		if (freq < 2484) {
			if (ath_hal_isgsmsku(ah))
				return mapgsm(freq, flags);
			return ((int)freq - 2407) / 5;
		}
		if (freq < 5000) {
			if (ath_hal_ispublicsafetysku(ah) &&
			    IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq)) {
				return mappsb(freq, flags);
			} else if (freq > 4900) {
				return (freq - 4000) / 5;
			} else {
				return 15 + ((freq - 2512) / 20);
			}
		}
		return (freq - 5000) / 5;
	}
}

#define	IS_CHAN_DFS(_c)	(((_c)->privFlags & CHANNEL_DFS) != 0)
#define	IS_CHAN_4MS(_c)	(((_c)->privFlags & CHANNEL_4MS_LIMIT) != 0)

static void
dumpchannels(struct ath_hal *ah, int nc, HAL_CHANNEL *chans, int16_t *txpow)
{
	int i;

	for (i = 0; i < nc; i++) {
		HAL_CHANNEL *c = &chans[i];
		int type;

		if (showchannels)
			printf("%s%3d", sep,
			    ath_hal_mhz2ieee(ah, c->channel, c->channelFlags));
		else
			printf("%s%u", sep, c->channel);
		if (IS_CHAN_HALF_RATE(c))
			type = 'H';
		else if (IS_CHAN_QUARTER_RATE(c))
			type = 'Q';
		else if (IS_CHAN_TURBO(c))
			type = 'T';
		else if (IS_CHAN_HT(c))
			type = 'N';
		else if (IS_CHAN_A(c))
			type = 'A';
		else if (IS_CHAN_108G(c))
			type = 'T';
		else if (IS_CHAN_G(c))
			type = 'G';
		else
			type = 'B';
		if (dopassive && IS_CHAN_PASSIVE(c))
			type = tolower(type);
		if (isdfs && is4ms)
			printf("%c%c%c %d.%d", type,
			    IS_CHAN_DFS(c) ? '*' : ' ',
			    IS_CHAN_4MS(c) ? '4' : ' ',
			    txpow[i]/2, (txpow[i]%2)*5);
		else if (isdfs)
			printf("%c%c %d.%d", type,
			    IS_CHAN_DFS(c) ? '*' : ' ',
			    txpow[i]/2, (txpow[i]%2)*5);
		else if (is4ms)
			printf("%c%c %d.%d", type,
			    IS_CHAN_4MS(c) ? '4' : ' ',
			    txpow[i]/2, (txpow[i]%2)*5);
		else
			printf("%c %d.%d", type, txpow[i]/2, (txpow[i]%2)*5);
		if ((n++ % (showchannels ? 7 : 6)) == 0)
			sep = "\n";
		else
			sep = " ";
	}
}

static void
checkchannels(struct ath_hal *ah, HAL_CHANNEL *chans, int nchan)
{
	int i;

	for (i = 0; i < nchan; i++) {
		HAL_CHANNEL *c = &chans[i];
		if (!ath_hal_checkchannel(ah, c))
			printf("Channel %u (0x%x) disallowed\n",
				c->channel, c->channelFlags);
	}
}

static void
intersect(HAL_CHANNEL *dst, int16_t *dtxpow, int *nd,
    const HAL_CHANNEL *src, int16_t *stxpow, int ns)
{
	int i = 0, j, k, l;
	while (i < *nd) {
		for (j = 0; j < ns && dst[i].channel != src[j].channel; j++)
			;
		if (j < ns && dtxpow[i] == stxpow[j]) {
			for (k = i+1, l = i; k < *nd; k++, l++)
				dst[l] = dst[k];
			(*nd)--;
		} else
			i++;
	}
}

static void
usage(const char *progname)
{
	printf("usage: %s [-acdefoilpr4ABGT] [-m opmode] [cc | rd]\n", progname);
	exit(-1);
}

static HAL_BOOL
getChipPowerLimits(struct ath_hal *ah, HAL_CHANNEL *chans, u_int32_t nchan)
{
}

static HAL_BOOL
eepromRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
	/* emulate enough stuff to handle japan channel shift */
	switch (off) {
	case AR_EEPROM_VERSION:
		*data = eeversion;
		return AH_TRUE;
	case AR_EEPROM_REG_CAPABILITIES_OFFSET:
		*data = AR_EEPROM_EEREGCAP_EN_KK_NEW_11A;
		return AH_TRUE;
	case AR_EEPROM_REG_CAPABILITIES_OFFSET_PRE4_0:
		*data = AR_EEPROM_EEREGCAP_EN_KK_NEW_11A_PRE4_0;
		return AH_TRUE;
	}
	return AH_FALSE;
}

HAL_STATUS
getCapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
	uint32_t capability, uint32_t *result)
{
	const HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

	switch (type) {
	case HAL_CAP_REG_DMN:		/* regulatory domain */
		*result = AH_PRIVATE(ah)->ah_currentRD;
		return HAL_OK;
	default:
		return HAL_EINVAL;
	}
}

#define HAL_MODE_HT20 \
	(HAL_MODE_11NG_HT20 |  HAL_MODE_11NA_HT20)
#define	HAL_MODE_HT40 \
	(HAL_MODE_11NG_HT40PLUS | HAL_MODE_11NG_HT40MINUS | \
	 HAL_MODE_11NA_HT40PLUS | HAL_MODE_11NA_HT40MINUS)
#define	HAL_MODE_HT	(HAL_MODE_HT20 | HAL_MODE_HT40)
     
int
main(int argc, char *argv[])
{
	static const u_int16_t tpcScaleReductionTable[5] =
		{ 0, 3, 6, 9, MAX_RATE_POWER };
	struct ath_hal_private ahp;
	HAL_CHANNEL achans[IEEE80211_CHAN_MAX];
	int16_t atxpow[IEEE80211_CHAN_MAX];
	HAL_CHANNEL bchans[IEEE80211_CHAN_MAX];
	int16_t btxpow[IEEE80211_CHAN_MAX];
	HAL_CHANNEL gchans[IEEE80211_CHAN_MAX];
	int16_t gtxpow[IEEE80211_CHAN_MAX];
	HAL_CHANNEL tchans[IEEE80211_CHAN_MAX];
	int16_t ttxpow[IEEE80211_CHAN_MAX];
	HAL_CHANNEL tgchans[IEEE80211_CHAN_MAX];
	int16_t tgtxpow[IEEE80211_CHAN_MAX];
	HAL_CHANNEL nchans[IEEE80211_CHAN_MAX];
	int16_t ntxpow[IEEE80211_CHAN_MAX];
	int i, na, nb, ng, nt, ntg, nn;
	HAL_BOOL showall = AH_FALSE;
	HAL_BOOL extendedChanMode = AH_TRUE;
	int modes = 0;
	int16_t tpcReduction, powerLimit;
	int8_t regids[IEEE80211_REGCLASSIDS_MAX];
	int nregids;
	int showdfs = 0;
	int show4ms = 0;

	memset(&ahp, 0, sizeof(ahp));
	ahp.ah_getChannelEdges = getChannelEdges;
	ahp.ah_getWirelessModes = getWirelessModes;
	ahp.ah_eepromRead = eepromRead;
	ahp.ah_getChipPowerLimits = getChipPowerLimits;
	ahp.ah_caps.halWirelessModes = HAL_MODE_ALL;
	ahp.ah_caps.halLow5GhzChan = 4920;
	ahp.ah_caps.halHigh5GhzChan = 6100;
	ahp.ah_caps.halLow2GhzChan = 2312;
	ahp.ah_caps.halHigh2GhzChan = 2732;
	ahp.ah_caps.halChanHalfRate = AH_TRUE;
	ahp.ah_caps.halChanQuarterRate = AH_TRUE;
	ahp.h.ah_getCapability = getCapability;
	ahp.ah_opmode = HAL_M_STA;

	tpcReduction = tpcScaleReductionTable[0];
	powerLimit =  MAX_RATE_POWER;

	while ((i = getopt(argc, argv, "acdefoilm:pr4ABGhHNT")) != -1)
		switch (i) {
		case 'a':
			showall = AH_TRUE;
			break;
		case 'c':
			showchannels = AH_TRUE;
			break;
		case 'd':
			ath_hal_debug = HAL_DEBUG_ANY;
			break;
		case 'e':
			extendedChanMode = AH_FALSE;
			break;
		case 'f':
			showchannels = AH_FALSE;
			break;
		case 'o':
			outdoor = AH_TRUE;
			break;
		case 'i':
			outdoor = AH_FALSE;
			break;
		case 'l':
			cclist();
			rdlist();
			exit(0);
		case 'm':
			if (strncasecmp(optarg, "sta", 2) == 0)
				ahp.ah_opmode = HAL_M_STA;
			else if (strncasecmp(optarg, "ibss", 2) == 0)
				ahp.ah_opmode = HAL_M_IBSS;
			else if (strncasecmp(optarg, "adhoc", 2) == 0)
				ahp.ah_opmode = HAL_M_IBSS;
			else if (strncasecmp(optarg, "ap", 2) == 0)
				ahp.ah_opmode = HAL_M_HOSTAP;
			else if (strncasecmp(optarg, "hostap", 2) == 0)
				ahp.ah_opmode = HAL_M_HOSTAP;
			else if (strncasecmp(optarg, "monitor", 2) == 0)
				ahp.ah_opmode = HAL_M_MONITOR;
			else
				usage(argv[0]);
			break;
		case 'p':
			dopassive = 1;
			break;
		case 'A':
			modes |= HAL_MODE_11A;
			break;
		case 'B':
			modes |= HAL_MODE_11B;
			break;
		case 'G':
			modes |= HAL_MODE_11G;
			break;
		case 'h':
			modes |= HAL_MODE_HT20;
			break;
		case 'H':
			modes |= HAL_MODE_HT40;
			break;
		case 'N':
			modes |= HAL_MODE_HT;
			break;
		case 'T':
			modes |= HAL_MODE_TURBO | HAL_MODE_108G;
			break;
		case 'r':
			showdfs = 1;
			break;
		case '4':
			show4ms = 1;
			break;
		default:
			usage(argv[0]);
		}
	switch (argc - optind)  {
	case 0:
		if (!cclookup("US", &rd, &cc)) {
			printf("%s: unknown country code\n", "US");
			exit(-1);
		}
		break;
	case 1:			/* cc/regdomain */
		if (!cclookup(argv[optind], &rd, &cc)) {
			if (!rdlookup(argv[optind], &rd)) {
				const char* rdname;

				rd = strtoul(argv[optind], NULL, 0);
				rdname = getrdname(rd);
				if (rdname == NULL) {
					printf("%s: unknown country/regulatory "
						"domain code\n", argv[optind]);
					exit(-1);
				}
			}
			cc = CTRY_DEFAULT;
		}
		break;
	default:		/* regdomain cc */
		if (!rdlookup(argv[optind], &rd)) {
			const char* rdname;

			rd = strtoul(argv[optind], NULL, 0);
			rdname = getrdname(rd);
			if (rdname == NULL) {
				printf("%s: unknown country/regulatory "
					"domain code\n", argv[optind]);
				exit(-1);
			}
		}
		if (!cclookup(argv[optind+1], &rd, &cc))
			cc = strtoul(argv[optind+1], NULL, 0);
		break;
	}
	if (cc != CTRY_DEFAULT)
		printf("\n%s (%s, 0x%x, %u) %s (0x%x, %u)\n",
			getccname(cc), getccisoname(cc), cc, cc,
			getrdname(rd), rd, rd);
	else
		printf("\n%s (0x%x, %u)\n",
			getrdname(rd), rd, rd);

	if (modes == 0)
		modes = HAL_MODE_11A | HAL_MODE_11B |
			HAL_MODE_11G | HAL_MODE_TURBO | HAL_MODE_108G |
			HAL_MODE_HT;
	na = nb = ng = nt = ntg = nn = 0;
	if (modes & HAL_MODE_11G) {
		ahp.ah_currentRD = rd;
		if (ath_hal_init_channels(&ahp.h,
		    gchans, IEEE80211_CHAN_MAX, &ng,
		    regids, IEEE80211_REGCLASSIDS_MAX, &nregids,
		    cc, HAL_MODE_11G, outdoor, extendedChanMode)) {
			checkchannels(&ahp.h, gchans, ng);
			calctxpower(&ahp.h, ng, gchans, tpcReduction, powerLimit, gtxpow);
			if (showdfs)
				isdfs |= anychan(gchans, ng, CHANNEL_DFS);
			if (show4ms)
				is4ms |= anychan(gchans, ng, CHANNEL_4MS_LIMIT);
		}
	}
	if (modes & HAL_MODE_11B) {
		ahp.ah_currentRD = rd;
		if (ath_hal_init_channels(&ahp.h,
		    bchans, IEEE80211_CHAN_MAX, &nb,
		    regids, IEEE80211_REGCLASSIDS_MAX, &nregids,
		    cc, HAL_MODE_11B, outdoor, extendedChanMode)) {
			checkchannels(&ahp.h, bchans, nb);
			calctxpower(&ahp.h, nb, bchans, tpcReduction, powerLimit, btxpow);
			if (showdfs)
				isdfs |= anychan(bchans, nb, CHANNEL_DFS);
			if (show4ms)
				is4ms |= anychan(bchans, nb, CHANNEL_4MS_LIMIT);
		}
	}
	if (modes & HAL_MODE_11A) {
		ahp.ah_currentRD = rd;
		if (ath_hal_init_channels(&ahp.h,
		    achans, IEEE80211_CHAN_MAX, &na,
		    regids, IEEE80211_REGCLASSIDS_MAX, &nregids,
		    cc, HAL_MODE_11A, outdoor, extendedChanMode)) {
			checkchannels(&ahp.h, achans, na);
			calctxpower(&ahp.h, na, achans, tpcReduction, powerLimit, atxpow);
			if (showdfs)
				isdfs |= anychan(achans, na, CHANNEL_DFS);
			if (show4ms)
				is4ms |= anychan(achans, na, CHANNEL_4MS_LIMIT);
		}
	}
	if (modes & HAL_MODE_TURBO) {
		ahp.ah_currentRD = rd;
		if (ath_hal_init_channels(&ahp.h,
		    tchans, IEEE80211_CHAN_MAX, &nt,
		    regids, IEEE80211_REGCLASSIDS_MAX, &nregids,
		    cc, HAL_MODE_TURBO, outdoor, extendedChanMode)) {
			checkchannels(&ahp.h, tchans, nt);
			calctxpower(&ahp.h, nt, tchans, tpcReduction, powerLimit, ttxpow);
			if (showdfs)
				isdfs |= anychan(tchans, nt, CHANNEL_DFS);
			if (show4ms)
				is4ms |= anychan(tchans, nt, CHANNEL_4MS_LIMIT);
		}
	}	
	if (modes & HAL_MODE_108G) {
		ahp.ah_currentRD = rd;
		if (ath_hal_init_channels(&ahp.h,
		    tgchans, IEEE80211_CHAN_MAX, &ntg,
		    regids, IEEE80211_REGCLASSIDS_MAX, &nregids,
		    cc, HAL_MODE_108G, outdoor, extendedChanMode)) {
			checkchannels(&ahp.h, tgchans, ntg);
			calctxpower(&ahp.h, ntg, tgchans, tpcReduction, powerLimit, tgtxpow);
			if (showdfs)
				isdfs |= anychan(tgchans, ntg, CHANNEL_DFS);
			if (show4ms)
				is4ms |= anychan(tgchans, ntg, CHANNEL_4MS_LIMIT);
		}
	}
	if (modes & HAL_MODE_HT) {
		ahp.ah_currentRD = rd;
		if (ath_hal_init_channels(&ahp.h,
		    nchans, IEEE80211_CHAN_MAX, &nn,
		    regids, IEEE80211_REGCLASSIDS_MAX, &nregids,
		    cc, modes & HAL_MODE_HT, outdoor, extendedChanMode)) {
			checkchannels(&ahp.h, nchans, nn);
			calctxpower(&ahp.h, nn, nchans, tpcReduction, powerLimit, ntxpow);
			if (showdfs)
				isdfs |= anychan(nchans, nn, CHANNEL_DFS);
			if (show4ms)
				is4ms |= anychan(nchans, nn, CHANNEL_4MS_LIMIT);
		}
	}

	if (!showall) {
#define	CHECKMODES(_modes, _m)	((_modes & (_m)) == (_m))
		if (CHECKMODES(modes, HAL_MODE_11B|HAL_MODE_11G)) {
			/* b ^= g */
			intersect(bchans, btxpow, &nb, gchans, gtxpow, ng);
		}
		if (CHECKMODES(modes, HAL_MODE_11A|HAL_MODE_TURBO)) {
			/* t ^= a */
			intersect(tchans, ttxpow, &nt, achans, atxpow, na);
		}
		if (CHECKMODES(modes, HAL_MODE_11G|HAL_MODE_108G)) {
			/* tg ^= g */
			intersect(tgchans, tgtxpow, &ntg, gchans, gtxpow, ng);
		}
		if (CHECKMODES(modes, HAL_MODE_11G|HAL_MODE_HT)) {
			/* g ^= n */
			intersect(gchans, gtxpow, &ng, nchans, ntxpow, nn);
		}
		if (CHECKMODES(modes, HAL_MODE_11A|HAL_MODE_HT)) {
			/* a ^= n */
			intersect(achans, atxpow, &na, nchans, ntxpow, nn);
		}
#undef CHECKMODES
	}

	if (modes & HAL_MODE_11G)
		dumpchannels(&ahp.h, ng, gchans, gtxpow);
	if (modes & HAL_MODE_11B)
		dumpchannels(&ahp.h, nb, bchans, btxpow);
	if (modes & HAL_MODE_11A)
		dumpchannels(&ahp.h, na, achans, atxpow);
	if (modes & HAL_MODE_108G)
		dumpchannels(&ahp.h, ntg, tgchans, tgtxpow);
	if (modes & HAL_MODE_TURBO)
		dumpchannels(&ahp.h, nt, tchans, ttxpow);
	if (modes & HAL_MODE_HT)
		dumpchannels(&ahp.h, nn, nchans, ntxpow);
	printf("\n");
	return (0);
}

/*
 * Search a list for a specified value v that is within
 * EEP_DELTA of the search values.  Return the closest
 * values in the list above and below the desired value.
 * EEP_DELTA is a factional value; everything is scaled
 * so only integer arithmetic is used.
 *
 * NB: the input list is assumed to be sorted in ascending order
 */
static void
ar5212GetLowerUpperValues(u_int16_t v, u_int16_t *lp, u_int16_t listSize,
                          u_int16_t *vlo, u_int16_t *vhi)
{
	u_int32_t target = v * EEP_SCALE;
	u_int16_t *ep = lp+listSize;

	/*
	 * Check first and last elements for out-of-bounds conditions.
	 */
	if (target < (u_int32_t)(lp[0] * EEP_SCALE - EEP_DELTA)) {
		*vlo = *vhi = lp[0];
		return;
	}
	if (target > (u_int32_t)(ep[-1] * EEP_SCALE + EEP_DELTA)) {
		*vlo = *vhi = ep[-1];
		return;
	}

	/* look for value being near or between 2 values in list */
	for (; lp < ep; lp++) {
		/*
		 * If value is close to the current value of the list
		 * then target is not between values, it is one of the values
		 */
		if (abs(lp[0] * EEP_SCALE - target) < EEP_DELTA) {
			*vlo = *vhi = lp[0];
			return;
		}
		/*
		 * Look for value being between current value and next value
		 * if so return these 2 values
		 */
		if (target < (u_int32_t)(lp[1] * EEP_SCALE - EEP_DELTA)) {
			*vlo = lp[0];
			*vhi = lp[1];
			return;
		}
	}
}

/*
 * Find the maximum conformance test limit for the given channel and CTL info
 */
static u_int16_t
ar5212GetMaxEdgePower(u_int16_t channel, RD_EDGES_POWER *pRdEdgesPower)
{
	/* temp array for holding edge channels */
	u_int16_t tempChannelList[NUM_EDGES];
	u_int16_t clo, chi, twiceMaxEdgePower;
	int i, numEdges;

	/* Get the edge power */
	for (i = 0; i < NUM_EDGES; i++) {
		if (pRdEdgesPower[i].rdEdge == 0)
			break;
		tempChannelList[i] = pRdEdgesPower[i].rdEdge;
	}
	numEdges = i;

	ar5212GetLowerUpperValues(channel, tempChannelList,
		numEdges, &clo, &chi);
	/* Get the index for the lower channel */
	for (i = 0; i < numEdges && clo != tempChannelList[i]; i++)
		;
	/* Is lower channel ever outside the rdEdge? */
	HALASSERT(i != numEdges);

	if ((clo == chi && clo == channel) || (pRdEdgesPower[i].flag)) {
		/* 
		 * If there's an exact channel match or an inband flag set
		 * on the lower channel use the given rdEdgePower 
		 */
		twiceMaxEdgePower = pRdEdgesPower[i].twice_rdEdgePower;
		HALASSERT(twiceMaxEdgePower > 0);
	} else
		twiceMaxEdgePower = MAX_RATE_POWER;
	return twiceMaxEdgePower;
}

/*
 * Returns interpolated or the scaled up interpolated value
 */
static u_int16_t
interpolate(u_int16_t target, u_int16_t srcLeft, u_int16_t srcRight,
	u_int16_t targetLeft, u_int16_t targetRight)
{
	u_int16_t rv;
	int16_t lRatio;

	/* to get an accurate ratio, always scale, if want to scale, then don't scale back down */
	if ((targetLeft * targetRight) == 0)
		return 0;

	if (srcRight != srcLeft) {
		/*
		 * Note the ratio always need to be scaled,
		 * since it will be a fraction.
		 */
		lRatio = (target - srcLeft) * EEP_SCALE / (srcRight - srcLeft);
		if (lRatio < 0) {
		    /* Return as Left target if value would be negative */
		    rv = targetLeft;
		} else if (lRatio > EEP_SCALE) {
		    /* Return as Right target if Ratio is greater than 100% (SCALE) */
		    rv = targetRight;
		} else {
			rv = (lRatio * targetRight + (EEP_SCALE - lRatio) *
					targetLeft) / EEP_SCALE;
		}
	} else {
		rv = targetLeft;
	}
	return rv;
}

/*
 * Return the four rates of target power for the given target power table 
 * channel, and number of channels
 */
static void
ar5212GetTargetPowers(struct ath_hal *ah, HAL_CHANNEL *chan,
	TRGT_POWER_INFO *powInfo,
	u_int16_t numChannels, TRGT_POWER_INFO *pNewPower)
{
	/* temp array for holding target power channels */
	u_int16_t tempChannelList[NUM_TEST_FREQUENCIES];
	u_int16_t clo, chi, ixlo, ixhi;
	int i;

	/* Copy the target powers into the temp channel list */
	for (i = 0; i < numChannels; i++)
		tempChannelList[i] = powInfo[i].testChannel;

	ar5212GetLowerUpperValues(chan->channel, tempChannelList,
		numChannels, &clo, &chi);

	/* Get the indices for the channel */
	ixlo = ixhi = 0;
	for (i = 0; i < numChannels; i++) {
		if (clo == tempChannelList[i]) {
			ixlo = i;
		}
		if (chi == tempChannelList[i]) {
			ixhi = i;
			break;
		}
	}

	/*
	 * Get the lower and upper channels, target powers,
	 * and interpolate between them.
	 */
	pNewPower->twicePwr6_24 = interpolate(chan->channel, clo, chi,
		powInfo[ixlo].twicePwr6_24, powInfo[ixhi].twicePwr6_24);
	pNewPower->twicePwr36 = interpolate(chan->channel, clo, chi,
		powInfo[ixlo].twicePwr36, powInfo[ixhi].twicePwr36);
	pNewPower->twicePwr48 = interpolate(chan->channel, clo, chi,
		powInfo[ixlo].twicePwr48, powInfo[ixhi].twicePwr48);
	pNewPower->twicePwr54 = interpolate(chan->channel, clo, chi,
		powInfo[ixlo].twicePwr54, powInfo[ixhi].twicePwr54);
}

static RD_EDGES_POWER*
findEdgePower(struct ath_hal *ah, u_int ctl)
{
	int i;

	for (i = 0; i < _numCtls; i++)
		if (_ctl[i] == ctl)
			return &_rdEdgesPower[i * NUM_EDGES];
	return AH_NULL;
}

/*
 * Sets the transmit power in the baseband for the given
 * operating channel and mode.
 */
static HAL_BOOL
setRateTable(struct ath_hal *ah, HAL_CHANNEL *chan, 
		   int16_t tpcScaleReduction, int16_t powerLimit,
                   int16_t *pMinPower, int16_t *pMaxPower)
{
	u_int16_t ratesArray[16];
	u_int16_t *rpow = ratesArray;
	u_int16_t twiceMaxRDPower, twiceMaxEdgePower, twiceMaxEdgePowerCck;
	int8_t twiceAntennaGain, twiceAntennaReduction;
	TRGT_POWER_INFO targetPowerOfdm, targetPowerCck;
	RD_EDGES_POWER *rep;
	int16_t scaledPower;
	u_int8_t cfgCtl;

	twiceMaxRDPower = chan->maxRegTxPower * 2;
	*pMaxPower = -MAX_RATE_POWER;
	*pMinPower = MAX_RATE_POWER;

	/* Get conformance test limit maximum for this channel */
	cfgCtl = ath_hal_getctl(ah, chan);
	rep = findEdgePower(ah, cfgCtl);
	if (rep != AH_NULL)
		twiceMaxEdgePower = ar5212GetMaxEdgePower(chan->channel, rep);
	else
		twiceMaxEdgePower = MAX_RATE_POWER;

	if (IS_CHAN_G(chan)) {
		/* Check for a CCK CTL for 11G CCK powers */
		cfgCtl = (cfgCtl & 0xFC) | 0x01;
		rep = findEdgePower(ah, cfgCtl);
		if (rep != AH_NULL)
			twiceMaxEdgePowerCck = ar5212GetMaxEdgePower(chan->channel, rep);
		else
			twiceMaxEdgePowerCck = MAX_RATE_POWER;
	} else {
		/* Set the 11B cck edge power to the one found before */
		twiceMaxEdgePowerCck = twiceMaxEdgePower;
	}

	/* Get Antenna Gain reduction */
	if (IS_CHAN_5GHZ(chan)) {
		twiceAntennaGain = antennaGainMax[0];
	} else {
		twiceAntennaGain = antennaGainMax[1];
	}
	twiceAntennaReduction =
		ath_hal_getantennareduction(ah, chan, twiceAntennaGain);

	if (IS_CHAN_OFDM(chan)) {
		/* Get final OFDM target powers */
		if (IS_CHAN_G(chan)) { 
			/* TODO - add Turbo 2.4 to this mode check */
			ar5212GetTargetPowers(ah, chan, trgtPwr_11g,
				numTargetPwr_11g, &targetPowerOfdm);
		} else {
			ar5212GetTargetPowers(ah, chan, trgtPwr_11a,
				numTargetPwr_11a, &targetPowerOfdm);
		}

		/* Get Maximum OFDM power */
		/* Minimum of target and edge powers */
		scaledPower = AH_MIN(twiceMaxEdgePower,
				twiceMaxRDPower - twiceAntennaReduction);

		/*
		 * If turbo is set, reduce power to keep power
		 * consumption under 2 Watts.  Note that we always do
		 * this unless specially configured.  Then we limit
		 * power only for non-AP operation.
		 */
		if (IS_CHAN_TURBO(chan)
#ifdef AH_ENABLE_AP_SUPPORT
		    && AH_PRIVATE(ah)->ah_opmode != HAL_M_HOSTAP
#endif
		) {
			/*
			 * If turbo is set, reduce power to keep power
			 * consumption under 2 Watts
			 */
			if (eeversion >= AR_EEPROM_VER3_1)
				scaledPower = AH_MIN(scaledPower,
					turbo2WMaxPower5);
			/*
			 * EEPROM version 4.0 added an additional
			 * constraint on 2.4GHz channels.
			 */
			if (eeversion >= AR_EEPROM_VER4_0 &&
			    IS_CHAN_2GHZ(chan))
				scaledPower = AH_MIN(scaledPower,
					turbo2WMaxPower2);
		}
		/* Reduce power by max regulatory domain allowed restrictions */
		scaledPower -= (tpcScaleReduction * 2);
		scaledPower = (scaledPower < 0) ? 0 : scaledPower;
		scaledPower = AH_MIN(scaledPower, powerLimit);

		scaledPower = AH_MIN(scaledPower, targetPowerOfdm.twicePwr6_24);

		/* Set OFDM rates 9, 12, 18, 24, 36, 48, 54, XR */
		rpow[0] = rpow[1] = rpow[2] = rpow[3] = rpow[4] = scaledPower;
		rpow[5] = AH_MIN(rpow[0], targetPowerOfdm.twicePwr36);
		rpow[6] = AH_MIN(rpow[0], targetPowerOfdm.twicePwr48);
		rpow[7] = AH_MIN(rpow[0], targetPowerOfdm.twicePwr54);

#ifdef notyet
		if (eeversion >= AR_EEPROM_VER4_0) {
			/* Setup XR target power from EEPROM */
			rpow[15] = AH_MIN(scaledPower, IS_CHAN_2GHZ(chan) ?
				xrTargetPower2 : xrTargetPower5);
		} else {
			/* XR uses 6mb power */
			rpow[15] = rpow[0];
		}
#else
		rpow[15] = rpow[0];
#endif

		*pMinPower = rpow[7];
		*pMaxPower = rpow[0];

#if 0
		ahp->ah_ofdmTxPower = rpow[0];
#endif

		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: MaxRD: %d TurboMax: %d MaxCTL: %d "
		    "TPC_Reduction %d\n", __func__,
		    twiceMaxRDPower, turbo2WMaxPower5,
		    twiceMaxEdgePower, tpcScaleReduction * 2);
	}

	if (IS_CHAN_CCK(chan) || IS_CHAN_G(chan)) {
		/* Get final CCK target powers */
		ar5212GetTargetPowers(ah, chan, trgtPwr_11b,
			numTargetPwr_11b, &targetPowerCck);

		/* Reduce power by max regulatory domain allowed restrictions */
		scaledPower = AH_MIN(twiceMaxEdgePowerCck,
			twiceMaxRDPower - twiceAntennaReduction);

		scaledPower -= (tpcScaleReduction * 2);
		scaledPower = (scaledPower < 0) ? 0 : scaledPower;
		scaledPower = AH_MIN(scaledPower, powerLimit);

		rpow[8] = (scaledPower < 1) ? 1 : scaledPower;

		/* Set CCK rates 2L, 2S, 5.5L, 5.5S, 11L, 11S */
		rpow[8]  = AH_MIN(scaledPower, targetPowerCck.twicePwr6_24);
		rpow[9]  = AH_MIN(scaledPower, targetPowerCck.twicePwr36);
		rpow[10] = rpow[9];
		rpow[11] = AH_MIN(scaledPower, targetPowerCck.twicePwr48);
		rpow[12] = rpow[11];
		rpow[13] = AH_MIN(scaledPower, targetPowerCck.twicePwr54);
		rpow[14] = rpow[13];

		/* Set min/max power based off OFDM values or initialization */
		if (rpow[13] < *pMinPower)
		    *pMinPower = rpow[13];
		if (rpow[9] > *pMaxPower)
		    *pMaxPower = rpow[9];

	}
#if 0
	ahp->ah_tx6PowerInHalfDbm = *pMaxPower;
#endif
	return AH_TRUE;
}

void*
ath_hal_malloc(size_t size)
{
	return calloc(1, size);
}

void
ath_hal_free(void* p)
{
	return free(p);
}

void
ath_hal_vprintf(struct ath_hal *ah, const char* fmt, va_list ap)
{
	vprintf(fmt, ap);
}

void
ath_hal_printf(struct ath_hal *ah, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ath_hal_vprintf(ah, fmt, ap);
	va_end(ap);
}

void
HALDEBUG(struct ath_hal *ah, u_int mask, const char* fmt, ...)
{
	if (ath_hal_debug & mask) {
		__va_list ap;
		va_start(ap, fmt);
		ath_hal_vprintf(ah, fmt, ap);
		va_end(ap);
	}
}
