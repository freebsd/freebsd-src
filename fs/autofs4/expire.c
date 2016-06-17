/* -*- c -*- --------------------------------------------------------------- *
 *
 * linux/fs/autofs/expire.c
 *
 *  Copyright 1997-1998 Transmeta Corporation -- All Rights Reserved
 *  Copyright 1999-2000 Jeremy Fitzhardinge <jeremy@goop.org>
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * ------------------------------------------------------------------------- */

#include "autofs_i.h"

/*
 * Determine if a subtree of the namespace is busy.
 *
 * mnt is the mount tree under the autofs mountpoint
 */
static inline int is_vfsmnt_tree_busy(struct vfsmount *mnt)
{
	struct vfsmount *this_parent = mnt;
	struct list_head *next;
	int count;

	count = atomic_read(&mnt->mnt_count) - 1;

repeat:
	next = this_parent->mnt_mounts.next;
	DPRINTK(("is_vfsmnt_tree_busy: mnt=%p, this_parent=%p, next=%p\n",
		 mnt, this_parent, next));
resume:
	for( ; next != &this_parent->mnt_mounts; next = next->next) {
		struct vfsmount *p = list_entry(next, struct vfsmount,
						mnt_child);

		/* -1 for struct vfs_mount's normal count, 
		   -1 to compensate for child's reference to parent */
		count += atomic_read(&p->mnt_count) - 1 - 1;

		DPRINTK(("is_vfsmnt_tree_busy: p=%p, count now %d\n",
			 p, count));

		if (!list_empty(&p->mnt_mounts)) {
			this_parent = p;
			goto repeat;
		}
		/* root is busy if any leaf is busy */
		if (atomic_read(&p->mnt_count) > 1)
			return 1;
	}

	/* All done at this level ... ascend and resume the search. */
	if (this_parent != mnt) {
		next = this_parent->mnt_child.next; 
		this_parent = this_parent->mnt_parent;
		goto resume;
	}

	DPRINTK(("is_vfsmnt_tree_busy: count=%d\n", count));
	return count != 0; /* remaining users? */
}

/* Traverse a dentry's list of vfsmounts and return the number of
   non-busy mounts */
static int check_vfsmnt(struct vfsmount *mnt, struct dentry *dentry)
{
	int ret = dentry->d_mounted;
	struct vfsmount *vfs = lookup_mnt(mnt, dentry);

	if (vfs && is_vfsmnt_tree_busy(vfs))
		ret--;
	DPRINTK(("check_vfsmnt: ret=%d\n", ret));
	return ret;
}

/* Check dentry tree for busyness.  If a dentry appears to be busy
   because it is a mountpoint, check to see if the mounted
   filesystem is busy. */
static int is_tree_busy(struct vfsmount *topmnt, struct dentry *top)
{
	struct dentry *this_parent;
	struct list_head *next;
	int count;

	count = atomic_read(&top->d_count);
	
	DPRINTK(("is_tree_busy: top=%p initial count=%d\n", 
		 top, count));
	this_parent = top;

	if (is_autofs4_dentry(top)) {
		count--;
		DPRINTK(("is_tree_busy: autofs; count=%d\n", count));
	}

	if (d_mountpoint(top))
		count -= check_vfsmnt(topmnt, top);

 repeat:
	next = this_parent->d_subdirs.next;
 resume:
	while (next != &this_parent->d_subdirs) {
		int adj = 0;
		struct dentry *dentry = list_entry(next, struct dentry,
						   d_child);
		next = next->next;

		count += atomic_read(&dentry->d_count) - 1;

		if (d_mountpoint(dentry))
			adj += check_vfsmnt(topmnt, dentry);

		if (is_autofs4_dentry(dentry)) {
			adj++;
			DPRINTK(("is_tree_busy: autofs; adj=%d\n",
				 adj));
		}

		count -= adj;

		if (!list_empty(&dentry->d_subdirs)) {
			this_parent = dentry;
			goto repeat;
		}

		if (atomic_read(&dentry->d_count) != adj) {
			DPRINTK(("is_tree_busy: busy leaf (d_count=%d adj=%d)\n",
				 atomic_read(&dentry->d_count), adj));
			return 1;
		}
	}

	/* All done at this level ... ascend and resume the search. */
	if (this_parent != top) {
		next = this_parent->d_child.next; 
		this_parent = this_parent->d_parent;
		goto resume;
	}

	DPRINTK(("is_tree_busy: count=%d\n", count));
	return count != 0; /* remaining users? */
}

