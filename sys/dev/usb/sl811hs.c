/*	$NetBSD: sl811hs.c,v 1.5 2005/02/27 00:27:02 perry Exp $	*/

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
/*
 * !! HIGHLY EXPERIMENTAL CODE !!
 */

#include <sys/cdefs.h>
//_RCSID(0, "$NetBSD: sl811hs.c,v 1.5 2005/02/27 00:27:02 perry Exp $");

#include "opt_slhci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <sys/module.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_port.h>
#include "usbdevs.h"

#include <dev/usb/sl811hsreg.h>
#include <dev/usb/sl811hsvar.h>

__FBSDID("$FreeBSD: src/sys/dev/usb/sl811hs.c,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $");

static inline u_int8_t sl11read(struct slhci_softc *, int);
static inline void     sl11write(struct slhci_softc *, int, u_int8_t);
static inline void sl11read_region(struct slhci_softc *, u_char *, int, int);
static inline void sl11write_region(struct slhci_softc *, int, u_char *, int);

static void		sl11_reset(struct slhci_softc *);
static void		sl11_speed(struct slhci_softc *);

static usbd_status	slhci_open(usbd_pipe_handle);
static void		slhci_softintr(void *);
static void		slhci_poll(struct usbd_bus *);
static void		slhci_poll_hub(void *);
static void		slhci_poll_device(void *arg);
static usbd_status	slhci_allocm(struct usbd_bus *, usb_dma_t *, u_int32_t);
static void		slhci_freem(struct usbd_bus *, usb_dma_t *);
static usbd_xfer_handle slhci_allocx(struct usbd_bus *);
static void		slhci_freex(struct usbd_bus *, usbd_xfer_handle);

static int		slhci_str(usb_string_descriptor_t *, int, const char *);

static usbd_status	slhci_root_ctrl_transfer(usbd_xfer_handle);
static usbd_status	slhci_root_ctrl_start(usbd_xfer_handle);
static void		slhci_root_ctrl_abort(usbd_xfer_handle);
static void		slhci_root_ctrl_close(usbd_pipe_handle);
static void		slhci_root_ctrl_done(usbd_xfer_handle);

static usbd_status	slhci_root_intr_transfer(usbd_xfer_handle);
static usbd_status	slhci_root_intr_start(usbd_xfer_handle);
static void		slhci_root_intr_abort(usbd_xfer_handle);
static void		slhci_root_intr_close(usbd_pipe_handle);
static void		slhci_root_intr_done(usbd_xfer_handle);

static usbd_status	slhci_device_ctrl_transfer(usbd_xfer_handle);
static usbd_status	slhci_device_ctrl_start(usbd_xfer_handle);
static void		slhci_device_ctrl_abort(usbd_xfer_handle);
static void		slhci_device_ctrl_close(usbd_pipe_handle);
static void		slhci_device_ctrl_done(usbd_xfer_handle);

static usbd_status	slhci_device_intr_transfer(usbd_xfer_handle);
static usbd_status	slhci_device_intr_start(usbd_xfer_handle);
static void		slhci_device_intr_abort(usbd_xfer_handle);
static void		slhci_device_intr_close(usbd_pipe_handle);
static void		slhci_device_intr_done(usbd_xfer_handle);

static usbd_status	slhci_device_isoc_transfer(usbd_xfer_handle);
static usbd_status	slhci_device_isoc_start(usbd_xfer_handle);
static void		slhci_device_isoc_abort(usbd_xfer_handle);
static void		slhci_device_isoc_close(usbd_pipe_handle);
static void		slhci_device_isoc_done(usbd_xfer_handle);

static usbd_status	slhci_device_bulk_transfer(usbd_xfer_handle);
static usbd_status	slhci_device_bulk_start(usbd_xfer_handle);
static void		slhci_device_bulk_abort(usbd_xfer_handle);
static void		slhci_device_bulk_close(usbd_pipe_handle);
static void		slhci_device_bulk_done(usbd_xfer_handle);

static int		slhci_transaction(struct slhci_softc *,
	usbd_pipe_handle, u_int8_t, int, u_char *, u_int8_t);
static void		slhci_noop(usbd_pipe_handle);
static void		slhci_abort_xfer(usbd_xfer_handle, usbd_status);
static void		slhci_device_clear_toggle(usbd_pipe_handle);

extern int usbdebug;

/* For root hub */
#define SLHCI_INTR_ENDPT	(1)

#ifdef SLHCI_DEBUG
#define D_TRACE	(0x0001)	/* function trace */
#define D_MSG	(0x0002)	/* debug messages */
#define D_XFER	(0x0004)	/* transfer messages (noisy!) */
#define D_MEM	(0x0008)	/* memory allocation */

int slhci_debug = D_MSG | D_XFER;
#define DPRINTF(z,x)	if((slhci_debug&(z))!=0)printf x
void		print_req(usb_device_request_t *);
void		print_req_hub(usb_device_request_t *);
void		print_dumpreg(struct slhci_softc *);
void		print_xfer(usbd_xfer_handle);
#else
#define DPRINTF(z,x)
#endif


/* XXX: sync with argument */
static const char *sltypestr [] = {
	"SL11H/T",
	"SL811HS/T",
};


struct usbd_bus_methods slhci_bus_methods = {
	slhci_open,
	slhci_softintr,
	slhci_poll,
	slhci_allocm,
	slhci_freem,
	slhci_allocx,
	slhci_freex,
};

struct usbd_pipe_methods slhci_root_ctrl_methods = {
	slhci_root_ctrl_transfer,
	slhci_root_ctrl_start,
	slhci_root_ctrl_abort,
	slhci_root_ctrl_close,
	slhci_noop,
	slhci_root_ctrl_done,
};

struct usbd_pipe_methods slhci_root_intr_methods = {
	slhci_root_intr_transfer,
	slhci_root_intr_start,
	slhci_root_intr_abort,
	slhci_root_intr_close,
	slhci_noop,
	slhci_root_intr_done,
};

struct usbd_pipe_methods slhci_device_ctrl_methods = {
	slhci_device_ctrl_transfer,
	slhci_device_ctrl_start,
	slhci_device_ctrl_abort,
	slhci_device_ctrl_close,
	slhci_noop,
	slhci_device_ctrl_done,
};

