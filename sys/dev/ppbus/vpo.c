/*-
 * Copyright (c) 1997 Nicolas Souchu
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
 *	$Id: vpo.c,v 1.3 1997/08/28 10:15:20 msmith Exp $
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
#include <dev/ppbus/vpo.h>

/* --------------------------------------------------------------------
 * HERE ARE THINGS YOU MAY HAVE/WANT TO CHANGE
 */

/*
 * XXX
 * We may add a timeout queue to avoid active polling on nACK.
 */
#define VP0_SELTMO		5000	/* select timeout */
#define VP0_FAST_SPINTMO	500000	/* wait status timeout */
#define VP0_LOW_SPINTMO		5000000	/* wait status timeout */

/*
 * DO NOT MODIFY ANYTHING UNDER THIS LINE
 * --------------------------------------------------------------------
 */

static inline int vpoio_do_scsi(struct vpo_data *, int, int, char *, int,
				char *, int, int *, int *);

static int32_t	vpo_scsi_cmd(struct scsi_xfer *);
static void	vpominphys(struct buf *);
static u_int32_t vpo_adapter_info(int);

static int	vpo_detect(struct vpo_data *vpo);

static int	nvpo = 0;
#define MAXVP0	8			/* XXX not much better! */
static struct vpo_data *vpodata[MAXVP0];

#ifdef KERNEL
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


#endif /* KERNEL */

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

	/* ppbus dependent initialisation */
	vpo->vpo_dev.id_unit = vpo->vpo_unit;
	vpo->vpo_dev.ppb = ppb;

	/* now, try to initialise the drive */
	if (vpo_detect(vpo)) {
		free(vpo, M_DEVBUF);
		return (NULL);
	}

	/* ok, go to next device on next probe */
	nvpo ++;

	return (&vpo->vpo_dev);
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

	vpo->sc_link.adapter_unit = vpo->vpo_unit;
	vpo->sc_link.adapter_targ = VP0_INITIATOR;
	vpo->sc_link.adapter = &vpo_switch;
	vpo->sc_link.device = &vpo_dev;
	vpo->sc_link.opennings = VP0_OPENNINGS;

	/*
	 * Report ourselves
	 */
	printf("vpo%d: <Adaptec aic7110 scsi> on ppbus %d\n",
	       dev->id_unit, dev->ppb->ppb_link->adapter_unit);

	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if(!scbus)
		return (0);
	scbus->adapter_link = &vpo->sc_link;

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

#ifdef VP0_WARNING
static inline void
vpo_warning(struct vpo_data *vpo, struct scsi_xfer *xs, int timeout)
{

	switch (timeout) {
	case 0:
	case VP0_ESELECT_TIMEOUT:
		/* log(LOG_WARNING,
			"vpo%d: select timeout\n", vpo->vpo_unit); */
		break;
	case VP0_EDISCONNECT:
		log(LOG_WARNING,
			"vpo%d: can't get printer state\n", vpo->vpo_unit);
		break;
	case VP0_ECONNECT:
		log(LOG_WARNING,
			"vpo%d: can't get disk state\n", vpo->vpo_unit);
		break;
	case VP0_ECMD_TIMEOUT:
		log(LOG_WARNING,
			"vpo%d: command timeout\n", vpo->vpo_unit);
		break;
	case VP0_EPPDATA_TIMEOUT:
		log(LOG_WARNING,
			"vpo%d: EPP data timeout\n", vpo->vpo_unit);
		break;
	case VP0_ESTATUS_TIMEOUT:
		log(LOG_WARNING,
			"vpo%d: status timeout\n", vpo->vpo_unit);
		break;
	case VP0_EDATA_OVERFLOW:
		log(LOG_WARNING,
			"vpo%d: data overflow\n", vpo->vpo_unit);
		break;
	case VP0_EINTR:
		log(LOG_WARNING,
			"vpo%d: ppb request interrupted\n", vpo->vpo_unit);
		break;
	default:
		log(LOG_WARNING,
			"vpo%d: timeout = %d\n", vpo->vpo_unit, timeout);
		break;
	}
}
#endif /* VP0_WARNING */

/*
 * vpointr()
 */
