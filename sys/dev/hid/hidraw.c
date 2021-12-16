/*-
 * SPDX-License-Identifier: BSD-2-Clause-NetBSD
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
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
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/tty.h>
#include <sys/uio.h>

#define HID_DEBUG_VAR	hidraw_debug
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidraw.h>

#ifdef HID_DEBUG
static int hidraw_debug = 0;
static SYSCTL_NODE(_hw_hid, OID_AUTO, hidraw, CTLFLAG_RW, 0,
    "HID raw interface");
SYSCTL_INT(_hw_hid_hidraw, OID_AUTO, debug, CTLFLAG_RWTUN,
    &hidraw_debug, 0, "Debug level");
#endif

#define	HIDRAW_INDEX		0xFF	/* Arbitrary high value */

#define	HIDRAW_LOCAL_BUFSIZE	64	/* Size of on-stack buffer. */
#define	HIDRAW_LOCAL_ALLOC(local_buf, size)		\
	(sizeof(local_buf) > (size) ? (local_buf) :	\
	    malloc((size), M_DEVBUF, M_ZERO | M_WAITOK))
#define	HIDRAW_LOCAL_FREE(local_buf, buf)		\
	if ((local_buf) != (buf)) {			\
		free((buf), M_DEVBUF);			\
	}

struct hidraw_softc {
	device_t sc_dev;		/* base device */

	struct mtx sc_mtx;		/* hidbus private mutex */

	struct hid_rdesc_info *sc_rdesc;
	const struct hid_device_info *sc_hw;

	uint8_t *sc_q;
	hid_size_t *sc_qlen;
	int sc_head;
	int sc_tail;
	int sc_sleepcnt;

	struct selinfo sc_rsel;
	struct proc *sc_async;	/* process that wants SIGIO */
	struct {			/* driver state */
		bool	open:1;		/* device is open */
		bool	aslp:1;		/* waiting for device data in read() */
		bool	sel:1;		/* waiting for device data in poll() */
		bool	quiet:1;	/* Ignore input data */
		bool	immed:1;	/* return read data immediately */
		bool	uhid:1;		/* driver switched in to uhid mode */
		bool	lock:1;		/* input queue sleepable lock */
		bool	flush:1;	/* do not wait for data in read() */
	} sc_state;
	int sc_fflags;			/* access mode for open lifetime */

	struct cdev *dev;
};

#ifdef COMPAT_FREEBSD32
struct hidraw_gen_descriptor32 {
	uint32_t hgd_data;	/* void * */
	uint16_t hgd_lang_id;
	uint16_t hgd_maxlen;
	uint16_t hgd_actlen;
	uint16_t hgd_offset;
	uint8_t hgd_config_index;
	uint8_t hgd_string_index;
	uint8_t hgd_iface_index;
	uint8_t hgd_altif_index;
	uint8_t hgd_endpt_index;
	uint8_t hgd_report_type;
	uint8_t reserved[8];
};
#define	HIDRAW_GET_REPORT_DESC32 \
    _IOC_NEWTYPE(HIDRAW_GET_REPORT_DESC, struct hidraw_gen_descriptor32)
#define	HIDRAW_GET_REPORT32 \
    _IOC_NEWTYPE(HIDRAW_GET_REPORT, struct hidraw_gen_descriptor32)
#define	HIDRAW_SET_REPORT_DESC32 \
    _IOC_NEWTYPE(HIDRAW_SET_REPORT_DESC, struct hidraw_gen_descriptor32)
#define	HIDRAW_SET_REPORT32 \
    _IOC_NEWTYPE(HIDRAW_SET_REPORT, struct hidraw_gen_descriptor32)
#endif

static d_open_t		hidraw_open;
static d_read_t		hidraw_read;
static d_write_t	hidraw_write;
static d_ioctl_t	hidraw_ioctl;
static d_poll_t		hidraw_poll;
static d_kqfilter_t	hidraw_kqfilter;

static d_priv_dtor_t	hidraw_dtor;

