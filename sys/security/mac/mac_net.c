/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001, 2002, 2003 Networks Associates Technology, Inc.
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
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <sys/mac_policy.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>

#include <security/mac/mac_internal.h>

static int	mac_enforce_network = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_network, CTLFLAG_RW,
    &mac_enforce_network, 0, "Enforce MAC policy on network packets");
TUNABLE_INT("security.mac.enforce_network", &mac_enforce_network);

static int	mac_enforce_socket = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_socket, CTLFLAG_RW,
    &mac_enforce_socket, 0, "Enforce MAC policy on socket operations");
TUNABLE_INT("security.mac.enforce_socket", &mac_enforce_socket);

#ifdef MAC_DEBUG
static unsigned int nmacmbufs, nmacifnets, nmacbpfdescs, nmacsockets,
    nmacipqs;

SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, mbufs, CTLFLAG_RD,
    &nmacmbufs, 0, "number of mbufs in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, ifnets, CTLFLAG_RD,
    &nmacifnets, 0, "number of ifnets in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, ipqs, CTLFLAG_RD,
    &nmacipqs, 0, "number of ipqs in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, bpfdescs, CTLFLAG_RD,
    &nmacbpfdescs, 0, "number of bpfdescs in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, sockets, CTLFLAG_RD,
    &nmacsockets, 0, "number of sockets in use");
#endif

static void	mac_socket_label_free(struct label *label);


static struct label *
mbuf_to_label(struct mbuf *mbuf)
{
	struct m_tag *tag;
	struct label *label;

	tag = m_tag_find(mbuf, PACKET_TAG_MACLABEL, NULL);
	label = (struct label *)(tag+1);

	return (label);
}

static struct label *
mac_bpfdesc_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(M_WAITOK);
	MAC_PERFORM(init_bpfdesc_label, label);
	MAC_DEBUG_COUNTER_INC(&nmacbpfdescs);
	return (label);
}

void
mac_init_bpfdesc(struct bpf_d *bpf_d)
{

	bpf_d->bd_label = mac_bpfdesc_label_alloc();
}

static struct label *
mac_ifnet_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(M_WAITOK);
	MAC_PERFORM(init_ifnet_label, label);
	MAC_DEBUG_COUNTER_INC(&nmacifnets);
	return (label);
}

