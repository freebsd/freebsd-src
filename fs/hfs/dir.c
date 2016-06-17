/*
 * linux/fs/hfs/dir.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains directory-related functions independent of which
 * scheme is being used to represent forks.
 *
 * Based on the minix file system code, (C) 1991, 1992 by Linus Torvalds
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#include "hfs.h"
#include <linux/hfs_fs_sb.h>
#include <linux/hfs_fs_i.h>
#include <linux/hfs_fs.h>

/*================ File-local functions ================*/

/*
 * build_key()
 *
 * Build a key for a file by the given name in the given directory.
 * If the name matches one of the reserved names returns 1 otherwise 0.
 */
static int build_key(struct hfs_cat_key *key, struct inode *dir,
		     const char *name, int len)
{
	struct hfs_name cname;
	const struct hfs_name *reserved;

	/* mangle the name */
	hfs_nameout(dir, &cname, name, len);

	/* check against reserved names */
	reserved = HFS_SB(dir->i_sb)->s_reserved1;
	while (reserved->Len) {
		if (hfs_streq(reserved->Name, reserved->Len, 
			      cname.Name, cname.Len)) {
			return 1;
		}
		++reserved;
	}

	/* check against the names reserved only in the root directory */
	if (HFS_I(dir)->entry->cnid == htonl(HFS_ROOT_CNID)) {
		reserved = HFS_SB(dir->i_sb)->s_reserved2;
		while (reserved->Len) {
			if (hfs_streq(reserved->Name, reserved->Len,
				      cname.Name, cname.Len)) {
				return 1;
			}
			++reserved;
		}
	}

	/* build the key */
	hfs_cat_build_key(HFS_I(dir)->entry->cnid, &cname, key);

	return 0;
}

/*
 * update_dirs_plus()
 *
 * Update the fields 'i_size', 'i_nlink', 'i_ctime', 'i_mtime' and
 * 'i_version' of the inodes associated with a directory that has
 * had a file ('is_dir'==0) or directory ('is_dir'!=0) added to it.
 */
static inline void update_dirs_plus(struct hfs_cat_entry *dir, int is_dir)
{
	int i;

	for (i = 0; i < 4; ++i) {
		struct dentry *de = dir->sys_entry[i];
		if (de) {
		        struct inode *tmp = de->d_inode;
			if (S_ISDIR(tmp->i_mode)) {
				if (is_dir &&
				    (i == HFS_ITYPE_TO_INT(HFS_ITYPE_NORM))) {
					/* In "normal" directory only */
					++(tmp->i_nlink);
				}
				tmp->i_size += HFS_I(tmp)->dir_size;
				tmp->i_version = ++event;
			}
			tmp->i_ctime = tmp->i_mtime = CURRENT_TIME;
			mark_inode_dirty(tmp);
		}
	}
}

/*
 * update_dirs_minus()
 *
 * Update the fields 'i_size', 'i_nlink', 'i_ctime', 'i_mtime' and
 * 'i_version' of the inodes associated with a directory that has
 * had a file ('is_dir'==0) or directory ('is_dir'!=0) removed.
 */
static inline void update_dirs_minus(struct hfs_cat_entry *dir, int is_dir)
{
	int i;

	for (i = 0; i < 4; ++i) {
		struct dentry *de = dir->sys_entry[i];
		if (de) {
		        struct inode *tmp = de->d_inode;
			if (S_ISDIR(tmp->i_mode)) {
				if (is_dir &&
				    (i == HFS_ITYPE_TO_INT(HFS_ITYPE_NORM))) {
					/* In "normal" directory only */
					--(tmp->i_nlink);
				}
				tmp->i_size -= HFS_I(tmp)->dir_size;
				tmp->i_version = ++event;
			}
			tmp->i_ctime = tmp->i_mtime = CURRENT_TIME;
			mark_inode_dirty(tmp);
		}
	}
}

/*
 * mark_inodes_deleted()
 *
 * Update inodes associated with a deleted entry to reflect its deletion.
 * Well, we really just drop the dentry.
 *
 * XXX: we should be using delete_inode for some of this stuff.
 */
static inline void mark_inodes_deleted(struct hfs_cat_entry *entry, 
				       struct dentry *dentry)
{
	struct dentry *de;
	struct inode *tmp;
	int i;

