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
#ifndef _ATH_AR5416_H_
#define _ATH_AR5416_H_

#include "ar5212/ar5212.h"
#include "ar5416_cal.h"
#include "ah_eeprom_v14.h"	/* for CAL_TARGET_POWER_* */

#define	AR5416_MAGIC	0x20065416

enum {
	HAL_RESET_POWER_ON,
	HAL_RESET_WARM,
	HAL_RESET_COLD,
};

typedef struct {
	uint16_t	synth_center;
	uint16_t	ctl_center;
	uint16_t	ext_center;
} CHAN_CENTERS;

#define	AR5416_DEFAULT_RXCHAINMASK	7
#define	AR5416_DEFAULT_TXCHAINMASK	1
#define	AR5416_MAX_RATE_POWER		63
#define	AR5416_KEYTABLE_SIZE		128

#define	AR5416_CCA_MAX_GOOD_VALUE	-85
#define	AR5416_CCA_MAX_HIGH_VALUE	-62
#define	AR5416_CCA_MIN_BAD_VALUE	-140
#define	AR9285_CCA_MAX_GOOD_VALUE	-118

#define AR5416_SPUR_RSSI_THRESH		40

struct ath_hal_5416 {
	struct ath_hal_5212 ah_5212;

	/* NB: RF data setup at attach */
	HAL_INI_ARRAY	ah_ini_bb_rfgain;
	HAL_INI_ARRAY	ah_ini_bank0;
	HAL_INI_ARRAY	ah_ini_bank1;
	HAL_INI_ARRAY	ah_ini_bank2;
	HAL_INI_ARRAY	ah_ini_bank3;
	HAL_INI_ARRAY	ah_ini_bank6;
	HAL_INI_ARRAY	ah_ini_bank7;
	HAL_INI_ARRAY	ah_ini_addac;
	HAL_INI_ARRAY	ah_ini_pcieserdes;

	void		(*ah_writeIni)(struct ath_hal *,
			    const struct ieee80211_channel *);
	void		(*ah_spurMitigate)(struct ath_hal *,
			    const struct ieee80211_channel *);

	u_int       	ah_globaltxtimeout;	/* global tx timeout */
	u_int		ah_gpioMask;
	int		ah_hangs;		/* h/w hangs state */
	uint8_t		ah_keytype[AR5416_KEYTABLE_SIZE];
	/*
	 * Extension Channel Rx Clear State
	 */
	uint32_t	ah_cycleCount;
	uint32_t	ah_ctlBusy;
	uint32_t	ah_extBusy;
	uint32_t	ah_rx_chainmask;
	uint32_t	ah_tx_chainmask;

	struct ar5416PerCal ah_cal;		/* periodic calibration state */
};
#define	AH5416(_ah)	((struct ath_hal_5416 *)(_ah))

#define IS_5416_PCI(ah) ((AH_PRIVATE(ah)->ah_macVersion) == AR_SREV_VERSION_OWL_PCI)
#define IS_5416_PCIE(ah) ((AH_PRIVATE(ah)->ah_macVersion) == AR_SREV_VERSION_OWL_PCIE)
#undef IS_PCIE
#define IS_PCIE(ah) (IS_5416_PCIE(ah))

extern	HAL_BOOL ar2133RfAttach(struct ath_hal *, HAL_STATUS *);

struct ath_hal;

extern	uint32_t ar5416GetRadioRev(struct ath_hal *ah);
extern	void ar5416InitState(struct ath_hal_5416 *, uint16_t devid,
		HAL_SOFTC sc, HAL_BUS_TAG st, HAL_BUS_HANDLE sh,
		HAL_STATUS *status);
extern	void ar5416Detach(struct ath_hal *ah);
extern	void ar5416AttachPCIE(struct ath_hal *ah);
extern	HAL_BOOL ar5416FillCapabilityInfo(struct ath_hal *ah);

#define	IS_5GHZ_FAST_CLOCK_EN(_ah, _c) \
	(IEEE80211_IS_CHAN_5GHZ(_c) && \
	 ath_hal_eepromGetFlag(ah, AR_EEP_FSTCLK_5G))

extern	void ar5416AniAttach(struct ath_hal *, const struct ar5212AniParams *,
		const struct ar5212AniParams *, HAL_BOOL ena);
extern	void ar5416AniDetach(struct ath_hal *);
extern	HAL_BOOL ar5416AniControl(struct ath_hal *, HAL_ANI_CMD cmd, int param);
extern	HAL_BOOL ar5416AniSetParams(struct ath_hal *,
		const struct ar5212AniParams *, const struct ar5212AniParams *);
extern	void ar5416ProcessMibIntr(struct ath_hal *, const HAL_NODE_STATS *);
extern	void ar5416RxMonitor(struct ath_hal *, const HAL_NODE_STATS *,
			     const struct ieee80211_channel *);
