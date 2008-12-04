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

/*
 * NOTE: all function names beginning like "usb2_com_cfg_" can only
 * be called from within the config thread function !
 */

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_cdc.h>

#define	USB_DEBUG_VAR usb2_com_debug
#define	usb2_config_td_cc usb2_com_config_copy
#define	usb2_config_td_softc usb2_com_softc

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
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

struct usb2_com_config_copy {
	struct usb2_com_softc *cc_softc;
	uint8_t	cc_flag0;
	uint8_t	cc_flag1;
	uint8_t	cc_flag2;
	uint8_t	cc_flag3;
};

static usb2_config_td_command_t usb2_com_config_copy;
static usb2_config_td_command_t usb2_com_cfg_start_transfers;
static usb2_config_td_command_t usb2_com_cfg_open;
static usb2_config_td_command_t usb2_com_cfg_close;
static usb2_config_td_command_t usb2_com_cfg_break;
static usb2_config_td_command_t usb2_com_cfg_dtr;
static usb2_config_td_command_t usb2_com_cfg_rts;
static usb2_config_td_command_t usb2_com_cfg_status_change;
static usb2_config_td_command_t usb2_com_cfg_param;

static uint8_t usb2_com_units_alloc(uint32_t sub_units, uint32_t *p_root_unit);
static void usb2_com_units_free(uint32_t root_unit, uint32_t sub_units);
static int usb2_com_attach_sub(struct usb2_com_softc *sc);
static void usb2_com_detach_sub(struct usb2_com_softc *sc);
static void usb2_com_queue_command(struct usb2_com_softc *sc, usb2_config_td_command_t *cmd, int flag);
static void usb2_com_shutdown(struct usb2_com_softc *sc);
static void usb2_com_start_transfers(struct usb2_com_softc *sc);
static void usb2_com_break(struct usb2_com_softc *sc, uint8_t onoff);
static void usb2_com_dtr(struct usb2_com_softc *sc, uint8_t onoff);
static void usb2_com_rts(struct usb2_com_softc *sc, uint8_t onoff);

static tsw_open_t usb2_com_open;
static tsw_close_t usb2_com_close;
static tsw_ioctl_t usb2_com_ioctl;
static tsw_modem_t usb2_com_modem;
static tsw_param_t usb2_com_param;
static tsw_outwakeup_t usb2_com_start_write;
static tsw_free_t usb2_com_free;

static struct ttydevsw usb2_com_class = {
	.tsw_flags = TF_INITLOCK | TF_CALLOUT,
	.tsw_open = usb2_com_open,
	.tsw_close = usb2_com_close,
	.tsw_outwakeup = usb2_com_start_write,
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

	return;
}

/*
 * "N" sub_units are setup at a time. All sub-units will
 * be given sequential unit numbers. The number of
 * sub-units can be used to differentiate among
 * different types of devices.
 *
 * The mutex pointed to by "p_mtx" is applied before all
 * callbacks are called back. Also "p_mtx" must be applied
 * before calling into the ucom-layer! Currently only Giant
 * is supported.
 */
int
usb2_com_attach(struct usb2_com_super_softc *ssc, struct usb2_com_softc *sc,
    uint32_t sub_units, void *parent,
    const struct usb2_com_callback *callback, struct mtx *p_mtx)
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
	if (usb2_com_units_alloc(sub_units, &root_unit)) {
		return (ENOMEM);
	}
	if (usb2_config_td_setup
	    (&ssc->sc_config_td, sc, p_mtx, NULL,
	    sizeof(struct usb2_com_config_copy), 24 * sub_units)) {
		usb2_com_units_free(root_unit, sub_units);
		return (ENOMEM);
	}
	for (n = 0; n < sub_units; n++, sc++) {
		sc->sc_unit = root_unit + n;
		sc->sc_local_unit = n;
		sc->sc_super = ssc;
		sc->sc_parent_mtx = p_mtx;
		sc->sc_parent = parent;
		sc->sc_callback = callback;

		error = usb2_com_attach_sub(sc);
		if (error) {
			usb2_com_detach(ssc, sc - n, n);
			usb2_com_units_free(root_unit + n, sub_units - n);
			break;
		}
		sc->sc_flag |= UCOM_FLAG_ATTACHED;
	}
	return (error);
}