void
mac_init_ifnet(struct ifnet *ifp)
{

	ifp->if_label = mac_ifnet_label_alloc();
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

int
mac_init_mbuf_tag(struct m_tag *tag, int flag)
{
	struct label *label;
	int error;

	label = (struct label *) (tag + 1);
	mac_init_label(label);

	MAC_CHECK(init_mbuf_label, label, flag);
	if (error) {
		MAC_PERFORM(destroy_mbuf_label, label);
		mac_destroy_label(label);
	} else {
		MAC_DEBUG_COUNTER_INC(&nmacmbufs);
	}
	return (error);
}

int
mac_init_mbuf(struct mbuf *m, int flag)
{
	struct m_tag *tag;
	int error;

	M_ASSERTPKTHDR(m);

#ifndef MAC_ALWAYS_LABEL_MBUF
	/*
	 * If conditionally allocating mbuf labels, don't allocate unless
	 * they are required.
	 */
	if (!mac_labelmbufs)
		return (0);
#endif
	tag = m_tag_get(PACKET_TAG_MACLABEL, sizeof(struct label),
	    flag);
	if (tag == NULL)
		return (ENOMEM);
	error = mac_init_mbuf_tag(tag, flag);
	if (error) {
		m_tag_free(tag);
		return (error);
	}
	m_tag_prepend(m, tag);
	return (0);
}

static struct label *
mac_socket_label_alloc(int flag)
{
	struct label *label;
	int error;

	label = mac_labelzone_alloc(flag);
	if (label == NULL)
		return (NULL);

	MAC_CHECK(init_socket_label, label, flag);
	if (error) {
		MAC_PERFORM(destroy_socket_label, label);
		mac_labelzone_free(label);
		return (NULL);
	}
	MAC_DEBUG_COUNTER_INC(&nmacsockets);
	return (label);
}

static struct label *
mac_socket_peer_label_alloc(int flag)
{
	struct label *label;
	int error;

	label = mac_labelzone_alloc(flag);
	if (label == NULL)
		return (NULL);

	MAC_CHECK(init_socket_peer_label, label, flag);
	if (error) {
		MAC_PERFORM(destroy_socket_peer_label, label);
		mac_labelzone_free(label);
		return (NULL);
	}
	MAC_DEBUG_COUNTER_INC(&nmacsockets);
	return (label);
}

int
mac_init_socket(struct socket *so, int flag)
{

	so->so_label = mac_socket_label_alloc(flag);
	if (so->so_label == NULL)
		return (ENOMEM);
	so->so_peerlabel = mac_socket_peer_label_alloc(flag);
	if (so->so_peerlabel == NULL) {
		mac_socket_label_free(so->so_label);
		so->so_label = NULL;
		return (ENOMEM);
	}
	return (0);
}

static void
mac_bpfdesc_label_free(struct label *label)
{

	MAC_PERFORM(destroy_bpfdesc_label, label);
	mac_labelzone_free(label);
	MAC_DEBUG_COUNTER_DEC(&nmacbpfdescs);
}

void
mac_destroy_bpfdesc(struct bpf_d *bpf_d)
{

	mac_bpfdesc_label_free(bpf_d->bd_label);
	bpf_d->bd_label = NULL;
}

static void
mac_ifnet_label_free(struct label *label)
{

	MAC_PERFORM(destroy_ifnet_label, label);
	mac_labelzone_free(label);
	MAC_DEBUG_COUNTER_DEC(&nmacifnets);
}

void
mac_destroy_ifnet(struct ifnet *ifp)
{

	mac_ifnet_label_free(ifp->if_label);
	ifp->if_label = NULL;
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
mac_destroy_mbuf_tag(struct m_tag *tag)
{
	struct label *label;

	label = (struct label *)(tag+1);

	MAC_PERFORM(destroy_mbuf_label, label);
	mac_destroy_label(label);
	MAC_DEBUG_COUNTER_DEC(&nmacmbufs);
}

static void
mac_socket_label_free(struct label *label)
{

	MAC_PERFORM(destroy_socket_label, label);
	mac_labelzone_free(label);
	MAC_DEBUG_COUNTER_DEC(&nmacsockets);
}

static void
mac_socket_peer_label_free(struct label *label)
{

	MAC_PERFORM(destroy_socket_peer_label, label);
	mac_labelzone_free(label);
	MAC_DEBUG_COUNTER_DEC(&nmacsockets);
}

void
mac_destroy_socket(struct socket *socket)
{

	mac_socket_label_free(socket->so_label);
	socket->so_label = NULL;
	mac_socket_peer_label_free(socket->so_peerlabel);
	socket->so_peerlabel = NULL;
}

void
mac_copy_mbuf_tag(struct m_tag *src, struct m_tag *dest)
{
	struct label *src_label, *dest_label;

	src_label = (struct label *)(src+1);
	dest_label = (struct label *)(dest+1);

	/*
	 * mac_init_mbuf_tag() is called on the target tag in
	 * m_tag_copy(), so we don't need to call it here.
	 */
	MAC_PERFORM(copy_mbuf_label, src_label, dest_label);
}

static int
mac_externalize_ifnet_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen)
{
	int error;

	MAC_EXTERNALIZE(ifnet, label, elements, outbuf, outbuflen);

	return (error);
}

static int
mac_externalize_socket_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen)
{
	int error;

	MAC_EXTERNALIZE(socket, label, elements, outbuf, outbuflen);

	return (error);
}

static int
mac_externalize_socket_peer_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen)
{
	int error;

	MAC_EXTERNALIZE(socket_peer, label, elements, outbuf, outbuflen);

	return (error);
}

static int
mac_internalize_ifnet_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(ifnet, label, string);

	return (error);
}

static int
mac_internalize_socket_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(socket, label, string);

	return (error);
}

void
mac_create_ifnet(struct ifnet *ifnet)
{

	MAC_PERFORM(create_ifnet, ifnet, ifnet->if_label);
}

