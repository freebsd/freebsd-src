/*-
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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

/*
 * ath statistics class.
 */
#include <sys/types.h>
#include <sys/file.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_var.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "ah.h"
#include "ah_desc.h"
#include "ieee80211_ioctl.h"
#include "ieee80211_radiotap.h"
#include "if_athioctl.h"

#include "athstats.h"

#define	NOTPRESENT	{ 0, "", "" }

#define	AFTER(prev)	((prev)+1)

static const struct fmt athstats[] = {
#define	S_INPUT		0
	{ 8,	"input",	"input",	"data frames received" },
#define	S_OUTPUT	AFTER(S_INPUT)
	{ 8,	"output",	"output",	"data frames transmit" },
#define	S_TX_ALTRATE	AFTER(S_OUTPUT)
	{ 7,	"altrate",	"altrate",	"tx frames with an alternate rate" },
#define	S_TX_SHORTRETRY	AFTER(S_TX_ALTRATE)
	{ 7,	"short",	"short",	"short on-chip tx retries" },
#define	S_TX_LONGRETRY	AFTER(S_TX_SHORTRETRY)
	{ 7,	"long",		"long",		"long on-chip tx retries" },
#define	S_TX_XRETRIES	AFTER(S_TX_LONGRETRY)
	{ 6,	"xretry",	"xretry",	"tx failed 'cuz too many retries" },
#define	S_MIB		AFTER(S_TX_XRETRIES)
	{ 5,	"mib",		"mib",		"mib overflow interrupts" },
#ifndef __linux__
#define	S_TX_LINEAR	AFTER(S_MIB)
	{ 5,	"txlinear",	"txlinear",	"tx linearized to cluster" },
#define	S_BSTUCK	AFTER(S_TX_LINEAR)
	{ 5,	"bstuck",	"bstuck",	"stuck beacon conditions" },
#define	S_INTRCOAL	AFTER(S_BSTUCK)
	{ 5,	"intrcoal",	"intrcoal",	"interrupts coalesced" },
#define	S_RATE		AFTER(S_INTRCOAL)
#else
#define	S_RATE		AFTER(S_MIB)
#endif
	{ 4,	"rate",		"rate",		"current transmit rate" },
#define	S_WATCHDOG	AFTER(S_RATE)
	{ 5,	"wdog",		"wdog",		"watchdog timeouts" },
#define	S_FATAL		AFTER(S_WATCHDOG)
	{ 5,	"fatal",	"fatal",	"hardware error interrupts" },
#define	S_BMISS		AFTER(S_FATAL)
	{ 5,	"bmiss",	"bmiss",	"beacon miss interrupts" },
#define	S_RXORN		AFTER(S_BMISS)
	{ 5,	"rxorn",	"rxorn",	"recv overrun interrupts" },
#define	S_RXEOL		AFTER(S_RXORN)
	{ 5,	"rxeol",	"rxeol",	"recv eol interrupts" },
#define	S_TXURN		AFTER(S_RXEOL)
	{ 5,	"txurn",	"txurn",	"txmit underrun interrupts" },
#define	S_TX_MGMT	AFTER(S_TXURN)
	{ 5,	"txmgt",	"txmgt",	"tx management frames" },
#define	S_TX_DISCARD	AFTER(S_TX_MGMT)
	{ 5,	"txdisc",	"txdisc",	"tx frames discarded prior to association" },
#define	S_TX_INVALID	AFTER(S_TX_DISCARD)
	{ 5,	"txinv",	"txinv",	"tx invalid (19)" },
#define	S_TX_QSTOP	AFTER(S_TX_INVALID)
	{ 5,	"qstop",	"qstop",	"tx stopped 'cuz no xmit buffer" },
#define	S_TX_ENCAP	AFTER(S_TX_QSTOP)
	{ 5,	"txencode",	"txencode",	"tx encapsulation failed" },
#define	S_TX_NONODE	AFTER(S_TX_ENCAP)
	{ 5,	"txnonode",	"txnonode",	"tx failed 'cuz no node" },
#define	S_TX_NOMBUF	AFTER(S_TX_NONODE)
	{ 5,	"txnombuf",	"txnombuf",	"tx failed 'cuz mbuf allocation failed" },
#ifndef __linux__
#define	S_TX_NOMCL	AFTER(S_TX_NOMBUF)
	{ 5,	"txnomcl",	"txnomcl",	"tx failed 'cuz cluster allocation failed" },
#define	S_TX_FIFOERR	AFTER(S_TX_NOMCL)
#else
#define	S_TX_FIFOERR	AFTER(S_TX_NOMBUF)
#endif
	{ 5,	"efifo",	"efifo",	"tx failed 'cuz FIFO underrun" },
#define	S_TX_FILTERED	AFTER(S_TX_FIFOERR)
	{ 5,	"efilt",	"efilt",	"tx failed 'cuz destination filtered" },
#define	S_TX_BADRATE	AFTER(S_TX_FILTERED)
	{ 5,	"txbadrate",	"txbadrate",	"tx failed 'cuz bogus xmit rate" },
#define	S_TX_NOACK	AFTER(S_TX_BADRATE)
	{ 5,	"noack",	"noack",	"tx frames with no ack marked" },
#define	S_TX_RTS	AFTER(S_TX_NOACK)
	{ 5,	"rts",		"rts",		"tx frames with rts enabled" },
#define	S_TX_CTS	AFTER(S_TX_RTS)
	{ 5,	"cts",		"cts",		"tx frames with cts enabled" },
#define	S_TX_SHORTPRE	AFTER(S_TX_CTS)
	{ 5,	"shpre",	"shpre",	"tx frames with short preamble" },
#define	S_TX_PROTECT	AFTER(S_TX_SHORTPRE)
	{ 5,	"protect",	"protect",	"tx frames with 11g protection" },
#define	S_RX_ORN	AFTER(S_TX_PROTECT)
	{ 5,	"rxorn",	"rxorn",	"rx failed 'cuz of desc overrun" },
#define	S_RX_CRC_ERR	AFTER(S_RX_ORN)
	{ 6,	"crcerr",	"crcerr",	"rx failed 'cuz of bad CRC" },
#define	S_RX_FIFO_ERR	AFTER(S_RX_CRC_ERR)
	{ 5,	"rxfifo",	"rxfifo",	"rx failed 'cuz of FIFO overrun" },
#define	S_RX_CRYPTO_ERR	AFTER(S_RX_FIFO_ERR)
	{ 5,	"crypt",	"crypt",	"rx failed 'cuz decryption" },
#define	S_RX_MIC_ERR	AFTER(S_RX_CRYPTO_ERR)
	{ 4,	"mic",		"mic",		"rx failed 'cuz MIC failure" },
#define	S_RX_TOOSHORT	AFTER(S_RX_MIC_ERR)
	{ 5,	"rxshort",	"rxshort",	"rx failed 'cuz frame too short" },
#define	S_RX_NOMBUF	AFTER(S_RX_TOOSHORT)
	{ 5,	"rxnombuf",	"rxnombuf",	"rx setup failed 'cuz no mbuf" },
#define	S_RX_MGT	AFTER(S_RX_NOMBUF)
	{ 5,	"rxmgt",	"rxmgt",	"rx management frames" },
#define	S_RX_CTL	AFTER(S_RX_MGT)
	{ 5,	"rxctl",	"rxctl",	"rx control frames" },
#define	S_RX_PHY_ERR	AFTER(S_RX_CTL)
	{ 7,	"phyerr",	"phyerr",	"rx failed 'cuz of PHY err" },
#define	S_RX_PHY_UNDERRUN		AFTER(S_RX_PHY_ERR)
	{ 6,	"phyund",	"phyund",	"transmit underrun" },
#define	S_RX_PHY_TIMING			AFTER(S_RX_PHY_UNDERRUN)
	{ 6,	"phytim",	"phytim",	"timing error" },
#define	S_RX_PHY_PARITY			AFTER(S_RX_PHY_TIMING)
	{ 6,	"phypar",	"phypar",	"illegal parity" },
#define	S_RX_PHY_RATE			AFTER(S_RX_PHY_PARITY)
	{ 6,	"phyrate",	"phyrate",	"illegal rate" },
#define	S_RX_PHY_LENGTH			AFTER(S_RX_PHY_RATE)
	{ 6,	"phylen",	"phylen",	"illegal length" },
#define	S_RX_PHY_RADAR			AFTER(S_RX_PHY_LENGTH)
	{ 6,	"phyradar",	"phyradar",	"radar detect" },
#define	S_RX_PHY_SERVICE		AFTER(S_RX_PHY_RADAR)
	{ 6,	"physervice",	"physervice",	"illegal service" },
#define	S_RX_PHY_TOR			AFTER(S_RX_PHY_SERVICE)
	{ 6,	"phytor",	"phytor",	"transmit override receive" },
#define	S_RX_PHY_OFDM_TIMING		AFTER(S_RX_PHY_TOR)
	{ 6,	"ofdmtim",	"ofdmtim",	"OFDM timing" },
#define	S_RX_PHY_OFDM_SIGNAL_PARITY	AFTER(S_RX_PHY_OFDM_TIMING)
	{ 6,	"ofdmsig",	"ofdmsig",	"OFDM illegal parity" },
#define	S_RX_PHY_OFDM_RATE_ILLEGAL	AFTER(S_RX_PHY_OFDM_SIGNAL_PARITY)
	{ 6,	"ofdmrate",	"ofdmrate",	"OFDM illegal rate" },
#define	S_RX_PHY_OFDM_POWER_DROP	AFTER(S_RX_PHY_OFDM_RATE_ILLEGAL)
	{ 6,	"ofdmpow",	"ofdmpow",	"OFDM power drop" },
#define	S_RX_PHY_OFDM_SERVICE		AFTER(S_RX_PHY_OFDM_POWER_DROP)
	{ 6,	"ofdmservice",	"ofdmservice",	"OFDM illegal service" },
#define	S_RX_PHY_OFDM_RESTART		AFTER(S_RX_PHY_OFDM_SERVICE)
	{ 6,	"ofdmrestart",	"ofdmrestart",	"OFDM restart" },
#define	S_RX_PHY_CCK_TIMING		AFTER(S_RX_PHY_OFDM_RESTART)
	{ 6,	"ccktim",	"ccktim",	"CCK timing" },
#define	S_RX_PHY_CCK_HEADER_CRC		AFTER(S_RX_PHY_CCK_TIMING)
	{ 6,	"cckhead",	"cckhead",	"CCK header crc" },
#define	S_RX_PHY_CCK_RATE_ILLEGAL	AFTER(S_RX_PHY_CCK_HEADER_CRC)
	{ 6,	"cckrate",	"cckrate",	"CCK illegal rate" },
#define	S_RX_PHY_CCK_SERVICE		AFTER(S_RX_PHY_CCK_RATE_ILLEGAL)
	{ 6,	"cckservice",	"cckservice",	"CCK illegal service" },
#define	S_RX_PHY_CCK_RESTART		AFTER(S_RX_PHY_CCK_SERVICE)
	{ 6,	"cckrestar",	"cckrestar",	"CCK restart" },
#define	S_BE_NOMBUF	AFTER(S_RX_PHY_CCK_RESTART)
	{ 4,	"benombuf",	"benombuf",	"beacon setup failed 'cuz no mbuf" },
#define	S_BE_XMIT	AFTER(S_BE_NOMBUF)
	{ 7,	"bexmit",	"bexmit",	"beacons transmitted" },
#define	S_PER_CAL	AFTER(S_BE_XMIT)
	{ 4,	"pcal",		"pcal",		"periodic calibrations" },
#define	S_PER_CALFAIL	AFTER(S_PER_CAL)
	{ 4,	"pcalf",	"pcalf",	"periodic calibration failures" },
#define	S_PER_RFGAIN	AFTER(S_PER_CALFAIL)
	{ 4,	"prfga",	"prfga",	"rfgain value change" },
#if ATH_SUPPORT_TDMA
#define	S_TDMA_UPDATE	AFTER(S_PER_RFGAIN)
	{ 5,	"tdmau",	"tdmau",	"TDMA slot timing updates" },
#define	S_TDMA_TIMERS	AFTER(S_TDMA_UPDATE)
	{ 5,	"tdmab",	"tdmab",	"TDMA slot update set beacon timers" },
#define	S_TDMA_TSF	AFTER(S_TDMA_TIMERS)
	{ 5,	"tdmat",	"tdmat",	"TDMA slot update set TSF" },
#define	S_RATE_CALLS	AFTER(S_TDMA_TSF)
#else
#define	S_RATE_CALLS	AFTER(S_PER_RFGAIN)
#endif
	{ 5,	"ratec",	"ratec",	"rate control checks" },
#define	S_RATE_RAISE	AFTER(S_RATE_CALLS)
	{ 5,	"rate+",	"rate+",	"rate control raised xmit rate" },
#define	S_RATE_DROP	AFTER(S_RATE_RAISE)
	{ 5,	"rate-",	"rate-",	"rate control dropped xmit rate" },
#define	S_TX_RSSI	AFTER(S_RATE_DROP)
	{ 4,	"arssi",	"arssi",	"rssi of last ack" },
#define	S_RX_RSSI	AFTER(S_TX_RSSI)
	{ 4,	"rssi",		"rssi",		"avg recv rssi" },
#define	S_RX_NOISE	AFTER(S_RX_RSSI)
	{ 5,	"noise",	"noise",	"rx noise floor" },
#define	S_BMISS_PHANTOM	AFTER(S_RX_NOISE)
	{ 5,	"bmissphantom",	"bmissphantom",	"phantom beacon misses" },
#define	S_TX_RAW	AFTER(S_BMISS_PHANTOM)
	{ 5,	"txraw",	"txraw",	"tx frames through raw api" },
#define	S_RX_TOOBIG	AFTER(S_TX_RAW)
	{ 5,	"rx2big",	"rx2big",	"rx failed 'cuz frame too large"  },
#ifndef __linux__
#define	S_CABQ_XMIT	AFTER(S_RX_TOOBIG)
	{ 5,	"cabxmit",	"cabxmit",	"cabq frames transmitted" },
#define	S_CABQ_BUSY	AFTER(S_CABQ_XMIT)
	{ 5,	"cabqbusy",	"cabqbusy",	"cabq xmit overflowed beacon interval" },
#define	S_TX_NODATA	AFTER(S_CABQ_BUSY)
	{ 5,	"txnodata",	"txnodata",	"tx discarded empty frame" },
#define	S_TX_BUSDMA	AFTER(S_TX_NODATA)
	{ 5,	"txbusdma",	"txbusdma",	"tx failed for dma resrcs" },
#define	S_RX_BUSDMA	AFTER(S_TX_BUSDMA)
	{ 5,	"rxbusdma",	"rxbusdma",	"rx setup failed for dma resrcs" },
#define	S_FF_TXOK	AFTER(S_RX_BUSDMA)
#else
#define	S_FF_TXOK	AFTER(S_RX_PHY_UNDERRUN)
#endif
	{ 5,	"fftxok",	"fftxok",	"fast frames xmit successfully" },
#define	S_FF_TXERR	AFTER(S_FF_TXOK)
	{ 5,	"fftxerr",	"fftxerr",	"fast frames not xmit due to error" },
#define	S_FF_RX		AFTER(S_FF_TXERR)
	{ 5,	"ffrx",		"ffrx",		"fast frames received" },
#define	S_FF_FLUSH	AFTER(S_FF_RX)
	{ 5,	"ffflush",	"ffflush",	"fast frames flushed from staging q" },
#define	S_TX_QFULL	AFTER(S_FF_FLUSH)
	{ 5,	"txqfull",	"txqfull",	"tx discarded 'cuz queue is full" },
#define	S_ANT_DEFSWITCH	AFTER(S_TX_QFULL)
	{ 5,	"defsw",	"defsw",	"switched default/rx antenna" },
#define	S_ANT_TXSWITCH	AFTER(S_ANT_DEFSWITCH)
	{ 5,	"txsw",		"txsw",		"tx used alternate antenna" },
#define	S_ANT_TX0	AFTER(S_ANT_TXSWITCH)
	{ 8,	"tx0",	"ant0(tx)",	"frames tx on antenna 0" },
#define	S_ANT_TX1	AFTER(S_ANT_TX0)
	{ 8,	"tx1",	"ant1(tx)",	"frames tx on antenna 1"  },
#define	S_ANT_TX2	AFTER(S_ANT_TX1)
	{ 8,	"tx2",	"ant2(tx)",	"frames tx on antenna 2"  },
#define	S_ANT_TX3	AFTER(S_ANT_TX2)
	{ 8,	"tx3",	"ant3(tx)",	"frames tx on antenna 3"  },
#define	S_ANT_TX4	AFTER(S_ANT_TX3)
	{ 8,	"tx4",	"ant4(tx)",	"frames tx on antenna 4"  },
#define	S_ANT_TX5	AFTER(S_ANT_TX4)
	{ 8,	"tx5",	"ant5(tx)",	"frames tx on antenna 5"  },
#define	S_ANT_TX6	AFTER(S_ANT_TX5)
	{ 8,	"tx6",	"ant6(tx)",	"frames tx on antenna 6"  },
#define	S_ANT_TX7	AFTER(S_ANT_TX6)
	{ 8,	"tx7",	"ant7(tx)",	"frames tx on antenna 7"  },
#define	S_ANT_RX0	AFTER(S_ANT_TX7)
	{ 8,	"rx0",	"ant0(rx)",	"frames rx on antenna 0"  },
#define	S_ANT_RX1	AFTER(S_ANT_RX0)
	{ 8,	"rx1",	"ant1(rx)",	"frames rx on antenna 1"   },
#define	S_ANT_RX2	AFTER(S_ANT_RX1)
	{ 8,	"rx2",	"ant2(rx)",	"frames rx on antenna 2"   },
#define	S_ANT_RX3	AFTER(S_ANT_RX2)
	{ 8,	"rx3",	"ant3(rx)",	"frames rx on antenna 3"   },
#define	S_ANT_RX4	AFTER(S_ANT_RX3)
	{ 8,	"rx4",	"ant4(rx)",	"frames rx on antenna 4"   },
#define	S_ANT_RX5	AFTER(S_ANT_RX4)
	{ 8,	"rx5",	"ant5(rx)",	"frames rx on antenna 5"   },
#define	S_ANT_RX6	AFTER(S_ANT_RX5)
	{ 8,	"rx6",	"ant6(rx)",	"frames rx on antenna 6"   },
#define	S_ANT_RX7	AFTER(S_ANT_RX6)
	{ 8,	"rx7",	"ant7(rx)",	"frames rx on antenna 7"   },
#define	S_TX_SIGNAL	AFTER(S_ANT_RX7)
	{ 4,	"asignal",	"asig",	"signal of last ack (dBm)" },
#define	S_RX_SIGNAL	AFTER(S_TX_SIGNAL)
	{ 4,	"signal",	"sig",	"avg recv signal (dBm)" },
};
#define	S_PHY_MIN	S_RX_PHY_UNDERRUN
#define	S_PHY_MAX	S_RX_PHY_CCK_RESTART
#define	S_LAST		S_ANT_TX0
#define	S_MAX	S_ANT_RX7+1

