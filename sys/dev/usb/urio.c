/*
 * Copyright (c) 2000 Iwasa Kazmi
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


/*
 * 2000/3/24  added NetBSD/OpenBSD support (from Alex Nemirovsky)
 * 2000/3/07  use two bulk-pipe handles for read and write (Dirk)
 * 2000/3/06  change major number(143), and copyright header
 *            some fix for 4.0 (Dirk)
 * 2000/3/05  codes for FreeBSD 4.x - CURRENT (Thanks to Dirk-Willem van Gulik)
 * 2000/3/01  remove retry code from urioioctl()
 *            change method of bulk transfer (no interrupt)
 * 2000/2/28  small fixes for new rio_usb.h
 * 2000/2/24  first version.
 */

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
#endif
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/tty.h>
#include <sys/file.h>
#if __FreeBSD_version >= 500014
#include <sys/selinfo.h>
#else
#include <sys/select.h>
#endif
#include <sys/vnode.h>
#include <sys/poll.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <dev/usb/usbdevs.h>
#include <dev/usb/rio500_usb.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (uriodebug) logprintf x
#define DPRINTFN(n,x)	if (uriodebug>(n)) logprintf x
int	uriodebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, urio, CTLFLAG_RW, 0, "USB urio");
SYSCTL_INT(_hw_usb_urio, OID_AUTO, debug, CTLFLAG_RW,
	   &uriodebug, 0, "urio debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/* difference of usbd interface */
#define USBDI 1

#define RIO_OUT 0
#define RIO_IN  1
#define RIO_NODIR  2

#if defined(__NetBSD__)
int urioopen(dev_t, int, int, struct proc *);
int urioclose(dev_t, int, int, struct proc *p);
int urioread(dev_t, struct uio *uio, int);
int uriowrite(dev_t, struct uio *uio, int);
int urioioctl(dev_t, u_long, caddr_t, int, struct proc *);

cdev_decl(urio);
#define RIO_UE_GET_DIR(p) ((UE_GET_DIR(p) == UE_DIR_IN) ? RIO_IN :\
			  ((UE_GET_DIR(p) == UE_DIR_OUT) ? RIO_OUT :\
							   RIO_NODIR))
#elif defined(__FreeBSD__)
d_open_t  urioopen;
d_close_t urioclose;
d_read_t  urioread;
d_write_t uriowrite;
d_ioctl_t urioioctl;

#define URIO_CDEV_MAJOR	143

Static struct cdevsw urio_cdevsw = {
	.d_open =	urioopen,
	.d_close =	urioclose,
	.d_read =	urioread,
	.d_write =	uriowrite,
	.d_ioctl =	urioioctl,
	.d_name =	"urio",
	.d_maj =	URIO_CDEV_MAJOR,
#if __FreeBSD_version < 500014
 	.d_bmaj =	-1
#endif
};
#define RIO_UE_GET_DIR(p) ((UE_GET_DIR(p) == UE_DIR_IN) ? RIO_IN :\
		 	  ((UE_GET_DIR(p) == UE_DIR_OUT) ? RIO_OUT :\
			    				   RIO_NODIR))
#endif  /*defined(__FreeBSD__)*/

#define	URIO_BBSIZE	1024

struct urio_softc {
 	USBBASEDEVICE sc_dev;
	usbd_device_handle sc_udev;
	usbd_interface_handle sc_iface;

	int sc_opened;
	usbd_pipe_handle sc_pipeh_in;
	usbd_pipe_handle sc_pipeh_out;
	int sc_epaddr[2];

	int sc_refcnt;
#if defined(__FreeBSD__)
	dev_t sc_dev_t;
#endif	/* defined(__FreeBSD__) */
#if defined(__NetBSD__) || defined(__OpenBSD__)
	u_char sc_dying;
#endif
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
	    ((UGETW(dd->idVendor) == USB_VENDOR_DIAMOND &&
	    UGETW(dd->idProduct) == USB_PRODUCT_DIAMOND_RIO500USB) ||
	    (UGETW(dd->idVendor) == USB_VENDOR_DIAMOND2 &&
	      (UGETW(dd->idProduct) == USB_PRODUCT_DIAMOND2_RIO600USB ||
	      UGETW(dd->idProduct) == USB_PRODUCT_DIAMOND2_RIO800USB))))
		return UMATCH_VENDOR_PRODUCT;
	else
		return UMATCH_NONE;
}

USB_ATTACH(urio)
{
	USB_ATTACH_START(urio, sc, uaa);
	char devinfo[1024];
	usbd_device_handle udev;
	usbd_interface_handle iface;
	u_int8_t epcount;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	u_int8_t niface;
#endif
	usbd_status r;
	char * ermsg = "<none>";
	int i;

	DPRINTFN(10,("urio_attach: sc=%p\n", sc));
	usbd_devinfo(uaa->device, 0, devinfo);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfo);

	sc->sc_udev = udev = uaa->device;