void
mac_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d)
{

	MAC_PERFORM(create_bpfdesc, cred, bpf_d, bpf_d->bd_label);
}

void
mac_create_socket(struct ucred *cred, struct socket *socket)
{

	MAC_PERFORM(create_socket, cred, socket, socket->so_label);
}

void
mac_create_socket_from_socket(struct socket *oldsocket,
    struct socket *newsocket)
{

	MAC_PERFORM(create_socket_from_socket, oldsocket, oldsocket->so_label,
	    newsocket, newsocket->so_label);
}

static void
mac_relabel_socket(struct ucred *cred, struct socket *socket,
    struct label *newlabel)
{

	MAC_PERFORM(relabel_socket, cred, socket, socket->so_label, newlabel);
}

void
mac_set_socket_peer_from_mbuf(struct mbuf *mbuf, struct socket *socket)
{
	struct label *label;

	label = mbuf_to_label(mbuf);

	MAC_PERFORM(set_socket_peer_from_mbuf, mbuf, label, socket,
	    socket->so_peerlabel);
}

void
mac_set_socket_peer_from_socket(struct socket *oldsocket,
    struct socket *newsocket)
{

	MAC_PERFORM(set_socket_peer_from_socket, oldsocket,
	    oldsocket->so_label, newsocket, newsocket->so_peerlabel);
}

void
mac_create_datagram_from_ipq(struct ipq *ipq, struct mbuf *datagram)
{
	struct label *label;

	label = mbuf_to_label(datagram);

	MAC_PERFORM(create_datagram_from_ipq, ipq, ipq->ipq_label,
	    datagram, label);
}

void
mac_create_fragment(struct mbuf *datagram, struct mbuf *fragment)
{
	struct label *datagramlabel, *fragmentlabel;

	datagramlabel = mbuf_to_label(datagram);
	fragmentlabel = mbuf_to_label(fragment);

	MAC_PERFORM(create_fragment, datagram, datagramlabel, fragment,
	    fragmentlabel);
}

void
mac_create_ipq(struct mbuf *fragment, struct ipq *ipq)
{
	struct label *label;

	label = mbuf_to_label(fragment);

	MAC_PERFORM(create_ipq, fragment, label, ipq, ipq->ipq_label);
}

void
mac_create_mbuf_from_mbuf(struct mbuf *oldmbuf, struct mbuf *newmbuf)
{
	struct label *oldmbuflabel, *newmbuflabel;

	oldmbuflabel = mbuf_to_label(oldmbuf);
	newmbuflabel = mbuf_to_label(newmbuf);

	MAC_PERFORM(create_mbuf_from_mbuf, oldmbuf, oldmbuflabel, newmbuf,
	    newmbuflabel);
}

void
mac_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct mbuf *mbuf)
{
	struct label *label;

	label = mbuf_to_label(mbuf);

	MAC_PERFORM(create_mbuf_from_bpfdesc, bpf_d, bpf_d->bd_label, mbuf,
	    label);
}

void
mac_create_mbuf_linklayer(struct ifnet *ifnet, struct mbuf *mbuf)
{
	struct label *label;

	label = mbuf_to_label(mbuf);

	MAC_PERFORM(create_mbuf_linklayer, ifnet, ifnet->if_label, mbuf,
	    label);
}

void
mac_create_mbuf_from_ifnet(struct ifnet *ifnet, struct mbuf *mbuf)
{
	struct label *label;

	label = mbuf_to_label(mbuf);

	MAC_PERFORM(create_mbuf_from_ifnet, ifnet, ifnet->if_label, mbuf,
	    label);
}

void
mac_create_mbuf_multicast_encap(struct mbuf *oldmbuf, struct ifnet *ifnet,
    struct mbuf *newmbuf)
{
	struct label *oldmbuflabel, *newmbuflabel;

	oldmbuflabel = mbuf_to_label(oldmbuf);
	newmbuflabel = mbuf_to_label(newmbuf);

	MAC_PERFORM(create_mbuf_multicast_encap, oldmbuf, oldmbuflabel,
	    ifnet, ifnet->if_label, newmbuf, newmbuflabel);
}

