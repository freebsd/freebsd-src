/*****************************************************************************/

/*
 *	sm_fsk9600.c  --  soundcard radio modem driver, 
 *                        9600 baud G3RUH compatible FSK modem
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
#include "sm_tbl_fsk9600.h"

/* --------------------------------------------------------------------- */

struct demod_state_fsk96 {
	unsigned int shreg;
	unsigned long descram;
	unsigned int bit_pll;
	unsigned char last_sample;
	unsigned int dcd_shreg;
	int dcd_sum0, dcd_sum1, dcd_sum2;
	unsigned int dcd_time;
};

struct mod_state_fsk96 {
	unsigned int shreg;
	unsigned long scram;
	unsigned char tx_bit;
	unsigned char *txtbl;
	unsigned int txphase;
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

static void modulator_9600_4_u8(struct sm_state *sm, unsigned char *buf, unsigned int buflen)
{
	struct mod_state_fsk96 *st = (struct mod_state_fsk96 *)(&sm->m);

	for (; buflen > 0; buflen--) {
		if (!st->txphase++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->scram = (st->scram << 1) | (st->scram & 1);
			st->scram ^= !(st->shreg & 1);
			st->shreg >>= 1;
			if (st->scram & (SCRAM_TAP1 << 1))
				st->scram ^= SCRAM_TAPN << 1;
			st->tx_bit = (st->tx_bit << 1) | (!!(st->scram & (SCRAM_TAP1 << 2)));
			st->txtbl = fsk96_txfilt_4 + (st->tx_bit & 0xff);
		}
		if (st->txphase >= 4)
			st->txphase = 0;
		*buf++ = *st->txtbl;
		st->txtbl += 0x100;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_9600_4_s16(struct sm_state *sm, short *buf, unsigned int buflen)
{
	struct mod_state_fsk96 *st = (struct mod_state_fsk96 *)(&sm->m);

	for (; buflen > 0; buflen--) {
		if (!st->txphase++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->scram = (st->scram << 1) | (st->scram & 1);
			st->scram ^= !(st->shreg & 1);
			st->shreg >>= 1;
			if (st->scram & (SCRAM_TAP1 << 1))
				st->scram ^= SCRAM_TAPN << 1;
			st->tx_bit = (st->tx_bit << 1) | (!!(st->scram & (SCRAM_TAP1 << 2)));
			st->txtbl = fsk96_txfilt_4 + (st->tx_bit & 0xff);
		}
		if (st->txphase >= 4)
			st->txphase = 0;
		*buf++ = ((*st->txtbl)-0x80) << 8;
		st->txtbl += 0x100;
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_9600_4_u8(struct sm_state *sm, const unsigned char *buf, unsigned int buflen)
{
	struct demod_state_fsk96 *st = (struct demod_state_fsk96 *)(&sm->d);
	static const int pll_corr[2] = { -0x1000, 0x1000 };
	unsigned char curbit;
	unsigned int descx;

	for (; buflen > 0; buflen--, buf++) {
		st->dcd_shreg <<= 1;
		st->bit_pll += 0x4000;
		curbit = (*buf >= 0x80);
		if (st->last_sample ^ curbit) {
			st->dcd_shreg |= 1;
			st->bit_pll += pll_corr[st->bit_pll < 0xa000];
			st->dcd_sum0 += 8 * hweight8(st->dcd_shreg & 0x0c) - 
				!!(st->dcd_shreg & 0x10);
		}
		st->last_sample = curbit;
		hdlcdrv_channelbit(&sm->hdrv, st->last_sample);
		if ((--st->dcd_time) <= 0) {
			hdlcdrv_setdcd(&sm->hdrv, (st->dcd_sum0 + 
						   st->dcd_sum1 + 
						   st->dcd_sum2) < 0);
			st->dcd_sum2 = st->dcd_sum1;
			st->dcd_sum1 = st->dcd_sum0;
			st->dcd_sum0 = 2; /* slight bias */
			st->dcd_time = 240;
		}
		if (st->bit_pll >= 0x10000) {
			st->bit_pll &= 0xffff;
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
		diag_add_one(sm, ((short)(*buf - 0x80)) << 8);
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_9600_4_s16(struct sm_state *sm, const short *buf, unsigned int buflen)
{
	struct demod_state_fsk96 *st = (struct demod_state_fsk96 *)(&sm->d);
	static const int pll_corr[2] = { -0x1000, 0x1000 };
	unsigned char curbit;
	unsigned int descx;

	for (; buflen > 0; buflen--, buf++) {
		st->dcd_shreg <<= 1;
		st->bit_pll += 0x4000;
		curbit = (*buf >= 0);
		if (st->last_sample ^ curbit) {
			st->dcd_shreg |= 1;
			st->bit_pll += pll_corr[st->bit_pll < 0xa000];
			st->dcd_sum0 += 8 * hweight8(st->dcd_shreg & 0x0c) - 
				!!(st->dcd_shreg & 0x10);
		}
		st->last_sample = curbit;
		hdlcdrv_channelbit(&sm->hdrv, st->last_sample);
		if ((--st->dcd_time) <= 0) {
			hdlcdrv_setdcd(&sm->hdrv, (st->dcd_sum0 + 
						   st->dcd_sum1 + 
						   st->dcd_sum2) < 0);
			st->dcd_sum2 = st->dcd_sum1;
			st->dcd_sum1 = st->dcd_sum0;
			st->dcd_sum0 = 2; /* slight bias */
			st->dcd_time = 240;
		}
		if (st->bit_pll >= 0x10000) {
			st->bit_pll &= 0xffff;
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
		diag_add_one(sm, *buf);
	}
}

/* --------------------------------------------------------------------- */

static void modulator_9600_5_u8(struct sm_state *sm, unsigned char *buf, unsigned int buflen)
{
	struct mod_state_fsk96 *st = (struct mod_state_fsk96 *)(&sm->m);

	for (; buflen > 0; buflen--) {
		if (!st->txphase++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->scram = (st->scram << 1) | (st->scram & 1);
			st->scram ^= !(st->shreg & 1);
			st->shreg >>= 1;
			if (st->scram & (SCRAM_TAP1 << 1))
				st->scram ^= SCRAM_TAPN << 1;
			st->tx_bit = (st->tx_bit << 1) | (!!(st->scram & (SCRAM_TAP1 << 2)));
			st->txtbl = fsk96_txfilt_5 + (st->tx_bit & 0xff);
		}
		if (st->txphase >= 5)
			st->txphase = 0;
		*buf++ = *st->txtbl;
		st->txtbl += 0x100;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_9600_5_s16(struct sm_state *sm, short *buf, unsigned int buflen)
{
	struct mod_state_fsk96 *st = (struct mod_state_fsk96 *)(&sm->m);

	for (; buflen > 0; buflen--) {
		if (!st->txphase++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->scram = (st->scram << 1) | (st->scram & 1);
			st->scram ^= !(st->shreg & 1);
			st->shreg >>= 1;
			if (st->scram & (SCRAM_TAP1 << 1))
				st->scram ^= SCRAM_TAPN << 1;
			st->tx_bit = (st->tx_bit << 1) | (!!(st->scram & (SCRAM_TAP1 << 2)));
			st->txtbl = fsk96_txfilt_5 + (st->tx_bit & 0xff);
		}
		if (st->txphase >= 5)
			st->txphase = 0;
		*buf++ = ((*st->txtbl)-0x80)<<8;
		st->txtbl += 0x100;
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_9600_5_u8(struct sm_state *sm, const unsigned char *buf, unsigned int buflen)
{
	struct demod_state_fsk96 *st = (struct demod_state_fsk96 *)(&sm->d);
	static const int pll_corr[2] = { -0x1000, 0x1000 };
	unsigned char curbit;
	unsigned int descx;

	for (; buflen > 0; buflen--, buf++) {
		st->dcd_shreg <<= 1;
		st->bit_pll += 0x3333;
		curbit = (*buf >= 0x80);
		if (st->last_sample ^ curbit) {
			st->dcd_shreg |= 1;
			st->bit_pll += pll_corr[st->bit_pll < 0x9999];
			st->dcd_sum0 += 16 * hweight8(st->dcd_shreg & 0x0c) - 
				hweight8(st->dcd_shreg & 0x70);
		}
		st->last_sample = curbit;
		hdlcdrv_channelbit(&sm->hdrv, st->last_sample);
		if ((--st->dcd_time) <= 0) {
			hdlcdrv_setdcd(&sm->hdrv, (st->dcd_sum0 + 
						   st->dcd_sum1 + 
						   st->dcd_sum2) < 0);
			st->dcd_sum2 = st->dcd_sum1;
			st->dcd_sum1 = st->dcd_sum0;
			st->dcd_sum0 = 2; /* slight bias */
			st->dcd_time = 240;
		}
		if (st->bit_pll >= 0x10000) {
			st->bit_pll &= 0xffff;
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
		diag_add_one(sm, ((short)(*buf - 0x80)) << 8);
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_9600_5_s16(struct sm_state *sm, const short *buf, unsigned int buflen)
{
	struct demod_state_fsk96 *st = (struct demod_state_fsk96 *)(&sm->d);
	static const int pll_corr[2] = { -0x1000, 0x1000 };
	unsigned char curbit;
	unsigned int descx;

	for (; buflen > 0; buflen--, buf++) {
		st->dcd_shreg <<= 1;
		st->bit_pll += 0x3333;
		curbit = (*buf >= 0);
		if (st->last_sample ^ curbit) {
			st->dcd_shreg |= 1;
			st->bit_pll += pll_corr[st->bit_pll < 0x9999];
			st->dcd_sum0 += 16 * hweight8(st->dcd_shreg & 0x0c) - 
				hweight8(st->dcd_shreg & 0x70);
		}
		st->last_sample = curbit;
		hdlcdrv_channelbit(&sm->hdrv, st->last_sample);
		if ((--st->dcd_time) <= 0) {
			hdlcdrv_setdcd(&sm->hdrv, (st->dcd_sum0 + 
						   st->dcd_sum1 + 
						   st->dcd_sum2) < 0);
			st->dcd_sum2 = st->dcd_sum1;
			st->dcd_sum1 = st->dcd_sum0;
			st->dcd_sum0 = 2; /* slight bias */
			st->dcd_time = 240;
		}
		if (st->bit_pll >= 0x10000) {
			st->bit_pll &= 0xffff;
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
		diag_add_one(sm, *buf);
	}
}

/* --------------------------------------------------------------------- */

static void demod_init_9600(struct sm_state *sm)
{
	struct demod_state_fsk96 *st = (struct demod_state_fsk96 *)(&sm->d);

	st->dcd_time = 240;
	st->dcd_sum0 = 2;	
}

/* --------------------------------------------------------------------- */

const struct modem_tx_info sm_fsk9600_4_tx = {
	"fsk9600", sizeof(struct mod_state_fsk96), 38400, 9600,
	modulator_9600_4_u8, modulator_9600_4_s16, NULL
};

const struct modem_rx_info sm_fsk9600_4_rx = {
	"fsk9600", sizeof(struct demod_state_fsk96), 38400, 9600, 1, 4,
	demodulator_9600_4_u8, demodulator_9600_4_s16, demod_init_9600
};

/* --------------------------------------------------------------------- */

const struct modem_tx_info sm_fsk9600_5_tx = {
	"fsk9600", sizeof(struct mod_state_fsk96), 48000, 9600,
	modulator_9600_5_u8, modulator_9600_5_s16, NULL
};

const struct modem_rx_info sm_fsk9600_5_rx = {
	"fsk9600", sizeof(struct demod_state_fsk96), 48000, 9600, 1, 5, 
	demodulator_9600_5_u8, demodulator_9600_5_s16, demod_init_9600
};

/* --------------------------------------------------------------------- */
