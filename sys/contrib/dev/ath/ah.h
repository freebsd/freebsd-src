/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 * $Id: ah.h,v 1.35 2003/07/21 02:36:53 sam Exp $
 */

#ifndef _ATH_AH_H_
#define _ATH_AH_H_
/*
 * Atheros Hardware Access Layer
 *
 * Clients of the HAL call ath_hal_attach to obtain a reference to an ath_hal
 * structure for use with the device.  Hardware-related operations that
 * follow must call back into the HAL through interface, supplying the
 * reference as the first parameter.
 */
#include "ah_osdep.h"

/*
 * Status codes that may be returned by the HAL.  Note that
 * interfaces that return a status code set it only when an
 * error occurs--i.e. you cannot check it for success.
 */
typedef enum {
	HAL_OK = 0,		/* No error */
	HAL_ENXIO,		/* No hardware present */
	HAL_ENOMEM,		/* Memory allocation failed */
	HAL_EIO,		/* Hardware didn't respond as expected */
	HAL_EEMAGIC,		/* EEPROM magic number invalid */
	HAL_EEVERSION,		/* EEPROM version invalid */
	HAL_EELOCKED,		/* EEPROM unreadable */
	HAL_EEBADSUM,		/* EEPROM checksum invalid */
	HAL_EEREAD,		/* EEPROM read problem */
	HAL_EEBADMAC,		/* EEPROM mac address invalid */
	HAL_EESIZE,		/* EEPROM size not supported */
	HAL_EEWRITE,		/* Attempt to change write-locked EEPROM */
	HAL_EINVAL,		/* Invalid parameter to function */
	HAL_ENOTSUPP,		/* Hardware revision not supported */
	HAL_ESELFTEST,		/* Hardware self-test failed */
	HAL_EINPROGRESS,	/* Operation incomplete */
} HAL_STATUS;

typedef enum {
	AH_FALSE = 0,		/* NB: lots of code assumes false is zero */
	AH_TRUE  = 1,
} HAL_BOOL;

/* 
 * "States" for setting the LED.  These correspond to
 * the possible 802.11 operational states and there may
 * be a many-to-one mapping between these states and the
 * actual hardware states for the LED's (i.e. the hardware
 * may have fewer states).
 */
typedef enum {
	HAL_LED_INIT	= 0,
	HAL_LED_SCAN	= 1,
	HAL_LED_AUTH	= 2,
	HAL_LED_ASSOC	= 3,
	HAL_LED_RUN	= 4
} HAL_LED_STATE;

/*
 * Transmit queue types/numbers.  These are used to tag
 * each transmit queue in the hardware and to identify a set
 * of transmit queues for operations such as start/stop dma.
 */
typedef enum {
	HAL_TX_QUEUE_INACTIVE	= 0,		/* queue is inactive/unused */
	HAL_TX_QUEUE_DATA,			/* data xmit q's */
	HAL_TX_QUEUE_BEACON,			/* beacon xmit q */
	HAL_TX_QUEUE_CAB,			/* "crap after beacon" xmit q */
	HAL_TX_QUEUE_PSPOLL,			/* power-save poll xmit q */
} HAL_TX_QUEUE;

#define	HAL_NUM_TX_QUEUES	10		/* max possible # of queues */

/*
 * Transmit packet types.  This belongs in ah_desc.h, but
 * is here so we can give a proper type to various parameters
 * (and not require everyone include the file).
 *
 * NB: These values are intentionally assigned for
 *     direct use when setting up h/w descriptors.
 */
typedef enum {
	HAL_PKT_TYPE_NORMAL	= 0,
	HAL_PKT_TYPE_ATIM	= 1,
	HAL_PKT_TYPE_PSPOLL	= 2,
	HAL_PKT_TYPE_BEACON	= 3,
	HAL_PKT_TYPE_PROBE_RESP	= 4,
} HAL_PKT_TYPE;

