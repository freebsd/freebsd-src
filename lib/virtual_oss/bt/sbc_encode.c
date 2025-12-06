/*-
 * Copyright (c) 2015 Nathanial Sloss <nathanialsloss@yahoo.com.au>
 *
 *		This software is dedicated to the memory of -
 *	   Baron James Anlezark (Barry) - 1 Jan 1949 - 13 May 2012.
 *
 *		Barry was a man who loved his music.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <sys/uio.h>

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "sbc_coeffs.h"
#include "bt.h"

#define	SYNCWORD	0x9c
#define	ABS(x)		(((x) < 0) ? -(x) : (x))
#define	BIT30		(1U << 30)
#define	BM(x)		((1LL << (x)) - 1LL)

/* Loudness offset allocations. */
static const int loudnessoffset8[4][8] = {
	{-2, 0, 0, 0, 0, 0, 0, 1},
	{-3, 0, 0, 0, 0, 0, 1, 2},
	{-4, 0, 0, 0, 0, 0, 1, 2},
	{-4, 0, 0, 0, 0, 0, 1, 2},
};

static const int loudnessoffset4[4][4] = {
	{-1, 0, 0, 0},
	{-2, 0, 0, 1},
	{-2, 0, 0, 1},
	{-2, 0, 0, 1},
};

static uint8_t
calc_scalefactors_joint(struct sbc_encode *sbc)
{
	float sb_j[16][2];
	uint32_t x;
	uint32_t y;
	uint8_t block;
	uint8_t joint;
	uint8_t sb;
	uint8_t lz;

	joint = 0;
	for (sb = 0; sb != sbc->bands - 1; sb++) {
		for (block = 0; block < sbc->blocks; block++) {
			sb_j[block][0] = (sbc->samples[block][0][sb] +
			    sbc->samples[block][1][sb]) / 2.0f;
			sb_j[block][1] = (sbc->samples[block][0][sb] -
			    sbc->samples[block][1][sb]) / 2.0f;
		}

		x = 1 << 15;
		y = 1 << 15;
		for (block = 0; block < sbc->blocks; block++) {
			x |= (uint32_t)ABS(sb_j[block][0]);
			y |= (uint32_t)ABS(sb_j[block][1]);
		}

		lz = 1;
		while (!(x & BIT30)) {
			lz++;
			x <<= 1;
		}
		x = 16 - lz;

		lz = 1;
		while (!(y & BIT30)) {
			lz++;
			y <<= 1;
		}
		y = 16 - lz;

		if ((sbc->scalefactor[0][sb] + sbc->scalefactor[1][sb]) > x + y) {
			joint |= 1 << (sbc->bands - sb - 1);
			sbc->scalefactor[0][sb] = x;
			sbc->scalefactor[1][sb] = y;
			for (block = 0; block < sbc->blocks; block++) {
				sbc->samples[block][0][sb] = sb_j[block][0];
				sbc->samples[block][1][sb] = sb_j[block][1];
			}
		}
	}
	return (joint);
}

static void
calc_scalefactors(struct sbc_encode *sbc)
{
	uint8_t block;
	uint8_t ch;
	uint8_t sb;

	for (ch = 0; ch != sbc->channels; ch++) {
		for (sb = 0; sb != sbc->bands; sb++) {
			uint32_t x = 1 << 15;
			uint8_t lx = 1;

			for (block = 0; block != sbc->blocks; block++)
				x |= (uint32_t)ABS(sbc->samples[block][ch][sb]);

			while (!(x & BIT30)) {
				lx++;
				x <<= 1;
			}
			sbc->scalefactor[ch][sb] = 16 - lx;
		}
	}
}

