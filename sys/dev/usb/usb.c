/*	$NetBSD: usb.c,v 1.68 2002/02/20 20:30:12 christos Exp $	*/
/*	$FreeBSD$	*/

/* Also already merged from NetBSD:
 *	$NetBSD: usb.c,v 1.70 2002/05/09 21:54:32 augustss Exp $
 *	$NetBSD: usb.c,v 1.73 2002/09/23 05:51:19 simonb Exp $
 */

/*
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

/*
 * USB specifications and other documentation can be found at
 * http://www.usb.org/developers/data/ and
 * http://www.usb.org/developers/index.html .
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/unistd.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/filio.h>
#include <sys/uio.h>
#endif
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/poll.h>
#if __FreeBSD_version >= 500014
#include <sys/selinfo.h>
#else
#include <sys/select.h>
#endif
#include <sys/vnode.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#define USBUNIT(d)	(minor(d))	/* usb_discover device nodes, kthread */
#define USB_DEV_MINOR	255		/* event queue device */

#if defined(__FreeBSD__)
MALLOC_DEFINE(M_USB, "USB", "USB");
MALLOC_DEFINE(M_USBDEV, "USBdev", "USB device");
MALLOC_DEFINE(M_USBHC, "USBHC", "USB host controller");

#include "usb_if.h"
#endif /* defined(__FreeBSD__) */

#include <machine/bus.h>

#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_quirks.h>

/* Define this unconditionally in case a kernel module is loaded that
 * has been compiled with debugging options.
 */
SYSCTL_NODE(_hw, OID_AUTO, usb, CTLFLAG_RW, 0, "USB debugging");

#ifdef USB_DEBUG
#define DPRINTF(x)	if (usbdebug) logprintf x
#define DPRINTFN(n,x)	if (usbdebug>(n)) logprintf x
int	usbdebug = 0;
SYSCTL_INT(_hw_usb, OID_AUTO, debug, CTLFLAG_RW,
	   &usbdebug, 0, "usb debug level");
/*
 * 0  - do usual exploration
 * 1  - do not use timeout exploration
 * >1 - do no exploration
 */
int	usb_noexplore = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct usb_softc {
	USBBASEDEVICE	sc_dev;		/* base device */
	usbd_bus_handle sc_bus;		/* USB controller */
	struct usbd_port sc_port;	/* dummy port for root hub */

	struct proc	*sc_event_thread;

	char		sc_dying;
};

TAILQ_HEAD(, usb_task) usb_all_tasks;

#if defined(__NetBSD__) || defined(__OpenBSD__)
cdev_decl(usb);
#elif defined(__FreeBSD__)
d_open_t  usbopen;
d_close_t usbclose;
d_read_t usbread;
d_ioctl_t usbioctl;
d_poll_t usbpoll;

struct cdevsw usb_cdevsw = {
	.d_open =	usbopen,
	.d_close =	usbclose,
	.d_read =	usbread,
	.d_ioctl =	usbioctl,
	.d_poll =	usbpoll,
	.d_name =	"usb",
	.d_maj =	USB_CDEV_MAJOR,
#if __FreeBSD_version < 500014
	/* bmaj */      -1
#endif
};
#endif

Static void	usb_discover(void *);
Static void	usb_create_event_thread(void *);
Static void	usb_event_thread(void *);
Static void	usb_task_thread(void *);
Static struct proc *usb_task_thread_proc = NULL;

#define USB_MAX_EVENTS 100
struct usb_event_q {
	struct usb_event ue;
	TAILQ_ENTRY(usb_event_q) next;
};
Static TAILQ_HEAD(, usb_event_q) usb_events =
	TAILQ_HEAD_INITIALIZER(usb_events);
Static int usb_nevents = 0;
Static struct selinfo usb_selevent;
Static struct proc *usb_async_proc;  /* process that wants USB SIGIO */
Static int usb_dev_open = 0;
Static void usb_add_event(int, struct usb_event *);

Static int usb_get_next_event(struct usb_event *);

Static const char *usbrev_str[] = USBREV_STR;