/* Rx Filter Frame Types */
typedef enum {
	HAL_RX_FILTER_UCAST	= 0x00000001,	/* Allow unicast frames */
	HAL_RX_FILTER_MCAST	= 0x00000002,	/* Allow multicast frames */
	HAL_RX_FILTER_BCAST	= 0x00000004,	/* Allow broadcast frames */
	HAL_RX_FILTER_CONTROL	= 0x00000008,	/* Allow control frames */
	HAL_RX_FILTER_BEACON	= 0x00000010,	/* Allow beacon frames */
	HAL_RX_FILTER_PROM	= 0x00000020,	/* Promiscuous mode */
	HAL_RX_FILTER_PROBEREQ	= 0x00000080,	/* Allow probe request frames */
	HAL_RX_FILTER_PHYERR	= 0x00000100,	/* Allow phy errors */
	HAL_RX_FILTER_PHYRADAR	= 0x00000200,	/* Allow phy radar errors*/
} HAL_RX_FILTER;

typedef enum {
	HAL_PM_UNDEFINED	= 0,
	HAL_PM_AUTO		= 1,
	HAL_PM_AWAKE		= 2,
	HAL_PM_FULL_SLEEP	= 3,
	HAL_PM_NETWORK_SLEEP	= 4
} HAL_POWER_MODE;

/*
 * NOTE WELL:
 * These are mapped to take advantage of the common locations for many of
 * the bits on all of the currently supported MAC chips. This is to make
 * the ISR as efficient as possible, while still abstracting HW differences.
 * When new hardware breaks this commonality this enumerated type, as well
 * as the HAL functions using it, must be modified. All values are directly
 * mapped unless commented otherwise.
 */
typedef enum {
	HAL_INT_RX	= 0x00000001,	/* Non-common mapping */
	HAL_INT_RXDESC	= 0x00000002,
	HAL_INT_RXNOFRM	= 0x00000008,
	HAL_INT_RXEOL	= 0x00000010,
	HAL_INT_RXORN	= 0x00000020,
	HAL_INT_TX	= 0x00000040,	/* Non-common mapping */
	HAL_INT_TXDESC	= 0x00000080,
	HAL_INT_TXURN	= 0x00000800,
	HAL_INT_MIB	= 0x00001000,
	HAL_INT_RXPHY	= 0x00004000,
	HAL_INT_RXKCM	= 0x00008000,
	HAL_INT_SWBA	= 0x00010000,
	HAL_INT_BMISS	= 0x00040000,
	HAL_INT_BNR	= 0x00100000,	/* Non-common mapping */
	HAL_INT_GPIO	= 0x01000000,
	HAL_INT_FATAL	= 0x40000000,	/* Non-common mapping */
	HAL_INT_GLOBAL	= 0x80000000,	/* Set/clear IER */

	/* Interrupt bits that map directly to ISR/IMR bits */
	HAL_INT_COMMON  = HAL_INT_RXNOFRM
			| HAL_INT_RXDESC
			| HAL_INT_RXEOL
			| HAL_INT_RXORN
			| HAL_INT_TXURN
			| HAL_INT_TXDESC
			| HAL_INT_MIB
			| HAL_INT_RXPHY
			| HAL_INT_RXKCM
			| HAL_INT_SWBA
			| HAL_INT_BMISS
			| HAL_INT_GPIO,
	HAL_INT_NOCARD	= 0xffffffff	/* To signal the card was removed */
} HAL_INT;

typedef enum {
	HAL_RFGAIN_INACTIVE,
	HAL_RFGAIN_READ_REQUESTED,
	HAL_RFGAIN_NEED_CHANGE
} HAL_RFGAIN;

/*
 * Channels are specified by frequency.
 */
typedef struct {
	u_int16_t	channel;	/* setting in Mhz */
	u_int16_t	channelFlags;	/* see below */
} HAL_CHANNEL;

#define	CHANNEL_RAD_INT	0x0001	/* Radar interference detected on channel */
#define	CHANNEL_CW_INT	0x0002	/* CW interference detected on channel */
#define	CHANNEL_BUSY	0x0004	/* Busy, occupied or overlap with adjoin chan */
#define	CHANNEL_TURBO	0x0010	/* Turbo Channel */
#define	CHANNEL_CCK	0x0020	/* CCK channel */
#define	CHANNEL_OFDM	0x0040	/* OFDM channel */
#define	CHANNEL_2GHZ	0x0080	/* 2 GHz spectrum channel. */
#define	CHANNEL_5GHZ	0x0100	/* 5 GHz spectrum channel */
#define	CHANNEL_PASSIVE	0x0200	/* Only passive scan allowed in the channel */
#define	CHANNEL_DYN	0x0400	/* dynamic CCK-OFDM channel */

