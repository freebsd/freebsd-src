/*
 * Copyright (c) 2008-2010 Atheros Communications Inc.
 * Copyright (c) 2011 Adrian Chadd, Xenion Pty Ltd.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include "opt_ah.h"

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"
#include "ah_eeprom_v4k.h"

#include "ar9002/ar9280.h"
#include "ar9002/ar9285_diversity.h"
#include "ar9002/ar9285.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"
#include "ar9002/ar9285phy.h"
#include "ar9002/ar9285_phy.h"


/* Linux compability macros */
/*
 * XXX these don't handle rounding, underflow, overflow, wrapping!
 */
#define	msecs_to_jiffies(a)		( (a) * hz / 1000 )
#define	time_after(a, b)		( (long) (b) - (long) (a) < 0 )

static HAL_BOOL
ath_is_alt_ant_ratio_better(int alt_ratio, int maxdelta, int mindelta,
    int main_rssi_avg, int alt_rssi_avg, int pkt_count)
{
	return (((alt_ratio >= ATH_ANT_DIV_COMB_ALT_ANT_RATIO2) &&
		(alt_rssi_avg > main_rssi_avg + maxdelta)) ||
		(alt_rssi_avg > main_rssi_avg + mindelta)) && (pkt_count > 50);
}

static void
ath_lnaconf_alt_good_scan(struct ar9285_ant_comb *antcomb,
    struct ar9285_antcomb_conf ant_conf, int main_rssi_avg)
{
	antcomb->quick_scan_cnt = 0;

	if (ant_conf.main_lna_conf == ATH_ANT_DIV_COMB_LNA2)
		antcomb->rssi_lna2 = main_rssi_avg;
	else if (ant_conf.main_lna_conf == ATH_ANT_DIV_COMB_LNA1)
		antcomb->rssi_lna1 = main_rssi_avg;

	switch ((ant_conf.main_lna_conf << 4) | ant_conf.alt_lna_conf) {
	case (0x10): /* LNA2 A-B */
		antcomb->main_conf = ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2;
		antcomb->first_quick_scan_conf =
			ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
		antcomb->second_quick_scan_conf = ATH_ANT_DIV_COMB_LNA1;
		break;
	case (0x20): /* LNA1 A-B */
		antcomb->main_conf = ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2;
		antcomb->first_quick_scan_conf =
			ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
		antcomb->second_quick_scan_conf = ATH_ANT_DIV_COMB_LNA2;
		break;
	case (0x21): /* LNA1 LNA2 */
		antcomb->main_conf = ATH_ANT_DIV_COMB_LNA2;
		antcomb->first_quick_scan_conf =
			ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2;
		antcomb->second_quick_scan_conf =
			ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
		break;
	case (0x12): /* LNA2 LNA1 */
		antcomb->main_conf = ATH_ANT_DIV_COMB_LNA1;
		antcomb->first_quick_scan_conf =
			ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2;
		antcomb->second_quick_scan_conf =
			ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
		break;
	case (0x13): /* LNA2 A+B */
		antcomb->main_conf = ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
		antcomb->first_quick_scan_conf =
			ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2;
		antcomb->second_quick_scan_conf = ATH_ANT_DIV_COMB_LNA1;
		break;
	case (0x23): /* LNA1 A+B */
		antcomb->main_conf = ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
		antcomb->first_quick_scan_conf =
			ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2;
		antcomb->second_quick_scan_conf = ATH_ANT_DIV_COMB_LNA2;
		break;
	default:
		break;
	}
}

