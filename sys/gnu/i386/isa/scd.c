/*-
 * Copyright (c) 1995 Mikael Hybsch
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
 * The Linux driver cdu31a has been used as a reference when writing this
 * code, there fore bringing it under the GNU Public License.  The following
 * conditions of redistribution therefore apply:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


/* $Id: scd.c,v 1.4 1995/01/28 23:53:16 micke Exp micke $ */

/* Please send any comments to micke@dynas.se */

#define	SCD_DEBUG	0

#include "scd.h"
#if NSCD > 0
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include <sys/errno.h>
#include <sys/dkbad.h>
#include <sys/disklabel.h>
#include <sys/devconf.h>
#include <machine/stdarg.h>

#include <i386/isa/isa.h>
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
   int scd_debuglevel = SCD_DEBUG;
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
	struct buf	*bp;
	int		p_offset;
	short		count;
};

struct scd_data {
	int	iobase;
	char	double_speed;
	char	*name;
	short	flags;
	int	blksize;
	u_long	disksize;
	struct disklabel dlabel;
	int	openflag;
	struct {
		unsigned char start_msf[3];
	} toc[MAX_TRACKS];
	short	first_track;
	short	last_track;
	struct	ioc_play_msf last_play;
	
	short	audio_status;
	struct buf head;		/* head of buf queue */
	struct scd_mbx mbx;
} scd_data[NSCD];

/* prototypes */
int	scdopen(dev_t dev);
int	scdclose(dev_t dev);
void	scdstrategy(struct buf *bp);
int	scdioctl(dev_t dev, int cmd, caddr_t addr, int flags);
int	scdsize(dev_t dev);

static	int	bcd2bin(bcd_t b);
static	bcd_t	bin2bcd(int b);
static	void	hsg2msf(int hsg, bcd_t *msf);
static	int	msf2hsg(bcd_t *msf);

static void process_attention(unsigned unit);
static inline void write_control(unsigned port, unsigned data);
static int waitfor_status_bits(int unit, int bits_set, int bits_clear);
static int waitfor_attention(int unit);
static int send_cmd(u_int unit, u_char cmd, u_int nargs, ...);
static void init_drive(unsigned unit);
static int spin_up(unsigned unit);
static int read_toc(dev_t dev);
static int get_result(u_int unit, int result_len, u_char *result);
static void print_error(int unit, int errcode);

static void scd_start(int unit);
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

extern	int	hz;

int	scd_probe(struct isa_device *dev);
int	scd_attach(struct isa_device *dev);
struct	isa_driver	scddriver = { scd_probe, scd_attach, "scd" };

static struct kern_devconf kdc_scd[NSCD] = { {
	0, 0, 0,		/* filled in by dev_attach */
	"scd", 0, { MDDT_ISA, 0, "bio" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_IDLE,		/* status */
	"Sony CD-ROM drive"     /* properly filled later */
} };

static inline void
scd_registerdev(struct isa_device *id)
{
	if(id->id_unit)
		kdc_scd[id->id_unit] = kdc_scd[0];
	kdc_scd[id->id_unit].kdc_unit = id->id_unit;
	kdc_scd[id->id_unit].kdc_isa = id;
	dev_attach(&kdc_scd[id->id_unit]);
}

int scd_attach(struct isa_device *dev)
{
	struct scd_data *cd = scd_data + dev->id_unit;
	int i;
	
	cd->iobase = dev->id_iobase;	/* Already set by probe, but ... */

	scd_registerdev(dev);
	/* name filled in probe */
	kdc_scd[dev->id_unit].kdc_description = scd_data[dev->id_unit].name;
	printf("scd%d: <%s>\n", dev->id_unit, scd_data[dev->id_unit].name);

	init_drive(dev->id_unit);

	cd->flags = SCDINIT;
	cd->audio_status = CD_AS_AUDIO_INVALID;

	return 1;
}

int
scdopen(dev_t dev)
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

