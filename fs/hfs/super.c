/*
 * linux/fs/hfs/super.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains hfs_read_super(), some of the super_ops and
 * init_module() and cleanup_module().	The remaining super_ops are in
 * inode.c since they deal with inodes.
 *
 * Based on the minix file system code, (C) 1991, 1992 by Linus Torvalds
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 *
 * The code in this file initializes some structures which contain
 * pointers by calling memset(&foo, 0, sizeof(foo)).
 * This produces the desired behavior only due to the non-ANSI
 * assumption that the machine representation of NULL is all zeros.
 */

#include "hfs.h"
#include <linux/hfs_fs_sb.h>
#include <linux/hfs_fs_i.h>
#include <linux/hfs_fs.h>

#include <linux/config.h> /* for CONFIG_MAC_PARTITION */
#include <linux/blkdev.h>
#include <linux/module.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");

/*================ Forward declarations ================*/

static void hfs_read_inode(struct inode *);
static void hfs_put_super(struct super_block *);
static int hfs_statfs(struct super_block *, struct statfs *);
static void hfs_write_super(struct super_block *);

/*================ Global variables ================*/

static struct super_operations hfs_super_operations = { 
	read_inode:	hfs_read_inode,
	put_inode:	hfs_put_inode,
	put_super:	hfs_put_super,
	write_super:	hfs_write_super,
	statfs:		hfs_statfs,
	remount_fs:     hfs_remount,
};

/*================ File-local variables ================*/

static DECLARE_FSTYPE_DEV(hfs_fs, "hfs", hfs_read_super);

/*================ File-local functions ================*/

/* 
 * hfs_read_inode()
 *
 * this doesn't actually do much. hfs_iget actually fills in the 
 * necessary inode information.
 */
static void hfs_read_inode(struct inode *inode)
{
  inode->i_mode = 0;
}

/*
 * hfs_write_super()
 *
 * Description:
 *   This function is called by the VFS only. When the filesystem
 *   is mounted r/w it updates the MDB on disk.
 * Input Variable(s):
 *   struct super_block *sb: Pointer to the hfs superblock
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'sb' points to a "valid" (struct super_block).
 * Postconditions:
 *   The MDB is marked 'unsuccessfully unmounted' by clearing bit 8 of drAtrb
 *   (hfs_put_super() must set this flag!). Some MDB fields are updated
 *   and the MDB buffer is written to disk by calling hfs_mdb_commit().
 */
static void hfs_write_super(struct super_block *sb)
{
	struct hfs_mdb *mdb = HFS_SB(sb)->s_mdb;

	/* is this a valid hfs superblock? */
	if (!sb || sb->s_magic != HFS_SUPER_MAGIC) {
		return;
	}

	if (!(sb->s_flags & MS_RDONLY)) {
		/* sync everything to the buffers */
		hfs_mdb_commit(mdb, 0);
	}
	sb->s_dirt = 0;
}

/*
 * hfs_put_super()
 *
 * This is the put_super() entry in the super_operations structure for
 * HFS filesystems.  The purpose is to release the resources
 * associated with the superblock sb.
 */
static void hfs_put_super(struct super_block *sb)
{
	struct hfs_mdb *mdb = HFS_SB(sb)->s_mdb;
 
	if (!(sb->s_flags & MS_RDONLY)) {
		hfs_mdb_commit(mdb, 0);
		sb->s_dirt = 0;
	}

	/* release the MDB's resources */
	hfs_mdb_put(mdb, sb->s_flags & MS_RDONLY);

	/* restore default blocksize for the device */
	set_blocksize(sb->s_dev, BLOCK_SIZE);
}

/*
 * hfs_statfs()
 *
 * This is the statfs() entry in the super_operations structure for
 * HFS filesystems.  The purpose is to return various data about the
 * filesystem.
 *
 * changed f_files/f_ffree to reflect the fs_ablock/free_ablocks.
 */
