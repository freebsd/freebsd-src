/*-
 * Copyright (c) 1995 Mikael Hybsch
 * All rights reserved.
 *
 * Portions of this file are copied from mcd.c
 * which has the following copyrights:
 *
 *	Copyright 1993 by Holger Veit (data part)
 *	Copyright 1993 by Brian Moore (audio part)
 *	Changes Copyright 1993 by Gary Clark II
 *	Changes Copyright (C) 1994 by Andrew A. Chernov
 *
 *	Rewrote probe routine to work on newer Mitsumi drives.
 *	Additional changes (C) 1994 by Jordan K. Hubbard
 *
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* $FreeBSD$ */

#undef	SCD_DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/bio.h>
#include <sys/cdio.h>
#include <sys/disk.h>
#include <sys/bus.h>

#include <machine/stdarg.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <dev/scd/scdreg.h>
#include <dev/scd/scdvar.h>

/* flags */
#define SCDOPEN		0x0001	/* device opened */
#define SCDVALID	0x0002	/* parameters loaded */
#define SCDINIT		0x0004	/* device is init'd */
#define	SCDPROBING	0x0020	/* probing */
#define	SCDTOC		0x0100	/* already read toc */
#define	SCDMBXBSY	0x0200	/* local mbx is busy */
#define	SCDSPINNING	0x0400  /* drive is spun up */

#define SCD_S_BEGIN	0
#define SCD_S_BEGIN1	1
#define SCD_S_WAITSTAT	2
#define	SCD_S_WAITFIFO	3
#define SCD_S_WAITSPIN	4
#define SCD_S_WAITREAD	5
#define	SCD_S_WAITPARAM 6

#define RDELAY_WAIT	300
#define RDELAY_WAITREAD	300

#define	SCDBLKSIZE	2048

#ifdef SCD_DEBUG
   static int scd_debuglevel = SCD_DEBUG;
#  define XDEBUG(sc, level, fmt, args...) \
	do { \
		if (scd_debuglevel >= level) \
			device_printf(sc->dev, fmt, ## args); \
	} while (0)
#else
#  define XDEBUG(sc, level, fmt, args...)
#endif

#define	IS_ATTENTION(sc)	((SCD_READ(sc, IREG_STATUS) & SBIT_ATTENTION) != 0)
#define	IS_BUSY(sc)		((SCD_READ(sc, IREG_STATUS) & SBIT_BUSY) != 0)
#define	IS_DATA_RDY(sc)		((SCD_READ(sc, IREG_STATUS) & SBIT_DATA_READY) != 0)
#define	STATUS_BIT(sc, bit)	((SCD_READ(sc, IREG_STATUS) & (bit)) != 0)
#define	FSTATUS_BIT(sc, bit)	((SCD_READ(sc, IREG_FSTATUS) & (bit)) != 0)

/* prototypes */
static	void	hsg2msf(int hsg, bcd_t *msf);
static	int	msf2hsg(bcd_t *msf);

static void process_attention(struct scd_softc *);
static int waitfor_status_bits(struct scd_softc *, int bits_set, int bits_clear);
static int send_cmd(struct scd_softc *, u_char cmd, u_int nargs, ...);
static void init_drive(struct scd_softc *);
static int spin_up(struct scd_softc *);
static int read_toc(struct scd_softc *);
static int get_result(struct scd_softc *, int result_len, u_char *result);
static void print_error(struct scd_softc *, int errcode);

static void scd_start(struct scd_softc *);
static timeout_t scd_timeout;
static void scd_doread(struct scd_softc *, int state, struct scd_mbx *mbxin);

static int scd_eject(struct scd_softc *);
static int scd_stop(struct scd_softc *);
static int scd_pause(struct scd_softc *);
static int scd_resume(struct scd_softc *);
static int scd_playtracks(struct scd_softc *, struct ioc_play_track *pt);
static int scd_playmsf(struct scd_softc *, struct ioc_play_msf *msf);
static int scd_play(struct scd_softc *, struct ioc_play_msf *msf);
static int scd_subchan(struct scd_softc *, struct ioc_read_subchannel *sch);
static int read_subcode(struct scd_softc *, struct sony_subchannel_position_data *sch);

/* for xcdplayer */
static int scd_toc_header(struct scd_softc *, struct ioc_toc_header *th);
static int scd_toc_entrys(struct scd_softc *, struct ioc_read_toc_entry *te);
static int scd_toc_entry(struct scd_softc *, struct ioc_read_toc_single_entry *te);
#define SCD_LASTPLUS1 170 /* don't ask, xcdplayer passes this in */

static	d_open_t	scdopen;
static	d_close_t	scdclose;
static	d_ioctl_t	scdioctl;
static	d_strategy_t	scdstrategy;

#define CDEV_MAJOR 45

static struct cdevsw scd_cdevsw = {
	/* open */	scdopen,
	/* close */	scdclose,
	/* read */	physread,
	/* write */	nowrite,
	/* ioctl */	scdioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	scdstrategy,
	/* name */	"scd",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_DISK,
};

int
scd_attach(struct scd_softc *sc)
{
	int unit;

	unit = device_get_unit(sc->dev);

	init_drive(sc);

	sc->data.flags = SCDINIT;
	sc->data.audio_status = CD_AS_AUDIO_INVALID;
	bioq_init(&sc->data.head);

	sc->scd_dev_t = make_dev(&scd_cdevsw, 8 * unit,
		UID_ROOT, GID_OPERATOR, 0640, "scd%d", unit);
	sc->scd_dev_t->si_drv1 = (void *)sc;

	return (0);
}

static	int
scdopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct scd_softc *sc;
	int rc;

	sc = (struct scd_softc *)dev->si_drv1;

	/* not initialized*/
	if (!(sc->data.flags & SCDINIT))
		return (ENXIO);

	/* invalidated in the meantime? mark all open part's invalid */
	if (sc->data.openflag)
		return (ENXIO);

	XDEBUG(sc, 1, "DEBUG: status = 0x%x\n", SCD_READ(sc, IREG_STATUS));

	if ((rc = spin_up(sc)) != 0) {
		print_error(sc, rc);
		return (EIO);
	}
	if (!(sc->data.flags & SCDTOC)) {
		int loop_count = 3;

		while (loop_count-- > 0 && (rc = read_toc(sc)) != 0) {
			if (rc == ERR_NOT_SPINNING) {
				rc = spin_up(sc);
				if (rc) {
					print_error(sc, rc);\
					return (EIO);
				}
				continue;
			}
			device_printf(sc->dev, "TOC read error 0x%x\n", rc);
			return (EIO);
		}
	}

	dev->si_bsize_phys = sc->data.blksize;

	sc->data.openflag = 1;
	sc->data.flags |= SCDVALID;

	return (0);
}