		while (loop_count-- > 0 && (rc = read_toc(dev)) != 0) {
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

	cd->openflag = 1;
	cd->flags |= SCDVALID;
	kdc_scd[unit].kdc_state = DC_BUSY;

	return 0;
}

int
scdclose(dev_t dev)
{
	int unit,part,phys;
	struct scd_data *cd;
	int rlen;
	char rdata[10];
	
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

	kdc_scd[unit].kdc_state = DC_IDLE;

	/* close channel */
	cd->openflag = 0;

	return 0;
}

void
scdstrategy(struct buf *bp)
{
	struct scd_data *cd;
	struct buf *qp;
	int s;
	int unit = scd_unit(bp->b_dev);

	cd = scd_data + unit;

	XDEBUG(2, ("scd%d: DEBUG: strategy: block=%ld, bcount=%ld\n", unit, bp->b_blkno, bp->b_bcount));

	if (unit >= NSCD || bp->b_blkno < 0 || (bp->b_bcount % SCDBLKSIZE)) {
		printf("scd%d: strategy failure: blkno = %d, bcount = %d\n",
			unit, bp->b_blkno, bp->b_bcount);
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto bad;
	}

	/* if device invalidated (e.g. media change, door open), error */
	if (!(cd->flags & SCDVALID)) {
		printf("scd%d: media changed\n", unit);
		bp->b_error = EIO;
		goto bad;
	}

	/* read only */
	if (!(bp->b_flags & B_READ)) {
		bp->b_error = EROFS;
		goto bad;
	}
	
	/* no data to read */
	if (bp->b_bcount == 0)
		goto done;
	
	if (!(cd->flags & SCDTOC)) {
		bp->b_error = EIO;
		goto bad;
	}
	/* adjust transfer if necessary */
	if (bounds_check_with_label(bp,&cd->dlabel,1) <= 0)
		goto done;

	bp->b_pblkno = bp->b_blkno;
	bp->b_resid = 0;
	
	/* queue it */
	qp = &cd->head;
	s = splbio();
	disksort(qp,bp);
	splx(s);
	
	/* now check whether we can perform processing */
	scd_start(unit);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}

static void
scd_start(int unit)
{
	struct scd_data *cd = scd_data + unit;
	struct buf *bp, *qp = &cd->head;
	struct partition *p;
	int part;
	register s = splbio();
	
	if (cd->flags & SCDMBXBSY) {
		splx(s);
		return;
	}

	if ((bp = qp->b_actf) != 0) {
		/* block found to process, dequeue */
		qp->b_actf = bp->b_actf;
		cd->flags |= SCDMBXBSY;
		splx(s);
	} else {
		/* nothing to do */
		splx(s);
		return;
	}

	p = cd->dlabel.d_partitions + scd_part(bp->b_dev);

	cd->mbx.unit = unit;
	cd->mbx.port = cd->iobase;
	cd->mbx.retry = 3;
	cd->mbx.bp = bp;
	cd->mbx.p_offset = p->p_offset;
	splx(s);

	scd_doread(SCD_S_BEGIN,&(cd->mbx));
	return;
}

int
scdioctl(dev_t dev, int cmd, caddr_t addr, int flags)
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
	case DIOCSBAD:
		return EINVAL;
	case DIOCGDINFO:
		*(struct disklabel *)addr = cd->dlabel;
		return 0;
	case DIOCGPART:
		((struct partinfo *)addr)->disklab = &cd->dlabel;
		((struct partinfo *)addr)->part =
		    &cd->dlabel.d_partitions[0];
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
	case CDIOREADTOCENTRYS:
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

int
scdsize(dev_t dev)
{
	return -1;
}

void
scdintr()
{
	return;
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
	int rc, i;

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
	int port = cd->iobase;

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

int
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
scd_doread(int state, struct scd_mbx *mbxin)
{
	struct scd_mbx *mbx = (state!=SCD_S_BEGIN) ? mbxsave : mbxin;
	int	unit = mbx->unit;
	int	port = mbx->port;
	struct	buf *bp = mbx->bp;
	struct	scd_data *cd = scd_data + unit;
	int	reg,i,k,c;
	int	blknum;
	caddr_t	addr;
	char	rdata[10];
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
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITSTAT);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout. drive busy.\n",unit);
			goto harderr;
		}

trystat:
		if (IS_BUSY(port)) {
			timeout((timeout_func_t)scd_doread,
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

firstblock:
		/* for first block */
		mbx->nblk = (bp->b_bcount + (mbx->sz-1)) / mbx->sz;
		mbx->skip = 0;

nextblock:
		if (!(cd->flags & SCDVALID))
			goto changed;

		blknum 	= (bp->b_blkno / (mbx->sz/DEV_BSIZE))
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
		timeout((timeout_func_t)scd_doread,
			(caddr_t)SCD_S_WAITFIFO,hz/100); /* XXX */
		return;

	case SCD_S_WAITSPIN:
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITSPIN);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout waiting for drive to spin up.\n", unit);
			goto harderr;
		}
		if (!STATUS_BIT(port, SBIT_RESULT_READY)) {
			timeout((timeout_func_t)scd_doread,
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
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITFIFO);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout. write param not ready.\n",unit);
			goto harderr;
		}
		if (!FSTATUS_BIT(port, FBIT_WPARAM_READY)) {
			timeout((timeout_func_t)scd_doread,
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
			timeout((timeout_func_t)scd_doread,
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

		timeout((timeout_func_t)scd_doread,
			(caddr_t)SCD_S_WAITREAD,hz/100); /* XXX */
		return;

	case SCD_S_WAITREAD:
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITREAD);
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
			timeout((timeout_func_t)scd_doread,
				(caddr_t)SCD_S_WAITREAD,hz/100); /* XXX */
			return;
		}
		XDEBUG(2, ("scd%d: mbx->count (after RDY_BIT) = %d(%d)\n", unit, mbx->count, RDELAY_WAITREAD));

got_data:
		/* data is ready */
		addr = bp->b_un.b_addr + mbx->skip;
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
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITPARAM);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout waiting for params\n",unit);
			goto readerr;
		}