static inline void
vpointr(struct vpo_data *vpo, struct scsi_xfer *xs)
{

	int errno;	/* error in errno.h */

	if (xs->datalen && !(xs->flags & SCSI_DATA_IN))
		bcopy(xs->data, vpo->vpo_buffer, xs->datalen);

	errno = vpoio_do_scsi(vpo, VP0_INITIATOR,
		xs->sc_link->target, (char *)xs->cmd, xs->cmdlen,
		vpo->vpo_buffer, xs->datalen, &vpo->vpo_stat, &vpo->vpo_count);

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
#ifdef VP0_WARNING
		vpo_warning(vpo, xs, vpo->vpo_error);
#endif
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

		errno = vpoio_do_scsi(vpo, VP0_INITIATOR,
			xs->sc_link->target, (char *)&vpo->vpo_sense.cmd,
			sizeof(vpo->vpo_sense.cmd),
			(char *)&xs->sense, sizeof(xs->sense),
			&vpo->vpo_sense.stat, &vpo->vpo_sense.count);

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
		vpointr(vpodata[xs->sc_link->adapter_unit], xs);
		return COMPLETE;
	}

	s = VP0_SPL();

	vpointr(vpodata[xs->sc_link->adapter_unit], xs);

	splx(s);
	return SUCCESSFULLY_QUEUED;
}

#define vpoio_d_pulse(vpo,b) { \
	ppb_wdtr(&(vpo)->vpo_dev, b); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev, H_nAUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev, H_nAUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev, H_nAUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO |  H_SELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO |  H_SELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO |  H_SELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE); \
}

#define vpoio_c_pulse(vpo,b) { \
	ppb_wdtr(&(vpo)->vpo_dev, b); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO |  H_SELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev, H_nAUTO |  H_SELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev, H_nAUTO |  H_SELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev, H_nAUTO |  H_SELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO |  H_SELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE); \
	ppb_wctr(&(vpo)->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE); \
}

static int
vpoio_disconnect(struct vpo_data *vpo)
{

	vpoio_d_pulse(vpo, 0);
	vpoio_d_pulse(vpo, 0x3c);
	vpoio_d_pulse(vpo, 0x20);
	vpoio_d_pulse(vpo, 0xf);

	return (ppb_release_bus(&vpo->vpo_dev));
}

/*
 * how	: PPB_WAIT or PPB_DONTWAIT
 */
static int
vpoio_connect(struct vpo_data *vpo, int how)
{
	int error;

	if ((error = ppb_request_bus(&vpo->vpo_dev, how)))
		return error;

	vpoio_c_pulse(vpo, 0);
	vpoio_c_pulse(vpo, 0x3c);
	vpoio_c_pulse(vpo, 0x20);

	if (PPB_IN_EPP_MODE(&vpo->vpo_dev)) {
		vpoio_c_pulse(vpo, 0xcf);
	} else {
		vpoio_c_pulse(vpo, 0x8f);
	}

	return (0);
}

/*
 * vpoio_in_disk_mode()
 *
 * Check if we are in disk mode
 */
static int
vpoio_in_disk_mode(struct vpo_data *vpo)
{

	/* first, set H_AUTO high */
	ppb_wctr(&vpo->vpo_dev, H_AUTO | H_nSELIN | H_INIT | H_STROBE);

	/* when H_AUTO is set low, H_FLT should be high */
	ppb_wctr(&vpo->vpo_dev, H_nAUTO | H_nSELIN | H_INIT | H_STROBE);
	if ((ppb_rstr(&vpo->vpo_dev) & H_FLT) == 0)
		return (0);

	/* when H_AUTO is set high, H_FLT should be low */
	ppb_wctr(&vpo->vpo_dev, H_AUTO | H_nSELIN | H_INIT | H_STROBE);
	if ((ppb_rstr(&vpo->vpo_dev) & H_FLT) != 0)
		return (0);

	return (1);
}

/*
 * vpoio_reset()
 *
 * SCSI reset signal, the drive must be in disk mode
 */
