/*
 * Copyright (c) UNIX System Laboratories, Inc.  All or some portions
 * of this file are derived from material licensed to the
 * University of California by American Telephone and Telegraph Co.
 * or UNIX System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 */

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
 *	$Id: conf.c,v 1.106 1995/11/11 05:10:48 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/conf.h>

#ifdef JREMOD

#define NUMCDEV 96
#define NUMBDEV 32

struct bdevsw	bdevsw[NUMBDEV];
int	nblkdev = NUMBDEV
struct cdevsw	cdevsw[NUMCDEV];
int	nchrdev = NUMCDEV

#else /*JREMOD*/
/* Bogus defines for compatibility. */
#define	noioc		noioctl
#define	nostrat		nostrategy
#define zerosize	nopsize

/* Lots of bogus defines for shorthand purposes */
int lkmenodev();
#define	lkmopen		(d_open_t *)lkmenodev
#define	lkmclose	(d_close_t *)lkmenodev
#define lkmread		(d_rdwr_t *)lkmenodev
#define lkmwrite	(d_rdwr_t *)lkmenodev
#define	lkmstrategy	(d_strategy_t *)lkmenodev
#define	lkmioctl	(d_ioctl_t *)lkmenodev
#define	lkmdump		(d_dump_t *)lkmenodev
#define	lkmsize		zerosize
#define lkmstop		(d_stop_t *)lkmenodev
#define lkmreset	(d_reset_t *)lkmenodev
#define lkmmmap		(d_mmap_t *)lkmenodev
#define lkmselect	(d_select_t *)lkmenodev

#include "wd.h"
#if NWD == 0
#define	wdopen		nxopen
#define	wdclose		nxclose
#define	wdstrategy	nxstrategy
#define	wdioctl		nxioctl
#define	wddump		nxdump
#define	wdsize		zerosize
#endif

#include "worm.h"
#if NWORM == 0
#define	wormopen		nxopen
#define	wormclose		nxclose
#define	wormstrategy	nxstrategy
#define	wormioctl		nxioctl
#define	wormdump		nxdump
#define	wormsize		zerosize
#endif

#include "sctarg.h"
#if NSCTARG == 0
#define	sctargopen		nxopen
#define	sctargclose		nxclose
#define	sctargstrategy	nxstrategy
#define	sctargioctl		nxioctl
#define	sctargdump		nxdump
#define	sctargsize		zerosize
#endif

#include "pt.h"
#if NPT == 0
#define	ptopen		nxopen
#define	ptclose		nxclose
#define	ptstrategy	nxstrategy
#define	ptioctl		nxioctl
#define	ptdump		nxdump
#define	ptsize		zerosize
#endif

#include "sd.h"
#if NSD == 0
#define	sdopen		nxopen
#define	sdclose		nxclose
#define	sdstrategy	nxstrategy
#define	sdioctl		nxioctl
#define	sddump		nxdump
#define	sdsize		zerosize
#endif

#include "st.h"
#if NST == 0
#define	stopen		nxopen
#define	stclose		nxclose
#define	ststrategy	nxstrategy
#define	stioctl		nxioctl
#endif

#include "od.h"
#if NOD == 0
#define	odopen		nxopen
#define	odclose		nxclose
#define	odstrategy	nxstrategy
#define	odioctl		nxioctl
#define	odsize		zerosize
#endif

#include "cd.h"
#if NCD == 0
#define	cdopen		nxopen
#define	cdclose		nxclose
#define	cdstrategy	nxstrategy
#define	cdioctl		nxioctl
#define	cdsize		zerosize
#endif

#include "mcd.h"
#if NMCD == 0
#define	mcdopen		nxopen
#define	mcdclose	nxclose
#define	mcdstrategy	nxstrategy
#define	mcdioctl	nxioctl
#define	mcdsize		zerosize
#endif

#include "scd.h"
#if NSCD == 0
#define	scdopen		nxopen
#define	scdclose	nxclose
#define	scdstrategy	nxstrategy
#define	scdioctl	nxioctl
#define	scdsize		zerosize
#endif

#include "matcd.h"
#if NMATCD == 0
#define	matcdopen	nxopen
#define	matcdclose	nxclose
#define	matcdstrategy	nxstrategy
#define	matcdioctl	nxioctl
#define	matcdsize	zerosize
#endif

#include "ata.h"
#if NATA == 0
#define	ataopen		nxopen
#define	ataclose	nxclose
#define	atastrategy	nxstrategy
#define	ataioctl	nxioctl
#define	atasize		zerosize
#endif

#include "wcd.h"
#if NWCD == 0
#define wcdbopen	nxopen
#define wcdropen	nxopen
#define wcdbclose	nxclose
#define wcdrclose	nxclose
#define wcdstrategy	nxstrategy
#define wcdioctl	nxioctl
#endif

