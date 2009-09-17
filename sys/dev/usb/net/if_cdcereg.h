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

#define	CDCE_FRAMES_MAX	8		/* units */
#define	CDCE_IND_SIZE_MAX 32            /* bytes */

enum {
	CDCE_BULK_A,
	CDCE_BULK_B,
	CDCE_INTR,
	CDCE_N_TRANSFER,
};

struct cdce_softc {
	struct usb2_ether	sc_ue;
	struct mtx		sc_mtx;
	struct usb2_xfer	*sc_xfer[CDCE_N_TRANSFER];
	struct mbuf		*sc_rx_buf[CDCE_FRAMES_MAX];
	struct mbuf		*sc_tx_buf[CDCE_FRAMES_MAX];

	int 			sc_flags;
#define	CDCE_FLAG_ZAURUS	0x0001
#define	CDCE_FLAG_NO_UNION	0x0002
#define	CDCE_FLAG_RX_DATA	0x0010

	uint8_t sc_eaddr_str_index;
	uint8_t	sc_data_iface_no;
	uint8_t	sc_ifaces_index[2];
};

#define	CDCE_LOCK(_sc)			mtx_lock(&(_sc)->sc_mtx)
#define	CDCE_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	CDCE_LOCK_ASSERT(_sc, t)	mtx_assert(&(_sc)->sc_mtx, t)
#endif					/* _USB_IF_CDCEREG_H_ */
