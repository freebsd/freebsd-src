/* $FreeBSD$ */
/*
 * Copyright (c) 2000 Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/dkbad.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/devicestat.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_extend.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_queue.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <sys/ioccom.h>
#include <cam/scsi/scsi_ses.h>

#include <opt_ses.h>

#define	QFRLS(ccb)	\
	if (((ccb)->ccb_h.status & CAM_DEV_QFRZN) != 0)	\
		cam_release_devq((ccb)->ccb_h.path, 0, 0, 0, FALSE)

#define ccb_state	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct ses_softc {
	enctyp		ses_type;	/* type of enclosure */
	encvec		ses_vec;	/* vector to handlers */
	u_int32_t	ses_nobjects;	/* number of objects */
	void *		ses_private;	/* private data */
	encobj *	ses_objmap;	/* objects */
	u_int8_t	ses_encstat;	/* overall status */
	u_int8_t	ses_flags;
	union ccb	ses_saved_ccb;
	dev_t		ses_dev;
};
#define	SES_FLAG_INVALID	0x01
#define	SES_FLAG_OPEN		0x02

#define SESUNIT(x)       (minor((x)))
#define SES_CDEV_MAJOR	110

typedef enum {
        SES_CCB_BUFFER_IO,
        SES_CCB_WAITING
} ses_ccb_types;
#define	ccb_type	ppriv_field0

static	d_open_t	sesopen;
static	d_close_t	sesclose;
static	d_ioctl_t	sesioctl;
static	periph_init_t	sesinit;
static  periph_ctor_t	sesregister;
static	periph_oninv_t	sesoninvalidate;
static  periph_dtor_t   sescleanup;
static  periph_start_t  sesstart;

static void sesasync(void *, u_int32_t, struct cam_path *, void *);
static void sesdone(struct cam_periph *, union ccb *);
static int seserror(union ccb *, u_int32_t, u_int32_t);

static struct periph_driver sesdriver = {
	sesinit, "ses",
	TAILQ_HEAD_INITIALIZER(sesdriver.units), /* generation */ 0
};

DATA_SET(periphdriver_set, sesdriver);

static struct cdevsw ses_cdevsw = 
{
	/* open */	sesopen,
	/* close */	sesclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	sesioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ses",
	/* maj */	SES_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};
static struct extend_array *sesperiphs;

void
sesinit(void)
{
	cam_status status;
	struct cam_path *path;

	/*
	 * Create our extend array for storing the devices we attach to.
	 */
	sesperiphs = cam_extend_new();
	if (sesperiphs == NULL) {
		printf("ses: Failed to alloc extend array!\n");
		return;
	}

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_create_path(&path, NULL, CAM_XPT_PATH_ID,
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);

	if (status == CAM_REQ_CMP) {
		struct ccb_setasync csa;

                xpt_setup_ccb(&csa.ccb_h, path, 5);
                csa.ccb_h.func_code = XPT_SASYNC_CB;
                csa.event_enable = AC_FOUND_DEVICE;
                csa.callback = sesasync;
                csa.callback_arg = NULL;
                xpt_action((union ccb *)&csa);
		status = csa.ccb_h.status;
                xpt_free_path(path);
        }

	if (status != CAM_REQ_CMP) {
		printf("ses: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}
}

static void
sesoninvalidate(struct cam_periph *periph)
{
	struct ses_softc *softc;
	struct ccb_setasync csa;

	softc = (struct ses_softc *)periph->softc;

	/*
	 * Unregister any async callbacks.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = 0;
	csa.callback = sesasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);

	softc->ses_flags |= SES_FLAG_INVALID;

	xpt_print_path(periph->path);
	printf("lost device\n");
}

static void
sescleanup(struct cam_periph *periph)
{
	struct ses_softc *softc;

	softc = (struct ses_softc *)periph->softc;

	destroy_dev(softc->ses_dev);

	cam_extend_release(sesperiphs, periph->unit_number);
	xpt_print_path(periph->path);
	printf("removing device entry\n");
	free(softc, M_DEVBUF);
}

static void
sesasync(void *callback_arg, u_int32_t code, struct cam_path *path, void *arg)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)callback_arg;

	switch(code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		cam_status status;

		cgd = (struct ccb_getdev *)arg;
		/*
		 * PROBLEM: WE NEED TO LOOK AT BYTES 48-53 TO SEE IF THIS IS
		 * PROBLEM: IS A SAF-TE DEVICE.
		 */

		switch (ses_type(&cgd->inq_data, sizeof (cgd->inq_data))) {
		case SES_SES:
		case SES_SEN:
		case SES_SAFT:
			break;
		default:
			return;
		}

		status = cam_periph_alloc(sesregister, sesoninvalidate,
		    sescleanup, sesstart, "ses", CAM_PERIPH_BIO,
		    cgd->ccb_h.path, sesasync, AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP && status != CAM_REQ_INPROG) {
			printf("sesasync: Unable to probe new device due to "
			    "status 0x%x\n", status);
		}
		break;
	}
	default:
		cam_periph_async(periph, code, path, arg);
		break;
	}
}

