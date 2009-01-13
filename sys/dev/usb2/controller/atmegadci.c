#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * Copyright (c) 2009 Hans Petter Selasky. All rights reserved.
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

/*
 * This file contains the driver for the ATMEGA series USB Device
 * Controller
 */

/*
 * NOTE: When the chip detects BUS-reset it will also reset the
 * endpoints, Function-address and more.
 */

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_defs.h>

#define	USB_DEBUG_VAR atmegadci_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_sw_transfer.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_hub.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>
#include <dev/usb2/controller/atmegadci.h>

#define	ATMEGA_BUS2SC(bus) \
   ((struct atmegadci_softc *)(((uint8_t *)(bus)) - \
   USB_P2U(&(((struct atmegadci_softc *)0)->sc_bus))))

#define	ATMEGA_PC2SC(pc) \
   ATMEGA_BUS2SC((pc)->tag_parent->info->bus)

#if USB_DEBUG
static int atmegadci_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, atmegadci, CTLFLAG_RW, 0, "USB ATMEGA DCI");
SYSCTL_INT(_hw_usb2_atmegadci, OID_AUTO, debug, CTLFLAG_RW,
    &atmegadci_debug, 0, "ATMEGA DCI debug level");
#endif

#define	ATMEGA_INTR_ENDPT 1

/* prototypes */

struct usb2_bus_methods atmegadci_bus_methods;
struct usb2_pipe_methods atmegadci_device_bulk_methods;
struct usb2_pipe_methods atmegadci_device_ctrl_methods;
struct usb2_pipe_methods atmegadci_device_intr_methods;
struct usb2_pipe_methods atmegadci_device_isoc_fs_methods;
struct usb2_pipe_methods atmegadci_root_ctrl_methods;
struct usb2_pipe_methods atmegadci_root_intr_methods;

static atmegadci_cmd_t atmegadci_setup_rx;
static atmegadci_cmd_t atmegadci_data_rx;
static atmegadci_cmd_t atmegadci_data_tx;
static atmegadci_cmd_t atmegadci_data_tx_sync;
static void atmegadci_device_done(struct usb2_xfer *, usb2_error_t);
static void atmegadci_do_poll(struct usb2_bus *);
static void atmegadci_root_ctrl_poll(struct atmegadci_softc *);
static void atmegadci_standard_done(struct usb2_xfer *);

static usb2_sw_transfer_func_t atmegadci_root_intr_done;
static usb2_sw_transfer_func_t atmegadci_root_ctrl_done;

/*
 * Here is a list of what the chip supports:
 */
static const struct usb2_hw_ep_profile
	atmegadci_ep_profile[2] = {

	[0] = {
		.max_in_frame_size = 64,
		.max_out_frame_size = 64,
		.is_simplex = 1,
		.support_control = 1,
	},
	[1] = {
		.max_in_frame_size = 64,
		.max_out_frame_size = 64,
		.is_simplex = 1,
		.support_multi_buffer = 1,
		.support_bulk = 1,
		.support_interrupt = 1,
		.support_isochronous = 1,
		.support_in = 1,
		.support_out = 1,
	},
};

static void
atmegadci_get_hw_ep_profile(struct usb2_device *udev,
    const struct usb2_hw_ep_profile **ppf, uint8_t ep_addr)
{
	if (ep_addr == 0)
		*ppf = atmegadci_ep_profile;
	else if (ep_addr < ATMEGA_EP_MAX)
		*ppf = atmegadci_ep_profile + 1;
	else
		*ppf = NULL;
}

static void
atmegadci_clocks_on(struct atmegadci_softc *sc)
{
	if (sc->sc_flags.clocks_off &&
	    sc->sc_flags.port_powered) {

		DPRINTFN(5, "\n");

		/* turn on clocks */
		(sc->sc_clocks_on) (&sc->sc_bus);

		ATMEGA_WRITE_1(sc, ATMEGA_USBCON,
		    ATMEGA_USBCON_USBE |
		    ATMEGA_USBCON_OTGPADE |
		    ATMEGA_USBCON_VBUSTE);

		sc->sc_flags.clocks_off = 0;

		/* enable transceiver ? */
	}
}

static void
atmegadci_clocks_off(struct atmegadci_softc *sc)
{
	if (!sc->sc_flags.clocks_off) {

		DPRINTFN(5, "\n");

		/* disable Transceiver ? */

		ATMEGA_WRITE_1(sc, ATMEGA_USBCON,
		    ATMEGA_USBCON_USBE |
		    ATMEGA_USBCON_OTGPADE |
		    ATMEGA_USBCON_FRZCLK |
		    ATMEGA_USBCON_VBUSTE);

		/* turn clocks off */
		(sc->sc_clocks_off) (&sc->sc_bus);

		sc->sc_flags.clocks_off = 1;
	}
}

static void
atmegadci_pull_up(struct atmegadci_softc *sc)
{
	/* pullup D+, if possible */

	if (!sc->sc_flags.d_pulled_up &&
	    sc->sc_flags.port_powered) {
		sc->sc_flags.d_pulled_up = 1;
		ATMEGA_WRITE_1(sc, ATMEGA_UDCON, 0);
	}
}

static void
atmegadci_pull_down(struct atmegadci_softc *sc)
{
	/* pulldown D+, if possible */

	if (sc->sc_flags.d_pulled_up) {
		sc->sc_flags.d_pulled_up = 0;
		ATMEGA_WRITE_1(sc, ATMEGA_UDCON, ATMEGA_UDCON_DETACH);
	}
}

static void
atmegadci_wakeup_peer(struct usb2_xfer *xfer)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(xfer->xroot->bus);
	uint8_t use_polling;
	uint8_t temp;

	if (!sc->sc_flags.status_suspend) {
		return;
	}
	use_polling = mtx_owned(xfer->xroot->xfer_mtx) ? 1 : 0;

	temp = ATMEGA_READ_1(sc, ATMEGA_UDCON);
	ATMEGA_WRITE_1(sc, ATMEGA_UDCON, temp | ATMEGA_UDCON_RMWKUP);

	/* wait 8 milliseconds */
	if (use_polling) {
		/* polling */
		DELAY(8000);
	} else {
		/* Wait for reset to complete. */
		usb2_pause_mtx(&sc->sc_bus.bus_mtx, 8);
	}

	/* hardware should have cleared RMWKUP bit */
}

static void
atmegadci_set_address(struct atmegadci_softc *sc, uint8_t addr)
{
	DPRINTFN(5, "addr=%d\n", addr);

	ATMEGA_WRITE_1(sc, ATMEGA_UDADDR, addr);

	addr |= ATMEGA_UDADDR_ADDEN;

	ATMEGA_WRITE_1(sc, ATMEGA_UDADDR, addr);
}

static uint8_t
atmegadci_setup_rx(struct atmegadci_td *td)
{
	struct atmegadci_softc *sc;
	struct usb2_device_request req;
	uint16_t count;
	uint8_t temp;

	/* get pointer to softc */
	sc = ATMEGA_PC2SC(td->pc);

	/* select endpoint number */
	ATMEGA_WRITE_1(sc, ATMEGA_UENUM, td->ep_no);

	/* check endpoint status */
	temp = ATMEGA_READ_1(sc, ATMEGA_UEINTX);

	DPRINTFN(5, "UEINTX=0x%02x\n", temp);

	if (!(temp & ATMEGA_UEINTX_RXSTPI)) {
		/* abort any ongoing transfer */
		if (!td->did_stall) {
			DPRINTFN(5, "stalling\n");
			ATMEGA_WRITE_1(sc, ATMEGA_UECONX,
			    ATMEGA_UECONX_EPEN |
			    ATMEGA_UECONX_STALLRQ);
			td->did_stall = 1;
		}
		goto not_complete;
	}
	/* get the packet byte count */
	count =
	    (ATMEGA_READ_1(sc, ATMEGA_UEBCHX) << 8) |
	    (ATMEGA_READ_1(sc, ATMEGA_UEBCLX));

	/* mask away undefined bits */
	count &= 0x7FF;

	/* verify data length */
	if (count != td->remainder) {
		DPRINTFN(0, "Invalid SETUP packet "
		    "length, %d bytes\n", count);
		goto not_complete;
	}
	if (count != sizeof(req)) {
		DPRINTFN(0, "Unsupported SETUP packet "
		    "length, %d bytes\n", count);
		goto not_complete;
	}
	/* receive data */
	ATMEGA_READ_MULTI_1(sc, ATMEGA_UEDATX,
	    (void *)&req, sizeof(req));

	/* copy data into real buffer */
	usb2_copy_in(td->pc, 0, &req, sizeof(req));

	td->offset = sizeof(req);
	td->remainder = 0;

	/* sneak peek the set address */
	if ((req.bmRequestType == UT_WRITE_DEVICE) &&
	    (req.bRequest == UR_SET_ADDRESS)) {
		sc->sc_dv_addr = req.wValue[0] & 0x7F;
	} else {
		sc->sc_dv_addr = 0xFF;
	}

	/* clear SETUP packet interrupt */
	ATMEGA_WRITE_1(sc, ATMEGA_UEINTX, ~ATMEGA_UEINTX_RXSTPI);
	return (0);			/* complete */

not_complete:
	/* we only want to know if there is a SETUP packet */
	ATMEGA_WRITE_1(sc, ATMEGA_UEIENX, ATMEGA_UEIENX_RXSTPE);
	return (1);			/* not complete */
}