static void
ath_select_ant_div_from_quick_scan(struct ar9285_ant_comb *antcomb,
    struct ar9285_antcomb_conf *div_ant_conf, int main_rssi_avg,
    int alt_rssi_avg, int alt_ratio)
{
	/* alt_good */
	switch (antcomb->quick_scan_cnt) {
	case 0:
		/* set alt to main, and alt to first conf */
		div_ant_conf->main_lna_conf = antcomb->main_conf;
		div_ant_conf->alt_lna_conf = antcomb->first_quick_scan_conf;
		break;
	case 1:
		/* set alt to main, and alt to first conf */
		div_ant_conf->main_lna_conf = antcomb->main_conf;
		div_ant_conf->alt_lna_conf = antcomb->second_quick_scan_conf;
		antcomb->rssi_first = main_rssi_avg;
		antcomb->rssi_second = alt_rssi_avg;

		if (antcomb->main_conf == ATH_ANT_DIV_COMB_LNA1) {
			/* main is LNA1 */
			if (ath_is_alt_ant_ratio_better(alt_ratio,
						ATH_ANT_DIV_COMB_LNA1_DELTA_HI,
						ATH_ANT_DIV_COMB_LNA1_DELTA_LOW,
						main_rssi_avg, alt_rssi_avg,
						antcomb->total_pkt_count))
				antcomb->first_ratio = AH_TRUE;
			else
				antcomb->first_ratio = AH_FALSE;
		} else if (antcomb->main_conf == ATH_ANT_DIV_COMB_LNA2) {
			if (ath_is_alt_ant_ratio_better(alt_ratio,
						ATH_ANT_DIV_COMB_LNA1_DELTA_MID,
						ATH_ANT_DIV_COMB_LNA1_DELTA_LOW,
						main_rssi_avg, alt_rssi_avg,
						antcomb->total_pkt_count))
				antcomb->first_ratio = AH_TRUE;
			else
				antcomb->first_ratio = AH_FALSE;
		} else {
			if ((((alt_ratio >= ATH_ANT_DIV_COMB_ALT_ANT_RATIO2) &&
			    (alt_rssi_avg > main_rssi_avg +
			    ATH_ANT_DIV_COMB_LNA1_DELTA_HI)) ||
			    (alt_rssi_avg > main_rssi_avg)) &&
			    (antcomb->total_pkt_count > 50))
				antcomb->first_ratio = AH_TRUE;
			else
				antcomb->first_ratio = AH_FALSE;
		}
		break;
	case 2:
		antcomb->alt_good = AH_FALSE;
		antcomb->scan_not_start = AH_FALSE;
		antcomb->scan = AH_FALSE;
		antcomb->rssi_first = main_rssi_avg;
		antcomb->rssi_third = alt_rssi_avg;

		if (antcomb->second_quick_scan_conf == ATH_ANT_DIV_COMB_LNA1)
			antcomb->rssi_lna1 = alt_rssi_avg;
		else if (antcomb->second_quick_scan_conf ==
			 ATH_ANT_DIV_COMB_LNA2)
			antcomb->rssi_lna2 = alt_rssi_avg;
		else if (antcomb->second_quick_scan_conf ==
			 ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2) {
			if (antcomb->main_conf == ATH_ANT_DIV_COMB_LNA2)
				antcomb->rssi_lna2 = main_rssi_avg;
			else if (antcomb->main_conf == ATH_ANT_DIV_COMB_LNA1)
				antcomb->rssi_lna1 = main_rssi_avg;
		}

		if (antcomb->rssi_lna2 > antcomb->rssi_lna1 +
		    ATH_ANT_DIV_COMB_LNA1_LNA2_SWITCH_DELTA)
			div_ant_conf->main_lna_conf = ATH_ANT_DIV_COMB_LNA2;
		else
			div_ant_conf->main_lna_conf = ATH_ANT_DIV_COMB_LNA1;

		if (antcomb->main_conf == ATH_ANT_DIV_COMB_LNA1) {
			if (ath_is_alt_ant_ratio_better(alt_ratio,
						ATH_ANT_DIV_COMB_LNA1_DELTA_HI,
						ATH_ANT_DIV_COMB_LNA1_DELTA_LOW,
						main_rssi_avg, alt_rssi_avg,
						antcomb->total_pkt_count))
				antcomb->second_ratio = AH_TRUE;
			else
				antcomb->second_ratio = AH_FALSE;
		} else if (antcomb->main_conf == ATH_ANT_DIV_COMB_LNA2) {
			if (ath_is_alt_ant_ratio_better(alt_ratio,
						ATH_ANT_DIV_COMB_LNA1_DELTA_MID,
						ATH_ANT_DIV_COMB_LNA1_DELTA_LOW,
						main_rssi_avg, alt_rssi_avg,
						antcomb->total_pkt_count))
				antcomb->second_ratio = AH_TRUE;
			else
				antcomb->second_ratio = AH_FALSE;
		} else {
			if ((((alt_ratio >= ATH_ANT_DIV_COMB_ALT_ANT_RATIO2) &&
			    (alt_rssi_avg > main_rssi_avg +
			    ATH_ANT_DIV_COMB_LNA1_DELTA_HI)) ||
			    (alt_rssi_avg > main_rssi_avg)) &&
			    (antcomb->total_pkt_count > 50))
				antcomb->second_ratio = AH_TRUE;
			else
				antcomb->second_ratio = AH_FALSE;
		}

		/* set alt to the conf with maximun ratio */
		if (antcomb->first_ratio && antcomb->second_ratio) {
			if (antcomb->rssi_second > antcomb->rssi_third) {
				/* first alt*/
				if ((antcomb->first_quick_scan_conf ==
				    ATH_ANT_DIV_COMB_LNA1) ||
				    (antcomb->first_quick_scan_conf ==
				    ATH_ANT_DIV_COMB_LNA2))
					/* Set alt LNA1 or LNA2*/
					if (div_ant_conf->main_lna_conf ==
					    ATH_ANT_DIV_COMB_LNA2)
						div_ant_conf->alt_lna_conf =
							ATH_ANT_DIV_COMB_LNA1;
					else
						div_ant_conf->alt_lna_conf =
							ATH_ANT_DIV_COMB_LNA2;
				else
					/* Set alt to A+B or A-B */
					div_ant_conf->alt_lna_conf =
						antcomb->first_quick_scan_conf;
			} else if ((antcomb->second_quick_scan_conf ==
				   ATH_ANT_DIV_COMB_LNA1) ||
				   (antcomb->second_quick_scan_conf ==
				   ATH_ANT_DIV_COMB_LNA2)) {
				/* Set alt LNA1 or LNA2 */
				if (div_ant_conf->main_lna_conf ==
				    ATH_ANT_DIV_COMB_LNA2)
					div_ant_conf->alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
				else
					div_ant_conf->alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;
			} else {
				/* Set alt to A+B or A-B */
				div_ant_conf->alt_lna_conf =
					antcomb->second_quick_scan_conf;
			}
		} else if (antcomb->first_ratio) {
			/* first alt */
			if ((antcomb->first_quick_scan_conf ==
			    ATH_ANT_DIV_COMB_LNA1) ||
			    (antcomb->first_quick_scan_conf ==
			    ATH_ANT_DIV_COMB_LNA2))
					/* Set alt LNA1 or LNA2 */
				if (div_ant_conf->main_lna_conf ==
				    ATH_ANT_DIV_COMB_LNA2)
					div_ant_conf->alt_lna_conf =
							ATH_ANT_DIV_COMB_LNA1;
				else
					div_ant_conf->alt_lna_conf =
							ATH_ANT_DIV_COMB_LNA2;
			else
				/* Set alt to A+B or A-B */
				div_ant_conf->alt_lna_conf =
						antcomb->first_quick_scan_conf;
		} else if (antcomb->second_ratio) {
				/* second alt */
			if ((antcomb->second_quick_scan_conf ==
			    ATH_ANT_DIV_COMB_LNA1) ||
			    (antcomb->second_quick_scan_conf ==
			    ATH_ANT_DIV_COMB_LNA2))
				/* Set alt LNA1 or LNA2 */
				if (div_ant_conf->main_lna_conf ==
				    ATH_ANT_DIV_COMB_LNA2)
					div_ant_conf->alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
				else
					div_ant_conf->alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;
			else
				/* Set alt to A+B or A-B */
				div_ant_conf->alt_lna_conf =
						antcomb->second_quick_scan_conf;
		} else {
			/* main is largest */
			if ((antcomb->main_conf == ATH_ANT_DIV_COMB_LNA1) ||
			    (antcomb->main_conf == ATH_ANT_DIV_COMB_LNA2))
				/* Set alt LNA1 or LNA2 */
				if (div_ant_conf->main_lna_conf ==
				    ATH_ANT_DIV_COMB_LNA2)
					div_ant_conf->alt_lna_conf =
							ATH_ANT_DIV_COMB_LNA1;
				else
					div_ant_conf->alt_lna_conf =
							ATH_ANT_DIV_COMB_LNA2;
			else
				/* Set alt to A+B or A-B */
				div_ant_conf->alt_lna_conf = antcomb->main_conf;
		}
		break;
	default:
		break;
	}
}

