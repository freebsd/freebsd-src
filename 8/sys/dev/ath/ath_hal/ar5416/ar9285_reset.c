/*
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
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

/*
 * This is almost the same as ar5416_reset.c but uses the v4k EEPROM and
 * supports only 2Ghz operation.
 */

#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"

#include "ah_eeprom_v14.h"
#include "ah_eeprom_v4k.h"

#include "ar5416/ar9285.h"
#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

/* Eeprom versioning macros. Returns true if the version is equal or newer than the ver specified */ 
#define	EEP_MINOR(_ah) \
	(AH_PRIVATE(_ah)->ah_eeversion & AR5416_EEP_VER_MINOR_MASK)
#define IS_EEP_MINOR_V2(_ah)	(EEP_MINOR(_ah) >= AR5416_EEP_MINOR_VER_2)
#define IS_EEP_MINOR_V3(_ah)	(EEP_MINOR(_ah) >= AR5416_EEP_MINOR_VER_3)

/* Additional Time delay to wait after activiting the Base band */
#define BASE_ACTIVATE_DELAY	100	/* 100 usec */
#define PLL_SETTLE_DELAY	300	/* 300 usec */
#define RTC_PLL_SETTLE_DELAY    1000    /* 1 ms     */

static HAL_BOOL ar9285SetPowerPerRateTable(struct ath_hal *ah,
	struct ar5416eeprom_4k *pEepData, 
	const struct ieee80211_channel *chan, int16_t *ratesArray,
	uint16_t cfgCtl, uint16_t AntennaReduction,
	uint16_t twiceMaxRegulatoryPower, 
	uint16_t powerLimit);
static HAL_BOOL ar9285SetPowerCalTable(struct ath_hal *ah,
	struct ar5416eeprom_4k *pEepData,
	const struct ieee80211_channel *chan,
	int16_t *pTxPowerIndexOffset);
static int16_t interpolate(uint16_t target, uint16_t srcLeft,
	uint16_t srcRight, int16_t targetLeft, int16_t targetRight);
static HAL_BOOL ar9285FillVpdTable(uint8_t, uint8_t, uint8_t *, uint8_t *,
		                   uint16_t, uint8_t *);
static void ar9285GetGainBoundariesAndPdadcs(struct ath_hal *ah, 
	const struct ieee80211_channel *chan, CAL_DATA_PER_FREQ_4K *pRawDataSet,
	uint8_t * bChans, uint16_t availPiers,
	uint16_t tPdGainOverlap, int16_t *pMinCalPower,
	uint16_t * pPdGainBoundaries, uint8_t * pPDADCValues,
	uint16_t numXpdGains);
static HAL_BOOL getLowerUpperIndex(uint8_t target, uint8_t *pList,
	uint16_t listSize,  uint16_t *indexL, uint16_t *indexR);
static uint16_t ar9285GetMaxEdgePower(uint16_t, CAL_CTL_EDGES *);

/* XXX gag, this is sick */
typedef enum Ar5416_Rates {
	rate6mb,  rate9mb,  rate12mb, rate18mb,
	rate24mb, rate36mb, rate48mb, rate54mb,
	rate1l,   rate2l,   rate2s,   rate5_5l,
	rate5_5s, rate11l,  rate11s,  rateXr,
	rateHt20_0, rateHt20_1, rateHt20_2, rateHt20_3,
	rateHt20_4, rateHt20_5, rateHt20_6, rateHt20_7,
	rateHt40_0, rateHt40_1, rateHt40_2, rateHt40_3,
	rateHt40_4, rateHt40_5, rateHt40_6, rateHt40_7,
	rateDupCck, rateDupOfdm, rateExtCck, rateExtOfdm,
	Ar5416RateSize
} AR5416_RATES;

