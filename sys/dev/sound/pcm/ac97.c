/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

#define AC97_MUTE	0x8000

#define AC97_REG_RESET	0x00
#define AC97_MIX_MASTER	0x02
#define AC97_MIX_PHONES	0x04
#define AC97_MIX_MONO 	0x06
#define AC97_MIX_TONE	0x08
#define AC97_MIX_BEEP	0x0a
#define AC97_MIX_PHONE	0x0c
#define AC97_MIX_MIC	0x0e
#define AC97_MIX_LINE	0x10
#define AC97_MIX_CD	0x12
#define AC97_MIX_VIDEO	0x14
#define AC97_MIX_AUX	0x16
#define AC97_MIX_PCM	0x18
#define AC97_REG_RECSEL	0x1a
#define AC97_MIX_RGAIN	0x1c
#define AC97_MIX_MGAIN	0x1e
#define AC97_REG_GEN	0x20
#define AC97_REG_3D	0x22
#define AC97_REG_POWER	0x26
#define AC97_REG_ID1	0x7c
#define AC97_REG_ID2	0x7e

struct ac97mixtable_entry {
	int		reg:8;
	unsigned	bits:4;
	unsigned	ofs:4;
	unsigned	stereo:1;
	unsigned	mute:1;
	unsigned	recidx:4;
	unsigned        mask:1;
};

struct ac97_info {
	ac97_read *read;
	ac97_write *write;
	void *devinfo;
	char id[4];
	char rev;
	unsigned caps, se;
	struct ac97mixtable_entry mix[32];
};

struct ac97_codecid {
	u_int32_t id;
	char *name;
};

static const struct ac97mixtable_entry ac97mixtable_default[32] = {
	[SOUND_MIXER_VOLUME]	= { AC97_MIX_MASTER, 	5, 0, 1, 1, 6, 0 },
	[SOUND_MIXER_BASS]	= { AC97_MIX_TONE, 	4, 8, 0, 0, 0, 1 },
	[SOUND_MIXER_TREBLE]	= { AC97_MIX_TONE, 	4, 0, 0, 0, 0, 1 },
	[SOUND_MIXER_PCM]	= { AC97_MIX_PCM, 	5, 0, 1, 1, 0, 0 },
	[SOUND_MIXER_SPEAKER]	= { AC97_MIX_BEEP, 	4, 1, 0, 1, 0, 0 },
	[SOUND_MIXER_LINE]	= { AC97_MIX_LINE, 	5, 0, 1, 1, 5, 0 },
	[SOUND_MIXER_MIC] 	= { AC97_MIX_MIC, 	5, 0, 0, 1, 1, 0 },
	[SOUND_MIXER_CD]	= { AC97_MIX_CD, 	5, 0, 1, 1, 2, 0 },
	[SOUND_MIXER_LINE1]	= { AC97_MIX_AUX, 	5, 0, 1, 1, 4, 0 },
	[SOUND_MIXER_VIDEO]	= { AC97_MIX_VIDEO, 	5, 0, 1, 1, 3, 0 },
	[SOUND_MIXER_RECLEV]	= { -AC97_MIX_RGAIN, 	4, 0, 1, 1, 0, 0 }
};

static const unsigned ac97mixdevs =
	SOUND_MASK_VOLUME |
	SOUND_MASK_PCM | SOUND_MASK_SPEAKER | SOUND_MASK_LINE |
	SOUND_MASK_MIC | SOUND_MASK_CD | SOUND_MASK_LINE1 |
	SOUND_MASK_VIDEO | SOUND_MASK_RECLEV;

static const unsigned ac97recdevs =
	SOUND_MASK_VOLUME | SOUND_MASK_LINE | SOUND_MASK_MIC |
	SOUND_MASK_CD | SOUND_MASK_LINE1 | SOUND_MASK_VIDEO;

