/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)conf.c	5.8 (Berkeley) 5/12/91
 *	$Id: conf.c,v 1.14 1993/11/03 18:07:32 nate Exp $
 */

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "ioctl.h"
#include "tty.h"
#include "conf.h"

int	nullop(), enxio(), enodev(), rawread(), rawwrite(), swstrategy();
int	rawread(), rawwrite(), swstrategy();

#include "wd.h"
#include "wx.h"
#if (NWD > 0) || (NWX > 0)
int	wdopen(),wdclose(),wdstrategy(),wdioctl();
int	wddump(),wdsize();
#else
#define	wdopen		enxio
#define	wdclose		enxio
#define	wdstrategy	enxio
#define	wdioctl		enxio
#define	wddump		enxio
#define	wdsize		NULL
#endif

#include "sd.h"
#if NSD > 0
int	sdopen(),sdclose(),sdstrategy(),sdioctl();
int	sddump(),sdsize();
#else
#define	sdopen		enxio
#define	sdclose		enxio
#define	sdstrategy	enxio
#define	sdioctl		enxio
#define	sddump		enxio
#define	sdsize		NULL
#endif

#include "st.h"
#if NST > 0
int	stopen(),stclose(),ststrategy(),stioctl();
/*int	stdump(),stsize();*/
#define	stdump		enxio
#define	stsize		NULL
#else
#define	stopen		enxio
#define	stclose		enxio
#define	ststrategy	enxio
#define	stioctl		enxio
#define	stdump		enxio
#define	stsize		NULL
#endif

#include "cd.h"
#if NCD > 0
int	cdopen(),cdclose(),cdstrategy(),cdioctl();
int	/*cddump(),*/cdsize();
#define	cddump		enxio
#else
#define	cdopen		enxio
#define	cdclose		enxio
#define	cdstrategy	enxio
#define	cdioctl		enxio
#define	cddump		enxio
#define	cdsize		NULL
#endif

#include "mcd.h"
#if NMCD > 0
int	mcdopen(),mcdclose(),mcdstrategy(),mcdioctl();
int	/*mcddump(),*/mcdsize();
#define	mcddump		enxio
#else
#define	mcdopen		enxio
#define	mcdclose	enxio
#define	mcdstrategy	enxio
#define	mcdioctl	enxio
#define	mcddump		enxio
#define	mcdsize		NULL
#endif

#include "ch.h"
#if NCH > 0
int	chopen(),chclose(),chioctl();
#else
#define	chopen		enxio
#define	chclose		enxio
#define	chioctl		enxio
#endif

#include "wt.h"
#if NWT > 0
int	wtopen(),wtclose(),wtstrategy(),wtioctl();
int	wtdump(),wtsize();
#else
#define	wtopen		enxio
#define	wtclose		enxio
#define	wtstrategy	enxio
#define	wtioctl		enxio
#define	wtdump		enxio
#define	wtsize		NULL
#endif

#include "fd.h"
#if NFD > 0
int	Fdopen(),fdclose(),fdstrategy(),fdioctl();
#define	fddump		enxio
#define	fdsize		NULL
#else
#define	Fdopen		enxio
#define	fdclose		enxio
#define	fdstrategy	enxio
#define	fdioctl		enxio
#define	fddump		enxio
#define	fdsize		NULL
#endif

int	swstrategy(),swread(),swwrite();

struct bdevsw	bdevsw[] =
{
	{ wdopen,	wdclose,	wdstrategy,	wdioctl,	/*0*/
	  wddump,	wdsize,		NULL },
	{ enodev,	enodev,		swstrategy,	enodev,		/*1*/
	  enodev,	enodev,		NULL },
	{ Fdopen,	fdclose,	fdstrategy,	fdioctl,	/*2*/
	  fddump,	fdsize,		NULL },
	{ wtopen,	wtclose,	wtstrategy,	wtioctl,	/*3*/
	  wtdump,	wtsize,		B_TAPE },
	{ sdopen,	sdclose,	sdstrategy,	sdioctl,	/*4*/
	  sddump,	sdsize,		NULL },
	{ stopen,	stclose,	ststrategy,	stioctl,	/*5*/
	  stdump,	stsize,		NULL },
	{ cdopen,	cdclose,	cdstrategy,	cdioctl,	/*6*/
	  cddump,	cdsize,		NULL },
	{ mcdopen,	mcdclose,	mcdstrategy,	mcdioctl,	/*7*/
	  mcddump,	mcdsize,	NULL },
/*
 * If you need a bdev major number, please contact the 386bsd patchkit
 * coordinator by sending mail to "patches@cs.montana.edu". 
 * If you assign one yourself it may conflict with someone else.
 */
};
int	nblkdev = sizeof (bdevsw) / sizeof (bdevsw[0]);

