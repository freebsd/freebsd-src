/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "xfs_itable.h"
#include "xfs_btree.h"
#include "xfs_alloc.h"
#include "xfs_ialloc.h"
#include "xfs_attr.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_error.h"
#include "xfs_buf_item.h"
#include "xfs_refcache.h"

STATIC lock_t		xfs_refcache_lock;
STATIC xfs_inode_t	**xfs_refcache;
STATIC int		xfs_refcache_index;
STATIC int		xfs_refcache_busy;
STATIC int		xfs_refcache_count;

void
xfs_refcache_init(void)
{
	spinlock_init(&xfs_refcache_lock, "xfs_refcache");
}
/*
 * Insert the given inode into the reference cache.
 */
void
xfs_refcache_insert(
	xfs_inode_t	*ip)
{
	vnode_t		*vp;
	xfs_inode_t	*release_ip;
	xfs_inode_t	**refcache;

	ASSERT(ismrlocked(&(ip->i_iolock), MR_UPDATE));

	/*
	 * If an unmount is busy blowing entries out of the cache,
	 * then don't bother.
	 */
	if (xfs_refcache_busy) {
		return;
	}

	/*
	 * If we tuned the refcache down to zero, don't do anything.
	 */
	 if (!xfs_refcache_size) {
		return;
	}

	/*
	 * The inode is already in the refcache, so don't bother
	 * with it.
	 */
	if (ip->i_refcache != NULL) {
		return;
	}

	vp = XFS_ITOV(ip);
	/* ASSERT(vp->v_count > 0); */
	VN_HOLD(vp);

	/*
	 * We allocate the reference cache on use so that we don't
	 * waste the memory on systems not being used as NFS servers.
	 */
	if (xfs_refcache == NULL) {
		refcache = (xfs_inode_t **)kmem_zalloc(XFS_REFCACHE_SIZE_MAX *
						       sizeof(xfs_inode_t *),
						       KM_SLEEP);
	} else {
		refcache = NULL;
	}

	spin_lock(&xfs_refcache_lock);

	/*
	 * If we allocated memory for the refcache above and it still
	 * needs it, then use the memory we allocated.  Otherwise we'll
	 * free the memory below.
	 */
	if (refcache != NULL) {
		if (xfs_refcache == NULL) {
			xfs_refcache = refcache;
			refcache = NULL;
		}
	}

	/*
	 * If an unmount is busy clearing out the cache, don't add new
	 * entries to it.
	 */
	if (xfs_refcache_busy) {
		spin_unlock(&xfs_refcache_lock);
		VN_RELE(vp);
		/*
		 * If we allocated memory for the refcache above but someone
		 * else beat us to using it, then free the memory now.
		 */
		if (refcache != NULL) {
			kmem_free(refcache,
				  XFS_REFCACHE_SIZE_MAX * sizeof(xfs_inode_t *));
		}
		return;
	}
	release_ip = xfs_refcache[xfs_refcache_index];
	if (release_ip != NULL) {
		release_ip->i_refcache = NULL;
		xfs_refcache_count--;
		ASSERT(xfs_refcache_count >= 0);
	}
	xfs_refcache[xfs_refcache_index] = ip;
	ASSERT(ip->i_refcache == NULL);
	ip->i_refcache = &(xfs_refcache[xfs_refcache_index]);
	xfs_refcache_count++;
	ASSERT(xfs_refcache_count <= xfs_refcache_size);
	xfs_refcache_index++;
	if (xfs_refcache_index == xfs_refcache_size) {
		xfs_refcache_index = 0;
	}
	spin_unlock(&xfs_refcache_lock);

	/*
	 * Save the pointer to the inode to be released so that we can
	 * VN_RELE it once we've dropped our inode locks in xfs_rwunlock().
	 * The pointer may be NULL, but that's OK.
	 */
	ip->i_release = release_ip;

	/*
	 * If we allocated memory for the refcache above but someone
	 * else beat us to using it, then free the memory now.
	 */
	if (refcache != NULL) {
		kmem_free(refcache,
			  XFS_REFCACHE_SIZE_MAX * sizeof(xfs_inode_t *));
	}
}