struct usbd_pipe_methods slhci_device_intr_methods = {
	slhci_device_intr_transfer,
	slhci_device_intr_start,
	slhci_device_intr_abort,
	slhci_device_intr_close,
	slhci_device_clear_toggle,
	slhci_device_intr_done,
};

struct usbd_pipe_methods slhci_device_isoc_methods = {
	slhci_device_isoc_transfer,
	slhci_device_isoc_start,
	slhci_device_isoc_abort,
	slhci_device_isoc_close,
	slhci_noop,
	slhci_device_isoc_done,
};

struct usbd_pipe_methods slhci_device_bulk_methods = {
	slhci_device_bulk_transfer,
	slhci_device_bulk_start,
	slhci_device_bulk_abort,
	slhci_device_bulk_close,
	slhci_noop,
	slhci_device_bulk_done,
};

struct slhci_pipe {
	struct usbd_pipe pipe;
};


/*
 * SL811HS Register read/write routine
 */
static inline u_int8_t
sl11read(struct slhci_softc *sc, int reg)
{
#if 1
	int b;
	DELAY(80);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SL11_IDX_ADDR, reg);
	b = bus_space_read_1(sc->sc_iot, sc->sc_ioh, SL11_IDX_DATA);
	return b;
#else
	outb(0x4000, reg&0xff);
	return (inb(0x4001)&0xff);
#endif
}

static inline void
sl11write(struct slhci_softc *sc, int reg, u_int8_t data)
{
#if 1
	DELAY(80);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SL11_IDX_ADDR, reg);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SL11_IDX_DATA, data);
#else
	outb(0x4000, reg&0xff);
	outb(0x4000, data&0xff);
#endif
}

static inline void
sl11read_region(struct slhci_softc *sc, u_char *buf, int reg, int len)
{
	int i;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SL11_IDX_ADDR, reg);
	for (i = 0; i < len; i++)
		buf[i] = bus_space_read_1(sc->sc_iot, sc->sc_ioh, SL11_IDX_DATA);
}

static inline void
sl11write_region(struct slhci_softc *sc, int reg, u_char *buf, int len)
{
	int i;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SL11_IDX_ADDR, reg);
	for (i = 0; i < len; i++)
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, SL11_IDX_DATA, buf[i]);
}

/*
 * USB bus reset. From sl811hs_appnote.pdf, p22
 */
static void
sl11_reset(struct slhci_softc *sc)
{
	u_int8_t r;

	DPRINTF(D_TRACE, ("%s() ", __FUNCTION__));
	//	r = sl11read(sc, SL11_CTRL);
	r = 0;
	sl11write(sc, SL11_CTRL, r | SL11_CTRL_RESETENGINE);
	delay_ms(250);
	sl11write(sc, SL11_CTRL, r | SL11_CTRL_JKSTATE | SL11_CTRL_RESETENGINE);	delay_ms(150);
	sl11write(sc, SL11_CTRL, r | SL11_CTRL_RESETENGINE);
	delay_ms(10);
	sl11write(sc, SL11_CTRL, r);
}

/*
 * Detect the speed of attached device.
 */
static void
sl11_speed(struct slhci_softc *sc)
{
	u_int8_t r;

	sl11write(sc, SL11_ISR, 0xff);
	r = sl11read(sc, SL11_ISR);
	if ((r & SL11_ISR_RESET)) {
		DPRINTF(D_MSG, ("NC "));
		sl11write(sc, SL11_ISR, SL11_ISR_RESET);
		sc->sc_connect = 0;
	}

	if ((sl11read(sc, SL11_ISR) & SL11_ISR_RESET)) {
		sl11write(sc, SL11_ISR, 0xff);
	} else {
		u_int8_t pol = 0, ctrl = 0;

		sc->sc_connect = 1;
		if (r & SL11_ISR_DATA) {
			DPRINTF(D_MSG, ("FS "));
			pol  = 0;
			ctrl = SL11_CTRL_EOF2;
			sc->sc_fullspeed = 1;
		} else {
			DPRINTF(D_MSG, ("LS "));
			pol  = SL811_CSOF_POLARITY;
			ctrl = SL11_CTRL_LOWSPEED;
			sc->sc_fullspeed = 0;
		}
		sl11write(sc, SL811_CSOF, pol | SL811_CSOF_MASTER | 0x2e);
		sl11write(sc, SL11_DATA, 0xe0);
		sl11write(sc, SL11_CTRL, ctrl | SL11_CTRL_ENABLESOF);
	}

	sl11write(sc, SL11_E0PID,  (SL11_PID_SOF << 4) + 0);
	sl11write(sc, SL11_E0DEV,  0);
	sl11write(sc, SL11_E0CTRL, SL11_EPCTRL_ARM);
	delay_ms(30);
}

/*
 * If detect some known controller, return the type.
 * If does not, return -1.
 */
int
sl811hs_find(struct slhci_softc *sc)
{
	int rev;
	sc->sc_sltype = -1;

	rev = sl11read(sc, SL11_REV) >> 4;
	if (rev >= SLTYPE_SL11H && rev <= SLTYPE_SL811HS_R14)
		sc->sc_sltype = rev;
	return sc->sc_sltype;
}

/*
 * Attach SL11H/SL811HS. Return 0 if success.
 */
