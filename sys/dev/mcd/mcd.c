/*-
 * Copyright 1993 by Holger Veit (data part)
 * Copyright 1993 by Brian Moore (audio part)
 * Changes Copyright 1993 by Gary Clark II
 * Changes Copyright (C) 1994-1995 by Andrey A. Chernov, Moscow, Russia
 *
 * Rewrote probe routine to work on newer Mitsumi drives.
 * Additional changes (C) 1994 by Jordan K. Hubbard
 *
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This software was developed by Holger Veit and Brian Moore
 *	for use with "386BSD" and similar operating systems.
 *    "Similar operating systems" includes mainly non-profit oriented
 *    systems for research and education, including but not restricted to
 *    "NetBSD", "FreeBSD", "Mach" (by CMU).
 * 4. Neither the name of the developer(s) nor the name "386BSD"
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
static const char COPYRIGHT[] = "mcd-driver (C)1993 by H.Veit & B.Moore";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/bio.h>
#include <sys/cdio.h>
#include <sys/disk.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>
  
#include <dev/mcd/mcdreg.h>
#include <dev/mcd/mcdvar.h>

#define	MCD_TRACE(format, args...)					\
{									\
	if (sc->debug) {						\
		device_printf(sc->dev, "status=0x%02x: ",		\
			sc->data.status);				\
		printf(format, ## args);				\
	}								\
}

#define RAW_PART        2

/* flags */
#define MCDVALID        0x0001  /* parameters loaded */
#define MCDINIT         0x0002  /* device is init'd */
#define MCDNEWMODEL     0x0004  /* device is new model */
#define MCDLABEL        0x0008  /* label is read */
#define MCDPROBING      0x0010  /* probing */
#define MCDREADRAW      0x0020  /* read raw mode (2352 bytes) */
#define MCDVOLINFO      0x0040  /* already read volinfo */
#define MCDTOC          0x0080  /* already read toc */
#define MCDMBXBSY       0x0100  /* local mbx is busy */

/* status */
#define	MCDAUDIOBSY	MCD_ST_AUDIOBSY		/* playing audio */
#define MCDDSKCHNG	MCD_ST_DSKCHNG		/* sensed change of disk */
#define MCDDSKIN	MCD_ST_DSKIN		/* sensed disk in drive */
#define MCDDOOROPEN	MCD_ST_DOOROPEN		/* sensed door open */

/* These are apparently the different states a mitsumi can get up to */
#define MCDCDABSENT	0x0030
#define MCDCDPRESENT	0x0020
#define MCDSCLOSED	0x0080
#define MCDSOPEN	0x00a0

#define MCD_MD_UNKNOWN  (-1)

#define	MCD_TYPE_UNKNOWN	0
#define	MCD_TYPE_LU002S		1
#define	MCD_TYPE_LU005S		2
#define MCD_TYPE_LU006S         3
#define MCD_TYPE_FX001          4
#define MCD_TYPE_FX001D         5

/* reader state machine */
#define MCD_S_BEGIN	0
#define MCD_S_BEGIN1	1
#define MCD_S_WAITSTAT	2
#define MCD_S_WAITMODE	3
#define MCD_S_WAITREAD	4

/* prototypes */
static void	mcd_start(struct mcd_softc *);
#ifdef NOTYET
static void	mcd_configure(struct mcd_softc *sc);
#endif
static int	mcd_get(struct mcd_softc *, char *buf, int nmax);
static int	mcd_setflags(struct mcd_softc *);
static int	mcd_getstat(struct mcd_softc *,int sflg);
static int	mcd_send(struct mcd_softc *, int cmd,int nretrys);
static void	hsg2msf(int hsg, bcd_t *msf);
static int	msf2hsg(bcd_t *msf, int relative);
static int	mcd_volinfo(struct mcd_softc *);
static int	mcd_waitrdy(struct mcd_softc *,int dly);
static timeout_t mcd_timeout;
static void	mcd_doread(struct mcd_softc *, int state, struct mcd_mbx *mbxin);
static void	mcd_soft_reset(struct mcd_softc *);
static int	mcd_hard_reset(struct mcd_softc *);
static int 	mcd_setmode(struct mcd_softc *, int mode);
static int	mcd_getqchan(struct mcd_softc *, struct mcd_qchninfo *q);
static int	mcd_subchan(struct mcd_softc *, struct ioc_read_subchannel *sc,
		    int nocopyout);
static int	mcd_toc_header(struct mcd_softc *, struct ioc_toc_header *th);
static int	mcd_read_toc(struct mcd_softc *);
static int	mcd_toc_entrys(struct mcd_softc *, struct ioc_read_toc_entry *te);
#if 0
static int	mcd_toc_entry(struct mcd_softc *, struct ioc_read_toc_single_entry *te);
#endif
static int	mcd_stop(struct mcd_softc *);
static int	mcd_eject(struct mcd_softc *);
static int	mcd_inject(struct mcd_softc *);
static int	mcd_playtracks(struct mcd_softc *, struct ioc_play_track *pt);
static int	mcd_play(struct mcd_softc *, struct mcd_read2 *pb);
static int	mcd_playmsf(struct mcd_softc *, struct ioc_play_msf *pt);
static int	mcd_playblocks(struct mcd_softc *, struct ioc_play_blocks *);
static int	mcd_pause(struct mcd_softc *);
static int	mcd_resume(struct mcd_softc *);
static int	mcd_lock_door(struct mcd_softc *, int lock);
static int	mcd_close_tray(struct mcd_softc *);
static int	mcd_size(struct cdev *dev);

static d_open_t		mcdopen;
static d_close_t	mcdclose;
static d_ioctl_t	mcdioctl;
static d_strategy_t	mcdstrategy;

static struct cdevsw mcd_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	mcdopen,
	.d_close =	mcdclose,
	.d_read =	physread,
	.d_ioctl =	mcdioctl,
	.d_strategy =	mcdstrategy,
	.d_name =	"mcd",
	.d_flags =	D_DISK | D_NEEDGIANT,
};

#define MCD_RETRYS	5
#define MCD_RDRETRYS	8

#define CLOSE_TRAY_SECS 8
#define DISK_SENSE_SECS 3
#define WAIT_FRAC 4

/* several delays */
#define RDELAY_WAITSTAT 300
#define RDELAY_WAITMODE 300
#define RDELAY_WAITREAD	800

#define MIN_DELAY       15
#define DELAY_GETREPLY  5000000

int
mcd_attach(struct mcd_softc *sc)
{
	int unit;

	unit = device_get_unit(sc->dev);

	sc->data.flags |= MCDINIT;
	mcd_soft_reset(sc);
	bioq_init(&sc->data.head);

#ifdef NOTYET
	/* wire controller for interrupts and dma */
	mcd_configure(sc);
#endif
	/* name filled in probe */
	sc->mcd_dev_t = make_dev(&mcd_cdevsw, 8 * unit,
				 UID_ROOT, GID_OPERATOR, 0640, "mcd%d", unit);

	sc->mcd_dev_t->si_drv1 = (void *)sc;

	return (0);
}

