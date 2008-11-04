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

#ifndef _USB2_DEFS_H_
#define	_USB2_DEFS_H_

/* Definition of some USB constants */

#define	USB_BUS_MAX 256			/* units */
#define	USB_DEV_MAX 128			/* units */
#define	USB_IFACE_MAX 32		/* units */
#define	USB_EP_MAX (2*16)		/* hardcoded */
#define	USB_FIFO_MAX (4 * USB_EP_MAX)

#define	USB_MAX_DEVICES USB_DEV_MAX	/* including virtual root HUB and
					 * address zero */
#define	USB_MAX_ENDPOINTS USB_EP_MAX	/* 2 directions on 16 endpoints */

#define	USB_UNCONFIG_INDEX 0xFF		/* internal use only */
#define	USB_IFACE_INDEX_ANY 0xFF	/* internal use only */

#define	USB_START_ADDR 0		/* default USB device BUS address
					 * after USB bus reset */

#define	USB_CONTROL_ENDPOINT 0		/* default control endpoint */

#define	USB_FRAMES_PER_SECOND_FS 1000	/* full speed */
#define	USB_FRAMES_PER_SECOND_HS 8000	/* high speed */

#define	USB_FS_BYTES_PER_HS_UFRAME 188	/* bytes */
#define	USB_HS_MICRO_FRAMES_MAX 8	/* units */

/* sanity checks */

#if (USB_FIFO_MAX < USB_EP_MAX)
#error "Misconfigured limits #1"
#endif
#if (USB_FIFO_MAX & 1)
#error "Misconfigured limits #2"
#endif
#if (USB_EP_MAX < (2*16))
#error "Misconfigured limits #3"
#endif

#endif					/* _USB2_DEFS_H_ */