static struct cdevsw hidraw_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	hidraw_open,
	.d_read =	hidraw_read,
	.d_write =	hidraw_write,
	.d_ioctl =	hidraw_ioctl,
	.d_poll =	hidraw_poll,
	.d_kqfilter =	hidraw_kqfilter,
	.d_name =	"hidraw",
};

static hid_intr_t	hidraw_intr;

static device_identify_t hidraw_identify;
static device_probe_t	hidraw_probe;
static device_attach_t	hidraw_attach;
static device_detach_t	hidraw_detach;

static int		hidraw_kqread(struct knote *, long);
static void		hidraw_kqdetach(struct knote *);
static void		hidraw_notify(struct hidraw_softc *);

static struct filterops hidraw_filterops_read = {
	.f_isfd =	1,
	.f_detach =	hidraw_kqdetach,
	.f_event =	hidraw_kqread,
};

static void
hidraw_identify(driver_t *driver, device_t parent)
{
	device_t child;

	if (device_find_child(parent, "hidraw", -1) == NULL) {
		child = BUS_ADD_CHILD(parent, 0, "hidraw",
		    device_get_unit(parent));
		if (child != NULL)
			hidbus_set_index(child, HIDRAW_INDEX);
	}
}

static int
hidraw_probe(device_t self)
{

	if (hidbus_get_index(self) != HIDRAW_INDEX)
		return (ENXIO);

	hidbus_set_desc(self, "Raw HID Device");

	return (BUS_PROBE_GENERIC);
}

static int
hidraw_attach(device_t self)
{
	struct hidraw_softc *sc = device_get_softc(self);
	struct make_dev_args mda;
	int error;

	sc->sc_dev = self;
	sc->sc_rdesc = hidbus_get_rdesc_info(self);
	sc->sc_hw = hid_get_device_info(self);

	/* Hidraw mode does not require report descriptor to work */
	if (sc->sc_rdesc->data == NULL || sc->sc_rdesc->len == 0)
		device_printf(self, "no report descriptor\n");

	mtx_init(&sc->sc_mtx, "hidraw lock", NULL, MTX_DEF);
	knlist_init_mtx(&sc->sc_rsel.si_note, &sc->sc_mtx);

	make_dev_args_init(&mda);
	mda.mda_flags = MAKEDEV_WAITOK;
	mda.mda_devsw = &hidraw_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_OPERATOR;
	mda.mda_mode = 0600;
	mda.mda_si_drv1 = sc;

	error = make_dev_s(&mda, &sc->dev, "hidraw%d", device_get_unit(self));
	if (error) {
		device_printf(self, "Can not create character device\n");
		hidraw_detach(self);
		return (error);
	}
#ifdef HIDRAW_MAKE_UHID_ALIAS
	(void)make_dev_alias(sc->dev, "uhid%d", device_get_unit(self));
#endif

	hidbus_set_lock(self, &sc->sc_mtx);
	hidbus_set_intr(self, hidraw_intr, sc);

	return (0);
}

