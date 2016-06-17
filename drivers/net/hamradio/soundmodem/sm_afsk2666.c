/*****************************************************************************/

/*
 *	sm_afsk2666.c  -- soundcard radio modem driver, 2666 baud AFSK modem
 *
 *	Copyright (C) 1997  Thomas Sailer (sailer@ife.ee.ethz.ch)
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
#include "sm_tbl_afsk2666.h"

/* --------------------------------------------------------------------- */

struct demod_state_afsk26 {
	unsigned int shreg;
	unsigned long descram;
	int dem_sum[8];
	int dem_sum_mean;
	int dem_cnt;
	unsigned int bit_pll;
	unsigned char last_sample;
	unsigned int dcd_shreg;
	int dcd_sum0, dcd_sum1, dcd_sum2;
	unsigned int dcd_time;
};

struct mod_state_afsk26 {
	unsigned int shreg;
	unsigned long scram;
	unsigned int bit_pll;
	unsigned int phinc;
	unsigned int tx_seq;
};

/* --------------------------------------------------------------------- */

#define DESCRAM_TAP1 0x20000
#define DESCRAM_TAP2 0x01000
#define DESCRAM_TAP3 0x00001

#define DESCRAM_TAPSH1 17
#define DESCRAM_TAPSH2 12
#define DESCRAM_TAPSH3 0

#define SCRAM_TAP1 0x20000 /* X^17 */
#define SCRAM_TAPN 0x00021 /* X^0+X^5 */

/* --------------------------------------------------------------------- */

static void modulator_2666_u8(struct sm_state *sm, unsigned char *buf, unsigned int buflen)
{
	struct mod_state_afsk26 *st = (struct mod_state_afsk26 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->scram = ((st->scram << 1) | (st->scram & 1));
			st->scram ^= (!(st->shreg & 1));
			st->shreg >>= 1;
			if (st->scram & (SCRAM_TAP1 << 1))
				st->scram ^= SCRAM_TAPN << 1;
			st->phinc = afsk26_carfreq[!(st->scram & (SCRAM_TAP1 << 2))];
		}
		if (st->tx_seq >= 6)
			st->tx_seq = 0;
		*buf = OFFSCOS(st->bit_pll);
		st->bit_pll += st->phinc;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_2666_s16(struct sm_state *sm, short *buf, unsigned int buflen)
{
	struct mod_state_afsk26 *st = (struct mod_state_afsk26 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->scram = ((st->scram << 1) | (st->scram & 1));
			st->scram ^= (!(st->shreg & 1));
			st->shreg >>= 1;
			if (st->scram & (SCRAM_TAP1 << 1))
				st->scram ^= SCRAM_TAPN << 1;
			st->phinc = afsk26_carfreq[!(st->scram & (SCRAM_TAP1 << 2))];
		}
		if (st->tx_seq >= 6)
			st->tx_seq = 0;
		*buf = COS(st->bit_pll);
		st->bit_pll += st->phinc;
	}
}

/* --------------------------------------------------------------------- */

static inline int convolution12_u8(const unsigned char *st, const int *coeff, int csum)
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
	sum += (st[-8] * coeff[8]);
	sum += (st[-9] * coeff[9]);
	sum += (st[-10] * coeff[10]);
	sum += (st[-11] * coeff[11]);

	return sum;
}

static inline int convolution12_s16(const short *st, const int *coeff, int csum)
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
	sum += (st[-8] * coeff[8]);
	sum += (st[-9] * coeff[9]);
	sum += (st[-10] * coeff[10]);
	sum += (st[-11] * coeff[11]);

	sum >>= 8;
	return sum;
}

/* ---------------------------------------------------------------------- */

