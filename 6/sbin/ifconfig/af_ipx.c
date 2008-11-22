/*
 * Copyright (c) 1983, 1993
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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <net/if_var.h>
#define	IPXIP
#define IPTUNNEL
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>

#include "ifconfig.h"

static struct ifaliasreq ipx_addreq;
static struct ifreq ipx_ridreq;

static void
ipx_status(int s __unused, const struct rt_addrinfo * info)
{
	struct sockaddr_ipx *sipx, null_sipx;

	sipx = (struct sockaddr_ipx *)info->rti_info[RTAX_IFA];
	if (sipx == NULL)
		return;

	printf("\tipx %s ", ipx_ntoa(sipx->sipx_addr));

	if (flags & IFF_POINTOPOINT) {
		sipx = (struct sockaddr_ipx *)info->rti_info[RTAX_BRD];
		if (!sipx) {
			memset(&null_sipx, 0, sizeof(null_sipx));
			sipx = &null_sipx;
		}
		printf("--> %s ", ipx_ntoa(sipx->sipx_addr));
	}
	putchar('\n');
}

#define SIPX(x) ((struct sockaddr_ipx *) &(x))
struct sockaddr_ipx *sipxtab[] = {
	SIPX(ipx_ridreq.ifr_addr), SIPX(ipx_addreq.ifra_addr),
	SIPX(ipx_addreq.ifra_mask), SIPX(ipx_addreq.ifra_broadaddr)
};

static void
ipx_getaddr(const char *addr, int which)
{
	struct sockaddr_ipx *sipx = sipxtab[which];

	sipx->sipx_family = AF_IPX;
	sipx->sipx_len = sizeof(*sipx);
	sipx->sipx_addr = ipx_addr(addr);
	if (which == MASK)
		printf("Attempt to set IPX netmask will be ineffectual\n");
}

static void
ipx_postproc(int s, const struct afswtch *afp)
{
	if (setipdst) {
		struct ipxip_req rq;
		int size = sizeof(rq);

		rq.rq_ipx = ipx_addreq.ifra_addr;
		rq.rq_ip = ipx_addreq.ifra_dstaddr;

		if (setsockopt(s, 0, SO_IPXIP_ROUTE, &rq, size) < 0)
			Perror("Encapsulation Routing");
	}
}

static struct afswtch af_ipx = {
	.af_name	= "ipx",
	.af_af		= AF_IPX,
	.af_status	= ipx_status,
	.af_getaddr	= ipx_getaddr,
	.af_postproc	= ipx_postproc,
	.af_difaddr	= SIOCDIFADDR,
	.af_aifaddr	= SIOCAIFADDR,
	.af_ridreq	= &ipx_ridreq,
	.af_addreq	= &ipx_addreq,
};

static __constructor void
ipx_ctor(void)
{
	af_register(&af_ipx);
}
