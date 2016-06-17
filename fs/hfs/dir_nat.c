/*
 * linux/fs/hfs/dir_nat.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the inode_operations and file_operations
 * structures for HFS directories.
 *
 * Based on the minix file system code, (C) 1991, 1992 by Linus Torvalds
 *
 * The source code distributions of Netatalk, versions 1.3.3b2 and
 * 1.4b2, were used as a specification of the location and format of
 * files used by Netatalk's afpd.  No code from Netatalk appears in
 * hfs_fs.  hfs_fs is not a work ``derived'' from Netatalk in the
 * sense of intellectual property law.
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

/*================ Forward declarations ================*/

static struct dentry *nat_lookup(struct inode *, struct dentry *);
static int nat_readdir(struct file *, void *, filldir_t);
static int nat_rmdir(struct inode *, struct dentry *);
static int nat_hdr_unlink(struct inode *, struct dentry *);
static int nat_hdr_rename(struct inode *, struct dentry *,
			  struct inode *, struct dentry *);

/*================ Global variables ================*/

#define DOT_LEN			1
#define DOT_DOT_LEN		2
#define DOT_APPLEDOUBLE_LEN	12
#define DOT_PARENT_LEN		7
#define ROOTINFO_LEN            8

const struct hfs_name hfs_nat_reserved1[] = {
	{DOT_LEN,		"."},
	{DOT_DOT_LEN,		".."},
	{DOT_APPLEDOUBLE_LEN,	".AppleDouble"},
	{DOT_PARENT_LEN,	".Parent"},
	{0,			""},
};

const struct hfs_name hfs_nat_reserved2[] = {
	{ROOTINFO_LEN,			"RootInfo"},
};

#define DOT		(&hfs_nat_reserved1[0])
#define DOT_DOT		(&hfs_nat_reserved1[1])
#define DOT_APPLEDOUBLE	(&hfs_nat_reserved1[2])
#define DOT_PARENT	(&hfs_nat_reserved1[3])
#define ROOTINFO        (&hfs_nat_reserved2[0])

struct file_operations hfs_nat_dir_operations = {
	read:		generic_read_dir,
	readdir:	nat_readdir,
	fsync:		file_fsync,
};

struct inode_operations hfs_nat_ndir_inode_operations = {
	create:		hfs_create,
	lookup:		nat_lookup,
	unlink:		hfs_unlink,
	mkdir:		hfs_mkdir,
	rmdir:		nat_rmdir,
	rename:		hfs_rename,
	setattr:	hfs_notify_change,
};

struct inode_operations hfs_nat_hdir_inode_operations = {
	create:		hfs_create,
	lookup:		nat_lookup,
	unlink:		nat_hdr_unlink,
	rename:		nat_hdr_rename,
	setattr:	hfs_notify_change,
};

/*================ File-local functions ================*/

/*
 * nat_lookup()
 *
 * This is the lookup() entry in the inode_operations structure for
 * HFS directories in the Netatalk scheme.  The purpose is to generate
 * the inode corresponding to an entry in a directory, given the inode
 * for the directory and the name (and its length) of the entry.
 */
static struct dentry *nat_lookup(struct inode * dir, struct dentry *dentry)
{
	ino_t dtype;
	struct hfs_name cname;
	struct hfs_cat_entry *entry;
	struct hfs_cat_key key;
	struct inode *inode = NULL;

	dentry->d_op = &hfs_dentry_operations;
	entry = HFS_I(dir)->entry;
	dtype = HFS_ITYPE(dir->i_ino);

	/* Perform name-mangling */
	hfs_nameout(dir, &cname, dentry->d_name.name, dentry->d_name.len);

	/* no need to check for "."  or ".." */

