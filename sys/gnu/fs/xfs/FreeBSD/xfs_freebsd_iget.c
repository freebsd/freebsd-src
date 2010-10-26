/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2006 Russell Cattelan Digital Elves, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include "xfs.h"

#include "xfs_types.h"
#include "xfs_bit.h"
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
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_quota.h"
#include "xfs_utils.h"
#include "xfs_vnode.h"

static int xfs_vn_allocate(xfs_mount_t *, xfs_inode_t *, struct xfs_vnode **);

/*
 * Look up an inode by number in the given file system.
 * The inode is looked up in the hash table for the file system
 * represented by the mount point parameter mp.  Each bucket of
 * the hash table is guarded by an individual semaphore.
 *
 * If the inode is found in the hash table, its corresponding vnode
 * is obtained with a call to vn_get().  This call takes care of
 * coordination with the reclamation of the inode and vnode.  Note
 * that the vmap structure is filled in while holding the hash lock.
 * This gives us the state of the inode/vnode when we found it and
 * is used for coordination in vn_get().
 *
 * If it is not in core, read it in from the file system's device and
 * add the inode into the hash table.
 *
 * The inode is locked according to the value of the lock_flags parameter.
 * This flag parameter indicates how and if the inode's IO lock and inode lock
 * should be taken.
 *
 * mp -- the mount point structure for the current file system.  It points
 *       to the inode hash table.
 * tp -- a pointer to the current transaction if there is one.  This is
 *       simply passed through to the xfs_iread() call.
 * ino -- the number of the inode desired.  This is the unique identifier
 *        within the file system for the inode being requested.
 * lock_flags -- flags indicating how to lock the inode.  See the comment
 *		 for xfs_ilock() for a list of valid values.
 * bno -- the block number starting the buffer containing the inode,
 *	  if known (as by bulkstat), else 0.
 */
