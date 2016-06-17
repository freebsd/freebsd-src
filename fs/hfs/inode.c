/*
 * linux/fs/hfs/inode.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains inode-related functions which do not depend on
 * which scheme is being used to represent forks.
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
#include <linux/smp_lock.h>

/*================ Variable-like macros ================*/

#define HFS_VALID_MODE_BITS  (S_IFREG | S_IFDIR | S_IRWXUGO)

/*================ File-local functions ================*/

/*
 * init_file_inode()
 *
 * Given an HFS catalog entry initialize an inode for a file.
 */
static void init_file_inode(struct inode *inode, hfs_u8 fork)
{
	struct hfs_fork *fk;
	struct hfs_cat_entry *entry = HFS_I(inode)->entry;

	if (fork == HFS_FK_DATA) {
		inode->i_mode = S_IRWXUGO | S_IFREG;
	} else {
		inode->i_mode = S_IRUGO | S_IWUGO | S_IFREG;
	}

	if (fork == HFS_FK_DATA) {
#if 0 /* XXX: disable crlf translations for now */
		hfs_u32 type = hfs_get_nl(entry->info.file.finfo.fdType);

		HFS_I(inode)->convert =
			((HFS_SB(inode->i_sb)->s_conv == 't') ||
			 ((HFS_SB(inode->i_sb)->s_conv == 'a') &&
			  ((type == htonl(0x54455854)) ||   /* "TEXT" */
			   (type == htonl(0x7474726f)))));  /* "ttro" */
#else
		HFS_I(inode)->convert = 0;
#endif
		fk = &entry->u.file.data_fork;
	} else {
		fk = &entry->u.file.rsrc_fork;
		HFS_I(inode)->convert = 0;
	}
	HFS_I(inode)->fork = fk;
	inode->i_size = fk->lsize;
	inode->i_blocks = fk->psize;
	inode->i_nlink = 1;
}

/*================ Global functions ================*/

/*
 * hfs_put_inode()
 *
 * This is the put_inode() entry in the super_operations for HFS
 * filesystems.  The purpose is to perform any filesystem-dependent 
 * cleanup necessary when the use-count of an inode falls to zero.
 */
void hfs_put_inode(struct inode * inode)
{
	struct hfs_cat_entry *entry = HFS_I(inode)->entry;

	lock_kernel();
	hfs_cat_put(entry);
	if (atomic_read(&inode->i_count) == 1) {
	  struct hfs_hdr_layout *tmp = HFS_I(inode)->layout;

	  if (tmp) {
		HFS_I(inode)->layout = NULL;
		HFS_DELETE(tmp);
	  }
	}
	unlock_kernel();
}

/*
 * hfs_notify_change()
 *
 * Based very closely on fs/msdos/inode.c by Werner Almesberger
 *
 * This is the notify_change() field in the super_operations structure
 * for HFS file systems.  The purpose is to take that changes made to
 * an inode and apply then in a filesystem-dependent manner.  In this
 * case the process has a few of tasks to do:
 *  1) prevent changes to the i_uid and i_gid fields.
 *  2) map file permissions to the closest allowable permissions
 *  3) Since multiple Linux files can share the same on-disk inode under
 *     HFS (for instance the data and resource forks of a file) a change
 *     to permissions must be applied to all other in-core inodes which 
 *     correspond to the same HFS file.
 */
enum {HFS_NORM, HFS_HDR, HFS_CAP};

