/*
 * Copyright (c) 1999 Cameron Grant <gandalf@vilnya.demon.co.uk>
 * Portions Copyright by Luigi Rizzo - 1997-99
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

#define MIN_CHUNK_SIZE 		256	/* for uiomove etc. */
#define	DMA_ALIGN_THRESHOLD	4
#define	DMA_ALIGN_MASK		(~(DMA_ALIGN_THRESHOLD - 1))

#define ISA_DMA(b) (((b)->chan >= 0 && (b)->chan != 4 && (b)->chan < 8))
#define CANCHANGE(c) (!(c)->buffer.dl)
#define ROUND(x) ((x) & DMA_ALIGN_MASK)
/*
#define DEB(x) x
*/
static void buf_clear(snd_dbuf *b, u_int32_t fmt, int length);
static void chn_dmaupdate(pcm_channel *c);
static void chn_wrintr(pcm_channel *c);
static void chn_rdintr(pcm_channel *c);
static u_int32_t chn_start(pcm_channel *c);
/*
 * SOUND OUTPUT

We use a circular buffer to store samples directed to the DAC.
The buffer is split into two variable-size regions, each identified
by an offset in the buffer (rp,fp) and a length (rl,fl):

      0          rp,rl        fp,fl    bufsize
      |__________>____________>________|
	  FREE   d   READY    w FREE

  READY: data written from the process and ready to be sent to the DAC;
  FREE: free part of the buffer.

Both regions can wrap around the end of the buffer.  At initialization,
READY is empty, FREE takes all the available space, and dma is
idle.  dl contains the length of the current DMA transfer, dl=0
means that the dma is idle.

The two boundaries (rp,fp) in the buffers are advanced by DMA [d]
and write() [w] operations. The first portion of the READY region
is used for DMA transfers. The transfer is started at rp and with
chunks of length dl. During DMA operations, dsp_wr_dmaupdate()
updates rp, rl and fl tracking the ISA DMA engine as the transfer
makes progress.
When a new block is written, fp advances and rl,fl are updated
accordingly.

The code works as follows: the user write routine dsp_write_body()
fills up the READY region with new data (reclaiming space from the
FREE region) and starts the write DMA engine if inactive.  When a
DMA transfer is complete, an interrupt causes dsp_wrintr() to be
called which extends the FREE region and possibly starts the next
transfer.

In some cases, the code tries to track the current status of DMA
operations by calling dsp_wr_dmaupdate() which changes rp, rl and fl.

The system tries to make all DMA transfers use the same size,
play_blocksize or rec_blocksize. The size is either selected by
the user, or computed by the system to correspond to about .25s of
audio. The blocksize must be within a range which is currently:

	min(5ms, 40 bytes) ... 1/2 buffer size.

When there aren't enough data (write) or space (read), a transfer
is started with a reduced size.

To reduce problems in case of overruns, the routine which fills up
the buffer should initialize (e.g. by repeating the last value) a
reasonably long area after the last block so that no noise is
produced on overruns.

  *
  */


/* XXX  this is broken: in the event a bounce buffer is used, data never
 * gets copied in or out of the real buffer.  fix requires mods to isa_dma.c
 * and possibly fixes to other autodma mode clients
 */
static void
chn_isadmabounce(pcm_channel *c)
{
	if (ISA_DMA(&c->buffer)) {
		/* tell isa_dma to bounce data in/out */
    	} else KASSERT(1, ("chn_isadmabounce called on invalid channel"));
}

static int
chn_polltrigger(pcm_channel *c)
{
	snd_dbuf *bs = &c->buffer2nd;
	unsigned lim = (c->flags & CHN_F_HAS_SIZE)? bs->blksz : 0;
	int trig = 0;

	if (c->flags & CHN_F_MAPPED)
		trig = ((bs->int_count > bs->prev_int_count) || bs->prev_int_count == 0);
	else
		trig = (((c->direction == PCMDIR_PLAY)? bs->fl : bs->rl) > lim);
	return trig;
}

static int
chn_pollreset(pcm_channel *c)
{
	snd_dbuf *bs = &c->buffer2nd;

	if (c->flags & CHN_F_MAPPED)
		bs->prev_int_count = bs->int_count;
	return 1;
}

/*
 * chn_dmadone() updates pointers and wakes up any process waiting
 * on a select(). Must be called at spltty().
 */
static void
chn_dmadone(pcm_channel *c)
{
	snd_dbuf *b = &c->buffer;

	if (c->direction == PCMDIR_PLAY)
		chn_checkunderflow(c);
	else
		chn_dmaupdate(c);
	if (ISA_DMA(b))
		chn_isadmabounce(c); /* sync bounce buffer */
	b->int_count++;
}

/*
 * chn_dmawakeup() wakes up any process sleeping. Separated from
 * chn_dmadone() so that wakeup occurs only when feed from a
 * secondary buffer to a DMA buffer takes place. Must be called
 * at spltty().
 */
static void
chn_dmawakeup(pcm_channel *c)
{
	snd_dbuf *b = &c->buffer;

	wakeup(b);
}

/*
 * chn_dmaupdate() tracks the status of a dma transfer,
 * updating pointers. It must be called at spltty().
 *
 * NOTE: when we are using auto dma in the device, rl might become
 * negative.
 */
DEB (static int chn_updatecount=0);

