/*
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
#include "opt_netsmb.h"
#ifndef NETSMB
#error "SMBFS requires option NETSMB"
#endif

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


#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

int smbfs_debuglevel = 0;

static int smbfs_version = SMBFS_VERSION;

#ifdef SMBFS_USEZONE
#include <vm/vm.h>
#include <vm/vm_extern.h>

vm_zone_t smbfsmount_zone;
#endif

SYSCTL_NODE(_vfs, OID_AUTO, smbfs, CTLFLAG_RW, 0, "SMB/CIFS file system");
SYSCTL_INT(_vfs_smbfs, OID_AUTO, version, CTLFLAG_RD, &smbfs_version, 0, "");
SYSCTL_INT(_vfs_smbfs, OID_AUTO, debuglevel, CTLFLAG_RW, &smbfs_debuglevel, 0, "");

static MALLOC_DEFINE(M_SMBFSHASH, "SMBFS hash", "SMBFS hash table");


static int smbfs_mount(struct mount *, char *, caddr_t,
			struct nameidata *, struct thread *);
static int smbfs_quotactl(struct mount *, int, uid_t, caddr_t, struct thread *);
static int smbfs_root(struct mount *, struct vnode **);
static int smbfs_start(struct mount *, int, struct thread *);
static int smbfs_statfs(struct mount *, struct statfs *, struct thread *);
static int smbfs_sync(struct mount *, int, struct ucred *, struct thread *);
static int smbfs_unmount(struct mount *, int, struct thread *);
static int smbfs_init(struct vfsconf *vfsp);
static int smbfs_uninit(struct vfsconf *vfsp);

#if __FreeBSD_version < 400009
static int smbfs_vget(struct mount *mp, ino_t ino, int flags,
			struct vnode **vpp);
static int smbfs_fhtovp(struct mount *, struct fid *,
			struct sockaddr *, struct vnode **, int *,
			struct ucred **);
static int smbfs_vptofh(struct vnode *, struct fid *);
#endif

static struct vfsops smbfs_vfsops = {
	smbfs_mount,
	smbfs_start,
	smbfs_unmount,
	smbfs_root,
	smbfs_quotactl,
	smbfs_statfs,
	smbfs_sync,
#if __FreeBSD_version > 400008
	vfs_stdvget,
	vfs_stdfhtovp,		/* shouldn't happen */
	vfs_stdcheckexp,
	vfs_stdvptofh,		/* shouldn't happen */
#else
	smbfs_vget,
	smbfs_fhtovp,
	smbfs_vptofh,
#endif
	smbfs_init,
	smbfs_uninit,
#ifndef FB_RELENG3
	vfs_stdextattrctl,
#else
#define	M_USE_RESERVE	M_KERNEL
	&sysctl___vfs_smbfs
#endif
};


VFS_SET(smbfs_vfsops, smbfs, VFCF_NETWORK);

MODULE_DEPEND(smbfs, netsmb, NSMB_VERSION, NSMB_VERSION, NSMB_VERSION);
MODULE_DEPEND(smbfs, libiconv, 1, 1, 1);
MODULE_DEPEND(smbfs, libmchain, 1, 1, 1);

int smbfs_pbuf_freecnt = -1;	/* start out unlimited */

static int
smbfs_mount(struct mount *mp, char *path, caddr_t data, 
	struct nameidata *ndp, struct thread *td)
{
	struct smbfs_args args; 	  /* will hold data from mount request */
	struct smbmount *smp = NULL;
	struct smb_vc *vcp;
	struct smb_share *ssp = NULL;
	struct vnode *vp;
	struct smb_cred scred;
#ifndef	FB_CURRENT
	size_t size;
#endif
	int error;
	char *pc, *pe;