int
slhci_attach(struct slhci_softc *sc)
{
	int rev;
	/* Detect and check the controller type */

	rev = sl811hs_find(sc);
	if (rev == -1)
		return -1;

	printf("%s: ScanLogic %s USB Host Controller",
		device_get_nameunit(sc->sc_bus.bdev), sltypestr[(rev > 0)]);
	switch (rev) {
	case SLTYPE_SL11H:
		break;
	case SLTYPE_SL811HS_R12:
		printf(" (rev 1.2)");
		break;
	case SLTYPE_SL811HS_R14:
		printf(" (rev 1.4)");
		break;
	default:
		printf(" (unknown revision)");
		break;
	}
	printf("\n");
	

	/* Initialize sc */
	sc->sc_bus.usbrev = USBREV_1_1;
	sc->sc_bus.methods = &slhci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct slhci_pipe);
	sc->sc_bus.parent_dmatag = NULL; /* XXX */
	sc->sc_bus.buffer_dmatag = NULL; /* XXX */
	STAILQ_INIT(&sc->sc_free_xfers);

	usb_callout_init(sc->sc_poll_handle);

	/* Disable interrupt, then wait 40msec */
	sl11write(sc, SL11_IER, 0x00);
	delay_ms(40);

	/* Initialize controller */
	sl11write(sc, SL811_CSOF, SL811_CSOF_MASTER | 0x2e);
	delay_ms(40);

	sl11write(sc, SL11_ISR, 0xff);

	/* Disable interrupt, then wait 40msec */
	sl11write(sc, SL11_IER, 0x00);

	/* Reset USB engine */
	sl11write(sc, SL11_CTRL, SL11_CTRL_JKSTATE| SL11_CTRL_RESETENGINE);
	delay_ms(40);
	sl11write(sc, SL11_CTRL, 0x00);
	delay_ms(10);

	/* USB Bus reset for GET_PORT_STATUS */
	sl11_reset(sc);
	sc->sc_flags = SLF_ATTACHED;

	/* Enable interrupt */
	sl11write(sc, SL11_IER, SL11_IER_INSERT);
	/* x68k Nereid USB controller needs it */
	if (sc->sc_enable_intr)
		sc->sc_enable_intr(sc->sc_arg, INTR_ON);
#ifdef USB_DEBUG
	usbdebug = 0;
#endif

	return 0;
}

int
slhci_intr(void *arg)
{
	struct slhci_softc *sc = arg;
	u_int8_t r;
#ifdef SLHCI_DEBUG
	char bitbuf[256];
#endif


	if((sc->sc_flags & SLF_ATTACHED) == 0)
	  return 0;

	r = sl11read(sc, SL11_ISR);



	sl11write(sc, SL11_ISR, SL11_ISR_DATA | SL11_ISR_SOFTIMER);

	if ((r & SL11_ISR_RESET)) {
		sc->sc_flags |= SLF_RESET;
		sl11write(sc, SL11_ISR, SL11_ISR_RESET);
	}
	if ((r & SL11_ISR_INSERT)) {
		sc->sc_flags |= SLF_INSERT;
		sl11write(sc, SL11_ISR, SL11_ISR_INSERT);
	}

#ifdef SLHCI_DEBUG
	bitmask_snprintf(r,
		(sl11read(sc, SL11_CTRL) & SL11_CTRL_SUSPEND)
		? "\20\x8""D+\7RESUME\6INSERT\5SOF\4res\3""BABBLE\2USBB\1USBA"
		: "\20\x8""D+\7RESET\6INSERT\5SOF\4res\3""BABBLE\2USBB\1USBA",
		bitbuf, sizeof(bitbuf));
	DPRINTF(D_XFER, ("I=%s ", bitbuf));
#endif /* SLHCI_DEBUG */

	return 0;
}

usbd_status
slhci_open(usbd_pipe_handle pipe)
{
	usbd_device_handle dev = pipe->device;
	struct slhci_softc *sc = (struct slhci_softc *)dev->bus;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;

	DPRINTF(D_TRACE, ("slhci_open(addr=%d,ep=%d,scaddr=%d)",
		dev->address, ed->bEndpointAddress, sc->sc_addr));

	if (dev->address == sc->sc_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &slhci_root_ctrl_methods;
			break;
		case UE_DIR_IN | SLHCI_INTR_ENDPT:
			pipe->methods = &slhci_root_intr_methods;
			break;
		default:
			printf("open:endpointErr!\n");
			return USBD_INVAL;
		}
	} else {
		switch (ed->bmAttributes & UE_XFERTYPE) {
		case UE_CONTROL:
			DPRINTF(D_MSG, ("control "));
			pipe->methods = &slhci_device_ctrl_methods;
			break;
		case UE_INTERRUPT:
			DPRINTF(D_MSG, ("interrupt "));
			pipe->methods = &slhci_device_intr_methods;
			break;
		case UE_ISOCHRONOUS:
			DPRINTF(D_MSG, ("isochronous "));
			pipe->methods = &slhci_device_isoc_methods;
			break;
		case UE_BULK:
			DPRINTF(D_MSG, ("bluk "));
			pipe->methods = &slhci_device_bulk_methods;
			break;
		}
	}
	return USBD_NORMAL_COMPLETION;
}

void
slhci_softintr(void *arg)
{
	DPRINTF(D_TRACE, ("%s()", __FUNCTION__));
}

void
slhci_poll(struct usbd_bus *bus)
{
	DPRINTF(D_TRACE, ("%s()", __FUNCTION__));
}

/*
 * Emulation of interrupt transfer for status change endpoint
 * of root hub.
 */
void
slhci_poll_hub(void *arg)
{
	usbd_xfer_handle xfer = arg;
	usbd_pipe_handle pipe = xfer->pipe;
	struct slhci_softc *sc = (struct slhci_softc *)pipe->device->bus;
	int s;
	u_char *p;

	usb_callout(sc->sc_poll_handle, sc->sc_interval, slhci_poll_hub, xfer);

	/* USB spec 11.13.3 (p.260) */
	p = xfer->buffer;
	p[0] = 0;
	if ((sc->sc_flags & (SLF_INSERT | SLF_RESET))) {
		p[0] = 2;
		DPRINTF(D_TRACE, ("!"));
	}

	/* no change, return NAK */
	if (p[0] == 0)
		return;

	xfer->actlen = 1;
	xfer->status = USBD_NORMAL_COMPLETION;
	s = splusb();
	xfer->device->bus->intr_context++;
	usb_transfer_complete(xfer);
	xfer->device->bus->intr_context--;
	splx(s);
}

usbd_status
slhci_allocm(struct usbd_bus *bus, usb_dma_t *dma, u_int32_t size)
{
	struct slhci_softc *sc = (struct slhci_softc *)bus;

	DPRINTF(D_MEM, ("SLallocm"));
	return usb_allocmem(&sc->sc_bus, size, 0, dma);
}

void
slhci_freem(struct usbd_bus *bus, usb_dma_t *dma)
{
	struct slhci_softc *sc = (struct slhci_softc *)bus;

	DPRINTF(D_MEM, ("SLfreem"));
	usb_freemem(&sc->sc_bus, dma);
}

