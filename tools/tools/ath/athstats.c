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
 * Simple Atheros-specific tool to inspect and monitor network traffic
 * statistics.
 *	athstats [-i interface] [interval]
 * (default interface is ath0).  If interval is specified a rolling output
 * a la netstat -i is displayed every interval seconds.
 *
 * To build: cc -o athstats athstats.c -lkvm
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

#include "../../../sys/contrib/dev/ath/ah_desc.h"
#include "../../../sys/net80211/ieee80211_ioctl.h"
#include "../../../sys/net80211/ieee80211_radiotap.h"
#include "../../../sys/dev/ath/if_athioctl.h"

static const struct {
	u_int		phyerr;
	const char*	desc;
} phyerrdescriptions[] = {
	{ HAL_PHYERR_UNDERRUN,		"transmit underrun" },
	{ HAL_PHYERR_TIMING,		"timing error" },
	{ HAL_PHYERR_PARITY,		"illegal parity" },
	{ HAL_PHYERR_RATE,		"illegal rate" },
	{ HAL_PHYERR_LENGTH,		"illegal length" },
	{ HAL_PHYERR_RADAR,		"radar detect" },
	{ HAL_PHYERR_SERVICE,		"illegal service" },
	{ HAL_PHYERR_TOR,		"transmit override receive" },
	{ HAL_PHYERR_OFDM_TIMING,	"OFDM timing" },
	{ HAL_PHYERR_OFDM_SIGNAL_PARITY,"OFDM illegal parity" },
	{ HAL_PHYERR_OFDM_RATE_ILLEGAL,	"OFDM illegal rate" },
	{ HAL_PHYERR_OFDM_POWER_DROP,	"OFDM power drop" },
	{ HAL_PHYERR_OFDM_SERVICE,	"OFDM illegal service" },
	{ HAL_PHYERR_OFDM_RESTART,	"OFDM restart" },
	{ HAL_PHYERR_CCK_TIMING,	"CCK timing" },
	{ HAL_PHYERR_CCK_HEADER_CRC,	"CCK header crc" },
	{ HAL_PHYERR_CCK_RATE_ILLEGAL,	"CCK illegal rate" },
	{ HAL_PHYERR_CCK_SERVICE,	"CCK illegal service" },
	{ HAL_PHYERR_CCK_RESTART,	"CCK restart" },
};

