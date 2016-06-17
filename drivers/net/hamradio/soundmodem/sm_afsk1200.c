/*****************************************************************************/

/*
 *	sm_afsk1200.c  -- soundcard radio modem driver, 1200 baud AFSK modem
 *
 *	Copyright (C) 1996  Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

#include "sm.h"
#include "sm_tbl_afsk1200.h"

/* --------------------------------------------------------------------- */

struct demod_state_afsk12 {
	unsigned int shreg;
	unsigned int bit_pll;
	unsigned char last_sample;
	unsigned int dcd_shreg;
	int dcd_sum0, dcd_sum1, dcd_sum2;
	unsigned int dcd_time;
	unsigned char last_rxbit;
};

struct mod_state_afsk12 {
	unsigned int shreg;
	unsigned char tx_bit;
	unsigned int bit_pll;
	unsigned int dds_inc;
	unsigned int txphase;
};

/* --------------------------------------------------------------------- */

static const int dds_inc[2] = { 
	AFSK12_TX_FREQ_LO*0x10000/AFSK12_SAMPLE_RATE,
	AFSK12_TX_FREQ_HI*0x10000/AFSK12_SAMPLE_RATE 
};

static void modulator_1200_u8(struct sm_state *sm, unsigned char *buf, 
			      unsigned int buflen)
{
	struct mod_state_afsk12 *st = (struct mod_state_afsk12 *)(&sm->m);

	for (; buflen > 0; buflen--) {
		if (!((st->txphase++) & 7)) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = (st->tx_bit ^ (!(st->shreg & 1))) & 1;
			st->shreg >>= 1;
		}
		st->dds_inc = dds_inc[st->tx_bit & 1];
		*buf++ = OFFSCOS(st->bit_pll);
		st->bit_pll += st->dds_inc;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_1200_s16(struct sm_state *sm, short *buf, unsigned int buflen)
{
	struct mod_state_afsk12 *st = (struct mod_state_afsk12 *)(&sm->m);

	for (; buflen > 0; buflen--) {
		if (!((st->txphase++) & 7)) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = (st->tx_bit ^ (!(st->shreg & 1))) & 1;
			st->shreg >>= 1;
		}
		st->dds_inc = dds_inc[st->tx_bit & 1];
		*buf++ = COS(st->bit_pll);
		st->bit_pll += st->dds_inc;
	}
}

/* --------------------------------------------------------------------- */

static inline int convolution8_u8(const unsigned char *st, const int *coeff, int csum)
{
	int sum = -0x80 * csum;
	
	sum += (st[0] * coeff[0]);
	sum += (st[-1] * coeff[1]);
	sum += (st[-2] * coeff[2]);
	sum += (st[-3] * coeff[3]);
	sum += (st[-4] * coeff[4]);
	sum += (st[-5] * coeff[5]);
	sum += (st[-6] * coeff[6]);
	sum += (st[-7] * coeff[7]);

	sum >>= 7;
	return sum * sum;
}

static inline int convolution8_s16(const short *st, const int *coeff, int csum)
{
	int sum = 0;
	
	sum += (st[0] * coeff[0]);
	sum += (st[-1] * coeff[1]);
	sum += (st[-2] * coeff[2]);
	sum += (st[-3] * coeff[3]);
	sum += (st[-4] * coeff[4]);
	sum += (st[-5] * coeff[5]);
	sum += (st[-6] * coeff[6]);
	sum += (st[-7] * coeff[7]);

	sum >>= 15;
	return sum * sum;
}

static inline int do_filter_1200_u8(const unsigned char *buf)
{
	int sum = convolution8_u8(buf, afsk12_tx_lo_i, SUM_AFSK12_TX_LO_I);
	sum += convolution8_u8(buf, afsk12_tx_lo_q, SUM_AFSK12_TX_LO_Q);
	sum -= convolution8_u8(buf, afsk12_tx_hi_i, SUM_AFSK12_TX_HI_I);
	sum -= convolution8_u8(buf, afsk12_tx_hi_q, SUM_AFSK12_TX_HI_Q);
	return sum;
}

static inline int do_filter_1200_s16(const short *buf)
{
	int sum = convolution8_s16(buf, afsk12_tx_lo_i, SUM_AFSK12_TX_LO_I);
	sum += convolution8_s16(buf, afsk12_tx_lo_q, SUM_AFSK12_TX_LO_Q);
	sum -= convolution8_s16(buf, afsk12_tx_hi_i, SUM_AFSK12_TX_HI_I);
	sum -= convolution8_s16(buf, afsk12_tx_hi_q, SUM_AFSK12_TX_HI_Q);
	return sum;
}

/* --------------------------------------------------------------------- */

static const int pll_corr[2] = { -0x1000, 0x1000 };

