/*-
 * Copyright (c) 2017 Juniper Networks, Inc.
 * All rights reserved.
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
 *
 */
/*-
 * 9P filesystem subroutines. This file consists of all the Non VFS subroutines.
 * It contains all of the functions related to the driver submission which form
 * the upper layer i.e, p9fs driver. This will interact with the client to make
 * sure we have correct API calls in the header.
 */

#include <sys/cdefs.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include "p9fs_proto.h"

#include <fs/p9fs/p9_client.h>
#include <fs/p9fs/p9_debug.h>
#include <fs/p9fs/p9_protocol.h>
#include <fs/p9fs/p9fs.h>

int
p9fs_proto_dotl(struct p9fs_session *vses)
{

	return (vses->flags & P9FS_PROTO_2000L);
}

/* Initialize a p9fs session */
struct p9_fid *
p9fs_init_session(struct mount *mp, int *error)
{
	struct p9fs_session *vses;
	struct p9fs_mount *virtmp;
	struct p9_fid *fid;
	char *access;

	virtmp = VFSTOP9(mp);
	vses = &virtmp->p9fs_session;
	vses->uid = P9_NONUNAME;
	vses->uname = P9_DEFUNAME;
	vses->aname = P9_DEFANAME;

	/*
	 * Create the client structure. Call into the driver to create
	 * driver structures for the actual IO transfer.
	 */
	vses->clnt = p9_client_create(mp, error, virtmp->mount_tag);

	if (vses->clnt == NULL) {
		P9_DEBUG(ERROR, "%s: p9_client_create failed\n", __func__);
		return (NULL);
	}
	/*
	 * Find the client version and cache the copy. We will use this copy
	 * throughout FS layer.
	 */
	if (p9_is_proto_dotl(vses->clnt))
		vses->flags |= P9FS_PROTO_2000L;
	else if (p9_is_proto_dotu(vses->clnt))
		vses->flags |= P9FS_PROTO_2000U;

	/* Set the access mode */
	access = vfs_getopts(mp->mnt_optnew, "access", error);
	if (access == NULL)
		vses->flags |= P9_ACCESS_USER;
	else if (!strcmp(access, "any"))
		vses->flags |= P9_ACCESS_ANY;
	else if (!strcmp(access, "single"))
		vses->flags |= P9_ACCESS_SINGLE;
	else if (!strcmp(access, "user"))
		vses->flags |= P9_ACCESS_USER;
	else {
		P9_DEBUG(ERROR, "%s: unknown access mode\n", __func__);
		*error = EINVAL;
		goto out;
	}

	*error = 0;
	/* Attach with the backend host*/
	fid = p9_client_attach(vses->clnt, NULL, vses->uname, P9_NONUNAME,
	    vses->aname, error);
	vses->mnt_fid = fid;

	if (*error != 0) {
		P9_DEBUG(ERROR, "%s: attach failed: %d\n", __func__, *error);
		goto out;
	}
	P9_DEBUG(SUBR, "%s: attach successful fid :%p\n", __func__, fid);
	fid->uid = vses->uid;

	/* initialize the node list for the session */
	STAILQ_INIT(&vses->virt_node_list);
	P9FS_LOCK_INIT(vses);

	P9_DEBUG(SUBR, "%s: INIT session successful\n", __func__);

	return (fid);
out:
	p9_client_destroy(vses->clnt);
	return (NULL);
}

/* Begin to terminate a session */
void
p9fs_prepare_to_close(struct mount *mp)
{
	struct p9fs_session *vses;
	struct p9fs_mount *vmp;
	struct p9fs_node *np, *pnp, *tmp;

	vmp = VFSTOP9(mp);
	vses = &vmp->p9fs_session;

	/* break the node->parent references */
	STAILQ_FOREACH_SAFE(np, &vses->virt_node_list, p9fs_node_next, tmp) {
		if (np->parent && np->parent != np) {
			pnp = np->parent;
			np->parent = NULL;
			vrele(P9FS_NTOV(pnp));
		}
	}

	/* We are about to teardown, we dont allow anything other than clunk after this.*/
	p9_client_begin_disconnect(vses->clnt);
}

