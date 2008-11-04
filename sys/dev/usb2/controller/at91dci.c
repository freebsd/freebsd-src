#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
 * reset !
 *
 * NOTE: When the chip detects BUS-reset it will also reset the
 * endpoints, Function-address and more.
 */

#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_defs.h>

#define	USB_DEBUG_VAR at91dcidebug
#define	usb2_config_td_cc at91dci_config_copy
#define	usb2_config_td_softc at91dci_softc

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_config_td.h>
#include <dev/usb2/core/usb2_sw_transfer.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_hub.h>
#include <dev/usb2/core/usb2_util.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>
#include <dev/usb2/controller/at91dci.h>

#define	AT9100_DCI_BUS2SC(bus) \
   ((struct at91dci_softc *)(((uint8_t *)(bus)) - \
   USB_P2U(&(((struct at91dci_softc *)0)->sc_bus))))

#define	AT9100_DCI_PC2SC(pc) \
   AT9100_DCI_BUS2SC((pc)->tag_parent->info->bus)

#if USB_DEBUG
static int at91dcidebug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, at91dci, CTLFLAG_RW, 0, "USB at91dci");
SYSCTL_INT(_hw_usb2_at91dci, OID_AUTO, debug, CTLFLAG_RW,
    &at91dcidebug, 0, "at91dci debug level");
#endif

#define	AT9100_DCI_INTR_ENDPT 1

/* prototypes */

struct usb2_bus_methods at91dci_bus_methods;
struct usb2_pipe_methods at91dci_device_bulk_methods;
struct usb2_pipe_methods at91dci_device_ctrl_methods;
struct usb2_pipe_methods at91dci_device_intr_methods;
struct usb2_pipe_methods at91dci_device_isoc_fs_methods;
struct usb2_pipe_methods at91dci_root_ctrl_methods;
struct usb2_pipe_methods at91dci_root_intr_methods;

static at91dci_cmd_t at91dci_setup_rx;
static at91dci_cmd_t at91dci_data_rx;
static at91dci_cmd_t at91dci_data_tx;
static at91dci_cmd_t at91dci_data_tx_sync;
static void at91dci_device_done(struct usb2_xfer *xfer, usb2_error_t error);
static void at91dci_do_poll(struct usb2_bus *bus);
static void at91dci_root_ctrl_poll(struct at91dci_softc *sc);
static void at91dci_standard_done(struct usb2_xfer *xfer);

static usb2_sw_transfer_func_t at91dci_root_intr_done;
static usb2_sw_transfer_func_t at91dci_root_ctrl_done;
static usb2_config_td_command_t at91dci_root_ctrl_task;

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
static const struct usb2_hw_ep_profile
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
at91dci_get_hw_ep_profile(struct usb2_device *udev,
    const struct usb2_hw_ep_profile **ppf, uint8_t ep_addr)
{
	if (ep_addr < AT91_UDP_EP_MAX) {
		*ppf = (at91dci_ep_profile + ep_addr);
	} else {
		*ppf = NULL;
	}
	return;
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
	return;
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
	return;
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
	return;
}

static void
at91dci_pull_down(struct at91dci_softc *sc)
{
	/* pulldown D+, if possible */

	if (sc->sc_flags.d_pulled_up) {
		sc->sc_flags.d_pulled_up = 0;
		(sc->sc_pull_down) (sc->sc_pull_arg);
	}
	return;
}

static void
at91dci_wakeup_peer(struct at91dci_softc *sc)
{
	uint32_t temp;

	if (!(sc->sc_flags.status_suspend)) {
		return;
	}
	temp = AT91_UDP_READ_4(sc, AT91_UDP_GSTATE);

	if (!(temp & AT91_UDP_GSTATE_ESR)) {
		return;
	}
	AT91_UDP_WRITE_4(sc, AT91_UDP_GSTATE, temp);

	return;
}

static void
at91dci_rem_wakeup_set(struct usb2_device *udev, uint8_t is_on)
{
	struct at91dci_softc *sc;
	uint32_t temp;

	DPRINTFN(5, "is_on=%u\n", is_on);

	mtx_assert(&udev->bus->mtx, MA_OWNED);

	sc = AT9100_DCI_BUS2SC(udev->bus);

	temp = AT91_UDP_READ_4(sc, AT91_UDP_GSTATE);

	if (is_on) {
		temp |= AT91_UDP_GSTATE_ESR;
	} else {
		temp &= ~AT91_UDP_GSTATE_ESR;
	}

	AT91_UDP_WRITE_4(sc, AT91_UDP_GSTATE, temp);

	return;
}

