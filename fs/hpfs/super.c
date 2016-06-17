/*
 *  linux/fs/hpfs/super.c
 *
 *  Mikulas Patocka (mikulas@artax.karlin.mff.cuni.cz), 1998-1999
 *
 *  mounting, unmounting, error handling
 */

#include <linux/string.h>
#include "hpfs_fn.h"
#include <linux/module.h>
#include <linux/init.h>

/* Mark the filesystem dirty, so that chkdsk checks it when os/2 booted */

static void mark_dirty(struct super_block *s)
{
	if (s->s_hpfs_chkdsk && !(s->s_flags & MS_RDONLY)) {
		struct buffer_head *bh;
		struct hpfs_spare_block *sb;
		if ((sb = hpfs_map_sector(s, 17, &bh, 0))) {
			sb->dirty = 1;
			sb->old_wrote = 0;
			mark_buffer_dirty(bh);
			brelse(bh);
		}
	}
}

/* Mark the filesystem clean (mark it dirty for chkdsk if chkdsk==2 or if there
   were errors) */

static void unmark_dirty(struct super_block *s)
{
	struct buffer_head *bh;
	struct hpfs_spare_block *sb;
	if (s->s_flags & MS_RDONLY) return;
	if ((sb = hpfs_map_sector(s, 17, &bh, 0))) {
		sb->dirty = s->s_hpfs_chkdsk > 1 - s->s_hpfs_was_error;
		sb->old_wrote = s->s_hpfs_chkdsk >= 2 && !s->s_hpfs_was_error;
		mark_buffer_dirty(bh);
		brelse(bh);
	}
}

/* Filesystem error... */

#define ERR_BUF_SIZE 1024

void hpfs_error(struct super_block *s, char *m,...)
{
	char *buf;
	va_list l;
	va_start(l, m);
	if (!(buf = kmalloc(ERR_BUF_SIZE, GFP_KERNEL)))
		printk("HPFS: No memory for error message '%s'\n",m);
	else if (vsprintf(buf, m, l) >= ERR_BUF_SIZE)
		printk("HPFS: Grrrr... Kernel memory corrupted ... going on, but it'll crash very soon :-(\n");
	printk("HPFS: filesystem error: ");
	if (buf) printk("%s", buf);
	else printk("%s\n",m);
	if (!s->s_hpfs_was_error) {
		if (s->s_hpfs_err == 2) {
			printk("; crashing the system because you wanted it\n");
			mark_dirty(s);
			panic("HPFS panic");
		} else if (s->s_hpfs_err == 1) {
			if (s->s_flags & MS_RDONLY) printk("; already mounted read-only\n");
			else {
				printk("; remounting read-only\n");
				mark_dirty(s);
				s->s_flags |= MS_RDONLY;
			}
		} else if (s->s_flags & MS_RDONLY) printk("; going on - but anything won't be destroyed because it's read-only\n");
		else printk("; corrupted filesystem mounted read/write - your computer will explode within 20 seconds ... but you wanted it so!\n");
	} else printk("\n");
	if (buf) kfree(buf);
	s->s_hpfs_was_error = 1;
}

/* 
 * A little trick to detect cycles in many hpfs structures and don't let the
 * kernel crash on corrupted filesystem. When first called, set c2 to 0.
 *
 * BTW. chkdsk doesn't detect cycles correctly. When I had 2 lost directories
 * nested each in other, chkdsk locked up happilly.
 */

int hpfs_stop_cycles(struct super_block *s, int key, int *c1, int *c2,
		char *msg)
{
	if (*c2 && *c1 == key) {
		hpfs_error(s, "cycle detected on key %08x in %s", key, msg);
		return 1;
	}
	(*c2)++;
	if (!((*c2 - 1) & *c2)) *c1 = key;
	return 0;
}

void hpfs_put_super(struct super_block *s)
{
	if (s->s_hpfs_cp_table) kfree(s->s_hpfs_cp_table);
	if (s->s_hpfs_bmp_dir) kfree(s->s_hpfs_bmp_dir);
	unmark_dirty(s);
}

unsigned hpfs_count_one_bitmap(struct super_block *s, secno secno)
{
	struct quad_buffer_head qbh;
	unsigned *bits;
	unsigned i, count;
	if (!(bits = hpfs_map_4sectors(s, secno, &qbh, 4))) return 0;
	count = 0;
	for (i = 0; i < 2048 / sizeof(unsigned); i++) {
		unsigned b; 
		if (!bits[i]) continue;
		for (b = bits[i]; b; b>>=1) count += b & 1;
	}
	hpfs_brelse4(&qbh);
	return count;
}

