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

#include <dev/usb/usb.h>
#include <dev/usb/usb_ioctl.h>
#include <dev/usb/usb_defs.h>
#include <dev/usb/usb_mfunc.h>
#include <dev/usb/usb_error.h>

#define	USB_DEBUG_VAR usb2_fifo_debug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_mbuf.h>
#include <dev/usb/usb_dev.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_generic.h>
#include <dev/usb/usb_dynamic.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

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

static int	usb2_fifo_open(struct usb2_fifo *, int);
static void	usb2_fifo_close(struct usb2_fifo *, int);
static void	usb2_dev_init(void *);
static void	usb2_dev_init_post(void *);
static void	usb2_dev_uninit(void *);
static int	usb2_fifo_uiomove(struct usb2_fifo *, void *, int,
		    struct uio *);
static void	usb2_fifo_check_methods(struct usb2_fifo_methods *);
static struct	usb2_fifo *usb2_fifo_alloc(void);
static struct	usb2_pipe *usb2_dev_get_pipe(struct usb2_device *, uint8_t,
		    uint8_t, uint8_t);
static void	usb2_loc_fill(struct usb2_fs_privdata *,
		    struct usb2_cdev_privdata *);
static void	usb2_close(void *);
static usb2_error_t usb2_ref_device(struct usb2_cdev_privdata *, int);
static usb2_error_t usb2_uref_location(struct usb2_cdev_privdata *);
static void	usb2_unref_device(struct usb2_cdev_privdata *);

static d_open_t usb2_open;
static d_ioctl_t usb2_ioctl;
static d_read_t usb2_read;
static d_write_t usb2_write;
static d_poll_t usb2_poll;

static d_ioctl_t usb2_static_ioctl;

static usb2_fifo_open_t usb2_fifo_dummy_open;
static usb2_fifo_close_t usb2_fifo_dummy_close;
static usb2_fifo_ioctl_t usb2_fifo_dummy_ioctl;
static usb2_fifo_cmd_t usb2_fifo_dummy_cmd;

/* character device structure used for devices (/dev/ugenX.Y and /dev/uXXX) */
struct cdevsw usb2_devsw = {
	.d_version = D_VERSION,
	.d_open = usb2_open,
	.d_ioctl = usb2_ioctl,
	.d_name = "usbdev",
	.d_flags = D_TRACKCLOSE,
	.d_read = usb2_read,
	.d_write = usb2_write,
	.d_poll = usb2_poll
};

static struct cdev* usb2_dev = NULL;

/* character device structure used for /dev/usb */
struct cdevsw usb2_static_devsw = {
	.d_version = D_VERSION,
	.d_ioctl = usb2_static_ioctl,
	.d_name = "usb"
};

static TAILQ_HEAD(, usb2_symlink) usb2_sym_head;
static struct sx usb2_sym_lock;

struct mtx usb2_ref_lock;

/*------------------------------------------------------------------------*
 *	usb2_loc_fill
 *
 * This is used to fill out a usb2_cdev_privdata structure based on the
 * device's address as contained in usb2_fs_privdata.
 *------------------------------------------------------------------------*/
