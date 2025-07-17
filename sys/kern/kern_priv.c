/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * Copyright (c) 2009 Robert N. M. Watson
 * Copyright (c) 2020 Mariusz Zaborski <oshogbo@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
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

static bool
suser_enabled(struct ucred *cred)
{

	return (prison_allow(cred, PR_ALLOW_SUSER));
}

static int
sysctl_kern_suser_enabled(SYSCTL_HANDLER_ARGS)
{
	struct ucred *cred;
	int error, enabled;

	cred = req->td->td_ucred;
	enabled = suser_enabled(cred);
	error = sysctl_handle_int(oidp, &enabled, 0, req);
	if (error || !req->newptr)
		return (error);
	prison_set_allow(cred, PR_ALLOW_SUSER, enabled);
	return (0);
}

SYSCTL_PROC(_security_bsd, OID_AUTO, suser_enabled, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_PRISON | CTLFLAG_MPSAFE, 0, 0,
    &sysctl_kern_suser_enabled, "I", "Processes with uid 0 have privilege");

static int	unprivileged_mlock = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, unprivileged_mlock, CTLFLAG_RWTUN,
    &unprivileged_mlock, 0, "Allow non-root users to call mlock(2)");

static int	unprivileged_read_msgbuf = 1;
SYSCTL_INT(_security_bsd, OID_AUTO, unprivileged_read_msgbuf,
    CTLFLAG_RW, &unprivileged_read_msgbuf, 0,
    "Unprivileged processes may read the kernel message buffer");

SDT_PROVIDER_DEFINE(priv);
SDT_PROBE_DEFINE1(priv, kernel, priv_check, priv__ok, "int");
SDT_PROBE_DEFINE1(priv, kernel, priv_check, priv__err, "int");

static __always_inline int
priv_check_cred_pre(struct ucred *cred, int priv)
{
	int error;

#ifdef MAC
	error = mac_priv_check(cred, priv);
#else
	error = 0;
#endif
	return (error);
}

static __always_inline int
priv_check_cred_post(struct ucred *cred, int priv, int error, bool handled)
{

	if (__predict_true(handled))
		goto out;
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
	if (SDT_PROBES_ENABLED()) {
		if (error)
			SDT_PROBE1(priv, kernel, priv_check, priv__err, priv);
		else
			SDT_PROBE1(priv, kernel, priv_check, priv__ok, priv);
	}
	return (error);
}

/*
 * Check a credential for privilege.  Lots of good reasons to deny privilege;
 * only a few to grant it.
 */
int
priv_check_cred(struct ucred *cred, int priv)
{
	int error;

	KASSERT(PRIV_VALID(priv), ("priv_check_cred: invalid privilege %d",
	    priv));

	switch (priv) {
	case PRIV_VFS_LOOKUP:
		return (priv_check_cred_vfs_lookup(cred));
	case PRIV_VFS_GENERATION:
		return (priv_check_cred_vfs_generation(cred));
	}

	/*
	 * We first evaluate policies that may deny the granting of
	 * privilege unilaterally.
	 */
	error = priv_check_cred_pre(cred, priv);
	if (error)
		goto out;

	/*
	 * Jail policy will restrict certain privileges that may otherwise be
	 * be granted.
	 */
	error = prison_priv_check(cred, priv);
	if (error)
		goto out;

	if (unprivileged_mlock) {
		/*
		 * Allow unprivileged users to call mlock(2)/munlock(2) and
		 * mlockall(2)/munlockall(2).
		 */
		switch (priv) {
		case PRIV_VM_MLOCK:
		case PRIV_VM_MUNLOCK:
			error = 0;
			goto out;
		}
	}

	if (unprivileged_read_msgbuf) {
		/*
		 * Allow an unprivileged user to read the kernel message
		 * buffer.
		 */
		if (priv == PRIV_MSGBUF) {
			error = 0;
			goto out;
		}
	}

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
	if (suser_enabled(cred)) {
		switch (priv) {
		case PRIV_MAXFILES:
		case PRIV_MAXPROC:
		case PRIV_PROC_LIMIT:
			if (cred->cr_ruid == 0) {
				error = 0;
				goto out;
			}
			break;
		case PRIV_VFS_READ_DIR:
			/*
			 * Allow PRIV_VFS_READ_DIR for root if we're not in a
			 * jail, otherwise deny unless a MAC policy grants it.
			 */
			if (jailed(cred))
				break;
			/* FALLTHROUGH */
		default:
			if (cred->cr_uid == 0) {
				error = 0;
				goto out;
			}
			break;
		}
	}

	/*
	 * Writes to kernel/physical memory are a typical root-only operation,
	 * but non-root users are expected to be able to read it (provided they
	 * have permission to access /dev/[k]mem).
	 */
	switch (priv) {
	case PRIV_KMEM_READ:
	case PRIV_PROC_MEM_WRITE:	/* we already checked candebug */
		error = 0;
		goto out;
	}

	/*
	 * Allow unprivileged process debugging on a per-jail basis.
	 * Do this here instead of prison_priv_check(), so it can also
	 * apply to prison0.
	 */
	if (priv == PRIV_DEBUG_UNPRIV) {
		if (prison_allow(cred, PR_ALLOW_UNPRIV_DEBUG)) {
			error = 0;
			goto out;
		}
	}

	return (priv_check_cred_post(cred, priv, error, false));
out:
	return (priv_check_cred_post(cred, priv, error, true));
}

