/*-
 * Copyright (c) 1997, 1998 Nicolas Souchu
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: vpo.c,v 1.6 1998/08/03 19:14:31 msmith Exp $
 *
 */

#ifdef KERNEL
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/buf.h>

#include <machine/clock.h>

#endif	/* KERNEL */
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#ifdef	KERNEL
#include <sys/kernel.h>
#endif /*KERNEL */

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/vpoio.h>

#define VP0_BUFFER_SIZE 0x12000

struct vpo_sense {
	struct scsi_sense cmd;
	unsigned int stat;
	unsigned int count;
};

struct vpo_data {
	unsigned short vpo_unit;

	int vpo_stat;
	int vpo_count;
	int vpo_error;

	int vpo_isplus;

	struct ppb_status vpo_status;
	struct vpo_sense vpo_sense;

	unsigned char vpo_buffer[VP0_BUFFER_SIZE];

	struct vpoio_data vpo_io;	/* interface to low level functions */

	struct scsi_link sc_link;
};


static int32_t	vpo_scsi_cmd(struct scsi_xfer *);
static void	vpominphys(struct buf *);
static u_int32_t vpo_adapter_info(int);

static int	nvpo = 0;
#define MAXVP0	8			/* XXX not much better! */
static struct vpo_data *vpodata[MAXVP0];

static struct scsi_adapter vpo_switch =
{
	vpo_scsi_cmd,
	vpominphys,
	0,
	0,
	vpo_adapter_info,
	"vpo",
	{ 0, 0 }
};

/* 
 * The below structure is so we have a default dev struct
 * for out link struct.
 */
static struct scsi_device vpo_dev =
{
	NULL,	/* Use default error handler */
	NULL,	/* have a queue, served by this */
	NULL,	/* have no async handler */
	NULL,	/* Use default 'done' routine */
	"vpo",
	0,
	{ 0, 0 }
};


/*
 * Make ourselves visible as a ppbus driver
 */

static struct ppb_device	*vpoprobe(struct ppb_data *ppb);
static int			vpoattach(struct ppb_device *dev);

static struct ppb_driver vpodriver = {
    vpoprobe, vpoattach, "vpo"
};
DATA_SET(ppbdriver_set, vpodriver);


static u_int32_t
vpo_adapter_info(int unit)
{

	return 1;
}

/*
 * vpoprobe()
 *
 * Called by ppb_attachdevs().
 */
static struct ppb_device *
vpoprobe(struct ppb_data *ppb)
{
	struct vpo_data *vpo;
	struct ppb_device *dev;

	if (nvpo >= MAXVP0) {
		printf("vpo: Too many devices (max %d)\n", MAXVP0);
		return(NULL);
	}

	vpo = (struct vpo_data *)malloc(sizeof(struct vpo_data),
							M_DEVBUF, M_NOWAIT);
	if (!vpo) {
		printf("vpo: cannot malloc!\n");
		return(NULL);
	}
	bzero(vpo, sizeof(struct vpo_data));

	vpodata[nvpo] = vpo;

	/* vpo dependent initialisation */
	vpo->vpo_unit = nvpo;

	/* ok, go to next device on next probe */
	nvpo ++;

	/* low level probe */
	vpoio_set_unit(&vpo->vpo_io, vpo->vpo_unit);

	if ((dev = imm_probe(ppb, &vpo->vpo_io))) {
		vpo->vpo_isplus = 1;

	} else if (!(dev = vpoio_probe(ppb, &vpo->vpo_io))) {
		free(vpo, M_DEVBUF);
		return (NULL);
	}

	return (dev);
}

/*
 * vpoattach()
 *
 * Called by ppb_attachdevs().
 */
static int
vpoattach(struct ppb_device *dev)
{

	struct scsibus_data *scbus;
	struct vpo_data *vpo = vpodata[dev->id_unit];

	/* low level attachment */
	if (vpo->vpo_isplus) {
		if (!imm_attach(&vpo->vpo_io))
			return (0);
	} else {
		if (!vpoio_attach(&vpo->vpo_io))
			return (0);
	}

	vpo->sc_link.adapter_unit = vpo->vpo_unit;
	vpo->sc_link.adapter_targ = VP0_INITIATOR;
	vpo->sc_link.adapter = &vpo_switch;
	vpo->sc_link.device = &vpo_dev;
	vpo->sc_link.opennings = VP0_OPENNINGS;

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus)
		return (0);
	scbus->adapter_link = &vpo->sc_link;

	/* all went ok */
	printf("vpo%d: <Iomega PPA-3/VPI0/IMM SCSI controller>\n",
		dev->id_unit);

	scsi_attachdevs(scbus);

	return (1);
}

