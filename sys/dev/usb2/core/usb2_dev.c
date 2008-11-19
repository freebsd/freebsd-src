/* $FreeBSD$ */
/*-
 * Copyright (c) 2006-2008 Hans Petter Selasky. All rights reserved.
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
 *
 * usb2_dev.c - An abstraction layer for creating devices under /dev/...
 */

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_ioctl.h>
#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>

#define	USB_DEBUG_VAR usb2_fifo_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_mbuf.h>
#include <dev/usb2/core/usb2_dev.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_generic.h>
#include <dev/usb2/core/usb2_dynamic.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>

#include <sys/filio.h>
#include <sys/ttycom.h>
#include <sys/syscallsubr.h>

#include <machine/stdarg.h>

#if USB_DEBUG
static int usb2_fifo_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, dev, CTLFLAG_RW, 0, "USB device");
SYSCTL_INT(_hw_usb2_dev, OID_AUTO, debug, CTLFLAG_RW,
    &usb2_fifo_debug, 0, "Debug Level");
#endif

#if ((__FreeBSD_version >= 700001) || (__FreeBSD_version == 0) || \
     ((__FreeBSD_version >= 600034) && (__FreeBSD_version < 700000)))
#define	USB_UCRED struct ucred *ucred,
#else
#define	USB_UCRED
#endif

/* prototypes */

static uint32_t usb2_path_convert_one(const char **pp);
static uint32_t usb2_path_convert(const char *path);
static int usb2_check_access(int fflags, struct usb2_perm *puser);
static int usb2_fifo_open(struct usb2_fifo *f, struct file *fp, struct thread *td, int fflags);
static void usb2_fifo_close(struct usb2_fifo *f, struct thread *td, int fflags);
static void usb2_dev_init(void *arg);
static void usb2_dev_init_post(void *arg);
static void usb2_dev_uninit(void *arg);
static int usb2_fifo_uiomove(struct usb2_fifo *f, void *cp, int n, struct uio *uio);
static void usb2_fifo_check_methods(struct usb2_fifo_methods *pm);
static void usb2_clone(void *arg, USB_UCRED char *name, int namelen, struct cdev **dev);
static struct usb2_fifo *usb2_fifo_alloc(void);
static struct usb2_pipe *usb2_dev_get_pipe(struct usb2_device *udev, uint8_t iface_index, uint8_t ep_index, uint8_t dir);

static d_fdopen_t usb2_fdopen;
static d_close_t usb2_close;
static d_ioctl_t usb2_ioctl;

static fo_rdwr_t usb2_read_f;
static fo_rdwr_t usb2_write_f;

#if __FreeBSD_version > 800009
static fo_truncate_t usb2_truncate_f;

#endif
static fo_ioctl_t usb2_ioctl_f;
static fo_poll_t usb2_poll_f;
static fo_kqfilter_t usb2_kqfilter_f;
static fo_stat_t usb2_stat_f;
static fo_close_t usb2_close_f;

static usb2_fifo_open_t usb2_fifo_dummy_open;
static usb2_fifo_close_t usb2_fifo_dummy_close;
static usb2_fifo_ioctl_t usb2_fifo_dummy_ioctl;
static usb2_fifo_cmd_t usb2_fifo_dummy_cmd;

static struct usb2_perm usb2_perm = {
	.uid = UID_ROOT,
	.gid = GID_OPERATOR,
	.mode = 0660,
};

static struct cdevsw usb2_devsw = {
	.d_version = D_VERSION,
	.d_fdopen = usb2_fdopen,
	.d_close = usb2_close,
	.d_ioctl = usb2_ioctl,
	.d_name = "usb",
	.d_flags = D_TRACKCLOSE,
};

static struct fileops usb2_ops_f = {
	.fo_read = usb2_read_f,
	.fo_write = usb2_write_f,
#if __FreeBSD_version > 800009
	.fo_truncate = usb2_truncate_f,
#endif
	.fo_ioctl = usb2_ioctl_f,
	.fo_poll = usb2_poll_f,
	.fo_kqfilter = usb2_kqfilter_f,
	.fo_stat = usb2_stat_f,
	.fo_close = usb2_close_f,
	.fo_flags = DFLAG_PASSABLE | DFLAG_SEEKABLE
};

static const dev_clone_fn usb2_clone_ptr = &usb2_clone;
static struct cdev *usb2_dev;
static uint32_t usb2_last_devloc = 0 - 1;
static eventhandler_tag usb2_clone_tag;
static void *usb2_old_f_data;
static struct fileops *usb2_old_f_ops;
static TAILQ_HEAD(, usb2_symlink) usb2_sym_head;
static struct sx usb2_sym_lock;

struct mtx usb2_ref_lock;

static uint32_t
usb2_path_convert_one(const char **pp)
{
	const char *ptr;
	uint32_t temp = 0;

	ptr = *pp;

	while ((*ptr >= '0') && (*ptr <= '9')) {
		temp *= 10;
		temp += (*ptr - '0');
		if (temp >= 1000000) {
			/* catch overflow early */
			return (0 - 1);
		}
		ptr++;
	}

	if (*ptr == '.') {
		/* skip dot */
		ptr++;
	}
	*pp = ptr;

	return (temp);
}

/*------------------------------------------------------------------------*
 *	usb2_path_convert
 *
 * Path format: "/dev/usb<bus>.<dev>.<iface>.<fifo>"
 *
 * Returns: Path converted into numerical format.
 *------------------------------------------------------------------------*/
static uint32_t
usb2_path_convert(const char *path)
{
	uint32_t temp;
	uint32_t devloc;

	devloc = 0;

	temp = usb2_path_convert_one(&path);

	if (temp >= USB_BUS_MAX) {
		return (0 - 1);
	}
	devloc += temp;

	temp = usb2_path_convert_one(&path);

	if (temp >= USB_DEV_MAX) {
		return (0 - 1);
	}
	devloc += (temp * USB_BUS_MAX);

	temp = usb2_path_convert_one(&path);

	if (temp >= USB_IFACE_MAX) {
		return (0 - 1);
	}
	devloc += (temp * USB_DEV_MAX * USB_BUS_MAX);

	temp = usb2_path_convert_one(&path);

	if (temp >= ((USB_FIFO_MAX / 2) + (USB_EP_MAX / 2))) {
		return (0 - 1);
	}
	devloc += (temp * USB_IFACE_MAX * USB_DEV_MAX * USB_BUS_MAX);

	return (devloc);
}

/*------------------------------------------------------------------------*
 *	usb2_set_iface_perm
 *
 * This function will set the interface permissions.
 *------------------------------------------------------------------------*/