static	int
scdclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct scd_softc *sc;

	sc = (struct scd_softc *)dev->si_drv1;

	if (!(sc->data.flags & SCDINIT) || !sc->data.openflag)
		return (ENXIO);

	if (sc->data.audio_status != CD_AS_PLAY_IN_PROGRESS) {
		(void)send_cmd(sc, CMD_SPIN_DOWN, 0);
		sc->data.flags &= ~SCDSPINNING;
	}


	/* close channel */
	sc->data.openflag = 0;

	return (0);
}

static	void
scdstrategy(struct bio *bp)
{
	int s;
	struct scd_softc *sc;

	sc = (struct scd_softc *)bp->bio_dev->si_drv1;

	XDEBUG(sc, 2, "DEBUG: strategy: block=%ld, bcount=%ld\n",
		(long)bp->bio_blkno, bp->bio_bcount);

	if (bp->bio_blkno < 0 || (bp->bio_bcount % SCDBLKSIZE)) {
		device_printf(sc->dev, "strategy failure: blkno = %ld, bcount = %ld\n",
			(long)bp->bio_blkno, bp->bio_bcount);
		bp->bio_error = EINVAL;
		bp->bio_flags |= BIO_ERROR;
		goto bad;
	}

	/* if device invalidated (e.g. media change, door open), error */
	if (!(sc->data.flags & SCDVALID)) {
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

	if (!(sc->data.flags & SCDTOC)) {
		bp->bio_error = EIO;
		goto bad;
	}

	bp->bio_pblkno = bp->bio_blkno;
	bp->bio_resid = 0;

	/* queue it */
	s = splbio();
	bioqdisksort(&sc->data.head, bp);
	splx(s);

	/* now check whether we can perform processing */
	scd_start(sc);
	return;

bad:
	bp->bio_flags |= BIO_ERROR;
done:
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);
	return;
}

static void
scd_start(struct scd_softc *sc)
{
	struct bio *bp;
	int s = splbio();

	if (sc->data.flags & SCDMBXBSY) {
		splx(s);
		return;
	}

	bp = bioq_first(&sc->data.head);
	if (bp != 0) {
		/* block found to process, dequeue */
		bioq_remove(&sc->data.head, bp);
		sc->data.flags |= SCDMBXBSY;
		splx(s);
	} else {
		/* nothing to do */
		splx(s);
		return;
	}

	sc->data.mbx.retry = 3;
	sc->data.mbx.bp = bp;
	splx(s);

	scd_doread(sc, SCD_S_BEGIN, &(sc->data.mbx));
	return;
}

static	int
scdioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
	struct scd_softc *sc;

	sc = (struct scd_softc *)dev->si_drv1;

	XDEBUG(sc, 1, "ioctl: cmd=0x%lx\n", cmd);

	if (!(sc->data.flags & SCDVALID))
		return (EIO);

	switch (cmd) {
	case DIOCGMEDIASIZE:
		*(off_t *)addr = (off_t)sc->data.disksize * sc->data.blksize;
		return (0);
		break;
	case DIOCGSECTORSIZE:
		*(u_int *)addr = sc->data.blksize;
		return (0);
		break;
	case CDIOCPLAYTRACKS:
		return scd_playtracks(sc, (struct ioc_play_track *) addr);
	case CDIOCPLAYBLOCKS:
		return (EINVAL);
	case CDIOCPLAYMSF:
		return scd_playmsf(sc, (struct ioc_play_msf *) addr);
	case CDIOCREADSUBCHANNEL:
		return scd_subchan(sc, (struct ioc_read_subchannel *) addr);
	case CDIOREADTOCHEADER:
		return scd_toc_header (sc, (struct ioc_toc_header *) addr);
	case CDIOREADTOCENTRYS:
		return scd_toc_entrys (sc, (struct ioc_read_toc_entry*) addr);
	case CDIOREADTOCENTRY:
		return scd_toc_entry (sc, (struct ioc_read_toc_single_entry*) addr);
	case CDIOCSETPATCH:
	case CDIOCGETVOL:
	case CDIOCSETVOL:
	case CDIOCSETMONO:
	case CDIOCSETSTERIO:
	case CDIOCSETMUTE:
	case CDIOCSETLEFT:
	case CDIOCSETRIGHT:
		return (EINVAL);
	case CDIOCRESUME:
		return scd_resume(sc);
	case CDIOCPAUSE:
		return scd_pause(sc);
	case CDIOCSTART:
		return (EINVAL);
	case CDIOCSTOP:
		return scd_stop(sc);
	case CDIOCEJECT:
		return scd_eject(sc);
	case CDIOCALLOW:
		return (0);
	case CDIOCSETDEBUG:
#ifdef SCD_DEBUG
		scd_debuglevel++;
#endif
		return (0);
	case CDIOCCLRDEBUG:
#ifdef SCD_DEBUG
		scd_debuglevel = 0;

#endif
		return (0);
	default:
		device_printf(sc->dev, "unsupported ioctl (cmd=0x%lx)\n", cmd);
		return (ENOTTY);
	}
}

