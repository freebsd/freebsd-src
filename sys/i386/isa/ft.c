/*
 *  Copyright (c) 1993 Steve Gerakines
 *
 *  This is freely redistributable software.  You may do anything you
 *  wish with it, so long as the above notice stays intact.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 *  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  ft.c - QIC-40/80 floppy tape driver
 *  $Id: ft.c,v 1.4 1994/02/14 22:24:28 nate Exp $
 *
 *
 *  01/26/94 v0.3b - Jim Babb
 *  Got rid of the hard coded device selection.  Moved (some of) the
 *  static variables into a structure for support of multiple devices.
 *  ( still has a way to go for 2 controllers - but closer )
 *  Changed the interface with fd.c so we no longer 'steal' it's 
 *  driver routine vectors.
 * 
 *  10/30/93 v0.3
 *  Fixed a couple more bugs.  Reading was sometimes looping when an
 *  an error such as address-mark-missing was encountered.  Both
 *  reading and writing was having more backup-and-retries than was
 *  necessary.  Added support to get hardware info.  Updated for use
 *  with FreeBSD.
 *
 *  09/15/93 v0.2 pl01
 *  Fixed a bunch of bugs:  extra isa_dmadone() in async_write() (shouldn't
 *  matter), fixed double buffering in async_req(), changed tape_end() in
 *  set_fdcmode() to reduce unexpected interrupts, changed end of track
 *  processing in async_req(), protected more of ftreq_rw() with an
 *  splbio().  Changed some of the ftreq_*() functions so that they wait
 *  for inactivity and then go, instead of aborting immediately.
 *
 *  08/07/93 v0.2 release
 *  Shifted from ftstrat to ioctl support for I/O.  Streaming is now much
 *  more reliable.  Added internal support for error correction, QIC-40,
 *  and variable length tapes.  Random access of segments greatly
 *  improved.  Formatting and verification support is close but still
 *  incomplete.
 *
 *  06/03/93 v0.1 Alpha release
 *  Hopefully the last re-write.  Many bugs fixed, many remain.
 */

#include "ft.h"
#if NFT > 0
#include "fd.h"

#include <sys/param.h>
#include <sys/dkbad.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/ftape.h>
#include <machine/pio.h>
#include "i386/isa/isa_device.h"
#include "i386/isa/fdreg.h"
#include "i386/isa/fdc.h"
#include "i386/isa/icu.h"
#include "i386/isa/rtc.h"
#include "ftreg.h"

/* Enable or disable debugging messages. */
#define FTDBGALL 0			/* everything */
/* #define DPRT(a) printf a		*/
#define DPRT(a) 		

/* Constants private to the driver */
#define FTPRI		(PRIBIO)	/* sleep priority */

/* The following items are needed from the fd driver. */
extern int in_fdc(int);			/* read fdc registers */
extern int out_fdc(int, int);		/* write fdc registers */

extern int hz;				/* system clock rate */

/* Type of tape attached */
/* use numbers that don't interfere with the possible floppy types */
#define NO_TYPE 0		/* (same as NO_TYPE in fd.c) */
			/* F_TAPE_TYPE must match value in fd.c */
#define F_TAPE_TYPE	0x020	/* bit for ft->types to indicate tape */
#define	FT_MOUNTAIN	(F_TAPE_TYPE | 1)
#define	FT_COLORADO	(F_TAPE_TYPE | 2)


/* Mode FDC is currently in: tape or disk */
enum { FDC_TAPE_MODE, FDC_DISK_MODE };

/* Command we are awaiting completion of */
enum { FTCMD_NONE, FTCMD_RESET, FTCMD_RECAL, FTCMD_SEEK, FTCMD_READID };

/* Tape interrupt status of current request */
enum { FTSTS_NONE, FTSTS_SNOOZE, FTSTS_INTERRUPT, FTSTS_TIMEOUT };

/* Tape I/O status */
enum {
	FTIO_READY,		/* No I/O activity */
	FTIO_READING,		/* Currently reading blocks */
	FTIO_RDAHEAD,		/* Currently reading ahead */
	FTIO_WRITING		/* Buffers are being written */
};

/* Current tape mode */
enum {
	FTM_PRIMARY,		/* Primary mode */
	FTM_VERIFY,		/* Verify mode */
	FTM_FORMAT,		/* Format mode */
	FTM_DIAG1,		/* Diagnostic mode 1 */
	FTM_DIAG2		/* Diagnostic mode 2 */
};

/* Tape geometries table */
QIC_Geom ftgtbl[] = {
	{ 0, 0, "Unformatted", "Unknown", 0,  0,     0,   0,     0 }, /* XXX */
	{ 1, 1, "QIC-40",  "205/550",	20,  68,  2176, 128, 21760 },
	{ 1, 2, "QIC-40",  "307.5/550",	20, 102,  3264, 128, 32640 },
	{ 1, 3, "QIC-40",  "295/900",	 0,   0,     0,   0,     0 }, /* ??? */
	{ 1, 4, "QIC-40",  "1100/550",	20, 365, 11680, 128, 32512 },
	{ 1, 5, "QIC-40",  "1100/900",	 0,   0,     0,   0,     0 }, /* ??? */
	{ 2, 1, "QIC-80",  "205/550",	28, 100,  3200, 128, 19200 },
	{ 2, 2, "QIC-80",  "307.5/550",	28, 150,  4800, 128, 19200 },
	{ 2, 3, "QIC-80",  "295/900",	 0,   0,     0,   0,     0 }, /* ??? */
	{ 2, 4, "QIC-80",  "1100/550",	28, 537, 17184, 128, 32512 },
	{ 2, 5, "QIC-80",  "1100/900",	 0,   0,     0,   0,     0 }, /* ??? */
	{ 3, 1, "QIC-500", "205/550",	 0,   0,     0,   0,     0 }, /* ??? */
	{ 3, 2, "QIC-500", "307.5/550",	 0,   0,     0,   0,     0 }, /* ??? */
	{ 3, 3, "QIC-500", "295/900",	 0,   0,     0,   0,     0 }, /* ??? */
	{ 3, 4, "QIC-500", "1100/550",	 0,   0,     0,   0,     0 }, /* ??? */
	{ 3, 5, "QIC-500", "1100/900",	 0,   0,     0,   0,     0 }  /* ??? */
};
#define NGEOM	(sizeof(ftgtbl) / sizeof(QIC_Geom))

QIC_Geom *ftg = NULL;			/* Current tape's geometry */

/*
 *  things relating to asynchronous commands
 */
static int astk_depth;		/* async_cmd stack depth */
static int awr_state;		/* state of async write */
static int ard_state;		/* state of async read */
static int arq_state;		/* state of async request */
static int async_retries;	/* retries, one per invocation */
static int async_func;		/* function to perform */
static int async_state;		/* state current function is at */
static int async_arg[5];	/* up to 5 arguments for async cmds */
static int async_ret;		/* return value */

/* List of valid async (interrupt driven) tape support functions. */
enum {
	ACMD_NONE, 		/* no command */
	ACMD_SEEK,		/* command seek */
	ACMD_STATUS,		/* report status */
	ACMD_STATE,		/* wait for state bits to be true */
	ACMD_SEEKSTS,		/* perform command and wait for status */
	ACMD_READID,		/* read id */
	ACMD_RUNBLK		/* ready tape for I/O on the given block */
};

/* Call another asyncronous command from within async_cmd(). */
#define CALL_ACMD(r,f,a,b,c,d,e) \
			astk[astk_depth].over_retries = async_retries; \
			astk[astk_depth].over_func = async_func; \
			astk[astk_depth].over_state = (r); \
			for (i = 0; i < 5; i++) \
			   astk[astk_depth].over_arg[i] = async_arg[i]; \
			async_func = (f); async_state = 0; async_retries = 0; \
			async_arg[0]=(a); async_arg[1]=(b); async_arg[2]=(c); \
			async_arg[3]=(d); async_arg[4]=(e); \
			astk_depth++; \
			goto restate

/* Perform an asyncronous command from outside async_cmd(). */
#define ACMD_FUNC(r,f,a,b,c,d,e) over_async = (r); astk_depth = 0; \
			async_func = (f); async_state = 0; async_retries = 0; \
			async_arg[0]=(a); async_arg[1]=(b); async_arg[2]=(c); \
			async_arg[3]=(d); async_arg[4]=(e); \
			async_cmd(ftu); \
			return

/* Various wait channels */
static struct {
	int buff_avail;
	int iosts_change;
	int long_delay;
	int intr_wait;
} ftsem;

/***********************************************************************\
* Per controller structure.						*
\***********************************************************************/
extern struct fdc_data fdc_data[NFDC];

/***********************************************************************\
* Per tape drive structure.						*
\***********************************************************************/
struct ft_data {
	struct	fdc_data *fdc;	/* pointer to controller structure */
	int	ftsu;		/* this units number on this controller */
	int	type;		/* Drive type (Mountain, Colorado) */
/*	QIC_Geom *ftg;	*/	/* pointer to Current tape's geometry */
	int	flags;
	int	cmd_wait;	/* Command we are awaiting completion of */
	int	sts_wait;	/* Tape interrupt status of current request */
	int	io_sts;		/* Tape I/O status */
	int	mode;
	int	pcn;		/* present cylinder number */
	int	attaching;	/* true when ft is attaching */
	unsigned char *xptr;	/* pointer to buffer blk to xfer  */
	int	xcnt;		/* transfer count                 */
	int	xblk;		/* block number to transfer       */
	SegReq *curseg;		/* Current segment to do I/O on	  */
	SegReq *bufseg;		/* Buffered segment to r/w ahead  */
				/* the next 3 should be defines in 'flags' */
	int active;		/* TRUE if transfer is active	  */
	int rdonly;		/* TRUE if tape is read-only	  */
	int newcart;		/* TRUE if new cartridge detected */
	int laststs;		/* last reported status code      */
	int lastcfg;		/* last reported QIC config	  */
	int lasterr;		/* last QIC error code 		  */
	int lastpos;		/* last known segment number	  */
	int moving;		/* TRUE if tape is moving	  */
	int rid[7];		/* read_id return values	  */

} ft_data[NFT];