static void
calc_bitneed(struct bt_config *cfg)
{
	struct sbc_encode *sbc = cfg->handle.sbc_enc;
	int32_t bitneed[2][8];
	int32_t max_bitneed, bitcount;
	int32_t slicecount, bitslice;
	int32_t loudness;
	int ch, sb, start_chan = 0;

	if (cfg->chmode == MODE_DUAL)
		sbc->channels = 1;

next_chan:
	max_bitneed = 0;
	bitcount = 0;
	slicecount = 0;

	if (cfg->allocm == ALLOC_SNR) {
		for (ch = start_chan; ch < sbc->channels; ch++) {
			for (sb = 0; sb < sbc->bands; sb++) {
				bitneed[ch][sb] = sbc->scalefactor[ch][sb];

				if (bitneed[ch][sb] > max_bitneed)
					max_bitneed = bitneed[ch][sb];
			}
		}
	} else {
		for (ch = start_chan; ch < sbc->channels; ch++) {
			for (sb = 0; sb < sbc->bands; sb++) {
				if (sbc->scalefactor[ch][sb] == 0) {
					bitneed[ch][sb] = -5;
				} else {
					if (sbc->bands == 8) {
						loudness = sbc->scalefactor[ch][sb] -
						    loudnessoffset8[cfg->freq][sb];
					} else {
						loudness = sbc->scalefactor[ch][sb] -
						    loudnessoffset4[cfg->freq][sb];
					}
					if (loudness > 0)
						bitneed[ch][sb] = loudness / 2;
					else
						bitneed[ch][sb] = loudness;
				}
				if (bitneed[ch][sb] > max_bitneed)
					max_bitneed = bitneed[ch][sb];
			}
		}
	}

	slicecount = bitcount = 0;
	bitslice = max_bitneed + 1;
	do {
		bitslice--;
		bitcount += slicecount;
		slicecount = 0;
		for (ch = start_chan; ch < sbc->channels; ch++) {
			for (sb = 0; sb < sbc->bands; sb++) {
				if ((bitneed[ch][sb] > bitslice + 1) &&
				    (bitneed[ch][sb] < bitslice + 16))
					slicecount++;
				else if (bitneed[ch][sb] == bitslice + 1)
					slicecount += 2;
			}
		}
	} while (bitcount + slicecount < cfg->bitpool);

	/* check if exactly one more fits */
	if (bitcount + slicecount == cfg->bitpool) {
		bitcount += slicecount;
		bitslice--;
	}
	for (ch = start_chan; ch < sbc->channels; ch++) {
		for (sb = 0; sb < sbc->bands; sb++) {
			if (bitneed[ch][sb] < bitslice + 2) {
				sbc->bits[ch][sb] = 0;
			} else {
				sbc->bits[ch][sb] = bitneed[ch][sb] - bitslice;
				if (sbc->bits[ch][sb] > 16)
					sbc->bits[ch][sb] = 16;
			}
		}
	}

	if (cfg->chmode == MODE_DUAL)
		ch = start_chan;
	else
		ch = 0;
	sb = 0;
	while (bitcount < cfg->bitpool && sb < sbc->bands) {
		if ((sbc->bits[ch][sb] >= 2) && (sbc->bits[ch][sb] < 16)) {
			sbc->bits[ch][sb]++;
			bitcount++;
		} else if ((bitneed[ch][sb] == bitslice + 1) &&
		    (cfg->bitpool > bitcount + 1)) {
			sbc->bits[ch][sb] = 2;
			bitcount += 2;
		}
		if (sbc->channels == 1 || start_chan == 1)
			sb++;
		else if (ch == 1) {
			ch = 0;
			sb++;
		} else
			ch = 1;
	}

	if (cfg->chmode == MODE_DUAL)
		ch = start_chan;
	else
		ch = 0;
	sb = 0;
	while (bitcount < cfg->bitpool && sb < sbc->bands) {
		if (sbc->bits[ch][sb] < 16) {
			sbc->bits[ch][sb]++;
			bitcount++;
		}
		if (sbc->channels == 1 || start_chan == 1)
			sb++;
		else if (ch == 1) {
			ch = 0;
			sb++;
		} else
			ch = 1;
	}

	if (cfg->chmode == MODE_DUAL && start_chan == 0) {
		start_chan = 1;
		sbc->channels = 2;
		goto next_chan;
	}
}

static void
sbc_store_bits_crc(struct sbc_encode *sbc, uint32_t numbits, uint32_t value)
{
	uint32_t off = sbc->bitoffset;

	while (numbits-- && off != sbc->maxoffset) {
		if (value & (1 << numbits)) {
			sbc->data[off / 8] |= 1 << ((7 - off) & 7);
			sbc->crc ^= 0x80;
		}
		sbc->crc *= 2;
		if (sbc->crc & 0x100)
			sbc->crc ^= 0x11d;	/* CRC-8 polynomial */

		off++;
	}
	sbc->bitoffset = off;
}

