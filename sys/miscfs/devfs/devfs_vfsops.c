/*-
 * Copyright 1997,1998 Julian Elischer.  All rights reserved.
 * julian@freebsd.org
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 *	$Id: devfs_vfsops.c,v 1.29 1998/04/19 23:32:12 julian Exp $
 *
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>

#include <miscfs/devfs/devfsdefs.h>

static MALLOC_DEFINE(M_DEVFSMNT, "DEVFS mount", "DEVFS mount structure");

static int devfs_statfs( struct mount *mp, struct statfs *sbp, struct proc *p);

/*-
 * Called from the generic VFS startups.
 * This is the second stage of DEVFS initialisation.
 * The probed devices have already been loaded and the 
 * basic structure of the DEVFS created.
 * We take the oportunity to mount the hidden DEVFS layer, so that
 * devices from devfs get sync'd.
 */
static int
devfs_init(struct vfsconf *vfsp)
{
	struct mount *mp = dev_root->dnp->dvm->mount;
	/*-
	 * fill in the missing members on the "hidden" mount
	 * we could almost use vfs_rootmountalloc() to do this.
	 */
	lockinit(&mp->mnt_lock, PVFS, "vfslock", 0, 0);
	mp->mnt_op = vfsp->vfc_vfsops;
	mp->mnt_vfc = vfsp;
	mp->mnt_stat.f_type = vfsp->vfc_typenum;
	mp->mnt_flag |= vfsp->vfc_flags & MNT_VISFLAGMASK;
	strncpy(mp->mnt_stat.f_fstypename, vfsp->vfc_name, MFSNAMELEN);
	mp->mnt_vnodecovered = NULLVP;

	/* Mark a reference for the "invisible" blueprint mount */
	mp->mnt_vfc->vfc_refcount++;
	CIRCLEQ_INSERT_TAIL(&mountlist, mp, mnt_list);

	printf("DEVFS: ready to run\n");
	return 0; /*XXX*/
}

/*-
 *  mp	 - pointer to 'mount' structure
 *  path - addr in user space of mount point (ie /usr or whatever)
 *  data - addr in user space of mount params including the
 *         name of the block special file to treat as a filesystem.
 *  ndp  - namei data pointer
 *  p    - proc pointer
 * devfs is special in that it doesn't require any device to be mounted..
 * It makes up its data as it goes along.
 * it must be mounted during single user.. until it is, only std{in/out/err}
 * and the root filesystem are available.
 */
/*proto*/
int
devfs_mount(struct mount *mp, char *path, caddr_t data,
	    struct nameidata *ndp, struct proc *p)
{
	struct devfsmount *devfs_mp_p;	/* devfs specific mount info */
	int error;
	u_int size;

DBPRINT(("mount "));

	/*-
	 *  If they just want to update, we don't need to do anything.
	 */
	if (mp->mnt_flag & MNT_UPDATE)
	{
		return 0;
	}

	/*-
	 *  Well, it's not an update, it's a real mount request.
	 *  Time to get dirty.
	 * HERE we should check to see if we are already mounted here.
	 */

	devfs_mp_p = (struct devfsmount *)malloc(sizeof *devfs_mp_p,
						M_DEVFSMNT, M_WAITOK);
	if (devfs_mp_p == NULL)
		return (ENOMEM);
	bzero(devfs_mp_p,sizeof(*devfs_mp_p));
	devfs_mp_p->mount = mp;
	mp->mnt_data = (void *)devfs_mp_p;

	/*-
	 *  Fill out some fields
	 */
	mp->mnt_data = (qaddr_t)devfs_mp_p;
	mp->mnt_stat.f_type = MOUNT_DEVFS;
	mp->mnt_stat.f_fsid.val[0] = (long)devfs_mp_p;
	mp->mnt_stat.f_fsid.val[1] = MOUNT_DEVFS;
	mp->mnt_flag |= MNT_LOCAL;

	if(error = dev_dup_plane(devfs_mp_p))
	{
		mp->mnt_data = (qaddr_t)0;
		free((caddr_t)devfs_mp_p, M_DEVFSMNT);
		return (error);
	}

	/*-
	 *  Copy in the name of the directory the filesystem
	 *  is to be mounted on.
	 *  And we clear the remainder of the character strings
	 *  to be tidy.
	 *  Then, we try to fill in the filesystem stats structure
	 *  as best we can with whatever we can think of at the time
	 */
	
	if(devfs_up_and_going) {
		copyinstr(path, (caddr_t)mp->mnt_stat.f_mntonname,
			sizeof(mp->mnt_stat.f_mntonname)-1, &size);
		bzero(mp->mnt_stat.f_mntonname + size,
			sizeof(mp->mnt_stat.f_mntonname) - size);
	} else {
		bcopy("dummy_mount", (caddr_t)mp->mnt_stat.f_mntonname,12);
	}
	bzero(mp->mnt_stat.f_mntfromname , MNAMELEN );
	bcopy("devfs",mp->mnt_stat.f_mntfromname, 5);
	(void)devfs_statfs(mp, &mp->mnt_stat, p);
	return 0;
}


