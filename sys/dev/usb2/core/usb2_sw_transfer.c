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

#include <dev/usb2/include/usb2_mfunc.h>
#include <dev/usb2/include/usb2_standard.h>
#include <dev/usb2/include/usb2_error.h>

#define	USB_DEBUG_VAR usb2_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_busdma.h>
#include <dev/usb2/core/usb2_transfer.h>
#include <dev/usb2/core/usb2_sw_transfer.h>
#include <dev/usb2/core/usb2_debug.h>

/*------------------------------------------------------------------------*
 *	usb2_sw_transfer - factored out code
 *
 * This function is basically used for the Virtual Root HUB, and can
 * emulate control, bulk and interrupt endpoints. Data is exchanged
 * using the "std->ptr" and "std->len" fields, that allows kernel
 * virtual memory to be transferred. All state is kept in the
 * structure pointed to by the "std" argument passed to this
 * function. The "func" argument points to a function that is called
 * back in the various states, so that the application using this
 * function can get a chance to select the outcome. The "func"
 * function is allowed to sleep, exiting all mutexes. If this function
 * will sleep the "enter" and "start" methods must be marked
 * non-cancelable, hence there is no extra cancelled checking in this
 * function.
 *------------------------------------------------------------------------*/
void
usb2_sw_transfer(struct usb2_sw_transfer *std,
    usb2_sw_transfer_func_t *func)
{
	struct usb2_xfer *xfer;
	uint32_t len;
	uint8_t shortpkt = 0;

	xfer = std->xfer;
	if (xfer == NULL) {
		/* the transfer is gone */
		DPRINTF("xfer gone\n");
		return;
	}
	mtx_assert(xfer->usb2_mtx, MA_OWNED);

	std->xfer = NULL;

	/* check for control transfer */
	if (xfer->flags_int.control_xfr) {
		/* check if we are transferring the SETUP packet */
		if (xfer->flags_int.control_hdr) {

			/* copy out the USB request */

			if (xfer->frlengths[0] == sizeof(std->req)) {
				usb2_copy_out(xfer->frbuffers, 0,
				    &std->req, sizeof(std->req));
			} else {
				std->err = USB_ERR_INVAL;
				goto done;
			}

			xfer->aframes = 1;

			std->err = 0;
			std->state = USB_SW_TR_SETUP;

			(func) (xfer, std);

			if (std->err) {
				goto done;
			}
		} else {
			/* skip the first frame in this case */
			xfer->aframes = 1;
		}
	}
	std->err = 0;
	std->state = USB_SW_TR_PRE_DATA;

	(func) (xfer, std);

	if (std->err) {
		goto done;
	}
	/* Transfer data. Iterate accross all frames. */
	while (xfer->aframes != xfer->nframes) {

		len = xfer->frlengths[xfer->aframes];

		if (len > std->len) {
			len = std->len;
			shortpkt = 1;
		}
		if (len > 0) {
			if ((xfer->endpoint & (UE_DIR_IN | UE_DIR_OUT)) == UE_DIR_IN) {
				usb2_copy_in(xfer->frbuffers + xfer->aframes, 0,
				    std->ptr, len);
			} else {
				usb2_copy_out(xfer->frbuffers + xfer->aframes, 0,
				    std->ptr, len);
			}
		}
		std->ptr += len;
		std->len -= len;
		xfer->frlengths[xfer->aframes] = len;
		xfer->aframes++;

		if (shortpkt) {
			break;
		}
	}

	std->err = 0;
	std->state = USB_SW_TR_POST_DATA;

	(func) (xfer, std);

	if (std->err) {
		goto done;
	}
	/* check if the control transfer is complete */
	if (xfer->flags_int.control_xfr &&
	    !xfer->flags_int.control_act) {

		std->err = 0;
		std->state = USB_SW_TR_STATUS;

		(func) (xfer, std);

		if (std->err) {
			goto done;
		}
	}
done:
	DPRINTF("done err=%s\n", usb2_errstr(std->err));
	std->state = USB_SW_TR_PRE_CALLBACK;
	(func) (xfer, std);
	return;
}
