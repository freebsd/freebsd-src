/*matcd.c--------------------------------------------------------------------
	Matsushita(Panasonic) / Creative CD-ROM Driver	(matcd)
	Authored by Frank Durda IV

	Copyright 1994, 1995  Frank Durda IV.  All rights reserved.
	"FDIV" is a trademark of Frank Durda IV.


	Redistribution and use in source and binary forms, with or
	without modification, are permitted provided that the following
	conditions are met:
	1.  Redistributions of source code must retain the above copyright
	    notice positioned at the very beginning of this file without
	    modification, all copyright strings, all related programming
	    codes that display the copyright strings, this list of
	    conditions and the following disclaimer.
	2.  Redistributions in binary form must contain all copyright strings
	    and related programming code that display the copyright strings.
	3.  Redistributions in binary form must reproduce the above copyright
	    notice, this list of conditions and the following disclaimer in
	    the documentation and/or other materials provided with the
	    distribution.
	4.  All advertising materials mentioning features or use of this
	    software must display the following acknowledgement:
		"The Matsushita/Panasonic CD-ROM driver  was developed
		 by Frank Durda IV for use with "FreeBSD" and similar
		 operating systems."
	    "Similar operating systems" includes mainly non-profit oriented
	    systems for research and education, including but not restricted
	    to "NetBSD", "386BSD", and "Mach" (by CMU).  The wording of the
	    acknowledgement (in electronic form or printed text) may not be
	    changed without permission from the author.
	5.  Absolutely no warranty of function, fitness or purpose is made
	    by the author Frank Durda IV.
	6.  Neither the name of the author nor the name "FreeBSD" may
	    be used to endorse or promote products derived from this software
	    without specific prior written permission.
	    (The author can be reached at   bsdmail@nemesis.lonestar.org)
	7.  The product containing this software must meet all of these
	    conditions even if it is unsupported, not a complete system
	    and/or does not contain compiled code.
	8.  These conditions will be in force for the full life of the
	    copyright.  
	9.  If all the above conditions are met, modifications to other
	    parts of this file may be freely made, although any person
	    or persons making changes do not receive the right to add their
	    name or names to the copyright strings and notices in this
	    software.  Persons making changes are encouraged to insert edit
	    history in matcd.c and to put your name and details of the
	    change there.  
	10. You must have prior written permission from the author to
	    deviate from these terms.

	Vendors who produce product(s) containing this code are encouraged 
	(but not required) to provide copies of the finished product(s) to
	the author and to correspond with the author about development
	activity relating to this code.   Donations of development hardware
	and/or software are also welcome.  (This is one of the faster ways
	to get a driver developed for a device.)

 	THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY
 	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 	PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 	OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 	OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
Dedicated to:	My family and Max, my Golden Retriever

Thanks to:	Jordan Hubbard (jkh) for getting me ramped-up to 2.x system
		quickly enough to make the 2.1 release.  He put up with
		plenty of silly questions.

and 		The people who donated equipment and other material to make
		development of this driver possible.  Donations and
		sponsors for projects are appreciated.


-----------------------------------------------------------------------------
Edit History - (should be in sync with any source control log entries)

	Never seen one of these before?  Ok, here is how it works.
	Every time you change the code, you increment the edit number,
	that number over there in the <%d> and in the (%d) in the
	version string.  You never set this number lower than it is.
	Near, or preferably on lines that change, insert the edit
	number.  If there is a number there already, you can replace it
	with a newer one.

	In the edit history, start with the edit number, and a good
	description of what changes were made.  Then follow it with
	the date, your name and an EMAIL address where you can be reached.

	Please follow this practice; it helps leave understandable code in
	your wake.

	FYI, you have major and minor release codes.  These are numbered
	1 thru n.  Major feature additions should get a new major release
	number.  Minor releases start with a null and then letters
	A thru Z.  So  3A(456) is Major release 3, Minor release 1,
	Edit 456 (in Microsoft-ese that would be 03.01.456), and 5(731)
	is Major release 5, Minor release 0, Edit 731.  Typically only the
	author will change the major and minor release codes.

				EDIT edit Edit HISTORY history History

<1>	This initial version is to get basic filesystem I/O working
	using the SoundBlaster 16 interface.  The stand-alone adapter
	card doesn't work yet.
	December 1994  Frank Durda IV	bsdmail@nemesis.lonestar.org

<2>	Corrections to resolve a race condition when multiple drives
	on the same controller was active.  Fixed drive 1 & 2 swap
	problem.  See selectdrive().
	21-Jan-95  Frank Durda IV	bsdmail@nemesis.lonestar.org

<3>	Added automatic probing and support for all Creative Labs sound
	cards with the Creative/Panasonic interface and the stand-alone
	interface adapters.  See AUTOHUNT and FULLCONFIG conditionals
	for more information.
	21-Jan-95  Frank Durda IV	bsdmail@nemesis.lonestar.org

<4>	Rebundled debug conditionals.
	14-Feb-95  Frank Durda IV	bsdmail@nemesis.lonestar.org

<5>	Changes needed to work on FreeBSD 2.1.  Also added draincmd
	since some conditions cause the drive to produce surprise data.
	See setmode and draincmd
	19-Feb-95  Frank Durda IV	bsdmail@nemesis.lonestar.org

<6>	Got rid of some redundant error code by creating chk_error().
	Also built a nice generic buss-lock function.
	20-Feb-95  Frank Durda IV	bsdmail@nemesis.lonestar.org

<7>	Improved comments, general structuring.
	Fixed a problem with disc eject not working if LOCKDRIVE was set.
	Apparently the drive will reject an EJECT command if the drive
	is LOCKED.
	21-Feb-95  Frank Durda IV	bsdmail@nemesis.lonestar.org

Edit number code marking begins here - earlier edits were during development.

<8>	Final device name selected and actually made to compile under
	>2.0. For newer systems, it is "matcd", for older it is "mat".
	24-Feb-95  Frank Durda IV	bsdmail@nemesis.lonestar.org

<9>	Added some additional disk-related ioctl functions that didn't
	make it into earlier versions.  
	26-Feb-95  Frank Durda IV	bsdmail@nemesis.lonestar.org

<10>	Updated some conditionals so the code will compile under
	1.1.5.1, although this is not the supported platform.
	Also found that some other devices probe code was changing the
	settings for the port 0x302 debug board, so added code to set it
	to a sane state before we use it.
	26-Feb-95  Frank Durda IV	bsdmail@nemesis.lonestar.org


---------------------------------------------------------------------------*/

/*Match this format:		Version__dc(d)__dd-mmm-yy	*/
static char	MATCDVERSION[]="Version  1(10)  26-Feb-95";

/*	The following strings may not be changed*/
static char	MATCDCOPYRIGHT[] = "Matsushita CD-ROM driver, Copr. 1994,1995 Frank Durda IV";
/*	The proceeding strings may not be changed*/


/*---------------------------------------------------------------------------
	Include declarations
---------------------------------------------------------------------------*/

#include	"types.h"
#include	"param.h"
#include	"systm.h"

#include	"buf.h"
#include	"dkbad.h"
#include	"cdio.h"
#include	"conf.h"
#include	"disklabel.h"
#include	"errno.h"
#include	"file.h"
#include	"i386/isa/isa.h"
#include	"i386/isa/isa_device.h"
#include	"ioctl.h"
#include	"stat.h"
#include	"uio.h"

#include	"options.h"		/*Conditional compile options and
					  probe port hints*/
#include	"matcd.h"		/*Drive-related defines and strings*/
#include	"creative.h"		/*Host interface related defines*/


/*---------------------------------------------------------------------------
	Defines and structures
---------------------------------------------------------------------------*/

#ifdef FULLCONFIG
#define NUMCTRLRS	4		/*With modern boards, four is max*/
#else /*FULLCONFIG*/
#define NUMCTRLRS	1		/*Produces a slightly smaller kernel*/
#endif	/*FULLCONFIG*/
#define DRIVESPERC	4		/*This is a constant*/
#define TOTALDRIVES	NUMCTRLRS*DRIVESPERC	/*Max possible drives*/
#if DIAGPORT > 0xff			/*<10>*/
#define DIAGOUT	outw			/*<10>*/
#else /*DIAGPORT*/			/*<10>*/
#define DIAGOUT	outb			/*<10>*/
#endif /*DIAGPORT*/			/*<10>*/
#ifdef DIAGPORT
int	diagloop;			/*Used to show looping*/
#endif /*DIAGPORT*/


#define	TICKRES		10		/*Our coarse timer resolution*/
#define ISABUSKHZ	8330		/*Number of IN/OUT ISA/sec*/

#ifndef FREE2
#define	RAW_PART	2		/*Needs to be defined in 1.1.5.1*/
#endif /*FREE2*/


#define MATCDBLK	2048		/*Standard block size*/
#define MATCDRBLK	2352		/*Raw and/or DA block size*/
#define	MATCD_RETRYS	5		/*Number of retries for read ops*/
#define	MATCD_READ_1	0x80		/*Read state machine defines*/
#define	MATCD_READ_2	0x90		/*Read state machine defines*/

struct matcd_volinfo {
	unsigned char	type;		/*00 CD-DA or CD-ROM
					  10 CD-I
					  20 XA */
	unsigned char	trk_low;	/*Normally 1*/
	unsigned char	trk_high;	/*Highest track number*/
	unsigned char	vol_msf[3];	/*Size of disc in min/sec/frame*/
};


struct	matcd_mbx {
	short	controller;
	short	ldrive;
	short	partition;
	short	port;
	short	retry;
	short	nblk;
	int	sz;
	u_long	skip;
	struct	buf	*bp;
	int	p_offset;
	short	count;
};


struct matcd_data {
	short	config;
	short	drivemode;		/*Last state drive was set to*/
	short	flags;
	short	status;
	int	blksize;
	u_long	disksize;
	int	iobase;
	struct	disklabel dlabel;
	int	partflags[MAXPARTITIONS];
	int	openflags;
	struct matcd_volinfo volinfo;
	short	debug;
	struct	matcd_mbx mbx;
} matcd_data[TOTALDRIVES];


