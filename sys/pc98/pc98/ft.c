/*
 *  Copyright (c) 1993, 1994 Steve Gerakines
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
 *  $Id: ft.c,v 1.4 1996/09/03 10:23:26 asami Exp $
 *
 *  01/19/95 ++sg
 *  Cleaned up recalibrate/seek code at attach time for FreeBSD 2.x.
 *
 *  06/07/94 v0.9 ++sg
 *  Tape stuck on segment problem should be gone.  Re-wrote buffering
 *  scheme.  Added support for drives that do not automatically perform
 *  seek load point.  Can handle more wakeup types now and should correctly
 *  report most manufacturer names.  Fixed places where unit 0 was being
 *  sent to the fdc instead of the actual unit number.  Added ioctl support
 *  for an in-core badmap.
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
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/disklabel.h>	/* temp. for dkunit() in fdc.h */
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/ftape.h>

#include <machine/clock.h>

#include <i386/isa/isa_device.h>
#ifdef PC98
#include <pc98/pc98/fdreg.h>
#include <pc98/pc98/fdc.h>
#else
#include <i386/isa/fdreg.h>
#include <i386/isa/fdc.h>
#include <i386/isa/rtc.h>
#endif
#include <i386/isa/ftreg.h>

extern int ftintr __P((ftu_t ftu));

/* Enable or disable debugging messages. */
#define FTDBGALL 0			/* 1 if you want everything */
/*#define DPRT(a) printf a		*/
#define DPRT(a)

/* Constants private to the driver */
#define FTPRI		(PRIBIO)	/* sleep priority */
#define	FTNBUFF		9		/* 8 for buffering, 1 for header */

/* The following items are needed from the fd driver. */
extern int in_fdc(int);			/* read fdc registers */
extern int out_fdc(int, int);		/* write fdc registers */

extern int hz;				/* system clock rate */

/* Flags in isadev struct */
#define	FT_PROBE	0x1		/* allow for "dangerous" tape probes */

/* Type of tape attached */
/* use numbers that don't interfere with the possible floppy types */
#define NO_TYPE 0		/* (same as NO_TYPE in fd.c) */

