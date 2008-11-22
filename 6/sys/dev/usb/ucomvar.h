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

/* Module interface related macros */
#define UCOM_MODVER	1

#define UCOM_MINVER	1
#define UCOM_PREFVER	UCOM_MODVER
#define UCOM_MAXVER	1

/* Macros to clear/set/test flags. */
#define SET(t, f)	(t) |= (f)
#define CLR(t, f)	(t) &= ~((unsigned)(f))
#define ISSET(t, f)	((t) & (f))

#define UCOM_CALLOUT_MASK	0x80

#define UCOMUNIT_MASK		0x3ff7f
#define UCOMDIALOUT_MASK	0x80000
#define UCOMCALLUNIT_MASK	0x40000

#define UCOMUNIT(x)		(minor(x) & UCOMUNIT_MASK)
#define UCOMDIALOUT(x)		(minor(x) & UCOMDIALOUT_MASK)
#define UCOMCALLUNIT(x)		(minor(x) & UCOMCALLUNIT_MASK)

#define UCOM_UNK_PORTNO		-1	/* XXX */

struct ucom_softc;

struct ucom_callback {
	void (*ucom_get_status)(void *, int, u_char *, u_char *);
	void (*ucom_set)(void *, int, int, int);
#define UCOM_SET_DTR 1
#define UCOM_SET_RTS 2
#define UCOM_SET_BREAK 3
	int (*ucom_param)(void *, int, struct termios *);
	int (*ucom_ioctl)(void *, int, u_long, caddr_t, int, usb_proc_ptr);
	int (*ucom_open)(void *, int);
	void (*ucom_close)(void *, int);
	void (*ucom_read)(void *, int, u_char **, u_int32_t *);
	void (*ucom_write)(void *, int, u_char *, u_char *, u_int32_t *);
};

/* line status register */
#define ULSR_RCV_FIFO	0x80
#define ULSR_TSRE	0x40	/* Transmitter empty: byte sent */
#define ULSR_TXRDY	0x20	/* Transmitter buffer empty */
#define ULSR_BI		0x10	/* Break detected */
#define ULSR_FE		0x08	/* Framing error: bad stop bit */
#define ULSR_PE		0x04	/* Parity error */
#define ULSR_OE		0x02	/* Overrun, lost incoming byte */
#define ULSR_RXRDY	0x01	/* Byte ready in Receive Buffer */
#define ULSR_RCV_MASK	0x1f	/* Mask for incoming data or error */

/* ucom state declarations */
#define UCS_RXSTOP	0x0001	/* Rx stopped */
#define UCS_RTS_IFLOW	0x0008	/* use RTS input flow control */

struct ucom_softc {
	USBBASEDEVICE		sc_dev;		/* base device */
	usbd_device_handle	sc_udev;	/* USB device */
	usbd_interface_handle	sc_iface;	/* data interface */

	int			sc_bulkin_no;	/* bulk in endpoint address */
	usbd_pipe_handle	sc_bulkin_pipe;	/* bulk in pipe */
	usbd_xfer_handle	sc_ixfer;	/* read request */
	u_char			*sc_ibuf;	/* read buffer */
	u_int			sc_ibufsize;	/* read buffer size */
	u_int			sc_ibufsizepad;	/* read buffer size padded */

	int			sc_bulkout_no;	/* bulk out endpoint address */
	usbd_pipe_handle	sc_bulkout_pipe;/* bulk out pipe */
	usbd_xfer_handle	sc_oxfer;	/* write request */
	u_char			*sc_obuf;	/* write buffer */
	u_int			sc_obufsize;	/* write buffer size */
	u_int			sc_opkthdrlen;	/* header length of
						   output packet */

	struct ucom_callback	*sc_callback;
	void			*sc_parent;
	int			sc_portno;

	struct tty		*sc_tty;	/* our tty */

	int			sc_state;

	int			sc_poll;

	u_char			sc_lsr;
	u_char			sc_msr;
	u_char			sc_mcr;

	u_char			sc_dying;	/* disconnecting */

};

extern devclass_t ucom_devclass;

int ucom_attach(struct ucom_softc *);
int ucom_detach(struct ucom_softc *);
void ucom_status_change(struct ucom_softc *);
