/*
 * Copyright (c) 2000 Iwasa Kazmi <kzmi@ca2.so-net.ne.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * This code is based on ugen.c and ulpt.c developed by Lennart Augustsson.
 * This code includes software developed by the NetBSD Foundation, Inc. and
 * its contributors.
 */

/* $FreeBSD$ */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#endif
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/rio_usb.h>

#ifdef URIO_DEBUG
#define DPRINTF(x)	if (uriodebug) logprintf x
#define DPRINTFN(n,x)	if (uriodebug>(n)) logprintf x
int	uriodebug = 100;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif


#if defined(__NetBSD__)
int urioopen __P((dev_t, int, int, struct proc *));
int urioclose __P((dev_t, int, int, struct proc *p));
int urioread __P((dev_t, struct uio *uio, int));
int uriowrite __P((dev_t, struct uio *uio, int));
int urioioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
#elif defined(__FreeBSD__)
d_open_t  urioopen;
d_close_t urioclose;
d_read_t  urioread;
d_write_t uriowrite;
d_ioctl_t urioioctl;

#define URIO_CDEV_MAJOR		143

#define RIO_OUT 0
#define RIO_IN  1
#define RIO_NODIR  2

#if (__FreeBSD__ >= 4)
static struct cdevsw urio_cdevsw = {
	urioopen,	urioclose,	urioread,	uriowrite,
 	urioioctl,	nopoll,		nommap,		nostrategy,
 	"urio",		URIO_CDEV_MAJOR,nodump,		nopsize,
 	0,		-1
};
#define RIO_UE_GET_DIR(p) ((UE_GET_DIR(p) == UE_DIR_IN) ? RIO_IN :\
		 	  ((UE_GET_DIR(p) == UE_DIR_OUT) ? RIO_OUT :\
			    				   RIO_NODIR))
#else
static struct cdevsw urio_cdevsw = {
	urioopen,	urioclose,	urioread,	uriowrite,
	urioioctl,	nostop,		nullreset,	nodevtotty,
	seltrue,	nommap,		nostrat,
	"urio",		NULL,		-1
};
#define USBBASEDEVICE bdevice
#define RIO_UE_GET_DIR(p) UE_GET_IN(p)
#endif

#endif  /*defined(__FreeBSD__)*/

#define	URIO_BBSIZE	1024

struct urio_softc {
 	USBBASEDEVICE sc_dev;
	usbd_device_handle sc_udev;
	usbd_interface_handle sc_iface;

	int sc_opened;
	usbd_pipe_handle sc_pipeh;
	int sc_dir;

	int sc_epaddr[2];
};

#define URIOUNIT(n) (minor(n))

#define RIO_RW_TIMEOUT 4000	/* ms */

USB_DECLARE_DRIVER(urio);

USB_MATCH(urio)
{
	USB_MATCH_START(urio, uaa);
	usb_device_descriptor_t *dd;

	DPRINTFN(10,("urio_match\n"));
	if (!uaa->iface)
		return UMATCH_NONE;

	dd = usbd_get_device_descriptor(uaa->device);

	if (dd &&
	    UGETW(dd->idVendor) == USB_VENDOR_DIAMOND &&
	    UGETW(dd->idProduct) == USB_PRODUCT_DIAMOND_RIO500USB)
		return UMATCH_VENDOR_PRODUCT;
	else
		return UMATCH_NONE;
}