/***************************************************************
 * lower level of driver starts here
 **************************************************************/

static int
scd_playtracks(struct scd_softc *sc, struct ioc_play_track *pt)
{
	struct ioc_play_msf msf;
	int a = pt->start_track;
	int z = pt->end_track;
	int rc;

	if (!(sc->data.flags & SCDTOC) && (rc = read_toc(sc)) != 0) {
		if (rc == -ERR_NOT_SPINNING) {
			if (spin_up(sc) != 0)
				return (EIO);
			rc = read_toc(sc);
		}
		if (rc != 0) {
			print_error(sc, rc);
			return (EIO);
		}
	}

	XDEBUG(sc, 1, "playtracks from %d:%d to %d:%d\n",
		a, pt->start_index, z, pt->end_index);

	if (   a < sc->data.first_track
	    || a > sc->data.last_track
	    || a > z
	    || z > sc->data.last_track)
		return (EINVAL);

	bcopy(sc->data.toc[a].start_msf, &msf.start_m, 3);
	hsg2msf(msf2hsg(sc->data.toc[z+1].start_msf)-1, &msf.end_m);

	return scd_play(sc, &msf);
}

/* The start/end msf is expected to be in bin format */
static int
scd_playmsf(struct scd_softc *sc, struct ioc_play_msf *msfin)
{
	struct ioc_play_msf msf;

	msf.start_m = bin2bcd(msfin->start_m);
	msf.start_s = bin2bcd(msfin->start_s);
	msf.start_f = bin2bcd(msfin->start_f);
	msf.end_m = bin2bcd(msfin->end_m);
	msf.end_s = bin2bcd(msfin->end_s);
	msf.end_f = bin2bcd(msfin->end_f);

	return scd_play(sc, &msf);
}

/* The start/end msf is expected to be in bcd format */
static int
scd_play(struct scd_softc *sc, struct ioc_play_msf *msf)
{
	int i, rc;

	XDEBUG(sc, 1, "playing: %02x:%02x:%02x -> %02x:%02x:%02x\n",
		msf->start_m, msf->start_s, msf->start_f,
		msf->end_m, msf->end_s, msf->end_f);

	for (i = 0; i < 2; i++) {
		rc = send_cmd(sc, CMD_PLAY_AUDIO, 7,
			0x03,
			msf->start_m, msf->start_s, msf->start_f,
			msf->end_m, msf->end_s, msf->end_f);
		if (rc == -ERR_NOT_SPINNING) {
			sc->data.flags &= ~SCDSPINNING;
			if (spin_up(sc) != 0)
				return (EIO);
		} else if (rc < 0) {
			print_error(sc, rc);
			return (EIO);
		} else {
			break;
		}
	}
	sc->data.audio_status = CD_AS_PLAY_IN_PROGRESS;
	bcopy((char *)msf, (char *)&sc->data.last_play, sizeof(struct ioc_play_msf));
	return (0);
}

static int
scd_stop(struct scd_softc *sc)
{

	(void)send_cmd(sc, CMD_STOP_AUDIO, 0);
	sc->data.audio_status = CD_AS_PLAY_COMPLETED;
	return (0);
}

static int
scd_pause(struct scd_softc *sc)
{
	struct sony_subchannel_position_data subpos;

	if (sc->data.audio_status != CD_AS_PLAY_IN_PROGRESS)
		return (EINVAL);

	if (read_subcode(sc, &subpos) != 0)
		return (EIO);

	if (send_cmd(sc, CMD_STOP_AUDIO, 0) != 0)
		return (EIO);

	sc->data.last_play.start_m = subpos.abs_msf[0];
	sc->data.last_play.start_s = subpos.abs_msf[1];
	sc->data.last_play.start_f = subpos.abs_msf[2];
	sc->data.audio_status = CD_AS_PLAY_PAUSED;

	XDEBUG(sc, 1, "pause @ %02x:%02x:%02x\n",
		sc->data.last_play.start_m,
		sc->data.last_play.start_s,
		sc->data.last_play.start_f);

	return (0);
}

static int
scd_resume(struct scd_softc *sc)
{

	if (sc->data.audio_status != CD_AS_PLAY_PAUSED)
		return (EINVAL);
	return scd_play(sc, &sc->data.last_play);
}

static int
scd_eject(struct scd_softc *sc)
{

	sc->data.audio_status = CD_AS_AUDIO_INVALID;
	sc->data.flags &= ~(SCDSPINNING|SCDTOC);

	if (send_cmd(sc, CMD_STOP_AUDIO, 0) != 0 ||
	    send_cmd(sc, CMD_SPIN_DOWN, 0) != 0 ||
	    send_cmd(sc, CMD_EJECT, 0) != 0)
	{
		return (EIO);
	}
	return (0);
}

static int
scd_subchan(struct scd_softc *sc, struct ioc_read_subchannel *sch)
{
	struct sony_subchannel_position_data q;
	struct cd_sub_channel_info data;

	XDEBUG(sc, 1, "subchan af=%d, df=%d\n",
		sch->address_format, sch->data_format);

	if (sch->address_format != CD_MSF_FORMAT)
		return (EINVAL);

	if (sch->data_format != CD_CURRENT_POSITION)
		return (EINVAL);

	if (read_subcode(sc, &q) != 0)
		return (EIO);

	data.header.audio_status = sc->data.audio_status;
	data.what.position.data_format = CD_MSF_FORMAT;
	data.what.position.track_number = bcd2bin(q.track_number);
	data.what.position.reladdr.msf.unused = 0;
	data.what.position.reladdr.msf.minute = bcd2bin(q.rel_msf[0]);
	data.what.position.reladdr.msf.second = bcd2bin(q.rel_msf[1]);
	data.what.position.reladdr.msf.frame = bcd2bin(q.rel_msf[2]);
	data.what.position.absaddr.msf.unused = 0;
	data.what.position.absaddr.msf.minute = bcd2bin(q.abs_msf[0]);
	data.what.position.absaddr.msf.second = bcd2bin(q.abs_msf[1]);
	data.what.position.absaddr.msf.frame = bcd2bin(q.abs_msf[2]);

	if (copyout(&data, sch->data, min(sizeof(struct cd_sub_channel_info), sch->data_len))!=0)
		return (EFAULT);
	return (0);
}

