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
 * $Id: ah_internal.h,v 1.21 2008/11/27 22:29:27 sam Exp $
 */
#ifndef _ATH_AH_INTERAL_H_
#define _ATH_AH_INTERAL_H_
/*
 * Atheros Device Hardware Access Layer (HAL).
 *
 * Internal definitions.
 */
#define	AH_NULL	0
#define	AH_MIN(a,b)	((a)<(b)?(a):(b))
#define	AH_MAX(a,b)	((a)>(b)?(a):(b))

#ifndef NBBY
#define	NBBY	8			/* number of bits/byte */
#endif

#ifndef roundup
#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))  /* to any y */
#endif
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

#ifndef offsetof
#define	offsetof(type, field)	((size_t)(&((type *)0)->field))
#endif

/*
 * Remove const in a way that keeps the compiler happy.
 * This works for gcc but may require other magic for
 * other compilers (not sure where this should reside).
 * Note that uintptr_t is C99.
 */
#ifndef __DECONST
#ifndef _UINTPTR_T
#if AH_WORDSIZE == 64
typedef unsigned long int uintptr_t;
#else
typedef unsigned int uintptr_t;
#endif
#endif
#define	__DECONST(type, var)	((type)(uintptr_t)(const void *)(var))
#endif

typedef struct {
	uint16_t	start;		/* first register */
	uint16_t	end;		/* ending register or zero */
} HAL_REGRANGE;

/*
 * Transmit power scale factor.
 *
 * NB: This is not public because we want to discourage the use of
 *     scaling; folks should use the tx power limit interface.
 */
typedef enum {
	HAL_TP_SCALE_MAX	= 0,		/* no scaling (default) */
	HAL_TP_SCALE_50		= 1,		/* 50% of max (-3 dBm) */
	HAL_TP_SCALE_25		= 2,		/* 25% of max (-6 dBm) */
	HAL_TP_SCALE_12		= 3,		/* 12% of max (-9 dBm) */
	HAL_TP_SCALE_MIN	= 4,		/* min, but still on */
} HAL_TP_SCALE;

typedef enum {
 	HAL_CAP_RADAR		= 0,		/* Radar capability */
 	HAL_CAP_AR		= 1,		/* AR capability */
} HAL_PHYDIAG_CAPS;

/*
 * Each chip or class of chips registers to offer support.
 */