usbd_xfer_handle
slhci_allocx(struct usbd_bus *bus)
{
	struct slhci_softc *sc = (struct slhci_softc *)bus;
	usbd_xfer_handle xfer;

	DPRINTF(D_MEM, ("SLallocx"));

	xfer = STAILQ_FIRST(&sc->sc_free_xfers);
	if (xfer) {
		STAILQ_REMOVE_HEAD(&sc->sc_free_xfers, next);
#ifdef DIAGNOSTIC
		if (xfer->busy_free != XFER_FREE) {
			printf("slhci_allocx: xfer=%p not free, 0x%08x\n",
				xfer, xfer->busy_free);
		}
#endif
	} else {
		xfer = malloc(sizeof(*xfer), M_USB, M_NOWAIT);
	}

	if (xfer) {
		memset(xfer, 0, sizeof(*xfer));
#ifdef DIAGNOSTIC
		xfer->busy_free = XFER_BUSY;
#endif
	}

	return xfer;
}

void
slhci_freex(struct usbd_bus *bus, usbd_xfer_handle xfer)
{
	struct slhci_softc *sc = (struct slhci_softc *)bus;

	DPRINTF(D_MEM, ("SLfreex"));

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		printf("slhci_freex: xfer=%p not busy, 0x%08x\n",
			xfer, xfer->busy_free);
		return;
	}
	xfer->busy_free = XFER_FREE;
#endif
	STAILQ_INSERT_HEAD(&sc->sc_free_xfers, xfer, next);
}

void
slhci_noop(usbd_pipe_handle pipe)
{
	DPRINTF(D_TRACE, ("%s()", __FUNCTION__));
}

/*
 * Data structures and routines to emulate the root hub.
 */
usb_device_descriptor_t slhci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x01, 0x01},			/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	0,			/* protocol */
	64,			/* max packet */
	{USB_VENDOR_SCANLOGIC & 0xff,	/* vendor ID (low)  */
	 USB_VENDOR_SCANLOGIC >> 8  },	/* vendor ID (high) */
	{0} /* ? */,		/* product ID */
	{0},			/* device */
	1,			/* index to manufacturer */
	2,			/* index to product */
	0,			/* index to serial number */
	1			/* number of configurations */
};

usb_config_descriptor_t slhci_confd = {
	USB_CONFIG_DESCRIPTOR_SIZE,
	UDESC_CONFIG,
	{USB_CONFIG_DESCRIPTOR_SIZE +
	 USB_INTERFACE_DESCRIPTOR_SIZE +
	 USB_ENDPOINT_DESCRIPTOR_SIZE},
	1,			/* number of interfaces */
	1,			/* configuration value */
	0,			/* index to configuration */
	UC_SELF_POWERED,	/* attributes */
	15			/* max current is 30mA... */
};

usb_interface_descriptor_t slhci_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,			/* interface number */
	0,			/* alternate setting */
	1,			/* number of endpoint */
	UICLASS_HUB,		/* class */
	UISUBCLASS_HUB,		/* subclass */
	0,			/* protocol */
	0			/* index to interface */
};

usb_endpoint_descriptor_t slhci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | SLHCI_INTR_ENDPT,	/* endpoint address */
	UE_INTERRUPT,		/* attributes */
	{8},			/* max packet size */
	255			/* interval */
};

usb_hub_descriptor_t slhci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	1,			/* number of ports */
	{UHD_PWR_INDIVIDUAL | UHD_OC_NONE, 0},	/* hub characteristics */
	20 /* ? */,		/* 5:power on to power good */
	50,			/* 6:maximum current */
	{ 0x00 },		/* both ports are removable */
	{ 0x00 }		/* port power control mask */
};

static int
slhci_str(usb_string_descriptor_t *p, int l, const char *s)
{
	int i;

	if (l == 0)
		return 0;
	p->bLength = 2 * strlen(s) + 2;
	if (l == 1)
		return 1;
	p->bDescriptorType = UDESC_STRING;
	l -= 2;
	for (i = 0; s[i] && l > 1; i++, l -= 2)
		USETW2(p->bString[i], 0, s[i]);
	return 2 * i + 2;
}

usbd_status
slhci_root_ctrl_transfer(usbd_xfer_handle xfer)
{
	usbd_status error;

	DPRINTF(D_TRACE, ("SLRCtrans "));

	/* Insert last in queue */
	error = usb_insert_transfer(xfer);
	if (error) {
		DPRINTF(D_MSG, ("usb_insert_transfer returns err! "));
		return error;
	}

	/*
	 * Pipe isn't running (otherwise error would be USBD_INPROG),
	 * so start it first.
	 */
	return slhci_root_ctrl_start(STAILQ_FIRST(&xfer->pipe->queue));
}

