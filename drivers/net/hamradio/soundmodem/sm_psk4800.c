/*****************************************************************************/

/*
 *	sm_psk4800.c  -- soundcard radio modem driver, 4800 baud 8PSK modem
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
#include "sm_tbl_psk4800.h"

/* --------------------------------------------------------------------- */

#define DESCRAM_TAP1 0x20000
#define DESCRAM_TAP2 0x01000
#define DESCRAM_TAP3 0x00001

#define DESCRAM_TAPSH1 17
#define DESCRAM_TAPSH2 12
#define DESCRAM_TAPSH3 0

#define SCRAM_TAP1 0x20000 /* X^17 */
#define SCRAM_TAPN 0x00021 /* X^0+X^5 */

#define SCRAM_SHIFT 17

/* --------------------------------------------------------------------- */

struct demod_state_psk48 {
	/*
	 * input mixer and lowpass
	 */
	short infi[PSK48_RXF_LEN/2], infq[PSK48_RXF_LEN/2];
	unsigned int downmixer;
	int ovrphase;
	short magi, magq;
	/*
	 * sampling instant recovery
	 */
	int pwrhist[5];
	unsigned int s_phase;
	int cur_sync;
	/*
	 * phase recovery
	 */
	short cur_phase_dev;
	short last_ph_err;
	unsigned short pskph;
	unsigned int phase;
	unsigned short last_pskph;
	unsigned char cur_raw, last_raw, rawbits;
	/*
	 * decoding
	 */
	unsigned int shreg;
	unsigned long descram;
	unsigned int bit_pll;
	unsigned char last_sample;
	unsigned int dcd_shreg;
	int dcd_sum0, dcd_sum1, dcd_sum2;
	unsigned int dcd_time;
};

struct mod_state_psk48 {
	unsigned char txbits[PSK48_TXF_NUMSAMPLES];
	unsigned short txphase;
	unsigned int shreg;
	unsigned long scram;
	const short *tbl;
	unsigned int txseq;
};

/* --------------------------------------------------------------------- */

static void modulator_4800_u8(struct sm_state *sm, unsigned char *buf, unsigned int buflen)
{
	struct mod_state_psk48 *st = (struct mod_state_psk48 *)(&sm->m);
	int i, j;
	int si, sq;

	for (; buflen > 0; buflen--, buf++) {
		if (!st->txseq++) {
			memmove(st->txbits+1, st->txbits, 
				sizeof(st->txbits)-sizeof(st->txbits[0]));
			for (i = 0; i < 3; i++) {
				if (st->shreg <= 1)
					st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
				st->scram = (st->scram << 1) |
					(st->shreg & 1);
				st->shreg >>= 1;
				if (st->scram & SCRAM_TAP1)
					st->scram ^= SCRAM_TAPN;
			}
			j = (st->scram >> (SCRAM_SHIFT+3)) & 7;
			st->txbits[0] -= (j ^ (j >> 1));
			st->txbits[0] &= 7;
			st->tbl = psk48_tx_table;
		}
		if (st->txseq >= PSK48_TXF_OVERSAMPLING)
			st->txseq = 0;
		for (j = si = sq = 0; j < PSK48_TXF_NUMSAMPLES; j++, st->tbl += 16) {
			si += st->tbl[st->txbits[j]];
			sq += st->tbl[st->txbits[j]+8];
		}
		*buf = ((si*COS(st->txphase)+ sq*SIN(st->txphase)) >> 23) + 0x80;
		st->txphase = (st->txphase + PSK48_PHASEINC) & 0xffffu;
	}
}

/* --------------------------------------------------------------------- */

static void modulator_4800_s16(struct sm_state *sm, short *buf, unsigned int buflen)
{
	struct mod_state_psk48 *st = (struct mod_state_psk48 *)(&sm->m);
	int i, j;
	int si, sq;

	for (; buflen > 0; buflen--, buf++) {
		if (!st->txseq++) {
			memmove(st->txbits+1, st->txbits, 
				sizeof(st->txbits)-sizeof(st->txbits[0]));
			for (i = 0; i < 3; i++) {
				if (st->shreg <= 1)
					st->shreg = hdlcdrv_getbits(&sm->hdrv) | 0x10000;
				st->scram = (st->scram << 1) |
					(st->shreg & 1);
				st->shreg >>= 1;
				if (st->scram & SCRAM_TAP1)
					st->scram ^= SCRAM_TAPN;
			}
			j = (st->scram >> (SCRAM_SHIFT+3)) & 7;
			st->txbits[0] -= (j ^ (j >> 1));
			st->txbits[0] &= 7;
			st->tbl = psk48_tx_table;
		}
		if (st->txseq >= PSK48_TXF_OVERSAMPLING)
			st->txseq = 0;
		for (j = si = sq = 0; j < PSK48_TXF_NUMSAMPLES; j++, st->tbl += 16) {
			si += st->tbl[st->txbits[j]];
			sq += st->tbl[st->txbits[j]+8];
		}
		*buf = (si*COS(st->txphase)+ sq*SIN(st->txphase)) >> 15;
		st->txphase = (st->txphase + PSK48_PHASEINC) & 0xffffu;
	}
}

