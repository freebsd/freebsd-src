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
 *	@(#)kernfs_vnops.c	8.15 (Berkeley) 5/21/95
 * $Id: kernfs_vnops.c,v 1.33 1998/06/14 12:34:42 bde Exp $
 */

/*
 * Kernel parameter filesystem (/kern)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vmmeter.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/resource.h>

#include <miscfs/kernfs/kernfs.h>

#define KSTRING	256		/* Largest I/O available via this filesystem */
#define	UIO_MX 32

#define	READ_MODE	(S_IRUSR|S_IRGRP|S_IROTH)
#define	WRITE_MODE	(S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH)
#define DIR_MODE	(S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

static struct kern_target {
	u_char kt_type;
	u_char kt_namlen;
	char *kt_name;
	void *kt_data;
#define	KTT_NULL	 1
#define	KTT_TIME	 5
#define KTT_INT		17
#define	KTT_STRING	31
#define KTT_HOSTNAME	47
#define KTT_BOOTFILE	49
#define KTT_AVENRUN	53
#define KTT_DEVICE	71
	u_char kt_tag;
	u_char kt_vtype;
	mode_t kt_mode;
} kern_targets[] = {
/* NOTE: The name must be less than UIO_MX-16 chars in length */
#define N(s) sizeof(s)-1, s
     /*        name            data          tag           type  ro/rw  */
     { DT_DIR, N("."),         0,            KTT_NULL,     VDIR, DIR_MODE   },
     { DT_DIR, N(".."),        0,            KTT_NULL,     VDIR, DIR_MODE   },
     { DT_REG, N("boottime"),  &boottime.tv_sec, KTT_INT,  VREG, READ_MODE  },
     { DT_REG, N("copyright"), copyright,    KTT_STRING,   VREG, READ_MODE  },
     { DT_REG, N("hostname"),  0,            KTT_HOSTNAME, VREG, WRITE_MODE },
     { DT_REG, N("bootfile"),  0,	     KTT_BOOTFILE, VREG, READ_MODE  },
     { DT_REG, N("hz"),        &hz,          KTT_INT,      VREG, READ_MODE  },
     { DT_REG, N("loadavg"),   0,            KTT_AVENRUN,  VREG, READ_MODE  },
     { DT_REG, N("pagesize"),  &cnt.v_page_size, KTT_INT,  VREG, READ_MODE  },
     { DT_REG, N("physmem"),   &physmem,     KTT_INT,      VREG, READ_MODE  },
#if 0
     { DT_DIR, N("root"),      0,            KTT_NULL,     VDIR, DIR_MODE   },
     { DT_BLK, N("rootdev"),   &rootdev,     KTT_DEVICE,   VBLK, READ_MODE  },
     { DT_CHR, N("rrootdev"),  &rrootdev,    KTT_DEVICE,   VCHR, READ_MODE  },
#endif
     { DT_REG, N("time"),      0,            KTT_TIME,     VREG, READ_MODE  },
     { DT_REG, N("version"),   version,      KTT_STRING,   VREG, READ_MODE  },
#undef N
};
static int nkern_targets = sizeof(kern_targets) / sizeof(kern_targets[0]);

static int	kernfs_access __P((struct vop_access_args *ap));
static int	kernfs_badop __P((void));
static int	kernfs_enotsupp __P((void));
static int	kernfs_getattr __P((struct vop_getattr_args *ap));
static int	kernfs_inactive __P((struct vop_inactive_args *ap));
static int	kernfs_lookup __P((struct vop_lookup_args *ap));
static int	kernfs_pathconf __P((struct vop_pathconf_args *ap));
static int	kernfs_print __P((struct vop_print_args *ap));
static int	kernfs_read __P((struct vop_read_args *ap));
static int	kernfs_readdir __P((struct vop_readdir_args *ap));
static int	kernfs_reclaim __P((struct vop_reclaim_args *ap));
static int	kernfs_setattr __P((struct vop_setattr_args *ap));
static int	kernfs_write __P((struct vop_write_args *ap));
static int	kernfs_xread __P((struct kern_target *kt, char *buf, int len,
				  int *lenp));
static int	kernfs_xwrite __P((struct kern_target *kt, char *buf, int len));

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
		int xlen = strlen(hostname);

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
		    averunnable.ldavg[0], averunnable.ldavg[1],
		    averunnable.ldavg[2], averunnable.fscale);
		break;

	default:
		return (EIO);
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
	case KTT_HOSTNAME:
		/* XXX BOGUS !!! no check for the length */
		if (buf[len-1] == '\n')
			--len;
		bcopy(buf, hostname, len);
		hostname[len] = '\0';
		return (0);

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
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	char *pname = cnp->cn_nameptr;
	struct proc *p = cnp->cn_proc;
	struct kern_target *kt;
	struct vnode *fvp;
	int nameiop = cnp->cn_nameiop;
	int error, i;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup(%x)\n", ap);
	printf("kernfs_lookup(dp = %x, vpp = %x, cnp = %x)\n", dvp, vpp, ap->a_cnp);
	printf("kernfs_lookup(%s)\n", pname);
