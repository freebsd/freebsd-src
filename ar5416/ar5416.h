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
 * $Id: ar5416.h,v 1.16 2008/11/10 04:08:04 sam Exp $
 */
#ifndef _ATH_AR5416_H_
#define _ATH_AR5416_H_

#include "ar5212/ar5212.h"

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

#define	AR5416_NUM_NF_READINGS		6	/* (3 chains * (ctl + ext) */
#define	AR5416_CCA_MAX_GOOD_VALUE	-85
#define	AR5416_CCA_MAX_HIGH_VALUE	-62
#define	AR5416_CCA_MIN_BAD_VALUE	-140

#define INIT_CAL(_perCal) do {				\
	(_perCal)->calState = CAL_WAITING;		\
	(_perCal)->calNext = AH_NULL;			\
} while (0)

#define INSERT_CAL(_ahp, _perCal) do {					\
	if ((_ahp)->ah_cal_last == AH_NULL) {			\
		(_ahp)->ah_cal_list = (_ahp)->ah_cal_last = (_perCal); \
		((_ahp)->ah_cal_last)->calNext = (_perCal);	\
	} else {							\
		((_ahp)->ah_cal_last)->calNext = (_perCal);	\
		(_ahp)->ah_cal_last = (_perCal);			\
		(_perCal)->calNext = (_ahp)->ah_cal_list;		\
	}								\
} while (0)
 
typedef enum cal_types {
	ADC_DC_INIT_CAL	= 0x1,
	ADC_GAIN_CAL	= 0x2,
	ADC_DC_CAL	= 0x4,
	IQ_MISMATCH_CAL	= 0x8
} HAL_CAL_TYPE;

/* Calibrate state */
typedef enum cal_state {
	CAL_INACTIVE,
	CAL_WAITING,
	CAL_RUNNING,
	CAL_DONE
} HAL_CAL_STATE;

typedef union {
	uint32_t	u;
	int32_t		s;
} HAL_CAL_SAMPLE;

#define	MIN_CAL_SAMPLES     1
#define	MAX_CAL_SAMPLES    64
#define	INIT_LOG_COUNT      5
#define	PER_MIN_LOG_COUNT   2
#define	PER_MAX_LOG_COUNT  10

/* Per Calibration data structure */
typedef struct per_cal_data {
	const char	*calName;		/* for diagnostics */
	HAL_CAL_TYPE	calType;		/* Type of calibration */
	uint32_t	calNumSamples;		/* # SW samples to collect */
	uint32_t	calCountMax;		/* # HW samples to collect */
	void (*calCollect)(struct ath_hal *);	/* Accumulator function */
						/* Post-processing function */
	void (*calPostProc)(struct ath_hal *, uint8_t);
} HAL_PERCAL_DATA;

/* List structure for calibration data */
typedef struct cal_list {
	struct cal_list		*calNext;
	HAL_CAL_STATE		calState;
	const HAL_PERCAL_DATA	*calData;
} HAL_CAL_LIST;

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

	u_int       	ah_globaltxtimeout;	/* global tx timeout */
	int		ah_clksel;
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
	/*
	 * Periodic calibration state.
	 */
	HAL_CAL_TYPE	ah_suppCals;
	HAL_CAL_LIST	ah_iqCalData;
	HAL_CAL_LIST	ah_adcGainCalData;
	HAL_CAL_LIST	ah_adcDcCalInitData;
	HAL_CAL_LIST	ah_adcDcCalData;
	HAL_CAL_LIST	*ah_cal_list;
	HAL_CAL_LIST	*ah_cal_last;
	HAL_CAL_LIST	*ah_cal_curr;
#define AR5416_MAX_CHAINS            	3	/* XXX dup's eeprom def */
	HAL_CAL_SAMPLE	ah_caldata[4][AR5416_MAX_CHAINS];
	int		ah_calSamples;
	/*
	 * Noise floor cal histogram support.
	 * XXX be nice to re-use space in ar5212
	 */
	struct ar5212NfCalHist ah_nfCalHist[AR5416_NUM_NF_READINGS];
};
#define	AH5416(_ah)	((struct ath_hal_5416 *)(_ah))

