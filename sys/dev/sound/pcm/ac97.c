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
 */

#include <dev/sound/pcm/sound.h>
#include <dev/sound/pcm/ac97.h>

#include "mixer_if.h"

SND_DECLARE_FILE("$FreeBSD$");

MALLOC_DEFINE(M_AC97, "ac97", "ac97 codec");

struct ac97mixtable_entry {
	int		reg:8;
	unsigned	bits:4;
	unsigned	ofs:4;
	unsigned	stereo:1;
	unsigned	mute:1;
	unsigned	recidx:4;
	unsigned        mask:1;
	unsigned	enable:1;
};

#define AC97_NAMELEN	16
struct ac97_info {
	kobj_t methods;
	device_t dev;
	void *devinfo;
	char *id;
	char rev;
	unsigned count, caps, se, extcaps, extid, extstat, noext:1;
	u_int32_t flags;
	struct ac97mixtable_entry mix[32];
	char name[AC97_NAMELEN];
	struct mtx *lock;
};

struct ac97_codecid {
	u_int32_t id, noext:1;
	char *name;
};

static const struct ac97mixtable_entry ac97mixtable_default[32] = {
	[SOUND_MIXER_VOLUME]	= { AC97_MIX_MASTER, 	5, 0, 1, 1, 6, 0, 1 },
	[SOUND_MIXER_MONITOR]	= { AC97_MIX_AUXOUT, 	5, 0, 1, 1, 0, 0, 0 },
	[SOUND_MIXER_PHONEOUT]	= { AC97_MIX_MONO, 	5, 0, 0, 1, 7, 0, 0 },
	[SOUND_MIXER_BASS]	= { AC97_MIX_TONE, 	4, 8, 0, 0, 0, 1, 0 },
	[SOUND_MIXER_TREBLE]	= { AC97_MIX_TONE, 	4, 0, 0, 0, 0, 1, 0 },
	[SOUND_MIXER_PCM]	= { AC97_MIX_PCM, 	5, 0, 1, 1, 0, 0, 1 },
	[SOUND_MIXER_SPEAKER]	= { AC97_MIX_BEEP, 	4, 1, 0, 1, 0, 0, 0 },
	[SOUND_MIXER_LINE]	= { AC97_MIX_LINE, 	5, 0, 1, 1, 5, 0, 1 },
	[SOUND_MIXER_PHONEIN]	= { AC97_MIX_PHONE, 	5, 0, 0, 1, 8, 0, 0 },
	[SOUND_MIXER_MIC] 	= { AC97_MIX_MIC, 	5, 0, 0, 1, 1, 0, 1 },
	[SOUND_MIXER_CD]	= { AC97_MIX_CD, 	5, 0, 1, 1, 2, 0, 1 },
	[SOUND_MIXER_LINE1]	= { AC97_MIX_AUX, 	5, 0, 1, 1, 4, 0, 0 },
	[SOUND_MIXER_VIDEO]	= { AC97_MIX_VIDEO, 	5, 0, 1, 1, 3, 0, 0 },
	[SOUND_MIXER_RECLEV]	= { -AC97_MIX_RGAIN, 	4, 0, 1, 1, 0, 0, 1 }
};