waitfor_param:
		if (!STATUS_BIT(port, SBIT_RESULT_READY)) {
			timeout((timeout_func_t)scd_doread,
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
		bp->b_resid = 0;
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
	bp->b_error = EIO;
	bp->b_flags |= B_ERROR;
	bp->b_resid = bp->b_bcount;
	biodone(bp);

	cd->flags &= ~SCDMBXBSY;
	scd_start(mbx->unit);
	return;

changed:
	printf("scd%d: media changed\n", unit);
	goto harderr;
}

static int
bcd2bin(bcd_t b)
{
	return (b >> 4) * 10 + (b & 15);
}

static bcd_t
bin2bcd(int b)
{
	return ((b / 10) << 4) | (b % 10);
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
	int i;

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
	rc = send_cmd(unit, CMD_SPIN_UP, NULL, 0, res_reg, &res_size);
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
read_toc(dev_t dev)
{
	unsigned unit;
	struct scd_data *cd;
	unsigned part = 0;	/* For now ... */
	struct sony_toc toc;
	struct sony_tracklist *tl;
	int rc, i, j;
	u_long first, last;

	unit = scd_unit(dev);
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

	XDEBUG(1, ("scd%d: firstsector = %d, lastsector = %d", unit,
			first, last));

	cd->first_track = bcd2bin(toc.first_track);
	cd->last_track = bcd2bin(toc.last_track);
	if (cd->last_track > (MAX_TRACKS-2))
		cd->last_track = MAX_TRACKS-2;
	for (j = 0, i = cd->first_track; i <= cd->last_track; i++, j++) {
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

static inline void
write_control(unsigned port, unsigned data)
{
	outb(port + OREG_CONTROL, data);
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
	unsigned char c;
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

	if (rc = waitfor_status_bits(unit, SBIT_RESULT_READY, SBIT_BUSY))
		return -0x100;

	reg = port + IREG_RESULT;
	write_control(port, CBIT_RESULT_READY_CLEAR);
	switch ((rc = inb(reg)) & 0xf0) {
	case 0x20:
		rc = inb(reg);
		/* FALL TROUGH */
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
#endif /* NSCD > 0 */
