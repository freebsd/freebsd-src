/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)vfs_subr.c	8.31 (Berkeley) 5/26/95
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <net/radix.h>
#include <sys/domain.h>
#include <sys/dirent.h>
#include <sys/vnode.h>

static MALLOC_DEFINE(M_NETADDR, "Export Host", "Export host address structure");

static void	vfs_free_addrlist(struct netexport *nep);
static int	vfs_free_netcred(struct radix_node *rn, void *w);
static int	vfs_hang_addrlist(struct mount *mp, struct netexport *nep,
		    struct export_args *argp);

/*
 * Network address lookup element
 */
struct netcred {
	struct	radix_node netc_rnodes[2];
	int	netc_exflags;
	struct	ucred netc_anon;
};

/*
 * Network export information
 */
struct netexport {
	struct	netcred ne_defexported;		      /* Default export */
	struct	radix_node_head *ne_rtable[AF_MAX+1]; /* Individual exports */
};

/*
 * Build hash lists of net addresses and hang them off the mount point.
 * Called by ufs_mount() to set up the lists of export addresses.
 */
static int
vfs_hang_addrlist(mp, nep, argp)
	struct mount *mp;
	struct netexport *nep;
	struct export_args *argp;
{
	register struct netcred *np;
	register struct radix_node_head *rnh;
	register int i;
	struct radix_node *rn;
	struct sockaddr *saddr, *smask = 0;
	struct domain *dom;
	int error;

	/*
	 * XXX: This routine converts from a `struct xucred'
	 * (argp->ex_anon) to a `struct ucred' (np->netc_anon).  This
	 * operation is questionable; for example, what should be done
	 * with fields like cr_uidinfo and cr_prison?  Currently, this
	 * routine does not touch them (leaves them as NULL).
	 */
	if (argp->ex_anon.cr_version != XUCRED_VERSION)
		return (EINVAL);

	if (argp->ex_addrlen == 0) {
		if (mp->mnt_flag & MNT_DEFEXPORTED)
			return (EPERM);
		np = &nep->ne_defexported;
		np->netc_exflags = argp->ex_flags;
		bzero(&np->netc_anon, sizeof(np->netc_anon));
		np->netc_anon.cr_uid = argp->ex_anon.cr_uid;
		np->netc_anon.cr_ngroups = argp->ex_anon.cr_ngroups;
		bcopy(argp->ex_anon.cr_groups, np->netc_anon.cr_groups,
		    sizeof(np->netc_anon.cr_groups));
		np->netc_anon.cr_ref = 1;
		mp->mnt_flag |= MNT_DEFEXPORTED;
		return (0);
	}

	if (argp->ex_addrlen > MLEN)
		return (EINVAL);

	i = sizeof(struct netcred) + argp->ex_addrlen + argp->ex_masklen;
	np = (struct netcred *) malloc(i, M_NETADDR, M_WAITOK | M_ZERO);
	saddr = (struct sockaddr *) (np + 1);
	if ((error = copyin(argp->ex_addr, saddr, argp->ex_addrlen)))
		goto out;
	if (saddr->sa_len > argp->ex_addrlen)
		saddr->sa_len = argp->ex_addrlen;
	if (argp->ex_masklen) {
		smask = (struct sockaddr *)((caddr_t)saddr + argp->ex_addrlen);
		error = copyin(argp->ex_mask, smask, argp->ex_masklen);
		if (error)
			goto out;
		if (smask->sa_len > argp->ex_masklen)
			smask->sa_len = argp->ex_masklen;
	}
	i = saddr->sa_family;
	if ((rnh = nep->ne_rtable[i]) == NULL) {
		/*
		 * Seems silly to initialize every AF when most are not used,
		 * do so on demand here
		 */
		for (dom = domains; dom; dom = dom->dom_next)
			if (dom->dom_family == i && dom->dom_rtattach) {
				dom->dom_rtattach((void **) &nep->ne_rtable[i],
				    dom->dom_rtoffset);
				break;
			}
		if ((rnh = nep->ne_rtable[i]) == NULL) {
			error = ENOBUFS;
			goto out;
		}
	}
	RADIX_NODE_HEAD_LOCK(rnh);
	rn = (*rnh->rnh_addaddr)(saddr, smask, rnh, np->netc_rnodes);
	RADIX_NODE_HEAD_UNLOCK(rnh);
	if (rn == NULL || np != (struct netcred *)rn) {	/* already exists */
		error = EPERM;
		goto out;
	}
	np->netc_exflags = argp->ex_flags;
	bzero(&np->netc_anon, sizeof(np->netc_anon));
	np->netc_anon.cr_uid = argp->ex_anon.cr_uid;
	np->netc_anon.cr_ngroups = argp->ex_anon.cr_ngroups;
	bcopy(argp->ex_anon.cr_groups, np->netc_anon.cr_groups,
	    sizeof(np->netc_anon.cr_groups));
	np->netc_anon.cr_ref = 1;
	return (0);
out:
	free(np, M_NETADDR);
	return (error);
}