static struct ac97_codecid ac97codecid[] = {
	{ 0x41445303, 0, "Analog Devices AD1819" },
	{ 0x41445340, 0, "Analog Devices AD1881" },
	{ 0x41445348, 0, "Analog Devices AD1881A" },
	{ 0x41445360, 0, "Analog Devices AD1885" },
	{ 0x414b4d00, 1, "Asahi Kasei AK4540" },
	{ 0x414b4d01, 1, "Asahi Kasei AK4542" },
	{ 0x414b4d02, 1, "Asahi Kasei AK4543" },
	{ 0x414c4710, 0, "Avance Logic ALC200/200P" },
	{ 0x43525900, 0, "Cirrus Logic CS4297" },
	{ 0x43525903, 0, "Cirrus Logic CS4297" },
	{ 0x43525913, 0, "Cirrus Logic CS4297A" },
	{ 0x43525914, 0, "Cirrus Logic CS4297B" },
	{ 0x43525923, 0, "Cirrus Logic CS4294C" },
	{ 0x4352592b, 0, "Cirrus Logic CS4298C" },
	{ 0x43525931, 0, "Cirrus Logic CS4299A" },
	{ 0x43525933, 0, "Cirrus Logic CS4299C" },
	{ 0x43525934, 0, "Cirrus Logic CS4299D" },
	{ 0x43525941, 0, "Cirrus Logic CS4201A" },
	{ 0x43525951, 0, "Cirrus Logic CS4205A" },
	{ 0x43525961, 0, "Cirrus Logic CS4291A" },
	{ 0x45838308, 0, "ESS Technology ES1921" },
	{ 0x49434511, 0, "ICEnsemble ICE1232" },
	{ 0x4e534331, 0, "National Semiconductor LM4549" },
	{ 0x83847600, 0, "SigmaTel STAC9700/9783/9784" },
	{ 0x83847604, 0, "SigmaTel STAC9701/9703/9704/9705" },
	{ 0x83847605, 0, "SigmaTel STAC9704" },
	{ 0x83847608, 0, "SigmaTel STAC9708/9711" },
	{ 0x83847609, 0, "SigmaTel STAC9721/9723" },
	{ 0x83847644, 0, "SigmaTel STAC9744" },
	{ 0x83847656, 0, "SigmaTel STAC9756/9757" },
	{ 0x53494c22, 0, "Silicon Laboratory Si3036" },
	{ 0x53494c23, 0, "Silicon Laboratory Si3038" },
	{ 0x54524103, 0, "TriTech TR?????" },
	{ 0x54524106, 0, "TriTech TR28026" },
	{ 0x54524108, 0, "TriTech TR28028" },
	{ 0x54524123, 0, "TriTech TR28602" },
	{ 0x574d4c00, 0, "Wolfson WM9701A" },
	{ 0x574d4c03, 0, "Wolfson WM9703/9704" },
	{ 0x574d4c04, 0, "Wolfson WM9704 (quad)" },
	{ 0, 0, NULL }
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
	return AC97_READ(codec->methods, codec->devinfo, reg);
}

static void
wrcd(struct ac97_info *codec, int reg, u_int16_t val)
{
	AC97_WRITE(codec->methods, codec->devinfo, reg, val);
}

static void
ac97_reset(struct ac97_info *codec)
{
	u_int32_t i, ps;
	wrcd(codec, AC97_REG_RESET, 0);
	for (i = 0; i < 500; i++) {
		ps = rdcd(codec, AC97_REG_POWER) & AC97_POWER_STATUS;
		if (ps == AC97_POWER_STATUS)
			return;
		DELAY(1000);
	}
	device_printf(codec->dev, "AC97 reset timed out.");
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

	snd_mtxlock(codec->lock);
	if (rate != 0) {
		v = rate;
		if (codec->extstat & AC97_EXTCAP_DRA)
			v >>= 1;
		wrcd(codec, which, v);
	}
	v = rdcd(codec, which);
	if (codec->extstat & AC97_EXTCAP_DRA)
		v <<= 1;
	snd_mtxunlock(codec->lock);
	return v;
}

int
ac97_setextmode(struct ac97_info *codec, u_int16_t mode)
{
	mode &= AC97_EXTCAPS;
	if ((mode & ~codec->extcaps) != 0) {
		device_printf(codec->dev, "ac97 invalid mode set 0x%04x\n",
			      mode);
		return -1;
	}
	snd_mtxlock(codec->lock);
	wrcd(codec, AC97_REGEXT_STAT, mode);
	codec->extstat = rdcd(codec, AC97_REGEXT_STAT) & AC97_EXTCAPS;
	snd_mtxunlock(codec->lock);
	return (mode == codec->extstat)? 0 : -1;
}

u_int16_t
ac97_getextmode(struct ac97_info *codec)
{
	return codec->extstat;
}

u_int16_t
ac97_getextcaps(struct ac97_info *codec)
{
	return codec->extcaps;
}

u_int16_t
ac97_getcaps(struct ac97_info *codec)
{
	return codec->caps;
}

static int
ac97_setrecsrc(struct ac97_info *codec, int channel)
{
	struct ac97mixtable_entry *e = &codec->mix[channel];

	if (e->recidx > 0) {
		int val = e->recidx - 1;
		val |= val << 8;
		snd_mtxlock(codec->lock);
		wrcd(codec, AC97_REG_RECSEL, val);
		snd_mtxunlock(codec->lock);
		return 0;
	} else
		return -1;
}

