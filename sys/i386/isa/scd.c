#include "opt_geom.h"
#ifndef NO_GEOM
#warning "The scd driver is currently incompatible with GEOM"
#else
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

/* Please send any comments to micke@dynas.se */

#define	SCD_DEBUG	0

#include "scd.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/cdio.h>
#include <sys/disklabel.h>
#include <sys/bus.h>

#include <machine/stdarg.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/scdreg.h>


#define scd_part(dev)	((minor(dev)) & 7)
#define scd_unit(dev)	(((minor(dev)) & 0x38) >> 3)
#define scd_phys(dev)	(((minor(dev)) & 0x40) >> 6)
#define RAW_PART        2

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
#  define XDEBUG(level, data) {if (scd_debuglevel >= level) printf data;}
#else
#  define XDEBUG(level, data)
#endif

struct scd_mbx {
	short		unit;
	short		port;
	short		retry;
	short		nblk;
	int		sz;
	u_long		skip;
	struct bio	*bp;
	int		p_offset;
	short		count;
};

static struct scd_data {
	int	iobase;
	char	double_speed;
	char	*name;
	short	flags;
	int	blksize;
	u_long	disksize;
	struct disklabel dlabel;
	int	openflag;
	struct {
		unsigned int  adr :4;
		unsigned int  ctl :4; /* xcdplayer needs this */
		unsigned char start_msf[3];
	} toc[MAX_TRACKS];
	short	first_track;
	short	last_track;
	struct	ioc_play_msf last_play;

	short	audio_status;
	struct bio_queue_head head;		/* head of bio queue */
	struct scd_mbx mbx;
} scd_data[NSCD];

/* prototypes */
static	void	hsg2msf(int hsg, bcd_t *msf);
static	int	msf2hsg(bcd_t *msf);

static void process_attention(unsigned unit);
static __inline void write_control(unsigned port, unsigned data);
static int waitfor_status_bits(int unit, int bits_set, int bits_clear);
static int send_cmd(u_int unit, u_char cmd, u_int nargs, ...);
static void init_drive(unsigned unit);
static int spin_up(unsigned unit);
static int read_toc(unsigned unit);
static int get_result(u_int unit, int result_len, u_char *result);
static void print_error(int unit, int errcode);

static void scd_start(int unit);
static timeout_t scd_timeout;
static void scd_doread(int state, struct scd_mbx *mbxin);

static int scd_eject(int unit);
static int scd_stop(int unit);
static int scd_pause(int unit);
static int scd_resume(int unit);
static int scd_playtracks(int unit, struct ioc_play_track *pt);
static int scd_playmsf(int unit, struct ioc_play_msf *msf);
static int scd_play(int unit, struct ioc_play_msf *msf);
static int scd_subchan(int unit, struct ioc_read_subchannel *sc);
static int read_subcode(int unit, struct sony_subchannel_position_data *sc);

/* for xcdplayer */
static int scd_toc_header(int unit, struct ioc_toc_header *th);
static int scd_toc_entrys(int unit, struct ioc_read_toc_entry *te);
static int scd_toc_entry(int unit, struct ioc_read_toc_single_entry *te);
#define SCD_LASTPLUS1 170 /* don't ask, xcdplayer passes this in */

static int	scd_probe(struct isa_device *dev);
static int	scd_attach(struct isa_device *dev);
struct	isa_driver	scddriver = {
	INTR_TYPE_BIO,
	scd_probe,
	scd_attach,
	"scd"
};
COMPAT_ISA_DRIVER(scd, scddriver);

/* For canceling our timeout */
static struct callout_handle tohandle = CALLOUT_HANDLE_INITIALIZER(&tohanle);

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


static int
scd_attach(struct isa_device *dev)
{
	int	unit = dev->id_unit;
	struct scd_data *cd = scd_data + unit;

	cd->iobase = dev->id_iobase;	/* Already set by probe, but ... */

	/* name filled in probe */
	printf("scd%d: <%s>\n", dev->id_unit, scd_data[dev->id_unit].name);

	init_drive(dev->id_unit);

	cd->flags = SCDINIT;
	cd->audio_status = CD_AS_AUDIO_INVALID;
	bioq_init(&cd->head);

	make_dev(&scd_cdevsw, dkmakeminor(unit, 0, 0),
	    UID_ROOT, GID_OPERATOR, 0640, "scd%da", unit);
	make_dev(&scd_cdevsw, dkmakeminor(unit, 0, RAW_PART),
	    UID_ROOT, GID_OPERATOR, 0640, "scd%dc", unit);
	return 1;
}

static	int
scdopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	int unit,part,phys;
	int rc;
	struct scd_data *cd;

	unit = scd_unit(dev);
	if (unit >= NSCD)
		return ENXIO;

	cd = scd_data + unit;
	part = scd_part(dev);
	phys = scd_phys(dev);

	/* not initialized*/
	if (!(cd->flags & SCDINIT))
		return ENXIO;

	/* invalidated in the meantime? mark all open part's invalid */
	if (cd->openflag)
		return ENXIO;

	XDEBUG(1,("scd%d: DEBUG: status = 0x%x\n", unit, inb(cd->iobase+IREG_STATUS)));

	if ((rc = spin_up(unit)) != 0) {
		print_error(unit, rc);
		return EIO;
	}
	if (!(cd->flags & SCDTOC)) {
		int loop_count = 3;

		while (loop_count-- > 0 && (rc = read_toc(unit)) != 0) {
			if (rc == ERR_NOT_SPINNING) {
				rc = spin_up(unit);
				if (rc) {
					print_error(unit, rc);\
					return EIO;
				}
				continue;
			}
			printf("scd%d: TOC read error 0x%x\n", unit, rc);
			return EIO;
		}
	}

	dev->si_bsize_phys = cd->blksize;

	cd->openflag = 1;
	cd->flags |= SCDVALID;

	return 0;
}

