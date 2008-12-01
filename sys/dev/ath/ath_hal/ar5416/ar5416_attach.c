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
 * $Id: ar5416_attach.c,v 1.27 2008/11/27 22:30:07 sam Exp $
 */
#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

#include "ar5416/ar5416.ini"

static void
ar5416AniSetup(struct ath_hal *ah)
{
	static const struct ar5212AniParams aniparams = {
		.maxNoiseImmunityLevel	= 4,	/* levels 0..4 */
		.totalSizeDesired	= { -55, -55, -55, -55, -62 },
		.coarseHigh		= { -14, -14, -14, -14, -12 },
		.coarseLow		= { -64, -64, -64, -64, -70 },
		.firpwr			= { -78, -78, -78, -78, -80 },
		.maxSpurImmunityLevel	= 2,
		.cycPwrThr1		= { 2, 4, 6 },
		.maxFirstepLevel	= 2,	/* levels 0..2 */
		.firstep		= { 0, 4, 8 },
		.ofdmTrigHigh		= 500,
		.ofdmTrigLow		= 200,
		.cckTrigHigh		= 200,
		.cckTrigLow		= 100,
		.rssiThrHigh		= 40,
		.rssiThrLow		= 7,
		.period			= 100,
	};
	/* NB: ANI is not enabled yet */
	ar5212AniAttach(ah, &aniparams, &aniparams, AH_FALSE);
}

/*
 * Attach for an AR5416 part.
 */
void
ar5416InitState(struct ath_hal_5416 *ahp5416, uint16_t devid, HAL_SOFTC sc,
	HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_STATUS *status)
{
	struct ath_hal_5212 *ahp;
	struct ath_hal *ah;

	ahp = &ahp5416->ah_5212;
	ar5212InitState(ahp, devid, sc, st, sh, status);
	ah = &ahp->ah_priv.h;

	/* override 5212 methods for our needs */
	ah->ah_magic			= AR5416_MAGIC;
	ah->ah_getRateTable		= ar5416GetRateTable;
	ah->ah_detach			= ar5416Detach;

	/* Reset functions */
	ah->ah_reset			= ar5416Reset;
	ah->ah_phyDisable		= ar5416PhyDisable;
	ah->ah_disable			= ar5416Disable;
	ah->ah_perCalibration		= ar5416PerCalibration;
	ah->ah_perCalibrationN		= ar5416PerCalibrationN,
	ah->ah_resetCalValid		= ar5416ResetCalValid,
	ah->ah_setTxPowerLimit		= ar5416SetTxPowerLimit;

	/* Transmit functions */
	ah->ah_stopTxDma		= ar5416StopTxDma;
	ah->ah_setupTxDesc		= ar5416SetupTxDesc;
	ah->ah_setupXTxDesc		= ar5416SetupXTxDesc;
	ah->ah_fillTxDesc		= ar5416FillTxDesc;
	ah->ah_procTxDesc		= ar5416ProcTxDesc;

	/* Receive Functions */
	ah->ah_startPcuReceive		= ar5416StartPcuReceive;
	ah->ah_stopPcuReceive		= ar5416StopPcuReceive;
	ah->ah_setupRxDesc		= ar5416SetupRxDesc;
	ah->ah_procRxDesc		= ar5416ProcRxDesc;
	ah->ah_rxMonitor		= ar5416AniPoll,
	ah->ah_procMibEvent		= ar5416ProcessMibIntr,

	/* Misc Functions */
	ah->ah_getDiagState		= ar5416GetDiagState;
	ah->ah_setLedState		= ar5416SetLedState;
	ah->ah_gpioCfgOutput		= ar5416GpioCfgOutput;
	ah->ah_gpioCfgInput		= ar5416GpioCfgInput;
	ah->ah_gpioGet			= ar5416GpioGet;
	ah->ah_gpioSet			= ar5416GpioSet;
	ah->ah_gpioSetIntr		= ar5416GpioSetIntr;
	ah->ah_resetTsf			= ar5416ResetTsf;
	ah->ah_getRfGain		= ar5416GetRfgain;
	ah->ah_setAntennaSwitch		= ar5416SetAntennaSwitch;
	ah->ah_setDecompMask		= ar5416SetDecompMask;
	ah->ah_setCoverageClass		= ar5416SetCoverageClass;

	ah->ah_resetKeyCacheEntry	= ar5416ResetKeyCacheEntry;
	ah->ah_setKeyCacheEntry		= ar5416SetKeyCacheEntry;

	/* Power Management Functions */
	ah->ah_setPowerMode		= ar5416SetPowerMode;

	/* Beacon Management Functions */
	ah->ah_setBeaconTimers		= ar5416SetBeaconTimers;
	ah->ah_beaconInit		= ar5416BeaconInit;
	ah->ah_setStationBeaconTimers	= ar5416SetStaBeaconTimers;
	ah->ah_resetStationBeaconTimers	= ar5416ResetStaBeaconTimers;

	/* XXX 802.11n Functions */
#if 0
	ah->ah_chainTxDesc		= ar5416ChainTxDesc;
	ah->ah_setupFirstTxDesc		= ar5416SetupFirstTxDesc;
	ah->ah_setupLastTxDesc		= ar5416SetupLastTxDesc;
	ah->ah_set11nRateScenario	= ar5416Set11nRateScenario;
	ah->ah_set11nAggrMiddle		= ar5416Set11nAggrMiddle;
	ah->ah_clr11nAggr		= ar5416Clr11nAggr;
	ah->ah_set11nBurstDuration	= ar5416Set11nBurstDuration;
	ah->ah_get11nExtBusy		= ar5416Get11nExtBusy;
	ah->ah_set11nMac2040		= ar5416Set11nMac2040;
	ah->ah_get11nRxClear		= ar5416Get11nRxClear;
	ah->ah_set11nRxClear		= ar5416Set11nRxClear;
#endif

	/* Interrupt functions */
	ah->ah_isInterruptPending	= ar5416IsInterruptPending;
	ah->ah_getPendingInterrupts	= ar5416GetPendingInterrupts;
	ah->ah_setInterrupts		= ar5416SetInterrupts;

	ahp->ah_priv.ah_getWirelessModes= ar5416GetWirelessModes;
	ahp->ah_priv.ah_eepromRead	= ar5416EepromRead;
#ifdef AH_SUPPORT_WRITE_EEPROM
	ahp->ah_priv.ah_eepromWrite	= ar5416EepromWrite;
#endif
	ahp->ah_priv.ah_gpioCfgOutput	= ar5416GpioCfgOutput;
	ahp->ah_priv.ah_gpioCfgInput	= ar5416GpioCfgInput;
	ahp->ah_priv.ah_gpioGet		= ar5416GpioGet;
	ahp->ah_priv.ah_gpioSet		= ar5416GpioSet;
	ahp->ah_priv.ah_gpioSetIntr	= ar5416GpioSetIntr;
	ahp->ah_priv.ah_getChipPowerLimits = ar5416GetChipPowerLimits;

	/*
	 * Start by setting all Owl devices to 2x2
	 */
	AH5416(ah)->ah_rx_chainmask = AR5416_DEFAULT_RXCHAINMASK;
	AH5416(ah)->ah_tx_chainmask = AR5416_DEFAULT_TXCHAINMASK;
}

