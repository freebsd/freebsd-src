/* $FreeBSD$ */
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc. All rights reserved.
 * Copyright (c) 1998 Lennart Augustsson. All rights reserved.
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
#include <dev/usb2/include/usb2_ioctl.h>
#include <dev/usb2/include/usb2_hid.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_request.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_device.h>
#include <dev/usb2/core/usb2_util.h>
#include <dev/usb2/core/usb2_dynamic.h>

#include <dev/usb2/controller/usb2_controller.h>
#include <dev/usb2/controller/usb2_bus.h>
#include <sys/ctype.h>

#if USB_DEBUG
static int usb2_pr_poll_delay = USB_PORT_RESET_DELAY;
static int usb2_pr_recovery_delay = USB_PORT_RESET_RECOVERY;
static int usb2_ss_delay = 0;

SYSCTL_INT(_hw_usb2, OID_AUTO, pr_poll_delay, CTLFLAG_RW,
    &usb2_pr_poll_delay, 0, "USB port reset poll delay in ms");
SYSCTL_INT(_hw_usb2, OID_AUTO, pr_recovery_delay, CTLFLAG_RW,
    &usb2_pr_recovery_delay, 0, "USB port reset recovery delay in ms");
SYSCTL_INT(_hw_usb2, OID_AUTO, ss_delay, CTLFLAG_RW,
    &usb2_ss_delay, 0, "USB status stage delay in ms");
#endif

/*------------------------------------------------------------------------*
 *	usb2_do_request_callback
 *
 * This function is the USB callback for generic USB Host control
 * transfers.
 *------------------------------------------------------------------------*/
void
usb2_do_request_callback(struct usb2_xfer *xfer)
{
	;				/* workaround for a bug in "indent" */

	DPRINTF("st=%u\n", USB_GET_STATE(xfer));

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_SETUP:
		usb2_start_hardware(xfer);
		break;
	default:
		usb2_cv_signal(xfer->udev->default_cv);
		break;
	}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_do_clear_stall_callback
 *
 * This function is the USB callback for generic clear stall requests.
 *------------------------------------------------------------------------*/
