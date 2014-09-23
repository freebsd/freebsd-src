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
#include <sys/filio.h>
#include <sys/fcntl.h>
#include <sys/selinfo.h>
#include <sys/malloc.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>

#define	DEBUG
#ifdef DEBUG
#define	debugf(fmt, args...)	printf("evdev: " fmt "\n", ##args);
#else
#define	debugf(fmt, args...)
#endif

#define	IOCNUM(x)	(x & 0xff)

static int evdev_open(struct cdev *, int, int, struct thread *);
static int evdev_close(struct cdev *, int, int, struct thread *);
static int evdev_read(struct cdev *, struct uio *, int);
static int evdev_write(struct cdev *, struct uio *, int);
static int evdev_ioctl(struct cdev *, u_long, caddr_t, int, struct thread *);
static int evdev_poll(struct cdev *, int, struct thread *);
static int evdev_kqfilter(struct cdev *, struct knote *);
static int evdev_kqread(struct knote *kn, long hint);
static void evdev_kqdetach(struct knote *kn);
static void evdev_dtor(void *);

static void evdev_notify_event(struct evdev_client *, void *);
static int evdev_ioctl_eviocgbit(struct evdev_dev *, int, int, caddr_t);

static struct cdevsw evdev_cdevsw = {
	.d_version = D_VERSION,
	.d_open = evdev_open,
	.d_close = evdev_close,
	.d_read = evdev_read,
	.d_write = evdev_write,
	.d_ioctl = evdev_ioctl,
	.d_poll = evdev_poll,
	.d_kqfilter = evdev_kqfilter,
	.d_name = "evdev",
	.d_flags = D_TRACKCLOSE
};

static struct filterops evdev_cdev_filterops = {
	.f_isfd = 1,
	.f_attach = NULL,
	.f_detach = evdev_kqdetach,
	.f_event = evdev_kqread,
};

struct evdev_cdev_softc
{
	struct evdev_dev *	ecs_evdev;
	int			ecs_open_count;

	LIST_ENTRY(evdev_cdev_softc) ecs_link;
};

struct evdev_cdev_state
{
	struct mtx		ecs_mtx;
	struct evdev_client *	ecs_client;
	struct selinfo		ecs_selp;
	struct sigio *		ecs_sigio;
	bool			ecs_async;
	bool			ecs_revoked;
};

static int evdev_cdev_count = 0;

static int
evdev_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct evdev_cdev_softc *sc = dev->si_drv1;
	struct evdev_cdev_state *state;
	int ret;

	state = malloc(sizeof(struct evdev_cdev_state), M_EVDEV, M_WAITOK | M_ZERO);
	
	ret = evdev_register_client(sc->ecs_evdev, &state->ecs_client);
	if (ret != 0) {
		debugf("cdev: cannot register evdev client");
		return (ret);
	}

	state->ecs_client->ec_ev_notify = &evdev_notify_event;
	state->ecs_client->ec_ev_arg = state;

	knlist_init_mtx(&state->ecs_selp.si_note, NULL);

	sc->ecs_open_count++;
	devfs_set_cdevpriv(state, evdev_dtor);
	return (0);
}

static int
evdev_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct evdev_cdev_softc *sc = dev->si_drv1;

	sc->ecs_open_count--;
	return (0);
}

static void
evdev_dtor(void *data)
{
	struct evdev_cdev_state *state = (struct evdev_cdev_state *)data;

	seldrain(&state->ecs_selp);
	funsetown(&state->ecs_sigio);
	evdev_dispose_client(state->ecs_client);
	free(data, M_EVDEV);
}