/***********************************************************************\
* Throughout this file the following conventions will be used:		*
* ft is a pointer to the ft_data struct for the drive in question	*
* fdc is a pointer to the fdc_data struct for the controller		*
* ftu is the tape drive unit number					*
* fdcu is the floppy controller unit number				*
* ftsu is the tape drive unit number on that controller. (sub-unit)	*
\***********************************************************************/


typedef int	ftu_t;
typedef int	ftsu_t;
typedef	struct ft_data *ft_p;

#define	id_physid id_scsiid	/* this biotab field doubles as a field */
				/* for the physical unit number on the controller */

int ftopen(dev_t, int);
int ftclose(dev_t, int);
void ftstrategy(struct buf *);
int ftioctl(dev_t, int, caddr_t, int, struct proc *);
int ftdump(dev_t);
int ftsize(dev_t);
static void ft_timeout(caddr_t arg1, int arg2);
void async_cmd(ftu_t);
void async_req(ftu_t, int);
void async_read(ftu_t, int);
void async_write(ftu_t, int);
void tape_start(ftu_t);
void tape_end(ftu_t);
void tape_inactive(ftu_t);





/*
 *  Probe/attach floppy tapes.
 */
int ftattach(isadev, fdup)
	struct isa_device *isadev, *fdup;
{
	fdcu_t	fdcu = isadev->id_unit;		/* fdc active unit */
	fdc_p	fdc = fdc_data + fdcu;	/* pointer to controller structure */
	ftu_t	ftu = fdup->id_unit;
	ft_p	ft;
	ftsu_t	ftsu = fdup->id_physid;

	if (ftu >= NFT)
		return 0;
	ft = &ft_data[ftu];
				/* Probe for tape */
	ft->attaching = 1;
	ft->type = NO_TYPE;
	ft->fdc = fdc;
	ft->ftsu = ftsu;

	tape_start(ftu);	/* ready controller for tape */
	tape_cmd(ftu, QC_COL_ENABLE1);
	tape_cmd(ftu, QC_COL_ENABLE2);
	if (tape_status(ftu) >= 0) {
		ft->type = FT_COLORADO;
		fdc->flags |= FDC_HASFTAPE;
		printf(" [%d: ft%d: Colorado tape]",
			 fdup->id_physid, fdup->id_unit );
		tape_cmd(ftu, QC_COL_DISABLE);
		goto out;
	}

	tape_start(ftu);	/* ready controller for tape */
	tape_cmd(ftu, QC_MTN_ENABLE1);
	tape_cmd(ftu, QC_MTN_ENABLE2);
	if (tape_status(ftu) >= 0) {
		ft->type = FT_MOUNTAIN;
		fdc->flags |= FDC_HASFTAPE;
		printf(" [%d: ft%d: Mountain tape]",
			 fdup->id_physid, fdup->id_unit );
		tape_cmd(ftu, QC_MTN_DISABLE);
		goto out;
	}

out:
	tape_end(ftu);
	ft->attaching = 0;
	return(ft->type);
}


/*
 *  Perform common commands asynchronously.
 */