struct ath_hal_chip {
	const char	*name;
	const char	*(*probe)(uint16_t vendorid, uint16_t devid);
	struct ath_hal	*(*attach)(uint16_t devid, HAL_SOFTC,
			    HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_STATUS *error);
};
#ifndef AH_CHIP
#define	AH_CHIP(_name, _probe, _attach)				\
static struct ath_hal_chip name##_chip = {			\
	.name		= #_name,				\
	.probe		= _probe,				\
	.attach		= _attach				\
};								\
OS_DATA_SET(ah_chips, name##_chip)
#endif

/*
 * Each RF backend registers to offer support; this is mostly
 * used by multi-chip 5212 solutions.  Single-chip solutions
 * have a fixed idea about which RF to use.
 */
struct ath_hal_rf {
	const char	*name;
	HAL_BOOL	(*probe)(struct ath_hal *ah);
	HAL_BOOL	(*attach)(struct ath_hal *ah, HAL_STATUS *ecode);
};
#ifndef AH_RF
#define	AH_RF(_name, _probe, _attach)				\
static struct ath_hal_rf name##_rf = {				\
	.name		= #_name,				\
	.probe		= _probe,				\
	.attach		= _attach				\
};								\
OS_DATA_SET(ah_rfs, name##_rf)
#endif

struct ath_hal_rf *ath_hal_rfprobe(struct ath_hal *ah, HAL_STATUS *ecode);

/*
 * Internal form of a HAL_CHANNEL.  Note that the structure
 * must be defined such that you can cast references to a
 * HAL_CHANNEL so don't shuffle the first two members.
 */
typedef struct {
	uint32_t	channelFlags;
	uint16_t	channel;	/* NB: must be first for casting */
	uint8_t		privFlags;
	int8_t		maxRegTxPower;
	int8_t		maxTxPower;
	int8_t		minTxPower;	/* as above... */

	HAL_BOOL	bssSendHere;
	uint8_t		gainI;
	HAL_BOOL	iqCalValid;
	uint8_t		calValid;		/* bitmask of cal types */
	int8_t		iCoff;
	int8_t		qCoff;
	int16_t		rawNoiseFloor;
	int16_t		noiseFloorAdjust;
	int8_t		antennaMax;
	uint32_t	regDmnFlags;		/* Flags for channel use in reg */
	uint32_t	conformanceTestLimit;	/* conformance test limit from reg domain */
	uint16_t	mainSpur;		/* cached spur value for this cahnnel */
} HAL_CHANNEL_INTERNAL;

typedef struct {
	uint32_t	halChanSpreadSupport 		: 1,
			halSleepAfterBeaconBroken	: 1,
			halCompressSupport		: 1,
			halBurstSupport			: 1,
			halFastFramesSupport		: 1,
			halChapTuningSupport		: 1,
			halTurboGSupport		: 1,
			halTurboPrimeSupport		: 1,
			halMicAesCcmSupport		: 1,
			halMicCkipSupport		: 1,
			halMicTkipSupport		: 1,
			halTkipMicTxRxKeySupport	: 1,
			halCipherAesCcmSupport		: 1,
			halCipherCkipSupport		: 1,
			halCipherTkipSupport		: 1,
			halPSPollBroken			: 1,
			halVEOLSupport			: 1,
			halBssIdMaskSupport		: 1,
			halMcastKeySrchSupport		: 1,
			halTsfAddSupport		: 1,
			halChanHalfRate			: 1,
			halChanQuarterRate		: 1,
			halHTSupport			: 1,
			halRfSilentSupport		: 1,
			halHwPhyCounterSupport		: 1,
			halWowSupport			: 1,
			halWowMatchPatternExact		: 1,
			halAutoSleepSupport		: 1,
			halFastCCSupport		: 1,
			halBtCoexSupport		: 1;
	uint32_t	halRxStbcSupport		: 1,
			halTxStbcSupport		: 1,
			halGTTSupport			: 1,
			halCSTSupport			: 1,
			halRifsRxSupport		: 1,
			halRifsTxSupport		: 1,
			halExtChanDfsSupport		: 1,
			halForcePpmSupport		: 1,
			halEnhancedPmSupport		: 1,
			halMbssidAggrSupport		: 1;
	uint32_t	halWirelessModes;
	uint16_t	halTotalQueues;
	uint16_t	halKeyCacheSize;
	uint16_t	halLow5GhzChan, halHigh5GhzChan;
	uint16_t	halLow2GhzChan, halHigh2GhzChan;
	int		halTstampPrecision;
	int		halRtsAggrLimit;
	uint8_t		halTxChainMask;
	uint8_t		halRxChainMask;
	uint8_t		halNumGpioPins;
	uint8_t		halNumAntCfg2GHz;
	uint8_t		halNumAntCfg5GHz;
} HAL_CAPABILITIES;

/*
 * The ``private area'' follows immediately after the ``public area''
 * in the data structure returned by ath_hal_attach.  Private data are
 * used by device-independent code such as the regulatory domain support.
 * In general, code within the HAL should never depend on data in the
 * public area.  Instead any public data needed internally should be
 * shadowed here.
 *
 * When declaring a device-specific ath_hal data structure this structure
 * is assumed to at the front; e.g.
 *
 *	struct ath_hal_5212 {
 *		struct ath_hal_private	ah_priv;
 *		...
 *	};
 *
 * It might be better to manage the method pointers in this structure
 * using an indirect pointer to a read-only data structure but this would
 * disallow class-style method overriding.
 */
struct ath_hal_private {
	struct ath_hal	h;			/* public area */

	/* NB: all methods go first to simplify initialization */
	HAL_BOOL	(*ah_getChannelEdges)(struct ath_hal*,
				uint16_t channelFlags,
				uint16_t *lowChannel, uint16_t *highChannel);
	u_int		(*ah_getWirelessModes)(struct ath_hal*);
	HAL_BOOL	(*ah_eepromRead)(struct ath_hal *, u_int off,
				uint16_t *data);
	HAL_BOOL	(*ah_eepromWrite)(struct ath_hal *, u_int off,
				uint16_t data);
	HAL_BOOL	(*ah_gpioCfgOutput)(struct ath_hal *, uint32_t gpio);
	HAL_BOOL	(*ah_gpioCfgInput)(struct ath_hal *, uint32_t gpio);
	uint32_t	(*ah_gpioGet)(struct ath_hal *, uint32_t gpio);
	HAL_BOOL	(*ah_gpioSet)(struct ath_hal *,
				uint32_t gpio, uint32_t val);
	void		(*ah_gpioSetIntr)(struct ath_hal*, u_int, uint32_t);
	HAL_BOOL	(*ah_getChipPowerLimits)(struct ath_hal *,
				HAL_CHANNEL *, uint32_t);
	int16_t		(*ah_getNfAdjust)(struct ath_hal *,
				const HAL_CHANNEL_INTERNAL*);
	void		(*ah_getNoiseFloor)(struct ath_hal *,
				int16_t nfarray[]);

	void		*ah_eeprom;		/* opaque EEPROM state */
	uint16_t	ah_eeversion;		/* EEPROM version */
	void		(*ah_eepromDetach)(struct ath_hal *);
	HAL_STATUS	(*ah_eepromGet)(struct ath_hal *, int, void *);
	HAL_BOOL	(*ah_eepromSet)(struct ath_hal *, int, int);
	uint16_t	(*ah_getSpurChan)(struct ath_hal *, int, HAL_BOOL);
	HAL_BOOL	(*ah_eepromDiag)(struct ath_hal *, int request,
			    const void *args, uint32_t argsize,
			    void **result, uint32_t *resultsize);

	/*
	 * Device revision information.
	 */
	uint16_t	ah_devid;		/* PCI device ID */
	uint16_t	ah_subvendorid;		/* PCI subvendor ID */
	uint32_t	ah_macVersion;		/* MAC version id */
	uint16_t	ah_macRev;		/* MAC revision */
	uint16_t	ah_phyRev;		/* PHY revision */
	uint16_t	ah_analog5GhzRev;	/* 2GHz radio revision */
	uint16_t	ah_analog2GhzRev;	/* 5GHz radio revision */


	HAL_OPMODE	ah_opmode;		/* operating mode from reset */
	HAL_CAPABILITIES ah_caps;		/* device capabilities */
	uint32_t	ah_diagreg;		/* user-specified AR_DIAG_SW */
	int16_t		ah_powerLimit;		/* tx power cap */
	uint16_t	ah_maxPowerLevel;	/* calculated max tx power */
	u_int		ah_tpScale;		/* tx power scale factor */
	uint32_t	ah_11nCompat;		/* 11n compat controls */

	/*
	 * State for regulatory domain handling.
	 */
	HAL_REG_DOMAIN	ah_currentRD;		/* Current regulatory domain */
	HAL_CTRY_CODE	ah_countryCode;		/* current country code */
	HAL_CHANNEL_INTERNAL ah_channels[256];	/* calculated channel list */
	u_int		ah_nchan;		/* valid channels in list */
	HAL_CHANNEL_INTERNAL *ah_curchan;	/* current channel */

	uint8_t    	ah_coverageClass;   	/* coverage class */
	HAL_BOOL    	ah_regdomainUpdate;     /* regdomain is updated? */
	/*
	 * RF Silent handling; setup according to the EEPROM.
	 */
	uint16_t	ah_rfsilent;		/* GPIO pin + polarity */
	HAL_BOOL	ah_rfkillEnabled;	/* enable/disable RfKill */
	/*
	 * Diagnostic support for discriminating HIUERR reports.
	 */
	uint32_t	ah_fatalState[6];	/* AR_ISR+shadow regs */
	int		ah_rxornIsFatal;	/* how to treat HAL_INT_RXORN */
};

#define	AH_PRIVATE(_ah)	((struct ath_hal_private *)(_ah))

#define	ath_hal_getChannelEdges(_ah, _cf, _lc, _hc) \
	AH_PRIVATE(_ah)->ah_getChannelEdges(_ah, _cf, _lc, _hc)
#define	ath_hal_getWirelessModes(_ah) \
	AH_PRIVATE(_ah)->ah_getWirelessModes(_ah)
#define	ath_hal_eepromRead(_ah, _off, _data) \
	AH_PRIVATE(_ah)->ah_eepromRead(_ah, _off, _data)
#define	ath_hal_eepromWrite(_ah, _off, _data) \
	AH_PRIVATE(_ah)->ah_eepromWrite(_ah, _off, _data)
#define	ath_hal_gpioCfgOutput(_ah, _gpio) \
	AH_PRIVATE(_ah)->ah_gpioCfgOutput(_ah, _gpio)
#define	ath_hal_gpioCfgInput(_ah, _gpio) \
	AH_PRIVATE(_ah)->ah_gpioCfgInput(_ah, _gpio)
#define	ath_hal_gpioGet(_ah, _gpio) \
	AH_PRIVATE(_ah)->ah_gpioGet(_ah, _gpio)
#define	ath_hal_gpioSet(_ah, _gpio, _val) \
	AH_PRIVATE(_ah)->ah_gpioGet(_ah, _gpio, _val)
#define	ath_hal_gpioSetIntr(_ah, _gpio, _ilevel) \
	AH_PRIVATE(_ah)->ah_gpioSetIntr(_ah, _gpio, _ilevel)
#define	ath_hal_getpowerlimits(_ah, _chans, _nchan) \
	AH_PRIVATE(_ah)->ah_getChipPowerLimits(_ah, _chans, _nchan)
#define ath_hal_getNfAdjust(_ah, _c) \
	AH_PRIVATE(_ah)->ah_getNfAdjust(_ah, _c)
#define	ath_hal_getNoiseFloor(_ah, _nfArray) \
	AH_PRIVATE(_ah)->ah_getNoiseFloor(_ah, _nfArray)

#define	ath_hal_eepromDetach(_ah) \
	AH_PRIVATE(_ah)->ah_eepromDetach(_ah)
#define	ath_hal_eepromGet(_ah, _param, _val) \
	AH_PRIVATE(_ah)->ah_eepromGet(_ah, _param, _val)
#define	ath_hal_eepromSet(_ah, _param, _val) \
	AH_PRIVATE(_ah)->ah_eepromSet(_ah, _param, _val)
#define	ath_hal_eepromGetFlag(_ah, _param) \
	(AH_PRIVATE(_ah)->ah_eepromGet(_ah, _param, AH_NULL) == HAL_OK)
#define ath_hal_getSpurChan(_ah, _ix, _is2G) \
	AH_PRIVATE(_ah)->ah_getSpurChan(_ah, _ix, _is2G)
#define	ath_hal_eepromDiag(_ah, _request, _a, _asize, _r, _rsize) \
	AH_PRIVATE(_ah)->ah_eepromDiag(_ah, _request, _a, _asize,  _r, _rsize)

#if !defined(_NET_IF_IEEE80211_H_) && !defined(_NET80211__IEEE80211_H_)
/*
 * Stuff that would naturally come from _ieee80211.h
 */
#define	IEEE80211_ADDR_LEN		6

#define	IEEE80211_WEP_KEYLEN			5	/* 40bit */
#define	IEEE80211_WEP_IVLEN			3	/* 24bit */
#define	IEEE80211_WEP_KIDLEN			1	/* 1 octet */
#define	IEEE80211_WEP_CRCLEN			4	/* CRC-32 */

#define	IEEE80211_CRC_LEN			4

#define	IEEE80211_MTU				1500
#define	IEEE80211_MAX_LEN			(2300 + IEEE80211_CRC_LEN + \
    (IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_CRCLEN))

