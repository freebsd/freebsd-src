/*
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2004 Atheros Communications, Inc.
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
 * $Id: ar5210_attach.c,v 1.7 2008/11/10 04:08:02 sam Exp $
 */
#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5210

#include "ah.h"
#include "ah_internal.h"

#include "ar5210/ar5210.h"
#include "ar5210/ar5210reg.h"
#include "ar5210/ar5210phy.h"

static	HAL_BOOL ar5210GetChannelEdges(struct ath_hal *,
		uint16_t flags, uint16_t *low, uint16_t *high);
static	HAL_BOOL ar5210GetChipPowerLimits(struct ath_hal *ah,
		HAL_CHANNEL *chans, uint32_t nchans);

static const struct ath_hal_private ar5210hal = {{
	.ah_magic			= AR5210_MAGIC,
	.ah_abi				= HAL_ABI_VERSION,
	.ah_countryCode			= CTRY_DEFAULT,

	.ah_getRateTable		= ar5210GetRateTable,
	.ah_detach			= ar5210Detach,

	/* Reset Functions */
	.ah_reset			= ar5210Reset,
	.ah_phyDisable			= ar5210PhyDisable,
	.ah_disable			= ar5210Disable,
	.ah_setPCUConfig		= ar5210SetPCUConfig,
	.ah_perCalibration		= ar5210PerCalibration,
	.ah_setTxPowerLimit		= ar5210SetTxPowerLimit,
	.ah_getChanNoise		= ath_hal_getChanNoise,

	/* Transmit functions */
	.ah_updateTxTrigLevel		= ar5210UpdateTxTrigLevel,
	.ah_setupTxQueue		= ar5210SetupTxQueue,
	.ah_setTxQueueProps             = ar5210SetTxQueueProps,
	.ah_getTxQueueProps             = ar5210GetTxQueueProps,
	.ah_releaseTxQueue		= ar5210ReleaseTxQueue,
	.ah_resetTxQueue		= ar5210ResetTxQueue,
	.ah_getTxDP			= ar5210GetTxDP,
	.ah_setTxDP			= ar5210SetTxDP,
	.ah_numTxPending		= ar5210NumTxPending,
	.ah_startTxDma			= ar5210StartTxDma,
	.ah_stopTxDma			= ar5210StopTxDma,
	.ah_setupTxDesc			= ar5210SetupTxDesc,
	.ah_setupXTxDesc		= ar5210SetupXTxDesc,
	.ah_fillTxDesc			= ar5210FillTxDesc,
	.ah_procTxDesc			= ar5210ProcTxDesc,
	.ah_getTxIntrQueue		= ar5210GetTxIntrQueue,
	.ah_reqTxIntrDesc 		= ar5210IntrReqTxDesc,

	/* RX Functions */
	.ah_getRxDP			= ar5210GetRxDP,
	.ah_setRxDP			= ar5210SetRxDP,
	.ah_enableReceive		= ar5210EnableReceive,
	.ah_stopDmaReceive		= ar5210StopDmaReceive,
	.ah_startPcuReceive		= ar5210StartPcuReceive,
	.ah_stopPcuReceive		= ar5210StopPcuReceive,
	.ah_setMulticastFilter		= ar5210SetMulticastFilter,
	.ah_setMulticastFilterIndex	= ar5210SetMulticastFilterIndex,
	.ah_clrMulticastFilterIndex	= ar5210ClrMulticastFilterIndex,
	.ah_getRxFilter			= ar5210GetRxFilter,
	.ah_setRxFilter			= ar5210SetRxFilter,
	.ah_setupRxDesc			= ar5210SetupRxDesc,
	.ah_procRxDesc			= ar5210ProcRxDesc,
	.ah_rxMonitor			= ar5210AniPoll,
	.ah_procMibEvent		= ar5210MibEvent,

	/* Misc Functions */
	.ah_getCapability		= ar5210GetCapability,
	.ah_setCapability		= ar5210SetCapability,
	.ah_getDiagState		= ar5210GetDiagState,
	.ah_getMacAddress		= ar5210GetMacAddress,
	.ah_setMacAddress		= ar5210SetMacAddress,
	.ah_getBssIdMask		= ar5210GetBssIdMask,
	.ah_setBssIdMask		= ar5210SetBssIdMask,
	.ah_setLedState			= ar5210SetLedState,
	.ah_writeAssocid		= ar5210WriteAssocid,
	.ah_gpioCfgInput		= ar5210GpioCfgInput,
	.ah_gpioCfgOutput		= ar5210GpioCfgOutput,
	.ah_gpioGet			= ar5210GpioGet,
	.ah_gpioSet			= ar5210GpioSet,
	.ah_gpioSetIntr			= ar5210Gpio0SetIntr,
	.ah_getTsf32			= ar5210GetTsf32,
	.ah_getTsf64			= ar5210GetTsf64,
	.ah_resetTsf			= ar5210ResetTsf,
	.ah_detectCardPresent		= ar5210DetectCardPresent,
	.ah_updateMibCounters		= ar5210UpdateMibCounters,
	.ah_getRfGain			= ar5210GetRfgain,
	.ah_getDefAntenna		= ar5210GetDefAntenna,
	.ah_setDefAntenna		= ar5210SetDefAntenna,
	.ah_getAntennaSwitch		= ar5210GetAntennaSwitch,
	.ah_setAntennaSwitch		= ar5210SetAntennaSwitch,
	.ah_setSifsTime			= ar5210SetSifsTime,
	.ah_getSifsTime			= ar5210GetSifsTime,
	.ah_setSlotTime			= ar5210SetSlotTime,
	.ah_getSlotTime			= ar5210GetSlotTime,
	.ah_setAckTimeout		= ar5210SetAckTimeout,
	.ah_getAckTimeout		= ar5210GetAckTimeout,
	.ah_setAckCTSRate		= ar5210SetAckCTSRate,
	.ah_getAckCTSRate		= ar5210GetAckCTSRate,
	.ah_setCTSTimeout		= ar5210SetCTSTimeout,
	.ah_getCTSTimeout		= ar5210GetCTSTimeout,
	.ah_setDecompMask               = ar5210SetDecompMask,
	.ah_setCoverageClass            = ar5210SetCoverageClass,

	/* Key Cache Functions */
	.ah_getKeyCacheSize		= ar5210GetKeyCacheSize,
	.ah_resetKeyCacheEntry		= ar5210ResetKeyCacheEntry,
	.ah_isKeyCacheEntryValid	= ar5210IsKeyCacheEntryValid,
	.ah_setKeyCacheEntry		= ar5210SetKeyCacheEntry,
	.ah_setKeyCacheEntryMac		= ar5210SetKeyCacheEntryMac,

	/* Power Management Functions */
	.ah_setPowerMode		= ar5210SetPowerMode,
	.ah_getPowerMode		= ar5210GetPowerMode,

	/* Beacon Functions */
	.ah_setBeaconTimers		= ar5210SetBeaconTimers,
	.ah_beaconInit			= ar5210BeaconInit,
	.ah_setStationBeaconTimers	= ar5210SetStaBeaconTimers,
	.ah_resetStationBeaconTimers	= ar5210ResetStaBeaconTimers,

	/* Interrupt Functions */
	.ah_isInterruptPending		= ar5210IsInterruptPending,
	.ah_getPendingInterrupts	= ar5210GetPendingInterrupts,
	.ah_getInterrupts		= ar5210GetInterrupts,
	.ah_setInterrupts		= ar5210SetInterrupts },

	.ah_getChannelEdges		= ar5210GetChannelEdges,
	.ah_getWirelessModes		= ar5210GetWirelessModes,
	.ah_eepromRead			= ar5210EepromRead,
#ifdef AH_SUPPORT_WRITE_EEPROM
	.ah_eepromWrite			= ar5210EepromWrite,
#endif
	.ah_gpioCfgInput		= ar5210GpioCfgInput,
	.ah_gpioCfgOutput		= ar5210GpioCfgOutput,
	.ah_gpioGet			= ar5210GpioGet,
	.ah_gpioSet			= ar5210GpioSet,
	.ah_gpioSetIntr			= ar5210Gpio0SetIntr,
	.ah_getChipPowerLimits		= ar5210GetChipPowerLimits,
};

