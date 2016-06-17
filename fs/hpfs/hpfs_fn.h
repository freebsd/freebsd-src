/*
 *  linux/fs/hpfs/hpfs_fn.h
 *
 *  Mikulas Patocka (mikulas@artax.karlin.mff.cuni.cz), 1998-1999
 *
 *  function headers
 */

//#define DBG
//#define DEBUG_LOCKS

#include <linux/fs.h>
#include <linux/hpfs_fs.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/locks.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <asm/bitops.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/smp_lock.h>

#include <stdarg.h>

#include "hpfs.h"

#define memcpy_tofs memcpy
#define memcpy_fromfs memcpy

#define EIOERROR  EIO
#define EFSERROR  EPERM
#define EMEMERROR ENOMEM

#define ANODE_ALLOC_FWD	512
#define FNODE_ALLOC_FWD	0
#define ALLOC_FWD_MIN	16
#define ALLOC_FWD_MAX	128
#define ALLOC_M		1
#define FNODE_RD_AHEAD	16
#define ANODE_RD_AHEAD	16
#define DNODE_RD_AHEAD	4

#define FREE_DNODES_ADD	58
#define FREE_DNODES_DEL	29

#define CHKCOND(x,y) if (!(x)) printk y

#ifdef DBG
#define PRINTK(x) printk x
#else
#undef PRINTK
#define PRINTK(x)
#endif

typedef void nonconst; /* What this is for ? */

/*
 * local time (HPFS) to GMT (Unix)
 */

extern inline time_t local_to_gmt(struct super_block *s, time_t t)
{
	extern struct timezone sys_tz;
	return t + sys_tz.tz_minuteswest * 60 + s->s_hpfs_timeshift;
}

extern inline time_t gmt_to_local(struct super_block *s, time_t t)
{
	extern struct timezone sys_tz;
	return t - sys_tz.tz_minuteswest * 60 - s->s_hpfs_timeshift;
}

/*
 * conv= options
 */

#define CONV_BINARY 0			/* no conversion */
#define CONV_TEXT 1			/* crlf->newline */
#define CONV_AUTO 2			/* decide based on file contents */

/* Four 512-byte buffers and the 2k block obtained by concatenating them */

struct quad_buffer_head {
	struct buffer_head *bh[4];
	void *data;
};

/* The b-tree down pointer from a dir entry */

extern inline dnode_secno de_down_pointer (struct hpfs_dirent *de)
{
  CHKCOND(de->down,("HPFS: de_down_pointer: !de->down\n"));
  return *(dnode_secno *) ((void *) de + de->length - 4);
}

/* The first dir entry in a dnode */

extern inline struct hpfs_dirent *dnode_first_de (struct dnode *dnode)
{
  return (void *) dnode->dirent;
}

/* The end+1 of the dir entries */

extern inline struct hpfs_dirent *dnode_end_de (struct dnode *dnode)
{
  CHKCOND(dnode->first_free>=0x14 && dnode->first_free<=0xa00,("HPFS: dnode_end_de: dnode->first_free = %d\n",(int)dnode->first_free));
  return (void *) dnode + dnode->first_free;
}

/* The dir entry after dir entry de */

extern inline struct hpfs_dirent *de_next_de (struct hpfs_dirent *de)
{
  CHKCOND(de->length>=0x20 && de->length<0x800,("HPFS: de_next_de: de->length = %d\n",(int)de->length));
  return (void *) de + de->length;
}

extern inline struct extended_attribute *fnode_ea(struct fnode *fnode)
{
	return (struct extended_attribute *)((char *)fnode + fnode->ea_offs);
}

extern inline struct extended_attribute *fnode_end_ea(struct fnode *fnode)
{
	return (struct extended_attribute *)((char *)fnode + fnode->ea_offs + fnode->ea_size_s);
}

extern inline struct extended_attribute *next_ea(struct extended_attribute *ea)
{
	return (struct extended_attribute *)((char *)ea + 5 + ea->namelen + ea->valuelen);
}

extern inline secno ea_sec(struct extended_attribute *ea)
{
	return *(secno *)((char *)ea + 9 + ea->namelen);
}

extern inline secno ea_len(struct extended_attribute *ea)
{
	return *(secno *)((char *)ea + 5 + ea->namelen);
}

extern inline char *ea_data(struct extended_attribute *ea)
{
	return (char *)((char *)ea + 5 + ea->namelen);
}

extern inline unsigned de_size(int namelen, secno down_ptr)
{
	return ((0x1f + namelen + 3) & ~3) + (down_ptr ? 4 : 0);
}