HAL_BOOL
ar9285SetTransmitPower(struct ath_hal *ah,
	const struct ieee80211_channel *chan, uint16_t *rfXpdGain)
{
#define POW_SM(_r, _s)     (((_r) & 0x3f) << (_s))
#define N(a)            (sizeof (a) / sizeof (a[0]))

    MODAL_EEP4K_HEADER	*pModal;
    struct ath_hal_5212 *ahp = AH5212(ah);
    int16_t		ratesArray[Ar5416RateSize];
    int16_t		txPowerIndexOffset = 0;
    uint8_t		ht40PowerIncForPdadc = 2;	
    int			i;
    
    uint16_t		cfgCtl;
    uint16_t		powerLimit;
    uint16_t		twiceAntennaReduction;
    uint16_t		twiceMaxRegulatoryPower;
    int16_t		maxPower;
    HAL_EEPROM_v4k *ee = AH_PRIVATE(ah)->ah_eeprom;
    struct ar5416eeprom_4k *pEepData = &ee->ee_base;

    HALASSERT(AH_PRIVATE(ah)->ah_eeversion >= AR_EEPROM_VER14_1);

    /* Setup info for the actual eeprom */
    OS_MEMZERO(ratesArray, sizeof(ratesArray));
    cfgCtl = ath_hal_getctl(ah, chan);
    powerLimit = chan->ic_maxregpower * 2;
    twiceAntennaReduction = chan->ic_maxantgain;
    twiceMaxRegulatoryPower = AH_MIN(MAX_RATE_POWER, AH_PRIVATE(ah)->ah_powerLimit); 
    pModal = &pEepData->modalHeader;
    HALDEBUG(ah, HAL_DEBUG_RESET, "%s Channel=%u CfgCtl=%u\n",
	__func__,chan->ic_freq, cfgCtl );      
  
    if (IS_EEP_MINOR_V2(ah)) {
        ht40PowerIncForPdadc = pModal->ht40PowerIncForPdadc;
    }
 
    if (!ar9285SetPowerPerRateTable(ah, pEepData,  chan,
                                    &ratesArray[0],cfgCtl,
                                    twiceAntennaReduction,
				    twiceMaxRegulatoryPower, powerLimit)) {
        HALDEBUG(ah, HAL_DEBUG_ANY,
	    "%s: unable to set tx power per rate table\n", __func__);
        return AH_FALSE;
    }

    if (!ar9285SetPowerCalTable(ah,  pEepData, chan, &txPowerIndexOffset)) {
        HALDEBUG(ah, HAL_DEBUG_ANY, "%s: unable to set power table\n",
	    __func__);
        return AH_FALSE;
    }
  
    maxPower = AH_MAX(ratesArray[rate6mb], ratesArray[rateHt20_0]);
    maxPower = AH_MAX(maxPower, ratesArray[rate1l]);

    if (IEEE80211_IS_CHAN_HT40(chan)) {
        maxPower = AH_MAX(maxPower, ratesArray[rateHt40_0]);
    }

    ahp->ah_tx6PowerInHalfDbm = maxPower;   
    AH_PRIVATE(ah)->ah_maxPowerLevel = maxPower;
    ahp->ah_txPowerIndexOffset = txPowerIndexOffset;

    /*
     * txPowerIndexOffset is set by the SetPowerTable() call -
     *  adjust the rate table (0 offset if rates EEPROM not loaded)
     */
    for (i = 0; i < N(ratesArray); i++) {
        ratesArray[i] = (int16_t)(txPowerIndexOffset + ratesArray[i]);
        if (ratesArray[i] > AR5416_MAX_RATE_POWER)
            ratesArray[i] = AR5416_MAX_RATE_POWER;
	ratesArray[i] -= AR5416_PWR_TABLE_OFFSET_DB * 2;
    }

#ifdef AH_EEPROM_DUMP
    ar5416PrintPowerPerRate(ah, ratesArray);
#endif

    /* Write the OFDM power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE1,
        POW_SM(ratesArray[rate18mb], 24)
          | POW_SM(ratesArray[rate12mb], 16)
          | POW_SM(ratesArray[rate9mb], 8)
          | POW_SM(ratesArray[rate6mb], 0)
    );
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE2,
        POW_SM(ratesArray[rate54mb], 24)
          | POW_SM(ratesArray[rate48mb], 16)
          | POW_SM(ratesArray[rate36mb], 8)
          | POW_SM(ratesArray[rate24mb], 0)
    );

    /* Write the CCK power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
        POW_SM(ratesArray[rate2s], 24)
          | POW_SM(ratesArray[rate2l],  16)
          | POW_SM(ratesArray[rateXr],  8) /* XR target power */
          | POW_SM(ratesArray[rate1l],   0)
    );
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
        POW_SM(ratesArray[rate11s], 24)
          | POW_SM(ratesArray[rate11l], 16)
          | POW_SM(ratesArray[rate5_5s], 8)
          | POW_SM(ratesArray[rate5_5l], 0)
    );
    HALDEBUG(ah, HAL_DEBUG_RESET,
	"%s AR_PHY_POWER_TX_RATE3=0x%x AR_PHY_POWER_TX_RATE4=0x%x\n",
	    __func__, OS_REG_READ(ah,AR_PHY_POWER_TX_RATE3),
	    OS_REG_READ(ah,AR_PHY_POWER_TX_RATE4)); 

    /* Write the HT20 power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE5,
        POW_SM(ratesArray[rateHt20_3], 24)
          | POW_SM(ratesArray[rateHt20_2], 16)
          | POW_SM(ratesArray[rateHt20_1], 8)
          | POW_SM(ratesArray[rateHt20_0], 0)
    );
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE6,
        POW_SM(ratesArray[rateHt20_7], 24)
          | POW_SM(ratesArray[rateHt20_6], 16)
          | POW_SM(ratesArray[rateHt20_5], 8)
          | POW_SM(ratesArray[rateHt20_4], 0)
    );

    if (IEEE80211_IS_CHAN_HT40(chan)) {
        /* Write the HT40 power per rate set */
	/* Correct PAR difference between HT40 and HT20/LEGACY */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE7,
            POW_SM(ratesArray[rateHt40_3] + ht40PowerIncForPdadc, 24)
              | POW_SM(ratesArray[rateHt40_2] + ht40PowerIncForPdadc, 16)
              | POW_SM(ratesArray[rateHt40_1] + ht40PowerIncForPdadc, 8)
              | POW_SM(ratesArray[rateHt40_0] + ht40PowerIncForPdadc, 0)
        );
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE8,
            POW_SM(ratesArray[rateHt40_7] + ht40PowerIncForPdadc, 24)
              | POW_SM(ratesArray[rateHt40_6] + ht40PowerIncForPdadc, 16)
              | POW_SM(ratesArray[rateHt40_5] + ht40PowerIncForPdadc, 8)
              | POW_SM(ratesArray[rateHt40_4] + ht40PowerIncForPdadc, 0)
        );
        /* Write the Dup/Ext 40 power per rate set */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE9,
            POW_SM(ratesArray[rateExtOfdm], 24)
              | POW_SM(ratesArray[rateExtCck], 16)
              | POW_SM(ratesArray[rateDupOfdm], 8)
              | POW_SM(ratesArray[rateDupCck], 0)
        );
    }

    return AH_TRUE;
#undef POW_SM
#undef N
}

