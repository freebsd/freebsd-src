/* 
 * Contributed by HD Associates (hd@world.std.com).
 * Copyright (c) 1992, 1993 HD Associates
 *
 * Berkeley style copyright.  I've just snarfed it out of stdio.h:
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
 *	from: @(#)stdio.h	5.17 (Berkeley) 6/3/91
 *	$Id: sg.c,v 1.2 1993/10/16 17:21:06 rgrimes Exp $
 */
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_generic.h>
#include "sg.h"
#include <sys/sgio.h>

#define SGOUTSTANDING   2
#define SG_RETRIES   2
#define SPLSG splbio

/* Use one of the implementation defined spare bits
 * to indicate the escape op:
 */
#define DSRQ_ESCAPE DSRQ_CTRL1

struct sg
{
	int flags;
	struct scsi_switch *sc_sw;
	int ctlr;

	long int ad_info;	/* info about the adapter */
	int cmdscount;		/* cmds allowed outstanding by the board */

	struct scsi_xfer *free_xfer;
	int free_xfer_wait;
};

/* This is used to associate a struct dsreq and a struct buf.
 */
typedef struct dsbuf
{
	dsreq_t *dsreq;
	struct buf buf;

	/* I think this is a portable way to get back to the base of
	 * the enclosing structure:
	 */
#	define DSBUF_P(BP) ((dsbuf_t *)((caddr_t)(BP) - (caddr_t)&((dsbuf_t *)0)->buf))

	int magic;

#	define DSBUF_MAGIC 0xDBFACDBF
} dsbuf_t;

#if NSG > 4
/* The host adapter unit is encoded in the upper 2 bits of the minor number
 * (the SGI flag bits).
 */
#error "NSG can't be > 4 unless the method of encoding the board unit changes"
#endif

struct sg *sgs[NSG];

#define SG(DEV) sgs[G_SCSI_UNIT(DEV)]

struct sg *sg_new(int lun)
{
	struct sg *sg  = (struct sg *)malloc(sizeof(*sg),M_TEMP, M_NOWAIT);

	if (sg == 0)
		return 0;

	bzero(sg, sizeof(struct sg));

	return sg;
}

int sg_attach(ctlr, scsi_addr, scsi_switch)
int ctlr,scsi_addr;
struct  scsi_switch *scsi_switch;
{
	struct sg *sg;
	int i;
	struct  scsi_xfer   *scsi_xfer;
	static int next_sg_unit = 0;

	int unit = next_sg_unit++;

	if (unit >= NSG)
	{
		printf("Too many generic SCSIs (%d > %d); reconfigure the kernel.\n",
		unit+1, NSG);

		if (NSG == 4)
			printf(
			"You have hit the max of 4.  You will have to change the driver.\n");

		return 0;
	}

	if ((sg = sg_new(0)) == 0)
		return 0;

	sgs[unit] = sg;

	sg->sc_sw = scsi_switch;
	sg->ctlr = ctlr;

	/* This is a bit confusing.  It looks like Julian calls back into the
	 * adapter to find out how many outstanding transactions it can
	 * handle.  How does he handle a tape/disk combo?
	 */

	if (sg->sc_sw->adapter_info)
	{
		sg->ad_info = ( (*(sg->sc_sw->adapter_info))(unit));
		sg->cmdscount = sg->ad_info & AD_INF_MAX_CMDS;
		if(sg->cmdscount > SGOUTSTANDING)
			sg->cmdscount = SGOUTSTANDING;
	}
	else
	{
		sg->ad_info = 1;
		sg->cmdscount = 1;
	}

	i = sg->cmdscount;

	scsi_xfer = (struct scsi_xfer *)malloc(sizeof(struct scsi_xfer) *
	i, M_TEMP, M_NOWAIT);

	if (scsi_xfer == 0)
	{
		printf("scsi_generic: Can't malloc.\n");
		return 0;
	}