	for (i = 0; i < 4; ++i) {
		if ((de = entry->sys_entry[i]) && (dentry != de)) {
		      dget(de);
		      tmp = de->d_inode;
		      tmp->i_nlink = 0;
		      tmp->i_ctime = CURRENT_TIME;
		      mark_inode_dirty(tmp);
		      d_delete(de);
		      dput(de);
		}
	}
}

/*================ Global functions ================*/

/*
 * hfs_create()
 *
 * This is the create() entry in the inode_operations structure for
 * regular HFS directories.  The purpose is to create a new file in
 * a directory and return a corresponding inode, given the inode for
 * the directory and the name (and its length) of the new file.
 */
int hfs_create(struct inode * dir, struct dentry *dentry, int mode)
{
	struct hfs_cat_entry *entry = HFS_I(dir)->entry;
	struct hfs_cat_entry *new;
	struct hfs_cat_key key;
	struct inode *inode;
	int error;

	/* build the key, checking against reserved names */
	if (build_key(&key, dir, dentry->d_name.name, dentry->d_name.len)) 
		return -EEXIST;

	if ((error = hfs_cat_create(entry, &key, 
			       (mode & S_IWUSR) ? 0 : HFS_FIL_LOCK,
			       HFS_SB(dir->i_sb)->s_type,
			       HFS_SB(dir->i_sb)->s_creator, &new)))
		return error;

	/* create an inode for the new file. back out if we run
	 * into trouble. */
	new->count++; /* hfs_iget() eats one */
	if (!(inode = hfs_iget(new, HFS_I(dir)->file_type, dentry))) {
		hfs_cat_delete(entry, new, 1);
		hfs_cat_put(new);
		return -EIO;
	}

	hfs_cat_put(new);
	update_dirs_plus(entry, 0);
	/* toss any relevant negative dentries */
	if (HFS_I(dir)->d_drop_op)
		HFS_I(dir)->d_drop_op(dentry, HFS_I(dir)->file_type);
	mark_inode_dirty(inode);
	d_instantiate(dentry, inode);
	return 0;
}

/*
 * hfs_mkdir()
 *
 * This is the mkdir() entry in the inode_operations structure for
 * regular HFS directories.  The purpose is to create a new directory
 * in a directory, given the inode for the parent directory and the
 * name (and its length) of the new directory.
 */
int hfs_mkdir(struct inode * parent, struct dentry *dentry, int mode)
{
	struct hfs_cat_entry *entry = HFS_I(parent)->entry;
	struct hfs_cat_entry *new;
	struct hfs_cat_key key;
	struct inode *inode;
	int error;

	/* build the key, checking against reserved names */
	if (build_key(&key, parent, dentry->d_name.name, 
		      dentry->d_name.len)) 
		return -EEXIST;

	/* try to create the directory */
	if ((error = hfs_cat_mkdir(entry, &key, &new)))
		return error;

	/* back out if we run into trouble */
	new->count++; /* hfs_iget eats one */
	if (!(inode = hfs_iget(new, HFS_I(parent)->file_type, dentry))) {
		hfs_cat_delete(entry, new, 1);
		hfs_cat_put(new);
		return -EIO;
	}

	hfs_cat_put(new);
	update_dirs_plus(entry, 1);
	mark_inode_dirty(inode);
	d_instantiate(dentry, inode);
	return 0;
}

/*
 * hfs_unlink()
 *
 * This is the unlink() entry in the inode_operations structure for
 * regular HFS directories.  The purpose is to delete an existing
 * file, given the inode for the parent directory and the name
 * (and its length) of the existing file.
 */
int hfs_unlink(struct inode * dir, struct dentry *dentry)
{
	struct hfs_cat_entry *entry = HFS_I(dir)->entry;
	struct hfs_cat_entry *victim = NULL;
	struct hfs_cat_key key;
	int error;

	if (build_key(&key, dir, dentry->d_name.name,
		      dentry->d_name.len)) 
		return -EPERM;

	if (!(victim = hfs_cat_get(entry->mdb, &key))) 
		return -ENOENT;

	error = -EPERM;
	if (victim->type != HFS_CDR_FIL)
		goto hfs_unlink_put;

	if (!(error = hfs_cat_delete(entry, victim, 1))) {
		struct inode *inode = dentry->d_inode;

		mark_inodes_deleted(victim, dentry);
		inode->i_nlink--; 
		inode->i_ctime = CURRENT_TIME;
		mark_inode_dirty(inode);
		update_dirs_minus(entry, 0);
	}

hfs_unlink_put:
	hfs_cat_put(victim);	/* Note that hfs_cat_put(NULL) is safe. */
	return error;
}

