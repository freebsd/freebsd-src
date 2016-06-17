/*
 * linux/fs/befs/linuxvfs.c
 *
 * Copyright (C) 2001 Will Dyson <will_dyson@pobox.com
 *
 */

#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/nls.h>

#include "befs.h"
#include "btree.h"
#include "inode.h"
#include "datastream.h"
#include "super.h"
#include "io.h"
#include "endian.h"

EXPORT_NO_SYMBOLS;
MODULE_DESCRIPTION("BeOS File System (BeFS) driver");
MODULE_AUTHOR("Will Dyson");
MODULE_LICENSE("GPL");

/* The units the vfs expects inode->i_blocks to be in */
#define VFS_BLOCK_SIZE 512

static int befs_readdir(struct file *, void *, filldir_t);
static int befs_get_block(struct inode *, long, struct buffer_head *, int);
static int befs_readpage(struct file *file, struct page *page);
static int befs_bmap(struct address_space *mapping, long block);
static struct dentry *befs_lookup(struct inode *, struct dentry *);
static void befs_read_inode(struct inode *ino);
static void befs_clear_inode(struct inode *ino);
static int befs_init_inodecache(void);
static void befs_destroy_inodecache(void);

static int befs_readlink(struct dentry *, char *, int);
static int befs_follow_link(struct dentry *, struct nameidata *nd);

static int befs_utf2nls(struct super_block *sb, const char *in, int in_len,
			char **out, int *out_len);
static int befs_nls2utf(struct super_block *sb, const char *in, int in_len,
			char **out, int *out_len);

static void befs_put_super(struct super_block *);
static struct super_block *befs_read_super(struct super_block *, void *, int);
static int befs_remount(struct super_block *, int *, char *);
static int befs_statfs(struct super_block *, struct statfs *);
static int parse_options(char *, befs_mount_options *);

static ssize_t befs_listxattr(struct dentry *dentry, char *buffer, size_t size);
static ssize_t befs_getxattr(struct dentry *dentry, const char *name,
			     void *buffer, size_t size);
static int befs_setxattr(struct dentry *dentry, const char *name, void *value,
			 size_t size, int flags);
static int befs_removexattr(struct dentry *dentry, const char *name);

/* slab cache for befs_inode_info objects */
static kmem_cache_t *befs_inode_cachep;

static const struct super_operations befs_sops = {
	read_inode:befs_read_inode,	/* initialize & read inode */
	clear_inode:befs_clear_inode,	/* uninit inode */
	put_super:befs_put_super,	/* uninit super */
	statfs:befs_statfs,	/* statfs */
	remount_fs:befs_remount,
};

struct file_operations befs_dir_operations = {
	read:generic_read_dir,
	readdir:befs_readdir,
};

struct inode_operations befs_dir_inode_operations = {
	lookup:befs_lookup,
};

struct file_operations befs_file_operations = {
	llseek:default_llseek,
	read:generic_file_read,
	mmap:generic_file_mmap,
};

struct inode_operations befs_file_inode_operations = {
};

struct address_space_operations befs_aops = {
	readpage:befs_readpage,
	sync_page:block_sync_page,
	bmap:befs_bmap,
};

static struct inode_operations befs_symlink_inode_operations = {
	readlink:befs_readlink,
	follow_link:befs_follow_link,
};

/* 
 * Called by generic_file_read() to read a page of data
 * 
 * In turn, simply calls a generic block read function and
 * passes it the address of befs_get_block, for mapping file
 * positions to disk blocks.
 */
static int
befs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, befs_get_block);
}

static int
befs_bmap(struct address_space *mapping, long block)
{
	return generic_block_bmap(mapping, block, befs_get_block);
}

/* 
 * Generic function to map a file position (block) to a 
 * disk offset (passed back in bh_result).
 *
 * Used by many higher level functions.
 *
 * Calls befs_fblock2brun() in datastream.c to do the real work.
 *
 * -WD 10-26-01
 */