/*
 * If the given inode is in the reference cache, purge its entry and
 * release the reference on the vnode.
 */
void
xfs_refcache_purge_ip(
	xfs_inode_t	*ip)
{
	vnode_t	*vp;
	int	error;

	/*
	 * If we're not pointing to our entry in the cache, then
	 * we must not be in the cache.
	 */
	if (ip->i_refcache == NULL) {
		return;
	}

	spin_lock(&xfs_refcache_lock);
	if (ip->i_refcache == NULL) {
		spin_unlock(&xfs_refcache_lock);
		return;
	}

	/*
	 * Clear both our pointer to the cache entry and its pointer
	 * back to us.
	 */
	ASSERT(*(ip->i_refcache) == ip);
	*(ip->i_refcache) = NULL;
	ip->i_refcache = NULL;
	xfs_refcache_count--;
	ASSERT(xfs_refcache_count >= 0);
	spin_unlock(&xfs_refcache_lock);

	vp = XFS_ITOV(ip);
	/* ASSERT(vp->v_count > 1); */
	VOP_RELEASE(vp, error);
	VN_RELE(vp);
}


/*
 * This is called from the XFS unmount code to purge all entries for the
 * given mount from the cache.  It uses the refcache busy counter to
 * make sure that new entries are not added to the cache as we purge them.
 */
void
xfs_refcache_purge_mp(
	xfs_mount_t	*mp)
{
	vnode_t		*vp;
	int		error, i;
	xfs_inode_t	*ip;

	if (xfs_refcache == NULL) {
		return;
	}

	spin_lock(&xfs_refcache_lock);
	/*
	 * Bumping the busy counter keeps new entries from being added
	 * to the cache.  We use a counter since multiple unmounts could
	 * be in here simultaneously.
	 */
	xfs_refcache_busy++;

	for (i = 0; i < xfs_refcache_size; i++) {
		ip = xfs_refcache[i];
		if ((ip != NULL) && (ip->i_mount == mp)) {
			xfs_refcache[i] = NULL;
			ip->i_refcache = NULL;
			xfs_refcache_count--;
			ASSERT(xfs_refcache_count >= 0);
			spin_unlock(&xfs_refcache_lock);
			vp = XFS_ITOV(ip);
			VOP_RELEASE(vp, error);
			VN_RELE(vp);
			spin_lock(&xfs_refcache_lock);
		}
	}

	xfs_refcache_busy--;
	ASSERT(xfs_refcache_busy >= 0);
	spin_unlock(&xfs_refcache_lock);
}


/*
 * This is called from the XFS sync code to ensure that the refcache
 * is emptied out over time.  We purge a small number of entries with
 * each call.
 */
void
xfs_refcache_purge_some(xfs_mount_t *mp)
{
	int		error, i;
	xfs_inode_t	*ip;
	int		iplist_index;
	xfs_inode_t	**iplist;

	if ((xfs_refcache == NULL) || (xfs_refcache_count == 0)) {
		return;
	}

	iplist_index = 0;
	iplist = (xfs_inode_t **)kmem_zalloc(xfs_refcache_purge_count *
					  sizeof(xfs_inode_t *), KM_SLEEP);

	spin_lock(&xfs_refcache_lock);

	/*
	 * Store any inodes we find in the next several entries
	 * into the iplist array to be released after dropping
	 * the spinlock.  We always start looking from the currently
	 * oldest place in the cache.  We move the refcache index
	 * forward as we go so that we are sure to eventually clear
	 * out the entire cache when the system goes idle.
	 */
	for (i = 0; i < xfs_refcache_purge_count; i++) {
		ip = xfs_refcache[xfs_refcache_index];
		if (ip != NULL) {
			xfs_refcache[xfs_refcache_index] = NULL;
			ip->i_refcache = NULL;
			xfs_refcache_count--;
			ASSERT(xfs_refcache_count >= 0);
			iplist[iplist_index] = ip;
			iplist_index++;
		}
		xfs_refcache_index++;
		if (xfs_refcache_index == xfs_refcache_size) {
			xfs_refcache_index = 0;
		}
	}

	spin_unlock(&xfs_refcache_lock);

	/*
	 * Now drop the inodes we collected.
	 */
	for (i = 0; i < iplist_index; i++) {
		VOP_RELEASE(XFS_ITOV(iplist[i]), error);
		VN_RELE(XFS_ITOV(iplist[i]));
	}

	kmem_free(iplist, xfs_refcache_purge_count *
			  sizeof(xfs_inode_t *));
}

