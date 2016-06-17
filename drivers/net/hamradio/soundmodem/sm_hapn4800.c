/*****************************************************************************/

/*
 *	sm_hapn4800.c  -- soundcard radio modem driver, 4800 baud HAPN modem
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
 *
 *  This module implements a (hopefully) HAPN (Hamilton Area Packet
 *  Network) compatible 4800 baud modem.
 *  The HAPN modem uses kind of "duobinary signalling" (not really,
 *  duobinary signalling gives ... 0 0 -1 0 1 0 0 ... at the sampling
 *  instants, whereas HAPN signalling gives ... 0 0 -1 1 0 0 ..., see
 *  Proakis, Digital Communications).
 *  The code is untested. It is compatible with itself (i.e. it can decode
 *  the packets it sent), but I could not test if it is compatible with
 *  any "real" HAPN modem, since noone uses it in my region of the world.
 *  Feedback therefore welcome.
 */

#include "sm.h"
#include "sm_tbl_hapn4800.h"

/* --------------------------------------------------------------------- */

struct demod_state_hapn48 {
	unsigned int shreg;
	unsigned int bit_pll;
	unsigned char last_bit;
	unsigned char last_bit2;
	unsigned int dcd_shreg;
	int dcd_sum0, dcd_sum1, dcd_sum2;
	unsigned int dcd_time;
	int lvlhi, lvllo;
};

struct mod_state_hapn48 {
	unsigned int shreg;
	unsigned char tx_bit;
	unsigned int tx_seq;
	const unsigned char *tbl;
};

/* --------------------------------------------------------------------- */

