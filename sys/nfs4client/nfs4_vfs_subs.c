/* $FreeBSD: src/sys/nfs4client/nfs4_vfs_subs.c,v 1.5 2007/01/25 13:07:25 bde Exp $ */
/* $Id: nfs4_vfs_subs.c,v 1.5 2003/11/05 14:59:00 rees Exp $ */

/*-
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
#include <nfs4client/nfs4_vfs.h>

#include <nfsclient/nfsnode.h>

void
nfs4_vfsop_fsinfo(struct nfsv4_fattr *fap, struct nfsmount *nmp)
{
	uint32_t max;

	if (fap->fa4_valid & FA4V_FSID) {
		nmp->nm_fsid.val[0] = fap->fa4_fsid_major;
		nmp->nm_fsid.val[1] = fap->fa4_fsid_minor;
	}
	if (fap->fa4_valid & FA4V_MAXREAD) {
                max = fap->fa4_maxread;
                if (max < nmp->nm_rsize) {
                        nmp->nm_rsize = max & ~(NFS_FABLKSIZE - 1);
                        if (nmp->nm_rsize == 0)
                                nmp->nm_rsize = max;
                }
                if (max < nmp->nm_readdirsize) {
                        nmp->nm_readdirsize = max & ~(NFS_DIRBLKSIZ - 1);
                        if (nmp->nm_readdirsize == 0)
                                nmp->nm_readdirsize = max;
                }
	}
	if (fap->fa4_valid & FA4V_MAXWRITE) {
                max = fap->fa4_maxwrite;
                if (max < nmp->nm_wsize) {
                        nmp->nm_wsize = max & ~(NFS_FABLKSIZE - 1);
                        if (nmp->nm_wsize == 0)
                                nmp->nm_wsize = max;
                }
	}
	if (fap->fa4_valid & FA4V_LEASE_TIME)
		nmp->nm_lease_time = fap->fa4_lease_time;

	/* nmp->nm_flag |= NFSMNT_GOTFSINFO; */
}

void
nfs4_vfsop_statfs(struct nfsv4_fattr *fap, struct statfs *sbp, struct mount *mp)
{
	struct nfsmount *nmp = VFSTONFS(mp);

	sbp->f_iosize = nfs_iosize(nmp);
	sbp->f_bsize = NFS_FABLKSIZE;

	if (fap->fa4_valid & FA4V_FSID) {
		sbp->f_fsid.val[0] = fap->fa4_fsid_major;
		sbp->f_fsid.val[1] = fap->fa4_fsid_minor;
	}

	sbp->f_ffree = fap->fa4_valid & FA4V_FFREE ? fap->fa4_ffree : 0;
	/* sbp->f_ftotal = fa->fa4_valid & FA4_FTOTAL ? fa->fa4_ftotal : 0; */
	sbp->f_bavail = fap->fa4_valid & FA4V_SAVAIL ?
	    fap->fa4_savail / NFS_FABLKSIZE : 500000;
	sbp->f_bfree = fap->fa4_valid & FA4V_SFREE ?
	    fap->fa4_sfree / NFS_FABLKSIZE : 500000;
	sbp->f_blocks = fap->fa4_valid & FA4V_STOTAL ?
	    fap->fa4_stotal / NFS_FABLKSIZE : 1000000;
}