static void
ath_ant_div_conf_fast_divbias(struct ar9285_antcomb_conf *ant_conf)
{
	/* Adjust the fast_div_bias based on main and alt lna conf */
	switch ((ant_conf->main_lna_conf << 4) | ant_conf->alt_lna_conf) {
	case (0x01): /* A-B LNA2 */
		ant_conf->fast_div_bias = 0x3b;
		break;
	case (0x02): /* A-B LNA1 */
		ant_conf->fast_div_bias = 0x3d;
		break;
	case (0x03): /* A-B A+B */
		ant_conf->fast_div_bias = 0x1;
		break;
	case (0x10): /* LNA2 A-B */
		ant_conf->fast_div_bias = 0x7;
		break;
	case (0x12): /* LNA2 LNA1 */
		ant_conf->fast_div_bias = 0x2;
		break;
	case (0x13): /* LNA2 A+B */
		ant_conf->fast_div_bias = 0x7;
		break;
	case (0x20): /* LNA1 A-B */
		ant_conf->fast_div_bias = 0x6;
		break;
	case (0x21): /* LNA1 LNA2 */
		ant_conf->fast_div_bias = 0x0;
		break;
	case (0x23): /* LNA1 A+B */
		ant_conf->fast_div_bias = 0x6;
		break;
	case (0x30): /* A+B A-B */
		ant_conf->fast_div_bias = 0x1;
		break;
	case (0x31): /* A+B LNA2 */
		ant_conf->fast_div_bias = 0x3b;
		break;
	case (0x32): /* A+B LNA1 */
		ant_conf->fast_div_bias = 0x3d;
		break;
	default:
		break;
	}
}

