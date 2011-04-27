/*	$NetBSD: ucom.c,v 1.40 2001/11/13 06:24:54 lukem Exp $	*/

/*-
 * Copyright (c) 2001-2003, 2005, 2008
 *	Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
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
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
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

#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#include <sys/cons.h>
#include <sys/kdb.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#define	USB_DEBUG_VAR ucom_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>

#include <dev/usb/serial/usb_serial.h>

#include "opt_gdb.h"

SYSCTL_NODE(_hw_usb, OID_AUTO, ucom, CTLFLAG_RW, 0, "USB ucom");

#ifdef USB_DEBUG
static int ucom_debug = 0;

SYSCTL_INT(_hw_usb_ucom, OID_AUTO, debug, CTLFLAG_RW,
    &ucom_debug, 0, "ucom debug level");
#endif

#define	UCOM_CONS_BUFSIZE 1024

static uint8_t ucom_cons_rx_buf[UCOM_CONS_BUFSIZE];
static uint8_t ucom_cons_tx_buf[UCOM_CONS_BUFSIZE];

static unsigned int ucom_cons_rx_low = 0;
static unsigned int ucom_cons_rx_high = 0;

static unsigned int ucom_cons_tx_low = 0;
static unsigned int ucom_cons_tx_high = 0;

static int ucom_cons_unit = -1;
static int ucom_cons_baud = 9600;
static struct ucom_softc *ucom_cons_softc = NULL;

TUNABLE_INT("hw.usb.ucom.cons_unit", &ucom_cons_unit);
SYSCTL_INT(_hw_usb_ucom, OID_AUTO, cons_unit, CTLFLAG_RW,
    &ucom_cons_unit, 0, "console unit number");

TUNABLE_INT("hw.usb.ucom.cons_baud", &ucom_cons_baud);
SYSCTL_INT(_hw_usb_ucom, OID_AUTO, cons_baud, CTLFLAG_RW,
    &ucom_cons_baud, 0, "console baud rate");

static usb_proc_callback_t ucom_cfg_start_transfers;
static usb_proc_callback_t ucom_cfg_open;
static usb_proc_callback_t ucom_cfg_close;
static usb_proc_callback_t ucom_cfg_line_state;
static usb_proc_callback_t ucom_cfg_status_change;
static usb_proc_callback_t ucom_cfg_param;

static uint8_t	ucom_units_alloc(uint32_t, uint32_t *);
static void	ucom_units_free(uint32_t, uint32_t);
static int	ucom_attach_tty(struct ucom_softc *, uint32_t);
static void	ucom_detach_tty(struct ucom_softc *);
static void	ucom_queue_command(struct ucom_softc *,
		    usb_proc_callback_t *, struct termios *pt,
		    struct usb_proc_msg *t0, struct usb_proc_msg *t1);
static void	ucom_shutdown(struct ucom_softc *);
static void	ucom_ring(struct ucom_softc *, uint8_t);
static void	ucom_break(struct ucom_softc *, uint8_t);
static void	ucom_dtr(struct ucom_softc *, uint8_t);
static void	ucom_rts(struct ucom_softc *, uint8_t);

static tsw_open_t ucom_open;
static tsw_close_t ucom_close;
static tsw_ioctl_t ucom_ioctl;
static tsw_modem_t ucom_modem;
static tsw_param_t ucom_param;
static tsw_outwakeup_t ucom_outwakeup;
static tsw_free_t ucom_free;

static struct ttydevsw ucom_class = {
	.tsw_flags = TF_INITLOCK | TF_CALLOUT,
	.tsw_open = ucom_open,
	.tsw_close = ucom_close,
	.tsw_outwakeup = ucom_outwakeup,
	.tsw_ioctl = ucom_ioctl,
	.tsw_param = ucom_param,
	.tsw_modem = ucom_modem,
	.tsw_free = ucom_free,
};

MODULE_DEPEND(ucom, usb, 1, 1, 1);
MODULE_VERSION(ucom, 1);

#define	UCOM_UNIT_MAX 0x200		/* exclusive */
#define	UCOM_SUB_UNIT_MAX 0x100		/* exclusive */

static uint8_t ucom_bitmap[(UCOM_UNIT_MAX + 7) / 8];
static struct mtx ucom_bitmap_mtx;
MTX_SYSINIT(ucom_bitmap_mtx, &ucom_bitmap_mtx, "ucom bitmap", MTX_DEF);

static uint8_t
ucom_units_alloc(uint32_t sub_units, uint32_t *p_root_unit)
{
	uint32_t n;
	uint32_t o;
	uint32_t x;
	uint32_t max = UCOM_UNIT_MAX - (UCOM_UNIT_MAX % sub_units);
	uint8_t error = 1;

	mtx_lock(&ucom_bitmap_mtx);

	for (n = 0; n < max; n += sub_units) {

		/* check for free consecutive bits */

		for (o = 0; o < sub_units; o++) {

			x = n + o;

			if (ucom_bitmap[x / 8] & (1 << (x % 8))) {
				goto skip;
			}
		}

		/* allocate */

		for (o = 0; o < sub_units; o++) {

			x = n + o;

			ucom_bitmap[x / 8] |= (1 << (x % 8));
		}

		error = 0;

		break;

skip:		;
	}

	mtx_unlock(&ucom_bitmap_mtx);

	/*
	 * Always set the variable pointed to by "p_root_unit" so that
	 * the compiler does not think that it is used uninitialised:
	 */
	*p_root_unit = n;

	return (error);
}

