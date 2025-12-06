/*-
 * Copyright (c) 2015-2019 Hans Petter Selasky
 * Copyright (c) 2015 Nathanial Sloss <nathanialsloss@yahoo.com.au>
 * Copyright (c) 2006 Itronix Inc
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

#include <sys/queue.h>
#include <sys/filio.h>
#include <sys/soundcard.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#define	L2CAP_SOCKET_CHECKED
#include <bluetooth.h>
#include <sdp.h>

#include "backend.h"
#include "int.h"

#include "avdtp_signal.h"
#include "bt.h"

#define	DPRINTF(...) printf("backend_bt: " __VA_ARGS__)

struct l2cap_info {
	bdaddr_t laddr;
	bdaddr_t raddr;
};

static struct bt_config bt_play_cfg;
static struct bt_config bt_rec_cfg;

int
bt_receive(struct bt_config *cfg, void *ptr, int len, int use_delay)
{
	struct sbc_header *phdr = (struct sbc_header *)cfg->mtu_data;
	struct sbc_encode *sbc = cfg->handle.sbc_enc;
	uint8_t *tmp = ptr;
	int old_len = len;
	int delta;
	int err;

	/* wait for service interval, if any */
	if (use_delay)
		virtual_oss_wait();

	switch (cfg->blocks) {
	case BLOCKS_4:
		sbc->blocks = 4;
		break;
	case BLOCKS_8:
		sbc->blocks = 8;
		break;
	case BLOCKS_12:
		sbc->blocks = 12;
		break;
	default:
		sbc->blocks = 16;
		break;
	}

	switch (cfg->bands) {
	case BANDS_4:
		sbc->bands = 4;
		break;
	default:
		sbc->bands = 8;
		break;
	}

	if (cfg->chmode != MODE_MONO) {
		sbc->channels = 2;
	} else {
		sbc->channels = 1;
	}

	while (1) {
		delta = len & ~1;
		if (delta > (int)(2 * sbc->rem_len))
			delta = (2 * sbc->rem_len);

		/* copy out samples, if any */
		memcpy(tmp, (char *)sbc->music_data + sbc->rem_off, delta);
		tmp += delta;
		len -= delta;
		sbc->rem_off += delta / 2;
		sbc->rem_len -= delta / 2;
		if (len == 0)
			break;

		if (sbc->rem_len == 0 &&
		    sbc->rem_data_frames != 0) {
			err = sbc_decode_frame(cfg, sbc->rem_data_len * 8);
			sbc->rem_data_frames--;
			sbc->rem_data_ptr += err;
			sbc->rem_data_len -= err;
			continue;
		}
		/* TODO: Support fragmented SBC frames */
		err = read(cfg->fd, cfg->mtu_data, cfg->mtu);

		if (err == 0) {
			break;
		} else if (err < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				return (-1);	/* disconnected */
		}

		/* verify RTP header */
		if (err < (int)sizeof(*phdr) || phdr->id != 0x80)
			continue;

		sbc->rem_data_frames = phdr->numFrames;
		sbc->rem_data_ptr = (uint8_t *)(phdr + 1);
		sbc->rem_data_len = err - sizeof(*phdr);
	}
	return (old_len - len);
}

static int
bt_set_format(int *format)
{
	int value;

	value = *format & AFMT_S16_NE;
	if (value != 0) {
		*format = value;
		return (0);
	}
	return (-1);
}

static void
bt_close(struct voss_backend *pbe)
{
	struct bt_config *cfg = pbe->arg;

	if (cfg->hc > 0) {
		avdtpAbort(cfg->hc, cfg->sep);
		avdtpClose(cfg->hc, cfg->sep);
		close(cfg->hc);
		cfg->hc = -1;
	}
	if (cfg->fd > 0) {
		close(cfg->fd);
		cfg->fd = -1;
	}
}

