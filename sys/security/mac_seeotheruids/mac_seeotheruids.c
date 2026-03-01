/*-
 * Copyright (c) 1999-2002, 2007 Robert N. M. Watson
 * Copyright (c) 2001-2002 Networks Associates Technology, Inc.
 * Copyright (c) 2006 SPARTA, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
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

/*
 * Developed by the TrustedBSD Project.
 *
 * Prevent processes owned by a particular uid from seeing various transient
 * kernel objects associated with other uids.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/rmlock.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>

#include <security/mac/mac_policy.h>

static MALLOC_DEFINE(M_SEEOTHERUIDS, "mac_seeotheruids",
    "mac_seeotheruids(4) security module");

static SYSCTL_NODE(_security_mac, OID_AUTO, seeotheruids,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TrustedBSD mac_seeotheruids policy controls");

static int	seeotheruids_enabled = 1;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, enabled, CTLFLAG_RW,
    &seeotheruids_enabled, 0, "Enforce seeotheruids policy");

/*
 * Exception: allow credentials to be aware of other credentials with the
 * same primary gid.
 */
static int	primarygroup_enabled = 0;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, primarygroup_enabled,
    CTLFLAG_RW, &primarygroup_enabled, 0, "Make an exception for credentials "
    "with the same real primary group id");

/*
 * Exception: allow the root user to be aware of other credentials by virtue
 * of privilege.
 */
static int	suser_privileged = 1;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, suser_privileged,
    CTLFLAG_RW, &suser_privileged, 0, "Make an exception for superuser");

/*
 * Exception: allow processes with a specific gid to be exempt from the
 * policy.  One sysctl enables this functionality; the other sets the
 * exempt gid.
 */
static int	specificgid_enabled = 0;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, specificgid_enabled,
    CTLFLAG_RW, &specificgid_enabled, 0, "Make an exception for credentials "
    "with a specific gid as their real primary group id or group set");

static struct rmlock seeotheruids_rmlock;
RM_SYSINIT_FLAGS(mac_seeotheruids_lock, &seeotheruids_rmlock,
    "mac_seeotheruids_lock", RM_SLEEPABLE);

static gid_t	*specificgids;
static size_t	 specificgidcnt;

static int
gidp_cmp(const void *p1, const void *p2)
{
	const gid_t g1 = *(const gid_t *)p1;
	const gid_t g2 = *(const gid_t *)p2;

	return ((g1 > g2) - (g1 < g2));
}

static void
specificgid_normalize(gid_t *gidlist, size_t *ngidp)
{
	int ins_idx;
	gid_t prev_g;

	if (*ngidp < 2)
		return;

	qsort(gidlist, *ngidp, sizeof(*gidlist), gidp_cmp);

	prev_g = gidlist[0];
	ins_idx = 1;
	for (int i = ins_idx; i < *ngidp; ++i) {
		const gid_t g = gidlist[i];

		if (g != prev_g) {
			if (i != ins_idx)
				gidlist[ins_idx] = g;
			++ins_idx;
			prev_g = g;
		}
	}

	*ngidp = ins_idx;
}

static int
specificgid_sysctl(SYSCTL_HANDLER_ARGS)
{
	gid_t *newgids = NULL;
	size_t ingidcnt, newgidcnt = 0;
	int error;

	/* Allocate our new gid array before we take our non-sleepable lock. */
	if (req->newptr != NULL) {
		if (req->newlen % sizeof(gid_t) != 0)
			return (EINVAL);
		ingidcnt = newgidcnt = howmany(req->newlen, sizeof(gid_t));
		newgids = mallocarray(newgidcnt, sizeof(*newgids),
		    M_SEEOTHERUIDS, M_WAITOK);

		error = SYSCTL_IN(req, newgids, newgidcnt * sizeof(*newgids));
		if (error != 0) {
			free(newgids, M_SEEOTHERUIDS);
			return (error);
		}

		specificgid_normalize(newgids, &newgidcnt);

		/*
		 * It might be debatable whether shrinking the allocation is
		 * worth it, but we'll do it in the off-chance that someone is
		 * generating specificgid entries from various configuration
		 * sources that won't de-duplicate.
		 */
		if (newgidcnt < ingidcnt) {
			newgids = realloc(newgids, newgidcnt * sizeof(*newgids),
			    M_SEEOTHERUIDS, M_WAITOK);
		}
	}

	rm_wlock(&seeotheruids_rmlock);

	error = SYSCTL_OUT(req, specificgids,
	    specificgidcnt * sizeof(*specificgids));
	if (error == 0 && req->newptr != NULL) {
		free(specificgids, M_SEEOTHERUIDS);

		specificgids = newgids;
		specificgidcnt = newgidcnt;
	} else if (error != 0) {
		free(newgids, M_SEEOTHERUIDS);
	}

	rm_wunlock(&seeotheruids_rmlock);
	return (error);
}
SYSCTL_PROC(_security_mac_seeotheruids, OID_AUTO, specificgid,
    CTLFLAG_RW | CTLTYPE_UINT | CTLFLAG_MPSAFE, 0, 0,
    &specificgid_sysctl, "I",
    "Specific gid(s) to be exempt from seeotheruids policy");

