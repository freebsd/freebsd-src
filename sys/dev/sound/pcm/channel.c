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
 */

#include <dev/sound/pcm/sound.h>

#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

#define MIN_CHUNK_SIZE 		256	/* for uiomove etc. */
#define	DMA_ALIGN_THRESHOLD	4
#define	DMA_ALIGN_MASK		(~(DMA_ALIGN_THRESHOLD - 1))

#define	MIN(x, y) (((x) < (y))? (x) : (y))
#define CANCHANGE(c) (!(c->flags & CHN_F_TRIGGERED))

/*
#define DEB(x) x
*/

static int chn_targetirqrate = 32;
TUNABLE_INT("hw.snd.targetirqrate", &chn_targetirqrate);

static int
sysctl_hw_snd_targetirqrate(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = chn_targetirqrate;
	err = sysctl_handle_int(oidp, &val, sizeof(val), req);
	if (val < 16 || val > 512)
		err = EINVAL;
	else
		chn_targetirqrate = val;

	return err;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, targetirqrate, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_targetirqrate, "I", "");
static int report_soft_formats = 1;
SYSCTL_INT(_hw_snd, OID_AUTO, report_soft_formats, CTLFLAG_RW,
	&report_soft_formats, 1, "report software-emulated formats");

static int chn_buildfeeder(struct pcm_channel *c);

static void
chn_lockinit(struct pcm_channel *c)
{
	c->lock = snd_mtxcreate(c->name, "pcm channel");
}

static void
chn_lockdestroy(struct pcm_channel *c)
{
	snd_mtxfree(c->lock);
}

static int
chn_polltrigger(struct pcm_channel *c)
{
	struct snd_dbuf *bs = c->bufsoft;
	unsigned amt, lim;

	CHN_LOCKASSERT(c);
	if (c->flags & CHN_F_MAPPED) {
		if (sndbuf_getprevblocks(bs) == 0)
			return 1;
		else
			return (sndbuf_getblocks(bs) > sndbuf_getprevblocks(bs))? 1 : 0;
	} else {
		amt = (c->direction == PCMDIR_PLAY)? sndbuf_getfree(bs) : sndbuf_getready(bs);
		lim = (c->flags & CHN_F_HAS_SIZE)? sndbuf_getblksz(bs) : 1;
		lim = 1;
		return (amt >= lim)? 1 : 0;
	}
	return 0;
}

static int
chn_pollreset(struct pcm_channel *c)
{
	struct snd_dbuf *bs = c->bufsoft;

	CHN_LOCKASSERT(c);
	sndbuf_updateprevtotal(bs);
	return 1;
}

static void
chn_wakeup(struct pcm_channel *c)
{
    	struct snd_dbuf *bs = c->bufsoft;

	CHN_LOCKASSERT(c);
	if (SEL_WAITING(sndbuf_getsel(bs)) && chn_polltrigger(c))
		selwakeup(sndbuf_getsel(bs));
	wakeup(bs);
}

static int
chn_sleep(struct pcm_channel *c, char *str, int timeout)
{
    	struct snd_dbuf *bs = c->bufsoft;
	int ret;

	CHN_LOCKASSERT(c);
#ifdef USING_MUTEX
	ret = msleep(bs, c->lock, PRIBIO | PCATCH, str, timeout);
#else
	ret = tsleep(bs, PRIBIO | PCATCH, str, timeout);
#endif

	return ret;
}

/*
 * chn_dmaupdate() tracks the status of a dma transfer,
 * updating pointers. It must be called at spltty().
 */

static unsigned int
chn_dmaupdate(struct pcm_channel *c)
{
	struct snd_dbuf *b = c->bufhard;
	unsigned int delta, old, hwptr, amt;

	KASSERT(sndbuf_getsize(b) > 0, ("bufsize == 0"));
	CHN_LOCKASSERT(c);

	old = sndbuf_gethwptr(b);
	hwptr = chn_getptr(c);
	delta = (sndbuf_getsize(b) + hwptr - old) % sndbuf_getsize(b);
	sndbuf_sethwptr(b, hwptr);

	DEB(
	if (delta >= ((sndbuf_getsize(b) * 15) / 16)) {
		if (!(c->flags & (CHN_F_CLOSING | CHN_F_ABORTING)))
			device_printf(c->dev, "hwptr went backwards %d -> %d\n", old, hwptr);
	}
	);

	if (c->direction == PCMDIR_PLAY) {
		amt = MIN(delta, sndbuf_getready(b));
		if (amt > 0)
			sndbuf_dispose(b, NULL, amt);
	} else {
		amt = MIN(delta, sndbuf_getfree(b));
		if (amt > 0)
		       sndbuf_acquire(b, NULL, amt);
	}

	return delta;
}

