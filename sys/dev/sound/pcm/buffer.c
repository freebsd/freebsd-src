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

static void
sndbuf_setmap(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	snd_dbuf *b = (snd_dbuf *)arg;

	if (bootverbose) {
		printf("pcm: setmap %lx, %lx; ", (unsigned long)segs->ds_addr,
		       (unsigned long)segs->ds_len);
		printf("%p -> %lx\n", b->buf, (unsigned long)vtophys(b->buf));
	}
}

/*
 * Allocate memory for DMA buffer. If the device do not perform DMA transfer,
 * the driver can call malloc(9) by its own.
 */
int
sndbuf_alloc(snd_dbuf *b, bus_dma_tag_t dmatag, int size)
{
	b->dmatag = dmatag;
	b->maxsize = size;
	b->bufsize = b->maxsize;
	if (bus_dmamem_alloc(b->dmatag, (void **)&b->buf, BUS_DMA_NOWAIT, &b->dmamap))
		return ENOSPC;
	if (bus_dmamap_load(b->dmatag, b->dmamap, b->buf, b->maxsize, sndbuf_setmap, b, 0))
		return ENOSPC;
	return sndbuf_resize(b, 2, b->maxsize / 2);
}

int
sndbuf_setup(snd_dbuf *b, void *buf, int size)
{
	bzero(b, sizeof(*b));
	b->buf = buf;
	b->maxsize = size;
	b->bufsize = b->maxsize;
	return sndbuf_resize(b, 2, b->maxsize / 2);
}

void
sndbuf_free(snd_dbuf *b)
{
	bus_dmamap_unload(b->dmatag, b->dmamap);
	bus_dmamem_free(b->dmatag, b->buf, b->dmamap);
}

int
sndbuf_resize(snd_dbuf *b, int blkcnt, int blksz)
{
	if (blkcnt == 0)
		blkcnt = b->blkcnt;
	if (blksz == 0)
		blksz = b->blksz;
	if (blkcnt < 2 || blksz < 16 || (blkcnt * blksz > b->maxsize))
		return EINVAL;
	b->blkcnt = blkcnt;
	b->blksz = blksz;
	b->bufsize = blkcnt * blksz;
	sndbuf_reset(b);
	return 0;
}

void
sndbuf_clear(snd_dbuf *b, int length)
{
	int i;
	u_int16_t data, *p;

	if (length == 0)
		return;

	if (b->fmt & AFMT_SIGNED)
		data = 0x00;
	else
		data = 0x80;

	if (b->fmt & AFMT_16BIT)
		data <<= 8;
	else
		data |= data << 8;

	if (b->fmt & AFMT_BIGENDIAN)
		data = ((data >> 8) & 0x00ff) | ((data << 8) & 0xff00);

	i = b->fp;
	p = (u_int16_t *)(b->buf + b->fp);
	while (length > 1) {
		*p++ = data;
		length -= 2;
		i += 2;
		if (i >= b->bufsize) {
			p = (u_int16_t *)b->buf;
			i = 0;
		}
	}
	if (length == 1)
		*(b->buf + i) = data & 0xff;
}

void
sndbuf_reset(snd_dbuf *b)
{
	b->rp = b->fp = 0;
	b->dl = b->rl = 0;
	b->fl = b->bufsize;
	b->prev_total = b->total = 0;
	b->prev_int_count = b->int_count = 0;
	b->underflow = 0;
	if (b->buf && b->bufsize > 0)
		sndbuf_clear(b, b->bufsize);
}

int
sndbuf_setfmt(snd_dbuf *b, u_int32_t fmt)
{
	b->fmt = fmt;
	b->bps = 1;
	b->bps <<= (b->fmt & AFMT_STEREO)? 1 : 0;
	b->bps <<= (b->fmt & AFMT_16BIT)? 1 : 0;
	b->bps <<= (b->fmt & AFMT_32BIT)? 2 : 0;
	return 0;
}

int
sndbuf_getbps(snd_dbuf *b)
{
	return b->bps;
}

void *
sndbuf_getbuf(snd_dbuf *b)
{
	return b->buf;
}

int
sndbuf_getsize(snd_dbuf *b)
{
	return b->bufsize;
}

int
sndbuf_runsz(snd_dbuf *b)
{
	return b->dl;
}

int
sndbuf_isadmasetup(snd_dbuf *b, struct resource *drq)
{
	/* should do isa_dma_acquire/isa_dma_release here */
	if (drq == NULL) {
		b->flags &= ~SNDBUF_F_ISADMA;
		b->chan = -1;
	} else {
		b->flags &= ~SNDBUF_F_ISADMA;
		b->chan = rman_get_start(drq);
	}
	return 0;
}

int
sndbuf_isadmasetdir(snd_dbuf *b, int dir)
{
	b->dir = (dir == PCMDIR_PLAY)? ISADMA_WRITE : ISADMA_READ;
	return 0;
}

void
sndbuf_isadma(snd_dbuf *b, int go)
{
	KASSERT(b, ("sndbuf_isadma called with b == NULL"));
	KASSERT(ISA_DMA(b), ("sndbuf_isadma called on non-ISA channel"));

	switch (go) {
	case PCMTRIG_START:
		/* isa_dmainit(b->chan, size); */
		isa_dmastart(b->dir | ISADMA_RAW, b->buf, b->bufsize, b->chan);
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		isa_dmastop(b->chan);
		isa_dmadone(b->dir | ISADMA_RAW, b->buf, b->bufsize, b->chan);
		break;
	}

	DEB(printf("buf 0x%p ISA DMA %s, channel %d\n",
		b,
		(go == PCMTRIG_START)? "started" : "stopped",
		b->chan));
}

int
sndbuf_isadmaptr(snd_dbuf *b)
{
	if (ISA_DMA(b)) {
		int i = b->dl? isa_dmastatus(b->chan) : b->bufsize;
		if (i < 0)
			i = 0;
		return b->bufsize - i;
    	} else KASSERT(1, ("sndbuf_isadmaptr called on invalid channel"));
	return -1;
}

void
sndbuf_isadmabounce(snd_dbuf *b)
{
	if (ISA_DMA(b)) {
		/* tell isa_dma to bounce data in/out */
    	} else
		KASSERT(1, ("chn_isadmabounce called on invalid channel"));
}

