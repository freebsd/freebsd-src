/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2023 Vladimir Kondratyev <wulf@FreeBSD.org>
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

#include "opt_hid.h"

#include <sys/param.h>
#ifdef COMPAT_FREEBSD32
#include <sys/abi_compat.h>
#endif
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/selinfo.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/uio.h>

#include <dev/evdev/input.h>

#define HID_DEBUG_VAR	u2f_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidquirk.h>

#include <dev/usb/usb_ioctl.h>

#ifdef HID_DEBUG
static int u2f_debug = 0;
static SYSCTL_NODE(_hw_hid, OID_AUTO, u2f, CTLFLAG_RW, 0,
    "FIDO/U2F authenticator");
SYSCTL_INT(_hw_hid_u2f, OID_AUTO, debug, CTLFLAG_RWTUN,
    &u2f_debug, 0, "Debug level");
#endif

#define	U2F_MAX_REPORT_SIZE	64

/* A match on these entries will load u2f */
static const struct hid_device_id u2f_devs[] = {
	{ HID_BUS(BUS_USB), HID_TLC(HUP_FIDO, HUF_U2FHID) },
};

struct u2f_softc {
	device_t sc_dev;		/* base device */
	struct cdev *dev;

	struct mtx sc_mtx;		/* hidbus private mutex */
	struct task sc_kqtask;		/* kqueue task */
	void *sc_rdesc;
	hid_size_t sc_rdesc_size;
	hid_size_t sc_isize;
	hid_size_t sc_osize;
	struct selinfo sc_rsel;
	struct {			/* driver state */
		bool	open:1;		/* device is open */
		bool	aslp:1;		/* waiting for device data in read() */
		bool	sel:1;		/* waiting for device data in poll() */
		bool	data:1;		/* input report is stored in sc_buf */
		int	reserved:28;
	} sc_state;
	int sc_fflags;			/* access mode for open lifetime */

	uint8_t sc_buf[U2F_MAX_REPORT_SIZE];
};

static d_open_t		u2f_open;
static d_read_t		u2f_read;
static d_write_t	u2f_write;
static d_ioctl_t	u2f_ioctl;
static d_poll_t		u2f_poll;
static d_kqfilter_t	u2f_kqfilter;

static d_priv_dtor_t	u2f_dtor;

static struct cdevsw u2f_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	u2f_open,
	.d_read =	u2f_read,
	.d_write =	u2f_write,
	.d_ioctl =	u2f_ioctl,
	.d_poll =	u2f_poll,
	.d_kqfilter =	u2f_kqfilter,
	.d_name =	"u2f",
};

static hid_intr_t	u2f_intr;

static device_probe_t	u2f_probe;
static device_attach_t	u2f_attach;
static device_detach_t	u2f_detach;

static void		u2f_kqtask(void *context, int pending);
static int		u2f_kqread(struct knote *, long);
static void		u2f_kqdetach(struct knote *);
static void		u2f_notify(struct u2f_softc *);

static struct filterops u2f_filterops_read = {
	.f_isfd =	1,
	.f_detach =	u2f_kqdetach,
	.f_event =	u2f_kqread,
};

static int
u2f_probe(device_t dev)
{
	int error;

	error = HIDBUS_LOOKUP_DRIVER_INFO(dev, u2f_devs);
	if (error != 0)
                return (error);

	hidbus_set_desc(dev, "Authenticator");

	return (BUS_PROBE_GENERIC);
}