/*
 * Attach for an AR5416 part.
 */
struct ath_hal *
ar5416Attach(uint16_t devid, HAL_SOFTC sc,
	HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_STATUS *status)
{
	struct ath_hal_5416 *ahp5416;
	struct ath_hal_5212 *ahp;
	struct ath_hal *ah;
	uint32_t val;
	HAL_STATUS ecode;
	HAL_BOOL rfStatus;

	HALDEBUG(AH_NULL, HAL_DEBUG_ATTACH, "%s: sc %p st %p sh %p\n",
	    __func__, sc, (void*) st, (void*) sh);

	/* NB: memory is returned zero'd */
	ahp5416 = ath_hal_malloc(sizeof (struct ath_hal_5416) +
		/* extra space for Owl 2.1/2.2 WAR */
		sizeof(ar5416Addac)
	);
	if (ahp5416 == AH_NULL) {
		HALDEBUG(AH_NULL, HAL_DEBUG_ANY,
		    "%s: cannot allocate memory for state block\n", __func__);
		*status = HAL_ENOMEM;
		return AH_NULL;
	}
	ar5416InitState(ahp5416, devid, sc, st, sh, status);
	ahp = &ahp5416->ah_5212;
	ah = &ahp->ah_priv.h;

	if (!ar5416SetResetReg(ah, HAL_RESET_POWER_ON)) {
		/* reset chip */
		HALDEBUG(ah, HAL_DEBUG_ANY, "%s: couldn't reset chip\n", __func__);
		ecode = HAL_EIO;
		goto bad;
	}

	if (!ar5416SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE)) {
		HALDEBUG(ah, HAL_DEBUG_ANY, "%s: couldn't wakeup chip\n", __func__);
		ecode = HAL_EIO;
		goto bad;
	}
	/* Read Revisions from Chips before taking out of reset */
	val = OS_REG_READ(ah, AR_SREV) & AR_SREV_ID;
	AH_PRIVATE(ah)->ah_macVersion = val >> AR_SREV_ID_S;
	AH_PRIVATE(ah)->ah_macRev = val & AR_SREV_REVISION;

	/* setup common ini data; rf backends handle remainder */
	HAL_INI_INIT(&ahp->ah_ini_modes, ar5416Modes, 6);
	HAL_INI_INIT(&ahp->ah_ini_common, ar5416Common, 2);

	HAL_INI_INIT(&AH5416(ah)->ah_ini_bb_rfgain, ar5416BB_RfGain, 3);
	HAL_INI_INIT(&AH5416(ah)->ah_ini_bank0, ar5416Bank0, 2);
	HAL_INI_INIT(&AH5416(ah)->ah_ini_bank1, ar5416Bank1, 2);
	HAL_INI_INIT(&AH5416(ah)->ah_ini_bank2, ar5416Bank2, 2);
	HAL_INI_INIT(&AH5416(ah)->ah_ini_bank3, ar5416Bank3, 3);
	HAL_INI_INIT(&AH5416(ah)->ah_ini_bank6, ar5416Bank6, 3);
	HAL_INI_INIT(&AH5416(ah)->ah_ini_bank7, ar5416Bank7, 2);
	HAL_INI_INIT(&AH5416(ah)->ah_ini_addac, ar5416Addac, 2);

	if (!IS_5416V2_2(ah)) {		/* Owl 2.1/2.0 */
		struct ini {
			uint32_t	*data;		/* NB: !const */
			int		rows, cols;
		};
		/* override CLKDRV value */
		OS_MEMCPY(&AH5416(ah)[1], ar5416Addac, sizeof(ar5416Addac));
		AH5416(ah)->ah_ini_addac.data = (uint32_t *) &AH5416(ah)[1];
		HAL_INI_VAL((struct ini *)&AH5416(ah)->ah_ini_addac, 31, 1) = 0;
	}

	if (!ar5416ChipReset(ah, AH_NULL)) {	/* reset chip */
		HALDEBUG(ah, HAL_DEBUG_ANY, "%s: chip reset failed\n",
		    __func__);
		ecode = HAL_EIO;
		goto bad;
	}

	AH_PRIVATE(ah)->ah_phyRev = OS_REG_READ(ah, AR_PHY_CHIP_ID);

	if (!ar5212ChipTest(ah)) {
		HALDEBUG(ah, HAL_DEBUG_ANY, "%s: hardware self-test failed\n",
		    __func__);
		ecode = HAL_ESELFTEST;
		goto bad;
	}

	/*
	 * Set correct Baseband to analog shift
	 * setting to access analog chips.
	 */
	OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);

	/* Read Radio Chip Rev Extract */
	AH_PRIVATE(ah)->ah_analog5GhzRev = ar5212GetRadioRev(ah);
	switch (AH_PRIVATE(ah)->ah_analog5GhzRev & AR_RADIO_SREV_MAJOR) {
        case AR_RAD5122_SREV_MAJOR:	/* Fowl: 5G/2x2 */
        case AR_RAD2122_SREV_MAJOR:	/* Fowl: 2+5G/2x2 */
        case AR_RAD2133_SREV_MAJOR:	/* Fowl: 2G/3x3 */
	case AR_RAD5133_SREV_MAJOR:	/* Fowl: 2+5G/3x3 */
		break;
	default:
		if (AH_PRIVATE(ah)->ah_analog5GhzRev == 0) {
			/*
			 * When RF_Silen is used the analog chip is reset.
			 * So when the system boots with radio switch off
			 * the RF chip rev reads back as zero and we need
			 * to use the mac+phy revs to set the radio rev.
			 */
			AH_PRIVATE(ah)->ah_analog5GhzRev =
				AR_RAD5133_SREV_MAJOR;
			break;
		}
		/* NB: silently accept anything in release code per Atheros */
#ifdef AH_DEBUG
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: 5G Radio Chip Rev 0x%02X is not supported by "
		    "this driver\n", __func__,
		    AH_PRIVATE(ah)->ah_analog5GhzRev);
		ecode = HAL_ENOTSUPP;
		goto bad;
