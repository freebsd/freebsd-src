/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by NAI Labs,
 * the Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 *
 * $FreeBSD$
 */

/*
 * Developed by the TrustedBSD Project.
 * Prevent processes owned by a particular uid from seeing various transient
 * kernel objects associated with other uids.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <vm/vm.h>

#include <sys/mac_policy.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, seeotheruids, CTLFLAG_RW, 0,
    "TrustedBSD mac_seeotheruids policy controls");

static int	mac_seeotheruids_enabled = 0;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_seeotheruids_enabled, 0, "Enforce seeotheruids policy");

/*
 * Exception: allow credentials to be aware of other credentials with the
 * same primary gid.
 */
static int	primarygroup_enabled = 0;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, primarygroup_enabled,
    CTLFLAG_RW, &primarygroup_enabled, 0, "Make an exception for credentials "
    "with the same real primary group id");

/*
 * Exception: allow processes with a specific gid to be exempt from the
 * policy.  One sysctl enables this functionality; the other sets the
 * exempt gid.
 */
static int	specificgid_enabled = 0;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, specificgid_enabled,
    CTLFLAG_RW, &specificgid_enabled, 0, "Make an exception for credentials "
    "with a specific gid as their real primary group id or group set");

static gid_t	specificgid = 0;
SYSCTL_INT(_security_mac_seeotheruids, OID_AUTO, specificgid, CTLFLAG_RW,
    &specificgid, 0, "Specific gid to be exempt from seeotheruids policy");

static int
mac_seeotheruids_check(struct ucred *u1, struct ucred *u2)
{

	if (!mac_seeotheruids_enabled)
		return (0);

	if (primarygroup_enabled) {
		if (u1->cr_rgid == u2->cr_rgid)
			return (0);
	}

	if (specificgid_enabled) {
		if (u1->cr_rgid == specificgid || groupmember(specificgid, u1))
			return (0);
	}

	if (u1->cr_ruid == u2->cr_ruid)
		return (0);

	return (ESRCH);
}

static int
mac_seeotheruids_check_cred_visible(struct ucred *u1, struct ucred *u2)
{

	return (mac_seeotheruids_check(u1, u2));
}

static int
mac_seeotheruids_check_proc_signal(struct ucred *cred, struct proc *proc,
    int signum)
{

	return (mac_seeotheruids_check(cred, proc->p_ucred));
}

static int
mac_seeotheruids_check_proc_sched(struct ucred *cred, struct proc *proc)
{

	return (mac_seeotheruids_check(cred, proc->p_ucred));
}

static int
mac_seeotheruids_check_proc_debug(struct ucred *cred, struct proc *proc)
{

	return (mac_seeotheruids_check(cred, proc->p_ucred));
}

static int
mac_seeotheruids_check_socket_visible(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{

	return (mac_seeotheruids_check(cred, socket->so_cred));
}

static struct mac_policy_ops mac_seeotheruids_ops =
{
	.mpo_check_cred_visible = mac_seeotheruids_check_cred_visible,
	.mpo_check_proc_debug = mac_seeotheruids_check_proc_debug,
	.mpo_check_proc_sched = mac_seeotheruids_check_proc_sched,
	.mpo_check_proc_signal = mac_seeotheruids_check_proc_signal,
	.mpo_check_socket_visible = mac_seeotheruids_check_socket_visible,
};

MAC_POLICY_SET(&mac_seeotheruids_ops, trustedbsd_mac_seeotheruids,
    "TrustedBSD MAC/seeotheruids", MPC_LOADTIME_FLAG_UNLOADOK, NULL);
