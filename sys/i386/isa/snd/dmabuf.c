/*
 * snd/dmabuf.c
 * 
 * This file implements the new DMA routines for the sound driver.
 * AUTO DMA MODE (ISA DMA SIDE).
 *
 * Copyright by Luigi Rizzo - 1997
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <i386/isa/snd/sound.h>
#include <i386/isa/sound/ulaw.h>

#define MIN_CHUNK_SIZE 256	/* for uiomove etc. */
#define	DMA_ALIGN_THRESHOLD	4
#define	DMA_ALIGN_MASK		(~ (DMA_ALIGN_THRESHOLD - 1))

static void dsp_wr_dmadone(snddev_info *d);
static void dsp_rd_dmadone(snddev_info *d);

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

The sistem tries to make all DMA transfers use the same size,
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


/*
 * dsp_wr_dmadone() updates pointers and wakes up any process sleeping
 * or waiting on a select().
 * Must be called at spltty().
 */
static void
dsp_wr_dmadone(snddev_info *d)
{
    snd_dbuf *b = & (d->dbuf_out) ;

    dsp_wr_dmaupdate(b);
    /*
     * XXX here it would be more efficient to record if there
     * actually is a sleeping process, but this should still work.
     */
    wakeup(b); 	/* wakeup possible sleepers */
    if (b->sel.si_pid &&
	    ( !(d->flags & SND_F_HAS_SIZE) || b->fl >= d->play_blocksize ) )
	selwakeup( & b->sel );
}

/*
 * dsp_wr_dmaupdate() tracks the status of a (write) dma transfer,
 * updating pointers. It must be called at spltty() and the ISA DMA must
 * have been started.
 *
 * NOTE: when we are using auto dma in the device, rl might become
 *  negative.
 */
void
dsp_wr_dmaupdate(snd_dbuf *b)
{
    int tmp, delta;

    tmp = b->bufsize - isa_dmastatus1(b->chan) ;
    tmp &= DMA_ALIGN_MASK; /* align... */
    delta = tmp - b->rp;
    if (delta < 0) /* wrapped */
	delta += b->bufsize ;
    b->rp = tmp;
    b->rl -= delta ;
    b->fl += delta ;
    b->total += delta ;
}

/*
 * Write interrupt routine. Can be called from other places (e.g.
 * to start a paused transfer), but with interrupts disabled.
 */