static int
mcdopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct mcd_softc *sc;
	int r,retry;

	sc = (struct mcd_softc *)dev->si_drv1;

	/* not initialized*/
	if (!(sc->data.flags & MCDINIT))
		return (ENXIO);

	/* invalidated in the meantime? mark all open part's invalid */
	if (!(sc->data.flags & MCDVALID) && sc->data.openflags)
		return (ENXIO);

	if (mcd_getstat(sc, 1) == -1)
		return (EIO);

	if (    (sc->data.status & (MCDDSKCHNG|MCDDOOROPEN))
	    || !(sc->data.status & MCDDSKIN))
		for (retry = 0; retry < DISK_SENSE_SECS * WAIT_FRAC; retry++) {
			(void) tsleep((caddr_t)sc, PSOCK | PCATCH, "mcdsn1", hz/WAIT_FRAC);
			if ((r = mcd_getstat(sc, 1)) == -1)
				return (EIO);
			if (r != -2)
				break;
		}

	if (sc->data.status & MCDDOOROPEN) {
		device_printf(sc->dev, "door is open\n");
		return (ENXIO);
	}
	if (!(sc->data.status & MCDDSKIN)) {
		device_printf(sc->dev, "no CD inside\n");
		return (ENXIO);
	}
	if (sc->data.status & MCDDSKCHNG) {
		device_printf(sc->dev, "CD not sensed\n");
		return (ENXIO);
	}

	if (mcd_size(dev) < 0) {
		device_printf(sc->dev, "failed to get disk size\n");
		return (ENXIO);
	}

	sc->data.openflags = 1;
	sc->data.partflags |= MCDREADRAW;
	sc->data.flags |= MCDVALID;

	(void) mcd_lock_door(sc, MCD_LK_LOCK);
	if (!(sc->data.flags & MCDVALID))
		return (ENXIO);

	return mcd_read_toc(sc);
}

static int
mcdclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct mcd_softc *sc;

	sc = (struct mcd_softc *)dev->si_drv1;

	if (!(sc->data.flags & MCDINIT) || !sc->data.openflags)
		return (ENXIO);

	(void) mcd_lock_door(sc, MCD_LK_UNLOCK);
	sc->data.openflags = 0;
	sc->data.partflags &= ~MCDREADRAW;

	return (0);
}

static void
mcdstrategy(struct bio *bp)
{
	struct mcd_softc *sc;
	int s;

	sc = (struct mcd_softc *)bp->bio_dev->si_drv1;

	/* if device invalidated (e.g. media change, door open), error */
	if (!(sc->data.flags & MCDVALID)) {
		device_printf(sc->dev, "media changed\n");
		bp->bio_error = EIO;
		goto bad;
	}

	/* read only */
	if (!(bp->bio_cmd == BIO_READ)) {
		bp->bio_error = EROFS;
		goto bad;
	}

	/* no data to read */
	if (bp->bio_bcount == 0)
		goto done;

	if (!(sc->data.flags & MCDTOC)) {
		bp->bio_error = EIO;
		goto bad;
	}

	bp->bio_resid = 0;

	/* queue it */
	s = splbio();
	bioq_disksort(&sc->data.head, bp);
	splx(s);

	/* now check whether we can perform processing */
	mcd_start(sc);
	return;

bad:
	bp->bio_flags |= BIO_ERROR;
done:
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);
	return;
}

static void
mcd_start(struct mcd_softc *sc)
{
	struct bio *bp;
	int s = splbio();

	if (sc->data.flags & MCDMBXBSY) {
		splx(s);
		return;
	}

	bp = bioq_takefirst(&sc->data.head);
	if (bp != 0) {
		/* block found to process, dequeue */
		/*MCD_TRACE("mcd_start: found block bp=0x%x\n",bp,0,0,0);*/
		sc->data.flags |= MCDMBXBSY;
		splx(s);
	} else {
		/* nothing to do */
		splx(s);
		return;
	}

	sc->data.mbx.retry = MCD_RETRYS;
	sc->data.mbx.bp = bp;

	mcd_doread(sc, MCD_S_BEGIN,&(sc->data.mbx));
	return;
}

static int
mcdioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
	struct mcd_softc *sc;
	int retry,r;

	sc = (struct mcd_softc *)dev->si_drv1;

	if (mcd_getstat(sc, 1) == -1) /* detect disk change too */
		return (EIO);
MCD_TRACE("ioctl called 0x%lx\n", cmd);

	switch (cmd) {
	case CDIOCSETPATCH:
	case CDIOCGETVOL:
	case CDIOCSETVOL:
	case CDIOCSETMONO:
	case CDIOCSETSTERIO:
	case CDIOCSETMUTE:
	case CDIOCSETLEFT:
	case CDIOCSETRIGHT:
		return (EINVAL);
	case CDIOCEJECT:
		return mcd_eject(sc);
	case CDIOCSETDEBUG:
		sc->data.debug = 1;
		return (0);
	case CDIOCCLRDEBUG:
		sc->data.debug = 0;
		return (0);
	case CDIOCRESET:
		return mcd_hard_reset(sc);
	case CDIOCALLOW:
		return mcd_lock_door(sc, MCD_LK_UNLOCK);
	case CDIOCPREVENT:
		return mcd_lock_door(sc, MCD_LK_LOCK);
	case CDIOCCLOSE:
		return mcd_inject(sc);
	}

	if (!(sc->data.flags & MCDVALID)) {
		if (    (sc->data.status & (MCDDSKCHNG|MCDDOOROPEN))
		    || !(sc->data.status & MCDDSKIN))
			for (retry = 0; retry < DISK_SENSE_SECS * WAIT_FRAC; retry++) {
				(void) tsleep((caddr_t)sc, PSOCK | PCATCH, "mcdsn2", hz/WAIT_FRAC);
				if ((r = mcd_getstat(sc, 1)) == -1)
					return (EIO);
				if (r != -2)
					break;
			}
		if (   (sc->data.status & (MCDDOOROPEN|MCDDSKCHNG))
		    || !(sc->data.status & MCDDSKIN)
		    || mcd_size(dev) < 0
		   )
			return (ENXIO);
		sc->data.flags |= MCDVALID;
		sc->data.partflags |= MCDREADRAW;
		(void) mcd_lock_door(sc, MCD_LK_LOCK);
		if (!(sc->data.flags & MCDVALID))
			return (ENXIO);
	}

	switch (cmd) {
	case DIOCGMEDIASIZE:
		*(off_t *)addr = (off_t)sc->data.disksize * sc->data.blksize;
		return (0);
	case DIOCGSECTORSIZE:
		*(u_int *)addr = sc->data.blksize;
		return (0);

	case CDIOCPLAYTRACKS:
		return mcd_playtracks(sc, (struct ioc_play_track *) addr);
	case CDIOCPLAYBLOCKS:
		return mcd_playblocks(sc, (struct ioc_play_blocks *) addr);
	case CDIOCPLAYMSF:
		return mcd_playmsf(sc, (struct ioc_play_msf *) addr);
	case CDIOCREADSUBCHANNEL_SYSSPACE:
		return mcd_subchan(sc, (struct ioc_read_subchannel *) addr, 1);
	case CDIOCREADSUBCHANNEL:
		return mcd_subchan(sc, (struct ioc_read_subchannel *) addr, 0);
	case CDIOREADTOCHEADER:
		return mcd_toc_header(sc, (struct ioc_toc_header *) addr);
	case CDIOREADTOCENTRYS:
		return mcd_toc_entrys(sc, (struct ioc_read_toc_entry *) addr);
	case CDIOCRESUME:
		return mcd_resume(sc);
	case CDIOCPAUSE:
		return mcd_pause(sc);
	case CDIOCSTART:
		if (mcd_setmode(sc, MCD_MD_COOKED) != 0)
			return (EIO);
		return (0);
	case CDIOCSTOP:
		return mcd_stop(sc);
	default:
		return (ENOTTY);
	}
	/*NOTREACHED*/
}

