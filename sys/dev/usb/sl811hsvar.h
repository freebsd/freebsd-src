/*	$NetBSD$	*/
/*      $FreeBSD: src/sys/dev/usb/sl811hsvar.h,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tetsuya Isaki.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
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
 * ScanLogic SL811HS/T USB Host Controller
 */

#define MS_TO_TICKS(ms) ((ms) * hz / 1000)
#define delay_ms(X) \
	pause("slhci", MS_TO_TICKS(X))

#define SL11_PID_OUT	(0x1)
#define SL11_PID_IN 	(0x9)
#define SL11_PID_SOF	(0x5)
#define SL11_PID_SETUP	(0xd)

struct slhci_xfer {
	usbd_xfer_handle sx_xfer;
	usb_callout_t sx_callout_t;
};

struct slhci_softc {
	struct usbd_bus		 sc_bus;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
  
#ifdef __FreeBSD__
	void *ih; 
	struct resource *io_res;
	struct resource *irq_res;
#endif
  
	void				(*sc_enable_power)(void *, int);
	void				(*sc_enable_intr)(void *, int);
	void				*sc_arg;
	int					 sc_powerstat;
#define POWER_ON	(1)
#define POWER_OFF	(0)
#define INTR_ON 	(1)
#define INTR_OFF	(0)

	struct device		*sc_parent;	/* parent device */
	int			 sc_sltype;	/* revision */
#define SLTYPE_SL11H	(0x00)
#define SLTYPE_SL811HS	(0x01)
#define SLTYPE_SL811HS_R12	SLTYPE_SL811HS
#define SLTYPE_SL811HS_R14	(0x02)

	u_int8_t		 sc_addr;	/* device address of root hub */
	u_int8_t		 sc_conf;
	STAILQ_HEAD(, usbd_xfer) sc_free_xfers;

	/* Information for the root hub interrupt pipe */
	int			 sc_interval;
	usbd_xfer_handle	 sc_intr_xfer;
	usb_callout_t		 sc_poll_handle;

	int				 sc_flags;
#define SLF_RESET	(0x01)
#define SLF_INSERT	(0x02)
#define SLF_ATTACHED  (0x04)

	/* Root HUB specific members */
	int				sc_fullspeed;
	int				sc_connect;	/* XXX */
	int				sc_change;
};

int  sl811hs_find(struct slhci_softc *);
int  slhci_attach(struct slhci_softc *);
int  slhci_intr(void *);