usbd_status
slhci_root_ctrl_start(usbd_xfer_handle xfer)
{
	struct slhci_softc *sc = (struct slhci_softc *)xfer->pipe->device->bus;
	usb_device_request_t *req;
	int len, value, index, l, s, status;
	int totlen = 0;
	void *buf = NULL;
	usb_port_status_t ps;
	usbd_status error;
	char slbuf[50];
	u_int8_t r;

	DPRINTF(D_TRACE, ("SLRCstart "));

	req = &xfer->request;

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	if (len)
		buf = xfer->buffer;

#ifdef SLHCI_DEBUG
	if ((slhci_debug & D_TRACE))
		print_req_hub(req);
#endif

#define C(x,y) ((x) | ((y) << 8))
	switch (C(req->bRequest, req->bmRequestType)) {
	case C(UR_CLEAR_FEATURE, UT_WRITE_DEVICE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		DPRINTF(D_MSG, ("UR_CLEAR_FEATURE "));
		break;
	case C(UR_GET_CONFIG, UT_READ_DEVICE):
		DPRINTF(D_MSG, ("UR_GET_CONFIG "));
		if (len > 0) {
			*(u_int8_t *)buf = sc->sc_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		switch (value >> 8) {
		case UDESC_DEVICE:
			DPRINTF(D_MSG, ("UDESC_DEVICE "));
			if ((value & 0xff) != 0) {
				error = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			memcpy(buf, &slhci_devd, l);
			break;
		case UDESC_CONFIG:
			DPRINTF(D_MSG, ("UDESC_CONFIG "));
			if ((value & 0xff) != 0) {
				error = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &slhci_confd, l);
			buf = (char *)buf + l;
			len -= l;

			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &slhci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;

			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &slhci_endpd, l);
			break;
		case UDESC_STRING:
			DPRINTF(D_MSG, ("UDESC_STR "));
			if (len == 0)
				break;
			*(u_int8_t *)buf = 0;
			totlen = 1;
			switch (value & 0xff) {
			case 0:
				break;
			case 1:	/* Vendor */
				totlen = slhci_str(buf, len, "ScanLogic");
				break;
			case 2:	/* Product */
				snprintf(slbuf, sizeof(slbuf), "%s root hub",
				    sltypestr[sc->sc_sltype>0]);
				totlen = slhci_str(buf, len, slbuf);
				break;
			default:
				printf("strerr%d ", value & 0xff);
				break;
			}
			break;
		default:
			printf("unknownGetDescriptor=%x", value);
			error = USBD_IOERROR;
			break;
		}
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE):
		/* Get Interface, 9.4.4 */
		if (len > 0) {
			*(u_int8_t *)buf = 0;
			totlen = 1;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_DEVICE):
		/* Get Status from device, 9.4.5 */
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus, UDS_SELF_POWERED);
			totlen = 2;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_ENDPOINT):
		/* Get Status from interface, endpoint, 9.4.5 */
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus, 0);
			totlen = 2;
		}
		break;
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		/* Set Address, 9.4.6 */
		DPRINTF(D_MSG, ("UR_SET_ADDRESS "));
		if (value >= USB_MAX_DEVICES) {
			error = USBD_IOERROR;
			goto ret;
		}
		sc->sc_addr = value;
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		/* Set Configuration, 9.4.7 */
		DPRINTF(D_MSG, ("UR_SET_CONFIG "));
		if (value != 0 && value != 1) {
			error = USBD_IOERROR;
			goto ret;
		}
		sc->sc_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		/* Set Descriptor, 9.4.8, not supported */
		DPRINTF(D_MSG, ("UR_SET_DESCRIPTOR,WRITE_DEVICE not supported\n"));
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		/* Set Feature, 9.4.9, not supported */
		DPRINTF(D_MSG, ("UR_SET_FEATURE not supported\n"));
		error = USBD_IOERROR;
		break;
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		/* Set Interface, 9.4.10, not supported */
		break;
	case C(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		/* Synch Frame, 9.4.11, not supported */
		break;

	/*
	 * Hub specific requests
	 */
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_DEVICE):
		/* Clear Hub Feature, 11.16.2.1, not supported */
		DPRINTF(D_MSG, ("ClearHubFeature not supported\n"));
		break;
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
		/* Clear Port Feature, 11.16.2.2 */
		if (index != 1) {
			error = USBD_IOERROR;
			goto ret;
		}
		switch (value) {
		case UHF_PORT_POWER:
			DPRINTF(D_MSG, ("POWER_OFF "));
			sc->sc_powerstat = POWER_OFF;
			/* x68k Nereid USB controller needs it */
			if (sc->sc_enable_power)
				sc->sc_enable_power(sc, sc->sc_powerstat);
			break;
		case UHF_PORT_SUSPEND:
			DPRINTF(D_MSG, ("SUSPEND "));
			sl11write(sc, SL11_CTRL,
				sl11read(sc, SL11_CTRL) & ~SL11_CTRL_SUSPEND);
			break;
		case UHF_C_PORT_CONNECTION:
			sc->sc_change &= ~UPS_C_CONNECT_STATUS;
			break;
		case UHF_C_PORT_RESET:
			sc->sc_change &= ~UPS_C_PORT_RESET;
			break;
		case UHF_PORT_ENABLE:
			break;
		case UHF_C_PORT_SUSPEND:
		case UHF_C_PORT_ENABLE:
		case UHF_C_PORT_OVER_CURRENT:
		default:
			printf("ClrPortFeatERR:value=0x%x ", value);
			error = USBD_IOERROR;
			break;
		}
		//DPRINTF(D_XFER, ("CH=%04x ", sc->sc_change));
		break;
	case C(UR_GET_BUS_STATE, UT_READ_CLASS_OTHER):
		/* Get Bus State, 11.16.2.3, not supported */
		/* shall return a STALL... */
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		/* Get Hub Descriptor, 11.16.2.4 */
		if (value != 0) {
			error = USBD_IOERROR;
			goto ret;
		}
		l = min(len, USB_HUB_DESCRIPTOR_SIZE);
		totlen = l;
		memcpy(buf, &slhci_hubd, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		/* Get Hub Status, 11.16.2.5 */
		DPRINTF(D_MSG, ("UR_GET_STATUS RCD"));
		if (len != 4) {
			error = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len);
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		/* Get Port Status, 11.16.2.6 */
		if (index != 1 || len != 4) {
			printf("index=%d,len=%d ", index, len);
			error = USBD_IOERROR;
			goto ret;
		}
		/*
		 * change
		 * o port is always enabled.
		 * o cannot detect over current.
		 */
		s = splusb();
		sc->sc_change &= ~(UPS_C_CONNECT_STATUS | UPS_C_PORT_RESET);
		if ((sc->sc_flags & SLF_INSERT)) {
			sc->sc_flags &= ~SLF_INSERT;
			sc->sc_change |= UPS_C_CONNECT_STATUS;
		}
		if ((sc->sc_flags & SLF_RESET)) {
			sc->sc_flags &= ~SLF_RESET;
			sc->sc_change |= UPS_C_PORT_RESET;
		}
		splx(s);
		/*
		 * XXX It can recognize that device is detached,
		 * while there is sl11_speed() here.
		 */
		if (sc->sc_change)
			sl11_speed(sc);
		/*
		 * status
		 * o port is always enabled.
		 * o cannot detect over current.
		 */
		status = 0;
		if (sc->sc_connect)
			status |= UPS_CURRENT_CONNECT_STATUS | UPS_PORT_ENABLED;
		r = sl11read(sc, SL11_CTRL);
		if (r & SL11_CTRL_SUSPEND)
			status |= UPS_SUSPEND;
		if (sc->sc_powerstat)
			status |= UPS_PORT_POWER;
		if (!sc->sc_fullspeed)
			status |= UPS_LOW_SPEED;

		//DPRINTF(D_XFER, ("ST=%04x,CH=%04x ", status, sc->sc_change));
		USETW(ps.wPortStatus, status);
		USETW(ps.wPortChange, sc->sc_change);
		l = min(len, sizeof(ps));
		memcpy(buf, &ps, l);
		totlen = l;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_CLASS_DEVICE):
		/* Set Hub Descriptor, 11.16.2.7, not supported */
		/* STALL ? */
		error = USBD_IOERROR;
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		/* Set Hub Feature, 11.16.2.8, not supported */
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
		/* Set Port Feature, 11.16.2.9 */
		if (index != 1) {
			printf("index=%d ", index);
			error = USBD_IOERROR;
			goto ret;
		}
		switch (value) {
		case UHF_PORT_RESET:
			DPRINTF(D_MSG, ("PORT_RESET "));
			sl11_reset(sc);
			sl11_speed(sc);
			sc->sc_change = 0;
			break;
		case UHF_PORT_POWER:
			DPRINTF(D_MSG, ("PORT_POWER "));
			sc->sc_powerstat = POWER_ON;
			/* x68k Nereid USB controller needs it */
			if (sc->sc_enable_power)
				sc->sc_enable_power(sc, sc->sc_powerstat);
			delay_ms(25);
			break;
		default:
			printf("SetPortFeatERR=0x%x ", value);
			error = USBD_IOERROR;
			break;
		}
		break;
	default:
		DPRINTF(D_MSG, ("ioerr(UR=%02x,UT=%02x) ",
			req->bRequest, req->bmRequestType));
		error = USBD_IOERROR;
		goto ret;
	}
	xfer->actlen = totlen;
	error = USBD_NORMAL_COMPLETION;
 ret:
	xfer->status = error;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
	return USBD_IN_PROGRESS;
}

