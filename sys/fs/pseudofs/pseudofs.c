/*-
 * Copyright (c) 2001 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/pseudofs/pseudofs_internal.h>

static MALLOC_DEFINE(M_PFSNODES, "pfs_nodes", "pseudofs nodes");

SYSCTL_NODE(_vfs, OID_AUTO, pfs, CTLFLAG_RW, 0,
    "pseudofs");

#if PFS_FSNAMELEN != MFSNAMELEN
#error "PFS_FSNAMELEN is not equal to MFSNAMELEN"
#endif

/*
 * Add a node to a directory
 */
static int
_pfs_add_node(struct pfs_node *parent, struct pfs_node *node)
{
	KASSERT(parent != NULL,
	    ("%s(): parent is NULL", __func__));
	KASSERT(parent->pn_info != NULL,
	    ("%s(): parent has no pn_info", __func__));
	KASSERT(parent->pn_type == pfstype_dir ||
	    parent->pn_type == pfstype_procdir ||
	    parent->pn_type == pfstype_root,
	    ("%s(): parent is not a directory", __func__));

	/* XXX should check for duplicate names etc. */
	
	mtx_lock(&parent->pn_info->pi_mutex);
	node->pn_info = parent->pn_info;
	node->pn_parent = parent;
	node->pn_next = parent->pn_nodes;
	parent->pn_nodes = node;
	mtx_unlock(&parent->pn_info->pi_mutex);
	
	return (0);
}

/*
 * Add . and .. to a directory
 */
static int
_pfs_fixup_dir(struct pfs_node *parent)
{
	struct pfs_node *dir;
	
	MALLOC(dir, struct pfs_node *, sizeof *dir,
	    M_PFSNODES, M_WAITOK|M_ZERO);
	dir->pn_name[0] = '.';
	dir->pn_type = pfstype_this;
	
	if (_pfs_add_node(parent, dir) != 0) {
		FREE(dir, M_PFSNODES);
		return (-1);
	}
	
	MALLOC(dir, struct pfs_node *, sizeof *dir,
	    M_PFSNODES, M_WAITOK|M_ZERO);
	dir->pn_name[0] = dir->pn_name[1] = '.';
	dir->pn_type = pfstype_parent;
	
	if (_pfs_add_node(parent, dir) != 0) {
		FREE(dir, M_PFSNODES);
		return (-1);
	}

	return (0);
}

/*
 * Create a directory
 */
struct pfs_node	*
pfs_create_dir(struct pfs_node *parent, char *name,
	       pfs_attr_t attr, pfs_vis_t vis, int flags)
{
	struct pfs_node *dir;

	KASSERT(strlen(name) < PFS_NAMELEN,
	    ("%s(): node name is too long", __func__));

	MALLOC(dir, struct pfs_node *, sizeof *dir,
	    M_PFSNODES, M_WAITOK|M_ZERO);
	strcpy(dir->pn_name, name);
	dir->pn_type = (flags & PFS_PROCDEP) ? pfstype_procdir : pfstype_dir;
	dir->pn_attr = attr;
	dir->pn_vis = vis;
	dir->pn_flags = flags & ~PFS_PROCDEP;

	if (_pfs_add_node(parent, dir) != 0) {
		FREE(dir, M_PFSNODES);
		return (NULL);
	}

	if (_pfs_fixup_dir(dir) != 0) {
		pfs_destroy(dir);
		return (NULL);
	}
	
	return (dir);
}

/*
 * Create a file
 */
struct pfs_node	*
pfs_create_file(struct pfs_node *parent, char *name, pfs_fill_t fill,
                pfs_attr_t attr, pfs_vis_t vis, int flags)
{
	struct pfs_node *node;

	KASSERT(strlen(name) < PFS_NAMELEN,
	    ("%s(): node name is too long", __func__));

	MALLOC(node, struct pfs_node *, sizeof *node,
	    M_PFSNODES, M_WAITOK|M_ZERO);
	strcpy(node->pn_name, name);
	node->pn_type = pfstype_file;
	node->pn_func = fill;
	node->pn_attr = attr;
	node->pn_vis = vis;
	node->pn_flags = flags;

	if (_pfs_add_node(parent, node) != 0) {
		FREE(node, M_PFSNODES);
		return (NULL);
	}
	
	return (node);
}

/*
 * Create a symlink
 */
struct pfs_node	*
pfs_create_link(struct pfs_node *parent, char *name, pfs_fill_t fill,
                pfs_attr_t attr, pfs_vis_t vis, int flags)
{
	struct pfs_node *node;

	node = pfs_create_file(parent, name, fill, attr, vis, flags);
	if (node == NULL)
		return (NULL);
	node->pn_type = pfstype_symlink;
	return (node);
}

/*
 * Destroy a node or a tree of nodes
 */