/* Antenna diversity and combining */
void
ar9285_ant_comb_scan(struct ath_hal *ah, struct ath_rx_status *rs,
    unsigned long ticks, int hz)
{
	struct ar9285_antcomb_conf div_ant_conf;
	struct ar9285_ant_comb *antcomb = &AH9285(ah)->ant_comb;
	int alt_ratio = 0, alt_rssi_avg = 0, main_rssi_avg = 0, curr_alt_set;
	int curr_main_set, curr_bias;
	int main_rssi = rs->rs_rssi_ctl[0];
	int alt_rssi = rs->rs_rssi_ctl[1];
	int rx_ant_conf, main_ant_conf;
	HAL_BOOL short_scan = AH_FALSE;

	if (! ar9285_check_div_comb(ah))
		return;

	if (AH5212(ah)->ah_diversity == AH_FALSE)
		return;

	rx_ant_conf = (rs->rs_rssi_ctl[2] >> ATH_ANT_RX_CURRENT_SHIFT) &
		       ATH_ANT_RX_MASK;
	main_ant_conf = (rs->rs_rssi_ctl[2] >> ATH_ANT_RX_MAIN_SHIFT) &
			 ATH_ANT_RX_MASK;

#if 0
	HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: main: %d, alt: %d, rx_ant_conf: %x, main_ant_conf: %x\n",
	    __func__, main_rssi, alt_rssi, rx_ant_conf, main_ant_conf);