void
usb2_do_clear_stall_callback(struct usb2_xfer *xfer)
{
	struct usb2_device_request req;
	struct usb2_pipe *pipe;
	struct usb2_pipe *pipe_end;
	struct usb2_pipe *pipe_first;
	uint8_t to = USB_EP_MAX;

	USB_BUS_LOCK(xfer->udev->bus);

	/* round robin pipe clear stall */

	pipe = xfer->udev->pipe_curr;
	pipe_end = xfer->udev->pipes + USB_EP_MAX;
	pipe_first = xfer->udev->pipes;
	if (pipe == NULL) {
		pipe = pipe_first;
	}
	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (pipe->edesc &&
		    pipe->is_stalled) {
			pipe->toggle_next = 0;
			pipe->is_stalled = 0;
			/* start up the current or next transfer, if any */
			usb2_command_wrapper(&pipe->pipe_q,
			    pipe->pipe_q.curr);
		}
		pipe++;

	case USB_ST_SETUP:
tr_setup:
		if (pipe == pipe_end) {
			pipe = pipe_first;
		}
		if (pipe->edesc &&
		    pipe->is_stalled) {

			/* setup a clear-stall packet */

			req.bmRequestType = UT_WRITE_ENDPOINT;
			req.bRequest = UR_CLEAR_FEATURE;
			USETW(req.wValue, UF_ENDPOINT_HALT);
			req.wIndex[0] = pipe->edesc->bEndpointAddress;
			req.wIndex[1] = 0;
			USETW(req.wLength, 0);

			/* copy in the transfer */

			usb2_copy_in(xfer->frbuffers, 0, &req, sizeof(req));

			/* set length */
			xfer->frlengths[0] = sizeof(req);
			xfer->nframes = 1;
			USB_BUS_UNLOCK(xfer->udev->bus);

			usb2_start_hardware(xfer);

			USB_BUS_LOCK(xfer->udev->bus);
			break;
		}
		pipe++;
		if (--to)
			goto tr_setup;
		break;

	default:
		if (xfer->error == USB_ERR_CANCELLED) {
			break;
		}
		goto tr_setup;
	}

	/* store current pipe */
	xfer->udev->pipe_curr = pipe;
	USB_BUS_UNLOCK(xfer->udev->bus);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_do_request_flags and usb2_do_request
 *
 * Description of arguments passed to these functions:
 *
 * "udev" - this is the "usb2_device" structure pointer on which the
 * request should be performed. It is possible to call this function
 * in both Host Side mode and Device Side mode.
 *
 * "mtx" - if this argument is non-NULL the mutex pointed to by it
 * will get dropped and picked up during the execution of this
 * function, hence this function sometimes needs to sleep. If this
 * argument is NULL it has no effect.
 *
 * "req" - this argument must always be non-NULL and points to an
 * 8-byte structure holding the USB request to be done. The USB
 * request structure has a bit telling the direction of the USB
 * request, if it is a read or a write.
 *
 * "data" - if the "wLength" part of the structure pointed to by "req"
 * is non-zero this argument must point to a valid kernel buffer which
 * can hold at least "wLength" bytes. If "wLength" is zero "data" can
 * be NULL.
 *
 * "flags" - here is a list of valid flags:
 *
 *  o USB_SHORT_XFER_OK: allows the data transfer to be shorter than
 *  specified
 *
 *  o USB_USE_POLLING: forces the transfer to complete from the
 *  current context by polling the interrupt handler. This flag can be
 *  used to perform USB transfers after that the kernel has crashed.
 *
 *  o USB_DELAY_STATUS_STAGE: allows the status stage to be performed
 *  at a later point in time. This is tunable by the "hw.usb.ss_delay"
 *  sysctl. This flag is mostly useful for debugging.
 *
 *  o USB_USER_DATA_PTR: treat the "data" pointer like a userland
 *  pointer.
 *
 * "actlen" - if non-NULL the actual transfer length will be stored in
 * the 16-bit unsigned integer pointed to by "actlen". This
 * information is mostly useful when the "USB_SHORT_XFER_OK" flag is
 * used.
 *
 * "timeout" - gives the timeout for the control transfer in
 * milliseconds. A "timeout" value less than 50 milliseconds is
 * treated like a 50 millisecond timeout. A "timeout" value greater
 * than 30 seconds is treated like a 30 second timeout. This USB stack
 * does not allow control requests without a timeout.
 *
 * NOTE: This function is thread safe. All calls to
 * "usb2_do_request_flags" will be serialised by the use of an
 * internal "sx_lock".
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_do_request_flags(struct usb2_device *udev, struct mtx *mtx,
    struct usb2_device_request *req, void *data, uint32_t flags,
    uint16_t *actlen, uint32_t timeout)
{
	struct usb2_xfer *xfer;
	const void *desc;
	int err = 0;
	uint32_t start_ticks;
	uint32_t delta_ticks;
	uint32_t max_ticks;
	uint16_t length;
	uint16_t temp;

	if (timeout < 50) {
		/* timeout is too small */
		timeout = 50;
	}
	if (timeout > 30000) {
		/* timeout is too big */
		timeout = 30000;
	}
	length = UGETW(req->wLength);

	DPRINTFN(5, "udev=%p bmRequestType=0x%02x bRequest=0x%02x "
	    "wValue=0x%02x%02x wIndex=0x%02x%02x wLength=0x%02x%02x\n",
	    udev, req->bmRequestType, req->bRequest,
	    req->wValue[1], req->wValue[0],
	    req->wIndex[1], req->wIndex[0],
	    req->wLength[1], req->wLength[0]);

	/*
	 * Set "actlen" to a known value in case the caller does not
	 * check the return value:
	 */
	if (actlen) {
		*actlen = 0;
	}
	if (udev->flags.usb2_mode == USB_MODE_DEVICE) {
		DPRINTF("USB device mode\n");
		(usb2_temp_get_desc_p) (udev, req, &desc, &temp);
		if (length > temp) {
			if (!(flags & USB_SHORT_XFER_OK)) {
				return (USB_ERR_SHORT_XFER);
			}
			length = temp;
		}
		if (actlen) {
			*actlen = length;
		}
		if (length > 0) {
			if (flags & USB_USER_DATA_PTR) {
				if (copyout(desc, data, length)) {
					return (USB_ERR_INVAL);
				}
			} else {
				bcopy(desc, data, length);
			}
		}
		return (0);		/* success */
	}
	if (mtx) {
		mtx_unlock(mtx);
		if (mtx != &Giant) {
			mtx_assert(mtx, MA_NOTOWNED);
		}
	}
	/*
	 * Grab the default sx-lock so that serialisation
	 * is achieved when multiple threads are involved:
	 */

	sx_xlock(udev->default_sx);

	/*
	 * Setup a new USB transfer or use the existing one, if any:
	 */
	usb2_default_transfer_setup(udev);

	xfer = udev->default_xfer[0];
	if (xfer == NULL) {
		/* most likely out of memory */
		err = USB_ERR_NOMEM;
		goto done;
	}
	USB_XFER_LOCK(xfer);

	if (flags & USB_DELAY_STATUS_STAGE) {
		xfer->flags.manual_status = 1;
	} else {
		xfer->flags.manual_status = 0;
	}

	xfer->timeout = timeout;

	start_ticks = ticks;

	max_ticks = USB_MS_TO_TICKS(timeout);

	usb2_copy_in(xfer->frbuffers, 0, req, sizeof(*req));

	xfer->frlengths[0] = sizeof(*req);
	xfer->nframes = 2;

	while (1) {
		temp = length;
		if (temp > xfer->max_data_length) {
			temp = xfer->max_data_length;
		}
		xfer->frlengths[1] = temp;

		if (temp > 0) {
			if (!(req->bmRequestType & UT_READ)) {
				if (flags & USB_USER_DATA_PTR) {
					USB_XFER_UNLOCK(xfer);
					err = usb2_copy_in_user(xfer->frbuffers + 1,
					    0, data, temp);
					USB_XFER_LOCK(xfer);
					if (err) {
						err = USB_ERR_INVAL;
						break;
					}
				} else {
					usb2_copy_in(xfer->frbuffers + 1, 0, data, temp);
				}
			}
			xfer->nframes = 2;
		} else {
			if (xfer->frlengths[0] == 0) {
				if (xfer->flags.manual_status) {
#if USB_DEBUG
					int temp;

					temp = usb2_ss_delay;
					if (temp > 5000) {
						temp = 5000;
					}
					if (temp > 0) {
						usb2_pause_mtx(
						    xfer->xfer_mtx, temp);
					}
#endif
					xfer->flags.manual_status = 0;
				} else {
					break;
				}
			}
			xfer->nframes = 1;
		}

		usb2_transfer_start(xfer);

		while (usb2_transfer_pending(xfer)) {
			if ((flags & USB_USE_POLLING) || cold) {
				usb2_do_poll(udev->default_xfer, USB_DEFAULT_XFER_MAX);
			} else {
				usb2_cv_wait(xfer->udev->default_cv,
				    xfer->xfer_mtx);
			}
		}

		err = xfer->error;

		if (err) {
			break;
		}
		/* subtract length of SETUP packet, if any */

		if (xfer->aframes > 0) {
			xfer->actlen -= xfer->frlengths[0];
		} else {
			xfer->actlen = 0;
		}

		/* check for short packet */

		if (temp > xfer->actlen) {
			temp = xfer->actlen;
			if (!(flags & USB_SHORT_XFER_OK)) {
				err = USB_ERR_SHORT_XFER;
			}
			length = temp;
		}
		if (temp > 0) {
			if (req->bmRequestType & UT_READ) {
				if (flags & USB_USER_DATA_PTR) {
					USB_XFER_UNLOCK(xfer);
					err = usb2_copy_out_user(xfer->frbuffers + 1,
					    0, data, temp);
					USB_XFER_LOCK(xfer);
					if (err) {
						err = USB_ERR_INVAL;
						break;
					}
				} else {
					usb2_copy_out(xfer->frbuffers + 1,
					    0, data, temp);
				}
			}
		}
		/*
		 * Clear "frlengths[0]" so that we don't send the setup
		 * packet again:
		 */
		xfer->frlengths[0] = 0;

		/* update length and data pointer */
		length -= temp;
		data = USB_ADD_BYTES(data, temp);

		if (actlen) {
			(*actlen) += temp;
		}
		/* check for timeout */

		delta_ticks = ticks - start_ticks;
		if (delta_ticks > max_ticks) {
			if (!err) {
				err = USB_ERR_TIMEOUT;
			}
		}
		if (err) {
			break;
		}
	}

	if (err) {
		/*
		 * Make sure that the control endpoint is no longer
		 * blocked in case of a non-transfer related error:
		 */
		usb2_transfer_stop(xfer);
	}
	USB_XFER_UNLOCK(xfer);

done:
	sx_xunlock(udev->default_sx);

	if (mtx) {
		mtx_lock(mtx);
	}
	return ((usb2_error_t)err);
}