/* NOTE: the following function will do nothing if
 * the structure pointed to by "ssc" and "sc" is zero.
 */
void
usb2_com_detach(struct usb2_com_super_softc *ssc, struct usb2_com_softc *sc,
    uint32_t sub_units)
{
	uint32_t n;

	usb2_config_td_drain(&ssc->sc_config_td);

	for (n = 0; n < sub_units; n++, sc++) {
		if (sc->sc_flag & UCOM_FLAG_ATTACHED) {

			usb2_com_detach_sub(sc);

			usb2_com_units_free(sc->sc_unit, 1);

			/* avoid duplicate detach: */
			sc->sc_flag &= ~UCOM_FLAG_ATTACHED;
		}
	}

	usb2_config_td_unsetup(&ssc->sc_config_td);

	return;
}

static int
usb2_com_attach_sub(struct usb2_com_softc *sc)
{
	struct tty *tp;
	int error = 0;
	char buf[32];			/* temporary TTY device name buffer */

	tp = tty_alloc(&usb2_com_class, sc, sc->sc_parent_mtx);
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
		if (snprintf(buf, sizeof(buf), "U%u", sc->sc_unit)) {
			/* ignore */
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
usb2_com_detach_sub(struct usb2_com_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	DPRINTF("sc = %p, tp = %p\n", sc, sc->sc_tty);

	/* the config thread has been stopped when we get here */

	mtx_lock(sc->sc_parent_mtx);
	sc->sc_flag |= UCOM_FLAG_GONE;
	sc->sc_flag &= ~(UCOM_FLAG_HL_READY |
	    UCOM_FLAG_LL_READY);
	mtx_unlock(sc->sc_parent_mtx);
	if (tp) {
		tty_lock(tp);

		usb2_com_close(tp);	/* close, if any */

		tty_rel_gone(tp);

		mtx_lock(sc->sc_parent_mtx);
		/* Wait for the callback after the TTY is torn down */
		while (sc->sc_ttyfreed == 0)
			usb2_cv_wait(&sc->sc_cv, sc->sc_parent_mtx);
		/*
		 * make sure that read and write transfers are stopped
		 */
		if (sc->sc_callback->usb2_com_stop_read) {
			(sc->sc_callback->usb2_com_stop_read) (sc);
		}
		if (sc->sc_callback->usb2_com_stop_write) {
			(sc->sc_callback->usb2_com_stop_write) (sc);
		}
		mtx_unlock(sc->sc_parent_mtx);
	}
	usb2_cv_destroy(&sc->sc_cv);
	return;
}

static void
usb2_com_config_copy(struct usb2_com_softc *sc, struct usb2_com_config_copy *cc,
    uint16_t refcount)
{
	cc->cc_softc = sc + (refcount % UCOM_SUB_UNIT_MAX);
	cc->cc_flag0 = (refcount / (1 * UCOM_SUB_UNIT_MAX)) % 2;
	cc->cc_flag1 = (refcount / (2 * UCOM_SUB_UNIT_MAX)) % 2;
	cc->cc_flag2 = (refcount / (4 * UCOM_SUB_UNIT_MAX)) % 2;
	cc->cc_flag3 = (refcount / (8 * UCOM_SUB_UNIT_MAX)) % 2;
	return;
}

static void
usb2_com_queue_command(struct usb2_com_softc *sc, usb2_config_td_command_t *cmd, int flag)
{
	struct usb2_com_super_softc *ssc = sc->sc_super;

	usb2_config_td_queue_command
	    (&ssc->sc_config_td, &usb2_com_config_copy,
	    cmd, (cmd == &usb2_com_cfg_status_change) ? 1 : 0,
	    ((sc->sc_local_unit % UCOM_SUB_UNIT_MAX) +
	    (flag ? UCOM_SUB_UNIT_MAX : 0)));
	return;
}

static void
usb2_com_shutdown(struct usb2_com_softc *sc)
{
	struct tty *tp = sc->sc_tty;

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

	DPRINTF("\n");

	/*
	 * Hang up if necessary:
	 */
	if (tp->t_termios.c_cflag & HUPCL) {
		usb2_com_modem(tp, 0, SER_DTR);
	}
	return;
}

/*
 * Return values:
 *    0: normal delay
 * else: config thread is gone
 */
uint8_t
usb2_com_cfg_sleep(struct usb2_com_softc *sc, uint32_t timeout)
{
	struct usb2_com_super_softc *ssc = sc->sc_super;

	return (usb2_config_td_sleep(&ssc->sc_config_td, timeout));
}

/*
 * Return values:
 *    0: normal
 * else: config thread is gone
 */
uint8_t
usb2_com_cfg_is_gone(struct usb2_com_softc *sc)
{
	struct usb2_com_super_softc *ssc = sc->sc_super;

	return (usb2_config_td_is_gone(&ssc->sc_config_td));
}

static void
usb2_com_cfg_start_transfers(struct usb2_com_softc *sc, struct usb2_com_config_copy *cc,
    uint16_t refcount)
{
	sc = cc->cc_softc;

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
	return;
}

static void
usb2_com_start_transfers(struct usb2_com_softc *sc)
{
	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}
	/*
	 * do a direct call first, to get hardware buffers flushed
	 */

	if (sc->sc_callback->usb2_com_start_read) {
		(sc->sc_callback->usb2_com_start_read) (sc);
	}
	if (sc->sc_callback->usb2_com_start_write) {
		(sc->sc_callback->usb2_com_start_write) (sc);
	}
	if (!(sc->sc_flag & UCOM_FLAG_GP_DATA)) {
		usb2_com_queue_command(sc, &usb2_com_cfg_start_transfers, 0);
	}
	return;
}

static void
usb2_com_cfg_open(struct usb2_com_softc *sc, struct usb2_com_config_copy *cc,
    uint16_t refcount)
{
	sc = cc->cc_softc;

	DPRINTF("\n");

	if (sc->sc_flag & UCOM_FLAG_LL_READY) {

		/* already opened */

	} else {

		sc->sc_flag |= UCOM_FLAG_LL_READY;

		if (sc->sc_callback->usb2_com_cfg_open) {
			(sc->sc_callback->usb2_com_cfg_open) (sc);

			/* wait a little */
			usb2_com_cfg_sleep(sc, hz / 10);
		}
	}
	return;
}

static int
usb2_com_open(struct tty *tp)
{
	struct usb2_com_softc *sc = tty_softc(tp);
	int error;

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

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

	usb2_com_queue_command(sc, &usb2_com_cfg_open, 0);

	usb2_com_start_transfers(sc);

	usb2_com_modem(tp, SER_DTR | SER_RTS, 0);

	usb2_com_break(sc, 0);

	usb2_com_status_change(sc);

	return (0);
}

static void
usb2_com_cfg_close(struct usb2_com_softc *sc, struct usb2_com_config_copy *cc,
    uint16_t refcount)
{
	sc = cc->cc_softc;

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
	return;
}

static void
usb2_com_close(struct tty *tp)
{
	struct usb2_com_softc *sc = tty_softc(tp);

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

	DPRINTF("tp=%p\n", tp);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		DPRINTF("tp=%p already closed\n", tp);
		return;
	}
	usb2_com_shutdown(sc);

	usb2_com_queue_command(sc, &usb2_com_cfg_close, 0);

	sc->sc_flag &= ~(UCOM_FLAG_HL_READY |
	    UCOM_FLAG_WR_START |
	    UCOM_FLAG_RTS_IFLOW);

	if (sc->sc_callback->usb2_com_stop_read) {
		(sc->sc_callback->usb2_com_stop_read) (sc);
	}
	if (sc->sc_callback->usb2_com_stop_write) {
		(sc->sc_callback->usb2_com_stop_write) (sc);
	}
	return;
}

