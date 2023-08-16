/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Semihalf, Stormshield
 * Copyright (c) 2018 Ian Lepore <ian@FreeBSD.org>
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>

#include <security/mac/mac_policy.h>

SYSCTL_DECL(_security_mac);

static SYSCTL_NODE(_security_mac, OID_AUTO, pimd,
    CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "mac_pimd policy controls");

static int pimd_enabled = 0;
SYSCTL_INT(_security_mac_pimd, OID_AUTO, enabled, CTLFLAG_RWTUN,
    &pimd_enabled, 0, "Enable mac_pimd policy");

static int pimd_uid = 0;
SYSCTL_INT(_security_mac_pimd, OID_AUTO, uid, CTLFLAG_RWTUN,
    &pimd_uid, 0, "User id for pimd user");

static int
pimd_priv_grant(struct ucred *cred, int priv)
{

	if (pimd_enabled && cred->cr_uid == pimd_uid) {
		switch (priv) {
		case PRIV_NETINET_MROUTE:
			return (0);
		default:
			break;
		}
	}
	return (EPERM);
}

static struct mac_policy_ops pimd_ops =
{
	.mpo_priv_grant = pimd_priv_grant,
};

MAC_POLICY_SET(&pimd_ops, mac_pimd, "MAC/pimd",
    MPC_LOADTIME_FLAG_UNLOADOK, NULL);
