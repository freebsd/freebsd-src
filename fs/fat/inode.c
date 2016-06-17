/*
 *  linux/fs/fat/inode.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *  VFAT extensions by Gordon Chaffee, merged with msdos fs by Henrik Storner
 *  Rewritten for the constant inumbers support by Al Viro
 *
 *  Fixes:
 *
 *  	Max Cohan: Fixed invalid FSINFO offset when info_sector is 0
 */

#include <linux/module.h>
#include <linux/msdos_fs.h>
#include <linux/nls.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/bitops.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/locks.h>
#include <linux/fat_cvf.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>
#include <asm/unaligned.h>

extern struct cvf_format default_cvf;

/* #define FAT_PARANOIA 1 */
#define DEBUG_LEVEL 0
#ifdef FAT_DEBUG
#  define PRINTK(x) printk x
#else
#  define PRINTK(x)
#endif
#if (DEBUG_LEVEL >= 1)
#  define PRINTK1(x) printk x
#else
#  define PRINTK1(x)
#endif

/*
 * New FAT inode stuff. We do the following:
 *	a) i_ino is constant and has nothing with on-disk location.
 *	b) FAT manages its own cache of directory entries.
 *	c) *This* cache is indexed by on-disk location.
 *	d) inode has an associated directory entry, all right, but
 *		it may be unhashed.
 *	e) currently entries are stored within struct inode. That should
 *		change.
 *	f) we deal with races in the following way:
 *		1. readdir() and lookup() do FAT-dir-cache lookup.
 *		2. rename() unhashes the F-d-c entry and rehashes it in
 *			a new place.
 *		3. unlink() and rmdir() unhash F-d-c entry.
 *		4. fat_write_inode() checks whether the thing is unhashed.
 *			If it is we silently return. If it isn't we do bread(),
 *			check if the location is still valid and retry if it
 *			isn't. Otherwise we do changes.
 *		5. Spinlock is used to protect hash/unhash/location check/lookup
 *		6. fat_clear_inode() unhashes the F-d-c entry.
 *		7. lookup() and readdir() do igrab() if they find a F-d-c entry
 *			and consider negative result as cache miss.
 */

#define FAT_HASH_BITS	8
#define FAT_HASH_SIZE	(1UL << FAT_HASH_BITS)
#define FAT_HASH_MASK	(FAT_HASH_SIZE-1)
static struct list_head fat_inode_hashtable[FAT_HASH_SIZE];
spinlock_t fat_inode_lock = SPIN_LOCK_UNLOCKED;

void fat_hash_init(void)
{
	int i;
	for(i = 0; i < FAT_HASH_SIZE; i++) {
		INIT_LIST_HEAD(&fat_inode_hashtable[i]);
	}
}

static inline unsigned long fat_hash(struct super_block *sb, loff_t i_pos)
{
	unsigned long tmp = (unsigned long)i_pos | (unsigned long) sb;
	tmp = tmp + (tmp >> FAT_HASH_BITS) + (tmp >> FAT_HASH_BITS * 2);
	return tmp & FAT_HASH_MASK;
}

void fat_attach(struct inode *inode, loff_t i_pos)
{
	spin_lock(&fat_inode_lock);
	MSDOS_I(inode)->i_pos = i_pos;
	list_add(&MSDOS_I(inode)->i_fat_hash,
		fat_inode_hashtable + fat_hash(inode->i_sb, i_pos));
	spin_unlock(&fat_inode_lock);
}

void fat_detach(struct inode *inode)
{
	spin_lock(&fat_inode_lock);
	MSDOS_I(inode)->i_pos = 0;
	list_del(&MSDOS_I(inode)->i_fat_hash);
	INIT_LIST_HEAD(&MSDOS_I(inode)->i_fat_hash);
	spin_unlock(&fat_inode_lock);
}

struct inode *fat_iget(struct super_block *sb, loff_t i_pos)
{
	struct list_head *p = fat_inode_hashtable + fat_hash(sb, i_pos);
	struct list_head *walk;
	struct msdos_inode_info *i;
	struct inode *inode = NULL;

	spin_lock(&fat_inode_lock);
	list_for_each(walk, p) {
		i = list_entry(walk, struct msdos_inode_info, i_fat_hash);
		if (i->i_fat_inode->i_sb != sb)
			continue;
		if (i->i_pos != i_pos)
			continue;
		inode = igrab(i->i_fat_inode);
		if (inode)
			break;
	}
	spin_unlock(&fat_inode_lock);
	return inode;
}

static void fat_fill_inode(struct inode *inode, struct msdos_dir_entry *de);

struct inode *fat_build_inode(struct super_block *sb,
			struct msdos_dir_entry *de, loff_t i_pos, int *res)
{
	struct inode *inode;
	*res = 0;
	inode = fat_iget(sb, i_pos);
	if (inode)
		goto out;
	inode = new_inode(sb);
	*res = -ENOMEM;
	if (!inode)
		goto out;
	*res = 0;
	inode->i_ino = iunique(sb, MSDOS_ROOT_INO);
	fat_fill_inode(inode, de);
	fat_attach(inode, i_pos);
	insert_inode_hash(inode);
out:
	return inode;
}

