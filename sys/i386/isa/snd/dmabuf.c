/*
 * snd/dmabuf.c
 * 
 * New DMA routines -- Luigi Rizzo, 19 jul 97
 * This file implements the new DMA routines for the sound driver.
 *
 * Copyright by Luigi Rizzo - 1997
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */

#include <i386/isa/snd/sound.h>
#include <i386/isa/snd/ulaw.h>

#define MIN_CHUNK_SIZE 256	/* for uiomove etc. */
#define	DMA_ALIGN_BITS		2			/* i.e. 4 bytes */
#define	DMA_ALIGN_THRESHOLD	(1<< DMA_ALIGN_BITS)
#define	DMA_ALIGN_MASK		(~ (DMA_ALIGN_THRESHOLD - 1))

static void dsp_wr_dmadone(snddev_info *d);
static void dsp_rd_dmadone(snddev_info *d);

/*
 *

SOUND OUTPUT

We use a circular buffer to store samples directed to the DAC.
The buffer is split into three variable-size regions, each identified
by an offset in the buffer (dp,rp,fp) and a lenght (dl,rl,fl).

      0          dp,dl        rp,rl         fp,fl    bufsize
      |__________|_____>______|_____________|________|
	  FREE        DMA          READY      FREE    

  READY region: contains data written from the process and ready
      to be sent to the DAC;

  FREE region: is the empty region of the buffer, where a process
      can write new data.

  DMA region: contains data being sent to the DAC by the DMA engine.
      the actual boundary between the FREE and READY regions is in
      fact within the DMA region (indicated by the > symbol above),
      and advances as the DMA engine makes progress.

Both the "READY" and "FREE" regions can wrap around the end of the
buffer. The "DMA" region can only wrap if AUTO DMA is used, otherwise
it cannot cross the end of the buffer.

Since dl can be updated on the fly, dl0 marks the value used when the
operation was started. When using AUTO DMA, bufsize-(count in the ISA DMA
register) directly reflects the position of dp.

At initialization, DMA and READY are empty, and FREE takes all the
available space:

    dp = rp = fp = 0 ;	--  beginning of buffer
    dl0 = dl = 0 ;	-- meaning no dma activity
    rl = 0 ;		-- meaning no data ready
    fl = bufsize ;

The DMA region is also empty whenever there is no DMA activity, for
whatever reason (e.g. no ready data, or output is paused).
The code works as follows: the user write routine dsp_write_body()
fills up the READY region with new data (reclaiming space from the
FREE region) and starts the write DMA engine if inactive (activity
is indicated by d->flags & SND_F_WR_DMA ). The size of each
DMA transfer is chosen according to a series of rules which will be
discussed later. When a DMA transfer is complete, an interrupt causes
dsp_wrintr() to be called which empties the DMA region, extends
the FREE region and possibly starts the next transfer.

In some cases, the code tries to track the current status of DMA
operations by calling isa_dmastatus() and advancing the boundary
between FREE and DMA regions accordingly.

The size of a DMA transfer is selected according to the following
rules:

  1. when not using AUTO DMA, do not wrap around the end of the
     buffer, and do not let fp move too close to the end of the
     buffer;

  2. do not use more than half of the buffer size.
     This serves to allow room for a next write operation concurrent
     with the dma transfer, and to reduce the time which is necessary
     to wait before a pending dma will end (optionally, the max
     size could be further limited to a fixed amount of play time,
     depending on number of channels, sample size and sample speed);

  3. use the current blocksize (either specified by the user, or
     corresponding roughly to 0.25s of data);

  *
  */


/*
 * dsp_wr_dmadone moves the write DMA region into the FREE region.
 * It is assumed to be called at spltty() and with a write dma
 * previously started.
 */
