/*
 *  linux/fs/affs/inode.c
 *
 *  (c) 1996  Hans-Joachim Widmaier - Rewritten
 *
 *  (C) 1993  Ray Burr - Modified for Amiga FFS filesystem.
 *
 *  (C) 1992  Eric Youngdale Modified for ISO 9660 filesystem.
 *
 *  (C) 1991  Linus Torvalds - minix filesystem
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/sched.h>
#include <linux/affs_fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/locks.h>
#include <linux/genhd.h>
#include <linux/amigaffs.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#include <asm/system.h>
#include <asm/uaccess.h>

extern int *blk_size[];
extern struct timezone sys_tz;

static int affs_statfs(struct super_block *sb, struct statfs *buf);
static int affs_remount (struct super_block *sb, int *flags, char *data);

static void
affs_put_super(struct super_block *sb)
{
	pr_debug("AFFS: put_super()\n");

	if (!(sb->s_flags & MS_RDONLY)) {
		AFFS_ROOT_TAIL(sb, AFFS_SB->s_root_bh)->bm_flag = be32_to_cpu(1);
		secs_to_datestamp(CURRENT_TIME,
				  &AFFS_ROOT_TAIL(sb, AFFS_SB->s_root_bh)->disk_change);
		affs_fix_checksum(sb, AFFS_SB->s_root_bh);
		mark_buffer_dirty(AFFS_SB->s_root_bh);
	}

	affs_brelse(AFFS_SB->s_bmap_bh);
	if (AFFS_SB->s_prefix)
		kfree(AFFS_SB->s_prefix);
	kfree(AFFS_SB->s_bitmap);
	affs_brelse(AFFS_SB->s_root_bh);

	return;
}

static void
affs_write_super(struct super_block *sb)
{
	int clean = 2;

	if (!(sb->s_flags & MS_RDONLY)) {
		//	if (AFFS_SB->s_bitmap[i].bm_bh) {
		//		if (buffer_dirty(AFFS_SB->s_bitmap[i].bm_bh)) {
		//			clean = 0;
		AFFS_ROOT_TAIL(sb, AFFS_SB->s_root_bh)->bm_flag = be32_to_cpu(clean);
		secs_to_datestamp(CURRENT_TIME,
				  &AFFS_ROOT_TAIL(sb, AFFS_SB->s_root_bh)->disk_change);
		affs_fix_checksum(sb, AFFS_SB->s_root_bh);
		mark_buffer_dirty(AFFS_SB->s_root_bh);
		sb->s_dirt = !clean;	/* redo until bitmap synced */
	} else
		sb->s_dirt = 0;

	pr_debug("AFFS: write_super() at %lu, clean=%d\n", CURRENT_TIME, clean);
}

static struct super_operations affs_sops = {
	read_inode:	affs_read_inode,
	write_inode:	affs_write_inode,
	put_inode:	affs_put_inode,
	delete_inode:	affs_delete_inode,
	clear_inode:	affs_clear_inode,
	put_super:	affs_put_super,
	write_super:	affs_write_super,
	statfs:		affs_statfs,
	remount_fs:	affs_remount,
};