	while (i--)
	{
		scsi_xfer->next = sg->free_xfer;
		sg->free_xfer = scsi_xfer;
		scsi_xfer++;
	}

#ifndef EMBEDDED
	if (unit == 0)
		printf("  /dev/gs%d (instance 0) generic SCSI via controller %d\n",
		scsi_addr, sg->ctlr);
	else
		printf("  /dev/gs%d-%d generic SCSI via controller %d\n",
		unit, scsi_addr, sg->ctlr);
#endif

	return 1;
}

/* It is trivial to add support for processor target devices
 * here - enable target mode on open and disable on close
 * if a flag bit is set in the minor number
 */
int sgopen(dev_t dev)
{
	if (SG(dev) == 0)
		return ENXIO;

	return 0;
}

int sgclose(dev_t dev)
{
	return 0;
}


/* Free a scsi_xfer, wake processes waiting for it
 */
void sg_free_xs(dev_t dev, struct scsi_xfer *xs, int flags)
{
	int	s;
	struct sg *sg = SG(dev);
	
	if(flags & SCSI_NOMASK)
	{
		if (sg->free_xfer_wait)
		{
			printf("sg_free_xs: doing a wakeup from NOMASK mode!\n");
			wakeup((caddr_t)&sg->free_xfer);
		}
		xs->next = sg->free_xfer;
		sg->free_xfer = xs;
	}
	else
	{
		s = SPLSG();
		if (sg->free_xfer_wait)
			wakeup((caddr_t)&sg->free_xfer);
		xs->next = sg->free_xfer;
		sg->free_xfer = xs;
		splx(s);
	}
}

/* Get ownership of a scsi_xfer
 * If need be, sleep on it, until it comes free
 */
struct scsi_xfer *sg_get_xs(dev_t dev, int flags)
{
	struct scsi_xfer *xs;
	int	s;
	struct sg *sg = SG(dev);

	if(flags & (SCSI_NOSLEEP |  SCSI_NOMASK))
	{
		if (xs = sg->free_xfer)
		{
			sg->free_xfer = xs->next;
			xs->flags = 0;
		}
	}
	else
	{
		s = SPLSG();
		while (!(xs = sg->free_xfer))
		{
			sg->free_xfer_wait++;  /* someone waiting! */
			sleep((caddr_t)&sg->free_xfer, PRIBIO+1);
			sg->free_xfer_wait--;
		}
		sg->free_xfer = xs->next;
		splx(s);
		xs->flags = 0;
	}

	return xs;
}

/* We let the user interpret his own sense in the
 * generic scsi world
 */
int sg_interpret_sense(dev_t dev, struct scsi_xfer *xs, int *flag_p)
{
	return 0;
}

/* ITSDONE is really used for things that are marked one
 * in the interrupt.  I'll leave the logic in in case I want
 * to move done processing (and therefore have a start queue)
 * back into the interrupt.
 * BUG: No start queue.
 */

int sg_done(dev_t dev,
struct  scsi_xfer   *xs)
{
	xs->flags |= ITSDONE;
	wakeup(xs);
	return 0;
}

