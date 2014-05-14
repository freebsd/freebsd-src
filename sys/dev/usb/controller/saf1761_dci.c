/* $FreeBSD$ */
/*-
 * Copyright (c) 2014 Hans Petter Selasky <hselasky@FreeBSD.org>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * This file contains the driver for the SAF1761 series USB OTG
 * controller.
 *
 * Datasheet is available from:
 * http://www.nxp.com/products/automotive/multimedia/usb/SAF1761BE.html
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

#define	USB_DEBUG_VAR saf1761_dci_debug

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
#endif					/* USB_GLOBAL_INCLUDE_FILE */

#include <dev/usb/controller/saf1761_dci.h>
#include <dev/usb/controller/saf1761_dci_reg.h>

#define	SAF1761_DCI_BUS2SC(bus) \
   ((struct saf1761_dci_softc *)(((uint8_t *)(bus)) - \
    ((uint8_t *)&(((struct saf1761_dci_softc *)0)->sc_bus))))

#ifdef USB_DEBUG
static int saf1761_dci_debug = 0;
static int saf1761_dci_forcefs = 0;

static 
SYSCTL_NODE(_hw_usb, OID_AUTO, saf1761_dci, CTLFLAG_RW, 0,
    "USB SAF1761 DCI");

SYSCTL_INT(_hw_usb_saf1761_dci, OID_AUTO, debug, CTLFLAG_RW,
    &saf1761_dci_debug, 0, "SAF1761 DCI debug level");
SYSCTL_INT(_hw_usb_saf1761_dci, OID_AUTO, forcefs, CTLFLAG_RW,
    &saf1761_dci_forcefs, 0, "SAF1761 DCI force FULL speed");
#endif

#define	SAF1761_DCI_INTR_ENDPT 1

/* prototypes */

static const struct usb_bus_methods saf1761_dci_bus_methods;
static const struct usb_pipe_methods saf1761_dci_device_non_isoc_methods;
static const struct usb_pipe_methods saf1761_dci_device_isoc_methods;

static saf1761_dci_cmd_t saf1761_dci_setup_rx;
static saf1761_dci_cmd_t saf1761_dci_data_rx;
static saf1761_dci_cmd_t saf1761_dci_data_tx;
static saf1761_dci_cmd_t saf1761_dci_data_tx_sync;
static void saf1761_dci_device_done(struct usb_xfer *, usb_error_t);
static void saf1761_dci_do_poll(struct usb_bus *);
static void saf1761_dci_standard_done(struct usb_xfer *);
static void saf1761_dci_intr_set(struct usb_xfer *, uint8_t);
static void saf1761_dci_root_intr(struct saf1761_dci_softc *);

/*
 * Here is a list of what the SAF1761 chip can support. The main
 * limitation is that the sum of the buffer sizes must be less than
 * 8192 bytes.
 */
static const struct usb_hw_ep_profile saf1761_dci_ep_profile[] = {

	[0] = {
		.max_in_frame_size = 64,
		.max_out_frame_size = 64,
		.is_simplex = 0,
		.support_control = 1,
	},
	[1] = {
		.max_in_frame_size = SOTG_HS_MAX_PACKET_SIZE,
		.max_out_frame_size = SOTG_HS_MAX_PACKET_SIZE,
		.is_simplex = 0,
		.support_interrupt = 1,
		.support_bulk = 1,
		.support_isochronous = 1,
		.support_in = 1,
		.support_out = 1,
	},
};

static void
saf1761_dci_get_hw_ep_profile(struct usb_device *udev,
    const struct usb_hw_ep_profile **ppf, uint8_t ep_addr)
{
	if (ep_addr == 0) {
		*ppf = saf1761_dci_ep_profile + 0;
	} else if (ep_addr < 8) {
		*ppf = saf1761_dci_ep_profile + 1;
	} else {
		*ppf = NULL;
	}
}

static void
saf1761_dci_pull_up(struct saf1761_dci_softc *sc)
{
	/* activate pullup on D+, if possible */

	if (!sc->sc_flags.d_pulled_up && sc->sc_flags.port_powered) {
		DPRINTF("\n");

		sc->sc_flags.d_pulled_up = 1;

		SAF1761_WRITE_2(sc, SOTG_CTRL_SET, SOTG_CTRL_DP_PULL_UP);
	}
}

static void
saf1761_dci_pull_down(struct saf1761_dci_softc *sc)
{
	/* release pullup on D+, if possible */

	if (sc->sc_flags.d_pulled_up) {
		DPRINTF("\n");

		sc->sc_flags.d_pulled_up = 0;

		SAF1761_WRITE_2(sc, SOTG_CTRL_CLR, SOTG_CTRL_DP_PULL_UP);
	}
}

static void
saf1761_dci_wakeup_peer(struct saf1761_dci_softc *sc)
{
	uint16_t temp;

	if (!(sc->sc_flags.status_suspend))
		return;

	DPRINTFN(5, "\n");

	temp = SAF1761_READ_2(sc, SOTG_MODE);
	SAF1761_WRITE_2(sc, SOTG_MODE, temp | SOTG_MODE_SNDRSU);
	SAF1761_WRITE_2(sc, SOTG_MODE, temp & ~SOTG_MODE_SNDRSU);

	/* Wait 8ms for remote wakeup to complete. */
	usb_pause_mtx(&sc->sc_bus.bus_mtx, hz / 125);

}

static void
saf1761_dci_set_address(struct saf1761_dci_softc *sc, uint8_t addr)
{
	DPRINTFN(5, "addr=%d\n", addr);

	SAF1761_WRITE_1(sc, SOTG_ADDRESS, addr | SOTG_ADDRESS_ENABLE);
}

static void
saf1761_read_fifo(struct saf1761_dci_softc *sc, void *buf, uint32_t len)
{
	bus_space_read_multi_1((sc)->sc_io_tag, (sc)->sc_io_hdl, SOTG_DATA_PORT, buf, len);
}

static void
saf1761_write_fifo(struct saf1761_dci_softc *sc, void *buf, uint32_t len)
{
	bus_space_write_multi_1((sc)->sc_io_tag, (sc)->sc_io_hdl, SOTG_DATA_PORT, buf, len);
}