void fat_delete_inode(struct inode *inode)
{
	if (!is_bad_inode(inode)) {
		lock_kernel();
		inode->i_size = 0;
		fat_truncate(inode);
		unlock_kernel();
	}
	clear_inode(inode);
}

void fat_clear_inode(struct inode *inode)
{
	if (is_bad_inode(inode))
		return;
	lock_kernel();
	spin_lock(&fat_inode_lock);
	fat_cache_inval_inode(inode);
	list_del(&MSDOS_I(inode)->i_fat_hash);
	spin_unlock(&fat_inode_lock);
	unlock_kernel();
}

void fat_put_super(struct super_block *sb)
{
	if (MSDOS_SB(sb)->cvf_format->cvf_version) {
		dec_cvf_format_use_count_by_version(MSDOS_SB(sb)->cvf_format->cvf_version);
		MSDOS_SB(sb)->cvf_format->unmount_cvf(sb);
	}
	if (MSDOS_SB(sb)->fat_bits == 32) {
		fat_clusters_flush(sb);
	}
	fat_cache_inval_dev(sb->s_dev);
	set_blocksize (sb->s_dev,BLOCK_SIZE);
	if (MSDOS_SB(sb)->nls_disk) {
		unload_nls(MSDOS_SB(sb)->nls_disk);
		MSDOS_SB(sb)->nls_disk = NULL;
		MSDOS_SB(sb)->options.codepage = 0;
	}
	if (MSDOS_SB(sb)->nls_io) {
		unload_nls(MSDOS_SB(sb)->nls_io);
		MSDOS_SB(sb)->nls_io = NULL;
	}
	/*
	 * Note: the iocharset option might have been specified
	 * without enabling nls_io, so check for it here.
	 */
	if (MSDOS_SB(sb)->options.iocharset) {
		kfree(MSDOS_SB(sb)->options.iocharset);
		MSDOS_SB(sb)->options.iocharset = NULL;
	}
}


static int parse_options(char *options,int *fat, int *debug,
			 struct fat_mount_options *opts,
			 char *cvf_format, char *cvf_options)
{
	char *this_char,*value,save,*savep;
	char *p;
	int ret = 1, len;

	opts->name_check = 'n';
	opts->conversion = 'b';
	opts->fs_uid = current->uid;
	opts->fs_gid = current->gid;
	opts->fs_umask = current->fs->umask;
	opts->quiet = opts->sys_immutable = opts->dotsOK = opts->showexec = 0;
	opts->codepage = 0;
	opts->nocase = 0;
	opts->shortname = 0;
	opts->utf8 = 0;
	opts->iocharset = NULL;
	*debug = *fat = 0;

	if (!options)
		goto out;
	save = 0;
	savep = NULL;
	for (this_char = strtok(options,","); this_char;
	     this_char = strtok(NULL,",")) {
		if ((value = strchr(this_char,'=')) != NULL) {
			save = *value;
			savep = value;
			*value++ = 0;
		}
		if (!strcmp(this_char,"check") && value) {
			if (value[0] && !value[1] && strchr("rns",*value))
				opts->name_check = *value;
			else if (!strcmp(value,"relaxed"))
				opts->name_check = 'r';
			else if (!strcmp(value,"normal"))
				opts->name_check = 'n';
			else if (!strcmp(value,"strict"))
				opts->name_check = 's';
			else ret = 0;
		}
		else if (!strcmp(this_char,"conv") && value) {
			if (value[0] && !value[1] && strchr("bta",*value))
				opts->conversion = *value;
			else if (!strcmp(value,"binary"))
				opts->conversion = 'b';
			else if (!strcmp(value,"text"))
				opts->conversion = 't';
			else if (!strcmp(value,"auto"))
				opts->conversion = 'a';
			else ret = 0;
		}
		else if (!strcmp(this_char,"dots")) {
			opts->dotsOK = 1;
		}
		else if (!strcmp(this_char,"nocase")) {
			opts->nocase = 1;
		}
		else if (!strcmp(this_char,"nodots")) {
			opts->dotsOK = 0;
		}
		else if (!strcmp(this_char,"showexec")) {
			opts->showexec = 1;
		}
		else if (!strcmp(this_char,"dotsOK") && value) {
			if (!strcmp(value,"yes")) opts->dotsOK = 1;
			else if (!strcmp(value,"no")) opts->dotsOK = 0;
			else ret = 0;
		}
		else if (!strcmp(this_char,"uid")) {
			if (!value || !*value) ret = 0;
			else {
				opts->fs_uid = simple_strtoul(value,&value,0);
				if (*value) ret = 0;
			}
		}
		else if (!strcmp(this_char,"gid")) {
			if (!value || !*value) ret= 0;
			else {
				opts->fs_gid = simple_strtoul(value,&value,0);
				if (*value) ret = 0;
			}
		}
		else if (!strcmp(this_char,"umask")) {
			if (!value || !*value) ret = 0;
			else {
				opts->fs_umask = simple_strtoul(value,&value,8);
				if (*value) ret = 0;
			}
		}
		else if (!strcmp(this_char,"debug")) {
			if (value) ret = 0;
			else *debug = 1;
		}
		else if (!strcmp(this_char,"fat")) {
			if (!value || !*value) ret = 0;
			else {
				*fat = simple_strtoul(value,&value,0);
				if (*value || (*fat != 12 && *fat != 16 &&
					       *fat != 32)) 
					ret = 0;
			}
		}
		else if (!strcmp(this_char,"quiet")) {
			if (value) ret = 0;
			else opts->quiet = 1;
		}
		else if (!strcmp(this_char,"blocksize")) {
			printk("FAT: blocksize option is obsolete, "
			       "not supported now\n");
		}
		else if (!strcmp(this_char,"sys_immutable")) {
			if (value) ret = 0;
			else opts->sys_immutable = 1;
		}
		else if (!strcmp(this_char,"codepage") && value) {
			opts->codepage = simple_strtoul(value,&value,0);
			if (*value) ret = 0;
			else printk ("MSDOS FS: Using codepage %d\n",
					opts->codepage);
		}
		else if (!strcmp(this_char,"iocharset") && value) {
			p = value;
			while (*value && *value != ',')
				value++;
			len = value - p;
			if (len) {
				char *buffer;

				if (opts->iocharset != NULL) {
					kfree(opts->iocharset);
					opts->iocharset = NULL;
				}
				buffer = kmalloc(len + 1, GFP_KERNEL);
				if (buffer != NULL) {
					opts->iocharset = buffer;
					memcpy(buffer, p, len);
					buffer[len] = 0;
					printk("MSDOS FS: IO charset %s\n", buffer);
				} else
					ret = 0;
			}
		}
		else if (!strcmp(this_char,"cvf_format")) {
			if (!value)
				return 0;
			strncpy(cvf_format,value,20);
		}
		else if (!strcmp(this_char,"cvf_options")) {
			if (!value)
				return 0;
			strncpy(cvf_options,value,100);
		}

		if (this_char != options) *(this_char-1) = ',';
		if (value) *savep = save;
		if (ret == 0)
			break;
	}
out:
	return ret;
}

