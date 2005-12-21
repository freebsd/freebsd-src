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
 *	@(#)in_proto.c	8.2 (Berkeley) 2/9/95
 * $FreeBSD$
 */

#include "opt_ipx.h"
#include "opt_mrouting.h"
#include "opt_ipsec.h"
#include "opt_inet6.h"
#include "opt_pf.h"
#include "opt_carp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/igmp_var.h>
#ifdef PIM
#include <netinet/pim_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_encap.h>

/*
 * TCP/IP protocol family: IP, ICMP, UDP, TCP.
 */

static struct pr_usrreqs nousrreqs;

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netinet6/ah.h>
#ifdef IPSEC_ESP
#include <netinet6/esp.h>
#endif
#include <netinet6/ipcomp.h>
#endif /* IPSEC */

#ifdef FAST_IPSEC
#include <netipsec/ipsec.h>
#endif /* FAST_IPSEC */

#ifdef IPXIP
#include <netipx/ipx_ip.h>
#endif

#ifdef DEV_PFSYNC
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#endif

#ifdef DEV_CARP
#include <netinet/ip_carp.h>
#endif

extern	struct domain inetdomain;

/* Spacer for loadable protocols. */
#define IPPROTOSPACER   			\
{						\
	.pr_domain =		&inetdomain,	\
	.pr_protocol =		PROTO_SPACER,	\
	.pr_usrreqs =		&nousrreqs	\
}

struct protosw inetsw[] = {
{
	.pr_type =		0,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_IP,
	.pr_init =		ip_init,
	.pr_slowtimo =		ip_slowtimo,
	.pr_drain =		ip_drain,
	.pr_usrreqs =		&nousrreqs
},
{
	.pr_type =		SOCK_DGRAM,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_UDP,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		udp_input,
	.pr_ctlinput =		udp_ctlinput,
	.pr_ctloutput =		ip_ctloutput,
	.pr_init =		udp_init,
	.pr_usrreqs =		&udp_usrreqs
},
{
	.pr_type =		SOCK_STREAM,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_TCP,
	.pr_flags =		PR_CONNREQUIRED|PR_IMPLOPCL|PR_WANTRCVD,
	.pr_input =		tcp_input,
	.pr_ctlinput =		tcp_ctlinput,
	.pr_ctloutput =		tcp_ctloutput,
	.pr_init =		tcp_init,
	.pr_slowtimo =		tcp_slowtimo,
	.pr_drain =		tcp_drain,
	.pr_usrreqs =		&tcp_usrreqs
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_RAW,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		rip_input,
	.pr_ctlinput =		rip_ctlinput,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_ICMP,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		icmp_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_IGMP,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		igmp_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_init =		igmp_init,
	.pr_fasttimo =		igmp_fasttimo,
	.pr_slowtimo =		igmp_slowtimo,
	.pr_usrreqs =		&rip_usrreqs
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_RSVP,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		rsvp_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
},
#ifdef IPSEC
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_AH,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		ah4_input,
	.pr_usrreqs =		&nousrreqs
},
#ifdef IPSEC_ESP
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_ESP,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		esp4_input,
	.pr_usrreqs =		&nousrreqs
},
#endif
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_IPCOMP,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		ipcomp4_input,
	.pr_usrreqs =		&nousrreqs
},
#endif /* IPSEC */
#ifdef FAST_IPSEC
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_AH,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		ah4_input,
	.pr_ctlinput =		ah4_ctlinput,
	.pr_usrreqs =		&nousrreqs
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_ESP,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		esp4_input,
	.pr_ctlinput =		esp4_ctlinput,
	.pr_usrreqs =		&nousrreqs
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_IPCOMP,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		ipcomp4_input,
	.pr_usrreqs =		&nousrreqs
},
#endif /* FAST_IPSEC */
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_IPV4,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		encap4_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_init =		encap_init,
	.pr_usrreqs =		&rip_usrreqs
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_MOBILE,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		encap4_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_init =		encap_init,
	.pr_usrreqs =		&rip_usrreqs
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_ETHERIP,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		encap4_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_init =		encap_init,
	.pr_usrreqs =		&rip_usrreqs
},
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_GRE,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		encap4_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_init =		encap_init,
	.pr_usrreqs =		&rip_usrreqs
},
# ifdef INET6
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_IPV6,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		encap4_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_init =		encap_init,
	.pr_usrreqs =		&rip_usrreqs
},
#endif
#ifdef IPXIP
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_IDP,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		ipxip_input,
	.pr_ctlinput =		ipxip_ctlinput,
	.pr_usrreqs =		&rip_usrreqs
},
#endif
#ifdef PIM
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_PIM,
	.pr_flags =		PR_ATOMIC|PR_ADDR|PR_LASTHDR,
	.pr_input =		pim_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
},
#endif	/* PIM */
#ifdef DEV_PFSYNC
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_PFSYNC,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		pfsync_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
},
#endif	/* DEV_PFSYNC */
#ifdef DEV_CARP
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_CARP,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		carp_input,
	.pr_output =		(pr_output_t*)rip_output,
	.pr_ctloutput =		rip_ctloutput,
	.pr_usrreqs =		&rip_usrreqs
},
#endif /* DEV_CARP */
/* Spacer n-times for loadable protocols. */
IPPROTOSPACER,
IPPROTOSPACER,
IPPROTOSPACER,
IPPROTOSPACER,
IPPROTOSPACER,
IPPROTOSPACER,
IPPROTOSPACER,
IPPROTOSPACER,
/* raw wildcard */
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&inetdomain,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_input =		rip_input,
	.pr_ctloutput =		rip_ctloutput,
	.pr_init =		rip_init,
	.pr_usrreqs =		&rip_usrreqs
},
};