static uint8_t
saf1761_dci_setup_rx(struct saf1761_dci_softc *sc, struct saf1761_dci_td *td)
{
	struct usb_device_request req;
	uint16_t count;

	/* select the correct endpoint */
	SAF1761_WRITE_1(sc, SOTG_EP_INDEX, SOTG_EP_INDEX_EP0SETUP);

	/* check buffer status */
	if ((SAF1761_READ_1(sc, SOTG_DCBUFFERSTATUS) &
	    SOTG_DCBUFFERSTATUS_FILLED_MASK) == 0)
		goto busy;

	/* read buffer length */
	count = SAF1761_READ_2(sc, SOTG_BUF_LENGTH);

	DPRINTFN(5, "count=%u rem=%u\n", count, td->remainder);

	/* clear did stall */
	td->did_stall = 0;

	/* clear stall */
	SAF1761_WRITE_1(sc, SOTG_CTRL_FUNC, 0);

	/* verify data length */
	if (count != td->remainder) {
		DPRINTFN(0, "Invalid SETUP packet "
		    "length, %d bytes\n", count);
		goto busy;
	}
	if (count != sizeof(req)) {
		DPRINTFN(0, "Unsupported SETUP packet "
		    "length, %d bytes\n", count);
		goto busy;
	}
	/* receive data */
	saf1761_read_fifo(sc, &req, sizeof(req));

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
	return (0);			/* complete */

busy:
	/* abort any ongoing transfer */
	if (!td->did_stall) {
		DPRINTFN(5, "stalling\n");

		/* set stall */
		SAF1761_WRITE_1(sc, SOTG_CTRL_FUNC, SOTG_CTRL_FUNC_STALL);

		td->did_stall = 1;
	}
	return (1);			/* not complete */
}

static uint8_t
saf1761_dci_data_rx(struct saf1761_dci_softc *sc, struct saf1761_dci_td *td)
{
	struct usb_page_search buf_res;
	uint16_t count;
	uint8_t got_short = 0;

	if (td->ep_index == 0) {
		/* select the correct endpoint */
		SAF1761_WRITE_1(sc, SOTG_EP_INDEX, SOTG_EP_INDEX_EP0SETUP);

		/* check buffer status */
		if ((SAF1761_READ_1(sc, SOTG_DCBUFFERSTATUS) &
		    SOTG_DCBUFFERSTATUS_FILLED_MASK) != 0) {

			if (td->remainder == 0) {
				/*
				 * We are actually complete and have
				 * received the next SETUP:
				 */
				DPRINTFN(5, "faking complete\n");
				return (0);	/* complete */
			}
			DPRINTFN(5, "SETUP packet while receiving data\n");
			/*
			 * USB Host Aborted the transfer.
			 */
			td->error = 1;
			return (0);	/* complete */
		}
	}
	/* select the correct endpoint */
	SAF1761_WRITE_1(sc, SOTG_EP_INDEX,
	    (td->ep_index << SOTG_EP_INDEX_ENDP_INDEX_SHIFT) |
	    SOTG_EP_INDEX_DIR_OUT);

	/* check buffer status */
	if ((SAF1761_READ_1(sc, SOTG_DCBUFFERSTATUS) &
	    SOTG_DCBUFFERSTATUS_FILLED_MASK) == 0) {
		return (1);		/* not complete */
	}
	/* read buffer length */
	count = SAF1761_READ_2(sc, SOTG_BUF_LENGTH);

	DPRINTFN(5, "rem=%u count=0x%04x\n", td->remainder, count);

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
		if (buf_res.length > count)
			buf_res.length = count;

		/* receive data */
		saf1761_read_fifo(sc, buf_res.buffer, buf_res.length);

		/* update counters */
		count -= buf_res.length;
		td->offset += buf_res.length;
		td->remainder -= buf_res.length;
	}
	/* check if we are complete */
	if ((td->remainder == 0) || got_short) {
		if (td->short_pkt) {
			/* we are complete */
			return (0);
		}
		/* else need to receive a zero length packet */
	}
	return (1);			/* not complete */
}

static uint8_t
saf1761_dci_data_tx(struct saf1761_dci_softc *sc, struct saf1761_dci_td *td)
{
	struct usb_page_search buf_res;
	uint16_t count;
	uint16_t count_old;

	if (td->ep_index == 0) {
		/* select the correct endpoint */
		SAF1761_WRITE_1(sc, SOTG_EP_INDEX, SOTG_EP_INDEX_EP0SETUP);

		/* check buffer status */
		if ((SAF1761_READ_1(sc, SOTG_DCBUFFERSTATUS) &
		    SOTG_DCBUFFERSTATUS_FILLED_MASK) != 0) {
			DPRINTFN(5, "SETUP abort\n");
			/*
			 * USB Host Aborted the transfer.
			 */
			td->error = 1;
			return (0);	/* complete */
		}
	}
	/* select the correct endpoint */
	SAF1761_WRITE_1(sc, SOTG_EP_INDEX,
	    (td->ep_index << SOTG_EP_INDEX_ENDP_INDEX_SHIFT) |
	    SOTG_EP_INDEX_DIR_IN);

	/* check buffer status */
	if ((SAF1761_READ_1(sc, SOTG_DCBUFFERSTATUS) &
	    SOTG_DCBUFFERSTATUS_FILLED_MASK) != 0) {
		return (1);		/* not complete */
	}
	DPRINTFN(5, "rem=%u\n", td->remainder);

	count = td->max_packet_size;
	if (td->remainder < count) {
		/* we have a short packet */
		td->short_pkt = 1;
		count = td->remainder;
	}
	count_old = count;

	while (count > 0) {

		usbd_get_page(td->pc, td->offset, &buf_res);

		/* get correct length */
		if (buf_res.length > count)
			buf_res.length = count;

		/* transmit data */
		saf1761_write_fifo(sc, buf_res.buffer, buf_res.length);

		/* update counters */
		count -= buf_res.length;
		td->offset += buf_res.length;
		td->remainder -= buf_res.length;
	}

	if (td->ep_index == 0) {
		if (count_old < SOTG_FS_MAX_PACKET_SIZE) {
			/* set end of packet */
			SAF1761_WRITE_1(sc, SOTG_CTRL_FUNC, SOTG_CTRL_FUNC_VENDP);
		}
	} else {
		if (count_old < SOTG_HS_MAX_PACKET_SIZE) {
			/* set end of packet */
			SAF1761_WRITE_1(sc, SOTG_CTRL_FUNC, SOTG_CTRL_FUNC_VENDP);
		}
	}

	/* check remainder */
	if (td->remainder == 0) {
		if (td->short_pkt) {
			return (0);	/* complete */
		}
		/* else we need to transmit a short packet */
	}
	return (1);			/* not complete */
}