static void
chn_dmaupdate(pcm_channel *c)
{
	snd_dbuf *b = &c->buffer;
	int delta, hwptr;
	DEB (int b_rl=b->rl; int b_fl=b->fl; int b_rp=b->rp; int b_fp=b->fp);

	hwptr = chn_getptr(c);
	delta = (b->bufsize + hwptr - b->hp) % b->bufsize;
	if (delta >= ((b->bufsize * 15) / 16)) {
		if (!(c->flags & (CHN_F_CLOSING | CHN_F_ABORTING)))
			device_printf(c->parent->dev, "hwptr went backwards %d -> %d\n", b->hp, hwptr);
	}
	if (c->direction == PCMDIR_PLAY) {
		delta = (b->bufsize + hwptr - b->rp) % b->bufsize;
		b->rp = hwptr;
		b->rl -= delta;
		b->fl += delta;

		if (b->rl < 0) {
			DEB(printf("OUCH!(%d) rl %d(%d) delta %d bufsize %d hwptr %d rp %d(%d)\n", chn_updatecount++, b->rl, b_rl, delta, b->bufsize, hwptr, b->rp, b_rp));
		}
	} else {
		delta = (b->bufsize + hwptr - b->fp) % b->bufsize;
		b->fp = hwptr;
		b->rl += delta;
		b->fl -= delta;
		if (b->fl < 0) {
			DEB(printf("OUCH!(%d) fl %d(%d) delta %d bufsize %d hwptr %d fp %d(%d)\n", chn_updatecount++, b->fl, b_fl, delta, b->bufsize, hwptr, b->fp, b_fp));
		}
	}
	b->hp = hwptr;
	b->total += delta;
}

/*
 * Check channel for underflow occured. Reset DMA buffer in case of
 * underflow, so that new data can go into the buffer. It must be
 * called at spltty().
 */
void
chn_checkunderflow(pcm_channel *c)
{
    	snd_dbuf *b = &c->buffer;

	if (b->underflow) {
		DEB(printf("Clear underflow condition\n"));
		/*
		 * The DMA keeps running even after underflow occurs.
		 * Hence the value returned by chn_getptr() here soon
		 * gets a lag when we get back to chn_write(). Although
		 * there are no easy and precise methods to figure out
		 * the lag, a quarter of b->bufsize would be a fair
		 * choice, provided that a DMA interrupt generates upon
		 * each transfer of a half b->bufsize.
		 */
		b->rp = chn_getptr(c);
		b->fp = (b->rp + b->bufsize / 4) % b->bufsize;
		b->rl = b->bufsize / 4;
		b->fl = b->bufsize - b->rl;
	  	b->underflow = 0;
	} else {
		chn_dmaupdate(c);
	}
}

/*
 * Feeds new data to the write dma buffer. Can be called in the bottom half.
 * Hence must be called at spltty.
 */
int
chn_wrfeed(pcm_channel *c)
{
    	snd_dbuf *b = &c->buffer;
    	snd_dbuf *bs = &c->buffer2nd;
	int a, l, lacc;

	/* ensure we always have a whole number of samples */
	a = (1 << c->align) - 1;
	lacc = 0;
    	if (c->flags & CHN_F_MAPPED) {
		bs->rl = min(b->blksz, b->fl);
		bs->fl = 0;
		a = 0;
	}
	/*
	printf("b: [rl: %d, rp %d, fl %d, fp %d]; bs: [rl: %d, rp %d, fl %d, fp %d]\n",
		b->rl, b->rp, b->fl, b->fp, bs->rl, bs->rp, bs->fl, bs->fp);
	*/
	/* Don't allow write unaligned data */
	while (bs->rl > a && b->fl > a) {
		/* ensure we always have a whole number of samples */
		l = min(min(bs->rl, bs->bufsize - bs->rp), min(b->fl, b->bufsize - b->fp)) & ~a;
		if (l == 0)
			return lacc;
		/* Move the samples, update the markers and pointers. */
		bcopy(bs->buf + bs->rp, b->buf + b->fp, l);
		bs->fl += l;
		bs->rl -= l;
		bs->rp = (bs->rp + l) % bs->bufsize;
		b->rl += l;
		b->fl -= l;
		b->fp = (b->fp + l) % b->bufsize;
		/* Clear the new space in the secondary buffer. */
		buf_clear(bs, bs->fmt, l);
		/* Accumulate the total bytes of the moved samples. */
		lacc += l;
		/* A feed to the DMA buffer is equivalent to an interrupt. */
		bs->total += l;
    		if (c->flags & CHN_F_MAPPED) {
			if (bs->total - bs->prev_total >= bs->blksz) {
				bs->prev_total = bs->total;
				bs->int_count++;
				c->blocks++;
			}
		} else
			bs->int_count++;
		if (bs->sel.si_pid && chn_polltrigger(c))
			selwakeup(&bs->sel);
	}

	return lacc;
}

/* Feeds new data to the secondary write buffer. */
static int
chn_wrfeed2nd(pcm_channel *c, struct uio *buf)
{
    	snd_dbuf *bs = &c->buffer2nd;
	int l, w, wacc;

	/* The DMA buffer may have some space. */
	while (chn_wrfeed(c) > 0);

	/* ensure we always have a whole number of samples */
	wacc = 0;
	while (buf->uio_resid > 0 && bs->fl > 0) {
		/*
		 * The size of the data to move here does not have to be
		 * aligned. We take care of it upon moving the data to a
		 * DMA buffer.
		 */
		l = min(bs->fl, bs->bufsize - bs->fp);
		/* Move the samples, update the markers and pointers. */
		w = c->feeder->feed(c->feeder, c, bs->buf + bs->fp, l, buf);
		if (w == 0)
			panic("no feed");
		bs->rl += w;
		bs->fl -= w;
		bs->fp = (bs->fp + w) % bs->bufsize;
		/* Accumulate the total bytes of the moved samples. */
		wacc += w;

		/* If any pcm data gets moved, push it to the DMA buffer. */
		if (w > 0)
			while (chn_wrfeed(c) > 0);
	}

	return wacc;
}