/*
 * Find an eligible tree to time-out
 * A tree is eligible if :-
 *  - it is unused by any user process
 *  - it has been unused for exp_timeout time
 */
static struct dentry *autofs4_expire(struct super_block *sb,
				     struct vfsmount *mnt,
				     struct autofs_sb_info *sbi,
				     int do_now)
{
	unsigned long now = jiffies;
	unsigned long timeout;
	struct dentry *root = sb->s_root;
	struct list_head *tmp;

	if (!sbi->exp_timeout || !root)
		return NULL;

	timeout = sbi->exp_timeout;

	spin_lock(&dcache_lock);
	for(tmp = root->d_subdirs.next;
	    tmp != &root->d_subdirs; 
	    tmp = tmp->next) {
		struct autofs_info *ino;
		struct dentry *dentry = list_entry(tmp, struct dentry, d_child);

		if (dentry->d_inode == NULL)
			continue;

		ino = autofs4_dentry_ino(dentry);

		if (ino == NULL) {
			/* dentry in the process of being deleted */
			continue;
		}

		/* No point expiring a pending mount */
		if (dentry->d_flags & DCACHE_AUTOFS_PENDING)
			continue;

		if (!do_now) {
			/* Too young to die */
			if (time_after(ino->last_used + timeout, now))
				continue;
		
			/* update last_used here :- 
			   - obviously makes sense if it is in use now
			   - less obviously, prevents rapid-fire expire
			     attempts if expire fails the first time */
			ino->last_used = now;
		}
		if (!is_tree_busy(mnt, dentry)) {
			DPRINTK(("autofs_expire: returning %p %.*s\n",
				 dentry, (int)dentry->d_name.len, dentry->d_name.name));
			/* Start from here next time */
			list_del(&root->d_subdirs);
			list_add(&root->d_subdirs, &dentry->d_child);
			dget(dentry);
			spin_unlock(&dcache_lock);

			return dentry;
		}
	}
	spin_unlock(&dcache_lock);

	return NULL;
}

/* Perform an expiry operation */
int autofs4_expire_run(struct super_block *sb,
		      struct vfsmount *mnt,
		      struct autofs_sb_info *sbi,
		      struct autofs_packet_expire *pkt_p)
{
	struct autofs_packet_expire pkt;
	struct dentry *dentry;

	memset(&pkt,0,sizeof pkt);

	pkt.hdr.proto_version = sbi->version;
	pkt.hdr.type = autofs_ptype_expire;

	if ((dentry = autofs4_expire(sb, mnt, sbi, 0)) == NULL)
		return -EAGAIN;

	pkt.len = dentry->d_name.len;
	memcpy(pkt.name, dentry->d_name.name, pkt.len);
	pkt.name[pkt.len] = '\0';
	dput(dentry);

	if ( copy_to_user(pkt_p, &pkt, sizeof(struct autofs_packet_expire)) )
		return -EFAULT;

	return 0;
}

/* Call repeatedly until it returns -EAGAIN, meaning there's nothing
   more to be done */
int autofs4_expire_multi(struct super_block *sb, struct vfsmount *mnt,
			struct autofs_sb_info *sbi, int *arg)
{
	struct dentry *dentry;
	int ret = -EAGAIN;
	int do_now = 0;

	if (arg && get_user(do_now, arg))
		return -EFAULT;

	if ((dentry = autofs4_expire(sb, mnt, sbi, do_now)) != NULL) {
		struct autofs_info *de_info = autofs4_dentry_ino(dentry);

		/* This is synchronous because it makes the daemon a
                   little easier */
		de_info->flags |= AUTOFS_INF_EXPIRING;
		ret = autofs4_wait(sbi, &dentry->d_name, NFY_EXPIRE);
		de_info->flags &= ~AUTOFS_INF_EXPIRING;
		dput(dentry);
	}
		
	return ret;
}

