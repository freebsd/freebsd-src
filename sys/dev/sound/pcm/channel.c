/*-
 * Copyright (c) 1999 Cameron Grant <cg@freebsd.org>
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

#include "opt_isa.h"

#include <dev/sound/pcm/sound.h>

#include "feeder_if.h"

SND_DECLARE_FILE("$FreeBSD$");

#define MIN_CHUNK_SIZE 		256	/* for uiomove etc. */
#if 0
#define	DMA_ALIGN_THRESHOLD	4
#define	DMA_ALIGN_MASK		(~(DMA_ALIGN_THRESHOLD - 1))
#endif

#define CHN_STARTED(c)		((c)->flags & CHN_F_TRIGGERED)
#define CHN_STOPPED(c)		(!CHN_STARTED(c))
#define CHN_DIRSTR(c)		(((c)->direction == PCMDIR_PLAY) ? \
				"PCMDIR_PLAY" : "PCMDIR_REC")

#define BUF_PARENT(c, b)	\
	(((c) != NULL && (c)->parentchannel != NULL && \
	(c)->parentchannel->bufhard != NULL) ? \
	(c)->parentchannel->bufhard : (b))

#define CHN_TIMEOUT	5
#define CHN_TIMEOUT_MIN	1
#define CHN_TIMEOUT_MAX	10

/*
#define DEB(x) x
*/

int report_soft_formats = 1;
SYSCTL_INT(_hw_snd, OID_AUTO, report_soft_formats, CTLFLAG_RW,
	&report_soft_formats, 1, "report software-emulated formats");

int chn_latency = CHN_LATENCY_DEFAULT;
TUNABLE_INT("hw.snd.latency", &chn_latency);

static int
sysctl_hw_snd_latency(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = chn_latency;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return err;
	if (val < CHN_LATENCY_MIN || val > CHN_LATENCY_MAX)
		err = EINVAL;
	else
		chn_latency = val;

	return err;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, latency, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_latency, "I",
	"buffering latency (0=low ... 10=high)");

int chn_latency_profile = CHN_LATENCY_PROFILE_DEFAULT;
TUNABLE_INT("hw.snd.latency_profile", &chn_latency_profile);

static int
sysctl_hw_snd_latency_profile(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = chn_latency_profile;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return err;
	if (val < CHN_LATENCY_PROFILE_MIN || val > CHN_LATENCY_PROFILE_MAX)
		err = EINVAL;
	else
		chn_latency_profile = val;

	return err;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, latency_profile, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_latency_profile, "I",
	"buffering latency profile (0=aggresive 1=safe)");

static int chn_timeout = CHN_TIMEOUT;
TUNABLE_INT("hw.snd.timeout", &chn_timeout);
#ifdef SND_DEBUG
static int
sysctl_hw_snd_timeout(SYSCTL_HANDLER_ARGS)
{
	int err, val;

	val = chn_timeout;
	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || req->newptr == NULL)
		return err;
	if (val < CHN_TIMEOUT_MIN || val > CHN_TIMEOUT_MAX)
		err = EINVAL;
	else
		chn_timeout = val;

	return err;
}
SYSCTL_PROC(_hw_snd, OID_AUTO, timeout, CTLTYPE_INT | CTLFLAG_RW,
	0, sizeof(int), sysctl_hw_snd_timeout, "I",
	"interrupt timeout (1 - 10) seconds");
#endif

static int chn_usefrags = 0;
TUNABLE_INT("hw.snd.usefrags", &chn_usefrags);
static int chn_syncdelay = -1;
TUNABLE_INT("hw.snd.syncdelay", &chn_syncdelay);
#ifdef SND_DEBUG
SYSCTL_INT(_hw_snd, OID_AUTO, usefrags, CTLFLAG_RW,
	&chn_usefrags, 1, "prefer setfragments() over setblocksize()");
SYSCTL_INT(_hw_snd, OID_AUTO, syncdelay, CTLFLAG_RW,
	&chn_syncdelay, 1,
	"append (0-1000) millisecond trailing buffer delay on each sync");
#endif

/**
 * @brief Channel sync group lock
 *
 * Clients should acquire this lock @b without holding any channel locks
 * before touching syncgroups or the main syncgroup list.
 */
struct mtx snd_pcm_syncgroups_mtx;
MTX_SYSINIT(pcm_syncgroup, &snd_pcm_syncgroups_mtx, "PCM channel sync group lock", MTX_DEF);
/**
 * @brief syncgroups' master list
 *
 * Each time a channel syncgroup is created, it's added to this list.  This
 * list should only be accessed with @sa snd_pcm_syncgroups_mtx held.
 *
 * See SNDCTL_DSP_SYNCGROUP for more information.
 */
struct pcm_synclist snd_pcm_syncgroups = SLIST_HEAD_INITIALIZER(head);

static int chn_buildfeeder(struct pcm_channel *c);

static void
chn_lockinit(struct pcm_channel *c, int dir)
{
	switch(dir) {
	case PCMDIR_PLAY:
		c->lock = snd_mtxcreate(c->name, "pcm play channel");
		break;
	case PCMDIR_PLAY_VIRTUAL:
		c->lock = snd_mtxcreate(c->name, "pcm virtual play channel");
		break;
	case PCMDIR_REC:
		c->lock = snd_mtxcreate(c->name, "pcm record channel");
		break;
	case PCMDIR_REC_VIRTUAL:
		c->lock = snd_mtxcreate(c->name, "pcm virtual record channel");
		break;
	case 0:
		c->lock = snd_mtxcreate(c->name, "pcm fake channel");
		break;
	}

	cv_init(&c->cv, c->name);
}

static void
chn_lockdestroy(struct pcm_channel *c)
{
	snd_mtxfree(c->lock);
	cv_destroy(&c->cv);
}

/**
 * @brief Determine channel is ready for I/O
 *
 * @retval 1 = ready for I/O
 * @retval 0 = not ready for I/O
 */
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
#if 0
		lim = (c->flags & CHN_F_HAS_SIZE)? sndbuf_getblksz(bs) : 1;
#endif
		lim = c->lw;
		return (amt >= lim) ? 1 : 0;
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
	struct pcm_channel *ch;

	CHN_LOCKASSERT(c);
	if (CHN_EMPTY(c, children)) {
		if (SEL_WAITING(sndbuf_getsel(bs)) && chn_polltrigger(c))
			selwakeuppri(sndbuf_getsel(bs), PRIBIO);
	} else if (CHN_EMPTY(c, children.busy)) {
		CHN_FOREACH(ch, c, children) {
			CHN_LOCK(ch);
			chn_wakeup(ch);
			CHN_UNLOCK(ch);
		}
	} else {
		CHN_FOREACH(ch, c, children.busy) {
			CHN_LOCK(ch);
			chn_wakeup(ch);
			CHN_UNLOCK(ch);
		}
	}
	if (c->flags & CHN_F_SLEEPING)
		wakeup_one(bs);
}

static int
chn_sleep(struct pcm_channel *c, char *str, int timeout)
{
    	struct snd_dbuf *bs = c->bufsoft;
	int ret;

	CHN_LOCKASSERT(c);

	c->flags |= CHN_F_SLEEPING;
#ifdef USING_MUTEX
	ret = msleep(bs, c->lock, PRIBIO | PCATCH, str, timeout);
#else
	ret = tsleep(bs, PRIBIO | PCATCH, str, timeout);
#endif
	c->flags &= ~CHN_F_SLEEPING;

	return ret;
}