static void
vpoio_reset (struct vpo_data *vpo)
{

	/*
	 * SCSI reset signal.
	 */
	ppb_wdtr(&vpo->vpo_dev, (1 << 7));
	ppb_wctr(&vpo->vpo_dev, H_AUTO | H_nSELIN | H_nINIT | H_STROBE);
	DELAY(25);
	ppb_wctr(&vpo->vpo_dev, H_AUTO | H_nSELIN |  H_INIT | H_STROBE);

	return;
}


/*
 * vpo_detect()
 *
 * Detect and initialise the VP0 adapter.
 */
static int
vpo_detect(struct vpo_data *vpo)
{

	vpoio_disconnect(vpo);
	vpoio_connect(vpo, PPB_DONTWAIT);

	if (!vpoio_in_disk_mode(vpo)) {
		vpoio_disconnect(vpo);
		return (VP0_EINITFAILED);
	}

	/* send SCSI reset signal */
	vpoio_reset (vpo);

	vpoio_disconnect(vpo);

	if (vpoio_in_disk_mode(vpo))
		return (VP0_EINITFAILED);

	return (0);
}

#define vpo_wctr(dev,byte,delay) {			 \
	int i; int iter = delay / MHZ_16_IO_DURATION;	 \
	for (i = 0; i < iter; i++) {			 \
		ppb_wctr(dev, byte);			 \
	}						 \
}

#define vpoio_spp_outbyte(vpo,byte) {					 \
	ppb_wdtr(&vpo->vpo_dev, byte);					 \
	ppb_wctr(&vpo->vpo_dev, H_nAUTO | H_nSELIN | H_INIT | H_STROBE); \
	vpo_wctr(&vpo->vpo_dev,  H_AUTO | H_nSELIN | H_INIT | H_STROBE,	 \
		VP0_SPP_WRITE_PULSE);					 \
}

#define vpoio_nibble_inbyte(vpo,buffer) {				\
	register char h, l;						\
	vpo_wctr(&vpo->vpo_dev,  H_AUTO | H_SELIN | H_INIT | H_STROBE,	\
		VP0_NIBBLE_READ_PULSE);					\
	h = ppb_rstr(&vpo->vpo_dev);					\
	ppb_wctr(&vpo->vpo_dev, H_nAUTO | H_SELIN | H_INIT | H_STROBE);	\
	l = ppb_rstr(&vpo->vpo_dev);					\
	*buffer = ((l >> 4) & 0x0f) + (h & 0xf0);			\
}

#define vpoio_ps2_inbyte(vpo,buffer) {					\
	*buffer = ppb_rdtr(&vpo->vpo_dev);				\
	ppb_wctr(&vpo->vpo_dev, PCD | H_nAUTO | H_SELIN | H_INIT | H_nSTROBE); \
	ppb_wctr(&vpo->vpo_dev, PCD |  H_AUTO | H_SELIN | H_INIT | H_nSTROBE); \
}

/*
 * vpoio_outstr()
 */
static int
vpoio_outstr(struct vpo_data *vpo, char *buffer, int size)
{

	register int k;
	int error = 0;
	int r, mode, epp;

	mode = ppb_get_mode(&vpo->vpo_dev);
	switch (mode) {
		case PPB_NIBBLE:
		case PPB_PS2:
			for (k = 0; k < size; k++) {
				vpoio_spp_outbyte(vpo, *buffer++);
			}
			break;

		case PPB_EPP:
		case PPB_ECP_EPP:
			epp = ppb_get_epp_protocol(&vpo->vpo_dev);

			ppb_reset_epp_timeout(&vpo->vpo_dev);
			ppb_wctr(&vpo->vpo_dev,
				H_AUTO | H_SELIN | H_INIT | H_STROBE);

			if (epp == EPP_1_7)
				for (k = 0; k < size; k++) {
					ppb_wepp(&vpo->vpo_dev, *buffer++);
					if ((ppb_rstr(&vpo->vpo_dev) & TIMEOUT)) {
						error = VP0_EPPDATA_TIMEOUT;
						break;
					}
				}
			else {
				if (((long) buffer | size) & 0x03)
					ppb_outsb_epp(&vpo->vpo_dev,
							buffer, size);
				else
					ppb_outsl_epp(&vpo->vpo_dev,
							buffer, size/4);

				if ((ppb_rstr(&vpo->vpo_dev) & TIMEOUT)) {
					error = VP0_EPPDATA_TIMEOUT;
					break;
				}
			}
			ppb_wctr(&vpo->vpo_dev,
				H_AUTO | H_nSELIN | H_INIT | H_STROBE);
			/* ppb_ecp_sync(&vpo->vpo_dev); */
			break;

		default:
			printf("vpoio_outstr(): unknown transfer mode (%d)!\n",
				mode);
			return (1);		/* XXX */
	}

	return (error);
}