#define	CHANNEL_A	(CHANNEL_5GHZ|CHANNEL_OFDM)
#define	CHANNEL_B	(CHANNEL_2GHZ|CHANNEL_CCK)
#define	CHANNEL_PUREG	(CHANNEL_2GHZ|CHANNEL_OFDM)
#ifdef notdef
#define	CHANNEL_G	(CHANNEL_2GHZ|CHANNEL_DYN)
#else
#define	CHANNEL_G	(CHANNEL_2GHZ|CHANNEL_OFDM)
#endif
#define	CHANNEL_T	(CHANNEL_5GHZ|CHANNEL_OFDM|CHANNEL_TURBO)
#define	CHANNEL_ALL \
	(CHANNEL_OFDM|CHANNEL_CCK|CHANNEL_5GHZ|CHANNEL_2GHZ|CHANNEL_TURBO)
#define	CHANNEL_ALL_NOTURBO 	(CHANNEL_ALL &~ CHANNEL_TURBO)

typedef struct {
	u_int32_t	ackrcv_bad;
	u_int32_t	rts_bad;
	u_int32_t	rts_good;
	u_int32_t	fcs_bad;
	u_int32_t	beacons;
} HAL_MIB_STATS;

typedef u_int16_t HAL_CTRY_CODE;		/* country code */
typedef u_int16_t HAL_REG_DOMAIN;		/* regulatory domain code */

enum {
	CTRY_DEBUG	= 0x1ff,		/* debug country code */
	CTRY_DEFAULT	= 0			/* default country code */
};

enum {
	HAL_MODE_11A	= 0x001,
	HAL_MODE_TURBO	= 0x002,
	HAL_MODE_11B	= 0x004,
	HAL_MODE_PUREG	= 0x008,
#ifdef notdef
	HAL_MODE_11G	= 0x010,
#else
	HAL_MODE_11G	= 0x008,
#endif
	HAL_MODE_ALL	= 0xfff
};

typedef struct {
	u_int16_t	rateCount;
	u_int8_t	rateCodeToIndex[32];	/* back mapping */
	struct {
		u_int8_t	valid;		/* valid for rate control use */
		u_int8_t	phy;		/* CCK/OFDM/XR */
		u_int16_t	rateKbps;	/* transfer rate in kbs */
		u_int8_t	rateCode;	/* rate for h/w descriptors */
		u_int8_t	shortPreamble;	/* mask for enabling short
						 * preamble in CCK rate code */
		u_int8_t	dot11Rate;	/* value for supported rates
						 * info element of MLME */
		u_int8_t	controlRate;	/* index of next lower basic
						 * rate; used for dur. calcs */
	} info[32];
} HAL_RATE_TABLE;

typedef struct {
	u_int		rs_count;		/* number of valid entries */
	u_int8_t	rs_rates[32];		/* rates */
} HAL_RATE_SET;

typedef enum {
	HAL_ANT_VARIABLE,			/* variable by programming */
	HAL_ANT_FIXED_A,			/* fixed to 11a frequencies */
	HAL_ANT_FIXED_B,			/* fixed to 11b frequencies */
} HAL_ANT_SETTING;

typedef enum {
	HAL_M_STA	= 1,			/* infrastructure station */
	HAL_M_IBSS	= 0,			/* IBSS (adhoc) station */
	HAL_M_HOSTAP	= 6,			/* Software Access Point */
	HAL_M_MONITOR	= 8			/* Monitor mode */
} HAL_OPMODE;

typedef struct {
	int		wk_len;
	u_int8_t	wk_key[16];		/* XXX big enough for WEP */
} HAL_KEYVAL;

typedef enum {
	HAL_CIPHER_WEP,
	HAL_CIPHER_AES_CCM,
	HAL_CIPHER_CKIP
} HAL_CIPHER;

/*
 * Per-station beacon timer state.
 */
