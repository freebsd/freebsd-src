/*-
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
 *	@(#)conf.c	5.8 (Berkeley) 5/12/91
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         5       00160
 * --------------------         -----   ----------------------
 *
 * 10 Feb 93	Jordan K. Hubbard	Added select entry for com driver
 * 10 Feb 93    Julian Elischer		Add empty table entries
 *					so we can allocate numbers
 * 15 Feb 93    Julian Elischer		Add basic SCSI device entries
 * 16 Feb 93    Julian Elischer		add entries for scsi media changer
 * 01 Mar 93	Jordan K. Hubbard	Reserve major numbers for codrv, fd, bpf
 * 10 Mar 83	Rodney W. Grimes	General clean up of the above patches
 * 06 Apr 93	Rodney W. Grimes	Fixed NLPT for LPA driver case, added
 *					spkr, dcfclock
 * 23 Apr 93	Holger Veit		added codrv
 * 25 May 93	Bruce Evans		New fast interrupt serial driver (sio)
 * 		Gene Stark		Xten power controller info added (tw)
 *		Rick Macklem		Bus mouse driver (mse)
 *
 */
static char rcsid[] = "$Header: /usr/src/sys.386bsd/i386/i386/RCS/conf.c,v 1.2 92/01/21 14:21:57 william Exp Locker: toor $";

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "ioctl.h"
#include "tty.h"
#include "conf.h"

int	nullop(), enxio(), enodev(), rawread(), rawwrite(), swstrategy();
int	rawread(), rawwrite(), swstrategy();

#include "wd.h"
#if NWD > 0
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

#include "as.h"
#if NAS > 0
int	asopen(),asclose(),asstrategy(),asioctl();
int	/*asdump(),*/assize();
#define	asdump		enxio
#else
#define	asopen		enxio
#define	asclose		enxio
#define	asstrategy	enxio
#define	asioctl		enxio
#define	asdump		enxio
#define	assize		NULL
#endif

#include "sd.h"
#if NSD > 0
int	sdopen(),sdclose(),sdstrategy(),sdioctl();
int	/*sddump(),*/sdsize();
#define	sddump		enxio
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
int	Fdopen(),fdclose(),fdstrategy();
#define	fdioctl		enxio
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
#if NSD > 0
	{ sdopen,	sdclose,	sdstrategy,	sdioctl,	/*4*/
	  sddump,	sdsize,		NULL },
#else NSD > 0
	{ asopen,	asclose,	asstrategy,	asioctl,	/*4*/
	  asdump,	assize,		NULL },
#endif NSD > 0
	{ stopen,	stclose,	ststrategy,	stioctl,	/*5*/
	  stdump,	stsize,		NULL },
	{ cdopen,	cdclose,	cdstrategy,	cdioctl,	/*6*/
	  cddump,	cdsize,		NULL },
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

int 	mmrw();
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

#include "co.h"
#if NCO > 0
int	coopen(),coclose(),coread(),coioctl(),coselect(),comap();
#define pcmmap		comap
#else
#define coopen		enxio
#define coclose		enxio
#define coread		enxio
#define coioctl		enxio
#define coselect	enxio
#define comap		enxio
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

struct cdevsw	cdevsw[] =
{
	{ cnopen,	cnclose,	cnread,		cnwrite,	/*0*/
	  cnioctl,	nullop,		nullop,		NULL,	/* console */
	  cnselect,	enodev,		NULL },
	{ cttyopen,	nullop,		cttyread,	cttywrite,	/*1*/
	  cttyioctl,	nullop,		nullop,		NULL,	/* tty */
	  cttyselect,	enodev,		NULL },
        { nullop,       nullop,         mmrw,           mmrw,           /*2*/
          enodev,       nullop,         nullop,         NULL,	/* memory */
          mmselect,     enodev,         NULL },
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
#if	NSD > 0
	{ sdopen,	sdclose,	rawread,	rawwrite,	/*13*/
	  sdioctl,	enodev,		nullop,		NULL,	/* sd */
	  seltrue,	enodev,		sdstrategy },
#else	NSD > 0
	{ asopen,	asclose,	rawread,	rawwrite,	/*13*/
	  asioctl,	enodev,		nullop,		NULL,	/* as */
	  seltrue,	enodev,		asstrategy },
#endif	NSD > 0
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
	{ enxio,	enxio,		enxio,		enxio,		/*18*/
	  enxio,	enxio,		enxio,		NULL,	/* scsi generic */
	  enxio,	enxio,		enxio },
	{ twopen,	twclose,	twread,		twwrite,	/*19*/
	  enodev,	nullop,		nullop,		NULL,	/* tw */
	  twselect,	enodev,		enodev },
	{ enxio,	enxio,		enxio,		enxio,		/*20*/
	  enxio,	enxio,		enxio,		NULL,	/* soundblaster?*/
	  enxio,	enxio,		enxio },
	{ coopen,	coclose,	coread,		enxio,		/*21*/
	  coioctl,	nullop,		nullop,		NULL,	/* co */
	  coselect,	comap,		NULL },
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
/*
 * If you need a cdev major number, please contact the 386bsd patchkit 
 * coordinator by sending mail to "patches@cs.montana.edu".
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