extern	void ar5416AniPoll(struct ath_hal *, const struct ieee80211_channel *);
extern	void ar5416AniReset(struct ath_hal *, const struct ieee80211_channel *,
		HAL_OPMODE, int);

extern	void ar5416SetBeaconTimers(struct ath_hal *, const HAL_BEACON_TIMERS *);
extern	void ar5416BeaconInit(struct ath_hal *ah,
		uint32_t next_beacon, uint32_t beacon_period);
extern	void ar5416ResetStaBeaconTimers(struct ath_hal *ah);
extern	void ar5416SetStaBeaconTimers(struct ath_hal *ah,
		const HAL_BEACON_STATE *);

extern	HAL_BOOL ar5416EepromRead(struct ath_hal *, u_int off, uint16_t *data);
extern	HAL_BOOL ar5416EepromWrite(struct ath_hal *, u_int off, uint16_t data);

extern	HAL_BOOL ar5416IsInterruptPending(struct ath_hal *ah);
extern	HAL_BOOL ar5416GetPendingInterrupts(struct ath_hal *, HAL_INT *masked);
extern	HAL_INT ar5416SetInterrupts(struct ath_hal *ah, HAL_INT ints);

extern	HAL_BOOL ar5416GpioCfgOutput(struct ath_hal *, uint32_t gpio,
		HAL_GPIO_MUX_TYPE);
extern	HAL_BOOL ar5416GpioCfgInput(struct ath_hal *, uint32_t gpio);
extern	HAL_BOOL ar5416GpioSet(struct ath_hal *, uint32_t gpio, uint32_t val);
extern	uint32_t ar5416GpioGet(struct ath_hal *ah, uint32_t gpio);
extern	void ar5416GpioSetIntr(struct ath_hal *ah, u_int, uint32_t ilevel);

extern	u_int ar5416GetWirelessModes(struct ath_hal *ah);
extern	void ar5416SetLedState(struct ath_hal *ah, HAL_LED_STATE state);
extern	void ar5416ResetTsf(struct ath_hal *ah);
extern	HAL_BOOL ar5416SetAntennaSwitch(struct ath_hal *, HAL_ANT_SETTING);
extern	HAL_BOOL ar5416SetDecompMask(struct ath_hal *, uint16_t, int);
extern	void ar5416SetCoverageClass(struct ath_hal *, uint8_t, int);
extern	uint32_t ar5416Get11nExtBusy(struct ath_hal *ah);
extern	void ar5416Set11nMac2040(struct ath_hal *ah, HAL_HT_MACMODE mode);
extern	HAL_HT_RXCLEAR ar5416Get11nRxClear(struct ath_hal *ah);
extern	void ar5416Set11nRxClear(struct ath_hal *ah, HAL_HT_RXCLEAR rxclear);
extern	HAL_STATUS ar5416GetCapability(struct ath_hal *ah,
	    HAL_CAPABILITY_TYPE type, uint32_t capability, uint32_t *result);
extern	HAL_BOOL ar5416GetDiagState(struct ath_hal *ah, int request,
	    const void *args, uint32_t argsize,
	    void **result, uint32_t *resultsize);

extern	HAL_BOOL ar5416SetPowerMode(struct ath_hal *ah, HAL_POWER_MODE mode,
		int setChip);
extern	HAL_POWER_MODE ar5416GetPowerMode(struct ath_hal *ah);
extern	HAL_BOOL ar5416GetPowerStatus(struct ath_hal *ah);

extern	HAL_BOOL ar5416ResetKeyCacheEntry(struct ath_hal *ah, uint16_t entry);
extern	HAL_BOOL ar5416SetKeyCacheEntry(struct ath_hal *ah, uint16_t entry,
	       const HAL_KEYVAL *k, const uint8_t *mac, int xorKey);

extern	void ar5416StartPcuReceive(struct ath_hal *ah);
extern	void ar5416StopPcuReceive(struct ath_hal *ah);
extern	HAL_BOOL ar5416SetupRxDesc(struct ath_hal *,
		struct ath_desc *, uint32_t size, u_int flags);
extern	HAL_STATUS ar5416ProcRxDesc(struct ath_hal *ah, struct ath_desc *,
		uint32_t, struct ath_desc *, uint64_t,
		struct ath_rx_status *);

extern	HAL_BOOL ar5416Reset(struct ath_hal *ah, HAL_OPMODE opmode,
		struct ieee80211_channel *chan,
		HAL_BOOL bChannelChange, HAL_STATUS *status);
extern	HAL_BOOL ar5416PhyDisable(struct ath_hal *ah);
extern	HAL_RFGAIN ar5416GetRfgain(struct ath_hal *ah);
extern	HAL_BOOL ar5416Disable(struct ath_hal *ah);
extern	HAL_BOOL ar5416ChipReset(struct ath_hal *ah,
		const struct ieee80211_channel *);
