/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001-2004 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
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

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <sys/mac_policy.h>

#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>

#include <security/mac/mac_internal.h>

#ifdef MAC_DEBUG
static unsigned int nmacinpcbs, nmacipqs;

SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, inpcbs, CTLFLAG_RD,
    &nmacinpcbs, 0, "number of inpcbs in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, ipqs, CTLFLAG_RD,
    &nmacipqs, 0, "number of ipqs in use");
#endif

static struct label *
mac_inpcb_label_alloc(int flag)
{
	struct label *label;
	int error;

	label = mac_labelzone_alloc(flag);
	if (label == NULL)
		return (NULL);
	MAC_CHECK(init_inpcb_label, label, flag);
	if (error) {
		MAC_PERFORM(destroy_inpcb_label, label);
		mac_labelzone_free(label);
		return (NULL);
	}
	MAC_DEBUG_COUNTER_INC(&nmacinpcbs);
	return (label);
}

int
mac_init_inpcb(struct inpcb *inp, int flag)
{

	inp->inp_label = mac_inpcb_label_alloc(flag);
	if (inp->inp_label == NULL)
		return (ENOMEM);
	return (0);
}

static struct label *
mac_ipq_label_alloc(int flag)
{
	struct label *label;
	int error;

	label = mac_labelzone_alloc(flag);
	if (label == NULL)
		return (NULL);

	MAC_CHECK(init_ipq_label, label, flag);
	if (error) {
		MAC_PERFORM(destroy_ipq_label, label);
		mac_labelzone_free(label);
		return (NULL);
	}
	MAC_DEBUG_COUNTER_INC(&nmacipqs);
	return (label);
}

int
mac_init_ipq(struct ipq *ipq, int flag)
{

	ipq->ipq_label = mac_ipq_label_alloc(flag);
	if (ipq->ipq_label == NULL)
		return (ENOMEM);
	return (0);
}

static void
mac_inpcb_label_free(struct label *label)
{

	MAC_PERFORM(destroy_inpcb_label, label);
	mac_labelzone_free(label);
	MAC_DEBUG_COUNTER_DEC(&nmacinpcbs);
}

void
mac_destroy_inpcb(struct inpcb *inp)
{

	mac_inpcb_label_free(inp->inp_label);
	inp->inp_label = NULL;
}

static void
mac_ipq_label_free(struct label *label)
{

	MAC_PERFORM(destroy_ipq_label, label);
	mac_labelzone_free(label);
	MAC_DEBUG_COUNTER_DEC(&nmacipqs);
}

void
mac_destroy_ipq(struct ipq *ipq)
{

	mac_ipq_label_free(ipq->ipq_label);
	ipq->ipq_label = NULL;
}

void
mac_create_inpcb_from_socket(struct socket *so, struct inpcb *inp)
{

	MAC_PERFORM(create_inpcb_from_socket, so, so->so_label, inp,
	    inp->inp_label);
}

void
mac_create_datagram_from_ipq(struct ipq *ipq, struct mbuf *datagram)
{
	struct label *label;

	label = mac_mbuf_to_label(datagram);

	MAC_PERFORM(create_datagram_from_ipq, ipq, ipq->ipq_label,
	    datagram, label);
}

void
mac_create_fragment(struct mbuf *datagram, struct mbuf *fragment)
{
	struct label *datagramlabel, *fragmentlabel;

	datagramlabel = mac_mbuf_to_label(datagram);
	fragmentlabel = mac_mbuf_to_label(fragment);

	MAC_PERFORM(create_fragment, datagram, datagramlabel, fragment,
	    fragmentlabel);
}

void
mac_create_ipq(struct mbuf *fragment, struct ipq *ipq)
{
	struct label *label;

	label = mac_mbuf_to_label(fragment);

	MAC_PERFORM(create_ipq, fragment, label, ipq, ipq->ipq_label);
}

void
mac_create_mbuf_from_inpcb(struct inpcb *inp, struct mbuf *m)
{
	struct label *mlabel;

	INP_LOCK_ASSERT(inp);
	mlabel = mac_mbuf_to_label(m);

	MAC_PERFORM(create_mbuf_from_inpcb, inp, inp->inp_label, m, mlabel);
}

int
mac_fragment_match(struct mbuf *fragment, struct ipq *ipq)
{
	struct label *label;
	int result;

	label = mac_mbuf_to_label(fragment);

	result = 1;
	MAC_BOOLEAN(fragment_match, &&, fragment, label, ipq,
	    ipq->ipq_label);

	return (result);
}

void
mac_reflect_mbuf_icmp(struct mbuf *m)
{
	struct label *label;

	label = mac_mbuf_to_label(m);

	MAC_PERFORM(reflect_mbuf_icmp, m, label);
}
void
mac_reflect_mbuf_tcp(struct mbuf *m)
{
	struct label *label;

	label = mac_mbuf_to_label(m);

	MAC_PERFORM(reflect_mbuf_tcp, m, label);
}

void
mac_update_ipq(struct mbuf *fragment, struct ipq *ipq)
{
	struct label *label;

	label = mac_mbuf_to_label(fragment);

	MAC_PERFORM(update_ipq, fragment, label, ipq, ipq->ipq_label);
}

int
mac_check_inpcb_deliver(struct inpcb *inp, struct mbuf *m)
{
	struct label *label;
	int error;

	M_ASSERTPKTHDR(m);

	if (!mac_enforce_socket)
		return (0);

	label = mac_mbuf_to_label(m);

	MAC_CHECK(check_inpcb_deliver, inp, inp->inp_label, m, label);

	return (error);
}

void
mac_inpcb_sosetlabel(struct socket *so, struct inpcb *inp)
{

	/* XXX: assert socket lock. */
	INP_LOCK_ASSERT(inp);
	MAC_PERFORM(inpcb_sosetlabel, so, so->so_label, inp, inp->inp_label);
}

void
mac_create_mbuf_from_firewall(struct mbuf *m)
{
	struct label *label;

	M_ASSERTPKTHDR(m);
	label = mac_mbuf_to_label(m);
	MAC_PERFORM(create_mbuf_from_firewall, m, label);
}