#include "ch.h"
#if NCH == 0
#define	chopen		nxopen
#define	chclose		nxclose
#define	chioctl		nxioctl
#endif

#include "wt.h"
#if NWT == 0
#define	wtopen		nxopen
#define	wtclose		nxclose
#define	wtstrategy	nxstrategy
#define	wtioctl		nxioctl
#define	wtdump		nxdump
#define	wtsize		zerosize
#endif

#include "fd.h"
#if NFD == 0
#define	Fdopen		nxopen
#define	fdclose		nxclose
#define	fdstrategy	nxstrategy
#define	fdioctl		nxioctl
#endif

#include "vn.h"
#if NVN == 0
#define	vnopen		nxopen
#define	vnclose		nxclose
#define	vnstrategy	nxstrategy
#define	vnioctl		nxioctl
#define	vndump		nxdump
#define	vnsize		zerosize
#endif

#include "meteor.h"
#if NMETEOR == 0
#define meteor_open     nxopen
#define meteor_close    nxclose 
#define meteor_read     nxread
#define meteor_write    nxwrite
#define meteor_ioctl    nxioctl
#define meteor_mmap     nxmmap
#endif

struct bdevsw	bdevsw[] =
{
	{ wdopen,	wdclose,	wdstrategy,	wdioctl,	/*0*/
	  wddump,	wdsize,		0 },
	{ noopen,	noclose,	swstrategy,	noioc,		/*1*/
	  nodump,	zerosize,	0 },
	{ Fdopen,	fdclose,	fdstrategy,	fdioctl,	/*2*/
	  nxdump,	zerosize,	0 },
	{ wtopen,	wtclose,	wtstrategy,	wtioctl,	/*3*/
	  wtdump,	wtsize,		B_TAPE },
	{ sdopen,	sdclose,	sdstrategy,	sdioctl,	/*4*/
	  sddump,	sdsize,		0 },
	{ stopen,	stclose,	ststrategy,	stioctl,	/*5*/
	  nxdump,	zerosize,	0 },
	{ cdopen,	cdclose,	cdstrategy,	cdioctl,	/*6*/
	  nxdump,	cdsize,		0 },
	{ mcdopen,	mcdclose,	mcdstrategy,	mcdioctl,	/*7*/
	  nxdump,	mcdsize,	0 },
	{ lkmopen,	lkmclose,	lkmstrategy,	lkmioctl,	/*8*/
	  lkmdump,	lkmsize,	NULL },
	{ lkmopen,	lkmclose,	lkmstrategy,	lkmioctl,	/*9*/
	  lkmdump,	lkmsize,	NULL },
	{ lkmopen,	lkmclose,	lkmstrategy,	lkmioctl,	/*10*/
	  lkmdump,	lkmsize,	NULL },
	{ lkmopen,	lkmclose,	lkmstrategy,	lkmioctl,	/*11*/
	  lkmdump,	lkmsize,	NULL },
	{ lkmopen,	lkmclose,	lkmstrategy,	lkmioctl,	/*12*/
	  lkmdump,	lkmsize,	NULL },
	{ lkmopen,	lkmclose,	lkmstrategy,	lkmioctl,	/*13*/
	  lkmdump,	lkmsize,	NULL },
	/* block device 14 is reserved for local use */
	{ nxopen,	nxclose,	nxstrategy,	nxioctl,	/*14*/
	  nxdump,	zerosize,	NULL },
	{ vnopen,	vnclose,	vnstrategy,	vnioctl,	/*15*/
	  vndump,	vnsize,		0 },
	{ scdopen,	scdclose,	scdstrategy,	scdioctl,	/*16*/
	  nxdump,	scdsize,	0 },
	{ matcdopen,	matcdclose,	matcdstrategy,	matcdioctl,	/*17*/
	  nxdump,	matcdsize,	0 },
	{ ataopen,	ataclose,	atastrategy,	ataioctl,	/*18*/
	  nxdump,	atasize,	0 },
	{ wcdbopen,	wcdbclose,	wcdstrategy,	wcdioctl,	/*19*/
	  nxdump,	zerosize,	0 },
	{ odopen,	odclose,	odstrategy,	odioctl,	/*20*/
	  nxdump,	odsize,		0 },

/*
 * If you need a bdev major number for a driver that you intend to donate
 * back to the group or release publically, please contact the FreeBSD team
 * by sending mail to "FreeBSD-hackers@freefall.cdrom.com".
 * If you assign one yourself it may conflict with someone else.
 * Otherwise, simply use the one reserved for local use.
 */
};
int	nblkdev = sizeof (bdevsw) / sizeof (bdevsw[0]);

/* console */
#include "machine/cons.h"

#include "pty.h"
#if NPTY == 0
#define ptsopen		nxopen
#define ptsclose	nxclose
#define ptsread		nxread
#define ptswrite	nxwrite
#define ptcopen		nxopen
#define ptcclose	nxclose
#define ptcread		nxread
#define ptcwrite	nxwrite
#define ptyioctl	nxioctl
#define	ptcselect	nxselect
#define	ptsstop		nullstop
#define	ptydevtotty	nxdevtotty
#endif