static int
hidraw_detach(device_t self)
{
	struct hidraw_softc *sc = device_get_softc(self);

	DPRINTF("sc=%p\n", sc);

	if (sc->dev != NULL) {
		mtx_lock(&sc->sc_mtx);
		sc->dev->si_drv1 = NULL;
		/* Wake everyone */
		hidraw_notify(sc);
		mtx_unlock(&sc->sc_mtx);
		destroy_dev(sc->dev);
	}

	knlist_clear(&sc->sc_rsel.si_note, 0);
	knlist_destroy(&sc->sc_rsel.si_note);
	seldrain(&sc->sc_rsel);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

void
hidraw_intr(void *context, void *buf, hid_size_t len)
{
	struct hidraw_softc *sc = context;
	int next;

	DPRINTFN(5, "len=%d\n", len);
	DPRINTFN(5, "data = %*D\n", len, buf, " ");

	next = (sc->sc_tail + 1) % HIDRAW_BUFFER_SIZE;
	if (sc->sc_state.quiet || next == sc->sc_head)
		return;

	bcopy(buf, sc->sc_q + sc->sc_tail * sc->sc_rdesc->rdsize, len);

	/* Make sure we don't process old data */
	if (len < sc->sc_rdesc->rdsize)
		bzero(sc->sc_q + sc->sc_tail * sc->sc_rdesc->rdsize + len,
		    sc->sc_rdesc->isize - len);

	sc->sc_qlen[sc->sc_tail] = len;
	sc->sc_tail = next;

	hidraw_notify(sc);
}

static inline int
hidraw_lock_queue(struct hidraw_softc *sc, bool flush)
{
	int error = 0;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	if (flush)
		sc->sc_state.flush = true;
	++sc->sc_sleepcnt;
	while (sc->sc_state.lock && error == 0) {
		/* Flush is requested. Wakeup all readers and forbid sleeps */
		if (flush && sc->sc_state.aslp) {
			sc->sc_state.aslp = false;
			DPRINTFN(5, "waking %p\n", &sc->sc_q);
			wakeup(&sc->sc_q);
		}
		error = mtx_sleep(&sc->sc_sleepcnt, &sc->sc_mtx,
		    PZERO | PCATCH, "hidrawio", 0);
	}
	--sc->sc_sleepcnt;
	if (flush)
		sc->sc_state.flush = false;
	if (error == 0)
		sc->sc_state.lock = true;

	return (error);
}

static inline void
hidraw_unlock_queue(struct hidraw_softc *sc)
{

	mtx_assert(&sc->sc_mtx, MA_OWNED);
	KASSERT(sc->sc_state.lock, ("input buffer is not locked"));

	if (sc->sc_sleepcnt != 0)
		wakeup_one(&sc->sc_sleepcnt);
	sc->sc_state.lock = false;
}

static int
hidraw_open(struct cdev *dev, int flag, int mode, struct thread *td)
{
	struct hidraw_softc *sc;
	int error;

	sc = dev->si_drv1;
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

	error = devfs_set_cdevpriv(sc, hidraw_dtor);
	if (error != 0) {
		mtx_lock(&sc->sc_mtx);
		sc->sc_state.open = false;
		mtx_unlock(&sc->sc_mtx);
		return (error);
	}

	sc->sc_q = malloc(sc->sc_rdesc->rdsize * HIDRAW_BUFFER_SIZE, M_DEVBUF,
	    M_ZERO | M_WAITOK);
	sc->sc_qlen = malloc(sizeof(hid_size_t) * HIDRAW_BUFFER_SIZE, M_DEVBUF,
	    M_ZERO | M_WAITOK);

	/* Set up interrupt pipe. */
	sc->sc_state.immed = false;
	sc->sc_async = 0;
	sc->sc_state.uhid = false;	/* hidraw mode is default */
	sc->sc_state.quiet = false;
	sc->sc_head = sc->sc_tail = 0;
	sc->sc_fflags = flag;

	hidbus_intr_start(sc->sc_dev);

	return (0);
}

static void
hidraw_dtor(void *data)
{
	struct hidraw_softc *sc = data;

	DPRINTF("sc=%p\n", sc);

	/* Disable interrupts. */
	hidbus_intr_stop(sc->sc_dev);

	sc->sc_tail = sc->sc_head = 0;
	sc->sc_async = 0;
	free(sc->sc_q, M_DEVBUF);
	free(sc->sc_qlen, M_DEVBUF);
	sc->sc_q = NULL;

	mtx_lock(&sc->sc_mtx);
	sc->sc_state.open = false;
	mtx_unlock(&sc->sc_mtx);
}

static int
hidraw_read(struct cdev *dev, struct uio *uio, int flag)
{
	struct hidraw_softc *sc;
	size_t length;
	int error;

	DPRINTFN(1, "\n");

	sc = dev->si_drv1;
	if (sc == NULL)
		return (EIO);

	mtx_lock(&sc->sc_mtx);
	error = dev->si_drv1 == NULL ? EIO : hidraw_lock_queue(sc, false);
	if (error != 0) {
		mtx_unlock(&sc->sc_mtx);
		return (error);
	}

	if (sc->sc_state.immed) {
		mtx_unlock(&sc->sc_mtx);
		DPRINTFN(1, "immed\n");

		error = hid_get_report(sc->sc_dev, sc->sc_q,
		    sc->sc_rdesc->isize, NULL, HID_INPUT_REPORT,
		    sc->sc_rdesc->iid);
		if (error == 0)
			error = uiomove(sc->sc_q, sc->sc_rdesc->isize, uio);
		mtx_lock(&sc->sc_mtx);
		goto exit;
	}

	while (sc->sc_tail == sc->sc_head && !sc->sc_state.flush) {
		if (flag & O_NONBLOCK) {
			error = EWOULDBLOCK;
			goto exit;
		}
		sc->sc_state.aslp = true;
		DPRINTFN(5, "sleep on %p\n", &sc->sc_q);
		error = mtx_sleep(&sc->sc_q, &sc->sc_mtx, PZERO | PCATCH,
		    "hidrawrd", 0);
		DPRINTFN(5, "woke, error=%d\n", error);
		if (dev->si_drv1 == NULL)
			error = EIO;
		if (error) {
			sc->sc_state.aslp = false;
			goto exit;
		}
	}

	while (sc->sc_tail != sc->sc_head && uio->uio_resid > 0) {
		length = min(uio->uio_resid, sc->sc_state.uhid ?
		    sc->sc_rdesc->isize : sc->sc_qlen[sc->sc_head]);
		mtx_unlock(&sc->sc_mtx);

		/* Copy the data to the user process. */
		DPRINTFN(5, "got %lu chars\n", (u_long)length);
		error = uiomove(sc->sc_q + sc->sc_head * sc->sc_rdesc->rdsize,
		    length, uio);

		mtx_lock(&sc->sc_mtx);
		if (error != 0)
			goto exit;
		/* Remove a small chunk from the input queue. */
		sc->sc_head = (sc->sc_head + 1) % HIDRAW_BUFFER_SIZE;
		/*
		 * In uhid mode transfer as many chunks as possible. Hidraw
		 * packets are transferred one by one due to different length.
		 */
		if (!sc->sc_state.uhid)
			goto exit;
	}
exit:
	hidraw_unlock_queue(sc);
	mtx_unlock(&sc->sc_mtx);

	return (error);
}

static int
hidraw_write(struct cdev *dev, struct uio *uio, int flag)
{
	uint8_t local_buf[HIDRAW_LOCAL_BUFSIZE], *buf;
	struct hidraw_softc *sc;
	int error;
	int size;
	size_t buf_offset;
	uint8_t id = 0;

	DPRINTFN(1, "\n");

	sc = dev->si_drv1;
	if (sc == NULL)
		return (EIO);

	if (sc->sc_rdesc->osize == 0)
		return (EOPNOTSUPP);

	buf_offset = 0;
	if (sc->sc_state.uhid) {
		size = sc->sc_rdesc->osize;
		if (uio->uio_resid != size)
			return (EINVAL);
	} else {
		size = uio->uio_resid;
		if (size < 2)
			return (EINVAL);
		/* Strip leading 0 if the device doesnt use numbered reports */
		error = uiomove(&id, 1, uio);
		if (error)
			return (error);
		if (id != 0)
			buf_offset++;
		else
			size--;
		/* Check if underlying driver could process this request */
		if (size > sc->sc_rdesc->wrsize)
			return (ENOBUFS);
	}
	buf = HIDRAW_LOCAL_ALLOC(local_buf, size);
	buf[0] = id;
	error = uiomove(buf + buf_offset, uio->uio_resid, uio);
	if (error == 0)
		error = hid_write(sc->sc_dev, buf, size);
	HIDRAW_LOCAL_FREE(local_buf, buf);

	return (error);
}

#ifdef COMPAT_FREEBSD32
static void
update_hgd32(const struct hidraw_gen_descriptor *hgd,
    struct hidraw_gen_descriptor32 *hgd32)
{
	/* Don't update hgd_data pointer */
	CP(*hgd, *hgd32, hgd_lang_id);
	CP(*hgd, *hgd32, hgd_maxlen);
	CP(*hgd, *hgd32, hgd_actlen);
	CP(*hgd, *hgd32, hgd_offset);
	CP(*hgd, *hgd32, hgd_config_index);
	CP(*hgd, *hgd32, hgd_string_index);
	CP(*hgd, *hgd32, hgd_iface_index);
	CP(*hgd, *hgd32, hgd_altif_index);
	CP(*hgd, *hgd32, hgd_endpt_index);
	CP(*hgd, *hgd32, hgd_report_type);
	/* Don't update reserved */
}
#endif

static int
hidraw_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
    struct thread *td)
{
	uint8_t local_buf[HIDRAW_LOCAL_BUFSIZE];
#ifdef COMPAT_FREEBSD32
	struct hidraw_gen_descriptor local_hgd;
	struct hidraw_gen_descriptor32 *hgd32 = NULL;
#endif
	void *buf;
	struct hidraw_softc *sc;
	struct hidraw_gen_descriptor *hgd;
	struct hidraw_report_descriptor *hrd;
	struct hidraw_devinfo *hdi;
	uint32_t size;
	int id, len;
	int error = 0;

