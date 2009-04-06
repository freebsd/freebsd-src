/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * HID spec: http://www.usb.org/developers/devclass_docs/HID1_11.pdf
 */

#include "usbdevs.h"
#include <dev/usb/usb.h>
#include <dev/usb/usb_mfunc.h>
#include <dev/usb/usb_error.h>
#include <dev/usb/usbhid.h>

#define	USB_DEBUG_VAR ums_debug

#include <dev/usb/usb_core.h>
#include <dev/usb/usb_util.h>
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_transfer.h>
#include <dev/usb/usb_request.h>
#include <dev/usb/usb_dynamic.h>
#include <dev/usb/usb_mbuf.h>
#include <dev/usb/usb_dev.h>
#include <dev/usb/usb_hid.h>

#include <dev/usb/quirk/usb_quirk.h>

#include <sys/ioccom.h>
#include <sys/filio.h>
#include <sys/tty.h>
#include <sys/mouse.h>

#if USB_DEBUG
static int ums_debug = 0;

SYSCTL_NODE(_hw_usb2, OID_AUTO, ums, CTLFLAG_RW, 0, "USB ums");
SYSCTL_INT(_hw_usb2_ums, OID_AUTO, debug, CTLFLAG_RW,
    &ums_debug, 0, "Debug level");
#endif

#define	MOUSE_FLAGS_MASK (HIO_CONST|HIO_RELATIVE)
#define	MOUSE_FLAGS (HIO_RELATIVE)

#define	UMS_BUF_SIZE      8		/* bytes */
#define	UMS_IFQ_MAXLEN   50		/* units */
#define	UMS_BUTTON_MAX   31		/* exclusive, must be less than 32 */
#define	UMS_BUT(i) ((i) < 3 ? (((i) + 2) % 3) : (i))
#define	UMS_INFO_MAX	  2		/* maximum number of HID sets */

enum {
	UMS_INTR_DT,
	UMS_N_TRANSFER,
};

struct ums_info {
	struct hid_location sc_loc_w;
	struct hid_location sc_loc_x;
	struct hid_location sc_loc_y;
	struct hid_location sc_loc_z;
	struct hid_location sc_loc_t;
	struct hid_location sc_loc_btn[UMS_BUTTON_MAX];

	uint32_t sc_flags;
#define	UMS_FLAG_X_AXIS     0x0001
#define	UMS_FLAG_Y_AXIS     0x0002
#define	UMS_FLAG_Z_AXIS     0x0004
#define	UMS_FLAG_T_AXIS     0x0008
#define	UMS_FLAG_SBU        0x0010	/* spurious button up events */
#define	UMS_FLAG_REVZ	    0x0020	/* Z-axis is reversed */
#define	UMS_FLAG_W_AXIS     0x0040

	uint8_t	sc_iid_w;
	uint8_t	sc_iid_x;
	uint8_t	sc_iid_y;
	uint8_t	sc_iid_z;
	uint8_t	sc_iid_t;
	uint8_t	sc_iid_btn[UMS_BUTTON_MAX];
	uint8_t	sc_buttons;
};

struct ums_softc {
	struct usb2_fifo_sc sc_fifo;
	struct mtx sc_mtx;
	struct usb2_callout sc_callout;
	struct ums_info sc_info[UMS_INFO_MAX];

	mousehw_t sc_hw;
	mousemode_t sc_mode;
	mousestatus_t sc_status;

	struct usb2_xfer *sc_xfer[UMS_N_TRANSFER];

	uint8_t	sc_buttons;
	uint8_t	sc_iid;
	uint8_t	sc_temp[64];
};

static void ums_put_queue_timeout(void *__sc);

static usb2_callback_t ums_intr_callback;

static device_probe_t ums_probe;
static device_attach_t ums_attach;
static device_detach_t ums_detach;

static usb2_fifo_cmd_t ums_start_read;
static usb2_fifo_cmd_t ums_stop_read;
static usb2_fifo_open_t ums_open;
static usb2_fifo_close_t ums_close;
static usb2_fifo_ioctl_t ums_ioctl;

static void ums_put_queue(struct ums_softc *sc, int32_t dx, int32_t dy, int32_t dz, int32_t dt, int32_t buttons);

