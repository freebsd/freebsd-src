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

#include "feeder_if.h"

#define	MIN(x, y) (((x) < (y))? (x) : (y))

#define SNDBUF_NAMELEN	48
struct snd_dbuf {
        u_int8_t *buf, *tmpbuf;
        unsigned int bufsize, maxsize;
        volatile int dl; /* transfer size */
        volatile int rp; /* pointers to the ready area */
	volatile int rl; /* length of ready area */
	volatile int hp;
	volatile u_int32_t total, prev_total;
	int isadmachan, dir;       /* dma channel */
	u_int32_t fmt, spd, bps;
	unsigned int blksz, blkcnt;
	int xrun;
	u_int32_t flags;
	bus_dmamap_t dmamap;
	bus_dma_tag_t dmatag;
	struct selinfo sel;
	char name[SNDBUF_NAMELEN];
};

struct snd_dbuf *
sndbuf_create(char *drv, char *desc)
{
	struct snd_dbuf *b;

	b = malloc(sizeof(*b), M_DEVBUF, M_WAITOK | M_ZERO);
	snprintf(b->name, SNDBUF_NAMELEN, "%s:%s", drv, desc);
	return b;
}

void
sndbuf_destroy(struct snd_dbuf *b)
{
	free(b, M_DEVBUF);
}

static void
sndbuf_setmap(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct snd_dbuf *b = (struct snd_dbuf *)arg;

	if (bootverbose) {
		printf("pcm: setmap %lx, %lx; ", (unsigned long)segs->ds_addr,
		       (unsigned long)segs->ds_len);
		printf("%p -> %lx\n", b->buf, (unsigned long)vtophys(b->buf));
	}
}

/*
 * Allocate memory for DMA buffer. If the device does not use DMA transfers,
 * the driver can call malloc(9) and sndbuf_setup() itself.
 */
int
sndbuf_alloc(struct snd_dbuf *b, bus_dma_tag_t dmatag, unsigned int size)
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
sndbuf_setup(struct snd_dbuf *b, void *buf, unsigned int size)
{
	b->buf = buf;
	b->maxsize = size;
	b->bufsize = b->maxsize;
	return sndbuf_resize(b, 2, b->maxsize / 2);
}

void
sndbuf_free(struct snd_dbuf *b)
{
	if (b->tmpbuf)
		free(b->tmpbuf, M_DEVBUF);
	b->tmpbuf = NULL;

	if (b->dmamap)
		bus_dmamap_unload(b->dmatag, b->dmamap);

	if (b->dmamap && b->buf)
		bus_dmamem_free(b->dmatag, b->buf, b->dmamap);
	b->dmamap = NULL;
	b->buf = NULL;
}

int
sndbuf_resize(struct snd_dbuf *b, unsigned int blkcnt, unsigned int blksz)
{
	if (b->maxsize == 0)
		return 0;
	if (blkcnt == 0)
		blkcnt = b->blkcnt;
	if (blksz == 0)
		blksz = b->blksz;
	if (blkcnt < 2 || blksz < 16 || (blkcnt * blksz > b->maxsize))
		return EINVAL;
	b->blkcnt = blkcnt;
	b->blksz = blksz;
	b->bufsize = blkcnt * blksz;
	if (b->tmpbuf)
		free(b->tmpbuf, M_DEVBUF);
	b->tmpbuf = malloc(b->bufsize, M_DEVBUF, M_WAITOK);
	sndbuf_reset(b);
	return 0;
}

int
sndbuf_remalloc(struct snd_dbuf *b, unsigned int blkcnt, unsigned int blksz)
{
	if (blkcnt < 2 || blksz < 16)
		return EINVAL;

	b->blkcnt = blkcnt;
	b->blksz = blksz;

	b->maxsize = blkcnt * blksz;
	b->bufsize = b->maxsize;

	if (b->buf)
		free(b->buf, M_DEVBUF);
	b->buf = malloc(b->bufsize, M_DEVBUF, M_WAITOK);
	if (b->buf == NULL)
		return ENOMEM;

	if (b->tmpbuf)
		free(b->tmpbuf, M_DEVBUF);
	b->tmpbuf = malloc(b->bufsize, M_DEVBUF, M_WAITOK);
	if (b->tmpbuf == NULL)
		return ENOMEM;

	sndbuf_reset(b);
	return 0;
}