void async_cmd(ftu_t ftu) {
	ft_p	ft = &ft_data[ftu];
	fdcu_t	fdcu = ft->fdc->fdcu;
	int cmd, i, st0, st3, pcn;
	static int bitn, retval, retpos, nbits, newcn;
	static struct {
		int over_func;
		int over_state;
		int over_retries;
		int over_arg[5];
	} astk[15];
	static int wanttrk, wantblk, wantdir;
	static int curpos, curtrk, curblk, curdir, curdiff;
	static int errcnt = 0;

restate:
#if FTDBGALL
  DPRT(("async_cmd state: func: %d  state: %d\n", async_func, async_state));
#endif
  switch(async_func) {
     case ACMD_SEEK:
	/*
	 *  Arguments:
	 *     0 - command to perform
	 */
	switch (async_state) {
	    case 0:
		cmd = async_arg[0];
#if FTDBGALL
		DPRT(("===>async_seek cmd = %d\n", cmd));
#endif
		newcn = (cmd <= ft->pcn) ? ft->pcn - cmd : ft->pcn + cmd;
		async_state = 1;
		i = 0;
		if (out_fdc(fdcu, NE7CMD_SEEK) < 0) i = 1;
		if (!i && out_fdc(fdcu, 0x00) < 0) i = 1;
		if (!i && out_fdc(fdcu, newcn) < 0) i = 1;
		if (i) {
			if (++async_retries >= 10) {
				DPRT(("ft%d: async_cmd command seek failed!!\n", ftu));
				goto complete;
			}
			DPRT(("ft%d: async_cmd command seek retry...\n",ftu));
			async_state = 0;
			goto restate;
		}
		break;
	    case 1:
		out_fdc(fdcu, NE7CMD_SENSEI);
		st0 = in_fdc(fdcu);
		pcn = in_fdc(fdcu);
		if (st0 < 0 || pcn < 0 || newcn != pcn) {
			if (++async_retries >= 10) {
				DPRT(("ft%d: async_cmd seek retries exceeded\n",ftu));
				goto complete;
			}
			DPRT(("ft%d: async_cmd command bad st0=$%02x pcn=$%02x\n",
							ftu, st0, pcn));
			async_state = 0;
			timeout(ft_timeout, (caddr_t)ftu, hz/10);
			break;
		}
		if (st0 & 0x20)	{ 	/* seek done */
			ft->pcn = pcn;
		}
#if FTDBGALL
		 else
			DPRT(("ft%d: async_seek error st0 = $%02x pcn = %d\n",
							ftu, st0, pcn));
#endif
		if (async_arg[1]) goto complete;
		async_state = 2;
		timeout(ft_timeout, (caddr_t)ftu, hz/50);
		break;
	    case 2:
		goto complete;
		/* NOTREACHED */
	}
	break;

     case ACMD_STATUS:
	/*
	 *  Arguments:
	 *     0 - command to issue report from
	 *     1 - number of bits
	 *  modifies: bitn, retval, st3
	 */
	switch (async_state) {
	    case 0:
		bitn = 0;
		retval = 0;
		cmd = async_arg[0];
		nbits = async_arg[1];
		DPRT(("async_status got cmd = %d nbits = %d\n", cmd,nbits));
		CALL_ACMD(5, ACMD_SEEK, QC_NEXTBIT, 0, 0, 0, 0);
		/* NOTREACHED */
	    case 1:
		out_fdc(fdcu, NE7CMD_SENSED);
		out_fdc(fdcu, 0x00);
		st3 = in_fdc(fdcu);
		if (st3 < 0) {
		DPRT(("ft%d: async_status timed out on bit %d r=$%02x\n",
					ftu,bitn,retval));
			async_ret = -1;
			goto complete;
		}
		if ((st3 & 0x10) != 0) retval |= (1 << bitn);
		bitn++;
		if (bitn >= (nbits+2)) {
			if ((retval & 1) && (retval & (1 << (nbits+1)))) {
				async_ret = (retval & ~(1<<(nbits+1))) >> 1;
				if (async_arg[0] == QC_STATUS && async_arg[2] == 0 &&
				   (async_ret & (QS_ERROR|QS_NEWCART))) {
					async_state = 2;
					goto restate;
				}
			DPRT(("async status got $%04x ($%04x)\n", async_ret,retval));
			} else {
				DPRT(("ft%d: async_status failed: retval=$%04x nbits=%d\n",
						ftu, retval,nbits));
				async_ret = -2; 
			}
			goto complete;
		}
		CALL_ACMD(1, ACMD_SEEK, QC_NEXTBIT, 0, 0, 0, 0);
		/* NOTREACHED */
	    case 2:
		if (async_ret & QS_NEWCART) ft->newcart = 1;
		CALL_ACMD(3, ACMD_STATUS, QC_ERRCODE, 16, 1, 0, 0);
	    case 3:
		ft->lasterr = async_ret;
		if ((ft->lasterr & QS_NEWCART) == 0 && ft->lasterr) {
			DPRT(("ft%d: QIC error %d occurred on cmd %d\n",
				ftu, ft->lasterr & 0xff, ft->lasterr >> 8));
		}
	        cmd = async_arg[0];
		nbits = async_arg[1];
		CALL_ACMD(4, ACMD_STATUS, QC_STATUS, 8, 1, 0, 0);
	    case 4:
		goto complete;
	    case 5:
		CALL_ACMD(6, ACMD_SEEK, QC_NEXTBIT, 0, 0, 0, 0);
	    case 6:
		CALL_ACMD(7, ACMD_SEEK, QC_NEXTBIT, 0, 0, 0, 0);
	    case 7:
		CALL_ACMD(8, ACMD_SEEK, QC_NEXTBIT, 0, 0, 0, 0);
	    case 8:
		cmd = async_arg[0];
		CALL_ACMD(1, ACMD_SEEK, cmd, 0, 0, 0, 0);
	}
	break;

     case ACMD_STATE:
	/*
	 *  Arguments:
	 *     0 - status bits to check
	 */
	switch(async_state) {
	    case 0:
		CALL_ACMD(1, ACMD_STATUS, QC_STATUS, 8, 0, 0, 0);
	    case 1:
		if ((async_ret & async_arg[0]) != 0) goto complete;
		async_state = 0;
		if (++async_retries == 360) {	/* 90 secs. */
			DPRT(("ft%d: acmd_state exceeded retry count\n", ftu));
			goto complete;
		}
		timeout(ft_timeout, (caddr_t)ftu, hz/4);
		break;
	}
	break;

     case ACMD_SEEKSTS:
	/*
	 *  Arguments:
	 *     0 - command to perform
	 *     1 - status bits to check
	 *     2 - (optional) seconds to wait until completion
	 */
	switch(async_state) {
	    case 0:
		cmd = async_arg[0];
		async_retries = (async_arg[2]) ? (async_arg[2]*4) : 10;
		CALL_ACMD(1, ACMD_SEEK, cmd, 0, 0, 0, 0);
	    case 1:
		CALL_ACMD(2, ACMD_STATUS, QC_STATUS, 8, 0, 0, 0);
	    case 2:
		if ((async_ret & async_arg[1]) != 0) goto complete;
		if (--async_retries == 0) {
			DPRT(("ft%d: acmd_seeksts retries exceeded\n", ftu));
			goto complete;
		}
		async_state = 1;
		timeout(ft_timeout, (caddr_t)ftu, hz/4);
		break;
	}
	break;

     case ACMD_READID:
	/*
	 *  Arguments: (none)
	 */
	switch(async_state) {
	    case 0:
		if (!ft->moving) {
			CALL_ACMD(4, ACMD_SEEKSTS, QC_STOP, QS_READY, 0, 0, 0);
			/* NOTREACHED */
		}
		async_state = 1;
		out_fdc(fdcu, 0x4a);		/* READ_ID */
		out_fdc(fdcu, 0);
		break;
	    case 1:
		for (i = 0; i < 7; i++) ft->rid[i] = in_fdc(fdcu);
		async_ret = (ft->rid[3]*ftg->g_fdtrk) +
			    (ft->rid[4]*ftg->g_fdside) + ft->rid[5] - 1;
		DPRT(("readid st0:%02x st1:%02x st2:%02x c:%d h:%d s:%d pos:%d\n",
			ft->rid[0], ft->rid[1], ft->rid[2], ft->rid[3],
			ft->rid[4], ft->rid[5], async_ret));
		if ((ft->rid[0] & 0xc0) == 0x40) {
			if (++errcnt >= 10) {
				DPRT(("ft%d: acmd_readid errcnt exceeded\n", fdcu));
				async_ret = ft->lastpos;
				errcnt = 0;
				goto complete;
			}
			if (errcnt > 2) {
				ft->moving = 0;
				CALL_ACMD(4, ACMD_SEEKSTS, QC_STOP, QS_READY, 0, 0, 0);
			}
			DPRT(("readid retry...\n"));
			async_state = 0;
			goto restate;
		}
		if ((async_ret % ftg->g_blktrk) == (ftg->g_blktrk-1)) {
			DPRT(("acmd_readid detected last block on track\n"));
			retpos = async_ret;
			CALL_ACMD(2, ACMD_STATE, QS_BOT|QS_EOT, 0, 0, 0, 0);
			/* NOTREACHED */
		}
		ft->lastpos = async_ret;
		errcnt = 0;
		goto complete;
		/* NOTREACHED */
	    case 2:
		CALL_ACMD(3, ACMD_STATE, QS_READY, 0, 0, 0, 0);
	    case 3:
		ft->moving = 0;
		async_ret = retpos+1;
		goto complete;
	    case 4:
		CALL_ACMD(5, ACMD_SEEK, QC_FORWARD, 0, 0, 0, 0);
	    case 5:
		ft->moving = 1;
		async_state = 0;
		timeout(ft_timeout, (caddr_t)ftu, hz/10); /* XXX */
		break;
	}
	break;

     case ACMD_RUNBLK:
	/*
	 *  Arguments:
	 *     0 - block number I/O will be performed on
	 *
	 *  modifies: curpos
	 */
	switch (async_state) {
	    case 0:
		wanttrk = async_arg[0] / ftg->g_blktrk;
		wantblk = async_arg[0] % ftg->g_blktrk;
		wantdir = wanttrk & 1;
		ft->moving = 0;
		CALL_ACMD(1, ACMD_SEEKSTS, QC_STOP, QS_READY, 0, 0, 0);
	    case 1:
		curtrk = wanttrk;
		curdir = curtrk & 1;
		DPRT(("Changing to track %d\n", wanttrk));
		CALL_ACMD(2, ACMD_SEEK, QC_SEEKTRACK, 0, 0, 0, 0);
	    case 2:
		cmd = wanttrk+2;
		CALL_ACMD(3, ACMD_SEEKSTS, cmd, QS_READY, 0, 0, 0);
	    case 3:
		CALL_ACMD(4, ACMD_STATUS, QC_STATUS, 8, 0, 0, 0);
	    case 4:
		ft->laststs = async_ret;
		if (wantblk == 0) {
			curblk = 0;
			cmd = (wantdir) ? QC_SEEKEND : QC_SEEKSTART;
			CALL_ACMD(6, ACMD_SEEKSTS, cmd, QS_READY, 90, 0, 0);
		}
		if (ft->laststs & QS_BOT) {
			DPRT(("Tape is at BOT\n"));
			curblk = (wantdir) ? 4800 : 0;
			async_state = 6;
			goto restate;
		}
		if (ft->laststs & QS_EOT) {
			DPRT(("Tape is at EOT\n"));
			curblk = (wantdir) ? 0 : 4800;
			async_state = 6;
			goto restate;
		}
		CALL_ACMD(5, ACMD_READID, 0, 0, 0, 0, 0);
	    case 5:
		curtrk = (async_ret+1) / ftg->g_blktrk;
		curblk = (async_ret+1) % ftg->g_blktrk;
		DPRT(("gotid: curtrk=%d wanttrk=%d curblk=%d wantblk=%d\n",
			curtrk, wanttrk, curblk, wantblk));
		if (curtrk != wanttrk) {	/* oops! */
			DPRT(("oops!! wrong track!\n"));
			CALL_ACMD(1, ACMD_SEEKSTS, QC_STOP, QS_READY, 0, 0, 0);
		}
		async_state = 6;
		goto restate;
	    case 6:
		DPRT(("curtrk = %d nextblk = %d\n", curtrk, curblk));
		if (curblk == wantblk) {
			ft->lastpos = curblk - 1;
			async_ret = ft->lastpos;
			if (ft->moving) goto complete;
			CALL_ACMD(7, ACMD_STATE, QS_READY, 0, 0, 0, 0);
		}
		if (curblk > wantblk) {		/* passed it */
			ft->moving = 0;
			CALL_ACMD(10, ACMD_SEEKSTS, QC_STOP, QS_READY, 0, 0, 0);
		}
		if ((wantblk - curblk) <= 96) {	/* approaching it */
			CALL_ACMD(5, ACMD_READID, 0, 0, 0, 0, 0);
		}
		/* way up ahead */
		ft->moving = 0;
		CALL_ACMD(14, ACMD_SEEKSTS, QC_STOP, QS_READY, 0, 0, 0);
		break;
	    case 7:
		ft->moving = 1;
		CALL_ACMD(8, ACMD_SEEK, QC_FORWARD, 0, 0, 0, 0);
		break;
	    case 8:
		async_state = 9;
		timeout(ft_timeout, (caddr_t)ftu, hz/10);  /* XXX */
		break;
	    case 9:
		goto complete;
	    case 10:
		curdiff = ((curblk - wantblk) / QCV_BLKSEG) + 2;
		if (curdiff >= ftg->g_segtrk) curdiff = ftg->g_segtrk - 1;
		DPRT(("pos %d past %d, reverse %d\n", curblk, wantblk, curdiff));
		CALL_ACMD(11, ACMD_SEEK, QC_SEEKREV, 0, 0, 0, 0);
	    case 11:
		DPRT(("reverse 1 done\n"));
		CALL_ACMD(12, ACMD_SEEK, (curdiff & 0xf)+2, 0, 0, 0, 0);
	    case 12:
		DPRT(("reverse 2 done\n"));
		CALL_ACMD(13, ACMD_SEEKSTS, ((curdiff>>4)&0xf)+2, QS_READY, 90, 0, 0);
	    case 13:
		CALL_ACMD(5, ACMD_READID, 0, 0, 0, 0, 0);
	    case 14:
		curdiff = ((wantblk - curblk) / QCV_BLKSEG) - 2;
		if (curdiff < 0) curdiff = 0;
		DPRT(("pos %d before %d, forward %d\n", curblk, wantblk, curdiff));
		CALL_ACMD(15, ACMD_SEEK, QC_SEEKFWD, 0, 0, 0, 0);
	    case 15:
		DPRT(("forward 1 done\n"));
		CALL_ACMD(16, ACMD_SEEK, (curdiff & 0xf)+2, 0, 0, 0, 0);
	    case 16:
		DPRT(("forward 2 done\n"));
		CALL_ACMD(13, ACMD_SEEKSTS, ((curdiff>>4)&0xf)+2, QS_READY, 90, 0, 0);
	}
	break;
  }

  return;

complete:
  if (astk_depth) {
	astk_depth--;
	async_retries = astk[astk_depth].over_retries;
	async_func = astk[astk_depth].over_func;
	async_state = astk[astk_depth].over_state;
	for(i = 0; i < 5; i++)
		async_arg[i] = astk[astk_depth].over_arg[i];
	goto restate;
  }
  async_func = ACMD_NONE;
  async_state = 0;
  switch (ft->io_sts) {
     case FTIO_READY:
	async_req(ftu, 2);
	break;
     case FTIO_READING:
	async_read(ftu, 2);
	break;
     case FTIO_WRITING:
	async_write(ftu, 2);
	break;
     default:
	DPRT(("ft%d: bad async_cmd ending I/O state!\n", ftu));
	break;
  }
}


