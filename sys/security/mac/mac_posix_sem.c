/*-
 * Copyright (c) 2003-2006 SPARTA, Inc.
 * All rights reserved.
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
#include "opt_posix.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ksem.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>

static struct label *
mac_posixsem_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(M_WAITOK);
	MAC_PERFORM(posixsem_init_label, label);
	return (label);
}

void
mac_posixsem_init(struct ksem *ks)
{

	ks->ks_label = mac_posixsem_label_alloc();
}

static void
mac_posixsem_label_free(struct label *label)
{

	MAC_PERFORM(posixsem_destroy_label, label);
	mac_labelzone_free(label);
}

void
mac_posixsem_destroy(struct ksem *ks)
{

	mac_posixsem_label_free(ks->ks_label);
	ks->ks_label = NULL;
}

void
mac_posixsem_create(struct ucred *cred, struct ksem *ks)
{

	MAC_PERFORM(posixsem_create, cred, ks, ks->ks_label);
}

int
mac_posixsem_check_destroy(struct ucred *cred, struct ksem *ks)
{
	int error;

	MAC_CHECK(posixsem_check_destroy, cred, ks, ks->ks_label);

	return (error);
}

int
mac_posixsem_check_open(struct ucred *cred, struct ksem *ks)
{
	int error;

	MAC_CHECK(posixsem_check_open, cred, ks, ks->ks_label);

	return (error);
}

int
mac_posixsem_check_getvalue(struct ucred *cred, struct ksem *ks)
{
	int error;

	MAC_CHECK(posixsem_check_getvalue, cred, ks, ks->ks_label);

	return (error);
}

int
mac_posixsem_check_post(struct ucred *cred, struct ksem *ks)
{
	int error;

	MAC_CHECK(posixsem_check_post, cred, ks, ks->ks_label);

	return (error);
}

int
mac_posixsem_check_unlink(struct ucred *cred, struct ksem *ks)
{
	int error;

	MAC_CHECK(posixsem_check_unlink, cred, ks, ks->ks_label);

	return (error);
}

int
mac_posixsem_check_wait(struct ucred *cred, struct ksem *ks)
{
	int error;

	MAC_CHECK(posixsem_check_wait, cred, ks, ks->ks_label);

	return (error);
}