static int
befs_get_block(struct inode *inode, long block,
	       struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	befs_data_stream *ds = &BEFS_I(inode)->i_data.ds;
	befs_block_run run = BAD_IADDR;
	int res = 0;
	ulong disk_off;

	befs_debug(sb, "---> befs_get_block() for inode %lu, block %ld",
		   inode->i_ino, block);

	if (block < 0) {
		befs_error(sb, "befs_get_block() was asked for a block "
			   "number less than zero: block %ld in inode %lu",
			   block, inode->i_ino);
		return -EIO;
	}

	if (create) {
		befs_error(sb, "befs_get_block() was asked to write to "
			   "block %ld in inode %lu", block, inode->i_ino);
		return -EPERM;
	}

	res = befs_fblock2brun(sb, ds, block, &run);
	if (res != BEFS_OK) {
		befs_error(sb,
			   "<--- befs_get_block() for inode %lu, block "
			   "%ld ERROR", inode->i_ino, block);
		return -EFBIG;
	}

	disk_off = (ulong) iaddr2blockno(sb, &run);

	bh_result->b_dev = inode->i_dev;
	bh_result->b_blocknr = disk_off;
	bh_result->b_state |= (1UL << BH_Mapped);

	befs_debug(sb, "<--- befs_get_block() for inode %lu, block %ld, "
		   "disk address %lu", inode->i_ino, block, disk_off);

	return 0;
}

static struct dentry *
befs_lookup(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = NULL;
	struct super_block *sb = dir->i_sb;
	befs_data_stream *ds = &BEFS_I(dir)->i_data.ds;
	befs_off_t offset;
	int ret;
	int utfnamelen;
	char *utfname;
	const char *name = dentry->d_name.name;

	befs_debug(sb, "---> befs_lookup() "
		   "name %s inode %ld", dentry->d_name.name, dir->i_ino);

	/* Convert to UTF-8 */
	if (BEFS_SB(sb)->nls) {
		ret =
		    befs_nls2utf(sb, name, strlen(name), &utfname, &utfnamelen);
		if (ret < 0) {
			befs_debug(sb, "<--- befs_lookup() ERROR");
			return ERR_PTR(ret);
		}
		ret = befs_btree_find(sb, ds, utfname, &offset);
		kfree(utfname);

	} else {
		ret = befs_btree_find(sb, ds, dentry->d_name.name, &offset);
	}

	if (ret == BEFS_BT_NOT_FOUND) {
		befs_debug(sb, "<--- befs_lookup() %s not found",
			   dentry->d_name.name);
		return ERR_PTR(-ENOENT);

	} else if (ret != BEFS_OK || offset == 0) {
		befs_warning(sb, "<--- befs_lookup() Error");
		return ERR_PTR(-ENODATA);
	}

	inode = iget(dir->i_sb, (ino_t) offset);
	if (!inode)
		return ERR_PTR(-EACCES);

	d_add(dentry, inode);

	befs_debug(sb, "<--- befs_lookup()");

	return NULL;
}

static int
befs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	struct super_block *sb = inode->i_sb;
	befs_data_stream *ds = &BEFS_I(inode)->i_data.ds;
	befs_off_t value;
	int result;
	size_t keysize;
	unsigned char d_type;
	char keybuf[BEFS_NAME_LEN + 1];
	char *nlsname;
	int nlsnamelen;
	const char *dirname = filp->f_dentry->d_name.name;

	befs_debug(sb, "---> befs_readdir() "
		   "name %s, inode %ld, filp->f_pos %Ld",
		   dirname, inode->i_ino, filp->f_pos);

	result = befs_btree_read(sb, ds, filp->f_pos, BEFS_NAME_LEN + 1,
				 keybuf, &keysize, &value);

	if (result == BEFS_ERR) {
		befs_debug(sb, "<--- befs_readdir() ERROR");
		befs_error(sb, "IO error reading %s (inode %lu)",
			   dirname, inode->i_ino);
		return -EIO;

	} else if (result == BEFS_BT_END) {
		befs_debug(sb, "<--- befs_readdir() END");
		return 0;

	} else if (result == BEFS_BT_EMPTY) {
		befs_debug(sb, "<--- befs_readdir() Empty directory");
		return 0;
	}

	d_type = DT_UNKNOWN;

	/* Convert to NLS */
	if (BEFS_SB(sb)->nls) {
		result =
		    befs_utf2nls(sb, keybuf, keysize, &nlsname, &nlsnamelen);
		if (result < 0) {
			befs_debug(sb, "<--- befs_readdir() ERROR");
			return result;
		}
		result = filldir(dirent, nlsname, nlsnamelen, filp->f_pos,
				 (ino_t) value, d_type);
		kfree(nlsname);

	} else {
		result = filldir(dirent, keybuf, keysize, filp->f_pos,
				 (ino_t) value, d_type);
	}

	filp->f_pos++;

	befs_debug(sb, "<--- befs_readdir() filp->f_pos %Ld", filp->f_pos);

	return 0;
}

