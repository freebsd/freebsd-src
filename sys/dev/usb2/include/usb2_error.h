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

/*
 * The "USB_STATUS" macro defines all the USB error codes.
 * NOTE: "USB_ERR_NORMAL_COMPLETION" is not an error code.
 * NOTE: "USB_ERR_STARTING" is not an error code.
 */
#define	USB_ERR(m,n)\
m(n, USB_ERR_NORMAL_COMPLETION)\
m(n, USB_ERR_PENDING_REQUESTS)\
m(n, USB_ERR_NOT_STARTED)\
m(n, USB_ERR_INVAL)\
m(n, USB_ERR_NOMEM)\
m(n, USB_ERR_CANCELLED)\
m(n, USB_ERR_BAD_ADDRESS)\
m(n, USB_ERR_BAD_BUFSIZE)\
m(n, USB_ERR_BAD_FLAG)\
m(n, USB_ERR_NO_CALLBACK)\
m(n, USB_ERR_IN_USE)\
m(n, USB_ERR_NO_ADDR)\
m(n, USB_ERR_NO_PIPE)\
m(n, USB_ERR_ZERO_NFRAMES)\
m(n, USB_ERR_ZERO_MAXP)\
m(n, USB_ERR_SET_ADDR_FAILED)\
m(n, USB_ERR_NO_POWER)\
m(n, USB_ERR_TOO_DEEP)\
m(n, USB_ERR_IOERROR)\
m(n, USB_ERR_NOT_CONFIGURED)\
m(n, USB_ERR_TIMEOUT)\
m(n, USB_ERR_SHORT_XFER)\
m(n, USB_ERR_STALLED)\
m(n, USB_ERR_INTERRUPTED)\
m(n, USB_ERR_DMA_LOAD_FAILED)\
m(n, USB_ERR_BAD_CONTEXT)\
m(n, USB_ERR_NO_ROOT_HUB)\
m(n, USB_ERR_NO_INTR_THREAD)\
m(n, USB_ERR_NOT_LOCKED)\

USB_MAKE_ENUM(USB_ERR);

#endif					/* _USB2_ERROR_H_ */