	DPRINTFN(2, "cmd=%lx\n", cmd);

	sc = dev->si_drv1;
	if (sc == NULL)
		return (EIO);

#ifdef COMPAT_FREEBSD32
	hgd = (struct hidraw_gen_descriptor *)addr;
	switch (cmd) {
	case HIDRAW_GET_REPORT_DESC32:
	case HIDRAW_GET_REPORT32:
	case HIDRAW_SET_REPORT_DESC32:
	case HIDRAW_SET_REPORT32:
		cmd = _IOC_NEWTYPE(cmd, struct hidraw_gen_descriptor);
		hgd32 = (struct hidraw_gen_descriptor32 *)addr;
		hgd = &local_hgd;
		PTRIN_CP(*hgd32, *hgd, hgd_data);
		CP(*hgd32, *hgd, hgd_lang_id);
		CP(*hgd32, *hgd, hgd_maxlen);
		CP(*hgd32, *hgd, hgd_actlen);
		CP(*hgd32, *hgd, hgd_offset);
		CP(*hgd32, *hgd, hgd_config_index);
		CP(*hgd32, *hgd, hgd_string_index);
		CP(*hgd32, *hgd, hgd_iface_index);
		CP(*hgd32, *hgd, hgd_altif_index);
		CP(*hgd32, *hgd, hgd_endpt_index);
		CP(*hgd32, *hgd, hgd_report_type);
		/* Don't copy reserved */
		break;
	}
#endif

