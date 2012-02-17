/*
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2008 Atheros Communications, Inc.
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
#include "ah_internal.h"
#include "ah_devid.h"

/* linker set of registered chips */
OS_SET_DECLARE(ah_chips, struct ath_hal_chip);

/*
 * Check the set of registered chips to see if any recognize
 * the device as one they can support.
 */
const char*
ath_hal_probe(uint16_t vendorid, uint16_t devid)
{
	struct ath_hal_chip * const *pchip;

	OS_SET_FOREACH(pchip, ah_chips) {
		const char *name = (*pchip)->probe(vendorid, devid);
		if (name != AH_NULL)
			return name;
	}
	return AH_NULL;
}

/*
 * Attach detects device chip revisions, initializes the hwLayer
 * function list, reads EEPROM information,
 * selects reset vectors, and performs a short self test.
 * Any failures will return an error that should cause a hardware
 * disable.
 */
struct ath_hal*
ath_hal_attach(uint16_t devid, HAL_SOFTC sc,
	HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_STATUS *error)
{
	struct ath_hal_chip * const *pchip;

	OS_SET_FOREACH(pchip, ah_chips) {
		struct ath_hal_chip *chip = *pchip;
		struct ath_hal *ah;

		/* XXX don't have vendorid, assume atheros one works */
		if (chip->probe(ATHEROS_VENDOR_ID, devid) == AH_NULL)
			continue;
		ah = chip->attach(devid, sc, st, sh, error);
		if (ah != AH_NULL) {
			/* copy back private state to public area */
			ah->ah_devid = AH_PRIVATE(ah)->ah_devid;
			ah->ah_subvendorid = AH_PRIVATE(ah)->ah_subvendorid;
			ah->ah_macVersion = AH_PRIVATE(ah)->ah_macVersion;
			ah->ah_macRev = AH_PRIVATE(ah)->ah_macRev;
			ah->ah_phyRev = AH_PRIVATE(ah)->ah_phyRev;
			ah->ah_analog5GhzRev = AH_PRIVATE(ah)->ah_analog5GhzRev;
			ah->ah_analog2GhzRev = AH_PRIVATE(ah)->ah_analog2GhzRev;
			return ah;
		}
	}
	return AH_NULL;
}

/* linker set of registered RF backends */
OS_SET_DECLARE(ah_rfs, struct ath_hal_rf);

/*
 * Check the set of registered RF backends to see if
 * any recognize the device as one they can support.
 */
struct ath_hal_rf *
ath_hal_rfprobe(struct ath_hal *ah, HAL_STATUS *ecode)
{
	struct ath_hal_rf * const *prf;

	OS_SET_FOREACH(prf, ah_rfs) {
		struct ath_hal_rf *rf = *prf;
		if (rf->probe(ah))
			return rf;
	}
	*ecode = HAL_ENOTSUPP;
	return AH_NULL;
}

/*
 * Poll the register looking for a specific value.
 */
HAL_BOOL
ath_hal_wait(struct ath_hal *ah, u_int reg, uint32_t mask, uint32_t val)
{
#define	AH_TIMEOUT	1000
	int i;

	for (i = 0; i < AH_TIMEOUT; i++) {
		if ((OS_REG_READ(ah, reg) & mask) == val)
			return AH_TRUE;
		OS_DELAY(10);
	}
	HALDEBUG(ah, HAL_DEBUG_REGIO | HAL_DEBUG_PHYIO,
	    "%s: timeout on reg 0x%x: 0x%08x & 0x%08x != 0x%08x\n",
	    __func__, reg, OS_REG_READ(ah, reg), mask, val);
	return AH_FALSE;
#undef AH_TIMEOUT
}

/*
 * Reverse the bits starting at the low bit for a value of
 * bit_count in size
 */
uint32_t
ath_hal_reverseBits(uint32_t val, uint32_t n)
{
	uint32_t retval;
	int i;

	for (i = 0, retval = 0; i < n; i++) {
		retval = (retval << 1) | (val & 1);
		val >>= 1;
	}
	return retval;
}