static void
printstats(FILE *fd, const struct ath_stats *stats)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
#define	STAT(x,fmt) \
	if (stats->ast_##x) fprintf(fd, "%u " fmt "\n", stats->ast_##x)
	int i, j;

	STAT(watchdog, "watchdog timeouts");
	STAT(hardware, "hardware error interrupts");
	STAT(bmiss, "beacon miss interrupts");
	STAT(bstuck, "stuck beacon conditions");
	STAT(rxorn, "recv overrun interrupts");
	STAT(rxeol, "recv eol interrupts");
	STAT(txurn, "txmit underrun interrupts");
	STAT(mib, "mib overflow interrupts");
	STAT(intrcoal, "interrupts coalesced");
	STAT(tx_mgmt, "tx management frames");
	STAT(tx_discard, "tx frames discarded prior to association");
	STAT(tx_qstop, "tx stopped 'cuz no xmit buffer");
	STAT(tx_encap, "tx encapsulation failed");
	STAT(tx_nonode, "tx failed 'cuz no node");
	STAT(tx_nombuf, "tx failed 'cuz no mbuf");
	STAT(tx_nomcl, "tx failed 'cuz no cluster");
	STAT(tx_linear, "tx linearized to cluster");
	STAT(tx_nodata, "tx discarded empty frame");
	STAT(tx_busdma, "tx failed for dma resrcs");
	STAT(tx_xretries, "tx failed 'cuz too many retries");
	STAT(tx_fifoerr, "tx failed 'cuz FIFO underrun");
	STAT(tx_filtered, "tx failed 'cuz xmit filtered");
	STAT(tx_shortretry, "short on-chip tx retries");
	STAT(tx_longretry, "long on-chip tx retries");
	STAT(tx_badrate, "tx failed 'cuz bogus xmit rate");
	STAT(tx_noack, "tx frames with no ack marked");
	STAT(tx_rts, "tx frames with rts enabled");
	STAT(tx_cts, "tx frames with cts enabled");
	STAT(tx_shortpre, "tx frames with short preamble");
	STAT(tx_altrate, "tx frames with an alternate rate");
	STAT(tx_protect, "tx frames with 11g protection");
	STAT(rx_nombuf,	"rx setup failed 'cuz no mbuf");
	STAT(rx_busdma,	"rx setup failed for dma resrcs");
	STAT(rx_orn, "rx failed 'cuz of desc overrun");
	STAT(rx_crcerr, "rx failed 'cuz of bad CRC");
	STAT(rx_fifoerr, "rx failed 'cuz of FIFO overrun");
	STAT(rx_badcrypt, "rx failed 'cuz decryption");
	STAT(rx_badmic, "rx failed 'cuz MIC failure");
	STAT(rx_tooshort, "rx failed 'cuz frame too short");
	STAT(rx_toobig, "rx failed 'cuz frame too large");
	STAT(rx_mgt, "rx management frames");
	STAT(rx_ctl, "rx control frames");
	STAT(rx_phyerr, "rx failed 'cuz of PHY err");
	if (stats->ast_rx_phyerr != 0) {
		for (i = 0; i < 32; i++) {
			if (stats->ast_rx_phy[i] == 0)
				continue;
			for (j = 0; j < N(phyerrdescriptions); j++)
				if (phyerrdescriptions[j].phyerr == i)
					break;
			if (j == N(phyerrdescriptions))
				fprintf(fd,
					"    %u (unknown phy error code %u)\n",
					stats->ast_rx_phy[i], i);
			else
				fprintf(fd, "    %u %s\n",
					stats->ast_rx_phy[i],
					phyerrdescriptions[j].desc);
		}
	}
	STAT(be_nombuf,	"beacon setup failed 'cuz no mbuf");
	STAT(be_xmit,	"beacons transmitted");
	STAT(per_cal, "periodic calibrations");
	STAT(per_calfail, "periodic calibration failures");
	STAT(per_rfgain, "rfgain value change");
	STAT(rate_calls, "rate control checks");
	STAT(rate_raise, "rate control raised xmit rate");
	STAT(rate_drop, "rate control dropped xmit rate");
	if (stats->ast_tx_rssi)
		fprintf(fd, "rssi of last ack: %u\n", stats->ast_tx_rssi);
	if (stats->ast_rx_rssi)
		fprintf(fd, "avg recv rssi: %u\n", stats->ast_rx_rssi);
	STAT(ant_defswitch, "switched default/rx antenna");
	STAT(ant_txswitch, "tx used alternate antenna");
	fprintf(fd, "Antenna profile:\n");
	for (i = 0; i < 8; i++)
		if (stats->ast_ant_rx[i] || stats->ast_ant_tx[i])
			fprintf(fd, "[%u] tx %8u rx %8u\n", i,
				stats->ast_ant_tx[i], stats->ast_ant_rx[i]);
#undef STAT
#undef N
}

static u_int
getifrate(int s, const char* ifname)
{
#define	N(a)	(sizeof(a) / sizeof(a[0]))
	static const int rates[] = {
		0,		/* IFM_AUTO */
		0,		/* IFM_MANUAL */
		0,		/* IFM_NONE */
		1,		/* IFM_IEEE80211_FH1 */
		2,		/* IFM_IEEE80211_FH2 */
		1,		/* IFM_IEEE80211_DS1 */
		2,		/* IFM_IEEE80211_DS2 */
		5,		/* IFM_IEEE80211_DS5 */
		11,		/* IFM_IEEE80211_DS11 */
		22,		/* IFM_IEEE80211_DS22 */
		6,		/* IFM_IEEE80211_OFDM6 */
		9,		/* IFM_IEEE80211_OFDM9 */
		12,		/* IFM_IEEE80211_OFDM12 */
		18,		/* IFM_IEEE80211_OFDM18 */
		24,		/* IFM_IEEE80211_OFDM24 */
		36,		/* IFM_IEEE80211_OFDM36 */
		48,		/* IFM_IEEE80211_OFDM48 */
		54,		/* IFM_IEEE80211_OFDM54 */
		72,		/* IFM_IEEE80211_OFDM72 */
	};
	struct ifmediareq ifmr;
	int *media_list, i;

	(void) memset(&ifmr, 0, sizeof(ifmr));
	(void) strncpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));

	if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0)
		return 0;
	return IFM_SUBTYPE(ifmr.ifm_active) < N(rates) ?
		rates[IFM_SUBTYPE(ifmr.ifm_active)] : 0;