static uint8_t
atmegadci_data_rx(struct atmegadci_td *td)
{
	struct atmegadci_softc *sc;
	struct usb2_page_search buf_res;
	uint16_t count;
	uint8_t temp;
	uint8_t to;
	uint8_t got_short;

	to = 3;				/* don't loop forever! */
	got_short = 0;

	/* get pointer to softc */
	sc = ATMEGA_PC2SC(td->pc);

	/* select endpoint number */
	ATMEGA_WRITE_1(sc, ATMEGA_UENUM, td->ep_no);

repeat:
	/* check if any of the FIFO banks have data */
	/* check endpoint status */
	temp = ATMEGA_READ_1(sc, ATMEGA_UEINTX);

	DPRINTFN(5, "temp=0x%02x rem=%u\n", temp, td->remainder);

	if (temp & ATMEGA_UEINTX_RXSTPI) {
		if (td->remainder == 0) {
			/*
			 * We are actually complete and have
			 * received the next SETUP
			 */
			DPRINTFN(5, "faking complete\n");
			return (0);	/* complete */
		}
		/*
	         * USB Host Aborted the transfer.
	         */
		td->error = 1;
		return (0);		/* complete */
	}
	/* check status */
	if (!(temp & (ATMEGA_UEINTX_FIFOCON |
	    ATMEGA_UEINTX_RXOUTI))) {
		/* no data */
		goto not_complete;
	}
	/* get the packet byte count */
	count =
	    (ATMEGA_READ_1(sc, ATMEGA_UEBCHX) << 8) |
	    (ATMEGA_READ_1(sc, ATMEGA_UEBCLX));

	/* mask away undefined bits */
	count &= 0x7FF;

	/* verify the packet byte count */
	if (count != td->max_packet_size) {
		if (count < td->max_packet_size) {
			/* we have a short packet */
			td->short_pkt = 1;
			got_short = 1;
		} else {
			/* invalid USB packet */
			td->error = 1;
			return (0);	/* we are complete */
		}
	}
	/* verify the packet byte count */
	if (count > td->remainder) {
		/* invalid USB packet */
		td->error = 1;
		return (0);		/* we are complete */
	}
	while (count > 0) {
		usb2_get_page(td->pc, td->offset, &buf_res);

		/* get correct length */
		if (buf_res.length > count) {
			buf_res.length = count;
		}
		/* receive data */
		ATMEGA_READ_MULTI_1(sc, ATMEGA_UEDATX,
		    buf_res.buffer, buf_res.length);

		/* update counters */
		count -= buf_res.length;
		td->offset += buf_res.length;
		td->remainder -= buf_res.length;
	}

	/* clear OUT packet interrupt */
	ATMEGA_WRITE_1(sc, ATMEGA_UEINTX, ATMEGA_UEINTX_RXOUTI ^ 0xFF);

	/* release FIFO bank */
	ATMEGA_WRITE_1(sc, ATMEGA_UEINTX, ATMEGA_UEINTX_FIFOCON ^ 0xFF);

	/* check if we are complete */
	if ((td->remainder == 0) || got_short) {
		if (td->short_pkt) {
			/* we are complete */
			return (0);
		}
		/* else need to receive a zero length packet */
	}
	if (--to) {
		goto repeat;
	}
not_complete:
	/* we only want to know if there is a SETUP packet or OUT packet */
	ATMEGA_WRITE_1(sc, ATMEGA_UEIENX,
	    ATMEGA_UEIENX_RXSTPE | ATMEGA_UEIENX_RXOUTE);
	return (1);			/* not complete */
}

static uint8_t
atmegadci_data_tx(struct atmegadci_td *td)
{
	struct atmegadci_softc *sc;
	struct usb2_page_search buf_res;
	uint16_t count;
	uint8_t to;
	uint8_t temp;

	to = 3;				/* don't loop forever! */

	/* get pointer to softc */
	sc = ATMEGA_PC2SC(td->pc);

	/* select endpoint number */
	ATMEGA_WRITE_1(sc, ATMEGA_UENUM, td->ep_no);

repeat:

	/* check endpoint status */
	temp = ATMEGA_READ_1(sc, ATMEGA_UEINTX);

	DPRINTFN(5, "temp=0x%02x rem=%u\n", temp, td->remainder);

	if (temp & ATMEGA_UEINTX_RXSTPI) {
		/*
	         * The current transfer was aborted
	         * by the USB Host
	         */
		td->error = 1;
		return (0);		/* complete */
	}
	if (!(temp & (ATMEGA_UEINTX_FIFOCON |
	    ATMEGA_UEINTX_TXINI))) {
		/* cannot write any data */
		goto not_complete;
	}
	count = td->max_packet_size;
	if (td->remainder < count) {
		/* we have a short packet */
		td->short_pkt = 1;
		count = td->remainder;
	}
	while (count > 0) {

		usb2_get_page(td->pc, td->offset, &buf_res);

		/* get correct length */
		if (buf_res.length > count) {
			buf_res.length = count;
		}
		/* transmit data */
		ATMEGA_WRITE_MULTI_1(sc, ATMEGA_UEDATX,
		    buf_res.buffer, buf_res.length);

		/* update counters */
		count -= buf_res.length;
		td->offset += buf_res.length;
		td->remainder -= buf_res.length;
	}

	/* clear IN packet interrupt */
	ATMEGA_WRITE_1(sc, ATMEGA_UEINTX, 0xFF ^ ATMEGA_UEINTX_TXINI);

	/* allocate FIFO bank */
	ATMEGA_WRITE_1(sc, ATMEGA_UEINTX, 0xFF ^ ATMEGA_UEINTX_FIFOCON);

	/* check remainder */
	if (td->remainder == 0) {
		if (td->short_pkt) {
			return (0);	/* complete */
		}
		/* else we need to transmit a short packet */
	}
	if (--to) {
		goto repeat;
	}
not_complete:
	/* we only want to know if there is a SETUP packet or free IN packet */
	ATMEGA_WRITE_1(sc, ATMEGA_UEIENX,
	    ATMEGA_UEIENX_RXSTPE | ATMEGA_UEIENX_TXINE);
	return (1);			/* not complete */
}

static uint8_t
atmegadci_data_tx_sync(struct atmegadci_td *td)
{
	struct atmegadci_softc *sc;
	uint8_t temp;

	/* get pointer to softc */
	sc = ATMEGA_PC2SC(td->pc);

	/* select endpoint number */
	ATMEGA_WRITE_1(sc, ATMEGA_UENUM, td->ep_no);

	/* check endpoint status */
	temp = ATMEGA_READ_1(sc, ATMEGA_UEINTX);

	DPRINTFN(5, "temp=0x%02x\n", temp);

	if (temp & ATMEGA_UEINTX_RXSTPI) {
		DPRINTFN(5, "faking complete\n");
		/* Race condition */
		return (0);		/* complete */
	}
	/*
	 * The control endpoint has only got one bank, so if that bank
	 * is free the packet has been transferred!
	 */
	if (!(temp & (ATMEGA_UEINTX_FIFOCON |
	    ATMEGA_UEINTX_TXINI))) {
		/* cannot write any data */
		goto not_complete;
	}
	if (sc->sc_dv_addr != 0xFF) {
		/* set new address */
		atmegadci_set_address(sc, sc->sc_dv_addr);
	}
	return (0);			/* complete */

not_complete:
	/* we only want to know if there is a SETUP packet or free IN packet */
	ATMEGA_WRITE_1(sc, ATMEGA_UEIENX,
	    ATMEGA_UEIENX_RXSTPE | ATMEGA_UEIENX_TXINE);
	return (1);			/* not complete */
}

