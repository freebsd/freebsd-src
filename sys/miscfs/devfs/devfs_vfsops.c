/*
 *  Written by Julian Elischer (julian@DIALix.oz.au)
 *
 *	$Header: /home/ncvs/src/sys/miscfs/devfs/devfs_vfsops.c,v 1.11 1996/04/07 01:15:02 joerg Exp $
 *
 *
 */

#include "param.h"
#include "systm.h"
#include "namei.h"
#include "proc.h"
#include "kernel.h"
#include "vnode.h"
#include "miscfs/specfs/specdev.h"	/* defines v_rdev	*/
#include "mount.h"
#include "buf.h"
#include "file.h"
#include "malloc.h"
#include "devfsdefs.h"

static int devfs_statfs( struct mount *mp, struct statfs *sbp, struct proc *p);
static int mountdevfs( struct mount *mp, struct proc *p);

static int
devfs_init(void)
{
	/*
	 * fill in the missing members on the "hidden" mount
	 */
	dev_root->dnp->dvm->mount->mnt_op  = vfssw[MOUNT_DEVFS]; 
	dev_root->dnp->dvm->mount->mnt_vfc = vfsconf[MOUNT_DEVFS];

	/* Mark a reference for the "invisible" blueprint mount */
	dev_root->dnp->dvm->mount->mnt_vfc->vfc_refcount++;

	printf("devfs ready to run\n");
	return 0; /*XXX*/
}

/*
 *  mp	 - pointer to 'mount' structure
 *  path - addr in user space of mount point (ie /usr or whatever)
 *  data - addr in user space of mount params including the
 *         name of the block special file to treat as a filesystem.
 *  ndp  - namei data pointer
 *  p    - proc pointer
 * devfs is special in that it doesn't require any device to be mounted..
 * It makes up it's data as it goes along.
 * it must be mounted during single user.. until it is, only std{in/out/err}
 * and the root filesystem are available.
 */
/*proto*/
int
devfs_mount(struct mount *mp, char *path, caddr_t data,
	    struct nameidata *ndp, struct proc *p)
{
	struct devfsmount *devfs_mp_p;	/* devfs specific mount control block	*/
	int error;
	u_int size;

DBPRINT(("mount "));
/*
 *  If they just want to update, we don't need to do anything.
 */
	if (mp->mnt_flag & MNT_UPDATE)
	{
		return 0;
	}

/*
 *  Well, it's not an update, it's a real mount request.
 *  Time to get dirty.
 * HERE we should check to see if we are already mounted here.
 */
	if(error =  mountdevfs( mp, p))
		return (error);

/*
 *  Copy in the name of the directory the filesystem
 *  is to be mounted on.
 *  And we clear the remainder of the character strings
 *  to be tidy.
 *  Then, we try to fill in the filesystem stats structure
 *  as best we can with whatever we can think of at the time
 */
	devfs_mp_p = (struct devfsmount *)mp->mnt_data;
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
mountdevfs( struct mount *mp, struct proc *p)
{
	int error = 0;
	struct devfsmount *devfs_mp_p;


	devfs_mp_p = (struct devfsmount *)malloc(sizeof *devfs_mp_p,
						M_DEVFSMNT, M_WAITOK);
	bzero(devfs_mp_p,sizeof(*devfs_mp_p));
	devfs_mp_p->mount = mp;

/*
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
	}
	return error;
}

static int
devfs_start(struct mount *mp, int flags, struct proc *p)
{
DBPRINT(("start "));
	return 0;
}

/*
 *  Unmount the filesystem described by mp.
 * Note: vnodes from this FS may hang around if being used..
 * This should not be a problem, they should be self contained.
 */
static int
devfs_unmount( struct mount *mp, int mntflags, struct proc *p)
{
	struct devfsmount *devfs_mp_p = (struct devfsmount *)mp->mnt_data;

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

/*
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

/*
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

static int
devfs_sync(struct mount *mp, int waitfor,struct ucred *cred,struct proc *p)
{
DBPRINT(("sync "));
	return 0;
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
devfs_fhtovp (struct mount *mp, struct fid *fhp, struct mbuf *nam,
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