int
pfs_destroy(struct pfs_node *node)
{
	struct pfs_node *parent, *rover;
	
	KASSERT(node != NULL,
	    ("%s(): node is NULL", __func__));
	KASSERT(node->pn_info != NULL,
	    ("%s(): node has no pn_info", __func__));

	/* destroy children */
	if (node->pn_type == pfstype_dir ||
	    node->pn_type == pfstype_procdir ||
	    node->pn_type == pfstype_root)
		while (node->pn_nodes != NULL)
			pfs_destroy(node->pn_nodes);

	/* unlink from parent */
	if ((parent = node->pn_parent) != NULL) {
		KASSERT(parent->pn_info == node->pn_info,
		    ("%s(): parent has different pn_info", __func__));
		mtx_lock(&node->pn_info->pi_mutex);
		if (parent->pn_nodes == node) {
			parent->pn_nodes = node->pn_next;
		} else {
			rover = parent->pn_nodes;
			while (rover->pn_next != NULL) {
				if (rover->pn_next == node) {
					rover->pn_next = node->pn_next;
					break;
				}
				rover = rover->pn_next;
			}
		}
		mtx_unlock(&node->pn_info->pi_mutex);
	}

	/* revoke vnodes and release memory */
	pfs_disable(node);
	FREE(node, M_PFSNODES);

	return (0);
}

/*
 * Mount a pseudofs instance
 */
int
pfs_mount(struct pfs_info *pi, struct mount *mp, char *path, caddr_t data,
	  struct nameidata *ndp, struct thread *td)
{
	struct statfs *sbp;
  
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);
	
	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t)pi;
	vfs_getnewfsid(mp);

	sbp = &mp->mnt_stat;
	bcopy(pi->pi_name, sbp->f_mntfromname, sizeof pi->pi_name);
	sbp->f_bsize = PAGE_SIZE;
	sbp->f_iosize = PAGE_SIZE;
	sbp->f_blocks = 1;
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 1;
	sbp->f_ffree = 0;

	return (0);
}

/*
 * Unmount a pseudofs instance
 */
int
pfs_unmount(struct mount *mp, int mntflags, struct thread *td)
{
	struct pfs_info *pi;
	int error;

	pi = (struct pfs_info *)mp->mnt_data;

	/* XXX do stuff with pi... */
	
	error = vflush(mp, 0, (mntflags & MNT_FORCE) ?  FORCECLOSE : 0);
	return (error);
}

/*
 * Return a root vnode
 */
int
pfs_root(struct mount *mp, struct vnode **vpp)
{
	struct pfs_info *pi;

	pi = (struct pfs_info *)mp->mnt_data;
	return pfs_vncache_alloc(mp, vpp, pi->pi_root, NO_PID);
}

/*
 * Return filesystem stats
 */
int
pfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
	bcopy(&mp->mnt_stat, sbp, sizeof *sbp);
	return (0);
}

/*
 * Initialize a pseudofs instance
 */
int
pfs_init(struct pfs_info *pi, struct vfsconf *vfc)
{
	struct pfs_node *root;
	int error;

	mtx_init(&pi->pi_mutex, "pseudofs", NULL, MTX_DEF);
	
	/* set up the root diretory */
	MALLOC(root, struct pfs_node *, sizeof *root,
	    M_PFSNODES, M_WAITOK|M_ZERO);
	root->pn_type = pfstype_root;
	root->pn_name[0] = '/';
	root->pn_info = pi;
	if (_pfs_fixup_dir(root) != 0) {
		FREE(root, M_PFSNODES);
		return (ENODEV); /* XXX not really the right errno */
	}
	pi->pi_root = root;

	/* construct file hierarchy */
	error = (pi->pi_init)(pi, vfc);
	if (error) {
		pfs_destroy(root);
		pi->pi_root = NULL;
		mtx_destroy(&pi->pi_mutex);
		return (error);
	}
	
	pfs_fileno_init(pi);
	if (bootverbose)
		printf("%s registered\n", pi->pi_name);
	return (0);
}

/*
 * Destroy a pseudofs instance
 */
int
pfs_uninit(struct pfs_info *pi, struct vfsconf *vfc)
{
	int error;
	
	pfs_fileno_uninit(pi);
	pfs_destroy(pi->pi_root);
	pi->pi_root = NULL;
	mtx_destroy(&pi->pi_mutex);
	if (bootverbose)
		printf("%s unregistered\n", pi->pi_name);
	error = (pi->pi_uninit)(pi, vfc);
	return (error);
}

/*
 * Handle load / unload events
 */
static int
pfs_modevent(module_t mod, int evt, void *arg)
{
	switch (evt) {
	case MOD_LOAD:
		pfs_fileno_load();
		pfs_vncache_load();
		break;
	case MOD_UNLOAD:
	case MOD_SHUTDOWN:
		pfs_vncache_unload();
		pfs_fileno_unload();
		break;
	default:
		printf("pseudofs: unexpected event type %d\n", evt);
		break;
	}
	return 0;
}

/*
 * Module declaration
 */
static moduledata_t pseudofs_data = {
	"pseudofs",
	pfs_modevent,
	NULL
};
DECLARE_MODULE(pseudofs, pseudofs_data, SI_SUB_EXEC, SI_ORDER_FIRST);
MODULE_VERSION(pseudofs, 1);