/*	Bit equates for matcd_data.flags*/

#define	MATCDINIT	0x0001		/*Probe ran on host adapter*/
#define	MATCDLABEL	0x0004		/*Valid TOC exists*/
#define	MATCDWARN	0x0020		/*Have reported an open disc change*/


/*	Bit equates for matcd_data.partflags*/

#define	MATCDOPEN	0x0001
#define	MATCDREADRAW	0x0002


#define DELAY_STATUS	10000l		/* 10000 * 1us */
#define DELAY_GETREPLY	200000l		/* 200000 * 2us */
#define DELAY_SEEKREAD	20000l		/* 20000 * 1us */
#define matcd_delay	DELAY



/*	Error classes returned by chk_error()*/

#define ERR_RETRY	1	/*A retry might recover this*/
#define ERR_INIT	2	/*A retry almost certainly will get this*/
#define ERR_FATAL	3	/*This cannot be recovered from*/


struct	buf request_head[NUMCTRLRS];	/*A queue for each host interface*/
	int	nextcontroller=0;	/*Number of interface units found*/
	int	drivepresent=0; 	/*Don't change this - see license*/
static	struct	matcd_mbx *mbxsave;

unsigned char	if_state[4]={0,0,0,0};	/*State of the host I/F and buss*/

/*	Flags in the if_state array
*/

#define	BUSSBUSY	0x01		/*Buss is already busy*/



struct matcd_read2 {
	unsigned char	start_msf[3];
	unsigned char	end_msf[3];
};

/*---------------------------------------------------------------------------
	These macros take apart the minor number and yield the
	partition, drive on controller, and controller.
	This must match the settings in /dev/MAKEDEV.
---------------------------------------------------------------------------*/

#define		matcd_partition(dev)	((minor(dev)) & 0x07)
#define		matcd_ldrive(dev)	(((minor(dev)) & 0x78) >> 3)
#define		matcd_cdrive(dev)	(((minor(dev)) & 0x18) >> 3)
#define		matcd_controller(dev)	(((minor(dev)) & 0x60) >> 5)


#ifndef FREE2
/*---------------------------------------------------------------------------
	This makes the long function names shorter for systems
	using the older kernel config program
---------------------------------------------------------------------------*/
#define	matcdopen	matopen		/*<8>*/
#define matcdclose	matclose	/*<8>*/
#define matcdstrategy	matstrategy	/*<8>*/
#define matcdioctl	matioctl	/*<8>*/
#define matcdsize	matsize		/*<8>*/
#define matcddriver	matdriver	/*<10>*/
#endif /*FREE2*/


/*---------------------------------------------------------------------------
	Entry points and other connections to/from kernel - see conf.c
---------------------------------------------------------------------------*/

	int	matcdopen(dev_t dev);
	int	matcdclose(dev_t dev);
	void	matcdstrategy(struct buf *bp);
	int	matcdioctl(dev_t dev, int command, caddr_t addr, int flags);
	int	matcdsize(dev_t dev);
extern	int	hz;
extern	int	matcd_probe(struct isa_device *dev);
extern	int	matcd_attach(struct isa_device *dev);
struct	isa_driver	matcddriver={matcd_probe, matcd_attach,
				     "matcd interface "};


/*---------------------------------------------------------------------------
	Internal function declarations
---------------------------------------------------------------------------*/

static	int	matcd_getdisklabel(int ldrive);
static	void	matcd_start(struct buf *dp);
static	void	zero_cmd(char *);
static	void	matcd_pread(int port, int count, unsigned char * data); 
static	int	matcd_fastcmd(int port,int ldrive,int cdrive,
			      unsigned char * cp);
static	void	matcd_slowcmd(int port,int ldrive,int cdrive,
			      unsigned char * cp);
static	int	matcd_getstat(int ldrive, int sflg);
static	void	matcd_setflags(int ldrive, struct matcd_data *cd);
static	int	msf2hsg(unsigned char *msf);
static	void	matcd_blockread(int state);
static	int	matcd_getreply(int ldrive, int dly);
static	void	selectdrive(int port,int drive);
static	void	doreset(int port,int cdrive);
static	int	doprobe(int port,int cdrive);
static	void	watchdog(int state, char * foo);
static	void	lockbuss(int controller, int ldrive);
static	void	unlockbuss(int controller, int ldrive);
static	int	matcd_volinfo(int ldrive);
static	void	draincmd(int port,int cdrive,int ldrive);
static	int 	get_error(int port, int ldrive, int cdrive);
static	int	chk_error(int errnum);
static	int	msf_to_blk(unsigned char * cd);
#ifdef	FULLDRIVER
static	int	matcd_playtracks(int ldrive, int cdrive, int controller,
				 struct ioc_play_track *pt);
static	int	matcd_playmsf(int ldrive, int cdrive, int controller,
				 struct ioc_play_msf *pt);
static	int	matcd_pause(int ldrive, int cdrive, int controller,
			     struct ioc_play_msf * addr,int action);
static	int	matcd_stop(int ldrive, int cdrive, int controller,
		           struct ioc_play_msf * addr);
#endif /*FULLDRIVER*/


/*----------------------------------------------------------------------
	matcdopen - Open the device

	This routine actually gets called every time anybody opens
	any partition on a drive.  But the first call is the one that
	does all the work.  

	If you #define LOCKDRIVE, the drive eject button will be ignored
	while any partition on the drive is open.
----------------------------------------------------------------------*/
int	matcdopen(dev_t dev)
{
	int cdrive,ldrive,partition,controller;
	struct matcd_data *cd;
	int	i,z,port;
	unsigned char	cmd[MAXCMDSIZ];

#if DIAGPORT == 0x302			/*<10>*/
	DIAGOUT(0x300,0x00);		/*<10>Init diag board in case some
					  other device probe scrambled it*/
#endif /*<10>DIAGPORT*/
#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0x10);		/*Show where we are*/
#endif /*DIAGPORT*/
	ldrive=matcd_ldrive(dev);	
	cdrive=matcd_cdrive(dev);
	partition=matcd_partition(dev);
	controller=matcd_controller(dev);
	cd= &matcd_data[ldrive];
	port=cd->iobase;	/*and port#*/

	if (ldrive >= TOTALDRIVES) return(ENXIO);


#ifdef DEBUGOPEN
	printf("matcd%d: Open: dev %x partition %x controller %x flags %x cdrive %x\n",
	       ldrive,dev,partition,controller,cd->flags,matcd_cdrive(dev));
#endif /*DEBUGOPEN*/

	if (!(cd->flags & MATCDINIT)) {	/*Did probe find this drive*/
		return(ENXIO);	
	}

	if (!(cd->flags & MATCDLABEL) &&
	    cd->openflags) {		/*Has drive completely closed?*/
		return(ENXIO);		/*No, all partitions must close*/
	}


/*	Now, test to see if the media is ready
*/

	lockbuss(controller,ldrive);
	zero_cmd(cmd);
	cmd[0]=NOP;			/*Test drive*/
	matcd_slowcmd(port,ldrive,cdrive,cmd);
	i=waitforit(10*TICKRES,DTEN,port,"matcdopen");
	z=get_stat(port,ldrive);	/*Read and toss status byte*/
	unlockbuss(controller, ldrive);	/*Release buss lock*/
	if ((z & MATCD_ST_DSKIN)==0) {	/*Is there a disc in the drive?*/ 
#ifdef DEBUGOPEN
		printf("matcd%d: No Disc in open\n",ldrive);
#endif /*DEBUGOPEN*/
		return(ENXIO);
	}
	if (z & MATCD_ST_ERROR) {			/*Was there an error*/
		i=get_error(port,ldrive,cdrive);	/*Find out what it was*/
		if (cd->openflags) {			/*Any parts open?*/
			if (media_chk(cd,i,ldrive)) {	/*Was it a disc chg?*/
#ifdef DEBUGOPEN
				printf("matcd%d: Disc change detected i %x z %x\n",
				       ldrive,i,z);
#endif /*DEBUGOPEN*/
				return(ENOTTY);
			}
		}
	}
	
/*	Here we fill in the disklabel structure although most is
	hardcoded.
*/

	if ((cd->flags & MATCDLABEL)==0) {
		bzero(&cd->dlabel,sizeof(struct disklabel));


/*	Now we query the drive for the actual size of the media.
	This is where we find out of there is any media or if the
	media isn't a Mode 1 or Mode 2/XA disc.
	See version information about Mode 2/XA support.
*/
		lockbuss(controller,ldrive);
		i=matcdsize(dev);
		unlockbuss(controller, ldrive);	/*Release buss lock*/
#ifdef DEBUGOPEN
		printf("matcd%d: Buss unlocked in open\n",ldrive);
#endif /*DEBUGOPEN*/
		if (i < 0) {
			printf("matcd%d: Could not read the disc size\n",ldrive);
			return(ENXIO);
		}			/*matcdsize filled in rest of dlabel*/

/*	Based on the results, fill in the variable entries in the disklabel
*/
		cd->dlabel.d_secsize=cd->blksize;
		cd->dlabel.d_ncylinders=(cd->disksize/100)+1;
		cd->dlabel.d_secperunit=cd->disksize;
		cd->dlabel.d_partitions[0].p_size=cd->disksize;
		cd->dlabel.d_checksum=dkcksum(&cd->dlabel);


/*	Now fill in the hardcoded section
*/
					     /*123456789012345678*/
		strncpy(cd->dlabel.d_typename,"Matsushita CDR ",16);
		strncpy(cd->dlabel.d_packname,"(c) 1994, fdiv ",16);
		cd->dlabel.d_magic=DISKMAGIC;
		cd->dlabel.d_magic2=DISKMAGIC;
		cd->dlabel.d_nsectors=100;
		cd->dlabel.d_secpercyl=100;
		cd->dlabel.d_ntracks=1;
		cd->dlabel.d_interleave=1;
		cd->dlabel.d_rpm=300;
		cd->dlabel.d_npartitions=1;	/*See note below*/
		cd->dlabel.d_partitions[0].p_offset=0;
		cd->dlabel.d_partitions[0].p_fstype=9;
		cd->dlabel.d_flags=D_REMOVABLE;

/*	I originally considered allowing the partition match tracks or
	sessions on the media, but since you are allowed up to 99
	tracks in the RedBook world, this would not fit in with the
	BSD fixed partition count scheme.  So ioctls are used to shift
	the track to be accessed into partition 1.
*/

		cd->flags |= MATCDLABEL;	/*Mark drive as having TOC*/
	}
	
#ifdef DEBUGOPEN
	printf("matcd%d open2: partition=%d disksize=%d blksize=%x flags=%x\n",
	       ldrive,partition,cd->disksize,cd->blksize,cd->flags);
#endif /*DEBUGOPEN*/

#ifdef LOCKDRIVE
	if (cd->openflags==0) {
		lockbuss(controller,ldrive);
		zero_cmd(cmd);
		cmd[0]=LOCK;		/*Lock drive*/
		cmd[1]=1;
		matcd_slowcmd(port,ldrive,cdrive,cmd);
		i=waitforit(10*TICKRES,DTEN,port,"matcdopen");
		z=get_stat(port,ldrive);/*Read and toss status byte*/
		unlockbuss(controller, ldrive);	/*Release buss lock*/
	}
#endif /*LOCKDRIVE*/
	cd->openflags |= (1<<partition);/*Mark partition open*/

	if (partition==RAW_PART || 
	    (partition < cd->dlabel.d_npartitions &&
	     cd->dlabel.d_partitions[partition].p_fstype != FS_UNUSED)) {
		cd->partflags[partition] |= MATCDOPEN;
		if (partition == RAW_PART) {
			cd->partflags[partition] |= MATCDREADRAW;
		}
#ifdef DEBUGOPEN
		printf("matcd%d: Open is complete\n",ldrive);
#endif /*DEBUGOPEN*/
		return(0);
	}
#ifdef DEBUGOPEN
	printf("matcd%d: Open FAILED\n",ldrive);
#endif /*DEBUGOPEN*/
	return(ENXIO);
}