#undef N
}

static int signalled;

static void
catchalarm(int signo __unused)
{
	signalled = 1;
}

int
main(int argc, char *argv[])
{
	int s;
	struct ifreq ifr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "socket");
	if (argc > 1 && strcmp(argv[1], "-i") == 0) {
		if (argc < 2) {
			fprintf(stderr, "%s: missing interface name for -i\n",
				argv[0]);
			exit(-1);
		}
		strncpy(ifr.ifr_name, argv[2], sizeof (ifr.ifr_name));
		argc -= 2, argv += 2;
	} else
		strncpy(ifr.ifr_name, "ath0", sizeof (ifr.ifr_name));
	if (argc > 1) {
		u_long interval = strtoul(argv[1], NULL, 0);
		int line, omask;
		u_int rate = getifrate(s, ifr.ifr_name);
		struct ath_stats cur, total;

		if (interval < 1)
			interval = 1;
		signal(SIGALRM, catchalarm);
		signalled = 0;
		alarm(interval);
	banner:
		printf("%8s %8s %7s %7s %7s %6s %6s %5s %7s %4s %4s"
			, "input"
			, "output"
			, "altrate"
			, "short"
			, "long"
			, "xretry"
			, "crcerr"
			, "crypt"
			, "phyerr"
			, "rssi"
			, "rate"
		);
		putchar('\n');
		fflush(stdout);
		line = 0;
	loop:
		if (line != 0) {
			ifr.ifr_data = (caddr_t) &cur;
			if (ioctl(s, SIOCGATHSTATS, &ifr) < 0)
				err(1, ifr.ifr_name);
			rate = getifrate(s, ifr.ifr_name);
			printf("%8u %8u %7u %7u %7u %6u %6u %5u %7u %4u %3uM\n"
				, cur.ast_rx_packets - total.ast_rx_packets
				, cur.ast_tx_packets - total.ast_tx_packets
				, cur.ast_tx_altrate - total.ast_tx_altrate
				, cur.ast_tx_shortretry - total.ast_tx_shortretry
				, cur.ast_tx_longretry - total.ast_tx_longretry
				, cur.ast_tx_xretries - total.ast_tx_xretries
				, cur.ast_rx_crcerr - total.ast_rx_crcerr
				, cur.ast_rx_badcrypt - total.ast_rx_badcrypt
				, cur.ast_rx_phyerr - total.ast_rx_phyerr
				, cur.ast_rx_rssi
				, rate
			);
			total = cur;
		} else {
			ifr.ifr_data = (caddr_t) &total;
			if (ioctl(s, SIOCGATHSTATS, &ifr) < 0)
				err(1, ifr.ifr_name);
			rate = getifrate(s, ifr.ifr_name);
			printf("%8u %8u %7u %7u %7u %6u %6u %5u %7u %4u %3uM\n"
				, total.ast_rx_packets
				, total.ast_tx_packets
				, total.ast_tx_altrate
				, total.ast_tx_shortretry
				, total.ast_tx_longretry
				, total.ast_tx_xretries
				, total.ast_rx_crcerr
				, total.ast_rx_badcrypt
				, total.ast_rx_phyerr
				, total.ast_rx_rssi
				, rate
			);
		}
		fflush(stdout);
		omask = sigblock(sigmask(SIGALRM));
		if (!signalled)
			sigpause(0);
		sigsetmask(omask);
		signalled = 0;
		alarm(interval);
		line++;
		if (line == 21)		/* XXX tty line count */
			goto banner;
		else
			goto loop;
		/*NOTREACHED*/
	} else {
		struct ath_stats stats;

		ifr.ifr_data = (caddr_t) &stats;
		if (ioctl(s, SIOCGATHSTATS, &ifr) < 0)
			err(1, ifr.ifr_name);
		printstats(stdout, &stats);
	}
	return 0;
}