/*
 * Compute the time to transmit a frame of length frameLen bytes
 * using the specified rate, phy, and short preamble setting.
 */
uint16_t
ath_hal_computetxtime(struct ath_hal *ah,
	const HAL_RATE_TABLE *rates, uint32_t frameLen, uint16_t rateix,
	HAL_BOOL shortPreamble)
{
	uint32_t bitsPerSymbol, numBits, numSymbols, phyTime, txTime;
	uint32_t kbps;

	kbps = rates->info[rateix].rateKbps;
	/*
	 * index can be invalid duting dynamic Turbo transitions. 
	 */
	if(kbps == 0) return 0;
	switch (rates->info[rateix].phy) {

	case IEEE80211_T_CCK:
#define CCK_SIFS_TIME        10
#define CCK_PREAMBLE_BITS   144
#define CCK_PLCP_BITS        48
		phyTime		= CCK_PREAMBLE_BITS + CCK_PLCP_BITS;
		if (shortPreamble && rates->info[rateix].shortPreamble)
			phyTime >>= 1;
		numBits		= frameLen << 3;
		txTime		= CCK_SIFS_TIME + phyTime
				+ ((numBits * 1000)/kbps);
		break;
#undef CCK_SIFS_TIME
#undef CCK_PREAMBLE_BITS
#undef CCK_PLCP_BITS

	case IEEE80211_T_OFDM:
#define OFDM_SIFS_TIME        16
#define OFDM_PREAMBLE_TIME    20
#define OFDM_PLCP_BITS        22
#define OFDM_SYMBOL_TIME       4

#define OFDM_SIFS_TIME_HALF	32
#define OFDM_PREAMBLE_TIME_HALF	40
#define OFDM_PLCP_BITS_HALF	22
#define OFDM_SYMBOL_TIME_HALF	8

#define OFDM_SIFS_TIME_QUARTER 		64
#define OFDM_PREAMBLE_TIME_QUARTER	80
#define OFDM_PLCP_BITS_QUARTER		22
#define OFDM_SYMBOL_TIME_QUARTER	16

		if (AH_PRIVATE(ah)->ah_curchan && 
			IS_CHAN_QUARTER_RATE(AH_PRIVATE(ah)->ah_curchan)) {
			bitsPerSymbol	= (kbps * OFDM_SYMBOL_TIME_QUARTER) / 1000;
			HALASSERT(bitsPerSymbol != 0);

			numBits		= OFDM_PLCP_BITS + (frameLen << 3);
			numSymbols	= howmany(numBits, bitsPerSymbol);
			txTime		= OFDM_SIFS_TIME_QUARTER 
						+ OFDM_PREAMBLE_TIME_QUARTER
					+ (numSymbols * OFDM_SYMBOL_TIME_QUARTER);
		} else if (AH_PRIVATE(ah)->ah_curchan &&
				IS_CHAN_HALF_RATE(AH_PRIVATE(ah)->ah_curchan)) {
			bitsPerSymbol	= (kbps * OFDM_SYMBOL_TIME_HALF) / 1000;
			HALASSERT(bitsPerSymbol != 0);

			numBits		= OFDM_PLCP_BITS + (frameLen << 3);
			numSymbols	= howmany(numBits, bitsPerSymbol);
			txTime		= OFDM_SIFS_TIME_HALF + 
						OFDM_PREAMBLE_TIME_HALF
					+ (numSymbols * OFDM_SYMBOL_TIME_HALF);
		} else { /* full rate channel */
			bitsPerSymbol	= (kbps * OFDM_SYMBOL_TIME) / 1000;
			HALASSERT(bitsPerSymbol != 0);

			numBits		= OFDM_PLCP_BITS + (frameLen << 3);
			numSymbols	= howmany(numBits, bitsPerSymbol);
			txTime		= OFDM_SIFS_TIME + OFDM_PREAMBLE_TIME
					+ (numSymbols * OFDM_SYMBOL_TIME);
		}
		break;

#undef OFDM_SIFS_TIME
#undef OFDM_PREAMBLE_TIME
#undef OFDM_PLCP_BITS
#undef OFDM_SYMBOL_TIME

	case IEEE80211_T_TURBO:
#define TURBO_SIFS_TIME         8
#define TURBO_PREAMBLE_TIME    14
#define TURBO_PLCP_BITS        22
#define TURBO_SYMBOL_TIME       4
		/* we still save OFDM rates in kbps - so double them */
		bitsPerSymbol = ((kbps << 1) * TURBO_SYMBOL_TIME) / 1000;
		HALASSERT(bitsPerSymbol != 0);

		numBits       = TURBO_PLCP_BITS + (frameLen << 3);
		numSymbols    = howmany(numBits, bitsPerSymbol);
		txTime        = TURBO_SIFS_TIME + TURBO_PREAMBLE_TIME
			      + (numSymbols * TURBO_SYMBOL_TIME);
		break;
#undef TURBO_SIFS_TIME
#undef TURBO_PREAMBLE_TIME
#undef TURBO_PLCP_BITS
#undef TURBO_SYMBOL_TIME

	default:
		HALDEBUG(ah, HAL_DEBUG_PHYIO,
		    "%s: unknown phy %u (rate ix %u)\n",
		    __func__, rates->info[rateix].phy, rateix);
		txTime = 0;
		break;
	}
	return txTime;
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

typedef enum {
	WIRELESS_MODE_11a   = 0,
	WIRELESS_MODE_TURBO = 1,
	WIRELESS_MODE_11b   = 2,
	WIRELESS_MODE_11g   = 3,
	WIRELESS_MODE_108g  = 4,

	WIRELESS_MODE_MAX
} WIRELESS_MODE;

static WIRELESS_MODE
ath_hal_chan2wmode(struct ath_hal *ah, const HAL_CHANNEL *chan)
{
	if (IS_CHAN_CCK(chan))
		return WIRELESS_MODE_11b;
	if (IS_CHAN_G(chan))
		return WIRELESS_MODE_11g;
	if (IS_CHAN_108G(chan))
		return WIRELESS_MODE_108g;
	if (IS_CHAN_TURBO(chan))
		return WIRELESS_MODE_TURBO;
	return WIRELESS_MODE_11a;
}

/*
 * Convert between microseconds and core system clocks.
 */
                                     /* 11a Turbo  11b  11g  108g */
static const uint8_t CLOCK_RATE[]  = { 40,  80,   22,  44,   88  };

u_int
ath_hal_mac_clks(struct ath_hal *ah, u_int usecs)
{
	const HAL_CHANNEL *c = (const HAL_CHANNEL *) AH_PRIVATE(ah)->ah_curchan;
	u_int clks;

	/* NB: ah_curchan may be null when called attach time */
	if (c != AH_NULL) {
		clks = usecs * CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
		if (IS_CHAN_HT40(c))
			clks <<= 1;
		else if (IS_CHAN_HALF_RATE(c))
			clks >>= 1;
		else if (IS_CHAN_QUARTER_RATE(c))
			clks >>= 2;
	} else
		clks = usecs * CLOCK_RATE[WIRELESS_MODE_11b];
	return clks;
}

u_int
ath_hal_mac_usec(struct ath_hal *ah, u_int clks)
{
	const HAL_CHANNEL *c = (const HAL_CHANNEL *) AH_PRIVATE(ah)->ah_curchan;
	u_int usec;

	/* NB: ah_curchan may be null when called attach time */
	if (c != AH_NULL) {
		usec = clks / CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
		if (IS_CHAN_HT40(c))
			usec >>= 1;
		else if (IS_CHAN_HALF_RATE(c))
			usec <<= 1;
		else if (IS_CHAN_QUARTER_RATE(c))
			usec <<= 2;
	} else
		usec = clks / CLOCK_RATE[WIRELESS_MODE_11b];
	return usec;
}

/*
 * Setup a h/w rate table's reverse lookup table and
 * fill in ack durations.  This routine is called for
 * each rate table returned through the ah_getRateTable
 * method.  The reverse lookup tables are assumed to be
 * initialized to zero (or at least the first entry).
 * We use this as a key that indicates whether or not
 * we've previously setup the reverse lookup table.
 *
 * XXX not reentrant, but shouldn't matter
 */
void
ath_hal_setupratetable(struct ath_hal *ah, HAL_RATE_TABLE *rt)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	int i;

	if (rt->rateCodeToIndex[0] != 0)	/* already setup */
		return;
	for (i = 0; i < N(rt->rateCodeToIndex); i++)
		rt->rateCodeToIndex[i] = (uint8_t) -1;
	for (i = 0; i < rt->rateCount; i++) {
		uint8_t code = rt->info[i].rateCode;
		uint8_t cix = rt->info[i].controlRate;

		HALASSERT(code < N(rt->rateCodeToIndex));
		rt->rateCodeToIndex[code] = i;
		HALASSERT((code | rt->info[i].shortPreamble) <
		    N(rt->rateCodeToIndex));
		rt->rateCodeToIndex[code | rt->info[i].shortPreamble] = i;
		/*
		 * XXX for 11g the control rate to use for 5.5 and 11 Mb/s
		 *     depends on whether they are marked as basic rates;
		 *     the static tables are setup with an 11b-compatible
		 *     2Mb/s rate which will work but is suboptimal
		 */
		rt->info[i].lpAckDuration = ath_hal_computetxtime(ah, rt,
			WLAN_CTRL_FRAME_SIZE, cix, AH_FALSE);
		rt->info[i].spAckDuration = ath_hal_computetxtime(ah, rt,
			WLAN_CTRL_FRAME_SIZE, cix, AH_TRUE);
	}
#undef N
}

