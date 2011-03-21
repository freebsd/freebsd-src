/*-
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _DEV_USB_PF_H
#define	_DEV_USB_PF_H

struct usbpf_pkthdr {
	uint32_t	up_busunit;	/* Host controller unit number */
	uint8_t		up_address;	/* USB device address */
	uint8_t		up_endpoint;	/* USB endpoint */
	uint8_t		up_type;	/* points SUBMIT / DONE */
	uint8_t		up_xfertype;	/* Transfer type */
	uint32_t	up_flags;	/* Transfer flags */
#define	USBPF_FLAG_FORCE_SHORT_XFER	(1 << 0)
#define	USBPF_FLAG_SHORT_XFER_OK	(1 << 1)
#define	USBPF_FLAG_SHORT_FRAMES_OK	(1 << 2)
#define	USBPF_FLAG_PIPE_BOF		(1 << 3)
#define	USBPF_FLAG_PROXY_BUFFER		(1 << 4)
#define	USBPF_FLAG_EXT_BUFFER		(1 << 5)
#define	USBPF_FLAG_MANUAL_STATUS	(1 << 6)
#define	USBPF_FLAG_NO_PIPE_OK		(1 << 7)
#define	USBPF_FLAG_STALL_PIPE		(1 << 8)
	uint32_t	up_status;	/* Transfer status */
#define	USBPF_STATUS_OPEN		(1 << 0)
#define	USBPF_STATUS_TRANSFERRING	(1 << 1)
#define	USBPF_STATUS_DID_DMA_DELAY	(1 << 2)
#define	USBPF_STATUS_DID_CLOSE		(1 << 3)
#define	USBPF_STATUS_DRAINING		(1 << 4)
#define	USBPF_STATUS_STARTED		(1 << 5)
#define	USBPF_STATUS_BW_RECLAIMED	(1 << 6)
#define	USBPF_STATUS_CONTROL_XFR	(1 << 7)
#define	USBPF_STATUS_CONTROL_HDR	(1 << 8)
#define	USBPF_STATUS_CONTROL_ACT	(1 << 9)
#define	USBPF_STATUS_CONTROL_STALL	(1 << 10)
#define	USBPF_STATUS_SHORT_FRAMES_OK	(1 << 11)
#define	USBPF_STATUS_SHORT_XFER_OK	(1 << 12)
#if USB_HAVE_BUSDMA
#define	USBPF_STATUS_BDMA_ENABLE	(1 << 13)
#define	USBPF_STATUS_BDMA_NO_POST_SYNC	(1 << 14)
#define	USBPF_STATUS_BDMA_SETUP		(1 << 15)
#endif
#define	USBPF_STATUS_ISOCHRONOUS_XFR	(1 << 16)
#define	USBPF_STATUS_CURR_DMA_SET	(1 << 17)
#define	USBPF_STATUS_CAN_CANCEL_IMMED	(1 << 18)
#define	USBPF_STATUS_DOING_CALLBACK	(1 << 19)
	uint32_t	up_length;	/* Total data length (submit/actual) */
	uint32_t	up_frames;	/* USB frame number (submit/actual) */
	uint32_t	up_error;	/* usb_error_t */
	uint32_t	up_interval;	/* for interrupt and isoc */
	/* sizeof(struct usbpf_pkthdr) == 128 bytes */
	uint8_t		up_reserved[96];
};

#define	USBPF_HDR_LEN		128

#define	USBPF_XFERTAP_SUBMIT	0
#define	USBPF_XFERTAP_DONE	1

#ifdef _KERNEL
void	usbpf_attach(struct usb_bus *);
void	usbpf_detach(struct usb_bus *);
void	usbpf_xfertap(struct usb_xfer *, int);
#endif

#endif