static void fat_read_root(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct msdos_sb_info *sbi = MSDOS_SB(sb);
	int nr;

	INIT_LIST_HEAD(&MSDOS_I(inode)->i_fat_hash);
	MSDOS_I(inode)->i_pos = 0;
	MSDOS_I(inode)->i_fat_inode = inode;
	inode->i_uid = sbi->options.fs_uid;
	inode->i_gid = sbi->options.fs_gid;
	inode->i_version = ++event;
	inode->i_generation = 0;
	inode->i_mode = (S_IRWXUGO & ~sbi->options.fs_umask) | S_IFDIR;
	inode->i_op = sbi->dir_ops;
	inode->i_fop = &fat_dir_operations;
	if (sbi->fat_bits == 32) {
		MSDOS_I(inode)->i_start = sbi->root_cluster;
		if ((nr = MSDOS_I(inode)->i_start) != 0) {
			while (nr != -1) {
				inode->i_size += 1 << sbi->cluster_bits;
				if (!(nr = fat_access(sb, nr, -1))) {
					printk("Directory %ld: bad FAT\n",
					       inode->i_ino);
					break;
				}
			}
		}
	} else {
		MSDOS_I(inode)->i_start = 0;
		inode->i_size = sbi->dir_entries * sizeof(struct msdos_dir_entry);
	}
	inode->i_blksize = 1 << sbi->cluster_bits;
	inode->i_blocks = ((inode->i_size + inode->i_blksize - 1)
			   & ~(inode->i_blksize - 1)) >> 9;
	MSDOS_I(inode)->i_logstart = 0;
	MSDOS_I(inode)->mmu_private = inode->i_size;

	MSDOS_I(inode)->i_attrs = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = 0;
	MSDOS_I(inode)->i_ctime_ms = 0;
	inode->i_nlink = fat_subdirs(inode)+2;
}

/*
 * a FAT file handle with fhtype 3 is
 *  0/  i_ino - for fast, reliable lookup if still in the cache
 *  1/  i_generation - to see if i_ino is still valid
 *          bit 0 == 0 iff directory
 *  2/  i_pos(8-39) - if ino has changed, but still in cache
 *  3/  i_pos(4-7)|i_logstart - to semi-verify inode found at i_pos
 *  4/  i_pos(0-3)|parent->i_logstart - maybe used to hunt for the file on disc
 *
 * Hack for NFSv2: Maximum FAT entry number is 28bits and maximum
 * i_pos is 40bits (blocknr(32) + dir offset(8)), so two 4bits
 * of i_logstart is used to store the directory entry offset.
 */
