/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Alexander V. Chernikov <melifaro@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/ck.h>
#include <sys/epoch.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sx.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_var.h>

#define	DEBUG_MOD_NAME	nl_generic_kpi
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);


/*
 * NETLINK_GENERIC families/groups registration logic
 */

#define	GENL_LOCK()		sx_xlock(&sx_lock)
#define	GENL_UNLOCK()		sx_xunlock(&sx_lock)
static struct sx sx_lock;
SX_SYSINIT(genl_lock, &sx_lock, "genetlink lock");

static struct genl_family	families[MAX_FAMILIES];
static struct genl_group	groups[MAX_GROUPS];

static struct genl_family *
find_family(const char *family_name)
{
	for (int i = 0; i < MAX_FAMILIES; i++) {
		struct genl_family *gf = &families[i];
		if (gf->family_name != NULL && !strcmp(gf->family_name, family_name))
			return (gf);
	}

	return (NULL);
}

static struct genl_family *
find_empty_family_id(const char *family_name)
{
	struct genl_family *gf = NULL;

	if (!strcmp(family_name, CTRL_FAMILY_NAME)) {
		gf = &families[0];
		gf->family_id = GENL_MIN_ID;
	} else {
		/* Index 0 is reserved for the control family */
		for (int i = 1; i < MAX_FAMILIES; i++) {
			gf = &families[i];
			if (gf->family_name == NULL) {
				gf->family_id = GENL_MIN_ID + i;
				break;
			}
		}
	}

	return (gf);
}

uint32_t
genl_register_family(const char *family_name, size_t hdrsize, int family_version,
    int max_attr_idx)
{
	uint32_t family_id = 0;

	MPASS(family_name != NULL);
	if (find_family(family_name) != NULL)
		return (0);

	GENL_LOCK();

	struct genl_family *gf = find_empty_family_id(family_name);
	MPASS(gf != NULL);

	gf->family_name = family_name;
	gf->family_version = family_version;
	gf->family_hdrsize = hdrsize;
	gf->family_attr_max = max_attr_idx;
	NL_LOG(LOG_DEBUG2, "Registered family %s id %d", gf->family_name, gf->family_id);
	family_id = gf->family_id;
	EVENTHANDLER_INVOKE(genl_family_event, gf, CTRL_CMD_NEWFAMILY);

	GENL_UNLOCK();

	return (family_id);
}

static void
free_family(struct genl_family *gf)
{
	if (gf->family_cmds != NULL)
		free(gf->family_cmds, M_NETLINK);
}

/*
 * unregister groups of a given family
 */
static void
unregister_groups(const struct genl_family *gf)
{

	for (int i = 0; i < MAX_GROUPS; i++) {
		struct genl_group *gg = &groups[i];
		if (gg->group_family == gf && gg->group_name != NULL) {
			gg->group_family = NULL;
			gg->group_name = NULL;
		}
	}
}

/*
 * Can sleep, I guess
 */
bool
genl_unregister_family(const char *family_name)
{
	bool found = false;

	GENL_LOCK();
	struct genl_family *gf = find_family(family_name);

	if (gf != NULL) {
		EVENTHANDLER_INVOKE(genl_family_event, gf, CTRL_CMD_DELFAMILY);
		found = true;
		unregister_groups(gf);
		/* TODO: zero pointer first */
		free_family(gf);
		bzero(gf, sizeof(*gf));
	}
	GENL_UNLOCK();

	return (found);
}

bool
genl_register_cmds(const char *family_name, const struct genl_cmd *cmds, int count)
{
	GENL_LOCK();
	struct genl_family *gf = find_family(family_name);
	if (gf == NULL) {
		GENL_UNLOCK();
		return (false);
	}

	int cmd_size = gf->family_cmd_size;

	for (int i = 0; i < count; i++) {
		MPASS(cmds[i].cmd_cb != NULL);
		if (cmds[i].cmd_num >= cmd_size)
			cmd_size = cmds[i].cmd_num + 1;
	}

	if (cmd_size > gf->family_cmd_size) {
		/* need to realloc */
		size_t sz = cmd_size * sizeof(struct genl_cmd);
		void *data = malloc(sz, M_NETLINK, M_WAITOK | M_ZERO);

		memcpy(data, gf->family_cmds, gf->family_cmd_size * sizeof(struct genl_cmd));
		void *old_data = gf->family_cmds;
		gf->family_cmds = data;
		gf->family_cmd_size = cmd_size;
		free(old_data, M_NETLINK);
	}

	for (int i = 0; i < count; i++) {
		const struct genl_cmd *cmd = &cmds[i];
		MPASS(gf->family_cmds[cmd->cmd_num].cmd_cb == NULL);
		gf->family_cmds[cmd->cmd_num] = cmds[i];
		NL_LOG(LOG_DEBUG2, "Adding cmd %s(%d) to family %s",
		    cmd->cmd_name, cmd->cmd_num, gf->family_name);
	}
	GENL_UNLOCK();
	return (true);
}

static struct genl_group *
find_group(const struct genl_family *gf, const char *group_name)
{
	for (int i = 0; i < MAX_GROUPS; i++) {
		struct genl_group *gg = &groups[i];
		if (gg->group_family == gf && !strcmp(gg->group_name, group_name))
			return (gg);
	}
	return (NULL);
}

uint32_t
genl_register_group(const char *family_name, const char *group_name)
{
	uint32_t group_id = 0;

	MPASS(family_name != NULL);
	MPASS(group_name != NULL);

	GENL_LOCK();
	struct genl_family *gf = find_family(family_name);

	if (gf == NULL || find_group(gf, group_name) != NULL) {
		GENL_UNLOCK();
		return (0);
	}

	for (int i = 0; i < MAX_GROUPS; i++) {
		struct genl_group *gg = &groups[i];
		if (gg->group_family == NULL) {
			gf->family_num_groups++;
			gg->group_family = gf;
			gg->group_name = group_name;
			group_id = i + MIN_GROUP_NUM;
			break;
		}
	}
	GENL_UNLOCK();

	return (group_id);
}

/* accessors */
struct genl_family *
genl_get_family(uint32_t family_id)
{
	return ((family_id < MAX_FAMILIES) ? &families[family_id] : NULL);
}

const char *
genl_get_family_name(const struct genl_family *gf)
{
	return (gf->family_name);
}

uint32_t
genl_get_family_id(const struct genl_family *gf)
{
	return (gf->family_id);
}

struct genl_group *
genl_get_group(uint32_t group_id)
{
	return ((group_id < MAX_GROUPS) ? &groups[group_id] : NULL);
}

