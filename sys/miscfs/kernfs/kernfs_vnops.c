/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)kernfs_vnops.c	8.6 (Berkeley) 2/10/94
 * $Id: kernfs_vnops.c,v 1.11 1995/10/29 15:31:37 phk Exp $
 */

/*
 * Kernel parameter filesystem (/kern)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vmmeter.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <miscfs/kernfs/kernfs.h>

#define KSTRING	256		/* Largest I/O available via this filesystem */
#define	UIO_MX 32

#define	READ_MODE	(S_IRUSR|S_IRGRP|S_IROTH)
#define	WRITE_MODE	(S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH)
#define DIR_MODE	(S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

static struct kern_target {
	char *kt_name;
	void *kt_data;
#define	KTT_NULL 1
#define	KTT_TIME 5
#define KTT_INT	17
#define	KTT_STRING 31
#define KTT_HOSTNAME 47
#define KTT_BOOTFILE 49
#define KTT_AVENRUN 53
	int kt_tag;
	int kt_rw;
	int kt_vtype;
	struct vnode **kt_vp;
} kern_targets[] = {
/* NOTE: The name must be less than UIO_MX-16 chars in length */
	/* name		data		tag		ro/rw 	type	vnodep*/
	{ ".",		0,		KTT_NULL,	VREAD,	VDIR,	NULL },
	{ "..",		0,		KTT_NULL,	VREAD,	VDIR,	NULL },
	{ "boottime",	&boottime.tv_sec, KTT_INT,	VREAD,	VREG,	NULL },
	{ "copyright",	copyright,	KTT_STRING,	VREAD,	VREG,	NULL },
	{ "hostname",	0,		KTT_HOSTNAME,VREAD|VWRITE,VREG, NULL },
	{ "bootfile",	0,		KTT_BOOTFILE,	VREAD,	VREG,	NULL },
	{ "hz",		&hz,		KTT_INT,	VREAD,	VREG,	NULL },
	{ "loadavg",	0,		KTT_AVENRUN,	VREAD,	VREG,	NULL },
	{ "pagesize",	&cnt.v_page_size, KTT_INT,	VREAD,	VREG,	NULL },
	{ "physmem",	&physmem,	KTT_INT,	VREAD,	VREG,	NULL },
#if 0
	{ "root",	0,		KTT_NULL,	VREAD,	VDIR, &rootdir},
#endif
	{ "rootdev",	0,		KTT_NULL,	VREAD,	VBLK, &rootvp },
	{ "rrootdev",	0,		KTT_NULL,	VREAD,	VCHR, &rrootvp},
	{ "time",	0,		KTT_TIME,	VREAD,	VREG,	NULL },
	{ "version",	version,	KTT_STRING,	VREAD,	VREG,	NULL },
};

static int nkern_targets = sizeof(kern_targets) / sizeof(kern_targets[0]);

static int
kernfs_xread(kt, buf, len, lenp)
	struct kern_target *kt;
	char *buf;
	int len;
	int *lenp;
{
	switch (kt->kt_tag) {
	case KTT_TIME: {
		struct timeval tv;
		microtime(&tv);
		sprintf(buf, "%ld %ld\n", tv.tv_sec, tv.tv_usec);
		break;
	}

	case KTT_INT: {
		int *ip = kt->kt_data;
		sprintf(buf, "%d\n", *ip);
		break;
	}

	case KTT_STRING: {
		char *cp = kt->kt_data;
		int xlen = strlen(cp) + 1;

		if (xlen >= len)
			return (EINVAL);

		bcopy(cp, buf, xlen);
		break;
	}

	case KTT_HOSTNAME: {
		char *cp = hostname;
		int xlen = hostnamelen;

		if (xlen >= (len-2))
			return (EINVAL);

		bcopy(cp, buf, xlen);
		buf[xlen] = '\n';
		buf[xlen+1] = '\0';
		break;
	}

	case KTT_BOOTFILE: {
		char *cp = kernelname;
		int xlen = strlen(cp) + 1;

		if (xlen >= (len-2))
			return (EINVAL);

		bcopy(cp, buf, xlen);
		buf[xlen] = '\n';
		buf[xlen+1] = '\0';
		break;
	}

	case KTT_AVENRUN:
		sprintf(buf, "%ld %ld %ld %ld\n",
				averunnable.ldavg[0],
				averunnable.ldavg[1],
				averunnable.ldavg[2],
				averunnable.fscale);
		break;

	default:
		return (EINVAL);
	}

	*lenp = strlen(buf);
	return (0);
}