static struct usb2_fifo_methods ums_fifo_methods = {
	.f_open = &ums_open,
	.f_close = &ums_close,
	.f_ioctl = &ums_ioctl,
	.f_start_read = &ums_start_read,
	.f_stop_read = &ums_stop_read,
	.basename[0] = "ums",
};

static void
ums_put_queue_timeout(void *__sc)
{
	struct ums_softc *sc = __sc;

	mtx_assert(&sc->sc_mtx, MA_OWNED);

	ums_put_queue(sc, 0, 0, 0, 0, 0);
}

static void
ums_intr_callback(struct usb2_xfer *xfer)
{
	struct ums_softc *sc = xfer->priv_sc;
	struct ums_info *info = &sc->sc_info[0];
	uint8_t *buf = sc->sc_temp;
	uint16_t len = xfer->actlen;
	int32_t buttons = 0;
	int32_t dw = 0;
	int32_t dx = 0;
	int32_t dy = 0;
	int32_t dz = 0;
	int32_t dt = 0;
	uint8_t i;
	uint8_t id;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		DPRINTFN(6, "sc=%p actlen=%d\n", sc, len);

		if (len > sizeof(sc->sc_temp)) {
			DPRINTFN(6, "truncating large packet to %zu bytes\n",
			    sizeof(sc->sc_temp));
			len = sizeof(sc->sc_temp);
		}
		if (len == 0)
			goto tr_setup;

		usb2_copy_out(xfer->frbuffers, 0, buf, len);

		DPRINTFN(6, "data = %02x %02x %02x %02x "
		    "%02x %02x %02x %02x\n",
		    (len > 0) ? buf[0] : 0, (len > 1) ? buf[1] : 0,
		    (len > 2) ? buf[2] : 0, (len > 3) ? buf[3] : 0,
		    (len > 4) ? buf[4] : 0, (len > 5) ? buf[5] : 0,
		    (len > 6) ? buf[6] : 0, (len > 7) ? buf[7] : 0);

		if (sc->sc_iid) {
			id = *buf;

			len--;
			buf++;

		} else {
			id = 0;
			if (sc->sc_info[0].sc_flags & UMS_FLAG_SBU) {
				if ((*buf == 0x14) || (*buf == 0x15)) {
					goto tr_setup;
				}
			}
		}

	repeat:
		if ((info->sc_flags & UMS_FLAG_W_AXIS) &&
		    (id == info->sc_iid_w))
			dw += hid_get_data(buf, len, &info->sc_loc_w);

		if ((info->sc_flags & UMS_FLAG_X_AXIS) && 
		    (id == info->sc_iid_x))
			dx += hid_get_data(buf, len, &info->sc_loc_x);

		if ((info->sc_flags & UMS_FLAG_Y_AXIS) &&
		    (id == info->sc_iid_y))
			dy = -hid_get_data(buf, len, &info->sc_loc_y);

		if ((info->sc_flags & UMS_FLAG_Z_AXIS) &&
		    (id == info->sc_iid_z)) {
			int32_t temp;
			temp = hid_get_data(buf, len, &info->sc_loc_z);
			if (info->sc_flags & UMS_FLAG_REVZ)
				temp = -temp;
			dz -= temp;
		}

		if ((info->sc_flags & UMS_FLAG_T_AXIS) &&
		    (id == info->sc_iid_t))
			dt -= hid_get_data(buf, len, &info->sc_loc_t);

		for (i = 0; i < info->sc_buttons; i++) {
			if (id != info->sc_iid_btn[i])
				continue;
			if (hid_get_data(buf, len, &info->sc_loc_btn[i])) {
				buttons |= (1 << UMS_BUT(i));
			}
		}

		if (++info != &sc->sc_info[UMS_INFO_MAX])
			goto repeat;

		if (dx || dy || dz || dt || dw ||
		    (buttons != sc->sc_status.button)) {

			DPRINTFN(6, "x:%d y:%d z:%d t:%d w:%d buttons:0x%08x\n",
			    dx, dy, dz, dt, dw, buttons);

			sc->sc_status.button = buttons;
			sc->sc_status.dx += dx;
			sc->sc_status.dy += dy;
			sc->sc_status.dz += dz;
			/*
			 * sc->sc_status.dt += dt;
			 * no way to export this yet
			 */

			/*
		         * The Qtronix keyboard has a built in PS/2
		         * port for a mouse.  The firmware once in a
		         * while posts a spurious button up
		         * event. This event we ignore by doing a
		         * timeout for 50 msecs.  If we receive
		         * dx=dy=dz=buttons=0 before we add the event
		         * to the queue.  In any other case we delete
		         * the timeout event.
		         */
			if ((sc->sc_info[0].sc_flags & UMS_FLAG_SBU) &&
			    (dx == 0) && (dy == 0) && (dz == 0) && (dt == 0) &&
			    (dw == 0) && (buttons == 0)) {

				usb2_callout_reset(&sc->sc_callout, hz / 20,
				    &ums_put_queue_timeout, sc);
			} else {

				usb2_callout_stop(&sc->sc_callout);

				ums_put_queue(sc, dx, dy, dz, dt, buttons);
			}
		}
	case USB_ST_SETUP:
tr_setup:
		/* check if we can put more data into the FIFO */
		if (usb2_fifo_put_bytes_max(
		    sc->sc_fifo.fp[USB_FIFO_RX]) != 0) {
			xfer->frlengths[0] = xfer->max_data_length;
			usb2_start_hardware(xfer);
		}
		break;

	default:			/* Error */
		if (xfer->error != USB_ERR_CANCELLED) {
			/* try clear stall first */
			xfer->flags.stall_pipe = 1;
			goto tr_setup;
		}
		break;
	}
}