static int
parse_options(char *options, uid_t *uid, gid_t *gid, int *mode, int *reserved, s32 *root,
		int *blocksize, char **prefix, char *volume, unsigned long *mount_opts)
{
	char	*this_char, *value, *optn;
	int	 f;

	/* Fill in defaults */

	*uid        = current->uid;
	*gid        = current->gid;
	*reserved   = 2;
	*root       = -1;
	*blocksize  = -1;
	volume[0]   = ':';
	volume[1]   = 0;
	*mount_opts = 0;
	if (!options)
		return 1;
	for (this_char = strtok(options,","); this_char; this_char = strtok(NULL,",")) {
		f = 0;
		if ((value = strchr(this_char,'=')) != NULL)
			*value++ = 0;
		if ((optn = "protect") && !strcmp(this_char, optn)) {
			if (value)
				goto out_inv_arg;
			*mount_opts |= SF_IMMUTABLE;
		} else if ((optn = "verbose") && !strcmp(this_char, optn)) {
			if (value)
				goto out_inv_arg;
			*mount_opts |= SF_VERBOSE;
		} else if ((optn = "mufs") && !strcmp(this_char, optn)) {
			if (value)
				goto out_inv_arg;
			*mount_opts |= SF_MUFS;
		} else if ((f = !strcmp(this_char,"setuid")) || !strcmp(this_char,"setgid")) {
			if (value) {
				if (!*value) {
					printk("AFFS: Argument for set[ug]id option missing\n");
					return 0;
				} else {
					(f ? *uid : *gid) = simple_strtoul(value,&value,0);
					if (*value) {
						printk("AFFS: Bad set[ug]id argument\n");
						return 0;
					}
					*mount_opts |= f ? SF_SETUID : SF_SETGID;
				}
			}
		} else if (!strcmp(this_char,"prefix")) {
			optn = "prefix";
			if (!value || !*value)
				goto out_no_arg;
			if (*prefix) {		/* Free any previous prefix */
				kfree(*prefix);
				*prefix = NULL;
			}
			*prefix = kmalloc(strlen(value) + 1,GFP_KERNEL);
			if (!*prefix)
				return 0;
			strcpy(*prefix,value);
			*mount_opts |= SF_PREFIX;
		} else if (!strcmp(this_char,"volume")) {
			optn = "volume";
			if (!value || !*value)
				goto out_no_arg;
			if (strlen(value) > 30)
				value[30] = 0;
			strncpy(volume,value,30);
		} else if (!strcmp(this_char,"mode")) {
			optn = "mode";
			if (!value || !*value)
				goto out_no_arg;
			*mode = simple_strtoul(value,&value,8) & 0777;
			if (*value)
				return 0;
			*mount_opts |= SF_SETMODE;
		} else if (!strcmp(this_char,"reserved")) {
			optn = "reserved";
			if (!value || !*value)
				goto out_no_arg;
			*reserved = simple_strtoul(value,&value,0);
			if (*value)
				return 0;
		} else if (!strcmp(this_char,"root")) {
			optn = "root";
			if (!value || !*value)
				goto out_no_arg;
			*root = simple_strtoul(value,&value,0);
			if (*value)
				return 0;
		} else if (!strcmp(this_char,"bs")) {
			optn = "bs";
			if (!value || !*value)
				goto out_no_arg;
			*blocksize = simple_strtoul(value,&value,0);
			if (*value)
				return 0;
			if (*blocksize != 512 && *blocksize != 1024 && *blocksize != 2048
			    && *blocksize != 4096) {
				printk ("AFFS: Invalid blocksize (512, 1024, 2048, 4096 allowed)\n");
				return 0;
			}
		} else if (!strcmp (this_char, "grpquota")
			 || !strcmp (this_char, "noquota")
			 || !strcmp (this_char, "quota")
			 || !strcmp (this_char, "usrquota"))
			 /* Silently ignore the quota options */
			;
		else {
			printk("AFFS: Unrecognized mount option %s\n", this_char);
			return 0;
		}
	}
	return 1;

out_no_arg:
	printk("AFFS: The %s option requires an argument\n", optn);
	return 0;
out_inv_arg:
	printk("AFFS: Option %s does not take an argument\n", optn);
	return 0;
}

/* This function definitely needs to be split up. Some fine day I'll
 * hopefully have the guts to do so. Until then: sorry for the mess.
 */