static uint8_t
atmegadci_xfer_do_fifo(struct usb2_xfer *xfer)
{
	struct atmegadci_td *td;

	DPRINTFN(9, "\n");

	td = xfer->td_transfer_cache;
	while (1) {
		if ((td->func) (td)) {
			/* operation in progress */
			break;
		}
		if (((void *)td) == xfer->td_transfer_last) {
			goto done;
		}
		if (td->error) {
			goto done;
		} else if (td->remainder > 0) {
			/*
			 * We had a short transfer. If there is no alternate
			 * next, stop processing !
			 */
			if (!td->alt_next) {
				goto done;
			}
		}
		/*
		 * Fetch the next transfer descriptor and transfer
		 * some flags to the next transfer descriptor
		 */
		td = td->obj_next;
		xfer->td_transfer_cache = td;
	}
	return (1);			/* not complete */

done:
	/* compute all actual lengths */

	atmegadci_standard_done(xfer);
	return (0);			/* complete */
}

static void
atmegadci_interrupt_poll(struct atmegadci_softc *sc)
{
	struct usb2_xfer *xfer;

repeat:
	TAILQ_FOREACH(xfer, &sc->sc_bus.intr_q.head, wait_entry) {
		if (!atmegadci_xfer_do_fifo(xfer)) {
			/* queue has been modified */
			goto repeat;
		}
	}
}

static void
atmegadci_vbus_interrupt(struct atmegadci_softc *sc, uint8_t is_on)
{
	DPRINTFN(5, "vbus = %u\n", is_on);

	if (is_on) {
		if (!sc->sc_flags.status_vbus) {
			sc->sc_flags.status_vbus = 1;

			/* complete root HUB interrupt endpoint */

			usb2_sw_transfer(&sc->sc_root_intr,
			    &atmegadci_root_intr_done);
		}
	} else {
		if (sc->sc_flags.status_vbus) {
			sc->sc_flags.status_vbus = 0;
			sc->sc_flags.status_bus_reset = 0;
			sc->sc_flags.status_suspend = 0;
			sc->sc_flags.change_suspend = 0;
			sc->sc_flags.change_connect = 1;

			/* complete root HUB interrupt endpoint */

			usb2_sw_transfer(&sc->sc_root_intr,
			    &atmegadci_root_intr_done);
		}
	}
}

void
atmegadci_interrupt(struct atmegadci_softc *sc)
{
	uint8_t status;

	USB_BUS_LOCK(&sc->sc_bus);

	/* read interrupt status */
	status = ATMEGA_READ_1(sc, ATMEGA_UDINT);

	/* clear all set interrupts */
	ATMEGA_WRITE_1(sc, ATMEGA_UDINT, ~status);

	/* check for any bus state change interrupts */
	if (status & ATMEGA_UDINT_EORSTI) {

		DPRINTFN(5, "end of reset\n");

		/* set correct state */
		sc->sc_flags.status_bus_reset = 1;
		sc->sc_flags.status_suspend = 0;
		sc->sc_flags.change_suspend = 0;
		sc->sc_flags.change_connect = 1;

		/* disable resume interrupt */
		ATMEGA_WRITE_1(sc, ATMEGA_UDIEN,
		    ATMEGA_UDINT_SUSPE |
		    ATMEGA_UDINT_EORSTE);

		/* complete root HUB interrupt endpoint */
		usb2_sw_transfer(&sc->sc_root_intr,
		    &atmegadci_root_intr_done);
	}
	/*
	 * If resume and suspend is set at the same time we interpret
	 * that like RESUME. Resume is set when there is at least 3
	 * milliseconds of inactivity on the USB BUS.
	 */
	if (status & ATMEGA_UDINT_EORSMI) {

		DPRINTFN(5, "resume interrupt\n");

		if (sc->sc_flags.status_suspend) {
			/* update status bits */
			sc->sc_flags.status_suspend = 0;
			sc->sc_flags.change_suspend = 1;

			/* disable resume interrupt */
			ATMEGA_WRITE_1(sc, ATMEGA_UDIEN,
			    ATMEGA_UDINT_SUSPE |
			    ATMEGA_UDINT_EORSTE);

			/* complete root HUB interrupt endpoint */
			usb2_sw_transfer(&sc->sc_root_intr,
			    &atmegadci_root_intr_done);
		}
	} else if (status & ATMEGA_UDINT_SUSPI) {

		DPRINTFN(5, "suspend interrupt\n");

		if (!sc->sc_flags.status_suspend) {
			/* update status bits */
			sc->sc_flags.status_suspend = 1;
			sc->sc_flags.change_suspend = 1;

			/* disable suspend interrupt */
			ATMEGA_WRITE_1(sc, ATMEGA_UDIEN,
			    ATMEGA_UDINT_EORSMI |
			    ATMEGA_UDINT_EORSTE);

			/* complete root HUB interrupt endpoint */
			usb2_sw_transfer(&sc->sc_root_intr,
			    &atmegadci_root_intr_done);
		}
	}
	/* check VBUS */
	status = ATMEGA_READ_1(sc, ATMEGA_USBINT);

	/* clear all set interrupts */
	ATMEGA_WRITE_1(sc, ATMEGA_USBINT, ~status);

	if (status & ATMEGA_USBINT_VBUSTI) {
		uint8_t temp;

		temp = ATMEGA_READ_1(sc, ATMEGA_USBSTA);
		atmegadci_vbus_interrupt(sc, temp & ATMEGA_USBSTA_VBUS);
	}
	/* check for any endpoint interrupts */
	status = ATMEGA_READ_1(sc, ATMEGA_UEINT);

	/* clear all set interrupts */
	ATMEGA_WRITE_1(sc, ATMEGA_UEINT, ~status);

	if (status) {

		DPRINTFN(5, "real endpoint interrupt 0x%02x\n", status);

		atmegadci_interrupt_poll(sc);
	}
	USB_BUS_UNLOCK(&sc->sc_bus);
}

static void
atmegadci_setup_standard_chain_sub(struct atmegadci_std_temp *temp)
{
	struct atmegadci_td *td;

	/* get current Transfer Descriptor */
	td = temp->td_next;
	temp->td = td;

	/* prepare for next TD */
	temp->td_next = td->obj_next;

	/* fill out the Transfer Descriptor */
	td->func = temp->func;
	td->pc = temp->pc;
	td->offset = temp->offset;
	td->remainder = temp->len;
	td->error = 0;
	td->did_stall = 0;
	td->short_pkt = temp->short_pkt;
	td->alt_next = temp->setup_alt_next;
}