static const struct usb2_config ums_config[UMS_N_TRANSFER] = {

	[UMS_INTR_DT] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = {.pipe_bof = 1,.short_xfer_ok = 1,},
		.bufsize = 0,	/* use wMaxPacketSize */
		.callback = &ums_intr_callback,
	},
};

static int
ums_probe(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct usb2_interface_descriptor *id;
	void *d_ptr;
	int error;
	uint16_t d_len;

	DPRINTFN(11, "\n");

	if (uaa->usb2_mode != USB_MODE_HOST)
		return (ENXIO);

	id = usb2_get_interface_descriptor(uaa->iface);

	if ((id == NULL) ||
	    (id->bInterfaceClass != UICLASS_HID))
		return (ENXIO);

	error = usb2_req_get_hid_desc(uaa->device, NULL,
	    &d_ptr, &d_len, M_TEMP, uaa->info.bIfaceIndex);

	if (error)
		return (ENXIO);

	if (hid_is_collection(d_ptr, d_len,
	    HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_MOUSE)))
		error = 0;
	else if ((id->bInterfaceSubClass == UISUBCLASS_BOOT) &&
	    (id->bInterfaceProtocol == UIPROTO_MOUSE))
		error = 0;
	else
		error = ENXIO;

	free(d_ptr, M_TEMP);
	return (error);
}