HAL_BOOL
ar9285SetBoardValues(struct ath_hal *ah, const struct ieee80211_channel *chan)
{
    const HAL_EEPROM_v4k *ee = AH_PRIVATE(ah)->ah_eeprom;
    const struct ar5416eeprom_4k *eep = &ee->ee_base;
    const MODAL_EEP4K_HEADER *pModal;
    uint8_t	txRxAttenLocal = 23;

    HALASSERT(AH_PRIVATE(ah)->ah_eeversion >= AR_EEPROM_VER14_1);
    pModal = &eep->modalHeader;

    OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, pModal->antCtrlCommon);
    OS_REG_WRITE(ah, AR_PHY_SWITCH_CHAIN_0, pModal->antCtrlChain[0]);
    OS_REG_WRITE(ah, AR_PHY_TIMING_CTRL4,
        	(OS_REG_READ(ah, AR_PHY_TIMING_CTRL4) &
        	~(AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF | AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF)) |
        	SM(pModal->iqCalICh[0], AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF) |
        	SM(pModal->iqCalQCh[0], AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF));

    if (IS_EEP_MINOR_V3(ah)) {
	if (IEEE80211_IS_CHAN_HT40(chan)) {
		/* Overwrite switch settling with HT40 value */
		OS_REG_RMW_FIELD(ah, AR_PHY_SETTLING, AR_PHY_SETTLING_SWITCH,
		    pModal->swSettleHt40);
	}
	txRxAttenLocal = pModal->txRxAttenCh[0];

        OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ, AR_PHY_GAIN_2GHZ_XATTEN1_MARGIN,
	    pModal->bswMargin[0]);
        OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ, AR_PHY_GAIN_2GHZ_XATTEN1_DB,
	    pModal->bswAtten[0]);
	OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ, AR_PHY_GAIN_2GHZ_XATTEN2_MARGIN,
	    pModal->xatten2Margin[0]);
	OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ, AR_PHY_GAIN_2GHZ_XATTEN2_DB,
	    pModal->xatten2Db[0]);

	/* block 1 has the same values as block 0 */	
        OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + 0x1000,
	    AR_PHY_GAIN_2GHZ_XATTEN1_MARGIN, pModal->bswMargin[0]);
        OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + 0x1000,
	    AR_PHY_GAIN_2GHZ_XATTEN1_DB, pModal->bswAtten[0]);
	OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + 0x1000,
	    AR_PHY_GAIN_2GHZ_XATTEN2_MARGIN, pModal->xatten2Margin[0]);
	OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + 0x1000,
	    AR_PHY_GAIN_2GHZ_XATTEN2_DB, pModal->xatten2Db[0]);

    }
    OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN,
        AR9280_PHY_RXGAIN_TXRX_ATTEN, txRxAttenLocal);
    OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN,
        AR9280_PHY_RXGAIN_TXRX_MARGIN, pModal->rxTxMarginCh[0]);

    OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN + 0x1000,
        AR9280_PHY_RXGAIN_TXRX_ATTEN, txRxAttenLocal);
    OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN + 0x1000,
        AR9280_PHY_RXGAIN_TXRX_MARGIN, pModal->rxTxMarginCh[0]);

    if (AR_SREV_KITE_11(ah))
	    OS_REG_WRITE(ah, AR9285_AN_TOP4, (AR9285_AN_TOP4_DEFAULT | 0x14));

    return AH_TRUE;
}

/*
 * Helper functions common for AP/CB/XB
 */

static HAL_BOOL
ar9285SetPowerPerRateTable(struct ath_hal *ah, struct ar5416eeprom_4k *pEepData,
                           const struct ieee80211_channel *chan,
                           int16_t *ratesArray, uint16_t cfgCtl,
                           uint16_t AntennaReduction, 
                           uint16_t twiceMaxRegulatoryPower,
                           uint16_t powerLimit)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
/* Local defines to distinguish between extension and control CTL's */
#define EXT_ADDITIVE (0x8000)
#define CTL_11G_EXT (CTL_11G | EXT_ADDITIVE)
#define CTL_11B_EXT (CTL_11B | EXT_ADDITIVE)

	uint16_t twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
	int i;
	int16_t  twiceLargestAntenna;
	CAL_CTL_DATA_4K *rep;
	CAL_TARGET_POWER_LEG targetPowerOfdm, targetPowerCck = {0, {0, 0, 0, 0}};
	CAL_TARGET_POWER_LEG targetPowerOfdmExt = {0, {0, 0, 0, 0}}, targetPowerCckExt = {0, {0, 0, 0, 0}};
	CAL_TARGET_POWER_HT  targetPowerHt20, targetPowerHt40 = {0, {0, 0, 0, 0}};
	int16_t scaledPower, minCtlPower;

