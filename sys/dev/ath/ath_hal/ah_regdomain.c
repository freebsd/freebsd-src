/*
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
 * Copyright (c) 2005-2006 Atheros Communications, Inc.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
 *
 * $FreeBSD$
 */
#include "opt_ah.h"

#include "ah.h"

#include <net80211/_ieee80211.h>
#include <net80211/ieee80211_regdomain.h>

#include "ah_internal.h"
#include "ah_eeprom.h"
#include "ah_devid.h"

/*
 * XXX this code needs a audit+review
 */

/* used throughout this file... */
#define	N(a)	(sizeof (a) / sizeof (a[0]))

#define HAL_MODE_11A_TURBO	HAL_MODE_108A
#define HAL_MODE_11G_TURBO	HAL_MODE_108G

/* 
 * BMLEN defines the size of the bitmask used to hold frequency
 * band specifications.  Note this must agree with the BM macro
 * definition that's used to setup initializers.  See also further
 * comments below.
 */
#define BMLEN 2		/* 2 x 64 bits in each channel bitmask */
typedef uint64_t chanbmask_t[BMLEN];

#define	W0(_a) \
	(((_a) >= 0 && (_a) < 64 ? (((uint64_t) 1)<<(_a)) : (uint64_t) 0))
#define	W1(_a) \
	(((_a) > 63 && (_a) < 128 ? (((uint64_t) 1)<<((_a)-64)) : (uint64_t) 0))
#define BM1(_fa)	{ W0(_fa), W1(_fa) }
#define BM2(_fa, _fb)	{ W0(_fa) | W0(_fb), W1(_fa) | W1(_fb) }
#define BM3(_fa, _fb, _fc) \
	{ W0(_fa) | W0(_fb) | W0(_fc), W1(_fa) | W1(_fb) | W1(_fc) }
#define BM4(_fa, _fb, _fc, _fd)						\
	{ W0(_fa) | W0(_fb) | W0(_fc) | W0(_fd),			\
	  W1(_fa) | W1(_fb) | W1(_fc) | W1(_fd) }
#define BM5(_fa, _fb, _fc, _fd, _fe)					\
	{ W0(_fa) | W0(_fb) | W0(_fc) | W0(_fd) | W0(_fe),		\
	  W1(_fa) | W1(_fb) | W1(_fc) | W1(_fd) | W1(_fe) }
#define BM6(_fa, _fb, _fc, _fd, _fe, _ff)				\
	{ W0(_fa) | W0(_fb) | W0(_fc) | W0(_fd) | W0(_fe) | W0(_ff),	\
	  W1(_fa) | W1(_fb) | W1(_fc) | W1(_fd) | W1(_fe) | W1(_ff) }
#define BM7(_fa, _fb, _fc, _fd, _fe, _ff, _fg)	\
	{ W0(_fa) | W0(_fb) | W0(_fc) | W0(_fd) | W0(_fe) | W0(_ff) |	\
	  W0(_fg),\
	  W1(_fa) | W1(_fb) | W1(_fc) | W1(_fd) | W1(_fe) | W1(_ff) |	\
	  W1(_fg) }
#define BM8(_fa, _fb, _fc, _fd, _fe, _ff, _fg, _fh)	\
	{ W0(_fa) | W0(_fb) | W0(_fc) | W0(_fd) | W0(_fe) | W0(_ff) |	\
	  W0(_fg) | W0(_fh) ,	\
	  W1(_fa) | W1(_fb) | W1(_fc) | W1(_fd) | W1(_fe) | W1(_ff) |	\
	  W1(_fg) | W1(_fh) }
#define BM9(_fa, _fb, _fc, _fd, _fe, _ff, _fg, _fh, _fi)	\
	{ W0(_fa) | W0(_fb) | W0(_fc) | W0(_fd) | W0(_fe) | W0(_ff) |	\
	  W0(_fg) | W0(_fh) | W0(_fi) ,	\
	  W1(_fa) | W1(_fb) | W1(_fc) | W1(_fd) | W1(_fe) | W1(_ff) |	\
	  W1(_fg) | W1(_fh) | W1(_fi) }

/*
 * Mask to check whether a domain is a multidomain or a single domain
 */
#define MULTI_DOMAIN_MASK 0xFF00

/*
 * Enumerated Regulatory Domain Information 8 bit values indicate that
 * the regdomain is really a pair of unitary regdomains.  12 bit values
 * are the real unitary regdomains and are the only ones which have the
 * frequency bitmasks and flags set.
 */
enum {
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
	FCC1_FCCA	= 0x10,		/* USA */
	FCC1_WORLD	= 0x11,		/* Hong Kong */
	FCC4_FCCA	= 0x12,		/* USA - Public Safety */
	FCC5_FCCB	= 0x13,		/* USA w/ 1/2 and 1/4 width channels */

	FCC2_FCCA	= 0x20,		/* Canada */
	FCC2_WORLD	= 0x21,		/* Australia & HK */
	FCC2_ETSIC	= 0x22,
	FRANCE_RES	= 0x31,		/* Legacy France for OEM */
	FCC3_FCCA	= 0x3A,		/* USA & Canada w/5470 band, 11h, DFS enabled */
	FCC3_WORLD	= 0x3B,		/* USA & Canada w/5470 band, 11h, DFS enabled */

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

	APL3_FCCA       = 0x50,
	APL1_WORLD	= 0x52,		/* Latin America */
	APL1_FCCA	= 0x53,
	APL1_APLA	= 0x54,
	APL1_ETSIC	= 0x55,
	APL2_ETSIC	= 0x56,		/* Venezuela */
	APL5_WORLD	= 0x58,		/* Chile */
	APL6_WORLD	= 0x5B,		/* Singapore */
	APL7_FCCA   	= 0x5C,     	/* Taiwan 5.47 Band */
	APL8_WORLD  	= 0x5D,     	/* Malaysia 5GHz */
	APL9_WORLD  	= 0x5E,     	/* Korea 5GHz */

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
	WORB_WORLD	= 0x6B,		/* WorldB (WOB SKU) */

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
	FCC5          	= 0x0166,     	/* US w/ 1/2 and 1/4 width channels */
	FCCA		= 0x0A10,	 
	FCCB		= 0x0A11,	/* US w/ 1/2 and 1/4 width channels */

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
	DEBUG_REG_DMN	= 0x01ff,
};

#define	WORLD_SKU_MASK		0x00F0
#define	WORLD_SKU_PREFIX	0x0060

enum {					/* conformance test limits */
	FCC	= 0x10,
	MKK	= 0x40,
	ETSI	= 0x30,
};

/*
 * The following are flags for different requirements per reg domain.
 * These requirements are either inhereted from the reg domain pair or
 * from the unitary reg domain if the reg domain pair flags value is 0
 */
enum {
	NO_REQ			= 0x00000000,	/* NB: must be zero */
	DISALLOW_ADHOC_11A	= 0x00000001,	/* adhoc not allowed in 5GHz */
	DISALLOW_ADHOC_11A_TURB	= 0x00000002,	/* not allowed w/ 5GHz turbo */
	NEED_NFC		= 0x00000004,	/* need noise floor check */
	ADHOC_PER_11D		= 0x00000008,	/* must receive 11d beacon */
	LIMIT_FRAME_4MS 	= 0x00000020,	/* 4msec tx burst limit */
	NO_HOSTAP		= 0x00000040,	/* No HOSTAP mode opereation */
};

/*
 * The following describe the bit masks for different passive scan
 * capability/requirements per regdomain.
 */
#define	NO_PSCAN	0x0ULL			/* NB: must be zero */
#define	PSCAN_FCC	0x0000000000000001ULL
#define	PSCAN_FCC_T	0x0000000000000002ULL
#define	PSCAN_ETSI	0x0000000000000004ULL
#define	PSCAN_MKK1	0x0000000000000008ULL
#define	PSCAN_MKK2	0x0000000000000010ULL
#define	PSCAN_MKKA	0x0000000000000020ULL
#define	PSCAN_MKKA_G	0x0000000000000040ULL
#define	PSCAN_ETSIA	0x0000000000000080ULL
#define	PSCAN_ETSIB	0x0000000000000100ULL
#define	PSCAN_ETSIC	0x0000000000000200ULL
#define	PSCAN_WWR	0x0000000000000400ULL
#define	PSCAN_MKKA1	0x0000000000000800ULL
#define	PSCAN_MKKA1_G	0x0000000000001000ULL
#define	PSCAN_MKKA2	0x0000000000002000ULL
#define	PSCAN_MKKA2_G	0x0000000000004000ULL
#define	PSCAN_MKK3	0x0000000000008000ULL
#define	PSCAN_DEFER	0x7FFFFFFFFFFFFFFFULL
#define	IS_ECM_CHAN	0x8000000000000000ULL

/*
 * THE following table is the mapping of regdomain pairs specified by
 * an 8 bit regdomain value to the individual unitary reg domains
 */
typedef struct regDomainPair {
	HAL_REG_DOMAIN regDmnEnum;	/* 16 bit reg domain pair */
	HAL_REG_DOMAIN regDmn5GHz;	/* 5GHz reg domain */
	HAL_REG_DOMAIN regDmn2GHz;	/* 2GHz reg domain */
	uint32_t flags5GHz;		/* Requirements flags (AdHoc
					   disallow, noise floor cal needed,
					   etc) */
	uint32_t flags2GHz;		/* Requirements flags (AdHoc
					   disallow, noise floor cal needed,
					   etc) */
	uint64_t pscanMask;		/* Passive Scan flags which
					   can override unitary domain
					   passive scan flags.  This
					   value is used as a mask on
					   the unitary flags*/
	uint16_t singleCC;		/* Country code of single country if
					   a one-on-one mapping exists */
}  REG_DMN_PAIR_MAPPING;

