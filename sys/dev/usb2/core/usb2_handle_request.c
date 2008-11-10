/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
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

#include <dev/usb2/include/usb2_defs.h>
#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_error.h>
#include <dev/usb2/include/usb2_standard.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_dynamic.h>
#include <dev/usb2/core/usb2_hub.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>

/* enum */

enum {
	ST_DATA,
	ST_POST_STATUS,
};

/* function prototypes */

static uint8_t usb2_handle_get_stall(struct usb2_device *udev, uint8_t ea_val);
static usb2_error_t usb2_handle_remote_wakeup(struct usb2_xfer *xfer, uint8_t is_on);
static usb2_error_t usb2_handle_request(struct usb2_xfer *xfer);
static usb2_error_t usb2_handle_set_config(struct usb2_xfer *xfer, uint8_t conf_no);
static usb2_error_t usb2_handle_set_stall(struct usb2_xfer *xfer, uint8_t ep, uint8_t do_stall);
static usb2_error_t usb2_handle_iface_request(struct usb2_xfer *xfer, void **ppdata, uint16_t *plen, struct usb2_device_request req, uint16_t off, uint8_t state);

/*------------------------------------------------------------------------*
 *	usb2_handle_request_callback
 *
 * This function is the USB callback for generic USB Device control
 * transfers.
 *------------------------------------------------------------------------*/
void
usb2_handle_request_callback(struct usb2_xfer *xfer)
{
	usb2_error_t err;

	/* check the current transfer state */

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
	case USB_ST_TRANSFERRED:

		/* handle the request */
		err = usb2_handle_request(xfer);

		if (err) {

			if (err == USB_ERR_BAD_CONTEXT) {
				/* we need to re-setup the control transfer */
				usb2_needs_explore(xfer->udev->bus, 0);
				break;
			}
			/*
		         * If no control transfer is active,
		         * receive the next SETUP message:
		         */
			goto tr_restart;
		}
		usb2_start_hardware(xfer);
		break;

	default:
		if (xfer->error != USB_ERR_CANCELLED) {
			/* should not happen - try stalling */
			goto tr_restart;
		}
		break;
	}
	return;