static struct ac97_codecid ac97codecid[] = {
	{ 0x414B4D00, "Asahi Kasei AK4540" 	},
	{ 0x43525900, "Cirrus Logic CS4297" 	},
	{ 0x83847600, "SigmaTel STAC????" 	},
	{ 0x83847604, "SigmaTel STAC9701/3/4/5" },
	{ 0x83847605, "SigmaTel STAC9704" 	},
	{ 0x83847608, "SigmaTel STAC9708" 	},
	{ 0x83847609, "SigmaTel STAC9721" 	},
	{ 0, 	      NULL			}
};

static char *ac97enhancement[] = {
	"",
	"Analog Devices Phat Stereo",
	"Creative Stereo Enhancement",
	"National Semi 3D Stereo Enhancement",
	"Yamaha Ymersion",
	"BBE 3D Stereo Enhancement",
	"Crystal Semi 3D Stereo Enhancement",
	"Qsound QXpander",
	"Spatializer 3D Stereo Enhancement",
	"SRS 3D Stereo Enhancement",
	"Platform Tech 3D Stereo Enhancement",
	"AKM 3D Audio",
	"Aureal Stereo Enhancement",
	"Aztech 3D Enhancement",
	"Binaura 3D Audio Enhancement",
	"ESS Technology Stereo Enhancement",
	"Harman International VMAx",
	"Nvidea 3D Stereo Enhancement",
	"Philips Incredible Sound",
	"Texas Instruments 3D Stereo Enhancement",
	"VLSI Technology 3D Stereo Enhancement",
	"TriTech 3D Stereo Enhancement",
	"Realtek 3D Stereo Enhancement",
	"Samsung 3D Stereo Enhancement",
	"Wolfson Microelectronics 3D Enhancement",
	"Delta Integration 3D Enhancement",
	"SigmaTel 3D Enhancement",
	"Reserved 27",
	"Rockwell 3D Stereo Enhancement",
	"Reserved 29",
	"Reserved 30",
	"Reserved 31"
};

static char *ac97feature[] = {
	"mic channel",
	"reserved",
	"tone",
	"simulated stereo",
	"headphone",
	"bass boost",
	"18 bit DAC",
	"20 bit DAC",
	"18 bit ADC",
	"20 bit ADC"
};

static int
ac97_setrecsrc(struct ac97_info *codec, int channel)
{
	struct ac97mixtable_entry *e = &codec->mix[channel];
	if (e->recidx > 0) {
		int val = e->recidx - 1;
		val |= val << 8;
		codec->write(codec->devinfo, AC97_REG_RECSEL, val);
		return 0;
	} else return -1;
}

static int
ac97_setmixer(struct ac97_info *codec, unsigned channel, unsigned left, unsigned right)
{
	struct ac97mixtable_entry *e = &codec->mix[channel];
	if (e->reg != 0) {
		int max, val;

		if (!e->stereo) right = left;
		if (e->reg > 0) {
			left = 100 - left;
			right = 100 - right;
		}

		max = (1 << e->bits) - 1;
		left = (left * max) / 100;
		right = (right * max) / 100;

		val = (left << 8) | right;

		left = (left * 100) / max;
		right = (right * 100) / max;

		if (e->reg > 0) {
			left = 100 - left;
			right = 100 - right;
		}

		if (!e->stereo) {
			val &= max;
			val <<= e->ofs;
			if (e->mask) {
				int cur = codec->read(codec->devinfo, e->reg);
				val |= cur & ~(max << e->ofs);
			}
		}
		if (left == 0 && right == 0 && e->mute == 1) val = AC97_MUTE;
		codec->write(codec->devinfo, abs(e->reg), val);
		return left | (right << 8);
	} else return -1;
}