/*----------------------------------------------------------------------
	matcdclose - Close the device

	Depending on how you compiled the driver, close may not 
	do much other than clear some driver settings.
	Note that audio playback will continue.

	If you did #define LOCKDRIVE, the drive was locked when the
	matcdopen call is done.  If we did that, then we unlock the
	drive now.
----------------------------------------------------------------------*/

int matcdclose(dev_t dev)
{
	int	ldrive,cdrive,port,partition,controller,i,z;
	struct matcd_data *cd;
	unsigned char cmd[MAXCMDSIZ];

	ldrive = matcd_ldrive(dev);
	cdrive=matcd_cdrive(dev);
	cd=matcd_data + ldrive;
	port=cd->iobase;		/*and port#*/

#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0x20);		/*Show where we are*/
#endif /*DIAGPORT*/
	if (ldrive >= TOTALDRIVES) 
		return(ENXIO);

	partition = matcd_partition(dev);
	controller=matcd_controller(dev);
#ifdef DEBUGOPEN
	printf("matcd%d: Close partition=%d\n", ldrive, partition);
#endif /*DEBUGOPEN*/

	if (!(cd->flags & MATCDINIT))
		return(ENXIO);

	cd->partflags[partition] &= ~(MATCDOPEN|MATCDREADRAW);
	cd->openflags &= ~(1<<partition);
#ifdef LOCKDRIVE
	if (cd->openflags==0) {
		lockbuss(controller,ldrive);
		zero_cmd(cmd);
		cmd[0]=LOCK;		/*Unlock drive*/
		matcd_slowcmd(port,ldrive,cdrive,cmd);
		i=waitforit(10*TICKRES,DTEN,port,"matcdopen");
		z=get_stat(port,ldrive);/*Read and toss status byte*/
		unlockbuss(controller, ldrive);	/*Release buss lock*/
	}
#endif /*LOCKDRIVE*/
	cd->flags &= ~MATCDWARN;	/*Clear any warning flag*/
	return(0);
}


/*----------------------------------------------------------------------
	matcdstrategy - Accepts I/O requests from kernel for processing

	This routine accepts a read request block pointer (historically
	but somewhat inaccurately called *bp for buffer pointer).
	Various sanity checks are performed on the request.
	When we are happy with the request and the state of the device,
	the request is added to the queue of requests for the controller
	that the drive is connected to.  We support multiple controllers
	so there are multiple queues.  Once the request is added, we
	call the matcd_start routine to start the device in case it isn't
	doing something already.   All I/O including ioctl requests
	rely on the current request starting the next one before exiting.
----------------------------------------------------------------------*/

void matcdstrategy(struct buf *bp)
{
	struct matcd_data *cd;
	struct buf *dp;
	int s;
	int ldrive,controller;

#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0x30);		/*Show where we are*/
#endif /*DIAGPORT*/
	ldrive=matcd_ldrive(bp->b_dev);
	controller=matcd_controller(bp->b_dev);
	cd= &matcd_data[ldrive];

#ifdef DEBUGIO
	printf("matcd%d: Strategy: buf=0x%lx, block#=%ld bcount=%ld\n",
		ldrive,bp,bp->b_blkno,bp->b_bcount);
#endif /*DEBUGIO*/


	if (ldrive >= TOTALDRIVES || bp->b_blkno < 0) {
		printf("matcd%d: Bogus parameters received - kernel may be corrupted\n",ldrive);
		bp->b_error=EINVAL;
		bp->b_flags|=B_ERROR;
		goto bad;
	}

	if (!(cd->flags & MATCDLABEL)) {
		bp->b_error = EIO;
		goto bad;
	}

	if (!(bp->b_flags & B_READ)) {
		bp->b_error = EROFS;
		goto bad;
	}

	if (bp->b_bcount==0)		/*Request is zero-length - all done*/
		goto done;

	if (matcd_partition(bp->b_dev) != RAW_PART) {
		if (!(cd->flags & MATCDLABEL)) {
			bp->b_error = EIO;
			goto bad;
		}
		if (bounds_check_with_label(bp,&cd->dlabel,1) <= 0) {
			goto done;
		}
	} else {
		bp->b_pblkno=bp->b_blkno;
		bp->b_resid=0;
	}

	s=splbio();			/*Make sure we don't get intr'ed*/
	dp=&request_head[controller];	/*Pointer to controller queue*/
	disksort(dp,bp);		/*Add new request (bp) to queue (dp
					  and sort the requests in a way that
					  may not be ideal for CD-ROM media*/

#ifdef DEBUGQUEUE
	printf("matcd%d: Dump BP chain:  -------\n",ldrive);
	while (bp) {
		printf("Block %d\n",bp->b_pblkno);
#ifdef FREE2
		bp=bp->b_actf;
#else /*FREE2*/	
		bp=bp->av_forw;
#endif /*FREE2*/
	}
	printf("matcd%d: ---------------------\n",ldrive);
#endif /*DEBUGQUEUE*/

	matcd_start(dp);		/*Ok, with our newly sorted queue,
					  see if we can start an I/O operation
					  right now*/
	splx(s);			/*Return priorities to normal*/
	return;				/*All done*/

bad:	bp->b_flags |= B_ERROR;		/*Request bad in some way*/
done:	bp->b_resid = bp->b_bcount;	/*Show un read amount*/
	biodone(bp);			/*Signal we have done all we plan to*/
	return;
}


/*----------------------------------------------------------------------
	matcd_start - Pull a request from the queue and consider doing it.
----------------------------------------------------------------------*/

static void matcd_start(struct buf *dp)
{
	struct matcd_data *cd;
	struct buf *bp;
	struct partition *p;
	int part,ldrive,controller;
	register s;

#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0x40);		/*Show where we are*/
	diagloop=0;
#endif /*DIAGPORT*/
	if ((bp=dp->b_actf) == NULL) {	/*Nothing on read queue to do?*/
		wakeup((caddr_t)&matcd_data->status);	/*Wakeup any blocked*/
		return;					/* opens, ioctls, etc*/
	}

	ldrive=matcd_ldrive(bp->b_dev);	/*Get logical drive#*/
	cd=&matcd_data[ldrive];		/*Get pointer to data for this drive*/
	controller=matcd_controller(bp->b_dev);	/*Also get interface #*/
#ifdef DEBUGIO
	printf("matcd%d: In start controller %d\n",ldrive,controller);
#endif	/*DEBUGIO*/

	if (if_state[controller] & BUSSBUSY) {
#ifdef DEBUGIO
		printf("matcd%d: Dropping thread in start,  controller %d\n",
	       		ldrive,controller);
#endif	/*DEBUGIO*/
		return;
	}

#ifdef FREE2
	dp->b_actf = bp->b_actf;
#else /*FREE2*/
	dp->b_actf = bp->av_forw;	/*Get next request from queue*/
#endif /*FREE2*/

	part=matcd_partition(bp->b_dev);
	p=cd->dlabel.d_partitions + part;

	if_state[controller] |= BUSSBUSY;/*Mark buss as busy*/
	cd->mbx.ldrive=ldrive;		/*Save current logical drive*/
	cd->mbx.controller=controller;	/*and controller*/
	cd->mbx.partition=part;		/*and partition (2048 vs 2532)*/
	cd->mbx.port=cd->iobase;	/*and port#*/
	cd->mbx.retry=MATCD_RETRYS;	/*and the retry count*/
	cd->mbx.bp=bp;			/*and the bp*/
	cd->mbx.p_offset=p->p_offset;	/*and where the data will go*/
	matcd_blockread(MATCD_READ_1+ldrive);	/*Actually start the read*/
	return;				/*Dropping thread.  matcd_blockread
					  must have scheduled a timeout or
					  we will go to sleep forever*/
}


/*----------------------------------------------------------------------
	matcdioctl - Process things that aren't block reads

	In this driver, ioctls are used mainly to change
	the mode the drive is running in, play audio and other
	things that don't fit into the block read scheme of things.
----------------------------------------------------------------------*/