void
slhci_root_ctrl_abort(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("SLRCabort "));
}

void
slhci_root_ctrl_close(usbd_pipe_handle pipe)
{
	DPRINTF(D_TRACE, ("SLRCclose "));
}

void
slhci_root_ctrl_done(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("SLRCdone\n"));
}

static usbd_status
slhci_root_intr_transfer(usbd_xfer_handle xfer)
{
	usbd_status error;

	DPRINTF(D_TRACE, ("SLRItransfer "));

	/* Insert last in queue */
	error = usb_insert_transfer(xfer);
	if (error)
		return error;

	/*
	 * Pipe isn't running (otherwise error would be USBD_INPROG),
	 * start first.
	 */
	return slhci_root_intr_start(STAILQ_FIRST(&xfer->pipe->queue));
}

static usbd_status
slhci_root_intr_start(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	struct slhci_softc *sc = (struct slhci_softc *)pipe->device->bus;

	DPRINTF(D_TRACE, ("SLRIstart "));

	sc->sc_interval = MS_TO_TICKS(xfer->pipe->endpoint->edesc->bInterval);
	usb_callout(sc->sc_poll_handle, sc->sc_interval, slhci_poll_hub, xfer);
	sc->sc_intr_xfer = xfer;
	return USBD_IN_PROGRESS;
}

static void
slhci_root_intr_abort(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("SLRIabort "));
}

static void
slhci_root_intr_close(usbd_pipe_handle pipe)
{
	struct slhci_softc *sc = (struct slhci_softc *)pipe->device->bus;

	DPRINTF(D_TRACE, ("SLRIclose "));

	usb_uncallout(sc->sc_poll_handle, slhci_poll_hub, sc->sc_intr_xfer);
	sc->sc_intr_xfer = NULL;
}

static void
slhci_root_intr_done(usbd_xfer_handle xfer)
{
	//DPRINTF(D_XFER, ("RIdn "));
}

static usbd_status
slhci_device_ctrl_transfer(usbd_xfer_handle xfer)
{
	usbd_status error;

	DPRINTF(D_TRACE, ("C"));

	error = usb_insert_transfer(xfer);
	if (error)
		return error;

	return slhci_device_ctrl_start(STAILQ_FIRST(&xfer->pipe->queue));
}

static usbd_status
slhci_device_ctrl_start(usbd_xfer_handle xfer)
{
	usb_device_request_t *req = &xfer->request;
	usbd_pipe_handle pipe = xfer->pipe;
	struct slhci_softc *sc = (struct slhci_softc *)pipe->device->bus;
	usbd_status status =  USBD_NORMAL_COMPLETION;
	u_char *buf;
	int pid = SL11_PID_OUT;
	int len, actlen, size;
	int s;
	u_int8_t toggle = 0;

	DPRINTF(D_TRACE, ("st "));
#ifdef SLHCI_DEBUG
	if ((slhci_debug & D_TRACE))
		print_req_hub(req);
#endif

	/* SETUP transaction */
	if (slhci_transaction(sc, pipe, SL11_PID_SETUP,
			sizeof(*req), (u_char*)req, toggle) == -1) {
		status = USBD_IOERROR;
		goto ret;
	}
	toggle ^= SL11_EPCTRL_DATATOGGLE;

	/* DATA transaction */
	actlen = 0;
	len = UGETW(req->wLength);
	if (len) {
		buf = xfer->buffer;
		if (req->bmRequestType & UT_READ)
			pid = SL11_PID_IN;
		for (; actlen < len; ) {
			size = min(len - actlen, 8/* Minimum size */);
			if (slhci_transaction(sc, pipe, pid, size, buf, toggle) == -1)
				break;
			toggle ^= SL11_EPCTRL_DATATOGGLE;
			buf += size;
			actlen += size;
		}
	}
	xfer->actlen = actlen;

	/* ACK (status) */
	if (pid == SL11_PID_IN)
		pid = SL11_PID_OUT;
	else
		pid = SL11_PID_IN;
	if (slhci_transaction(sc, pipe, pid, 0, NULL, toggle) == -1)
		status = USBD_IOERROR;

 ret:
	xfer->status = status;

#ifdef SLHCI_DEBUG
	if((slhci_debug & D_TRACE) && UGETW(req->wLength) > 0){
		int i;
		for(i=0; i < UGETW(req->wLength); i++)
			printf("%02x", ((unsigned char *)xfer->buffer)[i]);
		printf(" ");
	}
#endif
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
	return USBD_IN_PROGRESS;
}

