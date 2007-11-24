/*-
 * Copyright (c) 1999 Cameron Grant <cg@freebsd.org>
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

SND_DECLARE_FILE("$FreeBSD$");

static u_int32_t fk_fmt[] = {
	AFMT_MU_LAW,
	AFMT_STEREO | AFMT_MU_LAW,
	AFMT_A_LAW,
	AFMT_STEREO | AFMT_A_LAW,
	AFMT_U8,
	AFMT_STEREO | AFMT_U8,
	AFMT_S8,
	AFMT_STEREO | AFMT_S8,
	AFMT_S16_LE,
	AFMT_STEREO | AFMT_S16_LE,
	AFMT_U16_LE,
	AFMT_STEREO | AFMT_U16_LE,
	AFMT_S16_BE,
	AFMT_STEREO | AFMT_S16_BE,
	AFMT_U16_BE,
	AFMT_STEREO | AFMT_U16_BE,
	AFMT_S24_LE,
	AFMT_STEREO | AFMT_S24_LE,
	AFMT_U24_LE,
	AFMT_STEREO | AFMT_U24_LE,
	AFMT_S24_BE,
	AFMT_STEREO | AFMT_S24_BE,
	AFMT_U24_BE,
	AFMT_STEREO | AFMT_U24_BE,
	AFMT_S32_LE,
	AFMT_STEREO | AFMT_S32_LE,
	AFMT_U32_LE,
	AFMT_STEREO | AFMT_U32_LE,
	AFMT_S32_BE,
	AFMT_STEREO | AFMT_S32_BE,
	AFMT_U32_BE,
	AFMT_STEREO | AFMT_U32_BE,
	0
};
static struct pcmchan_caps fk_caps = {0, 1000000, fk_fmt, 0};

#define	FKBUFSZ	4096
static char fakebuf[FKBUFSZ];

/* channel interface */
static void *
fkchan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	sndbuf_setup(b, fakebuf, FKBUFSZ);
	return (void *)0xbabef00d;
}

static int
fkchan_free(kobj_t obj, void *data)
{
	return 0;
}

static int
fkchan_setformat(kobj_t obj, void *data, u_int32_t format)
{
	return 0;
}

static int
fkchan_setspeed(kobj_t obj, void *data, u_int32_t speed)
{
	return speed;
}

static int
fkchan_setblocksize(kobj_t obj, void *data, u_int32_t blocksize)
{
	return blocksize;
}

static int
fkchan_trigger(kobj_t obj, void *data, int go)
{
	return 0;
}

static int
fkchan_getptr(kobj_t obj, void *data)
{
	return 0;
}

static struct pcmchan_caps *
fkchan_getcaps(kobj_t obj, void *data)
{
	return &fk_caps;
}

static kobj_method_t fkchan_methods[] = {
    	KOBJMETHOD(channel_init,		fkchan_init),
    	KOBJMETHOD(channel_free,		fkchan_free),
    	KOBJMETHOD(channel_setformat,		fkchan_setformat),
    	KOBJMETHOD(channel_setspeed,		fkchan_setspeed),
    	KOBJMETHOD(channel_setblocksize,	fkchan_setblocksize),
    	KOBJMETHOD(channel_trigger,		fkchan_trigger),
    	KOBJMETHOD(channel_getptr,		fkchan_getptr),
    	KOBJMETHOD(channel_getcaps,		fkchan_getcaps),
	{ 0, 0 }
};
CHANNEL_DECLARE(fkchan);

struct pcm_channel *
fkchan_setup(device_t dev)
{
    	struct snddev_info *d = device_get_softc(dev);
	struct pcm_channel *c;

	c = malloc(sizeof(*c), M_DEVBUF, M_WAITOK);
	c->methods = kobj_create(&fkchan_class, M_DEVBUF, M_WAITOK);
	c->parentsnddev = d;
	/*
	 * Fake channel is such a blessing in disguise. Using this,
	 * we can keep track prefered virtual channel speed without
	 * querying kernel hint repetitively (see vchan_create / vchan.c).
	 */
	c->speed = 0;
	snprintf(c->name, CHN_NAMELEN, "%s:fake", device_get_nameunit(dev));

	return c;
}

int
fkchan_kill(struct pcm_channel *c)
{
	kobj_delete(c->methods, M_DEVBUF);
	c->methods = NULL;
	free(c, M_DEVBUF);
	return 0;
}