static int __hfs_notify_change(struct dentry *dentry, struct iattr * attr, int kind)
{
	struct inode *inode = dentry->d_inode;
	struct hfs_cat_entry *entry = HFS_I(inode)->entry;
	struct dentry **de = entry->sys_entry;
	struct hfs_sb_info *hsb = HFS_SB(inode->i_sb);
	int error, i;

	error = inode_change_ok(inode, attr); /* basic permission checks */
	if (error) {
		/* Let netatalk's afpd think chmod() always succeeds */
		if (hsb->s_afpd &&
		    (attr->ia_valid == (ATTR_MODE | ATTR_CTIME))) {
			return 0;
		} else {
			return error;
		}
	}

	/* no uig/gid changes and limit which mode bits can be set */
	if (((attr->ia_valid & ATTR_UID) && 
	     (attr->ia_uid != hsb->s_uid)) ||
	    ((attr->ia_valid & ATTR_GID) && 
	     (attr->ia_gid != hsb->s_gid)) ||
	    ((attr->ia_valid & ATTR_MODE) &&
	     (((entry->type == HFS_CDR_DIR) &&
	       (attr->ia_mode != inode->i_mode))||
	      (attr->ia_mode & ~HFS_VALID_MODE_BITS)))) {
		return hsb->s_quiet ? 0 : error;
	}
	
	if (entry->type == HFS_CDR_DIR) {
		attr->ia_valid &= ~ATTR_MODE;
	} else if (attr->ia_valid & ATTR_MODE) {
		/* Only the 'w' bits can ever change and only all together. */
		if (attr->ia_mode & S_IWUSR) {
			attr->ia_mode = inode->i_mode | S_IWUGO;
		} else {
			attr->ia_mode = inode->i_mode & ~S_IWUGO;
		}
		attr->ia_mode &= ~hsb->s_umask;
	}
	/*
	 * Normal files handle size change in normal way.
	 * Oddballs are served here.
	 */
	if (attr->ia_valid & ATTR_SIZE) {
		if (kind == HFS_CAP) {
			inode->i_size = attr->ia_size;
			if (inode->i_size > HFS_FORK_MAX)
				inode->i_size = HFS_FORK_MAX;
			mark_inode_dirty(inode);
			attr->ia_valid &= ~ATTR_SIZE;
		} else if (kind == HFS_HDR) {
			hdr_truncate(inode, attr->ia_size);
			attr->ia_valid &= ~ATTR_SIZE;
		}
	}
	error = inode_setattr(inode, attr);
	if (error)
		return error;

	/* We wouldn't want to mess with the sizes of the other fork */
	attr->ia_valid &= ~ATTR_SIZE;

	/* We must change all in-core inodes corresponding to this file. */
	for (i = 0; i < 4; ++i) {
	  if (de[i] && (de[i] != dentry)) {
		inode_setattr(de[i]->d_inode, attr);
	  }
	}

	/* Change the catalog entry if needed */
	if (attr->ia_valid & ATTR_MTIME) {
		entry->modify_date = hfs_u_to_mtime(inode->i_mtime);
		hfs_cat_mark_dirty(entry);
	}
	if (attr->ia_valid & ATTR_MODE) {
		hfs_u8 new_flags;

		if (inode->i_mode & S_IWUSR) {
			new_flags = entry->u.file.flags & ~HFS_FIL_LOCK;
		} else {
			new_flags = entry->u.file.flags | HFS_FIL_LOCK;
		}

		if (new_flags != entry->u.file.flags) {
			entry->u.file.flags = new_flags;
			hfs_cat_mark_dirty(entry);
		}
	}
	/* size changes handled in hfs_extent_adj() */

	return 0;
}

int hfs_notify_change(struct dentry *dentry, struct iattr * attr)
{
	return __hfs_notify_change(dentry, attr, HFS_NORM);
}

int hfs_notify_change_cap(struct dentry *dentry, struct iattr * attr)
{
	return __hfs_notify_change(dentry, attr, HFS_CAP);
}

int hfs_notify_change_hdr(struct dentry *dentry, struct iattr * attr)
{
	return __hfs_notify_change(dentry, attr, HFS_HDR);
}

