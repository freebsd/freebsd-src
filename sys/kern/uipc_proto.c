/*-
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
 *	@(#)uipc_proto.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/un.h>

#include <net/raw_cb.h>

/*
 * Definitions of protocols supported in the LOCAL domain.
 */

static struct protosw localsw[] = {
{
	.pr_type =		SOCK_STREAM,
	.pr_domain =		&localdomain,
	.pr_flags =		PR_CONNREQUIRED|PR_WANTRCVD|PR_RIGHTS,
	.pr_ctloutput =		&uipc_ctloutput,
	.pr_usrreqs =		&uipc_usrreqs
},
{
	.pr_type =		SOCK_DGRAM,
	.pr_domain =		&localdomain,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_RIGHTS,
	.pr_usrreqs =		&uipc_usrreqs
},
{
	.pr_ctlinput =		raw_ctlinput,
	.pr_init =		raw_init,
	.pr_usrreqs =		&raw_usrreqs
}
};

struct domain localdomain = {
	.dom_family =		AF_LOCAL,
	.dom_name =		"local",
	.dom_init =		unp_init,
	.dom_externalize =	unp_externalize,
	.dom_dispose =		unp_dispose,
	.dom_protosw =		localsw,
	.dom_protoswNPROTOSW =	&localsw[sizeof(localsw)/sizeof(localsw[0])]
};
DOMAIN_SET(local);

SYSCTL_NODE(_net, PF_LOCAL, local, CTLFLAG_RW, 0, "Local domain");
SYSCTL_NODE(_net_local, SOCK_STREAM, stream, CTLFLAG_RW, 0, "SOCK_STREAM");
SYSCTL_NODE(_net_local, SOCK_DGRAM, dgram, CTLFLAG_RW, 0, "SOCK_DGRAM");