static int
mcd_size(struct cdev *dev)
{
	struct mcd_softc *sc;
	int size;

	sc = (struct mcd_softc *)dev->si_drv1;

	if (mcd_volinfo(sc) == 0) {
		sc->data.blksize = MCDBLK;
		size = msf2hsg(sc->data.volinfo.vol_msf, 0);
		sc->data.disksize = size * (MCDBLK/DEV_BSIZE);
		return (0);
	}
	return (-1);
}

/***************************************************************
 * lower level of driver starts here
 **************************************************************/

#ifdef NOTDEF
static char
irqs[] = {
	0x00,0x00,0x10,0x20,0x00,0x30,0x00,0x00,
	0x00,0x10,0x40,0x50,0x00,0x00,0x00,0x00
};

static char
drqs[] = {
	0x00,0x01,0x00,0x03,0x00,0x05,0x06,0x07,
};
#endif

#ifdef NOT_YET
static void
mcd_configure(struct mcd_softc *sc)
{
	MCD_WRITE(sc, MCD_REG_CONFIG, sc->data.config);
}
#endif

/* Wait for non-busy - return 0 on timeout */
static int
twiddle_thumbs(struct mcd_softc *sc, int count, char *whine)
{
	int i;

	for (i = 0; i < count; i++) {
		if (!(MCD_READ(sc, MCD_FLAGS) & MFL_STATUS_NOT_AVAIL))
			return (1);
		}
	if (bootverbose)
		device_printf(sc->dev, "timeout %s\n", whine);
	return (0);
}

/* check to see if a Mitsumi CD-ROM is attached to the ISA bus */

int
mcd_probe(struct mcd_softc *sc)
{
	int i, j;
	unsigned char stbytes[3];

	sc->data.flags = MCDPROBING;

#ifdef NOTDEF
	/* get irq/drq configuration word */
	sc->data.config = irqs[dev->id_irq]; /* | drqs[dev->id_drq];*/
#else
	sc->data.config = 0;
#endif

	/* send a reset */
	MCD_WRITE(sc, MCD_FLAGS, M_RESET);

	/*
	 * delay awhile by getting any pending garbage (old data) and
	 * throwing it away.
	 */
	for (i = 1000000; i != 0; i--)
		(void)MCD_READ(sc, MCD_FLAGS);

	/* Get status */
	MCD_WRITE(sc, MCD_DATA, MCD_CMDGETSTAT);
	if (!twiddle_thumbs(sc, 1000000, "getting status"))
		return (ENXIO);	/* Timeout */
	/* Get version information */
	MCD_WRITE(sc, MCD_DATA, MCD_CMDCONTINFO);
	for (j = 0; j < 3; j++) {
		if (!twiddle_thumbs(sc, 3000, "getting version info"))
			return (ENXIO);
		stbytes[j] = (MCD_READ(sc, MCD_DATA) & 0xFF);
	}
	if (stbytes[1] == stbytes[2])
		return (ENXIO);
	if (stbytes[2] >= 4 || stbytes[1] != 'M') {
		MCD_WRITE(sc, MCD_CTRL, M_PICKLE);
		sc->data.flags |= MCDNEWMODEL;
	}
	sc->data.read_command = MCD_CMDSINGLESPEEDREAD;
	switch (stbytes[1]) {
	case 'M':
		if (stbytes[2] <= 2) {
			sc->data.type = MCD_TYPE_LU002S;
			sc->data.name = "Mitsumi LU002S";
		} else if (stbytes[2] <= 5) {
			sc->data.type = MCD_TYPE_LU005S;
			sc->data.name = "Mitsumi LU005S";
		} else {
			sc->data.type = MCD_TYPE_LU006S;
			sc->data.name = "Mitsumi LU006S";
		}
		break;
	case 'F':
		sc->data.type = MCD_TYPE_FX001;
		sc->data.name = "Mitsumi FX001";
		break;
	case 'D':
		sc->data.type = MCD_TYPE_FX001D;
		sc->data.name = "Mitsumi FX001D";
		sc->data.read_command = MCD_CMDDOUBLESPEEDREAD;
		break;
	default:
		sc->data.type = MCD_TYPE_UNKNOWN;
		sc->data.name = "Mitsumi ???";
		break;
	}

	if (bootverbose)
		device_printf(sc->dev, "type %s, version info: %c %x\n",
			sc->data.name, stbytes[1], stbytes[2]);

	return (0);
}


static int
mcd_waitrdy(struct mcd_softc *sc, int dly)
{
	int i;

	/* wait until flag port senses status ready */
	for (i=0; i<dly; i+=MIN_DELAY) {
		if (!(MCD_READ(sc, MCD_FLAGS) & MFL_STATUS_NOT_AVAIL))
			return (0);
		DELAY(MIN_DELAY);
	}
	return (-1);
}

static int
mcd_getreply(struct mcd_softc *sc, int dly)
{

	/* wait data to become ready */
	if (mcd_waitrdy(sc, dly)<0) {
		device_printf(sc->dev, "timeout getreply\n");
		return (-1);
	}

	/* get the data */
	return (MCD_READ(sc, MCD_REG_STATUS) & 0xFF);
}

static int
mcd_getstat(struct mcd_softc *sc, int sflg)
{
	int	i;

	/* get the status */
	if (sflg)
		MCD_WRITE(sc, MCD_REG_COMMAND, MCD_CMDGETSTAT);
	i = mcd_getreply(sc, DELAY_GETREPLY);
	if (i<0 || (i & MCD_ST_CMDCHECK)) {
		sc->data.curr_mode = MCD_MD_UNKNOWN;
		return (-1);
	}

	sc->data.status = i;

	if (mcd_setflags(sc) < 0)
		return (-2);
	return (sc->data.status);
}