tr_restart:
	xfer->frlengths[0] = sizeof(struct usb2_device_request);
	xfer->nframes = 1;
	xfer->flags.manual_status = 1;
	xfer->flags.force_short_xfer = 0;
	xfer->flags.stall_pipe = 1;	/* cancel previous transfer, if any */
	usb2_start_hardware(xfer);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_handle_set_config
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static usb2_error_t
usb2_handle_set_config(struct usb2_xfer *xfer, uint8_t conf_no)
{
	usb2_error_t err = 0;

	/*
	 * We need to protect against other threads doing probe and
	 * attach:
	 */
	USB_XFER_UNLOCK(xfer);
	mtx_lock(&Giant);		/* XXX */
	sx_xlock(xfer->udev->default_sx + 1);

	if (conf_no == USB_UNCONFIG_NO) {
		conf_no = USB_UNCONFIG_INDEX;
	} else {
		/*
		 * The relationship between config number and config index
		 * is very simple in our case:
		 */
		conf_no--;
	}

	if (usb2_set_config_index(xfer->udev, conf_no)) {
		DPRINTF("set config %d failed\n", conf_no);
		err = USB_ERR_STALLED;
		goto done;
	}
	if (usb2_probe_and_attach(xfer->udev, USB_IFACE_INDEX_ANY)) {
		DPRINTF("probe and attach failed\n");
		err = USB_ERR_STALLED;
		goto done;
	}
done:
	mtx_unlock(&Giant);		/* XXX */
	sx_unlock(xfer->udev->default_sx + 1);
	USB_XFER_LOCK(xfer);
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_handle_iface_request
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static usb2_error_t
usb2_handle_iface_request(struct usb2_xfer *xfer,
    void **ppdata, uint16_t *plen,
    struct usb2_device_request req, uint16_t off, uint8_t state)
{
	struct usb2_interface *iface;
	struct usb2_interface *iface_parent;	/* parent interface */
	struct usb2_device *udev = xfer->udev;
	int error;
	uint8_t iface_index;

	if ((req.bmRequestType & 0x1F) == UT_INTERFACE) {
		iface_index = req.wIndex[0];	/* unicast */
	} else {
		iface_index = 0;	/* broadcast */
	}

	/*
	 * We need to protect against other threads doing probe and
	 * attach:
	 */
	USB_XFER_UNLOCK(xfer);
	mtx_lock(&Giant);		/* XXX */
	sx_xlock(udev->default_sx + 1);

	error = ENXIO;

tr_repeat:
	iface = usb2_get_iface(udev, iface_index);
	if ((iface == NULL) ||
	    (iface->idesc == NULL)) {
		/* end of interfaces non-existing interface */
		goto tr_stalled;
	}
	/* forward request to interface, if any */

	if ((error != 0) &&
	    (error != ENOTTY) &&
	    (iface->subdev != NULL) &&
	    device_is_attached(iface->subdev)) {
#if 0
		DEVMETHOD(usb2_handle_request, NULL);	/* dummy */
#endif
		error = USB2_HANDLE_REQUEST(iface->subdev,
		    &req, ppdata, plen,
		    off, (state == ST_POST_STATUS));
	}
	iface_parent = usb2_get_iface(udev, iface->parent_iface_index);

	if ((iface_parent == NULL) ||
	    (iface_parent->idesc == NULL)) {
		/* non-existing interface */
		iface_parent = NULL;
	}
	/* forward request to parent interface, if any */

	if ((error != 0) &&
	    (error != ENOTTY) &&
	    (iface_parent != NULL) &&
	    (iface_parent->subdev != NULL) &&
	    ((req.bmRequestType & 0x1F) == UT_INTERFACE) &&
	    (iface_parent->subdev != iface->subdev) &&
	    device_is_attached(iface_parent->subdev)) {
		error = USB2_HANDLE_REQUEST(iface_parent->subdev,
		    &req, ppdata, plen, off,
		    (state == ST_POST_STATUS));
	}
	if (error == 0) {
		/* negativly adjust pointer and length */
		*ppdata = ((uint8_t *)(*ppdata)) - off;
		*plen += off;
		goto tr_valid;
	} else if (error == ENOTTY) {
		goto tr_stalled;
	}
	if ((req.bmRequestType & 0x1F) != UT_INTERFACE) {
		iface_index++;		/* iterate */
		goto tr_repeat;
	}
	if (state == ST_POST_STATUS) {
		/* we are complete */
		goto tr_valid;
	}
	switch (req.bmRequestType) {
	case UT_WRITE_INTERFACE:
		switch (req.bRequest) {
		case UR_SET_INTERFACE:
			/*
			 * Handle special case. If we have parent interface
			 * we just reset the endpoints, because this is a
			 * multi interface device and re-attaching only a
			 * part of the device is not possible. Also if the
			 * alternate setting is the same like before we just
			 * reset the interface endoints.
			 */
			if ((iface_parent != NULL) ||
			    (iface->alt_index == req.wValue[0])) {
				error = usb2_reset_iface_endpoints(udev,
				    iface_index);
				if (error) {
					DPRINTF("alt setting failed %s\n",
					    usb2_errstr(error));
					goto tr_stalled;
				}
				break;
			}
			error = usb2_set_alt_interface_index(udev,
			    iface_index, req.wValue[0]);
			if (error) {
				DPRINTF("alt setting failed %s\n",
				    usb2_errstr(error));
				goto tr_stalled;
			}
			error = usb2_probe_and_attach(udev,
			    iface_index);
			if (error) {
				DPRINTF("alt setting probe failed\n");
				goto tr_stalled;
			}
			break;
		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_INTERFACE:
		switch (req.bRequest) {
		case UR_GET_INTERFACE:
			*ppdata = &iface->alt_index;
			*plen = 1;
			break;

		default:
			goto tr_stalled;
		}
		break;
	default:
		goto tr_stalled;
	}
tr_valid:
	mtx_unlock(&Giant);
	sx_unlock(udev->default_sx + 1);
	USB_XFER_LOCK(xfer);
	return (0);

tr_stalled:
	mtx_unlock(&Giant);
	sx_unlock(udev->default_sx + 1);
	USB_XFER_LOCK(xfer);
	return (USB_ERR_STALLED);
}

/*------------------------------------------------------------------------*
 *	usb2_handle_stall
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static usb2_error_t
usb2_handle_set_stall(struct usb2_xfer *xfer, uint8_t ep, uint8_t do_stall)
{
	usb2_error_t err;

	USB_XFER_UNLOCK(xfer);
	err = usb2_set_endpoint_stall(xfer->udev,
	    usb2_get_pipe_by_addr(xfer->udev, ep), do_stall);
	USB_XFER_LOCK(xfer);
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_handle_get_stall
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static uint8_t
usb2_handle_get_stall(struct usb2_device *udev, uint8_t ea_val)
{
	struct usb2_pipe *pipe;
	uint8_t halted;

	pipe = usb2_get_pipe_by_addr(udev, ea_val);
	if (pipe == NULL) {
		/* nothing to do */
		return (0);
	}
	USB_BUS_LOCK(udev->bus);
	halted = pipe->is_stalled;
	USB_BUS_UNLOCK(udev->bus);

	return (halted);
}

