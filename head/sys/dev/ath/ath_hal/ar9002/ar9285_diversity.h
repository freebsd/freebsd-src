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
#ifndef	__AR9285_DIVERSITY_H__
#define	__AR9285_DIVERSITY_H__

/* Antenna diversity/combining */
#define	ATH_ANT_RX_CURRENT_SHIFT	4
#define	ATH_ANT_RX_MAIN_SHIFT		2
#define	ATH_ANT_RX_MASK			0x3

#define	ATH_ANT_DIV_COMB_SHORT_SCAN_INTR	50
#define	ATH_ANT_DIV_COMB_SHORT_SCAN_PKTCOUNT	0x100
#define	ATH_ANT_DIV_COMB_MAX_PKTCOUNT		0x200
#define	ATH_ANT_DIV_COMB_INIT_COUNT		95
#define	ATH_ANT_DIV_COMB_MAX_COUNT		100
#define	ATH_ANT_DIV_COMB_ALT_ANT_RATIO		30
#define	ATH_ANT_DIV_COMB_ALT_ANT_RATIO2		20

#define	ATH_ANT_DIV_COMB_LNA1_LNA2_DELTA	-3
#define	ATH_ANT_DIV_COMB_LNA1_LNA2_SWITCH_DELTA	-1
#define	ATH_ANT_DIV_COMB_LNA1_DELTA_HI		-4
#define	ATH_ANT_DIV_COMB_LNA1_DELTA_MID		-2
#define	ATH_ANT_DIV_COMB_LNA1_DELTA_LOW		2

extern	void ar9285_ant_comb_scan(struct ath_hal *ah, struct ath_rx_status *rs,
		unsigned long ticks, int hz);

#endif