static int hfs_writepage(struct page *page)
{
	return block_write_full_page(page,hfs_get_block);
}
static int hfs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page,hfs_get_block);
}
static int hfs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return cont_prepare_write(page,from,to,hfs_get_block,
		&page->mapping->host->u.hfs_i.mmu_private);
}
static int hfs_bmap(struct address_space *mapping, long block)
{
	return generic_block_bmap(mapping,block,hfs_get_block);
}
struct address_space_operations hfs_aops = {
	readpage: hfs_readpage,
	writepage: hfs_writepage,
	sync_page: block_sync_page,
	prepare_write: hfs_prepare_write,
	commit_write: generic_commit_write,
	bmap: hfs_bmap
};

/*
 * __hfs_iget()
 *
 * Given the MDB for a HFS filesystem, a 'key' and an 'entry' in
 * the catalog B-tree and the 'type' of the desired file return the
 * inode for that file/directory or NULL.  Note that 'type' indicates
 * whether we want the actual file or directory, or the corresponding
 * metadata (AppleDouble header file or CAP metadata file).
 *
 * In an ideal world we could call iget() and would not need this
 * function.  However, since there is no way to even know the inode
 * number until we've found the file/directory in the catalog B-tree
 * that simply won't happen.
 *
 * The main idea here is to look in the catalog B-tree to get the
 * vital info about the file or directory (including the file id which
 * becomes the inode number) and then to call iget() and return the
 * inode if it is complete.  If it is not then we use the catalog
 * entry to fill in the missing info, by calling the appropriate
 * 'fillin' function.  Note that these fillin functions are
 * essentially hfs_*_read_inode() functions, but since there is no way
 * to pass the catalog entry through iget() to such a read_inode()
 * function, we have to call them after iget() returns an incomplete
 * inode to us.	 This is pretty much the same problem faced in the NFS
 * code, and pretty much the same solution. The SMB filesystem deals
 * with this in a different way: by using the address of the
 * kmalloc()'d space which holds the data as the inode number.
 *
 * XXX: Both this function and NFS's corresponding nfs_fhget() would
 * benefit from a way to pass an additional (void *) through iget() to
 * the VFS read_inode() function.
 *
 * this will hfs_cat_put() the entry if it fails.
 */
struct inode *hfs_iget(struct hfs_cat_entry *entry, ino_t type,
		       struct dentry *dentry)
{
	struct dentry **sys_entry;
	struct super_block *sb;
	struct inode *inode;

	if (!entry) {
		return NULL;
	}

	/* If there are several processes all calling __iget() for
	   the same inode then they will all get the same one back.
	   The first one to return from __iget() will notice that the
	   i_mode field of the inode is blank and KNOW that it is
	   the first to return.  Therefore, it will set the appropriate
	   'sys_entry' field in the entry and initialize the inode.
	   All the initialization must be done without sleeping,
	   or else other processes could end up using a partially
	   initialized inode.				*/

	sb = entry->mdb->sys_mdb;
	sys_entry = &entry->sys_entry[HFS_ITYPE_TO_INT(type)];

	if (!(inode = iget(sb, ntohl(entry->cnid) | type))) {
	        hfs_cat_put(entry);
	        return NULL;
	}

	if (inode->i_dev != sb->s_dev) {
	        iput(inode); /* automatically does an hfs_cat_put */
		inode = NULL;
	} else if (!inode->i_mode || (*sys_entry == NULL)) {
		/* Initialize the inode */
		struct hfs_sb_info *hsb = HFS_SB(sb);

		inode->i_rdev = 0;
		inode->i_ctime = inode->i_atime = inode->i_mtime =
					hfs_m_to_utime(entry->modify_date);
		inode->i_blksize = HFS_SECTOR_SIZE;
		inode->i_uid = hsb->s_uid;
		inode->i_gid = hsb->s_gid;

		memset(HFS_I(inode), 0, sizeof(struct hfs_inode_info));
		HFS_I(inode)->magic = HFS_INO_MAGIC;
		HFS_I(inode)->entry = entry;
		HFS_I(inode)->tz_secondswest = hfs_to_utc(0);

		hsb->s_ifill(inode, type, hsb->s_version);
		if (!hsb->s_afpd && (entry->type == HFS_CDR_FIL) &&
		    (entry->u.file.flags & HFS_FIL_LOCK)) {
			inode->i_mode &= ~S_IWUGO;
		}
		inode->i_mode &= ~hsb->s_umask;

		if (!inode->i_mode) {
			iput(inode); /* does an hfs_cat_put */
			inode = NULL;
		} else
			*sys_entry = dentry; /* cache dentry */

	}