/*
 *  Entry point for the async request processor.
 */
void async_req(ftu_t ftu, int from)
{
  ft_p	ft = &ft_data[ftu];
  SegReq *sp;
  static int over_async, lastreq, domore;
  int cmd;

  if (from == 2) arq_state = over_async;

restate:
  switch (arq_state) {
     case 0:	/* Process segment */
	ft->io_sts = ft->curseg->reqtype;
	if (ft->io_sts == FTIO_WRITING)
		async_write(ftu, from);
	else
		async_read(ftu, from);
	if (ft->io_sts != FTIO_READY) return;

	/* Swap buffered and current segment */
	lastreq = ft->curseg->reqtype;
	ft->curseg->reqtype = FTIO_READY;
	sp = ft->curseg;
	ft->curseg = ft->bufseg;
	ft->bufseg = sp;

	wakeup((caddr_t)&ftsem.buff_avail);

	/* Detect end of track */
	if (((ft->xblk / QCV_BLKSEG) % ftg->g_segtrk) == 0) {
		domore = (ft->curseg->reqtype != FTIO_READY);
		ACMD_FUNC(2, ACMD_STATE, QS_BOT|QS_EOT, 0, 0, 0, 0);
	}
	arq_state = 1;
	goto restate;

     case 1:	/* Next request */
	if (ft->curseg->reqtype != FTIO_READY) {
		ft->curseg->reqcrc = 0;
		arq_state = ard_state = awr_state = 0;
		ft->xblk = ft->curseg->reqblk;
		ft->xcnt = 0;
		ft->xptr = ft->curseg->buff;
		DPRT(("I/O reqblk = %d\n", ft->curseg->reqblk));
		goto restate;
	}
	if (lastreq == FTIO_READING) {
		ft->curseg->reqtype = FTIO_RDAHEAD;
		ft->curseg->reqblk = ft->xblk;
		ft->curseg->reqcrc = 0;
		ft->curseg->reqcan = 0;
		bzero(ft->curseg->buff, QCV_SEGSIZE);
		arq_state = ard_state = awr_state = 0;
		ft->xblk = ft->curseg->reqblk;
		ft->xcnt = 0;
		ft->xptr = ft->curseg->buff;
		DPRT(("Processing readahead reqblk = %d\n", ft->curseg->reqblk));
		goto restate;
	}
	if (ft->moving) {
		DPRT(("No more I/O.. Stopping.\n"));
		ACMD_FUNC(7, ACMD_SEEKSTS, QC_STOP, QS_READY, 0, 0, 0);
		break;
	}
	arq_state = 7;
	goto restate;

     case 2:	/* End of track */
	ft->moving = 0;
	ACMD_FUNC(3, ACMD_STATE, QS_READY, 0, 0, 0, 0);
	break;

     case 3:
	DPRT(("async_req seek head to track %d\n", ft->xblk / ftg->g_blktrk));
	ACMD_FUNC(4, ACMD_SEEK, QC_SEEKTRACK, 0, 0, 0, 0);
	break;

     case 4:
	cmd = (ft->xblk / ftg->g_blktrk) + 2;
	if (domore) {
		ACMD_FUNC(5, ACMD_SEEKSTS, cmd, QS_READY, 0, 0, 0);
	} else {
		ACMD_FUNC(7, ACMD_SEEKSTS, cmd, QS_READY, 0, 0, 0);
	}
	break;

     case 5:
	ft->moving = 1;
	ACMD_FUNC(6, ACMD_SEEK, QC_FORWARD, 0, 0, 0, 0);
	break;

     case 6:
	arq_state = 1;
	timeout(ft_timeout, (caddr_t)ftu, hz/10); /* XXX */
	break;

     case 7:
	ft->moving = 0;

	/* Check one last time to see if a request came in. */
	if (ft->curseg->reqtype != FTIO_READY) {
		DPRT(("async_req: Never say no!\n"));
		arq_state = 1;
		goto restate;
	}

	/* Time to rest. */
	ft->active = 0;
	wakeup((caddr_t)&ftsem.iosts_change);	/* wakeup those who want an i/o chg */
	break;
  }
}

/*
 *  Entry for async read.
 */
void async_read(ftu_t ftu, int from)
{
  ft_p ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */
  unsigned long paddr;
  int i, cmd, newcn, rddta[7];
  int st0, pcn, where;
  static int over_async;

  if (from == 2) ard_state = over_async;

restate:
#if FTDBGALL
  DPRT(("async_read: state: %d  from = %d\n", ard_state, from));
#endif
  switch (ard_state) {
     case 0:	/* Start off */
	/* If tape is not at desired position, stop and locate */
	if (ft->lastpos != (ft->xblk-1)) {
		DPRT(("ft%d: position unknown: lastpos:%d ft->xblk:%d\n",
			ftu, ft->lastpos, ft->xblk));
		ACMD_FUNC(1, ACMD_RUNBLK, ft->xblk, 0, 0, 0, 0);
	}

	/* Tape is in position but stopped. */
	if (!ft->moving) {
		DPRT(("async_read ******STARTING TAPE\n"));
		ACMD_FUNC(3, ACMD_STATE, QS_READY, 0, 0, 0, 0);
	}
	ard_state = 1;
	goto restate;

     case 1:	/* Start DMA */
	/* Tape is now moving and in position-- start DMA now! */
	isa_dmastart(B_READ, ft->xptr, QCV_BLKSIZE, 2);
	out_fdc(fdcu, 0x66);				/* read */
	out_fdc(fdcu, 0x00);				/* unit */
	out_fdc(fdcu, (ft->xblk % ftg->g_fdside) / ftg->g_fdtrk);	/* cylinder */
	out_fdc(fdcu, ft->xblk / ftg->g_fdside);		/* head */
	out_fdc(fdcu, (ft->xblk % ftg->g_fdtrk) + 1);	/* sector */
	out_fdc(fdcu, 0x03);				/* 1K sectors */
	out_fdc(fdcu, (ft->xblk % ftg->g_fdtrk) + 1);	/* count */
	out_fdc(fdcu, 0x74);				/* gap length */
	out_fdc(fdcu, 0xff);				/* transfer size */
	ard_state = 2;
	break;

     case 2:	/* DMA completed */
	/* Transfer complete, get status */
	for (i = 0; i < 7; i++) rddta[i] = in_fdc(fdcu);
	isa_dmadone(B_READ, ft->xptr, QCV_BLKSIZE, 2);

#if FTDBGALL
	/* Compute where the controller thinks we are */
	where = (rddta[3]*ftg->g_fdtrk) + (rddta[4]*ftg->g_fdside) + rddta[5]-1;
	DPRT(("xfer done: st0:%02x st1:%02x st2:%02x c:%d h:%d s:%d pos:%d want:%d\n",
	    rddta[0], rddta[1], rddta[2], rddta[3], rddta[4], rddta[5],
	    where, ft->xblk));
#endif

	/* Check for errors */
	if ((rddta[0] & 0xc0) != 0x00) {
		if (rddta[1] & 0x04) {
			/* Probably wrong position */
			ft->lastpos = ft->xblk;
			ard_state = 0;
			goto restate;
		} else {
			/* CRC/Address-mark/Data-mark, et. al. */
			DPRT(("ft%d: CRC error on block %d\n", fdcu, ft->xblk));
			ft->curseg->reqcrc |= (1 << ft->xcnt);
		}
	}

	/* Otherwise, transfer completed okay. */
	ft->lastpos = ft->xblk;
	ft->xblk++;
	ft->xcnt++;
	ft->xptr += QCV_BLKSIZE;
	if (ft->xcnt < QCV_BLKSEG && ft->curseg->reqcan == 0) {
		ard_state = 0;
		goto restate;
	}
	DPRT(("Read done..  Cancel = %d\n", ft->curseg->reqcan));
	ft->io_sts = FTIO_READY;
	break;

     case 3:
	ft->moving = 1;
	ACMD_FUNC(4, ACMD_SEEK, QC_FORWARD, 0, 0, 0, 0);
	break;

     case 4:
	ard_state = 1;
	timeout(ft_timeout, (caddr_t)ftu, hz/10);  /* XXX */
	break;

     default:
	DPRT(("ft%d: bad async_read state %d!!\n", ftu, ard_state));
	break;
  }
}


/*
 *  Entry for async write.  If from is 0, this came from the interrupt
 *  routine, if it's 1 then it was a timeout, if it's 2, then an
 *  async_cmd completed.
 */