int
xfs_iget(
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	xfs_ino_t	ino,
	uint		flags,
	uint		lock_flags,
	xfs_inode_t	**ipp,
	xfs_daddr_t	bno)
{
	xfs_ihash_t	*ih;
	xfs_inode_t	*ip;
	xfs_inode_t	*iq;
	xfs_vnode_t	*vp;
	ulong		version;
	int		error;
	/* REFERENCED */
	int		newnode;
	xfs_chash_t	*ch;
	xfs_chashlist_t	*chl, *chlnew;
	vmap_t		vmap;
	SPLDECL(s);

	XFS_STATS_INC(xs_ig_attempts);

	ih = XFS_IHASH(mp, ino);

again:
	read_lock(&ih->ih_lock);

	for (ip = ih->ih_next; ip != NULL; ip = ip->i_next) {
		if (ip->i_ino == ino) {
			vp = XFS_ITOV(ip);
			VMAP(vp, vmap);
			/*
			 * Inode cache hit: if ip is not at the front of
			 * its hash chain, move it there now.
			 * Do this with the lock held for update, but
			 * do statistics after releasing the lock.
			 */
			if (ip->i_prevp != &ih->ih_next
			    && rwlock_trypromote(&ih->ih_lock)) {

				if ((iq = ip->i_next)) {
					iq->i_prevp = ip->i_prevp;
				}
				*ip->i_prevp = iq;
				iq = ih->ih_next;
				iq->i_prevp = &ip->i_next;
				ip->i_next = iq;
				ip->i_prevp = &ih->ih_next;
				ih->ih_next = ip;
				write_unlock(&ih->ih_lock);
			} else {
				read_unlock(&ih->ih_lock);
			}

			XFS_STATS_INC(xs_ig_found);

			/*
			 * Get a reference to the vnode/inode.
			 * vn_get() takes care of coordination with
			 * the file system inode release and reclaim
			 * functions.  If it returns NULL, the inode
			 * has been reclaimed so just start the search
			 * over again.  We probably won't find it,
			 * but we could be racing with another cpu
			 * looking for the same inode so we have to at
			 * least look.
			 */
			if (!(vp = vn_get(vp, &vmap))) {
				XFS_STATS_INC(xs_ig_frecycle);
				goto again;
			}

			if (lock_flags != 0) {
				ip->i_flags &= ~XFS_IRECLAIM;
				xfs_ilock(ip, lock_flags);
			}

			newnode = (ip->i_d.di_mode == 0);
			if (newnode) {
				xfs_iocore_inode_reinit(ip);
			}
			ip->i_flags &= ~XFS_ISTALE;

			vn_trace_exit(vp, "xfs_iget.found",
						(inst_t *)__return_address);
			goto return_ip;
		}
	}

	/*
	 * Inode cache miss: save the hash chain version stamp and unlock
	 * the chain, so we don't deadlock in vn_alloc.
	 */
	XFS_STATS_INC(xs_ig_missed);

	version = ih->ih_version;

	read_unlock(&ih->ih_lock);

	/*
	 * Read the disk inode attributes into a new inode structure and get
	 * a new vnode for it. This should also initialize i_ino and i_mount.
	 */
	error = xfs_iread(mp, tp, ino, &ip, bno);
	if (error) {
		return error;
	}

	error = xfs_vn_allocate(mp, ip, &vp);
	if (error) {
		return error;
	}
	vn_trace_exit(vp, "xfs_iget.alloc", (inst_t *)__return_address);

	xfs_inode_lock_init(ip, vp);
	xfs_iocore_inode_init(ip);

	if (lock_flags != 0) {
		xfs_ilock(ip, lock_flags);
	}

	/*
	 * Put ip on its hash chain, unless someone else hashed a duplicate
	 * after we released the hash lock.
	 */
	write_lock(&ih->ih_lock);

	if (ih->ih_version != version) {
		for (iq = ih->ih_next; iq != NULL; iq = iq->i_next) {
			if (iq->i_ino == ino) {
				write_unlock(&ih->ih_lock);
				xfs_idestroy(ip);

				XFS_STATS_INC(xs_ig_dup);
				goto again;
			}
		}
	}

	/*
	 * These values _must_ be set before releasing ihlock!
	 */
	ip->i_hash = ih;
	if ((iq = ih->ih_next)) {
		iq->i_prevp = &ip->i_next;
	}
	ip->i_next = iq;
	ip->i_prevp = &ih->ih_next;
	ih->ih_next = ip;
	ip->i_udquot = ip->i_gdquot = NULL;
	ih->ih_version++;

	write_unlock(&ih->ih_lock);

	/*
	 * put ip on its cluster's hash chain
	 */
	ASSERT(ip->i_chash == NULL && ip->i_cprev == NULL &&
	       ip->i_cnext == NULL);

	chlnew = NULL;
	ch = XFS_CHASH(mp, ip->i_blkno);
 chlredo:
	s = mutex_spinlock(&ch->ch_lock);
	for (chl = ch->ch_list; chl != NULL; chl = chl->chl_next) {
		if (chl->chl_blkno == ip->i_blkno) {

			/* insert this inode into the doubly-linked list
			 * where chl points */
			if ((iq = chl->chl_ip)) {
				ip->i_cprev = iq->i_cprev;
				iq->i_cprev->i_cnext = ip;
				iq->i_cprev = ip;
				ip->i_cnext = iq;
			} else {
				ip->i_cnext = ip;
				ip->i_cprev = ip;
			}
			chl->chl_ip = ip;
			ip->i_chash = chl;
			break;
		}
	}

	/* no hash list found for this block; add a new hash list */
	if (chl == NULL)  {
		if (chlnew == NULL) {
			mutex_spinunlock(&ch->ch_lock, s);
			ASSERT(xfs_chashlist_zone != NULL);
			chlnew = (xfs_chashlist_t *)
					kmem_zone_alloc(xfs_chashlist_zone,
						KM_SLEEP);
			ASSERT(chlnew != NULL);
			goto chlredo;
		} else {
			ip->i_cnext = ip;
			ip->i_cprev = ip;
			ip->i_chash = chlnew;
			chlnew->chl_ip = ip;
			chlnew->chl_blkno = ip->i_blkno;
			chlnew->chl_next = ch->ch_list;
			ch->ch_list = chlnew;
			chlnew = NULL;
		}
	} else {
		if (chlnew != NULL) {
			kmem_zone_free(xfs_chashlist_zone, chlnew);
		}
	}

	mutex_spinunlock(&ch->ch_lock, s);

	/*
	 * Link ip to its mount and thread it on the mount's inode list.
	 */
	XFS_MOUNT_ILOCK(mp);
	if ((iq = mp->m_inodes)) {
		ASSERT(iq->i_mprev->i_mnext == iq);
		ip->i_mprev = iq->i_mprev;
		iq->i_mprev->i_mnext = ip;
		iq->i_mprev = ip;
		ip->i_mnext = iq;
	} else {
		ip->i_mnext = ip;
		ip->i_mprev = ip;
	}
	mp->m_inodes = ip;

	XFS_MOUNT_IUNLOCK(mp);

	newnode = 1;

 return_ip:
	ASSERT(ip->i_df.if_ext_max ==
	       XFS_IFORK_DSIZE(ip) / sizeof(xfs_bmbt_rec_t));

	ASSERT(((ip->i_d.di_flags & XFS_DIFLAG_REALTIME) != 0) ==
	       ((ip->i_iocore.io_flags & XFS_IOCORE_RT) != 0));

	*ipp = ip;

	/*
	 * If we have a real type for an on-disk inode, we can set ops(&unlock)
	 * now.	 If it's a new inode being created, xfs_ialloc will handle it.
	 */
	XVFS_INIT_VNODE(XFS_MTOVFS(mp), vp, XFS_ITOBHV(ip), 1);

	return 0;
}