/*
 * Write interrupt routine. Can be called from other places (e.g.
 * to start a paused transfer), but with interrupts disabled.
 */
static void
chn_wrintr(pcm_channel *c)
{
    	snd_dbuf *b = &c->buffer;

    	if (b->underflow && !(c->flags & CHN_F_MAPPED)) {
/*		printf("underflow return\n");
*/		return; /* nothing new happened */
	}
	if (b->dl)
		chn_dmadone(c);

    	/*
	 * start another dma operation only if have ready data in the buffer,
	 * there is no pending abort, have a full-duplex device, or have a
	 * half duplex device and there is no pending op on the other side.
	 *
	 * Force transfers to be aligned to a boundary of 4, which is
	 * needed when doing stereo and 16-bit.
	 */

	/* Check underflow and update the pointers. */
	chn_checkunderflow(c);

	/*
	 * Fill up the DMA buffer, followed by waking up the top half.
	 * If some of the pcm data in uio are still left, the top half
	 * goes to sleep by itself.
	 */
	if (c->flags & CHN_F_MAPPED)
		chn_wrfeed(c);
	else {
		while (chn_wrfeed(c) > 0);
		buf_clear(b, b->fmt, b->fl);
	}
	chn_dmawakeup(c);
    	if (c->flags & CHN_F_TRIGGERED) {
		chn_dmaupdate(c);
		/*
	 	 * check if we need to reprogram the DMA on the sound card.
	 	 * This happens if the size has changed from zero
	 	 */
		if (b->dl == 0) {
			/* Start DMA operation */
	    		b->dl = b->blksz; /* record new transfer size */
	    		chn_trigger(c, PCMTRIG_START);
		}
 		/*
 		 * Emulate writing by DMA, i.e. transfer the pcm data from
 		 * the emulated-DMA buffer to the device itself.
 		 */
 		chn_trigger(c, PCMTRIG_EMLDMAWR);
		if (b->rl < b->dl) {
			DEB(printf("near underflow (%d < %d), %d\n", b->rl, b->dl, b->fl));
			/*
			 * we are near to underflow condition, so to prevent
			 * audio 'clicks' clear next b->fl bytes
			 */
			buf_clear(b, b->fmt, b->fl);
			if (b->rl < DMA_ALIGN_THRESHOLD)
				b->underflow = 1;
		}
    	} else {
		/* cannot start a new dma transfer */
		DEB(printf("underflow, flags 0x%08x rp %d rl %d\n", c->flags, b->rp, b->rl));
		if (b->dl) { /* DMA was active */
			b->underflow = 1; /* set underflow flag */
			buf_clear(b, b->fmt, b->bufsize);
		}
    	}
}

/*
 * user write routine
 *
 * advance the boundary between READY and FREE, fill the space with
 * uiomove(), and possibly start DMA. Do the above until the transfer
 * is complete.
 *
 * To minimize latency in case a pending DMA transfer is about to end,
 * we do the transfer in pieces of increasing sizes, extending the
 * READY area at every checkpoint. In the (necessary) assumption that
 * memory bandwidth is larger than the rate at which the dma consumes
 * data, we reduce the latency to something proportional to the length
 * of the first piece, while keeping the overhead low and being able
 * to feed the DMA with large blocks.
 */

int
chn_write(pcm_channel *c, struct uio *buf)
{
	int 		ret = 0, timeout, res, newsize, count;
	long		s;
	snd_dbuf       *b = &c->buffer;
	snd_dbuf       *bs = &c->buffer2nd;

	if (c->flags & CHN_F_WRITING) {
		/* This shouldn't happen and is actually silly
		 * - will never wake up, just timeout; why not sleep on b?
		 */
	       	tsleep(&s, PZERO, "pcmwrW", hz);
		return EBUSY;
	}
	c->flags |= CHN_F_WRITING;
	c->flags &= ~CHN_F_ABORTING;
	s = spltty();

	/*
	 * XXX Certain applications attempt to write larger size
	 * of pcm data than c->blocksize2nd without blocking,
	 * resulting partial write. Expand the block size so that
	 * the write operation avoids blocking.
	 */
	if ((c->flags & CHN_F_NBIO) && buf->uio_resid > bs->blksz) {
		DEB(printf("pcm warning: broken app, nbio and tried to write %d bytes with fragsz %d\n",
			buf->uio_resid, bs->blksz));
		newsize = 16;
		while (newsize < min(buf->uio_resid, CHN_2NDBUFMAXSIZE / 2))
			newsize <<= 1;
		chn_setblocksize(c, bs->blkcnt, newsize);
		DEB(printf("pcm warning: frags reset to %d x %d\n", bs->blkcnt, bs->blksz));
	}

	/*
	 * Fill up the secondary and DMA buffer.
	 * chn_wrfeed*() takes care of the alignment.
	 */

	/* Check for underflow before writing into the buffers. */
	chn_checkunderflow(c);
  	while (chn_wrfeed2nd(c, buf) > 0);
   	if ((c->flags & CHN_F_NBIO) && (buf->uio_resid > 0))
		ret = EAGAIN;

	/* Start playing if not yet. */
	if (!b->dl)
		chn_start(c);

	if (ret == 0) {
		count = hz;
		/* Wait until all samples are played in blocking mode. */
   		while ((buf->uio_resid > 0) && (count > 0)) {
			/* Check for underflow before writing into the buffers. */
			chn_checkunderflow(c);
			/* Fill up the buffers with new pcm data. */
			res = buf->uio_resid;
  			while (chn_wrfeed2nd(c, buf) > 0);
			if (buf->uio_resid < res)
				count = hz;
			else
				count--;

			/* Have we finished to feed the secondary buffer? */
			if (buf->uio_resid == 0)
				break;

			/* Wait for new free space to write new pcm samples. */
			/* splx(s); */
			timeout = 1; /*(buf->uio_resid >= b->dl)? hz / 20 : 1; */
   			ret = tsleep(b, PRIBIO | PCATCH, "pcmwr", timeout);
   			/* s = spltty(); */
 			/* if (ret == EINTR) chn_abort(c); */
 			if (ret == EINTR || ret == ERESTART)
				break;
 		}
		if (count == 0) {
			c->flags |= CHN_F_DEAD;
			device_printf(c->parent->dev, "play interrupt timeout, channel dead\n");
		}
	} else
		ret = 0;
	c->flags &= ~CHN_F_WRITING;
   	splx(s);
	return ret;
}