#include "snp.h"
#if NSNP == 0
#define snpopen		nxopen
#define snpclose	nxclose
#define snpread		nxread
#define snpwrite	nxwrite
#define snpioctl	nxioctl
#define	snpselect	nxselect
#endif

#include "bqu.h"
#if NBQU == 0
#define bquopen         nxopen
#define bquclose        nxclose
#define bquread         nxread
#define bquwrite        nxwrite
#define bquselect       nxselect
#define bquioctl        nxioctl
#endif

#include "lpt.h"
#if NLPT == 0
#define	lptopen		nxopen
#define	lptclose	nxclose
#define	lptwrite	nxwrite
#define	lptioctl	nxioctl
#endif

#include "tw.h"
#if NTW == 0
#define twopen		nxopen
#define twclose		nxclose
#define twread		nxread
#define twwrite		nxwrite
#define twselect	nxselect
#define	twdevtotty	nxdevtotty
#endif

#include "psm.h"
#if NPSM == 0
#define psmopen		nxopen
#define psmclose	nxclose
#define psmread		nxread
#define psmselect	nxselect
#define psmioctl	nxioctl
#endif

#include "snd.h"
#if NSND == 0
#define sndopen         nxopen
#define sndclose        nxclose
#define sndioctl       	nxioctl
#define sndread         nxread
#define sndwrite        nxwrite
#define sndselect       seltrue
#endif

#include "bpfilter.h"
#if NBPFILTER == 0
#define	bpfopen		nxopen
#define	bpfclose	nxclose
#define	bpfread		nxread
#define	bpfwrite	nxwrite
#define	bpfselect	nxselect
#define	bpfioctl	nxioctl
#endif

#include "speaker.h"
#if NSPEAKER == 0
#define spkropen	nxopen
#define spkrclose	nxclose
#define spkrwrite	nxwrite
#define spkrioctl	nxioctl
#endif

#include "pca.h"
#if NPCA == 0
#define pcaopen		nxopen
#define pcaclose	nxclose
#define pcawrite	nxwrite
#define pcaioctl	nxioctl
#define pcaselect	nxselect
#endif

#include "mse.h"
#if NMSE == 0
#define	mseopen		nxopen
#define	mseclose	nxclose
#define	mseread		nxread
#define	mseselect	nxselect
#endif

#include "sio.h"
#if NSIO == 0
#define sioopen		nxopen
#define sioclose	nxclose
#define sioread		nxread
#define siowrite	nxwrite
#define sioioctl	nxioctl
#define siostop		nxstop
#define	siodevtotty	nxdevtotty
#endif

#include "su.h"
#if NSU == 0
#define	suopen		nxopen
#define	suclose		nxclose
#define	suioctl		nxioctl
#define	suread		nxread
#define	suwrite		nxwrite
#define	suselect	nxselect
#define	sustrategy	nxstrategy
#endif

#include "scbus.h"
#if NSCBUS == 0
#define	ukopen		nxopen
#define	ukclose		nxclose
#define	ukioctl		nxioctl
#endif

#include "apm.h"
#if NAPM == 0
#define	apmopen		nxopen
#define	apmclose	nxclose
#define	apmioctl	nxioctl
#endif

#include "ctx.h"
#if NCTX == 0
#define ctxopen		nxopen
#define ctxclose	nxclose
#define ctxread		nxread
#define ctxwrite	nxwrite
#define ctxioctl	nxioctl
#endif

#include "ssc.h"
#if NSSC == 0
#define	sscopen		nxopen
#define	sscclose	nxclose
#define	sscioctl	nxioctl
#define	sscread		nxread
#define	sscwrite	nxwrite
#define	sscselect	nxselect
#define	sscstrategy	nxstrategy
#endif

#include "cx.h"
#if NCX == 0
#define cxopen		nxopen
#define cxclose		nxclose
#define cxread		nxread
#define cxwrite		nxwrite
#define cxioctl		nxioctl
#define cxstop		nxstop
#define cxselect	nxselect
#define	cxdevtotty	nxdevtotty
#endif

#include "gp.h"
#if NGP == 0
#define gpopen  	nxopen
#define gpclose 	nxclose
#define gpwrite 	nxwrite
#define gpioctl 	nxioctl
#endif

#include "gsc.h"
#if NGSC == 0
#define gscopen		nxopen
#define gscclose	nxclose
#define gscread		nxread
#define gscioctl	nxioctl
#endif

#include "crd.h"
#if NCRD == 0
#define crdopen		nxopen
#define crdclose	nxclose
#define crdread		nxread
#define crdwrite	nxwrite
#define	crdioctl	nxioctl
#define crdselect	nxselect

#endif