int
scd_probe(struct scd_softc *sc)
{
	struct sony_drive_configuration drive_config;
	int rc;
	static char namebuf[8+16+8+3];
	char *s = namebuf;
	int loop_count = 0;

	sc->data.flags = SCDPROBING;

	bzero(&drive_config, sizeof(drive_config));

again:
	/* Reset drive */
	SCD_WRITE(sc, OREG_CONTROL, CBIT_RESET_DRIVE);

	/* Calm down */
	DELAY(300000);

	/* Only the ATTENTION bit may be set */
	if ((SCD_READ(sc, IREG_STATUS) & ~1) != 0) {
		XDEBUG(sc, 1, "too many bits set. probe failed.\n");
		return (ENXIO);
	}
	rc = send_cmd(sc, CMD_GET_DRIVE_CONFIG, 0);
	if (rc != sizeof(drive_config)) {
		/* Sometimes if the drive is playing audio I get */
		/* the bad result 82. Fix by repeating the reset */
		if (rc > 0 && loop_count++ == 0)
			goto again;
		return (ENXIO);
	}
	if (get_result(sc, rc, (u_char *)&drive_config) != 0)
		return (ENXIO);

	bcopy(drive_config.vendor, namebuf, 8);
	s = namebuf+8;
	while (*(s-1) == ' ')	/* Strip trailing spaces */
		s--;
	*s++ = ' ';
	bcopy(drive_config.product, s, 16);
	s += 16;
	while (*(s-1) == ' ')
		s--;
	*s++ = ' ';
	bcopy(drive_config.revision, s, 8);
	s += 8;
	while (*(s-1) == ' ')
		s--;
	*s = 0;

	sc->data.name = namebuf;

	if (drive_config.config & 0x10)
		sc->data.double_speed = 1;
	else
		sc->data.double_speed = 0;

	return (0);
}

static int
read_subcode(struct scd_softc *sc, struct sony_subchannel_position_data *scp)
{
	int rc;

	rc = send_cmd(sc, CMD_GET_SUBCHANNEL_DATA, 0);
	if (rc < 0 || rc < sizeof(*scp))
		return (EIO);
	if (get_result(sc, rc, (u_char *)scp) != 0)
		return (EIO);
	return (0);
}

/* State machine copied from mcd.c */

/* This (and the code in mcd.c) will not work with more than one drive */
/* because there is only one sc->ch_mbxsave below. Should fix that some day. */
/* (sc->ch_mbxsave & state should probably be included in the scd_data struct and */
/*  the unit number used as first argument to scd_doread().) /Micke */

/* state machine to process read requests
 * initialize with SCD_S_BEGIN: reset state machine
 * SCD_S_WAITSTAT:  wait for ready (!busy)
 * SCD_S_WAITSPIN:  wait for drive to spin up (if not spinning)
 * SCD_S_WAITFIFO:  wait for param fifo to get ready, them exec. command.
 * SCD_S_WAITREAD:  wait for data ready, read data
 * SCD_S_WAITPARAM: wait for command result params, read them, error if bad data read.
 */

static void
scd_timeout(void *arg)
{
	struct scd_softc *sc;
	sc = (struct scd_softc *)arg;

	scd_doread(sc, sc->ch_state, sc->ch_mbxsave);
}