static	int
scdclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	int unit,part,phys;
	struct scd_data *cd;

	unit = scd_unit(dev);
	if (unit >= NSCD)
		return ENXIO;

	cd = scd_data + unit;
	part = scd_part(dev);
	phys = scd_phys(dev);

	if (!(cd->flags & SCDINIT) || !cd->openflag)
		return ENXIO;

	if (cd->audio_status != CD_AS_PLAY_IN_PROGRESS) {
		(void)send_cmd(unit, CMD_SPIN_DOWN, 0);
		cd->flags &= ~SCDSPINNING;
	}


	/* close channel */
	cd->openflag = 0;

	return 0;
}

static	void
scdstrategy(struct bio *bp)
{
	struct scd_data *cd;
	int s;
	int unit = scd_unit(bp->bio_dev);

	cd = scd_data + unit;

	XDEBUG(2, ("scd%d: DEBUG: strategy: block=%ld, bcount=%ld\n",
		unit, (long)bp->bio_blkno, bp->bio_bcount));

	if (unit >= NSCD || bp->bio_blkno < 0 || (bp->bio_bcount % SCDBLKSIZE)) {
		printf("scd%d: strategy failure: blkno = %ld, bcount = %ld\n",
			unit, (long)bp->bio_blkno, bp->bio_bcount);
		bp->bio_error = EINVAL;
		bp->bio_flags |= BIO_ERROR;
		goto bad;
	}

	/* if device invalidated (e.g. media change, door open), error */
	if (!(cd->flags & SCDVALID)) {
		printf("scd%d: media changed\n", unit);
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

	if (!(cd->flags & SCDTOC)) {
		bp->bio_error = EIO;
		goto bad;
	}
	/* adjust transfer if necessary */
	if (bounds_check_with_label(bp,&cd->dlabel,1) <= 0)
		goto done;

	bp->bio_pblkno = bp->bio_blkno;
	bp->bio_resid = 0;

	/* queue it */
	s = splbio();
	bioqdisksort(&cd->head, bp);
	splx(s);

	/* now check whether we can perform processing */
	scd_start(unit);
	return;

bad:
	bp->bio_flags |= BIO_ERROR;
done:
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);
	return;
}

static void
scd_start(int unit)
{
	struct scd_data *cd = scd_data + unit;
	struct bio *bp;
	struct partition *p;
	int s = splbio();

	if (cd->flags & SCDMBXBSY) {
		splx(s);
		return;
	}

	bp = bioq_first(&cd->head);
	if (bp != 0) {
		/* block found to process, dequeue */
		bioq_remove(&cd->head, bp);
		cd->flags |= SCDMBXBSY;
		splx(s);
	} else {
		/* nothing to do */
		splx(s);
		return;
	}

	p = cd->dlabel.d_partitions + scd_part(bp->bio_dev);

	cd->mbx.unit = unit;
	cd->mbx.port = cd->iobase;
	cd->mbx.retry = 3;
	cd->mbx.bp = bp;
	cd->mbx.p_offset = p->p_offset;
	splx(s);

	scd_doread(SCD_S_BEGIN,&(cd->mbx));
	return;
}

static	int
scdioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
	struct scd_data *cd;
	int unit,part;

	unit = scd_unit(dev);
	part = scd_part(dev);
	cd = scd_data + unit;

	XDEBUG(1, ("scd%d: ioctl: cmd=0x%lx\n", unit, cmd));

	if (!(cd->flags & SCDVALID))
		return EIO;

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = cd->dlabel;
		return 0;
	case CDIOCPLAYTRACKS:
		return scd_playtracks(unit, (struct ioc_play_track *) addr);
	case CDIOCPLAYBLOCKS:
		return EINVAL;
	case CDIOCPLAYMSF:
		return scd_playmsf(unit, (struct ioc_play_msf *) addr);
	case CDIOCREADSUBCHANNEL:
		return scd_subchan(unit, (struct ioc_read_subchannel *) addr);
	case CDIOREADTOCHEADER:
		return scd_toc_header (unit, (struct ioc_toc_header *) addr);
	case CDIOREADTOCENTRYS:
		return scd_toc_entrys (unit, (struct ioc_read_toc_entry*) addr);
	case CDIOREADTOCENTRY:
		return scd_toc_entry (unit, (struct ioc_read_toc_single_entry*) addr);
	case CDIOCSETPATCH:
	case CDIOCGETVOL:
	case CDIOCSETVOL:
	case CDIOCSETMONO:
	case CDIOCSETSTERIO:
	case CDIOCSETMUTE:
	case CDIOCSETLEFT:
	case CDIOCSETRIGHT:
		return EINVAL;
	case CDIOCRESUME:
		return scd_resume(unit);
	case CDIOCPAUSE:
		return scd_pause(unit);
	case CDIOCSTART:
		return EINVAL;
	case CDIOCSTOP:
		return scd_stop(unit);
	case CDIOCEJECT:
		return scd_eject(unit);
	case CDIOCALLOW:
		return 0;
	case CDIOCSETDEBUG:
#ifdef SCD_DEBUG
		scd_debuglevel++;
