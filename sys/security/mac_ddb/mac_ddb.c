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

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kdb.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <net/vnet.h>

#include <ddb/ddb.h>
#include <ddb/db_command.h>

#include <security/mac/mac_policy.h>

/*
 * This module provides a limited interface to the ddb(4) kernel debugger. The
 * intent is to allow execution of useful debugging commands while disallowing
 * the execution of commands which may be used to inspect/modify arbitrary
 * system memory.
 *
 * Commands which are deterministic in their output or effect and that have
 * been flagged with DB_CMD_MEMSAFE in their definition will be allowed.
 *
 * Other commands are valid within this context so long as there is some
 * constraint placed on their input arguments. This applies to most 'show'
 * commands which accept an arbitrary address. If the provided address can be
 * validated as a real instance of the object (e.g. the 'show proc' address
 * points to a real struct proc in the process list), then the command may be
 * executed. This module defines several validation functions which are used to
 * conditionally allow or block the execution of some commands. For these
 * commands we define and apply the DB_CMD_VALIDATE flag.
 *
 * Any other commands not flagged with DM_CMD_MEMSAFE or DB_CMD_VALIDATE are
 * considered unsafe for execution.
 */

#define	DB_CMD_VALIDATE		DB_MAC1

typedef int db_validation_fn_t(db_expr_t addr, bool have_addr, db_expr_t count,
    char *modif);

static db_validation_fn_t	db_thread_valid;
static db_validation_fn_t	db_show_ffs_valid;
static db_validation_fn_t	db_show_prison_valid;
static db_validation_fn_t	db_show_proc_valid;
static db_validation_fn_t	db_show_rman_valid;
#ifdef VIMAGE
static db_validation_fn_t	db_show_vnet_valid;
#endif

struct cmd_list_item {
	const char *name;
	db_validation_fn_t *validate_fn;
};

/* List of top-level ddb(4) commands which are allowed by this policy. */
static const struct cmd_list_item command_list[] = {
	{ "thread",	db_thread_valid },
};

/* List of ddb(4) 'show' commands which are allowed by this policy. */
static const struct cmd_list_item show_command_list[] = {
	{ "ffs",	db_show_ffs_valid },
	{ "prison",	db_show_prison_valid },
	{ "proc",	db_show_proc_valid },
	{ "rman",	db_show_rman_valid },
	{ "thread",	db_thread_valid },
#ifdef VIMAGE
	{ "vnet",	db_show_vnet_valid },
#endif
};

static int
db_thread_valid(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	struct thread *thr;
	lwpid_t tid;

	/* Default will show the current proc. */
	if (!have_addr)
		return (0);

	/* Validate the provided addr OR tid against the thread list. */
	tid = db_hex2dec(addr);
	for (thr = kdb_thr_first(); thr != NULL; thr = kdb_thr_next(thr)) {
		if ((void *)thr == (void *)addr || tid == thr->td_tid)
			return (0);
	}

	return (EACCES);
}

static int
db_show_ffs_valid(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	struct mount *mp;

	/* No addr will show all mounts. */
	if (!have_addr)
		return (0);

	TAILQ_FOREACH(mp, &mountlist, mnt_list)
		if ((void *)mp == (void *)addr)
			return (0);

	return (EACCES);
}

static int
db_show_prison_valid(db_expr_t addr, bool have_addr, db_expr_t count,
    char *modif)
{
	struct prison *pr;
	int pr_id;

	if (!have_addr || addr == 0)
		return (0);

	/* prison can match by pointer address or ID. */
	pr_id = (int)addr;
	TAILQ_FOREACH(pr, &allprison, pr_list)
		if (pr->pr_id == pr_id || (void *)pr == (void *)addr)
			return (0);

	return (EACCES);
}

static int
db_show_proc_valid(db_expr_t addr, bool have_addr, db_expr_t count,
    char *modif)
{
	struct proc *p;
	int i;

	/* Default will show the current proc. */
	if (!have_addr)
		return (0);

	for (i = 0; i <= pidhash; i++) {
		LIST_FOREACH(p, &pidhashtbl[i], p_hash) {
			if ((void *)p == (void *)addr)
				return (0);
		}
	}

	return (EACCES);
}

static int
db_show_rman_valid(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	struct rman *rm;

	TAILQ_FOREACH(rm, &rman_head, rm_link) {
		if ((void *)rm == (void *)rm)
			return (0);
	}

	return (EACCES);
}

#ifdef VIMAGE
static int
db_show_vnet_valid(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	VNET_ITERATOR_DECL(vnet);

	if (!have_addr)
		return (0);

	VNET_FOREACH(vnet) {
		if ((void *)vnet == (void *)addr)
			return (0);
	}

	return (EACCES);
}
#endif