/* F_TAPE_TYPE must match value in fd.c */
#define F_TAPE_TYPE	0x020		/* bit for ft->types to indicate tape */
#define FT_NONE		(F_TAPE_TYPE | 0)	/* no method required */
#define	FT_MOUNTAIN	(F_TAPE_TYPE | 1)	/* mountain */
#define	FT_COLORADO	(F_TAPE_TYPE | 2)	/* colorado */
#define FT_INSIGHT	(F_TAPE_TYPE | 3)	/* insight */

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
static QIC_Geom ftgtbl[] = {
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

static QIC_Geom *ftg = NULL;			/* Current tape's geometry */

/*
 *  things relating to asynchronous commands
 */
static int awr_state;		/* state of async write */
static int ard_state;		/* state of async read */
static int arq_state;		/* state of async request */
static int async_retries;	/* retries, one per invocation */
static int async_func;		/* function to perform */
static int async_state;		/* state current function is at */
static int async_arg0;		/* up to 3 arguments for async cmds */
static int async_arg1;		/**/
static int async_arg2;		/**/
static int async_ret;		/* return value */
static struct _astk {
	int over_func;
	int over_state;
	int over_retries;
	int over_arg0;
	int over_arg1;
	int over_arg2;
} astk[10];
static struct _astk *astk_ptr = &astk[0]; /* Pointer to stack position */

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
#define CALL_ACMD(r,f,a,b,c) \
			astk_ptr->over_retries = async_retries; \
			astk_ptr->over_func = async_func; \
			astk_ptr->over_state = (r); \
			astk_ptr->over_arg0 = async_arg0; \
			astk_ptr->over_arg1 = async_arg1; \
			astk_ptr->over_arg2 = async_arg2; \
			async_func = (f); async_state = 0; async_retries = 0; \
			async_arg0=(a); async_arg1=(b); async_arg2=(c); \
			astk_ptr++; \
			goto restate

/* Perform an asyncronous command from outside async_cmd(). */
#define ACMD_FUNC(r,f,a,b,c) over_async = (r); astk_ptr = &astk[0]; \
			async_func = (f); async_state = 0; async_retries = 0; \
			async_arg0=(a); async_arg1=(b); async_arg2=(c); \
			async_cmd(ftu); \
			return

/* Various wait channels */
static char *wc_buff_avail	= "bavail";
static char *wc_buff_done	= "bdone";
static char *wc_iosts_change	= "iochg";
static char *wc_long_delay	= "ldelay";
static char *wc_intr_wait	= "intrw";
#define ftsleep(wc,to)	tsleep((caddr_t)(wc),FTPRI,(wc),(to))

/***********************************************************************\
* Per controller structure.						*
\***********************************************************************/
extern struct fdc_data fdc_data[NFDC];

/***********************************************************************\
* Per tape drive structure.						*
\***********************************************************************/
static struct ft_data {
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
	int	xseg;		/* segment being transferred	  */
	SegReq *segh;		/* Current I/O request		  */
	SegReq *segt;		/* Tail of queued I/O requests	  */
	SegReq *doneh;		/* Completed I/O request queue    */
	SegReq *donet;		/* Completed I/O request tail	  */
	SegReq *segfree;	/* Free segments		  */
	SegReq *hdr;		/* Current tape header		  */
	int nsegq;		/* Segments on request queue	  */
	int ndoneq;		/* Segments on completed queue	  */
	int nfreelist;		/* Segments on free list	  */

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



#define	id_physid id_scsiid	/* this biotab field doubles as a field */
				/* for the physical unit number on the controller */

int ftopen(dev_t, int);
int ftclose(dev_t, int);
int ftioctl(dev_t, int, caddr_t, int, struct proc *);
int ftattach(struct isa_device *, struct isa_device *, int);
static timeout_t ft_timeout;
static void async_cmd(ftu_t);
static void async_req(ftu_t, int);
static void async_read(ftu_t, int);
static void async_write(ftu_t, int);
static void tape_start(ftu_t, int);
static void tape_end(ftu_t);
static void tape_inactive(ftu_t);
static int tape_cmd(ftu_t, int);
static int tape_status(ftu_t);
static int qic_status(ftu_t, int, int);
static int ftreq_rewind(ftu_t);
static int ftreq_hwinfo(ftu_t, QIC_HWInfo *);

/*****************************************************************************/


/*
 *  Allocate a segment I/O buffer from the free list.
 */
static SegReq *
segio_alloc(ft_p ft)
{
  SegReq *r;

  /* Grab first item from free list */
  if ((r = ft->segfree) != NULL) {
	ft->segfree = ft->segfree->next;
	ft->nfreelist--;
  }
  DPRT(("segio_alloc: nfree=%d ndone=%d nreq=%d\n", ft->nfreelist, ft->ndoneq, ft->nsegq));
  return(r);
}


/*
 *  Queue a segment I/O request.
 */
static void
segio_queue(ft_p ft, SegReq *sp)
{
  /* Put request on in process queue. */
  if (ft->segt == NULL)
	ft->segh = sp;
  else
	ft->segt->next = sp;
  sp->next = NULL;
  ft->segt = sp;
  ft->nsegq++;
  DPRT(("segio_queue: nfree=%d ndone=%d nreq=%d\n", ft->nfreelist, ft->ndoneq, ft->nsegq));
}


/*
 *  Segment I/O completed, place on correct queue.
 */
static void
segio_done(ft_p ft, SegReq *sp)
{
  /* First remove from current I/O queue */
  ft->segh = sp->next;
  if (ft->segh == NULL) ft->segt = NULL;
  ft->nsegq--;

  if (sp->reqtype == FTIO_WRITING) {
	/* Place on free list */
	sp->next = ft->segfree;
	ft->segfree = sp;
	ft->nfreelist++;
	wakeup((caddr_t)wc_buff_avail);
	DPRT(("segio_done: (w) nfree=%d ndone=%d nreq=%d\n", ft->nfreelist, ft->ndoneq, ft->nsegq));
  } else {
	/* Put on completed I/O queue */
	if (ft->donet == NULL)
		ft->doneh = sp;
	else
		ft->donet->next = sp;
	sp->next = NULL;
	ft->donet = sp;
	ft->ndoneq++;
	wakeup((caddr_t)wc_buff_done);
	DPRT(("segio_done: (r) nfree=%d ndone=%d nreq=%d\n", ft->nfreelist, ft->ndoneq, ft->nsegq));
  }
}


/*
 *  Take I/O request from finished queue to free queue.
 */
static void
segio_free(ft_p ft, SegReq *sp)
{
  /* First remove from done queue */
  ft->doneh = sp->next;
  if (ft->doneh == NULL) ft->donet = NULL;
  ft->ndoneq--;

  /* Place on free list */
  sp->next = ft->segfree;
  ft->segfree = sp;
  ft->nfreelist++;
  wakeup((caddr_t)wc_buff_avail);
  DPRT(("segio_free: nfree=%d ndone=%d nreq=%d\n", ft->nfreelist, ft->ndoneq, ft->nsegq));
}

/*
 *  Probe/attach floppy tapes.
 */
int
ftattach(isadev, fdup, unithasfd)
	struct isa_device *isadev, *fdup;
	int unithasfd;
{
  fdcu_t fdcu = isadev->id_unit;		/* fdc active unit */
  fdc_p fdc = fdc_data + fdcu;	/* pointer to controller structure */
  ftu_t ftu = fdup->id_unit;
  ft_p ft;
  ftsu_t ftsu = fdup->id_physid;
  QIC_HWInfo hw;
  char *manu;

  if (ftu >= NFT) return 0;
  ft = &ft_data[ftu];

  /* Probe for tape */
  ft->attaching = 1;
  ft->type = NO_TYPE;
  ft->fdc = fdc;
  ft->ftsu = ftsu;

  /*
   *  FT_NONE - no method, just do it
   */
  tape_start(ftu, 0);
  if (tape_status(ftu) >= 0) {
	ft->type = FT_NONE;
	ftreq_hwinfo(ftu, &hw);
	goto out;
  }

  /*
   *  FT_COLORADO - colorado style
   */
  tape_start(ftu, 0);
  tape_cmd(ftu, QC_COL_ENABLE1);
  tape_cmd(ftu, QC_COL_ENABLE2 + ftu);
  if (tape_status(ftu) >= 0) {
	ft->type = FT_COLORADO;
	ftreq_hwinfo(ftu, &hw);
	tape_cmd(ftu, QC_COL_DISABLE);
	goto out;
  }

  /*
   *  FT_MOUNTAIN - mountain style
   */
  tape_start(ftu, 0);
  tape_cmd(ftu, QC_MTN_ENABLE1);
  tape_cmd(ftu, QC_MTN_ENABLE2);
  if (tape_status(ftu) >= 0) {
	ft->type = FT_MOUNTAIN;
	ftreq_hwinfo(ftu, &hw);
	tape_cmd(ftu, QC_MTN_DISABLE);
	goto out;
  }

  if(isadev->id_flags & FT_PROBE) {
    /*
     * Insight probe is dangerous, since it requires the motor being
     * enabled and therefore risks attached floppy disk drives to jam.
     * Probe only if explicitly requested by a flag 0x1 from config
     */

    /*
     *  FT_INSIGHT - insight style
     *
     *  Since insight requires turning the drive motor on, we will not
     *  perform this probe if a floppy drive was already found with the
     *  the given unit and controller.
     */
    if (unithasfd) goto out;
    tape_start(ftu, 1);
    if (tape_status(ftu) >= 0) {
	ft->type = FT_INSIGHT;
	ftreq_hwinfo(ftu, &hw);
	goto out;
    }
  }

out:
  tape_end(ftu);
  if (ft->type != NO_TYPE) {
	fdc->flags |= FDC_HASFTAPE;
	switch(hw.hw_make) {
	    case 0x0000:
		if (ft->type == FT_COLORADO) {
			manu = "Colorado";
		} else if (ft->type == FT_INSIGHT) {
			manu = "Insight";
		} else if (ft->type == FT_MOUNTAIN && hw.hw_model == 0x05) {
			manu = "Archive";
		} else if (ft->type == FT_MOUNTAIN) {
			manu = "Mountain";
		} else {
			manu = "Unknown";
		}
		break;
	    case 0x0001:
		manu = "Colorado";
		break;
	    case 0x0005:
		if (hw.hw_model >= 0x09) {
			manu = "Conner";
		} else {
			manu = "Archive";
		}
		break;
	    case 0x0006:
		manu = "Mountain";
		break;
	    case 0x0007:
		manu = "Wangtek";
		break;
	    case 0x0222:
		manu = "IOMega";
		break;
	    default:
		manu = "Unknown";
		break;
	}
	printf("ft%d: %s tape\n", fdup->id_unit, manu);
  }
  ft->attaching = 0;
  return(ft->type);
}


/*
 *  Perform common commands asynchronously.
 */
static void
async_cmd(ftu_t ftu) {
	ft_p	ft = &ft_data[ftu];
	fdcu_t	fdcu = ft->fdc->fdcu;
	int cmd, i, st0, st3, pcn;
	static int bitn, retval, retpos, nbits, newcn;
	static int wanttrk, wantblk, wantdir;
	static int curtrk, curblk, curdir, curdiff;
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
		cmd = async_arg0;
#if FTDBGALL
		DPRT(("===>async_seek cmd = %d\n", cmd));
#endif
		newcn = (cmd <= ft->pcn) ? ft->pcn - cmd : ft->pcn + cmd;
		async_state = 1;
		i = 0;
		if (out_fdc(fdcu, NE7CMD_SEEK) < 0) i = 1;
#ifdef PC98
		if (!i && out_fdc(fdcu, 3) < 0) i = 1;
#else
		if (!i && out_fdc(fdcu, ftu) < 0) i = 1;
#endif
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
		if (async_arg1) goto complete;
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
		cmd = async_arg0;
		nbits = async_arg1;
		DPRT(("async_status got cmd = %d nbits = %d\n", cmd,nbits));
		CALL_ACMD(5, ACMD_SEEK, QC_NEXTBIT, 0, 0);
		/* NOTREACHED */
	    case 1:
		out_fdc(fdcu, NE7CMD_SENSED);
#ifdef PC98
		out_fdc(fdcu, 3);
#else
		out_fdc(fdcu, ftu);
#endif
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
				if (async_arg0 == QC_STATUS && async_arg2 == 0 &&
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
		CALL_ACMD(1, ACMD_SEEK, QC_NEXTBIT, 0, 0);
		/* NOTREACHED */
	    case 2:
		if (async_ret & QS_NEWCART) ft->newcart = 1;
		CALL_ACMD(3, ACMD_STATUS, QC_ERRCODE, 16, 1);
	    case 3:
		ft->lasterr = async_ret;
		if ((ft->lasterr & QS_NEWCART) == 0 && ft->lasterr) {
			DPRT(("ft%d: QIC error %d occurred on cmd %d\n",
				ftu, ft->lasterr & 0xff, ft->lasterr >> 8));
		}
	        cmd = async_arg0;
		nbits = async_arg1;
		CALL_ACMD(4, ACMD_STATUS, QC_STATUS, 8, 1);
	    case 4:
		goto complete;
	    case 5:
		CALL_ACMD(6, ACMD_SEEK, QC_NEXTBIT, 0, 0);
	    case 6:
		CALL_ACMD(7, ACMD_SEEK, QC_NEXTBIT, 0, 0);
	    case 7:
		CALL_ACMD(8, ACMD_SEEK, QC_NEXTBIT, 0, 0);
	    case 8:
		cmd = async_arg0;
		CALL_ACMD(1, ACMD_SEEK, cmd, 0, 0);
	}
	break;

     case ACMD_STATE:
	/*
	 *  Arguments:
	 *     0 - status bits to check
	 */
	switch(async_state) {
	    case 0:
		CALL_ACMD(1, ACMD_STATUS, QC_STATUS, 8, 0);
	    case 1:
		if ((async_ret & async_arg0) != 0) goto complete;
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
		cmd = async_arg0;
		async_retries = (async_arg2) ? (async_arg2 * 4) : 10;
		CALL_ACMD(1, ACMD_SEEK, cmd, 0, 0);
	    case 1:
		CALL_ACMD(2, ACMD_STATUS, QC_STATUS, 8, 0);
	    case 2:
		if ((async_ret & async_arg1) != 0) goto complete;
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
			CALL_ACMD(4, ACMD_SEEKSTS, QC_STOP, QS_READY, 0);
			/* NOTREACHED */
		}
		async_state = 1;
		out_fdc(fdcu, 0x4a);		/* READ_ID */
#ifdef PC98
		out_fdc(fdcu, 3);
#else
		out_fdc(fdcu, ftu);
#endif
		break;
	    case 1:
		for (i = 0; i < 7; i++) ft->rid[i] = in_fdc(fdcu);
		async_ret = (ft->rid[3]*ftg->g_fdtrk) +
			    (ft->rid[4]*ftg->g_fdside) + ft->rid[5] - 1;
		DPRT(("readid st0:%02x st1:%02x st2:%02x c:%d h:%d s:%d pos:%d\n",
			ft->rid[0], ft->rid[1], ft->rid[2], ft->rid[3],
			ft->rid[4], ft->rid[5], async_ret));
		if ((ft->rid[0] & 0xc0) != 0 || async_ret < 0) {
			/*
			 *  Method for retry:
			 *    errcnt == 1 regular retry
			 *		2 microstep head 1
			 * 		3 microstep head 2
			 *		4 microstep head back to 0
			 *		5 fail
			 */
			if (++errcnt >= 5) {
				DPRT(("ft%d: acmd_readid errcnt exceeded\n", fdcu));
				async_ret = -2;
				errcnt = 0;
				goto complete;
			}
			if (errcnt == 1) {
				ft->moving = 0;
				CALL_ACMD(4, ACMD_SEEKSTS, QC_STOP, QS_READY, 0);
			} else {
				ft->moving = 0;
				CALL_ACMD(4, ACMD_SEEKSTS, QC_STPAUSE, QS_READY, 0);
			}
			DPRT(("readid retry %d...\n", errcnt));
			async_state = 0;
			goto restate;
		}
		if ((async_ret % ftg->g_blktrk) == (ftg->g_blktrk-1)) {
			DPRT(("acmd_readid detected last block on track\n"));
			retpos = async_ret;
			CALL_ACMD(2, ACMD_STATE, QS_BOT|QS_EOT, 0, 0);
			/* NOTREACHED */
		}
		ft->lastpos = async_ret;
		errcnt = 0;
		goto complete;
		/* NOTREACHED */
	    case 2:
		CALL_ACMD(3, ACMD_STATE, QS_READY, 0, 0);
	    case 3:
		ft->moving = 0;
		async_ret = retpos+1;
		goto complete;
	    case 4:
		CALL_ACMD(5, ACMD_SEEK, QC_FORWARD, 0, 0);
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
		wanttrk = async_arg0 / ftg->g_blktrk;
		wantblk = async_arg0 % ftg->g_blktrk;
		wantdir = wanttrk & 1;
		ft->moving = 0;
		CALL_ACMD(1, ACMD_SEEKSTS, QC_STOP, QS_READY, 0);
	    case 1:
		curtrk = wanttrk;
		curdir = curtrk & 1;
		DPRT(("Changing to track %d\n", wanttrk));
		CALL_ACMD(2, ACMD_SEEK, QC_SEEKTRACK, 0, 0);
	    case 2:
		cmd = wanttrk+2;
		CALL_ACMD(3, ACMD_SEEKSTS, cmd, QS_READY, 0);
	    case 3:
		CALL_ACMD(4, ACMD_STATUS, QC_STATUS, 8, 0);
	    case 4:
		ft->laststs = async_ret;
		if (wantblk == 0) {
			curblk = 0;
			cmd = (wantdir) ? QC_SEEKEND : QC_SEEKSTART;
			CALL_ACMD(6, ACMD_SEEKSTS, cmd, QS_READY, 90);
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
		CALL_ACMD(5, ACMD_READID, 0, 0, 0);
	    case 5:
		if (async_ret < 0) {
			ft->moving = 0;
			ft->lastpos = -2;
			if (async_ret == -2) {
				CALL_ACMD(9, ACMD_SEEKSTS, QC_STOP, QS_READY, 0);
			}
			CALL_ACMD(1, ACMD_SEEKSTS, QC_STOP, QS_READY, 0);
		}
		curtrk = (async_ret+1) / ftg->g_blktrk;
		curblk = (async_ret+1) % ftg->g_blktrk;
		DPRT(("gotid: curtrk=%d wanttrk=%d curblk=%d wantblk=%d\n",
			curtrk, wanttrk, curblk, wantblk));
		if (curtrk != wanttrk) {	/* oops! */
			DPRT(("oops!! wrong track!\n"));
			CALL_ACMD(1, ACMD_SEEKSTS, QC_STOP, QS_READY, 0);
		}
		async_state = 6;
		goto restate;
	    case 6:
		DPRT(("curtrk = %d nextblk = %d\n", curtrk, curblk));
		if (curblk == wantblk) {
			ft->lastpos = curblk - 1;
			async_ret = ft->lastpos;
			if (ft->moving) goto complete;
			CALL_ACMD(7, ACMD_STATE, QS_READY, 0, 0);
		}
		if (curblk > wantblk) {		/* passed it */
			ft->moving = 0;
			CALL_ACMD(10, ACMD_SEEKSTS, QC_STOP, QS_READY, 0);
		}
		if ((wantblk - curblk) <= 256) {	/* approaching it */
			CALL_ACMD(5, ACMD_READID, 0, 0, 0);
		}
		/* way up ahead */
		ft->moving = 0;
		CALL_ACMD(14, ACMD_SEEKSTS, QC_STOP, QS_READY, 0);
		break;
	    case 7:
		ft->moving = 1;
		CALL_ACMD(8, ACMD_SEEK, QC_FORWARD, 0, 0);
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
		CALL_ACMD(11, ACMD_SEEK, QC_SEEKREV, 0, 0);
	    case 11:
		DPRT(("reverse 1 done\n"));
		CALL_ACMD(12, ACMD_SEEK, (curdiff & 0xf)+2, 0, 0);
	    case 12:
		DPRT(("reverse 2 done\n"));
		CALL_ACMD(13, ACMD_SEEKSTS, ((curdiff>>4)&0xf)+2, QS_READY, 90);
	    case 13:
		CALL_ACMD(5, ACMD_READID, 0, 0, 0);
	    case 14:
		curdiff = ((wantblk - curblk) / QCV_BLKSEG) - 2;
		if (curdiff < 0) curdiff = 0;
		DPRT(("pos %d before %d, forward %d\n", curblk, wantblk, curdiff));
		CALL_ACMD(15, ACMD_SEEK, QC_SEEKFWD, 0, 0);
	    case 15:
		DPRT(("forward 1 done\n"));
		CALL_ACMD(16, ACMD_SEEK, (curdiff & 0xf)+2, 0, 0);
	    case 16:
		DPRT(("forward 2 done\n"));
		CALL_ACMD(13, ACMD_SEEKSTS, ((curdiff>>4)&0xf)+2, QS_READY, 90);
	}
	break;
  }

  return;

complete:
  if (astk_ptr != &astk[0]) {
	astk_ptr--;
	async_retries = astk_ptr->over_retries;
	async_func = astk_ptr->over_func;
	async_state = astk_ptr->over_state;
	async_arg0 = astk_ptr->over_arg0;
	async_arg1 = astk_ptr->over_arg1;
	async_arg2 = astk_ptr->over_arg2;
	goto restate;
  }
  async_func = ACMD_NONE;
  async_state = 0;
  switch (ft->io_sts) {
     case FTIO_READY:
	async_req(ftu, 2);
	break;
     case FTIO_READING:
     case FTIO_RDAHEAD:
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
static void
async_req(ftu_t ftu, int from)
{
  ft_p	ft = &ft_data[ftu];
  SegReq *sp;
  static int over_async, lastreq;
  int cmd;

  if (from == 2) arq_state = over_async;

restate:
  switch (arq_state) {
     case 0:	/* Process segment */
	sp = ft->segh;
	ft->io_sts = (sp == NULL) ? FTIO_READY : sp->reqtype;

	if (ft->io_sts == FTIO_WRITING)
		async_write(ftu, from);
	else
		async_read(ftu, from);
	if (ft->io_sts != FTIO_READY) return;

	/* Pull buffer from current I/O queue */
	if (sp != NULL) {
		lastreq = sp->reqtype;
		segio_done(ft, sp);

		/* If I/O cancelled, clear finished queue. */
		if (sp->reqcan) {
			while (ft->doneh != NULL)
				segio_free(ft, ft->doneh);
			lastreq = FTIO_READY;
		}
	} else
		lastreq = FTIO_READY;

	/* Detect end of track */
	if (((ft->xblk / QCV_BLKSEG) % ftg->g_segtrk) == 0) {
		ACMD_FUNC(2, ACMD_STATE, QS_BOT|QS_EOT, 0, 0);
	}
	arq_state = 1;
	goto restate;

     case 1:	/* Next request */
	/* If we have another request queued, start it running. */
	if (ft->segh != NULL) {
		sp = ft->segh;
		sp->reqcrc = 0;
		arq_state = ard_state = awr_state = 0;
		ft->xblk = sp->reqblk;
		ft->xseg = sp->reqseg;
		ft->xcnt = 0;
		ft->xptr = sp->buff;
		DPRT(("I/O reqblk = %d\n", ft->xblk));
		goto restate;
	}

	/* If the last request was reading, do read ahead. */
	if ((lastreq == FTIO_READING || lastreq == FTIO_RDAHEAD) &&
					(sp = segio_alloc(ft)) != NULL) {
		sp->reqtype = FTIO_RDAHEAD;
		sp->reqblk = ft->xblk;
		sp->reqseg = ft->xseg+1;
		sp->reqcrc = 0;
		sp->reqcan = 0;
		segio_queue(ft, sp);
		bzero(sp->buff, QCV_SEGSIZE);
		arq_state = ard_state = awr_state = 0;
		ft->xblk = sp->reqblk;
		ft->xseg = sp->reqseg;
		ft->xcnt = 0;
		ft->xptr = sp->buff;
		DPRT(("Processing readahead reqblk = %d\n", ft->xblk));
		goto restate;
	}

	if (ft->moving) {
		DPRT(("No more I/O.. Stopping.\n"));
		ft->moving = 0;
		ACMD_FUNC(7, ACMD_SEEKSTS, QC_PAUSE, QS_READY, 0);
		break;
	}
	arq_state = 7;
	goto restate;

     case 2:	/* End of track */
	ft->moving = 0;
	ACMD_FUNC(3, ACMD_STATE, QS_READY, 0, 0);
	break;

     case 3:
	DPRT(("async_req seek head to track %d\n", ft->xblk / ftg->g_blktrk));
	ACMD_FUNC(4, ACMD_SEEK, QC_SEEKTRACK, 0, 0);
	break;

     case 4:
	cmd = (ft->xblk / ftg->g_blktrk) + 2;
	if (ft->segh != NULL) {
		ACMD_FUNC(5, ACMD_SEEKSTS, cmd, QS_READY, 0);
	} else {
		ACMD_FUNC(7, ACMD_SEEKSTS, cmd, QS_READY, 0);
	}
	break;

     case 5:
	ft->moving = 1;
	ACMD_FUNC(6, ACMD_SEEK, QC_FORWARD, 0, 0);
	break;

     case 6:
	arq_state = 1;
	timeout(ft_timeout, (caddr_t)ftu, hz/10); /* XXX */
	break;

     case 7:
	/* Time to rest. */
	ft->active = 0;
	ft->lastpos = -2;

	/* wakeup those who want an i/o chg */
	wakeup((caddr_t)wc_iosts_change);
	break;
  }
}


/*
 *  Entry for async read.
 */
static void
async_read(ftu_t ftu, int from)
{
  ft_p ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */
  int i, rddta[7];
  int where;
  static int over_async;
  static int retries = 0;

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
		ACMD_FUNC(1, ACMD_RUNBLK, ft->xblk, 0, 0);
	}

	/* Tape is in position but stopped. */
	if (!ft->moving) {
		DPRT(("async_read ******STARTING TAPE\n"));
		ACMD_FUNC(3, ACMD_STATE, QS_READY, 0, 0);
	}
	ard_state = 1;
	goto restate;

     case 1:	/* Start DMA */
	/* Tape is now moving and in position-- start DMA now! */
	isa_dmastart(B_READ, ft->xptr, QCV_BLKSIZE, 2);
	out_fdc(fdcu, 0x66);				/* read */
#ifdef PC98
	out_fdc(fdcu, 3);
#else
	out_fdc(fdcu, ftu);				/* unit */
#endif
	out_fdc(fdcu, (ft->xblk % ftg->g_fdside) / ftg->g_fdtrk); 	/* cylinder */
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
	where = (rddta[3]*ftg->g_fdtrk) + (rddta[4]*ftg->g_fdside)
			+ rddta[5]-1;
	DPRT(("xfer done: st0:%02x st1:%02x st2:%02x c:%d h:%d s:%d pos:%d want:%d\n",
	    rddta[0], rddta[1], rddta[2], rddta[3], rddta[4], rddta[5],
	    where, ft->xblk));
#endif

	/* Check for errors */
	if ((rddta[0] & 0xc0) != 0x00) {
#if !FTDBGALL
		where = (rddta[3]*ftg->g_fdtrk) + (rddta[4]*ftg->g_fdside)
			+ rddta[5]-1;
		DPRT(("xd: st0:%02x st1:%02x st2:%02x c:%d h:%d s:%d pos:%d want:%d\n",
		    rddta[0], rddta[1], rddta[2], rddta[3], rddta[4], rddta[5],
		    where, ft->xblk));
#endif
		if ((rddta[1] & 0x04) == 0x04 && retries < 2) {
			/* Probably wrong position */
			DPRT(("async_read: doing retry %d\n", retries));
			ft->lastpos = ft->xblk;
			ard_state = 0;
			retries++;
			goto restate;
		} else {
			/* CRC/Address-mark/Data-mark, et. al. */
			DPRT(("ft%d: CRC error on block %d\n", fdcu, ft->xblk));
			ft->segh->reqcrc |= (1 << ft->xcnt);
		}
	}

	/* Otherwise, transfer completed okay. */
	retries = 0;
	ft->lastpos = ft->xblk;
	ft->xblk++;
	ft->xcnt++;
	ft->xptr += QCV_BLKSIZE;
	if (ft->xcnt < QCV_BLKSEG && ft->segh->reqcan == 0) {
		ard_state = 0;
		goto restate;
	}
	DPRT(("Read done..  Cancel = %d\n", ft->segh->reqcan));
	ft->io_sts = FTIO_READY;
	break;

     case 3:
	ft->moving = 1;
	ACMD_FUNC(4, ACMD_SEEK, QC_FORWARD, 0, 0);
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
static void
async_write(ftu_t ftu, int from)
{
  ft_p ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */
  int i, rddta[7];
  int where;
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
		ACMD_FUNC(1, ACMD_RUNBLK, ft->xblk, 0, 0);
	}

	/* Tape is in position but stopped. */
	if (!ft->moving) {
		DPRT(("async_write ******STARTING TAPE\n"));
		ACMD_FUNC(3, ACMD_STATE, QS_READY, 0, 0);
	}
	awr_state = 1;
	goto restate;

     case 1:	/* Start DMA */
	/* Tape is now moving and in position-- start DMA now! */
	isa_dmastart(B_WRITE, ft->xptr, QCV_BLKSIZE, 2);
	out_fdc(fdcu, 0x45);				/* write */
#ifdef PC98
	out_fdc(fdcu, 3);
#else
	out_fdc(fdcu, ftu);				/* unit */
#endif
	out_fdc(fdcu, (ft->xblk % ftg->g_fdside) / ftg->g_fdtrk); /* cyl */
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
#if !FTDBGALL
		where = (rddta[3]*ftg->g_fdtrk) + (rddta[4]*ftg->g_fdside)
			 + rddta[5]-1;
		DPRT(("xfer done: st0:%02x st1:%02x st2:%02x c:%d h:%d s:%d pos:%d want:%d\n",
		    rddta[0], rddta[1], rddta[2], rddta[3], rddta[4], rddta[5],
		    where, ft->xblk));
#endif
		if (retries < 3) {
			/* Something happened -- try again */
			DPRT(("async_write: doing retry %d\n", retries));
			ft->lastpos = ft->xblk;
			awr_state = 0;
			retries++;
			goto restate;
		} else {
			/*
			 *  Retries failed.  Note the unrecoverable error.
			 *  Marking the block as bad is useless right now.
			 */
			printf("ft%d: unrecoverable write error on block %d\n",
					ftu, ft->xblk);
			ft->segh->reqcrc |= (1 << ft->xcnt);
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
	ACMD_FUNC(4, ACMD_SEEK, QC_FORWARD, 0, 0);
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
int
ftintr(ftu_t ftu)
{
  int st0, pcn, i;
  ft_p	ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */
  int s = splbio();

  st0 = 0;
  pcn = 0;

  /* I/O segment transfer completed */
  if (ft->active) {
	if (async_func != ACMD_NONE) {
		async_cmd(ftu);
		splx(s);
		return(1);
	}
#if FTDBGALL
	DPRT(("Got request interrupt\n"));
#endif
	async_req(ftu, 0);
	splx(s);
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
	splx(s);
	return(1);
  }

  switch (ft->cmd_wait) {
     case FTCMD_RESET:
	ft->sts_wait = FTSTS_INTERRUPT;
	wakeup((caddr_t)wc_intr_wait);
	break;
     case FTCMD_RECAL:
     case FTCMD_SEEK:
	if (st0 & 0x20)	{ 	/* seek done */
		ft->sts_wait = FTSTS_INTERRUPT;
		ft->pcn = pcn;
		wakeup((caddr_t)wc_intr_wait);
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
	wakeup((caddr_t)wc_intr_wait);
	break;

     default:
	goto huh_what;
  }

  splx(s);
  return(1);
}


/*
 *  Interrupt timeout routine.
 */
static void
ft_timeout(void *arg1)
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
	  wakeup((caddr_t)wc_intr_wait);
  }
  splx(s);
}


/*
 *  Wait for a particular interrupt to occur.  ftintr() will wake us up
 *  if it sees what we want.  Otherwise, time out and return error.
 *  Should always disable ints before trigger is sent and calling here.
 */
static int
ftintr_wait(ftu_t ftu, int cmd, int ticks)
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
			DELAY(150);
			out_fdc(fdcu, NE7CMD_SENSEI);
			st0 = in_fdc(fdcu);
			if ((st0 & 0xc0) == 0x80) continue;
			pcn = in_fdc(fdcu);
			if (st0 & 0x20) {
				ft->sts_wait = FTSTS_INTERRUPT;
				ft->pcn = pcn;
				goto intrdone;
			}
		}
		break;
	}
	ft->sts_wait = FTSTS_TIMEOUT;
	goto intrdone;
  }

  ftsleep(wc_intr_wait, ticks);