#if defined(__FreeBSD__)
 	if ((!uaa->device) || (!uaa->iface)) {
		ermsg = "device or iface";
 		goto nobulk;
	}
	sc->sc_iface = iface = uaa->iface;
#elif defined(__NetBSD__) || defined(__OpenBSD__)
 	if (!udev) {
		ermsg = "device";
 		goto nobulk;
	}
	r = usbd_interface_count(udev, &niface);
	if (r) {
		ermsg = "iface";
		goto nobulk;
	}
	r = usbd_device2interface_handle(udev, 0, &iface);
	if (r) {
		ermsg = "iface";
		goto nobulk;
	}
	sc->sc_iface = iface;
#endif
	sc->sc_opened = 0;
	sc->sc_pipeh_in = 0;
	sc->sc_pipeh_out = 0;
	sc->sc_refcnt = 0;

	r = usbd_endpoint_count(iface, &epcount);
	if (r != USBD_NORMAL_COMPLETION) {
		ermsg = "endpoints";
		goto nobulk;
	}

	sc->sc_epaddr[RIO_OUT] = 0xff;
	sc->sc_epaddr[RIO_IN] = 0x00;

	for (i = 0; i < epcount; i++) {
		usb_endpoint_descriptor_t *edesc =
			usbd_interface2endpoint_descriptor(iface, i);
		int d;

		if (!edesc) {
			ermsg = "interface endpoint";
			goto nobulk;
		}

		d = RIO_UE_GET_DIR(edesc->bEndpointAddress);
		if (d != RIO_NODIR)
			sc->sc_epaddr[d] = edesc->bEndpointAddress;
	}
	if ( sc->sc_epaddr[RIO_OUT] == 0xff ||
	     sc->sc_epaddr[RIO_IN] == 0x00) {
		ermsg = "Rio I&O";
		goto nobulk;
	}

#if defined(__FreeBSD__)
	/* XXX no error trapping, no storing of dev_t */
	sc->sc_dev_t = make_dev(&urio_cdevsw, device_get_unit(self),
			UID_ROOT, GID_OPERATOR,
			0644, "urio%d", device_get_unit(self));
#elif defined(__NetBSD__) || defined(__OpenBSD__)
	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));
#endif

	DPRINTFN(10, ("urio_attach: %p\n", sc->sc_udev));

	USB_ATTACH_SUCCESS_RETURN;

 nobulk:
	printf("%s: could not find %s\n", USBDEVNAME(sc->sc_dev),ermsg);
	USB_ATTACH_ERROR_RETURN;
}


int
urioopen(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
#if (USBDI >= 1)
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
	sc->sc_pipeh_in = 0;
	sc->sc_pipeh_out = 0;
	if (usbd_open_pipe(sc->sc_iface,
		sc->sc_epaddr[RIO_IN], 0, &sc->sc_pipeh_in)
	   		!= USBD_NORMAL_COMPLETION)
	{
			sc->sc_pipeh_in = 0;
			return EIO;
	};
	if (usbd_open_pipe(sc->sc_iface,
		sc->sc_epaddr[RIO_OUT], 0, &sc->sc_pipeh_out)
	   		!= USBD_NORMAL_COMPLETION)
	{
			usbd_close_pipe(sc->sc_pipeh_in);
			sc->sc_pipeh_in = 0;
			sc->sc_pipeh_out = 0;
			return EIO;
	};
	return 0;
}

int
urioclose(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
#if (USBDI >= 1)
	struct urio_softc * sc;
#endif
	int unit = URIOUNIT(dev);
	USB_GET_SC(urio, unit, sc);

	DPRINTFN(5, ("urioclose: flag=%d, mode=%d, unit=%d\n", flag, mode, unit));
	if (sc->sc_pipeh_in)
		usbd_close_pipe(sc->sc_pipeh_in);

	if (sc->sc_pipeh_out)
		usbd_close_pipe(sc->sc_pipeh_out);

	sc->sc_pipeh_in = 0;
	sc->sc_pipeh_out = 0;
	sc->sc_opened = 0;
	sc->sc_refcnt = 0;
	return 0;
}

