/*	$NetBSD: ucomvar.h,v 1.9 2001/01/23 21:56:17 augustss Exp $	*/
/*	$FreeBSD$	*/

/*-
 * Copyright (c) 2001-2002, Shunsuke Akiyama <akiyama@jp.FreeBSD.org>.
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

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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

#ifndef _USB2_SERIAL_H_
#define	_USB2_SERIAL_H_

#include <sys/tty.h>
#include <sys/serial.h>
#include <sys/fcntl.h>
#include <sys/termios.h>

/* Module interface related macros */
#define	UCOM_MODVER	1

#define	UCOM_MINVER	1
#define	UCOM_PREFVER	UCOM_MODVER
#define	UCOM_MAXVER	1

struct usb2_com_softc;
struct thread;

/* NOTE: Only callbacks with "_cfg_" in its name are called
 * from a config thread, and are allowed to sleep! The other
 * callbacks are _not_ allowed to sleep!
 *
 * NOTE: There is no guarantee that "usb2_com_cfg_close()" will
 * be called after "usb2_com_cfg_open()" if the device is detached
 * while it is open!
 */
struct usb2_com_callback {
	void    (*usb2_com_cfg_get_status) (struct usb2_com_softc *, uint8_t *plsr, uint8_t *pmsr);
	void    (*usb2_com_cfg_set_dtr) (struct usb2_com_softc *, uint8_t);
	void    (*usb2_com_cfg_set_rts) (struct usb2_com_softc *, uint8_t);
	void    (*usb2_com_cfg_set_break) (struct usb2_com_softc *, uint8_t);
	void    (*usb2_com_cfg_param) (struct usb2_com_softc *, struct termios *);
	void    (*usb2_com_cfg_open) (struct usb2_com_softc *);
	void    (*usb2_com_cfg_close) (struct usb2_com_softc *);
	int     (*usb2_com_pre_open) (struct usb2_com_softc *);
	int     (*usb2_com_pre_param) (struct usb2_com_softc *, struct termios *);
	int     (*usb2_com_ioctl) (struct usb2_com_softc *, uint32_t, caddr_t, int, struct thread *);
	void    (*usb2_com_start_read) (struct usb2_com_softc *);
	void    (*usb2_com_stop_read) (struct usb2_com_softc *);
	void    (*usb2_com_start_write) (struct usb2_com_softc *);
	void    (*usb2_com_stop_write) (struct usb2_com_softc *);
	void    (*usb2_com_tty_name) (struct usb2_com_softc *, char *pbuf, uint16_t buflen, uint16_t local_subunit);
};

/* Line status register */
#define	ULSR_RCV_FIFO	0x80
#define	ULSR_TSRE	0x40		/* Transmitter empty: byte sent */
#define	ULSR_TXRDY	0x20		/* Transmitter buffer empty */
#define	ULSR_BI		0x10		/* Break detected */
#define	ULSR_FE		0x08		/* Framing error: bad stop bit */
#define	ULSR_PE		0x04		/* Parity error */
#define	ULSR_OE		0x02		/* Overrun, lost incoming byte */
#define	ULSR_RXRDY	0x01		/* Byte ready in Receive Buffer */
#define	ULSR_RCV_MASK	0x1f		/* Mask for incoming data or error */

struct usb2_com_super_softc {
	struct usb2_config_td sc_config_td;
};

struct usb2_com_softc {
	struct termios sc_termios_copy;
	struct cv sc_cv;
	const struct usb2_com_callback *sc_callback;
	struct usb2_com_super_softc *sc_super;
	struct tty *sc_tty;
	struct mtx *sc_parent_mtx;
	void   *sc_parent;
	uint32_t sc_unit;
	uint32_t sc_local_unit;
	uint16_t sc_portno;
	uint8_t	sc_flag;
#define	UCOM_FLAG_RTS_IFLOW	0x01	/* use RTS input flow control */
#define	UCOM_FLAG_GONE		0x02	/* the device is gone */
#define	UCOM_FLAG_ATTACHED	0x04	/* set if attached */
#define	UCOM_FLAG_GP_DATA	0x08	/* set if get and put data is possible */
#define	UCOM_FLAG_WR_START	0x10	/* set if write start was issued */
#define	UCOM_FLAG_LL_READY	0x20	/* set if low layer is ready */
#define	UCOM_FLAG_HL_READY	0x40	/* set if high layer is ready */
	uint8_t	sc_lsr;
	uint8_t	sc_msr;
	uint8_t	sc_mcr;
	uint8_t	sc_ttyfreed;		/* set when TTY has been freed */
};

int	usb2_com_attach(struct usb2_com_super_softc *ssc, struct usb2_com_softc *sc, uint32_t sub_units, void *parent, const struct usb2_com_callback *callback, struct mtx *p_mtx);
void	usb2_com_detach(struct usb2_com_super_softc *ssc, struct usb2_com_softc *sc, uint32_t sub_units);
void	usb2_com_status_change(struct usb2_com_softc *);
uint8_t	usb2_com_get_data(struct usb2_com_softc *sc, struct usb2_page_cache *pc, uint32_t offset, uint32_t len, uint32_t *actlen);
void	usb2_com_put_data(struct usb2_com_softc *sc, struct usb2_page_cache *pc, uint32_t offset, uint32_t len);
uint8_t	usb2_com_cfg_sleep(struct usb2_com_softc *sc, uint32_t timeout);
uint8_t	usb2_com_cfg_is_gone(struct usb2_com_softc *sc);

#endif					/* _USB2_SERIAL_H_ */