#define SUB_NUM_CTL_MODES_AT_2G_40 3   /* excluding HT40, EXT-OFDM, EXT-CCK */
	static const uint16_t ctlModesFor11g[] = {
	   CTL_11B, CTL_11G, CTL_2GHT20, CTL_11B_EXT, CTL_11G_EXT, CTL_2GHT40
	};
	const uint16_t *pCtlMode;
	uint16_t numCtlModes, ctlMode, freq;
	CHAN_CENTERS centers;

	ar5416GetChannelCenters(ah,  chan, &centers);

	/* Compute TxPower reduction due to Antenna Gain */

	twiceLargestAntenna = pEepData->modalHeader.antennaGainCh[0];
	twiceLargestAntenna = (int16_t)AH_MIN((AntennaReduction) - twiceLargestAntenna, 0);

	/* XXX setup for 5212 use (really used?) */
	ath_hal_eepromSet(ah, AR_EEP_ANTGAINMAX_2, twiceLargestAntenna);

	/* 
	 * scaledPower is the minimum of the user input power level and
	 * the regulatory allowed power level
	 */
	scaledPower = AH_MIN(powerLimit, twiceMaxRegulatoryPower + twiceLargestAntenna);

	/* Get target powers from EEPROM - our baseline for TX Power */
	/* Setup for CTL modes */
	numCtlModes = N(ctlModesFor11g) - SUB_NUM_CTL_MODES_AT_2G_40; /* CTL_11B, CTL_11G, CTL_2GHT20 */
	pCtlMode = ctlModesFor11g;

	ar5416GetTargetPowersLeg(ah,  chan, pEepData->calTargetPowerCck,
			AR5416_4K_NUM_2G_CCK_TARGET_POWERS, &targetPowerCck, 4, AH_FALSE);
	ar5416GetTargetPowersLeg(ah,  chan, pEepData->calTargetPower2G,
			AR5416_4K_NUM_2G_20_TARGET_POWERS, &targetPowerOfdm, 4, AH_FALSE);
	ar5416GetTargetPowers(ah,  chan, pEepData->calTargetPower2GHT20,
			AR5416_4K_NUM_2G_20_TARGET_POWERS, &targetPowerHt20, 8, AH_FALSE);

	if (IEEE80211_IS_CHAN_HT40(chan)) {
		numCtlModes = N(ctlModesFor11g);    /* All 2G CTL's */

		ar5416GetTargetPowers(ah,  chan, pEepData->calTargetPower2GHT40,
			AR5416_4K_NUM_2G_40_TARGET_POWERS, &targetPowerHt40, 8, AH_TRUE);
		/* Get target powers for extension channels */
		ar5416GetTargetPowersLeg(ah,  chan, pEepData->calTargetPowerCck,
			AR5416_4K_NUM_2G_CCK_TARGET_POWERS, &targetPowerCckExt, 4, AH_TRUE);
		ar5416GetTargetPowersLeg(ah,  chan, pEepData->calTargetPower2G,
			AR5416_4K_NUM_2G_20_TARGET_POWERS, &targetPowerOfdmExt, 4, AH_TRUE);
	}

	/*
	 * For MIMO, need to apply regulatory caps individually across dynamically
	 * running modes: CCK, OFDM, HT20, HT40
	 *
	 * The outer loop walks through each possible applicable runtime mode.
	 * The inner loop walks through each ctlIndex entry in EEPROM.
	 * The ctl value is encoded as [7:4] == test group, [3:0] == test mode.
	 *
	 */
	for (ctlMode = 0; ctlMode < numCtlModes; ctlMode++) {
		HAL_BOOL isHt40CtlMode = (pCtlMode[ctlMode] == CTL_5GHT40) ||
		    (pCtlMode[ctlMode] == CTL_2GHT40);
		if (isHt40CtlMode) {
			freq = centers.ctl_center;
		} else if (pCtlMode[ctlMode] & EXT_ADDITIVE) {
			freq = centers.ext_center;
		} else {
			freq = centers.ctl_center;
		}

		/* walk through each CTL index stored in EEPROM */
		for (i = 0; (i < AR5416_4K_NUM_CTLS) && pEepData->ctlIndex[i]; i++) {
			uint16_t twiceMinEdgePower;

			/* compare test group from regulatory channel list with test mode from pCtlMode list */
			if ((((cfgCtl & ~CTL_MODE_M) | (pCtlMode[ctlMode] & CTL_MODE_M)) == pEepData->ctlIndex[i]) ||
				(((cfgCtl & ~CTL_MODE_M) | (pCtlMode[ctlMode] & CTL_MODE_M)) == 
				 ((pEepData->ctlIndex[i] & CTL_MODE_M) | SD_NO_CTL))) {
				rep = &(pEepData->ctlData[i]);
				twiceMinEdgePower = ar9285GetMaxEdgePower(freq,
							rep->ctlEdges[
							  owl_get_ntxchains(AH5416(ah)->ah_tx_chainmask) - 1]);
				if ((cfgCtl & ~CTL_MODE_M) == SD_NO_CTL) {
					/* Find the minimum of all CTL edge powers that apply to this channel */
					twiceMaxEdgePower = AH_MIN(twiceMaxEdgePower, twiceMinEdgePower);
				} else {
					/* specific */
					twiceMaxEdgePower = twiceMinEdgePower;
					break;
				}
			}
		}
		minCtlPower = (uint8_t)AH_MIN(twiceMaxEdgePower, scaledPower);
		/* Apply ctl mode to correct target power set */
		switch(pCtlMode[ctlMode]) {
		case CTL_11B:
			for (i = 0; i < N(targetPowerCck.tPow2x); i++) {
				targetPowerCck.tPow2x[i] = (uint8_t)AH_MIN(targetPowerCck.tPow2x[i], minCtlPower);
			}
			break;
		case CTL_11A:
		case CTL_11G:
			for (i = 0; i < N(targetPowerOfdm.tPow2x); i++) {
				targetPowerOfdm.tPow2x[i] = (uint8_t)AH_MIN(targetPowerOfdm.tPow2x[i], minCtlPower);
			}
			break;
		case CTL_5GHT20:
		case CTL_2GHT20:
			for (i = 0; i < N(targetPowerHt20.tPow2x); i++) {
				targetPowerHt20.tPow2x[i] = (uint8_t)AH_MIN(targetPowerHt20.tPow2x[i], minCtlPower);
			}
			break;
		case CTL_11B_EXT:
			targetPowerCckExt.tPow2x[0] = (uint8_t)AH_MIN(targetPowerCckExt.tPow2x[0], minCtlPower);
			break;
		case CTL_11G_EXT:
			targetPowerOfdmExt.tPow2x[0] = (uint8_t)AH_MIN(targetPowerOfdmExt.tPow2x[0], minCtlPower);
			break;
		case CTL_5GHT40:
		case CTL_2GHT40:
			for (i = 0; i < N(targetPowerHt40.tPow2x); i++) {
				targetPowerHt40.tPow2x[i] = (uint8_t)AH_MIN(targetPowerHt40.tPow2x[i], minCtlPower);
			}
			break;
		default:
			return AH_FALSE;
			break;
		}
	} /* end ctl mode checking */

	/* Set rates Array from collected data */
	ratesArray[rate6mb] = ratesArray[rate9mb] = ratesArray[rate12mb] = ratesArray[rate18mb] = ratesArray[rate24mb] = targetPowerOfdm.tPow2x[0];
	ratesArray[rate36mb] = targetPowerOfdm.tPow2x[1];
	ratesArray[rate48mb] = targetPowerOfdm.tPow2x[2];
	ratesArray[rate54mb] = targetPowerOfdm.tPow2x[3];
	ratesArray[rateXr] = targetPowerOfdm.tPow2x[0];

	for (i = 0; i < N(targetPowerHt20.tPow2x); i++) {
		ratesArray[rateHt20_0 + i] = targetPowerHt20.tPow2x[i];
	}

	ratesArray[rate1l]  = targetPowerCck.tPow2x[0];
	ratesArray[rate2s] = ratesArray[rate2l]  = targetPowerCck.tPow2x[1];
	ratesArray[rate5_5s] = ratesArray[rate5_5l] = targetPowerCck.tPow2x[2];
	ratesArray[rate11s] = ratesArray[rate11l] = targetPowerCck.tPow2x[3];
	if (IEEE80211_IS_CHAN_HT40(chan)) {
		for (i = 0; i < N(targetPowerHt40.tPow2x); i++) {
			ratesArray[rateHt40_0 + i] = targetPowerHt40.tPow2x[i];
		}
		ratesArray[rateDupOfdm] = targetPowerHt40.tPow2x[0];
		ratesArray[rateDupCck]  = targetPowerHt40.tPow2x[0];
		ratesArray[rateExtOfdm] = targetPowerOfdmExt.tPow2x[0];
		if (IEEE80211_IS_CHAN_2GHZ(chan)) {
			ratesArray[rateExtCck]  = targetPowerCckExt.tPow2x[0];
		}
	}
	return AH_TRUE;