static unsigned count_bitmaps(struct super_block *s)
{
	unsigned n, count, n_bands;
	n_bands = (s->s_hpfs_fs_size + 0x3fff) >> 14;
	count = 0;
	for (n = 0; n < n_bands; n++)
		count += hpfs_count_one_bitmap(s, s->s_hpfs_bmp_dir[n]);
	return count;
}

int hpfs_statfs(struct super_block *s, struct statfs *buf)
{
	/*if (s->s_hpfs_n_free == -1) {*/
		s->s_hpfs_n_free = count_bitmaps(s);
		s->s_hpfs_n_free_dnodes = hpfs_count_one_bitmap(s, s->s_hpfs_dmap);
	/*}*/
	buf->f_type = s->s_magic;
	buf->f_bsize = 512;
	buf->f_blocks = s->s_hpfs_fs_size;
	buf->f_bfree = s->s_hpfs_n_free;
	buf->f_bavail = s->s_hpfs_n_free;
	buf->f_files = s->s_hpfs_dirband_size / 4;
	buf->f_ffree = s->s_hpfs_n_free_dnodes;
	buf->f_namelen = 254;
	return 0;
}

/* Super operations */

static struct super_operations hpfs_sops =
{
        read_inode:	hpfs_read_inode,
	delete_inode:	hpfs_delete_inode,
	put_super:	hpfs_put_super,
	statfs:		hpfs_statfs,
	remount_fs:	hpfs_remount_fs,
};

/*
 * A tiny parser for option strings, stolen from dosfs.
 *
 * Stolen again from read-only hpfs.
 */

int parse_opts(char *opts, uid_t *uid, gid_t *gid, umode_t *umask,
	       int *lowercase, int *conv, int *eas, int *chk, int *errs,
	       int *chkdsk, int *timeshift)
{
	char *p, *rhs;

	if (!opts)
		return 1;

	/*printk("Parsing opts: '%s'\n",opts);*/

	for (p = strtok(opts, ","); p != 0; p = strtok(0, ",")) {
		if ((rhs = strchr(p, '=')) != 0)
			*rhs++ = '\0';
		if (!strcmp(p, "help")) return 2;
		if (!strcmp(p, "uid")) {
			if (!rhs || !*rhs)
				return 0;
			*uid = simple_strtoul(rhs, &rhs, 0);
			if (*rhs)
				return 0;
		}
		else if (!strcmp(p, "gid")) {
			if (!rhs || !*rhs)
				return 0;
			*gid = simple_strtoul(rhs, &rhs, 0);
			if (*rhs)
				return 0;
		}
		else if (!strcmp(p, "umask")) {
			if (!rhs || !*rhs)
				return 0;
			*umask = simple_strtoul(rhs, &rhs, 8);
			if (*rhs)
				return 0;
		}
		else if (!strcmp(p, "timeshift")) {
			int m = 1;
			if (!rhs || !*rhs)
				return 0;
			if (*rhs == '-') m = -1;
			if (*rhs == '+' || *rhs == '-') rhs++;
			*timeshift = simple_strtoul(rhs, &rhs, 0) * m;
			if (*rhs)
				return 0;
		}
		else if (!strcmp(p, "case")) {
			if (!rhs || !*rhs)
				return 0;
			if (!strcmp(rhs, "lower"))
				*lowercase = 1;
			else if (!strcmp(rhs, "asis"))
				*lowercase = 0;
			else
				return 0;
		}
		else if (!strcmp(p, "conv")) {
			if (!rhs || !*rhs)
				return 0;
			if (!strcmp(rhs, "binary"))
				*conv = CONV_BINARY;
			else if (!strcmp(rhs, "text"))
				*conv = CONV_TEXT;
			else if (!strcmp(rhs, "auto"))
				*conv = CONV_AUTO;
			else
				return 0;
		}
		else if (!strcmp(p, "check")) {
			if (!rhs || !*rhs)
				return 0;
			if (!strcmp(rhs, "none"))
				*chk = 0;
			else if (!strcmp(rhs, "normal"))
				*chk = 1;
			else if (!strcmp(rhs, "strict"))
				*chk = 2;
			else
				return 0;
		}
		else if (!strcmp(p, "errors")) {
			if (!rhs || !*rhs)
				return 0;
			if (!strcmp(rhs, "continue"))
				*errs = 0;
			else if (!strcmp(rhs, "remount-ro"))
				*errs = 1;
			else if (!strcmp(rhs, "panic"))
				*errs = 2;
			else
				return 0;
		}
		else if (!strcmp(p, "eas")) {
			if (!rhs || !*rhs)
				return 0;
			if (!strcmp(rhs, "no"))
				*eas = 0;
			else if (!strcmp(rhs, "ro"))
				*eas = 1;
			else if (!strcmp(rhs, "rw"))
				*eas = 2;
			else
				return 0;
		}
		else if (!strcmp(p, "chkdsk")) {
			if (!rhs || !*rhs)
				return 0;
			if (!strcmp(rhs, "no"))
				*chkdsk = 0;
			else if (!strcmp(rhs, "errors"))
				*chkdsk = 1;
			else if (!strcmp(rhs, "always"))
				*chkdsk = 2;
			else
				return 0;
		}
		else
			return 0;
	}
	return 1;
}

