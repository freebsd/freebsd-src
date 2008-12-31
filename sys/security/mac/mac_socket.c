/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001-2005 Networks Associates Technology, Inc.
 * Copyright (c) 2005 SPARTA, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson and Ilmar Habibulin for the
 * TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by McAfee
 * Research, the Technology Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * This software was enhanced by SPARTA ISSO under SPAWAR contract
 * N66001-04-C-6019 ("SEFOS").
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
__FBSDID("$FreeBSD: src/sys/security/mac/mac_socket.c,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $");

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

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>

/*
 * Currently, sockets hold two labels: the label of the socket itself, and a
 * peer label, which may be used by policies to hold a copy of the label of
 * any remote endpoint.
 *
 * Possibly, this peer label should be maintained at the protocol layer
 * (inpcb, unpcb, etc), as this would allow protocol-aware code to maintain
 * the label consistently.  For example, it might be copied live from a
 * remote socket for UNIX domain sockets rather than keeping a local copy on
 * this endpoint, but be cached and updated based on packets received for
 * TCP/IP.
 */

struct label *
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

void
mac_socket_label_free(struct label *label)
{

	MAC_PERFORM(destroy_socket_label, label);
	mac_labelzone_free(label);
}

static void
mac_socket_peer_label_free(struct label *label)
{

	MAC_PERFORM(destroy_socket_peer_label, label);
	mac_labelzone_free(label);
}

void
mac_destroy_socket(struct socket *so)
{

	mac_socket_label_free(so->so_label);
	so->so_label = NULL;
	mac_socket_peer_label_free(so->so_peerlabel);
	so->so_peerlabel = NULL;
}

void
mac_copy_socket_label(struct label *src, struct label *dest)
{

	MAC_PERFORM(copy_socket_label, src, dest);
}

int
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

int
mac_internalize_socket_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(socket, label, string);

	return (error);
}

void
mac_create_socket(struct ucred *cred, struct socket *so)
{

	MAC_PERFORM(create_socket, cred, so, so->so_label);
}

void
mac_create_socket_from_socket(struct socket *oldso, struct socket *newso)
{

	SOCK_LOCK_ASSERT(oldso);

	MAC_PERFORM(create_socket_from_socket, oldso, oldso->so_label, newso,
	    newso->so_label);
}

static void
mac_relabel_socket(struct ucred *cred, struct socket *so,
    struct label *newlabel)
{

	SOCK_LOCK_ASSERT(so);

	MAC_PERFORM(relabel_socket, cred, so, so->so_label, newlabel);
}

void
mac_set_socket_peer_from_mbuf(struct mbuf *m, struct socket *so)
{
	struct label *label;

	SOCK_LOCK_ASSERT(so);

	label = mac_mbuf_to_label(m);

	MAC_PERFORM(set_socket_peer_from_mbuf, m, label, so,
	    so->so_peerlabel);
}

void
mac_set_socket_peer_from_socket(struct socket *oldso, struct socket *newso)
{

	/*
	 * XXXRW: only hold the socket lock on one at a time, as one socket
	 * is the original, and one is the new.  However, it's called in both
	 * directions, so we can't assert the lock here currently.
	 */
	MAC_PERFORM(set_socket_peer_from_socket, oldso, oldso->so_label,
	    newso, newso->so_peerlabel);
}

void
mac_create_mbuf_from_socket(struct socket *so, struct mbuf *m)
{
	struct label *label;

	SOCK_LOCK_ASSERT(so);

	label = mac_mbuf_to_label(m);

	MAC_PERFORM(create_mbuf_from_socket, so, so->so_label, m, label);
}

int
mac_check_socket_accept(struct ucred *cred, struct socket *so)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_accept, cred, so, so->so_label);

	return (error);
}

int
mac_check_socket_bind(struct ucred *ucred, struct socket *so,
    struct sockaddr *sa)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_bind, ucred, so, so->so_label, sa);

	return (error);
}

int
mac_check_socket_connect(struct ucred *cred, struct socket *so,
    struct sockaddr *sa)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_connect, cred, so, so->so_label, sa);

	return (error);
}