static int
kernfs_xwrite(kt, buf, len)
	struct kern_target *kt;
	char *buf;
	int len;
{
	switch (kt->kt_tag) {
	case KTT_HOSTNAME: {
		if (buf[len-1] == '\n')
			--len;
		bcopy(buf, hostname, len);
		hostname[len] = '\0';
		hostnamelen = len;
		return (0);
	}

	default:
		return (EIO);
	}
}


/*
 * vp is the current namei directory
 * ndp is the name to locate in that directory...
 */
static int
kernfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct vnode *fvp;
	int nameiop = cnp->cn_nameiop;
	int error, i;
	char *pname;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup(%x)\n", ap);
	printf("kernfs_lookup(dp = %x, vpp = %x, cnp = %x)\n", dvp, vpp, ap->a_cnp);
#endif
	pname = cnp->cn_nameptr;
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup(%s)\n", pname);
#endif
	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		/*VOP_LOCK(dvp);*/
		return (0);
	}

#if 0
	if (cnp->cn_namelen == 4 && bcmp(pname, "root", 4) == 0) {
		*vpp = rootdir;
		VREF(rootdir);
		VOP_LOCK(rootdir);
		return (0);
	}
#endif

	error = ENOENT;

	for (i = 0; i < nkern_targets; i++) {
		struct kern_target *kt = &kern_targets[i];
		if (cnp->cn_namelen == strlen(kt->kt_name) &&
		    bcmp(kt->kt_name, pname, cnp->cn_namelen) == 0) {
			error = 0;
			break;
		}
	}

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: i = %d, error = %d\n", i, error);
#endif

	/*
	 * If the name wasn't found, and this is not a LOOKUP
	 * request, we return EOPNOTSUPP so that the initial namei()
	 * fails and the higher level routines will not try to call 
	 * our VOP_* functions.
	 */
	if (error) {
		if (nameiop != LOOKUP)
			error = EOPNOTSUPP;
		goto bad;
	}

	/*
	 * DELETE requests are not supported.
	 */
	if (nameiop == DELETE) {
		error = EOPNOTSUPP;
		goto bad;
	}

	/*
	 * Allow CREATE requests if the name in question can
	 * be written to.  This allows open(name, O_RDWR | O_CREAT)
	 * to work.  Otherwise CREATE requests are not supported.
	 */
	if (nameiop == CREATE && (kern_targets[i].kt_rw & VWRITE == 0)) {
		error = EOPNOTSUPP;
		goto bad;
	}

	/*
	 * Check if this name has already has a vnode associated with it.
	 */
	if (kern_targets[i].kt_vp) {
		if (*kern_targets[i].kt_vp) {
			*vpp = *kern_targets[i].kt_vp;
			VREF(*vpp);
			VOP_LOCK(*vpp);
			return (0);
		}
		error = ENXIO;
		goto bad;
	}

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: allocate new vnode\n");
#endif
	error = getnewvnode(VT_KERNFS, dvp->v_mount, kernfs_vnodeop_p, &fvp);
	if (error)
		goto bad;
	MALLOC(fvp->v_data, void *, sizeof(struct kernfs_node), M_TEMP, M_WAITOK);
	VTOKERN(fvp)->kf_kt = &kern_targets[i];
	fvp->v_type = VTOKERN(fvp)->kf_kt->kt_vtype;
	*vpp = fvp;
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: newvp = %x\n", fvp);
#endif
	return (0);

bad:;
	*vpp = NULL;
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: error = %d\n", error);
#endif
	return (error);
}

static int
kernfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	/*
	 * Can always open the root (modulo perms)
	 */
	if (vp->v_flag & VROOT)
		return (0);

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_open, mode = %x, file = %s\n",
			ap->a_mode, VTOKERN(vp)->kf_kt->kt_name);