static void
ucom_units_free(uint32_t root_unit, uint32_t sub_units)
{
	uint32_t x;

	mtx_lock(&ucom_bitmap_mtx);

	while (sub_units--) {
		x = root_unit + sub_units;
		ucom_bitmap[x / 8] &= ~(1 << (x % 8));
	}

	mtx_unlock(&ucom_bitmap_mtx);
}

/*
 * "N" sub_units are setup at a time. All sub-units will
 * be given sequential unit numbers. The number of
 * sub-units can be used to differentiate among
 * different types of devices.
 *
 * The mutex pointed to by "mtx" is applied before all
 * callbacks are called back. Also "mtx" must be applied
 * before calling into the ucom-layer!
 */
int
ucom_attach(struct ucom_super_softc *ssc, struct ucom_softc *sc,
    uint32_t sub_units, void *parent,
    const struct ucom_callback *callback, struct mtx *mtx)
{
	uint32_t n;
	uint32_t root_unit;
	int error = 0;

	if ((sc == NULL) ||
	    (sub_units == 0) ||
	    (sub_units > UCOM_SUB_UNIT_MAX) ||
	    (callback == NULL)) {
		return (EINVAL);
	}

	/* XXX unit management does not really belong here */
	if (ucom_units_alloc(sub_units, &root_unit)) {
		return (ENOMEM);
	}

	error = usb_proc_create(&ssc->sc_tq, mtx, "ucom", USB_PRI_MED);
	if (error) {
		ucom_units_free(root_unit, sub_units);
		return (error);
	}

	for (n = 0; n != sub_units; n++, sc++) {
		sc->sc_unit = root_unit + n;
		sc->sc_local_unit = n;
		sc->sc_super = ssc;
		sc->sc_mtx = mtx;
		sc->sc_parent = parent;
		sc->sc_callback = callback;

		error = ucom_attach_tty(sc, sub_units);
		if (error) {
			ucom_detach(ssc, sc - n, n);
			ucom_units_free(root_unit + n, sub_units - n);
			return (error);
		}
		sc->sc_flag |= UCOM_FLAG_ATTACHED;
	}
	return (0);
}

/*
 * NOTE: the following function will do nothing if
 * the structure pointed to by "ssc" and "sc" is zero.
 */
void
ucom_detach(struct ucom_super_softc *ssc, struct ucom_softc *sc,
    uint32_t sub_units)
{
	uint32_t n;

	usb_proc_drain(&ssc->sc_tq);

	for (n = 0; n != sub_units; n++, sc++) {
		if (sc->sc_flag & UCOM_FLAG_ATTACHED) {

			ucom_detach_tty(sc);

			ucom_units_free(sc->sc_unit, 1);

			/* avoid duplicate detach: */
			sc->sc_flag &= ~UCOM_FLAG_ATTACHED;
		}
	}
	usb_proc_free(&ssc->sc_tq);
}

static int
ucom_attach_tty(struct ucom_softc *sc, uint32_t sub_units)
{
	struct tty *tp;
	int error = 0;
	char buf[32];			/* temporary TTY device name buffer */

	tp = tty_alloc_mutex(&ucom_class, sc, sc->sc_mtx);
	if (tp == NULL) {
		error = ENOMEM;
		goto done;
	}
	DPRINTF("tp = %p, unit = %d\n", tp, sc->sc_unit);

	buf[0] = 0;			/* set some default value */

	/* Check if the client has a custom TTY name */
	if (sc->sc_callback->ucom_tty_name) {
		sc->sc_callback->ucom_tty_name(sc, buf,
		    sizeof(buf), sc->sc_local_unit);
	}
	if (buf[0] == 0) {
		/* Use default TTY name */
		if (sub_units > 1) {
			/* multiple modems in one */
			if (snprintf(buf, sizeof(buf), "U%u.%u",
			    sc->sc_unit - sc->sc_local_unit,
			    sc->sc_local_unit)) {
				/* ignore */
			}
		} else {
			/* single modem */
			if (snprintf(buf, sizeof(buf), "U%u", sc->sc_unit)) {
				/* ignore */
			}
		}
	}
	tty_makedev(tp, NULL, "%s", buf);

	sc->sc_tty = tp;

	DPRINTF("ttycreate: %s\n", buf);
	cv_init(&sc->sc_cv, "ucom");

	/* Check if this device should be a console */
	if ((ucom_cons_softc == NULL) && 
	    (sc->sc_unit == ucom_cons_unit)) {

		struct termios t;

		ucom_cons_softc = sc;

		memset(&t, 0, sizeof(t));
		t.c_ispeed = ucom_cons_baud;
		t.c_ospeed = t.c_ispeed;
		t.c_cflag = CS8;

		mtx_lock(ucom_cons_softc->sc_mtx);
		ucom_cons_rx_low = 0;
		ucom_cons_rx_high = 0;
		ucom_cons_tx_low = 0;
		ucom_cons_tx_high = 0;
		sc->sc_flag |= UCOM_FLAG_CONSOLE;
		ucom_open(ucom_cons_softc->sc_tty);
		ucom_param(ucom_cons_softc->sc_tty, &t);
		mtx_unlock(ucom_cons_softc->sc_mtx);
	}
done:
	return (error);
}