static int
mcd_setflags(struct mcd_softc *sc)
{

	/* check flags */
	if (    (sc->data.status & (MCDDSKCHNG|MCDDOOROPEN))
	    || !(sc->data.status & MCDDSKIN)) {
		MCD_TRACE("setflags: sensed DSKCHNG or DOOROPEN or !DSKIN\n");
		mcd_soft_reset(sc);
		return (-1);
	}

	if (sc->data.status & MCDAUDIOBSY)
		sc->data.audio_status = CD_AS_PLAY_IN_PROGRESS;
	else if (sc->data.audio_status == CD_AS_PLAY_IN_PROGRESS)
		sc->data.audio_status = CD_AS_PLAY_COMPLETED;
	return (0);
}

static int
mcd_get(struct mcd_softc *sc, char *buf, int nmax)
{
	int i,k;

	for (i=0; i<nmax; i++) {
		/* wait for data */
		if ((k = mcd_getreply(sc, DELAY_GETREPLY)) < 0) {
			device_printf(sc->dev, "timeout mcd_get\n");
			return (-1);
		}
		buf[i] = k;
	}
	return (i);
}

static int
mcd_send(struct mcd_softc *sc, int cmd,int nretrys)
{
	int i,k=0;

/*MCD_TRACE("mcd_send: command = 0x%02x\n",cmd,0,0,0);*/
	for (i=0; i<nretrys; i++) {
		MCD_WRITE(sc, MCD_REG_COMMAND, cmd);
		if ((k=mcd_getstat(sc, 0)) != -1)
			break;
	}
	if (k == -2) {
		device_printf(sc->dev, "media changed\n");
		return (-1);
	}
	if (i == nretrys) {
		device_printf(sc->dev, "mcd_send retry cnt exceeded\n");
		return (-1);
	}
/*MCD_TRACE("mcd_send: done\n",0,0,0,0);*/
	return (0);
}

static void
hsg2msf(int hsg, bcd_t *msf)
{
	hsg += 150;
	F_msf(msf) = bin2bcd(hsg % 75);
	hsg /= 75;
	S_msf(msf) = bin2bcd(hsg % 60);
	hsg /= 60;
	M_msf(msf) = bin2bcd(hsg);
}

static int
msf2hsg(bcd_t *msf, int relative)
{
	return (bcd2bin(M_msf(msf)) * 60 + bcd2bin(S_msf(msf))) * 75 +
		bcd2bin(F_msf(msf)) - (!relative) * 150;
}

static int
mcd_volinfo(struct mcd_softc *sc)
{

	/* Just return if we already have it */
	if (sc->data.flags & MCDVOLINFO) return (0);

/*MCD_TRACE("mcd_volinfo: enter\n",0,0,0,0);*/

	/* send volume info command */
	if (mcd_send(sc, MCD_CMDGETVOLINFO,MCD_RETRYS) < 0)
		return (EIO);

	/* get data */
	if (mcd_get(sc, (char*) &sc->data.volinfo,sizeof(struct mcd_volinfo)) < 0) {
		device_printf(sc->dev, "mcd_volinfo: error read data\n");
		return (EIO);
	}

	if (sc->data.volinfo.trk_low > 0 &&
	    sc->data.volinfo.trk_high >= sc->data.volinfo.trk_low
	   ) {
		sc->data.flags |= MCDVOLINFO;	/* volinfo is OK */
		return (0);
	}

	return (EINVAL);
}

/* state machine to process read requests
 * initialize with MCD_S_BEGIN: calculate sizes, and read status
 * MCD_S_WAITSTAT: wait for status reply, set mode
 * MCD_S_WAITMODE: waits for status reply from set mode, set read command
 * MCD_S_WAITREAD: wait for read ready, read data
 */
static void
mcd_timeout(void *arg)
{
	struct mcd_softc *sc;

	sc = (struct mcd_softc *)arg;

	mcd_doread(sc, sc->ch_state, sc->ch_mbxsave);
}

