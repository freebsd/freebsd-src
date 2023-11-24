/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002-2020 M. Warner Losh <imp@FreeBSD.org>
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
 */
#include <sys/cdefs.h>
#include "opt_bus.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/filio.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/condvar.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <sys/sbuf.h>
#include <sys/selinfo.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/bus.h>

#include <machine/cpu.h>
#include <machine/stdarg.h>

#include <vm/uma.h>
#include <vm/vm.h>

#include <ddb/ddb.h>

STAILQ_HEAD(devq, dev_event_info);

static struct dev_softc {
	int		inuse;
	int		nonblock;
	int		queued;
	int		async;
	struct mtx	mtx;
	struct cv	cv;
	struct selinfo	sel;
	struct devq	devq;
	struct sigio	*sigio;
	uma_zone_t	zone;
} devsoftc;

/*
 * This design allows only one reader for /dev/devctl.  This is not desirable
 * in the long run, but will get a lot of hair out of this implementation.
 * Maybe we should make this device a clonable device.
 *
 * Also note: we specifically do not attach a device to the device_t tree
 * to avoid potential chicken and egg problems.  One could argue that all
 * of this belongs to the root node.
 */

#define DEVCTL_DEFAULT_QUEUE_LEN 1000
static int sysctl_devctl_queue(SYSCTL_HANDLER_ARGS);
static int devctl_queue_length = DEVCTL_DEFAULT_QUEUE_LEN;
SYSCTL_PROC(_hw_bus, OID_AUTO, devctl_queue, CTLTYPE_INT | CTLFLAG_RWTUN |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_devctl_queue, "I", "devctl queue length");

static void devctl_attach_handler(void *arg __unused, device_t dev);
static void devctl_detach_handler(void *arg __unused, device_t dev,
    enum evhdev_detach state);
static void devctl_nomatch_handler(void *arg __unused, device_t dev);

static d_open_t		devopen;
static d_close_t	devclose;
static d_read_t		devread;
static d_ioctl_t	devioctl;
static d_poll_t		devpoll;
static d_kqfilter_t	devkqfilter;

#define DEVCTL_BUFFER (1024 - sizeof(void *))
struct dev_event_info {
	STAILQ_ENTRY(dev_event_info) dei_link;
	char dei_data[DEVCTL_BUFFER];
};


static struct cdevsw dev_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	devopen,
	.d_close =	devclose,
	.d_read =	devread,
	.d_ioctl =	devioctl,
	.d_poll =	devpoll,
	.d_kqfilter =	devkqfilter,
	.d_name =	"devctl",
};

static void	filt_devctl_detach(struct knote *kn);
static int	filt_devctl_read(struct knote *kn, long hint);

static struct filterops devctl_rfiltops = {
	.f_isfd = 1,
	.f_detach = filt_devctl_detach,
	.f_event = filt_devctl_read,
};

static struct cdev *devctl_dev;
static void devaddq(const char *type, const char *what, device_t dev);

static struct devctlbridge {
	send_event_f *send_f;
} devctl_notify_hook = { .send_f = NULL };

static void
devctl_init(void)
{
	int reserve;
	uma_zone_t z;

	devctl_dev = make_dev_credf(MAKEDEV_ETERNAL, &dev_cdevsw, 0, NULL,
	    UID_ROOT, GID_WHEEL, 0600, "devctl");
	mtx_init(&devsoftc.mtx, "dev mtx", "devd", MTX_DEF);
	cv_init(&devsoftc.cv, "dev cv");
	STAILQ_INIT(&devsoftc.devq);
	knlist_init_mtx(&devsoftc.sel.si_note, &devsoftc.mtx);
	if (devctl_queue_length > 0) {
		/*
		 * Allocate a zone for the messages. Preallocate 2% of these for
		 * a reserve. Allow only devctl_queue_length slabs to cap memory
		 * usage.  The reserve usually allows coverage of surges of
		 * events during memory shortages. Normally we won't have to
		 * re-use events from the queue, but will in extreme shortages.
		 */
		z = devsoftc.zone = uma_zcreate("DEVCTL",
		    sizeof(struct dev_event_info), NULL, NULL, NULL, NULL,
		    UMA_ALIGN_PTR, 0);
		reserve = max(devctl_queue_length / 50, 100);	/* 2% reserve */
		uma_zone_set_max(z, devctl_queue_length);
		uma_zone_set_maxcache(z, 0);
		uma_zone_reserve(z, reserve);
		uma_prealloc(z, reserve);
	}
	EVENTHANDLER_REGISTER(device_attach, devctl_attach_handler,
	    NULL, EVENTHANDLER_PRI_LAST);
	EVENTHANDLER_REGISTER(device_detach, devctl_detach_handler,
	    NULL, EVENTHANDLER_PRI_LAST);
	EVENTHANDLER_REGISTER(device_nomatch, devctl_nomatch_handler,
	    NULL, EVENTHANDLER_PRI_LAST);
}
SYSINIT(devctl_init, SI_SUB_DRIVERS, SI_ORDER_SECOND, devctl_init, NULL);