int sg_submit_cmd(dev_t dev, struct scsi_xfer *xs, dsreq_t *dsreq)
{
	int retval;

	struct sg *sg = SG(dev);

retry:
	xs->error	=	XS_NOERROR;

	xs->bp		=	0;	/* This bp doesn't seem to be used except to
						 * disable sleeping in the host adapter code.
						 * "st" does set it up, though.
						 */

	retval = (*(sg->sc_sw->scsi_cmd))(xs);

	switch(retval)
	{
		case	SUCCESSFULLY_QUEUED:
			while(!(xs->flags & ITSDONE))
				sleep(xs,PRIBIO+1);

		/* Fall through... */

		case	HAD_ERROR:

		if (dsreq)
			dsreq->ds_status = xs->status;

		switch(xs->error)
		{
		case	XS_NOERROR:
			if (dsreq)
				dsreq->ds_datasent = dsreq->ds_datalen - xs->resid;
			retval = 0;
			break;

		case	XS_SENSE:
			retval = (sg_interpret_sense(dev ,xs, (int *)0));
			if (dsreq)
			{
				dsreq->ds_sensesent = sizeof(xs->sense);
				dsreq->ds_ret = DSRT_SENSE;
			}
			retval = 0;
			break;

		case	XS_DRIVER_STUFFUP:
			if (dsreq)
				dsreq->ds_ret = DSRT_HOST;
			printf("sg%d: host adapter code inconsistency\n" ,G_SCSI_UNIT(dev));
			retval = EIO;
			break;

		case	XS_TIMEOUT:
			if (dsreq)
				dsreq->ds_ret = DSRT_TIMEOUT;
			retval = ETIMEDOUT;
			break;

		case	XS_BUSY:
			if(xs->retries-- )
			{
				xs->flags &= ~ITSDONE;
				goto retry;
			}
			retval = EBUSY;
			break;

		default:
			printf("sg%d: unknown error category from host adapter code\n"
				,G_SCSI_UNIT(dev));
			retval = EIO;
			break;
		}
		break;

	case	COMPLETE:
		if (dsreq)
			dsreq->ds_datasent = dsreq->ds_datalen - xs->resid;
		retval = 0;
		break;

	case 	TRY_AGAIN_LATER:
		if(xs->retries-- )
		{
			xs->flags &= ~ITSDONE;
			goto retry;
		}
		retval = EBUSY;
		break;

	case 	ESCAPE_NOT_SUPPORTED:
		retval = ENOSYS;	/* "Function not implemented" */
		break;

	default:
		printf("sg%d: illegal return from host adapter code\n",
		G_SCSI_UNIT(dev));
		retval = EIO;
		break;
	}

	return retval;
}

/* sg_escape: Do a generic SCSI escape
 */
int sg_escape(dev_t dev, int op_code, u_char *b, int nb)
{
	int retval;

	struct scsi_generic scsi_generic;

	int flags = SCSI_ESCAPE;

	struct scsi_xfer *xs;
	struct sg *sg = SG(dev);

	xs = sg_get_xs(dev, flags);

	if (xs == 0)
	{
		printf("sg_target%d: controller busy"
		" (this should never happen)\n",G_SCSI_UNIT(dev));
		return EBUSY;
	}

	scsi_generic.opcode = op_code;
	bcopy(b, scsi_generic.bytes, nb);

	/* Fill out the scsi_xfer structure
	 */
	xs->flags	=	(flags|INUSE);
	xs->adapter	=	sg->ctlr;
	xs->cmd		=	&scsi_generic;
	xs->targ	=	G_SCSI_ID(dev);
	xs->lu		=	G_SCSI_LUN(dev);
	xs->retries	=	SG_RETRIES;
	xs->timeout	=	100;
	xs->when_done	=	(flags & SCSI_NOMASK)
				?(int (*)())0
				:(int (*)())sg_done;
	xs->done_arg	=	dev;
	xs->done_arg2	=	(int)xs;

	xs->status = 0;

	retval = sg_submit_cmd(dev, xs, 0);

	bcopy(scsi_generic.bytes, b, nb);

	sg_free_xs(dev,xs,flags);

	return retval;
}

/* sg_target: Turn on / off target mode
 */
int sg_target(dev_t dev, int enable)
{
	u_char b0 = enable;
	return sg_escape(dev, SCSI_OP_TARGET, &b0, 1);
}

#ifdef EMBEDDED
/* This should REALLY be a select call!
 * This is used in a stand alone system without an O/S.  I didn't
 * have the time to add select, which the system was missing,
 * so I added this stuff to poll for the async arrival of
 * connections for target mode.
 */
int sg_poll(dev_t dev, int *send, int *recv)
{
	scsi_op_poll_t s;
	int ret;

	ret = sg_escape(dev, SCSI_OP_POLL, (u_char *)&s, sizeof(s));

	if (ret == 0)
	{
		*send = s.send;
		*recv = s.recv;
	}

	return ret;
}
#endif /* EMBEDDED */

int sg_scsi_cmd(dev_t dev,
dsreq_t *dsreq,
struct scsi_generic *scsi_cmd,
u_char *d_addr,
long d_count,
struct scsi_sense_data *scsi_sense)
{
	int retval;

