/*-
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
 * $FreeBSD$
 */

#ifndef _USB_IF_CDCEREG_H_
#define	_USB_IF_CDCEREG_H_

#define	CDCE_N_TRANSFER	3		/* units */
#define	CDCE_IND_SIZE_MAX 32		/* bytes */
#define	CDCE_512X4_IFQ_MAXLEN MAX((2*CDCE_512X4_FRAMES_MAX), IFQ_MAXLEN)

union cdce_eth_rx {			/* multiframe header */
	struct usb2_cdc_mf_eth_512x4_header hdr;
	uint8_t	data[MCLBYTES];
} __aligned(USB_HOST_ALIGN);

union cdce_eth_tx {			/* multiframe header */
	struct usb2_cdc_mf_eth_512x4_header hdr;
} __aligned(USB_HOST_ALIGN);

struct cdce_mq {			/* mini-queue */
	struct mbuf *ifq_head;
	struct mbuf *ifq_tail;
	uint16_t ifq_len;
};

struct cdce_softc {
	void   *sc_evilhack;		/* XXX this pointer must be first */

	union cdce_eth_tx sc_tx;
	union cdce_eth_rx sc_rx;
	struct ifmedia sc_ifmedia;
	struct mtx sc_mtx;
	struct cdce_mq sc_rx_mq;
	struct cdce_mq sc_tx_mq;

	struct ifnet *sc_ifp;
	struct usb2_xfer *sc_xfer[CDCE_N_TRANSFER];
	struct usb2_device *sc_udev;
	device_t sc_dev;

	uint32_t sc_unit;

	uint16_t sc_flags;
#define	CDCE_FLAG_ZAURUS	0x0001
#define	CDCE_FLAG_NO_UNION	0x0002
#define	CDCE_FLAG_LL_READY	0x0004
#define	CDCE_FLAG_HL_READY	0x0008
#define	CDCE_FLAG_RX_DATA	0x0010

	uint8_t	sc_name[16];
	uint8_t	sc_data_iface_no;
	uint8_t	sc_ifaces_index[2];
	uint8_t	sc_iface_protocol;
};

#endif					/* _USB_IF_CDCEREG_H_ */