static void
dsp_wr_dmadone(snddev_info *d)
{
    snd_dbuf *b = & (d->dbuf_out) ;

    isa_dmadone(B_WRITE, b->buf + b->dp, b->dl, d->dma1);
    b->fl += b->dl ;	/* make dl bytes free */
    /*
     * XXX here it would be more efficient to record if there
     * actually is a sleeping process, but this should still work.
     */
    wakeup(b); 	/* wakeup possible sleepers */
    if (d->wsel.si_pid &&
	    ( !(d->flags & SND_F_HAS_SIZE) || b->fl >= d->play_blocksize ) )
	selwakeup( & d->wsel );
    b->dp = b->rp ;
    b->dl0 = b->dl = 0 ;
}

/*
 * The following function tracks the status of a (write) dma transfer,
 * and moves the boundary between the FREE and the DMA regions.
 * It works under the following assumptions:
 *   - the DMA engine is running;
 *   - the routine is called with interrupts blocked.
 * BEWARE: when using AUTO DMA, dl can go negative! We assume that it
 * does not wrap!
 */
void
dsp_wr_dmaupdate(snddev_info *d)
{
    snd_dbuf *b = & (d->dbuf_out) ;
    int tmp;

    tmp = b->dl - isa_dmastatus1(d->dma1) ;
    tmp &= DMA_ALIGN_MASK; /* align... */
    if (tmp > 0) { /* we have some new data */
	b->dp += tmp;
	if (b->dp >= b->bufsize)
	    b->dp -= b->bufsize;
	b->dl -= tmp ;
	b->fl += tmp ;
    }
}

/*
 * Write interrupt routine. Can be called from other places, but
 * with interrupts disabled.
 */