	int flags = 0;
	struct scsi_xfer *xs;
	struct sg *sg = SG(dev);

	if (sg->sc_sw == 0)
		return ENODEV;

	dsreq->ds_status = 0;
	dsreq->ds_sensesent = 0;

	if (dsreq->ds_flags & DSRQ_READ)
		flags |= SCSI_DATA_IN;

	if (dsreq->ds_flags & DSRQ_WRITE)
		flags |= SCSI_DATA_OUT;

	if (dsreq->ds_flags & DSRQ_TARGET)
		flags |= SCSI_TARGET;

	if (dsreq->ds_flags & DSRQ_ESCAPE)
		flags |= SCSI_ESCAPE;

#ifdef SCSI_PHYSADDR
	if (dsreq->ds_flags & DSRQ_PHYSADDR)
		flags |= SCSI_PHYSADDR;
#endif

	xs = sg_get_xs(dev, flags);

	if (xs == 0)
	{
		printf("sg_scsi_cmd%d: controller busy"
		" (this should never happen)\n",G_SCSI_UNIT(dev));

		return EBUSY;
	}

	/* Fill out the scsi_xfer structure
	 */
	xs->flags	|=	(flags|INUSE);
	xs->adapter	=	sg->ctlr;
	xs->targ	=	G_SCSI_ID(dev);
	xs->lu		=	G_SCSI_LUN(dev);
	xs->retries	=	SG_RETRIES;
	xs->timeout	=	dsreq->ds_time;
	xs->cmd		=	scsi_cmd;
	xs->cmdlen	=	dsreq->ds_cmdlen;
	xs->data	=	d_addr;
	xs->datalen	=	d_count;
	xs->resid	=	d_count;
	xs->when_done	=	(flags & SCSI_NOMASK)
				?(int (*)())0
				:(int (*)())sg_done;
	xs->done_arg	=	dev;
	xs->done_arg2	=	(int)xs;

	xs->req_sense_length = (dsreq->ds_senselen < sizeof(struct scsi_sense_data))
		? dsreq->ds_senselen
		: sizeof(struct scsi_sense_data);
	xs->status = 0;

	retval = sg_submit_cmd(dev, xs, dsreq);

	if (dsreq->ds_ret == DSRT_SENSE)
		bcopy(&(xs->sense), scsi_sense, sizeof(xs->sense));

	sg_free_xs(dev,xs,flags);

	return retval;
}

void sgerr(struct buf *bp, int err)
{
	bp->b_error = err;
	bp->b_flags |= B_ERROR;

	iodone(bp);
}

/* strategy function
 * 
 * Should I reorganize this so it returns to physio instead
 * of sleeping in sg_scsi_cmd?  Is there any advantage, other
 * than avoiding the probable duplicate wakeup in iodone?
 *
 * Don't create a block device entry point for this
 * driver without making some fixes:
 * you have to be able to go from the bp to the dsreq somehow.
 */