/*
 * chn_dmaupdate() tracks the status of a dma transfer,
 * updating pointers.
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
		amt = min(delta, sndbuf_getready(b));
		amt -= amt % sndbuf_getbps(b);
		if (amt > 0)
			sndbuf_dispose(b, NULL, amt);
	} else {
		amt = min(delta, sndbuf_getfree(b));
		amt -= amt % sndbuf_getbps(b);
		if (amt > 0)
		       sndbuf_acquire(b, NULL, amt);
	}
	if (snd_verbose > 3 && CHN_STARTED(c) && delta == 0) {
		device_printf(c->dev, "WARNING: %s DMA completion "
			"too fast/slow ! hwptr=%u, old=%u "
			"delta=%u amt=%u ready=%u free=%u\n",
			CHN_DIRSTR(c), hwptr, old, delta, amt,
			sndbuf_getready(b), sndbuf_getfree(b));
	}

	return delta;
}

void
chn_wrupdate(struct pcm_channel *c)
{
	int ret;

	CHN_LOCKASSERT(c);
	KASSERT(c->direction == PCMDIR_PLAY, ("chn_wrupdate on bad channel"));

	if ((c->flags & (CHN_F_MAPPED | CHN_F_VIRTUAL)) || CHN_STOPPED(c))
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
#if 0
    	DEB(
	if (c->flags & CHN_F_CLOSING) {
		sndbuf_dump(b, "b", 0x02);
		sndbuf_dump(bs, "bs", 0x02);
	})
#endif

	if ((c->flags & CHN_F_MAPPED) && !(c->flags & CHN_F_CLOSING))
		sndbuf_acquire(bs, NULL, sndbuf_getfree(bs));

	amt = sndbuf_getfree(b);
	DEB(if (amt > sndbuf_getsize(bs) &&
		    sndbuf_getbps(bs) >= sndbuf_getbps(b)) {
		printf("%s(%s): amt %d > source size %d, flags 0x%x", __func__, c->name,
		    amt, sndbuf_getsize(bs), c->flags);
	});

	ret = (amt > 0) ? sndbuf_feed(bs, b, c, c->feeder, amt) : ENOSPC;
	/*
	 * Possible xruns. There should be no empty space left in buffer.
	 */
	if (sndbuf_getfree(b) > 0)
		c->xruns++;

	if (sndbuf_getfree(b) < amt)
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
	struct snd_dbuf *bs = c->bufsoft;
	void *off;
	int ret, timeout, sz, t, p;

	CHN_LOCKASSERT(c);

	ret = 0;
	timeout = chn_timeout * hz;

	while (ret == 0 && buf->uio_resid > 0) {
		sz = min(buf->uio_resid, sndbuf_getfree(bs));
		if (sz > 0) {
			/*
			 * The following assumes that the free space in
			 * the buffer can never be less around the
			 * unlock-uiomove-lock sequence.
			 */
			while (ret == 0 && sz > 0) {
				p = sndbuf_getfreeptr(bs);
				t = min(sz, sndbuf_getsize(bs) - p);
				off = sndbuf_getbufofs(bs, p);
				CHN_UNLOCK(c);
				ret = uiomove(off, t, buf);
				CHN_LOCK(c);
				sz -= t;
				sndbuf_acquire(bs, NULL, t);
			}
			ret = 0;
			if (CHN_STOPPED(c))
				chn_start(c, 0);
		} else if (c->flags & (CHN_F_NBIO | CHN_F_NOTRIGGER)) {
			/**
			 * @todo Evaluate whether EAGAIN is truly desirable.
			 * 	 4Front drivers behave like this, but I'm
			 * 	 not sure if it at all violates the "write
			 * 	 should be allowed to block" model.
			 *
			 * 	 The idea is that, while set with CHN_F_NOTRIGGER,
			 * 	 a channel isn't playing, *but* without this we
			 * 	 end up with "interrupt timeout / channel dead".
			 */
			ret = EAGAIN;
		} else {
   			ret = chn_sleep(c, "pcmwr", timeout);
			if (ret == EAGAIN) {
				ret = EINVAL;
				c->flags |= CHN_F_DEAD;
				printf("%s: play interrupt timeout, "
				    "channel dead\n", c->name);
			} else if (ret == ERESTART || ret == EINTR)
				c->flags |= CHN_F_ABORTING;
		}
	}

	return ret;
}

#if 0
static int
chn_rddump(struct pcm_channel *c, unsigned int cnt)
{
    	struct snd_dbuf *b = c->bufhard;

	CHN_LOCKASSERT(c);
#if 0
	static u_int32_t kk = 0;
	printf("%u: dumping %d bytes\n", ++kk, cnt);
#endif
	c->xruns++;
	sndbuf_setxrun(b, sndbuf_getxrun(b) + cnt);
	return sndbuf_dispose(b, NULL, cnt);
}
#endif