void
chn_wrupdate(struct pcm_channel *c)
{
	int ret;

	CHN_LOCKASSERT(c);
	KASSERT(c->direction == PCMDIR_PLAY, ("chn_wrupdate on bad channel"));

	if ((c->flags & (CHN_F_MAPPED | CHN_F_VIRTUAL)) || !(c->flags & CHN_F_TRIGGERED))
		return;
	chn_dmaupdate(c);
	ret = chn_wrfeed(c);
	/* tell the driver we've updated the primary buffer */
	chn_trigger(c, PCMTRIG_EMLDMAWR);
	DEB(if (ret)
		printf("chn_wrupdate: chn_wrfeed returned %d\n", ret);)

}

int
chn_wrfeed(struct pcm_channel *c)
{
    	struct snd_dbuf *b = c->bufhard;
    	struct snd_dbuf *bs = c->bufsoft;
	unsigned int ret, amt;

	CHN_LOCKASSERT(c);
    	DEB(
	if (c->flags & CHN_F_CLOSING) {
		sndbuf_dump(b, "b", 0x02);
		sndbuf_dump(bs, "bs", 0x02);
	})

	if (c->flags & CHN_F_MAPPED)
		sndbuf_acquire(bs, NULL, sndbuf_getfree(bs));

	amt = sndbuf_getfree(b);
	if (sndbuf_getready(bs) < amt)
		c->xruns++;

	ret = (amt > 0)? sndbuf_feed(bs, b, c, c->feeder, amt) : ENOSPC;
	if (ret == 0 && sndbuf_getfree(b) < amt)
		chn_wakeup(c);

	return ret;
}

static void
chn_wrintr(struct pcm_channel *c)
{
	int ret;

	CHN_LOCKASSERT(c);
	/* update pointers in primary buffer */
	chn_dmaupdate(c);
	/* ...and feed from secondary to primary */
	ret = chn_wrfeed(c);
	/* tell the driver we've updated the primary buffer */
	chn_trigger(c, PCMTRIG_EMLDMAWR);
	DEB(if (ret)
		printf("chn_wrintr: chn_wrfeed returned %d\n", ret);)
}

/*
 * user write routine - uiomove data into secondary buffer, trigger if necessary
 * if blocking, sleep, rinse and repeat.
 *
 * called externally, so must handle locking
 */

int
chn_write(struct pcm_channel *c, struct uio *buf)
{
	int ret, timeout, newsize, count, sz;
	struct snd_dbuf *bs = c->bufsoft;

	CHN_LOCKASSERT(c);
	/*
	 * XXX Certain applications attempt to write larger size
	 * of pcm data than c->blocksize2nd without blocking,
	 * resulting partial write. Expand the block size so that
	 * the write operation avoids blocking.
	 */
	if ((c->flags & CHN_F_NBIO) && buf->uio_resid > sndbuf_getblksz(bs)) {
		DEB(device_printf(c->dev, "broken app, nbio and tried to write %d bytes with fragsz %d\n",
			buf->uio_resid, sndbuf_getblksz(bs)));
		newsize = 16;
		while (newsize < min(buf->uio_resid, CHN_2NDBUFMAXSIZE / 2))
			newsize <<= 1;
		chn_setblocksize(c, sndbuf_getblkcnt(bs), newsize);
		DEB(device_printf(c->dev, "frags reset to %d x %d\n", sndbuf_getblkcnt(bs), sndbuf_getblksz(bs)));
	}

	ret = 0;
	count = hz;
	while (!ret && (buf->uio_resid > 0) && (count > 0)) {
		sz = sndbuf_getfree(bs);
		if (sz == 0) {
			if (c->flags & CHN_F_NBIO)
				ret = EWOULDBLOCK;
			else {
				timeout = (hz * sndbuf_getblksz(bs)) / (sndbuf_getspd(bs) * sndbuf_getbps(bs));
				if (timeout < 1)
					timeout = 1;
				timeout = 1;
	   			ret = chn_sleep(c, "pcmwr", timeout);
				if (ret == EWOULDBLOCK) {
					count -= timeout;
					ret = 0;
				} else if (ret == 0)
					count = hz;
			}
		} else {
			sz = MIN(sz, buf->uio_resid);
			KASSERT(sz > 0, ("confusion in chn_write"));
			/* printf("sz: %d\n", sz); */
			ret = sndbuf_uiomove(bs, buf, sz);
			if (ret == 0 && !(c->flags & CHN_F_TRIGGERED))
				chn_start(c, 0);
		}
	}
	/* printf("ret: %d left: %d\n", ret, buf->uio_resid); */

	if (count <= 0) {
		c->flags |= CHN_F_DEAD;
		printf("%s: play interrupt timeout, channel dead\n", c->name);
	}

	return ret;
}

