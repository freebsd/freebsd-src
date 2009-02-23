/*	$NetBSD: ugen.c,v 1.79 2006/03/01 12:38:13 yamt Exp $	*/

/* Also already merged from NetBSD:
 *	$NetBSD: ugen.c,v 1.61 2002/09/23 05:51:20 simonb Exp $
 *	$NetBSD: ugen.c,v 1.64 2003/06/28 14:21:46 darrenr Exp $
 *	$NetBSD: ugen.c,v 1.65 2003/06/29 22:30:56 fvdl Exp $
 *	$NetBSD: ugen.c,v 1.68 2004/06/23 02:30:52 mycroft Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/clist.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/ioccom.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/file.h>
#include <sys/selinfo.h>
#include <sys/poll.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#ifdef USB_DEBUG
#define DPRINTF(x)	if (ugendebug) printf x
#define DPRINTFN(n,x)	if (ugendebug>(n)) printf x
int	ugendebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, ugen, CTLFLAG_RW, 0, "USB ugen");
SYSCTL_INT(_hw_usb_ugen, OID_AUTO, debug, CTLFLAG_RW,
	   &ugendebug, 0, "ugen debug level");
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define	UGEN_CHUNK	128	/* chunk size for read */
#define	UGEN_IBSIZE	1020	/* buffer size */
#define	UGEN_BBSIZE	1024

#define	UGEN_NISOFRAMES	500	/* 0.5 seconds worth */
#define UGEN_NISOREQS	6	/* number of outstanding xfer requests */
#define UGEN_NISORFRMS	4	/* number of frames (miliseconds) per req */

struct ugen_endpoint {
	struct ugen_softc *sc;
	struct cdev *dev;
	usb_endpoint_descriptor_t *edesc;
	usbd_interface_handle iface;
	int state;
#define	UGEN_ASLP	0x02	/* waiting for data */
#define UGEN_SHORT_OK	0x04	/* short xfers are OK */
	usbd_pipe_handle pipeh;
	struct clist q;
	struct selinfo rsel;
	u_char *ibuf;		/* start of buffer (circular for isoc) */
	u_char *fill;		/* location for input (isoc) */
	u_char *limit;		/* end of circular buffer (isoc) */
	u_char *cur;		/* current read location (isoc) */
	u_int32_t timeout;
	struct isoreq {
		struct ugen_endpoint *sce;
		usbd_xfer_handle xfer;
		void *dmabuf;
		u_int16_t sizes[UGEN_NISORFRMS];
	} isoreqs[UGEN_NISOREQS];
};

struct ugen_softc {
	device_t sc_dev;		/* base device */
	usbd_device_handle sc_udev;
	struct cdev *dev;

	char sc_is_open[USB_MAX_ENDPOINTS];
	struct ugen_endpoint sc_endpoints[USB_MAX_ENDPOINTS][2];
#define OUT 0
#define IN  1

#define	UGEN_DEV_REF(dev, sc)					\
	if ((sc)->sc_dying || dev_refthread(dev) == NULL)	\
		return (ENXIO)
#define	UGEN_DEV_RELE(dev, sc)	\
	dev_relthread(dev)
#define	UGEN_DEV_OPEN(dev, sc)		\
	/* handled by dev layer */
#define	UGEN_DEV_CLOSE(dev, sc)		\
	/* handled by dev layer */
	u_char sc_dying;
};

d_open_t  ugenopen;
d_close_t ugenclose;
d_read_t  ugenread;
d_write_t ugenwrite;
d_ioctl_t ugenioctl;
d_poll_t  ugenpoll;
d_purge_t ugenpurge;

static struct cdevsw ugenctl_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	ugenopen,
	.d_close =	ugenclose,
	.d_ioctl =	ugenioctl,
	.d_purge =	ugenpurge,
	.d_name =	"ugenctl",
};

static struct cdevsw ugen_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	ugenopen,
	.d_close =	ugenclose,
	.d_read =	ugenread,
	.d_write =	ugenwrite,
	.d_ioctl =	ugenioctl,
	.d_poll =	ugenpoll,
	.d_purge =	ugenpurge,
	.d_name =	"ugen",
};

static void ugenintr(usbd_xfer_handle xfer, usbd_private_handle addr,
		     usbd_status status);
static void ugen_isoc_rintr(usbd_xfer_handle xfer, usbd_private_handle addr,
			    usbd_status status);
static int ugen_do_read(struct ugen_softc *, int, struct uio *, int);
static int ugen_do_write(struct ugen_softc *, int, struct uio *, int);
static int ugen_do_ioctl(struct ugen_softc *, int, u_long,
			 caddr_t, int, struct thread *);
static void ugen_make_devnodes(struct ugen_softc *sc);
static void ugen_destroy_devnodes(struct ugen_softc *sc);
static int ugen_set_config(struct ugen_softc *sc, int configno);
static usb_config_descriptor_t *ugen_get_cdesc(struct ugen_softc *sc,
					       int index, int *lenp);
static usbd_status ugen_set_interface(struct ugen_softc *, int, int);
static int ugen_get_alt_index(struct ugen_softc *sc, int ifaceidx);

#define UGENUNIT(n) ((dev2unit(n) >> 4) & 0xf)
#define UGENENDPOINT(n) (dev2unit(n) & 0xf)
#define UGENMINOR(u, e) (((u) << 4) | (e))

static device_probe_t ugen_match;
static device_attach_t ugen_attach;
static device_detach_t ugen_detach;

static devclass_t ugen_devclass;

static device_method_t ugen_methods[] = {
	DEVMETHOD(device_probe, ugen_match),
	DEVMETHOD(device_attach, ugen_attach),
	DEVMETHOD(device_detach, ugen_detach),
	{0,0},
	{0,0}
};

static driver_t ugen_driver = {
	"ugen",
        ugen_methods,
        sizeof(struct ugen_softc)
};

MODULE_DEPEND(ugen, usb, 1, 1, 1);

static int
ugen_match(device_t self)
{
        struct usb_attach_arg *uaa = device_get_ivars(self);

#if 0
	if (uaa->matchlvl)
		return (uaa->matchlvl);
#endif
	if (uaa->usegeneric)
		return (UMATCH_GENERIC);
	else
		return (UMATCH_NONE);
}