static void
mcd_doread(struct mcd_softc *sc, int state, struct mcd_mbx *mbxin)
{
	struct mcd_mbx *mbx;
	struct bio *bp;
	int rm, i, k;
	struct mcd_read2 rbuf;
	int blknum;
	caddr_t	addr;

	mbx = (state!=MCD_S_BEGIN) ? sc->ch_mbxsave : mbxin;
	bp = mbx->bp;

loop:
	switch (state) {
	case MCD_S_BEGIN:
		mbx = sc->ch_mbxsave = mbxin;

	case MCD_S_BEGIN1:
retry_status:
		/* get status */
		MCD_WRITE(sc, MCD_REG_COMMAND, MCD_CMDGETSTAT);
		mbx->count = RDELAY_WAITSTAT;
		sc->ch_state = MCD_S_WAITSTAT;
		sc->ch = timeout(mcd_timeout, (caddr_t)sc, hz/100); /* XXX */
		return;
	case MCD_S_WAITSTAT:
		sc->ch_state = MCD_S_WAITSTAT;
		untimeout(mcd_timeout,(caddr_t)sc, sc->ch);
		if (mbx->count-- >= 0) {
			if (MCD_READ(sc, MCD_FLAGS) & MFL_STATUS_NOT_AVAIL) {
				sc->ch_state = MCD_S_WAITSTAT;
				timeout(mcd_timeout, (caddr_t)sc, hz/100); /* XXX */
				return;
			}
			sc->data.status = MCD_READ(sc, MCD_REG_STATUS) & 0xFF;
			if (sc->data.status & MCD_ST_CMDCHECK)
				goto retry_status;
			if (mcd_setflags(sc) < 0)
				goto changed;
			MCD_TRACE("got WAITSTAT delay=%d\n",
				RDELAY_WAITSTAT-mbx->count);
			/* reject, if audio active */
			if (sc->data.status & MCDAUDIOBSY) {
				device_printf(sc->dev, "audio is active\n");
				goto readerr;
			}

retry_mode:
			/* to check for raw/cooked mode */
			if (sc->data.flags & MCDREADRAW) {
				rm = MCD_MD_RAW;
				mbx->sz = MCDRBLK;
			} else {
				rm = MCD_MD_COOKED;
				mbx->sz = sc->data.blksize;
			}

			if (rm == sc->data.curr_mode)
				goto modedone;

			mbx->count = RDELAY_WAITMODE;

			sc->data.curr_mode = MCD_MD_UNKNOWN;
			mbx->mode = rm;
			MCD_WRITE(sc, MCD_REG_COMMAND, MCD_CMDSETMODE);
			MCD_WRITE(sc, MCD_REG_COMMAND, rm);

			sc->ch_state = MCD_S_WAITMODE;
			sc->ch = timeout(mcd_timeout, (caddr_t)sc, hz/100); /* XXX */
			return;
		} else {
			device_printf(sc->dev, "timeout getstatus\n");
			goto readerr;
		}

	case MCD_S_WAITMODE:
		sc->ch_state = MCD_S_WAITMODE;
		untimeout(mcd_timeout, (caddr_t)sc, sc->ch);
		if (mbx->count-- < 0) {
			device_printf(sc->dev, "timeout set mode\n");
			goto readerr;
		}
		if (MCD_READ(sc, MCD_FLAGS) & MFL_STATUS_NOT_AVAIL) {
			sc->ch_state = MCD_S_WAITMODE;
			sc->ch = timeout(mcd_timeout, (caddr_t)sc, hz/100);
			return;
		}
		sc->data.status = MCD_READ(sc, MCD_REG_STATUS) & 0xFF;
		if (sc->data.status & MCD_ST_CMDCHECK) {
			sc->data.curr_mode = MCD_MD_UNKNOWN;
			goto retry_mode;
		}
		if (mcd_setflags(sc) < 0)
			goto changed;
		sc->data.curr_mode = mbx->mode;
		MCD_TRACE("got WAITMODE delay=%d\n",
			RDELAY_WAITMODE-mbx->count);
modedone:
		/* for first block */
		mbx->nblk = (bp->bio_bcount + (mbx->sz-1)) / mbx->sz;
		mbx->skip = 0;

nextblock:
		blknum 	= bp->bio_offset / mbx->sz + mbx->skip/mbx->sz;

		MCD_TRACE("mcd_doread: read blknum=%d for bp=%p\n",
			blknum, bp);

		/* build parameter block */
		hsg2msf(blknum,rbuf.start_msf);
retry_read:
		/* send the read command */
		critical_enter();
		MCD_WRITE(sc, MCD_REG_COMMAND, sc->data.read_command);
		MCD_WRITE(sc, MCD_REG_COMMAND, rbuf.start_msf[0]);
		MCD_WRITE(sc, MCD_REG_COMMAND, rbuf.start_msf[1]);
		MCD_WRITE(sc, MCD_REG_COMMAND, rbuf.start_msf[2]);
		MCD_WRITE(sc, MCD_REG_COMMAND, 0);
		MCD_WRITE(sc, MCD_REG_COMMAND, 0);
		MCD_WRITE(sc, MCD_REG_COMMAND, 1);
		critical_exit();

		/* Spin briefly (<= 2ms) to avoid missing next block */
		for (i = 0; i < 20; i++) {
			k = MCD_READ(sc, MCD_FLAGS);
			if (!(k & MFL_DATA_NOT_AVAIL))
				goto got_it;
			DELAY(100);
		}

		mbx->count = RDELAY_WAITREAD;
		sc->ch_state = MCD_S_WAITREAD;
		sc->ch = timeout(mcd_timeout, (caddr_t)sc, hz/100); /* XXX */
		return;
	case MCD_S_WAITREAD:
		sc->ch_state = MCD_S_WAITREAD;
		untimeout(mcd_timeout, (caddr_t)sc, sc->ch);
		if (mbx->count-- > 0) {
			k = MCD_READ(sc, MCD_FLAGS);
			if (!(k & MFL_DATA_NOT_AVAIL)) { /* XXX */
				MCD_TRACE("got data delay=%d\n",
					RDELAY_WAITREAD-mbx->count);
			got_it:
				/* data is ready */
				addr	= bp->bio_data + mbx->skip;

				MCD_WRITE(sc, MCD_REG_CTL2,0x04);	/* XXX */
				for (i=0; i<mbx->sz; i++)
					*addr++ = MCD_READ(sc, MCD_REG_RDATA);
				MCD_WRITE(sc, MCD_REG_CTL2,0x0c);	/* XXX */

				k = MCD_READ(sc, MCD_FLAGS);
				/* If we still have some junk, read it too */
				if (!(k & MFL_DATA_NOT_AVAIL)) {
					MCD_WRITE(sc, MCD_REG_CTL2, 0x04);       /* XXX */
					(void)MCD_READ(sc, MCD_REG_RDATA);
					(void)MCD_READ(sc, MCD_REG_RDATA);
					MCD_WRITE(sc, MCD_REG_CTL2, 0x0c);       /* XXX */
				}

				if (--mbx->nblk > 0) {
					mbx->skip += mbx->sz;
					goto nextblock;
				}

				/* return buffer */
				bp->bio_resid = 0;
				biodone(bp);

				sc->data.flags &= ~(MCDMBXBSY|MCDREADRAW);
				mcd_start(sc);
				return;
			}
			if (!(k & MFL_STATUS_NOT_AVAIL)) {
				sc->data.status = MCD_READ(sc, MCD_REG_STATUS) & 0xFF;
				if (sc->data.status & MCD_ST_CMDCHECK)
					goto retry_read;
				if (mcd_setflags(sc) < 0)
					goto changed;
			}
			sc->ch_state = MCD_S_WAITREAD;
			sc->ch = timeout(mcd_timeout, (caddr_t)sc, hz/100); /* XXX */
			return;
		} else {
			device_printf(sc->dev, "timeout read data\n");
			goto readerr;
		}
	}

readerr:
	if (mbx->retry-- > 0) {
		device_printf(sc->dev, "retrying\n");
		state = MCD_S_BEGIN1;
		goto loop;
	}
harderr:
	/* invalidate the buffer */
	bp->bio_flags |= BIO_ERROR;
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);

	sc->data.flags &= ~(MCDMBXBSY|MCDREADRAW);
	mcd_start(sc);
	return;

changed:
	device_printf(sc->dev, "media changed\n");
	goto harderr;

#ifdef NOTDEF
	device_printf(sc->dev, "unit timeout, resetting\n");
	MCD_WRITE(sc, MCD_REG_RESET, MCD_CMDRESET);
	DELAY(300000);
	(void)mcd_getstat(sc, 1);
	(void)mcd_getstat(sc, 1);
	/*sc->data.status &= ~MCDDSKCHNG; */
	sc->data.debug = 1; /* preventive set debug mode */

#endif

}

static int
mcd_lock_door(struct mcd_softc *sc, int lock)
{

	MCD_WRITE(sc, MCD_REG_COMMAND, MCD_CMDLOCKDRV);
	MCD_WRITE(sc, MCD_REG_COMMAND, lock);
	if (mcd_getstat(sc, 0) == -1)
		return (EIO);
	return (0);
}

static int
mcd_close_tray(struct mcd_softc *sc)
{
	int retry, r;

	if (mcd_getstat(sc, 1) == -1)
		return (EIO);
	if (sc->data.status & MCDDOOROPEN) {
		MCD_WRITE(sc, MCD_REG_COMMAND, MCD_CMDCLOSETRAY);
		for (retry = 0; retry < CLOSE_TRAY_SECS * WAIT_FRAC; retry++) {
			if (MCD_READ(sc, MCD_FLAGS) & MFL_STATUS_NOT_AVAIL)
				(void) tsleep((caddr_t)sc, PSOCK | PCATCH, "mcdcls", hz/WAIT_FRAC);
			else {
				if ((r = mcd_getstat(sc, 0)) == -1)
					return (EIO);
				return (0);
			}
		}
		return (ENXIO);
	}
	return (0);
}