/* Helper for vfs_free_addrlist. */
/* ARGSUSED */
static int
vfs_free_netcred(rn, w)
	struct radix_node *rn;
	void *w;
{
	register struct radix_node_head *rnh = (struct radix_node_head *) w;

	(*rnh->rnh_deladdr) (rn->rn_key, rn->rn_mask, rnh);
	free(rn, M_NETADDR);
	return (0);
}

/*
 * Free the net address hash lists that are hanging off the mount points.
 */
static void
vfs_free_addrlist(nep)
	struct netexport *nep;
{
	register int i;
	register struct radix_node_head *rnh;

	for (i = 0; i <= AF_MAX; i++)
		if ((rnh = nep->ne_rtable[i])) {
			RADIX_NODE_HEAD_LOCK(rnh);
			(*rnh->rnh_walktree) (rnh, vfs_free_netcred, rnh);
			RADIX_NODE_HEAD_DESTROY(rnh);
			free(rnh, M_RTABLE);
			nep->ne_rtable[i] = NULL;	/* not SMP safe XXX */
		}
}

/*
 * High level function to manipulate export options on a mount point
 * and the passed in netexport.
 * Struct export_args *argp is the variable used to twiddle options,
 * the structure is described in sys/mount.h
 */
int
vfs_export(mp, argp)
	struct mount *mp;
	struct export_args *argp;
{
	struct netexport *nep;
	int error;

	nep = mp->mnt_export;
	if (argp->ex_flags & MNT_DELEXPORT) {
		if (nep == NULL)
			return (ENOENT);
		if (mp->mnt_flag & MNT_EXPUBLIC) {
			vfs_setpublicfs(NULL, NULL, NULL);
			mp->mnt_flag &= ~MNT_EXPUBLIC;
		}
		vfs_free_addrlist(nep);
		mp->mnt_export = NULL;
		free(nep, M_MOUNT);
		mp->mnt_flag &= ~(MNT_EXPORTED | MNT_DEFEXPORTED);
	}
	if (argp->ex_flags & MNT_EXPORTED) {
		if (nep == NULL) {
			nep = malloc(sizeof(struct netexport), M_MOUNT, M_WAITOK | M_ZERO);
			mp->mnt_export = nep;
		}
		if (argp->ex_flags & MNT_EXPUBLIC) {
			if ((error = vfs_setpublicfs(mp, nep, argp)) != 0)
				return (error);
			mp->mnt_flag |= MNT_EXPUBLIC;
		}
		if ((error = vfs_hang_addrlist(mp, nep, argp)))
			return (error);
		mp->mnt_flag |= MNT_EXPORTED;
	}
	return (0);
}

/*
 * Set the publicly exported filesystem (WebNFS). Currently, only
 * one public filesystem is possible in the spec (RFC 2054 and 2055)
 */
int
vfs_setpublicfs(mp, nep, argp)
	struct mount *mp;
	struct netexport *nep;
	struct export_args *argp;
{
	int error;
	struct vnode *rvp;
	char *cp;