static void demodulator_1200_u8(struct sm_state *sm, const unsigned char *buf, unsigned int buflen)
{
	struct demod_state_afsk12 *st = (struct demod_state_afsk12 *)(&sm->d);
	int j;
	int sum;
	unsigned char newsample;

	for (; buflen > 0; buflen--, buf++) {
		sum = do_filter_1200_u8(buf);
		st->dcd_shreg <<= 1;
		st->bit_pll += 0x2000;
		newsample = (sum > 0);
		if (st->last_sample ^ newsample) {
			st->last_sample = newsample;
			st->dcd_shreg |= 1;
			st->bit_pll += pll_corr
				[st->bit_pll < 0x9000];
			j = 4 * hweight8(st->dcd_shreg & 0x38)
				- hweight16(st->dcd_shreg & 0x7c0);
			st->dcd_sum0 += j;
		}
		hdlcdrv_channelbit(&sm->hdrv, st->last_sample);
		if ((--st->dcd_time) <= 0) {
			hdlcdrv_setdcd(&sm->hdrv, (st->dcd_sum0 + 
						   st->dcd_sum1 + 
						   st->dcd_sum2) < 0);
			st->dcd_sum2 = st->dcd_sum1;
			st->dcd_sum1 = st->dcd_sum0;
			st->dcd_sum0 = 2; /* slight bias */
			st->dcd_time = 120;
		}
		if (st->bit_pll >= 0x10000) {
			st->bit_pll &= 0xffff;
			st->shreg >>= 1;
			st->shreg |= (!(st->last_rxbit ^
					st->last_sample)) << 16;
			st->last_rxbit = st->last_sample;
			diag_trigger(sm);
			if (st->shreg & 1) {
				hdlcdrv_putbits(&sm->hdrv, st->shreg >> 1);
				st->shreg = 0x10000;
			}
		}
		diag_add(sm, (((int)*buf)-0x80) << 8, sum);
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_1200_s16(struct sm_state *sm, const short *buf, unsigned int buflen)
{
	struct demod_state_afsk12 *st = (struct demod_state_afsk12 *)(&sm->d);
	int j;
	int sum;
	unsigned char newsample;

	for (; buflen > 0; buflen--, buf++) {
		sum = do_filter_1200_s16(buf);
		st->dcd_shreg <<= 1;
		st->bit_pll += 0x2000;
		newsample = (sum > 0);
		if (st->last_sample ^ newsample) {
			st->last_sample = newsample;
			st->dcd_shreg |= 1;
			st->bit_pll += pll_corr
				[st->bit_pll < 0x9000];
			j = 4 * hweight8(st->dcd_shreg & 0x38)
				- hweight16(st->dcd_shreg & 0x7c0);
			st->dcd_sum0 += j;
		}
		hdlcdrv_channelbit(&sm->hdrv, st->last_sample);
		if ((--st->dcd_time) <= 0) {
			hdlcdrv_setdcd(&sm->hdrv, (st->dcd_sum0 + 
						   st->dcd_sum1 + 
						   st->dcd_sum2) < 0);
			st->dcd_sum2 = st->dcd_sum1;
			st->dcd_sum1 = st->dcd_sum0;
			st->dcd_sum0 = 2; /* slight bias */
			st->dcd_time = 120;
		}
		if (st->bit_pll >= 0x10000) {
			st->bit_pll &= 0xffff;
			st->shreg >>= 1;
			st->shreg |= (!(st->last_rxbit ^
					st->last_sample)) << 16;
			st->last_rxbit = st->last_sample;
			diag_trigger(sm);
			if (st->shreg & 1) {
				hdlcdrv_putbits(&sm->hdrv, st->shreg >> 1);
				st->shreg = 0x10000;
			}
		}
		diag_add(sm, *buf, sum);
	}
}

/* --------------------------------------------------------------------- */

static void demod_init_1200(struct sm_state *sm)
{
	struct demod_state_afsk12 *st = (struct demod_state_afsk12 *)(&sm->d);

       	st->dcd_time = 120;
	st->dcd_sum0 = 2;	
}

/* --------------------------------------------------------------------- */

const struct modem_tx_info sm_afsk1200_tx = {
	"afsk1200", sizeof(struct mod_state_afsk12), 
	AFSK12_SAMPLE_RATE, 1200, modulator_1200_u8, modulator_1200_s16, NULL
};

const struct modem_rx_info sm_afsk1200_rx = {
	"afsk1200", sizeof(struct demod_state_afsk12), 
	AFSK12_SAMPLE_RATE, 1200, 8, AFSK12_SAMPLE_RATE/1200, 
	demodulator_1200_u8, demodulator_1200_s16, demod_init_1200
};

/* --------------------------------------------------------------------- */