	/* Check for ".AppleDouble" if in a normal directory,
	   and for ".Parent" in ".AppleDouble". */
	if (dtype==HFS_NAT_NDIR) {
		/* Check for ".AppleDouble" */
		if (hfs_streq(cname.Name, cname.Len, 
			      DOT_APPLEDOUBLE->Name, DOT_APPLEDOUBLE_LEN)) {
			++entry->count; /* __hfs_iget() eats one */
			inode = hfs_iget(entry, HFS_NAT_HDIR, dentry);
			goto done;
		}
	} else if (dtype==HFS_NAT_HDIR) {
		if (hfs_streq(cname.Name, cname.Len, 
			      DOT_PARENT->Name, DOT_PARENT_LEN)) {
			++entry->count; /* __hfs_iget() eats one */
			inode = hfs_iget(entry, HFS_NAT_HDR, dentry);
			goto done;
		}

		if ((entry->cnid == htonl(HFS_ROOT_CNID)) &&
		    hfs_streq(cname.Name, cname.Len, 
			      ROOTINFO->Name, ROOTINFO_LEN)) {
			++entry->count; /* __hfs_iget() eats one */
			inode = hfs_iget(entry, HFS_NAT_HDR, dentry);
                        goto done;
		}
	}

	/* Do an hfs_iget() on the mangled name. */
	hfs_cat_build_key(entry->cnid, &cname, &key);
	inode = hfs_iget(hfs_cat_get(entry->mdb, &key), 
			 HFS_I(dir)->file_type, dentry);

	/* Don't return a header file for a directory other than .Parent */
	if (inode && (dtype == HFS_NAT_HDIR) &&
	    (HFS_I(inode)->entry != entry) &&
	    (HFS_I(inode)->entry->type == HFS_CDR_DIR)) {
	        iput(inode); /* this does an hfs_cat_put */
		inode = NULL;
	}

done:
	d_add(dentry, inode);
	return NULL;
}

/*
 * nat_readdir()
 *
 * This is the readdir() entry in the file_operations structure for
 * HFS directories in the netatalk scheme.  The purpose is to
 * enumerate the entries in a directory, given the inode of the
 * directory and a struct file which indicates the location in the
 * directory.  The struct file is updated so that the next call with
 * the same dir and filp will produce the next directory entry.	 The
 * entries are returned in dirent, which is "filled-in" by calling
 * filldir().  This allows the same readdir() function be used for
 * different dirent formats.  We try to read in as many entries as we
 * can before filldir() refuses to take any more.
 *
 * Note that the Netatalk format doesn't have the problem with
 * metadata for covered directories that exists in the other formats,
 * since the metadata is contained within the directory.
 */
