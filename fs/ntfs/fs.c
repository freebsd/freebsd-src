/*
 * fs.c - NTFS driver for Linux 2.4.x
 *
 * Legato Systems, Inc. (http://www.legato.com) have sponsored Anton
 * Altaparmakov to develop NTFS on Linux since June 2001.
 *
 * Copyright (C) 1995-1997, 1999 Martin von Löwis
 * Copyright (C) 1996 Richard Russon
 * Copyright (C) 1996-1997 Régis Duchesne
 * Copyright (C) 2000-2001, Anton Altaparmakov (AIA)
 */

#include <linux/config.h>
#include <linux/errno.h>
#include "ntfstypes.h"
#include "struct.h"
#include "util.h"
#include "inode.h"
#include "super.h"
#include "dir.h"
#include "support.h"
#include "macros.h"
#include "sysctl.h"
#include "attr.h"
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/locks.h>
#include <linux/init.h>
#include <linux/smp_lock.h>
#include <linux/blkdev.h>
#include <asm/page.h>
#include <linux/nls.h>
#include <linux/ntfs_fs.h>

/* Forward declarations. */
static struct inode_operations ntfs_dir_inode_operations;
static struct file_operations ntfs_dir_operations;

#define ITEM_SIZE 2040

/* Io functions to user space. */
static void ntfs_putuser(ntfs_io* dest, void *src, ntfs_size_t len)
{
	copy_to_user(dest->param, src, len);
	dest->param += len;
}

#ifdef CONFIG_NTFS_RW
struct ntfs_getuser_update_vm_s {
	const char *user;
	struct inode *ino;
	loff_t off;
};

static void ntfs_getuser_update_vm(void *dest, ntfs_io *src, ntfs_size_t len)
{
	struct ntfs_getuser_update_vm_s *p = src->param;
	
	copy_from_user(dest, p->user, len);
	p->user += len;
	p->off += len;
}
#endif

/* loff_t is 64 bit signed, so is cool. */
static ssize_t ntfs_read(struct file *filp, char *buf, size_t count,loff_t *off)
{
	int error;
	ntfs_io io;
	ntfs_attribute *attr;
	ntfs_inode *ino = NTFS_LINO2NINO(filp->f_dentry->d_inode);

	/* Inode is not properly initialized. */
	if (!ino)
		return -EINVAL;
	ntfs_debug(DEBUG_OTHER, "ntfs_read %x, %Lx, %x ->",
		   (unsigned)ino->i_number, (unsigned long long)*off,
		   (unsigned)count);
	attr = ntfs_find_attr(ino, ino->vol->at_data, NULL);
	/* Inode has no unnamed data attribute. */
	if (!attr) {
		ntfs_debug(DEBUG_OTHER, "ntfs_read: $DATA not found!\n");
		return -EINVAL;
	}
	if (attr->flags & ATTR_IS_ENCRYPTED)
		return -EACCES;
	/* Read the data. */
	io.fn_put = ntfs_putuser;
	io.fn_get = 0;
	io.param = buf;
	io.size = count;
	error = ntfs_read_attr(ino, ino->vol->at_data, NULL, *off, &io);
	if (error && !io.size) {
		ntfs_debug(DEBUG_OTHER, "ntfs_read: read_attr failed with "
				"error %i, io size %u.\n", error, io.size);
		return error;
	}
	*off += io.size;
	ntfs_debug(DEBUG_OTHER, "ntfs_read: finished. read %u bytes.\n",
								io.size);
	return io.size;
}

#ifdef CONFIG_NTFS_RW
static ssize_t ntfs_write(struct file *filp, const char *buf, size_t count,
		loff_t *pos)
{
	int err;
	struct inode *vfs_ino = filp->f_dentry->d_inode;
	ntfs_inode *ntfs_ino = NTFS_LINO2NINO(vfs_ino);
	ntfs_attribute *data;
	ntfs_io io;
	struct ntfs_getuser_update_vm_s param;

	if (!ntfs_ino)
		return -EINVAL;
	ntfs_debug(DEBUG_LINUX, "%s(): Entering for inode 0x%lx, *pos 0x%Lx, "
			"count 0x%x.\n", __FUNCTION__, ntfs_ino->i_number,
			*pos, count);
	/* Allows to lock fs ro at any time. */
	if (vfs_ino->i_sb->s_flags & MS_RDONLY)
		return -EROFS;
	data = ntfs_find_attr(ntfs_ino, ntfs_ino->vol->at_data, NULL);
	if (!data)
		return -EINVAL;
	/* Evaluating O_APPEND is the file system's job... */
	if (filp->f_flags & O_APPEND)
		*pos = vfs_ino->i_size;
	if (!data->resident && *pos + count > data->allocated) {
		err = ntfs_extend_attr(ntfs_ino, data, *pos + count);
		if (err < 0)
			return err;
	}
	param.user = buf;
	param.ino = vfs_ino;
	param.off = *pos;
	io.fn_put = 0;
	io.fn_get = ntfs_getuser_update_vm;
	io.param = &param;
	io.size = count;
	io.do_read = 0;
	err = ntfs_readwrite_attr(ntfs_ino, data, *pos, &io);
	ntfs_debug(DEBUG_LINUX, "%s(): Returning %i\n", __FUNCTION__, -err);
	if (!err) {
		*pos += io.size;
		if (*pos > vfs_ino->i_size)
			vfs_ino->i_size = *pos;
		mark_inode_dirty(vfs_ino);
		return io.size;
	}
	return err;
}
#endif

struct ntfs_filldir {
	struct inode *dir;
	filldir_t filldir;
	unsigned int type;
	u32 ph, pl;
	void *dirent;
	char *name;
	int namelen;
	int ret_code;
};