static void
seeotheruids_destroy(struct mac_policy_conf *mpc __unused)
{
	free(specificgids, M_SEEOTHERUIDS);
}

static int
seeotheruids_check(struct ucred *cr1, struct ucred *cr2)
{
	struct rm_priotracker tracker;
	int error = ESRCH;

	if (!seeotheruids_enabled)
		return (0);

	if (primarygroup_enabled) {
		if (cr1->cr_rgid == cr2->cr_rgid)
			return (0);
	}

	if (cr1->cr_ruid == cr2->cr_ruid)
		return (0);

	if (suser_privileged) {
		if (priv_check_cred(cr1, PRIV_SEEOTHERUIDS) == 0)
			return (0);
	}

	rm_rlock(&seeotheruids_rmlock, &tracker);
	if (specificgid_enabled && specificgids != NULL) {
		const gid_t *suppgroups = cr1->cr_groups;
		size_t nsupp = cr1->cr_ngroups;

#if __FreeBSD_version < 1500056
		/*
		 * FreeBSD 15.0 changed the cr_groups layout: earlier versions
		 * used cr_groups[0] for the effective GID, but that's somewhat
		 * error-prone when propagated throughout the various parts of
		 * the system (e.g., setgroups/getgroups).  In older versions,
		 * we want to hop over the egid.
		 */
		suppgroups++;
		nsupp--;
#endif

		for (size_t i = 0, s = 0; i < specificgidcnt; i++) {
			gid_t cgid;

			cgid = specificgids[i];
			if (cgid == cr1->cr_rgid) {
				error = 0;
				break;
			}

			/*
			 * specificgids and suppgroups are both sorted
			 * ascending, so advance past all of the supplemental
			 * groups that are lower than the specificgid we're
			 * currently at.
			 */
			while (s < nsupp && cgid > suppgroups[s])
				s++;

			/*
			 * Out of supplementary groups, but we'll keep checking
			 * for rgid matches.
			 */
			if (s == nsupp)
				continue;

			if (cgid == suppgroups[s]) {
				error = 0;
				break;
			}
		}
	}

	rm_runlock(&seeotheruids_rmlock, &tracker);
	return (error);
}

static int
seeotheruids_proc_check_debug(struct ucred *cred, struct proc *p)
{

	return (seeotheruids_check(cred, p->p_ucred));
}

static int
seeotheruids_proc_check_sched(struct ucred *cred, struct proc *p)
{

	return (seeotheruids_check(cred, p->p_ucred));
}

static int
seeotheruids_proc_check_signal(struct ucred *cred, struct proc *p,
    int signum)
{

	return (seeotheruids_check(cred, p->p_ucred));
}

static int
seeotheruids_cred_check_visible(struct ucred *cr1, struct ucred *cr2)
{

	return (seeotheruids_check(cr1, cr2));
}

static int
seeotheruids_inpcb_check_visible(struct ucred *cred, struct inpcb *inp,
    struct label *inplabel)
{

	return (seeotheruids_check(cred, inp->inp_cred));
}

static int
seeotheruids_socket_check_visible(struct ucred *cred, struct socket *so,
    struct label *solabel)
{

	return (seeotheruids_check(cred, so->so_cred));
}

static struct mac_policy_ops seeotheruids_ops =
{
	.mpo_destroy = seeotheruids_destroy,
	.mpo_proc_check_debug = seeotheruids_proc_check_debug,
	.mpo_proc_check_sched = seeotheruids_proc_check_sched,
	.mpo_proc_check_signal = seeotheruids_proc_check_signal,
	.mpo_cred_check_visible = seeotheruids_cred_check_visible,
	.mpo_inpcb_check_visible = seeotheruids_inpcb_check_visible,
	.mpo_socket_check_visible = seeotheruids_socket_check_visible,
};

MAC_POLICY_SET(&seeotheruids_ops, mac_seeotheruids,
    "TrustedBSD MAC/seeotheruids", MPC_LOADTIME_FLAG_UNLOADOK, NULL);