struct _athstats {
	struct ath_stats ath;
};

struct athstatfoo_p {
	struct athstatfoo base;
	int s;
	int optstats;
	struct ifreq ifr;
	struct ath_diag atd;
	struct _athstats cur;
	struct _athstats total;
};

static void
ath_setifname(struct athstatfoo *wf0, const char *ifname)
{
	struct athstatfoo_p *wf = (struct athstatfoo_p *) wf0;

	strncpy(wf->ifr.ifr_name, ifname, sizeof (wf->ifr.ifr_name));
}

static void
ath_collect(struct athstatfoo_p *wf, struct _athstats *stats)
{
	wf->ifr.ifr_data = (caddr_t) &stats->ath;
	if (ioctl(wf->s, SIOCGATHSTATS, &wf->ifr) < 0)
		err(1, wf->ifr.ifr_name);
}

static void
ath_collect_cur(struct statfoo *sf)
{
	struct athstatfoo_p *wf = (struct athstatfoo_p *) sf;

	ath_collect(wf, &wf->cur);
}

static void
ath_collect_tot(struct statfoo *sf)
{
	struct athstatfoo_p *wf = (struct athstatfoo_p *) sf;

	ath_collect(wf, &wf->total);
}

static void
ath_update_tot(struct statfoo *sf)
{
	struct athstatfoo_p *wf = (struct athstatfoo_p *) sf;

	wf->total = wf->cur;
}

