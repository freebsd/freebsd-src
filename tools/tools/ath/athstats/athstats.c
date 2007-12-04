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
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "../../../../sys/contrib/dev/ath/ah_desc.h"
#include "../../../../sys/net80211/ieee80211_ioctl.h"
#include "../../../../sys/net80211/ieee80211_radiotap.h"
#include "../../../../sys/dev/ath/if_athioctl.h"

#include "athstats.h"

#define	NOTPRESENT	{ 0, "", "" }

static const struct fmt athstats[] = {
#define	S_INPUT		0
	{ 8,	"input",	"input",	"data frames received" },
#define	S_OUTPUT	1
	{ 8,	"output",	"output",	"data frames transmit" },
#define	S_TX_ALTRATE	2
	{ 7,	"altrate",	"altrate",	"tx frames with an alternate rate" },
#define	S_TX_SHORTRETRY	3
	{ 7,	"short",	"short",	"short on-chip tx retries" },
#define	S_TX_LONGRETRY	4
	{ 7,	"long",		"long",		"long on-chip tx retries" },
#define	S_TX_XRETRIES	5
	{ 6,	"xretry",	"xretry",	"tx failed 'cuz too many retries" },
#define	S_MIB		6
	{ 5,	"mib",		"mib",		"mib overflow interrupts" },
#ifndef __linux__
#define	S_TX_LINEAR	7
	{ 5,	"txlinear",	"txlinear",	"tx linearized to cluster" },
#define	S_BSTUCK	8
	{ 5,	"bstuck",	"bstuck",	"stuck beacon conditions" },
#define	S_INTRCOAL	9
	{ 5,	"intrcoal",	"intrcoal",	"interrupts coalesced" },
#else
	NOTPRESENT, NOTPRESENT, NOTPRESENT,
#endif
#define	S_RATE		10
	{ 4,	"rate",		"rate",		"current transmit rate" },
#define	S_WATCHDOG	11
	{ 5,	"wdog",		"wdog",		"watchdog timeouts" },
#define	S_FATAL		12
	{ 5,	"fatal",	"fatal",	"hardware error interrupts" },
#define	S_BMISS		13
	{ 5,	"bmiss",	"bmiss",	"beacon miss interrupts" },
#define	S_RXORN		14
	{ 5,	"rxorn",	"rxorn",	"recv overrun interrupts" },
#define	S_RXEOL		15
	{ 5,	"rxeol",	"rxeol",	"recv eol interrupts" },
#define	S_TXURN		16
	{ 5,	"txurn",	"txurn",	"txmit underrun interrupts" },
#define	S_TX_MGMT	17
	{ 5,	"txmgt",	"txmgt",	"tx management frames" },
#define	S_TX_DISCARD	18
	{ 5,	"txdisc",	"txdisc",	"tx frames discarded prior to association" },
#define	S_TX_INVALID	19
	{ 5,	"txinv",	"txinv",	"tx invalid (19)" },
#define	S_TX_QSTOP	20
	{ 5,	"qstop",	"qstop",	"tx stopped 'cuz no xmit buffer" },
#define	S_TX_ENCAP	21
	{ 5,	"txencode",	"txencode",	"tx encapsulation failed" },
#define	S_TX_NONODE	22
	{ 5,	"txnonode",	"txnonode",	"tx failed 'cuz no node" },
#define	S_TX_NOMBUF	23
	{ 5,	"txnombuf",	"txnombuf",	"tx failed 'cuz mbuf allocation failed" },
#ifndef __linux__
#define	S_TX_NOMCL	24
	{ 5,	"txnomcl",	"txnomcl",	"tx failed 'cuz cluster allocation failed" },
#else
	NOTPRESENT,
#endif
#define	S_TX_FIFOERR	25
	{ 5,	"efifo",	"efifo",	"tx failed 'cuz FIFO underrun" },
#define	S_TX_FILTERED	26
	{ 5,	"efilt",	"efilt",	"tx failed 'cuz destination filtered" },
#define	S_TX_BADRATE	27
	{ 5,	"txbadrate",	"txbadrate",	"tx failed 'cuz bogus xmit rate" },
#define	S_TX_NOACK	28
	{ 5,	"noack",	"noack",	"tx frames with no ack marked" },
#define	S_TX_RTS	29
	{ 5,	"rts",		"rts",		"tx frames with rts enabled" },
#define	S_TX_CTS	30
	{ 5,	"cts",		"cts",		"tx frames with cts enabled" },
#define	S_TX_SHORTPRE	31
	{ 5,	"shpre",	"shpre",	"tx frames with short preamble" },
#define	S_TX_PROTECT	32
	{ 5,	"protect",	"protect",	"tx frames with 11g protection" },
#define	S_RX_ORN	33
	{ 5,	"rxorn",	"rxorn",	"rx failed 'cuz of desc overrun" },
#define	S_RX_CRC_ERR	34
	{ 6,	"crcerr",	"crcerr",	"rx failed 'cuz of bad CRC" },
#define	S_RX_FIFO_ERR	35
	{ 5,	"rxfifo",	"rxfifo",	"rx failed 'cuz of FIFO overrun" },
#define	S_RX_CRYPTO_ERR	36
	{ 5,	"crypt",	"crypt",	"rx failed 'cuz decryption" },
#define	S_RX_MIC_ERR	37
	{ 4,	"mic",		"mic",		"rx failed 'cuz MIC failure" },
#define	S_RX_TOOSHORT	38
	{ 5,	"rxshort",	"rxshort",	"rx failed 'cuz frame too short" },
#define	S_RX_NOMBUF	39
	{ 5,	"rxnombuf",	"rxnombuf",	"rx setup failed 'cuz no mbuf" },
#define	S_RX_MGT	40
	{ 5,	"rxmgt",	"rxmgt",	"rx management frames" },
#define	S_RX_CTL	41
	{ 5,	"rxctl",	"rxctl",	"rx control frames" },
#define	S_RX_PHY_ERR	42
	{ 7,	"phyerr",	"phyerr",	"rx failed 'cuz of PHY err" },
#define	S_RX_PHY_UNDERRUN		43
	{ 6,	"phyund",	"phyund",	"transmit underrun" },
#define	S_RX_PHY_TIMING			44
	{ 6,	"phytim",	"phytim",	"timing error" },
#define	S_RX_PHY_PARITY			45
	{ 6,	"phypar",	"phypar",	"illegal parity" },
#define	S_RX_PHY_RATE			46
	{ 6,	"phyrate",	"phyrate",	"illegal rate" },
#define	S_RX_PHY_LENGTH			47
	{ 6,	"phylen",	"phylen",	"illegal length" },
#define	S_RX_PHY_RADAR			48
	{ 6,	"phyradar",	"phyradar",	"radar detect" },
#define	S_RX_PHY_SERVICE		49
	{ 6,	"physervice",	"physervice",	"illegal service" },
#define	S_RX_PHY_TOR			50
	{ 6,	"phytor",	"phytor",	"transmit override receive" },
#define	S_RX_PHY_OFDM_TIMING		51
	{ 6,	"ofdmtim",	"ofdmtim",	"OFDM timing" },
#define	S_RX_PHY_OFDM_SIGNAL_PARITY	52
	{ 6,	"ofdmsig",	"ofdmsig",	"OFDM illegal parity" },
#define	S_RX_PHY_OFDM_RATE_ILLEGAL	53
	{ 6,	"ofdmrate",	"ofdmrate",	"OFDM illegal rate" },
#define	S_RX_PHY_OFDM_POWER_DROP	54
	{ 6,	"ofdmpow",	"ofdmpow",	"OFDM power drop" },
#define	S_RX_PHY_OFDM_SERVICE		55
	{ 6,	"ofdmservice",	"ofdmservice",	"OFDM illegal service" },
#define	S_RX_PHY_OFDM_RESTART		56
	{ 6,	"ofdmrestart",	"ofdmrestart",	"OFDM restart" },
#define	S_RX_PHY_CCK_TIMING		57
	{ 6,	"ccktim",	"ccktim",	"CCK timing" },
#define	S_RX_PHY_CCK_HEADER_CRC		58
	{ 6,	"cckhead",	"cckhead",	"CCK header crc" },
#define	S_RX_PHY_CCK_RATE_ILLEGAL	59
	{ 6,	"cckrate",	"cckrate",	"CCK illegal rate" },
#define	S_RX_PHY_CCK_SERVICE		60
	{ 6,	"cckservice",	"cckservice",	"CCK illegal service" },
#define	S_RX_PHY_CCK_RESTART		61
	{ 6,	"cckrestar",	"cckrestar",	"CCK restart" },
#define	S_BE_NOMBUF	62
	{ 4,	"benombuf",	"benombuf",	"beacon setup failed 'cuz no mbuf" },
#define	S_BE_XMIT	63
	{ 7,	"bexmit",	"bexmit",	"beacons transmitted" },
#define	S_PER_CAL	64
	{ 4,	"pcal",		"pcal",		"periodic calibrations" },
#define	S_PER_CALFAIL	65
	{ 4,	"pcalf",	"pcalf",	"periodic calibration failures" },
#define	S_PER_RFGAIN	66
	{ 4,	"prfga",	"prfga",	"rfgain value change" },
#if 0
#define	S_TDMA_UPDATE	67
	{ 5,	"tdmau",	"tdmau",	"TDMA slot timing updates" },
#define	S_TDMA_TIMERS	68
	{ 5,	"tdmab",	"tdmab",	"TDMA slot update set beacon timers" },
#define	S_TDMA_TSF	69
	{ 5,	"tdmat",	"tdmat",	"TDMA slot update set TSF" },
#else
	NOTPRESENT, NOTPRESENT, NOTPRESENT,
#endif
#define	S_RATE_CALLS	70
	{ 5,	"ratec",	"ratec",	"rate control checks" },
#define	S_RATE_RAISE	71
	{ 5,	"rate+",	"rate+",	"rate control raised xmit rate" },
#define	S_RATE_DROP	72
	{ 5,	"rate-",	"rate-",	"rate control dropped xmit rate" },
#define	S_TX_RSSI	73
	{ 4,	"arssi",	"arssi",	"rssi of last ack" },
#define	S_RX_RSSI	74
	{ 4,	"rssi",		"rssi",		"avg recv rssi" },
#define	S_RX_NOISE	75
	{ 5,	"noise",	"noise",	"rx noise floor" },
#define	S_BMISS_PHANTOM	76
	{ 5,	"bmissphantom",	"bmissphantom",	"phantom beacon misses" },
#define	S_TX_RAW	77
	{ 5,	"txraw",	"txraw",	"tx frames through raw api" },
#define	S_RX_TOOBIG	78
	{ 5,	"rx2big",	"rx2big",	"rx failed 'cuz frame too large"  },
#ifndef __linux__
#define	S_CABQ_XMIT	79
	{ 5,	"cabxmit",	"cabxmit",	"cabq frames transmitted" },
#define	S_CABQ_BUSY	80
	{ 5,	"cabqbusy",	"cabqbusy",	"cabq xmit overflowed beacon interval" },
#define	S_TX_NODATA	81
	{ 5,	"txnodata",	"txnodata",	"tx discarded empty frame" },
#define	S_TX_BUSDMA	82
	{ 5,	"txbusdma",	"txbusdma",	"tx failed for dma resrcs" },
#define	S_RX_BUSDMA	83
	{ 5,	"rxbusdma",	"rxbusdma",	"rx setup failed for dma resrcs" },
#else
	NOTPRESENT, NOTPRESENT, NOTPRESENT, NOTPRESENT, NOTPRESENT,
#endif
#if 0
#define	S_FF_TXOK	84
	{ 5,	"fftxok",	"fftxok",	"fast frames xmit successfully" },
#define	S_FF_TXERR	85
	{ 5,	"fftxerr",	"fftxerr",	"fast frames not xmit due to error" },
#define	S_FF_RX		86
	{ 5,	"ffrx",		"ffrx",		"fast frames received" },
#define	S_FF_FLUSH	87
	{ 5,	"ffflush",	"ffflush",	"fast frames flushed from staging q" },
#else
	NOTPRESENT, NOTPRESENT, NOTPRESENT, NOTPRESENT,
#endif
#define	S_ANT_DEFSWITCH	88
	{ 5,	"defsw",	"defsw",	"switched default/rx antenna" },
#define	S_ANT_TXSWITCH	89
	{ 5,	"txsw",		"txsw",		"tx used alternate antenna" },
#define	S_ANT_TX0	90
	{ 8,	"tx0",	"ant0(tx)",	"frames tx on antenna 0" },
#define	S_ANT_TX1	91
	{ 8,	"tx1",	"ant1(tx)",	"frames tx on antenna 1"  },
#define	S_ANT_TX2	92
	{ 8,	"tx2",	"ant2(tx)",	"frames tx on antenna 2"  },
#define	S_ANT_TX3	93
	{ 8,	"tx3",	"ant3(tx)",	"frames tx on antenna 3"  },
#define	S_ANT_TX4	94
	{ 8,	"tx4",	"ant4(tx)",	"frames tx on antenna 4"  },
#define	S_ANT_TX5	95
	{ 8,	"tx5",	"ant5(tx)",	"frames tx on antenna 5"  },
#define	S_ANT_TX6	96
	{ 8,	"tx6",	"ant6(tx)",	"frames tx on antenna 6"  },
#define	S_ANT_TX7	97
	{ 8,	"tx7",	"ant7(tx)",	"frames tx on antenna 7"  },
#define	S_ANT_RX0	98
	{ 8,	"rx0",	"ant0(rx)",	"frames rx on antenna 0"  },
#define	S_ANT_RX1	99
	{ 8,	"rx1",	"ant1(rx)",	"frames rx on antenna 1"   },
#define	S_ANT_RX2	100
	{ 8,	"rx2",	"ant2(rx)",	"frames rx on antenna 2"   },
#define	S_ANT_RX3	101
	{ 8,	"rx3",	"ant3(rx)",	"frames rx on antenna 3"   },
#define	S_ANT_RX4	102
	{ 8,	"rx4",	"ant4(rx)",	"frames rx on antenna 4"   },
#define	S_ANT_RX5	103
	{ 8,	"rx5",	"ant5(rx)",	"frames rx on antenna 5"   },
#define	S_ANT_RX6	104
	{ 8,	"rx6",	"ant6(rx)",	"frames rx on antenna 6"   },
#define	S_ANT_RX7	105
	{ 8,	"rx7",	"ant7(rx)",	"frames rx on antenna 7"   },
#define	S_TX_SIGNAL	106
	{ 4,	"asignal",	"asig",	"signal of last ack (dBm)" },
#define	S_RX_SIGNAL	107
	{ 4,	"signal",	"sig",	"avg recv signal (dBm)" },
};
#define	S_PHY_MIN	S_RX_PHY_UNDERRUN
#define	S_PHY_MAX	S_RX_PHY_CCK_RESTART
#define	S_LAST		S_ANT_TX0
#define	S_MAX	S_ANT_RX7+1