static void
ucom_detach_tty(struct ucom_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	DPRINTF("sc = %p, tp = %p\n", sc, sc->sc_tty);

	if (sc->sc_flag & UCOM_FLAG_CONSOLE) {
		mtx_lock(ucom_cons_softc->sc_mtx);
		ucom_close(ucom_cons_softc->sc_tty);
		mtx_unlock(ucom_cons_softc->sc_mtx);
		ucom_cons_softc = NULL;
	}

	/* the config thread has been stopped when we get here */

	mtx_lock(sc->sc_mtx);
	sc->sc_flag |= UCOM_FLAG_GONE;
	sc->sc_flag &= ~(UCOM_FLAG_HL_READY | UCOM_FLAG_LL_READY);
	mtx_unlock(sc->sc_mtx);
	if (tp) {
		tty_lock(tp);

		ucom_close(tp);	/* close, if any */

		tty_rel_gone(tp);

		mtx_lock(sc->sc_mtx);
		/* Wait for the callback after the TTY is torn down */
		while (sc->sc_ttyfreed == 0)
			cv_wait(&sc->sc_cv, sc->sc_mtx);
		/*
		 * make sure that read and write transfers are stopped
		 */
		if (sc->sc_callback->ucom_stop_read) {
			(sc->sc_callback->ucom_stop_read) (sc);
		}
		if (sc->sc_callback->ucom_stop_write) {
			(sc->sc_callback->ucom_stop_write) (sc);
		}
		mtx_unlock(sc->sc_mtx);
	}
	cv_destroy(&sc->sc_cv);
}

static void
ucom_queue_command(struct ucom_softc *sc,
    usb_proc_callback_t *fn, struct termios *pt,
    struct usb_proc_msg *t0, struct usb_proc_msg *t1)
{
	struct ucom_super_softc *ssc = sc->sc_super;
	struct ucom_param_task *task;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (usb_proc_is_gone(&ssc->sc_tq)) {
		DPRINTF("proc is gone\n");
		return;         /* nothing to do */
	}
	/* 
	 * NOTE: The task cannot get executed before we drop the
	 * "sc_mtx" mutex. It is safe to update fields in the message
	 * structure after that the message got queued.
	 */
	task = (struct ucom_param_task *)
	  usb_proc_msignal(&ssc->sc_tq, t0, t1);

	/* Setup callback and softc pointers */
	task->hdr.pm_callback = fn;
	task->sc = sc;

	/* 
	 * Make a copy of the termios. This field is only present if
	 * the "pt" field is not NULL.
	 */
	if (pt != NULL)
		task->termios_copy = *pt;

	/*
	 * Closing the device should be synchronous.
	 */
	if (fn == ucom_cfg_close)
		usb_proc_mwait(&ssc->sc_tq, t0, t1);

	/*
	 * In case of multiple configure requests,
	 * keep track of the last one!
	 */
	if (fn == ucom_cfg_start_transfers)
		sc->sc_last_start_xfer = &task->hdr;
}

static void
ucom_shutdown(struct ucom_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	DPRINTF("\n");

	/*
	 * Hang up if necessary:
	 */
	if (tp->t_termios.c_cflag & HUPCL) {
		ucom_modem(tp, 0, SER_DTR);
	}
}

/*
 * Return values:
 *    0: normal
 * else: taskqueue is draining or gone
 */
uint8_t
ucom_cfg_is_gone(struct ucom_softc *sc)
{
	struct ucom_super_softc *ssc = sc->sc_super;

	return (usb_proc_is_gone(&ssc->sc_tq));
}

static void
ucom_cfg_start_transfers(struct usb_proc_msg *_task)
{
	struct ucom_cfg_task *task = 
	    (struct ucom_cfg_task *)_task;
	struct ucom_softc *sc = task->sc;

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		/* TTY device closed */
		return;
	}

	if (_task == sc->sc_last_start_xfer)
		sc->sc_flag |= UCOM_FLAG_GP_DATA;

	if (sc->sc_callback->ucom_start_read) {
		(sc->sc_callback->ucom_start_read) (sc);
	}
	if (sc->sc_callback->ucom_start_write) {
		(sc->sc_callback->ucom_start_write) (sc);
	}
}