static int
ath_get_curstat(struct statfoo *sf, int s, char b[], size_t bs)
{
	struct athstatfoo_p *wf = (struct athstatfoo_p *) sf;
#define	STAT(x) \
	snprintf(b, bs, "%u", wf->cur.ath.ast_##x - wf->total.ath.ast_##x); return 1
#define	PHY(x) \
	snprintf(b, bs, "%u", wf->cur.ath.ast_rx_phy[x] - wf->total.ath.ast_rx_phy[x]); return 1
#define	TXANT(x) \
	snprintf(b, bs, "%u", wf->cur.ath.ast_ant_tx[x] - wf->total.ath.ast_ant_tx[x]); return 1
#define	RXANT(x) \
	snprintf(b, bs, "%u", wf->cur.ath.ast_ant_rx[x] - wf->total.ath.ast_ant_rx[x]); return 1

	switch (s) {
	case S_INPUT:
		snprintf(b, bs, "%lu",
		    (wf->cur.ath.ast_rx_packets - wf->total.ath.ast_rx_packets) -
		    (wf->cur.ath.ast_rx_mgt - wf->total.ath.ast_rx_mgt));
		return 1;
	case S_OUTPUT:
		snprintf(b, bs, "%lu",
		    wf->cur.ath.ast_tx_packets - wf->total.ath.ast_tx_packets);
		return 1;
	case S_RATE:
		snprintf(b, bs, "%uM", wf->cur.ath.ast_tx_rate / 2);
		return 1;
	case S_WATCHDOG:	STAT(watchdog);
	case S_FATAL:		STAT(hardware);
	case S_BMISS:		STAT(bmiss);
	case S_BMISS_PHANTOM:	STAT(bmiss_phantom);
#ifdef S_BSTUCK
	case S_BSTUCK:		STAT(bstuck);
#endif
	case S_RXORN:		STAT(rxorn);
	case S_RXEOL:		STAT(rxeol);
	case S_TXURN:		STAT(txurn);
	case S_MIB:		STAT(mib);
#ifdef S_INTRCOAL
	case S_INTRCOAL:	STAT(intrcoal);
#endif
	case S_TX_MGMT:		STAT(tx_mgmt);
	case S_TX_DISCARD:	STAT(tx_discard);
	case S_TX_QSTOP:	STAT(tx_qstop);
	case S_TX_ENCAP:	STAT(tx_encap);
	case S_TX_NONODE:	STAT(tx_nonode);
	case S_TX_NOMBUF:	STAT(tx_nombuf);
#ifdef S_TX_NOMCL
	case S_TX_NOMCL:	STAT(tx_nomcl);
	case S_TX_LINEAR:	STAT(tx_linear);
	case S_TX_NODATA:	STAT(tx_nodata);
	case S_TX_BUSDMA:	STAT(tx_busdma);
#endif
	case S_TX_XRETRIES:	STAT(tx_xretries);
	case S_TX_FIFOERR:	STAT(tx_fifoerr);
	case S_TX_FILTERED:	STAT(tx_filtered);
	case S_TX_SHORTRETRY:	STAT(tx_shortretry);
	case S_TX_LONGRETRY:	STAT(tx_longretry);
	case S_TX_BADRATE:	STAT(tx_badrate);
	case S_TX_NOACK:	STAT(tx_noack);
	case S_TX_RTS:		STAT(tx_rts);
	case S_TX_CTS:		STAT(tx_cts);
	case S_TX_SHORTPRE:	STAT(tx_shortpre);
	case S_TX_ALTRATE:	STAT(tx_altrate);
	case S_TX_PROTECT:	STAT(tx_protect);
	case S_RX_NOMBUF:	STAT(rx_nombuf);
#ifdef S_RX_BUSDMA
	case S_RX_BUSDMA:	STAT(rx_busdma);
#endif
	case S_RX_ORN:		STAT(rx_orn);
	case S_RX_CRC_ERR:	STAT(rx_crcerr);
	case S_RX_FIFO_ERR: 	STAT(rx_fifoerr);
	case S_RX_CRYPTO_ERR: 	STAT(rx_badcrypt);
	case S_RX_MIC_ERR:	STAT(rx_badmic);
	case S_RX_PHY_ERR:	STAT(rx_phyerr);
	case S_RX_PHY_UNDERRUN:	PHY(HAL_PHYERR_UNDERRUN);
	case S_RX_PHY_TIMING:	PHY(HAL_PHYERR_TIMING);
	case S_RX_PHY_PARITY:	PHY(HAL_PHYERR_PARITY);
	case S_RX_PHY_RATE:	PHY(HAL_PHYERR_RATE);
	case S_RX_PHY_LENGTH:	PHY(HAL_PHYERR_LENGTH);
	case S_RX_PHY_RADAR:	PHY(HAL_PHYERR_RADAR);
	case S_RX_PHY_SERVICE:	PHY(HAL_PHYERR_SERVICE);
	case S_RX_PHY_TOR:	PHY(HAL_PHYERR_TOR);
	case S_RX_PHY_OFDM_TIMING:	  PHY(HAL_PHYERR_OFDM_TIMING);
	case S_RX_PHY_OFDM_SIGNAL_PARITY: PHY(HAL_PHYERR_OFDM_SIGNAL_PARITY);
	case S_RX_PHY_OFDM_RATE_ILLEGAL:  PHY(HAL_PHYERR_OFDM_RATE_ILLEGAL);
	case S_RX_PHY_OFDM_POWER_DROP:	  PHY(HAL_PHYERR_OFDM_POWER_DROP);
	case S_RX_PHY_OFDM_SERVICE:	  PHY(HAL_PHYERR_OFDM_SERVICE);
	case S_RX_PHY_OFDM_RESTART:	  PHY(HAL_PHYERR_OFDM_RESTART);
	case S_RX_PHY_CCK_TIMING:	  PHY(HAL_PHYERR_CCK_TIMING);
	case S_RX_PHY_CCK_HEADER_CRC:	  PHY(HAL_PHYERR_CCK_HEADER_CRC);
	case S_RX_PHY_CCK_RATE_ILLEGAL:	  PHY(HAL_PHYERR_CCK_RATE_ILLEGAL);
	case S_RX_PHY_CCK_SERVICE:	  PHY(HAL_PHYERR_CCK_SERVICE);
	case S_RX_PHY_CCK_RESTART:	  PHY(HAL_PHYERR_CCK_RESTART);
	case S_RX_TOOSHORT:	STAT(rx_tooshort);
	case S_RX_TOOBIG:	STAT(rx_toobig);
	case S_RX_MGT:		STAT(rx_mgt);
	case S_RX_CTL:		STAT(rx_ctl);
	case S_TX_RSSI:
		snprintf(b, bs, "%d", wf->cur.ath.ast_tx_rssi);
		return 1;
	case S_RX_RSSI:
		snprintf(b, bs, "%d", wf->cur.ath.ast_rx_rssi);
		return 1;
	case S_BE_XMIT:		STAT(be_xmit);
	case S_BE_NOMBUF:	STAT(be_nombuf);
	case S_PER_CAL:		STAT(per_cal);
	case S_PER_CALFAIL:	STAT(per_calfail);
	case S_PER_RFGAIN:	STAT(per_rfgain);
#ifdef S_TDMA_UPDATE
	case S_TDMA_UPDATE:	STAT(tdma_update);
	case S_TDMA_TIMERS:	STAT(tdma_timers);
	case S_TDMA_TSF:	STAT(tdma_tsf);
#endif
	case S_RATE_CALLS:	STAT(rate_calls);
	case S_RATE_RAISE:	STAT(rate_raise);
	case S_RATE_DROP:	STAT(rate_drop);
	case S_ANT_DEFSWITCH:	STAT(ant_defswitch);
	case S_ANT_TXSWITCH:	STAT(ant_txswitch);
	case S_ANT_TX0:		TXANT(0);
	case S_ANT_TX1:		TXANT(1);
	case S_ANT_TX2:		TXANT(2);
	case S_ANT_TX3:		TXANT(3);
	case S_ANT_TX4:		TXANT(4);
	case S_ANT_TX5:		TXANT(5);
	case S_ANT_TX6:		TXANT(6);
	case S_ANT_TX7:		TXANT(7);
	case S_ANT_RX0:		RXANT(0);
	case S_ANT_RX1:		RXANT(1);
	case S_ANT_RX2:		RXANT(2);
	case S_ANT_RX3:		RXANT(3);
	case S_ANT_RX4:		RXANT(4);
	case S_ANT_RX5:		RXANT(5);
	case S_ANT_RX6:		RXANT(6);
	case S_ANT_RX7:		RXANT(7);
#ifdef S_CABQ_XMIT
	case S_CABQ_XMIT:	STAT(cabq_xmit);
	case S_CABQ_BUSY:	STAT(cabq_busy);
#endif
	case S_FF_TXOK:		STAT(ff_txok);
	case S_FF_TXERR:	STAT(ff_txerr);
	case S_FF_RX:		STAT(ff_rx);
	case S_FF_FLUSH:	STAT(ff_flush);
	case S_TX_QFULL:	STAT(tx_qfull);
	case S_RX_NOISE:
		snprintf(b, bs, "%d", wf->cur.ath.ast_rx_noise);
		return 1;
	case S_TX_SIGNAL:
		snprintf(b, bs, "%d",
			wf->cur.ath.ast_tx_rssi + wf->cur.ath.ast_rx_noise);
		return 1;
	case S_RX_SIGNAL:
		snprintf(b, bs, "%d",
			wf->cur.ath.ast_rx_rssi + wf->cur.ath.ast_rx_noise);
		return 1;
	}
	b[0] = '\0';
	return 0;
#undef RXANT
#undef TXANT
#undef PHY
#undef STAT
}

static int
ath_get_totstat(struct statfoo *sf, int s, char b[], size_t bs)
{
	struct athstatfoo_p *wf = (struct athstatfoo_p *) sf;
#define	STAT(x) \
	snprintf(b, bs, "%u", wf->total.ath.ast_##x); return 1
#define	PHY(x) \
	snprintf(b, bs, "%u", wf->total.ath.ast_rx_phy[x]); return 1
#define	TXANT(x) \
	snprintf(b, bs, "%u", wf->total.ath.ast_ant_tx[x]); return 1
#define	RXANT(x) \
	snprintf(b, bs, "%u", wf->total.ath.ast_ant_rx[x]); return 1

	switch (s) {
	case S_INPUT:
		snprintf(b, bs, "%lu",
		    wf->total.ath.ast_rx_packets - wf->total.ath.ast_rx_mgt);
		return 1;
	case S_OUTPUT:
		snprintf(b, bs, "%lu", wf->total.ath.ast_tx_packets);
		return 1;
	case S_RATE:
		snprintf(b, bs, "%uM", wf->total.ath.ast_tx_rate / 2);
		return 1;
	case S_WATCHDOG:	STAT(watchdog);
	case S_FATAL:		STAT(hardware);
	case S_BMISS:		STAT(bmiss);
	case S_BMISS_PHANTOM:	STAT(bmiss_phantom);
#ifdef S_BSTUCK
	case S_BSTUCK:		STAT(bstuck);
#endif
	case S_RXORN:		STAT(rxorn);
	case S_RXEOL:		STAT(rxeol);
	case S_TXURN:		STAT(txurn);
	case S_MIB:		STAT(mib);
#ifdef S_INTRCOAL
	case S_INTRCOAL:	STAT(intrcoal);
#endif
	case S_TX_MGMT:		STAT(tx_mgmt);
	case S_TX_DISCARD:	STAT(tx_discard);
	case S_TX_QSTOP:	STAT(tx_qstop);
	case S_TX_ENCAP:	STAT(tx_encap);
	case S_TX_NONODE:	STAT(tx_nonode);
	case S_TX_NOMBUF:	STAT(tx_nombuf);
#ifdef S_TX_NOMCL
	case S_TX_NOMCL:	STAT(tx_nomcl);
	case S_TX_LINEAR:	STAT(tx_linear);
	case S_TX_NODATA:	STAT(tx_nodata);
	case S_TX_BUSDMA:	STAT(tx_busdma);
#endif
	case S_TX_XRETRIES:	STAT(tx_xretries);
	case S_TX_FIFOERR:	STAT(tx_fifoerr);
	case S_TX_FILTERED:	STAT(tx_filtered);
	case S_TX_SHORTRETRY:	STAT(tx_shortretry);
	case S_TX_LONGRETRY:	STAT(tx_longretry);
	case S_TX_BADRATE:	STAT(tx_badrate);
	case S_TX_NOACK:	STAT(tx_noack);
	case S_TX_RTS:		STAT(tx_rts);
	case S_TX_CTS:		STAT(tx_cts);
	case S_TX_SHORTPRE:	STAT(tx_shortpre);
	case S_TX_ALTRATE:	STAT(tx_altrate);
	case S_TX_PROTECT:	STAT(tx_protect);
	case S_RX_NOMBUF:	STAT(rx_nombuf);
#ifdef S_RX_BUSDMA
	case S_RX_BUSDMA:	STAT(rx_busdma);
#endif
	case S_RX_ORN:		STAT(rx_orn);
	case S_RX_CRC_ERR:	STAT(rx_crcerr);
	case S_RX_FIFO_ERR: 	STAT(rx_fifoerr);
	case S_RX_CRYPTO_ERR: 	STAT(rx_badcrypt);
	case S_RX_MIC_ERR:	STAT(rx_badmic);
	case S_RX_PHY_ERR:	STAT(rx_phyerr);
	case S_RX_PHY_UNDERRUN:	PHY(HAL_PHYERR_UNDERRUN);
	case S_RX_PHY_TIMING:	PHY(HAL_PHYERR_TIMING);
	case S_RX_PHY_PARITY:	PHY(HAL_PHYERR_PARITY);
	case S_RX_PHY_RATE:	PHY(HAL_PHYERR_RATE);
	case S_RX_PHY_LENGTH:	PHY(HAL_PHYERR_LENGTH);
	case S_RX_PHY_RADAR:	PHY(HAL_PHYERR_RADAR);
	case S_RX_PHY_SERVICE:	PHY(HAL_PHYERR_SERVICE);
	case S_RX_PHY_TOR:	PHY(HAL_PHYERR_TOR);
	case S_RX_PHY_OFDM_TIMING:	  PHY(HAL_PHYERR_OFDM_TIMING);
	case S_RX_PHY_OFDM_SIGNAL_PARITY: PHY(HAL_PHYERR_OFDM_SIGNAL_PARITY);
	case S_RX_PHY_OFDM_RATE_ILLEGAL:  PHY(HAL_PHYERR_OFDM_RATE_ILLEGAL);
	case S_RX_PHY_OFDM_POWER_DROP:	  PHY(HAL_PHYERR_OFDM_POWER_DROP);
	case S_RX_PHY_OFDM_SERVICE:	  PHY(HAL_PHYERR_OFDM_SERVICE);
	case S_RX_PHY_OFDM_RESTART:	  PHY(HAL_PHYERR_OFDM_RESTART);
	case S_RX_PHY_CCK_TIMING:	  PHY(HAL_PHYERR_CCK_TIMING);
	case S_RX_PHY_CCK_HEADER_CRC:	  PHY(HAL_PHYERR_CCK_HEADER_CRC);
	case S_RX_PHY_CCK_RATE_ILLEGAL:	  PHY(HAL_PHYERR_CCK_RATE_ILLEGAL);
	case S_RX_PHY_CCK_SERVICE:	  PHY(HAL_PHYERR_CCK_SERVICE);
	case S_RX_PHY_CCK_RESTART:	  PHY(HAL_PHYERR_CCK_RESTART);
	case S_RX_TOOSHORT:	STAT(rx_tooshort);
	case S_RX_TOOBIG:	STAT(rx_toobig);
	case S_RX_MGT:		STAT(rx_mgt);
	case S_RX_CTL:		STAT(rx_ctl);
	case S_TX_RSSI:
		snprintf(b, bs, "%d", wf->total.ath.ast_tx_rssi);
		return 1;
	case S_RX_RSSI:
		snprintf(b, bs, "%d", wf->total.ath.ast_rx_rssi);
		return 1;
	case S_BE_XMIT:		STAT(be_xmit);
	case S_BE_NOMBUF:	STAT(be_nombuf);
	case S_PER_CAL:		STAT(per_cal);
	case S_PER_CALFAIL:	STAT(per_calfail);
	case S_PER_RFGAIN:	STAT(per_rfgain);
#ifdef S_TDMA_UPDATE
	case S_TDMA_UPDATE:	STAT(tdma_update);
	case S_TDMA_TIMERS:	STAT(tdma_timers);
	case S_TDMA_TSF:	STAT(tdma_tsf);
#endif
	case S_RATE_CALLS:	STAT(rate_calls);
	case S_RATE_RAISE:	STAT(rate_raise);
	case S_RATE_DROP:	STAT(rate_drop);
	case S_ANT_DEFSWITCH:	STAT(ant_defswitch);
	case S_ANT_TXSWITCH:	STAT(ant_txswitch);
	case S_ANT_TX0:		TXANT(0);
	case S_ANT_TX1:		TXANT(1);
	case S_ANT_TX2:		TXANT(2);
	case S_ANT_TX3:		TXANT(3);
	case S_ANT_TX4:		TXANT(4);
	case S_ANT_TX5:		TXANT(5);
	case S_ANT_TX6:		TXANT(6);
	case S_ANT_TX7:		TXANT(7);
	case S_ANT_RX0:		RXANT(0);
	case S_ANT_RX1:		RXANT(1);
	case S_ANT_RX2:		RXANT(2);
	case S_ANT_RX3:		RXANT(3);
	case S_ANT_RX4:		RXANT(4);
	case S_ANT_RX5:		RXANT(5);
	case S_ANT_RX6:		RXANT(6);
	case S_ANT_RX7:		RXANT(7);
#ifdef S_CABQ_XMIT
	case S_CABQ_XMIT:	STAT(cabq_xmit);
	case S_CABQ_BUSY:	STAT(cabq_busy);
#endif
	case S_FF_TXOK:		STAT(ff_txok);
	case S_FF_TXERR:	STAT(ff_txerr);
	case S_FF_RX:		STAT(ff_rx);
	case S_FF_FLUSH:	STAT(ff_flush);
	case S_TX_QFULL:	STAT(tx_qfull);
	case S_RX_NOISE:
		snprintf(b, bs, "%d", wf->total.ath.ast_rx_noise);
		return 1;
	case S_TX_SIGNAL:
		snprintf(b, bs, "%d",
			wf->total.ath.ast_tx_rssi + wf->total.ath.ast_rx_noise);
		return 1;
	case S_RX_SIGNAL:
		snprintf(b, bs, "%d",
			wf->total.ath.ast_rx_rssi + wf->total.ath.ast_rx_noise);
		return 1;
	}
	b[0] = '\0';
	return 0;
#undef RXANT
#undef TXANT
#undef PHY
#undef STAT
}

static void
ath_print_verbose(struct statfoo *sf, FILE *fd)
{
	struct athstatfoo_p *wf = (struct athstatfoo_p *) sf;
#define	isphyerr(i)	(S_PHY_MIN <= i && i <= S_PHY_MAX)
	const struct fmt *f;
	char s[32];
	const char *indent;
	int i, width;

	width = 0;
	for (i = 0; i < S_LAST; i++) {
		f = &sf->stats[i];
		if (!isphyerr(i) && f->width > width)
			width = f->width;
	}
	for (i = 0; i < S_LAST; i++) {
		if (ath_get_totstat(sf, i, s, sizeof(s)) && strcmp(s, "0")) {
			if (isphyerr(i))
				indent = "    ";
			else
				indent = "";
			fprintf(fd, "%s%-*s %s\n", indent, width, s, athstats[i].desc);
		}
	}
	fprintf(fd, "Antenna profile:\n");
	for (i = 0; i < 8; i++)
		if (wf->total.ath.ast_ant_rx[i] || wf->total.ath.ast_ant_tx[i])
			fprintf(fd, "[%u] tx %8u rx %8u\n", i,
				wf->total.ath.ast_ant_tx[i],
				wf->total.ath.ast_ant_rx[i]);
#undef isphyerr
}

STATFOO_DEFINE_BOUNCE(athstatfoo)

struct athstatfoo *
athstats_new(const char *ifname, const char *fmtstring)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	struct athstatfoo_p *wf;

	wf = calloc(1, sizeof(struct athstatfoo_p));
	if (wf != NULL) {
		statfoo_init(&wf->base.base, "athstats", athstats, N(athstats));
		/* override base methods */
		wf->base.base.collect_cur = ath_collect_cur;
		wf->base.base.collect_tot = ath_collect_tot;
		wf->base.base.get_curstat = ath_get_curstat;
		wf->base.base.get_totstat = ath_get_totstat;
		wf->base.base.update_tot = ath_update_tot;
		wf->base.base.print_verbose = ath_print_verbose;

		/* setup bounce functions for public methods */
		STATFOO_BOUNCE(wf, athstatfoo);

		/* setup our public methods */
		wf->base.setifname = ath_setifname;
#if 0
		wf->base.setstamac = wlan_setstamac;
#endif
		wf->s = socket(AF_INET, SOCK_DGRAM, 0);
		if (wf->s < 0)
			err(1, "socket");

		ath_setifname(&wf->base, ifname);
		wf->base.setfmt(&wf->base, fmtstring);
	}
	return &wf->base;
#undef N
}
