/*-
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Callbacks in the USB code operate at splusb() (actually splbio()
 * in FreeBSD). However adding packets to the input queues has to be
 * done at splimp(). It is conceivable that this arrangement could
 * trigger a condition where the splimp() is ignored and the input
 * queues could get trampled in spite of our best effors to prevent
 * it. To work around this, we implement a special input queue for
 * USB ethernet adapter drivers. Rather than passing the frames directly
 * to ether_input(), we pass them here, then schedule a soft interrupt
 * to hand them to ether_input() later, outside of the USB interrupt
 * context.
 *
 * It's questional as to whether this code should be expanded to
 * handle other kinds of devices, or handle USB transfer callbacks
 * in general. Right now, I need USB network interfaces to work
 * properly.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/netisr.h>
#include <net/bpf.h>

#include <dev/usb/usb.h>
#include <dev/usb/usb_ethersubr.h>

Static struct ifqueue usbq_rx;
Static struct ifqueue usbq_tx;
Static int mtx_inited = 0;

Static void usbintr		(void);

Static void usbintr(void)
{
	struct mbuf		*m;
	struct usb_qdat		*q;
	struct ifnet		*ifp;

	/* Check the RX queue */
	while(1) {
		IF_DEQUEUE(&usbq_rx, m);
		if (m == NULL)
			break;
		q = (struct usb_qdat *)m->m_pkthdr.rcvif;
		ifp = q->ifp;
		m->m_pkthdr.rcvif = ifp;
		(*ifp->if_input)(ifp, m);

		/* Re-arm the receiver */
		(*q->if_rxstart)(ifp);
		if (ifp->if_snd.ifq_head != NULL)
			(*ifp->if_start)(ifp);
	}

	/* Check the TX queue */
	while(1) {
		IF_DEQUEUE(&usbq_tx, m);
		if (m == NULL)
			break;
		ifp = m->m_pkthdr.rcvif;
		m_freem(m);
		if (ifp->if_snd.ifq_head != NULL)
			(*ifp->if_start)(ifp);
	}

	return;
}

void usb_register_netisr()
{
	if (mtx_inited)
		return;
	netisr_register(NETISR_USB, (netisr_t *)usbintr, NULL, 0);
	mtx_init(&usbq_tx.ifq_mtx, "usbq_tx_mtx", NULL, MTX_DEF);
	mtx_init(&usbq_rx.ifq_mtx, "usbq_rx_mtx", NULL, MTX_DEF);
	mtx_inited++;
	return;
}

/*
 * Must be called at splusb() (actually splbio()). This should be
 * the case when called from a transfer callback routine.
 */
void usb_ether_input(m)
	struct mbuf		*m;
{
	IF_ENQUEUE(&usbq_rx, m);
	schednetisr(NETISR_USB);

	return;
}

void usb_tx_done(m)
	struct mbuf		*m;
{
	IF_ENQUEUE(&usbq_tx, m);
	schednetisr(NETISR_USB);

	return;
}

struct mbuf *
usb_ether_newbuf(void)
{
	struct mbuf *m_new;

	m_new = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m_new == NULL)
		return (NULL);
	m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;

	m_adj(m_new, ETHER_ALIGN);
	return (m_new);
}

int
usb_ether_rx_list_init(void *sc, struct ue_cdata *cd,
    usbd_device_handle ue_udev)
{
	struct ue_chain *c;
	int i;

	for (i = 0; i < UE_RX_LIST_CNT; i++) {
		c = &cd->ue_rx_chain[i];
		c->ue_sc = sc;
		c->ue_idx = i;
		c->ue_mbuf = usb_ether_newbuf();
		if (c->ue_mbuf == NULL)
			return (ENOBUFS);
		if (c->ue_xfer == NULL) {
			c->ue_xfer = usbd_alloc_xfer(ue_udev);
			if (c->ue_xfer == NULL)
				return (ENOBUFS);
			c->ue_buf = usbd_alloc_buffer(c->ue_xfer, UE_BUFSZ);
			if (c->ue_buf == NULL)
				return (ENOBUFS);
		}
	}

	return (0);
}

int
usb_ether_tx_list_init(void *sc, struct ue_cdata *cd,
    usbd_device_handle ue_udev)
{
	struct ue_chain *c;
	int i;

	for (i = 0; i < UE_TX_LIST_CNT; i++) {
		c = &cd->ue_tx_chain[i];
		c->ue_sc = sc;
		c->ue_idx = i;
		c->ue_mbuf = NULL;
		if (c->ue_xfer == NULL) {
			c->ue_xfer = usbd_alloc_xfer(ue_udev);
			if (c->ue_xfer == NULL)
				return (ENOBUFS);
			c->ue_buf = usbd_alloc_buffer(c->ue_xfer, UE_BUFSZ);
			if (c->ue_buf == NULL)
				return (ENOBUFS);
		}
	}

	return (0);
}

void
usb_ether_rx_list_free(struct ue_cdata *cd)
{
	int i;

	for (i = 0; i < UE_RX_LIST_CNT; i++) {
		if (cd->ue_rx_chain[i].ue_mbuf != NULL) {
			m_freem(cd->ue_rx_chain[i].ue_mbuf);
			cd->ue_rx_chain[i].ue_mbuf = NULL;
		}
		if (cd->ue_rx_chain[i].ue_xfer != NULL) {
			usbd_free_xfer(cd->ue_rx_chain[i].ue_xfer);
			cd->ue_rx_chain[i].ue_xfer = NULL;
		}
	}
}

void
usb_ether_tx_list_free(struct ue_cdata *cd)
{
	int i;

	for (i = 0; i < UE_RX_LIST_CNT; i++) {
		if (cd->ue_tx_chain[i].ue_mbuf != NULL) {
			m_freem(cd->ue_tx_chain[i].ue_mbuf);
			cd->ue_tx_chain[i].ue_mbuf = NULL;
		}
		if (cd->ue_tx_chain[i].ue_xfer != NULL) {
			usbd_free_xfer(cd->ue_tx_chain[i].ue_xfer);
			cd->ue_tx_chain[i].ue_xfer = NULL;
		}
	}
}