static uint8_t
saf1761_dci_data_tx_sync(struct saf1761_dci_softc *sc, struct saf1761_dci_td *td)
{
	if (td->ep_index == 0) {
		/* select the correct endpoint */
		SAF1761_WRITE_1(sc, SOTG_EP_INDEX, SOTG_EP_INDEX_EP0SETUP);

		/* check buffer status */
		if ((SAF1761_READ_1(sc, SOTG_DCBUFFERSTATUS) &
		    SOTG_DCBUFFERSTATUS_FILLED_MASK) != 0) {
			DPRINTFN(5, "Faking complete\n");
			return (0);	/* complete */
		}
	}
	/* select the correct endpoint */
	SAF1761_WRITE_1(sc, SOTG_EP_INDEX,
	    (td->ep_index << SOTG_EP_INDEX_ENDP_INDEX_SHIFT) |
	    SOTG_EP_INDEX_DIR_IN);

	/* check buffer status */
	if ((SAF1761_READ_1(sc, SOTG_DCBUFFERSTATUS) &
	    SOTG_DCBUFFERSTATUS_FILLED_MASK) != 0)
		return (1);		/* busy */

	if (sc->sc_dv_addr != 0xFF) {
		/* write function address */
		saf1761_dci_set_address(sc, sc->sc_dv_addr);
	}
	return (0);			/* complete */
}

static uint8_t
saf1761_dci_xfer_do_fifo(struct saf1761_dci_softc *sc, struct usb_xfer *xfer)
{
	struct saf1761_dci_td *td;

	DPRINTFN(9, "\n");

	td = xfer->td_transfer_cache;
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
		 * Fetch the next transfer descriptor.
		 */
		td = td->obj_next;
		xfer->td_transfer_cache = td;
	}
	return (1);			/* not complete */

done:
	/* compute all actual lengths */

	saf1761_dci_standard_done(xfer);

	return (0);			/* complete */
}

static void
saf1761_dci_interrupt_poll(struct saf1761_dci_softc *sc)
{
	struct usb_xfer *xfer;

repeat:
	TAILQ_FOREACH(xfer, &sc->sc_bus.intr_q.head, wait_entry) {
		if (!saf1761_dci_xfer_do_fifo(sc, xfer)) {
			/* queue has been modified */
			goto repeat;
		}
	}
}

static void
saf1761_dci_wait_suspend(struct saf1761_dci_softc *sc, uint8_t on)
{
	if (on) {
		sc->sc_intr_enable |= SOTG_DCINTERRUPT_IESUSP;
		sc->sc_intr_enable &= ~SOTG_DCINTERRUPT_IERESM;
	} else {
		sc->sc_intr_enable &= ~SOTG_DCINTERRUPT_IESUSP;
		sc->sc_intr_enable |= SOTG_DCINTERRUPT_IERESM;
	}
	SAF1761_WRITE_4(sc, SOTG_DCINTERRUPT_EN, sc->sc_intr_enable);
}

static void
saf1761_dci_update_vbus(struct saf1761_dci_softc *sc)
{
	if (SAF1761_READ_4(sc, SOTG_MODE) & SOTG_MODE_VBUSSTAT) {
		DPRINTFN(4, "VBUS ON\n");

		/* VBUS present */
		if (!sc->sc_flags.status_vbus) {
			sc->sc_flags.status_vbus = 1;

			/* complete root HUB interrupt endpoint */
			saf1761_dci_root_intr(sc);
		}
	} else {
		DPRINTFN(4, "VBUS OFF\n");

		/* VBUS not-present */
		if (sc->sc_flags.status_vbus) {
			sc->sc_flags.status_vbus = 0;
			sc->sc_flags.status_bus_reset = 0;
			sc->sc_flags.status_suspend = 0;
			sc->sc_flags.change_suspend = 0;
			sc->sc_flags.change_connect = 1;

			/* complete root HUB interrupt endpoint */
			saf1761_dci_root_intr(sc);
		}
	}
}

void
saf1761_dci_interrupt(struct saf1761_dci_softc *sc)
{
	uint32_t status;

	USB_BUS_LOCK(&sc->sc_bus);

	status = SAF1761_READ_4(sc, SOTG_DCINTERRUPT);

	/* acknowledge all interrupts */
	SAF1761_WRITE_4(sc, SOTG_DCINTERRUPT, status);

	if (status & SOTG_DCINTERRUPT_IEVBUS) {
		/* update VBUS bit */
		saf1761_dci_update_vbus(sc);
	}
	if (status & SOTG_DCINTERRUPT_IEBRST) {
		sc->sc_flags.status_bus_reset = 1;
		sc->sc_flags.status_suspend = 0;
		sc->sc_flags.change_suspend = 0;
		sc->sc_flags.change_connect = 1;

		/* disable resume interrupt */
		saf1761_dci_wait_suspend(sc, 1);
		/* complete root HUB interrupt endpoint */
		saf1761_dci_root_intr(sc);
	}
	/*
	 * If "RESUME" and "SUSPEND" is set at the same time we
	 * interpret that like "RESUME". Resume is set when there is
	 * at least 3 milliseconds of inactivity on the USB BUS:
	 */
	if (status & SOTG_DCINTERRUPT_IERESM) {
		if (sc->sc_flags.status_suspend) {
			sc->sc_flags.status_suspend = 0;
			sc->sc_flags.change_suspend = 1;
			/* disable resume interrupt */
			saf1761_dci_wait_suspend(sc, 1);
			/* complete root HUB interrupt endpoint */
			saf1761_dci_root_intr(sc);
		}
	} else if (status & SOTG_DCINTERRUPT_IESUSP) {
		if (!sc->sc_flags.status_suspend) {
			sc->sc_flags.status_suspend = 1;
			sc->sc_flags.change_suspend = 1;
			/* enable resume interrupt */
			saf1761_dci_wait_suspend(sc, 0);
			/* complete root HUB interrupt endpoint */
			saf1761_dci_root_intr(sc);
		}
	}
	/* poll all active transfers */
	saf1761_dci_interrupt_poll(sc);

	USB_BUS_UNLOCK(&sc->sc_bus);
}

