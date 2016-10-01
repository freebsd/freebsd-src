/* $FreeBSD$ */
/*-
 * Copyright (c) 2007-2008 Hans Petter Selasky. All rights reserved.
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
 * This file contains the driver for the AT91 series USB Device
 * Controller
 */

/*
 * Thanks to "David Brownell" for helping out regarding the hardware
 * endpoint profiles.
 */

/*
 * NOTE: The "fifo_bank" is not reset in hardware when the endpoint is
 * reset.
 *
 * NOTE: When the chip detects BUS-reset it will also reset the
 * endpoints, Function-address and more.
 */

#ifdef USB_GLOBAL_INCLUDE_FILE
#include USB_GLOBAL_INCLUDE_FILE
#else
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

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>

#define	USB_DEBUG_VAR at91dcidebug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_transfer.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_hub.h>
#include <dev/usb/usb_util.h>

#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#endif			/* USB_GLOBAL_INCLUDE_FILE */

#include <dev/usb/controller/at91dci.h>

#define	AT9100_DCI_BUS2SC(bus) \
   ((struct at91dci_softc *)(((uint8_t *)(bus)) - \
    ((uint8_t *)&(((struct at91dci_softc *)0)->sc_bus))))

#define	AT9100_DCI_PC2SC(pc) \
   AT9100_DCI_BUS2SC(USB_DMATAG_TO_XROOT((pc)->tag_parent)->bus)

#define	AT9100_DCI_THREAD_IRQ \
  (AT91_UDP_INT_BUS | AT91_UDP_INT_END_BR | AT91_UDP_INT_RXRSM | AT91_UDP_INT_RXSUSP)

#ifdef USB_DEBUG
static int at91dcidebug = 0;

static SYSCTL_NODE(_hw_usb, OID_AUTO, at91dci, CTLFLAG_RW, 0, "USB at91dci");
SYSCTL_INT(_hw_usb_at91dci, OID_AUTO, debug, CTLFLAG_RWTUN,
    &at91dcidebug, 0, "at91dci debug level");
#endif

#define	AT9100_DCI_INTR_ENDPT 1

/* prototypes */

static const struct usb_bus_methods at91dci_bus_methods;
static const struct usb_pipe_methods at91dci_device_bulk_methods;
static const struct usb_pipe_methods at91dci_device_ctrl_methods;
static const struct usb_pipe_methods at91dci_device_intr_methods;
static const struct usb_pipe_methods at91dci_device_isoc_fs_methods;

static at91dci_cmd_t at91dci_setup_rx;
static at91dci_cmd_t at91dci_data_rx;
static at91dci_cmd_t at91dci_data_tx;
static at91dci_cmd_t at91dci_data_tx_sync;
static void	at91dci_device_done(struct usb_xfer *, usb_error_t);
static void	at91dci_do_poll(struct usb_bus *);
static void	at91dci_standard_done(struct usb_xfer *);
static void	at91dci_root_intr(struct at91dci_softc *sc);

/*
 * NOTE: Some of the bits in the CSR register have inverse meaning so
 * we need a helper macro when acknowledging events:
 */
#define	AT91_CSR_ACK(csr, what) do {		\
  (csr) &= ~((AT91_UDP_CSR_FORCESTALL|		\
	      AT91_UDP_CSR_TXPKTRDY|		\
	      AT91_UDP_CSR_RXBYTECNT) ^ (what));\
  (csr) |= ((AT91_UDP_CSR_RX_DATA_BK0|		\
	     AT91_UDP_CSR_RX_DATA_BK1|		\
	     AT91_UDP_CSR_TXCOMP|		\
	     AT91_UDP_CSR_RXSETUP|		\
	     AT91_UDP_CSR_STALLSENT) ^ (what));	\
} while (0)

/*
 * Here is a list of what the chip supports.
 * Probably it supports more than listed here!
 */