static void
bt_play_close(struct voss_backend *pbe)
{
	struct bt_config *cfg = pbe->arg;

	switch (cfg->codec) {
	case CODEC_SBC:
		if (cfg->handle.sbc_enc == NULL)
			break;
		free(cfg->handle.sbc_enc);
		cfg->handle.sbc_enc = NULL;
		break;
#ifdef HAVE_LIBAV
	case CODEC_AAC:
		if (cfg->handle.av.context == NULL)
			break;
		av_free(cfg->rem_in_data);
		av_frame_free(&cfg->handle.av.frame);
		avcodec_close(cfg->handle.av.context);
		avformat_free_context(cfg->handle.av.format);
		cfg->handle.av.context = NULL;
		break;
#endif
	default:
		break;
	}
	return (bt_close(pbe));
}

static void
bt_rec_close(struct voss_backend *pbe)
{
	struct bt_config *cfg = pbe->arg;

	switch (cfg->codec) {
	case CODEC_SBC:
		break;
#ifdef HAVE_LIBAV
	case CODEC_AAC:
		break;
#endif

	default:
		break;
	}
	return (bt_close(pbe));
}

static const uint32_t bt_attrs[] = {
	SDP_ATTR_RANGE(SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST,
	    SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST),
};

#define	BT_NUM_VALUES 32
#define	BT_BUF_SIZE 32

static int
bt_find_psm(const uint8_t *start, const uint8_t *end)
{
	uint32_t type;
	uint32_t len;
	int protover = 0;
	int psm = -1;

	if ((end - start) < 2)
		return (-1);

	SDP_GET8(type, start);
	switch (type) {
	case SDP_DATA_SEQ8:
		SDP_GET8(len, start);
		break;

	case SDP_DATA_SEQ16:
		SDP_GET16(len, start);
		break;

	case SDP_DATA_SEQ32:
		SDP_GET32(len, start);
		break;

	default:
		return (-1);
	}

	while (start < end) {
		SDP_GET8(type, start);
		switch (type) {
		case SDP_DATA_SEQ8:
			SDP_GET8(len, start);
			break;

		case SDP_DATA_SEQ16:
			SDP_GET16(len, start);
			break;

		case SDP_DATA_SEQ32:
			SDP_GET32(len, start);
			break;

		default:
			return (-1);
		}
		/* check range */
		if (len > (uint32_t)(end - start))
			break;

		if (len >= 6) {
			const uint8_t *ptr = start;

			SDP_GET8(type, ptr);
			if (type == SDP_DATA_UUID16) {
				uint16_t temp;

				SDP_GET16(temp, ptr);
				switch (temp) {
				case SDP_UUID_PROTOCOL_L2CAP:
					SDP_GET8(type, ptr);
					SDP_GET16(psm, ptr);
					break;
				case SDP_UUID_PROTOCOL_AVDTP:
					SDP_GET8(type, ptr);
					SDP_GET16(protover, ptr);
					break;
				default:
					break;
				}
			}
		}
		start += len;

		if (protover >= 0x0100 && psm > -1)
			return (htole16(psm));
	}
	return (-1);
}

static int
bt_query(struct l2cap_info *info, uint16_t service_class)
{
	sdp_attr_t values[BT_NUM_VALUES];
	uint8_t buffer[BT_NUM_VALUES][BT_BUF_SIZE];
	void *ss;
	int psm = -1;
	int n;

	memset(buffer, 0, sizeof(buffer));
	memset(values, 0, sizeof(values));

	ss = sdp_open(&info->laddr, &info->raddr);
	if (ss == NULL || sdp_error(ss) != 0) {
		DPRINTF("Could not open SDP\n");
		sdp_close(ss);
		return (psm);
	}
	/* Initialize attribute values array */
	for (n = 0; n != BT_NUM_VALUES; n++) {
		values[n].flags = SDP_ATTR_INVALID;
		values[n].vlen = BT_BUF_SIZE;
		values[n].value = buffer[n];
	}

	/* Do SDP Service Search Attribute Request */
	n = sdp_search(ss, 1, &service_class, 1, bt_attrs, BT_NUM_VALUES, values);
	if (n != 0) {
		DPRINTF("SDP search failed\n");
		goto done;
	}
	/* Print attributes values */
	for (n = 0; n != BT_NUM_VALUES; n++) {
		if (values[n].flags != SDP_ATTR_OK)
			break;
		if (values[n].attr != SDP_ATTR_PROTOCOL_DESCRIPTOR_LIST)
			continue;
		psm = bt_find_psm(values[n].value, values[n].value + values[n].vlen);
		if (psm > -1)
			break;
	}
done:
	sdp_close(ss);
	return (psm);
}