static int
u2f_attach(device_t dev)
{
	struct u2f_softc *sc = device_get_softc(dev);
	struct hid_device_info *hw = __DECONST(struct hid_device_info *,
	    hid_get_device_info(dev));
	struct make_dev_args mda;
	int error;

	sc->sc_dev = dev;

	error = hid_get_report_descr(dev, &sc->sc_rdesc, &sc->sc_rdesc_size);
	if (error != 0)
		return (ENXIO);
	sc->sc_isize = hid_report_size_max(sc->sc_rdesc, sc->sc_rdesc_size,
	    hid_input, NULL);
	if (sc->sc_isize > U2F_MAX_REPORT_SIZE) {
		device_printf(dev, "Input report size too large. Truncate.\n");
		sc->sc_isize = U2F_MAX_REPORT_SIZE;
	}
	sc->sc_osize = hid_report_size_max(sc->sc_rdesc, sc->sc_rdesc_size,
	    hid_output, NULL);
	if (sc->sc_osize > U2F_MAX_REPORT_SIZE) {
		device_printf(dev, "Output report size too large. Truncate.\n");
		sc->sc_osize = U2F_MAX_REPORT_SIZE;
	}

	mtx_init(&sc->sc_mtx, "u2f lock", NULL, MTX_DEF);
	knlist_init_mtx(&sc->sc_rsel.si_note, &sc->sc_mtx);
	TASK_INIT(&sc->sc_kqtask, 0, u2f_kqtask, sc);

	make_dev_args_init(&mda);
	mda.mda_flags = MAKEDEV_WAITOK;
	mda.mda_devsw = &u2f_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_U2F;
	mda.mda_mode = 0660;
	mda.mda_si_drv1 = sc;

	error = make_dev_s(&mda, &sc->dev, "u2f/%d", device_get_unit(dev));
	if (error) {
		device_printf(dev, "Can not create character device\n");
		u2f_detach(dev);
		return (error);
	}
#ifndef U2F_DROP_UHID_ALIAS
	(void)make_dev_alias(sc->dev, "uhid%d", device_get_unit(dev));
#endif

	hid_add_dynamic_quirk(hw, HQ_NO_READAHEAD);

	hidbus_set_lock(dev, &sc->sc_mtx);
	hidbus_set_intr(dev, u2f_intr, sc);

	return (0);
}