USB_DECLARE_DRIVER_INIT(usb,
			DEVMETHOD(device_suspend, bus_generic_suspend),
			DEVMETHOD(device_resume, bus_generic_resume),
			DEVMETHOD(device_shutdown, bus_generic_shutdown)
			);

#if defined(__FreeBSD__)
MODULE_VERSION(usb, 1);
#endif

USB_MATCH(usb)
{
	DPRINTF(("usbd_match\n"));
	return (UMATCH_GENERIC);
}

USB_ATTACH(usb)
{
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct usb_softc *sc = (struct usb_softc *)self;
#elif defined(__FreeBSD__)
	struct usb_softc *sc = device_get_softc(self);
	void *aux = device_get_ivars(self);
	static int global_init_done = 0;
#endif
	usbd_device_handle dev;
	usbd_status err;
	int usbrev;
	int speed;
	struct usb_event ue;

	sc->sc_dev = self;

	DPRINTF(("usbd_attach\n"));

	usbd_init();
	sc->sc_bus = aux;
	sc->sc_bus->usbctl = sc;
	sc->sc_port.power = USB_MAX_POWER;

#if defined(__FreeBSD__)
	printf("%s", USBDEVNAME(sc->sc_dev));
#endif
	usbrev = sc->sc_bus->usbrev;
	printf(": USB revision %s", usbrev_str[usbrev]);
	switch (usbrev) {
	case USBREV_1_0:
	case USBREV_1_1:
		speed = USB_SPEED_FULL;
		break;
	case USBREV_2_0:
		speed = USB_SPEED_HIGH;
		break;
	default:
		printf(", not supported\n");
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}
	printf("\n");

	/* Make sure not to use tsleep() if we are cold booting. */
	if (cold)
		sc->sc_bus->use_polling++;

	ue.u.ue_ctrlr.ue_bus = USBDEVUNIT(sc->sc_dev);
	usb_add_event(USB_EVENT_CTRLR_ATTACH, &ue);

#ifdef USB_USE_SOFTINTR
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	/* XXX we should have our own level */
	sc->sc_bus->soft = softintr_establish(IPL_SOFTNET,
	    sc->sc_bus->methods->soft_intr, sc->sc_bus);
	if (sc->sc_bus->soft == NULL) {
		printf("%s: can't register softintr\n", USBDEVNAME(sc->sc_dev));
		sc->sc_dying = 1;
		USB_ATTACH_ERROR_RETURN;
	}
#else
	usb_callout_init(sc->sc_bus->softi);
#endif
#endif

	err = usbd_new_device(USBDEV(sc->sc_dev), sc->sc_bus, 0, speed, 0,
		  &sc->sc_port);
	if (!err) {
		dev = sc->sc_port.device;
		if (dev->hub == NULL) {
			sc->sc_dying = 1;
			printf("%s: root device is not a hub\n",
			       USBDEVNAME(sc->sc_dev));
			USB_ATTACH_ERROR_RETURN;
		}
		sc->sc_bus->root_hub = dev;
#if 1
		/*
		 * Turning this code off will delay attachment of USB devices
		 * until the USB event thread is running, which means that
		 * the keyboard will not work until after cold boot.
		 */
#if defined(__FreeBSD__)
		if (cold)
#else
		if (cold && (sc->sc_dev.dv_cfdata->cf_flags & 1))
#endif
			dev->hub->explore(sc->sc_bus->root_hub);
#endif
	} else {
		printf("%s: root hub problem, error=%d\n",
		       USBDEVNAME(sc->sc_dev), err);
		sc->sc_dying = 1;
	}
	if (cold)
		sc->sc_bus->use_polling--;

	config_pending_incr();
#if defined(__NetBSD__) || defined(__OpenBSD__)
	usb_kthread_create(usb_create_event_thread, sc);
#endif

#if defined(__FreeBSD__)
	usb_create_event_thread(sc);
	/* The per controller devices (used for usb_discover) */
	/* XXX This is redundant now, but old usbd's will want it */
	make_dev(&usb_cdevsw, device_get_unit(self), UID_ROOT, GID_OPERATOR,
		0660, "usb%d", device_get_unit(self));
	if (!global_init_done) {
		/* The device spitting out events */
		make_dev(&usb_cdevsw, USB_DEV_MINOR, UID_ROOT, GID_OPERATOR,
			0660, "usb");
		global_init_done = 1;
	}
#endif

	USB_ATTACH_SUCCESS_RETURN;
}

