/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <machine/stdarg.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <sys/ioccom.h>
#include <cam/scsi/scsi_ses.h>

#include <opt_ses.h>

MALLOC_DEFINE(M_SCSISES, "SCSI SES", "SCSI SES buffers");

/*
 * Platform Independent Driver Internal Definitions for SES devices.
 */
typedef enum {
	SES_NONE,
	SES_SES_SCSI2,
	SES_SES,
	SES_SES_PASSTHROUGH,
	SES_SEN,
	SES_SAFT
} enctyp;

struct ses_softc;
typedef struct ses_softc ses_softc_t;
typedef struct {
	int (*softc_init)(ses_softc_t *, int);
	int (*init_enc)(ses_softc_t *);
	int (*get_encstat)(ses_softc_t *, int);
	int (*set_encstat)(ses_softc_t *, ses_encstat, int);
	int (*get_objstat)(ses_softc_t *, ses_objstat *, int);
	int (*set_objstat)(ses_softc_t *, ses_objstat *, int);
} encvec;

#define	ENCI_SVALID	0x80

typedef struct {
	uint32_t
		enctype	: 8,		/* enclosure type */
		subenclosure : 8,	/* subenclosure id */
		svalid	: 1,		/* enclosure information valid */
		priv	: 15;		/* private data, per object */
	uint8_t	encstat[4];	/* state && stats */
} encobj;

#define	SEN_ID		"UNISYS           SUN_SEN"
#define	SEN_ID_LEN	24


static enctyp ses_type(void *, int);


/* Forward reference to Enclosure Functions */
static int ses_softc_init(ses_softc_t *, int);
static int ses_init_enc(ses_softc_t *);
static int ses_get_encstat(ses_softc_t *, int);
static int ses_set_encstat(ses_softc_t *, uint8_t, int);
static int ses_get_objstat(ses_softc_t *, ses_objstat *, int);
static int ses_set_objstat(ses_softc_t *, ses_objstat *, int);

static int safte_softc_init(ses_softc_t *, int);
static int safte_init_enc(ses_softc_t *);
static int safte_get_encstat(ses_softc_t *, int);
static int safte_set_encstat(ses_softc_t *, uint8_t, int);
static int safte_get_objstat(ses_softc_t *, ses_objstat *, int);
static int safte_set_objstat(ses_softc_t *, ses_objstat *, int);

/*
 * Platform implementation defines/functions for SES internal kernel stuff
 */

#define	STRNCMP			strncmp
#define	PRINTF			printf
#define	SES_LOG			ses_log
#ifdef	DEBUG
#define	SES_DLOG		ses_log
#else
#define	SES_DLOG		if (0) ses_log
#endif
#define	SES_VLOG		if (bootverbose) ses_log
#define	SES_MALLOC(amt)		malloc(amt, M_SCSISES, M_NOWAIT)
#define	SES_FREE(ptr, amt)	free(ptr, M_SCSISES)
#define	MEMZERO			bzero
#define	MEMCPY(dest, src, amt)	bcopy(src, dest, amt)

static int ses_runcmd(struct ses_softc *, char *, int, char *, int *);
static void ses_log(struct ses_softc *, const char *, ...);

/*
 * Gerenal FreeBSD kernel stuff.
 */


#define ccb_state	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct ses_softc {
	enctyp		ses_type;	/* type of enclosure */
	encvec		ses_vec;	/* vector to handlers */
	void *		ses_private;	/* per-type private data */
	encobj *	ses_objmap;	/* objects */
	uint32_t	ses_nobjects;	/* number of objects */
	ses_encstat	ses_encstat;	/* overall status */
	uint8_t	ses_flags;
	union ccb	ses_saved_ccb;
	struct cdev *ses_dev;
	struct cam_periph *periph;
};
#define	SES_FLAG_INVALID	0x01
#define	SES_FLAG_OPEN		0x02
#define	SES_FLAG_INITIALIZED	0x04

static	d_open_t	sesopen;
static	d_close_t	sesclose;
static	d_ioctl_t	sesioctl;
static	periph_init_t	sesinit;
static  periph_ctor_t	sesregister;
static	periph_oninv_t	sesoninvalidate;
static  periph_dtor_t   sescleanup;
static  periph_start_t  sesstart;

static void sesasync(void *, uint32_t, struct cam_path *, void *);
static void sesdone(struct cam_periph *, union ccb *);
static int seserror(union ccb *, uint32_t, uint32_t);

static struct periph_driver sesdriver = {
	sesinit, "ses",
	TAILQ_HEAD_INITIALIZER(sesdriver.units), /* generation */ 0
};

PERIPHDRIVER_DECLARE(ses, sesdriver);

static struct cdevsw ses_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	sesopen,
	.d_close =	sesclose,
	.d_ioctl =	sesioctl,
	.d_name =	"ses",
	.d_flags =	0,
};

static void
sesinit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, sesasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("ses: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	}
}

static void
sesoninvalidate(struct cam_periph *periph)
{
	struct ses_softc *softc;

	softc = (struct ses_softc *)periph->softc;

	/*
	 * Unregister any async callbacks.
	 */
	xpt_register_async(0, sesasync, periph, periph->path);

	softc->ses_flags |= SES_FLAG_INVALID;

	xpt_print(periph->path, "lost device\n");
}

static void
sescleanup(struct cam_periph *periph)
{
	struct ses_softc *softc;

	softc = (struct ses_softc *)periph->softc;

	xpt_print(periph->path, "removing device entry\n");
	cam_periph_unlock(periph);
	destroy_dev(softc->ses_dev);
	cam_periph_lock(periph);
	free(softc, M_SCSISES);
}

static void
sesasync(void *callback_arg, uint32_t code, struct cam_path *path, void *arg)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)callback_arg;

	switch(code) {
	case AC_FOUND_DEVICE:
	{
		cam_status status;
		struct ccb_getdev *cgd;
		int inq_len;

		cgd = (struct ccb_getdev *)arg;
		if (arg == NULL) {
			break;
		}

		if (cgd->protocol != PROTO_SCSI)
			break;

		inq_len = cgd->inq_data.additional_length + 4;

		/*
		 * PROBLEM: WE NEED TO LOOK AT BYTES 48-53 TO SEE IF THIS IS
		 * PROBLEM: IS A SAF-TE DEVICE.
		 */
		switch (ses_type(&cgd->inq_data, inq_len)) {
		case SES_SES:
		case SES_SES_SCSI2:
		case SES_SES_PASSTHROUGH:
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
	struct ccb_getdev *cgd;
	char *tname;

	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("sesregister: periph was NULL!!\n");
		return (CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("sesregister: no getdev CCB, can't register device\n");
		return (CAM_REQ_CMP_ERR);
	}

	softc = SES_MALLOC(sizeof (struct ses_softc));
	if (softc == NULL) {
		printf("sesregister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return (CAM_REQ_CMP_ERR);
	}
	bzero(softc, sizeof (struct ses_softc));
	periph->softc = softc;
	softc->periph = periph;

	softc->ses_type = ses_type(&cgd->inq_data, sizeof (cgd->inq_data));

	switch (softc->ses_type) {
	case SES_SES:
	case SES_SES_SCSI2:
        case SES_SES_PASSTHROUGH:
		softc->ses_vec.softc_init = ses_softc_init;
		softc->ses_vec.init_enc = ses_init_enc;
		softc->ses_vec.get_encstat = ses_get_encstat;
		softc->ses_vec.set_encstat = ses_set_encstat;
		softc->ses_vec.get_objstat = ses_get_objstat;
		softc->ses_vec.set_objstat = ses_set_objstat;
		break;
        case SES_SAFT:
		softc->ses_vec.softc_init = safte_softc_init;
		softc->ses_vec.init_enc = safte_init_enc;
		softc->ses_vec.get_encstat = safte_get_encstat;
		softc->ses_vec.set_encstat = safte_set_encstat;
		softc->ses_vec.get_objstat = safte_get_objstat;
		softc->ses_vec.set_objstat = safte_set_objstat;
		break;
        case SES_SEN:
		break;
	case SES_NONE:
	default:
		free(softc, M_SCSISES);
		return (CAM_REQ_CMP_ERR);
	}

	cam_periph_unlock(periph);
	softc->ses_dev = make_dev(&ses_cdevsw, periph->unit_number,
	    UID_ROOT, GID_OPERATOR, 0600, "%s%d",
	    periph->periph_name, periph->unit_number);
	cam_periph_lock(periph);
	softc->ses_dev->si_drv1 = periph;

	/*
	 * Add an async callback so that we get
	 * notified if this device goes away.
	 */
	xpt_register_async(AC_LOST_DEVICE, sesasync, periph, periph->path);

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
		tname = "UNISYS SEN Device (NOT HANDLED YET)";
		break;
        case SES_SAFT:
		tname = "SAF-TE Compliant Device";
		break;
	}
	xpt_announce_periph(periph, tname);
	return (CAM_REQ_CMP);
}

static int
sesopen(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct cam_periph *periph;
	struct ses_softc *softc;
	int error = 0;

	periph = (struct cam_periph *)dev->si_drv1;
	if (periph == NULL) {
		return (ENXIO);
	}

	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		cam_periph_unlock(periph);
		return (ENXIO);
	}

	cam_periph_lock(periph);

	softc = (struct ses_softc *)periph->softc;

	if (softc->ses_flags & SES_FLAG_INVALID) {
		error = ENXIO;
		goto out;
	}
	if (softc->ses_flags & SES_FLAG_OPEN) {
		error = EBUSY;
		goto out;
	}
	if (softc->ses_vec.softc_init == NULL) {
		error = ENXIO;
		goto out;
	}

	softc->ses_flags |= SES_FLAG_OPEN;
	if ((softc->ses_flags & SES_FLAG_INITIALIZED) == 0) {
		error = (*softc->ses_vec.softc_init)(softc, 1);
		if (error)
			softc->ses_flags &= ~SES_FLAG_OPEN;
		else
			softc->ses_flags |= SES_FLAG_INITIALIZED;
	}

out:
	cam_periph_unlock(periph);
	if (error) {
		cam_periph_release(periph);
	}
	return (error);
}

static int
sesclose(struct cdev *dev, int flag, int fmt, struct thread *td)
{
	struct cam_periph *periph;
	struct ses_softc *softc;
	int error;

	error = 0;

	periph = (struct cam_periph *)dev->si_drv1;
	if (periph == NULL)
		return (ENXIO);

	cam_periph_lock(periph);

	softc = (struct ses_softc *)periph->softc;
	softc->ses_flags &= ~SES_FLAG_OPEN;

	cam_periph_unlock(periph);
	cam_periph_release(periph);

	return (0);
}

static void
sesstart(struct cam_periph *p, union ccb *sccb)
{
	if (p->immediate_priority <= p->pinfo.priority) {
		SLIST_INSERT_HEAD(&p->ccb_list, &sccb->ccb_h, periph_links.sle);
		p->immediate_priority = CAM_PRIORITY_NONE;
		wakeup(&p->ccb_list);
	}
}

static void
sesdone(struct cam_periph *periph, union ccb *dccb)
{
	wakeup(&dccb->ccb_h.cbfcnp);
}

static int
seserror(union ccb *ccb, uint32_t cflags, uint32_t sflags)
{
	struct ses_softc *softc;
	struct cam_periph *periph;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct ses_softc *)periph->softc;

	return (cam_periph_error(ccb, cflags, sflags, &softc->ses_saved_ccb));
}