enum {
	IEEE80211_T_DS,			/* direct sequence spread spectrum */
	IEEE80211_T_FH,			/* frequency hopping */
	IEEE80211_T_OFDM,		/* frequency division multiplexing */
	IEEE80211_T_TURBO,		/* high rate DS */
	IEEE80211_T_HT,			/* HT - full GI */
};
#define	IEEE80211_T_CCK	IEEE80211_T_DS	/* more common nomenclatur */
#endif /* _NET_IF_IEEE80211_H_ */

/* NB: these are defined privately until XR support is announced */
enum {
	ATHEROS_T_XR	= IEEE80211_T_HT+1,	/* extended range */
};

#define HAL_TXQ_USE_LOCKOUT_BKOFF_DIS	0x00000001

#define INIT_AIFS		2
#define INIT_CWMIN		15
#define INIT_CWMIN_11B		31
#define INIT_CWMAX		1023
#define INIT_SH_RETRY		10
#define INIT_LG_RETRY		10
#define INIT_SSH_RETRY		32
#define INIT_SLG_RETRY		32

typedef struct {
	uint32_t	tqi_ver;		/* HAL TXQ verson */
	HAL_TX_QUEUE	tqi_type;		/* hw queue type*/
	HAL_TX_QUEUE_SUBTYPE tqi_subtype;	/* queue subtype, if applicable */
	HAL_TX_QUEUE_FLAGS tqi_qflags;		/* queue flags */
	uint32_t	tqi_priority;
	uint32_t	tqi_aifs;		/* aifs */
	uint32_t	tqi_cwmin;		/* cwMin */
	uint32_t	tqi_cwmax;		/* cwMax */
	uint16_t	tqi_shretry;		/* frame short retry limit */
	uint16_t	tqi_lgretry;		/* frame long retry limit */
	uint32_t	tqi_cbrPeriod;
	uint32_t	tqi_cbrOverflowLimit;
	uint32_t	tqi_burstTime;
	uint32_t	tqi_readyTime;
	uint32_t	tqi_physCompBuf;
	uint32_t	tqi_intFlags;		/* flags for internal use */
} HAL_TX_QUEUE_INFO;

