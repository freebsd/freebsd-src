/*
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
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
 *	@(#)ipx_proto.c
 *
 * $Id: ipx_proto.c,v 1.10 1997/05/10 09:58:54 jhay Exp $
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/radix.h>

#include <netipx/ipx.h>
#include <netipx/ipx_var.h>
#include <netipx/spx.h>

extern	struct domain ipxdomain;
static	struct pr_usrreqs nousrreqs;

/*
 * IPX protocol family: IPX, ERR, PXP, SPX, ROUTE.
 */

static struct protosw ipxsw[] = {
{ 0,		&ipxdomain,	0,		0,
  0,		0,		0,		0,
  0,
  ipx_init,	0,		0,		0,
  &nousrreqs
},
{ SOCK_DGRAM,	&ipxdomain,	0,		PR_ATOMIC|PR_ADDR,
  0,		0,		ipx_ctlinput,	ipx_ctloutput,
  0,
  0,		0,		0,		0,
  &ipx_usrreqs
},
{ SOCK_STREAM,	&ipxdomain,	IPXPROTO_SPX,	PR_CONNREQUIRED|PR_WANTRCVD,
  0,		0,		spx_ctlinput,	spx_ctloutput,
  0,
  spx_init,	spx_fasttimo,	spx_slowtimo,	0,
  &spx_usrreqs
},
{ SOCK_SEQPACKET,&ipxdomain,	IPXPROTO_SPX,	PR_CONNREQUIRED|PR_WANTRCVD|PR_ATOMIC,
  0,		0,		spx_ctlinput,	spx_ctloutput,
  0,
  0,		0,		0,		0,
  &spx_usrreq_sps
},
{ SOCK_RAW,	&ipxdomain,	IPXPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  0,		0,		0,		ipx_ctloutput,
  0,
  0,		0,		0,		0,
  &ripx_usrreqs
},
#ifdef IPTUNNEL
#if 0
{ SOCK_RAW,	&ipxdomain,	IPPROTO_IPX,	PR_ATOMIC|PR_ADDR,
  iptun_input,	rip_output,	iptun_ctlinput,	0,
  0,
  0,		0,		0,		0,
  &rip_usrreqs
},
#endif
#endif
};

struct	domain ipxdomain =
    { AF_IPX, "network systems", 0, 0, 0, 
      ipxsw, &ipxsw[sizeof(ipxsw)/sizeof(ipxsw[0])], 0,
      rn_inithead, 16, sizeof(struct sockaddr_ipx)};

DOMAIN_SET(ipx);
SYSCTL_NODE(_net,	PF_IPX,		ipx,	CTLFLAG_RW, 0,
	"IPX/SPX");

SYSCTL_NODE(_net_ipx,	IPXPROTO_RAW,	ipx,	CTLFLAG_RW, 0, "IPX");
SYSCTL_NODE(_net_ipx,	IPXPROTO_SPX,	spx,	CTLFLAG_RW, 0, "SPX");