static struct super_block *
affs_read_super(struct super_block *sb, void *data, int silent)
{
	struct buffer_head	*root_bh = NULL;
	struct buffer_head	*boot_bh;
	struct inode		*root_inode = NULL;
	kdev_t			 dev = sb->s_dev;
	s32			 root_block;
	int			 blocks, size, blocksize;
	u32			 chksum;
	int			 num_bm;
	int			 i, j;
	s32			 key;
	uid_t			 uid;
	gid_t			 gid;
	int			 reserved;
	unsigned long		 mount_flags;

	pr_debug("AFFS: read_super(%s)\n",data ? (const char *)data : "no options");

	sb->s_magic             = AFFS_SUPER_MAGIC;
	sb->s_op                = &affs_sops;
	memset(AFFS_SB, 0, sizeof(*AFFS_SB));
	init_MUTEX(&AFFS_SB->s_bmlock);

	if (!parse_options(data,&uid,&gid,&i,&reserved,&root_block,
				&blocksize,&AFFS_SB->s_prefix,
				AFFS_SB->s_volume, &mount_flags)) {
		printk(KERN_ERR "AFFS: Error parsing options\n");
		return NULL;
	}
	/* N.B. after this point s_prefix must be released */

	AFFS_SB->s_flags   = mount_flags;
	AFFS_SB->s_mode    = i;
	AFFS_SB->s_uid     = uid;
	AFFS_SB->s_gid     = gid;
	AFFS_SB->s_reserved= reserved;

	/* Get the size of the device in 512-byte blocks.
	 * If we later see that the partition uses bigger
	 * blocks, we will have to change it.
	 */

	blocks = blk_size[MAJOR(dev)] ? blk_size[MAJOR(dev)][MINOR(dev)] : 0;
	if (!blocks) {
		printk(KERN_ERR "AFFS: Could not determine device size\n");
		goto out_error;
	}
	size = (BLOCK_SIZE / 512) * blocks;
	pr_debug("AFFS: initial blksize=%d, blocks=%d\n", 512, blocks);

	affs_set_blocksize(sb, PAGE_SIZE);
	/* Try to find root block. Its location depends on the block size. */

	i = 512;
	j = 4096;
	if (blocksize > 0) {
		i = j = blocksize;
		size = size / (blocksize / 512);
	}
	for (blocksize = i, key = 0; blocksize <= j; blocksize <<= 1, size >>= 1) {
		AFFS_SB->s_root_block = root_block;
		if (root_block < 0)
			AFFS_SB->s_root_block = (reserved + size - 1) / 2;
		pr_debug("AFFS: setting blocksize to %d\n", blocksize);
		affs_set_blocksize(sb, blocksize);
		AFFS_SB->s_partition_size = size;

		/* The root block location that was calculated above is not
		 * correct if the partition size is an odd number of 512-
		 * byte blocks, which will be rounded down to a number of
		 * 1024-byte blocks, and if there were an even number of
		 * reserved blocks. Ideally, all partition checkers should
		 * report the real number of blocks of the real blocksize,
		 * but since this just cannot be done, we have to try to
		 * find the root block anyways. In the above case, it is one
		 * block behind the calculated one. So we check this one, too.
		 */
		for (num_bm = 0; num_bm < 2; num_bm++) {
			pr_debug("AFFS: Dev %s, trying root=%u, bs=%d, "
				"size=%d, reserved=%d\n",
				kdevname(dev),
				AFFS_SB->s_root_block + num_bm,
				blocksize, size, reserved);
			root_bh = affs_bread(sb, AFFS_SB->s_root_block + num_bm);
			if (!root_bh)
				continue;
			if (!affs_checksum_block(sb, root_bh) &&
			    be32_to_cpu(AFFS_ROOT_HEAD(root_bh)->ptype) == T_SHORT &&
			    be32_to_cpu(AFFS_ROOT_TAIL(sb, root_bh)->stype) == ST_ROOT) {
				AFFS_SB->s_hashsize    = blocksize / 4 - 56;
				AFFS_SB->s_root_block += num_bm;
				key                        = 1;
				goto got_root;
			}
			affs_brelse(root_bh);
			root_bh = NULL;
		}
	}
	if (!silent)
		printk(KERN_ERR "AFFS: No valid root block on device %s\n",
			kdevname(dev));
	goto out_error;

	/* N.B. after this point bh must be released */
got_root:
	root_block = AFFS_SB->s_root_block;

	sb->s_blocksize_bits = blocksize == 512 ? 9 :
			       blocksize == 1024 ? 10 :
			       blocksize == 2048 ? 11 : 12;

	/* Find out which kind of FS we have */
	boot_bh = sb_bread(sb, 0);
	if (!boot_bh) {
		printk(KERN_ERR "AFFS: Cannot read boot block\n");
		goto out_error;
	}
	chksum = be32_to_cpu(*(u32 *)boot_bh->b_data);
	brelse(boot_bh);

	/* Dircache filesystems are compatible with non-dircache ones
	 * when reading. As long as they aren't supported, writing is
	 * not recommended.
	 */
	if ((chksum == FS_DCFFS || chksum == MUFS_DCFFS || chksum == FS_DCOFS
	     || chksum == MUFS_DCOFS) && !(sb->s_flags & MS_RDONLY)) {
		printk(KERN_NOTICE "AFFS: Dircache FS - mounting %s read only\n",
			kdevname(dev));
		sb->s_flags |= MS_RDONLY;
		AFFS_SB->s_flags |= SF_READONLY;
	}
	switch (chksum) {
		case MUFS_FS:
		case MUFS_INTLFFS:
		case MUFS_DCFFS:
			AFFS_SB->s_flags |= SF_MUFS;
			/* fall thru */
		case FS_INTLFFS:
		case FS_DCFFS:
			AFFS_SB->s_flags |= SF_INTL;
			break;
		case MUFS_FFS:
			AFFS_SB->s_flags |= SF_MUFS;
			break;
		case FS_FFS:
			break;
		case MUFS_OFS:
			AFFS_SB->s_flags |= SF_MUFS;
			/* fall thru */
		case FS_OFS:
			AFFS_SB->s_flags |= SF_OFS;
			sb->s_flags |= MS_NOEXEC;
			break;
		case MUFS_DCOFS:
		case MUFS_INTLOFS:
			AFFS_SB->s_flags |= SF_MUFS;
		case FS_DCOFS:
		case FS_INTLOFS:
			AFFS_SB->s_flags |= SF_INTL | SF_OFS;
			sb->s_flags |= MS_NOEXEC;
			break;
		default:
			printk(KERN_ERR "AFFS: Unknown filesystem on device %s: %08X\n",
				kdevname(dev), chksum);
			goto out_error;
	}

	if (mount_flags & SF_VERBOSE) {
		chksum = cpu_to_be32(chksum);
		printk(KERN_NOTICE "AFFS: Mounting volume \"%*s\": Type=%.3s\\%c, Blocksize=%d\n",
			AFFS_ROOT_TAIL(sb, root_bh)->disk_name[0],
			AFFS_ROOT_TAIL(sb, root_bh)->disk_name + 1,
			(char *)&chksum,((char *)&chksum)[3] + '0',blocksize);
	}

	sb->s_flags |= MS_NODEV | MS_NOSUID;

	AFFS_SB->s_data_blksize = sb->s_blocksize;
	if (AFFS_SB->s_flags & SF_OFS)
		AFFS_SB->s_data_blksize -= 24;

	/* Keep super block in cache */
	AFFS_SB->s_root_bh = root_bh;
	/* N.B. after this point s_root_bh must be released */

	if (affs_init_bitmap(sb))
		goto out_error;

	/* set up enough so that it can read an inode */

	root_inode = iget(sb, root_block);
	sb->s_root = d_alloc_root(root_inode);
	if (!sb->s_root) {
		printk(KERN_ERR "AFFS: Get root inode failed\n");
		goto out_error;
	}
	sb->s_root->d_op = &affs_dentry_operations;

	pr_debug("AFFS: s_flags=%lX\n",sb->s_flags);
	return sb;

	/*
	 * Begin the cascaded cleanup ...
	 */
out_error:
	if (root_inode)
		iput(root_inode);
	if (AFFS_SB->s_bitmap)
		kfree(AFFS_SB->s_bitmap);
	affs_brelse(root_bh);
	if (AFFS_SB->s_prefix)
		kfree(AFFS_SB->s_prefix);
	return NULL;
}