#include "joy.h"
#if NJOY == 0
#define joyopen		nxopen
#define joyclose	nxclose
#define joyread		nxread
#define	joyioctl	nxioctl
#endif

#include "asc.h"
#if NASC == 0
#define ascopen               nxopen
#define ascclose      nxclose
#define ascread               nxread
#define ascioctl      nxioctl
#define ascselect       nxselect
#endif

#include "tun.h"
#if NTUN == 0
#define tunopen         nxopen
#define tunclose        nxclose
#define tunread         nxread
#define tunwrite        nxwrite
#define tunioctl        nxioctl
#define tunselect       nxselect
#endif

#include "spigot.h"
#if NSPIGOT == 0
#define spigot_open     nxopen
#define spigot_close    nxclose
#define spigot_ioctl    nxioctl
#define spigot_read     nxread
#define spigot_write    nxwrite
#define spigot_select   seltrue
#define spigot_mmap     nommap
#endif

#include "cy.h"
#if NCY == 0
#define	cyopen		nxopen
#define cyclose		nxclose
#define cyread		nxread
#define cywrite		nxwrite
#define cyioctl		nxioctl
#define cystop		nxstop
#define	cydevtotty	nxdevtotty
#endif

#include "dgb.h"      
#if NDGB == 0
#define dgbopen		nxopen
#define dgbclose	nxclose
#define dgbread		nxread
#define dgbwrite	nxwrite
#define dgbioctl	nxioctl
#define dgbstop		nxstop
#define dgbdevtotty	nxdevtotty
#endif

#include "si.h"
#if NSI == 0
#define	siopen		nxopen
#define siclose		nxclose
#define siread		nxread
#define siwrite		nxwrite
#define siioctl		nxioctl
#define sistop		nxstop
#define	sidevtotty	nxdevtotty
#endif

#include "ity.h"
#if NITY == 0
#define ityopen		nxopen
#define ityclose	nxclose
#define ityread		nxread
#define itywrite	nxwrite
#define ityioctl	nxioctl
#define	itydevtotty	nxdevtotty
#endif

#include "nic.h"
#if NNIC == 0
#define nicopen		nxopen
#define nicclose	nxclose
#define nicioctl	nxioctl
#endif

#include "nnic.h"
#if NNNIC == 0
#define nnicopen        nxopen
#define nnicclose       nxclose
#define nnicioctl       nxioctl
#endif

#include "isdn.h"
#if NISDN == 0
#define isdnopen	nxopen
#define isdnclose	nxclose
#define isdnread	nxread
#define isdnioctl	nxioctl
#endif

#include "itel.h"
#if NITEL == 0
#define itelopen	nxopen
#define itelclose	nxclose
#define itelread	nxread
#define itelwrite	nxwrite
#define itelioctl	nxioctl
#endif

#include "ispy.h"
#if NISPY == 0
#define ispyopen        nxopen
#define ispyclose       nxclose
#define ispyread        nxread
#define ispywrite       nxwrite
#define ispyioctl       nxioctl
#endif

#include "rc.h"
#if NRC == 0
#define rcopen         nxopen
#define rcclose        nxclose
#define rcread         nxread
#define rcwrite        nxwrite
#define rcioctl        nxioctl
#define rcstop         nxstop
#define rcdevtotty     nxdevtotty
#endif

#include "labpc.h"
#if NLABPC == 0
#define	labpcopen		nxopen
#define	labpcclose		nxclose
#define	labpcstrategy	nxstrategy
#define	labpcioctl		nxioctl
#endif