static int
sesioctl(struct cdev *dev, u_long cmd, caddr_t arg_addr, int flag, struct thread *td)
{
	struct cam_periph *periph;
	ses_encstat tmp;
	ses_objstat objs;
	ses_object *uobj;
	struct ses_softc *ssc;
	void *addr;
	int error, i;


	if (arg_addr)
		addr = *((caddr_t *) arg_addr);
	else
		addr = NULL;

	periph = (struct cam_periph *)dev->si_drv1;
	if (periph == NULL)
		return (ENXIO);

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("entering sesioctl\n"));

	cam_periph_lock(periph);
	ssc = (struct ses_softc *)periph->softc;

	/*
	 * Now check to see whether we're initialized or not.
	 * This actually should never fail as we're not supposed
	 * to get past ses_open w/o successfully initializing
	 * things.
	 */
	if ((ssc->ses_flags & SES_FLAG_INITIALIZED) == 0) {
		cam_periph_unlock(periph);
		return (ENXIO);
	}
	cam_periph_unlock(periph);

	error = 0;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
	    ("trying to do ioctl %#lx\n", cmd));

	/*
	 * If this command can change the device's state,
	 * we must have the device open for writing.
	 *
	 * For commands that get information about the
	 * device- we don't need to lock the peripheral
	 * if we aren't running a command. The number
	 * of objects and the contents will stay stable
	 * after the first open that does initialization.
	 * The periph also can't go away while a user
	 * process has it open.
	 */
	switch (cmd) {
	case SESIOC_GETNOBJ:
	case SESIOC_GETOBJMAP:
	case SESIOC_GETENCSTAT:
	case SESIOC_GETOBJSTAT:
		break;
	default:
		if ((flag & FWRITE) == 0) {
			return (EBADF);
		}
	}

	switch (cmd) {
	case SESIOC_GETNOBJ:
		error = copyout(&ssc->ses_nobjects, addr,
		    sizeof (ssc->ses_nobjects));
		break;
		
	case SESIOC_GETOBJMAP:
		for (uobj = addr, i = 0; i != ssc->ses_nobjects; i++) {
			ses_object kobj;
			kobj.obj_id = i;
			kobj.subencid = ssc->ses_objmap[i].subenclosure;
			kobj.object_type = ssc->ses_objmap[i].enctype;
			error = copyout(&kobj, &uobj[i], sizeof (ses_object));
			if (error) {
				break;
			}
		}
		break;

	case SESIOC_GETENCSTAT:
		cam_periph_lock(periph);
		error = (*ssc->ses_vec.get_encstat)(ssc, 1);
		if (error) {
			cam_periph_unlock(periph);
			break;
		}
		tmp = ssc->ses_encstat & ~ENCI_SVALID;
		cam_periph_unlock(periph);
		error = copyout(&tmp, addr, sizeof (ses_encstat));
		ssc->ses_encstat = tmp;
		break;

	case SESIOC_SETENCSTAT:
		error = copyin(addr, &tmp, sizeof (ses_encstat));
		if (error)
			break;
		cam_periph_lock(periph);
		error = (*ssc->ses_vec.set_encstat)(ssc, tmp, 1);
		cam_periph_unlock(periph);
		break;

	case SESIOC_GETOBJSTAT:
		error = copyin(addr, &objs, sizeof (ses_objstat));
		if (error)
			break;
		if (objs.obj_id >= ssc->ses_nobjects) {
			error = EINVAL;
			break;
		}
		cam_periph_lock(periph);
		error = (*ssc->ses_vec.get_objstat)(ssc, &objs, 1);
		cam_periph_unlock(periph);
		if (error)
			break;
		error = copyout(&objs, addr, sizeof (ses_objstat));
		/*
		 * Always (for now) invalidate entry.
		 */
		ssc->ses_objmap[objs.obj_id].svalid = 0;
		break;

	case SESIOC_SETOBJSTAT:
		error = copyin(addr, &objs, sizeof (ses_objstat));
		if (error)
			break;

		if (objs.obj_id >= ssc->ses_nobjects) {
			error = EINVAL;
			break;
		}
		cam_periph_lock(periph);
		error = (*ssc->ses_vec.set_objstat)(ssc, &objs, 1);
		cam_periph_unlock(periph);

		/*
		 * Always (for now) invalidate entry.
		 */
		ssc->ses_objmap[objs.obj_id].svalid = 0;
		break;

	case SESIOC_INIT:

		cam_periph_lock(periph);
		error = (*ssc->ses_vec.init_enc)(ssc);
		cam_periph_unlock(periph);
		break;

	default:
		cam_periph_lock(periph);
		error = cam_periph_ioctl(periph, cmd, arg_addr, seserror);
		cam_periph_unlock(periph);
		break;
	}
	return (error);
}

#define	SES_CFLAGS	CAM_RETRY_SELTO
#define	SES_FLAGS	SF_NO_PRINT | SF_RETRY_UA
static int
ses_runcmd(struct ses_softc *ssc, char *cdb, int cdbl, char *dptr, int *dlenp)
{
	int error, dlen;
	ccb_flags ddf;
	union ccb *ccb;

	if (dptr) {
		if ((dlen = *dlenp) < 0) {
			dlen = -dlen;
			ddf = CAM_DIR_OUT;
		} else {
			ddf = CAM_DIR_IN;
		}
	} else {
		dlen = 0;
		ddf = CAM_DIR_NONE;
	}

	if (cdbl > IOCDBLEN) {
		cdbl = IOCDBLEN;
	}

	ccb = cam_periph_getccb(ssc->periph, 1);
	cam_fill_csio(&ccb->csio, 0, sesdone, ddf, MSG_SIMPLE_Q_TAG, dptr,
	    dlen, sizeof (struct scsi_sense_data), cdbl, 60 * 1000);
	bcopy(cdb, ccb->csio.cdb_io.cdb_bytes, cdbl);

	error = cam_periph_runccb(ccb, seserror, SES_CFLAGS, SES_FLAGS, NULL);
	if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
		cam_release_devq(ccb->ccb_h.path, 0, 0, 0, FALSE);
	if (error) {
		if (dptr) {
			*dlenp = dlen;
		}
	} else {
		if (dptr) {
			*dlenp = ccb->csio.resid;
		}
	}
	xpt_release_ccb(ccb);
	return (error);
}

static void
ses_log(struct ses_softc *ssc, const char *fmt, ...)
{
	va_list ap;

	printf("%s%d: ", ssc->periph->periph_name, ssc->periph->unit_number);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

/*
 * The code after this point runs on many platforms,
 * so forgive the slightly awkward and nonconforming
 * appearance.
 */

/*
 * Is this a device that supports enclosure services?
 *
 * It's a a pretty simple ruleset- if it is device type 0x0D (13), it's
 * an SES device. If it happens to be an old UNISYS SEN device, we can
 * handle that too.
 */

#define	SAFTE_START	44
#define	SAFTE_END	50
#define	SAFTE_LEN	SAFTE_END-SAFTE_START

static enctyp
ses_type(void *buf, int buflen)
{
	unsigned char *iqd = buf;

	if (buflen < 8+SEN_ID_LEN)
		return (SES_NONE);

	if ((iqd[0] & 0x1f) == T_ENCLOSURE) {
		if (STRNCMP(&iqd[8], SEN_ID, SEN_ID_LEN) == 0) {
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
		return (SES_SES_PASSTHROUGH);
	}
#endif

	/*
	 * The comparison is short for a reason-
	 * some vendors were chopping it short.
	 */

	if (buflen < SAFTE_END - 2) {
		return (SES_NONE);
	}

	if (STRNCMP((char *)&iqd[SAFTE_START], "SAF-TE", SAFTE_LEN - 2) == 0) {
		return (SES_SAFT);
	}
	return (SES_NONE);
}

/*
 * SES Native Type Device Support
 */

/*
 * SES Diagnostic Page Codes
 */

typedef enum {
	SesConfigPage = 0x1,
	SesControlPage,
#define	SesStatusPage SesControlPage
	SesHelpTxt,
	SesStringOut,
#define	SesStringIn	SesStringOut
	SesThresholdOut,
#define	SesThresholdIn SesThresholdOut
	SesArrayControl,
#define	SesArrayStatus	SesArrayControl
	SesElementDescriptor,
	SesShortStatus
} SesDiagPageCodes;

/*
 * minimal amounts
 */

/*
 * Minimum amount of data, starting from byte 0, to have
 * the config header.
 */
#define	SES_CFGHDR_MINLEN	12

/*
 * Minimum amount of data, starting from byte 0, to have
 * the config header and one enclosure header.
 */
#define	SES_ENCHDR_MINLEN	48

/*
 * Take this value, subtract it from VEnclen and you know
 * the length of the vendor unique bytes.
 */
#define	SES_ENCHDR_VMIN		36

/*
 * SES Data Structures
 */

typedef struct {
	uint32_t GenCode;	/* Generation Code */
	uint8_t	Nsubenc;	/* Number of Subenclosures */
} SesCfgHdr;

typedef struct {
	uint8_t	Subencid;	/* SubEnclosure Identifier */
	uint8_t	Ntypes;		/* # of supported types */
	uint8_t	VEnclen;	/* Enclosure Descriptor Length */
} SesEncHdr;

typedef struct {
	uint8_t	encWWN[8];	/* XXX- Not Right Yet */
	uint8_t	encVid[8];
	uint8_t	encPid[16];
	uint8_t	encRev[4];
	uint8_t	encVen[1];
} SesEncDesc;

typedef struct {
	uint8_t	enc_type;		/* type of element */
	uint8_t	enc_maxelt;		/* maximum supported */
	uint8_t	enc_subenc;		/* in SubEnc # N */
	uint8_t	enc_tlen;		/* Type Descriptor Text Length */
} SesThdr;

typedef struct {
	uint8_t	comstatus;
	uint8_t	comstat[3];
} SesComStat;

struct typidx {
	int ses_tidx;
	int ses_oidx;
};

struct sscfg {
	uint8_t ses_ntypes;	/* total number of types supported */

	/*
	 * We need to keep a type index as well as an
	 * object index for each object in an enclosure.
	 */
	struct typidx *ses_typidx;

	/*
	 * We also need to keep track of the number of elements
	 * per type of element. This is needed later so that we
	 * can find precisely in the returned status data the
	 * status for the Nth element of the Kth type.
	 */
	uint8_t *	ses_eltmap;
};


/*
 * (de)canonicalization defines
 */
#define	sbyte(x, byte)		((((uint32_t)(x)) >> (byte * 8)) & 0xff)
#define	sbit(x, bit)		(((uint32_t)(x)) << bit)
#define	sset8(outp, idx, sval)	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 0)

#define	sset16(outp, idx, sval)	\
	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 1), \
	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 0)