/*
 * SOUND INPUT
 *

The input part is similar to the output one, with a circular buffer
split in two regions, and boundaries advancing because of read() calls
[r] or dma operation [d].  At initialization, as for the write
routine, READY is empty, and FREE takes all the space.

      0          rp,rl        fp,fl    bufsize
      |__________>____________>________|
	  FREE   r   READY    d  FREE

Operation is as follows: upon user read (dsp_read_body()) a DMA read
is started if not already active (marked by b->dl > 0),
then as soon as data are available in the READY region they are
transferred to the user buffer, thus advancing the boundary between FREE
and READY. Upon interrupts, caused by a completion of a DMA transfer,
the READY region is extended and possibly a new transfer is started.

When necessary, dsp_rd_dmaupdate() is called to advance fp (and update
rl,fl accordingly). Upon user reads, rp is advanced and rl,fl are
updated accordingly.

The rules to choose the size of the new DMA area are similar to
the other case, with a preferred constant transfer size equal to
rec_blocksize, and fallback to smaller sizes if no space is available.

 */

static int
chn_rddump(pcm_channel *c, int cnt)
{
    	snd_dbuf *b = &c->buffer;
	int maxover, ss;

	ss = 1;
	ss <<= (b->fmt & AFMT_STEREO)? 1 : 0;
	ss <<= (b->fmt & AFMT_16BIT)? 1 : 0;
	maxover = c->speed * ss;

	b->overrun += cnt;
	if (b->overrun > maxover) {
		device_printf(c->parent->dev, "record overrun, dumping %d bytes\n",
			b->overrun);
		b->overrun = 0;
	}
	b->rl -= cnt;
	b->fl += cnt;
	b->rp = (b->rp + cnt) % b->bufsize;
	return cnt;
}

/*
 * Feed new data from the read buffer. Can be called in the bottom half.
 * Hence must be called at spltty.
 */
int
chn_rdfeed(pcm_channel *c)
{
    	snd_dbuf *b = &c->buffer;
    	snd_dbuf *bs = &c->buffer2nd;
	int l, lacc;

	/*
	printf("b: [rl: %d, rp %d, fl %d, fp %d]; bs: [rl: %d, rp %d, fl %d, fp %d]\n",
		b->rl, b->rp, b->fl, b->fp, bs->rl, bs->rp, bs->fl, bs->fp);
	 */
	/* ensure we always have a whole number of samples */
	lacc = 0;
	while (bs->fl >= DMA_ALIGN_THRESHOLD && b->rl >= DMA_ALIGN_THRESHOLD) {
		l = min(min(bs->fl, bs->bufsize - bs->fp), min(b->rl, b->bufsize - b->rp)) & DMA_ALIGN_MASK;
		/* Move the samples, update the markers and pointers. */
		bcopy(b->buf + b->rp, bs->buf + bs->fp, l);
		bs->fl -= l;
		bs->rl += l;
		bs->fp = (bs->fp + l) % bs->bufsize;
		b->rl -= l;
		b->fl += l;
		b->rp = (b->rp + l) % b->bufsize;
		/* Accumulate the total bytes of the moved samples. */
		lacc += l;
		/* A feed from the DMA buffer is equivalent to an interrupt. */
		bs->int_count++;
		if (bs->sel.si_pid && chn_polltrigger(c))
			selwakeup(&bs->sel);
	}

	return lacc;
}

/* Feeds new data from the secondary read buffer. */
static int
chn_rdfeed2nd(pcm_channel *c, struct uio *buf)
{
    	snd_dbuf *bs = &c->buffer2nd;
	int l, w, wacc;

	/* ensure we always have a whole number of samples */
	wacc = 0;
	while ((buf->uio_resid > 0) && (bs->rl > 0)) {
		/* The DMA buffer may have pcm data. */
		/* while (chn_rdfeed(c) > 0); */
		/*
		 * The size of the data to move here does not have to be
		 * aligned. We take care of it upon moving the data to a
		 * DMA buffer.
		 */
		l = min(bs->rl, bs->bufsize - bs->rp);
		/* Move the samples, update the markers and pointers. */
		w = c->feeder->feed(c->feeder, c, bs->buf + bs->rp, l, buf);
		if (w == 0)
			panic("no feed");
		bs->fl += w;
		bs->rl -= w;
		bs->rp = (bs->rp + w) % bs->bufsize;
		/* Clear the new space in the secondary buffer. */
		buf_clear(bs, bs->fmt, l);
		/* Accumulate the total bytes of the moved samples. */
		bs->total += w;
		wacc += w;
	}

	return wacc;
}