static int
sbc_encode(struct bt_config *cfg)
{
	struct sbc_encode *sbc = cfg->handle.sbc_enc;
	const int16_t *input = sbc->music_data;
	float delta[2][8];
	float levels[2][8];
	float mask[2][8];
	float S;
	float *X;
	float Z[80];
	float Y[80];
	float audioout;
	int16_t left[8];
	int16_t right[8];
	int16_t *data;
	int numsamples;
	int i;
	int k;
	int block;
	int chan;
	int sb;

	for (block = 0; block < sbc->blocks; block++) {

		for (i = 0; i < sbc->bands; i++) {
			left[i] = *input++;
			if (sbc->channels == 2)
				right[i] = *input++;
		}

		for (chan = 0; chan < sbc->channels; chan++) {

			/* select right or left channel */
			if (chan == 0) {
				X = sbc->left;
				data = left;
			} else {
				X = sbc->right;
				data = right;
			}

			/* shift up old data */
			for (i = (sbc->bands * 10) - 1; i > sbc->bands - 1; i--)
				X[i] = X[i - sbc->bands];
			k = 0;
			for (i = sbc->bands - 1; i >= 0; i--)
				X[i] = data[k++];
			for (i = 0; i < sbc->bands * 10; i++) {
				if (sbc->bands == 8)
					Z[i] = sbc_coeffs8[i] * X[i];
				else
					Z[i] = sbc_coeffs4[i] * X[i];
			}
			for (i = 0; i < sbc->bands * 2; i++) {
				Y[i] = 0;
				for (k = 0; k < 5; k++)
					Y[i] += Z[i + k * sbc->bands * 2];
			}
			for (i = 0; i < sbc->bands; i++) {
				S = 0;
				for (k = 0; k < sbc->bands * 2; k++) {
					if (sbc->bands == 8) {
						S += cosdata8[i][k] * Y[k];
					} else {
						S += cosdata4[i][k] * Y[k];
					}
				}
				sbc->samples[block][chan][i] = S * (1 << 15);
			}
		}
	}

	calc_scalefactors(sbc);

	if (cfg->chmode == MODE_JOINT)
		sbc->join = calc_scalefactors_joint(sbc);
	else
		sbc->join = 0;

	calc_bitneed(cfg);

	for (chan = 0; chan < sbc->channels; chan++) {
		for (sb = 0; sb < sbc->bands; sb++) {
			if (sbc->bits[chan][sb] == 0)
				continue;
			mask[chan][sb] = BM(sbc->bits[chan][sb]);
			levels[chan][sb] = mask[chan][sb] *
			    (1LL << (15 - sbc->scalefactor[chan][sb]));
			delta[chan][sb] =
			    (1LL << (sbc->scalefactor[chan][sb] + 16));
		}
	}

	numsamples = 0;
	for (block = 0; block < sbc->blocks; block++) {
		for (chan = 0; chan < sbc->channels; chan++) {
			for (sb = 0; sb < sbc->bands; sb++) {
				if (sbc->bits[chan][sb] == 0)
					continue;
				audioout = (levels[chan][sb] *
				    (delta[chan][sb] + sbc->samples[block][chan][sb]));
				audioout /= (1LL << 32);

				audioout = roundf(audioout);

				/* range check */
				if (audioout > mask[chan][sb])
					audioout = mask[chan][sb];

				sbc->output[numsamples++] = audioout;
			}
		}
	}
	return (numsamples);
}