struct athstatfoo_p {
	struct athstatfoo base;
	int s;
	struct ifreq ifr;
	struct ath_stats cur;
	struct ath_stats total;
};

static void
ath_setifname(struct athstatfoo *wf0, const char *ifname)
{
	struct athstatfoo_p *wf = (struct athstatfoo_p *) wf0;

	strncpy(wf->ifr.ifr_name, ifname, sizeof (wf->ifr.ifr_name));
}

static void
ath_collect(struct athstatfoo_p *wf, struct ath_stats *stats)
{
	wf->ifr.ifr_data = (caddr_t) stats;
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
	snprintf(b, bs, "%u", wf->cur.ast_##x - wf->total.ast_##x); return 1
#define	PHY(x) \
	snprintf(b, bs, "%u", wf->cur.ast_rx_phy[x] - wf->total.ast_rx_phy[x]); return 1
#define	TXANT(x) \
	snprintf(b, bs, "%u", wf->cur.ast_ant_tx[x] - wf->total.ast_ant_tx[x]); return 1
#define	RXANT(x) \
	snprintf(b, bs, "%u", wf->cur.ast_ant_rx[x] - wf->total.ast_ant_rx[x]); return 1

	switch (s) {
	case S_INPUT:
		snprintf(b, bs, "%lu",
		    (wf->cur.ast_rx_packets - wf->total.ast_rx_packets) -
		    (wf->cur.ast_rx_mgt - wf->total.ast_rx_mgt));
		return 1;
	case S_OUTPUT:
		snprintf(b, bs, "%lu",
		    wf->cur.ast_tx_packets - wf->total.ast_tx_packets);
		return 1;
	case S_RATE:
		snprintf(b, bs, "%uM", wf->cur.ast_tx_rate / 2);
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
		snprintf(b, bs, "%d", wf->cur.ast_tx_rssi);
		return 1;
	case S_RX_RSSI:
		snprintf(b, bs, "%d", wf->cur.ast_rx_rssi);
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
#ifdef S_FF_TXOK
	case S_FF_TXOK:		STAT(ff_txok);
	case S_FF_TXERR:	STAT(ff_txerr);
	case S_FF_FLUSH:	STAT(ff_flush);
	case S_FF_QFULL:	STAT(ff_qfull);
#endif
	case S_RX_NOISE:
		snprintf(b, bs, "%d", wf->cur.ast_rx_noise);
		return 1;
	case S_TX_SIGNAL:
		snprintf(b, bs, "%d",
			wf->cur.ast_tx_rssi + wf->cur.ast_rx_noise);
		return 1;
	case S_RX_SIGNAL:
		snprintf(b, bs, "%d",
			wf->cur.ast_rx_rssi + wf->cur.ast_rx_noise);
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
	snprintf(b, bs, "%u", wf->total.ast_##x); return 1
#define	PHY(x) \
	snprintf(b, bs, "%u", wf->total.ast_rx_phy[x]); return 1
#define	TXANT(x) \
	snprintf(b, bs, "%u", wf->total.ast_ant_tx[x]); return 1
#define	RXANT(x) \
	snprintf(b, bs, "%u", wf->total.ast_ant_rx[x]); return 1

	switch (s) {
	case S_INPUT:
		snprintf(b, bs, "%lu",
		    wf->total.ast_rx_packets - wf->total.ast_rx_mgt);
		return 1;
	case S_OUTPUT:
		snprintf(b, bs, "%lu", wf->total.ast_tx_packets);
		return 1;
	case S_RATE:
		snprintf(b, bs, "%uM", wf->total.ast_tx_rate / 2);
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
		snprintf(b, bs, "%d", wf->total.ast_tx_rssi);
		return 1;
	case S_RX_RSSI:
		snprintf(b, bs, "%d", wf->total.ast_rx_rssi);
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
#ifdef S_FF_TXOK
	case S_FF_TXOK:		STAT(ff_txok);
	case S_FF_TXERR:	STAT(ff_txerr);
	case S_FF_FLUSH:	STAT(ff_flush);
	case S_FF_QFULL:	STAT(ff_qfull);
#endif
	case S_RX_NOISE:
		snprintf(b, bs, "%d", wf->total.ast_rx_noise);
		return 1;
	case S_TX_SIGNAL:
		snprintf(b, bs, "%d",
			wf->total.ast_tx_rssi + wf->total.ast_rx_noise);
		return 1;
	case S_RX_SIGNAL:
		snprintf(b, bs, "%d",
			wf->total.ast_rx_rssi + wf->total.ast_rx_noise);
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
	char s[32];
	const char *indent;
	int i;

	for (i = 0; i < S_LAST; i++) {
		if (ath_get_totstat(sf, i, s, sizeof(s)) && strcmp(s, "0")) {
			if (isphyerr(i))
				indent = "    ";
			else
				indent = "";
			fprintf(fd, "%s%s %s\n", indent, s, athstats[i].desc);
		}
	}
	fprintf(fd, "Antenna profile:\n");
	for (i = 0; i < 8; i++)
		if (wf->total.ast_ant_rx[i] || wf->total.ast_ant_tx[i])
			fprintf(fd, "[%u] tx %8u rx %8u\n", i,
				wf->total.ast_ant_tx[i],
				wf->total.ast_ant_rx[i]);
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