static void
scd_doread(struct scd_softc *sc, int state, struct scd_mbx *mbxin)
{
	struct scd_mbx *mbx = (state!=SCD_S_BEGIN) ? sc->ch_mbxsave : mbxin;
	struct	bio *bp = mbx->bp;
	int	i;
	int	blknum;
	caddr_t	addr;
	static char sdata[3];	/* Must be preserved between calls to this function */

loop:
	switch (state) {
	case SCD_S_BEGIN:
		mbx = sc->ch_mbxsave = mbxin;

	case SCD_S_BEGIN1:
		/* get status */
		mbx->count = RDELAY_WAIT;

		process_attention(sc);
		goto trystat;

	case SCD_S_WAITSTAT:
		sc->ch_state = SCD_S_WAITSTAT;
		untimeout(scd_timeout, (caddr_t)sc, sc->ch);
		if (mbx->count-- <= 0) {
			device_printf(sc->dev, "timeout. drive busy.\n");
			goto harderr;
		}

trystat:
		if (IS_BUSY(sc)) {
			sc->ch_state = SCD_S_WAITSTAT;
			sc->ch = timeout(scd_timeout, (caddr_t)sc, hz/100); /* XXX */
			return;
		}

		process_attention(sc);

		/* reject, if audio active */
		if (sc->data.audio_status & CD_AS_PLAY_IN_PROGRESS) {
			device_printf(sc->dev, "audio is active\n");
			goto harderr;
		}

		mbx->sz = sc->data.blksize;

		/* for first block */
		mbx->nblk = (bp->bio_bcount + (mbx->sz-1)) / mbx->sz;
		mbx->skip = 0;

nextblock:
		if (!(sc->data.flags & SCDVALID))
			goto changed;

		blknum 	= (bp->bio_blkno / (mbx->sz/DEV_BSIZE))
			+ mbx->skip/mbx->sz;

		XDEBUG(sc, 2, "scd_doread: read blknum=%d\n", blknum);

		/* build parameter block */
		hsg2msf(blknum, sdata);

		SCD_WRITE(sc, OREG_CONTROL, CBIT_RESULT_READY_CLEAR);
		SCD_WRITE(sc, OREG_CONTROL, CBIT_RPARAM_CLEAR);
		SCD_WRITE(sc, OREG_CONTROL, CBIT_DATA_READY_CLEAR);

		if (FSTATUS_BIT(sc, FBIT_WPARAM_READY))
			goto writeparam;

		mbx->count = 100;
		sc->ch_state = SCD_S_WAITFIFO;
		sc->ch = timeout(scd_timeout, (caddr_t)sc, hz/100); /* XXX */
		return;

	case SCD_S_WAITSPIN:
		sc->ch_state = SCD_S_WAITSPIN;
		untimeout(scd_timeout,(caddr_t)sc, sc->ch);
		if (mbx->count-- <= 0) {
			device_printf(sc->dev, "timeout waiting for drive to spin up.\n");
			goto harderr;
		}
		if (!STATUS_BIT(sc, SBIT_RESULT_READY)) {
			sc->ch_state = SCD_S_WAITSPIN;
			sc->ch = timeout(scd_timeout, (caddr_t)sc, hz/100); /* XXX */
			return;
		}
		SCD_WRITE(sc, OREG_CONTROL, CBIT_RESULT_READY_CLEAR);
		switch ((i = SCD_READ(sc, IREG_RESULT)) & 0xf0) {
		case 0x20:
			i = SCD_READ(sc, IREG_RESULT);
			print_error(sc, i);
			goto harderr;
		case 0x00:
			(void)SCD_READ(sc, IREG_RESULT);
			sc->data.flags |= SCDSPINNING;
			break;
		}
		XDEBUG(sc, 1, "DEBUG: spin up complete\n");

		state = SCD_S_BEGIN1;
		goto loop;

	case SCD_S_WAITFIFO:
		sc->ch_state = SCD_S_WAITFIFO;
		untimeout(scd_timeout,(caddr_t)sc, sc->ch);
		if (mbx->count-- <= 0) {
			device_printf(sc->dev, "timeout. write param not ready.\n");
			goto harderr;
		}
		if (!FSTATUS_BIT(sc, FBIT_WPARAM_READY)) {
			sc->ch_state = SCD_S_WAITFIFO;
			sc->ch = timeout(scd_timeout, (caddr_t)sc,hz/100); /* XXX */
			return;
		}
		XDEBUG(sc, 1, "mbx->count (writeparamwait) = %d(%d)\n", mbx->count, 100);

writeparam:
		/* The reason this test isn't done 'till now is to make sure */
		/* that it is ok to send the SPIN_UP cmd below. */
		if (!(sc->data.flags & SCDSPINNING)) {
			XDEBUG(sc, 1, "spinning up drive ...\n");
			SCD_WRITE(sc, OREG_COMMAND, CMD_SPIN_UP);
			mbx->count = 300;
			sc->ch_state = SCD_S_WAITSPIN;
			sc->ch = timeout(scd_timeout, (caddr_t)sc, hz/100); /* XXX */
			return;
		}

		/* send the read command */
		critical_enter();
		SCD_WRITE(sc, OREG_WPARAMS, sdata[0]);
		SCD_WRITE(sc, OREG_WPARAMS, sdata[1]);
		SCD_WRITE(sc, OREG_WPARAMS, sdata[2]);
		SCD_WRITE(sc, OREG_WPARAMS, 0);
		SCD_WRITE(sc, OREG_WPARAMS, 0);
		SCD_WRITE(sc, OREG_WPARAMS, 1);
		SCD_WRITE(sc, OREG_COMMAND, CMD_READ);
		critical_exit();

		mbx->count = RDELAY_WAITREAD;
		for (i = 0; i < 50; i++) {
			if (STATUS_BIT(sc, SBIT_DATA_READY))
				goto got_data;
			DELAY(100);
		}

		sc->ch_state = SCD_S_WAITREAD;
		sc->ch = timeout(scd_timeout, (caddr_t)sc, hz/100); /* XXX */
		return;

	case SCD_S_WAITREAD:
		sc->ch_state = SCD_S_WAITREAD;
		untimeout(scd_timeout,(caddr_t)sc, sc->ch);
		if (mbx->count-- <= 0) {
			if (STATUS_BIT(sc, SBIT_RESULT_READY))
				goto got_param;
			device_printf(sc->dev, "timeout while reading data\n");
			goto readerr;
		}
		if (!STATUS_BIT(sc, SBIT_DATA_READY)) {
			process_attention(sc);
			if (!(sc->data.flags & SCDVALID))
				goto changed;
			sc->ch_state = SCD_S_WAITREAD;
			sc->ch = timeout(scd_timeout, (caddr_t)sc, hz/100); /* XXX */
			return;
		}
		XDEBUG(sc, 2, "mbx->count (after RDY_BIT) = %d(%d)\n", mbx->count, RDELAY_WAITREAD);

got_data:
		/* data is ready */
		addr = bp->bio_data + mbx->skip;
		SCD_WRITE(sc, OREG_CONTROL, CBIT_DATA_READY_CLEAR);
		SCD_READ_MULTI(sc, IREG_DATA, addr, mbx->sz);

		mbx->count = 100;
		for (i = 0; i < 20; i++) {
			if (STATUS_BIT(sc, SBIT_RESULT_READY))
				goto waitfor_param;
			DELAY(100);
		}
		goto waitfor_param;

	case SCD_S_WAITPARAM:
		sc->ch_state = SCD_S_WAITPARAM;
		untimeout(scd_timeout,(caddr_t)sc, sc->ch);
		if (mbx->count-- <= 0) {
			device_printf(sc->dev, "timeout waiting for params\n");
			goto readerr;
		}

waitfor_param:
		if (!STATUS_BIT(sc, SBIT_RESULT_READY)) {
			sc->ch_state = SCD_S_WAITPARAM;
			sc->ch = timeout(scd_timeout, (caddr_t)sc, hz/100); /* XXX */
			return;
		}
#ifdef SCD_DEBUG
		if (mbx->count < 100 && scd_debuglevel > 0)
			device_printf(sc->dev, "mbx->count (paramwait) = %d(%d)\n", mbx->count, 100);
#endif

got_param:
		SCD_WRITE(sc, OREG_CONTROL, CBIT_RESULT_READY_CLEAR);
		switch ((i = SCD_READ(sc, IREG_RESULT)) & 0xf0) {
		case 0x50:
			switch (i) {
			case ERR_FATAL_READ_ERROR1:
			case ERR_FATAL_READ_ERROR2:
				device_printf(sc->dev, "unrecoverable read error 0x%x\n", i);
				goto harderr;
			}
			break;
		case 0x20:
			i = SCD_READ(sc, IREG_RESULT);
			switch (i) {
			case ERR_NOT_SPINNING:
				XDEBUG(sc, 1, "read error: drive not spinning\n");
				if (mbx->retry-- > 0) {
					state = SCD_S_BEGIN1;
					sc->data.flags &= ~SCDSPINNING;
					goto loop;
				}
				goto harderr;
			default:
				print_error(sc, i);
				goto readerr;
			}
		case 0x00:
			i = SCD_READ(sc, IREG_RESULT);
			break;
		}

		if (--mbx->nblk > 0) {
			mbx->skip += mbx->sz;
			goto nextblock;
		}

		/* return buffer */
		bp->bio_resid = 0;
		biodone(bp);

		sc->data.flags &= ~SCDMBXBSY;
		scd_start(sc);
		return;
	}

readerr:
	if (mbx->retry-- > 0) {
		device_printf(sc->dev, "retrying ...\n");
		state = SCD_S_BEGIN1;
		goto loop;
	}
harderr:
	/* invalidate the buffer */
	bp->bio_error = EIO;
	bp->bio_flags |= BIO_ERROR;
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);

	sc->data.flags &= ~SCDMBXBSY;
	scd_start(sc);
	return;

