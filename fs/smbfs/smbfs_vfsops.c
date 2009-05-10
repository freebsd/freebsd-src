/*-
 * Copyright (c) 2000-2001, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sx.h>


#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

static int smbfs_debuglevel = 0;

static int smbfs_version = SMBFS_VERSION;

#ifdef SMBFS_USEZONE
#include <vm/vm.h>
#include <vm/vm_extern.h>

vm_zone_t smbfsmount_zone;
#endif

SYSCTL_NODE(_vfs, OID_AUTO, smbfs, CTLFLAG_RW, 0, "SMB/CIFS filesystem");
SYSCTL_INT(_vfs_smbfs, OID_AUTO, version, CTLFLAG_RD, &smbfs_version, 0, "");
SYSCTL_INT(_vfs_smbfs, OID_AUTO, debuglevel, CTLFLAG_RW, &smbfs_debuglevel, 0, "");

static MALLOC_DEFINE(M_SMBFSHASH, "smbfs_hash", "SMBFS hash table");

static vfs_init_t       smbfs_init;
static vfs_uninit_t     smbfs_uninit;
static vfs_cmount_t     smbfs_cmount;
static vfs_mount_t      smbfs_mount;
static vfs_root_t       smbfs_root;
static vfs_quotactl_t   smbfs_quotactl;
static vfs_statfs_t     smbfs_statfs;
static vfs_unmount_t    smbfs_unmount;

static struct vfsops smbfs_vfsops = {
	.vfs_init =		smbfs_init,
	.vfs_cmount =		smbfs_cmount,
	.vfs_mount =		smbfs_mount,
	.vfs_quotactl =		smbfs_quotactl,
	.vfs_root =		smbfs_root,
	.vfs_statfs =		smbfs_statfs,
	.vfs_sync =		vfs_stdsync,
	.vfs_uninit =		smbfs_uninit,
	.vfs_unmount =		smbfs_unmount,
};


VFS_SET(smbfs_vfsops, smbfs, VFCF_NETWORK);

MODULE_DEPEND(smbfs, netsmb, NSMB_VERSION, NSMB_VERSION, NSMB_VERSION);
MODULE_DEPEND(smbfs, libiconv, 1, 1, 2);
MODULE_DEPEND(smbfs, libmchain, 1, 1, 1);

int smbfs_pbuf_freecnt = -1;	/* start out unlimited */

static int
smbfs_cmount(struct mntarg *ma, void * data, int flags, struct thread *td)
{
	struct smbfs_args args;
	int error;

	error = copyin(data, &args, sizeof(struct smbfs_args));
	if (error)
		return error;

	if (args.version != SMBFS_VERSION) {
		printf("mount version mismatch: kernel=%d, mount=%d\n",
		    SMBFS_VERSION, args.version);
		return EINVAL;
	}
	ma = mount_argf(ma, "dev", "%d", args.dev);
	ma = mount_argb(ma, args.flags & SMBFS_MOUNT_SOFT, "nosoft");
	ma = mount_argb(ma, args.flags & SMBFS_MOUNT_INTR, "nointr");
	ma = mount_argb(ma, args.flags & SMBFS_MOUNT_STRONG, "nostrong");
	ma = mount_argb(ma, args.flags & SMBFS_MOUNT_HAVE_NLS, "nohave_nls");
	ma = mount_argb(ma, !(args.flags & SMBFS_MOUNT_NO_LONG), "nolong");
	ma = mount_arg(ma, "rootpath", args.root_path, -1);
	ma = mount_argf(ma, "uid", "%d", args.uid);
	ma = mount_argf(ma, "gid", "%d", args.gid);
	ma = mount_argf(ma, "file_mode", "%d", args.file_mode);
	ma = mount_argf(ma, "dir_mode", "%d", args.dir_mode);
	ma = mount_argf(ma, "caseopt", "%d", args.caseopt);

	error = kernel_mount(ma, flags);

	return (error);
}

static const char *smbfs_opts[] = {
	"dev", "soft", "intr", "strong", "have_nls", "long",
	"mountpoint", "rootpath", "uid", "gid", "file_mode", "dir_mode",
	"caseopt", "errmsg", NULL
};