int
urioread(dev_t dev, struct uio *uio, int flag)
{
#if (USBDI >= 1)
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

#if (USBDI >= 1)
	sc->sc_refcnt++;
	reqh = usbd_alloc_xfer(sc->sc_udev);
#else
	reqh = usbd_alloc_request();
#endif
	if (reqh == 0)
		return ENOMEM;
	while ((n = min(URIO_BBSIZE, uio->uio_resid)) != 0) {
		DPRINTFN(1, ("urioread: start transfer %d bytes\n", n));
		tn = n;
#if (USBDI >= 1)
 		usbd_setup_xfer(reqh, sc->sc_pipeh_in, 0, buf, tn,
				       0, RIO_RW_TIMEOUT, 0);
#else
		r = usbd_setup_request(reqh, sc->sc_pipeh_in, 0, buf, tn,
				       0, RIO_RW_TIMEOUT, 0);
		if (r != USBD_NORMAL_COMPLETION) {
			error = EIO;
			break;
		}
#endif
		r = usbd_sync_transfer(reqh);
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTFN(1, ("urioread: error=%d\n", r));
			usbd_clear_endpoint_stall(sc->sc_pipeh_in);
			tn = 0;
			error = EIO;
			break;
		}
#if (USBDI >= 1)
		usbd_get_xfer_status(reqh, 0, 0, &tn, 0);
#else
		usbd_get_request_status(reqh, &r_priv, &r_buff, &tn, &r_status);
#endif

		DPRINTFN(1, ("urioread: got %d bytes\n", tn));
		error = uiomove(buf, tn, uio);
		if (error || tn < n)
			break;
	}
#if (USBDI >= 1)
	usbd_free_xfer(reqh);
#else
	usbd_free_request(reqh);
#endif

	return error;
}

int
uriowrite(dev_t dev, struct uio *uio, int flag)
{
#if (USBDI >= 1)
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

#if (USBDI >= 1)
	sc->sc_refcnt++;
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
#if (USBDI >= 1)
 		usbd_setup_xfer(reqh, sc->sc_pipeh_out, 0, buf, n,
				       0, RIO_RW_TIMEOUT, 0);
#else
		r = usbd_setup_request(reqh, sc->sc_pipeh_out, 0, buf, n,
				       0, RIO_RW_TIMEOUT, 0);
		if (r != USBD_NORMAL_COMPLETION) {
			error = EIO;
			break;
		}
#endif
		r = usbd_sync_transfer(reqh);
		if (r != USBD_NORMAL_COMPLETION) {
			DPRINTFN(1, ("uriowrite: error=%d\n", r));
			usbd_clear_endpoint_stall(sc->sc_pipeh_out);
			error = EIO;
			break;
		}
#if (USBDI >= 1)
		usbd_get_xfer_status(reqh, 0, 0, 0, 0);
#endif
	}

#if (USBDI >= 1)
	usbd_free_xfer(reqh);
#else
	usbd_free_request(reqh);
#endif

	return error;
}


int
urioioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, usb_proc_ptr p)
{
#if (USBDI >= 1)
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
				  ptr, req_flags, &req_actlen,
				  USBD_DEFAULT_TIMEOUT);
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


#if defined(__NetBSD__) || defined(__OpenBSD__)
int
urio_activate(device_ptr_t self, enum devact act)
{
	struct urio_softc *sc = (struct urio_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}

USB_DETACH(urio)
{
	USB_DETACH_START(urio, sc);
	struct urio_endpoint *sce;
	int i, dir;
	int s;
#if defined(__NetBSD__) || defined(__OpenBSD__)
	int maj, mn;

	DPRINTF(("urio_detach: sc=%p flags=%d\n", sc, flags));
#elif defined(__FreeBSD__)
	DPRINTF(("urio_detach: sc=%p\n", sc));
#endif

	sc->sc_dying = 1;
	/* Abort all pipes.  Causes processes waiting for transfer to wake. */
#if 0
	for (i = 0; i < USB_MAX_ENDPOINTS; i++) {
		for (dir = OUT; dir <= IN; dir++) {
			sce = &sc->sc_endpoints[i][dir];
			if (sce && sce->pipeh)
				usbd_abort_pipe(sce->pipeh);
		}
	}

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wake everyone */
		for (i = 0; i < USB_MAX_ENDPOINTS; i++)
			wakeup(&sc->sc_endpoints[i][IN]);
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);
#else
	if (sc->sc_pipeh_in)
		usbd_abort_pipe(sc->sc_pipeh_in);

	if (sc->sc_pipeh_out)
		usbd_abort_pipe(sc->sc_pipeh_out);

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == urioopen)
			break;

	/* Nuke the vnodes for any open instances (calls close). */
	mn = self->dv_unit * USB_MAX_ENDPOINTS;
	vdevgone(maj, mn, mn + USB_MAX_ENDPOINTS - 1, VCHR);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
			   USBDEV(sc->sc_dev));

	return (0);
}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

#if defined(__FreeBSD__)
Static int
urio_detach(device_t self)
{
	struct urio_softc *sc = device_get_softc(self);

	DPRINTF(("%s: disconnected\n", USBDEVNAME(self)));
	destroy_dev(sc->sc_dev_t);
	/* XXX not implemented yet */
	device_set_desc(self, NULL);
	return 0;
}

DRIVER_MODULE(urio, uhub, urio_driver, urio_devclass, usbd_driver_load, 0);
#endif