#endif

	*vpp = NULLVP;

	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		return (EROFS);

	VOP_UNLOCK(dvp, 0, p);
	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		vn_lock(dvp, LK_SHARED | LK_RETRY, p);
		return (0);
	}

#if 0
	if (cnp->cn_namelen == 4 && bcmp(pname, "root", 4) == 0) {
		*vpp = rootdir;
		VREF(rootdir);
		vn_lock(rootdir, LK_SHARED | LK_RETRY, p)
		return (0);
	}
#endif

	for (kt = kern_targets, i = 0; i < nkern_targets; kt++, i++) {
		if (cnp->cn_namelen == kt->kt_namlen &&
		    bcmp(kt->kt_name, pname, cnp->cn_namelen) == 0)
			goto found;
	}

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: i = %d, failed", i);
#endif

	vn_lock(dvp, LK_SHARED | LK_RETRY, p);
	return (cnp->cn_nameiop == LOOKUP ? ENOENT : EROFS);

found:
	if (kt->kt_tag == KTT_DEVICE) {
		dev_t *dp = kt->kt_data;
	loop:
		if (*dp == NODEV || !vfinddev(*dp, kt->kt_vtype, &fvp)) {
			vn_lock(dvp, LK_SHARED | LK_RETRY, p);
			return (ENOENT);
		}
		*vpp = fvp;
		if (vget(fvp, LK_EXCLUSIVE, p))
			goto loop;
		return (0);
	}

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: allocate new vnode\n");
#endif
	if (error = getnewvnode(VT_KERNFS, dvp->v_mount, kernfs_vnodeop_p,
	    &fvp)) {
		vn_lock(dvp, LK_SHARED | LK_RETRY, p);
		return (error);
	}

	MALLOC(fvp->v_data, void *, sizeof(struct kernfs_node), M_TEMP,
	    M_WAITOK);
	VTOKERN(fvp)->kf_kt = kt;
	fvp->v_type = kt->kt_vtype;
	vn_lock(fvp, LK_SHARED | LK_RETRY, p);
	*vpp = fvp;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup: newvp = %x\n", fvp);
