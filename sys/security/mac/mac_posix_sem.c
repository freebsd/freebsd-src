/*-
 * Copyright (c) 2003-2005 SPARTA, Inc.
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
#include "opt_posix.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mac.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <posix4/ksem.h>

#include <sys/mac_policy.h>

#include <security/mac/mac_internal.h>

static int	mac_enforce_posix_sem = 1;
SYSCTL_INT(_security_mac, OID_AUTO, enforce_posix_sem, CTLFLAG_RW,
    &mac_enforce_posix_sem, 0, "Enforce MAC policy on global POSIX semaphores");
TUNABLE_INT("security.mac.enforce_posix_sem", &mac_enforce_posix_sem);

#ifdef MAC_DEBUG
static unsigned int nmacposixsems;
SYSCTL_UINT(_security_mac_debug_counters, OID_AUTO, posix_sems, CTLFLAG_RD,
    &nmacposixsems, 0, "number of posix global semaphores inuse");
#endif

static struct label *
mac_posix_sem_label_alloc(void)
{
	struct label *label;

	label = mac_labelzone_alloc(M_WAITOK);
	MAC_PERFORM(init_posix_sem_label, label);
	MAC_DEBUG_COUNTER_INC(&nmacposixsems);
	return (label);
}

void 
mac_init_posix_sem(struct ksem *ksemptr)
{

	ksemptr->ks_label = mac_posix_sem_label_alloc();
}

static void
mac_posix_sem_label_free(struct label *label)
{

	MAC_PERFORM(destroy_posix_sem_label, label);
	MAC_DEBUG_COUNTER_DEC(&nmacposixsems);
}

void
mac_destroy_posix_sem(struct ksem *ksemptr)
{

	mac_posix_sem_label_free(ksemptr->ks_label);
	ksemptr->ks_label = NULL;
}

void 
mac_create_posix_sem(struct ucred *cred, struct ksem *ksemptr)
{

	MAC_PERFORM(create_posix_sem, cred, ksemptr, ksemptr->ks_label);
}

int
mac_check_posix_sem_destroy(struct ucred *cred, struct ksem *ksemptr)
{
	int error;

	if (!mac_enforce_posix_sem)
		return (0);

	MAC_CHECK(check_posix_sem_destroy, cred, ksemptr, ksemptr->ks_label);

	return(error);
}

int
mac_check_posix_sem_open(struct ucred *cred, struct ksem *ksemptr)
{
	int error;

	if (!mac_enforce_posix_sem)
		return (0);

	MAC_CHECK(check_posix_sem_open, cred, ksemptr, ksemptr->ks_label);

	return(error);
}

int
mac_check_posix_sem_getvalue(struct ucred *cred, struct ksem *ksemptr)
{
	int error;

	if (!mac_enforce_posix_sem)
		return (0);

	MAC_CHECK(check_posix_sem_getvalue, cred, ksemptr,
	    ksemptr->ks_label);

	return(error);
}

int
mac_check_posix_sem_post(struct ucred *cred, struct ksem *ksemptr)
{
	int error;

	if (!mac_enforce_posix_sem)
		return (0);

	MAC_CHECK(check_posix_sem_post, cred, ksemptr, ksemptr->ks_label);

	return(error);
}

int
mac_check_posix_sem_unlink(struct ucred *cred, struct ksem *ksemptr)
{
	int error;

	if (!mac_enforce_posix_sem)
		return (0);

	MAC_CHECK(check_posix_sem_unlink, cred, ksemptr, ksemptr->ks_label);

	return(error);
}

int
mac_check_posix_sem_wait(struct ucred *cred, struct ksem *ksemptr)
{
	int error;

	if (!mac_enforce_posix_sem)
		return (0);

	MAC_CHECK(check_posix_sem_wait, cred, ksemptr, ksemptr->ks_label);

	return(error);
}