/*------------------------------------------------------------------------*
 *	usb2_req_reset_port
 *
 * This function will instruct an USB HUB to perform a reset sequence
 * on the specified port number.
 *
 * Returns:
 *    0: Success. The USB device should now be at address zero.
 * Else: Failure. No USB device is present and the USB port should be
 *       disabled.
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_reset_port(struct usb2_device *udev, struct mtx *mtx, uint8_t port)
{
	struct usb2_port_status ps;
	usb2_error_t err;
	uint16_t n;

#if USB_DEBUG
	uint16_t pr_poll_delay;
	uint16_t pr_recovery_delay;

#endif
	err = usb2_req_set_port_feature(udev, mtx, port, UHF_PORT_RESET);
	if (err) {
		goto done;
	}
#if USB_DEBUG
	/* range check input parameters */
	pr_poll_delay = usb2_pr_poll_delay;
	if (pr_poll_delay < 1) {
		pr_poll_delay = 1;
	} else if (pr_poll_delay > 1000) {
		pr_poll_delay = 1000;
	}
	pr_recovery_delay = usb2_pr_recovery_delay;
	if (pr_recovery_delay > 1000) {
		pr_recovery_delay = 1000;
	}
#endif
	n = 0;
	while (1) {
#if USB_DEBUG
		/* wait for the device to recover from reset */
		usb2_pause_mtx(mtx, pr_poll_delay);
		n += pr_poll_delay;
#else
		/* wait for the device to recover from reset */
		usb2_pause_mtx(mtx, USB_PORT_RESET_DELAY);
		n += USB_PORT_RESET_DELAY;
#endif
		err = usb2_req_get_port_status(udev, mtx, &ps, port);
		if (err) {
			goto done;
		}
		/* if the device disappeared, just give up */
		if (!(UGETW(ps.wPortStatus) & UPS_CURRENT_CONNECT_STATUS)) {
			goto done;
		}
		/* check if reset is complete */
		if (UGETW(ps.wPortChange) & UPS_C_PORT_RESET) {
			break;
		}
		/* check for timeout */
		if (n > 1000) {
			n = 0;
			break;
		}
	}

	/* clear port reset first */
	err = usb2_req_clear_port_feature(
	    udev, mtx, port, UHF_C_PORT_RESET);
	if (err) {
		goto done;
	}
	/* check for timeout */
	if (n == 0) {
		err = USB_ERR_TIMEOUT;
		goto done;
	}