int	cnopen(),cnclose(),cnread(),cnwrite(),cnioctl(),cnselect();

int	pcopen(),pcclose(),pcread(),pcwrite(),pcioctl(),pcmmap();
extern	struct tty pccons;

int	cttyopen(), cttyread(), cttywrite(), cttyioctl(), cttyselect();

int 	mmopen(), mmclose(), mmrw();
#define	mmselect	seltrue

#include "pty.h"
#if NPTY > 0
int	ptsopen(),ptsclose(),ptsread(),ptswrite(),ptsstop();
int	ptcopen(),ptcclose(),ptcread(),ptcwrite(),ptcselect();
int	ptyioctl();
struct	tty pt_tty[];
#else
#define ptsopen		enxio
#define ptsclose	enxio
#define ptsread		enxio
#define ptswrite	enxio
#define ptcopen		enxio
#define ptcclose	enxio
#define ptcread		enxio
#define ptcwrite	enxio
#define ptyioctl	enxio
#define	pt_tty		NULL
#define	ptcselect	enxio
#define	ptsstop		nullop
#endif

#include "com.h"
#if NCOM > 0
int	comopen(),comclose(),comread(),comwrite(),comioctl(),comselect();
#define comreset	enxio
extern	struct tty com_tty[];
#else
#define comopen		enxio
#define comclose	enxio
#define comread		enxio
#define comwrite	enxio
#define comioctl	enxio
#define comreset	enxio
#define comselect	enxio
#define	com_tty		NULL
#endif

int	logopen(),logclose(),logread(),logioctl(),logselect();

int	ttselect(), seltrue();

#include "lpt.h"
#if NLPT > 0
int	lptopen(),lptclose(),lptwrite(),lptioctl();
#else
#define	lptopen		enxio
#define	lptclose	enxio
#define	lptwrite	enxio
#define	lptioctl	enxio
#endif

#include "tw.h"
#if NTW > 0
int	twopen(),twclose(),twread(),twwrite(),twselect();
#else
#define twopen		enxio
#define twclose		enxio
#define twread		enxio
#define twwrite		enxio
#define twselect	enxio
#endif

#include "sb.h"                 /* Sound Blaster */
#if     NSB > 0
int     sbopen(), sbclose(), sbioctl(), sbread(), sbwrite();
int     sbselect();
#else
#define sbopen         enxio
#define sbclose        enxio
#define sbioctl        enxio
#define sbread         enxio
#define sbwrite        enxio
#define sbselect       seltrue
#endif

#include "psm.h"
#if NPSM > 0
int	psmopen(),psmclose(),psmread(),psmselect(),psmioctl();
#else
#define psmopen		enxio
#define psmclose	enxio
#define psmread		enxio
#define psmselect	enxio
#define psmioctl	enxio
#endif

#include "snd.h"                 /* General Sound Driver */
#if     NSND > 0
int     sndopen(), sndclose(), sndioctl(), sndread(), sndwrite();
int     sndselect();
#else
#define sndopen         enxio
#define sndclose        enxio
#define sndioctl        enxio
#define sndread         enxio
#define sndwrite        enxio
#define sndselect       seltrue
#endif

int	fdopen();

#include "bpfilter.h"
#if NBPFILTER > 0
int	bpfopen(),bpfclose(),bpfread(),bpfwrite(),bpfselect(),bpfioctl();
#else
#define	bpfopen		enxio
#define	bpfclose	enxio
#define	bpfread		enxio
#define	bpfwrite	enxio
#define	bpfselect	enxio
#define	bpfioctl	enxio
#endif

#include "dcfclk.h"
#if NDCFCLK > 0
int	dcfclkopen(),dcfclkclose(),dcfclkread(),dcfclkioctl(),dcfclkselect();
#else
#define dcfclkopen	enxio
#define dcfclkclose	enxio
#define dcfclkread	enxio
#define dcfclkioctl	enxio
#define dcfclkselect	enxio
#endif

