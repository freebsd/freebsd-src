/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * $FreeBSD: src/sys/net/if_faith.c,v 1.3 2000/01/29 18:10:39 peter Exp $
 */
/*
 * derived from
 *	@(#)if_loop.c	8.1 (Berkeley) 6/10/93
 * Id: if_loop.c,v 1.22 1996/06/19 16:24:10 wollman Exp
 */

/*
 * Loopback interface driver for protocol testing and timing.
 */

#include "faith.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>

extern int loioctl __P((struct ifnet *, u_long, caddr_t));
extern int looutput __P((struct ifnet *ifp,
		struct mbuf *m, struct sockaddr *dst, struct rtentry *rt));

void faithattach __P((void *));
PSEUDO_SET(faithattach, if_faith);
static struct ifnet faithif[NFAITH];

#define	FAITHMTU	1500

/* ARGSUSED */
void
faithattach(faith)
	void *faith;
{
	register struct ifnet *ifp;
	register int i;

	for (i = 0; i < NFAITH; i++) {
		ifp = &faithif[i];
		bzero(ifp, sizeof(faithif[i]));
		ifp->if_name = "faith";
		ifp->if_unit = i;
		ifp->if_mtu = FAITHMTU;
		/* LOOPBACK commented out to announce IPv6 routes to faith */
		ifp->if_flags = /* IFF_LOOPBACK | */ IFF_MULTICAST;
		ifp->if_ioctl = loioctl;
		ifp->if_output = looutput;
		ifp->if_type = IFT_FAITH;
		ifp->if_snd.ifq_maxlen = ifqmaxlen;
		ifp->if_hdrlen = 0;
		ifp->if_addrlen = 0;
		if_attach(ifp);
		bpfattach(ifp, DLT_NULL, sizeof(u_int));
	}
}