static void
slhci_device_ctrl_abort(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("Cab "));
	slhci_abort_xfer(xfer, USBD_CANCELLED);
}

static void
slhci_device_ctrl_close(usbd_pipe_handle pipe)
{
	DPRINTF(D_TRACE, ("Ccl "));
}

static void
slhci_device_ctrl_done(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("Cdn "));
}

static usbd_status
slhci_device_intr_transfer(usbd_xfer_handle xfer)
{
	usbd_status error;

	DPRINTF(D_TRACE, ("INTRtrans "));

	error = usb_insert_transfer(xfer);
	if (error)
		return error;

	return slhci_device_intr_start(STAILQ_FIRST(&xfer->pipe->queue));
}

static usbd_status
slhci_device_intr_start(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	struct slhci_xfer *sx;

	DPRINTF(D_TRACE, ("INTRstart "));

	sx = malloc(sizeof(*sx), M_USB, M_NOWAIT);
	if (sx == NULL)
		goto reterr;
	memset(sx, 0, sizeof(*sx));
	sx->sx_xfer  = xfer;
	xfer->hcpriv = sx;

	/* initialize callout */
	usb_callout_init(sx->sx_callout_t);
	usb_callout(sx->sx_callout_t,
		MS_TO_TICKS(pipe->endpoint->edesc->bInterval),
		slhci_poll_device, sx);

	/* ACK */
	return USBD_IN_PROGRESS;

 reterr:
	return USBD_IOERROR;
}

static void
slhci_poll_device(void *arg)
{
	struct slhci_xfer *sx = (struct slhci_xfer *)arg;
	usbd_xfer_handle xfer = sx->sx_xfer;
	usbd_pipe_handle pipe = xfer->pipe;
	struct slhci_softc *sc = (struct slhci_softc *)pipe->device->bus;
	void *buf;
	int pid;
	int r;
	int s;

	DPRINTF(D_TRACE, ("pldev"));

	usb_callout(sx->sx_callout_t,
		MS_TO_TICKS(pipe->endpoint->edesc->bInterval),
		slhci_poll_device, sx);

	/* interrupt transfer */
	pid = (UE_GET_DIR(pipe->endpoint->edesc->bEndpointAddress) == UE_DIR_IN)
	    ? SL11_PID_IN : SL11_PID_OUT;
	buf = xfer->buffer;

	r = slhci_transaction(sc, pipe, pid, xfer->length, buf, 0/*toggle*/);
	if (r < 0) {
		DPRINTF(D_MSG, ("%s error", __FUNCTION__));
		return;
	}
	/* no change, return NAK */
	if (r == 0)
		return;

	xfer->status = USBD_NORMAL_COMPLETION;
	s = splusb();
	xfer->device->bus->intr_context++;
	usb_transfer_complete(xfer);
	xfer->device->bus->intr_context--;
	splx(s);
}

static void
slhci_device_intr_abort(usbd_xfer_handle xfer)
{
	struct slhci_xfer *sx;

	DPRINTF(D_TRACE, ("INTRabort "));

	sx = xfer->hcpriv;
	if (sx) {
		usb_uncallout(sx->sx_callout_t, slhci_poll_device, sx);
		free(sx, M_USB);
		xfer->hcpriv = NULL;
	} else {
		printf("%s: sx == NULL!\n", __FUNCTION__);
	}
	slhci_abort_xfer(xfer, USBD_CANCELLED);
}

static void
slhci_device_intr_close(usbd_pipe_handle pipe)
{
	DPRINTF(D_TRACE, ("INTRclose "));
}

static void
slhci_device_intr_done(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("INTRdone "));
}

static usbd_status
slhci_device_isoc_transfer(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("S"));
	return USBD_NORMAL_COMPLETION;
}

static usbd_status
slhci_device_isoc_start(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("st "));
	return USBD_NORMAL_COMPLETION;
}

static void
slhci_device_isoc_abort(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("Sab "));
}

static void
slhci_device_isoc_close(usbd_pipe_handle pipe)
{
	DPRINTF(D_TRACE, ("Scl "));
}

static void
slhci_device_isoc_done(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("Sdn "));
}

static usbd_status
slhci_device_bulk_transfer(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("B"));
	return USBD_NORMAL_COMPLETION;
}

static usbd_status
slhci_device_bulk_start(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("st "));
	return USBD_NORMAL_COMPLETION;
}

static void
slhci_device_bulk_abort(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("Bab "));
}

static void
slhci_device_bulk_close(usbd_pipe_handle pipe)
{
	DPRINTF(D_TRACE, ("Bcl "));
}

static void
slhci_device_bulk_done(usbd_xfer_handle xfer)
{
	DPRINTF(D_TRACE, ("Bdn "));
}

#define DATA0_RD	(0x03)
#define DATA0_WR	(0x07)
#define SLHCI_TIMEOUT	(5000)

/*
 * Do a transaction.
 * return 1 if ACK, 0 if NAK, -1 if error.
 */