void
mac_create_mbuf_netlayer(struct mbuf *oldmbuf, struct mbuf *newmbuf)
{
	struct label *oldmbuflabel, *newmbuflabel;

	oldmbuflabel = mbuf_to_label(oldmbuf);
	newmbuflabel = mbuf_to_label(newmbuf);

	MAC_PERFORM(create_mbuf_netlayer, oldmbuf, oldmbuflabel, newmbuf,
	    newmbuflabel);
}

int
mac_fragment_match(struct mbuf *fragment, struct ipq *ipq)
{
	struct label *label;
	int result;

	label = mbuf_to_label(fragment);

	result = 1;
	MAC_BOOLEAN(fragment_match, &&, fragment, label, ipq,
	    ipq->ipq_label);

	return (result);
}

void
mac_reflect_mbuf_icmp(struct mbuf *m)
{
	struct label *label;

	label = mbuf_to_label(m);

	MAC_PERFORM(reflect_mbuf_icmp, m, label);
}
void
mac_reflect_mbuf_tcp(struct mbuf *m)
{
	struct label *label;

	label = mbuf_to_label(m);

	MAC_PERFORM(reflect_mbuf_tcp, m, label);
}

void
mac_update_ipq(struct mbuf *fragment, struct ipq *ipq)
{
	struct label *label;

	label = mbuf_to_label(fragment);

	MAC_PERFORM(update_ipq, fragment, label, ipq, ipq->ipq_label);
}

void
mac_create_mbuf_from_socket(struct socket *socket, struct mbuf *mbuf)
{
	struct label *label;

	label = mbuf_to_label(mbuf);

	MAC_PERFORM(create_mbuf_from_socket, socket, socket->so_label, mbuf,
	    label);
}

int
mac_check_bpfdesc_receive(struct bpf_d *bpf_d, struct ifnet *ifnet)
{
	int error;

	if (!mac_enforce_network)
		return (0);

	MAC_CHECK(check_bpfdesc_receive, bpf_d, bpf_d->bd_label, ifnet,
	    ifnet->if_label);

	return (error);
}

int
mac_check_ifnet_transmit(struct ifnet *ifnet, struct mbuf *mbuf)
{
	struct label *label;
	int error;

	M_ASSERTPKTHDR(mbuf);

	if (!mac_enforce_network)
		return (0);

	label = mbuf_to_label(mbuf);

	MAC_CHECK(check_ifnet_transmit, ifnet, ifnet->if_label, mbuf,
	    label);

	return (error);
}

int
mac_check_socket_bind(struct ucred *ucred, struct socket *socket,
    struct sockaddr *sockaddr)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_bind, ucred, socket, socket->so_label,
	    sockaddr);

	return (error);
}

int
mac_check_socket_connect(struct ucred *cred, struct socket *socket,
    struct sockaddr *sockaddr)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_connect, cred, socket, socket->so_label,
	    sockaddr);

	return (error);
}

int
mac_check_socket_deliver(struct socket *socket, struct mbuf *mbuf)
{
	struct label *label;
	int error;

	if (!mac_enforce_socket)
		return (0);

	label = mbuf_to_label(mbuf);

	MAC_CHECK(check_socket_deliver, socket, socket->so_label, mbuf,
	    label);

	return (error);
}

int
mac_check_socket_listen(struct ucred *cred, struct socket *socket)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_listen, cred, socket, socket->so_label);
	return (error);
}

int
mac_check_socket_receive(struct ucred *cred, struct socket *so)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_receive, cred, so, so->so_label);

	return (error);
}

static int
mac_check_socket_relabel(struct ucred *cred, struct socket *socket,
    struct label *newlabel)
{
	int error;

	MAC_CHECK(check_socket_relabel, cred, socket, socket->so_label,
	    newlabel);

	return (error);
}

int
mac_check_socket_send(struct ucred *cred, struct socket *so)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_send, cred, so, so->so_label);

	return (error);
}

int
mac_check_socket_visible(struct ucred *cred, struct socket *socket)
{
	int error;

	if (!mac_enforce_socket)
		return (0);

	MAC_CHECK(check_socket_visible, cred, socket, socket->so_label);

	return (error);
}