/*
 * hfs_rmdir()
 *
 * This is the rmdir() entry in the inode_operations structure for
 * regular HFS directories.  The purpose is to delete an existing
 * directory, given the inode for the parent directory and the name
 * (and its length) of the existing directory.
 */
int hfs_rmdir(struct inode * parent, struct dentry *dentry)
{
	struct hfs_cat_entry *entry = HFS_I(parent)->entry;
	struct hfs_cat_entry *victim = NULL;
	struct inode *inode = dentry->d_inode;
	struct hfs_cat_key key;
	int error;

	if (build_key(&key, parent, dentry->d_name.name,
		      dentry->d_name.len))
		return -EPERM;

	if (!(victim = hfs_cat_get(entry->mdb, &key)))
		return -ENOENT;

	error = -ENOTDIR;
	if (victim->type != HFS_CDR_DIR) 
		goto hfs_rmdir_put;

	error = -EBUSY;
	if (!d_unhashed(dentry))
		goto hfs_rmdir_put;

	/* we only have to worry about 2 and 3 for mount points */
	if (victim->sys_entry[2] && d_mountpoint(victim->sys_entry[2]))
		goto hfs_rmdir_put;
	if (victim->sys_entry[3] && d_mountpoint(victim->sys_entry[3])) 
		goto hfs_rmdir_put;

	
	if ((error = hfs_cat_delete(entry, victim, 1)))
		goto hfs_rmdir_put;

	mark_inodes_deleted(victim, dentry);
	inode->i_nlink = 0;
	inode->i_ctime = CURRENT_TIME;
	mark_inode_dirty(inode);
	update_dirs_minus(entry, 1);
	 
hfs_rmdir_put:
	hfs_cat_put(victim);	/* Note that hfs_cat_put(NULL) is safe. */
	return error;
}

/*
 * hfs_rename()
 *
 * This is the rename() entry in the inode_operations structure for
 * regular HFS directories.  The purpose is to rename an existing
 * file or directory, given the inode for the current directory and
 * the name (and its length) of the existing file/directory and the
 * inode for the new directory and the name (and its length) of the
 * new file/directory.
 * XXX: how do you handle must_be dir?
 */
int hfs_rename(struct inode *old_dir, struct dentry *old_dentry,
	       struct inode *new_dir, struct dentry *new_dentry)
{
	struct hfs_cat_entry *old_parent = HFS_I(old_dir)->entry;
	struct hfs_cat_entry *new_parent = HFS_I(new_dir)->entry;
	struct hfs_cat_entry *victim = NULL;
	struct hfs_cat_entry *deleted;
	struct hfs_cat_key key;
	int error;

	if (build_key(&key, old_dir, old_dentry->d_name.name,
		      old_dentry->d_name.len) ||
	    (HFS_ITYPE(old_dir->i_ino) != HFS_ITYPE(new_dir->i_ino))) 
		return -EPERM;

	if (!(victim = hfs_cat_get(old_parent->mdb, &key))) 
		return -ENOENT;

	error = -EPERM;
	if (build_key(&key, new_dir, new_dentry->d_name.name,
			     new_dentry->d_name.len)) 
		goto hfs_rename_put;

	if (!(error = hfs_cat_move(old_parent, new_parent,
				   victim, &key, &deleted))) {
		int is_dir = (victim->type == HFS_CDR_DIR);
		
		/* drop the old dentries */
		mark_inodes_deleted(victim, old_dentry);
		update_dirs_minus(old_parent, is_dir);
		if (deleted) {
			mark_inodes_deleted(deleted, new_dentry);
			hfs_cat_put(deleted);
		} else {
			/* no existing inodes. just drop negative dentries */
			if (HFS_I(new_dir)->d_drop_op) 
				HFS_I(new_dir)->d_drop_op(new_dentry, 
					  HFS_I(new_dir)->file_type);
			update_dirs_plus(new_parent, is_dir);
		}
	
	}

hfs_rename_put:
	hfs_cat_put(victim);	/* Note that hfs_cat_put(NULL) is safe. */
	return error;
}