static int ntfs_printcb(ntfs_u8 *entry, void *param)
{
	unsigned long inum = NTFS_GETU64(entry) & 0xffffffffffff;
	struct ntfs_filldir *nf = param;
	u32 flags = NTFS_GETU32(entry + 0x48);
	char show_sys_files = 0;
	u8 name_len = NTFS_GETU8(entry + 0x50);
	u8 name_type = NTFS_GETU8(entry + 0x51);
	int err;
	unsigned file_type;

	switch (nf->type) {
	case ngt_dos:
		/* Don't display long names. */
		if (!(name_type & 2))
			return 0;
		break;
	case ngt_nt:
		/* Don't display short-only names. */
		if ((name_type & 3) == 2)
			return 0;
		break;
	case ngt_posix:
		break;
	case ngt_full:
		show_sys_files = 1;
		break;
	default:
		BUG();
	}
	err = ntfs_encodeuni(NTFS_INO2VOL(nf->dir), (ntfs_u16*)(entry + 0x52),
			name_len, &nf->name, &nf->namelen);
	if (err) {
		ntfs_debug(DEBUG_OTHER, "%s(): Skipping unrepresentable "
				"file.\n", __FUNCTION__);
		err = 0;
		goto err_noname;
	}
	if (!show_sys_files && inum < 0x10UL) {
		ntfs_debug(DEBUG_OTHER, "%s(): Skipping system file (%s).\n",
				__FUNCTION__, nf->name);
		err = 0;
		goto err_ret;
	}
	/* Do not return ".", as this is faked. */
	if (nf->namelen == 1 && nf->name[0] == '.') {
		ntfs_debug(DEBUG_OTHER, "%s(): Skipping \".\"\n", __FUNCTION__);
		err = 0;
		goto err_ret;
	}
	nf->name[nf->namelen] = 0;
	if (flags & 0x10000000) /* FILE_ATTR_DUP_FILE_NAME_INDEX_PRESENT */
		file_type = DT_DIR;
	else
		file_type = DT_REG;
	ntfs_debug(DEBUG_OTHER, "%s(): Calling filldir for %s with "
			"len %i, f_pos 0x%Lx, inode %lu, %s.\n", __FUNCTION__,
			nf->name, nf->namelen, (loff_t)(nf->ph << 16) | nf->pl,
			inum, file_type == DT_DIR ? "DT_DIR" : "DT_REG");
	/*
	 * Userspace side of filldir expects an off_t rather than an loff_t.
	 * And it also doesn't like the most significant bit being set as it
	 * then considers the value to be negative. Thus this implementation
	 * limits the number of index records to 32766, which should be plenty.
	 */
	err = nf->filldir(nf->dirent, nf->name, nf->namelen,
			(loff_t)(nf->ph << 16) | nf->pl, inum, file_type);
	if (err)
		nf->ret_code = err;
err_ret:
	ntfs_free(nf->name);
err_noname:
	nf->namelen = 0;
	nf->name = NULL;
	return err;
}

/*
 * readdir returns '.', then '..', then the directory entries in sequence.
 * As the root directory contains an entry for itself, '.' is not emulated for
 * the root directory.
 */
static int ntfs_readdir(struct file* filp, void *dirent, filldir_t filldir)
{
	struct inode *dir = filp->f_dentry->d_inode;
	int err;
	struct ntfs_filldir cb;

	cb.ret_code = 0;
	cb.pl = filp->f_pos & 0xffff;
	cb.ph = (filp->f_pos >> 16) & 0x7fff;
	filp->f_pos = (loff_t)(cb.ph << 16) | cb.pl;
	ntfs_debug(DEBUG_OTHER, "%s(): Entering for inode %lu, f_pos 0x%Lx, "
			"i_mode 0x%x, i_count %lu.\n", __FUNCTION__,
			dir->i_ino, filp->f_pos, (unsigned int)dir->i_mode,
			atomic_read(&dir->i_count));
	if (!cb.ph) {
		/* Start of directory. Emulate "." and "..". */
		if (!cb.pl) {
			ntfs_debug(DEBUG_OTHER, "%s(): Calling filldir for . "
				    "with len 1, f_pos 0x%Lx, inode %lu, "
				    "DT_DIR.\n", __FUNCTION__, filp->f_pos,
				    dir->i_ino);
			cb.ret_code = filldir(dirent, ".", 1, filp->f_pos,
				    dir->i_ino, DT_DIR);
			if (cb.ret_code)
				goto done;
			cb.pl++;
			filp->f_pos = (loff_t)(cb.ph << 16) | cb.pl;
		}
		if (cb.pl == (u32)1) {
			ntfs_debug(DEBUG_OTHER, "%s(): Calling filldir for .. "
				    "with len 2, f_pos 0x%Lx, inode %lu, "
				    "DT_DIR.\n", __FUNCTION__, filp->f_pos,
				    filp->f_dentry->d_parent->d_inode->i_ino);
			cb.ret_code = filldir(dirent, "..", 2, filp->f_pos,
				    filp->f_dentry->d_parent->d_inode->i_ino,
				    DT_DIR);
			if (cb.ret_code)
				goto done;
			cb.pl++;
			filp->f_pos = (loff_t)(cb.ph << 16) | cb.pl;
		}
	} else if (cb.ph >= 0x7fff)
		/* End of directory. */
		goto done;
	cb.dir = dir;
	cb.filldir = filldir;
	cb.dirent = dirent;
	cb.type = NTFS_INO2VOL(dir)->ngt;
	do {
		ntfs_debug(DEBUG_OTHER, "%s(): Looking for next file using "
				"ntfs_getdir_unsorted(), f_pos 0x%Lx.\n",
				__FUNCTION__, (loff_t)(cb.ph << 16) | cb.pl);
		err = ntfs_getdir_unsorted(NTFS_LINO2NINO(dir), &cb.ph, &cb.pl,
				ntfs_printcb, &cb);
	} while (!err && !cb.ret_code && cb.ph < 0x7fff);
	filp->f_pos = (loff_t)(cb.ph << 16) | cb.pl;
	ntfs_debug(DEBUG_OTHER, "%s(): After ntfs_getdir_unsorted()"
			" calls, f_pos 0x%Lx.\n", __FUNCTION__, filp->f_pos);
	if (!err) {
done:
#ifdef DEBUG
		if (!cb.ret_code)
			ntfs_debug(DEBUG_OTHER, "%s(): EOD, f_pos 0x%Lx, "
					"returning 0.\n", __FUNCTION__,
					filp->f_pos);
		else 
			ntfs_debug(DEBUG_OTHER, "%s(): filldir returned %i, "
					"returning 0, f_pos 0x%Lx.\n",
					__FUNCTION__, cb.ret_code, filp->f_pos);
#endif
		return 0;
	}
	ntfs_debug(DEBUG_OTHER, "%s(): Returning %i, f_pos 0x%Lx.\n",
			__FUNCTION__, err, filp->f_pos);
	return err;
}