static void
sbc_decode(struct bt_config *cfg)
{
	struct sbc_encode *sbc = cfg->handle.sbc_enc;
	float delta[2][8];
	float levels[2][8];
	float audioout;
	float *X;
	float *V;
	float left[160];
	float right[160];
	float U[160];
	float W[160];
	float S[8];
	int position;
	int block;
	int chan;
	int sb;
	int i;
	int k;

	for (chan = 0; chan < sbc->channels; chan++) {
		for (sb = 0; sb < sbc->bands; sb++) {
			levels[chan][sb] = (1 << sbc->bits[chan][sb]) - 1;
			delta[chan][sb] = (1 << sbc->scalefactor[chan][sb]);
		}
	}

	i = 0;
	for (block = 0; block < sbc->blocks; block++) {
		for (chan = 0; chan < sbc->channels; chan++) {
			for (sb = 0; sb < sbc->bands; sb++) {
				if (sbc->bits[chan][sb] == 0) {
					audioout = 0;
				} else {
					audioout =
					  ((((sbc->output[i] * 2.0f) + 1.0f) * delta[chan][sb]) /
					  levels[chan][sb]) - delta[chan][sb];
				}
				sbc->output[i++] = audioout;
			}
		}
	}

	if (cfg->chmode == MODE_JOINT) {
		i = 0;
		while (i < (sbc->blocks * sbc->bands * sbc->channels)) {
			for (sb = 0; sb < sbc->bands; sb++) {
				if (sbc->join & (1 << (sbc->bands - sb - 1))) {
					audioout = sbc->output[i];
					sbc->output[i] = (2.0f * sbc->output[i]) +
					    (2.0f * sbc->output[i + sbc->bands]);
					sbc->output[i + sbc->bands] =
					    (2.0f * audioout) -
					    (2.0f * sbc->output[i + sbc->bands]);
					sbc->output[i] /= 2.0f;
					sbc->output[i + sbc->bands] /= 2.0f;
				}
				i++;
			}
			i += sbc->bands;
		}
	}
	position = 0;
	for (block = 0; block < sbc->blocks; block++) {
		for (chan = 0; chan < sbc->channels; chan++) {
			/* select right or left channel */
			if (chan == 0) {
				X = left;
				V = sbc->left;
			} else {
				X = right;
				V = sbc->right;
			}
			for (i = 0; i < sbc->bands; i++)
				S[i] = sbc->output[position++];

			for (i = (sbc->bands * 20) - 1; i >= (sbc->bands * 2); i--)
				V[i] = V[i - (sbc->bands * 2)];
			for (k = 0; k < sbc->bands * 2; k++) {
				float vk = 0;
				for (i = 0; i < sbc->bands; i++) {
					if (sbc->bands == 8) {
						vk += cosdecdata8[i][k] * S[i];
					} else {
						vk += cosdecdata4[i][k] * S[i];
					}
				}
				V[k] = vk;
			}
			for (i = 0; i <= 4; i++) {
				for (k = 0; k < sbc->bands; k++) {
					U[(i * sbc->bands * 2) + k] =
					    V[(i * sbc->bands * 4) + k];
					U[(i * sbc->bands
					    * 2) + sbc->bands + k] =
					    V[(i * sbc->bands * 4) +
					    (sbc->bands * 3) + k];
				}
			}
			for (i = 0; i < sbc->bands * 10; i++) {
				if (sbc->bands == 4) {
					W[i] = U[i] * (sbc_coeffs4[i] * -4.0f);
				} else if (sbc->bands == 8) {
					W[i] = U[i] * (sbc_coeffs8[i] * -8.0f);
				} else {
					W[i] = 0;
				}
			}

			for (k = 0; k < sbc->bands; k++) {
				unsigned int offset = k + (block * sbc->bands);

				X[offset] = 0;
				for (i = 0; i < 10; i++) {
					X[offset] += W[k + (i * sbc->bands)];
				}

				if (X[offset] > 32767.0)
					X[offset] = 32767.0;
				else if (X[offset] < -32767.0)
					X[offset] = -32767.0;
			}
		}
	}

	for (i = 0, k = 0; k != (sbc->blocks * sbc->bands); k++) {
		sbc->music_data[i++] = left[k];
		if (sbc->channels == 2)
			sbc->music_data[i++] = right[k];
	}
}

