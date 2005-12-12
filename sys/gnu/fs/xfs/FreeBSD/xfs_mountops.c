#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include "xfs.h"
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_alloc.h"
#include "xfs_rtalloc.h"
#include "xfs_bmap.h"
#include "xfs_error.h"
#include "xfs_bit.h"
#include "xfs_rw.h"
#include "xfs_quota.h"
#include "xfs_fsops.h"
#include "xfs_clnt.h"

#include <xfs_mountops.h>

MALLOC_DEFINE(M_XFSNODE, "XFS node", "XFS vnode private part");

static vfs_mount_t	_xfs_mount;
static vfs_unmount_t	_xfs_unmount;
static vfs_root_t	_xfs_root;
static vfs_quotactl_t	_xfs_quotactl;
static vfs_statfs_t	_xfs_statfs;
static vfs_sync_t	_xfs_sync;
static vfs_vget_t	_xfs_vget;
static vfs_fhtovp_t	_xfs_fhtovp;
static vfs_checkexp_t	_xfs_checkexp;
static vfs_vptofh_t	_xfs_vptofh;
static vfs_init_t	_xfs_init;
static vfs_uninit_t	_xfs_uninit;
static vfs_extattrctl_t	_xfs_extattrctl;

static b_strategy_t	xfs_geom_strategy;

static const char *xfs_opts[] =
	{ "from", "flags", "logbufs", "logbufsize",
	  "rtname", "logname", "iosizelog", "sunit",
	  "swidth",
	  NULL };

static void
parse_int(struct mount *mp, const char *opt, int *val, int *error)
{
	char *tmp, *ep;

	tmp = vfs_getopts(mp->mnt_optnew, opt, error);
	if (*error != 0) {
		return;
	}
	if (tmp != NULL) {
		*val = (int)strtol(tmp, &ep, 10);
		if (*ep) {
			*error = EINVAL;
			return;
		}
	}
}

static int
_xfs_param_copyin(struct mount *mp, struct thread *td)
{
	struct xfsmount *xmp = MNTTOXFS(mp);
	struct xfs_mount_args *args = &xmp->m_args;
	char *path;
	char *fsname;
	char *rtname;
	char *logname;
	int error;

	path = vfs_getopts(mp->mnt_optnew, "fspath", &error);
	if  (error)
		return (error);

	bzero(args, sizeof(struct xfs_mount_args));
	args->logbufs = -1;
	args->logbufsize = -1;

	parse_int(mp, "flags", &args->flags, &error);
	if (error != 0)
		return error;

	args->flags |= XFSMNT_32BITINODES;

	parse_int(mp, "sunit", &args->sunit, &error);
	if (error != 0)
		return error;

	parse_int(mp, "swidth", &args->swidth, &error);
	if (error != 0)
		return error;

	parse_int(mp, "logbufs", &args->logbufs, &error);
	if (error != 0)
		return error;

	parse_int(mp, "logbufsize", &args->logbufsize, &error);
	if (error != 0)
		return error;

	fsname = vfs_getopts(mp->mnt_optnew, "from", &error);
	if (error == 0 && fsname != NULL) {
		strncpy(args->fsname, fsname, sizeof(args->fsname) - 1);
	}

	logname = vfs_getopts(mp->mnt_optnew, "logname", &error);
	if (error == 0 && logname != NULL) {
		strncpy(args->logname, logname, sizeof(args->logname) - 1);
	}

	rtname = vfs_getopts(mp->mnt_optnew, "rtname", &error);
	if (error == 0 && rtname != NULL) {
		strncpy(args->rtname, rtname, sizeof(args->rtname) - 1);
	}

	strncpy(args->mtpt, path, sizeof(args->mtpt));

	printf("fsname '%s' logname '%s' rtname '%s'\n"
	       "flags 0x%x sunit %d swidth %d logbufs %d logbufsize %d\n",
	       args->fsname, args->logname, args->rtname, args->flags,
	       args->sunit, args->swidth, args->logbufs, args->logbufsize);

	vfs_mountedfrom(mp, args->fsname);

	return (0);
}