/* Copied from vfat driver. */
static int simple_getbool(char *s, int *setval)
{
	if (s) {
		if (!strcmp(s, "1") || !strcmp(s, "yes") || !strcmp(s, "true"))
			*setval = 1;
		else if (!strcmp(s, "0") || !strcmp(s, "no") ||
							!strcmp(s, "false"))
			*setval = 0;
		else
			return 0;
	} else
		*setval = 1;
	return 1;
}

/*
 * This needs to be outside parse_options() otherwise a remount will reset
 * these unintentionally.
 */
static void init_ntfs_super_block(ntfs_volume* vol)
{
	vol->uid = vol->gid = 0;
	vol->umask = 0077;
	vol->ngt = ngt_nt;
	vol->nls_map = (void*)-1;
	vol->mft_zone_multiplier = -1;
}

/* Parse the (re)mount options. */
static int parse_options(ntfs_volume *vol, char *opt)
{
	char *value;		/* Defaults if not specified and !remount. */
	ntfs_uid_t uid = -1;	/* 0, root user only */
	ntfs_gid_t gid = -1;	/* 0, root user only */
	int umask = -1;		/* 0077, owner access only */
	unsigned int ngt = -1;	/* ngt_nt */
	void *nls_map = NULL;	/* Try to load the default NLS. */
	int use_utf8 = -1;	/* If no NLS specified and loading the default
				   NLS failed use utf8. */
	int mft_zone_mul = -1;	/* 1 */

	if (!opt)
		goto done;
	for (opt = strtok(opt, ","); opt; opt = strtok(NULL, ",")) {
		if ((value = strchr(opt, '=')) != NULL)
			*value ++= '\0';
		if (strcmp(opt, "uid") == 0) {
			if (!value || !*value)
				goto needs_arg;
			uid = simple_strtoul(value, &value, 0);
			if (*value) {
				printk(KERN_ERR "NTFS: uid invalid argument\n");
				return 0;
			}
		} else if (strcmp(opt, "gid") == 0) {
			if (!value || !*value)
				goto needs_arg;
			gid = simple_strtoul(value, &value, 0);
			if (*value) {
				printk(KERN_ERR "NTFS: gid invalid argument\n");
				return 0;
			}
		} else if (strcmp(opt, "umask") == 0) {
			if (!value || !*value)
				goto needs_arg;
			umask = simple_strtoul(value, &value, 0);
			if (*value) {
				printk(KERN_ERR "NTFS: umask invalid "
						"argument\n");
				return 0;
			}
		} else if (strcmp(opt, "mft_zone_multiplier") == 0) {
			unsigned long ul;

			if (!value || !*value)
				goto needs_arg;
			ul = simple_strtoul(value, &value, 0);
			if (*value) {
				printk(KERN_ERR "NTFS: mft_zone_multiplier "
						"invalid argument\n");
				return 0;
			}
			if (ul >= 1 && ul <= 4)
				mft_zone_mul = ul;
			else {
				mft_zone_mul = 1;
				printk(KERN_WARNING "NTFS: mft_zone_multiplier "
					      "out of range. Setting to 1.\n");
			}
		} else if (strcmp(opt, "posix") == 0) {
			int val;
			if (!value || !*value)
				goto needs_arg;
			if (!simple_getbool(value, &val))
				goto needs_bool;
			ngt = val ? ngt_posix : ngt_nt;
		} else if (strcmp(opt, "show_sys_files") == 0) {
			int val = 0;
			if (!value || !*value)
				val = 1;
			else if (!simple_getbool(value, &val))
				goto needs_bool;
			ngt = val ? ngt_full : ngt_nt;
		} else if (strcmp(opt, "iocharset") == 0) {
			if (!value || !*value)
				goto needs_arg;
			nls_map = load_nls(value);
			if (!nls_map) {
				printk(KERN_ERR "NTFS: charset not found");
				return 0;
			}
		} else if (strcmp(opt, "utf8") == 0) {
			int val = 0;
			if (!value || !*value)
				val = 1;
			else if (!simple_getbool(value, &val))
				goto needs_bool;
			use_utf8 = val;
		} else {
			printk(KERN_ERR "NTFS: unkown option '%s'\n", opt);
			return 0;
		}
	}
done:
	if (use_utf8 == -1) {
		/* utf8 was not specified at all. */
		if (!nls_map) {
			/*
			 * No NLS was specified. If first mount, load the
			 * default NLS, otherwise don't change the NLS setting.
			 */
			if (vol->nls_map == (void*)-1)
				vol->nls_map = load_nls_default();
		} else {
			/* If an NLS was already loaded, unload it first. */
			if (vol->nls_map && vol->nls_map != (void*)-1)
				unload_nls(vol->nls_map);
			/* Use the specified NLS. */
			vol->nls_map = nls_map;
		}
	} else {
		/* utf8 was specified. */
		if (use_utf8 && nls_map) {
			unload_nls(nls_map);
			printk(KERN_ERR "NTFS: utf8 cannot be combined with "
					"iocharset.\n");
			return 0;
		}
		/* If an NLS was already loaded, unload it first. */
		if (vol->nls_map && vol->nls_map != (void*)-1)
			unload_nls(vol->nls_map);
		if (!use_utf8) {
			/* utf8 was specified as false. */
			if (!nls_map)
				/* No NLS was specified, load the default. */
				vol->nls_map = load_nls_default();
			else
				/* Use the specified NLS. */
				vol->nls_map = nls_map;
		} else
			/* utf8 was specified as true. */
			vol->nls_map = NULL;
	}
	if (uid != -1)
		vol->uid = uid;
	if (gid != -1)
		vol->gid = gid;
	if (umask != -1)
		vol->umask = (ntmode_t)umask;
	if (ngt != -1)
		vol->ngt = ngt;
	if (mft_zone_mul != -1) {
		/* mft_zone_multiplier was specified. */
		if (vol->mft_zone_multiplier != -1) {
			/* This is a remount, ignore a change and warn user. */
			if (vol->mft_zone_multiplier != mft_zone_mul)
				printk(KERN_WARNING "NTFS: Ignoring changes in "
						"mft_zone_multiplier on "
						"remount. If you want to "
						"change this you need to "
						"umount and mount again.\n");
		} else
			/* Use the specified multiplier. */
			vol->mft_zone_multiplier = mft_zone_mul;
	} else if (vol->mft_zone_multiplier == -1)
		/* No multiplier specified and first mount, so set default. */
		vol->mft_zone_multiplier = 1;
	return 1;
needs_arg:
	printk(KERN_ERR "NTFS: %s needs an argument", opt);
	return 0;
needs_bool:
	printk(KERN_ERR "NTFS: %s needs boolean argument", opt);
	return 0;
}
			
