/*-
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */

/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _DEV_ATH_ATHVAR_H
#define _DEV_ATH_ATHVAR_H

#include <sys/taskqueue.h>

#include <contrib/dev/ath/ah.h>
#include <dev/ath/if_athioctl.h>

#define	ATH_TIMEOUT		1000

#define	ATH_RXBUF	40		/* number of RX buffers */
#define	ATH_TXBUF	60		/* number of TX buffers */
#define	ATH_TXDESC	8		/* number of descriptors per buffer */

/* driver-specific node */
struct ath_node {
	struct ieee80211_node st_node;	/* base class */
	u_int		an_tx_ok;	/* tx ok pkt */
	u_int		an_tx_err;	/* tx !ok pkt */
	u_int		an_tx_retr;	/* tx retry count */
	int		an_tx_upper;	/* tx upper rate req cnt */
	u_int		an_tx_antenna;	/* antenna for last good frame */
};

struct ath_buf {
	TAILQ_ENTRY(ath_buf)	bf_list;
	int			bf_nseg;
	bus_dmamap_t		bf_dmamap;	/* DMA map of the buffer */
	struct ath_desc		*bf_desc;	/* virtual addr of desc */
	bus_addr_t		bf_daddr;	/* physical addr of desc */
	struct mbuf		*bf_m;		/* mbuf for buf */
	struct ieee80211_node	*bf_node;	/* pointer to the node */
	bus_size_t		bf_mapsize;
#define	ATH_MAX_SCATTER		64
	bus_dma_segment_t	bf_segs[ATH_MAX_SCATTER];
};

struct ath_softc {
	struct ieee80211com	sc_ic;		/* IEEE 802.11 common */
	device_t		sc_dev;
	bus_space_tag_t		sc_st;		/* bus space tag */
	bus_space_handle_t	sc_sh;		/* bus space handle */
	bus_dma_tag_t		sc_dmat;	/* bus DMA tag */
	struct mtx		sc_mtx;		/* master lock (recursive) */
	struct ath_hal		*sc_ah;		/* Atheros HAL */
	unsigned int		sc_invalid  : 1,/* disable hardware accesses */
				sc_have11g  : 1,/* have 11g support */
				sc_doani    : 1,/* dynamic noise immunity */
				sc_probing  : 1;/* probing AP on beacon miss */
						/* rate tables */
	const HAL_RATE_TABLE	*sc_rates[IEEE80211_MODE_MAX];
	const HAL_RATE_TABLE	*sc_currates;	/* current rate table */
	enum ieee80211_phymode	sc_curmode;	/* current phy mode */
	u_int8_t		sc_rixmap[256];	/* IEEE to h/w rate table ix */
	HAL_INT			sc_imask;	/* interrupt mask copy */

	struct ath_desc		*sc_desc;	/* TX/RX descriptors */
	bus_dma_segment_t	sc_dseg;
	bus_dmamap_t		sc_ddmamap;	/* DMA map for descriptors */
	bus_addr_t		sc_desc_paddr;	/* physical addr of sc_desc */
	bus_addr_t		sc_desc_len;	/* size of sc_desc */

	struct task		sc_fataltask;	/* fatal int processing */
	struct task		sc_rxorntask;	/* rxorn int processing */

	TAILQ_HEAD(, ath_buf)	sc_rxbuf;	/* receive buffer */
	u_int32_t		*sc_rxlink;	/* link ptr in last RX desc */
	struct task		sc_rxtask;	/* rx int processing */

	u_int			sc_txhalq;	/* HAL q for outgoing frames */
	u_int32_t		*sc_txlink;	/* link ptr in last TX desc */
	int			sc_tx_timer;	/* transmit timeout */
	TAILQ_HEAD(, ath_buf)	sc_txbuf;	/* transmit buffer */
	struct mtx		sc_txbuflock;	/* txbuf lock */
	TAILQ_HEAD(, ath_buf)	sc_txq;		/* transmitting queue */
	struct mtx		sc_txqlock;	/* lock on txq and txlink */
	struct task		sc_txtask;	/* tx int processing */

	u_int			sc_bhalq;	/* HAL q for outgoing beacons */
	struct ath_buf		*sc_bcbuf;	/* beacon buffer */
	struct ath_buf		*sc_bufptr;	/* allocated buffer ptr */
	struct task		sc_swbatask;	/* swba int processing */
	struct task		sc_bmisstask;	/* bmiss int processing */

	struct callout		sc_cal_ch;	/* callout handle for cals */
	struct callout		sc_scan_ch;	/* callout handle for scan */
	struct ath_stats	sc_stats;	/* interface statistics */
};