intrdone:
  if (ft->sts_wait == FTSTS_TIMEOUT) {	/* timeout */
#if FTDBGALL
	if (ft->cmd_wait != FTCMD_RESET)
		DPRT(("ft%d: timeout on command %d\n", ftu, ft->cmd_wait));
#endif
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
static int
tape_recal(ftu_t ftu, int totape)
{
  int s;
  ft_p	ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */

  DPRT(("tape_recal start\n"));

#ifdef PC98
  outb(0xbe, FDP_FDDEXC | FDP_PORTEXC);
#endif
  out_fdc(fdcu, NE7CMD_SPECIFY);
#ifdef PC98
  out_fdc(fdcu, (totape) ? 0xEF : 0xCF);
  out_fdc(fdcu, 0x02);
#else
  out_fdc(fdcu, (totape) ? 0xAD : 0xDF);
  out_fdc(fdcu, 0x02);
#endif

  s = splbio();
  out_fdc(fdcu, NE7CMD_RECAL);
#ifdef PC98
  out_fdc(fdcu, 3);
#else
  out_fdc(fdcu, ftu);
#endif

  if (ftintr_wait(ftu, FTCMD_RECAL, hz)) {
	splx(s);
	DPRT(("ft%d: recalibrate timeout\n", ftu));
	return(1);
  }
  splx(s);

  out_fdc(fdcu, NE7CMD_SPECIFY);
#ifdef PC98
  out_fdc(fdcu, (totape) ? 0xEF : 0xCF);
  out_fdc(fdcu, 0x02);
#else
  out_fdc(fdcu, (totape) ? 0xFD : 0xDF);
  out_fdc(fdcu, 0x02);
#endif

  DPRT(("tape_recal end\n"));
  return(0);
}

/*
 *  Wait for a particular tape status to be met.  If all is TRUE, then
 *  all states must be met, otherwise any state can be met.
 */
static int
tape_state(ftu_t ftu, int all, int mask, int seconds)
{
  int r, tries, maxtries;

  maxtries = (seconds) ? (4 * seconds) : 1;
  for (tries = 0; tries < maxtries; tries++) {
	r = tape_status(ftu);
	if (r >= 0) {
		if (all && (r & mask) == mask) return(r);
		if ((r & mask) != 0) return(r);
	}
	if (seconds) ftsleep(wc_long_delay, hz/4);
  }
  DPRT(("ft%d: tape_state failed on mask=$%02x maxtries=%d\n",
				ftu, mask, maxtries));
  return(-1);
}


/*
 *  Send a QIC command to tape drive, wait for completion.
 */
static int
tape_cmd(ftu_t ftu, int cmd)
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
#ifdef PC98
  out_fdc(fdcu, 3);
#else
  out_fdc(fdcu, ftu);
#endif
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
static int
tape_status(ftu_t ftu)
{
  int r, err, tries;
  ft_p ft = &ft_data[ftu];
  int max = (ft->attaching) ? 2 : 3;

  for (r = -1, tries = 0; r < 0 && tries < max; tries++)
	r = qic_status(ftu, QC_STATUS, 8);
  if (tries == max) return(-1);

recheck:
  DPRT(("tape_status got $%04x\n",r));
  ft->laststs = r;

  if (r & (QS_ERROR|QS_NEWCART)) {
	err = qic_status(ftu, QC_ERRCODE, 16);
	ft->lasterr = err;
	if (r & QS_NEWCART) {
		ft->newcart = 1;
		/* If tape not referenced, do a seek load point. */
		if ((r & QS_FMTOK) == 0 && !ft->attaching) {
			tape_cmd(ftu, QC_SEEKLP);
			do {
				ftsleep(wc_long_delay, hz);
			} while ((r = qic_status(ftu, QC_STATUS, 8)) < 0 ||
					(r & (QS_READY|QS_CART)) == QS_CART);
			goto recheck;
		}
	} else if (err && !ft->attaching) {
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
static void
tape_start(ftu_t ftu, int motor)
{
  ft_p	ft = &ft_data[ftu];
  fdc_p	fdc = ft->fdc;
  int s, mbits;
#ifndef PC98
  static int mbmotor[] = { FDO_MOEN0, FDO_MOEN1, FDO_MOEN2, FDO_MOEN3 };
#endif

  s = splbio();
  DPRT(("tape_start start\n"));

  /* reset, dma disable */
#ifdef PC98
  outb(fdc->baseport+FDOUT, FDO_RST | FDO_FRY | FDO_AIE | FDO_MTON);
#else
  outb(fdc->baseport+FDOUT, 0x00);
#endif
  (void)ftintr_wait(ftu, FTCMD_RESET, hz/10);

  /* raise reset, enable DMA, motor on if needed */
#ifdef PC98
  outb(fdc->baseport+FDOUT, FDO_DMAE | FDO_MTON);
#else
  mbits = ftu & 3;
  if (motor && ftu < 4)
	mbits |= mbmotor[ftu];

  outb(fdc->baseport+FDOUT, FDO_FRST | FDO_FDMAEN | mbits);
#endif
  (void)ftintr_wait(ftu, FTCMD_RESET, hz/10);

  splx(s);

  tape_recal(ftu, 1);

  /* set transfer speed */
#ifndef PC98
  outb(fdc->baseport+FDCTL, FDC_500KBPS);
  DELAY(10);
#endif

  DPRT(("tape_start end\n"));
}


/*
 *  Transfer control back to floppy disks.
 */
static void
tape_end(ftu_t ftu)
{
  ft_p	ft = &ft_data[ftu];
  fdc_p	fdc = ft->fdc;
  int s;

  DPRT(("tape_end start\n"));
  tape_recal(ftu, 0);

  s = splbio();

  /* reset, dma disable */
#ifdef PC98
  outb(fdc->baseport+FDOUT, FDO_RST | FDO_FRY | FDO_AIE | FDO_MTON);
#else
  outb(fdc->baseport+FDOUT, 0x00);
#endif
  (void)ftintr_wait(ftu, FTCMD_RESET, hz/10);

  /* raise reset, enable DMA */
#ifdef PC98
  outb(fdc->baseport+FDOUT, FDO_DMAE | FDO_MTON);
#else
  outb(fdc->baseport+FDOUT, FDO_FRST | FDO_FDMAEN);
#endif
  (void)ftintr_wait(ftu, FTCMD_RESET, hz/10);

  splx(s);

  /* set transfer speed */
#ifndef PC98
  outb(fdc->baseport+FDCTL, FDC_500KBPS);
  DELAY(10);
#endif
  fdc->flags &= ~FDC_TAPE_BUSY;

  DPRT(("tape_end end\n"));
}


/*
 *  Wait for the driver to go inactive, cancel readahead if necessary.
 */
static void
tape_inactive(ftu_t ftu)
{
  ft_p	ft = &ft_data[ftu];
  int s = splbio();

  if (ft->segh != NULL) {
	if (ft->segh->reqtype == FTIO_RDAHEAD) {
		/* cancel read-ahead */
		ft->segh->reqcan = 1;
	} else if (ft->segh->reqtype == FTIO_WRITING && !ft->active) {
		/* flush out any remaining writes */
		DPRT(("Flushing write I/O chain\n"));
		arq_state = ard_state = awr_state = 0;
		ft->xblk = ft->segh->reqblk;
		ft->xseg = ft->segh->reqseg;
		ft->xcnt = 0;
		ft->xptr = ft->segh->buff;
		ft->active = 1;
		timeout(ft_timeout, (caddr_t)ftu, 1);
	}
  }
  while (ft->active) ftsleep(wc_iosts_change, 0);
  splx(s);
}


/*
 *  Get the geometry of the tape currently in the drive.
 */
static int
ftgetgeom(ftu_t ftu)
{
  int r, i, tries;
  int cfg, qic80, ext;
  int sts, fmt, len;
  ft_p	ft = &ft_data[ftu];

  r = tape_status(ftu);

  /* XXX fix me when format mode is finished */
  if (r < 0 || (r & QS_CART) == 0 || (r & QS_FMTOK) == 0) {
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
static int
set_fdcmode(dev_t dev, int newmode)
{
  ftu_t ftu = FDUNIT(minor(dev));
  ft_p	ft = &ft_data[ftu];
  fdc_p	fdc = ft->fdc;
  static int havebufs = 0;
  int i;
  SegReq *sp, *rsp;

  if (newmode == FDC_TAPE_MODE) {
	/* Wake up the tape drive */
	switch (ft->type) {
	    case NO_TYPE:
		fdc->flags &= ~FDC_TAPE_BUSY;
		return(ENXIO);
	    case FT_NONE:
		tape_start(ftu, 0);
		break;
	    case FT_COLORADO:
		tape_start(ftu, 0);
		if (tape_cmd(ftu, QC_COL_ENABLE1)) {
			tape_end(ftu);
			return(EIO);
		}
		if (tape_cmd(ftu, QC_COL_ENABLE2 + ftu)) {
			tape_end(ftu);
			return(EIO);
		}
		break;
	    case FT_MOUNTAIN:
		tape_start(ftu, 0);
		if (tape_cmd(ftu, QC_MTN_ENABLE1)) {
			tape_end(ftu);
			return(EIO);
		}
		if (tape_cmd(ftu, QC_MTN_ENABLE2)) {
			tape_end(ftu);
			return(EIO);
		}
		break;
	    case FT_INSIGHT:
		tape_start(ftu, 1);
		break;
	    default:
		DPRT(("ft%d: bad tape type\n", ftu));
		return(ENXIO);
	}
	if (tape_status(ftu) < 0) {
		if (ft->type == FT_COLORADO)
			tape_cmd(ftu, QC_COL_DISABLE);
		else if (ft->type == FT_MOUNTAIN)
			tape_cmd(ftu, QC_MTN_DISABLE);
		tape_end(ftu);
		return(EIO);
	}

	/* Grab buffers from memory. */
	if (!havebufs) {
		ft->segh = ft->segt = NULL;
		ft->doneh = ft->donet = NULL;
		ft->segfree = NULL;
		ft->hdr = NULL;
		ft->nsegq = ft->ndoneq = ft->nfreelist = 0;
		for (i = 0; i < FTNBUFF; i++) {
			sp = malloc(sizeof(SegReq), M_DEVBUF, M_WAITOK);
			if (sp == NULL) {
				printf("ft%d: not enough memory for buffers\n", ftu);
				for (sp=ft->segfree; sp != NULL; sp=sp->next)
					free(sp, M_DEVBUF);
				if (ft->type == FT_COLORADO)
					tape_cmd(ftu, QC_COL_DISABLE);
				else if (ft->type == FT_MOUNTAIN)
					tape_cmd(ftu, QC_MTN_DISABLE);
				tape_end(ftu);
				return(ENOMEM);
			}
			sp->reqtype = FTIO_READY;
			sp->next = ft->segfree;
			ft->segfree = sp;
			ft->nfreelist++;
		}
		/* take one buffer for header */
		ft->hdr = ft->segfree;
		ft->segfree = ft->segfree->next;
		ft->nfreelist--;
		havebufs = 1;
	}
	ft->io_sts = FTIO_READY;	/* tape drive is ready */
	ft->active = 0;			/* interrupt driver not active */
	ft->moving = 0;			/* tape not moving */
	ft->rdonly = 0;			/* tape read only */
	ft->newcart = 0;		/* new cartridge flag */
	ft->lastpos = -1;		/* tape is rewound */
	async_func = ACMD_NONE;		/* No async function */
	tape_state(ftu, 0, QS_READY, 60);
	tape_cmd(ftu, QC_RATE);
	tape_cmd(ftu, QCF_RT500+2);		/* 500K bps */
	tape_state(ftu, 0, QS_READY, 60);
	ft->mode = FTM_PRIMARY;
	tape_cmd(ftu, QC_PRIMARY);	/* Make sure we're in primary mode */
	tape_state(ftu, 0, QS_READY, 60);
	ftg = NULL;			/* No geometry yet */
	ftgetgeom(ftu);			/* Get tape geometry */
	ftreq_rewind(ftu);		/* Make sure tape is rewound */
  } else {
	if (ft->type == FT_COLORADO)
		tape_cmd(ftu, QC_COL_DISABLE);
	else if (ft->type == FT_MOUNTAIN)
		tape_cmd(ftu, QC_MTN_DISABLE);
	tape_end(ftu);
	ft->newcart = 0;		/* clear new cartridge */
	if (ft->hdr != NULL) free(ft->hdr, M_DEVBUF);
	if (havebufs) {
		for (sp = ft->segfree; sp != NULL;) {
			rsp = sp; sp = sp->next;
			free(rsp, M_DEVBUF);
		}
		for (sp = ft->segh; sp != NULL;) {
			rsp = sp; sp = sp->next;
			free(rsp, M_DEVBUF);
		}
		for (sp = ft->doneh; sp != NULL;) {
			rsp = sp; sp = sp->next;
			free(rsp, M_DEVBUF);
		}
	}
	havebufs = 0;
  }
  return(0);
}


/*
 *  Perform a QIC status function.
 */
static int
qic_status(ftu_t ftu, int cmd, int nbits)
{
  int st3, r, i;
  ft_p	ft = &ft_data[ftu];
  fdcu_t fdcu = ft->fdc->fdcu;		/* fdc active unit */

  if (tape_cmd(ftu, cmd)) {
	DPRT(("ft%d: QIC status timeout\n", ftu));
	return(-1);
  }

  /* Sense drive status */
  out_fdc(fdcu, NE7CMD_SENSED);
#ifdef PC98
  out_fdc(fdcu, 3);
#else
  out_fdc(fdcu, ftu);
#endif
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
#ifdef PC98
	out_fdc(fdcu, 3);
#else
	out_fdc(fdcu, ftu);
#endif
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
int
ftopen(dev_t dev, int arg2) {
  ftu_t ftu = FDUNIT(minor(dev));
  fdc_p fdc;

  /* check bounds */
  if (ftu >= NFT)
	return(ENXIO);
  fdc = ft_data[ftu].fdc;
  if ((fdc == NULL) || (ft_data[ftu].type == NO_TYPE))
	  return(ENXIO);
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
int
ftclose(dev_t dev, int flags)
{
  ftu_t ftu = FDUNIT(minor(dev));
  ft_p	ft = &ft_data[ftu];


  /* Wait for any remaining I/O activity to complete. */
  tape_inactive(ftu);

  ft->mode = FTM_PRIMARY;
  tape_cmd(ftu, QC_PRIMARY);
  tape_state(ftu, 0, QS_READY, 60);
  ftreq_rewind(ftu);
  return(set_fdcmode(dev, FDC_DISK_MODE));	/* Otherwise, close tape */
}

/*
 *  Read or write a segment.
 */
static int
ftreq_rw(ftu_t ftu, int cmd, QIC_Segment *sr, struct proc *p)
{
  int r, i;
  SegReq *sp;
  int s;
  long blk, bad, seg;
  unsigned char *cp, *cp2;
  ft_p	ft = &ft_data[ftu];

  if (!ft->active && ft->segh == NULL) {
	r = tape_status(ftu);
	if ((r & QS_CART) == 0)
		return(ENXIO);	/* No cartridge */
	if ((r & QS_FMTOK) == 0)
		return(ENXIO);	/* Not formatted */
	tape_state(ftu, 0, QS_READY, 90);
  }

  if (ftg == NULL || ft->newcart) {
	tape_inactive(ftu);
	tape_state(ftu, 0, QS_READY, 90);
	if (ftgetgeom(ftu) < 0)
		return(ENXIO);
  }

  /* Write not allowed on a read-only tape. */
  if (cmd == QIOWRITE && ft->rdonly)
	return(EROFS);

  /* Quick check of request and buffer. */
  if (sr == NULL || sr->sg_data == NULL)
	return(EINVAL);

  /* Make sure requested track and segment is in range. */
  if (sr->sg_trk >= ftg->g_trktape || sr->sg_seg >= ftg->g_segtrk)
	return(EINVAL);

  blk = sr->sg_trk * ftg->g_blktrk + sr->sg_seg * QCV_BLKSEG;
  seg = sr->sg_trk * ftg->g_segtrk + sr->sg_seg;

  s = splbio();
  if (cmd == QIOREAD) {
	/*
	 *  See if the driver is reading ahead.
	 */
	if (ft->doneh != NULL ||
		(ft->segh != NULL && ft->segh->reqtype == FTIO_RDAHEAD)) {
		/*
		 *  Eat the completion queue and see if the request
		 *  is already there.
		 */
		while (ft->doneh != NULL) {
			if (blk == ft->doneh->reqblk) {
				sp = ft->doneh;
				sp->reqtype = FTIO_READING;
				sp->reqbad = sr->sg_badmap;
				goto rddone;
			}
			segio_free(ft, ft->doneh);
		}

		/*
		 *  Not on the completed queue, in progress maybe?
		 */
		if (ft->segh != NULL && ft->segh->reqtype == FTIO_RDAHEAD &&
				blk == ft->segh->reqblk) {
			sp = ft->segh;
			sp->reqtype = FTIO_READING;
			sp->reqbad = sr->sg_badmap;
			goto rdwait;
 		}
	}

	/* Wait until we're ready. */
	tape_inactive(ftu);

	/* Set up a new read request. */
	sp = segio_alloc(ft);
	sp->reqcrc = 0;
	sp->reqbad = sr->sg_badmap;
	sp->reqblk = blk;
	sp->reqseg = seg;
	sp->reqcan = 0;
	sp->reqtype = FTIO_READING;
	segio_queue(ft, sp);

	/* Start the read request off. */
	DPRT(("Starting read I/O chain\n"));
	arq_state = ard_state = awr_state = 0;
	ft->xblk = sp->reqblk;
	ft->xseg = sp->reqseg;
	ft->xcnt = 0;
	ft->xptr = sp->buff;
	ft->active = 1;
	timeout(ft_timeout, (caddr_t)ftu, 1);

rdwait:
	ftsleep(wc_buff_done, 0);

rddone:
	bad = sp->reqbad;
	sr->sg_crcmap = sp->reqcrc & ~bad;

	/* Copy out segment and discard bad mapped blocks. */
	cp = sp->buff; cp2 = sr->sg_data;
	for (i = 0; i < QCV_BLKSEG; cp += QCV_BLKSIZE, i++) {
		if (bad & (1 << i)) continue;
		copyout(cp, cp2, QCV_BLKSIZE);
		cp2 += QCV_BLKSIZE;
	}
	segio_free(ft, sp);
  } else {
	if (ft->segh != NULL && ft->segh->reqtype != FTIO_WRITING)
		tape_inactive(ftu);

	/* Allocate a buffer and start tape if we're running low. */
	sp = segio_alloc(ft);
	if (!ft->active && (sp == NULL || ft->nfreelist <= 1)) {
		DPRT(("Starting write I/O chain\n"));
		arq_state = ard_state = awr_state = 0;
		ft->xblk = ft->segh->reqblk;
		ft->xseg = ft->segh->reqseg;
		ft->xcnt = 0;
		ft->xptr = ft->segh->buff;
		ft->active = 1;
		timeout(ft_timeout, (caddr_t)ftu, 1);
	}

	/* Sleep until a buffer becomes available. */
	while (sp == NULL) {
		ftsleep(wc_buff_avail, 0);
		sp = segio_alloc(ft);
	}

	/* Copy in segment and expand bad blocks. */
	bad = sr->sg_badmap;
	cp = sr->sg_data; cp2 = sp->buff;
	for (i = 0; i < QCV_BLKSEG; cp2 += QCV_BLKSIZE, i++) {
		if (bad & (1 << i)) continue;
		copyin(cp, cp2, QCV_BLKSIZE);
		cp += QCV_BLKSIZE;
	}
	sp->reqblk = blk;
	sp->reqseg = seg;
	sp->reqcan = 0;
	sp->reqtype = FTIO_WRITING;
	segio_queue(ft, sp);
  }
  splx(s);
  return(0);
}


/*
 *  Rewind to beginning of tape
 */
static int
ftreq_rewind(ftu_t ftu)
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


/*
 *  Move to logical beginning or end of track
 */
static int
ftreq_trkpos(ftu_t ftu, int req)
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


/*
 *  Seek tape head to a particular track.
 */
static int
ftreq_trkset(ftu_t ftu, int *trk)
{
  int r;
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


/*
 *  Start tape moving forward.
 */
static int
ftreq_lfwd(ftu_t ftu)
{
  ft_p	ft = &ft_data[ftu];

  tape_inactive(ftu);
  tape_cmd(ftu, QC_STOP);
  tape_state(ftu, 0, QS_READY, 90);
  tape_cmd(ftu, QC_FORWARD);
  ft->moving = 1;
  return(0);
}


/*
 *  Stop the tape
 */
static int
ftreq_stop(ftu_t ftu)
{
  ft_p	ft = &ft_data[ftu];

  tape_inactive(ftu);
  tape_cmd(ftu, QC_STOP);
  tape_state(ftu, 0, QS_READY, 90);
  ft->moving = 0;
  return(0);
}


/*
 *  Set the particular mode the drive should be in.
 */
static int
ftreq_setmode(ftu_t ftu, int cmd)
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


/*
 *  Return drive status bits
 */
static int
ftreq_status(ftu_t ftu, int cmd, int *sts, struct proc *p)
{
  ft_p	ft = &ft_data[ftu];

  if (ft->active)
	*sts = ft->laststs & ~QS_READY;
  else
	*sts = tape_status(ftu);
  return(0);
}


/*
 *  Return drive configuration bits
 */
static int
ftreq_config(ftu_t ftu, int cmd, int *cfg, struct proc *p)
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


/*
 *  Return current tape's geometry.
 */
static int
ftreq_geom(ftu_t ftu, QIC_Geom *g)
{
  tape_inactive(ftu);
  if (ftg == NULL && ftgetgeom(ftu) < 0) return(ENXIO);
  bcopy(ftg, g, sizeof(QIC_Geom));
  return(0);
}


/*
 *  Return drive hardware information
 */
static int
ftreq_hwinfo(ftu_t ftu, QIC_HWInfo *hwp)
{
  int tries;
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
 *  Receive or Send the in-core header segment.
 */
static int
ftreq_hdr(ftu_t ftu, int cmd, QIC_Segment *sp)
{
  ft_p	ft = &ft_data[ftu];
  QIC_Header *h = (QIC_Header *)ft->hdr->buff;

  if (sp == NULL || sp->sg_data == NULL) return(EINVAL);
  if (cmd == QIOSENDHDR) {
	copyin(sp->sg_data, ft->hdr->buff, QCV_SEGSIZE);
  } else {
	if (h->qh_sig != QCV_HDRMAGIC) return(EIO);
	copyout(ft->hdr->buff, sp->sg_data, QCV_SEGSIZE);
  }
  return(0);
}

/*
 *  I/O functions.
 */
int
ftioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
  ftu_t ftu = FDUNIT(minor(dev));

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

     case QIOSENDHDR:
     case QIORECVHDR:
	return(ftreq_hdr(ftu, cmd, (QIC_Segment *)data));
  }
badreq:
  DPRT(("ft%d: unknown ioctl(%d) request\n", ftu, cmd));
  return(ENXIO);
}

#endif