static struct dentry *ntfs_lookup(struct inode *dir, struct dentry *d)
{
	struct inode *res = 0;
	char *item = 0;
	ntfs_iterate_s walk;
	int err;
	
	ntfs_debug(DEBUG_NAME1, "%s(): Looking up %s in directory ino 0x%x.\n",
			__FUNCTION__, d->d_name.name, (unsigned)dir->i_ino);
	walk.name = NULL;
	walk.namelen = 0;
	/* Convert to wide string. */
	err = ntfs_decodeuni(NTFS_INO2VOL(dir), (char*)d->d_name.name,
			       d->d_name.len, &walk.name, &walk.namelen);
	if (err)
		goto err_ret;
	item = ntfs_malloc(ITEM_SIZE);
	if (!item) {
		err = -ENOMEM;
		goto err_ret;
	}
	/* ntfs_getdir will place the directory entry into item, and the first
	 * long long is the MFT record number. */
	walk.type = BY_NAME;
	walk.dir = NTFS_LINO2NINO(dir);
	walk.result = item;
	if (ntfs_getdir_byname(&walk))
		res = iget(dir->i_sb, NTFS_GETU32(item));
	d_add(d, res);
	ntfs_free(item);
	ntfs_free(walk.name);
	/* Always return success, the dcache will handle negative entries. */
	return NULL;
err_ret:
	ntfs_free(walk.name);
	return ERR_PTR(err);
}

static struct file_operations ntfs_file_operations = {
	llseek:		generic_file_llseek,
	read:		ntfs_read,
#ifdef CONFIG_NTFS_RW
	write:		ntfs_write,
#endif
	open:		generic_file_open,
};

static struct inode_operations ntfs_inode_operations;

#ifdef CONFIG_NTFS_RW
static int ntfs_create(struct inode* dir, struct dentry *d, int mode)
{
	struct inode *r = 0;
	ntfs_inode *ino = 0;
	ntfs_volume *vol;
	int error = 0;
	ntfs_attribute *si;

	r = new_inode(dir->i_sb);
	if (!r) {
		error = -ENOMEM;
		goto fail;
	}
	ntfs_debug(DEBUG_OTHER, "ntfs_create %s\n", d->d_name.name);
	vol = NTFS_INO2VOL(dir);
	ino = NTFS_LINO2NINO(r);
	error = ntfs_alloc_file(NTFS_LINO2NINO(dir), ino, (char*)d->d_name.name,
				d->d_name.len);
	if (error) {
		ntfs_error("ntfs_alloc_file FAILED: error = %i", error);
		goto fail;
	}
	/* Not doing this one was causing a huge amount of corruption! Now the
	 * bugger bytes the dust! (-8 (AIA) */
	r->i_ino = ino->i_number;
	error = ntfs_update_inode(ino);
	if (error)
		goto fail;
	error = ntfs_update_inode(NTFS_LINO2NINO(dir));
	if (error)
		goto fail;
	r->i_uid = vol->uid;
	r->i_gid = vol->gid;
	/* FIXME: dirty? dev? */
	/* Get the file modification times from the standard information. */
	si = ntfs_find_attr(ino, vol->at_standard_information, NULL);
	if (si) {
		char *attr = si->d.data;
		r->i_atime = ntfs_ntutc2unixutc(NTFS_GETU64(attr + 0x18));
		r->i_ctime = ntfs_ntutc2unixutc(NTFS_GETU64(attr));
		r->i_mtime = ntfs_ntutc2unixutc(NTFS_GETU64(attr + 8));
	}
	/* It's not a directory */
	r->i_op = &ntfs_inode_operations;
	r->i_fop = &ntfs_file_operations;
	r->i_mode = S_IFREG | S_IRUGO;
#ifdef CONFIG_NTFS_RW
	r->i_mode |= S_IWUGO;
#endif
	r->i_mode &= ~vol->umask;
	insert_inode_hash(r);
	d_instantiate(d, r);
	return 0;
 fail:
	if (r)
		iput(r);
	return error;
}