USB_ATTACH(urio)
{
	USB_ATTACH_START(urio, sc, uaa);
	char devinfo[1024];
	usb_endpoint_descriptor_t *edesc;
	u_int8_t epcount;
	usbd_status r;
	char * ermsg = "<none>";
	int i;

	DPRINTFN(10,("urio_attach: sc=%p\n", sc));	
	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfo);

	sc->sc_udev = uaa->device;
 	if ((!uaa->device) || (!uaa->iface)) {
		ermsg = "device or iface";
 		goto nobulk;
	}

	sc->sc_iface = uaa->iface;
	sc->sc_opened = 0;

	r = usbd_endpoint_count(uaa->iface, &epcount);
	if (r != USBD_NORMAL_COMPLETION) { 
		ermsg = "endpoints";
		goto nobulk;
	}

	sc->sc_epaddr[RIO_OUT] = 0xff;
	sc->sc_epaddr[RIO_IN] = 0x00;
	for (i = 0; i < epcount; i++) {
		edesc = usbd_interface2endpoint_descriptor(uaa->iface, i);
		if (!edesc) {
			ermsg = "interface endpoint";
			goto nobulk;
		}
		sc->sc_epaddr[RIO_UE_GET_DIR(edesc->bEndpointAddress)]
			= edesc->bEndpointAddress;
	}
	if ( sc->sc_epaddr[RIO_OUT] == 0xff ||
	     sc->sc_epaddr[RIO_IN] == 0x00) {
		ermsg = "Rio I&O";
		goto nobulk;
	}

#if (__FreeBSD__ >= 4)
	/* XXX no error trapping, no storing of dev_t */
	(void) make_dev(&urio_cdevsw, device_get_unit(self),
			UID_ROOT, GID_OPERATOR,
			0644, "urio%d", device_get_unit(self));
#endif

	DPRINTFN(10, ("urio_attach: %p\n", sc->sc_udev));

	USB_ATTACH_SUCCESS_RETURN;

 nobulk:
	printf("%s: could not find %s\n", USBDEVNAME(sc->sc_dev),ermsg);
	USB_ATTACH_ERROR_RETURN;
}


int
urioopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
#if (__FreeBSD__ >= 4)
	struct urio_softc * sc;
#endif
	int unit = URIOUNIT(dev);
	USB_GET_SC_OPEN(urio, unit, sc);

	DPRINTFN(5, ("urioopen: flag=%d, mode=%d, unit=%d\n", 
		     flag, mode, unit));

	if (sc->sc_opened)
		return EBUSY;

	if ((flag & (FWRITE|FREAD)) != (FWRITE|FREAD))
		return EACCES;

	sc->sc_opened = 1;
	sc->sc_pipeh = 0;
	sc->sc_dir = RIO_NODIR;
	return 0;
}

int
urioclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
#if (__FreeBSD__ >= 4)
	struct urio_softc * sc;
#endif
	int unit = URIOUNIT(dev);
	USB_GET_SC(urio, unit, sc);

	DPRINTFN(5, ("urioclose: flag=%d, mode=%d, unit=%d\n", flag, mode, unit));
	if (sc->sc_pipeh) {
		/*usbd_abort_pipe(sc->sc_pipeh);*/
		usbd_close_pipe(sc->sc_pipeh);	
	}
	sc->sc_opened = 0;

	return 0;	
}

int
urioread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
#if (__FreeBSD__ >= 4)
	struct urio_softc * sc;
	usbd_xfer_handle reqh;
#else
	usbd_request_handle reqh;
	usbd_private_handle r_priv;
        void *r_buff;
        usbd_status r_status;
#endif
	int unit = URIOUNIT(dev);
	usbd_status r;
	char buf[URIO_BBSIZE];
	u_int32_t n, tn;
	int error = 0;

	USB_GET_SC(urio, unit, sc);

	DPRINTFN(5, ("urioread: %d\n", unit));
	if (!sc->sc_opened)
		return EIO;

	if (sc->sc_dir != RIO_IN) {
		if (sc->sc_pipeh) {
			/*usbd_abort_pipe(sc->sc_pipeh);*/
			usbd_close_pipe(sc->sc_pipeh);	
		}
		sc->sc_pipeh = 0;
		sc->sc_dir = RIO_NODIR;
		r = usbd_open_pipe(sc->sc_iface, 
		                   sc->sc_epaddr[RIO_IN], 0, 
		                   &sc->sc_pipeh);
		if (r != USBD_NORMAL_COMPLETION)
			return EIO;
		sc->sc_dir = RIO_IN;
	}