static int
mcd_eject(struct mcd_softc *sc)
{
	int r;

	if (mcd_getstat(sc, 1) == -1)    /* detect disk change too */
		return (EIO);
	if (sc->data.status & MCDDOOROPEN)
		return (0);
	if ((r = mcd_stop(sc)) == EIO)
		return (r);
	MCD_WRITE(sc, MCD_REG_COMMAND, MCD_CMDEJECTDISK);
	if (mcd_getstat(sc, 0) == -1)
		return (EIO);
	return (0);
}

static int
mcd_inject(struct mcd_softc *sc)
{

	if (mcd_getstat(sc, 1) == -1)    /* detect disk change too */
		return (EIO);
	if (sc->data.status & MCDDOOROPEN)
		return mcd_close_tray(sc);
	return (0);
}

static int
mcd_hard_reset(struct mcd_softc *sc)
{

	MCD_WRITE(sc, MCD_REG_RESET, MCD_CMDRESET);
	sc->data.curr_mode = MCD_MD_UNKNOWN;
	sc->data.audio_status = CD_AS_AUDIO_INVALID;
	return (0);
}

static void
mcd_soft_reset(struct mcd_softc *sc)
{

	sc->data.flags &= (MCDINIT|MCDPROBING|MCDNEWMODEL);
	sc->data.curr_mode = MCD_MD_UNKNOWN;
	sc->data.partflags = 0;
	sc->data.audio_status = CD_AS_AUDIO_INVALID;
}

static int
mcd_setmode(struct mcd_softc *sc, int mode)
{
	int retry, st;

	if (sc->data.curr_mode == mode)
		return (0);
	if (sc->data.debug)
		device_printf(sc->dev, "setting mode to %d\n", mode);
	for(retry=0; retry<MCD_RETRYS; retry++)
	{
		sc->data.curr_mode = MCD_MD_UNKNOWN;
		MCD_WRITE(sc, MCD_REG_COMMAND, MCD_CMDSETMODE);
		MCD_WRITE(sc, MCD_REG_COMMAND, mode);
		if ((st = mcd_getstat(sc, 0)) >= 0) {
			sc->data.curr_mode = mode;
			return (0);
		}
		if (st == -2) {
			device_printf(sc->dev, "media changed\n");
			break;
		}
	}

	return (-1);
}

static int
mcd_toc_header(struct mcd_softc *sc, struct ioc_toc_header *th)
{
	int r;

	if ((r = mcd_volinfo(sc)) != 0)
		return (r);

	th->starting_track = bcd2bin(sc->data.volinfo.trk_low);
	th->ending_track = bcd2bin(sc->data.volinfo.trk_high);
	th->len = 2 * sizeof(u_char) /* start & end tracks */ +
		  (th->ending_track + 1 - th->starting_track + 1) *
		  sizeof(struct cd_toc_entry);

	return (0);
}

static int
mcd_read_toc(struct mcd_softc *sc)
{
	struct ioc_toc_header th;
	struct mcd_qchninfo q;
	int rc, trk, idx, retry;

	/* Only read TOC if needed */
	if (sc->data.flags & MCDTOC)
		return (0);

	if (sc->data.debug)
		device_printf(sc->dev, "reading toc header\n");

	if ((rc = mcd_toc_header(sc, &th)) != 0)
		return (rc);

	if (mcd_send(sc, MCD_CMDSTOPAUDIO, MCD_RETRYS) < 0)
		return (EIO);

	if (mcd_setmode(sc, MCD_MD_TOC) != 0)
		return (EIO);

	if (sc->data.debug)
		device_printf(sc->dev, "get_toc reading qchannel info\n");

	for(trk=th.starting_track; trk<=th.ending_track; trk++)
		sc->data.toc[trk].idx_no = 0;
	trk = th.ending_track - th.starting_track + 1;
	for(retry=0; retry<600 && trk>0; retry++)
	{
		if (mcd_getqchan(sc, &q) < 0) break;
		idx = bcd2bin(q.idx_no);
		if (idx>=th.starting_track && idx<=th.ending_track && q.trk_no==0) {
			if (sc->data.toc[idx].idx_no == 0) {
				sc->data.toc[idx] = q;
				trk--;
			}
		}
	}

	if (mcd_setmode(sc, MCD_MD_COOKED) != 0)
		return (EIO);

	if (trk != 0)
		return (ENXIO);

	/* add a fake last+1 */
	idx = th.ending_track + 1;
	sc->data.toc[idx].control = sc->data.toc[idx-1].control;
	sc->data.toc[idx].addr_type = sc->data.toc[idx-1].addr_type;
	sc->data.toc[idx].trk_no = 0;
	sc->data.toc[idx].idx_no = MCD_LASTPLUS1;
	sc->data.toc[idx].hd_pos_msf[0] = sc->data.volinfo.vol_msf[0];
	sc->data.toc[idx].hd_pos_msf[1] = sc->data.volinfo.vol_msf[1];
	sc->data.toc[idx].hd_pos_msf[2] = sc->data.volinfo.vol_msf[2];

	if (sc->data.debug)
	{ int i;
	for (i = th.starting_track; i <= idx; i++)
		device_printf(sc->dev, "trk %d idx %d pos %d %d %d\n",
			i,
			sc->data.toc[i].idx_no > 0x99 ? sc->data.toc[i].idx_no :
			bcd2bin(sc->data.toc[i].idx_no),
			bcd2bin(sc->data.toc[i].hd_pos_msf[0]),
			bcd2bin(sc->data.toc[i].hd_pos_msf[1]),
			bcd2bin(sc->data.toc[i].hd_pos_msf[2]));
	}

	sc->data.flags |= MCDTOC;

	return (0);
}

#if 0
static int
mcd_toc_entry(struct mcd_softc *sc, struct ioc_read_toc_single_entry *te)
{
	struct ioc_toc_header th;
	int rc, trk;

	if (te->address_format != CD_MSF_FORMAT
	    && te->address_format != CD_LBA_FORMAT)
		return (EINVAL);

	/* Copy the toc header */
	if ((rc = mcd_toc_header(sc, &th)) != 0)
		return (rc);

	/* verify starting track */
	trk = te->track;
	if (trk == 0)
		trk = th.starting_track;
	else if (trk == MCD_LASTPLUS1)
		trk = th.ending_track + 1;
	else if (trk < th.starting_track || trk > th.ending_track + 1)
		return (EINVAL);

	/* Make sure we have a valid toc */
	if ((rc=mcd_read_toc(sc)) != 0)
		return (rc);

	/* Copy the TOC data. */
	if (sc->data.toc[trk].idx_no == 0)
		return (EIO);

	te->entry.control = sc->data.toc[trk].control;
	te->entry.addr_type = sc->data.toc[trk].addr_type;
	te->entry.track =
		sc->data.toc[trk].idx_no > 0x99 ? sc->data.toc[trk].idx_no :
		bcd2bin(sc->data.toc[trk].idx_no);
	switch (te->address_format) {
	case CD_MSF_FORMAT:
		te->entry.addr.msf.unused = 0;
		te->entry.addr.msf.minute = bcd2bin(sc->data.toc[trk].hd_pos_msf[0]);
		te->entry.addr.msf.second = bcd2bin(sc->data.toc[trk].hd_pos_msf[1]);
		te->entry.addr.msf.frame = bcd2bin(sc->data.toc[trk].hd_pos_msf[2]);
		break;
	case CD_LBA_FORMAT:
		te->entry.addr.lba = htonl(msf2hsg(sc->data.toc[trk].hd_pos_msf, 0));
		break;
	}
	return (0);
}
#endif