/* Shutdown a session */
void
p9fs_complete_close(struct mount *mp)
{
	struct p9fs_session *vses;
	struct p9fs_mount *vmp;

	vmp = VFSTOP9(mp);
	vses = &vmp->p9fs_session;

	/* Finish the close*/
	p9_client_disconnect(vses->clnt);
}


/* Call from unmount. Close the session. */
void
p9fs_close_session(struct mount *mp)
{
	struct p9fs_session *vses;
	struct p9fs_mount *vmp;

	vmp = VFSTOP9(mp);
	vses = &vmp->p9fs_session;

	p9fs_complete_close(mp);
	/* Clean up the clnt structure. */
	p9_client_destroy(vses->clnt);
	P9FS_LOCK_DESTROY(vses);
	P9_DEBUG(SUBR, "%s: Clean close session .\n", __func__);
}

/*
 * Remove all the fids of a particular type from a p9fs node
 * as well as destroy/clunk them.
 */
void
p9fs_fid_remove_all(struct p9fs_node *np, int leave_ofids)
{
	struct p9_fid *fid, *tfid;

	STAILQ_FOREACH_SAFE(fid, &np->vfid_list, fid_next, tfid) {
		STAILQ_REMOVE(&np->vfid_list, fid, p9_fid, fid_next);
		p9_client_clunk(fid);
	}

	if (!leave_ofids) {
		STAILQ_FOREACH_SAFE(fid, &np->vofid_list, fid_next, tfid) {
			STAILQ_REMOVE(&np->vofid_list, fid, p9_fid, fid_next);
			p9_client_clunk(fid);
		}
	}
}


/* Remove a fid from its corresponding fid list */
void
p9fs_fid_remove(struct p9fs_node *np, struct p9_fid *fid, int fid_type)
{

	switch (fid_type) {
	case VFID:
		P9FS_VFID_LOCK(np);
		STAILQ_REMOVE(&np->vfid_list, fid, p9_fid, fid_next);
		P9FS_VFID_UNLOCK(np);
		break;
	case VOFID:
		P9FS_VOFID_LOCK(np);
		STAILQ_REMOVE(&np->vofid_list, fid, p9_fid, fid_next);
		P9FS_VOFID_UNLOCK(np);
		break;
	}
}

/* Add a fid to the corresponding fid list */
void
p9fs_fid_add(struct p9fs_node *np, struct p9_fid *fid, int fid_type)
{

	switch (fid_type) {
	case VFID:
		P9FS_VFID_LOCK(np);
		STAILQ_INSERT_TAIL(&np->vfid_list, fid, fid_next);
		P9FS_VFID_UNLOCK(np);
		break;
	case VOFID:
		P9FS_VOFID_LOCK(np);
		STAILQ_INSERT_TAIL(&np->vofid_list, fid, fid_next);
		P9FS_VOFID_UNLOCK(np);
		break;
	}
}

/* Build the path from root to current directory */
static int
p9fs_get_full_path(struct p9fs_node *np, char ***names)
{
	int i, n;
	struct p9fs_node *node;
	char **wnames;

	n = 0;
	for (node = np ; (node != NULL) && !IS_ROOT(node) ; node = node->parent)
		n++;

	if (node == NULL)
		return (0);

	wnames = malloc(n * sizeof(char *), M_TEMP, M_ZERO|M_WAITOK);

	for (i = n-1, node = np; i >= 0 ; i--, node = node->parent)
		wnames[i] = node->inode.i_name;

	*names = wnames;
	return (n);
}

/*
 * Return TRUE if this fid can be used for the requested mode.
 */
static int
p9fs_compatible_mode(struct p9_fid *fid, int mode)
{
	/*
	 * Return TRUE for an exact match. For OREAD and OWRITE, allow
	 * existing ORDWR fids to match. Only check the low two bits
	 * of mode.
	 *
	 * TODO: figure out if this is correct for O_APPEND
	 */
	int fid_mode = fid->mode & 3;
	if (fid_mode == mode)
		return (TRUE);
	if (fid_mode == P9PROTO_ORDWR)
		return (mode == P9PROTO_OREAD || mode == P9PROTO_OWRITE);
	return (FALSE);
}