changed:
	device_printf(sc->dev, "media changed\n");
	goto harderr;
}

static void
hsg2msf(int hsg, bcd_t *msf)
{

	hsg += 150;
	M_msf(msf) = bin2bcd(hsg / 4500);
	hsg %= 4500;
	S_msf(msf) = bin2bcd(hsg / 75);
	F_msf(msf) = bin2bcd(hsg % 75);
}

static int
msf2hsg(bcd_t *msf)
{

	return (bcd2bin(M_msf(msf)) * 60 +
		bcd2bin(S_msf(msf))) * 75 +
		bcd2bin(F_msf(msf)) - 150;
}

static void
process_attention(struct scd_softc *sc)
{
	unsigned char code;
	int count = 0;

	while (IS_ATTENTION(sc) && count++ < 30) {
		SCD_WRITE(sc, OREG_CONTROL, CBIT_ATTENTION_CLEAR);
		code = SCD_READ(sc, IREG_RESULT);

#ifdef SCD_DEBUG
		if (scd_debuglevel > 0) {
			if (count == 1)
				device_printf(sc->dev, "DEBUG: ATTENTIONS = 0x%x", code);
			else
				printf(",0x%x", code);
		}
#endif

		switch (code) {
		case ATTEN_SPIN_DOWN:
			sc->data.flags &= ~SCDSPINNING;
			break;

		case ATTEN_SPIN_UP_DONE:
			sc->data.flags |= SCDSPINNING;
			break;

		case ATTEN_AUDIO_DONE:
			sc->data.audio_status = CD_AS_PLAY_COMPLETED;
			break;

		case ATTEN_DRIVE_LOADED:
			sc->data.flags &= ~(SCDTOC|SCDSPINNING|SCDVALID);
			sc->data.audio_status = CD_AS_AUDIO_INVALID;
			break;

		case ATTEN_EJECT_PUSHED:
			sc->data.flags &= ~SCDVALID;
			break;
		}
		DELAY(100);
	}
#ifdef SCD_DEBUG
	if (scd_debuglevel > 0 && count > 0)
		printf("\n");
#endif
}

/* Returns 0 OR sony error code */
static int
spin_up(struct scd_softc *sc)
{
	unsigned char res_reg[12];
	unsigned int res_size;
	int rc;
	int loop_count = 0;

again:
	rc = send_cmd(sc, CMD_SPIN_UP, 0, 0, res_reg, &res_size);
	if (rc != 0) {
		XDEBUG(sc, 2, "CMD_SPIN_UP error 0x%x\n", rc);
		return (rc);
	}

	if (!(sc->data.flags & SCDTOC)) {
		rc = send_cmd(sc, CMD_READ_TOC, 0);
		if (rc == ERR_NOT_SPINNING) {
			if (loop_count++ < 3)
				goto again;
			return (rc);
		}
		if (rc != 0)
			return (rc);
	}

	sc->data.flags |= SCDSPINNING;

	return (0);
}

static struct sony_tracklist *
get_tl(struct sony_toc *toc, int size)
{
	struct sony_tracklist *tl = &toc->tracks[0];

	if (tl->track != 0xb0)
		return (tl);
	(char *)tl += 9;
	if (tl->track != 0xb1)
		return (tl);
	(char *)tl += 9;
	if (tl->track != 0xb2)
		return (tl);
	(char *)tl += 9;
	if (tl->track != 0xb3)
		return (tl);
	(char *)tl += 9;
	if (tl->track != 0xb4)
		return (tl);
	(char *)tl += 9;
	if (tl->track != 0xc0)
		return (tl);
	(char *)tl += 9;
	return (tl);
}