void
dsp_wrintr(snddev_info *d)
{
    snd_dbuf *b = & (d->dbuf_out) ;
    int cb_reason = SND_CB_WR ;

    DEB(printf("dsp_wrintr: start on dl %d, rl %d, fl %d\n",
	b->dl, b->rl, b->fl));
    if (d->flags & SND_F_WR_DMA) {		/* dma was active */
	b->int_count++;
	d->flags &= ~SND_F_WR_DMA;
	cb_reason = SND_CB_WR | SND_CB_RESTART ;
	dsp_wr_dmadone(d);
    } else
	cb_reason = SND_CB_WR | SND_CB_START ;

    /*
     * start another dma operation only if have ready data in the
     * buffer, there is no pending abort, have a full-duplex device
     * (dma1 != dma2) or have half duplex device and there is no
     * pending op on the other side.
     *
     * Force transfer to be aligned to a boundary of 4, which is
     * needed when doing stereo and 16-bit. We could make this
     * adaptive, but why bother for now...
     */
    if (  b->rl >= DMA_ALIGN_THRESHOLD  &&
	  ! (d->flags & SND_F_ABORTING) &&
	  ( (d->dma1 != d->dma2) || ! (d->flags & SND_F_READING)  ) ) {
	b->dl = min(b->rl, b->bufsize - b->rp ) ; /* do not wrap */
	b->dl = min(b->dl, d->play_blocksize );	/* avoid too large transfer */
	b->dl &= DMA_ALIGN_MASK ; /* realign things */
	b->rl -= b->dl ;
	b->rp += b->dl ;
	if (b->rp == b->bufsize)
	    b->rp = 0 ;
	/*
         * now try to avoid too small dma transfers in the near future.
         * This can happen if I let rp start too close to the end of
         * the buffer. If this happens, and have enough data, try to
         * split the available block in two approx. equal parts.
	 * XXX this code can go when we use auto dma.
         */
        if (b->bufsize - b->rp < MIN_CHUNK_SIZE &&
		b->bufsize - b->dp > 2*MIN_CHUNK_SIZE) {
            b->dl = (b->bufsize - b->dp) / 2;
	    b->dl &= ~3 ; /* align to a boundary of 4 */
            b->rl += (b->rp - (b->dp + b->dl) ) ;
            b->rp = b->dp + b->dl ; /* no need to check for wraps */
        }
	/*
	 * how important is the order of operations ?
	 */
	if (b->dl == 0) {
	    printf("ouch... want to start for 0 bytes!\n");
	    goto ferma;
	}
	b->dl0 = b->dl ; /* XXX */
	if (d->callback)
	    d->callback(d, cb_reason );		/* start/restart dma */
	isa_dmastart( B_WRITE , b->buf + b->dp, b->dl, d->dma1);
	d->flags |= SND_F_WR_DMA;
    } else {
ferma:
	if (d->callback && (cb_reason & SND_CB_REASON_MASK) == SND_CB_RESTART )
	    d->callback(d, SND_CB_WR | SND_CB_STOP );	/* stop dma */
	/*
	 * if switching to read, should start the read dma...
	 */
	if ( d->dma1 == d->dma2 && (d->flags & SND_F_READING) )
	    dsp_rdintr(d);
	DEB(printf("cannot start wr-dma flags 0x%08x dma_dl %d rl %d\n",
		d->flags, isa_dmastatus1(d->dma1), b->rl));
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
dsp_write_body(snddev_info *d, struct uio *buf)
{
    int timeout = 1, n, l, bsz, ret = 0 ;
    long s;
    snd_dbuf *b = & (d->dbuf_out) ;

    /* assume d->flags |= SND_F_WRITING ; has been done before */
    /*
     * bsz is the max size for the next transfer. If the dma was
     * idle, we want it as large as possible. Otherwise, start with
     * a small block to avoid underruns if we are close to the end of
     * the previous operation.
     */
    bsz =  (d->flags & SND_F_WR_DMA) ? MIN_CHUNK_SIZE : b->bufsize ;
    while ( n = buf->uio_resid ) {
        l = min (n, bsz);       /* at most n bytes ... */
        s = spltty();  /* no interrupts here ... */
	/*
	 * if i) the dma engine is running, ii) we do not have enough space
	 * in the FREE region, and iii) the current DMA transfer might let
	 * us complete the _whole_ transfer without sleeping, or we are doing
	 * non-blocking I/O, then try to extend the FREE region.
	 * Otherwise do not bother, we will need to sleep anyways, and
	 * make the timeout longer.
	 */

	if ( d->flags & SND_F_WR_DMA && b->fl < l && 
	     ( b->fl + b->dl >= n || d->flags & SND_F_NBIO ) )
	    dsp_wr_dmaupdate(d); /* should really change timeout... */
	else
	    timeout = hz;
        l = min( l, b->fl );    /* no more than avail. space */
        l = min( l, b->bufsize - b->fp ); /* do not wrap ... */
	DEB(printf("dsp_write_body: prepare %d bytes out of %d\n", l,n));
	/*
	 * at this point, we assume that if l==0 the dma engine
	 * must (or will, in cause it is paused) be running.
	 */
        if (l == 0) { /* no space, must sleep */
	    if (d->flags & SND_F_NBIO) {
		/* unless of course we are doing non-blocking i/o */
		splx(s);
		break;
	    }
	    DEB(printf("dsp_write_body: l=0, (fl %d) sleeping\n", b->fl));
            ret = tsleep( (caddr_t)b, PRIBIO|PCATCH, "dspwr", timeout);
	    if (ret == EINTR)
		d->flags |= SND_F_ABORTING ;
	    splx(s);
	    if (ret == ERESTART || ret == EINTR)
		break ;
	    timeout = min(2*timeout, hz);
            continue;
        }
        splx(s);
	timeout = 1 ; /* we got some data... */

	/*
	 * copy data to the buffer, and possibly do format
	 * conversions (here, from ULAW to U8).
	 * NOTE: I can use fp here since it is not modified by the
	 * interrupt routines.
	 */
        ret = uiomove(b->buf + b->fp, l, buf) ;
	if (ret !=0 ) {	/* an error occurred ... */
	    printf("uiomove error %d\n", ret);
	    break ;
	}
	if (d->flags & SND_F_XLAT8)
	    translate_bytes(ulaw_dsp, b->buf + b->fp, l);

        s = spltty();  /* no interrupts here ... */
        b->rl += l ;    /* this more ready bytes */
        b->fl -= l ;    /* this less free bytes */
        b->fp += l ;
        if (b->fp >= b->bufsize)        /* handle wraps */
            b->fp -= b->bufsize ;
        if ( !(d->flags & SND_F_WR_DMA) ) {/* dma was idle, restart it */
	    if ( (d->flags & (SND_F_INIT|SND_F_WR_DMA|SND_F_RD_DMA)) ==
		    SND_F_INIT) {
		/* want to init but no pending DMA activity */
		splx(s);
		d->callback(d, SND_CB_INIT); /* this is slow! */
		s = spltty();
	    }
            dsp_wrintr(d) ;
	}
        splx(s) ;
        bsz = min(b->bufsize, bsz*2);
    }
    s = spltty();  /* no interrupts here ... */
    d->flags &= ~SND_F_WRITING ;
    if (d->flags & SND_F_ABORTING) {
        d->flags &= ~SND_F_ABORTING;
	splx(s);
	dsp_wrabort(d);
    }
    splx(s) ;
    return ret ;
}

/*
 * SOUND INPUT
 *

The input part is similar to the output one. The only difference is in
the ordering of regions, which is the following:

      0          rp,rl        dp,dl         fp,fl    bufsize
      |__________|____________|______>______|________|
	  FREE       READY          DMA       FREE    

and the fact that input data are in the READY region.

At initialization, as for the write routine, DMA and READY are empty,
and FREE takes all the space:

    dp = rp = fp = 0 ;	-- beginning of buffer
    dl0 = dl = 0 ;	-- meaning no dma activity
    rl = 0 ;		-- meaning no data ready
    fl = bufsize ;

Operation is as follows: upon user read (dsp_read_body()) a DMA read
is started if not already active (marked by d->flags & SND_F_RD_DMA),
then as soon as data are available in the READY region they are
transferred to the user buffer, thus advancing the boundary between FREE
and READY. Upon interrupts, caused by a completion of a DMA transfer,
the READY region is extended and possibly a new transfer is started.

When necessary, isa_dmastatus() is called to advance the boundary
between READY and DMA to the real position.

The rules to choose the size of the new DMA area are similar to
the other case, i.e:

  1. if not using AUTO mode, do not wrap around the end of the
     buffer, and do not let fp move too close to the end of the
     buffer;

  2. do not use more than half the buffer size; this serves to
     leave room for the next dma operation.

  3. use the default blocksize, either user-specified, or
     corresponding to 0.25s of data;

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

    isa_dmadone(B_READ, b->buf + b->dp, b->dl, d->dma2);
    b->rl += b->dl ;	/* make dl bytes available */
    wakeup(b) ;	/* wakeup possibly sleeping processes */
    if (d->rsel.si_pid &&
	    ( !(d->flags & SND_F_HAS_SIZE) || b->rl >= d->rec_blocksize ) )
	selwakeup( & d->rsel );
    b->dp = b->fp ;
    b->dl0 = b->dl = 0 ;
}

/*
 * The following function tracks the status of a (read) dma transfer,
 * and moves the boundary between the READY and the DMA regions.
 * It works under the following assumptions:
 *   - the DMA engine is running;
 *   - the function is called with interrupts blocked.
 */
void
dsp_rd_dmaupdate(snddev_info *d)
{
    snd_dbuf *b = & (d->dbuf_in) ;
    int tmp ;

    tmp = b->dl - isa_dmastatus1(d->dma2) ;
    tmp &= DMA_ALIGN_MASK; /* align... */
    if (tmp > 0) { /* we have some data */
	b->dp += tmp;
	if (b->dp >= b->bufsize)
	    b->dp -= b->bufsize;
	b->dl -= tmp ;
	b->rl += tmp ;
    }
}

/*
 * read interrupt routine. Must be called with interrupts blocked.
 */
void
dsp_rdintr(snddev_info *d)
{
    snd_dbuf *b = & (d->dbuf_in) ;
    int cb_reason = SND_CB_RD ;

    DEB(printf("dsp_rdintr: start dl = %d fp %d blocksize %d\n",
	b->dl, b->fp, d->rec_blocksize));
    if (d->flags & SND_F_RD_DMA) {		/* dma was active */
	b->int_count++;
	d->flags &= ~SND_F_RD_DMA;
	cb_reason = SND_CB_RD | SND_CB_RESTART ;
	dsp_rd_dmadone(d);
    } else
	cb_reason = SND_CB_RD | SND_CB_START ;
    /*
     * Same checks as in the write case (mutatis mutandis) to decide
     * whether or not to restart a dma transfer.
     */
    if ( b->fl >= DMA_ALIGN_THRESHOLD &&
         ((d->flags & (SND_F_ABORTING|SND_F_CLOSING)) == 0) &&
	 ( (d->dma1 != d->dma2) || ( (d->flags & SND_F_WRITING) == 0) ) ) {
	b->dl = min(b->fl, b->bufsize - b->fp ) ; /* do not wrap */
	b->dl = min(b->dl, d->rec_blocksize );
	b->dl &= DMA_ALIGN_MASK ; /* realign sizes */
	b->fl -= b->dl ;
	b->fp += b->dl ;
	if (b->fp == b->bufsize)
	    b->fp = 0 ;
	/*
         * now try to avoid too small dma transfers in the near future.
         * This can happen if I let fp start too close to the end of
         * the buffer. If this happens, and have enough data, try to
         * split the available block in two approx. equal parts.
         */
        if (b->bufsize - b->fp < MIN_CHUNK_SIZE &&
		b->bufsize - b->dp > 2*MIN_CHUNK_SIZE) {
            b->dl = (b->bufsize - b->dp) / 2;
	    b->dl &= DMA_ALIGN_MASK ; /* align to multiples of 3 */
            b->fl += (b->fp - (b->dp + b->dl) ) ;
            b->fp = b->dp + b->dl ; /* no need to check that fp wraps */
        }
	if (b->dl == 0) {
	    printf("ouch! want to read 0 bytes\n");
	    goto ferma;
	}
	b->dl0 = b->dl ; /* XXX */
	if (d->callback)
	    d->callback(d, cb_reason);		/* restart_dma(); */
	isa_dmastart( B_READ , b->buf + b->dp, b->dl, d->dma2);
	d->flags |= SND_F_RD_DMA;
    } else {
ferma:
	if (d->callback && (cb_reason & SND_CB_REASON_MASK) == SND_CB_RESTART)
	    d->callback(d, SND_CB_RD | SND_CB_STOP);
	/*
	 * if switching to write, start write dma engine
	 */
	if ( d->dma1 == d->dma2 && (d->flags & SND_F_WRITING) )
	    dsp_wrintr(d) ;
	DEB(printf("cannot start rd-dma flags 0x%08x dma_dl %d fl %d\n",
		d->flags, isa_dmastatus1(d->dma2), b->fl));
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

    int timeout = 1 ; /* counter of how many ticks we sleep */

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
	/*
	 * here compute how many bytes to transfer, enforcing various
	 * limitations:
	 */
        l = min (n, bsz);        /* 1': at most bsz bytes ...        */
        s = spltty();            /* no interrupts here !             */
	/*
	 * if i) the dma engine is running, ii) we do not have enough
	 * ready bytes, and iii) the current DMA transfer could give
	 * us what we need, or we are doing non-blocking IO, then try
	 * to extend the READY region.
	 * Otherwise do not bother, we will need to sleep anyways,
	 * and make the timeout longer.
	 */

        if ( d->flags & SND_F_RD_DMA && b->rl < l &&
	     ( d->flags & SND_F_NBIO || b->rl + b->dl >= n - limit ) )
	    dsp_rd_dmaupdate(d);
	else
	    timeout = hz ;
        l = min( l, b->rl );     /* 2': no more than avail. data     */
        l = min( l, b->bufsize - b->rp ); /* 3': do not wrap buffer. */
	   /* the above limitation can be removed if we use auto DMA
	    * on the ISA controller. But then we have to make a check
	    * when doing the uiomove...
	    */ 
        if ( !(d->flags & SND_F_RD_DMA) ) { /* dma was idle, start it  */
	    /*
	     * There are two reasons the dma can be idle: either this
	     * is the first read, or the buffer has become full. In
	     * the latter case, the dma cannot be restarted until
	     * we have removed some data, which will be true at the
	     * second round.
	     *
	     * Call dsp_rdintr to start the dma. It would be nice to
	     * have a "need" field in the snd_dbuf, so that we do
	     * not start a long operation unnecessarily. However,
	     * the restart code will ask for at most d->blocksize
	     * bytes, and since we are sure we are the only reader,
	     * and the routine is not interrupted, we patch and
	     * restore d->blocksize around the call. A bit dirty,
	     * but it works, and saves some overhead :)
	     */

	    int old_bs = d->rec_blocksize;

	    if ( (d->flags & (SND_F_INIT|SND_F_WR_DMA|SND_F_RD_DMA)) ==
		    SND_F_INIT) {
		/* want to init but no pending DMA activity */
		splx(s);
		d->callback(d, SND_CB_INIT); /* this is slow! */
		s = spltty();
	    }
	    if (l < MIN_CHUNK_SIZE)
		d->rec_blocksize = MIN_CHUNK_SIZE ;
	    else if (l < d->rec_blocksize)
		d->rec_blocksize = l ;
            dsp_rdintr(d);
	    d->rec_blocksize = old_bs ;
	}

        if (l == 0) {
	    /*
	     * If, after all these efforts, we still have no data ready,
	     * then we must sleep (unless of course we have doing
	     * non-blocking i/o. But use exponential delays, starting
	     * at 1 tick and doubling each time.
	     */
	    if (d->flags & SND_F_NBIO) {
		splx(s);
		break;
	    }
	    DEB(printf("dsp_read_body: sleeping %d waiting for %d bytes\n",
		 timeout, buf->uio_resid));
            ret = tsleep( (caddr_t)b, PRIBIO | PCATCH , "dsprd", timeout ) ;
	    if (ret == EINTR)
		d->flags |= SND_F_ABORTING ;
	    splx(s); /* necessary before the goto again... */
	    if (ret == ERESTART || ret == EINTR)
		break ;
	    DEB(printf("woke up, ret %d, rl %d\n", ret, b->rl));
	    timeout = min(timeout*2, hz);
            continue;
        }
        splx(s);

	timeout = 1 ; /* we got some data, so reset exp. wait */
	/*
	 * if we are using /dev/audio and the device does not
	 * support it natively, we should do a format conversion.
	 * (in this case from uLAW to natural format).
	 * This can be messy in that it can require an intermediate
	 * buffer, and also screw up the byte count.
	 */
	/*
	 * NOTE: I _can_ use rp here because it is not modified by the
	 * interrupt routines.
	 */
	if (d->flags & SND_F_XLAT8)
	    translate_bytes(dsp_ulaw, b->buf + b->rp, l);
        ret = uiomove(b->buf + b->rp, l, buf) ;
	if (ret !=0 )	/* an error occurred ... */
	    break ;

        s = spltty();  /* no interrupts here ... */
        b->fl += l ;    /* this more free bytes */
        b->rl -= l ;    /* this less ready bytes */
        b->rp += l ;    /* advance ready pointer */
        if (b->rp == b->bufsize)        /* handle wraps */
            b->rp = 0 ;
        splx(s) ;
        bsz = min(b->bufsize, bsz*2);
    }
    s = spltty();          /* no interrupts here ... */
    d->flags &= ~SND_F_READING ;
    if (d->flags & SND_F_ABORTING) {
        d->flags |= ~SND_F_ABORTING;
	splx(s);
	dsp_rdabort(d);
    }
    splx(s) ;
    return ret ;
}


/*
 * short routine to initialize a dma buffer descriptor (usually
 * located in the XXX_desc structure). The first parameter is
 * the buffer size, the second one specifies the dma channel in use
 * At the moment we do not support more than 64K, since for some
 * cards I need to switch between dma1 and dma2. The channel is
 * unused.
 */
void
alloc_dbuf(snd_dbuf *b, int size, int chan)
{
    if (size > 0x10000)
	panic("max supported size is 64k");
    b->buf = contigmalloc(size, M_DEVBUF, M_NOWAIT,
	    0ul, 0xfffffful, 1ul, 0x10000ul);
    /* should check that it does not fail... */
    b->dp = b->rp = b->fp = 0 ;
    b->dl0 = b->dl = b->rl = 0 ;
    b->bufsize = b->fl = size ;
}

void
reset_dbuf(snd_dbuf *b)
{
    b->dp = b->rp = b->fp = 0 ;
    b->dl0 = b->dl = b->rl = 0 ;
    b->fl = b->bufsize ;
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
	     dsp_wr_dmaupdate(d);
	else
	     dsp_rd_dmaupdate(d);
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
 */
int
dsp_wrabort(snddev_info *d)
{
    long s;
    int missing = 0;
    snd_dbuf *b = & (d->dbuf_out) ;

    s = spltty();
    if ( d->flags & SND_F_WR_DMA ) {
	d->flags &= ~(SND_F_WR_DMA | SND_F_WRITING);
	if (d->callback)
	    d->callback(d, SND_CB_WR | SND_CB_ABORT);
	missing = isa_dmastop(d->dma1) ; /* this many missing bytes... */

	b->rl += missing ; /* which are part of the ready area */
	b->rp -= missing ;
	if (b->rp < 0)
	    b->rp += b->bufsize;
	DEB(printf("dsp_wrabort: stopped after %d bytes out of %d\n",
	    b->dl - missing, b->dl));
	b->dl -= missing;
	dsp_wr_dmadone(d);
	missing = b->rl;
    }
    reset_dbuf(b);
    splx(s);
    return missing;
}

int
dsp_rdabort(snddev_info *d)
{
    long s;
    int missing = 0;
    snd_dbuf *b = & (d->dbuf_in) ;

    s = spltty();
    if ( d->flags & SND_F_RD_DMA ) {
	d->flags &= ~(SND_F_RD_DMA | SND_F_READING);
	if (d->callback)
	    d->callback(d, SND_CB_RD | SND_CB_ABORT);
	missing = isa_dmastop(d->dma2) ; /* this many missing bytes... */

	b->fl += missing ; /* which are part of the free area */
	b->fp -= missing ;
	if (b->fp < 0)
	    b->fp += b->bufsize;
	DEB(printf("dsp_rdabort: stopped after %d bytes out of %d\n",
	    b->dl - missing, b->dl));
	b->dl -= missing;
	dsp_rd_dmadone(d);
	missing = b->rl ;
    }
    reset_dbuf(b);
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
    int ret, res, res1;
    int count=10;

DEB(printf("snd_flush d->flags 0x%08x\n", d->flags));
    dsp_rdabort(d);
    if ( d->flags & SND_F_WR_DMA ) {
	/* close write */
	while ( d->flags & SND_F_WR_DMA ) {
	    /*
	     * still pending output data.
	     */
	    ret = tsleep( (caddr_t)&(d->dbuf_out), PRIBIO|PCATCH, "dmafl1", hz);
	    if (ret == ERESTART || ret == EINTR) {
		printf("tsleep returns %d\n", ret);
		return -1 ;
	    }
	    if ( ret && --count == 0) {
		printf("timeout flushing dma1, cnt 0x%x flags 0x%08x\n",
			isa_dmastatus1(d->dma1), d->flags);
		return -1 ;
	    }
	}
	d->flags &= ~SND_F_CLOSING ;
    }
    reset_dbuf(& (d->dbuf_out) );
    return 0 ;
}

/*
 * end of new code for dma buffer handling
 */