size_t
sbc_encode_frame(struct bt_config *cfg)
{
	struct sbc_encode *sbc = cfg->handle.sbc_enc;
	uint8_t config;
	uint8_t block;
	uint8_t chan;
	uint8_t sb;
	uint8_t j;
	uint8_t i;

	config = (cfg->freq << 6) | (cfg->blocks << 4) |
	    (cfg->chmode << 2) | (cfg->allocm << 1) | cfg->bands;

	sbc_encode(cfg);

	/* set initial CRC */
	sbc->crc = 0x5e;

	/* reset data position and size */
	sbc->bitoffset = 0;
	sbc->maxoffset = sizeof(sbc->data) * 8;

	sbc_store_bits_crc(sbc, 8, SYNCWORD);
	sbc_store_bits_crc(sbc, 8, config);
	sbc_store_bits_crc(sbc, 8, cfg->bitpool);

	/* skip 8-bit CRC */
	sbc->bitoffset += 8;

	if (cfg->chmode == MODE_JOINT) {
		if (sbc->bands == 8)
			sbc_store_bits_crc(sbc, 8, sbc->join);
		else if (sbc->bands == 4)
			sbc_store_bits_crc(sbc, 4, sbc->join);
	}
	for (i = 0; i < sbc->channels; i++) {
		for (j = 0; j < sbc->bands; j++)
			sbc_store_bits_crc(sbc, 4, sbc->scalefactor[i][j]);
	}

	/* store 8-bit CRC */
	sbc->data[3] = (sbc->crc & 0xFF);

	i = 0;
	for (block = 0; block < sbc->blocks; block++) {
		for (chan = 0; chan < sbc->channels; chan++) {
			for (sb = 0; sb < sbc->bands; sb++) {
				if (sbc->bits[chan][sb] == 0)
					continue;

				sbc_store_bits_crc(sbc, sbc->bits[chan][sb], sbc->output[i++]);
			}
		}
	}
	return ((sbc->bitoffset + 7) / 8);
}

static uint32_t
sbc_load_bits_crc(struct sbc_encode *sbc, uint32_t numbits)
{
	uint32_t off = sbc->bitoffset;
	uint32_t value = 0;

	while (numbits-- && off != sbc->maxoffset) {
		if (sbc->rem_data_ptr[off / 8] & (1 << ((7 - off) & 7))) {
			value |= (1 << numbits);
			sbc->crc ^= 0x80;
		}
		sbc->crc *= 2;
		if (sbc->crc & 0x100)
			sbc->crc ^= 0x11d;	/* CRC-8 polynomial */

		off++;
	}
	sbc->bitoffset = off;
	return (value);
}

size_t
sbc_decode_frame(struct bt_config *cfg, int bits)
{
	struct sbc_encode *sbc = cfg->handle.sbc_enc;
	uint8_t config;
	uint8_t block;
	uint8_t chan;
	uint8_t sb;
	uint8_t j;
	uint8_t i;

	sbc->rem_off = 0;
	sbc->rem_len = 0;

	config = (cfg->freq << 6) | (cfg->blocks << 4) |
	    (cfg->chmode << 2) | (cfg->allocm << 1) | cfg->bands;

	/* set initial CRC */
	sbc->crc = 0x5e;

	/* reset data position and size */
	sbc->bitoffset = 0;
	sbc->maxoffset = bits;

	/* verify SBC header */
	if (sbc->maxoffset < (8 * 4))
		return (0);
	if (sbc_load_bits_crc(sbc, 8) != SYNCWORD)
		return (0);
	if (sbc_load_bits_crc(sbc, 8) != config)
		return (0);
	cfg->bitpool = sbc_load_bits_crc(sbc, 8);

	(void)sbc_load_bits_crc(sbc, 8);/* CRC */

	if (cfg->chmode == MODE_JOINT) {
		if (sbc->bands == 8)
			sbc->join = sbc_load_bits_crc(sbc, 8);
		else if (sbc->bands == 4)
			sbc->join = sbc_load_bits_crc(sbc, 4);
		else
			sbc->join = 0;
	} else {
		sbc->join = 0;
	}

	for (i = 0; i < sbc->channels; i++) {
		for (j = 0; j < sbc->bands; j++)
			sbc->scalefactor[i][j] = sbc_load_bits_crc(sbc, 4);
	}

	calc_bitneed(cfg);

	i = 0;
	for (block = 0; block < sbc->blocks; block++) {
		for (chan = 0; chan < sbc->channels; chan++) {
			for (sb = 0; sb < sbc->bands; sb++) {
				if (sbc->bits[chan][sb] == 0) {
					i++;
					continue;
				}
				sbc->output[i++] =
				    sbc_load_bits_crc(sbc, sbc->bits[chan][sb]);
			}
		}
	}

	sbc_decode(cfg);

	sbc->rem_off = 0;
	sbc->rem_len = sbc->blocks * sbc->channels * sbc->bands;

	return ((sbc->bitoffset + 7) / 8);
}