/* read interrupt routine. Must be called with interrupts blocked. */
static void
chn_rdintr(pcm_channel *c)
{
    	snd_dbuf *b = &c->buffer;

    	if (b->dl) chn_dmadone(c);

    	DEB(printf("rdintr: start dl %d, rp:rl %d:%d, fp:fl %d:%d\n",
		b->dl, b->rp, b->rl, b->fp, b->fl));

	/* Update the pointers. */
	chn_dmaupdate(c);

	/*
	 * Suck up the DMA buffer, followed by waking up the top half.
	 * If some of the pcm data in the secondary buffer are still left,
	 * the top half goes to sleep by itself.
	 */
	while(chn_rdfeed(c) > 0);
	chn_dmawakeup(c);

	if (b->fl < b->dl) {
		DEB(printf("near overflow (%d < %d), %d\n", b->fl, b->dl, b->rl));
		chn_rddump(c, b->blksz - b->fl);
	}

	if (c->flags & CHN_F_TRIGGERED) {
		/*
	 	 * check if we need to reprogram the DMA on the sound card.
	 	 * This happens if the size has changed from zero
	 	 */
		if (b->dl == 0) {
			/* Start DMA operation */
	    		b->dl = b->blksz; /* record new transfer size */
	    		chn_trigger(c, PCMTRIG_START);
		}
 		/*
 		 * Emulate writing by DMA, i.e. transfer the pcm data from
 		 * the emulated-DMA buffer to the device itself.
 		 */
 		chn_trigger(c, PCMTRIG_EMLDMARD);
    	} else {
		if (b->dl) { /* was active */
	    		b->dl = 0;
	    		chn_trigger(c, PCMTRIG_STOP);
	    		chn_dmaupdate(c);
		}
    	}
}

/*
 * body of user-read routine
 *
 * Start DMA if not active; wait for READY not empty.
 * Transfer data from READY region using uiomove(), advance boundary
 * between FREE and READY. Repeat until transfer is complete.
 *
 * To avoid excessive latency in freeing up space for the DMA
 * engine, transfers are done in blocks of increasing size, so that
 * the latency is proportional to the size of the smallest block, but
 * we have a low overhead and are able to feed the dma engine with
 * large blocks.
 *
 * NOTE: in the current version, read will not return more than
 * blocksize bytes at once (unless more are already available), to
 * avoid that requests using very large buffers block for too long.
 */

int
chn_read(pcm_channel *c, struct uio *buf)
{
	int		ret = 0, timeout, limit, res, count;
	long		s;
	snd_dbuf       *b = &c->buffer;
	snd_dbuf       *bs = &c->buffer2nd;

	if (c->flags & CHN_F_READING) {
		/* This shouldn't happen and is actually silly */
		tsleep(&s, PZERO, "pcmrdR", hz);
		return (EBUSY);
	}

  	s = spltty();

	/* Store the initial size in the uio. */
	res = buf->uio_resid;

	c->flags |= CHN_F_READING;
	c->flags &= ~CHN_F_ABORTING;

	/* suck up the DMA and secondary buffers. */
 	while (chn_rdfeed2nd(c, buf) > 0);

	if (buf->uio_resid == 0)
		goto skip;

	limit = res - b->blksz;
	if (limit < 0)
		limit = 0;

	/* Start capturing if not yet. */
  	if ((!bs->rl || !b->rl) && !b->dl)
		chn_start(c);

  	if (!(c->flags & CHN_F_NBIO)) {
		count = hz;
  		/* Wait until all samples are captured. */
  		while ((buf->uio_resid > 0) && (count > 0)) {
			/* Suck up the DMA and secondary buffers. */
			chn_dmaupdate(c);
			res = buf->uio_resid;
			while (chn_rdfeed(c) > 0);
 			while (chn_rdfeed2nd(c, buf) > 0);
			if (buf->uio_resid < res)
				count = hz;
			else
				count--;

			/* Have we finished to feed the uio? */
			if (buf->uio_resid == 0)
				break;

			/* Wait for new pcm samples. */
			/* splx(s); */
			timeout = (buf->uio_resid - limit >= b->dl)? hz / 20 : 1;
  			ret = tsleep(b, PRIBIO | PCATCH, "pcmrd", 1);
  			/* s = spltty(); */
			/* if (ret == EINTR) chn_abort(c); */
			if (ret == EINTR || ret == ERESTART)
				break;
		}
		if (count == 0) {
			c->flags |= CHN_F_DEAD;
			device_printf(c->parent->dev, "record interrupt timeout, channel dead\n");
		}
	} else {
		/* If no pcm data was read on nonblocking, return EAGAIN. */
		if (buf->uio_resid == res)
			ret = EAGAIN;
	}

skip:
	c->flags &= ~CHN_F_READING;
  	splx(s);
	return ret;
}

void
chn_intr(pcm_channel *c)
{
	if (c->flags & CHN_F_INIT)
		chn_reinit(c);
	if (c->direction == PCMDIR_PLAY)
		chn_wrintr(c);
	else
		chn_rdintr(c);
}