void
usb_create_event_thread(void *arg)
{
	struct usb_softc *sc = arg;
	static int created = 0;

	if (usb_kthread_create1(usb_event_thread, sc, &sc->sc_event_thread,
			   "%s", USBDEVNAME(sc->sc_dev))) {
		printf("%s: unable to create event thread for\n",
		       USBDEVNAME(sc->sc_dev));
		panic("usb_create_event_thread");
	}
	if (!created) {
		created = 1;
		TAILQ_INIT(&usb_all_tasks);
		if (usb_kthread_create2(usb_task_thread, NULL,
					&usb_task_thread_proc, "usbtask")) {
			printf("unable to create task thread\n");
			panic("usb_create_event_thread task");
		}
	}
}

/*
 * Add a task to be performed by the task thread.  This function can be
 * called from any context and the task will be executed in a process
 * context ASAP.
 */
void
usb_add_task(usbd_device_handle dev, struct usb_task *task)
{
	int s;

	s = splusb();
	if (!task->onqueue) {
		DPRINTFN(2,("usb_add_task: task=%p\n", task));
		TAILQ_INSERT_TAIL(&usb_all_tasks, task, next);
		task->onqueue = 1;
	} else {
		DPRINTFN(3,("usb_add_task: task=%p on q\n", task));
	}
	wakeup(&usb_all_tasks);
	splx(s);
}

void
usb_rem_task(usbd_device_handle dev, struct usb_task *task)
{
	int s;

	s = splusb();
	if (task->onqueue) {
		TAILQ_REMOVE(&usb_all_tasks, task, next);
		task->onqueue = 0;
	}
	splx(s);
}

void
usb_event_thread(void *arg)
{
	struct usb_softc *sc = arg;

#ifdef __FreeBSD__
	mtx_lock(&Giant);
#endif

	DPRINTF(("usb_event_thread: start\n"));

	/*
	 * In case this controller is a companion controller to an
	 * EHCI controller we need to wait until the EHCI controller
	 * has grabbed the port.
	 * XXX It would be nicer to do this with a tsleep(), but I don't
	 * know how to synchronize the creation of the threads so it
	 * will work.
	 */
	usb_delay_ms(sc->sc_bus, 500);

	/* Make sure first discover does something. */
	sc->sc_bus->needs_explore = 1;
	usb_discover(sc);
	config_pending_decr();

	while (!sc->sc_dying) {
#ifdef USB_DEBUG
		if (usb_noexplore < 2)
#endif
		usb_discover(sc);
#ifdef USB_DEBUG
		(void)tsleep(&sc->sc_bus->needs_explore, PWAIT, "usbevt",
		    usb_noexplore ? 0 : hz * 60);
#else
		(void)tsleep(&sc->sc_bus->needs_explore, PWAIT, "usbevt",
		    hz * 60);
#endif
		DPRINTFN(2,("usb_event_thread: woke up\n"));
	}
	sc->sc_event_thread = NULL;

	/* In case parent is waiting for us to exit. */
	wakeup(sc);

	DPRINTF(("usb_event_thread: exit\n"));
	kthread_exit(0);
}

