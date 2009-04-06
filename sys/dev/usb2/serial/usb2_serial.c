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

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>
#include <dev/usb2/include/usb2_ioctl.h>

#define	USB_DEBUG_VAR usb2_com_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/serial/usb2_serial.h>

#if USB_DEBUG
static int usb2_com_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, ucom, CTLFLAG_RW, 0, "USB ucom");
SYSCTL_INT(_hw_usb2_ucom, OID_AUTO, debug, CTLFLAG_RW,
    &usb2_com_debug, 0, "ucom debug level");
#endif

static usb2_proc_callback_t usb2_com_cfg_start_transfers;
static usb2_proc_callback_t usb2_com_cfg_open;
static usb2_proc_callback_t usb2_com_cfg_close;
static usb2_proc_callback_t usb2_com_cfg_line_state;
static usb2_proc_callback_t usb2_com_cfg_status_change;
static usb2_proc_callback_t usb2_com_cfg_param;

static uint8_t	usb2_com_units_alloc(uint32_t, uint32_t *);
static void	usb2_com_units_free(uint32_t, uint32_t);
static int	usb2_com_attach_tty(struct usb2_com_softc *, uint32_t);
static void	usb2_com_detach_tty(struct usb2_com_softc *);
static void	usb2_com_queue_command(struct usb2_com_softc *,
		    usb2_proc_callback_t *, struct termios *pt,
		    struct usb2_proc_msg *t0, struct usb2_proc_msg *t1);
static void	usb2_com_shutdown(struct usb2_com_softc *);
static void	usb2_com_break(struct usb2_com_softc *, uint8_t);
static void	usb2_com_dtr(struct usb2_com_softc *, uint8_t);
static void	usb2_com_rts(struct usb2_com_softc *, uint8_t);

static tsw_open_t usb2_com_open;
static tsw_close_t usb2_com_close;
static tsw_ioctl_t usb2_com_ioctl;
static tsw_modem_t usb2_com_modem;
static tsw_param_t usb2_com_param;
static tsw_outwakeup_t usb2_com_outwakeup;
static tsw_free_t usb2_com_free;

static struct ttydevsw usb2_com_class = {
	.tsw_flags = TF_INITLOCK | TF_CALLOUT,
	.tsw_open = usb2_com_open,
	.tsw_close = usb2_com_close,
	.tsw_outwakeup = usb2_com_outwakeup,
	.tsw_ioctl = usb2_com_ioctl,
	.tsw_param = usb2_com_param,
	.tsw_modem = usb2_com_modem,
	.tsw_free = usb2_com_free,
};

MODULE_DEPEND(usb2_serial, usb2_core, 1, 1, 1);
MODULE_VERSION(usb2_serial, 1);

#define	UCOM_UNIT_MAX 0x1000		/* exclusive */
#define	UCOM_SUB_UNIT_MAX 0x100		/* exclusive */

static uint8_t usb2_com_bitmap[(UCOM_UNIT_MAX + 7) / 8];

static uint8_t
usb2_com_units_alloc(uint32_t sub_units, uint32_t *p_root_unit)
{
	uint32_t n;
	uint32_t o;
	uint32_t x;
	uint32_t max = UCOM_UNIT_MAX - (UCOM_UNIT_MAX % sub_units);
	uint8_t error = 1;

	mtx_lock(&Giant);

	for (n = 0; n < max; n += sub_units) {

		/* check for free consecutive bits */

		for (o = 0; o < sub_units; o++) {

			x = n + o;

			if (usb2_com_bitmap[x / 8] & (1 << (x % 8))) {
				goto skip;
			}
		}

		/* allocate */

		for (o = 0; o < sub_units; o++) {

			x = n + o;

			usb2_com_bitmap[x / 8] |= (1 << (x % 8));
		}

		error = 0;

		break;

skip:		;
	}

	mtx_unlock(&Giant);

	/*
	 * Always set the variable pointed to by "p_root_unit" so that
	 * the compiler does not think that it is used uninitialised:
	 */
	*p_root_unit = n;

	return (error);
}