#undef EXT_ADDITIVE
#undef CTL_11G_EXT
#undef CTL_11B_EXT
#undef SUB_NUM_CTL_MODES_AT_2G_40
#undef N
}

/**************************************************************************
 * fbin2freq
 *
 * Get channel value from binary representation held in eeprom
 * RETURNS: the frequency in MHz
 */
static uint16_t
fbin2freq(uint8_t fbin)
{
    /*
     * Reserved value 0xFF provides an empty definition both as
     * an fbin and as a frequency - do not convert
     */
    if (fbin == AR5416_BCHAN_UNUSED) {
        return fbin;
    }

    return (uint16_t)(2300 + fbin);
}

/*
 * XXX almost the same as ar5416GetMaxEdgePower.
 */
static uint16_t
ar9285GetMaxEdgePower(uint16_t freq, CAL_CTL_EDGES *pRdEdgesPower)
{
    uint16_t twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
    int      i;

    /* Get the edge power */
    for (i = 0; (i < AR5416_NUM_BAND_EDGES) && (pRdEdgesPower[i].bChannel != AR5416_BCHAN_UNUSED) ; i++) {
        /*
         * If there's an exact channel match or an inband flag set
         * on the lower channel use the given rdEdgePower
         */
        if (freq == fbin2freq(pRdEdgesPower[i].bChannel)) {
            twiceMaxEdgePower = MS(pRdEdgesPower[i].tPowerFlag, CAL_CTL_EDGES_POWER);
            break;
        } else if ((i > 0) && (freq < fbin2freq(pRdEdgesPower[i].bChannel))) {
            if (fbin2freq(pRdEdgesPower[i - 1].bChannel) < freq && (pRdEdgesPower[i - 1].tPowerFlag & CAL_CTL_EDGES_FLAG) != 0) {
                twiceMaxEdgePower = MS(pRdEdgesPower[i - 1].tPowerFlag, CAL_CTL_EDGES_POWER);
            }
            /* Leave loop - no more affecting edges possible in this monotonic increasing list */
            break;
        }
    }
    HALASSERT(twiceMaxEdgePower > 0);
    return twiceMaxEdgePower;
}