/*------------------------------------------------------------------------*
 *	usb2_handle_remote_wakeup
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
static usb2_error_t
usb2_handle_remote_wakeup(struct usb2_xfer *xfer, uint8_t is_on)
{
	struct usb2_device *udev;
	struct usb2_bus *bus;

	udev = xfer->udev;
	bus = udev->bus;

	USB_BUS_LOCK(bus);

	if (is_on) {
		udev->flags.remote_wakeup = 1;
	} else {
		udev->flags.remote_wakeup = 0;
	}

	(bus->methods->rem_wakeup_set) (xfer->udev, is_on);

	USB_BUS_UNLOCK(bus);

	return (0);			/* success */
}

/*------------------------------------------------------------------------*
 *	usb2_handle_request
 *
 * Internal state sequence:
 *
 * ST_DATA -> ST_POST_STATUS
 *
 * Returns:
 * 0: Ready to start hardware
 * Else: Stall current transfer, if any
 *------------------------------------------------------------------------*/
static usb2_error_t
usb2_handle_request(struct usb2_xfer *xfer)
{
	struct usb2_device_request req;
	struct usb2_device *udev;
	const void *src_zcopy;		/* zero-copy source pointer */
	const void *src_mcopy;		/* non zero-copy source pointer */
	uint16_t off;			/* data offset */
	uint16_t rem;			/* data remainder */
	uint16_t max_len;		/* max fragment length */
	uint16_t wValue;
	uint16_t wIndex;
	uint8_t state;
	usb2_error_t err;
	union {
		uWord	wStatus;
		uint8_t	buf[2];
	}     temp;

	/*
	 * Filter the USB transfer state into
	 * something which we understand:
	 */

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
		state = ST_DATA;

		if (!xfer->flags_int.control_act) {
			/* nothing to do */
			goto tr_stalled;
		}
		break;

	default:			/* USB_ST_TRANSFERRED */
		if (!xfer->flags_int.control_act) {
			state = ST_POST_STATUS;
		} else {
			state = ST_DATA;
		}
		break;
	}

	/* reset frame stuff */

	xfer->frlengths[0] = 0;

	usb2_set_frame_offset(xfer, 0, 0);
	usb2_set_frame_offset(xfer, sizeof(req), 1);

	/* get the current request, if any */

	usb2_copy_out(xfer->frbuffers, 0, &req, sizeof(req));

	if (xfer->flags_int.control_rem == 0xFFFF) {
		/* first time - not initialised */
		rem = UGETW(req.wLength);
		off = 0;
	} else {
		/* not first time - initialised */
		rem = xfer->flags_int.control_rem;
		off = UGETW(req.wLength) - rem;
	}

	/* set some defaults */

	max_len = 0;
	src_zcopy = NULL;
	src_mcopy = NULL;
	udev = xfer->udev;

	/* get some request fields decoded */

	wValue = UGETW(req.wValue);
	wIndex = UGETW(req.wIndex);

	DPRINTF("req 0x%02x 0x%02x 0x%04x 0x%04x "
	    "off=0x%x rem=0x%x, state=%d\n", req.bmRequestType,
	    req.bRequest, wValue, wIndex, off, rem, state);

	/* demultiplex the control request */

	switch (req.bmRequestType) {
	case UT_READ_DEVICE:
		if (state != ST_DATA) {
			break;
		}
		switch (req.bRequest) {
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
		switch (req.bRequest) {
		case UR_SET_ADDRESS:
			goto tr_handle_set_address;
		case UR_SET_CONFIG:
			goto tr_handle_set_config;
		case UR_CLEAR_FEATURE:
			switch (wValue) {
			case UF_DEVICE_REMOTE_WAKEUP:
				goto tr_handle_clear_wakeup;
			default:
				goto tr_stalled;
			}
			break;
		case UR_SET_FEATURE:
			switch (wValue) {
			case UF_DEVICE_REMOTE_WAKEUP:
				goto tr_handle_set_wakeup;
			default:
				goto tr_stalled;
			}
			break;
		default:
			goto tr_stalled;
		}
		break;

	case UT_WRITE_ENDPOINT:
		switch (req.bRequest) {
		case UR_CLEAR_FEATURE:
			switch (wValue) {
			case UF_ENDPOINT_HALT:
				goto tr_handle_clear_halt;
			default:
				goto tr_stalled;
			}
			break;
		case UR_SET_FEATURE:
			switch (wValue) {
			case UF_ENDPOINT_HALT:
				goto tr_handle_set_halt;
			default:
				goto tr_stalled;
			}
			break;
		default:
			goto tr_stalled;
		}
		break;

	case UT_READ_ENDPOINT:
		switch (req.bRequest) {
		case UR_GET_STATUS:
			goto tr_handle_get_ep_status;
		default:
			goto tr_stalled;
		}
		break;
	default:
		/* we use "USB_ADD_BYTES" to de-const the src_zcopy */
		err = usb2_handle_iface_request(xfer,
		    USB_ADD_BYTES(&src_zcopy, 0),
		    &max_len, req, off, state);
		if (err == 0) {
			goto tr_valid;
		}
		/*
		 * Reset zero-copy pointer and max length
		 * variable in case they were unintentionally
		 * set:
		 */
		src_zcopy = NULL;
		max_len = 0;

		/*
		 * Check if we have a vendor specific
		 * descriptor:
		 */
		goto tr_handle_get_descriptor;
	}
	goto tr_valid;

tr_handle_get_descriptor:
	(usb2_temp_get_desc_p) (udev, &req, &src_zcopy, &max_len);
	if (src_zcopy == NULL) {
		goto tr_stalled;
	}
	goto tr_valid;

tr_handle_get_config:
	temp.buf[0] = udev->curr_config_no;
	src_mcopy = temp.buf;
	max_len = 1;
	goto tr_valid;

tr_handle_get_status:

	wValue = 0;

	USB_BUS_LOCK(udev->bus);
	if (udev->flags.remote_wakeup) {
		wValue |= UDS_REMOTE_WAKEUP;
	}
	if (udev->flags.self_powered) {
		wValue |= UDS_SELF_POWERED;
	}
	USB_BUS_UNLOCK(udev->bus);

	USETW(temp.wStatus, wValue);
	src_mcopy = temp.wStatus;
	max_len = sizeof(temp.wStatus);
	goto tr_valid;

tr_handle_set_address:
	if (state == ST_DATA) {
		if (wValue >= 0x80) {
			/* invalid value */
			goto tr_stalled;
		} else if (udev->curr_config_no != 0) {
			/* we are configured ! */
			goto tr_stalled;
		}
	} else if (state == ST_POST_STATUS) {
		udev->address = (wValue & 0x7F);
		goto tr_bad_context;
	}
	goto tr_valid;

tr_handle_set_config:
	if (state == ST_DATA) {
		if (usb2_handle_set_config(xfer, req.wValue[0])) {
			goto tr_stalled;
		}
	}
	goto tr_valid;

tr_handle_clear_halt:
	if (state == ST_DATA) {
		if (usb2_handle_set_stall(xfer, req.wIndex[0], 0)) {
			goto tr_stalled;
		}
	}
	goto tr_valid;

tr_handle_clear_wakeup:
	if (state == ST_DATA) {
		if (usb2_handle_remote_wakeup(xfer, 0)) {
			goto tr_stalled;
		}
	}
	goto tr_valid;

tr_handle_set_halt:
	if (state == ST_DATA) {
		if (usb2_handle_set_stall(xfer, req.wIndex[0], 1)) {
			goto tr_stalled;
		}
	}
	goto tr_valid;

tr_handle_set_wakeup:
	if (state == ST_DATA) {
		if (usb2_handle_remote_wakeup(xfer, 1)) {
			goto tr_stalled;
		}
	}
	goto tr_valid;

tr_handle_get_ep_status:
	if (state == ST_DATA) {
		temp.wStatus[0] =
		    usb2_handle_get_stall(udev, req.wIndex[0]);
		temp.wStatus[1] = 0;
		src_mcopy = temp.wStatus;
		max_len = sizeof(temp.wStatus);
	}
	goto tr_valid;

tr_valid:
	if (state == ST_POST_STATUS) {
		goto tr_stalled;
	}
	/* subtract offset from length */

	max_len -= off;

	/* Compute the real maximum data length */

	if (max_len > xfer->max_data_length) {
		max_len = xfer->max_data_length;
	}
	if (max_len > rem) {
		max_len = rem;
	}
	/*
	 * If the remainder is greater than the maximum data length,
	 * we need to truncate the value for the sake of the
	 * comparison below:
	 */
	if (rem > xfer->max_data_length) {
		rem = xfer->max_data_length;
	}
	if (rem != max_len) {
		/*
	         * If we don't transfer the data we can transfer, then
	         * the transfer is short !
	         */
		xfer->flags.force_short_xfer = 1;
		xfer->nframes = 2;
	} else {
		/*
		 * Default case
		 */
		xfer->flags.force_short_xfer = 0;
		xfer->nframes = max_len ? 2 : 1;
	}
	if (max_len > 0) {
		if (src_mcopy) {
			src_mcopy = USB_ADD_BYTES(src_mcopy, off);
			usb2_copy_in(xfer->frbuffers + 1, 0,
			    src_mcopy, max_len);
		} else {
			usb2_set_frame_data(xfer,
			    USB_ADD_BYTES(src_zcopy, off), 1);
		}
		xfer->frlengths[1] = max_len;
	} else {
		/* the end is reached, send status */
		xfer->flags.manual_status = 0;
		xfer->frlengths[1] = 0;
	}
	DPRINTF("success\n");
	return (0);			/* success */

tr_stalled:
	DPRINTF("%s\n", (state == ST_POST_STATUS) ?
	    "complete" : "stalled");
	return (USB_ERR_STALLED);

tr_bad_context:
	DPRINTF("bad context\n");
	return (USB_ERR_BAD_CONTEXT);
}