#endif
		return 0;
	case CDIOCCLRDEBUG:
#ifdef SCD_DEBUG
		scd_debuglevel = 0;

#endif
		return 0;
	default:
		printf("scd%d: unsupported ioctl (cmd=0x%lx)\n", unit, cmd);
		return ENOTTY;
	}
}

/***************************************************************
 * lower level of driver starts here
 **************************************************************/

static int
scd_playtracks(int unit, struct ioc_play_track *pt)
{
	struct scd_data *cd = scd_data + unit;
	struct ioc_play_msf msf;
	int a = pt->start_track;
	int z = pt->end_track;
	int rc;

	if (!(cd->flags & SCDTOC) && (rc = read_toc(unit)) != 0) {
		if (rc == -ERR_NOT_SPINNING) {
			if (spin_up(unit) != 0)
				return EIO;
			rc = read_toc(unit);
		}
		if (rc != 0) {
			print_error(unit, rc);
			return EIO;
		}
	}

	XDEBUG(1, ("scd%d: playtracks from %d:%d to %d:%d\n", unit,
		a, pt->start_index, z, pt->end_index));

	if (   a < cd->first_track
	    || a > cd->last_track
	    || a > z
	    || z > cd->last_track)
		return EINVAL;

	bcopy(cd->toc[a].start_msf, &msf.start_m, 3);
	hsg2msf(msf2hsg(cd->toc[z+1].start_msf)-1, &msf.end_m);

	return scd_play(unit, &msf);
}

/* The start/end msf is expected to be in bin format */
static int
scd_playmsf(int unit, struct ioc_play_msf *msfin)
{
	struct ioc_play_msf msf;

	msf.start_m = bin2bcd(msfin->start_m);
	msf.start_s = bin2bcd(msfin->start_s);
	msf.start_f = bin2bcd(msfin->start_f);
	msf.end_m = bin2bcd(msfin->end_m);
	msf.end_s = bin2bcd(msfin->end_s);
	msf.end_f = bin2bcd(msfin->end_f);

	return scd_play(unit, &msf);
}

/* The start/end msf is expected to be in bcd format */
static int
scd_play(int unit, struct ioc_play_msf *msf)
{
	struct scd_data *cd = scd_data + unit;
	int i, rc;

	XDEBUG(1, ("scd%d: playing: %02x:%02x:%02x -> %02x:%02x:%02x\n", unit,
		msf->start_m, msf->start_s, msf->start_f,
		msf->end_m, msf->end_s, msf->end_f));

	for (i = 0; i < 2; i++) {
		rc = send_cmd(unit, CMD_PLAY_AUDIO, 7,
			0x03,
			msf->start_m, msf->start_s, msf->start_f,
			msf->end_m, msf->end_s, msf->end_f);
		if (rc == -ERR_NOT_SPINNING) {
			cd->flags &= ~SCDSPINNING;
			if (spin_up(unit) != 0)
				return EIO;
		} else if (rc < 0) {
			print_error(unit, rc);
			return EIO;
		} else {
			break;
		}
	}
	cd->audio_status = CD_AS_PLAY_IN_PROGRESS;
	bcopy((char *)msf, (char *)&cd->last_play, sizeof(struct ioc_play_msf));
	return 0;
}

static int
scd_stop(int unit)
{
	struct scd_data *cd = scd_data + unit;

	(void)send_cmd(unit, CMD_STOP_AUDIO, 0);
	cd->audio_status = CD_AS_PLAY_COMPLETED;
	return 0;
}

static int
scd_pause(int unit)
{
	struct scd_data *cd = scd_data + unit;
	struct sony_subchannel_position_data subpos;

	if (cd->audio_status != CD_AS_PLAY_IN_PROGRESS)
		return EINVAL;

	if (read_subcode(unit, &subpos) != 0)
		return EIO;

	if (send_cmd(unit, CMD_STOP_AUDIO, 0) != 0)
		return EIO;

	cd->last_play.start_m = subpos.abs_msf[0];
	cd->last_play.start_s = subpos.abs_msf[1];
	cd->last_play.start_f = subpos.abs_msf[2];
	cd->audio_status = CD_AS_PLAY_PAUSED;

	XDEBUG(1, ("scd%d: pause @ %02x:%02x:%02x\n", unit,
		cd->last_play.start_m,
		cd->last_play.start_s,
		cd->last_play.start_f));

	return 0;
}

static int
scd_resume(int unit)
{
	if (scd_data[unit].audio_status != CD_AS_PLAY_PAUSED)
		return EINVAL;
	return scd_play(unit, &scd_data[unit].last_play);
}

static int
scd_eject(int unit)
{
	struct scd_data *cd = scd_data + unit;

	cd->audio_status = CD_AS_AUDIO_INVALID;
	cd->flags &= ~(SCDSPINNING|SCDTOC);

	if (send_cmd(unit, CMD_STOP_AUDIO, 0) != 0 ||
	    send_cmd(unit, CMD_SPIN_DOWN, 0) != 0 ||
	    send_cmd(unit, CMD_EJECT, 0) != 0)
	{
		return EIO;
	}
	return 0;
}

static int
scd_subchan(int unit, struct ioc_read_subchannel *sc)
{
	struct scd_data *cd = scd_data + unit;
	struct sony_subchannel_position_data q;
	struct cd_sub_channel_info data;

	XDEBUG(1, ("scd%d: subchan af=%d, df=%d\n", unit,
		sc->address_format,
		sc->data_format));

	if (sc->address_format != CD_MSF_FORMAT)
		return EINVAL;

	if (sc->data_format != CD_CURRENT_POSITION)
		return EINVAL;

	if (read_subcode(unit, &q) != 0)
		return EIO;

	data.header.audio_status = cd->audio_status;
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

	if (copyout(&data, sc->data, min(sizeof(struct cd_sub_channel_info), sc->data_len))!=0)
		return EFAULT;
	return 0;
}

