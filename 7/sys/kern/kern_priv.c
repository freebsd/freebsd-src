/*-
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * Copyright (c) 2009 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR, NCIRCLE NETWORK SECURITY,
 * INC., OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_kdtrace.h"
#include "opt_mac.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <security/mac/mac_framework.h>

/*
 * `suser_enabled' (which can be set by the security.bsd.suser_enabled
 * sysctl) determines whether the system 'super-user' policy is in effect.  If
 * it is nonzero, an effective uid of 0 connotes special privilege,
 * overriding many mandatory and discretionary protections.  If it is zero,
 * uid 0 is offered no special privilege in the kernel security policy.
 * Setting it to zero may seriously impact the functionality of many existing
 * userland programs, and should not be done without careful consideration of
 * the consequences. 
 */
static int	suser_enabled = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, suser_enabled, CTLFLAG_RW,
    &suser_enabled, 0, "processes with uid 0 have privilege");
TUNABLE_INT("security.bsd.suser_enabled", &suser_enabled);

SDT_PROVIDER_DEFINE(priv);
SDT_PROBE_DEFINE1(priv, kernel, priv_check, priv_ok, "int");
SDT_PROBE_DEFINE1(priv, kernel, priv_check, priv_err, "int");

/*
 * Check a credential for privilege.  Lots of good reasons to deny privilege;
 * only a few to grant it.
 */
int
priv_check_cred(struct ucred *cred, int priv, int flags)
{
	int error;

	KASSERT(PRIV_VALID(priv), ("priv_check_cred: invalid privilege %d",
	    priv));

	/*
	 * We first evaluate policies that may deny the granting of
	 * privilege unilaterally.
	 */
#ifdef MAC
	error = mac_priv_check(cred, priv);
	if (error)
		goto out;
#endif

	/*
	 * Jail policy will restrict certain privileges that may otherwise be
	 * be granted.
	 */
	error = prison_priv_check(cred, priv);
	if (error)
		goto out;

	/*
	 * Having determined if privilege is restricted by various policies,
	 * now determine if privilege is granted.  At this point, any policy
	 * may grant privilege.  For now, we allow short-circuit boolean
	 * evaluation, so may not call all policies.  Perhaps we should.
	 *
	 * Superuser policy grants privilege based on the effective (or in
	 * the case of specific privileges, real) uid being 0.  We allow the
	 * superuser policy to be globally disabled, although this is
	 * currenty of limited utility.
	 */
	if (suser_enabled) {
		switch (priv) {
		case PRIV_MAXFILES:
		case PRIV_MAXPROC:
		case PRIV_PROC_LIMIT:
			if (cred->cr_ruid == 0) {
				error = 0;
				goto out;
			}
			break;

		default:
			if (cred->cr_uid == 0) {
				error = 0;
				goto out;
			}
			break;
		}
	}

	/*
	 * Now check with MAC, if enabled, to see if a policy module grants
	 * privilege.
	 */
#ifdef MAC
	if (mac_priv_grant(cred, priv) == 0) {
		error = 0;
		goto out;
	}
#endif

	/*
	 * The default is deny, so if no policies have granted it, reject
	 * with a privilege error here.
	 */
	error = EPERM;
out:
	if (error) {
		SDT_PROBE(priv, kernel, priv_check, priv_err, priv, 0, 0, 0,
		    0);
	} else {
		SDT_PROBE(priv, kernel, priv_check, priv_ok, priv, 0, 0, 0,
		    0);
	}
	return (error);
}

int
priv_check(struct thread *td, int priv)
{

	KASSERT(td == curthread, ("priv_check: td != curthread"));

	return (priv_check_cred(td->td_ucred, priv, 0));
}

/*
 * Historical suser() wrapper functions, which now simply request PRIV_ROOT.
 * These will be removed in the near future, and exist solely because
 * the kernel and modules are not yet fully adapted to the new model.
 */
int
suser_cred(struct ucred *cred, int flags)
{

	return (priv_check_cred(cred, PRIV_ROOT, flags));
}

int
suser(struct thread *td)
{

	KASSERT(td == curthread, ("suser: td != curthread"));

	return (suser_cred(td->td_ucred, 0));
}
