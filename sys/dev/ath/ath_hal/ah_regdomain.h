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
#ifndef	__AH_REGDOMAIN_H__
#define	__AH_REGDOMAIN_H__

/* 
 * BMLEN defines the size of the bitmask used to hold frequency
 * band specifications.  Note this must agree with the BM macro
 * definition that's used to setup initializers.  See also further
 * comments below.
 */
#define BMLEN 2		/* 2 x 64 bits in each channel bitmask */
typedef uint64_t chanbmask_t[BMLEN];

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

typedef struct {
	HAL_CTRY_CODE		countryCode;	   
	HAL_REG_DOMAIN		regDmnEnum;
} COUNTRY_CODE_TO_ENUM_RD;

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

struct cmode {
	u_int	mode;
	u_int	flags;
};
#endif