static int
devfs_start(struct mount *mp, int flags, struct proc *p)
{
DBPRINT(("start "));
	return 0;
}

/*-
 *  Unmount the filesystem described by mp.
 */
static int
devfs_unmount( struct mount *mp, int mntflags, struct proc *p)
{
	struct devfsmount *devfs_mp_p = (struct devfsmount *)mp->mnt_data;
	int flags = 0;
	int error;
	
	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
	}
	error = vflush(mp, NULLVP, flags);
	if (error)
		return error;

DBPRINT(("unmount "));
	devfs_free_plane(devfs_mp_p);
	free((caddr_t)devfs_mp_p, M_DEVFSMNT);
	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;

	return 0;
}

/* return the address of the root vnode  in *vpp */
static int
devfs_root(struct mount *mp, struct vnode **vpp)
{
	struct devfsmount *devfs_mp_p = (struct devfsmount *)(mp->mnt_data);

DBPRINT(("root "));
	devfs_dntovn(devfs_mp_p->plane_root->dnp,vpp);
	return 0;
}

static int
devfs_quotactl(struct mount *mp, int cmds, uid_t uid, caddr_t arg,
	       struct proc *p)
{
DBPRINT(("quotactl "));
	return EOPNOTSUPP;
}

static int
devfs_statfs( struct mount *mp, struct statfs *sbp, struct proc *p)
{
	struct devfsmount *devfs_mp_p = (struct devfsmount *)mp->mnt_data;

/*-
 *  Fill in the stat block.
 */
DBPRINT(("statfs "));
	sbp->f_type   = MOUNT_DEVFS;
	sbp->f_flags  = 0;		/* XXX */
	sbp->f_bsize  = 128;
	sbp->f_iosize = 1024;	/* XXX*/
	sbp->f_blocks = 128;
	sbp->f_bfree  = 0;
	sbp->f_bavail = 0;
	sbp->f_files  = 128;
	sbp->f_ffree  = 0;		/* what to put in here? */
	sbp->f_fsid.val[0] = (long)devfs_mp_p;
	sbp->f_fsid.val[1] = MOUNT_DEVFS;

/*-
 *  Copy the mounted on and mounted from names into
 *  the passed in stat block, if it is not the one
 *  in the mount structure.
 */
	if (sbp != &mp->mnt_stat) {
		bcopy((caddr_t)mp->mnt_stat.f_mntonname,
			(caddr_t)&sbp->f_mntonname[0], MNAMELEN);
		bcopy((caddr_t)mp->mnt_stat.f_mntfromname,
			(caddr_t)&sbp->f_mntfromname[0], MNAMELEN);
	}
	return 0;
}

/*-
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
static int
devfs_sync(struct mount *mp, int waitfor,struct ucred *cred,struct proc *p)
{
	register struct vnode *vp, *nvp;
	struct timeval tv;
	int error, allerror = 0;

DBPRINT(("sync "));

	/*-
	 * Write back modified superblock.
	 * Consistency check that the superblock
	 * is still in the buffer cache.
	 */
	/*-
	 * Write back each (modified) inode.
	 */
loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		/*-
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
		nvp = vp->v_mntvnodes.le_next;
		if (VOP_ISLOCKED(vp))
			continue;
		if ( vp->v_dirtyblkhd.lh_first == NULL)
			continue;
		if (vp->v_type == VBLK) {
			if (vget(vp, LK_EXCLUSIVE, p))
				goto loop;
			error = VOP_FSYNC(vp, cred, waitfor, p);
			if (error)
				allerror = error;
			vput(vp);
		}
#ifdef NOTYET
		else {
			tv = time;
			/* VOP_UPDATE(vp, &tv, &tv, waitfor == MNT_WAIT); */
			VOP_UPDATE(vp, &tv, &tv, 0);
		}
#endif
	}
	/*-
	 * Force stale file system control information to be flushed.
	 *( except that htat makes no sense with devfs
	 */
	return (allerror);
}

static int
devfs_vget(struct mount *mp, ino_t ino,struct vnode **vpp)
{
DBPRINT(("vget "));
	return EOPNOTSUPP;
}

/*************************************************************
 * The concept of exporting a kernel generated devfs is stupid
 * So don't handle filehandles
 */

static int
devfs_fhtovp (struct mount *mp, struct fid *fhp, struct sockaddr *nam,
	      struct vnode **vpp, int *exflagsp, struct ucred **credanonp)
{
DBPRINT(("fhtovp "));
	return (EINVAL);
}


static int
devfs_vptofh (struct vnode *vp, struct fid *fhp)
{
DBPRINT(("vptofh "));
	return (EINVAL);
}

static struct vfsops devfs_vfsops = {
	devfs_mount,
	devfs_start,
	devfs_unmount,
	devfs_root,
	devfs_quotactl,
	devfs_statfs,
	devfs_sync,
	devfs_vget,
	devfs_fhtovp,
	devfs_vptofh,
	devfs_init
};

VFS_SET(devfs_vfsops, devfs, MOUNT_DEVFS, 0);
