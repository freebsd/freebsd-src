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

#define ISA_DMA(b) (((b)->chan >= 0 && (b)->chan != 4 && (b)->chan < 8))

int sndbuf_alloc(snd_dbuf *b, bus_dma_tag_t dmatag, int size);
int sndbuf_setup(snd_dbuf *b, void *buf, int size);
void sndbuf_free(snd_dbuf *b);
int sndbuf_resize(snd_dbuf *b, int blkcnt, int blksz);
void sndbuf_reset(snd_dbuf *b);
void sndbuf_clear(snd_dbuf *b, int length);
int sndbuf_setfmt(snd_dbuf *b, u_int32_t fmt);
int sndbuf_getbps(snd_dbuf *b);
void *sndbuf_getbuf(snd_dbuf *b);
int sndbuf_getsize(snd_dbuf *b);
int sndbuf_runsz(snd_dbuf *b);

int sndbuf_isadmasetup(snd_dbuf *b, struct resource *drq);
int sndbuf_isadmasetdir(snd_dbuf *b, int dir);
void sndbuf_isadma(snd_dbuf *b, int go);
int sndbuf_isadmaptr(snd_dbuf *b);
void sndbuf_isadmabounce(snd_dbuf *b);