HAL_STATUS
ath_hal_getcapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
	uint32_t capability, uint32_t *result)
{
	const HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

	switch (type) {
	case HAL_CAP_REG_DMN:		/* regulatory domain */
		*result = AH_PRIVATE(ah)->ah_currentRD;
		return HAL_OK;
	case HAL_CAP_CIPHER:		/* cipher handled in hardware */
	case HAL_CAP_TKIP_MIC:		/* handle TKIP MIC in hardware */
		return HAL_ENOTSUPP;
	case HAL_CAP_TKIP_SPLIT:	/* hardware TKIP uses split keys */
		return HAL_ENOTSUPP;
	case HAL_CAP_PHYCOUNTERS:	/* hardware PHY error counters */
		return pCap->halHwPhyCounterSupport ? HAL_OK : HAL_ENXIO;
	case HAL_CAP_WME_TKIPMIC:   /* hardware can do TKIP MIC when WMM is turned on */
		return HAL_ENOTSUPP;
	case HAL_CAP_DIVERSITY:		/* hardware supports fast diversity */
		return HAL_ENOTSUPP;
	case HAL_CAP_KEYCACHE_SIZE:	/* hardware key cache size */
		*result =  pCap->halKeyCacheSize;
		return HAL_OK;
	case HAL_CAP_NUM_TXQUEUES:	/* number of hardware tx queues */
		*result = pCap->halTotalQueues;
		return HAL_OK;
	case HAL_CAP_VEOL:		/* hardware supports virtual EOL */
		return pCap->halVEOLSupport ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_PSPOLL:		/* hardware PS-Poll support works */
		return pCap->halPSPollBroken ? HAL_ENOTSUPP : HAL_OK;
	case HAL_CAP_COMPRESSION:
		return pCap->halCompressSupport ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_BURST:
		return pCap->halBurstSupport ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_FASTFRAME:
		return pCap->halFastFramesSupport ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_DIAG:		/* hardware diagnostic support */
		*result = AH_PRIVATE(ah)->ah_diagreg;
		return HAL_OK;
	case HAL_CAP_TXPOW:		/* global tx power limit  */
		switch (capability) {
		case 0:			/* facility is supported */
			return HAL_OK;
		case 1:			/* current limit */
			*result = AH_PRIVATE(ah)->ah_powerLimit;
			return HAL_OK;
		case 2:			/* current max tx power */
			*result = AH_PRIVATE(ah)->ah_maxPowerLevel;
			return HAL_OK;
		case 3:			/* scale factor */
			*result = AH_PRIVATE(ah)->ah_tpScale;
			return HAL_OK;
		}
		return HAL_ENOTSUPP;
	case HAL_CAP_BSSIDMASK:		/* hardware supports bssid mask */
		return pCap->halBssIdMaskSupport ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_MCAST_KEYSRCH:	/* multicast frame keycache search */
		return pCap->halMcastKeySrchSupport ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_TSF_ADJUST:	/* hardware has beacon tsf adjust */
		return HAL_ENOTSUPP;
	case HAL_CAP_RFSILENT:		/* rfsilent support  */
		switch (capability) {
		case 0:			/* facility is supported */
			return pCap->halRfSilentSupport ? HAL_OK : HAL_ENOTSUPP;
		case 1:			/* current setting */
			return AH_PRIVATE(ah)->ah_rfkillEnabled ?
				HAL_OK : HAL_ENOTSUPP;
		case 2:			/* rfsilent config */
			*result = AH_PRIVATE(ah)->ah_rfsilent;
			return HAL_OK;
		}
		return HAL_ENOTSUPP;
	case HAL_CAP_11D:
#ifdef AH_SUPPORT_11D
		return HAL_OK;
#else
		return HAL_ENOTSUPP;
#endif
	case HAL_CAP_RXORN_FATAL:	/* HAL_INT_RXORN treated as fatal  */
		return AH_PRIVATE(ah)->ah_rxornIsFatal ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_HT:
		return pCap->halHTSupport ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_TX_CHAINMASK:	/* mask of TX chains supported */
		*result = pCap->halTxChainMask;
		return HAL_OK;
	case HAL_CAP_RX_CHAINMASK:	/* mask of RX chains supported */
		*result = pCap->halRxChainMask;
		return HAL_OK;
	case HAL_CAP_RXTSTAMP_PREC:	/* rx desc tstamp precision (bits) */
		*result = pCap->halTstampPrecision;
		return HAL_OK;
	default:
		return HAL_EINVAL;
	}
}