static HAL_BOOL ar5210FillCapabilityInfo(struct ath_hal *ah);

/*
 * Attach for an AR5210 part.
 */
struct ath_hal *
ar5210Attach(uint16_t devid, HAL_SOFTC sc, HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_STATUS *status)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ath_hal_5210 *ahp;
	struct ath_hal *ah;
	u_int i, loc;
	uint32_t revid, pcicfg, sum;
	uint16_t athvals[AR_EEPROM_ATHEROS_MAX], eeval;
	HAL_STATUS ecode;

	HALDEBUG(AH_NULL, HAL_DEBUG_ATTACH,
	    "%s: devid 0x%x sc %p st %p sh %p\n", __func__, devid,
	    sc, (void*) st, (void*) sh);

	/* NB: memory is returned zero'd */
	ahp = ath_hal_malloc(sizeof (struct ath_hal_5210));
	if (ahp == AH_NULL) {
		HALDEBUG(AH_NULL, HAL_DEBUG_ANY,
		    "%s: no memory for state block\n", __func__);
		ecode = HAL_ENOMEM;
		goto bad;
	}
	ah = &ahp->ah_priv.h;
	/* set initial values */
	OS_MEMCPY(&ahp->ah_priv, &ar5210hal, sizeof(struct ath_hal_private));
	ah->ah_sc = sc;
	ah->ah_st = st;
	ah->ah_sh = sh;

	ah->ah_devid = devid;			/* NB: for AH_DEBUG_ALQ */
	AH_PRIVATE(ah)->ah_devid = devid;
	AH_PRIVATE(ah)->ah_subvendorid = 0;	/* XXX */

	AH_PRIVATE(ah)->ah_powerLimit = MAX_RATE_POWER;
	AH_PRIVATE(ah)->ah_tpScale = HAL_TP_SCALE_MAX;	/* no scaling */

	ahp->ah_powerMode = HAL_PM_UNDEFINED;
	ahp->ah_staId1Defaults = 0;
	ahp->ah_rssiThr = INIT_RSSI_THR;
	ahp->ah_sifstime = (u_int) -1;
	ahp->ah_slottime = (u_int) -1;
	ahp->ah_acktimeout = (u_int) -1;
	ahp->ah_ctstimeout = (u_int) -1;

	if (!ar5210ChipReset(ah, AH_NULL)) {	/* reset chip */
		HALDEBUG(ah, HAL_DEBUG_ANY, "%s: chip reset failed\n",
		    __func__);
		ecode = HAL_EIO;
		goto bad;
	}

	/* Read Revisions from Chips */
	AH_PRIVATE(ah)->ah_macVersion = 1;
	AH_PRIVATE(ah)->ah_macRev = OS_REG_READ(ah, AR_SREV) & 0xff;
	AH_PRIVATE(ah)->ah_phyRev = OS_REG_READ(ah, AR_PHY_CHIPID);
	AH_PRIVATE(ah)->ah_analog2GhzRev = 0;

	/* Read Radio Chip Rev Extract */
	OS_REG_WRITE(ah, (AR_PHY_BASE + (0x34 << 2)), 0x00001c16);
	for (i = 0; i < 4; i++)
		OS_REG_WRITE(ah, (AR_PHY_BASE + (0x20 << 2)), 0x00010000);
	revid = (OS_REG_READ(ah, AR_PHY_BASE + (256 << 2)) >> 28) & 0xf;

	/* Chip labelling is 1 greater than revision register for AR5110 */
	AH_PRIVATE(ah)->ah_analog5GhzRev = ath_hal_reverseBits(revid, 4) + 1;

	/*
	 * Read all the settings from the EEPROM and stash
	 * ones we'll use later in our state block.
	 */
	pcicfg = OS_REG_READ(ah, AR_PCICFG);
	OS_REG_WRITE(ah, AR_PCICFG, pcicfg | AR_PCICFG_EEPROMSEL);

	if (!ar5210EepromRead(ah, AR_EEPROM_MAGIC, &eeval)) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: cannot read EEPROM magic number\n", __func__);
		ecode = HAL_EEREAD;
		goto eebad;
	}
	if (eeval != 0x5aa5) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: invalid EEPROM magic number 0x%x\n", __func__, eeval);
		ecode = HAL_EEMAGIC;
		goto eebad;
	}

	if (!ar5210EepromRead(ah, AR_EEPROM_PROTECT, &eeval)) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: cannot read EEPROM protection bits; read locked?\n",
		    __func__);
		ecode = HAL_EEREAD;
		goto eebad;
	}
	HALDEBUG(ah, HAL_DEBUG_ATTACH, "EEPROM protect 0x%x\n", eeval);
	ahp->ah_eeprotect	= eeval;
	/* XXX check proper access before continuing */

	if (!ar5210EepromRead(ah, AR_EEPROM_VERSION, &eeval)) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: unable to read EEPROM version\n", __func__);
		ecode = HAL_EEREAD;
		goto eebad;
	}
	ahp->ah_eeversion = (eeval>>12) & 0xf;
	if (ahp->ah_eeversion != 1) {
		/*
		 * This driver only groks the version 1 EEPROM layout.
		 */
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: unsupported EEPROM version %u (0x%x) found\n",
		    __func__, ahp->ah_eeversion, eeval);
		ecode = HAL_EEVERSION;
		goto eebad;
	}

	/*
	 * Read the Atheros EEPROM entries and calculate the checksum.
	 */
	sum = 0;
	for (i = 0; i < AR_EEPROM_ATHEROS_MAX; i++) {
		if (!ar5210EepromRead(ah, AR_EEPROM_ATHEROS(i), &athvals[i])) {
			ecode = HAL_EEREAD;
			goto eebad;
		}
		sum ^= athvals[i];
	}
	if (sum != 0xffff) {
		HALDEBUG(ah, HAL_DEBUG_ANY, "%s: bad EEPROM checksum 0x%x\n",
		    __func__, sum);
		ecode = HAL_EEBADSUM;
		goto eebad;
	}

	/*
	 * Valid checksum, fetch the regulatory domain and save values.
	 */
	if (!ar5210EepromRead(ah, AR_EEPROM_REG_DOMAIN, &eeval)) {
		HALDEBUG(ah, HAL_DEBUG_ANY,
		    "%s: cannot read regdomain from EEPROM\n", __func__);
		ecode = HAL_EEREAD;
		goto eebad;
	}

	AH_PRIVATE(ah)->ah_currentRD = eeval & 0xff;
	ahp->ah_antenna		= athvals[2];
	ahp->ah_biasCurrents	= athvals[3];
	ahp->ah_thresh62	= athvals[4] & 0xff;
	ahp->ah_xlnaOn		= (athvals[4] >> 8) & 0xff;
	ahp->ah_xpaOn		= athvals[5] & 0xff;
	ahp->ah_xpaOff		= (athvals[5] >> 8) & 0xff;
	ahp->ah_regDomain[0]	= (athvals[6] >> 8) & 0xff;
	ahp->ah_regDomain[1]	= athvals[6] & 0xff;
	ahp->ah_regDomain[2]	= (athvals[7] >> 8) & 0xff;
	ahp->ah_regDomain[3]	= athvals[7] & 0xff;
	ahp->ah_rfKill		= athvals[8] & 0x1;
	ahp->ah_devType		= (athvals[8] >> 1) & 0x7;
	AH_PRIVATE(ah)->ah_getNfAdjust = ar5210GetNfAdjust;

	for (i = 0, loc = AR_EEPROM_ATHEROS_TP_SETTINGS; i < AR_CHANNELS_MAX; i++, loc += AR_TP_SETTINGS_SIZE) {
		struct tpcMap *chan = &ahp->ah_tpc[i];

		/* Copy pcdac and gain_f values from EEPROM */
		chan->pcdac[0]	= (athvals[loc] >> 10) & 0x3F;
		chan->gainF[0]	= (athvals[loc] >> 4) & 0x3F;
		chan->pcdac[1]	= ((athvals[loc] << 2) & 0x3C)
				| ((athvals[loc+1] >> 14) & 0x03);
		chan->gainF[1]	= (athvals[loc+1] >> 8) & 0x3F;
		chan->pcdac[2]	= (athvals[loc+1] >> 2) & 0x3F;
		chan->gainF[2]	= ((athvals[loc+1] << 4) & 0x30)
				| ((athvals[loc+2] >> 12) & 0x0F);
		chan->pcdac[3]	= (athvals[loc+2] >> 6) & 0x3F;
		chan->gainF[3]	= athvals[loc+2] & 0x3F;
		chan->pcdac[4]	= (athvals[loc+3] >> 10) & 0x3F;
		chan->gainF[4]	= (athvals[loc+3] >> 4) & 0x3F;
		chan->pcdac[5]	= ((athvals[loc+3] << 2) & 0x3C)
				| ((athvals[loc+4] >> 14) & 0x03);
		chan->gainF[5]	= (athvals[loc+4] >> 8) & 0x3F;
		chan->pcdac[6]	= (athvals[loc+4] >> 2) & 0x3F;
		chan->gainF[6]	= ((athvals[loc+4] << 4) & 0x30)
				| ((athvals[loc+5] >> 12) & 0x0F);
		chan->pcdac[7]	= (athvals[loc+5] >> 6) & 0x3F;
		chan->gainF[7]	= athvals[loc+5] & 0x3F;
		chan->pcdac[8]	= (athvals[loc+6] >> 10) & 0x3F;
		chan->gainF[8]	= (athvals[loc+6] >> 4) & 0x3F;
		chan->pcdac[9]	= ((athvals[loc+6] << 2) & 0x3C)
				| ((athvals[loc+7] >> 14) & 0x03);
		chan->gainF[9]	= (athvals[loc+7] >> 8) & 0x3F;
		chan->pcdac[10]	= (athvals[loc+7] >> 2) & 0x3F;
		chan->gainF[10]	= ((athvals[loc+7] << 4) & 0x30)
				| ((athvals[loc+8] >> 12) & 0x0F);

		/* Copy Regulatory Domain and Rate Information from EEPROM */
		chan->rate36	= (athvals[loc+8] >> 6) & 0x3F;
		chan->rate48	= athvals[loc+8] & 0x3F;
		chan->rate54	= (athvals[loc+9] >> 10) & 0x3F;
		chan->regdmn[0]	= (athvals[loc+9] >> 4) & 0x3F;
		chan->regdmn[1]	= ((athvals[loc+9] << 2) & 0x3C)
				| ((athvals[loc+10] >> 14) & 0x03);
		chan->regdmn[2]	= (athvals[loc+10] >> 8) & 0x3F;
		chan->regdmn[3]	= (athvals[loc+10] >> 2) & 0x3F;
	}
	/*
	 * Got everything we need now to setup the capabilities.
	 */
	(void) ar5210FillCapabilityInfo(ah);

	sum = 0;
	for (i = 0; i < 3; i++) {
		if (!ar5210EepromRead(ah, AR_EEPROM_MAC(i), &eeval)) {
			HALDEBUG(ah, HAL_DEBUG_ANY,
			    "%s: cannot read EEPROM location %u\n", __func__, i);
			ecode = HAL_EEREAD;
			goto bad;
		}
		sum += eeval;
		ahp->ah_macaddr[2*i + 0] = eeval >> 8;
		ahp->ah_macaddr[2*i + 1] = eeval & 0xff;
	}
	if (sum == 0 || sum == 0xffff*3) {
		HALDEBUG(ah, HAL_DEBUG_ANY, "%s: mac address read failed: %s\n",
		    __func__, ath_hal_ether_sprintf(ahp->ah_macaddr));
		ecode = HAL_EEBADMAC;
		goto eebad;
	}

	OS_REG_WRITE(ah, AR_PCICFG, pcicfg);	/* disable EEPROM access */

	HALDEBUG(ah, HAL_DEBUG_ATTACH, "%s: return\n", __func__);

	return ah;