static int
mcd_toc_entrys(struct mcd_softc *sc, struct ioc_read_toc_entry *te)
{
	struct cd_toc_entry entries[MCD_MAXTOCS];
	struct ioc_toc_header th;
	int rc, n, trk, len;

	if (   te->data_len < sizeof(entries[0])
	    || (te->data_len % sizeof(entries[0])) != 0
	    || (te->address_format != CD_MSF_FORMAT
	        && te->address_format != CD_LBA_FORMAT)
	   )
		return (EINVAL);

	/* Copy the toc header */
	if ((rc = mcd_toc_header(sc, &th)) != 0)
		return (rc);

	/* verify starting track */
	trk = te->starting_track;
	if (trk == 0)
		trk = th.starting_track;
	else if (trk == MCD_LASTPLUS1)
		trk = th.ending_track + 1;
	else if (trk < th.starting_track || trk > th.ending_track + 1)
		return (EINVAL);

	len = ((th.ending_track + 1 - trk) + 1) *
		sizeof(entries[0]);
	if (te->data_len < len)
		len = te->data_len;
	if (len > sizeof(entries))
		return (EINVAL);

	/* Make sure we have a valid toc */
	if ((rc=mcd_read_toc(sc)) != 0)
		return (rc);

	/* Copy the TOC data. */
	for (n = 0; len > 0 && trk <= th.ending_track + 1; trk++) {
		if (sc->data.toc[trk].idx_no == 0)
			continue;
		entries[n].control = sc->data.toc[trk].control;
		entries[n].addr_type = sc->data.toc[trk].addr_type;
		entries[n].track =
			sc->data.toc[trk].idx_no > 0x99 ? sc->data.toc[trk].idx_no :
			bcd2bin(sc->data.toc[trk].idx_no);
		switch (te->address_format) {
		case CD_MSF_FORMAT:
			entries[n].addr.msf.unused = 0;
			entries[n].addr.msf.minute = bcd2bin(sc->data.toc[trk].hd_pos_msf[0]);
			entries[n].addr.msf.second = bcd2bin(sc->data.toc[trk].hd_pos_msf[1]);
			entries[n].addr.msf.frame = bcd2bin(sc->data.toc[trk].hd_pos_msf[2]);
			break;
		case CD_LBA_FORMAT:
			entries[n].addr.lba = htonl(msf2hsg(sc->data.toc[trk].hd_pos_msf, 0));
			break;
		}
		len -= sizeof(struct cd_toc_entry);
		n++;
	}

	/* copy the data back */
	return copyout(entries, te->data, n * sizeof(struct cd_toc_entry));
}

static int
mcd_stop(struct mcd_softc *sc)
{

	/* Verify current status */
	if (sc->data.audio_status != CD_AS_PLAY_IN_PROGRESS &&
	    sc->data.audio_status != CD_AS_PLAY_PAUSED &&
	    sc->data.audio_status != CD_AS_PLAY_COMPLETED) {
		if (sc->data.debug)
			device_printf(sc->dev,
				"stop attempted when not playing, audio status %d\n",
				sc->data.audio_status);
		return (EINVAL);
	}
	if (sc->data.audio_status == CD_AS_PLAY_IN_PROGRESS)
		if (mcd_send(sc, MCD_CMDSTOPAUDIO, MCD_RETRYS) < 0)
			return (EIO);
	sc->data.audio_status = CD_AS_PLAY_COMPLETED;
	return (0);
}

static int
mcd_getqchan(struct mcd_softc *sc, struct mcd_qchninfo *q)
{

	if (mcd_send(sc, MCD_CMDGETQCHN, MCD_RETRYS) < 0)
		return (-1);
	if (mcd_get(sc, (char *) q, sizeof(struct mcd_qchninfo)) < 0)
		return (-1);
	if (sc->data.debug) {
		device_printf(sc->dev,
			"getqchan control=0x%x addr_type=0x%x trk=%d ind=%d ttm=%d:%d.%d dtm=%d:%d.%d\n",
			q->control, q->addr_type,
			bcd2bin(q->trk_no),
			bcd2bin(q->idx_no),
			bcd2bin(q->trk_size_msf[0]),
			bcd2bin(q->trk_size_msf[1]),
			bcd2bin(q->trk_size_msf[2]),
			bcd2bin(q->hd_pos_msf[0]),
			bcd2bin(q->hd_pos_msf[1]),
			bcd2bin(q->hd_pos_msf[2]));
	}
	return (0);
}

static int
mcd_subchan(struct mcd_softc *sc, struct ioc_read_subchannel *sch, int nocopyout)
{
	struct mcd_qchninfo q;
	struct cd_sub_channel_info data;
	int lba;

	if (sc->data.debug)
		device_printf(sc->dev, "subchan af=%d, df=%d\n",
			sch->address_format,
			sch->data_format);

	if (sch->address_format != CD_MSF_FORMAT &&
	    sch->address_format != CD_LBA_FORMAT)
		return (EINVAL);

	if (sch->data_format != CD_CURRENT_POSITION &&
	    sch->data_format != CD_MEDIA_CATALOG)
		return (EINVAL);

	if (mcd_setmode(sc, MCD_MD_COOKED) != 0)
		return (EIO);

	if (mcd_getqchan(sc, &q) < 0)
		return (EIO);

	data.header.audio_status = sc->data.audio_status;
	data.what.position.data_format = sch->data_format;

	switch (sch->data_format) {
	case CD_MEDIA_CATALOG:
		data.what.media_catalog.mc_valid = 1;
		data.what.media_catalog.mc_number[0] = '\0';
		break;

	case CD_CURRENT_POSITION:
		data.what.position.control = q.control;
		data.what.position.addr_type = q.addr_type;
		data.what.position.track_number = bcd2bin(q.trk_no);
		data.what.position.index_number = bcd2bin(q.idx_no);
		switch (sch->address_format) {
		case CD_MSF_FORMAT:
			data.what.position.reladdr.msf.unused = 0;
			data.what.position.reladdr.msf.minute = bcd2bin(q.trk_size_msf[0]);
			data.what.position.reladdr.msf.second = bcd2bin(q.trk_size_msf[1]);
			data.what.position.reladdr.msf.frame = bcd2bin(q.trk_size_msf[2]);
			data.what.position.absaddr.msf.unused = 0;
			data.what.position.absaddr.msf.minute = bcd2bin(q.hd_pos_msf[0]);
			data.what.position.absaddr.msf.second = bcd2bin(q.hd_pos_msf[1]);
			data.what.position.absaddr.msf.frame = bcd2bin(q.hd_pos_msf[2]);
			break;
		case CD_LBA_FORMAT:
			lba = msf2hsg(q.trk_size_msf, 1);
			/*
			 * Pre-gap has index number of 0, and decreasing MSF
			 * address.  Must be converted to negative LBA, per
			 * SCSI spec.
			 */
			if (data.what.position.index_number == 0)
				lba = -lba;
			data.what.position.reladdr.lba = htonl(lba);
			data.what.position.absaddr.lba = htonl(msf2hsg(q.hd_pos_msf, 0));
			break;
		}
		break;
	}

	if (nocopyout == 0)
		return copyout(&data, sch->data, min(sizeof(struct cd_sub_channel_info), sch->data_len));
	bcopy(&data, sch->data, min(sizeof(struct cd_sub_channel_info), sch->data_len));
	return (0)
}