static const struct usb_hw_ep_profile
	at91dci_ep_profile[AT91_UDP_EP_MAX] = {

	[0] = {
		.max_in_frame_size = 8,
		.max_out_frame_size = 8,
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
	[2] = {
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
	[3] = {
		/* can also do BULK */
		.max_in_frame_size = 8,
		.max_out_frame_size = 8,
		.is_simplex = 1,
		.support_interrupt = 1,
		.support_in = 1,
		.support_out = 1,
	},
	[4] = {
		.max_in_frame_size = 256,
		.max_out_frame_size = 256,
		.is_simplex = 1,
		.support_multi_buffer = 1,
		.support_bulk = 1,
		.support_interrupt = 1,
		.support_isochronous = 1,
		.support_in = 1,
		.support_out = 1,
	},
	[5] = {
		.max_in_frame_size = 256,
		.max_out_frame_size = 256,
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
at91dci_get_hw_ep_profile(struct usb_device *udev,
    const struct usb_hw_ep_profile **ppf, uint8_t ep_addr)
{
	if (ep_addr < AT91_UDP_EP_MAX) {
		*ppf = (at91dci_ep_profile + ep_addr);
	} else {
		*ppf = NULL;
	}
}

static void
at91dci_clocks_on(struct at91dci_softc *sc)
{
	if (sc->sc_flags.clocks_off &&
	    sc->sc_flags.port_powered) {

		DPRINTFN(5, "\n");

		if (sc->sc_clocks_on) {
			(sc->sc_clocks_on) (sc->sc_clocks_arg);
		}
		sc->sc_flags.clocks_off = 0;

		/* enable Transceiver */
		AT91_UDP_WRITE_4(sc, AT91_UDP_TXVC, 0);
	}
}

static void
at91dci_clocks_off(struct at91dci_softc *sc)
{
	if (!sc->sc_flags.clocks_off) {

		DPRINTFN(5, "\n");

		/* disable Transceiver */
		AT91_UDP_WRITE_4(sc, AT91_UDP_TXVC, AT91_UDP_TXVC_DIS);

		if (sc->sc_clocks_off) {
			(sc->sc_clocks_off) (sc->sc_clocks_arg);
		}
		sc->sc_flags.clocks_off = 1;
	}
}

static void
at91dci_pull_up(struct at91dci_softc *sc)
{
	/* pullup D+, if possible */

	if (!sc->sc_flags.d_pulled_up &&
	    sc->sc_flags.port_powered) {
		sc->sc_flags.d_pulled_up = 1;
		(sc->sc_pull_up) (sc->sc_pull_arg);
	}
}

static void
at91dci_pull_down(struct at91dci_softc *sc)
{
	/* pulldown D+, if possible */

	if (sc->sc_flags.d_pulled_up) {
		sc->sc_flags.d_pulled_up = 0;
		(sc->sc_pull_down) (sc->sc_pull_arg);
	}
}

static void
at91dci_wakeup_peer(struct at91dci_softc *sc)
{
	if (!(sc->sc_flags.status_suspend)) {
		return;
	}

	AT91_UDP_WRITE_4(sc, AT91_UDP_GSTATE, AT91_UDP_GSTATE_ESR);

	/* wait 8 milliseconds */
	/* Wait for reset to complete. */
	usb_pause_mtx(&sc->sc_bus.bus_mtx, hz / 125);

	AT91_UDP_WRITE_4(sc, AT91_UDP_GSTATE, 0);
}

static void
at91dci_set_address(struct at91dci_softc *sc, uint8_t addr)
{
	DPRINTFN(5, "addr=%d\n", addr);

	AT91_UDP_WRITE_4(sc, AT91_UDP_FADDR, addr |
	    AT91_UDP_FADDR_EN);
}

static uint8_t
at91dci_setup_rx(struct at91dci_softc *sc, struct at91dci_td *td)
{
	struct usb_device_request req;
	uint32_t csr;
	uint32_t temp;
	uint16_t count;

	/* read out FIFO status */
	csr = AT91_UDP_READ_4(sc, td->status_reg);

	DPRINTFN(5, "csr=0x%08x rem=%u\n", csr, td->remainder);

	temp = csr;
	temp &= (AT91_UDP_CSR_RX_DATA_BK0 |
	    AT91_UDP_CSR_RX_DATA_BK1 |
	    AT91_UDP_CSR_STALLSENT |
	    AT91_UDP_CSR_RXSETUP |
	    AT91_UDP_CSR_TXCOMP);

	if (!(csr & AT91_UDP_CSR_RXSETUP)) {
		goto not_complete;
	}
	/* clear did stall */
	td->did_stall = 0;

	/* get the packet byte count */
	count = (csr & AT91_UDP_CSR_RXBYTECNT) >> 16;

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
	bus_space_read_multi_1(sc->sc_io_tag, sc->sc_io_hdl,
	    td->fifo_reg, (void *)&req, sizeof(req));

	/* copy data into real buffer */
	usbd_copy_in(td->pc, 0, &req, sizeof(req));

	td->offset = sizeof(req);
	td->remainder = 0;

	/* sneak peek the set address */
	if ((req.bmRequestType == UT_WRITE_DEVICE) &&
	    (req.bRequest == UR_SET_ADDRESS)) {
		sc->sc_dv_addr = req.wValue[0] & 0x7F;
	} else {
		sc->sc_dv_addr = 0xFF;
	}

	/* sneak peek the endpoint direction */
	if (req.bmRequestType & UE_DIR_IN) {
		csr |= AT91_UDP_CSR_DIR;
	} else {
		csr &= ~AT91_UDP_CSR_DIR;
	}

	/* write the direction of the control transfer */
	AT91_CSR_ACK(csr, temp);
	AT91_UDP_WRITE_4(sc, td->status_reg, csr);
	return (0);			/* complete */

not_complete:
	/* abort any ongoing transfer */
	if (!td->did_stall) {
		DPRINTFN(5, "stalling\n");
		temp |= AT91_UDP_CSR_FORCESTALL;
		td->did_stall = 1;
	}

	/* clear interrupts, if any */
	if (temp) {
		DPRINTFN(5, "clearing 0x%08x\n", temp);
		AT91_CSR_ACK(csr, temp);
		AT91_UDP_WRITE_4(sc, td->status_reg, csr);
	}
	return (1);			/* not complete */
}

static uint8_t
at91dci_data_rx(struct at91dci_softc *sc, struct at91dci_td *td)
{
	struct usb_page_search buf_res;
	uint32_t csr;
	uint32_t temp;
	uint16_t count;
	uint8_t to;
	uint8_t got_short;

	to = 2;				/* don't loop forever! */
	got_short = 0;

	/* check if any of the FIFO banks have data */
repeat:
	/* read out FIFO status */
	csr = AT91_UDP_READ_4(sc, td->status_reg);

	DPRINTFN(5, "csr=0x%08x rem=%u\n", csr, td->remainder);

	if (csr & AT91_UDP_CSR_RXSETUP) {
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
	/* Make sure that "STALLSENT" gets cleared */
	temp = csr;
	temp &= AT91_UDP_CSR_STALLSENT;

	/* check status */
	if (!(csr & (AT91_UDP_CSR_RX_DATA_BK0 |
	    AT91_UDP_CSR_RX_DATA_BK1))) {
		if (temp) {
			/* write command */
			AT91_CSR_ACK(csr, temp);
			AT91_UDP_WRITE_4(sc, td->status_reg, csr);
		}
		return (1);		/* not complete */
	}
	/* get the packet byte count */
	count = (csr & AT91_UDP_CSR_RXBYTECNT) >> 16;

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
		usbd_get_page(td->pc, td->offset, &buf_res);

		/* get correct length */
		if (buf_res.length > count) {
			buf_res.length = count;
		}
		/* receive data */
		bus_space_read_multi_1(sc->sc_io_tag, sc->sc_io_hdl,
		    td->fifo_reg, buf_res.buffer, buf_res.length);

		/* update counters */
		count -= buf_res.length;
		td->offset += buf_res.length;
		td->remainder -= buf_res.length;
	}

	/* clear status bits */
	if (td->support_multi_buffer) {
		if (td->fifo_bank) {
			td->fifo_bank = 0;
			temp |= AT91_UDP_CSR_RX_DATA_BK1;
		} else {
			td->fifo_bank = 1;
			temp |= AT91_UDP_CSR_RX_DATA_BK0;
		}
	} else {
		temp |= (AT91_UDP_CSR_RX_DATA_BK0 |
		    AT91_UDP_CSR_RX_DATA_BK1);
	}

	/* write command */
	AT91_CSR_ACK(csr, temp);
	AT91_UDP_WRITE_4(sc, td->status_reg, csr);

	/*
	 * NOTE: We may have to delay a little bit before
	 * proceeding after clearing the DATA_BK bits.
	 */

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
	return (1);			/* not complete */
}

static uint8_t
at91dci_data_tx(struct at91dci_softc *sc, struct at91dci_td *td)
{
	struct usb_page_search buf_res;
	uint32_t csr;
	uint32_t temp;
	uint16_t count;
	uint8_t to;

	to = 2;				/* don't loop forever! */

repeat:

	/* read out FIFO status */
	csr = AT91_UDP_READ_4(sc, td->status_reg);

	DPRINTFN(5, "csr=0x%08x rem=%u\n", csr, td->remainder);

	if (csr & AT91_UDP_CSR_RXSETUP) {
		/*
	         * The current transfer was aborted
	         * by the USB Host
	         */
		td->error = 1;
		return (0);		/* complete */
	}
	/* Make sure that "STALLSENT" gets cleared */
	temp = csr;
	temp &= AT91_UDP_CSR_STALLSENT;

	if (csr & AT91_UDP_CSR_TXPKTRDY) {
		if (temp) {
			/* write command */
			AT91_CSR_ACK(csr, temp);
			AT91_UDP_WRITE_4(sc, td->status_reg, csr);
		}
		return (1);		/* not complete */
	} else {
		/* clear TXCOMP and set TXPKTRDY */
		temp |= (AT91_UDP_CSR_TXCOMP |
		    AT91_UDP_CSR_TXPKTRDY);
	}

	count = td->max_packet_size;
	if (td->remainder < count) {
		/* we have a short packet */
		td->short_pkt = 1;
		count = td->remainder;
	}
	while (count > 0) {
		usbd_get_page(td->pc, td->offset, &buf_res);

		/* get correct length */
		if (buf_res.length > count) {
			buf_res.length = count;
		}
		/* transmit data */
		bus_space_write_multi_1(sc->sc_io_tag, sc->sc_io_hdl,
		    td->fifo_reg, buf_res.buffer, buf_res.length);

		/* update counters */
		count -= buf_res.length;
		td->offset += buf_res.length;
		td->remainder -= buf_res.length;
	}

	/* write command */
	AT91_CSR_ACK(csr, temp);
	AT91_UDP_WRITE_4(sc, td->status_reg, csr);

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
	return (1);			/* not complete */
}

static uint8_t
at91dci_data_tx_sync(struct at91dci_softc *sc, struct at91dci_td *td)
{
	uint32_t csr;
	uint32_t temp;

	/* read out FIFO status */
	csr = AT91_UDP_READ_4(sc, td->status_reg);

	DPRINTFN(5, "csr=0x%08x\n", csr);

	if (csr & AT91_UDP_CSR_RXSETUP) {
		DPRINTFN(5, "faking complete\n");
		/* Race condition */
		return (0);		/* complete */
	}
	temp = csr;
	temp &= (AT91_UDP_CSR_STALLSENT |
	    AT91_UDP_CSR_TXCOMP);

	/* check status */
	if (csr & AT91_UDP_CSR_TXPKTRDY) {
		goto not_complete;
	}
	if (!(csr & AT91_UDP_CSR_TXCOMP)) {
		goto not_complete;
	}
	if (td->status_reg == AT91_UDP_CSR(0) && sc->sc_dv_addr != 0xFF) {
		/*
		 * The AT91 has a special requirement with regard to
		 * setting the address and that is to write the new
		 * address before clearing TXCOMP:
		 */
		at91dci_set_address(sc, sc->sc_dv_addr);
	}
	/* write command */
	AT91_CSR_ACK(csr, temp);
	AT91_UDP_WRITE_4(sc, td->status_reg, csr);

	return (0);			/* complete */

not_complete:
	if (temp) {
		/* write command */
		AT91_CSR_ACK(csr, temp);
		AT91_UDP_WRITE_4(sc, td->status_reg, csr);
	}
	return (1);			/* not complete */
}

static void
at91dci_xfer_do_fifo(struct usb_xfer *xfer)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(xfer->xroot->bus);
	struct at91dci_td *td;
	uint8_t temp;

	DPRINTFN(9, "\n");

	td = xfer->td_transfer_cache;
	if (td == NULL)
		return;

	while (1) {
		if ((td->func) (sc, td)) {
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
		temp = 0;
		if (td->fifo_bank)
			temp |= 1;
		td = td->obj_next;
		xfer->td_transfer_cache = td;
		if (temp & 1)
			td->fifo_bank = 1;
	}
	return;

done:
	temp = (xfer->endpointno & UE_ADDR);

	/* update FIFO bank flag and multi buffer */
	if (td->fifo_bank) {
		sc->sc_ep_flags[temp].fifo_bank = 1;
	} else {
		sc->sc_ep_flags[temp].fifo_bank = 0;
	}

	/* compute all actual lengths */
	xfer->td_transfer_cache = NULL;
	sc->sc_xfer_complete = 1;
}

static uint8_t
at91dci_xfer_do_complete(struct usb_xfer *xfer)
{
	struct at91dci_td *td;

	DPRINTFN(9, "\n");
	td = xfer->td_transfer_cache;
	if (td == NULL) {
		/* compute all actual lengths */
		at91dci_standard_done(xfer);
		return(1);
	}
	return (0);
}

static void
at91dci_interrupt_poll_locked(struct at91dci_softc *sc)
{
	struct usb_xfer *xfer;

	TAILQ_FOREACH(xfer, &sc->sc_bus.intr_q.head, wait_entry)
		at91dci_xfer_do_fifo(xfer);
}

static void
at91dci_interrupt_complete_locked(struct at91dci_softc *sc)
{
	struct usb_xfer *xfer;
repeat:
	TAILQ_FOREACH(xfer, &sc->sc_bus.intr_q.head, wait_entry) {
		if (at91dci_xfer_do_complete(xfer))
			goto repeat;
	}
}

void
at91dci_vbus_interrupt(struct at91dci_softc *sc, uint8_t is_on)
{
	DPRINTFN(5, "vbus = %u\n", is_on);

	if (is_on) {
		if (!sc->sc_flags.status_vbus) {
			sc->sc_flags.status_vbus = 1;

			/* complete root HUB interrupt endpoint */
			at91dci_root_intr(sc);
		}
	} else {
		if (sc->sc_flags.status_vbus) {
			sc->sc_flags.status_vbus = 0;
			sc->sc_flags.status_bus_reset = 0;
			sc->sc_flags.status_suspend = 0;
			sc->sc_flags.change_suspend = 0;
			sc->sc_flags.change_connect = 1;

			/* complete root HUB interrupt endpoint */
			at91dci_root_intr(sc);
		}
	}
}

int
at91dci_filter_interrupt(void *arg)
{
	struct at91dci_softc *sc = arg;
	int retval = FILTER_HANDLED;
	uint32_t status;

	USB_BUS_SPIN_LOCK(&sc->sc_bus);

	status = AT91_UDP_READ_4(sc, AT91_UDP_ISR);
	status &= AT91_UDP_INT_DEFAULT;

	if (status & AT9100_DCI_THREAD_IRQ)
		retval = FILTER_SCHEDULE_THREAD;

	/* acknowledge interrupts */
	AT91_UDP_WRITE_4(sc, AT91_UDP_ICR, status & ~AT9100_DCI_THREAD_IRQ);

	/* poll FIFOs, if any */
	at91dci_interrupt_poll_locked(sc);

	if (sc->sc_xfer_complete != 0)
		retval = FILTER_SCHEDULE_THREAD;

	USB_BUS_SPIN_UNLOCK(&sc->sc_bus);

	return (retval);
}

void
at91dci_interrupt(void *arg)
{
	struct at91dci_softc *sc = arg;
	uint32_t status;

	USB_BUS_LOCK(&sc->sc_bus);
	USB_BUS_SPIN_LOCK(&sc->sc_bus);

	status = AT91_UDP_READ_4(sc, AT91_UDP_ISR);
	status &= AT9100_DCI_THREAD_IRQ;

	/* acknowledge interrupts */

	AT91_UDP_WRITE_4(sc, AT91_UDP_ICR, status);

	/* check for any bus state change interrupts */

	if (status & AT91_UDP_INT_BUS) {

		DPRINTFN(5, "real bus interrupt 0x%08x\n", status);

		if (status & AT91_UDP_INT_END_BR) {

			/* set correct state */
			sc->sc_flags.status_bus_reset = 1;
			sc->sc_flags.status_suspend = 0;
			sc->sc_flags.change_suspend = 0;
			sc->sc_flags.change_connect = 1;

			/* disable resume interrupt */
			AT91_UDP_WRITE_4(sc, AT91_UDP_IDR,
			    AT91_UDP_INT_RXRSM);
			/* enable suspend interrupt */
			AT91_UDP_WRITE_4(sc, AT91_UDP_IER,
			    AT91_UDP_INT_RXSUSP);
		}
		/*
	         * If RXRSM and RXSUSP is set at the same time we interpret
	         * that like RESUME. Resume is set when there is at least 3
	         * milliseconds of inactivity on the USB BUS.
	         */
		if (status & AT91_UDP_INT_RXRSM) {
			if (sc->sc_flags.status_suspend) {
				sc->sc_flags.status_suspend = 0;
				sc->sc_flags.change_suspend = 1;

				/* disable resume interrupt */
				AT91_UDP_WRITE_4(sc, AT91_UDP_IDR,
				    AT91_UDP_INT_RXRSM);
				/* enable suspend interrupt */
				AT91_UDP_WRITE_4(sc, AT91_UDP_IER,
				    AT91_UDP_INT_RXSUSP);
			}
		} else if (status & AT91_UDP_INT_RXSUSP) {
			if (!sc->sc_flags.status_suspend) {
				sc->sc_flags.status_suspend = 1;
				sc->sc_flags.change_suspend = 1;

				/* disable suspend interrupt */
				AT91_UDP_WRITE_4(sc, AT91_UDP_IDR,
				    AT91_UDP_INT_RXSUSP);

				/* enable resume interrupt */
				AT91_UDP_WRITE_4(sc, AT91_UDP_IER,
				    AT91_UDP_INT_RXRSM);
			}
		}
		/* complete root HUB interrupt endpoint */
		at91dci_root_intr(sc);
	}

	if (sc->sc_xfer_complete != 0) {
		sc->sc_xfer_complete = 0;
		at91dci_interrupt_complete_locked(sc);
	}
	USB_BUS_SPIN_UNLOCK(&sc->sc_bus);
	USB_BUS_UNLOCK(&sc->sc_bus);
}

static void
at91dci_setup_standard_chain_sub(struct at91dci_std_temp *temp)
{
	struct at91dci_td *td;

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
	td->fifo_bank = 0;
	td->error = 0;
	td->did_stall = temp->did_stall;
	td->short_pkt = temp->short_pkt;
	td->alt_next = temp->setup_alt_next;
}

static void
at91dci_setup_standard_chain(struct usb_xfer *xfer)
{
	struct at91dci_std_temp temp;
	struct at91dci_softc *sc;
	struct at91dci_td *td;
	uint32_t x;
	uint8_t ep_no;
	uint8_t need_sync;

	DPRINTFN(9, "addr=%d endpt=%d sumlen=%d speed=%d\n",
	    xfer->address, UE_GET_ADDR(xfer->endpointno),
	    xfer->sumlen, usbd_get_speed(xfer->xroot->udev));

	temp.max_frame_size = xfer->max_frame_size;

	td = xfer->td_start[0];
	xfer->td_transfer_first = td;
	xfer->td_transfer_cache = td;

	/* setup temp */

	temp.pc = NULL;
	temp.td = NULL;
	temp.td_next = xfer->td_start[0];
	temp.offset = 0;
	temp.setup_alt_next = xfer->flags_int.short_frames_ok ||
	    xfer->flags_int.isochronous_xfr;
	temp.did_stall = !xfer->flags_int.control_stall;

	sc = AT9100_DCI_BUS2SC(xfer->xroot->bus);
	ep_no = (xfer->endpointno & UE_ADDR);

	/* check if we should prepend a setup message */

	if (xfer->flags_int.control_xfr) {
		if (xfer->flags_int.control_hdr) {

			temp.func = &at91dci_setup_rx;
			temp.len = xfer->frlengths[0];
			temp.pc = xfer->frbuffers + 0;
			temp.short_pkt = temp.len ? 1 : 0;
			/* check for last frame */
			if (xfer->nframes == 1) {
				/* no STATUS stage yet, SETUP is last */
				if (xfer->flags_int.control_act)
					temp.setup_alt_next = 0;
			}

			at91dci_setup_standard_chain_sub(&temp);
		}
		x = 1;
	} else {
		x = 0;
	}

	if (x != xfer->nframes) {
		if (xfer->endpointno & UE_DIR_IN) {
			temp.func = &at91dci_data_tx;
			need_sync = 1;
		} else {
			temp.func = &at91dci_data_rx;
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
			if (xfer->flags_int.control_xfr) {
				if (xfer->flags_int.control_act) {
					temp.setup_alt_next = 0;
				}
			} else {
				temp.setup_alt_next = 0;
			}
		}
		if (temp.len == 0) {

			/* make sure that we send an USB packet */

			temp.short_pkt = 0;

		} else {

			/* regular data transfer */

			temp.short_pkt = (xfer->flags.force_short_xfer) ? 0 : 1;
		}

		at91dci_setup_standard_chain_sub(&temp);

		if (xfer->flags_int.isochronous_xfr) {
			temp.offset += temp.len;
		} else {
			/* get next Page Cache pointer */
			temp.pc = xfer->frbuffers + x;
		}
	}

	/* check for control transfer */
	if (xfer->flags_int.control_xfr) {

		/* always setup a valid "pc" pointer for status and sync */
		temp.pc = xfer->frbuffers + 0;
		temp.len = 0;
		temp.short_pkt = 0;
		temp.setup_alt_next = 0;

		/* check if we need to sync */
		if (need_sync) {
			/* we need a SYNC point after TX */
			temp.func = &at91dci_data_tx_sync;
			at91dci_setup_standard_chain_sub(&temp);
		}

		/* check if we should append a status stage */
		if (!xfer->flags_int.control_act) {

			/*
			 * Send a DATA1 message and invert the current
			 * endpoint direction.
			 */
			if (xfer->endpointno & UE_DIR_IN) {
				temp.func = &at91dci_data_rx;
				need_sync = 0;
			} else {
				temp.func = &at91dci_data_tx;
				need_sync = 1;
			}

			at91dci_setup_standard_chain_sub(&temp);
			if (need_sync) {
				/* we need a SYNC point after TX */
				temp.func = &at91dci_data_tx_sync;
				at91dci_setup_standard_chain_sub(&temp);
			}
		}
	}

	/* must have at least one frame! */
	td = temp.td;
	xfer->td_transfer_last = td;

	/* setup the correct fifo bank */
	if (sc->sc_ep_flags[ep_no].fifo_bank) {
		td = xfer->td_transfer_first;
		td->fifo_bank = 1;
	}
}

static void
at91dci_timeout(void *arg)
{
	struct usb_xfer *xfer = arg;

	DPRINTF("xfer=%p\n", xfer);

	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	/* transfer is transferred */
	at91dci_device_done(xfer, USB_ERR_TIMEOUT);
}

static void
at91dci_start_standard_chain(struct usb_xfer *xfer)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(xfer->xroot->bus);

	DPRINTFN(9, "\n");

	USB_BUS_SPIN_LOCK(&sc->sc_bus);

	/* poll one time */
	at91dci_xfer_do_fifo(xfer);

	if (at91dci_xfer_do_complete(xfer) == 0) {

		uint8_t ep_no = xfer->endpointno & UE_ADDR;

		/*
		 * Only enable the endpoint interrupt when we are actually
		 * waiting for data, hence we are dealing with level
		 * triggered interrupts !
		 */
		AT91_UDP_WRITE_4(sc, AT91_UDP_IER, AT91_UDP_INT_EP(ep_no));

		DPRINTFN(15, "enable interrupts on endpoint %d\n", ep_no);

		/* put transfer on interrupt queue */
		usbd_transfer_enqueue(&xfer->xroot->bus->intr_q, xfer);

		/* start timeout, if any */
		if (xfer->timeout != 0) {
			usbd_transfer_timeout_ms(xfer,
			    &at91dci_timeout, xfer->timeout);
		}
	}
	USB_BUS_SPIN_UNLOCK(&sc->sc_bus);
}

static void
at91dci_root_intr(struct at91dci_softc *sc)
{
	DPRINTFN(9, "\n");

	USB_BUS_LOCK_ASSERT(&sc->sc_bus, MA_OWNED);

	/* set port bit */
	sc->sc_hub_idata[0] = 0x02;	/* we only have one port */

	uhub_root_intr(&sc->sc_bus, sc->sc_hub_idata,
	    sizeof(sc->sc_hub_idata));
}

static usb_error_t
at91dci_standard_done_sub(struct usb_xfer *xfer)
{
	struct at91dci_td *td;
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
			if (xfer->flags_int.short_frames_ok ||
			    xfer->flags_int.isochronous_xfr) {
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
at91dci_standard_done(struct usb_xfer *xfer)
{
	usb_error_t err = 0;

	DPRINTFN(13, "xfer=%p endpoint=%p transfer done\n",
	    xfer, xfer->endpoint);

	/* reset scanner */

	xfer->td_transfer_cache = xfer->td_transfer_first;

	if (xfer->flags_int.control_xfr) {

		if (xfer->flags_int.control_hdr) {

			err = at91dci_standard_done_sub(xfer);
		}
		xfer->aframes = 1;

		if (xfer->td_transfer_cache == NULL) {
			goto done;
		}
	}
	while (xfer->aframes != xfer->nframes) {

		err = at91dci_standard_done_sub(xfer);
		xfer->aframes++;

		if (xfer->td_transfer_cache == NULL) {
			goto done;
		}
	}

	if (xfer->flags_int.control_xfr &&
	    !xfer->flags_int.control_act) {

		err = at91dci_standard_done_sub(xfer);
	}
done:
	at91dci_device_done(xfer, err);
}

/*------------------------------------------------------------------------*
 *	at91dci_device_done
 *
 * NOTE: this function can be called more than one time on the
 * same USB transfer!
 *------------------------------------------------------------------------*/
static void
at91dci_device_done(struct usb_xfer *xfer, usb_error_t error)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(xfer->xroot->bus);
	uint8_t ep_no;

	USB_BUS_LOCK_ASSERT(&sc->sc_bus, MA_OWNED);

	DPRINTFN(2, "xfer=%p, endpoint=%p, error=%d\n",
	    xfer, xfer->endpoint, error);

	USB_BUS_SPIN_LOCK(&sc->sc_bus);

	if (xfer->flags_int.usb_mode == USB_MODE_DEVICE) {
		ep_no = (xfer->endpointno & UE_ADDR);

		/* disable endpoint interrupt */
		AT91_UDP_WRITE_4(sc, AT91_UDP_IDR, AT91_UDP_INT_EP(ep_no));

		DPRINTFN(15, "disable interrupts on endpoint %d\n", ep_no);
	}

	/* dequeue transfer and start next transfer */
	usbd_transfer_done(xfer, error);

	USB_BUS_SPIN_UNLOCK(&sc->sc_bus);
}

static void
at91dci_xfer_stall(struct usb_xfer *xfer)
{
	at91dci_device_done(xfer, USB_ERR_STALLED);
}

static void
at91dci_set_stall(struct usb_device *udev,
    struct usb_endpoint *ep, uint8_t *did_stall)
{
	struct at91dci_softc *sc;
	uint32_t csr_val;
	uint8_t csr_reg;

	USB_BUS_LOCK_ASSERT(udev->bus, MA_OWNED);

	DPRINTFN(5, "endpoint=%p\n", ep);

	/* set FORCESTALL */
	sc = AT9100_DCI_BUS2SC(udev->bus);

	USB_BUS_SPIN_LOCK(&sc->sc_bus);
	csr_reg = (ep->edesc->bEndpointAddress & UE_ADDR);
	csr_reg = AT91_UDP_CSR(csr_reg);
	csr_val = AT91_UDP_READ_4(sc, csr_reg);
	AT91_CSR_ACK(csr_val, AT91_UDP_CSR_FORCESTALL);
	AT91_UDP_WRITE_4(sc, csr_reg, csr_val);
	USB_BUS_SPIN_UNLOCK(&sc->sc_bus);
}

static void
at91dci_clear_stall_sub(struct at91dci_softc *sc, uint8_t ep_no,
    uint8_t ep_type, uint8_t ep_dir)
{
	const struct usb_hw_ep_profile *pf;
	uint32_t csr_val;
	uint32_t temp;
	uint8_t csr_reg;
	uint8_t to;

	if (ep_type == UE_CONTROL) {
		/* clearing stall is not needed */
		return;
	}

	USB_BUS_SPIN_LOCK(&sc->sc_bus);

	/* compute CSR register offset */
	csr_reg = AT91_UDP_CSR(ep_no);

	/* compute default CSR value */
	csr_val = 0;
	AT91_CSR_ACK(csr_val, 0);

	/* disable endpoint */
	AT91_UDP_WRITE_4(sc, csr_reg, csr_val);

	/* get endpoint profile */
	at91dci_get_hw_ep_profile(NULL, &pf, ep_no);

	/* reset FIFO */
	AT91_UDP_WRITE_4(sc, AT91_UDP_RST, AT91_UDP_RST_EP(ep_no));
	AT91_UDP_WRITE_4(sc, AT91_UDP_RST, 0);

	/*
	 * NOTE: One would assume that a FIFO reset would release the
	 * FIFO banks as well, but it doesn't! We have to do this
	 * manually!
	 */

	/* release FIFO banks, if any */
	for (to = 0; to != 2; to++) {

		/* get csr value */
		csr_val = AT91_UDP_READ_4(sc, csr_reg);

		if (csr_val & (AT91_UDP_CSR_RX_DATA_BK0 |
		    AT91_UDP_CSR_RX_DATA_BK1)) {
			/* clear status bits */
			if (pf->support_multi_buffer) {
				if (sc->sc_ep_flags[ep_no].fifo_bank) {
					sc->sc_ep_flags[ep_no].fifo_bank = 0;
					temp = AT91_UDP_CSR_RX_DATA_BK1;
				} else {
					sc->sc_ep_flags[ep_no].fifo_bank = 1;
					temp = AT91_UDP_CSR_RX_DATA_BK0;
				}
			} else {
				temp = (AT91_UDP_CSR_RX_DATA_BK0 |
				    AT91_UDP_CSR_RX_DATA_BK1);
			}
		} else {
			temp = 0;
		}

		/* clear FORCESTALL */
		temp |= AT91_UDP_CSR_STALLSENT;

		AT91_CSR_ACK(csr_val, temp);
		AT91_UDP_WRITE_4(sc, csr_reg, csr_val);
	}

	/* compute default CSR value */
	csr_val = 0;
	AT91_CSR_ACK(csr_val, 0);

	/* enable endpoint */
	csr_val &= ~AT91_UDP_CSR_ET_MASK;
	csr_val |= AT91_UDP_CSR_EPEDS;

	if (ep_type == UE_CONTROL) {
		csr_val |= AT91_UDP_CSR_ET_CTRL;
	} else {
		if (ep_type == UE_BULK) {
			csr_val |= AT91_UDP_CSR_ET_BULK;
		} else if (ep_type == UE_INTERRUPT) {
			csr_val |= AT91_UDP_CSR_ET_INT;
		} else {
			csr_val |= AT91_UDP_CSR_ET_ISO;
		}
		if (ep_dir & UE_DIR_IN) {
			csr_val |= AT91_UDP_CSR_ET_DIR_IN;
		}
	}

	/* enable endpoint */
	AT91_UDP_WRITE_4(sc, AT91_UDP_CSR(ep_no), csr_val);

	USB_BUS_SPIN_UNLOCK(&sc->sc_bus);
}

static void
at91dci_clear_stall(struct usb_device *udev, struct usb_endpoint *ep)
{
	struct at91dci_softc *sc;
	struct usb_endpoint_descriptor *ed;

	DPRINTFN(5, "endpoint=%p\n", ep);

	USB_BUS_LOCK_ASSERT(udev->bus, MA_OWNED);

	/* check mode */
	if (udev->flags.usb_mode != USB_MODE_DEVICE) {
		/* not supported */
		return;
	}
	/* get softc */
	sc = AT9100_DCI_BUS2SC(udev->bus);

	/* get endpoint descriptor */
	ed = ep->edesc;

	/* reset endpoint */
	at91dci_clear_stall_sub(sc,
	    (ed->bEndpointAddress & UE_ADDR),
	    (ed->bmAttributes & UE_XFERTYPE),
	    (ed->bEndpointAddress & (UE_DIR_IN | UE_DIR_OUT)));
}

usb_error_t
at91dci_init(struct at91dci_softc *sc)
{
	uint32_t csr_val;
	uint8_t n;

	DPRINTF("start\n");

	/* set up the bus structure */
	sc->sc_bus.usbrev = USB_REV_1_1;
	sc->sc_bus.methods = &at91dci_bus_methods;

	USB_BUS_LOCK(&sc->sc_bus);

	/* turn on clocks */

	if (sc->sc_clocks_on) {
		(sc->sc_clocks_on) (sc->sc_clocks_arg);
	}
	/* wait a little for things to stabilise */
	usb_pause_mtx(&sc->sc_bus.bus_mtx, hz / 1000);

	/* disable and clear all interrupts */

	AT91_UDP_WRITE_4(sc, AT91_UDP_IDR, 0xFFFFFFFF);
	AT91_UDP_WRITE_4(sc, AT91_UDP_ICR, 0xFFFFFFFF);

	/* compute default CSR value */

	csr_val = 0;
	AT91_CSR_ACK(csr_val, 0);

	/* disable all endpoints */

	for (n = 0; n != AT91_UDP_EP_MAX; n++) {

		/* disable endpoint */
		AT91_UDP_WRITE_4(sc, AT91_UDP_CSR(n), csr_val);
	}

	/* enable the control endpoint */

	AT91_CSR_ACK(csr_val, AT91_UDP_CSR_ET_CTRL |
	    AT91_UDP_CSR_EPEDS);

	/* write to FIFO control register */

	AT91_UDP_WRITE_4(sc, AT91_UDP_CSR(0), csr_val);

	/* enable the interrupts we want */

	AT91_UDP_WRITE_4(sc, AT91_UDP_IER, AT91_UDP_INT_BUS);

	/* turn off clocks */

	at91dci_clocks_off(sc);

	USB_BUS_UNLOCK(&sc->sc_bus);

	/* catch any lost interrupts */

	at91dci_do_poll(&sc->sc_bus);

	return (0);			/* success */
}

void
at91dci_uninit(struct at91dci_softc *sc)
{
	USB_BUS_LOCK(&sc->sc_bus);

	/* disable and clear all interrupts */
	AT91_UDP_WRITE_4(sc, AT91_UDP_IDR, 0xFFFFFFFF);
	AT91_UDP_WRITE_4(sc, AT91_UDP_ICR, 0xFFFFFFFF);

	sc->sc_flags.port_powered = 0;
	sc->sc_flags.status_vbus = 0;
	sc->sc_flags.status_bus_reset = 0;
	sc->sc_flags.status_suspend = 0;
	sc->sc_flags.change_suspend = 0;
	sc->sc_flags.change_connect = 1;

	at91dci_pull_down(sc);
	at91dci_clocks_off(sc);
	USB_BUS_UNLOCK(&sc->sc_bus);
}

static void
at91dci_suspend(struct at91dci_softc *sc)
{
	/* TODO */
}

static void
at91dci_resume(struct at91dci_softc *sc)
{
	/* TODO */
}

static void
at91dci_do_poll(struct usb_bus *bus)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(bus);

	USB_BUS_LOCK(&sc->sc_bus);
	USB_BUS_SPIN_LOCK(&sc->sc_bus);
	at91dci_interrupt_poll_locked(sc);
	at91dci_interrupt_complete_locked(sc);
	USB_BUS_SPIN_UNLOCK(&sc->sc_bus);
	USB_BUS_UNLOCK(&sc->sc_bus);
}

/*------------------------------------------------------------------------*
 * at91dci bulk support
 *------------------------------------------------------------------------*/
static void
at91dci_device_bulk_open(struct usb_xfer *xfer)
{
	return;
}

static void
at91dci_device_bulk_close(struct usb_xfer *xfer)
{
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
at91dci_device_bulk_enter(struct usb_xfer *xfer)
{
	return;
}

static void
at91dci_device_bulk_start(struct usb_xfer *xfer)
{
	/* setup TDs */
	at91dci_setup_standard_chain(xfer);
	at91dci_start_standard_chain(xfer);
}

static const struct usb_pipe_methods at91dci_device_bulk_methods =
{
	.open = at91dci_device_bulk_open,
	.close = at91dci_device_bulk_close,
	.enter = at91dci_device_bulk_enter,
	.start = at91dci_device_bulk_start,
};

/*------------------------------------------------------------------------*
 * at91dci control support
 *------------------------------------------------------------------------*/
static void
at91dci_device_ctrl_open(struct usb_xfer *xfer)
{
	return;
}

static void
at91dci_device_ctrl_close(struct usb_xfer *xfer)
{
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
at91dci_device_ctrl_enter(struct usb_xfer *xfer)
{
	return;
}

static void
at91dci_device_ctrl_start(struct usb_xfer *xfer)
{
	/* setup TDs */
	at91dci_setup_standard_chain(xfer);
	at91dci_start_standard_chain(xfer);
}

static const struct usb_pipe_methods at91dci_device_ctrl_methods =
{
	.open = at91dci_device_ctrl_open,
	.close = at91dci_device_ctrl_close,
	.enter = at91dci_device_ctrl_enter,
	.start = at91dci_device_ctrl_start,
};

/*------------------------------------------------------------------------*
 * at91dci interrupt support
 *------------------------------------------------------------------------*/
static void
at91dci_device_intr_open(struct usb_xfer *xfer)
{
	return;
}

static void
at91dci_device_intr_close(struct usb_xfer *xfer)
{
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
at91dci_device_intr_enter(struct usb_xfer *xfer)
{
	return;
}

static void
at91dci_device_intr_start(struct usb_xfer *xfer)
{
	/* setup TDs */
	at91dci_setup_standard_chain(xfer);
	at91dci_start_standard_chain(xfer);
}

static const struct usb_pipe_methods at91dci_device_intr_methods =
{
	.open = at91dci_device_intr_open,
	.close = at91dci_device_intr_close,
	.enter = at91dci_device_intr_enter,
	.start = at91dci_device_intr_start,
};

/*------------------------------------------------------------------------*
 * at91dci full speed isochronous support
 *------------------------------------------------------------------------*/
static void
at91dci_device_isoc_fs_open(struct usb_xfer *xfer)
{
	return;
}

static void
at91dci_device_isoc_fs_close(struct usb_xfer *xfer)
{
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
at91dci_device_isoc_fs_enter(struct usb_xfer *xfer)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(xfer->xroot->bus);
	uint32_t temp;
	uint32_t nframes;

	DPRINTFN(6, "xfer=%p next=%d nframes=%d\n",
	    xfer, xfer->endpoint->isoc_next, xfer->nframes);

	/* get the current frame index */

	nframes = AT91_UDP_READ_4(sc, AT91_UDP_FRM);

	/*
	 * check if the frame index is within the window where the frames
	 * will be inserted
	 */
	temp = (nframes - xfer->endpoint->isoc_next) & AT91_UDP_FRM_MASK;

	if ((xfer->endpoint->is_synced == 0) ||
	    (temp < xfer->nframes)) {
		/*
		 * If there is data underflow or the endpoint queue is
		 * empty we schedule the transfer a few frames ahead
		 * of the current frame position. Else two isochronous
		 * transfers might overlap.
		 */
		xfer->endpoint->isoc_next = (nframes + 3) & AT91_UDP_FRM_MASK;
		xfer->endpoint->is_synced = 1;
		DPRINTFN(3, "start next=%d\n", xfer->endpoint->isoc_next);
	}
	/*
	 * compute how many milliseconds the insertion is ahead of the
	 * current frame position:
	 */
	temp = (xfer->endpoint->isoc_next - nframes) & AT91_UDP_FRM_MASK;

	/*
	 * pre-compute when the isochronous transfer will be finished:
	 */
	xfer->isoc_time_complete =
	    usb_isoc_time_expand(&sc->sc_bus, nframes) + temp +
	    xfer->nframes;

	/* compute frame number for next insertion */
	xfer->endpoint->isoc_next += xfer->nframes;

	/* setup TDs */
	at91dci_setup_standard_chain(xfer);
}

static void
at91dci_device_isoc_fs_start(struct usb_xfer *xfer)
{
	/* start TD chain */
	at91dci_start_standard_chain(xfer);
}

static const struct usb_pipe_methods at91dci_device_isoc_fs_methods =
{
	.open = at91dci_device_isoc_fs_open,
	.close = at91dci_device_isoc_fs_close,
	.enter = at91dci_device_isoc_fs_enter,
	.start = at91dci_device_isoc_fs_start,
};

/*------------------------------------------------------------------------*
 * at91dci root control support
 *------------------------------------------------------------------------*
 * Simulate a hardware HUB by handling all the necessary requests.
 *------------------------------------------------------------------------*/

static const struct usb_device_descriptor at91dci_devd = {
	.bLength = sizeof(struct usb_device_descriptor),
	.bDescriptorType = UDESC_DEVICE,
	.bcdUSB = {0x00, 0x02},
	.bDeviceClass = UDCLASS_HUB,
	.bDeviceSubClass = UDSUBCLASS_HUB,
	.bDeviceProtocol = UDPROTO_FSHUB,
	.bMaxPacketSize = 64,
	.bcdDevice = {0x00, 0x01},
	.iManufacturer = 1,
	.iProduct = 2,
	.bNumConfigurations = 1,
};

static const struct at91dci_config_desc at91dci_confd = {
	.confd = {
		.bLength = sizeof(struct usb_config_descriptor),
		.bDescriptorType = UDESC_CONFIG,
		.wTotalLength[0] = sizeof(at91dci_confd),
		.bNumInterface = 1,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = UC_SELF_POWERED,
		.bMaxPower = 0,
	},
	.ifcd = {
		.bLength = sizeof(struct usb_interface_descriptor),
		.bDescriptorType = UDESC_INTERFACE,
		.bNumEndpoints = 1,
		.bInterfaceClass = UICLASS_HUB,
		.bInterfaceSubClass = UISUBCLASS_HUB,
		.bInterfaceProtocol = 0,
	},
	.endpd = {
		.bLength = sizeof(struct usb_endpoint_descriptor),
		.bDescriptorType = UDESC_ENDPOINT,
		.bEndpointAddress = (UE_DIR_IN | AT9100_DCI_INTR_ENDPT),
		.bmAttributes = UE_INTERRUPT,
		.wMaxPacketSize[0] = 8,
		.bInterval = 255,
	},
};

#define	HSETW(ptr, val) ptr = { (uint8_t)(val), (uint8_t)((val) >> 8) }

static const struct usb_hub_descriptor_min at91dci_hubd = {
	.bDescLength = sizeof(at91dci_hubd),
	.bDescriptorType = UDESC_HUB,
	.bNbrPorts = 1,
	HSETW(.wHubCharacteristics, (UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL)),
	.bPwrOn2PwrGood = 50,
	.bHubContrCurrent = 0,
	.DeviceRemovable = {0},		/* port is removable */
};

#define	STRING_VENDOR \
  "A\0T\0M\0E\0L"

#define	STRING_PRODUCT \
  "D\0C\0I\0 \0R\0o\0o\0t\0 \0H\0U\0B"

USB_MAKE_STRING_DESC(STRING_VENDOR, at91dci_vendor);
USB_MAKE_STRING_DESC(STRING_PRODUCT, at91dci_product);

static usb_error_t
at91dci_roothub_exec(struct usb_device *udev,
    struct usb_device_request *req, const void **pptr, uint16_t *plength)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(udev->bus);
	const void *ptr;
	uint16_t len;
	uint16_t value;
	uint16_t index;
	usb_error_t err;

	USB_BUS_LOCK_ASSERT(&sc->sc_bus, MA_OWNED);

	/* buffer reset */
	ptr = (const void *)&sc->sc_hub_temp;
	len = 0;
	err = 0;

	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	/* demultiplex the control request */

	switch (req->bmRequestType) {
	case UT_READ_DEVICE:
		switch (req->bRequest) {
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
		switch (req->bRequest) {
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
		switch (req->bRequest) {
		case UR_CLEAR_FEATURE:
			switch (UGETW(req->wValue)) {
			case UF_ENDPOINT_HALT:
				goto tr_handle_clear_halt;
			case UF_DEVICE_REMOTE_WAKEUP:
				goto tr_handle_clear_wakeup;
			default:
				goto tr_stalled;
			}
			break;
		case UR_SET_FEATURE:
			switch (UGETW(req->wValue)) {
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
		switch (req->bRequest) {
		case UR_GET_STATUS:
			goto tr_handle_get_ep_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_INTERFACE:
		switch (req->bRequest) {
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
		switch (req->bRequest) {
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
		switch (req->bRequest) {
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
		switch (req->bRequest) {
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
		switch (req->bRequest) {
		case UR_GET_TT_STATE:
			goto tr_handle_get_tt_state;
		case UR_GET_STATUS:
			goto tr_handle_get_port_status;
		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_CLASS_DEVICE:
		switch (req->bRequest) {
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
		len = sizeof(at91dci_devd);
		ptr = (const void *)&at91dci_devd;
		goto tr_valid;
	case UDESC_CONFIG:
		if (value & 0xff) {
			goto tr_stalled;
		}
		len = sizeof(at91dci_confd);
		ptr = (const void *)&at91dci_confd;
		goto tr_valid;
	case UDESC_STRING:
		switch (value & 0xff) {
		case 0:		/* Language table */
			len = sizeof(usb_string_lang_en);
			ptr = (const void *)&usb_string_lang_en;
			goto tr_valid;

		case 1:		/* Vendor */
			len = sizeof(at91dci_vendor);
			ptr = (const void *)&at91dci_vendor;
			goto tr_valid;

		case 2:		/* Product */
			len = sizeof(at91dci_product);
			ptr = (const void *)&at91dci_product;
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
	len = 1;
	sc->sc_hub_temp.wValue[0] = sc->sc_conf;
	goto tr_valid;

tr_handle_get_status:
	len = 2;
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
	len = 1;
	sc->sc_hub_temp.wValue[0] = 0;
	goto tr_valid;

tr_handle_get_tt_state:
tr_handle_get_class_status:
tr_handle_get_iface_status:
tr_handle_get_ep_status:
	len = 2;
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
		at91dci_wakeup_peer(sc);
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
		at91dci_pull_down(sc);
		at91dci_clocks_off(sc);
		break;
	case UHF_C_PORT_CONNECTION:
		sc->sc_flags.change_connect = 0;
		break;
	case UHF_C_PORT_SUSPEND:
		sc->sc_flags.change_suspend = 0;
		break;
	default:
		err = USB_ERR_IOERROR;
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
		err = USB_ERR_IOERROR;
		goto done;
	}
	goto tr_valid;

tr_handle_get_port_status:

	DPRINTFN(9, "UR_GET_PORT_STATUS\n");

	if (index != 1) {
		goto tr_stalled;
	}
	if (sc->sc_flags.status_vbus) {
		at91dci_clocks_on(sc);
		at91dci_pull_up(sc);
	} else {
		at91dci_pull_down(sc);
		at91dci_clocks_off(sc);
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

		if (sc->sc_flags.status_vbus &&
		    sc->sc_flags.status_bus_reset) {
			/* reset endpoint flags */
			memset(sc->sc_ep_flags, 0, sizeof(sc->sc_ep_flags));
		}
	}
	if (sc->sc_flags.change_suspend) {
		value |= UPS_C_SUSPEND;
	}
	USETW(sc->sc_hub_temp.ps.wPortChange, value);
	len = sizeof(sc->sc_hub_temp.ps);
	goto tr_valid;

tr_handle_get_class_descriptor:
	if (value & 0xFF) {
		goto tr_stalled;
	}
	ptr = (const void *)&at91dci_hubd;
	len = sizeof(at91dci_hubd);
	goto tr_valid;

tr_stalled:
	err = USB_ERR_STALLED;
tr_valid:
done:
	*plength = len;
	*pptr = ptr;
	return (err);
}

static void
at91dci_xfer_setup(struct usb_setup_params *parm)
{
	const struct usb_hw_ep_profile *pf;
	struct at91dci_softc *sc;
	struct usb_xfer *xfer;
	void *last_obj;
	uint32_t ntd;
	uint32_t n;
	uint8_t ep_no;

	sc = AT9100_DCI_BUS2SC(parm->udev->bus);
	xfer = parm->curr_xfer;

	/*
	 * NOTE: This driver does not use any of the parameters that
	 * are computed from the following values. Just set some
	 * reasonable dummies:
	 */
	parm->hc_max_packet_size = 0x500;
	parm->hc_max_packet_count = 1;
	parm->hc_max_frame_size = 0x500;

	usbd_transfer_setup_sub(parm);

	/*
	 * compute maximum number of TDs
	 */
	if (parm->methods == &at91dci_device_ctrl_methods) {

		ntd = xfer->nframes + 1 /* STATUS */ + 1	/* SYNC 1 */
		    + 1 /* SYNC 2 */ ;

	} else if (parm->methods == &at91dci_device_bulk_methods) {

		ntd = xfer->nframes + 1 /* SYNC */ ;

	} else if (parm->methods == &at91dci_device_intr_methods) {

		ntd = xfer->nframes + 1 /* SYNC */ ;

	} else if (parm->methods == &at91dci_device_isoc_fs_methods) {

		ntd = xfer->nframes + 1 /* SYNC */ ;

	} else {

		ntd = 0;
	}

	/*
	 * check if "usbd_transfer_setup_sub" set an error
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

		ep_no = xfer->endpointno & UE_ADDR;
		at91dci_get_hw_ep_profile(parm->udev, &pf, ep_no);

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

		struct at91dci_td *td;

		if (parm->buf) {

			td = USB_ADD_BYTES(parm->buf, parm->size[0]);

			/* init TD */
			td->max_packet_size = xfer->max_packet_size;
			td->status_reg = AT91_UDP_CSR(ep_no);
			td->fifo_reg = AT91_UDP_FDR(ep_no);
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
at91dci_xfer_unsetup(struct usb_xfer *xfer)
{
	return;
}

static void
at91dci_ep_init(struct usb_device *udev, struct usb_endpoint_descriptor *edesc,
    struct usb_endpoint *ep)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(udev->bus);

	DPRINTFN(2, "endpoint=%p, addr=%d, endpt=%d, mode=%d (%d)\n",
	    ep, udev->address,
	    edesc->bEndpointAddress, udev->flags.usb_mode,
	    sc->sc_rt_addr);

	if (udev->device_index != sc->sc_rt_addr) {

		if (udev->speed != USB_SPEED_FULL) {
			/* not supported */
			return;
		}
		switch (edesc->bmAttributes & UE_XFERTYPE) {
		case UE_CONTROL:
			ep->methods = &at91dci_device_ctrl_methods;
			break;
		case UE_INTERRUPT:
			ep->methods = &at91dci_device_intr_methods;
			break;
		case UE_ISOCHRONOUS:
			ep->methods = &at91dci_device_isoc_fs_methods;
			break;
		case UE_BULK:
			ep->methods = &at91dci_device_bulk_methods;
			break;
		default:
			/* do nothing */
			break;
		}
	}
}

static void
at91dci_set_hw_power_sleep(struct usb_bus *bus, uint32_t state)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(bus);

	switch (state) {
	case USB_HW_POWER_SUSPEND:
		at91dci_suspend(sc);
		break;
	case USB_HW_POWER_SHUTDOWN:
		at91dci_uninit(sc);
		break;
	case USB_HW_POWER_RESUME:
		at91dci_resume(sc);
		break;
	default:
		break;
	}
}

static const struct usb_bus_methods at91dci_bus_methods =
{
	.endpoint_init = &at91dci_ep_init,
	.xfer_setup = &at91dci_xfer_setup,
	.xfer_unsetup = &at91dci_xfer_unsetup,
	.get_hw_ep_profile = &at91dci_get_hw_ep_profile,
	.set_stall = &at91dci_set_stall,
	.xfer_stall = &at91dci_xfer_stall,
	.clear_stall = &at91dci_clear_stall,
	.roothub_exec = &at91dci_roothub_exec,
	.xfer_poll = &at91dci_do_poll,
	.set_hw_power_sleep = &at91dci_set_hw_power_sleep,
};