void
dsp_wrintr(snddev_info *d)
{
    snd_dbuf *b = & (d->dbuf_out) ;

    if (b->dl) {		/* dma was active */
	b->int_count++;
	dsp_wr_dmadone(d);
    }

    DEB(if (b->rl < 0)
	printf("dsp_wrintr: dl %d, rp:rl %d:%d, fp:fl %d:%d\n",
	    b->dl, b->rp, b->rl, b->fp, b->fl));
    /*
     * start another dma operation only if have ready data in the buffer,
     * there is no pending abort, have a full-duplex device, or have a
     * half duplex device and there is no pending op on the other side.
     *
     * Force transfers to be aligned to a boundary of 4, which is
     * needed when doing stereo and 16-bit. We could make this
     * adaptive, but why bother for now...
     */
    if (  b->rl >= DMA_ALIGN_THRESHOLD  &&
	  ! (d->flags & SND_F_ABORTING) &&
	  ( FULL_DUPLEX(d) || ! (d->flags & SND_F_READING)  ) ) {
	int l = min(b->rl, d->play_blocksize );	/* avoid too large transfer */
	l &= DMA_ALIGN_MASK ; /* realign things */

	/*
	 * check if we need to reprogram the DMA on the sound card.
	 * This happens if the size has changed _and_ the new size
	 * is smaller, or it matches the blocksize.
	 */
	if (l != b->dl && (l < b->dl || l == d->play_blocksize) ) {
	    /* for any reason, size has changed. Stop and restart */
	    DEB(printf("wrintr: bsz change from %d to %d, rp %d rl %d\n",
		b->dl, l, b->rp, b->rl));
	    d->callback(d, SND_CB_WR | SND_CB_STOP );
	    /*
	     * at high speed, it might well be that the count
	     * changes in the meantime. So we try to update b->rl
	     */
	    dsp_wr_dmaupdate(b) ;
	    l = min(b->rl, d->play_blocksize );
	    l &= DMA_ALIGN_MASK ; /* realign things */
	    b->dl = l; /* record previous transfer size */
	    d->callback(d, SND_CB_WR | SND_CB_START );
	}
    } else {
	/* cannot start a new dma transfer */
	DEB(printf("cannot start wr-dma flags 0x%08x rp %d rl %d\n",
		d->flags, b->rp, b->rl));
	if (b->dl > 0) { /* was active */
	    b->dl = 0;
	    d->callback(d, SND_CB_WR | SND_CB_STOP );	/* stop dma */
	    if (d->flags & SND_F_WRITING)
		DEB(printf("Race! got wrint while reloading...\n"));
	    else if (b->rl <= 0) /* XXX added 980110 lr */
		reset_dbuf(b, SND_CHAN_WR);
	}
	/*
	 * if switching to read, should start the read dma...
	 */
	if ( !FULL_DUPLEX(d) && (d->flags & SND_F_READING) )
	    dsp_rdintr(d);
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
 *
 * assume d->flags |= SND_F_WRITING ; has been done before
 */

int
dsp_write_body(snddev_info *d, struct uio *buf)
{
    int n, l, bsz, ret = 0 ;
    long s;
    snd_dbuf *b = & (d->dbuf_out) ;

    /*
     * bsz is the max size for the next transfer. If the dma was idle
     * (dl == 0), we want it as large as possible. Otherwise, start with
     * a small block to avoid underruns if we are close to the end of
     * the previous operation.
     */
    bsz =  b->dl ? MIN_CHUNK_SIZE : b->bufsize ;
    while ( (n = buf->uio_resid) ) {
        l = min (n, bsz);       /* at most n bytes ... */
        s = spltty();  /* no interrupts here ... */
	dsp_wr_dmaupdate(b);
        l = min( l, b->fl );    /* no more than avail. space */
	DEB(printf("dsp_write_body: prepare %d bytes out of %d\n", l,n));
	/*
	 * at this point, we assume that if l==0 the dma engine
	 * must be running.
	 */
        if (l == 0) { /* no space, must sleep */
	    int timeout;
	    if (d->flags & SND_F_NBIO) {
		/* unless of course we are doing non-blocking i/o */
		splx(s);
		break;
	    }
	    DEB(printf("dsp_write_body: l=0, (fl %d) sleeping\n", b->fl));
	    if ( b->fl < n )
		timeout = hz;
	    else
		timeout = 1 ;
            ret = tsleep( (caddr_t)b, PRIBIO|PCATCH, "dspwr", timeout);
	    if (ret == EINTR)
		d->flags |= SND_F_ABORTING ;
	    splx(s);
	    if (ret == EINTR || ret == ERESTART)
		break ;
            continue;
        }
        splx(s);

	/*
	 * copy data to the buffer, and possibly do format
	 * conversions (here, from ULAW to U8).
	 * NOTE: I can use fp here since it is not modified by the
	 * interrupt routines.
	 */
	if (b->fp + l > b->bufsize) {
	    int l1 = b->bufsize - b->fp ;
	    uiomove(b->buf + b->fp, l1, buf) ;
	    uiomove(b->buf, l - l1, buf) ;
	    if (d->flags & SND_F_XLAT8) {
		translate_bytes(ulaw_dsp, b->buf + b->fp, l1);
		translate_bytes(ulaw_dsp, b->buf , l - l1);
	    }
	} else {
	    uiomove(b->buf + b->fp, l, buf) ;
	    if (d->flags & SND_F_XLAT8)
		translate_bytes(ulaw_dsp, b->buf + b->fp, l);
	}

        s = spltty();  /* no interrupts here ... */
        b->rl += l ;    /* this more ready bytes */
        b->fl -= l ;    /* this less free bytes */
        b->fp += l ;
        if (b->fp >= b->bufsize)        /* handle wraps */
            b->fp -= b->bufsize ;
        if ( b->dl == 0 ) /* dma was idle, restart it */
            dsp_wrintr(d) ;
        splx(s) ;
	if (buf->uio_resid == 0 && (b->fp & (b->sample_size - 1)) == 0) {
	    /*
	     * If data is correctly aligned, pad the region with
	     * replicas of the last sample. l0 goes from current to
	     * the buffer end, l1 is the portion which wraps around.
	     */
	    int l0, l1, i;

	    l1 = min(/* b->dl */ d->play_blocksize, b->fl);
	    l0 = min (l1, b->bufsize - b->fp);
	    l1 = l1 - l0 ;

	    i = b->fp - b->sample_size;
	    if (i < 0 ) i += b->bufsize ;
	    if (b->sample_size == 1) {
		u_char *p= (u_char *)(b->buf + i), sample = *p;
		
		for ( ; l0 ; l0--)
		    *p++ = sample ;
		for (p= (u_char *)(b->buf) ; l1 ; l1--)
		    *p++ = sample ;
	    } else if (b->sample_size == 2) {
		u_short *p= (u_short *)(b->buf + i), sample = *p;

		l1 /= 2 ;
		l0 /= 2 ;
		for ( ; l0 ; l0--)
		    *p++ = sample ;
		for (p= (u_short *)(b->buf) ; l1 ; l1--)
		    *p++ = sample ;
	    } else { /* must be 4 ... */
		u_long *p= (u_long *)(b->buf + i), sample = *p;

		l1 /= 4 ;
		l0 /= 4 ;
		for ( ; l0 ; l0--)
		    *p++ = sample ;
		for (p= (u_long *)(b->buf) ; l1 ; l1--)
		    *p++ = sample ;
	    }

	}
        bsz = min(b->bufsize, bsz*2);
    }
    s = spltty();  /* no interrupts here ... */
    d->flags &= ~SND_F_WRITING ;
    if (d->flags & SND_F_ABORTING) {
        d->flags &= ~SND_F_ABORTING;
	splx(s);
	dsp_wrabort(d, 1 /* restart */);
	/* XXX return EINTR ? */
    }
    splx(s) ;
    return ret ;
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

 *
 */

/*
 * dsp_rd_dmadone moves bytes in the input buffer from DMA region to
 * READY region. We assume it is called at spltty() and  with dl>0 
 */
static void
dsp_rd_dmadone(snddev_info *d)
{
    snd_dbuf *b = & (d->dbuf_in) ;

    dsp_rd_dmaupdate(b);
    wakeup(b) ;	/* wakeup possibly sleeping processes */
    if (b->sel.si_pid &&
	    ( !(d->flags & SND_F_HAS_SIZE) || b->rl >= d->rec_blocksize ) )
	selwakeup( & b->sel );
}

/*
 * The following function tracks the status of a (read) dma transfer,
 * and moves the boundary between the READY and the DMA regions.
 * It works under the following assumptions:
 *   - the DMA engine is running;
 *   - the function is called with interrupts blocked.
 */
void
dsp_rd_dmaupdate(snd_dbuf *b)
{
    int delta, tmp ;

    tmp = b->bufsize - isa_dmastatus1(b->chan) ;
    tmp &= DMA_ALIGN_MASK; /* align... */
    delta = tmp - b->fp;
    if (delta < 0) /* wrapped */
	delta += b->bufsize ;
    b->fp = tmp;
    b->fl -= delta ;
    b->rl += delta ;
    b->total += delta ;
}

/*
 * read interrupt routine. Must be called with interrupts blocked.
 */
void
dsp_rdintr(snddev_info *d)
{
    snd_dbuf *b = & (d->dbuf_in) ;

    if (b->dl) {		/* dma was active */
	b->int_count++;
	dsp_rd_dmadone(d);
    }

    DEB(printf("dsp_rdintr: start dl %d, rp:rl %d:%d, fp:fl %d:%d\n",
	b->dl, b->rp, b->rl, b->fp, b->fl));
    /*
     * Restart if have enough free space to absorb overruns;
     */
    if ( b->fl > 0x200 &&
         (d->flags & (SND_F_ABORTING|SND_F_CLOSING)) == 0 &&
	 ( FULL_DUPLEX(d) || (d->flags & SND_F_WRITING) == 0 ) ) {
	int l = min(b->fl - 0x100, d->rec_blocksize);
	l &= DMA_ALIGN_MASK ; /* realign sizes */
	if (l != b->dl) {
	    /* for any reason, size has changed. Stop and restart */
	    b->dl = l ;
	    d->callback(d, SND_CB_RD | SND_CB_STOP );
	    d->callback(d, SND_CB_RD | SND_CB_START );
	}
    } else {
	if (b->dl > 0) { /* was active */
	    b->dl = 0;
	    d->callback(d, SND_CB_RD | SND_CB_STOP);
	}
	/*
	 * if switching to write, start write dma engine
	 */
	if ( ! FULL_DUPLEX(d) && (d->flags & SND_F_WRITING) )
	    dsp_wrintr(d) ;
	DEB(printf("cannot start rd-dma rl %d fl %d\n",
		b->rl, b->fl));
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
 * When we enter this routine, we assume that d->flags |= SND_F_READING
 * was done before.
 *
 * NOTE: in the current version, read will not return more than
 * blocksize bytes at once (unless more are already available), to
 * avoid that requests using very large buffers block for too long.
 */

int
dsp_read_body(snddev_info *d, struct uio *buf)
{
    int limit, l, n, bsz, ret = 0 ;
    long s;
    snd_dbuf *b = & (d->dbuf_in) ;

    /*
     * "limit" serves to return after at most one blocksize of data
     * (unless more are already available).  Otherwise, things like
     * cat /dev/audio would use a 64K buffer and would start returning
     * data after a _very_ long time...
     * Note -- some applications depend on reads not returning short
     * blocks. But I believe these apps are broken, since interrupted
     * system calls might return short reads anyways, and the
     * application should better check that.
     */

    if (buf->uio_resid > d->rec_blocksize)
	limit = buf->uio_resid - d->rec_blocksize;
    else
	limit = 0;
    bsz = MIN_CHUNK_SIZE ;  /* the current transfer (doubles at each step) */
    while ( (n = buf->uio_resid) > limit ) {
	DEB(printf("dsp_read_body: start waiting for %d bytes\n", n));
        l = min (n, bsz);
        s = spltty();            /* no interrupts here !             */
	dsp_rd_dmaupdate(b);
        l = min( l, b->rl );     /* no more than avail. data     */
        if (l == 0) {
	    int timeout;
	    /*
	     * If there is no data ready, then we must sleep (unless
	     * of course we have doing non-blocking i/o). But also
	     * consider restarting the DMA engine.
	     */
	    if ( b->dl == 0 ) { /* dma was idle, start it  */
		if ( d->flags & SND_F_INIT && d->dbuf_out.dl == 0 ) {
		    /* want to init and there is no pending DMA activity */
		    splx(s);
		    d->callback(d, SND_CB_INIT); /* this is slow! */
		    s = spltty();
		}
		dsp_rdintr(d);
	    }
	    if (d->flags & SND_F_NBIO) {
		splx(s);
		break;
	    }
	    if (n-limit > b->dl)
		timeout = hz; /* we need to wait for an int. */
	    else
		timeout = 1; /* maybe data will be ready earlier */
            ret = tsleep( (caddr_t)b, PRIBIO | PCATCH , "dsprd", timeout ) ;
	    if (ret == EINTR)
		d->flags |= SND_F_ABORTING ;
	    splx(s);
	    if (ret == EINTR || ret == ERESTART)
		break ;
            continue;
        }
        splx(s);

	/*
	 * Do any necessary format conversion, and copy to user space.
	 * NOTE: I _can_ use rp here because it is not modified by the
	 * interrupt routines.
	 */
	if (b->rp + l > b->bufsize) { /* handle wraparounds */
	    int l1 = b->bufsize - b->rp ;
	    if (d->flags & SND_F_XLAT8) {
		translate_bytes(dsp_ulaw, b->buf + b->rp, l1);
		translate_bytes(dsp_ulaw, b->buf , l - l1);
	    }
	    uiomove(b->buf + b->rp, l1, buf) ;
	    uiomove(b->buf, l - l1, buf) ;
	} else {
	    if (d->flags & SND_F_XLAT8)
		translate_bytes(dsp_ulaw, b->buf + b->rp, l);
	    uiomove(b->buf + b->rp, l, buf) ;
	}

        s = spltty();  /* no interrupts here ... */
        b->fl += l ;    /* this more free bytes */
        b->rl -= l ;    /* this less ready bytes */
        b->rp += l ;    /* advance ready pointer */
        if (b->rp >= b->bufsize)        /* handle wraps */
            b->rp -= b->bufsize ;
        splx(s) ;
        bsz = min(b->bufsize, bsz*2);
    }
    s = spltty();          /* no interrupts here ... */
    d->flags &= ~SND_F_READING ;
    if (d->flags & SND_F_ABORTING) {
        d->flags &= ~SND_F_ABORTING; /* XXX */
	splx(s);
	dsp_rdabort(d, 1 /* restart */);
	/* XXX return EINTR ? */
    }
    splx(s) ;
    return ret ;
}


/*
 * short routine to initialize a dma buffer descriptor (usually
 * located in the XXX_desc structure). The first parameter is
 * the buffer size, the second one specifies that a 16-bit dma channel
 * is used (hence the buffer must be properly aligned).
 */
void
alloc_dbuf(snd_dbuf *b, int size)
{
    if (size > 0x10000)
	panic("max supported size is 64k");
    b->buf = contigmalloc(size, M_DEVBUF, M_NOWAIT,
	    0ul, 0xfffffful, 1ul, 0x10000ul);
    /* should check that malloc does not fail... */
    b->rp = b->fp = 0 ;
    b->dl = b->rl = 0 ;
    b->bufsize = b->fl = size ;
}

/*
 * this resets a buffer and starts the isa dma on that channel.
 * Must be called when the dma on the card is disabled (e.g. after init).
 */
void
reset_dbuf(snd_dbuf *b, int chan)
{
    DEB(printf("reset dbuf for chan %d\n", b->chan));
    b->rp = b->fp = 0 ;
    b->dl = b->rl = 0 ;
    b->fl = b->bufsize ;
    if (chan == SND_CHAN_NONE)
	return ;
    if (chan == SND_CHAN_WR)
	chan = B_WRITE | B_RAW ;
    else
	chan = B_READ | B_RAW ;
    isa_dmastart( chan , b->buf, b->bufsize, b->chan);
}

/*
 * snd_sync waits until the space in the given channel goes above
 * a threshold. chan = 1 : play, 2: capture. The threshold is
 * checked against fl or rl respectively.
 * Assume that the condition can become true, do not check here...
 */
int
snd_sync(snddev_info *d, int chan, int threshold)
{
    u_long s;
    int ret;
    snd_dbuf *b;

    b = (chan == 1) ? &(d->dbuf_out ) : &(d->dbuf_in ) ;

    for (;;) {
	s=spltty();
        if ( chan==1 )
	     dsp_wr_dmaupdate(b);
	else
	     dsp_rd_dmaupdate(b);
	if ( (chan == 1 && b->fl <= threshold) ||
	     (chan == 2 && b->rl <= threshold) ) {
		 ret = tsleep((caddr_t)b, PRIBIO|PCATCH, "sndsyn", 1);
	    splx(s);
	    if (ret == ERESTART || ret == EINTR) {
		printf("tsleep returns %d\n", ret);
		return -1 ;
	    }
	} else
	    break;
    }
    splx(s);
    return 0 ;
}

/*
 * dsp_wrabort(d) and dsp_rdabort(d) are non-blocking functions
 * which abort a pending DMA transfer and flush the buffers.
 * They return the number of bytes that has not been transferred.
 * The second parameter is used to restart the engine if needed.
 */
int
dsp_wrabort(snddev_info *d, int restart)
{
    long s;
    int missing = 0;
    snd_dbuf *b = & (d->dbuf_out) ;

    s = spltty();
    if ( b->dl ) {
	b->dl = 0 ;
	d->flags &= ~ SND_F_WRITING ;
	if (d->callback)
	    d->callback(d, SND_CB_WR | SND_CB_ABORT);
	isa_dmastop(b->chan) ;
	dsp_wr_dmadone(d);

	DEB(printf("dsp_wrabort: stopped, %d bytes left\n", b->rl));
    }
    missing = b->rl;
    isa_dmadone(B_WRITE, b->buf, b->bufsize, b->chan); /*free chan */
    reset_dbuf(b, restart ? SND_CHAN_WR : SND_CHAN_NONE);
    splx(s);
    return missing;
}

int
dsp_rdabort(snddev_info *d, int restart)
{
    long s;
    int missing = 0;
    snd_dbuf *b = & (d->dbuf_in) ;

    s = spltty();
    if ( b->dl ) {
	b->dl = 0 ;
	d->flags &= ~ SND_F_READING ;
	if (d->callback)
	    d->callback(d, SND_CB_RD | SND_CB_ABORT);
	isa_dmastop(b->chan) ;
	dsp_rd_dmadone(d);
    }
    missing = b->rl ;
    isa_dmadone(B_READ, b->buf, b->bufsize, b->chan);
    reset_dbuf(b, restart ? SND_CHAN_RD : SND_CHAN_NONE);
    splx(s);
    return missing;
}

/*
 * this routine tries to flush the dma transfer. It is called
 * on a close. The caller must set SND_F_CLOSING, and insure that
 * interrupts are enabled. We immediately abort any read DMA
 * operation, and then wait for the play buffer to drain.
 */

int
snd_flush(snddev_info *d)
{
    int ret, count=10;
    u_long s;
    snd_dbuf *b = &(d->dbuf_out) ;

    DEB(printf("snd_flush d->flags 0x%08x\n", d->flags));
    dsp_rdabort(d, 0 /* no restart */);
    /* close write */
    while ( b->dl ) {
	/*
	 * still pending output data.
	 */
	ret = tsleep( (caddr_t)b, PRIBIO|PCATCH, "dmafl1", hz);
	dsp_wr_dmaupdate(b);
	DEB( printf("snd_sync: now rl : fl  %d : %d\n", b->rl, b->fl ) );
	if (ret == EINTR) {
	    printf("tsleep returns %d\n", ret);
	    return -1 ;
	}
	if ( ret && --count == 0) {
	    printf("timeout flushing dbuf_out.chan, cnt 0x%x flags 0x%08lx\n",
		    b->rl, d->flags);
	    break;
	}
    }
    s = spltty(); /* should not be necessary... */
    d->flags &= ~SND_F_CLOSING ;
    dsp_wrabort(d, 0 /* no restart */);
    splx(s);
    return 0 ;
}

/*
 * end of new code for dma buffer handling
 */