static int
smbfs_mount(struct mount *mp, struct thread *td)
{
	struct smbmount *smp = NULL;
	struct smb_vc *vcp;
	struct smb_share *ssp = NULL;
	struct vnode *vp;
	struct smb_cred scred;
	int error, v;
	char *pc, *pe;

	if (mp->mnt_flag & (MNT_UPDATE | MNT_ROOTFS))
		return EOPNOTSUPP;

	if (vfs_filteropt(mp->mnt_optnew, smbfs_opts)) {
		vfs_mount_error(mp, "%s", "Invalid option");
		return (EINVAL);
	}

	smb_makescred(&scred, td, td->td_ucred);
	if (1 != vfs_scanopt(mp->mnt_optnew, "dev", "%d", &v)) {
		vfs_mount_error(mp, "No dev option");
		return (EINVAL);
	}
	error = smb_dev2share(v, SMBM_EXEC, &scred, &ssp);
	if (error) {
		printf("invalid device handle %d (%d)\n", v, error);
		vfs_mount_error(mp, "invalid device handle %d (%d)\n", v, error);
		return error;
	}
	vcp = SSTOVC(ssp);
	smb_share_unlock(ssp, 0);
	mp->mnt_stat.f_iosize = SSTOVC(ssp)->vc_txmax;

#ifdef SMBFS_USEZONE
	smp = zalloc(smbfsmount_zone);
#else
	smp = malloc(sizeof(*smp), M_SMBFSDATA,
	    M_WAITOK|M_USE_RESERVE);
#endif
        if (smp == NULL) {
		printf("could not alloc smbmount\n");
		vfs_mount_error(mp, "could not alloc smbmount", v, error);
		error = ENOMEM;
		goto bad;
        }
	bzero(smp, sizeof(*smp));
        mp->mnt_data = smp;
	smp->sm_hash = hashinit(desiredvnodes, M_SMBFSHASH, &smp->sm_hashlen);
	if (smp->sm_hash == NULL)
		goto bad;
	sx_init(&smp->sm_hashlock, "smbfsh");
	smp->sm_share = ssp;
	smp->sm_root = NULL;
	if (1 != vfs_scanopt(mp->mnt_optnew,
	    "caseopt", "%d", &smp->sm_caseopt)) {
		vfs_mount_error(mp, "Invalid caseopt");
		error = EINVAL;
		goto bad;
	}
	if (1 != vfs_scanopt(mp->mnt_optnew, "uid", "%d", &v)) {
		vfs_mount_error(mp, "Invalid uid");
		error = EINVAL;
		goto bad;
	}
	smp->sm_uid = v;

	if (1 != vfs_scanopt(mp->mnt_optnew, "gid", "%d", &v)) {
		vfs_mount_error(mp, "Invalid gid");
		error = EINVAL;
		goto bad;
	}
	smp->sm_gid = v;

	if (1 != vfs_scanopt(mp->mnt_optnew, "file_mode", "%d", &v)) {
		vfs_mount_error(mp, "Invalid file_mode");
		error = EINVAL;
		goto bad;
	}
	smp->sm_file_mode = (v & (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFREG;

	if (1 != vfs_scanopt(mp->mnt_optnew, "dir_mode", "%d", &v)) {
		vfs_mount_error(mp, "Invalid dir_mode");
		error = EINVAL;
		goto bad;
	}
	smp->sm_dir_mode  = (v & (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFDIR;

	vfs_flagopt(mp->mnt_optnew,
	    "nolong", &smp->sm_flags, SMBFS_MOUNT_NO_LONG);

/*	simple_lock_init(&smp->sm_npslock);*/
	pc = mp->mnt_stat.f_mntfromname;
	pe = pc + sizeof(mp->mnt_stat.f_mntfromname);
	bzero(pc, MNAMELEN);
	*pc++ = '/';
	*pc++ = '/';
	pc=index(strncpy(pc, vcp->vc_username, pe - pc - 2), 0);
	if (pc < pe-1) {
		*(pc++) = '@';
		pc = index(strncpy(pc, vcp->vc_srvname, pe - pc - 2), 0);
		if (pc < pe - 1) {
			*(pc++) = '/';
			strncpy(pc, ssp->ss_name, pe - pc - 2);
		}
	}
	vfs_getnewfsid(mp);
	error = smbfs_root(mp, LK_EXCLUSIVE, &vp, td);
	if (error) {
		vfs_mount_error(mp, "smbfs_root error: %d", error);
		goto bad;
	}
	VOP_UNLOCK(vp, 0);
	SMBVDEBUG("root.v_usecount = %d\n", vrefcnt(vp));

#ifdef DIAGNOSTICS
	SMBERROR("mp=%p\n", mp);
#endif
	return error;
bad:
        if (smp) {
		if (smp->sm_hash)
			free(smp->sm_hash, M_SMBFSHASH);
		sx_destroy(&smp->sm_hashlock);
#ifdef SMBFS_USEZONE
		zfree(smbfsmount_zone, smp);
#else
		free(smp, M_SMBFSDATA);
#endif
	}
	if (ssp)
		smb_share_put(ssp, &scred);
        return error;
}

/* Unmount the filesystem described by mp. */
static int
smbfs_unmount(struct mount *mp, int mntflags, struct thread *td)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smb_cred scred;
	int error, flags;

	SMBVDEBUG("smbfs_unmount: flags=%04x\n", mntflags);
	flags = 0;
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	/*
	 * Keep trying to flush the vnode list for the mount while 
	 * some are still busy and we are making progress towards
	 * making them not busy. This is needed because smbfs vnodes
	 * reference their parent directory but may appear after their
	 * parent in the list; one pass over the vnode list is not
	 * sufficient in this case.
	 */
	do {
		smp->sm_didrele = 0;
		/* There is 1 extra root vnode reference from smbfs_mount(). */
		error = vflush(mp, 1, flags, td);
	} while (error == EBUSY && smp->sm_didrele != 0);
	if (error)
		return error;
	smb_makescred(&scred, td, td->td_ucred);
	error = smb_share_lock(smp->sm_share, LK_EXCLUSIVE);
	if (error)
		return error;
	smb_share_put(smp->sm_share, &scred);
	mp->mnt_data = NULL;

	if (smp->sm_hash)
		free(smp->sm_hash, M_SMBFSHASH);
	sx_destroy(&smp->sm_hashlock);
#ifdef SMBFS_USEZONE
	zfree(smbfsmount_zone, smp);
#else
	free(smp, M_SMBFSDATA);
#endif
	MNT_ILOCK(mp);
	mp->mnt_flag &= ~MNT_LOCAL;
	MNT_IUNLOCK(mp);
	return error;
}

/* 
 * Return locked root vnode of a filesystem
 */
static int
smbfs_root(struct mount *mp, int flags, struct vnode **vpp, struct thread *td)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct vnode *vp;
	struct smbnode *np;
	struct smbfattr fattr;
	struct ucred *cred = td->td_ucred;
	struct smb_cred scred;
	int error;

	if (smp == NULL) {
		SMBERROR("smp == NULL (bug in umount)\n");
		vfs_mount_error(mp, "smp == NULL (bug in umount)");
		return EINVAL;
	}
	if (smp->sm_root) {
		*vpp = SMBTOV(smp->sm_root);
		return vget(*vpp, LK_EXCLUSIVE | LK_RETRY, td);
	}
	smb_makescred(&scred, td, cred);
	error = smbfs_smb_lookup(NULL, NULL, 0, &fattr, &scred);
	if (error)
		return error;
	error = smbfs_nget(mp, NULL, "TheRooT", 7, &fattr, &vp);
	if (error)
		return error;
	ASSERT_VOP_LOCKED(vp, "smbfs_root");
	vp->v_vflag |= VV_ROOT;
	np = VTOSMB(vp);
	smp->sm_root = np;
	*vpp = vp;
	return 0;
}

/*
 * Do operations associated with quotas, not supported
 */
/* ARGSUSED */
static int
smbfs_quotactl(mp, cmd, uid, arg, td)
	struct mount *mp;
	int cmd;
	uid_t uid;
	void *arg;
	struct thread *td;
{
	SMBVDEBUG("return EOPNOTSUPP\n");
	return EOPNOTSUPP;
}

/*ARGSUSED*/
int
smbfs_init(struct vfsconf *vfsp)
{
#ifdef SMBFS_USEZONE
	smbfsmount_zone = zinit("SMBFSMOUNT", sizeof(struct smbmount), 0, 0, 1);
#endif
	smbfs_pbuf_freecnt = nswbuf / 2 + 1;
	SMBVDEBUG("done.\n");
	return 0;
}

/*ARGSUSED*/
int
smbfs_uninit(struct vfsconf *vfsp)
{

	SMBVDEBUG("done.\n");
	return 0;
}

/*
 * smbfs_statfs call
 */
int
smbfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smbnode *np = smp->sm_root;
	struct smb_share *ssp = smp->sm_share;
	struct smb_cred scred;
	int error = 0;

	if (np == NULL) {
		vfs_mount_error(mp, "np == NULL");
		return EINVAL;
	}
	
	sbp->f_iosize = SSTOVC(ssp)->vc_txmax;		/* optimal transfer block size */
	smb_makescred(&scred, td, td->td_ucred);

	if (SMB_DIALECT(SSTOVC(ssp)) >= SMB_DIALECT_LANMAN2_0)
		error = smbfs_smb_statfs2(ssp, sbp, &scred);
	else
		error = smbfs_smb_statfs(ssp, sbp, &scred);
	if (error)
		return error;
	sbp->f_flags = 0;		/* copy of mount exported flags */
	return 0;
}