typedef struct {
	u_int32_t	bs_nexttbtt;		/* next beacon in TU */
	u_int32_t	bs_nextdtim;		/* next DTIM in TU */
	u_int16_t	bs_intval;		/* beacon interval/period */
	u_int8_t	bs_dtimperiod;
	u_int8_t	bs_cfpperiod;		/* # of DTIMs between CFPs */
	u_int16_t	bs_cfpmaxduration;	/* max CFP duration in TU */
	u_int16_t	bs_cfpduremain;		/* remaining CFP duration */
	u_int16_t	bs_timoffset;
	u_int16_t	bs_sleepduration;	/* max sleep duration */
	u_int16_t	bs_bmissthreshold;	/* beacon miss threshold */
} HAL_BEACON_STATE;

struct ath_desc;

/*
 * Hardware Access Layer (HAL) API.
 *
 * Clients of the HAL call ath_hal_attach to obtain a reference to an
 * ath_hal structure for use with the device.  Hardware-related operations
 * that follow must call back into the HAL through interface, supplying
 * the reference as the first parameter.
 */
struct ath_hal {
	u_int32_t	ah_magic;	/* consistency check magic number */
	u_int16_t	ah_devid;	/* PCI device ID */
	u_int16_t	ah_subvendorid;	/* PCI subvendor ID */
	HAL_SOFTC	ah_sc;		/* back pointer to driver/os state */
	HAL_BUS_TAG	ah_st;		/* params for register r+w */
	HAL_BUS_HANDLE	ah_sh;
	HAL_CTRY_CODE	ah_countryCode;

	const HAL_RATE_TABLE *(*ah_getRateTable)(struct ath_hal *, u_int mode);
	void		(*ah_detach)(struct ath_hal*);

	/* Reset functions */
	HAL_BOOL	(*ah_reset)(struct ath_hal *, HAL_OPMODE,
				HAL_CHANNEL *, HAL_BOOL bChannelChange,
				HAL_STATUS *status);
	HAL_BOOL	(*ah_setPCUConfig)(struct ath_hal *, HAL_OPMODE);
	HAL_BOOL	(*ah_perCalibration)(struct ath_hal*, HAL_CHANNEL *);

	/* Transmit functions */
	HAL_BOOL	(*ah_updateTxTrigLevel)(struct ath_hal*,
				HAL_BOOL incTrigLevel);
	int		(*ah_setupTxQueue)(struct ath_hal *, HAL_TX_QUEUE type,
				HAL_BOOL irq);
	HAL_BOOL	(*ah_releaseTxQueue)(struct ath_hal *ah, u_int q);
	HAL_BOOL	(*ah_resetTxQueue)(struct ath_hal *ah, u_int q);
	u_int32_t 	(*ah_getTxDP)(struct ath_hal*, u_int);
	HAL_BOOL	(*ah_setTxDP)(struct ath_hal*, u_int, u_int32_t txdp);
	HAL_BOOL	(*ah_startTxDma)(struct ath_hal*, u_int);
	HAL_BOOL	(*ah_stopTxDma)(struct ath_hal*, u_int);
	HAL_BOOL	(*ah_setupTxDesc)(struct ath_hal *, struct ath_desc *,
				u_int pktLen, u_int hdrLen,
				HAL_PKT_TYPE type, u_int txPower,
				u_int txRate0, u_int txTries0,
				u_int keyIx, u_int antMode, u_int flags,
				u_int rtsctsRate, u_int rtsctsDuration);
	HAL_BOOL	(*ah_setupXTxDesc)(struct ath_hal *, struct ath_desc *,
				HAL_BOOL shortPreamble,
				u_int txRate1, u_int txTries1,
				u_int txRate2, u_int txTries2,
				u_int txRate3, u_int txTries3);
	HAL_BOOL	(*ah_fillTxDesc)(struct ath_hal *, struct ath_desc *,
				u_int segLen, HAL_BOOL firstSeg,
				HAL_BOOL lastSeg);
	HAL_STATUS	(*ah_procTxDesc)(struct ath_hal *, struct ath_desc *);
	HAL_BOOL	(*ah_hasVEOL)(struct ath_hal *);

