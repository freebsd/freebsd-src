/*-
 * Copyright (c) 2014 Jakub Wojciech Klama <jceel@FreeBSD.org>
 * All rights reserved.
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/malloc.h>

#include <dev/evdev/input.h>
#include <dev/evdev/uinput.h>
#include <dev/evdev/evdev.h>

#define	DEBUG
#ifdef DEBUG
#define	debugf(fmt, args...)	printf("evdev: " fmt "\n", ##args);
#else
#define	debugf(fmt, args...)
#endif

static int uinput_open(struct cdev *, int, int, struct thread *);
static int uinput_close(struct cdev *, int, int, struct thread *);
static int uinput_read(struct cdev *, struct uio *, int);
static int uinput_write(struct cdev *, struct uio *, int);
static int uinput_ioctl(struct cdev *, u_long, caddr_t, int, struct thread *);
static int uinput_poll(struct cdev *, int, struct thread *);
static void uinput_dtor(void *);

static int uinput_setup_provider(struct evdev_dev *, struct uinput_user_dev *);
static int uinput_cdev_create(void);

static struct cdevsw uinput_cdevsw = {
	.d_version = D_VERSION,
	.d_open = uinput_open,
	.d_close = uinput_close,
	.d_read = uinput_read,
	.d_write = uinput_write,
	.d_ioctl = uinput_ioctl,
	.d_poll = uinput_poll,
	.d_name = "uinput",
	.d_flags = D_TRACKCLOSE,
};

static struct evdev_methods uinput_ev_methods = {
	.ev_open = NULL,
	.ev_close = NULL,
};

struct uinput_cdev_softc
{
	int			ucs_open_count;

	LIST_ENTRY(uinput_cdev_softc) ucs_link;
};

struct uinput_cdev_state
{
	bool			ucs_connected;
	struct evdev_dev *	ucs_evdev;
	struct evdev_dev	ucs_state;
	struct mtx		ucs_mtx;
};

static int
uinput_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct uinput_cdev_state *state;

	state = malloc(sizeof(struct uinput_cdev_state), M_EVDEV, M_WAITOK | M_ZERO);
	state->ucs_evdev = evdev_alloc();

	devfs_set_cdevpriv(state, uinput_dtor);
	return (0);
}

static int
uinput_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct uinput_cdev_softc *sc = dev->si_drv1;

	sc->ucs_open_count--;
	return (0);
}

static void
uinput_dtor(void *data)
{
	struct uinput_cdev_state *state = (struct uinput_cdev_state *)data;

	if (state->ucs_connected)
		evdev_unregister(NULL, state->ucs_evdev);

	evdev_free(state->ucs_evdev);
	free(data, M_EVDEV);
}

static int
uinput_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct uinput_cdev_state *state;
	struct evdev_dev *evdev;
	int ret = 0;

	debugf("uinput: read %ld bytes by thread %d", uio->uio_resid,
	    uio->uio_td->td_tid);

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	evdev = state->ucs_evdev;

	if (uio->uio_resid % sizeof(struct input_event) != 0) {
		debugf("read size not multiple of struct input_event size");
		return (EINVAL);
	}

	return (0);
}

static int
uinput_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct uinput_cdev_state *state;
	struct uinput_user_dev userdev;
	struct input_event event;
	int ret = 0;
	
	debugf("uinput: write %ld bytes by thread %d", uio->uio_resid,
	    uio->uio_td->td_tid);

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	if (!state->ucs_connected) {
		/* Process written struct uinput_user_dev */
		if (uio->uio_resid != sizeof(struct uinput_user_dev)) {
			debugf("write size not multiple of struct uinput_user_dev size");
			return (EINVAL);
		}

		uiomove(&userdev, sizeof(struct uinput_user_dev), uio);
		uinput_setup_provider(state->ucs_evdev, &userdev);
	} else {
		/* Process written event */
		if (uio->uio_resid % sizeof(struct input_event) != 0) {
			debugf("write size not multiple of struct input_event size");
			return (EINVAL);
		}

		while (uio->uio_resid > 0) {
			uiomove(&event, sizeof(struct input_event), uio);
			evdev_push_event(state->ucs_evdev, event.type, event.code,
			    event.value);
		}
	}

	return (0);
}