static int
slhci_transaction(struct slhci_softc *sc, usbd_pipe_handle pipe,
	u_int8_t pid, int len, u_char *buf, u_int8_t toggle)
{
#ifdef SLHCI_DEBUG
	char str[64];
	int i;
#endif
	int timeout;
	int ls_via_hub = 0;
	int pl;
	u_int8_t isr;
	u_int8_t result = 0;
	u_int8_t devaddr = pipe->device->address;
	u_int8_t endpointaddr = pipe->endpoint->edesc->bEndpointAddress;
	u_int8_t endpoint;
	u_int8_t cmd = DATA0_RD;

	endpoint = UE_GET_ADDR(endpointaddr);
	DPRINTF(D_XFER, ("\n(%x,%d%s%d,%d) ",
		pid, len, (pid == SL11_PID_IN) ? "<-" : "->", devaddr, endpoint));

	/* Set registers */
	sl11write(sc, SL11_E0ADDR, 0x40);
	sl11write(sc, SL11_E0LEN,  len);
	sl11write(sc, SL11_E0PID,  (pid << 4) + endpoint);
	sl11write(sc, SL11_E0DEV,  devaddr);

	/* Set buffer unless PID_IN */
	if (pid != SL11_PID_IN) {
		if (len > 0)
			sl11write_region(sc, 0x40, buf, len);
		cmd = DATA0_WR;
	}

	/* timing ? */
	pl = (len >> 3) + 3;

	/* Low speed device via HUB */
	/* XXX does not work... */
	if ((sc->sc_fullspeed) && pipe->device->speed == USB_SPEED_LOW) {
		pl = len + 16;
		cmd |= SL11_EPCTRL_PREAMBLE;

		/*
		 * SL811HS/T rev 1.2 has a bug, when it got PID_IN
		 * from LowSpeed device via HUB.
		 */
		if (sc->sc_sltype == SLTYPE_SL811HS_R12 && pid == SL11_PID_IN) {
			ls_via_hub = 1;
			DPRINTF(D_MSG, ("LSvH "));
		}
	}

	/* timing ? */
	if (sl11read(sc, SL811_CSOF) <= (u_int8_t)pl)
		cmd |= SL11_EPCTRL_SOF;

	/* Transfer */
	sl11write(sc, SL11_ISR, 0xff);
	sl11write(sc, SL11_E0CTRL, cmd | toggle);

	/* Polling */
	for (timeout = SLHCI_TIMEOUT; timeout; timeout--) {
		isr = sl11read(sc, SL11_ISR);
		if ((isr & SL11_ISR_USBA))
			break;
	}

	/* Check result status */
	result = sl11read(sc, SL11_E0STAT);
	if (!(result & SL11_EPSTAT_NAK) && ls_via_hub) {
		/* Resend PID_IN within 20usec */
		sl11write(sc, SL11_ISR, 0xff);
		sl11write(sc, SL11_E0CTRL, SL11_EPCTRL_ARM);
	}

	sl11write(sc, SL11_ISR, 0xff);

	DPRINTF(D_XFER, ("t=%d i=%x ", SLHCI_TIMEOUT - timeout, isr));
#ifdef SLHCI_DEBUG
	bitmask_snprintf(result,
		"\20\x8STALL\7NAK\6OV\5SETUP\4DATA1\3TIMEOUT\2ERR\1ACK",
		str, sizeof(str));
	DPRINTF(D_XFER, ("STAT=%s ", str));
#endif

	if ((result & SL11_EPSTAT_ERROR))
		return -1;

	if ((result & SL11_EPSTAT_NAK))
		return 0;

	/* Read buffer if PID_IN */
	if (pid == SL11_PID_IN && len > 0) {
		sl11read_region(sc, buf, 0x40, len);
#ifdef SLHCI_DEBUG
		for (i = 0; i < len; i++)
			DPRINTF(D_XFER, ("%02X ", buf[i]));
#endif
	}

	return 1;
}

void
slhci_abort_xfer(usbd_xfer_handle xfer, usbd_status status)
{
	xfer->status = status;
	usb_transfer_complete(xfer);
}

void
slhci_device_clear_toggle(usbd_pipe_handle pipe)
{
	DPRINTF(D_TRACE, ("SLdevice_clear_toggle "));
}

#ifdef SLHCI_DEBUG
void
print_req(usb_device_request_t *r)
{
	char *xmes[]={
		"GETSTAT",
		"CLRFEAT",
		"res",
		"SETFEAT",
		"res",
		"SETADDR",
		"GETDESC",
		"SETDESC",
		"GETCONF",
		"SETCONF",
		"GETIN/F",
		"SETIN/F",
		"SYNC_FR"
	};
	int req, type, value, index, len;

	req   = r->bRequest;
	type  = r->bmRequestType;
	value = UGETW(r->wValue);
	index = UGETW(r->wIndex);
	len   = UGETW(r->wLength);

	printf("%x,%s,v=%d,i=%d,l=%d ",
		type, xmes[req], value, index, len);
}

void
print_req_hub(usb_device_request_t *r)
{
	struct {
		int req;
		int type;
		char *str;
	} conf[] = {
		{ 1, 0x20, "ClrHubFeat"  },
		{ 1, 0x23, "ClrPortFeat" },
		{ 2, 0xa3, "GetBusState" },
		{ 6, 0xa0, "GetHubDesc"  },
		{ 0, 0xa0, "GetHubStat"  },
		{ 0, 0xa3, "GetPortStat" },
		{ 7, 0x20, "SetHubDesc"  },
		{ 3, 0x20, "SetHubFeat"  },
		{ 3, 0x23, "SetPortFeat" },
		{-1, 0, NULL},
	};
	int i;
	int value, index, len;

	value = UGETW(r->wValue);
	index = UGETW(r->wIndex);
	len   = UGETW(r->wLength);
	for (i = 0; ; i++) {
		if (conf[i].req == -1 )
			return print_req(r);
		if (r->bmRequestType == conf[i].type && r->bRequest == conf[i].req) {
			printf("%s", conf[i].str);
			break;
		}
	}
	printf(",v=%d,i=%d,l=%d ", value, index, len);
}

void
print_dumpreg(struct slhci_softc *sc)
{
	printf("00=%02x,01=%02x,02=%02x,03=%02x,04=%02x,"
	       "08=%02x,09=%02x,0A=%02x,0B=%02x,0C=%02x,",
		sl11read(sc, 0),  sl11read(sc, 1),
		sl11read(sc, 2),  sl11read(sc, 3),
		sl11read(sc, 4),  sl11read(sc, 8),
		sl11read(sc, 9),  sl11read(sc, 10),
		sl11read(sc, 11), sl11read(sc, 12)
	);
	printf("CR1=%02x,IER=%02x,0D=%02x,0E=%02x,0F=%02x ",
		sl11read(sc, 5), sl11read(sc, 6),
		sl11read(sc, 13), sl11read(sc, 14), sl11read(sc, 15)
	);
}

void
print_xfer(usbd_xfer_handle xfer)
{
	printf("xfer: length=%d, actlen=%d, flags=%x, timeout=%d,",
		xfer->length, xfer->actlen, xfer->flags, xfer->timeout);
	printf("request{ ");
	print_req_hub(&xfer->request);
	printf("} ");
}
#endif /* SLHCI_DEBUG */