	/* fixed-length ioctls handling */
	switch (cmd) {
	case FIONBIO:
		/* All handled in the upper FS layer. */
		return (0);

	case FIOASYNC:
		mtx_lock(&sc->sc_mtx);
		if (*(int *)addr) {
			if (sc->sc_async == NULL) {
				sc->sc_async = td->td_proc;
				DPRINTF("FIOASYNC %p\n", sc->sc_async);
			} else
				error = EBUSY;
		} else
			sc->sc_async = NULL;
		mtx_unlock(&sc->sc_mtx);
		return (error);

	/* XXX this is not the most general solution. */
	case TIOCSPGRP:
		mtx_lock(&sc->sc_mtx);
		if (sc->sc_async == NULL)
			error = EINVAL;
		else if (*(int *)addr != sc->sc_async->p_pgid)
			error = EPERM;
		mtx_unlock(&sc->sc_mtx);
		return (error);

	case HIDRAW_GET_REPORT_DESC:
		if (sc->sc_rdesc->data == NULL || sc->sc_rdesc->len == 0)
			return (EOPNOTSUPP);
		mtx_lock(&sc->sc_mtx);
		sc->sc_state.uhid = true;
		mtx_unlock(&sc->sc_mtx);
		if (sc->sc_rdesc->len > hgd->hgd_maxlen) {
			size = hgd->hgd_maxlen;
		} else {
			size = sc->sc_rdesc->len;
		}
		hgd->hgd_actlen = size;
#ifdef COMPAT_FREEBSD32
		if (hgd32 != NULL)
			update_hgd32(hgd, hgd32);
#endif
		if (hgd->hgd_data == NULL)
			return (0);		/* descriptor length only */

		return (copyout(sc->sc_rdesc->data, hgd->hgd_data, size));


	case HIDRAW_SET_REPORT_DESC:
		if (!(sc->sc_fflags & FWRITE))
			return (EPERM);

		/* check privileges */
		error = priv_check(curthread, PRIV_DRIVER);
		if (error)
			return (error);

		/* Stop interrupts and clear input report buffer */
		mtx_lock(&sc->sc_mtx);
		sc->sc_tail = sc->sc_head = 0;
		error = hidraw_lock_queue(sc, true);
		if (error == 0)
			sc->sc_state.quiet = true;
		mtx_unlock(&sc->sc_mtx);
		if (error != 0)
			return(error);

		buf = HIDRAW_LOCAL_ALLOC(local_buf, hgd->hgd_maxlen);
		copyin(hgd->hgd_data, buf, hgd->hgd_maxlen);
		/* Lock newbus around set_report_descr call */
		mtx_lock(&Giant);
		error = hid_set_report_descr(sc->sc_dev, buf, hgd->hgd_maxlen);
		mtx_unlock(&Giant);
		HIDRAW_LOCAL_FREE(local_buf, buf);

		/* Realloc hidraw input queue */
		if (error == 0)
			sc->sc_q = realloc(sc->sc_q,
			    sc->sc_rdesc->rdsize * HIDRAW_BUFFER_SIZE,
			    M_DEVBUF, M_ZERO | M_WAITOK);

		/* Start interrupts again */
		mtx_lock(&sc->sc_mtx);
		sc->sc_state.quiet = false;
		hidraw_unlock_queue(sc);
		mtx_unlock(&sc->sc_mtx);
		return (error);
	case HIDRAW_SET_IMMED:
		if (!(sc->sc_fflags & FREAD))
			return (EPERM);
		if (*(int *)addr) {
			/* XXX should read into ibuf, but does it matter? */
			size = sc->sc_rdesc->isize;
			buf = HIDRAW_LOCAL_ALLOC(local_buf, size);
			error = hid_get_report(sc->sc_dev, buf, size, NULL,
			    HID_INPUT_REPORT, sc->sc_rdesc->iid);
			HIDRAW_LOCAL_FREE(local_buf, buf);
			if (error)
				return (EOPNOTSUPP);

			mtx_lock(&sc->sc_mtx);
			sc->sc_state.immed = true;
			mtx_unlock(&sc->sc_mtx);
		} else {
			mtx_lock(&sc->sc_mtx);
			sc->sc_state.immed = false;
			mtx_unlock(&sc->sc_mtx);
		}
		return (0);

	case HIDRAW_GET_REPORT:
		if (!(sc->sc_fflags & FREAD))
			return (EPERM);
		switch (hgd->hgd_report_type) {
		case HID_INPUT_REPORT:
			size = sc->sc_rdesc->isize;
			id = sc->sc_rdesc->iid;
			break;
		case HID_OUTPUT_REPORT:
			size = sc->sc_rdesc->osize;
			id = sc->sc_rdesc->oid;
			break;
		case HID_FEATURE_REPORT:
			size = sc->sc_rdesc->fsize;
			id = sc->sc_rdesc->fid;
			break;
		default:
			return (EINVAL);
		}
		if (id != 0)
			copyin(hgd->hgd_data, &id, 1);
		size = MIN(hgd->hgd_maxlen, size);
		buf = HIDRAW_LOCAL_ALLOC(local_buf, size);
		error = hid_get_report(sc->sc_dev, buf, size, NULL,
		    hgd->hgd_report_type, id);
		if (!error)
			error = copyout(buf, hgd->hgd_data, size);
		HIDRAW_LOCAL_FREE(local_buf, buf);
#ifdef COMPAT_FREEBSD32
		/*
		 * HIDRAW_GET_REPORT is declared _IOWR, but hgd is not written
		 * so we don't call update_hgd32().
		 */
#endif
		return (error);

	case HIDRAW_SET_REPORT:
		if (!(sc->sc_fflags & FWRITE))
			return (EPERM);
		switch (hgd->hgd_report_type) {
		case HID_INPUT_REPORT:
			size = sc->sc_rdesc->isize;
			id = sc->sc_rdesc->iid;
			break;
		case HID_OUTPUT_REPORT:
			size = sc->sc_rdesc->osize;
			id = sc->sc_rdesc->oid;
			break;
		case HID_FEATURE_REPORT:
			size = sc->sc_rdesc->fsize;
			id = sc->sc_rdesc->fid;
			break;
		default:
			return (EINVAL);
		}
		size = MIN(hgd->hgd_maxlen, size);
		buf = HIDRAW_LOCAL_ALLOC(local_buf, size);
		copyin(hgd->hgd_data, buf, size);
		if (id != 0)
			id = *(uint8_t *)buf;
		error = hid_set_report(sc->sc_dev, buf, size,
		    hgd->hgd_report_type, id);
		HIDRAW_LOCAL_FREE(local_buf, buf);
		return (error);

	case HIDRAW_GET_REPORT_ID:
		*(int *)addr = 0;	/* XXX: we only support reportid 0? */
		return (0);

	case HIDIOCGRDESCSIZE:
		*(int *)addr = sc->sc_hw->rdescsize;
		return (0);

	case HIDIOCGRDESC:
		hrd = *(struct hidraw_report_descriptor **)addr;
		error = copyin(&hrd->size, &size, sizeof(uint32_t));
		if (error)
			return (error);
		/*
		 * HID_MAX_DESCRIPTOR_SIZE-1 is a limit of report descriptor
		 * size in current Linux implementation.
		 */
		if (size >= HID_MAX_DESCRIPTOR_SIZE)
			return (EINVAL);
		buf = HIDRAW_LOCAL_ALLOC(local_buf, size);
		error = hid_get_rdesc(sc->sc_dev, buf, size);
		if (error == 0) {
			size = MIN(size, sc->sc_rdesc->len);
			error = copyout(buf, hrd->value, size);
		}
		HIDRAW_LOCAL_FREE(local_buf, buf);
		return (error);

	case HIDIOCGRAWINFO:
		hdi = (struct hidraw_devinfo *)addr;
		hdi->bustype = sc->sc_hw->idBus;
		hdi->vendor = sc->sc_hw->idVendor;
		hdi->product = sc->sc_hw->idProduct;
		return (0);
	}