void
usb2_set_iface_perm(struct usb2_device *udev, uint8_t iface_index,
    uint32_t uid, uint32_t gid, uint16_t mode)
{
	struct usb2_interface *iface;

	iface = usb2_get_iface(udev, iface_index);
	if (iface && iface->idesc) {
		mtx_lock(&usb2_ref_lock);
		iface->perm.uid = uid;
		iface->perm.gid = gid;
		iface->perm.mode = mode;
		mtx_unlock(&usb2_ref_lock);

	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_set_perm
 *
 * This function will set the permissions at the given level.
 *
 * Return values:
 *    0: Success.
 * Else: Failure.
 *------------------------------------------------------------------------*/
static int
usb2_set_perm(struct usb2_dev_perm *psrc, uint8_t level)
{
	struct usb2_location loc;
	struct usb2_perm *pdst;
	uint32_t devloc;
	int error;

	/* check if the current thread can change USB permissions. */
	error = priv_check(curthread, PRIV_ROOT);
	if (error) {
		return (error);
	}
	/* range check device location */
	if ((psrc->bus_index >= USB_BUS_MAX) ||
	    (psrc->dev_index >= USB_DEV_MAX) ||
	    (psrc->iface_index >= USB_IFACE_MAX)) {
		return (EINVAL);
	}
	if (level == 1)
		devloc = USB_BUS_MAX;	/* use root-HUB to access bus */
	else
		devloc = 0;
	switch (level) {
	case 3:
		devloc += psrc->iface_index *
		    USB_DEV_MAX * USB_BUS_MAX;
		/* FALLTHROUGH */
	case 2:
		devloc += psrc->dev_index *
		    USB_BUS_MAX;
		/* FALLTHROUGH */
	case 1:
		devloc += psrc->bus_index;
		break;
	default:
		break;
	}

	if ((level > 0) && (level < 4)) {
		error = usb2_ref_device(NULL, &loc, devloc);
		if (error) {
			return (error);
		}
	}
	switch (level) {
	case 3:
		if (loc.iface == NULL) {
			usb2_unref_device(&loc);
			return (EINVAL);
		}
		pdst = &loc.iface->perm;
		break;
	case 2:
		pdst = &loc.udev->perm;
		break;
	case 1:
		pdst = &loc.bus->perm;
		break;
	default:
		pdst = &usb2_perm;
		break;
	}

	/* all permissions are protected by "usb2_ref_lock" */
	mtx_lock(&usb2_ref_lock);
	pdst->uid = psrc->user_id;
	pdst->gid = psrc->group_id;
	pdst->mode = psrc->mode;
	mtx_unlock(&usb2_ref_lock);

	if ((level > 0) && (level < 4)) {
		usb2_unref_device(&loc);
	}
	return (0);			/* success */
}

/*------------------------------------------------------------------------*
 *	usb2_get_perm
 *
 * This function will get the permissions at the given level.
 *
 * Return values:
 *    0: Success.
 * Else: Failure.
 *------------------------------------------------------------------------*/
static int
usb2_get_perm(struct usb2_dev_perm *pdst, uint8_t level)
{
	struct usb2_location loc;
	struct usb2_perm *psrc;
	uint32_t devloc;
	int error;

	if ((pdst->bus_index >= USB_BUS_MAX) ||
	    (pdst->dev_index >= USB_DEV_MAX) ||
	    (pdst->iface_index >= USB_IFACE_MAX)) {
		return (EINVAL);
	}
	if (level == 1)
		devloc = USB_BUS_MAX;	/* use root-HUB to access bus */
	else
		devloc = 0;
	switch (level) {
	case 3:
		devloc += pdst->iface_index *
		    USB_DEV_MAX * USB_BUS_MAX;
		/* FALLTHROUGH */
	case 2:
		devloc += pdst->dev_index *
		    USB_BUS_MAX;
		/* FALLTHROUGH */
	case 1:
		devloc += pdst->bus_index;
		break;
	default:
		break;
	}

	if ((level > 0) && (level < 4)) {
		error = usb2_ref_device(NULL, &loc, devloc);
		if (error) {
			return (error);
		}
	}
	switch (level) {
	case 3:
		if (loc.iface == NULL) {
			usb2_unref_device(&loc);
			return (EINVAL);
		}
		psrc = &loc.iface->perm;
		break;
	case 2:
		psrc = &loc.udev->perm;
		break;
	case 1:
		psrc = &loc.bus->perm;
		break;
	default:
		psrc = &usb2_perm;
		break;
	}

	/* all permissions are protected by "usb2_ref_lock" */
	mtx_lock(&usb2_ref_lock);
	if (psrc->mode != 0) {
		pdst->user_id = psrc->uid;
		pdst->group_id = psrc->gid;
		pdst->mode = psrc->mode;
	} else {
		/* access entry at this level and location is not active */
		pdst->user_id = 0;
		pdst->group_id = 0;
		pdst->mode = 0;
	}
	mtx_unlock(&usb2_ref_lock);

	if ((level > 0) && (level < 4)) {
		usb2_unref_device(&loc);
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_check_access
 *
 * This function will verify the given access information.
 *
 * Return values:
 * 0: Access granted.
 * Else: No access granted.
 *------------------------------------------------------------------------*/
static int
usb2_check_access(int fflags, struct usb2_perm *puser)
{
	mode_t accmode;

	if ((fflags & (FWRITE | FREAD)) && (puser->mode != 0)) {
		/* continue */
	} else {
		return (EPERM);		/* no access */
	}

	accmode = 0;
	if (fflags & FWRITE)
		accmode |= VWRITE;
	if (fflags & FREAD)
		accmode |= VREAD;

	return (vaccess(VCHR, puser->mode, puser->uid,
	    puser->gid, accmode, curthread->td_ucred, NULL));
}

/*------------------------------------------------------------------------*
 *	usb2_ref_device
 *
 * This function is used to atomically refer an USB device by its
 * device location. If this function returns success the USB device
 * will not dissappear until the USB device is unreferenced.
 *
 * Return values:
 *  0: Success, refcount incremented on the given USB device.
 *  Else: Failure.
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_ref_device(struct file *fp, struct usb2_location *ploc, uint32_t devloc)
{
	struct usb2_fifo **ppf;
	struct usb2_fifo *f;
	int fflags;
	uint8_t dev_ep_index;

	if (fp) {
		/* check if we need uref */
		ploc->is_uref = devloc ? 0 : 1;
		/* get devloc - already verified */
		devloc = USB_P2U(fp->f_data);
		/* get file flags */
		fflags = fp->f_flag;
	} else {
		/* only ref device */
		fflags = 0;
		/* search for FIFO */
		ploc->is_uref = 1;
		/* check "devloc" */
		if (devloc >= (USB_BUS_MAX * USB_DEV_MAX *
		    USB_IFACE_MAX * ((USB_EP_MAX / 2) + (USB_FIFO_MAX / 2)))) {
			return (USB_ERR_INVAL);
		}
	}

	/* store device location */
	ploc->devloc = devloc;
	ploc->bus_index = devloc % USB_BUS_MAX;
	ploc->dev_index = (devloc / USB_BUS_MAX) % USB_DEV_MAX;
	ploc->iface_index = (devloc / (USB_BUS_MAX *
	    USB_DEV_MAX)) % USB_IFACE_MAX;
	ploc->fifo_index = (devloc / (USB_BUS_MAX * USB_DEV_MAX *
	    USB_IFACE_MAX));

	mtx_lock(&usb2_ref_lock);
	ploc->bus = devclass_get_softc(usb2_devclass_ptr, ploc->bus_index);
	if (ploc->bus == NULL) {
		DPRINTFN(2, "no bus at %u\n", ploc->bus_index);
		goto error;
	}
	if (ploc->dev_index >= ploc->bus->devices_max) {
		DPRINTFN(2, "invalid dev index, %u\n", ploc->dev_index);
		goto error;
	}
	ploc->udev = ploc->bus->devices[ploc->dev_index];
	if (ploc->udev == NULL) {
		DPRINTFN(2, "no device at %u\n", ploc->dev_index);
		goto error;
	}
	if (ploc->udev->refcount == USB_DEV_REF_MAX) {
		DPRINTFN(2, "no dev ref\n");
		goto error;
	}
	/* check if we are doing an open */
	if (fp == NULL) {
		/* set defaults */
		ploc->txfifo = NULL;
		ploc->rxfifo = NULL;
		ploc->is_write = 0;
		ploc->is_read = 0;
		ploc->is_usbfs = 0;
		/* NOTE: variable overloading: */
		dev_ep_index = ploc->fifo_index;
	} else {
		/* initialise "is_usbfs" flag */
		ploc->is_usbfs = 0;
		dev_ep_index = 255;	/* dummy */

		/* check for write */
		if (fflags & FWRITE) {
			ppf = ploc->udev->fifo;
			f = ppf[ploc->fifo_index + USB_FIFO_TX];
			ploc->txfifo = f;
			ploc->is_write = 1;	/* ref */
			if ((f == NULL) ||
			    (f->refcount == USB_FIFO_REF_MAX) ||
			    (f->curr_file != fp)) {
				goto error;
			}
			/* check if USB-FS is active */
			if (f->fs_ep_max != 0) {
				ploc->is_usbfs = 1;
			}
			/*
			 * Get real endpoint index associated with
			 * this FIFO:
			 */
			dev_ep_index = f->dev_ep_index;
		} else {
			ploc->txfifo = NULL;
			ploc->is_write = 0;	/* no ref */
		}

		/* check for read */
		if (fflags & FREAD) {
			ppf = ploc->udev->fifo;
			f = ppf[ploc->fifo_index + USB_FIFO_RX];
			ploc->rxfifo = f;
			ploc->is_read = 1;	/* ref */
			if ((f == NULL) ||
			    (f->refcount == USB_FIFO_REF_MAX) ||
			    (f->curr_file != fp)) {
				goto error;
			}
			/* check if USB-FS is active */
			if (f->fs_ep_max != 0) {
				ploc->is_usbfs = 1;
			}
			/*
			 * Get real endpoint index associated with
			 * this FIFO:
			 */
			dev_ep_index = f->dev_ep_index;
		} else {
			ploc->rxfifo = NULL;
			ploc->is_read = 0;	/* no ref */
		}
	}

	/* check if we require an interface */
	ploc->iface = usb2_get_iface(ploc->udev, ploc->iface_index);
	if (dev_ep_index != 0) {
		/* non control endpoint - we need an interface */
		if (ploc->iface == NULL) {
			DPRINTFN(2, "no iface\n");
			goto error;
		}
		if (ploc->iface->idesc == NULL) {
			DPRINTFN(2, "no idesc\n");
			goto error;
		}
	}
	/* when everything is OK we increment the refcounts */
	if (ploc->is_write) {
		DPRINTFN(2, "ref write\n");
		ploc->txfifo->refcount++;
	}
	if (ploc->is_read) {
		DPRINTFN(2, "ref read\n");
		ploc->rxfifo->refcount++;
	}
	if (ploc->is_uref) {
		DPRINTFN(2, "ref udev - needed\n");
		ploc->udev->refcount++;
	}
	mtx_unlock(&usb2_ref_lock);

	if (ploc->is_uref) {
		/*
		 * We are about to alter the bus-state. Apply the
		 * required locks.
		 */
		sx_xlock(ploc->udev->default_sx + 1);
		mtx_lock(&Giant);	/* XXX */
	}
	return (0);

error:
	mtx_unlock(&usb2_ref_lock);
	DPRINTFN(2, "fail\n");
	return (USB_ERR_INVAL);
}

/*------------------------------------------------------------------------*
 *	usb2_uref_location
 *
 * This function is used to upgrade an USB reference to include the
 * USB device reference on a USB location.
 *
 * Return values:
 *  0: Success, refcount incremented on the given USB device.
 *  Else: Failure.
 *------------------------------------------------------------------------*/
static usb2_error_t
usb2_uref_location(struct usb2_location *ploc)
{
	/*
	 * Check if we already got an USB reference on this location:
	 */
	if (ploc->is_uref) {
		return (0);		/* success */
	}
	mtx_lock(&usb2_ref_lock);
	if (ploc->bus != devclass_get_softc(usb2_devclass_ptr, ploc->bus_index)) {
		DPRINTFN(2, "bus changed at %u\n", ploc->bus_index);
		goto error;
	}
	if (ploc->udev != ploc->bus->devices[ploc->dev_index]) {
		DPRINTFN(2, "device changed at %u\n", ploc->dev_index);
		goto error;
	}
	if (ploc->udev->refcount == USB_DEV_REF_MAX) {
		DPRINTFN(2, "no dev ref\n");
		goto error;
	}
	DPRINTFN(2, "ref udev\n");
	ploc->udev->refcount++;
	mtx_unlock(&usb2_ref_lock);

	/* set "uref" */
	ploc->is_uref = 1;

	/*
	 * We are about to alter the bus-state. Apply the
	 * required locks.
	 */
	sx_xlock(ploc->udev->default_sx + 1);
	mtx_lock(&Giant);		/* XXX */
	return (0);

error:
	mtx_unlock(&usb2_ref_lock);
	DPRINTFN(2, "fail\n");
	return (USB_ERR_INVAL);
}

/*------------------------------------------------------------------------*
 *	usb2_unref_device
 *
 * This function will release the reference count by one unit for the
 * given USB device.
 *------------------------------------------------------------------------*/
void
usb2_unref_device(struct usb2_location *ploc)
{
	if (ploc->is_uref) {
		mtx_unlock(&Giant);	/* XXX */
		sx_unlock(ploc->udev->default_sx + 1);
	}
	mtx_lock(&usb2_ref_lock);
	if (ploc->is_read) {
		if (--(ploc->rxfifo->refcount) == 0) {
			usb2_cv_signal(&ploc->rxfifo->cv_drain);
		}
	}
	if (ploc->is_write) {
		if (--(ploc->txfifo->refcount) == 0) {
			usb2_cv_signal(&ploc->txfifo->cv_drain);
		}
	}
	if (ploc->is_uref) {
		if (--(ploc->udev->refcount) == 0) {
			usb2_cv_signal(ploc->udev->default_cv + 1);
		}
	}
	mtx_unlock(&usb2_ref_lock);
	return;
}

static struct usb2_fifo *
usb2_fifo_alloc(void)
{
	struct usb2_fifo *f;

	f = malloc(sizeof(*f), M_USBDEV, M_WAITOK | M_ZERO);
	if (f) {
		usb2_cv_init(&f->cv_io, "FIFO-IO");
		usb2_cv_init(&f->cv_drain, "FIFO-DRAIN");
		f->refcount = 1;
	}
	return (f);
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_create
 *------------------------------------------------------------------------*/
static int
usb2_fifo_create(struct usb2_location *ploc, uint32_t *pdevloc, int fflags)
{
	struct usb2_device *udev = ploc->udev;
	struct usb2_fifo *f;
	struct usb2_pipe *pipe;
	uint8_t iface_index = ploc->iface_index;

	/* NOTE: variable overloading: */
	uint8_t dev_ep_index = ploc->fifo_index;
	uint8_t n;
	uint8_t is_tx;
	uint8_t is_rx;
	uint8_t no_null;
	uint8_t is_busy;

	is_tx = (fflags & FWRITE) ? 1 : 0;
	is_rx = (fflags & FREAD) ? 1 : 0;
	no_null = 1;
	is_busy = 0;

	/* search for a free FIFO slot */

	for (n = 0;; n += 2) {

		if (n == USB_FIFO_MAX) {
			if (no_null) {
				no_null = 0;
				n = 0;
			} else {
				/* end of FIFOs reached */
				return (ENOMEM);
			}
		}
		/* Check for TX FIFO */
		if (is_tx) {
			f = udev->fifo[n + USB_FIFO_TX];
			if (f != NULL) {
				if (f->dev_ep_index != dev_ep_index) {
					/* wrong endpoint index */
					continue;
				}
				if ((dev_ep_index != 0) &&
				    (f->iface_index != iface_index)) {
					/* wrong interface index */
					continue;
				}
				if (f->curr_file != NULL) {
					/* FIFO is opened */
					is_busy = 1;
					continue;
				}
			} else if (no_null) {
				continue;
			}
		}
		/* Check for RX FIFO */
		if (is_rx) {
			f = udev->fifo[n + USB_FIFO_RX];
			if (f != NULL) {
				if (f->dev_ep_index != dev_ep_index) {
					/* wrong endpoint index */
					continue;
				}
				if ((dev_ep_index != 0) &&
				    (f->iface_index != iface_index)) {
					/* wrong interface index */
					continue;
				}
				if (f->curr_file != NULL) {
					/* FIFO is opened */
					is_busy = 1;
					continue;
				}
			} else if (no_null) {
				continue;
			}
		}
		break;
	}

	if (no_null == 0) {
		if (dev_ep_index >= (USB_EP_MAX / 2)) {
			/* we don't create any endpoints in this range */
			return (is_busy ? EBUSY : EINVAL);
		}
	}
	/* Check TX FIFO */
	if (is_tx &&
	    (udev->fifo[n + USB_FIFO_TX] == NULL)) {
		pipe = usb2_dev_get_pipe(udev,
		    iface_index, dev_ep_index, USB_FIFO_TX);
		if (pipe == NULL) {
			return (EINVAL);
		}
		f = usb2_fifo_alloc();
		if (f == NULL) {
			return (ENOMEM);
		}
		/* update some fields */
		f->fifo_index = n + USB_FIFO_TX;
		f->dev_ep_index = dev_ep_index;
		f->priv_mtx = udev->default_mtx;
		f->priv_sc0 = pipe;
		f->methods = &usb2_ugen_methods;
		f->iface_index = iface_index;
		f->udev = udev;
		mtx_lock(&usb2_ref_lock);
		udev->fifo[n + USB_FIFO_TX] = f;
		mtx_unlock(&usb2_ref_lock);
	}
	/* Check RX FIFO */
	if (is_rx &&
	    (udev->fifo[n + USB_FIFO_RX] == NULL)) {

		pipe = usb2_dev_get_pipe(udev,
		    iface_index, dev_ep_index, USB_FIFO_RX);
		if (pipe == NULL) {
			return (EINVAL);
		}
		f = usb2_fifo_alloc();
		if (f == NULL) {
			return (ENOMEM);
		}
		/* update some fields */
		f->fifo_index = n + USB_FIFO_RX;
		f->dev_ep_index = dev_ep_index;
		f->priv_mtx = udev->default_mtx;
		f->priv_sc0 = pipe;
		f->methods = &usb2_ugen_methods;
		f->iface_index = iface_index;
		f->udev = udev;
		mtx_lock(&usb2_ref_lock);
		udev->fifo[n + USB_FIFO_RX] = f;
		mtx_unlock(&usb2_ref_lock);
	}
	if (is_tx) {
		ploc->txfifo = udev->fifo[n + USB_FIFO_TX];
	}
	if (is_rx) {
		ploc->rxfifo = udev->fifo[n + USB_FIFO_RX];
	}
	/* replace endpoint index by FIFO index */

	(*pdevloc) %= (USB_BUS_MAX * USB_DEV_MAX * USB_IFACE_MAX);
	(*pdevloc) += (USB_BUS_MAX * USB_DEV_MAX * USB_IFACE_MAX) * n;

	/* complete */

	return (0);
}

void
usb2_fifo_free(struct usb2_fifo *f)
{
	uint8_t n;

	if (f == NULL) {
		/* be NULL safe */
		return;
	}
	/* destroy symlink devices, if any */
	for (n = 0; n != 2; n++) {
		if (f->symlink[n]) {
			usb2_free_symlink(f->symlink[n]);
			f->symlink[n] = NULL;
		}
	}
	mtx_lock(&usb2_ref_lock);

	/* delink ourselves to stop calls from userland */
	if ((f->fifo_index < USB_FIFO_MAX) &&
	    (f->udev != NULL) &&
	    (f->udev->fifo[f->fifo_index] == f)) {
		f->udev->fifo[f->fifo_index] = NULL;
	} else {
		DPRINTFN(0, "USB FIFO %p has not been linked!\n", f);
	}

	/* decrease refcount */
	f->refcount--;
	/* prevent any write flush */
	f->flag_iserror = 1;
	/* need to wait until all callers have exited */
	while (f->refcount != 0) {
		mtx_unlock(&usb2_ref_lock);	/* avoid LOR */
		mtx_lock(f->priv_mtx);
		/* get I/O thread out of any sleep state */
		if (f->flag_sleeping) {
			f->flag_sleeping = 0;
			usb2_cv_broadcast(&f->cv_io);
		}
		mtx_unlock(f->priv_mtx);
		mtx_lock(&usb2_ref_lock);

		/* wait for sync */
		usb2_cv_wait(&f->cv_drain, &usb2_ref_lock);
	}
	mtx_unlock(&usb2_ref_lock);

	/* take care of closing the device here, if any */
	usb2_fifo_close(f, curthread, 0);

	usb2_cv_destroy(&f->cv_io);
	usb2_cv_destroy(&f->cv_drain);

	free(f, M_USBDEV);
	return;
}

static struct usb2_pipe *
usb2_dev_get_pipe(struct usb2_device *udev,
    uint8_t iface_index, uint8_t ep_index, uint8_t dir)
{
	struct usb2_pipe *pipe;
	uint8_t ep_dir;

	if (ep_index == 0) {
		pipe = &udev->default_pipe;
	} else {
		if (dir == USB_FIFO_RX) {
			if (udev->flags.usb2_mode == USB_MODE_HOST) {
				ep_dir = UE_DIR_IN;
			} else {
				ep_dir = UE_DIR_OUT;
			}
		} else {
			if (udev->flags.usb2_mode == USB_MODE_HOST) {
				ep_dir = UE_DIR_OUT;
			} else {
				ep_dir = UE_DIR_IN;
			}
		}
		pipe = usb2_get_pipe_by_addr(udev, ep_index | ep_dir);
	}

	if (pipe == NULL) {
		/* if the pipe does not exist then return */
		return (NULL);
	}
	if (pipe->edesc == NULL) {
		/* invalid pipe */
		return (NULL);
	}
	if (ep_index != 0) {
		if (pipe->iface_index != iface_index) {
			/*
			 * Permissions violation - trying to access a
			 * pipe that does not belong to the interface.
			 */
			return (NULL);
		}
	}
	return (pipe);			/* success */
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_open
 *
 * Returns:
 * 0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static int
usb2_fifo_open(struct usb2_fifo *f, struct file *fp, struct thread *td,
    int fflags)
{
	int err;

	if (f == NULL) {
		/* no FIFO there */
		DPRINTFN(2, "no FIFO\n");
		return (ENXIO);
	}
	/* remove FWRITE and FREAD flags */
	fflags &= ~(FWRITE | FREAD);

	/* set correct file flags */
	if ((f->fifo_index & 1) == USB_FIFO_TX) {
		fflags |= FWRITE;
	} else {
		fflags |= FREAD;
	}

	/* check if we are already opened */
	/* we don't need any locks when checking this variable */
	if (f->curr_file) {
		err = EBUSY;
		goto done;
	}
	/* call open method */
	err = (f->methods->f_open) (f, fflags, td);
	if (err) {
		goto done;
	}
	mtx_lock(f->priv_mtx);

	/* reset sleep flag */
	f->flag_sleeping = 0;

	/* reset error flag */
	f->flag_iserror = 0;

	/* reset complete flag */
	f->flag_iscomplete = 0;

	/* reset select flag */
	f->flag_isselect = 0;

	/* reset flushing flag */
	f->flag_flushing = 0;

	/* reset ASYNC proc flag */
	f->async_p = NULL;

	/* set which file we belong to */
	mtx_lock(&usb2_ref_lock);
	f->curr_file = fp;
	mtx_unlock(&usb2_ref_lock);

	/* reset queue */
	usb2_fifo_reset(f);

	mtx_unlock(f->priv_mtx);
done:
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_reset
 *------------------------------------------------------------------------*/
void
usb2_fifo_reset(struct usb2_fifo *f)
{
	struct usb2_mbuf *m;

	if (f == NULL) {
		return;
	}
	while (1) {
		USB_IF_DEQUEUE(&f->used_q, m);
		if (m) {
			USB_IF_ENQUEUE(&f->free_q, m);
		} else {
			break;
		}
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_close
 *------------------------------------------------------------------------*/
static void
usb2_fifo_close(struct usb2_fifo *f, struct thread *td, int fflags)
{
	int err;

	/* check if we are not opened */
	if (!f->curr_file) {
		/* nothing to do - already closed */
		return;
	}
	mtx_lock(f->priv_mtx);

	/* clear current file flag */
	f->curr_file = NULL;

	/* check if we are selected */
	if (f->flag_isselect) {
		selwakeup(&f->selinfo);
		f->flag_isselect = 0;
	}
	/* check if a thread wants SIGIO */
	if (f->async_p != NULL) {
		PROC_LOCK(f->async_p);
		psignal(f->async_p, SIGIO);
		PROC_UNLOCK(f->async_p);
		f->async_p = NULL;
	}
	/* remove FWRITE and FREAD flags */
	fflags &= ~(FWRITE | FREAD);

	/* flush written data, if any */
	if ((f->fifo_index & 1) == USB_FIFO_TX) {

		if (!f->flag_iserror) {

			/* set flushing flag */
			f->flag_flushing = 1;

			/* start write transfer, if not already started */
			(f->methods->f_start_write) (f);

			/* check if flushed already */
			while (f->flag_flushing &&
			    (!f->flag_iserror)) {
				/* wait until all data has been written */
				f->flag_sleeping = 1;
				err = usb2_cv_wait_sig(&f->cv_io, f->priv_mtx);
				if (err) {
					DPRINTF("signal received\n");
					break;
				}
			}
		}
		fflags |= FWRITE;

		/* stop write transfer, if not already stopped */
		(f->methods->f_stop_write) (f);
	} else {
		fflags |= FREAD;

		/* stop write transfer, if not already stopped */
		(f->methods->f_stop_read) (f);
	}

	/* check if we are sleeping */
	if (f->flag_sleeping) {
		DPRINTFN(2, "Sleeping at close!\n");
	}
	mtx_unlock(f->priv_mtx);

	/* call close method */
	(f->methods->f_close) (f, fflags, td);

	DPRINTF("closed\n");
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_check_thread_perm
 *
 * Returns:
 * 0: Has permission.
 * Else: No permission.
 *------------------------------------------------------------------------*/
int
usb2_check_thread_perm(struct usb2_device *udev, struct thread *td,
    int fflags, uint8_t iface_index, uint8_t ep_index)
{
	struct usb2_interface *iface;
	int err;

	if (ep_index != 0) {
		/*
		 * Non-control endpoints are always
		 * associated with an interface:
		 */
		iface = usb2_get_iface(udev, iface_index);
		if (iface == NULL) {
			return (EINVAL);
		}
		if (iface->idesc == NULL) {
			return (EINVAL);
		}
	} else {
		iface = NULL;
	}
	/* scan down the permissions tree */
	if ((iface != NULL) &&
	    (usb2_check_access(fflags, &iface->perm) == 0)) {
		/* we got access through the interface */
		err = 0;
	} else if (udev &&
	    (usb2_check_access(fflags, &udev->perm) == 0)) {
		/* we got access through the device */
		err = 0;
	} else if (udev->bus &&
	    (usb2_check_access(fflags, &udev->bus->perm) == 0)) {
		/* we got access through the USB bus */
		err = 0;
	} else if (usb2_check_access(fflags, &usb2_perm) == 0) {
		/* we got general access */
		err = 0;
	} else {
		/* no access */
		err = EPERM;
	}
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_fdopen - cdev callback
 *------------------------------------------------------------------------*/
static int
usb2_fdopen(struct cdev *dev, int xxx_oflags, struct thread *td,
    struct file *fp)
{
	struct usb2_location loc;
	uint32_t devloc;
	int err;
	int fflags;

	DPRINTFN(2, "oflags=0x%08x\n", xxx_oflags);

	devloc = usb2_last_devloc;
	usb2_last_devloc = (0 - 1);	/* reset "usb2_last_devloc" */

	if (fp == NULL) {
		DPRINTFN(2, "fp == NULL\n");
		return (ENXIO);
	}
	if (usb2_old_f_data != fp->f_data) {
		if (usb2_old_f_data != NULL) {
			DPRINTFN(0, "File data mismatch!\n");
			return (ENXIO);
		}
		usb2_old_f_data = fp->f_data;
	}
	if (usb2_old_f_ops != fp->f_ops) {
		if (usb2_old_f_ops != NULL) {
			DPRINTFN(0, "File ops mismatch!\n");
			return (ENXIO);
		}
		usb2_old_f_ops = fp->f_ops;
	}
	fflags = fp->f_flag;
	DPRINTFN(2, "fflags=0x%08x\n", fflags);

	if (!(fflags & (FREAD | FWRITE))) {
		/* should not happen */
		return (EPERM);
	}
	if (devloc == (uint32_t)(0 - 2)) {
		/* tried to open "/dev/usb" */
		return (0);
	} else if (devloc == (uint32_t)(0 - 1)) {
		/* tried to open "/dev/usb " */
		DPRINTFN(2, "no devloc\n");
		return (ENXIO);
	}
	err = usb2_ref_device(NULL, &loc, devloc);
	if (err) {
		DPRINTFN(2, "cannot ref device\n");
		return (ENXIO);
	}
	/*
	 * NOTE: Variable overloading. "usb2_fifo_create" will update
	 * the FIFO index. Right here we can assume that the
	 * "fifo_index" is the same like the endpoint number without
	 * direction mask, if the "fifo_index" is less than 16.
	 */
	err = usb2_check_thread_perm(loc.udev, td, fflags,
	    loc.iface_index, loc.fifo_index);

	/* check for error */
	if (err) {
		usb2_unref_device(&loc);
		return (err);
	}
	/* create FIFOs, if any */
	err = usb2_fifo_create(&loc, &devloc, fflags);
	/* check for error */
	if (err) {
		usb2_unref_device(&loc);
		return (err);
	}
	if (fflags & FREAD) {
		err = usb2_fifo_open(loc.rxfifo, fp, td, fflags);
		if (err) {
			DPRINTFN(2, "read open failed\n");
			usb2_unref_device(&loc);
			return (err);
		}
	}
	if (fflags & FWRITE) {
		err = usb2_fifo_open(loc.txfifo, fp, td, fflags);
		if (err) {
			DPRINTFN(2, "write open failed\n");
			if (fflags & FREAD) {
				usb2_fifo_close(loc.rxfifo, td,
				    fflags);
			}
			usb2_unref_device(&loc);
			return (err);
		}
	}
	/*
	 * Take over the file so that we get all the callbacks
	 * directly and don't have to create another device:
	 */
	finit(fp, fp->f_flag, DTYPE_VNODE,
	    ((uint8_t *)0) + devloc, &usb2_ops_f);

	usb2_unref_device(&loc);

	DPRINTFN(2, "error=%d\n", err);

	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_close - cdev callback
 *------------------------------------------------------------------------*/
static int
usb2_close(struct cdev *dev, int flag, int mode, struct thread *p)
{
	DPRINTF("\n");
	return (0);			/* nothing to do */
}

/*------------------------------------------------------------------------*
 *	usb2_close - cdev callback
 *------------------------------------------------------------------------*/
static int
usb2_ioctl(struct cdev *dev, u_long cmd, caddr_t data,
    int fflag, struct thread *td)
{
	union {
		struct usb2_read_dir *urd;
		struct usb2_dev_perm *udp;
		void   *data;
	}     u;
	int err;

	u.data = data;

	switch (cmd) {
	case USB_READ_DIR:
		err = usb2_read_symlink(u.urd->urd_data,
		    u.urd->urd_startentry, u.urd->urd_maxlen);
		break;
	case USB_SET_IFACE_PERM:
		err = usb2_set_perm(u.udp, 3);
		break;
	case USB_SET_DEVICE_PERM:
		err = usb2_set_perm(u.udp, 2);
		break;
	case USB_SET_BUS_PERM:
		err = usb2_set_perm(u.udp, 1);
		break;
	case USB_SET_ROOT_PERM:
		err = usb2_set_perm(u.udp, 0);
		break;
	case USB_GET_IFACE_PERM:
		err = usb2_get_perm(u.udp, 3);
		break;
	case USB_GET_DEVICE_PERM:
		err = usb2_get_perm(u.udp, 2);
		break;
	case USB_GET_BUS_PERM:
		err = usb2_get_perm(u.udp, 1);
		break;
	case USB_GET_ROOT_PERM:
		err = usb2_get_perm(u.udp, 0);
		break;
	case USB_DEV_QUIRK_GET:
	case USB_QUIRK_NAME_GET:
	case USB_DEV_QUIRK_ADD:
	case USB_DEV_QUIRK_REMOVE:
		err = usb2_quirk_ioctl_p(cmd, data, fflag, td);
		break;
	default:
		err = ENOTTY;
		break;
	}
	return (err);
}

/*------------------------------------------------------------------------*
 *      usb2_clone - cdev callback
 *
 * This function is the kernel clone callback for "/dev/usbX.Y".
 *
 * NOTE: This function assumes that the clone and device open
 * operation is atomic.
 *------------------------------------------------------------------------*/
static void
usb2_clone(void *arg, USB_UCRED char *name, int namelen, struct cdev **dev)
{
	enum {
		USB_DNAME_LEN = sizeof(USB_DEVICE_NAME) - 1,
		USB_GNAME_LEN = sizeof(USB_GENERIC_NAME) - 1,
	};

	if (*dev) {
		/* someone else has created a device */
		return;
	}
	/* reset device location */
	usb2_last_devloc = (uint32_t)(0 - 1);

	/*
	 * Check if we are matching "usb", "ugen" or an internal
	 * symbolic link:
	 */
	if ((namelen >= USB_DNAME_LEN) &&
	    (bcmp(name, USB_DEVICE_NAME, USB_DNAME_LEN) == 0)) {
		if (namelen == USB_DNAME_LEN) {
			/* USB management device location */
			usb2_last_devloc = (uint32_t)(0 - 2);
		} else {
			/* USB endpoint */
			usb2_last_devloc =
			    usb2_path_convert(name + USB_DNAME_LEN);
		}
	} else if ((namelen >= USB_GNAME_LEN) &&
	    (bcmp(name, USB_GENERIC_NAME, USB_GNAME_LEN) == 0)) {
		if (namelen == USB_GNAME_LEN) {
			/* USB management device location */
			usb2_last_devloc = (uint32_t)(0 - 2);
		} else {
			/* USB endpoint */
			usb2_last_devloc =
			    usb2_path_convert(name + USB_GNAME_LEN);
		}
	}
	if (usb2_last_devloc == (uint32_t)(0 - 1)) {
		/* Search for symbolic link */
		usb2_last_devloc =
		    usb2_lookup_symlink(name, namelen);
	}
	if (usb2_last_devloc == (uint32_t)(0 - 1)) {
		/* invalid location */
		return;
	}
	dev_ref(usb2_dev);
	*dev = usb2_dev;
	return;
}

static void
usb2_dev_init(void *arg)
{
	mtx_init(&usb2_ref_lock, "USB ref mutex", NULL, MTX_DEF);
	sx_init(&usb2_sym_lock, "USB sym mutex");
	TAILQ_INIT(&usb2_sym_head);

	/* check the UGEN methods */
	usb2_fifo_check_methods(&usb2_ugen_methods);
	return;
}

SYSINIT(usb2_dev_init, SI_SUB_KLD, SI_ORDER_FIRST, usb2_dev_init, NULL);

static void
usb2_dev_init_post(void *arg)
{
	/*
	 * Create a dummy device so that we are visible. This device
	 * should never be opened. Therefore a space character is
	 * appended after the USB device name.
	 *
	 * NOTE: The permissions of this device is 0777, because we
	 * check the permissions again in the open routine against the
	 * real USB permissions which are not 0777. Else USB access
	 * will be limited to one user and one group.
	 */
	usb2_dev = make_dev(&usb2_devsw, 0, UID_ROOT, GID_OPERATOR,
	    0777, USB_DEVICE_NAME " ");
	if (usb2_dev == NULL) {
		DPRINTFN(0, "Could not create usb bus device!\n");
	}
	usb2_clone_tag = EVENTHANDLER_REGISTER(dev_clone, usb2_clone_ptr, NULL, 1000);
	if (usb2_clone_tag == NULL) {
		DPRINTFN(0, "Registering clone handler failed!\n");
	}
	return;
}

SYSINIT(usb2_dev_init_post, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, usb2_dev_init_post, NULL);

static void
usb2_dev_uninit(void *arg)
{
	if (usb2_clone_tag) {
		EVENTHANDLER_DEREGISTER(dev_clone, usb2_clone_tag);
		usb2_clone_tag = NULL;
	}
	if (usb2_dev) {
		destroy_dev(usb2_dev);
		usb2_dev = NULL;
	}
	mtx_destroy(&usb2_ref_lock);
	sx_destroy(&usb2_sym_lock);
	return;
}

SYSUNINIT(usb2_dev_uninit, SI_SUB_KICK_SCHEDULER, SI_ORDER_ANY, usb2_dev_uninit, NULL);

static int
usb2_close_f(struct file *fp, struct thread *td)
{
	struct usb2_location loc;
	int fflags;
	int err;

	fflags = fp->f_flag;

	DPRINTFN(2, "fflags=%u\n", fflags);

	err = usb2_ref_device(fp, &loc, 0 /* need uref */ );;

	/* restore some file variables */
	fp->f_ops = usb2_old_f_ops;
	fp->f_data = usb2_old_f_data;

	/* check for error */
	if (err) {
		DPRINTFN(2, "could not ref\n");
		goto done;
	}
	if (fflags & FREAD) {
		usb2_fifo_close(loc.rxfifo, td, fflags);
	}
	if (fflags & FWRITE) {
		usb2_fifo_close(loc.txfifo, td, fflags);
	}
	usb2_unref_device(&loc);

done:
	/* call old close method */
	USB_VNOPS_FO_CLOSE(fp, td, &err);

	return (err);
}

static int
usb2_ioctl_f_sub(struct usb2_fifo *f, u_long cmd, void *addr,
    struct thread *td)
{
	int error = 0;

	switch (cmd) {
	case FIODTYPE:
		*(int *)addr = 0;	/* character device */
		break;

	case FIONBIO:
		/* handled by upper FS layer */
		break;

	case FIOASYNC:
		if (*(int *)addr) {
			if (f->async_p != NULL) {
				error = EBUSY;
				break;
			}
			f->async_p = USB_TD_GET_PROC(td);
		} else {
			f->async_p = NULL;
		}
		break;

		/* XXX this is not the most general solution */
	case TIOCSPGRP:
		if (f->async_p == NULL) {
			error = EINVAL;
			break;
		}
		if (*(int *)addr != USB_PROC_GET_GID(f->async_p)) {
			error = EPERM;
			break;
		}
		break;
	default:
		return (ENOIOCTL);
	}
	return (error);
}

static int
usb2_ioctl_f(struct file *fp, u_long cmd, void *addr,
    struct ucred *cred, struct thread *td)
{
	struct usb2_location loc;
	struct usb2_fifo *f;
	int fflags;
	int err;

	err = usb2_ref_device(fp, &loc, 1 /* no uref */ );;
	if (err) {
		return (ENXIO);
	}
	fflags = fp->f_flag;

	DPRINTFN(2, "fflags=%u, cmd=0x%lx\n", fflags, cmd);

	f = NULL;			/* set default value */
	err = ENOIOCTL;			/* set default value */

	if (fflags & FWRITE) {
		f = loc.txfifo;
		err = usb2_ioctl_f_sub(f, cmd, addr, td);
	}
	if (fflags & FREAD) {
		f = loc.rxfifo;
		err = usb2_ioctl_f_sub(f, cmd, addr, td);
	}
	if (err == ENOIOCTL) {
		err = (f->methods->f_ioctl) (f, cmd, addr, fflags, td);
		if (err == ENOIOCTL) {
			if (usb2_uref_location(&loc)) {
				err = ENXIO;
				goto done;
			}
			err = (f->methods->f_ioctl_post) (f, cmd, addr, fflags, td);
		}
	}
	if (err == ENOIOCTL) {
		err = ENOTTY;
	}
done:
	usb2_unref_device(&loc);
	return (err);
}

/* ARGSUSED */
static int
usb2_kqfilter_f(struct file *fp, struct knote *kn)
{
	return (ENXIO);
}

/* ARGSUSED */
static int
usb2_poll_f(struct file *fp, int events,
    struct ucred *cred, struct thread *td)
{
	struct usb2_location loc;
	struct usb2_fifo *f;
	struct usb2_mbuf *m;
	int fflags;
	int revents;

	revents = usb2_ref_device(fp, &loc, 1 /* no uref */ );;
	if (revents) {
		return (POLLHUP);
	}
	fflags = fp->f_flag;

	/* Figure out who needs service */

	if ((events & (POLLOUT | POLLWRNORM)) &&
	    (fflags & FWRITE)) {

		f = loc.txfifo;

		mtx_lock(f->priv_mtx);

		if (!loc.is_usbfs) {
			if (f->flag_iserror) {
				/* we got an error */
				m = (void *)1;
			} else {
				if (f->queue_data == NULL) {
					/*
					 * start write transfer, if not
					 * already started
					 */
					(f->methods->f_start_write) (f);
				}
				/* check if any packets are available */
				USB_IF_POLL(&f->free_q, m);
			}
		} else {
			if (f->flag_iscomplete) {
				m = (void *)1;
			} else {
				m = NULL;
			}
		}

		if (m) {
			revents |= events & (POLLOUT | POLLWRNORM);
		} else {
			f->flag_isselect = 1;
			selrecord(td, &f->selinfo);
		}

		mtx_unlock(f->priv_mtx);
	}
	if ((events & (POLLIN | POLLRDNORM)) &&
	    (fflags & FREAD)) {

		f = loc.rxfifo;

		mtx_lock(f->priv_mtx);

		if (!loc.is_usbfs) {
			if (f->flag_iserror) {
				/* we have and error */
				m = (void *)1;
			} else {
				if (f->queue_data == NULL) {
					/*
					 * start read transfer, if not
					 * already started
					 */
					(f->methods->f_start_read) (f);
				}
				/* check if any packets are available */
				USB_IF_POLL(&f->used_q, m);
			}
		} else {
			if (f->flag_iscomplete) {
				m = (void *)1;
			} else {
				m = NULL;
			}
		}

		if (m) {
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			f->flag_isselect = 1;
			selrecord(td, &f->selinfo);

			if (!loc.is_usbfs) {
				/* start reading data */
				(f->methods->f_start_read) (f);
			}
		}

		mtx_unlock(f->priv_mtx);
	}
	usb2_unref_device(&loc);
	return (revents);
}

/* ARGSUSED */
static int
usb2_read_f(struct file *fp, struct uio *uio, struct ucred *cred,
    int flags, struct thread *td)
{
	struct usb2_location loc;
	struct usb2_fifo *f;
	struct usb2_mbuf *m;
	int fflags;
	int resid;
	int io_len;
	int err;
	uint8_t tr_data = 0;

	DPRINTFN(2, "\n");

	fflags = fp->f_flag & (O_NONBLOCK | O_DIRECT | FREAD | FWRITE);
	if (fflags & O_DIRECT)
		fflags |= IO_DIRECT;

	err = usb2_ref_device(fp, &loc, 1 /* no uref */ );
	if (err) {
		return (ENXIO);
	}
	f = loc.rxfifo;
	if (f == NULL) {
		/* should not happen */
		return (EPERM);
	}
	resid = uio->uio_resid;

	if ((flags & FOF_OFFSET) == 0)
		uio->uio_offset = fp->f_offset;

	mtx_lock(f->priv_mtx);

	/* check for permanent read error */
	if (f->flag_iserror) {
		err = EIO;
		goto done;
	}
	/* check if USB-FS interface is active */
	if (loc.is_usbfs) {
		/*
		 * The queue is used for events that should be
		 * retrieved using the "USB_FS_COMPLETE" ioctl.
		 */
		err = EINVAL;
		goto done;
	}
	while (uio->uio_resid > 0) {

		USB_IF_DEQUEUE(&f->used_q, m);

		if (m == NULL) {

			/* start read transfer, if not already started */

			(f->methods->f_start_read) (f);

			if (fflags & O_NONBLOCK) {
				if (tr_data) {
					/* return length before error */
					break;
				}
				err = EWOULDBLOCK;
				break;
			}
			DPRINTF("sleeping\n");

			err = usb2_fifo_wait(f);
			if (err) {
				break;
			}
			continue;
		}
		if (f->methods->f_filter_read) {
			/*
			 * Sometimes it is convenient to process data at the
			 * expense of a userland process instead of a kernel
			 * process.
			 */
			(f->methods->f_filter_read) (f, m);
		}
		tr_data = 1;

		io_len = MIN(m->cur_data_len, uio->uio_resid);

		DPRINTFN(2, "transfer %d bytes from %p\n",
		    io_len, m->cur_data_ptr);

		err = usb2_fifo_uiomove(f,
		    m->cur_data_ptr, io_len, uio);

		m->cur_data_len -= io_len;
		m->cur_data_ptr += io_len;

		if (m->cur_data_len == 0) {

			uint8_t last_packet;

			last_packet = m->last_packet;

			USB_IF_ENQUEUE(&f->free_q, m);

			if (last_packet) {
				/* keep framing */
				break;
			}
		} else {
			USB_IF_PREPEND(&f->used_q, m);
		}

		if (err) {
			break;
		}
	}
done:
	mtx_unlock(f->priv_mtx);

	usb2_unref_device(&loc);

	if ((flags & FOF_OFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
	return (err);
}

static int
usb2_stat_f(struct file *fp, struct stat *sb, struct ucred *cred, struct thread *td)
{
	return (USB_VNOPS_FO_STAT(fp, sb, cred, td));
}

#if __FreeBSD_version > 800009
static int
usb2_truncate_f(struct file *fp, off_t length, struct ucred *cred, struct thread *td)
{
	return (USB_VNOPS_FO_TRUNCATE(fp, length, cred, td));
}

#endif

/* ARGSUSED */
static int
usb2_write_f(struct file *fp, struct uio *uio, struct ucred *cred,
    int flags, struct thread *td)
{
	struct usb2_location loc;
	struct usb2_fifo *f;
	struct usb2_mbuf *m;
	int fflags;
	int resid;
	int io_len;
	int err;
	uint8_t tr_data = 0;

	DPRINTFN(2, "\n");

	fflags = fp->f_flag & (O_NONBLOCK | O_DIRECT |
	    FREAD | FWRITE | O_FSYNC);
	if (fflags & O_DIRECT)
		fflags |= IO_DIRECT;

	err = usb2_ref_device(fp, &loc, 1 /* no uref */ );
	if (err) {
		return (ENXIO);
	}
	f = loc.txfifo;
	if (f == NULL) {
		/* should not happen */
		usb2_unref_device(&loc);
		return (EPERM);
	}
	resid = uio->uio_resid;

	if ((flags & FOF_OFFSET) == 0)
		uio->uio_offset = fp->f_offset;

	mtx_lock(f->priv_mtx);

	/* check for permanent write error */
	if (f->flag_iserror) {
		err = EIO;
		goto done;
	}
	/* check if USB-FS interface is active */
	if (loc.is_usbfs) {
		/*
		 * The queue is used for events that should be
		 * retrieved using the "USB_FS_COMPLETE" ioctl.
		 */
		err = EINVAL;
		goto done;
	}
	if (f->queue_data == NULL) {
		/* start write transfer, if not already started */
		(f->methods->f_start_write) (f);
	}
	/* we allow writing zero length data */
	do {
		USB_IF_DEQUEUE(&f->free_q, m);

		if (m == NULL) {

			if (fflags & O_NONBLOCK) {
				if (tr_data) {
					/* return length before error */
					break;
				}
				err = EWOULDBLOCK;
				break;
			}
			DPRINTF("sleeping\n");

			err = usb2_fifo_wait(f);
			if (err) {
				break;
			}
			continue;
		}
		tr_data = 1;

		USB_MBUF_RESET(m);

		io_len = MIN(m->cur_data_len, uio->uio_resid);

		m->cur_data_len = io_len;

		DPRINTFN(2, "transfer %d bytes to %p\n",
		    io_len, m->cur_data_ptr);

		err = usb2_fifo_uiomove(f,
		    m->cur_data_ptr, io_len, uio);

		if (err) {
			USB_IF_ENQUEUE(&f->free_q, m);
			break;
		}
		if (f->methods->f_filter_write) {
			/*
			 * Sometimes it is convenient to process data at the
			 * expense of a userland process instead of a kernel
			 * process.
			 */
			(f->methods->f_filter_write) (f, m);
		}
		USB_IF_ENQUEUE(&f->used_q, m);

		(f->methods->f_start_write) (f);

	} while (uio->uio_resid > 0);
done:
	mtx_unlock(f->priv_mtx);

	usb2_unref_device(&loc);

	if ((flags & FOF_OFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;

	return (err);
}

static int
usb2_fifo_uiomove(struct usb2_fifo *f, void *cp,
    int n, struct uio *uio)
{
	int error;

	mtx_unlock(f->priv_mtx);

	/*
	 * "uiomove()" can sleep so one needs to make a wrapper,
	 * exiting the mutex and checking things:
	 */
	error = uiomove(cp, n, uio);

	mtx_lock(f->priv_mtx);

	return (error);
}

int
usb2_fifo_wait(struct usb2_fifo *f)
{
	int err;

	mtx_assert(f->priv_mtx, MA_OWNED);

	if (f->flag_iserror) {
		/* we are gone */
		return (EIO);
	}
	f->flag_sleeping = 1;

	err = usb2_cv_wait_sig(&f->cv_io, f->priv_mtx);

	if (f->flag_iserror) {
		/* we are gone */
		err = EIO;
	}
	return (err);
}

void
usb2_fifo_signal(struct usb2_fifo *f)
{
	if (f->flag_sleeping) {
		f->flag_sleeping = 0;
		usb2_cv_broadcast(&f->cv_io);
	}
	return;
}

void
usb2_fifo_wakeup(struct usb2_fifo *f)
{
	usb2_fifo_signal(f);

	if (f->flag_isselect) {
		selwakeup(&f->selinfo);
		f->flag_isselect = 0;
	}
	if (f->async_p != NULL) {
		PROC_LOCK(f->async_p);
		psignal(f->async_p, SIGIO);
		PROC_UNLOCK(f->async_p);
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_opened
 *
 * Returns:
 * 0: FIFO not opened.
 * Else: FIFO is opened.
 *------------------------------------------------------------------------*/
uint8_t
usb2_fifo_opened(struct usb2_fifo *f)
{
	uint8_t temp;
	uint8_t do_unlock;

	if (f == NULL) {
		return (0);		/* be NULL safe */
	}
	if (mtx_owned(f->priv_mtx)) {
		do_unlock = 0;
	} else {
		do_unlock = 1;
		mtx_lock(f->priv_mtx);
	}
	temp = f->curr_file ? 1 : 0;
	if (do_unlock) {
		mtx_unlock(f->priv_mtx);
	}
	return (temp);
}


static int
usb2_fifo_dummy_open(struct usb2_fifo *fifo,
    int fflags, struct thread *td)
{
	return (0);
}

static void
usb2_fifo_dummy_close(struct usb2_fifo *fifo,
    int fflags, struct thread *td)
{
	return;
}

static int
usb2_fifo_dummy_ioctl(struct usb2_fifo *fifo, u_long cmd, void *addr,
    int fflags, struct thread *td)
{
	return (ENOIOCTL);
}

static void
usb2_fifo_dummy_cmd(struct usb2_fifo *fifo)
{
	fifo->flag_flushing = 0;	/* not flushing */
	return;
}

static void
usb2_fifo_check_methods(struct usb2_fifo_methods *pm)
{
	/* check that all callback functions are OK */

	if (pm->f_open == NULL)
		pm->f_open = &usb2_fifo_dummy_open;

	if (pm->f_close == NULL)
		pm->f_close = &usb2_fifo_dummy_close;

	if (pm->f_ioctl == NULL)
		pm->f_ioctl = &usb2_fifo_dummy_ioctl;

	if (pm->f_ioctl_post == NULL)
		pm->f_ioctl_post = &usb2_fifo_dummy_ioctl;

	if (pm->f_start_read == NULL)
		pm->f_start_read = &usb2_fifo_dummy_cmd;

	if (pm->f_stop_read == NULL)
		pm->f_stop_read = &usb2_fifo_dummy_cmd;

	if (pm->f_start_write == NULL)
		pm->f_start_write = &usb2_fifo_dummy_cmd;

	if (pm->f_stop_write == NULL)
		pm->f_stop_write = &usb2_fifo_dummy_cmd;

	return;
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_attach
 *
 * The following function will create a duplex FIFO.
 *
 * Return values:
 * 0: Success.
 * Else: Failure.
 *------------------------------------------------------------------------*/
int
usb2_fifo_attach(struct usb2_device *udev, void *priv_sc,
    struct mtx *priv_mtx, struct usb2_fifo_methods *pm,
    struct usb2_fifo_sc *f_sc, uint16_t unit, uint16_t subunit,
    uint8_t iface_index)
{
	struct usb2_fifo *f_tx;
	struct usb2_fifo *f_rx;
	char buf[32];
	char src[32];
	uint8_t n;

	f_sc->fp[USB_FIFO_TX] = NULL;
	f_sc->fp[USB_FIFO_RX] = NULL;

	if (pm == NULL)
		return (EINVAL);

	/* check the methods */
	usb2_fifo_check_methods(pm);

	if (priv_mtx == NULL)
		priv_mtx = &Giant;

	/* search for a free FIFO slot */
	for (n = 0;; n += 2) {

		if (n == USB_FIFO_MAX) {
			/* end of FIFOs reached */
			return (ENOMEM);
		}
		/* Check for TX FIFO */
		if (udev->fifo[n + USB_FIFO_TX] != NULL) {
			continue;
		}
		/* Check for RX FIFO */
		if (udev->fifo[n + USB_FIFO_RX] != NULL) {
			continue;
		}
		break;
	}

	f_tx = usb2_fifo_alloc();
	f_rx = usb2_fifo_alloc();

	if ((f_tx == NULL) || (f_rx == NULL)) {
		usb2_fifo_free(f_tx);
		usb2_fifo_free(f_rx);
		return (ENOMEM);
	}
	/* initialise FIFO structures */

	f_tx->fifo_index = n + USB_FIFO_TX;
	f_tx->dev_ep_index = (n / 2) + (USB_EP_MAX / 2);
	f_tx->priv_mtx = priv_mtx;
	f_tx->priv_sc0 = priv_sc;
	f_tx->methods = pm;
	f_tx->iface_index = iface_index;
	f_tx->udev = udev;

	f_rx->fifo_index = n + USB_FIFO_RX;
	f_rx->dev_ep_index = (n / 2) + (USB_EP_MAX / 2);
	f_rx->priv_mtx = priv_mtx;
	f_rx->priv_sc0 = priv_sc;
	f_rx->methods = pm;
	f_rx->iface_index = iface_index;
	f_rx->udev = udev;

	f_sc->fp[USB_FIFO_TX] = f_tx;
	f_sc->fp[USB_FIFO_RX] = f_rx;

	mtx_lock(&usb2_ref_lock);
	udev->fifo[f_tx->fifo_index] = f_tx;
	udev->fifo[f_rx->fifo_index] = f_rx;
	mtx_unlock(&usb2_ref_lock);

	if (snprintf(src, sizeof(src),
	    USB_DEVICE_NAME "%u.%u.%u.%u",
	    device_get_unit(udev->bus->bdev),
	    udev->device_index,
	    iface_index,
	    f_tx->dev_ep_index)) {
		/* ignore */
	}
	for (n = 0; n != 4; n++) {

		if (pm->basename[n] == NULL) {
			continue;
		}
		if (subunit == 0xFFFF) {
			if (snprintf(buf, sizeof(buf),
			    "%s%u%s", pm->basename[n],
			    unit, pm->postfix[n] ?
			    pm->postfix[n] : "")) {
				/* ignore */
			}
		} else {
			if (snprintf(buf, sizeof(buf),
			    "%s%u.%u%s", pm->basename[n],
			    unit, subunit, pm->postfix[n] ?
			    pm->postfix[n] : "")) {
				/* ignore */
			}
		}

		/*
		 * Distribute the symbolic links into two FIFO structures:
		 */
		if (n & 1) {
			f_rx->symlink[n / 2] =
			    usb2_alloc_symlink(src, "%s", buf);
		} else {
			f_tx->symlink[n / 2] =
			    usb2_alloc_symlink(src, "%s", buf);
		}
		printf("Symlink: %s -> %s\n", buf, src);
	}

	DPRINTFN(2, "attached %p/%p\n", f_tx, f_rx);
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_alloc_buffer
 *
 * Return values:
 * 0: Success
 * Else failure
 *------------------------------------------------------------------------*/
int
usb2_fifo_alloc_buffer(struct usb2_fifo *f, uint32_t bufsize,
    uint16_t nbuf)
{
	usb2_fifo_free_buffer(f);

	/* allocate an endpoint */
	f->free_q.ifq_maxlen = nbuf;
	f->used_q.ifq_maxlen = nbuf;

	f->queue_data = usb2_alloc_mbufs(
	    M_USBDEV, &f->free_q, bufsize, nbuf);

	if ((f->queue_data == NULL) && bufsize && nbuf) {
		return (ENOMEM);
	}
	return (0);			/* success */
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_free_buffer
 *
 * This function will free the buffers associated with a FIFO. This
 * function can be called multiple times in a row.
 *------------------------------------------------------------------------*/
void
usb2_fifo_free_buffer(struct usb2_fifo *f)
{
	if (f->queue_data) {
		/* free old buffer */
		free(f->queue_data, M_USBDEV);
		f->queue_data = NULL;
	}
	/* reset queues */

	bzero(&f->free_q, sizeof(f->free_q));
	bzero(&f->used_q, sizeof(f->used_q));
	return;
}

void
usb2_fifo_detach(struct usb2_fifo_sc *f_sc)
{
	if (f_sc == NULL) {
		return;
	}
	usb2_fifo_free(f_sc->fp[USB_FIFO_TX]);
	usb2_fifo_free(f_sc->fp[USB_FIFO_RX]);

	f_sc->fp[USB_FIFO_TX] = NULL;
	f_sc->fp[USB_FIFO_RX] = NULL;

	DPRINTFN(2, "detached %p\n", f_sc);

	return;
}

uint32_t
usb2_fifo_put_bytes_max(struct usb2_fifo *f)
{
	struct usb2_mbuf *m;
	uint32_t len;

	USB_IF_POLL(&f->free_q, m);

	if (m) {
		len = m->max_data_len;
	} else {
		len = 0;
	}
	return (len);
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_put_data
 *
 * what:
 *  0 - normal operation
 *  1 - set last packet flag to enforce framing
 *------------------------------------------------------------------------*/
void
usb2_fifo_put_data(struct usb2_fifo *f, struct usb2_page_cache *pc,
    uint32_t offset, uint32_t len, uint8_t what)
{
	struct usb2_mbuf *m;
	uint32_t io_len;

	while (len || (what == 1)) {

		USB_IF_DEQUEUE(&f->free_q, m);

		if (m) {
			USB_MBUF_RESET(m);

			io_len = MIN(len, m->cur_data_len);

			usb2_copy_out(pc, offset, m->cur_data_ptr, io_len);

			m->cur_data_len = io_len;
			offset += io_len;
			len -= io_len;

			if ((len == 0) && (what == 1)) {
				m->last_packet = 1;
			}
			USB_IF_ENQUEUE(&f->used_q, m);

			usb2_fifo_wakeup(f);

			if ((len == 0) || (what == 1)) {
				break;
			}
		} else {
			break;
		}
	}
	return;
}

void
usb2_fifo_put_data_linear(struct usb2_fifo *f, void *ptr,
    uint32_t len, uint8_t what)
{
	struct usb2_mbuf *m;
	uint32_t io_len;

	while (len || (what == 1)) {

		USB_IF_DEQUEUE(&f->free_q, m);

		if (m) {
			USB_MBUF_RESET(m);

			io_len = MIN(len, m->cur_data_len);

			bcopy(ptr, m->cur_data_ptr, io_len);

			m->cur_data_len = io_len;
			ptr = USB_ADD_BYTES(ptr, io_len);
			len -= io_len;

			if ((len == 0) && (what == 1)) {
				m->last_packet = 1;
			}
			USB_IF_ENQUEUE(&f->used_q, m);

			usb2_fifo_wakeup(f);

			if ((len == 0) || (what == 1)) {
				break;
			}
		} else {
			break;
		}
	}
	return;
}

uint8_t
usb2_fifo_put_data_buffer(struct usb2_fifo *f, void *ptr, uint32_t len)
{
	struct usb2_mbuf *m;

	USB_IF_DEQUEUE(&f->free_q, m);

	if (m) {
		m->cur_data_len = len;
		m->cur_data_ptr = ptr;
		USB_IF_ENQUEUE(&f->used_q, m);
		usb2_fifo_wakeup(f);
		return (1);
	}
	return (0);
}

void
usb2_fifo_put_data_error(struct usb2_fifo *f)
{
	f->flag_iserror = 1;
	usb2_fifo_wakeup(f);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_get_data
 *
 * what:
 *  0 - normal operation
 *  1 - only get one "usb2_mbuf"
 *
 * returns:
 *  0 - no more data
 *  1 - data in buffer
 *------------------------------------------------------------------------*/
uint8_t
usb2_fifo_get_data(struct usb2_fifo *f, struct usb2_page_cache *pc,
    uint32_t offset, uint32_t len, uint32_t *actlen,
    uint8_t what)
{
	struct usb2_mbuf *m;
	uint32_t io_len;
	uint8_t tr_data = 0;

	actlen[0] = 0;

	while (1) {

		USB_IF_DEQUEUE(&f->used_q, m);

		if (m) {

			tr_data = 1;

			io_len = MIN(len, m->cur_data_len);

			usb2_copy_in(pc, offset, m->cur_data_ptr, io_len);

			len -= io_len;
			offset += io_len;
			actlen[0] += io_len;
			m->cur_data_ptr += io_len;
			m->cur_data_len -= io_len;

			if ((m->cur_data_len == 0) || (what == 1)) {
				USB_IF_ENQUEUE(&f->free_q, m);

				usb2_fifo_wakeup(f);

				if (what == 1) {
					break;
				}
			} else {
				USB_IF_PREPEND(&f->used_q, m);
			}
		} else {

			if (tr_data) {
				/* wait for data to be written out */
				break;
			}
			if (f->flag_flushing) {
				f->flag_flushing = 0;
				usb2_fifo_wakeup(f);
			}
			break;
		}
		if (len == 0) {
			break;
		}
	}
	return (tr_data);
}

uint8_t
usb2_fifo_get_data_linear(struct usb2_fifo *f, void *ptr,
    uint32_t len, uint32_t *actlen, uint8_t what)
{
	struct usb2_mbuf *m;
	uint32_t io_len;
	uint8_t tr_data = 0;

	actlen[0] = 0;

	while (1) {

		USB_IF_DEQUEUE(&f->used_q, m);

		if (m) {

			tr_data = 1;

			io_len = MIN(len, m->cur_data_len);

			bcopy(m->cur_data_ptr, ptr, io_len);

			len -= io_len;
			ptr = USB_ADD_BYTES(ptr, io_len);
			actlen[0] += io_len;
			m->cur_data_ptr += io_len;
			m->cur_data_len -= io_len;

			if ((m->cur_data_len == 0) || (what == 1)) {
				USB_IF_ENQUEUE(&f->free_q, m);

				usb2_fifo_wakeup(f);

				if (what == 1) {
					break;
				}
			} else {
				USB_IF_PREPEND(&f->used_q, m);
			}
		} else {

			if (tr_data) {
				/* wait for data to be written out */
				break;
			}
			if (f->flag_flushing) {
				f->flag_flushing = 0;
				usb2_fifo_wakeup(f);
			}
			break;
		}
		if (len == 0) {
			break;
		}
	}
	return (tr_data);
}

uint8_t
usb2_fifo_get_data_buffer(struct usb2_fifo *f, void **pptr, uint32_t *plen)
{
	struct usb2_mbuf *m;

	USB_IF_POLL(&f->used_q, m);

	if (m) {
		*plen = m->cur_data_len;
		*pptr = m->cur_data_ptr;

		return (1);
	}
	return (0);
}

void
usb2_fifo_get_data_error(struct usb2_fifo *f)
{
	f->flag_iserror = 1;
	usb2_fifo_wakeup(f);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_alloc_symlink
 *
 * Return values:
 * NULL: Failure
 * Else: Pointer to symlink entry
 *------------------------------------------------------------------------*/
struct usb2_symlink *
usb2_alloc_symlink(const char *target, const char *fmt,...)
{
	struct usb2_symlink *ps;
	va_list ap;

	ps = malloc(sizeof(*ps), M_USBDEV, M_WAITOK);
	if (ps == NULL) {
		return (ps);
	}
	strlcpy(ps->dst_path, target, sizeof(ps->dst_path));
	ps->dst_len = strlen(ps->dst_path);

	va_start(ap, fmt);
	vsnrprintf(ps->src_path,
	    sizeof(ps->src_path), 32, fmt, ap);
	va_end(ap);
	ps->src_len = strlen(ps->src_path);

	sx_xlock(&usb2_sym_lock);
	TAILQ_INSERT_TAIL(&usb2_sym_head, ps, sym_entry);
	sx_unlock(&usb2_sym_lock);
	return (ps);
}

/*------------------------------------------------------------------------*
 *	usb2_free_symlink
 *------------------------------------------------------------------------*/
void
usb2_free_symlink(struct usb2_symlink *ps)
{
	if (ps == NULL) {
		return;
	}
	sx_xlock(&usb2_sym_lock);
	TAILQ_REMOVE(&usb2_sym_head, ps, sym_entry);
	sx_unlock(&usb2_sym_lock);

	free(ps, M_USBDEV);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_lookup_symlink
 *
 * Return value:
 * Numerical device location
 *------------------------------------------------------------------------*/
uint32_t
usb2_lookup_symlink(const char *src_ptr, uint8_t src_len)
{
	enum {
		USB_DNAME_LEN = sizeof(USB_DEVICE_NAME) - 1,
	};
	struct usb2_symlink *ps;
	uint32_t temp;

	sx_xlock(&usb2_sym_lock);

	TAILQ_FOREACH(ps, &usb2_sym_head, sym_entry) {

		if (src_len != ps->src_len)
			continue;

		if (memcmp(ps->src_path, src_ptr, src_len))
			continue;

		if (USB_DNAME_LEN > ps->dst_len)
			continue;

		if (memcmp(ps->dst_path, USB_DEVICE_NAME, USB_DNAME_LEN))
			continue;

		temp = usb2_path_convert(ps->dst_path + USB_DNAME_LEN);
		sx_unlock(&usb2_sym_lock);

		return (temp);
	}
	sx_unlock(&usb2_sym_lock);
	return (0 - 1);
}

/*------------------------------------------------------------------------*
 *	usb2_read_symlink
 *
 * Return value:
 * 0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
int
usb2_read_symlink(uint8_t *user_ptr, uint32_t startentry, uint32_t user_len)
{
	struct usb2_symlink *ps;
	uint32_t temp;
	uint32_t delta = 0;
	uint8_t len;
	int error = 0;

	sx_xlock(&usb2_sym_lock);

	TAILQ_FOREACH(ps, &usb2_sym_head, sym_entry) {

		/*
		 * Compute total length of source and destination symlink
		 * strings pluss one length byte and two NUL bytes:
		 */
		temp = ps->src_len + ps->dst_len + 3;

		if (temp > 255) {
			/*
			 * Skip entry because this length cannot fit
			 * into one byte:
			 */
			continue;
		}
		if (startentry != 0) {
			/* decrement read offset */
			startentry--;
			continue;
		}
		if (temp > user_len) {
			/* out of buffer space */
			break;
		}
		len = temp;

		/* copy out total length */

		error = copyout(&len,
		    USB_ADD_BYTES(user_ptr, delta), 1);
		if (error) {
			break;
		}
		delta += 1;

		/* copy out source string */

		error = copyout(ps->src_path,
		    USB_ADD_BYTES(user_ptr, delta), ps->src_len);
		if (error) {
			break;
		}
		len = 0;
		delta += ps->src_len;
		error = copyout(&len,
		    USB_ADD_BYTES(user_ptr, delta), 1);
		if (error) {
			break;
		}
		delta += 1;

		/* copy out destination string */

		error = copyout(ps->dst_path,
		    USB_ADD_BYTES(user_ptr, delta), ps->dst_len);
		if (error) {
			break;
		}
		len = 0;
		delta += ps->dst_len;
		error = copyout(&len,
		    USB_ADD_BYTES(user_ptr, delta), 1);
		if (error) {
			break;
		}
		delta += 1;

		user_len -= temp;
	}

	/* a zero length entry indicates the end */

	if ((user_len != 0) && (error == 0)) {

		len = 0;

		error = copyout(&len,
		    USB_ADD_BYTES(user_ptr, delta), 1);
	}
	sx_unlock(&usb2_sym_lock);
	return (error);
}