	/* Receive Functions */
	u_int32_t	(*ah_getRxDP)(struct ath_hal*);
	void		(*ah_setRxDP)(struct ath_hal*, u_int32_t rxdp);
	void		(*ah_enableReceive)(struct ath_hal*);
	HAL_BOOL	(*ah_stopDmaReceive)(struct ath_hal*);
	void		(*ah_startPcuReceive)(struct ath_hal*);
	void		(*ah_stopPcuReceive)(struct ath_hal*);
	void		(*ah_setMulticastFilter)(struct ath_hal*,
				u_int32_t filter0, u_int32_t filter1);
	HAL_BOOL	(*ah_setMulticastFilterIndex)(struct ath_hal*,
				u_int32_t index);
	HAL_BOOL	(*ah_clrMulticastFilterIndex)(struct ath_hal*,
				u_int32_t index);
	u_int32_t	(*ah_getRxFilter)(struct ath_hal*);
	void		(*ah_setRxFilter)(struct ath_hal*, u_int32_t);
	HAL_BOOL	(*ah_setupRxDesc)(struct ath_hal *, struct ath_desc *,
				u_int32_t size, u_int flags);
	HAL_STATUS	(*ah_procRxDesc)(struct ath_hal *, struct ath_desc *);
	void		(*ah_rxMonitor)(struct ath_hal *);

	/* Misc Functions */
	void		(*ah_dumpState)(struct ath_hal *);
	void		(*ah_dumpEeprom)(struct ath_hal *);
	void		(*ah_dumpRfGain)(struct ath_hal *);
	void		(*ah_dumpAni)(struct ath_hal *);
	void		(*ah_getMacAddress)(struct ath_hal *, u_int8_t *);
	HAL_BOOL	(*ah_setRegulatoryDomain)(struct ath_hal*,
				u_int16_t, HAL_STATUS *);
	void		(*ah_setLedState)(struct ath_hal*, HAL_LED_STATE);
	void		(*ah_writeAssocid)(struct ath_hal*,
				const u_int8_t *bssid, u_int16_t assocId,
				u_int16_t timOffset);
	u_int32_t	(*ah_gpioGet)(struct ath_hal*, u_int32_t gpio);
	void		(*ah_gpioSetIntr)(struct ath_hal*, u_int, u_int32_t);
	u_int32_t	(*ah_getTsf32)(struct ath_hal*);
	u_int64_t	(*ah_getTsf64)(struct ath_hal*);
	void		(*ah_resetTsf)(struct ath_hal*);
	u_int16_t	(*ah_getRegDomain)(struct ath_hal*);
	u_int		(*ah_getWirelessModes)(struct ath_hal*);
	HAL_BOOL	(*ah_getRfKill)(struct ath_hal*);
	u_int32_t	(*ah_getRandomSeed)(struct ath_hal*);
	HAL_BOOL	(*ah_detectCardPresent)(struct ath_hal*);
	void		(*ah_updateMibCounters)(struct ath_hal*, HAL_MIB_STATS*);
	HAL_BOOL	(*ah_isHwCipherSupported)(struct ath_hal*, HAL_CIPHER);
	HAL_RFGAIN	(*ah_getRfGain)(struct ath_hal*);
#if 0
	u_int32_t	(*ah_getCurRssi)(struct ath_hal*);
	u_int32_t	(*ah_getDefAntenna)(struct ath_hal*);
	void		(*ah_setDefAntenna)(struct ath_hal*, u_int32_t antenna);
#endif

	/* Key Cache Functions */
	u_int32_t	(*ah_getKeyCacheSize)(struct ath_hal*);
	HAL_BOOL	(*ah_resetKeyCacheEntry)(struct ath_hal*, u_int16_t);
	HAL_BOOL	(*ah_isKeyCacheEntryValid)(struct ath_hal *, u_int16_t);
	HAL_BOOL	(*ah_setKeyCacheEntry)(struct ath_hal*,
				u_int16_t, const HAL_KEYVAL *, u_int8_t *, int);
	HAL_BOOL	(*ah_setKeyCacheEntryMac)(struct ath_hal*,
				u_int16_t, u_int8_t *);