extern	HAL_BOOL ar5416SetBoardValues(struct ath_hal *,
		const struct ieee80211_channel *);
extern	HAL_BOOL ar5416SetResetReg(struct ath_hal *, uint32_t type);
extern	HAL_BOOL ar5416SetTxPowerLimit(struct ath_hal *ah, uint32_t limit);
extern	HAL_BOOL ar5416SetTransmitPower(struct ath_hal *,
    		const struct ieee80211_channel *, uint16_t *);
extern	HAL_BOOL ar5416GetChipPowerLimits(struct ath_hal *ah,
		struct ieee80211_channel *chan);
extern	void ar5416GetChannelCenters(struct ath_hal *,
		const struct ieee80211_channel *chan, CHAN_CENTERS *centers);
extern	void ar5416GetTargetPowers(struct ath_hal *ah, 
		const struct ieee80211_channel *chan,
		CAL_TARGET_POWER_HT *powInfo,
		uint16_t numChannels, CAL_TARGET_POWER_HT *pNewPower,
		uint16_t numRates, HAL_BOOL isHt40Target);
extern	void ar5416GetTargetPowersLeg(struct ath_hal *ah, 
		const struct ieee80211_channel *chan,
		CAL_TARGET_POWER_LEG *powInfo,
		uint16_t numChannels, CAL_TARGET_POWER_LEG *pNewPower,
		uint16_t numRates, HAL_BOOL isExtTarget);
extern	void ar5416InitChainMasks(struct ath_hal *ah);
extern	void ar5416RestoreChainMask(struct ath_hal *ah);

extern	HAL_BOOL ar5416StopTxDma(struct ath_hal *ah, u_int q);
extern	HAL_BOOL ar5416SetupTxDesc(struct ath_hal *ah, struct ath_desc *ds,
		u_int pktLen, u_int hdrLen, HAL_PKT_TYPE type, u_int txPower,
		u_int txRate0, u_int txTries0,
		u_int keyIx, u_int antMode, u_int flags,
		u_int rtsctsRate, u_int rtsctsDuration,
		u_int compicvLen, u_int compivLen, u_int comp);
extern	HAL_BOOL ar5416SetupXTxDesc(struct ath_hal *, struct ath_desc *,
		u_int txRate1, u_int txRetries1,
		u_int txRate2, u_int txRetries2,
		u_int txRate3, u_int txRetries3);
extern	HAL_BOOL ar5416FillTxDesc(struct ath_hal *ah, struct ath_desc *ds,
		u_int segLen, HAL_BOOL firstSeg, HAL_BOOL lastSeg,
		const struct ath_desc *ds0);
extern	HAL_STATUS ar5416ProcTxDesc(struct ath_hal *ah,
		struct ath_desc *, struct ath_tx_status *);
extern	HAL_BOOL ar5416GetTxCompletionRates(struct ath_hal *ah,
		const struct ath_desc *ds0, int *rates, int *tries);

extern	HAL_BOOL ar5416ChainTxDesc(struct ath_hal *ah, struct ath_desc *ds,
		u_int pktLen, u_int hdrLen, HAL_PKT_TYPE type, u_int keyIx,
		HAL_CIPHER cipher, uint8_t delims, u_int segLen, HAL_BOOL firstSeg,
		HAL_BOOL lastSeg);
extern	HAL_BOOL ar5416SetupFirstTxDesc(struct ath_hal *ah, struct ath_desc *ds,
		u_int aggrLen, u_int flags, u_int txPower, u_int txRate0, u_int txTries0,
		u_int antMode, u_int rtsctsRate, u_int rtsctsDuration);
extern	HAL_BOOL ar5416SetupLastTxDesc(struct ath_hal *ah, struct ath_desc *ds,
		const struct ath_desc *ds0);
extern	HAL_BOOL ar5416SetGlobalTxTimeout(struct ath_hal *ah, u_int tu);
extern	u_int ar5416GetGlobalTxTimeout(struct ath_hal *ah);
extern	void ar5416Set11nRateScenario(struct ath_hal *ah, struct ath_desc *ds,
		u_int durUpdateEn, u_int rtsctsRate, HAL_11N_RATE_SERIES series[],
		u_int nseries);
extern	void ar5416Set11nAggrMiddle(struct ath_hal *ah, struct ath_desc *ds, u_int numDelims);
extern	void ar5416Clr11nAggr(struct ath_hal *ah, struct ath_desc *ds);
extern	void ar5416Set11nBurstDuration(struct ath_hal *ah, struct ath_desc *ds, u_int burstDuration);

extern	const HAL_RATE_TABLE *ar5416GetRateTable(struct ath_hal *, u_int mode);
#endif	/* _ATH_AR5416_H_ */
