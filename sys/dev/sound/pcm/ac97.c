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
 * $FreeBSD: src/sys/dev/sound/pcm/ac97.c,v 1.5.2.2 2000/07/19 21:18:46 cg Exp $
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

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
	device_t dev;
	ac97_init *init;
	ac97_read *read;
	ac97_write *write;
	void *devinfo;
	char id[4];
	char rev;
	unsigned caps, se, extcaps, extid, extstat, noext:1;
	struct ac97mixtable_entry mix[32];
};

struct ac97_codecid {
	u_int32_t id, noext:1;
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
	{ 0x414b4d00, 1, "Asahi Kasei AK4540 rev 0" },
	{ 0x43525900, 0, "Cirrus Logic CS4297" 	},
	{ 0x83847600, 0, "SigmaTel STAC????" 	},
	{ 0x83847604, 0, "SigmaTel STAC9701/3/4/5" },
	{ 0x83847605, 0, "SigmaTel STAC9704" 	},
	{ 0x83847608, 0, "SigmaTel STAC9708" 	},
	{ 0x83847609, 0, "SigmaTel STAC9721" 	},
	{ 0x414b4d01, 1, "Asahi Kasei AK4540 rev 1" },
	{ 0, 	      0, NULL			}
};

static char *ac97enhancement[] = {
	"no 3D Stereo Enhancement",
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

static char *ac97extfeature[] = {
	"variable rate PCM",
	"double rate PCM",
	"reserved 1",
	"variable rate mic",
	"reserved 2",
	"reserved 3",
	"center DAC",
	"surround DAC",
	"LFE DAC",
	"AMAP",
	"reserved 4",
	"reserved 5",
	"reserved 6",
	"reserved 7",
};

static u_int16_t
rdcd(struct ac97_info *codec, int reg)
{
	return codec->read(codec->devinfo, reg);
}

static void
wrcd(struct ac97_info *codec, int reg, u_int16_t val)
{
	codec->write(codec->devinfo, reg, val);
}

int
ac97_setrate(struct ac97_info *codec, int which, int rate)
{
	u_int16_t v;

	switch(which) {
	case AC97_REGEXT_FDACRATE:
	case AC97_REGEXT_SDACRATE:
	case AC97_REGEXT_LDACRATE:
	case AC97_REGEXT_LADCRATE:
	case AC97_REGEXT_MADCRATE:
		break;

	default:
		return -1;
	}

	if (rate != 0) {
		v = rate;
		if (codec->extstat & AC97_EXTCAP_DRA)
			v >>= 1;
		wrcd(codec, which, v);
	}
	v = rdcd(codec, which);
	if (codec->extstat & AC97_EXTCAP_DRA)
		v <<= 1;
	return v;
}

int
ac97_setextmode(struct ac97_info *codec, u_int16_t mode)
{
	mode &= AC97_EXTCAPS;
	if ((mode & ~codec->extcaps) != 0)
		return -1;
	wrcd(codec, AC97_REGEXT_STAT, mode);
	codec->extstat = rdcd(codec, AC97_REGEXT_STAT) & AC97_EXTCAPS;
	return (mode == codec->extstat)? 0 : -1;
}

static int
ac97_setrecsrc(struct ac97_info *codec, int channel)
{
	struct ac97mixtable_entry *e = &codec->mix[channel];
	if (e->recidx > 0) {
		int val = e->recidx - 1;
		val |= val << 8;
		wrcd(codec, AC97_REG_RECSEL, val);
		return 0;
	} else
		return -1;
}

static int
ac97_setmixer(struct ac97_info *codec, unsigned channel, unsigned left, unsigned right)
{
	struct ac97mixtable_entry *e = &codec->mix[channel];
	if (e->reg != 0) {
		int max, val, reg = (e->reg >= 0)? e->reg : -e->reg;

		if (!e->stereo)
			right = left;
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
				int cur = rdcd(codec, e->reg);
				val |= cur & ~(max << e->ofs);
			}
		}
		if (left == 0 && right == 0 && e->mute == 1)
			val = AC97_MUTE;
		wrcd(codec, reg, val);
		return left | (right << 8);
	} else
		return -1;
}

#if 0
static int
ac97_getmixer(struct ac97_info *codec, int channel)
{
	struct ac97mixtable_entry *e = &codec->mix[channel];
	if (channel < SOUND_MIXER_NRDEVICES && e->reg != 0) {
		int max, val, volume;

		max = (1 << e->bits) - 1;
		val = rdcd(code, e->reg);
		if (val == AC97_MUTE && e->mute == 1)
			volume = 0;
		else {
			if (e->stereo == 0) val >>= e->ofs;
			val &= max;
			volume = (val * 100) / max;
			if (e->reg > 0) volume = 100 - volume;
		}
		return volume;
	} else
		return -1;
}
#endif