#endif

	/* Record packet only when alt_rssi is positive */
	if (alt_rssi > 0) {
		antcomb->total_pkt_count++;
		antcomb->main_total_rssi += main_rssi;
		antcomb->alt_total_rssi  += alt_rssi;
		if (main_ant_conf == rx_ant_conf)
			antcomb->main_recv_cnt++;
		else
			antcomb->alt_recv_cnt++;
	}

	/* Short scan check */
	if (antcomb->scan && antcomb->alt_good) {
		if (time_after(ticks, antcomb->scan_start_time +
		    msecs_to_jiffies(ATH_ANT_DIV_COMB_SHORT_SCAN_INTR)))
			short_scan = AH_TRUE;
		else
			if (antcomb->total_pkt_count ==
			    ATH_ANT_DIV_COMB_SHORT_SCAN_PKTCOUNT) {
				alt_ratio = ((antcomb->alt_recv_cnt * 100) /
					    antcomb->total_pkt_count);
				if (alt_ratio < ATH_ANT_DIV_COMB_ALT_ANT_RATIO)
					short_scan = AH_TRUE;
			}
	}

	if (((antcomb->total_pkt_count < ATH_ANT_DIV_COMB_MAX_PKTCOUNT) ||
	    rs->rs_moreaggr) && !short_scan)
		return;

	if (antcomb->total_pkt_count) {
		alt_ratio = ((antcomb->alt_recv_cnt * 100) /
			     antcomb->total_pkt_count);
		main_rssi_avg = (antcomb->main_total_rssi /
				 antcomb->total_pkt_count);
		alt_rssi_avg = (antcomb->alt_total_rssi /
				 antcomb->total_pkt_count);
	}

	ar9285_antdiv_comb_conf_get(ah, &div_ant_conf);
	curr_alt_set = div_ant_conf.alt_lna_conf;
	curr_main_set = div_ant_conf.main_lna_conf;
	curr_bias = div_ant_conf.fast_div_bias;

	antcomb->count++;

	if (antcomb->count == ATH_ANT_DIV_COMB_MAX_COUNT) {
		if (alt_ratio > ATH_ANT_DIV_COMB_ALT_ANT_RATIO) {
			ath_lnaconf_alt_good_scan(antcomb, div_ant_conf,
						  main_rssi_avg);
			antcomb->alt_good = AH_TRUE;
		} else {
			antcomb->alt_good = AH_FALSE;
		}

		antcomb->count = 0;
		antcomb->scan = AH_TRUE;
		antcomb->scan_not_start = AH_TRUE;
	}

	if (!antcomb->scan) {
		if (alt_ratio > ATH_ANT_DIV_COMB_ALT_ANT_RATIO) {
			if (curr_alt_set == ATH_ANT_DIV_COMB_LNA2) {
				/* Switch main and alt LNA */
				div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;
				div_ant_conf.alt_lna_conf  =
						ATH_ANT_DIV_COMB_LNA1;
			} else if (curr_alt_set == ATH_ANT_DIV_COMB_LNA1) {
				div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
				div_ant_conf.alt_lna_conf  =
						ATH_ANT_DIV_COMB_LNA2;
			}

			goto div_comb_done;
		} else if ((curr_alt_set != ATH_ANT_DIV_COMB_LNA1) &&
			   (curr_alt_set != ATH_ANT_DIV_COMB_LNA2)) {
			/* Set alt to another LNA */
			if (curr_main_set == ATH_ANT_DIV_COMB_LNA2)
				div_ant_conf.alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
			else if (curr_main_set == ATH_ANT_DIV_COMB_LNA1)
				div_ant_conf.alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;

			goto div_comb_done;
		}

		if ((alt_rssi_avg < (main_rssi_avg +
		    ATH_ANT_DIV_COMB_LNA1_LNA2_DELTA)))
			goto div_comb_done;
	}

	if (!antcomb->scan_not_start) {
		switch (curr_alt_set) {
		case ATH_ANT_DIV_COMB_LNA2:
			antcomb->rssi_lna2 = alt_rssi_avg;
			antcomb->rssi_lna1 = main_rssi_avg;
			antcomb->scan = AH_TRUE;
			/* set to A+B */
			div_ant_conf.main_lna_conf =
				ATH_ANT_DIV_COMB_LNA1;
			div_ant_conf.alt_lna_conf  =
				ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
			break;
		case ATH_ANT_DIV_COMB_LNA1:
			antcomb->rssi_lna1 = alt_rssi_avg;
			antcomb->rssi_lna2 = main_rssi_avg;
			antcomb->scan = AH_TRUE;
			/* set to A+B */
			div_ant_conf.main_lna_conf = ATH_ANT_DIV_COMB_LNA2;
			div_ant_conf.alt_lna_conf  =
				ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
			break;
		case ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2:
			antcomb->rssi_add = alt_rssi_avg;
			antcomb->scan = AH_TRUE;
			/* set to A-B */
			div_ant_conf.alt_lna_conf =
				ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2;
			break;
		case ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2:
			antcomb->rssi_sub = alt_rssi_avg;
			antcomb->scan = AH_FALSE;
			if (antcomb->rssi_lna2 >
			    (antcomb->rssi_lna1 +
			    ATH_ANT_DIV_COMB_LNA1_LNA2_SWITCH_DELTA)) {
				/* use LNA2 as main LNA */
				if ((antcomb->rssi_add > antcomb->rssi_lna1) &&
				    (antcomb->rssi_add > antcomb->rssi_sub)) {
					/* set to A+B */
					div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;
					div_ant_conf.alt_lna_conf  =
						ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
				} else if (antcomb->rssi_sub >
					   antcomb->rssi_lna1) {
					/* set to A-B */
					div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;
					div_ant_conf.alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2;
				} else {
					/* set to LNA1 */
					div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;
					div_ant_conf.alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
				}
			} else {
				/* use LNA1 as main LNA */
				if ((antcomb->rssi_add > antcomb->rssi_lna2) &&
				    (antcomb->rssi_add > antcomb->rssi_sub)) {
					/* set to A+B */
					div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
					div_ant_conf.alt_lna_conf  =
						ATH_ANT_DIV_COMB_LNA1_PLUS_LNA2;
				} else if (antcomb->rssi_sub >
					   antcomb->rssi_lna1) {
					/* set to A-B */
					div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
					div_ant_conf.alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA1_MINUS_LNA2;
				} else {
					/* set to LNA2 */
					div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
					div_ant_conf.alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;
				}
			}
			break;
		default:
			break;
		}
	} else {
		if (!antcomb->alt_good) {
			antcomb->scan_not_start = AH_FALSE;
			/* Set alt to another LNA */
			if (curr_main_set == ATH_ANT_DIV_COMB_LNA2) {
				div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;
				div_ant_conf.alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
			} else if (curr_main_set == ATH_ANT_DIV_COMB_LNA1) {
				div_ant_conf.main_lna_conf =
						ATH_ANT_DIV_COMB_LNA1;
				div_ant_conf.alt_lna_conf =
						ATH_ANT_DIV_COMB_LNA2;
			}
			goto div_comb_done;
		}
	}

	ath_select_ant_div_from_quick_scan(antcomb, &div_ant_conf,
					   main_rssi_avg, alt_rssi_avg,
					   alt_ratio);

	antcomb->quick_scan_cnt++;