u_int32_t
chn_start(pcm_channel *c)
{
	u_int32_t r, s;
	snd_dbuf *b = &c->buffer;

	r = 0;
	s = spltty();
    	if (b->dl == 0 && !(c->flags & (CHN_F_MAPPED | CHN_F_NOTRIGGER))) {
		if (c->direction == PCMDIR_PLAY) {
			/* Fill up the DMA buffer. */
			while (chn_wrfeed(c) > 0);
			if (b->rl >= b->blksz)
				r = CHN_F_TRIGGERED;
		} else {
			/* Suck up the DMA buffer. */
			while (chn_rdfeed(c) > 0);
			if (b->fl >= b->blksz)
				r = CHN_F_TRIGGERED;
		}
		c->flags |= r;
		chn_intr(c);
	}
	splx(s);
	return r;
}

static void
chn_dma_setmap(void *arg, bus_dma_segment_t *segs, int nseg, int error)
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
 * the drvier can call malloc(9) by its own.
 */
int
chn_allocbuf(snd_dbuf *b, bus_dma_tag_t parent_dmat)
{
	b->parent_dmat = parent_dmat;
	if (bus_dmamem_alloc(b->parent_dmat, (void **)&b->buf,
			     BUS_DMA_NOWAIT, &b->dmamap)) return -1;
	if (bus_dmamap_load(b->parent_dmat, b->dmamap, b->buf,
			    b->bufsize, chn_dma_setmap, b, 0)) return -1;
	return 0;
}

void
chn_freebuf(snd_dbuf *b)
{
	bus_dmamem_free(b->parent_dmat, b->buf, b->dmamap);
}