#if 0
static int
ac97_getmixer(struct ac97_info *codec, int channel)
{
	struct ac97mixtable_entry *e = &codec->mix[channel];
	if (channel < SOUND_MIXER_NRDEVICES && e->reg != 0) {
		int max, val, volume;

		max = (1 << e->bits) - 1;
		val = codec->read(codec->devinfo, e->reg);
		if (val == AC97_MUTE && e->mute == 1) volume = 0;
		else {
			if (e->stereo == 0) val >>= e->ofs;
			val &= max;
			volume = (val * 100) / max;
			if (e->reg > 0) volume = 100 - volume;
		}
		return volume;
	} else return -1;
}
#endif

static unsigned
ac97_init(struct ac97_info *codec)
{
	unsigned i, j;
	u_int32_t id;

	for (i = 0; i < 32; i++) codec->mix[i] = ac97mixtable_default[i];

	codec->write(codec->devinfo, AC97_REG_POWER, 0);
	codec->write(codec->devinfo, AC97_REG_RESET, 0);
	DELAY(10000);

	i = codec->read(codec->devinfo, AC97_REG_RESET);
	codec->caps = i & 0x03ff;
	codec->se =  (i & 0x7c00) >> 10;

	id = (codec->read(codec->devinfo, AC97_REG_ID1) << 16) |
	      codec->read(codec->devinfo, AC97_REG_ID2);
	codec->rev = id & 0x000000ff;

	codec->write(codec->devinfo, AC97_MIX_MASTER, 0x20);
	if ((codec->read(codec->devinfo, AC97_MIX_MASTER) & 0x20) == 0x20)
		codec->mix[SOUND_MIXER_VOLUME].bits++;
	codec->write(codec->devinfo, AC97_MIX_MASTER, 0x00);

	if (bootverbose) {
		printf("ac97: codec id 0x%8x", id);
		for (i = 0; ac97codecid[i].id; i++) {
			if (ac97codecid[i].id == id) printf(" (%s)", ac97codecid[i].name);
		}
		printf("\nac97: codec features ");
		for (i = j = 0; i < 10; i++) {
			if (codec->caps & (1 << i)) {
				printf("%s%s", j? ", " : "", ac97feature[i]);
				j++;
			}
		}
		printf("%s%d bit master volume", j? ", " : "", codec->mix[SOUND_MIXER_VOLUME].bits);
		printf("%s%s\n", j? ", " : "", ac97enhancement[codec->se]);
	}

	if ((codec->read(codec->devinfo, AC97_REG_POWER) & 2) == 0)
		printf("ac97: dac not ready\n");
	return 0;
}

struct ac97_info *
ac97_create(void *devinfo, ac97_read *rd, ac97_write *wr)
{
	struct ac97_info *codec;

	codec = (struct ac97_info *)malloc(sizeof *codec, M_DEVBUF, M_NOWAIT);
	if (codec != NULL) {
		codec->read = rd;
		codec->write = wr;
		codec->devinfo = devinfo;
	}
	return codec;
}

static int
ac97mix_init(snd_mixer *m)
{
	struct ac97_info *codec = mix_getdevinfo(m);
	if (codec == NULL) return -1;
	ac97_init(codec);
	mix_setdevs(m, ac97mixdevs | ((codec->caps & 4)? SOUND_MASK_BASS | SOUND_MASK_TREBLE : 0));
	mix_setrecdevs(m, ac97recdevs);
	return 0;
}

static int
ac97mix_set(snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
	struct ac97_info *codec = mix_getdevinfo(m);
	if (codec == NULL) return -1;
	return ac97_setmixer(codec, dev, left, right);
}

static int
ac97mix_setrecsrc(snd_mixer *m, u_int32_t src)
{
	int i;
	struct ac97_info *codec = mix_getdevinfo(m);
	if (codec == NULL) return -1;
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if ((src & (1 << i)) != 0) break;
	return (ac97_setrecsrc(codec, i) == 0)? 1 << i : -1;
}

snd_mixer ac97_mixer = {
	"AC97 mixer",
	ac97mix_init,
	ac97mix_set,
	ac97mix_setrecsrc,
};

