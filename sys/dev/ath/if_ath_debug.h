/*-
 * Copyright (c) 2002-2009 Sam Leffler, Errno Consulting
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
#ifndef	__IF_ATH_DEBUG_H__
#define	__IF_ATH_DEBUG_H__

#ifdef	ATH_DEBUG

enum { 
	ATH_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	ATH_DEBUG_XMIT_DESC	= 0x00000002,	/* xmit descriptors */
	ATH_DEBUG_RECV		= 0x00000004,	/* basic recv operation */
	ATH_DEBUG_RECV_DESC	= 0x00000008,	/* recv descriptors */
	ATH_DEBUG_RATE		= 0x00000010,	/* rate control */
	ATH_DEBUG_RESET		= 0x00000020,	/* reset processing */
	ATH_DEBUG_MODE		= 0x00000040,	/* mode init/setup */
	ATH_DEBUG_BEACON	= 0x00000080,	/* beacon handling */
	ATH_DEBUG_WATCHDOG	= 0x00000100,	/* watchdog timeout */
	ATH_DEBUG_INTR		= 0x00001000,	/* ISR */
	ATH_DEBUG_TX_PROC	= 0x00002000,	/* tx ISR proc */
	ATH_DEBUG_RX_PROC	= 0x00004000,	/* rx ISR proc */
	ATH_DEBUG_BEACON_PROC	= 0x00008000,	/* beacon ISR proc */
	ATH_DEBUG_CALIBRATE	= 0x00010000,	/* periodic calibration */
	ATH_DEBUG_KEYCACHE	= 0x00020000,	/* key cache management */
	ATH_DEBUG_STATE		= 0x00040000,	/* 802.11 state transitions */
	ATH_DEBUG_NODE		= 0x00080000,	/* node management */
	ATH_DEBUG_LED		= 0x00100000,	/* led management */
	ATH_DEBUG_FF		= 0x00200000,	/* fast frames */
	ATH_DEBUG_DFS		= 0x00400000,	/* DFS processing */
	ATH_DEBUG_TDMA		= 0x00800000,	/* TDMA processing */
	ATH_DEBUG_TDMA_TIMER	= 0x01000000,	/* TDMA timer processing */
	ATH_DEBUG_REGDOMAIN	= 0x02000000,	/* regulatory processing */
	ATH_DEBUG_SW_TX		= 0x04000000,	/* per-packet software TX */
	ATH_DEBUG_SW_TX_BAW	= 0x08000000,	/* BAW handling */
	ATH_DEBUG_SW_TX_CTRL	= 0x10000000,	/* queue control */
	ATH_DEBUG_SW_TX_AGGR	= 0x20000000,	/* aggregate TX */
	ATH_DEBUG_SW_TX_RETRIES	= 0x40000000,	/* software TX retries */
	ATH_DEBUG_FATAL		= 0x80000000,	/* fatal errors */
	ATH_DEBUG_ANY		= 0xffffffff
};

extern int	ath_debug;

#define	IFF_DUMPPKTS(sc, m) \
	((sc->sc_debug & (m)) || \
	    (sc->sc_ifp->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
#define	DPRINTF(sc, m, fmt, ...) do {				\
	if (sc->sc_debug & (m))					\
		device_printf(sc->sc_dev, fmt, __VA_ARGS__);		\
} while (0)
#define	KEYPRINTF(sc, ix, hk, mac) do {				\
	if (sc->sc_debug & ATH_DEBUG_KEYCACHE)			\
		ath_keyprint(sc, __func__, ix, hk, mac);	\
} while (0)

extern	void ath_printrxbuf(struct ath_softc *, const struct ath_buf *bf,
	u_int ix, int);
extern	void ath_printtxbuf(struct ath_softc *, const struct ath_buf *bf,
	u_int qnum, u_int ix, int done);
#else	/* ATH_DEBUG */
#define	IFF_DUMPPKTS(sc, m) \
	((sc->sc_ifp->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
#define	DPRINTF(sc, m, fmt, ...) do {				\
	(void) sc;						\
} while (0)
#define	KEYPRINTF(sc, k, ix, mac) do {				\
	(void) sc;						\
} while (0)
#endif	/* ATH_DEBUG */

#endif
