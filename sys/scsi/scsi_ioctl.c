/*
 * Copyright (C) 1992, 1993, 1994, HD Associates, Inc.
 * PO Box 276
 * Pepperell, MA 01463
 * 508 433 5266
 * dufault@hda.com
 *
 * This code is contributed to the University of California at Berkeley:
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
 *End copyright
 *
 * $FreeBSD$
 *
 *
 */

#include "opt_bounce.h"
#include "opt_scsi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/buf.h>

#define	b_screq b_driver1	/* a patch in buf.h */
#define	b_sc_link b_driver2	/* a patch in buf.h */

#include <sys/fcntl.h>
#include <sys/proc.h>
#include <vm/vm.h>

#include "scbus.h"
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <sys/scsiio.h>

static void scsierr(struct buf *, int); /* XXX ??? */

/*
 * We let the user interpret his own sense in the generic scsi world.
 * This routine is called at interrupt time if the SCSI_USER bit was set
 * in the flags passed to scsi_scsi_cmd(). No other completion processing
 * takes place, even if we are running over another device driver.
 * The lower level routines that call us here, will free the xs and restart
 * the device's queue if such exists.
 */
#ifndef min
#define min(A,B) ((A<B) ? A : B )
#endif

void scsi_user_done(xs)
struct	scsi_xfer *xs;
{

	struct	buf	*bp;
	scsireq_t *screq;

	bp = xs->bp;
	if(!bp) {	/* ALL user requests must have a buf */
		sc_print_addr(xs->sc_link);
		printf("User command with no buf\n");
		return ;
	}
	screq = bp->b_screq;
	if (!screq) {	/* Is it one of ours? (the SCSI_USER bit says it is) */
		sc_print_addr(xs->sc_link);
		printf("User command with no request\n");
		return ;
	}

	SC_DEBUG(xs->sc_link,SDEV_DB2,("user-done\n"));
	screq->retsts = 0;
	screq->status = xs->status;
	switch((int)xs->error) {
	case	XS_NOERROR:
		SC_DEBUG(xs->sc_link,SDEV_DB3,("no error\n"));
		if (xs->flags & SCSI_RESID_VALID)
			screq->datalen_used = xs->datalen - xs->resid;
		else
			screq->datalen_used = xs->datalen;
		screq->retsts = SCCMD_OK;
		break;

	case	XS_SENSE:
		SC_DEBUG(xs->sc_link,SDEV_DB3,("have sense\n"));
		screq->senselen_used = min(sizeof(xs->sense),SENSEBUFLEN);
		bcopy(&xs->sense,screq->sense,screq->senselen);
		screq->retsts = SCCMD_SENSE;
		break;

	case	XS_DRIVER_STUFFUP:
		sc_print_addr(xs->sc_link);
		printf("host adapter code inconsistency\n");
		screq->retsts = SCCMD_UNKNOWN;
		break;

	case	XS_TIMEOUT:
		SC_DEBUG(xs->sc_link,SDEV_DB3,("timeout\n"));
		screq->retsts = SCCMD_TIMEOUT;
		break;

	case	XS_BUSY:
		SC_DEBUG(xs->sc_link,SDEV_DB3,("busy\n"));
		screq->retsts = SCCMD_BUSY;
		break;

	default:
		sc_print_addr(xs->sc_link);
		printf("unknown error category from host adapter code\n");
		screq->retsts = SCCMD_UNKNOWN;
		break;
	}
	biodone(bp); 	/* we're waiting on it in scsistrategy() */
	return;		/* it'll free the xs and restart any queue */
}


/* Pseudo strategy function
 * Called by scsi_do_ioctl() via physio/physstrat if there is to
 * be data transfered, and directly if there is no data transfer.
 *
 * Can't be used with block devices or raw_read/raw_write directly
 * from the cdevsw/bdevsw tables because they couldn't have added
 * the screq structure. [JRE]
 */
static void 
scsistrategy(struct buf *bp)
{
	errval err;
	struct	scsi_link *sc_link = bp->b_sc_link;
	scsireq_t *screq;
	u_int32_t flags = 0;
	int s;


	if(!sc_link) {
		printf("user_strat: No link pointer\n");
		scsierr(bp,EINVAL);
		return;
	}
	SC_DEBUG(sc_link,SDEV_DB2,("user_strategy\n"));
	screq = bp->b_screq;
	if(!screq) {
		sc_print_addr(sc_link);
		printf("No request block\n");
		scsierr(bp,EINVAL);
		return;
	}

	/* We're in trouble if physio tried to break up the
	 * transfer:
	 */
	if (bp->b_bcount != screq->datalen) {
		sc_print_addr(sc_link);
		printf("physio split the request.. cannot proceed\n");
		scsierr(bp, EIO);
		return;
	}

	if (screq->timeout == 0) {
		scsierr(bp, EINVAL);
		return;
	}

	if (screq->cmdlen > sizeof(struct scsi_generic)) {
		sc_print_addr(sc_link);
		printf("cmdlen too big ");
		scsierr(bp, EFAULT);
		return;
	}


	if (screq->flags & SCCMD_READ)
		flags |= SCSI_DATA_IN;

	if (screq->flags & SCCMD_WRITE)
		flags |= SCSI_DATA_OUT;

	if (screq->flags & SCCMD_TARGET)
		flags |= SCSI_TARGET;

	if (screq->flags & SCCMD_ESCAPE)
		flags |= SCSI_ESCAPE;

#ifdef BOUNCE_BUFFERS
	if (sc_link->flags & SDEV_BOUNCE)
		vm_bounce_alloc(bp);
#endif

	err = scsi_scsi_cmd(sc_link,
			(struct	scsi_generic *)screq->cmd,
			screq->cmdlen,
			(u_char *)bp->b_un.b_addr,
			screq->datalen,
			0,	/* user must do the retries *//* ignored */
			screq->timeout,
			bp,
			flags | SCSI_USER);



	/*because there is a bp, scsi_scsi_cmd will return immediatly*/
	if (err)
	{
		scsierr(bp, err);
		return;
	}
	SC_DEBUG(sc_link,SDEV_DB3,("about to  sleep\n"));
	s = splbio();
	while(!(bp->b_flags & B_DONE))
	{
		tsleep((caddr_t)bp, PRIBIO, "scsistrat", 0);
	}
	splx(s);
	SC_DEBUG(sc_link,SDEV_DB3,("back from sleep\n"));
	return;
}