static int nat_readdir(struct file * filp,
		       void * dirent, filldir_t filldir)
{
	ino_t type;
	int skip_dirs;
	struct hfs_brec brec;
        struct hfs_cat_entry *entry;
	struct inode *dir = filp->f_dentry->d_inode;

	entry = HFS_I(dir)->entry;
	type = HFS_ITYPE(dir->i_ino);
	skip_dirs = (type == HFS_NAT_HDIR);

	if (filp->f_pos == 0) {
		/* Entry 0 is for "." */
		if (filldir(dirent, DOT->Name, DOT_LEN, 0, dir->i_ino,
			    DT_DIR)) {
			return 0;
		}
		filp->f_pos = 1;
	}

	if (filp->f_pos == 1) {
		/* Entry 1 is for ".." */
		hfs_u32 cnid;

		if (type == HFS_NAT_NDIR) {
			cnid = hfs_get_nl(entry->key.ParID);
		} else {
			cnid = entry->cnid;
		}

		if (filldir(dirent, DOT_DOT->Name,
			    DOT_DOT_LEN, 1, ntohl(cnid), DT_DIR)) {
			return 0;
		}
		filp->f_pos = 2;
	}

	if (filp->f_pos < (dir->i_size - 2)) {
		hfs_u32 cnid;
		hfs_u8 type;

	    	if (hfs_cat_open(entry, &brec) ||
		    hfs_cat_next(entry, &brec, filp->f_pos - 2, &cnid, &type)) {
			return 0;
		}
		while (filp->f_pos < (dir->i_size - 2)) {
			if (hfs_cat_next(entry, &brec, 1, &cnid, &type)) {
				return 0;
			}
			if (!skip_dirs || (type != HFS_CDR_DIR)) {
				ino_t ino;
				unsigned int len;
				unsigned char tmp_name[HFS_NAMEMAX];

				ino = ntohl(cnid) | HFS_I(dir)->file_type;
				len = hfs_namein(dir, tmp_name,
				    &((struct hfs_cat_key *)brec.key)->CName);
				if (filldir(dirent, tmp_name, len,
					    filp->f_pos, ino, DT_UNKNOWN)) {
					hfs_cat_close(entry, &brec);
					return 0;
				}
			}
			++filp->f_pos;
		}
		hfs_cat_close(entry, &brec);
	}

	if (filp->f_pos == (dir->i_size - 2)) {
		if (type == HFS_NAT_NDIR) {
			/* In normal dirs entry 2 is for ".AppleDouble" */
			if (filldir(dirent, DOT_APPLEDOUBLE->Name,
				    DOT_APPLEDOUBLE_LEN, filp->f_pos,
				    ntohl(entry->cnid) | HFS_NAT_HDIR,
				    DT_UNKNOWN)) {
				return 0;
			}
		} else if (type == HFS_NAT_HDIR) {
			/* In .AppleDouble entry 2 is for ".Parent" */
			if (filldir(dirent, DOT_PARENT->Name,
				    DOT_PARENT_LEN, filp->f_pos,
				    ntohl(entry->cnid) | HFS_NAT_HDR,
				    DT_UNKNOWN)) {
				return 0;
			}
		}
		++filp->f_pos;
	}

	if (filp->f_pos == (dir->i_size - 1)) {
		/* handle ROOT/.AppleDouble/RootInfo as the last entry. */
		if ((entry->cnid == htonl(HFS_ROOT_CNID)) &&
		    (type == HFS_NAT_HDIR)) {
			if (filldir(dirent, ROOTINFO->Name,
				    ROOTINFO_LEN, filp->f_pos,
				    ntohl(entry->cnid) | HFS_NAT_HDR,
				    DT_UNKNOWN)) {
				return 0;
			}
		}
		++filp->f_pos;
	}

	return 0;
}

/* due to the dcache caching negative dentries for non-existent files,
 * we need to drop those entries when a file silently gets created.
 * as far as i can tell, the calls that need to do this are the file
 * related calls (create, rename, and mknod). the directory calls
 * should be immune. the relevant calls in dir.c call drop_dentry 
 * upon successful completion. */
void hfs_nat_drop_dentry(struct dentry *dentry, const ino_t type)
{
  struct dentry *de;
  
  switch (type) {
  case HFS_NAT_HDR: /* given .AppleDouble/name */
    /* look for name */
    de = hfs_lookup_dentry(dentry->d_parent->d_parent,
			   dentry->d_name.name, dentry->d_name.len);

    if (de) {
      if (!de->d_inode)
	d_drop(de);
      dput(de);
    }
    break;
  case HFS_NAT_DATA: /* given name */
    /* look for .AppleDouble/name */
    hfs_drop_special(dentry->d_parent, DOT_APPLEDOUBLE, dentry);
    break;
  }

}

/*
 * nat_rmdir()
 *
 * This is the rmdir() entry in the inode_operations structure for
 * Netatalk directories.  The purpose is to delete an existing
 * directory, given the inode for the parent directory and the name
 * (and its length) of the existing directory.
 *
 * We handle .AppleDouble and call hfs_rmdir() for all other cases.
 */