static void
usb2_loc_fill(struct usb2_fs_privdata* pd, struct usb2_cdev_privdata *cpd)
{
	cpd->bus_index = pd->bus_index;
	cpd->dev_index = pd->dev_index;
	cpd->iface_index = pd->iface_index;
	cpd->ep_addr = pd->ep_addr;
	cpd->fifo_index = pd->fifo_index;
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
usb2_ref_device(struct usb2_cdev_privdata* cpd, int need_uref)
{
	struct usb2_fifo **ppf;
	struct usb2_fifo *f;
	int dev_ep_index;

	DPRINTFN(2, "usb2_ref_device, cpd=%p need uref=%d\n", cpd, need_uref);

	mtx_lock(&usb2_ref_lock);
	cpd->bus = devclass_get_softc(usb2_devclass_ptr, cpd->bus_index);
	if (cpd->bus == NULL) {
		DPRINTFN(2, "no bus at %u\n", cpd->bus_index);
		goto error;
	}
	cpd->udev = cpd->bus->devices[cpd->dev_index];
	if (cpd->udev == NULL) {
		DPRINTFN(2, "no device at %u\n", cpd->dev_index);
		goto error;
	}
	if (cpd->udev->refcount == USB_DEV_REF_MAX) {
		DPRINTFN(2, "no dev ref\n");
		goto error;
	}
	/* check if we are doing an open */
	dev_ep_index = cpd->ep_addr;
	if (cpd->fflags == 0) {
		/* set defaults */
		cpd->txfifo = NULL;
		cpd->rxfifo = NULL;
		cpd->is_write = 0;
		cpd->is_read = 0;
		cpd->is_usbfs = 0;
	} else {
		/* initialise "is_usbfs" flag */
		cpd->is_usbfs = 0;

		/* check for write */
		if (cpd->fflags & FWRITE) {
			ppf = cpd->udev->fifo;
			f = ppf[cpd->fifo_index + USB_FIFO_TX];
			cpd->txfifo = f;
			cpd->is_write = 1;	/* ref */
			if (f == NULL || f->refcount == USB_FIFO_REF_MAX)
				goto error;
			/* check if USB-FS is active */
			if (f->fs_ep_max != 0) {
				cpd->is_usbfs = 1;
			}
			/*
			 * Get real endpoint index associated with
			 * this FIFO:
			 */
			dev_ep_index = f->dev_ep_index;
		} else {
			cpd->txfifo = NULL;
			cpd->is_write = 0;	/* no ref */
		}

		/* check for read */
		if (cpd->fflags & FREAD) {
			ppf = cpd->udev->fifo;
			f = ppf[cpd->fifo_index + USB_FIFO_RX];
			cpd->rxfifo = f;
			cpd->is_read = 1;	/* ref */
			if (f == NULL || f->refcount == USB_FIFO_REF_MAX)
				goto error;
			/* check if USB-FS is active */
			if (f->fs_ep_max != 0) {
				cpd->is_usbfs = 1;
			}
			/*
			 * Get real endpoint index associated with
			 * this FIFO:
			 */
			dev_ep_index = f->dev_ep_index;
		} else {
			cpd->rxfifo = NULL;
			cpd->is_read = 0;	/* no ref */
		}
	}

	/* check if we require an interface */
	cpd->iface = usb2_get_iface(cpd->udev, cpd->iface_index);
	if (dev_ep_index != 0) {
		/* non control endpoint - we need an interface */
		if (cpd->iface == NULL) {
			DPRINTFN(2, "no iface\n");
			goto error;
		}
		if (cpd->iface->idesc == NULL) {
			DPRINTFN(2, "no idesc\n");
			goto error;
		}
	}
	/* when everything is OK we increment the refcounts */
	if (cpd->is_write) {
		DPRINTFN(2, "ref write\n");
		cpd->txfifo->refcount++;
	}
	if (cpd->is_read) {
		DPRINTFN(2, "ref read\n");
		cpd->rxfifo->refcount++;
	}
	if (need_uref) {
		DPRINTFN(2, "ref udev - needed\n");
		cpd->udev->refcount++;
		cpd->is_uref = 1;
	}
	mtx_unlock(&usb2_ref_lock);

	if (cpd->is_uref) {
		/*
		 * We are about to alter the bus-state. Apply the
		 * required locks.
		 */
		sx_xlock(cpd->udev->default_sx + 1);
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
usb2_uref_location(struct usb2_cdev_privdata *cpd)
{
	/*
	 * Check if we already got an USB reference on this location:
	 */
	if (cpd->is_uref) {
		return (0);		/* success */
	}
	mtx_lock(&usb2_ref_lock);
	if (cpd->bus != devclass_get_softc(usb2_devclass_ptr, cpd->bus_index)) {
		DPRINTFN(2, "bus changed at %u\n", cpd->bus_index);
		goto error;
	}
	if (cpd->udev != cpd->bus->devices[cpd->dev_index]) {
		DPRINTFN(2, "device changed at %u\n", cpd->dev_index);
		goto error;
	}
	if (cpd->udev->refcount == USB_DEV_REF_MAX) {
		DPRINTFN(2, "no dev ref\n");
		goto error;
	}
	DPRINTFN(2, "ref udev\n");
	cpd->udev->refcount++;
	mtx_unlock(&usb2_ref_lock);

	/* set "uref" */
	cpd->is_uref = 1;

	/*
	 * We are about to alter the bus-state. Apply the
	 * required locks.
	 */
	sx_xlock(cpd->udev->default_sx + 1);
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
usb2_unref_device(struct usb2_cdev_privdata *cpd)
{
	if (cpd->is_uref) {
		mtx_unlock(&Giant);	/* XXX */
		sx_unlock(cpd->udev->default_sx + 1);
	}
	mtx_lock(&usb2_ref_lock);
	if (cpd->is_read) {
		if (--(cpd->rxfifo->refcount) == 0) {
			usb2_cv_signal(&cpd->rxfifo->cv_drain);
		}
		cpd->is_read = 0;
	}
	if (cpd->is_write) {
		if (--(cpd->txfifo->refcount) == 0) {
			usb2_cv_signal(&cpd->txfifo->cv_drain);
		}
		cpd->is_write = 0;
	}
	if (cpd->is_uref) {
		if (--(cpd->udev->refcount) == 0) {
			usb2_cv_signal(cpd->udev->default_cv + 1);
		}
		cpd->is_uref = 0;
	}
	mtx_unlock(&usb2_ref_lock);
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
usb2_fifo_create(struct usb2_cdev_privdata *cpd)
{
	struct usb2_device *udev = cpd->udev;
	struct usb2_fifo *f;
	struct usb2_pipe *pipe;
	uint8_t iface_index = cpd->iface_index;
	uint8_t n;
	uint8_t is_tx;
	uint8_t is_rx;
	uint8_t no_null;
	uint8_t is_busy;
	int ep = cpd->ep_addr;

	is_tx = (cpd->fflags & FWRITE) ? 1 : 0;
	is_rx = (cpd->fflags & FREAD) ? 1 : 0;
	no_null = 1;
	is_busy = 0;

	/* Preallocated FIFO */
	if (ep < 0) {
		DPRINTFN(5, "Preallocated FIFO\n");
		if (is_tx) {
			f = udev->fifo[cpd->fifo_index + USB_FIFO_TX];
			if (f == NULL)
				return (EINVAL);
			cpd->txfifo = f;
		}
		if (is_rx) {
			f = udev->fifo[cpd->fifo_index + USB_FIFO_RX];
			if (f == NULL)
				return (EINVAL);
			cpd->rxfifo = f;
		}
		return (0);
	}

	KASSERT(ep >= 0 && ep <= 15, ("endpoint %d out of range", ep));

	/* search for a free FIFO slot */
	DPRINTFN(5, "Endpoint device, searching for 0x%02x\n", ep);
	for (n = 0;; n += 2) {

		if (n == USB_FIFO_MAX) {
			if (no_null) {
				no_null = 0;
				n = 0;
			} else {
				/* end of FIFOs reached */
				DPRINTFN(5, "out of FIFOs\n");
				return (ENOMEM);
			}
		}
		/* Check for TX FIFO */
		if (is_tx) {
			f = udev->fifo[n + USB_FIFO_TX];
			if (f != NULL) {
				if (f->dev_ep_index != ep) {
					/* wrong endpoint index */
					continue;
				}
				if (ep != 0 &&
				    f->iface_index != iface_index) {
					/* wrong interface index */
					continue;
				}
				if (f->opened) {
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
				if (f->dev_ep_index != ep) {
					/* wrong endpoint index */
					continue;
				}
				if (ep != 0 &&
				    f->iface_index != iface_index) {
					/* wrong interface index */
					continue;
				}
				if (f->opened) {
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
		if (ep >= (USB_EP_MAX / 2)) {
			/* we don't create any endpoints in this range */
			DPRINTFN(5, "dev_ep_index out of range\n");
			return (is_busy ? EBUSY : EINVAL);
		}
	}
	/* Check TX FIFO */
	if (is_tx &&
	    (udev->fifo[n + USB_FIFO_TX] == NULL)) {
		pipe = usb2_dev_get_pipe(udev,
		    iface_index, ep, USB_FIFO_TX);
		DPRINTFN(5, "dev_get_pipe(%d, 0x%x, 0x%x)\n", iface_index, ep, USB_FIFO_TX);
		if (pipe == NULL) {
			DPRINTFN(5, "dev_get_pipe returned NULL\n");
			return (EINVAL);
		}
		f = usb2_fifo_alloc();
		if (f == NULL) {
			DPRINTFN(5, "could not alloc tx fifo\n");
			return (ENOMEM);
		}
		/* update some fields */
		f->fifo_index = n + USB_FIFO_TX;
		f->dev_ep_index = ep;
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
		    iface_index, ep, USB_FIFO_RX);
		DPRINTFN(5, "dev_get_pipe(%d, 0x%x, 0x%x)\n", iface_index, ep, USB_FIFO_RX);
		if (pipe == NULL) {
			DPRINTFN(5, "dev_get_pipe returned NULL\n");
			return (EINVAL);
		}
		f = usb2_fifo_alloc();
		if (f == NULL) {
			DPRINTFN(5, "could not alloc rx fifo\n");
			return (ENOMEM);
		}
		/* update some fields */
		f->fifo_index = n + USB_FIFO_RX;
		f->dev_ep_index = ep;
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
		cpd->txfifo = udev->fifo[n + USB_FIFO_TX];
	}
	if (is_rx) {
		cpd->rxfifo = udev->fifo[n + USB_FIFO_RX];
	}
	/* fill out fifo index */
	DPRINTFN(5, "fifo index = %d\n", n);
	cpd->fifo_index = n;

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
	usb2_fifo_close(f, 0);

	usb2_cv_destroy(&f->cv_io);
	usb2_cv_destroy(&f->cv_drain);

	free(f, M_USBDEV);
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
usb2_fifo_open(struct usb2_fifo *f, int fflags)
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
	if (f->opened) {
		err = EBUSY;
		goto done;
	}

	/* call open method */
	err = (f->methods->f_open) (f, fflags);
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

	/* flag the fifo as opened to prevent others */
	mtx_lock(&usb2_ref_lock);
	f->opened = 1;
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
}

/*------------------------------------------------------------------------*
 *	usb2_fifo_close
 *------------------------------------------------------------------------*/
static void
usb2_fifo_close(struct usb2_fifo *f, int fflags)
{
	int err;

	/* check if we are not opened */
	if (!f->opened) {
		/* nothing to do - already closed */
		return;
	}
	mtx_lock(f->priv_mtx);

	/* clear current file flag */
	f->opened = 0;

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
	(f->methods->f_close) (f, fflags);

	DPRINTF("closed\n");
}

/*------------------------------------------------------------------------*
 *	usb2_open - cdev callback
 *------------------------------------------------------------------------*/
static int
usb2_open(struct cdev *dev, int fflags, int devtype, struct thread *td)
{
	struct usb2_fs_privdata* pd = (struct usb2_fs_privdata*)dev->si_drv1;
	struct usb2_cdev_privdata *cpd;
	int err, ep;

	DPRINTFN(2, "fflags=0x%08x\n", fflags);

	KASSERT(fflags & (FREAD|FWRITE), ("invalid open flags"));
	if (((fflags & FREAD) && !(pd->mode & FREAD)) ||
	    ((fflags & FWRITE) && !(pd->mode & FWRITE))) {
		DPRINTFN(2, "access mode not supported\n");
		return (EPERM);
	}

	cpd = malloc(sizeof(*cpd), M_USBDEV, M_WAITOK | M_ZERO);
	ep = cpd->ep_addr = pd->ep_addr;

	usb2_loc_fill(pd, cpd);
	err = usb2_ref_device(cpd, 1);
	if (err) {
		DPRINTFN(2, "cannot ref device\n");
		free(cpd, M_USBDEV);
		return (ENXIO);
	}
	cpd->fflags = fflags;	/* access mode for open lifetime */

	/* Check if the endpoint is already open, we always allow EP0 */
	if (ep > 0) {
		if ((fflags & FREAD && cpd->udev->ep_rd_opened & (1 << ep)) ||
		    (fflags & FWRITE && cpd->udev->ep_wr_opened & (1 << ep))) {
			DPRINTFN(2, "endpoint already open\n");
			usb2_unref_device(cpd);
			free(cpd, M_USBDEV);
			return (EBUSY);
		}
		if (fflags & FREAD)
			cpd->udev->ep_rd_opened |= (1 << ep);
		if (fflags & FWRITE)
			cpd->udev->ep_wr_opened |= (1 << ep);
	}

	/* create FIFOs, if any */
	err = usb2_fifo_create(cpd);
	/* check for error */
	if (err) {
		DPRINTFN(2, "cannot create fifo\n");
		usb2_unref_device(cpd);
		free(cpd, M_USBDEV);
		return (err);
	}
	if (fflags & FREAD) {
		err = usb2_fifo_open(cpd->rxfifo, fflags);
		if (err) {
			DPRINTFN(2, "read open failed\n");
			usb2_unref_device(cpd);
			free(cpd, M_USBDEV);
			return (err);
		}
	}
	if (fflags & FWRITE) {
		err = usb2_fifo_open(cpd->txfifo, fflags);
		if (err) {
			DPRINTFN(2, "write open failed\n");
			if (fflags & FREAD) {
				usb2_fifo_close(cpd->rxfifo, fflags);
			}
			usb2_unref_device(cpd);
			free(cpd, M_USBDEV);
			return (err);
		}
	}
	usb2_unref_device(cpd);
	devfs_set_cdevpriv(cpd, usb2_close);

	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_close - cdev callback
 *------------------------------------------------------------------------*/
static void
usb2_close(void *arg)
{
	struct usb2_cdev_privdata *cpd = arg;
	struct usb2_device *udev;
	int err;

	DPRINTFN(2, "usb2_close, cpd=%p\n", cpd);

	err = usb2_ref_device(cpd, 1);
	if (err) {
		free(cpd, M_USBDEV);
		return;
	}

	udev = cpd->udev;
	if (cpd->fflags & FREAD) {
		usb2_fifo_close(cpd->rxfifo, cpd->fflags);
		/* clear read bitmask */
		udev->ep_rd_opened &= ~(1 << cpd->ep_addr);
	}
	if (cpd->fflags & FWRITE) {
		usb2_fifo_close(cpd->txfifo, cpd->fflags);
		/* clear write bitmask */
		udev->ep_wr_opened &= ~(1 << cpd->ep_addr);
	}

	usb2_unref_device(cpd);
	free(cpd, M_USBDEV);
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
}

SYSINIT(usb2_dev_init, SI_SUB_KLD, SI_ORDER_FIRST, usb2_dev_init, NULL);

static void
usb2_dev_init_post(void *arg)
{
	/*
	 * Create /dev/usb - this is needed for usbconfig(8), which
	 * needs a well-known device name to access.
	 */
	usb2_dev = make_dev(&usb2_static_devsw, 0, UID_ROOT, GID_OPERATOR,
	    0644, USB_DEVICE_NAME);
	if (usb2_dev == NULL) {
		DPRINTFN(0, "Could not create usb bus device!\n");
	}
}

SYSINIT(usb2_dev_init_post, SI_SUB_KICK_SCHEDULER, SI_ORDER_FIRST, usb2_dev_init_post, NULL);

static void
usb2_dev_uninit(void *arg)
{
	if (usb2_dev != NULL) {
		destroy_dev(usb2_dev);
		usb2_dev = NULL;
	
	}
	mtx_destroy(&usb2_ref_lock);
	sx_destroy(&usb2_sym_lock);
}

SYSUNINIT(usb2_dev_uninit, SI_SUB_KICK_SCHEDULER, SI_ORDER_ANY, usb2_dev_uninit, NULL);

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

/*------------------------------------------------------------------------*
 *	usb2_ioctl - cdev callback
 *------------------------------------------------------------------------*/
static int
usb2_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int fflag, struct thread* td)
{
	struct usb2_cdev_privdata* cpd;
	struct usb2_fifo *f;
	int fflags;
	int err;

	err = devfs_get_cdevpriv((void **)&cpd);
	if (err != 0)
		return (err);

	err = usb2_ref_device(cpd, 1);
	if (err) {
		return (ENXIO);
	}
	fflags = cpd->fflags;

	DPRINTFN(2, "fflags=%u, cmd=0x%lx\n", fflags, cmd);

	f = NULL;			/* set default value */
	err = ENOIOCTL;			/* set default value */

	if (fflags & FWRITE) {
		f = cpd->txfifo;
		err = usb2_ioctl_f_sub(f, cmd, addr, td);
	}
	if (fflags & FREAD) {
		f = cpd->rxfifo;
		err = usb2_ioctl_f_sub(f, cmd, addr, td);
	}
	KASSERT(f != NULL, ("fifo not found"));
	if (err == ENOIOCTL) {
		err = (f->methods->f_ioctl) (f, cmd, addr, fflags);
		if (err == ENOIOCTL) {
			if (usb2_uref_location(cpd)) {
				err = ENXIO;
				goto done;
			}
			err = (f->methods->f_ioctl_post) (f, cmd, addr, fflags);
		}
	}
	if (err == ENOIOCTL) {
		err = ENOTTY;
	}
done:
	usb2_unref_device(cpd);
	return (err);
}

/* ARGSUSED */
static int
usb2_poll(struct cdev* dev, int events, struct thread* td)
{
	struct usb2_cdev_privdata* cpd;
	struct usb2_fifo *f;
	struct usb2_mbuf *m;
	int fflags;
	int err, revents;

	err = devfs_get_cdevpriv((void **)&cpd);
	if (err != 0)
		return (err);

	err = usb2_ref_device(cpd, 0 /* no uref */ );
	if (err)
		return (POLLHUP);

	fflags = cpd->fflags;

	/* Figure out who needs service */
	revents = 0;
	if ((events & (POLLOUT | POLLWRNORM)) &&
	    (fflags & FWRITE)) {

		f = cpd->txfifo;

		mtx_lock(f->priv_mtx);

		if (!cpd->is_usbfs) {
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

		f = cpd->rxfifo;

		mtx_lock(f->priv_mtx);

		if (!cpd->is_usbfs) {
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

			if (!cpd->is_usbfs) {
				/* start reading data */
				(f->methods->f_start_read) (f);
			}
		}

		mtx_unlock(f->priv_mtx);
	}
	usb2_unref_device(cpd);
	return (revents);
}

static int
usb2_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct usb2_cdev_privdata* cpd;
	struct usb2_fifo *f;
	struct usb2_mbuf *m;
	int fflags;
	int resid;
	int io_len;
	int err;
	uint8_t tr_data = 0;

	err = devfs_get_cdevpriv((void **)&cpd);
	if (err != 0)
		return (err);

	err = usb2_ref_device(cpd, 0 /* no uref */ );
	if (err) {
		return (ENXIO);
	}
	fflags = cpd->fflags;

	f = cpd->rxfifo;
	if (f == NULL) {
		/* should not happen */
		return (EPERM);
	}

	resid = uio->uio_resid;

	mtx_lock(f->priv_mtx);

	/* check for permanent read error */
	if (f->flag_iserror) {
		err = EIO;
		goto done;
	}
	/* check if USB-FS interface is active */
	if (cpd->is_usbfs) {
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

			if (fflags & IO_NDELAY) {
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

	usb2_unref_device(cpd);

	return (err);
}

static int
usb2_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct usb2_cdev_privdata* cpd;
	struct usb2_fifo *f;
	struct usb2_mbuf *m;
	int fflags;
	int resid;
	int io_len;
	int err;
	uint8_t tr_data = 0;

	DPRINTFN(2, "\n");

	err = devfs_get_cdevpriv((void **)&cpd);
	if (err != 0)
		return (err);

	err = usb2_ref_device(cpd, 0 /* no uref */ );
	if (err) {
		return (ENXIO);
	}
	fflags = cpd->fflags;

	f = cpd->txfifo;
	if (f == NULL) {
		/* should not happen */
		usb2_unref_device(cpd);
		return (EPERM);
	}
	resid = uio->uio_resid;

	mtx_lock(f->priv_mtx);

	/* check for permanent write error */
	if (f->flag_iserror) {
		err = EIO;
		goto done;
	}
	/* check if USB-FS interface is active */
	if (cpd->is_usbfs) {
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

			if (fflags & IO_NDELAY) {
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

	usb2_unref_device(cpd);

	return (err);
}

int
usb2_static_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	union {
		struct usb2_read_dir *urd;
		void* data;
	} u;
	int err = ENOTTY;

	u.data = data;
	switch (cmd) {
		case USB_READ_DIR:
			err = usb2_read_symlink(u.urd->urd_data,
			    u.urd->urd_startentry, u.urd->urd_maxlen);
			break;
		case USB_DEV_QUIRK_GET:
		case USB_QUIRK_NAME_GET:
		case USB_DEV_QUIRK_ADD:
		case USB_DEV_QUIRK_REMOVE:
			err = usb2_quirk_ioctl_p(cmd, data, fflag, td);
			break;
		case USB_GET_TEMPLATE:
			*(int *)data = usb2_template;
			break;
		case USB_SET_TEMPLATE:
			err = priv_check(curthread, PRIV_DRIVER);
			if (err)
				break;
			usb2_template = *(int *)data;
			break;
	}
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
}

static int
usb2_fifo_dummy_open(struct usb2_fifo *fifo, int fflags)
{
	return (0);
}

static void
usb2_fifo_dummy_close(struct usb2_fifo *fifo, int fflags)
{
	return;
}

static int
usb2_fifo_dummy_ioctl(struct usb2_fifo *fifo, u_long cmd, void *addr, int fflags)
{
	return (ENOIOCTL);
}

static void
usb2_fifo_dummy_cmd(struct usb2_fifo *fifo)
{
	fifo->flag_flushing = 0;	/* not flushing */
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
    uint8_t iface_index, uid_t uid, gid_t gid, int mode)
{
	struct usb2_fifo *f_tx;
	struct usb2_fifo *f_rx;
	char devname[32];
	uint8_t n;
	struct usb2_fs_privdata* pd;

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

	for (n = 0; n != 4; n++) {

		if (pm->basename[n] == NULL) {
			continue;
		}
		if (subunit == 0xFFFF) {
			if (snprintf(devname, sizeof(devname),
			    "%s%u%s", pm->basename[n],
			    unit, pm->postfix[n] ?
			    pm->postfix[n] : "")) {
				/* ignore */
			}
		} else {
			if (snprintf(devname, sizeof(devname),
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
			    usb2_alloc_symlink(devname);
		} else {
			f_tx->symlink[n / 2] =
			    usb2_alloc_symlink(devname);
		}

		/*
		 * Initialize device private data - this is used to find the
		 * actual USB device itself.
		 */
		pd = malloc(sizeof(struct usb2_fs_privdata), M_USBDEV, M_WAITOK | M_ZERO);
		pd->bus_index = device_get_unit(udev->bus->bdev);
		pd->dev_index = udev->device_index;
		pd->iface_index = iface_index;
		pd->ep_addr = -1;	/* not an endpoint */
		pd->fifo_index = f_tx->fifo_index;
		pd->mode = FREAD|FWRITE;

		/* Now, create the device itself */
		f_sc->dev = make_dev(&usb2_devsw, 0, uid, gid, mode,
		    devname);
		f_sc->dev->si_drv1 = pd;
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
}

static void
usb2_fifo_cleanup(void* ptr) 
{
	free(ptr, M_USBDEV);
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

	if (f_sc->dev != NULL) {
		destroy_dev_sched_cb(f_sc->dev, usb2_fifo_cleanup, f_sc->dev->si_drv1);
	}

	DPRINTFN(2, "detached %p\n", f_sc);
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
}

/*------------------------------------------------------------------------*
 *	usb2_alloc_symlink
 *
 * Return values:
 * NULL: Failure
 * Else: Pointer to symlink entry
 *------------------------------------------------------------------------*/
struct usb2_symlink *
usb2_alloc_symlink(const char *target)
{
	struct usb2_symlink *ps;

	ps = malloc(sizeof(*ps), M_USBDEV, M_WAITOK);
	if (ps == NULL) {
		return (ps);
	}
	/* XXX no longer needed */
	strlcpy(ps->src_path, target, sizeof(ps->src_path));
	ps->src_len = strlen(ps->src_path);
	strlcpy(ps->dst_path, target, sizeof(ps->dst_path));
	ps->dst_len = strlen(ps->dst_path);

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