static REG_DMN_PAIR_MAPPING regDomainPairs[] = {
	{NO_ENUMRD,	DEBUG_REG_DMN,	DEBUG_REG_DMN, NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{NULL1_WORLD,	NULL1,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{NULL1_ETSIB,	NULL1,		ETSIB,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{NULL1_ETSIC,	NULL1,		ETSIC,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },

	{FCC2_FCCA,	FCC2,		FCCA,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{FCC2_WORLD,	FCC2,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{FCC2_ETSIC,	FCC2,		ETSIC,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{FCC3_FCCA,	FCC3,		FCCA,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{FCC3_WORLD,	FCC3,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{FCC4_FCCA,	FCC4,		FCCA,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{FCC5_FCCB,	FCC5,		FCCB,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },

	{ETSI1_WORLD,	ETSI1,		WORLD,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{ETSI2_WORLD,	ETSI2,		WORLD,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{ETSI3_WORLD,	ETSI3,		WORLD,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{ETSI4_WORLD,	ETSI4,		WORLD,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{ETSI5_WORLD,	ETSI5,		WORLD,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{ETSI6_WORLD,	ETSI6,		WORLD,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },

	{ETSI3_ETSIA,	ETSI3,		WORLD,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{FRANCE_RES,	ETSI3,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },

	{FCC1_WORLD,	FCC1,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{FCC1_FCCA,	FCC1,		FCCA,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL1_WORLD,	APL1,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL2_WORLD,	APL2,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL3_WORLD,	APL3,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL4_WORLD,	APL4,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL5_WORLD,	APL5,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL6_WORLD,	APL6,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL8_WORLD,	APL8,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL9_WORLD,	APL9,		WORLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },

	{APL3_FCCA,	APL3,		FCCA,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL1_ETSIC,	APL1,		ETSIC,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL2_ETSIC,	APL2,		ETSIC,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{APL2_APLD,	APL2,		APLD,		NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },

	{MKK1_MKKA,	MKK1,		MKKA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKKA, CTRY_JAPAN },
	{MKK1_MKKB,	MKK1,		MKKA,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC| LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKKA | PSCAN_MKKA_G, CTRY_JAPAN1 },
	{MKK1_FCCA,	MKK1,		FCCA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC| LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1, CTRY_JAPAN2 },
	{MKK1_MKKA1,	MKK1,		MKKA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC| LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKKA1 | PSCAN_MKKA1_G, CTRY_JAPAN4 },
	{MKK1_MKKA2,	MKK1,		MKKA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC| LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKKA2 | PSCAN_MKKA2_G, CTRY_JAPAN5 },
	{MKK1_MKKC,	MKK1,		MKKC,		DISALLOW_ADHOC_11A_TURB | NEED_NFC| LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1, CTRY_JAPAN6 },

	/* MKK2 */
	{MKK2_MKKA,	MKK2,		MKKA,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC| LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK2 | PSCAN_MKKA | PSCAN_MKKA_G, CTRY_JAPAN3 },

	/* MKK3 */
	{MKK3_MKKA,	MKK3,	MKKA,	DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC , PSCAN_MKKA, CTRY_DEFAULT },
	{MKK3_MKKB,	MKK3,		MKKA,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKKA | PSCAN_MKKA_G, CTRY_JAPAN7 },
	{MKK3_MKKA1,	MKK3,	MKKA,	DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKKA1 | PSCAN_MKKA1_G, CTRY_DEFAULT },
	{MKK3_MKKA2,MKK3,		MKKA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKKA2 | PSCAN_MKKA2_G, CTRY_JAPAN8 },
	{MKK3_MKKC,	MKK3,		MKKC,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, NO_PSCAN, CTRY_JAPAN9 },
	{MKK3_FCCA,	MKK3,	FCCA,	DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, NO_PSCAN, CTRY_DEFAULT },

	/* MKK4 */
	{MKK4_MKKB,	MKK4,		MKKA,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3 | PSCAN_MKKA | PSCAN_MKKA_G, CTRY_JAPAN10 },
	{MKK4_MKKA1,	MKK4,	MKKA,	DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3 | PSCAN_MKKA1 | PSCAN_MKKA1_G, CTRY_DEFAULT },
	{MKK4_MKKA2,	MKK4,		MKKA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3 |PSCAN_MKKA2 | PSCAN_MKKA2_G, CTRY_JAPAN11 },
	{MKK4_MKKC,	MKK4,		MKKC,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3, CTRY_JAPAN12 },
	{MKK4_FCCA,	MKK4,	FCCA,	DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3, CTRY_DEFAULT },

	/* MKK5 */
	{MKK5_MKKB,	MKK5,		MKKA,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3 | PSCAN_MKKA | PSCAN_MKKA_G, CTRY_JAPAN13 },
	{MKK5_MKKA2,MKK5,		MKKA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3 | PSCAN_MKKA2 | PSCAN_MKKA2_G, CTRY_JAPAN14 },
	{MKK5_MKKC,	MKK5,		MKKC,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3, CTRY_JAPAN15 },

	/* MKK6 */
	{MKK6_MKKB,	MKK6,		MKKA,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKKA | PSCAN_MKKA_G, CTRY_JAPAN16 },
	{MKK6_MKKA2,	MKK6,		MKKA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKKA2 | PSCAN_MKKA2_G, CTRY_JAPAN17 },
	{MKK6_MKKC,	MKK6,		MKKC,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1, CTRY_JAPAN18 },

	/* MKK7 */
	{MKK7_MKKB,	MKK7,		MKKA,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKK3 | PSCAN_MKKA | PSCAN_MKKA_G, CTRY_JAPAN19 },
	{MKK7_MKKA2, MKK7,		MKKA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKK3 | PSCAN_MKKA2 | PSCAN_MKKA2_G, CTRY_JAPAN20 },
	{MKK7_MKKC,	MKK7,		MKKC,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKK3, CTRY_JAPAN21 },

	/* MKK8 */
	{MKK8_MKKB,	MKK8,		MKKA,		DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKK3 | PSCAN_MKKA | PSCAN_MKKA_G, CTRY_JAPAN22 },
	{MKK8_MKKA2,MKK8,		MKKA,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKK3 | PSCAN_MKKA2 | PSCAN_MKKA2_G, CTRY_JAPAN23 },
	{MKK8_MKKC,	MKK8,		MKKC,		DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK1 | PSCAN_MKK3 , CTRY_JAPAN24 },

	{MKK9_MKKA,	MKK9,	MKKA,	DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3 | PSCAN_MKKA | PSCAN_MKKA_G, CTRY_DEFAULT },
	{MKK10_MKKA,	MKK10,	MKKA,	DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB | NEED_NFC | LIMIT_FRAME_4MS, NEED_NFC, PSCAN_MKK3 | PSCAN_MKKA | PSCAN_MKKA_G, CTRY_DEFAULT },

		/* These are super domains */
	{WOR0_WORLD,	WOR0_WORLD,	WOR0_WORLD,	NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WOR1_WORLD,	WOR1_WORLD,	WOR1_WORLD,	DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WOR2_WORLD,	WOR2_WORLD,	WOR2_WORLD,	DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WOR3_WORLD,	WOR3_WORLD,	WOR3_WORLD,	NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WOR4_WORLD,	WOR4_WORLD,	WOR4_WORLD,	DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WOR5_ETSIC,	WOR5_ETSIC,	WOR5_ETSIC,	DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WOR01_WORLD,	WOR01_WORLD,	WOR01_WORLD,	NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WOR02_WORLD,	WOR02_WORLD,	WOR02_WORLD,	NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{EU1_WORLD,	EU1_WORLD,	EU1_WORLD,	NO_REQ, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WOR9_WORLD,	WOR9_WORLD,	WOR9_WORLD,	DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WORA_WORLD,	WORA_WORLD,	WORA_WORLD,	DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
	{WORB_WORLD,	WORB_WORLD,	WORB_WORLD,	DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB, NO_REQ, PSCAN_DEFER, CTRY_DEFAULT },
};

/* 
 * The following tables are the master list for all different freqeuncy
 * bands with the complete matrix of all possible flags and settings
 * for each band if it is used in ANY reg domain.
 */

#define DEF_REGDMN		FCC1_FCCA
#define	COUNTRY_ERD_FLAG        0x8000
#define WORLDWIDE_ROAMING_FLAG  0x4000

typedef struct {
	HAL_CTRY_CODE		countryCode;	   
	HAL_REG_DOMAIN		regDmnEnum;
} COUNTRY_CODE_TO_ENUM_RD;

static COUNTRY_CODE_TO_ENUM_RD allCountries[] = {
	{ CTRY_DEBUG,       NO_ENUMRD },
	{ CTRY_DEFAULT,     DEF_REGDMN },
	{ CTRY_ALBANIA,     NULL1_WORLD },
	{ CTRY_ALGERIA,     NULL1_WORLD },
	{ CTRY_ARGENTINA,   APL3_WORLD },
	{ CTRY_ARMENIA,     ETSI4_WORLD },
	{ CTRY_AUSTRALIA,   FCC2_WORLD },
	{ CTRY_AUSTRIA,     ETSI1_WORLD },
	{ CTRY_AZERBAIJAN,  ETSI4_WORLD },
	{ CTRY_BAHRAIN,     APL6_WORLD },
	{ CTRY_BELARUS,     NULL1_WORLD },
	{ CTRY_BELGIUM,     ETSI1_WORLD },
	{ CTRY_BELIZE,      APL1_ETSIC },
	{ CTRY_BOLIVIA,     APL1_ETSIC },
	{ CTRY_BRAZIL,      FCC3_WORLD },
	{ CTRY_BRUNEI_DARUSSALAM,APL1_WORLD },
	{ CTRY_BULGARIA,    ETSI6_WORLD },
	{ CTRY_CANADA,      FCC2_FCCA },
	{ CTRY_CHILE,       APL6_WORLD },
	{ CTRY_CHINA,       APL1_WORLD },
	{ CTRY_COLOMBIA,    FCC1_FCCA },
	{ CTRY_COSTA_RICA,  NULL1_WORLD },
	{ CTRY_CROATIA,     ETSI3_WORLD },
	{ CTRY_CYPRUS,      ETSI1_WORLD },
	{ CTRY_CZECH,       ETSI1_WORLD },
	{ CTRY_DENMARK,     ETSI1_WORLD },
	{ CTRY_DOMINICAN_REPUBLIC,FCC1_FCCA },
	{ CTRY_ECUADOR,     NULL1_WORLD },
	{ CTRY_EGYPT,       ETSI3_WORLD },
	{ CTRY_EL_SALVADOR, NULL1_WORLD },
	{ CTRY_ESTONIA,     ETSI1_WORLD },
	{ CTRY_FINLAND,     ETSI1_WORLD },
	{ CTRY_FRANCE,      ETSI1_WORLD },
	{ CTRY_FRANCE2,     ETSI3_WORLD },
	{ CTRY_GEORGIA,     ETSI4_WORLD },
	{ CTRY_GERMANY,     ETSI1_WORLD },
	{ CTRY_GREECE,      ETSI1_WORLD },
	{ CTRY_GUATEMALA,   FCC1_FCCA },
	{ CTRY_HONDURAS,    NULL1_WORLD },
	{ CTRY_HONG_KONG,   FCC2_WORLD },
	{ CTRY_HUNGARY,     ETSI1_WORLD },
	{ CTRY_ICELAND,     ETSI1_WORLD },
	{ CTRY_INDIA,       APL6_WORLD },
	{ CTRY_INDONESIA,   APL1_WORLD },
	{ CTRY_IRAN,        APL1_WORLD },
	{ CTRY_IRELAND,     ETSI1_WORLD },
	{ CTRY_ISRAEL,      NULL1_WORLD },
	{ CTRY_ITALY,       ETSI1_WORLD },
	{ CTRY_JAPAN,       MKK1_MKKA },
	{ CTRY_JAPAN1,      MKK1_MKKB },
	{ CTRY_JAPAN2,      MKK1_FCCA },
	{ CTRY_JAPAN3,      MKK2_MKKA },
	{ CTRY_JAPAN4,      MKK1_MKKA1 },
	{ CTRY_JAPAN5,      MKK1_MKKA2 },
	{ CTRY_JAPAN6,      MKK1_MKKC },

	{ CTRY_JAPAN7,      MKK3_MKKB },
	{ CTRY_JAPAN8,      MKK3_MKKA2 },
	{ CTRY_JAPAN9,      MKK3_MKKC },

	{ CTRY_JAPAN10,     MKK4_MKKB },
	{ CTRY_JAPAN11,     MKK4_MKKA2 },
	{ CTRY_JAPAN12,     MKK4_MKKC },

	{ CTRY_JAPAN13,     MKK5_MKKB },
	{ CTRY_JAPAN14,     MKK5_MKKA2 },
	{ CTRY_JAPAN15,     MKK5_MKKC },

	{ CTRY_JAPAN16,     MKK6_MKKB },
	{ CTRY_JAPAN17,     MKK6_MKKA2 },
	{ CTRY_JAPAN18,     MKK6_MKKC },

	{ CTRY_JAPAN19,     MKK7_MKKB },
	{ CTRY_JAPAN20,     MKK7_MKKA2 },
	{ CTRY_JAPAN21,     MKK7_MKKC },

	{ CTRY_JAPAN22,     MKK8_MKKB },
	{ CTRY_JAPAN23,     MKK8_MKKA2 },
	{ CTRY_JAPAN24,     MKK8_MKKC },

	{ CTRY_JORDAN,      APL4_WORLD },
	{ CTRY_KAZAKHSTAN,  NULL1_WORLD },
	{ CTRY_KOREA_NORTH, APL2_WORLD },
	{ CTRY_KOREA_ROC,   APL2_WORLD },
	{ CTRY_KOREA_ROC2,  APL2_WORLD },
	{ CTRY_KOREA_ROC3,  APL9_WORLD },
	{ CTRY_KUWAIT,      NULL1_WORLD },
	{ CTRY_LATVIA,      ETSI1_WORLD },
	{ CTRY_LEBANON,     NULL1_WORLD },
	{ CTRY_LIECHTENSTEIN,ETSI1_WORLD },
	{ CTRY_LITHUANIA,   ETSI1_WORLD },
	{ CTRY_LUXEMBOURG,  ETSI1_WORLD },
	{ CTRY_MACAU,       FCC2_WORLD },
	{ CTRY_MACEDONIA,   NULL1_WORLD },
	{ CTRY_MALAYSIA,    APL8_WORLD },
	{ CTRY_MALTA,       ETSI1_WORLD },
	{ CTRY_MEXICO,      FCC1_FCCA },
	{ CTRY_MONACO,      ETSI4_WORLD },
	{ CTRY_MOROCCO,     NULL1_WORLD },
	{ CTRY_NETHERLANDS, ETSI1_WORLD },
	{ CTRY_NEW_ZEALAND, FCC2_ETSIC },
	{ CTRY_NORWAY,      ETSI1_WORLD },
	{ CTRY_OMAN,        APL6_WORLD },
	{ CTRY_PAKISTAN,    NULL1_WORLD },
	{ CTRY_PANAMA,      FCC1_FCCA },
	{ CTRY_PERU,        APL1_WORLD },
	{ CTRY_PHILIPPINES, FCC3_WORLD },
	{ CTRY_POLAND,      ETSI1_WORLD },
	{ CTRY_PORTUGAL,    ETSI1_WORLD },
	{ CTRY_PUERTO_RICO, FCC1_FCCA },
	{ CTRY_QATAR,       NULL1_WORLD },
	{ CTRY_ROMANIA,     NULL1_WORLD },
	{ CTRY_RUSSIA,      NULL1_WORLD },
	{ CTRY_SAUDI_ARABIA,FCC2_WORLD },
	{ CTRY_SINGAPORE,   APL6_WORLD },
	{ CTRY_SLOVAKIA,    ETSI1_WORLD },
	{ CTRY_SLOVENIA,    ETSI1_WORLD },
	{ CTRY_SOUTH_AFRICA,FCC3_WORLD },
	{ CTRY_SPAIN,       ETSI1_WORLD },
	{ CTRY_SWEDEN,      ETSI1_WORLD },
	{ CTRY_SWITZERLAND, ETSI1_WORLD },
	{ CTRY_SYRIA,       NULL1_WORLD },
	{ CTRY_TAIWAN,      APL3_FCCA },
	{ CTRY_THAILAND,    FCC3_WORLD },
	{ CTRY_TRINIDAD_Y_TOBAGO,ETSI4_WORLD },
	{ CTRY_TUNISIA,     ETSI3_WORLD },
	{ CTRY_TURKEY,      ETSI3_WORLD },
	{ CTRY_UKRAINE,     NULL1_WORLD },
	{ CTRY_UAE,         NULL1_WORLD },
	{ CTRY_UNITED_KINGDOM, ETSI1_WORLD },
	{ CTRY_UNITED_STATES, FCC1_FCCA },
	{ CTRY_UNITED_STATES_FCC49,FCC4_FCCA },
	{ CTRY_URUGUAY,     FCC1_WORLD },
	{ CTRY_UZBEKISTAN,  FCC3_FCCA },
	{ CTRY_VENEZUELA,   APL2_ETSIC },
	{ CTRY_VIET_NAM,    NULL1_WORLD },
	{ CTRY_ZIMBABWE,    NULL1_WORLD }
};

/* Bit masks for DFS per regdomain */
enum {
	NO_DFS   = 0x0000000000000000ULL,	/* NB: must be zero */
	DFS_FCC3 = 0x0000000000000001ULL,
	DFS_ETSI = 0x0000000000000002ULL,
	DFS_MKK4 = 0x0000000000000004ULL,
};

#define	AFTER(x)	((x)+1)

/*
 * Frequency band collections are defined using bitmasks.  Each bit
 * in a mask is the index of an entry in one of the following tables.
 * Bitmasks are BMLEN*64 bits so if a table grows beyond that the bit
 * vectors must be enlarged or the tables split somehow (e.g. split
 * 1/2 and 1/4 rate channels into a separate table).
 *
 * Beware of ordering; the indices are defined relative to the preceding
 * entry so if things get off there will be confusion.  A good way to
 * check the indices is to collect them in a switch statement in a stub
 * function so the compiler checks for duplicates.
 */

typedef struct {
	uint16_t	lowChannel;	/* Low channel center in MHz */
	uint16_t	highChannel;	/* High Channel center in MHz */
	uint8_t		powerDfs;	/* Max power (dBm) for channel
					   range when using DFS */
	uint8_t		antennaMax;	/* Max allowed antenna gain */
	uint8_t		channelBW;	/* Bandwidth of the channel */
	uint8_t		channelSep;	/* Channel separation within
					   the band */
	uint64_t	useDfs;		/* Use DFS in the RegDomain
					   if corresponding bit is set */
	uint64_t	usePassScan;	/* Use Passive Scan in the RegDomain
					   if corresponding bit is set */
} REG_DMN_FREQ_BAND;

/*
 * 5GHz 11A channel tags
 */
static REG_DMN_FREQ_BAND regDmn5GhzFreq[] = {
	{ 4915, 4925, 23, 0, 10,  5, NO_DFS, PSCAN_MKK2 },
#define	F1_4915_4925	0
	{ 4935, 4945, 23, 0, 10,  5, NO_DFS, PSCAN_MKK2 },
#define	F1_4935_4945	AFTER(F1_4915_4925)
	{ 4920, 4980, 23, 0, 20, 20, NO_DFS, PSCAN_MKK2 },
#define	F1_4920_4980	AFTER(F1_4935_4945)
	{ 4942, 4987, 27, 6,  5,  5, NO_DFS, PSCAN_FCC },
#define	F1_4942_4987	AFTER(F1_4920_4980)
	{ 4945, 4985, 30, 6, 10,  5, NO_DFS, PSCAN_FCC },
#define	F1_4945_4985	AFTER(F1_4942_4987)
	{ 4950, 4980, 33, 6, 20,  5, NO_DFS, PSCAN_FCC },
#define	F1_4950_4980	AFTER(F1_4945_4985)
	{ 5035, 5040, 23, 0, 10,  5, NO_DFS, PSCAN_MKK2 },
#define	F1_5035_5040	AFTER(F1_4950_4980)
	{ 5040, 5080, 23, 0, 20, 20, NO_DFS, PSCAN_MKK2 },
#define	F1_5040_5080	AFTER(F1_5035_5040)
	{ 5055, 5055, 23, 0, 10,  5, NO_DFS, PSCAN_MKK2 },
#define	F1_5055_5055	AFTER(F1_5040_5080)

	{ 5120, 5240, 5,  6, 20, 20, NO_DFS, NO_PSCAN },
#define	F1_5120_5240	AFTER(F1_5055_5055)
	{ 5120, 5240, 5,  6, 10, 10, NO_DFS, NO_PSCAN },
#define	F2_5120_5240	AFTER(F1_5120_5240)
	{ 5120, 5240, 5,  6,  5,  5, NO_DFS, NO_PSCAN },
#define	F3_5120_5240	AFTER(F2_5120_5240)

	{ 5170, 5230, 23, 0, 20, 20, NO_DFS, PSCAN_MKK1 | PSCAN_MKK2 },
#define	F1_5170_5230	AFTER(F3_5120_5240)
	{ 5170, 5230, 20, 0, 20, 20, NO_DFS, PSCAN_MKK1 | PSCAN_MKK2 },
#define	F2_5170_5230	AFTER(F1_5170_5230)

	{ 5180, 5240, 15, 0, 20, 20, NO_DFS, PSCAN_FCC | PSCAN_ETSI },
#define	F1_5180_5240	AFTER(F2_5170_5230)
	{ 5180, 5240, 17, 6, 20, 20, NO_DFS, PSCAN_FCC },
#define	F2_5180_5240	AFTER(F1_5180_5240)
	{ 5180, 5240, 18, 0, 20, 20, NO_DFS, PSCAN_FCC | PSCAN_ETSI },
#define	F3_5180_5240	AFTER(F2_5180_5240)
	{ 5180, 5240, 20, 0, 20, 20, NO_DFS, PSCAN_FCC | PSCAN_ETSI },
#define	F4_5180_5240	AFTER(F3_5180_5240)
	{ 5180, 5240, 23, 0, 20, 20, NO_DFS, PSCAN_FCC | PSCAN_ETSI },
#define	F5_5180_5240	AFTER(F4_5180_5240)
	{ 5180, 5240, 23, 6, 20, 20, NO_DFS, PSCAN_FCC },
#define	F6_5180_5240	AFTER(F5_5180_5240)
	{ 5180, 5240, 17, 6, 20, 10, NO_DFS, PSCAN_FCC },
#define	F7_5180_5240	AFTER(F6_5180_5240)
	{ 5180, 5240, 17, 6, 20,  5, NO_DFS, PSCAN_FCC },
#define	F8_5180_5240	AFTER(F7_5180_5240)
	{ 5180, 5320, 20, 6, 20, 20, DFS_ETSI, PSCAN_ETSI },

#define	F1_5180_5320	AFTER(F8_5180_5240)
	{ 5240, 5280, 23, 0, 20, 20, DFS_FCC3, PSCAN_FCC | PSCAN_ETSI },

#define	F1_5240_5280	AFTER(F1_5180_5320)
	{ 5260, 5280, 23, 0, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_FCC | PSCAN_ETSI },

#define	F1_5260_5280	AFTER(F1_5240_5280)
	{ 5260, 5320, 18, 0, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_FCC | PSCAN_ETSI },

#define	F1_5260_5320	AFTER(F1_5260_5280)
	{ 5260, 5320, 20, 0, 20, 20, DFS_FCC3 | DFS_ETSI | DFS_MKK4, PSCAN_FCC | PSCAN_ETSI | PSCAN_MKK3  },
#define	F2_5260_5320	AFTER(F1_5260_5320)

	{ 5260, 5320, 20, 6, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_FCC },
#define	F3_5260_5320	AFTER(F2_5260_5320)
	{ 5260, 5320, 23, 6, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_FCC },
#define	F4_5260_5320	AFTER(F3_5260_5320)
	{ 5260, 5320, 23, 6, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_FCC },
#define	F5_5260_5320	AFTER(F4_5260_5320)
	{ 5260, 5320, 30, 0, 20, 20, NO_DFS, NO_PSCAN },
#define	F6_5260_5320	AFTER(F5_5260_5320)
	{ 5260, 5320, 23, 6, 20, 10, DFS_FCC3 | DFS_ETSI, PSCAN_FCC },
#define	F7_5260_5320	AFTER(F6_5260_5320)
	{ 5260, 5320, 23, 6, 20,  5, DFS_FCC3 | DFS_ETSI, PSCAN_FCC },
#define	F8_5260_5320	AFTER(F7_5260_5320)

	{ 5260, 5700, 5,  6, 20, 20, DFS_FCC3 | DFS_ETSI, NO_PSCAN },
#define	F1_5260_5700	AFTER(F8_5260_5320)
	{ 5260, 5700, 5,  6, 10, 10, DFS_FCC3 | DFS_ETSI, NO_PSCAN },
#define	F2_5260_5700	AFTER(F1_5260_5700)
	{ 5260, 5700, 5,  6,  5,  5, DFS_FCC3 | DFS_ETSI, NO_PSCAN },
#define	F3_5260_5700	AFTER(F2_5260_5700)

	{ 5280, 5320, 17, 6, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_FCC },
#define	F1_5280_5320	AFTER(F3_5260_5700)

	{ 5500, 5620, 30, 6, 20, 20, DFS_ETSI, PSCAN_ETSI },
#define	F1_5500_5620	AFTER(F1_5280_5320)

	{ 5500, 5700, 20, 6, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_FCC },
#define	F1_5500_5700	AFTER(F1_5500_5620)
	{ 5500, 5700, 27, 0, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_FCC | PSCAN_ETSI },
#define	F2_5500_5700	AFTER(F1_5500_5700)
	{ 5500, 5700, 30, 0, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_FCC | PSCAN_ETSI },
#define	F3_5500_5700	AFTER(F2_5500_5700)
	{ 5500, 5700, 23, 0, 20, 20, DFS_FCC3 | DFS_ETSI | DFS_MKK4, PSCAN_MKK3 | PSCAN_FCC },
#define	F4_5500_5700	AFTER(F3_5500_5700)

	{ 5745, 5805, 23, 0, 20, 20, NO_DFS, NO_PSCAN },
#define	F1_5745_5805	AFTER(F4_5500_5700)
	{ 5745, 5805, 30, 6, 20, 20, NO_DFS, NO_PSCAN },
#define	F2_5745_5805	AFTER(F1_5745_5805)
	{ 5745, 5805, 30, 6, 20, 20, DFS_ETSI, PSCAN_ETSI },
#define	F3_5745_5805	AFTER(F2_5745_5805)
	{ 5745, 5825, 5,  6, 20, 20, NO_DFS, NO_PSCAN },
#define	F1_5745_5825	AFTER(F3_5745_5805)
	{ 5745, 5825, 17, 0, 20, 20, NO_DFS, NO_PSCAN },
#define	F2_5745_5825	AFTER(F1_5745_5825)
	{ 5745, 5825, 20, 0, 20, 20, NO_DFS, NO_PSCAN },
#define	F3_5745_5825	AFTER(F2_5745_5825)
	{ 5745, 5825, 30, 0, 20, 20, NO_DFS, NO_PSCAN },
#define	F4_5745_5825	AFTER(F3_5745_5825)
	{ 5745, 5825, 30, 6, 20, 20, NO_DFS, NO_PSCAN },
#define	F5_5745_5825	AFTER(F4_5745_5825)
	{ 5745, 5825, 30, 6, 20, 20, NO_DFS, NO_PSCAN },
#define	F6_5745_5825	AFTER(F5_5745_5825)
	{ 5745, 5825, 5,  6, 10, 10, NO_DFS, NO_PSCAN },
#define	F7_5745_5825	AFTER(F6_5745_5825)
	{ 5745, 5825, 5,  6,  5,  5, NO_DFS, NO_PSCAN },
#define	F8_5745_5825	AFTER(F7_5745_5825)
	{ 5745, 5825, 30, 6, 20, 10, NO_DFS, NO_PSCAN },
#define	F9_5745_5825	AFTER(F8_5745_5825)
	{ 5745, 5825, 30, 6, 20,  5, NO_DFS, NO_PSCAN },
#define	F10_5745_5825	AFTER(F9_5745_5825)

	/*
	 * Below are the world roaming channels
	 * All WWR domains have no power limit, instead use the card's CTL
	 * or max power settings.
	 */
	{ 4920, 4980, 30, 0, 20, 20, NO_DFS, PSCAN_WWR },
#define	W1_4920_4980	AFTER(F10_5745_5825)
	{ 5040, 5080, 30, 0, 20, 20, NO_DFS, PSCAN_WWR },
#define	W1_5040_5080	AFTER(W1_4920_4980)
	{ 5170, 5230, 30, 0, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_WWR },
#define	W1_5170_5230	AFTER(W1_5040_5080)
	{ 5180, 5240, 30, 0, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_WWR },
#define	W1_5180_5240	AFTER(W1_5170_5230)
	{ 5260, 5320, 30, 0, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_WWR },
#define	W1_5260_5320	AFTER(W1_5180_5240)
	{ 5745, 5825, 30, 0, 20, 20, NO_DFS, PSCAN_WWR },
#define	W1_5745_5825	AFTER(W1_5260_5320)
	{ 5500, 5700, 30, 0, 20, 20, DFS_FCC3 | DFS_ETSI, PSCAN_WWR },
#define	W1_5500_5700	AFTER(W1_5745_5825)
	{ 5260, 5320, 30, 0, 20, 20, NO_DFS, NO_PSCAN },
#define	W2_5260_5320	AFTER(W1_5500_5700)
	{ 5180, 5240, 30, 0, 20, 20, NO_DFS, NO_PSCAN },
#define	W2_5180_5240	AFTER(W2_5260_5320)
	{ 5825, 5825, 30, 0, 20, 20, NO_DFS, PSCAN_WWR },
#define	W2_5825_5825	AFTER(W2_5180_5240)
};

/*
 * 5GHz Turbo (dynamic & static) tags
 */
static REG_DMN_FREQ_BAND regDmn5GhzTurboFreq[] = {
	{ 5130, 5210, 5,  6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_5130_5210	0
	{ 5250, 5330, 5,  6, 40, 40, DFS_FCC3, NO_PSCAN },
#define	T1_5250_5330	AFTER(T1_5130_5210)
	{ 5370, 5490, 5,  6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_5370_5490	AFTER(T1_5250_5330)
	{ 5530, 5650, 5,  6, 40, 40, DFS_FCC3, NO_PSCAN },
#define	T1_5530_5650	AFTER(T1_5370_5490)

	{ 5150, 5190, 5,  6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_5150_5190	AFTER(T1_5530_5650)
	{ 5230, 5310, 5,  6, 40, 40, DFS_FCC3, NO_PSCAN },
#define	T1_5230_5310	AFTER(T1_5150_5190)
	{ 5350, 5470, 5,  6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_5350_5470	AFTER(T1_5230_5310)
	{ 5510, 5670, 5,  6, 40, 40, DFS_FCC3, NO_PSCAN },
#define	T1_5510_5670	AFTER(T1_5350_5470)

	{ 5200, 5240, 17, 6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_5200_5240	AFTER(T1_5510_5670)
	{ 5200, 5240, 23, 6, 40, 40, NO_DFS, NO_PSCAN },
#define	T2_5200_5240	AFTER(T1_5200_5240)
	{ 5210, 5210, 17, 6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_5210_5210	AFTER(T2_5200_5240)
	{ 5210, 5210, 23, 0, 40, 40, NO_DFS, NO_PSCAN },
#define	T2_5210_5210	AFTER(T1_5210_5210)

	{ 5280, 5280, 23, 6, 40, 40, DFS_FCC3, PSCAN_FCC_T },
#define	T1_5280_5280	AFTER(T2_5210_5210)
	{ 5280, 5280, 20, 6, 40, 40, DFS_FCC3, PSCAN_FCC_T },
#define	T2_5280_5280	AFTER(T1_5280_5280)
	{ 5250, 5250, 17, 0, 40, 40, DFS_FCC3, PSCAN_FCC_T },
#define	T1_5250_5250	AFTER(T2_5280_5280)
	{ 5290, 5290, 20, 0, 40, 40, DFS_FCC3, PSCAN_FCC_T },
#define	T1_5290_5290	AFTER(T1_5250_5250)
	{ 5250, 5290, 20, 0, 40, 40, DFS_FCC3, PSCAN_FCC_T },
#define	T1_5250_5290	AFTER(T1_5290_5290)
	{ 5250, 5290, 23, 6, 40, 40, DFS_FCC3, PSCAN_FCC_T },
#define	T2_5250_5290	AFTER(T1_5250_5290)

	{ 5540, 5660, 20, 6, 40, 40, DFS_FCC3, PSCAN_FCC_T },
#define	T1_5540_5660	AFTER(T2_5250_5290)
	{ 5760, 5800, 20, 0, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_5760_5800	AFTER(T1_5540_5660)
	{ 5760, 5800, 30, 6, 40, 40, NO_DFS, NO_PSCAN },
#define	T2_5760_5800	AFTER(T1_5760_5800)

	{ 5765, 5805, 30, 6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_5765_5805	AFTER(T2_5760_5800)

	/*
	 * Below are the WWR frequencies
	 */
	{ 5210, 5250, 15, 0, 40, 40, DFS_FCC3 | DFS_ETSI, PSCAN_WWR },
#define	WT1_5210_5250	AFTER(T1_5765_5805)
	{ 5290, 5290, 18, 0, 40, 40, DFS_FCC3 | DFS_ETSI, PSCAN_WWR },
#define	WT1_5290_5290	AFTER(WT1_5210_5250)
	{ 5540, 5660, 20, 0, 40, 40, DFS_FCC3 | DFS_ETSI, PSCAN_WWR },
#define	WT1_5540_5660	AFTER(WT1_5290_5290)
	{ 5760, 5800, 20, 0, 40, 40, NO_DFS, PSCAN_WWR },
#define	WT1_5760_5800	AFTER(WT1_5540_5660)
};

/*
 * 2GHz 11b channel tags
 */
static REG_DMN_FREQ_BAND regDmn2GhzFreq[] = {
	{ 2312, 2372, 5,  6, 20, 5, NO_DFS, NO_PSCAN },
#define	F1_2312_2372	0
	{ 2312, 2372, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	F2_2312_2372	AFTER(F1_2312_2372)

	{ 2412, 2472, 5,  6, 20, 5, NO_DFS, NO_PSCAN },
#define	F1_2412_2472	AFTER(F2_2312_2372)
	{ 2412, 2472, 20, 0, 20, 5, NO_DFS, PSCAN_MKKA },
#define	F2_2412_2472	AFTER(F1_2412_2472)
	{ 2412, 2472, 30, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	F3_2412_2472	AFTER(F2_2412_2472)

	{ 2412, 2462, 27, 6, 20, 5, NO_DFS, NO_PSCAN },
#define	F1_2412_2462	AFTER(F3_2412_2472)
	{ 2412, 2462, 20, 0, 20, 5, NO_DFS, PSCAN_MKKA },
#define	F2_2412_2462	AFTER(F1_2412_2462)

	{ 2432, 2442, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	F1_2432_2442	AFTER(F2_2412_2462)

	{ 2457, 2472, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	F1_2457_2472	AFTER(F1_2432_2442)

	{ 2467, 2472, 20, 0, 20, 5, NO_DFS, PSCAN_MKKA2 | PSCAN_MKKA },
#define	F1_2467_2472	AFTER(F1_2457_2472)

	{ 2484, 2484, 5,  6, 20, 5, NO_DFS, NO_PSCAN },
#define	F1_2484_2484	AFTER(F1_2467_2472)
	{ 2484, 2484, 20, 0, 20, 5, NO_DFS, PSCAN_MKKA | PSCAN_MKKA1 | PSCAN_MKKA2 },
#define	F2_2484_2484	AFTER(F1_2484_2484)

	{ 2512, 2732, 5,  6, 20, 5, NO_DFS, NO_PSCAN },
#define	F1_2512_2732	AFTER(F2_2484_2484)

	/*
	 * WWR have powers opened up to 20dBm.
	 * Limits should often come from CTL/Max powers
	 */
	{ 2312, 2372, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	W1_2312_2372	AFTER(F1_2512_2732)
	{ 2412, 2412, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	W1_2412_2412	AFTER(W1_2312_2372)
	{ 2417, 2432, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	W1_2417_2432	AFTER(W1_2412_2412)
	{ 2437, 2442, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	W1_2437_2442	AFTER(W1_2417_2432)
	{ 2447, 2457, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	W1_2447_2457	AFTER(W1_2437_2442)
	{ 2462, 2462, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	W1_2462_2462	AFTER(W1_2447_2457)
	{ 2467, 2467, 20, 0, 20, 5, NO_DFS, PSCAN_WWR | IS_ECM_CHAN },
#define	W1_2467_2467	AFTER(W1_2462_2462)
	{ 2467, 2467, 20, 0, 20, 5, NO_DFS, NO_PSCAN | IS_ECM_CHAN },
#define	W2_2467_2467	AFTER(W1_2467_2467)
	{ 2472, 2472, 20, 0, 20, 5, NO_DFS, PSCAN_WWR | IS_ECM_CHAN },
#define	W1_2472_2472	AFTER(W2_2467_2467)
	{ 2472, 2472, 20, 0, 20, 5, NO_DFS, NO_PSCAN | IS_ECM_CHAN },
#define	W2_2472_2472	AFTER(W1_2472_2472)
	{ 2484, 2484, 20, 0, 20, 5, NO_DFS, PSCAN_WWR | IS_ECM_CHAN },
#define	W1_2484_2484	AFTER(W2_2472_2472)
	{ 2484, 2484, 20, 0, 20, 5, NO_DFS, NO_PSCAN | IS_ECM_CHAN },
#define	W2_2484_2484	AFTER(W1_2484_2484)
};

/*
 * 2GHz 11g channel tags
 */
static REG_DMN_FREQ_BAND regDmn2Ghz11gFreq[] = {
	{ 2312, 2372, 5,  6, 20, 5, NO_DFS, NO_PSCAN },
#define	G1_2312_2372	0
	{ 2312, 2372, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	G2_2312_2372	AFTER(G1_2312_2372)
	{ 2312, 2372, 5,  6, 10, 5, NO_DFS, NO_PSCAN },
#define	G3_2312_2372	AFTER(G2_2312_2372)
	{ 2312, 2372, 5,  6,  5, 5, NO_DFS, NO_PSCAN },
#define	G4_2312_2372	AFTER(G3_2312_2372)

	{ 2412, 2472, 5,  6, 20, 5, NO_DFS, NO_PSCAN },
#define	G1_2412_2472	AFTER(G4_2312_2372)
	{ 2412, 2472, 20, 0, 20, 5,  NO_DFS, PSCAN_MKKA_G },
#define	G2_2412_2472	AFTER(G1_2412_2472)
	{ 2412, 2472, 30, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	G3_2412_2472	AFTER(G2_2412_2472)
	{ 2412, 2472, 5,  6, 10, 5, NO_DFS, NO_PSCAN },
#define	G4_2412_2472	AFTER(G3_2412_2472)
	{ 2412, 2472, 5,  6,  5, 5, NO_DFS, NO_PSCAN },
#define	G5_2412_2472	AFTER(G4_2412_2472)

	{ 2412, 2462, 27, 6, 20, 5, NO_DFS, NO_PSCAN },
#define	G1_2412_2462	AFTER(G5_2412_2472)
	{ 2412, 2462, 20, 0, 20, 5, NO_DFS, PSCAN_MKKA_G },
#define	G2_2412_2462	AFTER(G1_2412_2462)
	{ 2412, 2462, 27, 6, 10, 5, NO_DFS, NO_PSCAN },
#define	G3_2412_2462	AFTER(G2_2412_2462)
	{ 2412, 2462, 27, 6,  5, 5, NO_DFS, NO_PSCAN },
#define	G4_2412_2462	AFTER(G3_2412_2462)
	
	{ 2432, 2442, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	G1_2432_2442	AFTER(G4_2412_2462)

	{ 2457, 2472, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	G1_2457_2472	AFTER(G1_2432_2442)

	{ 2512, 2732, 5,  6, 20, 5, NO_DFS, NO_PSCAN },
#define	G1_2512_2732	AFTER(G1_2457_2472)
	{ 2512, 2732, 5,  6, 10, 5, NO_DFS, NO_PSCAN },
#define	G2_2512_2732	AFTER(G1_2512_2732)
	{ 2512, 2732, 5,  6,  5, 5, NO_DFS, NO_PSCAN },
#define	G3_2512_2732	AFTER(G2_2512_2732)

	{ 2467, 2472, 20, 0, 20, 5, NO_DFS, PSCAN_MKKA2 | PSCAN_MKKA },
#define	G1_2467_2472	AFTER(G3_2512_2732)

	/*
	 * WWR open up the power to 20dBm
	 */
	{ 2312, 2372, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	WG1_2312_2372	AFTER(G1_2467_2472)
	{ 2412, 2412, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	WG1_2412_2412	AFTER(WG1_2312_2372)
	{ 2417, 2432, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	WG1_2417_2432	AFTER(WG1_2412_2412)
	{ 2437, 2442, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	WG1_2437_2442	AFTER(WG1_2417_2432)
	{ 2447, 2457, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	WG1_2447_2457	AFTER(WG1_2437_2442)
	{ 2462, 2462, 20, 0, 20, 5, NO_DFS, NO_PSCAN },
#define	WG1_2462_2462	AFTER(WG1_2447_2457)
	{ 2467, 2467, 20, 0, 20, 5, NO_DFS, PSCAN_WWR | IS_ECM_CHAN },
#define	WG1_2467_2467	AFTER(WG1_2462_2462)
	{ 2467, 2467, 20, 0, 20, 5, NO_DFS, NO_PSCAN | IS_ECM_CHAN },
#define	WG2_2467_2467	AFTER(WG1_2467_2467)
	{ 2472, 2472, 20, 0, 20, 5, NO_DFS, PSCAN_WWR | IS_ECM_CHAN },
#define	WG1_2472_2472	AFTER(WG2_2467_2467)
	{ 2472, 2472, 20, 0, 20, 5, NO_DFS, NO_PSCAN | IS_ECM_CHAN },
#define	WG2_2472_2472	AFTER(WG1_2472_2472)
};

/*
 * 2GHz Dynamic turbo tags
 */
static REG_DMN_FREQ_BAND regDmn2Ghz11gTurboFreq[] = {
	{ 2312, 2372, 5,  6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_2312_2372	0
	{ 2437, 2437, 5,  6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_2437_2437	AFTER(T1_2312_2372)
	{ 2437, 2437, 20, 6, 40, 40, NO_DFS, NO_PSCAN },
#define	T2_2437_2437	AFTER(T1_2437_2437)
	{ 2437, 2437, 18, 6, 40, 40, NO_DFS, PSCAN_WWR },
#define	T3_2437_2437	AFTER(T2_2437_2437)
	{ 2512, 2732, 5,  6, 40, 40, NO_DFS, NO_PSCAN },
#define	T1_2512_2732	AFTER(T3_2437_2437)
};

typedef struct regDomain {
	uint16_t regDmnEnum;		/* value from EnumRd table */
	uint8_t conformanceTestLimit;
	uint32_t flags;			/* Requirement flags (AdHoc disallow,
					   noise floor cal needed, etc) */
	uint64_t dfsMask;		/* DFS bitmask for 5Ghz tables */
	uint64_t pscan;			/* Bitmask for passive scan */
	chanbmask_t chan11a;		/* 11a channels */
	chanbmask_t chan11a_turbo;	/* 11a static turbo channels */
	chanbmask_t chan11a_dyn_turbo;	/* 11a dynamic turbo channels */
	chanbmask_t chan11a_half;	/* 11a 1/2 width channels */
	chanbmask_t chan11a_quarter;	/* 11a 1/4 width channels */
	chanbmask_t chan11b;		/* 11b channels */
	chanbmask_t chan11g;		/* 11g channels */
	chanbmask_t chan11g_turbo;	/* 11g dynamic turbo channels */
	chanbmask_t chan11g_half;	/* 11g 1/2 width channels */
	chanbmask_t chan11g_quarter;	/* 11g 1/4 width channels */
} REG_DOMAIN;

static REG_DOMAIN regDomains[] = {

	{.regDmnEnum		= DEBUG_REG_DMN,
	 .conformanceTestLimit	= FCC,
	 .dfsMask		= DFS_FCC3,
	 .chan11a		= BM4(F1_4950_4980,
				      F1_5120_5240,
				      F1_5260_5700,
				      F1_5745_5825),
	 .chan11a_half		= BM4(F1_4945_4985,
				      F2_5120_5240,
				      F2_5260_5700,
				      F7_5745_5825),
	 .chan11a_quarter	= BM4(F1_4942_4987,
				      F3_5120_5240,
				      F3_5260_5700,
				      F8_5745_5825),
	 .chan11a_turbo		= BM8(T1_5130_5210,
				      T1_5250_5330,
				      T1_5370_5490,
				      T1_5530_5650,
				      T1_5150_5190,
				      T1_5230_5310,
				      T1_5350_5470,
				      T1_5510_5670),
	 .chan11a_dyn_turbo	= BM4(T1_5200_5240,
				      T1_5280_5280,
				      T1_5540_5660,
				      T1_5765_5805),
	 .chan11b		= BM4(F1_2312_2372,
				      F1_2412_2472,
				      F1_2484_2484,
				      F1_2512_2732),
	 .chan11g		= BM3(G1_2312_2372, G1_2412_2472, G1_2512_2732),
	 .chan11g_turbo		= BM3(T1_2312_2372, T1_2437_2437, T1_2512_2732),
	 .chan11g_half		= BM3(G2_2312_2372, G4_2412_2472, G2_2512_2732),
	 .chan11g_quarter	= BM3(G3_2312_2372, G5_2412_2472, G3_2512_2732),
	},

	{.regDmnEnum		= APL1,
	 .conformanceTestLimit	= FCC,
	 .chan11a		= BM1(F4_5745_5825),
	},

	{.regDmnEnum		= APL2,
	 .conformanceTestLimit	= FCC,
	 .chan11a		= BM1(F1_5745_5805),
	},

	{.regDmnEnum		= APL3,
	 .conformanceTestLimit	= FCC,
	 .chan11a		= BM2(F1_5280_5320, F2_5745_5805),
	},

	{.regDmnEnum		= APL4,
	 .conformanceTestLimit	= FCC,
	 .chan11a		= BM2(F4_5180_5240, F3_5745_5825),
	},

	{.regDmnEnum		= APL5,
	 .conformanceTestLimit	= FCC,
	 .chan11a		= BM1(F2_5745_5825),
	},

	{.regDmnEnum		= APL6,
	 .conformanceTestLimit	= ETSI,
	 .dfsMask		= DFS_ETSI,
	 .pscan			= PSCAN_FCC_T | PSCAN_FCC,
	 .chan11a		= BM3(F4_5180_5240, F2_5260_5320, F3_5745_5825),
	 .chan11a_turbo		= BM3(T2_5210_5210, T1_5250_5290, T1_5760_5800),
	},

	{.regDmnEnum		= APL8,
	 .conformanceTestLimit	= ETSI,
	 .flags			= DISALLOW_ADHOC_11A|DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM2(F6_5260_5320, F4_5745_5825),
	},

	{.regDmnEnum		= APL9,
	 .conformanceTestLimit	= ETSI,
	 .dfsMask		= DFS_ETSI,
	 .pscan			= PSCAN_ETSI,
	 .flags			= DISALLOW_ADHOC_11A|DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM3(F1_5180_5320, F1_5500_5620, F3_5745_5805),
	},

	{.regDmnEnum		= ETSI1,
	 .conformanceTestLimit	= ETSI,
	 .dfsMask		= DFS_ETSI,
	 .pscan			= PSCAN_ETSI,
	 .flags			= DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM3(W2_5180_5240, F2_5260_5320, F2_5500_5700),
	},

	{.regDmnEnum		= ETSI2,
	 .conformanceTestLimit	= ETSI,
	 .dfsMask		= DFS_ETSI,
	 .pscan			= PSCAN_ETSI,
	 .flags			= DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM1(F3_5180_5240),
	},

	{.regDmnEnum		= ETSI3,
	 .conformanceTestLimit	= ETSI,
	 .dfsMask		= DFS_ETSI,
	 .pscan			= PSCAN_ETSI,
	 .flags			= DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM2(W2_5180_5240, F2_5260_5320),
	},

	{.regDmnEnum		= ETSI4,
	 .conformanceTestLimit	= ETSI,
	 .dfsMask		= DFS_ETSI,
	 .pscan			= PSCAN_ETSI,
	 .flags			= DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM2(F3_5180_5240, F1_5260_5320),
	},

	{.regDmnEnum		= ETSI5,
	 .conformanceTestLimit	= ETSI,
	 .dfsMask		= DFS_ETSI,
	 .pscan			= PSCAN_ETSI,
	 .flags			= DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM1(F1_5180_5240),
	},

	{.regDmnEnum		= ETSI6,
	 .conformanceTestLimit	= ETSI,
	 .dfsMask		= DFS_ETSI,
	 .pscan			= PSCAN_ETSI,
	 .flags			= DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM3(F5_5180_5240, F1_5260_5280, F3_5500_5700),
	},

	{.regDmnEnum		= FCC1,
	 .conformanceTestLimit	= FCC,
	 .chan11a		= BM3(F2_5180_5240, F4_5260_5320, F5_5745_5825),
	 .chan11a_turbo		= BM3(T1_5210_5210, T2_5250_5290, T2_5760_5800),
	 .chan11a_dyn_turbo	= BM3(T1_5200_5240, T1_5280_5280, T1_5765_5805),
	},

	{.regDmnEnum		= FCC2,
	 .conformanceTestLimit	= FCC,
	 .chan11a		= BM3(F6_5180_5240, F5_5260_5320, F6_5745_5825),
	 .chan11a_dyn_turbo	= BM3(T2_5200_5240, T1_5280_5280, T1_5765_5805),
	},

	{.regDmnEnum		= FCC3,
	 .conformanceTestLimit	= FCC,
	 .dfsMask		= DFS_FCC3,
	 .pscan			= PSCAN_FCC | PSCAN_FCC_T,
	 .chan11a		= BM4(F2_5180_5240,
				      F3_5260_5320,
				      F1_5500_5700,
				      F5_5745_5825),
	 .chan11a_turbo		= BM4(T1_5210_5210,
				      T1_5250_5250,
				      T1_5290_5290,
				      T2_5760_5800),
	 .chan11a_dyn_turbo	= BM3(T1_5200_5240, T2_5280_5280, T1_5540_5660),
	},

	{.regDmnEnum		= FCC4,
	 .conformanceTestLimit	= FCC,
	 .dfsMask		= DFS_FCC3,
	 .pscan			= PSCAN_FCC | PSCAN_FCC_T,
	 .chan11a		= BM1(F1_4950_4980),
	 .chan11a_half		= BM1(F1_4945_4985),
	 .chan11a_quarter	= BM1(F1_4942_4987),
	},

	/* FCC1 w/ 1/2 and 1/4 width channels */
	{.regDmnEnum		= FCC5,
	 .conformanceTestLimit	= FCC,
	 .chan11a		= BM3(F2_5180_5240, F4_5260_5320, F5_5745_5825),
	 .chan11a_turbo		= BM3(T1_5210_5210, T2_5250_5290, T2_5760_5800),
	 .chan11a_dyn_turbo	= BM3(T1_5200_5240, T1_5280_5280, T1_5765_5805),
	 .chan11a_half		= BM3(F7_5180_5240, F7_5260_5320, F9_5745_5825),
	 .chan11a_quarter	= BM3(F8_5180_5240, F8_5260_5320,F10_5745_5825),
	},

	{.regDmnEnum		= MKK1,
	 .conformanceTestLimit	= MKK,
	 .pscan			= PSCAN_MKK1,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM1(F1_5170_5230),
	},

	{.regDmnEnum		= MKK2,
	 .conformanceTestLimit	= MKK,
	 .pscan			= PSCAN_MKK2,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM3(F1_4920_4980, F1_5040_5080, F1_5170_5230),
	 .chan11a_half		= BM4(F1_4915_4925,
				      F1_4935_4945,
				      F1_5035_5040,
				      F1_5055_5055),
	},

	/* UNI-1 even */
	{.regDmnEnum		= MKK3,
	 .conformanceTestLimit	= MKK,
	 .pscan			= PSCAN_MKK3,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM1(F4_5180_5240),
	},

	/* UNI-1 even + UNI-2 */
	{.regDmnEnum		= MKK4,
	 .conformanceTestLimit	= MKK,
	 .dfsMask		= DFS_MKK4,
	 .pscan			= PSCAN_MKK3,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM2(F4_5180_5240, F2_5260_5320),
	},

	/* UNI-1 even + UNI-2 + mid-band */
	{.regDmnEnum		= MKK5,
	 .conformanceTestLimit	= MKK,
	 .dfsMask		= DFS_MKK4,
	 .pscan			= PSCAN_MKK3,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM3(F4_5180_5240, F2_5260_5320, F4_5500_5700),
	},

	/* UNI-1 odd + even */
	{.regDmnEnum		= MKK6,
	 .conformanceTestLimit	= MKK,
	 .pscan			= PSCAN_MKK1,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM2(F2_5170_5230, F4_5180_5240),
	},

	/* UNI-1 odd + UNI-1 even + UNI-2 */
	{.regDmnEnum		= MKK7,
	 .conformanceTestLimit	= MKK,
	 .dfsMask		= DFS_MKK4,
	 .pscan			= PSCAN_MKK1 | PSCAN_MKK3,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM3(F1_5170_5230, F4_5180_5240, F2_5260_5320),
	},

	/* UNI-1 odd + UNI-1 even + UNI-2 + mid-band */
	{.regDmnEnum		= MKK8,
	 .conformanceTestLimit	= MKK,
	 .dfsMask		= DFS_MKK4,
	 .pscan			= PSCAN_MKK1 | PSCAN_MKK3,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
	 .chan11a		= BM4(F1_5170_5230,
				      F4_5180_5240,
				      F2_5260_5320,
				      F4_5500_5700),
	},

        /* UNI-1 even + 4.9 GHZ */
        {.regDmnEnum		= MKK9,
	 .conformanceTestLimit	= MKK,
	 .pscan			= PSCAN_MKK3,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
         .chan11a		= BM7(F1_4915_4925,
				      F1_4935_4945,
				      F1_4920_4980,
				      F1_5035_5040,
				      F1_5055_5055,
				      F1_5040_5080,
				      F4_5180_5240),
        },

        /* UNI-1 even + UNI-2 + 4.9 GHZ */
        {.regDmnEnum		= MKK10,
	 .conformanceTestLimit	= MKK,
	 .dfsMask		= DFS_MKK4,
	 .pscan			= PSCAN_MKK3,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
         .chan11a		= BM8(F1_4915_4925,
				      F1_4935_4945,
				      F1_4920_4980,
				      F1_5035_5040,
				      F1_5055_5055,
				      F1_5040_5080,
				      F4_5180_5240,
				      F2_5260_5320),
        },

	/* Defined here to use when 2G channels are authorised for country K2 */
	{.regDmnEnum		= APLD,
	 .conformanceTestLimit	= NO_CTL,
	 .chan11b		= BM2(F2_2312_2372,F2_2412_2472),
	 .chan11g		= BM2(G2_2312_2372,G2_2412_2472),
	},

	{.regDmnEnum		= ETSIA,
	 .conformanceTestLimit	= NO_CTL,
	 .pscan			= PSCAN_ETSIA,
	 .flags			= DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB,
	 .chan11b		= BM1(F1_2457_2472),
	 .chan11g		= BM1(G1_2457_2472),
	 .chan11g_turbo		= BM1(T2_2437_2437)
	},

	{.regDmnEnum		= ETSIB,
	 .conformanceTestLimit	= ETSI,
	 .pscan			= PSCAN_ETSIB,
	 .flags			= DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB,
	 .chan11b		= BM1(F1_2432_2442),
	 .chan11g		= BM1(G1_2432_2442),
	 .chan11g_turbo		= BM1(T2_2437_2437)
	},

	{.regDmnEnum		= ETSIC,
	 .conformanceTestLimit	= ETSI,
	 .pscan			= PSCAN_ETSIC,
	 .flags			= DISALLOW_ADHOC_11A | DISALLOW_ADHOC_11A_TURB,
	 .chan11b		= BM1(F3_2412_2472),
	 .chan11g		= BM1(G3_2412_2472),
	 .chan11g_turbo		= BM1(T2_2437_2437)
	},

	{.regDmnEnum		= FCCA,
	 .conformanceTestLimit	= FCC,
	 .chan11b		= BM1(F1_2412_2462),
	 .chan11g		= BM1(G1_2412_2462),
	 .chan11g_turbo		= BM1(T2_2437_2437),
	},

	/* FCCA w/ 1/2 and 1/4 width channels */
	{.regDmnEnum		= FCCB,
	 .conformanceTestLimit	= FCC,
	 .chan11b		= BM1(F1_2412_2462),
	 .chan11g		= BM1(G1_2412_2462),
	 .chan11g_turbo		= BM1(T2_2437_2437),
	 .chan11g_half		= BM1(G3_2412_2462),
	 .chan11g_quarter	= BM1(G4_2412_2462),
	},

	{.regDmnEnum		= MKKA,
	 .conformanceTestLimit	= MKK,
	 .pscan			= PSCAN_MKKA | PSCAN_MKKA_G
				| PSCAN_MKKA1 | PSCAN_MKKA1_G
				| PSCAN_MKKA2 | PSCAN_MKKA2_G,
	 .flags			= DISALLOW_ADHOC_11A_TURB,
	 .chan11b		= BM3(F2_2412_2462, F1_2467_2472, F2_2484_2484),
	 .chan11g		= BM2(G2_2412_2462, G1_2467_2472),
	 .chan11g_turbo		= BM1(T2_2437_2437)
	},

	{.regDmnEnum		= MKKC,
	 .conformanceTestLimit	= MKK,
	 .chan11b		= BM1(F2_2412_2472),
	 .chan11g		= BM1(G2_2412_2472),
	 .chan11g_turbo		= BM1(T2_2437_2437)
	},

	{.regDmnEnum		= WORLD,
	 .conformanceTestLimit	= ETSI,
	 .chan11b		= BM1(F2_2412_2472),
	 .chan11g		= BM1(G2_2412_2472),
	 .chan11g_turbo		= BM1(T2_2437_2437)
	},

	{.regDmnEnum		= WOR0_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= ADHOC_PER_11D,
	 .chan11a		= BM5(W1_5260_5320,
				      W1_5180_5240,
				      W1_5170_5230,
				      W1_5745_5825,
				      W1_5500_5700),
	 .chan11a_turbo		= BM3(WT1_5210_5250,
				      WT1_5290_5290,
				      WT1_5760_5800),
	 .chan11b		= BM8(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2472_2472,
				      W1_2417_2432,
				      W1_2447_2457,
				      W1_2467_2467,
				      W1_2484_2484),
	 .chan11g		= BM7(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2472_2472,
				      WG1_2417_2432,
				      WG1_2447_2457,
				      WG1_2467_2467),
	 .chan11g_turbo		= BM1(T3_2437_2437)
	},

	{.regDmnEnum		= WOR01_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= ADHOC_PER_11D,
	 .chan11a		= BM5(W1_5260_5320,
				      W1_5180_5240,
				      W1_5170_5230,
				      W1_5745_5825,
				      W1_5500_5700),
	 .chan11a_turbo		= BM3(WT1_5210_5250,
				      WT1_5290_5290,
				      WT1_5760_5800),
	 .chan11b		= BM5(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2417_2432,
				      W1_2447_2457),
	 .chan11g		= BM5(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2417_2432,
				      WG1_2447_2457),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= WOR02_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= ADHOC_PER_11D,
	 .chan11a		= BM5(W1_5260_5320,
				      W1_5180_5240,
				      W1_5170_5230,
				      W1_5745_5825,
				      W1_5500_5700),
	 .chan11a_turbo		= BM3(WT1_5210_5250,
				      WT1_5290_5290,
				      WT1_5760_5800),
	 .chan11b		= BM7(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2472_2472,
				      W1_2417_2432,
				      W1_2447_2457,
				      W1_2467_2467),
	 .chan11g		= BM7(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2472_2472,
				      WG1_2417_2432,
				      WG1_2447_2457,
				      WG1_2467_2467),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= EU1_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= ADHOC_PER_11D,
	 .chan11a		= BM5(W1_5260_5320,
				      W1_5180_5240,
				      W1_5170_5230,
				      W1_5745_5825,
				      W1_5500_5700),
	 .chan11a_turbo		= BM3(WT1_5210_5250,
				      WT1_5290_5290,
				      WT1_5760_5800),
	 .chan11b		= BM7(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W2_2472_2472,
				      W1_2417_2432,
				      W1_2447_2457,
				      W2_2467_2467),
	 .chan11g		= BM7(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG2_2472_2472,
				      WG1_2417_2432,
				      WG1_2447_2457,
				      WG2_2467_2467),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= WOR1_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= DISALLOW_ADHOC_11A,
	 .chan11a		= BM5(W1_5260_5320,
				      W1_5180_5240,
				      W1_5170_5230,
				      W1_5745_5825,
				      W1_5500_5700),
	 .chan11b		= BM8(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2472_2472,
				      W1_2417_2432,
				      W1_2447_2457,
				      W1_2467_2467,
				      W1_2484_2484),
	 .chan11g		= BM7(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2472_2472,
				      WG1_2417_2432,
				      WG1_2447_2457,
				      WG1_2467_2467),
	 .chan11g_turbo		= BM1(T3_2437_2437)
	},

	{.regDmnEnum		= WOR2_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= DISALLOW_ADHOC_11A,
	 .chan11a		= BM5(W1_5260_5320,
				      W1_5180_5240,
				      W1_5170_5230,
				      W1_5745_5825,
				      W1_5500_5700),
	 .chan11a_turbo		= BM3(WT1_5210_5250,
				      WT1_5290_5290,
				      WT1_5760_5800),
	 .chan11b		= BM8(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2472_2472,
				      W1_2417_2432,
				      W1_2447_2457,
				      W1_2467_2467,
				      W1_2484_2484),
	 .chan11g		= BM7(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2472_2472,
				      WG1_2417_2432,
				      WG1_2447_2457,
				      WG1_2467_2467),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= WOR3_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= ADHOC_PER_11D,
	 .chan11a		= BM4(W1_5260_5320,
				      W1_5180_5240,
				      W1_5170_5230,
				      W1_5745_5825),
	 .chan11a_turbo		= BM3(WT1_5210_5250,
				      WT1_5290_5290,
				      WT1_5760_5800),
	 .chan11b		= BM7(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2472_2472,
				      W1_2417_2432,
				      W1_2447_2457,
				      W1_2467_2467),
	 .chan11g		= BM7(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2472_2472,
				      WG1_2417_2432,
				      WG1_2447_2457,
				      WG1_2467_2467),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= WOR4_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= DISALLOW_ADHOC_11A,
	 .chan11a		= BM4(W2_5260_5320,
				      W2_5180_5240,
				      F2_5745_5805,
				      W2_5825_5825),
	 .chan11a_turbo		= BM3(WT1_5210_5250,
				      WT1_5290_5290,
				      WT1_5760_5800),
	 .chan11b		= BM5(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2417_2432,
				      W1_2447_2457),
	 .chan11g		= BM5(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2417_2432,
				      WG1_2447_2457),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= WOR5_ETSIC,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= DISALLOW_ADHOC_11A,
	 .chan11a		= BM3(W1_5260_5320, W2_5180_5240, F6_5745_5825),
	 .chan11b		= BM7(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W2_2472_2472,
				      W1_2417_2432,
				      W1_2447_2457,
				      W2_2467_2467),
	 .chan11g		= BM7(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG2_2472_2472,
				      WG1_2417_2432,
				      WG1_2447_2457,
				      WG2_2467_2467),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= WOR9_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= DISALLOW_ADHOC_11A,
	 .chan11a		= BM4(W1_5260_5320,
				      W1_5180_5240,
				      W1_5745_5825,
				      W1_5500_5700),
	 .chan11a_turbo		= BM3(WT1_5210_5250,
				      WT1_5290_5290,
				      WT1_5760_5800),
	 .chan11b		= BM5(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2417_2432,
				      W1_2447_2457),
	 .chan11g		= BM5(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2417_2432,
				      WG1_2447_2457),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= WORA_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= DISALLOW_ADHOC_11A,
	 .chan11a		= BM4(W1_5260_5320,
				      W1_5180_5240,
				      W1_5745_5825,
				      W1_5500_5700),
	 .chan11b		= BM7(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2472_2472,
				      W1_2417_2432,
				      W1_2447_2457,
				      W1_2467_2467),
	 .chan11g		= BM7(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2472_2472,
				      WG1_2417_2432,
				      WG1_2447_2457,
				      WG1_2467_2467),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= WORB_WORLD,
	 .conformanceTestLimit	= NO_CTL,
	 .dfsMask		= DFS_FCC3 | DFS_ETSI,
	 .pscan			= PSCAN_WWR,
	 .flags			= DISALLOW_ADHOC_11A,
	 .chan11a		= BM4(W1_5260_5320,
				      W1_5180_5240,
				      W1_5745_5825,
				      W1_5500_5700),
	 .chan11b		= BM7(W1_2412_2412,
				      W1_2437_2442,
				      W1_2462_2462,
				      W1_2472_2472,
				      W1_2417_2432,
				      W1_2447_2457,
				      W1_2467_2467),
	 .chan11g		= BM7(WG1_2412_2412,
				      WG1_2437_2442,
				      WG1_2462_2462,
				      WG1_2472_2472,
				      WG1_2417_2432,
				      WG1_2447_2457,
				      WG1_2467_2467),
	 .chan11g_turbo		= BM1(T3_2437_2437)},

	{.regDmnEnum		= NULL1,
	 .conformanceTestLimit	= NO_CTL,
	}
};

struct cmode {
	u_int	mode;
	u_int	flags;
};

static const struct cmode modes[] = {
	{ HAL_MODE_TURBO,	IEEE80211_CHAN_ST },
	{ HAL_MODE_11A,		IEEE80211_CHAN_A },
	{ HAL_MODE_11B,		IEEE80211_CHAN_B },
	{ HAL_MODE_11G,		IEEE80211_CHAN_G },
	{ HAL_MODE_11G_TURBO,	IEEE80211_CHAN_108G },
	{ HAL_MODE_11A_TURBO,	IEEE80211_CHAN_108A },
	{ HAL_MODE_11A_QUARTER_RATE,
	  IEEE80211_CHAN_A | IEEE80211_CHAN_QUARTER },
	{ HAL_MODE_11A_HALF_RATE,
	  IEEE80211_CHAN_A | IEEE80211_CHAN_HALF },
	{ HAL_MODE_11G_QUARTER_RATE,
	  IEEE80211_CHAN_G | IEEE80211_CHAN_QUARTER },
	{ HAL_MODE_11G_HALF_RATE,
	  IEEE80211_CHAN_G | IEEE80211_CHAN_HALF },
	{ HAL_MODE_11NG_HT20,	IEEE80211_CHAN_G | IEEE80211_CHAN_HT20 },
	{ HAL_MODE_11NG_HT40PLUS,
	  IEEE80211_CHAN_G | IEEE80211_CHAN_HT40U },
	{ HAL_MODE_11NG_HT40MINUS,
	  IEEE80211_CHAN_G | IEEE80211_CHAN_HT40D },
	{ HAL_MODE_11NA_HT20,	IEEE80211_CHAN_A | IEEE80211_CHAN_HT20 },
	{ HAL_MODE_11NA_HT40PLUS,
	  IEEE80211_CHAN_A | IEEE80211_CHAN_HT40U },
	{ HAL_MODE_11NA_HT40MINUS,
	  IEEE80211_CHAN_A | IEEE80211_CHAN_HT40D },
};

static OS_INLINE uint16_t
getEepromRD(struct ath_hal *ah)
{
	return AH_PRIVATE(ah)->ah_currentRD &~ WORLDWIDE_ROAMING_FLAG;
}

/*
 * Test to see if the bitmask array is all zeros
 */
static HAL_BOOL
isChanBitMaskZero(const uint64_t *bitmask)
{
#if BMLEN > 2
#error	"add more cases"
#endif
#if BMLEN > 1
	if (bitmask[1] != 0)
		return AH_FALSE;
#endif
	return (bitmask[0] == 0);
}

/*
 * Return whether or not the regulatory domain/country in EEPROM
 * is acceptable.
 */
static HAL_BOOL
isEepromValid(struct ath_hal *ah)
{
	uint16_t rd = getEepromRD(ah);
	int i;

	if (rd & COUNTRY_ERD_FLAG) {
		uint16_t cc = rd &~ COUNTRY_ERD_FLAG;
		for (i = 0; i < N(allCountries); i++)
			if (allCountries[i].countryCode == cc)
				return AH_TRUE;
	} else {
		for (i = 0; i < N(regDomainPairs); i++)
			if (regDomainPairs[i].regDmnEnum == rd)
				return AH_TRUE;
	}
	HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
	    "%s: invalid regulatory domain/country code 0x%x\n", __func__, rd);
	return AH_FALSE;
}

/*
 * Find the pointer to the country element in the country table
 * corresponding to the country code
 */
static COUNTRY_CODE_TO_ENUM_RD*
findCountry(HAL_CTRY_CODE countryCode)
{
	int i;

	for (i = 0; i < N(allCountries); i++) {
		if (allCountries[i].countryCode == countryCode)
			return &allCountries[i];
	}
	return AH_NULL;
}

static REG_DOMAIN *
findRegDmn(int regDmn)
{
	int i;

	for (i = 0; i < N(regDomains); i++) {
		if (regDomains[i].regDmnEnum == regDmn)
			return &regDomains[i];
	}
	return AH_NULL;
}

static REG_DMN_PAIR_MAPPING *
findRegDmnPair(int regDmnPair)
{
	int i;

	if (regDmnPair != NO_ENUMRD) {
		for (i = 0; i < N(regDomainPairs); i++) {
			if (regDomainPairs[i].regDmnEnum == regDmnPair)
				return &regDomainPairs[i];
		}
	}
	return AH_NULL;
}

/*
 * Calculate a default country based on the EEPROM setting.
 */
static HAL_CTRY_CODE
getDefaultCountry(struct ath_hal *ah)
{
	REG_DMN_PAIR_MAPPING *regpair;
	uint16_t rd;

	rd = getEepromRD(ah);
	if (rd & COUNTRY_ERD_FLAG) {
		COUNTRY_CODE_TO_ENUM_RD *country;
		uint16_t cc = rd & ~COUNTRY_ERD_FLAG;
		country = findCountry(cc);
		if (country != AH_NULL)
			return cc;
	}
	/*
	 * Check reg domains that have only one country
	 */
	regpair = findRegDmnPair(rd);
	return (regpair != AH_NULL) ? regpair->singleCC : CTRY_DEFAULT;
}

static HAL_BOOL
IS_BIT_SET(int bit, const uint64_t bitmask[])
{
	int byteOffset, bitnum;
	uint64_t val;

	byteOffset = bit/64;
	bitnum = bit - byteOffset*64;
	val = ((uint64_t) 1) << bitnum;
	return (bitmask[byteOffset] & val) != 0;
}

static HAL_STATUS
getregstate(struct ath_hal *ah, HAL_CTRY_CODE cc, HAL_REG_DOMAIN regDmn,
    COUNTRY_CODE_TO_ENUM_RD **pcountry,
    REG_DOMAIN **prd2GHz, REG_DOMAIN **prd5GHz)
{
	COUNTRY_CODE_TO_ENUM_RD *country;
	REG_DOMAIN *rd5GHz, *rd2GHz;

	if (cc == CTRY_DEFAULT && regDmn == SKU_NONE) {
		/*
		 * Validate the EEPROM setting and setup defaults
		 */
		if (!isEepromValid(ah)) {
			/*
			 * Don't return any channels if the EEPROM has an
			 * invalid regulatory domain/country code setting.
			 */
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: invalid EEPROM contents\n",__func__);
			return HAL_EEBADREG;
		}

		cc = getDefaultCountry(ah);
		country = findCountry(cc);
		if (country == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "NULL Country!, cc %d\n", cc);
			return HAL_EEBADCC;
		}
		regDmn = country->regDmnEnum;
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: EEPROM cc %u rd 0x%x\n",
		    __func__, cc, regDmn);

		if (country->countryCode == CTRY_DEFAULT) {
			/*
			 * Check EEPROM; SKU may be for a country, single
			 * domain, or multiple domains (WWR).
			 */
			uint16_t rdnum = getEepromRD(ah);
			if ((rdnum & COUNTRY_ERD_FLAG) == 0 &&
			    (findRegDmn(rdnum) != AH_NULL ||
			     findRegDmnPair(rdnum) != AH_NULL)) {
				regDmn = rdnum;
				HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
				    "%s: EEPROM rd 0x%x\n", __func__, rdnum);
			}
		}
	} else {
		country = findCountry(cc);
		if (country == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "unknown country, cc %d\n", cc);
			return HAL_EINVAL;
		}
		if (regDmn == SKU_NONE)
			regDmn = country->regDmnEnum;
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: cc %u rd 0x%x\n",
		    __func__, cc, regDmn);
	}

	/*
	 * Setup per-band state.
	 */
	if ((regDmn & MULTI_DOMAIN_MASK) == 0) {
		REG_DMN_PAIR_MAPPING *regpair = findRegDmnPair(regDmn);
		if (regpair == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: no reg domain pair %u for country %u\n",
			    __func__, regDmn, country->countryCode);
			return HAL_EINVAL;
		}
		rd5GHz = findRegDmn(regpair->regDmn5GHz);
		if (rd5GHz == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: no 5GHz reg domain %u for country %u\n",
			    __func__, regpair->regDmn5GHz, country->countryCode);
			return HAL_EINVAL;
		}
		rd2GHz = findRegDmn(regpair->regDmn2GHz);
		if (rd2GHz == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: no 2GHz reg domain %u for country %u\n",
			    __func__, regpair->regDmn2GHz, country->countryCode);
			return HAL_EINVAL;
		}
	} else {
		rd5GHz = rd2GHz = findRegDmn(regDmn);
		if (rd2GHz == AH_NULL) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: no unitary reg domain %u for country %u\n",
			    __func__, regDmn, country->countryCode);
			return HAL_EINVAL;
		}
	}
	if (pcountry != AH_NULL)
		*pcountry = country;
	*prd2GHz = rd2GHz;
	*prd5GHz = rd5GHz;
	return HAL_OK;
}

/*
 * Construct the channel list for the specified regulatory config.
 */
static HAL_STATUS
getchannels(struct ath_hal *ah,
    struct ieee80211_channel chans[], u_int maxchans, int *nchans,
    u_int modeSelect, HAL_CTRY_CODE cc, HAL_REG_DOMAIN regDmn,
    HAL_BOOL enableExtendedChannels,
    COUNTRY_CODE_TO_ENUM_RD **pcountry,
    REG_DOMAIN **prd2GHz, REG_DOMAIN **prd5GHz)
{
#define CHANNEL_HALF_BW		10
#define CHANNEL_QUARTER_BW	5
#define	HAL_MODE_11A_ALL \
	(HAL_MODE_11A | HAL_MODE_11A_TURBO | HAL_MODE_TURBO | \
	 HAL_MODE_11A_QUARTER_RATE | HAL_MODE_11A_HALF_RATE)
	REG_DOMAIN *rd5GHz, *rd2GHz;
	u_int modesAvail;
	const struct cmode *cm;
	struct ieee80211_channel *ic;
	int next, b;
	HAL_STATUS status;

	HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: cc %u regDmn 0x%x mode 0x%x%s\n",
	    __func__, cc, regDmn, modeSelect, 
	    enableExtendedChannels ? " ecm" : "");

	status = getregstate(ah, cc, regDmn, pcountry, &rd2GHz, &rd5GHz);
	if (status != HAL_OK)
		return status;

	/* get modes that HW is capable of */
	modesAvail = ath_hal_getWirelessModes(ah);
	/* optimize work below if no 11a channels */
	if (isChanBitMaskZero(rd5GHz->chan11a) &&
	    (modesAvail & HAL_MODE_11A_ALL)) {
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
		    "%s: disallow all 11a\n", __func__);
		modesAvail &= ~HAL_MODE_11A_ALL;
	}

	next = 0;
	ic = &chans[0];
	for (cm = modes; cm < &modes[N(modes)]; cm++) {
		uint16_t c, c_hi, c_lo;
		uint64_t *channelBM = AH_NULL;
		REG_DMN_FREQ_BAND *fband = AH_NULL,*freqs;
		int low_adj, hi_adj, channelSep, lastc;
		uint32_t rdflags;
		uint64_t dfsMask;
		uint64_t pscan;

		if ((cm->mode & modeSelect) == 0) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: skip mode 0x%x flags 0x%x\n",
			    __func__, cm->mode, cm->flags);
			continue;
		}
		if ((cm->mode & modesAvail) == 0) {
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: !avail mode 0x%x (0x%x) flags 0x%x\n",
			    __func__, modesAvail, cm->mode, cm->flags);
			continue;
		}
		if (!ath_hal_getChannelEdges(ah, cm->flags, &c_lo, &c_hi)) {
			/* channel not supported by hardware, skip it */
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: channels 0x%x not supported by hardware\n",
			    __func__,cm->flags);
			continue;
		}
		switch (cm->mode) {
		case HAL_MODE_TURBO:
		case HAL_MODE_11A_TURBO:
			rdflags = rd5GHz->flags;
			dfsMask = rd5GHz->dfsMask;
			pscan = rd5GHz->pscan;
			if (cm->mode == HAL_MODE_TURBO)
				channelBM = rd5GHz->chan11a_turbo;
			else
				channelBM = rd5GHz->chan11a_dyn_turbo;
			freqs = &regDmn5GhzTurboFreq[0];
			break;
		case HAL_MODE_11G_TURBO:
			rdflags = rd2GHz->flags;
			dfsMask = rd2GHz->dfsMask;
			pscan = rd2GHz->pscan;
			channelBM = rd2GHz->chan11g_turbo;
			freqs = &regDmn2Ghz11gTurboFreq[0];
			break;
		case HAL_MODE_11A:
		case HAL_MODE_11A_HALF_RATE:
		case HAL_MODE_11A_QUARTER_RATE:
		case HAL_MODE_11NA_HT20:
		case HAL_MODE_11NA_HT40PLUS:
		case HAL_MODE_11NA_HT40MINUS:
			rdflags = rd5GHz->flags;
			dfsMask = rd5GHz->dfsMask;
			pscan = rd5GHz->pscan;
			if (cm->mode == HAL_MODE_11A_HALF_RATE)
				channelBM = rd5GHz->chan11a_half;
			else if (cm->mode == HAL_MODE_11A_QUARTER_RATE)
				channelBM = rd5GHz->chan11a_quarter;
			else
				channelBM = rd5GHz->chan11a;
			freqs = &regDmn5GhzFreq[0];
			break;
		case HAL_MODE_11B:
		case HAL_MODE_11G:
		case HAL_MODE_11G_HALF_RATE:
		case HAL_MODE_11G_QUARTER_RATE:
		case HAL_MODE_11NG_HT20:
		case HAL_MODE_11NG_HT40PLUS:
		case HAL_MODE_11NG_HT40MINUS:
			rdflags = rd2GHz->flags;
			dfsMask = rd2GHz->dfsMask;
			pscan = rd2GHz->pscan;
			if (cm->mode == HAL_MODE_11G_HALF_RATE)
				channelBM = rd2GHz->chan11g_half;
			else if (cm->mode == HAL_MODE_11G_QUARTER_RATE)
				channelBM = rd2GHz->chan11g_quarter;
			else if (cm->mode == HAL_MODE_11B)
				channelBM = rd2GHz->chan11b;
			else
				channelBM = rd2GHz->chan11g;
			if (cm->mode == HAL_MODE_11B)
				freqs = &regDmn2GhzFreq[0];
			else
				freqs = &regDmn2Ghz11gFreq[0];
			break;
		default:
			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: Unkonwn HAL mode 0x%x\n", __func__, cm->mode);
			continue;
		}
		if (isChanBitMaskZero(channelBM))
			continue;
		/*
		 * Setup special handling for HT40 channels; e.g.
		 * 5G HT40 channels require 40Mhz channel separation.
		 */
		hi_adj = (cm->mode == HAL_MODE_11NA_HT40PLUS ||
		    cm->mode == HAL_MODE_11NG_HT40PLUS) ? -20 : 0;
		low_adj = (cm->mode == HAL_MODE_11NA_HT40MINUS || 
		    cm->mode == HAL_MODE_11NG_HT40MINUS) ? 20 : 0;
		channelSep = (cm->mode == HAL_MODE_11NA_HT40PLUS ||
		    cm->mode == HAL_MODE_11NA_HT40MINUS) ? 40 : 0;

		for (b = 0; b < 64*BMLEN; b++) {
			if (!IS_BIT_SET(b, channelBM))
				continue;
			fband = &freqs[b];
			lastc = 0;

			for (c = fband->lowChannel + low_adj;
			     c <= fband->highChannel + hi_adj;
			     c += fband->channelSep) {
				if (!(c_lo <= c && c <= c_hi)) {
					HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
					    "%s: c %u out of range [%u..%u]\n",
					    __func__, c, c_lo, c_hi);
					continue;
				}
				if (next >= maxchans){
					HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
					    "%s: too many channels for channel table\n",
					    __func__);
					goto done;
				}
				if ((fband->usePassScan & IS_ECM_CHAN) &&
				    !enableExtendedChannels) {
					HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
					    "skip ecm channel\n");
					continue;
				}
				if ((fband->useDfs & dfsMask) && 
				    (cm->flags & IEEE80211_CHAN_HT40)) {
					/* NB: DFS and HT40 don't mix */
					HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
					    "skip HT40 chan, DFS required\n");
					continue;
				}
				/*
				 * Make sure that channel separation
				 * meets the requirement.
				 */
				if (lastc && channelSep &&
				    (c-lastc) < channelSep)
					continue;
				lastc = c;

				OS_MEMZERO(ic, sizeof(*ic));
				ic->ic_freq = c;
				ic->ic_flags = cm->flags;
				ic->ic_maxregpower = fband->powerDfs;
				ath_hal_getpowerlimits(ah, ic);
				ic->ic_maxantgain = fband->antennaMax;
				if (fband->usePassScan & pscan)
					ic->ic_flags |= IEEE80211_CHAN_PASSIVE;
				if (fband->useDfs & dfsMask)
					ic->ic_flags |= IEEE80211_CHAN_DFS;
				if (IEEE80211_IS_CHAN_5GHZ(ic) &&
				    (rdflags & DISALLOW_ADHOC_11A))
					ic->ic_flags |= IEEE80211_CHAN_NOADHOC;
				if (IEEE80211_IS_CHAN_TURBO(ic) &&
				    (rdflags & DISALLOW_ADHOC_11A_TURB))
					ic->ic_flags |= IEEE80211_CHAN_NOADHOC;
				if (rdflags & NO_HOSTAP)
					ic->ic_flags |= IEEE80211_CHAN_NOHOSTAP;
				if (rdflags & LIMIT_FRAME_4MS)
					ic->ic_flags |= IEEE80211_CHAN_4MSXMIT;
				if (rdflags & NEED_NFC)
					ic->ic_flags |= CHANNEL_NFCREQUIRED;

				ic++, next++;
			}
		}
	}
done:
	*nchans = next;
	/* NB: pcountry set above by getregstate */
	if (prd2GHz != AH_NULL)
		*prd2GHz = rd2GHz;
	if (prd5GHz != AH_NULL)
		*prd5GHz = rd5GHz;
	return HAL_OK;
#undef HAL_MODE_11A_ALL
#undef CHANNEL_HALF_BW
#undef CHANNEL_QUARTER_BW
}

/*
 * Retrieve a channel list without affecting runtime state.
 */
HAL_STATUS
ath_hal_getchannels(struct ath_hal *ah,
    struct ieee80211_channel chans[], u_int maxchans, int *nchans,
    u_int modeSelect, HAL_CTRY_CODE cc, HAL_REG_DOMAIN regDmn,
    HAL_BOOL enableExtendedChannels)
{
	return getchannels(ah, chans, maxchans, nchans, modeSelect,
	    cc, regDmn, enableExtendedChannels, AH_NULL, AH_NULL, AH_NULL);
}

/*
 * Handle frequency mapping from 900Mhz range to 2.4GHz range
 * for GSM radios.  This is done when we need the h/w frequency
 * and the channel is marked IEEE80211_CHAN_GSM.
 */
static int
ath_hal_mapgsm(int sku, int freq)
{
	if (sku == SKU_XR9)
		return 1520 + freq;
	if (sku == SKU_GZ901)
		return 1544 + freq;
	if (sku == SKU_SR9)
		return 3344 - freq;
	HALDEBUG(AH_NULL, HAL_DEBUG_ANY,
	    "%s: cannot map freq %u unknown gsm sku %u\n",
	    __func__, freq, sku);
	return freq;
}

/*
 * Setup the internal/private channel state given a table of
 * net80211 channels.  We collapse entries for the same frequency
 * and record the frequency for doing noise floor processing
 * where we don't have net80211 channel context.
 */
static HAL_BOOL
assignPrivateChannels(struct ath_hal *ah,
	struct ieee80211_channel chans[], int nchans, int sku)
{
	HAL_CHANNEL_INTERNAL *ic;
	int i, j, next, freq;

	next = 0;
	for (i = 0; i < nchans; i++) {
		struct ieee80211_channel *c = &chans[i];
		for (j = i-1; j >= 0; j--)
			if (chans[j].ic_freq == c->ic_freq) {
				c->ic_devdata = chans[j].ic_devdata;
				break;
			}
		if (j < 0) {
			/* new entry, assign a private channel entry */
			if (next >= N(AH_PRIVATE(ah)->ah_channels)) {
				HALDEBUG(ah, HAL_DEBUG_ANY,
				    "%s: too many channels, max %zu\n",
				    __func__, N(AH_PRIVATE(ah)->ah_channels));
				return AH_FALSE;
			}
			/*
			 * Handle frequency mapping for 900MHz devices.
			 * The hardware uses 2.4GHz frequencies that are
			 * down-converted.  The 802.11 layer uses the
			 * true frequencies.
			 */
			freq = IEEE80211_IS_CHAN_GSM(c) ?
			    ath_hal_mapgsm(sku, c->ic_freq) : c->ic_freq;

			HALDEBUG(ah, HAL_DEBUG_REGDOMAIN,
			    "%s: private[%3u] %u/0x%x -> channel %u\n",
			    __func__, next, c->ic_freq, c->ic_flags, freq);

			ic = &AH_PRIVATE(ah)->ah_channels[next];
			/*
			 * NB: This clears privFlags which means ancillary
			 *     code like ANI and IQ calibration will be
			 *     restarted and re-setup any per-channel state.
			 */
			OS_MEMZERO(ic, sizeof(*ic));
			ic->channel = freq;
			c->ic_devdata = next;
			next++;
		}
	}
	AH_PRIVATE(ah)->ah_nchan = next;
	HALDEBUG(ah, HAL_DEBUG_ANY, "%s: %u public, %u private channels\n",
	    __func__, nchans, next);
	return AH_TRUE;
}

/*
 * Setup the channel list based on the information in the EEPROM.
 */
HAL_STATUS
ath_hal_init_channels(struct ath_hal *ah,
    struct ieee80211_channel chans[], u_int maxchans, int *nchans,
    u_int modeSelect, HAL_CTRY_CODE cc, HAL_REG_DOMAIN regDmn,
    HAL_BOOL enableExtendedChannels)
{
	COUNTRY_CODE_TO_ENUM_RD *country;
	REG_DOMAIN *rd5GHz, *rd2GHz;
	HAL_STATUS status;

	status = getchannels(ah, chans, maxchans, nchans, modeSelect,
	    cc, regDmn, enableExtendedChannels, &country, &rd2GHz, &rd5GHz);
	if (status == HAL_OK &&
	    assignPrivateChannels(ah, chans, *nchans, AH_PRIVATE(ah)->ah_currentRD)) {
		AH_PRIVATE(ah)->ah_rd2GHz = rd2GHz;
		AH_PRIVATE(ah)->ah_rd5GHz = rd5GHz;

		ah->ah_countryCode = country->countryCode;
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: cc %u\n",
		    __func__, ah->ah_countryCode);
	} else
		status = HAL_EINVAL;
	return status;
}

/*
 * Set the channel list.
 */
HAL_STATUS
ath_hal_set_channels(struct ath_hal *ah,
    struct ieee80211_channel chans[], int nchans,
    HAL_CTRY_CODE cc, HAL_REG_DOMAIN rd)
{
	COUNTRY_CODE_TO_ENUM_RD *country;
	REG_DOMAIN *rd5GHz, *rd2GHz;
	HAL_STATUS status;

	switch (rd) {
	case SKU_SR9:
	case SKU_XR9:
	case SKU_GZ901:
		/*
		 * Map 900MHz sku's.  The frequencies will be mapped
		 * according to the sku to compensate for the down-converter.
		 * We use the FCC for these sku's as the mapped channel
		 * list is known compatible (will need to change if/when
		 * vendors do different mapping in different locales).
		 */
		status = getregstate(ah, CTRY_DEFAULT, SKU_FCC,
		    &country, &rd2GHz, &rd5GHz);
		break;
	default:
		status = getregstate(ah, cc, rd,
		    &country, &rd2GHz, &rd5GHz);
		rd = AH_PRIVATE(ah)->ah_currentRD;
		break;
	}
	if (status == HAL_OK && assignPrivateChannels(ah, chans, nchans, rd)) {
		AH_PRIVATE(ah)->ah_rd2GHz = rd2GHz;
		AH_PRIVATE(ah)->ah_rd5GHz = rd5GHz;

		ah->ah_countryCode = country->countryCode;
		HALDEBUG(ah, HAL_DEBUG_REGDOMAIN, "%s: cc %u\n",
		    __func__, ah->ah_countryCode);
	} else
		status = HAL_EINVAL;
	return status;
}

#ifdef AH_DEBUG
/*
 * Return the internal channel corresponding to a public channel.
 * NB: normally this routine is inline'd (see ah_internal.h)
 */
HAL_CHANNEL_INTERNAL *
ath_hal_checkchannel(struct ath_hal *ah, const struct ieee80211_channel *c)
{
	HAL_CHANNEL_INTERNAL *cc = &AH_PRIVATE(ah)->ah_channels[c->ic_devdata];

	if (c->ic_devdata < AH_PRIVATE(ah)->ah_nchan &&
	    (c->ic_freq == cc->channel || IEEE80211_IS_CHAN_GSM(c)))
		return cc;
	if (c->ic_devdata >= AH_PRIVATE(ah)->ah_nchan) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: bad mapping, devdata %u nchans %u\n",
		   __func__, c->ic_devdata, AH_PRIVATE(ah)->ah_nchan);
		HALASSERT(c->ic_devdata < AH_PRIVATE(ah)->ah_nchan);
	} else {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: no match for %u/0x%x devdata %u channel %u\n",
		   __func__, c->ic_freq, c->ic_flags, c->ic_devdata,
		   cc->channel);
		HALASSERT(c->ic_freq == cc->channel || IEEE80211_IS_CHAN_GSM(c));
	}
	return AH_NULL;
}
#endif /* AH_DEBUG */

#define isWwrSKU(_ah) \
	((getEepromRD((_ah)) & WORLD_SKU_MASK) == WORLD_SKU_PREFIX || \
	  getEepromRD(_ah) == WORLD)

/*
 * Return the test group for the specific channel based on
 * the current regulatory setup.
 */
u_int
ath_hal_getctl(struct ath_hal *ah, const struct ieee80211_channel *c)
{
	u_int ctl;

	if (AH_PRIVATE(ah)->ah_rd2GHz == AH_PRIVATE(ah)->ah_rd5GHz ||
	    (ah->ah_countryCode == CTRY_DEFAULT && isWwrSKU(ah)))
		ctl = SD_NO_CTL;
	else if (IEEE80211_IS_CHAN_2GHZ(c))
		ctl = AH_PRIVATE(ah)->ah_rd2GHz->conformanceTestLimit;
	else
		ctl = AH_PRIVATE(ah)->ah_rd5GHz->conformanceTestLimit;
	if (IEEE80211_IS_CHAN_B(c))
		return ctl | CTL_11B;
	if (IEEE80211_IS_CHAN_G(c))
		return ctl | CTL_11G;
	if (IEEE80211_IS_CHAN_108G(c))
		return ctl | CTL_108G;
	if (IEEE80211_IS_CHAN_TURBO(c))
		return ctl | CTL_TURBO;
	if (IEEE80211_IS_CHAN_A(c))
		return ctl | CTL_11A;
	return ctl;
}

/*
 * Return the max allowed antenna gain and apply any regulatory
 * domain specific changes.
 *
 * NOTE: a negative reduction is possible in RD's that only
 * measure radiated power (e.g., ETSI) which would increase
 * that actual conducted output power (though never beyond
 * the calibrated target power).
 */
u_int
ath_hal_getantennareduction(struct ath_hal *ah,
    const struct ieee80211_channel *chan, u_int twiceGain)
{
	int8_t antennaMax = twiceGain - chan->ic_maxantgain*2;
	return (antennaMax < 0) ? 0 : antennaMax;
}