#define	sset24(outp, idx, sval)	\
	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 2), \
	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 1), \
	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 0)


#define	sset32(outp, idx, sval)	\
	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 3), \
	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 2), \
	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 1), \
	(((uint8_t *)(outp))[idx++]) = sbyte(sval, 0)

#define	gbyte(x, byte)	((((uint32_t)(x)) & 0xff) << (byte * 8))
#define	gbit(lv, in, idx, shft, mask)	lv = ((in[idx] >> shft) & mask)
#define	sget8(inp, idx, lval)	lval = (((uint8_t *)(inp))[idx++])
#define	gget8(inp, idx, lval)	lval = (((uint8_t *)(inp))[idx])

#define	sget16(inp, idx, lval)	\
	lval = gbyte((((uint8_t *)(inp))[idx]), 1) | \
		(((uint8_t *)(inp))[idx+1]), idx += 2

#define	gget16(inp, idx, lval)	\
	lval = gbyte((((uint8_t *)(inp))[idx]), 1) | \
		(((uint8_t *)(inp))[idx+1])

#define	sget24(inp, idx, lval)	\
	lval = gbyte((((uint8_t *)(inp))[idx]), 2) | \
		gbyte((((uint8_t *)(inp))[idx+1]), 1) | \
			(((uint8_t *)(inp))[idx+2]), idx += 3

#define	gget24(inp, idx, lval)	\
	lval = gbyte((((uint8_t *)(inp))[idx]), 2) | \
		gbyte((((uint8_t *)(inp))[idx+1]), 1) | \
			(((uint8_t *)(inp))[idx+2])

#define	sget32(inp, idx, lval)	\
	lval = gbyte((((uint8_t *)(inp))[idx]), 3) | \
		gbyte((((uint8_t *)(inp))[idx+1]), 2) | \
		gbyte((((uint8_t *)(inp))[idx+2]), 1) | \
			(((uint8_t *)(inp))[idx+3]), idx += 4

#define	gget32(inp, idx, lval)	\
	lval = gbyte((((uint8_t *)(inp))[idx]), 3) | \
		gbyte((((uint8_t *)(inp))[idx+1]), 2) | \
		gbyte((((uint8_t *)(inp))[idx+2]), 1) | \
			(((uint8_t *)(inp))[idx+3])

#define	SCSZ	0x2000
#define	CFLEN	(256 + SES_ENCHDR_MINLEN)

/*
 * Routines specific && private to SES only
 */

static int ses_getconfig(ses_softc_t *);
static int ses_getputstat(ses_softc_t *, int, SesComStat *, int, int);
static int ses_cfghdr(uint8_t *, int, SesCfgHdr *);
static int ses_enchdr(uint8_t *, int, uint8_t, SesEncHdr *);
static int ses_encdesc(uint8_t *, int, uint8_t, SesEncDesc *);
static int ses_getthdr(uint8_t *, int,  int, SesThdr *);
static int ses_decode(char *, int, uint8_t *, int, int, SesComStat *);
static int ses_encode(char *, int, uint8_t *, int, int, SesComStat *);

static int
ses_softc_init(ses_softc_t *ssc, int doinit)
{
	if (doinit == 0) {
		struct sscfg *cc;
		if (ssc->ses_nobjects) {
			SES_FREE(ssc->ses_objmap,
			    ssc->ses_nobjects * sizeof (encobj));
			ssc->ses_objmap = NULL;
		}
		if ((cc = ssc->ses_private) != NULL) {
			if (cc->ses_eltmap && cc->ses_ntypes) {
				SES_FREE(cc->ses_eltmap, cc->ses_ntypes);
				cc->ses_eltmap = NULL;
				cc->ses_ntypes = 0;
			}
			if (cc->ses_typidx && ssc->ses_nobjects) {
				SES_FREE(cc->ses_typidx,
				    ssc->ses_nobjects * sizeof (struct typidx));
				cc->ses_typidx = NULL;
			}
			SES_FREE(cc, sizeof (struct sscfg));
			ssc->ses_private = NULL;
		}
		ssc->ses_nobjects = 0;
		return (0);
	}
	if (ssc->ses_private == NULL) {
		ssc->ses_private = SES_MALLOC(sizeof (struct sscfg));
	}
	if (ssc->ses_private == NULL) {
		return (ENOMEM);
	}
	ssc->ses_nobjects = 0;
	ssc->ses_encstat = 0;
	return (ses_getconfig(ssc));
}

static int
ses_init_enc(ses_softc_t *ssc)
{
	return (0);
}

static int
ses_get_encstat(ses_softc_t *ssc, int slpflag)
{
	SesComStat ComStat;
	int status;

	if ((status = ses_getputstat(ssc, -1, &ComStat, slpflag, 1)) != 0) {
		return (status);
	}
	ssc->ses_encstat = ComStat.comstatus | ENCI_SVALID;
	return (0);
}

static int
ses_set_encstat(ses_softc_t *ssc, uint8_t encstat, int slpflag)
{
	SesComStat ComStat;
	int status;

	ComStat.comstatus = encstat & 0xf;
	if ((status = ses_getputstat(ssc, -1, &ComStat, slpflag, 0)) != 0) {
		return (status);
	}
	ssc->ses_encstat = encstat & 0xf;	/* note no SVALID set */
	return (0);
}

static int
ses_get_objstat(ses_softc_t *ssc, ses_objstat *obp, int slpflag)
{
	int i = (int)obp->obj_id;

	if (ssc->ses_objmap[i].svalid == 0) {
		SesComStat ComStat;
		int err = ses_getputstat(ssc, i, &ComStat, slpflag, 1);
		if (err)
			return (err);
		ssc->ses_objmap[i].encstat[0] = ComStat.comstatus;
		ssc->ses_objmap[i].encstat[1] = ComStat.comstat[0];
		ssc->ses_objmap[i].encstat[2] = ComStat.comstat[1];
		ssc->ses_objmap[i].encstat[3] = ComStat.comstat[2];
		ssc->ses_objmap[i].svalid = 1;
	}
	obp->cstat[0] = ssc->ses_objmap[i].encstat[0];
	obp->cstat[1] = ssc->ses_objmap[i].encstat[1];
	obp->cstat[2] = ssc->ses_objmap[i].encstat[2];
	obp->cstat[3] = ssc->ses_objmap[i].encstat[3];
	return (0);
}

static int
ses_set_objstat(ses_softc_t *ssc, ses_objstat *obp, int slpflag)
{
	SesComStat ComStat;
	int err;
	/*
	 * If this is clear, we don't do diddly.
	 */
	if ((obp->cstat[0] & SESCTL_CSEL) == 0) {
		return (0);
	}
	ComStat.comstatus = obp->cstat[0];
	ComStat.comstat[0] = obp->cstat[1];
	ComStat.comstat[1] = obp->cstat[2];
	ComStat.comstat[2] = obp->cstat[3];
	err = ses_getputstat(ssc, (int)obp->obj_id, &ComStat, slpflag, 0);
	ssc->ses_objmap[(int)obp->obj_id].svalid = 0;
	return (err);
}