static int
ugen_attach(device_t self)
{
        struct ugen_softc *sc = device_get_softc(self);
        struct usb_attach_arg *uaa = device_get_ivars(self);
	usbd_device_handle udev;
	usbd_status err;
	int conf;

	sc->sc_dev = self;
	sc->sc_udev = udev = uaa->device;

	memset(sc->sc_endpoints, 0, sizeof sc->sc_endpoints);

	/* First set configuration index 0, the default one for ugen. */
	err = usbd_set_config_index(udev, 0, 0);
	if (err) {
		printf("%s: setting configuration index 0 failed\n",
		       device_get_nameunit(sc->sc_dev));
		sc->sc_dying = 1;
		return (ENXIO);
	}
	conf = usbd_get_config_descriptor(udev)->bConfigurationValue;

	/* Set up all the local state for this configuration. */
	err = ugen_set_config(sc, conf);
	if (err) {
		printf("%s: setting configuration %d failed\n",
		       device_get_nameunit(sc->sc_dev), conf);
		sc->sc_dying = 1;
		return (ENXIO);
	}

	/* the main device, ctrl endpoint */
	sc->dev = make_dev(&ugenctl_cdevsw,
	    UGENMINOR(device_get_unit(sc->sc_dev), 0), UID_ROOT, GID_OPERATOR, 0644,
	    "%s", device_get_nameunit(sc->sc_dev));
	ugen_make_devnodes(sc);
	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev, sc->sc_dev);
	return (0);
}

static void
ugen_make_devnodes(struct ugen_softc *sc)
{
	int endptno;
	struct cdev *dev;

	for (endptno = 1; endptno < USB_MAX_ENDPOINTS; endptno++) {
		if (sc->sc_endpoints[endptno][IN].sc != NULL ||
		    sc->sc_endpoints[endptno][OUT].sc != NULL ) {
			/* endpt can be 0x81 and 0x01, representing
			 * endpoint address 0x01 and IN/OUT directions.
			 * We map both endpts to the same device,
			 * IN is reading from it, OUT is writing to it.
			 *
			 * In the if clause above we check whether one
			 * of the structs is populated.
			 */
			dev = make_dev(&ugen_cdevsw,
				UGENMINOR(device_get_unit(sc->sc_dev), endptno),
				UID_ROOT, GID_OPERATOR, 0644,
				"%s.%d",
				device_get_nameunit(sc->sc_dev), endptno);
			dev_depends(sc->dev, dev);
			if (sc->sc_endpoints[endptno][IN].sc != NULL)
				sc->sc_endpoints[endptno][IN].dev = dev;
			if (sc->sc_endpoints[endptno][OUT].sc != NULL)
				sc->sc_endpoints[endptno][OUT].dev = dev;
		}
	}
}

static void
ugen_destroy_devnodes(struct ugen_softc *sc)
{
	int endptno, prev_sc_dying;
	struct cdev *dev;

	prev_sc_dying = sc->sc_dying;
	sc->sc_dying = 1;
	/* destroy all devices for the other (existing) endpoints as well */
	for (endptno = 1; endptno < USB_MAX_ENDPOINTS; endptno++) {
		if (sc->sc_endpoints[endptno][IN].sc != NULL ||
		    sc->sc_endpoints[endptno][OUT].sc != NULL ) {
			/* endpt can be 0x81 and 0x01, representing
			 * endpoint address 0x01 and IN/OUT directions.
			 * We map both endpoint addresses to the same device,
			 * IN is reading from it, OUT is writing to it.
			 *
			 * In the if clause above we check whether one
			 * of the structs is populated.
			 */
			if (sc->sc_endpoints[endptno][IN].sc != NULL)
				dev = sc->sc_endpoints[endptno][IN].dev;
			else
				dev = sc->sc_endpoints[endptno][OUT].dev;

			KASSERT(dev != NULL,
			    ("ugen_destroy_devnodes: NULL dev"));
			if(dev != NULL)
				destroy_dev(dev);

			sc->sc_endpoints[endptno][IN].sc = NULL;
			sc->sc_endpoints[endptno][OUT].sc = NULL;
		}
	}
	sc->sc_dying = prev_sc_dying;
}

static int
ugen_set_config(struct ugen_softc *sc, int configno)
{
	usbd_device_handle dev = sc->sc_udev;
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;
	struct ugen_endpoint *sce, **sce_cache, ***sce_cache_arr;
	u_int8_t niface, niface_cache, nendpt, *nendpt_cache;
	int ifaceno, endptno, endpt;
	usbd_status err;
	int dir;

	DPRINTFN(1,("ugen_set_config: %s to configno %d, sc=%p\n",
		    device_get_nameunit(sc->sc_dev), configno, sc));

	/* We start at 1, not 0, because we don't care whether the
	 * control endpoint is open or not. It is always present.
	 */
	for (endptno = 1; endptno < USB_MAX_ENDPOINTS; endptno++)
		if (sc->sc_is_open[endptno]) {
			DPRINTFN(1,
			     ("ugen_set_config: %s - endpoint %d is open\n",
			      device_get_nameunit(sc->sc_dev), endptno));
			return (USBD_IN_USE);
		}

	err = usbd_interface_count(dev, &niface);
	if (err)
		return (err);
	/* store an array of endpoint descriptors to clear if the configuration
	 * change succeeds - these aren't available afterwards */
	nendpt_cache = malloc(sizeof(u_int8_t) * niface, M_TEMP, M_WAITOK |
	    M_ZERO);
	sce_cache_arr = malloc(sizeof(struct ugen_endpoint **) * niface, M_TEMP,
	    M_WAITOK | M_ZERO);
	niface_cache = niface;

	for (ifaceno = 0; ifaceno < niface; ifaceno++) {
		DPRINTFN(1,("ugen_set_config: ifaceno %d\n", ifaceno));
		err = usbd_device2interface_handle(dev, ifaceno, &iface);
		if (err)
			panic("ugen_set_config: can't obtain interface handle");
		err = usbd_endpoint_count(iface, &nendpt);
		if (err)
			panic("ugen_set_config: endpoint count failed");

		/* store endpoint descriptors for each interface */
		nendpt_cache[ifaceno] = nendpt;
		sce_cache = malloc(sizeof(struct ugen_endpoint *) * nendpt, M_TEMP,
			M_WAITOK);
		sce_cache_arr[ifaceno] = sce_cache;

		for (endptno = 0; endptno < nendpt; endptno++) {
			ed = usbd_interface2endpoint_descriptor(iface,endptno);
			endpt = ed->bEndpointAddress;
			dir = UE_GET_DIR(endpt) == UE_DIR_IN ? IN : OUT;
			sce_cache[endptno] = &sc->sc_endpoints[UE_GET_ADDR(endpt)][dir];
		}
	}

	/* Avoid setting the current value. */
	if (usbd_get_config_descriptor(dev)->bConfigurationValue != configno) {
		/* attempt to perform the configuration change */
		err = usbd_set_config_no(dev, configno, 1);
		if (err) {
			for(ifaceno = 0; ifaceno < niface_cache; ifaceno++)
				free(sce_cache_arr[ifaceno], M_TEMP);
			free(sce_cache_arr, M_TEMP);
			free(nendpt_cache, M_TEMP);
			return (err);
		}
	}

	ugen_destroy_devnodes(sc);

	/* now we can clear the old interface's ugen_endpoints */
	for(ifaceno = 0; ifaceno < niface_cache; ifaceno++) {
		sce_cache = sce_cache_arr[ifaceno];
		for(endptno = 0; endptno < nendpt_cache[ifaceno]; endptno++) {
			sce = sce_cache[endptno];
			sce->sc = 0;
			sce->edesc = 0;
			sce->iface = 0;
		}
	}

	/* and free the cache storing them */
	for(ifaceno = 0; ifaceno < niface_cache; ifaceno++)
		free(sce_cache_arr[ifaceno], M_TEMP);
	free(sce_cache_arr, M_TEMP);
	free(nendpt_cache, M_TEMP);

	/* no endpoints if the device is in the unconfigured state */
	if (configno != USB_UNCONFIG_NO)
	{
		/* set the new configuration's ugen_endpoints */
		err = usbd_interface_count(dev, &niface);
		if (err)
			panic("ugen_set_config: interface count failed");

		memset(sc->sc_endpoints, 0, sizeof sc->sc_endpoints);
		for (ifaceno = 0; ifaceno < niface; ifaceno++) {
			DPRINTFN(1,("ugen_set_config: ifaceno %d\n", ifaceno));
			err = usbd_device2interface_handle(dev, ifaceno, &iface);
			if (err)
				panic("ugen_set_config: can't obtain interface handle");
			err = usbd_endpoint_count(iface, &nendpt);
			if (err)
				panic("ugen_set_config: endpoint count failed");
			for (endptno = 0; endptno < nendpt; endptno++) {
				ed = usbd_interface2endpoint_descriptor(iface,endptno);
				endpt = ed->bEndpointAddress;
				dir = UE_GET_DIR(endpt) == UE_DIR_IN ? IN : OUT;
				sce = &sc->sc_endpoints[UE_GET_ADDR(endpt)][dir];
				DPRINTFN(1,("ugen_set_config: endptno %d, endpt=0x%02x"
					    "(%d,%d), sce=%p\n",
					    endptno, endpt, UE_GET_ADDR(endpt),
					    UE_GET_DIR(endpt), sce));
				sce->sc = sc;
				sce->edesc = ed;
				sce->iface = iface;
			}
		}
	}

	return (USBD_NORMAL_COMPLETION);
}

