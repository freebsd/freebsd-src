/* $FreeBSD$ */
/* $Id: nfs4_vn_subs.c,v 1.9 2003/11/05 14:59:00 rees Exp $ */

/*
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/vnode.h>
#include <sys/types.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <rpc/rpcclnt.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>
#include <nfsclient/nfsdiskless.h>

/* NFSv4 */
#include <nfs4client/nfs4.h>
#include <nfs4client/nfs4m_subs.h>
#include <nfs4client/nfs4_vn.h>

#include <nfsclient/nfsnode.h>

void
nfs4_vnop_loadattrcache(struct vnode *vp, struct nfsv4_fattr *fap,
    struct vattr *vaper)
{
        struct vattr *vap;
        struct nfsnode *np;
        int32_t rdev;
        enum vtype vtyp;
        u_short vmode;
        struct timespec mtime;
	struct timeval tv;

        microtime(&tv);

        vtyp = nv3tov_type[fap->fa4_type & 0x7];
        vmode = (fap->fa4_valid & FA4V_MODE) ? fap->fa4_mode : 0777;
        rdev = (fap->fa4_valid & FA4V_RDEV) ?
	    makeudev(fap->fa4_rdev_major, fap->fa4_rdev_minor) : 0;
        if (fap->fa4_valid & FA4V_MTIME)
                mtime = fap->fa4_mtime;
        else
                bzero(&mtime, sizeof mtime);

        /*
         * If v_type == VNON it is a new node, so fill in the v_type,
         * n_mtime fields. Check to see if it represents a special
         * device, and if so, check for a possible alias. Once the
         * correct vnode has been obtained, fill in the rest of the
         * information.
         */
        np = VTONFS(vp);
        vap = &np->n_vattr;
        if (vp->v_type != vtyp || np->n_mtime == 0) {
                bzero(vap, sizeof *vap);
                vp->v_type = vtyp;
                np->n_mtime = mtime.tv_sec;
        }
        vap->va_type = vtyp;
        vap->va_mode = (vmode & 07777);
        vap->va_rdev = rdev;
        vap->va_mtime = mtime;
        vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
        if (fap->fa4_valid & FA4V_NLINK)
                vap->va_nlink = fap->fa4_nlink;
        if (fap->fa4_valid & FA4V_UID)
                vap->va_uid = fap->fa4_uid;
        if (fap->fa4_valid & FA4V_GID)
                vap->va_gid = fap->fa4_gid;
        vap->va_size = fap->fa4_size;
        vap->va_blocksize = NFS_FABLKSIZE;
        vap->va_bytes = fap->fa4_size;
        if (fap->fa4_valid & FA4V_FILEID)
                vap->va_fileid = nfs_v4fileid4_to_fileid(fap->fa4_fileid);
        if (fap->fa4_valid & FA4V_ATIME)
                vap->va_atime = fap->fa4_atime;
        if (fap->fa4_valid & FA4V_CTIME)
                vap->va_ctime = fap->fa4_ctime;
        vap->va_flags = 0;
        vap->va_filerev = 0;
	/* XXX dontshrink flag? */
        if (vap->va_size != np->n_size) {
                if (vap->va_type == VREG) {
                        if (np->n_flag & NMODIFIED) {
                                if (vap->va_size < np->n_size)
                                        vap->va_size = np->n_size;
                                else
                                        np->n_size = vap->va_size;
                        } else
                                np->n_size = vap->va_size;
			vnode_pager_setsize(vp, np->n_size);
                } else
                        np->n_size = vap->va_size;
        }
        np->n_attrstamp = tv.tv_sec;
        if (vaper != NULL) {
                bcopy((caddr_t)vap, (caddr_t)vaper, sizeof(*vap));
                if (np->n_flag & NCHG) {
                        if (np->n_flag & NACC)
                                vaper->va_atime = np->n_atim;
                        if (np->n_flag & NUPD)
                                vaper->va_mtime = np->n_mtim;
                }
        }
}

static uint64_t nfs_nullcookie = 0;
/*
 * This function finds the directory cookie that corresponds to the
 * logical byte offset given.
 */
uint64_t *
nfs4_getcookie(struct nfsnode *np, off_t off, int add)
{
	struct nfsdmap *dp, *dp2;
	int pos;

	pos = (uoff_t)off / NFS_DIRBLKSIZ;
	if (pos == 0 || off < 0) {
#ifdef DIAGNOSTIC
		if (add)
			panic("nfs getcookie add at <= 0");
#endif
		return (&nfs_nullcookie);
	}
	pos--;
	dp = LIST_FIRST(&np->n_cookies);
	if (!dp) {
		if (add) {
			MALLOC(dp, struct nfsdmap *, sizeof (struct nfsdmap),
				M_NFSDIROFF, M_WAITOK);
			dp->ndm_eocookie = 0;
			LIST_INSERT_HEAD(&np->n_cookies, dp, ndm_list);
		} else
			return (NULL);
	}
	while (pos >= NFSNUMCOOKIES) {
		pos -= NFSNUMCOOKIES;
		if (LIST_NEXT(dp, ndm_list)) {
			if (!add && dp->ndm_eocookie < NFSNUMCOOKIES &&
				pos >= dp->ndm_eocookie)
				return (NULL);
			dp = LIST_NEXT(dp, ndm_list);
		} else if (add) {
			MALLOC(dp2, struct nfsdmap *, sizeof (struct nfsdmap),
				M_NFSDIROFF, M_WAITOK);
			dp2->ndm_eocookie = 0;
			LIST_INSERT_AFTER(dp, dp2, ndm_list);
			dp = dp2;
		} else
			return (NULL);
	}
	if (pos >= dp->ndm_eocookie) {
		if (add)
			dp->ndm_eocookie = pos + 1;
		else
			return (NULL);
	}
	return (&dp->ndm4_cookies[pos]);
}

/*
 * Invalidate cached directory information, except for the actual directory
 * blocks (which are invalidated separately).
 * Done mainly to avoid the use of stale offset cookies.
 */
void
nfs4_invaldir(struct vnode *vp)
{
	struct nfsnode *np = VTONFS(vp);

	np->n_direofoffset = 0;
	bzero(np->n4_cookieverf, NFSX_V4VERF);
	if (LIST_FIRST(&np->n_cookies))
		LIST_FIRST(&np->n_cookies)->ndm_eocookie = 0;
}