static void
atmegadci_setup_standard_chain(struct usb2_xfer *xfer)
{
	struct atmegadci_std_temp temp;
	struct atmegadci_softc *sc;
	struct atmegadci_td *td;
	uint32_t x;
	uint8_t ep_no;
	uint8_t need_sync;

	DPRINTFN(9, "addr=%d endpt=%d sumlen=%d speed=%d\n",
	    xfer->address, UE_GET_ADDR(xfer->endpoint),
	    xfer->sumlen, usb2_get_speed(xfer->xroot->udev));

	temp.max_frame_size = xfer->max_frame_size;

	td = xfer->td_start[0];
	xfer->td_transfer_first = td;
	xfer->td_transfer_cache = td;

	/* setup temp */

	temp.td = NULL;
	temp.td_next = xfer->td_start[0];
	temp.setup_alt_next = xfer->flags_int.short_frames_ok;
	temp.offset = 0;

	sc = ATMEGA_BUS2SC(xfer->xroot->bus);
	ep_no = (xfer->endpoint & UE_ADDR);

	/* check if we should prepend a setup message */

	if (xfer->flags_int.control_xfr) {
		if (xfer->flags_int.control_hdr) {

			temp.func = &atmegadci_setup_rx;
			temp.len = xfer->frlengths[0];
			temp.pc = xfer->frbuffers + 0;
			temp.short_pkt = temp.len ? 1 : 0;

			atmegadci_setup_standard_chain_sub(&temp);
		}
		x = 1;
	} else {
		x = 0;
	}

	if (x != xfer->nframes) {
		if (xfer->endpoint & UE_DIR_IN) {
			temp.func = &atmegadci_data_tx;
			need_sync = 1;
		} else {
			temp.func = &atmegadci_data_rx;
			need_sync = 0;
		}

		/* setup "pc" pointer */
		temp.pc = xfer->frbuffers + x;
	} else {
		need_sync = 0;
	}
	while (x != xfer->nframes) {

		/* DATA0 / DATA1 message */

		temp.len = xfer->frlengths[x];

		x++;

		if (x == xfer->nframes) {
			temp.setup_alt_next = 0;
		}
		if (temp.len == 0) {

			/* make sure that we send an USB packet */

			temp.short_pkt = 0;

		} else {

			/* regular data transfer */

			temp.short_pkt = (xfer->flags.force_short_xfer) ? 0 : 1;
		}

		atmegadci_setup_standard_chain_sub(&temp);

		if (xfer->flags_int.isochronous_xfr) {
			temp.offset += temp.len;
		} else {
			/* get next Page Cache pointer */
			temp.pc = xfer->frbuffers + x;
		}
	}

	/* always setup a valid "pc" pointer for status and sync */
	temp.pc = xfer->frbuffers + 0;

	/* check if we need to sync */
	if (need_sync && xfer->flags_int.control_xfr) {

		/* we need a SYNC point after TX */
		temp.func = &atmegadci_data_tx_sync;
		temp.len = 0;
		temp.short_pkt = 0;

		atmegadci_setup_standard_chain_sub(&temp);
	}
	/* check if we should append a status stage */
	if (xfer->flags_int.control_xfr &&
	    !xfer->flags_int.control_act) {

		/*
		 * Send a DATA1 message and invert the current
		 * endpoint direction.
		 */
		if (xfer->endpoint & UE_DIR_IN) {
			temp.func = &atmegadci_data_rx;
			need_sync = 0;
		} else {
			temp.func = &atmegadci_data_tx;
			need_sync = 1;
		}
		temp.len = 0;
		temp.short_pkt = 0;

		atmegadci_setup_standard_chain_sub(&temp);
		if (need_sync) {
			/* we need a SYNC point after TX */
			temp.func = &atmegadci_data_tx_sync;
			temp.len = 0;
			temp.short_pkt = 0;

			atmegadci_setup_standard_chain_sub(&temp);
		}
	}
	/* must have at least one frame! */
	td = temp.td;
	xfer->td_transfer_last = td;
}

static void
atmegadci_timeout(void *arg)
{
	struct usb2_xfer *xfer = arg;

	DPRINTF("xfer=%p\n", xfer);

	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	/* transfer is transferred */
	atmegadci_device_done(xfer, USB_ERR_TIMEOUT);
}

static void
atmegadci_start_standard_chain(struct usb2_xfer *xfer)
{
	DPRINTFN(9, "\n");

	/* poll one time - will turn on interrupts */
	if (atmegadci_xfer_do_fifo(xfer)) {

		/* put transfer on interrupt queue */
		usb2_transfer_enqueue(&xfer->xroot->bus->intr_q, xfer);

		/* start timeout, if any */
		if (xfer->timeout != 0) {
			usb2_transfer_timeout_ms(xfer,
			    &atmegadci_timeout, xfer->timeout);
		}
	}
}

static void
atmegadci_root_intr_done(struct usb2_xfer *xfer,
    struct usb2_sw_transfer *std)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(xfer->xroot->bus);

	DPRINTFN(9, "\n");

	USB_BUS_LOCK_ASSERT(&sc->sc_bus, MA_OWNED);

	if (std->state != USB_SW_TR_PRE_DATA) {
		if (std->state == USB_SW_TR_PRE_CALLBACK) {
			/* transfer transferred */
			atmegadci_device_done(xfer, std->err);
		}
		goto done;
	}
	/* setup buffer */
	std->ptr = sc->sc_hub_idata;
	std->len = sizeof(sc->sc_hub_idata);

	/* set port bit */
	sc->sc_hub_idata[0] = 0x02;	/* we only have one port */

done:
	return;
}

static usb2_error_t
atmegadci_standard_done_sub(struct usb2_xfer *xfer)
{
	struct atmegadci_td *td;
	uint32_t len;
	uint8_t error;

	DPRINTFN(9, "\n");

	td = xfer->td_transfer_cache;

	do {
		len = td->remainder;

		if (xfer->aframes != xfer->nframes) {
			/*
		         * Verify the length and subtract
		         * the remainder from "frlengths[]":
		         */
			if (len > xfer->frlengths[xfer->aframes]) {
				td->error = 1;
			} else {
				xfer->frlengths[xfer->aframes] -= len;
			}
		}
		/* Check for transfer error */
		if (td->error) {
			/* the transfer is finished */
			error = 1;
			td = NULL;
			break;
		}
		/* Check for short transfer */
		if (len > 0) {
			if (xfer->flags_int.short_frames_ok) {
				/* follow alt next */
				if (td->alt_next) {
					td = td->obj_next;
				} else {
					td = NULL;
				}
			} else {
				/* the transfer is finished */
				td = NULL;
			}
			error = 0;
			break;
		}
		td = td->obj_next;

		/* this USB frame is complete */
		error = 0;
		break;

	} while (0);

	/* update transfer cache */

	xfer->td_transfer_cache = td;

	return (error ?
	    USB_ERR_STALLED : USB_ERR_NORMAL_COMPLETION);
}

static void
atmegadci_standard_done(struct usb2_xfer *xfer)
{
	usb2_error_t err = 0;

	DPRINTFN(13, "xfer=%p pipe=%p transfer done\n",
	    xfer, xfer->pipe);

	/* reset scanner */

	xfer->td_transfer_cache = xfer->td_transfer_first;

	if (xfer->flags_int.control_xfr) {

		if (xfer->flags_int.control_hdr) {

			err = atmegadci_standard_done_sub(xfer);
		}
		xfer->aframes = 1;

		if (xfer->td_transfer_cache == NULL) {
			goto done;
		}
	}
	while (xfer->aframes != xfer->nframes) {

		err = atmegadci_standard_done_sub(xfer);
		xfer->aframes++;

		if (xfer->td_transfer_cache == NULL) {
			goto done;
		}
	}

	if (xfer->flags_int.control_xfr &&
	    !xfer->flags_int.control_act) {

		err = atmegadci_standard_done_sub(xfer);
	}
done:
	atmegadci_device_done(xfer, err);
}

/*------------------------------------------------------------------------*
 *	atmegadci_device_done
 *
 * NOTE: this function can be called more than one time on the
 * same USB transfer!
 *------------------------------------------------------------------------*/
static void
atmegadci_device_done(struct usb2_xfer *xfer, usb2_error_t error)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(xfer->xroot->bus);
	uint8_t ep_no;

	USB_BUS_LOCK_ASSERT(&sc->sc_bus, MA_OWNED);

	DPRINTFN(2, "xfer=%p, pipe=%p, error=%d\n",
	    xfer, xfer->pipe, error);

	if (xfer->flags_int.usb2_mode == USB_MODE_DEVICE) {
		ep_no = (xfer->endpoint & UE_ADDR);

		/* select endpoint number */
		ATMEGA_WRITE_1(sc, ATMEGA_UENUM, ep_no);

		/* disable endpoint interrupt */
		ATMEGA_WRITE_1(sc, ATMEGA_UEIENX, 0);

		DPRINTFN(15, "disabled interrupts!\n");
	}
	/* dequeue transfer and start next transfer */
	usb2_transfer_done(xfer, error);
}

static void
atmegadci_set_stall(struct usb2_device *udev, struct usb2_xfer *xfer,
    struct usb2_pipe *pipe)
{
	struct atmegadci_softc *sc;
	uint8_t ep_no;

	USB_BUS_LOCK_ASSERT(udev->bus, MA_OWNED);

	DPRINTFN(5, "pipe=%p\n", pipe);

	if (xfer) {
		/* cancel any ongoing transfers */
		atmegadci_device_done(xfer, USB_ERR_STALLED);
	}
	sc = ATMEGA_BUS2SC(udev->bus);
	/* get endpoint number */
	ep_no = (pipe->edesc->bEndpointAddress & UE_ADDR);
	/* select endpoint number */
	ATMEGA_WRITE_1(sc, ATMEGA_UENUM, ep_no);
	/* set stall */
	ATMEGA_WRITE_1(sc, ATMEGA_UECONX,
	    ATMEGA_UECONX_EPEN |
	    ATMEGA_UECONX_STALLRQ);
}