extern	HAL_BOOL ath_hal_setTxQProps(struct ath_hal *ah,
		HAL_TX_QUEUE_INFO *qi, const HAL_TXQ_INFO *qInfo);
extern	HAL_BOOL ath_hal_getTxQProps(struct ath_hal *ah,
		HAL_TXQ_INFO *qInfo, const HAL_TX_QUEUE_INFO *qi);

typedef enum {
	HAL_ANI_PRESENT,			/* is ANI support present */
	HAL_ANI_NOISE_IMMUNITY_LEVEL,		/* set level */
	HAL_ANI_OFDM_WEAK_SIGNAL_DETECTION,	/* enable/disable */
	HAL_ANI_CCK_WEAK_SIGNAL_THR,		/* enable/disable */
	HAL_ANI_FIRSTEP_LEVEL,			/* set level */
	HAL_ANI_SPUR_IMMUNITY_LEVEL,		/* set level */
	HAL_ANI_MODE = 6,	/* 0 => manual, 1 => auto (XXX do not change) */
	HAL_ANI_PHYERR_RESET,			/* reset phy error stats */
} HAL_ANI_CMD;

#define	HAL_SPUR_VAL_MASK		0x3FFF
#define	HAL_SPUR_CHAN_WIDTH		87
#define	HAL_BIN_WIDTH_BASE_100HZ	3125
#define	HAL_BIN_WIDTH_TURBO_100HZ	6250
#define	HAL_MAX_BINS_ALLOWED		28