void
sndbuf_clear(struct snd_dbuf *b, unsigned int length)
{
	int i;
	u_char data, *p;

	if (length == 0)
		return;
	if (length > b->bufsize)
		length = b->bufsize;

	if (b->fmt & AFMT_SIGNED)
		data = 0x00;
	else
		data = 0x80;

	i = sndbuf_getfreeptr(b);
	p = sndbuf_getbuf(b);
	while (length > 0) {
		p[i] = data;
		length--;
		i++;
		if (i >= b->bufsize)
			i = 0;
	}
}

void
sndbuf_fillsilence(struct snd_dbuf *b)
{
	int i;
	u_char data, *p;

	if (b->fmt & AFMT_SIGNED)
		data = 0x00;
	else
		data = 0x80;

	i = 0;
	p = sndbuf_getbuf(b);
	while (i < b->bufsize)
		p[i++] = data;
	b->rp = 0;
	b->rl = b->bufsize;
}

void
sndbuf_reset(struct snd_dbuf *b)
{
	b->hp = 0;
	b->rp = 0;
	b->rl = 0;
	b->dl = 0;
	b->prev_total = 0;
	b->total = 0;
	b->xrun = 0;
	if (b->buf && b->bufsize > 0)
		sndbuf_clear(b, b->bufsize);
}

u_int32_t
sndbuf_getfmt(struct snd_dbuf *b)
{
	return b->fmt;
}

int
sndbuf_setfmt(struct snd_dbuf *b, u_int32_t fmt)
{
	b->fmt = fmt;
	b->bps = 1;
	b->bps <<= (b->fmt & AFMT_STEREO)? 1 : 0;
	b->bps <<= (b->fmt & AFMT_16BIT)? 1 : 0;
	b->bps <<= (b->fmt & AFMT_32BIT)? 2 : 0;
	return 0;
}

unsigned int
sndbuf_getspd(struct snd_dbuf *b)
{
	return b->spd;
}

void
sndbuf_setspd(struct snd_dbuf *b, unsigned int spd)
{
	b->spd = spd;
}

unsigned int
sndbuf_getalign(struct snd_dbuf *b)
{
	static int align[] = {0, 1, 1, 2, 2, 2, 2, 3};

	return align[b->bps - 1];
}

unsigned int
sndbuf_getblkcnt(struct snd_dbuf *b)
{
	return b->blkcnt;
}

void
sndbuf_setblkcnt(struct snd_dbuf *b, unsigned int blkcnt)
{
	b->blkcnt = blkcnt;
}

unsigned int
sndbuf_getblksz(struct snd_dbuf *b)
{
	return b->blksz;
}

void
sndbuf_setblksz(struct snd_dbuf *b, unsigned int blksz)
{
	b->blksz = blksz;
}

unsigned int
sndbuf_getbps(struct snd_dbuf *b)
{
	return b->bps;
}

void *
sndbuf_getbuf(struct snd_dbuf *b)
{
	return b->buf;
}

void *
sndbuf_getbufofs(struct snd_dbuf *b, unsigned int ofs)
{
	KASSERT((ofs >= 0) && (ofs <= b->bufsize), ("%s: ofs invalid %d", __FUNCTION__, ofs));

	return b->buf + ofs;
}

unsigned int
sndbuf_getsize(struct snd_dbuf *b)
{
	return b->bufsize;
}

unsigned int
sndbuf_getmaxsize(struct snd_dbuf *b)
{
	return b->maxsize;
}

unsigned int
sndbuf_runsz(struct snd_dbuf *b)
{
	return b->dl;
}

void
sndbuf_setrun(struct snd_dbuf *b, int go)
{
	b->dl = go? b->blksz : 0;
}

struct selinfo *
sndbuf_getsel(struct snd_dbuf *b)
{
	return &b->sel;
}

/************************************************************/
unsigned int
sndbuf_getxrun(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);

	return b->xrun;
}

void
sndbuf_setxrun(struct snd_dbuf *b, unsigned int cnt)
{
	SNDBUF_LOCKASSERT(b);

	b->xrun = cnt;
}

unsigned int
sndbuf_gethwptr(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);

	return b->hp;
}

void
sndbuf_sethwptr(struct snd_dbuf *b, unsigned int ptr)
{
	SNDBUF_LOCKASSERT(b);

	b->hp = ptr;
}

unsigned int
sndbuf_getready(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);
	KASSERT((b->rl >= 0) && (b->rl <= b->bufsize), ("%s: b->rl invalid %d", __FUNCTION__, b->rl));

	return b->rl;
}

