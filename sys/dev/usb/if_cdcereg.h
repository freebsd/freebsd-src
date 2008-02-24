/*
 * Copyright (c) 2003-2005 Craig Boston
 * All rights reserved.
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
 *      This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul, THE VOICES IN HIS HEAD OR
 * THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/usb/if_cdcereg.h,v 1.8 2007/06/10 07:33:48 imp Exp $
 */

#ifndef _USB_IF_CDCEREG_H_
#define _USB_IF_CDCEREG_H_

struct cdce_type {
	struct usb_devno	 cdce_dev;
	u_int16_t		 cdce_flags;
#define CDCE_ZAURUS	1
#define CDCE_NO_UNION	2
};

struct cdce_softc {
	struct ifnet		 *cdce_ifp;
#define GET_IFP(sc) ((sc)->cdce_ifp)
	struct ifmedia		 cdce_ifmedia;

	usbd_device_handle	 cdce_udev;
	usbd_interface_handle	 cdce_data_iface;
	int			 cdce_bulkin_no;
	usbd_pipe_handle	 cdce_bulkin_pipe;
	int			 cdce_bulkout_no;
	usbd_pipe_handle	 cdce_bulkout_pipe;
	char			 cdce_dying;
	device_t		 cdce_dev;

	struct ue_cdata		 cdce_cdata;
	struct timeval		 cdce_rx_notice;
	int			 cdce_rxeof_errors;

	u_int16_t		 cdce_flags;

	struct mtx		 cdce_mtx;

	struct usb_qdat		 q;
};

/* We are still under Giant */
#if 0
#define CDCE_LOCK(_sc)           mtx_lock(&(_sc)->cdce_mtx)
#define CDCE_UNLOCK(_sc)         mtx_unlock(&(_sc)->cdce_mtx)
#else
#define CDCE_LOCK(_sc)
#define CDCE_UNLOCK(_sc)
#endif

#endif /* _USB_IF_CDCEREG_H_ */