/*
 * A device was added to the tree.  We are called just after it successfully
 * attaches (that is, probe and attach success for this device).  No call
 * is made if a device is merely parented into the tree.  See devnomatch
 * if probe fails.  If attach fails, no notification is sent (but maybe
 * we should have a different message for this).
 */
static void
devctl_attach_handler(void *arg __unused, device_t dev)
{
	devaddq("+", device_get_nameunit(dev), dev);
}

/*
 * A device was removed from the tree.  We are called just before this
 * happens.
 */
static void
devctl_detach_handler(void *arg __unused, device_t dev, enum evhdev_detach state)
{
	if (state == EVHDEV_DETACH_COMPLETE)
		devaddq("-", device_get_nameunit(dev), dev);
}

/*
 * Called when there's no match for this device.  This is only called
 * the first time that no match happens, so we don't keep getting this
 * message.  Should that prove to be undesirable, we can change it.
 * This is called when all drivers that can attach to a given bus
 * decline to accept this device.  Other errors may not be detected.
 */
static void
devctl_nomatch_handler(void *arg __unused, device_t dev)
{
	devaddq("?", "", dev);
}

static int
devopen(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	mtx_lock(&devsoftc.mtx);
	if (devsoftc.inuse) {
		mtx_unlock(&devsoftc.mtx);
		return (EBUSY);
	}
	/* move to init */
	devsoftc.inuse = 1;
	mtx_unlock(&devsoftc.mtx);
	return (0);
}

static int
devclose(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	mtx_lock(&devsoftc.mtx);
	devsoftc.inuse = 0;
	devsoftc.nonblock = 0;
	devsoftc.async = 0;
	cv_broadcast(&devsoftc.cv);
	funsetown(&devsoftc.sigio);
	mtx_unlock(&devsoftc.mtx);
	return (0);
}

/*
 * The read channel for this device is used to report changes to
 * userland in realtime.  We are required to free the data as well as
 * the n1 object because we allocate them separately.  Also note that
 * we return one record at a time.  If you try to read this device a
 * character at a time, you will lose the rest of the data.  Listening
 * programs are expected to cope.
 */
static int
devread(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct dev_event_info *n1;
	int rv;

	mtx_lock(&devsoftc.mtx);
	while (STAILQ_EMPTY(&devsoftc.devq)) {
		if (devsoftc.nonblock) {
			mtx_unlock(&devsoftc.mtx);
			return (EAGAIN);
		}
		rv = cv_wait_sig(&devsoftc.cv, &devsoftc.mtx);
		if (rv) {
			/*
			 * Need to translate ERESTART to EINTR here? -- jake
			 */
			mtx_unlock(&devsoftc.mtx);
			return (rv);
		}
	}
	n1 = STAILQ_FIRST(&devsoftc.devq);
	STAILQ_REMOVE_HEAD(&devsoftc.devq, dei_link);
	devsoftc.queued--;
	mtx_unlock(&devsoftc.mtx);
	rv = uiomove(n1->dei_data, strlen(n1->dei_data), uio);
	uma_zfree(devsoftc.zone, n1);
	return (rv);
}

static	int
devioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	switch (cmd) {
	case FIONBIO:
		if (*(int*)data)
			devsoftc.nonblock = 1;
		else
			devsoftc.nonblock = 0;
		return (0);
	case FIOASYNC:
		if (*(int*)data)
			devsoftc.async = 1;
		else
			devsoftc.async = 0;
		return (0);
	case FIOSETOWN:
		return fsetown(*(int *)data, &devsoftc.sigio);
	case FIOGETOWN:
		*(int *)data = fgetown(&devsoftc.sigio);
		return (0);

		/* (un)Support for other fcntl() calls. */
	case FIOCLEX:
	case FIONCLEX:
	case FIONREAD:
	default:
		break;
	}
	return (ENOTTY);
}

