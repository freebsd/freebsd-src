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

/* channel interface */
static void *fkchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir);
static int fkchan_setdir(void *data, int dir);
static int fkchan_setformat(void *data, u_int32_t format);
static int fkchan_setspeed(void *data, u_int32_t speed);
static int fkchan_setblocksize(void *data, u_int32_t blocksize);
static int fkchan_trigger(void *data, int go);
static int fkchan_getptr(void *data);
static pcmchan_caps *fkchan_getcaps(void *data);

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
static pcmchan_caps fk_caps = {4000, 48000, fk_fmt, 0};

static pcm_channel fk_chantemplate = {
	fkchan_init,
	fkchan_setdir,
	fkchan_setformat,
	fkchan_setspeed,
	fkchan_setblocksize,
	fkchan_trigger,
	fkchan_getptr,
	fkchan_getcaps,
};

/* channel interface */
static void *
fkchan_init(void *devinfo, snd_dbuf *b, pcm_channel *c, int dir)
{
	b->bufsize = 16384;
	b->buf = malloc(b->bufsize, M_DEVBUF, M_NOWAIT);
	return (void *)0xbabef00d;
}

static int
fkchan_setdir(void *data, int dir)
{
	return 0;
}

static int
fkchan_setformat(void *data, u_int32_t format)
{
	return 0;
}

static int
fkchan_setspeed(void *data, u_int32_t speed)
{
	return speed;
}

static int
fkchan_setblocksize(void *data, u_int32_t blocksize)
{
	return blocksize;
}

static int
fkchan_trigger(void *data, int go)
{
	return 0;
}

static int
fkchan_getptr(void *data)
{
	return 0;
}

static pcmchan_caps *
fkchan_getcaps(void *data)
{
	return &fk_caps;
}

int
fkchan_setup(pcm_channel *c)
{
	*c = fk_chantemplate;
	return 0;
}