static int
mcd_playmsf(struct mcd_softc *sc, struct ioc_play_msf *p)
{
	struct mcd_read2 pb;

	if (sc->data.debug)
		device_printf(sc->dev, "playmsf: from %d:%d.%d to %d:%d.%d\n",
		    p->start_m, p->start_s, p->start_f,
		    p->end_m, p->end_s, p->end_f);

	if ((p->start_m * 60 * 75 + p->start_s * 75 + p->start_f) >=
	    (p->end_m * 60 * 75 + p->end_s * 75 + p->end_f) ||
	    (p->end_m * 60 * 75 + p->end_s * 75 + p->end_f) >
	    M_msf(sc->data.volinfo.vol_msf) * 60 * 75 +
	    S_msf(sc->data.volinfo.vol_msf) * 75 +
	    F_msf(sc->data.volinfo.vol_msf))
		return (EINVAL);

	pb.start_msf[0] = bin2bcd(p->start_m);
	pb.start_msf[1] = bin2bcd(p->start_s);
	pb.start_msf[2] = bin2bcd(p->start_f);
	pb.end_msf[0] = bin2bcd(p->end_m);
	pb.end_msf[1] = bin2bcd(p->end_s);
	pb.end_msf[2] = bin2bcd(p->end_f);

	if (mcd_setmode(sc, MCD_MD_COOKED) != 0)
		return (EIO);

	return mcd_play(sc, &pb);
}

static int
mcd_playtracks(struct mcd_softc *sc, struct ioc_play_track *pt)
{
	struct mcd_read2 pb;
	int a = pt->start_track;
	int z = pt->end_track;
	int rc, i;

	if ((rc = mcd_read_toc(sc)) != 0)
		return (rc);

	if (sc->data.debug)
		device_printf(sc->dev, "playtracks from %d:%d to %d:%d\n",
			a, pt->start_index, z, pt->end_index);

	if (   a < bcd2bin(sc->data.volinfo.trk_low)
	    || a > bcd2bin(sc->data.volinfo.trk_high)
	    || a > z
	    || z < bcd2bin(sc->data.volinfo.trk_low)
	    || z > bcd2bin(sc->data.volinfo.trk_high))
		return (EINVAL);

	for (i = 0; i < 3; i++) {
		pb.start_msf[i] = sc->data.toc[a].hd_pos_msf[i];
		pb.end_msf[i] = sc->data.toc[z+1].hd_pos_msf[i];
	}

	if (mcd_setmode(sc, MCD_MD_COOKED) != 0)
		return (EIO);

	return mcd_play(sc, &pb);
}

static int
mcd_playblocks(struct mcd_softc *sc, struct ioc_play_blocks *p)
{
	struct mcd_read2 pb;

	if (sc->data.debug)
		device_printf(sc->dev, "playblocks: blkno %d length %d\n",
		    p->blk, p->len);

	if (p->blk > sc->data.disksize || p->len > sc->data.disksize ||
	    p->blk < 0 || p->len < 0 ||
	    (p->blk + p->len) > sc->data.disksize)
		return (EINVAL);

	hsg2msf(p->blk, pb.start_msf);
	hsg2msf(p->blk + p->len, pb.end_msf);

	if (mcd_setmode(sc, MCD_MD_COOKED) != 0)
		return (EIO);

	return mcd_play(sc, &pb);
}

static int
mcd_play(struct mcd_softc *sc, struct mcd_read2 *pb)
{
	int retry, st = -1, status;

	sc->data.lastpb = *pb;
	for(retry=0; retry<MCD_RETRYS; retry++) {

		critical_enter();
		MCD_WRITE(sc, MCD_REG_COMMAND, MCD_CMDSINGLESPEEDREAD);
		MCD_WRITE(sc, MCD_REG_COMMAND, pb->start_msf[0]);
		MCD_WRITE(sc, MCD_REG_COMMAND, pb->start_msf[1]);
		MCD_WRITE(sc, MCD_REG_COMMAND, pb->start_msf[2]);
		MCD_WRITE(sc, MCD_REG_COMMAND, pb->end_msf[0]);
		MCD_WRITE(sc, MCD_REG_COMMAND, pb->end_msf[1]);
		MCD_WRITE(sc, MCD_REG_COMMAND, pb->end_msf[2]);
		critical_exit();

		status=mcd_getstat(sc, 0);
		if (status == -1)
			continue;
		else if (status != -2)
			st = 0;
		break;
	}

	if (status == -2) {
		device_printf(sc->dev, "media changed\n");
		return (ENXIO);
	}
	if (sc->data.debug)
		device_printf(sc->dev,
			"mcd_play retry=%d, status=0x%02x\n", retry, status);
	if (st < 0)
		return (ENXIO);
	sc->data.audio_status = CD_AS_PLAY_IN_PROGRESS;
	return (0);
}

static int
mcd_pause(struct mcd_softc *sc)
{
	struct mcd_qchninfo q;
	int rc;

	/* Verify current status */
	if (sc->data.audio_status != CD_AS_PLAY_IN_PROGRESS &&
	    sc->data.audio_status != CD_AS_PLAY_PAUSED) {
		if (sc->data.debug)
			device_printf(sc->dev,
				"pause attempted when not playing, audio status %d\n",
				sc->data.audio_status);
		return (EINVAL);
	}

	/* Get the current position */
	if (mcd_getqchan(sc, &q) < 0)
		return (EIO);

	/* Copy it into lastpb */
	sc->data.lastpb.start_msf[0] = q.hd_pos_msf[0];
	sc->data.lastpb.start_msf[1] = q.hd_pos_msf[1];
	sc->data.lastpb.start_msf[2] = q.hd_pos_msf[2];

	/* Stop playing */
	if ((rc=mcd_stop(sc)) != 0)
		return (rc);

	/* Set the proper status and exit */
	sc->data.audio_status = CD_AS_PLAY_PAUSED;
	return (0);
}

static int
mcd_resume(struct mcd_softc *sc)
{

	if (sc->data.audio_status != CD_AS_PLAY_PAUSED)
		return (EINVAL);
	return mcd_play(sc, &sc->data.lastpb);
}