static int
usb2_com_ioctl(struct tty *tp, u_long cmd, caddr_t data, struct thread *td)
{
	struct usb2_com_softc *sc = tty_softc(tp);
	int error;

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

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

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

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
usb2_com_cfg_break(struct usb2_com_softc *sc, struct usb2_com_config_copy *cc,
    uint16_t refcount)
{
	sc = cc->cc_softc;

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	DPRINTF("onoff=%d\n", cc->cc_flag0);

	if (sc->sc_callback->usb2_com_cfg_set_break) {
		(sc->sc_callback->usb2_com_cfg_set_break) (sc, cc->cc_flag0);
	}
	return;
}

static void
usb2_com_break(struct usb2_com_softc *sc, uint8_t onoff)
{
	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}
	DPRINTF("onoff = %d\n", onoff);

	usb2_com_queue_command(sc, &usb2_com_cfg_break, onoff);
	return;
}

static void
usb2_com_cfg_dtr(struct usb2_com_softc *sc, struct usb2_com_config_copy *cc,
    uint16_t refcount)
{
	sc = cc->cc_softc;

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	DPRINTF("onoff=%d\n", cc->cc_flag0);

	if (sc->sc_callback->usb2_com_cfg_set_dtr) {
		(sc->sc_callback->usb2_com_cfg_set_dtr) (sc, cc->cc_flag0);
	}
	return;
}