/*
 * Special iput for brand-new inodes that are still locked
 */
void
xfs_iput_new(xfs_inode_t	*ip,
	     uint		lock_flags)
{
	xfs_vnode_t		*vp = XFS_ITOV(ip);

	vn_trace_entry(vp, "xfs_iput_new", (inst_t *)__return_address);

	printf("xfs_iput_new: ip %p\n",ip);
	
	if ((ip->i_d.di_mode == 0)) {
		ASSERT(!(ip->i_flags & XFS_IRECLAIMABLE));
		//vn_mark_bad(vp);
		printf("xfs_iput_new: ip %p di_mode == 0\n",ip);
		/* mabe call vgone here? RMC */
	}
	if (lock_flags)
		xfs_iunlock(ip, lock_flags);

	ASSERT_VOP_LOCKED(vp->v_vnode, "xfs_iput_new");
	vput(vp->v_vnode);
}

extern struct vop_vector xfs_vnops;

static int
xfs_vn_allocate(xfs_mount_t *mp, xfs_inode_t *ip, struct xfs_vnode **vpp)
{
	struct vnode *vp;
	struct xfs_vnode *vdata;
	int error;

	/* Use zone allocator here? */
	vdata = kmem_zalloc(sizeof(*vdata), KM_SLEEP);

	error = getnewvnode("xfs", XVFSTOMNT(XFS_MTOVFS(mp)),
			    &xfs_vnops, &vp);
	if (error) {
		kmem_free(vdata, sizeof(*vdata));
		return (error);
	}

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	VN_LOCK_AREC(vp);
	error = insmntque(vp, XVFSTOMNT(XFS_MTOVFS(mp)));
	if (error != 0) {
		kmem_free(vdata, sizeof(*vdata));
		return (error);
	}

	vp->v_data = (void *)vdata;
	vdata->v_number= 0;
	vdata->v_inode = ip;
	vdata->v_vfsp  = XFS_MTOVFS(mp);
	vdata->v_vnode = vp;

 	vn_bhv_head_init(VN_BHV_HEAD(vdata), "vnode");


#ifdef  CONFIG_XFS_VNODE_TRACING
        vp->v_trace = ktrace_alloc(VNODE_TRACE_SIZE, KM_SLEEP);
#endif  /* CONFIG_XFS_VNODE_TRACING */

        vn_trace_exit(vp, "vn_initialize", (inst_t *)__return_address);

	if (error == 0)
		*vpp = vdata;

	return (error);
}