static __inline void
write_control(unsigned port, unsigned data)
{
	outb(port + OREG_CONTROL, data);
}

static int
scd_probe(struct isa_device *dev)
{
	struct sony_drive_configuration drive_config;
	int unit = dev->id_unit;
	int rc;
	static char namebuf[8+16+8+3];
	char *s = namebuf;
	int loop_count = 0;

	scd_data[unit].flags = SCDPROBING;
	scd_data[unit].iobase = dev->id_iobase;

	bzero(&drive_config, sizeof(drive_config));

again:
	/* Reset drive */
	write_control(dev->id_iobase, CBIT_RESET_DRIVE);

	/* Calm down */
	DELAY(300000);

	/* Only the ATTENTION bit may be set */
	if ((inb(dev->id_iobase+IREG_STATUS) & ~1) != 0) {
		XDEBUG(1, ("scd: too many bits set. probe failed.\n"));
		return 0;
	}
	rc = send_cmd(unit, CMD_GET_DRIVE_CONFIG, 0);
	if (rc != sizeof(drive_config)) {
		/* Sometimes if the drive is playing audio I get */
		/* the bad result 82. Fix by repeating the reset */
		if (rc > 0 && loop_count++ == 0)
			goto again;
		return 0;
	}
	if (get_result(unit, rc, (u_char *)&drive_config) != 0)
		return 0;

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

	scd_data[unit].name = namebuf;

	if (drive_config.config & 0x10)
		scd_data[unit].double_speed = 1;
	else
		scd_data[unit].double_speed = 0;

	return 4;
}

static int
read_subcode(int unit, struct sony_subchannel_position_data *sc)
{
	int rc;

	rc = send_cmd(unit, CMD_GET_SUBCHANNEL_DATA, 0);
	if (rc < 0 || rc < sizeof(*sc))
		return EIO;
	if (get_result(unit, rc, (u_char *)sc) != 0)
		return EIO;
	return 0;
}

/* State machine copied from mcd.c */

/* This (and the code in mcd.c) will not work with more than one drive */
/* because there is only one mbxsave below. Should fix that some day. */
/* (mbxsave & state should probably be included in the scd_data struct and */
/*  the unit number used as first argument to scd_doread().) /Micke */

/* state machine to process read requests
 * initialize with SCD_S_BEGIN: reset state machine
 * SCD_S_WAITSTAT:  wait for ready (!busy)
 * SCD_S_WAITSPIN:  wait for drive to spin up (if not spinning)
 * SCD_S_WAITFIFO:  wait for param fifo to get ready, them exec. command.
 * SCD_S_WAITREAD:  wait for data ready, read data
 * SCD_S_WAITPARAM: wait for command result params, read them, error if bad data read.
 */

static struct scd_mbx *mbxsave;

static void
scd_timeout(void *arg)
{
	scd_doread((int)arg, mbxsave);
}

static void
scd_doread(int state, struct scd_mbx *mbxin)
{
	struct scd_mbx *mbx = (state!=SCD_S_BEGIN) ? mbxsave : mbxin;
	int	unit = mbx->unit;
	int	port = mbx->port;
	struct	bio *bp = mbx->bp;
	struct	scd_data *cd = scd_data + unit;
	int	reg,i;
	int	blknum;
	caddr_t	addr;
	static char sdata[3];	/* Must be preserved between calls to this function */

loop:
	switch (state) {
	case SCD_S_BEGIN:
		mbx = mbxsave = mbxin;

	case SCD_S_BEGIN1:
		/* get status */
		mbx->count = RDELAY_WAIT;

		process_attention(unit);
		goto trystat;

	case SCD_S_WAITSTAT:
		untimeout(scd_timeout,(caddr_t)SCD_S_WAITSTAT, tohandle);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout. drive busy.\n",unit);
			goto harderr;
		}

trystat:
		if (IS_BUSY(port)) {
			tohandle = timeout(scd_timeout,
					   (caddr_t)SCD_S_WAITSTAT,hz/100); /* XXX */
			return;
		}

		process_attention(unit);

		/* reject, if audio active */
		if (cd->audio_status & CD_AS_PLAY_IN_PROGRESS) {
			printf("scd%d: audio is active\n",unit);
			goto harderr;
		}

		mbx->sz = cd->blksize;

		/* for first block */
		mbx->nblk = (bp->bio_bcount + (mbx->sz-1)) / mbx->sz;
		mbx->skip = 0;

nextblock:
		if (!(cd->flags & SCDVALID))
			goto changed;

		blknum 	= (bp->bio_blkno / (mbx->sz/DEV_BSIZE))
			+ mbx->p_offset + mbx->skip/mbx->sz;

		XDEBUG(2, ("scd%d: scd_doread: read blknum=%d\n", unit, blknum));

		/* build parameter block */
		hsg2msf(blknum, sdata);

		write_control(port, CBIT_RESULT_READY_CLEAR);
		write_control(port, CBIT_RPARAM_CLEAR);
		write_control(port, CBIT_DATA_READY_CLEAR);

		if (FSTATUS_BIT(port, FBIT_WPARAM_READY))
			goto writeparam;

		mbx->count = 100;
		tohandle = timeout(scd_timeout,
				   (caddr_t)SCD_S_WAITFIFO,hz/100); /* XXX */
		return;

	case SCD_S_WAITSPIN:
		untimeout(scd_timeout,(caddr_t)SCD_S_WAITSPIN, tohandle);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout waiting for drive to spin up.\n", unit);
			goto harderr;
		}
		if (!STATUS_BIT(port, SBIT_RESULT_READY)) {
			tohandle = timeout(scd_timeout,
					   (caddr_t)SCD_S_WAITSPIN,hz/100); /* XXX */
			return;
		}
		write_control(port, CBIT_RESULT_READY_CLEAR);
		switch ((i = inb(port+IREG_RESULT)) & 0xf0) {
		case 0x20:
			i = inb(port+IREG_RESULT);
			print_error(unit, i);
			goto harderr;
		case 0x00:
			(void)inb(port+IREG_RESULT);
			cd->flags |= SCDSPINNING;
			break;
		}
		XDEBUG(1, ("scd%d: DEBUG: spin up complete\n", unit));

		state = SCD_S_BEGIN1;
		goto loop;

	case SCD_S_WAITFIFO:
		untimeout(scd_timeout,(caddr_t)SCD_S_WAITFIFO, tohandle);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout. write param not ready.\n",unit);
			goto harderr;
		}
		if (!FSTATUS_BIT(port, FBIT_WPARAM_READY)) {
			tohandle = timeout(scd_timeout,
					   (caddr_t)SCD_S_WAITFIFO,hz/100); /* XXX */
			return;
		}
		XDEBUG(1, ("scd%d: mbx->count (writeparamwait) = %d(%d)\n", unit, mbx->count, 100));

