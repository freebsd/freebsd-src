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
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/devicestat.h>
#include <machine/stdarg.h>

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
	u_int32_t
		enctype	: 8,		/* enclosure type */
		subenclosure : 8,	/* subenclosure id */
		svalid	: 1,		/* enclosure information valid */
		priv	: 15;		/* private data, per object */
	u_int8_t	encstat[4];	/* state && stats */
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

/*
 * Platform implementation defines/functions for SES internal kernel stuff
 */

#define	STRNCMP			strncmp
#define	PRINTF			printf
#define	SES_LOG			ses_log
#define	SES_VLOG		if (bootverbose) ses_log
#define	SES_MALLOC(amt)		malloc(amt, M_DEVBUF, M_NOWAIT)
#define	SES_FREE(ptr, amt)	free(ptr, M_DEVBUF)
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
	u_int32_t	ses_nobjects;	/* number of objects */
	ses_encstat	ses_encstat;	/* overall status */
	u_int8_t	ses_flags;
	union ccb	ses_saved_ccb;
	dev_t		ses_dev;
	struct cam_periph *periph;
};
#define	SES_FLAG_INVALID	0x01
#define	SES_FLAG_OPEN		0x02
#define	SES_FLAG_INITIALIZED	0x04

#define SESUNIT(x)       (minor((x)))
#define SES_CDEV_MAJOR	110

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
		cam_status status;
		struct ccb_getdev *cgd;

		cgd = (struct ccb_getdev *)arg;

		/*
		 * PROBLEM: WE NEED TO LOOK AT BYTES 48-53 TO SEE IF THIS IS
		 * PROBLEM: IS A SAF-TE DEVICE.
		 */
		switch (ses_type(&cgd->inq_data, sizeof (cgd->inq_data))) {
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
	struct ccb_setasync csa;
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

	softc = malloc(sizeof (struct ses_softc), M_DEVBUF, M_NOWAIT);
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
        case SES_SEN:
        case SES_SAFT:
		break;
	case SES_NONE:
	default:
		free(softc, M_DEVBUF);
		return (CAM_REQ_CMP_ERR);
	}

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

	s = splsoftcam();
	periph = cam_extend_get(sesperiphs, SESUNIT(dev));
	if (periph == NULL) {
		splx(s);
		return (ENXIO);
	}
	if ((error = cam_periph_lock(periph, PRIBIO | PCATCH)) != 0) {
		splx(s);
		return (error);
	}
	splx(s);

	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		cam_periph_unlock(periph);
		return (ENXIO);
	}

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
	if (error) {
		cam_periph_release(periph);
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
sesstart(struct cam_periph *p, union ccb *sccb)
{
	int s = splbio();
	if (p->immediate_priority <= p->pinfo.priority) {
		SLIST_INSERT_HEAD(&p->ccb_list, &sccb->ccb_h, periph_links.sle);
		p->immediate_priority = CAM_PRIORITY_NONE;
		wakeup(&p->ccb_list);
	}
	splx(s);
}

static void
sesdone(struct cam_periph *periph, union ccb *dccb)
{
	wakeup(&dccb->ccb_h.cbfcnp);
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
sesioctl(dev_t dev, u_long cmd, caddr_t arg_addr, int flag, struct proc *p)
{
	struct cam_periph *periph;
	ses_encstat tmp;
	ses_objstat objs;
	ses_object obj, *uobj;
	struct ses_softc *ssc;
	void *addr;
	int error, i;


	if (arg_addr)
		addr = *((caddr_t *) arg_addr);
	else
		addr = NULL;

	periph = cam_extend_get(sesperiphs, SESUNIT(dev));
	if (periph == NULL)
		return (ENXIO);

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("entering sesioctl\n"));

	ssc = (struct ses_softc *)periph->softc;

	/*
	 * Now check to see whether we're initialized or not.
	 */
	if ((ssc->ses_flags & SES_FLAG_INITIALIZED) == 0) {
		return (ENXIO);
	}

	error = 0;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE,
	    ("trying to do ioctl %#lx\n", cmd));

	/*
	 * If this command can change the device's state,
	 * we must have the device open for writing.
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
		for (uobj = addr, i = 0; i != ssc->ses_nobjects; i++, uobj++) {
			obj.obj_id = i;
			obj.subencid = ssc->ses_objmap[i].subenclosure;
			obj.object_type = ssc->ses_objmap[i].enctype;
			error = copyout(&obj, uobj, sizeof (ses_object));
			if (error) {
				break;
			}
		}
		break;

	case SESIOC_GETENCSTAT:
		error = (*ssc->ses_vec.get_encstat)(ssc, 1);
		if (error)
			break;
		error = copyout(&ssc->ses_encstat, addr, sizeof (ses_encstat));
		ssc->ses_encstat &= ~ENCI_SVALID;
		break;

	case SESIOC_SETENCSTAT:
		error = copyin(addr, &tmp, sizeof (ses_encstat));
		if (error)
			break;
		error = (*ssc->ses_vec.set_encstat)(ssc, tmp, 1);
		break;

	case SESIOC_GETOBJSTAT:
		error = copyin(addr, &objs, sizeof (ses_objstat));
		if (error)
			break;
		if (objs.obj_id >= ssc->ses_nobjects) {
			error = EINVAL;
			break;
		}
		error = (*ssc->ses_vec.get_objstat)(ssc, &objs, 1);
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
		error = (*ssc->ses_vec.set_objstat)(ssc, &objs, 1);

		/*
		 * Always (for now) invalidate entry.
		 */
		ssc->ses_objmap[objs.obj_id].svalid = 0;
		break;

	case SESIOC_INIT:

		error = (*ssc->ses_vec.init_enc)(ssc);
		break;

	default:
		error = cam_periph_ioctl(periph, cmd, arg_addr, seserror);
		break;
	}
	return (error);
}

#define	SES_FLAGS	SF_NO_PRINT | SF_RETRY_SELTO | SF_RETRY_UA
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

	error = cam_periph_runccb(ccb, seserror, 0, SES_FLAGS, NULL);
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
 * so forgive the slightly awkward and nocorming
 * appearance.
 */

/*
 * Is this a device that supports enclosure services?
 *
 * It's a a pretty simple ruleset- if it is device type 0x0D (13), it's
 * an SES device. If it happens to be an old UNISYS SEN device, we can
 * handle that too.
 */

static enctyp
ses_type(void *buf, int buflen)
{
	unsigned char *iqd = buf;

	if (buflen < 32)
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
	if (STRNCMP((char *)&iqd[44], "SAF-TE", 4) == 0) {
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
static int ses_enchdr(uint8_t *, int, u_char, SesEncHdr *);
static int ses_encdesc(uint8_t *, int, u_char, SesEncDesc *);
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
		SES_LOG(ssc, "Unable to parse SES Config Header");
		SES_FREE(sdata, SCSZ);
		return (EIO);
	}
	if (amt < SES_ENCHDR_MINLEN) {
		SES_LOG(ssc, "runt enclosure length (%d)", amt);
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
			SES_LOG(ssc, "Cannot Extract Enclosure Header %d", i);
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
ses_enchdr(uint8_t *buffer, int amt, u_char SubEncId, SesEncHdr *chp)
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
ses_encdesc(uint8_t *buffer, int amt, u_char SubEncId, SesEncDesc *cdp)
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
 *		then overal enclosure status is being sought.
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