/*
 * Retrieve fid structure corresponding to a particular
 * uid and fid type for a p9fs node
 */
static struct p9_fid *
p9fs_get_fid_from_uid(struct p9fs_node *np, uid_t uid, int fid_type, int mode)
{
	struct p9_fid *fid;

	switch (fid_type) {
	case VFID:
		P9FS_VFID_LOCK(np);
		STAILQ_FOREACH(fid, &np->vfid_list, fid_next) {
			if (fid->uid == uid) {
				P9FS_VFID_UNLOCK(np);
				return (fid);
			}
		}
		P9FS_VFID_UNLOCK(np);
		break;
	case VOFID:
		P9FS_VOFID_LOCK(np);
		STAILQ_FOREACH(fid, &np->vofid_list, fid_next) {
			if (fid->uid == uid && p9fs_compatible_mode(fid, mode)) {
				P9FS_VOFID_UNLOCK(np);
				return (fid);
			}
		}
		P9FS_VOFID_UNLOCK(np);
		break;
	}

	return (NULL);
}

/*
 * Function returns the fid sturcture for a file corresponding to current user id.
 * First it searches in the fid list of the corresponding p9fs node.
 * New fid will be created if not already present and added in the corresponding
 * fid list in the p9fs node.
 * If the user is not already attached then this will attach the user first
 * and then create a new fid for this particular file by doing dir walk.
 */
struct p9_fid *
p9fs_get_fid(struct p9_client *clnt, struct p9fs_node *np, struct ucred *cred,
    int fid_type, int mode, int *error)
{
	uid_t uid;
	struct p9_fid *fid, *oldfid;
	struct p9fs_node *root;
	struct p9fs_session *vses;
	int i, l, clone;
	char **wnames = NULL;
	uint16_t nwnames;

	oldfid = NULL;
	vses = np->p9fs_ses;

	if (vses->flags & P9_ACCESS_ANY)
		uid = vses->uid;
	else if (cred)
		uid = cred->cr_uid;
	else
		uid = 0;

	/*
	 * Search for the fid in corresponding fid list.
	 * We should return NULL for VOFID if it is not present in the list.
	 * Because VOFID should have been created during the file open.
	 * If VFID is not present in the list then we should create one.
	 */
	fid = p9fs_get_fid_from_uid(np, uid, fid_type, mode);
	if (fid != NULL || fid_type == VOFID)
		return (fid);

	/* Check root if the user is attached */
	root = &np->p9fs_ses->rnp;
	fid = p9fs_get_fid_from_uid(root, uid, fid_type, mode);
	if(fid == NULL) {
		/* Attach the user */
		fid = p9_client_attach(clnt, NULL, NULL, uid,
		    vses->aname, error);
		if (*error != 0)
			return (NULL);
		p9fs_fid_add(root, fid, fid_type);
	}

	/* If we are looking for root then return it */
	if (IS_ROOT(np))
		return (fid);

	/* Get full path from root to p9fs node */
	nwnames = p9fs_get_full_path(np, &wnames);

	/*
	 * Could not get full path.
	 * If p9fs node is not deleted, parent should exist.
	 */
	KASSERT(nwnames != 0, ("%s: Directory of %s doesn't exist", __func__, np->inode.i_name));

	clone = 1;
	i = 0;
	while (i < nwnames) {
		l = MIN(nwnames - i, P9_MAXWELEM);

		fid = p9_client_walk(fid, l, wnames, clone, error);
		if (*error != 0) {
			if (oldfid)
				p9_client_clunk(oldfid);
			fid = NULL;
			goto bail_out;
		}
		oldfid = fid;
		clone = 0;
		i += l ;
	}
	p9fs_fid_add(np, fid, fid_type);
bail_out:
	free(wnames, M_TEMP);
	return (fid);
}
