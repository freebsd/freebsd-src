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

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_var.h>

#include <security/mac/mac_internal.h>

/*
 * mac_enforce_network is used by IPv4 and IPv6 checks, and so must
 * be non-static for now.
 */
int	mac_enforce_network = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_network, CTLFLAG_RW,
    &mac_enforce_network, 0, "Enforce MAC policy on network packets");
TUNABLE_INT("security.mac.enforce_network", &mac_enforce_network);

#ifdef MAC_DEBUG
static unsigned int nmacbpfdescs, nmacifnets, nmacmbufs;

SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, bpfdescs, CTLFLAG_RD,
    &nmacbpfdescs, 0, "number of bpfdescs in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, ifnets, CTLFLAG_RD,
    &nmacifnets, 0, "number of ifnets in use");
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, mbufs, CTLFLAG_RD,
    &nmacmbufs, 0, "number of mbufs in use");
#endif

/*
 * XXXRW: struct ifnet locking is incomplete in the network code, so we
 * use our own global mutex for struct ifnet.  Non-ideal, but should help
 * in the SMP environment.
 */
static struct mtx mac_ifnet_mtx;
MTX_SYSINIT(mac_ifnet_mtx, &mac_ifnet_mtx, "mac_ifnet", MTX_DEF);
#define	MAC_IFNET_LOCK(ifp)	mtx_lock(&mac_ifnet_mtx)
#define	MAC_IFNET_UNLOCK(ifp)	mtx_unlock(&mac_ifnet_mtx)

struct label *
mac_mbuf_to_label(struct mbuf *mbuf)
{
	struct m_tag *tag;
	struct label *label;

	if (mbuf == NULL)
		return (NULL);
	tag = m_tag_find(mbuf, PACKET_TAG_MACLABEL, NULL);
	if (tag == NULL)
		return (NULL);
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

void
mac_destroy_mbuf_tag(struct m_tag *tag)
{
	struct label *label;

	label = (struct label *)(tag+1);

	MAC_PERFORM(destroy_mbuf_label, label);
	mac_destroy_label(label);
	MAC_DEBUG_COUNTER_DEC(&nmacmbufs);
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

void
mac_copy_mbuf(struct mbuf *m_from, struct mbuf *m_to)
{
	struct label *src_label, *dest_label;

	src_label = mac_mbuf_to_label(m_from);
	dest_label = mac_mbuf_to_label(m_to);

	MAC_PERFORM(copy_mbuf_label, src_label, dest_label);
}

static void
mac_copy_ifnet_label(struct label *src, struct label *dest)
{

	MAC_PERFORM(copy_ifnet_label, src, dest);
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
mac_internalize_ifnet_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(ifnet, label, string);

	return (error);
}

void
mac_create_ifnet(struct ifnet *ifnet)
{

	MAC_IFNET_LOCK(ifnet);
	MAC_PERFORM(create_ifnet, ifnet, ifnet->if_label);
	MAC_IFNET_UNLOCK(ifnet);
}

void
mac_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d)
{

	MAC_PERFORM(create_bpfdesc, cred, bpf_d, bpf_d->bd_label);
}

void
mac_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct mbuf *mbuf)
{
	struct label *label;

	BPFD_LOCK_ASSERT(bpf_d);

	label = mac_mbuf_to_label(mbuf);

	MAC_PERFORM(create_mbuf_from_bpfdesc, bpf_d, bpf_d->bd_label, mbuf,
	    label);
}

void
mac_create_mbuf_linklayer(struct ifnet *ifnet, struct mbuf *mbuf)
{
	struct label *label;

	label = mac_mbuf_to_label(mbuf);

	MAC_IFNET_LOCK(ifnet);
	MAC_PERFORM(create_mbuf_linklayer, ifnet, ifnet->if_label, mbuf,
	    label);
	MAC_IFNET_UNLOCK(ifnet);
}

void
mac_create_mbuf_from_ifnet(struct ifnet *ifnet, struct mbuf *mbuf)
{
	struct label *label;

	label = mac_mbuf_to_label(mbuf);

	MAC_IFNET_LOCK(ifnet);
	MAC_PERFORM(create_mbuf_from_ifnet, ifnet, ifnet->if_label, mbuf,
	    label);
	MAC_IFNET_UNLOCK(ifnet);
}

void
mac_create_mbuf_multicast_encap(struct mbuf *oldmbuf, struct ifnet *ifnet,
    struct mbuf *newmbuf)
{
	struct label *oldmbuflabel, *newmbuflabel;

	oldmbuflabel = mac_mbuf_to_label(oldmbuf);
	newmbuflabel = mac_mbuf_to_label(newmbuf);

	MAC_IFNET_LOCK(ifnet);
	MAC_PERFORM(create_mbuf_multicast_encap, oldmbuf, oldmbuflabel,
	    ifnet, ifnet->if_label, newmbuf, newmbuflabel);
	MAC_IFNET_UNLOCK(ifnet);
}

void
mac_create_mbuf_netlayer(struct mbuf *oldmbuf, struct mbuf *newmbuf)
{
	struct label *oldmbuflabel, *newmbuflabel;

	oldmbuflabel = mac_mbuf_to_label(oldmbuf);
	newmbuflabel = mac_mbuf_to_label(newmbuf);

	MAC_PERFORM(create_mbuf_netlayer, oldmbuf, oldmbuflabel, newmbuf,
	    newmbuflabel);
}

int
mac_check_bpfdesc_receive(struct bpf_d *bpf_d, struct ifnet *ifnet)
{
	int error;

	BPFD_LOCK_ASSERT(bpf_d);

	if (!mac_enforce_network)
		return (0);

	MAC_IFNET_LOCK(ifnet);
	MAC_CHECK(check_bpfdesc_receive, bpf_d, bpf_d->bd_label, ifnet,
	    ifnet->if_label);
	MAC_IFNET_UNLOCK(ifnet);

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

	label = mac_mbuf_to_label(mbuf);

	MAC_IFNET_LOCK(ifnet);
	MAC_CHECK(check_ifnet_transmit, ifnet, ifnet->if_label, mbuf,
	    label);
	MAC_IFNET_UNLOCK(ifnet);

	return (error);
}

int
mac_ioctl_ifnet_get(struct ucred *cred, struct ifreq *ifr,
    struct ifnet *ifnet)
{
	char *elements, *buffer;
	struct label *intlabel;
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
	intlabel = mac_ifnet_label_alloc();
	MAC_IFNET_LOCK(ifnet);
	mac_copy_ifnet_label(ifnet->if_label, intlabel);
	MAC_IFNET_UNLOCK(ifnet);
	error = mac_externalize_ifnet_label(ifnet->if_label, elements,
	    buffer, mac.m_buflen);
	mac_ifnet_label_free(intlabel);
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

	MAC_IFNET_LOCK(ifnet);
	MAC_CHECK(check_ifnet_relabel, cred, ifnet, ifnet->if_label,
	    intlabel);
	if (error) {
		MAC_IFNET_UNLOCK(ifnet);
		mac_ifnet_label_free(intlabel);
		return (error);
	}

	MAC_PERFORM(relabel_ifnet, cred, ifnet, ifnet->if_label, intlabel);
	MAC_IFNET_UNLOCK(ifnet);

	mac_ifnet_label_free(intlabel);
	return (0);
}
