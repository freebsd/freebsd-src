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
 * $FreeBSD: src/sys/dev/sound/pcm/mixer.c,v 1.4.2.2 2000/07/19 21:18:46 cg Exp $
 */

#include <dev/sound/pcm/sound.h>

static u_int16_t snd_mixerdefaults[SOUND_MIXER_NRDEVICES] = {
	[SOUND_MIXER_VOLUME]	= 75,
	[SOUND_MIXER_BASS]	= 50,
	[SOUND_MIXER_TREBLE]	= 50,
	[SOUND_MIXER_SYNTH]	= 75,
	[SOUND_MIXER_PCM]	= 75,
	[SOUND_MIXER_SPEAKER]	= 75,
	[SOUND_MIXER_LINE]	= 75,
	[SOUND_MIXER_MIC] 	= 0,
	[SOUND_MIXER_CD]	= 75,
	[SOUND_MIXER_LINE1]	= 75,
	[SOUND_MIXER_VIDEO]	= 75,
	[SOUND_MIXER_RECLEV]	= 0,
	[SOUND_MIXER_OGAIN]	= 50,
};

int
mixer_init(snddev_info *d, snd_mixer *m, void *devinfo)
{
	if (d == NULL) return -1;
	d->mixer = *m;
	d->mixer.devinfo = devinfo;
	bzero(&d->mixer.level, sizeof d->mixer.level);
	if (d->mixer.init != NULL && d->mixer.init(&d->mixer) == 0) {
		int i;
		for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
			u_int16_t v = snd_mixerdefaults[i];
			mixer_set(d, i, v | (v << 8));
		}
		mixer_setrecsrc(d, SOUND_MASK_MIC);
		return 0;
	} else return -1;
}

int
mixer_reinit(snddev_info *d)
{
	int i;
	if (d == NULL) return -1;
	if (d->mixer.init != NULL && d->mixer.init(&d->mixer) == 0) {
		for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
			mixer_set(d, i, d->mixer.level[i]);
		mixer_setrecsrc(d, d->mixer.recsrc);
		return 0;
	} else return -1;
}

int
mixer_set(snddev_info *d, unsigned dev, unsigned lev)
{
	if (d == NULL || d->mixer.set == NULL) return -1;
	if ((dev < SOUND_MIXER_NRDEVICES) && (d->mixer.devs & (1 << dev))) {
		unsigned l = min((lev & 0x00ff), 100);
		unsigned r = min(((lev & 0xff00) >> 8), 100);
		int v = d->mixer.set(&d->mixer, dev, l, r);
		if (v >= 0) d->mixer.level[dev] = l | (r << 8);
		return 0;
	} else return -1;
}

int
mixer_get(snddev_info *d, int dev)
{
	if (d == NULL) return -1;
	if (dev < SOUND_MIXER_NRDEVICES && (d->mixer.devs & (1 << dev)))
		return d->mixer.level[dev];
	else return -1;
}

int
mixer_setrecsrc(snddev_info *d, u_int32_t src)
{
	if (d == NULL || d->mixer.setrecsrc == NULL) return -1;
	src &= d->mixer.recdevs;
	if (src == 0) src = SOUND_MASK_MIC;
	d->mixer.recsrc = d->mixer.setrecsrc(&d->mixer, src);
	return 0;
}

int
mixer_getrecsrc(snddev_info *d)
{
	if (d == NULL) return -1;
	return d->mixer.recsrc;
}

int
mixer_ioctl(snddev_info *d, u_long cmd, caddr_t arg)
{
	int ret, *arg_i = (int *)arg;

	if ((cmd & MIXER_WRITE(0)) == MIXER_WRITE(0)) {
		int j = cmd & 0xff;

		if (j == SOUND_MIXER_RECSRC) ret = mixer_setrecsrc(d, *arg_i);
		else ret = mixer_set(d, j, *arg_i);
		return (ret == 0)? 0 : ENXIO;
	}

    	if ((cmd & MIXER_READ(0)) == MIXER_READ(0)) {
		int v = -1, j = cmd & 0xff;

		switch (j) {
    		case SOUND_MIXER_DEVMASK:
    		case SOUND_MIXER_CAPS:
    		case SOUND_MIXER_STEREODEVS:
			v = d->mixer.devs;
			break;

    		case SOUND_MIXER_RECMASK:
			v = d->mixer.recdevs;
			break;

    		case SOUND_MIXER_RECSRC:
			v = mixer_getrecsrc(d);
			break;

		default:
			v = mixer_get(d, j);
		}
		*arg_i = v;
		return (v != -1)? 0 : ENXIO;
	}
	return ENXIO;
}

void
mix_setdevs(snd_mixer *m, u_int32_t v)
{
	m->devs = v;
}

void
mix_setrecdevs(snd_mixer *m, u_int32_t v)
{
	m->recdevs = v;
}

u_int32_t
mix_getdevs(snd_mixer *m)
{
	return m->devs;
}

u_int32_t
mix_getrecdevs(snd_mixer *m)
{
	return m->recdevs;
}

void *
mix_getdevinfo(snd_mixer *m)
{
	return m->devinfo;
}

/*
 * The various mixers use a variety of bitmasks etc. The Voxware
 * driver had a very nice technique to describe a mixer and interface
 * to it. A table defines, for each channel, which register, bits,
 * offset, polarity to use. This procedure creates the new value
 * using the table and the old value.
 */

void
change_bits(mixer_tab *t, u_char *regval, int dev, int chn, int newval)
{
    	u_char mask;
    	int shift;

    	DEB(printf("ch_bits dev %d ch %d val %d old 0x%02x "
		"r %d p %d bit %d off %d\n",
		dev, chn, newval, *regval,
		(*t)[dev][chn].regno, (*t)[dev][chn].polarity,
		(*t)[dev][chn].nbits, (*t)[dev][chn].bitoffs ) );

    	if ( (*t)[dev][chn].polarity == 1)	/* reverse */
		newval = 100 - newval ;

    	mask = (1 << (*t)[dev][chn].nbits) - 1;
    	newval = (int) ((newval * mask) + 50) / 100; /* Scale it */
    	shift = (*t)[dev][chn].bitoffs /*- (*t)[dev][LEFT_CHN].nbits + 1*/;

    	*regval &= ~(mask << shift);        /* Filter out the previous value */
    	*regval |= (newval & mask) << shift;        /* Set the new value */
}