eebad:
	OS_REG_WRITE(ah, AR_PCICFG, pcicfg);	/* disable EEPROM access */
bad:
	if (ahp)
		ath_hal_free(ahp);
	if (status)
		*status = ecode;
	return AH_NULL;
#undef N
}

void
ar5210Detach(struct ath_hal *ah)
{
	HALDEBUG(ah, HAL_DEBUG_ATTACH, "%s:\n", __func__);

	HALASSERT(ah != AH_NULL);
	HALASSERT(ah->ah_magic == AR5210_MAGIC);

	ath_hal_free(ah);
}

/*
 * Store the channel edges for the requested operational mode
 */
static HAL_BOOL
ar5210GetChannelEdges(struct ath_hal *ah,
	uint16_t flags, uint16_t *low, uint16_t *high)
{
	if (flags & CHANNEL_5GHZ) {
		*low = 5120;
		*high = 5430;
		return AH_TRUE;
	} else {
		return AH_FALSE;
	}
}

static HAL_BOOL
ar5210GetChipPowerLimits(struct ath_hal *ah, HAL_CHANNEL *chans, uint32_t nchans)
{
	HAL_CHANNEL *chan;
	int i;

	/* XXX fill in, this is just a placeholder */
	for (i = 0; i < nchans; i++) {
		chan = &chans[i];
		HALDEBUG(ah, HAL_DEBUG_ATTACH,
		    "%s: no min/max power for %u/0x%x\n",
		    __func__, chan->channel, chan->channelFlags);
		chan->maxTxPower = MAX_RATE_POWER;
		chan->minTxPower = 0;
	}
	return AH_TRUE;
}

