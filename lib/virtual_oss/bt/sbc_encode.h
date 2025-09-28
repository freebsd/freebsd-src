/* $NetBSD$ */

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

#ifndef _SBC_ENCODE_H_
#define	_SBC_ENCODE_H_

#define	MIN_BITPOOL     2
#define	DEFAULT_MAXBPOOL 250

/*
 * SBC header format
 */
struct sbc_header {
	uint8_t	id;
	uint8_t	id2;
	uint8_t	seqnumMSB;
	uint8_t	seqnumLSB;
	uint8_t	ts3;
	uint8_t	ts2;
	uint8_t	ts1;
	uint8_t	ts0;
	uint8_t	reserved3;
	uint8_t	reserved2;
	uint8_t	reserved1;
	uint8_t	reserved0;
	uint8_t	numFrames;
};

struct sbc_encode {
	int16_t	music_data[256];
	uint8_t	data[1024];
	uint8_t *rem_data_ptr;
	int	rem_data_len;
	int	rem_data_frames;
	int	bits[2][8];
	float	output[256];
	float	left[160];
	float	right[160];
	float	samples[16][2][8];
	uint32_t rem_len;
	uint32_t rem_off;
	uint32_t bitoffset;
	uint32_t maxoffset;
	uint32_t crc;
	uint16_t framesamples;
	uint8_t	scalefactor[2][8];
	uint8_t	channels;
	uint8_t	bands;
	uint8_t	blocks;
	uint8_t	join;
};

#endif					/* _SBC_ENCODE_H_ */
