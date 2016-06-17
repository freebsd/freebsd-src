/*
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the inode_operations and file_operations
 * structures for HFS directories under the CAP scheme.
 *
 * Based on the minix file system code, (C) 1991, 1992 by Linus Torvalds
 *
 * The source code distribution of the Columbia AppleTalk Package for
 * UNIX, version 6.0, (CAP) was used as a specification of the
 * location and format of files used by CAP's Aufs.  No code from CAP
 * appears in hfs_fs.  hfs_fs is not a work ``derived'' from CAP in
 * the sense of intellectual property law.
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

static struct dentry *cap_lookup(struct inode *, struct dentry *);
static int cap_readdir(struct file *, void *, filldir_t);

/*================ Global variables ================*/

#define DOT_LEN			1
#define DOT_DOT_LEN		2
#define DOT_RESOURCE_LEN	9
#define DOT_FINDERINFO_LEN	11
#define DOT_ROOTINFO_LEN	9

const struct hfs_name hfs_cap_reserved1[] = {
	{DOT_LEN,		"."},
	{DOT_DOT_LEN,		".."},
	{DOT_RESOURCE_LEN,	".resource"},
	{DOT_FINDERINFO_LEN,	".finderinfo"},
	{0,			""},
};

const struct hfs_name hfs_cap_reserved2[] = {
	{DOT_ROOTINFO_LEN,	".rootinfo"},
	{0,			""},
};

#define DOT		(&hfs_cap_reserved1[0])
#define DOT_DOT		(&hfs_cap_reserved1[1])
#define DOT_RESOURCE	(&hfs_cap_reserved1[2])
#define DOT_FINDERINFO	(&hfs_cap_reserved1[3])
#define DOT_ROOTINFO	(&hfs_cap_reserved2[0])

struct file_operations hfs_cap_dir_operations = {
	read:		generic_read_dir,
	readdir:	cap_readdir,
	fsync:		file_fsync,
};

struct inode_operations hfs_cap_ndir_inode_operations = {
	create:		hfs_create,
	lookup:		cap_lookup,
	unlink:		hfs_unlink,
	mkdir:		hfs_mkdir,
	rmdir:		hfs_rmdir,
	rename:		hfs_rename,
	setattr:	hfs_notify_change,
};

struct inode_operations hfs_cap_fdir_inode_operations = {
	lookup:		cap_lookup,
	setattr:	hfs_notify_change,
};

struct inode_operations hfs_cap_rdir_inode_operations = {
	create:		hfs_create,
	lookup:		cap_lookup,
	setattr:	hfs_notify_change,
};

/*================ File-local functions ================*/

/*
 * cap_lookup()
 *
 * This is the lookup() entry in the inode_operations structure for
 * HFS directories in the CAP scheme.  The purpose is to generate the
 * inode corresponding to an entry in a directory, given the inode for
 * the directory and the name (and its length) of the entry.
 */
static struct dentry *cap_lookup(struct inode * dir, struct dentry *dentry)
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
	hfs_nameout(dir, &cname, dentry->d_name.name, 
		    dentry->d_name.len);

	/* no need to check for "."  or ".." */

	/* Check for special directories if in a normal directory.
	   Note that cap_dupdir() does an iput(dir). */
	if (dtype==HFS_CAP_NDIR) {
		/* Check for ".resource", ".finderinfo" and ".rootinfo" */
		if (hfs_streq(cname.Name, cname.Len, 
			      DOT_RESOURCE->Name, DOT_RESOURCE_LEN)) {
			++entry->count; /* __hfs_iget() eats one */
			inode = hfs_iget(entry, HFS_CAP_RDIR, dentry);
			goto done;
		} else if (hfs_streq(cname.Name, cname.Len, 
				     DOT_FINDERINFO->Name, 
				     DOT_FINDERINFO_LEN)) {
			++entry->count; /* __hfs_iget() eats one */
			inode = hfs_iget(entry, HFS_CAP_FDIR, dentry);
			goto done;
		} else if ((entry->cnid == htonl(HFS_ROOT_CNID)) &&
			   hfs_streq(cname.Name, cname.Len, 
				     DOT_ROOTINFO->Name, DOT_ROOTINFO_LEN)) {
			++entry->count; /* __hfs_iget() eats one */
			inode = hfs_iget(entry, HFS_CAP_FNDR, dentry);
			goto done;
		}
	}

	/* Do an hfs_iget() on the mangled name. */
	hfs_cat_build_key(entry->cnid, &cname, &key);
	inode = hfs_iget(hfs_cat_get(entry->mdb, &key),
			 HFS_I(dir)->file_type, dentry);

	/* Don't return a resource fork for a directory */
	if (inode && (dtype == HFS_CAP_RDIR) && 
	    (HFS_I(inode)->entry->type == HFS_CDR_DIR)) {
	        iput(inode); /* this does an hfs_cat_put */
		inode = NULL;
	}