static int _linux_ntfs_mkdir(struct inode *dir, struct dentry* d, int mode)
{
	int error;
	struct inode *r = 0;
	ntfs_volume *vol;
	ntfs_inode *ino;
	ntfs_attribute *si;

	ntfs_debug (DEBUG_DIR1, "mkdir %s in %x\n", d->d_name.name, dir->i_ino);
	error = -ENAMETOOLONG;
	if (d->d_name.len > /* FIXME: */ 255)
		goto out;
	error = -EIO;
	r = new_inode(dir->i_sb);
	if (!r)
		goto out;
	vol = NTFS_INO2VOL(dir);
	ino = NTFS_LINO2NINO(r);
	error = ntfs_mkdir(NTFS_LINO2NINO(dir), d->d_name.name, d->d_name.len,
			   ino);
	if (error)
		goto out;
	/* Not doing this one was causing a huge amount of corruption! Now the
	 * bugger bytes the dust! (-8 (AIA) */
	r->i_ino = ino->i_number;
	r->i_uid = vol->uid;
	r->i_gid = vol->gid;
	si = ntfs_find_attr(ino, vol->at_standard_information, NULL);
	if (si) {
		char *attr = si->d.data;
		r->i_atime = ntfs_ntutc2unixutc(NTFS_GETU64(attr + 0x18));
		r->i_ctime = ntfs_ntutc2unixutc(NTFS_GETU64(attr));
		r->i_mtime = ntfs_ntutc2unixutc(NTFS_GETU64(attr + 8));
	}
	/* It's a directory. */
	r->i_op = &ntfs_dir_inode_operations;
	r->i_fop = &ntfs_dir_operations;
	r->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
#ifdef CONFIG_NTFS_RW
	r->i_mode |= S_IWUGO;
#endif
	r->i_mode &= ~vol->umask;	
	
	insert_inode_hash(r);
	d_instantiate(d, r);
	error = 0;
 out:
 	ntfs_debug (DEBUG_DIR1, "mkdir returns %d\n", error);
	return error;
}
#endif

static struct file_operations ntfs_dir_operations = {
	read:		generic_read_dir,
	readdir:	ntfs_readdir,
};

static struct inode_operations ntfs_dir_inode_operations = {
	lookup:		ntfs_lookup,
#ifdef CONFIG_NTFS_RW
	create:		ntfs_create,
	mkdir:		_linux_ntfs_mkdir,
#endif
};

/* ntfs_read_inode() is called by the Virtual File System (the kernel layer 
 * that deals with filesystems) when iget is called requesting an inode not
 * already present in the inode table. Typically filesystems have separate
 * inode_operations for directories, files and symlinks. */
static void ntfs_read_inode(struct inode* inode)
{
	ntfs_volume *vol;
	ntfs_inode *ino;
	ntfs_attribute *data;
	ntfs_attribute *si;

	vol = NTFS_INO2VOL(inode);
	inode->i_mode = 0;
	ntfs_debug(DEBUG_OTHER, "ntfs_read_inode 0x%lx\n", inode->i_ino);
	switch (inode->i_ino) {
		/* Those are loaded special files. */
	case FILE_Mft:
		if (!vol->mft_ino || ((vol->ino_flags & 1) == 0))
			goto sys_file_error;
		ntfs_memcpy(&inode->u.ntfs_i, vol->mft_ino, sizeof(ntfs_inode));
		ino = vol->mft_ino;
		vol->mft_ino = &inode->u.ntfs_i;
		vol->ino_flags &= ~1;
		ntfs_free(ino);
		ino = vol->mft_ino;
		ntfs_debug(DEBUG_OTHER, "Opening $MFT!\n");
		break;
	case FILE_MftMirr:
		if (!vol->mftmirr || ((vol->ino_flags & 2) == 0))
			goto sys_file_error;
		ntfs_memcpy(&inode->u.ntfs_i, vol->mftmirr, sizeof(ntfs_inode));
		ino = vol->mftmirr;
		vol->mftmirr = &inode->u.ntfs_i;
		vol->ino_flags &= ~2;
		ntfs_free(ino);
		ino = vol->mftmirr;
		ntfs_debug(DEBUG_OTHER, "Opening $MFTMirr!\n");
		break;
	case FILE_BitMap:
		if (!vol->bitmap || ((vol->ino_flags & 4) == 0))
			goto sys_file_error;
		ntfs_memcpy(&inode->u.ntfs_i, vol->bitmap, sizeof(ntfs_inode));
		ino = vol->bitmap;
		vol->bitmap = &inode->u.ntfs_i;
		vol->ino_flags &= ~4;
		ntfs_free(ino);
		ino = vol->bitmap;
		ntfs_debug(DEBUG_OTHER, "Opening $Bitmap!\n");
		break;
	case FILE_LogFile ... FILE_AttrDef:
	/* No need to log root directory accesses. */
	case FILE_Boot ... FILE_UpCase:
		ntfs_debug(DEBUG_OTHER, "Opening system file %i!\n",
				inode->i_ino);
	default:
		ino = &inode->u.ntfs_i;
		if (!ino || ntfs_init_inode(ino, NTFS_INO2VOL(inode),
								inode->i_ino))
		{
			ntfs_debug(DEBUG_OTHER, "NTFS: Error loading inode "
					"0x%x\n", (unsigned int)inode->i_ino);
			return;
		}
	}
	/* Set uid/gid from mount options */
	inode->i_uid = vol->uid;
	inode->i_gid = vol->gid;
	inode->i_nlink = 1;
	/* Use the size of the data attribute as file size */
	data = ntfs_find_attr(ino, vol->at_data, NULL);
	if (!data)
		inode->i_size = 0;
	else
		inode->i_size = data->size;
	/* Get the file modification times from the standard information. */
	si = ntfs_find_attr(ino, vol->at_standard_information, NULL);
	if (si) {
		char *attr = si->d.data;
		inode->i_atime = ntfs_ntutc2unixutc(NTFS_GETU64(attr + 0x18));
		inode->i_ctime = ntfs_ntutc2unixutc(NTFS_GETU64(attr));
		inode->i_mtime = ntfs_ntutc2unixutc(NTFS_GETU64(attr + 8));
	}
	/* If it has an index root, it's a directory. */
	if (ntfs_find_attr(ino, vol->at_index_root, "$I30")) {
		ntfs_attribute *at;
		at = ntfs_find_attr(ino, vol->at_index_allocation, "$I30");
		inode->i_size = at ? at->size : 0;
		inode->i_op = &ntfs_dir_inode_operations;
		inode->i_fop = &ntfs_dir_operations;
		inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO;
	} else {
		inode->i_op = &ntfs_inode_operations;
		inode->i_fop = &ntfs_file_operations;
		inode->i_mode = S_IFREG | S_IRUGO;
	}
#ifdef CONFIG_NTFS_RW
	if (!data || !(data->flags & (ATTR_IS_COMPRESSED | ATTR_IS_ENCRYPTED)))
		inode->i_mode |= S_IWUGO;
#endif
	inode->i_mode &= ~vol->umask;
	return;
sys_file_error:
	ntfs_error("Critical error. Tried to call ntfs_read_inode() before we "
		"have completed read_super() or VFS error.\n");
	// FIXME: Should we panic() at this stage?
}