HAL_BOOL
ath_hal_setcapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
	uint32_t capability, uint32_t setting, HAL_STATUS *status)
{

	switch (type) {
	case HAL_CAP_TXPOW:
		switch (capability) {
		case 3:
			if (setting <= HAL_TP_SCALE_MIN) {
				AH_PRIVATE(ah)->ah_tpScale = setting;
				return AH_TRUE;
			}
			break;
		}
		break;
	case HAL_CAP_RFSILENT:		/* rfsilent support  */
		/*
		 * NB: allow even if halRfSilentSupport is false
		 *     in case the EEPROM is misprogrammed.
		 */
		switch (capability) {
		case 1:			/* current setting */
			AH_PRIVATE(ah)->ah_rfkillEnabled = (setting != 0);
			return AH_TRUE;
		case 2:			/* rfsilent config */
			/* XXX better done per-chip for validation? */
			AH_PRIVATE(ah)->ah_rfsilent = setting;
			return AH_TRUE;
		}
		break;
	case HAL_CAP_REG_DMN:		/* regulatory domain */
		AH_PRIVATE(ah)->ah_currentRD = setting;
		return AH_TRUE;
	case HAL_CAP_RXORN_FATAL:	/* HAL_INT_RXORN treated as fatal  */
		AH_PRIVATE(ah)->ah_rxornIsFatal = setting;
		return AH_TRUE;
	default:
		break;
	}
	if (status)
		*status = HAL_EINVAL;
	return AH_FALSE;
}