static void
saf1761_dci_setup_standard_chain_sub(struct saf1761_dci_std_temp *temp)
{
	struct saf1761_dci_td *td;

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
	td->did_stall = temp->did_stall;
	td->short_pkt = temp->short_pkt;
	td->alt_next = temp->setup_alt_next;
}

static void
saf1761_dci_setup_standard_chain(struct usb_xfer *xfer)
{
	struct saf1761_dci_std_temp temp;
	struct saf1761_dci_softc *sc;
	struct saf1761_dci_td *td;
	uint32_t x;
	uint8_t ep_no;

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
	temp.setup_alt_next = xfer->flags_int.short_frames_ok;
	temp.did_stall = !xfer->flags_int.control_stall;

	sc = SAF1761_DCI_BUS2SC(xfer->xroot->bus);
	ep_no = (xfer->endpointno & UE_ADDR);

	/* check if we should prepend a setup message */

	if (xfer->flags_int.control_xfr) {
		if (xfer->flags_int.control_hdr) {

			temp.func = &saf1761_dci_setup_rx;
			temp.len = xfer->frlengths[0];
			temp.pc = xfer->frbuffers + 0;
			temp.short_pkt = temp.len ? 1 : 0;
			/* check for last frame */
			if (xfer->nframes == 1) {
				/* no STATUS stage yet, SETUP is last */
				if (xfer->flags_int.control_act)
					temp.setup_alt_next = 0;
			}
			saf1761_dci_setup_standard_chain_sub(&temp);
		}
		x = 1;
	} else {
		x = 0;
	}

	if (x != xfer->nframes) {
		if (xfer->endpointno & UE_DIR_IN) {
			temp.func = &saf1761_dci_data_tx;
		} else {
			temp.func = &saf1761_dci_data_rx;
		}

		/* setup "pc" pointer */
		temp.pc = xfer->frbuffers + x;
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

		saf1761_dci_setup_standard_chain_sub(&temp);

		if (xfer->flags_int.isochronous_xfr) {
			temp.offset += temp.len;
		} else {
			/* get next Page Cache pointer */
			temp.pc = xfer->frbuffers + x;
		}
	}

	/* check for control transfer */
	if (xfer->flags_int.control_xfr) {
		uint8_t need_sync;

		/* always setup a valid "pc" pointer for status and sync */
		temp.pc = xfer->frbuffers + 0;
		temp.len = 0;
		temp.short_pkt = 0;
		temp.setup_alt_next = 0;

		/* check if we should append a status stage */
		if (!xfer->flags_int.control_act) {

			/*
			 * Send a DATA1 message and invert the current
			 * endpoint direction.
			 */
			if (xfer->endpointno & UE_DIR_IN) {
				temp.func = &saf1761_dci_data_rx;
				need_sync = 0;
			} else {
				temp.func = &saf1761_dci_data_tx;
				need_sync = 1;
			}
			temp.len = 0;
			temp.short_pkt = 0;

			saf1761_dci_setup_standard_chain_sub(&temp);
			if (need_sync) {
				/* we need a SYNC point after TX */
				temp.func = &saf1761_dci_data_tx_sync;
				saf1761_dci_setup_standard_chain_sub(&temp);
			}
		}
	}
	/* must have at least one frame! */
	td = temp.td;
	xfer->td_transfer_last = td;
}

static void
saf1761_dci_timeout(void *arg)
{
	struct usb_xfer *xfer = arg;

	DPRINTF("xfer=%p\n", xfer);

	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	/* transfer is transferred */
	saf1761_dci_device_done(xfer, USB_ERR_TIMEOUT);
}

static void
saf1761_dci_intr_set(struct usb_xfer *xfer, uint8_t set)
{
	struct saf1761_dci_softc *sc = SAF1761_DCI_BUS2SC(xfer->xroot->bus);
	uint8_t ep_no = (xfer->endpointno & UE_ADDR);
	uint32_t mask;

	DPRINTFN(15, "endpoint 0x%02x\n", xfer->endpointno);

	if (ep_no == 0) {
		mask = SOTG_DCINTERRUPT_IEPRX(0) |
		    SOTG_DCINTERRUPT_IEPTX(0) |
		    SOTG_DCINTERRUPT_IEP0SETUP;
	} else if (xfer->endpointno & UE_DIR_IN) {
		mask = SOTG_DCINTERRUPT_IEPTX(ep_no);
	} else {
		mask = SOTG_DCINTERRUPT_IEPRX(ep_no);
	}

	if (set)
		sc->sc_intr_enable |= mask;
	else
		sc->sc_intr_enable &= ~mask;

	SAF1761_WRITE_4(sc, SOTG_DCINTERRUPT_EN, sc->sc_intr_enable);
}

static void
saf1761_dci_start_standard_chain(struct usb_xfer *xfer)
{
	struct saf1761_dci_softc *sc = SAF1761_DCI_BUS2SC(xfer->xroot->bus);

	DPRINTFN(9, "\n");

	/* poll one time */
	if (saf1761_dci_xfer_do_fifo(sc, xfer)) {

		/*
		 * Only enable the endpoint interrupt when we are
		 * actually waiting for data, hence we are dealing
		 * with level triggered interrupts !
		 */
		saf1761_dci_intr_set(xfer, 1);

		/* put transfer on interrupt queue */
		usbd_transfer_enqueue(&xfer->xroot->bus->intr_q, xfer);

		/* start timeout, if any */
		if (xfer->timeout != 0) {
			usbd_transfer_timeout_ms(xfer,
			    &saf1761_dci_timeout, xfer->timeout);
		}
	}
}