static inline void hpfs_help(void)
{
	printk("\n\
HPFS filesystem options:\n\
      help              do not mount and display this text\n\
      uid=xxx           set uid of files that don't have uid specified in eas\n\
      gid=xxx           set gid of files that don't have gid specified in eas\n\
      umask=xxx         set mode of files that don't have mode specified in eas\n\
      case=lower        lowercase all files\n\
      case=asis         do not lowercase files (default)\n\
      conv=binary       do not convert CR/LF -> LF (default)\n\
      conv=auto         convert only files with known text extensions\n\
      conv=text         convert all files\n\
      check=none        no fs checks - kernel may crash on corrupted filesystem\n\
      check=normal      do some checks - it should not crash (default)\n\
      check=strict      do extra time-consuming checks, used for debugging\n\
      errors=continue   continue on errors\n\
      errors=remount-ro remount read-only if errors found (default)\n\
      errors=panic      panic on errors\n\
      chkdsk=no         do not mark fs for chkdsking even if there were errors\n\
      chkdsk=errors     mark fs dirty if errors found (default)\n\
      chkdsk=always     always mark fs dirty - used for debugging\n\
      eas=no            ignore extended attributes\n\
      eas=ro            read but do not write extended attributes\n\
      eas=rw            r/w eas => enables chmod, chown, mknod, ln -s (default)\n\
      timeshift=nnn	add nnn seconds to file times\n\
\n");
}

int hpfs_remount_fs(struct super_block *s, int *flags, char *data)
{
	uid_t uid;
	gid_t gid;
	umode_t umask;
	int lowercase, conv, eas, chk, errs, chkdsk, timeshift;
	int o;
	
	*flags |= MS_NOATIME;
	
	uid = s->s_hpfs_uid; gid = s->s_hpfs_gid;
	umask = 0777 & ~s->s_hpfs_mode;
	lowercase = s->s_hpfs_lowercase; conv = s->s_hpfs_conv;
	eas = s->s_hpfs_eas; chk = s->s_hpfs_chk; chkdsk = s->s_hpfs_chkdsk;
	errs = s->s_hpfs_err; timeshift = s->s_hpfs_timeshift;

	if (!(o = parse_opts(data, &uid, &gid, &umask, &lowercase, &conv,
	    &eas, &chk, &errs, &chkdsk, &timeshift))) {
		printk("HPFS: bad mount options.\n");
	    	return 1;
	}
	if (o == 2) {
		hpfs_help();
		return 1;
	}
	if (timeshift != s->s_hpfs_timeshift) {
		printk("HPFS: timeshift can't be changed using remount.\n");
		return 1;
	}

	unmark_dirty(s);

	s->s_hpfs_uid = uid; s->s_hpfs_gid = gid;
	s->s_hpfs_mode = 0777 & ~umask;
	s->s_hpfs_lowercase = lowercase; s->s_hpfs_conv = conv;
	s->s_hpfs_eas = eas; s->s_hpfs_chk = chk; s->s_hpfs_chkdsk = chkdsk;
	s->s_hpfs_err = errs; s->s_hpfs_timeshift = timeshift;

	if (!(*flags & MS_RDONLY)) mark_dirty(s);

	return 0;
}