int
ugenopen(struct cdev *dev, int flag, int mode, struct thread *p)
{
	struct ugen_softc *sc;
	int unit = UGENUNIT(dev);
	int endpt = UGENENDPOINT(dev);
	usb_endpoint_descriptor_t *edesc;
	struct ugen_endpoint *sce;
	int dir, isize;
	usbd_status err;
	usbd_xfer_handle xfer;
	void *buf;
	int i, j;

	sc = devclass_get_softc(ugen_devclass, unit);
	if (sc == NULL)
		return (ENXIO);

 	DPRINTFN(5, ("ugenopen: flag=%d, mode=%d, unit=%d endpt=%d\n",
		     flag, mode, unit, endpt));

	if (sc == NULL || sc->sc_dying)
		return (ENXIO);

	if (sc->sc_is_open[endpt])
		return (EBUSY);

	if (endpt == USB_CONTROL_ENDPOINT) {
		sc->sc_is_open[USB_CONTROL_ENDPOINT] = 1;
		UGEN_DEV_OPEN(dev, sc);
		return (0);
	}

	/* Make sure there are pipes for all directions. */
	for (dir = OUT; dir <= IN; dir++) {
		if (flag & (dir == OUT ? FWRITE : FREAD)) {
			sce = &sc->sc_endpoints[endpt][dir];
			if (sce->edesc == 0)
				return (ENXIO);
		}
	}

	/* Actually open the pipes. */
	/* XXX Should back out properly if it fails. */
	for (dir = OUT; dir <= IN; dir++) {
		if (!(flag & (dir == OUT ? FWRITE : FREAD)))
			continue;
		sce = &sc->sc_endpoints[endpt][dir];
		sce->state = 0;
		sce->timeout = USBD_NO_TIMEOUT;
		DPRINTFN(5, ("ugenopen: sc=%p, endpt=%d, dir=%d, sce=%p\n",
			     sc, endpt, dir, sce));
		edesc = sce->edesc;
		switch (edesc->bmAttributes & UE_XFERTYPE) {
		case UE_INTERRUPT:
			if (dir == OUT) {
				err = usbd_open_pipe(sce->iface,
				    edesc->bEndpointAddress, 0, &sce->pipeh);
				if (err)
					return (EIO);
				break;
			}
			isize = UGETW(edesc->wMaxPacketSize);
			if (isize == 0)	/* shouldn't happen */
				return (EINVAL);
			sce->ibuf = malloc(isize, M_USBDEV, M_WAITOK);
			DPRINTFN(5, ("ugenopen: intr endpt=%d,isize=%d\n",
				     endpt, isize));
			if ((clist_alloc_cblocks(&sce->q, UGEN_IBSIZE,
			    UGEN_IBSIZE), 0) == -1)
				return (ENOMEM);
			err = usbd_open_pipe_intr(sce->iface,
				  edesc->bEndpointAddress,
				  USBD_SHORT_XFER_OK, &sce->pipeh, sce,
				  sce->ibuf, isize, ugenintr,
				  USBD_DEFAULT_INTERVAL);
			if (err) {
				free(sce->ibuf, M_USBDEV);
				clist_free_cblocks(&sce->q);
				return (EIO);
			}
			DPRINTFN(5, ("ugenopen: interrupt open done\n"));
			break;
		case UE_BULK:
			err = usbd_open_pipe(sce->iface,
				  edesc->bEndpointAddress, 0, &sce->pipeh);
			if (err)
				return (EIO);
			break;
		case UE_ISOCHRONOUS:
			if (dir == OUT)
				return (EINVAL);
			isize = UGETW(edesc->wMaxPacketSize);
			if (isize == 0)	/* shouldn't happen */
				return (EINVAL);
			sce->ibuf = malloc(isize * UGEN_NISOFRAMES,
				M_USBDEV, M_WAITOK);
			sce->cur = sce->fill = sce->ibuf;
			sce->limit = sce->ibuf + isize * UGEN_NISOFRAMES;
			DPRINTFN(5, ("ugenopen: isoc endpt=%d, isize=%d\n",
				     endpt, isize));
			err = usbd_open_pipe(sce->iface,
				  edesc->bEndpointAddress, 0, &sce->pipeh);
			if (err) {
				free(sce->ibuf, M_USBDEV);
				return (EIO);
			}
			for(i = 0; i < UGEN_NISOREQS; ++i) {
				sce->isoreqs[i].sce = sce;
				xfer = usbd_alloc_xfer(sc->sc_udev);
				if (xfer == 0)
					goto bad;
				sce->isoreqs[i].xfer = xfer;
				buf = usbd_alloc_buffer
					(xfer, isize * UGEN_NISORFRMS);
				if (buf == 0) {
					i++;
					goto bad;
				}
				sce->isoreqs[i].dmabuf = buf;
				for(j = 0; j < UGEN_NISORFRMS; ++j)
					sce->isoreqs[i].sizes[j] = isize;
				usbd_setup_isoc_xfer
					(xfer, sce->pipeh, &sce->isoreqs[i],
					 sce->isoreqs[i].sizes,
					 UGEN_NISORFRMS, USBD_NO_COPY,
					 ugen_isoc_rintr);
				(void)usbd_transfer(xfer);
			}
			DPRINTFN(5, ("ugenopen: isoc open done\n"));
			break;
		bad:
			while (--i >= 0) /* implicit buffer free */
				usbd_free_xfer(sce->isoreqs[i].xfer);
			return (ENOMEM);
		case UE_CONTROL:
			sce->timeout = USBD_DEFAULT_TIMEOUT;
			return (EINVAL);
		}
	}
	sc->sc_is_open[endpt] = 1;
	UGEN_DEV_OPEN(dev, sc);
	return (0);
}

