/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: rmp.c 1.3 89/06/07$
 *
 *	From: @(#)rmp.c	7.1 (Berkeley) 5/8/90
 *	$Id: rmp.c,v 1.3 1993/12/19 00:54:07 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"

#include "../net/if.h"
#include "../net/route.h"
#include "../net/raw_cb.h"

#include "../netrmp/rmp.h"
#include "../netrmp/rmp_var.h"

/*
**  rmp_output: route packet to proper network interface.
*/

int
rmp_output(m, so)
	struct mbuf *m;
	struct socket *so;
{
	struct ifnet *ifp;
	struct rawcb *rp = sotorawcb(so);
	struct rmp_packet *rmp;

	/*
	 *  Convert the mbuf back to an RMP packet so we can get the
	 *  address of the "ifnet struct" specifying the interface it
	 *  should go out on.
	 */
	rmp = mtod(m, struct rmp_packet *);
	ifp = rmp->ifp;

	/*
	 *  Strip off the "ifnet struct ptr" from the packet leaving
	 *  us with a complete IEEE 802.2 packet.
	 */
	m_adj(m, sizeof(struct ifnet *));

	/*
	 *  Send the packet.
	 */
	return (*ifp->if_output) (ifp, m, (struct sockaddr *)&rp->rcb_faddr,
		(struct rtentry *)0);
}