unsigned int
sndbuf_getreadyptr(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);
	KASSERT((b->rp >= 0) && (b->rp <= b->bufsize), ("%s: b->rp invalid %d", __FUNCTION__, b->rp));

	return b->rp;
}

unsigned int
sndbuf_getfree(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);
	KASSERT((b->rl >= 0) && (b->rl <= b->bufsize), ("%s: b->rl invalid %d", __FUNCTION__, b->rl));

	return b->bufsize - b->rl;
}

unsigned int
sndbuf_getfreeptr(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);
	KASSERT((b->rp >= 0) && (b->rp <= b->bufsize), ("%s: b->rp invalid %d", __FUNCTION__, b->rp));
	KASSERT((b->rl >= 0) && (b->rl <= b->bufsize), ("%s: b->rl invalid %d", __FUNCTION__, b->rl));

	return (b->rp + b->rl) % b->bufsize;
}

unsigned int
sndbuf_getblocks(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);

	return b->total / b->blksz;
}

unsigned int
sndbuf_getprevblocks(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);

	return b->prev_total / b->blksz;
}

unsigned int
sndbuf_gettotal(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);

	return b->total;
}

void
sndbuf_updateprevtotal(struct snd_dbuf *b)
{
	SNDBUF_LOCKASSERT(b);

	b->prev_total = b->total;
}

/************************************************************/

int
sndbuf_acquire(struct snd_dbuf *b, u_int8_t *from, unsigned int count)
{
	int l;

	KASSERT(count <= sndbuf_getfree(b), ("%s: count %d > free %d", __FUNCTION__, count, sndbuf_getfree(b)));
	KASSERT((b->rl >= 0) && (b->rl <= b->bufsize), ("%s: b->rl invalid %d", __FUNCTION__, b->rl));
	b->total += count;
	if (from != NULL) {
		while (count > 0) {
			l = MIN(count, sndbuf_getsize(b) - sndbuf_getfreeptr(b));
			bcopy(from, sndbuf_getbufofs(b, sndbuf_getfreeptr(b)), l);
			from += l;
			b->rl += l;
			count -= l;
		}
	} else
		b->rl += count;
	KASSERT((b->rl >= 0) && (b->rl <= b->bufsize), ("%s: b->rl invalid %d, count %d", __FUNCTION__, b->rl, count));

	return 0;
}

int
sndbuf_dispose(struct snd_dbuf *b, u_int8_t *to, unsigned int count)
{
	int l;

	KASSERT(count <= sndbuf_getready(b), ("%s: count %d > ready %d", __FUNCTION__, count, sndbuf_getready(b)));
	KASSERT((b->rl >= 0) && (b->rl <= b->bufsize), ("%s: b->rl invalid %d", __FUNCTION__, b->rl));
	if (to != NULL) {
		while (count > 0) {
			l = MIN(count, sndbuf_getsize(b) - sndbuf_getreadyptr(b));
			bcopy(sndbuf_getbufofs(b, sndbuf_getreadyptr(b)), to, l);
			to += l;
			b->rl -= l;
			b->rp = (b->rp + l) % b->bufsize;
			count -= l;
		}
	} else {
		b->rl -= count;
		b->rp = (b->rp + count) % b->bufsize;
	}
	KASSERT((b->rl >= 0) && (b->rl <= b->bufsize), ("%s: b->rl invalid %d, count %d", __FUNCTION__, b->rl, count));

	return 0;
}

int
sndbuf_uiomove(struct snd_dbuf *b, struct uio *uio, unsigned int count)
{
	int x, c, p, rd, err;

	err = 0;
	rd = (uio->uio_rw == UIO_READ)? 1 : 0;
	if (count > uio->uio_resid)
		return EINVAL;

	if (count > (rd? sndbuf_getready(b) : sndbuf_getfree(b))) {
		return EINVAL;
	}

	while (err == 0 && count > 0) {
		p = rd? sndbuf_getreadyptr(b) : sndbuf_getfreeptr(b);
		c = MIN(count, sndbuf_getsize(b) - p);
		x = uio->uio_resid;
		err = uiomove(sndbuf_getbufofs(b, p), c, uio);
		x -= uio->uio_resid;
		count -= x;
		x = rd? sndbuf_dispose(b, NULL, x) : sndbuf_acquire(b, NULL, x);
	}

	return 0;
}