static	int
devpoll(struct cdev *dev, int events, struct thread *td)
{
	int	revents = 0;

	mtx_lock(&devsoftc.mtx);
	if (events & (POLLIN | POLLRDNORM)) {
		if (!STAILQ_EMPTY(&devsoftc.devq))
			revents = events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &devsoftc.sel);
	}
	mtx_unlock(&devsoftc.mtx);

	return (revents);
}

static int
devkqfilter(struct cdev *dev, struct knote *kn)
{
	int error;

	if (kn->kn_filter == EVFILT_READ) {
		kn->kn_fop = &devctl_rfiltops;
		knlist_add(&devsoftc.sel.si_note, kn, 0);
		error = 0;
	} else
		error = EINVAL;
	return (error);
}

static void
filt_devctl_detach(struct knote *kn)
{
	knlist_remove(&devsoftc.sel.si_note, kn, 0);
}

static int
filt_devctl_read(struct knote *kn, long hint)
{
	kn->kn_data = devsoftc.queued;
	return (kn->kn_data != 0);
}

/**
 * @brief Return whether the userland process is running
 */
bool
devctl_process_running(void)
{
	return (devsoftc.inuse == 1);
}

static struct dev_event_info *
devctl_alloc_dei(void)
{
	struct dev_event_info *dei = NULL;

	mtx_lock(&devsoftc.mtx);
	if (devctl_queue_length == 0)
		goto out;
	dei = uma_zalloc(devsoftc.zone, M_NOWAIT);
	if (dei == NULL)
		dei = uma_zalloc(devsoftc.zone, M_NOWAIT | M_USE_RESERVE);
	if (dei == NULL) {
		/*
		 * Guard against no items in the queue. Normally, this won't
		 * happen, but if lots of events happen all at once and there's
		 * a chance we're out of allocated space but none have yet been
		 * queued when we get here, leaving nothing to steal. This can
		 * also happen with error injection. Fail safe by returning
		 * NULL in that case..
		 */
		if (devsoftc.queued == 0)
			goto out;
		dei = STAILQ_FIRST(&devsoftc.devq);
		STAILQ_REMOVE_HEAD(&devsoftc.devq, dei_link);
		devsoftc.queued--;
	}
	MPASS(dei != NULL);
	*dei->dei_data = '\0';
out:
	mtx_unlock(&devsoftc.mtx);
	return (dei);
}

static struct dev_event_info *
devctl_alloc_dei_sb(struct sbuf *sb)
{
	struct dev_event_info *dei;

	dei = devctl_alloc_dei();
	if (dei != NULL)
		sbuf_new(sb, dei->dei_data, sizeof(dei->dei_data), SBUF_FIXEDLEN);
	return (dei);
}

static void
devctl_free_dei(struct dev_event_info *dei)
{
	uma_zfree(devsoftc.zone, dei);
}

static void
devctl_queue(struct dev_event_info *dei)
{
	mtx_lock(&devsoftc.mtx);
	STAILQ_INSERT_TAIL(&devsoftc.devq, dei, dei_link);
	devsoftc.queued++;
	cv_broadcast(&devsoftc.cv);
	KNOTE_LOCKED(&devsoftc.sel.si_note, 0);
	mtx_unlock(&devsoftc.mtx);
	selwakeup(&devsoftc.sel);
	if (devsoftc.async && devsoftc.sigio != NULL)
		pgsigio(&devsoftc.sigio, SIGIO, 0);
}

/**
 * @brief Send a 'notification' to userland, using standard ways
 */
void
devctl_notify(const char *system, const char *subsystem, const char *type,
    const char *data)
{
	struct dev_event_info *dei;
	struct sbuf sb;

	if (system == NULL || subsystem == NULL || type == NULL)
		return;
	if (devctl_notify_hook.send_f != NULL)
		devctl_notify_hook.send_f(system, subsystem, type, data);
	dei = devctl_alloc_dei_sb(&sb);
	if (dei == NULL)
		return;
	sbuf_cpy(&sb, "!system=");
	sbuf_cat(&sb, system);
	sbuf_cat(&sb, " subsystem=");
	sbuf_cat(&sb, subsystem);
	sbuf_cat(&sb, " type=");
	sbuf_cat(&sb, type);
	if (data != NULL) {
		sbuf_putc(&sb, ' ');
		sbuf_cat(&sb, data);
	}
	sbuf_putc(&sb, '\n');
	if (sbuf_finish(&sb) != 0)
		devctl_free_dei(dei);	/* overflow -> drop it */
	else
		devctl_queue(dei);
}