/*
 * Fill all software cached or static hardware state information.
 */
static HAL_BOOL
ar5210FillCapabilityInfo(struct ath_hal *ah)
{
	struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
	HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;

	pCap->halWirelessModes |= HAL_MODE_11A;

	pCap->halLow5GhzChan = 5120;
	pCap->halHigh5GhzChan = 5430;

	pCap->halSleepAfterBeaconBroken = AH_TRUE;
	pCap->halPSPollBroken = AH_FALSE;

	pCap->halTotalQueues = HAL_NUM_TX_QUEUES;
	pCap->halKeyCacheSize = 64;

	/* XXX not needed */
	pCap->halChanHalfRate = AH_FALSE;
	pCap->halChanQuarterRate = AH_FALSE;

	if (AH5210(ah)->ah_rfKill) {
		/*
		 * Setup initial rfsilent settings based on the EEPROM
		 * contents.  Pin 0, polarity 0 is fixed; record this
		 * using the EEPROM format found in later parts.
		 */
		ahpriv->ah_rfsilent = SM(0, AR_EEPROM_RFSILENT_GPIO_SEL)
				    | SM(0, AR_EEPROM_RFSILENT_POLARITY);
		ahpriv->ah_rfkillEnabled = AH_TRUE;
		pCap->halRfSilentSupport = AH_TRUE;
	}

	pCap->halTstampPrecision = 15;		/* NB: s/w extended from 13 */

	ahpriv->ah_rxornIsFatal = AH_TRUE;
	return AH_TRUE;
}
#endif /* AH_SUPPORT_AR5210 */