	if (data == NULL) {
		printf("missing data argument\n");
		return EINVAL;
	}
	if (mp->mnt_flag & MNT_UPDATE) {
		printf("MNT_UPDATE not implemented");
		return EOPNOTSUPP;
	}
	error = copyin(data, (caddr_t)&args, sizeof(struct smbfs_args));
	if (error)
		return error;
	if (args.version != SMBFS_VERSION) {
		printf("mount version mismatch: kernel=%d, mount=%d\n",
		    SMBFS_VERSION, args.version);
		return EINVAL;
	}
	smb_makescred(&scred, td, td->td_ucred);
	error = smb_dev2share(args.dev, SMBM_EXEC, &scred, &ssp);
	if (error) {
		printf("invalid device handle %d (%d)\n", args.dev, error);
		return error;
	}
	vcp = SSTOVC(ssp);
	smb_share_unlock(ssp, 0, td);
	mp->mnt_stat.f_iosize = SSTOVC(ssp)->vc_txmax;

#ifdef SMBFS_USEZONE
	smp = zalloc(smbfsmount_zone);
#else
        MALLOC(smp, struct smbmount*, sizeof(*smp), M_SMBFSDATA, M_USE_RESERVE);
#endif
        if (smp == NULL) {
                printf("could not alloc smbmount\n");
                error = ENOMEM;
		goto bad;
        }
	bzero(smp, sizeof(*smp));
        mp->mnt_data = (qaddr_t)smp;
	smp->sm_hash = hashinit(desiredvnodes, M_SMBFSHASH, &smp->sm_hashlen);
	if (smp->sm_hash == NULL)
		goto bad;
	lockinit(&smp->sm_hashlock, PVFS, "smbfsh", 0, 0);
	smp->sm_share = ssp;
	smp->sm_root = NULL;
        smp->sm_args = args;
	smp->sm_caseopt = args.caseopt;
	smp->sm_args.file_mode = (smp->sm_args.file_mode &
			    (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFREG;
	smp->sm_args.dir_mode  = (smp->sm_args.dir_mode &
			    (S_IRWXU|S_IRWXG|S_IRWXO)) | S_IFDIR;

/*	simple_lock_init(&smp->sm_npslock);*/
#ifndef	FB_CURRENT
	error = copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	if (error)
		goto bad;
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
#endif
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
	/* protect against invalid mount points */
	smp->sm_args.mount_point[sizeof(smp->sm_args.mount_point) - 1] = '\0';
	vfs_getnewfsid(mp);
	error = smbfs_root(mp, &vp);
	if (error)
		goto bad;
	VOP_UNLOCK(vp, 0, td);
	SMBVDEBUG("root.v_usecount = %d\n", vp->v_usecount);

#ifdef DIAGNOSTICS
	SMBERROR("mp=%p\n", mp);
#endif
	return error;
bad:
        if (smp) {
		if (smp->sm_hash)
			free(smp->sm_hash, M_SMBFSHASH);
		lockdestroy(&smp->sm_hashlock);
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
	/* There is 1 extra root vnode reference from smbfs_mount(). */
	error = vflush(mp, 1, flags);
	if (error)
		return error;
	smb_makescred(&scred, td, td->td_ucred);
	smb_share_put(smp->sm_share, &scred);
	mp->mnt_data = (qaddr_t)0;

	if (smp->sm_hash)
		free(smp->sm_hash, M_SMBFSHASH);
	lockdestroy(&smp->sm_hashlock);
#ifdef SMBFS_USEZONE
	zfree(smbfsmount_zone, smp);
#else
	free(smp, M_SMBFSDATA);
#endif
	mp->mnt_flag &= ~MNT_LOCAL;
	return error;
}

/* 
 * Return locked root vnode of a filesystem
 */
static int
smbfs_root(struct mount *mp, struct vnode **vpp)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct vnode *vp;
	struct smbnode *np;
	struct smbfattr fattr;
	struct thread *td = curthread;
	struct ucred *cred = td->td_ucred;
	struct smb_cred scred;
	int error;

	if (smp == NULL) {
		SMBERROR("smp == NULL (bug in umount)\n");
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
	vp->v_flag |= VROOT;
	np = VTOSMB(vp);
	smp->sm_root = np;
	*vpp = vp;
	return 0;
}

/*
 * Vfs start routine, a no-op.
 */
/* ARGSUSED */
static int
smbfs_start(mp, flags, td)
	struct mount *mp;
	int flags;
	struct thread *td;
{
	SMBVDEBUG("flags=%04x\n", flags);
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
	caddr_t arg;
	struct thread *td;
{
	SMBVDEBUG("return EOPNOTSUPP\n");
	return EOPNOTSUPP;
}

/*ARGSUSED*/
int
smbfs_init(struct vfsconf *vfsp)
{
#ifndef SMP
	int name[2];
	int olen, ncpu, plen, error;

	name[0] = CTL_HW;
	name[1] = HW_NCPU;
	error = kernel_sysctl(curthread, name, 2, &ncpu, &olen, NULL, 0, &plen);
	if (error == 0 && ncpu > 1)
		printf("warning: smbfs module compiled without SMP support.");
#endif

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

	if (np == NULL)
		return EINVAL;
	
	sbp->f_iosize = SSTOVC(ssp)->vc_txmax;		/* optimal transfer block size */
	sbp->f_spare2 = 0;			/* placeholder */
	smb_makescred(&scred, td, td->td_ucred);

	if (SMB_DIALECT(SSTOVC(ssp)) >= SMB_DIALECT_LANMAN2_0)
		error = smbfs_smb_statfs2(ssp, sbp, &scred);
	else
		error = smbfs_smb_statfs(ssp, sbp, &scred);
	if (error)
		return error;
	sbp->f_flags = 0;		/* copy of mount exported flags */
	if (sbp != &mp->mnt_stat) {
		sbp->f_fsid = mp->mnt_stat.f_fsid;	/* file system id */
		sbp->f_owner = mp->mnt_stat.f_owner;	/* user that mounted the filesystem */
		sbp->f_type = mp->mnt_vfc->vfc_typenum;	/* type of filesystem */
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	strncpy(sbp->f_fstypename, mp->mnt_vfc->vfc_name, MFSNAMELEN);
	return 0;
}

/*
 * Flush out the buffer cache
 */
/* ARGSUSED */
static int
smbfs_sync(mp, waitfor, cred, td)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct thread *td;
{
	struct vnode *vp;
	int error, allerror = 0;
	/*
	 * Force stale buffer cache information to be flushed.
	 */
loop:
	for (vp = TAILQ_FIRST(&mp->mnt_nvnodelist);
	     vp != NULL;
	     vp = TAILQ_NEXT(vp, v_nmntvnodes)) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
#ifndef FB_RELENG3
		if (VOP_ISLOCKED(vp, NULL) || TAILQ_EMPTY(&vp->v_dirtyblkhd) ||
#else
		if (VOP_ISLOCKED(vp) || TAILQ_EMPTY(&vp->v_dirtyblkhd) ||
#endif
		    waitfor == MNT_LAZY)
			continue;
		if (vget(vp, LK_EXCLUSIVE, td))
			goto loop;
		error = VOP_FSYNC(vp, cred, waitfor, td);
		if (error)
			allerror = error;
		vput(vp);
	}
	return (allerror);
}

#if __FreeBSD_version < 400009
/*
 * smbfs flat namespace lookup. Unsupported.
 */
/* ARGSUSED */
static int smbfs_vget(mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{
	return (EOPNOTSUPP);
}

/* ARGSUSED */
static int smbfs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	struct mount *mp;
	struct fid *fhp;
	struct sockaddr *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{
	return (EINVAL);
}

/*
 * Vnode pointer to File handle, should never happen either
 */
/* ARGSUSED */
static int
smbfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{
	return (EINVAL);
}

#endif /* __FreeBSD_version < 400009 */
