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

#define ISA_DMA(b) (sndbuf_getflags((b)) & SNDBUF_F_ISADMA)
#define SNDBUF_LOCKASSERT(b)

#define	SNDBUF_F_ISADMA		0x00000001
#define	SNDBUF_F_XRUN		0x00000002
#define	SNDBUF_F_RUNNING	0x00000004

struct snd_dbuf *sndbuf_create(device_t dev, char *drv, char *desc);
void sndbuf_destroy(struct snd_dbuf *b);

void sndbuf_dump(struct snd_dbuf *b, char *s, u_int32_t what);

int sndbuf_alloc(struct snd_dbuf *b, bus_dma_tag_t dmatag, unsigned int size);
int sndbuf_setup(struct snd_dbuf *b, void *buf, unsigned int size);
void sndbuf_free(struct snd_dbuf *b);
int sndbuf_resize(struct snd_dbuf *b, unsigned int blkcnt, unsigned int blksz);
int sndbuf_remalloc(struct snd_dbuf *b, unsigned int blkcnt, unsigned int blksz);
void sndbuf_reset(struct snd_dbuf *b);
void sndbuf_clear(struct snd_dbuf *b, unsigned int length);
void sndbuf_fillsilence(struct snd_dbuf *b);

u_int32_t sndbuf_getfmt(struct snd_dbuf *b);
int sndbuf_setfmt(struct snd_dbuf *b, u_int32_t fmt);
unsigned int sndbuf_getspd(struct snd_dbuf *b);
void sndbuf_setspd(struct snd_dbuf *b, unsigned int spd);
unsigned int sndbuf_getbps(struct snd_dbuf *b);

void *sndbuf_getbuf(struct snd_dbuf *b);
void *sndbuf_getbufofs(struct snd_dbuf *b, unsigned int ofs);
unsigned int sndbuf_getsize(struct snd_dbuf *b);
unsigned int sndbuf_getmaxsize(struct snd_dbuf *b);
unsigned int sndbuf_getalign(struct snd_dbuf *b);
unsigned int sndbuf_getblkcnt(struct snd_dbuf *b);
void sndbuf_setblkcnt(struct snd_dbuf *b, unsigned int blkcnt);
unsigned int sndbuf_getblksz(struct snd_dbuf *b);
void sndbuf_setblksz(struct snd_dbuf *b, unsigned int blksz);
unsigned int sndbuf_runsz(struct snd_dbuf *b);
void sndbuf_setrun(struct snd_dbuf *b, int go);
struct selinfo *sndbuf_getsel(struct snd_dbuf *b);

unsigned int sndbuf_getxrun(struct snd_dbuf *b);
void sndbuf_setxrun(struct snd_dbuf *b, unsigned int cnt);
unsigned int sndbuf_gethwptr(struct snd_dbuf *b);
void sndbuf_sethwptr(struct snd_dbuf *b, unsigned int ptr);
unsigned int sndbuf_getfree(struct snd_dbuf *b);
unsigned int sndbuf_getfreeptr(struct snd_dbuf *b);
unsigned int sndbuf_getready(struct snd_dbuf *b);
unsigned int sndbuf_getreadyptr(struct snd_dbuf *b);
unsigned int sndbuf_getblocks(struct snd_dbuf *b);
unsigned int sndbuf_getprevblocks(struct snd_dbuf *b);
unsigned int sndbuf_gettotal(struct snd_dbuf *b);
void sndbuf_updateprevtotal(struct snd_dbuf *b);

int sndbuf_acquire(struct snd_dbuf *b, u_int8_t *from, unsigned int count);
int sndbuf_dispose(struct snd_dbuf *b, u_int8_t *to, unsigned int count);
int sndbuf_uiomove(struct snd_dbuf *b, struct uio *uio, unsigned int count);
int sndbuf_feed(struct snd_dbuf *from, struct snd_dbuf *to, struct pcm_channel *channel, struct pcm_feeder *feeder, unsigned int count);

u_int32_t sndbuf_getflags(struct snd_dbuf *b);
void sndbuf_setflags(struct snd_dbuf *b, u_int32_t flags, int on);

int sndbuf_isadmasetup(struct snd_dbuf *b, struct resource *drq);
int sndbuf_isadmasetdir(struct snd_dbuf *b, int dir);
void sndbuf_isadma(struct snd_dbuf *b, int go);
int sndbuf_isadmaptr(struct snd_dbuf *b);
void sndbuf_isadmabounce(struct snd_dbuf *b);


