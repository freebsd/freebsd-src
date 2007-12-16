/*-
 * Copyright (c) 2007, Chelsio Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Neither the name of the Chelsio Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_ofld.h>
#include <netinet/toedev.h>

int
ofld_connect(struct socket *so, struct sockaddr *nam)
{
	struct ifnet *ifp;
	struct toedev *tdev;
	struct rtentry *rt;
	int error;

	rt = rtalloc1(nam, 1, 0);
	if (rt)
		RT_UNLOCK(rt);
	else
		return (EHOSTUNREACH);

	ifp = rt->rt_ifp;
	tdev = TOEDEV(ifp);
	if (tdev == NULL)
		return (EINVAL);

	if (tdev->tod_can_offload(tdev, so) == 0)
		return (EINVAL);

	if ((error = tdev->tod_connect(tdev, so, rt, nam)))
		return (error);

	return (0);
}

int
ofld_send(struct tcpcb *tp)
{

	return (tp->t_tu->tu_send(tp));
}

int
ofld_rcvd(struct tcpcb *tp)
{

	return (tp->t_tu->tu_rcvd(tp));
}

int
ofld_disconnect(struct tcpcb *tp)
{

	return (tp->t_tu->tu_disconnect(tp));
}

int
ofld_abort(struct tcpcb *tp)
{

	return (tp->t_tu->tu_abort(tp));
}

void
ofld_detach(struct tcpcb *tp)
{

	tp->t_tu->tu_detach(tp);
}

void
ofld_listen_open(struct tcpcb *tp)
{

	EVENTHANDLER_INVOKE(ofld_listen, OFLD_LISTEN_OPEN, tp);
}

void
ofld_listen_close(struct tcpcb *tp)
{

	EVENTHANDLER_INVOKE(ofld_listen, OFLD_LISTEN_CLOSE, tp);
}