#if 0
static int binexp(unsigned int i)
{
	int ret = 31;
	
	if (!i)
		return 0;
	if (i < 0x10000LU) {
		i <<= 16;
		ret -= 16;
	}
	if (i < 0x1000000LU) {
		i <<= 8;
		ret -= 8;
	}
	if (i < 0x10000000LU) {
		i <<= 4;
		ret -= 4;
	}
	if (i < 0x40000000LU) {
		i <<= 2;
		ret -= 2;
	}
	if (i < 0x80000000LU)
		ret -= 1;
	return ret;
}

static const sqrt_tab[16] = {
	00000, 16384, 23170, 28378, 32768, 36636, 40132, 43348,
	46341, 49152, 51811, 54340, 56756, 59073, 61303, 63455
};


static unsigned int int_sqrt_approx(unsigned int i)
{
	unsigned int j;

	if (i < 16)
		return sqrt_tab[i] >> 14;
	j = binexp(i) >> 1;
	i >>= (j * 2 - 2);
      	return (sqrt_tab[i & 0xf] << j) >> 15;
}
#endif

/* --------------------------------------------------------------------- */

extern unsigned int est_pwr(int i, int q)
{
	unsigned int ui = abs(i);
	unsigned int uq = abs(q);

	if (uq > ui) {
		unsigned int tmp;
		tmp = ui;
		ui = uq;
		uq = tmp;
	}
	if (uq > (ui >> 1))
		return 7*(ui>>3) + 9*(uq>>4);
	else
		return ui + (uq>>2);
}

/* --------------------------------------------------------------------- */