int
mac_check_socket_create(struct ucred *cred, int domain, int type, int proto)
{
	int error;

	MAC_CHECK(check_socket_create, cred, domain, type, proto);

	return (error);
}

int
mac_check_socket_deliver(struct socket *so, struct mbuf *m)
{
	struct label *label;
	int error;

	SOCK_LOCK_ASSERT(so);

	label = mac_mbuf_to_label(m);

	MAC_CHECK(check_socket_deliver, so, so->so_label, m, label);

	return (error);
}

int
mac_check_socket_listen(struct ucred *cred, struct socket *so)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_listen, cred, so, so->so_label);

	return (error);
}

int
mac_check_socket_poll(struct ucred *cred, struct socket *so)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_poll, cred, so, so->so_label);

	return (error);
}

int
mac_check_socket_receive(struct ucred *cred, struct socket *so)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_receive, cred, so, so->so_label);

	return (error);
}

static int
mac_check_socket_relabel(struct ucred *cred, struct socket *so,
    struct label *newlabel)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_relabel, cred, so, so->so_label, newlabel);

	return (error);
}

int
mac_check_socket_send(struct ucred *cred, struct socket *so)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_send, cred, so, so->so_label);

	return (error);
}

int
mac_check_socket_stat(struct ucred *cred, struct socket *so)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_stat, cred, so, so->so_label);

	return (error);
}

int
mac_check_socket_visible(struct ucred *cred, struct socket *so)
{
	int error;

	SOCK_LOCK_ASSERT(so);

	MAC_CHECK(check_socket_visible, cred, so, so->so_label);

	return (error);
}

int
mac_socket_label_set(struct ucred *cred, struct socket *so,
    struct label *label)
{
	int error;

	/*
	 * We acquire the socket lock when we perform the test and set, but
	 * have to release it as the pcb code needs to acquire the pcb lock,
	 * which will precede the socket lock in the lock order.  However,
	 * this is fine, as any race will simply result in the inpcb being
	 * refreshed twice, but still consistently, as the inpcb code will
	 * acquire the socket lock before refreshing, holding both locks.
	 */
	SOCK_LOCK(so);
	error = mac_check_socket_relabel(cred, so, label);
	if (error) {
		SOCK_UNLOCK(so);
		return (error);
	}

	mac_relabel_socket(cred, so, label);
	SOCK_UNLOCK(so);

	/*
	 * If the protocol has expressed interest in socket layer changes,
	 * such as if it needs to propagate changes to a cached pcb label
	 * from the socket, notify it of the label change while holding the
	 * socket lock.
	 */
	if (so->so_proto->pr_usrreqs->pru_sosetlabel != NULL)
		(so->so_proto->pr_usrreqs->pru_sosetlabel)(so);

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

	error = mac_socket_label_set(cred, so, intlabel);
out:
	mac_socket_label_free(intlabel);
	return (error);
}

int
mac_getsockopt_label(struct ucred *cred, struct socket *so, struct mac *mac)
{
	char *buffer, *elements;
	struct label *intlabel;
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
	intlabel = mac_socket_label_alloc(M_WAITOK);
	SOCK_LOCK(so);
	mac_copy_socket_label(so->so_label, intlabel);
	SOCK_UNLOCK(so);
	error = mac_externalize_socket_label(intlabel, elements, buffer,
	    mac->m_buflen);
	mac_socket_label_free(intlabel);
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
	struct label *intlabel;
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
	intlabel = mac_socket_label_alloc(M_WAITOK);
	SOCK_LOCK(so);
	mac_copy_socket_label(so->so_peerlabel, intlabel);
	SOCK_UNLOCK(so);
	error = mac_externalize_socket_peer_label(intlabel, elements, buffer,
	    mac->m_buflen);
	mac_socket_label_free(intlabel);
	if (error == 0)
		error = copyout(buffer, mac->m_string, strlen(buffer)+1);

	free(buffer, M_MACTEMP);
	free(elements, M_MACTEMP);

	return (error);
}
