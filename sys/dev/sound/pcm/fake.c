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

static u_int32_t fk_fmt[] = {
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
	0
};
static pcmchan_caps fk_caps = {0, 1000000, fk_fmt, 0};

/* channel interface */
static void *
fkchan_init(kobj_t obj, void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	b->bufsize = 16384;
	b->buf = malloc(b->bufsize, M_DEVBUF, M_NOWAIT);
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

static pcmchan_caps *
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

int
fkchan_setup(pcm_channel *c)
{
	c->methods = kobj_create(&fkchan_class, M_DEVBUF, M_WAITOK);
	return 0;
}

int
fkchan_kill(pcm_channel *c)
{
	kobj_delete(c->methods, M_DEVBUF);
	c->methods = NULL;
	return 0;
}