div_comb_done:
	ath_ant_div_conf_fast_divbias(&div_ant_conf);

	ar9285_antdiv_comb_conf_set(ah, &div_ant_conf);

	HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: total_pkt_count=%d\n",
	   __func__, antcomb->total_pkt_count);

	HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: main_total_rssi=%d\n",
	   __func__, antcomb->main_total_rssi);
	HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: alt_total_rssi=%d\n",
	   __func__, antcomb->alt_total_rssi);

	HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: main_rssi_avg=%d\n",
	   __func__, main_rssi_avg);
	HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: alt_alt_rssi_avg=%d\n",
	   __func__, alt_rssi_avg);

	HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: main_recv_cnt=%d\n",
	   __func__, antcomb->main_recv_cnt);
	HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: alt_recv_cnt=%d\n",
	   __func__, antcomb->alt_recv_cnt);

	if (curr_alt_set != div_ant_conf.alt_lna_conf)
		HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: lna_conf: %x -> %x\n",
		    __func__, curr_alt_set, div_ant_conf.alt_lna_conf);
	if (curr_main_set != div_ant_conf.main_lna_conf)
		HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: main_lna_conf: %x -> %x\n",
		    __func__, curr_main_set, div_ant_conf.main_lna_conf);
	if (curr_bias != div_ant_conf.fast_div_bias)
		HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: fast_div_bias: %x -> %x\n",
		    __func__, curr_bias, div_ant_conf.fast_div_bias);

	antcomb->scan_start_time = ticks;
	antcomb->total_pkt_count = 0;
	antcomb->main_total_rssi = 0;
	antcomb->alt_total_rssi = 0;
	antcomb->main_recv_cnt = 0;
	antcomb->alt_recv_cnt = 0;
}