static cam_status
sesregister(struct cam_periph *periph, void *arg)
{
	struct ses_softc *softc;
	struct ccb_setasync csa;
	struct ccb_getdev *cgd;
	char *tname;

	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("chregister: periph was NULL!!\n");
		return (CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("chregister: no getdev CCB, can't register device\n");
		return (CAM_REQ_CMP_ERR);
	}

	softc = malloc(sizeof (struct ses_softc), M_DEVBUF, M_NOWAIT);
	if (softc == NULL) {
		printf("sesregister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return (CAM_REQ_CMP_ERR);
	}
	bzero(softc, sizeof (struct ses_softc));


	softc->ses_type = ses_type(&cgd->inq_data, sizeof (cgd->inq_data));

	switch (softc->ses_type) {
	case SES_SES_SCSI2:
	case SES_SES:
        case SES_SES_PASSTHROUGH:
        case SES_SEN:
        case SES_SAFT:
		break;
	case SES_NONE:
	default:
		free(softc, M_DEVBUF);
		return (CAM_REQ_CMP_ERR);
	}

	periph->softc = softc;
	cam_extend_set(sesperiphs, periph->unit_number, periph);

	softc->ses_dev = make_dev(&ses_cdevsw, periph->unit_number,
	    UID_ROOT, GID_OPERATOR, 0600, "%s%d",
	    periph->periph_name, periph->unit_number);

	/*
	 * Add an async callback so that we get
	 * notified if this device goes away.
	 */
	xpt_setup_ccb(&csa.ccb_h, periph->path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = sesasync;
	csa.callback_arg = periph;
	xpt_action((union ccb *)&csa);

	/*
	 * Lock this peripheral until we are setup.
	 * This first call can't block
	 */
	(void)cam_periph_lock(periph, PRIBIO);
	xpt_schedule(periph, 5);

	switch (softc->ses_type) {
	default:
	case SES_NONE:
		tname = "No SES device";
		break;
	case SES_SES_SCSI2:
		tname = "SCSI-2 SES Device";
		break;
	case SES_SES:
		tname = "SCSI-3 SES Device";
		break;
        case SES_SES_PASSTHROUGH:
		tname = "SES Passthrough Device";
		break;
        case SES_SEN:
		tname = "Unisys SEN Device";
		break;
        case SES_SAFT:
		tname = "SAF-TE Compliant Device";
		break;
	}
	xpt_announce_periph(periph, tname);
	return (CAM_REQ_CMP);
}

static int
sesopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct cam_periph *periph;
	struct ses_softc *softc;
	int error, s;

	periph = cam_extend_get(sesperiphs, SESUNIT(dev));
	if (periph == NULL)
		return (ENXIO);

	softc = (struct ses_softc *)periph->softc;

	s = splsoftcam();
	if (softc->ses_flags & SES_FLAG_INVALID) {
		splx(s);
		return (ENXIO);
	}
	if ((error = cam_periph_lock(periph, PRIBIO | PCATCH)) != 0) {
		splx(s);
		return (error);
	}
	splx(s);
	if ((softc->ses_flags & SES_FLAG_OPEN) == 0) {
		if (cam_periph_acquire(periph) != CAM_REQ_CMP)
			return (ENXIO);
		softc->ses_flags |= SES_FLAG_OPEN;
	}
	cam_periph_unlock(periph);
	return (error);
}

static int
sesclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct cam_periph *periph;
	struct ses_softc *softc;
	int unit, error;

	error = 0;

	unit = SESUNIT(dev);
	periph = cam_extend_get(sesperiphs, unit);
	if (periph == NULL)
		return (ENXIO);

	softc = (struct ses_softc *)periph->softc;

	if ((error = cam_periph_lock(periph, PRIBIO)) != 0)
		return (error);

	softc->ses_flags &= ~SES_FLAG_OPEN;

	cam_periph_unlock(periph);
	cam_periph_release(periph);

	return (0);
}