static void
at91dci_set_address(struct at91dci_softc *sc, uint8_t addr)
{
	DPRINTFN(5, "addr=%d\n", addr);

	AT91_UDP_WRITE_4(sc, AT91_UDP_FADDR, addr |
	    AT91_UDP_FADDR_EN);

	return;
}

static uint8_t
at91dci_setup_rx(struct at91dci_td *td)
{
	struct at91dci_softc *sc;
	struct usb2_device_request req;
	uint32_t csr;
	uint32_t temp;
	uint16_t count;

	/* read out FIFO status */
	csr = bus_space_read_4(td->io_tag, td->io_hdl,
	    td->status_reg);

	DPRINTFN(5, "csr=0x%08x rem=%u\n", csr, td->remainder);

	temp = csr;
	temp &= (AT91_UDP_CSR_RX_DATA_BK0 |
	    AT91_UDP_CSR_RX_DATA_BK1 |
	    AT91_UDP_CSR_STALLSENT |
	    AT91_UDP_CSR_RXSETUP |
	    AT91_UDP_CSR_TXCOMP);

	if (!(csr & AT91_UDP_CSR_RXSETUP)) {
		/* abort any ongoing transfer */
		if (!td->did_stall) {
			DPRINTFN(5, "stalling\n");
			temp |= AT91_UDP_CSR_FORCESTALL;
			td->did_stall = 1;
		}
		goto not_complete;
	}
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
	bus_space_read_multi_1(td->io_tag, td->io_hdl,
	    td->fifo_reg, (void *)&req, sizeof(req));

	/* copy data into real buffer */
	usb2_copy_in(td->pc, 0, &req, sizeof(req));

	td->offset = sizeof(req);
	td->remainder = 0;

	/* get pointer to softc */
	sc = AT9100_DCI_PC2SC(td->pc);

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
	bus_space_write_4(td->io_tag, td->io_hdl,
	    td->status_reg, csr);
	return (0);			/* complete */

not_complete:
	/* clear interrupts, if any */
	if (temp) {
		DPRINTFN(5, "clearing 0x%08x\n", temp);
		AT91_CSR_ACK(csr, temp);
		bus_space_write_4(td->io_tag, td->io_hdl,
		    td->status_reg, csr);
	}
	return (1);			/* not complete */

}

static uint8_t
at91dci_data_rx(struct at91dci_td *td)
{
	struct usb2_page_search buf_res;
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
	csr = bus_space_read_4(td->io_tag, td->io_hdl,
	    td->status_reg);

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
			bus_space_write_4(td->io_tag, td->io_hdl,
			    td->status_reg, csr);
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
		usb2_get_page(td->pc, td->offset, &buf_res);

		/* get correct length */
		if (buf_res.length > count) {
			buf_res.length = count;
		}
		/* receive data */
		bus_space_read_multi_1(td->io_tag, td->io_hdl,
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
	bus_space_write_4(td->io_tag, td->io_hdl,
	    td->status_reg, csr);

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
at91dci_data_tx(struct at91dci_td *td)
{
	struct usb2_page_search buf_res;
	uint32_t csr;
	uint32_t temp;
	uint16_t count;
	uint8_t to;

	to = 2;				/* don't loop forever! */

repeat:

	/* read out FIFO status */
	csr = bus_space_read_4(td->io_tag, td->io_hdl,
	    td->status_reg);

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
			bus_space_write_4(td->io_tag, td->io_hdl,
			    td->status_reg, csr);
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

		usb2_get_page(td->pc, td->offset, &buf_res);

		/* get correct length */
		if (buf_res.length > count) {
			buf_res.length = count;
		}
		/* transmit data */
		bus_space_write_multi_1(td->io_tag, td->io_hdl,
		    td->fifo_reg, buf_res.buffer, buf_res.length);

		/* update counters */
		count -= buf_res.length;
		td->offset += buf_res.length;
		td->remainder -= buf_res.length;
	}

	/* write command */
	AT91_CSR_ACK(csr, temp);
	bus_space_write_4(td->io_tag, td->io_hdl,
	    td->status_reg, csr);

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
at91dci_data_tx_sync(struct at91dci_td *td)
{
	struct at91dci_softc *sc;
	uint32_t csr;
	uint32_t temp;

#if 0
repeat:
#endif

	/* read out FIFO status */
	csr = bus_space_read_4(td->io_tag, td->io_hdl,
	    td->status_reg);

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
	sc = AT9100_DCI_PC2SC(td->pc);
	if (sc->sc_dv_addr != 0xFF) {
		/*
		 * The AT91 has a special requirement with regard to
		 * setting the address and that is to write the new
		 * address before clearing TXCOMP:
		 */
		at91dci_set_address(sc, sc->sc_dv_addr);
	}
	/* write command */
	AT91_CSR_ACK(csr, temp);
	bus_space_write_4(td->io_tag, td->io_hdl,
	    td->status_reg, csr);

	return (0);			/* complete */

not_complete:
	if (temp) {
		/* write command */
		AT91_CSR_ACK(csr, temp);
		bus_space_write_4(td->io_tag, td->io_hdl,
		    td->status_reg, csr);
	}
	return (1);			/* not complete */
}

static uint8_t
at91dci_xfer_do_fifo(struct usb2_xfer *xfer)
{
	struct at91dci_softc *sc;
	struct at91dci_td *td;
	uint8_t temp;

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
		temp = 0;
		if (td->fifo_bank)
			temp |= 1;
		td = td->obj_next;
		xfer->td_transfer_cache = td;
		if (temp & 1)
			td->fifo_bank = 1;
	}
	return (1);			/* not complete */

done:
	sc = xfer->usb2_sc;
	temp = (xfer->endpoint & UE_ADDR);

	/* update FIFO bank flag and multi buffer */
	if (td->fifo_bank) {
		sc->sc_ep_flags[temp].fifo_bank = 1;
	} else {
		sc->sc_ep_flags[temp].fifo_bank = 0;
	}

	/* compute all actual lengths */

	at91dci_standard_done(xfer);

	return (0);			/* complete */
}

static void
at91dci_interrupt_poll(struct at91dci_softc *sc)
{
	struct usb2_xfer *xfer;

repeat:
	TAILQ_FOREACH(xfer, &sc->sc_bus.intr_q.head, wait_entry) {
		if (!at91dci_xfer_do_fifo(xfer)) {
			/* queue has been modified */
			goto repeat;
		}
	}
	return;
}

static void
at91dci_vbus_interrupt(struct usb2_bus *bus, uint8_t is_on)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(bus);

	DPRINTFN(5, "vbus = %u\n", is_on);

	mtx_lock(&sc->sc_bus.mtx);
	if (is_on) {
		if (!sc->sc_flags.status_vbus) {
			sc->sc_flags.status_vbus = 1;

			/* complete root HUB interrupt endpoint */

			usb2_sw_transfer(&sc->sc_root_intr,
			    &at91dci_root_intr_done);
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
			    &at91dci_root_intr_done);
		}
	}

	mtx_unlock(&sc->sc_bus.mtx);

	return;
}