void sgstrategy(struct buf *bp)
{
	int err;
	struct scsi_generic scsi_generic;
	struct scsi_sense_data scsi_sense;
	int lun = G_SCSI_LUN(bp->b_dev);

	dsbuf_t *dsbuf = DSBUF_P(bp);
	dsreq_t *dsreq;

	if (dsbuf->magic != DSBUF_MAGIC)
	{
		printf("sgstrategy: struct buf not magic.\n");
		sgerr(bp, EFAULT);
		return;
	}

	dsreq = dsbuf->dsreq;

	/* We're in trouble if physio tried to break up the
	 * transfer:
	 */
	if (bp->b_bcount != dsreq->ds_datalen)
	{
		printf("sgstrategy unit%d: Transfer broken up.\n",
		G_SCSI_UNIT(bp->b_dev));
		sgerr(bp, EIO);
		return;
	}

	dsreq->ds_ret = DSRT_OK;

	/* Reject 0 length timeouts.
	 */
	if (dsreq->ds_time == 0)
	{
		sgerr(bp, EINVAL);
		return;
	}

	if (dsreq->ds_cmdlen > sizeof(struct scsi_generic))
	{
		sgerr(bp, EFAULT);
		return;
	}

	copyin(dsreq->ds_cmdbuf, (char *)&scsi_generic, dsreq->ds_cmdlen);

	/* Use device unit for the LUN.  Using the one the user provided
	 * would be a huge security problem.
	 */
	if ((dsreq->ds_flags & DSRQ_ESCAPE) == 0)
		scsi_generic.bytes[0] = (scsi_generic.bytes[0] & 0x1F) | (lun << 5);

	err = sg_scsi_cmd(bp->b_dev, dsreq,
	 &scsi_generic,
	 (u_char *)bp->b_un.b_addr,
	 bp->b_bcount,
	 &scsi_sense);

	if (dsreq->ds_sensesent)
	{
		if (dsreq->ds_sensesent > dsreq->ds_senselen)
			dsreq->ds_sensesent = dsreq->ds_senselen;

		copyout(&scsi_sense, dsreq->ds_sensebuf, dsreq->ds_sensesent);
	}

	if (err)
	{
		if (dsreq->ds_ret == DSRT_OK)
			dsreq->ds_ret = DSRT_DEVSCSI;

		sgerr(bp, err);
		return;
	}

	/* This is a fake.  It would be nice to know if the
	 * command was sent or not instead of pretending it was if
	 * we get this far.  That would involve adding "sent" members
	 * to the xs so it could be set up down in the host adapter code.
	 */
	dsreq->ds_cmdsent = dsreq->ds_cmdlen;

	if (dsreq->ds_ret == 0)
		dsreq->ds_ret = DSRT_OK;

	iodone(bp);	/* Shouldn't this iodone be done in the interrupt?
				 */

	return;
}

void sgminphys(struct buf *bp)
{
}

int sgioctl(dev_t dev, int cmd, caddr_t addr, int f)
{
	int ret = 0;
	int phys;

	switch(cmd)
	{
		case DS_ENTER:
		{
			dsreq_t *dsreq = (dsreq_t *)addr;

			int rwflag = (dsreq->ds_flags & DSRQ_READ) ? B_READ : B_WRITE;

			struct dsbuf dsbuf;
			struct buf *bp = &dsbuf.buf;

			bzero(&dsbuf, sizeof(dsbuf));

			dsbuf.dsreq = dsreq;
			dsbuf.magic = DSBUF_MAGIC;

#ifdef SCSI_PHYSADDR	/* Physical memory addressing option */
			phys = (dsreq->ds_flags & DSRQ_PHYSADDR);
#else
			phys = 0;
#endif

			if (phys)
			{
				bp->b_un.b_addr = dsreq->ds_databuf;
				bp->b_bcount = dsreq->ds_datalen;
				bp->b_dev = dev;
				bp->b_flags = rwflag;

				sgstrategy(bp);
				ret =  bp->b_error;
			}
			else if (dsreq->ds_datalen)
			{
				struct uio uio;
				struct iovec iovec;

				iovec.iov_base = dsreq->ds_databuf;
				iovec.iov_len = dsreq->ds_datalen;

				uio.uio_offset = 0;
				uio.uio_resid = dsreq->ds_datalen;

				uio.uio_segflg = UIO_USERSPACE;
				uio.uio_procp = curproc;
				uio.uio_rw = (rwflag == B_READ) ? UIO_READ : UIO_WRITE;
				uio.uio_iov = &iovec;
				uio.uio_iovcnt = 1;

				if ((ret = rawio(dev, &uio, bp)) == 0)
					ret = bp->b_error;
			}
			else
			{
				bp->b_un.b_addr = 0;
				bp->b_bcount = 0;
				bp->b_dev = dev;
				bp->b_flags = 0;

				sgstrategy(bp);
				ret =  bp->b_error;
			}

		}
		break;

		case DS_TARGET:
			ret = sg_target(dev, *(int *)addr);
		break;

		default:
			ret = ENOTTY;
		break;
	}

	return ret;
}