static int
_xfs_mount(struct mount		*mp,
	   struct thread	*td)
{
	struct xfsmount		*xmp;
	struct xfs_vnode	*rootvp;
	struct ucred		*curcred;
	struct vnode		*rvp;
	struct cdev		*ddev;
	int			error;

	if (vfs_filteropt(mp->mnt_optnew, xfs_opts))
		return (EINVAL);

        xmp = xfsmount_allocate(mp);
        if (xmp == NULL)
                return (ENOMEM);

	if((error = _xfs_param_copyin(mp, td)) != 0)
		goto fail;

	/* Force read-only mounts in this branch. */
	XFSTOVFS(xmp)->vfs_flag |= VFS_RDONLY;
        mp->mnt_flag |= MNT_RDONLY;

	/* XXX: Do not support MNT_UPDATE yet */
	if (mp->mnt_flag & MNT_UPDATE)
		return EOPNOTSUPP; 

	curcred = td->td_ucred;
	XVFS_MOUNT(XFSTOVFS(xmp), &xmp->m_args, curcred, error);
	if (error)
		goto fail;

 	XVFS_ROOT(XFSTOVFS(xmp), &rootvp, error);
	if (error)
		goto fail_unmount;

	ddev = XFS_VFSTOM(XFSTOVFS(xmp))->m_dev;
 	if (ddev->si_iosize_max != 0)
		mp->mnt_iosize_max = ddev->si_iosize_max;
        if (mp->mnt_iosize_max > MAXPHYS)
		mp->mnt_iosize_max = MAXPHYS;

        mp->mnt_flag |= MNT_LOCAL | MNT_RDONLY;
        mp->mnt_stat.f_fsid.val[0] = dev2udev(ddev);
        mp->mnt_stat.f_fsid.val[1] = mp->mnt_vfc->vfc_typenum;

        VFS_STATFS(mp, &mp->mnt_stat, td);
        if (error)
		goto fail_unmount;

	rvp = rootvp->v_vnode;
	rvp->v_vflag |= VV_ROOT;
	VN_RELE(rootvp);

	return (0);

 fail_unmount:
	XVFS_UNMOUNT(XFSTOVFS(xmp), 0, curcred, error);

 fail:
	if (xmp != NULL)
		xfsmount_deallocate(xmp);

	return (error);
}

/*
 * Free reference to null layer
 */
static int
_xfs_unmount(mp, mntflags, td)
	struct mount *mp;
	int mntflags;
	struct thread *td;
{
	int error;

	XVFS_UNMOUNT(MNTTOVFS(mp), 0, td->td_ucred, error);
	return (error);
}

static int
_xfs_root(mp, flags, vpp, td)
	struct mount *mp;
	int flags;
	struct vnode **vpp;
	struct thread *td;
{
	xfs_vnode_t *vp;
	int error;

        XVFS_ROOT(MNTTOVFS(mp), &vp, error);
	if (error == 0) {
		*vpp = vp->v_vnode;
		VOP_LOCK(*vpp, flags, curthread);
	}
	return (error);
}

static int
_xfs_quotactl(mp, cmd, uid, arg, td)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct thread *td;
{
	printf("xfs_quotactl\n");
	return ENOSYS;
}

static int
_xfs_statfs(mp, sbp, td)
	struct mount *mp;
	struct statfs *sbp;
	struct thread *td;
{
	int error;

        XVFS_STATVFS(MNTTOVFS(mp), sbp, NULL, error);
        if (error)
		return error;

	/* Fix up the values XFS statvfs calls does not know about. */
	sbp->f_iosize = sbp->f_bsize;

	return (error);
}

static int
_xfs_sync(mp, waitfor, td)
	struct mount *mp;
	int waitfor;
	struct thread *td;
{
	int error;
	int flags = SYNC_FSDATA|SYNC_ATTR|SYNC_REFCACHE;

	if (waitfor == MNT_WAIT)
		flags |= SYNC_WAIT;
	else if (waitfor == MNT_LAZY)
		flags |= SYNC_BDFLUSH;
        XVFS_SYNC(MNTTOVFS(mp), flags, td->td_ucred, error);
	return (error);
}