	return inode;
}

/*================ Scheme-specific functions ================*/

/* 
 * hfs_cap_ifill()
 *
 * This function serves the same purpose as a read_inode() function does
 * in other filesystems.  It is called by __hfs_iget() to fill in
 * the missing fields of an uninitialized inode under the CAP scheme.
 */
void hfs_cap_ifill(struct inode * inode, ino_t type, const int version)
{
	struct hfs_cat_entry *entry = HFS_I(inode)->entry;

	HFS_I(inode)->d_drop_op = hfs_cap_drop_dentry;
	if (type == HFS_CAP_FNDR) {
		inode->i_size = sizeof(struct hfs_cap_info);
		inode->i_blocks = 0;
		inode->i_nlink = 1;
		inode->i_mode = S_IRUGO | S_IWUGO | S_IFREG;
		inode->i_op = &hfs_cap_info_inode_operations;
		inode->i_fop = &hfs_cap_info_operations;
	} else if (entry->type == HFS_CDR_FIL) {
		init_file_inode(inode, (type == HFS_CAP_DATA) ?
						HFS_FK_DATA : HFS_FK_RSRC);
		inode->i_op = &hfs_file_inode_operations;
		inode->i_fop = &hfs_file_operations;
		inode->i_mapping->a_ops = &hfs_aops;
		inode->u.hfs_i.mmu_private = inode->i_size;
	} else { /* Directory */
		struct hfs_dir *hdir = &entry->u.dir;

		inode->i_blocks = 0;
		inode->i_size = hdir->files + hdir->dirs + 5;
		HFS_I(inode)->dir_size = 1;
		if (type == HFS_CAP_NDIR) {
			inode->i_mode = S_IRWXUGO | S_IFDIR;
			inode->i_nlink = hdir->dirs + 4;
			inode->i_op = &hfs_cap_ndir_inode_operations;
			inode->i_fop = &hfs_cap_dir_operations;
			HFS_I(inode)->file_type = HFS_CAP_NORM;
		} else if (type == HFS_CAP_FDIR) {
			inode->i_mode = S_IRUGO | S_IXUGO | S_IFDIR;
			inode->i_nlink = 2;
			inode->i_op = &hfs_cap_fdir_inode_operations;
			inode->i_fop = &hfs_cap_dir_operations;
			HFS_I(inode)->file_type = HFS_CAP_FNDR;
		} else if (type == HFS_CAP_RDIR) {
			inode->i_mode = S_IRUGO | S_IXUGO | S_IFDIR;
			inode->i_nlink = 2;
			inode->i_op = &hfs_cap_rdir_inode_operations;
			inode->i_fop = &hfs_cap_dir_operations;
			HFS_I(inode)->file_type = HFS_CAP_RSRC;
		}
	}
}

/* 
 * hfs_dbl_ifill()
 *
 * This function serves the same purpose as a read_inode() function does
 * in other filesystems.  It is called by __hfs_iget() to fill in
 * the missing fields of an uninitialized inode under the AppleDouble
 * scheme.
 */