static void
saf1761_dci_root_intr(struct saf1761_dci_softc *sc)
{
	DPRINTFN(9, "\n");

	USB_BUS_LOCK_ASSERT(&sc->sc_bus, MA_OWNED);

	/* set port bit - we only have one port */
	sc->sc_hub_idata[0] = 0x02;

	uhub_root_intr(&sc->sc_bus, sc->sc_hub_idata,
	    sizeof(sc->sc_hub_idata));
}

static usb_error_t
saf1761_dci_standard_done_sub(struct usb_xfer *xfer)
{
	struct saf1761_dci_td *td;
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
saf1761_dci_standard_done(struct usb_xfer *xfer)
{
	usb_error_t err = 0;

	DPRINTFN(13, "xfer=%p endpoint=%p transfer done\n",
	    xfer, xfer->endpoint);

	/* reset scanner */

	xfer->td_transfer_cache = xfer->td_transfer_first;

	if (xfer->flags_int.control_xfr) {

		if (xfer->flags_int.control_hdr) {

			err = saf1761_dci_standard_done_sub(xfer);
		}
		xfer->aframes = 1;

		if (xfer->td_transfer_cache == NULL) {
			goto done;
		}
	}
	while (xfer->aframes != xfer->nframes) {

		err = saf1761_dci_standard_done_sub(xfer);
		xfer->aframes++;

		if (xfer->td_transfer_cache == NULL) {
			goto done;
		}
	}

	if (xfer->flags_int.control_xfr &&
	    !xfer->flags_int.control_act) {

		err = saf1761_dci_standard_done_sub(xfer);
	}
done:
	saf1761_dci_device_done(xfer, err);
}

/*------------------------------------------------------------------------*
 *	saf1761_dci_device_done
 *
 * NOTE: this function can be called more than one time on the
 * same USB transfer!
 *------------------------------------------------------------------------*/
static void
saf1761_dci_device_done(struct usb_xfer *xfer, usb_error_t error)
{
	USB_BUS_LOCK_ASSERT(xfer->xroot->bus, MA_OWNED);

	DPRINTFN(2, "xfer=%p, endpoint=%p, error=%d\n",
	    xfer, xfer->endpoint, error);

	if (xfer->flags_int.usb_mode == USB_MODE_DEVICE)
		saf1761_dci_intr_set(xfer, 0);

	/* dequeue transfer and start next transfer */
	usbd_transfer_done(xfer, error);
}

static void
saf1761_dci_xfer_stall(struct usb_xfer *xfer)
{
	saf1761_dci_device_done(xfer, USB_ERR_STALLED);
}

static void
saf1761_dci_set_stall(struct usb_device *udev,
    struct usb_endpoint *ep, uint8_t *did_stall)
{
	struct saf1761_dci_softc *sc;
	uint8_t ep_no;
	uint8_t ep_type;
	uint8_t ep_dir;

	USB_BUS_LOCK_ASSERT(udev->bus, MA_OWNED);

	DPRINTFN(5, "endpoint=%p\n", ep);

	/* set FORCESTALL */
	sc = SAF1761_DCI_BUS2SC(udev->bus);
	ep_no = (ep->edesc->bEndpointAddress & UE_ADDR);
	ep_dir = (ep->edesc->bEndpointAddress & (UE_DIR_IN | UE_DIR_OUT));
	ep_type = (ep->edesc->bmAttributes & UE_XFERTYPE);

	if (ep_type == UE_CONTROL) {
		/* should not happen */
		return;
	}
	/* select the correct endpoint */
	SAF1761_WRITE_1(sc, SOTG_EP_INDEX,
	    (ep_no << SOTG_EP_INDEX_ENDP_INDEX_SHIFT) |
	    ((ep_dir == UE_DIR_IN) ? SOTG_EP_INDEX_DIR_IN :
	    SOTG_EP_INDEX_DIR_OUT));

	/* set stall */
	SAF1761_WRITE_1(sc, SOTG_CTRL_FUNC, SOTG_CTRL_FUNC_STALL);
}

static void
saf1761_dci_clear_stall_sub(struct saf1761_dci_softc *sc,
    uint8_t ep_no, uint8_t ep_type, uint8_t ep_dir)
{
	if (ep_type == UE_CONTROL) {
		/* clearing stall is not needed */
		return;
	}
	/* select the correct endpoint */
	SAF1761_WRITE_1(sc, SOTG_EP_INDEX,
	    (ep_no << SOTG_EP_INDEX_ENDP_INDEX_SHIFT) |
	    ((ep_dir == UE_DIR_IN) ? SOTG_EP_INDEX_DIR_IN :
	    SOTG_EP_INDEX_DIR_OUT));

	/* disable endpoint */
	SAF1761_WRITE_2(sc, SOTG_EP_TYPE, 0);
	/* enable endpoint again - will clear data toggle */
	SAF1761_WRITE_2(sc, SOTG_EP_TYPE, ep_type | SOTG_EP_TYPE_ENABLE);

	/* clear buffer */
	SAF1761_WRITE_1(sc, SOTG_CTRL_FUNC, SOTG_CTRL_FUNC_CLBUF);
	/* clear stall */
	SAF1761_WRITE_1(sc, SOTG_CTRL_FUNC, 0);
}

static void
saf1761_dci_clear_stall(struct usb_device *udev, struct usb_endpoint *ep)
{
	struct saf1761_dci_softc *sc;
	struct usb_endpoint_descriptor *ed;

	USB_BUS_LOCK_ASSERT(udev->bus, MA_OWNED);

	DPRINTFN(5, "endpoint=%p\n", ep);

	/* check mode */
	if (udev->flags.usb_mode != USB_MODE_DEVICE) {
		/* not supported */
		return;
	}
	/* get softc */
	sc = SAF1761_DCI_BUS2SC(udev->bus);

	/* get endpoint descriptor */
	ed = ep->edesc;

	/* reset endpoint */
	saf1761_dci_clear_stall_sub(sc,
	    (ed->bEndpointAddress & UE_ADDR),
	    (ed->bmAttributes & UE_XFERTYPE),
	    (ed->bEndpointAddress & (UE_DIR_IN | UE_DIR_OUT)));
}

usb_error_t
saf1761_dci_init(struct saf1761_dci_softc *sc)
{
	const struct usb_hw_ep_profile *pf;
	uint8_t x;

	DPRINTF("start\n");

	/* set up the bus structure */
	sc->sc_bus.usbrev = USB_REV_2_0;
	sc->sc_bus.methods = &saf1761_dci_bus_methods;

	USB_BUS_LOCK(&sc->sc_bus);

	/* reset device */
	SAF1761_WRITE_2(sc, SOTG_MODE, SOTG_MODE_SFRESET);
	SAF1761_WRITE_2(sc, SOTG_MODE, 0);

	/* wait a bit */
	DELAY(1000);

	/* do a pulldown */
	saf1761_dci_pull_down(sc);

	/* wait 10ms for pulldown to stabilise */
	usb_pause_mtx(&sc->sc_bus.bus_mtx, hz / 100);

	for (x = 0;; x++) {

		saf1761_dci_get_hw_ep_profile(NULL, &pf, x);
		if (pf == NULL)
			break;

		/* select the correct endpoint */
		SAF1761_WRITE_1(sc, SOTG_EP_INDEX,
		    (x << SOTG_EP_INDEX_ENDP_INDEX_SHIFT) |
		    SOTG_EP_INDEX_DIR_IN);

		/* select the maximum packet size */
		SAF1761_WRITE_2(sc, SOTG_EP_MAXPACKET, pf->max_in_frame_size);

		if (x == 0) {
			/* enable control endpoint */
			SAF1761_WRITE_2(sc, SOTG_EP_TYPE, SOTG_EP_TYPE_ENABLE);
		}
		/* select the correct endpoint */
		SAF1761_WRITE_1(sc, SOTG_EP_INDEX,
		    (x << SOTG_EP_INDEX_ENDP_INDEX_SHIFT) |
		    SOTG_EP_INDEX_DIR_OUT);

		/* select the maximum packet size */
		SAF1761_WRITE_2(sc, SOTG_EP_MAXPACKET, pf->max_out_frame_size);

		if (x == 0) {
			/* enable control endpoint */
			SAF1761_WRITE_2(sc, SOTG_EP_TYPE, SOTG_EP_TYPE_ENABLE);
		}
	}

	/* enable interrupts */
	SAF1761_WRITE_2(sc, SOTG_MODE, SOTG_MODE_GLINTENA);

	/* enable interrupts */
	sc->sc_intr_enable = SOTG_DCINTERRUPT_EN, SOTG_DCINTERRUPT_IEVBUS |
	    SOTG_DCINTERRUPT_IEBRST | SOTG_DCINTERRUPT_IESUSP;
	SAF1761_WRITE_2(sc, SOTG_DCINTERRUPT_EN, sc->sc_intr_enable);

	/* poll initial VBUS status */
	saf1761_dci_update_vbus(sc);

	USB_BUS_UNLOCK(&sc->sc_bus);

	/* catch any lost interrupts */

	saf1761_dci_do_poll(&sc->sc_bus);

	return (0);			/* success */
}

void
saf1761_dci_uninit(struct saf1761_dci_softc *sc)
{
	USB_BUS_LOCK(&sc->sc_bus);

	/* disable all interrupts */
	SAF1761_WRITE_2(sc, SOTG_MODE, 0);

	sc->sc_flags.port_powered = 0;
	sc->sc_flags.status_vbus = 0;
	sc->sc_flags.status_bus_reset = 0;
	sc->sc_flags.status_suspend = 0;
	sc->sc_flags.change_suspend = 0;
	sc->sc_flags.change_connect = 1;

	saf1761_dci_pull_down(sc);
	USB_BUS_UNLOCK(&sc->sc_bus);
}

static void
saf1761_dci_suspend(struct saf1761_dci_softc *sc)
{
	/* TODO */
}

static void
saf1761_dci_resume(struct saf1761_dci_softc *sc)
{
	/* TODO */
}

static void
saf1761_dci_do_poll(struct usb_bus *bus)
{
	struct saf1761_dci_softc *sc = SAF1761_DCI_BUS2SC(bus);

	USB_BUS_LOCK(&sc->sc_bus);
	saf1761_dci_interrupt_poll(sc);
	USB_BUS_UNLOCK(&sc->sc_bus);
}

/*------------------------------------------------------------------------*
 * saf1761_dci control support
 * saf1761_dci interrupt support
 * saf1761_dci bulk support
 *------------------------------------------------------------------------*/
static void
saf1761_dci_device_non_isoc_open(struct usb_xfer *xfer)
{
	return;
}

static void
saf1761_dci_device_non_isoc_close(struct usb_xfer *xfer)
{
	saf1761_dci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
saf1761_dci_device_non_isoc_enter(struct usb_xfer *xfer)
{
	return;
}

static void
saf1761_dci_device_non_isoc_start(struct usb_xfer *xfer)
{
	/* setup TDs */
	saf1761_dci_setup_standard_chain(xfer);
	saf1761_dci_start_standard_chain(xfer);
}

static const struct usb_pipe_methods saf1761_dci_device_non_isoc_methods =
{
	.open = saf1761_dci_device_non_isoc_open,
	.close = saf1761_dci_device_non_isoc_close,
	.enter = saf1761_dci_device_non_isoc_enter,
	.start = saf1761_dci_device_non_isoc_start,
};

/*------------------------------------------------------------------------*
 * saf1761_dci isochronous support
 *------------------------------------------------------------------------*/
static void
saf1761_dci_device_isoc_open(struct usb_xfer *xfer)
{
	return;
}

static void
saf1761_dci_device_isoc_close(struct usb_xfer *xfer)
{
	saf1761_dci_device_done(xfer, USB_ERR_CANCELLED);
}

static void
saf1761_dci_device_isoc_enter(struct usb_xfer *xfer)
{
	struct saf1761_dci_softc *sc = SAF1761_DCI_BUS2SC(xfer->xroot->bus);
	uint32_t temp;
	uint32_t nframes;

	DPRINTFN(6, "xfer=%p next=%d nframes=%d\n",
	    xfer, xfer->endpoint->isoc_next, xfer->nframes);

	/* get the current frame index - we don't need the high bits */

	nframes = SAF1761_READ_2(sc, SOTG_FRAME_NUM);

	/*
	 * check if the frame index is within the window where the
	 * frames will be inserted
	 */
	temp = (nframes - xfer->endpoint->isoc_next) & SOTG_FRAME_NUM_SOFR_MASK;

	if ((xfer->endpoint->is_synced == 0) ||
	    (temp < xfer->nframes)) {
		/*
		 * If there is data underflow or the pipe queue is
		 * empty we schedule the transfer a few frames ahead
		 * of the current frame position. Else two isochronous
		 * transfers might overlap.
		 */
		xfer->endpoint->isoc_next = (nframes + 3) & SOTG_FRAME_NUM_SOFR_MASK;
		xfer->endpoint->is_synced = 1;
		DPRINTFN(3, "start next=%d\n", xfer->endpoint->isoc_next);
	}
	/*
	 * compute how many milliseconds the insertion is ahead of the
	 * current frame position:
	 */
	temp = (xfer->endpoint->isoc_next - nframes) & SOTG_FRAME_NUM_SOFR_MASK;

	/*
	 * pre-compute when the isochronous transfer will be finished:
	 */
	xfer->isoc_time_complete =
	    usb_isoc_time_expand(&sc->sc_bus, nframes) + temp +
	    xfer->nframes;

	/* compute frame number for next insertion */
	xfer->endpoint->isoc_next += xfer->nframes;

	/* setup TDs */
	saf1761_dci_setup_standard_chain(xfer);
}

static void
saf1761_dci_device_isoc_start(struct usb_xfer *xfer)
{
	/* start TD chain */
	saf1761_dci_start_standard_chain(xfer);
}

static const struct usb_pipe_methods saf1761_dci_device_isoc_methods =
{
	.open = saf1761_dci_device_isoc_open,
	.close = saf1761_dci_device_isoc_close,
	.enter = saf1761_dci_device_isoc_enter,
	.start = saf1761_dci_device_isoc_start,
};

/*------------------------------------------------------------------------*
 * saf1761_dci root control support
 *------------------------------------------------------------------------*
 * Simulate a hardware HUB by handling all the necessary requests.
 *------------------------------------------------------------------------*/

static const struct usb_device_descriptor saf1761_dci_devd = {
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

static const struct usb_device_qualifier saf1761_dci_odevd = {
	.bLength = sizeof(struct usb_device_qualifier),
	.bDescriptorType = UDESC_DEVICE_QUALIFIER,
	.bcdUSB = {0x00, 0x02},
	.bDeviceClass = UDCLASS_HUB,
	.bDeviceSubClass = UDSUBCLASS_HUB,
	.bDeviceProtocol = UDPROTO_FSHUB,
	.bMaxPacketSize0 = 0,
	.bNumConfigurations = 0,
};

static const struct saf1761_dci_config_desc saf1761_dci_confd = {
	.confd = {
		.bLength = sizeof(struct usb_config_descriptor),
		.bDescriptorType = UDESC_CONFIG,
		.wTotalLength[0] = sizeof(saf1761_dci_confd),
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
		.bEndpointAddress = (UE_DIR_IN | SAF1761_DCI_INTR_ENDPT),
		.bmAttributes = UE_INTERRUPT,
		.wMaxPacketSize[0] = 8,
		.bInterval = 255,
	},
};

#define	HSETW(ptr, val) ptr = { (uint8_t)(val), (uint8_t)((val) >> 8) }

static const struct usb_hub_descriptor_min saf1761_dci_hubd = {
	.bDescLength = sizeof(saf1761_dci_hubd),
	.bDescriptorType = UDESC_HUB,
	.bNbrPorts = 1,
	HSETW(.wHubCharacteristics, (UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL)),
	.bPwrOn2PwrGood = 50,
	.bHubContrCurrent = 0,
	.DeviceRemovable = {0},		/* port is removable */
};

#define	STRING_VENDOR \
  "N\0X\0P"

#define	STRING_PRODUCT \
  "D\0C\0I\0 \0R\0o\0o\0t\0 \0H\0U\0B"

USB_MAKE_STRING_DESC(STRING_VENDOR, saf1761_dci_vendor);
USB_MAKE_STRING_DESC(STRING_PRODUCT, saf1761_dci_product);

static usb_error_t
saf1761_dci_roothub_exec(struct usb_device *udev,
    struct usb_device_request *req, const void **pptr, uint16_t *plength)
{
	struct saf1761_dci_softc *sc = SAF1761_DCI_BUS2SC(udev->bus);
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
		if (value & 0xff)
			goto tr_stalled;
		len = sizeof(saf1761_dci_devd);
		ptr = (const void *)&saf1761_dci_devd;
		goto tr_valid;
	case UDESC_DEVICE_QUALIFIER:
		if (value & 0xff)
			goto tr_stalled;
		len = sizeof(saf1761_dci_odevd);
		ptr = (const void *)&saf1761_dci_odevd;
		goto tr_valid;
	case UDESC_CONFIG:
		if (value & 0xff)
			goto tr_stalled;
		len = sizeof(saf1761_dci_confd);
		ptr = (const void *)&saf1761_dci_confd;
		goto tr_valid;
	case UDESC_STRING:
		switch (value & 0xff) {
		case 0:		/* Language table */
			len = sizeof(usb_string_lang_en);
			ptr = (const void *)&usb_string_lang_en;
			goto tr_valid;

		case 1:		/* Vendor */
			len = sizeof(saf1761_dci_vendor);
			ptr = (const void *)&saf1761_dci_vendor;
			goto tr_valid;

		case 2:		/* Product */
			len = sizeof(saf1761_dci_product);
			ptr = (const void *)&saf1761_dci_product;
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
	if (value & 0xFF00)
		goto tr_stalled;

	sc->sc_rt_addr = value;
	goto tr_valid;

tr_handle_set_config:
	if (value >= 2)
		goto tr_stalled;
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
	if (index != 1)
		goto tr_stalled;
	DPRINTFN(9, "UR_CLEAR_PORT_FEATURE on port %d\n", index);

	switch (value) {
	case UHF_PORT_SUSPEND:
		saf1761_dci_wakeup_peer(sc);
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
		saf1761_dci_pull_down(sc);
		break;
	case UHF_C_PORT_CONNECTION:
		sc->sc_flags.change_connect = 0;
		break;
	case UHF_C_PORT_SUSPEND:
		sc->sc_flags.change_suspend = 0;
		break;
	default:
		err = USB_ERR_IOERROR;
		goto tr_valid;
	}
	goto tr_valid;

tr_handle_set_port_feature:
	if (index != 1)
		goto tr_stalled;
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
		goto tr_valid;
	}
	goto tr_valid;

tr_handle_get_port_status:

	DPRINTFN(9, "UR_GET_PORT_STATUS\n");

	if (index != 1)
		goto tr_stalled;

	if (sc->sc_flags.status_vbus) {
		saf1761_dci_pull_up(sc);
	} else {
		saf1761_dci_pull_down(sc);
	}

	/* Select FULL-speed and Device Side Mode */

	value = UPS_PORT_MODE_DEVICE;

	if (sc->sc_flags.port_powered)
		value |= UPS_PORT_POWER;

	if (sc->sc_flags.port_enabled)
		value |= UPS_PORT_ENABLED;

	if (sc->sc_flags.status_vbus &&
	    sc->sc_flags.status_bus_reset)
		value |= UPS_CURRENT_CONNECT_STATUS;

	if (sc->sc_flags.status_suspend)
		value |= UPS_SUSPEND;

	USETW(sc->sc_hub_temp.ps.wPortStatus, value);

	value = 0;

	if (sc->sc_flags.change_connect)
		value |= UPS_C_CONNECT_STATUS;

	if (sc->sc_flags.change_suspend)
		value |= UPS_C_SUSPEND;

	USETW(sc->sc_hub_temp.ps.wPortChange, value);
	len = sizeof(sc->sc_hub_temp.ps);
	goto tr_valid;

tr_handle_get_class_descriptor:
	if (value & 0xFF)
		goto tr_stalled;
	ptr = (const void *)&saf1761_dci_hubd;
	len = sizeof(saf1761_dci_hubd);
	goto tr_valid;

tr_stalled:
	err = USB_ERR_STALLED;
tr_valid:
	*plength = len;
	*pptr = ptr;
	return (err);
}

static void
saf1761_dci_xfer_setup(struct usb_setup_params *parm)
{
	const struct usb_hw_ep_profile *pf;
	struct saf1761_dci_softc *sc;
	struct usb_xfer *xfer;
	void *last_obj;
	uint32_t ntd;
	uint32_t n;
	uint8_t ep_no;
	uint8_t ep_type;

	sc = SAF1761_DCI_BUS2SC(parm->udev->bus);
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
	 * Compute maximum number of TDs:
	 */
	ep_type = (xfer->endpoint->edesc->bmAttributes & UE_XFERTYPE);

	if (ep_type == UE_CONTROL) {

		ntd = xfer->nframes + 1 /* STATUS */ + 1 /* SYNC */ ;

	} else {
		ntd = xfer->nframes + 1 /* SYNC */ ;
	}

	/*
	 * check if "usbd_transfer_setup_sub" set an error
	 */
	if (parm->err)
		return;

	/*
	 * allocate transfer descriptors
	 */
	last_obj = NULL;

	/*
	 * get profile stuff
	 */
	if (ntd) {

		ep_no = xfer->endpointno & UE_ADDR;
		saf1761_dci_get_hw_ep_profile(parm->udev, &pf, ep_no);

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

		struct saf1761_dci_td *td;

		if (parm->buf) {

			td = USB_ADD_BYTES(parm->buf, parm->size[0]);

			/* init TD */
			td->max_packet_size = xfer->max_packet_size;
			td->ep_index = ep_no;

			td->obj_next = last_obj;

			last_obj = td;
		}
		parm->size[0] += sizeof(*td);
	}

	xfer->td_start[0] = last_obj;
}

static void
saf1761_dci_xfer_unsetup(struct usb_xfer *xfer)
{
}

static void
saf1761_dci_ep_init(struct usb_device *udev, struct usb_endpoint_descriptor *edesc,
    struct usb_endpoint *ep)
{
	struct saf1761_dci_softc *sc = SAF1761_DCI_BUS2SC(udev->bus);

	DPRINTFN(2, "endpoint=%p, addr=%d, endpt=%d, mode=%d (%d)\n",
	    ep, udev->address,
	    edesc->bEndpointAddress, udev->flags.usb_mode,
	    sc->sc_rt_addr);

	if (udev->device_index != sc->sc_rt_addr) {
		if (udev->speed != USB_SPEED_FULL &&
		    udev->speed != USB_SPEED_HIGH) {
			/* not supported */
			return;
		}
		switch (edesc->bmAttributes & UE_XFERTYPE) {
		case UE_ISOCHRONOUS:
			ep->methods = &saf1761_dci_device_isoc_methods;
			break;
		default:
			ep->methods = &saf1761_dci_device_non_isoc_methods;
			break;
		}
	}
}

static void
saf1761_dci_set_hw_power_sleep(struct usb_bus *bus, uint32_t state)
{
	struct saf1761_dci_softc *sc = SAF1761_DCI_BUS2SC(bus);

	switch (state) {
	case USB_HW_POWER_SUSPEND:
		saf1761_dci_suspend(sc);
		break;
	case USB_HW_POWER_SHUTDOWN:
		saf1761_dci_uninit(sc);
		break;
	case USB_HW_POWER_RESUME:
		saf1761_dci_resume(sc);
		break;
	default:
		break;
	}
}

static const struct usb_bus_methods saf1761_dci_bus_methods =
{
	.endpoint_init = &saf1761_dci_ep_init,
	.xfer_setup = &saf1761_dci_xfer_setup,
	.xfer_unsetup = &saf1761_dci_xfer_unsetup,
	.get_hw_ep_profile = &saf1761_dci_get_hw_ep_profile,
	.xfer_stall = &saf1761_dci_xfer_stall,
	.set_stall = &saf1761_dci_set_stall,
	.clear_stall = &saf1761_dci_clear_stall,
	.roothub_exec = &saf1761_dci_roothub_exec,
	.xfer_poll = &saf1761_dci_do_poll,
	.set_hw_power_sleep = saf1761_dci_set_hw_power_sleep,
};
