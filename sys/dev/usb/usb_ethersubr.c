/*
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
 *
 * $FreeBSD: src/sys/dev/usb/usb_ethersubr.c,v 1.4.2.3 2000/05/24 01:47:50 archie Exp $
 */

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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/sys/dev/usb/usb_ethersubr.c,v 1.4.2.3 2000/05/24 01:47:50 archie Exp $";
#endif

Static struct ifqueue usbq_rx;
Static struct ifqueue usbq_tx;

Static void usbintr		__P((void));

Static void usbintr()
{
	struct ether_header	*eh;
	struct mbuf		*m;
	struct usb_qdat		*q;
	struct ifnet		*ifp;
	int			s;

	s = splimp();

	/* Check the RX queue */
	while(1) {
		IF_DEQUEUE(&usbq_rx, m);
		if (m == NULL)
			break;
		eh = mtod(m, struct ether_header *);
		q = (struct usb_qdat *)m->m_pkthdr.rcvif;
		ifp = q->ifp;
		m->m_pkthdr.rcvif = ifp;
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, eh, m);

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

	splx(s);

	return;
}

void usb_register_netisr()
{
	register_netisr(NETISR_USB, usbintr);
	return;
}

/*
 * Must be called at splusp() (actually splbio()). This should be
 * the case when called from a transfer callback routine.
 */
void usb_ether_input(m)
	struct mbuf		*m;
{
	int			s;
	s = splimp();
	IF_ENQUEUE(&usbq_rx, m);
	schednetisr(NETISR_USB);
	splx(s);
	return;
}

void usb_tx_done(m)
	struct mbuf		*m;
{
	int			s;
	s = splimp();
	IF_ENQUEUE(&usbq_tx, m);
	schednetisr(NETISR_USB);
	splx(s);
	return;
}