int
priv_check(struct thread *td, int priv)
{

	KASSERT(td == curthread, ("priv_check: td != curthread"));

	return (priv_check_cred(td->td_ucred, priv));
}

static int __noinline
priv_check_cred_vfs_lookup_slow(struct ucred *cred)
{
	int error;

	error = priv_check_cred_pre(cred, PRIV_VFS_LOOKUP);
	if (error)
		goto out;

	if (cred->cr_uid == 0 && suser_enabled(cred)) {
		error = 0;
		goto out;
	}

	return (priv_check_cred_post(cred, PRIV_VFS_LOOKUP, error, false));
out:
	return (priv_check_cred_post(cred, PRIV_VFS_LOOKUP, error, true));

}

int
priv_check_cred_vfs_lookup(struct ucred *cred)
{
	int error;

	if (__predict_false(mac_priv_check_fp_flag ||
	    mac_priv_grant_fp_flag || SDT_PROBES_ENABLED()))
		return (priv_check_cred_vfs_lookup_slow(cred));

	error = EPERM;
	if (cred->cr_uid == 0 && suser_enabled(cred))
		error = 0;
	return (error);
}

int
priv_check_cred_vfs_lookup_nomac(struct ucred *cred)
{
	int error;

	if (__predict_false(mac_priv_check_fp_flag ||
	    mac_priv_grant_fp_flag || SDT_PROBES_ENABLED()))
		return (EAGAIN);

	error = EPERM;
	if (cred->cr_uid == 0 && suser_enabled(cred))
		error = 0;
	return (error);
}

static int __noinline
priv_check_cred_vfs_generation_slow(struct ucred *cred)
{
	int error;

	error = priv_check_cred_pre(cred, PRIV_VFS_GENERATION);
	if (error)
		goto out;

	if (jailed(cred)) {
		error = EPERM;
		goto out;
	}

	if (cred->cr_uid == 0 && suser_enabled(cred)) {
		error = 0;
		goto out;
	}

	return (priv_check_cred_post(cred, PRIV_VFS_GENERATION, error, false));
out:
	return (priv_check_cred_post(cred, PRIV_VFS_GENERATION, error, true));

}

int
priv_check_cred_vfs_generation(struct ucred *cred)
{
	int error;

	if (__predict_false(mac_priv_check_fp_flag ||
	    mac_priv_grant_fp_flag || SDT_PROBES_ENABLED()))
		return (priv_check_cred_vfs_generation_slow(cred));

	error = EPERM;
	if (!jailed(cred) && cred->cr_uid == 0 && suser_enabled(cred))
		error = 0;
	return (error);
}