static int
ses_getconfig(ses_softc_t *ssc)
{
	struct sscfg *cc;
	SesCfgHdr cf;
	SesEncHdr hd;
	SesEncDesc *cdp;
	SesThdr thdr;
	int err, amt, i, nobj, ntype, maxima;
	char storage[CFLEN], *sdata;
	static char cdb[6] = {
	    RECEIVE_DIAGNOSTIC, 0x1, SesConfigPage, SCSZ >> 8, SCSZ & 0xff, 0
	};

	cc = ssc->ses_private;
	if (cc == NULL) {
		return (ENXIO);
	}

	sdata = SES_MALLOC(SCSZ);
	if (sdata == NULL)
		return (ENOMEM);

	amt = SCSZ;
	err = ses_runcmd(ssc, cdb, 6, sdata, &amt);
	if (err) {
		SES_FREE(sdata, SCSZ);
		return (err);
	}
	amt = SCSZ - amt;

	if (ses_cfghdr((uint8_t *) sdata, amt, &cf)) {
		SES_LOG(ssc, "Unable to parse SES Config Header\n");
		SES_FREE(sdata, SCSZ);
		return (EIO);
	}
	if (amt < SES_ENCHDR_MINLEN) {
		SES_LOG(ssc, "runt enclosure length (%d)\n", amt);
		SES_FREE(sdata, SCSZ);
		return (EIO);
	}

	SES_VLOG(ssc, "GenCode %x %d Subenclosures\n", cf.GenCode, cf.Nsubenc);

	/*
	 * Now waltz through all the subenclosures toting up the
	 * number of types available in each. For this, we only
	 * really need the enclosure header. However, we get the
	 * enclosure descriptor for debug purposes, as well
	 * as self-consistency checking purposes.
	 */

	maxima = cf.Nsubenc + 1;
	cdp = (SesEncDesc *) storage;
	for (ntype = i = 0; i < maxima; i++) {
		MEMZERO((caddr_t)cdp, sizeof (*cdp));
		if (ses_enchdr((uint8_t *) sdata, amt, i, &hd)) {
			SES_LOG(ssc, "Cannot Extract Enclosure Header %d\n", i);
			SES_FREE(sdata, SCSZ);
			return (EIO);
		}
		SES_VLOG(ssc, " SubEnclosure ID %d, %d Types With this ID, En"
		    "closure Length %d\n", hd.Subencid, hd.Ntypes, hd.VEnclen);

		if (ses_encdesc((uint8_t *)sdata, amt, i, cdp)) {
			SES_LOG(ssc, "Can't get Enclosure Descriptor %d\n", i);
			SES_FREE(sdata, SCSZ);
			return (EIO);
		}
		SES_VLOG(ssc, " WWN: %02x%02x%02x%02x%02x%02x%02x%02x\n",
		    cdp->encWWN[0], cdp->encWWN[1], cdp->encWWN[2],
		    cdp->encWWN[3], cdp->encWWN[4], cdp->encWWN[5],
		    cdp->encWWN[6], cdp->encWWN[7]);
		ntype += hd.Ntypes;
	}

	/*
	 * Now waltz through all the types that are available, getting
	 * the type header so we can start adding up the number of
	 * objects available.
	 */
	for (nobj = i = 0; i < ntype; i++) {
		if (ses_getthdr((uint8_t *)sdata, amt, i, &thdr)) {
			SES_LOG(ssc, "Can't get Enclosure Type Header %d\n", i);
			SES_FREE(sdata, SCSZ);
			return (EIO);
		}
		SES_LOG(ssc, " Type Desc[%d]: Type 0x%x, MaxElt %d, In Subenc "
		    "%d, Text Length %d\n", i, thdr.enc_type, thdr.enc_maxelt,
		    thdr.enc_subenc, thdr.enc_tlen);
		nobj += thdr.enc_maxelt;
	}


	/*
	 * Now allocate the object array and type map.
	 */

	ssc->ses_objmap = SES_MALLOC(nobj * sizeof (encobj));
	cc->ses_typidx = SES_MALLOC(nobj * sizeof (struct typidx));
	cc->ses_eltmap = SES_MALLOC(ntype);

	if (ssc->ses_objmap == NULL || cc->ses_typidx == NULL ||
	    cc->ses_eltmap == NULL) {
		if (ssc->ses_objmap) {
			SES_FREE(ssc->ses_objmap, (nobj * sizeof (encobj)));
			ssc->ses_objmap = NULL;
		}
		if (cc->ses_typidx) {
			SES_FREE(cc->ses_typidx,
			    (nobj * sizeof (struct typidx)));
			cc->ses_typidx = NULL;
		}
		if (cc->ses_eltmap) {
			SES_FREE(cc->ses_eltmap, ntype);
			cc->ses_eltmap = NULL;
		}
		SES_FREE(sdata, SCSZ);
		return (ENOMEM);
	}
	MEMZERO(ssc->ses_objmap, nobj * sizeof (encobj));
	MEMZERO(cc->ses_typidx, nobj * sizeof (struct typidx));
	MEMZERO(cc->ses_eltmap, ntype);
	cc->ses_ntypes = (uint8_t) ntype;
	ssc->ses_nobjects = nobj;

	/*
	 * Now waltz through the # of types again to fill in the types
	 * (and subenclosure ids) of the allocated objects.
	 */
	nobj = 0;
	for (i = 0; i < ntype; i++) {
		int j;
		if (ses_getthdr((uint8_t *)sdata, amt, i, &thdr)) {
			continue;
		}
		cc->ses_eltmap[i] = thdr.enc_maxelt;
		for (j = 0; j < thdr.enc_maxelt; j++) {
			cc->ses_typidx[nobj].ses_tidx = i;
			cc->ses_typidx[nobj].ses_oidx = j;
			ssc->ses_objmap[nobj].subenclosure = thdr.enc_subenc;
			ssc->ses_objmap[nobj++].enctype = thdr.enc_type;
		}
	}
	SES_FREE(sdata, SCSZ);
	return (0);
}

static int
ses_getputstat(ses_softc_t *ssc, int objid, SesComStat *sp, int slp, int in)
{
	struct sscfg *cc;
	int err, amt, bufsiz, tidx, oidx;
	char cdb[6], *sdata;

	cc = ssc->ses_private;
	if (cc == NULL) {
		return (ENXIO);
	}

	/*
	 * If we're just getting overall enclosure status,
	 * we only need 2 bytes of data storage.
	 *
	 * If we're getting anything else, we know how much
	 * storage we need by noting that starting at offset
	 * 8 in returned data, all object status bytes are 4
	 * bytes long, and are stored in chunks of types(M)
	 * and nth+1 instances of type M.
	 */
	if (objid == -1) {
		bufsiz = 2;
	} else {
		bufsiz = (ssc->ses_nobjects * 4) + (cc->ses_ntypes * 4) + 8;
	}
	sdata = SES_MALLOC(bufsiz);
	if (sdata == NULL)
		return (ENOMEM);

	cdb[0] = RECEIVE_DIAGNOSTIC;
	cdb[1] = 1;
	cdb[2] = SesStatusPage;
	cdb[3] = bufsiz >> 8;
	cdb[4] = bufsiz & 0xff;
	cdb[5] = 0;
	amt = bufsiz;
	err = ses_runcmd(ssc, cdb, 6, sdata, &amt);
	if (err) {
		SES_FREE(sdata, bufsiz);
		return (err);
	}
	amt = bufsiz - amt;

	if (objid == -1) {
		tidx = -1;
		oidx = -1;
	} else {
		tidx = cc->ses_typidx[objid].ses_tidx;
		oidx = cc->ses_typidx[objid].ses_oidx;
	}
	if (in) {
		if (ses_decode(sdata, amt, cc->ses_eltmap, tidx, oidx, sp)) {
			err = ENODEV;
		}
	} else {
		if (ses_encode(sdata, amt, cc->ses_eltmap, tidx, oidx, sp)) {
			err = ENODEV;
		} else {
			cdb[0] = SEND_DIAGNOSTIC;
			cdb[1] = 0x10;
			cdb[2] = 0;
			cdb[3] = bufsiz >> 8;
			cdb[4] = bufsiz & 0xff;
			cdb[5] = 0;
			amt = -bufsiz;
			err = ses_runcmd(ssc, cdb, 6, sdata, &amt);   
		}
	}
	SES_FREE(sdata, bufsiz);
	return (0);
}


/*
 * Routines to parse returned SES data structures.
 * Architecture and compiler independent.
 */

static int
ses_cfghdr(uint8_t *buffer, int buflen, SesCfgHdr *cfp)
{
	if (buflen < SES_CFGHDR_MINLEN) {
		return (-1);
	}
	gget8(buffer, 1, cfp->Nsubenc);
	gget32(buffer, 4, cfp->GenCode);
	return (0);
}

static int
ses_enchdr(uint8_t *buffer, int amt, uint8_t SubEncId, SesEncHdr *chp)
{
	int s, off = 8;
	for (s = 0; s < SubEncId; s++) {
		if (off + 3 > amt)
			return (-1);
		off += buffer[off+3] + 4;
	}
	if (off + 3 > amt) {
		return (-1);
	}
	gget8(buffer, off+1, chp->Subencid);
	gget8(buffer, off+2, chp->Ntypes);
	gget8(buffer, off+3, chp->VEnclen);
	return (0);
}

static int
ses_encdesc(uint8_t *buffer, int amt, uint8_t SubEncId, SesEncDesc *cdp)
{
	int s, e, enclen, off = 8;
	for (s = 0; s < SubEncId; s++) {
		if (off + 3 > amt)
			return (-1);
		off += buffer[off+3] + 4;
	}
	if (off + 3 > amt) {
		return (-1);
	}
	gget8(buffer, off+3, enclen);
	off += 4;
	if (off  >= amt)
		return (-1);

	e = off + enclen;
	if (e > amt) {
		e = amt;
	}
	MEMCPY(cdp, &buffer[off], e - off);
	return (0);
}

static int
ses_getthdr(uint8_t *buffer, int amt, int nth, SesThdr *thp)
{
	int s, off = 8;

	if (amt < SES_CFGHDR_MINLEN) {
		return (-1);
	}
	for (s = 0; s < buffer[1]; s++) {
		if (off + 3 > amt)
			return (-1);
		off += buffer[off+3] + 4;
	}
	if (off + 3 > amt) {
		return (-1);
	}
	off += buffer[off+3] + 4 + (nth * 4);
	if (amt < (off + 4))
		return (-1);

	gget8(buffer, off++, thp->enc_type);
	gget8(buffer, off++, thp->enc_maxelt);
	gget8(buffer, off++, thp->enc_subenc);
	gget8(buffer, off, thp->enc_tlen);
	return (0);
}

/*
 * This function needs a little explanation.
 *
 * The arguments are:
 *
 *
 *	char *b, int amt
 *
 *		These describes the raw input SES status data and length.
 *
 *	uint8_t *ep
 *
 *		This is a map of the number of types for each element type
 *		in the enclosure.
 *
 *	int elt
 *
 *		This is the element type being sought. If elt is -1,
 *		then overall enclosure status is being sought.
 *
 *	int elm
 *
 *		This is the ordinal Mth element of type elt being sought.
 *
 *	SesComStat *sp
 *
 *		This is the output area to store the status for
 *		the Mth element of type Elt.
 */