static int
_xfs_vget(mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{
	xfs_vnode_t *vp;
	int error;

	printf("XVFS_GET_VNODE(MNTTOVFS(mp), &vp, ino, error);\n");
	error = ENOSYS;
	if (error == 0)
		*vpp = vp->v_vnode;
	return (error);
}

static int
_xfs_fhtovp(mp, fidp, vpp)
	struct mount *mp;
	struct fid *fidp;
	struct vnode **vpp;
{
	printf("xfs_fhtovp\n");
	return ENOSYS;
}

static int
_xfs_checkexp(mp, nam, extflagsp, credanonp)
	struct mount *mp;
	struct sockaddr *nam;
	int *extflagsp;
	struct ucred **credanonp;
{
	printf("xfs_checkexp\n");
	return ENOSYS;
}

static int
_xfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	printf("xfs_vptofh");
	return ENOSYS;
}

static int
_xfs_extattrctl(struct mount *mp, int cm,
                struct vnode *filename_v,
                int attrnamespace, const char *attrname,
                struct thread *td)
{
	printf("xfs_extattrctl\n");
	return ENOSYS;
}

int
_xfs_init(vfsp)
	struct vfsconf *vfsp;
{
	int error;

	error = init_xfs_fs();

	return (error);
}

int
_xfs_uninit(vfsp)
	struct vfsconf *vfsp;
{
	exit_xfs_fs();
	return 0;
}

static struct vfsops xfs_fsops = {
	.vfs_mount =	_xfs_mount,
	.vfs_unmount =	_xfs_unmount,
	.vfs_root =	_xfs_root,
	.vfs_quotactl = _xfs_quotactl,
	.vfs_statfs =	_xfs_statfs,
	.vfs_sync =	_xfs_sync,
	.vfs_vget =	_xfs_vget,
	.vfs_fhtovp =	_xfs_fhtovp,
	.vfs_checkexp =	_xfs_checkexp,
	.vfs_vptofh =	_xfs_vptofh,
	.vfs_init =	_xfs_init,
	.vfs_uninit =	_xfs_uninit,
	.vfs_extattrctl = _xfs_extattrctl,
};

/* XXX: Read-only for now */
VFS_SET(xfs_fsops, xfs, VFCF_READONLY);

/*
 *  Copy GEOM VFS functions here to provide a conveniet place to
 *  track all XFS-related IO without being distracted by other
 *  filesystems which happen to be mounted on the machine at the
 *  same time.
 */

static void
xfs_geom_biodone(struct bio *bip)
{
	struct buf *bp;

	if (bip->bio_error) {
		printf("g_vfs_done():");
		g_print_bio(bip);
		printf("error = %d\n", bip->bio_error);
	}
	bp = bip->bio_caller2;
	bp->b_error = bip->bio_error;
	bp->b_ioflags = bip->bio_flags;
	if (bip->bio_error)
		bp->b_ioflags |= BIO_ERROR;
	bp->b_resid = bp->b_bcount - bip->bio_completed;
	g_destroy_bio(bip);
	mtx_lock(&Giant);
	bufdone(bp);
	mtx_unlock(&Giant);
}

static void
xfs_geom_strategy(struct bufobj *bo, struct buf *bp)
{
	struct g_consumer *cp;
	struct bio *bip;

	cp = bo->bo_private;
	G_VALID_CONSUMER(cp);

	bip = g_alloc_bio();
	bip->bio_cmd = bp->b_iocmd;
	bip->bio_offset = bp->b_iooffset;
	bip->bio_data = bp->b_data;
	bip->bio_done = xfs_geom_biodone;
	bip->bio_caller2 = bp;
	bip->bio_length = bp->b_bcount;
	g_io_request(bip, cp);
}

static int
xfs_geom_bufwrite(struct buf *bp)
{
	return bufwrite(bp);
}

struct buf_ops xfs_ops = {
	.bop_name =     "XFS",
	.bop_write =    xfs_geom_bufwrite,
	.bop_strategy = xfs_geom_strategy,
	.bop_sync =     bufsync,
};