	/* variable-length ioctls handling */
	len = IOCPARM_LEN(cmd);
	switch (IOCBASECMD(cmd)) {
	case HIDIOCGRAWNAME(0):
		strlcpy(addr, sc->sc_hw->name, len);
		return (0);

	case HIDIOCGRAWPHYS(0):
		strlcpy(addr, device_get_nameunit(sc->sc_dev), len);
		return (0);

	case HIDIOCSFEATURE(0):
		if (!(sc->sc_fflags & FWRITE))
			return (EPERM);
		if (len < 2)
			return (EINVAL);
		id = *(uint8_t *)addr;
		if (id == 0) {
			addr = (uint8_t *)addr + 1;
			len--;
		}
		return (hid_set_report(sc->sc_dev, addr, len,
		    HID_FEATURE_REPORT, id));

	case HIDIOCGFEATURE(0):
		if (!(sc->sc_fflags & FREAD))
			return (EPERM);
		if (len < 2)
			return (EINVAL);
		id = *(uint8_t *)addr;
		if (id == 0) {
			addr = (uint8_t *)addr + 1;
			len--;
		}
		return (hid_get_report(sc->sc_dev, addr, len, NULL,
		    HID_FEATURE_REPORT, id));

	case HIDIOCGRAWUNIQ(0):
		strlcpy(addr, sc->sc_hw->serial, len);
		return (0);
	}