static void
usb2_com_dtr(struct usb2_com_softc *sc, uint8_t onoff)
{
	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}
	DPRINTF("onoff = %d\n", onoff);

	usb2_com_queue_command(sc, &usb2_com_cfg_dtr, onoff);
	return;
}

static void
usb2_com_cfg_rts(struct usb2_com_softc *sc, struct usb2_com_config_copy *cc,
    uint16_t refcount)
{
	sc = cc->cc_softc;

	DPRINTF("onoff=%d\n", cc->cc_flag0);

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	if (sc->sc_callback->usb2_com_cfg_set_rts) {
		(sc->sc_callback->usb2_com_cfg_set_rts) (sc, cc->cc_flag0);
	}
	return;
}

static void
usb2_com_rts(struct usb2_com_softc *sc, uint8_t onoff)
{
	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}
	DPRINTF("onoff = %d\n", onoff);

	usb2_com_queue_command(sc, &usb2_com_cfg_rts, onoff);

	return;
}

static void
usb2_com_cfg_status_change(struct usb2_com_softc *sc,
    struct usb2_com_config_copy *cc, uint16_t refcount)
{
	struct tty *tp;

	uint8_t new_msr;
	uint8_t new_lsr;
	uint8_t onoff;

	sc = cc->cc_softc;
	tp = sc->sc_tty;

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

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
	return;
}

void
usb2_com_status_change(struct usb2_com_softc *sc)
{
	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		return;
	}
	DPRINTF("\n");

	usb2_com_queue_command(sc, &usb2_com_cfg_status_change, 0);
	return;
}

static void
usb2_com_cfg_param(struct usb2_com_softc *sc, struct usb2_com_config_copy *cc,
    uint16_t refcount)
{
	struct termios t_copy;

	sc = cc->cc_softc;

	if (!(sc->sc_flag & UCOM_FLAG_LL_READY)) {
		return;
	}
	if (sc->sc_callback->usb2_com_cfg_param == NULL) {
		return;
	}
	t_copy = sc->sc_termios_copy;

	(sc->sc_callback->usb2_com_cfg_param) (sc, &t_copy);

	/* wait a little */
	usb2_com_cfg_sleep(sc, hz / 10);

	return;
}

static int
usb2_com_param(struct tty *tp, struct termios *t)
{
	struct usb2_com_softc *sc = tty_softc(tp);
	uint8_t opened;
	int error;

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

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
	/* Make a copy of the termios parameters */
	sc->sc_termios_copy = *t;

	/* Disable transfers */
	sc->sc_flag &= ~UCOM_FLAG_GP_DATA;

	/* Queue baud rate programming command first */
	usb2_com_queue_command(sc, &usb2_com_cfg_param, 0);

	/* Queue transfer enable command last */
	usb2_com_start_transfers(sc);

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
usb2_com_start_write(struct tty *tp)
{
	struct usb2_com_softc *sc = tty_softc(tp);

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

	DPRINTF("sc = %p\n", sc);

	if (!(sc->sc_flag & UCOM_FLAG_HL_READY)) {
		/* The higher layer is not ready */
		return;
	}
	sc->sc_flag |= UCOM_FLAG_WR_START;

	usb2_com_start_transfers(sc);

	return;
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

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

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

	mtx_assert(sc->sc_parent_mtx, MA_OWNED);

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
	return;
}

static void
usb2_com_free(void *xsc)
{
	struct usb2_com_softc *sc = xsc;

	mtx_lock(sc->sc_parent_mtx);
	sc->sc_ttyfreed = 1;
	usb2_cv_signal(&sc->sc_cv);
	mtx_unlock(sc->sc_parent_mtx);
}