/* IQ Cal aliases */
#define	ah_totalPowerMeasI(i)		ah_caldata[0][i].u
#define	ah_totalPowerMeasQ(i)		ah_caldata[1][i].u
#define	ah_totalIqCorrMeas(i)		ah_caldata[2][i].s
/* Adc Gain Cal aliases */
#define	ah_totalAdcIOddPhase(i)		ah_caldata[0][i].u
#define	ah_totalAdcIEvenPhase(i)	ah_caldata[1][i].u
#define	ah_totalAdcQOddPhase(i)		ah_caldata[2][i].u
#define	ah_totalAdcQEvenPhase(i)	ah_caldata[3][i].u
/* Adc DC Offset Cal aliases */
#define	ah_totalAdcDcOffsetIOddPhase(i)	 ah_caldata[0][i].s
#define	ah_totalAdcDcOffsetIEvenPhase(i) ah_caldata[1][i].s
#define	ah_totalAdcDcOffsetQOddPhase(i)	 ah_caldata[2][i].s
#define	ah_totalAdcDcOffsetQEvenPhase(i) ah_caldata[3][i].s

#define IS_5416_PCI(ah) ((AH_PRIVATE(ah)->ah_macVersion) == AR_SREV_VERSION_OWL_PCI)
#define IS_5416_PCIE(ah) ((AH_PRIVATE(ah)->ah_macVersion) == AR_SREV_VERSION_OWL_PCIE)
#undef IS_PCIE
#define IS_PCIE(ah) (IS_5416_PCIE(ah))

extern	HAL_BOOL ar2133RfAttach(struct ath_hal *, HAL_STATUS *);

struct ath_hal;

extern	struct ath_hal * ar5416Attach(uint16_t devid, HAL_SOFTC sc,
		HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_STATUS *status);
extern	void ar5416InitState(struct ath_hal_5416 *, uint16_t devid,
		HAL_SOFTC sc, HAL_BUS_TAG st, HAL_BUS_HANDLE sh,
		HAL_STATUS *status);
extern	void ar5416Detach(struct ath_hal *ah);
extern	HAL_BOOL ar5416FillCapabilityInfo(struct ath_hal *ah);

#define	IS_5GHZ_FAST_CLOCK_EN(_ah, _c) \
	(IS_CHAN_5GHZ(_c) && ath_hal_eepromGetFlag(ah, AR_EEP_FSTCLK_5G))

extern	HAL_BOOL ar5416AniAttach(struct ath_hal *ah);
extern	void ar5416AniDetach(struct ath_hal *ah);

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

extern	HAL_BOOL ar5416GpioCfgOutput(struct ath_hal *, uint32_t gpio);
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
		HAL_CHANNEL *chan, HAL_BOOL bChannelChange, HAL_STATUS *status);
extern	HAL_BOOL ar5416PhyDisable(struct ath_hal *ah);
extern	HAL_RFGAIN ar5416GetRfgain(struct ath_hal *ah);
extern	HAL_BOOL ar5416Disable(struct ath_hal *ah);
extern	HAL_BOOL ar5416ChipReset(struct ath_hal *ah, HAL_CHANNEL *);
extern	HAL_BOOL ar5416SetResetReg(struct ath_hal *, uint32_t type);
extern	HAL_BOOL ar5416PerCalibration(struct ath_hal *,  HAL_CHANNEL *,
		HAL_BOOL *isIQdone);
extern  void ar5416ResetCalValid(struct ath_hal *ah,  HAL_CHANNEL *chan,
		HAL_BOOL *isIQdone);
extern	void ar5416IQCalCollect(struct ath_hal *ah);
extern	void ar5416IQCalibration(struct ath_hal *ah, uint8_t numChains);
extern	void ar5416AdcGainCalCollect(struct ath_hal *ah);
extern	void ar5416AdcGainCalibration(struct ath_hal *ah, uint8_t numChains);
extern	void ar5416AdcDcCalCollect(struct ath_hal *ah);
extern	void ar5416AdcDcCalibration(struct ath_hal *ah, uint8_t numChains);
extern	void ar5416InitNfHistBuff(struct ar5212NfCalHist *h);
extern	HAL_BOOL ar5416SetTxPowerLimit(struct ath_hal *ah, uint32_t limit);
extern	HAL_BOOL ar5416GetChipPowerLimits(struct ath_hal *ah,
		HAL_CHANNEL *chans, uint32_t nchans);
extern	void ar5416GetChannelCenters(struct ath_hal *,
		HAL_CHANNEL_INTERNAL *chan, CHAN_CENTERS *centers);

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

extern	const HAL_RATE_TABLE *ar5416GetRateTable(struct ath_hal *, u_int mode);
#endif	/* _ATH_AR5416_H_ */