#ifdef CONFIG_NTFS_RW
static void ntfs_write_inode(struct inode *ino, int unused)
{
	lock_kernel();
	ntfs_debug(DEBUG_LINUX, "ntfs_write_inode 0x%x\n", ino->i_ino);
	ntfs_update_inode(NTFS_LINO2NINO(ino));
	unlock_kernel();
}
#endif

static void _ntfs_clear_inode(struct inode *inode)
{
	ntfs_inode *ino;
	ntfs_volume *vol;
	
	lock_kernel();
	ntfs_debug(DEBUG_OTHER, "_ntfs_clear_inode 0x%x\n", inode->i_ino);
	vol = NTFS_INO2VOL(inode);
	if (!vol)
		ntfs_error("_ntfs_clear_inode: vol = NTFS_INO2VOL(inode) is "
				"NULL.\n");
	switch (inode->i_ino) {
	case FILE_Mft:
		if (vol->mft_ino && ((vol->ino_flags & 1) == 0)) {
			ino = (ntfs_inode*)ntfs_malloc(sizeof(ntfs_inode));
			ntfs_memcpy(ino, &inode->u.ntfs_i, sizeof(ntfs_inode));
			vol->mft_ino = ino;
			vol->ino_flags |= 1;
			goto unl_out;
		}
		break;
	case FILE_MftMirr:
		if (vol->mftmirr && ((vol->ino_flags & 2) == 0)) {
			ino = (ntfs_inode*)ntfs_malloc(sizeof(ntfs_inode));
			ntfs_memcpy(ino, &inode->u.ntfs_i, sizeof(ntfs_inode));
			vol->mftmirr = ino;
			vol->ino_flags |= 2;
			goto unl_out;
		}
		break;
	case FILE_BitMap:
		if (vol->bitmap && ((vol->ino_flags & 4) == 0)) {
			ino = (ntfs_inode*)ntfs_malloc(sizeof(ntfs_inode));
			ntfs_memcpy(ino, &inode->u.ntfs_i, sizeof(ntfs_inode));
			vol->bitmap = ino;
			vol->ino_flags |= 4;
			goto unl_out;
		}
		break;
		/* Nothing. Just clear the inode and exit. */
	}
	ntfs_clear_inode(&inode->u.ntfs_i);
unl_out:
	unlock_kernel();
	return;
}

/* Called when umounting a filesystem by do_umount() in fs/super.c. */
static void ntfs_put_super(struct super_block *sb)
{
	ntfs_volume *vol;

	ntfs_debug(DEBUG_OTHER, "ntfs_put_super\n");
	vol = NTFS_SB2VOL(sb);
	ntfs_release_volume(vol);
	if (vol->nls_map)
		unload_nls(vol->nls_map);
	ntfs_debug(DEBUG_OTHER, "ntfs_put_super: done\n");
}