int matcdioctl(dev_t dev, int command, caddr_t addr, int flags)
{
	struct	matcd_data *cd;
	int	ldrive,cdrive,partition;
	int	i,z;
	unsigned char * p;
	int	port, controller;
	unsigned char zz;

#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0x50);		/*Show where we are*/
#endif /*DIAGPORT*/
	ldrive=matcd_ldrive(dev);
	cdrive=matcd_cdrive(dev);
	partition=matcd_partition(dev);
	controller=ldrive>>2;
	cd = &matcd_data[ldrive];
	port=cd->iobase;

#ifdef DEBUGIOCTL
	printf("matcd%d: ioctl %x cdrive %x parms ",ldrive,command,cdrive);
	for (i=0;i<10;i++) {
		zz=addr[i];
		printf("%02x ",zz);
	}
	printf("  flags %x\n",cd->flags);
#endif	/*DEBUGIOCTL*/

	if (!(cd->flags & MATCDLABEL))	/*Did we read TOC OK?*/
		return(EIO);

	switch(command) {
	case	DIOCSBAD:		/*<9>*/
		return(EINVAL);		/*<9>*/
	case	DIOCGDINFO:		/*<9>*/
		*(struct disklabel *) addr = cd->dlabel;	/*<9>*/
		return(0);		/*<9>*/
	case	DIOCGPART:		/*<9>*/
		((struct partinfo *) addr)->disklab=&cd->dlabel;/*<9>*/
		((struct partinfo *) addr)->part=	/*<9>*/
		    &cd->dlabel.d_partitions[matcd_partition(dev)];/*<9>*/
		return(0);		/*<9>*/
	case	DIOCWDINFO:		/*<9>*/
	case	DIOCSDINFO:		/*<9>*/
#ifdef FREE2				/*<10>*/
		if ((flags & FWRITE) == 0) {	/*<9>*/
			return(EBADF);	/*<9>*/
		}			/*<9>*/
		else {			/*<9>*/
			return setdisklabel(&cd->dlabel,	/*<9>*/
				            (struct disklabel *) addr, 0);/*<9>*/
		}			/*<9>*/
#endif /*<10>FREE2*/
	case	DIOCWLABEL:		/*<9>*/
		return(EBADF);		/*<9>*/
	case	CDIOCEJECT:
		return(matcd_eject(ldrive, cdrive, controller));
#ifdef FULLDRIVER
	case	CDIOCPLAYTRACKS:
		return(matcd_playtracks(ldrive, cdrive, controller,
		       (struct ioc_play_track *) addr));
	case	CDIOCPLAYMSF:
		return(matcd_playmsf(ldrive, cdrive, controller,
		       (struct ioc_play_msf *) addr));
	case	CDIOCRESUME:
		return(matcd_pause(ldrive, cdrive, controller,
		       (struct ioc_play_msf *) addr,RESUME));
	case	CDIOCPAUSE:
		return(matcd_pause(ldrive, cdrive, controller,
		       (struct ioc_play_msf *) addr,0));
	case	CDIOCSTOP:
		return(matcd_stop(ldrive, cdrive, controller,
		       (struct ioc_play_msf *) addr));

	case	CDIOCGETVOL:
	case	CDIOCSETVOL:
	case	CDIOCSETMONO:
	case	CDIOCSETSTERIO:
	case	CDIOCSETMUTE:
	case	CDIOCSETLEFT:
	case	CDIOCSETRIGHT:
#endif	/*FULLDRIVER*/

	case	CDIOCREADSUBCHANNEL:
	case	CDIOREADTOCHEADER:
	case	CDIOREADTOCENTRYS:

	case	CDIOCSETPATCH:
	case	CDIOCSTART:
	case	CDIOCRESET:
		return(EINVAL);
	default:
		return(ENOTTY);
	}
}

/*----------------------------------------------------------------------
	matcdsize - Reports how many blocks exist on the disc.
----------------------------------------------------------------------*/

int	matcdsize(dev_t dev)
{
	int	size,blksize;
	int	ldrive,part;
	struct	matcd_data *cd;

	ldrive=matcd_ldrive(dev);
	part=matcd_partition(dev);
	if (part==RAW_PART) 
		blksize=MATCDRBLK;	/*2353*/
	else	
		blksize=MATCDBLK;	/*2048*/

	cd = &matcd_data[ldrive];

	if (matcd_volinfo(ldrive) >= 0) {
		cd->blksize=blksize;
		size=msf_to_blk((char * )&cd->volinfo.vol_msf);

		cd->disksize=size*(blksize/DEV_BSIZE);
#ifdef DEBUGOPEN
	printf("matcd%d: Media size %d\n",ldrive,size);
#endif /*DEBUGOPEN*/
		return(0);
	}
	return(-1);
}

/*----------------------------------------------------------------------
	matcd_probe - Search for host interface/adapters

	The probe routine hunts for the first drive on the interface since
	there is no way to locate just the adapter.   It also resets the
	entire drive chain while it is there.  matcd_attach() takes care of
	the rest of the initialization.

	The probe routine can be compiled two ways.  In AUTOHUNT mode,
	the kernel config file can say "port?" and we will check all ports
	listed in the port_hint array (see above).  

	Without AUTOHUNT set, the config file must list a specific port
	address to check.  

	Note that specifying the explicit addresses makes boot-up a lot
	faster.

	The probe will locate Panasonic/Creative interface on the following
	Creative adapter boards:
		#1730  Sound Blaster 16
		#1740  Sound Blaster 16 (cost reduced)
		#1810  omniCD upgrade kit adapter card (stand-alone CD)
		#3100  PhoneBlaster SB16 + Sierra 14.4K modem combo
	Creative releases a newer and cheaper-to-make Sound Blaster
	board every few months, so by the original release date of this
	software, there are probably 8 different board models called
	Sound Blaster 16.  These include "Vibra", "Value", etc.

	Please report additional part numbers and board descriptions 
	and new port numbers that work to the author.

----------------------------------------------------------------------*/

int matcd_probe(struct isa_device *dev)
{
	int	i,cdrive;
	unsigned char	y,z,drive;
	int	level;
	int port = dev->id_iobase;	/*Take port hint from config file*/
	cdrive=nextcontroller;		/*Controller defined by pass for now*/

#if DIAGPORT == 0x302			/*<10>*/
	DIAGOUT(0x300,0x00);		/*<10>Init diag board in case some
					  other device probe scrambled it*/
#endif /*<10>DIAGPORT*/
#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0x60);		/*Show where we are*/
#endif /*DIAGPORT*/
	if (nextcontroller==NUMCTRLRS) {
		printf("matcdc%d: - Too many interfaces specified in config\n",
		       nextcontroller);
		return(0);
	}
	if (nextcontroller==0) {	/*Very first time to be called*/
		for (i=0; i<TOTALDRIVES; i++) {
			matcd_data[i].drivemode=MODE_UNKNOWN;
			matcd_data[i].flags=0;
		}
	}

	i=nextcontroller*DRIVESPERC;	/*Precompute controller offset*/
	for (y=0; y<DRIVESPERC; y++) {
		matcd_data[i+y].flags=0;
		matcd_data[i+y].config=0;
	}

#ifdef DEBUGPROBE
	printf("matcdc%d: In probe i %d y %d port %x\n",
	       nextcontroller,i,y,port);
#endif /*DEBUGPROBE*/
#ifdef AUTOHUNT
#ifdef DEBUGPROBE
	printf("matcd%d: size of port_hints %d\n",
	       nextcontroller,sizeof(port_hints));
#endif /*DEBUGPROBE*/
	if (port==-1) {
		for(i=0;i<(sizeof(port_hints)/sizeof(short));i++) {
			port=port_hints[i];
#ifdef DEBUGPROBE
	printf("matcdc%d: Port hint %x\n",nextcontroller,port);
#endif /*DEBUGPROBE*/
			if (port==-1) {
				dev->id_iobase=-1;	/*Put port ? back*/
				return(0);/*Nothing left to try*/
			}
			if (port!=0) {	/*Unused port found*/
				dev->id_iobase=port;
				port_hints[i]=0;/*Don't use that port again*/
				if (doprobe(port,cdrive)==0) return(NUMPORTS);
			}
		}
		dev->id_iobase=-1;	/*Put port ? back as it was*/
		return(0);		/*Interface not found*/

	} else {			/*Config specified a port*/
		i=0;			/*so eliminate it from the hint list*/
		for(i=0;;i++) {		/*or we might try to assign it again*/
			if (port_hints[i]== -1) break;	/*End of list*/
			if (port_hints[i]==port) {
				port_hints[i]=0;	/*Clear duplicate*/
				break;
			}
		}
		if (doprobe(port,cdrive)==0) return(NUMPORTS);
		else return(0);
	}
#else /*AUTOHUNT*/
	if (port==-1) {
		printf("matcd%d: AUTOHUNT disabled but port? specified in config\n",
		       nextcontroller);
		return(0);
	}
	if (doprobe(port,cdrive)==0) return(NUMPORTS);
	else return(0);
#endif /*AUTOHUNT*/
}

/*----------------------------------------------------------------------
	doprobe - Common probe code that actually checks the ports we 
		have decided to test.
----------------------------------------------------------------------*/

int doprobe(int port,int cdrive)
{
	unsigned char cmd[MAXCMDSIZ];

#ifdef RESETONBOOT
	doreset(port,cdrive);		/*Reset what might be our device*/
#endif /*RESETONBOOT*/

	zero_cmd(cmd);
	cmd[0]=NOP;			/*A reasonably harmless command.
				  	  This command will fail after
				  	  power-up or after reset. That's OK*/
	if (matcd_fastcmd(port,0,0,cmd)==0) {/*Issue command*/
		inb(port+CMD);		/*Read status byte*/
#ifdef DEBUGPROBE
		printf("matcdc%d: Probe found something\n",nextcontroller);
#endif /*DEBUGPROBE*/
		if (drivepresent==0) {	/*Don't change this - see license*/
			printf("matcd - Matsushita (Panasonic) CD-ROM Driver by FDIV, %s\n",MATCDVERSION);
					/*Don't change this - see license*/
			drivepresent++;	/*Don't change this - see license*/
		}			/*Don't change this - see license*/
		return(0);		/*Drive 0 detected*/
	}
#ifdef DEBUGPROBE
	printf("matcdc%d: Probe DID NOT find something\n",nextcontroller);
#endif /*DEBUGPROBE*/
	return(1);			/*Nothing detected*/
}