static int
u2f_detach(device_t dev)
{
	struct u2f_softc *sc = device_get_softc(dev);

	DPRINTF("sc=%p\n", sc);

	if (sc->dev != NULL) {
		mtx_lock(&sc->sc_mtx);
		sc->dev->si_drv1 = NULL;
		/* Wake everyone */
		u2f_notify(sc);
		mtx_unlock(&sc->sc_mtx);
		destroy_dev(sc->dev);
	}

	taskqueue_drain(taskqueue_thread, &sc->sc_kqtask);
	hid_intr_stop(sc->sc_dev);

	knlist_clear(&sc->sc_rsel.si_note, 0);
	knlist_destroy(&sc->sc_rsel.si_note);
	seldrain(&sc->sc_rsel);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

void
u2f_intr(void *context, void *buf, hid_size_t len)
{
	struct u2f_softc *sc = context;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	DPRINTFN(5, "len=%d\n", len);
	DPRINTFN(5, "data = %*D\n", len, buf, " ");

	if (sc->sc_state.data)
		return;

	if (len > sc->sc_isize)
		len = sc->sc_isize;

	bcopy(buf, sc->sc_buf, len);

	/* Make sure we don't process old data */
	if (len < sc->sc_isize)
		bzero(sc->sc_buf + len, sc->sc_isize - len);

	sc->sc_state.data = true;

	u2f_notify(sc);
}

static int
u2f_open(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct u2f_softc *sc = dev->si_drv1;
	int error;

	if (sc == NULL)
		return (ENXIO);

	DPRINTF("sc=%p\n", sc);

	mtx_lock(&sc->sc_mtx);
	if (sc->sc_state.open) {
		mtx_unlock(&sc->sc_mtx);
		return (EBUSY);
	}
	sc->sc_state.open = true;
	mtx_unlock(&sc->sc_mtx);

	error = devfs_set_cdevpriv(sc, u2f_dtor);
	if (error != 0) {
		mtx_lock(&sc->sc_mtx);
		sc->sc_state.open = false;
		mtx_unlock(&sc->sc_mtx);
		return (error);
	}

	/* Set up interrupt pipe. */
	sc->sc_state.data = false;
	sc->sc_fflags = flag;

	return (0);
}


static void
u2f_dtor(void *data)
{
	struct u2f_softc *sc = data;

#ifdef NOT_YET
	/* Disable interrupts. */
	hid_intr_stop(sc->sc_dev);
#endif

	mtx_lock(&sc->sc_mtx);
	sc->sc_state.open = false;
	mtx_unlock(&sc->sc_mtx);
}

static int
u2f_read(struct cdev *dev, struct uio *uio, int flag)
{
	uint8_t buf[U2F_MAX_REPORT_SIZE];
	struct u2f_softc *sc = dev->si_drv1;
	size_t length = 0;
	int error;

	DPRINTFN(1, "\n");

	if (sc == NULL)
		return (EIO);

	if (!sc->sc_state.data)
		hid_intr_start(sc->sc_dev);

	mtx_lock(&sc->sc_mtx);
	if (dev->si_drv1 == NULL) {
		error = EIO;
		goto exit;
	}

	while (!sc->sc_state.data) {
		if (flag & O_NONBLOCK) {
			error = EWOULDBLOCK;
			goto exit;
		}
		sc->sc_state.aslp = true;
		DPRINTFN(5, "sleep on %p\n", &sc->sc_buf);
		error = mtx_sleep(&sc->sc_buf, &sc->sc_mtx, PZERO | PCATCH,
		    "u2frd", 0);
		DPRINTFN(5, "woke, error=%d\n", error);
		if (dev->si_drv1 == NULL)
			error = EIO;
		if (error) {
			sc->sc_state.aslp = false;
			goto exit;
		}
	}

	if (sc->sc_state.data && uio->uio_resid > 0) {
		length = min(uio->uio_resid, sc->sc_isize);
		memcpy(buf, sc->sc_buf, length);
		sc->sc_state.data = false;
	}
exit:
	mtx_unlock(&sc->sc_mtx);
	if (length != 0) {
		/* Copy the data to the user process. */
		DPRINTFN(5, "got %lu chars\n", (u_long)length);
		error = uiomove(buf, length, uio);
	}

	return (error);
}

static int
u2f_write(struct cdev *dev, struct uio *uio, int flag)
{
	uint8_t buf[U2F_MAX_REPORT_SIZE];
	struct u2f_softc *sc = dev->si_drv1;
	int error;

	DPRINTFN(1, "\n");

	if (sc == NULL)
		return (EIO);

	if (uio->uio_resid != sc->sc_osize)
		return (EINVAL);
	error = uiomove(buf, uio->uio_resid, uio);
	if (error == 0)
		error = hid_write(sc->sc_dev, buf, sc->sc_osize);

	return (error);
}

#ifdef COMPAT_FREEBSD32
static void
update_ugd32(const struct usb_gen_descriptor *ugd,
    struct usb_gen_descriptor32 *ugd32)
{
	/* Don't update hgd_data pointer */
	CP(*ugd, *ugd32, ugd_lang_id);
	CP(*ugd, *ugd32, ugd_maxlen);
	CP(*ugd, *ugd32, ugd_actlen);
	CP(*ugd, *ugd32, ugd_offset);
	CP(*ugd, *ugd32, ugd_config_index);
	CP(*ugd, *ugd32, ugd_string_index);
	CP(*ugd, *ugd32, ugd_iface_index);
	CP(*ugd, *ugd32, ugd_altif_index);
	CP(*ugd, *ugd32, ugd_endpt_index);
	CP(*ugd, *ugd32, ugd_report_type);
	/* Don't update reserved */
}
#endif

static int
u2f_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *td)
{
#ifdef COMPAT_FREEBSD32
	struct usb_gen_descriptor local_ugd;
	struct usb_gen_descriptor32 *ugd32 = NULL;
#endif
	struct u2f_softc *sc = dev->si_drv1;
	struct usb_gen_descriptor *ugd = (struct usb_gen_descriptor *)addr;
	uint32_t size;

	DPRINTFN(2, "cmd=%lx\n", cmd);

	if (sc == NULL)
		return (EIO);

#ifdef COMPAT_FREEBSD32
	switch (cmd) {
	case USB_GET_REPORT_DESC32:
		cmd = _IOC_NEWTYPE(cmd, struct usb_gen_descriptor);
		ugd32 = (struct usb_gen_descriptor32 *)addr;
		ugd = &local_ugd;
		PTRIN_CP(*ugd32, *ugd, ugd_data);
		CP(*ugd32, *ugd, ugd_lang_id);
		CP(*ugd32, *ugd, ugd_maxlen);
		CP(*ugd32, *ugd, ugd_actlen);
		CP(*ugd32, *ugd, ugd_offset);
		CP(*ugd32, *ugd, ugd_config_index);
		CP(*ugd32, *ugd, ugd_string_index);
		CP(*ugd32, *ugd, ugd_iface_index);
		CP(*ugd32, *ugd, ugd_altif_index);
		CP(*ugd32, *ugd, ugd_endpt_index);
		CP(*ugd32, *ugd, ugd_report_type);
		/* Don't copy reserved */
		break;
	}
#endif

	/* fixed-length ioctls handling */
	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		return (0);

	case USB_GET_REPORT_DESC:
		size = MIN(sc->sc_rdesc_size, ugd->ugd_maxlen);
		ugd->ugd_actlen = size;
#ifdef COMPAT_FREEBSD32
		if (ugd32 != NULL)
			update_ugd32(ugd, ugd32);
#endif
		if (ugd->ugd_data == NULL)
			return (0);		/* descriptor length only */

		return (copyout(sc->sc_rdesc, ugd->ugd_data, size));

	case USB_GET_DEVICEINFO:
		return(hid_ioctl(
		    sc->sc_dev, USB_GET_DEVICEINFO, (uintptr_t)addr));
	}

	return (EINVAL);
}