static int
bt_open(struct voss_backend *pbe __unused, const char *devname, int samplerate,
    int bufsize __unused, int *pchannels, int *pformat, struct bt_config *cfg,
    int service_class, int isSink)
{
	struct sockaddr_l2cap addr;
	struct l2cap_info info;
	socklen_t mtusize = sizeof(uint16_t);
	int tmpbitpool;
	int l2cap_psm;
	int temp;

	memset(&info, 0, sizeof(info));

	if (strstr(devname, "/dev/bluetooth/") != devname) {
		printf("Invalid device name '%s'", devname);
		goto error;
	}
	/* skip prefix */
	devname += sizeof("/dev/bluetooth/") - 1;

	if (!bt_aton(devname, &info.raddr)) {
		struct hostent *he = NULL;

		if ((he = bt_gethostbyname(devname)) == NULL) {
			DPRINTF("Could not get host by name\n");
			goto error;
		}
		bdaddr_copy(&info.raddr, (bdaddr_t *)he->h_addr);
	}
	switch (samplerate) {
	case 8000:
		cfg->freq = FREQ_UNDEFINED;
		cfg->aacMode1 = 0x80;
		cfg->aacMode2 = 0x0C;
		break;
	case 11025:
		cfg->freq = FREQ_UNDEFINED;
		cfg->aacMode1 = 0x40;
		cfg->aacMode2 = 0x0C;
		break;
	case 12000:
		cfg->freq = FREQ_UNDEFINED;
		cfg->aacMode1 = 0x20;
		cfg->aacMode2 = 0x0C;
		break;
	case 16000:
		cfg->freq = FREQ_16K;
		cfg->aacMode1 = 0x10;
		cfg->aacMode2 = 0x0C;
		break;
	case 22050:
		cfg->freq = FREQ_UNDEFINED;
		cfg->aacMode1 = 0x08;
		cfg->aacMode2 = 0x0C;
		break;
	case 24000:
		cfg->freq = FREQ_UNDEFINED;
		cfg->aacMode1 = 0x04;
		cfg->aacMode2 = 0x0C;
		break;
	case 32000:
		cfg->freq = FREQ_32K;
		cfg->aacMode1 = 0x02;
		cfg->aacMode2 = 0x0C;
		break;
	case 44100:
		cfg->freq = FREQ_44_1K;
		cfg->aacMode1 = 0x01;
		cfg->aacMode2 = 0x0C;
		break;
	case 48000:
		cfg->freq = FREQ_48K;
		cfg->aacMode1 = 0;
		cfg->aacMode2 = 0x8C;
		break;
	case 64000:
		cfg->freq = FREQ_UNDEFINED;
		cfg->aacMode1 = 0;
		cfg->aacMode2 = 0x4C;
		break;
	case 88200:
		cfg->freq = FREQ_UNDEFINED;
		cfg->aacMode1 = 0;
		cfg->aacMode2 = 0x2C;
		break;
	case 96000:
		cfg->freq = FREQ_UNDEFINED;
		cfg->aacMode1 = 0;
		cfg->aacMode2 = 0x1C;
		break;
	default:
		DPRINTF("Invalid samplerate %d", samplerate);
		goto error;
	}
	cfg->bands = BANDS_8;
	cfg->bitpool = 0;

	switch (*pchannels) {
	case 1:
		cfg->aacMode2 &= 0xF8;
		cfg->chmode = MODE_MONO;
		break;
	default:
		cfg->aacMode2 &= 0xF4;
		cfg->chmode = MODE_STEREO;
		break;
	}

	cfg->allocm = ALLOC_LOUDNESS;

	if (cfg->chmode == MODE_MONO || cfg->chmode == MODE_DUAL)
		tmpbitpool = 16;
	else
		tmpbitpool = 32;

	if (cfg->bands == BANDS_8)
		tmpbitpool *= 8;
	else
		tmpbitpool *= 4;

	if (tmpbitpool > DEFAULT_MAXBPOOL)
		tmpbitpool = DEFAULT_MAXBPOOL;

	cfg->bitpool = tmpbitpool;

	if (bt_set_format(pformat)) {
		DPRINTF("Unsupported sample format\n");
		goto error;
	}
	l2cap_psm = bt_query(&info, service_class);
	DPRINTF("PSM=0x%02x\n", l2cap_psm);
	if (l2cap_psm < 0) {
		DPRINTF("PSM not found\n");
		goto error;
	}
	cfg->hc = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BLUETOOTH_PROTO_L2CAP);
	if (cfg->hc < 0) {
		DPRINTF("Could not create BT socket\n");
		goto error;
	}
	memset(&addr, 0, sizeof(addr));
	addr.l2cap_len = sizeof(addr);
	addr.l2cap_family = AF_BLUETOOTH;
	bdaddr_copy(&addr.l2cap_bdaddr, &info.laddr);

	if (bind(cfg->hc, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		DPRINTF("Could not bind to HC\n");
		goto error;
	}
	bdaddr_copy(&addr.l2cap_bdaddr, &info.raddr);
	addr.l2cap_psm = l2cap_psm;
	if (connect(cfg->hc, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		DPRINTF("Could not connect to HC: %d\n", errno);
		goto error;
	}
	if (avdtpDiscoverAndConfig(cfg, isSink)) {
		DPRINTF("DISCOVER FAILED\n");
		goto error;
	}
	if (avdtpOpen(cfg->hc, cfg->sep)) {
		DPRINTF("OPEN FAILED\n");
		goto error;
	}
	cfg->fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BLUETOOTH_PROTO_L2CAP);
	if (cfg->fd < 0) {
		DPRINTF("Could not create BT socket\n");
		goto error;
	}
	memset(&addr, 0, sizeof(addr));

	addr.l2cap_len = sizeof(addr);
	addr.l2cap_family = AF_BLUETOOTH;
	bdaddr_copy(&addr.l2cap_bdaddr, &info.laddr);

	if (bind(cfg->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		DPRINTF("Could not bind\n");
		goto error;
	}
	bdaddr_copy(&addr.l2cap_bdaddr, &info.raddr);
	addr.l2cap_psm = l2cap_psm;
	if (connect(cfg->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		DPRINTF("Could not connect: %d\n", errno);
		goto error;
	}
	if (isSink) {
		if (getsockopt(cfg->fd, SOL_L2CAP, SO_L2CAP_OMTU, &cfg->mtu, &mtusize) == -1) {
			DPRINTF("Could not get MTU\n");
			goto error;
		}
		temp = cfg->mtu * 16;
		if (setsockopt(cfg->fd, SOL_SOCKET, SO_SNDBUF, &temp, sizeof(temp)) == -1) {
			DPRINTF("Could not set send buffer size\n");
			goto error;
		}
		temp = cfg->mtu;
		if (setsockopt(cfg->fd, SOL_SOCKET, SO_SNDLOWAT, &temp, sizeof(temp)) == -1) {
			DPRINTF("Could not set low water mark\n");
			goto error;
		}
	} else {
		if (getsockopt(cfg->fd, SOL_L2CAP, SO_L2CAP_IMTU, &cfg->mtu, &mtusize) == -1) {
			DPRINTF("Could not get MTU\n");
			goto error;
		}
		temp = cfg->mtu * 16;
		if (setsockopt(cfg->fd, SOL_SOCKET, SO_RCVBUF, &temp, sizeof(temp)) == -1) {
			DPRINTF("Could not set receive buffer size\n");
			goto error;
		}
		temp = 1;
		if (setsockopt(cfg->fd, SOL_SOCKET, SO_RCVLOWAT, &temp, sizeof(temp)) == -1) {
			DPRINTF("Could not set low water mark\n");
			goto error;
		}
		temp = 1;
		if (ioctl(cfg->fd, FIONBIO, &temp) == -1) {
			DPRINTF("Could not set non-blocking I/O for receive direction\n");
			goto error;
		}
	}

	if (avdtpStart(cfg->hc, cfg->sep)) {
		DPRINTF("START FAILED\n");
		goto error;
	}
	switch (cfg->chmode) {
	case MODE_MONO:
		*pchannels = 1;
		break;
	default:
		*pchannels = 2;
		break;
	}
	return (0);

error:
	if (cfg->hc > 0) {
		close(cfg->hc);
		cfg->hc = -1;
	}
	if (cfg->fd > 0) {
		close(cfg->fd);
		cfg->fd = -1;
	}
	return (-1);
}

static void
bt_init_cfg(struct bt_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));
}