static void
ums_hid_parse(struct ums_softc *sc, device_t dev, const uint8_t *buf,
    uint16_t len, uint8_t index)
{
	struct ums_info *info = &sc->sc_info[index];
	uint32_t flags;
	uint8_t i;

	if (hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_X),
	    hid_input, index, &info->sc_loc_x, &flags, &info->sc_iid_x)) {

		if ((flags & MOUSE_FLAGS_MASK) == MOUSE_FLAGS) {
			info->sc_flags |= UMS_FLAG_X_AXIS;
		}
	}
	if (hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP, HUG_Y),
	    hid_input, index, &info->sc_loc_y, &flags, &info->sc_iid_y)) {

		if ((flags & MOUSE_FLAGS_MASK) == MOUSE_FLAGS) {
			info->sc_flags |= UMS_FLAG_Y_AXIS;
		}
	}
	/* Try the wheel first as the Z activator since it's tradition. */
	if (hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP,
	    HUG_WHEEL), hid_input, index, &info->sc_loc_z, &flags,
	    &info->sc_iid_z) ||
	    hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP,
	    HUG_TWHEEL), hid_input, index, &info->sc_loc_z, &flags,
	    &info->sc_iid_z)) {
		if ((flags & MOUSE_FLAGS_MASK) == MOUSE_FLAGS) {
			info->sc_flags |= UMS_FLAG_Z_AXIS;
		}
		/*
		 * We might have both a wheel and Z direction, if so put
		 * put the Z on the W coordinate.
		 */
		if (hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP,
		    HUG_Z), hid_input, index, &info->sc_loc_w, &flags,
		    &info->sc_iid_w)) {

			if ((flags & MOUSE_FLAGS_MASK) == MOUSE_FLAGS) {
				info->sc_flags |= UMS_FLAG_W_AXIS;
			}
		}
	} else if (hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP,
	    HUG_Z), hid_input, index, &info->sc_loc_z, &flags, 
	    &info->sc_iid_z)) {

		if ((flags & MOUSE_FLAGS_MASK) == MOUSE_FLAGS) {
			info->sc_flags |= UMS_FLAG_Z_AXIS;
		}
	}
	/*
	 * The Microsoft Wireless Intellimouse 2.0 reports it's wheel
	 * using 0x0048, which is HUG_TWHEEL, and seems to expect you
	 * to know that the byte after the wheel is the tilt axis.
	 * There are no other HID axis descriptors other than X,Y and
	 * TWHEEL
	 */
	if (hid_locate(buf, len, HID_USAGE2(HUP_GENERIC_DESKTOP,
	    HUG_TWHEEL), hid_input, index, &info->sc_loc_t, 
	    &flags, &info->sc_iid_t)) {

		info->sc_loc_t.pos += 8;

		if ((flags & MOUSE_FLAGS_MASK) == MOUSE_FLAGS) {
			info->sc_flags |= UMS_FLAG_T_AXIS;
		}
	}
	/* figure out the number of buttons */

	for (i = 0; i < UMS_BUTTON_MAX; i++) {
		if (!hid_locate(buf, len, HID_USAGE2(HUP_BUTTON, (i + 1)),
		    hid_input, index, &info->sc_loc_btn[i], NULL, 
		    &info->sc_iid_btn[i])) {
			break;
		}
	}
	info->sc_buttons = i;

	if (i > sc->sc_buttons)
		sc->sc_buttons = i;

	if (info->sc_flags == 0)
		return;

	/* announce information about the mouse */
	device_printf(dev, "%d buttons and [%s%s%s%s%s] coordinates ID=%u\n",
	    (info->sc_buttons),
	    (info->sc_flags & UMS_FLAG_X_AXIS) ? "X" : "",
	    (info->sc_flags & UMS_FLAG_Y_AXIS) ? "Y" : "",
	    (info->sc_flags & UMS_FLAG_Z_AXIS) ? "Z" : "",
	    (info->sc_flags & UMS_FLAG_T_AXIS) ? "T" : "",
	    (info->sc_flags & UMS_FLAG_W_AXIS) ? "W" : "",
	    info->sc_iid_x);
}