static void
ucom_start_transfers(struct ucom_softc *sc)
{
	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}
	/*
	 * Make sure that data transfers are started in both
	 * directions:
	 */
	if (sc->sc_callback->ucom_start_read) {
		(sc->sc_callback->ucom_start_read) (sc);
	}
	if (sc->sc_callback->ucom_start_write) {
		(sc->sc_callback->ucom_start_write) (sc);
	}
}

static void
ucom_cfg_open(struct usb_proc_msg *_task)
{
	struct ucom_cfg_task *task = 
	    (struct ucom_cfg_task *)_task;
	struct ucom_softc *sc = task->sc;

	DPRINTF("\n");

	if (sc->sc_flag & UCOM_FLAG_LL_READY) {

		/* already opened */

	} else {

		sc->sc_flag |= UCOM_FLAG_LL_READY;

		if (sc->sc_callback->ucom_cfg_open) {
			(sc->sc_callback->ucom_cfg_open) (sc);

			/* wait a little */
			usb_pause_mtx(sc->sc_mtx, hz / 10);
		}
	}
}

static int
ucom_open(struct tty *tp)
{
	struct ucom_softc *sc = tty_softc(tp);
	int error;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (sc->sc_flag & UCOM_FLAG_GONE) {
		return (ENXIO);
	}
	if (sc->sc_flag & UCOM_FLAG_HL_READY) {
		/* already opened */
		return (0);
	}
	DPRINTF("tp = %p\n", tp);

	if (sc->sc_callback->ucom_pre_open) {
		/*
		 * give the lower layer a chance to disallow TTY open, for
		 * example if the device is not present:
		 */
		error = (sc->sc_callback->ucom_pre_open) (sc);
		if (error) {
			return (error);
		}
	}
	sc->sc_flag |= UCOM_FLAG_HL_READY;

	/* Disable transfers */
	sc->sc_flag &= ~UCOM_FLAG_GP_DATA;

	sc->sc_lsr = 0;
	sc->sc_msr = 0;
	sc->sc_mcr = 0;

	/* reset programmed line state */
	sc->sc_pls_curr = 0;
	sc->sc_pls_set = 0;
	sc->sc_pls_clr = 0;

	ucom_queue_command(sc, ucom_cfg_open, NULL,
	    &sc->sc_open_task[0].hdr,
	    &sc->sc_open_task[1].hdr);

	/* Queue transfer enable command last */
	ucom_queue_command(sc, ucom_cfg_start_transfers, NULL,
	    &sc->sc_start_task[0].hdr, 
	    &sc->sc_start_task[1].hdr);

	ucom_modem(tp, SER_DTR | SER_RTS, 0);

	ucom_ring(sc, 0);

	ucom_break(sc, 0);

	ucom_status_change(sc);

	return (0);
}

static void
ucom_cfg_close(struct usb_proc_msg *_task)
{
	struct ucom_cfg_task *task = 
	    (struct ucom_cfg_task *)_task;
	struct ucom_softc *sc = task->sc;

	DPRINTF("\n");

	if (sc->sc_flag & UCOM_FLAG_LL_READY) {
		sc->sc_flag &= ~UCOM_FLAG_LL_READY;
		if (sc->sc_callback->ucom_cfg_close)
			(sc->sc_callback->ucom_cfg_close) (sc);
	} else {
		/* already closed */
	}
}

static void
ucom_close(struct tty *tp)
{
	struct ucom_softc *sc = tty_softc(tp);

	mtx_assert(sc->sc_mtx, MA_OWNED);

	DPRINTF("tp=%p\n", tp);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		DPRINTF("tp=%p already closed\n", tp);
		return;
	}
	ucom_shutdown(sc);

	ucom_queue_command(sc, ucom_cfg_close, NULL,
	    &sc->sc_close_task[0].hdr,
	    &sc->sc_close_task[1].hdr);

	sc->sc_flag &= ~(UCOM_FLAG_HL_READY | UCOM_FLAG_RTS_IFLOW);

	if (sc->sc_callback->ucom_stop_read) {
		(sc->sc_callback->ucom_stop_read) (sc);
	}
}

static int
ucom_ioctl(struct tty *tp, u_long cmd, caddr_t data, struct thread *td)
{
	struct ucom_softc *sc = tty_softc(tp);
	int error;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return (EIO);
	}
	DPRINTF("cmd = 0x%08lx\n", cmd);

	switch (cmd) {
#if 0
	case TIOCSRING:
		ucom_ring(sc, 1);
		error = 0;
		break;
	case TIOCCRING:
		ucom_ring(sc, 0);
		error = 0;
		break;
#endif
	case TIOCSBRK:
		ucom_break(sc, 1);
		error = 0;
		break;
	case TIOCCBRK:
		ucom_break(sc, 0);
		error = 0;
		break;
	default:
		if (sc->sc_callback->ucom_ioctl) {
			error = (sc->sc_callback->ucom_ioctl)
			    (sc, cmd, data, 0, td);
		} else {
			error = ENOIOCTL;
		}
		break;
	}
	return (error);
}