static void
atmegadci_clear_stall_sub(struct atmegadci_softc *sc, uint8_t ep_no,
    uint8_t ep_type, uint8_t ep_dir)
{
	uint8_t temp;

	if (ep_type == UE_CONTROL) {
		/* clearing stall is not needed */
		return;
	}
	/* select endpoint number */
	ATMEGA_WRITE_1(sc, ATMEGA_UENUM, ep_no);

	/* set endpoint reset */
	ATMEGA_WRITE_1(sc, ATMEGA_UERST, ATMEGA_UERST_MASK(ep_no));

	/* clear endpoint reset */
	ATMEGA_WRITE_1(sc, ATMEGA_UERST, 0);

	/* set stall */
	ATMEGA_WRITE_1(sc, ATMEGA_UECONX,
	    ATMEGA_UECONX_EPEN |
	    ATMEGA_UECONX_STALLRQ);

	/* reset data toggle */
	ATMEGA_WRITE_1(sc, ATMEGA_UECONX,
	    ATMEGA_UECONX_EPEN |
	    ATMEGA_UECONX_RSTDT);

	/* clear stall */
	ATMEGA_WRITE_1(sc, ATMEGA_UECONX,
	    ATMEGA_UECONX_EPEN |
	    ATMEGA_UECONX_STALLRQC);

	if (ep_type == UE_CONTROL) {
		/* one bank, 64-bytes wMaxPacket */
		ATMEGA_WRITE_1(sc, ATMEGA_UECFG0X,
		    ATMEGA_UECFG0X_EPTYPE0);
		ATMEGA_WRITE_1(sc, ATMEGA_UECFG1X,
		    ATMEGA_UECFG1X_ALLOC |
		    ATMEGA_UECFG1X_EPBK0 |
		    ATMEGA_UECFG1X_EPSIZE(7));
	} else {
		temp = 0;
		if (ep_type == UE_BULK) {
			temp |= ATMEGA_UECFG0X_EPTYPE2;
		} else if (ep_type == UE_INTERRUPT) {
			temp |= ATMEGA_UECFG0X_EPTYPE3;
		} else {
			temp |= ATMEGA_UECFG0X_EPTYPE1;
		}
		if (ep_dir & UE_DIR_IN) {
			temp |= ATMEGA_UECFG0X_EPDIR;
		}
		/* two banks, 64-bytes wMaxPacket */
		ATMEGA_WRITE_1(sc, ATMEGA_UECFG0X, temp);
		ATMEGA_WRITE_1(sc, ATMEGA_UECFG1X,
		    ATMEGA_UECFG1X_ALLOC |
		    ATMEGA_UECFG1X_EPBK1 |
		    ATMEGA_UECFG1X_EPSIZE(7));

		temp = ATMEGA_READ_1(sc, ATMEGA_UESTA0X);
		if (!(temp & ATMEGA_UESTA0X_CFGOK)) {
			DPRINTFN(0, "Chip rejected configuration\n");
		}
	}
}

static void
atmegadci_clear_stall(struct usb2_device *udev, struct usb2_pipe *pipe)
{
	struct atmegadci_softc *sc;
	struct usb2_endpoint_descriptor *ed;

	DPRINTFN(5, "pipe=%p\n", pipe);

	USB_BUS_LOCK_ASSERT(udev->bus, MA_OWNED);

	/* check mode */
	if (udev->flags.usb2_mode != USB_MODE_DEVICE) {
		/* not supported */
		return;
	}
	/* get softc */
	sc = ATMEGA_BUS2SC(udev->bus);

	/* get endpoint descriptor */
	ed = pipe->edesc;

	/* reset endpoint */
	atmegadci_clear_stall_sub(sc,
	    (ed->bEndpointAddress & UE_ADDR),
	    (ed->bmAttributes & UE_XFERTYPE),
	    (ed->bEndpointAddress & (UE_DIR_IN | UE_DIR_OUT)));
}

usb2_error_t
atmegadci_init(struct atmegadci_softc *sc)
{
	uint8_t n;

	DPRINTF("start\n");

	/* set up the bus structure */
	sc->sc_bus.usbrev = USB_REV_1_1;
	sc->sc_bus.methods = &atmegadci_bus_methods;

	USB_BUS_LOCK(&sc->sc_bus);

	/* enable USB PAD regulator */
	ATMEGA_WRITE_1(sc, ATMEGA_UHWCON,
	    ATMEGA_UHWCON_UVREGE);

	/* turn on clocks */
	(sc->sc_clocks_on) (&sc->sc_bus);

	/* wait a little for things to stabilise */
	usb2_pause_mtx(&sc->sc_bus.bus_mtx, 1);

	/* enable interrupts */
	ATMEGA_WRITE_1(sc, ATMEGA_UDIEN,
	    ATMEGA_UDINT_SUSPE |
	    ATMEGA_UDINT_EORSTE);

	/* reset all endpoints */
	ATMEGA_WRITE_1(sc, ATMEGA_UERST,
	    (1 << ATMEGA_EP_MAX) - 1);

	/* disable reset */
	ATMEGA_WRITE_1(sc, ATMEGA_UERST, 0);

	/* disable all endpoints */
	for (n = 1; n != ATMEGA_EP_MAX; n++) {

		/* select endpoint */
		ATMEGA_WRITE_1(sc, ATMEGA_UENUM, n);

		/* disable endpoint interrupt */
		ATMEGA_WRITE_1(sc, ATMEGA_UEIENX, 0);

		/* disable endpoint */
		ATMEGA_WRITE_1(sc, ATMEGA_UECONX, 0);
	}

	/* turn off clocks */

	atmegadci_clocks_off(sc);

	USB_BUS_UNLOCK(&sc->sc_bus);

	/* catch any lost interrupts */

	atmegadci_do_poll(&sc->sc_bus);

	return (0);			/* success */
}

void
atmegadci_uninit(struct atmegadci_softc *sc)
{
	USB_BUS_LOCK(&sc->sc_bus);

	/* turn on clocks */
	(sc->sc_clocks_on) (&sc->sc_bus);

	/* disable interrupts */
	ATMEGA_WRITE_1(sc, ATMEGA_UDIEN, 0);

	/* reset all endpoints */
	ATMEGA_WRITE_1(sc, ATMEGA_UERST,
	    (1 << ATMEGA_EP_MAX) - 1);

	/* disable reset */
	ATMEGA_WRITE_1(sc, ATMEGA_UERST, 0);

	sc->sc_flags.port_powered = 0;
	sc->sc_flags.status_vbus = 0;
	sc->sc_flags.status_bus_reset = 0;
	sc->sc_flags.status_suspend = 0;
	sc->sc_flags.change_suspend = 0;
	sc->sc_flags.change_connect = 1;

	atmegadci_pull_down(sc);
	atmegadci_clocks_off(sc);

	/* disable USB PAD regulator */
	ATMEGA_WRITE_1(sc, ATMEGA_UHWCON, 0);

	USB_BUS_UNLOCK(&sc->sc_bus);
}

void
atmegadci_suspend(struct atmegadci_softc *sc)
{
	return;
}

void
atmegadci_resume(struct atmegadci_softc *sc)
{
	return;
}

static void
atmegadci_do_poll(struct usb2_bus *bus)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(bus);

	USB_BUS_LOCK(&sc->sc_bus);
	atmegadci_interrupt_poll(sc);
	atmegadci_root_ctrl_poll(sc);
	USB_BUS_UNLOCK(&sc->sc_bus);
}

/*------------------------------------------------------------------------*
 * at91dci bulk support
 *------------------------------------------------------------------------*/