#endif
	}

	ecode = ath_hal_v14EepromAttach(ah);
	if (ecode != HAL_OK)
		goto bad;

	/*
	 * Got everything we need now to setup the capabilities.
	 */
	if (!ar5416FillCapabilityInfo(ah)) {
		ecode = HAL_EEREAD;
		goto bad;
	}

	ecode = ath_hal_eepromGet(ah, AR_EEP_MACADDR, ahp->ah_macaddr);
	if (ecode != HAL_OK) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: error getting mac address from EEPROM\n", __func__);
		goto bad;
        }
	/* XXX How about the serial number ? */
	/* Read Reg Domain */
	AH_PRIVATE(ah)->ah_currentRD =
	    ath_hal_eepromGet(ah, AR_EEP_REGDMN_0, AH_NULL);

	/*
	 * ah_miscMode is populated by ar5416FillCapabilityInfo()
	 * starting from griffin. Set here to make sure that
	 * AR_MISC_MODE_MIC_NEW_LOC_ENABLE is set before a GTK is
	 * placed into hardware.
	 */
	if (ahp->ah_miscMode != 0)
		OS_REG_WRITE(ah, AR_MISC_MODE, ahp->ah_miscMode);

	HALDEBUG(ah, HAL_DEBUG_ATTACH, "%s: Attaching AR2133 radio\n",
	    __func__);
	rfStatus = ar2133RfAttach(ah, &ecode);
	if (!rfStatus) {
		HALDEBUG(ah, HAL_DEBUG_ANY, "%s: RF setup failed, status %u\n",
		    __func__, ecode);
		goto bad;
	}

	ar5416AniSetup(ah);			/* Anti Noise Immunity */
	ar5416InitNfHistBuff(AH5416(ah)->ah_cal.nfCalHist);

	HALDEBUG(ah, HAL_DEBUG_ATTACH, "%s: return\n", __func__);

	return ah;