void async_write(ftu_t ftu, int from)
{
  ft_p ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */
  unsigned long paddr;
  int i, cmd, newcn, rddta[7];
  int st0, pcn, where;
  static int over_async;
  static int retries = 0;

  if (from == 2) awr_state = over_async;

restate:
#if FTDBGALL
  DPRT(("async_write: state: %d  from = %d\n", awr_state, from));
#endif
  switch (awr_state) {
     case 0:	/* Start off */
	/* If tape is not at desired position, stop and locate */
	if (ft->lastpos != (ft->xblk-1)) {
		DPRT(("ft%d: position unknown: lastpos:%d ft->xblk:%d\n",
			ftu, ft->lastpos, ft->xblk));
		ACMD_FUNC(1, ACMD_RUNBLK, ft->xblk, 0, 0, 0, 0);
	}

	/* Tape is in position but stopped. */
	if (!ft->moving) {
		DPRT(("async_write ******STARTING TAPE\n"));
		ACMD_FUNC(3, ACMD_STATE, QS_READY, 0, 0, 0, 0);
	}
	awr_state = 1;
	goto restate;

     case 1:	/* Start DMA */
	/* Tape is now moving and in position-- start DMA now! */
	isa_dmastart(B_WRITE, ft->xptr, QCV_BLKSIZE, 2);
	out_fdc(fdcu, 0x45);				/* write */
	out_fdc(fdcu, 0x00);				/* unit */
	out_fdc(fdcu, (ft->xblk % ftg->g_fdside) / ftg->g_fdtrk);	/* cylinder */
	out_fdc(fdcu, ft->xblk / ftg->g_fdside);		/* head */
	out_fdc(fdcu, (ft->xblk % ftg->g_fdtrk) + 1);	/* sector */
	out_fdc(fdcu, 0x03);				/* 1K sectors */
	out_fdc(fdcu, (ft->xblk % ftg->g_fdtrk) + 1);	/* count */
	out_fdc(fdcu, 0x74);				/* gap length */
	out_fdc(fdcu, 0xff);				/* transfer size */
	awr_state = 2;
	break;

     case 2:	/* DMA completed */
	/* Transfer complete, get status */
	for (i = 0; i < 7; i++) rddta[i] = in_fdc(fdcu);
	isa_dmadone(B_WRITE, ft->xptr, QCV_BLKSIZE, 2);

#if FTDBGALL
	/* Compute where the controller thinks we are */
	where = (rddta[3]*ftg->g_fdtrk) + (rddta[4]*ftg->g_fdside) + rddta[5]-1;
	DPRT(("xfer done: st0:%02x st1:%02x st2:%02x c:%d h:%d s:%d pos:%d want:%d\n",
	    rddta[0], rddta[1], rddta[2], rddta[3], rddta[4], rddta[5],
	    where, ft->xblk));
#endif

	/* Check for errors */
	if ((rddta[0] & 0xc0) != 0x00) {
		if (rddta[1] & 0x04) {
			/* Probably wrong position */
			ft->lastpos = ft->xblk;
			awr_state = 0;
			goto restate;
		} else if (retries < 5) {
			/* Something happened -- try again */
			ft->lastpos = ft->xblk;
			awr_state = 0;
			retries++;
			goto restate;
		} else {
			/*
			 *  Retries failed.  Note the unrecoverable error.
			 *  Marking the block as bad is fairly useless.
			 */ 
			printf("ft%d: unrecoverable write error on block %d\n",
					ftu, ft->xblk);
			ft->curseg->reqcrc |= (1 << ft->xcnt);
		}
	}

	/* Otherwise, transfer completed okay. */
	retries = 0;
	ft->lastpos = ft->xblk;
	ft->xblk++;
	ft->xcnt++;
	ft->xptr += QCV_BLKSIZE;
	if (ft->xcnt < QCV_BLKSEG) {
		awr_state = 0;	/* next block */
		goto restate;
	}
#if FTDBGALL
	DPRT(("Write done.\n"));
#endif
	ft->io_sts = FTIO_READY;
	break;

     case 3:
	ft->moving = 1;
	ACMD_FUNC(4, ACMD_SEEK, QC_FORWARD, 0, 0, 0, 0);
	break;

     case 4:
	awr_state = 1;
	timeout(ft_timeout, (caddr_t)ftu, hz/10); /* XXX */
	break;

     default:
	DPRT(("ft%d: bad async_write state %d!!\n", ftu, awr_state));
	break;
  }
}


/*
 *  Interrupt handler for active tape.  Bounced off of fdintr().
 */
int ftintr(ftu_t ftu)
{
  int st0, pcn, i;
  ft_p	ft = &ft_data[ftu];
	fdcu_t	fdcu = ft->fdc->fdcu;		/* fdc active unit */
  st0 = 0;
  pcn = 0;

  /* I/O segment transfer completed */
  if (ft->active) {
	if (async_func != ACMD_NONE) {
		async_cmd(ftu);
		return(1);
	}
#if FTDBGALL
	DPRT(("Got request interrupt\n"));
#endif
	async_req(ftu, 0);
	return(1);
  }

  /* Get interrupt status */
  if (ft->cmd_wait != FTCMD_READID) {
	out_fdc(fdcu, NE7CMD_SENSEI);
	st0 = in_fdc(fdcu);
	pcn = in_fdc(fdcu);
  }

  if (ft->cmd_wait == FTCMD_NONE || ft->sts_wait != FTSTS_SNOOZE) {
huh_what:
	printf("ft%d: unexpected interrupt; st0 = $%02x pcn = %d\n",
				ftu, st0, pcn);
	return(1);
  }

  switch (ft->cmd_wait) {
     case FTCMD_RESET:
	ft->sts_wait = FTSTS_INTERRUPT;
	wakeup((caddr_t)&ftsem.intr_wait);
	break;
     case FTCMD_RECAL:
     case FTCMD_SEEK:
	if (st0 & 0x20)	{ 	/* seek done */
		ft->sts_wait = FTSTS_INTERRUPT;
		ft->pcn = pcn;
		wakeup((caddr_t)&ftsem.intr_wait);
	}
#if FTDBGALL
	else
		DPRT(("ft%d: seek error st0 = $%02x pcn = %d\n",
						ftu, st0, pcn));
#endif
	break;
     case FTCMD_READID:
	for (i = 0; i < 7; i++) ft->rid[i] = in_fdc(fdcu);
	ft->sts_wait = FTSTS_INTERRUPT;
	wakeup((caddr_t)&ftsem.intr_wait);
	break;

     default:
	goto huh_what;
  }

  return(1);
}

/*
 *  Interrupt timeout routine.
 */
static void ft_timeout(caddr_t arg1, int arg2)
{
  int s;
  ftu_t ftu = (ftu_t)arg1;
  ft_p	ft = &ft_data[ftu];

  s = splbio();
  if (ft->active) {
	if (async_func != ACMD_NONE) {
		async_cmd(ftu);
		splx(s);
		return;
	}
	async_req(ftu, 1);
  } else {
	  ft->sts_wait = FTSTS_TIMEOUT;
	  wakeup((caddr_t)&ftsem.intr_wait);
  }
  splx(s);
}

/*
 *  Wait for a particular interrupt to occur.  ftintr() will wake us up
 *  if it sees what we want.  Otherwise, time out and return error.
 *  Should always disable ints before trigger is sent and calling here.
 */
int ftintr_wait(ftu_t ftu, int cmd, int ticks)
{
  int retries, st0, pcn;
  ft_p	ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */

  ft->cmd_wait = cmd;
  ft->sts_wait = FTSTS_SNOOZE;

  /* At attach time, we can't rely on having interrupts serviced */
  if (ft->attaching) {
	switch (cmd) {
	    case FTCMD_RESET:
		DELAY(100);
		ft->sts_wait = FTSTS_INTERRUPT;
		goto intrdone;
	    case FTCMD_RECAL:
	    case FTCMD_SEEK:
		for (retries = 0; retries < 10000; retries++) {
			out_fdc(fdcu, NE7CMD_SENSEI);
			st0 = in_fdc(fdcu);
			pcn = in_fdc(fdcu);
			if (st0 & 0x20) {
				ft->sts_wait = FTSTS_INTERRUPT;
				ft->pcn = pcn;
				goto intrdone;
			}
			DELAY(100);
		}
		break;
	}
	ft->sts_wait = FTSTS_TIMEOUT;
	goto intrdone;
  }

  if (ticks) timeout(ft_timeout, (caddr_t)ftu, ticks);
  tsleep((caddr_t)&ftsem.intr_wait, FTPRI, "ftwait", 0);

intrdone:
  if (ft->sts_wait == FTSTS_TIMEOUT) {	/* timeout */
	if (ft->cmd_wait != FTCMD_RESET)
		DPRT(("ft%d: timeout on command %d\n", ftu, ft->cmd_wait));
	ft->cmd_wait = FTCMD_NONE;
	ft->sts_wait = FTSTS_NONE;
	return(1);
  }

  /* got interrupt */
  if (ft->attaching == 0 && ticks) untimeout(ft_timeout, (caddr_t)ftu);
  ft->cmd_wait = FTCMD_NONE;
  ft->sts_wait = FTSTS_NONE;
  return(0);
}

/*
 *  Recalibrate tape drive.  Parameter totape is true, if we should
 *  recalibrate to tape drive settings.
 */
int tape_recal(ftu_t ftu, int totape)
{
  int s;
  ft_p	ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */

  DPRT(("tape_recal start\n"));

  out_fdc(fdcu, NE7CMD_SPECIFY);
  out_fdc(fdcu, (totape) ? 0xAD : 0xDF);
  out_fdc(fdcu, 0x02);

  s = splbio();
  out_fdc(fdcu, NE7CMD_RECAL);
  out_fdc(fdcu, 0x00);

  if (ftintr_wait(ftu, FTCMD_RECAL, hz)) {
	splx(s);
	DPRT(("ft%d: recalibrate timeout\n", ftu));
	return(1);
  }
  splx(s);

  out_fdc(fdcu, NE7CMD_SPECIFY);
  out_fdc(fdcu, (totape) ? 0xFD : 0xDF);
  out_fdc(fdcu, 0x02);

  DPRT(("tape_recal end\n"));
  return(0);
}

static void state_timeout(caddr_t arg1, int arg2)
{
  ftu_t ftu = (ftu_t)arg1;

  wakeup((caddr_t)&ftsem.long_delay);
}

/*
 *  Wait for a particular tape status to be met.  If all is TRUE, then
 *  all states must be met, otherwise any state can be met.
 */