int
mac_ioctl_ifnet_get(struct ucred *cred, struct ifreq *ifr,
    struct ifnet *ifnet)
{
	char *elements, *buffer;
	struct mac mac;
	int error;

	error = copyin(ifr->ifr_ifru.ifru_data, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	elements = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, elements, mac.m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_ifnet_label(ifnet->if_label, elements,
	    buffer, mac.m_buflen);
	if (error == 0)
		error = copyout(buffer, mac.m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

int
mac_ioctl_ifnet_set(struct ucred *cred, struct ifreq *ifr,
    struct ifnet *ifnet)
{
	struct label *intlabel;
	struct mac mac;
	char *buffer;
	int error;

	error = copyin(ifr->ifr_ifru.ifru_data, &mac, sizeof(mac));
	if (error)
		return (error);

	error = mac_check_structmac_consistent(&mac);
	if (error)
		return (error);

	buffer = malloc(mac.m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac.m_string, buffer, mac.m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	intlabel = mac_ifnet_label_alloc();
	error = mac_internalize_ifnet_label(intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error) {
		mac_ifnet_label_free(intlabel);
		return (error);
	}

	/*
	 * XXX: Note that this is a redundant privilege check, since
	 * policies impose this check themselves if required by the
	 * policy.  Eventually, this should go away.
	 */
	error = suser_cred(cred, 0);
	if (error) {
		mac_ifnet_label_free(intlabel);
		return (error);
	}

	MAC_CHECK(check_ifnet_relabel, cred, ifnet, ifnet->if_label,
	    intlabel);
	if (error) {
		mac_ifnet_label_free(intlabel);
		return (error);
	}

	MAC_PERFORM(relabel_ifnet, cred, ifnet, ifnet->if_label, intlabel);

	mac_ifnet_label_free(intlabel);
	return (0);
}

int
mac_socket_label_set(struct ucred *cred, struct socket *so,
    struct label *label)
{
	int error;

	error = mac_check_socket_relabel(cred, so, label);
	if (error)
		return (error);

	mac_relabel_socket(cred, so, label);
	return (0);
}

int
mac_setsockopt_label(struct ucred *cred, struct socket *so, struct mac *mac)
{
	struct label *intlabel;
	char *buffer;
	int error;

	error = mac_check_structmac_consistent(mac);
	if (error)
		return (error);

	buffer = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac->m_string, buffer, mac->m_buflen, NULL);
	if (error) {
		free(buffer, M_MACTEMP);
		return (error);
	}

	intlabel = mac_socket_label_alloc(M_WAITOK);
	error = mac_internalize_socket_label(intlabel, buffer);
	free(buffer, M_MACTEMP);
	if (error)
		goto out;

	/* XXX: Socket lock here. */
	error = mac_socket_label_set(cred, so, intlabel);
	/* XXX: Socket unlock here. */
out:
	mac_socket_label_free(intlabel);
	return (error);
}

int
mac_getsockopt_label(struct ucred *cred, struct socket *so, struct mac *mac)
{
	char *buffer, *elements;
	int error;

	error = mac_check_structmac_consistent(mac);
	if (error)
		return (error);

	elements = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac->m_string, elements, mac->m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_socket_label(so->so_label, elements,
	    buffer, mac->m_buflen);
	if (error == 0)
		error = copyout(buffer, mac->m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}

int
mac_getsockopt_peerlabel(struct ucred *cred, struct socket *so,
    struct mac *mac)
{
	char *elements, *buffer;
	int error;

	error = mac_check_structmac_consistent(mac);
	if (error)
		return (error);

	elements = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK);
	error = copyinstr(mac->m_string, elements, mac->m_buflen, NULL);
	if (error) {
		free(elements, M_MACTEMP);
		return (error);
	}

	buffer = malloc(mac->m_buflen, M_MACTEMP, M_WAITOK | M_ZERO);
	error = mac_externalize_socket_peer_label(so->so_peerlabel,
	    elements, buffer, mac->m_buflen);
	if (error == 0)
		error = copyout(buffer, mac->m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}