static HAL_BOOL
ar9285SetPowerCalTable(struct ath_hal *ah, struct ar5416eeprom_4k *pEepData,
	const struct ieee80211_channel *chan, int16_t *pTxPowerIndexOffset)
{
    CAL_DATA_PER_FREQ_4K *pRawDataset;
    uint8_t  *pCalBChans = AH_NULL;
    uint16_t pdGainOverlap_t2;
    static uint8_t  pdadcValues[AR5416_NUM_PDADC_VALUES];
    uint16_t gainBoundaries[AR5416_PD_GAINS_IN_MASK];
    uint16_t numPiers, i, j;
    int16_t  tMinCalPower;
    uint16_t numXpdGain, xpdMask;
    uint16_t xpdGainValues[AR5416_4K_NUM_PD_GAINS];
    uint32_t reg32, regOffset, regChainOffset;

    OS_MEMZERO(xpdGainValues, sizeof(xpdGainValues));
    
    xpdMask = pEepData->modalHeader.xpdGain;

    if (IS_EEP_MINOR_V2(ah)) {
        pdGainOverlap_t2 = pEepData->modalHeader.pdGainOverlap;
    } else { 
    	pdGainOverlap_t2 = (uint16_t)(MS(OS_REG_READ(ah, AR_PHY_TPCRG5), AR_PHY_TPCRG5_PD_GAIN_OVERLAP));
    }

    pCalBChans = pEepData->calFreqPier2G;
    numPiers = AR5416_4K_NUM_2G_CAL_PIERS;
    numXpdGain = 0;
    /* Calculate the value of xpdgains from the xpdGain Mask */
    for (i = 1; i <= AR5416_PD_GAINS_IN_MASK; i++) {
        if ((xpdMask >> (AR5416_PD_GAINS_IN_MASK - i)) & 1) {
            if (numXpdGain >= AR5416_4K_NUM_PD_GAINS) {
                HALASSERT(0);
                break;
            }
            xpdGainValues[numXpdGain] = (uint16_t)(AR5416_PD_GAINS_IN_MASK - i);
            numXpdGain++;
        }
    }
    
    /* Write the detector gain biases and their number */
    OS_REG_WRITE(ah, AR_PHY_TPCRG1, (OS_REG_READ(ah, AR_PHY_TPCRG1) & 
    	~(AR_PHY_TPCRG1_NUM_PD_GAIN | AR_PHY_TPCRG1_PD_GAIN_1 | AR_PHY_TPCRG1_PD_GAIN_2 | AR_PHY_TPCRG1_PD_GAIN_3)) | 
	SM(numXpdGain - 1, AR_PHY_TPCRG1_NUM_PD_GAIN) | SM(xpdGainValues[0], AR_PHY_TPCRG1_PD_GAIN_1 ) |
	SM(xpdGainValues[1], AR_PHY_TPCRG1_PD_GAIN_2) | SM(0, AR_PHY_TPCRG1_PD_GAIN_3));

    for (i = 0; i < AR5416_MAX_CHAINS; i++) {

            if (AR_SREV_OWL_20_OR_LATER(ah) && 
            ( AH5416(ah)->ah_rx_chainmask == 0x5 || AH5416(ah)->ah_tx_chainmask == 0x5) && (i != 0)) {
            /* Regs are swapped from chain 2 to 1 for 5416 2_0 with 
             * only chains 0 and 2 populated 
             */
            regChainOffset = (i == 1) ? 0x2000 : 0x1000;
        } else {
            regChainOffset = i * 0x1000;
        }

        if (pEepData->baseEepHeader.txMask & (1 << i)) {
            pRawDataset = pEepData->calPierData2G[i];

            ar9285GetGainBoundariesAndPdadcs(ah,  chan, pRawDataset,
                                             pCalBChans, numPiers,
                                             pdGainOverlap_t2,
                                             &tMinCalPower, gainBoundaries,
                                             pdadcValues, numXpdGain);

            if ((i == 0) || AR_SREV_OWL_20_OR_LATER(ah)) {
                /*
                 * Note the pdadc table may not start at 0 dBm power, could be
                 * negative or greater than 0.  Need to offset the power
                 * values by the amount of minPower for griffin
                 */

                OS_REG_WRITE(ah, AR_PHY_TPCRG5 + regChainOffset,
                     SM(pdGainOverlap_t2, AR_PHY_TPCRG5_PD_GAIN_OVERLAP) |
                     SM(gainBoundaries[0], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_1)  |
                     SM(gainBoundaries[1], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_2)  |
                     SM(gainBoundaries[2], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_3)  |
                     SM(gainBoundaries[3], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_4));
            }

            /* Write the power values into the baseband power table */
            regOffset = AR_PHY_BASE + (672 << 2) + regChainOffset;

            for (j = 0; j < 32; j++) {
                reg32 = ((pdadcValues[4*j + 0] & 0xFF) << 0)  |
                    ((pdadcValues[4*j + 1] & 0xFF) << 8)  |
                    ((pdadcValues[4*j + 2] & 0xFF) << 16) |
                    ((pdadcValues[4*j + 3] & 0xFF) << 24) ;
                OS_REG_WRITE(ah, regOffset, reg32);

#ifdef PDADC_DUMP
		ath_hal_printf(ah, "PDADC: Chain %d | PDADC %3d Value %3d | PDADC %3d Value %3d | PDADC %3d Value %3d | PDADC %3d Value %3d |\n",
			       i,
			       4*j, pdadcValues[4*j],
			       4*j+1, pdadcValues[4*j + 1],
			       4*j+2, pdadcValues[4*j + 2],
			       4*j+3, pdadcValues[4*j + 3]);
#endif
                regOffset += 4;
            }
        }
    }
    *pTxPowerIndexOffset = 0;

    return AH_TRUE;
}