static int
chn_rddump(struct pcm_channel *c, unsigned int cnt)
{
    	struct snd_dbuf *b = c->bufhard;

	CHN_LOCKASSERT(c);
	sndbuf_setxrun(b, sndbuf_getxrun(b) + cnt);
	return sndbuf_dispose(b, NULL, cnt);
}

/*
 * Feed new data from the read buffer. Can be called in the bottom half.
 * Hence must be called at spltty.
 */
int
chn_rdfeed(struct pcm_channel *c)
{
    	struct snd_dbuf *b = c->bufhard;
    	struct snd_dbuf *bs = c->bufsoft;
	unsigned int ret, amt;

	CHN_LOCKASSERT(c);
    	DEB(
	if (c->flags & CHN_F_CLOSING) {
		sndbuf_dump(b, "b", 0x02);
		sndbuf_dump(bs, "bs", 0x02);
	})

	amt = sndbuf_getready(b);
	if (sndbuf_getfree(bs) < amt) {
		c->xruns++;
		amt = sndbuf_getfree(bs);
	}
	ret = (amt > 0)? sndbuf_feed(b, bs, c, c->feeder, amt) : 0;

	amt = sndbuf_getready(b);
	if (amt > 0)
		chn_rddump(c, amt);

	chn_wakeup(c);

	return ret;
}

void
chn_rdupdate(struct pcm_channel *c)
{
	int ret;

	CHN_LOCKASSERT(c);
	KASSERT(c->direction == PCMDIR_REC, ("chn_rdupdate on bad channel"));

	if ((c->flags & CHN_F_MAPPED) || !(c->flags & CHN_F_TRIGGERED))
		return;
	chn_trigger(c, PCMTRIG_EMLDMARD);
	chn_dmaupdate(c);
	ret = chn_rdfeed(c);
	if (ret)
		printf("chn_rdfeed: %d\n", ret);

}

/* read interrupt routine. Must be called with interrupts blocked. */
static void
chn_rdintr(struct pcm_channel *c)
{
	int ret;

	CHN_LOCKASSERT(c);
	/* tell the driver to update the primary buffer if non-dma */
	chn_trigger(c, PCMTRIG_EMLDMARD);
	/* update pointers in primary buffer */
	chn_dmaupdate(c);
	/* ...and feed from primary to secondary */
	ret = chn_rdfeed(c);
}

/*
 * user read routine - trigger if necessary, uiomove data from secondary buffer
 * if blocking, sleep, rinse and repeat.
 *
 * called externally, so must handle locking
 */

int
chn_read(struct pcm_channel *c, struct uio *buf)
{
	int		ret, timeout, sz, count;
	struct snd_dbuf       *bs = c->bufsoft;

	CHN_LOCKASSERT(c);
	if (!(c->flags & CHN_F_TRIGGERED))
		chn_start(c, 0);

	ret = 0;
	count = hz;
	while (!ret && (buf->uio_resid > 0) && (count > 0)) {
		sz = MIN(buf->uio_resid, sndbuf_getready(bs));

		if (sz > 0) {
			ret = sndbuf_uiomove(bs, buf, sz);
		} else {
			if (c->flags & CHN_F_NBIO) {
				ret = EWOULDBLOCK;
			} else {
				timeout = (hz * sndbuf_getblksz(bs)) / (sndbuf_getspd(bs) * sndbuf_getbps(bs));
				if (timeout < 1)
					timeout = 1;
	   			ret = chn_sleep(c, "pcmrd", timeout);
				if (ret == EWOULDBLOCK) {
					count -= timeout;
					ret = 0;
				} else {
					count = hz;
				}

			}
		}
	}

	if (count <= 0) {
		c->flags |= CHN_F_DEAD;
		printf("%s: record interrupt timeout, channel dead\n", c->name);
	}

	return ret;
}

void
chn_intr(struct pcm_channel *c)
{
	CHN_LOCK(c);
	c->interrupts++;
	if (c->direction == PCMDIR_PLAY)
		chn_wrintr(c);
	else
		chn_rdintr(c);
	CHN_UNLOCK(c);
}

u_int32_t
chn_start(struct pcm_channel *c, int force)
{
	u_int32_t i, j;
	struct snd_dbuf *b = c->bufhard;
	struct snd_dbuf *bs = c->bufsoft;

	CHN_LOCKASSERT(c);
	/* if we're running, or if we're prevented from triggering, bail */
	if ((c->flags & CHN_F_TRIGGERED) || ((c->flags & CHN_F_NOTRIGGER) && !force))
		return EINVAL;

	i = (c->direction == PCMDIR_PLAY)? sndbuf_getready(bs) : sndbuf_getfree(bs);
	j = (c->direction == PCMDIR_PLAY)? sndbuf_getfree(b) : sndbuf_getready(b);
	if (force || (i >= j)) {
		c->flags |= CHN_F_TRIGGERED;
		/*
		 * if we're starting because a vchan started, don't feed any data
		 * or it becomes impossible to start vchans synchronised with the
		 * first one.  the hardbuf should be empty so we top it up with
		 * silence to give it something to chew.  the real data will be
		 * fed at the first irq.
		 */
		if (c->direction == PCMDIR_PLAY) {
			if (SLIST_EMPTY(&c->children))
				chn_wrfeed(c);
			else
				sndbuf_fillsilence(b);
		}
		sndbuf_setrun(b, 1);
		c->xruns = 0;
	    	chn_trigger(c, PCMTRIG_START);
		return 0;
	}

	return 0;
}