int tape_state(ftu_t ftu, int all, int mask, int seconds)
{
  int r, tries, maxtries;

  maxtries = (seconds) ? (4 * seconds) : 1;
  for (tries = 0; tries < maxtries; tries++) {
	r = tape_status(ftu);
	if (r >= 0) {
		if (all && (r & mask) == mask) return(r);
		if ((r & mask) != 0) return(r);
	}
	if (seconds) {
		timeout(state_timeout, (caddr_t)ftu, hz/4);
		tsleep((caddr_t)&ftsem.long_delay, FTPRI, "ftstate", 0);
	}
  }
  DPRT(("ft%d: tape_state failed on mask=$%02x maxtries=%d\n",
				ftu, mask, maxtries));
  return(-1);
}

/*
 *  Send a QIC command to tape drive, wait for completion.
 */
int tape_cmd(ftu_t ftu, int cmd)
{
  int newcn;
  int retries = 0;
  int s;
  ft_p	ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */

  DPRT(("===> tape_cmd: %d\n",cmd));
  newcn = (cmd <= ft->pcn) ? ft->pcn - cmd : ft->pcn + cmd;

retry:

  /* Perform seek */
  s = splbio();
  out_fdc(fdcu, NE7CMD_SEEK);
  out_fdc(fdcu, 0x00);
  out_fdc(fdcu, newcn);

  if (ftintr_wait(ftu, FTCMD_SEEK, hz)) {
	DPRT(("ft%d: tape_cmd seek timeout\n", ftu));
redo:
	splx(s);
	if (++retries < 5) goto retry;
	DPRT(("ft%d: tape_cmd seek failed!\n", ftu));
	return(1);
  }
  splx(s);

  if (ft->pcn != newcn) {
	DPRT(("ft%d: bad seek in tape_cmd; pcn = %d  newcn = %d\n",
			ftu, ft->pcn, newcn));
	goto redo;
  }
  DELAY(2500);
  return(0);
}

/*
 *  Return status of tape drive
 */
int tape_status(ftu_t ftu)
{
  int r, err, tries;
	ft_p	ft = &ft_data[ftu];

  for (r = -1, tries = 0; r < 0 && tries < 3; tries++)
	r = qic_status(ftu, QC_STATUS, 8);
  if (tries == 3) return(-1);
  DPRT(("tape_status got $%04x\n",r));
  ft->laststs = r;

  if (r & (QS_ERROR|QS_NEWCART)) {
	if (r & QS_NEWCART) ft->newcart = 1;
	err = qic_status(ftu, QC_ERRCODE, 16);
	ft->lasterr = err;
	if ((r & QS_NEWCART) == 0 && err && ft->attaching == 0) {
		DPRT(("ft%d: QIC error %d occurred on cmd %d\n",
					ftu, err & 0xff, err >> 8));
	}
	r = qic_status(ftu, QC_STATUS, 8);
	ft->laststs = r;
	DPRT(("tape_status got error code $%04x new sts = $%02x\n",err,r));
  }
  ft->rdonly = (r & QS_RDONLY);
  return(r);
}

/*
 *  Transfer control to tape drive.
 */
void tape_start(ftu_t ftu)
{
  ft_p	ft = &ft_data[ftu];
  fdc_p	fdc = ft->fdc;
  int s;

  DPRT(("tape_start start\n"));

  s = splbio();

  /* reset, dma disable */
  outb(fdc->baseport+fdout, 0x00);
  (void)ftintr_wait(ftu, FTCMD_RESET, hz/10);

  /* raise reset, enable DMA */
  outb(fdc->baseport+fdout, FDO_FRST | FDO_FDMAEN);
  (void)ftintr_wait(ftu, FTCMD_RESET, hz/10);

  splx(s);

  tape_recal(ftu, 1);

  /* set transfer speed */
  outb(fdc->baseport+fdctl, FDC_500KBPS);
  DELAY(10);

  DPRT(("tape_start end\n"));
}

/*
 *  Transfer control back to floppy disks.
 */
void tape_end(ftu_t ftu)
{
  ft_p	ft = &ft_data[ftu];
  fdc_p	fdc = ft->fdc;
  int s;

  DPRT(("tape_end start\n"));
  tape_recal(ftu, 0);

  s = splbio();

  /* reset, dma disable */
  outb(fdc->baseport+fdout, 0x00);
  (void)ftintr_wait(ftu, FTCMD_RESET, hz/10);

  /* raise reset, enable DMA */
  outb(fdc->baseport+fdout, FDO_FRST | FDO_FDMAEN);
  (void)ftintr_wait(ftu, FTCMD_RESET, hz/10);

  splx(s);

  /* set transfer speed */
  outb(fdc->baseport+fdctl, FDC_500KBPS);
  DELAY(10);
  fdc->flags &= ~FDC_TAPE_BUSY; 

  DPRT(("tape_end end\n"));
}

/*
 *  Wait for the driver to go inactive, cancel readahead if necessary.
 */
void tape_inactive(ftu_t ftu)
{
  ft_p	ft = &ft_data[ftu];

  if (ft->curseg->reqtype == FTIO_RDAHEAD) {
	ft->curseg->reqcan = 1;	/* XXX cancel rdahead */
	while (ft->active) 
	  tsleep((caddr_t)&ftsem.iosts_change, FTPRI, "ftinact", 0);
  }
  while (ft->active) 
    tsleep((caddr_t)&ftsem.iosts_change, FTPRI, "ftinact", 0);
}

/*
 *  Get the geometry of the tape currently in the drive.
 */
int ftgetgeom(ftu_t ftu)
{
  int r, i, tries;
  int cfg, qic80, ext;
  int sts, fmt, len;
  ft_p	ft = &ft_data[ftu];

  r = tape_status(ftu);

  /* XXX fix me when format mode is finished */
  if ((r & QS_CART) == 0 || (r & QS_FMTOK) == 0) {
	DPRT(("ftgetgeom: no cart or not formatted 0x%04x\n",r));
	ftg = NULL;
	ft->newcart = 1;
	return(0);
  }

  /* Report drive configuration */
  for (cfg = -1, tries = 0; cfg < 0 && tries < 3; tries++)
	cfg = qic_status(ftu, QC_CONFIG, 8);
  if (tries == 3) {
	DPRT(("ftgetgeom report config failed\n"));
	ftg = NULL;
	return(-1);
  }
  DPRT(("ftgetgeom report config got $%04x\n", cfg));
  ft->lastcfg = cfg;

  qic80 = cfg & QCF_QIC80;
  ext = cfg & QCF_EXTRA;

/*
 *  XXX - This doesn't seem to work on my Colorado Jumbo 250...
 *  if it works on your drive, I'd sure like to hear about it.
 */
#if 0
  /* Report drive status */
  for (sts = -1, tries = 0; sts < 0 && tries < 3; tries++)
	sts = qic_status(ftu, QC_TSTATUS, 8);
  if (tries == 3) {
	DPRT(("ftgetgeom report tape status failed\n"));
	ftg = NULL;
	return(-1);
  }
  DPRT(("ftgetgeom report tape status got $%04x\n", sts));
#else
  /*
   *  XXX - Forge a fake tape status based upon the returned
   *  configuration, since the above command or code is broken
   *  for my drive and probably other older drives.
   */
  sts = 0;
  sts = (qic80) ? QTS_QIC80 : QTS_QIC40;
  sts |= (ext) ? QTS_LEN2 : QTS_LEN1;
#endif

  fmt = sts & QTS_FMMASK;
  len = (sts & QTS_LNMASK) >> 4;

  if (fmt > QCV_NFMT) {
	ftg = NULL;
	printf("ft%d: unsupported tape format\n", ftu);
	return(-1);
  }
  if (len > QCV_NLEN) {
	ftg = NULL;
	printf("ft%d: unsupported tape length\n", ftu);
	return(-1);
  }

  /* Look up geometry in the table */
  for (i = 1; i < NGEOM; i++)
	if (ftgtbl[i].g_fmtno == fmt && ftgtbl[i].g_lenno == len) break;
  if (i == NGEOM) {
	printf("ft%d: unknown tape geometry\n", ftu);
	ftg = NULL;
	return(-1);
  }
  ftg = &ftgtbl[i];
  if (!ftg->g_trktape) {
	printf("ft%d: unsupported format %s w/len %s\n",
				ftu, ftg->g_fmtdesc, ftg->g_lendesc);
	ftg = NULL;
	return(-1);
  }
  DPRT(("Tape format is %s, length is %s\n", ftg->g_fmtdesc, ftg->g_lendesc));
  ft->newcart = 0;
  return(0);
}

/*
 *  Switch between tape/floppy.  This will send the tape enable/disable
 *  codes for this drive's manufacturer.
 */