#endif

	if ((ap->a_mode & FWRITE) && !(VTOKERN(vp)->kf_kt->kt_rw & VWRITE))
		return (EOPNOTSUPP);

	return (0);
}

static int
kernfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct ucred *cred = ap->a_cred;
	mode_t mode = ap->a_mode;

	if (mode & VEXEC) {
		if (vp->v_flag & VROOT)
			return (0);
		return (EACCES);
	}

	if (cred->cr_uid == 0) {
		if ((vp->v_flag & VROOT) == 0) {
			struct kern_target *kt = VTOKERN(vp)->kf_kt;

			if ((mode & VWRITE) && !(kt->kt_rw & VWRITE))
				return (EROFS);
		}
		return (0);
	}

	if (mode & VWRITE)
		return (EACCES);

	return (0);
}


static int
kernfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	int error = 0;
	char strbuf[KSTRING];

	bzero((caddr_t) vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	/* vap->va_qsize = 0; */
	vap->va_blocksize = DEV_BSIZE;
	{
		struct timeval tv;
		microtime(&tv);
		TIMEVAL_TO_TIMESPEC(&tv, &vap->va_atime);
	}
	vap->va_mtime = vap->va_atime;
	vap->va_ctime = vap->va_ctime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	/* vap->va_qbytes = 0; */
	vap->va_bytes = 0;

	if (vp->v_flag & VROOT) {
#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_getattr: stat rootdir\n");
#endif
		vap->va_type = VDIR;
		vap->va_mode = DIR_MODE;
		vap->va_nlink = 2;
		vap->va_fileid = 2;
		vap->va_size = DEV_BSIZE;
	} else {
		struct kern_target *kt = VTOKERN(vp)->kf_kt;
		int nbytes;
#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_getattr: stat target %s\n", kt->kt_name);
#endif
		vap->va_type = kt->kt_vtype;
		vap->va_mode = (kt->kt_rw & VWRITE ? WRITE_MODE : READ_MODE);
		vap->va_nlink = 1;
		vap->va_fileid = 3 + (kt - kern_targets) / sizeof(*kt);
		error = kernfs_xread(kt, strbuf, sizeof(strbuf), &nbytes);
		vap->va_size = nbytes;
	}

	vp->v_type = vap->va_type;
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_getattr: return error %d\n", error);
#endif
	return (error);
}

static int
kernfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	/*
	 * Silently ignore attribute changes.
	 * This allows for open with truncate to have no
	 * effect until some data is written.  I want to
	 * do it this way because all writes are atomic.
	 */
	return (0);
}

static int
kernfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct kern_target *kt;
	char strbuf[KSTRING];
	int off = uio->uio_offset;
	int error, len;
	char *cp;

	if (vp->v_flag & VROOT)
		return (EOPNOTSUPP);

	kt = VTOKERN(vp)->kf_kt;

#ifdef KERNFS_DIAGNOSTIC
	printf("kern_read %s\n", kt->kt_name);
#endif

	len = 0;
	error = kernfs_xread(kt, strbuf, sizeof(strbuf), &len);
	if (error)
		return (error);
	cp = strbuf + off;
	len -= off;
	return (uiomove(cp, len, uio));
}

static int
kernfs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct kern_target *kt;
	int error, xlen;
	char strbuf[KSTRING];

	if (vp->v_flag & VROOT)
		return (0);

	kt = VTOKERN(vp)->kf_kt;

	if (uio->uio_offset != 0)
		return (EINVAL);

	xlen = min(uio->uio_resid, KSTRING-1);
	error = uiomove(strbuf, xlen, uio);
	if (error)
		return (error);

	if (uio->uio_resid != 0)
		return (EIO);

	strbuf[xlen] = '\0';
	xlen = strlen(strbuf);
	return (kernfs_xwrite(kt, strbuf, xlen));
}


static int
kernfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	struct uio *uio = ap->a_uio;
	int i;
	int error;

	i = uio->uio_offset / UIO_MX;
	error = 0;
	while (uio->uio_resid > 0 && i < nkern_targets) {
		struct dirent d;
		struct dirent *dp = &d;
		struct kern_target *kt = &kern_targets[i];
#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_readdir: i = %d\n", i);
#endif

		bzero((caddr_t) dp, UIO_MX);

		dp->d_namlen = strlen(kt->kt_name);
		bcopy(kt->kt_name, dp->d_name, dp->d_namlen+1);

#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_readdir: name = %s, len = %d\n",
				dp->d_name, dp->d_namlen);