static int
ucom_modem(struct tty *tp, int sigon, int sigoff)
{
	struct ucom_softc *sc = tty_softc(tp);
	uint8_t onoff;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return (0);
	}
	if ((sigon == 0) && (sigoff == 0)) {

		if (sc->sc_mcr & SER_DTR) {
			sigon |= SER_DTR;
		}
		if (sc->sc_mcr & SER_RTS) {
			sigon |= SER_RTS;
		}
		if (sc->sc_msr & SER_CTS) {
			sigon |= SER_CTS;
		}
		if (sc->sc_msr & SER_DCD) {
			sigon |= SER_DCD;
		}
		if (sc->sc_msr & SER_DSR) {
			sigon |= SER_DSR;
		}
		if (sc->sc_msr & SER_RI) {
			sigon |= SER_RI;
		}
		return (sigon);
	}
	if (sigon & SER_DTR) {
		sc->sc_mcr |= SER_DTR;
	}
	if (sigoff & SER_DTR) {
		sc->sc_mcr &= ~SER_DTR;
	}
	if (sigon & SER_RTS) {
		sc->sc_mcr |= SER_RTS;
	}
	if (sigoff & SER_RTS) {
		sc->sc_mcr &= ~SER_RTS;
	}
	onoff = (sc->sc_mcr & SER_DTR) ? 1 : 0;
	ucom_dtr(sc, onoff);

	onoff = (sc->sc_mcr & SER_RTS) ? 1 : 0;
	ucom_rts(sc, onoff);

	return (0);
}

static void
ucom_cfg_line_state(struct usb_proc_msg *_task)
{
	struct ucom_cfg_task *task = 
	    (struct ucom_cfg_task *)_task;
	struct ucom_softc *sc = task->sc;
	uint8_t notch_bits;
	uint8_t any_bits;
	uint8_t prev_value;
	uint8_t last_value;
	uint8_t mask;

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}

	mask = 0;
	/* compute callback mask */
	if (sc->sc_callback->ucom_cfg_set_dtr)
		mask |= UCOM_LS_DTR;
	if (sc->sc_callback->ucom_cfg_set_rts)
		mask |= UCOM_LS_RTS;
	if (sc->sc_callback->ucom_cfg_set_break)
		mask |= UCOM_LS_BREAK;
	if (sc->sc_callback->ucom_cfg_set_ring)
		mask |= UCOM_LS_RING;

	/* compute the bits we are to program */
	notch_bits = (sc->sc_pls_set & sc->sc_pls_clr) & mask;
	any_bits = (sc->sc_pls_set | sc->sc_pls_clr) & mask;
	prev_value = sc->sc_pls_curr ^ notch_bits;
	last_value = sc->sc_pls_curr;

	/* reset programmed line state */
	sc->sc_pls_curr = 0;
	sc->sc_pls_set = 0;
	sc->sc_pls_clr = 0;

	/* ensure that we don't loose any levels */
	if (notch_bits & UCOM_LS_DTR)
		sc->sc_callback->ucom_cfg_set_dtr(sc,
		    (prev_value & UCOM_LS_DTR) ? 1 : 0);
	if (notch_bits & UCOM_LS_RTS)
		sc->sc_callback->ucom_cfg_set_rts(sc,
		    (prev_value & UCOM_LS_RTS) ? 1 : 0);
	if (notch_bits & UCOM_LS_BREAK)
		sc->sc_callback->ucom_cfg_set_break(sc,
		    (prev_value & UCOM_LS_BREAK) ? 1 : 0);
	if (notch_bits & UCOM_LS_RING)
		sc->sc_callback->ucom_cfg_set_ring(sc,
		    (prev_value & UCOM_LS_RING) ? 1 : 0);

	/* set last value */
	if (any_bits & UCOM_LS_DTR)
		sc->sc_callback->ucom_cfg_set_dtr(sc,
		    (last_value & UCOM_LS_DTR) ? 1 : 0);
	if (any_bits & UCOM_LS_RTS)
		sc->sc_callback->ucom_cfg_set_rts(sc,
		    (last_value & UCOM_LS_RTS) ? 1 : 0);
	if (any_bits & UCOM_LS_BREAK)
		sc->sc_callback->ucom_cfg_set_break(sc,
		    (last_value & UCOM_LS_BREAK) ? 1 : 0);
	if (any_bits & UCOM_LS_RING)
		sc->sc_callback->ucom_cfg_set_ring(sc,
		    (last_value & UCOM_LS_RING) ? 1 : 0);
}

static void
ucom_line_state(struct ucom_softc *sc,
    uint8_t set_bits, uint8_t clear_bits)
{
	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}

	DPRINTF("on=0x%02x, off=0x%02x\n", set_bits, clear_bits);

	/* update current programmed line state */
	sc->sc_pls_curr |= set_bits;
	sc->sc_pls_curr &= ~clear_bits;
	sc->sc_pls_set |= set_bits;
	sc->sc_pls_clr |= clear_bits;

	/* defer driver programming */
	ucom_queue_command(sc, ucom_cfg_line_state, NULL,
	    &sc->sc_line_state_task[0].hdr, 
	    &sc->sc_line_state_task[1].hdr);
}