int set_fdcmode(dev_t dev, int newmode)
{
  ftu_t ftu = FDUNIT(minor(dev));
  ft_p	ft = &ft_data[ftu];
  fdc_p	fdc = ft->fdc;

  static int havebufs = 0;
  void *buf;
  int r, s, i;
  SegReq *sp;

  if (newmode == FDC_TAPE_MODE) {
	/* Wake up the tape drive */
	switch (ft->type) {
	    case NO_TYPE:
		fdc->flags &= ~FDC_TAPE_BUSY; 
		return(ENXIO);
	    case FT_COLORADO:
		tape_start(ftu);
		if (tape_cmd(ftu, QC_COL_ENABLE1)) {
			tape_end(ftu);
			return(EIO);
		}
		if (tape_cmd(ftu, QC_COL_ENABLE2)) {
			tape_end(ftu);
			return(EIO);
		}
		break;
	    case FT_MOUNTAIN:
		tape_start(ftu);
		if (tape_cmd(ftu, QC_MTN_ENABLE1)) {
			tape_end(ftu);
			return(EIO);
		}
		if (tape_cmd(ftu, QC_MTN_ENABLE2)) {
			tape_end(ftu);
			return(EIO);
		}
		break;
	    default:
		DPRT(("ft%d: bad tape type\n", ftu));
		return(ENXIO);
	}
	if (tape_status(ftu) < 0) {
		tape_cmd(ftu, (ft->type == FT_COLORADO) ? QC_COL_DISABLE : QC_MTN_DISABLE);
		tape_end(ftu);
		return(EIO);
	}

	/* Grab buffers from memory. */
	if (!havebufs) {
		ft->curseg = malloc(sizeof(SegReq), M_DEVBUF, M_NOWAIT);
		if (ft->curseg == NULL) {
			printf("ft%d: not enough memory for buffers\n", ftu);
			return(ENOMEM);
		}
		ft->bufseg = malloc(sizeof(SegReq), M_DEVBUF, M_NOWAIT);
		if (ft->bufseg == NULL) {
			free(ft->curseg, M_DEVBUF);
			printf("ft%d: not enough memory for buffers\n", ftu);
			return(ENOMEM);
		}
		havebufs = 1;
	}
	ft->curseg->reqtype = FTIO_READY;
	ft->bufseg->reqtype = FTIO_READY;
	ft->io_sts = FTIO_READY;	/* tape drive is ready */
	ft->active = 0;			/* interrupt driver not active */
	ft->moving = 0;			/* tape not moving */
	ft->rdonly = 0;			/* tape read only */
	ft->newcart = 0;		/* a new cart was inserted */
	ft->lastpos = -1;		/* tape is rewound */
	tape_state(ftu, 0, QS_READY, 60);
	tape_cmd(ftu, QC_RATE);
	tape_cmd(ftu, QCF_RT500+2);		/* 500K bps */
	tape_state(ftu, 0, QS_READY, 60);
	ft->mode = FTM_PRIMARY;
	tape_cmd(ftu, QC_PRIMARY);		/* Make sure we're in primary mode */
	tape_state(ftu, 0, QS_READY, 60);
	ftg = NULL;			/* No geometry yet */
	ftgetgeom(ftu);			/* Get tape geometry */
	ftreq_rewind(ftu);		/* Make sure tape is rewound */
  } else {
	tape_cmd(ftu, (ft->type == FT_COLORADO) ? QC_COL_DISABLE : QC_MTN_DISABLE);
	tape_end(ftu);
	ft->newcart = 0;		/* clear new cartridge */
	havebufs = 0;
	free(ft->curseg, M_DEVBUF);
	free(ft->bufseg, M_DEVBUF);
  }
  return(0);
}


/*
 *  Perform a QIC status function.
 */
int qic_status(ftu_t ftu, int cmd, int nbits)
{
  int st3, val, r, i;
  ft_p	ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */

  if (tape_cmd(ftu, cmd)) {
	DPRT(("ft%d: QIC status timeout\n", ftu));
	return(-1);
  }

  /* Sense drive status */
  out_fdc(fdcu, NE7CMD_SENSED);
  out_fdc(fdcu, 0x00);
  st3 = in_fdc(fdcu);

  if ((st3 & 0x10) == 0) {	/* track 0 */
	DPRT(("qic_status has dead drive...  st3 = $%02x\n", st3));
	return(-1);
  }

  for (i = r = 0; i <= nbits; i++) {
	if (tape_cmd(ftu, QC_NEXTBIT)) {
		DPRT(("ft%d: QIC status bit timed out on %d\n", ftu, i));
		return(-1);
	}

	out_fdc(fdcu, NE7CMD_SENSED);
	out_fdc(fdcu, 0x00);
	st3 = in_fdc(fdcu);
	if (st3 < 0) {
		DPRT(("ft%d: controller timed out on bit %d r=$%02x\n",
					ftu, i, r));
		return(-1);
	}

	r >>= 1;
	if (i < nbits)
		r |= ((st3 & 0x10) ? 1 : 0) << nbits;
	else if ((st3 & 0x10) == 0) {
		DPRT(("ft%d: qic status stop bit missing at %d, st3=$%02x r=$%04x\n",
			ftu,i,st3,r));
		return(-1);
	}
  }

  DPRT(("qic_status returned $%02x\n", r));
  return(r);
}

/*
 *  Open tape drive for use.  Bounced off of Fdopen if tape minor is
 *  detected.
 */
int ftopen(dev_t dev, int arg2) {
  ftu_t ftu = FDUNIT(minor(dev));
  int type = FDTYPE(minor(dev));
  fdc_p fdc;

  /* check bounds */
  if (ftu >= NFT) 
	return(ENXIO);
  fdc = ft_data[ftu].fdc;
  /* check for controller already busy with tape */
  if (fdc->flags & FDC_TAPE_BUSY)
	return(EBUSY); 
  /* make sure we found a tape when probed */
  if (!(fdc->flags & FDC_HASFTAPE))
	return(ENODEV);
  fdc->fdu = ftu;
  fdc->flags |= FDC_TAPE_BUSY; 
  return(set_fdcmode(dev, FDC_TAPE_MODE)); /* try to switch to tape */
}

/*
 *  Close tape and return floppy controller to disk mode.
 */
int ftclose(dev_t dev, int flags)
{
  int s;
  SegReq *sp;
  ftu_t ftu = FDUNIT(minor(dev));
  ft_p	ft = &ft_data[ftu];

  /* Wait for any remaining I/O activity to complete. */
  if (ft->curseg->reqtype == FTIO_RDAHEAD) ft->curseg->reqcan = 1;
  while (ft->active) 
    tsleep((caddr_t)&ftsem.iosts_change, FTPRI, "ftclose", 0);

  ft->mode = FTM_PRIMARY;
  tape_cmd(ftu, QC_PRIMARY);
  tape_state(ftu, 0, QS_READY, 60);
  ftreq_rewind(ftu);
  return(set_fdcmode(dev, FDC_DISK_MODE));	/* Otherwise, close tape */
}

/*
 *  Perform strategy on a given buffer (not!).  The driver was not
 *  performing very efficiently using the buffering routines.  After
 *  support for error correction was added, this routine became
 *  obsolete in favor of doing ioctl's.  Ugly, yes.
 */
void ftstrategy(struct buf *bp)
{
  return;
}

/* Read or write a segment. */
int ftreq_rw(ftu_t ftu, int cmd, QIC_Segment *sr, struct proc *p)
{
  int r, i, j;
  SegReq *sp;
  int s;
  long blk, bad;
  unsigned char *cp, *cp2;
  ft_p	ft = &ft_data[ftu];

  if (!ft->active) {
	r = tape_status(ftu);
	if ((r & QS_CART) == 0) {
		return(ENXIO);	/* No cartridge */
	}
	if ((r & QS_FMTOK) == 0) {
		return(ENXIO);	/* Not formatted */
	}
	tape_state(ftu, 0, QS_READY, 90);
  }

  if (ftg == NULL || ft->newcart) {
	while (ft->active) 
	  tsleep((caddr_t)&ftsem.iosts_change, FTPRI, "ftrw", 0);
	tape_state(ftu, 0, QS_READY, 90);
	if (ftgetgeom(ftu) < 0) {
		return(ENXIO);
	}
  }

  /* Write not allowed on a read-only tape. */
  if (cmd == QIOWRITE && ft->rdonly) {
	return(EROFS);
  }
  /* Quick check of request and buffer. */
  if (sr == NULL || sr->sg_data == NULL) {
	return(EINVAL);
  }
  if (sr->sg_trk >= ftg->g_trktape ||
	sr->sg_seg >= ftg->g_segtrk) {
	return(EINVAL);
  }
  blk = sr->sg_trk * ftg->g_blktrk + sr->sg_seg * QCV_BLKSEG;

  s = splbio();
  if (cmd == QIOREAD) {
	if (ft->curseg->reqtype == FTIO_RDAHEAD) {
		if (blk == ft->curseg->reqblk) {
			sp = ft->curseg;
			sp->reqtype = FTIO_READING;
			sp->reqbad = sr->sg_badmap;
			goto rdwait;
		} else
			ft->curseg->reqcan = 1;	/* XXX cancel rdahead */
	}

	/* Wait until we're ready. */
	while (ft->active) 
	  tsleep((caddr_t)&ftsem.iosts_change, FTPRI, "ftrw", 0);

	/* Set up a new read request. */
	sp = ft->curseg;
	sp->reqcrc = 0;
	sp->reqbad = sr->sg_badmap;
	sp->reqblk = blk;
	sp->reqcan = 0;
	sp->reqtype = FTIO_READING;

	/* Start the read request off. */
	DPRT(("Starting read I/O chain\n"));
	arq_state = ard_state = awr_state = 0;
	ft->xblk = sp->reqblk;
	ft->xcnt = 0;
	ft->xptr = sp->buff;
	ft->active = 1;
	timeout(ft_timeout, (caddr_t)ftu, 1);

rdwait:
	tsleep((caddr_t)&ftsem.buff_avail, FTPRI, "ftrw", 0);
	bad = sp->reqbad;
	sr->sg_crcmap = sp->reqcrc & ~bad;

	/* Copy out segment and discard bad mapped blocks. */
	cp = sp->buff; cp2 = sr->sg_data;
	for (i = 0; i < QCV_BLKSEG; cp += QCV_BLKSIZE, i++) {
		if (bad & (1 << i)) continue;
		copyout(cp, cp2, QCV_BLKSIZE);
		cp2 += QCV_BLKSIZE;
	}
  } else {
	if (ft->curseg->reqtype == FTIO_RDAHEAD) {
		ft->curseg->reqcan = 1;	/* XXX cancel rdahead */
		while (ft->active)
		  tsleep((caddr_t)&ftsem.iosts_change, FTPRI, "ftrw", 0);
	}

	/* Sleep until a buffer becomes available. */
	while (ft->bufseg->reqtype != FTIO_READY)
	  tsleep((caddr_t)&ftsem.buff_avail, FTPRI, "ftrwbuf", 0);
	sp = (ft->curseg->reqtype == FTIO_READY) ? ft->curseg : ft->bufseg;

	/* Copy in segment and expand bad blocks. */
	bad = sr->sg_badmap;
	cp = sr->sg_data; cp2 = sp->buff;
	for (i = 0; i < QCV_BLKSEG; cp2 += QCV_BLKSIZE, i++) {
		if (bad & (1 << i)) continue;
		copyin(cp, cp2, QCV_BLKSIZE);
		cp += QCV_BLKSIZE;
	}

	sp->reqblk = blk;
	sp->reqcan = 0;
	sp->reqtype = FTIO_WRITING;

	if (!ft->active) {
		DPRT(("Starting write I/O chain\n"));
		arq_state = ard_state = awr_state = 0;
		ft->xblk = sp->reqblk;
		ft->xcnt = 0;
		ft->xptr = sp->buff;
		ft->active = 1;
		timeout(ft_timeout, (caddr_t)ftu, 1);
	}
  }
  splx(s);
  return(0);
}


