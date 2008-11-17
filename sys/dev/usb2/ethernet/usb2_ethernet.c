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

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/ethernet/usb2_ethernet.h>

MODULE_VERSION(usb2_ethernet, 1);
MODULE_DEPEND(usb2_ethernet, usb2_core, 1, 1, 1);

/*------------------------------------------------------------------------*
 *	usb2_ether_get_mbuf - get a new ethernet aligned mbuf
 *------------------------------------------------------------------------*/
struct mbuf *
usb2_ether_get_mbuf(void)
{
	struct mbuf *m;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m) {
		m->m_len = m->m_pkthdr.len = MCLBYTES;
		m_adj(m, ETHER_ALIGN);
	}
	return (m);
}

/*------------------------------------------------------------------------*
 *	usb2_ether_cc - common ethernet config copy
 *------------------------------------------------------------------------*/
void
usb2_ether_cc(struct ifnet *ifp, usb2_ether_mchash_t *fhash,
    struct usb2_ether_cc *cc)
{
	struct ifmultiaddr *ifma;
	uint8_t i;

	if (ifp == NULL) {
		/* Nothing to do */
		return;
	}
	/* Copy interface flags */

	cc->if_flags = ifp->if_flags;

	/* Copy link layer address */

	for (i = 0; i != ETHER_ADDR_LEN; i++) {
		cc->if_lladdr[i] = IF_LLADDR(ifp)[i];
	}

	/* Check hash filter disable bits */

	if ((ifp->if_flags & IFF_ALLMULTI) ||
	    (ifp->if_flags & IFF_PROMISC)) {

		memset(cc->if_hash, 0xFF, sizeof(cc->if_hash));

	} else if (fhash) {

		/* Compute hash bits for multicast filter */

		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK) {
				continue;
			}
			fhash(cc, LLADDR((struct sockaddr_dl *)
			    (ifma->ifma_addr)));
		}
		IF_ADDR_UNLOCK(ifp);

		/* Compute hash bits for broadcast address */

		if (ifp->if_flags & IFF_BROADCAST) {
			fhash(cc, ifp->if_broadcastaddr);
		}
	}
	return;
}