/* Called by the kernel when asking for stats. */
static int ntfs_statfs(struct super_block *sb, struct statfs *sf)
{
	struct inode *mft;
	ntfs_volume *vol;
	__s64 size;
	int error;

	ntfs_debug(DEBUG_OTHER, "ntfs_statfs\n");
	vol = NTFS_SB2VOL(sb);
	sf->f_type = NTFS_SUPER_MAGIC;
	sf->f_bsize = vol->cluster_size;
	error = ntfs_get_volumesize(NTFS_SB2VOL(sb), &size);
	if (error)
		return error;
	sf->f_blocks = size;	/* Volumesize is in clusters. */
	size = (__s64)ntfs_get_free_cluster_count(vol->bitmap);
	/* Just say zero if the call failed. */
	if (size < 0LL)
		size = 0;
	sf->f_bfree = sf->f_bavail = size;
	ntfs_debug(DEBUG_OTHER, "ntfs_statfs: calling mft = iget(sb, "
			"FILE_Mft)\n");
	mft = iget(sb, FILE_Mft);
	ntfs_debug(DEBUG_OTHER, "ntfs_statfs: iget(sb, FILE_Mft) returned "
			"0x%x\n", mft);
	if (!mft)
		return -EIO;
	sf->f_files = mft->i_size >> vol->mft_record_size_bits;
	ntfs_debug(DEBUG_OTHER, "ntfs_statfs: calling iput(mft)\n");
	iput(mft);
	/* Should be read from volume. */
	sf->f_namelen = 255;
	return 0;
}

/* Called when remounting a filesystem by do_remount_sb() in fs/super.c. */
static int ntfs_remount_fs(struct super_block *sb, int *flags, char *options)
{
	if (!parse_options(NTFS_SB2VOL(sb), options))
		return -EINVAL;
	return 0;
}

/* Define the super block operation that are implemented */
static struct super_operations ntfs_super_operations = {
	read_inode:	ntfs_read_inode,
#ifdef CONFIG_NTFS_RW
	write_inode:	ntfs_write_inode,
#endif
	put_super:	ntfs_put_super,
	statfs:		ntfs_statfs,
	remount_fs:	ntfs_remount_fs,
	clear_inode:	_ntfs_clear_inode,
};

/**
 * is_boot_sector_ntfs - check an NTFS boot sector for validity
 * @b:		buffer containing bootsector to check
 * 
 * Check whether @b contains a valid NTFS boot sector.
 * Return 1 if @b is a valid NTFS bootsector or 0 if not.
 */
static int is_boot_sector_ntfs(ntfs_u8 *b)
{
	ntfs_u32 i;

	/* FIXME: We don't use checksumming yet as NT4(SP6a) doesn't either...
	 * But we might as well have the code ready to do it. (AIA) */
#if 0
	/* Calculate the checksum. */
	if (b < b + 0x50) {
		ntfs_u32 *u;
		ntfs_u32 *bi = (ntfs_u32 *)(b + 0x50);
		
		for (u = bi, i = 0; u < bi; ++u)
			i += NTFS_GETU32(*u);
	}
#endif
	/* Check magic is "NTFS    " */
	if (b[3] != 0x4e) goto not_ntfs;
	if (b[4] != 0x54) goto not_ntfs;
	if (b[5] != 0x46) goto not_ntfs;
	if (b[6] != 0x53) goto not_ntfs;
	for (i = 7; i < 0xb; ++i)
		if (b[i] != 0x20) goto not_ntfs;
	/* Check bytes per sector value is between 512 and 4096. */
	if (b[0xb] != 0) goto not_ntfs;
	if (b[0xc] > 0x10) goto not_ntfs;
	/* Check sectors per cluster value is valid. */
	switch (b[0xd]) {
	case 1: case 2: case 4: case 8: case 16:
	case 32: case 64: case 128:
		break;
	default:
		goto not_ntfs;
	}
	/* Check reserved sectors value and four other fields are zero. */
	for (i = 0xe; i < 0x15; ++i) 
		if (b[i] != 0) goto not_ntfs;
	if (b[0x16] != 0) goto not_ntfs;
	if (b[0x17] != 0) goto not_ntfs;
	for (i = 0x20; i < 0x24; ++i)
		if (b[i] != 0) goto not_ntfs;
	/* Check clusters per file record segment value is valid. */
	if (b[0x40] < 0xe1 || b[0x40] > 0xf7) {
		switch (b[0x40]) {
		case 1: case 2: case 4: case 8: case 16: case 32: case 64:
			break;
		default:
			goto not_ntfs;
		}
	}
	/* Check clusters per index block value is valid. */
	if (b[0x44] < 0xe1 || b[0x44] > 0xf7) {
		switch (b[0x44]) {
		case 1: case 2: case 4: case 8: case 16: case 32: case 64:
			break;
		default:
			goto not_ntfs;
		}
	}
	return 1;
not_ntfs:
	return 0;
}

/* Called to mount a filesystem by read_super() in fs/super.c.
 * Return a super block, the main structure of a filesystem.
 *
 * NOTE : Don't store a pointer to an option, as the page containing the
 * options is freed after ntfs_read_super() returns.
 *
 * NOTE : A context switch can happen in kernel code only if the code blocks
 * (= calls schedule() in kernel/sched.c). */
