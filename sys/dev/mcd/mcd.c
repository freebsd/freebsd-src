/*
 * Copyright 1993 by Holger Veit (data part)
 * Copyright 1993 by Brian Moore (audio part)
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
 *      for use with "386BSD" and similar operating systems.
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
 *	$Id: mcd.c,v 1.3 1993/11/25 01:31:43 wollman Exp $
 */
static char COPYRIGHT[] = "mcd-driver (C)1993 by H.Veit & B.Moore";

#include "mcd.h"
#if NMCD > 0
#include "types.h"
#include "param.h"
#include "systm.h"
#include "conf.h"
#include "file.h"
#include "buf.h"
#include "stat.h"
#include "uio.h"
#include "ioctl.h"
#include "cdio.h"
#include "errno.h"
#include "dkbad.h"
#include "disklabel.h"
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "mcdreg.h"

/* user definable options */
/*#define MCD_TO_WARNING_ON*/	/* define to get timeout messages */
/*#define MCDMINI*/ 		/* define for a mini configuration for boot kernel */


#ifdef MCDMINI
#define MCD_TRACE(fmt,a,b,c,d)
#ifdef MCD_TO_WARNING_ON
#undef MCD_TO_WARNING_ON
#endif
#else
#define MCD_TRACE(fmt,a,b,c,xd)	{if (mcd_data[unit].debug) {printf("mcd%d st=%02x: ",unit,mcd_data[unit].status); printf(fmt,a,b,c,xd);}}
#endif

#define mcd_part(dev)	((minor(dev)) & 7)
#define mcd_unit(dev)	(((minor(dev)) & 0x38) >> 3)
#define mcd_phys(dev)	(((minor(dev)) & 0x40) >> 6)
#define RAW_PART	3

/* flags */
#define MCDOPEN		0x0001	/* device opened */
#define MCDVALID	0x0002	/* parameters loaded */
#define MCDINIT		0x0004	/* device is init'd */
#define MCDWAIT		0x0008	/* waiting for something */
#define MCDLABEL	0x0010	/* label is read */
#define	MCDPROBING	0x0020	/* probing */
#define	MCDREADRAW	0x0040	/* read raw mode (2352 bytes) */
#define	MCDVOLINFO	0x0080	/* already read volinfo */
#define	MCDTOC		0x0100	/* already read toc */
#define	MCDMBXBSY	0x0200	/* local mbx is busy */

/* status */
#define	MCDAUDIOBSY	MCD_ST_AUDIOBSY		/* playing audio */
#define MCDDSKCHNG	MCD_ST_DSKCHNG		/* sensed change of disk */
#define MCDDSKIN	MCD_ST_DSKIN		/* sensed disk in drive */
#define MCDDOOROPEN	MCD_ST_DOOROPEN		/* sensed door open */

/* toc */
#define MCD_MAXTOCS	104	/* from the Linux driver */
#define MCD_LASTPLUS1	170	/* special toc entry */