void hfs_dbl_ifill(struct inode * inode, ino_t type, const int version)
{
	struct hfs_cat_entry *entry = HFS_I(inode)->entry;

	HFS_I(inode)->d_drop_op = hfs_dbl_drop_dentry;
	if (type == HFS_DBL_HDR) {
		if (entry->type == HFS_CDR_FIL) {
			init_file_inode(inode, HFS_FK_RSRC);
			inode->i_size += HFS_DBL_HDR_LEN;
			HFS_I(inode)->default_layout = &hfs_dbl_fil_hdr_layout;
		} else {
			inode->i_size = HFS_DBL_HDR_LEN;
			inode->i_mode = S_IRUGO | S_IWUGO | S_IFREG;
			inode->i_nlink = 1;
			HFS_I(inode)->default_layout = &hfs_dbl_dir_hdr_layout;
		}
		inode->i_op = &hfs_hdr_inode_operations;
		inode->i_fop = &hfs_hdr_operations;
	} else if (entry->type == HFS_CDR_FIL) {
		init_file_inode(inode, HFS_FK_DATA);
		inode->i_op = &hfs_file_inode_operations;
		inode->i_fop = &hfs_file_operations;
		inode->i_mapping->a_ops = &hfs_aops;
		inode->u.hfs_i.mmu_private = inode->i_size;
	} else { /* Directory */
		struct hfs_dir *hdir = &entry->u.dir;

		inode->i_blocks = 0;
		inode->i_nlink = hdir->dirs + 2;
		inode->i_size = 3 + 2 * (hdir->dirs + hdir->files);
		inode->i_mode = S_IRWXUGO | S_IFDIR;
		inode->i_op = &hfs_dbl_dir_inode_operations;
		inode->i_fop = &hfs_dbl_dir_operations;
		HFS_I(inode)->file_type = HFS_DBL_NORM;
		HFS_I(inode)->dir_size = 2;
	}
}

/* 
 * hfs_nat_ifill()
 *
 * This function serves the same purpose as a read_inode() function does
 * in other filesystems.  It is called by __hfs_iget() to fill in
 * the missing fields of an uninitialized inode under the Netatalk
 * scheme.
 */
void hfs_nat_ifill(struct inode * inode, ino_t type, const int version)
{
	struct hfs_cat_entry *entry = HFS_I(inode)->entry;

	HFS_I(inode)->d_drop_op = hfs_nat_drop_dentry;
	if (type == HFS_NAT_HDR) {
		if (entry->type == HFS_CDR_FIL) {
			init_file_inode(inode, HFS_FK_RSRC);
			inode->i_size += HFS_NAT_HDR_LEN;
		} else {
			inode->i_size = HFS_NAT_HDR_LEN;
			inode->i_mode = S_IRUGO | S_IWUGO | S_IFREG;
			inode->i_nlink = 1;
		}
		inode->i_op = &hfs_hdr_inode_operations;
		inode->i_fop = &hfs_hdr_operations;
		HFS_I(inode)->default_layout = (version == 2) ?
			&hfs_nat2_hdr_layout : &hfs_nat_hdr_layout;
	} else if (entry->type == HFS_CDR_FIL) {
		init_file_inode(inode, HFS_FK_DATA);
		inode->i_op = &hfs_file_inode_operations;
		inode->i_fop = &hfs_file_operations;
		inode->i_mapping->a_ops = &hfs_aops;
		inode->u.hfs_i.mmu_private = inode->i_size;
	} else { /* Directory */
		struct hfs_dir *hdir = &entry->u.dir;

		inode->i_blocks = 0;
		inode->i_size = hdir->files + hdir->dirs + 4;
		inode->i_mode = S_IRWXUGO | S_IFDIR;
		HFS_I(inode)->dir_size = 1;
		if (type == HFS_NAT_NDIR) {
			inode->i_nlink = hdir->dirs + 3;
			inode->i_op = &hfs_nat_ndir_inode_operations;
			HFS_I(inode)->file_type = HFS_NAT_NORM;
		} else if (type == HFS_NAT_HDIR) {
			inode->i_nlink = 2;
			inode->i_op = &hfs_nat_hdir_inode_operations;
			HFS_I(inode)->file_type = HFS_NAT_HDR;
		}
		inode->i_fop = &hfs_nat_dir_operations;
	}
}