static void modulator_hapn4800_10_u8(struct sm_state *sm, unsigned char *buf, unsigned int buflen)
{
	struct mod_state_hapn48 *st = (struct mod_state_hapn48 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) { 
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = ((st->tx_bit << 1) |
				      (st->tx_bit & 1));
			st->tx_bit ^= (!(st->shreg & 1));
			st->shreg >>= 1;
			st->tbl = hapn48_txfilt_10 + (st->tx_bit & 0xf);
		}
		if (st->tx_seq >= 10)
			st->tx_seq = 0;
		*buf = *st->tbl;
		st->tbl += 0x10;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_hapn4800_10_s16(struct sm_state *sm, short *buf, unsigned int buflen)
{
	struct mod_state_hapn48 *st = (struct mod_state_hapn48 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) { 
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = ((st->tx_bit << 1) |
				      (st->tx_bit & 1));
			st->tx_bit ^= (!(st->shreg & 1));
			st->shreg >>= 1;
			st->tbl = hapn48_txfilt_10 + (st->tx_bit & 0xf);
		}
		if (st->tx_seq >= 10)
			st->tx_seq = 0;
		*buf = ((*st->tbl)-0x80)<<8;
		st->tbl += 0x10;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_hapn4800_8_u8(struct sm_state *sm, unsigned char *buf, unsigned int buflen)
{
	struct mod_state_hapn48 *st = (struct mod_state_hapn48 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = (st->tx_bit << 1) | (st->tx_bit & 1);
			st->tx_bit ^= !(st->shreg & 1);
			st->shreg >>= 1;
			st->tbl = hapn48_txfilt_8 + (st->tx_bit & 0xf);
		}
		if (st->tx_seq >= 8)
			st->tx_seq = 0;
		*buf = *st->tbl;
		st->tbl += 0x10;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_hapn4800_8_s16(struct sm_state *sm, short *buf, unsigned int buflen)
{
	struct mod_state_hapn48 *st = (struct mod_state_hapn48 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = (st->tx_bit << 1) | (st->tx_bit & 1);
			st->tx_bit ^= !(st->shreg & 1);
			st->shreg >>= 1;
			st->tbl = hapn48_txfilt_8 + (st->tx_bit & 0xf);
		}
		if (st->tx_seq >= 8)
			st->tx_seq = 0;
		*buf = ((*st->tbl)-0x80)<<8;
		st->tbl += 0x10;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_hapn4800_pm10_u8(struct sm_state *sm, unsigned char *buf, unsigned int buflen)
{
	struct mod_state_hapn48 *st = (struct mod_state_hapn48 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) { 
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = ((st->tx_bit << 1) |
				      (st->tx_bit & 1));
			st->tx_bit ^= (!(st->shreg & 1));
			st->shreg >>= 1;
			st->tbl = hapn48_txfilt_pm10 + (st->tx_bit & 0xf);
		}
		if (st->tx_seq >= 10)
			st->tx_seq = 0;
		*buf = *st->tbl;
		st->tbl += 0x10;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_hapn4800_pm10_s16(struct sm_state *sm, short *buf, unsigned int buflen)
{
	struct mod_state_hapn48 *st = (struct mod_state_hapn48 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) { 
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = ((st->tx_bit << 1) |
				      (st->tx_bit & 1));
			st->tx_bit ^= (!(st->shreg & 1));
			st->shreg >>= 1;
			st->tbl = hapn48_txfilt_pm10 + (st->tx_bit & 0xf);
		}
		if (st->tx_seq >= 10)
			st->tx_seq = 0;
		*buf = ((*st->tbl)-0x80)<<8;
		st->tbl += 0x10;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_hapn4800_pm8_u8(struct sm_state *sm, unsigned char *buf, unsigned int buflen)
{
	struct mod_state_hapn48 *st = (struct mod_state_hapn48 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = (st->tx_bit << 1) | (st->tx_bit & 1);
			st->tx_bit ^= !(st->shreg & 1);
			st->shreg >>= 1;
			st->tbl = hapn48_txfilt_pm8 + (st->tx_bit & 0xf);
		}
		if (st->tx_seq >= 8)
			st->tx_seq = 0;
		*buf = *st->tbl;
		st->tbl += 0x10;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_hapn4800_pm8_s16(struct sm_state *sm, short *buf, unsigned int buflen)
{
	struct mod_state_hapn48 *st = (struct mod_state_hapn48 *)(&sm->m);

	for (; buflen > 0; buflen--, buf++) {
		if (!st->tx_seq++) {
			if (st->shreg <= 1)
				st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
			st->tx_bit = (st->tx_bit << 1) | (st->tx_bit & 1);
			st->tx_bit ^= !(st->shreg & 1);
			st->shreg >>= 1;
			st->tbl = hapn48_txfilt_pm8 + (st->tx_bit & 0xf);
		}
		if (st->tx_seq >= 8)
			st->tx_seq = 0;
		*buf = ((*st->tbl)-0x80)<<8;
		st->tbl += 0x10;
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_hapn4800_10_u8(struct sm_state *sm, const unsigned char *buf, unsigned int buflen)
{
	struct demod_state_hapn48 *st = (struct demod_state_hapn48 *)(&sm->d);
	static const int pll_corr[2] = { -0x800, 0x800 };
	int curst, cursync;
	int inv;

	for (; buflen > 0; buflen--, buf++) {
		inv = ((int)(buf[-2])-0x80) << 8;
		st->lvlhi = (st->lvlhi * 65309) >> 16; /* decay */
		st->lvllo = (st->lvllo * 65309) >> 16; /* decay */
		if (inv > st->lvlhi)
			st->lvlhi = inv;
		if (inv < st->lvllo)
			st->lvllo = inv;
		if (buflen & 1)
			st->dcd_shreg <<= 1;
		st->bit_pll += 0x199a;
		curst = cursync = 0;
		if (inv > st->lvlhi >> 1) {
			curst = 1;
			cursync = (buf[-2] > buf[-1] && buf[-2] > buf[-3] &&
				   buf[-2] > buf[-0] && buf[-2] > buf[-4]);
		} else if (inv < st->lvllo >> 1) {
			curst = -1;
			cursync = (buf[-2] < buf[-1] && buf[-2] < buf[-3] &&
				   buf[-2] < buf[-0] && buf[-2] < buf[-4]);
		}
		if (cursync) {
			st->dcd_shreg |= cursync;
			st->bit_pll += pll_corr[((st->bit_pll - 0x8000u) & 0xffffu) < 0x8ccdu];
			st->dcd_sum0 += 16 * hweight32(st->dcd_shreg & 0x18c6318c) - 
				hweight32(st->dcd_shreg & 0xe739ce70);
		}
		hdlcdrv_channelbit(&sm->hdrv, cursync);
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
			st->last_bit2 = st->last_bit;
			if (curst < 0)
				st->last_bit = 0;
			else if (curst > 0)
				st->last_bit = 1;
			st->shreg >>= 1;
			st->shreg |= ((st->last_bit ^ st->last_bit2 ^ 1) & 1) << 16;
			if (st->shreg & 1) {
				hdlcdrv_putbits(&sm->hdrv, st->shreg >> 1);
				st->shreg = 0x10000;
			}
			diag_trigger(sm);
		}
		diag_add_one(sm, inv);
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_hapn4800_10_s16(struct sm_state *sm, const short *buf, unsigned int buflen)
{
	struct demod_state_hapn48 *st = (struct demod_state_hapn48 *)(&sm->d);
	static const int pll_corr[2] = { -0x800, 0x800 };
	int curst, cursync;
	int inv;

	for (; buflen > 0; buflen--, buf++) {
		inv = buf[-2];
		st->lvlhi = (st->lvlhi * 65309) >> 16; /* decay */
		st->lvllo = (st->lvllo * 65309) >> 16; /* decay */
		if (inv > st->lvlhi)
			st->lvlhi = inv;
		if (inv < st->lvllo)
			st->lvllo = inv;
		if (buflen & 1)
			st->dcd_shreg <<= 1;
		st->bit_pll += 0x199a;
		curst = cursync = 0;
		if (inv > st->lvlhi >> 1) {
			curst = 1;
			cursync = (buf[-2] > buf[-1] && buf[-2] > buf[-3] &&
				   buf[-2] > buf[-0] && buf[-2] > buf[-4]);
		} else if (inv < st->lvllo >> 1) {
			curst = -1;
			cursync = (buf[-2] < buf[-1] && buf[-2] < buf[-3] &&
				   buf[-2] < buf[-0] && buf[-2] < buf[-4]);
		}
		if (cursync) {
			st->dcd_shreg |= cursync;
			st->bit_pll += pll_corr[((st->bit_pll - 0x8000u) & 0xffffu) < 0x8ccdu];
			st->dcd_sum0 += 16 * hweight32(st->dcd_shreg & 0x18c6318c) - 
				hweight32(st->dcd_shreg & 0xe739ce70);
		}
		hdlcdrv_channelbit(&sm->hdrv, cursync);
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
			st->last_bit2 = st->last_bit;
			if (curst < 0)
				st->last_bit = 0;
			else if (curst > 0)
				st->last_bit = 1;
			st->shreg >>= 1;
			st->shreg |= ((st->last_bit ^ st->last_bit2 ^ 1) & 1) << 16;
			if (st->shreg & 1) {
				hdlcdrv_putbits(&sm->hdrv, st->shreg >> 1);
				st->shreg = 0x10000;
			}
			diag_trigger(sm);
		}
		diag_add_one(sm, inv);
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_hapn4800_8_u8(struct sm_state *sm, const unsigned char *buf, unsigned int buflen)
{
	struct demod_state_hapn48 *st = (struct demod_state_hapn48 *)(&sm->d);
	static const int pll_corr[2] = { -0x800, 0x800 };
	int curst, cursync;
	int inv;

	for (; buflen > 0; buflen--, buf++) {
		inv = ((int)(buf[-2])-0x80) << 8;
		st->lvlhi = (st->lvlhi * 65309) >> 16; /* decay */
		st->lvllo = (st->lvllo * 65309) >> 16; /* decay */
		if (inv > st->lvlhi)
			st->lvlhi = inv;
		if (inv < st->lvllo)
			st->lvllo = inv;
		if (buflen & 1)
			st->dcd_shreg <<= 1;
		st->bit_pll += 0x2000;
		curst = cursync = 0;
		if (inv > st->lvlhi >> 1) {
			curst = 1;
			cursync = (buf[-2] > buf[-1] && buf[-2] > buf[-3] &&
				   buf[-2] > buf[-0] && buf[-2] > buf[-4]);
		} else if (inv < st->lvllo >> 1) {
			curst = -1;
			cursync = (buf[-2] < buf[-1] && buf[-2] < buf[-3] &&
				   buf[-2] < buf[-0] && buf[-2] < buf[-4]);
		}
		if (cursync) {
			st->dcd_shreg |= cursync;
			st->bit_pll += pll_corr[((st->bit_pll - 0x8000u) & 0xffffu) < 0x9000u];
			st->dcd_sum0 += 16 * hweight32(st->dcd_shreg & 0x44444444) - 
				hweight32(st->dcd_shreg & 0xbbbbbbbb);
		}
		hdlcdrv_channelbit(&sm->hdrv, cursync);
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
			st->last_bit2 = st->last_bit;
			if (curst < 0)
				st->last_bit = 0;
			else if (curst > 0)
				st->last_bit = 1;
			st->shreg >>= 1;
			st->shreg |= ((st->last_bit ^ st->last_bit2 ^ 1) & 1) << 16;
			if (st->shreg & 1) {
				hdlcdrv_putbits(&sm->hdrv, st->shreg >> 1);
				st->shreg = 0x10000;
			}
			diag_trigger(sm);
		}
		diag_add_one(sm, inv);
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_hapn4800_8_s16(struct sm_state *sm, const short *buf, unsigned int buflen)
{
	struct demod_state_hapn48 *st = (struct demod_state_hapn48 *)(&sm->d);
	static const int pll_corr[2] = { -0x800, 0x800 };
	int curst, cursync;
	int inv;

	for (; buflen > 0; buflen--, buf++) {
		inv = buf[-2];
		st->lvlhi = (st->lvlhi * 65309) >> 16; /* decay */
		st->lvllo = (st->lvllo * 65309) >> 16; /* decay */
		if (inv > st->lvlhi)
			st->lvlhi = inv;
		if (inv < st->lvllo)
			st->lvllo = inv;
		if (buflen & 1)
			st->dcd_shreg <<= 1;
		st->bit_pll += 0x2000;
		curst = cursync = 0;
		if (inv > st->lvlhi >> 1) {
			curst = 1;
			cursync = (buf[-2] > buf[-1] && buf[-2] > buf[-3] &&
				   buf[-2] > buf[-0] && buf[-2] > buf[-4]);
		} else if (inv < st->lvllo >> 1) {
			curst = -1;
			cursync = (buf[-2] < buf[-1] && buf[-2] < buf[-3] &&
				   buf[-2] < buf[-0] && buf[-2] < buf[-4]);
		}
		if (cursync) {
			st->dcd_shreg |= cursync;
			st->bit_pll += pll_corr[((st->bit_pll - 0x8000u) & 0xffffu) < 0x9000u];
			st->dcd_sum0 += 16 * hweight32(st->dcd_shreg & 0x44444444) - 
				hweight32(st->dcd_shreg & 0xbbbbbbbb);
		}
		hdlcdrv_channelbit(&sm->hdrv, cursync);
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
			st->last_bit2 = st->last_bit;
			if (curst < 0)
				st->last_bit = 0;
			else if (curst > 0)
				st->last_bit = 1;
			st->shreg >>= 1;
			st->shreg |= ((st->last_bit ^ st->last_bit2 ^ 1) & 1) << 16;
			if (st->shreg & 1) {
				hdlcdrv_putbits(&sm->hdrv, st->shreg >> 1);
				st->shreg = 0x10000;
			}
			diag_trigger(sm);
		}
		diag_add_one(sm, inv);
	}
}

/* --------------------------------------------------------------------- */

static void demod_init_hapn4800(struct sm_state *sm)
{
	struct demod_state_hapn48 *st = (struct demod_state_hapn48 *)(&sm->d);

	st->dcd_time = 120;
	st->dcd_sum0 = 2;	
}

/* --------------------------------------------------------------------- */

const struct modem_tx_info sm_hapn4800_8_tx = {
	"hapn4800", sizeof(struct mod_state_hapn48), 38400, 4800, 
	modulator_hapn4800_8_u8, modulator_hapn4800_8_s16, NULL
};

const struct modem_rx_info sm_hapn4800_8_rx = {
	"hapn4800", sizeof(struct demod_state_hapn48), 38400, 4800, 5, 8, 
	demodulator_hapn4800_8_u8, demodulator_hapn4800_8_s16, demod_init_hapn4800
};

/* --------------------------------------------------------------------- */

const struct modem_tx_info sm_hapn4800_10_tx = {
	"hapn4800", sizeof(struct mod_state_hapn48), 48000, 4800,
	modulator_hapn4800_10_u8, modulator_hapn4800_10_s16, NULL
};

const struct modem_rx_info sm_hapn4800_10_rx = {
	"hapn4800", sizeof(struct demod_state_hapn48), 48000, 4800, 5, 10, 
	demodulator_hapn4800_10_u8, demodulator_hapn4800_10_s16, demod_init_hapn4800
};

/* --------------------------------------------------------------------- */

const struct modem_tx_info sm_hapn4800_pm8_tx = {
	"hapn4800pm", sizeof(struct mod_state_hapn48), 38400, 4800, 
	modulator_hapn4800_pm8_u8, modulator_hapn4800_pm8_s16, NULL
};

const struct modem_rx_info sm_hapn4800_pm8_rx = {
	"hapn4800pm", sizeof(struct demod_state_hapn48), 38400, 4800, 5, 8, 
	demodulator_hapn4800_8_u8, demodulator_hapn4800_8_s16, demod_init_hapn4800
};

/* --------------------------------------------------------------------- */

const struct modem_tx_info sm_hapn4800_pm10_tx = {
	"hapn4800pm", sizeof(struct mod_state_hapn48), 48000, 4800,
	modulator_hapn4800_pm10_u8, modulator_hapn4800_pm10_s16, NULL
};

const struct modem_rx_info sm_hapn4800_pm10_rx = {
	"hapn4800pm", sizeof(struct demod_state_hapn48), 48000, 4800, 5, 10,
	demodulator_hapn4800_10_u8, demodulator_hapn4800_10_s16, demod_init_hapn4800
};

/* --------------------------------------------------------------------- */