/* --------------------------------------------------------------------- */

static __inline__ unsigned short tbl_atan(short q, short i)
{
	short tmp;
	unsigned short argoffs = 0;

	if (i == 0 && q == 0)
		return 0;
	switch (((q < 0) << 1) | (i < 0)) {
	case 0:
		break;
	case 1:
		tmp = q;
		q = -i;
		i = tmp;
		argoffs = 0x4000;
		break;
	case 3:
		q = -q;
		i = -i;
		argoffs = 0x8000;
		break;
	case 2:
		tmp = -q;
		q = i;
		i = tmp;
		argoffs = 0xc000;
		break;
	}
	if (q > i) {
		tmp = i / q * ATAN_TABLEN;
		return (argoffs+0x4000-atan_tab[((i<<15)/q*ATAN_TABLEN>>15)])
			&0xffffu;
	}
	return (argoffs+atan_tab[((q<<15)/i*ATAN_TABLEN)>>15])&0xffffu;
}

#define ATAN(q,i) tbl_atan(q, i)

/* --------------------------------------------------------------------- */

static void demod_psk48_baseband(struct sm_state *sm, struct demod_state_psk48 *st,
				 short vali, short valq)
{
	int i, j;

	st->magi = vali;
	st->magq = valq;
	memmove(st->pwrhist+1, st->pwrhist, 
		sizeof(st->pwrhist)-sizeof(st->pwrhist[0]));
	st->pwrhist[0] = st->magi * st->magi +
		st->magq * st->magq;
	st->cur_sync = ((st->pwrhist[4] >> 2) > st->pwrhist[2] && 
			(st->pwrhist[0] >> 2) > st->pwrhist[2] &&
			st-> pwrhist[3] > st->pwrhist[2] && 
			st->pwrhist[1] > st->pwrhist[2]);
	st->s_phase &= 0xffff;
	st->s_phase += PSK48_SPHASEINC;
	st->dcd_shreg <<= 1;
	if (st->cur_sync) {
		if (st->s_phase >= (0x8000 + 5*PSK48_SPHASEINC/2))
			st->s_phase -= PSK48_SPHASEINC/6;
		else
			st->s_phase += PSK48_SPHASEINC/6;
		st->dcd_sum0 = 4*hweight8(st->dcd_shreg & 0xf8)-
			hweight16(st->dcd_shreg & 0x1f00);
	}
	if ((--st->dcd_time) <= 0) {
		hdlcdrv_setdcd(&sm->hdrv, (st->dcd_sum0 + st->dcd_sum1 + 
					   st->dcd_sum2) < 0);
		st->dcd_sum2 = st->dcd_sum1;
		st->dcd_sum1 = st->dcd_sum0;
		st->dcd_sum0 = 2; /* slight bias */
		st->dcd_time = 240;
	}
	if (st->s_phase < 0x10000)
		return;
	/*
	 * sample one constellation
	 */
	st->last_pskph = st->pskph;
	st->pskph = (ATAN(st->magq, st->magi)-
			       st->phase) & 0xffffu;
	st->last_ph_err = (st->pskph & 0x1fffu) - 0x1000;
	st->phase += st->last_ph_err/16;
	st->last_raw = st->cur_raw;
	st->cur_raw = ((st->pskph >> 13) & 7);
	i = (st->cur_raw - st->last_raw) & 7;
	st->rawbits = i ^ (i >> 1) ^ (i >> 2);
	st->descram = (st->descram << 3) | (st->rawbits);
	hdlcdrv_channelbit(&sm->hdrv, st->descram & 4);
	hdlcdrv_channelbit(&sm->hdrv, st->descram & 2);
	hdlcdrv_channelbit(&sm->hdrv, st->descram & 1);
	i = (((st->descram >> DESCRAM_TAPSH1) & 7) ^
	     ((st->descram >> DESCRAM_TAPSH2) & 7) ^
	     ((st->descram >> DESCRAM_TAPSH3) & 7));
 	for (j = 4; j; j >>= 1) {
		st->shreg >>= 1;
		st->shreg |= (!!(i & j)) << 16;
		if (st->shreg & 1) {
			hdlcdrv_putbits(&sm->hdrv, st->shreg >> 1);
			st->shreg = 0x10000;
		}
	}

#if 0
	st->dcd_shreg <<= 1;
	st->bit_pll += 0x4000;
	curbit = (*buf >= 0x80);
	if (st->last_sample ^ curbit) {
		st->dcd_shreg |= 1;
		st->bit_pll += pll_corr
			[st->bit_pll < 0xa000];
		st->dcd_sum0 += 8 * 
			hweight8(st->dcd_shreg & 0x0c) - 
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
	diag_add_one(sm, ((short)(*buf - 0x80)) << 8);
#endif
	
	diag_trigger(sm);
	diag_add_constellation(sm, (vali*COS(st->phase)+ valq*SIN(st->phase)) >> 13, 
			       (valq*COS(st->phase) - vali*SIN(st->phase)) >> 13);
}

/* --------------------------------------------------------------------- */

static void demodulator_4800_u8(struct sm_state *sm, const unsigned char *buf, unsigned int buflen)
{
	struct demod_state_psk48 *st = (struct demod_state_psk48 *)(&sm->d);
	int i, si, sq;
	const short *coeff;

	for (; buflen > 0; buflen--, buf++) {
		memmove(st->infi+1, st->infi, 
			sizeof(st->infi)-sizeof(st->infi[0]));
		memmove(st->infq+1, st->infq, 
			sizeof(st->infq)-sizeof(st->infq[0]));
		si = *buf;
		si &= 0xff;
		si -= 128;
		diag_add_one(sm, si << 8);
		st->infi[0] = (si * COS(st->downmixer))>>7;
		st->infq[0] = (si * SIN(st->downmixer))>>7;
		st->downmixer = (st->downmixer-PSK48_PHASEINC)&0xffffu;
		for (i = si = sq = 0, coeff = psk48_rx_coeff; i < (PSK48_RXF_LEN/2); 
		     i++, coeff += 2) {
			si += st->infi[i] * (*coeff);
			sq += st->infq[i] * (*coeff);
		}
		demod_psk48_baseband(sm, st, si >> 15, sq >> 15);
		for (i = si = sq = 0, coeff = psk48_rx_coeff + 1; i < (PSK48_RXF_LEN/2); 
		     i++, coeff += 2) {
			si += st->infi[i] * (*coeff);
			sq += st->infq[i] * (*coeff);
		}
		demod_psk48_baseband(sm, st, si >> 15, sq >> 15);
	}
}

/* --------------------------------------------------------------------- */

static void demodulator_4800_s16(struct sm_state *sm, const short *buf, unsigned int buflen)
{
	struct demod_state_psk48 *st = (struct demod_state_psk48 *)(&sm->d);
	int i, si, sq;
	const short *coeff;

	for (; buflen > 0; buflen--, buf++) {
		memmove(st->infi+1, st->infi, 
			sizeof(st->infi)-sizeof(st->infi[0]));
		memmove(st->infq+1, st->infq, 
			sizeof(st->infq)-sizeof(st->infq[0]));
		si = *buf;
		diag_add_one(sm, si);
		st->infi[0] = (si * COS(st->downmixer))>>15;
		st->infq[0] = (si * SIN(st->downmixer))>>15;
		st->downmixer = (st->downmixer-PSK48_PHASEINC)&0xffffu;
		for (i = si = sq = 0, coeff = psk48_rx_coeff; i < (PSK48_RXF_LEN/2); 
		     i++, coeff += 2) {
			si += st->infi[i] * (*coeff);
			sq += st->infq[i] * (*coeff);
		}
		demod_psk48_baseband(sm, st, si >> 15, sq >> 15);
		for (i = si = sq = 0, coeff = psk48_rx_coeff + 1; i < (PSK48_RXF_LEN/2); 
		     i++, coeff += 2) {
			si += st->infi[i] * (*coeff);
			sq += st->infq[i] * (*coeff);
		}
		demod_psk48_baseband(sm, st, si >> 15, sq >> 15);
	}
}

/* --------------------------------------------------------------------- */

static void mod_init_4800(struct sm_state *sm)
{
	struct mod_state_psk48 *st = (struct mod_state_psk48 *)(&sm->m);

	st->scram = 1;
}

/* --------------------------------------------------------------------- */

static void demod_init_4800(struct sm_state *sm)
{
	struct demod_state_psk48 *st = (struct demod_state_psk48 *)(&sm->d);

	st->dcd_time = 120;
	st->dcd_sum0 = 2;	
}

/* --------------------------------------------------------------------- */

const struct modem_tx_info sm_psk4800_tx = {
	"psk4800", sizeof(struct mod_state_psk48), 
	PSK48_SAMPLERATE, 4800,
	modulator_4800_u8, modulator_4800_s16, mod_init_4800
};

const struct modem_rx_info sm_psk4800_rx = {
	"psk4800", sizeof(struct demod_state_psk48), 
	PSK48_SAMPLERATE, 4800, 1, PSK48_TXF_OVERSAMPLING, 
	demodulator_4800_u8, demodulator_4800_s16, demod_init_4800
};

/* --------------------------------------------------------------------- */