static int hfs_statfs(struct super_block *sb, struct statfs *buf)
{
	struct hfs_mdb *mdb = HFS_SB(sb)->s_mdb;

	buf->f_type = HFS_SUPER_MAGIC;
	buf->f_bsize = HFS_SECTOR_SIZE;
	buf->f_blocks = mdb->alloc_blksz * mdb->fs_ablocks;
	buf->f_bfree = mdb->alloc_blksz * mdb->free_ablocks;
	buf->f_bavail = buf->f_bfree;
	buf->f_files = mdb->fs_ablocks;  
	buf->f_ffree = mdb->free_ablocks;
	buf->f_namelen = HFS_NAMELEN;

	return 0;
}

/*
 * parse_options()
 * 
 * adapted from linux/fs/msdos/inode.c written 1992,93 by Werner Almesberger
 * This function is called by hfs_read_super() to parse the mount options.
 */
static int parse_options(char *options, struct hfs_sb_info *hsb, int *part)
{
	char *this_char, *value;
	char names, fork;

	if (hsb->magic != HFS_SB_MAGIC) {
		/* initialize the sb with defaults */
		hsb->magic = HFS_SB_MAGIC;
		hsb->s_uid   = current->uid;
		hsb->s_gid   = current->gid;
		hsb->s_umask = current->fs->umask;
		hsb->s_type    = 0x3f3f3f3f;	/* == '????' */
		hsb->s_creator = 0x3f3f3f3f;	/* == '????' */
		hsb->s_lowercase = 0;
		hsb->s_quiet     = 0;
		hsb->s_afpd      = 0;
		/* default version. 0 just selects the defaults */
		hsb->s_version   = 0; 
		hsb->s_conv = 'b';
		names = '?';
		fork = '?';
		*part = 0;
	}

	if (!options) {
		goto done;
	}
	for (this_char = strtok(options,","); this_char;
	     this_char = strtok(NULL,",")) {
		if ((value = strchr(this_char,'=')) != NULL) {
			*value++ = 0;
		}
	/* Numeric-valued options */
		if (!strcmp(this_char, "version")) {
			if (!value || !*value) {
				return 0;
			}
			hsb->s_version = simple_strtoul(value,&value,0);
			if (*value) {
				return 0;
			}
		} else if (!strcmp(this_char,"uid")) {
			if (!value || !*value) {
				return 0;
			}
			hsb->s_uid = simple_strtoul(value,&value,0);
			if (*value) {
				return 0;
			}
		} else if (!strcmp(this_char,"gid")) {
			if (!value || !*value) {
				return 0;
			}
			hsb->s_gid = simple_strtoul(value,&value,0);
			if (*value) {
				return 0;
			}
		} else if (!strcmp(this_char,"umask")) {
			if (!value || !*value) {
				return 0;
			}
			hsb->s_umask = simple_strtoul(value,&value,8);
			if (*value) {
				return 0;
			}
		} else if (!strcmp(this_char,"part")) {
			if (!value || !*value) {
				return 0;
			}
			*part = simple_strtoul(value,&value,0);
			if (*value) {
				return 0;
			}
	/* String-valued options */
		} else if (!strcmp(this_char,"type") && value) {
			if (strlen(value) != 4) {
				return 0;
			}
			hsb->s_type = hfs_get_nl(value);
		} else if (!strcmp(this_char,"creator") && value) {
			if (strlen(value) != 4) {
				return 0;
			}
			hsb->s_creator = hfs_get_nl(value);
	/* Boolean-valued options */
		} else if (!strcmp(this_char,"quiet")) {
			if (value) {
				return 0;
			}
			hsb->s_quiet = 1;
		} else if (!strcmp(this_char,"afpd")) {
			if (value) {
				return 0;
			}
			hsb->s_afpd = 1;
	/* Multiple choice options */
		} else if (!strcmp(this_char,"names") && value) {
			if ((*value && !value[1] && strchr("ntal78c",*value)) ||
			    !strcmp(value,"netatalk") ||
			    !strcmp(value,"trivial") ||
			    !strcmp(value,"alpha") ||
			    !strcmp(value,"latin") ||
			    !strcmp(value,"7bit") ||
			    !strcmp(value,"8bit") ||
			    !strcmp(value,"cap")) {
				names = *value;
			} else {
				return 0;
			}
		} else if (!strcmp(this_char,"fork") && value) {
			if ((*value && !value[1] && strchr("nsdc",*value)) ||
			    !strcmp(value,"netatalk") ||
			    !strcmp(value,"single") ||
			    !strcmp(value,"double") ||
			    !strcmp(value,"cap")) {
				fork = *value;
			} else {
				return 0;
			}
		} else if (!strcmp(this_char,"case") && value) {
			if ((*value && !value[1] && strchr("la",*value)) ||
			    !strcmp(value,"lower") ||
			    !strcmp(value,"asis")) {
				hsb->s_lowercase = (*value == 'l');
			} else {
				return 0;
			}
		} else if (!strcmp(this_char,"conv") && value) {
			if ((*value && !value[1] && strchr("bta",*value)) ||
			    !strcmp(value,"binary") ||
			    !strcmp(value,"text") ||
			    !strcmp(value,"auto")) {
				hsb->s_conv = *value;
			} else {
				return 0;
			}
		} else {
			return 0;
		}
	}

done:
	/* Parse the "fork" and "names" options */
	if (fork == '?') {
		fork = hsb->s_afpd ? 'n' : 'c';
	}
	switch (fork) {
	default:
	case 'c':
		hsb->s_ifill = hfs_cap_ifill;
		hsb->s_reserved1 = hfs_cap_reserved1;
		hsb->s_reserved2 = hfs_cap_reserved2;
		break;

	case 's':
		hfs_warn("hfs_fs: AppleSingle not yet implemented.\n");
		return 0;
		/* break; */
	
	case 'd':
		hsb->s_ifill = hfs_dbl_ifill;
		hsb->s_reserved1 = hfs_dbl_reserved1;
		hsb->s_reserved2 = hfs_dbl_reserved2;
		break;

	case 'n':
		hsb->s_ifill = hfs_nat_ifill;
		hsb->s_reserved1 = hfs_nat_reserved1;
		hsb->s_reserved2 = hfs_nat_reserved2;
		break;
	}

	if (names == '?') {
		names = fork;
	}
	switch (names) {
	default:
	case 'n':
		hsb->s_nameout = hfs_colon2mac;
		hsb->s_namein = hfs_mac2nat;
		break;

	case 'c':
		hsb->s_nameout = hfs_colon2mac;
		hsb->s_namein = hfs_mac2cap;
		break;

	case 't':
		hsb->s_nameout = hfs_triv2mac;
		hsb->s_namein = hfs_mac2triv;
		break;

	case '7':
		hsb->s_nameout = hfs_prcnt2mac;
		hsb->s_namein = hfs_mac2seven;
		break;

	case '8':
		hsb->s_nameout = hfs_prcnt2mac;
		hsb->s_namein = hfs_mac2eight;
		break;

	case 'l':
		hsb->s_nameout = hfs_latin2mac;
		hsb->s_namein = hfs_mac2latin;
		break;

 	case 'a':	/* 's' and 'd' are unadvertised aliases for 'alpha', */
 	case 's':	/* since 'alpha' is the default if fork=s or fork=d. */
 	case 'd':	/* (It is also helpful for poor typists!)           */
		hsb->s_nameout = hfs_prcnt2mac;
		hsb->s_namein = hfs_mac2alpha;
		break;
	}

	return 1;
}