writeparam:
		/* The reason this test isn't done 'till now is to make sure */
		/* that it is ok to send the SPIN_UP cmd below. */
		if (!(cd->flags & SCDSPINNING)) {
			XDEBUG(1, ("scd%d: spinning up drive ...\n", unit));
			outb(port+OREG_COMMAND, CMD_SPIN_UP);
			mbx->count = 300;
			tohandle = timeout(scd_timeout,
					   (caddr_t)SCD_S_WAITSPIN,hz/100); /* XXX */
			return;
		}

		reg = port + OREG_WPARAMS;
		/* send the read command */
		disable_intr();
		outb(reg, sdata[0]);
		outb(reg, sdata[1]);
		outb(reg, sdata[2]);
		outb(reg, 0);
		outb(reg, 0);
		outb(reg, 1);
		outb(port+OREG_COMMAND, CMD_READ);
		enable_intr();

		mbx->count = RDELAY_WAITREAD;
		for (i = 0; i < 50; i++) {
			if (STATUS_BIT(port, SBIT_DATA_READY))
				goto got_data;
			DELAY(100);
		}

		tohandle = timeout(scd_timeout,
				   (caddr_t)SCD_S_WAITREAD,hz/100); /* XXX */
		return;

	case SCD_S_WAITREAD:
		untimeout(scd_timeout,(caddr_t)SCD_S_WAITREAD, tohandle);
		if (mbx->count-- <= 0) {
			if (STATUS_BIT(port, SBIT_RESULT_READY))
				goto got_param;
			printf("scd%d: timeout while reading data\n",unit);
			goto readerr;
		}
		if (!STATUS_BIT(port, SBIT_DATA_READY)) {
			process_attention(unit);
			if (!(cd->flags & SCDVALID))
				goto changed;
			tohandle = timeout(scd_timeout,
					   (caddr_t)SCD_S_WAITREAD,hz/100); /* XXX */
			return;
		}
		XDEBUG(2, ("scd%d: mbx->count (after RDY_BIT) = %d(%d)\n", unit, mbx->count, RDELAY_WAITREAD));

got_data:
		/* data is ready */
		addr = bp->bio_data + mbx->skip;
		write_control(port, CBIT_DATA_READY_CLEAR);
		insb(port+IREG_DATA, addr, mbx->sz);

		mbx->count = 100;
		for (i = 0; i < 20; i++) {
			if (STATUS_BIT(port, SBIT_RESULT_READY))
				goto waitfor_param;
			DELAY(100);
		}
		goto waitfor_param;

	case SCD_S_WAITPARAM:
		untimeout(scd_timeout,(caddr_t)SCD_S_WAITPARAM, tohandle);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout waiting for params\n",unit);
			goto readerr;
		}

waitfor_param:
		if (!STATUS_BIT(port, SBIT_RESULT_READY)) {
			tohandle = timeout(scd_timeout,
					   (caddr_t)SCD_S_WAITPARAM,hz/100); /* XXX */
			return;
		}
#if SCD_DEBUG
		if (mbx->count < 100 && scd_debuglevel > 0)
			printf("scd%d: mbx->count (paramwait) = %d(%d)\n", unit, mbx->count, 100);
#endif