static int
bt_rec_open(struct voss_backend *pbe, const char *devname, int samplerate,
    int bufsize, int *pchannels, int *pformat)
{
	struct bt_config *cfg = pbe->arg;
	int retval;

	bt_init_cfg(cfg);

	retval = bt_open(pbe, devname, samplerate, bufsize, pchannels, pformat,
	    cfg, SDP_SERVICE_CLASS_AUDIO_SOURCE, 0);
	if (retval != 0)
		return (retval);
	return (0);
}

static int
bt_play_open(struct voss_backend *pbe, const char *devname, int samplerate,
    int bufsize, int *pchannels, int *pformat)
{
	struct bt_config *cfg = pbe->arg;
	int retval;

	bt_init_cfg(cfg);

	retval = bt_open(pbe, devname, samplerate, bufsize, pchannels, pformat,
	    cfg, SDP_SERVICE_CLASS_AUDIO_SINK, 1);
	if (retval != 0)
		return (retval);

	/* setup codec */
	switch (cfg->codec) {
	case CODEC_SBC:
		cfg->handle.sbc_enc =
		    malloc(sizeof(*cfg->handle.sbc_enc));
		if (cfg->handle.sbc_enc == NULL)
			return (-1);
		memset(cfg->handle.sbc_enc, 0, sizeof(*cfg->handle.sbc_enc));
		break;
#ifdef HAVE_LIBAV
	case CODEC_AAC:
		cfg->handle.av.codec = __DECONST(AVCodec *,
		    avcodec_find_encoder_by_name("aac"));
		if (cfg->handle.av.codec == NULL) {
			DPRINTF("Codec AAC encoder not found\n");
			goto av_error_0;
		}
		cfg->handle.av.format = avformat_alloc_context();
		if (cfg->handle.av.format == NULL) {
			DPRINTF("Could not allocate format context\n");
			goto av_error_0;
		}
		cfg->handle.av.format->oformat =
		    av_guess_format("latm", NULL, NULL);
		if (cfg->handle.av.format->oformat == NULL) {
			DPRINTF("Could not guess output format\n");
			goto av_error_1;
		}
		cfg->handle.av.stream = avformat_new_stream(
		    cfg->handle.av.format, cfg->handle.av.codec);

		if (cfg->handle.av.stream == NULL) {
			DPRINTF("Could not create new stream\n");
			goto av_error_1;
		}
		cfg->handle.av.context = avcodec_alloc_context3(cfg->handle.av.codec);
		if (cfg->handle.av.context == NULL) {
			DPRINTF("Could not allocate audio context\n");
			goto av_error_1;
		}
		/*avcodec_get_context_defaults3(cfg->handle.av.context,*/
		    /*cfg->handle.av.codec);*/

		cfg->handle.av.context->bit_rate = 128000;
		cfg->handle.av.context->sample_fmt = AV_SAMPLE_FMT_FLTP;
		cfg->handle.av.context->sample_rate = samplerate;
		switch (*pchannels) {
		case 1:
			cfg->handle.av.context->ch_layout = *(AVChannelLayout *)AV_CH_LAYOUT_MONO;
			break;
		default:
			cfg->handle.av.context->ch_layout = *(AVChannelLayout *)AV_CH_LAYOUT_STEREO;
			break;
		}

		cfg->handle.av.context->profile = FF_PROFILE_AAC_LOW;
		if (1) {
			AVDictionary *opts = NULL;

			av_dict_set(&opts, "strict", "-2", 0);
			av_dict_set_int(&opts, "latm", 1, 0);

			if (avcodec_open2(cfg->handle.av.context,
			    cfg->handle.av.codec, &opts) < 0) {
				av_dict_free(&opts);

				DPRINTF("Could not open codec\n");
				goto av_error_1;
			}
			av_dict_free(&opts);
		}
		cfg->handle.av.frame = av_frame_alloc();
		if (cfg->handle.av.frame == NULL) {
			DPRINTF("Could not allocate audio frame\n");
			goto av_error_2;
		}
		cfg->handle.av.frame->nb_samples = cfg->handle.av.context->frame_size;
		cfg->handle.av.frame->format = cfg->handle.av.context->sample_fmt;
		cfg->handle.av.frame->ch_layout = cfg->handle.av.context->ch_layout;
		cfg->rem_in_size = av_samples_get_buffer_size(NULL,
		    cfg->handle.av.context->ch_layout.nb_channels,
		    cfg->handle.av.context->frame_size,
		    cfg->handle.av.context->sample_fmt, 0);

		cfg->rem_in_data = av_malloc(cfg->rem_in_size);
		if (cfg->rem_in_data == NULL) {
			DPRINTF("Could not allocate %u bytes sample buffer\n",
			    (unsigned)cfg->rem_in_size);
			goto av_error_3;
		}
		retval = avcodec_fill_audio_frame(cfg->handle.av.frame,
		    cfg->handle.av.context->ch_layout.nb_channels,
		    cfg->handle.av.context->sample_fmt,
		    cfg->rem_in_data, cfg->rem_in_size, 0);
		if (retval < 0) {
			DPRINTF("Could not setup audio frame\n");
			goto av_error_4;
		}
		break;
av_error_4:
		av_free(cfg->rem_in_data);
av_error_3:
		av_frame_free(&cfg->handle.av.frame);
av_error_2:
		avcodec_close(cfg->handle.av.context);
av_error_1:
		avformat_free_context(cfg->handle.av.format);
		cfg->handle.av.context = NULL;
av_error_0:
		bt_close(pbe);
		return (-1);
#endif
	default:
		bt_close(pbe);
		return (-1);
	}
	return (0);
}