struct dentry *fat_fh_to_dentry(struct super_block *sb, __u32 *fh,
				int len, int fhtype, int parent)
{
	struct inode *inode = NULL;
	struct list_head *lp;
	struct dentry *result;

	if (fhtype != 3)
		return ERR_PTR(-ESTALE);
	if (len < 5)
		return ERR_PTR(-ESTALE);
	/* We cannot find the parent,
	   It better just *be* there */
	if (parent)
		return ERR_PTR(-ESTALE);

	inode = iget(sb, fh[0]);
	if (!inode || is_bad_inode(inode) || inode->i_generation != fh[1]) {
		if (inode)
			iput(inode);
		inode = NULL;
	}
	if (!inode) {
		loff_t i_pos;
		int i_logstart = fh[3] & 0x0fffffff;

		i_pos = (loff_t)fh[2] << 8;
		i_pos |= ((fh[3] >> 24) & 0xf0) | (fh[4] >> 28);

		/* try 2 - see if i_pos is in F-d-c
		 * require i_logstart to be the same
		 * Will fail if you truncate and then re-write
		 */

		inode = fat_iget(sb, i_pos);
		if (inode && MSDOS_I(inode)->i_logstart != i_logstart) {
			iput(inode);
			inode = NULL;
		}
	}
	if (!inode) {
		/* For now, do nothing
		 * What we could do is:
		 * follow the file starting at fh[4], and record
		 * the ".." entry, and the name of the fh[2] entry.
		 * The follow the ".." file finding the next step up.
		 * This way we build a path to the root of
		 * the tree. If this works, we lookup the path and so
		 * get this inode into the cache.
		 * Finally try the fat_iget lookup again
		 * If that fails, then weare totally out of luck
		 * But all that is for another day
		 */
	}
	if (!inode)
		return ERR_PTR(-ESTALE);

	
	/* now to find a dentry.
	 * If possible, get a well-connected one
	 *
	 * Given the way that we found the inode, it *MUST* be
	 * well-connected, but it is easiest to just copy the
	 * code.
	 */
	spin_lock(&dcache_lock);
	for (lp = inode->i_dentry.next; lp != &inode->i_dentry ; lp=lp->next) {
		result = list_entry(lp,struct dentry, d_alias);
		if (! (result->d_flags & DCACHE_NFSD_DISCONNECTED)) {
			dget_locked(result);
			result->d_vfs_flags |= DCACHE_REFERENCED;
			spin_unlock(&dcache_lock);
			iput(inode);
			return result;
		}
	}
	spin_unlock(&dcache_lock);
	result = d_alloc_root(inode);
	if (result == NULL) {
		iput(inode);
		return ERR_PTR(-ENOMEM);
	}
	result->d_op = sb->s_root->d_op;
	result->d_flags |= DCACHE_NFSD_DISCONNECTED;
	return result;
}

int fat_dentry_to_fh(struct dentry *de, __u32 *fh, int *lenp, int needparent)
{
	int len = *lenp;
	struct inode *inode =  de->d_inode;
	u32 ipos_h, ipos_m, ipos_l;
	
	if (len < 5)
		return 255; /* no room */

	ipos_h = MSDOS_I(inode)->i_pos >> 8;
	ipos_m = (MSDOS_I(inode)->i_pos & 0xf0) << 24;
	ipos_l = (MSDOS_I(inode)->i_pos & 0x0f) << 28;
	*lenp = 5;
	fh[0] = inode->i_ino;
	fh[1] = inode->i_generation;
	fh[2] = ipos_h;
	fh[3] = ipos_m | MSDOS_I(inode)->i_logstart;
	fh[4] = ipos_l | MSDOS_I(de->d_parent->d_inode)->i_logstart;
	return 3;
}

static struct super_operations fat_sops = { 
	write_inode:	fat_write_inode,
	delete_inode:	fat_delete_inode,
	put_super:	fat_put_super,
	statfs:		fat_statfs,
	clear_inode:	fat_clear_inode,

	read_inode:	make_bad_inode,
	fh_to_dentry:	fat_fh_to_dentry,
	dentry_to_fh:	fat_dentry_to_fh,
};

/*
 * Read the super block of an MS-DOS FS.
 *
 * Note that this may be called from vfat_read_super
 * with some fields already initialized.
 */
struct super_block *
fat_read_super(struct super_block *sb, void *data, int silent,
		struct inode_operations *fs_dir_inode_ops)
{
	struct inode *root_inode;
	struct buffer_head *bh;
	struct fat_boot_sector *b;
	struct msdos_sb_info *sbi = MSDOS_SB(sb);
	char *p;
	int logical_sector_size, hard_blksize, fat_clusters = 0;
	unsigned int total_sectors, rootdir_sectors;
	int fat32, debug, error, fat, cp;
	struct fat_mount_options opts;
	char buf[50];
	int i;
	char cvf_format[21];
	char cvf_options[101];

	cvf_format[0] = '\0';
	cvf_options[0] = '\0';
	sbi->cvf_format = NULL;
	sbi->private_data = NULL;

	sbi->dir_ops = fs_dir_inode_ops;

	sb->s_maxbytes = MAX_NON_LFS;
	sb->s_op = &fat_sops;

	hard_blksize = get_hardsect_size(sb->s_dev);
	if (!hard_blksize)
		hard_blksize = 512;

