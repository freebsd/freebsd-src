/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
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
 *
 * $FreeBSD$
 */

/*
 * Developed by the TrustedBSD Project.
 * Limit access to interfaces until they are specifically administratively
 * enabled.  Prevents protocol stack-driven packet leakage in unsafe
 * environments.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <vm/vm.h>

#include <sys/mac_policy.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, ifoff, CTLFLAG_RW, 0,
    "TrustedBSD mac_ifoff policy controls");

static int	mac_ifoff_enabled = 1;
SYSCTL_INT(_security_mac_ifoff, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_ifoff_enabled, 0, "Enforce ifoff policy");
TUNABLE_INT("security.mac.ifoff.enabled", &mac_ifoff_enabled);

static int	mac_ifoff_lo_enabled = 1;
SYSCTL_INT(_security_mac_ifoff, OID_AUTO, lo_enabled, CTLFLAG_RW,
    &mac_ifoff_lo_enabled, 0, "Enable loopback interfaces");
TUNABLE_INT("security.mac.ifoff.lo_enabled", &mac_ifoff_lo_enabled);

static int	mac_ifoff_other_enabled = 0;
SYSCTL_INT(_security_mac_ifoff, OID_AUTO, other_enabled, CTLFLAG_RW,
    &mac_ifoff_other_enabled, 0, "Enable other interfaces");
TUNABLE_INT("security.mac.ifoff.other_enabled", &mac_ifoff_other_enabled);

static int	mac_ifoff_bpfrecv_enabled = 0;
SYSCTL_INT(_security_mac_ifoff, OID_AUTO, bpfrecv_enabled, CTLFLAG_RW,
    &mac_ifoff_bpfrecv_enabled, 0, "Enable BPF reception even when interface "
    "is disabled");
TUNABLE_INT("security.mac.ifoff.bpfrecv.enabled", &mac_ifoff_bpfrecv_enabled);

static int
check_ifnet_outgoing(struct ifnet *ifnet)
{

	if (!mac_ifoff_enabled)
		return (0);

	if (mac_ifoff_lo_enabled && ifnet->if_type == IFT_LOOP)
		return (0);

	if (mac_ifoff_other_enabled && ifnet->if_type != IFT_LOOP)
		return (0);

	return (EPERM);
}

static int
check_ifnet_incoming(struct ifnet *ifnet, int viabpf)
{
	if (!mac_ifoff_enabled)
		return (0);

	if (mac_ifoff_lo_enabled && ifnet->if_type == IFT_LOOP)  
		return (0);

	if (mac_ifoff_other_enabled && ifnet->if_type != IFT_LOOP)
		return (0);

	if (viabpf && mac_ifoff_bpfrecv_enabled)
		return (0);

	return (EPERM);
}

static int
mac_ifoff_check_bpfdesc_receive(struct bpf_d *bpf_d, struct label *bpflabel,
    struct ifnet *ifnet, struct label *ifnetlabel)
{

	return (check_ifnet_incoming(ifnet, 1));
}

static int
mac_ifoff_check_ifnet_transmit(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	return (check_ifnet_outgoing(ifnet));
}

static int
mac_ifoff_check_socket_deliver(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	if (m->m_flags & M_PKTHDR) {
		if (m->m_pkthdr.rcvif != NULL)
			return (check_ifnet_incoming(m->m_pkthdr.rcvif, 0));
	}

	return (0);
}

static struct mac_policy_ops mac_ifoff_ops =
{
	.mpo_check_bpfdesc_receive = mac_ifoff_check_bpfdesc_receive,
	.mpo_check_ifnet_transmit = mac_ifoff_check_ifnet_transmit,
	.mpo_check_socket_deliver = mac_ifoff_check_socket_deliver,
};

MAC_POLICY_SET(&mac_ifoff_ops, trustedbsd_mac_ifoff, "TrustedBSD MAC/ifoff",
    MPC_LOADTIME_FLAG_UNLOADOK, NULL);