/*
 * Common routine that tries to make sending messages as easy as possible.
 * We allocate memory for the data, copy strings into that, but do not
 * free it unless there's an error.  The dequeue part of the driver should
 * free the data.  We don't send data when the device is disabled.  We do
 * send data, even when we have no listeners, because we wish to avoid
 * races relating to startup and restart of listening applications.
 *
 * devaddq is designed to string together the type of event, with the
 * object of that event, plus the plug and play info and location info
 * for that event.  This is likely most useful for devices, but less
 * useful for other consumers of this interface.  Those should use
 * the devctl_notify() interface instead.
 *
 * Output:
 *	${type}${what} at $(location dev) $(pnp-info dev) on $(parent dev)
 */
static void
devaddq(const char *type, const char *what, device_t dev)
{
	struct dev_event_info *dei;
	const char *parstr;
	struct sbuf sb;
	size_t beginlen;

	dei = devctl_alloc_dei_sb(&sb);
	if (dei == NULL)
		return;
	sbuf_cpy(&sb, type);
	sbuf_cat(&sb, what);
	sbuf_cat(&sb, " at ");
	beginlen = sbuf_len(&sb);

	/* Add in the location */
	bus_child_location(dev, &sb);
	sbuf_putc(&sb, ' ');

	/* Add in pnpinfo */
	bus_child_pnpinfo(dev, &sb);

	/* Get the parent of this device, or / if high enough in the tree. */
	if (device_get_parent(dev) == NULL)
		parstr = ".";	/* Or '/' ? */
	else
		parstr = device_get_nameunit(device_get_parent(dev));
	sbuf_cat(&sb, " on ");
	sbuf_cat(&sb, parstr);
	sbuf_putc(&sb, '\n');
	if (sbuf_finish(&sb) != 0)
		goto bad;
	if (devctl_notify_hook.send_f != NULL) {
		const char *t;

		switch (*type) {
		case '+':
			t = "ATTACH";
			break;
		case '-':
			t = "DETACH";
			break;
		default:
			t = "NOMATCH";
			break;
		}
		devctl_notify_hook.send_f("device",
		    what, t, sbuf_data(&sb) + beginlen);
	}
	devctl_queue(dei);
	return;
bad:
	devctl_free_dei(dei);
}

static int
sysctl_devctl_queue(SYSCTL_HANDLER_ARGS)
{
	int q, error;

	q = devctl_queue_length;
	error = sysctl_handle_int(oidp, &q, 0, req);
	if (error || !req->newptr)
		return (error);
	if (q < 0)
		return (EINVAL);

	/*
	 * When set as a tunable, we've not yet initialized the mutex.
	 * It is safe to just assign to devctl_queue_length and return
	 * as we're racing no one. We'll use whatever value set in
	 * devinit.
	 */
	if (!mtx_initialized(&devsoftc.mtx)) {
		devctl_queue_length = q;
		return (0);
	}

	/*
	 * XXX It's hard to grow or shrink the UMA zone. Only allow
	 * disabling the queue size for the moment until underlying
	 * UMA issues can be sorted out.
	 */
	if (q != 0)
		return (EINVAL);
	if (q == devctl_queue_length)
		return (0);
	mtx_lock(&devsoftc.mtx);
	devctl_queue_length = 0;
	uma_zdestroy(devsoftc.zone);
	devsoftc.zone = 0;
	mtx_unlock(&devsoftc.mtx);
	return (0);
}

/**
 * @brief safely quotes strings that might have double quotes in them.
 *
 * The devctl protocol relies on quoted strings having matching quotes.
 * This routine quotes any internal quotes so the resulting string
 * is safe to pass to snprintf to construct, for example pnp info strings.
 *
 * @param sb	sbuf to place the characters into
 * @param src	Original buffer.
 */
void
devctl_safe_quote_sb(struct sbuf *sb, const char *src)
{
	while (*src != '\0') {
		if (*src == '"' || *src == '\\')
			sbuf_putc(sb, '\\');
		sbuf_putc(sb, *src++);
	}
}

void
devctl_set_notify_hook(send_event_f *hook)
{
	devctl_notify_hook.send_f = hook;
}

void
devctl_unset_notify_hook(void)
{
	devctl_notify_hook.send_f = NULL;
}