	opts.isvfat = sbi->options.isvfat;
	if (!parse_options((char *) data, &fat, &debug, &opts,
			   cvf_format, cvf_options))
		goto out_fail;
	/* N.B. we should parse directly into the sb structure */
	memcpy(&(sbi->options), &opts, sizeof(struct fat_mount_options));

	fat_cache_init();

	sb->s_blocksize = hard_blksize;
	set_blocksize(sb->s_dev, hard_blksize);
	bh = sb_bread(sb, 0);
	if (bh == NULL) {
		printk("FAT: unable to read boot sector\n");
		goto out_fail;
	}

/*
 * The DOS3 partition size limit is *not* 32M as many people think.  
 * Instead, it is 64K sectors (with the usual sector size being
 * 512 bytes, leading to a 32M limit).
 * 
 * DOS 3 partition managers got around this problem by faking a 
 * larger sector size, ie treating multiple physical sectors as 
 * a single logical sector.
 * 
 * We can accommodate this scheme by adjusting our cluster size,
 * fat_start, and data_start by an appropriate value.
 *
 * (by Drew Eckhardt)
 */


	b = (struct fat_boot_sector *) bh->b_data;
	logical_sector_size =
		CF_LE_W(get_unaligned((unsigned short *) &b->sector_size));
	if (!logical_sector_size
	    || (logical_sector_size & (logical_sector_size - 1))) {
		printk("FAT: bogus logical sector size %d\n",
		       logical_sector_size);
		brelse(bh);
		goto out_invalid;
	}

	sbi->cluster_size = b->cluster_size;
	if (!sbi->cluster_size
	    || (sbi->cluster_size & (sbi->cluster_size - 1))) {
		printk("FAT: bogus cluster size %d\n", sbi->cluster_size);
		brelse(bh);
		goto out_invalid;
	}

	if (logical_sector_size < hard_blksize) {
		printk("FAT: logical sector size too small for device"
		       " (logical sector size = %d)\n", logical_sector_size);
		brelse(bh);
		goto out_invalid;
	}

	sbi->cluster_bits = ffs(logical_sector_size * sbi->cluster_size) - 1;
	sbi->fats = b->fats;
	sbi->fat_start = CF_LE_W(b->reserved);
	sbi->prev_free = 0;
	if (!b->fat_length && b->fat32_length) {
		struct fat_boot_fsinfo *fsinfo;
		struct buffer_head *fsinfo_bh;
		int fsinfo_block, fsinfo_offset;

		/* Must be FAT32 */
		fat32 = 1;
		sbi->fat_length = CF_LE_L(b->fat32_length);
		sbi->root_cluster = CF_LE_L(b->root_cluster);

		sbi->fsinfo_sector = CF_LE_W(b->info_sector);
		/* MC - if info_sector is 0, don't multiply by 0 */
		if (sbi->fsinfo_sector == 0)
			sbi->fsinfo_sector = 1;

		fsinfo_block =
			(sbi->fsinfo_sector * logical_sector_size) / hard_blksize;
		fsinfo_offset =
			(sbi->fsinfo_sector * logical_sector_size) % hard_blksize;
		fsinfo_bh = bh;
		if (fsinfo_block != 0) {
			fsinfo_bh = sb_bread(sb, fsinfo_block);
			if (fsinfo_bh == NULL) {
				printk("FAT: bread failed, FSINFO block"
				       " (blocknr = %d)\n", fsinfo_block);
				brelse(bh);
				goto out_invalid;
			}
		}
		fsinfo = (struct fat_boot_fsinfo *)&fsinfo_bh->b_data[fsinfo_offset];
		if (!IS_FSINFO(fsinfo)) {
			printk("FAT: Did not find valid FSINFO signature.\n"
			       "Found signature1 0x%x signature2 0x%x sector=%ld.\n",
			       CF_LE_L(fsinfo->signature1),
			       CF_LE_L(fsinfo->signature2),
			       sbi->fsinfo_sector);
		} else {
			sbi->free_clusters = CF_LE_L(fsinfo->free_clusters);
			sbi->prev_free = CF_LE_L(fsinfo->next_cluster);
		}

		if (fsinfo_block != 0)
			brelse(fsinfo_bh);
	} else {
		fat32 = 0;
		sbi->fat_length = CF_LE_W(b->fat_length);
		sbi->root_cluster = 0;
		sbi->free_clusters = -1; /* Don't know yet */
	}

	sbi->dir_per_block = logical_sector_size / sizeof(struct msdos_dir_entry);
	sbi->dir_per_block_bits = ffs(sbi->dir_per_block) - 1;

	sbi->dir_start = sbi->fat_start + sbi->fats * sbi->fat_length;
	sbi->dir_entries =
		CF_LE_W(get_unaligned((unsigned short *)&b->dir_entries));
	rootdir_sectors = sbi->dir_entries
		* sizeof(struct msdos_dir_entry) / logical_sector_size;
	sbi->data_start = sbi->dir_start + rootdir_sectors;
	total_sectors = CF_LE_W(get_unaligned((unsigned short *)&b->sectors));
	if (total_sectors == 0)
		total_sectors = CF_LE_L(b->total_sect);
	sbi->clusters = (total_sectors - sbi->data_start) / sbi->cluster_size;