static void
befs_clear_inode(struct inode *inode)
{
	befs_inode_info *b_ino = BEFS_I(inode);
	inode->u.generic_ip = NULL;

	if (b_ino) {
		kmem_cache_free(befs_inode_cachep, b_ino);
	}
	return;
}

static void
befs_read_inode(struct inode *inode)
{
	struct buffer_head *bh = NULL;
	befs_inode *raw_inode = NULL;

	struct super_block *sb = inode->i_sb;
	befs_sb_info *befs_sb = BEFS_SB(sb);
	befs_inode_info *befs_ino = NULL;

	befs_debug(sb, "---> befs_read_inode() " "inode = %lu", inode->i_ino);

	inode->u.generic_ip = kmem_cache_alloc(befs_inode_cachep, GFP_NOFS);
	if (inode->u.generic_ip == NULL) {
		befs_error(sb, "Unable to allocate memory for private "
			   "portion of inode %lu.", inode->i_ino);
		goto unaquire_none;
	}
	befs_ino = BEFS_I(inode);

	/* convert from vfs's inode number to befs's inode number */
	befs_ino->i_inode_num = blockno2iaddr(sb, inode->i_ino);

	befs_debug(sb, "  real inode number [%u, %hu, %hu]",
		   befs_ino->i_inode_num.allocation_group,
		   befs_ino->i_inode_num.start, befs_ino->i_inode_num.len);

	bh = befs_bread_iaddr(sb, befs_ino->i_inode_num);
	if (!bh) {
		befs_error(sb, "unable to read inode block - "
			   "inode = %lu", inode->i_ino);
		goto unaquire_ino_info;
	}

	raw_inode = (befs_inode *) bh->b_data;

	befs_dump_inode(sb, raw_inode);

	if (befs_check_inode(sb, raw_inode, inode->i_ino) != BEFS_OK) {
		befs_error(sb, "Bad inode: %lu", inode->i_ino);
		goto unaquire_bh;
	}

	inode->i_mode = (umode_t) fs32_to_cpu(sb, raw_inode->mode);

	/*
	 * set uid and gid.  But since current BeOS is single user OS, so
	 * you can change by "uid" or "gid" options.
	 */

	inode->i_uid = befs_sb->mount_opts.use_uid ?
	    befs_sb->mount_opts.uid : (uid_t) fs32_to_cpu(sb, raw_inode->uid);
	inode->i_gid = befs_sb->mount_opts.use_gid ?
	    befs_sb->mount_opts.gid : (gid_t) fs32_to_cpu(sb, raw_inode->gid);

	inode->i_nlink = 1;

	/*
	 * BEFS's time is 64 bits, but current VFS is 32 bits...
	 * BEFS don't have access time. Nor inode change time. VFS
	 * doesn't have creation time.
	 */

	inode->i_mtime =
	    (time_t) (fs64_to_cpu(sb, raw_inode->last_modified_time) >> 16);
	inode->i_ctime = inode->i_mtime;
	inode->i_atime = inode->i_mtime;
	inode->i_blkbits = befs_sb->block_shift;
	inode->i_blksize = befs_sb->block_size;

	befs_ino->i_inode_num = fsrun_to_cpu(sb, raw_inode->inode_num);
	befs_ino->i_parent = fsrun_to_cpu(sb, raw_inode->parent);
	befs_ino->i_attribute = fsrun_to_cpu(sb, raw_inode->attributes);
	befs_ino->i_flags = fs32_to_cpu(sb, raw_inode->flags);

	if (S_ISLNK(inode->i_mode) && !(inode->i_flags & BEFS_LONG_SYMLINK)) {
		inode->i_size = 0;
		inode->i_blocks = befs_sb->block_size / VFS_BLOCK_SIZE;
		strncpy(befs_ino->i_data.symlink, raw_inode->data.symlink,
			BEFS_SYMLINK_LEN);
	} else {
		int num_blks;

		befs_ino->i_data.ds =
		    fsds_to_cpu(sb, raw_inode->data.datastream);

		num_blks = befs_count_blocks(sb, &befs_ino->i_data.ds);
		inode->i_blocks =
		    num_blks * (befs_sb->block_size / VFS_BLOCK_SIZE);
		inode->i_size = befs_ino->i_data.ds.size;
	}

	inode->i_mapping->a_ops = &befs_aops;

	if (S_ISREG(inode->i_mode)) {
		inode->i_fop = &befs_file_operations;
		inode->i_op = &befs_file_inode_operations;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &befs_dir_inode_operations;
		inode->i_fop = &befs_dir_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &befs_symlink_inode_operations;
	} else {
		befs_error(sb, "Inode %lu is not a regular file, "
			   "directory or symlink. THAT IS WRONG! BeFS has no "
			   "on disk special files", inode->i_ino);
		goto unaquire_bh;
	}

	brelse(bh);
	befs_debug(sb, "<--- befs_read_inode()");
	return;

      unaquire_bh:
	brelse(bh);

      unaquire_ino_info:
	kmem_cache_free(befs_inode_cachep, inode->u.generic_ip);

      unaquire_none:
	make_bad_inode(inode);
	inode->u.generic_ip = NULL;
	befs_debug(sb, "<--- befs_read_inode() - Bad inode");
	return;
}