/* 
 * Common support for getDiagState method.
 */

static u_int
ath_hal_getregdump(struct ath_hal *ah, const HAL_REGRANGE *regs,
	void *dstbuf, int space)
{
	uint32_t *dp = dstbuf;
	int i;

	for (i = 0; space >= 2*sizeof(uint32_t); i++) {
		u_int r = regs[i].start;
		u_int e = regs[i].end;
		*dp++ = (r<<16) | e;
		space -= sizeof(uint32_t);
		do {
			*dp++ = OS_REG_READ(ah, r);
			r += sizeof(uint32_t);
			space -= sizeof(uint32_t);
		} while (r <= e && space >= sizeof(uint32_t));
	}
	return (char *) dp - (char *) dstbuf;
}

HAL_BOOL
ath_hal_getdiagstate(struct ath_hal *ah, int request,
	const void *args, uint32_t argsize,
	void **result, uint32_t *resultsize)
{
	switch (request) {
	case HAL_DIAG_REVS:
		*result = &AH_PRIVATE(ah)->ah_devid;
		*resultsize = sizeof(HAL_REVS);
		return AH_TRUE;
	case HAL_DIAG_REGS:
		*resultsize = ath_hal_getregdump(ah, args, *result,*resultsize);
		return AH_TRUE;
	case HAL_DIAG_FATALERR:
		*result = &AH_PRIVATE(ah)->ah_fatalState[0];
		*resultsize = sizeof(AH_PRIVATE(ah)->ah_fatalState);
		return AH_TRUE;
	case HAL_DIAG_EEREAD:
		if (argsize != sizeof(uint16_t))
			return AH_FALSE;
		if (!ath_hal_eepromRead(ah, *(const uint16_t *)args, *result))
			return AH_FALSE;
		*resultsize = sizeof(uint16_t);
		return AH_TRUE;
#ifdef AH_PRIVATE_DIAG
	case HAL_DIAG_SETKEY: {
		const HAL_DIAG_KEYVAL *dk;

		if (argsize != sizeof(HAL_DIAG_KEYVAL))
			return AH_FALSE;
		dk = (const HAL_DIAG_KEYVAL *)args;
		return ah->ah_setKeyCacheEntry(ah, dk->dk_keyix,
			&dk->dk_keyval, dk->dk_mac, dk->dk_xor);
	}
	case HAL_DIAG_RESETKEY:
		if (argsize != sizeof(uint16_t))
			return AH_FALSE;
		return ah->ah_resetKeyCacheEntry(ah, *(const uint16_t *)args);
#ifdef AH_SUPPORT_WRITE_EEPROM
	case HAL_DIAG_EEWRITE: {
		const HAL_DIAG_EEVAL *ee;
		if (argsize != sizeof(HAL_DIAG_EEVAL))
			return AH_FALSE;
		ee = (const HAL_DIAG_EEVAL *)args;
		return ath_hal_eepromWrite(ah, ee->ee_off, ee->ee_data);
	}
#endif /* AH_SUPPORT_WRITE_EEPROM */
#endif /* AH_PRIVATE_DIAG */
	case HAL_DIAG_11NCOMPAT:
		if (argsize == 0) {
			*resultsize = sizeof(uint32_t);
			*((uint32_t *)(*result)) =
				AH_PRIVATE(ah)->ah_11nCompat;
		} else if (argsize == sizeof(uint32_t)) {
			AH_PRIVATE(ah)->ah_11nCompat = *(const uint32_t *)args;
		} else
			return AH_FALSE;
		return AH_TRUE;
	}
	return AH_FALSE;
}