#endif
		/*
		 * Fill in the remaining fields
		 */
		dp->d_reclen = UIO_MX;
		dp->d_fileno = i + 3;
		dp->d_type = DT_UNKNOWN;	/* XXX */
		/*
		 * And ship to userland
		 */
		error = uiomove((caddr_t) dp, UIO_MX, uio);
		if (error)
			break;
		i++;
	}

	uio->uio_offset = i * UIO_MX;

	return (error);
}

static int
kernfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	/*
	 * Clear out the v_type field to avoid
	 * nasty things happening in vgone().
	 */
	vp->v_type = VNON;
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_inactive(%x)\n", vp);
#endif
	return (0);
}

static int
kernfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_reclaim(%x)\n", vp);
#endif
	if (vp->v_data) {
		FREE(vp->v_data, M_TEMP);
		vp->v_data = 0;
	}
	return (0);
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
static int
kernfs_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Print out the contents of a kernfs vnode.
 */
/* ARGSUSED */
static int
kernfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	printf("tag VT_KERNFS, kernfs vnode\n");
	return (0);
}

/*void*/
static int
kernfs_vfree(ap)
	struct vop_vfree_args /* {
		struct vnode *a_pvp;
		ino_t a_ino;
		int a_mode;
	} */ *ap;
{

	return (0);
}

/*
 * Kernfs vnode unsupported operation
 */
static int
kernfs_enotsupp()
{

	return (EOPNOTSUPP);
}

/*
 * Kernfs "should never get here" operation
 */
static int
kernfs_badop()
{
	return (EIO);
}

#define kernfs_create ((int (*) __P((struct  vop_create_args *)))kernfs_enotsupp)
#define kernfs_mknod ((int (*) __P((struct  vop_mknod_args *)))kernfs_enotsupp)
#define kernfs_close ((int (*) __P((struct  vop_close_args *)))nullop)
#define kernfs_ioctl ((int (*) __P((struct  vop_ioctl_args *)))kernfs_enotsupp)
#define kernfs_select ((int (*) __P((struct  vop_select_args *)))kernfs_enotsupp)
#define kernfs_mmap ((int (*) __P((struct  vop_mmap_args *)))kernfs_enotsupp)
#define kernfs_fsync ((int (*) __P((struct  vop_fsync_args *)))nullop)
#define kernfs_seek ((int (*) __P((struct  vop_seek_args *)))nullop)
#define kernfs_remove ((int (*) __P((struct  vop_remove_args *)))kernfs_enotsupp)
#define kernfs_link ((int (*) __P((struct  vop_link_args *)))kernfs_enotsupp)
#define kernfs_rename ((int (*) __P((struct  vop_rename_args *)))kernfs_enotsupp)
#define kernfs_mkdir ((int (*) __P((struct  vop_mkdir_args *)))kernfs_enotsupp)
#define kernfs_rmdir ((int (*) __P((struct  vop_rmdir_args *)))kernfs_enotsupp)
#define kernfs_symlink ((int (*) __P((struct vop_symlink_args *)))kernfs_enotsupp)
#define kernfs_readlink \
	((int (*) __P((struct  vop_readlink_args *)))kernfs_enotsupp)
#define kernfs_abortop ((int (*) __P((struct  vop_abortop_args *)))nullop)
#define kernfs_lock ((int (*) __P((struct  vop_lock_args *)))nullop)
#define kernfs_unlock ((int (*) __P((struct  vop_unlock_args *)))nullop)
#define kernfs_bmap ((int (*) __P((struct  vop_bmap_args *)))kernfs_badop)
#define kernfs_strategy ((int (*) __P((struct  vop_strategy_args *)))kernfs_badop)
#define kernfs_islocked ((int (*) __P((struct  vop_islocked_args *)))nullop)
#define kernfs_advlock ((int (*) __P((struct vop_advlock_args *)))kernfs_enotsupp)
#define kernfs_blkatoff \
	((int (*) __P((struct  vop_blkatoff_args *)))kernfs_enotsupp)
