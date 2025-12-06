/* $NetBSD$ */

/*-
 * Copyright (c) 2015-2016 Nathanial Sloss <nathanialsloss@yahoo.com.au>
 * Copyright (c) 2016-2019 Hans Petter Selasky <hps@selasky.org>
 * Copyright (c) 2019 Google LLC, written by Richard Kralovic <riso@google.com>
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

#include <sys/uio.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "avdtp_signal.h"
#include "bt.h"

#define	DPRINTF(...) printf("backend_bt: " __VA_ARGS__)

struct avdtpGetPacketInfo {
	uint8_t buffer_data[512];
	uint16_t buffer_len;
	uint8_t trans;
	uint8_t signalID;
};

static int avdtpAutoConfig(struct bt_config *);

/* Return received message type if success, < 0 if failure. */
static int
avdtpGetPacket(int fd, struct avdtpGetPacketInfo *info)
{
	uint8_t *pos = info->buffer_data;
	uint8_t *end = info->buffer_data + sizeof(info->buffer_data);
	uint8_t message_type;
	int len;

	memset(info, 0, sizeof(*info));

	/* Handle fragmented packets */
	for (int remaining = 1; remaining > 0; --remaining) {
		len = read(fd, pos, end - pos);

		if (len < AVDTP_LEN_SUCCESS)
			return (-1);
		if (len == (int)(end - pos))
			return (-1);	/* buffer too small */

		uint8_t trans = (pos[0] & TRANSACTIONLABEL) >> TRANSACTIONLABEL_S;
		uint8_t packet_type = (pos[0] & PACKETTYPE) >> PACKETTYPE_S;
		uint8_t current_message_type = (info->buffer_data[0] & MESSAGETYPE);
		uint8_t shift;
		if (pos == info->buffer_data) {
			info->trans = trans;
			message_type = current_message_type;
			if (packet_type == singlePacket) {
				info->signalID = (pos[1] & SIGNALID_MASK);
				shift = 2;
			} else {
				if (packet_type != startPacket)
					return (-1);
				remaining = pos[1];
				info->signalID = (pos[2] & SIGNALID_MASK);
				shift = 3;
			}
		} else {
			if (info->trans != trans ||
			    message_type != current_message_type ||
			    (remaining == 1 && packet_type != endPacket) ||
			    (remaining > 1 && packet_type != continuePacket)) {
				return (-1);
			}
			shift = 1;
		}
		memmove(pos, pos + shift, len);
		pos += len;
	}
	info->buffer_len = pos - info->buffer_data;
	return (message_type);
}

/* Returns 0 on success, < 0 on failure. */
static int
avdtpSendPacket(int fd, uint8_t command, uint8_t trans, uint8_t type,
    uint8_t * data0, int datasize0, uint8_t * data1,
    int datasize1)
{
	struct iovec iov[3];
	uint8_t header[2];
	int retval;

	/* fill out command header */
	header[0] = (trans << 4) | (type & 3);
	if (command != 0)
		header[1] = command & 0x3f;
	else
		header[1] = 3;

	iov[0].iov_base = header;
	iov[0].iov_len = 2;
	iov[1].iov_base = data0;
	iov[1].iov_len = datasize0;
	iov[2].iov_base = data1;
	iov[2].iov_len = datasize1;

	retval = writev(fd, iov, 3);
	if (retval != (2 + datasize0 + datasize1))
		return (-EINVAL);
	else
		return (0);
}

/* Returns 0 on success, < 0 on failure. */
static int
avdtpSendSyncCommand(int fd, struct avdtpGetPacketInfo *info,
    uint8_t command, uint8_t type, uint8_t * data0,
    int datasize0, uint8_t * data1, int datasize1)
{
	static uint8_t transLabel;
	uint8_t trans;
	int retval;

	alarm(8);			/* set timeout */

	trans = (transLabel++) & 0xF;

	retval = avdtpSendPacket(fd, command, trans, type,
	    data0, datasize0, data1, datasize1);
	if (retval)
		goto done;
retry:
	switch (avdtpGetPacket(fd, info)) {
	case RESPONSEACCEPT:
		if (info->trans != trans)
			goto retry;
		retval = 0;
		break;
	case RESPONSEREJECT:
		if (info->trans != trans)
			goto retry;
		retval = -EINVAL;
		break;
	case COMMAND:
		retval = avdtpSendReject(fd, info->trans, info->signalID);
		if (retval == 0)
			goto retry;
		break;
	default:
		retval = -ENXIO;
		break;
	}
done:
	alarm(0);			/* clear timeout */

	return (retval);
}