static void
usb2_com_units_free(uint32_t root_unit, uint32_t sub_units)
{
	uint32_t x;

	mtx_lock(&Giant);

	while (sub_units--) {
		x = root_unit + sub_units;
		usb2_com_bitmap[x / 8] &= ~(1 << (x % 8));
	}

	mtx_unlock(&Giant);
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
usb2_com_attach(struct usb2_com_super_softc *ssc, struct usb2_com_softc *sc,
    uint32_t sub_units, void *parent,
    const struct usb2_com_callback *callback, struct mtx *mtx)
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
	if (usb2_com_units_alloc(sub_units, &root_unit)) {
		return (ENOMEM);
	}

	error = usb2_proc_create(&ssc->sc_tq, mtx, "ucom", USB_PRI_MED);
	if (error) {
		usb2_com_units_free(root_unit, sub_units);
		return (error);
	}

	for (n = 0; n != sub_units; n++, sc++) {
		sc->sc_unit = root_unit + n;
		sc->sc_local_unit = n;
		sc->sc_super = ssc;
		sc->sc_mtx = mtx;
		sc->sc_parent = parent;
		sc->sc_callback = callback;

		error = usb2_com_attach_tty(sc, sub_units);
		if (error) {
			usb2_com_detach(ssc, sc - n, n);
			usb2_com_units_free(root_unit + n, sub_units - n);
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
usb2_com_detach(struct usb2_com_super_softc *ssc, struct usb2_com_softc *sc,
    uint32_t sub_units)
{
	uint32_t n;

	usb2_proc_drain(&ssc->sc_tq);

	for (n = 0; n != sub_units; n++, sc++) {
		if (sc->sc_flag & UCOM_FLAG_ATTACHED) {

			usb2_com_detach_tty(sc);

			usb2_com_units_free(sc->sc_unit, 1);

			/* avoid duplicate detach: */
			sc->sc_flag &= ~UCOM_FLAG_ATTACHED;
		}
	}
	usb2_proc_free(&ssc->sc_tq);
}

static int
usb2_com_attach_tty(struct usb2_com_softc *sc, uint32_t sub_units)
{
	struct tty *tp;
	int error = 0;
	char buf[32];			/* temporary TTY device name buffer */

	tp = tty_alloc(&usb2_com_class, sc, sc->sc_mtx);
	if (tp == NULL) {
		error = ENOMEM;
		goto done;
	}
	DPRINTF("tp = %p, unit = %d\n", tp, sc->sc_unit);

	buf[0] = 0;			/* set some default value */

	/* Check if the client has a custom TTY name */
	if (sc->sc_callback->usb2_com_tty_name) {
		sc->sc_callback->usb2_com_tty_name(sc, buf,
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
	usb2_cv_init(&sc->sc_cv, "usb2_com");

done:
	return (error);
}

static void
usb2_com_detach_tty(struct usb2_com_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	DPRINTF("sc = %p, tp = %p\n", sc, sc->sc_tty);

	/* the config thread has been stopped when we get here */

	mtx_lock(sc->sc_mtx);
	sc->sc_flag |= UCOM_FLAG_GONE;
	sc->sc_flag &= ~(UCOM_FLAG_HL_READY |
	    UCOM_FLAG_LL_READY);
	mtx_unlock(sc->sc_mtx);
	if (tp) {
		tty_lock(tp);

		usb2_com_close(tp);	/* close, if any */

		tty_rel_gone(tp);

		mtx_lock(sc->sc_mtx);
		/* Wait for the callback after the TTY is torn down */
		while (sc->sc_ttyfreed == 0)
			usb2_cv_wait(&sc->sc_cv, sc->sc_mtx);
		/*
		 * make sure that read and write transfers are stopped
		 */
		if (sc->sc_callback->usb2_com_stop_read) {
			(sc->sc_callback->usb2_com_stop_read) (sc);
		}
		if (sc->sc_callback->usb2_com_stop_write) {
			(sc->sc_callback->usb2_com_stop_write) (sc);
		}
		mtx_unlock(sc->sc_mtx);
	}
	usb2_cv_destroy(&sc->sc_cv);
}

static void
usb2_com_queue_command(struct usb2_com_softc *sc,
    usb2_proc_callback_t *fn, struct termios *pt,
    struct usb2_proc_msg *t0, struct usb2_proc_msg *t1)
{
	struct usb2_com_super_softc *ssc = sc->sc_super;
	struct usb2_com_param_task *task;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (usb2_proc_is_gone(&ssc->sc_tq)) {
		DPRINTF("proc is gone\n");
		return;         /* nothing to do */
	}
	/* 
	 * NOTE: The task cannot get executed before we drop the
	 * "sc_mtx" mutex. It is safe to update fields in the message
	 * structure after that the message got queued.
	 */
	task = (struct usb2_com_param_task *)
	  usb2_proc_msignal(&ssc->sc_tq, t0, t1);

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
	if (fn == usb2_com_cfg_close)
		usb2_proc_mwait(&ssc->sc_tq, t0, t1);

}

static void
usb2_com_shutdown(struct usb2_com_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	DPRINTF("\n");

	/*
	 * Hang up if necessary:
	 */
	if (tp->t_termios.c_cflag & HUPCL) {
		usb2_com_modem(tp, 0, SER_DTR);
	}
}

/*
 * Return values:
 *    0: normal
 * else: taskqueue is draining or gone
 */
uint8_t
usb2_com_cfg_is_gone(struct usb2_com_softc *sc)
{
	struct usb2_com_super_softc *ssc = sc->sc_super;

	return (usb2_proc_is_gone(&ssc->sc_tq));
}

static void
usb2_com_cfg_start_transfers(struct usb2_proc_msg *_task)
{
	struct usb2_com_cfg_task *task = 
	    (struct usb2_com_cfg_task *)_task;
	struct usb2_com_softc *sc = task->sc;

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		/* TTY device closed */
		return;
	}
	sc->sc_flag |= UCOM_FLAG_GP_DATA;

	if (sc->sc_callback->usb2_com_start_read) {
		(sc->sc_callback->usb2_com_start_read) (sc);
	}
	if (sc->sc_callback->usb2_com_start_write) {
		(sc->sc_callback->usb2_com_start_write) (sc);
	}
}

static void
usb2_com_start_transfers(struct usb2_com_softc *sc)
{
	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}
	/*
	 * Make sure that data transfers are started in both
	 * directions:
	 */
	if (sc->sc_callback->usb2_com_start_read) {
		(sc->sc_callback->usb2_com_start_read) (sc);
	}
	if (sc->sc_callback->usb2_com_start_write) {
		(sc->sc_callback->usb2_com_start_write) (sc);
	}
}

static void
usb2_com_cfg_open(struct usb2_proc_msg *_task)
{
	struct usb2_com_cfg_task *task = 
	    (struct usb2_com_cfg_task *)_task;
	struct usb2_com_softc *sc = task->sc;

	DPRINTF("\n");

	if (sc->sc_flag & UCOM_FLAG_LL_READY) {

		/* already opened */

	} else {

		sc->sc_flag |= UCOM_FLAG_LL_READY;

		if (sc->sc_callback->usb2_com_cfg_open) {
			(sc->sc_callback->usb2_com_cfg_open) (sc);

			/* wait a little */
			usb2_pause_mtx(sc->sc_mtx, hz / 10);
		}
	}
}

static int
usb2_com_open(struct tty *tp)
{
	struct usb2_com_softc *sc = tty_softc(tp);
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

	if (sc->sc_callback->usb2_com_pre_open) {
		/*
		 * give the lower layer a chance to disallow TTY open, for
		 * example if the device is not present:
		 */
		error = (sc->sc_callback->usb2_com_pre_open) (sc);
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

	usb2_com_queue_command(sc, usb2_com_cfg_open, NULL,
	    &sc->sc_open_task[0].hdr,
	    &sc->sc_open_task[1].hdr);

	/* Queue transfer enable command last */
	usb2_com_queue_command(sc, usb2_com_cfg_start_transfers, NULL,
	    &sc->sc_start_task[0].hdr, 
	    &sc->sc_start_task[1].hdr);

	usb2_com_modem(tp, SER_DTR | SER_RTS, 0);

	usb2_com_break(sc, 0);

	usb2_com_status_change(sc);

	return (0);
}

static void
usb2_com_cfg_close(struct usb2_proc_msg *_task)
{
	struct usb2_com_cfg_task *task = 
	    (struct usb2_com_cfg_task *)_task;
	struct usb2_com_softc *sc = task->sc;

	DPRINTF("\n");

	if (sc->sc_flag & UCOM_FLAG_LL_READY) {

		sc->sc_flag &= ~(UCOM_FLAG_LL_READY |
		    UCOM_FLAG_GP_DATA);

		if (sc->sc_callback->usb2_com_cfg_close) {
			(sc->sc_callback->usb2_com_cfg_close) (sc);
		}
	} else {
		/* already closed */
	}
}

static void
usb2_com_close(struct tty *tp)
{
	struct usb2_com_softc *sc = tty_softc(tp);

	mtx_assert(sc->sc_mtx, MA_OWNED);

	DPRINTF("tp=%p\n", tp);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		DPRINTF("tp=%p already closed\n", tp);
		return;
	}
	usb2_com_shutdown(sc);

	usb2_com_queue_command(sc, usb2_com_cfg_close, NULL,
	    &sc->sc_close_task[0].hdr,
	    &sc->sc_close_task[1].hdr);

	sc->sc_flag &= ~(UCOM_FLAG_HL_READY |
	    UCOM_FLAG_WR_START |
	    UCOM_FLAG_RTS_IFLOW);

	if (sc->sc_callback->usb2_com_stop_read) {
		(sc->sc_callback->usb2_com_stop_read) (sc);
	}
	if (sc->sc_callback->usb2_com_stop_write) {
		(sc->sc_callback->usb2_com_stop_write) (sc);
	}
}

static int
usb2_com_ioctl(struct tty *tp, u_long cmd, caddr_t data, struct thread *td)
{
	struct usb2_com_softc *sc = tty_softc(tp);
	int error;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return (EIO);
	}
	DPRINTF("cmd = 0x%08lx\n", cmd);

	switch (cmd) {
	case TIOCSBRK:
		usb2_com_break(sc, 1);
		error = 0;
		break;
	case TIOCCBRK:
		usb2_com_break(sc, 0);
		error = 0;
		break;
	default:
		if (sc->sc_callback->usb2_com_ioctl) {
			error = (sc->sc_callback->usb2_com_ioctl)
			    (sc, cmd, data, 0, td);
		} else {
			error = ENOIOCTL;
		}
		break;
	}
	return (error);
}

static int
usb2_com_modem(struct tty *tp, int sigon, int sigoff)
{
	struct usb2_com_softc *sc = tty_softc(tp);
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
	usb2_com_dtr(sc, onoff);

	onoff = (sc->sc_mcr & SER_RTS) ? 1 : 0;
	usb2_com_rts(sc, onoff);

	return (0);
}

static void
usb2_com_cfg_line_state(struct usb2_proc_msg *_task)
{
	struct usb2_com_cfg_task *task = 
	    (struct usb2_com_cfg_task *)_task;
	struct usb2_com_softc *sc = task->sc;
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
	if (sc->sc_callback->usb2_com_cfg_set_dtr)
		mask |= UCOM_LS_DTR;
	if (sc->sc_callback->usb2_com_cfg_set_rts)
		mask |= UCOM_LS_RTS;
	if (sc->sc_callback->usb2_com_cfg_set_break)
		mask |= UCOM_LS_BREAK;

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
		sc->sc_callback->usb2_com_cfg_set_dtr(sc,
		    (prev_value & UCOM_LS_DTR) ? 1 : 0);
	if (notch_bits & UCOM_LS_RTS)
		sc->sc_callback->usb2_com_cfg_set_rts(sc,
		    (prev_value & UCOM_LS_RTS) ? 1 : 0);
	if (notch_bits & UCOM_LS_BREAK)
		sc->sc_callback->usb2_com_cfg_set_break(sc,
		    (prev_value & UCOM_LS_BREAK) ? 1 : 0);

	/* set last value */
	if (any_bits & UCOM_LS_DTR)
		sc->sc_callback->usb2_com_cfg_set_dtr(sc,
		    (last_value & UCOM_LS_DTR) ? 1 : 0);
	if (any_bits & UCOM_LS_RTS)
		sc->sc_callback->usb2_com_cfg_set_rts(sc,
		    (last_value & UCOM_LS_RTS) ? 1 : 0);
	if (any_bits & UCOM_LS_BREAK)
		sc->sc_callback->usb2_com_cfg_set_break(sc,
		    (last_value & UCOM_LS_BREAK) ? 1 : 0);
}

static void
usb2_com_line_state(struct usb2_com_softc *sc,
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
	usb2_com_queue_command(sc, usb2_com_cfg_line_state, NULL,
	    &sc->sc_line_state_task[0].hdr, 
	    &sc->sc_line_state_task[1].hdr);
}

static void
usb2_com_break(struct usb2_com_softc *sc, uint8_t onoff)
{
	DPRINTF("onoff = %d\n", onoff);

	if (onoff)
		usb2_com_line_state(sc, UCOM_LS_BREAK, 0);
	else
		usb2_com_line_state(sc, 0, UCOM_LS_BREAK);
}

static void
usb2_com_dtr(struct usb2_com_softc *sc, uint8_t onoff)
{
	DPRINTF("onoff = %d\n", onoff);

	if (onoff)
		usb2_com_line_state(sc, UCOM_LS_DTR, 0);
	else
		usb2_com_line_state(sc, 0, UCOM_LS_DTR);
}

static void
usb2_com_rts(struct usb2_com_softc *sc, uint8_t onoff)
{
	DPRINTF("onoff = %d\n", onoff);

	if (onoff)
		usb2_com_line_state(sc, UCOM_LS_RTS, 0);
	else
		usb2_com_line_state(sc, 0, UCOM_LS_RTS);
}

static void
usb2_com_cfg_status_change(struct usb2_proc_msg *_task)
{
	struct usb2_com_cfg_task *task = 
	    (struct usb2_com_cfg_task *)_task;
	struct usb2_com_softc *sc = task->sc;
	struct tty *tp;
	uint8_t new_msr;
	uint8_t new_lsr;
	uint8_t onoff;

	tp = sc->sc_tty;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	if (sc->sc_callback->usb2_com_cfg_get_status == NULL) {
		return;
	}
	/* get status */

	new_msr = 0;
	new_lsr = 0;

	(sc->sc_callback->usb2_com_cfg_get_status) (sc, &new_lsr, &new_msr);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		/* TTY device closed */
		return;
	}
	onoff = ((sc->sc_msr ^ new_msr) & SER_DCD);

	sc->sc_msr = new_msr;
	sc->sc_lsr = new_lsr;

	if (onoff) {

		onoff = (sc->sc_msr & SER_DCD) ? 1 : 0;

		DPRINTF("DCD changed to %d\n", onoff);

		ttydisc_modem(tp, onoff);
	}
}

void
usb2_com_status_change(struct usb2_com_softc *sc)
{
	mtx_assert(sc->sc_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}
	DPRINTF("\n");

	usb2_com_queue_command(sc, usb2_com_cfg_status_change, NULL,
	    &sc->sc_status_task[0].hdr,
	    &sc->sc_status_task[1].hdr);
}

static void
usb2_com_cfg_param(struct usb2_proc_msg *_task)
{
	struct usb2_com_param_task *task = 
	    (struct usb2_com_param_task *)_task;
	struct usb2_com_softc *sc = task->sc;

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	if (sc->sc_callback->usb2_com_cfg_param == NULL) {
		return;
	}

	(sc->sc_callback->usb2_com_cfg_param) (sc, &task->termios_copy);

	/* wait a little */
	usb2_pause_mtx(sc->sc_mtx, hz / 10);
}

static int
usb2_com_param(struct tty *tp, struct termios *t)
{
	struct usb2_com_softc *sc = tty_softc(tp);
	uint8_t opened;
	int error;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	opened = 0;
	error = 0;

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {

		/* XXX the TTY layer should call "open()" first! */

		error = usb2_com_open(tp);
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

	if (sc->sc_callback->usb2_com_pre_param) {
		/* Let the lower layer verify the parameters */
		error = (sc->sc_callback->usb2_com_pre_param) (sc, t);
		if (error) {
			DPRINTF("callback error = %d\n", error);
			goto done;
		}
	}

	/* Disable transfers */
	sc->sc_flag &= ~UCOM_FLAG_GP_DATA;

	/* Queue baud rate programming command first */
	usb2_com_queue_command(sc, usb2_com_cfg_param, t,
	    &sc->sc_param_task[0].hdr,
	    &sc->sc_param_task[1].hdr);

	/* Queue transfer enable command last */
	usb2_com_queue_command(sc, usb2_com_cfg_start_transfers, NULL,
	    &sc->sc_start_task[0].hdr, 
	    &sc->sc_start_task[1].hdr);

	if (t->c_cflag & CRTS_IFLOW) {
		sc->sc_flag |= UCOM_FLAG_RTS_IFLOW;
	} else if (sc->sc_flag & UCOM_FLAG_RTS_IFLOW) {
		sc->sc_flag &= ~UCOM_FLAG_RTS_IFLOW;
		usb2_com_modem(tp, SER_RTS, 0);
	}
done:
	if (error) {
		if (opened) {
			usb2_com_close(tp);
		}
	}
	return (error);
}

static void
usb2_com_outwakeup(struct tty *tp)
{
	struct usb2_com_softc *sc = tty_softc(tp);

	mtx_assert(sc->sc_mtx, MA_OWNED);

	DPRINTF("sc = %p\n", sc);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		/* The higher layer is not ready */
		return;
	}
	sc->sc_flag |= UCOM_FLAG_WR_START;

	usb2_com_start_transfers(sc);
}

/*------------------------------------------------------------------------*
 *	usb2_com_get_data
 *
 * Return values:
 * 0: No data is available.
 * Else: Data is available.
 *------------------------------------------------------------------------*/
uint8_t
usb2_com_get_data(struct usb2_com_softc *sc, struct usb2_page_cache *pc,
    uint32_t offset, uint32_t len, uint32_t *actlen)
{
	struct usb2_page_search res;
	struct tty *tp = sc->sc_tty;
	uint32_t cnt;
	uint32_t offset_orig;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if ((!(sc->sc_flag & UCOM_FLAG_HL_READY)) ||
	    (!(sc->sc_flag & UCOM_FLAG_GP_DATA)) ||
	    (!(sc->sc_flag & UCOM_FLAG_WR_START))) {
		actlen[0] = 0;
		return (0);		/* multiport device polling */
	}
	offset_orig = offset;

	while (len != 0) {

		usb2_get_page(pc, offset, &res);

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
usb2_com_put_data(struct usb2_com_softc *sc, struct usb2_page_cache *pc,
    uint32_t offset, uint32_t len)
{
	struct usb2_page_search res;
	struct tty *tp = sc->sc_tty;
	char *buf;
	uint32_t cnt;

	mtx_assert(sc->sc_mtx, MA_OWNED);

	if ((!(sc->sc_flag & UCOM_FLAG_HL_READY)) ||
	    (!(sc->sc_flag & UCOM_FLAG_GP_DATA))) {
		return;			/* multiport device polling */
	}
	if (len == 0)
		return;			/* no data */

	/* set a flag to prevent recursation ? */

	while (len > 0) {

		usb2_get_page(pc, offset, &res);

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
usb2_com_free(void *xsc)
{
	struct usb2_com_softc *sc = xsc;

	mtx_lock(sc->sc_mtx);
	sc->sc_ttyfreed = 1;
	usb2_cv_signal(&sc->sc_cv);
	mtx_unlock(sc->sc_mtx);
}