int	ath_attach(u_int16_t, struct ath_softc *);
int	ath_detach(struct ath_softc *);
void	ath_resume(struct ath_softc *);
void	ath_suspend(struct ath_softc *);
void	ath_shutdown(struct ath_softc *);
void	ath_intr(void *);

/*
 * HAL definitions to comply with local coding convention.
 */
#define	ath_hal_reset(_ah, _opmode, _chan, _outdoor, _pstatus) \
	((*(_ah)->ah_reset)((_ah), (_opmode), (_chan), (_outdoor), (_pstatus)))
#define	ath_hal_getratetable(_ah, _mode) \
	((*(_ah)->ah_getRateTable)((_ah), (_mode)))
#define	ath_hal_getmac(_ah, _mac) \
	((*(_ah)->ah_getMacAddress)((_ah), (_mac)))
#define	ath_hal_detach(_ah) \
	((*(_ah)->ah_detach)((_ah)))
#define	ath_hal_intrset(_ah, _mask) \
	((*(_ah)->ah_setInterrupts)((_ah), (_mask)))
#define	ath_hal_intrget(_ah) \
	((*(_ah)->ah_getInterrupts)((_ah)))
#define	ath_hal_intrpend(_ah) \
	((*(_ah)->ah_isInterruptPending)((_ah)))
#define	ath_hal_getisr(_ah, _pmask) \
	((*(_ah)->ah_getPendingInterrupts)((_ah), (_pmask)))
#define	ath_hal_updatetxtriglevel(_ah, _inc) \
	((*(_ah)->ah_updateTxTrigLevel)((_ah), (_inc)))
#define	ath_hal_setpower(_ah, _mode, _sleepduration) \
	((*(_ah)->ah_setPowerMode)((_ah), (_mode), AH_TRUE, (_sleepduration)))
#define	ath_hal_keyreset(_ah, _ix) \
	((*(_ah)->ah_resetKeyCacheEntry)((_ah), (_ix)))
#define	ath_hal_keyset(_ah, _ix, _pk) \
	((*(_ah)->ah_setKeyCacheEntry)((_ah), (_ix), (_pk), NULL, AH_FALSE))
#define	ath_hal_keyisvalid(_ah, _ix) \
	(((*(_ah)->ah_isKeyCacheEntryValid)((_ah), (_ix))))
#define	ath_hal_keysetmac(_ah, _ix, _mac) \
	((*(_ah)->ah_setKeyCacheEntryMac)((_ah), (_ix), (_mac)))
#define	ath_hal_getrxfilter(_ah) \
	((*(_ah)->ah_getRxFilter)((_ah)))
#define	ath_hal_setrxfilter(_ah, _filter) \
	((*(_ah)->ah_setRxFilter)((_ah), (_filter)))
#define	ath_hal_setmcastfilter(_ah, _mfilt0, _mfilt1) \
	((*(_ah)->ah_setMulticastFilter)((_ah), (_mfilt0), (_mfilt1)))
#define	ath_hal_waitforbeacon(_ah, _bf) \
	((*(_ah)->ah_waitForBeaconDone)((_ah), (_bf)->bf_daddr))
#define	ath_hal_putrxbuf(_ah, _bufaddr) \
	((*(_ah)->ah_setRxDP)((_ah), (_bufaddr)))
#define	ath_hal_gettsf32(_ah) \
	((*(_ah)->ah_getTsf32)((_ah)))
#define	ath_hal_gettsf64(_ah) \
	((*(_ah)->ah_getTsf64)((_ah)))
#define	ath_hal_resettsf(_ah) \
	((*(_ah)->ah_resetTsf)((_ah)))
#define	ath_hal_rxena(_ah) \
	((*(_ah)->ah_enableReceive)((_ah)))
#define	ath_hal_puttxbuf(_ah, _q, _bufaddr) \
	((*(_ah)->ah_setTxDP)((_ah), (_q), (_bufaddr)))
#define	ath_hal_gettxbuf(_ah, _q) \
	((*(_ah)->ah_getTxDP)((_ah), (_q)))
#define	ath_hal_getrxbuf(_ah) \
	((*(_ah)->ah_getRxDP)((_ah)))
#define	ath_hal_txstart(_ah, _q) \
	((*(_ah)->ah_startTxDma)((_ah), (_q)))
#define	ath_hal_setchannel(_ah, _chan) \
	((*(_ah)->ah_setChannel)((_ah), (_chan)))
#define	ath_hal_calibrate(_ah, _chan) \
	((*(_ah)->ah_perCalibration)((_ah), (_chan)))
#define	ath_hal_setledstate(_ah, _state) \
	((*(_ah)->ah_setLedState)((_ah), (_state)))