void
at91dci_interrupt(struct at91dci_softc *sc)
{
	uint32_t status;

	mtx_lock(&sc->sc_bus.mtx);

	status = AT91_UDP_READ_4(sc, AT91_UDP_ISR);
	status &= AT91_UDP_INT_DEFAULT;

	if (!status) {
		mtx_unlock(&sc->sc_bus.mtx);
		return;
	}
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

		usb2_sw_transfer(&sc->sc_root_intr,
		    &at91dci_root_intr_done);
	}
	/* check for any endpoint interrupts */

	if (status & AT91_UDP_INT_EPS) {

		DPRINTFN(5, "real endpoint interrupt 0x%08x\n", status);

		at91dci_interrupt_poll(sc);
	}
	mtx_unlock(&sc->sc_bus.mtx);

	return;
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
	td->did_stall = 0;
	td->short_pkt = temp->short_pkt;
	td->alt_next = temp->setup_alt_next;
	return;
}

static void
at91dci_setup_standard_chain(struct usb2_xfer *xfer)
{
	struct at91dci_std_temp temp;
	struct at91dci_softc *sc;
	struct at91dci_td *td;
	uint32_t x;
	uint8_t ep_no;
	uint8_t need_sync;

	DPRINTFN(9, "addr=%d endpt=%d sumlen=%d speed=%d\n",
	    xfer->address, UE_GET_ADDR(xfer->endpoint),
	    xfer->sumlen, usb2_get_speed(xfer->udev));

	temp.max_frame_size = xfer->max_frame_size;

	td = xfer->td_start[0];
	xfer->td_transfer_first = td;
	xfer->td_transfer_cache = td;

	/* setup temp */

	temp.td = NULL;
	temp.td_next = xfer->td_start[0];
	temp.setup_alt_next = xfer->flags_int.short_frames_ok;
	temp.offset = 0;

	sc = xfer->usb2_sc;
	ep_no = (xfer->endpoint & UE_ADDR);

	/* check if we should prepend a setup message */

	if (xfer->flags_int.control_xfr) {
		if (xfer->flags_int.control_hdr) {

			temp.func = &at91dci_setup_rx;
			temp.len = xfer->frlengths[0];
			temp.pc = xfer->frbuffers + 0;
			temp.short_pkt = temp.len ? 1 : 0;

			at91dci_setup_standard_chain_sub(&temp);
		}
		x = 1;
	} else {
		x = 0;
	}

	if (x != xfer->nframes) {
		if (xfer->endpoint & UE_DIR_IN) {
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
			temp.setup_alt_next = 0;
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

	/* always setup a valid "pc" pointer for status and sync */
	temp.pc = xfer->frbuffers + 0;

	/* check if we need to sync */
	if (need_sync && xfer->flags_int.control_xfr) {

		/* we need a SYNC point after TX */
		temp.func = &at91dci_data_tx_sync;
		temp.len = 0;
		temp.short_pkt = 0;

		at91dci_setup_standard_chain_sub(&temp);
	}
	/* check if we should append a status stage */
	if (xfer->flags_int.control_xfr &&
	    !xfer->flags_int.control_act) {

		/*
		 * Send a DATA1 message and invert the current
		 * endpoint direction.
		 */
		if (xfer->endpoint & UE_DIR_IN) {
			temp.func = &at91dci_data_rx;
			need_sync = 0;
		} else {
			temp.func = &at91dci_data_tx;
			need_sync = 1;
		}
		temp.len = 0;
		temp.short_pkt = 0;

		at91dci_setup_standard_chain_sub(&temp);
		if (need_sync) {
			/* we need a SYNC point after TX */
			temp.func = &at91dci_data_tx_sync;
			temp.len = 0;
			temp.short_pkt = 0;

			at91dci_setup_standard_chain_sub(&temp);
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
	return;
}

static void
at91dci_timeout(void *arg)
{
	struct usb2_xfer *xfer = arg;
	struct at91dci_softc *sc = xfer->usb2_sc;

	DPRINTF("xfer=%p\n", xfer);

	mtx_assert(&sc->sc_bus.mtx, MA_OWNED);

	/* transfer is transferred */
	at91dci_device_done(xfer, USB_ERR_TIMEOUT);

	mtx_unlock(&sc->sc_bus.mtx);

	return;
}

static void
at91dci_start_standard_chain(struct usb2_xfer *xfer)
{
	DPRINTFN(9, "\n");

	/* poll one time */
	if (at91dci_xfer_do_fifo(xfer)) {

		struct at91dci_softc *sc = xfer->usb2_sc;
		uint8_t ep_no = xfer->endpoint & UE_ADDR;

		/*
		 * Only enable the endpoint interrupt when we are actually
		 * waiting for data, hence we are dealing with level
		 * triggered interrupts !
		 */
		AT91_UDP_WRITE_4(sc, AT91_UDP_IER, AT91_UDP_INT_EP(ep_no));

		DPRINTFN(15, "enable interrupts on endpoint %d\n", ep_no);

		/* put transfer on interrupt queue */
		usb2_transfer_enqueue(&xfer->udev->bus->intr_q, xfer);

		/* start timeout, if any */
		if (xfer->timeout != 0) {
			usb2_transfer_timeout_ms(xfer,
			    &at91dci_timeout, xfer->timeout);
		}
	}
	return;
}

static void
at91dci_root_intr_done(struct usb2_xfer *xfer,
    struct usb2_sw_transfer *std)
{
	struct at91dci_softc *sc = xfer->usb2_sc;

	DPRINTFN(9, "\n");

	mtx_assert(&sc->sc_bus.mtx, MA_OWNED);

	if (std->state != USB_SW_TR_PRE_DATA) {
		if (std->state == USB_SW_TR_PRE_CALLBACK) {
			/* transfer transferred */
			at91dci_device_done(xfer, std->err);
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
at91dci_standard_done_sub(struct usb2_xfer *xfer)
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
at91dci_standard_done(struct usb2_xfer *xfer)
{
	usb2_error_t err = 0;

	DPRINTFN(13, "xfer=%p pipe=%p transfer done\n",
	    xfer, xfer->pipe);

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
	return;
}

/*------------------------------------------------------------------------*
 *	at91dci_device_done
 *
 * NOTE: this function can be called more than one time on the
 * same USB transfer!
 *------------------------------------------------------------------------*/
static void
at91dci_device_done(struct usb2_xfer *xfer, usb2_error_t error)
{
	struct at91dci_softc *sc = xfer->usb2_sc;
	uint8_t ep_no;

	mtx_assert(&sc->sc_bus.mtx, MA_OWNED);

	DPRINTFN(2, "xfer=%p, pipe=%p, error=%d\n",
	    xfer, xfer->pipe, error);

	if (xfer->flags_int.usb2_mode == USB_MODE_DEVICE) {
		ep_no = (xfer->endpoint & UE_ADDR);

		/* disable endpoint interrupt */
		AT91_UDP_WRITE_4(sc, AT91_UDP_IDR, AT91_UDP_INT_EP(ep_no));

		DPRINTFN(15, "disable interrupts on endpoint %d\n", ep_no);
	}
	/* dequeue transfer and start next transfer */
	usb2_transfer_done(xfer, error);
	return;
}

static void
at91dci_set_stall(struct usb2_device *udev, struct usb2_xfer *xfer,
    struct usb2_pipe *pipe)
{
	struct at91dci_softc *sc;
	uint32_t csr_val;
	uint8_t csr_reg;

	mtx_assert(&udev->bus->mtx, MA_OWNED);

	DPRINTFN(5, "pipe=%p\n", pipe);

	if (xfer) {
		/* cancel any ongoing transfers */
		at91dci_device_done(xfer, USB_ERR_STALLED);
	}
	/* set FORCESTALL */
	sc = AT9100_DCI_BUS2SC(udev->bus);
	csr_reg = (pipe->edesc->bEndpointAddress & UE_ADDR);
	csr_reg = AT91_UDP_CSR(csr_reg);
	csr_val = AT91_UDP_READ_4(sc, csr_reg);
	AT91_CSR_ACK(csr_val, AT91_UDP_CSR_FORCESTALL);
	AT91_UDP_WRITE_4(sc, csr_reg, csr_val);
	return;
}

static void
at91dci_clear_stall_sub(struct at91dci_softc *sc, uint8_t ep_no,
    uint8_t ep_type, uint8_t ep_dir)
{
	const struct usb2_hw_ep_profile *pf;
	uint32_t csr_val;
	uint32_t temp;
	uint8_t csr_reg;
	uint8_t to;

	if (ep_type == UE_CONTROL) {
		/* clearing stall is not needed */
		return;
	}
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
	 * FIFO banks aswell, but it doesn't! We have to do this
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

	return;
}

static void
at91dci_clear_stall(struct usb2_device *udev, struct usb2_pipe *pipe)
{
	struct at91dci_softc *sc;
	struct usb2_endpoint_descriptor *ed;

	DPRINTFN(5, "pipe=%p\n", pipe);

	mtx_assert(&udev->bus->mtx, MA_OWNED);

	/* check mode */
	if (udev->flags.usb2_mode != USB_MODE_DEVICE) {
		/* not supported */
		return;
	}
	/* get softc */
	sc = AT9100_DCI_BUS2SC(udev->bus);

	/* get endpoint descriptor */
	ed = pipe->edesc;

	/* reset endpoint */
	at91dci_clear_stall_sub(sc,
	    (ed->bEndpointAddress & UE_ADDR),
	    (ed->bmAttributes & UE_XFERTYPE),
	    (ed->bEndpointAddress & (UE_DIR_IN | UE_DIR_OUT)));
	return;
}

usb2_error_t
at91dci_init(struct at91dci_softc *sc)
{
	uint32_t csr_val;
	uint8_t n;

	DPRINTF("start\n");

	/* set up the bus structure */
	sc->sc_bus.usbrev = USB_REV_1_1;
	sc->sc_bus.methods = &at91dci_bus_methods;

	mtx_lock(&sc->sc_bus.mtx);

	/* turn on clocks */

	if (sc->sc_clocks_on) {
		(sc->sc_clocks_on) (sc->sc_clocks_arg);
	}
	/* wait a little for things to stabilise */
	usb2_pause_mtx(&sc->sc_bus.mtx, 1);

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

	mtx_unlock(&sc->sc_bus.mtx);

	/* catch any lost interrupts */

	at91dci_do_poll(&sc->sc_bus);

	return (0);			/* success */
}

void
at91dci_uninit(struct at91dci_softc *sc)
{
	mtx_lock(&sc->sc_bus.mtx);

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
	mtx_unlock(&sc->sc_bus.mtx);

	return;
}

void
at91dci_suspend(struct at91dci_softc *sc)
{
	return;
}

void
at91dci_resume(struct at91dci_softc *sc)
{
	return;
}

static void
at91dci_do_poll(struct usb2_bus *bus)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(bus);

	mtx_lock(&sc->sc_bus.mtx);
	at91dci_interrupt_poll(sc);
	at91dci_root_ctrl_poll(sc);
	mtx_unlock(&sc->sc_bus.mtx);
	return;
}

/*------------------------------------------------------------------------*
 * at91dci bulk support
 *------------------------------------------------------------------------*/
static void
at91dci_device_bulk_open(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_device_bulk_close(struct usb2_xfer *xfer)
{
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
	return;
}

static void
at91dci_device_bulk_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_device_bulk_start(struct usb2_xfer *xfer)
{
	/* setup TDs */
	at91dci_setup_standard_chain(xfer);
	at91dci_start_standard_chain(xfer);
	return;
}

struct usb2_pipe_methods at91dci_device_bulk_methods =
{
	.open = at91dci_device_bulk_open,
	.close = at91dci_device_bulk_close,
	.enter = at91dci_device_bulk_enter,
	.start = at91dci_device_bulk_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 1,
};

/*------------------------------------------------------------------------*
 * at91dci control support
 *------------------------------------------------------------------------*/
static void
at91dci_device_ctrl_open(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_device_ctrl_close(struct usb2_xfer *xfer)
{
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
	return;
}

static void
at91dci_device_ctrl_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_device_ctrl_start(struct usb2_xfer *xfer)
{
	/* setup TDs */
	at91dci_setup_standard_chain(xfer);
	at91dci_start_standard_chain(xfer);
	return;
}

struct usb2_pipe_methods at91dci_device_ctrl_methods =
{
	.open = at91dci_device_ctrl_open,
	.close = at91dci_device_ctrl_close,
	.enter = at91dci_device_ctrl_enter,
	.start = at91dci_device_ctrl_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 1,
};

/*------------------------------------------------------------------------*
 * at91dci interrupt support
 *------------------------------------------------------------------------*/
static void
at91dci_device_intr_open(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_device_intr_close(struct usb2_xfer *xfer)
{
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
	return;
}

static void
at91dci_device_intr_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_device_intr_start(struct usb2_xfer *xfer)
{
	/* setup TDs */
	at91dci_setup_standard_chain(xfer);
	at91dci_start_standard_chain(xfer);
	return;
}

struct usb2_pipe_methods at91dci_device_intr_methods =
{
	.open = at91dci_device_intr_open,
	.close = at91dci_device_intr_close,
	.enter = at91dci_device_intr_enter,
	.start = at91dci_device_intr_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 1,
};

/*------------------------------------------------------------------------*
 * at91dci full speed isochronous support
 *------------------------------------------------------------------------*/
static void
at91dci_device_isoc_fs_open(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_device_isoc_fs_close(struct usb2_xfer *xfer)
{
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
	return;
}

static void
at91dci_device_isoc_fs_enter(struct usb2_xfer *xfer)
{
	struct at91dci_softc *sc = xfer->usb2_sc;
	uint32_t temp;
	uint32_t nframes;

	DPRINTFN(6, "xfer=%p next=%d nframes=%d\n",
	    xfer, xfer->pipe->isoc_next, xfer->nframes);

	/* get the current frame index */

	nframes = AT91_UDP_READ_4(sc, AT91_UDP_FRM);

	/*
	 * check if the frame index is within the window where the frames
	 * will be inserted
	 */
	temp = (nframes - xfer->pipe->isoc_next) & AT91_UDP_FRM_MASK;

	if ((xfer->pipe->is_synced == 0) ||
	    (temp < xfer->nframes)) {
		/*
		 * If there is data underflow or the pipe queue is
		 * empty we schedule the transfer a few frames ahead
		 * of the current frame position. Else two isochronous
		 * transfers might overlap.
		 */
		xfer->pipe->isoc_next = (nframes + 3) & AT91_UDP_FRM_MASK;
		xfer->pipe->is_synced = 1;
		DPRINTFN(3, "start next=%d\n", xfer->pipe->isoc_next);
	}
	/*
	 * compute how many milliseconds the insertion is ahead of the
	 * current frame position:
	 */
	temp = (xfer->pipe->isoc_next - nframes) & AT91_UDP_FRM_MASK;

	/*
	 * pre-compute when the isochronous transfer will be finished:
	 */
	xfer->isoc_time_complete =
	    usb2_isoc_time_expand(&sc->sc_bus, nframes) + temp +
	    xfer->nframes;

	/* compute frame number for next insertion */
	xfer->pipe->isoc_next += xfer->nframes;

	/* setup TDs */
	at91dci_setup_standard_chain(xfer);
	return;
}

static void
at91dci_device_isoc_fs_start(struct usb2_xfer *xfer)
{
	/* start TD chain */
	at91dci_start_standard_chain(xfer);
	return;
}

struct usb2_pipe_methods at91dci_device_isoc_fs_methods =
{
	.open = at91dci_device_isoc_fs_open,
	.close = at91dci_device_isoc_fs_close,
	.enter = at91dci_device_isoc_fs_enter,
	.start = at91dci_device_isoc_fs_start,
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
at91dci_root_ctrl_open(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_root_ctrl_close(struct usb2_xfer *xfer)
{
	struct at91dci_softc *sc = xfer->usb2_sc;

	if (sc->sc_root_ctrl.xfer == xfer) {
		sc->sc_root_ctrl.xfer = NULL;
	}
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
	return;
}

/*
 * USB descriptors for the virtual Root HUB:
 */

static const struct usb2_device_descriptor at91dci_devd = {
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

static const struct usb2_device_qualifier at91dci_odevd = {
	.bLength = sizeof(struct usb2_device_qualifier),
	.bDescriptorType = UDESC_DEVICE_QUALIFIER,
	.bcdUSB = {0x00, 0x02},
	.bDeviceClass = UDCLASS_HUB,
	.bDeviceSubClass = UDSUBCLASS_HUB,
	.bDeviceProtocol = UDPROTO_FSHUB,
	.bMaxPacketSize0 = 0,
	.bNumConfigurations = 0,
};

static const struct at91dci_config_desc at91dci_confd = {
	.confd = {
		.bLength = sizeof(struct usb2_config_descriptor),
		.bDescriptorType = UDESC_CONFIG,
		.wTotalLength[0] = sizeof(at91dci_confd),
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
		.bEndpointAddress = (UE_DIR_IN | AT9100_DCI_INTR_ENDPT),
		.bmAttributes = UE_INTERRUPT,
		.wMaxPacketSize[0] = 8,
		.bInterval = 255,
	},
};

static const struct usb2_hub_descriptor_min at91dci_hubd = {
	.bDescLength = sizeof(at91dci_hubd),
	.bDescriptorType = UDESC_HUB,
	.bNbrPorts = 1,
	.wHubCharacteristics[0] =
	(UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL) & 0xFF,
	.wHubCharacteristics[1] =
	(UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL) >> 16,
	.bPwrOn2PwrGood = 50,
	.bHubContrCurrent = 0,
	.DeviceRemovable = {0},		/* port is removable */
};

#define	STRING_LANG \
  0x09, 0x04,				/* American English */

#define	STRING_VENDOR \
  'A', 0, 'T', 0, 'M', 0, 'E', 0, 'L', 0

#define	STRING_PRODUCT \
  'D', 0, 'C', 0, 'I', 0, ' ', 0, 'R', 0, \
  'o', 0, 'o', 0, 't', 0, ' ', 0, 'H', 0, \
  'U', 0, 'B', 0,

USB_MAKE_STRING_DESC(STRING_LANG, at91dci_langtab);
USB_MAKE_STRING_DESC(STRING_VENDOR, at91dci_vendor);
USB_MAKE_STRING_DESC(STRING_PRODUCT, at91dci_product);

static void
at91dci_root_ctrl_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_root_ctrl_start(struct usb2_xfer *xfer)
{
	struct at91dci_softc *sc = xfer->usb2_sc;

	sc->sc_root_ctrl.xfer = xfer;

	usb2_config_td_queue_command(
	    &sc->sc_config_td, NULL, &at91dci_root_ctrl_task, 0, 0);

	return;
}

static void
at91dci_root_ctrl_task(struct at91dci_softc *sc,
    struct at91dci_config_copy *cc, uint16_t refcount)
{
	at91dci_root_ctrl_poll(sc);
	return;
}

static void
at91dci_root_ctrl_done(struct usb2_xfer *xfer,
    struct usb2_sw_transfer *std)
{
	struct at91dci_softc *sc = xfer->usb2_sc;
	uint16_t value;
	uint16_t index;
	uint8_t use_polling;

	mtx_assert(&sc->sc_bus.mtx, MA_OWNED);

	if (std->state != USB_SW_TR_SETUP) {
		if (std->state == USB_SW_TR_PRE_CALLBACK) {
			/* transfer transferred */
			at91dci_device_done(xfer, std->err);
		}
		goto done;
	}
	/* buffer reset */
	std->ptr = USB_ADD_BYTES(&sc->sc_hub_temp, 0);
	std->len = 0;

	value = UGETW(std->req.wValue);
	index = UGETW(std->req.wIndex);

	use_polling = mtx_owned(xfer->priv_mtx) ? 1 : 0;

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
		std->len = sizeof(at91dci_devd);
		std->ptr = USB_ADD_BYTES(&at91dci_devd, 0);
		goto tr_valid;
	case UDESC_CONFIG:
		if (value & 0xff) {
			goto tr_stalled;
		}
		std->len = sizeof(at91dci_confd);
		std->ptr = USB_ADD_BYTES(&at91dci_confd, 0);
		goto tr_valid;
	case UDESC_STRING:
		switch (value & 0xff) {
		case 0:		/* Language table */
			std->len = sizeof(at91dci_langtab);
			std->ptr = USB_ADD_BYTES(&at91dci_langtab, 0);
			goto tr_valid;

		case 1:		/* Vendor */
			std->len = sizeof(at91dci_vendor);
			std->ptr = USB_ADD_BYTES(&at91dci_vendor, 0);
			goto tr_valid;

		case 2:		/* Product */
			std->len = sizeof(at91dci_product);
			std->ptr = USB_ADD_BYTES(&at91dci_product, 0);
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
			bzero(sc->sc_ep_flags, sizeof(sc->sc_ep_flags));
		}
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
	std->ptr = USB_ADD_BYTES(&at91dci_hubd, 0);
	std->len = sizeof(at91dci_hubd);
	goto tr_valid;

tr_stalled:
	std->err = USB_ERR_STALLED;
tr_valid:
done:
	return;
}

static void
at91dci_root_ctrl_poll(struct at91dci_softc *sc)
{
	usb2_sw_transfer(&sc->sc_root_ctrl,
	    &at91dci_root_ctrl_done);
	return;
}

struct usb2_pipe_methods at91dci_root_ctrl_methods =
{
	.open = at91dci_root_ctrl_open,
	.close = at91dci_root_ctrl_close,
	.enter = at91dci_root_ctrl_enter,
	.start = at91dci_root_ctrl_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 0,
};

/*------------------------------------------------------------------------*
 * at91dci root interrupt support
 *------------------------------------------------------------------------*/
static void
at91dci_root_intr_open(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_root_intr_close(struct usb2_xfer *xfer)
{
	struct at91dci_softc *sc = xfer->usb2_sc;

	if (sc->sc_root_intr.xfer == xfer) {
		sc->sc_root_intr.xfer = NULL;
	}
	at91dci_device_done(xfer, USB_ERR_CANCELLED);
	return;
}

static void
at91dci_root_intr_enter(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_root_intr_start(struct usb2_xfer *xfer)
{
	struct at91dci_softc *sc = xfer->usb2_sc;

	sc->sc_root_intr.xfer = xfer;
	return;
}

struct usb2_pipe_methods at91dci_root_intr_methods =
{
	.open = at91dci_root_intr_open,
	.close = at91dci_root_intr_close,
	.enter = at91dci_root_intr_enter,
	.start = at91dci_root_intr_start,
	.enter_is_cancelable = 1,
	.start_is_cancelable = 1,
};

static void
at91dci_xfer_setup(struct usb2_setup_params *parm)
{
	const struct usb2_hw_ep_profile *pf;
	struct at91dci_softc *sc;
	struct usb2_xfer *xfer;
	void *last_obj;
	uint32_t ntd;
	uint32_t n;
	uint8_t ep_no;

	sc = AT9100_DCI_BUS2SC(parm->udev->bus);
	xfer = parm->curr_xfer;

	/*
	 * setup xfer
	 */
	xfer->usb2_sc = sc;

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
			td->io_tag = sc->sc_io_tag;
			td->io_hdl = sc->sc_io_hdl;
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
	return;
}

static void
at91dci_xfer_unsetup(struct usb2_xfer *xfer)
{
	return;
}

static void
at91dci_pipe_init(struct usb2_device *udev, struct usb2_endpoint_descriptor *edesc,
    struct usb2_pipe *pipe)
{
	struct at91dci_softc *sc = AT9100_DCI_BUS2SC(udev->bus);

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
			pipe->methods = &at91dci_root_ctrl_methods;
			break;
		case UE_DIR_IN | AT9100_DCI_INTR_ENDPT:
			pipe->methods = &at91dci_root_intr_methods;
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
			pipe->methods = &at91dci_device_ctrl_methods;
			break;
		case UE_INTERRUPT:
			pipe->methods = &at91dci_device_intr_methods;
			break;
		case UE_ISOCHRONOUS:
			pipe->methods = &at91dci_device_isoc_fs_methods;
			break;
		case UE_BULK:
			pipe->methods = &at91dci_device_bulk_methods;
			break;
		default:
			/* do nothing */
			break;
		}
	}
	return;
}

struct usb2_bus_methods at91dci_bus_methods =
{
	.pipe_init = &at91dci_pipe_init,
	.xfer_setup = &at91dci_xfer_setup,
	.xfer_unsetup = &at91dci_xfer_unsetup,
	.do_poll = &at91dci_do_poll,
	.get_hw_ep_profile = &at91dci_get_hw_ep_profile,
	.set_stall = &at91dci_set_stall,
	.clear_stall = &at91dci_clear_stall,
	.vbus_interrupt = &at91dci_vbus_interrupt,
	.rem_wakeup_set = &at91dci_rem_wakeup_set,
};