	error = 0;
	if (!error) {
		sbi->fat_bits = fat32 ? 32 :
			(fat ? fat :
			 (sbi->clusters > MSDOS_FAT12 ? 16 : 12));
		fat_clusters =
			sbi->fat_length * logical_sector_size * 8 / sbi->fat_bits;
		error = !sbi->fats || (sbi->dir_entries & (sbi->dir_per_block - 1))
			|| sbi->clusters + 2 > fat_clusters + MSDOS_MAX_EXTRA
			|| logical_sector_size < 512
			|| PAGE_CACHE_SIZE < logical_sector_size
			|| !b->secs_track || !b->heads;
	}
	brelse(bh);

	if (error)
		goto out_invalid;

	sb->s_blocksize = logical_sector_size;
	sb->s_blocksize_bits = ffs(logical_sector_size) - 1;
	set_blocksize(sb->s_dev, sb->s_blocksize);
	sbi->cvf_format = &default_cvf;
	if (!strcmp(cvf_format, "none"))
		i = -1;
	else
		i = detect_cvf(sb,cvf_format);
	if (i >= 0)
		error = cvf_formats[i]->mount_cvf(sb, cvf_options);
	if (error || debug) {
		/* The MSDOS_CAN_BMAP is obsolete, but left just to remember */
		printk("[MS-DOS FS Rel. 12,FAT %d,check=%c,conv=%c,"
		       "uid=%d,gid=%d,umask=%03o%s]\n",
		       sbi->fat_bits,opts.name_check,
		       opts.conversion,opts.fs_uid,opts.fs_gid,opts.fs_umask,
		       MSDOS_CAN_BMAP(sbi) ? ",bmap" : "");
		printk("[me=0x%x,cs=%d,#f=%d,fs=%d,fl=%ld,ds=%ld,de=%d,data=%ld,"
		       "se=%u,ts=%u,ls=%d,rc=%ld,fc=%u]\n",
		       b->media, sbi->cluster_size, sbi->fats,
		       sbi->fat_start, sbi->fat_length, sbi->dir_start,
		       sbi->dir_entries, sbi->data_start,
		       CF_LE_W(get_unaligned((unsigned short *)&b->sectors)),
		       CF_LE_L(b->total_sect), logical_sector_size,
		       sbi->root_cluster, sbi->free_clusters);
		printk ("hard sector size = %d\n", hard_blksize);
	}
	if (i < 0)
		if (sbi->clusters + 2 > fat_clusters)
			sbi->clusters = fat_clusters - 2;
	if (error)
		goto out_invalid;

	sb->s_magic = MSDOS_SUPER_MAGIC;
	/* set up enough so that it can read an inode */
	init_MUTEX(&sbi->fat_lock);

	cp = opts.codepage ? opts.codepage : 437;
	sprintf(buf, "cp%d", cp);
	sbi->nls_disk = load_nls(buf);
	if (! sbi->nls_disk) {
		/* Fail only if explicit charset specified */
		if (opts.codepage != 0)
			goto out_fail;
		sbi->options.codepage = 0; /* already 0?? */
		sbi->nls_disk = load_nls_default();
	}

	sbi->nls_io = NULL;
	if (sbi->options.isvfat && !opts.utf8) {
		p = opts.iocharset ? opts.iocharset : CONFIG_NLS_DEFAULT;
		sbi->nls_io = load_nls(p);
		if (! sbi->nls_io)
			/* Fail only if explicit charset specified */
			if (opts.iocharset)
				goto out_unload_nls;
	}
	if (! sbi->nls_io)
		sbi->nls_io = load_nls_default();

	root_inode = new_inode(sb);
	if (!root_inode)
		goto out_unload_nls;
	root_inode->i_ino = MSDOS_ROOT_INO;
	fat_read_root(root_inode);
	insert_inode_hash(root_inode);
	sb->s_root = d_alloc_root(root_inode);
	if (!sb->s_root)
		goto out_no_root;
	if(i >= 0) {
		sbi->cvf_format = cvf_formats[i];
		++cvf_format_use_count[i];
	}
	return sb;

out_no_root:
	printk("FAT: get root inode failed\n");
	iput(root_inode);
	unload_nls(sbi->nls_io);
out_unload_nls:
	unload_nls(sbi->nls_disk);
	goto out_fail;
out_invalid:
	if (!silent) {
		printk("VFS: Can't find a valid FAT filesystem on dev %s.\n",
			kdevname(sb->s_dev));
	}
out_fail:
	if (opts.iocharset) {
		printk("FAT: freeing iocharset=%s\n", opts.iocharset);
		kfree(opts.iocharset);
	}
	if(sbi->private_data)
		kfree(sbi->private_data);
	sbi->private_data = NULL;
 
	return NULL;
}

