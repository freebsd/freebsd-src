/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed
 * to Berkeley by John Heidemann of the UCLA Ficus project.
 *
 * Source: * @(#)i405_init.c 2.10 92/04/27 UCLA Ficus project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)vfs_init.c	8.3 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/malloc.h>

static int	vfs_register(struct vfsconf *);
static int	vfs_unregister(struct vfsconf *);

MALLOC_DEFINE(M_VNODE, "vnodes", "Dynamically allocated vnodes");

/*
 * The highest defined VFS number.
 */
int maxvfsconf = VFS_GENERIC + 1;

/*
 * Single-linked list of configured VFSes.
 * New entries are added/deleted by vfs_register()/vfs_unregister()
 */
struct vfsconfhead vfsconf = TAILQ_HEAD_INITIALIZER(vfsconf);

/*
 * A Zen vnode attribute structure.
 *
 * Initialized when the first filesystem registers by vfs_register().
 */
struct vattr va_null;

/*
 * vfs_init.c
 *
 * Allocate and fill in operations vectors.
 *
 * An undocumented feature of this approach to defining operations is that
 * there can be multiple entries in vfs_opv_descs for the same operations
 * vector. This allows third parties to extend the set of operations
 * supported by another layer in a binary compatibile way. For example,
 * assume that NFS needed to be modified to support Ficus. NFS has an entry
 * (probably nfs_vnopdeop_decls) declaring all the operations NFS supports by
 * default. Ficus could add another entry (ficus_nfs_vnodeop_decl_entensions)
 * listing those new operations Ficus adds to NFS, all without modifying the
 * NFS code. (Of couse, the OTW NFS protocol still needs to be munged, but
 * that is a(whole)nother story.) This is a feature.
 */

/*
 * Routines having to do with the management of the vnode table.
 */

struct vfsconf *
vfs_byname(const char *name)
{
	struct vfsconf *vfsp;

	if (!strcmp(name, "ffs"))
		name = "ufs";
	TAILQ_FOREACH(vfsp, &vfsconf, vfc_list)
		if (!strcmp(name, vfsp->vfc_name))
			return (vfsp);
	return (NULL);
}

struct vfsconf *
vfs_byname_kld(const char *fstype, struct thread *td, int *error)
{
	struct vfsconf *vfsp;
	linker_file_t lf;

	vfsp = vfs_byname(fstype);
	if (vfsp != NULL)
		return (vfsp);

	/* Only load modules for root (very important!). */
	*error = suser(td);
	if (*error)
		return (NULL);
	*error = securelevel_gt(td->td_ucred, 0);
	if (*error) 
		return (NULL);
	*error = linker_load_module(NULL, fstype, NULL, NULL, &lf);
	if (lf == NULL)
		*error = ENODEV;
	if (*error)
		return (NULL);
	lf->userrefs++;
	/* Look up again to see if the VFS was loaded. */
	vfsp = vfs_byname(fstype);
	if (vfsp == NULL) {
		lf->userrefs--;
		linker_file_unload(lf, LINKER_UNLOAD_FORCE);
		*error = ENODEV;
		return (NULL);
	}
	return (vfsp);
}


/* Register a new filesystem type in the global table */
static int
vfs_register(struct vfsconf *vfc)
{
	struct sysctl_oid *oidp;
	struct vfsops *vfsops;
	static int once;

	if (!once) {
		vattr_null(&va_null);
		once = 1;
	}
	
	if (vfc->vfc_version != VFS_VERSION) {
		printf("ERROR: filesystem %s, unsupported ABI version %x\n",
		    vfc->vfc_name, vfc->vfc_version);
		return (EINVAL);
	}
	if (vfs_byname(vfc->vfc_name) != NULL)
		return EEXIST;

	vfc->vfc_typenum = maxvfsconf++;
	TAILQ_INSERT_TAIL(&vfsconf, vfc, vfc_list);

	/*
	 * If this filesystem has a sysctl node under vfs
	 * (i.e. vfs.xxfs), then change the oid number of that node to 
	 * match the filesystem's type number.  This allows user code
	 * which uses the type number to read sysctl variables defined
	 * by the filesystem to continue working. Since the oids are
	 * in a sorted list, we need to make sure the order is
	 * preserved by re-registering the oid after modifying its
	 * number.
	 */
	SLIST_FOREACH(oidp, &sysctl__vfs_children, oid_link)
		if (strcmp(oidp->oid_name, vfc->vfc_name) == 0) {
			sysctl_unregister_oid(oidp);
			oidp->oid_number = vfc->vfc_typenum;
			sysctl_register_oid(oidp);
		}

	/*
	 * Initialise unused ``struct vfsops'' fields, to use
	 * the vfs_std*() functions.  Note, we need the mount
	 * and unmount operations, at the least.  The check
	 * for vfsops available is just a debugging aid.
	 */
	KASSERT(vfc->vfc_vfsops != NULL,
	    ("Filesystem %s has no vfsops", vfc->vfc_name));
	/*
	 * Check the mount and unmount operations.
	 */
	vfsops = vfc->vfc_vfsops;
	KASSERT(vfsops->vfs_mount != NULL,
	    ("Filesystem %s has no mount op", vfc->vfc_name));
	KASSERT(vfsops->vfs_unmount != NULL,
	    ("Filesystem %s has no unmount op", vfc->vfc_name));

	if (vfsops->vfs_start == NULL)
		/* make a file system operational */ 
		vfsops->vfs_start =	vfs_stdstart;
	if (vfsops->vfs_root == NULL)
		/* return file system's root vnode */
		vfsops->vfs_root =	vfs_stdroot;
	if (vfsops->vfs_quotactl == NULL)
		/* quota control */
		vfsops->vfs_quotactl =	vfs_stdquotactl;
	if (vfsops->vfs_statfs == NULL)
		/* return file system's status */
		vfsops->vfs_statfs =	vfs_stdstatfs;
	if (vfsops->vfs_sync == NULL)
		/*
		 * flush unwritten data (nosync)
		 * file systems can use vfs_stdsync
		 * explicitly by setting it in the
		 * vfsop vector.
		 */
		vfsops->vfs_sync =	vfs_stdnosync;
	if (vfsops->vfs_vget == NULL)
		/* convert an inode number to a vnode */
		vfsops->vfs_vget =	vfs_stdvget;
	if (vfsops->vfs_fhtovp == NULL)
		/* turn an NFS file handle into a vnode */
		vfsops->vfs_fhtovp =	vfs_stdfhtovp;
	if (vfsops->vfs_checkexp == NULL)
		/* check if file system is exported */
		vfsops->vfs_checkexp =	vfs_stdcheckexp;
	if (vfsops->vfs_vptofh == NULL)
		/* turn a vnode into an NFS file handle */
		vfsops->vfs_vptofh =	vfs_stdvptofh;
	if (vfsops->vfs_init == NULL)
		/* file system specific initialisation */
		vfsops->vfs_init =	vfs_stdinit;
	if (vfsops->vfs_uninit == NULL)
		/* file system specific uninitialisation */
		vfsops->vfs_uninit =	vfs_stduninit;
	if (vfsops->vfs_extattrctl == NULL)
		/* extended attribute control */
		vfsops->vfs_extattrctl = vfs_stdextattrctl;
	if (vfsops->vfs_sysctl == NULL)
		vfsops->vfs_sysctl = vfs_stdsysctl;
	
	/*
	 * Call init function for this VFS...
	 */
	(*(vfc->vfc_vfsops->vfs_init))(vfc);

	return 0;
}


/* Remove registration of a filesystem type */
static int
vfs_unregister(struct vfsconf *vfc)
{
	struct vfsconf *vfsp;
	int error, i, maxtypenum;

	i = vfc->vfc_typenum;

	vfsp = vfs_byname(vfc->vfc_name);
	if (vfsp == NULL)
		return EINVAL;
	if (vfsp->vfc_refcount)
		return EBUSY;
	if (vfc->vfc_vfsops->vfs_uninit != NULL) {
		error = (*vfc->vfc_vfsops->vfs_uninit)(vfsp);
		if (error)
			return (error);
	}
	TAILQ_REMOVE(&vfsconf, vfsp, vfc_list);
	maxtypenum = VFS_GENERIC;
	TAILQ_FOREACH(vfsp, &vfsconf, vfc_list)
		if (maxtypenum < vfsp->vfc_typenum)
			maxtypenum = vfsp->vfc_typenum;
	maxvfsconf = maxtypenum + 1;
	return 0;
}

/*
 * Standard kernel module handling code for filesystem modules.
 * Referenced from VFS_SET().
 */
int
vfs_modevent(module_t mod, int type, void *data)
{
	struct vfsconf *vfc;
	int error = 0;

	vfc = (struct vfsconf *)data;

	switch (type) {
	case MOD_LOAD:
		if (vfc)
			error = vfs_register(vfc);
		break;

	case MOD_UNLOAD:
		if (vfc)
			error = vfs_unregister(vfc);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}