static void demod_one_sample(struct sm_state *sm, struct demod_state_afsk26 *st, int curval,
			     int loi, int loq, int hii, int hiq)
{
	static const int pll_corr[2] = { -0xa00, 0xa00 };
	unsigned char curbit;
	unsigned int descx;
	int val;

	/* 
	 * estimate power
	 */
	val = est_pwr(hii, hiq) - est_pwr(loi, loq);
	/*
	 * estimate center value
	 */
	st->dem_sum[0] += val >> 8;
	if ((++st->dem_cnt) >= 256) {
		st->dem_cnt = 0;
		st->dem_sum_mean = (st->dem_sum[0]+st->dem_sum[1]+
				    st->dem_sum[2]+st->dem_sum[3]+
				    st->dem_sum[4]+st->dem_sum[5]+
				    st->dem_sum[6]+st->dem_sum[7]) >> 3;
		memmove(st->dem_sum+1, st->dem_sum, 
			sizeof(st->dem_sum)-sizeof(st->dem_sum[0]));
		st->dem_sum[0] = 0;
	}
	/*
	 * decision and bit clock regen
	 */
	val -= st->dem_sum_mean;
	diag_add(sm, curval, val);
	
	st->dcd_shreg <<= 1;
	st->bit_pll += 0x1555;
	curbit = (val > 0);
	if (st->last_sample ^ curbit) {
		st->dcd_shreg |= 1;
		st->bit_pll += pll_corr[st->bit_pll < (0x8000+0x1555)];
		st->dcd_sum0 += 4*hweight8(st->dcd_shreg & 0x1e) -
			hweight16(st->dcd_shreg & 0xfe00);
	}
	st->last_sample = curbit;
	hdlcdrv_channelbit(&sm->hdrv, curbit);
	if ((--st->dcd_time) <= 0) {
		hdlcdrv_setdcd(&sm->hdrv, (st->dcd_sum0 + st->dcd_sum1 + 
					   st->dcd_sum2) < 0);
		st->dcd_sum2 = st->dcd_sum1;
		st->dcd_sum1 = st->dcd_sum0;
		st->dcd_sum0 = 2; /* slight bias */
		st->dcd_time = 400;
	}
	if (st->bit_pll >= 0x10000) {
		st->bit_pll &= 0xffffu;
		st->descram = (st->descram << 1) | curbit;
		descx = st->descram ^ (st->descram >> 1);
		descx ^= ((descx >> DESCRAM_TAPSH1) ^
			  (descx >> DESCRAM_TAPSH2));
		st->shreg >>= 1;
		st->shreg |= (!(descx & 1)) << 16;
		if (st->shreg & 1) {
			hdlcdrv_putbits(&sm->hdrv, st->shreg >> 1);
			st->shreg = 0x10000;
		}
		diag_trigger(sm);
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_2666_u8(struct sm_state *sm, const unsigned char *buf, unsigned int buflen)
{
	struct demod_state_afsk26 *st = (struct demod_state_afsk26 *)(&sm->d);

	for (; buflen > 0; buflen--, buf++) {
		demod_one_sample(sm, st, (*buf-0x80)<<8,
				 convolution12_u8(buf, afsk26_dem_tables[0][0].i, AFSK26_DEM_SUM_I_0_0),
				 convolution12_u8(buf, afsk26_dem_tables[0][0].q, AFSK26_DEM_SUM_Q_0_0),
				 convolution12_u8(buf, afsk26_dem_tables[0][1].i, AFSK26_DEM_SUM_I_0_1),
				 convolution12_u8(buf, afsk26_dem_tables[0][1].q, AFSK26_DEM_SUM_Q_0_1));
		demod_one_sample(sm, st,  (*buf-0x80)<<8,
				 convolution12_u8(buf, afsk26_dem_tables[1][0].i, AFSK26_DEM_SUM_I_1_0),
				 convolution12_u8(buf, afsk26_dem_tables[1][0].q, AFSK26_DEM_SUM_Q_1_0),
				 convolution12_u8(buf, afsk26_dem_tables[1][1].i, AFSK26_DEM_SUM_I_1_1),
				 convolution12_u8(buf, afsk26_dem_tables[1][1].q, AFSK26_DEM_SUM_Q_1_1));
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_2666_s16(struct sm_state *sm, const short *buf, unsigned int buflen)
{
	struct demod_state_afsk26 *st = (struct demod_state_afsk26 *)(&sm->d);

	for (; buflen > 0; buflen--, buf++) {
		demod_one_sample(sm, st, *buf,
				 convolution12_s16(buf, afsk26_dem_tables[0][0].i, AFSK26_DEM_SUM_I_0_0),
				 convolution12_s16(buf, afsk26_dem_tables[0][0].q, AFSK26_DEM_SUM_Q_0_0),
				 convolution12_s16(buf, afsk26_dem_tables[0][1].i, AFSK26_DEM_SUM_I_0_1),
				 convolution12_s16(buf, afsk26_dem_tables[0][1].q, AFSK26_DEM_SUM_Q_0_1));
		demod_one_sample(sm, st, *buf, 
				 convolution12_s16(buf, afsk26_dem_tables[1][0].i, AFSK26_DEM_SUM_I_1_0),
				 convolution12_s16(buf, afsk26_dem_tables[1][0].q, AFSK26_DEM_SUM_Q_1_0),
				 convolution12_s16(buf, afsk26_dem_tables[1][1].i, AFSK26_DEM_SUM_I_1_1),
				 convolution12_s16(buf, afsk26_dem_tables[1][1].q, AFSK26_DEM_SUM_Q_1_1));
	}
}

/* --------------------------------------------------------------------- */

static void demod_init_2666(struct sm_state *sm)
{
	struct demod_state_afsk26 *st = (struct demod_state_afsk26 *)(&sm->d);

	st->dcd_time = 400;
	st->dcd_sum0 = 2;	
}

/* --------------------------------------------------------------------- */

const struct modem_tx_info sm_afsk2666_tx = {
	"afsk2666", sizeof(struct mod_state_afsk26), AFSK26_SAMPLERATE, 2666, 
	modulator_2666_u8, modulator_2666_s16, NULL
};

const struct modem_rx_info sm_afsk2666_rx = {
	"afsk2666", sizeof(struct demod_state_afsk26), AFSK26_SAMPLERATE, 2666, 12, 6, 
	demodulator_2666_u8, demodulator_2666_s16, demod_init_2666
};

/* --------------------------------------------------------------------- */