static int
affs_remount(struct super_block *sb, int *flags, char *data)
{
	int			 blocksize;
	uid_t			 uid;
	gid_t			 gid;
	int			 mode;
	int			 reserved;
	int			 root_block;
	unsigned long		 mount_flags;
	unsigned long		 read_only = AFFS_SB->s_flags & SF_READONLY;

	pr_debug("AFFS: remount(flags=0x%x,opts=\"%s\")\n",*flags,data);

	if (!parse_options(data,&uid,&gid,&mode,&reserved,&root_block,
	    &blocksize,&AFFS_SB->s_prefix,AFFS_SB->s_volume,&mount_flags))
		return -EINVAL;
	AFFS_SB->s_flags = mount_flags | read_only;
	AFFS_SB->s_mode  = mode;
	AFFS_SB->s_uid   = uid;
	AFFS_SB->s_gid   = gid;

	if ((*flags & MS_RDONLY) == (sb->s_flags & MS_RDONLY))
		return 0;
	if (*flags & MS_RDONLY) {
		sb->s_dirt = 1;
		while (sb->s_dirt)
			affs_write_super(sb);
		sb->s_flags |= MS_RDONLY;
	} else if (!(AFFS_SB->s_flags & SF_READONLY)) {
		sb->s_flags &= ~MS_RDONLY;
	} else {
		affs_warning(sb,"remount","Cannot remount fs read/write because of errors");
		return -EINVAL;
	}
	return 0;
}

static int
affs_statfs(struct super_block *sb, struct statfs *buf)
{
	int		 free;

	pr_debug("AFFS: statfs() partsize=%d, reserved=%d\n",AFFS_SB->s_partition_size,
	     AFFS_SB->s_reserved);

	free          = affs_count_free_blocks(sb);
	buf->f_type    = AFFS_SUPER_MAGIC;
	buf->f_bsize   = sb->s_blocksize;
	buf->f_blocks  = AFFS_SB->s_partition_size - AFFS_SB->s_reserved;
	buf->f_bfree   = free;
	buf->f_bavail  = free;
	return 0;
}

static DECLARE_FSTYPE_DEV(affs_fs_type, "affs", affs_read_super);

static int __init init_affs_fs(void)
{
	return register_filesystem(&affs_fs_type);
}

static void __exit exit_affs_fs(void)
{
	unregister_filesystem(&affs_fs_type);
}

EXPORT_NO_SYMBOLS;

MODULE_DESCRIPTION("Amiga filesystem support for Linux");
MODULE_LICENSE("GPL");

module_init(init_affs_fs)
module_exit(exit_affs_fs)