/* Initialize the inode cache. Called at fs setup.
 * 
 * Taken from NFS implementation by Al Viro.
 */
static int
befs_init_inodecache(void)
{
	befs_inode_cachep = kmem_cache_create("befs_inode_cache",
					      sizeof (struct befs_inode_info),
					      0, SLAB_HWCACHE_ALIGN,
					      NULL, NULL);
	if (befs_inode_cachep == NULL) {
		printk(KERN_ERR "befs_init_inodecache: "
		       "Couldn't initalize inode slabcache\n");
		return -ENOMEM;
	}

	return 0;
}

/* Called at fs teardown.
 * 
 * Taken from NFS implementation by Al Viro.
 */
static void
befs_destroy_inodecache(void)
{
	if (kmem_cache_destroy(befs_inode_cachep))
		printk(KERN_ERR "befs_destroy_inodecache: "
		       "not all structures were freed\n");
}

/*
 * The inode of symbolic link is different to data stream.
 * The data stream become link name. Unless the LONG_SYMLINK
 * flag is set.
 */
static int
befs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct super_block *sb = dentry->d_sb;
	befs_inode_info *befs_ino = BEFS_I(dentry->d_inode);
	char *link;
	int res;

	if (befs_ino->i_flags & BEFS_LONG_SYMLINK) {
		befs_data_stream *data = &befs_ino->i_data.ds;
		befs_off_t linklen = data->size;

		befs_debug(sb, "Follow long symlink");

		link = kmalloc(linklen, GFP_NOFS);
		if (link == NULL)
			return -ENOMEM;

		if (befs_read_lsymlink(sb, data, link, linklen) != linklen) {
			kfree(link);
			befs_error(sb, "Failed to read entire long symlink");
			return -EIO;
		}

		res = vfs_follow_link(nd, link);

		kfree(link);
	} else {
		link = befs_ino->i_data.symlink;
		res = vfs_follow_link(nd, link);
	}

	return res;
}

static int
befs_readlink(struct dentry *dentry, char *buffer, int buflen)
{
	struct super_block *sb = dentry->d_sb;
	befs_inode_info *befs_ino = BEFS_I(dentry->d_inode);
	char *link;
	int res;

	if (befs_ino->i_flags & BEFS_LONG_SYMLINK) {
		befs_data_stream *data = &befs_ino->i_data.ds;
		befs_off_t linklen = data->size;

		befs_debug(sb, "Read long symlink");

		link = kmalloc(linklen, GFP_NOFS);
		if (link == NULL)
			return -ENOMEM;

		if (befs_read_lsymlink(sb, data, link, linklen) != linklen) {
			kfree(link);
			befs_error(sb, "Failed to read entire long symlink");
			return -EIO;
		}

		res = vfs_readlink(dentry, buffer, buflen, link);

		kfree(link);
	} else {
		link = befs_ino->i_data.symlink;
		res = vfs_readlink(dentry, buffer, buflen, link);
	}

	return res;
}

/*
 * UTF-8 to NLS charset  convert routine
 *
 * Changed 8/10/01 by Will Dyson. Now use uni2char() / char2uni() rather than
 * the nls tables directly
 */