static void
ucom_ring(struct ucom_softc *sc, uint8_t onoff)
{
	DPRINTF("onoff = %d\n", onoff);

	if (onoff)
		ucom_line_state(sc, UCOM_LS_RING, 0);
	else
		ucom_line_state(sc, 0, UCOM_LS_RING);
}

static void
ucom_break(struct ucom_softc *sc, uint8_t onoff)
{
	DPRINTF("onoff = %d\n", onoff);

	if (onoff)
		ucom_line_state(sc, UCOM_LS_BREAK, 0);
	else
		ucom_line_state(sc, 0, UCOM_LS_BREAK);
}

static void
ucom_dtr(struct ucom_softc *sc, uint8_t onoff)
{
	DPRINTF("onoff = %d\n", onoff);

	if (onoff)
		ucom_line_state(sc, UCOM_LS_DTR, 0);
	else
		ucom_line_state(sc, 0, UCOM_LS_DTR);
}

static void
ucom_rts(struct ucom_softc *sc, uint8_t onoff)
{
	DPRINTF("onoff = %d\n", onoff);

	if (onoff)
		ucom_line_state(sc, UCOM_LS_RTS, 0);
	else
		ucom_line_state(sc, 0, UCOM_LS_RTS);
}

static void
ucom_cfg_status_change(struct usb_proc_msg *_task)
{
	struct ucom_cfg_task *task = 
	    (struct ucom_cfg_task *)_task;
	struct ucom_softc *sc = task->sc;
	struct tty *tp;
	uint8_t new_msr;
	uint8_t new_lsr;
	uint8_t onoff;
	uint8_t lsr_delta;

	tp = sc->sc_tty;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	if (sc->sc_callback->ucom_cfg_get_status == NULL) {
		return;
	}
	/* get status */

	new_msr = 0;
	new_lsr = 0;

	(sc->sc_callback->ucom_cfg_get_status) (sc, &new_lsr, &new_msr);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		/* TTY device closed */
		return;
	}
	onoff = ((sc->sc_msr ^ new_msr) & SER_DCD);
	lsr_delta = (sc->sc_lsr ^ new_lsr);

	sc->sc_msr = new_msr;
	sc->sc_lsr = new_lsr;

	if (onoff) {

		onoff = (sc->sc_msr & SER_DCD) ? 1 : 0;

		DPRINTF("DCD changed to %d\n", onoff);

		ttydisc_modem(tp, onoff);
	}

	if ((lsr_delta & ULSR_BI) && (sc->sc_lsr & ULSR_BI)) {

		DPRINTF("BREAK detected\n");

		ttydisc_rint(tp, 0, TRE_BREAK);
		ttydisc_rint_done(tp);
	}

	if ((lsr_delta & ULSR_FE) && (sc->sc_lsr & ULSR_FE)) {

		DPRINTF("Frame error detected\n");

		ttydisc_rint(tp, 0, TRE_FRAMING);
		ttydisc_rint_done(tp);
	}

	if ((lsr_delta & ULSR_PE) && (sc->sc_lsr & ULSR_PE)) {

		DPRINTF("Parity error detected\n");

		ttydisc_rint(tp, 0, TRE_PARITY);
		ttydisc_rint_done(tp);
	}
}

void
ucom_status_change(struct ucom_softc *sc)
{
	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (sc->sc_flag & UCOM_FLAG_CONSOLE)
		return;		/* not supported */

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}
	DPRINTF("\n");

	ucom_queue_command(sc, ucom_cfg_status_change, NULL,
	    &sc->sc_status_task[0].hdr,
	    &sc->sc_status_task[1].hdr);
}

static void
ucom_cfg_param(struct usb_proc_msg *_task)
{
	struct ucom_param_task *task = 
	    (struct ucom_param_task *)_task;
	struct ucom_softc *sc = task->sc;

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	if (sc->sc_callback->ucom_cfg_param == NULL) {
		return;
	}

	(sc->sc_callback->ucom_cfg_param) (sc, &task->termios_copy);

	/* wait a little */
	usb_pause_mtx(sc->sc_mtx, hz / 10);
}