static int
bt_rec_transfer(struct voss_backend *pbe, void *ptr, int len)
{
	return (bt_receive(pbe->arg, ptr, len, 1));
}

static int
bt_play_sbc_transfer(struct voss_backend *pbe, void *ptr, int len)
{
	struct bt_config *cfg = pbe->arg;
	struct sbc_encode *sbc = cfg->handle.sbc_enc;
	int rem_size = 1;
	int old_len = len;
	int err = 0;

	switch (cfg->blocks) {
	case BLOCKS_4:
		sbc->blocks = 4;
		rem_size *= 4;
		break;
	case BLOCKS_8:
		sbc->blocks = 8;
		rem_size *= 8;
		break;
	case BLOCKS_12:
		sbc->blocks = 12;
		rem_size *= 12;
		break;
	default:
		sbc->blocks = 16;
		rem_size *= 16;
		break;
	}

	switch (cfg->bands) {
	case BANDS_4:
		rem_size *= 4;
		sbc->bands = 4;
		break;
	default:
		rem_size *= 8;
		sbc->bands = 8;
		break;
	}

	/* store number of samples per frame */
	sbc->framesamples = rem_size;

	if (cfg->chmode != MODE_MONO) {
		rem_size *= 2;
		sbc->channels = 2;
	} else {
		sbc->channels = 1;
	}

	rem_size *= 2;			/* 16-bit samples */

	while (len > 0) {
		int delta = len;

		if (delta > (int)(rem_size - sbc->rem_len))
			delta = (int)(rem_size - sbc->rem_len);

		/* copy in samples */
		memcpy((char *)sbc->music_data + sbc->rem_len, ptr, delta);

		ptr = (char *)ptr + delta;
		len -= delta;
		sbc->rem_len += delta;

		/* check if buffer is full */
		if ((int)sbc->rem_len == rem_size) {
			struct sbc_header *phdr = (struct sbc_header *)cfg->mtu_data;
			uint32_t pkt_len;
			uint32_t rem;

			if (cfg->chmode == MODE_MONO)
				sbc->channels = 1;
			else
				sbc->channels = 2;

			pkt_len = sbc_encode_frame(cfg);

	retry:
			if (cfg->mtu_offset == 0) {
				phdr->id = 0x80;	/* RTP v2 */
				phdr->id2 = 0x60;	/* payload type 96. */
				phdr->seqnumMSB = (uint8_t)(cfg->mtu_seqnumber >> 8);
				phdr->seqnumLSB = (uint8_t)(cfg->mtu_seqnumber);
				phdr->ts3 = (uint8_t)(cfg->mtu_timestamp >> 24);
				phdr->ts2 = (uint8_t)(cfg->mtu_timestamp >> 16);
				phdr->ts1 = (uint8_t)(cfg->mtu_timestamp >> 8);
				phdr->ts0 = (uint8_t)(cfg->mtu_timestamp);
				phdr->reserved0 = 0x01;
				phdr->numFrames = 0;

				cfg->mtu_seqnumber++;
				cfg->mtu_offset += sizeof(*phdr);
			}
			/* compute bytes left */
			rem = cfg->mtu - cfg->mtu_offset;

			if (phdr->numFrames == 255 || rem < pkt_len) {
				int xlen;

				if (phdr->numFrames == 0)
					return (-1);
				do {
					xlen = write(cfg->fd, cfg->mtu_data, cfg->mtu_offset);
				} while (xlen < 0 && errno == EAGAIN);

				if (xlen < 0)
					return (-1);

				cfg->mtu_offset = 0;
				goto retry;
			}
			memcpy(cfg->mtu_data + cfg->mtu_offset, sbc->data, pkt_len);
			memset(sbc->data, 0, pkt_len);
			cfg->mtu_offset += pkt_len;
			cfg->mtu_timestamp += sbc->framesamples;
			phdr->numFrames++;

			sbc->rem_len = 0;
		}
	}
	if (err == 0)
		return (old_len);
	return (err);
}