static int
read_toc(struct scd_softc *sc)
{
	struct sony_toc toc;
	struct sony_tracklist *tl;
	int rc, i, j;
	u_long first, last;

	rc = send_cmd(sc, CMD_GET_TOC, 1, 1);
	if (rc < 0)
		return (rc);
	if (rc > sizeof(toc)) {
		device_printf(sc->dev, "program error: toc too large (%d)\n", rc);
		return (EIO);
	}
	if (get_result(sc, rc, (u_char *)&toc) != 0)
		return (EIO);

	XDEBUG(sc, 1, "toc read. len = %d, sizeof(toc) = %d\n", rc, sizeof(toc));

	tl = get_tl(&toc, rc);
	first = msf2hsg(tl->start_msf);
	last = msf2hsg(toc.lead_out_start_msf);
	sc->data.blksize = SCDBLKSIZE;
	sc->data.disksize = last*sc->data.blksize/DEV_BSIZE;

	XDEBUG(sc, 1, "firstsector = %ld, lastsector = %ld", first, last);

	sc->data.first_track = bcd2bin(toc.first_track);
	sc->data.last_track = bcd2bin(toc.last_track);
	if (sc->data.last_track > (MAX_TRACKS-2))
		sc->data.last_track = MAX_TRACKS-2;
	for (j = 0, i = sc->data.first_track; i <= sc->data.last_track; i++, j++) {
		sc->data.toc[i].adr = tl[j].adr;
		sc->data.toc[i].ctl = tl[j].ctl; /* for xcdplayer */
		bcopy(tl[j].start_msf, sc->data.toc[i].start_msf, 3);
#ifdef SCD_DEBUG
		if (scd_debuglevel > 0) {
			if ((j % 3) == 0) {
				printf("\n");
				device_printf(sc->dev, "tracks ");
			}
			printf("[%03d: %2d %2d %2d]  ", i,
				bcd2bin(sc->data.toc[i].start_msf[0]),
				bcd2bin(sc->data.toc[i].start_msf[1]),
				bcd2bin(sc->data.toc[i].start_msf[2]));
		}
#endif
	}
	bcopy(toc.lead_out_start_msf, sc->data.toc[sc->data.last_track+1].start_msf, 3);
#ifdef SCD_DEBUG
	if (scd_debuglevel > 0) {
		i = sc->data.last_track+1;
		printf("[END: %2d %2d %2d]\n",
			bcd2bin(sc->data.toc[i].start_msf[0]),
			bcd2bin(sc->data.toc[i].start_msf[1]),
			bcd2bin(sc->data.toc[i].start_msf[2]));
	}
#endif

	sc->data.flags |= SCDTOC;

	return (0);
}

static void
init_drive(struct scd_softc *sc)
{
	int rc;

	rc = send_cmd(sc, CMD_SET_DRIVE_PARAM, 2,
		0x05, 0x03 | ((sc->data.double_speed) ? 0x04: 0));
	if (rc != 0)
		device_printf(sc->dev, "Unable to set parameters. Errcode = 0x%x\n", rc);
}

/* Returns 0 or errno */
static int
get_result(struct scd_softc *sc, int result_len, u_char *result)
{
	int loop_index = 2; /* send_cmd() reads two bytes ... */

	XDEBUG(sc, 1, "DEBUG: get_result: bytes=%d\n", result_len);

	while (result_len-- > 0) {
		if (loop_index++ >= 10) {
			loop_index = 1;
			if (waitfor_status_bits(sc, SBIT_RESULT_READY, 0))
				return (EIO);
			SCD_WRITE(sc, OREG_CONTROL, CBIT_RESULT_READY_CLEAR);
		}
		if (result)
			*result++ = SCD_READ(sc, IREG_RESULT);
		else
			(void)SCD_READ(sc, IREG_RESULT);
	}
	return (0);
}

/* Returns -0x100 for timeout, -(drive error code) OR number of result bytes */
static int
send_cmd(struct scd_softc *sc, u_char cmd, u_int nargs, ...)
{
	va_list ap;
	u_char c;
	int rc;
	int i;

	if (waitfor_status_bits(sc, 0, SBIT_BUSY)) {
		device_printf(sc->dev, "drive busy\n");
		return (-0x100);
	}

	XDEBUG(sc, 1, "DEBUG: send_cmd: cmd=0x%x nargs=%d", cmd, nargs);

	SCD_WRITE(sc, OREG_CONTROL, CBIT_RESULT_READY_CLEAR);
	SCD_WRITE(sc, OREG_CONTROL, CBIT_RPARAM_CLEAR);

	for (i = 0; i < 100; i++)
		if (FSTATUS_BIT(sc, FBIT_WPARAM_READY))
			break;
	if (!FSTATUS_BIT(sc, FBIT_WPARAM_READY)) {
		XDEBUG(sc, 1, "\nwparam timeout\n");
		return (-EIO);
	}

	va_start(ap, nargs);
	for (i = 0; i < nargs; i++) {
		c = (u_char)va_arg(ap, int);
		SCD_WRITE(sc, OREG_WPARAMS, c);
		XDEBUG(sc, 1, ",{0x%x}", c);
	}
	va_end(ap);
	XDEBUG(sc, 1, "\n");

	SCD_WRITE(sc, OREG_COMMAND, cmd);

	rc = waitfor_status_bits(sc, SBIT_RESULT_READY, SBIT_BUSY);
	if (rc)
		return (-0x100);

	SCD_WRITE(sc, OREG_CONTROL, CBIT_RESULT_READY_CLEAR);
	switch ((rc = SCD_READ(sc, IREG_RESULT)) & 0xf0) {
	case 0x20:
		rc = SCD_READ(sc, IREG_RESULT);
		/* FALLTHROUGH */
	case 0x50:
		XDEBUG(sc, 1, "DEBUG: send_cmd: drive_error=0x%x\n", rc);
		return (-rc);
	case 0x00:
	default:
		rc = SCD_READ(sc, IREG_RESULT);
		XDEBUG(sc, 1, "DEBUG: send_cmd: result_len=%d\n", rc);
		return (rc);
	}
}

