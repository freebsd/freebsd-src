/*
 * Connectix QuickCam parallel-port camera video capture driver.
 * Copyright (c) 1996, Paul Traina.
 *
 * This driver is based in part on work
 * Copyright (c) 1996, Thomas Davis.
 *
 * QuickCam(TM) is a registered trademark of Connectix Inc.
 * Use this driver at your own risk, it is not warranted by
 * Connectix or the authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include	"qcam.h"
#if NQCAM > 0

#include	<sys/param.h>
#include	<sys/systm.h>
#include	<sys/kernel.h>
#include	<sys/conf.h>
#include	<sys/ioctl.h>
#include	<sys/uio.h>
#include	<sys/malloc.h>
#include	<sys/devconf.h>
#include	<sys/errno.h>

#include	<machine/clock.h>
#include	<machine/qcam.h>

#include	<i386/isa/qcamreg.h>
#include	<i386/isa/qcamdefs.h>
#include	<i386/isa/isa.h>
#include	<i386/isa/isa_device.h>

#define	STATIC_CDEVSW

int	qcam_debug = 1;

static struct qcam_softc qcam_softc[NQCAM];

#define	QC_CONF_NODETECT 0x01		/* always assume camera is present */
#define	QC_CONF_FORCEUNI 0x02		/* force unidirectional transfers */

static struct kern_devconf kdc_qcam_template = {
	0, 0, 0,			/* filled in by dev_attach() */
	"qcam",				/* kdc_name */
	0, 				/* kdc_unit */
	{ 				/* kdc_md */
  	   MDDT_ISA,			/* mddc_devtype */
	   0,				/* mddc_flags */
	   "tty"			/* mddc_imask[4] */
	},
	isa_generic_externalize,	/* kdc_externalize */
	0,				/* kdc_internalize */
	0,				/* kdc_goaway */
	ISA_EXTERNALLEN,		/* kdc_datalen */
	&kdc_isa0,			/* kdc_parent */
	0,				/* kdc_parentdata */
	DC_UNCONFIGURED,		/* kdc_state */
	"QuickCam video input", 	/* kdc_description */
	DC_CLS_MISC			/* class */
};

#define	UNIT(dev)		minor(dev)

static void
qcam_registerdev (struct isa_device *id)
{
	struct kern_devconf *kdc = &qcam_softc[id->id_unit].kdc;

	*kdc = kdc_qcam_template;		/* byte-copy template */

	kdc->kdc_unit = id->id_unit;
	kdc->kdc_parentdata = id;
}

static int
qcam_probe (struct isa_device *devp)
{
	u_char reg, last;
	int i, transitions = 0;

	switch (devp->id_iobase) {	/* don't probe weird ports */
	case IO_LPT1:
	case IO_LPT2:
	case IO_LPT3:
		break;
	default:
		printf("qcam%d: ignoring non-standard port 0x%x\n",
		       devp->id_unit, devp->id_iobase);
		return 0;
	}

	/*
	 * XXX The probe code is reported to be flakey.
	 *     We need to work on this some more, so temporarily,
	 *     allow bit one of the "flags" parameter to bypass this
	 *     check.
	 */

	if (!(devp->id_flags & QC_CONF_NODETECT))
	    if (!qcam_detect(devp->id_iobase))
		return 0;	/* failure */

	qcam_registerdev(devp);
	return 1;		/* found */
}

static int
qcam_attach (struct isa_device *devp)
{
	struct	qcam_softc *qs = &qcam_softc[devp->id_unit];

	qs->iobase	 = devp->id_iobase;
	qs->unit	 = devp->id_unit;
	qs->kdc.kdc_state = DC_IDLE;
	qs->flags |= QC_ALIVE;

	/* force unidirectional parallel port mode? */
	if (devp->id_flags & QC_CONF_FORCEUNI)
		qs->flags |= QC_FORCEUNI;

	qcam_reset(qs);

	printf("qcam%d: %sdirectional parallel port\n",
	       qs->unit, qs->flags & QC_BIDIR_HW ? "bi" : "uni");

	return 1;
}

struct isa_driver	qcamdriver =
			{qcam_probe, qcam_attach, "qcam"};

STATIC_CDEVSW int
qcam_open (dev_t dev, int flags, int fmt, struct proc *p)
{
	struct qcam_softc *qs = &qcam_softc[UNIT(dev)];

	if (!(qs->flags & QC_ALIVE))
		return ENXIO;

	if (qs->flags & QC_OPEN)
		return EBUSY;

	qs->buffer_end = qs->buffer = malloc(QC_MAXFRAMEBUFSIZE, M_DEVBUF,
					     M_WAITOK);
	if (!qs->buffer)
		return ENOMEM;

	qcam_reset(qs);
	qcam_default(qs);
	qs->init_req = 1;	/* request initialization before scan */

	qs->flags |= QC_OPEN;
	qs->kdc.kdc_state = DC_BUSY;

	return 0;
}

STATIC_CDEVSW int
qcam_close (dev_t dev, int flags, int fmt, struct proc *p)
{
	struct qcam_softc *qs = &qcam_softc[UNIT(dev)];

	if (qs->buffer) {
	    free(qs->buffer, M_DEVBUF);
	    qs->buffer = NULL;
	    qs->buffer_end = NULL;
	}

	qs->flags &= ~QC_OPEN;
	qs->kdc.kdc_state = DC_IDLE;
	return 0;
}

STATIC_CDEVSW int
qcam_read (dev_t dev, struct uio *uio, int ioflag)
{
	struct qcam_softc *qs = &qcam_softc[UNIT(dev)];
	int bytes, bufsize;
	int error;

	/* if we've seeked back to 0, that's our signal to scan */
	if (uio->uio_offset == 0)
		if (qcam_scan(qs))
			return EIO;

	bufsize = qs->buffer_end - qs->buffer;
	if (uio->uio_offset > bufsize)
		return EIO;

	bytes = min(uio->uio_resid, (bufsize - uio->uio_offset));
	error = uiomove(qs->buffer + uio->uio_offset, bytes, uio);
	if (error)
		return error;

	return 0;		/* success */
}

STATIC_CDEVSW int
qcam_ioctl (dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	struct qcam_softc *qs = &qcam_softc[UNIT(dev)];
	struct qcam      *info = (struct qcam *)data;

	if (!data)
		return(EINVAL);

	switch (cmd) {
	case QC_GET:
		return qcam_ioctl_get(qs, info) ? 0 : EINVAL;

	case QC_SET:
		return qcam_ioctl_set(qs, info) ? 0 : EINVAL;

	default:
		return(ENOTTY);
	}

	return 0;
}

#endif /* NQCAM */