static void
sesstart(struct cam_periph *periph, union ccb *start_ccb)
{
	int s;

	s = splbio();
	if (periph->immediate_priority <= periph->pinfo.priority) {
		start_ccb->ccb_h.ccb_type = SES_CCB_WAITING;

		SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
		    periph_links.sle);
		periph->immediate_priority = CAM_PRIORITY_NONE;
		splx(s);
		wakeup(&periph->ccb_list);
	} else {
		splx(s);
	}
}

static void
sesdone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct ses_softc *softc;
	struct ccb_scsiio *csio;

	softc = (struct ses_softc *)periph->softc;
	csio = &done_ccb->csio;

	switch (done_ccb->ccb_h.ccb_type) {
	case SES_CCB_WAITING:
	{
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}
	default:
		break;
	}
	xpt_release_ccb(done_ccb);
}

static int
seserror(union ccb *ccb, u_int32_t cflags, u_int32_t sflags)
{
	struct ses_softc *softc;
	struct cam_periph *periph;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct ses_softc *)periph->softc;

	return (cam_periph_error(ccb, cflags, sflags, &softc->ses_saved_ccb));
}

static int
sesioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct cam_periph *periph;
	struct ses_softc *softc;
	u_int8_t unit;
	int error;

	unit = SESUNIT(dev);

	periph = cam_extend_get(sesperiphs, unit);
	if (periph == NULL)
		return (ENXIO);

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("entering sesioctl\n"));

	softc = (struct ses_softc *)periph->softc;

	error = 0;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
	    ("trying to do ioctl %#lx\n", cmd));

	/*
	 * If this command can change the device's state, we must
	 * have the device open for writing.
	 */
	switch (cmd) {
	case SESIOC_GETNOBJ:
	case SESIOC_GETOBJMAP:
	case SESIOC_GETENCSTAT:
	case SESIOC_GETOBJSTAT:
		break;
	default:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	switch (cmd) {
	case SESIOC_GETNOBJ:
	case SESIOC_GETOBJMAP:
	case SESIOC_GETENCSTAT:
	case SESIOC_SETENCSTAT:
	case SESIOC_GETOBJSTAT:
	case SESIOC_SETOBJSTAT:
	case SESIOC_INIT:
		error = EINVAL;
		break;
	default:
		error = cam_periph_ioctl(periph, cmd, addr, seserror);
		break;
	}
	return (error);
}

/*
 * Is this a device that supports enclosure services?
 *
 * It's a a pretty simple ruleset- if it is device type 0x0D (13), it's
 * an SES device. If it happens to be an old UNISYS SEN device, we can
 * handle that too.
 */

enctyp
ses_type(void *buf, int buflen)
{
	unsigned char *iqd = buf;

	if (buflen < 32)
		return (SES_NONE);

	if ((iqd[0] & 0x1f) == SES_DEVICE_TYPE) {
		if (strncmp(&iqd[8], SEN_ID, SEN_ID_LEN) == 0) {
			return (SES_SEN);
		} else if ((iqd[2] & 0x7) > 2) {
			return (SES_SES);
		} else {
			return (SES_SES_SCSI2);
		}
		return (SES_NONE);
	}

#ifdef	SES_ENABLE_PASSTHROUGH
	if ((iqd[6] & 0x40) && (iqd[2] & 0x7) >= 2) {
		/*
		 * PassThrough Device.
		 */
		return (SES_SES_PASSTHRU);
	}
#endif

	if (buflen < 47) {
		return (SES_NONE);
	}
	/*
	 * The comparison is short for a reason- some vendors were chopping
	 * it short.
	 */
	if (strncmp((char *)&iqd[44], "SAF-TE", 4) == 0) {
		return (SES_SAFT);
	}
	return (SES_NONE);
}
