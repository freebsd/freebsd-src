/*
 * Copyright (c) 2022 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* sftp client user/group lookup and caching */

#include "includes.h"

#include <sys/types.h>
#include <openbsd-compat/sys-tree.h>

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "log.h"
#include "xmalloc.h"

#include "sftp-common.h"
#include "sftp-client.h"
#include "sftp-usergroup.h"

/* Tree of id, name */
struct idname {
        u_int id;
	char *name;
        RB_ENTRY(idname) entry;
	/* XXX implement bounded cache as TAILQ */
};
static int
idname_cmp(struct idname *a, struct idname *b)
{
	if (a->id == b->id)
		return 0;
	return a->id > b->id ? 1 : -1;
}
RB_HEAD(idname_tree, idname);
RB_GENERATE_STATIC(idname_tree, idname, entry, idname_cmp)

static struct idname_tree user_idname = RB_INITIALIZER(&user_idname);
static struct idname_tree group_idname = RB_INITIALIZER(&group_idname);

static void
idname_free(struct idname *idname)
{
	if (idname == NULL)
		return;
	free(idname->name);
	free(idname);
}

static void
idname_enter(struct idname_tree *tree, u_int id, const char *name)
{
	struct idname *idname;

	if ((idname = xcalloc(1, sizeof(*idname))) == NULL)
		fatal_f("alloc");
	idname->id = id;
	idname->name = xstrdup(name);
	if (RB_INSERT(idname_tree, tree, idname) != NULL)
		idname_free(idname);
}

static const char *
idname_lookup(struct idname_tree *tree, u_int id)
{
	struct idname idname, *found;

	memset(&idname, 0, sizeof(idname));
	idname.id = id;
	if ((found = RB_FIND(idname_tree, tree, &idname)) != NULL)
		return found->name;
	return NULL;
}

static void
freenames(char **names, u_int nnames)
{
	u_int i;

	if (names == NULL)
		return;
	for (i = 0; i < nnames; i++)
		free(names[i]);
	free(names);
}

static void
lookup_and_record(struct sftp_conn *conn,
    u_int *uids, u_int nuids, u_int *gids, u_int ngids)
{
	int r;
	u_int i;
	char **usernames = NULL, **groupnames = NULL;

	if ((r = sftp_get_users_groups_by_id(conn, uids, nuids, gids, ngids,
	    &usernames, &groupnames)) != 0) {
		debug_fr(r, "sftp_get_users_groups_by_id");
		return;
	}
	for (i = 0; i < nuids; i++) {
		if (usernames[i] == NULL) {
			debug3_f("uid %u not resolved", uids[i]);
			continue;
		}
		debug3_f("record uid %u => \"%s\"", uids[i], usernames[i]);
		idname_enter(&user_idname, uids[i], usernames[i]);
	}
	for (i = 0; i < ngids; i++) {
		if (groupnames[i] == NULL) {
			debug3_f("gid %u not resolved", gids[i]);
			continue;
		}
		debug3_f("record gid %u => \"%s\"", gids[i], groupnames[i]);
		idname_enter(&group_idname, gids[i], groupnames[i]);
	}
	freenames(usernames, nuids);
	freenames(groupnames, ngids);
}

static int
has_id(u_int id, u_int *ids, u_int nids)
{
	u_int i;

	if (nids == 0)
		return 0;

	/* XXX O(N^2) */
	for (i = 0; i < nids; i++) {
		if (ids[i] == id)
			break;
	}
	return i < nids;
}

static void
collect_ids_from_glob(glob_t *g, int user, u_int **idsp, u_int *nidsp)
{
	u_int id, i, n = 0, *ids = NULL;

	for (i = 0; g->gl_pathv[i] != NULL; i++) {
		if (user) {
			if (ruser_name(g->gl_statv[i]->st_uid) != NULL)
				continue; /* Already seen */
			id = (u_int)g->gl_statv[i]->st_uid;
		} else {
			if (rgroup_name(g->gl_statv[i]->st_gid) != NULL)
				continue; /* Already seen */
			id = (u_int)g->gl_statv[i]->st_gid;
		}
		if (has_id(id, ids, n))
			continue;
		ids = xrecallocarray(ids, n, n + 1, sizeof(*ids));
		ids[n++] = id;
	}
	*idsp = ids;
	*nidsp = n;
}

void
get_remote_user_groups_from_glob(struct sftp_conn *conn, glob_t *g)
{
	u_int *uids = NULL, nuids = 0, *gids = NULL, ngids = 0;

	if (!sftp_can_get_users_groups_by_id(conn))
		return;

	collect_ids_from_glob(g, 1, &uids, &nuids);
	collect_ids_from_glob(g, 0, &gids, &ngids);
	lookup_and_record(conn, uids, nuids, gids, ngids);
	free(uids);
	free(gids);
}

static void
collect_ids_from_dirents(SFTP_DIRENT **d, int user, u_int **idsp, u_int *nidsp)
{
	u_int id, i, n = 0, *ids = NULL;

	for (i = 0; d[i] != NULL; i++) {
		if (user) {
			if (ruser_name((uid_t)(d[i]->a.uid)) != NULL)
				continue; /* Already seen */
			id = d[i]->a.uid;
		} else {
			if (rgroup_name((gid_t)(d[i]->a.gid)) != NULL)
				continue; /* Already seen */
			id = d[i]->a.gid;
		}
		if (has_id(id, ids, n))
			continue;
		ids = xrecallocarray(ids, n, n + 1, sizeof(*ids));
		ids[n++] = id;
	}
	*idsp = ids;
	*nidsp = n;
}

void
get_remote_user_groups_from_dirents(struct sftp_conn *conn, SFTP_DIRENT **d)
{
	u_int *uids = NULL, nuids = 0, *gids = NULL, ngids = 0;

	if (!sftp_can_get_users_groups_by_id(conn))
		return;

	collect_ids_from_dirents(d, 1, &uids, &nuids);
	collect_ids_from_dirents(d, 0, &gids, &ngids);
	lookup_and_record(conn, uids, nuids, gids, ngids);
	free(uids);
	free(gids);
}

const char *
ruser_name(uid_t uid)
{
	return idname_lookup(&user_idname, (u_int)uid);
}

const char *
rgroup_name(uid_t gid)
{
	return idname_lookup(&group_idname, (u_int)gid);
}