extern int in_inithead(void **, int);

struct domain inetdomain = {
	.dom_family =		AF_INET,
	.dom_name =		"internet",
	.dom_protosw =		inetsw,
	.dom_protoswNPROTOSW =	&inetsw[sizeof(inetsw)/sizeof(inetsw[0])],
	.dom_rtattach =		in_inithead,
	.dom_rtoffset =		32,
	.dom_maxrtkey =		sizeof(struct sockaddr_in)
};

DOMAIN_SET(inet);

SYSCTL_NODE(_net,      PF_INET,		inet,	CTLFLAG_RW, 0,
	"Internet Family");

SYSCTL_NODE(_net_inet, IPPROTO_IP,	ip,	CTLFLAG_RW, 0,	"IP");
SYSCTL_NODE(_net_inet, IPPROTO_ICMP,	icmp,	CTLFLAG_RW, 0,	"ICMP");
SYSCTL_NODE(_net_inet, IPPROTO_UDP,	udp,	CTLFLAG_RW, 0,	"UDP");
SYSCTL_NODE(_net_inet, IPPROTO_TCP,	tcp,	CTLFLAG_RW, 0,	"TCP");
SYSCTL_NODE(_net_inet, IPPROTO_IGMP,	igmp,	CTLFLAG_RW, 0,	"IGMP");
#ifdef FAST_IPSEC
/* XXX no protocol # to use, pick something "reserved" */
SYSCTL_NODE(_net_inet, 253,		ipsec,	CTLFLAG_RW, 0,	"IPSEC");
SYSCTL_NODE(_net_inet, IPPROTO_AH,	ah,	CTLFLAG_RW, 0,	"AH");
SYSCTL_NODE(_net_inet, IPPROTO_ESP,	esp,	CTLFLAG_RW, 0,	"ESP");
SYSCTL_NODE(_net_inet, IPPROTO_IPCOMP,	ipcomp,	CTLFLAG_RW, 0,	"IPCOMP");
SYSCTL_NODE(_net_inet, IPPROTO_IPIP,	ipip,	CTLFLAG_RW, 0,	"IPIP");
#else
#ifdef IPSEC
SYSCTL_NODE(_net_inet, IPPROTO_AH,	ipsec,	CTLFLAG_RW, 0,	"IPSEC");
#endif /* IPSEC */
#endif /* !FAST_IPSEC */
SYSCTL_NODE(_net_inet, IPPROTO_RAW,	raw,	CTLFLAG_RW, 0,	"RAW");
#ifdef PIM
SYSCTL_NODE(_net_inet, IPPROTO_PIM,	pim,	CTLFLAG_RW, 0,	"PIM");
#endif
#ifdef DEV_PFSYNC
SYSCTL_NODE(_net_inet, IPPROTO_PFSYNC,	pfsync,	CTLFLAG_RW, 0,	"PFSYNC");
#endif
#ifdef DEV_CARP
SYSCTL_NODE(_net_inet, IPPROTO_CARP,	carp,	CTLFLAG_RW, 0,	"CARP");
#endif