static int
evdev_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct evdev_cdev_state *state;
	struct evdev_client *client;
	struct input_event *event;
	int ret = 0;
	int remaining;

	debugf("cdev: read %ld bytes by thread %d", uio->uio_resid,
	    uio->uio_td->td_tid);

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	if (state->ecs_revoked)
		return (ENODEV);

	client = state->ecs_client;

	if (uio->uio_resid % sizeof(struct input_event) != 0) {
		debugf("read size not multiple of struct input_event size");
		return (EINVAL);
	}

	remaining = uio->uio_resid / sizeof(struct input_event);

	EVDEV_CLIENT_LOCKQ(client);

	if (EVDEV_CLIENT_EMPTYQ(client)) {
		if (ioflag & O_NONBLOCK) {
			EVDEV_CLIENT_UNLOCKQ(client);
			return (EWOULDBLOCK);
		}

		mtx_sleep(client, &client->ec_buffer_mtx, 0, "evrea", 0);
	}

	for (;;) {
		if (EVDEV_CLIENT_EMPTYQ(client))
			/* Short read :-( */
			break;
	
		event = &client->ec_buffer[client->ec_buffer_head];
		client->ec_buffer_head = (client->ec_buffer_head + 1) % client->ec_buffer_size;
		remaining--;

		EVDEV_CLIENT_UNLOCKQ(client);
		uiomove(event, sizeof(struct input_event), uio);
		EVDEV_CLIENT_LOCKQ(client);

		if (remaining == 0)
			break;
	}

	EVDEV_CLIENT_UNLOCKQ(client);

	return (0);
}

static int
evdev_write(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct evdev_cdev_state *state;
	int ret = 0;
	
	debugf("cdev: write %ld bytes by thread %d", uio->uio_resid,
	    uio->uio_td->td_tid);

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	if (state->ecs_revoked)
		return (EPERM);

	if (uio->uio_resid % sizeof(struct input_event) != 0) {
		debugf("write size not multiple of struct input_event size");
		return (EINVAL);
	}

	return (0);
}

static int
evdev_poll(struct cdev *dev, int events, struct thread *td)
{
	struct evdev_client *client;
	struct evdev_cdev_state *state;
	int ret;
	int revents = 0;

	debugf("cdev: poll by thread %d", td->td_tid);

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (POLLNVAL);

	if (state->ecs_revoked)
		return (POLLNVAL);

	client = state->ecs_client;

	if (events & (POLLIN | POLLRDNORM)) {
		EVDEV_CLIENT_LOCKQ(client);
		if (!EVDEV_CLIENT_EMPTYQ(client))
			revents = events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &state->ecs_selp);
		EVDEV_CLIENT_UNLOCKQ(client);
	}

	return (revents);
}

static int
evdev_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct evdev_cdev_state *state;
	int ret;

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	kn->kn_hook = (caddr_t)state;
	kn->kn_fop = &evdev_cdev_filterops;

	knlist_add(&state->ecs_selp.si_note, kn, 0);
	return (0);
}

static int
evdev_kqread(struct knote *kn, long hint)
{
	struct evdev_client *client;
	struct evdev_cdev_state *state;
	int ret;

	state = (struct evdev_cdev_state *)kn->kn_hook;
	client = state->ecs_client;

	EVDEV_CLIENT_LOCKQ(client);
	ret = !EVDEV_CLIENT_EMPTYQ(client);
	EVDEV_CLIENT_UNLOCKQ(client);
	return (ret);
}

static void
evdev_kqdetach(struct knote *kn)
{
	struct evdev_cdev_state *state;

	state = (struct evdev_cdev_state *)kn->kn_hook;
	knlist_remove(&state->ecs_selp.si_note, kn, 0);
}