static int
ses_decode(char *b, int amt, uint8_t *ep, int elt, int elm, SesComStat *sp)
{
	int idx, i;

	/*
	 * If it's overall enclosure status being sought, get that.
	 * We need at least 2 bytes of status data to get that.
	 */
	if (elt == -1) {
		if (amt < 2)
			return (-1);
		gget8(b, 1, sp->comstatus);
		sp->comstat[0] = 0;
		sp->comstat[1] = 0;
		sp->comstat[2] = 0;
		return (0);
	}

	/*
	 * Check to make sure that the Mth element is legal for type Elt.
	 */

	if (elm >= ep[elt])
		return (-1);

	/*
	 * Starting at offset 8, start skipping over the storage
	 * for the element types we're not interested in.
	 */
	for (idx = 8, i = 0; i < elt; i++) {
		idx += ((ep[i] + 1) * 4);
	}

	/*
	 * Skip over Overall status for this element type.
	 */
	idx += 4;

	/*
	 * And skip to the index for the Mth element that we're going for.
	 */
	idx += (4 * elm);

	/*
	 * Make sure we haven't overflowed the buffer.
	 */
	if (idx+4 > amt)
		return (-1);

	/*
	 * Retrieve the status.
	 */
	gget8(b, idx++, sp->comstatus);
	gget8(b, idx++, sp->comstat[0]);
	gget8(b, idx++, sp->comstat[1]);
	gget8(b, idx++, sp->comstat[2]);
#if	0
	PRINTF("Get Elt 0x%x Elm 0x%x (idx %d)\n", elt, elm, idx-4);
#endif
	return (0);
}

/*
 * This is the mirror function to ses_decode, but we set the 'select'
 * bit for the object which we're interested in. All other objects,
 * after a status fetch, should have that bit off. Hmm. It'd be easy
 * enough to ensure this, so we will.
 */

static int
ses_encode(char *b, int amt, uint8_t *ep, int elt, int elm, SesComStat *sp)
{
	int idx, i;

	/*
	 * If it's overall enclosure status being sought, get that.
	 * We need at least 2 bytes of status data to get that.
	 */
	if (elt == -1) {
		if (amt < 2)
			return (-1);
		i = 0;
		sset8(b, i, 0);
		sset8(b, i, sp->comstatus & 0xf);
#if	0
		PRINTF("set EncStat %x\n", sp->comstatus);
#endif
		return (0);
	}

	/*
	 * Check to make sure that the Mth element is legal for type Elt.
	 */

	if (elm >= ep[elt])
		return (-1);

	/*
	 * Starting at offset 8, start skipping over the storage
	 * for the element types we're not interested in.
	 */
	for (idx = 8, i = 0; i < elt; i++) {
		idx += ((ep[i] + 1) * 4);
	}

	/*
	 * Skip over Overall status for this element type.
	 */
	idx += 4;

	/*
	 * And skip to the index for the Mth element that we're going for.
	 */
	idx += (4 * elm);

	/*
	 * Make sure we haven't overflowed the buffer.
	 */
	if (idx+4 > amt)
		return (-1);

	/*
	 * Set the status.
	 */
	sset8(b, idx, sp->comstatus);
	sset8(b, idx, sp->comstat[0]);
	sset8(b, idx, sp->comstat[1]);
	sset8(b, idx, sp->comstat[2]);
	idx -= 4;

#if	0
	PRINTF("Set Elt 0x%x Elm 0x%x (idx %d) with %x %x %x %x\n",
	    elt, elm, idx, sp->comstatus, sp->comstat[0],
	    sp->comstat[1], sp->comstat[2]);
#endif

	/*
	 * Now make sure all other 'Select' bits are off.
	 */
	for (i = 8; i < amt; i += 4) {
		if (i != idx)
			b[i] &= ~0x80;
	}
	/*
	 * And make sure the INVOP bit is clear.
	 */
	b[2] &= ~0x10;

	return (0);
}

/*
 * SAF-TE Type Device Emulation
 */

static int safte_getconfig(ses_softc_t *);
static int safte_rdstat(ses_softc_t *, int);
static int set_objstat_sel(ses_softc_t *, ses_objstat *, int);
static int wrbuf16(ses_softc_t *, uint8_t, uint8_t, uint8_t, uint8_t, int);
static void wrslot_stat(ses_softc_t *, int);
static int perf_slotop(ses_softc_t *, uint8_t, uint8_t, int);

#define	ALL_ENC_STAT (SES_ENCSTAT_CRITICAL | SES_ENCSTAT_UNRECOV | \
	SES_ENCSTAT_NONCRITICAL | SES_ENCSTAT_INFO)
/*
 * SAF-TE specific defines- Mandatory ones only...
 */

/*
 * READ BUFFER ('get' commands) IDs- placed in offset 2 of cdb
 */
#define	SAFTE_RD_RDCFG	0x00	/* read enclosure configuration */
#define	SAFTE_RD_RDESTS	0x01	/* read enclosure status */
#define	SAFTE_RD_RDDSTS	0x04	/* read drive slot status */

/*
 * WRITE BUFFER ('set' commands) IDs- placed in offset 0 of databuf
 */
#define	SAFTE_WT_DSTAT	0x10	/* write device slot status */
#define	SAFTE_WT_SLTOP	0x12	/* perform slot operation */
#define	SAFTE_WT_FANSPD	0x13	/* set fan speed */
#define	SAFTE_WT_ACTPWS	0x14	/* turn on/off power supply */
#define	SAFTE_WT_GLOBAL	0x15	/* send global command */


#define	SAFT_SCRATCH	64
#define	NPSEUDO_THERM	16
#define	NPSEUDO_ALARM	1
struct scfg {
	/*
	 * Cached Configuration
	 */
	uint8_t	Nfans;		/* Number of Fans */
	uint8_t	Npwr;		/* Number of Power Supplies */
	uint8_t	Nslots;		/* Number of Device Slots */
	uint8_t	DoorLock;	/* Door Lock Installed */
	uint8_t	Ntherm;		/* Number of Temperature Sensors */
	uint8_t	Nspkrs;		/* Number of Speakers */
	uint8_t Nalarm;		/* Number of Alarms (at least one) */
	/*
	 * Cached Flag Bytes for Global Status
	 */
	uint8_t	flag1;
	uint8_t	flag2;
	/*
	 * What object index ID is where various slots start.
	 */
	uint8_t	pwroff;
	uint8_t	slotoff;
#define	SAFT_ALARM_OFFSET(cc)	(cc)->slotoff - 1
};

#define	SAFT_FLG1_ALARM		0x1
#define	SAFT_FLG1_GLOBFAIL	0x2
#define	SAFT_FLG1_GLOBWARN	0x4
#define	SAFT_FLG1_ENCPWROFF	0x8
#define	SAFT_FLG1_ENCFANFAIL	0x10
#define	SAFT_FLG1_ENCPWRFAIL	0x20
#define	SAFT_FLG1_ENCDRVFAIL	0x40
#define	SAFT_FLG1_ENCDRVWARN	0x80

#define	SAFT_FLG2_LOCKDOOR	0x4
#define	SAFT_PRIVATE		sizeof (struct scfg)

static char *safte_2little = "Too Little Data Returned (%d) at line %d\n";
#define	SAFT_BAIL(r, x, k, l)	\
	if ((r) >= (x)) { \
		SES_LOG(ssc, safte_2little, x, __LINE__);\
		SES_FREE((k), (l)); \
		return (EIO); \
	}


static int
safte_softc_init(ses_softc_t *ssc, int doinit)
{
	int err, i, r;
	struct scfg *cc;

	if (doinit == 0) {
		if (ssc->ses_nobjects) {
			if (ssc->ses_objmap) {
				SES_FREE(ssc->ses_objmap,
				    ssc->ses_nobjects * sizeof (encobj));
				ssc->ses_objmap = NULL;
			}
			ssc->ses_nobjects = 0;
		}
		if (ssc->ses_private) {
			SES_FREE(ssc->ses_private, SAFT_PRIVATE);
			ssc->ses_private = NULL;
		}
		return (0);
	}

	if (ssc->ses_private == NULL) {
		ssc->ses_private = SES_MALLOC(SAFT_PRIVATE);
		if (ssc->ses_private == NULL) {
			return (ENOMEM);
		}
		MEMZERO(ssc->ses_private, SAFT_PRIVATE);
	}

	ssc->ses_nobjects = 0;
	ssc->ses_encstat = 0;

	if ((err = safte_getconfig(ssc)) != 0) {
		return (err);
	}

	/*
	 * The number of objects here, as well as that reported by the
	 * READ_BUFFER/GET_CONFIG call, are the over-temperature flags (15)
	 * that get reported during READ_BUFFER/READ_ENC_STATUS.
	 */
	cc = ssc->ses_private;
	ssc->ses_nobjects = cc->Nfans + cc->Npwr + cc->Nslots + cc->DoorLock +
	    cc->Ntherm + cc->Nspkrs + NPSEUDO_THERM + NPSEUDO_ALARM;
	ssc->ses_objmap = (encobj *)
	    SES_MALLOC(ssc->ses_nobjects * sizeof (encobj));
	if (ssc->ses_objmap == NULL) {
		return (ENOMEM);
	}
	MEMZERO(ssc->ses_objmap, ssc->ses_nobjects * sizeof (encobj));

	r = 0;
	/*
	 * Note that this is all arranged for the convenience
	 * in later fetches of status.
	 */
	for (i = 0; i < cc->Nfans; i++)
		ssc->ses_objmap[r++].enctype = SESTYP_FAN;
	cc->pwroff = (uint8_t) r;
	for (i = 0; i < cc->Npwr; i++)
		ssc->ses_objmap[r++].enctype = SESTYP_POWER;
	for (i = 0; i < cc->DoorLock; i++)
		ssc->ses_objmap[r++].enctype = SESTYP_DOORLOCK;
	for (i = 0; i < cc->Nspkrs; i++)
		ssc->ses_objmap[r++].enctype = SESTYP_ALARM;
	for (i = 0; i < cc->Ntherm; i++)
		ssc->ses_objmap[r++].enctype = SESTYP_THERM;
	for (i = 0; i < NPSEUDO_THERM; i++)
		ssc->ses_objmap[r++].enctype = SESTYP_THERM;
	ssc->ses_objmap[r++].enctype = SESTYP_ALARM;
	cc->slotoff = (uint8_t) r;
	for (i = 0; i < cc->Nslots; i++)
		ssc->ses_objmap[r++].enctype = SESTYP_DEVICE;
	return (0);
}

