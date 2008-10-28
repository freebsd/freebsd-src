/*-
 * Copyright (c) 1999-2002, 2008 Robert N. M. Watson
 * Copyright (c) 2001 Ilmar S. Habibulin
 * Copyright (c) 2001-2003 Networks Associates Technology, Inc.
 * Copyright (c) 2005 Samy Al Bahra
 * Copyright (c) 2006 SPARTA, Inc.
 * Copyright (c) 2008 Apple Inc.
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
__FBSDID("$FreeBSD$");

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>

struct label *
mac_cred_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(M_WAITOK);
	MAC_PERFORM(cred_init_label, label);
	return (label);
}

void
mac_cred_init(struct ucred *cred)
{

	if (mac_labeled & MPC_OBJECT_CRED)
		cred->cr_label = mac_cred_label_alloc();
	else
		cred->cr_label = NULL;
}

void
mac_cred_label_free(struct label *label)
{

	MAC_PERFORM(cred_destroy_label, label);
	mac_labelzone_free(label);
}

void
mac_cred_destroy(struct ucred *cred)
{

	if (cred->cr_label != NULL) {
		mac_cred_label_free(cred->cr_label);
		cred->cr_label = NULL;
	}
}

/*
 * When a thread becomes an NFS server daemon, its credential may need to be
 * updated to reflect this so that policies can recognize when file system
 * operations originate from the network.
 *
 * At some point, it would be desirable if the credential used for each NFS
 * RPC could be set based on the RPC context (i.e., source system, etc) to
 * provide more fine-grained access control.
 */
void
mac_cred_associate_nfsd(struct ucred *cred)
{

	MAC_PERFORM(cred_associate_nfsd, cred);
}

/*
 * Initialize MAC label for the first kernel process, from which other kernel
 * processes and threads are spawned.
 */
void
mac_cred_create_swapper(struct ucred *cred)
{

	MAC_PERFORM(cred_create_swapper, cred);
}

/*
 * Initialize MAC label for the first userland process, from which other
 * userland processes and threads are spawned.
 */
void
mac_cred_create_init(struct ucred *cred)
{

	MAC_PERFORM(cred_create_init, cred);
}

int
mac_cred_externalize_label(struct label *label, char *elements,
    char *outbuf, size_t outbuflen)
{
	int error;

	MAC_EXTERNALIZE(cred, label, elements, outbuf, outbuflen);

	return (error);
}

int
mac_cred_internalize_label(struct label *label, char *string)
{
	int error;

	MAC_INTERNALIZE(cred, label, string);

	return (error);
}

/*
 * When a new process is created, its label must be initialized.  Generally,
 * this involves inheritence from the parent process, modulo possible deltas.
 * This function allows that processing to take place.
 */
void
mac_cred_copy(struct ucred *src, struct ucred *dest)
{

	MAC_PERFORM(cred_copy_label, src->cr_label, dest->cr_label);
}

/*
 * When the subject's label changes, it may require revocation of privilege
 * to mapped objects.  This can't be done on-the-fly later with a unified
 * buffer cache.
 */
void
mac_cred_relabel(struct ucred *cred, struct label *newlabel)
{

	MAC_PERFORM(cred_relabel, cred, newlabel);
}

int
mac_cred_check_relabel(struct ucred *cred, struct label *newlabel)
{
	int error;

	MAC_CHECK(cred_check_relabel, cred, newlabel);

	return (error);
}

int
mac_cred_check_visible(struct ucred *cr1, struct ucred *cr2)
{
	int error;

	MAC_CHECK(cred_check_visible, cr1, cr2);

	return (error);
}
