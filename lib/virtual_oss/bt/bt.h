/*-
 * Copyright (c) 2015 Hans Petter Selasky
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
 */

#ifndef _BACKEND_BT_H_
#define	_BACKEND_BT_H_

#ifdef HAVE_LIBAV
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#endif

#include "sbc_encode.h"

struct bt_config {
	uint8_t	sep;			/* SEID of the peer */
	uint8_t	media_Type;
	uint8_t	chmode;
#define	MODE_STEREO	2
#define	MODE_JOINT	3
#define	MODE_DUAL	1
#define	MODE_MONO	0
	uint8_t	allocm;
#define	ALLOC_LOUDNESS	0
#define	ALLOC_SNR 	1
	uint8_t	bitpool;
	uint8_t	bands;
#define	BANDS_4		0
#define	BANDS_8		1
	uint8_t	blocks;
#define	BLOCKS_4	0
#define	BLOCKS_8	1
#define	BLOCKS_12	2
#define	BLOCKS_16	3
	uint8_t	freq;
#define	FREQ_UNDEFINED	255
#define	FREQ_16K	0
#define	FREQ_32K	1
#define	FREQ_44_1K	2
#define	FREQ_48K	3
	uint16_t mtu;
	uint8_t	codec;
#define	CODEC_SBC 0x00
#define	CODEC_AAC 0x02
	uint8_t	aacMode1;
	uint8_t	aacMode2;

	/* transcoding handle(s) */
	union {
#ifdef HAVE_LIBAV
		struct {
			AVCodec *codec;
			AVCodecContext *context;
			AVFormatContext *format;
			AVFrame *frame;
			AVStream *stream;
		} av;
#endif
		struct sbc_encode *sbc_enc;
	}	handle;

	/* audio input buffer */
	uint32_t rem_in_len;
	uint32_t rem_in_size;
	uint8_t *rem_in_data;

	/* data transport */
	uint32_t mtu_seqnumber;
	uint32_t mtu_timestamp;
	uint32_t mtu_offset;

	/* bluetooth file handles */
	int fd;
	int hc;

	/* scratch buffer */
	uint8_t	mtu_data[65536];

	/* acceptor state */
	int8_t	acceptor_state;
#define	acpInitial 1
#define	acpConfigurationSet 2
#define	acpStreamOpened 3
#define	acpStreamStarted 4
#define	acpStreamSuspended 5
#define	acpStreamClosed 6
};

size_t	sbc_encode_frame(struct bt_config *);
size_t	sbc_decode_frame(struct bt_config *, int);

int	bt_receive(struct bt_config *cfg, void *ptr, int len, int use_delay);

#endif					/* _BACKEND_BT_H_ */