extern inline void copy_de(struct hpfs_dirent *dst, struct hpfs_dirent *src)
{
	int a;
	int n;
	if (!dst || !src) return;
	a = dst->down;
	n = dst->not_8x3;
	memcpy((char *)dst + 2, (char *)src + 2, 28);
	dst->down = a;
	dst->not_8x3 = n;
}

extern inline unsigned tstbits(unsigned *bmp, unsigned b, unsigned n)
{
	int i;
	if ((b >= 0x4000) || (b + n - 1 >= 0x4000)) return n;
	if (!((bmp[(b & 0x3fff) >> 5] >> (b & 0x1f)) & 1)) return 1;
	for (i = 1; i < n; i++)
		if (/*b+i < 0x4000 &&*/ !((bmp[((b+i) & 0x3fff) >> 5] >> ((b+i) & 0x1f)) & 1))
			return i + 1;
	return 0;
}

/* alloc.c */

int hpfs_chk_sectors(struct super_block *, secno, int, char *);
secno hpfs_alloc_sector(struct super_block *, secno, unsigned, int, int);
int hpfs_alloc_if_possible_nolock(struct super_block *, secno);
int hpfs_alloc_if_possible(struct super_block *, secno);
void hpfs_free_sectors(struct super_block *, secno, unsigned);
int hpfs_check_free_dnodes(struct super_block *, int);
void hpfs_free_dnode(struct super_block *, secno);
struct dnode *hpfs_alloc_dnode(struct super_block *, secno, dnode_secno *, struct quad_buffer_head *, int);
struct fnode *hpfs_alloc_fnode(struct super_block *, secno, fnode_secno *, struct buffer_head **);
struct anode *hpfs_alloc_anode(struct super_block *, secno, anode_secno *, struct buffer_head **);

/* anode.c */

secno hpfs_bplus_lookup(struct super_block *, struct inode *, struct bplus_header *, unsigned, struct buffer_head *);
secno hpfs_add_sector_to_btree(struct super_block *, secno, int, unsigned);
void hpfs_remove_btree(struct super_block *, struct bplus_header *);
int hpfs_ea_read(struct super_block *, secno, int, unsigned, unsigned, char *);
int hpfs_ea_write(struct super_block *, secno, int, unsigned, unsigned, char *);
void hpfs_ea_remove(struct super_block *, secno, int, unsigned);
void hpfs_truncate_btree(struct super_block *, secno, int, unsigned);
void hpfs_remove_fnode(struct super_block *, fnode_secno fno);

/* buffer.c */

void hpfs_lock_creation(struct super_block *);
void hpfs_unlock_creation(struct super_block *);
void hpfs_lock_iget(struct super_block *, int);
void hpfs_unlock_iget(struct super_block *);
void hpfs_lock_inode(struct inode *);
void hpfs_unlock_inode(struct inode *);
void hpfs_lock_2inodes(struct inode *, struct inode *);
void hpfs_unlock_2inodes(struct inode *, struct inode *);
void hpfs_lock_3inodes(struct inode *, struct inode *, struct inode *);
void hpfs_unlock_3inodes(struct inode *, struct inode *, struct inode *);
void *hpfs_map_sector(struct super_block *, unsigned, struct buffer_head **, int);
void *hpfs_get_sector(struct super_block *, unsigned, struct buffer_head **);
void *hpfs_map_4sectors(struct super_block *, unsigned, struct quad_buffer_head *, int);
void *hpfs_get_4sectors(struct super_block *, unsigned, struct quad_buffer_head *);
void hpfs_brelse4(struct quad_buffer_head *);
void hpfs_mark_4buffers_dirty(struct quad_buffer_head *);

/* dentry.c */

void hpfs_set_dentry_operations(struct dentry *);

/* dir.c */

int hpfs_dir_release(struct inode *, struct file *);
loff_t hpfs_dir_lseek(struct file *, loff_t, int);
int hpfs_readdir(struct file *, void *, filldir_t);
struct dentry *hpfs_lookup(struct inode *, struct dentry *);

/* dnode.c */