int
ugenclose(struct cdev *dev, int flag, int mode, struct thread *p)
{
	int endpt = UGENENDPOINT(dev);
	struct ugen_softc *sc;
	struct ugen_endpoint *sce;
	int dir;
	int i;

	sc = devclass_get_softc(ugen_devclass, UGENUNIT(dev));

	DPRINTFN(5, ("ugenclose: flag=%d, mode=%d, unit=%d, endpt=%d\n",
		     flag, mode, UGENUNIT(dev), endpt));

#ifdef DIAGNOSTIC
	if (!sc->sc_is_open[endpt]) {
		printf("ugenclose: not open\n");
		return (EINVAL);
	}
#endif

	if (endpt == USB_CONTROL_ENDPOINT) {
		DPRINTFN(5, ("ugenclose: close control\n"));
		sc->sc_is_open[endpt] = 0;
		UGEN_DEV_CLOSE(dev, sc);
		return (0);
	}

	for (dir = OUT; dir <= IN; dir++) {
		if (!(flag & (dir == OUT ? FWRITE : FREAD)))
			continue;
		sce = &sc->sc_endpoints[endpt][dir];
		if (sce->pipeh == NULL)
			continue;
		DPRINTFN(5, ("ugenclose: endpt=%d dir=%d sce=%p\n",
			     endpt, dir, sce));

		usbd_abort_pipe(sce->pipeh);
		usbd_close_pipe(sce->pipeh);
		sce->pipeh = NULL;

		switch (sce->edesc->bmAttributes & UE_XFERTYPE) {
		case UE_INTERRUPT:
			ndflush(&sce->q, sce->q.c_cc);
			clist_free_cblocks(&sce->q);
			break;
		case UE_ISOCHRONOUS:
			for (i = 0; i < UGEN_NISOREQS; ++i)
				usbd_free_xfer(sce->isoreqs[i].xfer);

		default:
			break;
		}

		if (sce->ibuf != NULL) {
			free(sce->ibuf, M_USBDEV);
			sce->ibuf = NULL;
			clist_free_cblocks(&sce->q);
		}
	}
	sc->sc_is_open[endpt] = 0;
	UGEN_DEV_CLOSE(dev, sc);

	return (0);
}

static int
ugen_do_read(struct ugen_softc *sc, int endpt, struct uio *uio, int flag)
{
	struct ugen_endpoint *sce = &sc->sc_endpoints[endpt][IN];
	u_int32_t n, tn;
	char buf[UGEN_BBSIZE];
	usbd_xfer_handle xfer;
	usbd_status err;
	int s;
	int error = 0, doneone = 0;
	u_char buffer[UGEN_CHUNK];

	DPRINTFN(5, ("%s: ugenread: %d\n", device_get_nameunit(sc->sc_dev), endpt));

	if (sc->sc_dying)
		return (EIO);

	if (endpt == USB_CONTROL_ENDPOINT)
		return (ENODEV);

#ifdef DIAGNOSTIC
	if (sce->edesc == NULL) {
		printf("ugenread: no edesc\n");
		return (EIO);
	}
	if (sce->pipeh == NULL) {
		printf("ugenread: no pipe\n");
		return (EIO);
	}
#endif

	switch (sce->edesc->bmAttributes & UE_XFERTYPE) {
	case UE_INTERRUPT:
		/* Block until activity occurred. */
		s = splusb();
		while (sce->q.c_cc == 0) {
			if (flag & O_NONBLOCK) {
				splx(s);
				return (EWOULDBLOCK);
			}
			sce->state |= UGEN_ASLP;
			DPRINTFN(5, ("ugenread: sleep on %p\n", sce));
			error = tsleep(sce, PZERO | PCATCH, "ugenri",
			    (sce->timeout * hz + 999) / 1000);
			sce->state &= ~UGEN_ASLP;
			DPRINTFN(5, ("ugenread: woke, error=%d\n", error));
			if (sc->sc_dying)
				error = EIO;
			if (error == EAGAIN) {
				error = 0;	/* timeout, return 0 bytes */
				break;
			}
			if (error)
				break;
		}
		splx(s);

		/* Transfer as many chunks as possible. */
		while (sce->q.c_cc > 0 && uio->uio_resid > 0 && !error) {
			n = min(sce->q.c_cc, uio->uio_resid);
			if (n > sizeof(buffer))
				n = sizeof(buffer);

			/* Remove a small chunk from the input queue. */
			q_to_b(&sce->q, buffer, n);
			DPRINTFN(5, ("ugenread: got %d chars\n", n));

			/* Copy the data to the user process. */
			error = uiomove(buffer, n, uio);
			if (error)
				break;
		}
		break;
	case UE_BULK:
		xfer = usbd_alloc_xfer(sc->sc_udev);
		if (xfer == 0)
			return (ENOMEM);
		while ((n = min(UGEN_BBSIZE, uio->uio_resid)) != 0 ||
		    !doneone) {
			DPRINTFN(1, ("ugenread: start transfer %d bytes\n",n));
			tn = n;
			doneone = 1;
			err = usbd_bulk_transfer(
				  xfer, sce->pipeh,
				  sce->state & UGEN_SHORT_OK ?
				      USBD_SHORT_XFER_OK : 0,
				  sce->timeout, buf, &tn, "ugenrb");
			if (err) {
				if (err == USBD_INTERRUPTED)
					error = EINTR;
				else if (err == USBD_TIMEOUT)
					error = ETIMEDOUT;
				else
					error = EIO;
				break;
			}
			DPRINTFN(1, ("ugenread: got %d bytes\n", tn));
			error = uiomove(buf, tn, uio);
			if (error || tn < n)
				break;
		}
		usbd_free_xfer(xfer);
		break;
	case UE_ISOCHRONOUS:
		s = splusb();
		while (sce->cur == sce->fill) {
			if (flag & O_NONBLOCK) {
				splx(s);
				return (EWOULDBLOCK);
			}
			sce->state |= UGEN_ASLP;
			DPRINTFN(5, ("ugenread: sleep on %p\n", sce));
			error = tsleep(sce, PZERO | PCATCH, "ugenri",
			    (sce->timeout * hz + 999) / 1000);
			sce->state &= ~UGEN_ASLP;
			DPRINTFN(5, ("ugenread: woke, error=%d\n", error));
			if (sc->sc_dying)
				error = EIO;
			if (error == EAGAIN) {
				error = 0;	/* timeout, return 0 bytes */
				break;
			}
			if (error)
				break;
		}

		while (sce->cur != sce->fill && uio->uio_resid > 0 && !error) {
			if (sce->fill > sce->cur)
				n = min(sce->fill - sce->cur, uio->uio_resid);
			else
				n = min(sce->limit - sce->cur, uio->uio_resid);

			DPRINTFN(5, ("ugenread: isoc got %d chars\n", n));

			/* Copy the data to the user process. */
			error = uiomove(sce->cur, n, uio);
			if (error)
				break;
			sce->cur += n;
			if(sce->cur >= sce->limit)
				sce->cur = sce->ibuf;
		}
		splx(s);
		break;


	default:
		return (ENXIO);
	}
	return (error);
}

