/*-
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
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
#include "diag.h"

#include "ah.h"
#include "ah_internal.h"
#include "ar5210/ar5210reg.h"

#include "dumpregs.h"

#define	N(a)	(sizeof(a) / sizeof(a[0]))

static struct dumpreg ar5210regs[] = {
    { AR_TXDP0,		"TXDP0",	DUMP_BASIC },
    { AR_TXDP1,		"TXDP1",	DUMP_BASIC },
    { AR_CR,		"CR",		DUMP_BASIC },
    { AR_RXDP,		"RXDP",		DUMP_BASIC },
    { AR_CFG,		"CFG",		DUMP_BASIC },
#if 0	/* read clears pending interrupts */
    { AR_ISR,		"ISR",		DUMP_INTERRUPT },
#endif
    { AR_IMR,		"IMR",		DUMP_BASIC },
    { AR_IER,		"IER",		DUMP_BASIC },
    { AR_BCR,		"BCR",		DUMP_BASIC },
    { AR_BSR,		"BSR",		DUMP_BASIC },
    { AR_TXCFG,		"TXCFG",	DUMP_BASIC },
    { AR_RXCFG,		"RXCFG",	DUMP_BASIC },
    { AR_MIBC,		"MIBC",		DUMP_BASIC },
    { AR_TOPS,		"TOPS",		DUMP_BASIC },
    { AR_RXNOFRM,	"RXNOFR",	DUMP_BASIC },
    { AR_TXNOFRM,	"TXNOFR",	DUMP_BASIC },
    { AR_RPGTO,		"RPGTO",	DUMP_BASIC },
    { AR_RFCNT,		"RFCNT",	DUMP_BASIC },
    { AR_MISC,		"MISC",		DUMP_BASIC },
    { AR_RC,		"RC",		DUMP_BASIC },
    { AR_SCR,		"SCR",		DUMP_BASIC },
    { AR_INTPEND,	"INTPEND",	DUMP_BASIC },
    { AR_SFR,		"SFR",		DUMP_BASIC },
    { AR_PCICFG,	"PCICFG",	DUMP_BASIC },
    { AR_GPIOCR,	"GPIOCR",	DUMP_BASIC },
#if 0
    { AR_GPIODO,	"GPIODO",	DUMP_BASIC },
    { AR_GPIODI,	"GPIODI",	DUMP_BASIC },
#endif
    { AR_SREV,		"SREV",		DUMP_BASIC },
    { AR_STA_ID0,	"STA_ID0",	DUMP_BASIC },
    { AR_STA_ID1,	"STA_ID1",	DUMP_BASIC },
    { AR_BSS_ID0,	"BSS_ID0",	DUMP_BASIC },
    { AR_BSS_ID1,	"BSS_ID1",	DUMP_BASIC },
    { AR_SLOT_TIME,	"SLOTTIME",	DUMP_BASIC },
    { AR_TIME_OUT,	"TIME_OUT",	DUMP_BASIC },
    { AR_RSSI_THR,	"RSSI_THR",	DUMP_BASIC },
    { AR_RETRY_LMT,	"RETRY_LM",	DUMP_BASIC },
    { AR_USEC,		"USEC",		DUMP_BASIC },
    { AR_BEACON,	"BEACON",	DUMP_BASIC },
    { AR_CFP_PERIOD,	"CFP_PER",	DUMP_BASIC },
    { AR_TIMER0,	"TIMER0",	DUMP_BASIC },
    { AR_TIMER1,	"TIMER1",	DUMP_BASIC },
    { AR_TIMER2,	"TIMER2",	DUMP_BASIC },
    { AR_TIMER3,	"TIMER3",	DUMP_BASIC },
    { AR_IFS0,		"IFS0",		DUMP_BASIC },
    { AR_IFS1,		"IFS1"	,	DUMP_BASIC },
    { AR_CFP_DUR,	"CFP_DUR",	DUMP_BASIC },
    { AR_RX_FILTER,	"RXFILTER",	DUMP_BASIC },
    { AR_MCAST_FIL0,	"MCAST_0",	DUMP_BASIC },
    { AR_MCAST_FIL1,	"MCAST_1",	DUMP_BASIC },
    { AR_TX_MASK0,	"TX_MASK0",	DUMP_BASIC },
    { AR_TX_MASK1,	"TX_MASK1",	DUMP_BASIC },
#if 0
    { AR_CLR_TMASK,	"CLR_TMASK",	DUMP_BASIC },
#endif
    { AR_TRIG_LEV,	"TRIG_LEV",	DUMP_BASIC },
    { AR_DIAG_SW,	"DIAG_SW",	DUMP_BASIC },
    { AR_TSF_L32,	"TSF_L32",	DUMP_BASIC },
    { AR_TSF_U32,	"TSF_U32",	DUMP_BASIC },
    { AR_LAST_TSTP,	"LAST_TST",	DUMP_BASIC },
    { AR_RETRY_CNT,	"RETRYCNT",	DUMP_BASIC },
    { AR_BACKOFF,	"BACKOFF",	DUMP_BASIC },
    { AR_NAV,		"NAV",		DUMP_BASIC },
    { AR_RTS_OK,	"RTS_OK",	DUMP_BASIC },
    { AR_RTS_FAIL,	"RTS_FAIL",	DUMP_BASIC },
    { AR_ACK_FAIL,	"ACK_FAIL",	DUMP_BASIC },
    { AR_FCS_FAIL,	"FCS_FAIL",	DUMP_BASIC },
    { AR_BEACON_CNT,	"BEAC_CNT",	DUMP_BASIC },
};

static __constructor void
ar5210_ctor(void)
{
#define	MAC5210	SREV(1,0), SREV(2,0)
	register_regs(ar5210regs, N(ar5210regs), MAC5210, PHYANY);
	register_keycache(64, MAC5210, PHYANY);

	register_range(0x9800, 0x9840, DUMP_BASEBAND, MAC5210, PHYANY);
}