/* open, close, read, write, ioctl, stop, reset, ttys, select, mmap, strat */
struct cdevsw	cdevsw[] =
{
	{ cnopen,	cnclose,	cnread,		cnwrite,	/*0*/
	  cnioctl,	nullstop,	nullreset,	nodevtotty,/* console */
	  cnselect,	nommap,		NULL },
	{ cttyopen,	nullclose,	cttyread,	cttywrite,	/*1*/
	  cttyioctl,	nullstop,	nullreset,	nodevtotty,/* tty */
	  cttyselect,	nommap,		NULL },
	{ mmopen,	mmclose,	mmrw,		mmrw,		/*2*/
	  mmioctl,	nullstop,	nullreset,	nodevtotty,/* memory */
	  seltrue,	memmmap,	NULL },
	{ wdopen,	wdclose,	rawread,	rawwrite,	/*3*/
	  wdioctl,	nostop,		nullreset,	nodevtotty,/* wd */
	  seltrue,	nommap,		wdstrategy },
	{ nullopen,	nullclose,	rawread,	rawwrite,	/*4*/
	  noioc,	nostop,		noreset,	nodevtotty,/* swap */
	  noselect,	nommap,		swstrategy },
	{ ptsopen,	ptsclose,	ptsread,	ptswrite,	/*5*/
	  ptyioctl,	ptsstop,	nullreset,	ptydevtotty,/* ttyp */
	  ttselect,	nommap,		NULL },
	{ ptcopen,	ptcclose,	ptcread,	ptcwrite,	/*6*/
	  ptyioctl,	nullstop,	nullreset,	ptydevtotty,/* ptyp */
	  ptcselect,	nommap,		NULL },
	{ logopen,	logclose,	logread,	nowrite,	/*7*/
	  logioctl,	nostop,		nullreset,	nodevtotty,/* klog */
	  logselect,	nommap,		NULL },
	{ bquopen,      bquclose,       bquread,        bquwrite,       /*8*/
	  bquioctl,     nostop,         nullreset,      nodevtotty,/* tputer */
	  bquselect,    nommap,         NULL },
	{ Fdopen,	fdclose,	rawread,	rawwrite,	/*9*/
	  fdioctl,	nostop,		nullreset,	nodevtotty,/* Fd (!=fd) */
	  seltrue,	nommap,		fdstrategy },
	{ wtopen,	wtclose,	rawread,	rawwrite,	/*10*/
	  wtioctl,	nostop,		nullreset,	nodevtotty,/* wt */
	  seltrue,	nommap,		wtstrategy },
	{ spigot_open,	spigot_close,	spigot_read,	spigot_write,	/*11*/
	  spigot_ioctl,	nostop,		nullreset,	nodevtotty,/* Spigot */
	  spigot_select, spigot_mmap,	NULL },
	{ nxopen,	nxclose,	nxread,		nxwrite,	/*12*/
	  nxioctl,	nxstop,		nxreset,	nxdevtotty,/* sc, ... */
	  nxselect,	nxmmap,		NULL },
	{ sdopen,	sdclose,	rawread,	rawwrite,	/*13*/
	  sdioctl,	nostop,		nullreset,	nodevtotty,/* sd */
	  seltrue,	nommap,		sdstrategy },
	{ stopen,	stclose,	rawread,	rawwrite,	/*14*/
	  stioctl,	nostop,		nullreset,	nodevtotty,/* st */
	  seltrue,	nommap,		ststrategy },
	{ cdopen,	cdclose,	rawread,	nowrite,	/*15*/
	  cdioctl,	nostop,		nullreset,	nodevtotty,/* cd */
	  seltrue,	nommap,		cdstrategy },
	{ lptopen,	lptclose,	noread,		lptwrite,	/*16*/
	  lptioctl,	nullstop,	nullreset,	nodevtotty,/* lpt */
	  seltrue,	nommap,		nostrat},
	{ chopen,	chclose,	noread,		nowrite,	/*17*/
	  chioctl,	nostop,		nullreset,	nodevtotty,/* ch */
	  noselect,	nommap,		nostrat },
	{ suopen,	suclose,	suread,		suwrite,	/*18*/
	  suioctl,	nostop,		nullreset,	nodevtotty,/* scsi */
	  suselect,	nxmmap,		sustrategy },		/* 'generic' */
	{ twopen,	twclose,	twread,		twwrite,	/*19*/
	  noioc,	nullstop,	nullreset,	nodevtotty,/* tw */
	  twselect,	nommap,		nostrat },
/*
 * If you need a cdev major number for a driver that you intend to donate
 * back to the group or release publically, please contact the FreeBSD team
 * by sending mail to "hackers@freebsd.org".
 * If you assign one yourself it may conflict with someone else.
 * Otherwise, simply use the one reserved for local use.
 */
	/* character device 20 is reserved for local use */
	{ nxopen,	nxclose,	nxread,		nxwrite,	/*20*/
	  nxioctl,	nxstop,		nxreset,	nxdevtotty,/* reserved */
	  nxselect,	nxmmap,		NULL },
	{ psmopen,	psmclose,	psmread,	nowrite,	/*21*/
	  psmioctl,	nostop,		nullreset,	nodevtotty,/* psm mice */
	  psmselect,	nommap,		NULL },
	{ fdopen,	noclose,	noread,		nowrite,	/*22*/
	  noioc,	nostop,		nullreset,	nodevtotty,/* fd (!=Fd) */
	  noselect,	nommap,		nostrat },
 	{ bpfopen,	bpfclose,	bpfread,	bpfwrite,	/*23*/
 	  bpfioctl,	nostop,		nullreset,	nodevtotty,/* bpf */
 	  bpfselect,	nommap,		NULL },
 	{ pcaopen,      pcaclose,       noread,         pcawrite,       /*24*/
 	  pcaioctl,     nostop,         nullreset,      nodevtotty,/* pcaudio */
 	  pcaselect,	nommap,		NULL },
	{ nxopen,	nxclose,	nxread,		nxwrite,	/*25*/
	  nxioctl,	nxstop,		nxreset,	nxdevtotty,/* was vat */
	  nxselect,	nxmmap,		NULL },
	{ spkropen,     spkrclose,      noread,         spkrwrite,      /*26*/
	  spkrioctl,    nostop,         nullreset,      nodevtotty,/* spkr */
	  seltrue,	nommap,		NULL },
	{ mseopen,	mseclose,	mseread,	nowrite,	/*27*/
	  noioc,	nostop,		nullreset,	nodevtotty,/* mse */
	  mseselect,	nommap,		NULL },
	{ sioopen,	sioclose,	sioread,	siowrite,	/*28*/
	  sioioctl,	siostop,	nxreset,	siodevtotty,/* sio */
	  ttselect,	nommap,		NULL },
	{ mcdopen,	mcdclose,	rawread,	nowrite,	/*29*/
	  mcdioctl,	nostop,		nullreset,	nodevtotty,/* mitsumi cd */
	  seltrue,	nommap,		mcdstrategy },
	{ sndopen,	sndclose,	sndread,	sndwrite,	/*30*/
  	  sndioctl,	nostop,		nullreset,	nodevtotty,/* sound */
  	  sndselect,	nommap,		NULL },
	{ ukopen,	ukclose,	noread,         nowrite,      	/*31*/
	  ukioctl,	nostop,		nullreset,	nodevtotty,/* unknown */
	  seltrue,	nommap,		NULL },			   /* scsi */
	{ lkmcopen,	lkmcclose,	noread,		nowrite,	/*32*/
	  lkmcioctl,	nostop,		nullreset,	nodevtotty,
	  noselect,	nommap,		NULL },
	{ lkmopen,	lkmclose,	lkmread,	lkmwrite,	/*33*/
	  lkmioctl,	lkmstop,	lkmreset,	nodevtotty,
	  lkmselect,	lkmmmap,	NULL },
	{ lkmopen,	lkmclose,	lkmread,	lkmwrite,	/*34*/
	  lkmioctl,	lkmstop,	lkmreset,	nodevtotty,
	  lkmselect,	lkmmmap,	NULL },
	{ lkmopen,	lkmclose,	lkmread,	lkmwrite,	/*35*/
	  lkmioctl,	lkmstop,	lkmreset,	nodevtotty,
	  lkmselect,	lkmmmap,	NULL },
	{ lkmopen,	lkmclose,	lkmread,	lkmwrite,	/*36*/
	  lkmioctl,	lkmstop,	lkmreset,	nodevtotty,
	  lkmselect,	lkmmmap,	NULL },
	{ lkmopen,	lkmclose,	lkmread,	lkmwrite,	/*37*/
	  lkmioctl,	lkmstop,	lkmreset,	nodevtotty,
	  lkmselect,	lkmmmap,	NULL },
	{ lkmopen,	lkmclose,	lkmread,	lkmwrite,	/*38*/
	  lkmioctl,	lkmstop,	lkmreset,	nodevtotty,
	  lkmselect,	lkmmmap,	NULL },
	{ apmopen,	apmclose,	noread,		nowrite,	/*39*/
	  apmioctl,	nostop,		nullreset,	nodevtotty,/* APM */
	  seltrue,	nommap,		NULL },
	{ ctxopen,	ctxclose,	ctxread,	ctxwrite,	/*40*/
	  ctxioctl,	nostop,		nullreset,	nodevtotty,/* cortex */
	  seltrue,	nommap,		NULL },
	{ nxopen,	nxclose,	nxread,		nxwrite,	/*41*/
	  nxioctl,	nxstop,		nullreset,	nxdevtotty,/* was socksys */
	  seltrue,	nommap,		NULL },
	{ cxopen,	cxclose,	cxread,		cxwrite,	/*42*/
	  cxioctl,	cxstop,		nullreset,	cxdevtotty,/* cronyx */
	  cxselect,	nommap,		NULL },
	{ vnopen,	vnclose,	rawread,	rawwrite,	/*43*/
	  vnioctl,	nostop,		nullreset,	nodevtotty,/* vn */
	  seltrue,	nommap,		vnstrategy },
	{ gpopen,	gpclose,	noread,		gpwrite,	/*44*/
	  gpioctl,	nostop,		nullreset,	nodevtotty,/* GPIB */
          seltrue,	nommap,		NULL },
	{ scdopen,	scdclose,	rawread,	nowrite,	/*45*/
	  scdioctl,	nostop,		nullreset,	nodevtotty,/* sony cd */
	  seltrue,	nommap,		scdstrategy },
	{ matcdopen,	matcdclose,	rawread,	nowrite,	/*46*/
	  matcdioctl,	nostop,		nullreset,	nodevtotty,/* SB cd */
	  seltrue,	nommap,		matcdstrategy },
	{ gscopen,      gscclose,       gscread,        nowrite,	/*47*/
	  gscioctl,     nostop,         nullreset,      nodevtotty,/* gsc */
	  seltrue,      nommap,         NULL },
	{ cyopen,	cyclose,	cyread,		cywrite,	/*48*/
	  cyioctl,	cystop,		nxreset,	cydevtotty,/*cyclades*/
	  ttselect,	nxmmap,		NULL },
	{ sscopen,	sscclose,	sscread,	sscwrite,	/*49*/
	  sscioctl,	nostop,		nullreset,	nodevtotty,/* scsi super */
	  sscselect,	nxmmap,		sscstrategy },
	{ crdopen,	crdclose,	crdread,	crdwrite,	/*50*/
	  crdioctl,	nostop,		nullreset,	nodevtotty,/* pcmcia */
	  crdselect,	nommap,		NULL },
	{ joyopen,	joyclose,	joyread,	nowrite,	/*51*/
	  joyioctl,	nostop,		nullreset,	nodevtotty,/*joystick */
	  seltrue,	nommap,		NULL},
	{ tunopen,      tunclose,       tunread,        tunwrite,       /*52*/
	  tunioctl,     nostop,         nullreset,      nodevtotty,/* tunnel */
	  tunselect,    nommap,         NULL },
	{ snpopen,	snpclose,	snpread,	snpwrite,	/*53*/
	  snpioctl,	nostop,		nullreset,	nodevtotty,/* snoop */
	  snpselect,	nommap,		NULL },
	{ nicopen,	nicclose,	noread,		nowrite,	/*54*/
	  nicioctl,	nostop,		nullreset,	nodevtotty,/* nic */
	  seltrue,	nommap,		NULL },
	{ isdnopen,	isdnclose,	isdnread,	nowrite,	/*55*/
	  isdnioctl,	nostop,		nullreset,	nodevtotty,/* isdn */
	  seltrue,	nommap,		NULL },
	{ ityopen,	ityclose,	ityread,	itywrite,	/*56*/
	  ityioctl,	nostop,		nxreset,	itydevtotty,/* ity */
	  ttselect,	nommap,		NULL },
	{ itelopen,	itelclose,	itelread,	itelwrite,	/*57*/
	  itelioctl,	nostop,		nullreset,	nodevtotty,/* itel */
	  seltrue,	nommap,		NULL },
	{ dgbopen,	dgbclose,	dgbread,	dgbwrite,	/*58*/
	  dgbioctl,	dgbstop,	nxreset,	dgbdevtotty, /* dgb */
	  ttselect,	nommap,		NULL },
	{ ispyopen,	ispyclose,	ispyread,	nowrite,	/*59*/
	  ispyioctl,	nostop,		nullreset,	nodevtotty,/* ispy */
	  seltrue,	nommap,         NULL },
	{ nnicopen,	nnicclose,	noread,		nowrite,	/*60*/
	  nnicioctl,	nostop,		nullreset,	nodevtotty,/* nnic */
	  seltrue,	nommap,		NULL },
	{ ptopen,	ptclose,	rawread,	rawwrite,	/*61*/
	  ptioctl,	nostop,		nullreset,	nodevtotty,/* pt */
	  seltrue,	nommap,		ptstrategy },
	{ wormopen,	wormclose,	rawread,	rawwrite,	/*62*/
	  wormioctl,	nostop,		nullreset,	nodevtotty,/* worm */
	  seltrue,	nommap,		wormstrategy },
	{ rcopen,       rcclose,        rcread,         rcwrite,        /*63*/
	  rcioctl,      rcstop,         nxreset,        rcdevtotty,/* rc */
	  ttselect,	nommap,		NULL },
	{ nxopen,	nxclose,	nxread,		nxwrite,	/*64*/
	  nxioctl,	nxstop,		nxreset,	nxdevtotty,/* Talisman */
	  nxselect,	nxmmap,		NULL },
	{ sctargopen,	sctargclose,	rawread,	rawwrite,	/*65*/
	  sctargioctl,	nostop,		nullreset,	nodevtotty,/* sctarg */
	  seltrue,	nommap,		sctargstrategy },
	{ labpcopen,	labpcclose,	rawread,	rawwrite,	/*66*/
	  labpcioctl,	nostop,		nullreset,	nodevtotty,/* labpc */
	  seltrue,	nommap,		labpcstrategy },
        { meteor_open,  meteor_close,   meteor_read,    meteor_write,   /*67*/
          meteor_ioctl, nostop,         nullreset,      nodevtotty,/* Meteor */
          seltrue, meteor_mmap, NULL },
	{ siopen,	siclose,	siread,		siwrite,	/*68*/
	  siioctl,	sistop,		nxreset,	sidevtotty,/* slxos */
	  ttselect,	nxmmap,		NULL },
	{ wcdropen,	wcdrclose,	rawread,	nowrite,	/*69*/
	  wcdioctl,	nostop,		nullreset,	nodevtotty,/* atapi */
	  seltrue,	nommap,		wcdstrategy },
	{ odopen,	odclose,	rawread,	rawwrite,	/*70*/
	  odioctl,	nostop,		nullreset,	nodevtotty,/* od */
	  seltrue,	nommap,		odstrategy },
	{ ascopen,      ascclose,       ascread,        nowrite,        /*71*/
	  ascioctl,     nostop,         nullreset,      nodevtotty, /* asc */   
	  ascselect,    nommap,         NULL }

};
int	nchrdev = sizeof (cdevsw) / sizeof (cdevsw[0]);
#endif /*JREMOD*/
/*
 * The routines below are total "BULLSHIT" and will be trashed
 * When I have 'proved' the JREMOD changes above..
 */

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