int
ugenread(struct cdev *dev, struct uio *uio, int flag)
{
	int endpt = UGENENDPOINT(dev);
	struct ugen_softc *sc;
	int error;

	sc = devclass_get_softc(ugen_devclass, UGENUNIT(dev));

	if (sc->sc_dying)
		return (EIO);

	UGEN_DEV_REF(dev, sc);
	error = ugen_do_read(sc, endpt, uio, flag);
	UGEN_DEV_RELE(dev, sc);
	return (error);
}

static int
ugen_do_write(struct ugen_softc *sc, int endpt, struct uio *uio, int flag)
{
	struct ugen_endpoint *sce = &sc->sc_endpoints[endpt][OUT];
	u_int32_t n;
	int error = 0, doneone = 0;
	char buf[UGEN_BBSIZE];
	usbd_xfer_handle xfer;
	usbd_status err;

	DPRINTFN(5, ("%s: ugenwrite: %d\n", device_get_nameunit(sc->sc_dev), endpt));

	if (sc->sc_dying)
		return (EIO);

	if (endpt == USB_CONTROL_ENDPOINT)
		return (ENODEV);

#ifdef DIAGNOSTIC
	if (sce->edesc == NULL) {
		printf("ugenwrite: no edesc\n");
		return (EIO);
	}
	if (sce->pipeh == NULL) {
		printf("ugenwrite: no pipe\n");
		return (EIO);
	}
#endif

	switch (sce->edesc->bmAttributes & UE_XFERTYPE) {
	case UE_BULK:
		xfer = usbd_alloc_xfer(sc->sc_udev);
		if (xfer == 0)
			return (EIO);
		while ((n = min(UGEN_BBSIZE, uio->uio_resid)) != 0 ||
		    !doneone) {
			doneone = 1;
			error = uiomove(buf, n, uio);
			if (error)
				break;
			DPRINTFN(1, ("ugenwrite: transfer %d bytes\n", n));
			err = usbd_bulk_transfer(xfer, sce->pipeh, 0,
				  sce->timeout, buf, &n,"ugenwb");
			if (err) {
				if (err == USBD_INTERRUPTED)
					error = EINTR;
				else if (err == USBD_TIMEOUT)
					error = ETIMEDOUT;
				else
					error = EIO;
				break;
			}
		}
		usbd_free_xfer(xfer);
		break;
	case UE_INTERRUPT:
		xfer = usbd_alloc_xfer(sc->sc_udev);
		if (xfer == 0)
			return (EIO);
		while ((n = min(UGETW(sce->edesc->wMaxPacketSize),
		    uio->uio_resid)) != 0 || !doneone) {
			doneone = 1;
			error = uiomove(buf, n, uio);
			if (error)
				break;
			DPRINTFN(1, ("ugenwrite: transfer %d bytes\n", n));
			err = usbd_intr_transfer(xfer, sce->pipeh, 0,
			    sce->timeout, buf, &n, "ugenwi");
			if (err) {
				if (err == USBD_INTERRUPTED)
					error = EINTR;
				else if (err == USBD_TIMEOUT)
					error = ETIMEDOUT;
				else
					error = EIO;
				break;
			}
		}
		usbd_free_xfer(xfer);
		break;
	default:
		return (ENXIO);
	}
	return (error);
}

int
ugenwrite(struct cdev *dev, struct uio *uio, int flag)
{
	int endpt = UGENENDPOINT(dev);
	struct ugen_softc *sc;
	int error;

	sc = devclass_get_softc(ugen_devclass, UGENUNIT(dev));

	if (sc->sc_dying)
		return (EIO);

	UGEN_DEV_REF(dev, sc);
	error = ugen_do_write(sc, endpt, uio, flag);
	UGEN_DEV_RELE(dev, sc);
	return (error);
}

void
ugenpurge(struct cdev *dev)
{
	int endpt = UGENENDPOINT(dev);
	struct ugen_endpoint *sce;
	struct ugen_softc *sc;

	if (endpt == USB_CONTROL_ENDPOINT)
		return;
	sc = devclass_get_softc(ugen_devclass, UGENUNIT(dev));
	sce = &sc->sc_endpoints[endpt][IN];
	if (sce->pipeh)
		usbd_abort_pipe(sce->pipeh);
	if (sce->state & UGEN_ASLP) {
		DPRINTFN(5, ("ugenpurge: waking %p\n", sce));
		wakeup(sce);
	}
	selwakeuppri(&sce->rsel, PZERO);

	sce = &sc->sc_endpoints[endpt][OUT];
	if (sce->pipeh)
		usbd_abort_pipe(sce->pipeh);
	if (sce->state & UGEN_ASLP) {
		DPRINTFN(5, ("ugenpurge: waking %p\n", sce));
		wakeup(sce);
	}
	selwakeuppri(&sce->rsel, PZERO);
}

static int
ugen_detach(device_t self)
{
	struct ugen_softc *sc = device_get_softc(self);
	struct ugen_endpoint *sce;
	int i, dir;

	DPRINTF(("ugen_detach: sc=%p\n", sc));

	sc->sc_dying = 1;
	/* Abort all pipes.  Causes processes waiting for transfer to wake. */
	for (i = 0; i < USB_MAX_ENDPOINTS; i++) {
		for (dir = OUT; dir <= IN; dir++) {
			sce = &sc->sc_endpoints[i][dir];
			if (sce->pipeh)
				usbd_abort_pipe(sce->pipeh);
			selwakeuppri(&sce->rsel, PZERO);
		}
	}

	/* destroy the device for the control endpoint */
	destroy_dev(sc->dev);
	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev, sc->sc_dev);
	return (0);
}