static int
safte_init_enc(ses_softc_t *ssc)
{
	int err;
	static char cdb0[6] = { SEND_DIAGNOSTIC };

	err = ses_runcmd(ssc, cdb0, 6, NULL, 0);
	if (err) {
		return (err);
	}
	DELAY(5000);
	err = wrbuf16(ssc, SAFTE_WT_GLOBAL, 0, 0, 0, 1);
	return (err);
}

static int
safte_get_encstat(ses_softc_t *ssc, int slpflg)
{
	return (safte_rdstat(ssc, slpflg));
}

static int
safte_set_encstat(ses_softc_t *ssc, uint8_t encstat, int slpflg)
{
	struct scfg *cc = ssc->ses_private;
	if (cc == NULL)
		return (0);
	/*
	 * Since SAF-TE devices aren't necessarily sticky in terms
	 * of state, make our soft copy of enclosure status 'sticky'-
	 * that is, things set in enclosure status stay set (as implied
	 * by conditions set in reading object status) until cleared.
	 */
	ssc->ses_encstat &= ~ALL_ENC_STAT;
	ssc->ses_encstat |= (encstat & ALL_ENC_STAT);
	ssc->ses_encstat |= ENCI_SVALID;
	cc->flag1 &= ~(SAFT_FLG1_ALARM|SAFT_FLG1_GLOBFAIL|SAFT_FLG1_GLOBWARN);
	if ((encstat & (SES_ENCSTAT_CRITICAL|SES_ENCSTAT_UNRECOV)) != 0) {
		cc->flag1 |= SAFT_FLG1_ALARM|SAFT_FLG1_GLOBFAIL;
	} else if ((encstat & SES_ENCSTAT_NONCRITICAL) != 0) {
		cc->flag1 |= SAFT_FLG1_GLOBWARN;
	}
	return (wrbuf16(ssc, SAFTE_WT_GLOBAL, cc->flag1, cc->flag2, 0, slpflg));
}

static int
safte_get_objstat(ses_softc_t *ssc, ses_objstat *obp, int slpflg)
{
	int i = (int)obp->obj_id;

	if ((ssc->ses_encstat & ENCI_SVALID) == 0 ||
	    (ssc->ses_objmap[i].svalid) == 0) {
		int err = safte_rdstat(ssc, slpflg);
		if (err)
			return (err);
	}
	obp->cstat[0] = ssc->ses_objmap[i].encstat[0];
	obp->cstat[1] = ssc->ses_objmap[i].encstat[1];
	obp->cstat[2] = ssc->ses_objmap[i].encstat[2];
	obp->cstat[3] = ssc->ses_objmap[i].encstat[3];
	return (0);
}


static int
safte_set_objstat(ses_softc_t *ssc, ses_objstat *obp, int slp)
{
	int idx, err;
	encobj *ep;
	struct scfg *cc;


	SES_DLOG(ssc, "safte_set_objstat(%d): %x %x %x %x\n",
	    (int)obp->obj_id, obp->cstat[0], obp->cstat[1], obp->cstat[2],
	    obp->cstat[3]);

	/*
	 * If this is clear, we don't do diddly.
	 */
	if ((obp->cstat[0] & SESCTL_CSEL) == 0) {
		return (0);
	}

	err = 0;
	/*
	 * Check to see if the common bits are set and do them first.
	 */
	if (obp->cstat[0] & ~SESCTL_CSEL) {
		err = set_objstat_sel(ssc, obp, slp);
		if (err)
			return (err);
	}

	cc = ssc->ses_private;
	if (cc == NULL)
		return (0);

	idx = (int)obp->obj_id;
	ep = &ssc->ses_objmap[idx];

	switch (ep->enctype) {
	case SESTYP_DEVICE:
	{
		uint8_t slotop = 0;
		/*
		 * XXX: I should probably cache the previous state
		 * XXX: of SESCTL_DEVOFF so that when it goes from
		 * XXX: true to false I can then set PREPARE FOR OPERATION
		 * XXX: flag in PERFORM SLOT OPERATION write buffer command.
		 */
		if (obp->cstat[2] & (SESCTL_RQSINS|SESCTL_RQSRMV)) {
			slotop |= 0x2;
		}
		if (obp->cstat[2] & SESCTL_RQSID) {
			slotop |= 0x4;
		}
		err = perf_slotop(ssc, (uint8_t) idx - (uint8_t) cc->slotoff,
		    slotop, slp);
		if (err)
			return (err);
		if (obp->cstat[3] & SESCTL_RQSFLT) {
			ep->priv |= 0x2;
		} else {
			ep->priv &= ~0x2;
		}
		if (ep->priv & 0xc6) {
			ep->priv &= ~0x1;
		} else {
			ep->priv |= 0x1;	/* no errors */
		}
		wrslot_stat(ssc, slp);
		break;
	}
	case SESTYP_POWER:
		if (obp->cstat[3] & SESCTL_RQSTFAIL) {
			cc->flag1 |= SAFT_FLG1_ENCPWRFAIL;
		} else {
			cc->flag1 &= ~SAFT_FLG1_ENCPWRFAIL;
		}
		err = wrbuf16(ssc, SAFTE_WT_GLOBAL, cc->flag1,
		    cc->flag2, 0, slp);
		if (err)
			return (err);
		if (obp->cstat[3] & SESCTL_RQSTON) {
			(void) wrbuf16(ssc, SAFTE_WT_ACTPWS,
				idx - cc->pwroff, 0, 0, slp);
		} else {
			(void) wrbuf16(ssc, SAFTE_WT_ACTPWS,
				idx - cc->pwroff, 0, 1, slp);
		}
		break;
	case SESTYP_FAN:
		if (obp->cstat[3] & SESCTL_RQSTFAIL) {
			cc->flag1 |= SAFT_FLG1_ENCFANFAIL;
		} else {
			cc->flag1 &= ~SAFT_FLG1_ENCFANFAIL;
		}
		err = wrbuf16(ssc, SAFTE_WT_GLOBAL, cc->flag1,
		    cc->flag2, 0, slp);
		if (err)
			return (err);
		if (obp->cstat[3] & SESCTL_RQSTON) {
			uint8_t fsp;
			if ((obp->cstat[3] & 0x7) == 7) {
				fsp = 4;
			} else if ((obp->cstat[3] & 0x7) == 6) {
				fsp = 3;
			} else if ((obp->cstat[3] & 0x7) == 4) {
				fsp = 2;
			} else {
				fsp = 1;
			}
			(void) wrbuf16(ssc, SAFTE_WT_FANSPD, idx, fsp, 0, slp);
		} else {
			(void) wrbuf16(ssc, SAFTE_WT_FANSPD, idx, 0, 0, slp);
		}
		break;
	case SESTYP_DOORLOCK:
		if (obp->cstat[3] & 0x1) {
			cc->flag2 &= ~SAFT_FLG2_LOCKDOOR;
		} else {
			cc->flag2 |= SAFT_FLG2_LOCKDOOR;
		}
		(void) wrbuf16(ssc, SAFTE_WT_GLOBAL, cc->flag1,
		    cc->flag2, 0, slp);
		break;
	case SESTYP_ALARM:
		/*
		 * On all nonzero but the 'muted' bit, we turn on the alarm,
		 */
		obp->cstat[3] &= ~0xa;
		if (obp->cstat[3] & 0x40) {
			cc->flag2 &= ~SAFT_FLG1_ALARM;
		} else if (obp->cstat[3] != 0) {
			cc->flag2 |= SAFT_FLG1_ALARM;
		} else {
			cc->flag2 &= ~SAFT_FLG1_ALARM;
		}
		ep->priv = obp->cstat[3];
		(void) wrbuf16(ssc, SAFTE_WT_GLOBAL, cc->flag1,
			cc->flag2, 0, slp);
		break;
	default:
		break;
	}
	ep->svalid = 0;
	return (0);
}

static int
safte_getconfig(ses_softc_t *ssc)
{
	struct scfg *cfg;
	int err, amt;
	char *sdata;
	static char cdb[10] =
	    { READ_BUFFER, 1, SAFTE_RD_RDCFG, 0, 0, 0, 0, 0, SAFT_SCRATCH, 0 };

	cfg = ssc->ses_private;
	if (cfg == NULL)
		return (ENXIO);

	sdata = SES_MALLOC(SAFT_SCRATCH);
	if (sdata == NULL)
		return (ENOMEM);

	amt = SAFT_SCRATCH;
	err = ses_runcmd(ssc, cdb, 10, sdata, &amt);
	if (err) {
		SES_FREE(sdata, SAFT_SCRATCH);
		return (err);
	}
	amt = SAFT_SCRATCH - amt;
	if (amt < 6) {
		SES_LOG(ssc, "too little data (%d) for configuration\n", amt);
		SES_FREE(sdata, SAFT_SCRATCH);
		return (EIO);
	}
	SES_VLOG(ssc, "Nfans %d Npwr %d Nslots %d Lck %d Ntherm %d Nspkrs %d\n",
	    sdata[0], sdata[1], sdata[2], sdata[3], sdata[4], sdata[5]);
	cfg->Nfans = sdata[0];
	cfg->Npwr = sdata[1];
	cfg->Nslots = sdata[2];
	cfg->DoorLock = sdata[3];
	cfg->Ntherm = sdata[4];
	cfg->Nspkrs = sdata[5];
	cfg->Nalarm = NPSEUDO_ALARM;
	SES_FREE(sdata, SAFT_SCRATCH);
	return (0);
}