static int
evdev_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct evdev_cdev_softc *sc = dev->si_drv1;
	struct evdev_dev *evdev = sc->ecs_evdev;
	struct evdev_cdev_state *state;
	struct input_keymap_entry *ke;
	int rep_params[2];
	int ret, len, num, limit;

	ret = devfs_get_cdevpriv((void **)&state);
	if (ret != 0)
		return (ret);

	if (state->ecs_revoked)
		return (EPERM);

	/* file I/O ioctl handling */
	switch (cmd) {
	case FIOSETOWN:
		return (fsetown(*(int *)data, &state->ecs_sigio));

	case FIOGETOWN:
		*(int *)data = fgetown(&state->ecs_sigio);
		return (0);

	case FIONBIO:
		return (0);

	case FIOASYNC:
		if (*(int *)data)
			state->ecs_async = true;
		else
			state->ecs_async = false;

		return (0);
	}

	len = IOCPARM_LEN(cmd);
	cmd = IOCBASECMD(cmd);
	num = IOCNUM(cmd);
	debugf("cdev: ioctl called: cmd=0x%08lx, data=%p", cmd, data);

	/* evdev ioctls handling */
	switch (cmd) {
	case IOCBASECMD(EVIOCGVERSION):
		data = (caddr_t)EV_VERSION;
		break;

	case IOCBASECMD(EVIOCGID):
		debugf("cdev: EVIOCGID: bus=%d vendor=0x%04x product=0x%04x",
		    evdev->ev_id.bustype, evdev->ev_id.vendor,
		    evdev->ev_id.product);
		memcpy(data, &evdev->ev_id, sizeof(struct input_id));
		break;

	case IOCBASECMD(EVIOCGREP):
		if (evdev->ev_repeat_mode == NO_REPEAT)
			return ENOTSUP;

		rep_params[0] = evdev->ev_rep[REP_DELAY];
		rep_params[1] = evdev->ev_rep[REP_PERIOD];
		memcpy(data, rep_params, sizeof(rep_params));
		break;

	case IOCBASECMD(EVIOCSREP):
		if (evdev->ev_repeat_mode == NO_REPEAT)
			return ENOTSUP;

		memcpy(rep_params, data, sizeof(rep_params));
		evdev->ev_rep[REP_DELAY] = rep_params[0];
		evdev->ev_rep[REP_PERIOD] = rep_params[1];

		if (evdev->ev_repeat_mode == DRIVER_REPEAT) {
			evdev_inject_event(evdev, EV_REP, REP_DELAY, rep_params[0]);
			evdev_inject_event(evdev, EV_REP, REP_PERIOD, rep_params[1]);
		}

		break;

	case IOCBASECMD(EVIOCGKEYCODE_V2):
		if (evdev->ev_methods->ev_get_keycode == NULL)
			return ENOTSUP;

		ke = (struct input_keymap_entry *)data;
		evdev->ev_methods->ev_get_keycode(evdev, evdev->ev_softc, ke);
		break;

	case IOCBASECMD(EVIOCSKEYCODE_V2):
		if (evdev->ev_methods->ev_set_keycode == NULL)
			return ENOTSUP;

		ke = (struct input_keymap_entry *)data;
		evdev->ev_methods->ev_set_keycode(evdev, evdev->ev_softc, ke);
		break;

	case EVIOCGNAME(0):
		strlcpy(data, evdev->ev_name, len);
		break;

	case EVIOCGPHYS(0):
		strlcpy(data, evdev->ev_shortname, len);
		break;

	case EVIOCGUNIQ(0):
		strlcpy(data, evdev->ev_serial, len);
		break;

	case EVIOCGPROP(0):
		limit = MIN(len, howmany(EV_CNT, 8));
		memcpy(data, evdev->ev_type_flags, limit);
		break;

	case EVIOCGKEY(0):
		evdev_client_filter_queue(state->ecs_client, EV_KEY);
		limit = MIN(len, howmany(KEY_CNT, 8));
		memcpy(data, evdev->ev_key_states, limit);
		break;

	case EVIOCGLED(0):
		evdev_client_filter_queue(state->ecs_client, EV_LED);
		limit = MIN(len, howmany(LED_CNT, 8));
		memcpy(data, evdev->ev_led_states, limit);

		break;

	case EVIOCGSND(0):
		evdev_client_filter_queue(state->ecs_client, EV_SND);
		limit = MIN(len, howmany(SND_CNT, 8));
		memcpy(data, evdev->ev_snd_states, limit);

		break;

	case EVIOCGSW(0):
		evdev_client_filter_queue(state->ecs_client, EV_SW);
		limit = MIN(len, howmany(SW_CNT, 8));
		memcpy(data, evdev->ev_sw_states, limit);
		break;

	case IOCBASECMD(EVIOCGRAB):
		if (data)
			evdev_grab_client(state->ecs_client);
		else
			evdev_release_client(state->ecs_client);
		break;

	case IOCBASECMD(EVIOCREVOKE):
		if (*(int *)data != 0)
			return (EINVAL);

		state->ecs_revoked = true;
		break;

	}

	if (IOCGROUP(cmd) != 'E')
		return (EINVAL);

	/* Handle EVIOCGBIT variants */
	if (num >= IOCNUM(EVIOCGBIT(0, 0)) &&
	    num < IOCNUM(EVIOCGBIT(EV_MAX, 0))) {
		int type_num = num - IOCNUM(EVIOCGBIT(0, 0));
		debugf("cdev: EVIOCGBIT(%d): data=%p, len=%d", type_num, data, len);
		return (evdev_ioctl_eviocgbit(evdev, type_num, len, data));
	}

	/* Handle EVIOCGABS variants */
	if (num >= IOCNUM(EVIOCGABS(0)) && num < IOCNUM(EVIOCGABS(ABS_CNT))) {
		int index = num - IOCNUM(EVIOCGABS(0));
		memcpy(data, &evdev->ev_absinfo[index], len);
	}

	/* Handle EVIOCSABS variants */
	if (num >= IOCNUM(EVIOCSABS(0)) && num < IOCNUM(EVIOCSABS(ABS_CNT))) {
		int index = num - IOCNUM(EVIOCSABS(0));
		EVDEV_LOCK(evdev);
		memcpy(&evdev->ev_absinfo[index], data, len);
		EVDEV_UNLOCK(evdev);
	}

	return (0);
}