/*
 * Set the antenna switch to control RX antenna diversity.
 *
 * If a fixed configuration is used, the LNA and div bias
 * settings are fixed and the antenna diversity scanning routine
 * is disabled.
 *
 * If a variable configuration is used, a default is programmed
 * in and sampling commences per RXed packet.
 *
 * Since this is called from ar9285SetBoardValues() to setup
 * diversity, it means that after a reset or scan, any current
 * software diversity combining settings will be lost and won't
 * re-appear until after the first successful sample run.
 * Please keep this in mind if you're seeing weird performance
 * that happens to relate to scan/diversity timing.
 */
HAL_BOOL
ar9285SetAntennaSwitch(struct ath_hal *ah, HAL_ANT_SETTING settings)
{
	int regVal;
	const HAL_EEPROM_v4k *ee = AH_PRIVATE(ah)->ah_eeprom;
	const MODAL_EEP4K_HEADER *pModal = &ee->ee_base.modalHeader;
	uint8_t ant_div_control1, ant_div_control2;

	if (pModal->version < 3) {
		HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: not supported\n",
	    __func__);
		return AH_FALSE;	/* Can't do diversity */
	}

	/* Store settings */
	AH5212(ah)->ah_antControl = settings;
	AH5212(ah)->ah_diversity = (settings == HAL_ANT_VARIABLE);
	
	/* XXX don't fiddle if the PHY is in sleep mode or ! chan */

	/* Begin setting the relevant registers */

	ant_div_control1 = pModal->antdiv_ctl1;
	ant_div_control2 = pModal->antdiv_ctl2;

	regVal = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
	regVal &= (~(AR_PHY_9285_ANT_DIV_CTL_ALL));

	/* enable antenna diversity only if diversityControl == HAL_ANT_VARIABLE */
	if (settings == HAL_ANT_VARIABLE)
	    regVal |= SM(ant_div_control1, AR_PHY_9285_ANT_DIV_CTL);

	if (settings == HAL_ANT_VARIABLE) {
	    HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: HAL_ANT_VARIABLE\n",
	      __func__);
	    regVal |= SM(ant_div_control2, AR_PHY_9285_ANT_DIV_ALT_LNACONF);
	    regVal |= SM((ant_div_control2 >> 2), AR_PHY_9285_ANT_DIV_MAIN_LNACONF);
	    regVal |= SM((ant_div_control1 >> 1), AR_PHY_9285_ANT_DIV_ALT_GAINTB);
	    regVal |= SM((ant_div_control1 >> 2), AR_PHY_9285_ANT_DIV_MAIN_GAINTB);
	} else {
	    if (settings == HAL_ANT_FIXED_A) {
		/* Diversity disabled, RX = LNA1 */
		HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: HAL_ANT_FIXED_A\n",
		    __func__);
		regVal |= SM(ATH_ANT_DIV_COMB_LNA2, AR_PHY_9285_ANT_DIV_ALT_LNACONF);
		regVal |= SM(ATH_ANT_DIV_COMB_LNA1, AR_PHY_9285_ANT_DIV_MAIN_LNACONF);
		regVal |= SM(AR_PHY_9285_ANT_DIV_GAINTB_0, AR_PHY_9285_ANT_DIV_ALT_GAINTB);
		regVal |= SM(AR_PHY_9285_ANT_DIV_GAINTB_1, AR_PHY_9285_ANT_DIV_MAIN_GAINTB);
	    }
	    else if (settings == HAL_ANT_FIXED_B) {
		/* Diversity disabled, RX = LNA2 */
		HALDEBUG(ah, HAL_DEBUG_DIVERSITY, "%s: HAL_ANT_FIXED_B\n",
		    __func__);
		regVal |= SM(ATH_ANT_DIV_COMB_LNA1, AR_PHY_9285_ANT_DIV_ALT_LNACONF);
		regVal |= SM(ATH_ANT_DIV_COMB_LNA2, AR_PHY_9285_ANT_DIV_MAIN_LNACONF);
		regVal |= SM(AR_PHY_9285_ANT_DIV_GAINTB_1, AR_PHY_9285_ANT_DIV_ALT_GAINTB);
		regVal |= SM(AR_PHY_9285_ANT_DIV_GAINTB_0, AR_PHY_9285_ANT_DIV_MAIN_GAINTB);
	    }
	}

	OS_REG_WRITE(ah, AR_PHY_MULTICHAIN_GAIN_CTL, regVal);
	regVal = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
	regVal = OS_REG_READ(ah, AR_PHY_CCK_DETECT);
	regVal &= (~AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV);
	if (settings == HAL_ANT_VARIABLE)
	    regVal |= SM((ant_div_control1 >> 3), AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV);

	OS_REG_WRITE(ah, AR_PHY_CCK_DETECT, regVal);
	regVal = OS_REG_READ(ah, AR_PHY_CCK_DETECT);

	/*
	 * If Diversity combining is available and the diversity setting
	 * is to allow variable diversity, enable it by default.
	 *
	 * This will be eventually overridden by the software antenna
	 * diversity logic.
	 *
	 * Note that yes, this following section overrides the above
	 * settings for the LNA configuration and fast-bias.
	 */
	if (ar9285_check_div_comb(ah) && AH5212(ah)->ah_diversity == AH_TRUE) {
		// If support DivComb, set MAIN to LNA1 and ALT to LNA2 at the first beginning
		HALDEBUG(ah, HAL_DEBUG_DIVERSITY,
		    "%s: Enable initial settings for combined diversity\n",
		    __func__);
		regVal = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
		regVal &= (~(AR_PHY_9285_ANT_DIV_MAIN_LNACONF | AR_PHY_9285_ANT_DIV_ALT_LNACONF));
		regVal |= (ATH_ANT_DIV_COMB_LNA1 << AR_PHY_9285_ANT_DIV_MAIN_LNACONF_S);
		regVal |= (ATH_ANT_DIV_COMB_LNA2 << AR_PHY_9285_ANT_DIV_ALT_LNACONF_S);
		regVal &= (~(AR_PHY_9285_FAST_DIV_BIAS));
		regVal |= (0 << AR_PHY_9285_FAST_DIV_BIAS_S);
		OS_REG_WRITE(ah, AR_PHY_MULTICHAIN_GAIN_CTL, regVal);
	}

	return AH_TRUE;
}