static void
ugenintr(usbd_xfer_handle xfer, usbd_private_handle addr, usbd_status status)
{
	struct ugen_endpoint *sce = addr;
	/*struct ugen_softc *sc = sce->sc;*/
	u_int32_t count;
	u_char *ibuf;

	if (status == USBD_CANCELLED)
		return;

	if (status != USBD_NORMAL_COMPLETION) {
		DPRINTF(("ugenintr: status=%d\n", status));
		if (status == USBD_STALLED)
		    usbd_clear_endpoint_stall_async(sce->pipeh);
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	ibuf = sce->ibuf;

	DPRINTFN(5, ("ugenintr: xfer=%p status=%d count=%d\n",
		     xfer, status, count));
	DPRINTFN(5, ("          data = %02x %02x %02x\n",
		     ibuf[0], ibuf[1], ibuf[2]));

	(void)b_to_q(ibuf, count, &sce->q);

	if (sce->state & UGEN_ASLP) {
		sce->state &= ~UGEN_ASLP;
		DPRINTFN(5, ("ugen_intr: waking %p\n", sce));
		wakeup(sce);
	}
	selwakeuppri(&sce->rsel, PZERO);
}

static void
ugen_isoc_rintr(usbd_xfer_handle xfer, usbd_private_handle addr,
		usbd_status status)
{
	struct isoreq *req = addr;
	struct ugen_endpoint *sce = req->sce;
	u_int32_t count, n;
	int i, isize;

	/* Return if we are aborting. */
	if (status == USBD_CANCELLED)
		return;

	usbd_get_xfer_status(xfer, NULL, NULL, &count, NULL);
	DPRINTFN(5,("ugen_isoc_rintr: xfer %td, count=%d\n", req - sce->isoreqs,
		    count));

	/* throw away oldest input if the buffer is full */
	if(sce->fill < sce->cur && sce->cur <= sce->fill + count) {
		sce->cur += count;
		if(sce->cur >= sce->limit)
			sce->cur = sce->ibuf + (sce->limit - sce->cur);
		DPRINTF(("ugen_isoc_rintr: throwing away %d bytes\n",
			 count));
	}

	isize = UGETW(sce->edesc->wMaxPacketSize);
	for (i = 0; i < UGEN_NISORFRMS; i++) {
		u_int32_t actlen = req->sizes[i];
		char const *buf = (char const *)req->dmabuf + isize * i;

		/* copy data to buffer */
		while (actlen > 0) {
			n = min(actlen, sce->limit - sce->fill);
			memcpy(sce->fill, buf, n);

			buf += n;
			actlen -= n;
			sce->fill += n;
			if(sce->fill == sce->limit)
				sce->fill = sce->ibuf;
		}

		/* setup size for next transfer */
		req->sizes[i] = isize;
	}

	usbd_setup_isoc_xfer(xfer, sce->pipeh, req, req->sizes, UGEN_NISORFRMS,
			     USBD_NO_COPY, ugen_isoc_rintr);
	(void)usbd_transfer(xfer);

	if (sce->state & UGEN_ASLP) {
		sce->state &= ~UGEN_ASLP;
		DPRINTFN(5, ("ugen_isoc_rintr: waking %p\n", sce));
		wakeup(sce);
	}
	selwakeuppri(&sce->rsel, PZERO);
}

static usbd_status
ugen_set_interface(struct ugen_softc *sc, int ifaceidx, int altno)
{
	usbd_interface_handle iface;
	usb_endpoint_descriptor_t *ed;
	usbd_status err;
	struct ugen_endpoint *sce, **sce_cache;
	u_int8_t niface, nendpt, nendpt_cache, endptno, endpt;
	int dir;

	DPRINTFN(15, ("ugen_set_interface %d %d\n", ifaceidx, altno));

	err = usbd_interface_count(sc->sc_udev, &niface);
	if (err)
		return (err);
	if (ifaceidx < 0 || ifaceidx >= niface)
		return (USBD_INVAL);

	err = usbd_device2interface_handle(sc->sc_udev, ifaceidx, &iface);
	if (err)
		return (err);
	err = usbd_endpoint_count(iface, &nendpt);
	if (err)
		return (err);

	/* store an array of endpoint descriptors to clear if the interface
	 * change succeeds - these aren't available afterwards */
	sce_cache = malloc(sizeof(struct ugen_endpoint *) * nendpt, M_TEMP,
		M_WAITOK);
	nendpt_cache = nendpt;

	for (endptno = 0; endptno < nendpt; endptno++) {
		ed = usbd_interface2endpoint_descriptor(iface,endptno);
		endpt = ed->bEndpointAddress;
		dir = UE_GET_DIR(endpt) == UE_DIR_IN ? IN : OUT;
		sce_cache[endptno] = &sc->sc_endpoints[UE_GET_ADDR(endpt)][dir];
	}

	/* change setting */
	err = usbd_set_interface(iface, altno);
	if (err) {
		free(sce_cache, M_TEMP);
		return (err);
	}
	err = usbd_endpoint_count(iface, &nendpt);
	if (err)
		panic("ugen_set_interface: endpoint count failed");

	/* destroy the existing devices, we remake the new ones in a moment */
	ugen_destroy_devnodes(sc);

	/* now we can clear the old interface's ugen_endpoints */
	for (endptno = 0; endptno < nendpt_cache; endptno++) {
		sce = sce_cache[endptno];
		sce->sc = 0;
		sce->edesc = 0;
		sce->iface = 0;
	}
	free(sce_cache, M_TEMP);

	/* set the new interface's ugen_endpoints */
	for (endptno = 0; endptno < nendpt; endptno++) {
		ed = usbd_interface2endpoint_descriptor(iface,endptno);
		endpt = ed->bEndpointAddress;
		dir = UE_GET_DIR(endpt) == UE_DIR_IN ? IN : OUT;
		sce = &sc->sc_endpoints[UE_GET_ADDR(endpt)][dir];
		sce->sc = sc;
		sce->edesc = ed;
		sce->iface = iface;
	}

	/* make the new devices */
	ugen_make_devnodes(sc);

	return (0);
}

/* Retrieve a complete descriptor for a certain device and index. */
static usb_config_descriptor_t *
ugen_get_cdesc(struct ugen_softc *sc, int index, int *lenp)
{
	usb_config_descriptor_t *cdesc, *tdesc, cdescr;
	int len;
	usbd_status err;

	if (index == USB_CURRENT_CONFIG_INDEX) {
		tdesc = usbd_get_config_descriptor(sc->sc_udev);
		len = UGETW(tdesc->wTotalLength);
		if (lenp)
			*lenp = len;
		cdesc = malloc(len, M_TEMP, M_WAITOK);
		memcpy(cdesc, tdesc, len);
		DPRINTFN(5,("ugen_get_cdesc: current, len=%d\n", len));
	} else {
		err = usbd_get_config_desc(sc->sc_udev, index, &cdescr);
		if (err)
			return (0);
		len = UGETW(cdescr.wTotalLength);
		DPRINTFN(5,("ugen_get_cdesc: index=%d, len=%d\n", index, len));
		if (lenp)
			*lenp = len;
		cdesc = malloc(len, M_TEMP, M_WAITOK);
		err = usbd_get_config_desc_full(sc->sc_udev, index, cdesc, len);
		if (err) {
			free(cdesc, M_TEMP);
			return (0);
		}
	}
	return (cdesc);
}

static int
ugen_get_alt_index(struct ugen_softc *sc, int ifaceidx)
{
	usbd_interface_handle iface;
	usbd_status err;

	err = usbd_device2interface_handle(sc->sc_udev, ifaceidx, &iface);
	if (err)
		return (-1);
	return (usbd_get_interface_altindex(iface));
}

static int
ugen_do_ioctl(struct ugen_softc *sc, int endpt, u_long cmd,
	      caddr_t addr, int flag, struct thread *p)
{
	struct ugen_endpoint *sce;
	usbd_status err;
	usbd_interface_handle iface;
	struct usb_config_desc *cd;
	usb_config_descriptor_t *cdesc;
	struct usb_interface_desc *id;
	usb_interface_descriptor_t *idesc;
	struct usb_endpoint_desc *ed;
	usb_endpoint_descriptor_t *edesc;
	struct usb_alt_interface *ai;
	struct usb_string_desc *si;
	u_int8_t conf, alt;

	DPRINTFN(5, ("ugenioctl: cmd=%08lx\n", cmd));
	if (sc->sc_dying)
		return (EIO);

	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		/* All handled in the upper FS layer. */
		return (0);
	case USB_SET_SHORT_XFER:
		if (endpt == USB_CONTROL_ENDPOINT)
			return (EINVAL);
		/* This flag only affects read */
		sce = &sc->sc_endpoints[endpt][IN];

		if (sce->pipeh == NULL) {
			printf("ugenioctl: USB_SET_SHORT_XFER, no pipe\n");
			return (EIO);
		}

		if (*(int *)addr)
			sce->state |= UGEN_SHORT_OK;
		else
			sce->state &= ~UGEN_SHORT_OK;
		return (0);
	case USB_SET_TIMEOUT:
		sce = &sc->sc_endpoints[endpt][IN];
		sce->timeout = *(int *)addr;
		sce = &sc->sc_endpoints[endpt][OUT];
		sce->timeout = *(int *)addr;
		return (0);
	default:
		break;
	}

	if (endpt != USB_CONTROL_ENDPOINT)
		return (EINVAL);

	switch (cmd) {
#ifdef USB_DEBUG
	case USB_SETDEBUG:
		ugendebug = *(int *)addr;
		break;
#endif
	case USB_GET_CONFIG:
		err = usbd_get_config(sc->sc_udev, &conf);
		if (err)
			return (EIO);
		*(int *)addr = conf;
		break;
	case USB_SET_CONFIG:
		if (!(flag & FWRITE))
			return (EPERM);
		err = ugen_set_config(sc, *(int *)addr);
		switch (err) {
		case USBD_NORMAL_COMPLETION:
			ugen_make_devnodes(sc);
			break;
		case USBD_IN_USE:
			return (EBUSY);
		default:
			return (EIO);
		}
		break;
	case USB_GET_ALTINTERFACE:
		ai = (struct usb_alt_interface *)addr;
		err = usbd_device2interface_handle(sc->sc_udev,
			  ai->uai_interface_index, &iface);
		if (err)
			return (EINVAL);
		idesc = usbd_get_interface_descriptor(iface);
		if (idesc == NULL)
			return (EIO);
		ai->uai_alt_no = idesc->bAlternateSetting;
		break;
	case USB_SET_ALTINTERFACE:
		if (!(flag & FWRITE))
			return (EPERM);
		ai = (struct usb_alt_interface *)addr;
		err = usbd_device2interface_handle(sc->sc_udev,
			  ai->uai_interface_index, &iface);
		if (err)
			return (EINVAL);
		err = ugen_set_interface(sc, ai->uai_interface_index,
		    ai->uai_alt_no);
		if (err)
			return (EINVAL);
		break;
	case USB_GET_NO_ALT:
		ai = (struct usb_alt_interface *)addr;
		cdesc = ugen_get_cdesc(sc, ai->uai_config_index, 0);
		if (cdesc == NULL)
			return (EINVAL);
		idesc = usbd_find_idesc(cdesc, ai->uai_interface_index, 0);
		if (idesc == NULL) {
			free(cdesc, M_TEMP);
			return (EINVAL);
		}
		ai->uai_alt_no = usbd_get_no_alts(cdesc,
		    idesc->bInterfaceNumber);
		free(cdesc, M_TEMP);
		break;
	case USB_GET_DEVICE_DESC:
		*(usb_device_descriptor_t *)addr =
			*usbd_get_device_descriptor(sc->sc_udev);
		break;
	case USB_GET_CONFIG_DESC:
		cd = (struct usb_config_desc *)addr;
		cdesc = ugen_get_cdesc(sc, cd->ucd_config_index, 0);
		if (cdesc == NULL)
			return (EINVAL);
		cd->ucd_desc = *cdesc;
		free(cdesc, M_TEMP);
		break;
	case USB_GET_INTERFACE_DESC:
		id = (struct usb_interface_desc *)addr;
		cdesc = ugen_get_cdesc(sc, id->uid_config_index, 0);
		if (cdesc == NULL)
			return (EINVAL);
		if (id->uid_config_index == USB_CURRENT_CONFIG_INDEX &&
		    id->uid_alt_index == USB_CURRENT_ALT_INDEX)
			alt = ugen_get_alt_index(sc, id->uid_interface_index);
		else
			alt = id->uid_alt_index;
		idesc = usbd_find_idesc(cdesc, id->uid_interface_index, alt);
		if (idesc == NULL) {
			free(cdesc, M_TEMP);
			return (EINVAL);
		}
		id->uid_desc = *idesc;
		free(cdesc, M_TEMP);
		break;
	case USB_GET_ENDPOINT_DESC:
		ed = (struct usb_endpoint_desc *)addr;
		cdesc = ugen_get_cdesc(sc, ed->ued_config_index, 0);
		if (cdesc == NULL)
			return (EINVAL);
		if (ed->ued_config_index == USB_CURRENT_CONFIG_INDEX &&
		    ed->ued_alt_index == USB_CURRENT_ALT_INDEX)
			alt = ugen_get_alt_index(sc, ed->ued_interface_index);
		else
			alt = ed->ued_alt_index;
		edesc = usbd_find_edesc(cdesc, ed->ued_interface_index,
					alt, ed->ued_endpoint_index);
		if (edesc == NULL) {
			free(cdesc, M_TEMP);
			return (EINVAL);
		}
		ed->ued_desc = *edesc;
		free(cdesc, M_TEMP);
		break;
	case USB_GET_FULL_DESC:
	{
		int len;
		struct iovec iov;
		struct uio uio;
		struct usb_full_desc *fd = (struct usb_full_desc *)addr;
		int error;

		cdesc = ugen_get_cdesc(sc, fd->ufd_config_index, &len);
		if (len > fd->ufd_size)
			len = fd->ufd_size;
		iov.iov_base = (caddr_t)fd->ufd_data;
		iov.iov_len = len;
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_resid = len;
		uio.uio_offset = 0;
		uio.uio_segflg = UIO_USERSPACE;
		uio.uio_rw = UIO_READ;
		uio.uio_td = p;
		error = uiomove((void *)cdesc, len, &uio);
		free(cdesc, M_TEMP);
		return (error);
	}
	case USB_GET_STRING_DESC: {
		int len;
		si = (struct usb_string_desc *)addr;
		err = usbd_get_string_desc(sc->sc_udev, si->usd_string_index,
			  si->usd_language_id, &si->usd_desc, &len);
		if (err)
			return (EINVAL);
		break;
	}
	case USB_DO_REQUEST:
	{
		struct usb_ctl_request *ur = (void *)addr;
		int len = UGETW(ur->ucr_request.wLength);
		struct iovec iov;
		struct uio uio;
		void *ptr = 0;
		int error = 0;

		if (!(flag & FWRITE))
			return (EPERM);
		/* Avoid requests that would damage the bus integrity. */
		if ((ur->ucr_request.bmRequestType == UT_WRITE_DEVICE &&
		     ur->ucr_request.bRequest == UR_SET_ADDRESS) ||
		    (ur->ucr_request.bmRequestType == UT_WRITE_DEVICE &&
		     ur->ucr_request.bRequest == UR_SET_CONFIG) ||
		    (ur->ucr_request.bmRequestType == UT_WRITE_INTERFACE &&
		     ur->ucr_request.bRequest == UR_SET_INTERFACE))
			return (EINVAL);

		if (len < 0 || len > 32767)
			return (EINVAL);
		if (len != 0) {
			iov.iov_base = (caddr_t)ur->ucr_data;
			iov.iov_len = len;
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_resid = len;
			uio.uio_offset = 0;
			uio.uio_segflg = UIO_USERSPACE;
			uio.uio_rw =
				ur->ucr_request.bmRequestType & UT_READ ?
				UIO_READ : UIO_WRITE;
			uio.uio_td = p;
			ptr = malloc(len, M_TEMP, M_WAITOK);
			if (uio.uio_rw == UIO_WRITE) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
		sce = &sc->sc_endpoints[endpt][IN];
		err = usbd_do_request_flags(sc->sc_udev, &ur->ucr_request,
			  ptr, ur->ucr_flags, &ur->ucr_actlen, sce->timeout);
		if (err) {
			error = EIO;
			goto ret;
		}
		if (len != 0) {
			if (uio.uio_rw == UIO_READ) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
	ret:
		if (ptr)
			free(ptr, M_TEMP);
		return (error);
	}
	case USB_GET_DEVICEINFO:
		usbd_fill_deviceinfo(sc->sc_udev,
		    (struct usb_device_info *)addr, 1);
		break;
	case USB_RESET_DEVICE:
		err = usbd_reset_device(sc->sc_udev);
		if (err)
			return EIO;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

int
ugenioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *p)
{
	int endpt = UGENENDPOINT(dev);
	struct ugen_softc *sc;
	int error;

	sc = devclass_get_softc(ugen_devclass, UGENUNIT(dev));

	if (sc->sc_dying)
		return (EIO);

	UGEN_DEV_REF(dev, sc);
	error = ugen_do_ioctl(sc, endpt, cmd, addr, flag, p);
	UGEN_DEV_RELE(dev, sc);
	return (error);
}

int
ugenpoll(struct cdev *dev, int events, struct thread *p)
{
	struct ugen_softc *sc;
	struct ugen_endpoint *sce_in, *sce_out;
	usb_endpoint_descriptor_t *edesc;
	int revents = 0;
	int s;

	sc = devclass_get_softc(ugen_devclass, UGENUNIT(dev));

	if (sc->sc_dying)
		return ((events & (POLLIN | POLLOUT | POLLRDNORM |
		    POLLWRNORM)) | POLLHUP);
	/* Do not allow to poll a control endpoint */
	if (UGENENDPOINT(dev) == USB_CONTROL_ENDPOINT)
		return (events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));

	sce_in = &sc->sc_endpoints[UGENENDPOINT(dev)][IN];
	sce_out = &sc->sc_endpoints[UGENENDPOINT(dev)][OUT];
	edesc = (sce_in->edesc != NULL) ? sce_in->edesc : sce_out->edesc;
	KASSERT(edesc != NULL, ("ugenpoll: NULL edesc"));
	if (sce_in->edesc == NULL || sce_in->pipeh == NULL)
		sce_in = NULL;
	if (sce_out->edesc == NULL || sce_out->pipeh == NULL)
		sce_out = NULL;

	s = splusb();
	switch (edesc->bmAttributes & UE_XFERTYPE) {
	case UE_INTERRUPT:
		if (sce_in != NULL && (events & (POLLIN | POLLRDNORM))) {
			if (sce_in->q.c_cc > 0)
				revents |= events & (POLLIN | POLLRDNORM);
			else
				selrecord(p, &sce_in->rsel);
		}
		if (sce_out != NULL && (events & (POLLOUT | POLLWRNORM))) {
			if (sce_out->q.c_cc > 0)
				revents |= events & (POLLOUT | POLLWRNORM);
			else
				selrecord(p, &sce_out->rsel);
		}
		break;
	case UE_ISOCHRONOUS:
		if (sce_in != NULL && (events & (POLLIN | POLLRDNORM))) {
			if (sce_in->cur != sce_in->fill)
				revents |= events & (POLLIN | POLLRDNORM);
			else
				selrecord(p, &sce_in->rsel);
		}
		if (sce_out != NULL && (events & (POLLOUT | POLLWRNORM))) {
			if (sce_out->cur != sce_out->fill)
				revents |= events & (POLLOUT | POLLWRNORM);
			else
				selrecord(p, &sce_out->rsel);
		}
		break;
	case UE_BULK:
		/*
		 * We have no easy way of determining if a read will
		 * yield any data or a write will happen.
		 * Pretend they will.
		 */
		revents |= events &
			   (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);
		break;
	default:
		break;
	}
	splx(s);
	return (revents);
}

DRIVER_MODULE(ugen, uhub, ugen_driver, ugen_devclass, usbd_driver_load, 0);