static int nat_rmdir(struct inode *parent, struct dentry *dentry)
{
	struct hfs_cat_entry *entry = HFS_I(parent)->entry;
	struct hfs_name cname;
	int error;

	hfs_nameout(parent, &cname, dentry->d_name.name, dentry->d_name.len);
	if (hfs_streq(cname.Name, cname.Len,
		      DOT_APPLEDOUBLE->Name, DOT_APPLEDOUBLE_LEN)) {
		if (!HFS_SB(parent->i_sb)->s_afpd) {
			/* Not in AFPD compatibility mode */
			error = -EPERM;
		} else if (entry->u.dir.files || entry->u.dir.dirs) {
			/* AFPD compatible, but the directory is not empty */
			error = -ENOTEMPTY;
		} else {
			/* AFPD compatible, so pretend to succeed */
			error = 0;
		}
	} else {
		error = hfs_rmdir(parent, dentry);
	}
	return error;
}

/*
 * nat_hdr_unlink()
 *
 * This is the unlink() entry in the inode_operations structure for
 * Netatalk .AppleDouble directories.  The purpose is to delete an
 * existing file, given the inode for the parent directory and the name
 * (and its length) of the existing file.
 *
 * WE DON'T ACTUALLY DELETE HEADER THE FILE.
 * In non-afpd-compatible mode:
 *   We return -EPERM.
 * In afpd-compatible mode:
 *   We return success if the file exists or is .Parent.
 *   Otherwise we return -ENOENT.
 */
static int nat_hdr_unlink(struct inode *dir, struct dentry *dentry)
{
	struct hfs_cat_entry *entry = HFS_I(dir)->entry;
	int error = 0;

	if (!HFS_SB(dir->i_sb)->s_afpd) {
		/* Not in AFPD compatibility mode */
		error = -EPERM;
	} else {
		struct hfs_name cname;

		hfs_nameout(dir, &cname, dentry->d_name.name, 
			    dentry->d_name.len);
		if (!hfs_streq(cname.Name, cname.Len,
			       DOT_PARENT->Name, DOT_PARENT_LEN)) {
			struct hfs_cat_entry *victim;
			struct hfs_cat_key key;

			hfs_cat_build_key(entry->cnid, &cname, &key);
			victim = hfs_cat_get(entry->mdb, &key);

			if (victim) {
				/* pretend to succeed */
				hfs_cat_put(victim);
			} else {
				error = -ENOENT;
			}
		}
	}
	return error;
}

/*
 * nat_hdr_rename()
 *
 * This is the rename() entry in the inode_operations structure for
 * Netatalk header directories.  The purpose is to rename an existing
 * file given the inode for the current directory and the name 
 * (and its length) of the existing file and the inode for the new
 * directory and the name (and its length) of the new file/directory.
 *
 * WE NEVER MOVE ANYTHING.
 * In non-afpd-compatible mode:
 *   We return -EPERM.
 * In afpd-compatible mode:
 *   If the source header doesn't exist, we return -ENOENT.
 *   If the destination is not a header directory we return -EPERM.
 *   We return success if the destination is also a header directory
 *    and the header exists or is ".Parent".
 */
static int nat_hdr_rename(struct inode *old_dir, struct dentry *old_dentry,
			  struct inode *new_dir, struct dentry *new_dentry)
{
	struct hfs_cat_entry *entry = HFS_I(old_dir)->entry;
	int error = 0;

	if (!HFS_SB(old_dir->i_sb)->s_afpd) {
		/* Not in AFPD compatibility mode */
		error = -EPERM;
	} else {
		struct hfs_name cname;

		hfs_nameout(old_dir, &cname, old_dentry->d_name.name,
			    old_dentry->d_name.len);
		if (!hfs_streq(cname.Name, cname.Len, 
			       DOT_PARENT->Name, DOT_PARENT_LEN)) {
			struct hfs_cat_entry *victim;
			struct hfs_cat_key key;

			hfs_cat_build_key(entry->cnid, &cname, &key);
			victim = hfs_cat_get(entry->mdb, &key);

			if (victim) {
				/* pretend to succeed */
				hfs_cat_put(victim);
			} else {
				error = -ENOENT;
			}
		}

		if (!error && (HFS_ITYPE(new_dir->i_ino) != HFS_NAT_HDIR)) {
			error = -EPERM;
		}
	}
	return error;
}