static int
safte_rdstat(ses_softc_t *ssc, int slpflg)
{
	int err, oid, r, i, hiwater, nitems, amt;
	uint16_t tempflags;
	size_t buflen;
	uint8_t status, oencstat;
	char *sdata, cdb[10];
	struct scfg *cc = ssc->ses_private;


	/*
	 * The number of objects overstates things a bit,
	 * both for the bogus 'thermometer' entries and
	 * the drive status (which isn't read at the same
	 * time as the enclosure status), but that's okay.
	 */
	buflen = 4 * cc->Nslots;
	if (ssc->ses_nobjects > buflen)
		buflen = ssc->ses_nobjects;
	sdata = SES_MALLOC(buflen);
	if (sdata == NULL)
		return (ENOMEM);

	cdb[0] = READ_BUFFER;
	cdb[1] = 1;
	cdb[2] = SAFTE_RD_RDESTS;
	cdb[3] = 0;
	cdb[4] = 0;
	cdb[5] = 0;
	cdb[6] = 0;
	cdb[7] = (buflen >> 8) & 0xff;
	cdb[8] = buflen & 0xff;
	cdb[9] = 0;
	amt = buflen;
	err = ses_runcmd(ssc, cdb, 10, sdata, &amt);
	if (err) {
		SES_FREE(sdata, buflen);
		return (err);
	}
	hiwater = buflen - amt;


	/*
	 * invalidate all status bits.
	 */
	for (i = 0; i < ssc->ses_nobjects; i++)
		ssc->ses_objmap[i].svalid = 0;
	oencstat = ssc->ses_encstat & ALL_ENC_STAT;
	ssc->ses_encstat = 0;


	/*
	 * Now parse returned buffer.
	 * If we didn't get enough data back,
	 * that's considered a fatal error.
	 */
	oid = r = 0;

	for (nitems = i = 0; i < cc->Nfans; i++) {
		SAFT_BAIL(r, hiwater, sdata, buflen);
		/*
		 * 0 = Fan Operational
		 * 1 = Fan is malfunctioning
		 * 2 = Fan is not present
		 * 0x80 = Unknown or Not Reportable Status
		 */
		ssc->ses_objmap[oid].encstat[1] = 0;	/* resvd */
		ssc->ses_objmap[oid].encstat[2] = 0;	/* resvd */
		switch ((int)(uint8_t)sdata[r]) {
		case 0:
			nitems++;
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_OK;
			/*
			 * We could get fancier and cache
			 * fan speeds that we have set, but
			 * that isn't done now.
			 */
			ssc->ses_objmap[oid].encstat[3] = 7;
			break;

		case 1:
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_CRIT;
			/*
			 * FAIL and FAN STOPPED synthesized
			 */
			ssc->ses_objmap[oid].encstat[3] = 0x40;
			/*
			 * Enclosure marked with CRITICAL error
			 * if only one fan or no thermometers,
			 * else the NONCRITICAL error is set.
			 */
			if (cc->Nfans == 1 || cc->Ntherm == 0)
				ssc->ses_encstat |= SES_ENCSTAT_CRITICAL;
			else
				ssc->ses_encstat |= SES_ENCSTAT_NONCRITICAL;
			break;
		case 2:
			ssc->ses_objmap[oid].encstat[0] =
			    SES_OBJSTAT_NOTINSTALLED;
			ssc->ses_objmap[oid].encstat[3] = 0;
			/*
			 * Enclosure marked with CRITICAL error
			 * if only one fan or no thermometers,
			 * else the NONCRITICAL error is set.
			 */
			if (cc->Nfans == 1)
				ssc->ses_encstat |= SES_ENCSTAT_CRITICAL;
			else
				ssc->ses_encstat |= SES_ENCSTAT_NONCRITICAL;
			break;
		case 0x80:
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_UNKNOWN;
			ssc->ses_objmap[oid].encstat[3] = 0;
			ssc->ses_encstat |= SES_ENCSTAT_INFO;
			break;
		default:
			ssc->ses_objmap[oid].encstat[0] =
			    SES_OBJSTAT_UNSUPPORTED;
			SES_LOG(ssc, "Unknown fan%d status 0x%x\n", i,
			    sdata[r] & 0xff);
			break;
		}
		ssc->ses_objmap[oid++].svalid = 1;
		r++;
	}

	/*
	 * No matter how you cut it, no cooling elements when there
	 * should be some there is critical.
	 */
	if (cc->Nfans && nitems == 0) {
		ssc->ses_encstat |= SES_ENCSTAT_CRITICAL;
	}


	for (i = 0; i < cc->Npwr; i++) {
		SAFT_BAIL(r, hiwater, sdata, buflen);
		ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_UNKNOWN;
		ssc->ses_objmap[oid].encstat[1] = 0;	/* resvd */
		ssc->ses_objmap[oid].encstat[2] = 0;	/* resvd */
		ssc->ses_objmap[oid].encstat[3] = 0x20;	/* requested on */
		switch ((uint8_t)sdata[r]) {
		case 0x00:	/* pws operational and on */
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_OK;
			break;
		case 0x01:	/* pws operational and off */
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_OK;
			ssc->ses_objmap[oid].encstat[3] = 0x10;
			ssc->ses_encstat |= SES_ENCSTAT_INFO;
			break;
		case 0x10:	/* pws is malfunctioning and commanded on */
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_CRIT;
			ssc->ses_objmap[oid].encstat[3] = 0x61;
			ssc->ses_encstat |= SES_ENCSTAT_NONCRITICAL;
			break;

		case 0x11:	/* pws is malfunctioning and commanded off */
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_NONCRIT;
			ssc->ses_objmap[oid].encstat[3] = 0x51;
			ssc->ses_encstat |= SES_ENCSTAT_NONCRITICAL;
			break;
		case 0x20:	/* pws is not present */
			ssc->ses_objmap[oid].encstat[0] =
			    SES_OBJSTAT_NOTINSTALLED;
			ssc->ses_objmap[oid].encstat[3] = 0;
			ssc->ses_encstat |= SES_ENCSTAT_INFO;
			break;
		case 0x21:	/* pws is present */
			/*
			 * This is for enclosures that cannot tell whether the
			 * device is on or malfunctioning, but know that it is
			 * present. Just fall through.
			 */
			/* FALLTHROUGH */
		case 0x80:	/* Unknown or Not Reportable Status */
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_UNKNOWN;
			ssc->ses_objmap[oid].encstat[3] = 0;
			ssc->ses_encstat |= SES_ENCSTAT_INFO;
			break;
		default:
			SES_LOG(ssc, "unknown power supply %d status (0x%x)\n",
			    i, sdata[r] & 0xff);
			break;
		}
		ssc->ses_objmap[oid++].svalid = 1;
		r++;
	}

	/*
	 * Skip over Slot SCSI IDs
	 */
	r += cc->Nslots;

	/*
	 * We always have doorlock status, no matter what,
	 * but we only save the status if we have one.
	 */
	SAFT_BAIL(r, hiwater, sdata, buflen);
	if (cc->DoorLock) {
		/*
		 * 0 = Door Locked
		 * 1 = Door Unlocked, or no Lock Installed
		 * 0x80 = Unknown or Not Reportable Status
		 */
		ssc->ses_objmap[oid].encstat[1] = 0;
		ssc->ses_objmap[oid].encstat[2] = 0;
		switch ((uint8_t)sdata[r]) {
		case 0:
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_OK;
			ssc->ses_objmap[oid].encstat[3] = 0;
			break;
		case 1:
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_OK;
			ssc->ses_objmap[oid].encstat[3] = 1;
			break;
		case 0x80:
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_UNKNOWN;
			ssc->ses_objmap[oid].encstat[3] = 0;
			ssc->ses_encstat |= SES_ENCSTAT_INFO;
			break;
		default:
			ssc->ses_objmap[oid].encstat[0] =
			    SES_OBJSTAT_UNSUPPORTED;
			SES_LOG(ssc, "unknown lock status 0x%x\n",
			    sdata[r] & 0xff);
			break;
		}
		ssc->ses_objmap[oid++].svalid = 1;
	}
	r++;

	/*
	 * We always have speaker status, no matter what,
	 * but we only save the status if we have one.
	 */
	SAFT_BAIL(r, hiwater, sdata, buflen);
	if (cc->Nspkrs) {
		ssc->ses_objmap[oid].encstat[1] = 0;
		ssc->ses_objmap[oid].encstat[2] = 0;
		if (sdata[r] == 1) {
			/*
			 * We need to cache tone urgency indicators.
			 * Someday.
			 */
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_NONCRIT;
			ssc->ses_objmap[oid].encstat[3] = 0x8;
			ssc->ses_encstat |= SES_ENCSTAT_NONCRITICAL;
		} else if (sdata[r] == 0) {
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_OK;
			ssc->ses_objmap[oid].encstat[3] = 0;
		} else {
			ssc->ses_objmap[oid].encstat[0] =
			    SES_OBJSTAT_UNSUPPORTED;
			ssc->ses_objmap[oid].encstat[3] = 0;
			SES_LOG(ssc, "unknown spkr status 0x%x\n",
			    sdata[r] & 0xff);
		}
		ssc->ses_objmap[oid++].svalid = 1;
	}
	r++;

	for (i = 0; i < cc->Ntherm; i++) {
		SAFT_BAIL(r, hiwater, sdata, buflen);
		/*
		 * Status is a range from -10 to 245 deg Celsius,
		 * which we need to normalize to -20 to -245 according
		 * to the latest SCSI spec, which makes little
		 * sense since this would overflow an 8bit value.
		 * Well, still, the base normalization is -20,
		 * not -10, so we have to adjust.
		 *
		 * So what's over and under temperature?
		 * Hmm- we'll state that 'normal' operating
		 * is 10 to 40 deg Celsius.
		 */

		/*
		 * Actually.... All of the units that people out in the world
		 * seem to have do not come even close to setting a value that
		 * complies with this spec.
		 *
		 * The closest explanation I could find was in an
		 * LSI-Logic manual, which seemed to indicate that
		 * this value would be set by whatever the I2C code
		 * would interpolate from the output of an LM75
		 * temperature sensor.
		 *
		 * This means that it is impossible to use the actual
		 * numeric value to predict anything. But we don't want
		 * to lose the value. So, we'll propagate the *uncorrected*
		 * value and set SES_OBJSTAT_NOTAVAIL. We'll depend on the
		 * temperature flags for warnings.
		 */
		ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_NOTAVAIL;
		ssc->ses_objmap[oid].encstat[1] = 0;
		ssc->ses_objmap[oid].encstat[2] = sdata[r];
		ssc->ses_objmap[oid].encstat[3] = 0;
		ssc->ses_objmap[oid++].svalid = 1;
		r++;
	}

	/*
	 * Now, for "pseudo" thermometers, we have two bytes
	 * of information in enclosure status- 16 bits. Actually,
	 * the MSB is a single TEMP ALERT flag indicating whether
	 * any other bits are set, but, thanks to fuzzy thinking,
	 * in the SAF-TE spec, this can also be set even if no
	 * other bits are set, thus making this really another
	 * binary temperature sensor.
	 */

	SAFT_BAIL(r, hiwater, sdata, buflen);
	tempflags = sdata[r++];
	SAFT_BAIL(r, hiwater, sdata, buflen);
	tempflags |= (tempflags << 8) | sdata[r++];

	for (i = 0; i < NPSEUDO_THERM; i++) {
		ssc->ses_objmap[oid].encstat[1] = 0;
		if (tempflags & (1 << (NPSEUDO_THERM - i - 1))) {
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_CRIT;
			ssc->ses_objmap[4].encstat[2] = 0xff;
			/*
			 * Set 'over temperature' failure.
			 */
			ssc->ses_objmap[oid].encstat[3] = 8;
			ssc->ses_encstat |= SES_ENCSTAT_CRITICAL;
		} else {
			/*
			 * We used to say 'not available' and synthesize a
			 * nominal 30 deg (C)- that was wrong. Actually,
			 * Just say 'OK', and use the reserved value of
			 * zero.
			 */
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_OK;
			ssc->ses_objmap[oid].encstat[2] = 0;
			ssc->ses_objmap[oid].encstat[3] = 0;
		}
		ssc->ses_objmap[oid++].svalid = 1;
	}

	/*
	 * Get alarm status.
	 */
	ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_OK;
	ssc->ses_objmap[oid].encstat[3] = ssc->ses_objmap[oid].priv;
	ssc->ses_objmap[oid++].svalid = 1;

	/*
	 * Now get drive slot status
	 */
	cdb[2] = SAFTE_RD_RDDSTS;
	amt = buflen;
	err = ses_runcmd(ssc, cdb, 10, sdata, &amt);
	if (err) {
		SES_FREE(sdata, buflen);
		return (err);
	}
	hiwater = buflen - amt;
	for (r = i = 0; i < cc->Nslots; i++, r += 4) {
		SAFT_BAIL(r+3, hiwater, sdata, buflen);
		ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_UNSUPPORTED;
		ssc->ses_objmap[oid].encstat[1] = (uint8_t) i;
		ssc->ses_objmap[oid].encstat[2] = 0;
		ssc->ses_objmap[oid].encstat[3] = 0;
		status = sdata[r+3];
		if ((status & 0x1) == 0) {	/* no device */
			ssc->ses_objmap[oid].encstat[0] =
			    SES_OBJSTAT_NOTINSTALLED;
		} else {
			ssc->ses_objmap[oid].encstat[0] = SES_OBJSTAT_OK;
		}
		if (status & 0x2) {
			ssc->ses_objmap[oid].encstat[2] = 0x8;
		}
		if ((status & 0x4) == 0) {
			ssc->ses_objmap[oid].encstat[3] = 0x10;
		}
		ssc->ses_objmap[oid++].svalid = 1;
	}
	/* see comment below about sticky enclosure status */
	ssc->ses_encstat |= ENCI_SVALID | oencstat;
	SES_FREE(sdata, buflen);
	return (0);
}