static int
befs_utf2nls(struct super_block *sb, const char *in,
	     int in_len, char **out, int *out_len)
{
	struct nls_table *nls = BEFS_SB(sb)->nls;
	int i, o;
	wchar_t uni;
	int unilen, utflen;
	char *result;
	int maxlen = in_len;	/* The utf8->nls conversion cant make more chars */

	befs_debug(sb, "---> utf2nls()");

	if (!nls) {
		befs_error(sb, "befs_utf2nls called with no NLS table loaded");
		return -EINVAL;
	}

	*out = result = kmalloc(maxlen, GFP_NOFS);
	if (!*out) {
		befs_error(sb, "befs_utf2nls() cannot allocate memory");
		*out_len = 0;
		return -ENOMEM;
	}

	for (i = o = 0; i < in_len; i += utflen, o += unilen) {

		/* convert from UTF-8 to Unicode */
		utflen = utf8_mbtowc(&uni, &in[i], in_len - i);
		if (utflen < 0) {
			goto conv_err;
		}

		/* convert from Unicode to nls */
		unilen = nls->uni2char(uni, &result[o], 1);
		if (unilen < 0) {
			goto conv_err;
		}
	}
	result[o] = '\0';

	befs_debug(sb, "<--- utf2nls()");

	return o;
	*out_len = o;

      conv_err:
	befs_error(sb, "Name using charecter set %s contains a charecter that "
		   "cannot be converted to unicode.", nls->charset);
	befs_debug(sb, "<--- utf2nls()");
	kfree(result);
	return -EILSEQ;
}

/**
 * befs_nls2utf - Convert NLS string to utf8 encodeing
 * @sb: Superblock
 * @src: Input string buffer in NLS format
 * @srclen: Length of input string in bytes
 * @dest: The output string in UTF8 format
 * @destlen: Length of the output buffer
 * 
 * Converts input string @src, which is in the format of the loaded NLS map,
 * into a utf8 string.
 * 
 * The destination string @dest is allocated by this function and the caller is
 * responsible for freeing it with kfree()
 * 
 * On return, *@destlen is the length of @dest in bytes.
 *
 * On success, the return value is the number of utf8 charecters written to
 * the ouput buffer @dest.
 *  
 * On Failure, a negative number coresponding to the error code is returned.
 */

static int
befs_nls2utf(struct super_block *sb, const char *in,
	     int in_len, char **out, int *out_len)
{
	struct nls_table *nls = BEFS_SB(sb)->nls;
	int i, o;
	wchar_t uni;
	int unilen, utflen;
	char *result;
	int maxlen = 3 * in_len;

	befs_debug(sb, "---> nls2utf()\n");

	if (!nls) {
		befs_error(sb, "befs_nls2utf called with no NLS table loaded.");
		return -EINVAL;
	}

	*out = result = kmalloc(maxlen, GFP_NOFS);
	if (!*out) {
		befs_error(sb, "befs_nls2utf() cannot allocate memory");
		*out_len = 0;
		return -ENOMEM;
	}

	for (i = o = 0; i < in_len; i += unilen, o += utflen) {

		/* convert from nls to unicode */
		unilen = nls->char2uni(&in[i], in_len - i, &uni);
		if (unilen < 0) {
			goto conv_err;
		}

		/* convert from unicode to UTF-8 */
		utflen = utf8_wctomb(&result[o], uni, 3);
		if (utflen <= 0) {
			goto conv_err;
		}
	}

	result[o] = '\0';
	*out_len = o;

	befs_debug(sb, "<--- nls2utf()");

	return i;

      conv_err:
	befs_error(sb, "Name using charecter set %s contains a charecter that "
		   "cannot be converted to unicode.", nls->charset);
	befs_debug(sb, "<--- nls2utf()");
	kfree(result);
	return -EILSEQ;
}

/****Xattr****/

static ssize_t
befs_listxattr(struct dentry *dentry, char *buffer, size_t size)
{
	printk(KERN_ERR "befs_listxattr called\n");
	return 0;
}

static ssize_t
befs_getxattr(struct dentry *dentry, const char *name,
	      void *buffer, size_t size)
{
	return 0;
}

static int
befs_setxattr(struct dentry *dentry, const char *name,
	      void *value, size_t size, int flags)
{
	return 0;
}

static int
befs_removexattr(struct dentry *dentry, const char *name)
{
	return 0;
}

/****Superblock****/