static int
ucom_param(struct tty *tp, struct termios *t)
{
	struct ucom_softc *sc = tty_softc(tp);
	uint8_t opened;
	int error;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	opened = 0;
	error = 0;

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {

		/* XXX the TTY layer should call "open()" first! */

		error = ucom_open(tp);
		if (error) {
			goto done;
		}
		opened = 1;
	}
	DPRINTF("sc = %p\n", sc);

	/* Check requested parameters. */
	if (t->c_ospeed < 0) {
		DPRINTF("negative ospeed\n");
		error = EINVAL;
		goto done;
	}
	if (t->c_ispeed && (t->c_ispeed != t->c_ospeed)) {
		DPRINTF("mismatch ispeed and ospeed\n");
		error = EINVAL;
		goto done;
	}
	t->c_ispeed = t->c_ospeed;

	if (sc->sc_callback->ucom_pre_param) {
		/* Let the lower layer verify the parameters */
		error = (sc->sc_callback->ucom_pre_param) (sc, t);
		if (error) {
			DPRINTF("callback error = %d\n", error);
			goto done;
		}
	}

	/* Disable transfers */
	sc->sc_flag &= ~UCOM_FLAG_GP_DATA;

	/* Queue baud rate programming command first */
	ucom_queue_command(sc, ucom_cfg_param, t,
	    &sc->sc_param_task[0].hdr,
	    &sc->sc_param_task[1].hdr);

	/* Queue transfer enable command last */
	ucom_queue_command(sc, ucom_cfg_start_transfers, NULL,
	    &sc->sc_start_task[0].hdr, 
	    &sc->sc_start_task[1].hdr);

	if (t->c_cflag & CRTS_IFLOW) {
		sc->sc_flag |= UCOM_FLAG_RTS_IFLOW;
	} else if (sc->sc_flag & UCOM_FLAG_RTS_IFLOW) {
		sc->sc_flag &= ~UCOM_FLAG_RTS_IFLOW;
		ucom_modem(tp, SER_RTS, 0);
	}
done:
	if (error) {
		if (opened) {
			ucom_close(tp);
		}
	}
	return (error);
}

static void
ucom_outwakeup(struct tty *tp)
{
	struct ucom_softc *sc = tty_softc(tp);

	mtx_assert(sc->sc_mtx, MA_OWNED);

	DPRINTF("sc = %p\n", sc);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		/* The higher layer is not ready */
		return;
	}
	ucom_start_transfers(sc);
}

/*------------------------------------------------------------------------*
 *	ucom_get_data
 *
 * Return values:
 * 0: No data is available.
 * Else: Data is available.
 *------------------------------------------------------------------------*/
uint8_t
ucom_get_data(struct ucom_softc *sc, struct usb_page_cache *pc,
    uint32_t offset, uint32_t len, uint32_t *actlen)
{
	struct usb_page_search res;
	struct tty *tp = sc->sc_tty;
	uint32_t cnt;
	uint32_t offset_orig;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (sc->sc_flag & UCOM_FLAG_CONSOLE) {
		unsigned int temp;

		/* get total TX length */

		temp = ucom_cons_tx_high - ucom_cons_tx_low;
		temp %= UCOM_CONS_BUFSIZE;

		/* limit TX length */

		if (temp > (UCOM_CONS_BUFSIZE - ucom_cons_tx_low))
			temp = (UCOM_CONS_BUFSIZE - ucom_cons_tx_low);

		if (temp > len)
			temp = len;

		/* copy in data */

		usbd_copy_in(pc, offset, ucom_cons_tx_buf + ucom_cons_tx_low, temp);

		/* update counters */

		ucom_cons_tx_low += temp;
		ucom_cons_tx_low %= UCOM_CONS_BUFSIZE;

		/* store actual length */

		*actlen = temp;

		return (temp ? 1 : 0);
	}

	if (tty_gone(tp) ||
	    !(sc->sc_flag & UCOM_FLAG_GP_DATA)) {
		actlen[0] = 0;
		return (0);		/* multiport device polling */
	}
	offset_orig = offset;

	while (len != 0) {

		usbd_get_page(pc, offset, &res);

		if (res.length > len) {
			res.length = len;
		}
		/* copy data directly into USB buffer */
		cnt = ttydisc_getc(tp, res.buffer, res.length);

		offset += cnt;
		len -= cnt;

		if (cnt < res.length) {
			/* end of buffer */
			break;
		}
	}

	actlen[0] = offset - offset_orig;

	DPRINTF("cnt=%d\n", actlen[0]);

	if (actlen[0] == 0) {
		return (0);
	}
	return (1);
}