#endif
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
	register struct vnode *vp = ap->a_vp;
	register struct ucred *cred = ap->a_cred;
	mode_t amode = ap->a_mode;
	mode_t fmode =
	    (vp->v_flag & VROOT) ? DIR_MODE : VTOKERN(vp)->kf_kt->kt_mode;
	mode_t mask = 0;
	register gid_t *gp;
	int i;

	/* Some files are simply not modifiable. */
	if ((amode & VWRITE) && (fmode & (S_IWUSR|S_IWGRP|S_IWOTH)) == 0)
		return (EPERM);

	/* Root can do anything else. */
	if (cred->cr_uid == 0)
		return (0);

	/* Check for group 0 (wheel) permissions. */
	for (i = 0, gp = cred->cr_groups; i < cred->cr_ngroups; i++, gp++)
		if (*gp == 0) {
			if (amode & VEXEC)
				mask |= S_IXGRP;
			if (amode & VREAD)
				mask |= S_IRGRP;
			if (amode & VWRITE)
				mask |= S_IWGRP;
			return ((fmode & mask) == mask ?  0 : EACCES);
		}

        /* Otherwise, check everyone else. */
	if (amode & VEXEC)
		mask |= S_IXOTH;
	if (amode & VREAD)
		mask |= S_IROTH;
	if (amode & VWRITE)
		mask |= S_IWOTH;
	return ((fmode & mask) == mask ? 0 : EACCES);
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
	struct timeval tv;
	int error = 0;
	char strbuf[KSTRING];

	bzero((caddr_t) vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size = 0;
	vap->va_blocksize = DEV_BSIZE;
	nanotime(&vap->va_atime);
	vap->va_mtime = vap->va_atime;
	vap->va_ctime = vap->va_ctime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
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
		vap->va_mode = kt->kt_mode;
		vap->va_nlink = 1;
		vap->va_fileid = 1 + (kt - kern_targets) / sizeof(*kt);
		error = kernfs_xread(kt, strbuf, sizeof(strbuf), &nbytes);
		vap->va_size = nbytes;
	}

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

	if (ap->a_vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

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

	if (vp->v_type == VDIR)
		return (EOPNOTSUPP);

	kt = VTOKERN(vp)->kf_kt;

#ifdef KERNFS_DIAGNOSTIC
	printf("kern_read %s\n", kt->kt_name);
#endif

	len = 0;
	if (error = kernfs_xread(kt, strbuf, sizeof(strbuf), &len))
		return (error);
	if (len <= off)
		return (0);
	return (uiomove(&strbuf[off], len - off, uio));
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

	if (vp->v_type == VDIR)
		return (EOPNOTSUPP);

	kt = VTOKERN(vp)->kf_kt;

	if (uio->uio_offset != 0)
		return (EINVAL);

	xlen = min(uio->uio_resid, KSTRING-1);
	if (error = uiomove(strbuf, xlen, uio))
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
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	int error, i, off;
	struct uio *uio = ap->a_uio;
	struct kern_target *kt;
	struct dirent d;

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	off = (int)uio->uio_offset;
	if (off != uio->uio_offset || off < 0 || (u_int)off % UIO_MX != 0 ||
	    uio->uio_resid < UIO_MX)
		return (EINVAL);

	i = (u_int)off / UIO_MX;
	error = 0;
	for (kt = &kern_targets[i];
		uio->uio_resid >= UIO_MX && i < nkern_targets; kt++, i++) {
		struct dirent *dp = &d;
#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_readdir: i = %d\n", i);
#endif

		if (kt->kt_tag == KTT_DEVICE) {
			dev_t *dp = kt->kt_data;
			struct vnode *fvp;

			if (*dp == NODEV || !vfinddev(*dp, kt->kt_vtype, &fvp))
				continue;
		}

		bzero((caddr_t)dp, UIO_MX);
		dp->d_namlen = kt->kt_namlen;
		bcopy(kt->kt_name, dp->d_name, kt->kt_namlen+1);

#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_readdir: name = %s, len = %d\n",
				dp->d_name, dp->d_namlen);
#endif
		/*
		 * Fill in the remaining fields
		 */
		dp->d_reclen = UIO_MX;
		dp->d_fileno = i + 3;
		dp->d_type = kt->kt_type;
		/*
		 * And ship to userland
		 */
		if (error = uiomove((caddr_t)dp, UIO_MX, uio))
			break;
	}

	uio->uio_offset = i * UIO_MX;

	return (error);
}

static int
kernfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_inactive(%x)\n", vp);
#endif
	/*
	 * Clear out the v_type field to avoid
	 * nasty things happening in vgone().
	 */
	VOP_UNLOCK(vp, 0, ap->a_p);
	vp->v_type = VNON;
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

/*
 * Kernfs "should never get here" operation
 */
static int
kernfs_badop()
{
	return (EIO);
}


vop_t	**kernfs_vnodeop_p;
static struct vnodeopv_entry_desc kernfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) kernfs_access },
	{ &vop_bmap_desc,		(vop_t *) kernfs_badop },
	{ &vop_getattr_desc,		(vop_t *) kernfs_getattr },
	{ &vop_inactive_desc,		(vop_t *) kernfs_inactive },
	{ &vop_lookup_desc,		(vop_t *) kernfs_lookup },
	{ &vop_pathconf_desc,		(vop_t *) vop_stdpathconf },
	{ &vop_print_desc,		(vop_t *) kernfs_print },
	{ &vop_read_desc,		(vop_t *) kernfs_read },
	{ &vop_readdir_desc,		(vop_t *) kernfs_readdir },
	{ &vop_reclaim_desc,		(vop_t *) kernfs_reclaim },
	{ &vop_setattr_desc,		(vop_t *) kernfs_setattr },
	{ &vop_write_desc,		(vop_t *) kernfs_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc kernfs_vnodeop_opv_desc =
	{ &kernfs_vnodeop_p, kernfs_vnodeop_entries };

VNODEOP_SET(kernfs_vnodeop_opv_desc);