	/* Power Management Functions */
	HAL_BOOL	(*ah_setPowerMode)(struct ath_hal*,
				HAL_POWER_MODE mode, int setChip,
				u_int16_t sleepDuration);
	HAL_POWER_MODE	(*ah_getPowerMode)(struct ath_hal*);
	HAL_BOOL	(*ah_queryPSPollSupport)(struct ath_hal*);
	HAL_BOOL	(*ah_initPSPoll)(struct ath_hal*);
	HAL_BOOL	(*ah_enablePSPoll)(struct ath_hal *,
				u_int8_t *, u_int16_t);
	HAL_BOOL	(*ah_disablePSPoll)(struct ath_hal *);

	/* Beacon Management Functions */
	void		(*ah_beaconInit)(struct ath_hal *, HAL_OPMODE,
				u_int32_t, u_int32_t);
	void		(*ah_setStationBeaconTimers)(struct ath_hal*,
				const HAL_BEACON_STATE *, u_int32_t tsf,
				u_int32_t dtimCount, u_int32_t cfpCcount);
	void		(*ah_resetStationBeaconTimers)(struct ath_hal*);
	HAL_BOOL	(*ah_waitForBeaconDone)(struct ath_hal *,
				HAL_BUS_ADDR);

	/* Interrupt functions */
	HAL_BOOL	(*ah_isInterruptPending)(struct ath_hal*);
	HAL_BOOL	(*ah_getPendingInterrupts)(struct ath_hal*, HAL_INT *);
	HAL_INT		(*ah_getInterrupts)(struct ath_hal*);
	HAL_INT		(*ah_setInterrupts)(struct ath_hal*, HAL_INT);
};

/* 
 * Check the PCI vendor ID and device ID against Atheros' values
 * and return a printable description for any Atheros hardware.
 * AH_NULL is returned if the ID's do not describe Atheros hardware.
 */
extern	const char *ath_hal_probe(u_int16_t vendorid, u_int16_t devid);

/*
 * Attach the HAL for use with the specified device.  The device is
 * defined by the PCI device ID.  The caller provides an opaque pointer
 * to an upper-layer data structure (HAL_SOFTC) that is stored in the
 * HAL state block for later use.  Hardware register accesses are done
 * using the specified bus tag and handle.  On successful return a
 * reference to a state block is returned that must be supplied in all
 * subsequent HAL calls.  Storage associated with this reference is
 * dynamically allocated and must be freed by calling the ah_detach
 * method when the client is done.  If the attach operation fails a
 * null (AH_NULL) reference will be returned and a status code will
 * be returned if the status parameter is non-zero.
 */
extern	struct ath_hal *ath_hal_attach(u_int16_t devid, HAL_SOFTC,
		HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_STATUS* status);

/*
 * Return a list of channels available for use with the hardware.
 * The list is based on what the hardware is capable of, the specified
 * country code, the modeSelect mask, and whether or not outdoor
 * channels are to be permitted.
 *
 * The channel list is returned in the supplied array.  maxchans
 * defines the maximum size of this array.  nchans contains the actual
 * number of channels returned.  If a problem occurred or there were
 * no channels that met the criteria then AH_FALSE is returned.
 */
extern	HAL_BOOL ath_hal_init_channels(struct ath_hal *,
		HAL_CHANNEL *chans, u_int maxchans, u_int *nchans,
		HAL_CTRY_CODE cc, u_int16_t modeSelect, int enableOutdoor);

/*
 * Return bit mask of wireless modes supported by the hardware.
 */
extern	u_int ath_hal_getwirelessmodes(struct ath_hal *ah, HAL_CTRY_CODE cc);

/*
 * Return rate table for specified mode (11a, 11b, 11g, etc).
 */
extern	const HAL_RATE_TABLE *ath_hal_getratetable(struct ath_hal *,
		u_int mode);

/*
 * Calculate the transmit duration of a frame.
 */
extern u_int16_t ath_hal_computetxtime(struct ath_hal *,
		const HAL_RATE_TABLE *rates, u_int32_t frameLen,
		u_int16_t rateix, HAL_BOOL shortPreamble);

/*
 * Convert between IEEE channel number and channel frequency
 * using the specified channel flags; e.g. CHANNEL_2GHZ.
 */
extern	u_int ath_hal_mhz2ieee(u_int mhz, u_int flags);
extern	u_int ath_hal_ieee2mhz(u_int ieee, u_int flags);
#endif /* _ATH_AH_H_ */
