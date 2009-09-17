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

#ifndef _USB2_ERROR_H_
#define	_USB2_ERROR_H_

enum {	/* keep in sync with usb_errstr_table */
	USB_ERR_NORMAL_COMPLETION = 0,
	USB_ERR_PENDING_REQUESTS,	/* 1 */
	USB_ERR_NOT_STARTED,		/* 2 */
	USB_ERR_INVAL,			/* 3 */
	USB_ERR_NOMEM,			/* 4 */
	USB_ERR_CANCELLED,		/* 5 */
	USB_ERR_BAD_ADDRESS,		/* 6 */
	USB_ERR_BAD_BUFSIZE,		/* 7 */
	USB_ERR_BAD_FLAG,		/* 8 */
	USB_ERR_NO_CALLBACK,		/* 9 */
	USB_ERR_IN_USE,			/* 10 */
	USB_ERR_NO_ADDR,		/* 11 */
	USB_ERR_NO_PIPE,		/* 12 */
	USB_ERR_ZERO_NFRAMES,		/* 13 */
	USB_ERR_ZERO_MAXP,		/* 14 */
	USB_ERR_SET_ADDR_FAILED,	/* 15 */
	USB_ERR_NO_POWER,		/* 16 */
	USB_ERR_TOO_DEEP,		/* 17 */
	USB_ERR_IOERROR,		/* 18 */
	USB_ERR_NOT_CONFIGURED,		/* 19 */
	USB_ERR_TIMEOUT,		/* 20 */
	USB_ERR_SHORT_XFER,		/* 21 */
	USB_ERR_STALLED,		/* 22 */
	USB_ERR_INTERRUPTED,		/* 23 */
	USB_ERR_DMA_LOAD_FAILED,	/* 24 */
	USB_ERR_BAD_CONTEXT,		/* 25 */
	USB_ERR_NO_ROOT_HUB,		/* 26 */
	USB_ERR_NO_INTR_THREAD,		/* 27 */
	USB_ERR_NOT_LOCKED,		/* 28 */
	USB_ERR_MAX
};

#endif					/* _USB2_ERROR_H_ */