void
ucom_put_data(struct ucom_softc *sc, struct usb_page_cache *pc,
    uint32_t offset, uint32_t len)
{
	struct usb_page_search res;
	struct tty *tp = sc->sc_tty;
	char *buf;
	uint32_t cnt;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (sc->sc_flag & UCOM_FLAG_CONSOLE) {
		unsigned int temp;

		/* get maximum RX length */

		temp = (UCOM_CONS_BUFSIZE - 1) - ucom_cons_rx_high + ucom_cons_rx_low;
		temp %= UCOM_CONS_BUFSIZE;

		/* limit RX length */

		if (temp > (UCOM_CONS_BUFSIZE - ucom_cons_rx_high))
			temp = (UCOM_CONS_BUFSIZE - ucom_cons_rx_high);

		if (temp > len)
			temp = len;

		/* copy out data */

		usbd_copy_out(pc, offset, ucom_cons_rx_buf + ucom_cons_rx_high, temp);

		/* update counters */

		ucom_cons_rx_high += temp;
		ucom_cons_rx_high %= UCOM_CONS_BUFSIZE;

		return;
	}

	if (tty_gone(tp))
		return;			/* multiport device polling */

	if (len == 0)
		return;			/* no data */

	/* set a flag to prevent recursation ? */

	while (len > 0) {

		usbd_get_page(pc, offset, &res);

		if (res.length > len) {
			res.length = len;
		}
		len -= res.length;
		offset += res.length;

		/* pass characters to tty layer */

		buf = res.buffer;
		cnt = res.length;

		/* first check if we can pass the buffer directly */

		if (ttydisc_can_bypass(tp)) {
			if (ttydisc_rint_bypass(tp, buf, cnt) != cnt) {
				DPRINTF("tp=%p, data lost\n", tp);
			}
			continue;
		}
		/* need to loop */

		for (cnt = 0; cnt != res.length; cnt++) {
			if (ttydisc_rint(tp, buf[cnt], 0) == -1) {
				/* XXX what should we do? */

				DPRINTF("tp=%p, lost %d "
				    "chars\n", tp, res.length - cnt);
				break;
			}
		}
	}
	ttydisc_rint_done(tp);
}

static void
ucom_free(void *xsc)
{
	struct ucom_softc *sc = xsc;

	mtx_lock(sc->sc_mtx);
	sc->sc_ttyfreed = 1;
	cv_signal(&sc->sc_cv);
	mtx_unlock(sc->sc_mtx);
}

static cn_probe_t ucom_cnprobe;
static cn_init_t ucom_cninit;
static cn_term_t ucom_cnterm;
static cn_getc_t ucom_cngetc;
static cn_putc_t ucom_cnputc;

CONSOLE_DRIVER(ucom);

static void
ucom_cnprobe(struct consdev  *cp)
{
	if (ucom_cons_unit != -1)
		cp->cn_pri = CN_NORMAL;
	else
		cp->cn_pri = CN_DEAD;

	strlcpy(cp->cn_name, "ucom", sizeof(cp->cn_name));
}

static void
ucom_cninit(struct consdev  *cp)
{
}

static void
ucom_cnterm(struct consdev  *cp)
{
}

static int
ucom_cngetc(struct consdev *cd)
{
	struct ucom_softc *sc = ucom_cons_softc;
	int c;

	if (sc == NULL)
		return (-1);

	mtx_lock(sc->sc_mtx);

	if (ucom_cons_rx_low != ucom_cons_rx_high) {
		c = ucom_cons_rx_buf[ucom_cons_rx_low];
		ucom_cons_rx_low ++;
		ucom_cons_rx_low %= UCOM_CONS_BUFSIZE;
	} else {
		c = -1;
	}

	/* start USB transfers */
	ucom_outwakeup(sc->sc_tty);

	mtx_unlock(sc->sc_mtx);

	/* poll if necessary */
	if (kdb_active && sc->sc_callback->ucom_poll)
		(sc->sc_callback->ucom_poll) (sc);

	return (c);
}

static void
ucom_cnputc(struct consdev *cd, int c)
{
	struct ucom_softc *sc = ucom_cons_softc;
	unsigned int temp;

	if (sc == NULL)
		return;

 repeat:

	mtx_lock(sc->sc_mtx);

	/* compute maximum TX length */

	temp = (UCOM_CONS_BUFSIZE - 1) - ucom_cons_tx_high + ucom_cons_tx_low;
	temp %= UCOM_CONS_BUFSIZE;

	if (temp) {
		ucom_cons_tx_buf[ucom_cons_tx_high] = c;
		ucom_cons_tx_high ++;
		ucom_cons_tx_high %= UCOM_CONS_BUFSIZE;
	}

	/* start USB transfers */
	ucom_outwakeup(sc->sc_tty);

	mtx_unlock(sc->sc_mtx);

	/* poll if necessary */
	if (kdb_active && sc->sc_callback->ucom_poll) {
		(sc->sc_callback->ucom_poll) (sc);
		/* simple flow control */
		if (temp == 0)
			goto repeat;
	}
}

#if defined(GDB)

#include <gdb/gdb.h>

static gdb_probe_f ucom_gdbprobe;
static gdb_init_f ucom_gdbinit;
static gdb_term_f ucom_gdbterm;
static gdb_getc_f ucom_gdbgetc;
static gdb_putc_f ucom_gdbputc;

GDB_DBGPORT(sio, ucom_gdbprobe, ucom_gdbinit, ucom_gdbterm, ucom_gdbgetc, ucom_gdbputc);

static int
ucom_gdbprobe(void)
{
	return ((ucom_cons_softc != NULL) ? 0 : -1);
}

static void
ucom_gdbinit(void)
{
}

static void
ucom_gdbterm(void)
{
}

static void
ucom_gdbputc(int c)
{
        ucom_cnputc(NULL, c);
}

static int
ucom_gdbgetc(void)
{
        return (ucom_cngetc(NULL));
}

#endif
