/*-
 * Copyright (c) 2002, 2003 Networks Associates Technology, Inc.
 * All rights reserved.
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
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/mac.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>

#include <sys/mac_policy.h>

#include <security/mac/mac_internal.h>

static int	mac_enforce_kld = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_kld, CTLFLAG_RW,
    &mac_enforce_kld, 0, "Enforce MAC policy on kld operations");
TUNABLE_INT("security.mac.enforce_kld", &mac_enforce_kld);

static int	mac_enforce_system = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_system, CTLFLAG_RW,
    &mac_enforce_system, 0, "Enforce MAC policy on system operations");
TUNABLE_INT("security.mac.enforce_system", &mac_enforce_system);

int
mac_check_kenv_dump(struct ucred *cred)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_kenv_dump, cred);

	return (error);
}

int
mac_check_kenv_get(struct ucred *cred, char *name)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_kenv_get, cred, name);

	return (error);
}

int
mac_check_kenv_set(struct ucred *cred, char *name, char *value)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_kenv_set, cred, name, value);

	return (error);
}

int
mac_check_kenv_unset(struct ucred *cred, char *name)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_kenv_unset, cred, name);

	return (error);
}

int
mac_check_kld_load(struct ucred *cred, struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_kld_load");

	if (!mac_enforce_kld)
		return (0);

	MAC_CHECK(check_kld_load, cred, vp, vp->v_label);

	return (error);
}

int
mac_check_kld_stat(struct ucred *cred)
{
	int error;

	if (!mac_enforce_kld)
		return (0);

	MAC_CHECK(check_kld_stat, cred);

	return (error);
}

int
mac_check_kld_unload(struct ucred *cred)
{
	int error;

	if (!mac_enforce_kld)
		return (0);

	MAC_CHECK(check_kld_unload, cred);

	return (error);
}

int
mac_check_sysarch_ioperm(struct ucred *cred)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_sysarch_ioperm, cred);
	return (error);
}

int
mac_check_system_acct(struct ucred *cred, struct vnode *vp)
{
	int error;

	if (vp != NULL) {
		ASSERT_VOP_LOCKED(vp, "mac_check_system_acct");
	}

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_acct, cred, vp,
	    vp != NULL ? vp->v_label : NULL);

	return (error);
}

int
mac_check_system_nfsd(struct ucred *cred)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_nfsd, cred);

	return (error);
}

int
mac_check_system_reboot(struct ucred *cred, int howto)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_reboot, cred, howto);

	return (error);
}

int
mac_check_system_settime(struct ucred *cred)
{
	int error;

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_settime, cred);

	return (error);
}

int
mac_check_system_swapon(struct ucred *cred, struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_system_swapon");

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_swapon, cred, vp, vp->v_label);
	return (error);
}

int
mac_check_system_swapoff(struct ucred *cred, struct vnode *vp)
{
	int error;

	ASSERT_VOP_LOCKED(vp, "mac_check_system_swapoff");

	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_swapoff, cred, vp, vp->v_label);
	return (error);
}

int
mac_check_system_sysctl(struct ucred *cred, struct sysctl_oid *oidp, void *arg1,
    int arg2, struct sysctl_req *req)
{
	int error;

	/*
	 * XXXMAC: We're very much like to assert the SYSCTL_LOCK here,
	 * but since it's not exported from kern_sysctl.c, we can't.
	 */
	if (!mac_enforce_system)
		return (0);

	MAC_CHECK(check_system_sysctl, cred, oidp, arg1, arg2, req);

	return (error);
}