static int
uinput_setup_provider(struct evdev_dev *evdev, struct uinput_user_dev *udev)
{
	struct input_absinfo absinfo;
	int i;

	debugf("uinput: setup_provider called, udev=%p", udev);

	evdev_set_name(evdev, udev->name);
	memcpy(&evdev->ev_id, &udev->id, sizeof(struct input_id));
	
	for (i = 0; i < ABS_CNT; i++) {
		if (!isset(&evdev->ev_abs_flags, i))
			continue;

		absinfo.minimum = udev->absmin[i];
		absinfo.maximum = udev->absmax[i];
		absinfo.fuzz = udev->absfuzz[i];
		absinfo.flat = udev->absflat[i];
		evdev_set_absinfo(evdev, i, &absinfo);
	}

	return (0);
}

static int
uinput_poll(struct cdev *dev, int events, struct thread *td)
{
	int revents = 0;

	/* Always allow write */
	if (events & (POLLOUT | POLLWRNORM))
		revents |= (events & (POLLOUT | POLLWRNORM));

	return (revents);
}

static int
uinput_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct uinput_cdev_state *state;
	int ret, len;

	len = IOCPARM_LEN(cmd);

	debugf("uinput: ioctl called: cmd=0x%08lx, data=%p", cmd, data);

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	switch (cmd) {
	case UI_DEV_CREATE:
		evdev_set_methods(state->ucs_evdev, &uinput_ev_methods);
		evdev_set_softc(state->ucs_evdev, state);
		evdev_register(NULL, state->ucs_evdev);
		state->ucs_connected = true;
		break;

	case UI_DEV_DESTROY:
		if (!state->ucs_connected)
			return (0);

		evdev_unregister(NULL, state->ucs_evdev);
		state->ucs_connected = false;
		break;

	case UI_DEV_GETPATH:
		strncpy((char *)data, state->ucs_evdev->ev_cdev_name,
		    UINPUT_MAXLEN);
		break;

	case UI_SET_EVBIT:
		evdev_support_event(state->ucs_evdev, (uint16_t)(uintptr_t)data);
		break;

	case UI_SET_KEYBIT:
		evdev_support_key(state->ucs_evdev, (uint16_t)(uintptr_t)data);
		break;

	case UI_SET_RELBIT:
		evdev_support_rel(state->ucs_evdev, (uint16_t)(uintptr_t)data);
		break;

	case UI_SET_ABSBIT:
		evdev_support_abs(state->ucs_evdev, (uint16_t)(uintptr_t)data);
		break;

	case UI_SET_MSCBIT:
		evdev_support_msc(state->ucs_evdev, (uint16_t)(uintptr_t)data);
		break;

	case UI_SET_LEDBIT:
		evdev_support_led(state->ucs_evdev, (uint16_t)(uintptr_t)data);
		break;

	case UI_SET_SNDBIT:
		evdev_support_snd(state->ucs_evdev, (uint16_t)(uintptr_t)data);
		break;

	case UI_SET_PHYS:
		evdev_set_phys(state->ucs_evdev, (char *)data);
		break;

	case UI_SET_SWBIT:
		evdev_support_sw(state->ucs_evdev, (uint16_t)(uintptr_t)data);
		break;

	case UI_SET_PROPBIT:
		break;
	}
	
	return (0);
}

static int
uinput_cdev_create(void)
{
	struct uinput_cdev_softc *sc;
	struct cdev *cdev;

	cdev = make_dev(&uinput_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "uinput");

	sc = malloc(sizeof(struct uinput_cdev_softc), M_EVDEV, M_WAITOK | M_ZERO);
	
	cdev->si_drv1 = sc;
	return (0);
}

SYSINIT(uinput, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, uinput_cdev_create, NULL);