/*
 * The tunnel device's LKM wants to know where to install itself in the
 * cdevsw table.  Sigh.
 */
dev_t	tuncdev = makedev(52, 0);

/*
 * Routine that identifies /dev/mem and /dev/kmem.
 *
 * A minimal stub routine can always return 0.
 */
int
iskmemdev(dev)
	dev_t dev;
{

	return (major(dev) == 2 && (minor(dev) == 0 || minor(dev) == 1));
}

int
iszerodev(dev)
	dev_t dev;
{
	return (major(dev) == 2 && minor(dev) == 12);
}

/*
 * Routine to determine if a device is a disk.
 *
 * A minimal stub routine can always return 0.
 */
int
isdisk(dev, type)
	dev_t dev;
	int type;
{

	switch (major(dev)) {
	case 15:		/* VBLK: vn, VCHR: cd */
		return (1);
	case 0:			/* wd */
	case 2:			/* fd */
	case 4:			/* sd */
	case 6:			/* cd */
	case 7:			/* mcd */
	case 16:		/* scd */
	case 17:		/* matcd */
	case 18:		/* ata */
	case 19:		/* wcd */
	case 20:		/* od */
		if (type == VBLK)
			return (1);
		return (0);
	case 3:			/* wd */
	case 9:			/* fd */
	case 13:		/* sd */
	case 29:		/* mcd */
	case 43:		/* vn */
	case 45:		/* scd */
	case 46:		/* matcd */
	case 69:		/* wcd */
	case 70:		/* od */
		if (type == VCHR)
			return (1);
		/* fall through */
	default:
		return (0);
	}
	/* NOTREACHED */
}