#if (__FreeBSD__ >= 4)
	reqh = usbd_alloc_xfer(sc->sc_udev);
#else
	reqh = usbd_alloc_request();
#endif
	if (reqh == 0)
		return ENOMEM;
	while ((n = min(URIO_BBSIZE, uio->uio_resid)) != 0) {
		DPRINTFN(1, ("urioread: start transfer %d bytes\n", n));
		tn = n;
#if (__FreeBSD__ >= 4)
 		usbd_setup_xfer(reqh, sc->sc_pipeh, 0, buf, tn,
				       0, RIO_RW_TIMEOUT, 0);
#else
		r = usbd_setup_request(reqh, sc->sc_pipeh, 0, buf, tn,
				       0, RIO_RW_TIMEOUT, 0);
		if (r != USBD_NORMAL_COMPLETION) {
			error = EIO;
			break;
		}
#endif
		r = usbd_sync_transfer(reqh);
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTFN(1, ("urioread: error=%d\n", r));
			usbd_clear_endpoint_stall(sc->sc_pipeh);
			tn = 0;
			error = EIO;
			break;
		}
#if (__FreeBSD__ >= 4)
		usbd_get_xfer_status(reqh, 0, 0, &tn, 0);
#else
		usbd_get_request_status(reqh, &r_priv, &r_buff, &tn, &r_status);
#endif

		DPRINTFN(1, ("urioread: got %d bytes\n", tn));
		error = uiomove(buf, tn, uio);
		if (error || tn < n)
			break;
	}
#if (__FreeBSD__ >= 4)
	usbd_free_xfer(reqh);
#else
	usbd_free_request(reqh);
#endif

	return error;
}

int
uriowrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
#if (__FreeBSD__ >= 4)
	struct urio_softc * sc;
	usbd_xfer_handle reqh;
#else
	usbd_request_handle reqh;
#endif
	int unit = URIOUNIT(dev);
	usbd_status r;
	char buf[URIO_BBSIZE];
	u_int32_t n;
	int error = 0;

	USB_GET_SC(urio, unit, sc);

	DPRINTFN(5, ("uriowrite: %d\n", unit));
	if (!sc->sc_opened)
		return EIO;

	if (sc->sc_dir != RIO_OUT) {
		if (sc->sc_pipeh) {
			/*usbd_abort_pipe(sc->sc_pipeh);*/
			usbd_close_pipe(sc->sc_pipeh);	
		}
		sc->sc_pipeh = 0;
		sc->sc_dir = RIO_NODIR;
		r = usbd_open_pipe(sc->sc_iface, 
		                   sc->sc_epaddr[RIO_OUT], 0, 
		                   &sc->sc_pipeh);
		if (r != USBD_NORMAL_COMPLETION)
			return EIO;
		sc->sc_dir = RIO_OUT;
	}

#if (__FreeBSD__ >= 4)
	reqh = usbd_alloc_xfer(sc->sc_udev);
#else
	reqh = usbd_alloc_request();
#endif
	if (reqh == 0)
		return EIO;
	while ((n = min(URIO_BBSIZE, uio->uio_resid)) != 0) {
		error = uiomove(buf, n, uio);
		if (error)
			break;
		DPRINTFN(1, ("uriowrite: transfer %d bytes\n", n));
#if (__FreeBSD__ >= 4)
 		usbd_setup_xfer(reqh, sc->sc_pipeh, 0, buf, n,
				       0, RIO_RW_TIMEOUT, 0);
#else
		r = usbd_setup_request(reqh, sc->sc_pipeh, 0, buf, n,
				       0, RIO_RW_TIMEOUT, 0);
		if (r != USBD_NORMAL_COMPLETION) {
			error = EIO;
			break;
		}
#endif
		r = usbd_sync_transfer(reqh);
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTFN(1, ("uriowrite: error=%d\n", r));
			usbd_clear_endpoint_stall(sc->sc_pipeh);
			error = EIO;
			break;
		}