#include "lpa.h"
#if NLPA > 0
int	lpaopen(),lpaclose(),lpawrite(),lpaioctl();
#else
#define lpaopen		enxio
#define lpaclose	enxio
#define lpawrite	enxio
#define lpaioctl	enxio
#endif

#include "speaker.h"
#if NSPEAKER > 0
int     spkropen(),spkrclose(),spkrwrite(),spkrioctl();
#else
#define spkropen	enxio
#define spkrclose	enxio
#define spkrwrite	enxio
#define spkrioctl	enxio
#endif

#include "mse.h"
#if NMSE > 0
int	mseopen(),mseclose(),mseread(),mseselect();
#else
#define	mseopen		enxio
#define	mseclose	enxio
#define	mseread		enxio
#define	mseselect	enxio
#endif

#include "sio.h"
#if NSIO > 0
int	sioopen(),sioclose(),sioread(),siowrite(),sioioctl(),sioselect(),
	siostop();
#define sioreset	enxio
extern	struct tty sio_tty[];
#else
#define sioopen		enxio
#define sioclose	enxio
#define sioread		enxio
#define siowrite	enxio
#define sioioctl	enxio
#define siostop		enxio
#define sioreset	enxio
#define sioselect	enxio
#define	sio_tty		NULL
#endif

#include "su.h"
#if NSU > 0
int	suopen(),suclose(),suioctl();
#define	susize		NULL
#else
#define	suopen		enxio
#define	suclose		enxio
#define	suioctl		enxio
#define	susize		NULL
#endif

#include "uk.h"
#if NUK > 0
int	ukopen(),ukclose(),ukioctl();
#else
#define	ukopen		enxio
#define	ukclose		enxio
#define	ukioctl		enxio
#endif