/*
 * vpoio_instr()
 */
static int
vpoio_instr(struct vpo_data *vpo, char *buffer, int size)
{

	register int k;
	int error = 0;
	int r, mode, epp;

	mode = ppb_get_mode(&vpo->vpo_dev);
	switch (mode) {
		case PPB_NIBBLE:
			for (k = 0; k < size; k++) {
				vpoio_nibble_inbyte(vpo, buffer++);
			}
			ppb_wctr(&vpo->vpo_dev,
				H_AUTO | H_nSELIN | H_INIT | H_STROBE);
			break;

		case PPB_PS2:
			ppb_wctr(&vpo->vpo_dev, PCD |
				H_AUTO | H_SELIN | H_INIT | H_nSTROBE);

			for (k = 0; k < size; k++) {
				vpoio_ps2_inbyte(vpo, buffer++);
			}
			ppb_wctr(&vpo->vpo_dev,
				H_AUTO | H_nSELIN | H_INIT | H_STROBE);
			break;

		case PPB_EPP:
		case PPB_ECP_EPP:
			epp = ppb_get_epp_protocol(&vpo->vpo_dev);

			ppb_reset_epp_timeout(&vpo->vpo_dev);
			ppb_wctr(&vpo->vpo_dev, PCD |
				H_AUTO | H_SELIN | H_INIT | H_STROBE);

			if (epp == EPP_1_7)
				for (k = 0; k < size; k++) {
					*buffer++ = ppb_repp(&vpo->vpo_dev);
					if ((ppb_rstr(&vpo->vpo_dev) & TIMEOUT)) {
						error = VP0_EPPDATA_TIMEOUT;
						break;
					}
				}
			else {
				if (((long) buffer | size) & 0x03)
					ppb_insb_epp(&vpo->vpo_dev,
							buffer, size);
				else
					ppb_insl_epp(&vpo->vpo_dev,
							buffer, size/4);

				if ((ppb_rstr(&vpo->vpo_dev) & TIMEOUT)) {
					error = VP0_EPPDATA_TIMEOUT;
					break;
				}
			}
			ppb_wctr(&vpo->vpo_dev, PCD |
				H_AUTO | H_nSELIN | H_INIT | H_STROBE);
			/* ppb_ecp_sync(&vpo->vpo_dev); */
			break;

		default:
			printf("vpoio_instr(): unknown transfer mode (%d)!\n",
				mode);
			return (1);		/* XXX */
	}

	return (error);
}

static inline char
vpoio_select(struct vpo_data *vpo, int initiator, int target)
{

	register int	k;

	ppb_wdtr(&vpo->vpo_dev, (1 << target));
	ppb_wctr(&vpo->vpo_dev, H_nAUTO | H_nSELIN |  H_INIT | H_STROBE);
	ppb_wctr(&vpo->vpo_dev,  H_AUTO | H_nSELIN |  H_INIT | H_STROBE);
	ppb_wdtr(&vpo->vpo_dev, (1 << initiator));
	ppb_wctr(&vpo->vpo_dev,  H_AUTO | H_nSELIN | H_nINIT | H_STROBE);

	k = 0;
	while (!(ppb_rstr(&vpo->vpo_dev) & 0x40) && (k++ < VP0_SELTMO))
		barrier();

	if (k >= VP0_SELTMO)
		return (VP0_ESELECT_TIMEOUT);

	return (0);
}

/*
 * vpoio_wait()
 *
 * H_SELIN must be low.
 */
