/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Florian Walpen <dev@submerge.ch>
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

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>

#include <security/mac/mac_policy.h>

SYSCTL_DECL(_security_mac);

static SYSCTL_NODE(_security_mac, OID_AUTO, priority,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "mac_priority policy controls");

static int realtime_enabled = 1;
SYSCTL_INT(_security_mac_priority, OID_AUTO, realtime, CTLFLAG_RWTUN,
    &realtime_enabled, 0,
    "Enable realtime priority scheduling for group realtime_gid");

static int realtime_gid = GID_RT_PRIO;
SYSCTL_INT(_security_mac_priority, OID_AUTO, realtime_gid, CTLFLAG_RWTUN,
    &realtime_gid, 0,
    "Group id of the realtime privilege group");

static int idletime_enabled = 1;
SYSCTL_INT(_security_mac_priority, OID_AUTO, idletime, CTLFLAG_RWTUN,
    &idletime_enabled, 0,
    "Enable idle priority scheduling for group idletime_gid");

static int idletime_gid = GID_ID_PRIO;
SYSCTL_INT(_security_mac_priority, OID_AUTO, idletime_gid, CTLFLAG_RWTUN,
    &idletime_gid, 0,
    "Group id of the idletime privilege group");

static int
priority_priv_grant(struct ucred *cred, int priv)
{
	if (priv == PRIV_SCHED_RTPRIO && realtime_enabled &&
	    groupmember(realtime_gid, cred))
		return (0);

	if (priv == PRIV_SCHED_IDPRIO && idletime_enabled &&
	    groupmember(idletime_gid, cred))
		return (0);

	return (EPERM);
}

static struct mac_policy_ops priority_ops = {
	.mpo_priv_grant = priority_priv_grant,
};

MAC_POLICY_SET(&priority_ops, mac_priority, "MAC/priority",
    MPC_LOADTIME_FLAG_UNLOADOK, NULL);