/*----------------------------------------------------------------------
	matcd_attach - Locates drives on the adapters that were located.
		If we got here, we located a interface and at least one
		drive.  Now we figure out how many drives are under that
		interface.  The Panasonic interface is too simple to call
		it a controller, but in the existing PDP model, that is
		what it would be.
----------------------------------------------------------------------*/

int matcd_attach(struct isa_device *dev)
{
	int	i;
	unsigned char	y,z,cdrive;
	int	level;
	unsigned char cmd[MAXCMDSIZ];
	unsigned char data[12];
	struct matcd_data *cd;
	int port = dev->id_iobase;	/*Take port ID selected in probe()*/

#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0x70);		/*Show where we are*/
#endif /*DIAGPORT*/
#ifdef DEBUGPROBE
	printf("matcdc: Attach dev %x id_unit %d\n",dev,dev->id_unit);
#endif /*DEBUGPROBE*/
	for (cdrive=0; cdrive<4; cdrive++) {	/*We're hunting drives...*/
		zero_cmd(cmd);
		cmd[0]=NOP;		/*A reasonably harmless command.
					  This command will fail after
					  power-up or after reset. It's OK*/
		i=cdrive+(DRIVESPERC*nextcontroller);
		if (matcd_fastcmd(port,i,cdrive,cmd)==0) {	/*Issue command*/
			z=get_stat(port,cdrive);/*Read and toss status byte*/
			if ((z & MATCD_ST_ERROR)) {	/*If there was an error,
						  we must ask for error info
						  or subsequent cmds fail*/
				zero_cmd(cmd);
				cmd[0]=READERROR;	/*Inquire*/
				matcd_fastcmd(port,i,cdrive,cmd);
				matcd_pread(port,8,data);/*Read data returned*/
				z=get_stat(port,i);/*Read and toss status byte*/
#ifdef DEBUGPROBE
				printf("matcd%d: Status byte %x ",i,z);
#endif /*DEBUGPROBE*/
			}
			zero_cmd(cmd);
			cmd[0]=READID;	/*Get drive ID*/
			matcd_fastcmd(port,i,cdrive,cmd);
			matcd_pread(port,10,data);/*Read Drive Parm*/
			z=get_stat(port,i);	/*Read and toss status byte*/
			data[10]=0;	/*Build ASCIZ string*/
			printf("matcd%d: [%s]  ",i,data);
			cd=&matcd_data[i];
			cd->flags |= MATCDINIT;
			cd->iobase=dev->id_iobase;
			cd->openflags=0;
			for (i=0; i<MAXPARTITIONS; i++) {
				cd->partflags[i]=0;
			}
		}
	}
	nextcontroller++;		/*Bump ctlr assign to next number*/
	printf("\n");			/*End line of drive reports*/
	return(1);
}


/*----------------------------------------------------------------------
	zero_cmd - Initialize command buffer
----------------------------------------------------------------------*/

void zero_cmd(char * lcmd)
{
	int	i;
	for (i=0; i<MAXCMDSIZ; lcmd[i++]=0);
	return;
}


/*----------------------------------------------------------------------
	doreset - Resets all the drives connected to a interface
----------------------------------------------------------------------*/

void doreset(int port,int cdrive)
{
	register int	i,z;
#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0x80);		/*Show where we are*/
#endif /*DIAGPORT*/
	outb(port+RESET,0);		/*Reset what might be our device*/
					/*Although this ensures a known
					  state, it does close the drive
					  door (if open) and aborts any
					  audio playback in progress. */
	for (i=0;i<(125*ISABUSKHZ);i++){/*DELAY 500msec minimum. Worst
					  case is door open and none or
					  unreadable media */
		z=inb(port+CMD);	/*This makes the loop run at a
					  known speed.  This value is ok
					  for 8.33MHz bus*/
	}
	for (i=0;i<4;i++) {
		matcd_data[(cdrive*4)+i].drivemode=MODE_UNKNOWN;
	}
	return;
}


/*----------------------------------------------------------------------
	matcd_fastcmd - Send a command to a drive

	This routine executed commands that return instantly (or reasonably
	quick), such as RESET, NOP, READ ERROR, etc.  The only difference
	between it and handling for slower commands, is the slower commands
	will invoke a timeout/sleep if they don't get an instant response.

	Fastcmd is mainly used in probe(), attach() and error related
	functions.  Every attempt should be made to NOT use this
	function for any command that might be executed when the system
	is up.
----------------------------------------------------------------------*/

int matcd_fastcmd(int port,int ldrive,int cdrive,unsigned char * cp)
{
	unsigned int	i;
	unsigned char	z;
	int	level;
#ifdef DEBUGCMD
	unsigned char *cx;
#endif /*DEBUGCMD*/

#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0x90);		/*Show where we are*/
#endif /*DIAGPORT*/


	draincmd(port,cdrive,ldrive);	/*Make sure buss is really idle*/
#ifdef DEBUGCMD
	cx=cp;
	printf("matcd%d: Fast Send port %x sel %d command %x %x %x %x %x %x %x\n",
	       ldrive,port,cdrive,cx[0],cx[1],cx[2],cx[3],cx[4],cx[5],cx[6]);
#endif	/*DEBUGCMD*/
	selectdrive(port,cdrive);	/*Enable the desired target drive*/
	level=splhigh();	/*----------------------------------------*/
	for (i=0; i<7; i++) {		/*The seven bytes of the command*/
		outb(port+CMD,*cp++);	/*must be sent within 10msec or*/
	}				/*the drive will ignore the cmd*/
	splx(level);	/*------------------------------------------------*/

/*	Now we wait a maximum of 240msec for a response.
	Only in a few rare cases does it take this long.
	If it is longer, the command should probably be slept on
	rather than increasing the timing value
*/

	for (i=0; i<(60*ISABUSKHZ); i++) {
		z = (inb(port+STATUS)) & (DTEN|STEN);
		if (z != (DTEN|STEN)) break;
	}

/*	We are now either in a data or status phase, OR we timed-out.*/

	if (z == (DTEN|STEN)) {
#ifdef DEBUGCMD
		printf("matcd%d: Command time-out\n",ldrive);
#endif /*DEBUGCMD*/
		return(-1);
	}
	if (z != DTEN) {
		return(1);
	}
	return(0);
}


/*----------------------------------------------------------------------
	matcd_slowcmd - Issue a command to the drive

	This routine is for commands that might take a long time, such
	as a read or seek.  The caller must determine if the command
	completes instantly or schedule a poll later on.
----------------------------------------------------------------------*/

void matcd_slowcmd(int port,int ldrive,int cdrive,unsigned char * cp)
{
	unsigned int	i;
	unsigned char	z;
	int	level,size;
#ifdef DEBUGCMD
	unsigned char *cx;
#endif /*DEBUGCMD*/

#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0xa0);		/*Show where we are*/
#endif /*DIAGPORT*/

	draincmd(port,cdrive,ldrive);	/*Make sure buss is really idle*/

#ifdef DEBUGCMD
	cx=cp;
	printf("matcd%d: Slow Send port %x sel %d command %x %x %x %x %x %x %x\n",
	       ldrive,port,cdrive,cx[0],cx[1],cx[2],cx[3],cx[4],cx[5],cx[6]);
#endif	/*DEBUGCMD*/
	selectdrive(port,cdrive);	/*Enable the desired target drive*/
	if (*cp==ABORT) size=1;
	else size=7;
	level=splhigh();	/*----------------------------------------*/
	for (i=0; i<size; i++) {	/*The seven bytes of the command*/
		outb(port+CMD,*cp++);	/*must be sent within 10msec or*/
	}				/*the drive will ignore the cmd*/
	splx(level);	/*------------------------------------------------*/
	return;
}


/*----------------------------------------------------------------------
	draincmd - Makes certain the buss is idle and throws away
		any residual data from the drive if there is any.
		Called as preface to most commands.
		Added in Edit 5.

		This was added because switching drive modes causes
		the drive to emit buffers that were meant to be sent
		to the D-to-A to be sent to the host.  See setmode.
----------------------------------------------------------------------*/
void draincmd(int port,int cdrive,int ldrive)
{
	int i,z;

	i=inb(port+STATUS);
	if (i==0xff) return;
	
	printf("matcd%d: in draincmd: buss not idle %x - trying to fix\n",
	       ldrive,inb(port+STATUS));
	if ((i & DTEN|STEN) == STEN) {
#ifdef DEBUGCMD
		printf("matcd%d: Data present READING - ",ldrive);
#endif /*DEBUGCMD*/
		i=0;
		outb(port+STATUS,1);	/*Enable data read*/
		while ((inb(port+STATUS) & (DTEN|STEN)) == STEN) {
			inb(port+DATA);
			i++;
		}
		outb(port+STATUS,0);
#ifdef DEBUGCMD
		printf("%d bytes read\n",i);
#endif /*DEBUGCMD*/
	}
#ifdef DEBUGCMD
	printf("matcd%d: Now read status: ",ldrive);
#endif /*DEBUGCMD*/
	i=get_stat(port,ldrive);	/*Read and toss status byte*/
	z=inb(port+STATUS);		/*Read buss status*/
#ifdef DEBUGCMD
	printf("Data byte %x and status is now %x\n",i,z);
#endif /*DEBUGCMD*/
	if (z!=0xff) {
		printf("matcd%d: Buss not idle %x - resetting\n",
		       cdrive,inb(port+STATUS));
		doreset(port,cdrive);
	}
	return;
}


/*----------------------------------------------------------------------
	selectdrive - Swaps drive select bits

	On Creative SB/SB16/stand-alone adapters, possibly to make them
	to reverse engineer.  On these boards, the drive select signals
	are swapped.
----------------------------------------------------------------------*/