got_param:
		write_control(port, CBIT_RESULT_READY_CLEAR);
		switch ((i = inb(port+IREG_RESULT)) & 0xf0) {
		case 0x50:
			switch (i) {
			case ERR_FATAL_READ_ERROR1:
			case ERR_FATAL_READ_ERROR2:
				printf("scd%d: unrecoverable read error 0x%x\n", unit, i);
				goto harderr;
			}
			break;
		case 0x20:
			i = inb(port+IREG_RESULT);
			switch (i) {
			case ERR_NOT_SPINNING:
				XDEBUG(1, ("scd%d: read error: drive not spinning\n", unit));
				if (mbx->retry-- > 0) {
					state = SCD_S_BEGIN1;
					cd->flags &= ~SCDSPINNING;
					goto loop;
				}
				goto harderr;
			default:
				print_error(unit, i);
				goto readerr;
			}
		case 0x00:
			i = inb(port+IREG_RESULT);
			break;
		}

		if (--mbx->nblk > 0) {
			mbx->skip += mbx->sz;
			goto nextblock;
		}

		/* return buffer */
		bp->bio_resid = 0;
		biodone(bp);

		cd->flags &= ~SCDMBXBSY;
		scd_start(mbx->unit);
		return;
	}

readerr:
	if (mbx->retry-- > 0) {
		printf("scd%d: retrying ...\n",unit);
		state = SCD_S_BEGIN1;
		goto loop;
	}
harderr:
	/* invalidate the buffer */
	bp->bio_error = EIO;
	bp->bio_flags |= BIO_ERROR;
	bp->bio_resid = bp->bio_bcount;
	biodone(bp);

	cd->flags &= ~SCDMBXBSY;
	scd_start(mbx->unit);
	return;

changed:
	printf("scd%d: media changed\n", unit);
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
process_attention(unsigned unit)
{
	unsigned port = scd_data[unit].iobase;
	unsigned char code;
	int count = 0;

	while (IS_ATTENTION(port) && count++ < 30) {
		write_control(port, CBIT_ATTENTION_CLEAR);
		code = inb(port+IREG_RESULT);

#if SCD_DEBUG
		if (scd_debuglevel > 0) {
			if (count == 1)
				printf("scd%d: DEBUG: ATTENTIONS = 0x%x", unit, code);
			else
				printf(",0x%x", code);
		}
#endif

		switch (code) {
		case ATTEN_SPIN_DOWN:
			scd_data[unit].flags &= ~SCDSPINNING;
			break;

		case ATTEN_SPIN_UP_DONE:
			scd_data[unit].flags |= SCDSPINNING;
			break;

		case ATTEN_AUDIO_DONE:
			scd_data[unit].audio_status = CD_AS_PLAY_COMPLETED;
			break;

		case ATTEN_DRIVE_LOADED:
			scd_data[unit].flags &= ~(SCDTOC|SCDSPINNING|SCDVALID);
			scd_data[unit].audio_status = CD_AS_AUDIO_INVALID;
			break;

		case ATTEN_EJECT_PUSHED:
			scd_data[unit].flags &= ~SCDVALID;
			break;
		}
		DELAY(100);
	}
#if SCD_DEBUG
	if (scd_debuglevel > 0 && count > 0)
		printf("\n");
#endif
}

/* Returns 0 OR sony error code */
static int
spin_up(unsigned unit)
{
	unsigned char res_reg[12];
	unsigned int res_size;
	int rc;
	int loop_count = 0;

again:
	rc = send_cmd(unit, CMD_SPIN_UP, 0, 0, res_reg, &res_size);
	if (rc != 0) {
		XDEBUG(2, ("scd%d: CMD_SPIN_UP error 0x%x\n", unit, rc));
		return rc;
	}

	if (!(scd_data[unit].flags & SCDTOC)) {
		rc = send_cmd(unit, CMD_READ_TOC, 0);
		if (rc == ERR_NOT_SPINNING) {
			if (loop_count++ < 3)
				goto again;
			return rc;
		}
		if (rc != 0)
			return rc;
	}

	scd_data[unit].flags |= SCDSPINNING;

	return 0;
}

static struct sony_tracklist *
get_tl(struct sony_toc *toc, int size)
{
	struct sony_tracklist *tl = &toc->tracks[0];

	if (tl->track != 0xb0)
		return tl;
	(char *)tl += 9;
	if (tl->track != 0xb1)
		return tl;
	(char *)tl += 9;
	if (tl->track != 0xb2)
		return tl;
	(char *)tl += 9;
	if (tl->track != 0xb3)
		return tl;
	(char *)tl += 9;
	if (tl->track != 0xb4)
		return tl;
	(char *)tl += 9;
	if (tl->track != 0xc0)
		return tl;
	(char *)tl += 9;
	return tl;
}