static void
print_error(struct scd_softc *sc, int errcode)
{

	switch (errcode) {
	case -ERR_CD_NOT_LOADED:
		device_printf(sc->dev, "door is open\n");
		break;
	case -ERR_NO_CD_INSIDE:
		device_printf(sc->dev, "no cd inside\n");
		break;
	default:
		if (errcode == -0x100 || errcode > 0)
			device_printf(sc->dev, "device timeout\n");
		else
			device_printf(sc->dev, "unexpected error 0x%x\n", -errcode);
		break;
	}
}

/* Returns 0 or errno value */
static int
waitfor_status_bits(struct scd_softc *sc, int bits_set, int bits_clear)
{
	u_int flags = sc->data.flags;
	u_int max_loop;
	u_char c = 0;

	if (flags & SCDPROBING) {
		max_loop = 0;
		while (max_loop++ < 1000) {
			c = SCD_READ(sc, IREG_STATUS);
			if (c == 0xff)
				return (EIO);
			if (c & SBIT_ATTENTION) {
				process_attention(sc);
				continue;
			}
			if ((c & bits_set) == bits_set &&
			    (c & bits_clear) == 0)
			{
				break;
			}
			DELAY(10000);
		}
	} else {
		max_loop = 100;
		while (max_loop-- > 0) {
			c = SCD_READ(sc, IREG_STATUS);
			if (c & SBIT_ATTENTION) {
				process_attention(sc);
				continue;
			}
			if ((c & bits_set) == bits_set &&
			    (c & bits_clear) == 0)
			{
				break;
			}
			tsleep(waitfor_status_bits, PZERO - 1, "waitfor", hz/10);
		}
	}
	if ((c & bits_set) == bits_set &&
	    (c & bits_clear) == 0)
	{
		return (0);
	}
#ifdef SCD_DEBUG
	if (scd_debuglevel > 0)
		device_printf(sc->dev, "DEBUG: waitfor: TIMEOUT (0x%x,(0x%x,0x%x))\n", c, bits_set, bits_clear);
	else
#endif
		device_printf(sc->dev, "timeout.\n");
	return (EIO);
}

/* these two routines for xcdplayer - "borrowed" from mcd.c */
static int
scd_toc_header (struct scd_softc *sc, struct ioc_toc_header* th)
{
	int rc;

	if (!(sc->data.flags & SCDTOC) && (rc = read_toc(sc)) != 0) {
		print_error(sc, rc);
		return (EIO);
	}

	th->starting_track = sc->data.first_track;
	th->ending_track = sc->data.last_track;
	th->len = 0; /* not used */

	return (0);
}

static int
scd_toc_entrys (struct scd_softc *sc, struct ioc_read_toc_entry *te)
{
	struct cd_toc_entry toc_entry;
	int rc, i, len = te->data_len;

	if (!(sc->data.flags & SCDTOC) && (rc = read_toc(sc)) != 0) {
		print_error(sc, rc);
		return (EIO);
	}

	/* find the toc to copy*/
	i = te->starting_track;
	if (i == SCD_LASTPLUS1)
		i = sc->data.last_track + 1;

	/* verify starting track */
	if (i < sc->data.first_track || i > sc->data.last_track+1)
		return (EINVAL);

	/* valid length ? */
	if (len < sizeof(struct cd_toc_entry)
	    || (len % sizeof(struct cd_toc_entry)) != 0)
		return (EINVAL);

	/* copy the toc data */
	toc_entry.control = sc->data.toc[i].ctl;
	toc_entry.addr_type = te->address_format;
	toc_entry.track = i;
	if (te->address_format == CD_MSF_FORMAT) {
		toc_entry.addr.msf.unused = 0;
		toc_entry.addr.msf.minute = bcd2bin(sc->data.toc[i].start_msf[0]);
		toc_entry.addr.msf.second = bcd2bin(sc->data.toc[i].start_msf[1]);
		toc_entry.addr.msf.frame = bcd2bin(sc->data.toc[i].start_msf[2]);
	}

	/* copy the data back */
	if (copyout(&toc_entry, te->data, sizeof(struct cd_toc_entry)) != 0)
		return (EFAULT);

	return (0);
}


static int
scd_toc_entry (struct scd_softc *sc, struct ioc_read_toc_single_entry *te)
{
	struct cd_toc_entry toc_entry;
	int rc, i;

	if (!(sc->data.flags & SCDTOC) && (rc = read_toc(sc)) != 0) {
		print_error(sc, rc);
		return (EIO);
	}

	/* find the toc to copy*/
	i = te->track;
	if (i == SCD_LASTPLUS1)
		i = sc->data.last_track + 1;

	/* verify starting track */
	if (i < sc->data.first_track || i > sc->data.last_track+1)
		return (EINVAL);

	/* copy the toc data */
	toc_entry.control = sc->data.toc[i].ctl;
	toc_entry.addr_type = te->address_format;
	toc_entry.track = i;
	if (te->address_format == CD_MSF_FORMAT) {
		toc_entry.addr.msf.unused = 0;
		toc_entry.addr.msf.minute = bcd2bin(sc->data.toc[i].start_msf[0]);
		toc_entry.addr.msf.second = bcd2bin(sc->data.toc[i].start_msf[1]);
		toc_entry.addr.msf.frame = bcd2bin(sc->data.toc[i].start_msf[2]);
	}

	/* copy the data back */
	bcopy(&toc_entry, &te->entry, sizeof(struct cd_toc_entry));

	return (0);
}