#ifdef HAVE_LIBAV
static int
bt_play_aac_transfer(struct voss_backend *pbe, void *ptr, int len)
{
	struct bt_config *cfg = pbe->arg;
	struct aac_header {
		uint8_t	id;
		uint8_t	id2;
		uint8_t	seqnumMSB;
		uint8_t	seqnumLSB;
		uint8_t	ts3;
		uint8_t	ts2;
		uint8_t	ts1;
		uint8_t	ts0;
		uint8_t	sync3;
		uint8_t	sync2;
		uint8_t	sync1;
		uint8_t	sync0;
		uint8_t	fixed[8];
	};
	int old_len = len;
	int err = 0;

	while (len > 0) {
		int delta = len;
		int rem;

		if (delta > (int)(cfg->rem_in_size - cfg->rem_in_len))
			delta = (int)(cfg->rem_in_size - cfg->rem_in_len);

		memcpy(cfg->rem_in_data + cfg->rem_in_len, ptr, delta);

		ptr = (char *)ptr + delta;
		len -= delta;
		cfg->rem_in_len += delta;

		/* check if buffer is full */
		if (cfg->rem_in_len == cfg->rem_in_size) {
			struct aac_header *phdr = (struct aac_header *)cfg->mtu_data;
			AVPacket *pkt;
			uint8_t *pkt_buf;
			int pkt_len;

			pkt = av_packet_alloc();
			err = avcodec_send_frame(cfg->handle.av.context,
			    cfg->handle.av.frame);
			if (err < 0) {
				DPRINTF("Error encoding audio frame\n");
				return (-1);
			}
			phdr->id = 0x80;/* RTP v2 */
			phdr->id2 = 0x60;	/* payload type 96. */
			phdr->seqnumMSB = (uint8_t)(cfg->mtu_seqnumber >> 8);
			phdr->seqnumLSB = (uint8_t)(cfg->mtu_seqnumber);
			phdr->ts3 = (uint8_t)(cfg->mtu_timestamp >> 24);
			phdr->ts2 = (uint8_t)(cfg->mtu_timestamp >> 16);
			phdr->ts1 = (uint8_t)(cfg->mtu_timestamp >> 8);
			phdr->ts0 = (uint8_t)(cfg->mtu_timestamp);
			phdr->sync3 = 0;
			phdr->sync2 = 0;
			phdr->sync1 = 0;
			phdr->sync0 = 0;
			phdr->fixed[0] = 0xfc;
			phdr->fixed[1] = 0x00;
			phdr->fixed[2] = 0x00;
			phdr->fixed[3] = 0xb0;
			phdr->fixed[4] = 0x90;
			phdr->fixed[5] = 0x80;
			phdr->fixed[6] = 0x03;
			phdr->fixed[7] = 0x00;

			cfg->mtu_seqnumber++;
			cfg->mtu_offset = sizeof(*phdr);

			/* compute bytes left */
			rem = cfg->mtu - cfg->mtu_offset;

			if (avio_open_dyn_buf(&cfg->handle.av.format->pb) == 0) {
				static int once = 0;

				if (!once++)
					(void)avformat_write_header(cfg->handle.av.format, NULL);
				av_write_frame(cfg->handle.av.format, pkt);
				av_packet_unref(pkt);
				pkt_len = avio_close_dyn_buf(cfg->handle.av.format->pb, &pkt_buf);
				if (rem < pkt_len)
					DPRINTF("Out of buffer space\n");
				if (pkt_len >= 3 && rem >= pkt_len) {
					int xlen;

					memcpy(cfg->mtu_data + cfg->mtu_offset, pkt_buf + 3, pkt_len - 3);

					av_free(pkt_buf);

					cfg->mtu_offset += pkt_len - 3;
					if (cfg->chmode != MODE_MONO)
						cfg->mtu_timestamp += cfg->rem_in_size / 4;
					else
						cfg->mtu_timestamp += cfg->rem_in_size / 2;
					do {
						xlen = write(cfg->fd, cfg->mtu_data, cfg->mtu_offset);
					} while (xlen < 0 && errno == EAGAIN);

					if (xlen < 0)
						return (-1);
				} else {
					av_free(pkt_buf);
				}
			} else {
				av_packet_unref(pkt);
			}
			/* reset remaining length */
			cfg->rem_in_len = 0;
		}
	}
	if (err == 0)
		return (old_len);
	return (err);
}

#endif

static int
bt_play_transfer(struct voss_backend *pbe, void *ptr, int len)
{
	struct bt_config *cfg = pbe->arg;

	switch (cfg->codec) {
	case CODEC_SBC:
		return (bt_play_sbc_transfer(pbe, ptr, len));
#ifdef HAVE_LIBAV
	case CODEC_AAC:
		return (bt_play_aac_transfer(pbe, ptr, len));
#endif
	default:
		return (-1);
	}
}

static void
bt_rec_delay(struct voss_backend *pbe __unused, int *pdelay)
{
	*pdelay = -1;
}

static void
bt_play_delay(struct voss_backend *pbe __unused, int *pdelay)
{
	/* TODO */
	*pdelay = -1;
}

struct voss_backend voss_backend_bt_rec = {
	.open = bt_rec_open,
	.close = bt_rec_close,
	.transfer = bt_rec_transfer,
	.delay = bt_rec_delay,
	.arg = &bt_rec_cfg,
};

struct voss_backend voss_backend_bt_play = {
	.open = bt_play_open,
	.close = bt_play_close,
	.transfer = bt_play_transfer,
	.delay = bt_play_delay,
	.arg = &bt_play_cfg,
};