/*
 * Variant for acceptor role: We support any frequency, blocks, bands, and
 * allocation. Returns 0 on success, < 0 on failure.
 */
static int
avdtpSendCapabilitiesResponseSBCForACP(int fd, int trans)
{
	uint8_t data[10];

	data[0] = mediaTransport;
	data[1] = 0;
	data[2] = mediaCodec;
	data[3] = 0x6;
	data[4] = mediaTypeAudio;
	data[5] = SBC_CODEC_ID;
	data[6] =
	    (1 << (3 - MODE_STEREO)) |
	    (1 << (3 - MODE_JOINT)) |
	    (1 << (3 - MODE_DUAL)) |
	    (1 << (3 - MODE_MONO)) |
	    (1 << (7 - FREQ_44_1K)) |
	    (1 << (7 - FREQ_48K)) |
	    (1 << (7 - FREQ_32K)) |
	    (1 << (7 - FREQ_16K));
	data[7] =
	    (1 << (7 - BLOCKS_4)) |
	    (1 << (7 - BLOCKS_8)) |
	    (1 << (7 - BLOCKS_12)) |
	    (1 << (7 - BLOCKS_16)) |
	    (1 << (3 - BANDS_4)) |
	    (1 << (3 - BANDS_8)) | (1 << ALLOC_LOUDNESS) | (1 << ALLOC_SNR);
	data[8] = MIN_BITPOOL;
	data[9] = DEFAULT_MAXBPOOL;

	return (avdtpSendPacket(fd, AVDTP_GET_CAPABILITIES, trans,
	    RESPONSEACCEPT, data, sizeof(data), NULL, 0));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpSendAccept(int fd, uint8_t trans, uint8_t myCommand)
{
	return (avdtpSendPacket(fd, myCommand, trans, RESPONSEACCEPT,
	    NULL, 0, NULL, 0));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpSendReject(int fd, uint8_t trans, uint8_t myCommand)
{
	uint8_t value = 0;

	return (avdtpSendPacket(fd, myCommand, trans, RESPONSEREJECT,
	    &value, 1, NULL, 0));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpSendDiscResponseAudio(int fd, uint8_t trans,
    uint8_t mySep, uint8_t is_sink)
{
	uint8_t data[2];

	data[0] = mySep << 2;
	data[1] = mediaTypeAudio << 4 | (is_sink ? (1 << 3) : 0);

	return (avdtpSendPacket(fd, AVDTP_DISCOVER, trans, RESPONSEACCEPT,
	    data, 2, NULL, 0));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpDiscoverAndConfig(struct bt_config *cfg, bool isSink)
{
	struct avdtpGetPacketInfo info;
	uint16_t offset;
	uint8_t chmode = cfg->chmode;
	uint8_t aacMode1 = cfg->aacMode1;
	uint8_t aacMode2 = cfg->aacMode2;
	int retval;

	retval = avdtpSendSyncCommand(cfg->hc, &info, AVDTP_DISCOVER, 0,
	    NULL, 0, NULL, 0);
	if (retval)
		return (retval);

	retval = -EBUSY;
	for (offset = 0; offset + 2 <= info.buffer_len; offset += 2) {
		cfg->sep = info.buffer_data[offset] >> 2;
		cfg->media_Type = info.buffer_data[offset + 1] >> 4;
		cfg->chmode = chmode;
		cfg->aacMode1 = aacMode1;
		cfg->aacMode2 = aacMode2;
		if (info.buffer_data[offset] & DISCOVER_SEP_IN_USE)
			continue;
		if (info.buffer_data[offset + 1] & DISCOVER_IS_SINK) {
			if (!isSink)
				continue;
		} else {
			if (isSink)
				continue;
		}
		/* try to configure SBC */
		retval = avdtpAutoConfig(cfg);
		if (retval == 0)
			return (0);
	}
	return (retval);
}

/* Returns 0 on success, < 0 on failure. */
static int
avdtpGetCapabilities(int fd, uint8_t sep, struct avdtpGetPacketInfo *info)
{
	uint8_t address = (sep << 2);

	return (avdtpSendSyncCommand(fd, info,
	    AVDTP_GET_CAPABILITIES, 0, &address, 1,
	    NULL, 0));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpSetConfiguration(int fd, uint8_t sep, uint8_t * data, int datasize)
{
	struct avdtpGetPacketInfo info;
	uint8_t configAddresses[2];

	configAddresses[0] = sep << 2;
	configAddresses[1] = INTSEP << 2;

	return (avdtpSendSyncCommand(fd, &info, AVDTP_SET_CONFIGURATION, 0,
	    configAddresses, 2, data, datasize));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpOpen(int fd, uint8_t sep)
{
	struct avdtpGetPacketInfo info;
	uint8_t address = sep << 2;

	return (avdtpSendSyncCommand(fd, &info, AVDTP_OPEN, 0,
	    &address, 1, NULL, 0));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpStart(int fd, uint8_t sep)
{
	struct avdtpGetPacketInfo info;
	uint8_t address = sep << 2;

	return (avdtpSendSyncCommand(fd, &info, AVDTP_START, 0,
	    &address, 1, NULL, 0));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpClose(int fd, uint8_t sep)
{
	struct avdtpGetPacketInfo info;
	uint8_t address = sep << 2;

	return (avdtpSendSyncCommand(fd, &info, AVDTP_CLOSE, 0,
	    &address, 1, NULL, 0));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpSuspend(int fd, uint8_t sep)
{
	struct avdtpGetPacketInfo info;
	uint8_t address = sep << 2;

	return (avdtpSendSyncCommand(fd, &info, AVDTP_SUSPEND, 0,
	    &address, 1, NULL, 0));
}

/* Returns 0 on success, < 0 on failure. */
int
avdtpAbort(int fd, uint8_t sep)
{
	struct avdtpGetPacketInfo info;
	uint8_t address = sep << 2;

	return (avdtpSendSyncCommand(fd, &info, AVDTP_ABORT, 0,
	    &address, 1, NULL, 0));
}

static int
avdtpAutoConfig(struct bt_config *cfg)
{
	struct avdtpGetPacketInfo info;
	uint8_t freqmode;
	uint8_t blk_len_sb_alloc;
	uint8_t availFreqMode = 0;
	uint8_t availConfig = 0;
	uint8_t supBitpoolMin = 0;
	uint8_t supBitpoolMax = 0;
	uint8_t aacMode1 = 0;
	uint8_t aacMode2 = 0;
#ifdef HAVE_LIBAV
	uint8_t aacBitrate3 = 0;
	uint8_t aacBitrate4 = 0;
	uint8_t aacBitrate5 = 0;
#endif
	int retval;
	int i;

	retval = avdtpGetCapabilities(cfg->hc, cfg->sep, &info);
	if (retval) {
		DPRINTF("Cannot get capabilities\n");
		return (retval);
	}
retry:
	for (i = 0; (i + 1) < info.buffer_len;) {
#if 0
		DPRINTF("0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		    info.buffer_data[i + 0],
		    info.buffer_data[i + 1],
		    info.buffer_data[i + 2],
		    info.buffer_data[i + 3],
		    info.buffer_data[i + 4], info.buffer_data[i + 5]);
#endif
		if (i + 2 + info.buffer_data[i + 1] > info.buffer_len)
			break;
		switch (info.buffer_data[i]) {
		case mediaTransport:
			break;
		case mediaCodec:
			if (info.buffer_data[i + 1] < 2)
				break;
			/* check codec */
			switch (info.buffer_data[i + 3]) {
			case 0:			/* SBC */
				if (info.buffer_data[i + 1] < 6)
					break;
				availFreqMode = info.buffer_data[i + 4];
				availConfig = info.buffer_data[i + 5];
				supBitpoolMin = info.buffer_data[i + 6];
				supBitpoolMax = info.buffer_data[i + 7];
				break;
			case 2:			/* MPEG2/4 AAC */
				if (info.buffer_data[i + 1] < 8)
					break;
				aacMode1 = info.buffer_data[i + 5];
				aacMode2 = info.buffer_data[i + 6];
#ifdef HAVE_LIBAV
				aacBitrate3 = info.buffer_data[i + 7];
				aacBitrate4 = info.buffer_data[i + 8];
				aacBitrate5 = info.buffer_data[i + 9];
#endif
				break;
			default:
				break;
			}
		}
		/* jump to next information element */
		i += 2 + info.buffer_data[i + 1];
	}
	aacMode1 &= cfg->aacMode1;
	aacMode2 &= cfg->aacMode2;

	/* Try AAC first */
	if (aacMode1 == cfg->aacMode1 && aacMode2 == cfg->aacMode2) {
#ifdef HAVE_LIBAV
		uint8_t config[12] = { mediaTransport, 0x0, mediaCodec,
			0x8, 0x0, 0x02, 0x80, aacMode1, aacMode2, aacBitrate3,
			aacBitrate4, aacBitrate5
		};

		if (avdtpSetConfiguration
		    (cfg->hc, cfg->sep, config, sizeof(config)) == 0) {
			cfg->codec = CODEC_AAC;
			return (0);
		}
#endif
	}
	/* Try SBC second */
	if (cfg->freq == FREQ_UNDEFINED)
		goto auto_config_failed;

	freqmode = (1 << (3 - cfg->freq + 4)) | (1 << (3 - cfg->chmode));

	if ((availFreqMode & freqmode) != freqmode) {
		DPRINTF("No frequency and mode match\n");
		goto auto_config_failed;
	}
	for (i = 0; i != 4; i++) {
		blk_len_sb_alloc = (1 << (i + 4)) |
		    (1 << (1 - cfg->bands + 2)) | (1 << cfg->allocm);

		if ((availConfig & blk_len_sb_alloc) == blk_len_sb_alloc)
			break;
	}
	if (i == 4) {
		DPRINTF("No bands available\n");
		goto auto_config_failed;
	}
	cfg->blocks = (3 - i);

	if (cfg->allocm == ALLOC_SNR)
		supBitpoolMax &= ~1;

	if (cfg->chmode == MODE_DUAL || cfg->chmode == MODE_MONO)
		supBitpoolMax /= 2;

	if (cfg->bands == BANDS_4)
		supBitpoolMax /= 2;

	if (supBitpoolMax > cfg->bitpool)
		supBitpoolMax = cfg->bitpool;
	else
		cfg->bitpool = supBitpoolMax;

	do {
		uint8_t config[10] = { mediaTransport, 0x0, mediaCodec, 0x6,
			0x0, 0x0, freqmode, blk_len_sb_alloc, supBitpoolMin,
			supBitpoolMax
		};

		if (avdtpSetConfiguration
		    (cfg->hc, cfg->sep, config, sizeof(config)) == 0) {
			cfg->codec = CODEC_SBC;
			return (0);
		}
	} while (0);

auto_config_failed:
	if (cfg->chmode == MODE_STEREO) {
		cfg->chmode = MODE_MONO;
		cfg->aacMode2 ^= 0x0C;
		goto retry;
	}
	return (-EINVAL);
}

void
avdtpACPFree(struct bt_config *cfg)
{
	if (cfg->handle.sbc_enc) {
		free(cfg->handle.sbc_enc);
		cfg->handle.sbc_enc = NULL;
	}
}

/* Returns 0 on success, < 0 on failure. */
static int
avdtpParseSBCConfig(uint8_t * data, struct bt_config *cfg)
{
	if (data[0] & (1 << (7 - FREQ_48K))) {
		cfg->freq = FREQ_48K;
	} else if (data[0] & (1 << (7 - FREQ_44_1K))) {
		cfg->freq = FREQ_44_1K;
	} else if (data[0] & (1 << (7 - FREQ_32K))) {
		cfg->freq = FREQ_32K;
	} else if (data[0] & (1 << (7 - FREQ_16K))) {
		cfg->freq = FREQ_16K;
	} else {
		return -EINVAL;
	}

	if (data[0] & (1 << (3 - MODE_STEREO))) {
		cfg->chmode = MODE_STEREO;
	} else if (data[0] & (1 << (3 - MODE_JOINT))) {
		cfg->chmode = MODE_JOINT;
	} else if (data[0] & (1 << (3 - MODE_DUAL))) {
		cfg->chmode = MODE_DUAL;
	} else if (data[0] & (1 << (3 - MODE_MONO))) {
		cfg->chmode = MODE_MONO;
	} else {
		return -EINVAL;
	}

	if (data[1] & (1 << (7 - BLOCKS_16))) {
		cfg->blocks = BLOCKS_16;
	} else if (data[1] & (1 << (7 - BLOCKS_12))) {
		cfg->blocks = BLOCKS_12;
	} else if (data[1] & (1 << (7 - BLOCKS_8))) {
		cfg->blocks = BLOCKS_8;
	} else if (data[1] & (1 << (7 - BLOCKS_4))) {
		cfg->blocks = BLOCKS_4;
	} else {
		return -EINVAL;
	}

	if (data[1] & (1 << (3 - BANDS_8))) {
		cfg->bands = BANDS_8;
	} else if (data[1] & (1 << (3 - BANDS_4))) {
		cfg->bands = BANDS_4;
	} else {
		return -EINVAL;
	}

	if (data[1] & (1 << ALLOC_LOUDNESS)) {
		cfg->allocm = ALLOC_LOUDNESS;
	} else if (data[1] & (1 << ALLOC_SNR)) {
		cfg->allocm = ALLOC_SNR;
	} else {
		return -EINVAL;
	}
	cfg->bitpool = data[3];
	return 0;
}

int
avdtpACPHandlePacket(struct bt_config *cfg)
{
	struct avdtpGetPacketInfo info;
	int retval;

	if (avdtpGetPacket(cfg->hc, &info) != COMMAND)
		return (-ENXIO);

	switch (info.signalID) {
	case AVDTP_DISCOVER:
		retval =
		    avdtpSendDiscResponseAudio(cfg->hc, info.trans, ACPSEP, 1);
		if (!retval)
			retval = AVDTP_DISCOVER;
		break;
	case AVDTP_GET_CAPABILITIES:
		retval =
		    avdtpSendCapabilitiesResponseSBCForACP(cfg->hc, info.trans);
		if (!retval)
			retval = AVDTP_GET_CAPABILITIES;
		break;
	case AVDTP_SET_CONFIGURATION:
		if (cfg->acceptor_state != acpInitial)
			goto err;
		cfg->sep = info.buffer_data[1] >> 2;
		int is_configured = 0;
		for (int i = 2; (i + 1) < info.buffer_len;) {
			if (i + 2 + info.buffer_data[i + 1] > info.buffer_len)
				break;
			switch (info.buffer_data[i]) {
			case mediaTransport:
				break;
			case mediaCodec:
				if (info.buffer_data[i + 1] < 2)
					break;
				/* check codec */
				switch (info.buffer_data[i + 3]) {
				case 0:		/* SBC */
					if (info.buffer_data[i + 1] < 6)
						break;
					retval =
					    avdtpParseSBCConfig(info.buffer_data + i + 4, cfg);
					if (retval)
						return retval;
					is_configured = 1;
					break;
				case 2:		/* MPEG2/4 AAC */
					/* TODO: Add support */
				default:
					break;
				}
			}
			/* jump to next information element */
			i += 2 + info.buffer_data[i + 1];
		}
		if (!is_configured)
			goto err;

		retval =
		    avdtpSendAccept(cfg->hc, info.trans, AVDTP_SET_CONFIGURATION);
		if (retval)
			return (retval);

		/* TODO: Handle other codecs */
		if (cfg->handle.sbc_enc == NULL) {
			cfg->handle.sbc_enc = malloc(sizeof(*cfg->handle.sbc_enc));
			if (cfg->handle.sbc_enc == NULL)
				return (-ENOMEM);
		}
		memset(cfg->handle.sbc_enc, 0, sizeof(*cfg->handle.sbc_enc));

		retval = AVDTP_SET_CONFIGURATION;
		cfg->acceptor_state = acpConfigurationSet;
		break;
	case AVDTP_OPEN:
		if (cfg->acceptor_state != acpConfigurationSet)
			goto err;
		retval = avdtpSendAccept(cfg->hc, info.trans, info.signalID);
		if (retval)
			return (retval);
		retval = info.signalID;
		cfg->acceptor_state = acpStreamOpened;
		break;
	case AVDTP_START:
		if (cfg->acceptor_state != acpStreamOpened &&
		    cfg->acceptor_state != acpStreamSuspended) {
			goto err;
		}
		retval = avdtpSendAccept(cfg->hc, info.trans, info.signalID);
		if (retval)
			return retval;
		retval = info.signalID;
		cfg->acceptor_state = acpStreamStarted;
		break;
	case AVDTP_CLOSE:
		if (cfg->acceptor_state != acpStreamOpened &&
		    cfg->acceptor_state != acpStreamStarted &&
		    cfg->acceptor_state != acpStreamSuspended) {
			goto err;
		}
		retval = avdtpSendAccept(cfg->hc, info.trans, info.signalID);
		if (retval)
			return (retval);
		retval = info.signalID;
		cfg->acceptor_state = acpStreamClosed;
		break;
	case AVDTP_SUSPEND:
		if (cfg->acceptor_state != acpStreamOpened &&
		    cfg->acceptor_state != acpStreamStarted) {
			goto err;
		}
		retval = avdtpSendAccept(cfg->hc, info.trans, info.signalID);
		if (retval)
			return (retval);
		retval = info.signalID;
		cfg->acceptor_state = acpStreamSuspended;
		break;
	case AVDTP_GET_CONFIGURATION:
	case AVDTP_RECONFIGURE:
	case AVDTP_ABORT:
		/* TODO: Implement this. */
	default:
err:
		avdtpSendReject(cfg->hc, info.trans, info.signalID);
		return (-ENXIO);
	}
	return (retval);
}