/*
 * A    = 5GHZ|OFDM
 * T    = 5GHZ|OFDM|TURBO
 *
 * IS_CHAN_A(T) will return TRUE.  This is probably
 * not the default behavior we want.  We should migrate to a better mask --
 * perhaps CHANNEL_ALL.
 *
 * For now, IS_CHAN_G() masks itself with CHANNEL_108G.
 *
 */

#define	IS_CHAN_A(_c)	(((_c)->channelFlags & CHANNEL_A) == CHANNEL_A)
#define	IS_CHAN_B(_c)	(((_c)->channelFlags & CHANNEL_B) == CHANNEL_B)
#define	IS_CHAN_G(_c)	(((_c)->channelFlags & (CHANNEL_108G|CHANNEL_G)) == CHANNEL_G)
#define	IS_CHAN_108G(_c)(((_c)->channelFlags & CHANNEL_108G) == CHANNEL_108G)
#define	IS_CHAN_T(_c)	(((_c)->channelFlags & CHANNEL_T) == CHANNEL_T)
#define	IS_CHAN_PUREG(_c) \
	(((_c)->channelFlags & CHANNEL_PUREG) == CHANNEL_PUREG)

#define	IS_CHAN_TURBO(_c)	(((_c)->channelFlags & CHANNEL_TURBO) != 0)
#define	IS_CHAN_CCK(_c)		(((_c)->channelFlags & CHANNEL_CCK) != 0)
#define	IS_CHAN_OFDM(_c)	(((_c)->channelFlags & CHANNEL_OFDM) != 0)
#define	IS_CHAN_5GHZ(_c)	(((_c)->channelFlags & CHANNEL_5GHZ) != 0)
#define	IS_CHAN_2GHZ(_c)	(((_c)->channelFlags & CHANNEL_2GHZ) != 0)
#define	IS_CHAN_PASSIVE(_c)	(((_c)->channelFlags & CHANNEL_PASSIVE) != 0)
#define	IS_CHAN_HALF_RATE(_c)	(((_c)->channelFlags & CHANNEL_HALF) != 0)
#define	IS_CHAN_QUARTER_RATE(_c) (((_c)->channelFlags & CHANNEL_QUARTER) != 0)

#define	IS_CHAN_IN_PUBLIC_SAFETY_BAND(_c) ((_c) > 4940 && (_c) < 4990)

#define	CHANNEL_HT40		(CHANNEL_HT40PLUS | CHANNEL_HT40MINUS)
#define	CHANNEL_HT		(CHANNEL_HT20 | CHANNEL_HT40)
#define	IS_CHAN_HT(_c)		(((_c)->channelFlags & CHANNEL_HT) != 0)
#define	IS_CHAN_HT20(_c)	(((_c)->channelFlags & CHANNEL_HT) == CHANNEL_HT20)
#define	IS_CHAN_HT40(_c)	(((_c)->channelFlags & CHANNEL_HT40) != 0)

/*
 * Deduce if the host cpu has big- or litt-endian byte order.
 */
static __inline__ int
isBigEndian(void)
{
	union {
		int32_t i;
		char c[4];
	} u;
	u.i = 1;
	return (u.c[0] == 0);
}

/* unalligned little endian access */     
#define LE_READ_2(p)							\
	((uint16_t)							\
	 ((((const uint8_t *)(p))[0]    ) | (((const uint8_t *)(p))[1]<< 8)))
#define LE_READ_4(p)							\
	((uint32_t)							\
	 ((((const uint8_t *)(p))[0]    ) | (((const uint8_t *)(p))[1]<< 8) |\
	  (((const uint8_t *)(p))[2]<<16) | (((const uint8_t *)(p))[3]<<24)))