void
chn_resetbuf(struct pcm_channel *c)
{
	struct snd_dbuf *b = c->bufhard;
	struct snd_dbuf *bs = c->bufsoft;

	c->blocks = 0;
	sndbuf_reset(b);
	sndbuf_reset(bs);
}

/*
 * chn_sync waits until the space in the given channel goes above
 * a threshold. The threshold is checked against fl or rl respectively.
 * Assume that the condition can become true, do not check here...
 */
int
chn_sync(struct pcm_channel *c, int threshold)
{
    	u_long rdy;
    	int ret;
    	struct snd_dbuf *bs = c->bufsoft;

	CHN_LOCKASSERT(c);
    	for (;;) {
		rdy = (c->direction == PCMDIR_PLAY)? sndbuf_getfree(bs) : sndbuf_getready(bs);
		if (rdy <= threshold) {
	    		ret = chn_sleep(c, "pcmsyn", 1);
	    		if (ret == ERESTART || ret == EINTR) {
				DEB(printf("chn_sync: tsleep returns %d\n", ret));
				return -1;
	    		}
		} else
			break;
    	}
    	return 0;
}

/* called externally, handle locking */
int
chn_poll(struct pcm_channel *c, int ev, struct thread *td)
{
	struct snd_dbuf *bs = c->bufsoft;
	int ret;

	CHN_LOCKASSERT(c);
    	if (!(c->flags & CHN_F_MAPPED) && !(c->flags & CHN_F_TRIGGERED))
		chn_start(c, 1);
	ret = 0;
	if (chn_polltrigger(c) && chn_pollreset(c))
		ret = ev;
	else
		selrecord(td, sndbuf_getsel(bs));
	return ret;
}

/*
 * chn_abort terminates a running dma transfer.  it may sleep up to 200ms.
 * it returns the number of bytes that have not been transferred.
 *
 * called from: dsp_close, dsp_ioctl, with channel locked
 */
int
chn_abort(struct pcm_channel *c)
{
    	int missing = 0;
    	struct snd_dbuf *b = c->bufhard;
    	struct snd_dbuf *bs = c->bufsoft;

	CHN_LOCKASSERT(c);
	if (!(c->flags & CHN_F_TRIGGERED))
		return 0;
	c->flags |= CHN_F_ABORTING;

	c->flags &= ~CHN_F_TRIGGERED;
	/* kill the channel */
	chn_trigger(c, PCMTRIG_ABORT);
	sndbuf_setrun(b, 0);
	if (!(c->flags & CHN_F_VIRTUAL))
		chn_dmaupdate(c);
    	missing = sndbuf_getready(bs) + sndbuf_getready(b);

	c->flags &= ~CHN_F_ABORTING;
	return missing;
}

/*
 * this routine tries to flush the dma transfer. It is called
 * on a close. We immediately abort any read DMA
 * operation, and then wait for the play buffer to drain.
 *
 * called from: dsp_close
 */

