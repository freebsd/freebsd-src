/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Alexander V. Chernikov <melifaro@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/proc.h>
#include <sys/ck.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/priv.h> /* priv_check */

#include <net/route.h>
#include <net/route/route_ctl.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_var.h>
#include <netlink/route/route_var.h>

/* Standard bits: built-in the kernel */
SYSCTL_NODE(_net, OID_AUTO, netlink, CTLFLAG_RD, 0,
    "RFC3549 Netlink network state socket family");
SYSCTL_NODE(_net_netlink, OID_AUTO, debug, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Netlink per-subsystem debug levels");

MALLOC_DEFINE(M_NETLINK, "netlink", "Memory used for netlink packets");

/* Netlink-related callbacks needed to glue rtsock, netlink and linuxolator */
static void
ignore_route_event(uint32_t fibnum, const struct rib_cmd_info *rc)
{
}

static void
ignore_ifmsg_event(struct ifnet *ifp, int if_flags_mask)
{
}

static struct rtbridge ignore_cb = {
	.route_f = ignore_route_event,
	.ifmsg_f = ignore_ifmsg_event,
};

void *linux_netlink_p = NULL; /* Callback pointer for Linux translator functions */
struct rtbridge *rtsock_callback_p = &ignore_cb;
struct rtbridge *netlink_callback_p = &ignore_cb;


/*
 * nlp accessors.
 * TODO: move to a separate file once the number grows.
 */
bool
nlp_has_priv(struct nlpcb *nlp, int priv)
{
	return (priv_check_cred(nlp->nl_socket->so_cred, priv) == 0);
}

struct ucred *
nlp_get_cred(struct nlpcb *nlp)
{
	return (nlp->nl_socket->so_cred);
}

uint32_t
nlp_get_pid(const struct nlpcb *nlp)
{
	return (nlp->nl_process_id);
}

bool
nlp_unconstrained_vnet(const struct nlpcb *nlp)
{
	return (nlp->nl_unconstrained_vnet);
}

#ifndef NETLINK
/* Stub implementations for the loadable functions */

static bool
nl_writer_unicast_stub(struct nl_writer *nw, size_t size, struct nlpcb *nlp,
    bool waitok)
{
	return (get_stub_writer(nw));
}

static bool
nl_writer_group_stub(struct nl_writer *nw, size_t size, uint16_t protocol,
    uint16_t group_id, int priv, bool waitok)
{
	return (get_stub_writer(nw));
}

static bool
nlmsg_flush_stub(struct nl_writer *nw __unused)
{
	return (false);
}

static void
nlmsg_ignore_limit_stub(struct nl_writer *nw __unused)
{
}

static bool
nlmsg_refill_buffer_stub(struct nl_writer *nw __unused,
    size_t required_len __unused)
{
	return (false);
}

static bool
nlmsg_add_stub(struct nl_writer *nw, uint32_t portid, uint32_t seq, uint16_t type,
    uint16_t flags, uint32_t len)
{
	return (false);
}

static bool
nlmsg_end_stub(struct nl_writer *nw __unused)
{
	return (false);
}

static void
nlmsg_abort_stub(struct nl_writer *nw __unused)
{
}

static bool
nlmsg_end_dump_stub(struct nl_writer *nw, int error, struct nlmsghdr *hdr)
{
	return (false);
}

static int
nl_modify_ifp_generic_stub(struct ifnet *ifp __unused,
    struct nl_parsed_link *lattrs __unused, const struct nlattr_bmask *bm __unused,
    struct nl_pstate *npt __unused)
{
	return (ENOTSUP);
}

static void
nl_store_ifp_cookie_stub(struct nl_pstate *npt __unused, struct ifnet *ifp __unused)
{
}

static struct nlpcb *
nl_get_thread_nlp_stub(struct thread *td __unused)
{
	return (NULL);
}

const static struct nl_function_wrapper nl_stub = {
	.nlmsg_add = nlmsg_add_stub,
	.nlmsg_refill_buffer = nlmsg_refill_buffer_stub,
	.nlmsg_flush = nlmsg_flush_stub,
	.nlmsg_end = nlmsg_end_stub,
	.nlmsg_abort = nlmsg_abort_stub,
	.nlmsg_ignore_limit = nlmsg_ignore_limit_stub,
	.nl_writer_unicast = nl_writer_unicast_stub,
	.nl_writer_group = nl_writer_group_stub,
	.nlmsg_end_dump = nlmsg_end_dump_stub,
	.nl_modify_ifp_generic = nl_modify_ifp_generic_stub,
	.nl_store_ifp_cookie = nl_store_ifp_cookie_stub,
	.nl_get_thread_nlp = nl_get_thread_nlp_stub,
};

/*
 * If the kernel is compiled with netlink as a module,
 *  provide a way to introduce non-stub functioms
 */
static const struct nl_function_wrapper *_nl = &nl_stub;

void
nl_set_functions(const struct nl_function_wrapper *nl)
{
	_nl = (nl != NULL) ? nl : &nl_stub;
}

/* Function wrappers */
bool
nl_writer_unicast(struct nl_writer *nw, size_t size, struct nlpcb *nlp,
    bool waitok)
{
	return (_nl->nl_writer_unicast(nw, size, nlp, waitok));
}

bool
nl_writer_group(struct nl_writer *nw, size_t size, uint16_t protocol,
    uint16_t group_id, int priv, bool waitok)
{
	return (_nl->nl_writer_group(nw, size, protocol, group_id, priv,
	    waitok));
}

bool
nlmsg_flush(struct nl_writer *nw)
{
	return (_nl->nlmsg_flush(nw));
}

void nlmsg_ignore_limit(struct nl_writer *nw)
{
	_nl->nlmsg_ignore_limit(nw);
}

bool
nlmsg_refill_buffer(struct nl_writer *nw, size_t required_len)
{
	return (_nl->nlmsg_refill_buffer(nw, required_len));
}

bool
nlmsg_add(struct nl_writer *nw, uint32_t portid, uint32_t seq, uint16_t type,
    uint16_t flags, uint32_t len)
{
	return (_nl->nlmsg_add(nw, portid, seq, type, flags, len));
}

bool
nlmsg_end(struct nl_writer *nw)
{
	return (_nl->nlmsg_end(nw));
}

void
nlmsg_abort(struct nl_writer *nw)
{
	_nl->nlmsg_abort(nw);
}

bool
nlmsg_end_dump(struct nl_writer *nw, int error, struct nlmsghdr *hdr)
{
	return (_nl->nlmsg_end_dump(nw, error, hdr));
}

int
nl_modify_ifp_generic(struct ifnet *ifp, struct nl_parsed_link *lattrs,
    const struct nlattr_bmask *bm , struct nl_pstate *npt)
{
	return (_nl->nl_modify_ifp_generic(ifp, lattrs, bm, npt));
}

void
nl_store_ifp_cookie(struct nl_pstate *npt, struct ifnet *ifp)
{
	return (_nl->nl_store_ifp_cookie(npt, ifp));
}

struct nlpcb *
nl_get_thread_nlp(struct thread *td)
{
	return (_nl->nl_get_thread_nlp(td));
}

#endif /* !NETLINK */