static int
ums_attach(device_t dev)
{
	struct usb2_attach_arg *uaa = device_get_ivars(dev);
	struct ums_softc *sc = device_get_softc(dev);
	struct ums_info *info;
	void *d_ptr = NULL;
	int isize;
	int err;
	uint16_t d_len;
	uint8_t i;
	uint8_t j;

	DPRINTFN(11, "sc=%p\n", sc);

	device_set_usb2_desc(dev);

	mtx_init(&sc->sc_mtx, "ums lock", NULL, MTX_DEF | MTX_RECURSE);

	usb2_callout_init_mtx(&sc->sc_callout, &sc->sc_mtx, 0);

	/*
         * Force the report (non-boot) protocol.
         *
         * Mice without boot protocol support may choose not to implement
         * Set_Protocol at all; Ignore any error.
         */
	err = usb2_req_set_protocol(uaa->device, NULL,
	    uaa->info.bIfaceIndex, 1);

	err = usb2_transfer_setup(uaa->device,
	    &uaa->info.bIfaceIndex, sc->sc_xfer, ums_config,
	    UMS_N_TRANSFER, sc, &sc->sc_mtx);

	if (err) {
		DPRINTF("error=%s\n", usb2_errstr(err));
		goto detach;
	}
	err = usb2_req_get_hid_desc(uaa->device, NULL, &d_ptr,
	    &d_len, M_TEMP, uaa->info.bIfaceIndex);

	if (err) {
		device_printf(dev, "error reading report description\n");
		goto detach;
	}

	isize = hid_report_size(d_ptr, d_len, hid_input, &sc->sc_iid);

	/*
	 * The Microsoft Wireless Notebook Optical Mouse seems to be in worse
	 * shape than the Wireless Intellimouse 2.0, as its X, Y, wheel, and
	 * all of its other button positions are all off. It also reports that
	 * it has two addional buttons and a tilt wheel.
	 */
	if (usb2_test_quirk(uaa, UQ_MS_BAD_CLASS)) {
		info = &sc->sc_info[0];
		info->sc_flags = (UMS_FLAG_X_AXIS |
		    UMS_FLAG_Y_AXIS |
		    UMS_FLAG_Z_AXIS |
		    UMS_FLAG_SBU);
		info->sc_buttons = 3;
		isize = 5;
		/* 1st byte of descriptor report contains garbage */
		info->sc_loc_x.pos = 16;
		info->sc_loc_y.pos = 24;
		info->sc_loc_z.pos = 32;
		info->sc_loc_btn[0].pos = 8;
		info->sc_loc_btn[1].pos = 9;
		info->sc_loc_btn[2].pos = 10;

		/* Announce device */
		device_printf(dev, "3 buttons and [XYZ] "
		    "coordinates ID=0\n");

	} else {
		/* Search the HID descriptor and announce device */
		for (i = 0; i < UMS_INFO_MAX; i++) {
			ums_hid_parse(sc, dev, d_ptr, d_len, i);
		}
	}

	if (usb2_test_quirk(uaa, UQ_MS_REVZ)) {
		info = &sc->sc_info[0];
		/* Some wheels need the Z axis reversed. */
		info->sc_flags |= UMS_FLAG_REVZ;
	}
	if (isize > sc->sc_xfer[UMS_INTR_DT]->max_frame_size) {
		DPRINTF("WARNING: report size, %d bytes, is larger "
		    "than interrupt size, %d bytes!\n",
		    isize, sc->sc_xfer[UMS_INTR_DT]->max_frame_size);
	}
	free(d_ptr, M_TEMP);
	d_ptr = NULL;

#if USB_DEBUG
	for (j = 0; j < UMS_INFO_MAX; j++) {
		info = &sc->sc_info[j];

		DPRINTF("sc=%p, index=%d\n", sc, j);
		DPRINTF("X\t%d/%d id=%d\n", info->sc_loc_x.pos,
		    info->sc_loc_x.size, info->sc_iid_x);
		DPRINTF("Y\t%d/%d id=%d\n", info->sc_loc_y.pos,
		    info->sc_loc_y.size, info->sc_iid_y);
		DPRINTF("Z\t%d/%d id=%d\n", info->sc_loc_z.pos,
		    info->sc_loc_z.size, info->sc_iid_z);
		DPRINTF("T\t%d/%d id=%d\n", info->sc_loc_t.pos,
		    info->sc_loc_t.size, info->sc_iid_t);
		DPRINTF("W\t%d/%d id=%d\n", info->sc_loc_w.pos,
		    info->sc_loc_w.size, info->sc_iid_w);

		for (i = 0; i < info->sc_buttons; i++) {
			DPRINTF("B%d\t%d/%d id=%d\n",
			    i + 1, info->sc_loc_btn[i].pos,
			    info->sc_loc_btn[i].size, info->sc_iid_btn[i]);
		}
	}
	DPRINTF("size=%d, id=%d\n", isize, sc->sc_iid);
#endif

	if (sc->sc_buttons > MOUSE_MSC_MAXBUTTON)
		sc->sc_hw.buttons = MOUSE_MSC_MAXBUTTON;
	else
		sc->sc_hw.buttons = sc->sc_buttons;

	sc->sc_hw.iftype = MOUSE_IF_USB;
	sc->sc_hw.type = MOUSE_MOUSE;
	sc->sc_hw.model = MOUSE_MODEL_GENERIC;
	sc->sc_hw.hwid = 0;

	sc->sc_mode.protocol = MOUSE_PROTO_MSC;
	sc->sc_mode.rate = -1;
	sc->sc_mode.resolution = MOUSE_RES_UNKNOWN;
	sc->sc_mode.accelfactor = 0;
	sc->sc_mode.level = 0;
	sc->sc_mode.packetsize = MOUSE_MSC_PACKETSIZE;
	sc->sc_mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
	sc->sc_mode.syncmask[1] = MOUSE_MSC_SYNC;

	err = usb2_fifo_attach(uaa->device, sc, &sc->sc_mtx,
	    &ums_fifo_methods, &sc->sc_fifo,
	    device_get_unit(dev), 0 - 1, uaa->info.bIfaceIndex,
  	    UID_ROOT, GID_OPERATOR, 0644);
	if (err) {
		goto detach;
	}
	return (0);

detach:
	if (d_ptr) {
		free(d_ptr, M_TEMP);
	}
	ums_detach(dev);
	return (ENOMEM);
}