/*
 * Something (e.g. another driver) has called us
 * with an sc_link for a target/lun/adapter, and a scsi
 * specific ioctl to perform, better try.
 * If user-level type command, we must still be running
 * in the context of the calling process
 */
errval	scsi_do_ioctl(dev_t dev, int cmd, caddr_t addr, int flags,
struct proc *p, struct scsi_link *sc_link)
{
	errval ret = 0;

	/* If we can't write the device we can't permit much:
	 */

	if (cmd != SCIOCIDENTIFY && !(flags & FWRITE))
		return EACCES;

	SC_DEBUG(sc_link,SDEV_DB2,("scsi_do_ioctl(0x%x)\n",cmd));
	switch(cmd)
	{
		case SCIOCCOMMAND:
		{
			/*
			 * You won't believe this, but the arg copied in
 			 * from the user space, is on the kernel stack
			 * for this process, so we can't write
			 * to it at interrupt time..
			 * we need to copy it in and out!
			 * Make a static copy using malloc!
			 */
			scsireq_t *screq2 = (scsireq_t *)addr;
			scsireq_t *screq = (scsireq_t *)addr;
			int rwflag = (screq->flags & SCCMD_READ) ? B_READ : B_WRITE;
			struct buf *bp;
			caddr_t	d_addr;
			int	len;

#if 0	/* XXX dufault@hda.com: This looks too rev dependent.  Do it always? */
			if((unsigned int)screq < (unsigned int)KERNBASE)
#endif
			{
				screq = malloc(sizeof(scsireq_t),M_TEMP,M_WAITOK);
				bcopy(screq2,screq,sizeof(scsireq_t));
			}
			bp = malloc(sizeof (struct buf),M_TEMP,M_WAITOK);
			bzero(bp,sizeof(struct buf));
			d_addr = screq->databuf;
			bp->b_bcount = len = screq->datalen;
			bp->b_screq = screq;
			bp->b_sc_link = sc_link;
			if (len) {
				struct uio auio;
				struct iovec aiov;
				long cnt;

				aiov.iov_base = d_addr;
				aiov.iov_len = len;
				auio.uio_iov = &aiov;
				auio.uio_iovcnt = 1;

				auio.uio_resid = len;
				if (auio.uio_resid < 0)
					return (EINVAL);

				auio.uio_rw = (rwflag == B_READ)  ? UIO_READ : UIO_WRITE;
				auio.uio_segflg = UIO_USERSPACE;
				auio.uio_procp = curproc;
				cnt = len;
				ret = physio(scsistrategy, bp, dev, rwflag,
					minphys, &auio);
			} else {
				/* if no data, no need to translate it.. */
				bp->b_un.b_addr = 0;
				bp->b_dev = dev;
				bp->b_flags |= B_BUSY;

				scsistrategy(bp);
				ret =  bp->b_error;
			}
			free(bp,M_TEMP);
#if 0	/* XXX dufault@hda.com: This looks too rev dependent.  Do it always? */
			if((unsigned int)screq2 < (unsigned int)KERNBASE)
#endif
			{
				bcopy(screq,screq2,sizeof(scsireq_t));
				free(screq,M_TEMP);
			}
			break;
		}
		case SCIOCDEBUG:
		{
			int level = *((int *)addr);
			SC_DEBUG(sc_link,SDEV_DB3,("debug set to %d\n",level));
			sc_link->flags &= ~SDEV_DBX; /*clear debug bits */
			if(level & 1) sc_link->flags |= SDEV_DB1;
			if(level & 2) sc_link->flags |= SDEV_DB2;
			if(level & 4) sc_link->flags |= SDEV_DB3;
			if(level & 8) sc_link->flags |= SDEV_DB4;
			ret = 0;
			break;
		}
		case SCIOCREPROBE:
		{
			struct scsi_addr *sca = (struct scsi_addr *) addr;

			ret = scsi_probe_busses(sca->scbus,sca->target,sca->lun);
			break;
		}
		case SCIOCRECONFIG:
		case SCIOCDECONFIG:
			ret = EINVAL;
			break;

		case SCIOCIDENTIFY:
		{
			struct scsi_addr *sca = (struct scsi_addr *) addr;
			sca->scbus	= sc_link->scsibus;
			sca->target	= sc_link->target;
			sca->lun	= sc_link->lun;
			break;
		}

		default:
			ret = ENOTTY;
		break;
	}

	return ret;
}

void
scsierr(bp,err)
	struct buf *bp;
	int	err;
{
		bp->b_flags |= B_ERROR;
		bp->b_error = err;
		biodone(bp);
		return;
}