void hpfs_add_pos(struct inode *, loff_t *);
void hpfs_del_pos(struct inode *, loff_t *);
struct hpfs_dirent *hpfs_add_de(struct super_block *, struct dnode *, unsigned char *, unsigned, secno);
void hpfs_delete_de(struct super_block *, struct dnode *, struct hpfs_dirent *);
int hpfs_add_to_dnode(struct inode *, dnode_secno, unsigned char *, unsigned, struct hpfs_dirent *, dnode_secno);
int hpfs_add_dirent(struct inode *, unsigned char *, unsigned, struct hpfs_dirent *, int);
int hpfs_remove_dirent(struct inode *, dnode_secno, struct hpfs_dirent *, struct quad_buffer_head *, int);
void hpfs_count_dnodes(struct super_block *, dnode_secno, int *, int *, int *);
dnode_secno hpfs_de_as_down_as_possible(struct super_block *, dnode_secno dno);
struct hpfs_dirent *map_pos_dirent(struct inode *, loff_t *, struct quad_buffer_head *);
struct hpfs_dirent *map_dirent(struct inode *, dnode_secno, char *, unsigned, dnode_secno *, struct quad_buffer_head *);
void hpfs_remove_dtree(struct super_block *, dnode_secno);
struct hpfs_dirent *map_fnode_dirent(struct super_block *, fnode_secno, struct fnode *, struct quad_buffer_head *);

/* ea.c */

void hpfs_ea_ext_remove(struct super_block *, secno, int, unsigned);
int hpfs_read_ea(struct super_block *, struct fnode *, char *, char *, int);
char *hpfs_get_ea(struct super_block *, struct fnode *, char *, int *);
void hpfs_set_ea(struct inode *, struct fnode *, char *, char *, int);

/* file.c */

int hpfs_file_release(struct inode *, struct file *);
int hpfs_open(struct inode *, struct file *);
int hpfs_file_fsync(struct file *, struct dentry *, int);
secno hpfs_bmap(struct inode *, unsigned);
void hpfs_truncate(struct inode *);
int hpfs_get_block(struct inode *inode, long iblock, struct buffer_head *bh_result, int create);
ssize_t hpfs_file_write(struct file *file, const char *buf, size_t count, loff_t *ppos);

/* inode.c */

void hpfs_read_inode(struct inode *);
void hpfs_write_inode_ea(struct inode *, struct fnode *);
void hpfs_write_inode(struct inode *);
void hpfs_write_inode_nolock(struct inode *);
int hpfs_notify_change(struct dentry *, struct iattr *);
void hpfs_write_if_changed(struct inode *);
void hpfs_delete_inode(struct inode *);

/* map.c */

unsigned *hpfs_map_dnode_bitmap(struct super_block *, struct quad_buffer_head *);
unsigned *hpfs_map_bitmap(struct super_block *, unsigned, struct quad_buffer_head *, char *);
char *hpfs_load_code_page(struct super_block *, secno);
secno *hpfs_load_bitmap_directory(struct super_block *, secno bmp);
struct fnode *hpfs_map_fnode(struct super_block *s, ino_t, struct buffer_head **);
struct anode *hpfs_map_anode(struct super_block *s, anode_secno, struct buffer_head **);
struct dnode *hpfs_map_dnode(struct super_block *s, dnode_secno, struct quad_buffer_head *);
dnode_secno hpfs_fnode_dno(struct super_block *s, ino_t ino);

/* name.c */

unsigned char hpfs_upcase(unsigned char *, unsigned char);
int hpfs_chk_name(unsigned char *, unsigned *);
char *hpfs_translate_name(struct super_block *, unsigned char *, unsigned, int, int);
int hpfs_compare_names(struct super_block *, unsigned char *, unsigned, unsigned char *, unsigned, int);
int hpfs_is_name_long(unsigned char *, unsigned);
void hpfs_adjust_length(unsigned char *, unsigned *);
void hpfs_decide_conv(struct inode *, unsigned char *, unsigned);

/* namei.c */

int hpfs_mkdir(struct inode *, struct dentry *, int);
int hpfs_create(struct inode *, struct dentry *, int);
int hpfs_mknod(struct inode *, struct dentry *, int, int);
int hpfs_symlink(struct inode *, struct dentry *, const char *);
int hpfs_unlink(struct inode *, struct dentry *);
int hpfs_rmdir(struct inode *, struct dentry *);
int hpfs_symlink_readpage(struct file *, struct page *);
int hpfs_rename(struct inode *, struct dentry *, struct inode *, struct dentry *);

/* super.c */

void hpfs_error(struct super_block *, char *, ...);
int hpfs_stop_cycles(struct super_block *, int, int *, int *, char *);
int hpfs_remount_fs(struct super_block *, int *, char *);
void hpfs_put_super(struct super_block *);
unsigned hpfs_count_one_bitmap(struct super_block *, secno);
int hpfs_statfs(struct super_block *, struct statfs *);

extern struct address_space_operations hpfs_aops;
