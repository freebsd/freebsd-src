/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_FIREWIRE_FWISOUND_H_
#define _DEV_FIREWIRE_FWISOUND_H_

#include <dev/firewire/fwcam.h>

/* Protocol reverse-engineered by Clemens Ladisch (Linux isight-firmware). */
#define FWISOUND_SPEC_ID	APPLE_FW_OUI
#define FWISOUND_VERSION	0x000010	/* SW_ISIGHT_AUDIO */

#define APPLE_FW_OUI		0x000a27

/* Audio register offsets from audio_base */
#define FWISOUND_REG_AUDIO_ENABLE	0x000	/* bit31 = enable streaming */
#define FWISOUND_REG_DEF_AUDIO_GAIN	0x204
#define FWISOUND_REG_GAIN_RAW_START	0x210
#define FWISOUND_REG_GAIN_RAW_END	0x214
#define FWISOUND_REG_GAIN_DB_START	0x218
#define FWISOUND_REG_GAIN_DB_END	0x21c
#define FWISOUND_REG_SAMPLE_RATE_INQ	0x280
#define FWISOUND_REG_ISO_TX_CONFIG	0x300	/* bits[19:16]=speed, [3:0]=ch*/
#define FWISOUND_REG_SAMPLE_RATE	0x400	/* bit31 = 1 → 48000 Hz */
#define FWISOUND_REG_GAIN		0x500
#define FWISOUND_REG_MUTE		0x504

/* REG_AUDIO_ENABLE / REG_SAMPLE_RATE bit */
#define FWISOUND_AUDIO_ENABLE		(1u << 31)
#define FWISOUND_RATE_48000		(1u << 31)

#define FWISOUND_MAX_SAMPLES	475		/* max samples per ISO packet */
#define FWISOUND_SIGNATURE	0x73676874u	/* "sght" */

/* Apple FireWire audio ISO payload. */
struct fwisound_payload {
	uint32_t	sample_count;		/* samples in this packet */
	uint32_t	signature;		/* 0x73676874 = "sght" */
	uint32_t	sample_total;		/* running total (drop detect)*/
	uint32_t	reserved;
	int16_t		samples[2 * FWISOUND_MAX_SAMPLES]; /* stereo S16BE */
};

/* ISO DMA parameters */
#define FWISOUND_ISO_CHANNEL	1
#define FWISOUND_ISO_NCHUNK	64
#define FWISOUND_ISO_PKTSIZE	2048		/* MCLBYTES */

/* Driver states */
#define FWISOUND_STATE_IDLE	0
#define FWISOUND_STATE_PROBED	1
#define FWISOUND_STATE_STREAMING 2
#define FWISOUND_STATE_DETACHING 3

#ifdef _KERNEL

#include <dev/sound/pcm/sound.h>

struct fwisound_chan {
	struct fwisound_softc	*sc;
	struct pcm_channel	*pcm_chan;
	struct snd_dbuf		*buf;
	struct pcmchan_caps	 caps;
	uint32_t		 fmts[2];
	uint32_t		 hw_ptr;	/* ring write offset (bytes) */
	int			 running;
};

struct fwisound_softc {
	struct firewire_dev_comm fd;	/* MUST BE FIRST */
	struct mtx		 mtx;
	device_t		 dev;
	device_t		 pcm_dev;

	struct fw_device	*fwdev;
	uint16_t		 cmd_hi;	/* always 0xffff */
	uint32_t		 cmd_lo;	/* 0xf0000000|(audio_base<<2) */

	struct task		 probe_task;
	struct task		 start_task;	/* deferred iso_start */
	struct task		 stop_task;	/* deferred iso_stop */

	/* ISO streaming */
	int			 dma_ch;	/* -1 = not open */
	int			 iso_active;	/* iso_input running */
	uint8_t			 iso_channel;

	/* PCM channel back-reference */
	struct fwisound_chan	*chan;

	/* Sample drop detection */
	uint32_t		 sample_total;

	/* Gain range (read from camera during probe) */
	int32_t			 gain_raw_min;
	int32_t			 gain_raw_max;

	int			 state;
	int			 dropped;
};

#define FWISOUND_LOCK(sc)	mtx_lock(&(sc)->mtx)
#define FWISOUND_UNLOCK(sc)	mtx_unlock(&(sc)->mtx)

int	fwisound_iso_start(struct fwisound_softc *);
void	fwisound_iso_stop(struct fwisound_softc *);

#endif /* _KERNEL */

#endif /* _DEV_FIREWIRE_FWISOUND_H_ */