static int
read_toc(unsigned unit)
{
	struct scd_data *cd;
	unsigned part = 0;	/* For now ... */
	struct sony_toc toc;
	struct sony_tracklist *tl;
	int rc, i, j;
	u_long first, last;

	cd = scd_data + unit;

	rc = send_cmd(unit, CMD_GET_TOC, 1, part+1);
	if (rc < 0)
		return rc;
	if (rc > sizeof(toc)) {
		printf("scd%d: program error: toc too large (%d)\n", unit, rc);
		return EIO;
	}
	if (get_result(unit, rc, (u_char *)&toc) != 0)
		return EIO;

	XDEBUG(1, ("scd%d: toc read. len = %d, sizeof(toc) = %d\n", unit, rc, sizeof(toc)));

	tl = get_tl(&toc, rc);
	first = msf2hsg(tl->start_msf);
	last = msf2hsg(toc.lead_out_start_msf);
	cd->blksize = SCDBLKSIZE;
	cd->disksize = last*cd->blksize/DEV_BSIZE;

	XDEBUG(1, ("scd%d: firstsector = %ld, lastsector = %ld", unit,
			first, last));

	cd->first_track = bcd2bin(toc.first_track);
	cd->last_track = bcd2bin(toc.last_track);
	if (cd->last_track > (MAX_TRACKS-2))
		cd->last_track = MAX_TRACKS-2;
	for (j = 0, i = cd->first_track; i <= cd->last_track; i++, j++) {
		cd->toc[i].adr = tl[j].adr;
		cd->toc[i].ctl = tl[j].ctl; /* for xcdplayer */
		bcopy(tl[j].start_msf, cd->toc[i].start_msf, 3);
#ifdef SCD_DEBUG
		if (scd_debuglevel > 0) {
			if ((j % 3) == 0)
				printf("\nscd%d: tracks ", unit);
			printf("[%03d: %2d %2d %2d]  ", i,
				bcd2bin(cd->toc[i].start_msf[0]),
				bcd2bin(cd->toc[i].start_msf[1]),
				bcd2bin(cd->toc[i].start_msf[2]));
		}
#endif
	}
	bcopy(toc.lead_out_start_msf, cd->toc[cd->last_track+1].start_msf, 3);
#ifdef SCD_DEBUG
	if (scd_debuglevel > 0) {
		i = cd->last_track+1;
		printf("[END: %2d %2d %2d]\n",
			bcd2bin(cd->toc[i].start_msf[0]),
			bcd2bin(cd->toc[i].start_msf[1]),
			bcd2bin(cd->toc[i].start_msf[2]));
	}
#endif

	bzero(&cd->dlabel,sizeof(struct disklabel));
	/* filled with spaces first */
	strncpy(cd->dlabel.d_typename,"               ",
		sizeof(cd->dlabel.d_typename));
	strncpy(cd->dlabel.d_typename, cd->name,
		min(strlen(cd->name), sizeof(cd->dlabel.d_typename) - 1));
	strncpy(cd->dlabel.d_packname,"unknown        ",
		sizeof(cd->dlabel.d_packname));
	cd->dlabel.d_secsize 	= cd->blksize;
	cd->dlabel.d_nsectors	= 100;
	cd->dlabel.d_ntracks	= 1;
	cd->dlabel.d_ncylinders	= (cd->disksize/100)+1;
	cd->dlabel.d_secpercyl	= 100;
	cd->dlabel.d_secperunit	= cd->disksize;
	cd->dlabel.d_rpm	= 300;
	cd->dlabel.d_interleave	= 1;
	cd->dlabel.d_flags	= D_REMOVABLE;
	cd->dlabel.d_npartitions= 1;
	cd->dlabel.d_partitions[0].p_offset = 0;
	cd->dlabel.d_partitions[0].p_size = cd->disksize;
	cd->dlabel.d_partitions[0].p_fstype = 9;
	cd->dlabel.d_magic	= DISKMAGIC;
	cd->dlabel.d_magic2	= DISKMAGIC;
	cd->dlabel.d_checksum	= dkcksum(&cd->dlabel);

	cd->flags |= SCDTOC;

	return 0;
}

static void
init_drive(unsigned unit)
{
	int rc;

	rc = send_cmd(unit, CMD_SET_DRIVE_PARAM, 2,
		0x05, 0x03 | ((scd_data[unit].double_speed) ? 0x04: 0));
	if (rc != 0)
		printf("scd%d: Unable to set parameters. Errcode = 0x%x\n", unit, rc);
}

/* Returns 0 or errno */
static int
get_result(u_int unit, int result_len, u_char *result)
{
	unsigned int port = scd_data[unit].iobase;
	unsigned int res_reg = port + IREG_RESULT;
	int loop_index = 2; /* send_cmd() reads two bytes ... */

	XDEBUG(1, ("scd%d: DEBUG: get_result: bytes=%d\n", unit, result_len));

	while (result_len-- > 0) {
		if (loop_index++ >= 10) {
			loop_index = 1;
			if (waitfor_status_bits(unit, SBIT_RESULT_READY, 0))
				return EIO;
			write_control(port, CBIT_RESULT_READY_CLEAR);
		}
		if (result)
			*result++ = inb(res_reg);
		else
			(void)inb(res_reg);
	}
	return 0;
}

/* Returns -0x100 for timeout, -(drive error code) OR number of result bytes */
static int
send_cmd(u_int unit, u_char cmd, u_int nargs, ...)
{
	va_list ap;
	u_int port = scd_data[unit].iobase;
	u_int reg;
	u_char c;
	int rc;
	int i;

	if (waitfor_status_bits(unit, 0, SBIT_BUSY)) {
		printf("scd%d: drive busy\n", unit);
		return -0x100;
	}

	XDEBUG(1,("scd%d: DEBUG: send_cmd: cmd=0x%x nargs=%d", unit, cmd, nargs));

	write_control(port, CBIT_RESULT_READY_CLEAR);
	write_control(port, CBIT_RPARAM_CLEAR);

	for (i = 0; i < 100; i++)
		if (FSTATUS_BIT(port, FBIT_WPARAM_READY))
			break;
	if (!FSTATUS_BIT(port, FBIT_WPARAM_READY)) {
		XDEBUG(1, ("\nscd%d: wparam timeout\n", unit));
		return -EIO;
	}

	va_start(ap, nargs);
	reg = port + OREG_WPARAMS;
	for (i = 0; i < nargs; i++) {
		c = (u_char)va_arg(ap, int);
		outb(reg, c);
		XDEBUG(1, (",{0x%x}", c));
	}
	va_end(ap);
	XDEBUG(1, ("\n"));

	outb(port+OREG_COMMAND, cmd);

	rc = waitfor_status_bits(unit, SBIT_RESULT_READY, SBIT_BUSY);
	if (rc)
		return -0x100;

	reg = port + IREG_RESULT;
	write_control(port, CBIT_RESULT_READY_CLEAR);
	switch ((rc = inb(reg)) & 0xf0) {
	case 0x20:
		rc = inb(reg);
		/* FALLTHROUGH */
	case 0x50:
		XDEBUG(1, ("scd%d: DEBUG: send_cmd: drive_error=0x%x\n", unit, rc));
		return -rc;
	case 0x00:
	default:
		rc = inb(reg);
		XDEBUG(1, ("scd%d: DEBUG: send_cmd: result_len=%d\n", unit, rc));
		return rc;
	}
}