/* count is number of bytes we want added to destination buffer */
int
sndbuf_feed(struct snd_dbuf *from, struct snd_dbuf *to, struct pcm_channel *channel, struct pcm_feeder *feeder, unsigned int count)
{
	KASSERT(count > 0, ("can't feed 0 bytes"));

	if (sndbuf_getfree(to) < count)
		return EINVAL;

	count = FEEDER_FEED(feeder, channel, to->tmpbuf, count, from);
	if (count)
		sndbuf_acquire(to, to->tmpbuf, count);
	/* the root feeder has called sndbuf_dispose(from, , bytes fetched) */

	return 0;
}

/************************************************************/

void
sndbuf_dump(struct snd_dbuf *b, char *s, u_int32_t what)
{
	printf("%s: [", s);
	if (what & 0x01)
		printf(" bufsize: %d, maxsize: %d", b->bufsize, b->maxsize);
	if (what & 0x02)
		printf(" dl: %d, rp: %d, rl: %d, hp: %d", b->dl, b->rp, b->rl, b->hp);
	if (what & 0x04)
		printf(" total: %d, prev_total: %d, xrun: %d", b->total, b->prev_total, b->xrun);
   	if (what & 0x08)
		printf(" fmt: 0x%x, spd: %d", b->fmt, b->spd);
	if (what & 0x10)
		printf(" blksz: %d, blkcnt: %d, flags: 0x%x", b->blksz, b->blkcnt, b->flags);
	printf(" ]\n");
}

/************************************************************/
u_int32_t
sndbuf_getflags(struct snd_dbuf *b)
{
	return b->flags;
}

void
sndbuf_setflags(struct snd_dbuf *b, u_int32_t flags, int on)
{
	b->flags &= ~flags;
	if (on)
		b->flags |= flags;
}

/************************************************************/

int
sndbuf_isadmasetup(struct snd_dbuf *b, struct resource *drq)
{
	/* should do isa_dma_acquire/isa_dma_release here */
	if (drq == NULL) {
		b->isadmachan = -1;
	} else {
		sndbuf_setflags(b, SNDBUF_F_ISADMA, 1);
		b->isadmachan = rman_get_start(drq);
	}
	return 0;
}

int
sndbuf_isadmasetdir(struct snd_dbuf *b, int dir)
{
	KASSERT(b, ("sndbuf_isadmasetdir called with b == NULL"));
	KASSERT(sndbuf_getflags(b) & SNDBUF_F_ISADMA, ("sndbuf_isadmasetdir called on non-ISA buffer"));

	b->dir = (dir == PCMDIR_PLAY)? ISADMA_WRITE : ISADMA_READ;
	return 0;
}

void
sndbuf_isadma(struct snd_dbuf *b, int go)
{
	KASSERT(b, ("sndbuf_isadma called with b == NULL"));
	KASSERT(sndbuf_getflags(b) & SNDBUF_F_ISADMA, ("sndbuf_isadma called on non-ISA buffer"));

	switch (go) {
	case PCMTRIG_START:
		/* isa_dmainit(b->chan, size); */
		isa_dmastart(b->dir | ISADMA_RAW, b->buf, b->bufsize, b->isadmachan);
		break;

	case PCMTRIG_STOP:
	case PCMTRIG_ABORT:
		isa_dmastop(b->isadmachan);
		isa_dmadone(b->dir | ISADMA_RAW, b->buf, b->bufsize, b->isadmachan);
		break;
	}

	DEB(printf("buf 0x%p ISA DMA %s, channel %d\n",
		b,
		(go == PCMTRIG_START)? "started" : "stopped",
		b->chan));
}

int
sndbuf_isadmaptr(struct snd_dbuf *b)
{
	int i;

	KASSERT(b, ("sndbuf_isadmaptr called with b == NULL"));
	KASSERT(sndbuf_getflags(b) & SNDBUF_F_ISADMA, ("sndbuf_isadmaptr called on non-ISA buffer"));

	if (!sndbuf_runsz(b))
		return 0;
	i = isa_dmastatus(b->isadmachan);
	KASSERT(i >= 0, ("isa_dmastatus returned %d", i));
	return b->bufsize - i;
}

void
sndbuf_isadmabounce(struct snd_dbuf *b)
{
	KASSERT(b, ("sndbuf_isadmabounce called with b == NULL"));
	KASSERT(sndbuf_getflags(b) & SNDBUF_F_ISADMA, ("sndbuf_isadmabounce called on non-ISA buffer"));

	/* tell isa_dma to bounce data in/out */
}