/*
 * Routine to convert from character to block device number.
 *
 * A minimal stub routine can always return NODEV.
 */
dev_t
chrtoblk(dev)
	dev_t dev;
{
	int blkmaj;

	switch (major(dev)) {
	case 3:		blkmaj = 0;  break; /* wd */
	case 9:		blkmaj = 2;  break; /* fd */
	case 10:	blkmaj = 3;  break; /* wt */
	case 13:	blkmaj = 4;  break; /* sd */
	case 14:	blkmaj = 5;  break; /* st */
	case 15:	blkmaj = 6;  break; /* cd */
	case 29:	blkmaj = 7;  break; /* mcd */
	case 43:	blkmaj = 15; break; /* vn */
	case 45:	blkmaj = 16; break; /* scd */
	case 46:	blkmaj = 17; break; /* matcd */
	case 69:	blkmaj = 19; break; /* wcd */
	case 70:	blkmaj = 20; break; /* od */
	default:
		return (NODEV);
	}
	return (makedev(blkmaj, minor(dev)));
}

int
getmajorbyname(name)
	const char *name;
{

	if (strcmp(name, "sc") == 0)
		return (12);
	if (strcmp(name, "vt") == 0)
		return (12);
	return (NULL);
}


static struct cdevsw *getcdevbyname __P((const char *name));
static struct cdevsw *
getcdevbyname(name)
	const char *name;
{
	int maj;

	maj = getmajorbyname(name);
	return (maj < 0 ? NULL : &cdevsw[maj]);
}

int
register_cdev(name, cdp)
	const char *name;
	const struct cdevsw *cdp;
{
	struct cdevsw *dst_cdp;

	dst_cdp = getcdevbyname(name);
	if (dst_cdp == NULL)
		return (ENXIO);
#ifdef JREMOD
	if ((dst_cdp->d_open != nxopen) && (dst_cdp->d_open != NULL))
#else /*JREMOD*/
	if (dst_cdp->d_open != nxopen)
#endif /*JREMOD*/
		return (EBUSY);
	*dst_cdp = *cdp;
	return (0);
}

static struct cdevsw nxcdevsw = {
	nxopen,		nxclose,	nxread,		nxwrite,
	nxioctl,	nxstop,		nxreset,	nxdevtotty,
	nxselect,	nxmmap,		NULL,
};

int
unregister_cdev(name, cdp)
	const char *name;
	const struct cdevsw *cdp;
{
	struct cdevsw *dst_cdp;

	dst_cdp = getcdevbyname(name);
	if (dst_cdp == NULL)
		return (ENXIO);
	if (dst_cdp->d_open != cdp->d_open)
		return (EBUSY);
	*dst_cdp = nxcdevsw;
	return (0);
}