/*
 * Set the properties of the tx queue with the parameters
 * from qInfo.
 */
HAL_BOOL
ath_hal_setTxQProps(struct ath_hal *ah,
	HAL_TX_QUEUE_INFO *qi, const HAL_TXQ_INFO *qInfo)
{
	uint32_t cw;

	if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
		HALDEBUG(ah, HAL_DEBUG_TXQUEUE,
		    "%s: inactive queue\n", __func__);
		return AH_FALSE;
	}
	/* XXX validate parameters */
	qi->tqi_ver = qInfo->tqi_ver;
	qi->tqi_subtype = qInfo->tqi_subtype;
	qi->tqi_qflags = qInfo->tqi_qflags;
	qi->tqi_priority = qInfo->tqi_priority;
	if (qInfo->tqi_aifs != HAL_TXQ_USEDEFAULT)
		qi->tqi_aifs = AH_MIN(qInfo->tqi_aifs, 255);
	else
		qi->tqi_aifs = INIT_AIFS;
	if (qInfo->tqi_cwmin != HAL_TXQ_USEDEFAULT) {
		cw = AH_MIN(qInfo->tqi_cwmin, 1024);
		/* make sure that the CWmin is of the form (2^n - 1) */
		qi->tqi_cwmin = 1;
		while (qi->tqi_cwmin < cw)
			qi->tqi_cwmin = (qi->tqi_cwmin << 1) | 1;
	} else
		qi->tqi_cwmin = qInfo->tqi_cwmin;
	if (qInfo->tqi_cwmax != HAL_TXQ_USEDEFAULT) {
		cw = AH_MIN(qInfo->tqi_cwmax, 1024);
		/* make sure that the CWmax is of the form (2^n - 1) */
		qi->tqi_cwmax = 1;
		while (qi->tqi_cwmax < cw)
			qi->tqi_cwmax = (qi->tqi_cwmax << 1) | 1;
	} else
		qi->tqi_cwmax = INIT_CWMAX;
	/* Set retry limit values */
	if (qInfo->tqi_shretry != 0)
		qi->tqi_shretry = AH_MIN(qInfo->tqi_shretry, 15);
	else
		qi->tqi_shretry = INIT_SH_RETRY;
	if (qInfo->tqi_lgretry != 0)
		qi->tqi_lgretry = AH_MIN(qInfo->tqi_lgretry, 15);
	else
		qi->tqi_lgretry = INIT_LG_RETRY;
	qi->tqi_cbrPeriod = qInfo->tqi_cbrPeriod;
	qi->tqi_cbrOverflowLimit = qInfo->tqi_cbrOverflowLimit;
	qi->tqi_burstTime = qInfo->tqi_burstTime;
	qi->tqi_readyTime = qInfo->tqi_readyTime;

	switch (qInfo->tqi_subtype) {
	case HAL_WME_UPSD:
		if (qi->tqi_type == HAL_TX_QUEUE_DATA)
			qi->tqi_intFlags = HAL_TXQ_USE_LOCKOUT_BKOFF_DIS;
		break;
	default:
		break;		/* NB: silence compiler */
	}
	return AH_TRUE;
}

