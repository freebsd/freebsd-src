/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2022 Klara Systems
 *
 * This software was developed by Mitchell Horne <mhorne@FreeBSD.org>
 * under sponsorship from Juniper Networks and Klara Systems.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "opt_mac.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <ddb/ddb.h>

#include <security/mac/mac_framework.h>
#include <security/mac/mac_internal.h>
#include <security/mac/mac_policy.h>

int
mac_kdb_check_backend(struct kdb_dbbe *be)
{
	int error = 0;

	MAC_POLICY_CHECK_NOSLEEP(kdb_check_backend, be);
	return (error);
}

int
mac_ddb_command_register(struct db_command_table *table, struct db_command *cmd)
{
	int error = 0;

	MAC_POLICY_CHECK_NOSLEEP(ddb_command_register, table, cmd);
	return (error);
}

int
mac_ddb_command_exec(struct db_command *cmd, db_expr_t addr,
    bool have_addr, db_expr_t count, char *modif)
{
	int error = 0;

	MAC_POLICY_CHECK_NOSLEEP(ddb_command_exec, cmd, addr, have_addr,
	    count, modif);
	return (error);
}
