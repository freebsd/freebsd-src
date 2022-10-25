/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_mrouting.h"
#include "opt_ipsec.h"
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/domain.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

/*
 * While this file provides the domain and protocol switch tables for IPv4, it
 * also provides the sysctl node declarations for net.inet.* often shared with
 * IPv6 for common features or by upper layer protocols.  In case of no IPv4
 * support compile out everything but these sysctl nodes.
 */
#ifdef INET

#ifdef SCTP
#include <netinet/in_pcb.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp.h>
#include <netinet/sctp_var.h>
#endif

/* netinet/raw_ip.c */
extern struct protosw rip_protosw;
/* netinet/udp_usrreq.c */
extern struct protosw udp_protosw, udplite_protosw;
/* netinet/tcp_usrreq.c */
extern struct protosw tcp_protosw;

FEATURE(inet, "Internet Protocol version 4");

struct domain inetdomain = {
	.dom_family =		AF_INET,
	.dom_name =		"internet",
	.dom_rtattach =		in_inithead,
#ifdef VIMAGE
	.dom_rtdetach =		in_detachhead,
#endif
	.dom_ifattach =		in_domifattach,
	.dom_ifdetach =		in_domifdetach,
	.dom_nprotosw =		14,
	.dom_protosw = {
		&tcp_protosw,
		&udp_protosw,
#ifdef SCTP
		&sctp_seqpacket_protosw,
		&sctp_stream_protosw,
#else
		NULL, NULL,
#endif
		&udplite_protosw,
		&rip_protosw,
		/* Spacer 8 times for loadable protocols. XXXGL: why 8? */
		NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	},
};

DOMAIN_SET(inet);
#endif /* INET */

SYSCTL_NODE(_net, PF_INET, inet, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Internet Family");

SYSCTL_NODE(_net_inet, IPPROTO_IP, ip, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IP");
SYSCTL_NODE(_net_inet, IPPROTO_ICMP, icmp, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "ICMP");
SYSCTL_NODE(_net_inet, IPPROTO_UDP, udp, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "UDP");
SYSCTL_NODE(_net_inet, IPPROTO_TCP, tcp, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TCP");
#if defined(SCTP) || defined(SCTP_SUPPORT)
SYSCTL_NODE(_net_inet, IPPROTO_SCTP, sctp, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "SCTP");
#endif
SYSCTL_NODE(_net_inet, IPPROTO_IGMP, igmp, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IGMP");
#if defined(IPSEC) || defined(IPSEC_SUPPORT)
/* XXX no protocol # to use, pick something "reserved" */
SYSCTL_NODE(_net_inet, 253, ipsec, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IPSEC");
SYSCTL_NODE(_net_inet, IPPROTO_AH, ah, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "AH");
SYSCTL_NODE(_net_inet, IPPROTO_ESP, esp, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "ESP");
SYSCTL_NODE(_net_inet, IPPROTO_IPCOMP, ipcomp, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IPCOMP");
SYSCTL_NODE(_net_inet, IPPROTO_IPIP, ipip, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "IPIP");
#endif /* IPSEC */
SYSCTL_NODE(_net_inet, IPPROTO_RAW, raw, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "RAW");
SYSCTL_NODE(_net_inet, OID_AUTO, accf, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Accept filters");