static void
vpominphys(struct buf *bp)
{

	if (bp->b_bcount > VP0_BUFFER_SIZE)
		bp->b_bcount = VP0_BUFFER_SIZE;

	return;
}

/*
 * vpo_intr()
 */
static void
vpo_intr(struct vpo_data *vpo, struct scsi_xfer *xs)
{

	int errno;	/* error in errno.h */

	if (xs->datalen && !(xs->flags & SCSI_DATA_IN))
		bcopy(xs->data, vpo->vpo_buffer, xs->datalen);

	if (vpo->vpo_isplus) {
		errno = imm_do_scsi(&vpo->vpo_io, VP0_INITIATOR,
			xs->sc_link->target, (char *)xs->cmd, xs->cmdlen,
			vpo->vpo_buffer, xs->datalen, &vpo->vpo_stat,
			&vpo->vpo_count, &vpo->vpo_error);
	} else {
		errno = vpoio_do_scsi(&vpo->vpo_io, VP0_INITIATOR,
			xs->sc_link->target, (char *)xs->cmd, xs->cmdlen,
			vpo->vpo_buffer, xs->datalen, &vpo->vpo_stat,
			&vpo->vpo_count, &vpo->vpo_error);
	}

#ifdef VP0_DEBUG
	printf("vpo_do_scsi = %d, status = 0x%x, count = %d, vpo_error = %d\n", 
		 errno, vpo->vpo_stat, vpo->vpo_count, vpo->vpo_error);
#endif

	if (errno) {
#ifdef VP0_WARNING
		log(LOG_WARNING, "vpo%d: errno = %d\n", vpo->vpo_unit, errno);
#endif
		/* connection to ppbus interrupted */
		xs->error = XS_DRIVER_STUFFUP;
		goto error;
	}

	/* if a timeout occured, no sense */
	if (vpo->vpo_error) {
		xs->error = XS_TIMEOUT;
		goto error;
	}

#define RESERVED_BITS_MASK 0x3e		/* 00111110b */
#define NO_SENSE	0x0
#define CHECK_CONDITION	0x02

	switch (vpo->vpo_stat & RESERVED_BITS_MASK) {
	case NO_SENSE:
		break;

	case CHECK_CONDITION:
		vpo->vpo_sense.cmd.op_code = REQUEST_SENSE;
		vpo->vpo_sense.cmd.length = sizeof(xs->sense);
		vpo->vpo_sense.cmd.control = 0;

		if (vpo->vpo_isplus) {
			errno = imm_do_scsi(&vpo->vpo_io, VP0_INITIATOR,
				xs->sc_link->target,
				(char *)&vpo->vpo_sense.cmd,
				sizeof(vpo->vpo_sense.cmd),
				(char *)&xs->sense, sizeof(xs->sense),
				&vpo->vpo_sense.stat, &vpo->vpo_sense.count,
				&vpo->vpo_error);
		} else {
			errno = vpoio_do_scsi(&vpo->vpo_io, VP0_INITIATOR,
				xs->sc_link->target,
				(char *)&vpo->vpo_sense.cmd,
				sizeof(vpo->vpo_sense.cmd),
				(char *)&xs->sense, sizeof(xs->sense),
				&vpo->vpo_sense.stat, &vpo->vpo_sense.count,
				&vpo->vpo_error);
		}

		if (errno)
			/* connection to ppbus interrupted */
			xs->error = XS_DRIVER_STUFFUP;
		else
			xs->error = XS_SENSE;

		goto error;

	default:	/* BUSY or RESERVATION_CONFLICT */
		xs->error = XS_TIMEOUT;
		goto error;
	}

	if (xs->datalen && (xs->flags & SCSI_DATA_IN))
		bcopy(vpo->vpo_buffer, xs->data, xs->datalen);

done:
	xs->resid = 0;
	xs->error = XS_NOERROR;

error:
	xs->flags |= ITSDONE;
	scsi_done(xs);

	return;
}

static int32_t
vpo_scsi_cmd(struct scsi_xfer *xs)
{

	int s;

	if (xs->sc_link->lun > 0) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}

	if (xs->flags & SCSI_DATA_UIO) {
		printf("UIO not supported by vpo_driver !\n");
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}

#ifdef VP0_DEBUG
	printf("vpo_scsi_cmd(): xs->flags = 0x%x, "\
		"xs->data = 0x%x, xs->datalen = %d\ncommand : %*D\n",
		xs->flags, xs->data, xs->datalen,
		xs->cmdlen, xs->cmd, " " );
#endif

	if (xs->flags & SCSI_NOMASK) {
		vpo_intr(vpodata[xs->sc_link->adapter_unit], xs);
		return COMPLETE;
	}

	s = splbio();

	vpo_intr(vpodata[xs->sc_link->adapter_unit], xs);

	splx(s);
	return SUCCESSFULLY_QUEUED;
}