struct cdevsw	cdevsw[] =
{
	{ cnopen,	cnclose,	cnread,		cnwrite,	/*0*/
	  cnioctl,	nullop,		nullop,		NULL,	/* console */
	  cnselect,	enodev,		NULL },
	{ cttyopen,	nullop,		cttyread,	cttywrite,	/*1*/
	  cttyioctl,	nullop,		nullop,		NULL,	/* tty */
	  cttyselect,	enodev,		NULL },
	{ mmopen,	mmclose,	mmrw,		mmrw,		/*2*/
	  enodev,	nullop,		nullop,		NULL,	/* memory */
	  mmselect,	enodev,		NULL },
	{ wdopen,	wdclose,	rawread,	rawwrite,	/*3*/
	  wdioctl,	enodev,		nullop,		NULL,	/* wd */
	  seltrue,	enodev,		wdstrategy },
	{ nullop,	nullop,		rawread,	rawwrite,	/*4*/
	  enodev,	enodev,		nullop,		NULL,	/* swap */
	  enodev,	enodev,		swstrategy },
	{ ptsopen,	ptsclose,	ptsread,	ptswrite,	/*5*/
	  ptyioctl,	ptsstop,	nullop,		pt_tty, /* ttyp */
	  ttselect,	enodev,		NULL },
	{ ptcopen,	ptcclose,	ptcread,	ptcwrite,	/*6*/
	  ptyioctl,	nullop,		nullop,		pt_tty, /* ptyp */
	  ptcselect,	enodev,		NULL },
	{ logopen,	logclose,	logread,	enodev,		/*7*/
	  logioctl,	enodev,		nullop,		NULL,	/* klog */
	  logselect,	enodev,		NULL },
	{ comopen,	comclose,	comread,	comwrite,	/*8*/
	  comioctl,	enodev,		comreset,	com_tty, /* com */
	  comselect,	enodev,		NULL },
	{ Fdopen,	fdclose,	rawread,	rawwrite,	/*9*/
	  fdioctl,	enodev,		nullop,		NULL,	/* Fd (!=fd) */
	  seltrue,	enodev,		fdstrategy },
	{ wtopen,	wtclose,	rawread,	rawwrite,	/*10*/
	  wtioctl,	enodev,		nullop,		NULL,	/* wt */
	  seltrue,	enodev,		wtstrategy },
	{ enodev,	enodev,		enodev,		enodev,		/*11*/
	  enodev,	enodev,		nullop,		NULL,
	  seltrue,	enodev,		enodev },
	{ pcopen,	pcclose,	pcread,		pcwrite,	/*12*/
	  pcioctl,	nullop,		nullop,		&pccons, /* pc */
	  ttselect,	pcmmap,		NULL },
	{ sdopen,	sdclose,	rawread,	rawwrite,	/*13*/
	  sdioctl,	enodev,		nullop,		NULL,	/* sd */
	  seltrue,	enodev,		sdstrategy },
	{ stopen,	stclose,	rawread,	rawwrite,	/*14*/
	  stioctl,	enodev,		nullop,		NULL,	/* st */
	  seltrue,	enodev,		ststrategy },
	{ cdopen,	cdclose,	rawread,	enodev,		/*15*/
	  cdioctl,	enodev,		nullop,		NULL,	/* cd */
	  seltrue,	enodev,		cdstrategy },
	{ lptopen,	lptclose,	nullop,		lptwrite,	/*16*/
	  lptioctl,	nullop,		nullop,		NULL,	/* lpt */
	  seltrue,	enodev,		enodev},
	{ chopen,	chclose,	enxio,		enxio,		/*17*/
	  chioctl,	enxio,		enxio,		NULL,	/* ch */
	  enxio,	enxio,		enxio },
	{ suopen,	suclose,	enodev,		enodev,		/*18*/
	  suioctl,	enodev,		nullop,		NULL,	/* scsi 'generic' */
	  seltrue,	enodev,		enodev },
	{ twopen,	twclose,	twread,		twwrite,	/*19*/
	  enodev,	nullop,		nullop,		NULL,	/* tw */
	  twselect,	enodev,		enodev },
	{ sbopen,	sbclose,	sbread,		sbwrite,	/*20*/
	  sbioctl,	enodev,		enodev,		NULL,	/* soundblaster*/
	  sbselect,	enodev,		NULL },
	{ psmopen,	psmclose,	psmread,	nullop,		/*21*/
	  psmioctl,	enodev,		nullop,		NULL,	/* psm mice */
	  psmselect,	enodev,		NULL },
	{ fdopen,	enxio,		enxio,		enxio,		/*22*/
	  enxio,	enxio,		enxio,		NULL,	/* fd (!=Fd) */
	  enxio,	enxio,		enxio },
 	{ bpfopen,	bpfclose,	bpfread,	bpfwrite,	/*23*/
 	  bpfioctl,	enodev,		nullop,		NULL,	/* bpf */
 	  bpfselect,	enodev,		NULL },
	{ dcfclkopen,	dcfclkclose,	dcfclkread,	enodev,		/*24*/
	  dcfclkioctl,	enodev,		nullop,		NULL,	/* dcfclk */
	  dcfclkselect,	enodev,		NULL },
	{ lpaopen,	lpaclose,	nullop,		lpawrite,	/*25*/
	  lpaioctl,	nullop,		nullop,		NULL,	/* lpa */
	  seltrue,	enodev,		enodev},
	{ spkropen,     spkrclose,      enxio,          spkrwrite,      /*26*/
	  spkrioctl,    enxio,          enxio,          NULL,	/* spkr */
	  enxio,        enxio,          enxio },
	{ mseopen,	mseclose,	mseread,	nullop,		/*27*/
	  nullop,	enodev,		nullop,		NULL,	/* mse */
	  mseselect,	enodev,		NULL },
	{ sioopen,	sioclose,	sioread,	siowrite,	/*28*/
	  sioioctl,	siostop,	sioreset,	sio_tty, /* sio */
	  sioselect,	enodev,		NULL },
	{ mcdopen,	mcdclose,	rawread,	enodev,		/*29*/
	  mcdioctl,	enodev,		nullop,		NULL,	/* mitsumi cd */
	  seltrue,	enodev,		mcdstrategy },
	{ sndopen,	sndclose,	sndread,	sndwrite,	/*30*/
  	  sndioctl,	enodev,		enodev,		NULL,	/* sound driver */
  	  sndselect,	enodev,		NULL },
	{ ukopen,	ukclose,	enxio,          enxio,      	/*31*/
	  ukioctl,	enxio,          enxio,          NULL,	/* unknown */
	  enxio,        enxio,          enxio },		/* scsi */
/*
 * If you need a cdev major number, please contact the FreeBSD team
 * by sending mail to `freebsd-hackers@freefall.cdrom.com'.
 * If you assign one yourself it may then conflict with someone else.
 */
};
int	nchrdev = sizeof (cdevsw) / sizeof (cdevsw[0]);

int	mem_no = 2; 	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(1, 0);