done:
	d_add(dentry, inode);
	return NULL;
}

/*
 * cap_readdir()
 *
 * This is the readdir() entry in the file_operations structure for
 * HFS directories in the CAP scheme.  The purpose is to enumerate the
 * entries in a directory, given the inode of the directory and a
 * (struct file *), the 'f_pos' field of which indicates the location
 * in the directory.  The (struct file *) is updated so that the next
 * call with the same 'dir' and 'filp' arguments will produce the next
 * directory entry.  The entries are returned in 'dirent', which is
 * "filled-in" by calling filldir().  This allows the same readdir()
 * function be used for different dirent formats.  We try to read in
 * as many entries as we can before filldir() refuses to take any more.
 *
 * XXX: In the future it may be a good idea to consider not generating
 * metadata files for covered directories since the data doesn't
 * correspond to the mounted directory.	 However this requires an
 * iget() for every directory which could be considered an excessive
 * amount of overhead.	Since the inode for a mount point is always
 * in-core this is another argument for a call to get an inode if it
 * is in-core or NULL if it is not.
 */
static int cap_readdir(struct file * filp,
		       void * dirent, filldir_t filldir)
{
	ino_t type;
	int skip_dirs;
	struct hfs_brec brec;
        struct hfs_cat_entry *entry;
	struct inode *dir = filp->f_dentry->d_inode;

	entry = HFS_I(dir)->entry;
	type = HFS_ITYPE(dir->i_ino);
	skip_dirs = (type == HFS_CAP_RDIR);

	if (filp->f_pos == 0) {
		/* Entry 0 is for "." */
		if (filldir(dirent, DOT->Name, DOT_LEN, 0, dir->i_ino, DT_DIR)) {
			return 0;
		}
		filp->f_pos = 1;
	}

	if (filp->f_pos == 1) {
		/* Entry 1 is for ".." */
		hfs_u32 cnid;

		if (type == HFS_CAP_NDIR) {
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

	if (filp->f_pos < (dir->i_size - 3)) {
		hfs_u32 cnid;
		hfs_u8 type;

	    	if (hfs_cat_open(entry, &brec) ||
	    	    hfs_cat_next(entry, &brec, filp->f_pos - 2, &cnid, &type)) {
			return 0;
		}
		while (filp->f_pos < (dir->i_size - 3)) {
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

	if (filp->f_pos == (dir->i_size - 3)) {
		if ((entry->cnid == htonl(HFS_ROOT_CNID)) &&
		    (type == HFS_CAP_NDIR)) {
			/* In root dir last-2 entry is for ".rootinfo" */
			if (filldir(dirent, DOT_ROOTINFO->Name,
				    DOT_ROOTINFO_LEN, filp->f_pos,
				    ntohl(entry->cnid) | HFS_CAP_FNDR,
				    DT_UNKNOWN)) {
				return 0;
			}
		}
		++filp->f_pos;
	}

	if (filp->f_pos == (dir->i_size - 2)) {
		if (type == HFS_CAP_NDIR) {
			/* In normal dirs last-1 entry is for ".finderinfo" */
			if (filldir(dirent, DOT_FINDERINFO->Name,
				    DOT_FINDERINFO_LEN, filp->f_pos,
				    ntohl(entry->cnid) | HFS_CAP_FDIR,
				    DT_UNKNOWN)) {
				return 0;
			}
		}
		++filp->f_pos;
	}

	if (filp->f_pos == (dir->i_size - 1)) {
		if (type == HFS_CAP_NDIR) {
			/* In normal dirs last entry is for ".resource" */
			if (filldir(dirent, DOT_RESOURCE->Name,
				    DOT_RESOURCE_LEN, filp->f_pos,
				    ntohl(entry->cnid) | HFS_CAP_RDIR,
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
void hfs_cap_drop_dentry(struct dentry *dentry, const ino_t type)
{
  if (type == HFS_CAP_DATA) { /* given name */
    hfs_drop_special(dentry->d_parent, DOT_FINDERINFO, dentry);
    hfs_drop_special(dentry->d_parent, DOT_RESOURCE, dentry);
  } else {
    struct dentry *de;

    /* given {.resource,.finderinfo}/name, look for name */
    if ((de = hfs_lookup_dentry(dentry->d_parent->d_parent,
				dentry->d_name.name, dentry->d_name.len))) {
      if (!de->d_inode)
	d_drop(de);
      dput(de);
    }
    
    switch (type) {
    case HFS_CAP_RSRC: /* given .resource/name */
       /* look for .finderinfo/name */
      hfs_drop_special(dentry->d_parent->d_parent, DOT_FINDERINFO, 
		       dentry);
      break;
    case HFS_CAP_FNDR: /* given .finderinfo/name. i don't this 
			* happens. */
      /* look for .resource/name */
      hfs_drop_special(dentry->d_parent->d_parent, DOT_RESOURCE, 
		       dentry);
      break;
    }
  }
}