static int
command_match(struct db_command *cmd, struct cmd_list_item item)
{
	db_validation_fn_t *vfn;
	int n;

	n = strcmp(cmd->name, item.name);
	if (n != 0)
		return (n);

	/* Got an exact match. Update the command struct */
	vfn = item.validate_fn;
	if (vfn != NULL) {
		cmd->flag |= DB_CMD_VALIDATE;
		cmd->mac_priv = vfn;
	}
	return (0);
}

static void
mac_ddb_init(struct mac_policy_conf *conf)
{
	struct db_command *cmd, *prev;
	int i, n;

	/* The command lists are sorted lexographically, as are our arrays. */

	/* Register basic commands. */
	for (i = 0, cmd = prev = NULL; i < nitems(command_list); i++) {
		LIST_FOREACH_FROM(cmd, &db_cmd_table, next) {
			n = command_match(cmd, command_list[i]);
			if (n == 0) {
				/* Got an exact match. */
				prev = cmd;
				break;
			} else if (n > 0) {
				/* Desired command is not registered. */
				break;
			}
		}

		/* Next search begins at the previous match. */
		cmd = prev;
	}

	/* Register 'show' commands which require validation. */
	for (i = 0, cmd = prev = NULL; i < nitems(show_command_list); i++) {
		LIST_FOREACH_FROM(cmd, &db_show_table, next) {
			n = command_match(cmd, show_command_list[i]);
			if (n == 0) {
				/* Got an exact match. */
				prev = cmd;
				break;
			} else if (n > 0) {
				/* Desired command is not registered. */
				break;
			}
		}

		/* Next search begins at the previous match. */
		cmd = prev;
	}

#ifdef INVARIANTS
	/* Verify the lists are sorted correctly. */
	const char *a, *b;

	for (i = 0; i < nitems(command_list) - 1; i++) {
		a = command_list[i].name;
		b = command_list[i + 1].name;
		if (strcmp(a, b) > 0)
			panic("%s: command_list[] not alphabetical: %s,%s",
			    __func__, a, b);
	}
	for (i = 0; i < nitems(show_command_list) - 1; i++) {
		a = show_command_list[i].name;
		b = show_command_list[i + 1].name;
		if (strcmp(a, b) > 0)
			panic("%s: show_command_list[] not alphabetical: %s,%s",
			    __func__, a, b);
	}
#endif
}

static int
mac_ddb_command_register(struct db_command_table *table,
    struct db_command *cmd)
{
	int i, n;

	if ((cmd->flag & DB_CMD_MEMSAFE) != 0)
		return (0);

	/* For other commands, search the allow-lists. */
	if (table == &db_show_table) {
		for (i = 0; i < nitems(show_command_list); i++) {
			n = command_match(cmd, show_command_list[i]);
			if (n == 0)
				/* Got an exact match. */
				return (0);
			else if (n > 0)
				/* Command is not in the policy list. */
				break;
		}
	} else if (table == &db_cmd_table) {
		for (i = 0; i < nitems(command_list); i++) {
			n = command_match(cmd, command_list[i]);
			if (n == 0)
				/* Got an exact match. */
				return (0);
			else if (n > 0)
				/* Command is not in the policy list. */
				break;
		}
	}

	/* The command will not be registered. */
	return (EACCES);
}

static int
mac_ddb_command_exec(struct db_command *cmd, db_expr_t addr,
    bool have_addr, db_expr_t count, char *modif)
{
	db_validation_fn_t *vfn = cmd->mac_priv;

	/* Validate the command and args based on policy. */
	if ((cmd->flag & DB_CMD_VALIDATE) != 0) {
		MPASS(vfn != NULL);
		if (vfn(addr, have_addr, count, modif) == 0)
			return (0);
	} else if ((cmd->flag & DB_CMD_MEMSAFE) != 0)
		return (0);

	return (EACCES);
}

static int
mac_ddb_check_backend(struct kdb_dbbe *be)
{

	/* Only allow DDB backend to execute. */
	if (strcmp(be->dbbe_name, "ddb") == 0)
		return (0);

	return (EACCES);
}

/*
 * Register functions with MAC Framework policy entry points.
 */
static struct mac_policy_ops mac_ddb_ops =
{
	.mpo_init = mac_ddb_init,

	.mpo_ddb_command_register = mac_ddb_command_register,
	.mpo_ddb_command_exec = mac_ddb_command_exec,

	.mpo_kdb_check_backend = mac_ddb_check_backend,
};
MAC_POLICY_SET(&mac_ddb_ops, mac_ddb, "MAC/DDB", 0, NULL);