static int
u2f_poll(struct cdev *dev, int events, struct thread *td)
{
	struct u2f_softc *sc = dev->si_drv1;
	int revents = 0;
	bool start_intr = false;

	if (sc == NULL)
                return (POLLHUP);

	if (events & (POLLOUT | POLLWRNORM) && (sc->sc_fflags & FWRITE))
		revents |= events & (POLLOUT | POLLWRNORM);
	if (events & (POLLIN | POLLRDNORM) && (sc->sc_fflags & FREAD)) {
		mtx_lock(&sc->sc_mtx);
		if (sc->sc_state.data)
			revents |= events & (POLLIN | POLLRDNORM);
		else {
			sc->sc_state.sel = true;
			start_intr = true;
			selrecord(td, &sc->sc_rsel);
		}
		mtx_unlock(&sc->sc_mtx);
		if (start_intr)
			hid_intr_start(sc->sc_dev);
	}

	return (revents);
}

static int
u2f_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct u2f_softc *sc = dev->si_drv1;

	if (sc == NULL)
		return (ENXIO);

	switch(kn->kn_filter) {
	case EVFILT_READ:
		if (sc->sc_fflags & FREAD) {
			kn->kn_fop = &u2f_filterops_read;
			break;
		}
		/* FALLTHROUGH */
	default:
		return(EINVAL);
	}
	kn->kn_hook = sc;

	knlist_add(&sc->sc_rsel.si_note, kn, 0);
	return (0);
}

static void
u2f_kqtask(void *context, int pending)
{
	struct u2f_softc *sc = context;

	hid_intr_start(sc->sc_dev);
}

static int
u2f_kqread(struct knote *kn, long hint)
{
	struct u2f_softc *sc = kn->kn_hook;
	int ret;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	if (sc->dev->si_drv1 == NULL) {
		kn->kn_flags |= EV_EOF;
		ret = 1;
	} else {
		ret = sc->sc_state.data ? 1 : 0;
		if (!sc->sc_state.data)
			taskqueue_enqueue(taskqueue_thread, &sc->sc_kqtask);
	}

	return (ret);
}

static void
u2f_kqdetach(struct knote *kn)
{
	struct u2f_softc *sc = kn->kn_hook;

	knlist_remove(&sc->sc_rsel.si_note, kn, 0);
}

static void
u2f_notify(struct u2f_softc *sc)
{
	mtx_assert(&sc->sc_mtx, MA_OWNED);

	if (sc->sc_state.aslp) {
		sc->sc_state.aslp = false;
		DPRINTFN(5, "waking %p\n", &sc->sc_buf);
		wakeup(&sc->sc_buf);
	}
	if (sc->sc_state.sel) {
		sc->sc_state.sel = false;
		selwakeuppri(&sc->sc_rsel, PZERO);
	}
	KNOTE_LOCKED(&sc->sc_rsel.si_note, 0);
}

static device_method_t u2f_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		u2f_probe),
	DEVMETHOD(device_attach,	u2f_attach),
	DEVMETHOD(device_detach,	u2f_detach),

	DEVMETHOD_END
};

static driver_t u2f_driver = {
#ifdef U2F_DROP_UHID_ALIAS
	"uf2",
#else
	"uhid",
#endif
	u2f_methods,
	sizeof(struct u2f_softc)
};

DRIVER_MODULE(u2f, hidbus, u2f_driver, NULL, NULL);
MODULE_DEPEND(u2f, hidbus, 1, 1, 1);
MODULE_DEPEND(u2f, hid, 1, 1, 1);
MODULE_VERSION(u2f, 1);
HID_PNP_INFO(u2f_devs);