static int
evdev_ioctl_eviocgbit(struct evdev_dev *evdev, int type, int len, caddr_t data)
{
	unsigned long *bitmap;
	int limit;

	switch (type) {
	case 0:
		bitmap = evdev->ev_type_flags;
		limit = EV_CNT;
		break;
	case EV_KEY:
		bitmap = evdev->ev_key_flags;
		limit = KEY_CNT;
		break;
	case EV_REL:
		bitmap = evdev->ev_rel_flags;
		limit = REL_CNT;
		break;
	case EV_ABS:
		bitmap = evdev->ev_abs_flags;
		limit = ABS_CNT;
		break;
	case EV_MSC:
		bitmap = evdev->ev_msc_flags;
		limit = MSC_CNT;
		break;
	case EV_LED:
		bitmap = evdev->ev_led_flags;
		limit = LED_CNT;
		break;
	case EV_SND:
		bitmap = evdev->ev_snd_flags;
		limit = SND_CNT;
		break;
	case EV_SW:
		bitmap = evdev->ev_sw_flags;
		limit = SW_CNT;
		break;
	case EV_FF:
		/*
		 * We don't support EV_FF now, so let's
		 * just fake it returning only zeros.
		 */
		bzero(data, len);
		return (0);
	default:
		return (ENOTTY);
	}

	/* 
	 * Clear ioctl data buffer in case it's bigger than
	 * bitmap size
	 */
	bzero(data, len);

	limit = nlongs(limit) * sizeof(unsigned long);
	len = MIN(limit, len);
	memcpy(data, bitmap, len);
	return (0);
}

static void
evdev_notify_event(struct evdev_client *client, void *data)
{
	struct evdev_cdev_state *state = (struct evdev_cdev_state *)data;

	selwakeup(&state->ecs_selp);

	if (state->ecs_async && state->ecs_sigio != NULL)
		pgsigio(&state->ecs_sigio, SIGIO, 0);
}

int
evdev_cdev_create(struct evdev_dev *evdev)
{
	struct evdev_cdev_softc *sc;
	struct cdev *cdev;

	snprintf(evdev->ev_cdev_name, NAMELEN, "input/event%d",
	    evdev_cdev_count++);
	cdev = make_dev(&evdev_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
	    "%s", evdev->ev_cdev_name);

	sc = malloc(sizeof(struct evdev_cdev_softc), M_EVDEV,
	    M_WAITOK | M_ZERO);
	
	sc->ecs_evdev = evdev;
	evdev->ev_cdev = cdev;
	cdev->si_drv1 = sc;
	return (0);
}

int
evdev_cdev_destroy(struct evdev_dev *evdev)
{

	destroy_dev(evdev->ev_cdev);
	evdev_cdev_count--;
	return (0);
}