static int
ums_detach(device_t self)
{
	struct ums_softc *sc = device_get_softc(self);

	DPRINTF("sc=%p\n", sc);

	usb2_fifo_detach(&sc->sc_fifo);

	usb2_transfer_unsetup(sc->sc_xfer, UMS_N_TRANSFER);

	usb2_callout_drain(&sc->sc_callout);

	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static void
ums_start_read(struct usb2_fifo *fifo)
{
	struct ums_softc *sc = fifo->priv_sc0;

	usb2_transfer_start(sc->sc_xfer[UMS_INTR_DT]);
}

static void
ums_stop_read(struct usb2_fifo *fifo)
{
	struct ums_softc *sc = fifo->priv_sc0;

	usb2_transfer_stop(sc->sc_xfer[UMS_INTR_DT]);
	usb2_callout_stop(&sc->sc_callout);
}


#if ((MOUSE_SYS_PACKETSIZE != 8) || \
     (MOUSE_MSC_PACKETSIZE != 5))
#error "Software assumptions are not met. Please update code."
#endif

static void
ums_put_queue(struct ums_softc *sc, int32_t dx, int32_t dy,
    int32_t dz, int32_t dt, int32_t buttons)
{
	uint8_t buf[8];

	if (1) {

		if (dx > 254)
			dx = 254;
		if (dx < -256)
			dx = -256;
		if (dy > 254)
			dy = 254;
		if (dy < -256)
			dy = -256;
		if (dz > 126)
			dz = 126;
		if (dz < -128)
			dz = -128;
		if (dt > 126)
			dt = 126;
		if (dt < -128)
			dt = -128;

		buf[0] = sc->sc_mode.syncmask[1];
		buf[0] |= (~buttons) & MOUSE_MSC_BUTTONS;
		buf[1] = dx >> 1;
		buf[2] = dy >> 1;
		buf[3] = dx - (dx >> 1);
		buf[4] = dy - (dy >> 1);

		if (sc->sc_mode.level == 1) {
			buf[5] = dz >> 1;
			buf[6] = dz - (dz >> 1);
			buf[7] = (((~buttons) >> 3) & MOUSE_SYS_EXTBUTTONS);
		}
		usb2_fifo_put_data_linear(sc->sc_fifo.fp[USB_FIFO_RX], buf,
		    sc->sc_mode.packetsize, 1);

	} else {
		DPRINTF("Buffer full, discarded packet\n");
	}
}

static void
ums_reset_buf(struct ums_softc *sc)
{
	/* reset read queue */
	usb2_fifo_reset(sc->sc_fifo.fp[USB_FIFO_RX]);
}

static int
ums_open(struct usb2_fifo *fifo, int fflags)
{
	struct ums_softc *sc = fifo->priv_sc0;

	DPRINTFN(2, "\n");

	if (fflags & FREAD) {

		/* reset status */

		sc->sc_status.flags = 0;
		sc->sc_status.button = 0;
		sc->sc_status.obutton = 0;
		sc->sc_status.dx = 0;
		sc->sc_status.dy = 0;
		sc->sc_status.dz = 0;
		/* sc->sc_status.dt = 0; */

		if (usb2_fifo_alloc_buffer(fifo,
		    UMS_BUF_SIZE, UMS_IFQ_MAXLEN)) {
			return (ENOMEM);
		}
	}
	return (0);
}

static void
ums_close(struct usb2_fifo *fifo, int fflags)
{
	if (fflags & FREAD) {
		usb2_fifo_free_buffer(fifo);
	}
}