/*
 * Register manipulation macros that expect bit field defines
 * to follow the convention that an _S suffix is appended for
 * a shift count, while the field mask has no suffix.
 */
#define	SM(_v, _f)	(((_v) << _f##_S) & (_f))
#define	MS(_v, _f)	(((_v) & (_f)) >> _f##_S)
#define	OS_REG_RMW_FIELD(_a, _r, _f, _v) \
	OS_REG_WRITE(_a, _r, \
		(OS_REG_READ(_a, _r) &~ (_f)) | (((_v) << _f##_S) & (_f)))
#define	OS_REG_SET_BIT(_a, _r, _f) \
	OS_REG_WRITE(_a, _r, OS_REG_READ(_a, _r) | (_f))
#define	OS_REG_CLR_BIT(_a, _r, _f) \
	OS_REG_WRITE(_a, _r, OS_REG_READ(_a, _r) &~ (_f))

/* 
 * Regulatory domain support.
 */

/*
 * Return the max allowed antenna gain based on the current
 * regulatory domain.
 */
extern	u_int ath_hal_getantennareduction(struct ath_hal *,
		HAL_CHANNEL *, u_int twiceGain);
/*
 * Return the test group for the specific channel based on
 * the current regulator domain.
 */
extern	u_int ath_hal_getctl(struct ath_hal *, HAL_CHANNEL *);
/*
 * Return whether or not a noise floor check is required
 * based on the current regulatory domain for the specified
 * channel.
 */
extern	u_int ath_hal_getnfcheckrequired(struct ath_hal *, HAL_CHANNEL *);

/*
 * Map a public channel definition to the corresponding
 * internal data structure.  This implicitly specifies
 * whether or not the specified channel is ok to use
 * based on the current regulatory domain constraints.
 */
extern	HAL_CHANNEL_INTERNAL *ath_hal_checkchannel(struct ath_hal *,
		const HAL_CHANNEL *);

/* system-configurable parameters */
extern	int ath_hal_dma_beacon_response_time;	/* in TU's */
extern	int ath_hal_sw_beacon_response_time;	/* in TU's */
extern	int ath_hal_additional_swba_backoff;	/* in TU's */

/* wait for the register contents to have the specified value */
extern	HAL_BOOL ath_hal_wait(struct ath_hal *, u_int reg,
		uint32_t mask, uint32_t val);

/* return the first n bits in val reversed */
extern	uint32_t ath_hal_reverseBits(uint32_t val, uint32_t n);

/* printf interfaces */
extern	void ath_hal_printf(struct ath_hal *, const char*, ...)
		__printflike(2,3);
extern	void ath_hal_vprintf(struct ath_hal *, const char*, __va_list)
		__printflike(2, 0);
extern	const char* ath_hal_ether_sprintf(const uint8_t *mac);

/* allocate and free memory */
extern	void *ath_hal_malloc(size_t);
extern	void ath_hal_free(void *);

/* common debugging interfaces */
#ifdef AH_DEBUG
#include "ah_debug.h"
extern	int ath_hal_debug;
extern	void HALDEBUG(struct ath_hal *ah, u_int mask, const char* fmt, ...)
	__printflike(3,4);
#else
#define HALDEBUG(_ah, __m, _fmt, ...)
#endif /* AH_DEBUG */

/*
 * Register logging definitions shared with ardecode.
 */
#include "ah_decode.h"

/*
 * Common assertion interface.  Note: it is a bad idea to generate
 * an assertion failure for any recoverable event.  Instead catch
 * the violation and, if possible, fix it up or recover from it; either
 * with an error return value or a diagnostic messages.  System software
 * does not panic unless the situation is hopeless.
 */
#ifdef AH_ASSERT
extern	void ath_hal_assert_failed(const char* filename,
		int lineno, const char* msg);

#define	HALASSERT(_x) do {					\
	if (!(_x)) {						\
		ath_hal_assert_failed(__FILE__, __LINE__, #_x);	\
	}							\
} while (0)
#else
#define	HALASSERT(_x)
#endif /* AH_ASSERT */

/*
 * Convert between microseconds and core system clocks.
 */
extern	u_int ath_hal_mac_clks(struct ath_hal *ah, u_int usecs);
extern	u_int ath_hal_mac_usec(struct ath_hal *ah, u_int clks);

/*
 * Generic get/set capability support.  Each chip overrides
 * this routine to support chip-specific capabilities.
 */
extern	HAL_STATUS ath_hal_getcapability(struct ath_hal *ah,
		HAL_CAPABILITY_TYPE type, uint32_t capability,
		uint32_t *result);
extern	HAL_BOOL ath_hal_setcapability(struct ath_hal *ah,
		HAL_CAPABILITY_TYPE type, uint32_t capability,
		uint32_t setting, HAL_STATUS *status);

/*
 * Diagnostic interface.  This is an open-ended interface that
 * is opaque to applications.  Diagnostic programs use this to
 * retrieve internal data structures, etc.  There is no guarantee
 * that calling conventions for calls other than HAL_DIAG_REVS
 * are stable between HAL releases; a diagnostic application must
 * use the HAL revision information to deal with ABI/API differences.
 *
 * NB: do not renumber these, certain codes are publicly used.
 */
enum {
	HAL_DIAG_REVS		= 0,	/* MAC/PHY/Radio revs */
	HAL_DIAG_EEPROM		= 1,	/* EEPROM contents */
	HAL_DIAG_EEPROM_EXP_11A	= 2,	/* EEPROM 5112 power exp for 11a */
	HAL_DIAG_EEPROM_EXP_11B	= 3,	/* EEPROM 5112 power exp for 11b */
	HAL_DIAG_EEPROM_EXP_11G	= 4,	/* EEPROM 5112 power exp for 11g */
	HAL_DIAG_ANI_CURRENT	= 5,	/* ANI current channel state */
	HAL_DIAG_ANI_OFDM	= 6,	/* ANI OFDM timing error stats */
	HAL_DIAG_ANI_CCK	= 7,	/* ANI CCK timing error stats */
	HAL_DIAG_ANI_STATS	= 8,	/* ANI statistics */
	HAL_DIAG_RFGAIN		= 9,	/* RfGain GAIN_VALUES */
	HAL_DIAG_RFGAIN_CURSTEP	= 10,	/* RfGain GAIN_OPTIMIZATION_STEP */
	HAL_DIAG_PCDAC		= 11,	/* PCDAC table */
	HAL_DIAG_TXRATES	= 12,	/* Transmit rate table */
	HAL_DIAG_REGS		= 13,	/* Registers */
	HAL_DIAG_ANI_CMD	= 14,	/* ANI issue command (XXX do not change!) */
	HAL_DIAG_SETKEY		= 15,	/* Set keycache backdoor */
	HAL_DIAG_RESETKEY	= 16,	/* Reset keycache backdoor */
	HAL_DIAG_EEREAD		= 17,	/* Read EEPROM word */
	HAL_DIAG_EEWRITE	= 18,	/* Write EEPROM word */
	/* 19 was HAL_DIAG_TXCONT, 20-23 were for radar */
	HAL_DIAG_REGREAD        = 24,   /* Reg reads */
	HAL_DIAG_REGWRITE       = 25,   /* Reg writes */
	HAL_DIAG_GET_REGBASE    = 26,   /* Get register base */
	HAL_DIAG_RDWRITE	= 27,	/* Write regulatory domain */
	HAL_DIAG_RDREAD		= 28,	/* Get regulatory domain */
	HAL_DIAG_FATALERR	= 29,	/* Read cached interrupt state */
	HAL_DIAG_11NCOMPAT	= 30,	/* 11n compatibility tweaks */
	HAL_DIAG_ANI_PARAMS	= 31,	/* ANI noise immunity parameters */
	HAL_DIAG_CHECK_HANGS	= 32,	/* check h/w hangs */
};

enum {
    HAL_BB_HANG_DFS		= 0x0001,
    HAL_BB_HANG_RIFS		= 0x0002,
    HAL_BB_HANG_RX_CLEAR	= 0x0004,
    HAL_BB_HANG_UNKNOWN		= 0x0080,

    HAL_MAC_HANG_SIG1		= 0x0100,
    HAL_MAC_HANG_SIG2		= 0x0200,
    HAL_MAC_HANG_UNKNOWN	= 0x8000,

    HAL_BB_HANGS = HAL_BB_HANG_DFS
		 | HAL_BB_HANG_RIFS
		 | HAL_BB_HANG_RX_CLEAR
		 | HAL_BB_HANG_UNKNOWN,
    HAL_MAC_HANGS = HAL_MAC_HANG_SIG1
		 | HAL_MAC_HANG_SIG2
		 | HAL_MAC_HANG_UNKNOWN,
};

/*
 * Device revision information.
 */
typedef struct {
	uint16_t	ah_devid;		/* PCI device ID */
	uint16_t	ah_subvendorid;		/* PCI subvendor ID */
	uint32_t	ah_macVersion;		/* MAC version id */
	uint16_t	ah_macRev;		/* MAC revision */
	uint16_t	ah_phyRev;		/* PHY revision */
	uint16_t	ah_analog5GhzRev;	/* 2GHz radio revision */
	uint16_t	ah_analog2GhzRev;	/* 5GHz radio revision */
} HAL_REVS;

/*
 * Argument payload for HAL_DIAG_SETKEY.
 */
typedef struct {
	HAL_KEYVAL	dk_keyval;
	uint16_t	dk_keyix;	/* key index */
	uint8_t		dk_mac[IEEE80211_ADDR_LEN];
	int		dk_xor;		/* XOR key data */
} HAL_DIAG_KEYVAL;

/*
 * Argument payload for HAL_DIAG_EEWRITE.
 */
typedef struct {
	uint16_t	ee_off;		/* eeprom offset */
	uint16_t	ee_data;	/* write data */
} HAL_DIAG_EEVAL;


typedef struct {
	u_int offset;		/* reg offset */
	uint32_t val;		/* reg value  */
} HAL_DIAG_REGVAL;

/*
 * 11n compatibility tweaks.
 */
#define	HAL_DIAG_11N_SERVICES	0x00000003
#define	HAL_DIAG_11N_SERVICES_S	0
#define	HAL_DIAG_11N_TXSTOMP	0x0000000c
#define	HAL_DIAG_11N_TXSTOMP_S	2

typedef struct {
	int		maxNoiseImmunityLevel;	/* [0..4] */
	int		totalSizeDesired[5];
	int		coarseHigh[5];
	int		coarseLow[5];
	int		firpwr[5];

	int		maxSpurImmunityLevel;	/* [0..7] */
	int		cycPwrThr1[8];

	int		maxFirstepLevel;	/* [0..2] */
	int		firstep[3];

	uint32_t	ofdmTrigHigh;
	uint32_t	ofdmTrigLow;
	int32_t		cckTrigHigh;
	int32_t		cckTrigLow;
	int32_t		rssiThrLow;
	int32_t		rssiThrHigh;

	int		period;			/* update listen period */
} HAL_ANI_PARAMS;

extern	HAL_BOOL ath_hal_getdiagstate(struct ath_hal *ah, int request,
			const void *args, uint32_t argsize,
			void **result, uint32_t *resultsize);

/*
 * Setup a h/w rate table for use.
 */
extern	void ath_hal_setupratetable(struct ath_hal *ah, HAL_RATE_TABLE *rt);

/*
 * Common routine for implementing getChanNoise api.
 */
extern	int16_t ath_hal_getChanNoise(struct ath_hal *ah, HAL_CHANNEL *chan);

/*
 * Initialization support.
 */
typedef struct {
	const uint32_t	*data;
	int		rows, cols;
} HAL_INI_ARRAY;

#define	HAL_INI_INIT(_ia, _data, _cols) do {			\
	(_ia)->data = (const uint32_t *)(_data);		\
	(_ia)->rows = sizeof(_data) / sizeof((_data)[0]);	\
	(_ia)->cols = (_cols);					\
} while (0)
#define	HAL_INI_VAL(_ia, _r, _c) \
	((_ia)->data[((_r)*(_ia)->cols) + (_c)])

/*
 * OS_DELAY() does a PIO READ on the PCI bus which allows
 * other cards' DMA reads to complete in the middle of our reset.
 */
#define DMA_YIELD(x) do {		\
	if ((++(x) % 64) == 0)		\
		OS_DELAY(1);		\
} while (0)

#define HAL_INI_WRITE_ARRAY(ah, regArray, col, regWr) do {             	\
	int r;								\
	for (r = 0; r < N(regArray); r++) {				\
		OS_REG_WRITE(ah, (regArray)[r][0], (regArray)[r][col]);	\
		DMA_YIELD(regWr);					\
	}								\
} while (0)

#define HAL_INI_WRITE_BANK(ah, regArray, bankData, regWr) do {		\
	int r;								\
	for (r = 0; r < N(regArray); r++) {				\
		OS_REG_WRITE(ah, (regArray)[r][0], (bankData)[r]);	\
		DMA_YIELD(regWr);					\
	}								\
} while (0)

extern	int ath_hal_ini_write(struct ath_hal *ah, const HAL_INI_ARRAY *ia,
		int col, int regWr);
extern	void ath_hal_ini_bank_setup(uint32_t data[], const HAL_INI_ARRAY *ia,
		int col);
extern	int ath_hal_ini_bank_write(struct ath_hal *ah, const HAL_INI_ARRAY *ia,
		const uint32_t data[], int regWr);

#define	WLAN_CTRL_FRAME_SIZE	(2+2+6+4)	/* ACK+FCS */
#endif /* _ATH_AH_INTERAL_H_ */