struct mcd_mbx {
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

struct mcd_data {
	short	config;
	short	flags;
	short	status;
	int	blksize;
	u_long	disksize;
	int	iobase;
	struct disklabel dlabel;
	int	partflags[MAXPARTITIONS];
	int	openflags;
	struct mcd_volinfo volinfo;
#ifndef MCDMINI
	struct mcd_qchninfo toc[MCD_MAXTOCS];
	short	audio_status;
	struct mcd_read2 lastpb;
#endif
	short	debug;
	struct buf head;		/* head of buf queue */
	struct mcd_mbx mbx;
} mcd_data[NMCD];

/* reader state machine */
#define MCD_S_BEGIN	0
#define MCD_S_BEGIN1	1
#define MCD_S_WAITSTAT	2
#define MCD_S_WAITMODE	3
#define MCD_S_WAITREAD	4

/* prototypes */
int	mcdopen(dev_t dev);
int	mcdclose(dev_t dev);
void	mcdstrategy(struct buf *bp);
int	mcdioctl(dev_t dev, int cmd, caddr_t addr, int flags);
int	mcdsize(dev_t dev);
static	void	mcd_done(struct mcd_mbx *mbx);
static	void	mcd_start(int unit);
static	int	mcd_getdisklabel(int unit);
static	void	mcd_configure(struct mcd_data *cd);
static	int	mcd_get(int unit, char *buf, int nmax);
static	void	mcd_setflags(int unit,struct mcd_data *cd);
static	int	mcd_getstat(int unit,int sflg);
static	int	mcd_send(int unit, int cmd,int nretrys);
static	int	bcd2bin(bcd_t b);
static	bcd_t	bin2bcd(int b);
static	void	hsg2msf(int hsg, bcd_t *msf);
static	int	msf2hsg(bcd_t *msf);
static	int	mcd_volinfo(int unit);
static	int	mcd_waitrdy(int port,int dly);
static 	void	mcd_doread(caddr_t, int);
#ifndef MCDMINI
static	int 	mcd_setmode(int unit, int mode);
static	int	mcd_getqchan(int unit, struct mcd_qchninfo *q);
static	int	mcd_subchan(int unit, struct ioc_read_subchannel *sc);
static	int	mcd_toc_header(int unit, struct ioc_toc_header *th);
static	int	mcd_read_toc(int unit);
static	int	mcd_toc_entry(int unit, struct ioc_read_toc_entry *te);
static	int	mcd_stop(int unit);
static	int	mcd_playtracks(int unit, struct ioc_play_track *pt);
static	int	mcd_play(int unit, struct mcd_read2 *pb);
static	int	mcd_pause(int unit);
static	int	mcd_resume(int unit);
#endif

extern	int	hz;
extern	int	mcd_probe(struct isa_device *dev);
extern	int	mcd_attach(struct isa_device *dev);
struct	isa_driver	mcddriver = { mcd_probe, mcd_attach, "mcd" };

#define mcd_put(port,byte)	outb(port,byte)

#define MCD_RETRYS	5
#define MCD_RDRETRYS	8

#define MCDBLK	2048	/* for cooked mode */
#define MCDRBLK	2352	/* for raw mode */

/* several delays */
#define RDELAY_WAITSTAT	300
#define RDELAY_WAITMODE	300
#define RDELAY_WAITREAD	800

#define DELAY_STATUS	10000l		/* 10000 * 1us */
#define DELAY_GETREPLY	200000l		/* 200000 * 2us */
#define DELAY_SEEKREAD	20000l		/* 20000 * 1us */
#define mcd_delay	DELAY

int mcd_attach(struct isa_device *dev)
{
	struct mcd_data *cd = mcd_data + dev->id_unit;
	int i;
	
	cd->iobase = dev->id_iobase;
	cd->flags |= MCDINIT;
	cd->openflags = 0;
	for (i=0; i<MAXPARTITIONS; i++) cd->partflags[i] = 0;

#ifdef NOTYET
	/* wire controller for interrupts and dma */
	mcd_configure(cd);
#endif

	return 1;
}

int mcdopen(dev_t dev)
{
	int unit,part,phys;
	struct mcd_data *cd;
	
	unit = mcd_unit(dev);
	if (unit >= NMCD)
		return ENXIO;

	cd = mcd_data + unit;
	part = mcd_part(dev);
	phys = mcd_phys(dev);
	
	/* not initialized*/
	if (!(cd->flags & MCDINIT))
		return ENXIO;

	/* invalidated in the meantime? mark all open part's invalid */
	if (!(cd->flags & MCDVALID) && cd->openflags)
		return ENXIO;

	if (mcd_getstat(unit,1) < 0)
		return ENXIO;

	/* XXX get a default disklabel */
	mcd_getdisklabel(unit);

	if (mcdsize(dev) < 0) {
		printf("mcd%d: failed to get disk size\n",unit);
		return ENXIO;
	} else
		cd->flags |= MCDVALID;

MCD_TRACE("open: partition=%d, disksize = %d, blksize=%d\n",
	part,cd->disksize,cd->blksize,0);

	if (part == RAW_PART ||
		(part < cd->dlabel.d_npartitions &&
		cd->dlabel.d_partitions[part].p_fstype != FS_UNUSED)) {
		cd->partflags[part] |= MCDOPEN;
		cd->openflags |= (1<<part);
		if (part == RAW_PART && phys != 0)
			cd->partflags[part] |= MCDREADRAW;
		return 0;
	}
	
	return ENXIO;
}

int mcdclose(dev_t dev)
{
	int unit,part,phys;
	struct mcd_data *cd;
	
	unit = mcd_unit(dev);
	if (unit >= NMCD)
		return ENXIO;

	cd = mcd_data + unit;
	part = mcd_part(dev);
	phys = mcd_phys(dev);
	
	if (!(cd->flags & MCDINIT))
		return ENXIO;

	mcd_getstat(unit,1);	/* get status */

	/* close channel */
	cd->partflags[part] &= ~(MCDOPEN|MCDREADRAW);
	cd->openflags &= ~(1<<part);
	MCD_TRACE("close: partition=%d\n",part,0,0,0);

	return 0;
}

void
mcdstrategy(struct buf *bp)
{
	struct mcd_data *cd;
	struct buf *qp;
	int s;
	
	int unit = mcd_unit(bp->b_dev);

	cd = mcd_data + unit;

	/* test validity */
/*MCD_TRACE("strategy: buf=0x%lx, unit=%ld, block#=%ld bcount=%ld\n",
	bp,unit,bp->b_blkno,bp->b_bcount);*/
	if (unit >= NMCD || bp->b_blkno < 0) {
		printf("mcdstrategy: unit = %d, blkno = %d, bcount = %d\n",
			unit, bp->b_blkno, bp->b_bcount);
		pg("mcd: mcdstratregy failure");
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto bad;
	}

	/* if device invalidated (e.g. media change, door open), error */
	if (!(cd->flags & MCDVALID)) {
MCD_TRACE("strategy: drive not valid\n",0,0,0,0);
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
	
	/* for non raw access, check partition limits */
	if (mcd_part(bp->b_dev) != RAW_PART) {
		if (!(cd->flags & MCDLABEL)) {
			bp->b_error = EIO;
			goto bad;
		}
		/* adjust transfer if necessary */
		if (bounds_check_with_label(bp,&cd->dlabel,1) <= 0) {
			goto done;
		}
	}
	
	/* queue it */
	qp = &cd->head;
	s = splbio();
	disksort(qp,bp);
	splx(s);
	
	/* now check whether we can perform processing */
	mcd_start(unit);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}

static void mcd_start(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
	struct buf *bp, *qp = &cd->head;
	struct partition *p;
	int part;
	register s = splbio();
	
	if (cd->flags & MCDMBXBSY)
		return;

	if ((bp = qp->b_actf) != 0) {
		/* block found to process, dequeue */
		/*MCD_TRACE("mcd_start: found block bp=0x%x\n",bp,0,0,0);*/
		qp->b_actf = bp->av_forw;
		splx(s);
	} else {
		/* nothing to do */
		splx(s);
		return;
	}

	/* changed media? */
	if (!(cd->flags	& MCDVALID)) {
		MCD_TRACE("mcd_start: drive not valid\n",0,0,0,0);
		return;
	}

	p = cd->dlabel.d_partitions + mcd_part(bp->b_dev);

	cd->flags |= MCDMBXBSY;
	cd->mbx.unit = unit;
	cd->mbx.port = cd->iobase;
	cd->mbx.retry = MCD_RETRYS;
	cd->mbx.bp = bp;
	cd->mbx.p_offset = p->p_offset;

	/* calling the read routine */
	mcd_doread((caddr_t)MCD_S_BEGIN, (int)&(cd->mbx));
	/* triggers mcd_start, when successful finished */
	return;
}

int mcdioctl(dev_t dev, int cmd, caddr_t addr, int flags)
{
	struct mcd_data *cd;
	int unit,part;
	
	unit = mcd_unit(dev);
	part = mcd_part(dev);
	cd = mcd_data + unit;

#ifdef MCDMINI
	return ENOTTY;
#else
	if (!(cd->flags & MCDVALID))
		return EIO;
MCD_TRACE("ioctl called 0x%x\n",cmd,0,0,0);

	switch (cmd) {
	case DIOCSBAD:
		return EINVAL;
	case DIOCGDINFO:
	case DIOCGPART:
	case DIOCWDINFO:
	case DIOCSDINFO:
	case DIOCWLABEL:
		return ENOTTY;
	case CDIOCPLAYTRACKS:
		return mcd_playtracks(unit, (struct ioc_play_track *) addr);
	case CDIOCPLAYBLOCKS:
		return mcd_play(unit, (struct mcd_read2 *) addr);
	case CDIOCREADSUBCHANNEL:
		return mcd_subchan(unit, (struct ioc_read_subchannel *) addr);
	case CDIOREADTOCHEADER:
		return mcd_toc_header(unit, (struct ioc_toc_header *) addr);
	case CDIOREADTOCENTRYS:
		return mcd_toc_entry(unit, (struct ioc_read_toc_entry *) addr);
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
		return mcd_resume(unit);
	case CDIOCPAUSE:
		return mcd_pause(unit);
	case CDIOCSTART:
		return EINVAL;
	case CDIOCSTOP:
		return mcd_stop(unit);
	case CDIOCEJECT:
		return EINVAL;
	case CDIOCSETDEBUG:
		cd->debug = 1;
		return 0;
	case CDIOCCLRDEBUG:
		cd->debug = 0;
		return 0;
	case CDIOCRESET:
		return EINVAL;
	default:
		return ENOTTY;
	}
	/*NOTREACHED*/
#endif /*!MCDMINI*/
}

/* this could have been taken from scsi/cd.c, but it is not clear
 * whether the scsi cd driver is linked in
 */
static int mcd_getdisklabel(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
	
	if (cd->flags & MCDLABEL)
		return -1;
	
	bzero(&cd->dlabel,sizeof(struct disklabel));
	strncpy(cd->dlabel.d_typename,"Mitsumi CD ROM ",16);
	strncpy(cd->dlabel.d_packname,"unknown        ",16);
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
	
	cd->flags |= MCDLABEL;
	return 0;
}

int mcdsize(dev_t dev)
{
	int size;
	int unit = mcd_unit(dev);
	struct mcd_data *cd = mcd_data + unit;

	if (mcd_volinfo(unit) >= 0) {
		cd->blksize = MCDBLK;
		size = msf2hsg(cd->volinfo.vol_msf);
		cd->disksize = size * (MCDBLK/DEV_BSIZE);
		return 0;
	}
	return -1;
}

/***************************************************************
 * lower level of driver starts here
 **************************************************************/

#ifdef NOTDEF
static char irqs[] = {
	0x00,0x00,0x10,0x20,0x00,0x30,0x00,0x00,
	0x00,0x10,0x40,0x50,0x00,0x00,0x00,0x00
};

static char drqs[] = {
	0x00,0x01,0x00,0x03,0x00,0x05,0x06,0x07,
};
#endif

static void mcd_configure(struct mcd_data *cd)
{
	outb(cd->iobase+mcd_config,cd->config);
}

/* check if there is a cdrom */
/* Heavly hacked by gclarkii@sugar.neosoft.com */

int mcd_probe(struct isa_device *dev)
{
	int port = dev->id_iobase;	
	int unit = dev->id_unit;
	int i;
	int st;
	int check;
	int junk;

	mcd_data[unit].flags = MCDPROBING;

#ifdef NOTDEF
	/* get irq/drq configuration word */
	mcd_data[unit].config = irqs[dev->id_irq]; /* | drqs[dev->id_drq];*/
#else
	mcd_data[unit].config = 0;
#endif

	/* send a reset */
	outb(port+MCD_FLAGS,0);
	DELAY(100000);
	/* get any pending status and throw away...*/
	for (i=10; i != 0; i--) {
		inb(port+MCD_DATA);
	}
	DELAY(1000);

	outb(port+MCD_DATA,MCD_CMDGETSTAT);	/* Send get status command */

	/* Loop looking for avail of status */
	/* XXX May have to increase for fast machinces */
	for (i = 1000; i != 0; i--) {
		if ((inb(port+MCD_FLAGS) & 0xF ) == STATUS_AVAIL) {
			break;
		}
		DELAY(10);
	}
	/* get status */

	if (i == 0) {
#ifdef DEBUG
		printf ("Mitsumi drive NOT detected\n");
#endif
	return 0;
	}

/*
 * The following code uses the 0xDC command, it returns a M from the
 * second byte and a number in the third.  Does anyone know what the
 * number is for? Better yet, how about someone thats REAL good in
 * i80x86 asm looking at the Dos driver... Most of this info came
 * from a friend of mine spending a whole weekend.....
 */

	DELAY (2000);
	outb(port+MCD_DATA,MCD_CMDCONTINFO);
	for (i = 0; i < 100000; i++) {
		if ((inb(port+MCD_FLAGS) & 0xF) == STATUS_AVAIL)
			break;
	}
	if (i > 100000) {
#ifdef DEBUG
		printf ("Mitsumi drive error\n");
#endif
		return 0;
	}
	DELAY (40000);
	st = inb(port+MCD_DATA);
	DELAY (500);
	check = inb(port+MCD_DATA);
	DELAY (500);
	junk = inb(port+MCD_DATA);	/* What is byte used for?!?!? */

	if (check = 'M') {
#ifdef DEBUG
		printf("Mitsumi drive detected\n");
#endif
		return 4;
	} else {
		printf("Mitsumi drive NOT detected\n");
		printf("Mitsumi drive error\n");
		return 0;
	}
}

static int mcd_waitrdy(int port,int dly)
{
	int i;

	/* wait until xfer port senses data ready */
	for (i=0; i<dly; i++) {
		if ((inb(port+mcd_xfer) & MCD_ST_BUSY)==0)
			return 0;
		mcd_delay(1);
	}
	return -1;
}

static int mcd_getreply(int unit,int dly)
{
	int	i;
	struct	mcd_data *cd = mcd_data + unit;
	int	port = cd->iobase;

	/* wait data to become ready */
	if (mcd_waitrdy(port,dly)<0) {
#ifdef MCD_TO_WARNING_ON
		printf("mcd%d: timeout getreply\n",unit);
#endif
		return -1;
	}

	/* get the data */
	return inb(port+mcd_status) & 0xFF;
}

static int mcd_getstat(int unit,int sflg)
{
	int	i;
	struct	mcd_data *cd = mcd_data + unit;
	int	port = cd->iobase;

	/* get the status */
	if (sflg)
		outb(port+mcd_command, MCD_CMDGETSTAT);
	i = mcd_getreply(unit,DELAY_GETREPLY);
	if (i<0) return -1;

	cd->status = i;

	mcd_setflags(unit,cd);
	return cd->status;
}

static void mcd_setflags(int unit, struct mcd_data *cd)
{
	/* check flags */
	if (cd->status & (MCDDSKCHNG|MCDDOOROPEN)) {
		MCD_TRACE("getstat: sensed DSKCHNG or DOOROPEN\n",0,0,0,0);
		cd->flags &= ~MCDVALID;
	}

#ifndef MCDMINI
	if (cd->status & MCDAUDIOBSY)
		cd->audio_status = CD_AS_PLAY_IN_PROGRESS;
	else if (cd->audio_status == CD_AS_PLAY_IN_PROGRESS)
		cd->audio_status = CD_AS_PLAY_COMPLETED;
#endif
}

static int mcd_get(int unit, char *buf, int nmax)
{
	int port = mcd_data[unit].iobase;
	int i,k;
	
	for (i=0; i<nmax; i++) {

		/* wait for data */
		if ((k = mcd_getreply(unit,DELAY_GETREPLY)) < 0) {
#ifdef MCD_TO_WARNING_ON
			printf("mcd%d: timeout mcd_get\n",unit);
#endif
			return -1;
		}
		buf[i] = k;
	}
	return i;
}

static int mcd_send(int unit, int cmd,int nretrys)
{
	int i,k;
	int port = mcd_data[unit].iobase;
	
/*MCD_TRACE("mcd_send: command = 0x%x\n",cmd,0,0,0);*/
	for (i=0; i<nretrys; i++) {
		outb(port+mcd_command, cmd);
		if ((k=mcd_getstat(unit,0)) != -1)
			break;
	}
	if (i == nretrys) {
		printf("mcd%d: mcd_send retry cnt exceeded\n",unit);
		return -1;
	}
/*MCD_TRACE("mcd_send: status = 0x%x\n",k,0,0,0);*/
	return 0;
}

static int bcd2bin(bcd_t b)
{
	return (b >> 4) * 10 + (b & 15);
}

static bcd_t bin2bcd(int b)
{
	return ((b / 10) << 4) | (b % 10);
}

static void hsg2msf(int hsg, bcd_t *msf)
{
	hsg += 150;
	M_msf(msf) = bin2bcd(hsg / 4500);
	hsg %= 4500;
	S_msf(msf) = bin2bcd(hsg / 75);
	F_msf(msf) = bin2bcd(hsg % 75);
}

static int msf2hsg(bcd_t *msf)
{
	return (bcd2bin(M_msf(msf)) * 60 +
		bcd2bin(S_msf(msf))) * 75 +
		bcd2bin(F_msf(msf)) - 150;
}

static int mcd_volinfo(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
	int i;

/*MCD_TRACE("mcd_volinfo: enter\n",0,0,0,0);*/

	/* Get the status, in case the disc has been changed */
	if (mcd_getstat(unit, 1) < 0) return EIO;

	/* Just return if we already have it */
	if (cd->flags & MCDVOLINFO) return 0;

	/* send volume info command */
	if (mcd_send(unit,MCD_CMDGETVOLINFO,MCD_RETRYS) < 0)
		return -1;

	/* get data */
	if (mcd_get(unit,(char*) &cd->volinfo,sizeof(struct mcd_volinfo)) < 0) {
		printf("mcd%d: mcd_volinfo: error read data\n",unit);
		return -1;
	}

	if (cd->volinfo.trk_low != 0 || cd->volinfo.trk_high != 0) {
		cd->flags |= MCDVOLINFO;	/* volinfo is OK */
		return 0;
	}

	return -1;
}

void
mcdintr(unit)
	int unit;
{
	int	port = mcd_data[unit].iobase;
	u_int	i;
	
	MCD_TRACE("stray interrupt xfer=0x%x\n",inb(port+mcd_xfer),0,0,0);

	/* just read out status and ignore the rest */
	if ((inb(port+mcd_xfer)&0xFF) != 0xFF) {
		i = inb(port+mcd_status);
	}
}

/* state machine to process read requests
 * initialize with MCD_S_BEGIN: calculate sizes, and read status
 * MCD_S_WAITSTAT: wait for status reply, set mode
 * MCD_S_WAITMODE: waits for status reply from set mode, set read command
 * MCD_S_WAITREAD: wait for read ready, read data
 */
static struct mcd_mbx *mbxsave;

/*
 * Good thing Alphas come with real CD players...
 */
static void mcd_doread(caddr_t xstate, int xmbxin)
{
	int state = (int)xstate;
	struct mcd_mbx *mbxin = (struct mcd_mbx *)xmbxin;
	struct mcd_mbx *mbx = (state!=MCD_S_BEGIN) ? mbxsave : mbxin;
	int	unit = mbx->unit;
	int	port = mbx->port;
	struct	buf *bp = mbx->bp;
	struct	mcd_data *cd = mcd_data + unit;

	int	rm,i,k;
	struct mcd_read2 rbuf;
	int	blknum;
	caddr_t	addr;

loop:
	switch (state) {
	case MCD_S_BEGIN:
		mbx = mbxsave = mbxin;

	case MCD_S_BEGIN1:
		/* get status */
		outb(port+mcd_command, MCD_CMDGETSTAT);
		mbx->count = RDELAY_WAITSTAT;
		timeout((timeout_func_t)mcd_doread,(caddr_t)MCD_S_WAITSTAT,hz/100); /* XXX */
		return;
	case MCD_S_WAITSTAT:
		untimeout(mcd_doread, (caddr_t)MCD_S_WAITSTAT);
		if (mbx->count-- >= 0) {
			if (inb(port+mcd_xfer) & MCD_ST_BUSY) {
				timeout((timeout_func_t)mcd_doread,(caddr_t)MCD_S_WAITSTAT,hz/100); /* XXX */
				return;
			}
			mcd_setflags(unit,cd);
			MCD_TRACE("got WAITSTAT delay=%d\n",RDELAY_WAITSTAT-mbx->count,0,0,0);
			/* reject, if audio active */
			if (cd->status & MCDAUDIOBSY) {
				printf("mcd%d: audio is active\n",unit);
				goto readerr;
			}

			/* to check for raw/cooked mode */
			if (cd->flags & MCDREADRAW) {
				rm = MCD_MD_RAW;
				mbx->sz = MCDRBLK;
			} else {
				rm = MCD_MD_COOKED;
				mbx->sz = cd->blksize;
			}

			mbx->count = RDELAY_WAITMODE;
		
			mcd_put(port+mcd_command, MCD_CMDSETMODE);
			mcd_put(port+mcd_command, rm);
			timeout((timeout_func_t)mcd_doread,(caddr_t)MCD_S_WAITMODE,hz/100); /* XXX */
			return;
		} else {
#ifdef MCD_TO_WARNING_ON
			printf("mcd%d: timeout getstatus\n",unit);
#endif
			goto readerr;
		}

	case MCD_S_WAITMODE:
		untimeout(mcd_doread, (caddr_t)MCD_S_WAITMODE);
		if (mbx->count-- < 0) {
#ifdef MCD_TO_WARNING_ON
			printf("mcd%d: timeout set mode\n",unit);
#endif
			goto readerr;
		}
		if (inb(port+mcd_xfer) & MCD_ST_BUSY) {
			timeout((timeout_func_t)mcd_doread,(caddr_t)MCD_S_WAITMODE,hz/100);
			return;
		}
		mcd_setflags(unit,cd);
		MCD_TRACE("got WAITMODE delay=%d\n",RDELAY_WAITMODE-mbx->count,0,0,0);
		/* for first block */
		mbx->nblk = (bp->b_bcount + (mbx->sz-1)) / mbx->sz;
		mbx->skip = 0;

nextblock:
		blknum 	= (bp->b_blkno / (mbx->sz/DEV_BSIZE))
			+ mbx->p_offset + mbx->skip/mbx->sz;

		MCD_TRACE("mcd_doread: read blknum=%d for bp=0x%x\n",blknum,bp,0,0);

		/* build parameter block */
		hsg2msf(blknum,rbuf.start_msf);

		/* send the read command */
		mcd_put(port+mcd_command,MCD_CMDREAD2);
		mcd_put(port+mcd_command,rbuf.start_msf[0]);
		mcd_put(port+mcd_command,rbuf.start_msf[1]);
		mcd_put(port+mcd_command,rbuf.start_msf[2]);
		mcd_put(port+mcd_command,0);
		mcd_put(port+mcd_command,0);
		mcd_put(port+mcd_command,1);
		mbx->count = RDELAY_WAITREAD;
		timeout((timeout_func_t)mcd_doread,(caddr_t)MCD_S_WAITREAD,hz/100); /* XXX */
		return;
	case MCD_S_WAITREAD:
		untimeout(mcd_doread,(caddr_t)MCD_S_WAITREAD);
		if (mbx->count-- > 0) {
			k = inb(port+mcd_xfer);
			if ((k & 2)==0) {
			MCD_TRACE("got data delay=%d\n",RDELAY_WAITREAD-mbx->count,0,0,0);
				/* data is ready */
				addr	= bp->b_un.b_addr + mbx->skip;
				outb(port+mcd_ctl2,0x04);	/* XXX */
				for (i=0; i<mbx->sz; i++)
					*addr++	= inb(port+mcd_rdata);
				outb(port+mcd_ctl2,0x0c);	/* XXX */

				if (--mbx->nblk > 0) {
					mbx->skip += mbx->sz;
					goto nextblock;
				}

				/* return buffer */
				bp->b_resid = 0;
				biodone(bp);

				cd->flags &= ~MCDMBXBSY;
				mcd_start(mbx->unit);
				return;
			}
			if ((k & 4)==0)
				mcd_getstat(unit,0);
			timeout((timeout_func_t)mcd_doread,(caddr_t)MCD_S_WAITREAD,hz/100); /* XXX */
			return;
		} else {
#ifdef MCD_TO_WARNING_ON
			printf("mcd%d: timeout read data\n",unit);
#endif
			goto readerr;
		}
	}

readerr:
	if (mbx->retry-- > 0) {
#ifdef MCD_TO_WARNING_ON
		printf("mcd%d: retrying\n",unit);
#endif
		state = MCD_S_BEGIN1;
		goto loop;
	}

	/* invalidate the buffer */
	bp->b_flags |= B_ERROR;
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	mcd_start(mbx->unit);
	return;

#ifdef NOTDEF
	printf("mcd%d: unit timeout, resetting\n",mbx->unit);
	outb(mbx->port+mcd_reset,MCD_CMDRESET);
	DELAY(300000);
	(void)mcd_getstat(mbx->unit,1);
	(void)mcd_getstat(mbx->unit,1);
	/*cd->status &= ~MCDDSKCHNG; */
	cd->debug = 1; /* preventive set debug mode */

#endif

}

#ifndef MCDMINI
static int mcd_setmode(int unit, int mode)
{
	struct mcd_data *cd = mcd_data + unit;
	int port = cd->iobase;
	int retry;

	printf("mcd%d: setting mode to %d\n", unit, mode);
	for(retry=0; retry<MCD_RETRYS; retry++)
	{
		outb(port+mcd_command, MCD_CMDSETMODE);
		outb(port+mcd_command, mode);
		if (mcd_getstat(unit, 0) != -1) return 0;
	}

	return -1;
}

static int mcd_toc_header(int unit, struct ioc_toc_header *th)
{
	struct mcd_data *cd = mcd_data + unit;

	if (mcd_volinfo(unit) < 0)
		return ENXIO;

	th->len = msf2hsg(cd->volinfo.vol_msf);
	th->starting_track = bcd2bin(cd->volinfo.trk_low);
	th->ending_track = bcd2bin(cd->volinfo.trk_high);

	return 0;
}

static int mcd_read_toc(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
	struct ioc_toc_header th;
	struct mcd_qchninfo q;
	int rc, trk, idx, retry;

	/* Only read TOC if needed */
	if (cd->flags & MCDTOC) return 0;

	printf("mcd%d: reading toc header\n", unit);
	if (mcd_toc_header(unit, &th) != 0)
		return ENXIO;

	printf("mcd%d: stopping play\n", unit);
	if ((rc=mcd_stop(unit)) != 0)
		return rc;

	/* try setting the mode twice */
	if (mcd_setmode(unit, MCD_MD_TOC) != 0)
		return EIO;
	if (mcd_setmode(unit, MCD_MD_TOC) != 0)
		return EIO;

	printf("mcd%d: get_toc reading qchannel info\n",unit);	
	for(trk=th.starting_track; trk<=th.ending_track; trk++)
		cd->toc[trk].idx_no = 0;
	trk = th.ending_track - th.starting_track + 1;
	for(retry=0; retry<300 && trk>0; retry++)
	{
		if (mcd_getqchan(unit, &q) < 0) break;
		idx = bcd2bin(q.idx_no);
		if (idx>0 && idx < MCD_MAXTOCS && q.trk_no==0)
			if (cd->toc[idx].idx_no == 0)
			{
				cd->toc[idx] = q;
				trk--;
			}
	}

	if (mcd_setmode(unit, MCD_MD_COOKED) != 0)
		return EIO;

	if (trk != 0) return ENXIO;

	/* add a fake last+1 */
	idx = th.ending_track + 1;
	cd->toc[idx].ctrl_adr = cd->toc[idx-1].ctrl_adr;
	cd->toc[idx].trk_no = 0;
	cd->toc[idx].idx_no = 0xAA;
	cd->toc[idx].hd_pos_msf[0] = cd->volinfo.vol_msf[0];
	cd->toc[idx].hd_pos_msf[1] = cd->volinfo.vol_msf[1];
	cd->toc[idx].hd_pos_msf[2] = cd->volinfo.vol_msf[2];

	cd->flags |= MCDTOC;

	return 0;
}

static int mcd_toc_entry(int unit, struct ioc_read_toc_entry *te)
{
	struct mcd_data *cd = mcd_data + unit;
	struct ret_toc
	{
		struct ioc_toc_header th;
		struct cd_toc_entry rt;
	} ret_toc;
	struct ioc_toc_header th;
	int rc, i;

	/* Make sure we have a valid toc */
	if ((rc=mcd_read_toc(unit)) != 0)
		return rc;

	/* find the toc to copy*/
	i = te->starting_track;
	if (i == MCD_LASTPLUS1)
		i = bcd2bin(cd->volinfo.trk_high) + 1;
	
	/* verify starting track */
	if (i < bcd2bin(cd->volinfo.trk_low) ||
		i > bcd2bin(cd->volinfo.trk_high)+1)
		return EINVAL;

	/* do we have room */
	if (te->data_len < sizeof(struct ioc_toc_header) +
		sizeof(struct cd_toc_entry)) return EINVAL;

	/* Copy the toc header */
	if (mcd_toc_header(unit, &th) < 0) return EIO;
	ret_toc.th = th;

	/* copy the toc data */
	ret_toc.rt.control = cd->toc[i].ctrl_adr;
	ret_toc.rt.addr_type = te->address_format;
	ret_toc.rt.track = i;
	if (te->address_format == CD_MSF_FORMAT)
	{
		ret_toc.rt.addr[1] = cd->toc[i].hd_pos_msf[0];
		ret_toc.rt.addr[2] = cd->toc[i].hd_pos_msf[1];
		ret_toc.rt.addr[3] = cd->toc[i].hd_pos_msf[2];
	}

	/* copy the data back */
	copyout(&ret_toc, te->data, sizeof(struct cd_toc_entry)
		+ sizeof(struct ioc_toc_header));

	return 0;
}

static int mcd_stop(int unit)
{
	struct mcd_data *cd = mcd_data + unit;

	if (mcd_send(unit, MCD_CMDSTOPAUDIO, MCD_RETRYS) < 0)
		return ENXIO;
	cd->audio_status = CD_AS_PLAY_COMPLETED;
	return 0;
}

static int mcd_getqchan(int unit, struct mcd_qchninfo *q)
{
	struct mcd_data *cd = mcd_data + unit;

	if (mcd_send(unit, MCD_CMDGETQCHN, MCD_RETRYS) < 0)
		return -1;
	if (mcd_get(unit, (char *) q, sizeof(struct mcd_qchninfo)) < 0)
		return -1;
	if (cd->debug)
	printf("mcd%d: qchannel ctl=%d, t=%d, i=%d, ttm=%d:%d.%d dtm=%d:%d.%d\n",
		unit,
		q->ctrl_adr, q->trk_no, q->idx_no,
		q->trk_size_msf[0], q->trk_size_msf[1], q->trk_size_msf[2],
		q->trk_size_msf[0], q->trk_size_msf[1], q->trk_size_msf[2]);
	return 0;
}

static int mcd_subchan(int unit, struct ioc_read_subchannel *sc)
{
	struct mcd_data *cd = mcd_data + unit;
	struct mcd_qchninfo q;
	struct cd_sub_channel_info data;

	printf("mcd%d: subchan af=%d, df=%d\n", unit,
		sc->address_format,
		sc->data_format);
	if (sc->address_format != CD_MSF_FORMAT) return EIO;
	if (sc->data_format != CD_CURRENT_POSITION) return EIO;

	if (mcd_getqchan(unit, &q) < 0) return EIO;

	data.header.audio_status = cd->audio_status;
	data.what.position.data_format = CD_MSF_FORMAT;
	data.what.position.track_number = bcd2bin(q.trk_no);

	if (copyout(&data, sc->data, sizeof(struct cd_sub_channel_info))!=0)
		return EFAULT;
	return 0;
}

static int mcd_playtracks(int unit, struct ioc_play_track *pt)
{
	struct mcd_data *cd = mcd_data + unit;
	struct mcd_read2 pb;
	int a = pt->start_track;
	int z = pt->end_track;
	int rc;

	if ((rc = mcd_read_toc(unit)) != 0) return rc;

	printf("mcd%d: playtracks from %d:%d to %d:%d\n", unit,
		a, pt->start_index, z, pt->end_index);

	if (a < cd->volinfo.trk_low || a > cd->volinfo.trk_high || a > z ||
		z < cd->volinfo.trk_low || z > cd->volinfo.trk_high)
		return EINVAL;

	pb.start_msf[0] = cd->toc[a].hd_pos_msf[0];
	pb.start_msf[1] = cd->toc[a].hd_pos_msf[1];
	pb.start_msf[2] = cd->toc[a].hd_pos_msf[2];
	pb.end_msf[0] = cd->toc[z+1].hd_pos_msf[0];
	pb.end_msf[1] = cd->toc[z+1].hd_pos_msf[1];
	pb.end_msf[2] = cd->toc[z+1].hd_pos_msf[2];

	return mcd_play(unit, &pb);
}

static int mcd_play(int unit, struct mcd_read2 *pb)
{
	struct mcd_data *cd = mcd_data + unit;
	int port = cd->iobase;
	int retry, st;

	cd->lastpb = *pb;
	for(retry=0; retry<MCD_RETRYS; retry++)
	{
		outb(port+mcd_command, MCD_CMDREAD2);
		outb(port+mcd_command, pb->start_msf[0]);
		outb(port+mcd_command, pb->start_msf[1]);
		outb(port+mcd_command, pb->start_msf[2]);
		outb(port+mcd_command, pb->end_msf[0]);
		outb(port+mcd_command, pb->end_msf[1]);
		outb(port+mcd_command, pb->end_msf[2]);
		if ((st=mcd_getstat(unit, 0)) != -1) break;
	}

	if (cd->debug)
	printf("mcd%d: mcd_play retry=%d, status=%d\n", unit, retry, st);
	if (st == -1) return ENXIO;
	cd->audio_status = CD_AS_PLAY_IN_PROGRESS;
	return 0;
}

static int mcd_pause(int unit)
{
	struct mcd_data *cd = mcd_data + unit;
	struct mcd_qchninfo q;
	int rc;

	/* Verify current status */
	if (cd->audio_status != CD_AS_PLAY_IN_PROGRESS)
	{
		printf("mcd%d: pause attempted when not playing\n", unit);
		return EINVAL;
	}

	/* Get the current position */
	if (mcd_getqchan(unit, &q) < 0) return EIO;

	/* Copy it into lastpb */
	cd->lastpb.start_msf[0] = q.hd_pos_msf[0];
	cd->lastpb.start_msf[1] = q.hd_pos_msf[1];
	cd->lastpb.start_msf[2] = q.hd_pos_msf[2];

	/* Stop playing */
	if ((rc=mcd_stop(unit)) != 0) return rc;

	/* Set the proper status and exit */
	cd->audio_status = CD_AS_PLAY_PAUSED;
	return 0;
}

static int mcd_resume(int unit)
{
	struct mcd_data *cd = mcd_data + unit;

	if (cd->audio_status != CD_AS_PLAY_PAUSED) return EINVAL;
	return mcd_play(unit, &cd->lastpb);
}
#endif /*!MCDMINI*/

#endif /* NMCD > 0 */