int fat_statfs(struct super_block *sb,struct statfs *buf)
{
	int free,nr;
       
	if (MSDOS_SB(sb)->cvf_format &&
	    MSDOS_SB(sb)->cvf_format->cvf_statfs)
		return MSDOS_SB(sb)->cvf_format->cvf_statfs(sb,buf,
						sizeof(struct statfs));
	  
	lock_fat(sb);
	if (MSDOS_SB(sb)->free_clusters != -1)
		free = MSDOS_SB(sb)->free_clusters;
	else {
		free = 0;
		for (nr = 2; nr < MSDOS_SB(sb)->clusters+2; nr++)
			if (!fat_access(sb,nr,-1)) free++;
		MSDOS_SB(sb)->free_clusters = free;
	}
	unlock_fat(sb);
	buf->f_type = sb->s_magic;
	buf->f_bsize = 1 << MSDOS_SB(sb)->cluster_bits;
	buf->f_blocks = MSDOS_SB(sb)->clusters;
	buf->f_bfree = free;
	buf->f_bavail = free;
	buf->f_namelen = MSDOS_SB(sb)->options.isvfat ? 260 : 12;
	return 0;
}

static int is_exec(char *extension)
{
	char *exe_extensions = "EXECOMBAT", *walk;

	for (walk = exe_extensions; *walk; walk += 3)
		if (!strncmp(extension, walk, 3))
			return 1;
	return 0;
}

static int fat_writepage(struct page *page)
{
	return block_write_full_page(page,fat_get_block);
}
static int fat_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page,fat_get_block);
}
static int fat_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return cont_prepare_write(page,from,to,fat_get_block,
		&MSDOS_I(page->mapping->host)->mmu_private);
}
static int _fat_bmap(struct address_space *mapping, long block)
{
	return generic_block_bmap(mapping,block,fat_get_block);
}
static struct address_space_operations fat_aops = {
	readpage: fat_readpage,
	writepage: fat_writepage,
	sync_page: block_sync_page,
	prepare_write: fat_prepare_write,
	commit_write: generic_commit_write,
	bmap: _fat_bmap
};

/* doesn't deal with root inode */
static void fat_fill_inode(struct inode *inode, struct msdos_dir_entry *de)
{
	struct super_block *sb = inode->i_sb;
	struct msdos_sb_info *sbi = MSDOS_SB(sb);
	int nr;

	INIT_LIST_HEAD(&MSDOS_I(inode)->i_fat_hash);
	MSDOS_I(inode)->i_pos = 0;
	MSDOS_I(inode)->i_fat_inode = inode;
	inode->i_uid = sbi->options.fs_uid;
	inode->i_gid = sbi->options.fs_gid;
	inode->i_version = ++event;
	inode->i_generation = CURRENT_TIME;
	
	if ((de->attr & ATTR_DIR) && !IS_FREE(de->name)) {
		inode->i_generation &= ~1;
		inode->i_mode = MSDOS_MKMODE(de->attr,S_IRWXUGO &
		    ~sbi->options.fs_umask) | S_IFDIR;
		inode->i_op = sbi->dir_ops;
		inode->i_fop = &fat_dir_operations;

		MSDOS_I(inode)->i_start = CF_LE_W(de->start);
		if (sbi->fat_bits == 32)
			MSDOS_I(inode)->i_start |= (CF_LE_W(de->starthi) << 16);

		MSDOS_I(inode)->i_logstart = MSDOS_I(inode)->i_start;
		inode->i_nlink = fat_subdirs(inode);
		    /* includes .., compensating for "self" */
#ifdef DEBUG
		if (!inode->i_nlink) {
			printk("directory %d: i_nlink == 0\n",inode->i_ino);
			inode->i_nlink = 1;
		}
#endif
		if ((nr = MSDOS_I(inode)->i_start) != 0)
			while (nr != -1) {
				inode->i_size += 1 << sbi->cluster_bits;
				if (!(nr = fat_access(sb, nr, -1))) {
					printk("Directory %ld: bad FAT\n",
					    inode->i_ino);
					break;
				}
			}
		MSDOS_I(inode)->mmu_private = inode->i_size;
	} else { /* not a directory */
		inode->i_generation |= 1;
		inode->i_mode = MSDOS_MKMODE(de->attr,
		    ((sbi->options.showexec &&
		       !is_exec(de->ext))
		    	? S_IRUGO|S_IWUGO : S_IRWXUGO)
		    & ~sbi->options.fs_umask) | S_IFREG;
		MSDOS_I(inode)->i_start = CF_LE_W(de->start);
		if (sbi->fat_bits == 32)
			MSDOS_I(inode)->i_start |= (CF_LE_W(de->starthi) << 16);

		MSDOS_I(inode)->i_logstart = MSDOS_I(inode)->i_start;
		inode->i_size = CF_LE_L(de->size);
	        inode->i_op = &fat_file_inode_operations;
	        inode->i_fop = &fat_file_operations;
		inode->i_mapping->a_ops = &fat_aops;
		MSDOS_I(inode)->mmu_private = inode->i_size;
	}
	if(de->attr & ATTR_SYS)
		if (sbi->options.sys_immutable)
			inode->i_flags |= S_IMMUTABLE;
	MSDOS_I(inode)->i_attrs = de->attr & ATTR_UNUSED;
	/* this is as close to the truth as we can get ... */
	inode->i_blksize = 1 << sbi->cluster_bits;
	inode->i_blocks = ((inode->i_size + inode->i_blksize - 1)
			   & ~(inode->i_blksize - 1)) >> 9;
	inode->i_mtime = inode->i_atime =
		date_dos2unix(CF_LE_W(de->time),CF_LE_W(de->date));
	inode->i_ctime =
		MSDOS_SB(sb)->options.isvfat
		? date_dos2unix(CF_LE_W(de->ctime),CF_LE_W(de->cdate))
		: inode->i_mtime;
	MSDOS_I(inode)->i_ctime_ms = de->ctime_ms;
}