/* Rewind to beginning of tape */
int ftreq_rewind(ftu_t ftu)
{
  ft_p	ft = &ft_data[ftu];

  tape_inactive(ftu);
  tape_cmd(ftu, QC_STOP);
  tape_state(ftu, 0, QS_READY, 90);
  tape_cmd(ftu, QC_SEEKSTART);
  tape_state(ftu, 0, QS_READY, 90);
  tape_cmd(ftu, QC_SEEKTRACK);
  tape_cmd(ftu, 2);
  tape_state(ftu, 0, QS_READY, 90);
  ft->lastpos = -1;
  ft->moving = 0;
  return(0);
}

/* Move to logical beginning or end of track */
int ftreq_trkpos(ftu_t ftu, int req)
{
  int curtrk, r, cmd;
  ft_p	ft = &ft_data[ftu];

  tape_inactive(ftu);
  tape_cmd(ftu, QC_STOP);
  tape_state(ftu, 0, QS_READY, 90);

  r = tape_status(ftu);
  if ((r & QS_CART) == 0) return(ENXIO);	/* No cartridge */
  if ((r & QS_FMTOK) == 0) return(ENXIO);	/* Not formatted */

  if (ftg == NULL || ft->newcart) {
	if (ftgetgeom(ftu) < 0) return(ENXIO);
  }

  curtrk = (ft->lastpos < 0) ? 0 : ft->lastpos / ftg->g_blktrk;
  if (req == QIOBOT)
	cmd = (curtrk & 1) ? QC_SEEKEND : QC_SEEKSTART;
  else
	cmd = (curtrk & 1) ? QC_SEEKSTART : QC_SEEKEND;
  tape_cmd(ftu, cmd);
  tape_state(ftu, 0, QS_READY, 90);
  return(0);
}

/* Seek tape head to a particular track. */
int ftreq_trkset(ftu_t ftu, int *trk)
{
  int curtrk, r, cmd;
  ft_p	ft = &ft_data[ftu];

  tape_inactive(ftu);
  tape_cmd(ftu, QC_STOP);
  tape_state(ftu, 0, QS_READY, 90);

  r = tape_status(ftu);
  if ((r & QS_CART) == 0) return(ENXIO);	/* No cartridge */
  if ((r & QS_FMTOK) == 0) return(ENXIO);	/* Not formatted */
  if (ftg == NULL || ft->newcart) {
	if (ftgetgeom(ftu) < 0) return(ENXIO);
  }

  tape_cmd(ftu, QC_SEEKTRACK);
  tape_cmd(ftu, *trk + 2);
  tape_state(ftu, 0, QS_READY, 90);
  return(0);
}

/* Start tape moving forward. */
int ftreq_lfwd(ftu_t ftu)
{
  tape_inactive(ftu);
  tape_cmd(ftu, QC_STOP);
  tape_state(ftu, 0, QS_READY, 90);
  tape_cmd(ftu, QC_FORWARD);
  return(0);
}

/* Stop the tape */
int ftreq_stop(ftu_t ftu)
{
  tape_inactive(ftu);
  tape_cmd(ftu, QC_STOP);
  tape_state(ftu, 0, QS_READY, 90);
  return(0);
}

/* Set the particular mode the drive should be in. */
int ftreq_setmode(ftu_t ftu, int cmd)
{
  int r;
  ft_p	ft = &ft_data[ftu];

  tape_inactive(ftu);
  r = tape_status(ftu);

  switch(cmd) {
     case QIOPRIMARY:
	ft->mode = FTM_PRIMARY;
	tape_cmd(ftu, QC_PRIMARY);
	break;
     case QIOFORMAT:
	if (r & QS_RDONLY) return(ENXIO);
	if ((r & QS_BOT) == 0) return(ENXIO);
	tape_cmd(ftu, QC_FORMAT);
	break;
     case QIOVERIFY:
	if ((r & QS_FMTOK) == 0) return(ENXIO);	/* Not formatted */
	tape_cmd(ftu, QC_VERIFY);
	break;
  }
  tape_state(ftu, 0, QS_READY, 60);
  return(0);
}

/* Return drive status bits */
int ftreq_status(ftu_t ftu, int cmd, int *sts, struct proc *p)
{
  ft_p	ft = &ft_data[ftu];

  if (ft->active)
	*sts = ft->laststs & ~QS_READY;
  else
	*sts = tape_status(ftu);
  return(0);
}

/* Return drive configuration bits */
int ftreq_config(ftu_t ftu, int cmd, int *cfg, struct proc *p)
{
  int r, tries;
  ft_p	ft = &ft_data[ftu];

  if (ft->active)
	r = ft->lastcfg;
  else {
	for (r = -1, tries = 0; r < 0 && tries < 3; tries++)
		r = qic_status(ftu, QC_CONFIG, 8);
	if (tries == 3) return(ENXIO);
  }
  *cfg = r;
  return(0);
}

/* Return current tape's geometry. */
int ftreq_geom(ftu_t ftu, QIC_Geom *g)
{
  tape_inactive(ftu);
  if (ftg == NULL && ftgetgeom(ftu) < 0) return(ENXIO);
  bcopy(ftg, g, sizeof(QIC_Geom));
  return(0);
}

/* Return drive hardware information */
int ftreq_hwinfo(ftu_t ftu, QIC_HWInfo *hwp)
{
  int r, tries;
  int rom, vend;

  tape_inactive(ftu);
  bzero(hwp, sizeof(QIC_HWInfo));

  for (rom = -1, tries = 0; rom < 0 && tries < 3; tries++)
	rom = qic_status(ftu, QC_VERSION, 8);
  if (rom > 0) {
	hwp->hw_rombeta = (rom >> 7) & 0x01;
	hwp->hw_romid = rom & 0x7f;
  }

  for (vend = -1, tries = 0; vend < 0 && tries < 3; tries++)
	vend = qic_status(ftu, QC_VENDORID, 16);
  if (vend > 0) {
	hwp->hw_make = (vend >> 6) & 0x3ff;
	hwp->hw_model = vend & 0x3f;
  }

  return(0);
}

/*
 *  I/O functions.
 */
int ftioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
  ftu_t ftu = FDUNIT(minor(dev));
  ft_p	ft = &ft_data[ftu];

  switch(cmd) {
     case QIOREAD:	/* Request reading a segment from tape.		*/
     case QIOWRITE:	/* Request writing a segment to tape.		*/
	return(ftreq_rw(ftu, cmd, (QIC_Segment *)data, p));

     case QIOREWIND:	/* Rewind tape.					*/
	return(ftreq_rewind(ftu));

     case QIOBOT:	/* Seek to logical beginning of track.		*/
     case QIOEOT:	/* Seek to logical end of track.		*/
	return(ftreq_trkpos(ftu, cmd));

     case QIOTRACK:	/* Seek tape head to specified track.		*/
	return(ftreq_trkset(ftu, (int *)data));

     case QIOSEEKLP:	/* Seek load point.				*/
	goto badreq;

     case QIOFORWARD:	/* Move tape in logical forward direction.	*/
	return(ftreq_lfwd(ftu));

     case QIOSTOP:	/* Causes tape to stop.				*/
	return(ftreq_stop(ftu));

     case QIOPRIMARY:	/* Enter primary mode.				*/
     case QIOFORMAT:	/* Enter format mode.				*/
     case QIOVERIFY:	/* Enter verify mode.				*/
	return(ftreq_setmode(ftu, cmd));

     case QIOWRREF:	/* Write reference burst.			*/
	goto badreq;

     case QIOSTATUS:	/* Get drive status.				*/
	return(ftreq_status(ftu, cmd, (int *)data, p));

     case QIOCONFIG:	/* Get tape configuration.			*/
	return(ftreq_config(ftu, cmd, (int *)data, p));

     case QIOGEOM:
	return(ftreq_geom(ftu, (QIC_Geom *)data));

     case QIOHWINFO:
	return(ftreq_hwinfo(ftu, (QIC_HWInfo *)data));
  }
badreq:
  DPRT(("ft%d: unknown ioctl(%d) request\n", ftu, cmd));
  return(ENXIO);
}

/* Not implemented */
int ftdump(dev_t dev)
{
  return(EINVAL);
}

/* Not implemented */
int ftsize(dev_t dev)
{
  return(EINVAL);
}
#endif