static void
ar9285GetGainBoundariesAndPdadcs(struct ath_hal *ah, 
                                 const struct ieee80211_channel *chan,
				 CAL_DATA_PER_FREQ_4K *pRawDataSet,
                                 uint8_t * bChans,  uint16_t availPiers,
                                 uint16_t tPdGainOverlap, int16_t *pMinCalPower, uint16_t * pPdGainBoundaries,
                                 uint8_t * pPDADCValues, uint16_t numXpdGains)
{

    int       i, j, k;
    int16_t   ss;         /* potentially -ve index for taking care of pdGainOverlap */
    uint16_t  idxL, idxR, numPiers; /* Pier indexes */

    /* filled out Vpd table for all pdGains (chanL) */
    static uint8_t   vpdTableL[AR5416_4K_NUM_PD_GAINS][AR5416_MAX_PWR_RANGE_IN_HALF_DB];

    /* filled out Vpd table for all pdGains (chanR) */
    static uint8_t   vpdTableR[AR5416_4K_NUM_PD_GAINS][AR5416_MAX_PWR_RANGE_IN_HALF_DB];

    /* filled out Vpd table for all pdGains (interpolated) */
    static uint8_t   vpdTableI[AR5416_4K_NUM_PD_GAINS][AR5416_MAX_PWR_RANGE_IN_HALF_DB];

    uint8_t   *pVpdL, *pVpdR, *pPwrL, *pPwrR;
    uint8_t   minPwrT4[AR5416_4K_NUM_PD_GAINS];
    uint8_t   maxPwrT4[AR5416_4K_NUM_PD_GAINS];
    int16_t   vpdStep;
    int16_t   tmpVal;
    uint16_t  sizeCurrVpdTable, maxIndex, tgtIndex;
    HAL_BOOL    match;
    int16_t  minDelta = 0;
    CHAN_CENTERS centers;

    ar5416GetChannelCenters(ah, chan, &centers);

    /* Trim numPiers for the number of populated channel Piers */
    for (numPiers = 0; numPiers < availPiers; numPiers++) {
        if (bChans[numPiers] == AR5416_BCHAN_UNUSED) {
            break;
        }
    }

    /* Find pier indexes around the current channel */
    match = getLowerUpperIndex((uint8_t)FREQ2FBIN(centers.synth_center, IEEE80211_IS_CHAN_2GHZ(chan)),
			bChans, numPiers, &idxL, &idxR);

    if (match) {
        /* Directly fill both vpd tables from the matching index */
        for (i = 0; i < numXpdGains; i++) {
            minPwrT4[i] = pRawDataSet[idxL].pwrPdg[i][0];
            maxPwrT4[i] = pRawDataSet[idxL].pwrPdg[i][4];
            ar9285FillVpdTable(minPwrT4[i], maxPwrT4[i],
			       pRawDataSet[idxL].pwrPdg[i],
                               pRawDataSet[idxL].vpdPdg[i],
			       AR5416_PD_GAIN_ICEPTS, vpdTableI[i]);
        }
    } else {
        for (i = 0; i < numXpdGains; i++) {
            pVpdL = pRawDataSet[idxL].vpdPdg[i];
            pPwrL = pRawDataSet[idxL].pwrPdg[i];
            pVpdR = pRawDataSet[idxR].vpdPdg[i];
            pPwrR = pRawDataSet[idxR].pwrPdg[i];

            /* Start Vpd interpolation from the max of the minimum powers */
            minPwrT4[i] = AH_MAX(pPwrL[0], pPwrR[0]);

            /* End Vpd interpolation from the min of the max powers */
            maxPwrT4[i] = AH_MIN(pPwrL[AR5416_PD_GAIN_ICEPTS - 1], pPwrR[AR5416_PD_GAIN_ICEPTS - 1]);
            HALASSERT(maxPwrT4[i] > minPwrT4[i]);

            /* Fill pier Vpds */
            ar9285FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrL, pVpdL,
			       AR5416_PD_GAIN_ICEPTS, vpdTableL[i]);
            ar9285FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrR, pVpdR,
			       AR5416_PD_GAIN_ICEPTS, vpdTableR[i]);

            /* Interpolate the final vpd */
            for (j = 0; j <= (maxPwrT4[i] - minPwrT4[i]) / 2; j++) {
                vpdTableI[i][j] = (uint8_t)(interpolate((uint16_t)FREQ2FBIN(centers.synth_center, IEEE80211_IS_CHAN_2GHZ(chan)),
                    bChans[idxL], bChans[idxR], vpdTableL[i][j], vpdTableR[i][j]));
            }
        }
    }
    *pMinCalPower = (int16_t)(minPwrT4[0] / 2);

    k = 0; /* index for the final table */
    for (i = 0; i < numXpdGains; i++) {
        if (i == (numXpdGains - 1)) {
            pPdGainBoundaries[i] = (uint16_t)(maxPwrT4[i] / 2);
        } else {
            pPdGainBoundaries[i] = (uint16_t)((maxPwrT4[i] + minPwrT4[i+1]) / 4);
        }

        pPdGainBoundaries[i] = (uint16_t)AH_MIN(AR5416_MAX_RATE_POWER, pPdGainBoundaries[i]);

	/* NB: only applies to owl 1.0 */
        if ((i == 0) && !AR_SREV_OWL_20_OR_LATER(ah) ) {
	    /*
             * fix the gain delta, but get a delta that can be applied to min to
             * keep the upper power values accurate, don't think max needs to
             * be adjusted because should not be at that area of the table?
	     */
            minDelta = pPdGainBoundaries[0] - 23;
            pPdGainBoundaries[0] = 23;
        }
        else {
            minDelta = 0;
        }

        /* Find starting index for this pdGain */
        if (i == 0) {
            ss = 0; /* for the first pdGain, start from index 0 */
        } else {
	    /* need overlap entries extrapolated below. */
            ss = (int16_t)((pPdGainBoundaries[i-1] - (minPwrT4[i] / 2)) - tPdGainOverlap + 1 + minDelta);
        }
        vpdStep = (int16_t)(vpdTableI[i][1] - vpdTableI[i][0]);
        vpdStep = (int16_t)((vpdStep < 1) ? 1 : vpdStep);
        /*
         *-ve ss indicates need to extrapolate data below for this pdGain
         */
        while ((ss < 0) && (k < (AR5416_NUM_PDADC_VALUES - 1))) {
            tmpVal = (int16_t)(vpdTableI[i][0] + ss * vpdStep);
            pPDADCValues[k++] = (uint8_t)((tmpVal < 0) ? 0 : tmpVal);
            ss++;
        }

        sizeCurrVpdTable = (uint8_t)((maxPwrT4[i] - minPwrT4[i]) / 2 +1);
        tgtIndex = (uint8_t)(pPdGainBoundaries[i] + tPdGainOverlap - (minPwrT4[i] / 2));
        maxIndex = (tgtIndex < sizeCurrVpdTable) ? tgtIndex : sizeCurrVpdTable;

        while ((ss < maxIndex) && (k < (AR5416_NUM_PDADC_VALUES - 1))) {
            pPDADCValues[k++] = vpdTableI[i][ss++];
        }

        vpdStep = (int16_t)(vpdTableI[i][sizeCurrVpdTable - 1] - vpdTableI[i][sizeCurrVpdTable - 2]);
        vpdStep = (int16_t)((vpdStep < 1) ? 1 : vpdStep);
        /*
         * for last gain, pdGainBoundary == Pmax_t2, so will
         * have to extrapolate
         */
        if (tgtIndex > maxIndex) {  /* need to extrapolate above */
            while ((ss <= tgtIndex) && (k < (AR5416_NUM_PDADC_VALUES - 1))) {
                tmpVal = (int16_t)((vpdTableI[i][sizeCurrVpdTable - 1] +
                          (ss - maxIndex +1) * vpdStep));
                pPDADCValues[k++] = (uint8_t)((tmpVal > 255) ? 255 : tmpVal);
                ss++;
            }
        }               /* extrapolated above */
    }                   /* for all pdGainUsed */

    /* Fill out pdGainBoundaries - only up to 2 allowed here, but hardware allows up to 4 */
    while (i < AR5416_PD_GAINS_IN_MASK) {
        pPdGainBoundaries[i] = pPdGainBoundaries[i-1];
        i++;
    }

    while (k < AR5416_NUM_PDADC_VALUES) {
        pPDADCValues[k] = pPDADCValues[k-1];
        k++;
    }
    return;
}
/*
 * XXX same as ar5416FillVpdTable
 */