void
usb_task_thread(void *arg)
{
	struct usb_task *task;
	int s;

#ifdef __FreeBSD__
	mtx_lock(&Giant);
#endif

	DPRINTF(("usb_task_thread: start\n"));

	s = splusb();
	for (;;) {
		task = TAILQ_FIRST(&usb_all_tasks);
		if (task == NULL) {
			tsleep(&usb_all_tasks, PWAIT, "usbtsk", 0);
			task = TAILQ_FIRST(&usb_all_tasks);
		}
		DPRINTFN(2,("usb_task_thread: woke up task=%p\n", task));
		if (task != NULL) {
			TAILQ_REMOVE(&usb_all_tasks, task, next);
			task->onqueue = 0;
			splx(s);
			task->fun(task->arg);
			s = splusb();
		}
	}
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
usbctlprint(void *aux, const char *pnp)
{
	/* only "usb"es can attach to host controllers */
	if (pnp)
		printf("usb at %s", pnp);

	return (UNCONF);
}
#endif /* defined(__NetBSD__) || defined(__OpenBSD__) */

int
usbopen(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	int unit = USBUNIT(dev);
	struct usb_softc *sc;

	if (unit == USB_DEV_MINOR) {
		if (usb_dev_open)
			return (EBUSY);
		usb_dev_open = 1;
		usb_async_proc = 0;
		return (0);
	}

	USB_GET_SC_OPEN(usb, unit, sc);

	if (sc->sc_dying)
		return (EIO);

	return (0);
}

int
usbread(dev_t dev, struct uio *uio, int flag)
{
	struct usb_event ue;
	int unit = USBUNIT(dev);
	int s, error, n;

	if (unit != USB_DEV_MINOR)
		return (ENODEV);

	if (uio->uio_resid != sizeof(struct usb_event))
		return (EINVAL);

	error = 0;
	s = splusb();
	for (;;) {
		n = usb_get_next_event(&ue);
		if (n != 0)
			break;
		if (flag & IO_NDELAY) {
			error = EWOULDBLOCK;
			break;
		}
		error = tsleep(&usb_events, PZERO | PCATCH, "usbrea", 0);
		if (error)
			break;
	}
	splx(s);
	if (!error)
		error = uiomove((void *)&ue, uio->uio_resid, uio);

	return (error);
}

int
usbclose(dev_t dev, int flag, int mode, usb_proc_ptr p)
{
	int unit = USBUNIT(dev);

	if (unit == USB_DEV_MINOR) {
		usb_async_proc = 0;
		usb_dev_open = 0;
	}

	return (0);
}

int
usbioctl(dev_t devt, u_long cmd, caddr_t data, int flag, usb_proc_ptr p)
{
	struct usb_softc *sc;
	int unit = USBUNIT(devt);

	if (unit == USB_DEV_MINOR) {
		switch (cmd) {
		case FIONBIO:
			/* All handled in the upper FS layer. */
			return (0);

		case FIOASYNC:
			if (*(int *)data)
				usb_async_proc = p->td_proc;
			else
				usb_async_proc = 0;
			return (0);

		default:
			return (EINVAL);
		}
	}

	USB_GET_SC(usb, unit, sc);

	if (sc->sc_dying)
		return (EIO);

	switch (cmd) {
#if defined(__FreeBSD__)
	/* This part should be deleted */
  	case USB_DISCOVER:
  		break;
#endif
	case USB_REQUEST:
	{
		struct usb_ctl_request *ur = (void *)data;
		int len = UGETW(ur->ucr_request.wLength);
		struct iovec iov;
		struct uio uio;
		void *ptr = 0;
		int addr = ur->ucr_addr;
		usbd_status err;
		int error = 0;

		DPRINTF(("usbioctl: USB_REQUEST addr=%d len=%d\n", addr, len));
		if (len < 0 || len > 32768)
			return (EINVAL);
		if (addr < 0 || addr >= USB_MAX_DEVICES ||
		    sc->sc_bus->devices[addr] == 0)
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
			uio.uio_procp = p;
			ptr = malloc(len, M_TEMP, M_WAITOK);
			if (uio.uio_rw == UIO_WRITE) {
				error = uiomove(ptr, len, &uio);
				if (error)
					goto ret;
			}
		}
		err = usbd_do_request_flags(sc->sc_bus->devices[addr],
			  &ur->ucr_request, ptr, ur->ucr_flags, &ur->ucr_actlen,
			  USBD_DEFAULT_TIMEOUT);
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

	case USB_DEVICEINFO:
	{
		struct usb_device_info *di = (void *)data;
		int addr = di->udi_addr;
		usbd_device_handle dev;

		if (addr < 1 || addr >= USB_MAX_DEVICES)
			return (EINVAL);
		dev = sc->sc_bus->devices[addr];
		if (dev == NULL)
			return (ENXIO);
		usbd_fill_deviceinfo(dev, di, 1);
		break;
	}

	case USB_DEVICESTATS:
		*(struct usb_device_stats *)data = sc->sc_bus->stats;
		break;

	default:
		return (EINVAL);
	}
	return (0);
}

int
usbpoll(dev_t dev, int events, usb_proc_ptr p)
{
	int revents, mask, s;
	int unit = USBUNIT(dev);

	if (unit == USB_DEV_MINOR) {
		revents = 0;
		mask = POLLIN | POLLRDNORM;

		s = splusb();
		if (events & mask && usb_nevents > 0)
			revents |= events & mask;
		if (revents == 0 && events & mask)
			selrecord(p, &usb_selevent);
		splx(s);

		return (revents);
	} else {
#if defined(__FreeBSD__)
		return (0);	/* select/poll never wakes up - back compat */
#else
		return (ENXIO);
#endif
	}
}

/* Explore device tree from the root. */
Static void
usb_discover(void *v)
{
	struct usb_softc *sc = v;

#if defined(__FreeBSD__)
	/* splxxx should be changed to mutexes for preemption safety some day */
	int s;
#endif

	DPRINTFN(2,("usb_discover\n"));
#ifdef USB_DEBUG
	if (usb_noexplore > 1)
		return;
#endif

	/*
	 * We need mutual exclusion while traversing the device tree,
	 * but this is guaranteed since this function is only called
	 * from the event thread for the controller.
	 */
#if defined(__FreeBSD__)
	s = splusb();
#endif
	while (sc->sc_bus->needs_explore && !sc->sc_dying) {
		sc->sc_bus->needs_explore = 0;
#if defined(__FreeBSD__)
		splx(s);
#endif
		sc->sc_bus->root_hub->hub->explore(sc->sc_bus->root_hub);
#if defined(__FreeBSD__)
		s = splusb();
#endif
	}
#if defined(__FreeBSD__)
	splx(s);
#endif
}

void
usb_needs_explore(usbd_device_handle dev)
{
	DPRINTFN(2,("usb_needs_explore\n"));
	dev->bus->needs_explore = 1;
	wakeup(&dev->bus->needs_explore);
}

/* Called at splusb() */
int
usb_get_next_event(struct usb_event *ue)
{
	struct usb_event_q *ueq;

	if (usb_nevents <= 0)
		return (0);
	ueq = TAILQ_FIRST(&usb_events);
#ifdef DIAGNOSTIC
	if (ueq == NULL) {
		printf("usb: usb_nevents got out of sync! %d\n", usb_nevents);
		usb_nevents = 0;
		return (0);
	}
#endif
	*ue = ueq->ue;
	TAILQ_REMOVE(&usb_events, ueq, next);
	free(ueq, M_USBDEV);
	usb_nevents--;
	return (1);
}

void
usbd_add_dev_event(int type, usbd_device_handle udev)
{
	struct usb_event ue;

	usbd_fill_deviceinfo(udev, &ue.u.ue_device, USB_EVENT_IS_ATTACH(type));
	usb_add_event(type, &ue);
}

void
usbd_add_drv_event(int type, usbd_device_handle udev, device_ptr_t dev)
{
	struct usb_event ue;

	ue.u.ue_driver.ue_cookie = udev->cookie;
	strncpy(ue.u.ue_driver.ue_devname, USBDEVPTRNAME(dev),
	    sizeof ue.u.ue_driver.ue_devname);
	usb_add_event(type, &ue);
}

void
usb_add_event(int type, struct usb_event *uep)
{
	struct usb_event_q *ueq;
	struct usb_event ue;
	struct timeval thetime;
	int s;

	ueq = malloc(sizeof *ueq, M_USBDEV, M_WAITOK);
	ueq->ue = *uep;
	ueq->ue.ue_type = type;
	microtime(&thetime);
	TIMEVAL_TO_TIMESPEC(&thetime, &ueq->ue.ue_time);

	s = splusb();
	if (USB_EVENT_IS_DETACH(type)) {
		struct usb_event_q *ueqi, *ueqi_next;

		for (ueqi = TAILQ_FIRST(&usb_events); ueqi; ueqi = ueqi_next) {
			ueqi_next = TAILQ_NEXT(ueqi, next);
			if (ueqi->ue.u.ue_driver.ue_cookie.cookie ==
			    uep->u.ue_device.udi_cookie.cookie) {
				TAILQ_REMOVE(&usb_events, ueqi, next);
				free(ueqi, M_USBDEV);
				usb_nevents--;
				ueqi_next = TAILQ_FIRST(&usb_events);
			}
		}
	}
	if (usb_nevents >= USB_MAX_EVENTS) {
		/* Too many queued events, drop an old one. */
		DPRINTF(("usb: event dropped\n"));
		(void)usb_get_next_event(&ue);
	}
	TAILQ_INSERT_TAIL(&usb_events, ueq, next);
	usb_nevents++;
	wakeup(&usb_events);
	selwakeup(&usb_selevent);
	if (usb_async_proc != NULL) {
		PROC_LOCK(usb_async_proc);
		psignal(usb_async_proc, SIGIO);
		PROC_UNLOCK(usb_async_proc);
	}
	splx(s);
}

void
usb_schedsoftintr(usbd_bus_handle bus)
{
	DPRINTFN(10,("usb_schedsoftintr: polling=%d\n", bus->use_polling));
#ifdef USB_USE_SOFTINTR
	if (bus->use_polling) {
		bus->methods->soft_intr(bus);
	} else {
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
		softintr_schedule(bus->soft);
#else
		if (!callout_pending(&bus->softi))
			callout_reset(&bus->softi, 0, bus->methods->soft_intr,
			    bus);
#endif /* __HAVE_GENERIC_SOFT_INTERRUPTS */
	}
#else
       bus->methods->soft_intr(bus);
#endif /* USB_USE_SOFTINTR */
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
usb_activate(device_ptr_t self, enum devact act)
{
	struct usb_softc *sc = (struct usb_softc *)self;
	usbd_device_handle dev = sc->sc_port.device;
	int i, rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		if (dev != NULL && dev->cdesc != NULL && dev->subdevs != NULL) {
			for (i = 0; dev->subdevs[i]; i++)
				rv |= config_deactivate(dev->subdevs[i]);
		}
		break;
	}
	return (rv);
}

int
usb_detach(device_ptr_t self, int flags)
{
	struct usb_softc *sc = (struct usb_softc *)self;
	struct usb_event ue;

	DPRINTF(("usb_detach: start\n"));

	sc->sc_dying = 1;

	/* Make all devices disconnect. */
	if (sc->sc_port.device != NULL)
		usb_disconnect_port(&sc->sc_port, self);

	/* Kill off event thread. */
	if (sc->sc_event_thread != NULL) {
		wakeup(&sc->sc_bus->needs_explore);
		if (tsleep(sc, PWAIT, "usbdet", hz * 60))
			printf("%s: event thread didn't die\n",
			       USBDEVNAME(sc->sc_dev));
		DPRINTF(("usb_detach: event thread dead\n"));
	}

	usbd_finish();

#ifdef USB_USE_SOFTINTR
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	if (sc->sc_bus->soft != NULL) {
		softintr_disestablish(sc->sc_bus->soft);
		sc->sc_bus->soft = NULL;
	}
#else
	callout_stop(&sc->sc_bus->softi);
#endif
#endif

	ue.u.ue_ctrlr.ue_bus = USBDEVUNIT(sc->sc_dev);
	usb_add_event(USB_EVENT_CTRLR_DETACH, &ue);

	return (0);
}
#elif defined(__FreeBSD__)
int
usb_detach(device_t self)
{
	DPRINTF(("%s: unload, prevented\n", USBDEVNAME(self)));

	return (EINVAL);
}
#endif


#if defined(__FreeBSD__)
DRIVER_MODULE(usb, ohci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usb, uhci, usb_driver, usb_devclass, 0, 0);
DRIVER_MODULE(usb, ehci, usb_driver, usb_devclass, 0, 0);
#endif