/*
 * This is called when the refcache is dynamically resized
 * via a sysctl.
 *
 * If the new size is smaller than the old size, purge all
 * entries in slots greater than the new size, and move
 * the index if necessary.
 *
 * If the refcache hasn't even been allocated yet, or the
 * new size is larger than the old size, just set the value
 * of xfs_refcache_size.
 */

void
xfs_refcache_resize(int xfs_refcache_new_size)
{
	int		i;
	xfs_inode_t	*ip;
	int		iplist_index = 0;
	xfs_inode_t	**iplist;
	int		error;

	/*
	 * If the new size is smaller than the current size,
	 * purge entries to create smaller cache, and
	 * reposition index if necessary.
	 * Don't bother if no refcache yet.
	 */
	if (xfs_refcache && (xfs_refcache_new_size < xfs_refcache_size)) {

		iplist = (xfs_inode_t **)kmem_zalloc(XFS_REFCACHE_SIZE_MAX *
				sizeof(xfs_inode_t *), KM_SLEEP);

		spin_lock(&xfs_refcache_lock);

		for (i = xfs_refcache_new_size; i < xfs_refcache_size; i++) {
			ip = xfs_refcache[i];
			if (ip != NULL) {
				xfs_refcache[i] = NULL;
				ip->i_refcache = NULL;
				xfs_refcache_count--;
				ASSERT(xfs_refcache_count >= 0);
				iplist[iplist_index] = ip;
				iplist_index++;
			}
		}

		xfs_refcache_size = xfs_refcache_new_size;

		/*
		 * Move index to beginning of cache if it's now past the end
		 */
		if (xfs_refcache_index >= xfs_refcache_new_size)
			xfs_refcache_index = 0;

		spin_unlock(&xfs_refcache_lock);

		/*
		 * Now drop the inodes we collected.
		 */
		for (i = 0; i < iplist_index; i++) {
			VOP_RELEASE(XFS_ITOV(iplist[i]), error);
			VN_RELE(XFS_ITOV(iplist[i]));
		}

		kmem_free(iplist, XFS_REFCACHE_SIZE_MAX *
				  sizeof(xfs_inode_t *));
	} else {
		spin_lock(&xfs_refcache_lock);
		xfs_refcache_size = xfs_refcache_new_size;
		spin_unlock(&xfs_refcache_lock);
	}
}

void
xfs_refcache_iunlock(
	xfs_inode_t	*ip,
	uint		lock_flags)
{
	xfs_inode_t	*release_ip;
	int		error;

	release_ip = ip->i_release;
	ip->i_release = NULL;

	xfs_iunlock(ip, lock_flags);

	if (release_ip != NULL) {
		VOP_RELEASE(XFS_ITOV(release_ip), error);
		VN_RELE(XFS_ITOV(release_ip));
	}
}

void
xfs_refcache_destroy(void)
{
	if (xfs_refcache) {
		kmem_free(xfs_refcache,
			XFS_REFCACHE_SIZE_MAX * sizeof(xfs_inode_t *));
		xfs_refcache = NULL;
	}
	spinlock_destroy(&xfs_refcache_lock);
}