static HAL_BOOL
ar9285FillVpdTable(uint8_t pwrMin, uint8_t pwrMax, uint8_t *pPwrList,
                   uint8_t *pVpdList, uint16_t numIntercepts, uint8_t *pRetVpdList)
{
    uint16_t  i, k;
    uint8_t   currPwr = pwrMin;
    uint16_t  idxL, idxR;

    HALASSERT(pwrMax > pwrMin);
    for (i = 0; i <= (pwrMax - pwrMin) / 2; i++) {
        getLowerUpperIndex(currPwr, pPwrList, numIntercepts,
                           &(idxL), &(idxR));
        if (idxR < 1)
            idxR = 1;           /* extrapolate below */
        if (idxL == numIntercepts - 1)
            idxL = (uint16_t)(numIntercepts - 2);   /* extrapolate above */
        if (pPwrList[idxL] == pPwrList[idxR])
            k = pVpdList[idxL];
        else
            k = (uint16_t)( ((currPwr - pPwrList[idxL]) * pVpdList[idxR] + (pPwrList[idxR] - currPwr) * pVpdList[idxL]) /
                  (pPwrList[idxR] - pPwrList[idxL]) );
        HALASSERT(k < 256);
        pRetVpdList[i] = (uint8_t)k;
        currPwr += 2;               /* half dB steps */
    }

    return AH_TRUE;
}
static int16_t
interpolate(uint16_t target, uint16_t srcLeft, uint16_t srcRight,
            int16_t targetLeft, int16_t targetRight)
{
    int16_t rv;

    if (srcRight == srcLeft) {
        rv = targetLeft;
    } else {
        rv = (int16_t)( ((target - srcLeft) * targetRight +
              (srcRight - target) * targetLeft) / (srcRight - srcLeft) );
    }
    return rv;
}

HAL_BOOL
getLowerUpperIndex(uint8_t target, uint8_t *pList, uint16_t listSize,
                   uint16_t *indexL, uint16_t *indexR)
{
    uint16_t i;

    /*
     * Check first and last elements for beyond ordered array cases.
     */
    if (target <= pList[0]) {
        *indexL = *indexR = 0;
        return AH_TRUE;
    }
    if (target >= pList[listSize-1]) {
        *indexL = *indexR = (uint16_t)(listSize - 1);
        return AH_TRUE;
    }

    /* look for value being near or between 2 values in list */
    for (i = 0; i < listSize - 1; i++) {
        /*
         * If value is close to the current value of the list
         * then target is not between values, it is one of the values
         */
        if (pList[i] == target) {
            *indexL = *indexR = i;
            return AH_TRUE;
        }
        /*
         * Look for value being between current value and next value
         * if so return these 2 values
         */
        if (target < pList[i + 1]) {
            *indexL = i;
            *indexR = (uint16_t)(i + 1);
            return AH_FALSE;
        }
    }
    HALASSERT(0);
    *indexL = *indexR = 0;
    return AH_FALSE;
}