/*
 * Feed new data from the read buffer. Can be called in the bottom half.
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

#if 0
	amt = sndbuf_getready(b);
	if (sndbuf_getfree(bs) < amt) {
		c->xruns++;
		amt = sndbuf_getfree(bs);
	}
#endif
	amt = sndbuf_getfree(bs);
	ret = (amt > 0) ? sndbuf_feed(b, bs, c, c->feeder, amt) : ENOSPC;

	amt = sndbuf_getready(b);
	if (amt > 0) {
		c->xruns++;
		sndbuf_dispose(b, NULL, amt);
	}

	if (sndbuf_getready(bs) > 0)
		chn_wakeup(c);

	return ret;
}

void
chn_rdupdate(struct pcm_channel *c)
{
	int ret;

	CHN_LOCKASSERT(c);
	KASSERT(c->direction == PCMDIR_REC, ("chn_rdupdate on bad channel"));

	if ((c->flags & (CHN_F_MAPPED | CHN_F_VIRTUAL)) || CHN_STOPPED(c))
		return;
	chn_trigger(c, PCMTRIG_EMLDMARD);
	chn_dmaupdate(c);
	ret = chn_rdfeed(c);
	DEB(if (ret)
		printf("chn_rdfeed: %d\n", ret);)

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
	struct snd_dbuf *bs = c->bufsoft;
	void *off;
	int ret, timeout, sz, t, p;

	CHN_LOCKASSERT(c);

	if (CHN_STOPPED(c))
		chn_start(c, 0);

	ret = 0;
	timeout = chn_timeout * hz;

	while (ret == 0 && buf->uio_resid > 0) {
		sz = min(buf->uio_resid, sndbuf_getready(bs));
		if (sz > 0) {
			/*
			 * The following assumes that the free space in
			 * the buffer can never be less around the
			 * unlock-uiomove-lock sequence.
			 */
			while (ret == 0 && sz > 0) {
				p = sndbuf_getreadyptr(bs);
				t = min(sz, sndbuf_getsize(bs) - p);
				off = sndbuf_getbufofs(bs, p);
				CHN_UNLOCK(c);
				ret = uiomove(off, t, buf);
				CHN_LOCK(c);
				sz -= t;
				sndbuf_dispose(bs, NULL, t);
			}
			ret = 0;
		} else if (c->flags & (CHN_F_NBIO | CHN_F_NOTRIGGER))
			ret = EAGAIN;
		else {
   			ret = chn_sleep(c, "pcmrd", timeout);
			if (ret == EAGAIN) {
				ret = EINVAL;
				c->flags |= CHN_F_DEAD;
				printf("%s: record interrupt timeout, "
				    "channel dead\n", c->name);
			} else if (ret == ERESTART || ret == EINTR)
				c->flags |= CHN_F_ABORTING;
		}
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
	if (CHN_STARTED(c) || ((c->flags & CHN_F_NOTRIGGER) && !force))
		return EINVAL;

	if (force) {
		i = 1;
		j = 0;
	} else {
		if (c->direction == PCMDIR_REC) {
			i = sndbuf_getfree(bs);
			j = (i > 0) ? 1 : sndbuf_getready(b);
		} else {
			if (sndbuf_getfree(bs) == 0) {
				i = 1;
				j = 0;
			} else {
				struct snd_dbuf *pb;

				pb = BUF_PARENT(c, b);
				i = sndbuf_xbytes(sndbuf_getready(bs), bs, pb);
				j = sndbuf_getbps(pb);
			}
		}
		if (snd_verbose > 3 && CHN_EMPTY(c, children))
			printf("%s: %s (%s) threshold i=%d j=%d\n",
			    __func__, CHN_DIRSTR(c),
			    (c->flags & CHN_F_VIRTUAL) ? "virtual" : "hardware",
			    i, j);
	}

	if (i >= j) {
		c->flags |= CHN_F_TRIGGERED;
		sndbuf_setrun(b, 1);
		c->feedcount = (c->flags & CHN_F_CLOSING) ? 2 : 0;
		c->interrupts = 0;
		c->xruns = 0;
		if (c->direction == PCMDIR_PLAY && c->parentchannel == NULL) {
			sndbuf_fillsilence(b);
			if (snd_verbose > 3)
				printf("%s: %s starting! (%s) (ready=%d "
				    "force=%d i=%d j=%d intrtimeout=%u "
				    "latency=%dms)\n",
				    __func__,
				    (c->flags & CHN_F_HAS_VCHAN) ?
				    "VCHAN" : "HW",
				    (c->flags & CHN_F_CLOSING) ? "closing" :
				    "running",
				    sndbuf_getready(b),
				    force, i, j, c->timeout,
				    (sndbuf_getsize(b) * 1000) /
				    (sndbuf_getbps(b) * sndbuf_getspd(b)));
		}
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
    	struct snd_dbuf *b, *bs;
	int ret, count, hcount, minflush, resid, residp, syncdelay, blksz;
	u_int32_t cflag;

	CHN_LOCKASSERT(c);

	bs = c->bufsoft;

	if ((c->flags & (CHN_F_DEAD | CHN_F_ABORTING)) ||
	    (threshold < 1 && sndbuf_getready(bs) < 1))
		return 0;

	if (c->direction != PCMDIR_PLAY)
		return EINVAL;

	/* if we haven't yet started and nothing is buffered, else start*/
	if (CHN_STOPPED(c)) {
		if (threshold > 0 || sndbuf_getready(bs) > 0) {
			ret = chn_start(c, 1);
			if (ret)
				return ret;
		} else
			return 0;
	}

	b = BUF_PARENT(c, c->bufhard);

	minflush = threshold + sndbuf_xbytes(sndbuf_getready(b), b, bs);

	syncdelay = chn_syncdelay;

	if (syncdelay < 0 && (threshold > 0 || sndbuf_getready(bs) > 0))
		minflush += sndbuf_xbytes(sndbuf_getsize(b), b, bs);

	/*
	 * Append (0-1000) millisecond trailing buffer (if needed)
	 * for slower / high latency hardwares (notably USB audio)
	 * to avoid audible truncation.
	 */
	if (syncdelay > 0)
		minflush += (sndbuf_getbps(bs) * sndbuf_getspd(bs) *
		    ((syncdelay > 1000) ? 1000 : syncdelay)) / 1000;

	minflush -= minflush % sndbuf_getbps(bs);

	if (minflush > 0) {
		threshold = min(minflush, sndbuf_getfree(bs));
		sndbuf_clear(bs, threshold);
		sndbuf_acquire(bs, NULL, threshold);
		minflush -= threshold;
	}

	resid = sndbuf_getready(bs);
	residp = resid;
	blksz = sndbuf_getblksz(b);
	if (blksz < 1) {
		printf("%s: WARNING: blksz < 1 ! maxsize=%d [%d/%d/%d]\n",
		    __func__, sndbuf_getmaxsize(b), sndbuf_getsize(b),
		    sndbuf_getblksz(b), sndbuf_getblkcnt(b));
		if (sndbuf_getblkcnt(b) > 0)
			blksz = sndbuf_getsize(b) / sndbuf_getblkcnt(b);
		if (blksz < 1)
			blksz = 1;
	}
	count = sndbuf_xbytes(minflush + resid, bs, b) / blksz;
	hcount = count;
	ret = 0;

	if (snd_verbose > 3)
		printf("%s: [begin] timeout=%d count=%d "
		    "minflush=%d resid=%d\n", __func__, c->timeout, count,
		    minflush, resid);

	cflag = c->flags & CHN_F_CLOSING;
	c->flags |= CHN_F_CLOSING;
	while (count > 0 && (resid > 0 || minflush > 0)) {
		ret = chn_sleep(c, "pcmsyn", c->timeout);
    		if (ret == ERESTART || ret == EINTR) {
			c->flags |= CHN_F_ABORTING;
			break;
		}
		if (ret == 0 || ret == EAGAIN) {
			resid = sndbuf_getready(bs);
			if (resid == residp) {
				--count;
				if (snd_verbose > 3)
					printf("%s: [stalled] timeout=%d "
					    "count=%d hcount=%d "
					    "resid=%d minflush=%d\n",
					    __func__, c->timeout, count,
					    hcount, resid, minflush);
			} else if (resid < residp && count < hcount) {
				++count;
				if (snd_verbose > 3)
					printf("%s: [resume] timeout=%d "
					    "count=%d hcount=%d "
					    "resid=%d minflush=%d\n",
					    __func__, c->timeout, count,
					    hcount, resid, minflush);
			}
			if (minflush > 0 && sndbuf_getfree(bs) > 0) {
				threshold = min(minflush,
				    sndbuf_getfree(bs));
				sndbuf_clear(bs, threshold);
				sndbuf_acquire(bs, NULL, threshold);
				resid = sndbuf_getready(bs);
				minflush -= threshold;
			}
			residp = resid;
		}
	}
	c->flags &= ~CHN_F_CLOSING;
	c->flags |= cflag;

	if (snd_verbose > 3)
		printf("%s: timeout=%d count=%d hcount=%d resid=%d residp=%d "
		    "minflush=%d ret=%d\n",
		    __func__, c->timeout, count, hcount, resid, residp,
		    minflush, ret);

    	return 0;
}

/* called externally, handle locking */
int
chn_poll(struct pcm_channel *c, int ev, struct thread *td)
{
	struct snd_dbuf *bs = c->bufsoft;
	int ret;

	CHN_LOCKASSERT(c);
    	if (!(c->flags & (CHN_F_MAPPED | CHN_F_TRIGGERED)))
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
	if (CHN_STOPPED(c))
		return 0;
	c->flags |= CHN_F_ABORTING;

	c->flags &= ~CHN_F_TRIGGERED;
	/* kill the channel */
	chn_trigger(c, PCMTRIG_ABORT);
	sndbuf_setrun(b, 0);
	if (!(c->flags & CHN_F_VIRTUAL))
		chn_dmaupdate(c);
    	missing = sndbuf_getready(bs);

	c->flags &= ~CHN_F_ABORTING;
	return missing;
}

/*
 * this routine tries to flush the dma transfer. It is called
 * on a close of a playback channel.
 * first, if there is data in the buffer, but the dma has not yet
 * begun, we need to start it.
 * next, we wait for the play buffer to drain
 * finally, we stop the dma.
 *
 * called from: dsp_close, not valid for record channels.
 */

