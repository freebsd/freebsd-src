/*-
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/radix.h>

#include <netipx/ipx.h>
#include <netipx/ipx_var.h>
#include <netipx/spx.h>

static	struct pr_usrreqs nousrreqs;

/*
 * IPX protocol family: IPX, ERR, PXP, SPX, ROUTE.
 */

static	struct domain ipxdomain;

static struct protosw ipxsw[] = {
{
	.pr_domain =		&ipxdomain,
	.pr_init =		ipx_init,
	.pr_usrreqs =		&nousrreqs
},
{
	.pr_type =		SOCK_DGRAM,
	.pr_domain =		&ipxdomain,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_ctlinput =		ipx_ctlinput,
	.pr_ctloutput =		ipx_ctloutput,
	.pr_usrreqs =		&ipx_usrreqs
},
{
	.pr_type =		SOCK_STREAM,
	.pr_domain =		&ipxdomain,
	.pr_protocol =		IPXPROTO_SPX,
	.pr_flags =		PR_CONNREQUIRED|PR_WANTRCVD,
	.pr_ctlinput =		spx_ctlinput,
	.pr_ctloutput =		spx_ctloutput,
	.pr_init =		spx_init,
	.pr_fasttimo =		spx_fasttimo,
	.pr_slowtimo =		spx_slowtimo,
	.pr_usrreqs =		&spx_usrreqs
},
{
	.pr_type =		SOCK_SEQPACKET,
	.pr_domain =		&ipxdomain,
	.pr_protocol =		IPXPROTO_SPX,
	.pr_flags =		PR_CONNREQUIRED|PR_WANTRCVD|PR_ATOMIC,
	.pr_ctlinput =		spx_ctlinput,
	.pr_ctloutput =		spx_ctloutput,
	.pr_usrreqs =		&spx_usrreq_sps
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&ipxdomain,
	.pr_protocol =		IPXPROTO_RAW,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_ctloutput =		ipx_ctloutput,
	.pr_usrreqs =		&ripx_usrreqs
},
};

static struct	domain ipxdomain = {
	.dom_family =		AF_IPX,
	.dom_name =		"network systems",
	.dom_protosw =		ipxsw,
	.dom_protoswNPROTOSW =	&ipxsw[sizeof(ipxsw)/sizeof(ipxsw[0])],
	.dom_rtattach =		rn_inithead,
	.dom_rtoffset =		16,
	.dom_maxrtkey =		sizeof(struct sockaddr_ipx)
};

DOMAIN_SET(ipx);
SYSCTL_NODE(_net,	PF_IPX,		ipx,	CTLFLAG_RW, 0,
	"IPX/SPX");

SYSCTL_NODE(_net_ipx,	IPXPROTO_RAW,	ipx,	CTLFLAG_RW, 0, "IPX");
SYSCTL_NODE(_net_ipx,	IPXPROTO_SPX,	spx,	CTLFLAG_RW, 0, "SPX");