static void
print_error(int unit, int errcode)
{
	switch (errcode) {
	case -ERR_CD_NOT_LOADED:
		printf("scd%d: door is open\n", unit);
		break;
	case -ERR_NO_CD_INSIDE:
		printf("scd%d: no cd inside\n", unit);
		break;
	default:
		if (errcode == -0x100 || errcode > 0)
			printf("scd%d: device timeout\n", unit);
		else
			printf("scd%d: unexpected error 0x%x\n", unit, -errcode);
		break;
	}
}

/* Returns 0 or errno value */
static int
waitfor_status_bits(int unit, int bits_set, int bits_clear)
{
	u_int port = scd_data[unit].iobase;
	u_int flags = scd_data[unit].flags;
	u_int reg = port + IREG_STATUS;
	u_int max_loop;
	u_char c = 0;

	if (flags & SCDPROBING) {
		max_loop = 0;
		while (max_loop++ < 1000) {
			c = inb(reg);
			if (c == 0xff)
				return EIO;
			if (c & SBIT_ATTENTION) {
				process_attention(unit);
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
			c = inb(reg);
			if (c & SBIT_ATTENTION) {
				process_attention(unit);
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
		return 0;
	}
#ifdef SCD_DEBUG
	if (scd_debuglevel > 0)
		printf("scd%d: DEBUG: waitfor: TIMEOUT (0x%x,(0x%x,0x%x))\n", unit, c, bits_set, bits_clear);
	else
#endif
		printf("scd%d: timeout.\n", unit);
	return EIO;
}

/* these two routines for xcdplayer - "borrowed" from mcd.c */
static int
scd_toc_header (int unit, struct ioc_toc_header* th)
{
	struct scd_data *cd = scd_data + unit;
	int rc;

	if (!(cd->flags & SCDTOC) && (rc = read_toc(unit)) != 0) {
		print_error(unit, rc);
		return EIO;
	}

	th->starting_track = cd->first_track;
	th->ending_track = cd->last_track;
	th->len = 0; /* not used */

	return 0;
}

static int
scd_toc_entrys (int unit, struct ioc_read_toc_entry *te)
{
	struct scd_data *cd = scd_data + unit;
	struct cd_toc_entry toc_entry;
	int rc, i, len = te->data_len;

	if (!(cd->flags & SCDTOC) && (rc = read_toc(unit)) != 0) {
		print_error(unit, rc);
		return EIO;
	}

	/* find the toc to copy*/
	i = te->starting_track;
	if (i == SCD_LASTPLUS1)
		i = cd->last_track + 1;

	/* verify starting track */
	if (i < cd->first_track || i > cd->last_track+1)
		return EINVAL;

	/* valid length ? */
	if (len < sizeof(struct cd_toc_entry)
	    || (len % sizeof(struct cd_toc_entry)) != 0)
		return EINVAL;

	/* copy the toc data */
	toc_entry.control = cd->toc[i].ctl;
	toc_entry.addr_type = te->address_format;
	toc_entry.track = i;
	if (te->address_format == CD_MSF_FORMAT) {
		toc_entry.addr.msf.unused = 0;
		toc_entry.addr.msf.minute = bcd2bin(cd->toc[i].start_msf[0]);
		toc_entry.addr.msf.second = bcd2bin(cd->toc[i].start_msf[1]);
		toc_entry.addr.msf.frame = bcd2bin(cd->toc[i].start_msf[2]);
	}

	/* copy the data back */
	if (copyout(&toc_entry, te->data, sizeof(struct cd_toc_entry)) != 0)
		return EFAULT;

	return 0;
}


static int
scd_toc_entry (int unit, struct ioc_read_toc_single_entry *te)
{
	struct scd_data *cd = scd_data + unit;
	struct cd_toc_entry toc_entry;
	int rc, i;

	if (!(cd->flags & SCDTOC) && (rc = read_toc(unit)) != 0) {
		print_error(unit, rc);
		return EIO;
	}

	/* find the toc to copy*/
	i = te->track;
	if (i == SCD_LASTPLUS1)
		i = cd->last_track + 1;

	/* verify starting track */
	if (i < cd->first_track || i > cd->last_track+1)
		return EINVAL;

	/* copy the toc data */
	toc_entry.control = cd->toc[i].ctl;
	toc_entry.addr_type = te->address_format;
	toc_entry.track = i;
	if (te->address_format == CD_MSF_FORMAT) {
		toc_entry.addr.msf.unused = 0;
		toc_entry.addr.msf.minute = bcd2bin(cd->toc[i].start_msf[0]);
		toc_entry.addr.msf.second = bcd2bin(cd->toc[i].start_msf[1]);
		toc_entry.addr.msf.frame = bcd2bin(cd->toc[i].start_msf[2]);
	}

	/* copy the data back */
	bcopy(&toc_entry, &te->entry, sizeof(struct cd_toc_entry));

	return 0;
}
#endif