void selectdrive(int port,int drive)
{
	switch(drive) {
	case 0:				/*0x00 -> 0x00*/
		outb(port+SELECT,CRDRIVE0);
		break;
	case 1:				/*0x01 -> 0x02*/
		outb(port+SELECT,CRDRIVE1);
		break;
	case 2:				/*0x02 -> 0x01*/
		outb(port+SELECT,CRDRIVE2);
		break;
	case 3:				/*0x03 -> 0x03*/
		outb(port+SELECT,CRDRIVE3);
		break;
	}
	return;
}


/*----------------------------------------------------------------------
	matcd_pread - Read small blocks of control data from a drive
----------------------------------------------------------------------*/

void matcd_pread(int port, int count, unsigned char * data) 
{
	int	i;

	for (i=0; i<count; i++) {
		*data++ = inb(port+CMD);
	}
	return;
}


/*----------------------------------------------------------------------
	matcd_setmode - Configures disc to run in the desired data mode

	This routine assumes the drive is already idle.

NOTE -	Undocumented action of hardware:  If you change (or reaffirm) data
	modes with MODESELECT + BLOCKPARAM immediately after a command was
	issued that aborted a DA play operation, the drive will unexpectedly
	return 2532 bytes of data in a data phase on the first or second
	subsequent command.

	Original Symptom: drive will refuse to go idle after reading data
	and status expected for a command.  State mechanics for this are
	not fully understood.
----------------------------------------------------------------------*/

int matcd_setmode(int ldrive, int mode)
{
	struct	matcd_data *cd;
	int retries;
	int i,z,port,cdrive;
	unsigned char cmd[MAXCMDSIZ];

	cd = matcd_data + ldrive;
	retries=3;
	cdrive=ldrive&0x03;
	port=cd->iobase;
	if (cd->drivemode==mode) {
		return(0);		/*Drive already set*/
	}

/*	The drive is not in the right mode, so we need to set it.
*/

	zero_cmd(cmd);
	cmd[0]=MODESELECT;	/*Set drive transfer modes*/
/*	cmd[1]=BLOCKPARAM;	  BLOCKPARAM==0*/
	cmd[2]=mode;
	switch(mode) {
	case MODE_DATA:
		cmd[3]=0x08;	/*2048 bytes*/
		break;
	case MODE_USER:
		cmd[3]=0x09;	/*2352 bytes*/
		cmd[4]=0x30;
		break;
	case MODE_DA:
		cmd[3]=0x09;	/*2352 bytes*/
		cmd[4]=0x30;
		break;
	}
	i=0;
	while(retries-- > 0) {
		i=matcd_fastcmd(port,ldrive,cdrive,cmd);
		z=get_stat(port,ldrive);/*Read and toss status byte*/
		if (i==0) {
			cd->drivemode=mode;	/*Set new mode*/
			return(i);
		}
		get_error(port,ldrive,cdrive);
	}
	cd->drivemode=MODE_UNKNOWN;	/*We failed*/
	return(i);
}


/*----------------------------------------------------------------------
	matcd_volinfo - Read information from disc Table of Contents
----------------------------------------------------------------------*/

static	int	matcd_volinfo(int ldrive)
{
	struct	matcd_data *cd;
	int	port,i;
	int	z,cdrive;
	int	retry;
	unsigned char cmd[MAXCMDSIZ];
	unsigned char data[12];

	retry=5;
	cd = &matcd_data[ldrive];
	cdrive=ldrive&0x03;
	port=cd->iobase;

#ifdef DEBUGOPEN
	printf("matcd%d: In volinfo, port %x\n",ldrive,port);
#endif /*DEBUGOPEN*/

	while(retry>0) {
#ifdef DIAGPORT
		DIAGOUT(DIAGPORT,0xB0);		/*Show where we are*/
#endif /*DIAGPORT*/
		zero_cmd(cmd);
		cmd[0]=READDINFO;	/*Read Disc Info*/
		matcd_slowcmd(port,ldrive,cdrive,cmd);
		i=waitforit(10*TICKRES,DTEN,port,"volinfo");
		if (i) {		/*THIS SHOULD NOT HAPPEN*/
			z=get_stat(port,ldrive);/*Read and toss status byte*/
			printf("matcd%d: command failed, status %x\n",
			       ldrive,z);
		return(-1);
		}
		matcd_pread(port, 6, data);	/*Read data returned*/
		z=get_stat(port,ldrive);/*Read and toss status byte*/
#ifdef DEBUGOPEN
		printf("matcd%d: Data got was %x %x %x %x %x %x   ",ldrive,
		       data[0],data[1],data[2], data[3],data[4],data[5]);
		printf("status byte %x\n",z);
#endif	/*DEBUGOPEN*/
		if ((z & MATCD_ST_ERROR)==0)	
			break;		/*No Error*/

/*	If media change or other error, you have to read error data or
	the drive will reject subsequent commands.
*/

		if (chk_error(get_error(port, ldrive, cdrive))==ERR_FATAL) { 
#ifdef DEBUGOPEN
			printf("matcd%d: command failed, status %x\n",
			       ldrive,z);
#endif /*DEBUGOPEN*/
			return(-1);
		}
		if ((--retry)==0) return(-1);
#ifdef DEBUGOPEN
		printf("matcd%d: Retrying",ldrive);
#endif /*DEBUGOPEN*/
	}
#ifdef DEBUGOPEN
	printf("matcd%d: Status port %x  \n",ldrive,inb(port+STATUS));
#endif /*DEBUGOPEN*/

	cd->volinfo.type=data[0];
	cd->volinfo.trk_high=data[2];
	cd->volinfo.trk_low=data[1];
	cd->volinfo.vol_msf[0]=data[3];
	cd->volinfo.vol_msf[1]=data[4];
	cd->volinfo.vol_msf[2]=data[5];

	if (cd->volinfo.trk_low + cd->volinfo.trk_high) {
		cd->flags |= MATCDLABEL;
		return(0);
	}
	return(-1);
}


/*----------------------------------------------------------------------
	blk_to_msf - Convert block numbers into CD disk block ids	
----------------------------------------------------------------------*/

static void blk_to_msf(int blk, unsigned char *msf)
{
	blk += 150;			/*2 seconds skip required to
					  reach ISO data*/
	msf[0] = blk/4500;
	blk %= 4500;
	msf[1] = blk / 75;
	msf[2] = blk % 75;
	return;
}


/*----------------------------------------------------------------------
	msf_to_blk - Convert CD disk block ids into block numbers
----------------------------------------------------------------------*/

static int msf_to_blk(unsigned char * cd)
{
	return(((cd[0]*60)		/*Convert MSF to*/
	      +cd[1])*75		/*Blocks minus 2*/
	      +cd[2]-150);		/*seconds*/
}
	

/*----------------------------------------------------------------------
	matcd_blockread - Performs actual background disc I/O operations

	This routine is handed the block number to read, issues the
	command to the drive, waits for it to complete, reads the
	data or error, retries if needed, and returns the results
	to the host.
----------------------------------------------------------------------*/