#if (__FreeBSD__ >= 4)
		usbd_get_xfer_status(reqh, 0, 0, 0, 0);
#endif
	}

#if (__FreeBSD__ >= 4)
	usbd_free_xfer(reqh);
#else
	usbd_free_request(reqh);
#endif

	return error;
}


int
urioioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr; 
	int flag;
	struct proc *p;
{
#if (__FreeBSD__ >= 4)
	struct urio_softc * sc;
#endif
	int unit = URIOUNIT(dev);
	struct RioCommand *rio_cmd;
	int requesttype, len;
	struct iovec iov;
	struct uio uio;
	usb_device_request_t req;
	int req_flags = 0, req_actlen = 0;
	void *ptr = 0;
	int error = 0;
	usbd_status r;

	USB_GET_SC(urio, unit, sc);

	switch (cmd) {
	case RIO_RECV_COMMAND:
		if (!(flag & FWRITE))
			return EPERM;
		rio_cmd = (struct RioCommand *)addr;
		if (rio_cmd == NULL)
			return EINVAL;
		len = rio_cmd->length;

		requesttype = rio_cmd->requesttype | UT_READ_VENDOR_DEVICE;
		DPRINTFN(1,("sending command:reqtype=%0x req=%0x value=%0x index=%0x len=%0x\n", 
			requesttype, rio_cmd->request, rio_cmd->value, rio_cmd->index, len));
		break;

	case RIO_SEND_COMMAND:
		if (!(flag & FWRITE))
			return EPERM;
		rio_cmd = (struct RioCommand *)addr;
		if (rio_cmd == NULL)
			return EINVAL;
		len = rio_cmd->length;

		requesttype = rio_cmd->requesttype | UT_WRITE_VENDOR_DEVICE;
		DPRINTFN(1,("sending command:reqtype=%0x req=%0x value=%0x index=%0x len=%0x\n", 
			requesttype, rio_cmd->request, rio_cmd->value, rio_cmd->index, len));
		break;

	default:
		return EINVAL;
		break;
	}

	/* Send rio control message */
	req.bmRequestType = requesttype;
	req.bRequest = rio_cmd->request;
	USETW(req.wValue, rio_cmd->value);
	USETW(req.wIndex, rio_cmd->index);
	USETW(req.wLength, len);

	if (len < 0 || len > 32767)
		return EINVAL;
	if (len != 0) {
		iov.iov_base = (caddr_t)rio_cmd->buffer;
		iov.iov_len = len;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_resid = len;
		uio.uio_offset = 0;
		uio.uio_segflg = UIO_USERSPACE;
		uio.uio_rw =
			req.bmRequestType & UT_READ ? 
			UIO_READ : UIO_WRITE;
		uio.uio_procp = p;
		ptr = malloc(len, M_TEMP, M_WAITOK);
		if (uio.uio_rw == UIO_WRITE) {
			error = uiomove(ptr, len, &uio);
			if (error)
				goto ret;
		}
	}

	r = usbd_do_request_flags(sc->sc_udev, &req, 
				  ptr, req_flags, &req_actlen);
	if (r == USBD_NORMAL_COMPLETION) {
		error = 0;
		if (len != 0) {
			if (uio.uio_rw == UIO_READ) {
				error = uiomove(ptr, len, &uio);
			}
		}
	} else {
		error = EIO;
	}

ret:
	if (ptr)
		free(ptr, M_TEMP);
	return error;
}


#if defined(__FreeBSD__)
static int
urio_detach(device_t self)
{       
	DPRINTF(("%s: disconnected\n", USBDEVNAME(self)));
	device_set_desc(self, NULL);
	return 0;
}

#if (__FreeBSD__ >= 4)
DRIVER_MODULE(urio, uhub, urio_driver, urio_devclass, usbd_driver_load, 0);
#else
CDEV_DRIVER_MODULE(urio, uhub, urio_driver, urio_devclass,
			URIO_CDEV_MAJOR, urio_cdevsw, usbd_driver_load, 0);
#endif

#endif