#define	ath_hal_beaconinit(_ah, _opmode, _nextb, _bperiod) \
	((*(_ah)->ah_beaconInit)((_ah), (_opmode), (_nextb), (_bperiod)))
#define	ath_hal_beaconreset(_ah) \
	((*(_ah)->ah_resetStationBeaconTimers)((_ah)))
#define	ath_hal_beacontimers(_ah, _bs, _tsf, _dc, _cc) \
	((*(_ah)->ah_setStationBeaconTimers)((_ah), (_bs), (_tsf), \
		(_dc), (_cc)))
#define	ath_hal_setassocid(_ah, _bss, _associd) \
	((*(_ah)->ah_writeAssocid)((_ah), (_bss), (_associd), 0))
#define	ath_hal_setopmode(_ah, _opmode) \
	((*(_ah)->ah_setPCUConfig)((_ah), (_opmode)))
#define	ath_hal_stoptxdma(_ah, _qnum) \
	((*(_ah)->ah_stopTxDma)((_ah), (_qnum)))
#define	ath_hal_stoppcurecv(_ah) \
	((*(_ah)->ah_stopPcuReceive)((_ah)))
#define	ath_hal_startpcurecv(_ah) \
	((*(_ah)->ah_startPcuReceive)((_ah)))
#define	ath_hal_stopdmarecv(_ah) \
	((*(_ah)->ah_stopDmaReceive)((_ah)))
#define	ath_hal_dumpstate(_ah) \
	((*(_ah)->ah_dumpState)((_ah)))
#define	ath_hal_dumpeeprom(_ah) \
	((*(_ah)->ah_dumpEeprom)((_ah)))
#define	ath_hal_dumprfgain(_ah) \
	((*(_ah)->ah_dumpRfGain)((_ah)))
#define	ath_hal_dumpani(_ah) \
	((*(_ah)->ah_dumpAni)((_ah)))
#define	ath_hal_setuptxqueue(_ah, _type, _irq) \
	((*(_ah)->ah_setupTxQueue)((_ah), (_type), (_irq)))
#define	ath_hal_resettxqueue(_ah, _q) \
	((*(_ah)->ah_resetTxQueue)((_ah), (_q)))
#define	ath_hal_releasetxqueue(_ah, _q) \
	((*(_ah)->ah_releaseTxQueue)((_ah), (_q)))
#define	ath_hal_hasveol(_ah) \
	((*(_ah)->ah_hasVEOL)((_ah)))
#define	ath_hal_getrfgain(_ah) \
	((*(_ah)->ah_getRfGain)((_ah)))
#define	ath_hal_rxmonitor(_ah) \
	((*(_ah)->ah_rxMonitor)((_ah)))

#define	ath_hal_setupbeacondesc(_ah, _ds, _opmode, _flen, _hlen, \
		_rate, _antmode) \
	((*(_ah)->ah_setupBeaconDesc)((_ah), (_ds), (_opmode), \
		(_flen), (_hlen), (_rate), (_antmode)))
#define	ath_hal_setuprxdesc(_ah, _ds, _size, _intreq) \
	((*(_ah)->ah_setupRxDesc)((_ah), (_ds), (_size), (_intreq)))
#define	ath_hal_rxprocdesc(_ah, _ds) \
	((*(_ah)->ah_procRxDesc)((_ah), (_ds)))
#define	ath_hal_setuptxdesc(_ah, _ds, _plen, _hlen, _atype, _txpow, \
		_txr0, _txtr0, _keyix, _ant, _flags, \
		_rtsrate, _rtsdura) \
	((*(_ah)->ah_setupTxDesc)((_ah), (_ds), (_plen), (_hlen), (_atype), \
		(_txpow), (_txr0), (_txtr0), (_keyix), (_ant), \
		(_flags), (_rtsrate), (_rtsdura)))
#define	ath_hal_setupxtxdesc(_ah, _ds, _short, \
		_txr1, _txtr1, _txr2, _txtr2, _txr3, _txtr3) \
	((*(_ah)->ah_setupXTxDesc)((_ah), (_ds), (_short), \
		(_txr1), (_txtr1), (_txr2), (_txtr2), (_txr3), (_txtr3)))
#define	ath_hal_filltxdesc(_ah, _ds, _l, _first, _last) \
	((*(_ah)->ah_fillTxDesc)((_ah), (_ds), (_l), (_first), (_last)))
#define	ath_hal_txprocdesc(_ah, _ds) \
	((*(_ah)->ah_procTxDesc)((_ah), (_ds)))

#endif /* _DEV_ATH_ATHVAR_H */