static void matcd_blockread(int state)
{
	struct	matcd_mbx *mbx;
	int	ldrive,cdrive;
	int	port;
	struct	buf *bp;
	struct	buf *dp;
	struct	matcd_data *cd;
	int	i,k,z;
	int	l,m;
	struct	matcd_read2 rbuf;
	int	blknum;
	caddr_t addr;
	int	status;
	int	errtyp;
	int	phase;
	unsigned char cmd[MAXCMDSIZ];

#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0xC0 + (diagloop * 0x100));/*Show where we are*/
#endif /*DIAGPORT*/
	mbx = &matcd_data[state & 0x0f].mbx;
	ldrive=mbx->ldrive;		/*ldrive is logical drive #*/
	cdrive=ldrive & 0x03;		/*cdrive is drive # on a controller*/
	port=mbx->port;			/*port is base port for i/f*/
	bp= mbx->bp;
	cd=&matcd_data[ldrive];

	dp=&request_head[mbx->controller];

#ifdef DEBUGIO
	printf("matcd%d: Show state %x cdrive %d partition %d\n",
	       ldrive,state,cdrive,mbx->partition);
#endif /*DEBUGIO*/

loop:
#ifdef DEBUGIO
	printf("matcd%d: Top  dp %x\n",ldrive,dp);
#endif /*DEBUGIO*/
#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0xCF + (diagloop * 0x100));/*Show where we are*/
#endif /*DIAGPORT*/
	switch (state & 0xf0) {
	case	MATCD_READ_1:
#ifdef DEBUGIO
		printf("matcd%d: State 1 cd->flags %x\n",ldrive,cd->flags);
#endif /*DEBUGIO*/
#ifdef DIAGPORT
		diagloop=0;
		DIAGOUT(DIAGPORT,0xC1);		/*Show where we are*/
#endif /*DIAGPORT*/
		/* to check for raw/cooked mode */
		if (cd->partflags[mbx->partition] & MATCDREADRAW) {
			mbx->sz = MATCDRBLK;
			i=matcd_setmode(ldrive, MODE_DA);
#ifdef DEBUGIO
			printf("matcd%d: Set MODE_DA result %d\n",ldrive,i);
#endif /*DEBUGIO*/
		} else {
			mbx->sz = cd->blksize;
			i=matcd_setmode(ldrive, MODE_DATA);
#ifdef DEBUGIO
			printf("matcd%d: Set MODE_DATA result %d\n",ldrive,i);
#endif /*DEBUGIO*/
		}
		/*for first block*/
#ifdef DEBUGIO
		printf("matcd%d: A mbx %x bp %x b_bcount %x sz %x\n",
		       ldrive,mbx,bp,bp->b_bcount,mbx->sz);
#endif /*DEBUGIO*/
		mbx->nblk = (bp->b_bcount + (mbx->sz-1)) / mbx->sz;
		mbx->skip=0;
nextblock:
#ifdef DEBUGIO
		printf("matcd%d: at Nextblock b_blkno %d\n",
		       ldrive,bp->b_blkno);
#endif /*DEBUGIO*/

		blknum=(bp->b_blkno / (mbx->sz/DEV_BSIZE))
		       + mbx->p_offset + mbx->skip/mbx->sz; 

		blk_to_msf(blknum,rbuf.start_msf);
			
		zero_cmd(cmd);
		cmd[0]=READ;	/*Get drive ID*/
		cmd[1]=rbuf.start_msf[0];
		cmd[2]=rbuf.start_msf[1];
		cmd[3]=rbuf.start_msf[2];
		cmd[6]=1;
		matcd_slowcmd(port,ldrive,cdrive,cmd);

/*	Now that we have issued the command, check immediately to
	see if data is ready.   The drive has read-ahead caching, so
	it is possible the data is already in the drive buffer.

	If the data is not ready, schedule a wakeup and later on this
	code will run again to see if the data is ready then.
*/

	case MATCD_READ_2:
		state=MATCD_READ_2+ldrive;
		phase = (inb(port+STATUS)) & (DTEN|STEN);
#ifdef DEBUGIO
		printf("matcd%d: In state 2 status %x  ",ldrive,phase);
#endif /*DEBUGIO*/
#ifdef DIAGPORT
		DIAGOUT(DIAGPORT,0xC2 + (diagloop++ * 0x100));/*Show where we are*/
#endif /*DIAGPORT*/
		switch(phase) {
		case (DTEN|STEN):	/*DTEN==H  STEN==H*/
#ifdef DEBUGIO
			printf("matcd%d: Sleeping\n",ldrive);
#endif /*DEBUGIO*/
			timeout((timeout_func_t)matcd_blockread,
				(caddr_t)MATCD_READ_2+ldrive,hz/100);
			return;


		case	STEN:		/*DTEN=L STEN=H*/
		case	0:		/*DTEN=L STEN=L*/
#ifdef DEBUGIO
			printf("matcd%d: Data Phase\n",ldrive);
#endif /*DEBUGIO*/
			outb(port+STATUS,1);	/*Enable data read*/
			addr=bp->b_un.b_addr + mbx->skip;
#ifdef DEBUGIO
			printf("matcd%d: Xfer Addr %x  size %x",
			       ldrive,addr,mbx->sz);
#endif /*DEBUGIO*/
			i=0;
			while(inb(port+STATUS)==0xfd) {
				*addr++=inb(port+DATA);
				i++;
			}
#ifdef DEBUGIO
			printf("matcd%d: Read %d bytes\n",ldrive,i);
#endif /*DEBUGIO*/
			outb(port+STATUS,0);	/*Disable data read*/


/*	Now, wait for the Status phase to arrive.   This will also
	tell us if any went wrong with the request.
*/
			while((inb(port+STATUS)&(DTEN|STEN)) != DTEN);
			status=get_stat(port,ldrive);	/*Read and toss status byte*/
#ifdef DEBUGIO
			printf("matcd%d: Status port %x byte %x  ",
			       ldrive,i,status);
#endif /*DEBUGIO*/
			if (status & MATCD_ST_ERROR) {
				i=get_error(port,ldrive,cdrive);
				printf("matcd%d: %s while reading block %d [Soft]\n",
				       ldrive,matcderrors[i],bp->b_blkno);
			}
			media_chk(cd,i,ldrive);

			if (--mbx->nblk > 0) {
				mbx->skip += mbx->sz;
				goto nextblock;	/*Oooooh, you flunk the course*/
			}
			bp->b_resid=0;
			biodone(bp);
	
			unlockbuss(ldrive>>2, ldrive);	/*Release buss lock*/
			matcd_start(dp);
			return;
	
/*	Here we skipped the data phase and went directly to status.
	This indicates a hard error.
*/

		case	DTEN:		/*DTEN=H STEN=L*/
			status=get_stat(port,ldrive);	/*Read and toss status byte*/
#ifdef DEBUGIO
			printf("matcd%d: error, status was %x\n",
			       ldrive,status);
#endif /*DEBUGIO*/

/*	Ok, we need more details, so read error.  This is needed to issue
	any further commands anyway
*/

			errtyp=get_error(port,ldrive,cdrive);
			printf("matcd%d: %s while reading block %d\n",
				ldrive,matcderrors[errtyp],bp->b_blkno);

			if (media_chk(cd,errtyp,ldrive))
				goto giveup;

			errtyp=chk_error(errtyp);
			switch(errtyp) {
			case ERR_RETRY:	/*We can retry this error, but the
					  drive probably has already*/
				if (mbx->retry-- > 0 ) {
					state=MATCD_READ_1+ldrive;
#ifdef DEBUGIO
					printf("matcd%d: Attempting retry\n",
					       ldrive);
#endif /*DEBUGIO*/
					goto loop;
				}
				goto giveup;

/*	These errors usually indicate the user took the media from the
	drive while the dev was open.  We will invalidate the unit
	until it closes when we see this.
*/
			case	ERR_INIT:/*Media probably was removed
					   while the dev was open.
					   Invalidate the unit until
					   it is closed.*/

			case	ERR_FATAL:/*This type of error is so
					    bad we will never recover
					    even if we retry.*/
			default:
giveup:
#ifdef DIAGPORT
				DIAGOUT(DIAGPORT,0xCE + (diagloop * 0x100));/*Show where we are*/
#endif /*DIAGPORT*/
				bp->b_flags |= B_ERROR;
				bp->b_resid = bp->b_bcount;
				biodone(bp);
				unlockbuss(ldrive>>2, ldrive);
				matcd_start(dp);
				return;
			}
		}
	}
}


/*----------------------------------------------------------------------
	matcd_eject - Open drive tray
----------------------------------------------------------------------*/

int matcd_eject(int ldrive, int cdrive, int controller)
{
	int	 retries,i,z,port;
	struct	matcd_data *cd;
	unsigned	char	cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;		/*Get I/O port base*/

	zero_cmd(cmd);			/*Initialize command buffer*/
#ifdef LOCKDRIVE
	cmd[0]=LOCK;			/*Unlock drive*/
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#endif /*LOCKDRIVE*/
	cmd[0]=DOOROPEN;		/*Open Door*/
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
	cd->flags &= ~MATCDLABEL;	/*Mark volume info invalid*/
	return(i);			/*Return result we got*/
}


/*----------------------------------------------------------------------
	docmd - Get the buss, do the command, wait for completion,
		attempt retries, give up the buss.
		For commands that do not return data.
----------------------------------------------------------------------*/

int docmd(char * cmd, int ldrive, int cdrive, int controller, int port)
{
	int retries,i,z;

	lockbuss(controller, ldrive);	/*Request buss*/
	retries=3;
	while(retries-- > 0) {
#ifdef DIAGPORT
		DIAGOUT(DIAGPORT,0xD0);	/*Show where we are*/
#endif /*DIAGPORT*/
		matcd_slowcmd(port,ldrive,cdrive,cmd);
		i=waitforit(80*TICKRES,DTEN,port,"cmd");
		z=get_stat(port,ldrive);/*Read and toss status byte*/
		if ((z & MATCD_ST_ERROR)==0) break;
		i=chk_error(get_error(port,ldrive,cdrive));
		if (i!=ERR_INIT) {
			unlockbuss(controller, ldrive);	/*Release buss*/
			return(EFAULT);
		}
	}
	unlockbuss(controller, ldrive);	/*Release buss*/
	return(i);
}


/*----------------------------------------------------------------------
	get_error - Read the error that aborted a command.
	Created in Edit 6
----------------------------------------------------------------------*/

int get_error(int port, int ldrive, int cdrive)
{
	int	status,errnum;
	unsigned char cmd1[MAXCMDSIZ];
	unsigned char data[12];

	zero_cmd(cmd1);
	cmd1[0]=READERROR;	/*Enquire*/
	matcd_fastcmd(port,ldrive,cdrive,cmd1);
	matcd_pread(port, 8, data);	/*Read data returned*/
	errnum=data[2];		/*Caller wants it classified*/
	status=get_stat(port,ldrive);	/*Read and toss status byte*/

#ifdef DEBUGCMD
	printf("matcd%d: Chkerror found %x on command %x addrval %x statusdata %x statusport %x\n",
		ldrive,errnum,data[1],data[0],status,inb(port+STATUS));
#endif /*DEBUGCMD*/
	return(errnum);
}


/*----------------------------------------------------------------------
	chk_error - Classify the error that the drive reported
	Created in Edit 6
----------------------------------------------------------------------*/

int chk_error(int errnum)
{
	switch(errnum) {
/*	These are errors we can attempt a retry for, although the drive
	has already done so.
*/
	case	UNRECV_ERROR:
	case	SEEK_ERROR:
	case	TRACK_ERROR:
	case	FOCUS_ERROR:
	case	CLV_ERROR:
	case	DATA_ERROR:
		return(ERR_RETRY);

/*	These errors usually indicate the user took the media from the
	drive while the dev was open.  We will invalidate the unit
	until it closes when we see this.
*/
	case	NOT_READY:
	case	MEDIA_CHANGED:
	case	DISC_OUT:
	case	HARD_RESET:
		return	(ERR_INIT);
			
/*	These errors indicate the system is confused about the drive
	or media, and point to bugs in the driver or OS.  These errors
	cannot be retried since you will always get the same error.
*/

	case	RAM_ERROR:
	case	DIAG_ERROR:
	case	CDB_ERROR:
	case	END_ADDRESS:
	case	MODE_ERROR:
	case	ILLEGAL_REQ:
	case	ADDRESS_ERROR:
	default:
		return	(ERR_FATAL);
	}
}


/*----------------------------------------------------------------------
	get_stat - Reads status byte

	This routine should be totally unnecessary, performing the
	task with a single line of in-line code.  However in special
	cases, the drives return blocks of data that are not associated
	with the command in question.  This appears to be a firmware
	error and the rest of the driver makes an effort to avoid
	triggering the fault.  However, reading and throwing this
	bogus data is faster and less destructive than resetting all
	the drives on a given controller, plus it leaves the other drives
	unaffected.
----------------------------------------------------------------------*/
	