HAL_BOOL
ath_hal_getTxQProps(struct ath_hal *ah,
	HAL_TXQ_INFO *qInfo, const HAL_TX_QUEUE_INFO *qi)
{
	if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
		HALDEBUG(ah, HAL_DEBUG_TXQUEUE,
		    "%s: inactive queue\n", __func__);
		return AH_FALSE;
	}

	qInfo->tqi_qflags = qi->tqi_qflags;
	qInfo->tqi_ver = qi->tqi_ver;
	qInfo->tqi_subtype = qi->tqi_subtype;
	qInfo->tqi_qflags = qi->tqi_qflags;
	qInfo->tqi_priority = qi->tqi_priority;
	qInfo->tqi_aifs = qi->tqi_aifs;
	qInfo->tqi_cwmin = qi->tqi_cwmin;
	qInfo->tqi_cwmax = qi->tqi_cwmax;
	qInfo->tqi_shretry = qi->tqi_shretry;
	qInfo->tqi_lgretry = qi->tqi_lgretry;
	qInfo->tqi_cbrPeriod = qi->tqi_cbrPeriod;
	qInfo->tqi_cbrOverflowLimit = qi->tqi_cbrOverflowLimit;
	qInfo->tqi_burstTime = qi->tqi_burstTime;
	qInfo->tqi_readyTime = qi->tqi_readyTime;
	return AH_TRUE;
}

                                     /* 11a Turbo  11b  11g  108g */
static const int16_t NOISE_FLOOR[] = { -96, -93,  -98, -96,  -93 };

/*
 * Read the current channel noise floor and return.
 * If nf cal hasn't finished, channel noise floor should be 0
 * and we return a nominal value based on band and frequency.
 *
 * NB: This is a private routine used by per-chip code to
 *     implement the ah_getChanNoise method.
 */