	return (EINVAL);
}

static int
hidraw_poll(struct cdev *dev, int events, struct thread *td)
{
	struct hidraw_softc *sc;
	int revents = 0;

	sc = dev->si_drv1;
	if (sc == NULL)
		return (POLLHUP);

	if (events & (POLLOUT | POLLWRNORM) && (sc->sc_fflags & FWRITE))
		revents |= events & (POLLOUT | POLLWRNORM);
	if (events & (POLLIN | POLLRDNORM) && (sc->sc_fflags & FREAD)) {
		mtx_lock(&sc->sc_mtx);
		if (sc->sc_head != sc->sc_tail)
			revents |= events & (POLLIN | POLLRDNORM);
		else {
			sc->sc_state.sel = true;
			selrecord(td, &sc->sc_rsel);
		}
		mtx_unlock(&sc->sc_mtx);
	}

	return (revents);
}

static int
hidraw_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct hidraw_softc *sc;

	sc = dev->si_drv1;
	if (sc == NULL)
		return (ENXIO);

	switch(kn->kn_filter) {
	case EVFILT_READ:
		if (sc->sc_fflags & FREAD) {
			kn->kn_fop = &hidraw_filterops_read;
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

static int
hidraw_kqread(struct knote *kn, long hint)
{
	struct hidraw_softc *sc;
	int ret;

	sc = kn->kn_hook;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	if (sc->dev->si_drv1 == NULL) {
		kn->kn_flags |= EV_EOF;
		ret = 1;
	} else
		ret = (sc->sc_head != sc->sc_tail) ? 1 : 0;

	return (ret);
}

static void
hidraw_kqdetach(struct knote *kn)
{
	struct hidraw_softc *sc;

	sc = kn->kn_hook;
	knlist_remove(&sc->sc_rsel.si_note, kn, 0);
}

static void
hidraw_notify(struct hidraw_softc *sc)
{

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	if (sc->sc_state.aslp) {
		sc->sc_state.aslp = false;
		DPRINTFN(5, "waking %p\n", &sc->sc_q);
		wakeup(&sc->sc_q);
	}
	if (sc->sc_state.sel) {
		sc->sc_state.sel = false;
		selwakeuppri(&sc->sc_rsel, PZERO);
	}
	if (sc->sc_async != NULL) {
		DPRINTFN(3, "sending SIGIO %p\n", sc->sc_async);
		PROC_LOCK(sc->sc_async);
		kern_psignal(sc->sc_async, SIGIO);
		PROC_UNLOCK(sc->sc_async);
	}
	KNOTE_LOCKED(&sc->sc_rsel.si_note, 0);
}

static device_method_t hidraw_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	hidraw_identify),
	DEVMETHOD(device_probe,		hidraw_probe),
	DEVMETHOD(device_attach,	hidraw_attach),
	DEVMETHOD(device_detach,	hidraw_detach),

	DEVMETHOD_END
};

static driver_t hidraw_driver = {
	"hidraw",
	hidraw_methods,
	sizeof(struct hidraw_softc)
};

#ifndef HIDRAW_MAKE_UHID_ALIAS
devclass_t hidraw_devclass;
#endif

DRIVER_MODULE(hidraw, hidbus, hidraw_driver, hidraw_devclass, NULL, 0);
MODULE_DEPEND(hidraw, hidbus, 1, 1, 1);
MODULE_DEPEND(hidraw, hid, 1, 1, 1);
MODULE_DEPEND(hidraw, usb, 1, 1, 1);
MODULE_VERSION(hidraw, 1);