static void
buf_clear(snd_dbuf *b, u_int32_t fmt, int length)
{
	int i;
	u_int16_t data, *p;

	if (length == 0)
		return;

	if (fmt & AFMT_SIGNED)
		data = 0x00;
	else
		data = 0x80;

	if (fmt & AFMT_16BIT)
		data <<= 8;
	else
		data |= data << 8;

	if (fmt & AFMT_BIGENDIAN)
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
chn_resetbuf(pcm_channel *c)
{
	snd_dbuf *b = &c->buffer;
	snd_dbuf *bs = &c->buffer2nd;

	c->blocks = 0;
	b->rp = b->fp = 0;
	b->dl = b->rl = 0;
	b->fl = b->bufsize;
	b->prev_total = b->total = 0;
	b->prev_int_count = b->int_count = 0;
	b->underflow = 0;
	if (b->buf && b->bufsize > 0)
		buf_clear(b, b->fmt, b->bufsize);

	bs->rp = bs->fp = 0;
	bs->dl = bs->rl = 0;
	bs->fl = bs->bufsize;
	bs->prev_total = bs->total = 0;
	bs->prev_int_count = bs->int_count = 0;
	bs->underflow = 0;
	if (bs->buf && bs->bufsize > 0)
		buf_clear(bs, bs->fmt, bs->bufsize);
}

void
buf_isadma(snd_dbuf *b, int go)
{
	if (ISA_DMA(b)) {
		switch (go) {
		case PCMTRIG_START:
			DEB(printf("buf 0x%p ISA DMA started\n", b));
			isa_dmastart(b->dir | ISADMA_RAW, b->buf,
					b->bufsize, b->chan);
			break;
		case PCMTRIG_STOP:
		case PCMTRIG_ABORT:
			DEB(printf("buf 0x%p ISA DMA stopped\n", b));
			isa_dmastop(b->chan);
			isa_dmadone(b->dir | ISADMA_RAW, b->buf, b->bufsize,
				    b->chan);
			break;
		}
    	} else KASSERT(1, ("buf_isadma called on invalid channel"));
}

int
buf_isadmaptr(snd_dbuf *b)
{
	if (ISA_DMA(b)) {
		int i = b->dl? isa_dmastatus(b->chan) : b->bufsize;
		if (i < 0)
			i = 0;
		return b->bufsize - i;
    	} else KASSERT(1, ("buf_isadmaptr called on invalid channel"));
	return -1;
}

/*
 * chn_sync waits until the space in the given channel goes above
 * a threshold. The threshold is checked against fl or rl respectively.
 * Assume that the condition can become true, do not check here...
 */
int
chn_sync(pcm_channel *c, int threshold)
{
    	u_long s, rdy;
    	int ret;
    	snd_dbuf *b = &c->buffer;
    	snd_dbuf *bs = &c->buffer2nd;

    	for (;;) {
		s = spltty();
		chn_checkunderflow(c);
		while (chn_wrfeed(c) > 0);
		rdy = (c->direction == PCMDIR_PLAY)? bs->fl : bs->rl;
		if (rdy <= threshold) {
	    		ret = tsleep((caddr_t)b, PRIBIO | PCATCH, "pcmsyn", 1);
	    		splx(s);
	    		if (ret == ERESTART || ret == EINTR) {
				DEB(printf("chn_sync: tsleep returns %d\n", ret));
				return -1;
	    		}
		} else break;
    	}
    	splx(s);
    	return 0;
}

int
chn_poll(pcm_channel *c, int ev, struct proc *p)
{
	snd_dbuf *b = &c->buffer;
	snd_dbuf *bs = &c->buffer2nd;
	u_long s;
	int ret;

	s = spltty();
    	if (!(c->flags & CHN_F_MAPPED)) {
		if (c->direction == PCMDIR_PLAY) {
			/* Fill up the DMA buffer. */
			chn_checkunderflow(c);
			while (chn_wrfeed(c) > 0);
		} else {
			/* Suck up the DMA buffer. */
			chn_dmaupdate(c);
			while (chn_rdfeed(c) > 0);
		}
		if (!b->dl)
			chn_start(c);
	}
	ret = 0;
	if (chn_polltrigger(c) && chn_pollreset(c))
		ret = ev;
	else
		selrecord(p, &bs->sel);
	splx(s);
	return ret;
}

/*
 * chn_abort is a non-blocking function which aborts a pending
 * DMA transfer and flushes the buffers.
 * It returns the number of bytes that have not been transferred.
 */
int
chn_abort(pcm_channel *c)
{
    	int missing = 0, cnt = 0;
    	snd_dbuf *b = &c->buffer;
    	snd_dbuf *bs = &c->buffer2nd;

	if (!b->dl)
		return 0;
	c->flags |= CHN_F_ABORTING;
	c->flags &= ~CHN_F_TRIGGERED;
	cnt = 10;
	while (!b->underflow && (cnt-- > 0))
		tsleep((caddr_t)b, PRIBIO | PCATCH, "pcmabr", hz / 50);
	chn_trigger(c, PCMTRIG_ABORT);
	b->dl = 0;
	chn_dmaupdate(c);
    	missing = bs->rl + b->rl;
    	return missing;
}

/*
 * this routine tries to flush the dma transfer. It is called
 * on a close. We immediately abort any read DMA
 * operation, and then wait for the play buffer to drain.
 */

int
chn_flush(pcm_channel *c)
{
    	int ret, count, s, resid, resid_p;
    	snd_dbuf *b = &c->buffer;
    	snd_dbuf *bs = &c->buffer2nd;

    	DEB(printf("chn_flush c->flags 0x%08x\n", c->flags));
    	c->flags |= CHN_F_CLOSING;
    	if (c->direction == PCMDIR_REC)
		chn_abort(c);
    	else if (b->dl) {
		resid_p = resid = b->rl + bs->rl;
		count = 10;
		while ((count > 0) && (resid > 0) && !b->underflow) {
			/* still pending output data. */
			ret = tsleep((caddr_t)b, PRIBIO | PCATCH, "pcmflu", hz / 10);
			if (ret == EINTR || ret == ERESTART) {
	    			DEB(printf("chn_flush: tsleep returns %d\n", ret));
	    			return ret;
			}
 			s = spltty();
			chn_dmaupdate(c);
			splx(s);
			DEB(printf("chn_flush: now rl = %d, fl = %d\n", b->rl, b->fl));
			resid = b->rl + bs->rl;
			if (resid >= resid_p)
				count--;
			resid_p = resid;
   		}
		if (count == 0)
			DEB(printf("chn_flush: timeout flushing dbuf_out, cnt 0x%x flags 0x%x\n", b->rl, c->flags));
    		if (c->direction == PCMDIR_PLAY && b->dl)
			chn_abort(c);
	}
    	c->flags &= ~CHN_F_CLOSING;
    	return 0;
}

int
chn_reset(pcm_channel *c, u_int32_t fmt)
{
	int r = 0;

	chn_abort(c);
	c->flags &= CHN_F_RESET;
	r = chn_setblocksize(c, CHN_2NDBUFBLKNUM, CHN_2NDBUFBLKSIZE);
	if (r)
		return r;
	if (fmt) {
		r = chn_setformat(c, fmt);
		if (r == 0)
			r = chn_setspeed(c, DSP_DEFAULT_SPEED);
		if (r == 0)
			r = chn_setvolume(c, 100, 100);
	}
	chn_resetbuf(c);
	/* c->flags |= CHN_F_INIT; */
	return 0;
}

int
chn_reinit(pcm_channel *c)
{
	if ((c->flags & CHN_F_INIT) && CANCHANGE(c)) {
		chn_setformat(c, c->format);
		chn_setspeed(c, c->speed);
		chn_setvolume(c, (c->volume >> 8) & 0xff, c->volume & 0xff);
		c->flags &= ~CHN_F_INIT;
		return 1;
	}
	return 0;
}

int
chn_init(pcm_channel *c, void *devinfo, int dir)
{
	snd_dbuf       *bs = &c->buffer2nd;

	/* Initialize the hardware and DMA buffer first. */
	c->feeder = malloc(sizeof(*(c->feeder)), M_DEVBUF, M_NOWAIT);
	*(c->feeder) = *feeder_getroot();
	c->feederdesc = malloc(sizeof(*(c->feeder)), M_DEVBUF, M_NOWAIT);
	c->feederdesc->type = FEEDER_ROOT;
	c->feederdesc->in = 0;
	c->feederdesc->out = 0;
	c->feederdesc->flags = 0;
	c->feederdesc->idx = 0;
	c->feeder->desc = c->feederdesc;
	c->feeder->source = NULL;

	c->flags = 0;
	c->buffer.chan = -1;
	c->devinfo = c->init(devinfo, &c->buffer, c, dir);
	if (c->devinfo == NULL || c->buffer.bufsize == 0)
		return 1;
	chn_setdir(c, dir);

	/* And the secondary buffer. */
	bs->buf = NULL;
	bs->bufsize = 0;
	return 0;
}

int
chn_kill(pcm_channel *c)
{
	if (c->flags & CHN_F_TRIGGERED)
		chn_trigger(c, PCMTRIG_ABORT);
	while (chn_removefeeder(c) == 0);
	free(c->feeder->desc, M_DEVBUF);
	free(c->feeder, M_DEVBUF);
	if (c->free)
		c->free(c->devinfo);
	else
		chn_freebuf(&c->buffer);
	c->flags |= CHN_F_DEAD;
	return 0;
}

int
chn_setdir(pcm_channel *c, int dir)
{
	int r;

	c->direction = dir;
	r = c->setdir? c->setdir(c->devinfo, c->direction) : 0;
	if (!r && ISA_DMA(&c->buffer))
		c->buffer.dir = (dir == PCMDIR_PLAY)? ISADMA_WRITE : ISADMA_READ;
	return r;
}

int
chn_setvolume(pcm_channel *c, int left, int right)
{
	/* could add a feeder for volume changing if channel returns -1 */
	if (CANCHANGE(c)) {
		c->volume = (left << 8) | right;
		return 0;
	}
	c->volume = (left << 8) | right;
	c->flags |= CHN_F_INIT;
	return 0;
}

int
chn_setspeed(pcm_channel *c, int speed)
{
	if (speed <= 0)
		return EINVAL;
	/* could add a feeder for rate conversion */
	if (CANCHANGE(c)) {
		c->speed = c->setspeed(c->devinfo, speed);
		return 0;
	}
	c->speed = speed;
	c->flags |= CHN_F_INIT;
	return 0;
}

int
fmtvalid(u_int32_t fmt, u_int32_t *fmtlist)
{
	int i;

	for (i = 0; fmtlist[i]; i++)
		if (fmt == fmtlist[i])
			return 1;

	return 0;
}

int
chn_setformat(pcm_channel *c, u_int32_t fmt)
{
	snd_dbuf *b = &c->buffer;
	snd_dbuf *bs = &c->buffer2nd;

	u_int32_t hwfmt;
	if (CANCHANGE(c)) {
		while (chn_removefeeder(c) == 0);
		c->format = fmt;
		c->feederdesc->out = c->format;
		hwfmt = c->format;
		if (!fmtvalid(hwfmt, chn_getcaps(c)->fmtlist)) {
			if (c->flags & CHN_F_MAPPED)
				return EINVAL;
			hwfmt = chn_feedchain(c, chn_getcaps(c)->fmtlist);
			if (hwfmt == 0)
				return EINVAL;
		}
		b->fmt = hwfmt;
		bs->fmt = hwfmt;
		chn_resetbuf(c);
		c->setformat(c->devinfo, hwfmt);
		return 0;
	}
	c->format = fmt;
	c->flags |= CHN_F_INIT;
	return 0;
}

int
chn_setblocksize(pcm_channel *c, int blkcnt, int blksz)
{
	snd_dbuf *b = &c->buffer;
	snd_dbuf *bs = &c->buffer2nd;
	int s, ss, bufsz;

	if (bs->blkcnt == blkcnt && bs->blksz == blksz)
		return 0;
    	if (c->flags & CHN_F_MAPPED) {
		DEB(printf("chn_setblocksize: can't work on mapped channel"));
		return EINVAL;
	}
	c->flags &= ~CHN_F_HAS_SIZE;

	ss = 1;
	ss <<= (bs->fmt & AFMT_STEREO)? 1 : 0;
	ss <<= (bs->fmt & AFMT_16BIT)? 1 : 0;

	if (blksz >= 2)
		c->flags |= CHN_F_HAS_SIZE;
	/* let us specify blksz without setting CHN_F_HAS_SIZE */
	if (blksz < 0)
		blksz = -blksz;
	/* default to blksz = ~0.25s */
	if (blksz < 16)
		blksz = (ss * c->speed) >> 2;
	if (blksz > CHN_2NDBUFMAXSIZE / 2)
		blksz = CHN_2NDBUFMAXSIZE / 2;
	if (blkcnt < 2)
		blkcnt = 2;

	if (blkcnt * blksz > CHN_2NDBUFMAXSIZE)
		blkcnt = CHN_2NDBUFMAXSIZE / blksz;
	bufsz = blkcnt * blksz;

	s = spltty();
	if (bs->buf != NULL)
		free(bs->buf, M_DEVBUF);
	bs->buf = malloc(bufsz, M_DEVBUF, M_WAITOK);
	if (bs->buf == NULL) {
      		splx(s);
		DEB(printf("chn_setblocksize: out of memory."));
		return ENOSPC;
	}
	bs->bufsize = bufsz;
	bs->rl = bs->rp = bs->fp = 0;
	bs->fl = bs->bufsize;
	buf_clear(bs, bs->fmt, bs->bufsize);
	bs->blkcnt = blkcnt;
	bs->blksz = blksz;
	RANGE(blksz, 16, b->bufsize / 2);
	b->blksz = c->setblocksize(c->devinfo, blksz);
	splx(s);

	return 0;
}

int
chn_trigger(pcm_channel *c, int go)
{
	return c->trigger(c->devinfo, go);
}

int
chn_getptr(pcm_channel *c)
{
	int hwptr;
	int a = (1 << c->align) - 1;
	snd_dbuf *b = &c->buffer;

	hwptr = b->dl? c->getptr(c->devinfo) : 0;
	/* don't allow unaligned values in the hwa ptr */
	hwptr &= ~a ; /* Apply channel align mask */
	hwptr &= DMA_ALIGN_MASK; /* Apply DMA align mask */
	return hwptr;
}

pcmchan_caps *
chn_getcaps(pcm_channel *c)
{
	return c->getcaps(c->devinfo);
}

u_int32_t
chn_getformats(pcm_channel *c)
{
	u_int32_t *fmtlist, fmts;
	int i;

	fmtlist = chn_getcaps(c)->fmtlist;
	fmts = 0;
	for (i = 0; fmtlist[i]; i++)
		fmts |= fmtlist[i];

	return fmts;
}