static int
parse_options(char *options, befs_mount_options * opts)
{
	char *this_char;
	char *value;
	int ret = 1;

	/* Initialize options */
	opts->uid = 0;
	opts->gid = 0;
	opts->use_uid = 0;
	opts->use_gid = 0;
	opts->iocharset = NULL;
	opts->debug = 0;

	if (!options)
		return ret;

	for (this_char = strtok(options, ","); this_char != NULL;
	     this_char = strtok(NULL, ",")) {

		if ((value = strchr(this_char, '=')) != NULL)
			*value++ = 0;

		if (!strcmp(this_char, "uid")) {
			if (!value || !*value) {
				ret = 0;
			} else {
				opts->uid = simple_strtoul(value, &value, 0);
				opts->use_uid = 1;
				if (*value) {
					printk(KERN_ERR "BEFS: Invalid uid "
					       "option: %s\n", value);
					ret = 0;
				}
			}
		} else if (!strcmp(this_char, "gid")) {
			if (!value || !*value)
				ret = 0;
			else {
				opts->gid = simple_strtoul(value, &value, 0);
				opts->use_gid = 1;
				if (*value) {
					printk(KERN_ERR
					       "BEFS: Invalid gid option: "
					       "%s\n", value);
					ret = 0;
				}
			}
		} else if (!strcmp(this_char, "iocharset") && value) {
			char *p = value;
			int len;

			while (*value && *value != ',')
				value++;
			len = value - p;
			if (len) {
				char *buffer = kmalloc(len + 1, GFP_NOFS);
				if (buffer) {
					opts->iocharset = buffer;
					memcpy(buffer, p, len);
					buffer[len] = 0;

				} else {
					printk(KERN_ERR "BEFS: "
					       "cannot allocate memory\n");
					ret = 0;
				}
			}
		} else if (!strcmp(this_char, "debug")) {
			opts->debug = 1;
		}
	}

	return ret;
}

/* This function has the responsibiltiy of getting the
 * filesystem ready for unmounting. 
 * Basicly, we free everything that we allocated in
 * befs_read_inode
 */
static void
befs_put_super(struct super_block *sb)
{
	if (BEFS_SB(sb)->mount_opts.iocharset) {
		kfree(BEFS_SB(sb)->mount_opts.iocharset);
		BEFS_SB(sb)->mount_opts.iocharset = NULL;
	}

	if (BEFS_SB(sb)->nls) {
		unload_nls(BEFS_SB(sb)->nls);
		BEFS_SB(sb)->nls = NULL;
	}

	if (sb->u.generic_sbp) {
		kfree(sb->u.generic_sbp);
		sb->u.generic_sbp = NULL;
	}
	return;
}

/* Allocate private field of the superblock, fill it.
 *
 * Finish filling the public superblock fields
 * Make the root directory
 * Load a set of NLS translations if needed.
 */