static void
atmegadci_device_bulk_open(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_device_bulk_close(struct usb2_xfer *xfer)
{
	atmegadci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
atmegadci_device_bulk_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_device_bulk_start(struct usb2_xfer *xfer)
{
	/* setup TDs */
	atmegadci_setup_standard_chain(xfer);
	atmegadci_start_standard_chain(xfer);
}

struct usb2_pipe_methods atmegadci_device_bulk_methods =
{
	.open = atmegadci_device_bulk_open,
	.close = atmegadci_device_bulk_close,
	.enter = atmegadci_device_bulk_enter,
	.start = atmegadci_device_bulk_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 1,
};

/*------------------------------------------------------------------------*
 * at91dci control support
 *------------------------------------------------------------------------*/
static void
atmegadci_device_ctrl_open(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_device_ctrl_close(struct usb2_xfer *xfer)
{
	atmegadci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
atmegadci_device_ctrl_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_device_ctrl_start(struct usb2_xfer *xfer)
{
	/* setup TDs */
	atmegadci_setup_standard_chain(xfer);
	atmegadci_start_standard_chain(xfer);
}

struct usb2_pipe_methods atmegadci_device_ctrl_methods =
{
	.open = atmegadci_device_ctrl_open,
	.close = atmegadci_device_ctrl_close,
	.enter = atmegadci_device_ctrl_enter,
	.start = atmegadci_device_ctrl_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 1,
};

/*------------------------------------------------------------------------*
 * at91dci interrupt support
 *------------------------------------------------------------------------*/
static void
atmegadci_device_intr_open(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_device_intr_close(struct usb2_xfer *xfer)
{
	atmegadci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
atmegadci_device_intr_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_device_intr_start(struct usb2_xfer *xfer)
{
	/* setup TDs */
	atmegadci_setup_standard_chain(xfer);
	atmegadci_start_standard_chain(xfer);
}

struct usb2_pipe_methods atmegadci_device_intr_methods =
{
	.open = atmegadci_device_intr_open,
	.close = atmegadci_device_intr_close,
	.enter = atmegadci_device_intr_enter,
	.start = atmegadci_device_intr_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 1,
};

/*------------------------------------------------------------------------*
 * at91dci full speed isochronous support
 *------------------------------------------------------------------------*/
static void
atmegadci_device_isoc_fs_open(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_device_isoc_fs_close(struct usb2_xfer *xfer)
{
	atmegadci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
atmegadci_device_isoc_fs_enter(struct usb2_xfer *xfer)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(xfer->xroot->bus);
	uint32_t temp;
	uint32_t nframes;

	DPRINTFN(6, "xfer=%p next=%d nframes=%d\n",
	    xfer, xfer->pipe->isoc_next, xfer->nframes);

	/* get the current frame index */

	nframes =
	    (ATMEGA_READ_1(sc, ATMEGA_UDFNUMH) << 8) |
	    (ATMEGA_READ_1(sc, ATMEGA_UDFNUML));

	nframes &= ATMEGA_FRAME_MASK;

	/*
	 * check if the frame index is within the window where the frames
	 * will be inserted
	 */
	temp = (nframes - xfer->pipe->isoc_next) & ATMEGA_FRAME_MASK;

	if ((xfer->pipe->is_synced == 0) ||
	    (temp < xfer->nframes)) {
		/*
		 * If there is data underflow or the pipe queue is
		 * empty we schedule the transfer a few frames ahead
		 * of the current frame position. Else two isochronous
		 * transfers might overlap.
		 */
		xfer->pipe->isoc_next = (nframes + 3) & ATMEGA_FRAME_MASK;
		xfer->pipe->is_synced = 1;
		DPRINTFN(3, "start next=%d\n", xfer->pipe->isoc_next);
	}
	/*
	 * compute how many milliseconds the insertion is ahead of the
	 * current frame position:
	 */
	temp = (xfer->pipe->isoc_next - nframes) & ATMEGA_FRAME_MASK;

	/*
	 * pre-compute when the isochronous transfer will be finished:
	 */
	xfer->isoc_time_complete =
	    usb2_isoc_time_expand(&sc->sc_bus, nframes) + temp +
	    xfer->nframes;

	/* compute frame number for next insertion */
	xfer->pipe->isoc_next += xfer->nframes;

	/* setup TDs */
	atmegadci_setup_standard_chain(xfer);
}

static void
atmegadci_device_isoc_fs_start(struct usb2_xfer *xfer)
{
	/* start TD chain */
	atmegadci_start_standard_chain(xfer);
}

struct usb2_pipe_methods atmegadci_device_isoc_fs_methods =
{
	.open = atmegadci_device_isoc_fs_open,
	.close = atmegadci_device_isoc_fs_close,
	.enter = atmegadci_device_isoc_fs_enter,
	.start = atmegadci_device_isoc_fs_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 1,
};

/*------------------------------------------------------------------------*
 * at91dci root control support
 *------------------------------------------------------------------------*
 * simulate a hardware HUB by handling
 * all the necessary requests
 *------------------------------------------------------------------------*/

static void
atmegadci_root_ctrl_open(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_root_ctrl_close(struct usb2_xfer *xfer)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(xfer->xroot->bus);

	if (sc->sc_root_ctrl.xfer == xfer) {
		sc->sc_root_ctrl.xfer = NULL;
	}
	atmegadci_device_done(xfer, USB_ERR_CANCELLED);
}

/*
 * USB descriptors for the virtual Root HUB:
 */

static const struct usb2_device_descriptor atmegadci_devd = {
	.bLength = sizeof(struct usb2_device_descriptor),
	.bDescriptorType = UDESC_DEVICE,
	.bcdUSB = {0x00, 0x02},
	.bDeviceClass = UDCLASS_HUB,
	.bDeviceSubClass = UDSUBCLASS_HUB,
	.bDeviceProtocol = UDPROTO_HSHUBSTT,
	.bMaxPacketSize = 64,
	.bcdDevice = {0x00, 0x01},
	.iManufacturer = 1,
	.iProduct = 2,
	.bNumConfigurations = 1,
};

static const struct usb2_device_qualifier atmegadci_odevd = {
	.bLength = sizeof(struct usb2_device_qualifier),
	.bDescriptorType = UDESC_DEVICE_QUALIFIER,
	.bcdUSB = {0x00, 0x02},
	.bDeviceClass = UDCLASS_HUB,
	.bDeviceSubClass = UDSUBCLASS_HUB,
	.bDeviceProtocol = UDPROTO_FSHUB,
	.bMaxPacketSize0 = 0,
	.bNumConfigurations = 0,
};

static const struct atmegadci_config_desc atmegadci_confd = {
	.confd = {
		.bLength = sizeof(struct usb2_config_descriptor),
		.bDescriptorType = UDESC_CONFIG,
		.wTotalLength[0] = sizeof(atmegadci_confd),
		.bNumInterface = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = UC_SELF_POWERED,
		.bMaxPower = 0,
	},
	.ifcd = {
		.bLength = sizeof(struct usb2_interface_descriptor),
		.bDescriptorType = UDESC_INTERFACE,
		.bNumEndpoints = 1,
		.bInterfaceClass = UICLASS_HUB,
		.bInterfaceSubClass = UISUBCLASS_HUB,
		.bInterfaceProtocol = UIPROTO_HSHUBSTT,
	},

	.endpd = {
		.bLength = sizeof(struct usb2_endpoint_descriptor),
		.bDescriptorType = UDESC_ENDPOINT,
		.bEndpointAddress = (UE_DIR_IN | ATMEGA_INTR_ENDPT),
		.bmAttributes = UE_INTERRUPT,
		.wMaxPacketSize[0] = 8,
		.bInterval = 255,
	},
};

static const struct usb2_hub_descriptor_min atmegadci_hubd = {
	.bDescLength = sizeof(atmegadci_hubd),
	.bDescriptorType = UDESC_HUB,
	.bNbrPorts = 1,
	.wHubCharacteristics[0] =
	(UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL) & 0xFF,
	.wHubCharacteristics[1] =
	(UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL) >> 8,
	.bPwrOn2PwrGood = 50,
	.bHubContrCurrent = 0,
	.DeviceRemovable = {0},		/* port is removable */
};

#define	STRING_LANG \
  0x09, 0x04,				/* American English */

#define	STRING_VENDOR \
  'A', 0, 'T', 0, 'M', 0, 'E', 0, 'G', 0, 'A', 0

#define	STRING_PRODUCT \
  'D', 0, 'C', 0, 'I', 0, ' ', 0, 'R', 0, \
  'o', 0, 'o', 0, 't', 0, ' ', 0, 'H', 0, \
  'U', 0, 'B', 0,

USB_MAKE_STRING_DESC(STRING_LANG, atmegadci_langtab);
USB_MAKE_STRING_DESC(STRING_VENDOR, atmegadci_vendor);
USB_MAKE_STRING_DESC(STRING_PRODUCT, atmegadci_product);

static void
atmegadci_root_ctrl_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_root_ctrl_start(struct usb2_xfer *xfer)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(xfer->xroot->bus);

	sc->sc_root_ctrl.xfer = xfer;

	usb2_bus_roothub_exec(xfer->xroot->bus);
}

static void
atmegadci_root_ctrl_task(struct usb2_bus *bus)
{
	atmegadci_root_ctrl_poll(ATMEGA_BUS2SC(bus));
}

static void
atmegadci_root_ctrl_done(struct usb2_xfer *xfer,
    struct usb2_sw_transfer *std)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(xfer->xroot->bus);
	uint16_t value;
	uint16_t index;
	uint8_t use_polling;

	USB_BUS_LOCK_ASSERT(&sc->sc_bus, MA_OWNED);

	if (std->state != USB_SW_TR_SETUP) {
		if (std->state == USB_SW_TR_PRE_CALLBACK) {
			/* transfer transferred */
			atmegadci_device_done(xfer, std->err);
		}
		goto done;
	}
	/* buffer reset */
	std->ptr = USB_ADD_BYTES(&sc->sc_hub_temp, 0);
	std->len = 0;

	value = UGETW(std->req.wValue);
	index = UGETW(std->req.wIndex);

	use_polling = mtx_owned(xfer->xroot->xfer_mtx) ? 1 : 0;

	/* demultiplex the control request */

	switch (std->req.bmRequestType) {
	case UT_READ_DEVICE:
		switch (std->req.bRequest) {
		case UR_GET_DESCRIPTOR:
			goto tr_handle_get_descriptor;
		case UR_GET_CONFIG:
			goto tr_handle_get_config;
		case UR_GET_STATUS:
			goto tr_handle_get_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_DEVICE:
		switch (std->req.bRequest) {
		case UR_SET_ADDRESS:
			goto tr_handle_set_address;
		case UR_SET_CONFIG:
			goto tr_handle_set_config;
		case UR_CLEAR_FEATURE:
			goto tr_valid;	/* nop */
		case UR_SET_DESCRIPTOR:
			goto tr_valid;	/* nop */
		case UR_SET_FEATURE:
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_ENDPOINT:
		switch (std->req.bRequest) {
		case UR_CLEAR_FEATURE:
			switch (UGETW(std->req.wValue)) {
			case UF_ENDPOINT_HALT:
				goto tr_handle_clear_halt;
			case UF_DEVICE_REMOTE_WAKEUP:
				goto tr_handle_clear_wakeup;
			default:
				goto tr_stalled;
			}
			break;
		case UR_SET_FEATURE:
			switch (UGETW(std->req.wValue)) {
			case UF_ENDPOINT_HALT:
				goto tr_handle_set_halt;
			case UF_DEVICE_REMOTE_WAKEUP:
				goto tr_handle_set_wakeup;
			default:
				goto tr_stalled;
			}
			break;
		case UR_SYNCH_FRAME:
			goto tr_valid;	/* nop */
		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_ENDPOINT:
		switch (std->req.bRequest) {
		case UR_GET_STATUS:
			goto tr_handle_get_ep_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_INTERFACE:
		switch (std->req.bRequest) {
		case UR_SET_INTERFACE:
			goto tr_handle_set_interface;
		case UR_CLEAR_FEATURE:
			goto tr_valid;	/* nop */
		case UR_SET_FEATURE:
		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_INTERFACE:
		switch (std->req.bRequest) {
		case UR_GET_INTERFACE:
			goto tr_handle_get_interface;
		case UR_GET_STATUS:
			goto tr_handle_get_iface_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_CLASS_INTERFACE:
	case UT_WRITE_VENDOR_INTERFACE:
		/* XXX forward */
		break;

	case UT_READ_CLASS_INTERFACE:
	case UT_READ_VENDOR_INTERFACE:
		/* XXX forward */
		break;

	case UT_WRITE_CLASS_DEVICE:
		switch (std->req.bRequest) {
		case UR_CLEAR_FEATURE:
			goto tr_valid;
		case UR_SET_DESCRIPTOR:
		case UR_SET_FEATURE:
			break;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_CLASS_OTHER:
		switch (std->req.bRequest) {
		case UR_CLEAR_FEATURE:
			goto tr_handle_clear_port_feature;
		case UR_SET_FEATURE:
			goto tr_handle_set_port_feature;
		case UR_CLEAR_TT_BUFFER:
		case UR_RESET_TT:
		case UR_STOP_TT:
			goto tr_valid;

		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_CLASS_OTHER:
		switch (std->req.bRequest) {
		case UR_GET_TT_STATE:
			goto tr_handle_get_tt_state;
		case UR_GET_STATUS:
			goto tr_handle_get_port_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_CLASS_DEVICE:
		switch (std->req.bRequest) {
		case UR_GET_DESCRIPTOR:
			goto tr_handle_get_class_descriptor;
		case UR_GET_STATUS:
			goto tr_handle_get_class_status;

		default:
			goto tr_stalled;
		}
		break;
	default:
		goto tr_stalled;
	}
	goto tr_valid;

tr_handle_get_descriptor:
	switch (value >> 8) {
	case UDESC_DEVICE:
		if (value & 0xff) {
			goto tr_stalled;
		}
		std->len = sizeof(atmegadci_devd);
		std->ptr = USB_ADD_BYTES(&atmegadci_devd, 0);
		goto tr_valid;
	case UDESC_CONFIG:
		if (value & 0xff) {
			goto tr_stalled;
		}
		std->len = sizeof(atmegadci_confd);
		std->ptr = USB_ADD_BYTES(&atmegadci_confd, 0);
		goto tr_valid;
	case UDESC_STRING:
		switch (value & 0xff) {
		case 0:		/* Language table */
			std->len = sizeof(atmegadci_langtab);
			std->ptr = USB_ADD_BYTES(&atmegadci_langtab, 0);
			goto tr_valid;

		case 1:		/* Vendor */
			std->len = sizeof(atmegadci_vendor);
			std->ptr = USB_ADD_BYTES(&atmegadci_vendor, 0);
			goto tr_valid;

		case 2:		/* Product */
			std->len = sizeof(atmegadci_product);
			std->ptr = USB_ADD_BYTES(&atmegadci_product, 0);
			goto tr_valid;
		default:
			break;
		}
		break;
	default:
		goto tr_stalled;
	}
	goto tr_stalled;

tr_handle_get_config:
	std->len = 1;
	sc->sc_hub_temp.wValue[0] = sc->sc_conf;
	goto tr_valid;

tr_handle_get_status:
	std->len = 2;
	USETW(sc->sc_hub_temp.wValue, UDS_SELF_POWERED);
	goto tr_valid;

tr_handle_set_address:
	if (value & 0xFF00) {
		goto tr_stalled;
	}
	sc->sc_rt_addr = value;
	goto tr_valid;

tr_handle_set_config:
	if (value >= 2) {
		goto tr_stalled;
	}
	sc->sc_conf = value;
	goto tr_valid;

tr_handle_get_interface:
	std->len = 1;
	sc->sc_hub_temp.wValue[0] = 0;
	goto tr_valid;

tr_handle_get_tt_state:
tr_handle_get_class_status:
tr_handle_get_iface_status:
tr_handle_get_ep_status:
	std->len = 2;
	USETW(sc->sc_hub_temp.wValue, 0);
	goto tr_valid;

tr_handle_set_halt:
tr_handle_set_interface:
tr_handle_set_wakeup:
tr_handle_clear_wakeup:
tr_handle_clear_halt:
	goto tr_valid;

tr_handle_clear_port_feature:
	if (index != 1) {
		goto tr_stalled;
	}
	DPRINTFN(9, "UR_CLEAR_PORT_FEATURE on port %d\n", index);

	switch (value) {
	case UHF_PORT_SUSPEND:
		atmegadci_wakeup_peer(xfer);
		break;

	case UHF_PORT_ENABLE:
		sc->sc_flags.port_enabled = 0;
		break;

	case UHF_PORT_TEST:
	case UHF_PORT_INDICATOR:
	case UHF_C_PORT_ENABLE:
	case UHF_C_PORT_OVER_CURRENT:
	case UHF_C_PORT_RESET:
		/* nops */
		break;
	case UHF_PORT_POWER:
		sc->sc_flags.port_powered = 0;
		atmegadci_pull_down(sc);
		atmegadci_clocks_off(sc);
		break;
	case UHF_C_PORT_CONNECTION:
		sc->sc_flags.change_connect = 0;
		break;
	case UHF_C_PORT_SUSPEND:
		sc->sc_flags.change_suspend = 0;
		break;
	default:
		std->err = USB_ERR_IOERROR;
		goto done;
	}
	goto tr_valid;

tr_handle_set_port_feature:
	if (index != 1) {
		goto tr_stalled;
	}
	DPRINTFN(9, "UR_SET_PORT_FEATURE\n");

	switch (value) {
	case UHF_PORT_ENABLE:
		sc->sc_flags.port_enabled = 1;
		break;
	case UHF_PORT_SUSPEND:
	case UHF_PORT_RESET:
	case UHF_PORT_TEST:
	case UHF_PORT_INDICATOR:
		/* nops */
		break;
	case UHF_PORT_POWER:
		sc->sc_flags.port_powered = 1;
		break;
	default:
		std->err = USB_ERR_IOERROR;
		goto done;
	}
	goto tr_valid;

tr_handle_get_port_status:

	DPRINTFN(9, "UR_GET_PORT_STATUS\n");

	if (index != 1) {
		goto tr_stalled;
	}
	if (sc->sc_flags.status_vbus) {
		atmegadci_clocks_on(sc);
		atmegadci_pull_up(sc);
	} else {
		atmegadci_pull_down(sc);
		atmegadci_clocks_off(sc);
	}

	/* Select FULL-speed and Device Side Mode */

	value = UPS_PORT_MODE_DEVICE;

	if (sc->sc_flags.port_powered) {
		value |= UPS_PORT_POWER;
	}
	if (sc->sc_flags.port_enabled) {
		value |= UPS_PORT_ENABLED;
	}
	if (sc->sc_flags.status_vbus &&
	    sc->sc_flags.status_bus_reset) {
		value |= UPS_CURRENT_CONNECT_STATUS;
	}
	if (sc->sc_flags.status_suspend) {
		value |= UPS_SUSPEND;
	}
	USETW(sc->sc_hub_temp.ps.wPortStatus, value);

	value = 0;

	if (sc->sc_flags.change_connect) {
		value |= UPS_C_CONNECT_STATUS;
	}
	if (sc->sc_flags.change_suspend) {
		value |= UPS_C_SUSPEND;
	}
	USETW(sc->sc_hub_temp.ps.wPortChange, value);
	std->len = sizeof(sc->sc_hub_temp.ps);
	goto tr_valid;

tr_handle_get_class_descriptor:
	if (value & 0xFF) {
		goto tr_stalled;
	}
	std->ptr = USB_ADD_BYTES(&atmegadci_hubd, 0);
	std->len = sizeof(atmegadci_hubd);
	goto tr_valid;

tr_stalled:
	std->err = USB_ERR_STALLED;
tr_valid:
done:
	return;
}

static void
atmegadci_root_ctrl_poll(struct atmegadci_softc *sc)
{
	usb2_sw_transfer(&sc->sc_root_ctrl,
	    &atmegadci_root_ctrl_done);
}

struct usb2_pipe_methods atmegadci_root_ctrl_methods =
{
	.open = atmegadci_root_ctrl_open,
	.close = atmegadci_root_ctrl_close,
	.enter = atmegadci_root_ctrl_enter,
	.start = atmegadci_root_ctrl_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 0,
};

/*------------------------------------------------------------------------*
 * at91dci root interrupt support
 *------------------------------------------------------------------------*/
static void
atmegadci_root_intr_open(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_root_intr_close(struct usb2_xfer *xfer)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(xfer->xroot->bus);

	if (sc->sc_root_intr.xfer == xfer) {
		sc->sc_root_intr.xfer = NULL;
	}
	atmegadci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
atmegadci_root_intr_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_root_intr_start(struct usb2_xfer *xfer)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(xfer->xroot->bus);

	sc->sc_root_intr.xfer = xfer;
}

struct usb2_pipe_methods atmegadci_root_intr_methods =
{
	.open = atmegadci_root_intr_open,
	.close = atmegadci_root_intr_close,
	.enter = atmegadci_root_intr_enter,
	.start = atmegadci_root_intr_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 1,
};

static void
atmegadci_xfer_setup(struct usb2_setup_params *parm)
{
	const struct usb2_hw_ep_profile *pf;
	struct atmegadci_softc *sc;
	struct usb2_xfer *xfer;
	void *last_obj;
	uint32_t ntd;
	uint32_t n;
	uint8_t ep_no;

	sc = ATMEGA_BUS2SC(parm->udev->bus);
	xfer = parm->curr_xfer;

	/*
	 * NOTE: This driver does not use any of the parameters that
	 * are computed from the following values. Just set some
	 * reasonable dummies:
	 */
	parm->hc_max_packet_size = 0x500;
	parm->hc_max_packet_count = 1;
	parm->hc_max_frame_size = 0x500;

	usb2_transfer_setup_sub(parm);

	/*
	 * compute maximum number of TDs
	 */
	if (parm->methods == &atmegadci_device_ctrl_methods) {

		ntd = xfer->nframes + 1 /* STATUS */ + 1	/* SYNC 1 */
		    + 1 /* SYNC 2 */ ;

	} else if (parm->methods == &atmegadci_device_bulk_methods) {

		ntd = xfer->nframes + 1 /* SYNC */ ;

	} else if (parm->methods == &atmegadci_device_intr_methods) {

		ntd = xfer->nframes + 1 /* SYNC */ ;

	} else if (parm->methods == &atmegadci_device_isoc_fs_methods) {

		ntd = xfer->nframes + 1 /* SYNC */ ;

	} else {

		ntd = 0;
	}

	/*
	 * check if "usb2_transfer_setup_sub" set an error
	 */
	if (parm->err) {
		return;
	}
	/*
	 * allocate transfer descriptors
	 */
	last_obj = NULL;

	/*
	 * get profile stuff
	 */
	if (ntd) {

		ep_no = xfer->endpoint & UE_ADDR;
		atmegadci_get_hw_ep_profile(parm->udev, &pf, ep_no);

		if (pf == NULL) {
			/* should not happen */
			parm->err = USB_ERR_INVAL;
			return;
		}
	} else {
		ep_no = 0;
		pf = NULL;
	}

	/* align data */
	parm->size[0] += ((-parm->size[0]) & (USB_HOST_ALIGN - 1));

	for (n = 0; n != ntd; n++) {

		struct atmegadci_td *td;

		if (parm->buf) {

			td = USB_ADD_BYTES(parm->buf, parm->size[0]);

			/* init TD */
			td->max_packet_size = xfer->max_packet_size;
			td->ep_no = ep_no;
			if (pf->support_multi_buffer) {
				td->support_multi_buffer = 1;
			}
			td->obj_next = last_obj;

			last_obj = td;
		}
		parm->size[0] += sizeof(*td);
	}

	xfer->td_start[0] = last_obj;
}

static void
atmegadci_xfer_unsetup(struct usb2_xfer *xfer)
{
	return;
}

static void
atmegadci_pipe_init(struct usb2_device *udev, struct usb2_endpoint_descriptor *edesc,
    struct usb2_pipe *pipe)
{
	struct atmegadci_softc *sc = ATMEGA_BUS2SC(udev->bus);

	DPRINTFN(2, "pipe=%p, addr=%d, endpt=%d, mode=%d (%d)\n",
	    pipe, udev->address,
	    edesc->bEndpointAddress, udev->flags.usb2_mode,
	    sc->sc_rt_addr);

	if (udev->device_index == sc->sc_rt_addr) {

		if (udev->flags.usb2_mode != USB_MODE_HOST) {
			/* not supported */
			return;
		}
		switch (edesc->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &atmegadci_root_ctrl_methods;
			break;
		case UE_DIR_IN | ATMEGA_INTR_ENDPT:
			pipe->methods = &atmegadci_root_intr_methods;
			break;
		default:
			/* do nothing */
			break;
		}
	} else {

		if (udev->flags.usb2_mode != USB_MODE_DEVICE) {
			/* not supported */
			return;
		}
		if (udev->speed != USB_SPEED_FULL) {
			/* not supported */
			return;
		}
		switch (edesc->bmAttributes & UE_XFERTYPE) {
		case UE_CONTROL:
			pipe->methods = &atmegadci_device_ctrl_methods;
			break;
		case UE_INTERRUPT:
			pipe->methods = &atmegadci_device_intr_methods;
			break;
		case UE_ISOCHRONOUS:
			pipe->methods = &atmegadci_device_isoc_fs_methods;
			break;
		case UE_BULK:
			pipe->methods = &atmegadci_device_bulk_methods;
			break;
		default:
			/* do nothing */
			break;
		}
	}
}

struct usb2_bus_methods atmegadci_bus_methods =
{
	.pipe_init = &atmegadci_pipe_init,
	.xfer_setup = &atmegadci_xfer_setup,
	.xfer_unsetup = &atmegadci_xfer_unsetup,
	.do_poll = &atmegadci_do_poll,
	.get_hw_ep_profile = &atmegadci_get_hw_ep_profile,
	.set_stall = &atmegadci_set_stall,
	.clear_stall = &atmegadci_clear_stall,
	.roothub_exec = &atmegadci_root_ctrl_task,
};