static int
ums_ioctl(struct usb2_fifo *fifo, u_long cmd, void *addr, int fflags)
{
	struct ums_softc *sc = fifo->priv_sc0;
	mousemode_t mode;
	int error = 0;

	DPRINTFN(2, "\n");

	mtx_lock(&sc->sc_mtx);

	switch (cmd) {
	case MOUSE_GETHWINFO:
		*(mousehw_t *)addr = sc->sc_hw;
		break;

	case MOUSE_GETMODE:
		*(mousemode_t *)addr = sc->sc_mode;
		break;

	case MOUSE_SETMODE:
		mode = *(mousemode_t *)addr;

		if (mode.level == -1) {
			/* don't change the current setting */
		} else if ((mode.level < 0) || (mode.level > 1)) {
			error = EINVAL;
			goto done;
		} else {
			sc->sc_mode.level = mode.level;
		}

		if (sc->sc_mode.level == 0) {
			if (sc->sc_buttons > MOUSE_MSC_MAXBUTTON)
				sc->sc_hw.buttons = MOUSE_MSC_MAXBUTTON;
			else
				sc->sc_hw.buttons = sc->sc_buttons;
			sc->sc_mode.protocol = MOUSE_PROTO_MSC;
			sc->sc_mode.packetsize = MOUSE_MSC_PACKETSIZE;
			sc->sc_mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
			sc->sc_mode.syncmask[1] = MOUSE_MSC_SYNC;
		} else if (sc->sc_mode.level == 1) {
			if (sc->sc_buttons > MOUSE_SYS_MAXBUTTON)
				sc->sc_hw.buttons = MOUSE_SYS_MAXBUTTON;
			else
				sc->sc_hw.buttons = sc->sc_buttons;
			sc->sc_mode.protocol = MOUSE_PROTO_SYSMOUSE;
			sc->sc_mode.packetsize = MOUSE_SYS_PACKETSIZE;
			sc->sc_mode.syncmask[0] = MOUSE_SYS_SYNCMASK;
			sc->sc_mode.syncmask[1] = MOUSE_SYS_SYNC;
		}
		ums_reset_buf(sc);
		break;

	case MOUSE_GETLEVEL:
		*(int *)addr = sc->sc_mode.level;
		break;

	case MOUSE_SETLEVEL:
		if (*(int *)addr < 0 || *(int *)addr > 1) {
			error = EINVAL;
			goto done;
		}
		sc->sc_mode.level = *(int *)addr;

		if (sc->sc_mode.level == 0) {
			if (sc->sc_buttons > MOUSE_MSC_MAXBUTTON)
				sc->sc_hw.buttons = MOUSE_MSC_MAXBUTTON;
			else
				sc->sc_hw.buttons = sc->sc_buttons;
			sc->sc_mode.protocol = MOUSE_PROTO_MSC;
			sc->sc_mode.packetsize = MOUSE_MSC_PACKETSIZE;
			sc->sc_mode.syncmask[0] = MOUSE_MSC_SYNCMASK;
			sc->sc_mode.syncmask[1] = MOUSE_MSC_SYNC;
		} else if (sc->sc_mode.level == 1) {
			if (sc->sc_buttons > MOUSE_SYS_MAXBUTTON)
				sc->sc_hw.buttons = MOUSE_SYS_MAXBUTTON;
			else
				sc->sc_hw.buttons = sc->sc_buttons;
			sc->sc_mode.protocol = MOUSE_PROTO_SYSMOUSE;
			sc->sc_mode.packetsize = MOUSE_SYS_PACKETSIZE;
			sc->sc_mode.syncmask[0] = MOUSE_SYS_SYNCMASK;
			sc->sc_mode.syncmask[1] = MOUSE_SYS_SYNC;
		}
		ums_reset_buf(sc);
		break;

	case MOUSE_GETSTATUS:{
			mousestatus_t *status = (mousestatus_t *)addr;

			*status = sc->sc_status;
			sc->sc_status.obutton = sc->sc_status.button;
			sc->sc_status.button = 0;
			sc->sc_status.dx = 0;
			sc->sc_status.dy = 0;
			sc->sc_status.dz = 0;
			/* sc->sc_status.dt = 0; */

			if (status->dx || status->dy || status->dz /* || status->dt */ ) {
				status->flags |= MOUSE_POSCHANGED;
			}
			if (status->button != status->obutton) {
				status->flags |= MOUSE_BUTTONSCHANGED;
			}
			break;
		}
	default:
		error = ENOTTY;
	}

done:
	mtx_unlock(&sc->sc_mtx);
	return (error);
}

static devclass_t ums_devclass;

static device_method_t ums_methods[] = {
	DEVMETHOD(device_probe, ums_probe),
	DEVMETHOD(device_attach, ums_attach),
	DEVMETHOD(device_detach, ums_detach),
	{0, 0}
};

static driver_t ums_driver = {
	.name = "ums",
	.methods = ums_methods,
	.size = sizeof(struct ums_softc),
};

DRIVER_MODULE(ums, uhub, ums_driver, ums_devclass, NULL, 0);
MODULE_DEPEND(ums, usb, 1, 1, 1);