static int
ac97_setmixer(struct ac97_info *codec, unsigned channel, unsigned left, unsigned right)
{
	struct ac97mixtable_entry *e = &codec->mix[channel];

	if (e->reg && e->enable && e->bits) {
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
		snd_mtxlock(codec->lock);
		wrcd(codec, reg, val);
		snd_mtxunlock(codec->lock);
		return left | (right << 8);
	} else {
		/* printf("ac97_setmixer: reg=%d, bits=%d, enable=%d\n", e->reg, e->bits, e->enable); */
		return -1;
	}
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

static void
ac97_fix_auxout(struct ac97_info *codec)
{
	/* Determine what AUXOUT really means, it can be:
	 *
	 * 1. Headphone out.
	 * 2. 4-Channel Out
	 * 3. True line level out (effectively master volume).
	 *
	 * See Sections 5.2.1 and 5.27 for AUX_OUT Options in AC97r2.{2,3}.
	 */
	if (codec->caps & AC97_CAP_HEADPHONE) {
		/* XXX We should probably check the AUX_OUT initial value.
		 * Leave AC97_MIX_AUXOUT - SOUND_MIXER_MONITOR relationship */
		return;
	} else if (codec->extcaps & AC97_EXTCAP_SDAC &&
		   rdcd(codec, AC97_MIXEXT_SURROUND) == 0x8080) {
		/* 4-Channel Out, add an additional gain setting. */
		codec->mix[SOUND_MIXER_OGAIN] = codec->mix[SOUND_MIXER_MONITOR];
	} else {
		/* Master volume is/maybe fixed in h/w, not sufficiently
		 * clear in spec to blat SOUND_MIXER_MASTER. */
		codec->mix[SOUND_MIXER_OGAIN] = codec->mix[SOUND_MIXER_MONITOR];
	}
	/* Blat monitor, inappropriate label if we get here */
	bzero(&codec->mix[SOUND_MIXER_MONITOR],
	      sizeof(codec->mix[SOUND_MIXER_MONITOR]));
}

static unsigned
ac97_initmixer(struct ac97_info *codec)
{
	unsigned i, j, k, old;
	u_int32_t id;

	snd_mtxlock(codec->lock);
	codec->count = AC97_INIT(codec->methods, codec->devinfo);
	if (codec->count == 0) {
		device_printf(codec->dev, "ac97 codec init failed\n");
		snd_mtxunlock(codec->lock);
		return ENODEV;
	}

	wrcd(codec, AC97_REG_POWER, (codec->flags & AC97_F_EAPD_INV)? 0x8000 : 0x0000);
	ac97_reset(codec);
	wrcd(codec, AC97_REG_POWER, (codec->flags & AC97_F_EAPD_INV)? 0x8000 : 0x0000);

	i = rdcd(codec, AC97_REG_RESET);
	codec->caps = i & 0x03ff;
	codec->se =  (i & 0x7c00) >> 10;

	id = (rdcd(codec, AC97_REG_ID1) << 16) | rdcd(codec, AC97_REG_ID2);
	codec->rev = id & 0x000000ff;
	if (id == 0 || id == 0xffffffff) {
		device_printf(codec->dev, "ac97 codec invalid or not present (id == %x)\n", id);
		snd_mtxunlock(codec->lock);
		return ENODEV;
	}

	codec->noext = 0;
	codec->id = NULL;
	for (i = 0; ac97codecid[i].id; i++) {
		if (ac97codecid[i].id == id) {
			codec->id = ac97codecid[i].name;
			codec->noext = ac97codecid[i].noext;
		}
	}

	codec->extcaps = 0;
	codec->extid = 0;
	codec->extstat = 0;
	if (!codec->noext) {
		i = rdcd(codec, AC97_REGEXT_ID);
		if (i != 0xffff) {
			codec->extcaps = i & 0x3fff;
			codec->extid =  (i & 0xc000) >> 14;
			codec->extstat = rdcd(codec, AC97_REGEXT_STAT) & AC97_EXTCAPS;
		}
	}

	for (i = 0; i < 32; i++) {
		codec->mix[i] = ac97mixtable_default[i];
	}
	ac97_fix_auxout(codec);

	for (i = 0; i < 32; i++) {
		k = codec->noext? codec->mix[i].enable : 1;
		if (k && (codec->mix[i].reg > 0)) {
			old = rdcd(codec, codec->mix[i].reg);
			wrcd(codec, codec->mix[i].reg, 0x3f);
			j = rdcd(codec, codec->mix[i].reg);
			wrcd(codec, codec->mix[i].reg, old);
			codec->mix[i].enable = (j != 0 && j != old)? 1 : 0;
			for (k = 1; j & (1 << k); k++);
			codec->mix[i].bits = j? k - codec->mix[i].ofs : 0;
		}
		/* printf("mixch %d, en=%d, b=%d\n", i, codec->mix[i].enable, codec->mix[i].bits); */
	}

	if (bootverbose) {
		device_printf(codec->dev, "ac97 codec id 0x%08x", id);
		if (codec->id)
			printf(" (%s)", codec->id);
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
	snd_mtxunlock(codec->lock);
	return 0;
}

static unsigned
ac97_reinitmixer(struct ac97_info *codec)
{
	snd_mtxlock(codec->lock);
	codec->count = AC97_INIT(codec->methods, codec->devinfo);
	if (codec->count == 0) {
		device_printf(codec->dev, "ac97 codec init failed\n");
		snd_mtxunlock(codec->lock);
		return ENODEV;
	}

	wrcd(codec, AC97_REG_POWER, (codec->flags & AC97_F_EAPD_INV)? 0x8000 : 0x0000);
	ac97_reset(codec);
	wrcd(codec, AC97_REG_POWER, (codec->flags & AC97_F_EAPD_INV)? 0x8000 : 0x0000);

	if (!codec->noext) {
		wrcd(codec, AC97_REGEXT_STAT, codec->extstat);
		if ((rdcd(codec, AC97_REGEXT_STAT) & AC97_EXTCAPS)
		    != codec->extstat)
			device_printf(codec->dev, "ac97 codec failed to reset extended mode (%x, got %x)\n",
				      codec->extstat,
				      rdcd(codec, AC97_REGEXT_STAT) &
					AC97_EXTCAPS);
	}

	if ((rdcd(codec, AC97_REG_POWER) & 2) == 0)
		device_printf(codec->dev, "ac97 codec reports dac not ready\n");
	snd_mtxunlock(codec->lock);
	return 0;
}

struct ac97_info *
ac97_create(device_t dev, void *devinfo, kobj_class_t cls)
{
	struct ac97_info *codec;

	codec = (struct ac97_info *)malloc(sizeof *codec, M_AC97, M_NOWAIT);
	if (codec == NULL)
		return NULL;

	snprintf(codec->name, AC97_NAMELEN, "%s:ac97", device_get_nameunit(dev));
	codec->lock = snd_mtxcreate(codec->name, "ac97 codec");
	codec->methods = kobj_create(cls, M_AC97, M_WAITOK);
	if (codec->methods == NULL) {
		snd_mtxlock(codec->lock);
		snd_mtxfree(codec->lock);
		free(codec, M_AC97);
		return NULL;
	}

	codec->dev = dev;
	codec->devinfo = devinfo;
	codec->flags = 0;
	return codec;
}

void
ac97_destroy(struct ac97_info *codec)
{
	snd_mtxlock(codec->lock);
	if (codec->methods != NULL)
		kobj_delete(codec->methods, M_AC97);
	snd_mtxfree(codec->lock);
	free(codec, M_AC97);
}

void
ac97_setflags(struct ac97_info *codec, u_int32_t val)
{
	codec->flags = val;
}

u_int32_t
ac97_getflags(struct ac97_info *codec)
{
	return codec->flags;
}

/* -------------------------------------------------------------------- */

static int
ac97mix_init(struct snd_mixer *m)
{
	struct ac97_info *codec = mix_getdevinfo(m);
	u_int32_t i, mask;

	if (codec == NULL)
		return -1;

	if (ac97_initmixer(codec))
		return -1;

	mask = 0;
	for (i = 0; i < 32; i++)
		mask |= codec->mix[i].enable? 1 << i : 0;
	mix_setdevs(m, mask);

	mask = 0;
	for (i = 0; i < 32; i++)
		mask |= codec->mix[i].recidx? 1 << i : 0;
	mix_setrecdevs(m, mask);
	return 0;
}

static int
ac97mix_uninit(struct snd_mixer *m)
{
	struct ac97_info *codec = mix_getdevinfo(m);

	if (codec == NULL)
		return -1;
	/*
	if (ac97_uninitmixer(codec))
		return -1;
	*/
	ac97_destroy(codec);
	return 0;
}

static int
ac97mix_reinit(struct snd_mixer *m)
{
	struct ac97_info *codec = mix_getdevinfo(m);

	if (codec == NULL)
		return -1;
	return ac97_reinitmixer(codec);
}

static int
ac97mix_set(struct snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
	struct ac97_info *codec = mix_getdevinfo(m);

	if (codec == NULL)
		return -1;
	return ac97_setmixer(codec, dev, left, right);
}

static int
ac97mix_setrecsrc(struct snd_mixer *m, u_int32_t src)
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

static kobj_method_t ac97mixer_methods[] = {
    	KOBJMETHOD(mixer_init,		ac97mix_init),
    	KOBJMETHOD(mixer_uninit,	ac97mix_uninit),
    	KOBJMETHOD(mixer_reinit,	ac97mix_reinit),
    	KOBJMETHOD(mixer_set,		ac97mix_set),
    	KOBJMETHOD(mixer_setrecsrc,	ac97mix_setrecsrc),
	{ 0, 0 }
};
MIXER_DECLARE(ac97mixer);

/* -------------------------------------------------------------------- */

kobj_class_t
ac97_getmixerclass(void)
{
	return &ac97mixer_class;
}