int
chn_flush(struct pcm_channel *c)
{
    	int ret, count, resid, resid_p;
    	struct snd_dbuf *b = c->bufhard;
    	struct snd_dbuf *bs = c->bufsoft;

	CHN_LOCKASSERT(c);
	KASSERT(c->direction == PCMDIR_PLAY, ("chn_wrupdate on bad channel"));
    	DEB(printf("chn_flush c->flags 0x%08x\n", c->flags));
	if (!(c->flags & CHN_F_TRIGGERED))
		return 0;

	c->flags |= CHN_F_CLOSING;
	resid = sndbuf_getready(bs) + sndbuf_getready(b);
	resid_p = resid;
	count = 10;
	ret = 0;
	while ((count > 0) && (resid > sndbuf_getsize(b)) && (ret == 0)) {
		/* still pending output data. */
		ret = chn_sleep(c, "pcmflu", hz / 10);
		if (ret == EWOULDBLOCK)
			ret = 0;
		if (ret == 0) {
			resid = sndbuf_getready(bs) + sndbuf_getready(b);
			if (resid >= resid_p)
				count--;
			resid_p = resid;
		}
   	}
	if (count == 0)
		DEB(printf("chn_flush: timeout\n"));

	c->flags &= ~CHN_F_TRIGGERED;
	/* kill the channel */
	chn_trigger(c, PCMTRIG_ABORT);
	sndbuf_setrun(b, 0);

    	c->flags &= ~CHN_F_CLOSING;
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
chn_reset(struct pcm_channel *c, u_int32_t fmt)
{
	int hwspd, r;

	CHN_LOCKASSERT(c);
	c->flags &= CHN_F_RESET;
	c->interrupts = 0;
	c->xruns = 0;

	r = CHANNEL_RESET(c->methods, c->devinfo);
	if (fmt != 0) {
		hwspd = DSP_DEFAULT_SPEED;
		/* only do this on a record channel until feederbuilder works */
		if (c->direction == PCMDIR_REC)
			RANGE(hwspd, chn_getcaps(c)->minspeed, chn_getcaps(c)->maxspeed);
		c->speed = hwspd;

		if (r == 0)
			r = chn_setformat(c, fmt);
		if (r == 0)
			r = chn_setspeed(c, hwspd);
		if (r == 0)
			r = chn_setvolume(c, 100, 100);
	}
	if (r == 0)
		r = chn_setblocksize(c, 0, 0);
	if (r == 0) {
		chn_resetbuf(c);
		r = CHANNEL_RESETDONE(c->methods, c->devinfo);
	}
	return r;
}

int
chn_init(struct pcm_channel *c, void *devinfo, int dir)
{
	struct feeder_class *fc;
	struct snd_dbuf *b, *bs;
	int ret;

	chn_lockinit(c);
	CHN_LOCK(c);

	b = NULL;
	bs = NULL;
	c->devinfo = NULL;
	c->feeder = NULL;

	ret = EINVAL;
	fc = feeder_getclass(NULL);
	if (fc == NULL)
		goto out;
	if (chn_addfeeder(c, fc, NULL))
		goto out;

	ret = ENOMEM;
	b = sndbuf_create(c->dev, c->name, "primary");
	if (b == NULL)
		goto out;
	bs = sndbuf_create(c->dev, c->name, "secondary");
	if (bs == NULL)
		goto out;
	sndbuf_setup(bs, NULL, 0);
	c->bufhard = b;
	c->bufsoft = bs;
	c->flags = 0;
	c->feederflags = 0;

	ret = ENODEV;
	c->devinfo = CHANNEL_INIT(c->methods, devinfo, b, c, dir);
	if (c->devinfo == NULL)
		goto out;

	ret = ENOMEM;
	if ((sndbuf_getsize(b) == 0) && ((c->flags & CHN_F_VIRTUAL) == 0))
		goto out;

	ret = chn_setdir(c, dir);
	if (ret)
		goto out;

	ret = sndbuf_setfmt(b, AFMT_U8);
	if (ret)
		goto out;

	ret = sndbuf_setfmt(bs, AFMT_U8);
	if (ret)
		goto out;


out:
	if (ret) {
		if (c->devinfo) {
			if (CHANNEL_FREE(c->methods, c->devinfo))
				sndbuf_free(b);
		}
		if (bs)
			sndbuf_destroy(bs);
		if (b)
			sndbuf_destroy(b);
		c->flags |= CHN_F_DEAD;
		chn_lockdestroy(c);

		return ret;
	}

	CHN_UNLOCK(c);
	return 0;
}

int
chn_kill(struct pcm_channel *c)
{
    	struct snd_dbuf *b = c->bufhard;
    	struct snd_dbuf *bs = c->bufsoft;

	CHN_LOCK(c);
	if (c->flags & CHN_F_TRIGGERED)
		chn_trigger(c, PCMTRIG_ABORT);
	while (chn_removefeeder(c) == 0);
	if (CHANNEL_FREE(c->methods, c->devinfo))
		sndbuf_free(b);
	c->flags |= CHN_F_DEAD;
	sndbuf_destroy(bs);
	sndbuf_destroy(b);
	chn_lockdestroy(c);
	return 0;
}

int
chn_setdir(struct pcm_channel *c, int dir)
{
    	struct snd_dbuf *b = c->bufhard;
	int r;

	CHN_LOCKASSERT(c);
	c->direction = dir;
	r = CHANNEL_SETDIR(c->methods, c->devinfo, c->direction);
	if (!r && ISA_DMA(b))
		sndbuf_isadmasetdir(b, c->direction);
	return r;
}

int
chn_setvolume(struct pcm_channel *c, int left, int right)
{
	CHN_LOCKASSERT(c);
	/* could add a feeder for volume changing if channel returns -1 */
	c->volume = (left << 8) | right;
	return 0;
}

static int
chn_tryspeed(struct pcm_channel *c, int speed)
{
	struct pcm_feeder *f;
    	struct snd_dbuf *b = c->bufhard;
    	struct snd_dbuf *bs = c->bufsoft;
    	struct snd_dbuf *x;
	int r, delta;

	CHN_LOCKASSERT(c);
	DEB(printf("setspeed, channel %s\n", c->name));
	DEB(printf("want speed %d, ", speed));
	if (speed <= 0)
		return EINVAL;
	if (CANCHANGE(c)) {
		r = 0;
		c->speed = speed;
		sndbuf_setspd(bs, speed);
		RANGE(speed, chn_getcaps(c)->minspeed, chn_getcaps(c)->maxspeed);
		DEB(printf("try speed %d, ", speed));
		sndbuf_setspd(b, CHANNEL_SETSPEED(c->methods, c->devinfo, speed));
		DEB(printf("got speed %d\n", sndbuf_getspd(b)));

		delta = sndbuf_getspd(b) - sndbuf_getspd(bs);
		if (delta < 0)
			delta = -delta;

		c->feederflags &= ~(1 << FEEDER_RATE);
		if (delta > 500)
			c->feederflags |= 1 << FEEDER_RATE;
		else
			sndbuf_setspd(bs, sndbuf_getspd(b));

		r = chn_buildfeeder(c);
		DEB(printf("r = %d\n", r));
		if (r)
			goto out;

		r = chn_setblocksize(c, 0, 0);
		if (r)
			goto out;

		if (!(c->feederflags & (1 << FEEDER_RATE)))
			goto out;

		r = EINVAL;
		f = chn_findfeeder(c, FEEDER_RATE);
		DEB(printf("feedrate = %p\n", f));
		if (f == NULL)
			goto out;

		x = (c->direction == PCMDIR_REC)? b : bs;
		r = FEEDER_SET(f, FEEDRATE_SRC, sndbuf_getspd(x));
		DEB(printf("feeder_set(FEEDRATE_SRC, %d) = %d\n", sndbuf_getspd(x), r));
		if (r)
			goto out;

		x = (c->direction == PCMDIR_REC)? bs : b;
		r = FEEDER_SET(f, FEEDRATE_DST, sndbuf_getspd(x));
		DEB(printf("feeder_set(FEEDRATE_DST, %d) = %d\n", sndbuf_getspd(x), r));
out:
		DEB(printf("setspeed done, r = %d\n", r));
		return r;
	} else
		return EINVAL;
}

int
chn_setspeed(struct pcm_channel *c, int speed)
{
	int r, oldspeed = c->speed;

	r = chn_tryspeed(c, speed);
	if (r) {
		DEB(printf("Failed to set speed %d falling back to %d\n", speed, oldspeed));
		chn_tryspeed(c, oldspeed);
	}
	return r;
}

static int
chn_tryformat(struct pcm_channel *c, u_int32_t fmt)
{
	struct snd_dbuf *b = c->bufhard;
	struct snd_dbuf *bs = c->bufsoft;
	int r;

	CHN_LOCKASSERT(c);
	if (CANCHANGE(c)) {
		DEB(printf("want format %d\n", fmt));
		c->format = fmt;
		r = chn_buildfeeder(c);
		if (r == 0) {
			sndbuf_setfmt(bs, c->format);
			chn_resetbuf(c);
			r = CHANNEL_SETFORMAT(c->methods, c->devinfo, sndbuf_getfmt(b));
			if (r == 0)
				r = chn_tryspeed(c, c->speed);
		}
		return r;
	} else
		return EINVAL;
}

int
chn_setformat(struct pcm_channel *c, u_int32_t fmt)
{
	u_int32_t oldfmt = c->format;
	int r;

	r = chn_tryformat(c, fmt);
	if (r) {
		DEB(printf("Format change %d failed, reverting to %d\n", fmt, oldfmt));
		chn_tryformat(c, oldfmt);
	}
	return r;
}

int
chn_setblocksize(struct pcm_channel *c, int blkcnt, int blksz)
{
	struct snd_dbuf *b = c->bufhard;
	struct snd_dbuf *bs = c->bufsoft;
	int bufsz, irqhz, tmp, ret;

	CHN_LOCKASSERT(c);
	if (!CANCHANGE(c) || (c->flags & CHN_F_MAPPED))
		return EINVAL;

	ret = 0;
	DEB(printf("%s(%d, %d)\n", __func__, blkcnt, blksz));
	if (blksz == 0 || blksz == -1) {
		if (blksz == -1)
			c->flags &= ~CHN_F_HAS_SIZE;
		if (!(c->flags & CHN_F_HAS_SIZE)) {
			blksz = (sndbuf_getbps(bs) * sndbuf_getspd(bs)) / chn_targetirqrate;
	      		tmp = 32;
			while (tmp <= blksz)
				tmp <<= 1;
			tmp >>= 1;
			blksz = tmp;
			blkcnt = CHN_2NDBUFMAXSIZE / blksz;

			RANGE(blksz, 16, CHN_2NDBUFMAXSIZE / 2);
			RANGE(blkcnt, 2, CHN_2NDBUFMAXSIZE / blksz);
			DEB(printf("%s: defaulting to (%d, %d)\n", __func__, blkcnt, blksz));
		} else {
			blkcnt = sndbuf_getblkcnt(bs);
			blksz = sndbuf_getblksz(bs);
			DEB(printf("%s: updating (%d, %d)\n", __func__, blkcnt, blksz));
		}
	} else {
		ret = EINVAL;
		if ((blksz < 16) || (blkcnt < 2) || (blkcnt * blksz > CHN_2NDBUFMAXSIZE))
			goto out;
		ret = 0;
		c->flags |= CHN_F_HAS_SIZE;
	}

	bufsz = blkcnt * blksz;

	ret = ENOMEM;
	if (sndbuf_remalloc(bs, blkcnt, blksz))
		goto out;
	ret = 0;

	/* adjust for different hw format/speed */
	irqhz = (sndbuf_getbps(bs) * sndbuf_getspd(bs)) / sndbuf_getblksz(bs);
	DEB(printf("%s: soft bps %d, spd %d, irqhz == %d\n", __func__, sndbuf_getbps(bs), sndbuf_getspd(bs), irqhz));
	RANGE(irqhz, 16, 512);

	sndbuf_setblksz(b, (sndbuf_getbps(b) * sndbuf_getspd(b)) / irqhz);

	/* round down to 2^x */
	blksz = 32;
	while (blksz <= sndbuf_getblksz(b))
		blksz <<= 1;
	blksz >>= 1;

	/* round down to fit hw buffer size */
	RANGE(blksz, 16, sndbuf_getmaxsize(b) / 2);
	DEB(printf("%s: hard blksz requested %d (maxsize %d), ", __func__, blksz, sndbuf_getmaxsize(b)));

	sndbuf_setblksz(b, CHANNEL_SETBLOCKSIZE(c->methods, c->devinfo, blksz));

	irqhz = (sndbuf_getbps(b) * sndbuf_getspd(b)) / sndbuf_getblksz(b);
	DEB(printf("got %d, irqhz == %d\n", sndbuf_getblksz(b), irqhz));

	chn_resetbuf(c);
out:
	return ret;
}

int
chn_trigger(struct pcm_channel *c, int go)
{
    	struct snd_dbuf *b = c->bufhard;
	int ret;

	CHN_LOCKASSERT(c);
	if (ISA_DMA(b) && (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD))
		sndbuf_isadmabounce(b);
	ret = CHANNEL_TRIGGER(c->methods, c->devinfo, go);

	return ret;
}

int
chn_getptr(struct pcm_channel *c)
{
	int hwptr;
	int a = (1 << c->align) - 1;

	CHN_LOCKASSERT(c);
	hwptr = (c->flags & CHN_F_TRIGGERED)? CHANNEL_GETPTR(c->methods, c->devinfo) : 0;
	/* don't allow unaligned values in the hwa ptr */
#if 1
	hwptr &= ~a ; /* Apply channel align mask */
#endif
	hwptr &= DMA_ALIGN_MASK; /* Apply DMA align mask */
	return hwptr;
}

struct pcmchan_caps *
chn_getcaps(struct pcm_channel *c)
{
	CHN_LOCKASSERT(c);
	return CHANNEL_GETCAPS(c->methods, c->devinfo);
}

u_int32_t
chn_getformats(struct pcm_channel *c)
{
	u_int32_t *fmtlist, fmts;
	int i;

	fmtlist = chn_getcaps(c)->fmtlist;
	fmts = 0;
	for (i = 0; fmtlist[i]; i++)
		fmts |= fmtlist[i];

	/* report software-supported formats */
	if (report_soft_formats)
		fmts |= AFMT_MU_LAW|AFMT_A_LAW|AFMT_U16_LE|AFMT_U16_BE|
		    AFMT_S16_LE|AFMT_S16_BE|AFMT_U8|AFMT_S8;

	return fmts;
}

static int
chn_buildfeeder(struct pcm_channel *c)
{
	struct feeder_class *fc;
	struct pcm_feederdesc desc;
	u_int32_t tmp[2], type, flags, hwfmt;
	int err;

	CHN_LOCKASSERT(c);
	while (chn_removefeeder(c) == 0);
	KASSERT((c->feeder == NULL), ("feeder chain not empty"));

	c->align = sndbuf_getalign(c->bufsoft);

	if (SLIST_EMPTY(&c->children)) {
		fc = feeder_getclass(NULL);
		KASSERT(fc != NULL, ("can't find root feeder"));

		err = chn_addfeeder(c, fc, NULL);
		if (err) {
			DEB(printf("can't add root feeder, err %d\n", err));

			return err;
		}
		c->feeder->desc->out = c->format;
	} else {
		desc.type = FEEDER_MIXER;
		desc.in = 0;
		desc.out = c->format;
		desc.flags = 0;
		fc = feeder_getclass(&desc);
		if (fc == NULL) {
			DEB(printf("can't find vchan feeder\n"));

			return EOPNOTSUPP;
		}

		err = chn_addfeeder(c, fc, &desc);
		if (err) {
			DEB(printf("can't add vchan feeder, err %d\n", err));

			return err;
		}
	}
	flags = c->feederflags;

	DEB(printf("not mapped, feederflags %x\n", flags));

	for (type = FEEDER_RATE; type <= FEEDER_LAST; type++) {
		if (flags & (1 << type)) {
			desc.type = type;
			desc.in = 0;
			desc.out = 0;
			desc.flags = 0;
			DEB(printf("find feeder type %d, ", type));
			fc = feeder_getclass(&desc);
			DEB(printf("got %p\n", fc));
			if (fc == NULL) {
				DEB(printf("can't find required feeder type %d\n", type));

				return EOPNOTSUPP;
			}

			if (c->feeder->desc->out != fc->desc->in) {
 				DEB(printf("build fmtchain from %x to %x: ", c->feeder->desc->out, fc->desc->in));
				tmp[0] = fc->desc->in;
				tmp[1] = 0;
				if (chn_fmtchain(c, tmp) == 0) {
					DEB(printf("failed\n"));

					return ENODEV;
				}
 				DEB(printf("ok\n"));
			}

			err = chn_addfeeder(c, fc, fc->desc);
			if (err) {
				DEB(printf("can't add feeder %p, output %x, err %d\n", fc, fc->desc->out, err));

				return err;
			}
			DEB(printf("added feeder %p, output %x\n", fc, c->feeder->desc->out));
		}
	}

	if (fmtvalid(c->feeder->desc->out, chn_getcaps(c)->fmtlist)) {
		hwfmt = c->feeder->desc->out;
	} else {
		if (c->direction == PCMDIR_REC) {
			tmp[0] = c->format;
			tmp[1] = 0;
			hwfmt = chn_fmtchain(c, tmp);
		} else {
			hwfmt = chn_fmtchain(c, chn_getcaps(c)->fmtlist);
		}
	}

	if (hwfmt == 0)
		return ENODEV;

	sndbuf_setfmt(c->bufhard, hwfmt);

	return 0;
}

int
chn_notify(struct pcm_channel *c, u_int32_t flags)
{
	struct pcmchan_children *pce;
	struct pcm_channel *child;
	int run;

	if (SLIST_EMPTY(&c->children))
		return ENODEV;

	run = (c->flags & CHN_F_TRIGGERED)? 1 : 0;
	/*
	 * if the hwchan is running, we can't change its rate, format or
	 * blocksize
	 */
	if (run)
		flags &= CHN_N_VOLUME | CHN_N_TRIGGER;

	if (flags & CHN_N_RATE) {
		/*
		 * we could do something here, like scan children and decide on
		 * the most appropriate rate to mix at, but we don't for now
		 */
	}
	if (flags & CHN_N_FORMAT) {
		/*
		 * we could do something here, like scan children and decide on
		 * the most appropriate mixer feeder to use, but we don't for now
		 */
	}
	if (flags & CHN_N_VOLUME) {
		/*
		 * we could do something here but we don't for now
		 */
	}
	if (flags & CHN_N_BLOCKSIZE) {
		int blksz;
		/*
		 * scan the children, find the lowest blocksize and use that
		 * for the hard blocksize
		 */
		blksz = sndbuf_getmaxsize(c->bufhard) / 2;
		SLIST_FOREACH(pce, &c->children, link) {
			child = pce->channel;
			if (sndbuf_getblksz(child->bufhard) < blksz)
				blksz = sndbuf_getblksz(child->bufhard);
		}
		chn_setblocksize(c, 2, blksz);
	}
	if (flags & CHN_N_TRIGGER) {
		int nrun;
		/*
		 * scan the children, and figure out if any are running
		 * if so, we need to be running, otherwise we need to be stopped
		 * if we aren't in our target sstate, move to it
		 */
		nrun = 0;
		SLIST_FOREACH(pce, &c->children, link) {
			child = pce->channel;
			if (child->flags & CHN_F_TRIGGERED)
				nrun = 1;
		}
		if (nrun && !run)
			chn_start(c, 1);
		if (!nrun && run)
			chn_abort(c);
	}
	return 0;
}