static struct super_block *
befs_read_super(struct super_block *sb, void *data, int silent)
{
	struct buffer_head *bh;
	befs_sb_info *befs_sb;
	befs_super_block *disk_sb;
	int blocksize;

	const unsigned long sb_block = 0;
	const off_t x86_sb_off = 512;

	sb->u.generic_sbp = kmalloc(sizeof (struct befs_sb_info), GFP_NOFS);
	if (sb->u.generic_sbp == NULL) {
		printk(KERN_ERR
		       "BeFS(%s): Unable to allocate memory for private "
		       "portion of superblock. Bailing.\n",
		       kdevname(sb->s_dev));
		goto unaquire_none;
	}
	befs_sb = BEFS_SB(sb);

	if (!parse_options((char *) data, &befs_sb->mount_opts)) {
		befs_error(sb, "cannot parse mount options");
		goto unaquire_priv_sbp;
	}

	befs_debug(sb, "---> befs_read_super()");

#ifndef CONFIG_BEFS_RW
	if (!(sb->s_flags & MS_RDONLY)) {
		befs_warning(sb,
			     "No write support. Marking filesystem read-only");
		sb->s_flags |= MS_RDONLY;
	}
#endif				/* CONFIG_BEFS_RW */

	/*
	 * Set dummy blocksize to read super block.
	 * Will be set to real fs blocksize later.
	 *
	 * Linux 2.4.10 and later refuse to read blocks smaller than
	 * the hardsect size for the device. But we also need to read at 
	 * least 1k to get the second 512 bytes of the volume.
	 * -WD 10-26-01
	 */
	blocksize = max_t(int, get_hardsect_size(sb->s_dev), 1024);
	set_blocksize(sb->s_dev, blocksize);

	if (!(bh = bread(sb->s_dev, sb_block, blocksize))) {
		befs_error(sb, "unable to read superblock");
		goto unaquire_priv_sbp;
	}

	/* account for offset of super block on x86 */
	disk_sb = (befs_super_block *) bh->b_data;
	if ((le32_to_cpu(disk_sb->magic1) == BEFS_SUPER_MAGIC1) ||
	    (be32_to_cpu(disk_sb->magic1) == BEFS_SUPER_MAGIC1)) {
		befs_debug(sb, "Using PPC superblock location");
	} else {
		befs_debug(sb, "Using x86 superblock location");
		disk_sb =
		    (befs_super_block *) ((void *) bh->b_data + x86_sb_off);
	}

	if (befs_load_sb(sb, disk_sb) != BEFS_OK)
		goto unaquire_bh;

	befs_dump_super_block(sb, disk_sb);

	brelse(bh);

	if (befs_check_sb(sb) != BEFS_OK)
		goto unaquire_priv_sbp;

	/*
	 * set up enough so that it can read an inode
	 * Fill in kernel superblock fields from private sb
	 */
	sb->s_magic = BEFS_SUPER_MAGIC;
	sb->s_blocksize = (ulong) befs_sb->block_size;
	sb->s_blocksize_bits = (unsigned char) befs_sb->block_shift;
	sb->s_op = (struct super_operations *) &befs_sops;
	sb->s_root =
	    d_alloc_root(iget(sb, iaddr2blockno(sb, &(befs_sb->root_dir))));
	if (!sb->s_root) {
		befs_error(sb, "get root inode failed");
		goto unaquire_priv_sbp;
	}

	/* load nls library */
	if (befs_sb->mount_opts.iocharset) {
		befs_debug(sb, "Loading nls: %s",
			   befs_sb->mount_opts.iocharset);
		befs_sb->nls = load_nls(befs_sb->mount_opts.iocharset);
		if (!befs_sb->nls) {
			befs_warning(sb, "Cannot load nls %s"
				     "loding default nls",
				     befs_sb->mount_opts.iocharset);
			befs_sb->nls = load_nls_default();
		}
	}

	/* Set real blocksize of fs */
	set_blocksize(sb->s_dev, (int) befs_sb->block_size);

	return sb;
/*****************/
      unaquire_bh:
	brelse(bh);

      unaquire_priv_sbp:
	kfree(sb->u.generic_sbp);

      unaquire_none:
	sb->s_dev = 0;
	sb->u.generic_sbp = NULL;
	return NULL;
}

static int
befs_remount(struct super_block *sb, int *flags, char *data)
{
	if (!(*flags & MS_RDONLY))
		return -EINVAL;
	return 0;
}

static int
befs_statfs(struct super_block *sb, struct statfs *buf)
{

	befs_debug(sb, "---> befs_statfs()");

	buf->f_type = BEFS_SUPER_MAGIC;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = BEFS_SB(sb)->num_blocks;
	buf->f_bfree = BEFS_SB(sb)->num_blocks - BEFS_SB(sb)->used_blocks;
	buf->f_bavail = buf->f_bfree;
	buf->f_files = 0;	/* UNKNOWN */
	buf->f_ffree = 0;	/* UNKNOWN */
	buf->f_namelen = BEFS_NAME_LEN;

	befs_debug(sb, "<--- befs_statfs()");

	return 0;
}

/*
	Makes a variable of type file_system_type, 
	named befs_fs_tipe, identified by the "befs" string,
	and containing a reference to the befs_read_super function
	
	Macro declared in <linux/fs.h>
*/
static DECLARE_FSTYPE_DEV(befs_fs_type, "befs", befs_read_super);

static int __init
init_befs_fs(void)
{
	int err;

	printk(KERN_INFO "BeFS version: %s\n", BEFS_VERSION);

	err = befs_init_inodecache();
	if (err)
		return err;

	return register_filesystem(&befs_fs_type);
}

static void __exit
exit_befs_fs(void)
{
	befs_destroy_inodecache();

	unregister_filesystem(&befs_fs_type);
}

/*
Macros that typecheck the init and exit functions,
ensures that they are called at init and cleanup,
and eliminates warnings about unused functions.
*/
module_init(init_befs_fs)
    module_exit(exit_befs_fs)