#if USB_DEBUG
	/* wait for the device to recover from reset */
	usb2_pause_mtx(mtx, pr_recovery_delay);
#else
	/* wait for the device to recover from reset */
	usb2_pause_mtx(mtx, USB_PORT_RESET_RECOVERY);
#endif

done:
	DPRINTFN(2, "port %d reset returning error=%s\n",
	    port, usb2_errstr(err));
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_desc
 *
 * This function can be used to retrieve USB descriptors. It contains
 * some additional logic like zeroing of missing descriptor bytes and
 * retrying an USB descriptor in case of failure. The "min_len"
 * argument specifies the minimum descriptor length. The "max_len"
 * argument specifies the maximum descriptor length. If the real
 * descriptor length is less than the minimum length the missing
 * byte(s) will be zeroed. The length field, first byte, of the USB
 * descriptor will get overwritten in case it indicates a length that
 * is too big. Also the type field, second byte, of the USB descriptor
 * will get forced to the correct type.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_desc(struct usb2_device *udev, struct mtx *mtx, void *desc,
    uint16_t min_len, uint16_t max_len,
    uint16_t id, uint8_t type, uint8_t index,
    uint8_t retries)
{
	struct usb2_device_request req;
	uint8_t *buf;
	usb2_error_t err;

	DPRINTFN(4, "id=%d, type=%d, index=%d, max_len=%d\n",
	    id, type, index, max_len);

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, type, index);
	USETW(req.wIndex, id);

	while (1) {

		if ((min_len < 2) || (max_len < 2)) {
			err = USB_ERR_INVAL;
			goto done;
		}
		USETW(req.wLength, min_len);

		err = usb2_do_request(udev, mtx, &req, desc);

		if (err) {
			if (!retries) {
				goto done;
			}
			retries--;

			usb2_pause_mtx(mtx, 200);

			continue;
		}
		buf = desc;

		if (min_len == max_len) {

			/* enforce correct type and length */

			if (buf[0] > min_len) {
				buf[0] = min_len;
			}
			buf[1] = type;

			goto done;
		}
		/* range check */

		if (max_len > buf[0]) {
			max_len = buf[0];
		}
		/* zero minimum data */

		while (min_len > max_len) {
			min_len--;
			buf[min_len] = 0;
		}

		/* set new minimum length */

		min_len = max_len;
	}