bad:
	if (ahp)
		ar5416Detach((struct ath_hal *) ahp);
	if (status)
		*status = ecode;
	return AH_NULL;
}

void
ar5416Detach(struct ath_hal *ah)
{
	HALDEBUG(ah, HAL_DEBUG_ATTACH, "%s:\n", __func__);

	HALASSERT(ah != AH_NULL);
	HALASSERT(ah->ah_magic == AR5416_MAGIC);

	ar5416AniDetach(ah);
	ar5212RfDetach(ah);
	ah->ah_disable(ah);
	ar5416SetPowerMode(ah, HAL_PM_FULL_SLEEP, AH_TRUE);
	ath_hal_eepromDetach(ah);
	ath_hal_free(ah);
}

/*
 * Fill all software cached or static hardware state information.
 * Return failure if capabilities are to come from EEPROM and
 * cannot be read.
 */
HAL_BOOL
ar5416FillCapabilityInfo(struct ath_hal *ah)
{
	struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
	HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;
	uint16_t val;
	
	/* Construct wireless mode from EEPROM */
	pCap->halWirelessModes = 0;
	if (ath_hal_eepromGetFlag(ah, AR_EEP_AMODE)) {
		pCap->halWirelessModes |= HAL_MODE_11A
				       |  HAL_MODE_11NA_HT20
				       |  HAL_MODE_11NA_HT40PLUS
				       |  HAL_MODE_11NA_HT40MINUS
				       ;
	}
	if (ath_hal_eepromGetFlag(ah, AR_EEP_GMODE)) {
		pCap->halWirelessModes |= HAL_MODE_11G
				       |  HAL_MODE_11NG_HT20
				       |  HAL_MODE_11NG_HT40PLUS
				       |  HAL_MODE_11NG_HT40MINUS
				       ;
		pCap->halWirelessModes |= HAL_MODE_11A
				       |  HAL_MODE_11NA_HT20
				       |  HAL_MODE_11NA_HT40PLUS
				       |  HAL_MODE_11NA_HT40MINUS
				       ;
	}

	pCap->halLow2GhzChan = 2312;
	pCap->halHigh2GhzChan = 2732;

	pCap->halLow5GhzChan = 4915;
	pCap->halHigh5GhzChan = 6100;

	pCap->halCipherCkipSupport = AH_FALSE;
	pCap->halCipherTkipSupport = AH_TRUE;
	pCap->halCipherAesCcmSupport = ath_hal_eepromGetFlag(ah, AR_EEP_AES);

	pCap->halMicCkipSupport    = AH_FALSE;
	pCap->halMicTkipSupport    = AH_TRUE;
	pCap->halMicAesCcmSupport  = ath_hal_eepromGetFlag(ah, AR_EEP_AES);
	/*
	 * Starting with Griffin TX+RX mic keys can be combined
	 * in one key cache slot.
	 */
	pCap->halTkipMicTxRxKeySupport = AH_TRUE;
	pCap->halChanSpreadSupport = AH_TRUE;
	pCap->halSleepAfterBeaconBroken = AH_TRUE;

	pCap->halCompressSupport = AH_FALSE;
	pCap->halBurstSupport = AH_TRUE;
	pCap->halFastFramesSupport = AH_FALSE;	/* XXX? */
	pCap->halChapTuningSupport = AH_TRUE;
	pCap->halTurboPrimeSupport = AH_TRUE;

	pCap->halTurboGSupport = pCap->halWirelessModes & HAL_MODE_108G;

	pCap->halPSPollBroken = AH_TRUE;	/* XXX fixed in later revs? */
	pCap->halVEOLSupport = AH_TRUE;
	pCap->halBssIdMaskSupport = AH_TRUE;
	pCap->halMcastKeySrchSupport = AH_FALSE;
	pCap->halTsfAddSupport = AH_TRUE;

	if (ath_hal_eepromGet(ah, AR_EEP_MAXQCU, &val) == HAL_OK)
		pCap->halTotalQueues = val;
	else
		pCap->halTotalQueues = HAL_NUM_TX_QUEUES;

	if (ath_hal_eepromGet(ah, AR_EEP_KCENTRIES, &val) == HAL_OK)
		pCap->halKeyCacheSize = val;
	else
		pCap->halKeyCacheSize = AR5416_KEYTABLE_SIZE;

	/* XXX not needed */
	pCap->halChanHalfRate = AH_FALSE;	/* XXX ? */
	pCap->halChanQuarterRate = AH_FALSE;	/* XXX ? */

	pCap->halTstampPrecision = 32;
	pCap->halHwPhyCounterSupport = AH_TRUE;

	pCap->halFastCCSupport = AH_TRUE;
	pCap->halNumGpioPins = 6;
	pCap->halWowSupport = AH_FALSE;
	pCap->halWowMatchPatternExact = AH_FALSE;
	pCap->halBtCoexSupport = AH_FALSE;	/* XXX need support */
	pCap->halAutoSleepSupport = AH_FALSE;
#if 0	/* XXX not yet */
	pCap->halNumAntCfg2GHz = ar5416GetNumAntConfig(ahp, HAL_FREQ_BAND_2GHZ);
	pCap->halNumAntCfg5GHz = ar5416GetNumAntConfig(ahp, HAL_FREQ_BAND_5GHZ);
#endif
	pCap->halHTSupport = AH_TRUE;
	pCap->halTxChainMask = ath_hal_eepromGet(ah, AR_EEP_TXMASK, AH_NULL);
	/* XXX CB71 uses GPIO 0 to indicate 3 rx chains */
	pCap->halRxChainMask = ath_hal_eepromGet(ah, AR_EEP_RXMASK, AH_NULL);
	pCap->halRtsAggrLimit = 8*1024;		/* Owl 2.0 limit */
	pCap->halMbssidAggrSupport = AH_TRUE;
	pCap->halForcePpmSupport = AH_TRUE;
	pCap->halEnhancedPmSupport = AH_TRUE;

	if (ath_hal_eepromGetFlag(ah, AR_EEP_RFKILL) &&
	    ath_hal_eepromGet(ah, AR_EEP_RFSILENT, &ahpriv->ah_rfsilent) == HAL_OK) {
		/* NB: enabled by default */
		ahpriv->ah_rfkillEnabled = AH_TRUE;
		pCap->halRfSilentSupport = AH_TRUE;
	}

	ahpriv->ah_rxornIsFatal = AH_FALSE;

	return AH_TRUE;
}

static const char*
ar5416Probe(uint16_t vendorid, uint16_t devid)
{
	if (vendorid == ATHEROS_VENDOR_ID &&
	    (devid == AR5416_DEVID_PCI || devid == AR5416_DEVID_PCIE))
		return "Atheros 5416";
	return AH_NULL;
}
AH_CHIP(AR5416, ar5416Probe, ar5416Attach);