int
chn_flush(struct pcm_channel *c)
{
    	struct snd_dbuf *b = c->bufhard;

	CHN_LOCKASSERT(c);
	KASSERT(c->direction == PCMDIR_PLAY, ("chn_flush on bad channel"));
    	DEB(printf("chn_flush: c->flags 0x%08x\n", c->flags));

	c->flags |= CHN_F_CLOSING;
	chn_sync(c, 0);
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

static struct afmtstr_table default_afmtstr_table[] = {
	{  "alaw", AFMT_A_LAW  }, { "mulaw", AFMT_MU_LAW },
	{    "u8", AFMT_U8     }, {    "s8", AFMT_S8     },
	{ "s16le", AFMT_S16_LE }, { "s16be", AFMT_S16_BE },
	{ "u16le", AFMT_U16_LE }, { "u16be", AFMT_U16_BE },
	{ "s24le", AFMT_S24_LE }, { "s24be", AFMT_S24_BE },
	{ "u24le", AFMT_U24_LE }, { "u24be", AFMT_U24_BE },
	{ "s32le", AFMT_S32_LE }, { "s32be", AFMT_S32_BE },
	{ "u32le", AFMT_U32_LE }, { "u32be", AFMT_U32_BE },
	{    NULL, 0           },
};

int
afmtstr_swap_sign(char *s)
{
	if (s == NULL || strlen(s) < 2) /* full length of "s8" */
		return 0;
	if (*s == 's')
		*s = 'u';
	else if (*s == 'u')
		*s = 's';
	else
		return 0;
	return 1;
}

int
afmtstr_swap_endian(char *s)
{
	if (s == NULL || strlen(s) < 5) /* full length of "s16le" */
		return 0;
	if (s[3] == 'l')
		s[3] = 'b';
	else if (s[3] == 'b')
		s[3] = 'l';
	else
		return 0;
	return 1;
}

u_int32_t
afmtstr2afmt(struct afmtstr_table *tbl, const char *s, int stereo)
{
	size_t fsz, sz;

	sz = (s == NULL) ? 0 : strlen(s);

	if (sz > 1) {

		if (tbl == NULL)
			tbl = default_afmtstr_table;

		for (; tbl->fmtstr != NULL; tbl++) {
			fsz = strlen(tbl->fmtstr);
			if (sz < fsz)
				continue;
			if (strncmp(s, tbl->fmtstr, fsz) != 0)
				continue;
			if (fsz == sz)
				return tbl->format |
					    ((stereo) ? AFMT_STEREO : 0);
			if ((sz - fsz) < 2 || s[fsz] != ':')
				break;
			/*
			 * For now, just handle mono/stereo.
			 */
			if ((s[fsz + 2] == '\0' && (s[fsz + 1] == 'm' ||
				    s[fsz + 1] == '1')) ||
				    strcmp(s + fsz + 1, "mono") == 0)
				return tbl->format;
			if ((s[fsz + 2] == '\0' && (s[fsz + 1] == 's' ||
				    s[fsz + 1] == '2')) ||
				    strcmp(s + fsz + 1, "stereo") == 0)
				return tbl->format | AFMT_STEREO;
			break;
		}
	}

	return 0;
}

u_int32_t
afmt2afmtstr(struct afmtstr_table *tbl, u_int32_t afmt, char *dst,
					size_t len, int type, int stereo)
{
	u_int32_t fmt = 0;
	char *fmtstr = NULL, *tag = "";

	if (tbl == NULL)
		tbl = default_afmtstr_table;

	for (; tbl->format != 0; tbl++) {
		if (tbl->format == 0)
			break;
		if ((afmt & ~AFMT_STEREO) != tbl->format)
			continue;
		fmt = afmt;
		fmtstr = tbl->fmtstr;
		break;
	}

	if (fmt != 0 && fmtstr != NULL && dst != NULL && len > 0) {
		strlcpy(dst, fmtstr, len);
		switch (type) {
		case AFMTSTR_SIMPLE:
			tag = (fmt & AFMT_STEREO) ? ":s" : ":m";
			break;
		case AFMTSTR_NUM:
			tag = (fmt & AFMT_STEREO) ? ":2" : ":1";
			break;
		case AFMTSTR_FULL:
			tag = (fmt & AFMT_STEREO) ? ":stereo" : ":mono";
			break;
		case AFMTSTR_NONE:
		default:
			break;
		}
		if (strlen(tag) > 0 && ((stereo && !(fmt & AFMT_STEREO)) || \
			    (!stereo && (fmt & AFMT_STEREO))))
			strlcat(dst, tag, len);
	}

	return fmt;
}

int
chn_reset(struct pcm_channel *c, u_int32_t fmt)
{
	int hwspd, r;

	CHN_LOCKASSERT(c);
	c->feedcount = 0;
	c->flags &= CHN_F_RESET;
	c->interrupts = 0;
	c->timeout = 1;
	c->xruns = 0;

	r = CHANNEL_RESET(c->methods, c->devinfo);
	if (fmt != 0) {
#if 0
		hwspd = DSP_DEFAULT_SPEED;
		/* only do this on a record channel until feederbuilder works */
		if (c->direction == PCMDIR_REC)
			RANGE(hwspd, chn_getcaps(c)->minspeed, chn_getcaps(c)->maxspeed);
		c->speed = hwspd;
#endif
		hwspd = chn_getcaps(c)->minspeed;
		c->speed = hwspd;

		if (r == 0)
			r = chn_setformat(c, fmt);
		if (r == 0)
			r = chn_setspeed(c, hwspd);
#if 0
		if (r == 0)
			r = chn_setvolume(c, 100, 100);
#endif
	}
	if (r == 0)
		r = chn_setlatency(c, chn_latency);
	if (r == 0) {
		chn_resetbuf(c);
		r = CHANNEL_RESETDONE(c->methods, c->devinfo);
	}
	return r;
}

int
chn_init(struct pcm_channel *c, void *devinfo, int dir, int direction)
{
	struct feeder_class *fc;
	struct snd_dbuf *b, *bs;
	int ret;

	if (chn_timeout < CHN_TIMEOUT_MIN || chn_timeout > CHN_TIMEOUT_MAX)
		chn_timeout = CHN_TIMEOUT;

	chn_lockinit(c, dir);

	b = NULL;
	bs = NULL;
	CHN_INIT(c, children);
	CHN_INIT(c, children.busy);
	c->devinfo = NULL;
	c->feeder = NULL;
	c->latency = -1;
	c->timeout = 1;

	ret = ENOMEM;
	b = sndbuf_create(c->dev, c->name, "primary", c);
	if (b == NULL)
		goto out;
	bs = sndbuf_create(c->dev, c->name, "secondary", c);
	if (bs == NULL)
		goto out;

	CHN_LOCK(c);

	ret = EINVAL;
	fc = feeder_getclass(NULL);
	if (fc == NULL)
		goto out;
	if (chn_addfeeder(c, fc, NULL))
		goto out;

	/*
	 * XXX - sndbuf_setup() & sndbuf_resize() expect to be called
	 *	 with the channel unlocked because they are also called
	 *	 from driver methods that don't know about locking
	 */
	CHN_UNLOCK(c);
	sndbuf_setup(bs, NULL, 0);
	CHN_LOCK(c);
	c->bufhard = b;
	c->bufsoft = bs;
	c->flags = 0;
	c->feederflags = 0;
	c->sm = NULL;

	ret = ENODEV;
	CHN_UNLOCK(c); /* XXX - Unlock for CHANNEL_INIT() malloc() call */
	c->devinfo = CHANNEL_INIT(c->methods, devinfo, b, c, direction);
	CHN_LOCK(c);
	if (c->devinfo == NULL)
		goto out;

	ret = ENOMEM;
	if ((sndbuf_getsize(b) == 0) && ((c->flags & CHN_F_VIRTUAL) == 0))
		goto out;

	ret = chn_setdir(c, direction);
	if (ret)
		goto out;

	ret = sndbuf_setfmt(b, AFMT_U8);
	if (ret)
		goto out;

	ret = sndbuf_setfmt(bs, AFMT_U8);
	if (ret)
		goto out;

	/**
	 * @todo Should this be moved somewhere else?  The primary buffer
	 * 	 is allocated by the driver or via DMA map setup, and tmpbuf
	 * 	 seems to only come into existence in sndbuf_resize().
	 */
	if (c->direction == PCMDIR_PLAY) {
		bs->sl = sndbuf_getmaxsize(bs);
		bs->shadbuf = malloc(bs->sl, M_DEVBUF, M_NOWAIT);
		if (bs->shadbuf == NULL) {
			ret = ENOMEM;
			goto out;
		}
	}

out:
	CHN_UNLOCK(c);
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

	return 0;
}

int
chn_kill(struct pcm_channel *c)
{
    	struct snd_dbuf *b = c->bufhard;
    	struct snd_dbuf *bs = c->bufsoft;

	if (CHN_STARTED(c)) {
		CHN_LOCK(c);
		chn_trigger(c, PCMTRIG_ABORT);
		CHN_UNLOCK(c);
	}
	while (chn_removefeeder(c) == 0)
		;
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
#ifdef DEV_ISA
    	struct snd_dbuf *b = c->bufhard;
#endif
	int r;

	CHN_LOCKASSERT(c);
	c->direction = dir;
	r = CHANNEL_SETDIR(c->methods, c->devinfo, c->direction);
#ifdef DEV_ISA
	if (!r && SND_DMA(b))
		sndbuf_dmasetdir(b, c->direction);
#endif
	return r;
}

int
chn_setvolume(struct pcm_channel *c, int left, int right)
{
	CHN_LOCKASSERT(c);
	/* should add a feeder for volume changing if channel returns -1 */
	if (left > 100)
		left = 100;
	if (left < 0)
		left = 0;
	if (right > 100)
		right = 100;
	if (right < 0)
		right = 0;
	c->volume = left | (right << 8);
	return 0;
}

static u_int32_t
round_pow2(u_int32_t v)
{
	u_int32_t ret;

	if (v < 2)
		v = 2;
	ret = 0;
	while (v >> ret)
		ret++;
	ret = 1 << (ret - 1);
	while (ret < v)
		ret <<= 1;
	return ret;
}

static u_int32_t
round_blksz(u_int32_t v, int round)
{
	u_int32_t ret, tmp;

	if (round < 1)
		round = 1;

	ret = min(round_pow2(v), CHN_2NDBUFMAXSIZE >> 1);

	if (ret > v && (ret >> 1) > 0 && (ret >> 1) >= ((v * 3) >> 2))
		ret >>= 1;

	tmp = ret - (ret % round);
	while (tmp < 16 || tmp < round) {
		ret <<= 1;
		tmp = ret - (ret % round);
	}

	return ret;
}

/*
 * 4Front call it DSP Policy, while we call it "Latency Profile". The idea
 * is to keep 2nd buffer short so that it doesn't cause long queue during
 * buffer transfer.
 *
 *    Latency reference table for 48khz stereo 16bit: (PLAY)
 *
 *      +---------+------------+-----------+------------+
 *      | Latency | Blockcount | Blocksize | Buffersize |
 *      +---------+------------+-----------+------------+
 *      |     0   |       2    |   64      |    128     |
 *      +---------+------------+-----------+------------+
 *      |     1   |       4    |   128     |    512     |
 *      +---------+------------+-----------+------------+
 *      |     2   |       8    |   512     |    4096    |
 *      +---------+------------+-----------+------------+
 *      |     3   |      16    |   512     |    8192    |
 *      +---------+------------+-----------+------------+
 *      |     4   |      32    |   512     |    16384   |
 *      +---------+------------+-----------+------------+
 *      |     5   |      32    |   1024    |    32768   |
 *      +---------+------------+-----------+------------+
 *      |     6   |      16    |   2048    |    32768   |
 *      +---------+------------+-----------+------------+
 *      |     7   |       8    |   4096    |    32768   |
 *      +---------+------------+-----------+------------+
 *      |     8   |       4    |   8192    |    32768   |
 *      +---------+------------+-----------+------------+
 *      |     9   |       2    |   16384   |    32768   |
 *      +---------+------------+-----------+------------+
 *      |    10   |       2    |   32768   |    65536   |
 *      +---------+------------+-----------+------------+
 *
 * Recording need a different reference table. All we care is
 * gobbling up everything within reasonable buffering threshold.
 *
 *    Latency reference table for 48khz stereo 16bit: (REC)
 *
 *      +---------+------------+-----------+------------+
 *      | Latency | Blockcount | Blocksize | Buffersize |
 *      +---------+------------+-----------+------------+
 *      |     0   |     512    |   32      |    16384   |
 *      +---------+------------+-----------+------------+
 *      |     1   |     256    |   64      |    16384   |
 *      +---------+------------+-----------+------------+
 *      |     2   |     128    |   128     |    16384   |
 *      +---------+------------+-----------+------------+
 *      |     3   |      64    |   256     |    16384   |
 *      +---------+------------+-----------+------------+
 *      |     4   |      32    |   512     |    16384   |
 *      +---------+------------+-----------+------------+
 *      |     5   |      32    |   1024    |    32768   |
 *      +---------+------------+-----------+------------+
 *      |     6   |      16    |   2048    |    32768   |
 *      +---------+------------+-----------+------------+
 *      |     7   |       8    |   4096    |    32768   |
 *      +---------+------------+-----------+------------+
 *      |     8   |       4    |   8192    |    32768   |
 *      +---------+------------+-----------+------------+
 *      |     9   |       2    |   16384   |    32768   |
 *      +---------+------------+-----------+------------+
 *      |    10   |       2    |   32768   |    65536   |
 *      +---------+------------+-----------+------------+
 *
 * Calculations for other data rate are entirely based on these reference
 * tables. For normal operation, Latency 5 seems give the best, well
 * balanced performance for typical workload. Anything below 5 will
 * eat up CPU to keep up with increasing context switches because of
 * shorter buffer space and usually require the application to handle it
 * aggresively through possibly real time programming technique.
 *
 */
#define CHN_LATENCY_PBLKCNT_REF				\
	{{1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 1},		\
	{1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 1}}
#define CHN_LATENCY_PBUFSZ_REF				\
	{{7, 9, 12, 13, 14, 15, 15, 15, 15, 15, 16},	\
	{11, 12, 13, 14, 15, 16, 16, 16, 16, 16, 17}}

#define CHN_LATENCY_RBLKCNT_REF				\
	{{9, 8, 7, 6, 5, 5, 4, 3, 2, 1, 1},		\
	{9, 8, 7, 6, 5, 5, 4, 3, 2, 1, 1}}
#define CHN_LATENCY_RBUFSZ_REF				\
	{{14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 16},	\
	{15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 17}}

#define CHN_LATENCY_DATA_REF	192000 /* 48khz stereo 16bit ~ 48000 x 2 x 2 */

static int
chn_calclatency(int dir, int latency, int bps, u_int32_t datarate,
				u_int32_t max, int *rblksz, int *rblkcnt)
{
	static int pblkcnts[CHN_LATENCY_PROFILE_MAX + 1][CHN_LATENCY_MAX + 1] =
	    CHN_LATENCY_PBLKCNT_REF;
	static int  pbufszs[CHN_LATENCY_PROFILE_MAX + 1][CHN_LATENCY_MAX + 1] =
	    CHN_LATENCY_PBUFSZ_REF;
	static int rblkcnts[CHN_LATENCY_PROFILE_MAX + 1][CHN_LATENCY_MAX + 1] =
	    CHN_LATENCY_RBLKCNT_REF;
	static int  rbufszs[CHN_LATENCY_PROFILE_MAX + 1][CHN_LATENCY_MAX + 1] =
	    CHN_LATENCY_RBUFSZ_REF;
	u_int32_t bufsz;
	int lprofile, blksz, blkcnt;

	if (latency < CHN_LATENCY_MIN || latency > CHN_LATENCY_MAX ||
	    bps < 1 || datarate < 1 ||
	    !(dir == PCMDIR_PLAY || dir == PCMDIR_REC)) {
		if (rblksz != NULL)
			*rblksz = CHN_2NDBUFMAXSIZE >> 1;
		if (rblkcnt != NULL)
			*rblkcnt = 2;
		printf("%s: FAILED dir=%d latency=%d bps=%d "
		    "datarate=%u max=%u\n",
		    __func__, dir, latency, bps, datarate, max);
		return CHN_2NDBUFMAXSIZE;
	}

	lprofile = chn_latency_profile;

	if (dir == PCMDIR_PLAY) {
		blkcnt = pblkcnts[lprofile][latency];
		bufsz = pbufszs[lprofile][latency];
	} else {
		blkcnt = rblkcnts[lprofile][latency];
		bufsz = rbufszs[lprofile][latency];
	}

	bufsz = round_pow2(snd_xbytes(1 << bufsz, CHN_LATENCY_DATA_REF,
	    datarate));
	if (bufsz > max)
		bufsz = max;
	blksz = round_blksz(bufsz >> blkcnt, bps);

	if (rblksz != NULL)
		*rblksz = blksz;
	if (rblkcnt != NULL)
		*rblkcnt = 1 << blkcnt;

	return blksz << blkcnt;
}

static int
chn_resizebuf(struct pcm_channel *c, int latency,
					int blkcnt, int blksz)
{
	struct snd_dbuf *b, *bs, *pb;
	int sblksz, sblkcnt, hblksz, hblkcnt, limit = 1;
	int ret;

	CHN_LOCKASSERT(c);

	if ((c->flags & (CHN_F_MAPPED | CHN_F_TRIGGERED)) ||
	    !(c->direction == PCMDIR_PLAY || c->direction == PCMDIR_REC))
		return EINVAL;

	if (latency == -1) {
		c->latency = -1;
		latency = chn_latency;
	} else if (latency == -2) {
		latency = c->latency;
		if (latency < CHN_LATENCY_MIN || latency > CHN_LATENCY_MAX)
			latency = chn_latency;
	} else if (latency < CHN_LATENCY_MIN || latency > CHN_LATENCY_MAX)
		return EINVAL;
	else {
		c->latency = latency;
		limit = 0;
	}

	bs = c->bufsoft;
	b = c->bufhard;

	if (!(blksz == 0 || blkcnt == -1) &&
	    (blksz < 16 || blksz < sndbuf_getbps(bs) || blkcnt < 2 ||
	    (blksz * blkcnt) > CHN_2NDBUFMAXSIZE))
		return EINVAL;

	chn_calclatency(c->direction, latency, sndbuf_getbps(bs),
	    sndbuf_getbps(bs) * sndbuf_getspd(bs), CHN_2NDBUFMAXSIZE,
	    &sblksz, &sblkcnt);

	if (blksz == 0 || blkcnt == -1) {
		if (blkcnt == -1)
			c->flags &= ~CHN_F_HAS_SIZE;
		if (c->flags & CHN_F_HAS_SIZE) {
			blksz = sndbuf_getblksz(bs);
			blkcnt = sndbuf_getblkcnt(bs);
		}
	} else
		c->flags |= CHN_F_HAS_SIZE;

	if (c->flags & CHN_F_HAS_SIZE) {
		/*
		 * The application has requested their own blksz/blkcnt.
		 * Just obey with it, and let them toast alone. We can
		 * clamp it to the nearest latency profile, but that would
		 * defeat the purpose of having custom control. The least
		 * we can do is round it to the nearest ^2 and align it.
		 */
		sblksz = round_blksz(blksz, sndbuf_getbps(bs));
		sblkcnt = round_pow2(blkcnt);
		limit = 0;
	}

	if (c->parentchannel != NULL) {
		pb = BUF_PARENT(c, NULL);
		CHN_UNLOCK(c);
		chn_notify(c->parentchannel, CHN_N_BLOCKSIZE);
		CHN_LOCK(c);
		limit = (limit != 0 && pb != NULL) ?
		    sndbuf_xbytes(sndbuf_getsize(pb), pb, bs) : 0;
		c->timeout = c->parentchannel->timeout;
	} else {
		hblkcnt = 2;
		if (c->flags & CHN_F_HAS_SIZE) {
			hblksz = round_blksz(sndbuf_xbytes(sblksz, bs, b),
			    sndbuf_getbps(b));
			hblkcnt = round_pow2(sndbuf_getblkcnt(bs));
		} else
			chn_calclatency(c->direction, latency,
			    sndbuf_getbps(b),
			    sndbuf_getbps(b) * sndbuf_getspd(b),
			    CHN_2NDBUFMAXSIZE, &hblksz, &hblkcnt);

		if ((hblksz << 1) > sndbuf_getmaxsize(b))
			hblksz = round_blksz(sndbuf_getmaxsize(b) >> 1,
			    sndbuf_getbps(b));

		while ((hblksz * hblkcnt) > sndbuf_getmaxsize(b)) {
			if (hblkcnt < 4)
				hblksz >>= 1;
			else
				hblkcnt >>= 1;
		}

		hblksz -= hblksz % sndbuf_getbps(b);

#if 0
		hblksz = sndbuf_getmaxsize(b) >> 1;
		hblksz -= hblksz % sndbuf_getbps(b);
		hblkcnt = 2;
#endif

		CHN_UNLOCK(c);
		if (chn_usefrags == 0 ||
		    CHANNEL_SETFRAGMENTS(c->methods, c->devinfo,
		    hblksz, hblkcnt) < 1)
			sndbuf_setblksz(b, CHANNEL_SETBLOCKSIZE(c->methods,
			    c->devinfo, hblksz));
		CHN_LOCK(c);

		if (!CHN_EMPTY(c, children)) {
			sblksz = round_blksz(
			    sndbuf_xbytes(sndbuf_getsize(b) >> 1, b, bs),
			    sndbuf_getbps(bs));
			sblkcnt = 2;
			limit = 0;
		} else if (limit != 0)
			limit = sndbuf_xbytes(sndbuf_getsize(b), b, bs);

		/*
		 * Interrupt timeout
		 */
		c->timeout = ((u_int64_t)hz * sndbuf_getsize(b)) /
		    ((u_int64_t)sndbuf_getspd(b) * sndbuf_getbps(b));
		if (c->timeout < 1)
			c->timeout = 1;
	}

	if (limit > CHN_2NDBUFMAXSIZE)
		limit = CHN_2NDBUFMAXSIZE;

#if 0
	while (limit > 0 && (sblksz * sblkcnt) > limit) {
		if (sblkcnt < 4)
			break;
		sblkcnt >>= 1;
	}
#endif

	while ((sblksz * sblkcnt) < limit)
		sblkcnt <<= 1;

	while ((sblksz * sblkcnt) > CHN_2NDBUFMAXSIZE) {
		if (sblkcnt < 4)
			sblksz >>= 1;
		else
			sblkcnt >>= 1;
	}

	sblksz -= sblksz % sndbuf_getbps(bs);

	if (sndbuf_getblkcnt(bs) != sblkcnt || sndbuf_getblksz(bs) != sblksz ||
	    sndbuf_getsize(bs) != (sblkcnt * sblksz)) {
		ret = sndbuf_remalloc(bs, sblkcnt, sblksz);
		if (ret != 0) {
			printf("%s: Failed: %d %d\n", __func__,
			    sblkcnt, sblksz);
			return ret;
		}
	}

	/*
	 * OSSv4 docs: "By default OSS will set the low water level equal
	 * to the fragment size which is optimal in most cases."
	 */
	c->lw = sndbuf_getblksz(bs);
	chn_resetbuf(c);

	if (snd_verbose > 3)
		printf("%s: %s (%s) timeout=%u "
		    "b[%d/%d/%d] bs[%d/%d/%d] limit=%d\n",
		    __func__, CHN_DIRSTR(c),
		    (c->flags & CHN_F_VIRTUAL) ? "virtual" : "hardware",
		    c->timeout,
		    sndbuf_getsize(b), sndbuf_getblksz(b),
		    sndbuf_getblkcnt(b),
		    sndbuf_getsize(bs), sndbuf_getblksz(bs),
		    sndbuf_getblkcnt(bs), limit);

	return 0;
}

int
chn_setlatency(struct pcm_channel *c, int latency)
{
	CHN_LOCKASSERT(c);
	/* Destroy blksz/blkcnt, enforce latency profile. */
	return chn_resizebuf(c, latency, -1, 0);
}

int
chn_setblocksize(struct pcm_channel *c, int blkcnt, int blksz)
{
	CHN_LOCKASSERT(c);
	/* Destroy latency profile, enforce blksz/blkcnt */
	return chn_resizebuf(c, -1, blkcnt, blksz);
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
	if (CHN_STOPPED(c)) {
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
		/*
		 * Used to be 500. It was too big!
		 */
		if (delta > feeder_rate_round)
			c->feederflags |= 1 << FEEDER_RATE;
		else
			sndbuf_setspd(bs, sndbuf_getspd(b));

		r = chn_buildfeeder(c);
		DEB(printf("r = %d\n", r));
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
		if (!r)
			r = CHANNEL_SETFORMAT(c->methods, c->devinfo,
							sndbuf_getfmt(b));
		if (!r)
			sndbuf_setfmt(bs, c->format);
		if (!r)
			r = chn_resizebuf(c, -2, 0, 0);
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
		if (snd_verbose > 3)
			printf("Failed to set speed %d falling back to %d\n",
			    speed, oldspeed);
		r = chn_tryspeed(c, oldspeed);
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
	if (CHN_STOPPED(c)) {
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
		if (snd_verbose > 3)
			printf("Format change 0x%08x failed, reverting to 0x%08x\n",
			    fmt, oldfmt);
		chn_tryformat(c, oldfmt);
	}
	return r;
}

int
chn_trigger(struct pcm_channel *c, int go)
{
#ifdef DEV_ISA
    	struct snd_dbuf *b = c->bufhard;
#endif
	struct snddev_info *d = c->parentsnddev;
	int ret;

	CHN_LOCKASSERT(c);
#ifdef DEV_ISA
	if (SND_DMA(b) && (go == PCMTRIG_EMLDMAWR || go == PCMTRIG_EMLDMARD))
		sndbuf_dmabounce(b);
#endif
	if ((go == PCMTRIG_START || go == PCMTRIG_STOP ||
	    go == PCMTRIG_ABORT) && go == c->trigger)
			return 0;

	ret = CHANNEL_TRIGGER(c->methods, c->devinfo, go);

	if (ret == 0) {
		switch (go) {
		case PCMTRIG_START:
			if (snd_verbose > 3)
				device_printf(c->dev,
				    "%s() %s: calling go=0x%08x , "
				    "prev=0x%08x\n", __func__, c->name, go,
				    c->trigger);
			if (c->trigger != PCMTRIG_START) {
				c->trigger = go;
				CHN_UNLOCK(c);
				pcm_lock(d);
				CHN_INSERT_HEAD(d, c, channels.pcm.busy);
				pcm_unlock(d);
				CHN_LOCK(c);
			}
			break;
		case PCMTRIG_STOP:
		case PCMTRIG_ABORT:
			if (snd_verbose > 3)
				device_printf(c->dev,
				    "%s() %s: calling go=0x%08x , "
				    "prev=0x%08x\n", __func__, c->name, go,
				    c->trigger);
			if (c->trigger == PCMTRIG_START) {
				c->trigger = go;
				CHN_UNLOCK(c);
				pcm_lock(d);
				CHN_REMOVE(d, c, channels.pcm.busy);
				pcm_unlock(d);
				CHN_LOCK(c);
			}
			break;
		default:
			break;
		}
	}

	return ret;
}

/**
 * @brief Queries sound driver for sample-aligned hardware buffer pointer index
 *
 * This function obtains the hardware pointer location, then aligns it to
 * the current bytes-per-sample value before returning.  (E.g., a channel
 * running in 16 bit stereo mode would require 4 bytes per sample, so a
 * hwptr value ranging from 32-35 would be returned as 32.)
 *
 * @param c	PCM channel context	
 * @returns 	sample-aligned hardware buffer pointer index
 */
int
chn_getptr(struct pcm_channel *c)
{
#if 0
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
#endif
	int hwptr;

	CHN_LOCKASSERT(c);
	hwptr = (CHN_STARTED(c)) ? CHANNEL_GETPTR(c->methods, c->devinfo) : 0;
	return (hwptr - (hwptr % sndbuf_getbps(c->bufhard)));
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
		fmts |= AFMT_MU_LAW|AFMT_A_LAW|AFMT_U32_LE|AFMT_U32_BE|
		    AFMT_S32_LE|AFMT_S32_BE|AFMT_U24_LE|AFMT_U24_BE|
		    AFMT_S24_LE|AFMT_S24_BE|AFMT_U16_LE|AFMT_U16_BE|
		    AFMT_S16_LE|AFMT_S16_BE|AFMT_U8|AFMT_S8;

	return fmts;
}

static int
chn_buildfeeder(struct pcm_channel *c)
{
	struct feeder_class *fc;
	struct pcm_feederdesc desc;
	u_int32_t tmp[2], type, flags, hwfmt, *fmtlist;
	int err;
	char fmtstr[AFMTSTR_MAXSZ];

	CHN_LOCKASSERT(c);
	while (chn_removefeeder(c) == 0)
		;
	KASSERT((c->feeder == NULL), ("feeder chain not empty"));

	c->align = sndbuf_getalign(c->bufsoft);

	if (CHN_EMPTY(c, children) || c->direction == PCMDIR_REC) {
		/*
		 * Virtual rec need this.
		 */
		fc = feeder_getclass(NULL);
		KASSERT(fc != NULL, ("can't find root feeder"));

		err = chn_addfeeder(c, fc, NULL);
		if (err) {
			DEB(printf("can't add root feeder, err %d\n", err));

			return err;
		}
		c->feeder->desc->out = c->format;
	} else if (c->direction == PCMDIR_PLAY) {
		if (c->flags & CHN_F_HAS_VCHAN) {
			desc.type = FEEDER_MIXER;
			desc.in = c->format;
		} else {
			DEB(printf("can't decide which feeder type to use!\n"));
			return EOPNOTSUPP;
		}
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
	} else
		return EOPNOTSUPP;

	c->feederflags &= ~(1 << FEEDER_VOLUME);
	if (c->direction == PCMDIR_PLAY &&
	    !(c->flags & CHN_F_VIRTUAL) && c->parentsnddev &&
	    (c->parentsnddev->flags & SD_F_SOFTPCMVOL) &&
	    c->parentsnddev->mixer_dev)
		c->feederflags |= 1 << FEEDER_VOLUME;
	if (!(c->flags & CHN_F_VIRTUAL) && c->parentsnddev &&
	    ((c->direction == PCMDIR_PLAY &&
	    (c->parentsnddev->flags & SD_F_PSWAPLR)) ||
	    (c->direction == PCMDIR_REC &&
	    (c->parentsnddev->flags & SD_F_RSWAPLR))))
		c->feederflags |= 1 << FEEDER_SWAPLR;
	flags = c->feederflags;
	fmtlist = chn_getcaps(c)->fmtlist;

	DEB(printf("feederflags %x\n", flags));

	for (type = FEEDER_RATE; type < FEEDER_LAST; type++) {
		if (flags & (1 << type)) {
			desc.type = type;
			desc.in = 0;
			desc.out = 0;
			desc.flags = 0;
			DEB(printf("find feeder type %d, ", type));
			if (type == FEEDER_VOLUME || type == FEEDER_RATE) {
				if (c->feeder->desc->out & AFMT_32BIT)
					strlcpy(fmtstr,"s32le", sizeof(fmtstr));
				else if (c->feeder->desc->out & AFMT_24BIT)
					strlcpy(fmtstr, "s24le", sizeof(fmtstr));
				else {
					/*
					 * 8bit doesn't provide enough headroom
					 * for proper processing without
					 * creating too much noises. Force to
					 * 16bit instead.
					 */
					strlcpy(fmtstr, "s16le", sizeof(fmtstr));
				}
				if (!(c->feeder->desc->out & AFMT_8BIT) &&
					    c->feeder->desc->out & AFMT_BIGENDIAN)
					afmtstr_swap_endian(fmtstr);
				if (!(c->feeder->desc->out & (AFMT_A_LAW | AFMT_MU_LAW)) &&
					    !(c->feeder->desc->out & AFMT_SIGNED))
					afmtstr_swap_sign(fmtstr);
				desc.in = afmtstr2afmt(NULL, fmtstr, AFMTSTR_MONO_RETURN);
				if (desc.in == 0)
					desc.in = AFMT_S16_LE;
				/* feeder_volume need stereo processing */
				if (type == FEEDER_VOLUME ||
					    c->feeder->desc->out & AFMT_STEREO)
					desc.in |= AFMT_STEREO;
				desc.out = desc.in;
			} else if (type == FEEDER_SWAPLR) {
				desc.in = c->feeder->desc->out;
				desc.in |= AFMT_STEREO;
				desc.out = desc.in;
			}

			fc = feeder_getclass(&desc);
			DEB(printf("got %p\n", fc));
			if (fc == NULL) {
				DEB(printf("can't find required feeder type %d\n", type));

				return EOPNOTSUPP;
			}

			if (desc.in == 0 || desc.out == 0)
				desc = *fc->desc;

 			DEB(printf("build fmtchain from 0x%08x to 0x%08x: ", c->feeder->desc->out, fc->desc->in));
			tmp[0] = desc.in;
			tmp[1] = 0;
			if (chn_fmtchain(c, tmp) == 0) {
				DEB(printf("failed\n"));

				return ENODEV;
			}
 			DEB(printf("ok\n"));

			err = chn_addfeeder(c, fc, &desc);
			if (err) {
				DEB(printf("can't add feeder %p, output 0x%x, err %d\n", fc, fc->desc->out, err));

				return err;
			}
			DEB(printf("added feeder %p, output 0x%x\n", fc, c->feeder->desc->out));
		}
	}

 	if (c->direction == PCMDIR_REC) {
	 	tmp[0] = c->format;
 		tmp[1] = 0;
 		hwfmt = chn_fmtchain(c, tmp);
 	} else
 		hwfmt = chn_fmtchain(c, fmtlist);

	if (hwfmt == 0 || !fmtvalid(hwfmt, fmtlist)) {
		DEB(printf("Invalid hardware format: 0x%08x\n", hwfmt));
		return ENODEV;
	} else if (c->direction == PCMDIR_REC && !CHN_EMPTY(c, children)) {
		/*
		 * Kind of awkward. This whole "MIXER" concept need a
		 * rethinking, I guess :) . Recording is the inverse
		 * of Playback, which is why we push mixer vchan down here.
		 */
		if (c->flags & CHN_F_HAS_VCHAN) {
			desc.type = FEEDER_MIXER;
			desc.in = c->format;
		} else
			return EOPNOTSUPP;
		desc.out = c->format;
		desc.flags = 0;
		fc = feeder_getclass(&desc);
		if (fc == NULL)
			return EOPNOTSUPP;

		err = chn_addfeeder(c, fc, &desc);
		if (err != 0)
			return err;
	}

	sndbuf_setfmt(c->bufhard, hwfmt);

	if ((flags & (1 << FEEDER_VOLUME))) {
		u_int32_t parent = SOUND_MIXER_NONE;
		int vol, left, right;

		vol = 100 | (100 << 8);

		CHN_UNLOCK(c);
		/*
		 * XXX This is ugly! The way mixer subs being so secretive
		 * about its own internals force us to use this silly
		 * monkey trick.
		 */
		if (mixer_ioctl(c->parentsnddev->mixer_dev,
				MIXER_READ(SOUND_MIXER_PCM), (caddr_t)&vol, -1, NULL) != 0)
			device_printf(c->dev, "Soft PCM Volume: Failed to read default value\n");
		left = vol & 0x7f;
		right = (vol >> 8) & 0x7f;
		if (c->parentsnddev != NULL &&
		    c->parentsnddev->mixer_dev != NULL &&
		    c->parentsnddev->mixer_dev->si_drv1 != NULL)
			parent = mix_getparent(
			    c->parentsnddev->mixer_dev->si_drv1,
			    SOUND_MIXER_PCM);
		if (parent != SOUND_MIXER_NONE) {
			vol = 100 | (100 << 8);
			if (mixer_ioctl(c->parentsnddev->mixer_dev,
					MIXER_READ(parent),
					(caddr_t)&vol, -1, NULL) != 0)
				device_printf(c->dev, "Soft Volume: Failed to read parent default value\n");
			left = (left * (vol & 0x7f)) / 100;
			right = (right * ((vol >> 8) & 0x7f)) / 100;
		}
		CHN_LOCK(c);
		chn_setvolume(c, left, right);
	}

	return 0;
}

int
chn_notify(struct pcm_channel *c, u_int32_t flags)
{
	int run;

	CHN_LOCK(c);

	if (CHN_EMPTY(c, children)) {
		CHN_UNLOCK(c);
		return ENODEV;
	}

	run = (CHN_STARTED(c)) ? 1 : 0;
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
		/*
		 * Set to default latency profile
		 */
		chn_setlatency(c, chn_latency);
	}
	if (flags & CHN_N_TRIGGER) {
		int nrun;

		nrun = CHN_EMPTY(c, children.busy) ? 0 : 1;
		if (nrun && !run)
			chn_start(c, 1);
		if (!nrun && run)
			chn_abort(c);
	}
	CHN_UNLOCK(c);
	return 0;
}

/**
 * @brief Fetch array of supported discrete sample rates
 *
 * Wrapper for CHANNEL_GETRATES.  Please see channel_if.m:getrates() for
 * detailed information.
 *
 * @note If the operation isn't supported, this function will just return 0
 *       (no rates in the array), and *rates will be set to NULL.  Callers
 *       should examine rates @b only if this function returns non-zero.
 *
 * @param c	pcm channel to examine
 * @param rates	pointer to array of integers; rate table will be recorded here
 *
 * @return number of rates in the array pointed to be @c rates
 */
int
chn_getrates(struct pcm_channel *c, int **rates)
{
	KASSERT(rates != NULL, ("rates is null"));
	CHN_LOCKASSERT(c);
	return CHANNEL_GETRATES(c->methods, c->devinfo, rates);
}

/**
 * @brief Remove channel from a sync group, if there is one.
 *
 * This function is initially intended for the following conditions:
 *   - Starting a syncgroup (@c SNDCTL_DSP_SYNCSTART ioctl)
 *   - Closing a device.  (A channel can't be destroyed if it's still in use.)
 *
 * @note Before calling this function, the syncgroup list mutex must be
 * held.  (Consider pcm_channel::sm protected by the SG list mutex
 * whether @c c is locked or not.)
 *
 * @param c	channel device to be started or closed
 * @returns	If this channel was the only member of a group, the group ID
 * 		is returned to the caller so that the caller can release it
 * 		via free_unr() after giving up the syncgroup lock.  Else it
 * 		returns 0.
 */
int
chn_syncdestroy(struct pcm_channel *c)
{
	struct pcmchan_syncmember *sm;
	struct pcmchan_syncgroup *sg;
	int sg_id;

	sg_id = 0;

	PCM_SG_LOCKASSERT(MA_OWNED);

	if (c->sm != NULL) {
		sm = c->sm;
		sg = sm->parent;
		c->sm = NULL;

		KASSERT(sg != NULL, ("syncmember has null parent"));

		SLIST_REMOVE(&sg->members, sm, pcmchan_syncmember, link);
		free(sm, M_DEVBUF);

		if (SLIST_EMPTY(&sg->members)) {
			SLIST_REMOVE(&snd_pcm_syncgroups, sg, pcmchan_syncgroup, link);
			sg_id = sg->id;
			free(sg, M_DEVBUF);
		}
	}

	return sg_id;
}

void
chn_lock(struct pcm_channel *c)
{
	CHN_LOCK(c);
}

void
chn_unlock(struct pcm_channel *c)
{
	CHN_UNLOCK(c);
}

#ifdef OSSV4_EXPERIMENT
int
chn_getpeaks(struct pcm_channel *c, int *lpeak, int *rpeak)
{
	CHN_LOCKASSERT(c);
	return CHANNEL_GETPEAKS(c->methods, c->devinfo, lpeak, rpeak);
}
#endif