struct super_block *hpfs_read_super(struct super_block *s, void *options,
				    int silent)
{
	kdev_t dev;
	struct buffer_head *bh0, *bh1, *bh2;
	struct hpfs_boot_block *bootblock;
	struct hpfs_super_block *superblock;
	struct hpfs_spare_block *spareblock;

	uid_t uid;
	gid_t gid;
	umode_t umask;
	int lowercase, conv, eas, chk, errs, chkdsk, timeshift;

	dnode_secno root_dno;
	struct hpfs_dirent *de = NULL;
	struct quad_buffer_head qbh;

	int o;

	s->s_hpfs_bmp_dir = NULL;
	s->s_hpfs_cp_table = NULL;

	s->s_hpfs_creation_de_lock = s->s_hpfs_rd_inode = 0;
	init_waitqueue_head(&s->s_hpfs_creation_de);
	init_waitqueue_head(&s->s_hpfs_iget_q);

	uid = current->uid;
	gid = current->gid;
	umask = current->fs->umask;
	lowercase = 0;
	conv = CONV_BINARY;
	eas = 2;
	chk = 1;
	errs = 1;
	chkdsk = 1;
	timeshift = 0;

	if (!(o = parse_opts(options, &uid, &gid, &umask, &lowercase, &conv,
	    &eas, &chk, &errs, &chkdsk, &timeshift))) {
		printk("HPFS: bad mount options.\n");
		goto bail0;
	}
	if (o==2) {
		hpfs_help();
		goto bail0;
	}

	/*s->s_hpfs_mounting = 1;*/
	dev = s->s_dev;
	set_blocksize(dev, 512);
	s->s_blocksize = 512;
	s->s_blocksize_bits = 9;
	s->s_hpfs_fs_size = -1;
	if (!(bootblock = hpfs_map_sector(s, 0, &bh0, 0))) goto bail1;
	if (!(superblock = hpfs_map_sector(s, 16, &bh1, 1))) goto bail2;
	if (!(spareblock = hpfs_map_sector(s, 17, &bh2, 0))) goto bail3;

	/* Check magics */
	if (/*bootblock->magic != BB_MAGIC
	    ||*/ superblock->magic != SB_MAGIC
	    || spareblock->magic != SP_MAGIC) {
		if (!silent) printk("HPFS: Bad magic ... probably not HPFS\n");
		goto bail4;
	}

	/* Check version */
	if (!(s->s_flags & MS_RDONLY) &&
	      superblock->funcversion != 2 && superblock->funcversion != 3) {
		printk("HPFS: Bad version %d,%d. Mount readonly to go around\n",
			(int)superblock->version, (int)superblock->funcversion);
		printk("HPFS: please try recent version of HPFS driver at http://artax.karlin.mff.cuni.cz/~mikulas/vyplody/hpfs/index-e.cgi and if it still can't understand this format, contact author - mikulas@artax.karlin.mff.cuni.cz\n");
		goto bail4;
	}

	s->s_flags |= MS_NOATIME;

	/* Fill superblock stuff */
	s->s_magic = HPFS_SUPER_MAGIC;
	s->s_op = &hpfs_sops;

	s->s_hpfs_root = superblock->root;
	s->s_hpfs_fs_size = superblock->n_sectors;
	s->s_hpfs_bitmaps = superblock->bitmaps;
	s->s_hpfs_dirband_start = superblock->dir_band_start;
	s->s_hpfs_dirband_size = superblock->n_dir_band;
	s->s_hpfs_dmap = superblock->dir_band_bitmap;
	s->s_hpfs_uid = uid;
	s->s_hpfs_gid = gid;
	s->s_hpfs_mode = 0777 & ~umask;
	s->s_hpfs_n_free = -1;
	s->s_hpfs_n_free_dnodes = -1;
	s->s_hpfs_lowercase = lowercase;
	s->s_hpfs_conv = conv;
	s->s_hpfs_eas = eas;
	s->s_hpfs_chk = chk;
	s->s_hpfs_chkdsk = chkdsk;
	s->s_hpfs_err = errs;
	s->s_hpfs_timeshift = timeshift;
	s->s_hpfs_was_error = 0;
	s->s_hpfs_cp_table = NULL;
	s->s_hpfs_c_bitmap = -1;
	
	/* Load bitmap directory */
	if (!(s->s_hpfs_bmp_dir = hpfs_load_bitmap_directory(s, superblock->bitmaps)))
		goto bail4;
	
	/* Check for general fs errors*/
	if (spareblock->dirty && !spareblock->old_wrote) {
		if (errs == 2) {
			printk("HPFS: Improperly stopped, not mounted\n");
			goto bail4;
		}
		hpfs_error(s, "improperly stopped");
	}

	if (!(s->s_flags & MS_RDONLY)) {
		spareblock->dirty = 1;
		spareblock->old_wrote = 0;
		mark_buffer_dirty(bh2);
	}

	if (spareblock->hotfixes_used || spareblock->n_spares_used) {
		if (errs >= 2) {
			printk("HPFS: Hotfixes not supported here, try chkdsk\n");
			mark_dirty(s);
			goto bail4;
		}
		hpfs_error(s, "hotfixes not supported here, try chkdsk");
		if (errs == 0) printk("HPFS: Proceeding, but your filesystem will be probably corrupted by this driver...\n");
		else printk("HPFS: This driver may read bad files or crash when operating on disk with hotfixes.\n");
	}
	if (spareblock->n_dnode_spares != spareblock->n_dnode_spares_free) {
		if (errs >= 2) {
			printk("HPFS: Spare dnodes used, try chkdsk\n");
			mark_dirty(s);
			goto bail4;
		}
		hpfs_error(s, "warning: spare dnodes used, try chkdsk");
		if (errs == 0) printk("HPFS: Proceeding, but your filesystem could be corrupted if you delete files or directories\n");
	}
	if (chk) {
		unsigned a;
		if (superblock->dir_band_end - superblock->dir_band_start + 1 != superblock->n_dir_band ||
		    superblock->dir_band_end < superblock->dir_band_start || superblock->n_dir_band > 0x4000) {
			hpfs_error(s, "dir band size mismatch: dir_band_start==%08x, dir_band_end==%08x, n_dir_band==%08x",
				superblock->dir_band_start, superblock->dir_band_end, superblock->n_dir_band);
			goto bail4;
		}
		a = s->s_hpfs_dirband_size;
		s->s_hpfs_dirband_size = 0;
		if (hpfs_chk_sectors(s, superblock->dir_band_start, superblock->n_dir_band, "dir_band") ||
		    hpfs_chk_sectors(s, superblock->dir_band_bitmap, 4, "dir_band_bitmap") ||
		    hpfs_chk_sectors(s, superblock->bitmaps, 4, "bitmaps")) {
			mark_dirty(s);
			goto bail4;
		}
		s->s_hpfs_dirband_size = a;
	} else printk("HPFS: You really don't want any checks? You are crazy...\n");

	/* Load code page table */
	if (spareblock->n_code_pages)
		if (!(s->s_hpfs_cp_table = hpfs_load_code_page(s, spareblock->code_page_dir)))
			printk("HPFS: Warning: code page support is disabled\n");

	brelse(bh2);
	brelse(bh1);
	brelse(bh0);

	hpfs_lock_iget(s, 1);
	s->s_root = d_alloc_root(iget(s, s->s_hpfs_root));
	hpfs_unlock_iget(s);
	if (!s->s_root || !s->s_root->d_inode) {
		printk("HPFS: iget failed. Why???\n");
		goto bail0;
	}
	hpfs_set_dentry_operations(s->s_root);

	/*
	 * find the root directory's . pointer & finish filling in the inode
	 */

	root_dno = hpfs_fnode_dno(s, s->s_hpfs_root);
	if (root_dno)
		de = map_dirent(s->s_root->d_inode, root_dno, "\001\001", 2, NULL, &qbh);
	if (!root_dno || !de) hpfs_error(s, "unable to find root dir");
	else {
		s->s_root->d_inode->i_atime = local_to_gmt(s, de->read_date);
		s->s_root->d_inode->i_mtime = local_to_gmt(s, de->write_date);
		s->s_root->d_inode->i_ctime = local_to_gmt(s, de->creation_date);
		s->s_root->d_inode->i_hpfs_ea_size = de->ea_size;
		s->s_root->d_inode->i_hpfs_parent_dir = s->s_root->d_inode->i_ino;
		if (s->s_root->d_inode->i_size == -1) s->s_root->d_inode->i_size = 2048;
		if (s->s_root->d_inode->i_blocks == -1) s->s_root->d_inode->i_blocks = 5;
	}
	if (de) hpfs_brelse4(&qbh);

	return s;

bail4:	brelse(bh2);
bail3:	brelse(bh1);
bail2:	brelse(bh0);
bail1:
bail0:
	if (s->s_hpfs_bmp_dir) kfree(s->s_hpfs_bmp_dir);
	if (s->s_hpfs_cp_table) kfree(s->s_hpfs_cp_table);
	return NULL;
}

DECLARE_FSTYPE_DEV(hpfs_fs_type, "hpfs", hpfs_read_super);

static int __init init_hpfs_fs(void)
{
	return register_filesystem(&hpfs_fs_type);
}

static void __exit exit_hpfs_fs(void)
{
	unregister_filesystem(&hpfs_fs_type);
}

EXPORT_NO_SYMBOLS;

module_init(init_hpfs_fs)
module_exit(exit_hpfs_fs)
MODULE_LICENSE("GPL");