int get_stat(int port,int ldrive)
{
	int 	status;
	status=inb(port+DATA);	/*Read the status byte, last step of cmd*/
	while ((inb(port+STATUS))!=0xff) {
		printf("matcd%d: get_stat: After reading status byte, buss didn't go idle\n",ldrive);
		if (( status & DTEN|STEN) == STEN) {
			int k;
			k=0;
#ifdef DEBUGCMD
			printf("matcd%d: DATA PRESENT!!!! DISCARDING\n",ldrive);
#endif /*DEBUGCMD*/
			outb(port+STATUS,1);	/*Enable data read*/
			while ((inb(port+STATUS) & (DTEN|STEN)) == STEN) {
				inb(port+DATA);
/*				printf("%2x ",inb(port+DATA));*/
				k++;
			}
			outb(port+STATUS,0);
#ifdef DEBUGCMD
			printf("\nmatcd%d: BYTES READ IN DATA was %d\n",
			       ldrive,k);
#endif /*DEBUGCMD*/
		}
		status=inb(port+DATA);	/*Read the status byte again*/
#ifdef DEBUGCMD
		printf("matcd%d: Next status byte is %x\n",ldrive,status);
#endif /*DEBUGCMD*/
	}
	return(status);
}


/*----------------------------------------------------------------------
	waitforit - Waits for a command started by slowcmd to complete.
----------------------------------------------------------------------*/

int waitforit(int timelimit, int state, int port, char * where)
{
	int i,j;

	j=i=0;
#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0xE0);	/*Show where we are*/
	diagloop=0;
#endif /*DIAGPORT*/
#ifdef DEBUGCMD
	printf("matcd: waitforit port %x timelimit %x hz %x\n",
	       port,timelimit,hz);
#endif /*DEBUGCMD*/
	while (i<timelimit) {
		j=inb(port+STATUS) & (STEN|DTEN);	/*Read status*/
		if (j!=(STEN|DTEN)) break;
		timeout((timeout_func_t)watchdog, (caddr_t)0,hz/100);
		tsleep((caddr_t)&port_hints, PRIBIO, where, 0);
		i++;
#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0xE1+(diagloop++ * 0x100));	/*Show where we are*/
#endif /*DIAGPORT*/
	}
#ifdef DEBUGCMD
	printf("matcd: Count was %d\n",i);
#endif /*DEBUGCMD*/
	if (j==state) return(0);	/*Command complete*/
#ifdef DEBUGCMD
	printf("matcd: Timeout!");
#endif /*DEBUGCMD*/
	return(1);			/*Timeout occurred*/
}


/*----------------------------------------------------------------------
	watchdog - Gives us a heartbeat for things we are waiting on
----------------------------------------------------------------------*/

static void watchdog(int state, char * foo)
{
#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0xF0);	/*Show where we are*/
#endif /*DIAGPORT*/
	wakeup((caddr_t)&port_hints);
	return;
}


/*----------------------------------------------------------------------
	lockbuss - Wait for the buss on the requested driver interface
		to go idle and acquire it.
	Created in Edit 6
----------------------------------------------------------------------*/

void lockbuss(int controller, int ldrive)
{
	while ((if_state[controller] & BUSSBUSY)) {
#ifdef DEBUGSLEEP
		printf("matcd%d: Can't do it now - going to sleep\n,
		       ldrive");
#endif /*DEBUGSLEEP*/
#ifdef DIAGPORT
		DIAGOUT(DIAGPORT,0xF1);	/*Show where we are*/
#endif /*DIAGPORT*/
		tsleep((caddr_t)&matcd_data->status, PRIBIO,
		       "matcdopen", 0);
	}
	if_state[controller] |= BUSSBUSY;	/*It's ours NOW*/
#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0xF2);		/*Show where we are*/
#endif /*DIAGPORT*/
#ifdef DEBUGSLEEP
	printf("matcd%d: BUSS locked in lockbuss\n",ldrive);
#endif /*DEBUGSLEEP*/
}


/*----------------------------------------------------------------------
	lockbuss - Wait for the buss on the requested driver interface
		to go idle and acquire it.
	Created in Edit 6
----------------------------------------------------------------------*/

void unlockbuss(int controller, int ldrive)
{
#ifdef DIAGPORT
	DIAGOUT(DIAGPORT,0xF4);		/*Show where we are*/
#endif /*DIAGPORT*/
	if_state[controller] &= ~BUSSBUSY;
#ifdef DEBUGSLEEP
	printf("matcd%d: bussunlocked\n",ldrive);
#endif /*DEBUGSLEEP*/
	wakeup((caddr_t)&matcd_data->status);	/*Wakeup other users*/
	matcd_start(&request_head[controller]);	/*Wake up any block I/O*/
}


/*----------------------------------------------------------------------
	media_chk - 	Checks error for types related to media
			changes.
----------------------------------------------------------------------*/

int media_chk(struct matcd_data *cd,int errnum,int ldrive)
{
	if (errnum==NOT_READY ||
	    errnum==MEDIA_CHANGED ||
	    errnum==HARD_RESET ||
	    errnum==DISC_OUT) {
		cd->flags &= ~MATCDLABEL;	/*Mark label as invalid*/

		if ((cd->flags & MATCDWARN)==0) {	/*Have we said this*/
			printf("matcd%d: Media changed - Further I/O aborted until device closed\n",ldrive);
			cd->flags |= MATCDWARN;
		}
		return(1);
	}
	return(0);
}


/*----------------------------------------------------------------------
	The following functions are related to the audio playback
	capabilities of the drive.   They can be omitted from the
	finished driver using the FULLDRIVER conditional.

	The full set of features the drive is capable of are currently
	not implemented but will be added in an upcoming release.
----------------------------------------------------------------------*/
#ifdef FULLDRIVER
/*----------------------------------------------------------------------
	matcd_playtracks - Plays one or more audio tracks
----------------------------------------------------------------------*/

static int matcd_playtracks(int ldrive, int cdrive, int controller,
			    struct ioc_play_track *pt)
{
	struct	matcd_data *cd;
	int	start,end;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	start=pt->start_track;
	end=pt->end_track;

	if (start < 1 ||		/*Starting track valid?*/
	    end < 1 ||			/*Ending track valid?*/
	    start > end || 		/*Start higher than end?*/
	    end > cd->volinfo.trk_high)	/*End track higher than disc size?*/
		return(ESPIPE);		/*Track out of range*/

	lockbuss(controller, ldrive);	/*Request buss*/
	i=matcd_setmode(ldrive, MODE_DA);/*Force drive into audio mode*/
	unlockbuss(controller, ldrive);	/*Release buss*/
	if (i!=0) {
		return(i);		/*Not legal for this media?*/
	}
	zero_cmd(cmd);
	cmd[0]=PLAYTRKS;		/*Play Audio Track/Index*/
	cmd[1]=start;
	cmd[2]=pt->start_index;
	cmd[3]=end;
	cmd[4]=pt->end_index;
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Play track results %d \n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(i);
}


/*----------------------------------------------------------------------
	matcd_playmsf - Plays between a range of blocks
----------------------------------------------------------------------*/

static int matcd_playmsf(int ldrive, int cdrive, int controller,
			    struct ioc_play_msf *pt)
{
	struct matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

#ifdef DEBUGIOCTL
	printf("matcd%d: playmsf %2x %2x %2x -> %2x %2x %2x\n",
	       ldrive,pt->start_m, pt->start_s, pt->start_f, pt->end_m,
	       pt->end_s,pt->end_f);
#endif /*DEBUGIOCTL*/

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	if ((cd->volinfo.vol_msf[0]==0 &&
	     cd->volinfo.vol_msf[1]<2) ||	/*Must be after 0'1"75F*/
	     msf_to_blk((char *)&pt->start_m) >
	     msf_to_blk((char *)&cd->volinfo.vol_msf)) {
#ifdef DEBUGIOCTL
	printf("matcd%d: Invalid block combination\n",ldrive);
#endif /*DEBUGIOCTL*/
		return(ESPIPE);		/*Track out of range*/
	}


	lockbuss(controller, ldrive);	/*Request buss*/
	i=matcd_setmode(ldrive, MODE_DA);/*Force drive into audio mode*/
	unlockbuss(controller, ldrive);	/*Release buss*/
	if (i!=0) {
		return(i);		/*Not legal for this media?*/
	}
	zero_cmd(cmd);
	cmd[0]=PLAYBLOCKS;		/*Play Audio Blocks*/
	cmd[1]=pt->start_m;
	cmd[2]=pt->start_s;
	cmd[3]=pt->start_f;
	cmd[4]=pt->end_m;
	cmd[5]=pt->end_s;
	cmd[6]=pt->end_f;
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
	return(i);
}


/*----------------------------------------------------------------------
	matcd_pause - Pause or Resume audio playback
----------------------------------------------------------------------*/

static int matcd_pause(int ldrive, int cdrive, int controller,
		       struct ioc_play_msf * addr,int action)
{
	struct	matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	zero_cmd(cmd);
	cmd[0]=PAUSE;			/*Pause or Resume playing audio*/
	cmd[1]=action;
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Pause / Resume results %d \n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(i);
}


/*----------------------------------------------------------------------
	matcd_stop  - Stop audio playback
----------------------------------------------------------------------*/

static int matcd_stop(int ldrive, int cdrive, int controller,
		       struct ioc_play_msf * addr)
{
	struct	matcd_data *cd;
	int	i,port;
	unsigned char cmd[MAXCMDSIZ];

	cd=&matcd_data[ldrive];
	port=cd->iobase;

	if ((cd->flags & MATCDLABEL)==0)
		return(EIO);		/*Refuse after chg error*/

	zero_cmd(cmd);
	cmd[0]=ABORT;			/*Abort playing audio*/
	i=docmd(cmd,ldrive,cdrive,controller,port);	/*Issue command*/
#ifdef DEBUGIOCTL
	printf("matcd%d: Abort results %d \n",ldrive,i);
#endif /*DEBUGIOCTL*/
	return(i);
}

#endif /*FULLDRIVER*/

/*End of matcd.c*/