done:
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_string_any
 *
 * This function will return the string given by "string_index"
 * using the first language ID. The maximum length "len" includes
 * the terminating zero. The "len" argument should be twice as
 * big pluss 2 bytes, compared with the actual maximum string length !
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_string_any(struct usb2_device *udev, struct mtx *mtx, char *buf,
    uint16_t len, uint8_t string_index)
{
	char *s;
	uint8_t *temp;
	uint16_t i;
	uint16_t n;
	uint16_t c;
	uint8_t swap;
	usb2_error_t err;

	if (len == 0) {
		/* should not happen */
		return (USB_ERR_NORMAL_COMPLETION);
	}
	if (string_index == 0) {
		/* this is the language table */
		buf[0] = 0;
		return (USB_ERR_INVAL);
	}
	if (udev->flags.no_strings) {
		buf[0] = 0;
		return (USB_ERR_STALLED);
	}
	err = usb2_req_get_string_desc
	    (udev, mtx, buf, len, udev->langid, string_index);
	if (err) {
		buf[0] = 0;
		return (err);
	}
	temp = (uint8_t *)buf;

	if (temp[0] < 2) {
		/* string length is too short */
		buf[0] = 0;
		return (USB_ERR_INVAL);
	}
	/* reserve one byte for terminating zero */
	len--;

	/* find maximum length */
	s = buf;
	n = (temp[0] / 2) - 1;
	if (n > len) {
		n = len;
	}
	/* skip descriptor header */
	temp += 2;

	/* reset swap state */
	swap = 3;

	/* convert and filter */
	for (i = 0; (i != n); i++) {
		c = UGETW(temp + (2 * i));

		/* convert from Unicode, handle buggy strings */
		if (((c & 0xff00) == 0) && (swap & 1)) {
			/* Little Endian, default */
			*s = c;
			swap = 1;
		} else if (((c & 0x00ff) == 0) && (swap & 2)) {
			/* Big Endian */
			*s = c >> 8;
			swap = 2;
		} else {
			/* silently skip bad character */
			continue;
		}

		/*
		 * Filter by default - we don't allow greater and less than
		 * signs because they might confuse the dmesg printouts!
		 */
		if ((*s == '<') || (*s == '>') || (!isprint(*s))) {
			/* silently skip bad character */
			continue;
		}
		s++;
	}
	*s = 0;				/* zero terminate resulting string */
	return (USB_ERR_NORMAL_COMPLETION);
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_string_desc
 *
 * If you don't know the language ID, consider using
 * "usb2_req_get_string_any()".
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_string_desc(struct usb2_device *udev, struct mtx *mtx, void *sdesc,
    uint16_t max_len, uint16_t lang_id,
    uint8_t string_index)
{
	return (usb2_req_get_desc(udev, mtx, sdesc, 2, max_len, lang_id,
	    UDESC_STRING, string_index, 0));
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_config_desc
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_config_desc(struct usb2_device *udev, struct mtx *mtx,
    struct usb2_config_descriptor *d, uint8_t conf_index)
{
	usb2_error_t err;

	DPRINTFN(4, "confidx=%d\n", conf_index);

	err = usb2_req_get_desc(udev, mtx, d, sizeof(*d),
	    sizeof(*d), 0, UDESC_CONFIG, conf_index, 0);
	if (err) {
		goto done;
	}
	/* Extra sanity checking */
	if (UGETW(d->wTotalLength) < sizeof(*d)) {
		err = USB_ERR_INVAL;
	}
done:
	return (err);
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_config_desc_full
 *
 * This function gets the complete USB configuration descriptor and
 * ensures that "wTotalLength" is correct.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_config_desc_full(struct usb2_device *udev, struct mtx *mtx,
    struct usb2_config_descriptor **ppcd, struct malloc_type *mtype,
    uint8_t index)
{
	struct usb2_config_descriptor cd;
	struct usb2_config_descriptor *cdesc;
	uint16_t len;
	usb2_error_t err;

	DPRINTFN(4, "index=%d\n", index);

	*ppcd = NULL;

	err = usb2_req_get_config_desc(udev, mtx, &cd, index);
	if (err) {
		return (err);
	}
	/* get full descriptor */
	len = UGETW(cd.wTotalLength);
	if (len < sizeof(*cdesc)) {
		/* corrupt descriptor */
		return (USB_ERR_INVAL);
	}
	cdesc = malloc(len, mtype, M_WAITOK);
	if (cdesc == NULL) {
		return (USB_ERR_NOMEM);
	}
	err = usb2_req_get_desc(udev, mtx, cdesc, len, len, 0,
	    UDESC_CONFIG, index, 3);
	if (err) {
		free(cdesc, mtype);
		return (err);
	}
	/* make sure that the device is not fooling us: */
	USETW(cdesc->wTotalLength, len);

	*ppcd = cdesc;

	return (0);			/* success */
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_device_desc
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_device_desc(struct usb2_device *udev, struct mtx *mtx,
    struct usb2_device_descriptor *d)
{
	DPRINTFN(4, "\n");
	return (usb2_req_get_desc(udev, mtx, d, sizeof(*d),
	    sizeof(*d), 0, UDESC_DEVICE, 0, 3));
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_alt_interface_no
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_alt_interface_no(struct usb2_device *udev, struct mtx *mtx,
    uint8_t *alt_iface_no, uint8_t iface_index)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	struct usb2_device_request req;

	if ((iface == NULL) || (iface->idesc == NULL)) {
		return (USB_ERR_INVAL);
	}
	req.bmRequestType = UT_READ_INTERFACE;
	req.bRequest = UR_GET_INTERFACE;
	USETW(req.wValue, 0);
	req.wIndex[0] = iface->idesc->bInterfaceNumber;
	req.wIndex[1] = 0;
	USETW(req.wLength, 1);
	return (usb2_do_request(udev, mtx, &req, alt_iface_no));
}

/*------------------------------------------------------------------------*
 *	usb2_req_set_alt_interface_no
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_set_alt_interface_no(struct usb2_device *udev, struct mtx *mtx,
    uint8_t iface_index, uint8_t alt_no)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	struct usb2_device_request req;

	if ((iface == NULL) || (iface->idesc == NULL)) {
		return (USB_ERR_INVAL);
	}
	req.bmRequestType = UT_WRITE_INTERFACE;
	req.bRequest = UR_SET_INTERFACE;
	req.wValue[0] = alt_no;
	req.wValue[1] = 0;
	req.wIndex[0] = iface->idesc->bInterfaceNumber;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);
	return (usb2_do_request(udev, mtx, &req, 0));
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_device_status
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_device_status(struct usb2_device *udev, struct mtx *mtx,
    struct usb2_status *st)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(*st));
	return (usb2_do_request(udev, mtx, &req, st));
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_hub_descriptor
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_hub_descriptor(struct usb2_device *udev, struct mtx *mtx,
    struct usb2_hub_descriptor *hd, uint8_t nports)
{
	struct usb2_device_request req;
	uint16_t len = (nports + 7 + (8 * 8)) / 8;

	req.bmRequestType = UT_READ_CLASS_DEVICE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_HUB, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, len);
	return (usb2_do_request(udev, mtx, &req, hd));
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_hub_status
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_hub_status(struct usb2_device *udev, struct mtx *mtx,
    struct usb2_hub_status *st)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_READ_CLASS_DEVICE;
	req.bRequest = UR_GET_STATUS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof(struct usb2_hub_status));
	return (usb2_do_request(udev, mtx, &req, st));
}

/*------------------------------------------------------------------------*
 *	usb2_req_set_address
 *
 * This function is used to set the address for an USB device. After
 * port reset the USB device will respond at address zero.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_set_address(struct usb2_device *udev, struct mtx *mtx, uint16_t addr)
{
	struct usb2_device_request req;

	DPRINTFN(6, "setting device address=%d\n", addr);

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_ADDRESS;
	USETW(req.wValue, addr);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);

	/* Setting the address should not take more than 1 second ! */
	return (usb2_do_request_flags(udev, mtx, &req, NULL,
	    USB_DELAY_STATUS_STAGE, NULL, 1000));
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_port_status
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_port_status(struct usb2_device *udev, struct mtx *mtx,
    struct usb2_port_status *ps, uint8_t port)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_READ_CLASS_OTHER;
	req.bRequest = UR_GET_STATUS;
	USETW(req.wValue, 0);
	req.wIndex[0] = port;
	req.wIndex[1] = 0;
	USETW(req.wLength, sizeof *ps);
	return (usb2_do_request(udev, mtx, &req, ps));
}

/*------------------------------------------------------------------------*
 *	usb2_req_clear_hub_feature
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_clear_hub_feature(struct usb2_device *udev, struct mtx *mtx,
    uint16_t sel)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_CLASS_DEVICE;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usb2_do_request(udev, mtx, &req, 0));
}

/*------------------------------------------------------------------------*
 *	usb2_req_set_hub_feature
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_set_hub_feature(struct usb2_device *udev, struct mtx *mtx,
    uint16_t sel)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_CLASS_DEVICE;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, sel);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usb2_do_request(udev, mtx, &req, 0));
}

/*------------------------------------------------------------------------*
 *	usb2_req_clear_port_feature
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_clear_port_feature(struct usb2_device *udev, struct mtx *mtx,
    uint8_t port, uint16_t sel)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_CLEAR_FEATURE;
	USETW(req.wValue, sel);
	req.wIndex[0] = port;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);
	return (usb2_do_request(udev, mtx, &req, 0));
}

/*------------------------------------------------------------------------*
 *	usb2_req_set_port_feature
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_set_port_feature(struct usb2_device *udev, struct mtx *mtx,
    uint8_t port, uint16_t sel)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_WRITE_CLASS_OTHER;
	req.bRequest = UR_SET_FEATURE;
	USETW(req.wValue, sel);
	req.wIndex[0] = port;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);
	return (usb2_do_request(udev, mtx, &req, 0));
}

/*------------------------------------------------------------------------*
 *	usb2_req_set_protocol
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_set_protocol(struct usb2_device *udev, struct mtx *mtx,
    uint8_t iface_index, uint16_t report)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	struct usb2_device_request req;

	if ((iface == NULL) || (iface->idesc == NULL)) {
		return (USB_ERR_INVAL);
	}
	DPRINTFN(5, "iface=%p, report=%d, endpt=%d\n",
	    iface, report, iface->idesc->bInterfaceNumber);

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_PROTOCOL;
	USETW(req.wValue, report);
	req.wIndex[0] = iface->idesc->bInterfaceNumber;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);
	return (usb2_do_request(udev, mtx, &req, 0));
}

/*------------------------------------------------------------------------*
 *	usb2_req_set_report
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_set_report(struct usb2_device *udev, struct mtx *mtx, void *data, uint16_t len,
    uint8_t iface_index, uint8_t type, uint8_t id)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	struct usb2_device_request req;

	if ((iface == NULL) || (iface->idesc == NULL)) {
		return (USB_ERR_INVAL);
	}
	DPRINTFN(5, "len=%d\n", len);

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_REPORT;
	USETW2(req.wValue, type, id);
	req.wIndex[0] = iface->idesc->bInterfaceNumber;
	req.wIndex[1] = 0;
	USETW(req.wLength, len);
	return (usb2_do_request(udev, mtx, &req, data));
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_report
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_report(struct usb2_device *udev, struct mtx *mtx, void *data,
    uint16_t len, uint8_t iface_index, uint8_t type, uint8_t id)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	struct usb2_device_request req;

	if ((iface == NULL) || (iface->idesc == NULL) || (id == 0)) {
		return (USB_ERR_INVAL);
	}
	DPRINTFN(5, "len=%d\n", len);

	req.bmRequestType = UT_READ_CLASS_INTERFACE;
	req.bRequest = UR_GET_REPORT;
	USETW2(req.wValue, type, id);
	req.wIndex[0] = iface->idesc->bInterfaceNumber;
	req.wIndex[1] = 0;
	USETW(req.wLength, len);
	return (usb2_do_request(udev, mtx, &req, data));
}

/*------------------------------------------------------------------------*
 *	usb2_req_set_idle
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_set_idle(struct usb2_device *udev, struct mtx *mtx,
    uint8_t iface_index, uint8_t duration, uint8_t id)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	struct usb2_device_request req;

	if ((iface == NULL) || (iface->idesc == NULL)) {
		return (USB_ERR_INVAL);
	}
	DPRINTFN(5, "%d %d\n", duration, id);

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UR_SET_IDLE;
	USETW2(req.wValue, duration, id);
	req.wIndex[0] = iface->idesc->bInterfaceNumber;
	req.wIndex[1] = 0;
	USETW(req.wLength, 0);
	return (usb2_do_request(udev, mtx, &req, 0));
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_report_descriptor
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_report_descriptor(struct usb2_device *udev, struct mtx *mtx,
    void *d, uint16_t size, uint8_t iface_index)
{
	struct usb2_interface *iface = usb2_get_iface(udev, iface_index);
	struct usb2_device_request req;

	if ((iface == NULL) || (iface->idesc == NULL)) {
		return (USB_ERR_INVAL);
	}
	req.bmRequestType = UT_READ_INTERFACE;
	req.bRequest = UR_GET_DESCRIPTOR;
	USETW2(req.wValue, UDESC_REPORT, 0);	/* report id should be 0 */
	req.wIndex[0] = iface->idesc->bInterfaceNumber;
	req.wIndex[1] = 0;
	USETW(req.wLength, size);
	return (usb2_do_request(udev, mtx, &req, d));
}

/*------------------------------------------------------------------------*
 *	usb2_req_set_config
 *
 * This function is used to select the current configuration number in
 * both USB device side mode and USB host side mode. When setting the
 * configuration the function of the interfaces can change.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_set_config(struct usb2_device *udev, struct mtx *mtx, uint8_t conf)
{
	struct usb2_device_request req;

	DPRINTF("setting config %d\n", conf);

	/* do "set configuration" request */

	req.bmRequestType = UT_WRITE_DEVICE;
	req.bRequest = UR_SET_CONFIG;
	req.wValue[0] = conf;
	req.wValue[1] = 0;
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	return (usb2_do_request(udev, mtx, &req, 0));
}

/*------------------------------------------------------------------------*
 *	usb2_req_get_config
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_get_config(struct usb2_device *udev, struct mtx *mtx, uint8_t *pconf)
{
	struct usb2_device_request req;

	req.bmRequestType = UT_READ_DEVICE;
	req.bRequest = UR_GET_CONFIG;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 1);
	return (usb2_do_request(udev, mtx, &req, pconf));
}

/*------------------------------------------------------------------------*
 *	usb2_req_re_enumerate
 *
 * NOTE: After this function returns the hardware is in the
 * unconfigured state! The application is responsible for setting a
 * new configuration.
 *
 * Returns:
 *    0: Success
 * Else: Failure
 *------------------------------------------------------------------------*/
usb2_error_t
usb2_req_re_enumerate(struct usb2_device *udev, struct mtx *mtx)
{
	struct usb2_device *parent_hub;
	usb2_error_t err;
	uint8_t old_addr;

	if (udev->flags.usb2_mode != USB_MODE_HOST) {
		return (USB_ERR_INVAL);
	}
	old_addr = udev->address;
	parent_hub = udev->parent_hub;
	if (parent_hub == NULL) {
		return (USB_ERR_INVAL);
	}
	err = usb2_req_reset_port(parent_hub, mtx, udev->port_no);
	if (err) {
		DPRINTFN(0, "addr=%d, port reset failed\n", old_addr);
		goto done;
	}
	/*
	 * After that the port has been reset our device should be at
	 * address zero:
	 */
	udev->address = USB_START_ADDR;

	/* reset "bMaxPacketSize" */
	udev->ddesc.bMaxPacketSize = USB_MAX_IPACKET;

	/*
	 * Restore device address:
	 */
	err = usb2_req_set_address(udev, mtx, old_addr);
	if (err) {
		/* XXX ignore any errors! */
		DPRINTFN(0, "addr=%d, set address failed\n",
		    old_addr);
		err = 0;
	}
	/* restore device address */
	udev->address = old_addr;

	/* allow device time to set new address */
	usb2_pause_mtx(mtx, USB_SET_ADDRESS_SETTLE);

	/* get the device descriptor */
	err = usb2_req_get_desc(udev, mtx, &udev->ddesc,
	    USB_MAX_IPACKET, USB_MAX_IPACKET, 0, UDESC_DEVICE, 0, 0);
	if (err) {
		DPRINTFN(0, "getting device descriptor "
		    "at addr %d failed!\n", udev->address);
		goto done;
	}
	/* get the full device descriptor */
	err = usb2_req_get_device_desc(udev, mtx, &udev->ddesc);
	if (err) {
		DPRINTFN(0, "addr=%d, getting device "
		    "descriptor failed!\n", old_addr);
		goto done;
	}
done:
	/* restore address */
	udev->address = old_addr;
	return (err);
}