static int
set_objstat_sel(ses_softc_t *ssc, ses_objstat *obp, int slp)
{
	int idx;
	encobj *ep;
	struct scfg *cc = ssc->ses_private;

	if (cc == NULL)
		return (0);

	idx = (int)obp->obj_id;
	ep = &ssc->ses_objmap[idx];

	switch (ep->enctype) {
	case SESTYP_DEVICE:
		if (obp->cstat[0] & SESCTL_PRDFAIL) {
			ep->priv |= 0x40;
		}
		/* SESCTL_RSTSWAP has no correspondence in SAF-TE */
		if (obp->cstat[0] & SESCTL_DISABLE) {
			ep->priv |= 0x80;
			/*
			 * Hmm. Try to set the 'No Drive' flag.
			 * Maybe that will count as a 'disable'.
			 */
		}
		if (ep->priv & 0xc6) {
			ep->priv &= ~0x1;
		} else {
			ep->priv |= 0x1;	/* no errors */
		}
		wrslot_stat(ssc, slp);
		break;
	case SESTYP_POWER:
		/*
		 * Okay- the only one that makes sense here is to
		 * do the 'disable' for a power supply.
		 */
		if (obp->cstat[0] & SESCTL_DISABLE) {
			(void) wrbuf16(ssc, SAFTE_WT_ACTPWS,
				idx - cc->pwroff, 0, 0, slp);
		}
		break;
	case SESTYP_FAN:
		/*
		 * Okay- the only one that makes sense here is to
		 * set fan speed to zero on disable.
		 */
		if (obp->cstat[0] & SESCTL_DISABLE) {
			/* remember- fans are the first items, so idx works */
			(void) wrbuf16(ssc, SAFTE_WT_FANSPD, idx, 0, 0, slp);
		}
		break;
	case SESTYP_DOORLOCK:
		/*
		 * Well, we can 'disable' the lock.
		 */
		if (obp->cstat[0] & SESCTL_DISABLE) {
			cc->flag2 &= ~SAFT_FLG2_LOCKDOOR;
			(void) wrbuf16(ssc, SAFTE_WT_GLOBAL, cc->flag1,
				cc->flag2, 0, slp);
		}
		break;
	case SESTYP_ALARM:
		/*
		 * Well, we can 'disable' the alarm.
		 */
		if (obp->cstat[0] & SESCTL_DISABLE) {
			cc->flag2 &= ~SAFT_FLG1_ALARM;
			ep->priv |= 0x40;	/* Muted */
			(void) wrbuf16(ssc, SAFTE_WT_GLOBAL, cc->flag1,
				cc->flag2, 0, slp);
		}
		break;
	default:
		break;
	}
	ep->svalid = 0;
	return (0);
}

/*
 * This function handles all of the 16 byte WRITE BUFFER commands.
 */
static int
wrbuf16(ses_softc_t *ssc, uint8_t op, uint8_t b1, uint8_t b2,
    uint8_t b3, int slp)
{
	int err, amt;
	char *sdata;
	struct scfg *cc = ssc->ses_private;
	static char cdb[10] = { WRITE_BUFFER, 1, 0, 0, 0, 0, 0, 0, 16, 0 };

	if (cc == NULL)
		return (0);

	sdata = SES_MALLOC(16);
	if (sdata == NULL)
		return (ENOMEM);

	SES_DLOG(ssc, "saf_wrbuf16 %x %x %x %x\n", op, b1, b2, b3);

	sdata[0] = op;
	sdata[1] = b1;
	sdata[2] = b2;
	sdata[3] = b3;
	MEMZERO(&sdata[4], 12);
	amt = -16;
	err = ses_runcmd(ssc, cdb, 10, sdata, &amt);
	SES_FREE(sdata, 16);
	return (err);
}

/*
 * This function updates the status byte for the device slot described.
 *
 * Since this is an optional SAF-TE command, there's no point in
 * returning an error.
 */
static void
wrslot_stat(ses_softc_t *ssc, int slp)
{
	int i, amt;
	encobj *ep;
	char cdb[10], *sdata;
	struct scfg *cc = ssc->ses_private;

	if (cc == NULL)
		return;

	SES_DLOG(ssc, "saf_wrslot\n");
	cdb[0] = WRITE_BUFFER;
	cdb[1] = 1;
	cdb[2] = 0;
	cdb[3] = 0;
	cdb[4] = 0;
	cdb[5] = 0;
	cdb[6] = 0;
	cdb[7] = 0;
	cdb[8] = cc->Nslots * 3 + 1;
	cdb[9] = 0;

	sdata = SES_MALLOC(cc->Nslots * 3 + 1);
	if (sdata == NULL)
		return;
	MEMZERO(sdata, cc->Nslots * 3 + 1);

	sdata[0] = SAFTE_WT_DSTAT;
	for (i = 0; i < cc->Nslots; i++) {
		ep = &ssc->ses_objmap[cc->slotoff + i];
		SES_DLOG(ssc, "saf_wrslot %d <- %x\n", i, ep->priv & 0xff);
		sdata[1 + (3 * i)] = ep->priv & 0xff;
	}
	amt = -(cc->Nslots * 3 + 1);
	(void) ses_runcmd(ssc, cdb, 10, sdata, &amt);
	SES_FREE(sdata, cc->Nslots * 3 + 1);
}

/*
 * This function issues the "PERFORM SLOT OPERATION" command.
 */
static int
perf_slotop(ses_softc_t *ssc, uint8_t slot, uint8_t opflag, int slp)
{
	int err, amt;
	char *sdata;
	struct scfg *cc = ssc->ses_private;
	static char cdb[10] =
	    { WRITE_BUFFER, 1, 0, 0, 0, 0, 0, 0, SAFT_SCRATCH, 0 };

	if (cc == NULL)
		return (0);

	sdata = SES_MALLOC(SAFT_SCRATCH);
	if (sdata == NULL)
		return (ENOMEM);
	MEMZERO(sdata, SAFT_SCRATCH);

	sdata[0] = SAFTE_WT_SLTOP;
	sdata[1] = slot;
	sdata[2] = opflag;
	SES_DLOG(ssc, "saf_slotop slot %d op %x\n", slot, opflag);
	amt = -SAFT_SCRATCH;
	err = ses_runcmd(ssc, cdb, 10, sdata, &amt);
	SES_FREE(sdata, SAFT_SCRATCH);
	return (err);
}