struct super_block *ntfs_read_super(struct super_block *sb, void *options,
		int silent)
{
	ntfs_volume *vol;
	struct buffer_head *bh;
	int i, to_read, blocksize;

	ntfs_debug(DEBUG_OTHER, "ntfs_read_super\n");
	vol = NTFS_SB2VOL(sb);
	init_ntfs_super_block(vol);
	if (!parse_options(vol, (char*)options))
		goto ntfs_read_super_vol;
	blocksize = get_hardsect_size(sb->s_dev);
	if (blocksize < 512)
		blocksize = 512;
	if (set_blocksize(sb->s_dev, blocksize) < 0) {
		ntfs_error("Unable to set blocksize %d.\n", blocksize);
		goto ntfs_read_super_vol;
	}
	sb->s_blocksize = blocksize;
	/* Read the super block (boot block). */
	if (!(bh = sb_bread(sb, 0))) {
		ntfs_error("Reading super block failed\n");
		goto ntfs_read_super_unl;
	}
	ntfs_debug(DEBUG_OTHER, "Done reading boot block\n");
	/* Check for valid 'NTFS' boot sector. */
	if (!is_boot_sector_ntfs(bh->b_data)) {
		ntfs_debug(DEBUG_OTHER, "Not a NTFS volume\n");
		bforget(bh);
		goto ntfs_read_super_unl;
	}
	ntfs_debug(DEBUG_OTHER, "Going to init volume\n");
	if (ntfs_init_volume(vol, bh->b_data) < 0) {
		ntfs_debug(DEBUG_OTHER, "Init volume failed.\n");
		bforget(bh);
		goto ntfs_read_super_unl;
	}
	ntfs_debug(DEBUG_OTHER, "$Mft at cluster 0x%lx\n", vol->mft_lcn);
	brelse(bh);
	NTFS_SB(vol) = sb;
	if (vol->cluster_size > PAGE_SIZE) {
		ntfs_error("Partition cluster size is not supported yet (it "
			   "is > max kernel blocksize).\n");
		goto ntfs_read_super_unl;
	}
	ntfs_debug(DEBUG_OTHER, "Done to init volume\n");
	/* Inform the kernel that a device block is a NTFS cluster. */
	sb->s_blocksize = vol->cluster_size;
	sb->s_blocksize_bits = vol->cluster_size_bits;
	if (blocksize != vol->cluster_size &&
			set_blocksize(sb->s_dev, sb->s_blocksize) < 0) {
		ntfs_error("Cluster size too small for device.\n");
		goto ntfs_read_super_unl;
	}
	ntfs_debug(DEBUG_OTHER, "set_blocksize\n");
	/* Allocate an MFT record (MFT record can be smaller than a cluster). */
	i = vol->cluster_size;
	if (i < vol->mft_record_size)
		i = vol->mft_record_size;
	if (!(vol->mft = ntfs_malloc(i)))
		goto ntfs_read_super_unl;

	/* Read at least the MFT record for $Mft. */
	to_read = vol->mft_clusters_per_record;
	if (to_read < 1)
		to_read = 1;
	for (i = 0; i < to_read; i++) {
		if (!(bh = sb_bread(sb, vol->mft_lcn + i))) {
			ntfs_error("Could not read $Mft record 0\n");
			goto ntfs_read_super_mft;
		}
		ntfs_memcpy(vol->mft + ((__s64)i << vol->cluster_size_bits),
						bh->b_data, vol->cluster_size);
		brelse(bh);
		ntfs_debug(DEBUG_OTHER, "Read cluster 0x%x\n",
							 vol->mft_lcn + i);
	}
	/* Check and fixup this MFT record */
	if (!ntfs_check_mft_record(vol, vol->mft)){
		ntfs_error("Invalid $Mft record 0\n");
		goto ntfs_read_super_mft;
	}
	/* Inform the kernel about which super operations are available. */
	sb->s_op = &ntfs_super_operations;
	sb->s_magic = NTFS_SUPER_MAGIC;
	sb->s_maxbytes = MAX_LFS_FILESIZE;
	ntfs_debug(DEBUG_OTHER, "Reading special files\n");
	if (ntfs_load_special_files(vol)) {
		ntfs_error("Error loading special files\n");
		goto ntfs_read_super_mft;
	}
	ntfs_debug(DEBUG_OTHER, "Getting RootDir\n");
	/* Get the root directory. */
	if (!(sb->s_root = d_alloc_root(iget(sb, FILE_root)))) {
		ntfs_error("Could not get root dir inode\n");
		goto ntfs_read_super_mft;
	}
ntfs_read_super_ret:
	ntfs_debug(DEBUG_OTHER, "read_super: done\n");
	return sb;
ntfs_read_super_mft:
	ntfs_free(vol->mft);
ntfs_read_super_unl:
ntfs_read_super_vol:
	sb = NULL;
	goto ntfs_read_super_ret;
}

/* Define the filesystem */
static DECLARE_FSTYPE_DEV(ntfs_fs_type, "ntfs", ntfs_read_super);

static int __init init_ntfs_fs(void)
{
	/* Comment this if you trust klogd. There are reasons not to trust it */
#if defined(DEBUG) && !defined(MODULE)
	console_verbose();
#endif
	printk(KERN_NOTICE "NTFS driver v" NTFS_VERSION " [Flags: R/"
#ifdef CONFIG_NTFS_RW
			"W"
#else
			"O"
#endif
#ifdef DEBUG
			" DEBUG"
#endif
#ifdef MODULE
			" MODULE"
#endif
			"]\n");
	SYSCTL(1);
	ntfs_debug(DEBUG_OTHER, "registering %s\n", ntfs_fs_type.name);
	/* Add this filesystem to the kernel table of filesystems. */
	return register_filesystem(&ntfs_fs_type);
}

static void __exit exit_ntfs_fs(void)
{
	SYSCTL(0);
	ntfs_debug(DEBUG_OTHER, "unregistering %s\n", ntfs_fs_type.name);
	unregister_filesystem(&ntfs_fs_type);
}

EXPORT_NO_SYMBOLS;
/*
 * Not strictly true. The driver was written originally by Martin von Löwis.
 * I am just maintaining and rewriting it.
 */
MODULE_AUTHOR("Anton Altaparmakov <aia21@cus.cam.ac.uk>");
MODULE_DESCRIPTION("Linux NTFS driver");
MODULE_LICENSE("GPL");
#ifdef DEBUG
MODULE_PARM(ntdebug, "i");
MODULE_PARM_DESC(ntdebug, "Debug level");
#endif

module_init(init_ntfs_fs)
module_exit(exit_ntfs_fs)