#define kernfs_valloc ((int(*) __P(( \
		struct vnode *pvp, \
		int mode, \
		struct ucred *cred, \
		struct vnode **vpp))) kernfs_enotsupp)
#define kernfs_truncate \
	((int (*) __P((struct  vop_truncate_args *)))kernfs_enotsupp)
#define kernfs_update ((int (*) __P((struct  vop_update_args *)))kernfs_enotsupp)
#define kernfs_bwrite ((int (*) __P((struct  vop_bwrite_args *)))kernfs_enotsupp)

vop_t **kernfs_vnodeop_p;
static struct vnodeopv_entry_desc kernfs_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *)vn_default_error },
	{ &vop_lookup_desc, (vop_t *)kernfs_lookup },		/* lookup */
	{ &vop_create_desc, (vop_t *)kernfs_create },		/* create */
	{ &vop_mknod_desc, (vop_t *)kernfs_mknod },		/* mknod */
	{ &vop_open_desc, (vop_t *)kernfs_open },		/* open */
	{ &vop_close_desc, (vop_t *)kernfs_close },		/* close */
	{ &vop_access_desc, (vop_t *)kernfs_access },		/* access */
	{ &vop_getattr_desc, (vop_t *)kernfs_getattr },		/* getattr */
	{ &vop_setattr_desc, (vop_t *)kernfs_setattr },		/* setattr */
	{ &vop_read_desc, (vop_t *)kernfs_read },		/* read */
	{ &vop_write_desc, (vop_t *)kernfs_write },		/* write */
	{ &vop_ioctl_desc, (vop_t *)kernfs_ioctl },		/* ioctl */
	{ &vop_select_desc, (vop_t *)kernfs_select },		/* select */
	{ &vop_mmap_desc, (vop_t *)kernfs_mmap },		/* mmap */
	{ &vop_fsync_desc, (vop_t *)kernfs_fsync },		/* fsync */
	{ &vop_seek_desc, (vop_t *)kernfs_seek },		/* seek */
	{ &vop_remove_desc, (vop_t *)kernfs_remove },		/* remove */
	{ &vop_link_desc, (vop_t *)kernfs_link },		/* link */
	{ &vop_rename_desc, (vop_t *)kernfs_rename },		/* rename */
	{ &vop_mkdir_desc, (vop_t *)kernfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, (vop_t *)kernfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, (vop_t *)kernfs_symlink },		/* symlink */
	{ &vop_readdir_desc, (vop_t *)kernfs_readdir },		/* readdir */
	{ &vop_readlink_desc, (vop_t *)kernfs_readlink },	/* readlink */
	{ &vop_abortop_desc, (vop_t *)kernfs_abortop },		/* abortop */
	{ &vop_inactive_desc, (vop_t *)kernfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, (vop_t *)kernfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, (vop_t *)kernfs_lock },		/* lock */
	{ &vop_unlock_desc, (vop_t *)kernfs_unlock },		/* unlock */
	{ &vop_bmap_desc, (vop_t *)kernfs_bmap },		/* bmap */
	{ &vop_strategy_desc, (vop_t *)kernfs_strategy },	/* strategy */
	{ &vop_print_desc, (vop_t *)kernfs_print },		/* print */
	{ &vop_islocked_desc, (vop_t *)kernfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, (vop_t *)kernfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, (vop_t *)kernfs_advlock },		/* advlock */
	{ &vop_blkatoff_desc, (vop_t *)kernfs_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, (vop_t *)kernfs_valloc },		/* valloc */
	{ &vop_vfree_desc, (vop_t *)kernfs_vfree },		/* vfree */
	{ &vop_truncate_desc, (vop_t *)kernfs_truncate },	/* truncate */
	{ &vop_update_desc, (vop_t *)kernfs_update },		/* update */
	{ &vop_bwrite_desc, (vop_t *)kernfs_bwrite },		/* bwrite */
	{ NULL, NULL }
};
static struct vnodeopv_desc kernfs_vnodeop_opv_desc =
	{ &kernfs_vnodeop_p, kernfs_vnodeop_entries };

VNODEOP_SET(kernfs_vnodeop_opv_desc);