	/*
	 * mp == NULL -> invalidate the current info, the FS is
	 * no longer exported. May be called from either vfs_export
	 * or unmount, so check if it hasn't already been done.
	 */
	if (mp == NULL) {
		if (nfs_pub.np_valid) {
			nfs_pub.np_valid = 0;
			if (nfs_pub.np_index != NULL) {
				FREE(nfs_pub.np_index, M_TEMP);
				nfs_pub.np_index = NULL;
			}
		}
		return (0);
	}

	/*
	 * Only one allowed at a time.
	 */
	if (nfs_pub.np_valid != 0 && mp != nfs_pub.np_mount)
		return (EBUSY);

	/*
	 * Get real filehandle for root of exported FS.
	 */
	bzero(&nfs_pub.np_handle, sizeof(nfs_pub.np_handle));
	nfs_pub.np_handle.fh_fsid = mp->mnt_stat.f_fsid;

	if ((error = VFS_ROOT(mp, &rvp)))
		return (error);

	if ((error = VFS_VPTOFH(rvp, &nfs_pub.np_handle.fh_fid)))
		return (error);

	vput(rvp);

	/*
	 * If an indexfile was specified, pull it in.
	 */
	if (argp->ex_indexfile != NULL) {
		MALLOC(nfs_pub.np_index, char *, MAXNAMLEN + 1, M_TEMP,
		    M_WAITOK);
		error = copyinstr(argp->ex_indexfile, nfs_pub.np_index,
		    MAXNAMLEN, (size_t *)0);
		if (!error) {
			/*
			 * Check for illegal filenames.
			 */
			for (cp = nfs_pub.np_index; *cp; cp++) {
				if (*cp == '/') {
					error = EINVAL;
					break;
				}
			}
		}
		if (error) {
			FREE(nfs_pub.np_index, M_TEMP);
			return (error);
		}
	}

	nfs_pub.np_mount = mp;
	nfs_pub.np_valid = 1;
	return (0);
}

/*
 * Used by the filesystems to determine if a given network address
 * (passed in 'nam') is present in thier exports list, returns a pointer
 * to struct netcred so that the filesystem can examine it for
 * access rights (read/write/etc).
 */
struct netcred *
vfs_export_lookup(mp, nam)
	register struct mount *mp;
	struct sockaddr *nam;
{
	struct netexport *nep;
	register struct netcred *np;
	register struct radix_node_head *rnh;
	struct sockaddr *saddr;

	nep = mp->mnt_export;
	if (nep == NULL)
		return (NULL);
	np = NULL;
	if (mp->mnt_flag & MNT_EXPORTED) {
		/*
		 * Lookup in the export list first.
		 */
		if (nam != NULL) {
			saddr = nam;
			rnh = nep->ne_rtable[saddr->sa_family];
			if (rnh != NULL) {
				RADIX_NODE_HEAD_LOCK(rnh);
				np = (struct netcred *)
				    (*rnh->rnh_matchaddr)(saddr, rnh);
				RADIX_NODE_HEAD_UNLOCK(rnh);
				if (np && np->netc_rnodes->rn_flags & RNF_ROOT)
					np = NULL;
			}
		}
		/*
		 * If no address match, use the default if it exists.
		 */
		if (np == NULL && mp->mnt_flag & MNT_DEFEXPORTED)
			np = &nep->ne_defexported;
	}
	return (np);
}

/*
 * XXX: This comment comes from the deprecated ufs_check_export()
 * XXX: and may not entirely apply, but lacking something better:
 * This is the generic part of fhtovp called after the underlying
 * filesystem has validated the file handle.
 *
 * Verify that a host should have access to a filesystem.
 */

int 
vfs_stdcheckexp(mp, nam, extflagsp, credanonp)
	struct mount *mp;
	struct sockaddr *nam;
	int *extflagsp;
	struct ucred **credanonp;
{
	struct netcred *np;

	np = vfs_export_lookup(mp, nam);
	if (np == NULL)
		return (EACCES);
	*extflagsp = np->netc_exflags;
	*credanonp = &np->netc_anon;
	return (0);
}