void fat_write_inode(struct inode *inode, int wait)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head *bh;
	struct msdos_dir_entry *raw_entry;
	loff_t i_pos;

retry:
	i_pos = MSDOS_I(inode)->i_pos;
	if (inode->i_ino == MSDOS_ROOT_INO || !i_pos) {
		return;
	}
	lock_kernel();
	if (!(bh = fat_bread(sb, i_pos >> MSDOS_SB(sb)->dir_per_block_bits))) {
		printk("dev = %s, i_pos = %llu\n", kdevname(inode->i_dev), i_pos);
		fat_fs_panic(sb, "msdos_write_inode: unable to read i-node block");
		unlock_kernel();
		return;
	}
	spin_lock(&fat_inode_lock);
	if (i_pos != MSDOS_I(inode)->i_pos) {
		spin_unlock(&fat_inode_lock);
		fat_brelse(sb, bh);
		unlock_kernel();
		goto retry;
	}

	raw_entry = &((struct msdos_dir_entry *) (bh->b_data))
	    [i_pos & (MSDOS_SB(sb)->dir_per_block - 1)];
	if (S_ISDIR(inode->i_mode)) {
		raw_entry->attr = ATTR_DIR;
		raw_entry->size = 0;
	}
	else {
		raw_entry->attr = ATTR_NONE;
		raw_entry->size = CT_LE_L(inode->i_size);
	}
	raw_entry->attr |= MSDOS_MKATTR(inode->i_mode) |
	    MSDOS_I(inode)->i_attrs;
	raw_entry->start = CT_LE_W(MSDOS_I(inode)->i_logstart);
	raw_entry->starthi = CT_LE_W(MSDOS_I(inode)->i_logstart >> 16);
	fat_date_unix2dos(inode->i_mtime,&raw_entry->time,&raw_entry->date);
	raw_entry->time = CT_LE_W(raw_entry->time);
	raw_entry->date = CT_LE_W(raw_entry->date);
	if (MSDOS_SB(sb)->options.isvfat) {
		fat_date_unix2dos(inode->i_ctime,&raw_entry->ctime,&raw_entry->cdate);
		raw_entry->ctime_ms = MSDOS_I(inode)->i_ctime_ms;
		raw_entry->ctime = CT_LE_W(raw_entry->ctime);
		raw_entry->cdate = CT_LE_W(raw_entry->cdate);
	}
	spin_unlock(&fat_inode_lock);
	fat_mark_buffer_dirty(sb, bh);
	fat_brelse(sb, bh);
	unlock_kernel();
}


int fat_notify_change(struct dentry * dentry, struct iattr * attr)
{
	struct super_block *sb = dentry->d_sb;
	struct inode *inode = dentry->d_inode;
	int error;

	/* FAT cannot truncate to a longer file */
	if (attr->ia_valid & ATTR_SIZE) {
		if (attr->ia_size > inode->i_size)
			return -EPERM;
	}

	error = inode_change_ok(inode, attr);
	if (error)
		return MSDOS_SB(sb)->options.quiet ? 0 : error;

	if (((attr->ia_valid & ATTR_UID) && 
	     (attr->ia_uid != MSDOS_SB(sb)->options.fs_uid)) ||
	    ((attr->ia_valid & ATTR_GID) && 
	     (attr->ia_gid != MSDOS_SB(sb)->options.fs_gid)) ||
	    ((attr->ia_valid & ATTR_MODE) &&
	     (attr->ia_mode & ~MSDOS_VALID_MODE)))
		error = -EPERM;

	if (error)
		return MSDOS_SB(sb)->options.quiet ? 0 : error;

	error = inode_setattr(inode, attr);
	if (error)
		return error;

	if (S_ISDIR(inode->i_mode))
		inode->i_mode |= S_IXUGO;

	inode->i_mode = ((inode->i_mode & S_IFMT) | ((((inode->i_mode & S_IRWXU
	    & ~MSDOS_SB(sb)->options.fs_umask) | S_IRUSR) >> 6)*S_IXUGO)) &
	    ~MSDOS_SB(sb)->options.fs_umask;
	return 0;
}
MODULE_LICENSE("GPL");