int16_t
ath_hal_getChanNoise(struct ath_hal *ah, HAL_CHANNEL *chan)
{
	HAL_CHANNEL_INTERNAL *ichan;

	ichan = ath_hal_checkchannel(ah, chan);
	if (ichan == AH_NULL) {
		HALDEBUG(ah, HAL_DEBUG_NFCAL,
		    "%s: invalid channel %u/0x%x; no mapping\n",
		    __func__, chan->channel, chan->channelFlags);
		return 0;
	}
	if (ichan->rawNoiseFloor == 0) {
		WIRELESS_MODE mode = ath_hal_chan2wmode(ah, chan);

		HALASSERT(mode < WIRELESS_MODE_MAX);
		return NOISE_FLOOR[mode] + ath_hal_getNfAdjust(ah, ichan);
	} else
		return ichan->rawNoiseFloor + ichan->noiseFloorAdjust;
}

/*
 * Process all valid raw noise floors into the dBm noise floor values.
 * Though our device has no reference for a dBm noise floor, we perform
 * a relative minimization of NF's based on the lowest NF found across a
 * channel scan.
 */
void
ath_hal_process_noisefloor(struct ath_hal *ah)
{
	HAL_CHANNEL_INTERNAL *c;
	int16_t correct2, correct5;
	int16_t lowest2, lowest5;
	int i;

	/* 
	 * Find the lowest 2GHz and 5GHz noise floor values after adjusting
	 * for statistically recorded NF/channel deviation.
	 */
	correct2 = lowest2 = 0;
	correct5 = lowest5 = 0;
	for (i = 0; i < AH_PRIVATE(ah)->ah_nchan; i++) {
		WIRELESS_MODE mode;
		int16_t nf;

		c = &AH_PRIVATE(ah)->ah_channels[i];
		if (c->rawNoiseFloor >= 0)
			continue;
		mode = ath_hal_chan2wmode(ah, (HAL_CHANNEL *) c);
		HALASSERT(mode < WIRELESS_MODE_MAX);
		nf = c->rawNoiseFloor + NOISE_FLOOR[mode] +
			ath_hal_getNfAdjust(ah, c);
		if (IS_CHAN_5GHZ(c)) {
			if (nf < lowest5) { 
				lowest5 = nf;
				correct5 = NOISE_FLOOR[mode] -
				    (c->rawNoiseFloor + ath_hal_getNfAdjust(ah, c));
			}
		} else {
			if (nf < lowest2) { 
				lowest2 = nf;
				correct2 = NOISE_FLOOR[mode] -
				    (c->rawNoiseFloor + ath_hal_getNfAdjust(ah, c));
			}
		}
	}

	/* Correct the channels to reach the expected NF value */
	for (i = 0; i < AH_PRIVATE(ah)->ah_nchan; i++) {
		c = &AH_PRIVATE(ah)->ah_channels[i];
		if (c->rawNoiseFloor >= 0)
			continue;
		/* Apply correction factor */
		c->noiseFloorAdjust = ath_hal_getNfAdjust(ah, c) +
			(IS_CHAN_5GHZ(c) ? correct5 : correct2);
		HALDEBUG(ah, HAL_DEBUG_NFCAL, "%u/0x%x raw nf %d adjust %d\n",
		    c->channel, c->channelFlags, c->rawNoiseFloor,
		    c->noiseFloorAdjust);
	}
}

/*
 * INI support routines.
 */

int
ath_hal_ini_write(struct ath_hal *ah, const HAL_INI_ARRAY *ia,
	int col, int regWr)
{
	int r;

	for (r = 0; r < ia->rows; r++) {
		OS_REG_WRITE(ah, HAL_INI_VAL(ia, r, 0),
		    HAL_INI_VAL(ia, r, col));
		DMA_YIELD(regWr);
	}
	return regWr;
}

void
ath_hal_ini_bank_setup(uint32_t data[], const HAL_INI_ARRAY *ia, int col)
{
	int r;

	for (r = 0; r < ia->rows; r++)
		data[r] = HAL_INI_VAL(ia, r, col);
}

int
ath_hal_ini_bank_write(struct ath_hal *ah, const HAL_INI_ARRAY *ia,
	const uint32_t data[], int regWr)
{
	int r;

	for (r = 0; r < ia->rows; r++) {
		OS_REG_WRITE(ah, HAL_INI_VAL(ia, r, 0), data[r]);
		DMA_YIELD(regWr);
	}
	return regWr;
}
