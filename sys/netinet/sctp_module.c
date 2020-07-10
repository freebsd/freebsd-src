/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019-2020 The FreeBSD Foundation
 *
 * This software was developed by Mark Johnston under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/sctp.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_os_bsd.h>

#include <netinet6/ip6_var.h>
#include <netinet6/sctp6_var.h>

#ifdef INET
extern struct domain inetdomain;

struct protosw sctp_stream_protosw = {
	.pr_type =		SOCK_STREAM,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_SCTP,
	.pr_flags =		PR_CONNREQUIRED|PR_WANTRCVD|PR_LASTHDR,
	.pr_input =		sctp_input,
	.pr_ctlinput =		sctp_ctlinput,
	.pr_ctloutput =		sctp_ctloutput,
	.pr_drain =		sctp_drain,
	.pr_usrreqs =		&sctp_usrreqs,
};

struct protosw sctp_seqpacket_protosw = {
	.pr_type =		SOCK_SEQPACKET,
	.pr_domain =		&inetdomain,
	.pr_protocol =		IPPROTO_SCTP,
	.pr_flags =		PR_WANTRCVD|PR_LASTHDR,
	.pr_input =		sctp_input,
	.pr_ctlinput =		sctp_ctlinput,
	.pr_ctloutput =		sctp_ctloutput,
	.pr_drain =		sctp_drain,
	.pr_usrreqs =		&sctp_usrreqs,
};
#endif

#ifdef INET6
extern struct domain inet6domain;

struct protosw sctp6_stream_protosw = {
	.pr_type =		SOCK_STREAM,
	.pr_domain =		&inet6domain,
	.pr_protocol =		IPPROTO_SCTP,
	.pr_flags =		PR_CONNREQUIRED|PR_WANTRCVD|PR_LASTHDR,
	.pr_input =		sctp6_input,
	.pr_ctlinput =		sctp6_ctlinput,
	.pr_ctloutput =		sctp_ctloutput,
	.pr_init =		sctp_init,
	.pr_drain =		sctp_drain,
	.pr_usrreqs =		&sctp6_usrreqs,
};

struct protosw sctp6_seqpacket_protosw = {
	.pr_type =		SOCK_SEQPACKET,
	.pr_domain =		&inet6domain,
	.pr_protocol =		IPPROTO_SCTP,
	.pr_flags =		PR_WANTRCVD|PR_LASTHDR,
	.pr_input =		sctp6_input,
	.pr_ctlinput =		sctp6_ctlinput,
	.pr_ctloutput =		sctp_ctloutput,
#ifndef INET	/* Do not call initialization and drain routines twice. */
	.pr_init =		sctp_init,
	.pr_drain =		sctp_drain,
#endif
	.pr_usrreqs =		&sctp6_usrreqs,
};
#endif

static int
sctp_module_load(void)
{
	int error;

#ifdef INET
	error = pf_proto_register(PF_INET, &sctp_stream_protosw);
	if (error != 0)
		return (error);
	error = pf_proto_register(PF_INET, &sctp_seqpacket_protosw);
	if (error != 0)
		return (error);
	error = ipproto_register(IPPROTO_SCTP);
	if (error != 0)
		return (error);
#endif
#ifdef INET6
	error = pf_proto_register(PF_INET6, &sctp6_stream_protosw);
	if (error != 0)
		return (error);
	error = pf_proto_register(PF_INET6, &sctp6_seqpacket_protosw);
	if (error != 0)
		return (error);
	error = ip6proto_register(IPPROTO_SCTP);
	if (error != 0)
		return (error);
#endif
	error = sctp_syscalls_init();
	if (error != 0)
		return (error);
	return (0);
}

static int __unused
sctp_module_unload(void)
{

	(void)sctp_syscalls_uninit();

#ifdef INET
	(void)ipproto_unregister(IPPROTO_SCTP);
	(void)pf_proto_unregister(PF_INET, IPPROTO_SCTP, SOCK_STREAM);
	(void)pf_proto_unregister(PF_INET, IPPROTO_SCTP, SOCK_SEQPACKET);
#endif
#ifdef INET6
	(void)ip6proto_unregister(IPPROTO_SCTP);
	(void)pf_proto_unregister(PF_INET6, IPPROTO_SCTP, SOCK_STREAM);
	(void)pf_proto_unregister(PF_INET6, IPPROTO_SCTP, SOCK_SEQPACKET);
#endif
	return (0);
}

static int
sctp_modload(struct module *module, int cmd, void *arg)
{
	int error;

	switch (cmd) {
	case MOD_LOAD:
		error = sctp_module_load();
		break;
	case MOD_UNLOAD:
		/*
		 * Unloading SCTP is currently unsupported.  Currently, SCTP
		 * iterator threads are not stopped during unload.
		 */
		error = EOPNOTSUPP;
		break;
	default:
		error = 0;
		break;
	}
	return (error);
}

static moduledata_t sctp_mod = {
	"sctp",
	&sctp_modload,
	NULL,
};

DECLARE_MODULE(sctp, sctp_mod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);
MODULE_VERSION(sctp, 1);