static inline char
vpoio_wait(struct vpo_data *vpo, int tmo)
{

	register int	k;
	register char	r;

#if 0	/* broken */
	if (ppb_poll_device(&vpo->vpo_dev, 150, nBUSY, nBUSY, PPB_INTR))
		return (0);

	return (ppb_rstr(&vpo->vpo_dev) & 0xf0);
#endif

	k = 0;
	while (!((r = ppb_rstr(&vpo->vpo_dev)) & nBUSY) && (k++ < tmo))
		barrier();

	/*
	 * Return some status information.
	 * Semantics :	0xc0 = ZIP wants more data
	 *		0xd0 = ZIP wants to send more data
	 *		0xe0 = ZIP wants command
	 *		0xf0 = end of transfer, ZIP is sending status
	 */
	if (k < tmo)
	  return (r & 0xf0);

	return (0);			   /* command timed out */	
}

static inline int 
vpoio_do_scsi(struct vpo_data *vpo, int host, int target, char *command,
		int clen, char *buffer, int blen, int *result, int *count)
{

	register char r;
	char l, h = 0;
	int rw, len, error = 0;
	register int k;

	/*
	 * enter disk state, allocate the ppbus
	 *
	 * XXX
	 * Should we allow this call to be interruptible?
	 * The only way to report the interruption is to return
	 * EIO do upper SCSI code :^(
	 */
	if ((error = vpoio_connect(vpo, PPB_WAIT|PPB_INTR)))
		return (error);

	if (!vpoio_in_disk_mode(vpo)) {
		vpo->vpo_error = VP0_ECONNECT; goto error;
	}

	if ((vpo->vpo_error = vpoio_select(vpo,host,target)))
		goto error;

	/*
	 * Send the command ...
	 *
	 * set H_SELIN low for vpoio_wait().
	 */
	ppb_wctr(&vpo->vpo_dev, H_AUTO | H_nSELIN | H_INIT | H_STROBE);

#ifdef VP0_DEBUG
	printf("vpo%d: drive selected, now sending the command...\n",
		vpo->vpo_unit);
#endif

	for (k = 0; k < clen; k++) {
		if (vpoio_wait(vpo, VP0_FAST_SPINTMO) != (char)0xe0) {
			vpo->vpo_error = VP0_ECMD_TIMEOUT;
			goto error;
		}
		if (vpoio_outstr(vpo, &command[k], 1)) {
			vpo->vpo_error = VP0_EPPDATA_TIMEOUT;
			goto error;
		}
	}

#ifdef VP0_DEBUG
	printf("vpo%d: command sent, now completing the request...\n",
		vpo->vpo_unit);
#endif

	/* 
	 * Completion ... 
	 */
	rw = ((command[0] == READ_COMMAND) || (command[0] == READ_BIG) ||
		(command[0] == WRITE_COMMAND) || (command[0] == WRITE_BIG));

	*count = 0;
	for (;;) {

		if (!(r = vpoio_wait(vpo, VP0_LOW_SPINTMO))) {
			vpo->vpo_error = VP0_ESTATUS_TIMEOUT; goto error;
		}

		/* stop when the ZIP wants to send status */
		if (r == (char)0xf0)
			break;

		if (*count >= blen) {
			vpo->vpo_error = VP0_EDATA_OVERFLOW;
			goto error;
		}
		len = (rw && ((blen - *count) >= VP0_SECTOR_SIZE)) ?
			VP0_SECTOR_SIZE : 1;

		/* ZIP wants to send data? */
		if (r == (char)0xc0)
			error = vpoio_outstr(vpo, &buffer[*count], len);
		else
			error = vpoio_instr(vpo, &buffer[*count], len);

		if (error) {
			vpo->vpo_error = error;
			goto error;
		}

		*count += len;
	}

	if (vpoio_instr(vpo, &l, 1)) {
		vpo->vpo_error = VP0_EOTHER; goto error;
	}

	/* check if the ZIP wants to send more status */
	if (vpoio_wait(vpo, VP0_FAST_SPINTMO) == (char)0xf0)
		if (vpoio_instr(vpo, &h, 1)) {
			vpo->vpo_error = VP0_EOTHER+2; goto error;
		}

	*result = ((int) h << 8) | ((int) l & 0xff);

error:
	/* return to printer state, release the ppbus */
	vpoio_disconnect(vpo);
	return (0);
}