/*================ Global functions ================*/

/*
 * hfs_read_super()
 *
 * This is the function that is responsible for mounting an HFS
 * filesystem.	It performs all the tasks necessary to get enough data
 * from the disk to read the root inode.  This includes parsing the
 * mount options, dealing with Macintosh partitions, reading the
 * superblock and the allocation bitmap blocks, calling
 * hfs_btree_init() to get the necessary data about the extents and
 * catalog B-trees and, finally, reading the root inode into memory.
 */
struct super_block *hfs_read_super(struct super_block *s, void *data,
				   int silent)
{
	struct hfs_mdb *mdb;
	struct hfs_cat_key key;
	kdev_t dev = s->s_dev;
	int dev_blocksize;
	hfs_s32 part_size, part_start;
	struct inode *root_inode;
	int part;

	memset(HFS_SB(s), 0, sizeof(*(HFS_SB(s))));	
	if (!parse_options((char *)data, HFS_SB(s), &part)) {
		hfs_warn("hfs_fs: unable to parse mount options.\n");
		goto bail3;
	}

	/* set the device driver to 512-byte blocks */
	if (set_blocksize(dev, HFS_SECTOR_SIZE) < 0) {
		dev_blocksize = get_hardsect_size(dev);
		hfs_warn("hfs_fs: unsupported device block size: %d\n",
			 dev_blocksize);
		goto bail3;
	}
	s->s_blocksize_bits = HFS_SECTOR_SIZE_BITS;
	s->s_blocksize = HFS_SECTOR_SIZE;

#ifdef CONFIG_MAC_PARTITION
	/* check to see if we're in a partition */
	mdb = hfs_mdb_get(s, s->s_flags & MS_RDONLY, 0);

	/* erk. try parsing the partition table ourselves */
	if (!mdb) {
		if (hfs_part_find(s, part, silent, &part_size, &part_start)) {
	    		goto bail2;
	  	}
	  	mdb = hfs_mdb_get(s, s->s_flags & MS_RDONLY, part_start);
	}
#else
	if (hfs_part_find(s, part, silent, &part_size, &part_start)) {
		goto bail2;
	}

	mdb = hfs_mdb_get(s, s->s_flags & MS_RDONLY, part_start);
#endif

	if (!mdb) {
		if (!silent) {
			hfs_warn("VFS: Can't find a HFS filesystem on dev %s.\n",
			       kdevname(dev));
		}
		goto bail2;
	}

	if (mdb->attrib & (HFS_SB_ATTRIB_HLOCK | HFS_SB_ATTRIB_SLOCK)) {
		if (!silent)
			hfs_warn("hfs_fs: Filesystem is marked locked, mounting read-only.\n");
		s->s_flags |= MS_RDONLY;
	}

	HFS_SB(s)->s_mdb = mdb;
	if (HFS_ITYPE(mdb->next_id) != 0) {
		hfs_warn("hfs_fs: too many files.\n");
		goto bail1;
	}

	s->s_magic = HFS_SUPER_MAGIC;
	s->s_op = &hfs_super_operations;

	/* try to get the root inode */
	hfs_cat_build_key(htonl(HFS_POR_CNID),
			  (struct hfs_name *)(mdb->vname), &key);

	root_inode = hfs_iget(hfs_cat_get(mdb, &key), HFS_ITYPE_NORM, NULL);
	if (!root_inode) 
		goto bail_no_root;
	  
	s->s_root = d_alloc_root(root_inode);
	if (!s->s_root) 
		goto bail_no_root;

	/* fix up pointers. */
	HFS_I(root_inode)->entry->sys_entry[HFS_ITYPE_TO_INT(HFS_ITYPE_NORM)] =
	  s->s_root;
	s->s_root->d_op = &hfs_dentry_operations;

	/* everything's okay */
	return s;

bail_no_root: 
	hfs_warn("hfs_fs: get root inode failed.\n");
	iput(root_inode);
bail1:
	hfs_mdb_put(mdb, s->s_flags & MS_RDONLY);
bail2:
	set_blocksize(dev, BLOCK_SIZE);
bail3:
	return NULL;	
}

int hfs_remount(struct super_block *s, int *flags, char *data)
{
        int part; /* ignored */

        if (!parse_options(data, HFS_SB(s), &part)) {
                hfs_warn("hfs_fs: unable to parse mount options.\n");
                return -EINVAL;
	}

        if ((*flags & MS_RDONLY) == (s->s_flags & MS_RDONLY))
                return 0;
	if (!(*flags & MS_RDONLY)) {
                if (HFS_SB(s)->s_mdb->attrib & (HFS_SB_ATTRIB_HLOCK | HFS_SB_ATTRIB_SLOCK)) {
                        hfs_warn("hfs_fs: Filesystem is marked locked, leaving it read-only.\n");
		        s->s_flags |= MS_RDONLY;
			*flags |= MS_RDONLY;
	        }
        }
	return 0;
}

static int __init init_hfs_fs(void)
{
        hfs_cat_init();
	return register_filesystem(&hfs_fs);
}

static void __exit exit_hfs_fs(void) {
	hfs_cat_free();
	unregister_filesystem(&hfs_fs);
}

module_init(init_hfs_fs)
module_exit(exit_hfs_fs)

#if defined(DEBUG_ALL) || defined(DEBUG_MEM)
long int hfs_alloc = 0;
#endif