static unsigned
ac97_initmixer(struct ac97_info *codec)
{
	unsigned i, j;
	u_int32_t id;

	for (i = 0; i < 32; i++)
		codec->mix[i] = ac97mixtable_default[i];

	if (codec->init) {
		if (codec->init(codec->devinfo)) {
			device_printf(codec->dev, "ac97 codec init failed\n");
			return ENODEV;
		}
	}
	wrcd(codec, AC97_REG_POWER, 0);
	wrcd(codec, AC97_REG_RESET, 0);
	DELAY(100000);

	i = rdcd(codec, AC97_REG_RESET);
	codec->caps = i & 0x03ff;
	codec->se =  (i & 0x7c00) >> 10;

	id = (rdcd(codec, AC97_REG_ID1) << 16) | rdcd(codec, AC97_REG_ID2);
	codec->rev = id & 0x000000ff;
	if (id == 0 || id == 0xffffffff) {
		device_printf(codec->dev, "ac97 codec invalid or not present (id == %x)\n", id);
		return ENODEV;
	}

	for (i = 0; ac97codecid[i].id; i++)
		if (ac97codecid[i].id == id)
			codec->noext = 1;

	if (!codec->noext) {
		i = rdcd(codec, AC97_REGEXT_ID);
		codec->extcaps = i & 0x3fff;
		codec->extid =  (i & 0xc000) >> 14;
		codec->extstat = rdcd(codec, AC97_REGEXT_STAT) & AC97_EXTCAPS;
	} else {
		codec->extcaps = 0;
		codec->extid = 0;
		codec->extstat = 0;
	}

	wrcd(codec, AC97_MIX_MASTER, 0x20);
	if ((rdcd(codec, AC97_MIX_MASTER) & 0x20) == 0x20)
		codec->mix[SOUND_MIXER_VOLUME].bits++;
	wrcd(codec, AC97_MIX_MASTER, 0x00);

	if (bootverbose) {
		device_printf(codec->dev, "ac97 codec id 0x%08x", id);
		for (i = 0; ac97codecid[i].id; i++)
			if (ac97codecid[i].id == id)
				printf(" (%s)", ac97codecid[i].name);
		printf("\n");
		device_printf(codec->dev, "ac97 codec features ");
		for (i = j = 0; i < 10; i++)
			if (codec->caps & (1 << i))
				printf("%s%s", j++? ", " : "", ac97feature[i]);
		printf("%s%d bit master volume", j++? ", " : "", codec->mix[SOUND_MIXER_VOLUME].bits);
		printf("%s%s\n", j? ", " : "", ac97enhancement[codec->se]);

		if (codec->extcaps != 0 || codec->extid) {
			device_printf(codec->dev, "ac97 %s codec",
				      codec->extid? "secondary" : "primary");
			if (codec->extcaps)
				printf(" extended features ");
			for (i = j = 0; i < 14; i++)
				if (codec->extcaps & (1 << i))
					printf("%s%s", j++? ", " : "", ac97extfeature[i]);
			printf("\n");
		}
	}

	if ((rdcd(codec, AC97_REG_POWER) & 2) == 0)
		device_printf(codec->dev, "ac97 codec reports dac not ready\n");
	return 0;
}

struct ac97_info *
ac97_create(device_t dev, void *devinfo, ac97_init *init, ac97_read *rd, ac97_write *wr)
{
	struct ac97_info *codec;

	codec = (struct ac97_info *)malloc(sizeof *codec, M_DEVBUF, M_NOWAIT);
	if (codec != NULL) {
		codec->dev = dev;
		codec->init = init;
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
	if (codec == NULL)
		return -1;
	if (ac97_initmixer(codec))
		return -1;
	mix_setdevs(m, ac97mixdevs | ((codec->caps & 4)? SOUND_MASK_BASS | SOUND_MASK_TREBLE : 0));
	mix_setrecdevs(m, ac97recdevs);
	return 0;
}

static int
ac97mix_set(snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
	struct ac97_info *codec = mix_getdevinfo(m);
	if (codec == NULL)
		return -1;
	return ac97_setmixer(codec, dev, left, right);
}

static int
ac97mix_setrecsrc(snd_mixer *m, u_int32_t src)
{
	int i;
	struct ac97_info *codec = mix_getdevinfo(m);
	if (codec == NULL)
		return -1;
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if ((src & (1 << i)) != 0)
			break;
	return (ac97_setrecsrc(codec, i) == 0)? 1 << i : -1;
}

snd_mixer ac97_mixer = {
	"AC97 mixer",
	ac97mix_init,
	ac97mix_set,
	ac97mix_setrecsrc,
};

